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
**
** sqlite3GlobalConfig的结构体 
** Structure containing global configuration data for the SQLite library.
** This structure also contains some state information.
** 全局配置数据，也包括一些状态信息
** struct Sqlite3Config {
**  int bMemstat;                     //True to enable memory status  为真则开启内存状态
**  int bCoreMutex;                   // True to enable core mutexing  为真，则开启核互斥
**  int bFullMutex;                   // True to enable full mutexing  为真，则开启全互斥
**  int bOpenUri;                     // True to interpret filenames as URIs  为真，则将文件名转化为URI
**  int mxStrlen;                     // Maximum string length  最大字符串长度
**  int szLookaside;                  // Default lookaside buffer size  默认后备缓存大小
**  int nLookaside;                   // Default lookaside buffer count  默认后备缓存数目
**  sqlite3_mem_methods m;            // Low-level memory allocation interface  底层内存分配器接口
**  sqlite3_mutex_methods mutex;      // Low-level mutex interface  底层互斥接口
**  sqlite3_pcache_methods2 pcache2;  // Low-level page-cache interface  底层页缓存接口
**  void *pHeap;                      // Heap storage space  堆存储空间
**  int nHeap;                        // Size of pHeap[]  pHeadp[]大小
**  int mnReq, mxReq;                 // Min and max heap requests sizes  最小和最大堆请求大小 
**  void *pScratch;                   // Scratch memory  临时内存
**  int szScratch;                    // Size of each scratch buffer  每个临时缓存的大小 
**  int nScratch;                     // Number of scratch buffers  临时缓存大数目
**  void *pPage;                      // Page cache memory 页缓存
**  int szPage;                       // Size of each page in pPage[]  每个页缓存的大小
**  int nPage;                        // Number of pages in pPage[]  页的数目
**  int mxParserStack;                // maximum depth of the parser stack  解析栈的最大深度
**  int sharedCacheEnabled;           // true if shared-cache mode enabled  如果共享缓存模式开启了，则为真
**  // The above might be initialized to non-zero.  The following need to always
**  ** initially be zero, however. 
**  //以上的变量的初始值可为非零值，以下的变量则必须初始化为零
**  int isInit;                       // True after initialization has finished  初始化完成以后置为真
**  int inProgress;                   // True while initialization in progress  初始化过程中则为真
**  int isMutexInit;                  // True after mutexes are initialized  互斥锁初始化以后为真
**  int isMallocInit;                 // True after malloc is initialized  分配初始化以后为真
**  int isPCacheInit;                 // True after malloc is initialized   分配初始化以后为真
**  sqlite3_mutex *pInitMutex;        // Mutex used by sqlite3_initialize()  初始化函数中使用的锁
**  int nRefInitMutex;                // Number of users of pInitMutex  用户使用的pInitMutex的数量
**  void (*xLog)(void*,int,const char*); // Function for logging  记录日志函数
**  void *pLogArg;                       // First argument to xLog()  xLog()函数的第一个参数
**  int bLocaltimeFault;              // True to fail localtime() calls  localtime()函数调用失败为真
** };
**
**
** Memory allocation functions used throughout sqlite.
**
** 内存分配函数贯穿整个sqlite
*/
#include "sqliteInt.h"
#include <stdarg.h>

/*
** Attempt to release up to n bytes of non-essential memory currently
** held by SQLite. An example of non-essential memory is memory used to
** cache database pages that are not currently in use.
** 
** 试图释放当前不重要的被sqlite占有的内存，
** 一个例子不重要的内存的例子就是当前没有用到的缓存数据库页
*/
int sqlite3_release_memory(int n){/*释放内存*/
#ifdef SQLITE_ENABLE_MEMORY_MANAGEMENT//编译时使用SQLITE_ENABLE_MEMORY_MANAGEMENT
  return sqlite3PcacheReleaseMemory(n);//返回一个sqlite3PcacheReleaseMemory函数
#else
  /* IMPLEMENTATION-OF: R-34391-24921 The sqlite3_release_memory() routine
  ** is a no-op returning zero if SQLite is not compiled with
  ** SQLITE_ENABLE_MEMORY_MANAGEMENT. */
  //如果sqlite编译没有使用SQLITE_ENABLE_MEMORY_MANAGEMENT，sqlite3_release_memory将是一个返回的空操作
  NUSED_PARAMETER(n);//返回0的空操作
  return 0;
#endif
}

/*
** An instance of the following object records the location of   
** each unused scratch buffer.
**
** 接下来的例子记录了每一个没有用到的缓冲区的位置
*/
typedef struct ScratchFreeslot {		/*结构体  记录空闲缓冲区*/
  struct ScratchFreeslot *pNext;   /* Next unused scratch buffer *//*结构指针*/
} ScratchFreeslot;

