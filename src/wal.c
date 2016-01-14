 /*
** 2010 February 1
**
** The author disclaims���� copyright to this source code.  In place of
** a legal notice���ɾ���, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
**
** This file contains the implementation ʵ��of a write-ahead log (WAL) used in 
** "journal��־_mode=WAL" mode.
**
** WRITE-AHEAD LOG (WAL) FILE FORMAT
**
** A WAL file consists of a header followed by zero or more "frames".֡
** Each frame records the revised�޸ĵ� content of a single page from the
** database file.  All changes to the database are recorded by writing
** frames into the WAL.  Transactions���� commit when a frame is written that
** contains a commit marker�ύ��ǩ.  A single WAL can and usually does record 
** multiple transactions.  Periodically���ڵ�, the content of the WAL is
** transferred back into the database file in an operation called a
** "checkpoint".
**
** A single WAL file can be used multiple times.  In other words, the
** WAL can fill up with frames and then be checkpointed and then new
** frames can overwrite the old ones.  A WAL always grows from beginning
** toward the end.  Checksums�ܺͼ�� and counters���� attached to each frame are
** used to determineȷ�� which frames within the WAL are valid and which
** are leftovers from prior checkpoints.
**
** The WAL header is 32 bytes in size and consists of the following eight
** big-endian 32-bit unsigned�޷��ŵ� integer values:
**
**
**     0: Magic number.  0x377f0682 or 0x377f0683
**     4: File format version.  Currently 3007000
**     8: Database page size.  Example: 1024
**    12: Checkpoint sequence���� number
**    16: Salt-1, random integer incremented���� with each checkpoint
**    20: Salt-2, a different random integer changing with each ckpt
**    24: Checksum-1 (first part of checksum for first 24 bytes of header).
**    28: Checksum-2 (second part of checksum for first 24 bytes of header).
**
** Immediately following the wal-header are zero or more frames. Each
** frame consists of a 24-byte frame-header followed by������ a <page-size> bytes
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
**        exactly match the checksum computed consecutively������ on the
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
** algorithm�㷨 used for the checksum is as follows:
** 
**   for i from 0 to n-1 step 2:
**     s0 += x[i] + s1;
**     s1 += x[i+1] + s0;
**   endfor
**
** Note that s0 and s1 are both weighted ��Ȩ��checksums using fibonacci weights
** in reverse order����� (the largest fibonacci weight occurs on the first element
** of the sequence being summed.)  The s1 value spans all 32-bit 
** terms of the sequence���� whereasȻ�� s0 omits the final term.
**
** On a checkpoint, the WAL is first VFS.xSync-ed, then valid content of the
** WAL is transferred�ᵽ into the database, then the database is VFS.xSync-ed.
** The VFS.xSync operations serve as write barriers - all writes launched
** before the xSync must complete before any write that launches after the
** xSync begins.
**
** After each checkpoint, the salt-1 value is incremented ����and the salt-2
** value is randomized.  This prevents old and new frames in the WAL from
** being considered valid at the same time and being checkpointing together
** following a crash.
**
** READER ALGORITHM
**
** To read a page from the database (call it page number P), a reader
** first checks the WAL to see if it contains page P.  If so, then the
** last valid instanceʵ�� of page P that is a followed by a commit frame
** or is a commit frame itself becomes the value read.  If the WAL
** contains no copies of page P that are valid and which are a commit
** frame or are followed by a commit frame, then page P is read from
** the database file.
**
** To start a read transaction, the reader records the index of the last
** valid frame in the WAL.  The reader uses this recorded "mxFrame" value
** for all subsequent����� read operations.  New transactions can be appended
** to the WAL, but as long as the reader uses its original mxFrame value
** and ignores the newly appended content, it will see a consistent snapshot
** of the database from a single point in time.  This technique allows
** multiple concurrent������ readers to view different versions of the database
** content simultaneouslyͬʱ��.
**
** The reader algorithm��ȡ�㷨 in the previous paragraphs works correctly, but 
** because frames for page P can appear anywhere within the WAL, the
** reader has to scan the entireȫ���� WAL looking for page P frames.  If the
** WAL is large (multiple megabytes is typical) that scan can be slow,
** and read performance���� suffers.  To overcome this problem, a separate
** data structure called the wal-index is maintained to expedite �ӿ�the
** search for frames of a particular page.
** 
** WAL-INDEX FORMAT ��־�����ṹ
**
** Conceptually, the wal-index is shared memory, though VFS implementations
** might choose to implementʵʩ the wal-index using a mmapped fileӳ���ļ�.  Because
** the wal-index is shared memory, SQLite does not support journal_mode=WAL 
** on a network filesystem.  All users of the database must be able to
** share memory�����ڴ�.
**
** The wal-index is transient���ݵ�.  After a crash, the wal-index can (and should
** be) reconstructed���� from the original WAL file.  In fact, the VFS is required
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
** first index-block contains the database page number corresponding to��...��һ�� the
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
** table is never more than half full.  The expected number of collisions ��ͻ
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
**      iKey = (P * 383) % HASHTABLE_NSLOT     ����
**
** Then start scanning entries of the hash table, starting with iKey
** (wrapping around to the beginning when the end of the hash table is
** reached) until an unused hash slot is found. Let the first unused��δ�ù� slot
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
** comparisons�Ƚ� (on average) suffice to either locate a frame in the
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
**�������ع�ʱ ��kֵ��С����ϣ���н�ɾ�� ������kֵ��֡
*/
#ifndef SQLITE_OMIT_WAL

#include "wal.h"  

/*
** Trace output macros ���������
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
** returns SQLITE_CANTOPEN.  ����wal_max_version ��WALINDEX_MAX_VERSION ��ֵ
*/ 
/*
**���İ汾����Ψһ�İ汾��Ԥд��־��Ԥд��־�ĸ�ʽ����ͨ����ͨ���˰汾��SQLite�Ľ��͡�
**����ͻ�����ʼ����WAL�ļ������֣�a����У����ֵ����ȷ�ĺ�(b���ð汾�ֶβ�����ȷ�ġ�
**������Ľ��ΪWAL_MAX_VERSION�ָ�ʧ�ܺ�SQLite����SQLITE_CANTOPEN��
*/
#define WAL_MAX_VERSION      3007000


/*
** Indices of various locking bytes.   WAL_NREADER is the number
** of available reader locks and should be at least 3.������������ֽ�




*/
#define WAL_WRITE_LOCK         0
#define WAL_ALL_BUT_WRITE      1
#define WAL_CKPT_LOCK          1
#define WAL_RECOVER_LOCK       2
#define WAL_READ_LOCK(I)       (3+(I))
#define WAL_NREADER            (SQLITE_SHM_NLOCK-3)


/* Object declarations  �ṹ������*/
typedef struct WalIndexHdr WalIndexHdr;
typedef struct WalIterator WalIterator;
typedef struct WalCkptInfo WalCkptInfo;


/*
** The following object holds a copy of the wal-index header content.    �������ݰ��� Wal���� ͷ������
**
** The actual header in the wal-index consists of two copies of this  
** object. ʵ����wal����ͷ������������
**
** The szPage value can be any power of 2 between 512 and 32768, inclusive. szpage ��ֵ������512 ��32768 ֮�䣬������2�ı���
** Or it can be 1 to represent a 65536-byte page.  The latter case was ��������1����65536�ֽڵ�ҳ�� ��3.7.1�汾��ʼ֧�����Թ���
** added in 3.7.1 when support for 64K pages was added.  
*/
struct WalIndexHdr {
  u32 iVersion;               /* Wal-index version */          Wal-index�汾��Ϣ
  u32 unused;                 /* Unused (padding) field */     ���õĵط�     
  u32 iChange;                /* Counter incremented each transaction */��¼ÿ�����������
  u8 isInit;                  /* 1 when initialized */ ����ʼ��ʱ��  1
  u8 bigEndCksum;             /* True if checksums in WAL are big-endian */�����WAl���ܺͼ���Ƕ����ƵĻ���Ϊtrue
  u16 szPage;                 /* Database page size in bytes. 1==64K */ ���ݿ�ҳ�Ĵ�С����λΪbyte��1==64k
  u32 mxFrame;                /* Index of last valid frame in the WAL */д��WAl �����µ���Ч������ֵ
  u32 nPage;                  /* Size of database in pages */һ�����ݿ��ж��ٸ�ҳ
  u32 aFrameCksum[2];         /* Checksum of last frame in log */ �������д��log��
  u32 aSalt[2];               /* Two salt values copied from WAL header */��Wal header ���Ƶ���������ֵ
  u32 aCksum[2];              /* Checksum over all prior fields */�����ֶν���У��
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
  u32 nBackfill;                  /* Number of WAL frames backfilled into DB */  �ж��ٸ�Wal ����DB
  u32 aReadMark[WAL_NREADER];     /* Reader marks */������ı�־
};
#define READMARK_NOT_USED  0xffffffff


/* A block of WALINDEX_LOCK_RESERVED bytes beginning at
** WALINDEX_LOCK_OFFSET is reserved for locks. Since some systems
** only support mandatory file-locks, we do not read or write data
** from the region of the file on which locks are applied.
*/
/*WALINDEX_LOCK_RESERVED���ֽڿ���ʼ�ڱ���������WALINDEX_LOCK_OFFSET
** ������ĳЩϵͳֻ֧��ǿ���ļ��������ǲ����ȡ��д�����������ϵı������ļ�������.
*/

#define WALINDEX_LOCK_OFFSET   (sizeof(WalIndexHdr)*2 + sizeof(WalCkptInfo))
#define WALINDEX_LOCK_RESERVED 16
#define WALINDEX_HDR_SIZE      (WALINDEX_LOCK_OFFSET+WALINDEX_LOCK_RESERVED)

/* Size of header before each frame in wal */ wal��ÿһ��frame��ͷ���ݴ�С
#define WAL_FRAME_HDRSIZE 24

/* Size of write ahead log header, including checksum. */֮ǰд��־ͷ�Ĵ�С,����У��͡�
/* #define WAL_HDRSIZE 24 */                           ������־ͷ���ݺ�У��ֵ
#define WAL_HDRSIZE 32

/* WAL magic value. Either this value, or the same value with the least
** significant bit also set (WAL_MAGIC | 0x00000001) is stored in 32-bit
** big-endian format in the first 4 bytes of a WAL file.
**
** If the LSB is set, then the checksums for each frame within the WAL
** file are calculated by treating all data as an array of 32-bit 
** big-endian words. Otherwise, they are calculated by interpreting 
** all data as 32-bit little-endian words.WALħ��ֵ�����ֵ,��������ͬ��ֵ��Чλ����(WAL_MAGIC | 0 x00000001)�洢��32λ��˸�ʽWAL��ǰ4���ֽڵ��ļ���
**�������LSB,Ȼ�������ڵ�ÿһ֡��У����ļ��������е����ݼ�����һ��32λ�������˷��Ļ�������,�������ǵĽ�������������Ϊ32λ��λ���ȵĵ��ʡ�
*/
#define WAL_MAGIC 0x377f0682

/*
** Return the offset of frame iFrame in the write-ahead log file, 
** assuming a database page size of szPage bytes. The offset returned
** is to the start of the write-ahead log frame-header.
*/
/*
**������Ϊ�ֲ��Ŀ�ܣ���Ƕ�Ŀ������ǰ��д����־�ļ��С�
**�����¼���ݿ�ҳ���С���ֽ�������Ԥд��־�Ŀ��ͷ��
*/
#define walFrameOffset(iFrame, szPage) (                               \
  WAL_HDRSIZE + ((iFrame)-1)*(i64)((szPage)+WAL_FRAME_HDRSIZE)         \
)

/*
** An open write-ahead log file is represented��д by an instance of the
** following object.��־ͷ�ļ�
*/
struct Wal {
  sqlite3_vfs *pVfs;         /* The VFS used to create pDbFd */
  sqlite3_file *pDbFd;       /* File handle for the database file */
  sqlite3_file *pWalFd;      /* File handle for WAL file */
  u32 iCallback;             /* Value to pass to log callback (or 0) */���ݸ�������־ֵ�Ĵ�С
  i64 mxWalSize;             /* Truncate WAL to this size upon reset */���������趨��Ԥд��־�Ĵ�С���н�ȡ��
  int nWiData;               /* Size of array apWiData */��������apWiData�Ĵ�С
  int szFirstBlock;          /* Size of first block written to WAL file */����Ԥд��־�ļ��е�һ��д�Ŀ�Ĵ�С
  volatile u32 **apWiData;   /* Pointer to wal-index content in memory */**ָ���С
  u32 szPage;                /* Database page size */�������ݿ�ҳ��С
  i16 readLock;              /* Which read lock is being held.  -1 for none */���ֶ��������С�-1 ��ʾû��
  u8 syncFlags;              /* Flags to use to sync header writes */���������ͬ��Ҳ��д����
  u8 exclusiveMode;          /* Non-zero if connection is in exclusive mode */
  u8 writeLock;              /* True if in a write transaction */                �������һ��д����ʱ��ֵΪ��
  u8 ckptLock;               /* True if holding a checkpoint lock */   �����һ��checkpoint �� �� ֵΪ��
  u8 readOnly;               /* WAL_RDWR, WAL_RDONLY, or WAL_SHM_RDONLY */
  u8 truncateOnCommit;       /* True to truncate WAL file on commit */
  u8 syncHeader;             /* Fsync the WAL header if true */
  u8 padToSectorBoundary;    /* Pad transactions out to the next sector */
  WalIndexHdr hdr;           /* Wal-index header for current transaction */  ��ǰ���� Wal-index header
  const char *zWalName;      /* Name of WAL file */����Ԥд��־�ļ�������
  u32 nCkpt;                 /* Checkpoint sequence counter in the wal-header */wal-header�������м�����
#ifdef SQLITE_DEBUG
  u8 lockError;              /* True if a locking error has occurred */������һ����������ֵΪ��
#endif
};

/*
** Candidate values for Wal.exclusiveMode.
Wal.exclusiveMode �ĺ�ѡֵ

*/
#define WAL_NORMAL_MODE     0
#define WAL_EXCLUSIVE_MODE  1     
#define WAL_HEAPMEMORY_MODE 2

/*
** Possible values for WAL.readOnly  ���ܵ�ֵֻ������
*/
#define WAL_RDWR        0    /* Normal read/write connection */�����Ķ�д����ֵΪ0
#define WAL_RDONLY      1    /* The WAL file is readonly */��Ԥд��־�ļ���ֻ����ֵΪ1
#define WAL_SHM_RDONLY  2    /* The SHM file is readonly */�ù���洢�ļ���ֻ����ֵΪ2

/*
** Each page of the wal-index mapping contains a hash-table made up of ��/*wal-indexӳ���ÿ��ҳ�����һ����ϣ�����HASHTABLE_NSLOT����Ԫ�ص����͡�
** an array of HASHTABLE_NSLOT elements of the following type.  
*/
typedef u16 ht_slot;

/*
** This structure is used to implement an iterator that loops through
** all frames in the WAL in database page order. Where two or more frames
** correspond to the same database page, the iterator visits only the 
** frame most recently written to the WAL (in other words, the frame with
** the largest index).����ṹ������ʵ�ֵ���������WAL�����ݿ��е�����֡ҳ��˳���������������ϵ�֡�������Ӧ����ͬ�����ݿ�ҳ��,������ֻ����֡���д��WAL( ** ���仰˵,�������ָ��)
**
** The internals of this structure are only accessed by: ���ֽṹ���ڲ�ֻ�ܱ�һ�·�ʽ���ʡ�
**
**   walIteratorInit() - Create a new iterator, ��������
**   walIteratorNext() - Step an iterator,        Ȼ�� ������һ��
**   walIteratorFree() - Free an iterator.   ����ͷŵ���
**
** This functionality is used by the checkpoint code (see walCheckpoint()). �������������checkpoint 
*/
struct WalIterator {
  int iPrior;                     /* Last result returned from the iterator */ �����������ķ���ֵ
  int nSegment;                   /* Number of entries in aSegment[] */  aSegment����Ŀ��
    int iNext;                    /* Next slot in aIndex[] not yet returned */ aIndex����һ���±�
    ht_slot *aIndex;              /* i0, i1, i2... such that aPgno[iN] ascend */
    u32 *aPgno;                   /* Array of page numbers. */  ����ҳ��
    int nEntry;                   /* Nr. of entries in aPgno[] and aIndex[] */ aPgno������aIndex����
    int iZero;                    /* Frame number associated with aPgno[0] */ ֡����aPgno[]һ��
   aSegment[1];                  /* One for every 32KB page in the wal-index */ 32kb ��ҳ
};

/*
** Define the parameters of the hash tables in the wal-index file. There
** is a hash-table following every HASHTABLE_NPAGE page numbers in the
** wal-index.
** Changing any of these constants will alter the wal-index format and
** create incompatibilities.
*/
/*
**����wal-index �ļ��й�ϣ��Ĳ���
**wal-index�ļ�����ÿһ��HASHTABLE_NPAGE ҳ����涼��һ����ϣ��
**�ı��κ�һ����������ı�wal-index�ĸ�ʽ��ɲ�һ�¡�
*/
#define HASHTABLE_NPAGE      4096                 /* Must be power of 2 */HASHTABLE_NPAGE�Ĵ�С
#define HASHTABLE_HASH_1     383                  /* Should be prime */
#define HASHTABLE_NSLOT      (HASHTABLE_NPAGE*2)  /* Must be a power of 2 */

/* 
** The block of page numbers associated with the first hash-table in a
** wal-index is smaller than usual. This is so that there is a complete
** hash-table on each aligned 32KB page of the wal-index.
/*
**ҳ�����һ��ϣ����һ��������Ŀ���wal-indexƽ��С��
**�����ʹ���һ�������Ĺ�ϣ���ÿһ��ҳ���СΪ32KB��wal-index.��Ӧ��

*/
#define HASHTABLE_NPAGE_ONE  (HASHTABLE_NPAGE - (WALINDEX_HDR_SIZE/sizeof(u32)))

/* The wal-index is divided into pages of WALINDEX_PGSZ bytes each. */
/*wal-index��־����������ΪWALINDEX_PGSZ����¼ */
#define WALINDEX_PGSZ   (                                         \
    sizeof(ht_slot)*HASHTABLE_NSLOT + HASHTABLE_NPAGE*sizeof(u32) \
)

