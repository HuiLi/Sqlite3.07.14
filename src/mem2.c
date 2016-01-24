/*
** 2007 August 15
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
** to obtain the memory it needs while adding lots of additional debugging
** information to each allocation in order to help detect and fix memory
** leaks and memory usage errors.
**
** This file contains implementations of the low-level memory allocation
** routines specified in the sqlite3_mem_methods object.
*/
/*这个文件是底层的内存分配驱动程序，当SQLite运用标准的c-library malloc /realloc /free 接口去获得它需要的内存，而且还对每一个分配附加了额外
的排错信息，为了帮助保护和检查内存遗漏以及内存运用错误。
这个文件中的底层内存分配程序的具体实现是在sqlite3_mem_methods对象中。*/
#include "sqliteInt.h"

/*
** This version of the memory allocator is used only if the
** SQLITE_MEMDEBUG macro is defined
*/
/*这个版本的内存分配器仅用于宏SQLITE_MEMDEBUG被定义时*/
#ifdef SQLITE_MEMDEBUG

/*
** The backtrace functionality is only available with GLIBC
*/
/*回溯功能仅用于GLIBC运行库*/
#ifdef __GLIBC__
  extern int backtrace(void**,int);
  extern void backtrace_symbols_fd(void*const*,int,int);
#else
# define backtrace(A,B) 1
# define backtrace_symbols_fd(A,B,C)
#endif
#include <stdio.h>

/*
** Each memory allocation looks like this:
**
**  ------------------------------------------------------------------------
**  | Title |  backtrace pointers |  MemBlockHdr |  allocation |  EndGuard |
**  ------------------------------------------------------------------------
**
** The application code sees only a pointer to the allocation.  We have
** to back up from the allocation pointer to find the MemBlockHdr.  The
** MemBlockHdr tells us the size of the allocation and the number of
** backtrace pointers.  There is also a guard word at the end of the
** MemBlockHdr.
*/
/*
每一个内存分配的结构**
**  ------------------------------------------------------------------------
**  | 标题 |  返回路径指针 | 内存块HDR |  内存分配|  结束标志
**  ------------------------------------------------------------------------
**
** 应用程序代码只看到一个指向分配。我们必须再从分配指针
** 找到内存块内HDR。该HDR告诉我们分配的大小和回溯的指针数。
** 这也有保护语句在内存块内HDR的尾端。
**
** Title：用于描述这段内存，在出错时可以打印出来
** backtrace pointer：用于保留调用堆栈
** MemBlockHdr：负责这片内存的管理，以及串联未释放的MemBlock
** allocation：分配给上层的空间
** EndGuard：尾部的哨兵，用于检查内存被踩。还有个“HeadGaurd ”在MemBlockHdr中。
*/
//内存分配结构
struct MemBlockHdr {
  i64 iSize;                          /* Size of this allocation     内存分配的大小*/
  struct MemBlockHdr *pNext, *pPrev;  /* Linked list of all unfreed memory    指的是没有被释放的内存的链接表结构〃*/
  char nBacktrace;                    /* Number of backtraces on this alloc     在内存分配中的返回路径的编号*/
  char nBacktraceSlots;               /* Available backtrace slots     可利用的路径返回槽*/
  u8 nTitle;                          /* Bytes of title; includes '\0'    空格在内的标题的字节*/
  u8 eType;                           /* Allocation type code    内存分配类型编码*/
  int iForeGuard;                     /* Guard word for sanity     为了结构明确做的标记*/
};

/*
** Guard words  关键字
*/
#define FOREGUARD 0x80F5E153 /*前关键字*/
#define REARGUARD 0xE4676B53 /*后关键字*/
/*
** Number of malloc size increments to track.
*/
/*为了追踪内存分配大小的增量的数量*/
#define NCSIZE  1000

/*
** All of the static variables used by this module are collected
** into a single structure named "mem".  This is to keep the
** static variables organized and to reduce namespace pollution
** when this module is combined with other in the amalgamation.
*/
/*
所有的被这个组件使用的静态变量，被聚集在一个名字叫“mem”的结构的中。这样做是为了有组织的建立静态变量，
以及为了减少当这个组件与其他的结合时名字域污染*/
static struct {
  
