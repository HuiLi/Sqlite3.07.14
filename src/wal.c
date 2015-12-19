/*
** 2010 February 1
**
** The author disclaims否认 copyright to this source code.  In place of
** a legal notice法律警告, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
**
** This file contains the implementation 实现of a write-ahead log (WAL) used in 
** "journal日志_mode=WAL" mode.
**
** WRITE-AHEAD LOG (WAL) FILE FORMAT
**
** A WAL file consists of a header followed by zero or more "frames".�?
** Each frame records the revised修改�?content of a single page from the
** database file.  All changes to the database are recorded by writing
** frames into the WAL.  Transactions事务 commit when a frame is written that
** contains a commit marker提交标签.  A single WAL can and usually does record 
** multiple transactions.  Periodically定期�? the content of the WAL is
** transferred back into the database file in an operation called a
** "checkpoint".
**
** A single WAL file can be used multiple times.  In other words, the
** WAL can fill up with frames and then be checkpointed and then new
** frames can overwrite the old ones.  A WAL always grows from beginning
** toward the end.  Checksums总和检�?and counters计数 attached to each frame are
** used to determine确定 which frames within the WAL are valid and which
** are leftovers from prior checkpoints.
**
** The WAL header is 32 bytes in size and consists of the following eight
** big-endian 32-bit unsigned无符号的 integer values:
**
**     0: Magic number.  0x377f0682 or 0x377f0683
**     4: File format version.  Currently 3007000
**     8: Database page size.  Example: 1024
**    12: Checkpoint sequence序列 number
**    16: Salt-1, random integer incremented增大 with each checkpoint
**    20: Salt-2, a different random integer changing with each ckpt
**    24: Checksum-1 (first part of checksum for first 24 bytes of header).
**    28: Checksum-2 (second part of checksum for first 24 bytes of header).
**
** Immediately following the wal-header are zero or more frames. Each
** frame consists of a 24-byte frame-header followed by接着�?a <page-size> bytes
** of page data. The frame-header is six big-endian 32-bit unsigned 
** integer values, as follows:
**
**     0: Page number.
**     4: For commit records, the size of the database image in pages 
**        after the commit. For all other records, zero.
**     8: Salt-1 (copied from the header)
**    12: Salt-2 (copied from the header)
**    16: Checksum-1.
**    20: Checksum-2.
**
** A frame is considered valid if and only if the following conditions are
** true:
**
**    (1) The salt-1 and salt-2 values in the frame-header match
**        salt values in the wal-header
**
**    (2) The checksum values in the final 8 bytes of the frame-header
**        exactly match the checksum computed consecutively连续�?on the
**        WAL header and the first 8 bytes and the content of all frames
**        up to and including the current frame.
**
** The checksum is computed using 32-bit big-endian integers if the
** magic number in the first 4 bytes of the WAL is 0x377f0683 and it
** is computed using little-endian if the magic number is 0x377f0682.
** The checksum values are always stored in the frame header in a
** big-endian format regardless of which byte order is used to compute
** the checksum.  The checksum is computed by interpreting the input as
** an even number of unsigned 32-bit integers: x[0] through x[N].  The
** algorithm算法 used for the checksum is as follows:
** 
**   for i from 0 to n-1 step 2:
**     s0 += x[i] + s1;
**     s1 += x[i+1] + s0;
**   endfor
**
** Note that s0 and s1 are both weighted 加权中checksums using fibonacci weights
** in reverse order倒序�?(the largest fibonacci weight occurs on the first element
** of the sequence being summed.)  The s1 value spans all 32-bit 
** terms of the sequence序列 whereas然�?s0 omits the final term.
**
** On a checkpoint, the WAL is first VFS.xSync-ed, then valid content of the
** WAL is transferred搬到 into the database, then the database is VFS.xSync-ed.
** The VFS.xSync operations serve as write barriers - all writes launched
** before the xSync must complete before any write that launches after the
** xSync begins.
**
** After each checkpoint, the salt-1 value is incremented 增加and the salt-2
** value is randomized.  This prevents old and new frames in the WAL from
** being considered valid at the same time and being checkpointing together
** following a crash.
**
** READER ALGORITHM
**
** To read a page from the database (call it page number P), a reader
** first checks the WAL to see if it contains page P.  If so, then the
** last valid instance实例 of page P that is a followed by a commit frame
** or is a commit frame itself becomes the value read.  If the WAL
** contains no copies of page P that are valid and which are a commit
** frame or are followed by a commit frame, then page P is read from
** the database file.
**
** To start a read transaction, the reader records the index of the last
** valid frame in the WAL.  The reader uses this recorded "mxFrame" value
** for all subsequent后面�?read operations.  New transactions can be appended
** to the WAL, but as long as the reader uses its original mxFrame value
** and ignores the newly appended content, it will see a consistent(一致的�?snapshot快照
** of the database from a single point in time.  This technique allows
** multiple concurrent并发�?readers to view different versions of the database
** content simultaneously同时�?
**
** The reader algorithm读取算法 in the previous paragraphs works correctly, but 
** because frames for page P can appear anywhere within the WAL, the
** reader has to scan the entire全部�?WAL looking for page P frames.  If the
** WAL is large (multiple megabytes is typical) that scan can be slow,
** and read performance性能 suffers.  To overcome this problem, a separate
** data structure called the wal-index is maintained to expedite 加快the
** search for frames of a particular page.
** 
** WAL-INDEX FORMAT 日志索引结构
**
** Conceptually, the wal-index is shared memory, though VFS implementations
** might choose to implement实施 the wal-index using a mmapped file映射文件.  Because
** the wal-index is shared memory, SQLite does not support journal_mode=WAL 
** on a network filesystem.  All users of the database must be able to
** share memory共享内存.
**
** The wal-index is transient短暂�?  After a crash, the wal-index can (and should
** be) reconstructed重组 from the original WAL file.  In fact, the VFS is required
** to either truncate or zero the header of the wal-index when the last
** connection to it closes.  Because the wal-index is transient, it can
** use an architecture-specific format; it does not have to be cross-platform.
** Hence, unlike the database and WAL file formats which store all values
** as big endian, the wal-index can store multi-byte values in the native
** byte order of the host computer.
**
** The purpose of the wal-index is to answer this question quickly:  Given
** a page number P and a maximum frame index M, return the index of the 
** last frame in the wal before frame M for page P in the WAL, or return
** NULL if there are no frames for page P in the WAL prior to M.
**
** The wal-index consists of a header region, followed by an one or
** more index blocks.  
**
** The wal-index header contains the total number of frames within the WAL
** in the mxFrame field.
**
** Each index block except for the first contains information on 
** HASHTABLE_NPAGE frames. The first index block contains information on
** HASHTABLE_NPAGE_ONE frames. The values of HASHTABLE_NPAGE_ONE and 
** HASHTABLE_NPAGE are selected so that together the wal-index header and
** first index block are the same size as all other index blocks in the
** wal-index.
**
** Each index block contains two sections, a page-mapping that contains the
** database page number associated with each wal frame, and a hash-table 
** that allows readers to query an index block for a specific page number.
** The page-mapping is an array of HASHTABLE_NPAGE (or HASHTABLE_NPAGE_ONE
** for the first index block) 32-bit page numbers. The first entry in the 
** first index-block contains the database page number corresponding to�?..相一�?the
** first frame in the WAL file. The first entry in the second index block
** in the WAL file corresponds to the (HASHTABLE_NPAGE_ONE+1)th frame in
** the log, and so on.
**
** The last index block in a wal-index usually contains less than the full
** complement of HASHTABLE_NPAGE (or HASHTABLE_NPAGE_ONE) page-numbers,
** depending on the contents of the WAL file. This does not change the
** allocated size of the page-mapping array - the page-mapping array merely
** contains unused entries.
**
** Even without using the hash table, the last frame for page P
** can be found by scanning the page-mapping sections of each index block
** starting with the last index block and moving toward the first, and
** within each index block, starting at the end and moving toward the
** beginning.  The first entry that equals P corresponds to the frame
** holding the content for that page.
**
** The hash table consists of HASHTABLE_NSLOT 16-bit unsigned integers.
** HASHTABLE_NSLOT = 2*HASHTABLE_NPAGE, and there is one entry in the
** hash table for each page number in the mapping section, so the hash 
** table is never more than half full.  The expected number of collisions 冲突
** prior to finding a match is 1.  Each entry of the hash table is an
** 1-based index of an entry in the mapping section of the same
** index block.   Let K be the 1-based index of the largest entry in
** the mapping section.  (For index blocks other than the last, K will
** always be exactly HASHTABLE_NPAGE (4096) and for the last index block
** K will be (mxFrame%HASHTABLE_NPAGE).)  Unused slots of the hash table
** contain a value of 0.
**
** To look for page P in the hash table, first compute a hash iKey on
** P as follows:
**
**      iKey = (P * 383) % HASHTABLE_NSLOT     方法
**
** Then start scanning entries of the hash table, starting with iKey
** (wrapping around to the beginning when the end of the hash table is
** reached) until an unused hash slot is found. Let the first unused从未用过 slot
** be at index iUnused.  (iUnused might be less than iKey if there was
** wrap-around.) Because the hash table is never more than half full,
** the search is guaranteed to eventually hit an unused entry.  Let 
** iMax be the value between iKey and iUnused, closest to iUnused,
** where aHash[iMax]==P.  If there is no iMax entry (if there exists
** no hash slot such that aHash[i]==p) then page P is not in the
** current index block.  Otherwise the iMax-th mapping entry of the
** current index block corresponds to the last entry that references 
** page P.
**
** A hash search begins with the last index block and moves toward the
** first index block, looking for entries corresponding to page P.  On
** average, only two or three slots in each index block need to be
** examined in order to either find the last entry for page P, or to
** establish that no such entry exists in the block.  Each index block
** holds over 4000 entries.  So two or three index blocks are sufficient
** to cover a typical 10 megabyte WAL file, assuming 1K pages.  8 or 10
** comparisons比较 (on average) suffice to either locate a frame in the
** WAL or to establish that the frame does not exist in the WAL.  This
** is much faster than scanning the entire 10MB WAL.
**
** Note that entries are added in order of increasing K.  Hence, one
** reader might be using some value K0 and a second reader that started
** at a later time (after additional transactions were added to the WAL
** and to the wal-index) might be using a different value K1, where K1>K0.
** Both readers can use the same hash table and mapping section to get
** the correct result.  There may be entries in the hash table with
** K>K0 but to the first reader, those entries will appear to be unused
** slots in the hash table and so the first reader will get an answer as
** if no values greater than K0 had ever been inserted into the hash table
** in the first place - which is what reader one wants.  Meanwhile, the
** second reader using K1 will see additional values that were inserted
** later, which is exactly what reader two wants.  
**
** When a rollback occurs, the value of K is decreased. Hash table entries
** that correspond to frames greater than the new K value are removed
** from the hash table at this point.
**当发生回滚时 ，k值减小，哈希表中将删�?大于新k值得�?
*/

/*
WAL��־����һ��ͷ��0�������frames����ÿ�����¼һ��ҳ��page���޸ĵ����ݡ������ݿ����е��޸Ķ�ͨ���޸Ŀ�ķ�ʽд��WAL��־��
������WAL��־�ɼ�¼�������WAL��־�е����ݻᶨ���޸ĵ����ݿ��ļ��У����������Ϊ���㡣
����WAL��־�ɶ��ʹ�á�Ҳ����˵��WAL���Ա���ռ���������㶯�����������Ŀ�����д��ԭ����λ���ϡ�
WAL��־���Ǵӿ�ʼ����β˳�����������ӵ������ļ��������checksums���ͼ�����counters������ȷ��WAL�е���Щ����Ч����Щ������ǰ���㶯��������ɵġ�
WALͷ��32�ֽڳ�����������8��32λ�Ĵ���޷�������
0: Magic number. 0x377f0682 or 0x377f0683 
4: File format version. Currently 3007000
8: Database page size. Example: 1024
12: Checkpoint sequence number
16: Salt-1, random integer incremented with each checkpoint
20: Salt-2, a different random integer changing with each ckpt
24: Checksum-1 (first part of checksum for first 24 bytes of header).
28: Checksum-2 (second part of checksum for first 24 bytes of header).
*/

#ifndef SQLITE_OMIT_WAL

#include "wal.h"

/*
** Trace output macros 跟踪输出�?
*/
#if defined(SQLITE_TEST) && defined(SQLITE_DEBUG)
int sqlite3WalTrace = 0;
# define WALTRACE(X)  if(sqlite3WalTrace) sqlite3DebugPrintf X
#else
# define WALTRACE(X)
#endif

/*
** The maximum (and only) versions of the wal and wal-index formats
** that may be interpreted by this version of SQLite.
**
** If a client begins recovering a WAL file and finds that (a) the checksum
** values in the wal-header are correct and (b) the version field is not
** WAL_MAX_VERSION, recovery fails and SQLite returns SQLITE_CANTOPEN.
**
** Similarly, if a client successfully reads a wal-index header (i.e. the 
** checksum test is successful) and finds that the version field is not
** WALINDEX_MAX_VERSION, then no read-transaction is opened and SQLite
** returns SQLITE_CANTOPEN.  定义wal_max_version 和WALINDEX_MAX_VERSION 得�?
*/
#define WAL_MAX_VERSION      3007000
#define WALINDEX_MAX_VERSION 3007000

/*
** Indices of various locking bytes.   WAL_NREADER is the number
** of available reader locks and should be at least 3.定义各种锁的字节
*/
#define WAL_WRITE_LOCK         0
#define WAL_ALL_BUT_WRITE      1
#define WAL_CKPT_LOCK          1
#define WAL_RECOVER_LOCK       2
#define WAL_READ_LOCK(I)       (3+(I))
#define WAL_NREADER            (SQLITE_SHM_NLOCK-3)


/* Object declarations  结构的声�?/
typedef struct WalIndexHdr WalIndexHdr;
typedef struct WalIterator WalIterator;
typedef struct WalCkptInfo WalCkptInfo;


/*
** The following object holds a copy of the wal-index header content.    包含 Wal索引 头的内容
**
** The actual header in the wal-index consists of two copies of this  
** object. 实际�?wal索引头包括两个部�?
**
** The szPage value can be any power of 2 between 512 and 32768, inclusive. szpage 的值必须在512 �?2768 之间，必须是2的倍数
** Or it can be 1 to represent a 65536-byte page.  The latter case was 它可以用1代表65536字节的页�?�?.7.1版本开始支持这以功�?
** added in 3.7.1 when support for 64K pages was added.  
*/
struct WalIndexHdr {
  u32 iVersion;                   /* Wal-index version */          Wal-index版本信息                                              //wal�����汾
  u32 unused;                     /* Unused (padding) field */     没有过的地方                                                //δʹ������
  u32 iChange;                    /* Counter incremented each transaction */记录每个事务的增�?                             // ÿ������ļ�����
  u8 isInit;                      /* 1 when initialized */ 当初始化时是  1                         
  u8 bigEndCksum;                 /* True if checksums in WAL are big-endian */如果在WAl的总和检查是二进制则为true   //�ж�wal�е�checksum�������Ƿ�Ϊbig-endian
  u16 szPage;                     /* Database page size in bytes. 1==64K */ 数据库页的大小，单位为byte�?==64k            //���ݿ�page����
  u32 mxFrame;                    /* Index of last valid frame in the WAL */写入WAl 的最新的有效的索引�?                //WAL�����һ����Ч֡������
  u32 nPage;                      /* Size of database in pages */一个数据库有多少个�?                                     //���ݿ��е�ҳ��С
  u32 aFrameCksum[2];             /* Checksum of last frame in log */ 检验最后写入log�?                                      //��־���һ֡��checksum
  u32 aSalt[2];                   /* Two salt values copied from WAL header */从Wal header 复制的两个混淆�?                //��WALͷ��������saltֵ
  u32 aCksum[2];                  /* Checksum over all prior fields */进行所有字段进行校�?                                //Ԥд��־�ļ��������ж����һ��ʵ������ʾ
};

/*
** A copy of the following object occurs in the wal-index immediately
** following the second copy of the WalIndexHdr.  This object stores
** information used by checkpoint.
**
** nBackfill is the number of frames in the WAL that have been written
** back into the database. (We call the act of moving content from WAL to
** database "backfilling".)  The nBackfill number is never greater than
** WalIndexHdr.mxFrame.  nBackfill can only be increased by threads
** holding the WAL_CKPT_LOCK lock (which includes a recovery thread).
** However, a WAL_WRITE_LOCK thread can move the value of nBackfill from
** mxFrame back to zero when the WAL is reset.
**
** There is one entry in aReadMark[] for each reader lock.  If a reader
** holds read-lock K, then the value in aReadMark[K] is no greater than
** the mxFrame for that reader.  The value READMARK_NOT_USED (0xffffffff)
** for any aReadMark[] means that entry is unused.  aReadMark[0] is 
** a special case; its value is never used and it exists as a place-holder
** to avoid having to offset aReadMark[] indexs by one.  Readers holding
** WAL_READ_LOCK(0) always ignore the entire WAL and read all content
** directly from the database.
**
** The value of aReadMark[K] may only be changed by a thread that
** is holding an exclusive lock on WAL_READ_LOCK(K).  Thus, the value of
** aReadMark[K] cannot changed while there is a reader is using that mark
** since the reader will be holding a shared lock on WAL_READ_LOCK(K).
**
** The checkpointer may only transfer frames from WAL to database where
** the frame numbers are less than or equal to every aReadMark[] that is
** in use (that is, every aReadMark[j] for which there is a corresponding
** WAL_READ_LOCK(j)).  New readers (usually) pick the aReadMark[] with the
** largest value and will increase an unused aReadMark[] to mxFrame if there
** is not already an aReadMark[] equal to mxFrame.  The exception to the
** previous sentence is when nBackfill equals mxFrame (meaning that everything
** in the WAL has been backfilled into the database) then new readers
** will choose aReadMark[0] which has value 0 and hence such reader will
** get all their all content directly from the database file and ignore 
** the WAL.
**
** Writers normally append new frames to the end of the WAL.  However,
** if nBackfill equals mxFrame (meaning that all WAL content has been
** written back into the database) and if no readers are using the WAL
** (in other words, if there are no WAL_READ_LOCK(i) where i>0) then
** the writer will first "reset" the WAL back to the beginning and start
** writing new content beginning at frame 1.
**
** We assume that 32-bit loads are atomic and so no locks are needed in
** order to read from any aReadMark[] entries.
*/
struct WalCkptInfo {                      
  u32 nBackfill;                  /* Number of WAL frames backfilled into DB */  有多少个Wal �?回填到DB //�ж��ٸ�Wal �� ���DB
  u32 aReadMark[WAL_NREADER];     /* Reader marks */读的标志                                                   //����־
}; 
#define READMARK_NOT_USED  0xffffffff


/* A block of WALINDEX_LOCK_RESERVED bytes beginning at
** WALINDEX_LOCK_OFFSET is reserved for locks. Since some systems
** only support mandatory file-locks, we do not read or write data
** from the region of the file on which locks are applied.
*/
#define WALINDEX_LOCK_OFFSET   (sizeof(WalIndexHdr)*2 + sizeof(WalCkptInfo))
#define WALINDEX_LOCK_RESERVED 16
#define WALINDEX_HDR_SIZE      (WALINDEX_LOCK_OFFSET+WALINDEX_LOCK_RESERVED)
 
/* Size of header before each frame in wal */ wal中每一个frame的头数据大小                                  // wal��ÿһ��frame��ͷ���ݴ�С
#define WAL_FRAME_HDRSIZE 24

/* Size of write ahead log header, including checksum. */之前写日志的大小�?包括校验和�?                 //֮ǰд��־�Ĵ�Сͷ,����У��͡�
/* #define WAL_HDRSIZE 24 */                           包括日志头数据和校验�?                                //������־ͷ���ݺ�У��ֵ
#define WAL_HDRSIZE 32

/* WAL magic value. Either this value, or the same value with the least
** significant bit also set (WAL_MAGIC | 0x00000001) is stored in 32-bit
** big-endian format in the first 4 bytes of a WAL file.
**
** If the LSB is set, then the checksums for each frame within the WAL
** file are calculated by treating all data as an array of 32-bit 
** big-endian words. Otherwise, they are calculated by interpreting 
** all data as 32-bit little-endian words.WAL魔法值。这个�?或至少相同的值还有效位设�?WAL_MAGIC | 0 x00000001)存储�?2位　　 大端格式WAL的前4个字节的文件。如果设置LSB,然后在身内的每一帧的校验和文件处理所有的数据计算了一�?2位的数组大端法的话。否�?计算它们的解释所有数据作�?2位低位优先的单词�?
*/
#define WAL_MAGIC 0x377f0682

/*
** Return the offset of frame iFrame in the write-ahead log file, 
** assuming a database page size of szPage bytes. The offset returned
** is to the start of the write-ahead log frame-header.
*/
#define walFrameOffset(iFrame, szPage) (                               \
  WAL_HDRSIZE + ((iFrame)-1)*(i64)((szPage)+WAL_FRAME_HDRSIZE)         \
)

/*
** An open write-ahead log file is represented描写 by an instance of the
** following object.日志头文�?
*/
struct Wal {
  sqlite3_vfs *pVfs;         /* The VFS used to create pDbFd */                                                             //��������pDbFd��VFS
  sqlite3_file *pDbFd;       /* File handle for the database file */                                                         //���ݿ��ļ�
  sqlite3_file *pWalFd;      /* File handle for WAL file */                                                                   //WAL�ļ�
  u32 iCallback;             /* Value to pass to log callback (or 0) */                                                       //���ݸ��ص���־��ֵ
  i64 mxWalSize;             /* Truncate WAL to this size upon reset */                                                      //��λ�����WAL�������С
  int nWiData;               /* Size of array apWiData */                                                                    //apWiData����Ĵ�С 
  int szFirstBlock;          /* Size of first block written to WAL file */                                                   //д��WAL�ļ��ĵ�һ����Ĵ�С
  volatile u32 **apWiData;   /* Pointer to wal-index content in memory */**指针大小                                      //ָ���ڴ���WAL��������                        
  u32 szPage;                /* Database page size */                                                                         //���ݿ�ҳ���С
  i16 readLock;              /* Which read lock is being held.  -1 for none */那种读锁被持有�?1 表示没有         //���ĸ������ھ��С� -1��ʾ��
  u8 syncFlags;              /* Flags to use to sync header writes */                                                         //��־ʹ��ͬ��ͷд
  u8 exclusiveMode;          /* Non-zero if connection is in exclusive mode */                                                //���㣬��������Ƕ�ռģʽ
  u8 writeLock;              /* True if in a write transaction */                如果在一个写事务中为�?             //�������д����
  u8 ckptLock;               /* True if holding a checkpoint lock */   如果有一个checkpoint �?�?值为�?               //���������һ�����վ����
  u8 readOnly;               /* WAL_RDWR, WAL_RDONLY, or WAL_SHM_RDONLY */                                                      //WAL_RDWR �� WAL_RDONLY ����WAL_SHM_RDONLY 
  u8 truncateOnCommit;       /* True to truncate WAL file on commit */                                                         //�����ض�WAL���ύ���ļ�
  u8 syncHeader;             /* Fsync the WAL header if true */                                                                 //�����FsyncΪ�׵�WALΪtrue
  u8 padToSectorBoundary;    /* Pad transactions out to the next sector */                                                     //����һ��������ͨ��  
  WalIndexHdr hdr;           /* Wal-index header for current transaction */  当前事务 Wal-index header                    //Ϊ��ǰ����wal����ͷ 
  const char *zWalName;      /* Name of WAL file */                                                                             //WAL�ļ����ļ���
  u32 nCkpt;                 /* Checkpoint sequence counter in the wal-header */wal-header检查点序列计数�?              //wal�������м�����
#ifdef SQLITE_DEBUG
  u8 lockError;              /* True if a locking error has occurred */                                                           //������������ʱ
#endif
};

