/*
** 2004 April 6
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file implements a external (disk-based) database using BTrees.
** For a detailed discussion of BTrees, refer to
**
**     Donald E. Knuth, THE ART OF COMPUTER PROGRAMMING, Volume 3:
**     "Sorting And Searching", pages 473-480. Addison-Wesley
**     Publishing Company, Reading, Massachusetts.
**
** The basic idea is that each page of the file contains N database
** entries and N+1 pointers to subpages.
**
**   ----------------------------------------------------------------
**   |  Ptr(0) | Key(0) | Ptr(1) | Key(1) | ... | Key(N-1) | Ptr(N) |
**   ----------------------------------------------------------------
**
** All of the keys on the page that Ptr(0) points to have values less
** than Key(0).  All of the keys on page Ptr(1) and its subpages have
** values greater than Key(0) and less than Key(1).  All of the keys
** on Ptr(N) and its subpages have values greater than Key(N-1).  And
** so forth.
**
** Finding a particular key requires reading O(log(M)) pages from the 
** disk where M is the number of entries in the tree.
**
** In this implementation, a single file can hold one or more separate 
** BTrees.  Each BTree is identified by the index of its root page.  The
** key and data for any entry are combined to form the "payload".  A
** fixed amount of payload can be carried directly on the database
** page.  If the payload is larger than the preset amount then surplus
** bytes are stored on overflow pages.  The payload for an entry
** and the preceding pointer are combined to form a "Cell".  Each 
** page has a small header which contains the Ptr(N) pointer and other
** information such as the size of key and data.
**
** FORMAT DETAILS
**
** The file is divided into pages.  The first page is called page 1,
** the second is page 2, and so forth.  A page number of zero indicates
** "no such page".  The page size can be any power of 2 between 512 and 65536.
** Each page can be either a btree page, a freelist page, an overflow
** page, or a pointer-map page.
**
** The first page is always a btree page.  The first 100 bytes of the first
** page contain a special header (the "file header") that describes the file.
** The format of the file header is as follows:
**   偏移量   大小    说明
**   OFFSET   SIZE    DESCRIPTION
**      0      16     Header string: "SQLite format 3\000"  //头字符串，如果不改源程序，此字符串永远是"SQLite format 3"
**     16       2     Page size in bytes.                   //页大小(以字节为单位)。
**     18       1     File format write version             //文件格式版本(写)。如果该值大于1，表示文件为只读。
**     19       1     File format read version              //文件格式版本(读)。如果该值大于1，SQLite认为文件格式错，拒绝打开此文件。
**     20       1     Bytes of unused space at the end of each page //每页尾部保留空间的大小。(留作它用，默认为0。)
**     21       1     Max embedded payload fraction         //Btree内部页中一个单元最多能够使用的空间。255意味着100%，默认值为0x40，即64(25%)，这保证了一个结点(页)至少有4个单元。
**     22       1     Min embedded payload fraction         //Btree内部页中一个单元使用空间的最小值。默认值为0x20，即32(12.5%)。
**     23       1     Min leaf payload fraction             //Btree叶子页中一个单元使用空间的最小值。默认值为0x20，即32(12.5%)。
**     24       4     File change counter                   //文件修改计数，通常被事务使用，由事务增加其值。SQLite用此域的值验证内存缓冲区中数据的有效性。
**     28       4     Reserved for future use               //留作以后使用。
**     32       4     First freelist page                   //空闲页列表首指针。
**     36       4     Number of freelist pages in the file  //文件内空闲页的数量。
**     40      60     15 4-byte meta values passed to higher layers   //15个4字节的元数据变量。
**
**     40       4     Schema cookie       //Schema版本：每次schema改变(创建或删除表、索引、视图或触发器等对象，造成sqlite_master表被修改)时，此值+1。
**     44       4     File format of schema layer //模式层的文件格式。当前允许值为1~4，超过此范围，将被认为是文件格式错。
**     48       4     Size of page cache          //文件缓存大小
**     52       4     Largest root-page (auto/incr_vacuum) //对于auto-vacuum数据库，此域为数据库中根页编号的最大值，非0。对于非auto-vacuum数据库，此域值为0。
**     56       4     1=UTF-8 2=UTF16le 3=UTF16be
**     60       4     User version                //此域值供用户应用程序自由存取，其含义也由用户定义。
**     64       4     Incremental vacuum mode     //对于auto-vacuum数据库，如果是Incremental vacuum模式，此域值为1。否则，此域值为0。
**     68       4     unused    //未使用
**     72       4     unused    //未使用
**     76       4     unused    //未使用
**
** All of the integer values are big-endian (most significant byte first).
** 整型值都是大端对齐
** The file change counter is incremented when the database is changed
** This counter allows other processes to know when the file has changed
** and thus when they need to flush their cache.
**
** The max embedded payload fraction is the amount of the total usable
** space in a page that can be consumed by a single cell for standard
** B-tree (non-LEAFDATA) tables.  A value of 255 means 100%.  The default
** is to limit the maximum cell size so that at least 4 cells will fit
** on one page.  Thus the default max embedded payload fraction is 64.
**
** If the payload for a cell is larger than the max payload, then extra
** payload is spilled to overflow pages.  Once an overflow page is allocated,
** as many bytes as possible are moved into the overflow pages without letting
** the cell size drop below the min embedded payload fraction.
**
** The min leaf payload fraction is like the min embedded payload fraction
** except that it applies to leaf nodes in a LEAFDATA tree.  The maximum
** payload fraction for a LEAFDATA tree is always 100% (or 255) and it
** not specified in the header.
**
** Each btree pages is divided into three sections:  The header, the
** cell pointer array, and the cell content area.  Page 1 also has a 100-byte
** file header that occurs before the page header.
** 每个B树页都别划分为三个部分：头部、单元指针数组、单元常量区域
**      |----------------------|
**      | file header 文件头   |   100 bytes.  Page 1 only.            //100字节，只有页1有该文件头
**      |----------------------|
**      | page header  页头    |   8 bytes for leaves.  12 bytes for interior nodes
**      |----------------------|                                       //留有8字节。内部节点有12字节。
**      | cell pointer         |   |  2 bytes per cell.  Sorted order. //每个单元两个字节，顺序排列。
**      | array  单元指针数组  |   |  Grows downward                   //向下增加
**      |                      |   v
**      |----------------------|
**      | unallocated          |
**      | space   未分配空白区 |
**      |----------------------|   ^  Grows upwards                    //向上增加
**      | cell content         |   |  Arbitrary order interspersed with freeblocks.
**      | area  单元内容区域   |   |  and free space fragments.        //任意顺序地散布在自由块。
**      |----------------------|
**
** 页头包含用来管理页的信息，它通常位于页的开始处。对于数据库文件的page 1，
** 页头始于第100个字节处，因为前100个字节是文件头(file header)。
** The page headers looks like this:  //页头有如下形式
**
**   OFFSET   SIZE     DESCRIPTION
**      0       1      Flags. 1: intkey, 2: zerodata, 4: leafdata, 8: leaf  //页类型标志。
**      1       2      byte offset to the first freeblock                   //第1个自由块的偏移量。
**      3       2      number of cells on this page                         //本页的单元数。
**      5       2      first byte of the cell content area                  //单元内容区的起始字节（地址）。
**      7       1      number of fragmented free bytes                      //碎片的字节数。
**      8       4      Right child (the Ptr(N) value).  Omitted on leaves.  //最右孩子的页号(the Ptr(n) value)。仅内部页有此域。
**
**     //如果leaf位被设置，则该页是一个叶子页，没有儿子；
**     //如果zerodata位被设置，则该页只有关键字，而没有数据；
**     //如果intkey位被设置，则关键字是整型；
**     //如果leafdata位设置，则tree只存储数据在叶子页。 
** The flags define the format of this btree page.  The leaf flag means that
** this page has no children.  The zerodata flag means that this page carries
** only keys and no data.  The intkey flag means that the key is a integer
** which is stored in the key size entry of the cell header rather than in
** the payload area.
**
** 这些标志定义该B树页的格式。叶标志意味着此页面没有孩子。该zerodata标志意味着
** 该页面进行只有关键字而没有数据。该intkey标志意味着，关键是一个整数
** 被存储在单元头的关键字大小条目，而不是在有效负载区域。
**
** The cell pointer array begins on the first byte after the page header.
** The cell pointer array contains zero or more 2-byte numbers which are
** offsets from the beginning of the page to the cell content in the cell
** content area.  The cell pointers occur in sorted order.  The system strives
** to keep free space after the last cell pointer so that new cells can
** be easily added without having to defragment the page.
** 在单元格指针数组开始在页头之后的第一个字节。在单元格指针数组包含零个或多个2字节数字
** 这些数字是从页面的开始到单元内容区域的单元内容的偏移单元指针是有序的。该系统尽量保持
** 最后一个单元指针后仍保持自由空间，以便于新的单元可以在无需进行碎片整理页面的情况下很容易的添加。
**
** Cell content is stored at the very end of the page and grows toward the
** beginning of the page.
** 单元内容存储在页面的最末端和朝向页面的开始方向增加
**
** Unused space within the cell content area is collected into a linked list of
** freeblocks.  Each freeblock is at least 4 bytes in size.  The byte offset
** to the first freeblock is given in the header.  Freeblocks occur in
** increasing order.  Because a freeblock must be at least 4 bytes in size,
** any group of 3 or fewer unused bytes in the cell content area cannot
** exist on the freeblock chain.  A group of 3 or fewer free bytes is called
** a fragment.  The total number of bytes in all fragments is recorded.
** in the page header at offset 7.
** 单元内容区域内未使用的空间被收集到空闲块的链接列表中。每个freeblock大小至少为4个字节。
** 字节偏移到在首部给出的第一个freeblock。 Freeblocks按顺序递增。因为一个freeblock必须至少4个字节的大小，
** 在单元格内容区域任何一组3个或更少的未使用的字节不能存在于freeblock链中。一组3个或更少的可用字节被称为的碎片。
** 所有碎片的字节的总数在页头偏移量7处被记录。
**
**    SIZE    DESCRIPTION
**      2     Byte offset of the next freeblock    //下一个空闲块的偏移量字节数
**      2     Bytes in this freeblock              //自由块的大小（字节）
**
** Cells are of variable length.  Cells are stored in the cell content area at
** the end of the page.  Pointers to the cells are in the cell pointer array
** that immediately follows the page header.  Cells is not necessarily
** contiguous or in order, but cell pointers are contiguous and in order.
**记录的指针值必须连续、有序
** Cell content makes use of variable length integers.  A variable
** length integer is 1 to 9 bytes where the lower 7 bits of each 
** byte are used.  The integer consists of all bytes that have bit 8 set and
** the first byte with bit 8 clear.  The most significant byte of the integer
** appears first.  A variable-length integer may not be more than 9 bytes long./变长整数不能超过9个字节
** As a special case, all 8 bytes of the 9th byte are used as data.  This
** allows a 64-bit integer to be encoded in 9 bytes.
**
**    0x00                      becomes  0x00000000
**    0x7f                      becomes  0x0000007f
**    0x81 0x00                 becomes  0x00000080
**    0x82 0x00                 becomes  0x00000100
**    0x80 0x7f                 becomes  0x0000007f
**    0x8a 0x91 0xd1 0xac 0x78  becomes  0x12345678
**    0x81 0x81 0x81 0x81 0x01  becomes  0x10204081
**
** Variable length integers are used for rowids and to hold the number of
** bytes of key and data in a btree cell.
**
** The content of a cell looks like this:
**
**    SIZE    DESCRIPTION
**      4     Page number of the left child. Omitted if leaf flag is set.
**     var    Number of bytes of data. Omitted if the zerodata flag is set.
**     var    Number of bytes of key. Or the key itself if intkey flag is set.
**      *     Payload                                                    //Payload内容，存储数据库中某个表一条记录的数据。
**      4     First page of the overflow chain.  Omitted if no overflow  //溢出页链表中第1个溢出页的页号。如果没有溢出页，无此域。
**
** Overflow pages form a linked list.  Each page except the last is completely
** filled with data (pagesize - 4 bytes).  The last page can have as little
** as 1 byte of data.
**
**    SIZE    DESCRIPTION
**      4     Page number of next overflow page
**      *     Data
**
** Freelist pages come in two subtypes: trunk pages and leaf pages.  The
** file header points to the first in a linked list of trunk page.  Each trunk
** page points to multiple leaf pages.  The content of a leaf page is
** unspecified.  A trunk page looks like this:
**
**    SIZE    DESCRIPTION
**      4     Page number of next trunk page
**      4     Number of leaf pointers on this page
**      *     zero or more pages numbers of leaves
*/
#include "sqliteInt.h"
/*sqliteInt.h头文件*/
/* The following value is the maximum cell size assuming a maximum page
** size give above.
*/
#define MX_CELL_SIZE(pBt)  ((int)(pBt->pageSize-8))
/*定义最大记录（有效载荷）的大小，其值是整型量*/
/* The maximum number of cells on a single page of the database.  This
** assumes a minimum cell size of 6 bytes  (4 bytes for the cell itself
** plus 2 bytes for the index to the cell in the page header).  Such
** small cells will be rare, but they are possible.
*/
#define MX_CELL(pBt) ((pBt->pageSize-8)/6)
/*每个页面的记录（有效载荷）数量的最大值*/