  /*
  ** Mutex to control access to the memory allocation subsystem.
  */
  /*内存分配子系统的控制存取的互斥*/
  sqlite3_mutex *mutex;

  /*
  ** Head and tail of a linked list of all outstanding allocations
  */
 /*所有的未被分配的内存的链接表的头和尾*/
  struct MemBlockHdr *pFirst;
  struct MemBlockHdr *pLast;
  
  /*
  ** The number of levels of backtrace to save in new allocations.
  */
  /*在新的内存分配中，含有回溯路径等级（水平）的号码*/
  int nBacktrace;
  void (*xBacktrace)(int, int, void **);

  /*
  ** Title text to insert in front of each block
  */
  /*在每一个内存块前插入标题文本*/
  int nTitle;        /* Bytes of zTitle to save.  Includes '\0' and padding  用于存储数组zTitle包含'\ 0'和填充内容，存储大小*/
  char zTitle[100];  /* The title text 用数组存储标题文本 */

  /* 
  ** sqlite3MallocDisallow() increments the following counter.
  ** sqlite3MallocAllow() decrements it.
  */
  /*
  sqlite3MallocDisallow()是增加下面的计数器
  sqlite3MallocAllow()是减少下面的计数器  */
  int disallow; /* Do not allow memory allocation  不允许内存分配*/

  /*
  ** Gather statistics on the sizes of memory allocations.
  ** nAlloc[i] is the number of allocation attempts of i*8
  ** bytes.  i==NCSIZE is the number of allocation attempts for
  ** sizes more than NCSIZE*8 bytes.
  **
  ** 收集有关内存分配大小的统计数据。nAlloc[i]是分配尝试的次数为i*8个字节。
  ** i==NCSIZE是参数ncsize分配尝试的大小超过参数ncsize*8个字节。
  */
  int nAlloc[NCSIZE];      /* Total number of allocations 分配总数*/
  int nCurrent[NCSIZE];    /* Current number of allocations 当前分配数量*/
  int mxCurrent[NCSIZE];   /* Highwater mark for nCurrent 分配数量的最大值*/

} mem;


/*
** Adjust memory usage statistics
*/
/*调整内存使用的统计数据*/
static void adjustStats(int iSize, int increment){
  int i = ROUND8(iSize)/8;/*iSize是企图获得的内存的大小，说明是以8个字节的形式获取内存*/
  if( i>NCSIZE-1 ){
    i = NCSIZE - 1;/*超过最大值，设为最大值*/
  }
  if( increment>0 ){
    mem.nAlloc[i]++;
    mem.nCurrent[i]++;
    if( mem.nCurrent[i]>mem.mxCurrent[i] ){
      mem.mxCurrent[i] = mem.nCurrent[i];
    }
  }else{
    mem.nCurrent[i]--;
    assert( mem.nCurrent[i]>=0 );
  }
}

/*
** Given an allocation, find the MemBlockHdr for that allocation.
**
** This routine checks the guards at either end of the allocation and
** if they are incorrect it asserts.
*/
//得到锁分配内存的MemBlockHdr
static struct MemBlockHdr *sqlite3MemsysGetHeader(void *pAllocation){
  struct MemBlockHdr *p;
  int *pInt;
  u8 *pU8;
  int nReserve;

  p = (struct MemBlockHdr*)pAllocation;
  p--;
  assert( p->iForeGuard==(int)FOREGUARD );
  nReserve = ROUND8(p->iSize);
  pInt = (int*)pAllocation;
  pU8 = (u8*)pAllocation;
  assert( pInt[nReserve/sizeof(int)]==(int)REARGUARD );/*pInt[nReserve/sizeof(int)]是指针数组，指的是pInt[]指针最后一个是否是尾关键字*/
  /* This checks any of the "extra" bytes allocated due
  ** to rounding up to an 8 byte boundary to ensure 
  ** they haven't been overwritten.
  */
  /*对于达到8字节的界限的任何的“额外”的被分配的字节，检查它们是为了避免它们被重写*/
  while( nReserve-- > p->iSize ) assert( pU8[nReserve]==0x65 );
  return p;
}

