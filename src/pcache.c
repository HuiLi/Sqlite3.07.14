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
** This file implements that page cache.
这个文件实现了页面缓存。
*/
#include "sqliteInt.h"

/*
** A complete page cache is an instance of this structure.
一个完整的页面缓存是这种结构的一个实例。
*/
struct PCache {
  PgHdr *pDirty, *pDirtyTail;         /* List of dirty pages in LRU order   LRU顺序的脏页列表*/
  PgHdr *pSynced;                     /* Last synced page in dirty page list    在脏页列表中的最后同步页*/
  int nRef;                           /* Number of referenced pages   参考页数*/
  int szCache;                        /* Configured cache size   配置缓存大小*/
  int szPage;                         /* Size of every page in this cache   在这个高速缓存中的每一页大小*/
  int szExtra;                        /* Size of extra space for each page   每个页面的额外空间大小*/
  int bPurgeable;                     /* True if pages are on backing store   如果页面在后备存储器中就是true*/
  int (*xStress)(void*,PgHdr*);       /* Call to try make a page clean   尝试使页面干净*/
  void *pStress;                      /* Argument to xStress   参数xStress*/
  sqlite3_pcache *pCache;             /* Pluggable cache module   可插拔缓存模块*/
  PgHdr *pPage1;                      /* Reference to page 1   参考第一页*/
};

/*
** Some of the assert() macros in this code are too expensive to run
** even during normal debugging.  Use them only rarely on long-running
** tests.  Enable the expensive asserts using the
** -DSQLITE_ENABLE_EXPENSIVE_ASSERT=1 compile-time option.
**代码中的一些assert()宏代码太贵，甚至在正常调试期间不能运行。
**在长时间的运行测试中很少使用它们。使用-DSQLITE_ENABLE_EXPENSIVE_ASSERT = 1
**编译时的选项启用昂贵的asserts。
*/
#ifdef SQLITE_ENABLE_EXPENSIVE_ASSERT
# define expensive_assert(X)  assert(X)
#else
# define expensive_assert(X)
#endif

/********************************** Linked List Management ********************/

#if !defined(NDEBUG) && defined(SQLITE_ENABLE_EXPENSIVE_ASSERT)
/*
** Check that the pCache->pSynced variable is set correctly. If it
** is not, either fail an assert or return zero. Otherwise, return
** non-zero. This is only used in debugging builds, as follows:
**
**   expensive_assert( pcacheCheckSynced(pCache) );
**
**检查pCache- > pSynced变量是否设置正确。如果不正确，或者失败断言或返回零。
**否则，返回非零。这仅用于调试构建，如下所示：
**expensive_assert( pcacheCheckSynced(pCache) );
*/
static int pcacheCheckSynced(PCache *pCache){
  PgHdr *p;
  for(p=pCache->pDirtyTail; p!=pCache->pSynced; p=p->pDirtyPrev){
    assert( p->nRef || (p->flags&PGHDR_NEED_SYNC) );
  }
  return (p==0 || p->nRef || (p->flags&PGHDR_NEED_SYNC)==0);
}
#endif /* !NDEBUG && SQLITE_ENABLE_EXPENSIVE_ASSERT */

/*
** Remove page pPage from the list of dirty pages.
**从脏页列表中移除页面pPage。
*/
static void pcacheRemoveFromDirtyList(PgHdr *pPage){
  PCache *p = pPage->pCache;

  assert( pPage->pDirtyNext || pPage==p->pDirtyTail );
  assert( pPage->pDirtyPrev || pPage==p->pDirty );

  /* Update the PCache1.pSynced variable if necessary. 如果有必要就更新变量PCache1.pSynced。*/
  if( p->pSynced==pPage ){
    PgHdr *pSynced = pPage->pDirtyPrev;
    while( pSynced && (pSynced->flags&PGHDR_NEED_SYNC) ){
      pSynced = pSynced->pDirtyPrev;
    }
    p->pSynced = pSynced;
  }

  if( pPage->pDirtyNext ){
    pPage->pDirtyNext->pDirtyPrev = pPage->pDirtyPrev;
  }else{
    assert( pPage==p->pDirtyTail );
    p->pDirtyTail = pPage->pDirtyPrev;
  }
  if( pPage->pDirtyPrev ){
    pPage->pDirtyPrev->pDirtyNext = pPage->pDirtyNext;
  }else{
    assert( pPage==p->pDirty );
    p->pDirty = pPage->pDirtyNext;
  }
  pPage->pDirtyNext = 0;
  pPage->pDirtyPrev = 0;

  expensive_assert( pcacheCheckSynced(p) );
}