/* Forward declarations */
typedef struct MemPage MemPage;
typedef struct BtLock BtLock;
/*声明定义结构体类型的变量*/

/*
** This is a magic string that appears at the beginning of every
** SQLite database in order to identify the file as a real database.
**
** You can change this value at compile-time by specifying a
** -DSQLITE_FILE_HEADER="..." on the compiler command-line.  The
** header must be exactly 16 bytes including the zero-terminator so
** the string itself should be 15 characters long.  If you change
** the header, then your custom library will not be able to read 
** databases generated by the standard tools and the standard tools
** will not be able to read databases created by your custom library.
*/
#ifndef SQLITE_FILE_HEADER /* 123456789 123456 */
#  define SQLITE_FILE_HEADER "SQLite format 3"
#endif
/*主要目的是防止SQLITE_FILE_HEADER "SQLite format 3"的重复包含和编译

/*
** Page type flags.  An ORed combination of these flags appear as the
** first byte of on-disk image of every BTree page.
** 页面类型的标志，出现在每个B树页磁盘镜像的第一个字节.
*/
#define PTF_INTKEY    0x01
#define PTF_ZERODATA  0x02
#define PTF_LEAFDATA  0x04
#define PTF_LEAF      0x08
/*页头部定义的常量*/

/*
** As each page of the file is loaded into memory, an instance of the following
** structure is appended and initialized to zero.  This structure stores
** information about the page that is decoded from the raw file page.
** ** 每当文件的一个页加载到内存，下面结构的一个实例也会增加并被初始化为0。
** 这个结构存储有页的信息，这些信息从原始的文件页中解码得到的。
**
** The pParent field points back to the parent page.  This allows us to
** walk up the BTree from any leaf to the root.  Care must be taken to
** unref() the parent page pointer when this page is no longer referenced.
** The pageDestructor() routine handles that chore.
**
** Access to all fields of this structure is controlled by the mutex
** stored in MemPage.pBt->mutex.
*/
struct MemPage {
  u8 isInit;           /* True if previously initialized. MUST BE FIRST! */  //如果预先初始化则为真
  u8 nOverflow;        /* Number of overflow cell bodies in aCell[] */       //在aCell[]中溢出单元体的数目
  u8 intKey;           /* True if intkey flag is set */                      // 如果intkey标志设置则为True 
  u8 leaf;             /* True if leaf flag is set */                        //如果是叶子，则为True 
  u8 hasData;          /* True if this page stores data */                   //页面已存数据则为真
  u8 hdrOffset;        /* 100 for page 1.  0 otherwise */                    //对page1为100其他为0
  u8 childPtrSize;     /* 0 if leaf==1.  4 if leaf==0 */                     //如果是叶子则为0，如果不是叶子则为4
  u8 max1bytePayload;  /* min(maxLocal,127) */
  u16 maxLocal;        /* Copy of BtShared.maxLocal or BtShared.maxLeaf */   // BtShared.maxLocal或BtShared.maxLeaf的副本
  u16 minLocal;        /* Copy of BtShared.minLocal or BtShared.minLeaf */   //BtShared.minLocal或BtShared.minLeaf的副本
  u16 cellOffset;      /* Index in aData of first cell pointer */            //单元指针数组的偏移量，aData中第1个单元的指针
  u16 nFree;           /* Number of free bytes on the page */                //页上的可使用空间的总和（字节数）
  u16 nCell;           /* Number of cells on this page, local and ovfl */    //本页的单元数, local and ovfl 
  u16 maskPage;        /* Mask for page offset */                            //页偏移量的标记
  u16 aiOvfl[5];       /* Insert the i-th overflow cell before the aiOvfl-th //在aiOvfl-th的non-overflow 单元的前面插入i-th溢出单元
                       ** non-overflow cell */          
  u8 *apOvfl[5];       /* Pointers to the body of overflow cells */          //指向溢出单元的指针
  BtShared *pBt;       /* Pointer to BtShared that this page is part of */   //指向BtShared的指针，该页是BtShared的一部分
  u8 *aData;           /* Pointer to disk image of the page data */          //指向页数据的磁盘映像
  u8 *aDataEnd;        /* One byte past the end of usable data */            //可用数据的最后的一个字节
  u8 *aCellIdx;        /* The cell index area */                             //单元的指针域
  DbPage *pDbPage;     /* Pager page handle */                               //Pager的页句柄
  Pgno pgno;           /* Page number for this page */                       //本页的编号
};

