/*
** 2007 October 14
**
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
** This version of the memory allocation subsystem omits all
** use of malloc(). The SQLite user supplies a block of memory
** before calling sqlite3_initialize() from which allocations
** are made and returned by the xMalloc() and xRealloc() 
** implementations. Once sqlite3_initialize() has been called,
** the amount of memory available to SQLite is fixed and cannot
** be changed.
**
** This version of the memory allocation subsystem is included
** in the build only if SQLITE_ENABLE_MEMSYS3 is defined.
**
** 此文件包含C函数实现使用SQLite存储分配子系统。 
** 这个版本的内存分配子系统没有使用malloc()。
** SQLite用户提供了一个内存块用于sqlite3_initialize()之前调用和返回xMalloc()和xRealloc()的分配实现。
** 一旦sqlite3_initialize()被调用，则SQLite的可用内存量是固定的，不能改变。
** 
** 这个内存分配器需要定义SQLITE_ENABLE_MEMSYS3来将其加入构建中。
*/
#include "sqliteInt.h"           //SQLite初始化

/*
** This version of the memory allocator is only built into the library
** SQLITE_ENABLE_MEMSYS3 is defined. Defining this symbol does not
** mean that the library will use a memory-pool by default, just that
** it is available. The mempool allocator is activated by calling
** sqlite3_config().
**
** 这个版本的内存分配器是由数据字典里的SQLITE_ENABLE_MEMSYS3定义建成的。
** 定义该符号并不意味着字典将默认使用内存池，它只是可用的。
** 内存池分配器需要sqlite3_config()来激活。
*/

#ifdef SQLITE_ENABLE_MEMSYS3   //宏定义触发该内存分配子系统被组建到库中

/*
** Maximum size (in Mem3Blocks) of a "small" chunk.
*/
#define MX_SMALL 10    //Mem3Blocks中的一个块


/*
** Number of freelist hash slots     
**
** 在mem3块中一“小“块的最大尺寸。
*/
#define N_HASH  61    //自由列表中hash slots的个数

/*
** A memory allocation (also called a "chunk") consists of two or 
** more blocks where each block is 8 bytes.  The first 8 bytes are 
** a header that is not returned to the user.
**
** A chunk is two or more blocks that is either checked out or
** free.  The first block has format u.hdr.  u.hdr.size4x is 4 times the
** size of the allocation in blocks if the allocation is free.
** The u.hdr.size4x&1 bit is true if the chunk is checked out and
** false if the chunk is on the freelist.  The u.hdr.size4x&2 bit
** is true if the previous chunk is checked out and false if the
** previous chunk is free.  The u.hdr.prevSize field is the size of
** the previous chunk in blocks if the previous chunk is on the
** freelist. If the previous chunk is checked out, then
** u.hdr.prevSize can be part of the data for that chunk and should
** not be read or written.
**
** We often identify a chunk by its index in mem3.aPool[].  When
** this is done, the chunk index refers to the second block of
** the chunk.  In this way, the first chunk has an index of 1.
** A chunk index of 0 means "no such chunk" and is the equivalent
** of a NULL pointer.
**
** The second block of free chunks is of the form u.list.  The
** two fields form a double-linked list of chunks of related sizes.
** Pointers to the head of the list are stored in mem3.aiSmall[] 
** for smaller chunks and mem3.aiHash[] for larger chunks.
**
** The second block of a chunk is user data if the chunk is checked 
** out.  If a chunk is checked out, the user data may extend into
** the u.hdr.prevSize value of the following chunk.
**
** 一个内存分配（也被称为“块“）由两个或多个8字节的块组成。
** 第一个8字节头块不返回给用户。一块是由两个或两个以上，自由出入的块组成。
** 第一块格式为u.hdr。  如果分配自由则u.hdr.size4x 将分配4倍块大小
** 如果块在自由列表上则u.hdr.size4x&1字节错误，若块被检查那么u.hdr.size4x&1字节是真。
** 如果前一块被检查则u.hdr.size4x&2 字节为真，若前一块为自由则u.hdr.size4x&2字节为假。
** 若前一块在自由列表上，则u.hdr.prevSize空间大小是前一块块上大小。
** 如果前一个块被检查，那u.hdr.prevSize作为数据块的一部分，不能被读与写。
**
** 我们经常定义一个块的索引在 mem3.aPool[]中。这样做时，这块的索引与第二块相关。
** 用这种方法，第一个块有了1索引。一个块索引为0意味着“无这样的块”和等效为一个空指针u.list由第二块为自由块组成。
** 这两块领域形成一个双链表的相关尺寸块。 
** 较小的块将头指针储存在mem3.aiSmall[]，较大的块将头指针储存在mem3.aiHash[]
** 如果chunk被检出，那么chunk中的第二个块是用户数据。
** 如果chunk被检出,那么用户数据将会被延伸至下一个chunk的u.hdr.prevSize值内。
*/
typedef struct Mem3Block Mem3Block;   //定义一个任意类型的数据块结构体
struct Mem3Block {
  union {       //联合体，它里面定义的每个变量使用同一段内存空间，达到节约空间的目的
    struct {
      u32 prevSize;   /* Size of previous chunk in Mem3Block elements  前一个chunk的大小 */
      u32 size4x;     /* 4x the size of current chunk in Mem3Block elements  当前chunk大小的4倍 */
    } hdr;    //chunk中的第一个block名为hdr，不返回给用户
    struct {
      u32 next;       /* Index in mem3.aPool[] of next free chunk  下一个未使用chunk的索引号 */
      u32 prev;       /* Index in mem3.aPool[] of previous free chunk  前一个未使用chunk的索引号 */
    } list;   //chunk中的第二个block名为list
  } u;   //定义了一个名为u的chunk
};