/*
** Add page pPage to the head of the dirty list (PCache1.pDirty is set to pPage).
添加页面pPage在脏页列表的头部(PCache1.pDirty设置为pPage )。
*/
static void pcacheAddToDirtyList(PgHdr *pPage){
  PCache *p = pPage->pCache;

  assert( pPage->pDirtyNext==0 && pPage->pDirtyPrev==0 && p->pDirty!=pPage );

  pPage->pDirtyNext = p->pDirty;
  if( pPage->pDirtyNext ){
    assert( pPage->pDirtyNext->pDirtyPrev==0 );
    pPage->pDirtyNext->pDirtyPrev = pPage;
  }
  p->pDirty = pPage;
  if( !p->pDirtyTail ){
    p->pDirtyTail = pPage;
  }
  if( !p->pSynced && 0==(pPage->flags&PGHDR_NEED_SYNC) ){
    p->pSynced = pPage;
  }
  expensive_assert( pcacheCheckSynced(p) );
}

/*
** Wrapper around the pluggable caches xUnpin method. If the cache is
** being used for an in-memory database, this function is a no-op.
**主类可插入缓存xUnpin方法。 如果缓存被用于一个内存中的数据库,
**这个函数是一个空操作。
*/
static void pcacheUnpin(PgHdr *p){
  PCache *pCache = p->pCache;
  if( pCache->bPurgeable ){
    if( p->pgno==1 ){
      pCache->pPage1 = 0;
    }
    sqlite3GlobalConfig.pcache2.xUnpin(pCache->pCache, p->pPage, 0);
  }
}

/*************************************************** General Interfaces ******
**
** Initialize and shutdown the page cache subsystem. Neither of these 
** functions are threadsafe.
**初始化和关闭页面缓存子系统。 没有这些功能是线程安全的。
*/
int sqlite3PcacheInitialize(void){
  if( sqlite3GlobalConfig.pcache2.xInit==0 ){
    /* IMPLEMENTATION-OF: R-26801-64137 If the xInit() method is NULL, then the
    ** built-in default page cache is used instead of the application defined
    ** page cache. */
    sqlite3PCacheSetDefault();
  }
  return sqlite3GlobalConfig.pcache2.xInit(sqlite3GlobalConfig.pcache2.pArg);
}
void sqlite3PcacheShutdown(void){
  if( sqlite3GlobalConfig.pcache2.xShutdown ){
    /* IMPLEMENTATION-OF: R-26000-56589 The xShutdown() method may be NULL. */
    sqlite3GlobalConfig.pcache2.xShutdown(sqlite3GlobalConfig.pcache2.pArg);
  }
}

/*
** Return the size in bytes of a PCache object.返回PCache对象的字节大小。
*/
int sqlite3PcacheSize(void){ return sizeof(PCache); }

/*
** Create a new PCache object. Storage space to hold the object
** has already been allocated and is passed in as the p pointer. 
** The caller discovers how much space needs to be allocated by 
** calling sqlite3PcacheSize().
**创建一个新的PCache对象。 存储空间的对象已经被分配,作为p指针传递。 
**调用者通过调用sqlite3PcacheSize()发现需要分配多少空间。
*/
void sqlite3PcacheOpen(
  int szPage,                  /* Size of every page 每一页的大小*/
  int szExtra,                 /* Extra space associated with each page 与每一页相关的额外空间*/
  int bPurgeable,              /* True if pages are on backing store 如果页面在后备存储器中就是true*/
  int (*xStress)(void*,PgHdr*),/* Call to try to make pages clean 尝试使页面干净*/
  void *pStress,               /* Argument to xStress 参数xStress*/
  PCache *p                    /* Preallocated space for the PCache 为PCache预分配 的空间*/
){
  memset(p, 0, sizeof(PCache));
  p->szPage = szPage;
  p->szExtra = szExtra;
  p->bPurgeable = bPurgeable;
  p->xStress = xStress;
  p->pStress = pStress;
  p->szCache = 100;
}

