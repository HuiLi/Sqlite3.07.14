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
**这个头文件定义了sqlite page cache子系统的接口
*/

#ifndef _PCACHE_H_

typedef struct PgHdr PgHdr;
typedef struct PCache PCache;

/*
** Every page in the cache is controlled by an instance of the following structure.
** 缓冲区中的每个页由此结构的一个实例来控制。
*/
struct PgHdr {
  sqlite3_pcache_page *pPage;    /* Pcache object page handle      存放页实际数据的结构*/
  void *pData;                   /* Page data  页的实际内容*/
  void *pExtra;                  /* Extra content 页的扩展内容*/
  PgHdr *pDirty;                 /* Transient list of dirty pages   暂时的脏页链表，在写入脏页到库中时使用*/
  Pager *pPager;                 /* The pager this page is part of   页所属的Pager */
  Pgno pgno;                     /* Page number for this page   页码*/
#ifdef SQLITE_CHECK_PAGES
  u32 pageHash;                  /* Hash of page content   页内容的hash*/
#endif
  u16 flags;                     /* PGHDR flags defined below PGHDR  下面定义PGHDR标记*/
  /**********************************************************************
  ** Elements above are public.  All that follows is private to pcache.c
  ** and should not be accessed by other modules.
  **上面的域是公共的，下面的域是pcache.c私有的，其他模块不能存取
  **此结构是外部（Pager）直接使用的页结构，通过此结构来对实际缓冲区中的页进行操作。
  */
  
  i16 nRef;                      /* Number of users of this page  此页的引用计数*/
  PCache *pCache;                /* Cache that owns this page  此页所属的缓冲区,指向cache列表 */

  PgHdr *pDirtyNext;             /* Next element in list of dirty pages	 脏页链表中的Next指针，
                                                           指向下一个脏页*/
  PgHdr *pDirtyPrev;             /* Previous element in list of dirty pages 	脏页链表中的Previous指针
  								      指向前一个脏页*/
};
/* Bit values for PgHdr.flags 
** PgHdr.flags的值
*/
#define PGHDR_DIRTY             0x002  /* 页面已经修改时*/
#define PGHDR_NEED_SYNC         0x004  /* 在写这个页面时同步回滚日志 */
#define PGHDR_NEED_READ         0x008  /* 内容不可读*/
#define PGHDR_REUSE_UNLIKELY    0x010  /* 暗示不可重用*/
#define PGHDR_DONT_WRITE        0x020  /* 不能向磁盘写内容*/

/* Initialize and shutdown the page cache subsystem 
** 初始化和关闭页面缓存子系统
*/
int sqlite3PcacheInitialize(void);
void sqlite3PcacheShutdown(void);

/* Page cache buffer management:
** These routines implement SQLITE_CONFIG_PAGECACHE. 
** 页面缓存缓冲区管理
** 这些实例实现了SQLITE_CONFIG_PAGECACHE
*/
void sqlite3PCacheBufferSetup(void *, int sz, int n);

/* Create a new pager cache.
** Under memory stress, invoke xStress to try to make pages clean.
** Only clean and unpinned pages can be reclaimed.
** 创建一个新页面缓存
** 内存的压力下，调用xStress 保持页面干净
** 只有清洁的和不再需要的页面可以被回收
*/
void sqlite3PcacheOpen(
  int szPage,                    /* Size of every page  	每一个页面的大小*/
  int szExtra,                   /* Extra space associated with each page 		和每个页面关联的额外空间*/
  int bPurgeable,                /* True if pages are on backing store 	如果这个页面是在辅存中则为真*/
  int (*xStress)(void*, PgHdr*), /* Call to try to make pages clean 	用于清除页面*/
  void *pStress,                 /* Argument to xStress 	xStress 的参数*/
  PCache *pToInit                /* Preallocated space for the PCache 	为PCache 预先分配的空间*/
);

/* Modify the page-size after the cache has been created.
 **在缓存被创建后修改页面大小
*/
void sqlite3PcacheSetPageSize(PCache *, int);

/* Return the size in bytes of a PCache object.  Used to preallocate
** storage space.
** 返回邋PCache对象的值(字节数).用于预先分配存储空间
*/
int sqlite3PcacheSize(void);