/*
** The in-memory image of a disk page has the auxiliary information appended
** to the end.  EXTRA_SIZE is the number of bytes of space needed to hold
** that extra information.
*/
#define EXTRA_SIZE sizeof(MemPage)
/*附加信息大小*/

/*
** A linked list of the following structures is stored at BtShared.pLock.
** Locks are added (or upgraded from READ_LOCK to WRITE_LOCK) when a cursor 
** is opened on the table with root page BtShared.iTable. Locks are removed
** from this list when a transaction is committed or rolled back, or when
** a btree handle is closed.         /*btree句柄
*/
struct BtLock {                                                 //btree锁结构
  Btree *pBtree;        /* Btree handle holding this lock */    //btree句柄持有锁
  Pgno iTable;          /* Root page of table */                //表的根页
  u8 eLock;             /* READ_LOCK or WRITE_LOCK*/            //读锁或者写锁
  BtLock *pNext;        /* Next in BtShared.pLock list*/        //BtShared.pLock列表的后继
};

/* Candidate values for BtLock.eLock */
#define READ_LOCK     1
#define WRITE_LOCK    2
/*定义读锁为1，写锁为2 */

/* A Btree handle
**
** A database connection contains a pointer to an instance of
** this object for every database file that it has open.  This structure
** is opaque（透明）to the database connection.  The database connection cannot
** see the internals of this structure and only deals with pointers to
** this structure.
**
** For some database files, the same underlying(底层)database cache might be 
** shared between multiple connections.  In that case, each connection
** has it own instance of this object.  But each instance of this object
** points to the same BtShared object.  The database cache and the
** schema associated with the database file are all contained within
** the BtShared object.
**
** All fields in this structure are accessed under sqlite3.mutex.
** The pBt pointer itself may not be changed while there exists cursors 
** in the referenced BtShared that point back to this Btree since those
** cursors have to go through this Btree to find their BtShared and
** they often do so without holding sqlite3.mutex.
** 在数据库连接中，为每一个打开的数据库文件保持一个指向本对象实例的指针。
** 这个结构对数据库连接是透明的。数据库连接不能看到此结构的内部，只能通过指针来操作此结构。
** 有些数据库文件，其缓冲区可能被多个连接所共享。在这种情况下，每个连接都单独保持到此对象的指针。
** 但此对象的每个实例指向相同的BtShared对象。数据库缓冲区和schema都包含在BtShared对象中。
** 
*/
struct Btree {
  sqlite3 *db;       /* The database connection holding this btree */        //拥有此btree的数据库连接
  BtShared *pBt;     /* Sharable content of this btree  */                   //此btree的可共享内容
  u8 inTrans;        /* TRANS_NONE, TRANS_READ or TRANS_WRITE */             //事务类型
  u8 sharable;       /* True if we can share pBt with another db */          //如果共享pBt给数据库连接db则返回true
  u8 locked;         /* True if db currently has pBt locked */               //当前数据库连接锁定了pBt则返回true
  int wantToLock;    /* Number of nested calls to sqlite3BtreeEnter() */     //嵌套调用sqlite3BtreeEnter()的数量
  int nBackup;       /* Number of backup operations reading this btree */    //读这btree备份操作的数量
  Btree *pNext;      /* List of other sharable Btrees from the same db */    //相同数据库连接的其他可共享B树列表
  Btree *pPrev;      /* Back pointer of the same list */                     //相同列表的返回指针
#ifndef SQLITE_OMIT_SHARED_CACHE
  BtLock lock;       /* Object used to lock page 1 */                        //对象用于锁第1页            
#endif
};

