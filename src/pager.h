/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
*************************************************************************
** This header file defines the interface that the sqlite page cache
** subsystem.  The page cache subsystem reads and writes a file a page
** at a time and provides a journal for rollback.
*/
//这个头文件定义了一个SQLite缓存页面子系统的界面。这个页面缓存子系统读和写一个文件的一个页面并且提供了一个回滚的日志。
#ifndef _PAGER_H_
#define _PAGER_H_
//如果没有定义pager-h，那就定义一个
/*
** Default maximum size for persistent journal files. A negative 
** value means no limit. This value may be overridden using the 
** sqlite3PagerJournalSizeLimit() API. See also "PRAGMA journal_size_limit".
*/
//对当前日志文件定义最大值。一个负数值意味着没有限制。
//这个值可能被函数sqlite3PagerJournalSizeLimit（）应用程序界面覆写。也是看“编译指示 journal-size-limit”
#ifndef SQLITE_DEFAULT_JOURNAL_SIZE_LIMIT
  #define SQLITE_DEFAULT_JOURNAL_SIZE_LIMIT -1
#endif
//如果没有定义SQLite-default-journal-size-limit(SQLite默认日志大小限制)，那就定义一个。
/*
** The type used to represent a page number.  The first page in a file
** is called page 1.  0 is used to represent "not a page".
*/
//这个类型是被用于表示页面数量（页号）的。文件中的第一页被称为page 1，0是被用来表示“没有页面”。
typedef u32 Pgno;
定义Pgno为u32类型的
/*
** Each open file is managed by a separate instance of the "Pager" structure.
*/
//每个打开的文件都是由一个单独的“Pager”结构实例管理。
typedef struct Pager Pager;
定义Pager（后）为Pager（前）类型结构体
/*
** Handle type for pages.
*/
//处理页面类型。
typedef struct PgHdr DbPage;
定义每一个内存页面的页面头结构为DbPage
/*
** Page number PAGER_MJ_PGNO is never used in an SQLite database (it is
** reserved for working around a windows/posix incompatibility). It is
** used in the journal to signify that the remainder of the journal file 
** is devoted to storing a master journal name - there are no more pages to
** roll back. See comments for function writeMasterJournal() in pager.c 
** for details.
*/
/*页码PAGER_MJ_PGNO从未在一个SQLite数据库中使用过(它是专为解决在windows / posix不兼容问题保留的)。
这是在日志中运用来表示日志文件的其余部分致力于存储主日志名称-没有更多的页面用于回滚了。
在pager.c看到writeMasterJournal()的函数的评论的细节。*/
#define PAGER_MJ_PGNO(x) ((Pgno)((PENDING_BYTE/((x)->pageSize))+1))
定义PAGER_MJ_PGNO(x)  ( (Pgno)  ( (PENDING_BYTE/ ((x)->pageSize) )+1))
/*
** Allowed values for the flags parameter to sqlite3PagerOpen().
**
** NOTE: These values must match the corresponding BTREE_ values in btree.h.
*/
//允许sqlite3PagerOpen()的标志参数的值。注意:这些值必须匹配相应的在btree.h BTREE值。
#define PAGER_OMIT_JOURNAL  0x0001    /* Do not use a rollback journal */
#define PAGER_MEMORY        0x0002    /* In-memory database */
定义PAGER_OMIT_JOURNAL 0 x0001 / *不使用回滚日志* / 　　
定义PAGER_MEMORY 0 x0002 /*内存数据库* /
/*
** Valid values for the second argument to sqlite3PagerLockingMode().
*/
sqlite3PagerLockingMode()第二个参数的有效值。
#define PAGER_LOCKINGMODE_QUERY      -1
#define PAGER_LOCKINGMODE_NORMAL      0
#define PAGER_LOCKINGMODE_EXCLUSIVE   1
#定义PAGER_LOCKINGMODE_QUERY -1 　　
#定义PAGER_LOCKINGMODE_NORMAL 0 　　
#定义PAGER_LOCKINGMODE_EXCLUSIVE 1
/*
** Numeric constants that encode the journalmode.  
*/
//数字常量journalmode编码。
#define PAGER_JOURNALMODE_QUERY     (-1)  /* Query the value of journalmode */
#define PAGER_JOURNALMODE_DELETE      0   /* Commit by deleting journal file */
#define PAGER_JOURNALMODE_PERSIST     1   /* Commit by zeroing journal header */
#define PAGER_JOURNALMODE_OFF         2   /* Journal omitted.  */
#define PAGER_JOURNALMODE_TRUNCATE    3   /* Commit by truncating journal */
#define PAGER_JOURNALMODE_MEMORY      4   /* In-memory journal file */
#define PAGER_JOURNALMODE_WAL         5   /* Use write-ahead logging */
 /*#定义PAGER_JOURNALMODE_QUERY(-1)    /*查询journalmode的值* /　　
#定义PAGER_JOURNALMODE_DELETE 0  //提交通过删除日志文件　　
# define PAGER_JOURNALMODE_PERSIST 1 //提交通过归零日志标题　　
#定义PAGER_JOURNALMODE_OFF 2 //省略日志。* / 　　
#定义PAGER_JOURNALMODE_TRUNCATE 3 / *通过删除日志提交* / 　　
#定义PAGER_JOURNALMODE_MEMORY 4 / *内存日志文件* / 　　
#定义PAGER_JOURNALMODE_WAL 5 / *使用写前日志记录* /
/*
** The remainder of this file contains the declarations of the functions
** that make up the Pager sub-system API. See source code comments for 
** a detailed description of each routine.
*/
//本文件中剩余部分包含的函数的声明，这个声明组成了Pager子系统API。为了了解每个例程的详细介绍去看源码注释。
/* Open and close a Pager connection. */ 
int sqlite3PagerOpen(
  sqlite3_vfs*,//pvfs  所应用的虚拟文件系统             
  Pager **ppPager,//out：这里返回页面结构
  const char*,// zFilename //所打开的数据库文件名字
  int,//nExtra 在每一个内存页面上附加的额外的字节
  int,//flags 控制文件的标志
  int,//vfsFlag 通过sqlite3-vfs.XOpen（）的标志
  void(*xReinit)(DbPage*)
)

