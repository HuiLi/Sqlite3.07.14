/*
** 2007 October 14
**此文件是可选的底层内存分配器,通用分配器(memsys5)
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file contains the C functions that implement a memory
** allocation subsystem for use by SQLite. 
**
** 此文件包含SQLite使用的实现内存分配子系统的C函数。
**
** This version of the memory allocation subsystem omits all
** use of malloc(). The application gives SQLite a block of memory
** before calling sqlite3_initialize() from which allocations
** are made and returned by the xMalloc() and xRealloc() 
** implementations. Once sqlite3_initialize() has been called,
** the amount of memory available to SQLite is fixed and cannot
** be changed.
**
** 此版本的内存分配子系统省略了所有malloc()函数的使用，
** 该应用程序在调用sqlite3_initialize()前提供给Sqlite一个内存块，这个分配通过xMalloc()和xRealloc()实现，
** 一旦sqlite3_initialize（被调用，可用的内存SQLite的量是固定的，不能被改变。
** 
** This version of the memory allocation subsystem is included
** in the build only if SQLITE_ENABLE_MEMSYS5 is defined.
**
** 此版本的内存分配子系统仅包含在SQLITE_ENABLE_MEMSYS5的定义构建中。
** 
** This memory allocator uses the following algorithm:
**   1.  All memory allocations sizes are rounded up to a power of 2.
**
**   2.  If two adjacent free blocks are the halves of a larger block,
**       then the two blocks are coalesed into the single larger block.
**
**   3.  New memory is allocated from the first available free block.
**
** 该内存分配器使用下面的算法
** 1.所有的内存分配大小四舍五入至 2 的幂。
** 2.如果两个相邻的自由块是一个大块的两部分，那么这两个块会被合并成一个大的单独的块。
** 3.从第一个可用的空闲块分配新的内存
**
** This algorithm is described in: J. M. Robson. "Bounds for Some Functions
** Concerning Dynamic Storage Allocation". Journal of the Association for
** Computing Machinery, Volume 21, Number 8, July 1974, pages 491-499.
** 
** Let n be the size of the largest allocation divided by the minimum
** allocation size (after rounding all sizes up to a power of 2.)  Let M
** be the maximum amount of memory ever outstanding at one time.  Let
** N be the total amount of memory available for allocation.  Robson
** proved that this memory allocator will never breakdown due to 
** fragmentation as long as the following constraint holds:
**
**      N >=  M*(1 + log2(n)/2) - n + 1
**
** The sqlite3_status() logic tracks the maximum values of n and M so
** that an application can, at any time, verify this constraint.
**
** 设n是最大内存分配与最小分配的比值(四舍五入至 2 的幂)。
** 设M是应用程序曾经在任何时间点取出的最大内存数量。
** 设N 是内存的的可供分配总量。
** 罗布森证明了这个内存分配器将不会因为内存碎片崩溃，只要满足以下约束
**      N >=  M*(1 + log2(n)/2) - n + 1
** 函数Sqlite3_status() 逻辑追踪 n 和 M 的最大值所以应用程序可以在任何时间，验证此约束。
*/
#include "sqliteInt.h"

/*
** This version of the memory allocator is used only when 
** SQLITE_ENABLE_MEMSYS5 is defined.
** 
** 此版本的内存分配器只有在SQLITE_ENABLE_MEMSYS5 被定义时才被使用
*/
#ifdef SQLITE_ENABLE_MEMSYS5

/*
** A minimum allocation is an instance of the following structure.
** Larger allocations are an array of these structures where the
** size of the array is a power of 2.
** 
** 最低配置如下面程序中结构体所示.更高的配置是结构体数组，数组的大小是 2 的幂。
** 此对象的大小必须是 2 的幂。在函数memsys5Init() 中，得以验证。
** The size of this object must be a power of two.  That fact is
** verified in memsys5Init().定义结构体Mem5Link
*/
typedef struct Mem5Link Mem5Link;
struct Mem5Link {
  int next;       /* Index of next free chunk *//*下一个空闲块的索引*/
  int prev;       /* Index of previous free chunk *//*前一个空闲块的索引*/
};
/*next指向下一个空闲chunk的地址，prev指向上一个空闲chunk的
地址， prev = -1 表示该chunk在双向链表的表头位置，没有prev；*/