/*
** Btree.inTrans may take one of the following values.
**
** If the shared-data extension is enabled, there may be multiple users
** of the Btree structure. At most one of these may open a write transaction,
** but any number may have active read transactions.
*/
#define TRANS_NONE  0
#define TRANS_READ  1
#define TRANS_WRITE 2

/*
** An instance of this object represents a single database file.
** 
** A single database file can be in use at the same time by two
** or more database connections.  When two or more connections are
** sharing the same database file, each connection has it own
** private Btree object for the file and each of those Btrees points
** to this one BtShared object.  BtShared.nRef is the number of
** connections currently sharing this database file.
** 此对象的一个实例描述一个单独的数据库文件。一个数据库文件同时可以被多个数据库连接使用。
** 当多个数据库连接共享相同的数据库文件时，每个连接有它自己的文件Btree，这些Btree指向同一个本BtShared对象。
** BtShared.nRef是共享此数据库文件的连接的个数。
** 
** Fields in this structure are accessed under the BtShared.mutex
** mutex, except for nRef and pNext which are accessed under the
** global SQLITE_MUTEX_STATIC_MASTER mutex.  The pPager field
** may not be modified once it is initially set as long as nRef>0.
** The pSchema field may be set once under BtShared.mutex and
** thereafter is unchanged as long as nRef>0.
**
** isPending:
**
**   If a BtShared client fails to obtain a write-lock on a database
**   table (because there exists one or more read-locks on the table),
**   the shared-cache enters 'pending-lock' state and isPending is
**   set to true.
**
**   The shared-cache leaves the 'pending lock' state when either of
**   the following occur:
**
**     1) The current writer (BtShared.pWriter) concludes its transaction, OR
**     2) The number of locks held by other connections drops to zero.
**
**   while in the 'pending-lock' state, no connection may start a new
**   transaction.
**
**   This feature is included to help prevent writer-starvation.
*/
struct BtShared {
  Pager *pPager;        /* The page cache */                                       //页缓冲区 
  sqlite3 *db;          /* Database connection currently using this Btree */       //当前正在使用B树的数据库连接
  BtCursor *pCursor;    /* A list of all open cursors */                           //包含当前打开的所有游标的列表 
  MemPage *pPage1;      /* First page of the database */                           //数据库的第一个页
  u8 openFlags;         /* Flags to sqlite3BtreeOpen() */                          //sqlite3BtreeOpen()的标签
#ifndef SQLITE_OMIT_AUTOVACUUM
  u8 autoVacuum;        /* True if auto-vacuum is enabled */                       //auto-vacuum数据库可用返回true
  u8 incrVacuum;        /* True if incr-vacuum is enabled */                       //incr-vacuum数据库可用返回true
#endif
  u8 inTransaction;     /* Transaction state */                                    //事务状态
  u8 max1bytePayload;   /* Maximum first byte of cell for a 1-byte payload */      //对于一个1-byte的有效载荷，其单元的第一个字节的最大值
  u16 btsFlags;         /* Boolean parameters.  See BTS_* macros below */          //布尔型参数。参阅下面的BTS_*宏
  u16 maxLocal;         /* Maximum local payload in non-LEAFDATA tables */         //在non-LEAFDATA表中本地有效载荷的最大值
  u16 minLocal;         /* Minimum local payload in non-LEAFDATA tables */         //在non-LEAFDATA表中本地有效载荷的最小值
  u16 maxLeaf;          /* Maximum local payload in a LEAFDATA table */            //在LEAFDATA表中本地有效载荷的最大值
  u16 minLeaf;          /* Minimum local payload in a LEAFDATA table */            //在LEAFDATA表中本地有效载荷的最小值
  u32 pageSize;         /* Total number of bytes on a page */                      //每页的字节数
  u32 usableSize;       /* Number of usable bytes on each page */  //每页可用的字节数。pageSize-每页尾部保留空间的大小，在文件头偏移为20处设定。
  int nTransaction;     /* Number of open transactions (read + write) */           //开放事务（读+写）的数量
  u32 nPage;            /* Number of pages in the database */                      //在数据库中页的数量
  void *pSchema;        /* Pointer to space allocated by sqlite3BtreeSchema() */   //指向由sqlite3BtreeSchema()所申请空间的指针
  void (*xFreeSchema)(void*);  /* Destructor for BtShared.pSchema */               //BtShared.pSchema的析构函数
  sqlite3_mutex *mutex; /* Non-recursive mutex required to access this object */   //非递归互斥锁需要访问这个对象
  Bitvec *pHasContent;  /* Set of pages moved to free-list this transaction */     //这个事务移动页面集合到空闲列表
#ifndef SQLITE_OMIT_SHARED_CACHE
  int nRef;             /* Number of references to this structure */               //共享此数据库文件的连接的个数
  BtShared *pNext;      /* Next on a list of sharable BtShared structs */          //在可共享BtShared结构上的后继
  BtLock *pLock;        /* List of locks held on this shared-btree struct */       //在shared-btree结构上持有的锁列表
  Btree *pWriter;       /* Btree with currently open write transaction */          //B树带有当前开放性写事务
#endif
  u8 *pTmpSpace;        /* BtShared.pageSize bytes of space for tmp use */         //BtShared.pageSize临时使用的空字节数
};