/*
** State information local to the memory allocation subsystem.
**
** 内存分配子系统的状态信息
*/
static SQLITE_WSD struct Mem0Global {/* SQLITE_WSD 是const* MemGlobal结构体 */
  sqlite3_mutex *mutex;         /* Mutex to serialize access  串行访问的变量的锁*/

  /*
  ** The alarm callback and its arguments.  The mem0.mutex lock will
  ** be held while the callback is running.  Recursive calls into
  ** the memory subsystem are allowed, but no new callbacks will be
  ** issued.
  **
  ** 回凋和参数
  ** 当回调函数被执行，mem0.mutex加锁，进入内存子系统的递归调用将会被执行，但是不能执行新的回调
  */
  sqlite3_int64 alarmThreshold;  //申明长整型的变量 预警值
  void (*alarmCallback)(void*, sqlite3_int64,int);  //互斥锁的回调函数
  void *alarmArg;

  /*
  ** Pointers to the end of sqlite3GlobalConfig.pScratch memory
  ** (so that a range test can be used to determine if an allocation
  ** being freed came from pScratch) and a pointer to the list of
  ** unused scratch allocations. 
  ** 
  ** 指向临时内存得末端 释放的分配是否来自临时内存的指针和没有用到的临时内存
  */
  void *pScratchEnd;  /*指向缓冲区的末端*/
  ScratchFreeslot *pScratchFree;  /*指向空闲缓冲区*/
  u32 nScratchFree;  /*无符号整型 表示连续空闲缓冲区的个数*/

  /*
  ** True if heap is nearly "full" where "full" is defined by the
  ** sqlite3_soft_heap_limit() setting.
  ** 
  ** 当由sqlite3_soft_heap_limit()定义的堆快满时，置该值为真
  */
  int nearlyFull; 
} mem0 = { 0, 0, 0, 0, 0, 0, 0, 0 };

#define mem0 GLOBAL(struct Mem0Global, mem0)//定义全局函数

/*
** This routine runs when the memory allocator sees that the
** total memory allocation is about to exceed the soft heap
** limit.
**
** 这个例程是在当内存分配将超过软队限制的大小时执行。
*/
static void softHeapLimitEnforcer(
  void *NotUsed, //是否被占用用
  sqlite3_int64 NotUsed2,//没有被利用
  int allocSize//分配的大小
){
  UNUSED_PARAMETER2(NotUsed, NotUsed2);//没有被利用的元素
  sqlite3_release_memory(allocSize);//释放内存
}

/*
** Change the alarm callback
**
** 改变警告回调
*/
static int sqlite3MemoryAlarm(
  void(*xCallback)(void *pArg, sqlite3_int64 used,int N),  //回调函数
  void *pArg,
  sqlite3_int64 iThreshold
){
  int nUsed;  //有n块可用
  sqlite3_mutex_enter(mem0.mutex);
  mem0.alarmCallback = xCallback;  /*调用回调函数*/
  mem0.alarmArg = pArg;
  mem0.alarmThreshold = iThreshold;  /*报警阈值*/
  nUsed = sqlite3StatusValue(SQLITE_STATUS_MEMORY_USED);
  mem0.nearlyFull = (iThreshold>0 && iThreshold<=nUsed);  /*内存是否溢出*/
  sqlite3_mutex_leave(mem0.mutex);
  return SQLITE_OK;
}

#ifndef SQLITE_OMIT_DEPRECATE
/*
** Deprecated external interface.  Internal/core SQLite code
** should call sqlite3MemoryAlarm.
**
** 废弃的额外的接口
*/
int sqlite3_memory_alarm(
  void(*xCallback)(void *pArg, sqlite3_int64 used,int N),  /*回调函数*/
  void *pArg,
  sqlite3_int64 iThreshold
){
  return sqlite3MemoryAlarm(xCallback, pArg, iThreshold);
}
#endif

/*
** Set the soft heap-size limit for the library. Passing a zero or 
** negative value indicates no limit.
**
** 内存使用限制。
** sqlite3_soft_heap_limit64()机制可以让应用程序设置SQLite的内存使用限制。
** SQLite会从缓存中重用内存，而不是分配新的内存，以满足设置的限制。
*/
sqlite3_int64 sqlite3_soft_heap_limit64(sqlite3_int64 n){
  sqlite3_int64 priorLimit;
  sqlite3_int64 excess;
#ifndef SQLITE_OMIT_AUTOINIT  
  int rc = sqlite3_initialize();
  if( rc ) return -1;
#endif
  sqlite3_mutex_enter(mem0.mutex);	
  priorLimit = mem0.alarmThreshold;	 /*最小阈值*/
  sqlite3_mutex_leave(mem0.mutex);
  if( n<0 ) return priorLimit;
  if( n>0 ){
    sqlite3MemoryAlarm(softHeapLimitEnforcer, 0, n);/*发出警告*/
  }else{
    sqlite3MemoryAlarm(0, 0, 0);
  }
  excess = sqlite3_memory_used() - n;
  if( excess>0 ) sqlite3_release_memory((int)(excess & 0x7fffffff));  /*越界就释放内存*/
  return priorLimit;
}
void sqlite3_soft_heap_limit(int n){  /*堆限制*/
  if( n<0 ) n = 0;
  sqlite3_soft_heap_limit64(n);  /*内存使用限制*/
}