int sqlite3PagerClose(Pager *pPager);

int sqlite3PagerReadFileheader(Pager*, int, unsigned char*);

/* Functions used to configure a Pager object. */
void sqlite3PagerSetBusyhandler(Pager*, int(*)(void *), void *);

int sqlite3PagerSetPagesize(Pager*, u32*, int);

int sqlite3PagerMaxPageCount(Pager*, int);

void sqlite3PagerSetCachesize(Pager*, int);

void sqlite3PagerShrink(Pager*);

void sqlite3PagerSetSafetyLevel(Pager*,int,int,int);

int sqlite3PagerLockingMode(Pager *, int);

int sqlite3PagerSetJournalMode(Pager *, int);

int sqlite3PagerGetJournalMode(Pager*);

int sqlite3PagerOkToChangeJournalMode(Pager*);

i64 sqlite3PagerJournalSizeLimit(Pager *, i64);

sqlite3_backup **sqlite3PagerBackupPtr(Pager*);

/* Functions used to obtain and release page references. */ 
int sqlite3PagerAcquire(Pager *pPager, Pgno pgno, DbPage **ppPage, int clrFlag);
#define sqlite3PagerGet(A,B,C) sqlite3PagerAcquire(A,B,C,0)

DbPage *sqlite3PagerLookup(Pager *pPager, Pgno pgno);

void sqlite3PagerRef(DbPage*);

void sqlite3PagerUnref(DbPage*);

/* Operations on page references. */
int sqlite3PagerWrite(DbPage*);

void sqlite3PagerDontWrite(DbPage*);

int sqlite3PagerMovepage(Pager*,DbPage*,Pgno,int);

int sqlite3PagerPageRefcount(DbPage*);

void *sqlite3PagerGetData(DbPage *); 

void *sqlite3PagerGetExtra(DbPage *); 

/* Functions used to manage pager transactions and savepoints. */
void sqlite3PagerPagecount(Pager*, int*);

int sqlite3PagerBegin(Pager*, int exFlag, int);

int sqlite3PagerCommitPhaseOne(Pager*,const char *zMaster, int);

int sqlite3PagerExclusiveLock(Pager*);

int sqlite3PagerSync(Pager *pPager);

int sqlite3PagerCommitPhaseTwo(Pager*);
int sqlite3PagerRollback(Pager*);
int sqlite3PagerOpenSavepoint(Pager *pPager, int n);
int sqlite3PagerSavepoint(Pager *pPager, int op, int iSavepoint);
int sqlite3PagerSharedLock(Pager *pPager);

int sqlite3PagerCheckpoint(Pager *pPager, int, int*, int*);
int sqlite3PagerWalSupported(Pager *pPager);
int sqlite3PagerWalCallback(Pager *pPager);
int sqlite3PagerOpenWal(Pager *pPager, int *pisOpen);
int sqlite3PagerCloseWal(Pager *pPager);
#ifdef SQLITE_ENABLE_ZIPVFS
  int sqlite3PagerWalFramesize(Pager *pPager);
#endif

