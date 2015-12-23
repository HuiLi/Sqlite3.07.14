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
**当SQLite用标准的c库malloc/realloc/free自由接口来获得需要的内存时，这个文件是为了实现底层内存分配的驱动器。


** This file contains implementations of the low-level memory allocation
** routines specified in the sqlite3_mem_methods object.  The content of
** this file is only used if SQLITE_SYSTEM_MALLOC is defined.  The
** SQLITE_SYSTEM_MALLOC macro is defined automatically if neither the
** SQLITE_MEMDEBUG nor the SQLITE_WIN32_MALLOC macros are defined.  The
** default configuration is to use memory allocation routines in this
** file.
**对底层内存分配程序具体的实现是在sqlite3_mem_methods这一对象中，。
同时这个文件的内容仅用于宏QLITE_SYSTEM_MALLOC被定义的时候。而且，
是当 SQLITE_MEMDEBUG和宏SQLITE_WIN32_MALLOC都没有被定义时，宏SQLITE_SYSTEM_MALLOC才被自动定义。
这个默认设置是为了在这个文件中运用内存分配程序。
** C-preprocessor macro summary:
**
**    HAVE_MALLOC_USABLE_SIZE     The configure script sets this symbol if
**                                the malloc_usable_size() interface exists
**                                on the target platform.  Or, this symbol
**                                can be set manually, if desired.
**                                If an equivalent interface exists by
**                                a different name, using a separate -D
**                                option to rename it.
**c编译宏概述：
**    HAVE_MALLOC_USABLE_SIZE      如果malloc_usable_size()函数接口在目标程序中存在，默认脚本设置这个标志。
**                                 或者，这个标志根据需要能够被人为地设置。如果一个相同的函数接口以一个不同的名字存在，
**                                 那么用一个分隔的选项去重命名它。
**                          
**    SQLITE_WITHOUT_ZONEMALLOC   Some older macs lack support for the zone
**                                memory allocator.  Set this symbol to enable
**                                building on older macs.
**
**    SQLITE_WITHOUT_ZONEMALLOC  一些旧的内存存储器不支持区域内存分配程序。所以在旧的内存存储器上设置一个标志，让旧的内存存储器能够支持区域内存分配。     													*
      SQLITE_WITHOUT_MSIZE        Set this symbol to disable the use of
**                                _msize() on windows systems.  This might
**                                be necessary when compiling for Delphi,
**                                for example.
**
**    SQLITE_WITHOUT_MSIZE         设置这个标记是为了在win系统中避免对_msize()的运用。例如，在Delphi中编译时可能是必须的。 ?**
*/
#include "sqliteInt.h"

/*
** This version of the memory allocator is the default.  It is
** used when no other memory allocator is specified using compile-time
** macros.
*/
/*这个是这个版本的默认的内存分配程序。仅用于内存分配程序正在被具体的用于宏编辑的时候。
*/
#ifdef SQLITE_SYSTEM_MALLOC

/*
** The MSVCRT has malloc_usable_size() but it is called _msize().
** The use of _msize() is automatic, but can be disabled by compiling
** with -DSQLITE_WITHOUT_MSIZE
*/
/*
MSVCRT（选择编译器） 中有alloc_usable_size()函数，但它被叫做-msize().
且对_msize() 的运用是自动的，但是通过用 -DSQLITE_WITHOUT_MSIZE（应该也是一个宏）通过编译是不能运行的。?*/
#if defined(_MSC_VER) && !defined(SQLITE_WITHOUT_MSIZE)
# define SQLITE_MALLOCSIZE _msize
#endif

#if defined(__APPLE__) && !defined(SQLITE_WITHOUT_ZONEMALLOC)

/*
** Use the zone allocator available on apple products unless the
** SQLITE_WITHOUT_ZONEMALLOC symbol is defined.
*/
/*
如果宏SQLITE_WITHOUT_ZONEMALLOC没有被定义，苹果产品中用区域分配程序。?*/
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
*/
/*
对非苹果系统用标准的c 库函数分配内存和释放内存。也可以用于宏SQLITE_WITHOUT_ZONEMALLOC malloc被
定义时，apple 系统也可以运用。?*/
#define SQLITE_MALLOC(x)    malloc(x)
#define SQLITE_FREE(x)      free(x)
#define SQLITE_REALLOC(x,y) realloc((x),(y))

