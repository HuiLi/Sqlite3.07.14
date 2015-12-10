/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This header file defines the interface that the sqlite B-Tree file
** subsystem.  See comments in the source code for a detailed description
** of what each interface routine does.
*/
#ifndef _BTREE_H_
#define _BTREE_H_

/* TODO: This definition is just included so other modules compile. It
** needs to be revisited.
*/
#define SQLITE_N_BTREE_META 10

/*
** If defined as non-zero, auto-vacuum is enabled by default. Otherwise
** it must be turned on for each database using "PRAGMA auto_vacuum = 1".
*/
#ifndef SQLITE_DEFAULT_AUTOVACUUM
  #define SQLITE_DEFAULT_AUTOVACUUM 0
#endif

#define BTREE_AUTOVACUUM_NONE 0        /* Do not do auto-vacuum */  //不采用auto-vacuum数据库
#define BTREE_AUTOVACUUM_FULL 1        /* Do full auto-vacuum */    //全部采用auto-vacuum数据库
#define BTREE_AUTOVACUUM_INCR 2        /* Incremental vacuum */     //增量空间数据库

/*
** Forward declarations of structure                                //提出结构声明
*/
typedef struct Btree Btree;
typedef struct BtCursor BtCursor;
typedef struct BtShared BtShared;


int sqlite3BtreeOpen(                                              //打开数据库文件并返回B树对象
  sqlite3_vfs *pVfs,       /* VFS to use with this b-tree */       //B树采用VFS文件系统
  const char *zFilename,   /* Name of database file to open */     //打开的数据库文件的名称
  sqlite3 *db,             /* Associated database connection */    //相关数据库连接
  Btree **ppBtree,         /* Return open Btree* here */           //返回开放Btree*
  int flags,               /* Flags */                             //标记
  int vfsFlags             /* Flags passed through to VFS open */  //通过VFS打开的标记
);

/* The flags parameter to sqlite3BtreeOpen can be the bitwise or of the
** following values.
**
** NOTE:  These values must match the corresponding PAGER_ values in
** pager.h.
** sqlite3BtreeOpen标记参数可以是按位或者下列的值。注意：这些值必须和pager.h头文件中PAGER_ values相对应。
*/
#define BTREE_OMIT_JOURNAL  1  /* Do not create or use a rollback journal */   //不创建或使用回滚日志
#define BTREE_MEMORY        2  /* This is an in-memory DB */                   //这是一个内存数据库
#define BTREE_SINGLE        4  /* The file contains at most 1 b-tree */        //这个文件最多包含一个B树
#define BTREE_UNORDERED     8  /* Use of a hash implementation is OK */        //使用一个散列实现即可

int sqlite3BtreeClose(Btree*);                                                 //关闭数据库并使所有游标无效
int sqlite3BtreeSetCacheSize(Btree*,int);                                      //控制页缓存大小
int sqlite3BtreeSetSafetyLevel(Btree*,int,int,int);    //改变磁盘数据的访问方式，以增加或减少数据库抵御操作系统崩溃或电源故障等损害的能力
int sqlite3BtreeSyncDisabled(Btree*);
int sqlite3BtreeSetPageSize(Btree *p, int nPagesize, int nReserve, int eFix);  //设置数据库页大小
int sqlite3BtreeGetPageSize(Btree*);                                           //返回数据库页大小
int sqlite3BtreeMaxPageCount(Btree*,int);                                      //设置数据库的最大页数
u32 sqlite3BtreeLastPage(Btree*);                                              //
int sqlite3BtreeSecureDelete(Btree*,int);                                      //设置BTS_SECURE_DELETE标志
int sqlite3BtreeGetReserve(Btree*);                                            //页中未被使用的字节数
int sqlite3BtreeSetAutoVacuum(Btree *, int);                                   //设置数据库自动清理空闲页属性
int sqlite3BtreeGetAutoVacuum(Btree *);                                        //获取数据库是否是自动清理页
int sqlite3BtreeBeginTrans(Btree*,int);                                        //开始一个新事务
int sqlite3BtreeCommitPhaseOne(Btree*, const char *zMaster);
int sqlite3BtreeCommitPhaseTwo(Btree*, int);
int sqlite3BtreeCommit(Btree*);                                                //提交当前事务
int sqlite3BtreeRollback(Btree*,int);                                          //回滚当前进程中的事务
int sqlite3BtreeBeginStmt(Btree*,int);                                         //开始一个语句子事务
int sqlite3BtreeCreateTable(Btree*, int*, int flags);                          //在数据库中创建一个空B树，采用图格式（B+树）或索引格式（B树）
int sqlite3BtreeIsInTrans(Btree*);
int sqlite3BtreeIsInReadTrans(Btree*);
int sqlite3BtreeIsInBackup(Btree*);
void *sqlite3BtreeSchema(Btree *, int, void(*)(void *));
int sqlite3BtreeSchemaLocked(Btree *pBtree);
int sqlite3BtreeLockTable(Btree *pBtree, int iTab, u8 isWriteLock);
int sqlite3BtreeSavepoint(Btree *, int, int);