/*
** Allowed values for BtShared.btsFlags
*/
#define BTS_READ_ONLY        0x0001   /* Underlying file is readonly */            //优先文件是只读的
#define BTS_PAGESIZE_FIXED   0x0002   /* Page size can no longer be changed */     //固定页面大小
#define BTS_SECURE_DELETE    0x0004   /* PRAGMA secure_delete is enabled */        //编译指令secure_delete启用
#define BTS_INITIALLY_EMPTY  0x0008   /* Database was empty at trans start */      //在事务的开始数据库是空
#define BTS_NO_WAL           0x0010   /* Do not open write-ahead-log files */      //不打开write-ahead-log文件
#define BTS_EXCLUSIVE        0x0020   /* pWriter has an exclusive lock */          //pWrite独占锁
#define BTS_PENDING          0x0040   /* Waiting for read-locks to clear */        //等待读锁清除

/*
** An instance of the following structure is used to hold information
** about a cell.  The parseCellPtr() function fills in this structure
** based on information extract from the raw disk page.
** 此结构的实例用来保存单元头信息。parseCellPtr()负责根据从原始磁盘页中取得的信息填写此结构。
*/
typedef struct CellInfo CellInfo;
struct CellInfo {
  i64 nKey;      /* The key for INTKEY tables, or number of bytes in key */    //关键字的字节数。如果intkey标志被设置，此域即为关键字本身。
  u8 *pCell;     /* Pointer to the start of cell content */                    //指向单元内容的指针
  u32 nData;     /* Number of bytes of data */                                 //数据的字节数
  u32 nPayload;  /* Total amount of payload */                                 //有效载荷的总量
  u16 nHeader;   /* Size of the cell content header in bytes */                //记录头的字节数
  u16 nLocal;    /* Amount of payload held locally */                          //有效载荷局部持有的量
  u16 iOverflow; /* Offset to overflow page number.  Zero if no overflow */    //溢出页链表中第1个溢出页的页号。如果没有溢出页，无此域。
  u16 nSize;     /* Size of the cell content on the main b-tree page */        //单元数据的大小（不包括溢出页上的内容）
};