/* One release per successful fetch.  Page is pinned until released.
** Reference counted. 
** 每成功获取一个版本，页面被锁定，知道被释放
** 引用计数
*/
int sqlite3PcacheFetch(PCache*, Pgno, int createFlag, PgHdr**);
/*外部具体获取一个页的实现代码,在pache.c中被定义*/
void sqlite3PcacheRelease(PgHdr*);

void sqlite3PcacheDrop(PgHdr*);         /* Remove page from cache 		从cache中移除页面*/
void sqlite3PcacheMakeDirty(PgHdr*);    /* Make sure page is marked dirty 	确定页面已经被标记为脏页面*/
void sqlite3PcacheMakeClean(PgHdr*);    /* Mark a single page as clean 		标记单个页面为干净的*/
void sqlite3PcacheCleanAll(PCache*);    /* Mark all dirty list pages as clean 	标记所有脏页面列表都是干净的*/

/* Change a page number.  Used by incr-vacuum.	
 ** 改变一个页面的编号，由incr-vacuum 使用
*/
void sqlite3PcacheMove(PgHdr*, Pgno);

/* Remove all pages with pgno>x.  Reset the cache if x==0 	用pgno的x移除所有页面。重置cache当x==0时*/
void sqlite3PcacheTruncate(PCache*, Pgno x);

/* Get a list of all dirty pages in the cache, sorted by page number 		获得cache中所有脏页面的一个列表，
 **按页面编号排序
*/
PgHdr *sqlite3PcacheDirtyList(PCache*);

/* Reset and close the cache object	重置并关闭这个cache对象 */
void sqlite3PcacheClose(PCache*);

/* Clear flags from pages of the page cache 	从页面的页面缓存中清除flags标记*/
void sqlite3PcacheClearSyncFlags(PCache *);

/* Discard the contents of the cache 	丢掉这个cache的内容*/
void sqlite3PcacheClear(PCache*);

/* Return the total number of outstanding page references  	返回所有未解决的页面引用的编号*/
int sqlite3PcacheRefCount(PCache*);

/* Increment the reference count of an existing page		增加现有页面的引用数 */
void sqlite3PcacheRef(PgHdr*);

int sqlite3PcachePageRefcount(PgHdr*);

/* Return the total number of pages stored in the cache 		返回所有存储在cache中的页面编号*/
int sqlite3PcachePagecount(PCache*);

#if defined(SQLITE_CHECK_PAGES) || defined(SQLITE_DEBUG)
/* Iterate through all dirty pages currently stored in the cache. This
** interface is only available if SQLITE_CHECK_PAGES is defined when the 
** library is built.
** 遍历所有当前存储在cache中的脏页面
** 库被建立时，这个接口只能当SQLITE_CHECK_PAGES 被定义时可用
*/
void sqlite3PcacheIterateDirty(PCache *pCache, void (*xIter)(PgHdr *));
#endif

/* Set and get the suggested cache-size for the specified pager-cache.
**
** If no global maximum is configured, then the system attempts to limit
** the total number of pages cached by purgeable pager-caches to the sum
** of the suggested cache-sizes.
** 设置和获取指定pager-cache建议的 缓存大小
**
** 如果没有配置全局变量maximum，系统会尝试限制所有页面缓存的数量，通过
** 可净化的页面缓存区统计建议缓存大小
**
**
** 如果全局最大值已经被配置，那么系统尝试去限制页面总数，
** 这个页面是通过所建议的缓存大小的总数去缓存的可清除的页面缓存
*/
void sqlite3PcacheSetCachesize(PCache *, int);
#ifdef SQLITE_TEST
int sqlite3PcacheGetCachesize(PCache *);
#endif

/* Free up as much memory as possible from the page cache 		腾出尽可能多的内存页面缓存*/
void sqlite3PcacheShrink(PCache*);

#ifdef SQLITE_ENABLE_MEMORY_MANAGEMENT
/* Try to return memory used by the pcache module to the main memory heap 
** 尝试将pcache模块使用的内存返回给主内存堆
*/
int sqlite3PcacheReleaseMemory(int);
#endif

#ifdef SQLITE_TEST
void sqlite3PcacheStats(int*,int*,int*,int*);
#endif

void sqlite3PCacheSetDefault(void);

#endif /* _PCACHE_H_ */