const char *sqlite3BtreeGetFilename(Btree *);
const char *sqlite3BtreeGetJournalname(Btree *);
int sqlite3BtreeCopyFile(Btree *, Btree *);

int sqlite3BtreeIncrVacuum(Btree *);

/* The flags parameter to sqlite3BtreeCreateTable can be the bitwise OR
** of the flags shown below.
**
** Every SQLite table must have either BTREE_INTKEY or BTREE_BLOBKEY set.
** With BTREE_INTKEY, the table key is a 64-bit integer and arbitrary data
** is stored in the leaves.  (BTREE_INTKEY is used for SQL tables.)  With
** BTREE_BLOBKEY, the key is an arbitrary BLOB and no content is stored
** anywhere - the key is the content.  (BTREE_BLOBKEY is used for SQL
** indices.)
*/
#define BTREE_INTKEY     1    /* Table has only 64-bit signed integer keys */
#define BTREE_BLOBKEY    2    /* Table has keys only - no data */

int sqlite3BtreeDropTable(Btree*, int, int*);                  //删除数据库中的一个B树
int sqlite3BtreeClearTable(Btree*, int, int*);                 //删除B树中所有数据，但保持B树结构完整
void sqlite3BtreeTripAllCursors(Btree*, int);                  //遍历所有游标

void sqlite3BtreeGetMeta(Btree *pBtree, int idx, u32 *pValue); //读数据库文件的元数据信息
int sqlite3BtreeUpdateMeta(Btree*, int idx, u32 value);

/*
** The second parameter to sqlite3BtreeGetMeta or sqlite3BtreeUpdateMeta
** should be one of the following values. The integer values are assigned 
** to constants so that the offset of the corresponding field in an
** SQLite database header may be found using the following formula:
**
**   offset = 36 + (idx * 4)
**
** For example, the free-page-count field is located at byte offset 36 of
** the database file header. The incr-vacuum-flag field is located at
** byte offset 64 (== 36+4*7).
*/
#define BTREE_FREE_PAGE_COUNT     0
#define BTREE_SCHEMA_VERSION      1
#define BTREE_FILE_FORMAT         2
#define BTREE_DEFAULT_CACHE_SIZE  3
#define BTREE_LARGEST_ROOT_PAGE   4
#define BTREE_TEXT_ENCODING       5
#define BTREE_USER_VERSION        6
#define BTREE_INCR_VACUUM         7

/*
** Values that may be OR'd together to form the second argument of an
** sqlite3BtreeCursorHints() call.
*/
#define BTREE_BULKLOAD 0x00000001

int sqlite3BtreeCursor(     //创建一个指向特定B树的游标。可以是读或写游标，但读游标和写游标不能同时在同一B树中存在
  Btree*,                              /* BTree containing table to open */      //打开B树包含的表
  int iTable,                          /* Index of root page */                  //根页索引
  int wrFlag,                          /* 1 for writing.  0 for read-only */     //wrFlag值为1时表示正在写，为0时为只读
  struct KeyInfo*,                     /* First argument to compare function */  //比较函数的第一个参数
  BtCursor *pCursor                    /* Space to write cursor structure */     //写游标结构的空间
);
int sqlite3BtreeCursorSize(void);
void sqlite3BtreeCursorZero(BtCursor*);

int sqlite3BtreeCloseCursor(BtCursor*);
int sqlite3BtreeMovetoUnpacked(
  BtCursor*,
  UnpackedRecord *pUnKey,
  i64 intKey,
  int bias,
  int *pRes
);
int sqlite3BtreeCursorHasMoved(BtCursor*, int*);
int sqlite3BtreeDelete(BtCursor*);                                //删除游标所指记录
int sqlite3BtreeInsert(BtCursor*, const void *pKey, i64 nKey,     //在B树的适当位置插入一条记录
                                  const void *pData, int nData,
                                  int nZero, int bias, int seekResult);