/*
** Initialize the memory allocation subsystem 
** 
** 初始化内存分配子系统
*/
int sqlite3MallocInit(void){
  if( sqlite3GlobalConfig.m.xMalloc==0 ){/*没有分配*/
    sqlite3MemSetDefault();
  }
  memset(&mem0, 0, sizeof(mem0));/*分配内存*/
  if( sqlite3GlobalConfig.bCoreMutex ){/*核心锁*/
    mem0.mutex = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MEM);/*分配一个互斥锁*/
  }
  if( sqlite3GlobalConfig.pScratch && sqlite3GlobalConfig.szScratch>=100/*szScratch每个缓冲区的大小*/
      && sqlite3GlobalConfig.nScratch>0 ){/*nScratch缓冲区的数量*/
    int i, n, sz;
    ScratchFreeslot *pSlot;/*空闲缓冲区的指针*/
    sz = ROUNDDOWN8(sqlite3GlobalConfig.szScratch);/*四舍五入*/
    sqlite3GlobalConfig.szScratch = sz;/*重新获得的值给szScratch*/
    pSlot = (ScratchFreeslot*)sqlite3GlobalConfig.pScratch;/*将sqlite3GlobalConfig头指针给pSlot*/
    n = sqlite3GlobalConfig.nScratch;/*缓冲区数量给变量n*/
    mem0.pScratchFree = pSlot;/*pSlot给mem0的空闲缓冲区地址*/
    mem0.nScratchFree = n;/*缓冲光的个数nScratchFree是n*/
    for(i=0; i<n-1; i++){/*求出最后一个缓冲区的地址*/
      pSlot->pNext = (ScratchFreeslot*)(sz+(char*)pSlot);
      pSlot = pSlot->pNext;
    }

    pSlot->pNext = 0;
    mem0.pScratchEnd = (void*)&pSlot[1];/*mem0的最后地址pSlot*/
  }else{/*空间不够全部清空为0*/
    mem0.pScratchEnd = 0;
    sqlite3GlobalConfig.pScratch = 0;
    sqlite3GlobalConfig.szScratch = 0;
    sqlite3GlobalConfig.nScratch = 0;
  }
  if( sqlite3GlobalConfig.pPage==0 || sqlite3GlobalConfig.szPage<512
      || sqlite3GlobalConfig.nPage<1 ){/*pPage页缓存没有分配地址 或者地址小于1 nPage页数小于一页 */
    sqlite3GlobalConfig.pPage = 0;
    sqlite3GlobalConfig.szPage = 0;
    sqlite3GlobalConfig.nPage = 0;
  }
  return sqlite3GlobalConfig.m.xInit(sqlite3GlobalConfig.m.pAppData);
}

/*
** Return true if the heap is currently under memory pressure - in other
** words if the amount of heap used is close to the limit set by
** sqlite3_soft_heap_limit().
**
** 如果堆内存没有满，返回真
*/
int sqlite3HeapNearlyFull(void){
  return mem0.nearlyFull;/*内存是否满*/
}

/*
** Deinitialize the memory allocation subsystem.
** 
** 取消初始化内存分配子系统
*/
void sqlite3MallocEnd(void){
  if( sqlite3GlobalConfig.m.xShutdown ){
    sqlite3GlobalConfig.m.xShutdown(sqlite3GlobalConfig.m.pAppData);/*解除所有资源*/
  }
  memset(&mem0, 0, sizeof(mem0));/*men0数据全部清0*/
}

/*
** Return the amount of memory currently checked out.
** 
** 返回当前使用的内存数量
*/
sqlite3_int64 sqlite3_memory_used(void){
  int n, mx;
  sqlite3_int64 res;
  sqlite3_status(SQLITE_STATUS_MEMORY_USED, &n, &mx, 0);
  res = (sqlite3_int64)n;  /* Work around bug in Borland C. Ticket #3216 */
  return res;
}

/*
** Return the maximum amount of memory that has ever been 
** checked out since either the beginning of this process
** or since the most recent reset.
**
** 返回曾经使用的内存最大值
*/
sqlite3_int64 sqlite3_memory_highwater(int resetFlag){
  int n, mx;
  sqlite3_int64 res;
  sqlite3_status(SQLITE_STATUS_MEMORY_USED, &n, &mx, resetFlag);
  res = (sqlite3_int64)mx;  /* Work around bug in Borland C. Ticket #3216 */
  return res;
}

/*
** Trigger the alarm 
**
** 触发警告
*/
static void sqlite3MallocAlarm(int nByte){
  void (*xCallback)(void*,sqlite3_int64,int);
  sqlite3_int64 nowUsed;
  void *pArg;
  if( mem0.alarmCallback==0 ) return;/*没有回调 直接返回*/
  xCallback = mem0.alarmCallback;
  nowUsed = sqlite3StatusValue(SQLITE_STATUS_MEMORY_USED);/*现在是否被占用*/
  pArg = mem0.alarmArg;/*备份警告参数*/
  mem0.alarmCallback = 0;/*回调为0*/
  sqlite3_mutex_leave(mem0.mutex);/*解除锁定*/
  xCallback(pArg, nowUsed, nByte);/*调用回调*/
  sqlite3_mutex_enter(mem0.mutex);/*进入锁定状态*/
  mem0.alarmCallback = xCallback;/*还原结构体参数*/
  mem0.alarmArg = pArg;
}