/*
** Candidate values for Wal.exclusiveMode.
Wal.exclusiveMode 的候选�?

*/
#define WAL_NORMAL_MODE     0               //����WAL����ģʽ 0��
#define WAL_EXCLUSIVE_MODE  1               //����WAL����ģʽ 1��
#define WAL_HEAPMEMORY_MODE 2               

/*
** Possible values for WAL.readOnly  可能的值只有下�?
*/
#define WAL_RDWR        0    /* Normal read/write connection */    //������/д����
#define WAL_RDONLY      1    /* The WAL file is readonly */        //WAL�ļ�ֻ�ܶ�
#define WAL_SHM_RDONLY  2    /* The SHM file is readonly */        //SHM�ļ�ֻ�ܶ�

/*
** Each page of the wal-index mapping contains a hash-table made up of   wal-index映射的每个页面包含一个哈希表组成HASHTABLE_NSLOT数组元素的类型�?
** an array of HASHTABLE_NSLOT elements of the following type.  
*/
typedef u16 ht_slot;

/*
** This structure is used to implement an iterator that loops through
** all frames in the WAL in database page order. Where two or more frames
** correspond to the same database page, the iterator visits only the 
** frame most recently written to the WAL (in other words, the frame with
** the largest index).这个结构是用来实现迭代器遍历WAL在数据库中的所有帧页面顺序。两个或两个以上的帧在哪里对应于相同的数据库页面,迭代器只访问帧最近写入WAL( ** 换句话说,框架最大的指数)
**
** The internals of this structure are only accessed by: 这种结构的内部只能被访问方式
**
**   walIteratorInit() - Create a new iterator, 创建迭代
**   walIteratorNext() - Step an iterator,         进行下一�?
**   walIteratorFree() - Free an iterator.    释放迭代
**
** This functionality is used by the checkpoint code (see walCheckpoint()). 用于checkpoint 
*/

//����WAL�������ṹ��
struct WalIterator {
  int iPrior;                     /* Last result returned from the iterator */ 最后的返回�?                 //���ķ���ֵ
  int nSegment;                   /* Number of entries in aSegment[] */ 项目�?                                   // ��Ŀ��
    int iNext;                    /* Next slot in aIndex[] not yet returned */ aIndex的下一个下�?            //aIndex����һ���±�
    ht_slot *aIndex;              /* i0, i1, i2... such that aPgno[iN] ascend */                                    
    u32 *aPgno;                   /* Array of page numbers. */    数组页码                                      //����ҳ��
    int nEntry;                   /* Nr. of entries in aPgno[] and aIndex[] */ aPgno【】和aIndex【�?
    int iZero;                    /* Frame number associated with aPgno[0] */ 帧数和aPgno[]一�?
   aSegment[1];                  /* One for every 32KB page in the wal-index */ 32kb 的页
};

/*
** Define the parameters of the hash tables in the wal-index file. There
** is a hash-table following every HASHTABLE_NPAGE page numbers in the
** wal-index.
**
** Changing any of these constants will alter the wal-index format and
** create incompatibilities.
*/
#define HASHTABLE_NPAGE      4096                 /* Must be power of 2 */      //����Hash����ҳ��С   
#define HASHTABLE_HASH_1     383                  /* Should be prime */           //����Hash���Ĵ�С
#define HASHTABLE_NSLOT      (HASHTABLE_NPAGE*2)  /* Must be a power of 2 */      //����Hash����NSLOT 

/* 
** The block of page numbers associated with the first hash-table in a
** wal-index is smaller than usual. This is so that there is a complete
** hash-table on each aligned 32KB page of the wal-index.
*/
#define HASHTABLE_NPAGE_ONE  (HASHTABLE_NPAGE - (WALINDEX_HDR_SIZE/sizeof(u32)))

/* The wal-index is divided into pages of WALINDEX_PGSZ bytes each. */
#define WALINDEX_PGSZ   (                                         \
    sizeof(ht_slot)*HASHTABLE_NSLOT + HASHTABLE_NPAGE*sizeof(u32) \
)

/*
** Obtain a pointer to the iPage'th page of the wal-index. The wal-index
** is broken into pages of WALINDEX_PGSZ bytes. Wal-index pages are
** numbered from zero. 获取日志索引i 页的指针，日志索引被分解 WALINDEX_PGSZ，日志索引页�?开始编�?
**
** If this call is successful, *ppPage is set to point to the wal-index
** page and SQLITE_OK is returned. If an error (an OOM or VFS error) occurs,
** then an SQLite error code is returned and *ppPage is set to 0.如果这个函数调用成功，ppPage 等于日志索引页返回return ok
发生错误，返�?SQLite error code pppage 等于 0
*/

//����WALҳ��������Ϊ��̬����
static int walIndexPage(Wal *pWal, int iPage, volatile u32 **ppPage){
  int rc = SQLITE_OK;

  /* Enlarge the pWal->apWiData[] array if required */扩大pWal - > apWiData[]数组
  if( pWal->nWiData<=iPage ){           **nWiData为指针内存大�?
    int nByte = sizeof(u32*)*(iPage+1); **就算第i个所需字节�?
    volatile u32 **apNew; ?*定义一个新的指�?
    apNew = (volatile u32 **)sqlite3_realloc((void *)pWal->apWiData, nByte);** 为新的指针分配内�?
    
    if( !apNew ){                                    
      *ppPage = 0;
      return SQLITE_NOMEM;
    }
    memset((void*)&apNew[pWal->nWiData], 0,
           sizeof(u32*)*(iPage+1-pWal->nWiData));
    pWal->apWiData = apNew;
    pWal->nWiData = iPage+1;
  }

  /* Request a pointer to the required page from the VFS */
  if( pWal->apWiData[iPage]==0 ){
    if( pWal->exclusiveMode==WAL_HEAPMEMORY_MODE ){
      pWal->apWiData[iPage] = (u32 volatile *)sqlite3MallocZero(WALINDEX_PGSZ);
      if( !pWal->apWiData[iPage] ) rc = SQLITE_NOMEM;
    }else{
      rc = sqlite3OsShmMap(pWal->pDbFd, iPage, WALINDEX_PGSZ, 
          pWal->writeLock, (void volatile **)&pWal->apWiData[iPage]
      );
      if( rc==SQLITE_READONLY ){
        pWal->readOnly |= WAL_SHM_RDONLY;
        rc = SQLITE_OK;
      }
    }
  }

  *ppPage = pWal->apWiData[iPage];
  assert( iPage==0 || *ppPage || rc!=SQLITE_OK );
  return rc;
}

/*
** Return a pointer to the WalCkptInfo structure in the wal-index.返回一个WalCKptINfo指针  //����һ��ָ��walckptinfo�ṹ��WALָ��
*/

//���徲̬����WalCkptInfo
static volatile WalCkptInfo *walCkptInfo(Wal *pWal){
  assert( pWal->nWiData>0 && pWal->apWiData[0] );           assert c函数  其作用是如果它的条件返回错误，则终止程序执行
  return (volatile WalCkptInfo*)&(pWal->apWiData[0][sizeof(WalIndexHdr)/2]);  �?
}

/*
** Return a pointer to the WalIndexHdr structure in the wal-index.   返回一个WalIndexHdr 结构指针
*/
static volatile WalIndexHdr *walIndexHdr(Wal *pWal){
  assert( pWal->nWiData>0 && pWal->apWiData[0] );
  return (volatile WalIndexHdr*)pWal->apWiData[0];
}

/*
** The argument to this macro must be of type u32. On a little-endian 定义的宏参数必须�?2位的�?
** architecture, it returns the u32 value that results from interpreting
** the 4 bytes as a big-endian value. On a big-endian architecture, it
** returns the value that would be produced by intepreting the 4 bytes
** of the input value as a little-endian integer.
*/
/*
������������ͱ�����U32�ꡣ
��С�˵ļܹ��������ص�U32��ֵ������4�ֽ�Ϊ���ֵ�ý����
��big-endian�ļܹ��������ص�ֵ�������͵�����ֵ��4���ֽ�Ϊlittle-endian����������
*/


#define BYTESWAP32(x) ( \
    (((x)&0x000000FF)<<24) + (((x)&0x0000FF00)<<8)  \
  + (((x)&0x00FF0000)>>8)  + (((x)&0xFF000000)>>24) \
)

/*
** Generate or extend an 8 byte checksum based on the data in 
** array aByte[] and the initial values of aIn[0] and aIn[1] (or
** initial values of 0 and 0 if aIn==NULL).对一�?位字节的校验是基于数组abyte【】和ain【�? 0 的初始�?
**
** The checksum is written back into aOut[] before returning. 校验结果在返回之前写�?在aout【�?
**
** nByte must be a positive multiple of 8.  nbyte 必须�?的整数�?
*/
/*
����WALͷ�ϵ���0�������ÿ������һ��24�ֽڵĿ�ͷ��frame-header����һ��ҳ��С��ҳ������ɡ�
��ͷ��6����˵�32λ�޷������������£�
0: Page number.
4: For commit records, the size of the database image in pages after the commit. For all other records, zero.
8: Salt-1 (copied from the header)
12: Salt-2 (copied from the header)
16: Checksum-1.
20: Checksum-2.
��һ����������������ʱ���ű���Ϊ����Ч�ģ�
1.�ڿ�ͷ��Salt-1��salt-2��ֵ����walͷ�����һ��
2.��ͷ������8���ֽڵ�У�����ȫƥ��һ�����ݵ�У����������WALͷ���ʼ��8���ֽں͵�ǰ����������ݡ�
���WAL�ļ��Ŀ�ͷ4���ֽ���0x377f0683����ôУ��;�ʹ��32λ�Ĵ�˼��㣬�����0x377f0682������С�˼��㡣
У���ʼ�մ洢�ڿ�ͷʹ�ô��ģʽ�洢������ʹ��ʲôģʽ����ġ�
*/
static void walChecksumBytes(
  int nativeCksum, /* True for native byte-order, false for non-native */
  u8 *a,           /* Content to be checksummed */     校验 内容                                                          //У������
  int nByte,       /* Bytes of content in a[].  Must be a multiple of 8. */a[] 有多少字节，必须�?的倍数          //a[]�ж����ֽڣ�������8�ı���
  const u32 *aIn,  /* Initial checksum value input */   校验�? 的初始�?                                                //У��͵ĳ�ʼֵ
  u32 *aOut        /* OUT: Final checksum value output */ 最�?校验值得 输出                                           //���У��ֵ�����
){
  u32 s1, s2;                               定义 s1,s2;                                                                       //����u32���͵ı���s1��s2
  u32 *aData = (u32 *)a;                    �?*a 赋予 *aData                                                                 //����u32����ָ��aData������ָ��a
  u32 *aEnd = (u32 *)&a[nByte];             

  if( aIn ){                        如果 ain 不为�?                                                                        //���aIn��Ϊ��
    s1 = aIn[0];                                                                                                                //��aIn[0]��ֵ����s1
    s2 = aIn[1];                                                                                                                //��aIn[1]��ֵ����s2
  }else{                           否则                                                                                        //����s1��s2��Ϊ0
    s1 = s2 = 0;
  }

  assert( nByte>=8 );          如果nByteb不大�?为假，则终止程序                                                     //���nByteb������8Ϊ�٣�����ֹ���� 
  assert( (nByte&0x00000007)==0 );  如果 nByte 不是8的倍数 ，则程序终止                                            // ��� nByte ����8�ı��� ���������ֹ

  if( nativeCksum ){                       如果nativeCksum 为真，则                                                         //���nativeCksum Ϊ�棬��
    do {
      s1 += *aData++ + s2;
      s2 += *aData++ + s1;
    }while( aData<aEnd );
  }else{                                  否则                                                                                  //����
    do {
      s1 += BYTESWAP32(aData[0]) + s2;
      s2 += BYTESWAP32(aData[1]) + s1;
      aData += 2;
    }while( aData<aEnd );
  }

  aOut[0] = s1;            将s1赋值给aOut[0]                                                                                    //��s1��ֵ��aOut[0] 
  aOut[1] = s2;            将s2赋值给aout[1]                                                                                    // ��s2��ֵ��aout[1] 
}

static void walShmBarrier(Wal *pWal){ 
  if( pWal->exclusiveMode!=WAL_HEAPMEMORY_MODE ){     如果pWal->exclusiveMode 不等�?                                              // ���pWal->exclusiveMode ������2
    sqlite3OsShmBarrier(pWal->pDbFd);
  }
}

/*
** Write the header information in pWal->hdr into the wal-index.�?标题信息写入pWal->hdr
**
** The checksum on pWal->hdr is updated before it is written. pWal ->hdr 的校验和更新是在它被写之�?
*/
static void walIndexWriteHdr(Wal *pWal){
  volatile WalIndexHdr *aHdr = walIndexHdr(pWal);                返回一个WalIndexHdr 结构指针         // ����һ��WalIndexHdr �ṹָ�� 
  const int nCksum = offsetof(WalIndexHdr, aCksum);              

  assert( pWal->writeLock );                         如果不为�?则程序终�?                            //�����Ϊ�� �������ֹ                  
  pWal->hdr.isInit = 1;                              初始值为1
  pWal->hdr.iVersion = WALINDEX_MAX_VERSION;          设置版本�?为WALINDEX_MAX_VERSION
  walChecksumBytes(1, (u8*)&pWal->hdr, nCksum, 0, pWal->hdr.aCksum);  进行校验
  memcpy((void *)&aHdr[1], (void *)&pWal->hdr, sizeof(WalIndexHdr));         memcpy函数的功能是从源src所指的内存地址的起始位置开始拷贝n个字节到目标dest所指的内存地址的起始位置中�?
  walShmBarrier(pWal);              调用  walShmBarrier（）
  memcpy((void *)&aHdr[0], (void *)&pWal->hdr, sizeof(WalIndexHdr));
}

/*
** This function encodes a single frame header and writes it to a buffer
** supplied by the caller. A frame-header is made up of a series of 4-byte big-endian integers, as follows:    这个结构编码单一帧头，它的作用是将其写入到由调用者提供的缓冲区，由下列组�?
**
**     0: Page number.
**     4: For commit records, the size of the database image in pages 
**        after the commit. For all other records, zero.
**     8: Salt-1 (copied from the wal-header)
**    12: Salt-2 (copied from the wal-header)
**    16: Checksum-1.
**    20: Checksum-2.
*/

//����WALԤд��־���
static void walEncodeFrame(
  Wal *pWal,                      /* The write-ahead log */  预写日志
  u32 iPage,                      /* Database page number for frame */  对某一帧在数据库中那一�?
  u32 nTruncate,                  /* New db size (or 0 for non-commit frames) */ 新db 大小
  u8 *aData,                      /* Pointer to page data */  指向 页数据的指针
  u8 *aFrame                      /* OUT: Write encoded frame here */
){
  int nativeCksum;                /* True for native byte-order checksums */ 
  u32 *aCksum = pWal->hdr.aFrameCksum;  
  assert( WAL_FRAME_HDRSIZE==24 );           如果为假，则终止程序
  sqlite3Put4byte(&aFrame[0], iPage);
  sqlite3Put4byte(&aFrame[4], nTruncate);
  memcpy(&aFrame[8], pWal->hdr.aSalt, 8);

  nativeCksum = (pWal->hdr.bigEndCksum==SQLITE_BIGENDIAN);
  walChecksumBytes(nativeCksum, aFrame, 8, aCksum, aCksum);
  walChecksumBytes(nativeCksum, aData, pWal->szPage, aCksum, aCksum);

  sqlite3Put4byte(&aFrame[16], aCksum[0]);
  sqlite3Put4byte(&aFrame[20], aCksum[1]);
}

/*
** Check to see if the frame with header in aFrame[] and content检查aFrame【】和adata[] 是否正确，如果正确，填写*piPage  pnTruncate指针return true
** in aData[] is valid.  If it is a valid frame, fill *piPage and
** *pnTruncate and return true.  Return if the frame is not valid.
*/
//WALɾ���ڵ���
static int walDecodeFrame(
  Wal *pWal,                      /* The write-ahead log */   
  u32 *piPage,                    /* OUT: Database page number for frame */  数据库页�?
  u32 *pnTruncate,                /* OUT: New db size (or 0 if not commit) */新db大小
  u8 *aData,                      /* Pointer to page data (for checksum) */ 指向页数据的指针
  u8 *aFrame                      /* Frame data */ 框架数据
){
  int nativeCksum;                /* True for native byte-order checksums */   检查�?
  u32 *aCksum = pWal->hdr.aFrameCksum;
  u32 pgno;                       /* Page number of the frame */ 定义数据库的页码
  assert( WAL_FRAME_HDRSIZE==24 );     如果为假，则终止程序

  /* A frame is only valid if the salt values in the frame-header
  ** match the salt values in the wal-header. 
  */
  if( memcmp(&pWal->hdr.aSalt, &aFrame[8], 8)!=0 ){  如果不匹配则 
    return 0;
  }

  /* A frame is only valid if the page number is creater than zero.
  */
  pgno = sqlite3Get4byte(&aFrame[0]);  为pgno赋�?
  if( pgno==0 ){  为真，则
    return 0;
  }

  /* A frame is only valid if a checksum of the WAL header,
  ** all prior frams, the first 16 bytes of this frame-header,  好像是前16和后8个字节代表的信息
  ** and the frame-data matches the checksum in the last 8 
  ** bytes of this frame-header.
  */
  nativeCksum = (pWal->hdr.bigEndCksum==SQLITE_BIGENDIAN);    ？？�?
  walChecksumBytes(nativeCksum, aFrame, 8, aCksum, aCksum);
  walChecksumBytes(nativeCksum, aData, pWal->szPage, aCksum, aCksum);
  if( aCksum[0]!=sqlite3Get4byte(&aFrame[16]) 
   || aCksum[1]!=sqlite3Get4byte(&aFrame[20]) 
  ){
    /* Checksum failed. */
    return 0;
  }

  /* If we reach this point, the frame is valid.  Return the page number
  ** and the new database size.如果 帧是有效的，返回页数和新的数据库大小
  */
  *piPage = pgno;
  *pnTruncate = sqlite3Get4byte(&aFrame[4]);
  return 1;
}
      

#if defined(SQLITE_TEST) && defined(SQLITE_DEBUG)
/*
** Names of locks.  This routine is used to provide debugging output and is not
** a part of an ordinary build.      获取Wal锁得命名  通过传入的参�?lockIdx 的值进行比�?返回锁名
*/

//����WAL������ д�����ָ�����������
static const char *walLockName(int lockIdx){   
  if( lockIdx==WAL_WRITE_LOCK ){   
    return "WRITE-LOCK";
  }else if( lockIdx==WAL_CKPT_LOCK ){
    return "CKPT-LOCK";
  }else if( lockIdx==WAL_RECOVER_LOCK ){
    return "RECOVER-LOCK";
  }else{
    static char zName[15];
    sqlite3_snprintf(sizeof(zName), zName, "READ-LOCK[%d]",
                     lockIdx-WAL_READ_LOCK(0));
    return zName;
  }
}
#endif /*defined(SQLITE_TEST) || defined(SQLITE_DEBUG) */
    

/*
** Set or release locks on the WAL.  Locks are either shared or exclusive.设置或释放一个锁，锁可能是一个共享或排斥�?
** A lock cannot be moved directly between shared and exclusive - it must go一个锁不能直接从共享锁移动到排斥，他必须进入解锁状�?
** through the unlocked state first.
**
** In locking_mode=EXCLUSIVE, all of these routines become no-ops.
*/
//����������������������Ҫͬʱ���ļ�ʱ������
static int walLockShared(Wal *pWal, int lockIdx){                        加共享锁
  int rc;                                                   返回�?
  if( pWal->exclusiveMode ) return SQLITE_OK;                    如果Wal在互斥模式下 ，则返回 �?
  rc = sqlite3OsShmLock(pWal->pDbFd, lockIdx, 1,
                        SQLITE_SHM_LOCK | SQLITE_SHM_SHARED);
  WALTRACE(("WAL%p: acquire SHARED-%s %s\n", pWal,
            walLockName(lockIdx), rc ? "failed" : "ok"));
  VVA_ONLY( pWal->lockError = (u8)(rc!=SQLITE_OK && rc!=SQLITE_BUSY); )
  return rc;
}
static void walUnlockShared(Wal *pWal, int lockIdx){                       释放共享�?        
  if( pWal->exclusiveMode ) return;
  (void)sqlite3OsShmLock(pWal->pDbFd, lockIdx, 1,
                         SQLITE_SHM_UNLOCK | SQLITE_SHM_SHARED);
  WALTRACE(("WAL%p: release SHARED-%s\n", pWal, walLockName(lockIdx)));
}
static int walLockExclusive(Wal *pWal, int lockIdx, int n){         加排它锁
  int rc;  
  if( pWal->exclusiveMode ) return SQLITE_OK;
  rc = sqlite3OsShmLock(pWal->pDbFd, lockIdx, n,
                        SQLITE_SHM_LOCK | SQLITE_SHM_EXCLUSIVE);
  WALTRACE(("WAL%p: acquire EXCLUSIVE-%s cnt=%d %s\n", pWal,
            walLockName(lockIdx), n, rc ? "failed" : "ok"));
  VVA_ONLY( pWal->lockError = (u8)(rc!=SQLITE_OK && rc!=SQLITE_BUSY); )
  return rc;
}
static void walUnlockExclusive(Wal *pWal, int lockIdx, int n){        释放排它�?
  if( pWal->exclusiveMode ) return;
  (void)sqlite3OsShmLock(pWal->pDbFd, lockIdx, n,
                         SQLITE_SHM_UNLOCK | SQLITE_SHM_EXCLUSIVE);
  WALTRACE(("WAL%p: release EXCLUSIVE-%s cnt=%d\n", pWal,
             walLockName(lockIdx), n));
}

/*
** Compute a hash on a page number.  The resulting hash value must land
** between 0 and (HASHTABLE_NSLOT-1).  The walHashNext() function advances
** the hash to the next value in the event of a collision.
*/
//����WALhash����������Hash����ʱ����
static int walHash(u32 iPage){         一个哈希值在 对应的那一�?的�?
  assert( iPage>0 );                    如果IPage>0 为假，终止程�?
  assert( (HASHTABLE_NSLOT & (HASHTABLE_NSLOT-1))==0 ); 如果不等�?则，终止程序
  return (iPage*HASHTABLE_HASH_1) & (HASHTABLE_NSLOT-1);返回页对应的哈希�?
}
static int walNextHash(int iPriorHash){   如果发生碰撞�?
  return (iPriorHash+1)&(HASHTABLE_NSLOT-1);
}