/*
** All of the static variables used by this module are collected
** into a single structure named "mem3".  This is to keep the
** static variables organized and to reduce namespace pollution
** when this module is combined with other in the amalgamation.
** 
** 所有通过该模块使用静态变量的被收集到同一单一结构中，被命名为“mem3”。
** 这是当这个模块与其他相融合时保持静态变量组织并减少命名空间浪费。
*/
static SQLITE_WSD struct Mem3Global {
  /*
  ** Memory available for allocation. nPool is the size of the array
  ** (in Mem3Blocks) pointed to by aPool less 2.
  ** 
  ** 可用内存. nPool配置的可用内存大小为数组(in Mem3Blocks)所指的小于2的aPool。
  */
  u32 nPool;   //内存变量数组分配的空间大小
  Mem3Block *aPool;//指向Mem3Block类型变量的指针

  /*
  ** True if we are evaluating an out-of-memory callback.  
  **
  ** 如果我们评估了内存出的回溯则为真。
  */
  int alarmBusy;  //为真时进行内存回收
  
  /*
  ** Mutex to control access to the memory allocation subsystem.  
  **
  ** 控制访问互斥内存分配子系统。
  */
  sqlite3_mutex *mutex;    //控制内存分配子系统的访问
  
  /*
  ** The minimum amount of free space that we have seen.  
  ** 
  ** 这是我们见过的最小自由空间量。
  */
  u32 mnMaster;    //最小可分配空闲空间的大小

  /*
  ** iMaster is the index of the master chunk.  Most new allocations
  ** occur off of this chunk.  szMaster is the size (in Mem3Blocks)
  ** of the current master.  iMaster is 0 if there is not master chunk.
  ** The master chunk is not in either the aiHash[] or aiSmall[].
  **
  ** iMaster是主块索引。这个块发生大部分新分配。szMaster的大小（在Mem3Blocks）由当前主块决定。
  ** 如果没有主块，则iMaster为0.主块既不在aiHash[]，也不在aiSmall[]。
  */
  u32 iMaster;  //新分配的chunk的索引号
  u32 szMaster;  //当前chunk的大小(block的数目)，不构成双链表

  /*
  ** Array of lists of free blocks according to the block size 
  ** for smaller chunks, or a hash on the block size for larger
  ** chunks.
  ** 
  ** 根据块的大小为更小的块排列空闲块列表数组，或是为更大块建哈希表。
  */
  
  u32 aiSmall[MX_SMALL-1]; /* For sizes 2 through MX_SMALL, inclusive  双链表中较小的chunk数组 */
  u32 aiHash[N_HASH];        /* For sizes MX_SMALL+1 and larger  较大chunk */
} mem3 = { 97535575 };//定义一个名为mem3的全局变量并赋值

#define mem3 GLOBAL(struct Mem3Global, mem3)
/*
** Unlink the chunk at mem3.aPool[i] from list it is currently
** on.  *pRoot is the list that i is a member of.
** 
** 该函数把当前使用的块移出列表
*/   
//将第i个项从双向链表中删除， 如果是第一个，则调整pRoot指针；
//同时调整 前一个的next指针，和后一个的prev指针。将卸下来的这一项的prev和next都设置为null。               
static void memsys3UnlinkFromList(u32 i, u32 *pRoot){
  u32 next = mem3.aPool[i].u.list.next;  //将索引号为aPool[i]的块的下一个块索引号赋值给next
  u32 prev = mem3.aPool[i].u.list.prev;  //将索引号为aPool[i]的块的前一个块索引号赋给prev
  assert( sqlite3_mutex_held(mem3.mutex) );//若当前有互斥锁，则终止程序
  if( prev==0 ){    //若当前chunk的前一个chunk不存在
    *pRoot = next;     //将指针pRoot指向下一个chunk的 索引号
  }else{ 
    mem3.aPool[prev].u.list.next = next;//否则将当前chunk的next赋给前一个chunk的下一个chunk
  }
  if( next ){
    mem3.aPool[next].u.list.prev = prev;
  }
  mem3.aPool[i].u.list.next = 0;
  mem3.aPool[i].u.list.prev = 0; //将当前使用的chunk从list中移出，前后指向为0表示没有该块
}