/*
** Return the number of bytes currently allocated at address p.
*/
/*返回目前在地址p中分配的字节的数目*/
static int sqlite3MemSize(void *p){
  struct MemBlockHdr *pHdr;
  if( !p ){
    return 0;/*如果p==0，则返回0*/
  }
  pHdr = sqlite3MemsysGetHeader(p);/*大于0,则用函数sqlite3MemsysGetHeader()获取p的大小*/
  return pHdr->iSize;
}

/*
** Initialize the memory allocation subsystem.
*/
/*初始化内存分配子系统*/
static int sqlite3MemInit(void *NotUsed){
  UNUSED_PARAMETER(NotUsed);
  assert( (sizeof(struct MemBlockHdr)&7) == 0 );
  if( !sqlite3GlobalConfig.bMemstat ){
    /* If memory status is enabled, then the malloc.c wrapper will already
    ** hold the STATIC_MEM mutex when the routines here are invoked. */
    /*如果内存是不可以用的状态，那么STATIC_MEM mutex 程序在这里被调用，malloc.c封装器将保持内存静态互斥状态*/
    mem.mutex = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MEM);
  }
  return SQLITE_OK;/*指的是一切准备工作已完成*/
}

/*
** Deinitialize the memory allocation subsystem.
*/
/*取消初始化内存分配系统*/
static void sqlite3MemShutdown(void *NotUsed){
  UNUSED_PARAMETER(NotUsed);
  mem.mutex = 0;
}

/*
** Round up a request size to the next valid allocation size.
**
** 向上舍入请求大小到一个有效分配的大小
*/
/*收集满足下一个可用大小的内存分配*/
static int sqlite3MemRoundup(int n){
  return ROUND8(n);
}

/*
** Fill a buffer with pseudo-random bytes.  This is used to preset
** the content of a new memory allocation to unpredictable values and
** to clear the content of a freed allocation to unpredictable values.
*/
/*用伪随机字节填充一个缓冲区。这样做是为了对一个不能估计其值的新分配的内存的内容进行预设置，
以及对无法估计值的已经释放的内存的内容进行清除。
*/
static void randomFill(char *pBuf, int nByte){
  unsigned int x, y, r;
  x = SQLITE_PTR_TO_INT(pBuf);
  y = nByte | 1;
  while( nByte >= 4 ){
    x = (x>>1) ^ (-(x&1) & 0xd0000001);
    y = y*1103515245 + 12345;
    r = x ^ y;
    *(int*)pBuf = r;
    pBuf += 4;
    nByte -= 4;
  }
  while( nByte-- > 0 ){
    x = (x>>1) ^ (-(x&1) & 0xd0000001);
    y = y*1103515245 + 12345;
    r = x ^ y;
    *(pBuf++) = r & 0xff;
  }
}