/*
** Maximum size of any allocation is ((1<<LOGMAX)*mem5.szAtom). Since
** mem5.szAtom is always at least 8 and 32-bit integers are used,
** it is not actually possible to reach this limit.
**
** 任何分配的最大尺寸为（（1<< LOGMAX）* mem5.azAtom）。
** 因为mem5.Atom总是至少8位和32位的整数时，它实际上不可能达到这个限度。
*/
#define LOGMAX 30             /*宏定义LOGMAX为30*/

/*
** Masks used for mem5.aCtrl[] elements.以下两个常量用于 mem5.aCtrl [] 元素的掩码
*/
#define CTRL_LOGSIZE  0x1f    /* Log2 Size of this block *//*宏定义 CTRL_LOGSIZE块的大小，为16进制的1f，即十进制的31*/
#define CTRL_FREE     0x20    /* True if not checked out */

/*
** All of the static variables used by this module are collected
** into a single structure named "mem5".  This is to keep the
** static variables organized and to reduce namespace pollution
** when this module is combined with other in the amalgamation.
**
** 本模块使用的所有静态变量被聚集在一个结构体”mem5”中。
** 这样当本模块和其他模块合并时，就能保持这些静态变量，并且能减少命名空间污染。
** mem5是Mem5Global类型的结构体.
*/
static SQLITE_WSD struct Mem5Global {
  /*
  ** Memory available for allocation可供分配的内存
  */
  int szAtom;      /* Smallest possible allocation in bytes*//*以字节为单位的最小可能分配*/
  int nBlock;      /* Number of szAtom sized blocks in zPool*//*在zPool存储池中一些szAtom大小的块数目 */
  u8 *zPool;       /* Memory available to be allocated *//*可用来分配的内存，zPool是无符号字符类型的指针*/
  
  /*
  ** Mutex to control access to the memory allocation subsystem.
  */
  sqlite3_mutex *mutex;    /*互斥锁来控制对内存分配子系统访问*/

  /*
  ** Performance statistics
  ** 性能统计信息 u32:无符号整型
  */
  u64 nAlloc;         /* Total number of calls to malloc *//*调用malloc总数*/
  u64 totalAlloc;     /* Total of all malloc calls - includes internal frag *//*所有Mallocde 调用总合— 包括内部碎片*/
  u64 totalExcess;    /* Total internal fragmentation *//*总内部碎片*/
  u32 currentOut;     /* Current checkout, including internal fragmentation *//*当前校验，包括内部碎片*/
  u32 currentCount;   /* Current number of distinct checkouts *//*当前不同的检出数*/
  u32 maxOut;         /* Maximum instantaneous currentOut *//*currentOut瞬时最大值*/
  u32 maxCount;       /* Maximum instantaneous currentCount *//*currentCount 瞬时最大值*/
  u32 maxRequest;     /* Largest allocation (exclusive of internal frag) *//*最大配置 （不包括内部碎片）*/
  /*整体思想为buddy算法
  ** Lists of free blocks.  aiFreelist[0] is a list of free blocks of
  ** size mem5.szAtom.  aiFreelist[1] holds blocks of size szAtom*2.
  ** and so forth.
  */
  /*
  **空闲块数组。 aiFreelist[0]是大小为mem5.szAtom空闲块数组。 
  aiFreelist[1]大小为szAtom*2，以此类推。
  */
/*使用了一个大小为31个字节的空闲链表来保存空闲的block，
 值为 -1 表示是空链表；
 aiFreelist[0] 保存了大小为 mem5.szAtom个字节的空闲block  
 aiFreelist[1] 保存了大小为 mem5.szAtom * 2 个字节的空闲block  
 等等；*/
  int aiFreelist[LOGMAX+1];

  /*
  ** Space for tracking which blocks are checked out and the size
  ** of each block.  One byte per block.
  */
  u8 *aCtrl;
/*aCtrl是无符号类型的指针用于追踪哪些内存块被划出，检查了每块的大小。*/
/*aCtrl是一个字符数组，保存了每个block的控制信息，当前block的大小，  
  以及是否checkout，等等，每个大小为szAtom的block 占一个位置；*/
} mem5;

/*
** Access the static variable through a macro for SQLITE_OMIT_WSD
**
** 通过宏sqlite_omit_wsd访问静态变量
*/
#define mem5 GLOBAL(struct Mem5Global, mem5)