/*
** Unlink the chunk at index i from 
** whatever list is currently a member of.
** 
** 该函数将某个块移出列表
*/    
//从双向循环链表中删除第i项。  
static void memsys3Unlink(u32 i){
  u32 size, hash;
  assert( sqlite3_mutex_held(mem3.mutex) );
  assert( (mem3.aPool[i-1].u.hdr.size4x & 1)==0 );  
  assert( i>=1 );                              
  size = mem3.aPool[i-1].u.hdr.size4x/4;

  assert( size==mem3.aPool[i+size-1].u.hdr.prevSize );
  assert( size>=2 );               
  if( size <= MX_SMALL ){
    memsys3UnlinkFromList(i, &mem3.aiSmall[size-2]);
  }else{
    hash = size % N_HASH;
    memsys3UnlinkFromList(i, &mem3.aiHash[hash]);
  }
}

/*
** Link the chunk at mem3.aPool[i] so that is on the list rooted
** at *pRoot.
** 
** 将mem3.aPool[i]对应块链接到列表中
*/    
//将mem3.aPool[i]插入到pRoot指向的链表的头部，更新pRoot指向新插入的这一项。
static void memsys3LinkIntoList(u32 i, u32 *pRoot){
  assert( sqlite3_mutex_held(mem3.mutex) );
  mem3.aPool[i].u.list.next = *pRoot;   //索引号为i的块的下一块索引号设为*pRoot
  mem3.aPool[i].u.list.prev = 0;   //将索引号为i的前一块索引号设置为0
  if( *pRoot ){
  mem3.aPool[*pRoot].u.list.prev = i;//若*pRoot存在，则i的值设为该索引号对应块的前一块
  }
  *pRoot = i;
}

/*
** Link the chunk at index i into either the appropriate
** small chunk list, or into the large chunk hash table.
** 
** 将索引为i的块链接到合适的块列表或者大块hash列表中
*/  
//将第i项插入链表中。
static void memsys3Link(u32 i){
  u32 size, hash;
  assert( sqlite3_mutex_held(mem3.mutex) );
  assert( i>=1 );    
  assert( (mem3.aPool[i-1].u.hdr.size4x & 1)==0 );
  size = mem3.aPool[i-1].u.hdr.size4x/4;  //用来判断是block还是chunk
  assert( size==mem3.aPool[i+size-1].u.hdr.prevSize );
  assert( size>=2 );               //若只是一个block，则终止程序
  if( size <= MX_SMALL ){
    memsys3LinkIntoList(i, &mem3.aiSmall[size-2]);  //如果size小于10，将i块加入小chunk
  }else{
    hash = size % N_HASH;
    memsys3LinkIntoList(i, &mem3.aiHash[hash]);  //将i块加入大chunk
  }
}

/*
** If the STATIC_MEM mutex is not already held, obtain it now. The mutex
** will already be held (obtained by code in malloc.c) if
** sqlite3GlobalConfig.bMemStat is true.
** 
** 如果STATIC_MEM锁没有被获取，则现在获取锁。如果sqlite3GlobalConfig.bMemStat为真的话，互斥锁也会被获取。
*/    
//该函数用于获取互斥锁,通过sqlite3GlobalConfig.bMemstat的值来判断是否已经获取
static void memsys3Enter(void){
  if( sqlite3GlobalConfig.bMemstat==0 && mem3.mutex==0 ){//判断是否已经获得互斥锁
    mem3.mutex = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MEM);//获取互斥锁
  }
  sqlite3_mutex_enter(mem3.mutex);     //加锁
}
static void memsys3Leave(void){
  sqlite3_mutex_leave(mem3.mutex);    //释放锁
}