/*
** Allocate nByte bytes of memory.
*/
/*分配nByte个字节作为内存的大小*/
static void *sqlite3MemMalloc(int nByte){
  struct MemBlockHdr *pHdr;
  void **pBt;
  char *z;
  int *pInt;
  void *p = 0;
  int totalSize;
  int nReserve;
  sqlite3_mutex_enter(mem.mutex);
  assert( mem.disallow==0 );/*如果disallow==0，则不能进行内存分配*/
  nReserve = ROUND8(nByte);
  totalSize = nReserve + sizeof(*pHdr) + sizeof(int) +
               mem.nBacktrace*sizeof(void*) + mem.nTitle;/*计算一个内存所占用的总的空间大小*/
  /*以上是内存分配准备阶段*/
  p = malloc(totalSize);
  if( p ){
    z = p;
    pBt = (void**)&z[mem.nTitle];/*将大小为nTitle的数组z的首地址给pBt*/
    pHdr = (struct MemBlockHdr*)&pBt[mem.nBacktrace];/*用回溯路径的数目的指针pBt来表示内存的MemBlockHdr*/
    pHdr->pNext = 0;
    pHdr->pPrev = mem.pLast;
    if( mem.pLast ){
      mem.pLast->pNext = pHdr;
    }else{
      mem.pFirst = pHdr;
    }
    mem.pLast = pHdr;
    pHdr->iForeGuard = FOREGUARD;
    pHdr->eType = MEMTYPE_HEAP;
    pHdr->nBacktraceSlots = mem.nBacktrace;
    pHdr->nTitle = mem.nTitle;
    if( mem.nBacktrace ){
      /*如果mem.nBacktrace的值为真*/
      void *aAddr[40];
      pHdr->nBacktrace = backtrace(aAddr, mem.nBacktrace+1)-1;
      memcpy(pBt, &aAddr[1], pHdr->nBacktrace*sizeof(void*));
      assert(pBt[0]);
      if( mem.xBacktrace ){
      /*濡傛灉鏈夊洖璋冨嚱鏁帮紝閭ｄ箞鎵ц鍥炶皟鍑芥暟*/
        mem.xBacktrace(nByte, pHdr->nBacktrace-1, &aAddr[1]);
      }
    }else{
      pHdr->nBacktrace = 0;
    }
    if( mem.nTitle ){
      memcpy(z, mem.zTitle, mem.nTitle);
    }
    pHdr->iSize = nByte;
    adjustStats(nByte, +1);
    pInt = (int*)&pHdr[1];
    pInt[nReserve/sizeof(int)] = REARGUARD;
    randomFill((char*)pInt, nByte);
    memset(((char*)pInt)+nByte, 0x65, nReserve-nByte);
    p = (void*)pInt;
  }
  sqlite3_mutex_leave(mem.mutex);
  return p; 
}

/*
** Free memory.
*/
/*释放内存*/
static void sqlite3MemFree(void *pPrior){
  struct MemBlockHdr *pHdr;
  void **pBt;
  char *z;
  assert( sqlite3GlobalConfig.bMemstat || sqlite3GlobalConfig.bCoreMutex==0 
       || mem.mutex!=0 );
  pHdr = sqlite3MemsysGetHeader(pPrior);
  pBt = (void**)pHdr;
  pBt -= pHdr->nBacktraceSlots;
  sqlite3_mutex_enter(mem.mutex);
  if( pHdr->pPrev ){
    assert( pHdr->pPrev->pNext==pHdr );
    pHdr->pPrev->pNext = pHdr->pNext;
  }else{
    assert( mem.pFirst==pHdr );
    mem.pFirst = pHdr->pNext;
  }
  if( pHdr->pNext ){
    assert( pHdr->pNext->pPrev==pHdr );
    pHdr->pNext->pPrev = pHdr->pPrev;
  }else{
    assert( mem.pLast==pHdr );
    mem.pLast = pHdr->pPrev;
  }
  z = (char*)pBt;
  z -= pHdr->nTitle;
  adjustStats(pHdr->iSize, -1);
  randomFill(z, sizeof(void*)*pHdr->nBacktraceSlots + sizeof(*pHdr) +
                pHdr->iSize + sizeof(int) + pHdr->nTitle);
  free(z);
  sqlite3_mutex_leave(mem.mutex);  
}

/*
** Change the size of an existing memory allocation.
**
** For this debugging implementation, we *always* make a copy of the
** allocation into a new place in memory.  In this way, if the 
** higher level code is using pointer to the old allocation, it is 
** much more likely to break and we are much more liking to find
** the error.
**
** 更改现有的内存分配的大小。
** 对于这种调试的实现，我们总是做一分配副本储存在内存的新位置。
** 用这种方法，如果更上层的代码将用指针回到旧分配上， 
** 这样更易中断并且我们更容易找出错误
*/
static void *sqlite3MemRealloc(void *pPrior, int nByte){
  struct MemBlockHdr *pOldHdr;
  void *pNew;
  assert( mem.disallow==0 );
  assert( (nByte & 7)==0 );     /* EV: R-46199-30249 */
  pOldHdr = sqlite3MemsysGetHeader(pPrior);
  pNew = sqlite3MemMalloc(nByte);/*将一个内存分配复制到新的的地方*/
  if( pNew ){
    memcpy(pNew, pPrior, nByte<pOldHdr->iSize ? nByte : pOldHdr->iSize);
    if( nByte>pOldHdr->iSize ){
      randomFill(&((char*)pNew)[pOldHdr->iSize], nByte - pOldHdr->iSize);
    }
    sqlite3MemFree(pPrior);
  }
  return pNew;
}