/*
** Assuming mem5.zPool is divided up into an array of Mem5Link
** structures, return a pointer to the idx-th such lik.
**
** 假设mem5.zPool被分成Mem5Link结构的阵列，则返回一个指向idx的指针
*/
#define MEM5LINK(idx) ((Mem5Link *)(&mem5.zPool[(idx)*mem5.szAtom]))

/*
** Unlink the chunk at mem5.aPool[i] from list it is currently
** on.  It should be found on mem5.aiFreelist[iLogsize].
**
** 从当前的链表中取消可用内存块的链接。该内存块位于mem5.aiFreelist[iLogsize]
*/
static void memsys5Unlink(int i, int iLogsize){
  int next, prev;
  assert( i>=0 && i<mem5.nBlock );
  assert( iLogsize>=0 && iLogsize<=LOGMAX );
  assert( (mem5.aCtrl[i] & CTRL_LOGSIZE)==iLogsize );
/*assert是宏，用于断言其作用是如果它的条件返回错误，则终止程序执行 。
在函数memsys5Unlink开始处检验传入参数i和iLogsize的合法性*/
  next = MEM5LINK(i)->next;
  prev = MEM5LINK(i)->prev;              /*将该节点从链表中删除*/
  if( prev<0 ){
    mem5.aiFreelist[iLogsize] = next;    /*将该节点从链表中删除*/
  }else{
    MEM5LINK(prev)->next = next;
  }
  if( next>=0 ){
    MEM5LINK(next)->prev = prev;
  }
}

/*
** Link the chunk at mem5.aPool[i] so that is on the iLogsize
** free list.
**
** 将 mem5.aPool[i] 位置的chunk， 插入到iLogSize链表的头部
** 链接可用内存分配aPool中的块，此块在空闲块数组中
*/
static void memsys5Link(int i, int iLogsize){
  int x;
  assert( sqlite3_mutex_held(mem5.mutex) );
  assert( i>=0 && i<mem5.nBlock );
  assert( iLogsize>=0 && iLogsize<=LOGMAX );
  assert( (mem5.aCtrl[i] & CTRL_LOGSIZE)==iLogsize );

  x = MEM5LINK(i)->next = mem5.aiFreelist[iLogsize];  /*将 mem5.aPool[i] 位置的chunk， 插入到iLogSize链表的头部*/
  MEM5LINK(i)->prev = -1;
  if( x>=0 ){
    assert( x<mem5.nBlock );
    MEM5LINK(x)->prev = i;
  }
  mem5.aiFreelist[iLogsize] = i;
}

/*
** If the STATIC_MEM mutex is not already held, obtain it now. The mutex
** will already be held (obtained by code in malloc.c) if
** sqlite3GlobalConfig.bMemStat is true.
**
** 如果STATIC_MEM互斥锁未发生，则进行加锁。如果sqlite3GlobalConfig.
** bMemStat互斥状态为真，那么互斥状态在malloc.c中就已经生成。
*/
static void memsys5Enter(void){
  sqlite3_mutex_enter(mem5.mutex);        /*加互斥锁*/
}
static void memsys5Leave(void){
  sqlite3_mutex_leave(mem5.mutex);        /*退出互斥锁*/
}

/*
** Return the size of an outstanding allocation, in bytes.  The
** size returned omits the 8-byte header overhead.  This only
** works for chunks that are currently checked out.
**
** 返回一个以字节为单位的未分配的内存大小,该返回值省略了8字节大小的头开销。
** 该函数只适用于当前划分出的内存块。
*/
static int memsys5Size(void *p){
  int iSize = 0;
  if( p ){
    int i = ((u8 *)p-mem5.zPool)/mem5.szAtom;
    assert( i>=0 && i<mem5.nBlock );
    iSize = mem5.szAtom * (1 << (mem5.aCtrl[i]&CTRL_LOGSIZE));
  }
  return iSize;
}

/*
** Find the first entry on the freelist iLogsize.  Unlink that
** entry and return its index. 
** 
** 查找空闲链表中的第一项条目，并返回其索引。
** 找到freelist链表中的第iLogSize项指向的链表中的，
** 下标最小的那一项的下标，返回这个下标
*/
static int memsys5UnlinkFirst(int iLogsize){
  int i;
  int iFirst;
/*iFirst指向 下标最小的那一项*/
  assert( iLogsize>=0 && iLogsize<=LOGMAX );   /*判断传入参数的合法性*/
  i = iFirst = mem5.aiFreelist[iLogsize];
  assert( iFirst>=0 );
  while( i>0 ){
    if( i<iFirst ) iFirst = i;
    i = MEM5LINK(i)->next;
  }
  memsys5Unlink(iFirst, iLogsize);
  return iFirst;
}
/*将 iLogSize链表中的下标最小的那一项，也就是第iFirst 个从链表中删除*/