/*
** Change the page size for PCache object. The caller must ensure that there
** are no outstanding page references when this function is called.
**改变页面大小PCache对象。当该函数被调用时调用者必须确保没有
**突出的页面引用它。
*/
void sqlite3PcacheSetPageSize(PCache *pCache, int szPage){
  assert( pCache->nRef==0 && pCache->pDirty==0 );
  if( pCache->pCache ){
    sqlite3GlobalConfig.pcache2.xDestroy(pCache->pCache);
    pCache->pCache = 0;
    pCache->pPage1 = 0;
  }
  pCache->szPage = szPage;
}

/*
** Compute the number of pages of cache requested.计算页面的缓存请求的数量。
*/
static int numberOfCachePages(PCache *p){
  if( p->szCache>=0 ){
    return p->szCache;
  }else{
    return (int)((-1024*(i64)p->szCache)/(p->szPage+p->szExtra));
  }
}

/*
** Try to obtain a page from the cache.试图从缓存中获取一个页面。
*/
int sqlite3PcacheFetch(
  PCache *pCache,       /* Obtain the page from this cache  从这个缓存中获取页面*/
  Pgno pgno,            /* Page number to obtain 获取的页面数量*/
  int createFlag,       /* If true, create page if it does not exist already 如果是真的，如果不存在就创建页面*/
  PgHdr **ppPage        /* Write the page here 在这里写入页面*/
){
  sqlite3_pcache_page *pPage = 0;
  PgHdr *pPgHdr = 0;
  int eCreate;

  assert( pCache!=0 );
  assert( createFlag==1 || createFlag==0 );
  assert( pgno>0 );

  /* If the pluggable cache (sqlite3_pcache*) has not been allocated,
  ** allocate it now.
  **如果可插拔缓存（ sqlite3_pcache * ）尚未分配，现在分配它。
  */
  if( !pCache->pCache && createFlag ){
    sqlite3_pcache *p;
    p = sqlite3GlobalConfig.pcache2.xCreate(
        pCache->szPage, pCache->szExtra + sizeof(PgHdr), pCache->bPurgeable
    );
    if( !p ){
      return SQLITE_NOMEM;
    }
    sqlite3GlobalConfig.pcache2.xCachesize(p, numberOfCachePages(pCache));
    pCache->pCache = p;
  }

  eCreate = createFlag * (1 + (!pCache->bPurgeable || !pCache->pDirty));
  if( pCache->pCache ){
    pPage = sqlite3GlobalConfig.pcache2.xFetch(pCache->pCache, pgno, eCreate);
  }

  if( !pPage && eCreate==1 ){
    PgHdr *pPg;

    /* Find a dirty page to write-out and recycle. First try to find a 
    ** page that does not require a journal-sync (one with PGHDR_NEED_SYNC
    ** cleared), but if that is not possible settle for any other 
    ** unreferenced dirty page.
    **找到一个脏页去写和再利用。首先尝试找到一个页面不需要一个journal-sync
    **(一个PGHDR_NEED_SYNC清零)，但是如果那样，不可能满足任何没有引用的脏页
    */
    expensive_assert( pcacheCheckSynced(pCache) );
    for(pPg=pCache->pSynced; 
        pPg && (pPg->nRef || (pPg->flags&PGHDR_NEED_SYNC)); 
        pPg=pPg->pDirtyPrev
    );
    pCache->pSynced = pPg;
    if( !pPg ){
      for(pPg=pCache->pDirtyTail; pPg && pPg->nRef; pPg=pPg->pDirtyPrev);
    }
    if( pPg ){
      int rc;
#ifdef SQLITE_LOG_CACHE_SPILL
      sqlite3_log(SQLITE_FULL, 
                  "spill page %d making room for %d - cache used: %d/%d",
                  pPg->pgno, pgno,
                  sqlite3GlobalConfig.pcache.xPagecount(pCache->pCache),
                  numberOfCachePages(pCache));
#endif
      rc = pCache->xStress(pCache->pStress, pPg);
      if( rc!=SQLITE_OK && rc!=SQLITE_BUSY ){
        return rc;
      }
    }

    pPage = sqlite3GlobalConfig.pcache2.xFetch(pCache->pCache, pgno, 2);
  }

  if( pPage ){
    pPgHdr = (PgHdr *)pPage->pExtra;

    if( !pPgHdr->pPage ){
      memset(pPgHdr, 0, sizeof(PgHdr));
      pPgHdr->pPage = pPage;
      pPgHdr->pData = pPage->pBuf;
      pPgHdr->pExtra = (void *)&pPgHdr[1];
      memset(pPgHdr->pExtra, 0, pCache->szExtra);
      pPgHdr->pCache = pCache;
      pPgHdr->pgno = pgno;
    }
    assert( pPgHdr->pCache==pCache );
    assert( pPgHdr->pgno==pgno );
    assert( pPgHdr->pData==pPage->pBuf );
    assert( pPgHdr->pExtra==(void *)&pPgHdr[1] );

    if( 0==pPgHdr->nRef ){
      pCache->nRef++;
    }
    pPgHdr->nRef++;
    if( pgno==1 ){
      pCache->pPage1 = pPgHdr;
    }
  }
  *ppPage = pPgHdr;
  return (pPgHdr==0 && eCreate) ? SQLITE_NOMEM : SQLITE_OK;
}