/*
** Populate the low-level memory allocation function pointers in
** sqlite3GlobalConfig.m with pointers to the routines in this file.
*/
/*
将底层的内存分配函数指针地址用指向这个文件中的程序的指针填充到qlite3GlobalConfig.m中（自己的版本）*/
/*在sqlite3GlobalConfig.m与这个文件指针的例程中填充底层内存分配函数指针（以前的版本）*/
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

/*
** Set the "type" of an allocation.
*/
/*设置一个内存分配的类型*/
void sqlite3MemdebugSetType(void *p, u8 eType){
  if( p && sqlite3GlobalConfig.m.xMalloc==sqlite3MemMalloc ){
    struct MemBlockHdr *pHdr;
    pHdr = sqlite3MemsysGetHeader(p);
    assert( pHdr->iForeGuard==FOREGUARD );
    pHdr->eType = eType;
  }
}

/*
** Return TRUE if the mask of type in eType matches the type of the
** allocation p.  Also return true if p==NULL.
**
** This routine is designed for use within an assert() statement, to
** verify the type of an allocation.  For example:
**
**     assert( sqlite3MemdebugHasType(p, MEMTYPE_DB) );
*/
/*当在eType中的内存的类型的外在值与内存分配p的类型相同时，返回真。而且，当p是空值时也返回真值。

 下面这个程序是被用在assert（）函数声明中的，是为了检查分配内存的类型的。举个例子来说，assert( sqlite3MemdebugHasType(p, MEMTYPE_DB) );
*/
int sqlite3MemdebugHasType(void *p, u8 eType){
  int rc = 1;
  if( p && sqlite3GlobalConfig.m.xMalloc==sqlite3MemMalloc ){
    struct MemBlockHdr *pHdr;
    pHdr = sqlite3MemsysGetHeader(p);
    assert( pHdr->iForeGuard==FOREGUARD );         /* Allocation is valid 鍒嗛厤鏄湁鏁堢殑*/
    if( (pHdr->eType&eType)==0 ){
      rc = 0;
    }
  }
  return rc;
}

/*
** Return TRUE if the mask of type in eType matches no bits of the type of the
** allocation p.  Also return true if p==NULL.
**
** This routine is designed for use within an assert() statement, to
** verify the type of an allocation.  For example:
**
**     assert( sqlite3MemdebugNoType(p, MEMTYPE_DB) );
**
** 如果eType的字节掩码与分配的P无相匹配字节则返回真。
** 如果p为空则也为真。
** 这个程序被设计用于在一个assert（）声明里，用于验证配置类型。
** 例如：assert( sqlite3MemdebugNoType(p, MEMTYPE_DB) );
*/
/*
如果在eType中的类型的外表与（内存分配p的类型不匹配，那么就返回真值。同时，当p为空值时也返回真值。
这个程序是用在assert() 函数中的，为了检查内存分配的类型的。例如，ssert( sqlite3MemdebugNoType(p, MEMTYPE_DB) );
*/
int sqlite3MemdebugNoType(void *p, u8 eType){
  int rc = 1;
  if( p && sqlite3GlobalConfig.m.xMalloc==sqlite3MemMalloc ){
    struct MemBlockHdr *pHdr;
    pHdr = sqlite3MemsysGetHeader(p);
    assert( pHdr->iForeGuard==FOREGUARD );         /* Allocation is valid 内存分配是能够使用的*/
    if( (pHdr->eType&eType)!=0 ){
      rc = 0;
    }
  }
  return rc;
}