/*
** Return a block of memory of at least nBytes in size.
** Return NULL if unable.  Return NULL if nBytes==0.
**返回一个大小至少为nBytes的内存块。如果不能或nBytes等于0，返回空值NULL.。
** The caller guarantees that nByte positive.
**调用方需保证nByte 为正。
** The caller has obtained a mutex prior to invoking this
** routine so there is never any chance that two or more
** threads can be in this routine at the same time.
**调用方在调用这个线程前会加上互斥锁，所以不可能有两个或多个该线程同时发生。
*/
static void *memsys5MallocUnsafe(int nByte){
  int i;           /* Index of a mem5.aPool[] slot */  /*aPool的索引*/
  int iBin;        /* Index into mem5.aiFreelist[] */  /*空闲链表的索引*/
  int iFullSz;     /* Size of allocation rounded up to power of 2 */ /*内存分配大小四舍五入至 2 的幂*/
  int iLogsize;    /* Log2 of iFullSz/POW2_MIN */

  /* nByte must be a positive */
  assert( nByte>0 );    /*nByte必须大于0*/

  /* Keep track of the maximum allocation request.  Even unfulfilled
  ** requests are counted */
  /*比较nByte与最大分配请求，若nByte较大，则将nByte的值赋予该请求*/
  if( (u32)nByte>mem5.maxRequest ){
    mem5.maxRequest = nByte;
  }

  /* Abort if the requested allocation size is larger than the largest
  ** power of two that we can represent using 32-bit signed integers.
  */
  /*如果nByte 大于32位有符号整数能表示的2的幂的最大值，那么返回0程序中止*/
  if( nByte > 0x40000000 ){
    return 0;
  }

  /* Round nByte up to the next valid power of two nByte作用范围到下一个2的幂*/
  for(iFullSz=mem5.szAtom, iLogsize=0; iFullSz<nByte; iFullSz *= 2, iLogsize++){}

  /* Make sure mem5.aiFreelist[iLogsize] contains at least one free
  ** block.  If not, then split a block of the next larger power of
  ** two in order to create a new free block of size iLogsize.
  确保mem5.aiFreelist[iLogsize] 中至少包含一个空闲块。
  如果没有，那就把两块中效益更大的一块分裂
  以创建一个新的大小iLogsize空闲块。
  */
  /*aiFreelist[iLogsize]至少包含一个空闲块。
  如果没有，那么就从下一个大小为2的幂的内存块中划出iLogsize大小的内存块，作为的新的空闲块。*/
/*找到FreeList链表中的第iLogsize项指向的链表，如果为空，则向上找，
  直到找到第一个不为空的项为止*/
  for(iBin=iLogsize; mem5.aiFreelist[iBin]<0 && iBin<=LOGMAX; iBin++){}
  if( iBin>LOGMAX ){      /*没有找到这样的项，则返回失败*/
    testcase( sqlite3GlobalConfig.xLog!=0 );
    sqlite3_log(SQLITE_NOMEM, "failed to allocate %u bytes", nByte);
    return 0;
  }
  i = memsys5UnlinkFirst(iBin);    /*找到了这样的项，则从freelist中删除*/
/*
  将最大的块iBin 大小切分成  iBin / 2 , 链入对应的iBin链表；
  然后切分成  iBin / 4 , 链入对应的iBin（此时iBin已经--了，指向上一项）链表，然后切分成  iBin / 8 ,等等，
  直到 剩余到的iBin可以放得下 iLogSize为止
  */
  while( iBin>iLogsize ){
    int newSize;

    iBin--;
    newSize = 1 << iBin;
    mem5.aCtrl[i+newSize] = CTRL_FREE | iBin;
    memsys5Link(i+newSize, iBin);
  }
  mem5.aCtrl[i] = iLogsize;

  /* Update allocator performance statistics. */
  /*更新内存分配器性能统计数据。*/
  mem5.nAlloc++;
  mem5.totalAlloc += iFullSz;
  mem5.totalExcess += iFullSz - nByte;
  mem5.currentCount++;
  mem5.currentOut += iFullSz;
  if( mem5.maxCount<mem5.currentCount ) mem5.maxCount = mem5.currentCount;
  if( mem5.maxOut<mem5.currentOut ) mem5.maxOut = mem5.currentOut;

  /* Return a pointer to the allocated memory. */
  return (void*)&mem5.zPool[i*mem5.szAtom];   /*返回一个指向所分配内存的指针。*/
}