/*
** Maximum depth of an SQLite B-Tree structure. Any B-Tree deeper than
** this will be declared corrupt. This value is calculated based on a
** maximum database size of 2^31 pages a minimum fanout of 2 for a
** root-node and 3 for all other internal nodes.
**
** If a tree that appears to be taller than this is encountered, it is
** assumed that the database is corrupt.
*/
#define BTCURSOR_MAX_DEPTH 20

/*
** A cursor is a pointer to a particular entry within a particular
** b-tree within a database file.
**
** The entry is identified by its MemPage and the index in
** MemPage.aCell[] of the entry.
**
** A single database file can be shared by two more database connections,
** but cursors cannot be shared.  Each cursor is associated with a
** particular database connection identified BtCursor.pBtree.db.
** 游标是指向一个特定条目的指针，这个条目在一个数据库文件的特定b-tree中。
** 条目由MemPage和MemPage.aCell[]的下标确定。
** 一个数据库文件可被多个数据库连接共享，但游标不能被共享。
**
** Fields in this structure are accessed under the BtShared.mutex
** found at self->pBt->mutex. 
** 在BtShared.mutex下，这个结构中的域被访问，发现self->pBt->mutex.
*/
struct BtCursor {           //B树上的游标，游标是指向一个特定条目的指针
  Btree *pBtree;            /* The Btree to which this cursor belongs */          //属于这个B树的游标
  BtShared *pBt;            /* The BtShared this cursor points to */              //该游标指向BtShared
  BtCursor *pNext, *pPrev;  /* Forms a linked list of all cursors */              //形成一个所有游标的链表
  struct KeyInfo *pKeyInfo; /* Argument passed to comparison function */          //参数传递给比较函数
#ifndef SQLITE_OMIT_INCRBLOB
  Pgno *aOverflow;          /* Cache of overflow page locations */                //缓存溢出的页面位置
#endif
  Pgno pgnoRoot;            /* The root page of this tree */                      //此Btree的根页页号
  sqlite3_int64 cachedRowid; /* Next rowid cache.  0 means not valid */           //下一个rowid的缓存，0表示无效
  CellInfo info;            /* A parse of the cell we are pointing at */          //当前指向的单元（cell）的解析结果
  i64 nKey;        /* Size of pKey, or last integer key */                        //pKey的大小或最后的整数键值
  void *pKey;      /* Saved key that was cursor's last known position */          //游标最后已知的位置的键值
  int skipNext;    /* Prev() is noop if negative. Next() is noop if positive */   //如果为负Prev()无操作，如果为正Next()无操作
  u8 wrFlag;                /* True if writable */                                //写标签，如果可写为真
  u8 atLast;                /* Cursor pointing to the last entry */               //指针指向最后条目
  u8 validNKey;             /* True if info.nKey is valid */                      //如果info.nKey有效为真
  u8 eState;                /* One of the CURSOR_XXX constants (see below) */     //CURSOR_XXX常量之一
#ifndef SQLITE_OMIT_INCRBLOB
  u8 isIncrblobHandle;      /* True if this cursor is an incr. io handle */       //如果游标是一个incr.io句柄则为真
#endif
  u8 hints;                             /* As configured by CursorSetHints() */   //通过CursorSetHints()设置
  i16 iPage;                            /* Index of current page in apPage */     //当前页在apPage中的索引
  u16 aiIdx[BTCURSOR_MAX_DEPTH];        /* Current index in apPage[i] */          //apPage[i]中的当前索引。空注：单元指针数组中的当前下标。