/*
** Do a memory allocation with statistics and alarms.  Assume the
** lock is already held.
**
** 分配内存，更新统计和设置警报，假设已经加锁
*/
static int mallocWithAlarm(int n, void **pp){
  int nFull;
  void *p;
  assert( sqlite3_mutex_held(mem0.mutex) );/*假设执行锁*/
  nFull = sqlite3GlobalConfig.m.xRoundup(n);/*将请求的大小进行舍入处理*/
  sqlite3StatusSet(SQLITE_STATUS_MALLOC_SIZE, n);/*设置状态*/
  if( mem0.alarmCallback!=0 ){/*发生警告*/
    int nUsed = sqlite3StatusValue(SQLITE_STATUS_MEMORY_USED);/*此时被占用*/
    if( nUsed >= mem0.alarmThreshold - nFull ){/*占用空间大于剩下的值*/
      mem0.nearlyFull = 1;/*内存已经满*/
      sqlite3MallocAlarm(nFull);/*分配警告*/
    }else{
      mem0.nearlyFull = 0;/*否则显示没有满*/
    }
  }
  p = sqlite3GlobalConfig.m.xMalloc(nFull);/*重新分配指针*/
#ifdef SQLITE_ENABLE_MEMORY_MANAGEMENT/*预处理*/
  if( p==0 && mem0.alarmCallback ){
    sqlite3MallocAlarm(nFull);
    p = sqlite3GlobalConfig.m.xMalloc(nFull);
  }
#endif
  if( p ){
    nFull = sqlite3MallocSize(p);/*分配大小*/
    sqlite3StatusAdd(SQLITE_STATUS_MEMORY_USED, nFull);/*加入状态*/
    sqlite3StatusAdd(SQLITE_STATUS_MALLOC_COUNT, 1);
  }
  *pp = p;
  return nFull;
}

/*
** Allocate memory.  This routine is like sqlite3_malloc() except that it
** assumes the memory subsystem has already been initialized.
**
** 分配内存，这个函数像sqlite3_malloc()，但不必初始化内存子系统
*/
void *sqlite3Malloc(int n){
  void *p;
  if( n<=0               /* IMP: R-65312-04917 */ 
   || n>=0x7fffff00
  ){
    /* A memory allocation of a number of bytes which is near the maximum
    ** signed integer value might cause an integer overflow inside of the
    ** xMalloc().  Hence we limit the maximum size to 0x7fffff00, giving
    ** 255 bytes of overhead.  SQLite itself will never use anything near
    ** this amount.  The only way to reach the limit is with sqlite3_malloc() 
    ** 
    ** 限制最大的值为0x7fffff00，
    */
    p = 0;
  }else if( sqlite3GlobalConfig.bMemstat ){//允许分配
    sqlite3_mutex_enter(mem0.mutex);
    mallocWithAlarm(n, &p);
    sqlite3_mutex_leave(mem0.mutex);
  }else{
    p = sqlite3GlobalConfig.m.xMalloc(n);//重新分配
  }
  assert( EIGHT_BYTE_ALIGNMENT(p) );  /* IMP: R-04675-44850 */
  return p;
}

/*
** This version of the memory allocation is for use by the application.
** First make sure the memory subsystem is initialized, then do the
** allocation.
**
** 这个版本的内存分配是由应用程序使用，使用前需要确保内存分配子系统要初始化，之后再进行内存分配
*/
void *sqlite3_malloc(int n){
#ifndef SQLITE_OMIT_AUTOINIT
  if( sqlite3_initialize() ) return 0;
#endif
  return sqlite3Malloc(n);
}

/*
** Each thread may only have a single outstanding allocation from
** xScratchMalloc().  We verify this constraint in the single-threaded
** case by setting scratchAllocOut to 1 when an allocation
** is outstanding clearing it when the allocation is freed.
** 
** 每个线程可能只有一个单独的分配器，我们通过设置scratchAllocOut=1在单线程验证这个约束
*/
#if SQLITE_THREADSAFE==0 && !defined(NDEBUG)
static int scratchAllocOut = 0;
#endif