/*
** Called when we are unable to satisfy an allocation of nBytes.
** 
** 当内存不够时调用
*/   
//分配的内存不足时释放n字节空间。
static void memsys3OutOfMemory(int nByte){
  if( !mem3.alarmBusy ){  //mem3.alarmBusy为假时进行内存回收
    mem3.alarmBusy = 1;  //赋值为1表示进行内存回收
    assert( sqlite3_mutex_held(mem3.mutex) );
    sqlite3_mutex_leave(mem3.mutex); //释放互斥锁
    sqlite3_release_memory(nByte);  //释放n字节内存
    sqlite3_mutex_enter(mem3.mutex);  //加锁
    mem3.alarmBusy = 0;  //回收完毕
  }
}


/*
** Chunk i is a free chunk that has been unlinked.  Adjust its 
** size parameters for check-out and return a pointer to the 
** user portion of the chunk.
** 
** 块i是没有链接的空闲块，调整它的小大，然后返回指向用户部分的块的指针
*/   
//调整第i块的大小为nblock，为用户使用，返回该空间的地址。
static void *memsys3Checkout(u32 i, u32 nBlock){
  u32 x;
  assert( sqlite3_mutex_held(mem3.mutex) );
  assert( i>=1 );  //该块不存在则终止程序
  assert( mem3.aPool[i-1].u.hdr.size4x/4==nBlock ); 
  assert( mem3.aPool[i+nBlock-1].u.hdr.prevSize==nBlock );
  x = mem3.aPool[i-1].u.hdr.size4x;
  mem3.aPool[i-1].u.hdr.size4x = nBlock*4 | 1 | (x&2);  //调整块大小，并置当前chunk为已使用
  mem3.aPool[i+nBlock-1].u.hdr.prevSize = nBlock;
  mem3.aPool[i+nBlock-1].u.hdr.size4x |= 2;  //置前一chunk为已使用
  return &mem3.aPool[i];    //返回一个指向用户使用该块处的指针
}

/*
** Carve a piece off of the end of the mem3.iMaster free chunk.
** Return a pointer to the new allocation.  Or, if the master chunk
** is not large enough, return 0.
** 
** 从mem3.iMaster的尾端取一块空闲的内存。返回指向新分配器的指针。
** 或者，当主块不够大时，返回0。
*/      
//从iMaster开始的大chunk上切下nBlock的chunk供用户使用，返回该空间的地址。
static void *memsys3FromMaster(u32 nBlock){
  assert( sqlite3_mutex_held(mem3.mutex) ); 
  assert( mem3.szMaster>=nBlock );  //主要块的大小若小于nBlock，则终止程序
  if( nBlock>=mem3.szMaster-1 ){   //若nBlock等于该主要块大小，则使用整个主chunk
    /* Use the entire master */
    void *p = memsys3Checkout(mem3.iMaster, mem3.szMaster);
    mem3.iMaster = 0;
    mem3.szMaster = 0;
    mem3.mnMaster = 0;
    return p;
  }else{       //若nBlock小于主chunk大小，则分裂master free chunk，返回尾部地址
    /* Split the master block.  Return the tail. */
    u32 newi, x;
    newi = mem3.iMaster + mem3.szMaster - nBlock; //将多出来的空间赋给newi
    assert( newi > mem3.iMaster+1 ); //除去nBlock大小外的空间小于等于mem3.iMaster，则终止
    mem3.aPool[mem3.iMaster+mem3.szMaster-1].u.hdr.prevSize = nBlock;//分裂出的chunk
    mem3.aPool[mem3.iMaster+mem3.szMaster-1].u.hdr.size4x |= 2;  //置前一块为已使用
    mem3.aPool[newi-1].u.hdr.size4x = nBlock*4 + 1;
    mem3.szMaster -= nBlock;       //当前chunk大小为nBlock
    mem3.aPool[newi-1].u.hdr.prevSize = mem3.szMaster; //剩余部分的前一块大小也即为nBlock
    x = mem3.aPool[mem3.iMaster-1].u.hdr.size4x & 2;
    mem3.aPool[mem3.iMaster-1].u.hdr.size4x = mem3.szMaster*4 | x;
    if( mem3.szMaster < mem3.mnMaster ){ //若当前chunk空间小于块中的最小空间，则将最小空间大小赋值为当前块空间大小
      mem3.mnMaster = mem3.szMaster;
    }
    return (void*)&mem3.aPool[newi];  //返回新空间地址
  }
}

