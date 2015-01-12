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
** 这个文件实现了页面缓存
*/
#include "sqliteInt.h"
/*
** A complete page cache is an instance of this structure.  
**一个完整的页面缓存是这种结构的一个实例。 
**补充:
**Cache的定义，在这一层上，是没有内存淘汰算法的，只是记录一些信息
*/
struct PCache {
  PgHdr *pDirty, *pDirtyTail;         /* List of dirty pages in LRU order   按LRU次序
  排列的缓冲区脏页链表 ，补充:(*pDirty 是脏列表首指针)和PgHdr中的pDirtyNext、pDirtyPrev一起使用*/
  PgHdr *pSynced;                     /* Last synced page in dirty page list  	脏页链
  表中最近同步过的页*/
  int nRef;                           /* Number of referenced pages  缓冲区中页的引用计数 */
  int szCache;                        /* Configured cache size   缓冲区大小*/
  int szPage;                         /* Size of every page in this cache  缓冲区中每页的大小*/
  int szExtra;                        /* Size of extra space for each page   每页扩
  展空间的大小*/
  int bPurgeable;                     /* True if pages are on backing store  页面是否已经备份，页缓冲区是否可净化标识*/
  int (*xStress)(void*,PgHdr*);       /* Call to try make a page clean   用于清除页面内容*/
  void *pStress;                      /* Argument to xStress  	xStress参数 */
  sqlite3_pcache *pCache;             /* Pluggable cache module  可填充的缓存模块*/
  PgHdr *pPage1;                      /* Reference to page 1     指向第一页指针*/
};

/*
** Some of the assert() macros in this code are too expensive to run
** even during normal debugging.  Use them only rarely on long-running
** tests.  Enable the expensive asserts using the
** -DSQLITE_ENABLE_EXPENSIVE_ASSERT=1 compile-time option.
**
** 即使在正常运行调试中，这段代码中的一些宏定义的assert(维护) 函数运行花销是巨大的
** 很少在长时间运行测试中使用它们。
** 选用这种花销代价很大的维护方法仅当DSQLITE_ENABLE_EXPENSIVE_ASSERT = 1编译时选择
*/
#ifdef SQLITE_ENABLE_EXPENSIVE_ASSERT
# define expensive_assert(X)  assert(X)
#else
# define expensive_assert(X)
#endif

/********************************** Linked List Management		连接链表管理 ********************/

#if !defined(NDEBUG) && defined(SQLITE_ENABLE_EXPENSIVE_ASSERT)
/*
** Check that the pCache->pSynced variable is set correctly. If it
** is not, either fail an assert or return zero. Otherwise, return
** non-zero. This is only used in debugging builds, as follows:
**
** 检查pCache - > pSynced变量设置正确
** 如果它不正确，维护失败或者返回0.否则返回
** 非0。这个函数仅仅当运行调试建立时可用，如以下情况:
**
** expensive_assert( pcacheCheckSynced(pCache) );
** 花销代价很大的维护( pcacheCheckSynced(pCache) )
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
** 从脏页面列表中 移除页面pPage
*/