/*
** Decrement the reference count on a page. If the page is clean and the
** reference count drops to 0, then it is made elible for recycling.
**减少一个页面上的引用计数。如果页面是干净的，引用计数下降到0，则它由elible回收。
*/
void sqlite3PcacheRelease(PgHdr *p){
  assert( p->nRef>0 );
  p->nRef--;
  if( p->nRef==0 ){
    PCache *pCache = p->pCache;
    pCache->nRef--;
    if( (p->flags&PGHDR_DIRTY)==0 ){
      pcacheUnpin(p);
    }else{
      /* Move the page to the head of the dirty list. 移动页面到脏列表的头部*/
      pcacheRemoveFromDirtyList(p);
      pcacheAddToDirtyList(p);
    }
  }
}

/*
** Increase the reference count of a supplied page by 1.提供的页面引用总数增加1.
*/
void sqlite3PcacheRef(PgHdr *p){
  assert(p->nRef>0);
  p->nRef++;
}

/*
** Drop a page from the cache. There must be exactly one reference to the
** page. This function deletes that reference, so after it returns the
** page pointed to by p is invalid.
**从缓存中删除一个页面。这里必须有一个参考页。这个功能会删除参考，所以在返回页面指向p是无效的。
*/
void sqlite3PcacheDrop(PgHdr *p){
  PCache *pCache;
  assert( p->nRef==1 );
  if( p->flags&PGHDR_DIRTY ){
    pcacheRemoveFromDirtyList(p);
  }
  pCache = p->pCache;
  pCache->nRef--;
  if( p->pgno==1 ){
    pCache->pPage1 = 0;
  }
  sqlite3GlobalConfig.pcache2.xUnpin(pCache->pCache, p->pPage, 1);
}

/*
** Make sure the page is marked as dirty. If it isn't dirty already,make it so.
**确保页面标记为脏。如果不是脏页面，就让它如此。
*/
void sqlite3PcacheMakeDirty(PgHdr *p){
  p->flags &= ~PGHDR_DONT_WRITE;
  assert( p->nRef>0 );
  if( 0==(p->flags & PGHDR_DIRTY) ){
    p->flags |= PGHDR_DIRTY;
    pcacheAddToDirtyList( p);
  }
}

/*
** Make sure the page is marked as clean. If it isn't clean already,make it so.
**确保页面标记为干净。如果已经是干净的页面，就让它如此。
*/
void sqlite3PcacheMakeClean(PgHdr *p){
  if( (p->flags & PGHDR_DIRTY) ){
    pcacheRemoveFromDirtyList(p);
    p->flags &= ~(PGHDR_DIRTY|PGHDR_NEED_SYNC);
    if( p->nRef==0 ){
      pcacheUnpin(p);
    }
  }
}

/*
** Make every page in the cache clean.使在高速缓存中的页面干净。
*/
void sqlite3PcacheCleanAll(PCache *pCache){
  PgHdr *p;
  while( (p = pCache->pDirty)!=0 ){
    sqlite3PcacheMakeClean(p);
  }
}

