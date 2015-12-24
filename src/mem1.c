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
**µ±SQLiteÓÃ±ê×¼µÄc¿âmalloc/realloc/free×ÔÓÉ½Ó¿ÚÀ´»ñµÃĞèÒªµÄÄÚ´æÊ±£¬Õâ¸öÎÄ¼şÊÇÎªÁËÊµÏÖµ×²ãÄÚ´æ·ÖÅäµÄÇı¶¯Æ÷¡£


** This file contains implementations of the low-level memory allocation
** routines specified in the sqlite3_mem_methods object.  The content of
** this file is only used if SQLITE_SYSTEM_MALLOC is defined.  The
** SQLITE_SYSTEM_MALLOC macro is defined automatically if neither the
** SQLITE_MEMDEBUG nor the SQLITE_WIN32_MALLOC macros are defined.  The
** default configuration is to use memory allocation routines in this
** file.
**¶Ôµ×²ãÄÚ´æ·ÖÅä³ÌĞò¾ßÌåµÄÊµÏÖÊÇÔÚsqlite3_mem_methodsÕâÒ»¶ÔÏóÖĞ£¬¡£
Í¬Ê±Õâ¸öÎÄ¼şµÄÄÚÈİ½öÓÃÓÚºêQLITE_SYSTEM_MALLOC±»¶¨ÒåµÄÊ±ºò¡£¶øÇÒ£¬
ÊÇµ± SQLITE_MEMDEBUGºÍºêSQLITE_WIN32_MALLOC¶¼Ã»ÓĞ±»¶¨ÒåÊ±£¬ºêSQLITE_SYSTEM_MALLOC²Å±»×Ô¶¯¶¨Òå¡£
Õâ¸öÄ¬ÈÏÉèÖÃÊÇÎªÁËÔÚÕâ¸öÎÄ¼şÖĞÔËÓÃÄÚ´æ·ÖÅä³ÌĞò¡£
** C-preprocessor macro summary:
**
**    HAVE_MALLOC_USABLE_SIZE     The configure script sets this symbol if
**                                the malloc_usable_size() interface exists
**                                on the target platform.  Or, this symbol
**                                can be set manually, if desired.
**                                If an equivalent interface exists by
**                                a different name, using a separate -D
**                                option to rename it.
**c±àÒëºê¸ÅÊö£º
**    HAVE_MALLOC_USABLE_SIZE      Èç¹ûmalloc_usable_size()º¯Êı½Ó¿ÚÔÚÄ¿±ê³ÌĞòÖĞ´æÔÚ£¬Ä¬ÈÏ½Å±¾ÉèÖÃÕâ¸ö±êÖ¾¡£
**                                 »òÕß£¬Õâ¸ö±êÖ¾¸ù¾İĞèÒªÄÜ¹»±»ÈËÎªµØÉèÖÃ¡£Èç¹ûÒ»¸öÏàÍ¬µÄº¯Êı½Ó¿ÚÒÔÒ»¸ö²»Í¬µÄÃû×Ö´æÔÚ£¬
**                                 ÄÇÃ´ÓÃÒ»¸ö·Ö¸ôµÄÑ¡ÏîÈ¥ÖØÃüÃûËü¡£
**                          
**    SQLITE_WITHOUT_ZONEMALLOC   Some older macs lack support for the zone
**                                memory allocator.  Set this symbol to enable
**                                building on older macs.
**
**    SQLITE_WITHOUT_ZONEMALLOC  Ò»Ğ©¾ÉµÄÄÚ´æ´æ´¢Æ÷²»Ö§³ÖÇøÓòÄÚ´æ·ÖÅä³ÌĞò¡£ËùÒÔÔÚ¾ÉµÄÄÚ´æ´æ´¢Æ÷ÉÏÉèÖÃÒ»¸ö±êÖ¾£¬ÈÃ¾ÉµÄÄÚ´æ´æ´¢Æ÷ÄÜ¹»Ö§³ÖÇøÓòÄÚ´æ·ÖÅä¡£     													*
      SQLITE_WITHOUT_MSIZE        Set this symbol to disable the use of
**                                _msize() on windows systems.  This might
**                                be necessary when compiling for Delphi,
**                                for example.
**
**    SQLITE_WITHOUT_MSIZE         ÉèÖÃÕâ¸ö±ê¼ÇÊÇÎªÁËÔÚwinÏµÍ³ÖĞ±ÜÃâ¶Ô_msize()µÄÔËÓÃ¡£ÀıÈç£¬ÔÚDelphiÖĞ±àÒëÊ±¿ÉÄÜÊÇ±ØĞëµÄ¡£ ‚
**
*/
#include "sqliteInt.h"