#if (defined(_MSC_VER) && !defined(SQLITE_WITHOUT_MSIZE)) \
      || (defined(HAVE_MALLOC_H) && defined(HAVE_MALLOC_USABLE_SIZE))
# include <malloc.h>    /* Needed for malloc_usable_size on linux */ /*在linux系统中，被alloc_usable_size所需要的*/
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
*/
/*
像malloc(),但记得内存大小这样可以之后运用sqlite3MemSize()找到它。
对于这个底层的程序，由于当nByte<=0时解释和处理都由高层程序进行，所以我们必须确保nByte>0。
*/
static void *sqlite3MemMalloc(int nByte){/*这里是内存分配的实现程序，前面是进行执行前的判断和准备*/
#ifdef SQLITE_MALLOCSIZE
  void *p = SQLITE_MALLOC( nByte );
  if( p==0 ){
    testcase( sqlite3GlobalConfig.xLog!=0 );/*测试*/
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
*/
/*
类似free() 函数的功能，但是内存释放的功能只能用于是从qlite3MemMalloc()或者qlite3MemRealloc()获得的内存分配.

关于这个底层程序，由于 pPrior==0时内存分配是由高层内存分配程序处理和解释的，所以Prior!=0。
*/
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
*/
/*
提示从xMalloc()或者xRealloc()中返回的prior的被分配的大小。
*/
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
*/
/*
类似realloc()功能，对从之前在qlite3MemMalloc()获得的内存的大小进行重新设定。
关于底层接口函数，一般可以认为pPrior!=0。因为pPrior==0被高层内存分配程序解释和被xMalloc重定向. 
所以简单滴认为Byte>0正是Byte<=0时内存分配由高层程序处理和被xFree函数重新定向。
*/
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
*/
/*
搜集下一个需求的内存大小，满足它。*/
static int sqlite3MemRoundup(int n){
  return ROUND8(n);
}

/*
** Initialize this module.
**初始化组件
*/
static int sqlite3MemInit(void *NotUsed){
#if defined(__APPLE__) && !defined(SQLITE_WITHOUT_ZONEMALLOC)
  int cpuCount;
  size_t len;
  if( _sqliteZone_ ){
    return SQLITE_OK;
  }
  len = sizeof(cpuCount);
  /* One usually wants to use hw.acctivecpu for MT decisions, but not here */
  /*一般情况下，用hw.acctivecpu做内存分配决策，但在这里不是这种策略*/
  sysctlbyname("hw.ncpu", &cpuCount, &len, NULL, 0);/*sysctlbyname是一个查找CPU信息的函数*/
  if( cpuCount>1 ){
    /* defer MT decisions to system malloc */
    /*对系统分配推迟内存分配决策*/
    _sqliteZone_ = malloc_default_zone();
  }else{
    /* only 1 core, use our own zone to contention over global locks, 
    ** e.g. we have our own dedicated locks */
    /*仅有一个内核，用我们自己的区域去竞争全局锁。例如，有我们自己的专用锁。*/
    bool success;
    malloc_zone_t* newzone = malloc_create_zone(4096, 0);
    malloc_set_zone_name(newzone, "Sqlite_Heap");
    do{
      success = OSAtomicCompareAndSwapPtrBarrier(NULL, newzone, 
                                 (void * volatile *)&_sqliteZone_);
    }while(!_sqliteZone_);
    if( !success ){
      /* somebody registered a zone first */
      /*有人已经注册了一个区域*/
      malloc_destroy_zone(newzone);
    }
  }
#endif
  UNUSED_PARAMETER(NotUsed);
  return SQLITE_OK;
}

/*
** Deinitialize this module.
**释放初始化的那个组件
*/
static void sqlite3MemShutdown(void *NotUsed){
  UNUSED_PARAMETER(NotUsed);
  return;
}

/*
** This routine is the only routine in this file with external linkage.
**
** Populate the low-level memory allocation function pointers in
** sqlite3GlobalConfig.m with pointers to the routines in this file.
*/
/*
这个程序在这个文件中仅是一个用于外部链接的程序。
用指向这个文件中的指针，填充底层内存分配函数指针在sqlite3GlobalConfig.m中。
*/
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