/*
** Allocate memory that is to be used and released right away.
** This routine is similar to alloca() in that it is not intended
** for situations where the memory might be held long-term.  This
** routine is intended to get memory to old large transient data
** structures that would not normally fit on the stack of an
** embedded processor.
**
** 分配一些立即使用并释放的内存，
** 该例程类似于alloc()，即适用于内存不被长期占有的情形
** 例子是为了用了很久的内存，大内存，临时内存的数据结构，该数据结构不能满足嵌入式处理器的堆栈
*/
void *sqlite3ScratchMalloc(int n){
  void *p;
  assert( n>0 );

  sqlite3_mutex_enter(mem0.mutex);/*进入加锁状态*/
  if( mem0.nScratchFree && sqlite3GlobalConfig.szScratch>=n ){/*存在缓冲区 每个缓冲区大于n*/
    p = mem0.pScratchFree;/*空闲缓冲区*/
    mem0.pScratchFree = mem0.pScratchFree->pNext;/*指向下一个空闲缓冲区*/
    mem0.nScratchFree--;/*缓冲区个数减少*/
    sqlite3StatusAdd(SQLITE_STATUS_SCRATCH_USED, 1);/*加入状态*/
    sqlite3StatusSet(SQLITE_STATUS_SCRATCH_SIZE, n);
    sqlite3_mutex_leave(mem0.mutex);/*释放锁*/
  }else{
    if( sqlite3GlobalConfig.bMemstat ){/*能分配内存*/
      sqlite3StatusSet(SQLITE_STATUS_SCRATCH_SIZE, n);/*设置状态*/
      n = mallocWithAlarm(n, &p);/*分配个数*/
      if( p ) //存在空闲缓冲区
      	sqlite3StatusAdd(SQLITE_STATUS_SCRATCH_OVERFLOW, n);//加入状态
      sqlite3_mutex_leave(mem0.mutex);//释放锁
    }else{
      sqlite3_mutex_leave(mem0.mutex);
      p = sqlite3GlobalConfig.m.xMalloc(n);//重新分配
    }
    sqlite3MemdebugSetType(p, MEMTYPE_SCRATCH);
  }
  assert( sqlite3_mutex_notheld(mem0.mutex) );


#if SQLITE_THREADSAFE==0 && !defined(NDEBUG)
  /* Verify that no more than two scratch allocations per thread
  ** are outstanding at one time.  (This is only checked in the
  ** single-threaded case since checking in the multi-threaded case
  ** would be much more complicated.) */
  /*确定每个线程在同一时间没有超过两个内存分配*/
  assert( scratchAllocOut<=1 );
  if( p ) scratchAllocOut++;
#endif

  return p;
}
void sqlite3ScratchFree(void *p){/*释放缓冲区*/
  if( p ){

#if SQLITE_THREADSAFE==0 && !defined(NDEBUG)
    /* Verify that no more than two scratch allocation per thread
    ** is outstanding at one time.  (This is only checked in the
    ** single-threaded case since checking in the multi-threaded case
    ** would be much more complicated.) */
    assert( scratchAllocOut>=1 && scratchAllocOut<=2 );/*没有两个以上缓冲区 释放*/
    scratchAllocOut--;
#endif

    if( p>=sqlite3GlobalConfig.pScratch && p<mem0.pScratchEnd ){//当前地址大于全局地址并且小于缓冲区末端
      /* Release memory from the SQLITE_CONFIG_SCRATCH allocation */
    	//释放SQLITE_CONFIG_SCRATCH的内存
      ScratchFreeslot *pSlot;
      pSlot = (ScratchFreeslot*)p;
      sqlite3_mutex_enter(mem0.mutex);
      pSlot->pNext = mem0.pScratchFree;
      mem0.pScratchFree = pSlot;
      mem0.nScratchFree++;
      assert( mem0.nScratchFree <= (u32)sqlite3GlobalConfig.nScratch );
      sqlite3StatusAdd(SQLITE_STATUS_SCRATCH_USED, -1);
      sqlite3_mutex_leave(mem0.mutex);
    }else{
      /* Release memory back to the heap */
    	/*释放内存到堆中*/
      assert( sqlite3MemdebugHasType(p, MEMTYPE_SCRATCH) );
      assert( sqlite3MemdebugNoType(p, ~MEMTYPE_SCRATCH) );
      sqlite3MemdebugSetType(p, MEMTYPE_HEAP);
      if( sqlite3GlobalConfig.bMemstat ){
        int iSize = sqlite3MallocSize(p);//分配的大小
        sqlite3_mutex_enter(mem0.mutex);
        sqlite3StatusAdd(SQLITE_STATUS_SCRATCH_OVERFLOW, -iSize);//加入状态
        sqlite3StatusAdd(SQLITE_STATUS_MEMORY_USED, -iSize);
        sqlite3StatusAdd(SQLITE_STATUS_MALLOC_COUNT, -1);
        sqlite3GlobalConfig.m.xFree(p);//释放内存
        sqlite3_mutex_leave(mem0.mutex);
      }else{
        sqlite3GlobalConfig.m.xFree(p);
      }
    }
  }
}

/*
** TRUE if p is a lookaside memory allocation from db
**
** p是否是后备内存
*/
#ifndef SQLITE_OMIT_LOOKASIDE
static int isLookaside(sqlite3 *db, void *p){
  return p && p>=db->lookaside.pStart && p<db->lookaside.pEnd;
}
#else
#define isLookaside(A,B) 0
#endif