int sqlite3BtreeFirst(BtCursor*, int *pRes);                      //游标移至B树第一条记录
int sqlite3BtreeLast(BtCursor*, int *pRes);                       //游标移至B树最后一条记录
int sqlite3BtreeNext(BtCursor*, int *pRes);                       //移动游标至当前游标所指记录的下一条
int sqlite3BtreeEof(BtCursor*);
int sqlite3BtreePrevious(BtCursor*, int *pRes);                   //移动游标至当前游标所指记录的前一条
int sqlite3BtreeKeySize(BtCursor*, i64 *pSize);                   //返回当前游标锁时记录的关键字长度
int sqlite3BtreeKey(BtCursor*, u32 offset, u32 amt, void*);       //返回当前游标锁时记录的关键字
const void *sqlite3BtreeKeyFetch(BtCursor*, int *pAmt);
const void *sqlite3BtreeDataFetch(BtCursor*, int *pAmt);
int sqlite3BtreeDataSize(BtCursor*, u32 *pSize);                  //返回当前游标锁时记录的数据字长度
int sqlite3BtreeData(BtCursor*, u32 offset, u32 amt, void*);      //返回当前游标锁时记录的数据
void sqlite3BtreeSetCachedRowid(BtCursor*, sqlite3_int64);
sqlite3_int64 sqlite3BtreeGetCachedRowid(BtCursor*);

char *sqlite3BtreeIntegrityCheck(Btree*, int *aRoot, int nRoot, int, int*);
struct Pager *sqlite3BtreePager(Btree*);

int sqlite3BtreePutData(BtCursor*, u32 offset, u32 amt, void*);
void sqlite3BtreeCacheOverflow(BtCursor *);
void sqlite3BtreeClearCursor(BtCursor *);
int sqlite3BtreeSetVersion(Btree *pBt, int iVersion);
void sqlite3BtreeCursorHints(BtCursor *, unsigned int mask);

#ifndef NDEBUG
int sqlite3BtreeCursorIsValid(BtCursor*);
#endif

#ifndef SQLITE_OMIT_BTREECOUNT
int sqlite3BtreeCount(BtCursor *, i64 *);
#endif

#ifdef SQLITE_TEST
int sqlite3BtreeCursorInfo(BtCursor*, int*, int);
void sqlite3BtreeCursorList(Btree*);
#endif

#ifndef SQLITE_OMIT_WAL
  int sqlite3BtreeCheckpoint(Btree*, int, int *, int *);
#endif

/*
** If we are not using shared cache, then there is no need to
** use mutexes to access the BtShared structures.  So make the
** Enter and Leave procedures no-ops.
*/
#ifndef SQLITE_OMIT_SHARED_CACHE
  void sqlite3BtreeEnter(Btree*);
  void sqlite3BtreeEnterAll(sqlite3*);
#else
# define sqlite3BtreeEnter(X) 
# define sqlite3BtreeEnterAll(X)
#endif

#if !defined(SQLITE_OMIT_SHARED_CACHE) && SQLITE_THREADSAFE
  int sqlite3BtreeSharable(Btree*);
  void sqlite3BtreeLeave(Btree*);
  void sqlite3BtreeEnterCursor(BtCursor*);
  void sqlite3BtreeLeaveCursor(BtCursor*);
  void sqlite3BtreeLeaveAll(sqlite3*);
#ifndef NDEBUG
  /* These routines are used inside assert() statements only. */
  int sqlite3BtreeHoldsMutex(Btree*);
  int sqlite3BtreeHoldsAllMutexes(sqlite3*);
  int sqlite3SchemaMutexHeld(sqlite3*,int,Schema*);
#endif
#else

# define sqlite3BtreeSharable(X) 0
# define sqlite3BtreeLeave(X)
# define sqlite3BtreeEnterCursor(X)
# define sqlite3BtreeLeaveCursor(X)
# define sqlite3BtreeLeaveAll(X)

# define sqlite3BtreeHoldsMutex(X) 1
# define sqlite3BtreeHoldsAllMutexes(X) 1
# define sqlite3SchemaMutexHeld(X,Y,Z) 1
#endif


#endif /* _BTREE_H_ */