/*
** *pRoot is the head of a list of free chunks of the same size
** or same size hash.  In other words, *pRoot is an entry in either
** mem3.aiSmall[] or mem3.aiHash[].  
** 
** *pRoot是相同大小或相同索引大小的空闲块列表的的头部。
** *pRoot是mem3.aiSmall[]或mem3.aiHash[]的入口。
**
** This routine examines all entries on the given list and tries
** to coalesce each entries with adjacent free chunks.  
** 
** 这个例程检查所有给定列表的入口，并且试着合并相邻块的入口。
**
** If it sees a chunk that is larger than mem3.iMaster, it replaces 
** the current mem3.iMaster with the new larger chunk.  In order for
** this mem3.iMaster replacement to work, the master chunk must be
** linked into the hash tables.  That is not the normal state of
** affairs, of course.  The calling routine must link the master
** chunk before invoking this routine, then must unlink the (possibly
** changed) master chunk once this routine has finished.
** 
** 如果一个块的大小比mem3.iMaster大，则用这个块的大小的值替换mem3.iMaster的值。
** 为了替换成功，主块必须链接到哈希表中。当然，这不是事务的正常状态。
** 在引用这个例程前，调用例程必须链接到主块，然后，在完成后，去掉主块链接。
*/   
//该函数用于合并每一个chunk入口，*pRoot是chunk列表的头指针
//合并相邻chunk块到*pRoot指向的链表中，若块大于当前master，则将其替换。
static void memsys3Merge(u32 *pRoot){
  u32 iNext, prev, size, i, x;

  assert( sqlite3_mutex_held(mem3.mutex) );
  for(i=*pRoot; i>0; i=iNext){       //循环查找chunk列表中的chunk
    iNext = mem3.aPool[i].u.list.next;  //使iNext为aPool[i]的下一个chunk的索引号
    size = mem3.aPool[i-1].u.hdr.size4x; //size存储aPool[i-1]索引号对应块的大小
    assert( (size&1)==0 );
    if( (size&2)==0 ){
      memsys3UnlinkFromList(i, pRoot); 
      assert( i > mem3.aPool[i-1].u.hdr.prevSize );
      prev = i - mem3.aPool[i-1].u.hdr.prevSize;
      if( prev==iNext ){
        iNext = mem3.aPool[prev].u.list.next;
      }
      memsys3Unlink(prev);
      size = i + size/4 - prev;
      x = mem3.aPool[prev-1].u.hdr.size4x & 2;
      mem3.aPool[prev-1].u.hdr.size4x = size*4 | x;
      mem3.aPool[prev+size-1].u.hdr.prevSize = size;
      memsys3Link(prev);
      i = prev;
    }else{
      size /= 4;
    }
    if( size>mem3.szMaster ){
      mem3.iMaster = i;
      mem3.szMaster = size;
    }
  }
}

/*
** Return a block of memory of at least nBytes in size.
** Return NULL if unable.
** 
** 返回一块大小至少为nBytes的的内存块，如果不可用，则返回空。
**
** This function assumes that the necessary mutexes, if any, are
** already held by the caller. Hence "Unsafe".
** 
** 这个函数假设调用者已经获得了必要的锁，所以称为“不安全”。
*/
//给用户分配n字节的空间，返回该空间的地址。
//该函数返回至少n字节大小的block，没有则返回null。该函数假设所有必要的互斥锁都上了，所以不安全
static void *memsys3MallocUnsafe(int nByte){
  u32 i;
  u32 nBlock;
  u32 toFree;

  assert( sqlite3_mutex_held(mem3.mutex) );   //如果不能加锁，则终止程序
  assert( sizeof(Mem3Block)==8 );   //若Mem3Block大小为8，继续往下执行
  if( nByte<=12 ){                //给nBlock赋值
    nBlock = 2;
  }else{
    nBlock = (nByte + 11)/8;
  }
  assert( nBlock>=2 );

  /* STEP 1:
  ** Look for an entry of the correct size in either the small
  ** chunk table or in the large chunk hash table.  This is
  ** successful most of the time (about 9 times out of 10).
  */   
  //首先在小chunk或者大chunk中寻找正确大小块的入口，一般都会成功
  if( nBlock <= MX_SMALL ){        //nBlock小于MX_SMALL，则在小chunk中找
    i = mem3.aiSmall[nBlock-2];
    if( i>0 ){
      memsys3UnlinkFromList(i, &mem3.aiSmall[nBlock-2]);
      return memsys3Checkout(i, nBlock);  //返回找到的满足的chunk
    }
  }else{    //若nBlock大于MX_SMALL，则在大chunk中找
    int hash = nBlock % N_HASH;
    for(i=mem3.aiHash[hash]; i>0; i=mem3.aPool[i].u.list.next){
      if( mem3.aPool[i-1].u.hdr.size4x/4==nBlock ){
        memsys3UnlinkFromList(i, &mem3.aiHash[hash]);
        return memsys3Checkout(i, nBlock);   //返回找到的chunk
      }
    }
  }

  /* STEP 2:
  ** Try to satisfy the allocation by carving a piece off of the end
  ** of the master chunk.  This step usually works if step 1 fails.
  */     
  //尝试从master chunk中分裂出合适的空间，第一步失败才执行
  if( mem3.szMaster>=nBlock ){
    return memsys3FromMaster(nBlock);  //从master chunk中获取chunk
  }


  /* STEP 3:  
  ** Loop through the entire memory pool.  Coalesce adjacent free
  ** chunks.  Recompute the master chunk as the largest free chunk.
  ** Then try again to satisfy the allocation by carving a piece off
  ** of the end of the master chunk.  This step happens very
  ** rarely (we hope!)
  */
  //遍历整个内存池，合并相邻空闲chunk，重新计算主要的chunk大小，
  //再次尝试从master chunk中分裂出满足分配条件的chunk。前面都不行才执行该步骤。
  for(toFree=nBlock*16; toFree<(mem3.nPool*16); toFree *= 2){  //遍历内存池
    memsys3OutOfMemory(toFree);     //不够分配则释放
    if( mem3.iMaster ){               //master chunk存在，将其链接到相应块索引表中        
      memsys3Link(mem3.iMaster);
      mem3.iMaster = 0;
      mem3.szMaster = 0;
    }
    for(i=0; i<N_HASH; i++){
      memsys3Merge(&mem3.aiHash[i]);  //链接相邻空chunk到aiHash中
    }
    for(i=0; i<MX_SMALL-1; i++){
      memsys3Merge(&mem3.aiSmall[i]); //链接相邻空chunk到aiSmall中
    }
    if( mem3.szMaster ){             //当前master chunk不为0，则从索引表中断开
      memsys3Unlink(mem3.iMaster);
      if( mem3.szMaster>=nBlock ){
        return memsys3FromMaster(nBlock); //返回得到的内存空间
      }
    }
  }

  /* If none of the above worked, then we fail. */ 
  return 0;  //若上面三步都失败了，那就失败了，返回0
}