/*
** Clear the PGHDR_NEED_SYNC flag from all dirty pages.从脏页面中清除PGHDR_NEED_SYNC标记。
*/
void sqlite3PcacheClearSyncFlags(PCache *pCache){
  PgHdr *p;
  for(p=pCache->pDirty; p; p=p->pDirtyNext){
    p->flags &= ~PGHDR_NEED_SYNC;
  }
  pCache->pSynced = pCache->pDirtyTail;
}

/*
** Change the page number of page p to newPgno. 改变页面p的页码为newPgno。
*/
void sqlite3PcacheMove(PgHdr *p, Pgno newPgno){
  PCache *pCache = p->pCache;
  assert( p->nRef>0 );
  assert( newPgno>0 );
  sqlite3GlobalConfig.pcache2.xRekey(pCache->pCache, p->pPage, p->pgno,newPgno);
  p->pgno = newPgno;
  if( (p->flags&PGHDR_DIRTY) && (p->flags&PGHDR_NEED_SYNC) ){
    pcacheRemoveFromDirtyList(p);
    pcacheAddToDirtyList(p);
  }
}

/*
** Drop every cache entry whose page number is greater than "pgno". The
** caller must ensure that there are no outstanding references to any pages
** other than page 1 with a page number greater than pgno.
**减少每个页码大于"pgno"的缓存条目。 调用者必须
**确保没有突出以外的任何页面的引用页码大于pgno第1页。 
**
** If there is a reference to page 1 and the pgno parameter passed to this
** function is 0, then the data area associated with page 1 is zeroed, but
** the page object is not dropped.
**如果有一个参考页面1和pgno参数传递给此函数的是0，
**则与第1页相关的数据区被归零，但页面对象未被丢弃
*/
void sqlite3PcacheTruncate(PCache *pCache, Pgno pgno){
  if( pCache->pCache ){
    PgHdr *p;
    PgHdr *pNext;
    for(p=pCache->pDirty; p; p=pNext){
      pNext = p->pDirtyNext;
      /* This routine never gets call with a positive pgno except right
      ** after sqlite3PcacheCleanAll().  So if there are dirty pages,
      ** it must be that pgno==0.
      ** 这个程序从来没有得到一个积极的pgno，除了sqlite3PcacheCleanAll () 。因此，如果有脏页，
	    ** 它必须是pgno== 0 。
      */
      assert( p->pgno>0 );
      if( ALWAYS(p->pgno>pgno) ){
        assert( p->flags&PGHDR_DIRTY );
        sqlite3PcacheMakeClean(p);
      }
    }
    if( pgno==0 && pCache->pPage1 ){
      memset(pCache->pPage1->pData, 0, pCache->szPage);
      pgno = 1;
    }
    sqlite3GlobalConfig.pcache2.xTruncate(pCache->pCache, pgno+1);
  }
}

/*
** Close a cache.关闭缓存。
*/
void sqlite3PcacheClose(PCache *pCache){
  if( pCache->pCache ){
    sqlite3GlobalConfig.pcache2.xDestroy(pCache->pCache);
  }
}

/* 
** Discard the contents of the cache.丢弃缓存的内容。
*/
void sqlite3PcacheClear(PCache *pCache){
  sqlite3PcacheTruncate(pCache, 0);
}

/*
** Merge two lists of pages connected by pDirty and in pgno order.
** Do not both fixing the pDirtyPrev pointers.
**合并两个列表的页面由pDirty连接，pgno顺序。 不修复pDirtyPrev指针。 
*/
static PgHdr *pcacheMergeDirtyList(PgHdr *pA, PgHdr *pB){
  PgHdr result, *pTail;
  pTail = &result;
  while( pA && pB ){
    if( pA->pgno<pB->pgno ){
      pTail->pDirty = pA;
      pTail = pA;
      pA = pA->pDirty;
    }else{
      pTail->pDirty = pB;
      pTail = pB;
      pB = pB->pDirty;
    }
  }
  if( pA ){
    pTail->pDirty = pA;
  }else if( pB ){
    pTail->pDirty = pB;
  }else{
    pTail->pDirty = 0;
  }
  return result.pDirty;
}