/*
** Free an outstanding memory allocation.
**
** 释放未分配内存
*/
static void memsys5FreeUnsafe(void *pOld){
  u32 size, iLogsize;
  int iBlock;

  /* Set iBlock to the index of the block pointed to by pOld in 
  ** the array of mem5.szAtom byte blocks pointed to by mem5.zPool.
  */
/*iBlock 为当前要释放的块在pool中的下标*/
  iBlock = ((u8 *)pOld-mem5.zPool)/mem5.szAtom;   /*设置iBlock为内存块的索引指向 mem5.zPool与mem5.szAtom的比值*/

  /* Check that the pointer pOld points to a valid, non-free block. */
  /*检查指针pOld是否指向一个有效的非空闲块。*/
  assert( iBlock>=0 && iBlock<mem5.nBlock );
  assert( ((u8 *)pOld-mem5.zPool)%mem5.szAtom==0 );
  assert( (mem5.aCtrl[iBlock] & CTRL_FREE)==0 );

  iLogsize = mem5.aCtrl[iBlock] & CTRL_LOGSIZE;
  size = 1<<iLogsize;
  assert( iBlock+size-1<(u32)mem5.nBlock );

  mem5.aCtrl[iBlock] |= CTRL_FREE;
  mem5.aCtrl[iBlock+size-1] |= CTRL_FREE;
  assert( mem5.currentCount>0 );
  assert( mem5.currentOut>=(size*mem5.szAtom) );
  mem5.currentCount--;
  mem5.currentOut -= size*mem5.szAtom;
  assert( mem5.currentOut>0 || mem5.currentCount==0 );
  assert( mem5.currentCount>0 || mem5.currentOut==0 );

  mem5.aCtrl[iBlock] = CTRL_FREE | iLogsize;
/*
  由于分配都是按照 iLogsize的整数倍来分配的，所以iBlock有可能落在
  单数倍和偶数倍上，则需要按照是单数倍还是偶数倍来向前或者向后合并，  
  例如 iBlock   = 4 ， iLogSize  = 2， 这时iBlock就落在单数倍上了，需要向前合并；   
  iBuddy =  iBlock - size;
  iBlock  = 8， iLogSize  = 2， 这是iBlock就落在偶数倍上，需要向后合并；
  iBuddy =  iBlock  + size;*/
  while( ALWAYS(iLogsize<LOGMAX) ){
    int iBuddy;
    if( (iBlock>>iLogsize) & 1 ){
      iBuddy = iBlock - size;
    }else{
      iBuddy = iBlock + size;
    }
    assert( iBuddy>=0 );
    if( (iBuddy+(1<<iLogsize))>mem5.nBlock ) break;
    if( mem5.aCtrl[iBuddy]!=(CTRL_FREE | iLogsize) ) break;
    memsys5Unlink(iBuddy, iLogsize);
    iLogsize++;
    if( iBuddy<iBlock ){
      mem5.aCtrl[iBuddy] = CTRL_FREE | iLogsize;
      mem5.aCtrl[iBlock] = 0;
      iBlock = iBuddy;
    }else{
      mem5.aCtrl[iBlock] = CTRL_FREE | iLogsize;
      mem5.aCtrl[iBuddy] = 0;
    }
    size *= 2;
  }
  memsys5Link(iBlock, iLogsize);
}

/*
** Allocate nBytes of memory
** 分配大小为nBytes的内存
*/
static void *memsys5Malloc(int nBytes){
  sqlite3_int64 *p = 0;
  if( nBytes>0 ){
    memsys5Enter();
    p = memsys5MallocUnsafe(nBytes);
    memsys5Leave();
  }
  return (void*)p; 
}

/*
** Free memory.
** 可用内存
** The outer layer memory allocator prevents this routine from
** being called with pPrior==0.
** 当pPrior==0，防止外层内存分配器调用此程序
*/
static void memsys5Free(void *pPrior){
  assert( pPrior!=0 );
  memsys5Enter();
  memsys5FreeUnsafe(pPrior);
  memsys5Leave();  
}