/* 
** Return pointers to the hash table and page number array stored on      返回存储在哈希表的指针和页码数组页面iHash wal-index
** page iHash of the wal-index. The wal-index is broken into 32KB pages     wal-index分为32 kb的页面　编号�?开�?
** numbered starting from 0. 
**
** Set output variable *paHash to point to the start of the hash table   在wal-index文件中设置输出变�? paHash哈希表的开�?
** in the wal-index file. Set *piZero to one less than the frame        piZero设置为一个小于第一帧由这个哈希表索�?
** number of the first frame indexed by this hash table. If a            　如果一个槽在哈希表中设置为N,它指的是帧数(* piZero + N)在日志中�?
** slot in the hash table is set to N, it refers to frame number 
** (*piZero+N) in the log.
**
** Finally, set *paPgno so that *paPgno[1] is the page number of the      最�?设置* paPgno�? paPgno[1]的页码第一帧索引的哈希�?�?* piZero + 1�?
** first frame indexed by the hash table, frame (*piZero+1).
*/


//����Hash���غ���

static int walHashGet(                返回文件第i页的指针
  Wal *pWal,                      /* WAL handle */    Wal文件
  int iHash,                      /* Find the iHash'th table */找到哈希值对应的�?
  volatile ht_slot **paHash,      /* OUT: Pointer to hash index */ hash索引的指�?
  volatile u32 **paPgno,          /* OUT: Pointer to page number array */页码数组的指�?
  u32 *piZero                     /* OUT: Frame associated with *paPgno[0] */�?paPgno[0] 定义一个指�?
){
  int rc;                         /* Return code */  返回�?
  volatile u32 *aPgno;              

  rc = walIndexPage(pWal, iHash, &aPgno);  获取日志文件第i页的指针
  assert( rc==SQLITE_OK || iHash>0 );	 判断是否成功，不成功则终止程�?

  if( rc==SQLITE_OK ){                     如果获取成功
    u32 iZero;                           定义 U32 的变�?    
    volatile ht_slot *aHash;             定义一�?ht_slot�?指针变量

    aHash = (volatile ht_slot *)&aPgno[HASHTABLE_NPAGE];   HASHTABLE_NPAGE�?096 ，给aHash赋�?
    if( iHash==0 ){                                           当Ihash 值为 0 �?
      aPgno = &aPgno[WALINDEX_HDR_SIZE/sizeof(u32)];         aPgno等值方�?
      iZero = 0;                                              IZero �?
    }else{                                                     iHash 不为0
      iZero = HASHTABLE_NPAGE_ONE + (iHash-1)*HASHTABLE_NPAGE; IZero 的赋值方�?
    }
  
    *paPgno = &aPgno[-1];  为paPgno 赋�?
    *paHash = aHash;       为PaHash赋�?  
    *piZero = iZero;       为PiZero 赋�?
  }
  return rc;             返回 rc
}

/*
** Return the number of the wal-index page that contains the hash-table
** and page-number array that contain entries corresponding to WAL frame
** iFrame. The wal-index is broken up into 32KB pages. Wal-index pages 
** are numbered starting from 0.                                   返回的数量wal-index页面包含哈希表和页码数组包含条目对应于WAL框架iFrame。wal-index分为32 kb的页面。Wal-index页面�?开始编号�?
*/
����WAL���ҳ
static int walFramePage(u32 iFrame){
  int iHash = (iFrame+HASHTABLE_NPAGE-HASHTABLE_NPAGE_ONE-1) / HASHTABLE_NPAGE; 计算 IHash的�?
  assert( (iHash==0 || iFrame>HASHTABLE_NPAGE_ONE)     判断是否终止程序
       && (iHash>=1 || iFrame<=HASHTABLE_NPAGE_ONE)
       && (iHash<=1 || iFrame>(HASHTABLE_NPAGE_ONE+HASHTABLE_NPAGE))
       && (iHash>=2 || iFrame<=HASHTABLE_NPAGE_ONE+HASHTABLE_NPAGE)
       && (iHash<=2 || iFrame>(HASHTABLE_NPAGE_ONE+2*HASHTABLE_NPAGE))
  );
  return iHash;    返回 Ihash
}

/*
** Return the page number associated with frame iFrame in this WAL. 返回与IFrame对应�?页的�?
*/
static u32 walFramePgno(Wal *pWal, u32 iFrame){
  int iHash = walFramePage(iFrame);  调用walFramePage函数获取与Iframe对应�?索引页第几页
  if( iHash==0 ){           如果Ihash�?
    return pWal->apWiData[0][WALINDEX_HDR_SIZE/sizeof(u32) + iFrame - 1];  返回 与iFrame对在Wal中的�?
  }
  return pWal->apWiData[iHash][(iFrame-1-HASHTABLE_NPAGE_ONE)%HASHTABLE_NPAGE];返回 与iFrame对在Wal中的�?
}

/*
** Remove entries from the hash table that point to WAL slots greater 从哈希表中删除条目指向WAL　　比pWal - > hdr.mxFrame更大 
** than pWal->hdr.mxFrame.
**
** This function is called whenever pWal->hdr.mxFrame is decreased due 这个函数被调用时pWal - > hdr。mxFrame下降是由于回滚或保存�?
** to a rollback or savepoint.
**
** At most only the hash table containing pWal->hdr.mxFrame needs to be
** updated.  Any later hash tables will be automatically cleared when
** pWal->hdr.mxFrame advances to the point where those hash tables are
** actually needed.最多只包含pWal - > hdr.mxFrame需要更新。任何后来哈希表时将自动清除 pWal - > hdr.mxFrame进步,这些哈希表实际需要�?
*/
static void walCleanupHash(Wal *pWal){
  volatile ht_slot *aHash = 0;    /* Pointer to hash table to clear */指向要删除的指针
  volatile u32 *aPgno = 0;        /* Page number array for hash table */页码为哈希表数组
  u32 iZero = 0;                  /* frame == (aHash[x]+iZero) */
  int iLimit = 0;                 /* Zero values greater than this */ 大于这个�?
  int nByte;                      /* Number of bytes to zero in aPgno[] */
  int i;                          /* Used to iterate through aHash[] */ 变量用于循环

  assert( pWal->writeLock ); 看Wal在写事务中，在的�?终止程序
  testcase( pWal->hdr.mxFrame==HASHTABLE_NPAGE_ONE-1 );调用testcase（）函数 测试评估
  testcase( pWal->hdr.mxFrame==HASHTABLE_NPAGE_ONE );调用testcase（）函数 测试评估
  testcase( pWal->hdr.mxFrame==HASHTABLE_NPAGE_ONE+1 );调用testcase（）函数 测试评估

  if( pWal->hdr.mxFrame==0 ) return;   如果索引值为0 ，则返回�?

  /* Obtain pointers to the hash-table and page-number array containing 
  ** the entry that corresponds to frame pWal->hdr.mxFrame. It is guaranteed
  ** that the page said hash-table and array reside on is already mapped.获取包含哈希表和页码的指针数组的条目对应帧pWal - > hdr.mxFrame。这是保证页面说哈希表和数组驻留在已经映射�?
  */
  assert( pWal->nWiData>walFramePage(pWal->hdr.mxFrame) ); 判断是否终止程序
  assert( pWal->apWiData[walFramePage(pWal->hdr.mxFrame)] );判断是否终止程序
  walHashGet(pWal, walFramePage(pWal->hdr.mxFrame), &aHash, &aPgno, &iZero);

  /* Zero all hash-table entries that correspond to frame numbers greater
  ** than pWal->hdr.mxFrame.
  */
  iLimit = pWal->hdr.mxFrame - iZero; 获取ilimit�?
  assert( iLimit>0 );            如果 ilimit 小于0 则程序终�?
  for(i=0; i<HASHTABLE_NSLOT; i++){ 对aHash进行遍历�?
    if( aHash[i]>iLimit ){            如果hash值超过限�?�?
      aHash[i] = 0;                       则将�?赋值为0
    }
  }
  
  /* Zero the entries in the aPgno array that correspond to frames with
  ** frame numbers greater than pWal->hdr.mxFrame. 
  */
  nByte = (int)((char *)aHash - (char *)&aPgno[iLimit+1]); 获取 nByte 的�?
  memset((void *)&aPgno[iLimit+1], 0, nByte);          �?aPgno分配内存

#ifdef SQLITE_ENABLE_EXPENSIVE_ASSERT
  /* Verify that the every entry in the mapping region is still reachable
  ** via the hash table even after the cleanup. 确保每一个映射区�?可以通过映射达到
  */
  if( iLimit ){                如果 Ilimit 不为0
    int i;           /* Loop counter */ 循环计数
    int iKey;        /* Hash key */    哈希�?
    for(i=1; i<=iLimit; i++){              循环
      for(iKey=walHash(aPgno[i]); aHash[iKey]; iKey=walNextHash(iKey)){ 获取IKey的值，判断aHash是否为空，获取下一个hash�?
        if( aHash[iKey]==i ) break;  如果aHash的值与下标相同 则跳出循�?
      }
      assert( aHash[iKey]==i );       如果aHash的值与下标不相同，则终止程�?
    }
  }
#endif /* SQLITE_ENABLE_EXPENSIVE_ASSERT */
}


/*
** Set an entry in the wal-index that will map database page number
** pPage into WAL frame iFrame.       在Wal-index设置一个标记作�?可以�?数据页码映射�?Wal帧中的第iFrame
*/

//����WAL����׷�Ӻ���
static int walIndexAppend(Wal *pWal, u32 iFrame, u32 iPage){
  int rc;                         /* Return code */ 返回�?
  u32 iZero = 0;                  /* One less than frame number of aPgno[1] */ 小于aPgno�?】的帧号
  volatile u32 *aPgno = 0;        /* Page number array */ 变量 数组页码
  volatile ht_slot *aHash = 0;    /* Hash table */    哈希�?

  rc = walHashGet(pWal, walFramePage(iFrame), &aHash, &aPgno, &iZero);

  /* Assuming the wal-index file was successfully mapped, populate the
  ** page number array and hash table entry.
  */
  if( rc==SQLITE_OK ){                   如果调用函数成功 
    int iKey;                     /* Hash table key */  哈希�?
    int idx;                      /* Value to write to hash-table slot */ 写入hash槽的�?
    int nCollide;                 /* Number of hash collisions */ 哈希碰撞数目

    idx = iFrame - iZero;       �?idx的�?                  
    assert( idx <= HASHTABLE_NSLOT/2 + 1 );  终止程序
    
    /* If this is the first entry to be added to this hash-table, zero the
    ** entire hash table and aPgno[] array before proceding. 
    */
    if( idx==1 ){ 
      int nByte = (int)((u8 *)&aHash[HASHTABLE_NSLOT] - (u8 *)&aPgno[1]);
      memset((void*)&aPgno[1], 0, nByte);    为aPgno分配内存，并初始化为0
    }

    /* If the entry in aPgno[] is already set, then the previous writer
    ** must have exited unexpectedly in the middle of a transaction (after
    ** writing one or more dirty pages to the WAL to free up memory). 
    ** Remove the remnants of that writers uncommitted transaction from 
    ** the hash-table before writing any new entries.
    */
    if( aPgno[idx] ){            aPgno[idx] 不为0
      walCleanupHash(pWal);
      assert( !aPgno[idx] );         终止程序
    }

    /* Write the aPgno[] array entry and the hash-table slot. */ 
    nCollide = idx;       为nCollide 赋�?
    for(iKey=walHash(iPage); aHash[iKey]; iKey=walNextHash(iKey)){ 获取ikey值，判段aHash 
      if( (nCollide--)==0 ) return SQLITE_CORRUPT_BKPT;  如果碰撞数为0 则返�?SQLITE_CORRUPT_BKPT
    }
    aPgno[idx] = iPage;        为apgno[] 赋�?
    aHash[iKey] = (ht_slot)idx;  第ikey的hash值为 idx

#ifdef SQLITE_ENABLE_EXPENSIVE_ASSERT
    /* Verify that the number of entries in the hash table exactly equals
    ** the number of entries in the mapping region. 确保 hash 表的入口�?映射区域的入口的数目相同
    {
      int i;           /* Loop counter */ 循环计数
      int nEntry = 0;  /* Number of entries in the hash table */ 入口数目�?
      for(i=0; i<HASHTABLE_NSLOT; i++){ if( aHash[i] ) nEntry++; } 进行循环
      assert( nEntry==idx );    终止程序
    }

    /* Verify that the every entry in the mapping region is reachable
    ** via the hash table.  This turns out to be a really, really expensive
    ** thing to check, so only do this occasionally - not on every
    ** iteration.　验证每个条目映射区域是可获得的通过哈希表这被证明是一个非常非常昂贵要检�?所以只是偶尔这样做——而不是在每一个迭代�?
    */
    if( (idx&0x3ff)==0 ){  
      int i;           /* Loop counter */ 循环变量
      for(i=1; i<=idx; i++){    对idex进行遍历
        for(iKey=walHash(aPgno[i]); aHash[iKey]; iKey=walNextHash(iKey)){
          if( aHash[iKey]==i ) break;
        }
        assert( aHash[iKey]==i ); 终止程序
      }
    }
#endif /* SQLITE_ENABLE_EXPENSIVE_ASSERT */
  }


  return rc;
}


/*
** Recover the wal-index by reading the write-ahead log file. 
**
** This routine first tries to establish an exclusive lock on the
** wal-index to prevent other threads/processes from doing anything
** with the WAL or wal-index while recovery is running.  The
** WAL_RECOVER_LOCK is also held so that other threads will know
** that this thread is running recovery.  If unable to establish
** the necessary locks, this routine returns SQLITE_BUSY.　通过阅读写前恢复wal- wal-index阻止其他线程/与WAL或wal-index而恢复运行。的 WAL_RECOVER_LOCK也举�?其他线程就会知道,这个线程运行的复苏。如果无法建立必要的�?这个例程返回SQLITE_BUSY�?
*/

//����WAL�����ָ�����
static int walIndexRecover(Wal *pWal){  返回是否加锁进行恢复，不成功返回 SQLite——busy
  int rc;                         /* Return Code */  返回�?
  i64 nSize;                      /* Size of log file */  log file的大�?
  u32 aFrameCksum[2] = {0, 0};           定义 aFrameCksum 数组
  int iLock;                      /* Lock offset to lock for checkpoint */ 定义�?
  int nLock;                      /* Number of locks to hold */ 第几个锁

  /* Obtain an exclusive lock on all byte in the locking range not already
  ** locked by the caller. The caller is guaranteed to have locked the
  ** WAL_WRITE_LOCK byte, and may have also locked the WAL_CKPT_LOCK byte.
  ** If successful, the same bytes that are locked here are unlocked before
  ** this function returns./ *获取所有字节的独占锁锁定范围不了被调用者。调用者保证锁上了WAL_WRITE_LOCK字节,可能也锁定WAL_CKPT_LOCK字节如果成功,
  */
  assert( pWal->ckptLock==1 || pWal->ckptLock==0 );如果锁类型为 ，则终止
  assert( WAL_ALL_BUT_WRITE==WAL_WRITE_LOCK+1 );
  assert( WAL_CKPT_LOCK==WAL_ALL_BUT_WRITE );
  assert( pWal->writeLock ); 如果Wal在写事务下，则终止程�?
  iLock = WAL_ALL_BUT_WRITE + pWal->ckptLock; wal_all_but_write �?
  
  nLock = SQLITE_SHM_NLOCK - iLock;
  
  rc = walLockExclusive(pWal, iLock, nLock); 是否获取排它�?
  
  if( rc ){  如果获取成功�?
    return rc; 返回 rc
  }
  WALTRACE(("WAL%p: recovery begin...\n", pWal));

  memset(&pWal->hdr, 0, sizeof(WalIndexHdr));  为pWal->hdr 分配空间 并初始化�?

  rc = sqlite3OsFileSize(pWal->pWalFd, &nSize); Wal文件的大小，获取返回�?
  if( rc!=SQLITE_OK ){   如果获取不成�?
    goto recovery_error; 跳转�?recovery_error
  }

  if( nSize>WAL_HDRSIZE ){            nSize �?2
    u8 aBuf[WAL_HDRSIZE];         /* Buffer to load WAL header into */ 获取Wal头数�?
    u8 *aFrame = 0;               /* Malloc'd buffer to load entire frame */
    int szFrame;                  /* Number of bytes in buffer aFrame[] */
    u8 *aData;                    /* Pointer to data part of aFrame buffer */
    int iFrame;                   /* Index of last frame read */
    i64 iOffset;                  /* Next offset to read from log file */
    int szPage;                   /* Page size according to the log */根据日志页面大小
    u32 magic;                    /* Magic value read from WAL header */
    u32 version;                  /* Magic value read from WAL header */
    int isValid;                  /* True if this frame is valid */

    /* Read in the WAL header. */
    rc = sqlite3OsRead(pWal->pWalFd, aBuf, WAL_HDRSIZE, 0); 获取Wal头数�?
    if( rc!=SQLITE_OK ){ 如果 不成�?
      goto recovery_error; 跳转  recovery_error
    }

    /* If the database page size is not a power of two, or is greater than
    ** SQLITE_MAX_PAGE_SIZE, conclude that the WAL file contains no valid 
    ** data. Similarly, if the 'magic' value is invalid, ignore the whole
    ** WAL file.如果数据库页面大小不是一个两�?或者大于　　SQLITE_MAX_PAGE_SIZE,得出这样的结�?WAL文件不包含有效的　数据。类似地,如果‘魔法’值是无效�?忽略了整�? WAL文件�?
    */
    magic = sqlite3Get4byte(&aBuf[0]);获取数据
    szPage = sqlite3Get4byte(&aBuf[8]);获取数据大小
    if( (magic&0xFFFFFFFE)!=WAL_MAGIC                 WAL_MAGIC 0x377f0682
     || szPage&(szPage-1)          �?szpage是否�?的倍数
     || szPage>SQLITE_MAX_PAGE_SIZE       SQLITE_MAX_PAGE_SIZE 65536
     || szPage<512                         
    ){
      goto finished; 跳转�?finished
    }
    pWal->hdr.bigEndCksum = (u8)(magic&0x00000001); 获取校验�?
    pWal->szPage = szPage;   将pWal->szpage 赋�?
    pWal->nCkpt = sqlite3Get4byte(&aBuf[12]);  wal-header检查点序列计数�?
    memcpy(&pWal->hdr.aSalt, &aBuf[16], 8); 将aBuf的数据的8位赋给pWal->hdr.aSalt

    /* Verify that the WAL header checksum is correct */ 核实Wal的头数据的检验室正确�?
    walChecksumBytes(pWal->hdr.bigEndCksum==SQLITE_BIGENDIAN,          调用walchecksumBytes进行Wal header的检�?
        aBuf, WAL_HDRSIZE-2*4, 0, pWal->hdr.aFrameCksum
    );
    if( pWal->hdr.aFrameCksum[0]!=sqlite3Get4byte(&aBuf[24])             如果检验结果与 ABuf[] 不相�?
     || pWal->hdr.aFrameCksum[1]!=sqlite3Get4byte(&aBuf[28])
    ){
      goto finished;                     跳转�?finished
    }

    /* Verify that the version number on the WAL format is one that
    ** are able to understand */                 验证WAL格式是一个版本号能够理解
    version = sqlite3Get4byte(&aBuf[4]); 获取Wal头数据中version数据
    if( version!=WAL_MAX_VERSION ){       如果获取到的version数据 不等于WAL_MAX_VERSION
      rc = SQLITE_CANTOPEN_BKPT;                rc获取�?
      goto finished;                   跳转�?finished
    }

    /* Malloc a buffer to read frames into. */ 分配一个缓冲区 �?frames读入
    szFrame = szPage + WAL_FRAME_HDRSIZE;       WAL_FRAME_HDRSIZE 24  �?一页的大小
    aFrame = (u8 *)sqlite3_malloc(szFrame);    �?aFrame分配 内存 
    if( !aFrame ){             如果分配不成�?
      rc = SQLITE_NOMEM;       �?rc 赋�?
      goto recovery_error;    跳转�?recovery_error
    }
    aData = &aFrame[WAL_FRAME_HDRSIZE]; 将aFrame 的地址赋给 aData

    /* Read all frames from the log file. */ 种日志文件中读出所有的frame
    iFrame = 0;
    for(iOffset=WAL_HDRSIZE; (iOffset+szFrame)<=nSize; iOffset+=szFrame){          WAL_HDRSIZE 32  如果iOffset+szFrame小于日志文件大小  
      u32 pgno;                   /* Database page number for frame */      框架�?某数据页�?
      u32 nTruncate;              /* dbsize field from frame header */       帧头中数据库大小 

      /* Read and decode the next log frame. */  读入和解码日志帧
      iFrame++;                                   iFrame 自加
      rc = sqlite3OsRead(pWal->pWalFd, aFrame, szFrame, iOffset); 调用 SQLite3Osread函数 
      if( rc!=SQLITE_OK ) break;   如果 rc 不等�?SQLIte——ok 则跳出循�?
      isValid = walDecodeFrame(pWal, &pgno, &nTruncate, aData, aFrame);  获取解码 ，返回一�?看是否成�?
      if( !isValid ) break;         如果不成�?，则跳出循环
      rc = walIndexAppend(pWal, iFrame, pgno);
      if( rc!=SQLITE_OK ) break;

      /* If nTruncate is non-zero, this is a commit record. */ 如果nTruncate是非空的，这个作为一个提交记�?
      if( nTruncate ){                    判断是否为空
        pWal->hdr.mxFrame = iFrame;   为最新的有效的索引赋�?
        pWal->hdr.nPage = nTruncate;  Wal文件有多少页 
        pWal->hdr.szPage = (u16)((szPage&0xff00) | (szPage>>16)); 一页有多大
        testcase( szPage<=32768 );   调用测试示例
        testcase( szPage>=65536 );
        aFrameCksum[0] = pWal->hdr.aFrameCksum[0];将hdr的检验和的值赋给aFrameCksum[0]
        aFrameCksum[1] = pWal->hdr.aFrameCksum[1];将hdr的检验和的值赋给aFrameCksum[0]
      }
    }

    sqlite3_free(aFrame); 释放指针
  }

finished:                      goto 的标�?
  if( rc==SQLITE_OK ){                  r如果 rc �?Sqlite——ok
    volatile WalCkptInfo *pInfo;            定义校验信息指针变量  pInfo
    int i;                                  变量i
    pWal->hdr.aFrameCksum[0] = aFrameCksum[0]; 将hdr的检验和的值赋给aFrameCksum[0]
    pWal->hdr.aFrameCksum[1] = aFrameCksum[1];将hdr的检验和的值赋给aFrameCksum[0]
    walIndexWriteHdr(pWal); 调用函数 

    /* Reset the checkpoint-header. This is safe because this thread is 
    ** currently holding locks that exclude all other readers, writers and
    ** checkpointers. 重设 checkpoint-header ，这个线程会加排他锁
    */
    pInfo = walCkptInfo(pWal); 获取checkpoint信息
    pInfo->nBackfill = 0;    将nBackfill 赋值为0
    pInfo->aReadMark[0] = 0;将aReadMark赋值为0
    for(i=1; i<WAL_NREADER; i++) pInfo->aReadMark[i] = READMARK_NOT_USED; 为aReadMar[]赋值READMARK_NOT_USED
    if( pWal->hdr.mxFrame ) pInfo->aReadMark[1] = pWal->hdr.mxFrame; 如果MxFrame有效 �?

    /* If more than one frame was recovered from the log file, report an
    ** event via sqlite3_log(). This is to help with identifying performance
    ** problems caused by applications routinely shutting down without
    ** checkpointing the log file.            如果不止一个框架从日志文件中恢复过�?一个报告事件通过sqlite3_log()。这是帮助识别性能问题造成应用程序经常关闭检查点的日志文件�?
    */
    if( pWal->hdr.nPage ){            
      sqlite3_log(SQLITE_OK, "Recovered %d frames from WAL file %s",
          pWal->hdr.nPage, pWal->zWalName
      );
    }
  }

recovery_error:   goto 标记
  WALTRACE(("WAL%p: recovery %s\n", pWal, rc ? "failed" : "ok"));
  walUnlockExclusive(pWal, iLock, nLock); 调用解锁函数
  return rc;       返回 rc
}