/*
** Free an outstanding memory allocation.   //释放未完成分配的内存
**
** This function assumes that the necessary mutexes, if any, are
** already held by the caller. Hence "Unsafe".
**
** 自由的高效内存分配。此函数假定必为互斥体，如果有的话，已由调用者所有。因此，“不安全”。
*/
//释放内存空间。
static void memsys3FreeUnsafe(void *pOld){//*pOld指向为完成分配的内存空间
  Mem3Block *p = (Mem3Block*)pOld;
  int i;
  u32 size, x;
  assert( sqlite3_mutex_held(mem3.mutex) );
  assert( p>mem3.aPool && p<&mem3.aPool[mem3.nPool] );
  i = p - mem3.aPool;
  assert( (mem3.aPool[i-1].u.hdr.size4x&1)==1 );
  size = mem3.aPool[i-1].u.hdr.size4x/4;
  assert( i+size<=mem3.nPool+1 );
  mem3.aPool[i-1].u.hdr.size4x &= ~1;
  mem3.aPool[i+size-1].u.hdr.prevSize = size;
  mem3.aPool[i+size-1].u.hdr.size4x &= ~2;
  memsys3Link(i);  //将索引号为i的chunk链接到合适的chunk数组中

  /* Try to expand the master using the newly freed chunk */
  //尝试使用释放了的chunk扩大主要的chunk大小
  if( mem3.iMaster ){
    while( (mem3.aPool[mem3.iMaster-1].u.hdr.size4x&2)==0 ){
      size = mem3.aPool[mem3.iMaster-1].u.hdr.prevSize;
      mem3.iMaster -= size;
      mem3.szMaster += size;
      memsys3Unlink(mem3.iMaster);
      x = mem3.aPool[mem3.iMaster-1].u.hdr.size4x & 2;
      mem3.aPool[mem3.iMaster-1].u.hdr.size4x = mem3.szMaster*4 | x;
      mem3.aPool[mem3.iMaster+mem3.szMaster-1].u.hdr.prevSize = mem3.szMaster;
    }
    x = mem3.aPool[mem3.iMaster-1].u.hdr.size4x & 2;
    while( (mem3.aPool[mem3.iMaster+mem3.szMaster-1].u.hdr.size4x&1)==0 ){
      memsys3Unlink(mem3.iMaster+mem3.szMaster);
      mem3.szMaster += mem3.aPool[mem3.iMaster+mem3.szMaster-1].u.hdr.size4x/4;
      mem3.aPool[mem3.iMaster-1].u.hdr.size4x = mem3.szMaster*4 | x;
      mem3.aPool[mem3.iMaster+mem3.szMaster-1].u.hdr.prevSize = mem3.szMaster;
    }
  }
}