/*
** Return the size of a memory allocation previously obtained from
** sqlite3Malloc() or sqlite3_malloc().
** 
** 返回利用sqlite3Malloc()和sqlite3_malloc()函数分配得到的内存大小。
*/
int sqlite3MallocSize(void *p){
  assert( sqlite3MemdebugHasType(p, MEMTYPE_HEAP) );
  assert( sqlite3MemdebugNoType(p, MEMTYPE_DB) );
  return sqlite3GlobalConfig.m.xSize(p);
}
int sqlite3DbMallocSize(sqlite3 *db, void *p){
  assert( db==0 || sqlite3_mutex_held(db->mutex) );
  if( db && isLookaside(db, p) ){
    return db->lookaside.sz;
  }else{
    assert( sqlite3MemdebugHasType(p, MEMTYPE_DB) );
    assert( sqlite3MemdebugHasType(p, MEMTYPE_LOOKASIDE|MEMTYPE_HEAP) );
    assert( db!=0 || sqlite3MemdebugNoType(p, MEMTYPE_LOOKASIDE) );
    return sqlite3GlobalConfig.m.xSize(p);
  }
}

/*
** Free memory previously obtained from sqlite3Malloc().
** 
** 释放从sqlite3Malloc函数中得到的内存
*/
void sqlite3_free(void *p){
  if( p==0 ) return;  /* IMP: R-49053-54554 */
  assert( sqlite3MemdebugNoType(p, MEMTYPE_DB) );
  assert( sqlite3MemdebugHasType(p, MEMTYPE_HEAP) );
  if( sqlite3GlobalConfig.bMemstat ){
    sqlite3_mutex_enter(mem0.mutex);
    sqlite3StatusAdd(SQLITE_STATUS_MEMORY_USED, -sqlite3MallocSize(p));
    sqlite3StatusAdd(SQLITE_STATUS_MALLOC_COUNT, -1);
    sqlite3GlobalConfig.m.xFree(p);
    sqlite3_mutex_leave(mem0.mutex);
  }else{
    sqlite3GlobalConfig.m.xFree(p);
  }
}

/*
** Free memory that might be associated with a particular database
** connection.
**
** 释放数据库连接关联的内存
*/
void sqlite3DbFree(sqlite3 *db, void *p){
  assert( db==0 || sqlite3_mutex_held(db->mutex) );
  if( db ){
    if( db->pnBytesFreed ){
      *db->pnBytesFreed += sqlite3DbMallocSize(db, p);
      return;
    }
    if( isLookaside(db, p) ){
      LookasideSlot *pBuf = (LookasideSlot*)p;
#if SQLITE_DEBUG
      /* Trash all content in the buffer being freed */
      memset(p, 0xaa, db->lookaside.sz);
#endif
      pBuf->pNext = db->lookaside.pFree;
      db->lookaside.pFree = pBuf;
      db->lookaside.nOut--;
      return;
    }
  }
  assert( sqlite3MemdebugHasType(p, MEMTYPE_DB) );
  assert( sqlite3MemdebugHasType(p, MEMTYPE_LOOKASIDE|MEMTYPE_HEAP) );
  assert( db!=0 || sqlite3MemdebugNoType(p, MEMTYPE_LOOKASIDE) );
  sqlite3MemdebugSetType(p, MEMTYPE_HEAP);
  sqlite3_free(p);
}

/*
** Change the size of an existing memory allocation
** 
** 改变存在内存分配的大小
*/
void *sqlite3Realloc(void *pOld, int nBytes){
  int nOld, nNew, nDiff;
  void *pNew;
  if( pOld==0 ){//以前内存为0
    return sqlite3Malloc(nBytes); /* IMP: R-28354-25769 */
  }
  if( nBytes<=0 ){//分配空间不合法
    sqlite3_free(pOld); /* IMP: R-31593-10574 */
    return 0;
  }
  if( nBytes>=0x7fffff00 ){
    /* The 0x7ffff00 limit term is explained in comments on sqlite3Malloc() */
    return 0;
  }
  nOld = sqlite3MallocSize(pOld);//计算分配的大小
  /* IMPLEMENTATION-OF: R-46199-30249 SQLite guarantees that the second
  ** argument to xRealloc is always a value returned by a prior call to
  ** xRoundup. */
  nNew = sqlite3GlobalConfig.m.xRoundup(nBytes);//四舍五入给nNew
  if( nOld==nNew ){//新的等于旧的
    pNew = pOld;//新指针指向pOld
  }else if( sqlite3GlobalConfig.bMemstat ){
    sqlite3_mutex_enter(mem0.mutex);
    sqlite3StatusSet(SQLITE_STATUS_MALLOC_SIZE, nBytes);
    nDiff = nNew - nOld;//计算机差值
    if( sqlite3StatusValue(SQLITE_STATUS_MEMORY_USED) >= 
          mem0.alarmThreshold-nDiff ){
      sqlite3MallocAlarm(nDiff);
    }
    assert( sqlite3MemdebugHasType(pOld, MEMTYPE_HEAP) );
    assert( sqlite3MemdebugNoType(pOld, ~MEMTYPE_HEAP) );
    pNew = sqlite3GlobalConfig.m.xRealloc(pOld, nNew);//重新分配新指针
    if( pNew==0 && mem0.alarmCallback ){//指针没有分配 发生警告
      sqlite3MallocAlarm(nBytes);//重新分配
      pNew = sqlite3GlobalConfig.m.xRealloc(pOld, nNew);
    }
    if( pNew ){
      nNew = sqlite3MallocSize(pNew);//计算机新的内存大小
      sqlite3StatusAdd(SQLITE_STATUS_MEMORY_USED, nNew-nOld);
    }
    sqlite3_mutex_leave(mem0.mutex);
  }else{
    pNew = sqlite3GlobalConfig.m.xRealloc(pOld, nNew);
  }
  assert( EIGHT_BYTE_ALIGNMENT(pNew) ); /* IMP: R-04675-44850 */
  return pNew;
}