/*
** Close an open wal-index. 关闭 开发的 wal-index
*/
//����WAL�����رպ���
static void walIndexClose(Wal *pWal, int isDelete){ 	 								
  if( pWal->exclusiveMode==WAL_HEAPMEMORY_MODE ){     如果 Wal 的在 对内存模�?
    int i;                                           定义变量
    for(i=0; i<pWal->nWiData; i++){             循环nWiData
      sqlite3_free((void *)pWal->apWiData[i]);     释放指针
      pWal->apWiData[i] = 0;                     将apWiData赋值为0
    }
  }else{
    sqlite3OsShmUnmap(pWal->pDbFd, isDelete); 调用sqlite3OsShmUnmap 函数
  }
}

/* 
** Open a connection to the WAL file zWalName. The database file must 
** already be opened on connection pDbFd. The buffer that zWalName points
** to must remain valid for the lifetime of the returned Wal* handle.　打开一个连接文件zWalName的wal。数据库文件必须 pDbFd已经打开连接。zWalName点的缓冲区必须保持有效的生命周期内返回Wal 处理�?
**
** A SHARED lock should be held on the database file when this function
** is called. The purpose of this SHARED lock is to prevent any other
** client from unlinking the WAL or wal-index file. If another process
** were to do this just after this client opened one of these files, the
** system would be badly broken.一个共享锁时应该在数据库文件这个函数。这个共享锁的目的是防止任何其他　 客户解除WAL或wal-index文件的链接。如果另一个进程后这样做只是这个客户端打开这些文件之一, 系统将严重破坏�?
**
** If the log file is successfully opened, SQLITE_OK is returned and 
** *ppWal is set to point to a new WAL handle. If an error occurs,
** an SQLite error code is returned and *ppWal is left unmodified.如果成功打开日志文件,SQLITE_OK和返回ppWal设置为指向一个新的WAL处理。如果出现错�?  一个SQLite返回错误代码�? ppWal修改的�?
*/
//�����WAL����
int sqlite3WalOpen( 
  sqlite3_vfs *pVfs,              /* vfs module to open wal and wal-index */ vfs 打开Wal 和wal-index
  sqlite3_file *pDbFd,            /* The open database file */ 数据库文�?
  const char *zWalName,           /* Name of the WAL file */   Wal文件的名
  int bNoShm,                     /* True to run in heap-memory mode */ 在堆内存中运行则为真
  i64 mxWalSize,                  /* Truncate WAL to this size on reset */ 重设Wal文件的大�?
  Wal **ppWal                     /* OUT: Allocated Wal handle */ 分配Wal
){
  int rc;                         /* Return Code */ 返回�?
  Wal *pRet;                      /* Object to allocate and return */分配和返回对�?
  int flags;                      /* Flags passed to OsOpen() */ 进入osOpen的标�?

  assert( zWalName && zWalName[0] ); 终止程序
  assert( pDbFd );终止程序

  /* In the amalgamation, the os_unix.c and os_win.c source files come before
  ** this source file.  Verify that the #defines of the locking byte offsets
  ** in os_unix.c and os_win.c agree with the WALINDEX_LOCK_OFFSET value.在融�?os_unix。c和os_win。c源文件这个源文件。确�?锁定字节偏移量的定义　　在os_unix。c和os_win。c同意WALINDEX_LOCK_OFFSET值�?
  */
#ifdef WIN_SHM_BASE
  assert( WIN_SHM_BASE==WALINDEX_LOCK_OFFSET );
#endif
#ifdef UNIX_SHM_BASE
  assert( UNIX_SHM_BASE==WALINDEX_LOCK_OFFSET );
#endif


  /* Allocate an instance of struct Wal to return. */ 分配一个Wal实例作为返回
  *ppWal = 0;              设置值为0
  pRet = (Wal*)sqlite3MallocZero(sizeof(Wal) + pVfs->szOsFile); 重设Wal文件
  if( !pRet ){              如果设置不成�?
    return SQLITE_NOMEM;     返回SqLite_NOMEM
  }

  pRet->pVfs = pVfs;     为pRet-> pVfs 赋�?
  pRet->pWalFd = (sqlite3_file *)&pRet[1]; 为pRet-> pVfs 赋�?
  pRet->pDbFd = pDbFd;为pRet->pWalFd赋�?
  pRet->readLock = -1;�?pRet->readLock 赋�?
  pRet->mxWalSize = mxWalSize;为pRet->mxWalSize 赋�?
  pRet->zWalName = zWalName;为pRet->zWalName赋�?
  pRet->syncHeader = 1;为pRet->syncHeader赋�?
  pRet->padToSectorBoundary = 1;为pRet->padToSectorBoundary赋�?
  pRet->exclusiveMode = (bNoShm ? WAL_HEAPMEMORY_MODE: WAL_NORMAL_MODE);为pRet->exclusiveMode赋�?

  /* Open file handle on the write-ahead log file. */写前日志文件打开文件句柄�?
  flags = (SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_WAL); falgs标记
  rc = sqlite3OsOpen(pVfs, zWalName, pRet->pWalFd, flags, &flags); 调用sqlite3OSOpen（）函数
  if( rc==SQLITE_OK && flags&SQLITE_OPEN_READONLY ){    
    pRet->readOnly = WAL_RDONLY;      设置pRet->readonly 的�?
  }

  if( rc!=SQLITE_OK ){ 如果rc 不成�?
    walIndexClose(pRet, 0);  调用walIndexClose函数
    sqlite3OsClose(pRet->pWalFd);  调用sqlite3OSclose（）函数
    sqlite3_free(pRet); 释放 pRet函数
  }else{
    int iDC = sqlite3OsDeviceCharacteristics(pRet->pWalFd); 调用系统函数
    if( iDC & SQLITE_IOCAP_SEQUENTIAL ){ pRet->syncHeader = 0; }
    if( iDC & SQLITE_IOCAP_POWERSAFE_OVERWRITE ){
      pRet->padToSectorBoundary = 0;    设置Wal中的padToSectorBoundary属�?
    }
    *ppWal = pRet;  将新的wal赋给ppWal 参数
    WALTRACE(("WAL%d: opened\n", pRet)); 
  }
  return rc; 返回rc
}

/*
** Change the size to which the WAL file is truncated on each reset. 改变 Wal 文件的大小当每次重设都会使Wal文件减小
*/
void sqlite3WalLimit(Wal *pWal, i64 iLimit){
  if( pWal ) pWal->mxWalSize = iLimit;
}

/*
** Find the smallest page number out of all pages held in the WAL that
** has not been returned by any prior invocation of this method on the
** same WalIterator object.   Write into *piFrame the frame index where
** that page was last written into the WAL.  Write into *piPage the page
** number.找到最小的页码的所有页面的在身之前并没有被返回的任何调用这个方法的WalIterator对象相同。写�? piFrame帧索引页面最后写入在身。写入输送管号�?
**
** Return 0 on success.  If there are no pages in the WAL with a page
** number larger than *piPage, then return 1.成功返回0。如果没有页面WAL的页面数量大于输送管,然后返回1�?
*/
//����WAL��һ��������
static int walIteratorNext(
  WalIterator *p,               /* Iterator */  迭代
  u32 *piPage,                  /* OUT: The page number of the next page */ 下一�?
  u32 *piFrame                  /* OUT: Wal frame index of next page */ 下一页的Wal索引
){
  u32 iMin;                     /* Result pgno must be greater than iMin */ 返回 pgno 它比iMin �?
  u32 iRet = 0xFFFFFFFF;        /* 0xffffffff is never a valid page number */ 不是一个有效的 页数
  int i;                        /* For looping through segments */   循环参数

  iMin = p->iPrior;                           获取 迭代 �?   
  assert( iMin<0xffffffff );             如果 iMin 的�? oxfffffffff ,说明 imin 不是有效�?
  for(i=p->nSegment-1; i>=0; i--){         
    struct WalSegment *pSegment = &p->aSegment[i]; 定义WalSegment 的变�?并赋�?
    while( pSegment->iNext<pSegment->nEntry ){  �?inext 小于 nEntry 
      u32 iPg = pSegment->aPgno[pSegment->aIndex[pSegment->iNext]]; 定义变量 并赋�?aPgno的�?
      if( iPg>iMin ){        如果ipg 大于 iMIn
        if( iPg<iRet ){    ipg不是有效�?
          iRet = iPg;       iRet 就赋�?iPg
          *piFrame = pSegment->iZero + pSegment->aIndex[pSegment->iNext]; 下一个Wal索引 的�?
        }
        break;  跳出循环  
      }
      pSegment->iNext++; 进行 iNext 进行自加
    }
  }

  *piPage = p->iPrior = iRet; �?iPrior 等于 iret
  return (iRet==0xFFFFFFFF);  对iRet赋值并返回
}

/*
** This function merges two sorted lists into a single sorted list. 合并
**
** aLeft[] and aRight[] are arrays of indices.  The sort key is
** aContent[aLeft[]] and aContent[aRight[]].  Upon entry, the following
** is guaranteed for all J<K:
**
**        aContent[aLeft[J]] < aContent[aLeft[K]]
**        aContent[aRight[J]] < aContent[aRight[K]]
**
** This routine overwrites aRight[] with a new (probably longer) sequence
** of indices such that the aRight[] contains every index that appears in
** either aLeft[] or the old aRight[] and such that the second condition
** above is still met.
**
** The aContent[aLeft[X]] values will be unique for all X.  And the
** aContent[aRight[X]] values will be unique too.  But there might be
** one or more combinations of X and Y such that
**
**      aLeft[X]!=aRight[Y]  &&  aContent[aLeft[X]] == aContent[aRight[Y]]
**
** When that happens, omit the aLeft[X] and use the aRight[Y] index.
*/
//����WAL�ĺϲ�
static void walMerge(                  
  const u32 *aContent,            /* Pages in wal - keys for the sort */ 
  ht_slot *aLeft,                 /* IN: Left hand input list */ 左链�?       
  int nLeft,                      /* IN: Elements in array *paLeft */  做链表的元素
  ht_slot **paRight,              /* IN/OUT: Right hand input list */右链表输入列�?
  int *pnRight,                   /* IN/OUT: Elements in *paRight */ �?paRight 里的元素
  ht_slot *aTmp                   /* Temporary buffer */ 临时变量
){
  int iLeft = 0;                  /* Current index in aLeft */左链表索引�?
  int iRight = 0;                 /* Current index in aRight */右链表索引�?
  int iOut = 0;                   /* Current index in output buffer */ 输出
  int nRight = *pnRight;
  ht_slot *aRight = *paRight;   

  assert( nLeft>0 && nRight>0 );如果左右链表数小�? 则终止程�?
  while( iRight<nRight || iLeft<nLeft ){ 对链表进行合�?
    ht_slot logpage;  
    Pgno dbpage;  

    if( (iLeft<nLeft) 
     && (iRight>=nRight || aContent[aLeft[iLeft]]<aContent[aRight[iRight]])
    ){
      logpage = aLeft[iLeft++]; 将aLeft赋给logPage
    }else{
      logpage = aRight[iRight++];
    }
    dbpage = aContent[logpage];     dbpage赋�?

    aTmp[iOut++] = logpage;    为临时变量赋�?
    if( iLeft<nLeft && aContent[aLeft[iLeft]]==dbpage ) iLeft++;

    assert( iLeft>=nLeft || aContent[aLeft[iLeft]]>dbpage ); 终止程序
    assert( iRight>=nRight || aContent[aRight[iRight]]>dbpage );终止程序
  }

  *paRight = aLeft; 
  *pnRight = iOut;
  memcpy(aLeft, aTmp, sizeof(aTmp[0])*iOut);调用拷贝函数
}

/*
** Sort the elements in list aList using aContent[] as the sort key.
** Remove elements with duplicate keys, preferring to keep the
** larger aList[] values.
**
** The aList[] entries are indices into aContent[].  The values in
** aList[] are to be sorted so that for all J<K:
**
**      aContent[aList[J]] < aContent[aList[K]]
**
** For any X and Y such that
**
**      aContent[aList[X]] == aContent[aList[Y]]
**
** Keep the larger of the two values aList[X] and aList[Y] and discard
** the smaller.
*/
//����WAL�ĺϲ�����
static void walMergesort( wal的归�?
  const u32 *aContent,            /* Pages in wal */ wal的页
  ht_slot *aBuffer,               /* Buffer of at least *pnList items to use */
  ht_slot *aList,                 /* IN/OUT: List to sort */定义一个链�?
 s                   /* IN/OUT: Number of elements in aList[] */数目
){
  struct Sublist {              //�������б��ṹ��
    int nList;                    /* Number of elements in aList */ 链表�?元素的个�?
    ht_slot *aList;               /* Pointer to sub-list content */ 指向子链表的指针
  };

  const int nList = *pnList;      /* Size of input list */ 输入链表的大�?
  int nMerge = 0;                 /* Number of elements in list aMerge */在合并链表的元素个数
  ht_slot *aMerge = 0;            /* List to be merged */ 
  int iList;                      /* Index into input list */ 输入链表的索�?
  int iSub = 0;                   /* Index into aSub array */ asub 数组�?索引
  struct Sublist aSub[13];        /* Array of sub-lists */ 

  memset(aSub, 0, sizeof(aSub)); �?asub分配内存
  assert( nList<=HASHTABLE_NPAGE && nList>0 );终止程序
  assert( HASHTABLE_NPAGE==(1<<(ArraySize(aSub)-1)) );终止程序

  for(iList=0; iList<nList; iList++){ 对链表进行循�?
    nMerge = 1;    
    aMerge = &aList[iList]; 取地址
    for(iSub=0; iList & (1<<iSub); iSub++){
      struct Sublist *p = &aSub[iSub]; 赋�?
      assert( p->aList && p->nList<=(1<<iSub) );
      assert( p->aList==&aList[iList&~((2<<iSub)-1)] );
      walMerge(aContent, p->aList, p->nList, &aMerge, &nMerge, aBuffer);
    }
    aSub[iSub].aList = aMerge; 
    aSub[iSub].nList = nMerge; 元素的个�?
  }

  for(iSub++; iSub<ArraySize(aSub); iSub++){ 
    if( nList & (1<<iSub) ){
      struct Sublist *p = &aSub[iSub];定义个数
      assert( p->nList<=(1<<iSub) );终止程序
      assert( p->aList==&aList[nList&~((2<<iSub)-1)] );终止程序
      walMerge(aContent, p->aList, p->nList, &aMerge, &nMerge, aBuffer);调用函数
    }
  }
  assert( aMerge==aList ); 
  *pnList = nMerge; 为链表值赋�?

#ifdef SQLITE_DEBUG     如果定义 SQLITE_DEBUG 
  {
    int i;
    for(i=1; i<*pnList; i++){
      assert( aContent[aList[i]] > aContent[aList[i-1]] );进行判断
    }
  }
#endif
}

/* 
** Free an iterator allocated by walIteratorInit().
*/
//����WAL�յ�����
static void walIteratorFree(WalIterator *p){
  sqlite3ScratchFree(p);      调用释放指针p
}

/*
** Construct a WalInterator object that can be used to loop over all 
** pages in the WAL in ascending order. The caller must hold the checkpoint
** lock. 构建一个WalInterator 对象 ，它可以可以对整�?wal文件按升序pages，它被调用必须在 checkpoint 锁下
**
** On success, make *pp point to the newly allocated WalInterator object 成功,�?页指向新WalInterator分配对象返回SQLITE_OK
** return SQLITE_OK. Otherwise, return an error code. If this routine 否则，返�?error code�?如果出差，则**p 的值就不确定了
** returns an error, the value of *pp is undefined.
**
** The calling routine should invoke walIteratorFree() to destroy the
** WalIterator object when it has finished with it.调用程序应该调用walIteratorFree()来破�?WalIterator对象当它完成它�?
*/
//��ʼ��WAL������
static int walIteratorInit(Wal *pWal, WalIterator **pp){ 
  WalIterator *p;                 /* Return value */  他的值时返回�?
  int nSegment;                   /* Number of segments to merge */ 有几个段来合�?
  u32 iLast;                      /* Last frame in log */ 日志中的 最后的�?
  int nByte;                      /* Number of bytes to allocate */ 分配几个字节
  int i;                          /* Iterator variable */  迭代变量
  ht_slot *aTmp;                  /* Temp space used by merge-sort */ 分配内存用于合并排序
  int rc = SQLITE_OK;             /* Return Code */ 返回 SQLITE_OK

  /* This routine only runs while holding the checkpoint lock. And
  ** it only runs if there is actually content in the log (mxFrame>0).
  这个例程运行而检查点锁。和只运行如果有实际内容的日�?mxFrame > 0)
  */
  assert( pWal->ckptLock && pWal->hdr.mxFrame>0 ); 若果不在枷锁下，终止程序
  iLast = pWal->hdr.mxFrame;      获取 Wal的�?

  /* Allocate space for the WalIterator object. */ 为WalIterator分配空间
  nSegment = walFramePage(iLast) + 1; 获取几个段的�?
  nByte = sizeof(WalIterator)          计算要分配多少个字节
        + (nSegment-1)*sizeof(struct WalSegment)
        + iLast*sizeof(ht_slot);
  p = (WalIterator *)sqlite3ScratchMalloc(nByte); 分配WalIterator 分配内存
  if( !p ){                     如果分配不成�?
    return SQLITE_NOMEM;      返回 SQLITE_NOMEM
  }
  memset(p, 0, nByte);   将P的清0
  p->nSegment = nSegment;  WalIterator 中的 nSegment 赋�?
 
  /* Allocate temporary space used by the merge-sort routine. This block
  ** of memory will be freed before this function returns.  分配临时合并排序例程使用的空间。这一块的内存将这个函数返回之前被释放�?
  */
  aTmp = (ht_slot *)sqlite3ScratchMalloc(          调用函数分配 内存
      sizeof(ht_slot) * (iLast>HASHTABLE_NPAGE?HASHTABLE_NPAGE:iLast)
  );
  if( !aTmp ){         入果分配不成功，�?
    rc = SQLITE_NOMEM;  返回 SQLlIte_NOMEM
  }

  for(i=0; rc==SQLITE_OK && i<nSegment; i++){   循环语句
    volatile ht_slot *aHash;            定义一个aHash 变量
    u32 iZero;                                 
    volatile u32 *aPgno;

    rc = walHashGet(pWal, i, &aHash, &aPgno, &iZero); 调用walHashGet（）
    if( rc==SQLITE_OK ){               如果调用成功
      int j;                      /* Counter variable */ 变量 
      int nEntry;                 /* Number of entries in this segment */ 在这一段中 有几个项目数
      ht_slot *aIndex;            /* Sorted index for this segment */ 对segment 分类指针

      aPgno++;
      if( (i+1)==nSegment ){ 
        nEntry = (int)(iLast - iZero);
      }else{
        nEntry = (int)((u32*)aHash - (u32*)aPgno);
      }
      aIndex = &((ht_slot *)&p->aSegment[p->nSegment])[iZero];
      iZero++;
  
      for(j=0; j<nEntry; j++){
        aIndex[j] = (ht_slot)j;
      }
      walMergesort((u32 *)aPgno, aTmp, aIndex, &nEntry);
      p->aSegment[i].iZero = iZero;
      p->aSegment[i].nEntry = nEntry;
      p->aSegment[i].aIndex = aIndex;
      p->aSegment[i].aPgno = (u32 *)aPgno;
    }
  }
  sqlite3ScratchFree(aTmp);

  if( rc!=SQLITE_OK ){
    walIteratorFree(p);
  }
  *pp = p;
  return rc;
}

/*
** Attempt to obtain the exclusive WAL lock defined by parameters lockIdx and
** n. If the attempt fails and parameter xBusy is not NULL, then it is a
** busy-handler function. Invoke it and retry the lock until either the
** lock is successfully obtained or the busy-handler returns 0.试图获得独家WAL锁lockIdx和定义的参数n。如果尝试失败和参数xBusy不是NULL,那么它就是一�?
** busy-handler功能锁直到调用它并重试成功获得锁或busy-handler返回0�?
*/
static int walBusyLock(        试图获取Wal的锁根据LockIdex �?    //����WALæ��
  Wal *pWal,                      /* WAL connection */
  int (*xBusy)(void*),            /* Function to call when busy */
  void *pBusyArg,                 /* Context argument for xBusyHandler */
  int lockIdx,                    /* Offset of first byte to lock */
  int n                           /* Number of bytes to lock */
){
  int rc;          定义返回�?
  do {
    rc = walLockExclusive(pWal, lockIdx, n); 调用函数 进行加锁
  }while( xBusy && rc==SQLITE_BUSY && xBusy(pBusyArg) ); 
  return rc;
}

/*
** The cache of the wal-index header must be valid to call this function.
** Return the page-size in bytes used by the database.          wal-index头必须是有效的缓存调用这个函�?返回页面大小字节所使用的数据库中�?
*/
static int walPagesize(Wal *pWal){     //����WALҳ��С
  return (pWal->hdr.szPage&0xfe00) + ((pWal->hdr.szPage&0x0001)<<16); 
}