/*
** Sort the list of pages in accending order by pgno.  Pages are
** connected by pDirty pointers.  The pDirtyPrev pointers are
** corrupted by this sort.
**由pgno按照accending顺序对页面列表进行排序。页面由pDirty指针连接。
**pDirtyPrev指针由这个顺序损坏。
**
** Since there cannot be more than 2^31 distinct pages in a database,
** there cannot be more than 31 buckets required by the merge sorter.
** One extra bucket is added to catch overflow in case something
** ever changes to make the previous sentence incorrect.
*/
#define N_SORT_BUCKET  32
static PgHdr *pcacheSortDirtyList(PgHdr *pIn){
  PgHdr *a[N_SORT_BUCKET], *p;
  int i;
  memset(a, 0, sizeof(a));
  while( pIn ){
    p = pIn;
    pIn = p->pDirty;
    p->pDirty = 0;
    for(i=0; ALWAYS(i<N_SORT_BUCKET-1); i++){
      if( a[i]==0 ){
        a[i] = p;
        break;
      }else{
        p = pcacheMergeDirtyList(a[i], p);
        a[i] = 0;
      }
    }
    if( NEVER(i==N_SORT_BUCKET-1) ){
      /* To get here, there need to be 2^(N_SORT_BUCKET) elements in
      ** the input list.  But that is impossible.
      ** 到这里，我们需要有2 ^（ N_SORT_BUCKET ）个元素在输入列表中。但是，
	    ** 这是不可能的。
      */
      a[i] = pcacheMergeDirtyList(a[i], p);
    }
  }
  p = a[0];
  for(i=1; i<N_SORT_BUCKET; i++){
    p = pcacheMergeDirtyList(p, a[i]);
  }
  return p;
}

/*
** Return a list of all dirty pages in the cache, sorted by page number.
**返回缓存中所有的脏页列表，按页码顺序排序。
*/
PgHdr *sqlite3PcacheDirtyList(PCache *pCache){
  PgHdr *p;
  for(p=pCache->pDirty; p; p=p->pDirtyNext){
    p->pDirty = p->pDirtyNext;
  }
  return pcacheSortDirtyList(pCache->pDirty);
}

/* 
** Return the total number of referenced pages held by the cache.
**返回由高速缓存保持引用的页的总数。
*/
int sqlite3PcacheRefCount(PCache *pCache){
  return pCache->nRef;
}

/*
** Return the number of references to the page supplied as an argument.
**返回引用的页面提供的数量作为参数。
*/
int sqlite3PcachePageRefcount(PgHdr *p){
  return p->nRef;
}

/* 
** Return the total number of pages in the cache.返回在高速缓存中的页面总数。
*/
int sqlite3PcachePagecount(PCache *pCache){
  int nPage = 0;
  if( pCache->pCache ){
    nPage = sqlite3GlobalConfig.pcache2.xPagecount(pCache->pCache);
  }
  return nPage;
}

#ifdef SQLITE_TEST
/*
** Get the suggested cache-size value.获取建议的缓存大小值。
*/
int sqlite3PcacheGetCachesize(PCache *pCache){
  return numberOfCachePages(pCache);
}
#endif

/*
** Set the suggested cache-size value.设置建议的缓存大小值。
*/
void sqlite3PcacheSetCachesize(PCache *pCache, int mxPage){
  pCache->szCache = mxPage;
  if( pCache->pCache ){
    sqlite3GlobalConfig.pcache2.xCachesize(pCache->pCache,
                                           numberOfCachePages(pCache));
  }
}

/*
** Free up as much memory as possible from the page cache.从页面缓存中释放尽可能多的内存。
*/
void sqlite3PcacheShrink(PCache *pCache){
  if( pCache->pCache ){
    sqlite3GlobalConfig.pcache2.xShrink(pCache->pCache);
  }
}

#if defined(SQLITE_CHECK_PAGES) || defined(SQLITE_DEBUG)
/*
** For all dirty pages currently in the cache, invoke the specified
** callback. This is only used if the SQLITE_CHECK_PAGES macro is
** defined.
** 目前在缓存中的所有脏页面，调用指定的回调。如果SQLITE_CHECK_PAGES宏被定义时才使用。
*/
void sqlite3PcacheIterateDirty(PCache *pCache, void (*xIter)(PgHdr *)){
  PgHdr *pDirty;
  for(pDirty=pCache->pDirty; pDirty; pDirty=pDirty->pDirtyNext){
    xIter(pDirty);
  }
}
#endif