/*
** The public interface to sqlite3Realloc.  Make sure that the memory
** subsystem is initialized prior to invoking sqliteRealloc.
**
** sqlite3Realloc公共接口，确保内存子系统在调用sqliteRealloc前已经初始化
*/
void *sqlite3_realloc(void *pOld, int n){
#ifndef SQLITE_OMIT_AUTOINIT
  if( sqlite3_initialize() ) return 0;
#endif
  return sqlite3Realloc(pOld, n);
}


/*
** Allocate and zero memory.
**
** 分配0内存
*/ 
void *sqlite3MallocZero(int n){
  void *p = sqlite3Malloc(n);
  if( p ){
    memset(p, 0, n);
  }
  return p;
}

/*
** Allocate and zero memory.  If the allocation fails, make
** the mallocFailed flag in the connection pointer.
**
** 分配0内存，如果分配失败，就在连接指针中置分配失败的标志。
*/
void *sqlite3DbMallocZero(sqlite3 *db, int n){
  void *p = sqlite3DbMallocRaw(db, n);
  if( p ){
    memset(p, 0, n);
  }
  return p;
}

/*
** Allocate and zero memory.  If the allocation fails, make
** the mallocFailed flag in the connection pointer.
**
** If db!=0 and db->mallocFailed is true (indicating a prior malloc
** failure on the same database connection) then always return 0.
** Hence for a particular database connection, once malloc starts
** failing, it fails consistently until mallocFailed is reset.
** This is an important assumption.  There are many places in the
** code that do things like this:
**
**         int *a = (int*)sqlite3DbMallocRaw(db, 100);
**         int *b = (int*)sqlite3DbMallocRaw(db, 200);
**         if( b ) a[10] = 9;
**
** In other words, if a subsequent malloc (ex: "b") worked, it is assumed
** that all prior mallocs (ex: "a") worked too.
**
** 分配0内存，如果分配失败了，在数据库连接中置分配失败的标志
** 如果db不存在 或者失败分配 那就返回0
** 因此一个特定的数据库连接，只要一个分配失败，之后的分配都会失败
** 这是很重要的假设，代码中很多地方都是这样的
** 
** 如果sqlite3DbMallocRaw后一个分配成功的话，说明之前的分配都会成功。
*/
void *sqlite3DbMallocRaw(sqlite3 *db, int n){
  void *p;
  assert( db==0 || sqlite3_mutex_held(db->mutex) );
  assert( db==0 || db->pnBytesFreed==0 );
#ifndef SQLITE_OMIT_LOOKASIDE  //定义宏
  if( db ){
    LookasideSlot *pBuf;  //pBuf指向一个大的连续的内存块
    if( db->mallocFailed ){  //分配失败 返回0
      return 0;    }
      ///*0：命中。 1：不是完全命中。 2：全部未命中*/
    if( db->lookaside.bEnabled ){ 
      if( n>db->lookaside.sz ){  //如果给的n大于每个缓冲区大小
        db->lookaside.anStat[1]++;
      }else if( (pBuf = db->lookaside.pFree)==0 ){
        db->lookaside.anStat[2]++;
      }else{
        db->lookaside.pFree = pBuf->pNext;
        db->lookaside.nOut++;
        db->lookaside.anStat[0]++;  //命中
        if( db->lookaside.nOut>db->lookaside.mxOut ){  //最大值
          db->lookaside.mxOut = db->lookaside.nOut;
        }
        return (void*)pBuf;
      }
    }
  }
#else
  if( db && db->mallocFailed ){
    return 0;
  }
#endif
  p = sqlite3Malloc(n);
  if( !p && db ){
    db->mallocFailed = 1;
  }
  sqlite3MemdebugSetType(p, MEMTYPE_DB |
         ((db && db->lookaside.bEnabled) ? MEMTYPE_LOOKASIDE : MEMTYPE_HEAP));//设置内存类型
  return p;
}