/*
** This version of the memory allocator is the default.  It is
** used when no other memory allocator is specified using compile-time
** macros.
*/
/*Õâ¸öÊÇÕâ¸ö°æ±¾µÄÄ¬ÈÏµÄÄÚ´æ·ÖÅä³ÌĞò¡£½öÓÃÓÚÄÚ´æ·ÖÅä³ÌĞòÕıÔÚ±»¾ßÌåµÄÓÃÓÚºê±à¼­µÄÊ±ºò¡£
*/
#ifdef SQLITE_SYSTEM_MALLOC

/*
** The MSVCRT has malloc_usable_size() but it is called _msize().
** The use of _msize() is automatic, but can be disabled by compiling
** with -DSQLITE_WITHOUT_MSIZE
*/
/*
MSVCRT£¨Ñ¡Ôñ±àÒëÆ÷£© ÖĞÓĞalloc_usable_size()º¯Êı£¬µ«Ëü±»½Ğ×ö-msize().
ÇÒ¶Ô_msize() µÄÔËÓÃÊÇ×Ô¶¯µÄ£¬µ«ÊÇÍ¨¹ıÓÃ -DSQLITE_WITHOUT_MSIZE£¨Ó¦¸ÃÒ²ÊÇÒ»¸öºê£©Í¨¹ı±àÒëÊÇ²»ÄÜÔËĞĞµÄ¡£¯ä‚
*/
#if defined(_MSC_VER) && !defined(SQLITE_WITHOUT_MSIZE)
# define SQLITE_MALLOCSIZE _msize
#endif

#if defined(__APPLE__) && !defined(SQLITE_WITHOUT_ZONEMALLOC)

/*
** Use the zone allocator available on apple products unless the
** SQLITE_WITHOUT_ZONEMALLOC symbol is defined.
*/
/*
Èç¹ûºêSQLITE_WITHOUT_ZONEMALLOCÃ»ÓĞ±»¶¨Òå£¬Æ»¹û²úÆ·ÖĞÓÃÇøÓò·ÖÅä³ÌĞò¡£‚
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
*/
/*
¶Ô·ÇÆ»¹ûÏµÍ³ÓÃ±ê×¼µÄc ¿âº¯Êı·ÖÅäÄÚ´æºÍÊÍ·ÅÄÚ´æ¡£Ò²¿ÉÒÔÓÃÓÚºêSQLITE_WITHOUT_ZONEMALLOC malloc±»
¶¨ÒåÊ±£¬apple ÏµÍ³Ò²¿ÉÒÔÔËÓÃ¡£‚
*/
#define SQLITE_MALLOC(x)    malloc(x)
#define SQLITE_FREE(x)      free(x)
#define SQLITE_REALLOC(x,y) realloc((x),(y))

#if (defined(_MSC_VER) && !defined(SQLITE_WITHOUT_MSIZE)) \
      || (defined(HAVE_MALLOC_H) && defined(HAVE_MALLOC_USABLE_SIZE))