/*
** Return the size of an outstanding allocation, in bytes.  The
** size returned omits the 8-byte header overhead.  This only
** works for chunks that are currently checked out.
**
** 以字节的方式返回未完成分配的内存大小，不返回头8byte，节约开销，此函数仅针对刚刚被check out的内存
** 返回一个未分配的大小，以字节为单位。 返回8字节的包头开销大小 仅当前被检查时，该块工作。
*/
//以字节返回未分配内存大小（头8字节除外）。
static int memsys3Size(void *p){
  Mem3Block *pBlock;
  if( p==0 ) return 0;
  pBlock = (Mem3Block*)p;
  assert( (pBlock[-1].u.hdr.size4x&1)!=0 );
  return (pBlock[-1].u.hdr.size4x&~3)*2 - 4;
}

/*
** Round up a request size to the next valid allocation size.
** 
** 聚集请求大小给下一个有效的内存分配大小
*/
//聚集请求大小给下一个有效的内存分配大小。
static int memsys3Roundup(int n){
  if( n<=12 ){
    return 12;
  }else{
    return ((n+11)&~7) - 4;
  }
}

/*
** Allocate nBytes of memory.  
**
** 召集请求大小到下一个有效的分配大小。
*/
//申请分配n字节的内存空间。
static void *memsys3Malloc(int nBytes){//分配n字节的内存
  sqlite3_int64 *p;
  assert( nBytes>0 );          /* malloc.c filters out 0 byte requests *///请求字节为0则终止程序
  memsys3Enter();          //获取共享锁
  p = memsys3MallocUnsafe(nBytes);   //分配内存
  memsys3Leave();    //释放锁
  return (void*)p;    //返回空指针
}

/*
** Free memory.   //释放内存
*/
//释放*pPrior指向的内存空间。
static void memsys3Free(void *pPrior){
  assert( pPrior );
  memsys3Enter();                //加锁
  memsys3FreeUnsafe(pPrior);      //释放内存
  memsys3Leave();              //解锁
}

/*
** Change the size of an existing memory allocation
**
** 改变一个已存在的内存的大小
*/
//重新分配*pPrior指向的内存空间大小为n字节。
static void *memsys3Realloc(void *pPrior, int nBytes){
  int nOld;
  void *p;
  if( pPrior==0 ){              //原来的内存为0，则直接分配n字节
    return sqlite3_malloc(nBytes);
  }
  if( nBytes<=0 ){    
    sqlite3_free(pPrior);  //释放从由sqlite3Malloc获得的内存空间
    return 0;
  }
  nOld = memsys3Size(pPrior);  //获取未完成分配的内存空间
  if( nBytes<=nOld && nBytes>=nOld-128 ){
    return pPrior;
  }
  memsys3Enter();              
  p = memsys3MallocUnsafe(nBytes);     //申请n字节的内存空间
  if( p ){
    if( nOld<nBytes ){
      memcpy(p, pPrior, nOld);
    }else{
      memcpy(p, pPrior, nBytes);
    }
    memsys3FreeUnsafe(pPrior);
  }
  memsys3Leave();
  return p;
}

/*
** Initialize this module. 
**
** 初始化该模块
*/   
static int memsys3Init(void *NotUsed){
  UNUSED_PARAMETER(NotUsed);
  if( !sqlite3GlobalConfig.pHeap ){
    return SQLITE_ERROR;
  }

  /* Store a pointer to the memory block in global structure mem3. */
  //在结构体mem3中存储一个指向该内存块的指针
  assert( sizeof(Mem3Block)==8 ); //若该内存块大小为8字节，继续执行
  mem3.aPool = (Mem3Block *)sqlite3GlobalConfig.pHeap;
  mem3.nPool = (sqlite3GlobalConfig.nHeap / sizeof(Mem3Block)) - 2;

  /* Initialize the master block. */  //初始化master chunk
  mem3.szMaster = mem3.nPool;
  mem3.mnMaster = mem3.szMaster;
  mem3.iMaster = 1;
  mem3.aPool[0].u.hdr.size4x = (mem3.szMaster<<2) + 2;
  mem3.aPool[mem3.nPool].u.hdr.prevSize = mem3.nPool;
  mem3.aPool[mem3.nPool].u.hdr.size4x = 1;

  return SQLITE_OK;
}

/*
** Deinitialize this module.  
**
** 取消该模块的初始化设置
*/
static void memsys3Shutdown(void *NotUsed){
  UNUSED_PARAMETER(NotUsed);
  mem3.mutex = 0;
  return;
}