static void pcacheRemoveFromDirtyList(PgHdr *pPage){
  PCache *p = pPage->pCache;
  /*  assert接受两个参数，一个就是bool值，另一个是如果违反了
  断言将会产生的异常的字面，异常字面值可以省略*/
  assert( pPage->pDirtyNext || pPage==p->pDirtyTail );
  assert( pPage->pDirtyPrev || pPage==p->pDirty );

  /* Update the PCache1.pSynced variable if necessary.  	更新PCache1.pSynced变量如果有必要*/
  if( p->pSynced==pPage ){//如果该页是最近被同步过的页，则更新pSynced?  //将该页前一个指针给pSyced指针
    PgHdr *pSynced = pPage->pDirtyPrev;
    while( pSynced && (pSynced->flags&PGHDR_NEED_SYNC) ){
      pSynced = pSynced->pDirtyPrev;
    }
    p->pSynced = pSynced;
  }
  /*通过变化指针就可以实现移除该页*/
  if( pPage->pDirtyNext ){/*修改该脏页prev指针，指向它后面prev。从前往后改*/
    pPage->pDirtyNext->pDirtyPrev = pPage->pDirtyPrev;
  }else{
    assert( pPage==p->pDirtyTail );
    p->pDirtyTail = pPage->pDirtyPrev;
  }
  if( pPage->pDirtyPrev ){/*修改该脏页next指针，指向它后面prev。从后往前改*/
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
** Add page pPage to the head of the dirty list (PCache1.pDirty is set to
** pPage).
**添加pPage这个页面到脏列表的首部(PCache1.pDirty用于设置 pPage)。
**
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
** 包装可插入缓存xUnpin方法。如果缓存被用于一个内存中的
** 数据库,这个函数是一个空操作。
** 使页不被钉住（使其可被回收）
*/
static void pcache1Unpin(sqlite3
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
** 初始化和关闭页面缓存子系统。这些功能都不是线程安全的。
*/
int sqlite3PcacheInitialize(void){
  if( sqlite3GlobalConfig.pcache2.xInit==0 ){
    /* IMPLEMENTATION-OF: R-26801-64137 If the xInit() method is NULL, then the
    ** built-in default page cache is used instead of the application defined
    ** page cache. 
    ** 如果xInit() 方法为空，建立一个默认的页面cache去代替函数定义的(cache)
    */
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
** Return the size in bytes of a PCache object.
** 返回PCache对象的大小(字节数)
*/
int sqlite3PcacheSize(void){ return sizeof(PCache); }

/*
** Create a new PCache object. Storage space to hold the object
** has already been allocated and is passed in as the p pointer. 
** The caller discovers how much space needs to be allocated by 
** calling sqlite3PcacheSize().
** 创建一个新的 PCache 对象。存储空间的对象已经被分配,并用p指针
** 传递。 通过调用sqlite3PcacheSize()，调用者能发现需要分配多少空间。
*/
void sqlite3PcacheOpen(
  int szPage,                  /* Size of every page 	每个页面的大小*/
  int szExtra,                 /* Extra space associated with each page 	和每个页面相关的额外空间*/
  int bPurgeable,              /* True if pages are on backing store 	如果页面是在辅存中为真*/
  int (*xStress)(void*,PgHdr*),/* Call to try to make pages clean 		用于清理页面*/
  void *pStress,               /* Argument to xStress 		xStress 	的参数*/
  PCache *p                    /* Preallocated space for the PCache 	给PCache预先分配的空间 	*/
){
  memset(p, 0, sizeof(PCache));
  p->szPage = szPage;
  p->szExtra = szExtra;
  p->bPurgeable = bPurgeable;
  p->xStress = xStress;
  p->pStress = pStress;
  p->szCache = 100;//cache默认大小为100字节
}

/*
** Change the page size for PCache object. The caller must ensure that there
** are no outstanding page references when this function is called.
** 改变PCache页面的大小。 调用此函数时，调用者必须确保没有未处理的页面被引用
**
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
** Compute the number of pages of cache requested.
** 计算缓存 请求页面数量
*/
static int numberOfCachePages(PCache *p){
  if( p->szCache>=0 ){
    return p->szCache;
  }else{
    return (int)((-1024*(i64)p->szCache)/(p->szPage+p->szExtra));
  }
}

/*
** Try to obtain a page from the cache.
** 尝试从cache中获取页面
*/
int sqlite3PcacheFetch(
  PCache *pCache,       /* Obtain the page from this cache 这个缓存区所包含的页面*/
  Pgno pgno,            /* Page number to obtain 页码*/
  int createFlag,       /* If true, create page if it does not exist already 为真，当页不存在时创建该页*/
  PgHdr **ppPage        /* Write the page here 在这里写页*/
){

  /* If the pluggable cache (sqlite3_pcache*) has not been allocated,
  **  allocate it now.
  **  如果可插入缓存(sqlite3_pcache *)尚未分配,现在给它分配
  */
  if( !pCache->pCache && createFlag ){/*确定该缓存确实不存在*/
    sqlite3_pcache *p;
    p = sqlite3GlobalConfig.pcache2.xCreate(/*不存在就创建一个*/
        pCache->szPage, pCache->szExtra + sizeof(PgHdr), pCache->bPurgeable
    );
    if( !p ){/*如果没有创建成功，则说明内存不足，返回SQLITE_NOMEM*/
      return SQLITE_NOMEM;
    }
	//再次调用 sqlite3GlobalConfig，算需要分配多少的空间
    sqlite3GlobalConfig.pcache2.xCachesize(p, numberOfCachePages(pCache));
    pCache->pCache = p;
  }
  //不是立即再请求分配空间，而是看一下是否有空间可以回收
  eCreate = createFlag * (1 + (!pCache->bPurgeable || !pCache->pDirty));
 
  if( pCache->pCache ){/*如果该缓存已经存在直接获取它*/
  	/*获取缓冲区中第pgno页*/
    pPage = sqlite3GlobalConfig.pcache2.xFetch(pCache->pCache, pgno, eCreate);
  }
   //eCreate为1，则说明也不存在，且cache可以被净化且脏列表不为空
  if( !pPage && eCreate==1 ){
    PgHdr *pPg;

    /* Find a dirty page to write-out and recycle. First try to find a 
    ** page that does not require a journal-sync (one with PGHDR_NEED_SYNC
    ** cleared), but if that is not possible settle for any other 
    ** unreferenced dirty page.
    ** 找到一个脏页标记并回收。首先找到一个不需要日志同步的页面
    ** 一个有 PGHDR_NEED_SYNC标识为已清洁的),但是是不可能满足任何其他
    ** 未被引用的脏页面
    */
    expensive_assert( pcacheCheckSynced(pCache) );
    for(pPg=pCache->pSynced; //如果该页面才被同步，遍历脏页面链表
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
      //日志，显示溢出**(页码)页的空间给cache的**页用
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
** 减少页面的引用计数。
** 如果页面是干净和引用计数减少为0,那么它是由elible回收
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
      /* Move the page to the head of the dirty list. 	把页面移动到脏列表首部*/
      pcacheRemoveFromDirtyList(p);
      pcacheAddToDirtyList(p);
    }
  }
}

/*
** Increase the reference count of a supplied page by 1.
** 提供页面的引用计数增加1。
*/
void sqlite3PcacheRef(PgHdr *p){
  assert(p->nRef>0);
  p->nRef++;
}

/*
** Drop a page from the cache. There must be exactly one reference to the
** page. This function deletes that reference, so after it returns the
** page pointed to by p is invalid.
** 从cache中删除页面。
** 必须有一个页面的引用。这个函数的作用是删除这个引用
** 所以它返回的页面指针p是无效的
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
** Make sure the page is marked as dirty. If it isn't dirty already,
** make it so.
** 确保页面被标记为脏页面。如果它不是脏页面就标记为是脏页面
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
** Make sure the page is marked as clean. If it isn't clean already,
** make it so.
** 确保页面标记为干净页面。如果不是干净页面就标记为干净页面
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
** Make every page in the cache clean.
** 使在cache中的每个页面都是干净的
*/
void sqlite3PcacheCleanAll(PCache *pCache){
  PgHdr *p;
  while( (p = pCache->pDirty)!=0 ){
    sqlite3PcacheMakeClean(p);
  }
}

/*
** Clear the PGHDR_NEED_SYNC flag from all dirty pages.
** 从所有脏页面中清除PGHDR_NEED_SYNC标识
*/
void sqlite3PcacheClearSyncFlags(PCache *pCache){
  PgHdr *p;
  for(p=pCache->pDirty; p; p=p->pDirtyNext){
    p->flags &= ~PGHDR_NEED_SYNC;
  }
  pCache->pSynced = pCache->pDirtyTail;
}

/*
** Change the page number of page p to newPgno. 
** 改变页面的页码p为newPgno
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
** 删除页码大于“pgno”的每个缓存项。调用者必须确保每个页码
** 大于pgno的页面都没有外部引用,除了page1
**
** If there is a reference to page 1 and the pgno parameter passed to this
** function is 0, then the data area associated with page 1 is zeroed, but
** the page object is not dropped.
** 如果有一个函数引用了page1,并且pgno参数传递给该函数的值为0，
** 这时和page1关联的数据域被调整为0，但是页面对象没有被删除
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
      ** 这个程序从来没有被pgno主动调用过，除了在sqlite3PcacheCleanAll()(确保页面是干净的)执行后。
      ** 所以如果有脏页面，必须把pano设置为0.
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
** Close a cache.
** 关闭cache
*/
void sqlite3PcacheClose(PCache *pCache){
  if( pCache->pCache ){
    sqlite3GlobalConfig.pcache2.xDestroy(pCache->pCache);
  }
}

/* 
** Discard the contents of the cache.
** 丢弃缓存(cache)的内容。
*/
void sqlite3PcacheClear(PCache *pCache){
  sqlite3PcacheTruncate(pCache, 0);
}

/*
** 通过pDirty和pgno页码，合并两个页面列表。
** 不合并pDirtyPrev的指针。
** 设指针pA 与pB分别均指向两个链表的
** 当前结点，只需要每次比较当前的这两个节点，
** 谁小就把谁加入到pTail（新链表）中，然后小的
** 这个链表的指针继续后移。
*/
static PgHdr *pcacheMergeDirtyList(PgHdr *pA, PgHdr *pB){
  PgHdr result, *pTail;
  pTail = &result;
  while( pA && pB ){// 不指向末尾
    if( pA->pgno<pB->pgno ){ // 如果pA页码比pB小，插入pA
      pTail->pDirty = pA;    
      pTail = pA; // pTail后移
      pA = pA->pDirty;//pA后移
    }else{
      pTail->pDirty = pB;
      pTail = pB;
      pB = pB->pDirty;
    }
  } //结束该循环后应该会剩下一个链表没合并完(假设俩个链表长度不一样) 
  if( pA ){//当遍历完后
    pTail->pDirty = pA; //剩余
  }else if( pB ){
    pTail->pDirty = pB;
  }else{
    pTail->pDirty = 0;
  }
  return result.pDirty;
}
/*
** Sort the list of pages in ascending order by pgno.  Pages are
** connected by pDirty pointers.  The pDirtyPrev pointers are
** corrupted by this sort.
** 按照pgno升序排列页面列表。页面用pDirty指针连接。
** pDirtyPrev指针会被这种排序毁掉
**
** Since there cannot be more than 2^31 distinct pages in a database,
** there cannot be more than 31 buckets required by the merge sorter.
** One extra bucket is added to catch overflow in case something
** ever changes to make the previous sentence incorrect.
** 因为在数据库中不可能有超过2-31个不同的页面，所以
** 这个合并分类函数不可能需要31个以上的桶。
** 添加一个额外的桶抓溢出,以防一些变化使前面的句子不正确。
*/
#define N_SORT_BUCKET  32
static PgHdr *pcacheSortDirtyList(PgHdr *pIn){
  PgHdr *a[N_SORT_BUCKET], *p;
  int i;
  memset(a, 0, sizeof(a));//分配给a 32 个空间
  while( pIn ){//需加入的脏页不为空，则:
    p = pIn;
    pIn = p->pDirty;
    p->pDirty = 0;
    for(i=0; ALWAYS(i<N_SORT_BUCKET-1); i++){//always定义了三种情况， 、
                                             //这里只是保证结果为
                                             //N_SORT_BUCKET-1值
      if( a[i]==0 ){//a[i] 为0，则直接插入
        a[i] = p;
        break;
      }else{ //否则合并两个脏列表
        p = pcacheMergeDirtyList(a[i], p);
        a[i] = 0; 
      }
    }
    if( NEVER(i==N_SORT_BUCKET-1) ){
      /* To get here, there need to be 2^(N_SORT_BUCKET) elements in
      ** the input list.  But that is impossible.
      ** 在输入列表中，这里需要2-(N_SORT_BUCKET) 个元素。
      但这是不可能的
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
** 返回在cache中的所有脏页面列表，用页面排序
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
** 返回cache引用页面的总数
*/
int sqlite3PcacheRefCount(PCache *pCache){
  return pCache->nRef;
}

/*
** Return the number of references to the page supplied as an argument.
** 返回作为参数的页的引用数量
*/
int sqlite3PcachePageRefcount(PgHdr *p){
  return p->nRef;
}



/* 
** Return the total number of pages in the cache.
** 返回所有在cache中的页面数
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
** Get the suggested cache-size value.
** 获得建议的cache大小值
*/
int sqlite3PcacheGetCachesize(PCache *pCache){
  return numberOfCachePages(pCache);
}
#endif

/*
** Set the suggested cache-size value.
** 设置建议的cache大小值
*/
void sqlite3PcacheSetCachesize(PCache *pCache, int mxPage){
  pCache->szCache = mxPage;
  if( pCache->pCache ){
    sqlite3GlobalConfig.pcache2.xCachesize(pCache->pCache,
                                           numberOfCachePages(pCache));
  }
}

/*
** Free up as much memory as possible from the page cache.
** 腾出尽可能多的内存页面缓存。
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
** 当前在cache中的所有脏页面，可调用一个特定的函数回调。
** 这个函数仅当SQLITE_CHECK_PAGES已经被宏定义时
*/
void sqlite3PcacheIterateDirty(PCache *pCache, void (*xIter)(PgHdr *)){
  PgHdr *pDirty;
  for(pDirty=pCache->pDirty; pDirty; pDirty=pDirty->pDirtyNext){
    xIter(pDirty);
  }
}
#endif
