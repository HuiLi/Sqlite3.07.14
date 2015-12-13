/*
** 2007 August 14
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
** This file contains low-level memory allocation drivers for when
** SQLite will use the standard C-library malloc/realloc/free interface
** to obtain the memory it needs.
**
** 这个文件包含当SQLite将使用标准C库malloc/realloc/自由接口获得所需要的内存时的底层内存分配驱动程序。
**
** This file contains implementations of the low-level memory allocation
** routines specified in the sqlite3_mem_methods object.  The content of
** this file is only used if SQLITE_SYSTEM_MALLOC is defined.  The
** SQLITE_SYSTEM_MALLOC macro is defined automatically if neither the
** SQLITE_MEMDEBUG nor the SQLITE_WIN32_MALLOC macros are defined.  The
** default configuration is to use memory allocation routines in this
** file.
**
** 这个文件包含实现的底层sqlite3_mem_methods对象中指定的内存分配例程。
** 这个文件的内容仅是如果SQLITE_SYSTEM_MALLOC被定义使用。
** 当SQLITE_MEMDEBUG和SQLITE_WIN32_MALLOC宏命令都没有被定义时，就会自动定义为 SQLITE_SYSTEM_MALLOC宏命令。
** 缺省配置是使用这个文件中的例程
** 
** C-preprocessor macro summary:
**
**    HAVE_MALLOC_USABLE_SIZE     The configure script sets this symbol if
**                                the malloc_usable_size() interface exists
**                                on the target platform.  Or, this symbol
**                                can be set manually, if desired.
**                                If an equivalent interface exists by
**                                a different name, using a separate -D
**                                option to rename it.
** 
**
**                                如果malloc_usable_size()接口的存在
**                                在目标平台上，配置脚本设置这个符号。
**                                或者需要的话，这个符号可以手动配置。
**                                如果一个等效界面存在一个不同的名称,
**                                使用一个单独的- d选项来重命名它。
**
**    SQLITE_WITHOUT_ZONEMALLOC   Some older macs lack support for the zone
**                                memory allocator.  Set this symbol to enable
**                                building on older macs.
**
**                                一些旧的mac缺乏支持空间内存分配器。
                                  设置此标志,使其可在在旧的mac构建。
**
**    SQLITE_WITHOUT_MSIZE        Set this symbol to disable the use of
**                                _msize() on windows systems.  This might
**                                be necessary when compiling for Delphi,
**                                for example.
**
**                                这个符号设置为在windows系统上禁用msize()。
**                                例如，当要为Delphi编译时可能有必要的。
**
*/
#include "sqliteInt.h"

/*
** This version of the memory allocator is the default.  It is
** used when no other memory allocator is specified using compile-time
** macros.
**
** 这是默认的内存分配器。使用时编译时间指定宏没有使用其他内存分配器使用。
*/
#ifdef SQLITE_SYSTEM_MALLOC

/*
** The MSVCRT has malloc_usable_size() but it is called _msize().
** The use of _msize() is automatic, but can be disabled by compiling
** with -DSQLITE_WITHOUT_MSIZE
**
** MSVCRT 有malloc_usable_size()，但被称为_msize().
** _msize()是自动使用的, 但是可以通过编译是-DSQLITE_WITHOUT_MSIZE选项禁用。
*/
#if defined(_MSC_VER) && !defined(SQLITE_WITHOUT_MSIZE)
# define SQLITE_MALLOCSIZE _msize
#endif

#if defined(__APPLE__) && !defined(SQLITE_WITHOUT_ZONEMALLOC)

/*
** Use the zone allocator available on apple products unless the
** SQLITE_WITHOUT_ZONEMALLOC symbol is defined.
**
** 除非SQLITE_WITHOUT_ZONEMALLOC被定义，不然就用苹果产品上的区域内存分配器。
*/
#include <sys/sysctl.h>
#include <malloc/malloc.h>
#include <libkern/OSAtomic.h>
static malloc_zone_t* _sqliteZone_;
#define SQLITE_MALLOC(x) malloc_zone_malloc(_sqliteZone_, (x))
#define SQLITE_FREE(x) malloc_zone_free(_sqliteZone_, (x));
#define SQLITE_REALLOC(x,y) malloc_zone_realloc(_sqliteZone_, (x), (y))
#define SQLITE_MALLOCSIZE(x) \
        (_sqliteZone_ ? _sqliteZone_->size(_sqliteZone_,x) : malloc_size(x))

#else /* if not __APPLE__ */