/*
** Resize the block of memory pointed to by p to n bytes. If the
** resize fails, set the mallocFailed flag in the connection object.
**
** 重新调整内存块，把p调整成n bytes，如果调整失败，在连接对象中设置失败标志
*/
void *sqlite3DbRealloc(sqlite3 *db, void *p, int n){
  void *pNew = 0;
  assert( db!=0 );
  assert( sqlite3_mutex_held(db->mutex) );
  if( db->mallocFailed==0 ){
    if( p==0 ){
      return sqlite3DbMallocRaw(db, n);
    }
    if( isLookaside(db, p) ){//后备内存
      if( n<=db->lookaside.sz ){//小于每一个块的大小
        return p;
      }
      pNew = sqlite3DbMallocRaw(db, n);//pNew指向新的地址空间
      if( pNew ){
        memcpy(pNew, p, db->lookaside.sz);//拷贝p的内存到pNew中
        sqlite3DbFree(db, p);//释放p
      }
    }else{
      assert( sqlite3MemdebugHasType(p, MEMTYPE_DB) );//调试的类型
      assert( sqlite3MemdebugHasType(p, MEMTYPE_LOOKASIDE|MEMTYPE_HEAP) );
      sqlite3MemdebugSetType(p, MEMTYPE_HEAP);
      pNew = sqlite3_realloc(p, n);//重新分配
      if( !pNew ){//分配失败
        sqlite3MemdebugSetType(p, MEMTYPE_DB|MEMTYPE_HEAP);
        db->mallocFailed = 1;
      }
      sqlite3MemdebugSetType(pNew, MEMTYPE_DB | 
            (db->lookaside.bEnabled ? MEMTYPE_LOOKASIDE : MEMTYPE_HEAP));
    }
  }
  return pNew;
}

/*
** Attempt to reallocate p.  If the reallocation fails, then free p
** and set the mallocFailed flag in the database connection.
**
** 试着重新分配p，如果分配失败，就释放，并且在数据库连接中设置失败标志
*/
void *sqlite3DbReallocOrFree(sqlite3 *db, void *p, int n){
  void *pNew;
  pNew = sqlite3DbRealloc(db, p, n);
  if( !pNew ){
    sqlite3DbFree(db, p);//分配失败就释放
  }
  return pNew;
}

/*
** Make a copy of a string in memory obtained from sqliteMalloc(). These 
** functions call sqlite3MallocRaw() directly instead of sqliteMalloc(). This
** is because when memory debugging is turned on, these two functions are 
** called via macros that record the current file and line number in the
** ThreadData structure.
** 
** 从利用sqliteMalloc函数获取的内存中拷贝一个字符串，这些函数直接调用sqlite3MallocRaw，
** 这是因为内存调试开关打开了，两个函数调用了宏，在线程数据结构中
** 这些宏记录了当前的文件和行号
*/
char *sqlite3DbStrDup(sqlite3 *db, const char *z){
  char *zNew;  //定义新的指针 指向新的字符串
  size_t n;
  if( z==0 ){
    return 0;
  }
  n = sqlite3Strlen30(z) + 1;//z计算长度
  assert( (n&0x7fffffff)==n );
  zNew = sqlite3DbMallocRaw(db, (int)n);//新分配空间
  if( zNew ){
    memcpy(zNew, z, n);
  }
  return zNew;
}
char *sqlite3DbStrNDup(sqlite3 *db, const char *z, int n){
  char *zNew;
  if( z==0 ){
    return 0;
  }
  assert( (n&0x7fffffff)==n );
  zNew = sqlite3DbMallocRaw(db, n+1);
  if( zNew ){
    memcpy(zNew, z, n);
    zNew[n] = 0;//结束符号
  }
  return zNew;
}

/*
** Create a string from the zFromat argument and the va_list that follows.
** Store the string in memory obtained from sqliteMalloc() and make *pz
** point to that string.
**
** 设置字符串*pz 指向zFromat这个字符串
*/
void sqlite3SetString(char **pz, sqlite3 *db, const char *zFormat, ...){
  va_list ap;
  char *z;

  va_start(ap, zFormat);//开始字符串
  z = sqlite3VMPrintf(db, zFormat, ap);//分配
  va_end(ap);//结束
  sqlite3DbFree(db, *pz);//释放pz
  *pz = z;//*pz指向z
}


/*
** This function must be called before exiting any API function (i.e. 
** returning control to the user) that has called sqlite3_malloc or
** sqlite3_realloc.
**
** The returned value is normally a copy of the second argument to this
** function. However, if a malloc() failure has occurred since the previous
** invocation SQLITE_NOMEM is returned instead. 
**
** If the first argument, db, is not NULL and a malloc() error has occurred,
** then the connection error-code (the value returned by sqlite3_errcode())
** is set to SQLITE_NOMEM.
**
** 在退出任何API函数（调用sqlite3_malloc或者sqlite3_realloc）必须被调用
** 返回类型是第二个参数的复制 如果malloc()失败 系统返回SQLITE_NOMEM
** 如果db 不空 并且分配错误 然后连接错误码被设置为SQLITE_NOMEM
*/
int sqlite3ApiExit(sqlite3* db, int rc){
  /* If the db handle is not NULL, then we must hold the connection handle
  ** mutex here. Otherwise the read (and possible write) of db->mallocFailed 
  ** is unsafe, as is the call to sqlite3Error().
  ** 
  ** 如果数据库句柄不为空，则我们必须把这个连接加锁。
  ** 否则，读取db->mallocFailed是不安全的，可能会调用sqlite3Error()。
  */
  assert( !db || sqlite3_mutex_held(db->mutex) );
  if( db && (db->mallocFailed || rc==SQLITE_IOERR_NOMEM) ){
    sqlite3Error(db, SQLITE_NOMEM, 0);//返回错误码
    db->mallocFailed = 0;
    rc = SQLITE_NOMEM;
  }
  return rc & (db ? db->errMask : 0xff);
}