  MemPage *apPage[BTCURSOR_MAX_DEPTH];  /* Pages from root to current page */     //从根页到本页的所有页
}; 

/*
** Potential values for BtCursor.eState.
**
** CURSOR_VALID:
**   Cursor points to a valid entry. getPayload() etc. may be called.
**
** CURSOR_INVALID:
**   Cursor does not point to a valid entry. This can happen (for example) 
**   because the table is empty or because BtreeCursorFirst() has not been
**   called.
**
** CURSOR_REQUIRESEEK:
**   The table that this cursor was opened on still exists, but has been 
**   modified since the cursor was last used. The cursor position is saved
**   in variables BtCursor.pKey and BtCursor.nKey. When a cursor is in 
**   this state, restoreCursorPosition() can be called to attempt to
**   seek the cursor to the saved position.
**
** CURSOR_FAULT:
**   A unrecoverable error (an I/O error or a malloc failure) has occurred
**   on a different connection that shares the BtShared cache with this
**   cursor.  The error has left the cache in an inconsistent state.
**   Do nothing else with this cursor.  Any attempt to use the cursor
**   should return the error code stored in BtCursor.skip
*/
#define CURSOR_INVALID           0
#define CURSOR_VALID             1
#define CURSOR_REQUIRESEEK       2
#define CURSOR_FAULT             3

/* 
** The database page the PENDING_BYTE occupies. This page is never used.
*/
# define PENDING_BYTE_PAGE(pBt) PAGER_MJ_PGNO(pBt)

