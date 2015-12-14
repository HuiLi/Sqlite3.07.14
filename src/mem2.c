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
**
** 该文件包含了底层内存分配时，SQLite的驱动程序将使用标准的c-library malloc/realloc/free接口获取内存需求，
** 同时，添加很多额外的调试信息，每个分配以帮助检测和修复内存泄漏和内存使用错误。

** 该文件包含的底层的内存分配例程在sqlite3_mem_methods指定对象的实现。
*/
#include "sqliteInt.h"

/*
** This version of the memory allocator is used only if the
** SQLITE_MEMDEBUG macro is defined
**
** 这个版本的内存分配器是只有SQLITE_MEMDEBUG宏定义时使用。
*/
#ifdef SQLITE_MEMDEBUG

/*
** The backtrace functionality is only available with GLIBC
**
** 回溯功能仅可使用GLIBC
*/
#ifdef __GLIBC__
  extern int backtrace(void**,int); //函数用于获取当前线程的调用堆栈，获取的信息将会被存在指针数组中，函数返回值是实际获取的指针的个数。
  extern void backtrace_symbols_fd(void*const*,int,int);  //从backtrace函数获取的信息转化为一个字符串数组，函数返回将结果写入文件描述符为fd的文件中，每个函数对应一行。
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
**
** 每个内存分配看起来像这样：
**
**  ------------------------------------------------------------------------
**  | 标题 |  回溯指针 | 内存块内HDR |  配置|  尾|
**  ------------------------------------------------------------------------
**
** 应用程序代码只看到一个指向分配。我们必须再从分配指针
** 找到内存块内HDR。该HDR告诉我们分配的大小和回溯的指针数。
** 这也有保护语句在内存块内HDR的尾端。

** Title：用于描述这段内存，在出错时可以打印出来
** backtrace pointer：用于保留调用堆栈
** MemBlockHdr：负责这片内存的管理，以及串联未释放的MemBlock
** allocation：分配给上层的空间
** EndGuard：尾部的哨兵，用于检查内存被踩。还有个“HeadGaurd ”在MemBlockHdr中。
** 应用程序代码将只有一个指针分配。
** 我们必须从分配指针备份找到MemBlockHdr。
** 所述MemBlockHdr告诉我们的分配的大小和回溯指针的数目。还有在MemBlockHdr结束保护字。
*/
//内存分配结构
struct MemBlockHdr {
  i64 iSize;                          /* Size of this allocation  分配的大小*/
  struct MemBlockHdr *pNext, *pPrev;  /* Linked list of all unfreed memory  所有未释放内存链表*/
  char nBacktrace;                    /* Number of backtraces on this alloc  这个分配的跟踪指针数*/
  char nBacktraceSlots;               /* Available backtrace slots     可跟踪指针槽 */
  u8 nTitle;                          /* Bytes of title; includes '\0'    标题字节,包括 '\0'*/
  u8 eType;                           /* Allocation type code    分配类型代码*/
  int iForeGuard;                     /* Guard word for sanity     保护字*/
};

/*
** Guard words  
** 
** 保护字段
*/
#define FOREGUARD 0x80F5E153
#define REARGUARD 0xE4676B53

/*
** Number of malloc size increments to track.
**
** 对malloc的大小增量数量进行跟踪。
*/
#define NCSIZE  1000

/*
** All of the static variables used by this module are collected
** into a single structure named "mem".  This is to keep the
** static variables organized and to reduce namespace pollution
** when this module is combined with other in the amalgamation.
**
** 所有该模块所使用的静态变量被收集到一个单一的结构，命名为“mem”。
** 这是让静态变量变得更容易组织和减少该模块结合其他的时产生的命名空间污染。
*/
static struct {
  
  /*
  ** Mutex to control access to the memory allocation subsystem.
  **
  ** 互斥控制访问所述内存分配子系统
  */
  sqlite3_mutex *mutex;

  /*
  ** Head and tail of a linked list of all outstanding allocations
  **
  ** 所有未分配的链表的头和尾
  */
  struct MemBlockHdr *pFirst;
  struct MemBlockHdr *pLast;
  
  /*
  ** The number of levels of backtrace to save in new allocations.
  **
  ** 在新的分配中，保存回溯的级别数
  */
  int nBacktrace;
  void (*xBacktrace)(int, int, void **);