/*
** Copy as much content as we can from the WAL back into the database file
** in response to an sqlite3_wal_checkpoint() request or the equivalent.将尽可能多的内容是我们可以从WAL回数据库文件在回应一个sqlite3_wal_checkpoint()请求或等�?
**
** The amount of information copies from WAL to database might be limited
** by active readers.  This routine will never overwrite a database page
** that a concurrent reader might be using.从WAL数据库副本的信息量可能是有限的由活跃的读者。这个例程将永远不会覆盖数据库页�?并发读者可能会使用
**
** All I/O barrier operations (a.k.a fsyncs) occur in this routine when�?SQLite是在同步WAL-mode =正常所有I / O屏障操作(a.k.。fsync)发生在这个例�?
** SQLite is in WAL-mode in synchronous=NORMAL.  That means that if  
** checkpoints are always run by a background thread or background 
** process, foreground threads will never block on a lengthy fsync call.这意味着,如果检查点总是由一个后台线程或后台　　* *进程�?前台线程不会阻塞在冗长的fsync调用�?
**
** Fsync is called on the WAL before writing content out of the WAL and
** into the database.  This ensures that if the new content is persistent
** in the WAL and can be recovered following a power-loss or hard reset.调用Fsync函数，在将内容写入到数据库中。这将确保如果新内容是持久的wal,在断电或重启后可以恢�?
**
** Fsync is also called on the database file if (and only if) the entire
** WAL content is copied into the database file.  This second fsync makes
** it safe to delete the WAL since the new content will persist in the
** database file.Fsync被调用在数据库文件如果且仅当整个WAL内容复制到数据库文件。这第二个fsync使安全删除WAL自从新内容将持续下去数据库文件�?
**
** This routine uses and updates the nBackfill field of the wal-index header.
** This is the only routine tha will increase the value of nBackfill.  这个程序使用和更新nBackfill wal-index头数据。这是唯一的例程将增加nBackfill的�?
** (A WAL reset or recovery will revert nBackfill to zero, but not increase
** its value.)
**
** The caller must be holding sufficient locks to ensure that no other
** checkpoint is running (in any other thread or process) at the same
** time. 必须调用锁来确保在同一时间内没有其他的 checkpoint运行�?
*/
//����WAL����
static int walCheckpoint(
  Wal *pWal,               .       /* Wal connection */ 定义 Wal
  int eMode,                      /* One of PASSIVE, FULL or RESTART */ 定义 变量
  int (*xBusyCall)(void*),        /* Function to call when busy */ 调用函数
  void *pBusyArg,                 /* Context argument for xBusyHandler */xBusyHandler的参�?
  int sync_flags,                 /* Flags for OsSync() (or 0) */ 同步的标�?
  u8 *zBuf                        /* Temporary buffer to use */ 临时的缓冲区
){
  int rc;                         /* Return code */  返回�?
  int szPage;                     /* Database page-size */ 数据库页的大�?
  WalIterator *pIter = 0;         /* Wal iterator context */ 定义一�?迭代指针
  u32 iDbpage = 0;                /* Next database page to write */ 下一个要写的数据库页 
  u32 iFrame = 0;                 /* Wal frame containing data for iDbpage */
  u32 mxSafeFrame;                /* Max frame that can be backfilled */ 最大的Frame  可以回填
  u32 mxPage;                     /* Max database page to write */ 最大的数据库页
  int i;                          /* Loop counter */   循环变量
  volatile WalCkptInfo *pInfo;    /* The checkpoint status information */检查的信息
  int (*xBusy)(void*) = 0;        /* Function to call when waiting for locks */

  szPage = walPagesize(pWal); 调用函数 获取 数据页的大小
  testcase( szPage<=32768 );      测试
  testcase( szPage>=65536 );      测试
  pInfo = walCkptInfo(pWal); 调用函数获取检验的信息
  if( pInfo->nBackfill>=pWal->hdr.mxFrame ) return SQLITE_OK; 如果 则返�?SQLITE_OK

  /* Allocate the iterator */   配置 迭代
  rc = walIteratorInit(pWal, &pIter); 进行 wal的初始化
  if( rc!=SQLITE_OK ){      如果调用不成�?
    return rc;              返回 rc
  }
  assert( pIter );     如果 pIter 没初始化 则终止程�?

  if( eMode!=SQLITE_CHECKPOINT_PASSIVE ) xBusy = xBusyCall; 如果 emode 不是被调用的模式

  /* Compute in mxSafeFrame the index of the last frame of the WAL that is
  ** safe to write into the database.  Frames beyond mxSafeFrame might
  ** overwrite database pages that are in use by active readers and thus
  ** cannot be backfilled from the WAL.　计算在mxSafeFrame指数的最后一帧在身安全写入数据库。框架之外mxSafeFrame可能覆盖数据库页面所使用的活跃的读�?因此无法回填在wal
  */
  mxSafeFrame = pWal->hdr.mxFrame; 获取 mxSafeFrame的�?
  mxPage = pWal->hdr.nPage;         获取mxpage de �?
  for(i=1; i<WAL_NREADER; i++){     
    u32 y = pInfo->aReadMark[i]; 定义 变量 
    if( mxSafeFrame>y ){      
      assert( y<=pWal->hdr.mxFrame );
      rc = walBusyLock(pWal, xBusy, pBusyArg, WAL_READ_LOCK(i), 1); 返回�?
      if( rc==SQLITE_OK ){      如果 WalBusyLock 函数调用成功
        pInfo->aReadMark[i] = (i==1 ? mxSafeFrame : READMARK_NOT_USED); 通过判断i是否等于1 来为其赋�?
        walUnlockExclusive(pWal, WAL_READ_LOCK(i), 1); 调用解锁函数
      }else if( rc==SQLITE_BUSY ){ 如果 rc 是Sqllite——busy
        mxSafeFrame = y;        令y值赋�?mxSafeFrame
        xBusy = 0;               将xBusy�?
      }else{
        goto walcheckpoint_out; 跳转�?Walcheckpoint_out
      }
    }
  }

  if( pInfo->nBackfill<mxSafeFrame
   && (rc = walBusyLock(pWal, xBusy, pBusyArg, WAL_READ_LOCK(0), 1))==SQLITE_OK      判断语句
  ){
    i64 nSize;                    /* Current size of database file */ 当前数据库大�?
    u32 nBackfill = pInfo->nBackfill;       

    /* Sync the WAL to disk */ 将Wal同步�?磁盘�?
    if( sync_flags ){   是否同步
      rc = sqlite3OsSync(pWal->pWalFd, sync_flags); 调用同步函数
    }

    /* If the database file may grow as a result of this checkpoint, hint
    ** about the eventual size of the db file to the VFS layer. 如果数据库文件可能会由于这个检查点,暗示关于db文件的最终大小VFS层�?
    */
    if( rc==SQLITE_OK ){  如果调用成功
      i64 nReq = ((i64)mxPage * szPage);  定义64为的变量
      rc = sqlite3OsFileSize(pWal->pDbFd, &nSize); 调用系统函数 确定文件大小
      if( rc==SQLITE_OK && nSize<nReq ){     如果调用成功 �?数据文件 小于 最大的阀�?
        sqlite3OsFileControlHint(pWal->pDbFd, SQLITE_FCNTL_SIZE_HINT, &nReq);  调用函数
      }
    }

    /* Iterate through the contents of the WAL, copying data to the db file. */  将Wal的内容复制到数据文件�?
    while( rc==SQLITE_OK && 0==walIteratorNext(pIter, &iDbpage, &iFrame) ){ 
      i64 iOffset;        定义 64 的变�?
      assert( walFramePgno(pWal, iFrame)==iDbpage ); 如果调用函数的返回值不等于 IDbpage，则终止程序
      if( iFrame<=nBackfill || iFrame>mxSafeFrame || iDbpage>mxPage ) continue; 如果不满足程�?，则跳过此次循环
      iOffset = walFrameOffset(iFrame, szPage) + WAL_FRAME_HDRSIZE; 为IoffSET赋�?
      /* testcase( IS_BIG_INT(iOffset) ); // requires a 4GiB WAL file */
      rc = sqlite3OsRead(pWal->pWalFd, zBuf, szPage, iOffset);  调用系统读函�?
      if( rc!=SQLITE_OK ) break; 如果调用不成�?，终止循�?
      iOffset = (iDbpage-1)*(i64)szPage; 求取�?
      testcase( IS_BIG_INT(iOffset) ); 测试函数
      rc = sqlite3OsWrite(pWal->pDbFd, zBuf, szPage, iOffset);   调用写函�?
      if( rc!=SQLITE_OK ) break;如果调用不成功，则跳出循�?
    }

    /* If work was actually accomplished... */若果完成
    if( rc==SQLITE_OK ){  如果rc 等于SQLite_ok
      if( mxSafeFrame==walIndexHdr(pWal)->mxFrame ){ 
        i64 szDb = pWal->hdr.nPage*(i64)szPage; 定义64为的变量 数据库大�?
        testcase( IS_BIG_INT(szDb) );        测试函数
        rc = sqlite3OsTruncate(pWal->pDbFd, szDb); 调用 函数
        if( rc==SQLITE_OK && sync_flags ){      
          rc = sqlite3OsSync(pWal->pDbFd, sync_flags); 调用同步函数
        }
      }
      if( rc==SQLITE_OK ){        如果调用成功  
        pInfo->nBackfill = mxSafeFrame;
      }
    }

    /* Release the reader lock held while backfilling */
    walUnlockExclusive(pWal, WAL_READ_LOCK(0), 1); 释放�?
  }

  if( rc==SQLITE_BUSY ){      如果 
    /* Reset the return code so as not to report a checkpoint failure
    ** just because there are active readers.  */
    rc = SQLITE_OK;     rc 赋�?SQLITE_OK
  }

  /* If this is an SQLITE_CHECKPOINT_RESTART operation, and the entire wal
  ** file has been copied into the database file, then block until all
  ** readers have finished using the wal file. This ensures that the next
  ** process to write to the database restarts the wal file.如果这是一个SQLITE_CHECKPOINT_RESTART操作,整个在身文件已复制到数据库文�?然后阻止,直到所有读者使用wal文件已经完成。这将确保未来过程编写数据库重启wal文件�?
  */
  if( rc==SQLITE_OK && eMode!=SQLITE_CHECKPOINT_PASSIVE ){ 如果rc 不是 SQLITE_ok ,不在SQLITE_CHECKPOINT_PASSIVE模式�?
    assert( pWal->writeLock ); 终止程序
    if( pInfo->nBackfill<pWal->hdr.mxFrame ){
      rc = SQLITE_BUSY; 
    }else if( eMode==SQLITE_CHECKPOINT_RESTART ){
      assert( mxSafeFrame==pWal->hdr.mxFrame );
      rc = walBusyLock(pWal, xBusy, pBusyArg, WAL_READ_LOCK(1), WAL_NREADER-1);
      if( rc==SQLITE_OK ){
        walUnlockExclusive(pWal, WAL_READ_LOCK(1), WAL_NREADER-1); 调用解锁函数
      }
    }
  }

 walcheckpoint_out: goto 标志
  walIteratorFree(pIter); 释放指针
  return rc;
}

/*
** If the WAL file is currently larger than nMax bytes in size, truncate
** it to exactly nMax bytes. If an error occurs while doing so, ignore it. 如果Wal文件大于最大大小，缩短它到正确的长度�?
*/
//WAL���ƴ�С
static void walLimitSize(Wal *pWal, i64 nMax){
  i64 sz;     定义64为的变量 
  int rx; 
  sqlite3BeginBenignMalloc();调用函数
  rx = sqlite3OsFileSize(pWal->pWalFd, &sz); 调用系统函数得到Wal的大�?
  if( rx==SQLITE_OK && (sz > nMax ) ){    如果调用函数成功，如果文件大小超过范�?
    rx = sqlite3OsTruncate(pWal->pWalFd, nMax); 调用函数，将文件大小缩短
  }
  sqlite3EndBenignMalloc(); 结束内存管理
  if( rx ){       如果rx为真   
    sqlite3_log(rx, "cannot limit WAL size: %s", pWal->zWalName); 将日志信息写入到日志中，如果日志已经被激活�?
  }
}

/*
** Close a connection to a log file. 关闭日志文件链接         �?
*/
//�ر�WAL
int sqlite3WalClose(
  Wal *pWal,                      /* Wal to close */ 定义Wal 结构指针
  int sync_flags,                 /* Flags to pass to OsSync() (or 0) */  同步的标�?
  int nBuf,
  u8 *zBuf                        /* Buffer of at least nBuf bytes */ 至少有多大字节的缓冲�?
){
  int rc = SQLITE_OK;             
  if( pWal ){                         如果wal不为�?
    int isDelete = 0;             /* True to unlink wal and wal-index files */ 解开Wal和Wal-inde的链接则为真

    /* If an EXCLUSIVE lock can be obtained on the database file (using the
    ** ordinary, rollback-mode locking methods, this guarantees that the
    ** connection associated with this log file is the only connection to
    ** the database. In this case checkpoint the database and unlink both
    ** the wal and wal-index files. 如果数据库文件可以获取一个EXCLUSIVE 锁，使用普通的rollback-mode锁定方法,这保证了连接相关的日志文件是唯一的连接数据库
    **这样可以进行检查数据库和解开Wal和Wal-index
    ** The EXCLUSIVE lock is not released before returning. 直到结束才释放该�?
    */
    rc = sqlite3OsLock(pWal->pDbFd, SQLITE_LOCK_EXCLUSIVE); 调用函数进行加锁
    if( rc==SQLITE_OK ){            如果调用函数成功
      if( pWal->exclusiveMode==WAL_NORMAL_MODE   { 如果Wal没加�?
        pWal->exclusiveMode = WAL_EXCLUSIVE_MODE; 进行加锁
      }
      rc = sqlite3WalCheckpoint(                         
          pWal, SQLITE_CHECKPOINT_PASSIVE, 0, 0, sync_flags, nBuf, zBuf, 0, 0
      ); 进行检查点
      if( rc==SQLITE_OK ){ 如果调用函数成功
        int bPersist = -1; 
        sqlite3OsFileControlHint(      调用系统函数
            pWal->pDbFd, SQLITE_FCNTL_PERSIST_WAL, &bPersist
        );
        if( bPersist!=1 ){   
          /* Try to delete the WAL file if the checkpoint completed and
          ** fsyned (rc==SQLITE_OK) and if we are not in persistent-wal
          ** mode (!bPersist) */ 尝试删除WAL文件如果检查点和完成fsyned(rc = = SQLITE_OK),如果我们不persistent-wal模式
          isDelete = 1;
        }else if( pWal->mxWalSize>=0 ){ 
          /* Try to truncate the WAL file to zero bytes if the checkpoint
          ** completed and fsynced (rc==SQLITE_OK) and we are in persistent
          ** WAL mode (bPersist) and if the PRAGMA journal_size_limit is a
          ** non-negative value (pWal->mxWalSize>=0).  Note that we truncate
          ** to zero bytes as truncating to the journal_size_limit might
          ** leave a corrupt WAL file on disk. */试图截断WAL文件零字节如果检查点完成并fsync(rc = = SQLITE_OK),我们在持续在身模�?bPersist)如果编译指示journal_size_limit是一个非负价�?pWal - > mxWalSize > = 0)。注�?我们截断为零字节journal_size_limit可能删除离开腐败WAL磁盘上的文件�? /
          walLimitSize(pWal, 0);
        }
      }
    }

    walIndexClose(pWal, isDelete);调用关闭索�?
    sqlite3OsClose(pWal->pWalFd); 关闭日志文件链接
    if( isDelete ){如果调用函数成功
      sqlite3BeginBenignMalloc();调用管理内存
      sqlite3OsDelete(pWal->pVfs, pWal->zWalName, 0); 清空内存
      sqlite3EndBenignMalloc(); 关闭内存管理
    }
    WALTRACE(("WAL%p: closed\n", pWal));关闭日志
    sqlite3_free((void *)pWal->apWiData);释放指针
    sqlite3_free(pWal);释放指针
  }
  return rc;
}

/*
** Try to read the wal-index header.  Return 0 on success and 1 if
** there is a problem. 读取Wal-index头数据，如果成功则返�?，出错则返回1
**
** The wal-index is in shared memory.  Another thread or process might
** be writing the header at the same time this procedure is trying to
** read it, which might result in inconsistency.  A dirty read is detected
** by verifying that both copies of the header are the same and also by
** a checksum on the header. wal-index在共享内存。另一个线程或进程可能写的头数据同时这个程序正在读�?这可能会导致不一致。检测到脏读通过验证这两个副本的读数据都是一样的,也头一个校验和
**
** If and only if the read is consistent and the header is different from
** pWal->hdr, then pWal->hdr is updated to the content of the new header
** and *pChanged is set to 1. 当且仅当读数据是一样的，头数据和Pwal->hdr不同的，进行新头数据的内容进行更新，*pChanged设值为1
**
** If the checksum cannot be verified return non-zero. If the header 如果检查不能被证实，则返回非空，如果读取数据成功和被证实，返回0
** is read successfully and the checksum verified, return zero.
*/
static int walIndexTryHdr(Wal *pWal, int *pChanged){
  u32 aCksum[2];                  /* Checksum on the header content * 在头数据的内容进行校�?
  WalIndexHdr h1, h2;             /* Two copies of the header content */ 定义两个 WalIndexHdr 变量
  WalIndexHdr volatile *aHdr;     /* Header in shared memory */ 

  /* The first page of the wal-index must be mapped at this point. */  这个指针映射到wal-index 的第一�?
  assert( pWal->nWiData>0 && pWal->apWiData[0] );

  /* Read the header. This might happen concurrently with a write to the 
  ** same area of shared memory on a different CPU in a SMP,
  ** meaning it is possible that an inconsistent snapshot is read
  ** from the file. If this happens, return non-zero. 读取数据头。可能发生并行写入内存同一片区域。这可能导致不一致。如果发生这种情况，返回非空值�?
  **
  ** There are two copies of the header at the beginning of the wal-index. 提前赋值两个备份，读的顺序是read�?】，[1],写的顺序�?�?
  ** When reading, read [0] first then [1].  Writes are in the reverse order.内存障碍是用来防止编译器或硬件重新排序的读和�?
  ** Memory barriers are used to prevent the compiler or the hardware from
  ** reordering the reads and writes.
  */
  aHdr = walIndexHdr(pWal); 
  memcpy(&h1, (void *)&aHdr[0], sizeof(h1)); 将内容复制到aHdr[0]
  walShmBarrier(pWal);
  memcpy(&h2, (void *)&aHdr[1], sizeof(h2));将内容复制到aHdr[1]

  if( memcmp(&h1, &h2, sizeof(h1))!=0 ){ �?�?进行比较，如果不相同
    return 1;   /* Dirty read */  返回1 是脏数据
  }  
  if( h1.isInit==0 ){ 如果初始化不成功
    return 1;   /* Malformed header - probably all zeros */
  }
  walChecksumBytes(1, (u8*)&h1, sizeof(h1)-sizeof(h1.aCksum), 0, aCksum);
  if( aCksum[0]!=h1.aCksum[0] || aCksum[1]!=h1.aCksum[1] ){ 如果不匹�?
    return 1;   /* Checksum does not match */ 返回1
  }

  if( memcmp(&pWal->hdr, &h1, sizeof(WalIndexHdr)) ){
    *pChanged = 1; 更改 
    memcpy(&pWal->hdr, &h1, sizeof(WalIndexHdr));
    pWal->szPage = (pWal->hdr.szPage&0xfe00) + ((pWal->hdr.szPage&0x0001)<<16); 计算Wal的页的大�?
    testcase( pWal->szPage<=32768 ); 测试函数
    testcase( pWal->szPage>=65536 );
  }

  /* The header was successfully read. Return zero. */ 如果头数据读取成�?
  return 0; 返回0
}

/*
** Read the wal-index header from the wal-index and into pWal->hdr.
** If the wal-header appears to be corrupt, try to reconstruct the
** wal-index from the WAL before returning.解读Wal-index，如果Wal-header出现错误，在返回前重构它
**
** Set *pChanged to 1 if the wal-index header value in pWal->hdr is
** changed by this opertion.  If pWal->hdr is unchanged, set *pChanged
** to 0. 设置*pchange�? ，如果在运行过中pWal->hdr 没改变，则设�?�?
**
** If the wal-index header is successfully read, return SQLITE_OK. 
** Otherwise an SQLite error code. 读出成功，返回ok，否则返回error code
*/
//��WAL����
static int walIndexReadHdr(Wal *pWal, int *pChanged){
  int rc;                         /* Return code */ 返回 �?
  int badHdr;                     /* True if a header read failed */ 读出失败，值为�?
  volatile u32 *page0;            /* Chunk of wal-index containing header */包含 header的wal-index�?

  /* Ensure that page 0 of the wal-index (the page that contains the 
  ** wal-index header) is mapped. Return early if an error occurs here.
  */
  assert( pChanged ); 如果pChange �? 则程序终�?
  rc = walIndexPage(pWal, 0, &page0); 调用函数获取�?
  if( rc!=SQLITE_OK ){ 如果调用不成功，返回
    return rc;
  };
  assert( page0 || pWal->writeLock==0 );  如果page没成功获取�?，则终止程序

  /* If the first page of the wal-index has been mapped, try to read the
  ** wal-index header immediately, without holding any lock. This usually
  ** works, but may fail if the wal-index header is corrupt or currently 
  ** being modified by another thread or process.　如果第一页wal-index映射,试着立即�?wal-index头数�?没有持有任何锁。这通常工作,但是可能会失败如果wal-index头目前腐败或被另一个线程或进程修改
  */
  badHdr = (page0 ? walIndexTryHdr(pWal, pChanged) : 1); 如果建立连接，则获取walIndexTryHDr(pWal,pChange),否则�?

  /* If the first attempt failed, it might have been due to a race
  ** with a writer.  So get a WRITE lock and try again.如果第一次尝试失败了,这可能是由于写操作。所以得到一个写�?再试一�?
  */
  assert( badHdr==0 || pWal->writeLock==0 );
  if( badHdr ){  如果读取失败
    if( pWal->readOnly & WAL_SHM_RDONLY ){
      if( SQLITE_OK==(rc = walLockShared(pWal, WAL_WRITE_LOCK)) ){  如果获取的是共享�?
        walUnlockShared(pWal, WAL_WRITE_LOCK);  释放共享�?
        rc = SQLITE_READONLY_RECOVERY;
      }
    }else if( SQLITE_OK==(rc = walLockExclusive(pWal, WAL_WRITE_LOCK, 1)) ){ 如果获取的是排它�?
      pWal->writeLock = 1; 将Wal的writeLock赋值为1
      if( SQLITE_OK==(rc = walIndexPage(pWal, 0, &page0)) ){ 获取索引页成�?
        badHdr = walIndexTryHdr(pWal, pChanged); 获取索引头数�?
        if( badHdr ){
          /* If the wal-index header is still malformed even while holding
          ** a WRITE lock, it can only mean that the header is corrupted and
          ** needs to be reconstructed.  So run recovery to do exactly that.
          */　如果wal-index头仍然是脏数据即使在加锁之后,它只能意味着header损坏，它需要重建。所以恢复运�?
          rc = walIndexRecover(pWal);重建Wal
          *pChanged = 1; 赋�?
        }
      }
      pWal->writeLock = 0; 设置苏醒�?
      walUnlockExclusive(pWal, WAL_WRITE_LOCK, 1); 释放写锁
    }
  }

  /* If the header is read successfully, check the version number to make
  ** sure the wal-index was not constructed with some future format that
  ** this version of SQLite cannot understand.如果成功读头数据,检查版本确保Wal的结构体不被这个版本所识别�?
  */
  if( badHdr==0 && pWal->hdr.iVersion!=WALINDEX_MAX_VERSION ){
    rc = SQLITE_CANTOPEN_BKPT;
  }

  return rc;返回�?
}

/*
** This is the value that walTryBeginRead returns when it needs to
** be retried. 
*/
////*这是�?walTryBeginRead需要重试时返回的值�?
#define WAL_RETRY  (-1)