/*
** Change the size of an existing memory allocation.
**
** The outer layer memory allocator prevents this routine from
** being called with pPrior==0.  
**
** nBytes is always a value obtained from a prior call to
** memsys5Round().  Hence nBytes is always a non-negative power
** of two.  If nBytes==0 that means that an oversize allocation
** (an allocation larger than 0x40000000) was requested and this
** routine should return 0 without freeing pPrior.
**
** 改变现有的内存分配的大小。
** 当pPrior==0，防止外层内存分配器调用此程序
** nBytes的值从调用memsys5Round()函数得来。因此nBytes的值始终为正的2次幂。
** 如果nBytes = = 0意味着一个溢出的内存分配请求(分配大于0 x40000000),这个函数返回0并且不释放指针pPrior。
*/
static void *memsys5Realloc(void *pPrior, int nBytes){
  int nOld;
  void *p;
  assert( pPrior!=0 );
  assert( (nBytes&(nBytes-1))==0 );  /* EV: R-46199-30249 */
  assert( nBytes>=0 );
  if( nBytes==0 ){
    return 0;
  }
  nOld = memsys5Size(pPrior);
  if( nBytes<=nOld ){
    return pPrior;
  }
  memsys5Enter();
  p = memsys5MallocUnsafe(nBytes);
  if( p ){
    memcpy(p, pPrior, nOld);
    memsys5FreeUnsafe(pPrior);
  }
  memsys5Leave();
  return p;
}

/*
** Round up a request size to the next valid allocation size.  If
** the allocation is too large to be handled by this allocation system,
** return 0.
** 计算一个请求到下一个有效分配的大小。如果请求分配的内存过大，无法通过该内存分配系统来处理，则返回0。
** All allocations must be a power of two and must be expressed by a
** 32-bit signed integer.  Hence the largest allocation is 0x40000000
** or 1073741824 bytes.
** 所有的内存分配大小必须是2的幂，并且必须由一个32位有符号整数表示。
** 因此，最大的内存分配是0x40000000或1073741824字节。*/
static int memsys5Roundup(int n){
  int iFullSz;
  if( n > 0x40000000 ) return 0;
  for(iFullSz=mem5.szAtom; iFullSz<n; iFullSz *= 2);
  return iFullSz;
}

/*
** Return the ceiling of the logarithm base 2 of iValue.
**返回向上取整的以2为底数的对数值iValue
** Examples:   memsys5Log(1) -> 0
**             memsys5Log(2) -> 1
**             memsys5Log(4) -> 2
**             memsys5Log(5) -> 3
**             memsys5Log(8) -> 3
**             memsys5Log(9) -> 4
*/
static int memsys5Log(int iValue){
  int iLog;
  for(iLog=0; (iLog<(int)((sizeof(int)*8)-1)) && (1<<iLog)<iValue; iLog++);
  return iLog;
}