  /*
  ** Title text to insert in front of each block
  **
  ** 在每个块的前部插入标题文本
  */
  int nTitle;        /* Bytes of zTitle to save.  Includes '\0' and padding  保存zTitle的字节,包括'\ 0'和填充*/
  char zTitle[100];  /* The title text 标题文本 */

  /* 
  ** sqlite3MallocDisallow() increments the following counter.
  ** sqlite3MallocAllow() decrements it.
  **
  ** sqlite3MallocDisallow()递增以下计数器
  ** sqlite3MallocAllow()递减它
  */
  int disallow; /* Do not allow memory allocation  不让内存分配*/

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
  int mxCurrent[NCSIZE];   /* Highwater mark for nCurrent 分配数量的高水平线*/

} mem;


/*
** Adjust memory usage statistics
** 
** 调整内存使用情况统计
*/

static void adjustStats(int iSize, int increment){
  int i = ROUND8(iSize)/8;
  if( i>NCSIZE-1 ){
    i = NCSIZE - 1;
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
**
** 给定一个分配器，寻找该分配器的MemBlockHdr
** 如果不是正确的声明，这个例程将检查配置的任意一段保护。
*/
//函数设置哨兵对内存破坏进行检查
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
  assert( pInt[nReserve/sizeof(int)]==(int)REARGUARD );
  /* This checks any of the "extra" bytes allocated due
  ** to rounding up to an 8 byte boundary to ensure 
  ** they haven't been overwritten.
  **
  ** 这检查任何"额外的"分配的字节数四舍五入到8字节边界，以确保他们没有被覆盖。
  */
  while( nReserve-- > p->iSize ) assert( pU8[nReserve]==0x65 );
  return p;
}

/*
** Return the number of bytes currently allocated at address p.
**
** 返回当前分配在地址P的字节数
*/
static int sqlite3MemSize(void *p){
  struct MemBlockHdr *pHdr;
  if( !p ){
    return 0;
  }
  pHdr = sqlite3MemsysGetHeader(p);
  return pHdr->iSize;
}

/*
** Initialize the memory allocation subsystem.
**
** 初始化内存分配子系统
*/
static int sqlite3MemInit(void *NotUsed){
  UNUSED_PARAMETER(NotUsed);
  assert( (sizeof(struct MemBlockHdr)&7) == 0 );
  if( !sqlite3GlobalConfig.bMemstat ){
    /* If memory status is enabled, then the malloc.c wrapper will already
    ** hold the STATIC_MEM mutex when the routines here are invoked. */
    /*如果内存状态为已启动, 那么例程将在malloc.c封装于已持有的ATATIC_MEM互斥体里时调用。*/
    mem.mutex = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MEM);
  }
  return SQLITE_OK;
}

/*
** Deinitialize the memory allocation subsystem.
**
** 取消初始化内存分配子系统
*/
static void sqlite3MemShutdown(void *NotUsed){
  UNUSED_PARAMETER(NotUsed);
  mem.mutex = 0;
}

/*
** Round up a request size to the next valid allocation size.
**
** 向上舍入请求大小到一个有效分配的大
*/
static int sqlite3MemRoundup(int n){
  return ROUND8(n);
}