/*
** Use standard C library malloc and free on non-Apple systems.  
** Also used by Apple systems if SQLITE_WITHOUT_ZONEMALLOC is defined.
**
** 非苹果系统上，使用标准C库中malloc和free函数。
** 如果SQLITE_WITHOUT_ZONEMALLOC被定义，还可以使用苹果系统。
*/
#define SQLITE_MALLOC(x)    malloc(x)
#define SQLITE_FREE(x)      free(x)
#define SQLITE_REALLOC(x,y) realloc((x),(y))

#if (defined(_MSC_VER) && !defined(SQLITE_WITHOUT_MSIZE)) \
      || (defined(HAVE_MALLOC_H) && defined(HAVE_MALLOC_USABLE_SIZE))
# include <malloc.h>    /* Needed for malloc_usable_size on linux */
#endif
#ifdef HAVE_MALLOC_USABLE_SIZE
# ifndef SQLITE_MALLOCSIZE
#  define SQLITE_MALLOCSIZE(x) malloc_usable_size(x)
# endif
#else
# undef SQLITE_MALLOCSIZE
#endif

#endif /* __APPLE__ or not __APPLE__ */

/*
** Like malloc(), but remember the size of the allocation
** so that we can find it later using sqlite3MemSize().
**
** For this low-level routine, we are guaranteed that nByte>0 because
** cases of nByte<=0 will be intercepted and dealt with by higher level
** routines.
**
** 像malloc(),但记忆分配大小，这样我们可以在之后使用sqlite3MemSize()来找到它.
** 对于这个低级的例程，我们必须保证nByte>0,因为如果nByte<=0，则将被打断和被更高级别的例程处理。
*/
//在内存的动态分配存储区中分配一块长度为nByte字节的连续区域，参数nByte为需要内存空间的长度，返回该区域的首地址。
static void *sqlite3MemMalloc(int nByte){
#ifdef SQLITE_MALLOCSIZE
  void *p = SQLITE_MALLOC( nByte );
  if( p==0 ){
    testcase( sqlite3GlobalConfig.xLog!=0 );
    sqlite3_log(SQLITE_NOMEM, "failed to allocate %u bytes of memory", nByte);
  }
  return p;
#else
  sqlite3_int64 *p;
  assert( nByte>0 );
  nByte = ROUND8(nByte);
  p = SQLITE_MALLOC( nByte+8 );
  if( p ){
    p[0] = nByte;
    p++;
  }else{
    testcase( sqlite3GlobalConfig.xLog!=0 );
    sqlite3_log(SQLITE_NOMEM, "failed to allocate %u bytes of memory", nByte);
  }
  return (void *)p;
#endif
}

/*
** Like free() but works for allocations obtained from sqlite3MemMalloc()
** or sqlite3MemRealloc().
**
** For this low-level routine, we already know that pPrior!=0 since
** cases where pPrior==0 will have been intecepted and dealt with
** by higher-level routines.
**
** 像free() 当在工作时被分配到sqlite3MemMalloc()或者sqlite3MemRealloc().
** 对于这个底层程序, 我们已知pPrior!=0，因为如果pPrior==0时，将被高层程序获取和处理。
*/
//释放pPrior指向的存储空间，被释放的空间通常被送入可用存储区域，以后可在调用malloc、realloc函数来再分配。
static void sqlite3MemFree(void *pPrior){
#ifdef SQLITE_MALLOCSIZE
  SQLITE_FREE(pPrior);
#else
  sqlite3_int64 *p = (sqlite3_int64*)pPrior;
  assert( pPrior!=0 );
  p--;
  SQLITE_FREE(p);
#endif
}

/*
** Report the allocated size of a prior return from xMalloc()
** or xRealloc().
**
** 报告来自于xMalloc()或xRealloc()之前的分配大小。
*/
//函数返回已分配内存的大小，也能够跟踪未归还内存的字节数，它能确定当一个分配被释放时有多少字节从未归还内存中移除。
static int sqlite3MemSize(void *pPrior){
#ifdef SQLITE_MALLOCSIZE
  return pPrior ? (int)SQLITE_MALLOCSIZE(pPrior) : 0;
#else
  sqlite3_int64 *p;
  if( pPrior==0 ) return 0;
  p = (sqlite3_int64*)pPrior;
  p--;
  return (int)p[0];
#endif
}