/* Functions used to query pager state and configuration. */
u8 sqlite3PagerIsreadonly(Pager*);
int sqlite3PagerRefcount(Pager*);
int sqlite3PagerMemUsed(Pager*);
const char *sqlite3PagerFilename(Pager*, int);
const sqlite3_vfs *sqlite3PagerVfs(Pager*);
sqlite3_file *sqlite3PagerFile(Pager*);
const char *sqlite3PagerJournalname(Pager*);
int sqlite3PagerNosync(Pager*);
void *sqlite3PagerTempSpace(Pager*);
int sqlite3PagerIsMemdb(Pager*);
void sqlite3PagerCacheStat(Pager *, int, int, int *);
void sqlite3PagerClearCache(Pager *);

/* Functions used to truncate the database file. */
void sqlite3PagerTruncateImage(Pager*,Pgno);

#if defined(SQLITE_HAS_CODEC) && !defined(SQLITE_OMIT_WAL)
void *sqlite3PagerCodec(DbPage *);
#endif

/* Functions to support testing and debugging. */
#if !defined(NDEBUG) || defined(SQLITE_TEST)
  Pgno sqlite3PagerPagenumber(DbPage*);
  int sqlite3PagerIswriteable(DbPage*);
#endif
#ifdef SQLITE_TEST
  int *sqlite3PagerStats(Pager*);
  void sqlite3PagerRefdump(Pager*);
  void disable_simulated_io_errors(void);
  void enable_simulated_io_errors(void);
#else
# define disable_simulated_io_errors()
# define enable_simulated_io_errors()
#endif
//SQLite的Pager部分按功能分有八个模块，每个模块都包括各自不同功能的函数。下面对每个模块进行说明。
//1打开/关闭一个页面连接
/*这个模块主要负责打开、关闭一个页面，并且读取数据库文件标题。需要的空间，计算路径长度并存储完整路径名，
对页面分配固定结构的存储，缓存页面文件名和日志，设置页面默认大小值 ；调用非锁且回滚前同步，
开放日志的不同步部分返回数据库，发生错误后用非锁且回滚来解锁；读取数据库标题，
读取文件前N个字节放入pDest指向的存储中。
*/
//2配置页面对象的函数
/*这个模块主要负责设置忙操作、页面大小、最大累计页面数、最大内存页面数、页面安全等级、页面锁模式和日志模式，
可以获取页面锁模式、日志模式和指向备份的指针，还能判定日志模式是否改变，并且尽可能的释放存储。
锁升级时设置并调用忙操作；改变对象调用的页面大小，在*pPageSize中传递；设置数据库页面计数最大值；
改变内存页面所允许的最大页数；从页面中尽可能的释放存储；对页面设置安全等级；
获取/设置页面的锁模式和日志文件的大小限制；设置页面的日志模式；返回当前的日志模式；
判定是否可以改变日志模式；得到指向备份的指针。
*/
//3获取和释放页面引用的函数。
/*这个模块主要负责获取页面的引用、查找页面、计算引用计数的增量和释放页面引用。
获取页码pgno对页面的引用，判定请求页面是否在缓存中，页面初始化额外数据为0，
读取空闲列表的叶子页面且回滚保存点并加载新页面进入写有日志数据的缓存中，
请求失败后返回错误代码并设置*ppPager为NULL；获取缓存中的页面，如果不在则返回指针或0；页面引用计数的增量；
释放页面引用。
*/
//4对页面引用的操作
/*这个模块主要负责把页面标记为可写，移动页面，获得引用页面数量、指定数据的指针和指向nExtra的指针。
数据页面标记为可写，在修改前调用来检查函数返回值；调用之后无需将页面信息写回磁盘；定位页码来移动页面；
获取引用页面数量；获取页面数据的指针；获取页面nExtra的额外空间字节指针。
*/
//5管理页面事务和保存点的函数
/*这个模块主要负责获取数据库页面总数，开始一个写事务，同步数据库文件，设置排他锁。
获取数据库中页面总数；在指定页面上开始一个写事务；对页面pPager同步数据库文件；
数据库文件被事务变化和日志文件完全更新；同步数据库文件到磁盘上；写日志活跃时自动调用互斥锁；
回滚模式成功与否的不同返回；检查至少有固定个数的保存点打开；回滚或释放保存点；获取一个共享锁；
在wal模式下调用检查点；判定是否是支持底层VFS的原语；打开/关闭wal模式；关闭日志文件的连接。
*/
//6	查询页面状态/配置
/*判定是否是只读文件；获得页面引用数；获得当前页面使用的内存；获得数据库完整路径名；获得页面VFS结构；
获得与页面相关的数据库文件操作；获得日志文件完整文件名；判定是否同步页面；获得页面内部的临时页面缓冲指针；
获得eStat的状态；清除页面缓存。
*/
//7	截断数据库文件
/*截断内存数据库文件图像到nPage上，并非真正修改磁盘上的数据库文件，只是在设置页面对象的内部状态；
把页面内容写到日志文件中时调用wal模块
*/
//8	测试/调试
 
//返回页面页码；做测试和分析 

#endif /* _PAGER_H_ */