# include <malloc.h>    /* Needed for malloc_usable_size on linux */ /*ÔÚlinuxÏµÍ³ÖĞ£¬±»alloc_usable_sizeËùĞèÒªµÄ*/
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
Ïñmalloc(),µ«¼ÇµÃÄÚ´æ´óĞ¡ÕâÑù¿ÉÒÔÖ®ºóÔËÓÃsqlite3MemSize()ÕÒµ½Ëü¡£
¶ÔÓÚÕâ¸öµ×²ãµÄ³ÌĞò£¬ÓÉÓÚµ±nByte<=0Ê±½âÊÍºÍ´¦Àí¶¼ÓÉ¸ß²ã³ÌĞò½øĞĞ£¬ËùÒÔÎÒÃÇ±ØĞëÈ·±£nByte>0¡£
*/
static void *sqlite3MemMalloc(int nByte){/*ÕâÀïÊÇÄÚ´æ·ÖÅäµÄÊµÏÖ³ÌĞò£¬Ç°ÃæÊÇ½øĞĞÖ´ĞĞÇ°µÄÅĞ¶ÏºÍ×¼±¸*/
#ifdef SQLITE_MALLOCSIZE
  void *p = SQLITE_MALLOC( nByte );
  if( p==0 ){
    testcase( sqlite3GlobalConfig.xLog!=0 );/*²âÊÔ*/
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
ÀàËÆfree() º¯ÊıµÄ¹¦ÄÜ£¬µ«ÊÇÄÚ´æÊÍ·ÅµÄ¹¦ÄÜÖ»ÄÜÓÃÓÚÊÇ´Óqlite3MemMalloc()»òÕßqlite3MemRealloc()»ñµÃµÄÄÚ´æ·ÖÅä.

¹ØÓÚÕâ¸öµ×²ã³ÌĞò£¬ÓÉÓÚ pPrior==0Ê±ÄÚ´æ·ÖÅäÊÇÓÉ¸ß²ãÄÚ´æ·ÖÅä³ÌĞò´¦ÀíºÍ½âÊÍµÄ£¬ËùÒÔPrior!=0¡£
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
ÌáÊ¾´ÓxMalloc()»òÕßxRealloc()ÖĞ·µ»ØµÄpriorµÄ±»·ÖÅäµÄ´óĞ¡¡£
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
ÀàËÆrealloc()¹¦ÄÜ£¬¶Ô´ÓÖ®Ç°ÔÚqlite3MemMalloc()»ñµÃµÄÄÚ´æµÄ´óĞ¡½øĞĞÖØĞÂÉè¶¨¡£
¹ØÓÚµ×²ã½Ó¿Úº¯Êı£¬Ò»°ã¿ÉÒÔÈÏÎªpPrior!=0¡£ÒòÎªpPrior==0±»¸ß²ãÄÚ´æ·ÖÅä³ÌĞò½âÊÍºÍ±»xMallocÖØ¶¨Ïò. 
ËùÒÔ¼òµ¥µÎÈÏÎªByte>0ÕıÊÇByte<=0Ê±ÄÚ´æ·ÖÅäÓÉ¸ß²ã³ÌĞò´¦ÀíºÍ±»xFreeº¯ÊıÖØĞÂ¶¨Ïò¡£
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
ËÑ¼¯ÏÂÒ»¸öĞèÇóµÄÄÚ´æ´óĞ¡£¬Âú×ãËü¡£*/
static int sqlite3MemRoundup(int n){
  return ROUND8(n);
}

/*
** Initialize this module.
**³õÊ¼»¯×é¼ş
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
  /*Ò»°ãÇé¿öÏÂ£¬ÓÃhw.acctivecpu×öÄÚ´æ·ÖÅä¾ö²ß£¬µ«ÔÚÕâÀï²»ÊÇÕâÖÖ²ßÂÔ*/
  sysctlbyname("hw.ncpu", &cpuCount, &len, NULL, 0);/*sysctlbynameÊÇÒ»¸ö²éÕÒCPUĞÅÏ¢µÄº¯Êı*/
  if( cpuCount>1 ){
    /* defer MT decisions to system malloc */
    /*¶ÔÏµÍ³·ÖÅäÍÆ³ÙÄÚ´æ·ÖÅä¾ö²ß*/
    _sqliteZone_ = malloc_default_zone();
  }else{
    /* only 1 core, use our own zone to contention over global locks, 
    ** e.g. we have our own dedicated locks */
    /*½öÓĞÒ»¸öÄÚºË£¬ÓÃÎÒÃÇ×Ô¼ºµÄÇøÓòÈ¥¾ºÕùÈ«¾ÖËø¡£ÀıÈç£¬ÓĞÎÒÃÇ×Ô¼ºµÄ×¨ÓÃËø¡£*/
    bool success;
    malloc_zone_t* newzone = malloc_create_zone(4096, 0);
    malloc_set_zone_name(newzone, "Sqlite_Heap");
    do{
      success = OSAtomicCompareAndSwapPtrBarrier(NULL, newzone, 
                                 (void * volatile *)&_sqliteZone_);
    }while(!_sqliteZone_);
    if( !success ){
      /* somebody registered a zone first */
      /*ÓĞÈËÒÑ¾­×¢²áÁËÒ»¸öÇøÓò*/
      malloc_destroy_zone(newzone);
    }
  }
#endif
  UNUSED_PARAMETER(NotUsed);
  return SQLITE_OK;
}

/*
** Deinitialize this module.
**ÊÍ·Å³õÊ¼»¯µÄÄÇ¸ö×é¼ş
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
Õâ¸ö³ÌĞòÔÚÕâ¸öÎÄ¼şÖĞ½öÊÇÒ»¸öÓÃÓÚÍâ²¿Á´½ÓµÄ³ÌĞò¡£
ÓÃÖ¸ÏòÕâ¸öÎÄ¼şÖĞµÄÖ¸Õë£¬Ìî³äµ×²ãÄÚ´æ·ÖÅäº¯ÊıÖ¸ÕëÔÚsqlite3GlobalConfig.mÖĞ¡£
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