/*
** Attempt to start a read transaction.  This might fail due to a race or
** other transient condition.  When that happens, it returns WAL_RETRY to
** indicate to the caller that it is safe to retry immediately.
**尝试开始一个读事务，这可能由于竞争或其他突发情况。如果失�?就返�?Wal_RETRY 提示 立即重试�?
** On success return SQLITE_OK.  On a permanent failure (such an
** I/O error or an SQLITE_BUSY because another process is running
** recovery) return a positive error code.
** 在永久性故障（如I/O错误或sqlite_busy，因为另一个进程正在运行恢复）返回一个正的错误代码�?
** The useWal parameter is true to force the use of the WAL and disable
** the case where the WAL is bypassed because it has been completely
** checkpointed.  If useWal==0 then this routine calls walIndexReadHdr() 
** to make a copy of the wal-index header into pWal->hdr. 
////* 为了强制使用WAL并且使忽略WAL这种情况不发生，useWal参数被设置为真，因为他已经被完全检查过�?
////  如果 useWal==0，为了使wal-index的头部复制给pWal->hdr，就调用walIndexReadHdr() 。（为了使pWal->hdr=wal-index的头部）
*/
** If the wal-index header has changed, *pChanged is set to 1 (as an indication 
** to the caller that the local paget cache is obsolete and needs to be 
** flushed.)  When useWal==1, the wal-index header is assumed to already
** be loaded and the pChanged parameter is unused.
////* 如果wal-index的头部已经改变，则设�?pChanged=1（用来指示调用者当前缓冲区已经陈旧需要冲掉）�?
///// 当useWal==1，wal-index的头部已经被加载，并且pChanged参数没被使用�?
*/
**useWal参数是真的强迫使用WAL和禁用的WAL忽略,因为它已经完全设置检查点�?
如果useWal = = 0,这个例程调用walIndexReadHdr()复制wal-index头到pWal - > hdr�?
如果wal-index头已经改�?pChanged设置�?(就像一个想法给调用�?当地的佩吉特缓存过时的和需要刷�?�?
useWal = = 1�?wal-index头已经假定被加载和pChanged参数是未使用的�?
** The caller must set the cnt parameter to the number of prior calls to
** this routine during the current read attempt that returned WAL_RETRY.
** This routine will start taking more aggressive measures to clear the
** race conditions after multiple WAL_RETRY returns, and after an excessive
** number of errors will ultimately return SQLITE_PROTOCOL.  The
** SQLITE_PROTOCOL return indicates that some other process has gone rogue
** and is not honoring the locking protocol.  There is a vanishingly small
** chance that SQLITE_PROTOCOL could be returned because of a run of really
** bad luck when there is lots of contention for the wal-index, but that
** possibility is so small that it can be safely neglected, we believe.
**
////*当当前的读尝试返回WAL_RETRY使，调用者为该程序将参数cnt设为之前调用的数量�?
//// 
** On success, this routine obtains a read lock on 
** WAL_READ_LOCK(pWal->readLock).  The pWal->readLock integer is
** in the range 0 <= pWal->readLock < WAL_NREADER.  If pWal->readLock==(-1)
** that means the Wal does not hold any read lock.  The reader must not
** access any database page that is modified by a WAL frame up to and
** including frame number aReadMark[pWal->readLock].  The reader will
** use WAL frames up to and including pWal->hdr.mxFrame if pWal->readLock>0
** Or if pWal->readLock==0, then the reader will ignore the WAL
** completely and get all content directly from the database file.
** If the useWal parameter is 1 then the WAL will never be ignored and
** this routine will always set pWal->readLock>0 on success.
** When the read transaction is completed, the caller must release the
** lock on WAL_READ_LOCK(pWal->readLock) and set pWal->readLock to -1.
**
////*如果成功，这个程序从 WAL_READ_LOCK(pWal->readLock)获得一个读锁�?
////  pWal->readLock的取值范围是�? <= pWal->readLock < WAL_NREADER�?
//// 如果 pWal->readLock==(-1)，说明Wal没有任何读锁�?
//// 访问者一定不能访问任何被WAL架构修改的数据库页并且�?
//// 如果pWal->readLock>0或pWal->readLock==0，访问者将使用WAL架构并且包含pWal->hdr.mxFrame，否则访问者将完全忽视
//// WAL并且直接从数据库文件获取内容�?
//// 如果useWal==1，WAL将不会被忽视，并程序总是使pWal->readLock>0�?
//// 当读事务完成时，调用者必须释放在 WAL_READ_LOCK(pWal->readLock)上的锁，并且设置pWal->readLock=-1.
*/
** This routine uses the nBackfill and aReadMark[] fields of the header
** to select a particular WAL_READ_LOCK() that strives to let the
** checkpoint process do as much work as possible.  This routine might
** update values of the aReadMark[] array in the header, but if it does
** so it takes care to hold an exclusive lock on the corresponding
** WAL_READ_LOCK() while changing values.
*/
////* 这个程序使用头的nBackfill �?aReadMark[]领域来选择一个特殊的WAL_READ_LOCK()，以至于使检查程序做尽量多的工作�?
//// 这个程序可能更新在头部的aReadMark[]数组的值，但是如果它真的要改变值，则它必须在相关的WAL_READ_LOCK()上持有排斥锁以便改变值�?
*/
//WAL��һ�γ��Զ�
static int walTryBeginRead(Wal *pWal, int *pChanged, int useWal, int cnt){
  volatile WalCkptInfo *pInfo;    /* Checkpoint information in wal-index */ 检查�?
  u32 mxReadMark;                 /* Largest aReadMark[] value */ aReadMark[] 的最大�?
  int mxI;                        /* Index of largest aReadMark[] value */aReadMark最大的索引�?
  int i;                          /* Loop counter */ 循环计数
  int rc = SQLITE_OK;             /* Return code  */ 返回�?

  assert( pWal->readLock<0 );     /* Not currently locked */没有加锁

  /* Take steps to avoid spinning forever if there is a protocol error. 采取措施来避免死循环
  **
  ** Circumstances that cause a RETRY should only last for the briefest情况下导致重试应该只简短的最后的实例
  ** instances of time.  No I/O or other system calls are done while the
  ** locks are held, so the locks should not be held for very long. But 
  ** if we are unlucky, another process that is holding a lock might get
  ** paged out or take a page-fault that is time-consuming to resolve, 
  ** during the few nanoseconds that it is holding the lock.  In that case,
  ** it might take longer than normal for the lock to free.任何I / O和其他系统调用而完成的�?锁不应该了很长时间。但如果我们倒霉,另一个进�?持有一个锁调出或页面错�?耗费时间来解�?在几纳秒持有的锁。在这种情况�? 它可能需要更长的时间比正常锁自由�?
  **
  ** After 5 RETRYs, we begin calling sqlite3OsSleep().  The first few
  ** calls to sqlite3OsSleep() have a delay of 1 microsecond.  Really this
  ** is more of a scheduler yield than an actual delay.  But on the 10th
  ** an subsequent retries, the delays start becoming longer and longer, 
  ** so that on the 100th (and last) RETRY we delay for 21 milliseconds.
  ** The total delay time before giving up is less than 1 second. 5次重试后,我们开始调用sqlite3OsSleep()。最初的几调用sqlite3OsSleep()有一个延�?微秒。真的这比一个实际的调度程序产生的延迟。但在第十一个后续重�?延迟开始变得越来越�? �?00(最�?重试我们延迟�?1毫秒。放弃前的总延迟时间小�?秒�?
  */
  if( cnt>5 ){
    int nDelay = 1;                      /* Pause time in microseconds */暂停时间以微秒为单位
    if( cnt>100 ){                       
      VVA_ONLY( pWal->lockError = 1; )
      return SQLITE_PROTOCOL;
    }
    if( cnt>=10 ) nDelay = (cnt-9)*238;  /* Max delay 21ms. Total delay 996ms */
    sqlite3OsSleep(pWal->pVfs, nDelay);
  }

  if( !useWal ){
    rc = walIndexReadHdr(pWal, pChanged);
    if( rc==SQLITE_BUSY ){
      /* If there is not a recovery running in another thread or process
      ** then convert BUSY errors to WAL_RETRY.  If recovery is known to
      ** be running, convert BUSY to BUSY_RECOVERY.  There is a race here
      ** which might cause WAL_RETRY to be returned even if BUSY_RECOVERY
      ** would be technically correct.  But the race is benign since with
      ** WAL_RETRY this routine will be called again and will probably be
      ** right on the second iteration.
      */
      if( pWal->apWiData[0]==0 ){
        /* This branch is taken when the xShmMap() method returns SQLITE_BUSY.
        ** We assume this is a transient condition, so return WAL_RETRY. The
        ** xShmMap() implementation used by the default unix and win32 VFS 
        ** modules may return SQLITE_BUSY due to a race condition in the 
        ** code that determines whether or not the shared-memory region 
        ** must be zeroed before the requested page is returned.
        */
        rc = WAL_RETRY;
      }else if( SQLITE_OK==(rc = walLockShared(pWal, WAL_RECOVER_LOCK)) ){
        walUnlockShared(pWal, WAL_RECOVER_LOCK);
        rc = WAL_RETRY;
      }else if( rc==SQLITE_BUSY ){
        rc = SQLITE_BUSY_RECOVERY;
      }
    }
    if( rc!=SQLITE_OK ){
      return rc;
    }
  }

  pInfo = walCkptInfo(pWal);
  if( !useWal && pInfo->nBackfill==pWal->hdr.mxFrame ){
    /* The WAL has been completely backfilled (or it is empty).
    ** and can be safely ignored.
    */
    rc = walLockShared(pWal, WAL_READ_LOCK(0));
    walShmBarrier(pWal);
    if( rc==SQLITE_OK ){
      if( memcmp((void *)walIndexHdr(pWal), &pWal->hdr, sizeof(WalIndexHdr)) ){
        /* It is not safe to allow the reader to continue here if frames
        ** may have been appended to the log before READ_LOCK(0) was obtained.
        ** When holding READ_LOCK(0), the reader ignores the entire log file,
        ** which implies that the database file contains a trustworthy
        ** snapshoT. Since holding READ_LOCK(0) prevents a checkpoint from
        ** happening, this is usually correct.
        **
        ** However, if frames have been appended to the log (or if the log 
        ** is wrapped and written for that matter) before the READ_LOCK(0)
        ** is obtained, that is not necessarily true. A checkpointer may
        ** have started to backfill the appended frames but crashed before
        ** it finished. Leaving a corrupt image in the database file.
        */
        walUnlockShared(pWal, WAL_READ_LOCK(0));
        return WAL_RETRY;
      }
      pWal->readLock = 0;
      return SQLITE_OK;
    }else if( rc!=SQLITE_BUSY ){
      return rc;
    }
  }

  /* If we get this far, it means that the reader will want to use
  ** the WAL to get at content from recent commits.  The job now is
  ** to select one of the aReadMark[] entries that is closest to
  ** but not exceeding pWal->hdr.mxFrame and lock that entry.
  */
  mxReadMark = 0;
  mxI = 0;
  for(i=1; i<WAL_NREADER; i++){
    u32 thisMark = pInfo->aReadMark[i];
    if( mxReadMark<=thisMark && thisMark<=pWal->hdr.mxFrame ){
      assert( thisMark!=READMARK_NOT_USED );
      mxReadMark = thisMark;
      mxI = i;
    }
  }
  /* There was once an "if" here. The extra "{" is to preserve indentation. */
  {
    if( (pWal->readOnly & WAL_SHM_RDONLY)==0
     && (mxReadMark<pWal->hdr.mxFrame || mxI==0)
    ){
      for(i=1; i<WAL_NREADER; i++){
        rc = walLockExclusive(pWal, WAL_READ_LOCK(i), 1);
        if( rc==SQLITE_OK ){
          mxReadMark = pInfo->aReadMark[i] = pWal->hdr.mxFrame;
          mxI = i;
          walUnlockExclusive(pWal, WAL_READ_LOCK(i), 1);
          break;
        }else if( rc!=SQLITE_BUSY ){
          return rc;
        }
      }
    }
    if( mxI==0 ){
      assert( rc==SQLITE_BUSY || (pWal->readOnly & WAL_SHM_RDONLY)!=0 );
      return rc==SQLITE_BUSY ? WAL_RETRY : SQLITE_READONLY_CANTLOCK;
    }

    rc = walLockShared(pWal, WAL_READ_LOCK(mxI));
    if( rc ){
      return rc==SQLITE_BUSY ? WAL_RETRY : rc;
    }
    /* Now that the read-lock has been obtained, check that neither the
    ** value in the aReadMark[] array or the contents of the wal-index
    ** header have changed.
    **
    ** It is necessary to check that the wal-index header did not change
    ** between the time it was read and when the shared-lock was obtained
    ** on WAL_READ_LOCK(mxI) was obtained to account for the possibility
    ** that the log file may have been wrapped by a writer, or that frames
    ** that occur later in the log than pWal->hdr.mxFrame may have been
    ** copied into the database by a checkpointer. If either of these things
    ** happened, then reading the database with the current value of
    ** pWal->hdr.mxFrame risks reading a corrupted snapshot. So, retry
    ** instead.
    **
    ** This does not guarantee that the copy of the wal-index header is up to
    ** date before proceeding. That would not be possible without somehow
    ** blocking writers. It only guarantees that a dangerous checkpoint or 
    ** log-wrap (either of which would require an exclusive lock on
    ** WAL_READ_LOCK(mxI)) has not occurred since the snapshot was valid.
    */
    walShmBarrier(pWal);
    if( pInfo->aReadMark[mxI]!=mxReadMark
     || memcmp((void *)walIndexHdr(pWal), &pWal->hdr, sizeof(WalIndexHdr))
    ){
      walUnlockShared(pWal, WAL_READ_LOCK(mxI));
      return WAL_RETRY;
    }else{
      assert( mxReadMark<=pWal->hdr.mxFrame );
      pWal->readLock = (i16)mxI;
    }
  }
  return rc;
}

/*
** Begin a read transaction on the database. 在数据库开始一个读事务
**
** This routine used to be called sqlite3OpenSnapshot() and with good reason:
** it takes a snapshot（快照） of the state of the WAL and wal-index for the current
** instant（立即的；紧急的；紧迫的�?in time（及时）.  The current thread will continue to use this snapshot.
** Other threads might append new content to the WAL and wal-index but
** that extra content is ignored by the current thread.这个程序通常被SQLite3OpenSnapshot（）函数调用�?
原因：需要的快照WAL和wal-index当前的即时状态，当前线程继续使用抓拍技术�?
其他的线程可能添加新的内容到Wal和Wal-index，但当前线程不考虑它�?
**
** If the database contents have changes since the previous read
** transaction, then *pChanged is set to 1 before returning.  The
** Pager layer will use this to know that is cache is stale and
** needs to be flushed.　如果数据库内容变化这是由于从以前读事�?然后*pChanged返回之前被设置为1。要刷新�?
*/

/////*在数据库开始一个读事务�?
///// 这个程序过去被叫做sqlite3OpenSnapshot()，并且有好的理由�?
///// 它及时地为当前的紧迫的预写式日志(WAL)和预写式日志索引(WAL-index)的状态做下快照�?
///// 当前的线程会继续使用这个快照�?
///// 其他线程可能会为WAL �?wal-index添加内容，但是当前线程将会忽略额外的内容�?
///// 如果由于之前的读事务使得数据库内容发生了改变，则在返回之前，*pChanged被设置为1.
///// 页管理层使用此知道缓存已失效并且需要被冲掉�?
*/
int sqlite3WalBeginReadTransaction(Wal *pWal, int *pChanged){
  int rc;                         /* Return code */ 返回�?    //������
  int cnt = 0;                    /* Number of TryBeginRead attempts */ 重新TryBeginRead的次�?   // ���¶��Ĵ���

  do{
    rc = walTryBeginRead(pWal, pChanged, 0, ++cnt);  ////wal开始读，成功返回一个SQLITE_OK，失败就返回WAL_RETRY，并立即重试�?
  }while( rc==WAL_RETRY ); ////当读取失败，则什么也不做
  testcase( (rc&0xff)==SQLITE_BUSY );测试函数
  testcase( (rc&0xff)==SQLITE_IOERR );测试函数
  testcase( rc==SQLITE_PROTOCOL );测试函数
  testcase( rc==SQLITE_OK );测试函数
  return rc; 返回 rc
}

/*
** Finish with a read transaction.  All this does is release the
** read-lock. 当完成读事务�? 释放 read-lock
*/
////* 当完成一个事务后，要做的事就是释放读锁�?
*/
void sqlite3WalEndReadTransaction(Wal *pWal){
  sqlite3WalEndWriteTransaction(pWal); 调用结束写事�?
  if( pWal->readLock>=0 ){ 如果存在readLock�?
    walUnlockShared(pWal, WAL_READ_LOCK(pWal->readLock)); 解锁
    pWal->readLock = -1; 赋�?
  }
}

/*
** Read a page from the WAL, if it is present in the WAL and if the 
** current read transaction is configured to use the WAL.  从在身读取一个页�?如果它存在于WAL如果当前读事务配置使用Wal
** 
////*如果一个页在WAL中，并且当前的读事务被设置来使用WAL，则从预写式日志(WAL)中读取一个页�?
*/
** The *pInWal is set to 1 if the requested page is in the WAL and          
** has been loaded.  Or *pInWal is set to 0 if the page was not in 
** the WAL and needs to be read out of the database.*pInWal 赋值为1  当需要的page 在Wal中，且已被加载， 赋值为0 ，如�?不在wal中，需要充数据库中加载
*/
////*如果被访问的页存在于WAL中，并且已经被加载，则使*pInWal=1.
*/
int sqlite3WalRead(
  Wal *pWal,                      /* WAL handle */ 第一指针////WAL的头指针
  Pgno pgno,                      /* Database page number to read data for */ 数据页号////要读取的数据的数据库页号
  int *pInWal,                    /* OUT: True if data is read from WAL */ 数据是充Wal中读取则为真////输出：如果数据是从WAL中读取，�?pInWal为真
  int nOut,                       /* Size of buffer pOut in bytes */ 输出字节流的大小  字节为单�?///输出的字节缓冲区的大�?
  u8 *pOut                        /* Buffer to write page data to */写数据的缓冲�?
){
  u32 iRead = 0;                  /* If !=0, WAL frame to return data from */
  u32 iLast = pWal->hdr.mxFrame;  /* Last page in WAL for this reader */Wal 最新页////如果不为0，则WAL框架为读取者从WAL的最后一页返回数据�?
  int iHash;                      /* Used to loop through N hash tables */  哈希�?///通过N哈希表执行循�?

  /* This routine is only be called from within a read transaction. */ 只能被读事务所调用
  assert( pWal->readLock>=0 || pWal->lockError );  判断是否终止程序

  /* If the "last page" field of the wal-index header snapshot is 0, then
  ** no data will be read from the wal under any circumstances. Return early
  ** in this case as an optimization.  Likewise, if pWal->readLock==0, 
  ** then the WAL is ignored by the reader so return early, as if the 
  ** WAL were empty.如果wal-index头的“最后一页”字段快照为0,任何情况下没有数据将从Wal读取。同样的，如�?pWal->readLock�?，然后被读者忽�?好像WAL是空�?
  */
////* 如果wal-index头部快照的“最后一页”为0，则在任何环境下都不会有数据从wal中被读取�?
////  在这种情况下作为最优性提前返回�?
////  同样，如�?pWal->readLock==0，WAL被读取这忽视，就像WAL为空，被提前返回�?
*/
  if( iLast==0 || pWal->readLock==0 ){ 如果ILast或readLock�?
    *pInWal = 0;  数据不是从wal �?所以为�?
    return SQLITE_OK; 返回ok
  }

  /* Search the hash table or tables for an entry matching page number
  ** pgno. Each iteration of the following for() loop searches one
  ** hash table (each hash table indexes up to HASHTABLE_NPAGE frames).
  ** 搜索哈希表搜索一个和页码相匹配的条目�?每一次迭�?都是搜索一�?
////*在哈希表或表中搜索与pgno（要访问的页号）匹配的页�?
////下面的for循环每一次迭代搜索一个哈希表（每一个哈希表索引在HASHTABLE_NPAGE架构中）�?
*/
  ** This code might run concurrently to the code in walIndexAppend()
  ** that adds entries to the wal-index (and possibly to this hash 
  ** table). This means the value just read from the hash 
  ** slot (aHash[iKey]) may have been added before or after the 
  ** current read transaction was opened. Values added after the
  ** read transaction was opened may have been written incorrectly -
  ** i.e. these slots may contain garbage data. However, we assume
  ** that any slots written before the current read transaction was
  ** opened remain unmodified.
  **
////*这些代码可能同时运行在walIndexAppend()中会给wal-index添加条目（可能也会给哈希表添加）的代码�?
//// 这意味着刚从哈希表中读的值可能在当前读事务被开始前或后被添加�?
//// 在当前读事务被开始后加入的值可能已经被错误写入，即：这些条目可能包含垃圾数据�?
//// 然而，我们假设，当前读事务被开始前被写入的条目是不被更改的�?
*/
  ** For the reasons above, the if(...) condition featured in the inner
  ** loop of the following block is more stringent that would be required 
  ** if we had exclusive access to the hash-table:
  **
  **   (aPgno[iFrame]==pgno): 
  **     This condition filters out normal hash-table collisions.
  **
  **   (iFrame<=iLast): 
  **     This condition filters out entries that were added to the hash
  **     table after the current read-transaction had started.
  */
////*由于以上原因�?
  for(iHash=walFramePage(iLast); iHash>=0 && iRead==0; iHash--){ 获取最新页所对应的hash�?
    volatile ht_slot *aHash;      /* Pointer to hash table */ 哈希表的指针
    volatile u32 *aPgno;          /* Pointer to array of page numbers */ 页码的指�?
    u32 iZero;                    /* Frame number corresponding to aPgno[0] */ Frame�?aPgno�?】一致则为真
    int iKey;                     /* Hash slot index */ 哈希槽索�?
    int nCollide;                 /* Number of hash collisions remaining */哈希碰撞的次�?
    int rc;                       /* Error code */ 

    rc = walHashGet(pWal, iHash, &aHash, &aPgno, &iZero); 调用函数 获取�?
    if( rc!=SQLITE_OK ){ 如果调用不成功，返回
      return rc;
    }
    nCollide = HASHTABLE_NSLOT;  哈希碰撞数目
    for(iKey=walHash(pgno); aHash[iKey]; iKey=walNextHash(iKey)){  循环哈希
      u32 iFrame = aHash[iKey] + iZero;        计算获取 IFrame
      if( iFrame<=iLast && aPgno[aHash[iKey]]==pgno ){  如果IFrame小于Ilast�?为真
        /* assert( iFrame>iRead ); -- not true if there is corruption */
        iRead = iFrame;  
      }
      if( (nCollide--)==0 ){  如果hash碰撞�?�?
        return SQLITE_CORRUPT_BKPT; 返回
      }
    }
  }

#ifdef SQLITE_ENABLE_EXPENSIVE_ASSERT
  /* If expensive assert() statements are available, do a linear search
  ** of the wal-index file content. Make sure the results agree with the
  ** result obtained using the hash indexes above.  */如果昂贵的assert()语句可以使用,进行线性搜索wal-index文件的内容。确保结果同意结果上面使用的散列索引
 ////*如果昂贵的assert()语句可以使用,进行线性搜索wal-index文件的内容�?
//// 确保该结果与上面使用哈希索引获得的结果一致�?
*/
 {
    u32 iRead2 = 0;
    u32 iTest; 
    for(iTest=iLast; iTest>0; iTest--){  循环iLast 
      if( walFramePgno(pWal, iTest)==pgno ){  ////如果找到相应的页 
        iRead2 = iTest;////取得数据页在wal中的索引
        break;                                 �?跳出循环
      }
    }
    assert( iRead==iRead2 );
  }
#endif

  /* If iRead is non-zero, then it is the log frame number that contains the
  ** required page. Read and return data from the log file.如果iRead�?,那么它就是日志框架包含数量所需的页面。从日志文件中读取并返回数据
  */
////*如果iRead�?,那么它就是包含所要访问页的日志框架的编号�?
////从日志文件中读取并返回数�?
*/
  if( iRead ){  如果非空
    int sz;    
    i64 iOffset; 
    sz = pWal->hdr.szPage; 获取Wal的页的大�?
    sz = (sz&0xfe00) + ((sz&0x0001)<<16); ？？�?
    testcase( sz<=32768 ); 测试sz的范�?
    testcase( sz>=65536 );
    iOffset = walFrameOffset(iRead, sz) + WAL_FRAME_HDRSIZE;
    *pInWal = 1; 设置PINWal的值为1
    /* testcase( IS_BIG_INT(iOffset) ); // requires a 4GiB WAL */
    return sqlite3OsRead(pWal->pWalFd, pOut, (nOut>sz ? sz : nOut), iOffset); 调用系统函数
  }

  *pInWal = 0; 设置PINWal�?
  return SQLITE_OK; 返回 
}


/* 
** Return the size of the database in pages (or zero, if unknown).////以页为单位返回数据库的长度（或者当不知道时�?�?
*/
Pgno sqlite3WalDbsize(Wal *pWal){ 获取数据库大小，页为单位
  if( pWal && ALWAYS(pWal->readLock>=0) ){
    return pWal->hdr.nPage;  获取Wal有多少页
  }
  return 0;
}


/* 
** This function starts a write transaction on the WAL. 开始一个写事务
**
** A read transaction must have already been started by a prior call
** to sqlite3WalBeginReadTransaction().读事务必须在调用前已经开始通过调用 sqlite3WalBeginReadTransaction()�?
**
** If another thread or process has written into the database since
** the read transaction was started, then it is not possible for this
** thread to write as doing so would cause a fork.  So this routine
** returns SQLITE_BUSY in that case and no write transaction is started.如果另一个线程或进程写入数据库读事务开�?那么它是不可能的线程写这样做将导致一个叉。所以这个例程返回SQLITE_BUSY在这种情况下,没有写事务开始�?
** There can only be a single writer active at a time.同一时间内只能进行一个写
*/
////* 在WAL中开始一个写事务
//// 一个读事务必须通过优先调用sqlite3WalBeginReadTransaction()来开始�?
//// 如果这个读事务开始后，另一个线程或进程已经往数据库中写入了数据，那么该线程不可能执行写操作，因为会导致冲突�?
//// 因此，在这种情况下该程序会返回一个SQLITE_BUSY并且没有些食物被开始�?
//// 同一时间内只能进行一个写操作�?
*/
int sqlite3WalBeginWriteTransaction(Wal *pWal){
  int rc; 

  /* Cannot start a write transaction without first holding a read
  ** transaction. */ 写事务开启前必须有读事务在运�?
  assert( pWal->readLock>=0 ); 如果有锁

  if( pWal->readOnly ){ 如果不是只读////如果只读
    return SQLITE_READONLY;返回
  }

  /* Only one writer allowed at a time.  Get the write lock.  Return
  ** SQLITE_BUSY if unable. 同一时间内只能进行一个写，获取写锁。返回SQlote_busy 如果不能的话
  */
////* 同一时间内只能进行一个写。获取写锁。如果不能获得写锁，则返回SQlote_busy �?
  rc = walLockExclusive(pWal, WAL_WRITE_LOCK, 1); 加锁////获取排斥�?
  if( rc ){
    return rc;
  }
  pWal->writeLock = 1; 赋�?

  /* If another connection has written to the database file since the
  ** time the read transaction on this connection was started, then
  ** the write is disallowed.如果另一个连接后写入数据库文件时间读事务开始在这个连接,那么写无�?
  */
////* 如果另一个连接已经写入数据库文件而此时在这个连接上的读事务已经开始了，那么写是不被允许的�?
*/
  if( memcmp(&pWal->hdr, (void *)walIndexHdr(pWal), sizeof(WalIndexHdr))!=0 ){ 比较前sizeof(WalIndexHdr)字节
    walUnlockExclusive(pWal, WAL_WRITE_LOCK, 1); 解锁
    pWal->writeLock = 0; 更该它的�?
    rc = SQLITE_BUSY; 返回 �?
  }

  return rc;
}

/*
** End a write transaction.  The commit has already been done.  This
** routine merely releases the lock.写事务结束。提交已经完成。这程序只是释放�?
*/
int sqlite3WalEndWriteTransaction(Wal *pWal){
  if( pWal->writeLock ){  如果WAL有锁
    walUnlockExclusive(pWal, WAL_WRITE_LOCK, 1); 调用函数释放�?
    pWal->writeLock = 0; 更改参数
    pWal->truncateOnCommit = 0;
  }
  return SQLITE_OK; 成功返回
}

/*
** If any data has been written (but not committed) to the log file, this
** function moves the write-pointer back to the start of the transaction.
**如果已经将任何数据写到日志文�?但没提交),这函数移动写指针回到事务开始处�?
** Additionally, the callback function is invoked for each frame written
** to the WAL since the start of the transaction. If the callback returns
** other than SQLITE_OK, it is not invoked again and the error code is
** returned to the caller.
**此外,调用回调函数对每一帧WAL开始以来的事务。如果回调返回SQLITE_OK以外�?这不是再次调用和错误代码返回给调用者�?
** Otherwise, if the callback function does not return an error, this
** function returns SQLITE_OK.否则,如果回调函数不返回一个错�?这一点函数返回SQLITE_OK
*/
////*如果有任何数据已经被写到日志文件(但还没提�?,这个函数移动写指针回到事务开始处�?
//// 此外，回调函数被唤醒，直到到达WAL中该事物开始的地方�?
//// 如果回调函数返回的不是SQLITE_OK，则它将不会在被唤醒并返回错误码给调用者�?
//// 否则，如果没有返回任何错误吗，则返回SQLITE_OK�?
*/
int sqlite3WalUndo(Wal *pWal, int (*xUndo)(void *, Pgno), void *pUndoCtx){
  int rc = SQLITE_OK;
  if( ALWAYS(pWal->writeLock) ){ 如果pWal->writeLock是否为真�?
    Pgno iMax = pWal->hdr.mxFrame;   定义Pgno 赋值Wal中最大的�?
    Pgno iFrame; 定义帧数
  
    /* Restore the clients cache of the wal-index header to the state it
    ** was in before the client began writing to the database. 恢复客户Wal索引头的缓存到客户开始向数据库写之前的状态�?
    */
    memcpy(&pWal->hdr, (void *)walIndexHdr(pWal), sizeof(WalIndexHdr)); 将WalindexHdr复制到初始状�?

    for(iFrame=pWal->hdr.mxFrame+1; 
        ALWAYS(rc==SQLITE_OK) && iFrame<=iMax; 
        iFrame++
    ){
      /* This call cannot fail. Unless the page for which the page number
      ** is passed as the second argument is (a) in the cache and 
      ** (b) has an outstanding reference, then xUndo is either a no-op
      ** (if (a) is false) or simply expels the page from the cache (if (b)
      ** is false).
**　这调用不能失败。除非页码的页面作为在缓存中第二个参数传递的�?a)�?b)有一个明显的的引�?然后xUndo要么是无为法(如果(a)是假�?或从缓存中排出一些页(如果(b)是假�?�?
/////*这个回调不会失败。除非当第二个参数a存在于缓冲区并且b被调用时这个页的页码已经被通过，那么xUndo要么是一个空操作（如果a是错的）
///// 要么从缓冲区讲该页冲掉（如果b是错的）�?
      ** If the upper layer is doing a rollback, it is guaranteed that there
      ** are no outstanding references to any page other than page 1. And
      ** page 1 is never written to the log until the transaction is
      ** committed. As a result, the call to xUndo may not fail.
      */如果上层做回�?这是保证没有明显的第1页以外的任何页面的引用。和�?页不会写入日�?直到事务。因�?调用xUndo可能不会失败
      assert( walFramePgno(pWal, iFrame)!=1 );
      rc = xUndo(pUndoCtx, walFramePgno(pWal, iFrame));
    }
    walCleanupHash(pWal); 清除哈希
  }
  assert( rc==SQLITE_OK ); 如果rc不为sqlite——ok 
  return rc; 返回
}

/* 
** Argument aWalData must point to an array of WAL_SAVEPOINT_NDATA u32 
** values. This function populates the array with values required to 
** "rollback" the write position of the WAL handle back to the current 
** point in the event of a savepoint rollback (via WalSavepointUndo()).
*/ 参数aWalData 必须 指向WAL_SAVEPOINT_NDATA 数组，为u32�?
////* 参数aWalData 必须 指向WAL_SAVEPOINT_NDATA 数组，为u32�?
////  这个函数给数组赋值，要求将回滚的WAL的写位置给在回滚事件中的当前指针�?经由 WalSavepointUndo())�?
void sqlite3WalSavepoint(Wal *pWal, u32 *aWalData){
  assert( pWal->writeLock ); 如果Wal中有�?
  aWalData[0] = pWal->hdr.mxFrame; 为awaldata的数组赋�?
  aWalData[1] = pWal->hdr.aFrameCksum[0];
  aWalData[2] = pWal->hdr.aFrameCksum[1];
  aWalData[3] = pWal->nCkpt;
}

/* 
** Move the write position of the WAL back to the point identified by
** the values in the aWalData[] array. aWalData must point to an array
** of WAL_SAVEPOINT_NDATA u32 values that has been previously populated
** by a call to WalSavepoint().
*/将写入位置移回到保存点位置�?
/////*通过aWalData[] 数组中的值将WAL的写指针给指针证实�?
///// 是sqlite3WalSavepoint的逆过程�?
*/
int sqlite3WalSavepointUndo(Wal *pWal, u32 *aWalData){
  int rc = SQLITE_OK; 先令rc赋值为ok

  assert( pWal->writeLock ); 判段Wal中是否有�?
  assert( aWalData[3]!=pWal->nCkpt || aWalData[0]<=pWal->hdr.mxFrame ); 判读aWalData和Wal中的参数是否相等

  if( aWalData[3]!=pWal->nCkpt ){ 
    /* This savepoint was opened immediately after the write-transaction
    ** was started. Right after that, the writer decided to wrap around
    ** to the start of the log. Update the savepoint values to match.
    */ 在一个写事务开始，这个保存点也立即被打开，　之后,写操作决定回滚日志开始出，更新保存点值与它相匹配�?
    aWalData[0] = 0;
    aWalData[3] = pWal->nCkpt;
  }

  if( aWalData[0]<pWal->hdr.mxFrame ){  如果数组�?】的值小于Wal的最大帧�?
    pWal->hdr.mxFrame = aWalData[0]; 更改�?
    pWal->hdr.aFrameCksum[0] = aWalData[1]; 更改参数�?
    pWal->hdr.aFrameCksum[1] = aWalData[2];
    walCleanupHash(pWal); 调用函数
  }

  return rc;
}


/*
** This function is called just before writing a set of frames to the log
** file (see sqlite3WalFrames()). It checks to see if, instead of appending
** to the current log file, it is possible to overwrite the start of the
** existing log file with the new frames (i.e. "reset" the log). If so,
** it sets pWal->hdr.mxFrame to 0. Otherwise, pWal->hdr.mxFrame is left
** unchanged.
**这个函数被调用之前写框架集到日志文件。相�?它检查是否添加当前日志文�?可以覆盖的开始现有日志文件的新帧.如果是这�? 这集pWal - > hdr.mxFrame�?。　否则,pWal - > hdr。mxFrame不变�?
** SQLITE_OK is returned if no error is encountered (regardless of whether
** or not pWal->hdr.mxFrame is modified). An SQLite error code is returned
** if an error occurs.成功返回，出错返�?
*/
////*在写一组帧到日志文件之前调用该函数�?
//// 它检查是否用新的frame（框架或帧）重写日志文件的开头部分，而不是给当前日志文件添加帧�?
//// 如果是，则设置pWal->hdr.mxFrame=0.否则，pWal->hdr.mxFrame不被改变�?
*/
static int walRestartLog(Wal *pWal){
  int rc = SQLITE_OK; 
  int cnt; 重新读的次数

  if( pWal->readLock==0 ){ 如果加锁�?
    volatile WalCkptInfo *pInfo = walCkptInfo(pWal); 获取校验信息
    assert( pInfo->nBackfill==pWal->hdr.mxFrame ); 如果两者不同，终止程序
    if( pInfo->nBackfill>0 ){  
      u32 salt1;     定义32为的变量
      sqlite3_randomness(4, &salt1); 调用函数
      rc = walLockExclusive(pWal, WAL_READ_LOCK(1), WAL_NREADER-1); 调用加锁函数
      if( rc==SQLITE_OK ){ 如果成功
        /* If all readers are using WAL_READ_LOCK(0) (in other words if no
        ** readers are currently using the WAL), then the transactions
        ** frames will overwrite the start of the existing log. Update the
        ** wal-index header to reflect this. 如果所有的读都加WAL_READ_LOCK(0)，　那么,事务帧将重写现有的日志的。　更新wal-index头来反映这一�?
        **
        ** In theory it would be Ok to update the cache of the header only
        ** at this point. But updating the actual wal-index header is also
        ** safe and means there is no special case for sqlite3WalUndo()
        ** to handle if this transaction is rolled back. 理论上说可以成功进行更新�?
        */
/////* 如果所有的访问者都使用WAL_READ_LOCK(0)（换句话说，当前没有读取WAl的事务），那么事务frame（帧或框架）
/////  将覆盖现有的日志的开始部分。更新wal-index的头部来反映此�?
/////  理论上说，只有此时才能更新缓冲区的头部。但是更新实际的wal-index头是安全的，并且这意味着对sqlite3WalUndo()
/////  没有特殊的情况来处理是否事务被回滚了�?
*/
        int i;                    /* Loop counter */ 循环变量
        u32 *aSalt = pWal->hdr.aSalt;       /* Big-endian salt values */ 获取混淆�?

        pWal->nCkpt++; 自加
        pWal->hdr.mxFrame = 0; wal的最大帧�?
        sqlite3Put4byte((u8*)&aSalt[0], 1 + sqlite3Get4byte((u8*)&aSalt[0])); 调用函数
        aSalt[1] = salt1;  赋�?
        walIndexWriteHdr(pWal);调用函数
        pInfo->nBackfill = 0; 赋�?
        pInfo->aReadMark[1] = 0; 赋�?
        for(i=2; i<WAL_NREADER; i++) pInfo->aReadMark[i] = READMARK_NOT_USED; 赋�?
        assert( pInfo->aReadMark[0]==0 ); 判断
        walUnlockExclusive(pWal, WAL_READ_LOCK(1), WAL_NREADER-1); 解锁
      }else if( rc!=SQLITE_BUSY ){
        return rc;
      }
    }
    walUnlockShared(pWal, WAL_READ_LOCK(0)); 解除共享�?
    pWal->readLock = -1; 更改参数�?
    cnt = 0; 赋�?
    do{
      int notUsed;
      rc = walTryBeginRead(pWal, &notUsed, 1, ++cnt);
    }while( rc==WAL_RETRY );开始读
    assert( (rc&0xff)!=SQLITE_BUSY ); /* BUSY not possible when useWal==1 */////当useWal==1时，BUSY是不可能的�?
    testcase( (rc&0xff)==SQLITE_IOERR );测试
    testcase( rc==SQLITE_PROTOCOL );测试
    testcase( rc==SQLITE_OK );测试
  }
  return rc;
}

/*
** Information about the current state of the WAL file and where
** the next fsync should occur - passed from sqlite3WalFrames() into
** walWriteToLog(). 当前Wal文件所处状态的信息和何处发生同步通过sqlite3WalFrames()转化�?walWriteToLog()的信息�?
*/
////*当前Wal文件所处状态的信息和下一个通过sqlite3WalFrames()转化�?walWriteToLog()的同步信�?
*/
typedef struct WalWriter {
  Wal *pWal;                   /* The complete WAL information */ 定义Wal 指针变量  ///WAL的完整信�?
  sqlite3_file *pFd;           /* The WAL file to which we write */ Wal  ////我们要写的文�?
  sqlite3_int64 iSyncPoint;    /* Fsync at this offset */  /////在该偏移量下帧同步信�?
  int syncFlags;               /* Flags for the fsync */同步的标�?
  int szPage;                  /* Size of one page */ 页的大小
} WalWriter;

/*
** Write iAmt bytes of content into the WAL file beginning at iOffset.
** Do a sync when crossing the p->iSyncPoint boundary.
** 通过偏移向Wal文件中IAmt 字节。超出p - > iSyncPoint边界时同步�?
** In other words, if iSyncPoint is in between iOffset and iOffset+iAmt,
** first write the part before iSyncPoint, then sync, then write the
** rest.
*/
////*将iAmt字节的内容写入到WAL文件的开始的iOffset偏移量处�?
//// 当超过p->iSyncPoint边界时，做帧同步信息标记�?
//// 换句话说，在iSyncPoint之前首先写下这部分，则sync，如果iSyncPoint在iOffset和iOffset+iAmt之间，那么写下的其余部分�?
static int walWriteToLog(
  WalWriter *p,              /* WAL to write to */ WalWriter 指针变量
  void *pContent,            /* Content to be written */ 要写的内�?
  int iAmt,                  /* Number of bytes to write */ 写多少字�?
  sqlite3_int64 iOffset      /* Start writing at this offset */ 开始写
){
  int rc; 返回�?
  if( iOffset<p->iSyncPoint && iOffset+iAmt>=p->iSyncPoint ){                                    P->ISyncPoint 是否在偏移和偏移后之�?
    int iFirstAmt = (int)(p->iSyncPoint - iOffset); ////计算要插入的长度
    rc = sqlite3OsWrite(p->pFd, pContent, iFirstAmt, iOffset); 调用系统函数，写入内�?
    if( rc ) return rc; 是否成功
    iOffset += iFirstAmt; 计算IOffset的�?
    iAmt -= iFirstAmt; 计算 iAMt的�?
    pContent = (void*)(iFirstAmt + (char*)pContent); 获取要写内容
    assert( p->syncFlags & (SQLITE_SYNC_NORMAL|SQLITE_SYNC_FULL) ); 判断是否成功，不成功则终止程�?
    rc = sqlite3OsSync(p->pFd, p->syncFlags);调用系统同步函数�?
    if( iAmt==0 || rc ) return rc;  返回�?
  }
  rc = sqlite3OsWrite(p->pFd, pContent, iAmt, iOffset); 调用写入函数
  return rc;
}

/*
** Write out a single frame of the WAL 写出Wal的单�?
*/
////写下WAL的一个单帧�?
static int walWriteOneFrame(
  WalWriter *p,               /* Where to write the frame */ 定义WalWrite 指针///// 定义要写下frame的WalWrite 指针
  PgHdr *pPage,               /* The page of the frame to be written */ 要写入的帧页   ////要写入的frame的页
  int nTruncate,              /* The commit flag.  Usually 0.  >0 for commit */ 提交标记 ////通常�?，如果大�?，则已经提交
  sqlite3_int64 iOffset       /* Byte offset at which to write */ 偏移字节   ////要写如的字节偏移�?
){
  int rc;                         /* Result code from subfunctions */从子函数返回
  void *pData;                    /* Data actually written */实际要写数据
  u8 aFrame[WAL_FRAME_HDRSIZE];   /* Buffer to assemble frame-header in */帧头数据要编译的缓冲�?///组装frame头的缓冲�?
#if defined(SQLITE_HAS_CODEC)
  if( (pData = sqlite3PagerCodec(pPage))==0 ) return SQLITE_NOMEM; 获取包含页内容的指针，如果出错，返回null
#else
  pData = pPage->pData;给pData赋�?
#endif
  walEncodeFrame(p->pWal, pPage->pgno, nTruncate, pData, aFrame);调用函数进行编码�?
  rc = walWriteToLog(p, aFrame, sizeof(aFrame), iOffset); 调用函数写入到Wal中�?//将frame头写入WAl�?
  if( rc ) return rc;
  /* Write the page data */ 编写页面数据
  rc = walWriteToLog(p, pData, p->szPage, iOffset+sizeof(aFrame));调用函数写入到页�?//将pData写入WAl�?
  return rc;
}

/* 
** Write a set of frames to the log. The caller must hold the write-lock
** on the log file (obtained using sqlite3WalBeginWriteTransaction()).编写框架集到日志中。调用者必须持有写锁在日志文件�?
*/
////*往日志里写入一组frame。调用者必须持有在日志文件上的写锁（使用sqlite3WalBeginWriteTransaction()获得）�?
int sqlite3WalFrames(
  Wal *pWal,                      /* Wal handle to write to */ 定义变量 ///要写入的日志文件的指�?
  int szPage,                     /* Database page-size in bytes */ 数据库页的大小，单位是字�?
  PgHdr *pList,                   /* List of dirty pages to write */  脏页写的列表   ////写脏页的列表的指�?
  Pgno nTruncate,                 /* Database size after this commit */ 这次提交后数据库大小
  int isCommit,                   /* True if this is a commit */如果提交，则为真�?
  int sync_flags                  /* Flags to pass to OsSync() (or 0) */ 同步的标�?
){
  int rc;                         /* Used to catch return codes */ 返回�?
  u32 iFrame;                     /* Next frame address */ 下一帧地址
  PgHdr *p;                       /* Iterator to run through pList with. */ 沿着pList 迭代 
  PgHdr *pLast = 0;               /* Last frame in list */ 在链表中最后一�? ////初始值为0
  int nExtra = 0;                 /* Number of extra copies of last page */   ///最后一页的额外的复制的数量，初始值为0
  int szFrame;                    /* The size of a single frame */ 单帧的大�?
  i64 iOffset;                    /* Next byte to write in WAL file */ 偏移字节  ///要写入日志文件中的下一字节
  WalWriter w;                    /* The writer */ WalW的变�?  /////当前Wal文件所处状态的信息和下一个通过sqlite3WalFrames()转化�?walWriteToLog()的同步信�?

  assert( pList );判断链表是否为空，为空则终止程序
  assert( pWal->writeLock ); 判断是否有写�?

  /* If this frame set completes a transaction, then nTruncate>0.  If
  ** nTruncate==0 then this frame set does not complete the transaction. */
 ////*如果完成了一个事务，则nTruncate>0。如果nTruncate==0，则该事务没有完成�?
*/
  assert( (isCommit!=0)==(nTruncate!=0) ); ////判断该事务成功提交，并写入了frame�?

#if defined(SQLITE_TEST) && defined(SQLITE_DEBUG)////调试和除虫，输出wal的路�?
  { int cnt; 
  for(cnt=0, p=pList; p; p=p->pDirty, cnt++){} 
    WALTRACE(("WAL%p: frame write begin. %d frames. mxFrame=%d. %s\n",
              pWal, cnt, pWal->hdr.mxFrame, isCommit ? "Commit" : "Spill"));
  }
#endif

  /* See if it is possible to write these frames into the start of the
  ** log file, instead of appending to it at pWal->hdr.mxFrame.
  */是否可以写这些框的开始日志文�?而不是附加在pWal - > hdr.mxFrame�?
////*检查是否可以将这些frame写入日志文件的开头部分，而不是附加在pWal - > hdr.mxFrame后面�?
*/
  if( SQLITE_OK!=(rc = walRestartLog(pWal)) ){
    return rc;
  }

  /* If this is the first frame written into the log, write the WAL
  ** header to the start of the WAL file. See comments at the top of
  ** this source file for a description of the WAL header format.
  */如果这是第一帧写入日�?Wal头数据写在WAL文件。看部这个源文件的描述WAL标题格式
/////*如果第一个frame写入了日志，则将WAL的头部写入日志文件的开始部分�?
*/
  iFrame = pWal->hdr.mxFrame; 获取Wal中的最大帧参数�?
  if( iFrame==0 ){
    u8 aWalHdr[WAL_HDRSIZE];      /* Buffer to assemble wal-header in */
    u32 aCksum[2];                /* Checksum for wal-header */

/////以下为为日志文件的头部的8个部分赋值�?
    sqlite3Put4byte(&aWalHdr[0], (WAL_MAGIC | SQLITE_BIGENDIAN)); 调用函数 ，为aWalHdr[] 赋�?
    sqlite3Put4byte(&aWalHdr[4], WAL_MAX_VERSION);
    sqlite3Put4byte(&aWalHdr[8], szPage);
    sqlite3Put4byte(&aWalHdr[12], pWal->nCkpt);
    if( pWal->nCkpt==0 ) sqlite3_randomness(8, pWal->hdr.aSalt); 如果校验信息�?，为aSalta随机8个字�?
    memcpy(&aWalHdr[16], pWal->hdr.aSalt, 8); 调用字符串赋�?
    walChecksumBytes(1, aWalHdr, WAL_HDRSIZE-2*4, 0, aCksum); 调用校验和函�?
    sqlite3Put4byte(&aWalHdr[24], aCksum[0]); 调用函数 ，为aWalHdr[] 赋�?
    sqlite3Put4byte(&aWalHdr[28], aCksum[1]);
    
    pWal->szPage = szPage; 为Wal参数赋�?
    pWal->hdr.bigEndCksum = SQLITE_BIGENDIAN;
    pWal->hdr.aFrameCksum[0] = aCksum[0];
    pWal->hdr.aFrameCksum[1] = aCksum[1];
    pWal->truncateOnCommit = 1;

    rc = sqlite3OsWrite(pWal->pWalFd, aWalHdr, sizeof(aWalHdr), 0); 调用系统写入函数
    WALTRACE(("WAL%p: wal-header write %s\n", pWal, rc ? "failed" : "ok"));
    if( rc!=SQLITE_OK ){
      return rc;
    }

    /* Sync the header (unless SQLITE_IOCAP_SEQUENTIAL is true or unless
    ** all syncing is turned off by PRAGMA synchronous=OFF).  Otherwise
    ** an out-of-order write following a WAL restart could result in
    ** database corruption.  See the ticket: 同步头数据，（除非SQLITE_IOCAP_SEQUENTIAL 是真的或 所有的同步都被PRAGMA synchronous=OFF关闭）否则一个无序写WAL重启后可能导致　　* *数据库损�?
    **
    **     http://localhost:591/sqlite/info/ff5be73dee
    */
////* 头部的帧同步信息（除非SQLITE_IOCAP_SEQUENTIAL是真的或者除非帧同步信息被PRAGMA关闭，synchronous=OFF）�?
///// 否则由于乱序的写入引起的WAL重启会导致数据库的崩溃�?
*/
    if( pWal->syncHeader && sync_flags ){  如果Walden的参数为真且同步标记为真
      rc = sqlite3OsSync(pWal->pWalFd, sync_flags & SQLITE_SYNC_MASK);调用系统同步函数
      if( rc ) return rc;
    }
  }
  assert( (int)pWal->szPage==szPage ); 如果Wal的参数szPage 与当前szPage不同，则终止程序

  /* Setup information needed to write frames into the WAL */设置帧写入在Wal所需的信�?
////设置帧写入Wal所需的信�?
  w.pWal = pWal; 设置WalWrite结构体的参数�?
  w.pFd = pWal->pWalFd;
  w.iSyncPoint = 0;
  w.syncFlags = sync_flags;
  w.szPage = szPage;
  iOffset = walFrameOffset(iFrame+1, szPage); 计算偏移�?///The offset returned  is to the start of the write-ahead log frame-header.
  szFrame = szPage + WAL_FRAME_HDRSIZE; 

  /* Write all frames into the log file exactly once */所有帧写入日志文件完全一�?///一次性将所有帧写入日志文件
  for(p=pList; p; p=p->pDirty){ 对链表进行遍�?////对脏链进行遍�?
    int nDbSize;   /* 0 normally.  Positive == commit flag */ 正常�? ，正数为commit flag
    iFrame++; /////帧的地址自增
    assert( iOffset==walFrameOffset(iFrame, szPage) ); 如果偏移量为
    nDbSize = (isCommit && p->pDirty==0) ? nTruncate : 0; 如果提交且P的脏数据标记�? �?返回nTruncate，否则为0////如果提交且P链遍历结束，则返回nTruncate，否则为0
    rc = walWriteOneFrame(&w, p, nDbSize, iOffset); 调用函数写入到oneFrame
    if( rc ) return rc;
    pLast = p; 重新给pLast赋�?
    iOffset += szFrame;
  }

  /* If this is the end of a transaction, then we might need to pad
  ** the transaction and/or sync the WAL file.
  **如果这是结束一个事�?那么我们可能需要填补事务和/或同步WAL文件
  ** Padding and syncing only occur if this set of frames complete a
  ** transaction and if PRAGMA synchronous=FULL.  If synchronous==NORMAL
  ** or synchonous==OFF, then no padding or syncing are needed.
  ** 填充和同步只发生如果这组帧完成事�?如果编译指示同步=FULL。如果同�? 正常或synchonous = =OFF,然后不需要填充或同步�?
  ** If SQLITE_IOCAP_POWERSAFE_OVERWRITE is defined, then padding is not
  ** needed and only the sync is done.  If padding is needed, then the
  ** final frame is repeated (with its commit mark) until the next sector 
  ** boundary is crossed.  Only the part of the WAL prior to the last
  ** sector boundary is synced; the part of the last frame that extends
  ** past the sector boundary is written after the sync.
  */如果SQLITE_IOCAP_POWERSAFE_OVERWRITE  被定义，�?padding不需要，只用做同步。如果需要填�?然后最后一帧重�?其提交标�?,直到下一个部分边界交叉。只有WAL之前最后的一部分边界是同步的;最后一帧扩展的一部分过去部分边界是同步后写的
 //////*如果这是一个事务的结束,那么我们可能需要填补事务和/或同步WAL文件�?
/////   如果一个事务完成了一组frame的写入且PRAGMA synchronous=FULL，则填充和同步才会发生�?
/////   如果synchronous==NORMAL�?synchonous==OFF，那么填充和同步都不需要�?
/////   SQLITE_IOCAP_POWERSAFE_OVERWRITE被定义了，则填充是不被需要的，只有同步会发生�?
/////   如果需要填充，那么最后一帧被重复写入(其提交标�?,直到下一个部分边界被超过�?
/////   只有WAL之前最后的一部分边界是同步的;最后一帧扩展的超过边界的部分是同步后写的�?
*/
  if( isCommit && (sync_flags & WAL_SYNC_TRANSACTIONS)!=0 ){ 
    if( pWal->padToSectorBoundary ){
      int sectorSize = sqlite3OsSectorSize(pWal->pWalFd);通过调用系统函数 获取？？
      w.iSyncPoint = ((iOffset+sectorSize-1)/sectorSize)*sectorSize;
      while( iOffset<w.iSyncPoint ){///如果需要填�?
        rc = walWriteOneFrame(&w, pLast, nTruncate, iOffset);
        if( rc ) return rc;
        iOffset += szFrame;
        nExtra++;//最后一帧的复制的数量自�?
      }
    }else{
      rc = sqlite3OsSync(w.pFd, sync_flags & SQLITE_SYNC_MASK);
    }
  }

  /* If this frame set completes the first transaction in the WAL and
  ** if PRAGMA journal_size_limit is set, then truncate the WAL to the
  ** journal size limit, if possible.
  */如果这个帧设置完成第一个事�?如果编译指示journal_size_limit被设�?然后截断WAL的限制日志大�?
/////如果如果这个帧设置在WAL上完成第一个事务并且PRAGMA journal_size_limit被设置，那么如果可能的话将WAL截断设置为journal_size_limit�?
  if( isCommit && pWal->truncateOnCommit && pWal->mxWalSize>=0 ){
    i64 sz = pWal->mxWalSize; 获取wal 可能的最大�?
    if( walFrameOffset(iFrame+nExtra+1, szPage)>pWal->mxWalSize ){     
      sz = walFrameOffset(iFrame+nExtra+1, szPage); 
    }
    walLimitSize(pWal, sz);调用函数限制Wal的大�?
    pWal->truncateOnCommit = 0;
  }

  /* Append data to the wal-index. It is not necessary to lock the 
  ** wal-index to do this as the SQLITE_SHM_WRITE lock held on the wal-index
  ** guarantees that there are no other writers, and no data that may
  ** be in use by existing readers is being overwritten.
  */将数据添加到wal-index。没有必要锁定wal-index为此wal-index SQLITE_SHM_WRITE锁了保证没有其它写操�?和没有数据被现有的读操作使用且被覆盖�?
 /////*将数据附加到 wal-index�?
////// 为了这样做，没必要给 wal-index上锁，因�?wal-index持有的SQLITE_SHM_WRITE锁保证了没有其他的写者并且没有可能正被现存访�?
/////  者使用的数据被重写�?
*/
 iFrame = pWal->hdr.mxFrame;
  for(p=pList; p && rc==SQLITE_OK; p=p->pDirty){   对链表进行遍�?
    iFrame++;
    rc = walIndexAppend(pWal, iFrame, p->pgno); 调用函数
  }
  while( rc==SQLITE_OK && nExtra>0 ){ //将最后一帧的复制映射到WAL
    iFrame++;
    nExtra--;
    rc = walIndexAppend(pWal, iFrame, pLast->pgno);
  }

  if( rc==SQLITE_OK ){
    /* Update the private copy of the header. */ 更新头数据的私有复制
    pWal->hdr.szPage = (u16)((szPage&0xff00) | (szPage>>16));
    testcase( szPage<=32768 ); 测试Szpage所在的范围
    testcase( szPage>=65536 );
    pWal->hdr.mxFrame = iFrame;  设置Wal中参数�?
    if( isCommit ){  如果提交标志位真
      pWal->hdr.iChange++; 将Wal的IChange自加
      pWal->hdr.nPage = nTruncate; Waldennpage 赋�?
    }
    /* If this is a commit, update the wal-index header too. */如果这是一个提�?更新wal-index�?
    if( isCommit ){ 如果提交标志为真
      walIndexWriteHdr(pWal);
      pWal->iCallback = iFrame;
    }
  }

  WALTRACE(("WAL%p: frame write %s\n", pWal, rc ? "failed" : "ok"));
  return rc;
}

/* 
** This routine is called to implement sqlite3_wal_checkpoint() and
** related interfaces.这个程序被调用事项sqlite3_wal_checkpoint()和相关接口�?
**
** Obtain a CHECKPOINT lock and then backfill as much information as
** we can from WAL into the database.获取一个checkpoint 锁， 然后 尽可能多的回填信息到数据�?
**
** If parameter xBusy is not NULL, it is a pointer to a busy-handler
** callback. In this case this function runs a blocking checkpoint.
*/如果参数 Xbusy 不为空，有个Busy-Handler 指针 �?在这种情况下,该函数运行阻塞检查点
////*这个程序被调用来完成 sqlite3_wal_checkpoint()和相关的接口�?
//// 获取一个CHECKPOINT锁，并且尽可能多的回填WAL的信息到数据中�?
//// 如果参数xBusy不为空，它是busy-handler回滚的一个指针。在这种情况�?该函数运行阻塞检查点
*/
int sqlite3WalCheckpoint(
  Wal *pWal,                      /* Wal connection */ 定义 wal 指针
  int eMode,                      /* PASSIVE, FULL or RESTART */ 哪一种模�?
  int (*xBusy)(void*),            /* Function to call when busy */ 当忙时调�?
  void *pBusyArg,                 /* Context argument for xBusyHandler */ xBusyHandler的上下环�?
  int sync_flags,                 /* Flags to sync db file with (or 0) */同步标志///数据库文件的同步标志
  int nBuf,                       /* Size of temporary buffer */临时缓冲区的大小
  u8 *zBuf,                       /* Temporary buffer to use */ 临时缓存�?
  int *pnLog,                     /* OUT: Number of frames in WAL */在Wal中的个数  ////WAL中frame的个�?
  int *pnCkpt                     /* OUT: Number of backfilled frames in WAL */   ////WAL中被回填的frame的个�?
){
  int rc;                         /* Return code */ 返回�?
  int isChanged = 0;              /* True if a new wal-index header is loaded */  如果一个新的wal-index被加载，则为�?
  int eMode2 = eMode;             /* Mode to pass to walCheckpoint() */ 通过WalCheckpoint得到Mode

  assert( pWal->ckptLock==0 ); 如果有加检查点锁，�?
  assert( pWal->writeLock==0 ); 如果有加写锁，则

  if( pWal->readOnly ) return SQLITE_READONLY; 如果Wal中readonly 不为0
  WALTRACE(("WAL%p: checkpoint begins\n", pWal));
  rc = walLockExclusive(pWal, WAL_CKPT_LOCK, 1); 加锁 ////WAL获取排它�?
  if( rc ){  
    /* Usually this is SQLITE_BUSY meaning that another thread or process
    ** is already running a checkpoint, or maybe a recovery.  But it might
    ** also be SQLITE_IOERR. */ 
//////当用SQLITE_BUSY意味着 另一个线程或进程已经在运行checkpoint或则进行恢复�?也可能是SQLITE_IOERR
    return rc;
  }
  pWal->ckptLock = 1; 为ckptLock赋�?  //// 如果有一个checkpoint �?则值为�?

  /* If this is a blocking-checkpoint, then obtain the write-lock as well
  ** to prevent any writers from running while the checkpoint is underway.
  ** This has to be done before the call to walIndexReadHdr() below.如果这是一个blocking-checkpoint,然后获得写锁，以防止任何写当检查点正在运行。　　* *必须贼调用walIndexReadHdr()之前做�?
  **
  ** If the writer lock cannot be obtained, then a passive checkpoint is
  ** run instead. Since the checkpointer is not holding the writer lock,
  ** there is no point in blocking waiting for any readers. Assuming no 
  ** other error occurs, this function will return SQLITE_BUSY to the caller.
  */如果无法获得写锁,那么一个被动的检查点将代替运行。由于checkpoint无法加载锁，是毫无意义的阻塞等待任何读者，假设没有其他错误发生�?该函数将返回SQLITE_BUSY给调用者�?
 /////*如果有blocking-checkpoint，那么尽管checkpoint在进行中，都会获得一个写锁以阻止任何的写程序运行�?
/////  在下面调用walIndexReadHdr()之前，上述已经被做�?
/////  如果不能获得写锁，那么运行一个消极的checkpoint �?
/////  因为checkpointer没有持有写锁，为任何访问者阻塞等待是没有意义的�?
/////  假定没有其他错误发生，这个程序会为调用者返回SQLITE_BUSY �?
*/
  if( eMode!=SQLITE_CHECKPOINT_PASSIVE ){  如果模式不是SQLITE_CHECKPOINT_PASSIVE 
    rc = walBusyLock(pWal, xBusy, pBusyArg, WAL_WRITE_LOCK, 1); ////试图获取Wal的排他锁根据LockIdex �?�?
    if( rc==SQLITE_OK ){ 如果调用成功
      pWal->writeLock = 1;  更改参数�?
    }else if( rc==SQLITE_BUSY ){ 如果返回时Sqlite_busy
      eMode2 = SQLITE_CHECKPOINT_PASSIVE;  更该它的�?
      rc = SQLITE_OK;
    }
  }

  /* Read the wal-index header. */ 读取 Wal-index �?
  if( rc==SQLITE_OK ){  
    rc = walIndexReadHdr(pWal, &isChanged); 
  }

  /* Copy data from the log to the database file. */ 将日志文件中的数据拷贝到 数据文件�?�?
  if( rc==SQLITE_OK ){
    if( pWal->hdr.mxFrame && walPagesize(pWal)!=nBuf ){  如果信息不匹�?
      rc = SQLITE_CORRUPT_BKPT; 返回
    }else{
      rc = walCheckpoint(pWal, eMode2, xBusy, pBusyArg, sync_flags, zBuf); 调用检查点函数
    }

    /* If no error occurred, set the output variables. */ 如果没有error 发生，设置输出变�?
    if( rc==SQLITE_OK || rc==SQLITE_BUSY ){ 
      if( pnLog ) *pnLog = (int)pWal->hdr.mxFrame; 获取Frame的个�?
      if( pnCkpt ) *pnCkpt = (int)(walCkptInfo(pWal)->nBackfill); 回填Frame个数
    }
  }

  if( isChanged ){  是否改变
    /* If a new wal-index header was loaded before the checkpoint was 
    ** performed, then the pager-cache associated with pWal is now
    ** out of date. So zero the cached wal-index header to ensure that
    ** next time the pager opens a snapshot on this database it knows that
    ** the cache needs to be reset.
    */
/////*如果一个新的wal-index头在checkpoint开始前被加载，那么页缓冲器与当前输出数据的pWal相连�?
///// 因此，为了保证下一次页管理器在知道缓冲器需要重置的数据库上打开一个快照，则使wal-index头部的缓冲器�?�?
    memset(&pWal->hdr, 0, sizeof(WalIndexHdr)); 清零
  }

  /* Release the locks. */ 释放�?
  sqlite3WalEndWriteTransaction(pWal);
  walUnlockExclusive(pWal, WAL_CKPT_LOCK, 1);
  pWal->ckptLock = 0;
  WALTRACE(("WAL%p: checkpoint %s\n", pWal, rc ? "failed" : "ok"));
  return (rc==SQLITE_OK && eMode!=eMode2 ? SQLITE_BUSY : rc);
}

/* Return the value to pass to a sqlite3_wal_hook callback, the
** number of frames in the WAL at the point of the last commit since
** sqlite3WalCallback() was called.  If no commits have occurred since
** the last call, then return 0.
*/
////*通过回调返回一个值，该值等于从sqlite3WalCallback()被调用开始最后一个事务提交时的frame的数量�?
//// 如果最后一次回调时没有任何的提交发生，则返�?�?
*/
int sqlite3WalCallback(Wal *pWal){
  u32 ret = 0;
  if( pWal ){
    ret = pWal->iCallback;
    pWal->iCallback = 0;
  }
  return (int)ret;
}

/*
** This function is called to change the WAL subsystem into or out
** of locking_mode=EXCLUSIVE.
**
////这个程序被调用来将WAL子系统改变成locking_mode=EXCLUSIVE�?
** If op is zero, then attempt to change from locking_mode=EXCLUSIVE
** into locking_mode=NORMAL.  This means that we must acquire a lock
** on the pWal->readLock byte.  If the WAL is already in locking_mode=NORMAL
** or if the acquisition of the lock fails, then return 0.  If the
** transition out of exclusive-mode is successful, return 1.  This
** operation must occur while the pager is still holding the exclusive
** lock on the main database file.
**
////*如果操作的个数为0，那么尝试将locking_mode=EXCLUSIVE改变为locking_mode=NORMAL�?
//// 这意味着我们必须pWal->readLock上获取一个锁�?
//// 如果WAL已经处于locking_mode=NORMAL状态或者如果请求锁失败，那么返�?.
//// 如果该事务是排他模型，则返回1.
//// 这个操作必须发生，而页管理器仍然在主数据库文件上持有排它锁�?
*/
** If op is one, then change from locking_mode=NORMAL into 
** locking_mode=EXCLUSIVE.  This means that the pWal->readLock must
** be released.  Return 1 if the transition is made and 0 if the
** WAL is already in exclusive-locking mode - meaning that this
** routine is a no-op.  The pager must already hold the exclusive lock
** on the main database file before invoking this operation.
**
////*如果操作的个数为1，那么将locking_mode=NORMAL变为locking_mode=EXCLUSIVE�?
//// 这意味着pWal->readLock必须被释放�?
//// 如果该事务已经被执行并且WAL已经在排它锁模式——意味着改程序是空操作，那么返回1�?
//// 在这个操作被唤醒之前，页管理器仍然在主数据库文件上持有排它锁�?
*/
** If op is negative, then do a dry-run of the op==1 case but do
** not actually change anything. The pager uses this to see if it
** should acquire the database exclusive lock prior to invoking
** the op==1 case.
*/
/////*如果操作的个数为负，那么做op==1的情形，但是这实际上没什么改变�?
///// 页管理器以此判断在唤醒op==1情形之前是否应该获取数据库的排它锁�?
*/
int sqlite3WalExclusiveMode(Wal *pWal, int op){
  int rc;
  assert( pWal->writeLock==0 );
  assert( pWal->exclusiveMode!=WAL_HEAPMEMORY_MODE || op==-1 );

  /* pWal->readLock is usually set, but might be -1 if there was a 
  ** prior error while attempting to acquire are read-lock. This cannot 
  ** happen if the connection is actually in exclusive mode (as no xShmLock
  ** locks are taken in this case). Nor should the pager attempt to
  ** upgrade to exclusive-mode following such an error.
  */
////*pWal->readLock通常被设置，但是如果在尝试获取读锁时返回一个错误，那么pWal->readLock可能�?1.
//// 如果链接处于排他模式，那么上述情况不能发生（因为在这种情况下没有xShmLock锁）�?
//// 页管理器也不应该在该错误情况下升级为排他模式�?
*/
  assert( pWal->readLock>=0 || pWal->lockError );
  assert( pWal->readLock>=0 || (op<=0 && pWal->exclusiveMode==0) );

  if( op==0 ){
    if( pWal->exclusiveMode ){
      pWal->exclusiveMode = 0;
      if( walLockShared(pWal, WAL_READ_LOCK(pWal->readLock))!=SQLITE_OK ){
        pWal->exclusiveMode = 1;
      }
      rc = pWal->exclusiveMode==0;
    }else{
      /* Already in locking_mode=NORMAL */
      rc = 0;
    }
  }else if( op>0 ){
    assert( pWal->exclusiveMode==0 );
    assert( pWal->readLock>=0 );
    walUnlockShared(pWal, WAL_READ_LOCK(pWal->readLock));
    pWal->exclusiveMode = 1;
    rc = 1;
  }else{
    rc = pWal->exclusiveMode==0;
  }
  return rc;
}

/* 
** Return true if the argument is non-NULL and the WAL module is using
** heap-memory for the wal-index. Otherwise, if the argument is NULL or the
** WAL module is using shared-memory, return false. 返回true,如果参数是null和WAL模块wal-index使用堆内存。否�?如果参数是NULL或WAL模块是使用共享内�?返回false
*/
int sqlite3WalHeapMemory(Wal *pWal){
  return (pWal && pWal->exclusiveMode==WAL_HEAPMEMORY_MODE );
}

#ifdef SQLITE_ENABLE_ZIPVFS
/*
** If the argument is not NULL, it points to a Wal object that holds a
** read-lock. This function returns the database page-size if it is known,
** or zero if it is not (or if pWal is NULL).如果参数不空,它指向Wal对象持有读锁。这个程序返回数据库页大小，或则0 
*/
///如果参数不空,它指向持有读锁的Wal对象。这个程序返回数据库页大小（当页可知），或当不可知时（或wal为空）返�?�?
int sqlite3WalFramesize(Wal *pWal){
  assert( pWal==0 || pWal->readLock>=0 ); 如果Wal�?，或持有读锁�?
  return (pWal ? pWal->szPage : 0); 返回数据库页大小�?
}
#endif

#endif /* #ifndef SQLITE_OMIT_WAL */