/*
** Fill a buffer with pseudo-random bytes.  This is used to preset
** the content of a new memory allocation to unpredictable values and
** to clear the content of a freed allocation to unpredictable values.
**
** 填充伪随机字节的缓冲区。这用于预置一个新的内存分配到不可预测值的内容，并清除释放分配的不可预测值的内容。
*/
//函数填充伪随机字节的缓冲区，预留一个内存分配到不可预测值并清除释放不可预测值的内容。
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
**
** 分配内存nByte字节
*/
static void *sqlite3MemMalloc(int nByte){
  struct MemBlockHdr *pHdr;
  void **pBt;
  char *z;
  int *pInt;
  void *p = 0;
  int totalSize;
  int nReserve;
  sqlite3_mutex_enter(mem.mutex);
  assert( mem.disallow==0 );
  nReserve = ROUND8(nByte);
  totalSize = nReserve + sizeof(*pHdr) + sizeof(int) +
               mem.nBacktrace*sizeof(void*) + mem.nTitle;
  p = malloc(totalSize);
  if( p ){
    z = p;
    pBt = (void**)&z[mem.nTitle];
    pHdr = (struct MemBlockHdr*)&pBt[mem.nBacktrace];
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
      /*如果backtrace深度为0.那么就不用执行*/
      void *aAddr[40];
      pHdr->nBacktrace = backtrace(aAddr, mem.nBacktrace+1)-1;
      memcpy(pBt, &aAddr[1], pHdr->nBacktrace*sizeof(void*));
      assert(pBt[0]);
      if( mem.xBacktrace ){
      /*如果有回调函数，那么执行回调函数*/
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
**
** 释放内存
*/
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
** 用这种方法，如果更高级别的代码将用指针回到旧配置上， 
** 这样更易中断并且我们更容易找出错误
*/
static void *sqlite3MemRealloc(void *pPrior, int nByte){
  struct MemBlockHdr *pOldHdr;
  void *pNew;
  assert( mem.disallow==0 );
  assert( (nByte & 7)==0 );     /* EV: R-46199-30249 */
  pOldHdr = sqlite3MemsysGetHeader(pPrior);
  pNew = sqlite3MemMalloc(nByte);
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
**
** 在sqlite3GlobalConfig.m与这个文件指针的例程中填充底层内存分配函数指针。
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

/*
** Set the "type" of an allocation.
**
** 设置分配的“类型
*/
//设置内存分配的类型
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
**
** 如果eType的类型与分配P类型相匹配，则返回真。
** 如果p==NULL则也返回真。
** 这个程序被设计用于在assert（）验证配置类型的一个声明。
** 举例如：assert( sqlite3MemdebugHasType(p, MEMTYPE_DB) );
*/
//函数使用assert（）语句验证内存分配的类型
int sqlite3MemdebugHasType(void *p, u8 eType){
  int rc = 1;
  if( p && sqlite3GlobalConfig.m.xMalloc==sqlite3MemMalloc ){
    struct MemBlockHdr *pHdr;
    pHdr = sqlite3MemsysGetHeader(p);
    assert( pHdr->iForeGuard==FOREGUARD );         /* Allocation is valid 分配是有效的*/
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
** 举个例子：assert( sqlite3MemdebugNoType(p, MEMTYPE_DB) );
*/
//验证内存分配的类型
int sqlite3MemdebugNoType(void *p, u8 eType){
  int rc = 1;
  if( p && sqlite3GlobalConfig.m.xMalloc==sqlite3MemMalloc ){
    struct MemBlockHdr *pHdr;
    pHdr = sqlite3MemsysGetHeader(p);
    assert( pHdr->iForeGuard==FOREGUARD );         /* Allocation is valid 分配是有效的*/
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
**
** 设置保持各分配回溯跟踪级别数。
** 如果值为零将关闭回溯,数字始终四舍五入到 2 的倍数。
*/
//函数设置每个内存分配回溯跟踪级别数
void sqlite3MemdebugBacktrace(int depth){
  if( depth<0 ){ depth = 0; }
  if( depth>20 ){ depth = 20; }
  depth = (depth+1)&0xfe;
  mem.nBacktrace = depth;
}

//函数内存分配的回调函数
void sqlite3MemdebugBacktraceCallback(void (*xBacktrace)(int, int, void **)){
  mem.xBacktrace = xBacktrace;
}

/*
** Set the title string for subsequent allocations.
**
** 设置标题字符串进行后续分配
*/
//函数设置标题字符串进行后续的分配
void sqlite3MemdebugSettitle(const char *zTitle){
  unsigned int n = sqlite3Strlen30(zTitle) + 1;
  sqlite3_mutex_enter(mem.mutex);
  if( n>=sizeof(mem.zTitle) ) n = sizeof(mem.zTitle)-1;
  memcpy(mem.zTitle, zTitle, n);
  mem.zTitle[n] = 0;
  mem.nTitle = ROUND8(n);
  sqlite3_mutex_leave(mem.mutex);
}

//函数同步
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
**
** 打开指定的文件，并写入日志所有未释放内存分配到该日志。
*/
//函数打印调用栈
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
**
** 返回的是函数sqlite3MemMalloc()被调用的次数。
*/
//函数返回sqlite3MemdebugMalloc()被调用的次数
int sqlite3MemdebugMallocCount(){
  int i;
  int nTotal = 0;
  for(i=0; i<NCSIZE; i++){
    nTotal += mem.nAlloc[i];
  }
  return nTotal;
}


#endif /* SQLITE_MEMDEBUG */