/*
** Open the file indicated and write a log of all unfreed memory 
** allocations into that log.
**
** 将所有内存分配的日志写入该文件
*/ 
//将该内存分配器进行的操作写入日志文件。
void sqlite3Memsys3Dump(const char *zFilename){
#ifdef SQLITE_DEBUG
  FILE *out;
  u32 i, j;
  u32 size;
 if( zFilename==0 || zFilename[0]==0 ){//若文件夹不存在或文件夹中没有内容，则输出设为stdout
    out = stdout;
  }else{   
    out = fopen(zFilename, "w");   //以可写的方式打开该文件
    if( out==0 ){          //若没有输出内容，将提示信息写到文件中     
      fprintf(stderr, "** Unable to output memory debug output log: %s **\n",
                      zFilename);
      return;
    }
  }
  memsys3Enter();
  fprintf(out, "CHUNKS:\n");  //打印提示信息
  for(i=1; i<=mem3.nPool; i+=size/4){        //遍历可分配内存块
    size = mem3.aPool[i-1].u.hdr.size4x;
    if( size/4<=1 ){
      fprintf(out, "%p size error\n", &mem3.aPool[i]); //内存空间错误
      assert( 0 );
      break;
    }
    if( (size&1)==0 && mem3.aPool[i+size/4-1].u.hdr.prevSize!=size/4 ){
      fprintf(out, "%p tail size does not match\n", &mem3.aPool[i]);  //结尾块大小不正确
      assert( 0 );
      break;
    }
    if( ((mem3.aPool[i+size/4-1].u.hdr.size4x&2)>>1)!=(size&1) ){
      fprintf(out, "%p tail checkout bit is incorrect\n", &mem3.aPool[i]); //结尾块不正确
      assert( 0 );
      break;
    }
    if( size&1 ){
      fprintf(out, "%p %6d bytes checked out\n", &mem3.aPool[i], (size/4)*8-8); //检查完
    }else{
      fprintf(out, "%p %6d bytes free%s\n", &mem3.aPool[i], (size/4)*8-8,
                  i==mem3.iMaster ? " **master**" : "");
    }
  }
  for(i=0; i<MX_SMALL-1; i++){
    if( mem3.aiSmall[i]==0 ) continue;
    fprintf(out, "small(%2d):", i);
    for(j = mem3.aiSmall[i]; j>0; j=mem3.aPool[j].u.list.next){
      fprintf(out, " %p(%d)", &mem3.aPool[j],
              (mem3.aPool[j-1].u.hdr.size4x/4)*8-8);
    }
    fprintf(out, "\n"); 
  }
  for(i=0; i<N_HASH; i++){
    if( mem3.aiHash[i]==0 ) continue;
    fprintf(out, "hash(%2d):", i);
    for(j = mem3.aiHash[i]; j>0; j=mem3.aPool[j].u.list.next){
      fprintf(out, " %p(%d)", &mem3.aPool[j],
              (mem3.aPool[j-1].u.hdr.size4x/4)*8-8);
    }
    fprintf(out, "\n"); 
  }
  fprintf(out, "master=%d\n", mem3.iMaster);
  fprintf(out, "nowUsed=%d\n", mem3.nPool*8 - mem3.szMaster*8);
  fprintf(out, "mxUsed=%d\n", mem3.nPool*8 - mem3.mnMaster*8);
  sqlite3_mutex_leave(mem3.mutex);
  if( out==stdout ){
    fflush(stdout);
  }else{
    fclose(out);
  }
#else
  UNUSED_PARAMETER(zFilename);
#endif
}

/*
** This routine is the only routine in this file with external 
** linkage.
** 
** 该线程是这个文件中唯一一个用于外部链接的函数
** 
** Populate the low-level memory allocation function pointers in
** sqlite3GlobalConfig.m with pointers to the routines in this file. The
** arguments specify the block of memory to manage.
** 
** 用该文件中指向这个例程的指针填充代替内存分配器指针。参数指定了要操作的内存块。
**
** This routine is only called by sqlite3_config(), and therefore
** is not required to be threadsafe (it is not).
**
** 该线程仅被sqlite3_config()调用，因此不需要保证线程安全
** 低级别的内存分配函数指针与指针在sqlite3GlobalConfig.m中的例程。
** 该参数指定的内存管理。这个程序被sqlite3_config()调用并不需要线程安全（这不安全）
*/ 
//配置参数
const sqlite3_mem_methods *sqlite3MemGetMemsys3(void){
  static const sqlite3_mem_methods mempoolMethods = {
     memsys3Malloc,
     memsys3Free,
     memsys3Realloc,
     memsys3Size,
     memsys3Roundup,
     memsys3Init,
     memsys3Shutdown,
     0
  };
  return &mempoolMethods;
}

#endif /* SQLITE_ENABLE_MEMSYS3 */