/*
** Set the number of backtrace levels kept for each allocation.
** A value of zero turns off backtracing.  The number is always rounded
** up to a multiple of 2.
*/
/*为每一个内存分配，设置回溯等级的深度。零值是关闭回溯，且这个数值总是2的倍数。
*/
void sqlite3MemdebugBacktrace(int depth){
  if( depth<0 ){ depth = 0; }
  if( depth>20 ){ depth = 20; }
  depth = (depth+1)&0xfe;
  mem.nBacktrace = depth;
}

void sqlite3MemdebugBacktraceCallback(void (*xBacktrace)(int, int, void **)){
  mem.xBacktrace = xBacktrace;
}

/*
** Set the title string for subsequent allocations.
*/
/*为后续的内存分配设置标题字符串*/
void sqlite3MemdebugSettitle(const char *zTitle){
  unsigned int n = sqlite3Strlen30(zTitle) + 1;
  sqlite3_mutex_enter(mem.mutex);
  if( n>=sizeof(mem.zTitle) ) n = sizeof(mem.zTitle)-1;
  memcpy(mem.zTitle, zTitle, n);
  mem.zTitle[n] = 0;
  mem.nTitle = ROUND8(n);
  sqlite3_mutex_leave(mem.mutex);
}

void sqlite3MemdebugSync(){
  struct MemBlockHdr *pHdr;
  for(pHdr=mem.pFirst; pHdr; pHdr=pHdr->pNext){
    void **pBt = (void**)pHdr;
    pBt -= pHdr->nBacktraceSlots;
    mem.xBacktrace(pHdr->iSize, pHdr->nBacktrace-1, &pBt[1]);
  }
}

/*
** Open the file indicated and write a log of all unfreed memory 
** allocations into that log.
*/
/*打开被指明的文件，然后向所有的释放了的内存中写入日志。*/
void sqlite3MemdebugDump(const char *zFilename){
  FILE *out;
  struct MemBlockHdr *pHdr;
  void **pBt;
  int i;
  out = fopen(zFilename, "w");
  if( out==0 ){
    fprintf(stderr, "** Unable to output memory debug output log: %s **\n",
                    zFilename);
    return;
  }
  for(pHdr=mem.pFirst; pHdr; pHdr=pHdr->pNext){
    char *z = (char*)pHdr;
    z -= pHdr->nBacktraceSlots*sizeof(void*) + pHdr->nTitle;
    fprintf(out, "**** %lld bytes at %p from %s ****\n", 
            pHdr->iSize, &pHdr[1], pHdr->nTitle ? z : "???");
    if( pHdr->nBacktrace ){
      fflush(out);
      pBt = (void**)pHdr;
      pBt -= pHdr->nBacktraceSlots;
      backtrace_symbols_fd(pBt, pHdr->nBacktrace, fileno(out));
      fprintf(out, "\n");
    }
  }
  fprintf(out, "COUNTS:\n");
  for(i=0; i<NCSIZE-1; i++){
    if( mem.nAlloc[i] ){
      fprintf(out, "   %5d: %10d %10d %10d\n", 
            i*8, mem.nAlloc[i], mem.nCurrent[i], mem.mxCurrent[i]);
    }
  }
  if( mem.nAlloc[NCSIZE-1] ){
    fprintf(out, "   %5d: %10d %10d %10d\n",
             NCSIZE*8-8, mem.nAlloc[NCSIZE-1],
             mem.nCurrent[NCSIZE-1], mem.mxCurrent[NCSIZE-1]);
  }
  fclose(out);
}

/*
** Return the number of times sqlite3MemMalloc() has been called.
*/
/*返回sqlite3MemMalloc()函数已被调用的次数的数目 */
int sqlite3MemdebugMallocCount(){
  int i;
  int nTotal = 0;
  for(i=0; i<NCSIZE; i++){
    nTotal += mem.nAlloc[i];
  }
  return nTotal;
}


#endif /* SQLITE_MEMDEBUG */