/*
** Initialize the memory allocator.
** 初始化内存分配器。这个例程并不是线程安全的。调用方必须加上互斥锁来防止多个线程同时调用.
** This routine is not threadsafe.  The caller must be holding a mutex
** to prevent multiple threads from entering at the same time.
*/
static int memsys5Init(void *NotUsed){
  int ii;            /* Loop counter 循环计数器*/
  int nByte;         /* Number of bytes of memory available to this allocator 这个分配器可用的内存字节数*/
  u8 *zByte;         /* Memory usable by this allocator 通过这种分配得到的可用内存*/
  int nMinLog;       /* Log base 2 of minimum allocation size in bytes 基于对数2的最小分配的字节数*/
  int iOffset;       /* An offset into mem5.aCtrl[] mem5.aCtrl[]的偏移量*/

  UNUSED_PARAMETER(NotUsed);

  /* For the purposes of this routine, disable the mutex */
  mem5.mutex = 0;          /*禁用互斥锁*/

  /* The size of a Mem5Link object must be a power of two.  Verify that
  ** this is case.
  */
  assert( (sizeof(Mem5Link)&(sizeof(Mem5Link)-1))==0 );   /*Mem5Link大小的对象必须是2的幂*/

  nByte = sqlite3GlobalConfig.nHeap;
  zByte = (u8*)sqlite3GlobalConfig.pHeap;
  assert( zByte!=0 );  /* sqlite3_config() does not allow otherwise */
                       /*若断言assert返回值错误，sqlite3_config（）不执行*/
  /* boundaries on sqlite3GlobalConfig.mnReq are enforced in sqlite3_config() */
  /*在sqlite3GlobalConfig.mnReq边界强制执行sqlite3_config（）*/
  nMinLog = memsys5Log(sqlite3GlobalConfig.mnReq);
  mem5.szAtom = (1<<nMinLog);
  while( (int)sizeof(Mem5Link)>mem5.szAtom ){
    mem5.szAtom = mem5.szAtom << 1;
  }

  mem5.nBlock = (nByte / (mem5.szAtom+sizeof(u8)));
  mem5.zPool = zByte;
  mem5.aCtrl = (u8 *)&mem5.zPool[mem5.nBlock*mem5.szAtom];

  for(ii=0; ii<=LOGMAX; ii++){
    mem5.aiFreelist[ii] = -1;
  }

  iOffset = 0;
  for(ii=LOGMAX; ii>=0; ii--){
    int nAlloc = (1<<ii);
    if( (iOffset+nAlloc)<=mem5.nBlock ){
      mem5.aCtrl[iOffset] = ii | CTRL_FREE;
      memsys5Link(iOffset, ii);
      iOffset += nAlloc;
    }
    assert((iOffset+nAlloc)>mem5.nBlock);
  }

  /* If a mutex is required for normal operation, allocate one */
  /*如果程序正常运行需要互斥,则分配一个互斥锁 */
  if( sqlite3GlobalConfig.bMemstat==0 ){
    mem5.mutex = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MEM);
  }

  return SQLITE_OK;
}

/*
** Deinitialize this module.取消初始化这个模块。
*/
static void memsys5Shutdown(void *NotUsed){
  UNUSED_PARAMETER(NotUsed);
  mem5.mutex = 0;
  return;
}

#ifdef SQLITE_TEST
/*
** Open the file indicated and write a log of all unfreed memory 
** allocations into that log.
** 打开日志文件显示并写入所有非空闲内存分配
*/
void sqlite3Memsys5Dump(const char *zFilename){
  FILE *out;
  int i, j, n;
  int nMinLog;

  if( zFilename==0 || zFilename[0]==0 ){
    out = stdout;
  }else{
    out = fopen(zFilename, "w");
    if( out==0 ){
      fprintf(stderr, "** Unable to output memory debug output log: %s **\n",
                      zFilename);
      return;
    }
  }
  memsys5Enter();
  nMinLog = memsys5Log(mem5.szAtom);
  for(i=0; i<=LOGMAX && i+nMinLog<32; i++){
    for(n=0, j=mem5.aiFreelist[i]; j>=0; j = MEM5LINK(j)->next, n++){}
    fprintf(out, "freelist items of size %d: %d\n", mem5.szAtom << i, n);
  }
  fprintf(out, "mem5.nAlloc       = %llu\n", mem5.nAlloc);
  fprintf(out, "mem5.totalAlloc   = %llu\n", mem5.totalAlloc);
  fprintf(out, "mem5.totalExcess  = %llu\n", mem5.totalExcess);
  fprintf(out, "mem5.currentOut   = %u\n", mem5.currentOut);
  fprintf(out, "mem5.currentCount = %u\n", mem5.currentCount);
  fprintf(out, "mem5.maxOut       = %u\n", mem5.maxOut);
  fprintf(out, "mem5.maxCount     = %u\n", mem5.maxCount);
  fprintf(out, "mem5.maxRequest   = %u\n", mem5.maxRequest);
  memsys5Leave();
  if( out==stdout ){
    fflush(stdout);
  }else{
    fclose(out);
  }
}
#endif

/*
** This routine is the only routine in this file with external 
** linkage. It returns a pointer to a static sqlite3_mem_methods
** struct populated with the memsys5 methods.
** 
** 此函数是这个文件唯一与外部联系的函数。它返回一个指向sqlite3_mem_methods的指针
*/
const sqlite3_mem_methods *sqlite3MemGetMemsys5(void){
  static const sqlite3_mem_methods memsys5Methods = {
     memsys5Malloc,
     memsys5Free,
     memsys5Realloc,
     memsys5Size,
     memsys5Roundup,
     memsys5Init,
     memsys5Shutdown,
     0
  };
  return &memsys5Methods;
}

#endif /* SQLITE_ENABLE_MEMSYS5 */