/*
** Like realloc().  Resize an allocation previously obtained from
** sqlite3MemMalloc().
**
** For this low-level interface, we know that pPrior!=0.  Cases where
** pPrior==0 while have been intercepted by higher-level routine and
** redirected to xMalloc.  Similarly, we know that nByte>0 becauses
** cases where nByte<=0 will have been intercepted by higher-level
** routines and redirected to xFree.
**
** 像 realloc()，调整分配之前获取sqlite3MemMalloc()。
** 对于底层接口，我们已知pPrior!=0，因为如果pPrior==0已拦截了高层的例程和重定向到xMalloc. 
** 同样，我们知道nByte>0，因为nByte<=0的情况会被高层的例程拦截和重定向到xFree。
*/
//给一个已经分配了地址的指针重新分配空间，参数pPrior为原有空间地址，nByte是重新申请的地址长度。
static void *sqlite3MemRealloc(void *pPrior, int nByte){
#ifdef SQLITE_MALLOCSIZE
  void *p = SQLITE_REALLOC(pPrior, nByte);
  if( p==0 ){
    testcase( sqlite3GlobalConfig.xLog!=0 );
    sqlite3_log(SQLITE_NOMEM,
      "failed memory resize %u to %u bytes",
      SQLITE_MALLOCSIZE(pPrior), nByte);
  }
  return p;
#else
  sqlite3_int64 *p = (sqlite3_int64*)pPrior;
  assert( pPrior!=0 && nByte>0 );
  assert( nByte==ROUND8(nByte) ); /* EV: R-46199-30249 */
  p--;
  p = SQLITE_REALLOC(p, nByte+8 );
  if( p ){
    p[0] = nByte;
    p++;
  }else{
    testcase( sqlite3GlobalConfig.xLog!=0 );
    sqlite3_log(SQLITE_NOMEM,
      "failed memory resize %u to %u bytes",
      sqlite3MemSize(pPrior), nByte);
  }
  return (void*)p;
#endif
}

/*
** Round up a request size to the next valid allocation size.
**
** 集合一个请求大小到下一个有效分配的大小。
*/
//函数返回下一个有效内存分配的大小。
static int sqlite3MemRoundup(int n){
  return ROUND8(n);
}

/*
** Initialize this module.
**
** 初始化这个模块。
*/
//函数初始化
static int sqlite3MemInit(void *NotUsed){
#if defined(__APPLE__) && !defined(SQLITE_WITHOUT_ZONEMALLOC)
  int cpuCount;
  size_t len;
  if( _sqliteZone_ ){
    return SQLITE_OK;
  }
  len = sizeof(cpuCount);
  /* One usually wants to use hw.acctivecpu for MT decisions, but not here */
  /*通常希望使用hw.acctivecpu为MT的决定，但是不是在这里*/
  sysctlbyname("hw.ncpu", &cpuCount, &len, NULL, 0);
  if( cpuCount>1 ){
    /* defer MT decisions to system malloc */
    /*推迟MT决策系统的malloc*/
    _sqliteZone_ = malloc_default_zone();
  }else{
    /* only 1 core, use our own zone to contention over global locks, 
    ** e.g. we have our own dedicated locks */
    /*仅有一个核心，使用我们的区域在争用全局锁，例如，我们只有自己的专用锁*/
    bool success;
    malloc_zone_t* newzone = malloc_create_zone(4096, 0);
    malloc_set_zone_name(newzone, "Sqlite_Heap");
    do{
      success = OSAtomicCompareAndSwapPtrBarrier(NULL, newzone, 
                                 (void * volatile *)&_sqliteZone_);
    }while(!_sqliteZone_);
    if( !success ){
      /* somebody registered a zone first */
      /*有人已先注册区域*/
      malloc_destroy_zone(newzone);
    }
  }
#endif
  UNUSED_PARAMETER(NotUsed);
  return SQLITE_OK;
}

/*
** Deinitialize this module.
**
** 取消初始化这个模块
*/
//取消函数的初始化
static void sqlite3MemShutdown(void *NotUsed){
  UNUSED_PARAMETER(NotUsed);
  return;
}

/*
** This routine is the only routine in this file with external linkage.
**
** Populate the low-level memory allocation function pointers in
** sqlite3GlobalConfig.m with pointers to the routines in this file.
** 
** 该例程是外部链接这个文件的唯一的例程。
** 在sqlite3GlobalConfig.m与这个文件指针的例程中填充底层内存分配函数指针。
*/
//函数填充底层的内存分配函数指针
void sqlite3MemSetDefault(void){
  static const sqlite3_mem_methods defaultMethods = {
     sqlite3MemMalloc,
     sqlite3MemFree,
     sqlite3MemRealloc,
     sqlite3MemSize,
     sqlite3MemRoundup,
     sqlite3MemInit,
     sqlite3MemShutdown,
     0
  };
  sqlite3_config(SQLITE_CONFIG_MALLOC, &defaultMethods);
}

#endif /* SQLITE_SYSTEM_MALLOC */
