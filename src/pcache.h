/*
** 2008 August 05
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This header file defines the interface that the sqlite page cache
** subsystem. 
**这个头文件定义了sqlite页面缓存的子系统。
*/

#ifndef _PCACHE_H_

typedef struct PgHdr PgHdr;
typedef struct PCache PCache;

/*
** Every page in the cache is controlled by an instance of the following
** structure.
**缓存中的每一个页面由下面结构中的一个实例控制。
*/
struct PgHdr {
  sqlite3_pcache_page *pPage;    /* Pcache object page handle   Pcache对象页面处理*/
  void *pData;                   /* Page data  页面数据*/
  void *pExtra;                  /* Extra content 额外的内容*/
  PgHdr *pDirty;                 /* Transient list of dirty pages 脏页的瞬变列表*/
  Pager *pPager;                 /* The pager this page is part of 这个页面pager的一部分*/
  Pgno pgno;                     /* Page number for this page 这个页面的页码*/
#ifdef SQLITE_CHECK_PAGES
  u32 pageHash;                  /* Hash of page content 页面内容的散列*/
#endif
  u16 flags;                     /* PGHDR flags defined below   PGHDR标志定义如下*/

  /**********************************************************************
  ** Elements above are public.  All that follows is private to pcache.c
  ** and should not be accessed by other modules.
  **以上内容是公开的。所有下面是专用于pcache.c ，其他模块不能进行访问。
  */
  i16 nRef;                      /* Number of users of this page 此页的用户数*/
  PCache *pCache;                /* Cache that owns this page 高速缓存拥有这个页面*/

  PgHdr *pDirtyNext;             /* Next element in list of dirty pages 脏页列表的下一个元素*/
  PgHdr *pDirtyPrev;             /* Previous element in list of dirty pages  脏页列表的以前元素*/
};

/* Bit values for PgHdr.flags */
#define PGHDR_DIRTY             0x002  /* Page has changed 页面已经改变*/
#define PGHDR_NEED_SYNC         0x004  /* Fsync the rollback journal before
                                       ** writing this page to the database 
                                       **此页面写入数据库之前FSYNC回滚日志
                                       */
#define PGHDR_NEED_READ         0x008  /* Content is unread 内容为未读*/
#define PGHDR_REUSE_UNLIKELY    0x010  /* A hint that reuse is unlikely 暗示再利用不可能*/
#define PGHDR_DONT_WRITE        0x020  /* Do not write content to disk 不写内容到磁盘*/

/* Initialize and shutdown the page cache subsystem 初始化和关闭页面缓存子系统*/
int sqlite3PcacheInitialize(void);
void sqlite3PcacheShutdown(void);

/* Page cache buffer management:页面缓存缓冲区管理: 
** These routines implement SQLITE_CONFIG_PAGECACHE.这些例程实现SQLITE_CONFIG_PAGECACHE。
*/
void sqlite3PCacheBufferSetup(void *, int sz, int n);

/* Create a new pager cache.创建一个新的页面pager缓存。
** Under memory stress, invoke xStress to try to make pages clean.内存的压力下,调用xStress试图使页面干净。
** Only clean and unpinned pages can be reclaimed.只有干净和不固定的页面可以被回收。
*/
void sqlite3PcacheOpen(
  int szPage,                    /* Size of every page 只有干净和不固定的页面可以被回收。*/
  int szExtra,                   /* Extra space associated with each page 与每一页相关的额外空间*/
  int bPurgeable,                /* True if pages are on backing store 如果页面在后备存储上就是true*/
  int (*xStress)(void*, PgHdr*), /* Call to try to make pages clean 尝试使页面干净*/
  void *pStress,                 /* Argument to xStress 参数xStress**/
  PCache *pToInit                /* Preallocated space for the PCache 为PCache预分配的空间*/
);

/* Modify the page-size after the cache has been created.  缓存创建后修改页面大小*/
void sqlite3PcacheSetPageSize(PCache *, int);

/* Return the size in bytes of a PCache object.  Used to preallocate
** storage space.
**返回PCache对象的字节大小。用于预分配存储空间。
*/
int sqlite3PcacheSize(void);