/*
** These macros define the location of the pointer-map entry for a 
** database page. The first argument to each is the number of usable
** bytes on each page of the database (often 1024). The second is the
** page number to look up in the pointer map.
**
** PTRMAP_PAGENO returns the database page number of the pointer-map
** page that stores the required pointer. PTRMAP_PTROFFSET returns
** the offset of the requested map entry.
**
** If the pgno argument passed to PTRMAP_PAGENO is a pointer-map page,
** then pgno is returned. So (pgno==PTRMAP_PAGENO(pgsz, pgno)) can be
** used to test if pgno is a pointer-map page. PTRMAP_ISPAGE implements
** this test.
*/
#define PTRMAP_PAGENO(pBt, pgno) ptrmapPageno(pBt, pgno)
#define PTRMAP_PTROFFSET(pgptrmap, pgno) (5*(pgno-pgptrmap-1))
#define PTRMAP_ISPAGE(pBt, pgno) (PTRMAP_PAGENO((pBt),(pgno))==(pgno))

/*
** The pointer map is a lookup table that identifies the parent page for
** each child page in the database file.  The parent page is the page that
** contains a pointer to the child.  Every page in the database contains
** 0 or 1 parent pages.  (In this context 'database page' refers
** to any page that is not part of the pointer map itself.)  Each pointer map
** entry consists of a single byte 'type' and a 4 byte parent page number.
** The PTRMAP_XXX identifiers below are the valid types.
**
** The purpose of the pointer map is to facility moving pages from one
** position in the file to another as part of autovacuum.  When a page
** is moved, the pointer in its parent must be updated to point to the
** new location.  The pointer map is used to locate the parent page quickly.
**
** PTRMAP_ROOTPAGE: The database page is a root-page. The page-number is not
**                  used in this case.
**
** PTRMAP_FREEPAGE: The database page is an unused (free) page. The page-number 
**                  is not used in this case.
**
** PTRMAP_OVERFLOW1: The database page is the first page in a list of 
**                   overflow pages. The page number identifies the page that
**                   contains the cell with a pointer to this overflow page.
**
** PTRMAP_OVERFLOW2: The database page is the second or later page in a list of
**                   overflow pages. The page-number identifies the previous
**                   page in the overflow page list.
**
** PTRMAP_BTREE: The database page is a non-root btree page. The page number
**               identifies the parent page in the btree.
*/
#define PTRMAP_ROOTPAGE 1
#define PTRMAP_FREEPAGE 2
#define PTRMAP_OVERFLOW1 3
#define PTRMAP_OVERFLOW2 4
#define PTRMAP_BTREE 5

/* A bunch of assert() statements to check the transaction state variables
** of handle p (type Btree*) are internally consistent.
*/
#define btreeIntegrity(p) \
  assert( p->pBt->inTransaction!=TRANS_NONE || p->pBt->nTransaction==0 ); \
  assert( p->pBt->inTransaction>=p->inTrans ); 


/*
** The ISAUTOVACUUM macro is used within balance_nonroot() to determine
** if the database supports auto-vacuum or not. Because it is used
** within an expression that is an argument to another macro 
** (sqliteMallocRaw), it is not possible to use conditional compilation.
** So, this macro is defined instead.
*/
#ifndef SQLITE_OMIT_AUTOVACUUM
#define ISAUTOVACUUM (pBt->autoVacuum)
#else
#define ISAUTOVACUUM 0
#endif


/*
** This structure is passed around through all the sanity checking routines
** in order to keep track of some global state information.
**
** The aRef[] array is allocated so that there is 1 bit for each page in
** the database. As the integrity-check proceeds, for each page used in
** the database the corresponding bit is set. This allows integrity-check to 
** detect pages that are used twice and orphaned pages (both of which 
** indicate corruption).
** 
*/
typedef struct IntegrityCk IntegrityCk;
struct IntegrityCk {
  BtShared *pBt;    /* The tree being checked out */                             //B树正在检查数据完整性
  Pager *pPager;    /* The associated pager.  Also accessible by pBt->pPager */  //相关页对象，也可以通过pBt->pPager访问
  u8 *aPgRef;       /* 1 bit per page in the db (see above) */                   //在db中每页1位
  Pgno nPage;       /* Number of pages in the database */                        //在数据库中页的数量
  int mxErr;        /* Stop accumulating errors when this reaches zero */        //当这个变量达到零的时候，停止积累错误
  int nErr;         /* Number of messages written to zErrMsg so far */           //当前已经写到zErrMsg中的信息数量
  int mallocFailed; /* A memory allocation error has occurred */                 //一个内存分配发生错误
  StrAccum errMsg;  /* Accumulate the error message text here */                 //收集错误信息文本
};

/*
** Routines to read or write a two- and four-byte big-endian integer values.
*/
#define get2byte(x)   ((x)[0]<<8 | (x)[1])
#define put2byte(p,v) ((p)[0] = (u8)((v)>>8), (p)[1] = (u8)(v))
#define get4byte sqlite3Get4byte
#define put4byte sqlite3Put4byte