/*
** Obtain a pointer to the iPage'th page of the wal-index. The wal-index
** is broken into pages of WALINDEX_PGSZ bytes. Wal-index pages are
** numbered from zero. ��ȡ��־����i ҳ��ָ�룬��־�������ֽ� WALINDEX_PGSZ����־����ҳ��0��ʼ���
**
** If this call is successful, *ppPage is set to point to the wal-index
** page and SQLITE_OK is returned. If an error (an OOM or VFS error) occurs,
** then an SQLite error code is returned and *ppPage is set to 0.�������������óɹ���ppPage ������־����ҳ���ҷ���SQLITE_OK��
�������󣬷��� SQLite ������룬��ʱ pppage ���� 0
*/
static int walIndexPage(Wal *pWal, int iPage, volatile u32 **ppPage){
  int rc = SQLITE_OK;

  /* Enlarge the pWal->apWiData[] array if required */����pWal - > apWiData[]����
  if( pWal->nWiData<=iPage ){           //nWiDataΪָ���ڴ��С
    int nByte = sizeof(u32*)*(iPage+1); //�����i�������ֽ���
    volatile u32 **apNew; //����һ���µ�ָ��
    apNew = (volatile u32 **)sqlite3_realloc((void *)pWal->apWiData, nByte);// Ϊ�µ�ָ������ڴ�
    
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
  /*����ҳ�������ָ������VFS*/
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
** Return a pointer to the WalCkptInfo structure in the wal-index.����һ��WalCKptINfo�ṹָ��
*/
static volatile WalCkptInfo *walCkptInfo(Wal *pWal){
  assert( pWal->nWiData>0 && pWal->apWiData[0] );         //  assert c����  ����������������������ش�������ֹ����ִ��
  return (volatile WalCkptInfo*)&(pWal->apWiData[0][sizeof(WalIndexHdr)/2]);  ��
}

/*
** Return a pointer to the WalIndexHdr structure in the wal-index.   ����һ��WalIndexHdr �ṹָ��
*/
static volatile WalIndexHdr *walIndexHdr(Wal *pWal){
  assert( pWal->nWiData>0 && pWal->apWiData[0] );
  return (volatile WalIndexHdr*)pWal->apWiData[0];
}

/*
** The argument to this macro must be of type u32. On a little-endian ����ĺ����������32λ�ģ�
** architecture, it returns the u32 value that results from interpreting
** the 4 bytes as a big-endian value. On a big-endian architecture, it
** returns the value that would be produced by intepreting the 4 bytes
** of the input value as a little-endian integer.
*/
#define BYTESWAP32(x) ( \
    (((x)&0x000000FF)<<24) + (((x)&0x0000FF00)<<8)  \
  + (((x)&0x00FF0000)>>8)  + (((x)&0xFF000000)>>24) \
)

/*
** Generate or extend an 8 byte checksum based on the data in 
** array aByte[] and the initial values of aIn[0] and aIn[1] (or
** initial values of 0 and 0 if aIn==NULL).��һ��8λ�ֽڵ�У���ǻ�������aIn[0] and aIn[1]�ĳ�ʼֵ
**
** The checksum is written back into aOut[] before returning. У�����ڷ���֮ǰд����aOut[]
**
** nByte must be a positive multiple of 8.  nbyte ������8��������
*/
static void walChecksumBytes(
  int nativeCksum, /* True for native byte-order, false for non-native */
  u8 *a,           /* Content to be checksummed */ ����У�� ����
  int nByte,       /* Bytes of content in a[].  Must be a multiple of 8. */a[] �ж����ֽڣ�������8�ı���
  const u32 *aIn,  /* Initial checksum value input */   У���  �ĳ�ʼֵ
  u32 *aOut        /* OUT: Final checksum value output */ ��� У��ֵ�� ���
){
  u32 s1, s2;                               //���� s1,s2;
  u32 *aData = (u32 *)a;                   // �� *a ���� *aData
  u32 *aEnd = (u32 *)&a[nByte];             

  if( aIn ){                        //��� ain ��Ϊ��
    s1 = aIn[0];                        
    s2 = aIn[1];
  }else{                           ����
    s1 = s2 = 0;
  }

  assert( nByte>=8 );          //���nByteb������8Ϊ�٣�����ֹ���� 
  assert( (nByte&0x00000007)==0 );  //��� nByte ����8�ı��� ���������ֹ

  if( nativeCksum ){                      // ���nativeCksum Ϊ�棬��
    do {
      s1 += *aData++ + s2;
      s2 += *aData++ + s1;
    }while( aData<aEnd );
  }else{                                 // ����
    do {
      s1 += BYTESWAP32(aData[0]) + s2;
      s2 += BYTESWAP32(aData[1]) + s1;
      aData += 2;
    }while( aData<aEnd );
  }

  aOut[0] = s1;            //��s1��ֵ��aOut[0] 
  aOut[1] = s2;            //��s2��ֵ��aout[1] 
}

static void walShmBarrier(Wal *pWal){ 
  if( pWal->exclusiveMode!=WAL_HEAPMEMORY_MODE ){     //���pWal->exclusiveMode ������2
    sqlite3OsShmBarrier(pWal->pDbFd);
  }
}

/*
** Write the header information in pWal->hdr into the wal-index.��pWal->hdr�еı�����Ϣд��wal-index
**
** The checksum on pWal->hdr is updated before it is written. pWal ->hdr ��У����ڱ�д֮ǰҪ����
*/
static void walIndexWriteHdr(Wal *pWal){
  volatile WalIndexHdr *aHdr = walIndexHdr(pWal);        // ����һ��WalIndexHdr �ṹָ�� 
  const int nCksum = offsetof(WalIndexHdr, aCksum);              

  assert( pWal->writeLock );                        // �����Ϊ�� �������ֹ                          
  pWal->hdr.isInit = 1;                              //��ʼֵΪ1
  pWal->hdr.iVersion = WALINDEX_MAX_VERSION;          //���ð汾�� ΪWALINDEX_MAX_VERSION
  walChecksumBytes(1, (u8*)&pWal->hdr, nCksum, 0, pWal->hdr.aCksum);  //����У��
  memcpy((void *)&aHdr[1], (void *)&pWal->hdr, sizeof(WalIndexHdr)); //memcpy�����Ĺ����Ǵ�Դsrc��ָ���ڴ��ַ����ʼλ�ÿ�ʼ����n���ֽڵ�Ŀ��dest��ָ���ڴ��ַ����ʼλ���С�
  walShmBarrier(pWal);              ����  walShmBarrier��������
  memcpy((void *)&aHdr[0], (void *)&pWal->hdr, sizeof(WalIndexHdr));
}

/*
** This function encodes a single frame header and writes it to a buffer
** supplied by the caller. A frame-header is made up of a series of 4-byte big-endian integers, as follows: 
** ��������Ǳ��뵥һ֡ͷ������д�뵽�ɵ������ṩ�Ļ����������������
**     0: Page number.
**     4: For commit records, the size of the database image in pages 
**        after the commit. For all other records, zero.
**     8: Salt-1 (copied from the wal-header)
**    12: Salt-2 (copied from the wal-header)
**    16: Checksum-1.
**    20: Checksum-2.
*/
static void walEncodeFrame(
  Wal *pWal,                      /* The write-ahead log */  Ԥд��־
  u32 iPage,                      /* Database page number for frame */  ĳһ֡�����ݿ�������һҳ
  u32 nTruncate,                  /* New db size (or 0 for non-commit frames) */ ��db ��С
  u8 *aData,                      /* Pointer to page data */  ָ�� ҳ���ݵ�ָ��
  u8 *aFrame                      /* OUT: Write encoded frame here */
){
  int nativeCksum;                /* True for native byte-order checksums */ 
  u32 *aCksum = pWal->hdr.aFrameCksum;  
  assert( WAL_FRAME_HDRSIZE==24 );           //���Ϊ�٣�����ֹ����
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
** Check to see if the frame with header in aFrame[] and content���aFrame������adata[] �Ƿ���ȷ�������ȷ����д*piPage  pnTruncate
** in aData[] is valid.  If it is a valid frame, fill *piPage and����ָ��return true
** *pnTruncate and return true.  Return if the frame is not valid.
*/
static int walDecodeFrame(
  Wal *pWal,                      /* The write-ahead log */   
  u32 *piPage,                    /* OUT: Database page number for frame */  �������ݿ�ҳ��
  u32 *pnTruncate,                /* OUT: New db size (or 0 if not commit) */��db��С
  u8 *aData,                      /* Pointer to page data (for checksum) */ ָ��ҳ���ݵ�ָ��
  u8 *aFrame                      /* Frame data */ �������
){
  int nativeCksum;                /* True for native byte-order checksums */   ���ֵ
  u32 *aCksum = pWal->hdr.aFrameCksum;
  u32 pgno;                       /* Page number of the frame */ �������ݿ��ҳ��
  assert( WAL_FRAME_HDRSIZE==24 );     ���Ϊ�٣�����ֹ����

  /* A frame is only valid if the salt values in the frame-header
  ** match the salt values in the wal-header. 
  */
  if( memcmp(&pWal->hdr.aSalt, &aFrame[8], 8)!=0 ){  �����ƥ���� 
    return 0;
  }

  /* A frame is only valid if the page number is creater than zero.ֻ����ҳ��������0ʱ֡������Ч��
  */
  pgno = sqlite3Get4byte(&aFrame[0]);  Ϊpgno��ֵ
  if( pgno==0 ){  
    return 0;
  }

  /* A frame is only valid if a checksum of the WAL header,
  ** all prior frams, the first 16 bytes of this frame-header,  ������ǰ16�ͺ�8���ֽڴ������Ϣ
  ** and the frame-data matches the checksum in the last 8 
  ** bytes of this frame-header.
  */
  nativeCksum = (pWal->hdr.bigEndCksum==SQLITE_BIGENDIAN);    ������ 
  walChecksumBytes(nativeCksum, aFrame, 8, aCksum, aCksum);
  walChecksumBytes(nativeCksum, aData, pWal->szPage, aCksum, aCksum);
  if( aCksum[0]!=sqlite3Get4byte(&aFrame[16]) 
   || aCksum[1]!=sqlite3Get4byte(&aFrame[20]) 
  ){
    /* Checksum failed. */
    return 0;
  }

  /* If we reach this point, the frame is valid.  Return the page number
  ** and the new database size.��� ֡����Ч�ģ�����ҳ�����µ����ݿ��С
  */
  *piPage = pgno;
  *pnTruncate = sqlite3Get4byte(&aFrame[4]);
  return 1;
}
      

#if defined(SQLITE_TEST) && defined(SQLITE_DEBUG)
/*
** Names of locks.  This routine is used to provide debugging output and is not
** a part of an ordinary build.      ��ȡWal��������  ��ͨ��ͨ������Ĳ��� lockIdx ��ֵ���бȽ� ��������
*/
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
** Set or release locks on the WAL.  Locks are either shared or exclusive.���û��ͷ�һ��������������һ��������ų���
** A lock cannot be moved directly between shared and exclusive - it must goһ��������ֱ���ڹ���ͺ���״̬֮���ƶ��� ����������״̬
** through the unlocked state first.
**
** In locking_mode=EXCLUSIVE, all of these routines become no-ops.
*/
static int walLockShared(Wal *pWal, int lockIdx){    //   �ӹ�����
  int rc;                                                   //����ֵ
  if( pWal->exclusiveMode ) return SQLITE_OK;                // ���Wal�ڻ���ģʽ�� ���򷵻� ��
  rc = sqlite3OsShmLock(pWal->pDbFd, lockIdx, 1,
                        SQLITE_SHM_LOCK | SQLITE_SHM_SHARED);
  WALTRACE(("WAL%p: acquire SHARED-%s %s\n", pWal,
            walLockName(lockIdx), rc ? "failed" : "ok"));
  VVA_ONLY( pWal->lockError = (u8)(rc!=SQLITE_OK && rc!=SQLITE_BUSY); )
  return rc;
}
static void walUnlockShared(Wal *pWal, int lockIdx){             //�ͷŹ�����         
  if( pWal->exclusiveMode ) return;
  (void)sqlite3OsShmLock(pWal->pDbFd, lockIdx, 1,
                         SQLITE_SHM_UNLOCK | SQLITE_SHM_SHARED);
  WALTRACE(("WAL%p: release SHARED-%s\n", pWal, walLockName(lockIdx)));
}
static int walLockExclusive(Wal *pWal, int lockIdx, int n){        // ��������
  int rc;  
  if( pWal->exclusiveMode ) return SQLITE_OK;
  rc = sqlite3OsShmLock(pWal->pDbFd, lockIdx, n,
                        SQLITE_SHM_LOCK | SQLITE_SHM_EXCLUSIVE);
  WALTRACE(("WAL%p: acquire EXCLUSIVE-%s cnt=%d %s\n", pWal,
            walLockName(lockIdx), n, rc ? "failed" : "ok"));
  VVA_ONLY( pWal->lockError = (u8)(rc!=SQLITE_OK && rc!=SQLITE_BUSY); )
  return rc;
}
static void walUnlockExclusive(Wal *pWal, int lockIdx, int n){        //�ͷ�������
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
static int walHash(u32 iPage){         һ����ϣֵ�� ��Ӧ����һҳ ��ֵ
  assert( iPage>0 );                    ���IPage>0 Ϊ�٣���ֹ����
  assert( (HASHTABLE_NSLOT & (HASHTABLE_NSLOT-1))==0 ); ���������0����ֹ����
  return (iPage*HASHTABLE_HASH_1) & (HASHTABLE_NSLOT-1);����ҳ��Ӧ�Ĺ�ϣֵ
}
static int walNextHash(int iPriorHash){   ���������ײ��
  return (iPriorHash+1)&(HASHTABLE_NSLOT-1);
}

/* 
** Return pointers to the hash table and page number array stored on      ���ش洢�ڹ�ϣ���ָ���ҳ������ҳ��iHash wal-index
** page iHash of the wal-index. The wal-index is broken into 32KB pages     wal-index��Ϊ32 kb��ҳ�桡��Ŵ�0��ʼ
** numbered starting from 0. 
**
** Set output variable *paHash to point to the start of the hash table   ��wal-index�ļ��������������* paHash��ϣ��Ŀ�ʼ
** in the wal-index file. Set *piZero to one less than the frame        piZero����Ϊһ��С�ڵ�һ֡�������ϣ������
** number of the first frame indexed by this hash table. If a            �����һ�����ڹ�ϣ��������ΪN,��ָ����֡��(* piZero + N)����־�С�
** slot in the hash table is set to N, it refers to frame number 
** (*piZero+N) in the log.
**
** Finally, set *paPgno so that *paPgno[1] is the page number of the      ���,����* paPgnoʹ* paPgno[1]��ҳ���һ֡�����Ĺ�ϣ��,֡(* piZero + 1��
** first frame indexed by the hash table, frame (*piZero+1).
*/
static int walHashGet(                �����ļ���iҳ��ָ��
  Wal *pWal,                      /* WAL handle */    Wal�ļ�
  int iHash,                      /* Find the iHash'th table */�ҵ���ϣֵ��Ӧ�ı�
  volatile ht_slot **paHash,      /* OUT: Pointer to hash index */ hash������ָ��
  volatile u32 **paPgno,          /* OUT: Pointer to page number array */ҳ�������ָ��
  u32 *piZero                     /* OUT: Frame associated with *paPgno[0] */Ϊ*paPgno[0] ����һ��ָ��
){
  int rc;                         /* Return code */  ������
  volatile u32 *aPgno;              

  rc = walIndexPage(pWal, iHash, &aPgno);  //��ȡ��־�ļ���iҳ��ָ��
  assert( rc==SQLITE_OK || iHash>0 );	 //�ж��Ƿ�ɹ������ɹ�����ֹ����	

  if( rc==SQLITE_OK ){                    // �����ȡ�ɹ�
    u32 iZero;                          // ���� U32 �ı���     
    volatile ht_slot *aHash;             //����һ�� ht_slot�� ָ�����

    aHash = (volatile ht_slot *)&aPgno[HASHTABLE_NPAGE];   HASHTABLE_NPAGEΪ4096 ����aHash��ֵ
    if( iHash==0 ){                                           ��Ihash ֵΪ 0 ʱ
      aPgno = &aPgno[WALINDEX_HDR_SIZE/sizeof(u32)];         aPgno��ֵ��ʽ
      iZero = 0;                                              IZero Ϊ0
    }else{                                                     iHash ��Ϊ0
      iZero = HASHTABLE_NPAGE_ONE + (iHash-1)*HASHTABLE_NPAGE; IZero �ĸ�ֵ��ʽ
    }
  
    *paPgno = &aPgno[-1];  //ΪpaPgno ��ֵ
    *paHash = aHash;      // ΪPaHash��ֵ   
    *piZero = iZero;       //ΪPiZero ��ֵ
  }
  return rc;            // ���� rc
}

/*
** Return the number of the wal-index page that contains the hash-table
** and page-number array that contain entries corresponding to WAL frame
** iFrame. The wal-index is broken up into 32KB pages. Wal-index pages 
** are numbered starting from 0.   ���ص�wal-indexҳ�������ϣ���ҳ�����������Ŀ��Ӧ��WAL���iFrame��wal-index��Ϊ32 kb��ҳ�档Wal-indexҳ���0��ʼ��š�
*/
static int walFramePage(u32 iFrame){
  int iHash = (iFrame+HASHTABLE_NPAGE-HASHTABLE_NPAGE_ONE-1) / HASHTABLE_NPAGE; ���� IHash��ֵ
  assert( (iHash==0 || iFrame>HASHTABLE_NPAGE_ONE)     �ж��Ƿ���ֹ����
       && (iHash>=1 || iFrame<=HASHTABLE_NPAGE_ONE)
       && (iHash<=1 || iFrame>(HASHTABLE_NPAGE_ONE+HASHTABLE_NPAGE))
       && (iHash>=2 || iFrame<=HASHTABLE_NPAGE_ONE+HASHTABLE_NPAGE)
       && (iHash<=2 || iFrame>(HASHTABLE_NPAGE_ONE+2*HASHTABLE_NPAGE))
  );
  return iHash;    ���� Ihash
}

/*
** Return the page number associated with frame iFrame in this WAL. ������IFrame��Ӧ��ҳ����
*/
static u32 walFramePgno(Wal *pWal, u32 iFrame){
  int iHash = walFramePage(iFrame); // ����walFramePage������ȡ��Iframe��Ӧ�� ����ҳ�ڵڼ�ҳ
  if( iHash==0 ){          // ���IhashΪ0
    return pWal->apWiData[0][WALINDEX_HDR_SIZE/sizeof(u32) + iFrame - 1];  //���� ��iFrame����Wal�е�ҳ
  }
  return pWal->apWiData[iHash][(iFrame-1-HASHTABLE_NPAGE_ONE)%HASHTABLE_NPAGE];//���� ��iFrame����Wal�е�ҳ
}

/*
** Remove entries from the hash table that point to WAL slots greater �ӹ�ϣ����ɾ����pWal->hdr.mxFrame�������Ӧ��Ŀ
** than pWal->hdr.mxFrame.
**
** This function is called whenever pWal->hdr.mxFrame is decreased due �������������ʱpWal - > hdr��mxFrame�½������ڻع��򱣴��
** to a rollback or savepoint.
**
** At most only the hash table containing pWal->hdr.mxFrame needs to be
** updated.  Any later hash tables will be automatically cleared when
** pWal->hdr.mxFrame advances to the point where those hash tables are
** actually needed.���ֻ����pWal - > hdr.mxFrame��Ҫ���¡��κκ�����ϣ��ʱ���Զ���� pWal - > hdr.mxFrame����,��Щ��ϣ��ʵ����Ҫ��
*/
static void walCleanupHash(Wal *pWal){
  volatile ht_slot *aHash = 0;    /* Pointer to hash table to clear */ָ��Ҫɾ����ָ��
  volatile u32 *aPgno = 0;        /* Page number array for hash table */ҳ��Ϊ��ϣ������
  u32 iZero = 0;                  /* frame == (aHash[x]+iZero) */
  int iLimit = 0;                 /* Zero values greater than this */ �������ֵ
  int nByte;                      /* Number of bytes to zero in aPgno[] */
  int i;                          /* Used to iterate through aHash[] */ ��������ѭ��

  assert( pWal->writeLock ); �ж�Wal�Ƿ���д�����У��ڵĻ���ֹ����
  testcase( pWal->hdr.mxFrame==HASHTABLE_NPAGE_ONE-1 );����testcase�������� ��������
  testcase( pWal->hdr.mxFrame==HASHTABLE_NPAGE_ONE );����testcase�������� ��������
  testcase( pWal->hdr.mxFrame==HASHTABLE_NPAGE_ONE+1 );����testcase�������� ��������

  if( pWal->hdr.mxFrame==0 ) return;   �������ֵΪ0 ���򷵻ؿ�

  /* Obtain pointers to the hash-table and page-number array containing 
  ** the entry that corresponds to frame pWal->hdr.mxFrame. It is guaranteed
  ** that the page said hash-table and array reside on is already mapped.
  **��ȡ������ϣ���ҳ���ָ���������Ŀ��Ӧ֡pWal - > hdr.mxFrame��
  **���Ǳ�֤ҳ��������ϣ�������פ�����Ѿ�ӳ�䡣
  */
  assert( pWal->nWiData>walFramePage(pWal->hdr.mxFrame) ); �ж��Ƿ���ֹ����
  assert( pWal->apWiData[walFramePage(pWal->hdr.mxFrame)] );�ж��Ƿ���ֹ����
  walHashGet(pWal, walFramePage(pWal->hdr.mxFrame), &aHash, &aPgno, &iZero);

  /* Zero all hash-table entries that correspond to frame numbers greater
  ** than pWal->hdr.mxFrame.
  */
  iLimit = pWal->hdr.mxFrame - iZero; ��ȡilimit��ֵ
  assert( iLimit>0 );            ��� ilimit С��0 �������ֹ
  for(i=0; i<HASHTABLE_NSLOT; i++){ ��aHash���б�����
    if( aHash[i]>iLimit ){            ���hashֵ�������� ��
      aHash[i] = 0;                       ���� ��ֵΪ0
    }
  }
  
  /* Zero the entries in the aPgno array that correspond to frames with
  ** frame numbers greater than pWal->hdr.mxFrame. 
  */
  nByte = (int)((char *)aHash - (char *)&aPgno[iLimit+1]); ��ȡ nByte ��ֵ
  memset((void *)&aPgno[iLimit+1], 0, nByte);          Ϊ aPgno�����ڴ�

#ifdef SQLITE_ENABLE_EXPENSIVE_ASSERT
  /* Verify that the every entry in the mapping region is still reachable
  ** via the hash table even after the cleanup. ȷ��ÿһ��ӳ������ ����ͨ��ӳ��ﵽ
  */
  if( iLimit ){                ��� Ilimit ��Ϊ0
    int i;           /* Loop counter */ ѭ������
    int iKey;        /* Hash key */    ��ϣ��ֵ
    for(i=1; i<=iLimit; i++){              ѭ��
      for(iKey=walHash(aPgno[i]); aHash[iKey]; iKey=walNextHash(iKey)){ ��ȡIKey��ֵ���ж�aHash�Ƿ�Ϊ�գ���ȡ��һ��hashֵ
        if( aHash[iKey]==i ) break;  ���aHash��ֵ���±���ͬ ������ѭ��
      }
      assert( aHash[iKey]==i );       ���aHash��ֵ���±겻��ͬ������ֹ����
    }
  }
#endif /* SQLITE_ENABLE_EXPENSIVE_ASSERT */
}


/*
** Set an entry in the wal-index that will map database page number
** pPage into WAL frame iFrame.       ��Wal-index����һ�������Ϊ ���Խ� ����ҳ��ӳ�䵽 Wal֡�еĵ�iFrame
*/
static int walIndexAppend(Wal *pWal, u32 iFrame, u32 iPage){
  int rc;                         /* Return code */ ������
  u32 iZero = 0;                  /* One less than frame number of aPgno[1] */ С��aPgno��1����֡��
  volatile u32 *aPgno = 0;        /* Page number array */ ���� ����ҳ��
  volatile ht_slot *aHash = 0;    /* Hash table */    ��ϣ��

  rc = walHashGet(pWal, walFramePage(iFrame), &aHash, &aPgno, &iZero);

  /* Assuming the wal-index file was successfully mapped, populate the
  ** page number array and hash table entry.
  */
  if( rc==SQLITE_OK ){                   ������ú����ɹ� 
    int iKey;                     /* Hash table key */  ��ϣ��
    int idx;                      /* Value to write to hash-table slot */ д��hash�۵�ֵ
    int nCollide;                 /* Number of hash collisions */ ��ϣ��ײ��Ŀ

    idx = iFrame - iZero;       �� idx��ֵ                   
    assert( idx <= HASHTABLE_NSLOT/2 + 1 );  ��ֹ����
    
    /* If this is the first entry to be added to this hash-table, zero the
    ** entire hash table and aPgno[] array before proceding. 
    */
    /*
    **�������Ҫ����ӵ��ù�ϣ��ĵ�һ����Ŀ
    **����������ϣ���aPgno[]��֮ǰ
    */
    if( idx==1 ){ 
      int nByte = (int)((u8 *)&aHash[HASHTABLE_NSLOT] - (u8 *)&aPgno[1]);
      memset((void*)&aPgno[1], 0, nByte);    ΪaPgno�����ڴ棬����ʼ��Ϊ0
    }

    /* If the entry in aPgno[] is already set, then the previous writer
    ** must have exited unexpectedly in the middle of a transaction (after
    ** writing one or more dirty pages to the WAL to free up memory). 
    ** Remove the remnants of that writers uncommitted transaction from 
    ** the hash-table before writing any new entries.
    **�����aPgno[]����Ŀ�Ѿ����ã�����ǰ��writerһ����������������������˳�
    **(ͨ��дһ�����߶����ҳWAL���ͷ��ڴ�)
    **�ڱ�д�κ��µ���Ŀ֮ǰ��Ҫ�ȴӹ�ϣ����ɾ��writersδ�ύ�Ĳ�������
    */
    if( aPgno[idx] ){            aPgno[idx] ��Ϊ0
      walCleanupHash(pWal);
      assert( !aPgno[idx] );         ��ֹ����
    }

    /* Write the aPgno[] array entry and the hash-table slot. */ 
    nCollide = idx;       ΪnCollide ��ֵ
    for(iKey=walHash(iPage); aHash[iKey]; iKey=walNextHash(iKey)){ ��ȡikeyֵ���ж�aHash 
      if( (nCollide--)==0 ) return SQLITE_CORRUPT_BKPT;  �����ײ��Ϊ0 �򷵻� SQLITE_CORRUPT_BKPT
    }
    aPgno[idx] = iPage;        Ϊapgno[] ��ֵ
    aHash[iKey] = (ht_slot)idx;  ��ikey��hashֵΪidx

#ifdef SQLITE_ENABLE_EXPENSIVE_ASSERT
    /* Verify that the number of entries in the hash table exactly equals
    ** the number of entries in the mapping region. ȷ�� hash �����ں� ӳ���������ڵ���Ŀ��ͬ
    {
      int i;           /* Loop counter */ ѭ������
      int nEntry = 0;  /* Number of entries in the hash table */ �����ĿΪ0
      for(i=0; i<HASHTABLE_NSLOT; i++){ if( aHash[i] ) nEntry++; } ����ѭ��
      assert( nEntry==idx );    ��ֹ����
    }

    /* Verify that the every entry in the mapping region is reachable
    ** via the hash table.  This turns out to be a really, really expensive
    ** thing to check, so only do this occasionally - not on every
    ** iteration.����֤ÿ����Ŀӳ�������ǿɻ�õ�ͨ����ϣ���ⱻ֤����һ���ǳ��ǳ���Ҫ�ļ��,����ֻ��ż��������������������ÿһ��������
    */
    if( (idx&0x3ff)==0 ){  
      int i;           /* Loop counter */ ѭ������
      for(i=1; i<=idx; i++){    ��idex���б���
        for(iKey=walHash(aPgno[i]); aHash[iKey]; iKey=walNextHash(iKey)){
          if( aHash[iKey]==i ) break;
        }
        assert( aHash[iKey]==i ); ��ֹ����
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
** the necessary locks, this routine returns SQLITE_BUSY.��
**ͨ���Ķ�дǰ��־�ļ��ָ�wal- wal-index
**����������ȳ�����wal-index����һ����������ֹ�����߳�ȥ������������WAL��wal-index�ڻָ�����ʱ
**������޷�������Ҫ����,������̷���SQLITE_BUSY��
*/
static int walIndexRecover(Wal *pWal){  �����Ƿ�������лָ������ɹ����� SQLite����busy
  int rc;                         /* Return Code */  ����ֵ
  i64 nSize;                      /* Size of log file */  log file�Ĵ�С
  u32 aFrameCksum[2] = {0, 0};           ���� aFrameCksum ����
  int iLock;                      /* Lock offset to lock for checkpoint */ ������
  int nLock;                      /* Number of locks to hold */ �ڼ�����

  /* Obtain an exclusive lock on all byte in the locking range not already
  ** locked by the caller. The caller is guaranteed to have locked the
  ** WAL_WRITE_LOCK byte, and may have also locked the WAL_CKPT_LOCK byte.
  ** If successful, the same bytes that are locked here are unlocked before
  ** this function returns./ * ��ȡ�������������ֽ��ڲ������������������ķ�Χ�� 
  **��������������֤������WAL_WRITE_LOCK�ֽ�,Ҳ����Ҳ����WAL_CKPT_LOCK���ֽ�
  **����ɹ�,��֮ǰû�б��������ֽ�Ҳ��������
  */
  assert( pWal->ckptLock==1 || pWal->ckptLock==0 );��������ͷ���ֵΪ1����0 ������ֹ
  assert( WAL_ALL_BUT_WRITE==WAL_WRITE_LOCK+1 );
  assert( WAL_CKPT_LOCK==WAL_ALL_BUT_WRITE );
  assert( pWal->writeLock ); ���Wal��д�����£�����ֹ����
  iLock = WAL_ALL_BUT_WRITE + pWal->ckptLock; wal_all_but_write Ϊ1
  
  nLock = SQLITE_SHM_NLOCK - iLock;
  
  rc = walLockExclusive(pWal, iLock, nLock); �Ƿ��ȡ������
  
  if( rc ){  �����ȡ�ɹ���
    return rc; ���� rc
  }
  WALTRACE(("WAL%p: recovery begin...\n", pWal));

  memset(&pWal->hdr, 0, sizeof(WalIndexHdr));  ΪpWal->hdr ����ռ� ����ʼ��Ϊ0

  rc = sqlite3OsFileSize(pWal->pWalFd, &nSize); Wal�ļ��Ĵ�С����ȡ����ֵ
  if( rc!=SQLITE_OK ){   �����ȡ���ɹ�
    goto recovery_error; ��ת�� recovery_error
  }

  if( nSize>WAL_HDRSIZE ){            nSize Ϊ32
    u8 aBuf[WAL_HDRSIZE];         /* Buffer to load WAL header into */ ��ȡWalͷ����
    u8 *aFrame = 0;               /* Malloc'd buffer to load entire frame */
    int szFrame;                  /* Number of bytes in buffer aFrame[] */
    u8 *aData;                    /* Pointer to data part of aFrame buffer */
    int iFrame;                   /* Index of last frame read */
    i64 iOffset;                  /* Next offset to read from log file */
    int szPage;                   /* Page size according to the log */������־ҳ���С
    u32 magic;                    /* Magic value read from WAL header */
    u32 version;                  /* Magic value read from WAL header */
    int isValid;                  /* True if this frame is valid */

    /* Read in the WAL header. */
    rc = sqlite3OsRead(pWal->pWalFd, aBuf, WAL_HDRSIZE, 0); ��ȡWalͷ����
    if( rc!=SQLITE_OK ){ ��� ���ɹ� 
      goto recovery_error; ��ת  recovery_error
    }

    /* If the database page size is not a power of two, or is greater than
    ** SQLITE_MAX_PAGE_SIZE, conclude that the WAL file contains no valid 
    ** data. Similarly, if the 'magic' value is invalid, ignore the whole
    ** WAL file.������ݿ�ҳ���С����һ������,���ߴ���SQLITE_MAX_PAGE_SIZE,�����֪��WAL�ļ���������Ч�����ݡ�
    **���Ƶ�,�����ħ����ֵ����Ч��,����������WAL�ļ���
    */
    magic = sqlite3Get4byte(&aBuf[0]);��ȡ����
    szPage = sqlite3Get4byte(&aBuf[8]);��ȡ���ݴ�С
    if( (magic&0xFFFFFFFE)!=WAL_MAGIC                 WAL_MAGIC 0x377f0682
     || szPage&(szPage-1)          �� szpage�Ƿ�Ϊ2�ı���
     || szPage>SQLITE_MAX_PAGE_SIZE       SQLITE_MAX_PAGE_SIZE 65536
     || szPage<512                         
    ){
      goto finished; ��ת�� finished
    }
    pWal->hdr.bigEndCksum = (u8)(magic&0x00000001); ��ȡУ��ֵ
    pWal->szPage = szPage;   ��pWal->szpage ��ֵ 
    pWal->nCkpt = sqlite3Get4byte(&aBuf[12]);  wal-header�������м����� 
    memcpy(&pWal->hdr.aSalt, &aBuf[16], 8); ��aBuf�����ݵ�8λ����pWal->hdr.aSalt

    /* Verify that the WAL header checksum is correct */ ��ʵWal��ͷ���ݵļ����Ƿ�����ȷ��
    walChecksumBytes(pWal->hdr.bigEndCksum==SQLITE_BIGENDIAN, ����walchecksumBytes����Wal header�ļ���
        aBuf, WAL_HDRSIZE-2*4, 0, pWal->hdr.aFrameCksum
    );
    if( pWal->hdr.aFrameCksum[0]!=sqlite3Get4byte(&aBuf[24])  // ����������� ABuf[] ����ͬ
     || pWal->hdr.aFrameCksum[1]!=sqlite3Get4byte(&aBuf[28])
    ){
      goto finished;                   //  ��ת�� finished
    }

    /* Verify that the version number on the WAL format is one that
    ** are able to understand */                 ��֤WAL��ʽ�İ汾�����ܹ�����
    version = sqlite3Get4byte(&aBuf[4]); ��ȡWalͷ������version����
    if( version!=WAL_MAX_VERSION ){       �����ȡ����version���� ������WAL_MAX_VERSION
      rc = SQLITE_CANTOPEN_BKPT;                ͨ��rc��ȡֵ
      goto finished;                   ��ת�� finished
    }

    /* Malloc a buffer to read frames into. */ ����һ�������� �� frames����
    szFrame = szPage + WAL_FRAME_HDRSIZE;       WAL_FRAME_HDRSIZE 24  �� һҳ�Ĵ�С
    aFrame = (u8 *)sqlite3_malloc(szFrame);    Ϊ aFrame���� �ڴ� 
    if( !aFrame ){             ������䲻�ɹ�
      rc = SQLITE_NOMEM;       Ϊ rc ��ֵ
      goto recovery_error;    ��ת�� recovery_error
    }
    aData = &aFrame[WAL_FRAME_HDRSIZE]; ��aFrame �ĵ�ַ���� aData

    /* Read all frames from the log file. */ ����־�ļ��ж������е�frame
    iFrame = 0;
    for(iOffset=WAL_HDRSIZE; (iOffset+szFrame)<=nSize; iOffset+=szFrame){ WAL_HDRSIZE 32  ���iOffset+szFrameС����־�ļ���С  
      u32 pgno;                   /* Database page number for frame */   ����� ĳ����ҳ��
      u32 nTruncate;              /* dbsize field from frame header */ ֡ͷ�����ݿ��С 

      /* Read and decode the next log frame. */  ����ͽ�����־֡
      iFrame++;                                   iFrame �Լ�
      rc = sqlite3OsRead(pWal->pWalFd, aFrame, szFrame, iOffset); ���� SQLite3Osread���� 
      if( rc!=SQLITE_OK ) break;   ��� rc ������ SQLIte����ok ������ѭ��
      isValid = walDecodeFrame(pWal, &pgno, &nTruncate, aData, aFrame);  ��ȡ���� ������һֵ ���Ƿ�ɹ�
      if( !isValid ) break;         ������ɹ� ��������ѭ��
      rc = walIndexAppend(pWal, iFrame, pgno);
      if( rc!=SQLITE_OK ) break;

      /* If nTruncate is non-zero, this is a commit record. */ ���nTruncate�Ƿǿյģ������Ϊһ���ύ��¼
      if( nTruncate ){                    �ж��Ƿ�Ϊ��
        pWal->hdr.mxFrame = iFrame;   Ϊ���µ���Ч��������ֵ
        pWal->hdr.nPage = nTruncate;  Wal�ļ��ж���ҳ 
        pWal->hdr.szPage = (u16)((szPage&0xff00) | (szPage>>16)); һҳ�ж��
        testcase( szPage<=32768 );   ���ò���ʾ��
        testcase( szPage>=65536 );
        aFrameCksum[0] = pWal->hdr.aFrameCksum[0];��hdr�ļ���͵�ֵ����aFrameCksum[0]
        aFrameCksum[1] = pWal->hdr.aFrameCksum[1];��hdr�ļ���͵�ֵ����aFrameCksum[0]
      }
    }

    sqlite3_free(aFrame); �ͷ�ָ��
  }

finished:                      goto �ı��
  if( rc==SQLITE_OK ){                  r��� rc Ϊ Sqlite����ok
    volatile WalCkptInfo *pInfo;            ����У����Ϣָ�����  pInfo
    int i;                                  ����i
    pWal->hdr.aFrameCksum[0] = aFrameCksum[0]; ��hdr�ļ���͵�ֵ����aFrameCksum[0]
    pWal->hdr.aFrameCksum[1] = aFrameCksum[1];��hdr�ļ���͵�ֵ����aFrameCksum[0]
    walIndexWriteHdr(pWal); ���ú��� 

    /* Reset the checkpoint-header. This is safe because this thread is 
    ** currently holding locks that exclude all other readers, writers and
    ** checkpointers. ���� checkpoint-header ������̻߳��������
    */
    pInfo = walCkptInfo(pWal); ��ȡcheckpoint��Ϣ
    pInfo->nBackfill = 0;    ��nBackfill ��ֵΪ0
    pInfo->aReadMark[0] = 0;��aReadMark��ֵΪ0
    for(i=1; i<WAL_NREADER; i++) pInfo->aReadMark[i] = READMARK_NOT_USED; ΪaReadMar[]��ֵREADMARK_NOT_USED
    if( pWal->hdr.mxFrame ) pInfo->aReadMark[1] = pWal->hdr.mxFrame; ���MxFrame��Ч ��

    /* If more than one frame was recovered from the log file, report an
    ** event via sqlite3_log(). This is to help with identifying performance
    ** problems caused by applications routinely shutting down without
    ** checkpointing the log file. �����ֹһ����ܴ���־�ļ��лָ�������ͨ��sqlite3_log()����һ���¼���
    **�⽫����ʶ�������û�м��ָʾ��־�ļ��������Ӧ�ó��򾭳��رյ��������⡣
    **
    */
    if( pWal->hdr.nPage ){            
      sqlite3_log(SQLITE_OK, "Recovered %d frames from WAL file %s",
          pWal->hdr.nPage, pWal->zWalName
      );
    }
  }

recovery_error:   goto ���
  WALTRACE(("WAL%p: recovery %s\n", pWal, rc ? "failed" : "ok"));
  walUnlockExclusive(pWal, iLock, nLock); ���ý�������
  return rc;       ���� rc
}

/*
** Close an open wal-index. �ر� ������ wal-index
*/
static void walIndexClose(Wal *pWal, int isDelete){ 
  if( pWal->exclusiveMode==WAL_HEAPMEMORY_MODE ){    
    int i;                                           �������
    for(i=0; i<pWal->nWiData; i++){             ѭ��nWiData
      sqlite3_free((void *)pWal->apWiData[i]);     �ͷ�ָ��
      pWal->apWiData[i] = 0;                     ��apWiData��ֵΪ0
    }
  }else{
    sqlite3OsShmUnmap(pWal->pDbFd, isDelete); ����sqlite3OsShmUnmap ����
  }
}

/* 
** Open a connection to the WAL file zWalName. The database file must 
** already be opened on connection pDbFd. The buffer that zWalName points
** to must remain valid for the lifetime of the returned Wal* handle.��
**��һ�������ļ�zWalName��wal�����ݿ��ļ������Ѿ��� pDbFd���ӡ�
**zWalName��Ļ��������뱣����Ч�����������ڷ���Wal ����

** A SHARED lock should be held on the database file when this function
** is called. The purpose of this SHARED lock is to prevent any other
** client from unlinking the WAL or wal-index file. If another process
** were to do this just after this client opened one of these files, the
** system would be badly broken.
**���������������ʱһ��������Ӧ�������ݿ��ļ������С�
**�����������Ŀ���Ƿ�ֹ�κ������ͻ����WAL��wal-index�ļ������ӡ�
**�����һ������Ҳ������������ͻ��˴���Щ�ļ�֮һʱ ϵͳ�������ƻ���
** If the log file is successfully opened, SQLITE_OK is returned and 
** *ppWal is set to point to a new WAL handle. If an error occurs,
** an SQLite error code is returned and *ppWal is left unmodified.
**����ɹ�����־�ļ�,������SQLITE_OK��ppWal������Ϊָ��һ���µ�WAL����
**������ִ���, ���᷵�� һ��SQLite��������* ppWal�޸ĵġ�
*/
int sqlite3WalOpen( 
  sqlite3_vfs *pVfs,              /* vfs module to open wal and wal-index */ vfs ��Wal ��wal-index
  sqlite3_file *pDbFd,            /* The open database file */ �����ݿ��ļ�
  const char *zWalName,           /* Name of the WAL file */   ����Wal�ļ�����
  int bNoShm,                     /* True to run in heap-memory mode */ �ڶ��ڴ���������Ϊ��
  i64 mxWalSize,                  /* Truncate WAL to this size on reset */ ����Wal�ļ��Ĵ�С
  Wal **ppWal                     /* OUT: Allocated Wal handle */ ����Wal
){
  int rc;                         /* Return Code */ ������
  Wal *pRet;                      /* Object to allocate and return */����ͷ��ض���
  int flags;                      /* Flags passed to OsOpen() */ ����osOpen�ı�־

  assert( zWalName && zWalName[0] ); ��ֹ����
  assert( pDbFd );��ֹ����

  /* In the amalgamation, the os_unix.c and os_win.c source files come before
  ** this source file.  Verify that the #defines of the locking byte offsets
  ** in os_unix.c and os_win.c agree with the WALINDEX_LOCK_OFFSET value.
  **��Դ�ļ�֮ǰ�ں�os_unix��c��os_win��ȷ��#�����ֽ�ƫ�����Ķ�����os_unix��c��os_win��cͬ��WALINDEX_LOCK_OFFSETֵ��
  */
#ifdef WIN_SHM_BASE
  assert( WIN_SHM_BASE==WALINDEX_LOCK_OFFSET );
#endif
#ifdef UNIX_SHM_BASE
  assert( UNIX_SHM_BASE==WALINDEX_LOCK_OFFSET );
#endif


  /* Allocate an instance of struct Wal to return. */ ����һ��Walʵ����Ϊ����
  *ppWal = 0;              ����ֵΪ0
  pRet = (Wal*)sqlite3MallocZero(sizeof(Wal) + pVfs->szOsFile); ����Wal�ļ�
  if( !pRet ){              ������ò��ɹ�
    return SQLITE_NOMEM;     ����SqLite_NOMEM
  }

  pRet->pVfs = pVfs;     ΪpRet-> pVfs ��ֵ
  pRet->pWalFd = (sqlite3_file *)&pRet[1]; ΪpRet-> pVfs ��ֵ
  pRet->pDbFd = pDbFd;ΪpRet->pWalFd��ֵ
  pRet->readLock = -1;Ϊ pRet->readLock ��ֵ
  pRet->mxWalSize = mxWalSize;ΪpRet->mxWalSize ��ֵ
  pRet->zWalName = zWalName;ΪpRet->zWalName��ֵ
  pRet->syncHeader = 1;ΪpRet->syncHeader��ֵ
  pRet->padToSectorBoundary = 1;ΪpRet->padToSectorBoundary��ֵ
  pRet->exclusiveMode = (bNoShm ? WAL_HEAPMEMORY_MODE: WAL_NORMAL_MODE);ΪpRet->exclusiveMode��ֵ

  /* Open file handle on the write-ahead log file. */дǰ��־�ļ����ļ������
  flags = (SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_WAL); falgs���
  rc = sqlite3OsOpen(pVfs, zWalName, pRet->pWalFd, flags, &flags); ����sqlite3OSOpen��������
  if( rc==SQLITE_OK && flags&SQLITE_OPEN_READONLY ){    
    pRet->readOnly = WAL_RDONLY;      ����pRet->readonly ��ֵ
  }

  if( rc!=SQLITE_OK ){ ���rc ���ɹ�
    walIndexClose(pRet, 0);  ����walIndexClose����
    sqlite3OsClose(pRet->pWalFd);  ����sqlite3OSclose��������
    sqlite3_free(pRet); �ͷ� pRet����
  }else{
    int iDC = sqlite3OsDeviceCharacteristics(pRet->pWalFd); ����ϵͳ����
    if( iDC & SQLITE_IOCAP_SEQUENTIAL ){ pRet->syncHeader = 0; }
    if( iDC & SQLITE_IOCAP_POWERSAFE_OVERWRITE ){
      pRet->padToSectorBoundary = 0;    ����Wal�е�padToSectorBoundary����
    }
    *ppWal = pRet;  ���µ�wal����ppWal ����
    WALTRACE(("WAL%d: opened\n", pRet)); 
  }
  return rc; ����rc
}

/*
** Change the size to which the WAL file is truncated on each reset. �ı� Wal �ļ��Ĵ�С��ÿ�����趼��ʹWal�ļ���С
*/
void sqlite3WalLimit(Wal *pWal, i64 iLimit){
  if( pWal ) pWal->mxWalSize = iLimit;
}

/*
** Find the smallest page number out of all pages held in the WAL that
** has not been returned by any prior invocation of this method on the
** same WalIterator object.   Write into *piFrame the frame index where
** that page was last written into the WAL.  Write into *piPage the page
** number.�ҵ���С��ҳ�������ҳ�������֮ǰ��û�б����ص��κε������������WalIterator������ͬ��д��* piFrame֡����ҳ�����д������д�����͹ܺš�
**
** Return 0 on success.  If there are no pages in the WAL with a page
** number larger than , then return 1.//�ɹ�����0�������WAL��û��ҳ��������*piPage,Ȼ�󷵻�1��
*/
static int walIteratorNext(
  WalIterator *p,               /* Iterator */  ����
  u32 *piPage,                  /* OUT: The page number of the next page */ ��һҳ
  u32 *piFrame                  /* OUT: Wal frame index of next page */ ��һҳ��Wal����
){
  u32 iMin;                     /* Result pgno must be greater than iMin */ ���� pgno һ��Ҫ��iMin ��
  u32 iRet = 0xFFFFFFFF;        /* 0xffffffff is never a valid page number */ 0xffffffff ����һ����Ч�� ҳ��
  int i;                        /* For looping through segments */   ѭ������

  iMin = p->iPrior;                           ��ȡ ���� ֵ    
  assert( iMin<0xffffffff );             ��� iMin ��ֵ< oxfffffffff ,˵�� imin ������Чֵ
  for(i=p->nSegment-1; i>=0; i--){         
    struct WalSegment *pSegment = &p->aSegment[i]; ����WalSegment �ı��� ����ֵ
    while( pSegment->iNext<pSegment->nEntry ){  �� inext С�� nEntry 
      u32 iPg = pSegment->aPgno[pSegment->aIndex[pSegment->iNext]]; ������� ������ aPgno��ֵ
      if( iPg>iMin ){        ���ipg ���� iMIn
        if( iPg<iRet ){    ipg������Чֵ
          iRet = iPg;       iRet �͸�ֵ iPg
          *piFrame = pSegment->iZero + pSegment->aIndex[pSegment->iNext]; ��һ��Wal���� ��ֵ 
        }
        break;  ����ѭ��  
      }
      pSegment->iNext++; ���� iNext �����Լ�
    }
  }

  *piPage = p->iPrior = iRet; �� iPrior ���� iret
  return (iRet==0xFFFFFFFF);  ��iRet��ֵ������
}

/*
** This function merges two sorted lists into a single sorted list. �ϲ�
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
static void walMerge(                  
  const u32 *aContent,            /* Pages in wal - keys for the sort */ 
  ht_slot *aLeft,                 /* IN: Left hand input list */ ������        
  int nLeft,                      /* IN: Elements in array *paLeft */  �������Ԫ��
  ht_slot **paRight,              /* IN/OUT: Right hand input list */�����������б�
  int *pnRight,                   /* IN/OUT: Elements in *paRight */ �� paRight ���Ԫ��
  ht_slot *aTmp                   /* Temporary buffer */ ��ʱ����
){
  int iLeft = 0;                  /* Current index in aLeft */����������ֵ
  int iRight = 0;                 /* Current index in aRight */����������ֵ
  int iOut = 0;                   /* Current index in output buffer */ ���
  int nRight = *pnRight;
  ht_slot *aRight = *paRight;   

  assert( nLeft>0 && nRight>0 );�������������С��0 ����ֹ����
  while( iRight<nRight || iLeft<nLeft ){ ��������кϲ�
    ht_slot logpage;  
    Pgno dbpage;  

    if( (iLeft<nLeft) 
     && (iRight>=nRight || aContent[aLeft[iLeft]]<aContent[aRight[iRight]])
    ){
      logpage = aLeft[iLeft++]; ��aLeft����logPage
    }else{
      logpage = aRight[iRight++];
    }
    dbpage = aContent[logpage];     dbpage��ֵ

    aTmp[iOut++] = logpage;    Ϊ��ʱ������ֵ
    if( iLeft<nLeft && aContent[aLeft[iLeft]]==dbpage ) iLeft++;

    assert( iLeft>=nLeft || aContent[aLeft[iLeft]]>dbpage ); ��ֹ����
    assert( iRight>=nRight || aContent[aRight[iRight]]>dbpage );��ֹ����
  }

  *paRight = aLeft; 
  *pnRight = iOut;
  memcpy(aLeft, aTmp, sizeof(aTmp[0])*iOut);���ÿ�������
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
**����aList[X] �� aList[Y]������ֵ��������С��ֵ��
*/
static void walMergesort( wal�Ĺ鲢
  const u32 *aContent,            /* Pages in wal */ wal��ҳ
  ht_slot *aBuffer,               /* Buffer of at least *pnList items to use */
  ht_slot *aList,                 /* IN/OUT: List to sort */����һ������
 s                   /* IN/OUT: Number of elements in aList[] */��Ŀ
){
  struct Sublist {
    int nList;                    /* Number of elements in aList */ ������ Ԫ�صĸ���
    ht_slot *aList;               /* Pointer to sub-list content */ ָ���������ָ��
  };

  const int nList = *pnList;      /* Size of input list */ ��������Ĵ�С
  int nMerge = 0;                 /* Number of elements in list aMerge */�ںϲ������Ԫ�ظ���
  ht_slot *aMerge = 0;            /* List to be merged */ 
  int iList;                      /* Index into input list */ �������������
  int iSub = 0;                   /* Index into aSub array */ asub ����� ����
  struct Sublist aSub[13];        /* Array of sub-lists */ ����sub-lists *

  memset(aSub, 0, sizeof(aSub)); Ϊ asub�����ڴ�
  assert( nList<=HASHTABLE_NPAGE && nList>0 );��ֹ����
  assert( HASHTABLE_NPAGE==(1<<(ArraySize(aSub)-1)) );��ֹ����

  for(iList=0; iList<nList; iList++){ ���������ѭ��
    nMerge = 1;    
    aMerge = &aList[iList]; ȡ��ַ
    for(iSub=0; iList & (1<<iSub); iSub++){
      struct Sublist *p = &aSub[iSub]; ��ֵ
      assert( p->aList && p->nList<=(1<<iSub) );
      assert( p->aList==&aList[iList&~((2<<iSub)-1)] );
      walMerge(aContent, p->aList, p->nList, &aMerge, &nMerge, aBuffer);
    }
    aSub[iSub].aList = aMerge; 
    aSub[iSub].nList = nMerge; //Ԫ�صĸ���
  }

  for(iSub++; iSub<ArraySize(aSub); iSub++){ 
    if( nList & (1<<iSub) ){
      struct Sublist *p = &aSub[iSub];�������
      assert( p->nList<=(1<<iSub) );��ֹ����
      assert( p->aList==&aList[nList&~((2<<iSub)-1)] );��ֹ����
      walMerge(aContent, p->aList, p->nList, &aMerge, &nMerge, aBuffer);���ú���
    }
  }
  assert( aMerge==aList ); 
  *pnList = nMerge; Ϊ����ֵ��ֵ

#ifdef SQLITE_DEBUG     ���� SQLITE_DEBUG 
  {
    int i;
    for(i=1; i<*pnList; i++){
      assert( aContent[aList[i]] > aContent[aList[i-1]] );�����ж�
    }
  }
#endif
}

/* 
** Free an iterator allocated by walIteratorInit().
**ͨ��walIteratorInit()�����ͷŷ���ĵ�����
*/
static void walIteratorFree(WalIterator *p){
  sqlite3ScratchFree(p);      �����ͷ�ָ��p
}

/*
** Construct a WalInterator object that can be used to loop over all 
** pages in the WAL in ascending order. The caller must hold the checkpoint
 ** lock. ����һ��WalInterator ���� �������Զ�wal�ļ��а��������е�ҳ����ѭ������ ��������һ��Ҫ�� checkpoint������   
 **
** On success, make *pp point to the newly allocated WalInterator object ����ɹ�,ʹ*ҳָ����WalInterator������󲢷���SQLITE_OK
** return SQLITE_OK. Otherwise, return an error code. If this routine ���򣬷����򷵻ش�����룬���������򷵻ص��Ǵ���ġ�**p ��ֵ�Ͳ�ȷ����
** returns an error, the value of *pp is undefined.
**
** The calling routine should invoke walIteratorFree() to destroy the
** WalIterator object when it has finished with it.�������ʱ���ó���Ӧ�õ���walIteratorFree()������ WalIterator����
*/
static int walIteratorInit(Wal *pWal, WalIterator **pp){ 
  WalIterator *p;                 /* Return value */  ����ֵ
  int nSegment;                   /* Number of segments to merge */ �����м��������ϲ�
  u32 iLast;                      /* Last frame in log */ ��־�е� ����֡
  int nByte;                      /* Number of bytes to allocate */ ���伸���ֽ�
  int i;                          /* Iterator variable */ ���� ��������
  ht_slot *aTmp;                  /* Temp space used by merge-sort */ �����ڴ����ںϲ�����
  int rc = SQLITE_OK;             /* Return Code */ ���� SQLITE_OK

  /* This routine only runs while holding the checkpoint lock. And
  ** it only runs if there is actually content in the log (mxFrame>0).
  **�������ֻ�б�����������ʱ��������
  **����־�ļ�ȷʵ������(mxFrame>0)ʱ���������С� 
  */
  assert( pWal->ckptLock && pWal->hdr.mxFrame>0 ); �������ڼ����£���ֹ����
  iLast = pWal->hdr.mxFrame;      ��ȡ Wal��ֵ

  /* Allocate space for the WalIterator object. */ ΪWalIterator����ռ�
  nSegment = walFramePage(iLast) + 1; ��ȡ�����ε�ֵ
  nByte = sizeof(WalIterator)          ����Ҫ������ٸ��ֽ�
        + (nSegment-1)*sizeof(struct WalSegment)
        + iLast*sizeof(ht_slot);
  p = (WalIterator *)sqlite3ScratchMalloc(nByte); ����WalIterator �����ڴ�
  if( !p ){                     ������䲻�ɹ�
    return SQLITE_NOMEM;      ���� SQLITE_NOMEM
  }
  memset(p, 0, nByte);   ��P����0
  p->nSegment = nSegment;  WalIterator �е� nSegment ��ֵ
 
  /* Allocate temporary space used by the merge-sort routine. This block
  ** of memory will be freed before this function returns
  **����ʹ�õĺϲ�����������ʱ�ռ䡣
  **�˿��ڴ潫�ں�������ǰ���ͷ�
  */
  aTmp = (ht_slot *)sqlite3ScratchMalloc(          ���ú������� �ڴ�
      sizeof(ht_slot) * (iLast>HASHTABLE_NPAGE?HASHTABLE_NPAGE:iLast)
  );ru'g
  if( !aTmp ){         ������䲻�ɹ�����
    rc = SQLITE_NOMEM;  ���� SQLlIte_NOMEM
  }

  for(i=0; rc==SQLITE_OK && i<nSegment; i++){   ѭ�����
    volatile ht_slot *aHash;            ����һ��aHash ����
    u32 iZero;                                 
    volatile u32 *aPgno;

    rc = walHashGet(pWal, i, &aHash, &aPgno, &iZero); ����walHashGet����
    if( rc==SQLITE_OK ){               ������óɹ�
      int j;                      /* Counter variable */ ���� 
      int nEntry;                 /* Number of entries in this segment */ ����һ���� �м�����Ŀ��
      ht_slot *aIndex;            /* Sorted index for this segment */ ��segment ����ָ��

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
** lock is successfully obtained or the busy-handler returns 0.��ͼ�������WAL��lockIdx�Ͷ���Ĳ���n��
** busy-handler
**������ֱ�������������Գɹ��������busy-handler����0��
**�������ʧ�ܺͲ���xBusy��Ϊ�գ���ô����һ��æ�Ŵ���ĺ�����
**��������Ȼ����������ֱ�����ɹ���û�busy-handler����0
*/
static int walBusyLock(        ��ͼ��ȡWal��������LockIdex ��     
  Wal *pWal,                      /* WAL connection */
  int (*xBusy)(void*),            /* Function to call when busy */��æµʱ���øú���
  void *pBusyArg,                 /* Context argument for xBusyHandler */
  int lockIdx,                    /* Offset of first byte to lock */����ƫ�Ƶĵ�һ�ֽ�
  int n                           /* Number of bytes to lock */���������ֽ���
){
  int rc;          ���巵��ֵ
  do {
    rc = walLockExclusive(pWal, lockIdx, n); ���ú��� ���м���
  }while( xBusy && rc==SQLITE_BUSY && xBusy(pBusyArg) ); 
  return rc;
}

/*
** The cache of the wal-index header must be valid to call this function.
** Return the page-size in bytes used by the database.
**wal-indexͷ�Ļ�������ǿ�����Ч�ĵ������������
**�������ݿ���ʹ�õ��ֽڵ�ҳ���С
*/
static int walPagesize(Wal *pWal){     
  return (pWal->hdr.szPage&0xfe00) + ((pWal->hdr.szPage&0x0001)<<16); 
}

/*
** Copy as much content as we can from the WAL back into the database file
** in response to an sqlite3_wal_checkpoint() request or the equivalent.
** ��WAL�и��ƾ����ܶ�����ݵ����ݿ��ļ�������Ӧsqlite3_wal_checkpoint()���� 
**
** The amount of information copies from WAL to database might be limited
** by active readers.  This routine will never overwrite a database page
** that a concurrent reader might be using.
**��WAL���ݿ⸴�Ƶĵ���Ϣ���Ի�Ծ�Ķ��߿����������Ƶġ�
**���������Զ���Ḳ�ǲ������߿��ܻ�ʹ�õ����ݿ�ҳ��
**
** All I/O barrier operations (a.k.a fsyncs) occur in this routine when�� SQLite
** SQLite is in WAL-mode in synchronous=NORMAL.  That means that if  
** checkpoints are always run by a background thread or background 
** process, foreground threads will never block on a lengthy fsync call.����ζ��,�������������һ����̨�̻߳��̨����* *������,ǰ̨�̲߳����������߳���fsync���á�
**������fsyncs�������ڸó���ʱ����SQLite��
** ��SQLite����WAL-modeͬ��ʱ������������I / O���ϲ������ᷢ��
**����ζ��,�������������һ����̨�̻߳��̨�������У�ǰ̨�̲߳����������߳���fsync���á�

** Fsync is called on the WAL before writing content out of the WAL and
** into the database.  This ensures that if the new content is persistent
** in the WAL and can be recovered following a power-loss or hard reset.
** ������д�뵽���ݿ�֮ǰ�������Fsync 
**�ڶϵ������������Ա�֤�µ����ݳ�������WAL
**
** Fsync is also called on the database file if (and only if) the entire
** WAL content is copied into the database file.  This second fsync makes
** it safe to delete the WAL since the new content will persist in the
** database file.
**���ݿ��ļ����ҽ�������WAL���ݸ��Ƶ����ݿ��ļ�ʱFsync�ᱻ����
**��ڶ���fsyncʹ�����԰�ȫ��ɾ��WAL��Ϊ�����ݽ�һֱ�������ݿ��ļ���
**
** This routine uses and updates the nBackfill field of the wal-index header.//�������ʹ�ú͸���nBackfill wal-indexͷ����
** This is the only routine tha will increase the value of nBackfill.  //����Ψһ�ĳ�������nBackfill��ֵ
** (A WAL reset or recovery will revert nBackfill to zero, but not increase
** its value.)//���û��߻ָ�WAL����ʹ nBackfill�ָ���0״̬�����ǲ�����������ֵ
**
** The caller must be holding sufficient locks to ensure that no other
** checkpoint is running (in any other thread or process) at the same
** time. 
**��������߱���Ҫ�д������������У��Ա�֤û��������checkpoint��ͬһʱ�����С�
*/
static int walCheckpoint(
  Wal *pWal,               .       /* Wal connection */ ���� Wal
  int eMode,                      /* One of PASSIVE, FULL or RESTART */ ���� ����
  int (*xBusyCall)(void*),        /* Function to call when busy */ ���ú���
  void *pBusyArg,                 /* Context argument for xBusyHandler */xBusyHandler�Ĳ���
  int sync_flags,                 /* Flags for OsSync() (or 0) */ ͬ���ı�־
  u8 *zBuf                        /* Temporary buffer to use */ ��ʱ�Ļ�����
){
  int rc;                         /* Return code */  ����ֵ
  int szPage;                     /* Database page-size */ ���ݿ�ҳ�Ĵ�С
  WalIterator *pIter = 0;         /* Wal iterator context */ ����һ�� ����ָ��
  u32 iDbpage = 0;                /* Next database page to write */ ��һ��Ҫд�����ݿ�ҳ 
  u32 iFrame = 0;                 /* Wal frame containing data for iDbpage */
  u32 mxSafeFrame;                /* Max frame that can be backfilled */ ����Frame  ���Ի���
  u32 mxPage;                     /* Max database page to write */ �������ݿ�ҳ
  int i;                          /* Loop counter */   ѭ������
  volatile WalCkptInfo *pInfo;    /* The checkpoint status information */������Ϣ
  int (*xBusy)(void*) = 0;        /* Function to call when waiting for locks */

  szPage = walPagesize(pWal); ���ú�������ȡ����ҳ�Ĵ�С
  testcase( szPage<=32768 );      ����
  testcase( szPage>=65536 );      ����
  pInfo = walCkptInfo(pWal); ���ú�����ȡ�������Ϣ
  if( pInfo->nBackfill>=pWal->hdr.mxFrame ) return SQLITE_OK; ����� �򷵻� SQLITE_OK

  /* Allocate the iterator */ ���� ������
  rc = walIteratorInit(pWal, &pIter); ���� wal�ĳ�ʼ��
  if( rc!=SQLITE_OK ){      ������ò��ɹ�
    return rc;              �򷵻� rc
  }
  assert( pIter );     ��� pIter û��ʼ�� ����ֹ����

  if( eMode!=SQLITE_CHECKPOINT_PASSIVE ) xBusy = xBusyCall; ��� emode ���Ǳ����õ�ģʽ

  /* Compute in mxSafeFrame the index of the last frame of the WAL that is
  ** safe to write into the database.  Frames beyond mxSafeFrame might
  ** overwrite database pages that are in use by active readers and thus
  ** cannot be backfilled from the WAL.��������mxSafeFrameָ�������һ֡��ȫд�����ݿ⡣���֮��mxSafeFrame���ܸ������ݿ�ҳ����ʹ�õĻ�Ծ�Ķ���,����޷�������wal
  */
  mxSafeFrame = pWal->hdr.mxFrame; ��ȡ mxSafeFrame��ֵ
  mxPage = pWal->hdr.nPage;         ��ȡmxpage de ֵ
  for(i=1; i<WAL_NREADER; i++){     
    u32 y = pInfo->aReadMark[i]; ���� ���� 
    if( mxSafeFrame>y ){      
      assert( y<=pWal->hdr.mxFrame );
      rc = walBusyLock(pWal, xBusy, pBusyArg, WAL_READ_LOCK(i), 1); ����ֵ
      if( rc==SQLITE_OK ){      ��� WalBusyLock �������óɹ�
        pInfo->aReadMark[i] = (i==1 ? mxSafeFrame : READMARK_NOT_USED); ͨ���ж�i�Ƿ����1 ��Ϊ�丳ֵ
        walUnlockExclusive(pWal, WAL_READ_LOCK(i), 1); ���ý�������
      }else if( rc==SQLITE_BUSY ){ ��� rc ��Sqllite����busy
        mxSafeFrame = y;        ��yֵ���� mxSafeFrame
        xBusy = 0;               ��xBusy��0
      }else{
        goto walcheckpoint_out; ��ת�� Walcheckpoint_out
      }
    }
  }

  if( pInfo->nBackfill<mxSafeFrame
   && (rc = walBusyLock(pWal, xBusy, pBusyArg, WAL_READ_LOCK(0), 1))==SQLITE_OK      �ж����
  ){
    i64 nSize;                    /* Current size of database file */ ��ǰ���ݿ��С
    u32 nBackfill = pInfo->nBackfill;       

    /* Sync the WAL to disk */ ��Walͬ���� ������
    if( sync_flags ){   �ж��Ƿ�ͬ��
      rc = sqlite3OsSync(pWal->pWalFd, sync_flags); ����ͬ������
    }

    /* If the database file may grow as a result of this checkpoint, hint
    ** about the eventual size of the db file to the VFS layer. 
    **������ݿ��ļ�������Ϊcheckpoint��չ
    **��ʾ����db�ļ������մ�СVFS�㡣
    */
    if( rc==SQLITE_OK ){  ������óɹ�
      i64 nReq = ((i64)mxPage * szPage); ���������С
      rc = sqlite3OsFileSize(pWal->pDbFd, &nSize); ����ϵͳ���� ȷ���ļ���С
      if( rc==SQLITE_OK && nSize<nReq ){     ������óɹ� �� �����ļ� С�� ���ķ�ֵ
        sqlite3OsFileControlHint(pWal->pDbFd, SQLITE_FCNTL_SIZE_HINT, &nReq);  ���ú���
      }
    }

    /* Iterate through the contents of the WAL, copying data to the db file. */  ��Wal�����ݸ��Ƶ������ļ���
    while( rc==SQLITE_OK && 0==walIteratorNext(pIter, &iDbpage, &iFrame) ){ 
      i64 iOffset;        ���� 64 �ı���
      assert( walFramePgno(pWal, iFrame)==iDbpage ); ������ú����ķ���ֵ������ IDbpage������ֹ����
      if( iFrame<=nBackfill || iFrame>mxSafeFrame || iDbpage>mxPage ) continue; ������������ ���������˴�ѭ��
      iOffset = walFrameOffset(iFrame, szPage) + WAL_FRAME_HDRSIZE; ΪIoffSET��ֵ
      /* testcase( IS_BIG_INT(iOffset) ); // requires a 4GiB WAL file */
      rc = sqlite3OsRead(pWal->pWalFd, zBuf, szPage, iOffset);  ����ϵͳ������
      if( rc!=SQLITE_OK ) break; ������ò��ɹ� ����ֹѭ��
      iOffset = (iDbpage-1)*(i64)szPage; ��ȡֵ
      testcase( IS_BIG_INT(iOffset) ); ���Ժ���
      rc = sqlite3OsWrite(pWal->pDbFd, zBuf, szPage, iOffset);   ����д����
      if( rc!=SQLITE_OK ) break;������ò��ɹ���������ѭ��
    }

    /* If work was actually accomplished...
    if( rc==SQLITE_OK ){  ���rc ����SQLite_ok
      if( mxSafeFrame==walIndexHdr(pWal)->mxFrame ){ 
        i64 szDb = pWal->hdr.nPage*(i64)szPage; ����64Ϊ�ı��� ���ݿ��С
        testcase( IS_BIG_INT(szDb) );        ���Ժ���
        rc = sqlite3OsTruncate(pWal->pDbFd, szDb); ���� ����
        if( rc==SQLITE_OK && sync_flags ){      
          rc = sqlite3OsSync(pWal->pDbFd, sync_flags); ����ͬ������
        }
      }
      if( rc==SQLITE_OK ){        ������óɹ�  
        pInfo->nBackfill = mxSafeFrame;
      }
    }

    /* Release the reader lock held while backfilling */
    walUnlockExclusive(pWal, WAL_READ_LOCK(0), 1); �ͷ���
  }

  if( rc==SQLITE_BUSY ){      
    /* Reset the return code so as not to report a checkpoint failure
    ** just because there are active readers.  */
    rc = SQLITE_OK;     rc ��ֵ SQLITE_OK
  }

  /* If this is an SQLITE_CHECKPOINT_RESTART operation, and the entire wal
  ** file has been copied into the database file, then block until all
  ** readers have finished using the wal file. This ensures that the next
  ** process to write to the database restarts the wal file.
  **�������һ��SQLITE_CHECKPOINT_RESTART����,���������ļ��Ѹ��Ƶ����ݿ��ļ�,Ȼ����ֹ,ֱ�����ж���ʹ��wal�ļ��Ѿ���ɡ�
  **
  */
  if( rc==SQLITE_OK && eMode!=SQLITE_CHECKPOINT_PASSIVE ){ ���rc ���� SQLITE_ok ,����SQLITE_CHECKPOINT_PASSIVEģʽ��
    assert( pWal->writeLock ); ��ֹ����
    if( pInfo->nBackfill<pWal->hdr.mxFrame ){
      rc = SQLITE_BUSY; 
    }else if( eMode==SQLITE_CHECKPOINT_RESTART ){
      assert( mxSafeFrame==pWal->hdr.mxFrame );
      rc = walBusyLock(pWal, xBusy, pBusyArg, WAL_READ_LOCK(1), WAL_NREADER-1);
      if( rc==SQLITE_OK ){
        walUnlockExclusive(pWal, WAL_READ_LOCK(1), WAL_NREADER-1); ���ý�������
      }
    }
  }

 walcheckpoint_out: goto ��־
  walIteratorFree(pIter); �ͷ�ָ��
  return rc;
}

/*
** If the WAL file is currently larger than nMax bytes in size, truncate
** it to exactly nMax bytes. If an error occurs while doing so, ignore it. ���Wal�ļ��������ֵ������������ȷ�ĳ��ȡ�
*/
static void walLimitSize(Wal *pWal, i64 nMax){
  i64 sz;     ����64Ϊ�ı��� 
  int rx; 
  sqlite3BeginBenignMalloc();���ú���
  rx = sqlite3OsFileSize(pWal->pWalFd, &sz); ����ϵͳ�����õ�Wal�Ĵ�С
  if( rx==SQLITE_OK && (sz > nMax ) ){    ������ú����ɹ�������ļ���С������Χ
    rx = sqlite3OsTruncate(pWal->pWalFd, nMax); ���ú��������ļ���С����
  }
  sqlite3EndBenignMalloc(); �����ڴ����
  if( rx ){       ���rxΪ��   
    sqlite3_log(rx, "cannot limit WAL size: %s", pWal->zWalName);�����־�Ѿ����������־��Ϣд�뵽��־�У�
  }
}

/*
** Close a connection to a log file. �ر���־�ļ�����
*/
int sqlite3WalClose(
  Wal *pWal,                      /* Wal to close */ ����Wal �ṹָ��
  int sync_flags,                 /* Flags to pass to OsSync() (or 0) */  ����ͬ���ı�־
  int nBuf,
  u8 *zBuf                        /* Buffer of at least nBuf bytes */ ���������ж���ֽڵĻ�����
){
  int rc = SQLITE_OK;             
  if( pWal ){                         ���wal��Ϊ��
    int isDelete = 0;             /* True to unlink wal and wal-index files */ �⿪Wal��Wal-inde��������Ϊ��

    /* If an EXCLUSIVE lock can be obtained on the database file (using the
    ** ordinary, rollback-mode locking methods, this guarantees that the
    ** connection associated with this log file is the only connection to
    ** the database. In this case checkpoint the database and unlink both
    ** the wal and wal-index files. ������ݿ��ļ����Ի�ȡһ��EXCLUSIVE ����ʹ����ͨ��rollback-mode��������,�Ᵽ֤��������ص���־�ļ���Ψһ���������ݿ�
    **�������Խ��м�����ݿ�ͽ⿪Wal��Wal-index
    ** The EXCLUSIVE lock is not released before returning. ֱ���������ͷŸ���
    */
    rc = sqlite3OsLock(pWal->pDbFd, SQLITE_LOCK_EXCLUSIVE); ���ú������м���
    if( rc==SQLITE_OK ){            ������ú����ɹ�
      if( pWal->exclusiveMode==WAL_NORMAL_MODE   { ���Walû����
        pWal->exclusiveMode = WAL_EXCLUSIVE_MODE; ���м���
      }
      rc = sqlite3WalCheckpoint(                         
          pWal, SQLITE_CHECKPOINT_PASSIVE, 0, 0, sync_flags, nBuf, zBuf, 0, 0
      ); ���м���
      if( rc==SQLITE_OK ){ ������ú����ɹ�
        int bPersist = -1; 
        sqlite3OsFileControlHint(      ����ϵͳ����
            pWal->pDbFd, SQLITE_FCNTL_PERSIST_WAL, &bPersist
        );
        if( bPersist!=1 ){   
          /* Try to delete the WAL file if the checkpoint completed and
          ** fsyned (rc==SQLITE_OK) and if we are not in persistent-wal
          ** mode (!bPersist) */ ����ɾ��WAL�ļ������������fsyned(rc = = SQLITE_OK),������ǲ�persistent-walģʽ
          isDelete = 1;
        }else if( pWal->mxWalSize>=0 ){ 
          /* Try to truncate the WAL file to zero bytes if the checkpoint
          ** completed and fsynced (rc==SQLITE_OK) and we are in persistent
          ** WAL mode (bPersist) and if the PRAGMA journal_size_limit is a
          ** non-negative value (pWal->mxWalSize>=0).  Note that we truncate
          ** to zero bytes as truncating to the journal_size_limit might
          ** leave a corrupt WAL file on disk. */��ͼ�ض�WAL�ļ����ֽ����������ɲ�fsync(rc = = SQLITE_OK),�����ڳ�������ģʽ(bPersist)�������ָʾjournal_size_limit��һ���Ǹ���ֵ(pWal - > mxWalSize > = 0)��ע��,���ǽض�Ϊ���ֽ�journal_size_limit����ɾ���뿪����WAL�����ϵ��ļ���* /
          walLimitSize(pWal, 0);
        }
      }
    }

    walIndexClose(pWal, isDelete);���ùر�����
    sqlite3OsClose(pWal->pWalFd); �ر���־�ļ�����
    if( isDelete ){������ú����ɹ�
      sqlite3BeginBenignMalloc();���ù����ڴ�
      sqlite3OsDelete(pWal->pVfs, pWal->zWalName, 0); ����ڴ�
      sqlite3EndBenignMalloc(); �ر��ڴ����
    }
    WALTRACE(("WAL%p: closed\n", pWal));�ر���־
    sqlite3_free((void *)pWal->apWiData);�ͷ�ָ��
    sqlite3_free(pWal);�ͷ�ָ��
  }
  return rc;
}

/*
** Try to read the wal-index header.  Return 0 on success and 1 if
** there is a problem. ��ȡWal-indexͷ���ݣ�����ɹ��򷵻�0�������򷵻�1
**
** The wal-index is in shared memory.  Another thread or process might
** be writing the header at the same time this procedure is trying to
** read it, which might result in inconsistency.  A dirty read is detected
** by verifying that both copies of the header are the same and also by
** a checksum on the header.    wal-index�ڹ����ڴ档��һ���̻߳������дͷ���ݵ�ͬʱ�������������ڶ���,����ܻᵼ�²�һ�¡�
**��⵽���ͨ����֤�����������Ķ����ݶ���һ����.
** If and only if the read is consistent and the header is different from
** pWal->hdr, then pWal->hdr is updated to the content of the new header
** and *pChanged is set to 1. ���ҽ�����������һ���ģ�ͷ���ݺ�Pwal->hdr��ͬ��,��ͷ���ݵ����ݽ��и��� �� ��*pChanged��ֵ��Ϊ1
**
** If the checksum cannot be verified return non-zero. If the header �����鲻�ܱ�֤ʵ���򷵻طǿգ������ȡ���ݳɹ��ͱ�֤ʵ������0
** is read successfully and the checksum verified, return zero.
*/
static int walIndexTryHdr(Wal *pWal, int *pChanged){
  u32 aCksum[2];                  /* Checksum on the header content * ��ͷ���ݵ����ݽ���У��/
  WalIndexHdr h1, h2;             /* Two copies of the header content */ �������� WalIndexHdr ����
  WalIndexHdr volatile *aHdr;     /* Header in shared memory */ 

  /* The first page of the wal-index must be mapped at this point. */  ���ָ��ӳ�䵽wal-index �ĵ�һҳ
  assert( pWal->nWiData>0 && pWal->apWiData[0] );

  /* Read the header. This might happen concurrently with a write to the 
  ** same area of shared memory on a different CPU in a SMP,
  ** meaning it is possible that an inconsistent snapshot is read
  ** from the file. If this happens, return non-zero. ��ȡ����ͷ�����ܷ�������д���ڴ�ͬһƬ��������ܵ��²�һ�¡��������������������طǿ�ֵ��
  **
  ** There are two copies of the header at the beginning of the wal-index. ��ǰ��ֵ�������ݣ�����˳����read��0����[1],д��˳����1��0
  ** When reading, read [0] first then [1].  Writes are in the reverse order.�ڴ��ϰ���������ֹ��������Ӳ����������Ķ���д
  ** Memory barriers are used to prevent the compiler or the hardware from
  ** reordering the reads and writes.
  */
  aHdr = walIndexHdr(pWal); 
  memcpy(&h1, (void *)&aHdr[0], sizeof(h1)); �����ݸ��Ƶ�aHdr[0]
  walShmBarrier(pWal);
  memcpy(&h2, (void *)&aHdr[1], sizeof(h2));�����ݸ��Ƶ�aHdr[1]

  if( memcmp(&h1, &h2, sizeof(h1))!=0 ){ ��1��2���бȽϣ��������ͬ
    return 1;   /* Dirty read */  ����1 ��������
  }  
  if( h1.isInit==0 ){ �����ʼ�����ɹ�
    return 1;   /* Malformed header - probably all zeros */
  }
  walChecksumBytes(1, (u8*)&h1, sizeof(h1)-sizeof(h1.aCksum), 0, aCksum);
  if( aCksum[0]!=h1.aCksum[0] || aCksum[1]!=h1.aCksum[1] ){ �����ƥ��
    return 1;   /* Checksum does not match */ ����1
  }

  if( memcmp(&pWal->hdr, &h1, sizeof(WalIndexHdr)) ){
    *pChanged = 1; ���� 
    memcpy(&pWal->hdr, &h1, sizeof(WalIndexHdr));
    pWal->szPage = (pWal->hdr.szPage&0xfe00) + ((pWal->hdr.szPage&0x0001)<<16); ����Wal��ҳ�Ĵ�С
    testcase( pWal->szPage<=32768 ); ���Ժ���
    testcase( pWal->szPage>=65536 );
  }

  /* The header was successfully read. Return zero. */ ���ͷ���ݶ�ȡ�ɹ�
  return 0; ����0
}

/*
** Read the wal-index header from the wal-index and into pWal->hdr.
** If the wal-header appears to be corrupt, try to reconstruct the
** wal-index from the WAL before returning.���Wal-index�����Wal-header���ִ����ڷ���ǰ�ع���
**
** Set *pChanged to 1 if the wal-index header value in pWal->hdr is
** changed by this opertion.  If pWal->hdr is unchanged, set *pChanged
** to 0. ����*pchangeΪ1 ����������й���pWal->hdr û�ı䣬������ Ϊ0
**
** If the wal-index header is successfully read, return SQLITE_OK. 
** Otherwise an SQLite error code. �����ɹ�������ok�����򷵻�error code
*/
static int walIndexReadHdr(Wal *pWal, int *pChanged){
  int rc;                         /* Return code */ ���� ֵ
  int badHdr;                     /* True if a header read failed */ ����ʧ�ܣ�ֵΪ��
  volatile u32 *page0;            /* Chunk of wal-index containing header */���� header��wal-index��

  /* Ensure that page 0 of the wal-index (the page that contains the 
  ** wal-index header) is mapped. Return early if an error occurs here.
  */
  assert( pChanged ); ���pChange Ϊ0 �������ֹ
  rc = walIndexPage(pWal, 0, &page0); ���ú�����ȡҳ
  if( rc!=SQLITE_OK ){ ������ò��ɹ�������
    return rc;
  };
  assert( page0 || pWal->writeLock==0 );  ���pageû�ɹ���ȡֵ ������ֹ����

  /* If the first page of the wal-index has been mapped, try to read the
  ** wal-index header immediately, without holding any lock. This usually
  ** works, but may fail if the wal-index header is corrupt or currently 
  ** being modified by another thread or process.��
  **�����־�����ĵ�һҳ�Ѿ�ӳ�� ���������� wal-indexͷ����,û�г����κ�����
  **��ͨ���Ĺ���,���ǿ��ܻ�ʧ�����wal-indexͷĿǰ��ռ�û���һ���̻߳�����޸�
  */
  badHdr = (page0 ? walIndexTryHdr(pWal, pChanged) : 1); ����������ӣ����ȡwalIndexTryHDr(pWal,pChange),����Ϊ1

  /* If the first attempt failed, it might have been due to a race
  ** with a writer.  So get a WRITE lock and try again.�����һ�γ���ʧ����,�����������д���������Եõ�һ��д��,����һ��
  */
  assert( badHdr==0 || pWal->writeLock==0 );
  if( badHdr ){  �����ȡʧ��
    if( pWal->readOnly & WAL_SHM_RDONLY ){
      if( SQLITE_OK==(rc = walLockShared(pWal, WAL_WRITE_LOCK)) ){  �����ȡ���ǹ�����
        walUnlockShared(pWal, WAL_WRITE_LOCK);  �ͷŹ�����
        rc = SQLITE_READONLY_RECOVERY;
      }
    }else if( SQLITE_OK==(rc = walLockExclusive(pWal, WAL_WRITE_LOCK, 1)) ){ �����ȡ����������
      pWal->writeLock = 1; ��Wal��writeLock��ֵΪ1
      if( SQLITE_OK==(rc = walIndexPage(pWal, 0, &page0)) ){��� ��ȡ����ҳ�ɹ�
        badHdr = walIndexTryHdr(pWal, pChanged); ��ȡ����ͷ����
        if( badHdr ){
          /* If the wal-index header is still malformed even while holding
          ** a WRITE lock, it can only mean that the header is corrupted and
          ** needs to be reconstructed.  So run recovery to do exactly that.
          */�����wal-indexͷ��Ȼ�������ݼ�ʹ�ڼ���֮��,��ֻ����ζ��header�𻵣�����Ҫ�ؽ������Իָ�����
          rc = walIndexRecover(pWal);�ؽ�Wal
          *pChanged = 1; ��ֵ
        }
      }
      pWal->writeLock = 0; ��������Ϊ0
      walUnlockExclusive(pWal, WAL_WRITE_LOCK, 1); �ͷ�д��
    }
  }

  /* If the header is read successfully, check the version number to make
  ** sure the wal-index was not constructed with some future format that
  ** this version of SQLite cannot understand.����ɹ���ͷ����,���汾ȷ��Wal�Ľṹ�岻������汾��ʶ��
  */
  if( badHdr==0 && pWal->hdr.iVersion!=WALINDEX_MAX_VERSION ){
    rc = SQLITE_CANTOPEN_BKPT;
  }

  return rc;����ֵ
}

/*
** This is the value that walTryBeginRead returns when it needs to
** be retried. 
**������Ҫ����ʱ������walTryBeginRead���ص�ֵ
*/
#define WAL_RETRY  (-1)

/*
** Attempt to start a read transaction.  This might fail due to a race or
** other transient condition.  When that happens, it returns WAL_RETRY to
** indicate to the caller that it is safe to retry immediately.
**���Կ�ʼһ����������������ھ���������ͻ���������ʧ�� ���ʧ�ܷ��� Wal_RETRY ��ʾ �������ԡ�
** On success return SQLITE_OK.  On a permanent failure (such an
** I/O error or an SQLITE_BUSY because another process is running
** recovery) return a positive error code.
**���ɹ�����SQLITE_OK��(����һ������ʧ��I / O�����SQLITE_BUSY��Ϊ��һ�������������лָ�)����һ������
** The useWal parameter is true to force the use of the WAL and disable
** the case where the WAL is bypassed because it has been completely
** checkpointed.  If useWal==0 then this routine calls walIndexReadHdr() 
** to make a copy of the wal-index header into pWal->hdr.  If the 
** wal-index header has changed, *pChanged is set to 1 (as an indication 
** to the caller that the local paget cache is obsolete and needs to be 
** flushed.)  When useWal==1, the wal-index header is assumed to already
** be loaded and the pChanged parameter is unused.
**useWal���������ǿ��ʹ��WAL�ͽ��õ�WAL����,��Ϊ���Ѿ���ȫ���ü��㡣���useWal = = 0,������̵���walIndexReadHdr()����wal-indexͷ��pWal - > hdr�����wal-indexͷ�Ѿ��ı�,pChanged����Ϊ1(����һ���뷨��������,���ص��弪�ػ����ʱ�ĺ���Ҫˢ��)��useWal = = 1ʱ,wal-indexͷ�Ѿ��ٶ������غ�pChanged������δʹ�õġ�
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
** This routine uses the nBackfill and aReadMark[] fields of the header
** to select a particular WAL_READ_LOCK() that strives to let the
** checkpoint process do as much work as possible.  This routine might
** update values of the aReadMark[] array in the header, but if it does
** so it takes care to hold an exclusive lock on the corresponding
** WAL_READ_LOCK() while changing values.
*/
static int walTryBeginRead(Wal *pWal, int *pChanged, int useWal, int cnt){
  volatile WalCkptInfo *pInfo;    /* Checkpoint information in wal-index */ ���ֵ
  u32 mxReadMark;                 /* Largest aReadMark[] value */ aReadMark[] �����ֵ
  int mxI;                        /* Index of largest aReadMark[] value */aReadMark��������ֵ
  int i;                          /* Loop counter */ ѭ������
  int rc = SQLITE_OK;             /* Return code  */ ������

  assert( pWal->readLock<0 );     /* Not currently locked */û�м���

  /* Take steps to avoid spinning forever if there is a protocol error. ��ȡ��ʩ��������ѭ��
  **
  ** Circumstances that cause a RETRY should only last for the briefest
  ** instances of time.  No I/O or other system calls are done while the
  ** locks are held, so the locks should not be held for very long. But 
  ** if we are unlucky, another process that is holding a lock might get
  ** paged out or take a page-fault that is time-consuming to resolve, 
  ** during the few nanoseconds that it is holding the lock.  In that case,
  ** it might take longer than normal for the lock to free.
  **
  ** After 5 RETRYs, we begin calling sqlite3OsSleep().  The first few
  ** calls to sqlite3OsSleep() have a delay of 1 microsecond.  Really this
  ** is more of a scheduler yield than an actual delay.  But on the 10th
  ** an subsequent retries, the delays start becoming longer and longer, 
  ** so that on the 100th (and last) RETRY we delay for 21 milliseconds.
  ** The total delay time before giving up is less than 1 second. 5�����Ժ�,���ǿ�ʼ����sqlite3OsSleep()������ļ�����sqlite3OsSleep()��һ���ӳ�1΢�롣������һ��ʵ�ʵĵ��ȳ���������ӳ١����ڵ�ʮһ����������,�ӳٿ�ʼ���Խ��Խ��, ��100(���)���������ӳ���21���롣����ǰ�����ӳ�ʱ��С��1�롣
  */
  if( cnt>5 ){
    int nDelay = 1;                      /* Pause time in microseconds */��ͣʱ����΢��Ϊ��λ
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
      **�������һ���̻߳��߽���û�лָ����У�WAL_RETRY��תΪBUSY����
      */
      if( pWal->apWiData[0]==0 ){
        /* This branch is taken when the xShmMap() method returns SQLITE_BUSY.
        ** We assume this is a transient condition, so return WAL_RETRY. The
        ** xShmMap() implementation used by the default unix and win32 VFS 
        ** modules may return SQLITE_BUSY due to a race condition in the 
        ** code that determines whether or not the shared-memory region 
        ** must be zeroed before the requested page is returned.
        **�����֧ʱ��ȡ��xShmMap������������SQLITE_BUSY��
        **���Ǽ�������һ�����ݵ�״̬����˷���WAL_RETRY��
        */
        rc = WAL_RETRY;
      }else if( SQLITE_OK==(rc = walLockShared(pWal, WAL_RECOVER_LOCK)) ){//��ȡ������
        walUnlockShared(pWal, WAL_RECOVER_LOCK);//�ͷŹ�����
        rc = WAL_RETRY;//����WAL_RETRY
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
    **WAl�Ѿ���ȫ�Ļ�����ҿ��԰�ȫ�ĺ���
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
        **�ڻ��READ_LOCK(0)֮ǰ�������Ѿ�׷�ӵ������������Ķ��������������ǲ���ȫ��
        **�������READ_LOCK(0)�Ķ����������������־�ļ�������ζ�����ݿ��ļ�������һ���ɿ��Ŀ���
        **��ͨ������ȷ����ΪREAD_LOCK(0)��֯checkpoint�ķ�����
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
  **�����������Ϊֹ�õ����������ζ���Ķ�����Ҫʹ��WAL��������ύ�л���������ݡ�
  **���ڵĹ�����ѡ����ӽ�aReadMark[]���е�һ����������pWal-> hdr.mxFrame����������Ŀ
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
    **�ⲻ�ܱ�֤��־����ͷ�ĸ�������ǰ�ĳ�����
    **����ĳ�ַ�ʽ����
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
** Begin a read transaction on the database. �����ݿ⿪ʼһ��������
**
** This routine used to be called sqlite3OpenSnapshot() and with good reason:
** it takes a snapshot of the state of the WAL and wal-index for the current
** instant in time.  The current thread will continue to use this snapshot.
** Other threads might append new content to the WAL and wal-index but
** that extra content is ignored by the current thread.
**�������ͨ����SQLite3OpenSnapshot������������ԭ��
**��Ҫ�Ŀ���WAL��wal-index��ǰ�ļ�ʱ״̬����ǰ�̼߳���ʹ��ץ�ļ������������߳̿�������µ����ݵ�Wal��Wal-index������ǰ�̲߳���������
** If the database contents have changes since the previous read
** transaction, then *pChanged is set to 1 before returning.  The
** Pager layer will use this to know that is cache is stale and
** needs to be flushed.��������ݿ����ݱ仯�����ڴ���ǰ������,Ȼ��*pChanged����֮ǰ������Ϊ1����Ҫˢ�¡�
*/
int sqlite3WalBeginReadTransaction(Wal *pWal, int *pChanged){
  int rc;                         /* Return code */ ������
  int cnt = 0;                    /* Number of TryBeginRead attempts */ ���¶� �Ĵ���

  do{
    rc = walTryBeginRead(pWal, pChanged, 0, ++cnt);  ���ú���
  }while( rc==WAL_RETRY ); �� ��ȡ�ɹ�
  testcase( (rc&0xff)==SQLITE_BUSY );���Ժ���
  testcase( (rc&0xff)==SQLITE_IOERR );���Ժ���
  testcase( rc==SQLITE_PROTOCOL );���Ժ���
  testcase( rc==SQLITE_OK );���Ժ���
  return rc; ���� rc
}

/*
** Finish with a read transaction.  All this does is release the
** read-lock. ����ɶ������  �ͷ� read-lock
*/
void sqlite3WalEndReadTransaction(Wal *pWal){
  sqlite3WalEndWriteTransaction(pWal); ���ý���д����
  if( pWal->readLock>=0 ){ �������readLock��
    walUnlockShared(pWal, WAL_READ_LOCK(pWal->readLock)); ����
    pWal->readLock = -1; ��ֵ
  }
}

/*
** Read a page from the WAL, if it is present in the WAL and if the 
** current read transaction is configured to use the WAL.  ����WAL��ȡһ��ҳ��,�����������WAL�����ǰ����������ʹ��Wal
** 
** The *pInWal is set to 1 if the requested page is in the WAL and          
** has been loaded.  Or *pInWal is set to 0 if the page was not in 
** the WAL and needs to be read out of the database.*pInWal ��ֵΪ1  ����Ҫ��page ��Wal�У����ѱ����ظ�ֵΪ0 ��� ����wal�У���Ҫ�����ݿ��м���
*/
int sqlite3WalRead(
  Wal *pWal,                      /* WAL handle */ ��һָ��
  Pgno pgno,                      /* Database page number to read data for */ ����ҳ��
  int *pInWal,                    /* OUT: True if data is read from WAL */ �����Ǵ�Wal�ж�ȡ��Ϊ��
  int nOut,                       /* Size of buffer pOut in bytes */ ����ֽ����Ĵ�С  �ֽ�Ϊ��λ
  u8 *pOut                        /* Buffer to write page data to */д���ݵĻ�����
){
  u32 iRead = 0;                  /* If !=0, WAL frame to return data from */
  u32 iLast = pWal->hdr.mxFrame;  /* Last page in WAL for this reader */Wal ����ҳ
  int iHash;                      /* Used to loop through N hash tables */  ��ϣ��

  /* This routine is only be called from within a read transaction. */ ֻ�ܱ�������������
  assert( pWal->readLock>=0 || pWal->lockError );  �ж��Ƿ���ֹ����

  /* If the "last page" field of the wal-index header snapshot is 0, then
  ** no data will be read from the wal under any circumstances. Return early
  ** in this case as an optimization.  Likewise, if pWal->readLock==0, 
  ** then the WAL is ignored by the reader so return early, as if the 
  ** WAL were empty.���wal-indexͷ�ġ����һҳ���ֶο���Ϊ0,�κ������û�����ݽ���Wal��ȡ��ͬ���ģ���� pWal->readLockΪ0��Ȼ�󱻶��ߺ���,����WAL�ǿյ�
  */
  if( iLast==0 || pWal->readLock==0 ){ ���ILast��readLockΪ0
    *pInWal = 0;  ���ݲ��Ǵ�wal �� ����Ϊ��
    return SQLITE_OK; ����ok
  }

  /* Search the hash table or tables for an entry matching page number
  ** pgno. Each iteration of the following for() loop searches one
  ** hash table (each hash table indexes up to HASHTABLE_NPAGE frames).
  ** ������ϣ������һ����ҳ����ƥ�����Ŀ�� ÿһ�ε��� ��������һ��
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
  for(iHash=walFramePage(iLast); iHash>=0 && iRead==0; iHash--){ ��ȡ����ҳ����Ӧ��hashֵ 
    volatile ht_slot *aHash;      /* Pointer to hash table */ ��ϣ���ָ��
    volatile u32 *aPgno;          /* Pointer to array of page numbers */ ҳ���ָ��
    u32 iZero;                    /* Frame number corresponding to aPgno[0] */ Frame�� aPgno��0��һ����Ϊ��
    int iKey;                     /* Hash slot index */ ��ϣ������
    int nCollide;                 /* Number of hash collisions remaining */��ϣ��ײ�Ĵ���
    int rc;                       /* Error code */ ���ش���Ĵ���

    rc = walHashGet(pWal, iHash, &aHash, &aPgno, &iZero); ���ú��� ��ȡֵ
    if( rc!=SQLITE_OK ){ ������ò��ɹ�������
      return rc;
    }
    nCollide = HASHTABLE_NSLOT;  ��ϣ��ײ��Ŀ
    for(iKey=walHash(pgno); aHash[iKey]; iKey=walNextHash(iKey)){  ѭ����ϣ
      u32 iFrame = aHash[iKey] + iZero;        �����ȡ IFrame
      if( iFrame<=iLast && aPgno[aHash[iKey]]==pgno ){  ���IFrameС��Ilast�� Ϊ��
        /* assert( iFrame>iRead ); -- not true if there is corruption */
        iRead = iFrame; 
      }
      if( (nCollide--)==0 ){  ���hash��ײΪ0��
        return SQLITE_CORRUPT_BKPT; ����
      }
    }
  }

#ifdef SQLITE_ENABLE_EXPENSIVE_ASSERT
  /* If expensive assert() statements are available, do a linear search
  ** of the wal-index file content. Make sure the results agree with the
  ** result obtained using the hash indexes above.  */������۴�ĵ�assert()������ʹ��,������������wal-index�ļ������ݡ�ȷ�����ͬ��������ʹ�õ�ɢ������
  {
    u32 iRead2 = 0;
    u32 iTest; 
    for(iTest=iLast; iTest>0; iTest--){  ѭ��iLast 
      if( walFramePgno(pWal, iTest)==pgno ){  ��� ��ѯ������ƥ��� 
        iRead2 = iTest;
        break;                                 �� ����ѭ��
      }
    }
    assert( iRead==iRead2 );
  }
#endif

  /* If iRead is non-zero, then it is the log frame number that contains the
  ** required page. Read and return data from the log file.���iRead��0,��ô��������־��ܰ������������ҳ�档����־�ļ��ж�ȡ����������
  */
  if( iRead ){  ����ǿ�
    int sz;    
    i64 iOffset; 
    sz = pWal->hdr.szPage; ��ȡWal��ҳ�Ĵ�С
    sz = (sz&0xfe00) + ((sz&0x0001)<<16); ������
    testcase( sz<=32768 ); ����sz�ķ�Χ
    testcase( sz>=65536 );
    iOffset = walFrameOffset(iRead, sz) + WAL_FRAME_HDRSIZE;
    *pInWal = 1; ����PINWal��ֵΪ1
    /* testcase( IS_BIG_INT(iOffset) ); // requires a 4GiB WAL */
    return sqlite3OsRead(pWal->pWalFd, pOut, (nOut>sz ? sz : nOut), iOffset); ����ϵͳ����
  }

  *pInWal = 0; ����PINWalΪ0
  return SQLITE_OK; ���� 
}


/* 
** Return the size of the database in pages (or zero, if unknown).
*/
Pgno sqlite3WalDbsize(Wal *pWal){ ��ȡ���ݿ��С��ҳΪ��λ
  if( pWal && ALWAYS(pWal->readLock>=0) ){
    return pWal->hdr.nPage;  ��ȡWal�ж���ҳ
  }
  return 0;
}


/* 
** This function starts a write transaction on the WAL. ��ʼһ��д����
**
** A read transaction must have already been started by a prior call
** to sqlite3WalBeginReadTransaction().����������ڵ���ǰ�Ѿ���ʼͨ������ sqlite3WalBeginReadTransaction()��
**
** If another thread or process has written into the database since
** the read transaction was started, then it is not possible for this
** thread to write as doing so would cause a fork.  So this routine
** returns SQLITE_BUSY in that case and no write transaction is started.�����һ���̻߳����д�����ݿ������ʼ,��ô���ǲ����ܵ��߳�д������������һ���档����������̷���SQLITE_BUSY�����������,û��д����ʼ��
** There can only be a single writer active at a time.ͬһʱ����ֻ�ܽ���һ��д
*/
int sqlite3WalBeginWriteTransaction(Wal *pWal){
  int rc; 

  /* Cannot start a write transaction without first holding a read
  ** transaction. */ д������ǰ���������������
  assert( pWal->readLock>=0 ); �������

  if( pWal->readOnly ){ �������ֻ��
    return SQLITE_READONLY;����
  }

  /* Only one writer allowed at a time.  Get the write lock.  Return
  ** SQLITE_BUSY if unable. ͬһʱ����ֻ�ܽ���һ��д����ȡд��������SQlote_busy ������ܵĻ�
  */
  rc = walLockExclusive(pWal, WAL_WRITE_LOCK, 1); ����
  if( rc ){
    return rc;
  }
  pWal->writeLock = 1; ��ֵ

  /* If another connection has written to the database file since the
  ** time the read transaction on this connection was started, then
  ** the write is disallowed.�����һ�����Ӻ�д�����ݿ��ļ�ʱ�������ʼ���������,��ôд��Ч
  */
  if( memcmp(&pWal->hdr, (void *)walIndexHdr(pWal), sizeof(WalIndexHdr))!=0 ){ �Ƚ�ǰsizeof(WalIndexHdr)�ֽ�
    walUnlockExclusive(pWal, WAL_WRITE_LOCK, 1); ����
    pWal->writeLock = 0; ��������ֵ
    rc = SQLITE_BUSY; ���� ֵ
  }

  return rc;
}

/*
** End a write transaction.  The commit has already been done.  This
** routine merely releases the lock.д����������ύ�Ѿ���ɡ���������ͷ���
*/
int sqlite3WalEndWriteTransaction(Wal *pWal){
  if( pWal->writeLock ){  ���WAL����
	  }
    walUnlockExclusive(pWal, WAL_WRITE_LOCK, 1); ���ú����ͷ���
    pWal->writeLock = 0; ���Ĳ���
    pWal->truncateOnCommit = 0;
  }
  return SQLITE_OK; �ɹ�����

/*
** If any data has been written (but not committed) to the log file, this
** function moves the write-pointer back to the start of the transaction.
** Additionally, the callback function is invoked for each frame written
** to the WAL since the start of the transaction. If the callback returns
** other than SQLITE_OK, it is not invoked again and the error code is
** returned to the caller.
** Otherwise, if the callback function does not return an error, this
** function returns SQLITE_OK.
*/

/*
** ������κ����ݱ�д�루�����ύ������־�ļ��У��ú����ƶ�дָ�뷵�ص�����Ŀ�ʼ�����Ҵ�����뷵�ظ������ߡ���������ص�����������һ�����������������SQLITE_OK��

*/
int sqlite3WalUndo(Wal *pWal, int (*xUndo)(void *, Pgno), void *pUndoCtx){
  int rc = SQLITE_OK;
  if( ALWAYS(pWal->writeLock) ){ ���pWal->writeLock�Ƿ�Ϊ�棬 
    Pgno iMax = pWal->hdr.mxFrame;   ����Pgno ��ֵWal������֡
    Pgno iFrame; ����֡��
  
    /* Restore the clients cache of the wal-index header to the state it
    ** was in before the client began writing to the database.  �ָ�WAL����ͷ��״̬��iframe���ڿͻ��˿�ʼд�뵽���ݿ�֮ǰ�Ŀͻ��˻��档
    */
    memcpy(&pWal->hdr, (void *)walIndexHdr(pWal), sizeof(WalIndexHdr)); ��WalindexHdr���Ƶ���ʼ״̬

    for(iFrame=pWal->hdr.mxFrame+1; 
        ALWAYS(rc==SQLITE_OK) && iFrame<=iMax; 
        iFrame++
    ){
      /* This call cannot fail. Unless the page for which the page number
      ** is passed as the second argument is (a) in the cache and 
      ** (b) has an outstanding reference, then xUndo is either a no-op
      ** (if (a) is false) or simply expels the page from the cache (if (b)
      ** is false).
**������ò���ʧ�ܡ�����ҳ���ҳ����Ϊ�ڻ����еڶ����������ݵ���(a)��(b)��һ�����Եĵ�����,Ȼ��xUndoҪô����Ϊ��(���(a)�Ǽٵ�)��ӻ������ų�һЩҳ(���(b) �����Ǽٵ�)��
      ** If the upper layer is doing a rollback, it is guaranteed that there
      ** are no outstanding references to any page other than page 1. And
      ** page 1 is never written to the log until the transaction is
      ** committed. As a result, the call to xUndo may not fail.
      */����ϲ����ع�,���Ǳ�֤û�����Եĵ�1ҳ������κ�ҳ������á��͵�1ҳ����д����־,ֱ���������,����xUndo���ܲ���ʧ��
      assert( walFramePgno(pWal, iFrame)!=1 );
      rc = xUndo(pUndoCtx, walFramePgno(pWal, iFrame));
    }
    walCleanupHash(pWal); �����ϣ
  }
  assert( rc==SQLITE_OK ); ���rc��Ϊsqlite����ok 
  return rc; ����
}

/* 
** Argument aWalData must point to an array of WAL_SAVEPOINT_NDATA u32 
** values. This function populates the array with values required to 
** "rollback" the write position of the WAL handle back to the current 
** point in the event of a savepoint rollback (via WalSavepointUndo()).

*/ ����aWalData ���� ָ��WAL_SAVEPOINT_NDATA ���飬Ϊu32��

// ��������㣬������ع�ʱ�������������㣬�ͻ��󽵵ͻع����ۡ�����WAL_SAVEPOINT_NDATA��ֵ��aWALData��
void sqlite3WalSavepoint(Wal *pWal, u32 *aWalData){
  assert( pWal->writeLock ); ���Wal������
  aWalData[0] = pWal->hdr.mxFrame; Ϊawaldata�����鸳ֵ
  aWalData[1] = pWal->hdr.aFrameCksum[0];
  aWalData[2] = pWal->hdr.aFrameCksum[1];
  aWalData[3] = pWal->nCkpt;
}

/* 
** Move the write position of the WAL back to the point identified by
** the values in the aWalData[] array. aWalData must point to an array
** of WAL_SAVEPOINT_NDATA u32 values that has been previously populated
** by a call to WalSavepoint().
*/��д��λ���ƻص������λ�á�
int sqlite3WalSavepointUndo(Wal *pWal, u32 *aWalData){
  int rc = SQLITE_OK; ����rc��ֵΪok

  assert( pWal->writeLock ); �ж�Wal���Ƿ�����
  assert( aWalData[3]!=pWal->nCkpt || aWalData[0]<=pWal->hdr.mxFrame ); �ж�aWalData��Wal�еĲ����Ƿ����

  if( aWalData[3]!=pWal->nCkpt ){ 
    /* This savepoint was opened immediately after the write-transaction
    ** was started. Right after that, the writer decided to wrap around
    ** to the start of the log. Update the savepoint values to match.
    */ ��һ��д����ʼ����������Ҳ�������򿪣���֮��,д���������ع���־��ʼ�������±����ֵ������ƥ�䡣
    aWalData[0] = 0;
    aWalData[3] = pWal->nCkpt;
  }

  if( aWalData[0]<pWal->hdr.mxFrame ){  ������顾0����ֵС��Wal�����֡��
    pWal->hdr.mxFrame = aWalData[0]; ����ֵ
    pWal->hdr.aFrameCksum[0] = aWalData[1]; ���Ĳ���ֵ
    pWal->hdr.aFrameCksum[1] = aWalData[2];
    walCleanupHash(pWal); ���ú���
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
**�������������֮ǰд��ܼ�����־�ļ����෴,������Ƿ���ӵ�ǰ��־�ļ�,���Ը��ǵĿ�ʼ������־�ļ�����֡.���������, �⼯pWal - > hdr.mxFrameΪ0��������,pWal - > hdr��mxFrame���䡣
** SQLITE_OK is returned if no error is encountered (regardless of whether
** or not pWal->hdr.mxFrame is modified). An SQLite error code is returned
** if an error occurs.�ɹ����أ�������
*/
static int walRestartLog(Wal *pWal){
  int rc = SQLITE_OK; 
  int cnt; ���¶��Ĵ���

  if( pWal->readLock==0 ){ �������Ϊ0
    volatile WalCkptInfo *pInfo = walCkptInfo(pWal); ��ȡУ����Ϣ
    assert( pInfo->nBackfill==pWal->hdr.mxFrame ); ������߲�ͬ����ֹ����
    if( pInfo->nBackfill>0 ){  
      u32 salt1;     ����32Ϊ�ı���
      sqlite3_randomness(4, &salt1); ���ú���
      rc = walLockExclusive(pWal, WAL_READ_LOCK(1), WAL_NREADER-1); ���ü�������
      if( rc==SQLITE_OK ){ ����ɹ�
        /* If all readers are using WAL_READ_LOCK(0) (in other words if no
        ** readers are currently using the WAL), then the transactions
        ** frames will overwrite the start of the existing log. Update the
        ** wal-index header to reflect this. ������еĶ�����WAL_READ_LOCK(0)������ô,����֡����д���е���־�ġ�������wal-indexͷ����ӳ��һ��
        **
        ** In theory it would be Ok to update the cache of the header only
        ** at this point. But updating the actual wal-index header is also
        ** safe and means there is no special case for sqlite3WalUndo()
        ** to handle if this transaction is rolled back. ������˵���Գɹ����и��£�
        */
        int i;                    /* Loop counter */ ѭ������
        u32 *aSalt = pWal->hdr.aSalt;       /* Big-endian salt values */ ��ȡ����ֵ

        pWal->nCkpt++; �Լ�
        pWal->hdr.mxFrame = 0; wal�����֡Ϊ0
        sqlite3Put4byte((u8*)&aSalt[0], 1 + sqlite3Get4byte((u8*)&aSalt[0])); ���ú���
        aSalt[1] = salt1;  ��ֵ
        walIndexWriteHdr(pWal);���ú���
        pInfo->nBackfill = 0; ��ֵ
        pInfo->aReadMark[1] = 0; ��ֵ
        for(i=2; i<WAL_NREADER; i++) pInfo->aReadMark[i] = READMARK_NOT_USED; ��ֵ
        assert( pInfo->aReadMark[0]==0 ); �ж�
        walUnlockExclusive(pWal, WAL_READ_LOCK(1), WAL_NREADER-1); ����
      }else if( rc!=SQLITE_BUSY ){
        return rc;
      }
    }
    walUnlockShared(pWal, WAL_READ_LOCK(0)); ���������
    pWal->readLock = -1; ���Ĳ���ֵ
    cnt = 0; ��ֵ
    do{
      int notUsed;
      rc = walTryBeginRead(pWal, &notUsed, 1, ++cnt);
    }while( rc==WAL_RETRY );��ʼ��
    assert( (rc&0xff)!=SQLITE_BUSY ); /* BUSY not possible when useWal==1 */
    testcase( (rc&0xff)==SQLITE_IOERR );����
    testcase( rc==SQLITE_PROTOCOL );����
    testcase( rc==SQLITE_OK );����
  }
  return rc;
}

/*
** Information about the current state of the WAL file and where
** the next fsync should occur - passed from sqlite3WalFrames() into
** walWriteToLog(). ��ǰWal�ļ�����״̬�ͺδ�����ͬ��ͨ��sqlite3WalFrames()ת���� walWriteToLog()����Ϣ��
*/
typedef struct WalWriter {
  Wal *pWal;                   /* The complete WAL information */ ����Wal ָ�����
  sqlite3_file *pFd;           /* The WAL file to which we write */ Wal
  sqlite3_int64 iSyncPoint;    /* Fsync at this offset */ 
  int syncFlags;               /* Flags for the fsync */ͬ���ı�־
  int szPage;                  /* Size of one page */ ҳ�Ĵ�С
} WalWriter;

/*
** Write iAmt bytes of content into the WAL file beginning at iOffset.
** Do a sync when crossing the p->iSyncPoint boundary.
** ͨ��ƫ����Wal�ļ���IAmt �ֽڡ�����p - > iSyncPoint�߽�ʱͬ����
** In other words, if iSyncPoint is in between iOffset and iOffset+iAmt,
** first write the part before iSyncPoint, then sync, then write the
** rest.
*/
static int walWriteToLog(
  WalWriter *p,              /* WAL to write to */ WalWriter ָ�����
  void *pContent,            /* Content to be written */ Ҫд������
  int iAmt,                  /* Number of bytes to write */ д�����ֽ�
  sqlite3_int64 iOffset      /* Start writing at this offset */ ��ʼд
){
  int rc; ����ֵ
  if( iOffset<p->iSyncPoint && iOffset+iAmt>=p->iSyncPoint ){                                    P->ISyncPoint �Ƿ���ƫ�ƺ�ƫ�ƺ�֮��
    int iFirstAmt = (int)(p->iSyncPoint - iOffset); �����һ��ƫ����
    rc = sqlite3OsWrite(p->pFd, pContent, iFirstAmt, iOffset); ����ϵͳ������д������
    if( rc ) return rc; �Ƿ�ɹ�
    iOffset += iFirstAmt; ����IOffset��ֵ
    iAmt -= iFirstAmt; ���� iAMt��ֵ
    pContent = (void*)(iFirstAmt + (char*)pContent); ��ȡҪд����
    assert( p->syncFlags & (SQLITE_SYNC_NORMAL|SQLITE_SYNC_FULL) ); �ж��Ƿ�ɹ������ɹ�����ֹ����
    rc = sqlite3OsSync(p->pFd, p->syncFlags);����ϵͳͬ��������
    if( iAmt==0 || rc ) return rc;  ����ֵ
  }
  rc = sqlite3OsWrite(p->pFd, pContent, iAmt, iOffset); ����д�뺯��
  return rc;
}

/*
** Write out a single frame of the WAL д��Wal�ĵ�֡
*/
static int walWriteOneFrame(
  WalWriter *p,               /* Where to write the frame */ ����WalWrite ָ��
  PgHdr *pPage,               /* The page of the frame to be written */ Ҫд���֡ҳ
  int nTruncate,              /* The commit flag.  Usually 0.  >0 for commit */ �ύ���
  sqlite3_int64 iOffset       /* Byte offset at which to write */ ƫ���ֽ�
){
  int rc;                         /* Result code from subfunctions */���Ӻ�������
  void *pData;                    /* Data actually written */ʵ��Ҫд����
  u8 aFrame[WAL_FRAME_HDRSIZE];   /* Buffer to assemble frame-header in */֡ͷ����Ҫ����Ļ�����
#if defined(SQLITE_HAS_CODEC)
  if( (pData = sqlite3PagerCodec(pPage))==0 ) return SQLITE_NOMEM; ��ȡ����ҳ���ݵ�ָ�룬�����������null
#else
  pData = pPage->pData;��pData��ֵ
#endif
  walEncodeFrame(p->pWal, pPage->pgno, nTruncate, pData, aFrame);���ú������б��롣
  rc = walWriteToLog(p, aFrame, sizeof(aFrame), iOffset); ���ú���д�뵽Wal�С�
  if( rc ) return rc;
  /* Write the page data */ ��дҳ������
  rc = walWriteToLog(p, pData, p->szPage, iOffset+sizeof(aFrame));���ú���д�뵽ҳ��
  return rc;
}

/* 
** Write a set of frames to the log. The caller must hold the write-lock
** on the log file (obtained using sqlite3WalBeginWriteTransaction()).��д��ܼ�����־�С������߱������д������־�ļ���
*/
int sqlite3WalFrames(
  Wal *pWal,                      /* Wal handle to write to */ �������
  int szPage,                     /* Database page-size in bytes */ ���ݿ�ҳ�Ĵ�С����λ���ֽ�
  PgHdr *pList,                   /* List of dirty pages to write */  ��ҳд���б�
  Pgno nTruncate,                 /* Database size after this commit */ ����ύ�����ݿ��С
  int isCommit,                   /* True if this is a commit */����ύ����Ϊ�档
  int sync_flags                  /* Flags to pass to OsSync() (or 0) */ ͬ���ı�־
){
  int rc;                         /* Used to catch return codes */ ������
  u32 iFrame;                     /* Next frame address */ ��һ֡��ַ
  PgHdr *p;                       /* Iterator to run through pList with. */ ����pList ����
  PgHdr *pLast = 0;               /* Last frame in list */ �����������һ֡
  int nExtra = 0;                 /* Number of extra copies of last page */ 
  int szFrame;                    /* The size of a single frame */ ��֡�Ĵ�С
  i64 iOffset;                    /* Next byte to write in WAL file */ ƫ���ֽ�
  WalWriter w;                    /* The writer */ WalW�ı���

  assert( pList );�ж������Ƿ�Ϊ�գ�Ϊ������ֹ����
  assert( pWal->writeLock ); �ж��Ǽ���

  /* If this frame set completes a transaction, then nTruncate>0.  If
  ** nTruncate==0 then this frame set does not complete the transaction. */
  assert( (isCommit!=0)==(nTruncate!=0) ); ����ύΪ�棬Wal��Ϊ0 

#if defined(SQLITE_TEST) && defined(SQLITE_DEBUG)
  { int cnt; 
  for(cnt=0, p=pList; p; p=p->pDirty, cnt++){} 
    WALTRACE(("WAL%p: frame write begin. %d frames. mxFrame=%d. %s\n",
              pWal, cnt, pWal->hdr.mxFrame, isCommit ? "Commit" : "Spill"));
  }
#endif

  /* See if it is possible to write these frames into the start of the
  ** log file, instead of appending to it at pWal->hdr.mxFrame.
  */�Ƿ����д��Щ��Ŀ�ʼ��־�ļ�,�����Ǹ�����pWal - > hdr.mxFrame��
  if( SQLITE_OK!=(rc = walRestartLog(pWal)) ){
    return rc;
  }

  /* If this is the first frame written into the log, write the WAL
  ** header to the start of the WAL file. See comments at the top of
  ** this source file for a description of the WAL header format.
  */������ǵ�һ֡д����־,Walͷ����д��WAL�ļ����������Դ�ļ�������WAL�����ʽ
  iFrame = pWal->hdr.mxFrame; ��ȡWal�е����֡����ֵ
  if( iFrame==0 ){
    u8 aWalHdr[WAL_HDRSIZE];      /* Buffer to assemble wal-header in */
    u32 aCksum[2];                /* Checksum for wal-header */

    sqlite3Put4byte(&aWalHdr[0], (WAL_MAGIC | SQLITE_BIGENDIAN)); ���ú��� ��ΪaWalHdr[] ��ֵ
    sqlite3Put4byte(&aWalHdr[4], WAL_MAX_VERSION);
    sqlite3Put4byte(&aWalHdr[8], szPage);
    sqlite3Put4byte(&aWalHdr[12], pWal->nCkpt);
    if( pWal->nCkpt==0 ) sqlite3_randomness(8, pWal->hdr.aSalt); ���У����ϢΪ0��ΪaSalta���8���ֽ�
    memcpy(&aWalHdr[16], pWal->hdr.aSalt, 8); �����ַ�����ֵ
    walChecksumBytes(1, aWalHdr, WAL_HDRSIZE-2*4, 0, aCksum); ����У��ͺ���
    sqlite3Put4byte(&aWalHdr[24], aCksum[0]); ���ú��� ��ΪaWalHdr[] ��ֵ
    sqlite3Put4byte(&aWalHdr[28], aCksum[1]);
    
    pWal->szPage = szPage; ΪWal������ֵ
    pWal->hdr.bigEndCksum = SQLITE_BIGENDIAN;
    pWal->hdr.aFrameCksum[0] = aCksum[0];
    pWal->hdr.aFrameCksum[1] = aCksum[1];
    pWal->truncateOnCommit = 1;

    rc = sqlite3OsWrite(pWal->pWalFd, aWalHdr, sizeof(aWalHdr), 0); ����ϵͳд�뺯��
    WALTRACE(("WAL%p: wal-header write %s\n", pWal, rc ? "failed" : "ok"));
    if( rc!=SQLITE_OK ){
      return rc;
    }

    /* Sync the header (unless SQLITE_IOCAP_SEQUENTIAL is true or unless
    ** all syncing is turned off by PRAGMA synchronous=OFF).  Otherwise
    ** an out-of-order write following a WAL restart could result in
    ** database corruption.  See the ticket: ͬ��ͷ���ݣ�������SQLITE_IOCAP_SEQUENTIAL ����Ļ� ���е�ͬ������PRAGMA synchronous=OFF�رգ�����һ������дWAL��������ܵ��¡���* *���ݿ���
    **
    **     http://localhost:591/sqlite/info/ff5be73dee
    */
    if( pWal->syncHeader && sync_flags ){  ���Walden�Ĳ���Ϊ����ͬ�����Ϊ��
      rc = sqlite3OsSync(pWal->pWalFd, sync_flags & SQLITE_SYNC_MASK);����ϵͳͬ������
      if( rc ) return rc;
    }
  }
  assert( (int)pWal->szPage==szPage ); ���Wal�Ĳ���szPage �뵱ǰszPage��ͬ������ֹ����

  /* Setup information needed to write frames into the WAL */����֡д����Wal�������Ϣ
  w.pWal = pWal; ����WalWrite�ṹ��Ĳ���ֵ
  w.pFd = pWal->pWalFd;
  w.iSyncPoint = 0;
  w.syncFlags = sync_flags;
  w.szPage = szPage;
  iOffset = walFrameOffset(iFrame+1, szPage); ����ƫ����
  szFrame = szPage + WAL_FRAME_HDRSIZE; 

  /* Write all frames into the log file exactly once */һ�ξ�ȷ�İ�����֡д����־�ļ�
  for(p=pList; p; p=p->pDirty){ ��������б���
    int nDbSize;   /* 0 normally.  Positive == commit flag */ ����λ0 ������Ϊcommit flag
    iFrame++; 
    assert( iOffset==walFrameOffset(iFrame, szPage) ); ���ƫ����Ϊ
    nDbSize = (isCommit && p->pDirty==0) ? nTruncate : 0; ����ύ��P�������ݱ��Ϊ0 �� ����nTruncate������Ϊ0
    rc = walWriteOneFrame(&w, p, nDbSize, iOffset); ���ú���д�뵽oneFrame
    if( rc ) return rc;
    pLast = p; ���¸�pLast��ֵ
    iOffset += szFrame;ƫ����
  }

  /* If this is the end of a transaction, then we might need to pad
  ** the transaction and/or sync the WAL file.
  **���������Ľ���,��ô���ǿ�����Ҫ������/��ͬ��WAL�ļ�
  ** Padding and syncing only occur if this set of frames complete a
  ** transaction and if PRAGMA synchronous=FULL.  If synchronous==NORMAL
  ** or synchonous==OFF, then no padding or syncing are needed.
  ** ����ͬ��ֻ�����������֡�������,�������ָʾͬ��=FULL�����ͬ��= ������synchonous = =OFF,Ȼ����Ҫ����ͬ����
  ** If SQLITE_IOCAP_POWERSAFE_OVERWRITE is defined, then padding is not
  ** needed and only the sync is done.  If padding is needed, then the
  ** final frame is repeated (with its commit mark) until the next sector 
  ** boundary is crossed.  Only the part of the WAL prior to the last
  ** sector boundary is synced; the part of the last frame that extends
  ** past the sector boundary is written after the sync.
  */���SQLITE_IOCAP_POWERSAFE_OVERWRITE  �����壬�� padding����Ҫ��ֻ����ͬ���������Ҫ���,Ȼ�����һ֡�ظ�(���ύ���),ֱ����һ�����ֱ߽罻�档ֻ��WAL֮ǰ����һ���ֱ߽���ͬ����;���һ֡��չ��һ���ֹ�ȥ���ֱ߽���ͬ����д��
  if( isCommit && (sync_flags & WAL_SYNC_TRANSACTIONS)!=0 ){ 
    if( pWal->padToSectorBoundary ){
      int sectorSize = sqlite3OsSectorSize(pWal->pWalFd);ͨ������ϵͳ���� ��ȡ
      w.iSyncPoint = ((iOffset+sectorSize-1)/sectorSize)*sectorSize;
      while( iOffset<w.iSyncPoint ){
        rc = walWriteOneFrame(&w, pLast, nTruncate, iOffset);���ú���д�뵽OneFrame
        if( rc ) return rc;
        iOffset += szFrame;
        nExtra++;
      }
    }else{
      rc = sqlite3OsSync(w.pFd, sync_flags & SQLITE_SYNC_MASK);
    }
  }

  /* If this frame set completes the first transaction in the WAL and
  ** if PRAGMA journal_size_limit is set, then truncate the WAL to the
  ** journal size limit, if possible.
  */������֡������ɵ�һ������,�������ָʾjournal_size_limit������,Ȼ��ض�WAL��������־��С,
  if( isCommit && pWal->truncateOnCommit && pWal->mxWalSize>=0 ){
    i64 sz = pWal->mxWalSize; ��ȡwal ���ܵ����ֵ
    if( walFrameOffset(iFrame+nExtra+1, szPage)>pWal->mxWalSize ){     
      sz = walFrameOffset(iFrame+nExtra+1, szPage); 
    }
    walLimitSize(pWal, sz);���ú�������Wal�Ĵ�С
    pWal->truncateOnCommit = 0;
  }

  /* Append data to the wal-index. It is not necessary to lock the 
  ** wal-index to do this as the SQLITE_SHM_WRITE lock held on the wal-index
  ** guarantees that there are no other writers, and no data that may
  ** be in use by existing readers is being overwritten.
  */��������ӵ�wal-index��û�б�Ҫ����wal-indexΪ��wal-index SQLITE_SHM_WRITE������֤û������д����,��û�����ݱ����еĶ�����ʹ���ұ����ǡ�
  iFrame = pWal->hdr.mxFrame;
  for(p=pList; p && rc==SQLITE_OK; p=p->pDirty){   ��������б���
    iFrame++;
    rc = walIndexAppend(pWal, iFrame, p->pgno); ���ú���
  }
  while( rc==SQLITE_OK && nExtra>0 ){ �����óɹ�
    iFrame++;
    nExtra--;
    rc = walIndexAppend(pWal, iFrame, pLast->pgno);
  }

  if( rc==SQLITE_OK ){
    /* Update the private copy of the header. */ ����ͷ���ݵ�˽�и���
    pWal->hdr.szPage = (u16)((szPage&0xff00) | (szPage>>16));
    testcase( szPage<=32768 ); ����Szpage���ڵķ�Χ
    testcase( szPage>=65536 );
    pWal->hdr.mxFrame = iFrame;  ����Wal�в���ֵ
    if( isCommit ){  ����ύ��־λ��
      pWal->hdr.iChange++; ��Wal��IChange�Լ�
      pWal->hdr.nPage = nTruncate; Waldennpage ��ֵ 
    }
    /* If this is a commit, update the wal-index header too. */�������һ���ύ,����wal-indexͷ
    if( isCommit ){ ����ύ��־Ϊ��
      walIndexWriteHdr(pWal);
      pWal->iCallback = iFrame;
    }
  }

  WALTRACE(("WAL%p: frame write %s\n", pWal, rc ? "failed" : "ok"));
  return rc;
}

/* 
** This routine is called to implement sqlite3_wal_checkpoint() and
** related interfaces.������򱻵�������sqlite3_wal_checkpoint()����ؽӿڡ�
**
** Obtain a CHECKPOINT lock and then backfill as much information as
** we can from WAL into the database.��ȡһ��checkpoint ���� Ȼ�� �����ܶ�Ļ�����Ϣ��������
**
** If parameter xBusy is not NULL, it is a pointer to a busy-handler
** callback. In this case this function runs a blocking checkpoint.
*/������� Xbusy ��Ϊ�գ��и�Busy-Handler ָ�� �� �����������,�ú���������������
int sqlite3WalCheckpoint(
  Wal *pWal,                      /* Wal connection */ ���� wal ָ��
  int eMode,                      /* PASSIVE, FULL or RESTART */ ��һ��ģʽ
  int (*xBusy)(void*),            /* Function to call when busy */ ��æʱ����
  void *pBusyArg,                 /* Context argument for xBusyHandler */ xBusyHandler�����»���
  int sync_flags,                 /* Flags to sync db file with (or 0) */ͬ����־
  int nBuf,                       /* Size of temporary buffer */��ʱ�������Ĵ�С
  u8 *zBuf,                       /* Temporary buffer to use */ ��ʱ������
  int *pnLog,                     /* OUT: Number of frames in WAL */��Wal�еĸ���
  int *pnCkpt                     /* OUT: Number of backfilled frames in WAL */��WAL�л���ĸ���
){
  int rc;                         /* Return code */ ������
  int isChanged = 0;              /* True if a new wal-index header is loaded */  ���һ���µ�wal-index�����أ���Ϊ��
  int eMode2 = eMode;             /* Mode to pass to walCheckpoint() */ ͨ��WalCheckpoint�õ�Mode

  assert( pWal->ckptLock==0 ); ����мӼ���������
  assert( pWal->writeLock==0 ); ����м�д������

  if( pWal->readOnly ) return SQLITE_READONLY; ���Wal��readonly ��Ϊ0
  WALTRACE(("WAL%p: checkpoint begins\n", pWal));
  rc = walLockExclusive(pWal, WAL_CKPT_LOCK, 1); ����
  if( rc ){
    /* Usually this is SQLITE_BUSY meaning that another thread or process
    ** is already running a checkpoint, or maybe a recovery.  But it might
    ** also be SQLITE_IOERR. */ һ����˵SQLITE_BUSY��ζ�� ��һ���̻߳�����Ѿ�������checkpoint������лָ��� Ҳ������SQLITE_IOERR
    return rc;
  }
  pWal->ckptLock = 1; ΪckptLock��ֵ

  /* If this is a blocking-checkpoint, then obtain the write-lock as well
  ** to prevent any writers from running while the checkpoint is underway.
  ** This has to be done before the call to walIndexReadHdr() below.�������һ��blocking-checkpoint,Ȼ����д�����Է�ֹ�κ�д�����������С�����* *����������walIndexReadHdr()֮ǰ����
  **
  ** If the writer lock cannot be obtained, then a passive checkpoint is
  ** run instead. Since the checkpointer is not holding the writer lock,
  ** there is no point in blocking waiting for any readers. Assuming no 
  ** other error occurs, this function will return SQLITE_BUSY to the caller.
  */����޷����д��,��ôһ�������ļ��㽫�������С�����checkpoint�޷������������Ǻ�������������ȴ��κζ��ߣ�����û������������ʱ,�ú���������SQLITE_BUSY�������ߡ�
  if( eMode!=SQLITE_CHECKPOINT_PASSIVE ){  ���ģʽ����SQLITE_CHECKPOINT_PASSIVE 
    rc = walBusyLock(pWal, xBusy, pBusyArg, WAL_WRITE_LOCK, 1); ��ȡ��
    if( rc==SQLITE_OK ){ ������óɹ�
      pWal->writeLock = 1;  ���Ĳ���ֵ
    }else if( rc==SQLITE_BUSY ){ �������ʱSqlite_busy
      eMode2 = SQLITE_CHECKPOINT_PASSIVE;  ��������ֵ
      rc = SQLITE_OK;
    }
  }

  /* Read the wal-index header. */ ��ȡ Wal-index ͷ
  if( rc==SQLITE_OK ){  ���
    rc = walIndexReadHdr(pWal, &isChanged); 
  }

  /* Copy data from the log to the database file. */ ����־�ļ��е����ݿ����� �����ļ��� ��
  if( rc==SQLITE_OK ){
    if( pWal->hdr.mxFrame && walPagesize(pWal)!=nBuf ){  �����Ϣ��ƥ��
      rc = SQLITE_CORRUPT_BKPT; ����
    }else{
      rc = walCheckpoint(pWal, eMode2, xBusy, pBusyArg, sync_flags, zBuf); ���ü��㺯��
    }

    /* If no error occurred, set the output variables. */ ���û��error �����������������
    if( rc==SQLITE_OK || rc==SQLITE_BUSY ){ 
      if( pnLog ) *pnLog = (int)pWal->hdr.mxFrame; ��ȡFrame�ĸ���
      if( pnCkpt ) *pnCkpt = (int)(walCkptInfo(pWal)->nBackfill); ����Frame����
    }
  }

  if( isChanged ){ �ж� �Ƿ�ı�
    /* If a new wal-index header was loaded before the checkpoint was 
    ** performed, then the pager-cache associated with pWal is now
    ** out of date. So zero the cached wal-index header to ensure that
    ** next time the pager opens a snapshot on this database it knows that
    ** the cache needs to be reset.
    */
    memset(&pWal->hdr, 0, sizeof(WalIndexHdr)); ����
  }

  /* Release the locks. */ �ͷ���
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
**���ص�ֵ���ݸ�sqlite3_wal_hook�ص��У�
**��WAL֡�е�����������Ҫ�ύ
**��Ϊ sqlite3WalCallback���������á�
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
**����������������ı�WAL��ϵͳ���������
**��locking_mode��**= EXCLUSIVE��
** If op is zero, then attempt to change from locking_mode=EXCLUSIVE
** into locking_mode=NORMAL.  This means that we must acquire a lock
** on the pWal->readLock byte.  If the WAL is already in locking_mode=NORMAL
** or if the acquisition of the lock fails, then return 0.  If the
** transition out of exclusive-mode is successful, return 1.  This
** operation must occur while the pager is still holding the exclusive
** lock on the main database file.
**
** If op is one, then change from locking_mode=NORMAL into 
** locking_mode=EXCLUSIVE.  This means that the pWal->readLock must
** be released.  Return 1 if the transition is made and 0 if the
** WAL is already in exclusive-locking mode - meaning that this
** routine is a no-op.  The pager must already hold the exclusive lock
** on the main database file before invoking this operation.
**
** If op is negative, then do a dry-run of the op==1 case but do
** not actually change anything. The pager uses this to see if it
** should acquire the database exclusive lock prior to invoking
** the op==1 case.
*/
int sqlite3WalExclusiveMode(Wal *pWal, int op){
  int rc;-
  assert( pWal->writeLock==0 );
  assert( pWal->exclusiveMode!=WAL_HEAPMEMORY_MODE || op==-1 );

  /* pWal->readLock is usually set, but might be -1 if there was a 
  ** prior error while attempting to acquire are read-lock. This cannot 
  ** happen if the connection is actually in exclusive mode (as no xShmLock
  ** locks are taken in this case). Nor should the pager attempt to
  ** upgrade to exclusive-mode following such an error.
  */
  assert( pWal->readLock>=0 || pWal->lockError );
  assert( pWal->readLock>=0 || (op<=0 && pWal->exclusiveMode==0) );

  if( op==0 ){//���op==0  ��locking_mode=NORMAL �ı�Ϊlocking_mode=EXCLUSIVE
    if( pWal->exclusiveMode ){
      pWal->exclusiveMode = 0;
      if( walLockShared(pWal, WAL_READ_LOCK(pWal->readLock))!=SQLITE_OK ){
        pWal->exclusiveMode = 1;
      }
      rc = pWal->exclusiveMode==0;
    }else{
      /* Already in locking_mode=NORMAL *///���locking_mode=NORMAL ����ֵΪ0
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
** WAL module is using shared-memory, return false. ����true,���������null��WALģ��wal-indexʹ�ö��ڴ档����,���������NULL��WALģ����ʹ�ù����ڴ�,����false
*/
int sqlite3WalHeapMemory(Wal *pWal){
  return (pWal && pWal->exclusiveMode==WAL_HEAPMEMORY_MODE );
}

#ifdef SQLITE_ENABLE_ZIPVFS
/*
** If the argument is not NULL, it points to a Wal object that holds a
** read-lock. This function returns the database page-size if it is known,
** or zero if it is not (or if pWal is NULL).�����������,��ָ��Wal������ж�����������򷵻����ݿ�ҳ��С������0 
*/
int sqlite3WalFramesize(Wal *pWal){
  assert( pWal==0 || pWal->readLock>=0 ); ���WalΪ0������ж�����
  return (pWal ? pWal->szPage : 0); �������ݿ�ҳ��С��0
}
#endif

#endif /* #ifndef SQLITE_OMIT_WAL */