/* One release per successful fetch.  Page is pinned until released.
** Reference counted. 
**每成功获取一个版本。 页面固定，直到释放。 引用计数。
*/
int sqlite3PcacheFetch(PCache*, Pgno, int createFlag, PgHdr**);
void sqlite3PcacheRelease(PgHdr*);/*减少一个页面上的引用计数。如果页面是干净的，
                                    引用计数下降到0，则它由elible回收。
                                  */

void sqlite3PcacheDrop(PgHdr*);         /* Remove page from cache 从缓存中删除页面*/
void sqlite3PcacheMakeDirty(PgHdr*);    /* Make sure page is marked dirty 确保页面标记为脏*/
void sqlite3PcacheMakeClean(PgHdr*);    /* Mark a single page as clean 标记一个单一的页面为干净*/
void sqlite3PcacheCleanAll(PCache*);    /* Mark all dirty list pages as clean 标记所有的脏页面列表为干净*/

/* Change a page number.  Used by incr-vacuum. 改变一个页码。由incr-vacuum使用。*/
void sqlite3PcacheMove(PgHdr*, Pgno);

/* Remove all pages with pgno>x.  Reset the cache if x==0 
**删除所有与pgno>x有关的页面。如果x==0 重置缓存。
*/
void sqlite3PcacheTruncate(PCache*, Pgno x);

/* Get a list of all dirty pages in the cache, sorted by page number 
**获取在缓存中所有的脏页列表，用页面排序。
*/
PgHdr *sqlite3PcacheDirtyList(PCache*);

/* Reset and close the cache object 重置并关闭缓存对象*/
void sqlite3PcacheClose(PCache*);

/* Clear flags from pages of the page cache 来自页面缓存中的页面的清晰标志*/
void sqlite3PcacheClearSyncFlags(PCache *);

/* Discard the contents of the cache 丢弃高速缓存的内容*/
void sqlite3PcacheClear(PCache*);

/* Return the total number of outstanding page references 返回提及的所有优秀页面的总数*/
int sqlite3PcacheRefCount(PCache*);

/* Increment the reference count of an existing page  增加现存页面的引用总数*/
void sqlite3PcacheRef(PgHdr*);

int sqlite3PcachePageRefcount(PgHdr*);/*返回引用的页面提供的数量作为参数。*/

/* Return the total number of pages stored in the cache 返回存储在缓存寄存器中的页面的总页数*/
int sqlite3PcachePagecount(PCache*);

#if defined(SQLITE_CHECK_PAGES) || defined(SQLITE_DEBUG)
/* Iterate through all dirty pages currently stored in the cache. This
** interface is only available if SQLITE_CHECK_PAGES is defined when the 
** library is built.
**遍历当前存储在缓存中的所有脏页。 如果建库时SQLITE_CHECK_PAGES被定义，
**这个接口才存在。
*/
void sqlite3PcacheIterateDirty(PCache *pCache, void (*xIter)(PgHdr *));
#endif

/* Set and get the suggested cache-size for the specified pager-cache.
**设置和获取指定pager-cache建议缓存大小。 
**
** If no global maximum is configured, then the system attempts to limit
** the total number of pages cached by purgeable pager-caches to the sum
** of the suggested cache-sizes.
**如果没有全局最大配置，则系统尝试限制由pager-caches缓存到建议的页面缓存大小的总页数。
*/
void sqlite3PcacheSetCachesize(PCache *, int);
#ifdef SQLITE_TEST
int sqlite3PcacheGetCachesize(PCache *);
#endif

/* Free up as much memory as possible from the page cache 尽可能多的从页面缓存中释放内存*/
void sqlite3PcacheShrink(PCache*);

#ifdef SQLITE_ENABLE_MEMORY_MANAGEMENT
/* Try to return memory used by the pcache module to the main memory heap 尝试pcache模块使用的内存返回给主内存堆*/
int sqlite3PcacheReleaseMemory(int);
#endif

#ifdef SQLITE_TEST
void sqlite3PcacheStats(int*,int*,int*,int*);
#endif

void sqlite3PCacheSetDefault(void);

#endif /* _PCACHE_H_ */
