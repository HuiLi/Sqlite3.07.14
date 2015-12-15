/*
** 2004 April 6
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file implements a external (disk-based) database using BTrees.
** See the header comment on "btreeInt.h" for additional information.
** Including a description of file format and an overview of operation.
** 这个文件使用来实现外部(基于磁盘)数据库。在“btreeInt.h”文件中查看声明的附加信息。
** 包括文件格式描述和操作概述。
*/
/*
【潘光珍】
这个文件使用来实现外部(基于磁盘)数据库在"btreeInt.h"文件里附加一些需要调用的方法。 
包括文件格式的描述和操作的概述。
*/
#include "btreeInt.h"

/*
** The header string that appears at the beginning of every
** SQLite database.
** "btreeInt.h"这个头文件在所有的SQLite数据库的开头中都会出现。
*/
/*
【潘光珍】定义一个常字符串，然后将"btreeInt.h"里的定义的一个SQLite头文件的值赋给这个常字符串

*/
static const char zMagicHeader[] = SQLITE_FILE_HEADER;

/*
** Set this global variable to 1 to enable tracing using the TRACE
** macro.
** 设置全局变量，值为1可以用宏TRACE跟踪
*/
/*
【潘光珍】这是一个宏指令，它将全局变量设置为1，如果是0，就定义一个整形的追踪树，
并赋值为1；然后定义一个追踪函数，如果sqlite3BtreeTrace为真，就进行追踪。否则一开始就为真的话，就进行追踪。
*/
#if 0
int sqlite3BtreeTrace=1;  /* True to enable tracing *//* 逻辑值为真表示可以追踪 *///【潘光珍】如果是true，则追踪
# define TRACE(X)  if(sqlite3BtreeTrace){printf X;fflush(stdout);}
#else
# define TRACE(X)
#endif

/*
** Extract a 2-byte big-endian integer from an array of unsigned bytes.
** But if the value is zero, make it 65536.
**
** This routine is used to extract the "offset to cell content area" value
** from the header of a btree page.  If the page size is 65536 and the page
** is empty, the offset should be 65536, but the 2-byte value stores zero.
** This routine makes the necessary adjustment to 65536.
*/
/*
**从无符号字节数组中取出一个2字节的大端整数。但是，如果该值为零，使它等于65536。
此程序用来从B树页面的标题中提取“偏移单元格的内容区”的值。
如果页面大小是65536和页是空的，偏移应该是65536，但2个字节的值存储为零。
这个程序进行必要的调整，调整到65536。
*/
/*
【潘光珍】**从一个无符号字节数组中提取保存cell的地址的大端整数，但如果这个值为0，就将它赋值为65536。
这个程序通常用来从一个btree页的头部(首部)中提取“偏移格cell内容区域”。如果页的大小为65536和页
的大小为空，偏移的大小应该为65536，但是保存cell的地址的值存储为0。这个程序进行必要调整到65536。
*/

#define get2byteNotZero(X)  (((((int)get2byte(X))-1)&0xffff)+1)

#ifndef SQLITE_OMIT_SHARED_CACHE
/*
** A list of BtShared objects that are eligible for participation
** in shared cache.  This variable has file scope during normal builds,
** but the test harness needs to access it so we make it global for 
** test builds.
**
** Access to this variable is protected by SQLITE_MUTEX_STATIC_MASTER.
*/
/*
一系列BtShared对象有权限访问共享缓存。这个变量在创建时有一个文件作用域，但测试工具需要访问它，
所以为了测试我们把它变为全局变量。访问这个由SQLITE_MUTEX_STATIC_MASTER保护的变量。
*/
/*
【潘光珍】** btree结构中最主要包含一个BtShared结构，该结构有权限访问共享缓存，这个变量构建时有一个文件作用域，
但是测试工具需要访问这个变量，因此我们为了测试就把这个变量变成全局变量。访问这个变量时，会受SQLITE_MUTEX_STATIC_MASTER保护
*/
#ifdef SQLITE_TEST
BtShared *SQLITE_WSD sqlite3SharedCacheList = 0;
#else
static BtShared *SQLITE_WSD sqlite3SharedCacheList = 0;
#endif
#endif /* SQLITE_OMIT_SHARED_CACHE */

#ifndef SQLITE_OMIT_SHARED_CACHE
/*
** Enable or disable the shared pager and schema features.
**
** This routine has no effect on existing database connections.
** The shared cache setting effects only future calls to
** sqlite3_open(), sqlite3_open16(), or sqlite3_open_v2().
*/
/*
启用或禁用共享的页和模式的特点。这个程序对现有的数据库连接没有影响。
共享缓存设置仅影响将来调用sqlite3_open（），sqlite3_open16（），或sqlite3_open_v2（）。
*/
/*
【潘光珍】**启动或禁止共享页和模式特征，这个程序没有影响到现存的数据库的连接，
共享缓存设置只影响未来的调用sqlite3_open(), sqlite3_open16(), or sqlite3_open_v2()

*/
int sqlite3_enable_shared_cache(int enable){
  sqlite3GlobalConfig.sharedCacheEnabled = enable;
  return SQLITE_OK;
}
#endif



#ifdef SQLITE_OMIT_SHARED_CACHE
  /*
  ** The functions querySharedCacheTableLock(), setSharedCacheTableLock(),
  ** and clearAllSharedCacheTableLocks()
  ** manipulate entries in the BtShared.pLock linked list used to store
  ** shared-cache table level locks. If the library is compiled with the
  ** shared-cache feature disabled, then there is only ever one user
  ** of each BtShared structure and so this locking is not necessary. 
  ** So define the lock related functions as no-ops.
  */
  /*
函数querySharedCacheTableLock（），setSharedCacheTableLock（）和clearAllSharedCacheTableLocks（）
操纵链表BtShared.pLock中的记录，这个链表存储共享缓存表级锁。如果库在共享缓存功能禁用的情况下编译，
那么每个BtShared结构就永远只能有一个用户，因此该锁定是没有必要的。
所以定义锁相关的功能为空操作。
*/
  /*
【潘光珍】分别定义了函数querySharedCacheTableLock（）：链表存储共享缓存表级锁，
setSharedCacheTableLock（）：设置共享缓存表锁，
clearAllSharedCacheTableLocks（）：清空所有共享缓存表锁。
通过操纵链表BtShared.pLock中的记录，这个链表存储共享缓存表级锁。如果库在共享缓存功能禁用的情况下编译，
那么每个BtShared结构就永远只能有一个用户，因此该锁定是没有必要的。所以定义锁相关的功能为空操作。
*/
  #define querySharedCacheTableLock(a,b,c) SQLITE_OK  //查询共享缓存表锁
  #define setSharedCacheTableLock(a,b,c) SQLITE_OK    //设置共享缓存表锁
  #define clearAllSharedCacheTableLocks(a)            //删除所有共享缓存表锁
  #define downgradeAllSharedCacheTableLocks(a)        //降低共享缓存表锁的优先级
  #define hasSharedCacheTableLock(a,b,c,d) 1          //判断是否有共享缓存表锁，1代表有锁
  #define hasReadConflicts(a, b) 0                    //有读冲突
#endif

#ifndef SQLITE_OMIT_SHARED_CACHE

#ifdef SQLITE_DEBUG
/*
**** This function is only used as part of an assert() statement. ***
**
** Check to see if pBtree holds the required locks to read or write to the 
** table with root page iRoot.   Return 1 if it does and 0 if not.
**
** For example, when writing to a table with root-page iRoot via 
** Btree connection pBtree:
**
**    assert( hasSharedCacheTableLock(pBtree, iRoot, 0, WRITE_LOCK) );
**
** When writing to an index that resides in a sharable database, the 
** caller should have first obtained a lock specifying the root page of
** the corresponding table. This makes things a bit more complicated,
** as this module treats each table as a separate structure. To determine
** the table corresponding to the index being written, this
** function has to search through the database schema.
**
** Instead of a lock on the table/index rooted at page iRoot, the caller may
** hold a write-lock on the schema table (root page 1). This is also
** acceptable.
*/
/*
此功能仅作为assert（）语句的一部分。 检查pBtree(Btree持有锁的句柄)是否持有所需的锁来读取或写入
表的根页iRoot。如果有则返回1，否则返回0。例如，通过B树连接pBtree，写入表根页iRoot：
assert（hasSharedCacheTableLock（pBtree，iRoot，0，WRITE_LOCK））;
写驻留在共享数据库索引时，该主调应该先获得一个锁指定的根页相应的表。因为该模块将每个表看作
为一个独立的结构，这使事情变得有点复杂。为了确定对应于该索引的哪个表被写入，这个函数必须搜遍
数据库架构。主调可能持有架构表中的一个写锁，而不是根植在页面iRoot上的表或者索引上的锁。
这也是可以接受的。
*/
/*
【潘光珍】这个函数仅仅是作为一个assert()语句的一部分。检查pBtree拥有所需的锁读或写iRoot与根表页面。
如果是真的则返回1，否则返回0。
例如,当写入表根页iRoot通过Btree连接pBtree:
assert(hasSharedCacheTableLock(WRITE_LOCK pBtree iRoot 0));
当编写一个索引,驻留在共享数据库,调用者应该首先获得一个锁指定相应的根页表。
这使得事情更加复杂,因为这个模块对每个表作为一个单独的结构。
确定写入表的索引,这个函数搜索数据库模式。而不是根植在页面iRoot上的表或者索引上的锁,调用者
在进行一个写锁模式表(根1页)。这也是可以接受的。
*/
static int hasSharedCacheTableLock(
  Btree *pBtree,         /* Handle that must hold lock *这个句柄要持有锁*/ /*【潘光珍】b树页必须持有锁*/
  Pgno iRoot,            /* Root page of b-tree  *B—树的根页*/   /*【潘光珍】此Btree的根页页号*/
  int isIndex,           /* True if iRoot is the root of an index b-tree *如果iRoot是Brtee索引的根页则为true*/ /*【潘光珍】索引B树的根页*/
  int eLockType          /* Required lock type (READ_LOCK or WRITE_LOCK) 需要锁类型*/  /*【潘光珍】需要锁类型（读锁或是写锁）*/
){
  Schema *pSchema = (Schema *)pBtree->pBt->pSchema;
  Pgno iTab = 0;
  BtLock *pLock;

  /* If this database is not shareable, or if the client is reading
  ** and has the read-uncommitted flag set, then no lock is required. 
  ** Return true immediately.
  */
  /*
  如果该数据库是非共享的，或者如果客户端正在读并且具有读未提交的标志设置，则不需要锁。
  立即返回true。
  */
   /*
  如果这个数据库不是可共享的,或者客户端在读,读未提交的标记设置,然后不需要锁。立即返回true。
  */
  
  if( (pBtree->sharable==0)
   || (eLockType==READ_LOCK && (pBtree->db->flags & SQLITE_ReadUncommitted))
  ){
    return 1;
  }

  /* If the client is reading  or writing an index and the schema is
  ** not loaded, then it is too difficult to actually check to see if
  ** the correct locks are held.  So do not bother - just return true.
  ** This case does not come up very often anyhow.
  */
  /*如果用户正在读取或写入索引的时候(isIndex)，模式没有加载(!pSchema)，
  此时去判断pBtree是否持有正确的锁非常困难((pSchema->flags&DB_SchemaLoaded)==0)。
  所以，不要困惑，仅仅返回true。  幸好，此情况出现的很少。*/
  /*
【潘光珍】如果客户端是读和写一个索引模式并不是加载,那么它实际上是很难检查正确的锁。因此不要担心,返回true就行。
这种情况并不经常出现。
  */
  if( isIndex && (!pSchema || (pSchema->flags&DB_SchemaLoaded)==0) ){
    return 1; //返回真
  }

  /* Figure out the root-page that the lock should be held on. For table
  ** b-trees, this is just the root page of the b-tree being read or
  ** written. For index b-trees, it is the root page of the associated
  ** table.  */
  /*找出根页应该持有的锁。对于表B树， B树的根页被读取或写入(else iRoot)。索引B树，它是相关联的根页表。*/
  /*
  ** 计算出应该持有锁的根页，对表B树（表是B+-tree），这只是正在被读或者写的B树的根页。
  ** 对于索引B树，它是相对应表的根页。
  */
  /*
   【潘光珍】找出根页应该持有的锁。对于表b树,这只是b树的根页被读或写。索引b树,它的根页相关表。
  */
  if( isIndex ){
    HashElem *p;
    for(p=sqliteHashFirst(&pSchema->idxHash); p; p=sqliteHashNext(p)){
      Index *pIdx = (Index *)sqliteHashData(p);
      if( pIdx->tnum==(int)iRoot ){
        iTab = pIdx->pTable->tnum;
      }
    }
  }else{
    iTab = iRoot;
  }

  /* Search for the required lock. Either a write-lock on root-page iTab, a 
  ** write-lock on the schema table, or (if the client is reading) a
  ** read-lock on iTab will suffice. Return 1 if any of these are found.  */
  /*搜索所需的锁(pLock)。在根页iTAB上的写锁(pLock->eLock==WRITE_LOCK) ，在架构表上的写锁( pLock->iTable==1)，或（如果客户正在读）ITAB上的读锁
  就足够了(pLock->eLock>=eLockType,eLockType为所需要的锁)。如果上述情况出现就返回1。
  */
  /*
  【潘光珍】寻找所需的锁（pLock）。在根页iTab上的写锁(pLock->eLock==WRITE_LOCK) ，
  在架构表上的写锁( pLock->iTable==1)，或（如果客户正在读）iTab上的读锁
  就足够了(pLock->eLock>=eLockType,eLockType为所需要的锁)。如果这些发现返回1。
  */
  for(pLock=pBtree->pBt->pLock; pLock; pLock=pLock->pNext){
    if( pLock->pBtree==pBtree 
     && (pLock->iTable==iTab || (pLock->eLock==WRITE_LOCK && pLock->iTable==1))
     && pLock->eLock>=eLockType 
    ){
      return 1;
    }
  }

  /* Failed to find the required lock. 未查询到相应的锁则返回0 *//*【潘光珍】没有找到所需的锁,则返回0*/
  return 0;
}
#endif /* SQLITE_DEBUG */  //调试程序SQLITE_DEBUG 

#ifdef SQLITE_DEBUG
/*
**** This function may be used as part of assert() statements only. ****
**
** Return true if it would be illegal for pBtree to write into the
** table or index rooted at iRoot because other shared connections are
** simultaneously reading that same table or index.
**
** It is illegal for pBtree to write if some other Btree object that
** shares the same BtShared object is currently reading or writing
** the iRoot table.  Except, if the other Btree object has the
** read-uncommitted flag set, then it is OK for the other object to
** have a read cursor.
**
** For example, before writing to any part of the table or index
** rooted at page iRoot, one should call:
**
**    assert( !hasReadConflicts(pBtree, iRoot) );
*/
/*
** 如果因为其他的共享连接同时读取相同的表或者索引，导致pBtree写进去的
** 表或根iRoot上的索引是非法的，则返回true.
如果一些其他的B树对象共享相同的BtShared对象，BtShared对象正在读取或写入的iRoot表
(p->pgnoRoot==iRoot )，此时pBtree的写入是非法的。除外，如果其余的B树对象具有读未提交标志设置
(SQLITE_ReadUncommitted)，则它确保其他对象有一个读指针。
例如，在写根页上的表或索引之前，应该调用：
	assert（！hasReadConflicts（pBtree，iRoot））;
*/
/*
【潘光珍】可以使用这个函数只assert()语句的一部分。如果是因为其他共享连接同时读取同一个表或索引，
导致非法的pBtree写进去的表或根iRoot上的索引，如果一些其他的B树对象共享相同的BtShared对象，
BtShared对象正在读取或写入的iRoot表(p->pgnoRoot==iRoot )，此时pBtree的写入是非法的。
除外，如果有其他Btree对象读未提交标记集,那么它可以为其他对象有一个读指针，返回true。
例如,在写根页上的表或索引的一部分根页面iRoot之前，
应该调用：assert( !hasReadConflicts(pBtree, iRoot) );
*/
static int hasReadConflicts(Btree *pBtree, Pgno iRoot){
  BtCursor *p;
  for(p=pBtree->pBt->pCursor; p; p=p->pNext){
    if( p->pgnoRoot==iRoot 
     && p->pBtree!=pBtree
     && 0==(p->pBtree->db->flags & SQLITE_ReadUncommitted)
    ){
      return 1;
    }
  }
  return 0;
}
#endif    /* #ifdef SQLITE_DEBUG */

/*
** Query to see if Btree handle p may obtain a lock of type eLock 
** (READ_LOCK or WRITE_LOCK) on the table with root-page iTab. Return
** SQLITE_OK if the lock may be obtained (by calling
** setSharedCacheTableLock()), or SQLITE_LOCKED if not.
*/
/*
查询，判断B树句柄p是否能在iTab根页的表上获取eLock类型的锁（READ_LOCK或WRITE_LOCK）(eLock==READ_LOCK || eLock==WRITE_LOCK )。
如果通过调用 setSharedCacheTableLock（），可以获得锁，返回SQLITE_OK。否则返回SQLITE_LOCKED。
**查看Btree句柄p是否在具有根页iTab的表上获得了eLock类型（读锁或写锁）的锁。
** 如果通过调用setSharedCacheTableLock()获得了锁，返回SQLITE_OK,否则返回SQLITE_LOCKED.
*/
/*
【潘光珍】查询，判断B树句柄p是否能在iTab根页的表上获取eLock类型的锁（读锁或者是写锁）
如果通过调用 setSharedCacheTableLock（），可以获得锁，返回SQLITE_OK。否则返回SQLITE_LOCKED。
*/
static int querySharedCacheTableLock(Btree *p, Pgno iTab, u8 eLock){//定义一个查询共享缓存表锁的函数
  BtShared *pBt = p->pBt;
  BtLock *pIter;    //pIterB树上的锁指针变量

  assert( sqlite3BtreeHoldsMutex(p) );  
  assert( eLock==READ_LOCK || eLock==WRITE_LOCK );
  assert( p->db!=0 );
  assert( !(p->db->flags&SQLITE_ReadUncommitted)||eLock==WRITE_LOCK||iTab==1 );
  
  /* If requesting a write-lock, then the Btree must have an open write
  ** transaction on this file. And, obviously, for this to be so there 
  ** must be an open write transaction on the file itself.
  ** 如果需要一个写锁，那么B树必须有一个开放的写事务。显然，为了达到这种效果
  ** 文件本身必须有一个开放的写事务。
  */
  /*
  【潘光珍】如果请求一个写锁,那么Btree必须有一个在这个文件打开写事务。
  很明显,这是必须有一个开放的写事务文件本身。  */
  assert( eLock==READ_LOCK || (p==pBt->pWriter && p->inTrans==TRANS_WRITE) );
  assert( eLock==READ_LOCK || pBt->inTransaction==TRANS_WRITE );
  
  /* This routine is a no-op if the shared-cache is not enabled */
  /*如果未启用共享缓存，这个程序则是一个空操作*/ /*【潘光珍】如果没有启用共享缓存，这个程序是一个空操作*/
  if( !p->sharable ){    //获得写锁返回SQLITE_OK
    return SQLITE_OK;
  }

  /* If some other connection is holding an exclusive lock, the
  ** requested lock may not be obtained.
  ** 如果一些其他的连接正在持有互斥锁(pBt->btsFlags & BTS_EXCLUSIVE)!=0,那么无法获得所请求的锁。
  */
  /*
【潘光珍】如果其他连接持有排它锁,可能不会获得所请求的锁。  */
  if( pBt->pWriter!=p && (pBt->btsFlags & BTS_EXCLUSIVE)!=0 ){
    sqlite3ConnectionBlocked(p->db, pBt->pWriter->db);
    return SQLITE_LOCKED_SHAREDCACHE;
  }

  for(pIter=pBt->pLock; pIter; pIter=pIter->pNext){
    /* The condition (pIter->eLock!=eLock) in the following if(...) 
    ** statement is a simplification of:
    **
    **   (eLock==WRITE_LOCK || pIter->eLock==WRITE_LOCK)
    **
    ** since we know that if eLock==WRITE_LOCK, then no other connection
    ** may hold a WRITE_LOCK on any table in this file (since there can
    ** only be a single writer).
    */
    /*
	** 条件(pIter->eLock!=eLock)是if语句中(eLock==WRITE_LOCK || pIter->eLock==WRITE_LOCK)的简化。
	** 因为我们知道，如果eLock== WRITE_LOCK，则没有其他连接可能持有这个文件的任何表的WRITE_LOCK
	**（因为只有一个写进程）。
	*/
	  /*
	【潘光珍】在下面的条件（pIter-> eLock！= eLock）如果（...）   
    语句是一个简化：        
    （eLock== WRITE_LOCK|| pIter-> eLock== WRITE_LOCK），
    因为我们知道，如果eLock== WRITE_LOCK，然后没有其他连接
	可能持有对这个文件中的任何表的WRITE_LOCK（因为只有一个写进程）。
    	*/
    assert( pIter->eLock==READ_LOCK || pIter->eLock==WRITE_LOCK );
    assert( eLock==READ_LOCK || pIter->pBtree==p || pIter->eLock==READ_LOCK);
    if( pIter->pBtree!=p && pIter->iTable==iTab && pIter->eLock!=eLock ){
      sqlite3ConnectionBlocked(p->db, pIter->pBtree->db);
      if( eLock==WRITE_LOCK ){
        assert( p==pBt->pWriter );
        pBt->btsFlags |= BTS_PENDING;
      }
      return SQLITE_LOCKED_SHAREDCACHE; //返回共享缓存锁
    }
  }
  return SQLITE_OK;
}
#endif /* !SQLITE_OMIT_SHARED_CACHE */

#ifndef SQLITE_OMIT_SHARED_CACHE
/*
** Add a lock on the table with root-page iTable to the shared-btree used
** by Btree handle p. Parameter eLock must be either READ_LOCK or 
** WRITE_LOCK.
**
** This function assumes the following:
**
**   (a) The specified Btree object p is connected to a sharable
**       database (one with the BtShared.sharable flag set), and
**
**   (b) No other Btree objects hold a lock that conflicts
**       with the requested lock (i.e. querySharedCacheTableLock() has
**       already been called and returned SQLITE_OK).
**
** SQLITE_OK is returned if the lock is added successfully. SQLITE_NOMEM 
** is returned if a malloc attempt fails.
** 通过B树句柄p在根页iTable的表上添加锁到共享B树上。参数eLock必须是READ_LOCK或 WRITE_LOCK。
** 此功能假定如下：
**  (a)指定的B树对象p被连接到一个可共享数据库（一个与BtShare可共享的标志设置），以及
**  (b)没有其他B树对象持有与所请求的锁相冲突的锁（例如querySharedCacheTableLock（）
**     已经被调用并且返回SQLITE_OK）
** 如果成功的添加了锁，则返回SQLITE_OK，如果内存分配失败，则返回SQLITE_NOMEM.
*/
/*
通过B树句柄p在根页iTable的表上添加锁到共享B树上。 参数eLock必须是READ_LOCK或 WRITE_LOCK。
此功能假定：
（一）指定的B树对象p被连接到一个可共享数据库（一个与BtShare可共享的标志设置），以及
（二）没有其他B树对象持有与所请求的锁相冲突的锁（例如querySharedCacheTableLock（）
已经被调用并且返回SQLITE_OK）。如果锁成功添加返回SQLITE_OK。如果malloc失败则返回SQLITE_NOMEM。
*/
/*
【潘光珍】通过B树句柄p在根页iTable的表上添加锁到共享B树上。 
参数eLock必须是READ_LOCK或 WRITE_LOCK。
这个函数假设如下:
（a）用来指定B树对象p是连接到一个共享数据库(一个BtShared。设置共享标志)
（b）没有其他B树对象持有的锁与请求的锁冲突(即querySharedCacheTableLock()
已经被调用并返回SQLITE_OK)。
如果锁添加成功则返回SQLITE_OK。如果malloc尝试失败则返回SQLITE_NOMEM。
*/
static int setSharedCacheTableLock(Btree *p, Pgno iTable, u8 eLock){//设置共享缓存表锁的函数
  BtShared *pBt = p->pBt;
  BtLock *pLock = 0;
  BtLock *pIter;

  assert( sqlite3BtreeHoldsMutex(p) );
  assert( eLock==READ_LOCK || eLock==WRITE_LOCK );
  assert( p->db!=0 );

  /* A connection with the read-uncommitted flag set will never try to
  ** obtain a read-lock using this function. The only read-lock obtained
  ** by a connection in read-uncommitted mode is on the sqlite_master 
  ** table, and that lock is obtained in BtreeBeginTrans(). 
  **带有读未提交标记设置的连接不能尝试用这个函数获取读锁。在SQLITE_MASTER表中读未提交模式下
  ** 才能获得只读锁。并且在BtreeBeginTrans（）中获得锁。*/
  /*
  有读未提交标志的连接不会通过该功能获得读锁。在SQLITE_MASTER表中读未提交模式下才能获得只读锁。
  并且在BtreeBeginTrans（）中获得锁。
  */
  /*
  【潘光珍】有读未提交标志的永远不会尝试使用这个函数获得读锁。唯一通过连接见sqlite_master表读未提交模式,
  才获得锁在BtreeBeginTrans()。
  */
  assert( 0==(p->db->flags&SQLITE_ReadUncommitted) || eLock==WRITE_LOCK );

  /* This function should only be called on a sharable b-tree after it 
  ** has been determined that no other b-tree holds a conflicting lock.  
  ** 在没有其他的B树持有一个冲突的锁之后，才能在一个共享的B树上调用这个函数。
  */
  /*
 【潘光珍】在确定没有其他B树持有冲突的锁后，这个函数只能调用一个可分享的B树。
	  */
  assert( p->sharable );//指向共享的b树
  assert( SQLITE_OK==querySharedCacheTableLock(p, iTable, eLock) );/*有锁*/

  /* First search the list for an existing lock on this table. */
  /*首先搜索在表上已存在的锁列表   */
   /*
  【潘光珍】优先搜索现有的锁在这个表的列表上。
    */
  for(pIter=pBt->pLock; pIter; pIter=pIter->pNext){
    if( pIter->iTable==iTable && pIter->pBtree==p ){
      pLock = pIter;
      break;
    }
  }
    /* If the above search did not find a BtLock struct associating Btree p
  ** with table iTable, allocate one and link it into the list.
  */
  /*
  ** 如果上面的搜索没有找到( !pLock)BtLock结构关联的在表iTable上的B树p，那么就分配
  ** 一个( pLock = (BtLock *)sqlite3MallocZero(sizeof(BtLock)); )并把它链接到列表中。
  */
  
  if( !pLock ){ //【潘光珍】如果在表iTable上的B树p没有找到关联的锁
    pLock = (BtLock *)sqlite3MallocZero(sizeof(BtLock));//【潘光珍】分配锁并把它链接到列表中
    if( !pLock ){
      return SQLITE_NOMEM;
    }
    pLock->iTable = iTable;
    pLock->pBtree = p;
    pLock->pNext = pBt->pLock;
    pBt->pLock = pLock;
  }

  /* Set the BtLock.eLock variable to the maximum of the current lock
  ** and the requested lock. This means if a write-lock was already held
  ** and a read-lock requested, we don't incorrectly downgrade the lock.
  ** 将BtLock.eLock变量设置为当前的锁与所请求的锁的最大值。意思是如果已经持有一个写锁
  ** 并且请求一个读锁，我们将降低这个锁的级别。
  */
  /*将BtLock.eLock变量设置为当前的锁与所请求的锁的最大 值。这意味着，
  如果已经持有一个写锁  和请求一个读锁，我们正确地降级锁。*/
   /*
【潘光珍】设置BtLock.eLock变量为当前的锁与所请求的锁的最大值
这意味着如果已经持有一个写锁和一个请求读锁，我们适当地降级锁。
  */
  assert( WRITE_LOCK>READ_LOCK );
  if( eLock>pLock->eLock ){
    pLock->eLock = eLock;
  }

  return SQLITE_OK;
}
#endif /* !SQLITE_OMIT_SHARED_CACHE */

#ifndef SQLITE_OMIT_SHARED_CACHE
/*
** Release all the table locks (locks obtained via calls to
** the setSharedCacheTableLock() procedure) held by Btree object p.
**
** This function assumes that Btree p has an open read or write 
** transaction. If it does not, then the BTS_PENDING flag
** may be incorrectly cleared.
*/
/*
释放所有B树对象P所持有的表锁（通过调用setSharedCacheTableLock（）
方法获得的锁）。此函数假定B树P有一个开放的读或写操作事务。
如果没有，则BTS_PENDING标志可能被错误地清除。pBt->btsFlags &= ~(BTS_EXCLUSIVE|BTS_PENDING);
*/
/*
【潘光珍】释放所有的表锁(锁的获得是通过调用setSharedCacheTableLock()的过程)Btree对象持有的p。
此函数假定B树P有一个开放的读或写操作事务。如果没有，那么BTS_PENDING标志可能被不正确地清除。
*/
static void clearAllSharedCacheTableLocks(Btree *p){
  BtShared *pBt = p->pBt;
  BtLock **ppIter = &pBt->pLock;

  assert( sqlite3BtreeHoldsMutex(p) );
  assert( p->sharable || 0==*ppIter );
  assert( p->inTrans>0 );

  while( *ppIter ){
    BtLock *pLock = *ppIter;
    assert( (pBt->btsFlags & BTS_EXCLUSIVE)==0 || pBt->pWriter==pLock->pBtree );
    assert( pLock->pBtree->inTrans>=pLock->eLock );
    if( pLock->pBtree==p ){
      *ppIter = pLock->pNext;
      assert( pLock->iTable!=1 || pLock==&p->lock );
      if( pLock->iTable!=1 ){
        sqlite3_free(pLock);
      }
    }else{
      ppIter = &pLock->pNext;
    }
  }

  assert( (pBt->btsFlags & BTS_PENDING)==0 || pBt->pWriter );
  if( pBt->pWriter==p ){
    pBt->pWriter = 0;
    pBt->btsFlags &= ~(BTS_EXCLUSIVE|BTS_PENDING);
  }else if( pBt->nTransaction==2 ){
    /* This function is called when Btree p is concluding its 
    ** transaction. If there currently exists a writer, and p is not
    ** that writer, then the number of locks held by connections other
    ** than the writer must be about to drop to zero. In this case
    ** set the BTS_PENDING flag to 0.
    **
    ** If there is not currently a writer, then BTS_PENDING must
    ** be zero already. So this next line is harmless in that case.
	** 这个函数在B树p正在结束事务时被调用。如果当前存在一个写事务并且
	** p不是那个写事务，那么连接进程而不是写进程持有的锁的数量大约降至零。
	** 在这种情况下设置BTS_PENDING标签为0.
    */
    /*在B树p结束事务时，该函数被调用。如果有当前存在一个写事务，p不是那个写事务。
    那么连接进程而不是写进程持有的锁的数量大约降至零。在这种情况下，设置BTS_PENDING标志为0。
    如果目前还没有一个写事务，那么BTS_PENDING为零。因此，下一行(pBt->btsFlags &= ~BTS_PENDING;)在这种情况下是没有影响的。*/
	   /*
	【潘光珍】在B树p结束事务时，将调用此函数。如果目前存在一个写事务，并且p不是那个写事务。那么连接进程而不是
	写进程持有的锁的数量大约降至零。在这种情况下，设置BTS_PENDING标志为0。
	如果目前还没有一个写事务，那么BTS_PENDING必须为零。因此，下一行(pBt->btsFlags &= ~BTS_PENDING;)在这种情况下是没有影响的。
	*/
    pBt->btsFlags &= ~BTS_PENDING;
  }
}

/*
** This function changes all write-locks held by Btree p into read-locks.
*/
/*这个函数将B树p持有的所有写锁改变为读锁。*/
/*
【潘光珍】该函数将B树P持有的所有写锁变为读锁
*/
static void downgradeAllSharedCacheTableLocks(Btree *p){
  BtShared *pBt = p->pBt;
  if( pBt->pWriter==p ){
    BtLock *pLock;
    pBt->pWriter = 0;
    pBt->btsFlags &= ~(BTS_EXCLUSIVE|BTS_PENDING);
    for(pLock=pBt->pLock; pLock; pLock=pLock->pNext){
      assert( pLock->eLock==READ_LOCK || pLock->pBtree==p );
      pLock->eLock = READ_LOCK;
    }
  }
}

#endif /* SQLITE_OMIT_SHARED_CACHE */

static void releasePage(MemPage *pPage);  /* Forward reference 向前引用*/ //【潘光珍】释放pPage

/*
***** This routine is used inside of assert() only ****
** 这个程序仅仅被用在assert()内部
** Verify that the cursor holds the mutex on its BtShared
*/
/*这个程序里面只有assert（），确认游标持有BtShared上的互斥量。*/
/*
【潘光珍】这个程序用于内部的assert()仅仅确认游标BtShared持有互斥锁
*/
#ifdef SQLITE_DEBUG
static int cursorHoldsMutex(BtCursor *p){
  return sqlite3_mutex_held(p->pBt->mutex);
}
#endif


#ifndef SQLITE_OMIT_INCRBLOB
/*
** Invalidate the overflow page-list cache for cursor pCur, if any.
*/
/*
游标pCur持有的溢出页面（如有）缓存列表无效( pCur->aOverflow = 0)。
*/
static void invalidateOverflowCache(BtCursor *pCur){
  assert( cursorHoldsMutex(pCur) );
  sqlite3_free(pCur->aOverflow);
  pCur->aOverflow = 0;  //【潘光珍】游标pCur持有的溢出页面为0
}

/*
** Invalidate the overflow page-list cache for all cursors opened
** on the shared btree structure pBt.
*/
/*在共享B树结构pBt上，对所有打开的游标使溢出页列表无效。invalidateOverflowCache(p)*/
static void invalidateAllOverflowCache(BtShared *pBt){
  BtCursor *p;
  assert( sqlite3_mutex_held(pBt->mutex));
  for(p=pBt->pCursor; p; p=p->pNext){
    invalidateOverflowCache(p); //【潘光珍】调用游标pCur持有的溢出页面（如有）缓存列表无效的方法
  }
}

/*
** This function is called before modifying the contents of a table
** to invalidate any incrblob cursors that are open on the
** row or one of the rows being modified.
**
** If argument isClearTable is true, then the entire contents of the
** table is about to be deleted. In this case invalidate all incrblob
** cursors open on any row within the table with root-page pgnoRoot.
**
** Otherwise, if argument isClearTable is false, then the row with
** rowid iRow is being replaced or deleted. In this case invalidate
** only those incrblob cursors open on that specific row.
** 这个函数在表的内容被修改之前被调用，使开放的行或行中的一个
** 被修改的一个incrblob游标无效。
** 如果参数isClearTable为真，则表的全部内容都将被删除。在这样的情况下，
** 使在根页pgnoRoot上开放的行或行中的一个被修改的incrblob游标无效。
** 另外，如果参数isClearTable为假，那么有rowid iRow的行将被代替或删除。
** 在这种情况下，在特定行上的开放的这些incrblob游标无效。
*/
/*
【潘光珍】这个函数被调用之前修改一个表的内容无效的任何incrblob游标打开行或行被修改。
如果论点isClearTable是真的,那么的全部内容表将被删除。在这种情况下所有incrblob失效
游标打开表中的所有行根页pgnoRoot。否则,如果论点isClearTable是假的,那么行
rowid iRow被替换或删除。在这种情况下无效只有那些incrblob游标打开特定行。
*/
static void invalidateIncrblobCursors(        //使开放的行或行中的一个被修改的一个incrblob游标无效
  Btree *pBtree,          /* The database file to check */         //检查数据库文件 
  i64 iRow,               /* The rowid that might be changing */   //rowid可能发生改变
  int isClearTable        /* True if all rows are being deleted */ //如果所有的行都被删除返回真
){
  BtCursor *p;
  BtShared *pBt = pBtree->pBt;
  assert( sqlite3BtreeHoldsMutex(pBtree) );
  for(p=pBt->pCursor; p; p=p->pNext){
    if( p->isIncrblobHandle && (isClearTable || p->info.nKey==iRow) ){
      p->eState = CURSOR_INVALID;
    }
  }
}

#else
  /* Stub functions when INCRBLOB is omitted 当INCRBLOB被忽略时，清除函数*/ /*【潘光珍】当INCRBLOB被省略时，存根函数*/
  #define invalidateOverflowCache(x)
  #define invalidateAllOverflowCache(x)
  #define invalidateIncrblobCursors(x,y,z)
#endif /* SQLITE_OMIT_INCRBLOB */

/*
** Set bit pgno of the BtShared.pHasContent bitvec. This is called 
** when a page that previously contained data becomes a free-list leaf 
** page.
**
** The BtShared.pHasContent bitvec exists to work around an obscure
** bug caused by the interaction of two useful IO optimizations surrounding
** free-list leaf pages:
**
**   1) When all data is deleted from a page and the page becomes
**      a free-list leaf page, the page is not written to the database
**      (as free-list leaf pages contain no meaningful data). Sometimes
**      such a page is not even journalled (as it will not be modified,
**      why bother journalling it?).
**
**   2) When a free-list leaf page is reused, its content is not read
**      from the database or written to the journal file (why should it
**      be, if it is not at all meaningful?).
**
** By themselves, these optimizations work fine and provide a handy(方便的)
** performance boost to bulk delete or insert operations. However, if
** a page is moved to the free-list and then reused within the same
** transaction, a problem comes up. If the page is not journalled when
** it is moved to the free-list and it is also not journalled when it
** is extracted from the free-list and reused, then the original data
** may be lost. In the event of a rollback, it may not be possible
** to restore the database to its original configuration.
**
** The solution is the BtShared.pHasContent bitvec. Whenever a page is 
** moved to become a free-list leaf page, the corresponding bit is
** set in the bitvec. Whenever a leaf page is extracted from the free-list,
** optimization 2 above is omitted if the corresponding bit is already
** set in BtShared.pHasContent. The contents of the bitvec are cleared
** at the end of every transaction.
** 设定位向量BtShared.pHasContent的pgno的位。这个函数将在先前包含数据的页
** 变为一个空列叶节点页的时候被调用。
**
** BtShared.pHasContent位向量存在是为了解决未知的错误，这个错误是发生在空列叶节点页的
** 临近节点两个有用的IO优化的相互作用产生的：
** 1）当页的所有数据都被删除并且页变成空列表叶节点页的时候，这个页将不能被写到数据库中
**   （空列表叶节点的页中包含的是无效的数据）。有时这样的页甚至不会被记录到日志中。
**	 （因为它并没有被修改所有没有必要记录到日志）
** 2）当空列表叶节点的页被重新使用的时候，它的不会从数据库中或已经被写的日志文件中读取。
**    因为它是不是都有意义的。
**
** 通过本身，这些优化起到很好的作用，并提供一个灵活的性能提升达到批量删除或插入操作。
** 然而，如果一个页面移除而变为空闲列表，然后在相同事务中重新利用，就会产生问题。
** 当页面移除而变为空闲列表时，如果页面没有加入到日志，它也不会加入到日志，它从自由表中
** 提取和重新使用，则原始数据可能会丢失。在回滚的情况下，它可能无法将数据库恢复到原来的配置。
**
** 该解决方案就是BtShared.pHasContent 位向量。每当一个页面移除成为空列表叶子节点页时，
** 相应的位将在位向量中设置。每当一个叶节点页从空列表中提取，如果相应的位已经在
** BtShared.pHasContent中设置，则以上两种优化将被忽略。在每一事务结束该时，位向量的内容将被清除。
*/
/*
【潘光珍】设置一些使用pgno BtShared。pHasContent bitvec。
当一个页面之前包含数据成为空闲列表的叶子页面,这时被调用。
BtShared。pHasContent bitvec存在在一个不起眼的工作
错误引起的周围两个有用的IO优化之间的交互
叶空闲列表页:
1)当所有数据从一个页面和页面删除叶空闲列表页面,页面不写入数据库(如叶空闲列表页面不包含有意义的数据)。
有时这样一个页面甚至不是日志(不会被修改,为什么其他的日志?)。
2)当一个空闲列表叶页面重用,其内容不是阅读从数据库或写入日志文件(为什么它是,如果不是有意义呢?)。

本身这些优化工作,提供一个方便的批量删除或插入操作的性能提升。然而,如果搬到一个页面内的空闲列表然后重用相同交易,出现问题。如果页面没有杂志时
搬到空闲列表,它也不是杂志时从空闲列表中提取和重用,那么原始数据可能会丢失。在发生回滚,它可能是不可能的恢复数据库到原来的配置。

解决方案是BtShared。pHasContent bitvec。当一个页面搬到成为一个空闲列表页,相应的位在bitvec中设置。
每当从空闲列表中提取叶子页面时,优化2如果相应的一些已经被忽略BtShared.pHasContent。bitvec的内容被清除在每笔交易的结束。

*/
static int btreeSetHasContent(BtShared *pBt, Pgno pgno){
  int rc = SQLITE_OK;
  if( !pBt->pHasContent ){  /*自由页*/
    assert( pgno<=pBt->nPage );
    pBt->pHasContent = sqlite3BitvecCreate(pBt->nPage);
    if( !pBt->pHasContent ){
      rc = SQLITE_NOMEM;   /*分配内存失败*/
    }
  }
  if( rc==SQLITE_OK && pgno<=sqlite3BitvecSize(pBt->pHasContent) ){
    rc = sqlite3BitvecSet(pBt->pHasContent, pgno);
  }
  return rc;
}

/*
** Query the BtShared.pHasContent vector.
**
** This function is called when a free-list leaf page is removed from the
** free-list for reuse. It returns false if it is safe to retrieve the
** page from the pager layer with the 'no-content' flag set. True otherwise.
** 查询BtShared.pHasContent向量。
** 当一个空表的叶节点的页面从重用的空表中被移除时，这个函数将被调用。如果从
** 带有no-content标签的页面对象层检索页面是安全的则返回false。否则返回true
*/
/*
【潘光珍】**查询BtShared.pHasContent向量。这个函数被调用时叶空闲列表页面中
空闲列表以便重用。它检索返回false,如果它是安全的从页面调度程序层页面设置没有内容的标志。否则为True。
*/
static int btreeGetHasContent(BtShared *pBt, Pgno pgno){
  Bitvec *p = pBt->pHasContent; /*是否为自由页*/
  return (p && (pgno>sqlite3BitvecSize(p) || sqlite3BitvecTest(p, pgno)));
}

/*
** Clear (destroy) the BtShared.pHasContent bitvec. This should be
** invoked at the conclusion of each write-transaction.
** 清除BtShared.pHasContent位向量。这个程序应该在得到每个写事务结论是调用。
*/
/*在每个写事务的结尾被调用*/
static void btreeClearHasContent(BtShared *pBt){
  sqlite3BitvecDestroy(pBt->pHasContent);/*销毁位图对象，回收用过的内存*/
  pBt->pHasContent = 0;
}

/*
** Save the current cursor position in the variables BtCursor.nKey 
** and BtCursor.pKey. The cursor's state is set to CURSOR_REQUIRESEEK.
**
** The caller must ensure that the cursor is valid (has eState==CURSOR_VALID)
** prior to calling this routine.  
** 保存当前游标在变量BtCursor.nKey和BtCursor.pKey上的位置。游标的状态被设置为CURSOR_REQUIRESEEK.
** 这个调用者必须确保之前调用这个程序的游标是有效(有eState==CURSOR_VALID)。
*/
/*将游标的位置保存在变量BtCursor.nKey和BtCursor.pKey中，取pKey
中nKey长度的字段就可以找到游标所在位置。其中游标从0开始，pKey
指向游标的 last known位置。
*/
static int saveCursorPosition(BtCursor *pCur){//【潘光珍】保存游标所在的位置的方法
  int rc;

  assert( CURSOR_VALID==pCur->eState );/*前提:游标有效*/
  assert( 0==pCur->pKey );
  assert( cursorHoldsMutex(pCur) );

  rc = sqlite3BtreeKeySize(pCur, &pCur->nKey);
  assert( rc==SQLITE_OK );  /* KeySize() cannot fail，永远返回SQLITE_OK*/

  /* If this is an intKey table, then the above call to BtreeKeySize()
  ** stores the integer key in pCur->nKey. In this case this value is
  ** all that is required. Otherwise, if pCur is not open on an intKey
  ** table, then malloc space for and store the pCur->nKey bytes of key 
  ** data.
  ** 如果有一个intKey表，然后上边调用BtreeKeySize()并存储这个整数到pCur->nKey里。
  ** 在这时，这个值就是所需要的值。否则，如果在intKey表上pCur不是开放的，那么
  ** 动态分配空间并且存储关键字数据的 pCur->nKey字节。
  */
  /*
  【潘光珍】如果这是一个intKey表,然后上面的调用BtreeKeySize()
  将整数键存储在pCur->nKey。在这种情况下,这个值是需要的。
  否则,如果游标在intKey表中没有打开,然后分配malloc和存储空间 pCur->nKey字节的关键数据。
  */
  if( 0==pCur->apPage[0]->intKey ){/*游标在intKey表中没有打开，分配pCur->nKey大小的空间*/
    void *pKey = sqlite3Malloc( (int)pCur->nKey );
    if( pKey ){
      rc = sqlite3BtreeKey(pCur, 0, (int)pCur->nKey, pKey);
      if( rc==SQLITE_OK ){
        pCur->pKey = pKey;
      }else{
        sqlite3_free(pKey);
      }
    }else{
      rc = SQLITE_NOMEM;
    }
  }
  assert( !pCur->apPage[0]->intKey || !pCur->pKey );

  if( rc==SQLITE_OK ){
    int i;
    for(i=0; i<=pCur->iPage; i++){
      releasePage(pCur->apPage[i]);
      pCur->apPage[i] = 0;
    }
    pCur->iPage = -1;
    pCur->eState = CURSOR_REQUIRESEEK;/*游标指向的表被修改了，需要重新定位游标的位置*/
  }

  invalidateOverflowCache(pCur);//【潘光珍】调用游标pCur持有的溢出页面（如有）缓存列表无效的方法
  return rc;
}

/*
** Save the positions of all cursors (except pExcept) that are open on
** the table  with root-page iRoot. Usually, this is called just before cursor
** pExcept is used to modify the table (BtreeDelete() or BtreeInsert()).
** 保存所有在有根页iRoot的表上开放的游标的位置。保存有标的未知的意思是在B树上的位置
** 被记录这样可以在B树被修改后移回到相同的点。这个程序在游标pExcept被用于修改表之前被调用
** 例如在BtreeDelete()或 BtreeInsert()中。
** 如果在相同的Btree上有两个或者更多的游标，则所有这样的游标都应该有BTCF_Multiple标记。
** btreeCursor()会执行这个规则。这个程序被调用在不常见的情况下，如pExpect已经设置了BTCF_Multiple标记时。
** 如果pExpect!=NULL并且如果在相同的根页上没有游标，那么在pExpect上的BTCF_Multiple标记将被清除
** 避免再一次无意义的调用这个程序。
** 实现时注意：这个程序很少去核对是否有游标需要保存。它调用saveCursorsOnList()(异常)事件,游标是需要被保存。
*/
/*
【潘光珍】保存所有游标的位置(pExcept除外)与打开表上根页的iRoot。
通常,这调用前游标pExcept用于修改表(BtreeDelete()或BtreeInsert())。

*/
static int saveAllCursors(BtShared *pBt, Pgno iRoot, BtCursor *pExcept){
  BtCursor *p;
  assert( sqlite3_mutex_held(pBt->mutex) );
  assert( pExcept==0 || pExcept->pBt==pBt );
  for(p=pBt->pCursor; p; p=p->pNext){
    if( p!=pExcept && (0==iRoot || p->pgnoRoot==iRoot) && 
        p->eState==CURSOR_VALID ){/*指向根页的游标不需要保存*/
      int rc = saveCursorPosition(p);
      if( SQLITE_OK!=rc ){
        return rc;
      }
    }
  }
  return SQLITE_OK;
}

/* Clear the current cursor position.   清除当前游标位置*//*【潘光珍】删除当前游标所在位置*/
void sqlite3BtreeClearCursor(BtCursor *pCur){
  assert( cursorHoldsMutex(pCur) );
  sqlite3_free(pCur->pKey);
  pCur->pKey = 0;
  pCur->eState = CURSOR_INVALID;
}

/*
** In this version of BtreeMoveto, pKey is a packed index record
** such as is generated by the OP_MakeRecord opcode.  Unpack the
** record and then call BtreeMovetoUnpacked() to do the work.
** 在BtreeMoveto的这个版本中，pKey是一个包索引记录如由OP_MakeRecord生成的操作码。
** 打开记录然后调用BtreeMovetoUnpacked()来完成这项工作。
*/
/*【潘光珍】在这个版本的BtreeMoveto,pKey拥挤指数由OP_MakeRecord操作码生成等记录。
打开记录,然后调用BtreeMovetoUnpacked()来做这个工作。*/
static int btreeMoveto(
  BtCursor *pCur,     /* Cursor open on the btree to be searched  在B树上开放游标使之能被搜索到*//*【潘光珍】游标打开btree搜索*/
  const void *pKey,   /* Packed key if the btree is an index 如果B树是一个索引则打包关键字*/ /*【潘光珍】如果btree索引是包装的关键*/
  i64 nKey,           /* Integer key for tables.  Size of pKey for indices 表的整数关键字，pKey的大小*/  /*【潘光珍】表中的整形键*/
  int bias,           /* Bias search to the high end 搜索最终高度*/ /*【潘光珍】搜索到最大值*/
  int *pRes           /* Write search results here 写搜索结果*/    /*【潘光珍】搜索结果写在这里*/
){
  int rc;                    /* Status code 状态码*/
  UnpackedRecord *pIdxKey;   /* Unpacked index key 打开索引键 */
  char aSpace[150];          /* Temp space for pIdxKey - to avoid a malloc **pIdxKey的临时空间，避免动态分配*/
  char *pFree = 0;

  if( pKey ){
    assert( nKey==(i64)(int)nKey );
    pIdxKey = sqlite3VdbeAllocUnpackedRecord(
        pCur->pKeyInfo, aSpace, sizeof(aSpace), &pFree
    );
    if( pIdxKey==0 ) return SQLITE_NOMEM;
    sqlite3VdbeRecordUnpack(pCur->pKeyInfo, (int)nKey, pKey, pIdxKey);
  }else{
    pIdxKey = 0;
  }
  rc = sqlite3BtreeMovetoUnpacked(pCur, pIdxKey, nKey, bias, pRes);
  if( pFree ){
    sqlite3DbFree(pCur->pKeyInfo->db, pFree);
  }
  return rc;
}
/*
** Restore the cursor to the position it was in (or as close to as possible)
** when saveCursorPosition() was called. Note that this call deletes the 
** saved position info stored by saveCursorPosition(), so there can be
** at most one effective restoreCursorPosition() call after each 
** saveCursorPosition().
** 当saveCursorPosition()被调用的时候，重新保存有标的位置。注意这个调用会
** 删除saveCursorPosition()之前保存的位置信息。因此在每一个saveCursorPosition()后
** 有一个有效的restoreCursorPosition()调用
*/
/*调用saveCursorPosition()之后，saveCursorPosition()中保存的位置信息被删除，因此要恢复
游标位置。*/
static int btreeRestoreCursorPosition(BtCursor *pCur){
  int rc;
  assert( cursorHoldsMutex(pCur) );
  assert( pCur->eState>=CURSOR_REQUIRESEEK );/*游标处于CURSOR_FAULT|CURSOR_REQUIRESEEK状态 */
  if( pCur->eState==CURSOR_FAULT ){
    return pCur->skipNext;  /* Prev() is noop if (skipNext) negative. Next() is noop if positive  如果skipNext是负的则 Prev()无操作。如果为正则Next()无操作*/
  }
  pCur->eState = CURSOR_INVALID;
  rc = btreeMoveto(pCur, pCur->pKey, pCur->nKey, 0, &pCur->skipNext);
  if( rc==SQLITE_OK ){
    sqlite3_free(pCur->pKey);
    pCur->pKey = 0;
    assert( pCur->eState==CURSOR_VALID || pCur->eState==CURSOR_INVALID );
  }
  return rc;
}

#define restoreCursorPosition(p) \
  (p->eState>=CURSOR_REQUIRESEEK ? \
         btreeRestoreCursorPosition(p) : \
         SQLITE_OK)

/*
** Determine whether or not a cursor has moved from the position it
** was last placed at.  Cursors can move when the row they are pointing
** at is deleted out from under them.
**
** This routine returns an error code if something goes wrong.  The
** integer *pHasMoved is set to one if the cursor has moved and 0 if not.
** 确定游标是否已经从上一次的位置发生了移动或者由于其他原因而无效。
** 例如，当游标正指向被删除的列时游标可以移动。如果B树再平衡，游标也许会发生移动。
** 调用带有空游标的程序时返回false。
** 使用单独的sqlite3BtreeCursorRestore()程序恢复一个游标到它应该在的位置，如果这个程序返回true的话。
*/
/*游标是否移动。出错返回错误代码。*/
/*
【潘光珍】确定是否一个游标已经从它的位置最后被放置。游标可以移动,当行指向
在被删除从他们。如果出现错误，这个程序返回一个错误代码。如果游标已经移动了，
则 pHasMoved这个整形指针被设置为1，否则为0。
*/
int sqlite3BtreeCursorHasMoved(BtCursor *pCur, int *pHasMoved){
  int rc;  //状态码

  rc = restoreCursorPosition(pCur);
  if( rc ){
    *pHasMoved = 1;
    return rc;
  }
  if( pCur->eState!=CURSOR_VALID || pCur->skipNext!=0 ){
    *pHasMoved = 1;
  }else{
    *pHasMoved = 0;
  }
  return SQLITE_OK;
}

#ifndef SQLITE_OMIT_AUTOVACUUM
/*
** Given a page number of a regular database page, return the page
** number for the pointer-map page that contains the entry for the
** input page number.
**
** Return 0 (not a valid page) for pgno==1 since there is
** no pointer map associated with page 1.  The integrity_check logic
** requires that ptrmapPageno(*,1)!=1.
** 鉴于常规数据库页的页号，返回页号为包含用于将输入页号条目的指针位图页。
** 对于pgno==1，返回0（不是一个有效的页）。因为没有与页1相关的指针位图。
** 完整性检查的逻辑要求是ptrmapPageno(*,1)!=1
*/
/*
【潘光珍】鉴于常规数据库页面的页码,返回的页面数量pointer-map页面,其中包含的条目
输入页码。返回0(不是一个有效的页面)以来pgno = = 1没有指针映射与第1页。integrity_check逻辑
要求ptrmapPageno(* 1)! = 1。
*/
static Pgno ptrmapPageno(BtShared *pBt, Pgno pgno){
  int nPagesPerMapPage;
  Pgno iPtrMap, ret;
  assert( sqlite3_mutex_held(pBt->mutex) );
  if( pgno<2 ) return 0;     /*无效页，因为没有指针指向页面1*/
  nPagesPerMapPage = (pBt->usableSize/5)+1;
  iPtrMap = (pgno-2)/nPagesPerMapPage;
  ret = (iPtrMap*nPagesPerMapPage) + 2; 
  if( ret==PENDING_BYTE_PAGE(pBt) ){
    ret++;
  }
  return ret;
}

/*
** Write an entry into the pointer map.
**
** This routine updates the pointer map entry for page number 'key'
** so that it maps to type 'eType' and parent page number 'pgno'.
**
** If *pRC is initially non-zero (non-SQLITE_OK) then this routine is
** a no-op.  If an error occurs, the appropriate error code is written
** into *pRC.
** 写一个条目进入指针位图。这个程序更新页码“key”的指针位图条目，
** 以便于，它映射到类型'eType'与父页码'pgno'.
** 如果*pRC的初始化是非零的(non-SQLITE_OK)，那么这个程序无操作的。
** 如果一个错误发生，则相应的错误代码被写入*pRC.
*/
/*
【潘光珍】
*写一个进入的指针映项。
**这个程序更新页码'key'的指针映射项以便它映射到类型'eType'和父页码'pgno'如果*pRC最初非零(non-SQLITE_OK)，
则这个程序可以任何操作。如果发生错误，适当的错误代码会写进*pRC*/
static void ptrmapPut(BtShared *pBt, Pgno key, u8 eType, Pgno parent, int *pRC){
  DbPage *pDbPage;  /* The pointer map page 指针位图页*/ /*【潘光珍】Pager的页句柄*/
  u8 *pPtrmap;      /* The pointer map data 指针位图的数据域*/ 
  Pgno iPtrmap;     /* The pointer map page number 指针位图的页码*/
  int offset;       /* Offset in pointer map page 指针位图页的偏移量*/
  int rc;           /* Return code from subfunctions(子函数) 从子函数返回到代码*//* 【潘光珍】返回子函数代码*/

  if( *pRC ) return;

  assert( sqlite3_mutex_held(pBt->mutex) );
  /* The master-journal page number must never be used as a pointer map page 
  ** 主日志页码一定不能用作指针位图页
  */
  /*【潘光珍】master-journal页面数量绝不能被用作一个指针位图页面*/
  assert( 0==PTRMAP_ISPAGE(pBt, PENDING_BYTE_PAGE(pBt)) );

  assert( pBt->autoVacuum );
  if( key==0 ){
    *pRC = SQLITE_CORRUPT_BKPT;
    return;
  }
  iPtrmap = PTRMAP_PAGENO(pBt, key);
  rc = sqlite3PagerGet(pBt->pPager, iPtrmap, &pDbPage);
  if( rc!=SQLITE_OK ){
    *pRC = rc;
    return;
  }
  offset = PTRMAP_PTROFFSET(iPtrmap, key);
  if( offset<0 ){
    *pRC = SQLITE_CORRUPT_BKPT;
    goto ptrmap_exit;
  }
  assert( offset <= (int)pBt->usableSize-5 );
  pPtrmap = (u8 *)sqlite3PagerGetData(pDbPage);

  if( eType!=pPtrmap[offset] || get4byte(&pPtrmap[offset+1])!=parent ){
    TRACE(("PTRMAP_UPDATE: %d->(%d,%d)\n", key, eType, parent));
    *pRC= rc = sqlite3PagerWrite(pDbPage);
    if( rc==SQLITE_OK ){
      pPtrmap[offset] = eType;
      put4byte(&pPtrmap[offset+1], parent);
    }
  }

ptrmap_exit:
  sqlite3PagerUnref(pDbPage);
}

/*
** Read an entry from the pointer map.
** 
** This routine retrieves the pointer map entry for page 'key', writing
** the type and parent page number to *pEType and *pPgno respectively.
** An error code is returned if something goes wrong, otherwise SQLITE_OK.
** 从指针位图读取条目。
** 这个函数检索页面 'key'的指针位图条目，分别将类型和父页码写入到*pEType 和 *pPgno中。
** 如果运行出错则返回错误代码，其他返回SQLITE_OK.
*/
/*读取pointer map的条目，写入pEType和pPgno中*/
/*
【潘光珍】*读一个进入映像的指针。
**这个程序获取指针映射条目页面'key',写的类型和父页数目的*pEType 和 *pPgno相互分开
**如果出现错误,返回一个错误的代码，否则返回SQLITE_OK。
*/
static int ptrmapGet(BtShared *pBt, Pgno key, u8 *pEType, Pgno *pPgno){
  DbPage *pDbPage;   /* The pointer map page 指针位图页*/  /*【潘光珍】Pager的页句柄*/
  int iPtrmap;       /* Pointer map page index 指针位图页索引*//*【潘光珍】指针映射页面索引*/
  u8 *pPtrmap;       /* Pointer map page data 指针位图页数据*/ /*【潘光珍】指针映射页数据*/
  int offset;        /* Offset of entry in pointer map 指针位图页的偏移量*//*【潘光珍】指针映射的输入偏移*/
  int rc;

  assert( sqlite3_mutex_held(pBt->mutex) );

  iPtrmap = PTRMAP_PAGENO(pBt, key);
  rc = sqlite3PagerGet(pBt->pPager, iPtrmap, &pDbPage);
  if( rc!=0 ){
    return rc;
  }
  pPtrmap = (u8 *)sqlite3PagerGetData(pDbPage);

  offset = PTRMAP_PTROFFSET(iPtrmap, key);
  if( offset<0 ){
    sqlite3PagerUnref(pDbPage);
    return SQLITE_CORRUPT_BKPT;
  }
  assert( offset <= (int)pBt->usableSize-5 );
  assert( pEType!=0 );
  *pEType = pPtrmap[offset];
  if( pPgno ) *pPgno = get4byte(&pPtrmap[offset+1]);

  sqlite3PagerUnref(pDbPage);
  if( *pEType<1 || *pEType>5 ) return SQLITE_CORRUPT_BKPT;
  return SQLITE_OK;
}

#else /* if defined SQLITE_OMIT_AUTOVACUUM */
  #define ptrmapPut(w,x,y,z,rc)
  #define ptrmapGet(w,x,y,z) SQLITE_OK
  #define ptrmapPutOvflPtr(x, y, rc)
#endif

/*
** Given a btree page and a cell index (0 means the first cell on
** the page, 1 means the second cell, and so forth) return a pointer
** to the cell content.
**
** This routine works only for pages that do not contain overflow cells.
** 给定的B树页和单元索引（0意味着页上的第一个单元，1是第二个单元，等等）返回一个指向单元内容的指针
** 这个程序只对不包含溢出单元的页起作用
*/
/*给定的B树页和单元索引（0意味着页上的第一个单元，1是第二个单元，等等）返回一个指向单元内容的指针*/
/*
【潘光珍】在btree页面和一个cell index当中 (0意味着页面上的第一个单元格,1意味着第二个单元格,等等)返回一个指针指向单元内容。
这个程序只适合页面不包含溢出的cells。
*/
#define findCell(P,I) \
  ((P)->aData + ((P)->maskPage & get2byte(&(P)->aCellIdx[2*(I)])))
#define findCellv2(D,M,O,I) (D+(M&get2byte(D+(O+2*(I)))))


/*
** This a more complex version of findCell() that works for
** pages that do contain overflow cells.
** 针对包含溢出单元的更为复杂的findCell()版本
*/
/*
【潘光珍】这个更复杂的版本的findCell()为页面做的工作包含溢出的cells。
*/
static u8 *findOverflowCell(MemPage *pPage, int iCell){
  int i;
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  for(i=pPage->nOverflow-1; i>=0; i--){
    int k;
    k = pPage->aiOvfl[i];
    if( k<=iCell ){
      if( k==iCell ){
        return pPage->apOvfl[i];
      }
      iCell--;
    }
  }
  return findCell(pPage, iCell);
}

/*
** Parse a cell content block and fill in the CellInfo structure.  There
** are two versions of this function.  btreeParseCell() takes a 
** cell index as the second argument and btreeParseCellPtr() 
** takes a pointer to the body of the cell as its second argument.
**
** Within this file, the parseCell() macro can be called instead of
** btreeParseCellPtr(). Using some compilers, this will be faster.
** 解析单元内容块，填在CellInfo结构中。这个函数有两个版本。btreeParseCell()
** 占了一个单元索引，它是第一个。btreeParseCellPtr()占了一个指向单元体的指针是第二个版本。
** 在这文件内，可以调用宏parseCell()而不是btreeParseCellPtr()。用一些编译程序会更快。
*/
/*解析cell content block，填在CellInfo结构中。*/
/*
【潘光珍】**解析cell content block，填在CellInfo结构中。这个函数有两个版本。
在btreeParseCell()这函数中，将一个单元格作为第二个参数，
在btreeParseCellPtr()这个函数中，将一个指针指向单元格本身作为它第二个参数。

**在这个文件中,parseCell()的宏可以代替btreeParseCellPtr()。使用编译器,这样将会更快。

*/
static void btreeParseCellPtr(           //解析单元内容块，填在CellInfo结构中
  MemPage *pPage,         /* Page containing the cell 包含单元的页*/  /*【潘光珍】包含单元格的页面*/
  u8 *pCell,              /* Pointer to the cell text. 单元文本的指针*/ /*【潘光珍】指向单元内容的指针*/
  CellInfo *pInfo         /* Fill in this structure 填充这个结构*/
){
  u16 n;                  /* Number bytes in cell content header 单元内容头部的字节数*/  
  u32 nPayload;           /* Number of bytes of cell payload 单元有效载荷（B树记录）的字节数*/ /*【潘光珍】单元有效负载的字节数*/

  assert( sqlite3_mutex_held(pPage->pBt->mutex) );

  pInfo->pCell = pCell;
  assert( pPage->leaf==0 || pPage->leaf==1 );
  n = pPage->childPtrSize;
  assert( n==4-4*pPage->leaf );
  if( pPage->intKey ){
    if( pPage->hasData ){
      n += getVarint32(&pCell[n], nPayload);
    }else{
      nPayload = 0;
    }
    n += getVarint(&pCell[n], (u64*)&pInfo->nKey);
    pInfo->nData = nPayload;
  }else{
    pInfo->nData = 0;
    n += getVarint32(&pCell[n], nPayload);
    pInfo->nKey = nPayload;
  }
  pInfo->nPayload = nPayload;
  pInfo->nHeader = n;
  testcase( nPayload==pPage->maxLocal );
  testcase( nPayload==pPage->maxLocal+1 );
  if( likely(nPayload<=pPage->maxLocal) ){
    /* This is the (easy) common case where the entire payload fits
    ** on the local page.  No overflow is required.
	** 这是个常见的情况，所有的有效载荷都固定在本地页上。不需要溢出。
    */
	   /*
	  【潘光珍】这是在容易的情况下，整个负载适合在本地页面上。不需要溢出
	  */
    if( (pInfo->nSize = (u16)(n+nPayload))<4 ) pInfo->nSize = 4;
    pInfo->nLocal = (u16)nPayload;
    pInfo->iOverflow = 0;
  }else{
    /* If the payload will not fit completely on the local page, we have
    ** to decide how much to store locally and how much to spill onto
    ** overflow pages.  The strategy is to minimize the amount of unused
    ** space on overflow pages while keeping the amount of local storage
    ** in between minLocal and maxLocal.
    **
    ** Warning:  changing the way overflow payload is distributed in any
    ** way will result in an incompatible file format.
	** 如果负载不适合完全在本地页面,我们必须决定多少存储在本地和多少存储在溢出页。
	** 这个策略是减少溢出页上未使用空间的数量 同时保持本地存储的数量在minLocal和maxLocal之间。
	** 警告:任意改变溢流载荷分布会导致不兼容的文件格式。
    */
	  /*
	【潘光珍】如果负载将不完全匹配本地页，我们必须决定多少存储在本地和多少泄漏到溢出页。
	未使用的策略是减少空间溢出页同时保持中的本地存储minLocal和maxLocal之间。
	警告:以任何方式改变溢流载荷分布的方式会导致不兼容的文件格式。
	*/
    /*使本地存储在最小和最大值之间并且将溢出页未使用区域最小化*/
    int minLocal;  /* Minimum amount of payload held locally */       //本地持有的最小有效载荷数量 
    int maxLocal;  /* Maximum amount of payload held locally */       //本地持有的最大有效载荷数量
    int surplus;   /* Overflow payload available for local storage */ //溢出的有效载荷可用于本地存储  /*【潘光珍】可用于本地存储的溢出有效负载*/

    minLocal = pPage->minLocal;
    maxLocal = pPage->maxLocal;
    surplus = minLocal + (nPayload - minLocal)%(pPage->pBt->usableSize - 4);
    testcase( surplus==maxLocal );
    testcase( surplus==maxLocal+1 );
    if( surplus <= maxLocal ){
      pInfo->nLocal = (u16)surplus;
    }else{   /*溢出*/
      pInfo->nLocal = (u16)minLocal;
    }
    pInfo->iOverflow = (u16)(pInfo->nLocal + n);
    pInfo->nSize = pInfo->iOverflow + 4;
  }
}
#define parseCell(pPage, iCell, pInfo) \
  btreeParseCellPtr((pPage), findCell((pPage), (iCell)), (pInfo))
static void btreeParseCell(                                      //解析单元内容块
  MemPage *pPage,         /* Page containing the cell */         //包含单元的页  /*【潘光珍】包含单元格的页面*/
  int iCell,              /* The cell index.  First cell is 0 */ //单元的索引，第一个单元iCell为0  /*【潘光珍】单元格索引，首单元格是0*/
  CellInfo *pInfo         /* Fill in this structure */           //填写这个结构
){
  parseCell(pPage, iCell, pInfo);
}

/*
** Compute the total number of bytes that a Cell needs in the cell
** data area of the btree-page.  The return number includes the cell
** data header and the local payload, but not any overflow page or
** the space used by the cell pointer.
*/
/*计算一个Cell需要的总的字节数*/
/*【潘光珍】计算单元格数据区域中b树页的单元格需要的总字节数。
返回的数字包括小区数据头和本地负载，但没有任何溢出页或单元指针所使用的空间。

*/
static u16 cellSizePtr(MemPage *pPage, u8 *pCell){  //计算一个Cell需要的总的字节数
  u8 *pIter = &pCell[pPage->childPtrSize];
  u32 nSize;

#ifdef SQLITE_DEBUG
  /* The value returned by this function should always be the same as
  ** the (CellInfo.nSize) value found by doing a full parse of the
  ** cell. If SQLITE_DEBUG is defined, an assert() at the bottom of
  ** this function verifies that this invariant(不变式) is not violated(违反). 
  ** 这个函数的返回值应始终是和执行一个完整的解析中发现的单元（CellInfo.nSize）值相同。
  ** 如果SQLITE_DEBUG被定义，一个assert（）处的底部他的功能验证这个不变不违反。*/
  /*
  【潘光珍】这个函数返回的值应该是相同的（CellInfo.nSize）值做一个全面的解析单元格的发现。
  如果SQLITE_DEBUG被定义，在这个函数的底部assert()证明这种不变（不变式）不违反（违反）。
  */
  CellInfo debuginfo;
  btreeParseCellPtr(pPage, pCell, &debuginfo);
#endif

  if( pPage->intKey ){
    u8 *pEnd;
    if( pPage->hasData ){
      pIter += getVarint32(pIter, nSize);
    }else{
      nSize = 0;
    }

    /* pIter now points at the 64-bit integer key value, a variable length 
    ** integer. The following block moves pIter to point at the first byte
    ** past the end of the key value. 
	** pIter现在指向在64位整数关键字值，可变长度整数。下面的块移动pIter指向键值末尾的第一个字节。*/
	/*【潘光珍】pIter指在64位整数的关键值，一个可变长度的整数。
	下面的块移动pIter指向第一个字节的关键值结束过去*/
    pEnd = &pIter[9];
    while( (*pIter++)&0x80 && pIter<pEnd );
  }else{
    pIter += getVarint32(pIter, nSize);
  }

  testcase( nSize==pPage->maxLocal );
  testcase( nSize==pPage->maxLocal+1 );
  if( nSize>pPage->maxLocal ){
    int minLocal = pPage->minLocal;
    nSize = minLocal + (nSize - minLocal) % (pPage->pBt->usableSize - 4);
    testcase( nSize==pPage->maxLocal );
    testcase( nSize==pPage->maxLocal+1 );
    if( nSize>pPage->maxLocal ){
      nSize = minLocal;
    }
    nSize += 4;
  }
  nSize += (u32)(pIter - pCell);

  /* The minimum size of any cell is 4 bytes. */ /*【潘光珍】任何单元的最小尺寸为4个字节。*/
  if( nSize<4 ){
    nSize = 4;
  }

  assert( nSize==debuginfo.nSize );
  return (u16)nSize;
}

#ifdef SQLITE_DEBUG
/* This variation on cellSizePtr() is used inside of assert() statements
** only. 
** cellSizePtr()中的变量仅仅被用在assert()语句中 */
/*【潘光珍】这种变化对cellsizeptr()里面assert()语句只能使用。*/
static u16 cellSize(MemPage *pPage, int iCell){
  return cellSizePtr(pPage, findCell(pPage, iCell));
}
#endif

#ifndef SQLITE_OMIT_AUTOVACUUM
/*
** If the cell pCell, part of page pPage contains a pointer
** to an overflow page, insert an entry into the pointer-map
** for the overflow page.
*/
/*如果pCell(pPage的一部分)包含指向溢出页的指针，则为这个溢出页插入一个条目到pointer-map*/
static void ptrmapPutOvflPtr(MemPage *pPage, u8 *pCell, int *pRC){
  CellInfo info;
  if( *pRC ) return;
  assert( pCell!=0 );
  btreeParseCellPtr(pPage, pCell, &info);
  assert( (info.nData+(pPage->intKey?0:info.nKey))==info.nPayload );
  if( info.iOverflow ){
    Pgno ovfl = get4byte(&pCell[info.iOverflow]);
    ptrmapPut(pPage->pBt, ovfl, PTRMAP_OVERFLOW1, pPage->pgno, pRC);
  }
}
#endif


/*
** Defragment the page given.  All Cells are moved to the
** end of the page and all free space is collected into one
** big FreeBlk that occurs in between the header and cell
** pointer array and the cell content area.
** 重整给出的页面。所有的单元被转移到该页的结束，所有的自由空间被收集到
** 在所述头部和单元指针数组和小区内容区域之间的一个大的FreeBlk。
*/
/*重整页面。*/
/*
【潘光珍】整理页面。所有的单元格都移到页面结束所有的自由空间被收集到
一个大的FreeBlk发生在头和单元格指针和数组内容区域之间。
*/
static int defragmentPage(MemPage *pPage){
  int i;                     /* Loop counter */                      //循环内的参数i  /*【潘光珍】循环计数器*/
  int pc;                    /* Address of a i-th cell */            //第i个单元的地址 /*【潘光珍】一个单元格地址*/
  int hdr;                   /* Offset to the page header */         //页头部得偏移量  /*【潘光珍】偏移页首*/
  int size;                  /* Size of a cell */                    //单元的大小     /
  int usableSize;            /* Number of usable bytes on a page */  //页上可用单元的数量  /*【潘光珍】页面上可用字节数* 
  int cellOffset;            /* Offset to the cell pointer array */  //单元指针数组的偏移量/*【潘光珍】偏移到单元格指针数组*/
  int cbrk;                  /* Offset to the cell content area */   //单元内容区域的偏移量/*【潘光珍】偏移到单元格内容区域*/
  int nCell;                 /* Number of cells on the page */       //页上单元的数量    /*【潘光珍】页面上的单元格数*/
  unsigned char *data;       /* The page data */                     //页数据
  unsigned char *temp;       /* Temp area for cell content */        //单元内容的临时区   /*【潘光珍】单元格内容的临时区域*/
  int iCellFirst;            /* First allowable cell index */        //第一个许可的单元索引    /*【潘光珍】允许单元格索引的第一个*/
  int iCellLast;             /* Last possible cell index */          //最后一个可能的单元索引   /*【潘光珍】最后一个可能的单元格索引*/


  assert( sqlite3PagerIswriteable(pPage->pDbPage) );
  assert( pPage->pBt!=0 );
  assert( pPage->pBt->usableSize <= SQLITE_MAX_PAGE_SIZE );
  assert( pPage->nOverflow==0 );
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  temp = sqlite3PagerTempSpace(pPage->pBt->pPager);
  data = pPage->aData;
  hdr = pPage->hdrOffset;
  cellOffset = pPage->cellOffset;
  nCell = pPage->nCell;
  assert( nCell==get2byte(&data[hdr+3]) );
  usableSize = pPage->pBt->usableSize;
  cbrk = get2byte(&data[hdr+5]);
  memcpy(&temp[cbrk], &data[cbrk], usableSize - cbrk);
  cbrk = usableSize;
  iCellFirst = cellOffset + 2*nCell;
  iCellLast = usableSize - 4;
  for(i=0; i<nCell; i++){
    u8 *pAddr;     /* The i-th cell pointer */  //第i个单元的指针
    pAddr = &data[cellOffset + i*2];
    pc = get2byte(pAddr);
    testcase( pc==iCellFirst );
    testcase( pc==iCellLast );
#if !defined(SQLITE_ENABLE_OVERSIZE_CELL_CHECK)
    /* These conditions have already been verified in btreeInitPage()
    ** if SQLITE_ENABLE_OVERSIZE_CELL_CHECK is defined 
	** 如果SQLITE_ENABLE_OVERSIZE_CELL_CHECK被定义了，则这些条件已经在btreeInitPage()中被验证
    */
	/*
	【潘光珍】如果SQLITE_ENABLE_OVERSIZE_CELL_CHECK已经被定义,则这些条件已经验证了btreeinitpage()
	*/
    if( pc<iCellFirst || pc>iCellLast ){
      return SQLITE_CORRUPT_BKPT;
    }
#endif
    assert( pc>=iCellFirst && pc<=iCellLast );/*第一个允许cell索引，最后一个可能的cell索引之间。*/ /*【潘光珍】在第一个允许cell索引，最后一个可能的cell索引之间。*/
    size = cellSizePtr(pPage, &temp[pc]);
    cbrk -= size;
#if defined(SQLITE_ENABLE_OVERSIZE_CELL_CHECK)
    if( cbrk<iCellFirst ){         // 单元内容偏移量比第一个单元的偏移量还小，错误处理
      return SQLITE_CORRUPT_BKPT;
    }
#else
    if( cbrk<iCellFirst || pc+size>usableSize ){
      return SQLITE_CORRUPT_BKPT;
    }
#endif
    assert( cbrk+size<=usableSize && cbrk>=iCellFirst );
    testcase( cbrk+size==usableSize );
    testcase( pc+size==usableSize );
    memcpy(&data[cbrk], &temp[pc], size);/*memcpy(&temp[cbrk], &data[cbrk], usableSize - cbrk);*/
    put2byte(pAddr, cbrk);
  }
  assert( cbrk>=iCellFirst );
  put2byte(&data[hdr+5], cbrk);
  data[hdr+1] = 0;
  data[hdr+2] = 0;
  data[hdr+7] = 0;
  memset(&data[iCellFirst], 0, cbrk-iCellFirst);
  assert( sqlite3PagerIswriteable(pPage->pDbPage) );
  if( cbrk-iCellFirst!=pPage->nFree ){
    return SQLITE_CORRUPT_BKPT;
  }
  return SQLITE_OK;
}

/*
** Allocate nByte bytes of space from within the B-Tree page passed
** as the first argument. Write into *pIdx the index into pPage->aData[]
** of the first byte of allocated space. Return either SQLITE_OK or
** an error code (usually SQLITE_CORRUPT).
** 从通过B树页中分配的空间nByte字节作为第一个参数。写入*pIdx（索引Page->aData[]的分配
** 空间的第一个字节）。返回要么SQLITE_OK或一个错误码（通常SQLITE_CORRUPT）。
**
** The caller guarantees that there is sufficient space to make the
** allocation.  This routine might need to defragment in order to bring
** all the space together, however.  This routine will avoid using
** the first two bytes past the cell pointer area since presumably this
** allocation is being made in order to insert a new cell, so we will
** also end up needing a new cell pointer.
** 调用保证有足够的空间去分配。但是，该程序可能需要进行碎片整理使所有的空间合并在一起。
** 这个程序将避免使用通过单元指针指针区域的前两个字节，推测为了插入一个新的单元而正在分配，
** 所以我们将也最终需要一个新的单元格指针。
*/
/*在pPage上分配nByte字节的空间，将索引写入pIdx中*/
/*
【潘光珍】**在pPage上分配nByte字节的空间，将索引写入pIdx中分配空间的第一个字节。
返回SQLITE_OK或错误代码（通常SQLITE_CORRUPT）。
**调用方保证有足够的空间来进行分配。这个程序可能需要整理才能带来
所有的空间，但是，这个程序将避免使用第一个2个字节过去的单元格指针区域，
因为大概是这样为了插入一个新的单元格，我们将分配也结束了需要一个新的单元格指针。
*/
static int allocateSpace(MemPage *pPage, int nByte, int *pIdx){
  const int hdr = pPage->hdrOffset;    /* Local cache of pPage->hdrOffset */       //pPage->hdrOffset的本地缓存
  u8 * const data = pPage->aData;      /* Local cache of pPage->aData */           //pPage->aData的本地缓存
  int nFrag;                           /* Number of fragmented bytes on pPage */   //页上的碎片字节数
  int top;                             /* First byte of cell content area */       //单元内容的第一个字节
  int gap;        /* First byte of gap between cell pointers and cell content */   //单元指针和单元内容之间间隙的第一个字节
  int rc;         /* Integer return code */                                        //整型返回码
  int usableSize; /* Usable size of the page */    //页能够使用的大小/*【潘光珍】 每页可用的字节数。pageSize-每页尾部保留空间的大小，在文件头偏移为20处设定。 */
                        
  
  assert( sqlite3PagerIswriteable(pPage->pDbPage) );
  assert( pPage->pBt );
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  assert( nByte>=0 );  /* Minimum cell size is 4 */   //最小单元大小为4字节
  assert( pPage->nFree>=nByte );
  assert( pPage->nOverflow==0 );
  usableSize = pPage->pBt->usableSize;
  assert( nByte < usableSize-8 );

  nFrag = data[hdr+7];
  assert( pPage->cellOffset == hdr + 12 - 4*pPage->leaf );
  gap = pPage->cellOffset + 2*pPage->nCell;
  top = get2byteNotZero(&data[hdr+5]);
  if( gap>top ) return SQLITE_CORRUPT_BKPT;
  testcase( gap+2==top );
  testcase( gap+1==top );
  testcase( gap==top );

  if( nFrag>=60 ){
    /* Always defragment highly fragmented pages */  //整理碎片较多的页
    rc = defragmentPage(pPage);
    if( rc ) return rc;
    top = get2byteNotZero(&data[hdr+5]);
  }else if( gap+2<=top ){
    /* Search the freelist looking for a free slot big enough to satisfy 
    ** the request. The allocation is made from the first free slot in 
    ** the list that is large enough to accomadate it.
	** 搜索空闲列表寻找满足要求的足够大的 free slot。分配的区域由列表中的
	** 第一个 free slot组成，其中列表是足够装 free slot。
    */
	  /*
	  【潘光珍】搜索空闲列表中寻找一个空闲槽的足够大以满足要求。
	  配置是由列表中的是大到足以容纳它的第一个空闲时隙。
	  */
    int pc, addr;
    for(addr=hdr+1; (pc = get2byte(&data[addr]))>0; addr=pc){
      int size;            /* Size of the free slot */   // free slot的大小  //【潘光珍】设置空闲槽的大小
      if( pc>usableSize-4 || pc<addr+4 ){
        return SQLITE_CORRUPT_BKPT;
      }
      size = get2byte(&data[pc+2]);
      if( size>=nByte ){
        int x = size - nByte;
        testcase( x==4 );
        testcase( x==3 );
        if( x<4 ){
          /* Remove the slot from the free-list. Update the number of
          ** fragmented bytes within the page. */  //从自由列表中移除slot，在页内更新碎片的数量
			/*
			【潘光珍】从空闲列表中删除插槽。更新页面内的碎片字节数。
			*/
          memcpy(&data[addr], &data[pc], 2);
          data[hdr+7] = (u8)(nFrag + x);
        }else if( size+pc > usableSize ){
          return SQLITE_CORRUPT_BKPT;
        }else{
          /* The slot remains on the free-list. Reduce its size to account   //slot保留在自由列表上，
          ** for the portion used by the new allocation. */                 //减少其占所使用的新分配的部分的大小。
			/*【潘光珍】插槽仍在自由列表上。减少它的大小来说明新分配所使用的部分。*/
          put2byte(&data[pc+2], x);
        }
        *pIdx = pc + x;
        return SQLITE_OK; //返回SQLITE_OK
      }
    }
  }

  /* Check to make sure there is enough space in the gap to satisfy
  ** the allocation.  If not, defragment.
  ** 检查确认在gap中有足够的空间来满足分配的需要，如果空间不足，碎片整理。
  */
   /*
 【潘光珍】 检查间隙以确保有足够的空间来满足分配。如果没有，则整理。
  */
  testcase( gap+2+nByte==top );
  if( gap+2+nByte>top ){
    rc = defragmentPage(pPage);
    if( rc ) return rc;
    top = get2byteNotZero(&data[hdr+5]);
    assert( gap+nByte<=top );
  }

  /* Allocate memory from the gap in between the cell pointer array
  ** and the cell content area.  The btreeInitPage() call has already
  ** validated the freelist.  Given that the freelist is valid, there
  ** is no way that the allocation can extend off the end of the page.
  ** The assert() below verifies the previous sentence.
  ** 从单元指针数组和单元内容区域之间的间隙分配内存。该btreeInitPage（）
  ** 调用已经有有效的空闲列表。鉴于空闲列表是有效的，分配可以扩展超出页
  ** 是不行的。assert()下面验证了前面的语句。
  */
  /*
 【潘光珍】 分配存储器从单元指针阵列和单元格内容区之间的间隙中进行分配。
  btreeInitPage()调用了有效的空闲列表。由于数据是有效的，没有这样的配置可以延长页的结束。
   assert()在验证前边的语句。
  */
  top -= nByte;
  put2byte(&data[hdr+5], top);
  assert( top+nByte <= (int)pPage->pBt->usableSize );
  *pIdx = top;
  return SQLITE_OK;
}

/*
** Return a section of the pPage->aData to the freelist.
** The first byte of the new free block is pPage->aDisk[start]
** and the size of the block is "size" bytes.
** 新空闲块的第一个字节是pPage->aDisk[start]且块的字节大小是"size"字节。
** Most of the effort here is involved in coalesing adjacent
** free blocks into a single big free block.
** 返回pPage-> ADATA的部分到自由列表。从而在新的空闲块的第一字节是pPage-> aDisk[start]
** 和块的大小为“size”字节。这里的大多数功能涉及合并相邻空闲块成一个单独的大空闲块。
*/
/*释放pPage->aDisk[start]，大小为size字节的块*/
/*
【潘光珍】释放pPage->aDisk[start]，大小为size字节的块
这里大部分的精力放在coalesing相邻的空闲块成一个大的空闲块。

*/
static int freeSpace(MemPage *pPage, int start, int size){  //释放pPage->aData的部分并写入空闲列表
  int addr, pbegin, hdr;
  int iLast;                        /* Largest possible freeblock offset */   //最大的可能freeblock偏移 
  unsigned char *data = pPage->aData;

  assert( pPage->pBt!=0 );
  assert( sqlite3PagerIswriteable(pPage->pDbPage) );
  assert( start>=pPage->hdrOffset+6+pPage->childPtrSize );
  assert( (start + size) <= (int)pPage->pBt->usableSize );
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  assert( size>=0 );   /* Minimum cell size is 4 */  //【潘光珍】最小单元的大小为4

  if( pPage->pBt->btsFlags & BTS_SECURE_DELETE ){
    /* Overwrite deleted information with zeros when the secure_delete
    ** option is enabled */  //当secure_delete可用的时候，将删除信息置零。 /*【潘光珍】覆盖删除信息时，secure_delete零点选项启用*/
    memset(&data[start], 0, size);
  }

  /* Add the space back into the linked list of freeblocks.  Note that
  ** even though the freeblock list was checked by btreeInitPage(),
  ** btreeInitPage() did not detect overlapping cells or
  ** freeblocks that overlapped cells.   Nor does it detect when the
  ** cell content area exceeds the value in the page header.  If these
  ** situations arise, then subsequent insert operations might corrupt
  ** the freelist.  So we do need to check for corruption while scanning
  ** the freelist.
  ** 添加空间到空闲块的链表中。注意即使btreeInitPage()已检查过空闲块列表，btreeInitPage()也不能检测到重复单元或重复单元的空闲块。
  ** 当单元内容区域超出该页头的值时也不会检测。如果这种情况发生，那么随后的插入操作可能会破坏自由列表。
  ** 所以我们需要检查是否有损坏，同时扫描空闲列表。
  */ 
   /*
  【潘光珍】添加空间回到freeblocks链表。注意，即使freeblock名单是由btreeinitpage()，btreeinitpage()没有检测到cells
  或freeblocks重叠，重叠cells 。当单元格内容区域超过该页头中的值时，它也不检测。
  如果这些情况出现，那么后续的插入操作可能会损坏数据。所以我们需要在扫描时检查腐败自由列表。
  
  */
  hdr = pPage->hdrOffset;
  addr = hdr + 1;
  iLast = pPage->pBt->usableSize - 4;
  assert( start<=iLast );
  while( (pbegin = get2byte(&data[addr]))<start && pbegin>0 ){
    if( pbegin<addr+4 ){
      return SQLITE_CORRUPT_BKPT;
    }
    addr = pbegin;
  }
  if( pbegin>iLast ){
    return SQLITE_CORRUPT_BKPT;
  }
  assert( pbegin>addr || pbegin==0 );
  put2byte(&data[addr], start);
  put2byte(&data[start], pbegin);
  put2byte(&data[start+2], size);
  pPage->nFree = pPage->nFree + (u16)size;

  /* Coalesce adjacent free blocks */ //合并相邻的空闲块
  addr = hdr + 1;
  while( (pbegin = get2byte(&data[addr]))>0 ){
    int pnext, psize, x;
    assert( pbegin>addr );
    assert( pbegin <= (int)pPage->pBt->usableSize-4 );
    pnext = get2byte(&data[pbegin]);
    psize = get2byte(&data[pbegin+2]);
    if( pbegin + psize + 3 >= pnext && pnext>0 ){/*合并空闲块*/
      int frag = pnext - (pbegin+psize);
      if( (frag<0) || (frag>(int)data[hdr+7]) ){
        return SQLITE_CORRUPT_BKPT;
      }
      data[hdr+7] -= (u8)frag;
      x = get2byte(&data[pnext]);
      put2byte(&data[pbegin], x);
      x = pnext + get2byte(&data[pnext+2]) - pbegin;
      put2byte(&data[pbegin+2], x);
    }else{
      addr = pbegin;
    }
  }

  /* If the cell content area begins with a freeblock, remove it. */  //如果单元格内容区域以freeblock开始,删除它。
  /*【潘光珍】如果单元格内容区域开始是空闲块，则删除它。*/
  if( data[hdr+1]==data[hdr+5] && data[hdr+2]==data[hdr+6] ){
    int top;
    pbegin = get2byte(&data[hdr+1]);
    memcpy(&data[hdr+1], &data[pbegin], 2);
    top = get2byte(&data[hdr+5]) + get2byte(&data[pbegin+2]);
    put2byte(&data[hdr+5], top);
  }
  assert( sqlite3PagerIswriteable(pPage->pDbPage) );
  return SQLITE_OK;
}

/*
** Decode the flags byte (the first byte of the header) for a page
** and initialize fields of the MemPage structure accordingly.
** // 为一个页和MemPage相应结构的初始化区域解码标志字节。
** Only the following combinations are supported.  Anything different
** indicates a corrupt database files:
** //只支持以下组合。任何不同都是指示一个不良的数据文件
**         PTF_ZERODATA
**         PTF_ZERODATA | PTF_LEAF
**         PTF_LEAFDATA | PTF_INTKEY
**         PTF_LEAFDATA | PTF_INTKEY | PTF_LEAF
*/
/*【潘光珍】解码一页的标记字节（头部的第一个字节）初始化的MemPage相应结构域。
只有下面的组合支持。任何不同的指示一个损坏的数据库文件：
**         PTF_ZERODATA
**         PTF_ZERODATA | PTF_LEAF
**         PTF_LEAFDATA | PTF_INTKEY
**         PTF_LEAFDATA | PTF_INTKEY | PTF_LEAF
*/
static int decodeFlags(MemPage *pPage, int flagByte){
  BtShared *pBt;     /* A copy of pPage->pBt */   //pPage->pBt的一个副本

  assert( pPage->hdrOffset==(pPage->pgno==1 ? 100 : 0) );
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  pPage->leaf = (u8)(flagByte>>3);  assert( PTF_LEAF == 1<<3 );
  flagByte &= ~PTF_LEAF;
  pPage->childPtrSize = 4-4*pPage->leaf;
  pBt = pPage->pBt;
  if( flagByte==(PTF_LEAFDATA | PTF_INTKEY) ){
    pPage->intKey = 1;
    pPage->hasData = pPage->leaf;
    pPage->maxLocal = pBt->maxLeaf;
    pPage->minLocal = pBt->minLeaf;
  }else if( flagByte==PTF_ZERODATA ){
    pPage->intKey = 0;
    pPage->hasData = 0;
    pPage->maxLocal = pBt->maxLocal;
    pPage->minLocal = pBt->minLocal;
  }else{
    return SQLITE_CORRUPT_BKPT;
  }
  pPage->max1bytePayload = pBt->max1bytePayload;
  return SQLITE_OK;
}

/*
** Initialize the auxiliary information for a disk block.
** 初始化磁盘块的辅助信息。
** Return SQLITE_OK on success.  If we see that the page does
** not contain a well-formed database page, then return 
** SQLITE_CORRUPT.  Note that a return of SQLITE_OK does not
** guarantee that the page is well-formed.  It only shows that
** we failed to detect any corruption.
** 成功则返回SQLITE OK。如果我们看到页面不包含一个格式良好的数据库页面,然后返回
** SQLITE_CORRUPT。注意,SQLITE_OK的回归可以不保证页面的格式是正确的。它只表明我们失败了
*/
/*【潘光珍】初始化磁盘块的辅助信息。
返回sqlite_ok成功。
如果我们看到该页没有良好的数据库页面，然后返回sqlite_corrupt。
注意，返回sqlite_ok不保证页面是很好的。它只说明我们没有发现任何异常。*/
static int btreeInitPage(MemPage *pPage){     //B树初始化页

  assert( pPage->pBt!=0 );
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  assert( pPage->pgno==sqlite3PagerPagenumber(pPage->pDbPage) );
  assert( pPage == sqlite3PagerGetExtra(pPage->pDbPage) );
  assert( pPage->aData == sqlite3PagerGetData(pPage->pDbPage) );

  if( !pPage->isInit ){
    u16 pc;            /* Address of a freeblock within pPage->aData[] */      //pPage->aData[]内部的空闲块的地址
    u8 hdr;            /* Offset to beginning of page header */                //页头开始的偏移量/*【潘光珍】 对page 1为100，对其它页为0 */
    u8 *data;          /* Equal to pPage->aData */                             //等于pPage->aData
    BtShared *pBt;     /* The main btree structure */                          //可共享的B树结构
    int usableSize;    /* Amount of usable space on each page */      //每个页上的可用空间的数量 /* 【潘光珍】每页可用的字节数。pageSize-每页尾部保留空间的大小，在文件头偏移为20处设定。*/
																		
    u16 cellOffset;    /* Offset from start of page to first cell pointer */   //从页面的开始到第一个单元指针的偏移量/* 【潘光珍】单元指针数组的偏移量，aData中第1个单元的指针 */
    int nFree;         /* Number of unused bytes on the page */                //页上不能使用字节的数量  /* 【潘光珍】可使用空间的总和（字节数） */
    int top;           /* First byte of the cell content area */               //单元内容的第一个字节
    int iCellFirst;    /* First allowable cell or freeblock offset */          //第一个可用单元或空闲块偏移量
    int iCellLast;     /* Last possible cell or freeblock offset */            //最后一个可能单元或空闲块偏移量

    pBt = pPage->pBt;

    hdr = pPage->hdrOffset;
    data = pPage->aData;
    if( decodeFlags(pPage, data[hdr]) ) return SQLITE_CORRUPT_BKPT;
    assert( pBt->pageSize>=512 && pBt->pageSize<=65536 );
    pPage->maskPage = (u16)(pBt->pageSize - 1);
    pPage->nOverflow = 0;
    usableSize = pBt->usableSize;
    pPage->cellOffset = cellOffset = hdr + 12 - 4*pPage->leaf;
    pPage->aDataEnd = &data[usableSize];
    pPage->aCellIdx = &data[cellOffset];
    top = get2byteNotZero(&data[hdr+5]);
    pPage->nCell = get2byte(&data[hdr+3]);
    if( pPage->nCell>MX_CELL(pBt) ){
      /* To many cells for a single page.  The page must be corrupt */  //对于单页面的若干单元也一定是不良的
      return SQLITE_CORRUPT_BKPT;
    }
    testcase( pPage->nCell==MX_CELL(pBt) );

    /* A malformed(有缺陷的) database page might cause us to read past the end
    ** of page when parsing a cell.  
    ** 解析单元时，一个有缺陷的数据库页可能会导致我们去读页面末尾的部分。
    ** The following block of code checks early to see if a cell extends
    ** past the end of a page boundary and causes SQLITE_CORRUPT to be 
    ** returned if it does.
	** 下面的代码块将提前核对是否一个单元扩展超过页面边界，并且如果确实如此SQLITE_CORRUPT将被返回。
    */
	/*
	【潘光珍】 有缺陷的数据库错误页面可能会为过去的读结束时,进行分析一个单元格。
	下面的代码检查块，看看是否有一个单元格在过去的最后一页的边界。
	并且如果原因是 SQLITE_CORRUPT，则它是返回的。
	*/
    iCellFirst = cellOffset + 2*pPage->nCell;
    iCellLast = usableSize - 4;
#if defined(SQLITE_ENABLE_OVERSIZE_CELL_CHECK)
    {
      int i;            /* Index into the cell pointer array */   //到单元指针数组的索引 /*【潘光珍】定义一个索引到单元格数组的指针的变量i*/
      int sz;           /* Size of a cell */      //单元的大小

      if( !pPage->leaf ) iCellLast--;
      for(i=0; i<pPage->nCell; i++){
        pc = get2byte(&data[cellOffset+i*2]);
        testcase( pc==iCellFirst );
        testcase( pc==iCellLast );
        if( pc<iCellFirst || pc>iCellLast ){
          return SQLITE_CORRUPT_BKPT;
        }
        sz = cellSizePtr(pPage, &data[pc]);
        testcase( pc+sz==usableSize );
        if( pc+sz>usableSize ){
          return SQLITE_CORRUPT_BKPT;
        }
      }
      if( !pPage->leaf ) iCellLast++;
    }  
#endif

    /* Compute the total free space on the page */  //计算页面上自由空间的总量 /*【潘光珍】计算页面上的总空闲空间*/
    pc = get2byte(&data[hdr+1]);
    nFree = data[hdr+7] + top;
    while( pc>0 ){
      u16 next, size;
      if( pc<iCellFirst || pc>iCellLast ){
        /* Start of free block is off the page */  //空闲块的开始不在页面上 /*【潘光珍】启动空闲块是关闭页面*/
        return SQLITE_CORRUPT_BKPT; 
      }
      next = get2byte(&data[pc]);
      size = get2byte(&data[pc+2]);
      if( (next>0 && next<=pc+size+3) || pc+size>usableSize ){
        /* Free blocks must be in ascending order. And the last byte of
        ** the free-block must lie on the database page.  
		** 空闲块必须是一个地鞥的顺序。并且空闲开的最后一个字节一定是在一个数据库页上的*/
		  /*【潘光珍】空闲块必须按升序排列。和空闲块的最后一个字节必须位于数据库页*/
        return SQLITE_CORRUPT_BKPT; 
      }
      nFree = nFree + size;
      pc = next;
    }

    /* At this point, nFree contains the sum of the offset to the start
    ** of the cell-content area plus the number of free bytes within
    ** the cell-content area. If this is greater than the usable-size
    ** of the page, then the page must be corrupted. This check also
    ** serves to verify that the offset to the start of the cell-content
    ** area, according to the page header, lies within the page.
	** 此时，nFree包含偏移量的总量，偏移量是单元内容区开始部分加上单元内容区内的空闲字节的数量的和。
	** 如果这比页面的可用大小更大，则该页面必须被破坏。根据页头，此检查还用于验证位于该页面内的单元内容区开始部分的偏移量。
    */
	/*
	【潘光珍】**在这一点上，nFree包含偏移的总和的内容区域开始加单元格内容范围内可用的字节数。
	如果这是大于页面的可用大小，则页面必须被损坏。
	此检查也可用于验证该单元格内容区域的起始偏移量，根据该页头，位于该页中。
	*/
    if( nFree>usableSize ){
      return SQLITE_CORRUPT_BKPT; 
    }
    pPage->nFree = (u16)(nFree - iCellFirst);
    pPage->isInit = 1;
  }
  return SQLITE_OK;
}

/*
** Set up a raw page so that it looks like a database page holding
** no entries.  //建立一个原始页面,以便它看起来像一个数据库没有条目。
*/
/*【潘光珍】设置一个原始页面，这样它看起来像一个数据库页，没有任何条目。*/
static void zeroPage(MemPage *pPage, int flags){
  unsigned char *data = pPage->aData;
  BtShared *pBt = pPage->pBt;
  u8 hdr = pPage->hdrOffset;//指向page1
  u16 first;

  assert( sqlite3PagerPagenumber(pPage->pDbPage)==pPage->pgno );
  assert( sqlite3PagerGetExtra(pPage->pDbPage) == (void*)pPage );
  assert( sqlite3PagerGetData(pPage->pDbPage) == data );
  assert( sqlite3PagerIswriteable(pPage->pDbPage) );
  assert( sqlite3_mutex_held(pBt->mutex) );
  if( pBt->btsFlags & BTS_SECURE_DELETE ){
    memset(&data[hdr], 0, pBt->usableSize - hdr);
  }
  data[hdr] = (char)flags;
  first = hdr + 8 + 4*((flags&PTF_LEAF)==0 ?1:0);
  memset(&data[hdr+1], 0, 4);
  data[hdr+7] = 0;
  put2byte(&data[hdr+5], pBt->usableSize);
  pPage->nFree = (u16)(pBt->usableSize - first);
  decodeFlags(pPage, flags);
  pPage->hdrOffset = hdr;
  pPage->cellOffset = first;
  pPage->aDataEnd = &data[pBt->usableSize];
  pPage->aCellIdx = &data[first];
  pPage->nOverflow = 0;
  assert( pBt->pageSize>=512 && pBt->pageSize<=65536 );
  pPage->maskPage = (u16)(pBt->pageSize - 1);
  pPage->nCell = 0;
  pPage->isInit = 1;
}


/*
** Convert a DbPage obtained from the pager into a MemPage used by
** the btree layer.  //通过B树层，将DbPage转化成MemPage
*/
/*convert DbPage into MemPage*//*【潘光珍】将从pager中获得的DbPage转化为btree中使用的MemPage*/
static MemPage *btreePageFromDbPage(DbPage *pDbPage, Pgno pgno, BtShared *pBt){
  MemPage *pPage = (MemPage*)sqlite3PagerGetExtra(pDbPage);
  pPage->aData = sqlite3PagerGetData(pDbPage);
  pPage->pDbPage = pDbPage;
  pPage->pBt = pBt;
  pPage->pgno = pgno;
  pPage->hdrOffset = pPage->pgno==1 ? 100 : 0;
  return pPage; 
}

/*
** Get a page from the pager.  Initialize the MemPage.pBt and
** MemPage.aData elements if needed.
** 从页对象得到一个页。如果需要，则初始化MemPage.pBt和MemPage.aData的元素
** If the noContent flag is set, it means that we do not care about
** the content of the page at this time.  So do not go to the disk
** to fetch the content.  Just fill in the content with zeros for now.
** If in the future we call sqlite3PagerWrite() on this page, that
** means we have started to be concerned about content and the disk
** read should occur at that point.
** 如果无内容标签设定了，那意味着我们将不关心此时的页面内容。所以不要去磁盘获取内容。只需在内容中填写使用零即可。
** 如果以后我们在这个页面上调用sqlite3PagerWrite（），这意味着我们已经开始关注内容，并应出现在该点的磁盘读取。*/
/*
【潘光珍】**如果需要，则进行初始化 MemPage.pBt和MemPage.aData的元素
**如果noContent标志设置，这意味着我们不在乎此时的页面内容。所以不要去磁盘获取内容。只需填写内容与零现在。
**如果将来我们调用sqlite3pagerwrite()这个页面上，这意味着我们已经开始关注内容和读盘应该发生在那一点。

*/
static int btreeGetPage(
  BtShared *pBt,       /* The btree */                          //B树
  Pgno pgno,           /* Number of the page to fetch */        //获取的页面数  /*【潘光珍】本页的页号*/
  MemPage **ppPage,    /* Return the page in this parameter */  //用这个参数返回页  /*【潘光珍】返回此参数中的页*/
  int noContent        /* Do not load page content if true */   //如果为真，则不会加载页   /*【潘光珍】如果真的,不要加载页面内容*/
){
  int rc;
  DbPage *pDbPage;

  assert( sqlite3_mutex_held(pBt->mutex) );
  rc = sqlite3PagerAcquire(pBt->pPager, pgno, (DbPage**)&pDbPage, noContent);
  if( rc ) return rc; 
  *ppPage = btreePageFromDbPage(pDbPage, pgno, pBt);/*从pager中获取page，放在ppPage中*/
  return SQLITE_OK;
}

/*
** Retrieve a page from the pager cache. If the requested page is not
** already in the pager cache return NULL. Initialize the MemPage.pBt and
** MemPage.aData elements if needed.
** 从页对象缓存检索一个页面。如果没有而返回NULL。若有必要，初始化MemPage.pBt和MemPage.aData的元素*/
/*【潘光珍】从缓存页检索。如果请求的页不在缓存返回null。如果需要初始化mempage.pbt和mempage.adata元素*/
static MemPage *btreePageLookup(BtShared *pBt, Pgno pgno){
  DbPage *pDbPage;
  assert( sqlite3_mutex_held(pBt->mutex) );
  pDbPage = sqlite3PagerLookup(pBt->pPager, pgno);
  if( pDbPage ){
    return btreePageFromDbPage(pDbPage, pgno, pBt);/*从pager cache中取page*/
  }
  return 0;
}

/*
** Return the size of the database file in pages. If there is any kind of
** error, return ((unsigned int)-1).
** 返回页中数据库文件的大小。若有错return ((unsigned int)-1)*/
/*【潘光珍】返回数据库文件的大小。如果有任何错误,则返回((unsigned int)-1)*/
static Pgno btreePagecount(BtShared *pBt){
  return pBt->nPage;
}
u32 sqlite3BtreeLastPage(Btree *p){
  assert( sqlite3BtreeHoldsMutex(p) );
  assert( ((p->pBt->nPage)&0x8000000)==0 );
  return (int)btreePagecount(p->pBt);//强制转换，返回btree的总页数
}

/*
** Get a page from the pager and initialize it.  This routine is just a
** convenience wrapper around separate calls to btreeGetPage() and 
** btreeInitPage().
** 从页对象中获得一个页面并初始化。这个程序只一个关于分别调用btreeGetPage（）和btreeInitPage（）的便捷的包。
** If an error occurs, then the value *ppPage is set to is undefined. It
** may remain unchanged, or it may be set to an invalid value.
** 如果发生错误，则该值* ppPage被设置为未定义。它可以保持不变，或者它可以被设置为无效值。*/
/*
【潘光珍】**初始化，这个程序仅仅是一个方便的包装，单独调用btreegetpage()和btreeinitpage()。
**如果出现错误，那么值* pppage将是未定义的。它可以保持不变，或可能被设置为无效值。
*/
static int getAndInitPage(
  BtShared *pBt,          /* The database file */         //数据库文件
  Pgno pgno,           /* Number of the page to get */    //获得的页面的数量 /*【潘光珍】获得本页的页号*/
  MemPage **ppPage     /* Write the page pointer here */  //在该变量上写指针
){
  int rc;
  assert( sqlite3_mutex_held(pBt->mutex) );

  if( pgno>btreePagecount(pBt) ){
    rc = SQLITE_CORRUPT_BKPT;
  }else{
    rc = btreeGetPage(pBt, pgno, ppPage, 0); /*Get a page from the pager*/
    if( rc==SQLITE_OK ){
      rc = btreeInitPage(*ppPage);/*初始化page*/
      if( rc!=SQLITE_OK ){/*ppPage的值未被定义。它的值可能未变化或者为无效值。*/
        releasePage(*ppPage);//释放page
      }
    }
  }

  testcase( pgno==0 );
  assert( pgno!=0 || rc==SQLITE_CORRUPT );
  return rc;
}

/*
** Release a MemPage.  This should be called once for each prior
** call to btreeGetPage.
** 每次调用之前应该被调用btreeGetPage一次。*/
/*释放内存页*/
static void releasePage(MemPage *pPage){
  if( pPage ){
    assert( pPage->aData );
    assert( pPage->pBt );
    assert( sqlite3PagerGetExtra(pPage->pDbPage) == (void*)pPage );
    assert( sqlite3PagerGetData(pPage->pDbPage)==pPage->aData );
    assert( sqlite3_mutex_held(pPage->pBt->mutex) );
    sqlite3PagerUnref(pPage->pDbPage);/*释放页上的引用*/
  }
}

/*
** During a rollback, when the pager reloads(重新装) information into the cache
** so that the cache is restored to its original state at the start of
** the transaction, for each page restored this routine is called.
** 在回滚期间，当pager对象重新装载信息到缓存以便于缓存恢复到事务开始的缓存原始状态的时候，对于每个页面恢复都调用这个程序。
** This routine needs to reset the extra data section at the end of the
** page to agree with the restored data.
** 这个程序需要在页面的最后重新设定额外的数据部分以适合恢复数据 */
/*回滚后，页重新装information到cache。*/

/*
【潘光珍】**回滚后，页重新装information到cache。因此，在事务开始时，该缓存将恢复到它的原始状态，
对于每一个页面都恢复了这个程序的调用。
**该程序需要重置页面的额外数据段以与恢复的数据一致。
*/
static void pageReinit(DbPage *pData){    //pager对象重新装载信息到缓存
  MemPage *pPage;
  pPage = (MemPage *)sqlite3PagerGetExtra(pData);
  assert( sqlite3PagerPageRefcount(pData)>0 );
  if( pPage->isInit ){
    assert( sqlite3_mutex_held(pPage->pBt->mutex) );
    pPage->isInit = 0;
    if( sqlite3PagerPageRefcount(pData)>1 ){
      /* pPage might not be a btree page;  it might be an overflow page
      ** or ptrmap page or a free page.  In those cases, the following  //pPage可能不是一个B树页面;它可能是一个溢出页面或ptrmap页或空白页
      ** call to btreeInitPage() will likely return SQLITE_CORRUPT.     //在这些情况下，下面对btreeInitPage()的调用可能返回SQLITE_CORRUPT。
      ** But no harm is done by this.  And it is very important that    //在每一个B树页上调用btreeInitPage()是很重要的，
      ** btreeInitPage() be called on every btree page so we make       //因此我们为每一个重新初始化的每一个页面发出调用请求。
      ** the call for every page that comes in for re-initing. 
	  */	
	/*【潘光珍】页可能不是Btree页；它可能是一个溢出页或ptrmap页或一个空闲的主页。
		在这种情况下，下面的调用会返回sqlite_corrupt btreeinitpage()。但是没有害处的。
		这是非常重要的，btreeinitpage()被每个B树页调用所以我们做出的每一页，都在重新初始化调用。
		
		*/
      btreeInitPage(pPage);
    }
  }
}

/*
** Invoke the busy handler for a btree.      //调用btree繁忙的处理程序。
*/
static int btreeInvokeBusyHandler(void *pArg){
  BtShared *pBt = (BtShared*)pArg;
  assert( pBt->db );
  assert( sqlite3_mutex_held(pBt->db->mutex) );
  return sqlite3InvokeBusyHandler(&pBt->db->busyHandler);/*【潘光珍】调用一个btree繁忙的处理程序。*/
}

/*
** Open a database file.             //打开数据库文件
** 
** zFilename is the name of the database file.  If zFilename is NULL             
** then an ephemeral(短暂的) database is created.  The ephemeral database might 
** be exclusively in memory, or it might use a disk-based memory cache. 
** Either way, the ephemeral database will be automatically deleted              
** when sqlite3BtreeClose() is called. 
** zFilename是这个数据库文件的名字。如果zFilename为空，则将创建一个临时数据库。
** 这个临时数据库在内存中唯一的，或用了基于磁盘的内存缓存无论哪种方式当sqlite3BtreeClose()被调用的时候，
** 这个临时数据库将自动删除。
**
** If zFilename is ":memory:" then an in-memory database is created  
** that is automatically destroyed when it is closed.
** 如果zFilename是":memory:"那么关闭时自动销毁的内存数据库将会创建。
**
** The "flags" parameter is a bitmask that might contain bits like
** BTREE_OMIT_JOURNAL and/or BTREE_MEMORY.  
** “flags”参数是一个可能包含的位掩码位BTREE_OMIT_JOURNAL、BTREE_MEMORY。

** If the database is already opened in the same database connection
** and we are in shared cache mode, then the open will fail with an
** SQLITE_CONSTRAINT error.  We cannot allow two or more BtShared
** objects in the same database connection since doing so will lead
** to problems with locking.
** 如果数据库已经在相同的数据库连接中打开了并且在共享缓存模式下,然后用一个打开将会失败返回SQLITE_CONSTRAINT错误。
** 在同一数据库连接中我们不能允许两个或多个BtShared对象，因为这样做会导致锁问题。
*/
/*【潘光珍】打开数据库文件。
**zfilename是数据库文件的名称。如果zFilename是NULL，则创建一个短暂的数据库。
短暂的数据库可能是专门在内存中，或者它可以使用基于磁盘的内存缓存。无论哪种方式，
短暂的数据库将自动删除当调用sqlite3BtreeClose()时。
**如果zFilename是":memory:"那么一个内存数据库的创建，关闭时自动销毁。
**flags参数的位掩码可能包含位像BTREE_OMIT_JOURNAL and/or BTREE_MEMORY。
**如果数据库在同一个数据库连接中已打开是我们在共享缓存模式，然后打开将失败与sqlite_constraint误差。
我们不能允许两个或两个以上的btshared在同一数据库连接中的对象，因为这样做将导致锁定问题。
*/
int sqlite3BtreeOpen(     //打开数据库文件并返回B树对象
  sqlite3_vfs *pVfs,      /* VFS to use for this b-tree */                      //VFS使用B树
  const char *zFilename,  /* Name of the file containing the BTree database */  //包含B树数据库文件的名字
  sqlite3 *db,            /* Associated database handle */                      //相关数据库句柄
  Btree **ppBtree,        /* Pointer to new Btree object written here */        //指向在此被写的新的B树对象
  int flags,              /* Options */                                         //选项标签
  int vfsFlags            /* Flags passed through to sqlite3_vfs.xOpen() */     //通过sqlite3_vfs.xOpen()标记
){
  BtShared *pBt = 0;             /* Shared part of btree structure */           //B树结构的共享部分
  Btree *p;                      /* Handle to return */                         //返回的句柄
  sqlite3_mutex *mutexOpen = 0;  /* Prevents a race condition. Ticket #3537 */  //避免竞态条件。标签#3537/*【潘光珍】防止竞争条件。标签#3537*/
  int rc = SQLITE_OK;            /* Result code from this function */           //这个函数的状态码/*【潘光珍】此函数的结果代码*/
  u8 nReserve;                   /* Byte of unused space on each page */        //每个页上的不用空间的字节数
  unsigned char zDbHeader[100];  /* Database header content */                  //数据库文件头内容

  /* True if opening an ephemeral, temporary database */
  const int isTempDb = zFilename==0 || zFilename[0]==0;/*zFilename为空表示是临时数据库*/

  /* Set the variable isMemdb to true for an in-memory database, or 
  ** false for a file-based database.
  ** 对于内存数据库设置变量isMemdb真，对于基于文件的数据库变量isMemdb设为假。
  */
   /*
  【潘光珍】设置变量ismemdb真正为一个内存数据库，或虚假的一个基于文件的数据库。
  */
#ifdef SQLITE_OMIT_MEMORYDB
  const int isMemdb = 0;
#else
  const int isMemdb = (zFilename && strcmp(zFilename, ":memory:")==0)//【潘光珍】zFilename为":memory:"，所有信息都放到缓冲区中，不会被写入磁盘。
                       || (isTempDb && sqlite3TempInMemory(db))
                       || (vfsFlags & SQLITE_OPEN_MEMORY)!=0;
#endif

  assert( db!=0 );
  assert( pVfs!=0 );
  assert( sqlite3_mutex_held(db->mutex) );
  assert( (flags&0xff)==flags );   /* flags fit in 8 bits */    //标记占8个字节

  /* Only a BTREE_SINGLE database can be BTREE_UNORDERED */  /*【潘光珍】只有一个btree_single数据库可以是btree_unordered*/
  assert( (flags & BTREE_UNORDERED)==0 || (flags & BTREE_SINGLE)!=0 );

  /* A BTREE_SINGLE database is always a temporary and/or ephemeral */  //BTREE_SINGLE数据库总是临时的。
  assert( (flags & BTREE_SINGLE)==0 || isTempDb );

  if( isMemdb ){
    flags |= BTREE_MEMORY;
  }
  if( (vfsFlags & SQLITE_OPEN_MAIN_DB)!=0 && (isMemdb || isTempDb) ){
    vfsFlags = (vfsFlags & ~SQLITE_OPEN_MAIN_DB) | SQLITE_OPEN_TEMP_DB;
  }
  p = sqlite3MallocZero(sizeof(Btree));
  if( !p ){
    return SQLITE_NOMEM;
  }
  p->inTrans = TRANS_NONE;
  p->db = db;
#ifndef SQLITE_OMIT_SHARED_CACHE
  p->lock.pBtree = p;
  p->lock.iTable = 1;
#endif

#if !defined(SQLITE_OMIT_SHARED_CACHE) && !defined(SQLITE_OMIT_DISKIO)
  /*
  ** If this Btree is a candidate for shared cache, try to find an
  ** existing BtShared object that we can share with
  ** 如果这Btree共享缓存是候选的,尝试找到一个可共享的存在的BtShared对象。
  */
  /*【潘光珍】如果这B树是一个共享缓存的候选,则试图找到一个现有的btshared对象可以让我们分享*/
  if( isTempDb==0 && (isMemdb==0 || (vfsFlags&SQLITE_OPEN_URI)!=0) ){
    if( vfsFlags & SQLITE_OPEN_SHAREDCACHE ){
      int nFullPathname = pVfs->mxPathname+1;
      char *zFullPathname = sqlite3Malloc(nFullPathname);
      MUTEX_LOGIC( sqlite3_mutex *mutexShared; )
      p->sharable = 1;
      if( !zFullPathname ){
        sqlite3_free(p);
        return SQLITE_NOMEM;
      }
      if( isMemdb ){
        memcpy(zFullPathname, zFilename, sqlite3Strlen30(zFilename)+1);
      }else{
        rc = sqlite3OsFullPathname(pVfs, zFilename,
                                   nFullPathname, zFullPathname);
        if( rc ){
          sqlite3_free(zFullPathname);
          sqlite3_free(p);
          return rc;
        }
      }
#if SQLITE_THREADSAFE
      mutexOpen = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_OPEN);
      sqlite3_mutex_enter(mutexOpen);
      mutexShared = sqlite3MutexAlloc(
	  	);
      sqlite3_mutex_enter(mutexShared);
#endif
      for(pBt=GLOBAL(BtShared*,sqlite3SharedCacheList); pBt; pBt=pBt->pNext){
        assert( pBt->nRef>0 );
        if( 0==strcmp(zFullPathname, sqlite3PagerFilename(pBt->pPager, 0))
                 && sqlite3PagerVfs(pBt->pPager)==pVfs ){
          int iDb;
          for(iDb=db->nDb-1; iDb>=0; iDb--){
            Btree *pExisting = db->aDb[iDb].pBt;
            if( pExisting && pExisting->pBt==pBt ){/*在同一个共享cache模式下，在相同数据库连接中，数据库是打开的，返回SQLITE_CONSTRAINT*/
              sqlite3_mutex_leave(mutexShared);
              sqlite3_mutex_leave(mutexOpen);
              sqlite3_free(zFullPathname);
              sqlite3_free(p);
              return SQLITE_CONSTRAINT;
            }
          }
          p->pBt = pBt;
          pBt->nRef++;
          break;
        }
      }
      sqlite3_mutex_leave(mutexShared);
      sqlite3_free(zFullPathname);
    }
#ifdef SQLITE_DEBUG
    else{
      /* In debug mode, we mark all persistent databases as sharable
      ** even when they are not.  This exercises the locking code and
      ** gives more opportunity for asserts(sqlite3_mutex_held())
      ** statements to find locking problems.
	  ** 在调试模式下,我们将所有持久化数据库标记为可共享的，即使他们不是持久化的。这练习锁定代码
	  ** 和asserts(sqlite3_mutex_held())语句给更多的机会找到锁问题。
      */

/*【潘光珍】在调试模式下，我们标记所有持续数据库，即使他们不共享。提供更多的asserts(sqlite3_mutex_held())语句来找到锁定问题。
*/
      p->sharable = 1;
    }
#endif
  }
#endif
  if( pBt==0 ){
    /*
    ** The following asserts make sure that structures used by the btree are
    ** the right size.  This is to guard against size changes that result
    ** when compiling on a different architecture.
	** 下面的断言是确保B树使用的结构的大小是正确的。这是为了防止编译不同的架构时大小变化的结果。
    */
	  /*
	  【潘光珍】以下断言确保使用的B树结构正确的大小。这是在一个不同的体系结构编译时，对结果的大小变化进行保护。
	  */
    assert( sizeof(i64)==8 || sizeof(i64)==4 );
    assert( sizeof(u64)==8 || sizeof(u64)==4 );
    assert( sizeof(u32)==4 );
    assert( sizeof(u16)==2 );
    assert( sizeof(Pgno)==4 );
  
    pBt = sqlite3MallocZero( sizeof(*pBt) );
    if( pBt==0 ){
      rc = SQLITE_NOMEM;
      goto btree_open_out;
    }
    rc = sqlite3PagerOpen(pVfs, &pBt->pPager, zFilename,
                          EXTRA_SIZE, flags, vfsFlags, pageReinit);
    if( rc==SQLITE_OK ){
      rc = sqlite3PagerReadFileheader(pBt->pPager,sizeof(zDbHeader),zDbHeader);
    }
    if( rc!=SQLITE_OK ){
      goto btree_open_out;
    }
    pBt->openFlags = (u8)flags;
    pBt->db = db;
    sqlite3PagerSetBusyhandler(pBt->pPager, btreeInvokeBusyHandler, pBt);
    p->pBt = pBt;
  
    pBt->pCursor = 0;
    pBt->pPage1 = 0;
    if( sqlite3PagerIsreadonly(pBt->pPager) ) pBt->btsFlags |= BTS_READ_ONLY;
#ifdef SQLITE_SECURE_DELETE
    pBt->btsFlags |= BTS_SECURE_DELETE;
#endif
    pBt->pageSize = (zDbHeader[16]<<8) | (zDbHeader[17]<<16);
    if( pBt->pageSize<512 || pBt->pageSize>SQLITE_MAX_PAGE_SIZE
         || ((pBt->pageSize-1)&pBt->pageSize)!=0 ){
      pBt->pageSize = 0;
#ifndef SQLITE_OMIT_AUTOVACUUM
      /* If the magic name ":memory:" will create an in-memory database, then
      ** leave the autoVacuum mode at 0 (do not auto-vacuum), even if
      ** SQLITE_DEFAULT_AUTOVACUUM is true. On the other hand, if
      ** SQLITE_OMIT_MEMORYDB has been defined, then ":memory:" is just a
      ** regular file-name. In this case the auto-vacuum applies as per normal.
	  ** 如果magic名为":memory:"，将创建一个内存中的数据库,然后使autoVacuum模式为0(即不要auto-vacuum)
	  ** 即使SQLITE_DEFAULT_AUTOVACUUM值为真。另一方面，如果SQLITE_OMIT_MEMORYDB已经被定义，则":memory:"
	  ** 是一个规则的文件名。此种情况下，auto-vacuum正常使用。
      */
	  /*
	  **如果这个magic为 ":memory:"将创建一个内存数据库，然后把autovacuum模式0（不设置auto-vacuum），
        即使sqlite_default_autovacuum为真。另一方面，如果sqlite_omit_memorydb已经被定义，
		那么":memory:"只是一个普通的文件名。在这种情况下，auto-vacuum适用正常。
	  */
      if( zFilename && !isMemdb ){
        pBt->autoVacuum = (SQLITE_DEFAULT_AUTOVACUUM ? 1 : 0);
        pBt->incrVacuum = (SQLITE_DEFAULT_AUTOVACUUM==2 ? 1 : 0);
      }
#endif
      nReserve = 0;
    }else{
      nReserve = zDbHeader[20];
      pBt->btsFlags |= BTS_PAGESIZE_FIXED;
#ifndef SQLITE_OMIT_AUTOVACUUM
      pBt->autoVacuum = (get4byte(&zDbHeader[36 + 4*4])?1:0);
      pBt->incrVacuum = (get4byte(&zDbHeader[36 + 7*4])?1:0);
#endif
    }
    rc = sqlite3PagerSetPagesize(pBt->pPager, &pBt->pageSize, nReserve);
    if( rc ) goto btree_open_out;
    pBt->usableSize = pBt->pageSize - nReserve;
    assert( (pBt->pageSize & 7)==0 );  /* 8-byte alignment of pageSize *//*8字节平衡的页大小*/
   
#if !defined(SQLITE_OMIT_SHARED_CACHE) && !defined(SQLITE_OMIT_DISKIO)
    /* Add the new BtShared object to the linked list sharable BtShareds.
    ** 添加新的BtShared对象到可共享的BtShareds的链表*/
    if( p->sharable ){
      MUTEX_LOGIC( sqlite3_mutex *mutexShared; )
      pBt->nRef = 1;
      MUTEX_LOGIC( mutexShared = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER);)
      if( SQLITE_THREADSAFE && sqlite3GlobalConfig.bCoreMutex ){
        pBt->mutex = sqlite3MutexAlloc(SQLITE_MUTEX_FAST);?        if( pBt->mutex==0 ){
          rc = SQLITE_NOMEM;
          db->mallocFailed = 0;
          goto btree_open_out;
        }
      }
      sqlite3_mutex_enter(mutexShared);
      pBt->pNext = GLOBAL(BtShared*,sqlite3SharedCacheList);
      GLOBAL(BtShared*,sqlite3SharedCacheList) = pBt;//添加新的BtShared对象到可共享的BtShareds的链表
      sqlite3_mutex_leave(mutexShared);
    }
#endif
  }
#if !defined(SQLITE_OMIT_SHARED_CACHE) && !defined(SQLITE_OMIT_DISKIO)
  /* If the new Btree uses a sharable pBtShared, then link the new
  ** Btree into the list of all sharable Btrees for the same connection.
  ** The list is kept in ascending order by pBt address.
  ** 如果新的Btree使用可共享pBtShared,那么对于相同的连接，链接新B树到所有可共享Btree的列表。列表pBt的地址递增有序。
  */
  /*
 【潘光珍】 如果新的B树使用一个共享的pBtShared，然后链接新的B树的所有共享Btrees列表相同的连接。
  该列表保存在上升的PBT地址顺序。
  */
  if( p->sharable ){
    int i;
    Btree *pSib;
    for(i=0; i<db->nDb; i++){
      if( (pSib = db->aDb[i].pBt)!=0 && pSib->sharable ){
        while( pSib->pPrev ){ pSib = pSib->pPrev; }
        if( p->pBt<pSib->pBt ){ /*将所有可共享B树的连接在一起*/
          p->pNext = pSib;
          p->pPrev = 0;
          pSib->pPrev = p;
        }else{
          while( pSib->pNext && pSib->pNext->pBt<p->pBt ){
            pSib = pSib->pNext;
          }
          p->pNext = pSib->pNext;
          p->pPrev = pSib;
          if( p->pNext ){
            p->pNext->pPrev = p;
          }
          pSib->pNext = p;
        }
        break;
      }
    }
  }
#endif
  *ppBtree = p;

btree_open_out:
  if( rc!=SQLITE_OK ){
    if( pBt && pBt->pPager ){
      sqlite3PagerClose(pBt->pPager);
    }
    sqlite3_free(pBt);
    sqlite3_free(p);
    *ppBtree = 0;
  }else{
    /* If the B-Tree was successfully opened, set the pager-cache size to the
    ** default value. Except, when opening on an existing shared pager-cache,
    ** do not change the pager-cache size.
	** 如果B树打开成功，则设置页面缓存的大小为默认值。除此之外，当在一个已存在的可共享页面缓存上打开时，不要改变页面缓存的大小。
    */
	  /*
	  【潘光珍】如果B树被成功打开，设置缓存大小的默认值。只是，当打开一个现有的共享缓存，不改变缓存大小。
	  */
    if( sqlite3BtreeSchema(p, 0, 0)==0 ){
      sqlite3PagerSetCachesize(p->pBt->pPager, SQLITE_DEFAULT_CACHE_SIZE);
    }
  }
  if( mutexOpen ){
    assert( sqlite3_mutex_held(mutexOpen) );
    sqlite3_mutex_leave(mutexOpen);
  }
  return rc;
}

/*
** Decrement the BtShared.nRef counter.  When it reaches zero,
** remove the BtShared structure from the sharing list.  Return
** true if the BtShared.nRef counter reaches zero and return
** false if it is still positive.
** 递减BtShared.nRef计数器。当它达到零时，从共享列表中删除BtShared结构。
** 如果BtShared.nRef计数器达到零返回true，如果它仍然为正并返回false。
*/
/*
【潘光珍】BtShared.nRef递减计数器。当它到达零，从共享列表中删除BtShared结构。
如果BtShared.nRef计数器达到零，返回正确并且如果它仍然是正，则返回错误。
*/
static int removeFromSharingList(BtShared *pBt){
#ifndef SQLITE_OMIT_SHARED_CACHE
  MUTEX_LOGIC( sqlite3_mutex *pMaster; )
  BtShared *pList;
  int removed = 0;

  assert( sqlite3_mutex_notheld(pBt->mutex) );
  MUTEX_LOGIC( pMaster = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER); )
  sqlite3_mutex_enter(pMaster);
  pBt->nRef--;
  if( pBt->nRef<=0 ){
    if( GLOBAL(BtShared*,sqlite3SharedCacheList)==pBt ){
      GLOBAL(BtShared*,sqlite3SharedCacheList) = pBt->pNext;/*若BtShared.nRef counter降为0，将pBt从分享列表中删除。*/
    }else{
      pList = GLOBAL(BtShared*,sqlite3SharedCacheList);
      while( ALWAYS(pList) && pList->pNext!=pBt ){
        pList=pList->pNext;
      }
      if( ALWAYS(pList) ){
        pList->pNext = pBt->pNext;
      }
    }
    if( SQLITE_THREADSAFE ){
      sqlite3_mutex_free(pBt->mutex);
    }
    removed = 1;
  }
  sqlite3_mutex_leave(pMaster);
  return removed;
#else
  return 1;
#endif
}

/*
** Make sure pBt->pTmpSpace points to an allocation of 
** MX_CELL_SIZE(pBt) bytes.
** 确保pBt->pTmpSpace指向一个MX_CELL_SIZE(pBt)字节的分配
*/
static void allocateTempSpace(BtShared *pBt){
  if( !pBt->pTmpSpace ){
    pBt->pTmpSpace = sqlite3PageMalloc( pBt->pageSize );/*确保pBt->pTmpSpace指向MX_CELL_SIZE(pBt)字节*/
  }
}

/*
** Free the pBt->pTmpSpace allocation  //释放pBt->pTmpSpace分配
*/
static void freeTempSpace(BtShared *pBt){
  sqlite3PageFree( pBt->pTmpSpace);
  pBt->pTmpSpace = 0;//【潘光珍】将pBt->pTmpSpace分配释放
}

/*
** Close an open database and invalidate all cursors. //关闭已打开的数据库并且使游标无效
*/
int sqlite3BtreeClose(Btree *p){/*【潘光珍】定义一个关闭已打开的数据库和无效的所有游标的函数。*/
  BtShared *pBt = p->pBt;
  BtCursor *pCur;

  /* Close all cursors opened via this handle.  */  //通过这句柄关闭所有打开的游标。
  assert( sqlite3_mutex_held(p->db->mutex) );
  sqlite3BtreeEnter(p);
  pCur = pBt->pCursor;
  while( pCur ){
    BtCursor *pTmp = pCur;
    pCur = pCur->pNext;
    if( pTmp->pBtree==p ){		
      sqlite3BtreeCloseCursor(pTmp);/* 使所有游标无效 *//* 【潘光珍】调用使所有游标无效的函数 */
    }
  }

  /* Rollback any active transaction and free the handle structure.
  ** The call to sqlite3BtreeRollback() drops any table-locks held by
  ** this handle.
  ** 回滚任何活动事务并且释放句柄结构。调用sqlite3BtreeRollback()，删除被这个句柄持有的任何锁标。
  */
  /*【潘光珍】回滚任何活动事务，并释放句柄结构.调用sqlite3BtreeRollback()，删除这个句柄所持有的所有表锁。*/
  sqlite3BtreeRollback(p, SQLITE_OK);/*删除了这个句柄上所持有的所有表锁*/
  sqlite3BtreeLeave(p);

  /* If there are still other outstanding references to the shared-btree
  ** structure, return now. The remainder of this procedure cleans 
  ** up the shared-btree.
  ** 如果仍然有其他未解决的对shared-btree结构的引用，则立即返回。程序的剩余部分清理shared-btree。
  */
  assert( p->wantToLock==0 && p->locked==0 );
  if( !p->sharable || removeFromSharingList(pBt) ){
    /* The pBt is no longer on the sharing list, so we can access
    ** it without having to hold the mutex.
    ** pBt将不再留在共享列表上，所以我们可以访问它而无需持有互斥锁。
    ** Clean out and delete the BtShared object.
    ** 清理并删除BtShared对象 */
    assert( !pBt->pCursor );
    sqlite3PagerClose(pBt->pPager);/*在共享列表中不再有此对象，删除BtShared共享对象*/
    if( pBt->xFreeSchema && pBt->pSchema ){
      pBt->xFreeSchema(pBt->pSchema);
    }
    sqlite3DbFree(0, pBt->pSchema);
    freeTempSpace(pBt);//【潘光珍】将pBt释放临时空间
    sqlite3_free(pBt);//【潘光珍】删除BtShared对象中的pBt
  }

#ifndef SQLITE_OMIT_SHARED_CACHE
  assert( p->wantToLock==0 );
  assert( p->locked==0 );
  if( p->pPrev ) p->pPrev->pNext = p->pNext;//上页指向下下页
  if( p->pNext ) p->pNext->pPrev = p->pPrev;//下页指向上上页
#endif

  sqlite3_free(p); //将p释放
  return SQLITE_OK;
}

/*
** Change the limit on the number of pages allowed in the cache.
** 改变在缓存中允许的页面数量的限制
** The maximum number of cache pages is set to the absolute
** value of mxPage.  If mxPage is negative, the pager will
** operate asynchronously(不同时) - it will not stop to do fsync()s
** to insure data is written to the disk surface before
** continuing.  Transactions still work if synchronous is off,
** and the database cannot be corrupted if this program
** crashes.  But if the operating system crashes or there is
** an abrupt power failure when synchronous is off, the database
** could be left in an inconsistent and unrecoverable state.
** Synchronous is on by default so database corruption is not
** normally a worry.
** 缓存页面的最大数量设置为mxPage的价值。如果mxPage为负，则pager将进行异步操作
** 在继续之前表面它不会停止做fsync()以确保数据被写到磁盘。如果同步是关闭的事务仍然起作用，
** 并且如果程序崩溃而数据库可能不会损坏。但是，如果操作系统崩溃或当同步关闭时出现突然断电，
** 数据库可能会处于不一致的和不可恢复的状态。同步是默认的，因此数据库损坏通常是令人担忧的。
*/
/*控制页缓存大小以及同步写入（在编译指示synchronous中定义）*/
/*【潘光珍】控制页缓存大小以及同步写入（在编译指示synchronous中定义）
**缓存页面的最大数量设置为mxpage绝对值。如果mxpage是负的，分页器将异步操作（不同时）-它会不停的做fsync()确保数据被写入到磁盘表面继续前。
如果同步是关闭的，则事务仍然工作，如果该程序将无法被损坏崩溃。但是，如果操作系统崩溃或有突然断电时同步时，数据库可能处于不一致的和不可恢复的状态离开。
同步是默认情况下，所以数据库损坏通常是不担心。
*/
int sqlite3BtreeSetCacheSize(Btree *p, int mxPage){
  BtShared *pBt = p->pBt;
  assert( sqlite3_mutex_held(p->db->mutex) );
  sqlite3BtreeEnter(p);
  sqlite3PagerSetCachesize(pBt->pPager, mxPage); /*设置cache中页的数量*/
  sqlite3BtreeLeave(p);
  return SQLITE_OK;
}

/*
** Change the way data is synced to disk in order to increase or decrease
** how well the database resists damage due to OS crashes and power
** failures.  Level 1 is the same as asynchronous (no syncs() occur and
** there is a high probability of damage)  Level 2 is the default.  There
** is a very low but non-zero probability of damage.  Level 3 reduces the
** probability of damage to near zero but with a write performance reduction.
*/
/*
sqlite3BtreeSetSafetyLevel ：改变磁盘数据的同步方式，以增加或减少数据库抵御操作系统崩溃或电源故障等损害的能力。
1级等同于异步（无syncs（）发生，存在较高的损害风险），等同于设置编译指示synchronous=OFF。
2级是默认级别，存在较低的损害风险，等同于设置编译指示synchronous=NORMAL。
3级降低了损害可能性，风险接近为0，但降低了写性能，等同于设置编译指示synchronous=FULL。

*/
#ifndef SQLITE_OMIT_PAGER_PRAGMAS
int sqlite3BtreeSetSafetyLevel(      //改变磁盘数据的访问方式，以增加或减少数据库抵御操作系统崩溃或电源故障等损害的能力
  Btree *p,              /* The btree to set the safety level on */         //btree设置安全级别
  int level,             /* PRAGMA synchronous.  1=OFF, 2=NORMAL, 3=FULL */ //编译指示同步，1=OFF, 2=NORMAL, 3=FULL
  int fullSync,          /* PRAGMA fullfsync. */                            //编译指示fullfsync
  int ckptFullSync       /* PRAGMA checkpoint_fullfync */                   //编译指示checkpoint_fullfync
){
  BtShared *pBt = p->pBt;
  assert( sqlite3_mutex_held(p->db->mutex) );
  assert( level>=1 && level<=3 );//【潘光珍】安全级别为1，2，3
  sqlite3BtreeEnter(p);//【潘光珍】调用进入B树的函数
  sqlite3PagerSetSafetyLevel(pBt->pPager, level, fullSync, ckptFullSync);//【潘光珍】调用Pager设置安全级别的函数
  sqlite3BtreeLeave(p);//【潘光珍】调用离开B树的函数
  return SQLITE_OK;
}
#endif

/*
** Return TRUE if the given btree is set to safety level 1.  In other
** words, return TRUE if no sync() occurs on the disk files.
** 给定的B树被设定的安全级别为1返回true。即是如果在磁盘上没有sync()出现返回true。
*/
/*
【潘光珍】如果给定的B树的安全级别是1，则返回true。换句话说，如果没有sync()发生在磁盘文件，则返回true。
*/
int sqlite3BtreeSyncDisabled(Btree *p){
  BtShared *pBt = p->pBt;
  int rc;
  assert( sqlite3_mutex_held(p->db->mutex) );  
  sqlite3BtreeEnter(p);
  assert( pBt && pBt->pPager );
  rc = sqlite3PagerNosync(pBt->pPager);
  sqlite3BtreeLeave(p);
  return rc;
}

/*
** Change the default pages size and the number of reserved bytes per page.
** Or, if the page size has already been fixed, return SQLITE_READONLY 
** without changing anything.
**
** The page size must be a power of 2 between 512 and 65536.  If the page
** size supplied does not meet this constraint then the page size is not
** changed.
**
** Page sizes are constrained to be a power of two so that the region
** of the database file used for locking (beginning at PENDING_BYTE,
** the first byte past the 1GB boundary, 0x40000000) needs to occur
** at the beginning of a page.
**
** If parameter nReserve is less than zero, then the number of reserved
** bytes per page is left unchanged.
**
** If the iFix!=0 then the BTS_PAGESIZE_FIXED flag is set so that the page size
** and autovacuum mode can no longer be changed.
*/
/*设置数据库页大小*/
/*
【潘光珍】**更改默认的页大小和保留的字节数量。或者，如果网页的大小已经固定，sqlite_readonly不做任何改变。
**页面大小必须是2的幂在512和65536之间。如果页面大小不满足此约束，则页面大小没有改变。
**页面大小限制为2的幂，区域用于锁定数据库文件（从pending_byte，第一个字节过去1GB的边界，0x40000000）需要发生在一页的开头。
**如果参数nReserve小于零，然后数保留每一页的字节数不变。
**如果iFIX！= 0然后设置bts_pagesize_fixed标志，页面大小和autovacuum模式不再能被改变。
*/
int sqlite3BtreeSetPageSize(Btree *p, int pageSize, int nReserve, int iFix){
  int rc = SQLITE_OK;
  BtShared *pBt = p->pBt;
  assert( nReserve>=-1 && nReserve<=255 );
  sqlite3BtreeEnter(p);
  if( pBt->btsFlags & BTS_PAGESIZE_FIXED ){
    sqlite3BtreeLeave(p);
    return SQLITE_READONLY;
  }
  if( nReserve<0 ){
    nReserve = pBt->pageSize - pBt->usableSize; /*保留页大小*/
  }
  assert( nReserve>=0 && nReserve<=255 );
  if( pageSize>=512 && pageSize<=SQLITE_MAX_PAGE_SIZE &&
        ((pageSize-1)&pageSize)==0 ){
    assert( (pageSize & 7)==0 );
    assert( !pBt->pPage1 && !pBt->pCursor );
    pBt->pageSize = (u32)pageSize;
    freeTempSpace(pBt);
  }
  rc = sqlite3PagerSetPagesize(pBt->pPager, &pBt->pageSize, nReserve);/*设置页的大小*/
  pBt->usableSize = pBt->pageSize - (u16)nReserve;
  if( iFix ) pBt->btsFlags |= BTS_PAGESIZE_FIXED;/*iFix!=0，设置BTS_PAGESIZE_FIXED标志*/
  sqlite3BtreeLeave(p);
  return rc;
}

/*
** Return the currently defined page size
*/
/*
返回数据库页的大小
*/
int sqlite3BtreeGetPageSize(Btree *p){
  return p->pBt->pageSize;//返回数据库页的大小
}

#if !defined(SQLITE_OMIT_PAGER_PRAGMAS) || !defined(SQLITE_OMIT_VACUUM)
/*
** Return the number of bytes of space at the end of every page that
** are intentually left unused.  This is the "reserved" space that is
** sometimes used by extensions.
*/
/*
【潘光珍】返回在最后每一页都未被使用的字节数的空间。这是“保留”的空间，有时使用扩展。
*/
int sqlite3BtreeGetReserve(Btree *p){//定义一个保留空间的函数
  int n;
  sqlite3BtreeEnter(p);
  n = p->pBt->pageSize - p->pBt->usableSize;/*页中未被使用的字节数*/
  sqlite3BtreeLeave(p);
  return n;
}

/*
** Set the maximum page count for a database if mxPage is positive.
** No changes are made if mxPage is 0 or negative.
** Regardless of the value of mxPage, return the maximum page count.
** 如果mxPage是正的，设置数据库的最大页数。如果mxPage是0或负则不改变大小。不管mxPage的值,返回最大页数。
*/
/*
【潘光珍】如果mxpage是正，则设置最大页数的数据库。如果mxpage是0或负数了，则没有变化。不管mxpage的值，返回的最大页数。

*/
int sqlite3BtreeMaxPageCount(Btree *p, int mxPage){//定义一个最大页数的数据库的函数
  int n;
  sqlite3BtreeEnter(p);
  n = sqlite3PagerMaxPageCount(p->pBt->pPager, mxPage);/*mxPage为正，pPager->mxPgno = mxPage;*/
  sqlite3BtreeLeave(p);
  return n;
}

/*
** Set the BTS_SECURE_DELETE flag if newFlag is 0 or 1.  If newFlag is -1,
** then make no changes.  Always return the value of the BTS_SECURE_DELETE
** setting after the change.
** 如果newFlag是0或1，设置BTS_SECURE_DELETE标志。如果newFlag是-1,则不设置。一旦设定将总是返回BTS_SECURE_DELETE的值。
*/
/*
【潘光珍】如果newflag是0或1，则设置BTS_SECURE_DELETE标志。如果newFlag是-1，那么不需要改变。总之返回BTS_SECURE_DELETE设置更改后的值就行。
*/
int sqlite3BtreeSecureDelete(Btree *p, int newFlag){
  int b;
  if( p==0 ) return 0;
  sqlite3BtreeEnter(p);
  if( newFlag>=0 ){
    p->pBt->btsFlags &= ~BTS_SECURE_DELETE;
    if( newFlag ) p->pBt->btsFlags |= BTS_SECURE_DELETE;
  } 
  b = (p->pBt->btsFlags & BTS_SECURE_DELETE)!=0;
  sqlite3BtreeLeave(p);
  return b;
}
#endif /* !defined(SQLITE_OMIT_PAGER_PRAGMAS) || !defined(SQLITE_OMIT_VACUUM) */

/*
** Change the 'auto-vacuum' property of the database. If the 'autoVacuum'
** parameter is non-zero, then auto-vacuum mode is enabled. If zero, it
** is disabled. The default value for the auto-vacuum property is 
** determined by the SQLITE_DEFAULT_AUTOVACUUM macro.
** 设置数据库自动清理空闲页属性。如果“autoVacuum”参数是零,那么auto-vacuum模式开启。如果为零则禁用。
** auto-vacuum属性的默认值由宏SQLITE_DEFAULT_AUTOVACUUM定义。
*//*设置数据库自动清理空闲页属性。*/
/*
【潘光珍】**设置数据库自动清理空闲页属性。
**改变数据库的'auto-vacuum'属性。如果“autovacuum”参数为非零，则auto-vacuum模式被启用。
如果零，它被禁用。为auto-vacuum属性的默认值是由SQLITE_DEFAULT_AUTOVACUUM 宏观决定。
*/
int sqlite3BtreeSetAutoVacuum(Btree *p, int autoVacuum){//定义一个设置数据库自动清理的函数
#ifdef SQLITE_OMIT_AUTOVACUUM
  return SQLITE_READONLY;
#else
  BtShared *pBt = p->pBt;
  int rc = SQLITE_OK;
  u8 av = (u8)autoVacuum;

  sqlite3BtreeEnter(p);
  if( (pBt->btsFlags & BTS_PAGESIZE_FIXED)!=0 && (av ?1:0)!=pBt->autoVacuum ){
    rc = SQLITE_READONLY;
  }else{
    pBt->autoVacuum = av ?1:0; /*av如果为非0，auto-vacuum模式启动*/
    pBt->incrVacuum = av==2 ?1:0;
  }
  sqlite3BtreeLeave(p);
  return rc;
#endif
}

/*
** Return the value of the 'auto-vacuum' property. If auto-vacuum is 
** enabled 1 is returned. Otherwise 0.
*//*获取数据库是否自动清理页。*/
/*
【潘光珍】
**返回'auto-vacuum'属性的值。如果启用auto-vacuum，则返回1。否则0。
*/
int sqlite3BtreeGetAutoVacuum(Btree *p){//获取数据库是否自动清理页。
#ifdef SQLITE_OMIT_AUTOVACUUM
  return BTREE_AUTOVACUUM_NONE;
#else
  int rc;
  sqlite3BtreeEnter(p);//将p进入btree
  rc = (
    (!p->pBt->autoVacuum)?BTREE_AUTOVACUUM_NONE:
    (!p->pBt->incrVacuum)?BTREE_AUTOVACUUM_FULL:
    BTREE_AUTOVACUUM_INCR
  );/*若autoVacuum为1，除去空白页，判断incrVacuum的值，若incrVacuum=1， Incremental vacuum*/
  sqlite3BtreeLeave(p);//将p离开btree
  return rc;
#endif
}

/*
** Get a reference to pPage1 of the database file.  This will
** also acquire a readlock on that file.
** 得到一个关于数据库文件pPage1的参考。这也将在此文件上获得读锁
** SQLITE_OK is returned on success.  If the file is not a
** well-formed database file, then SQLITE_CORRUPT is returned.
** SQLITE_BUSY is returned if the database is locked.  SQLITE_NOMEM
** is returned if we run out of memory. 
** 成功则返回SQLITE_OK。如果文件不是一个格式良好的数据库文件,然后返回SQLITE_CORRUPT。
** 如果数据库被锁定返回SQLITE_BUSY。如果内存耗尽返回SQLITE_NOMEM.
*/
/*
【潘光珍】**得到一个参考的数据库文件pPage1。这也将对该文件获得读锁。
**SQLITE_OK 被返回成功。如果这个文件不是一个很好的数据库文件，然后SQLITE_CORRUPT被返回。
**如果数据库被锁定，则SQLITE_BUSY 被返回。如果我们使用完它的内存，则SQLITE_NOMEM被返回。
*/
static int lockBtree(BtShared *pBt){
  int rc;              /* Result code from subfunctions */                     //从子函数返回结果代码
  MemPage *pPage1;     /* Page 1 of the database file */                       //数据库文件的页1 /*【潘光珍】 数据库的page 1 */
  int nPage;           /* Number of pages in the database */                   //数据库中的页数量
  int nPageFile = 0;   /* Number of pages in the database file */              //数据库文件中的页数量
  int nPageHeader;     /* Number of pages in the database according to hdr */  //据hdr在数据库中的页面数

  assert( sqlite3_mutex_held(pBt->mutex) );
  assert( pBt->pPage1==0 );
  rc = sqlite3PagerSharedLock(pBt->pPager);
  if( rc!=SQLITE_OK ) return rc;
  rc = btreeGetPage(pBt, 1, &pPage1, 0);
  if( rc!=SQLITE_OK ) return rc;
  /* Do some checking to help insure the file we opened really is
  ** a valid database file. 
  ** 做一些检查,帮助我们确保打开的文件是一个有效的数据库文件。
  */
  nPage = nPageHeader = get4byte(28+(u8*)pPage1->aData);
  sqlite3PagerPagecount(pBt->pPager, &nPageFile);
  if( nPage==0 || memcmp(24+(u8*)pPage1->aData, 92+(u8*)pPage1->aData,4)!=0 ){
    nPage = nPageFile;
  }
  if( nPage>0 ){
    u32 pageSize;  /* 每页的字节数 */
    u32 usableSize; /* 每页可用的字节数。pageSize-每页尾部保留空间的大小，在文件头偏移为20处设定。 */
    u8 *page1 = pPage1->aData;
    rc = SQLITE_NOTADB;
    if( memcmp(page1, zMagicHeader, 16)!=0 ){
      goto page1_init_failed;
    }

#ifdef SQLITE_OMIT_WAL
    if( page1[18]>1 ){
      pBt->btsFlags |= BTS_READ_ONLY;
    }
    if( page1[19]>1 ){
      goto page1_init_failed;
    }
#else
    if( page1[18]>2 ){
      pBt->btsFlags |= BTS_READ_ONLY;
    }
    if( page1[19]>2 ){
      goto page1_init_failed;
    }
    /* If the write version is set to 2, this database should be accessed
    ** in WAL mode. If the log is not already open, open it now. Then 
    ** return SQLITE_OK and return without populating BtShared.pPage1.
    ** The caller detects this and calls this function again. This is
    ** required as the version of page 1 currently in the page1 buffer
    ** may not be the latest version - there may be a newer one in the log
    ** file.
	** 如果写版本设置为2,应该在WAL模式下访问这个数据库。如果日志不是已经打开,打开它。然后返回SQLITE_OK并且返回没有占据BtShared.pPage1 
	** 调用者检测到这一点并再次调用这个函数。这是需要第1页的版本目前在page1缓冲区，可能不是最新版本,可能会有一个新的日志文件。
    */
	/*
	【潘光珍】如果写的版本是2，这应该是在预写日志系统模式访问数据库。如果日志尚未打开，打开它。
	然后返回SQLITE_OK还没有填充BtShared.pPage1来检测，再调用这个函数。
	这是需要1页的版本目前在第一页缓冲区可能不是最新的版本可能会有一个新的日志文件中。
	*/
    if( page1[19]==2 && (pBt->btsFlags & BTS_NO_WAL)==0 ){
      int isOpen = 0;
      rc = sqlite3PagerOpenWal(pBt->pPager, &isOpen);/*打开日志*/
      if( rc!=SQLITE_OK ){
        goto page1_init_failed;
      }else if( isOpen==0 ){
        releasePage(pPage1);
        return SQLITE_OK;
      }
      rc = SQLITE_NOTADB;
    }
#endif
    /* The maximum embedded fraction must be exactly 25%.  And the minimum
    ** embedded fraction must be 12.5% for both leaf-data and non-leaf-data.
    ** The original design allowed these amounts to vary, but as of
    ** version 3.6.0, we require them to be fixed.
	** 最大的嵌入部分必须是25%而且最小嵌入部分包括叶数据域和non-leaf-data必须是12.5%。最初的设计允许这些数量不同,但截至3.6.0版本,我们要求他们是固定的。
    */
	/*
	【潘光珍】对于叶数据和非叶数据，最大嵌入式部分必须是25%和最小嵌入部分必须为12.5%。原设计允许这些数量有所不同，但作为版本3.6.0，我们要求他们是固定的。
	*/
    if( memcmp(&page1[21], "\100\040\040",3)!=0 ){
      goto page1_init_failed;
    }
    pageSize = (page1[16]<<8) | (page1[17]<<16);
    if( ((pageSize-1)&pageSize)!=0
     || pageSize>SQLITE_MAX_PAGE_SIZE 
     || pageSize<=256 
    ){
      goto page1_init_failed;
    }
    assert( (pageSize & 7)==0 );
    usableSize = pageSize - page1[20];
    if( (u32)pageSize!=pBt->pageSize ){
      /* After reading the first page of the database assuming a page size
      ** of BtShared.pageSize, we have discovered that the page-size is
      ** actually pageSize. Unlock the database, leave pBt->pPage1 at
      ** zero and return SQLITE_OK. The caller will call this function
      ** again with the correct page-size.
	  ** 读完第一页数据库的假设一个BtShared.pageSize的页面大小。我们已经发现page-size是实际的页大小。打开数据库,将pBt->pPage1留在0处
	  ** 并返回SQLITE_OK。调用者将会用正确的page-size再次调用这个函数。
      */
		/*
		【潘光珍】读第一页后数据库假设一个BtShared.pageSize页面大小，我们发现，实际上是为页面大小。打开数据库，将pBt->pPage1为零和返回SQLITE_OK。调用方将再次调用这个函数，用正确的页面大小。
		*/
      releasePage(pPage1);
      pBt->usableSize = usableSize;
      pBt->pageSize = pageSize;
      freeTempSpace(pBt);
      rc = sqlite3PagerSetPagesize(pBt->pPager, &pBt->pageSize,
                                   pageSize-usableSize);
      return rc;
    }
    if( (pBt->db->flags & SQLITE_RecoveryMode)==0 && nPage>nPageFile ){
      rc = SQLITE_CORRUPT_BKPT;
      goto page1_init_failed;
    }
    if( usableSize<480 ){
      goto page1_init_failed;
    }
    pBt->pageSize = pageSize;
    pBt->usableSize = usableSize;
#ifndef SQLITE_OMIT_AUTOVACUUM
    pBt->autoVacuum = (get4byte(&page1[36 + 4*4])?1:0);
    pBt->incrVacuum = (get4byte(&page1[36 + 7*4])?1:0);
#endif
  }
  /* maxLocal is the maximum amount of payload to store locally for
  ** a cell.  Make sure it is small enough so that at least minFanout
  ** cells can will fit on one page.  We assume a 10-byte page header.
  ** Besides the payload, the cell must store:
  **     2-byte pointer to the cell
  **     4-byte child pointer
  **     9-byte nKey value
  **     4-byte nData value
  **     4-byte overflow page pointer
  ** So a cell consists of a 2-byte pointer, a header which is as much as
  ** 17 bytes long, 0 to N bytes of payload, and an optional 4 byte overflow
  ** page pointer.
  ** maxLocal是存储在本地一个单元的负载的最大数量。确保它是足够小,这样至少minFanout单元可以固定在一个页面上。我们假设一个10-byte页头。
  ** 除了负载,单元必须存储:
  **     2字节的指向单元的指针
  **     4字节的孩子指针
  **     9个字节的nKey值
  **     4字节的nData值
  **     4字节的溢出页指针
  ** 所以单元由一个2字节的指针,一个17字节长的头，0到N个字节的有效负载,和一个可选的4字节溢出页面指针。
  */
   /*
 【潘光珍】 maxlocal是有效载荷的最大存储量局部单元格。确保它是足够小，
  这样至少minfanout单元格可以将适合在一个页面。我们假设一个10字节的页头。
  除了有效载荷，这个单元格必须存储：
  2个字节指针的单元格
  4个字节的页指针
  9个字节nKey的值
  4个字节nData的值
  4个字节溢出页指针
所以一个单元由一个2字节的指针，头部是一样的17字节长，0到N字节的有效载荷，和一个可选的4字节溢出
页面指针。
  */
  pBt->maxLocal = (u16)((pBt->usableSize-12)*64/255 - 23);
  pBt->minLocal = (u16)((pBt->usableSize-12)*32/255 - 23);
  pBt->maxLeaf = (u16)(pBt->usableSize - 35);
  pBt->minLeaf = (u16)((pBt->usableSize-12)*32/255 - 23);
  if( pBt->maxLocal>127 ){
    pBt->max1bytePayload = 127;
  }else{
    pBt->max1bytePayload = (u8)pBt->maxLocal;
  }
  assert( pBt->maxLeaf + 23 <= MX_CELL_SIZE(pBt) );
  pBt->pPage1 = pPage1;
  pBt->nPage = nPage;
  return SQLITE_OK;

page1_init_failed:
  releasePage(pPage1);
  pBt->pPage1 = 0;
  return rc;
}

/*
** If there are no outstanding cursors and we are not in the middle
** of a transaction but there is a read lock on the database, then
** this routine unrefs the first page of the database file which 
** has the effect of releasing the read lock.
** 如果没有显著的游标并且不是在事务中而是在数据库上有一个读锁，那么这个例程不会引用已经释放读锁的数据库文件的第一页。
** If there is a transaction in progress, this routine is a no-op.
** 如果在进程中有一个事务,这个例程将是一个空操作。
*/
/*
【潘光珍】**如果没有很好的游标和我们不在一个事务中但有读锁的数据库，然后这个程序unrefs数据库的文件，释放读锁影响的第一页。
**如果有一个进程中的事务，这个程序是一个空操作。
*/
static void unlockBtreeIfUnused(BtShared *pBt){
  assert( sqlite3_mutex_held(pBt->mutex) );
  assert( pBt->pCursor==0 || pBt->inTransaction>TRANS_NONE );
  if( pBt->inTransaction==TRANS_NONE && pBt->pPage1!=0 ){	/*没有事务*/
    assert( pBt->pPage1->aData );
    assert( sqlite3PagerRefcount(pBt->pPager)==1 );
    assert( pBt->pPage1->aData );
    releasePage(pBt->pPage1);/*释放内存*/
    pBt->pPage1 = 0;
  }
}

/*
** If pBt points to an empty file then convert that empty file
** into a new empty database by initializing the first page of
** the database.
** 如果pBt指向一个空文件,那么通过初始化数据库的第一页传送空文件到一个新的空数据库。
*/
/*
【潘光珍】如果pBt指向空文件，然后将空文件到一个新的空数据库初始化数据库的第一页。
*/
static int newDatabase(BtShared *pBt){
  MemPage *pP1;
  unsigned char *data;
  int rc;

  assert( sqlite3_mutex_held(pBt->mutex) );
  if( pBt->nPage>0 ){
    return SQLITE_OK;
  }
  pP1 = pBt->pPage1;
  assert( pP1!=0 );
  data = pP1->aData;
  rc = sqlite3PagerWrite(pP1->pDbPage);/*初始化数据库中的第一页，传送空文件到空的数据库*/
  if( rc ) return rc;
  memcpy(data, zMagicHeader, sizeof(zMagicHeader));
  assert( sizeof(zMagicHeader)==16 );
  data[16] = (u8)((pBt->pageSize>>8)&0xff);
  data[17] = (u8)((pBt->pageSize>>16)&0xff);
  data[18] = 1;
  data[19] = 1;
  assert( pBt->usableSize<=pBt->pageSize && pBt->usableSize+255>=pBt->pageSize);
  data[20] = (u8)(pBt->pageSize - pBt->usableSize);
  data[21] = 64;
  data[22] = 32;
  data[23] = 32;
  memset(&data[24], 0, 100-24);
  zeroPage(pP1, PTF_INTKEY|PTF_LEAF|PTF_LEAFDATA );
  pBt->btsFlags |= BTS_PAGESIZE_FIXED;
#ifndef SQLITE_OMIT_AUTOVACUUM
  assert( pBt->autoVacuum==1 || pBt->autoVacuum==0 );
  assert( pBt->incrVacuum==1 || pBt->incrVacuum==0 );
  put4byte(&data[36 + 4*4], pBt->autoVacuum);
  put4byte(&data[36 + 7*4], pBt->incrVacuum);
#endif
  pBt->nPage = 1;
  data[31] = 1;
  return SQLITE_OK;
}

/*
** Attempt to start a new transaction. A write-transaction
** is started if the second argument is nonzero, otherwise a read-
** transaction.  If the second argument is 2 or more and exclusive
** transaction is started, meaning that no other process is allowed
** to access the database.  A preexisting transaction may not be
** upgraded to exclusive by calling this routine a second time - the
** exclusivity flag only works for a new transaction.
** 尝试开始一个新事务。如果第二个参数是非零0，开始一个写事务,否则开始读事务。如果第二个参数是2或比2更大则开始一个互斥的事务,
** 也就是说,不允许其他进程来访问数据库。一个先前存在的事务可能不是通过第二次调用这个函数升级到互斥的--互斥标志只适用于一个新事务。
** A write-transaction must be started before attempting any 
** changes to the database.  None of the following routines 
** will work unless a transaction is started first:
** 写事务必须在修改数据库之前开始。下面的函数没有一个会起作用除非事务首先开始:
**      sqlite3BtreeCreateTable()
**      sqlite3BtreeCreateIndex()
**      sqlite3BtreeClearTable()
**      sqlite3BtreeDropTable()
**      sqlite3BtreeInsert()
**      sqlite3BtreeDelete()
**      sqlite3BtreeUpdateMeta()
**
** If an initial attempt to acquire the lock fails because of lock contention
** and the database was previously unlocked, then invoke the busy handler
** if there is one.  But if there was previously a read-lock, do not
** invoke the busy handler - just return SQLITE_BUSY.  SQLITE_BUSY is 
** returned when there is already a read-lock in order to avoid a deadlock.
** 如果首次尝试获得锁失败是因为锁竞争和数据库之前没有锁,然后调用繁忙的处理程序，如果有的话。但是如果之前有读锁,
** 则不调用——只是返回SQLITE_BUSY。已经有一个读锁以避免死锁时，SQLITE_BUSY被返回。
** Suppose there are two processes A and B.  A has a read lock and B has
** a reserved lock.  B tries to promote to exclusive but is blocked because
** of A's read lock.  A tries to promote to reserved but is blocked by B.
** One or the other of the two processes must give way or there can be
** no progress.  By returning SQLITE_BUSY and not invoking the busy callback
** when A already has a read lock, we encourage A to give up and let B
** proceed.
** 假设有两个进程A和B，A有一个读锁和B有reserved锁。B试图获得互斥但因为A的读锁被锁。A试图促进保留但被B锁。
** 一个或两个进程必须给其他的方式或者没有进程。当A已经有过一个读锁时返回SQLITE_BUSY而不是调用忙,尽量让A放弃，让B持有。
*/
/*
【潘光珍】**尝试启动新的事务。如果另一个参数为非零，则为非零，则为读事务。
如果二个参数是2个或多个，并且独占事务被启动，这意味着没有其他进程可以访问数据库。
一个已经存在的事务可能无法通过调用这个例程第二次——排它标志只能用于一个新的事务升级为独家。
**在尝试更改数据库之前，必须先开始写事务处理。没有下列例程将工作，除非一个事务是开始的：
**      sqlite3BtreeCreateTable()
**      sqlite3BtreeCreateIndex()
**      sqlite3BtreeClearTable()
**      sqlite3BtreeDropTable()
**      sqlite3BtreeInsert()
**      sqlite3BtreeDelete()
**      sqlite3BtreeUpdateMeta()
**如果最初的尝试获得锁由于锁争用失败和数据库是先前解锁的，如果有一个数据库，则调用这个繁忙的处理程序。
但是如果有以前读锁，不调用处理程序只返回SQLITE_BUSY。SQLITE_BUSY返回时，已经有一个读锁，以避免死锁。
**假设有两个进程A和B。A具有读锁和B具有保留的锁。B试图促进独占但受阻是因为一个读锁，A试图推广到保留但被B阻止。
一个或另一个进程必须让路或有没有进展。返回SQLITE_BUSY不调用回调时忙已经有一个读锁，我们放弃A让B进行。

*/
int sqlite3BtreeBeginTrans(Btree *p, int wrflag){   //wrflag非零开始写事务，否则开始读事务
  sqlite3 *pBlock = 0;
  BtShared *pBt = p->pBt;
  int rc = SQLITE_OK;

  sqlite3BtreeEnter(p);
  btreeIntegrity(p);

  /* If the btree is already in a write-transaction, or it
  /* If the btree is already in a write-transaction, or it
  ** is already in a read-transaction and a read-transaction
  ** is requested, this is a no-op.
  ** 如果btree已经在写事务中,或者它已在读事务中并且读事务被请求,那么这是一个空操作。
  */
  /*
如果B树已经在写事务，或是已经在读事务和请求读取事务，这是一个空操作。
*/
  if( p->inTrans==TRANS_WRITE || (p->inTrans==TRANS_READ && !wrflag) ){
    goto trans_begun;
  }

  /* Write transactions are not possible on a read-only database */ //写事务不可能在一个只读的数据库上 /*【潘光珍】在只读数据库中写入事务是不可能的*/
  if( (pBt->btsFlags & BTS_READ_ONLY)!=0 && wrflag ){
    rc = SQLITE_READONLY;
    goto trans_begun;
  }

#ifndef SQLITE_OMIT_SHARED_CACHE
  /* If another database handle has already opened a write transaction 
  ** on this shared-btree structure and a second write transaction is
  ** requested, return SQLITE_LOCKED.
  ** 如果另一个数据库处理程序已经在这shared-btree结构开启了写事务并且请求第二个写事务,则返回SQLITE_LOCKED。
  */
  /*
  【潘光珍】如果一个数据库句柄已开通写事务在这共享的B树结构和请求第二个写事务，则返回SQLITE_LOCKED。
  */
  if( (wrflag && pBt->inTransaction==TRANS_WRITE)
   || (pBt->btsFlags & BTS_PENDING)!=0
  ){
    pBlock = pBt->pWriter->db;/*不能同时有两个写事务，返回SQLITE_LOCKED*/
  }else if( wrflag>1 ){
    BtLock *pIter;
    for(pIter=pBt->pLock; pIter; pIter=pIter->pNext){
      if( pIter->pBtree!=p ){
        pBlock = pIter->pBtree->db;
        break;
      }
    }
  }
  if( pBlock ){
    sqlite3ConnectionBlocked(p->db, pBlock);
    rc = SQLITE_LOCKED_SHAREDCACHE;
    goto trans_begun;
  }
#endif

  /* Any read-only or read-write transaction implies a read-lock on 
  ** page 1. So if some other shared-cache client already has a write-lock 
  ** on page 1, the transaction cannot be opened. 
  ** 任何只读或读写事务意味着在页1上有读锁。如果其他共享缓存客户端在页1上已经有一个写锁,那么事务不能被开启。*/
  /*
 【潘光珍】 任何只读或读写事务意味着读锁页为1。因此，如果一些其他共享缓存客户端已经有一个写锁页为1，则该事务不能被打开。
  */
  rc = querySharedCacheTableLock(p, MASTER_ROOT, READ_LOCK);
  if( SQLITE_OK!=rc ) goto trans_begun;

  pBt->btsFlags &= ~BTS_INITIALLY_EMPTY;
  if( pBt->nPage==0 ) pBt->btsFlags |= BTS_INITIALLY_EMPTY;
  do {
    /* Call lockBtree() until either pBt->pPage1 is populated or
    ** lockBtree() returns something other than SQLITE_OK. lockBtree()
    ** may return SQLITE_OK but leave pBt->pPage1 set to 0 if after
    ** reading page 1 it discovers that the page-size of the database 
    ** file is not pBt->pageSize. In this case lockBtree() will update
    ** pBt->pageSize to the page-size of the file on disk.
	** 调用lockBtree(),直到pBt->pPage1被赋值或者lockBtree()返回SQLITE_OK以外的信息。
	** lockBtree()可能返回SQLITE_OK但赋pBt->pPage1为0 ，如果读第1页后发现数据库文件页面大小不是pBt->pageSize。
	** 在这种情况下lockBtree()将更新pBt->pageSize的大小为磁盘上文件的页大小。
    */
	  /*
	 【潘光珍】 调用lockBtree()函数直到pBt->pPage1被填充或者lockBtree()函数返回SQLITE_OK以外的内容。
	  lockbtree()函数可能返回SQLITE_OK但是pBt->pPage1设置为0如果读的页为1发现页面大小的 数据库文件不是pBt->pageSize。
	  在这种情况下lockbtree()将更新pBt->pageSize的页面文件大小的磁盘上。
	  */
    while( pBt->pPage1==0 && SQLITE_OK==(rc = lockBtree(pBt)) );

    if( rc==SQLITE_OK && wrflag ){
      if( (pBt->btsFlags & BTS_READ_ONLY)!=0 ){
        rc = SQLITE_READONLY;
      }else{
        rc = sqlite3PagerBegin(pBt->pPager,wrflag>1,sqlite3TempInMemory(p->db));
        if( rc==SQLITE_OK ){
          rc = newDatabase(pBt);
        }
      }
    }
  
    if( rc!=SQLITE_OK ){
      unlockBtreeIfUnused(pBt);
    }
  }while( (rc&0xFF)==SQLITE_BUSY && pBt->inTransaction==TRANS_NONE &&
          btreeInvokeBusyHandler(pBt) );

  if( rc==SQLITE_OK ){
    if( p->inTrans==TRANS_NONE ){
      pBt->nTransaction++;
#ifndef SQLITE_OMIT_SHARED_CACHE
      if( p->sharable ){
        assert( p->lock.pBtree==p && p->lock.iTable==1 );
        p->lock.eLock = READ_LOCK;
        p->lock.pNext = pBt->pLock;
        pBt->pLock = &p->lock;
      }
#endif
    }
    p->inTrans = (wrflag?TRANS_WRITE:TRANS_READ);/*为1是写锁，否则读锁*/
    if( p->inTrans>pBt->inTransaction ){
      pBt->inTransaction = p->inTrans;
    }
    if( wrflag ){
      MemPage *pPage1 = pBt->pPage1;
#ifndef SQLITE_OMIT_SHARED_CACHE
      assert( !pBt->pWriter );
      pBt->pWriter = p;
      pBt->btsFlags &= ~BTS_EXCLUSIVE;
      if( wrflag>1 ) pBt->btsFlags |= BTS_EXCLUSIVE;
#endif

      /* If the db-size header field is incorrect (as it may be if an old
      ** client has been writing the database file), update it now. Doing
      ** this sooner rather than later means the database size can safely 
      ** re-read the database size from page 1 if a savepoint or transaction
      ** rollback occurs within the transaction.
	  ** 如果db-size头字段不正确(如果一个旧客户端一直在写数据库文件，则这种情况可能发生),则立即更新。
	  ** 更新宜早不宜迟，因为如果一个保存点或事务在事务中发生回滚，数据库大小可以从第1页安全地重读。
      */
	  /*
	 【潘光珍】 如果数据库大小的头部是错误的(因为这可能是一个旧客户写数据库文件),则马上更新。
	  这样做迟早意味着，如果一个保存点回滚或事务发生在事务数据库大小可以安全地重新读取数据库大小页为1。
	  */
      if( pBt->nPage!=get4byte(&pPage1->aData[28]) ){
        rc = sqlite3PagerWrite(pPage1->pDbPage);/*更新db-size的头字段*/
        if( rc==SQLITE_OK ){
          put4byte(&pPage1->aData[28], pBt->nPage);
        }
      }
    }
  }
trans_begun:
  if( rc==SQLITE_OK && wrflag ){
    /* This call makes sure that the pager has the correct number of
    ** open savepoints. If the second parameter is greater than 0 and
    ** the sub-journal is not already open, then it will be opened here.
	** 这个调用确保pager有正确的开放性保存点数目。如果第二个参数大于0并且sub-journal没有打开,那么它将被打开。
    */
	  /*
	  【潘光珍】这个调用保证页缓冲区具有开放的保存点正确的数量。如果二个参数大于0和sub-journal不是已经打开，那么它将在这里打开。
	  */
    rc = sqlite3PagerOpenSavepoint(pBt->pPager, p->db->nSavepoint);/*wrflag>0,打开保存点*/
  }

  btreeIntegrity(p);
  sqlite3BtreeLeave(p);
  return rc;
}

#ifndef SQLITE_OMIT_AUTOVACUUM
/*
** Set the pointer-map entries for all children of page pPage. Also, if
** pPage contains cells that point to overflow pages, set the pointer
** map entries for the overflow pages as well.
** 对页pPage的所有孩子节点设置指针映射条目。如果pPage包含指向溢出页的指针的单元，也对溢出页设置指针映射条目。
*/
/*
【潘光珍】设置指针位图为pPage所有孩子页。同时，如果pPage包含指向溢出页的单元，设置溢出页的指针位图。
*/
static int setChildPtrmaps(MemPage *pPage){
  int i;                             /* Counter variable */    //计数器变量
  int nCell;                         /* Number of cells in page pPage */  //在页pPage中的单元的数量  /* 【潘光珍】本页的单元数*/
  int rc;                            /* Return code */    //返回值变量
  BtShared *pBt = pPage->pBt;
  u8 isInitOrig = pPage->isInit;
  Pgno pgno = pPage->pgno;

  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  rc = btreeInitPage(pPage);
  if( rc!=SQLITE_OK ){
    goto set_child_ptrmaps_out;
  }
  nCell = pPage->nCell;

  for(i=0; i<nCell; i++){
    u8 *pCell = findCell(pPage, i);

    ptrmapPutOvflPtr(pPage, pCell, &rc);

    if( !pPage->leaf ){
      Pgno childPgno = get4byte(pCell);
      ptrmapPut(pBt, childPgno, PTRMAP_BTREE, pgno, &rc);
    }
  }

  if( !pPage->leaf ){
    Pgno childPgno = get4byte(&pPage->aData[pPage->hdrOffset+8]);
    ptrmapPut(pBt, childPgno, PTRMAP_BTREE, pgno, &rc);
  }

set_child_ptrmaps_out:
  pPage->isInit = isInitOrig;
  return rc;
}

/*
** Somewhere on pPage is a pointer to page iFrom.  Modify this pointer so
** that it points to iTo. Parameter eType describes the type of pointer to
** be modified, as  follows:
** 页iFrom是一个指针，指向页面上的某个地方。修改这个指针使它指向iTo。参数eType描述被修改指针的类型,如下所示:
** PTRMAP_BTREE:     pPage is a btree-page. The pointer points at a child 
**                   page of pPage.
**                   指针指向pPage的一个孩子页面。
** PTRMAP_OVERFLOW1: pPage is a btree-page. The pointer points at an overflow
**                   page pointed to by one of the cells on pPage.
**                   指针指向一个溢出页面，从pPage上的单元格中的一个指向该溢出页面
** PTRMAP_OVERFLOW2: pPage is an overflow-page. The pointer points at the next
**                   overflow page in the list.
*/                   //指针指向列表中的下一个溢出页面
/*
【潘光珍】**从pPage指向iFrom页。修改该指针使其指向ITO。参数eType描述要修改指针的类型，如下：
** PTRMAP_BTREE: pPage是b树页，这个指针指向子页的pPage。
** PTRMAP_OVERFLOW1:pPage是b树页。指针指向一个溢出页指着由一个在pPage的单元格。
** PTRMAP_OVERFLOW2:pPage是溢出页。该指针指向列表中的下一个溢出页。
*/
static int modifyPagePointer(MemPage *pPage, Pgno iFrom, Pgno iTo, u8 eType){
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  assert( sqlite3PagerIswriteable(pPage->pDbPage) );
  if( eType==PTRMAP_OVERFLOW2 ){
    /* The pointer is always the first 4 bytes of the page in this case.*/  //指针总是第一个页面的4个字节。
    if( get4byte(pPage->aData)!=iFrom ){
      return SQLITE_CORRUPT_BKPT;
    }
    put4byte(pPage->aData, iTo);
  }else{
    u8 isInitOrig = pPage->isInit;
    int i;
    int nCell;

    btreeInitPage(pPage);
    nCell = pPage->nCell;

    for(i=0; i<nCell; i++){
      u8 *pCell = findCell(pPage, i);
      if( eType==PTRMAP_OVERFLOW1 ){
        CellInfo info;
        btreeParseCellPtr(pPage, pCell, &info);
        if( info.iOverflow
         && pCell+info.iOverflow+3<=pPage->aData+pPage->maskPage
         && iFrom==get4byte(&pCell[info.iOverflow])
        ){
          put4byte(&pCell[info.iOverflow], iTo);
          break;
        }
      }else{
        if( get4byte(pCell)==iFrom ){
          put4byte(pCell, iTo);
          break;
        }
      }
    }
  
    if( i==nCell ){
      if( eType!=PTRMAP_BTREE || 
          get4byte(&pPage->aData[pPage->hdrOffset+8])!=iFrom ){
        return SQLITE_CORRUPT_BKPT;
      }
      put4byte(&pPage->aData[pPage->hdrOffset+8], iTo);
    }

    pPage->isInit = isInitOrig;
  }
  return SQLITE_OK;
}
/*
** Move the open database page pDbPage to location iFreePage in the 
** database. The pDbPage reference remains valid.
** 移动开放数据库页pDbPage到数据库中的要存放位置iFreePage。pDbPage参数仍可用。
** The isCommit flag indicates that there is no need to remember that
** the journal needs to be sync()ed before database page pDbPage->pgno 
** can be written to. The caller has already promised not to write to that
** page.
** isCommit标志表示在数据库页pDbPage->pgno可能被写之前日志需要sync()同步没有必要记录。调用者不去写那个页。
*/
/*
【潘光珍】**将打开的数据库页pDbPage数据库中的位置iFreePage。pDbPage仍然有效的参考。
**isCommit标志表示不需要记住日志而需要sync() ED在数据库页面pDbPage->pgno 
可以写。调用者已经答应不写该页面。
*/
static int relocatePage(
  BtShared *pBt,           /* Btree */               //B树
  MemPage *pDbPage,        /* Open page to move */   //要移动的开放性页
  u8 eType,                /* Pointer map 'type' entry for pDbPage */    //pDbPage指针映射类型条目 /*【潘光珍】pDbPage指针位图'type'*/
  Pgno iPtrPage,           /* Pointer map 'page-no' entry for pDbPage */ //pDbPage指针映射'page-no'条目//【潘光珍】pDbPage指针位图'page-no'
  Pgno iFreePage,          /* The location to move pDbPage to */         //移动pDbPage到的位置
  int isCommit             /* isCommit flag passed to sqlite3PagerMovepage */  //传递给sqlite3PagerMovepage的isCommit标志
){
  MemPage *pPtrPage;   /* The page that contains a pointer to pDbPage */   //包含到pDbPage的页
  Pgno iDbPage = pDbPage->pgno;
  Pager *pPager = pBt->pPager;
  int rc;

  assert( eType==PTRMAP_OVERFLOW2 || eType==PTRMAP_OVERFLOW1 || 
      eType==PTRMAP_BTREE || eType==PTRMAP_ROOTPAGE );
  assert( sqlite3_mutex_held(pBt->mutex) );
  assert( pDbPage->pBt==pBt );

  /* Move page iDbPage from its current location to page number iFreePage */ //从当前位置移动页面iDbPage到页码iFreePage
  TRACE(("AUTOVACUUM: Moving %d to free page %d (ptr page %d type %d)\n", 
      iDbPage, iFreePage, iPtrPage, eType));//【潘光珍】iDbPage从当前位置移动到iFreePage页数上
  rc = sqlite3PagerMovepage(pPager, pDbPage->pDbPage, iFreePage, isCommit);
  if( rc!=SQLITE_OK ){
    return rc;
  }
  pDbPage->pgno = iFreePage;

  /* If pDbPage was a btree-page, then it may have child pages and/or cells
  ** that point to overflow pages. The pointer map entries for all these
  ** pages need to be changed.
  ** 如果pDbPage是btree-page,那么它可能有孩子页或指向溢出页的单元。所有这些指针映射条目都需要更改。
  ** If pDbPage is an overflow page, then the first 4 bytes may store a
  ** pointer to a subsequent overflow page. If this is the case, then
  ** the pointer map needs to be updated for the subsequent overflow page.
  ** 如果pDbPage是一个溢出页,那么第一个4字节存储一个指向后续溢出页的指针。如果是这种情况,那么指针映射需要随后继溢出页面更新。
  */
  /*
  【潘光珍】**如果pDbPage是B树页，那么它可能有子页面和/或单元格指向溢出页。所有这些页的指针位图需要更改。
  **如果pDbPage是溢出页，然后第一个4字节可以存储一个指向随后溢出页。如果是这样的情况，则该指针位图需要为随后的溢出页进行更新。
  */
  if( eType==PTRMAP_BTREE || eType==PTRMAP_ROOTPAGE ){
    rc = setChildPtrmaps(pDbPage);
    if( rc!=SQLITE_OK ){
      return rc;
    }
  }else{
    Pgno nextOvfl = get4byte(pDbPage->aData);
    if( nextOvfl!=0 ){
      ptrmapPut(pBt, nextOvfl, PTRMAP_OVERFLOW2, iFreePage, &rc);
      if( rc!=SQLITE_OK ){
        return rc;
      }
    }
  }

  /* Fix the database pointer on page iPtrPage that pointed at iDbPage so
  ** that it points at iFreePage. Also fix the pointer map entry for
  ** iPtrPage.
  ** 固定数据库指针到页iPtrPage上，该页指向iDbPage,以便它指向iFreePage。同时对于iPtrPage，固定指针映射条目。
  */
   /*
 【潘光珍】 修改数据库指针iPtrPage页指向iDbPage使它指向iFreePage。并且还将修改iPtrPage指针位图。
  */
  if( eType!=PTRMAP_ROOTPAGE ){
    rc = btreeGetPage(pBt, iPtrPage, &pPtrPage, 0);
    if( rc!=SQLITE_OK ){
      return rc;
    }
    rc = sqlite3PagerWrite(pPtrPage->pDbPage);
    if( rc!=SQLITE_OK ){
      releasePage(pPtrPage);
      return rc;
    }
    rc = modifyPagePointer(pPtrPage, iDbPage, iFreePage, eType);
    releasePage(pPtrPage);
    if( rc==SQLITE_OK ){
      ptrmapPut(pBt, iFreePage, eType, iPtrPage, &rc);
    }
  }
  return rc;
}

/* Forward declaration required by incrVacuumStep(). */   //要求通过incrVacuumStep()提前声明
static int allocateBtreePage(BtShared *, MemPage **, Pgno *, Pgno, u8);

/*
** Perform a single step of an incremental-vacuum. If successful,
** return SQLITE_OK. If there is no work to do (and therefore no
** point in calling this function again), return SQLITE_DONE.
** 执行一个单独的incremental-vacuum步骤。如果成功,返回SQLITE_OK。如果没有成功(并没有再调用这个函数),返回SQLITE_DONE。
** More specificly, this function attempts to re-organize the 
** database so that the last page of the file currently in use
** is no longer in use.
** 更具体地,这个函数试图重组数据库,以使当前使用的文件的最后一页已不再使用。
** If the nFin parameter is non-zero, this function assumes
** that the caller will keep calling incrVacuumStep() until
** it returns SQLITE_DONE or an error, and that nFin is the
** number of pages the database file will contain after this 
** process is complete.  If nFin is zero, it is assumed that
** incrVacuumStep() will be called a finite amount of times
** which may or may not empty the freelist.  A full autovacuum
** has nFin>0.  A "PRAGMA incremental_vacuum" has nFin==0.
** 如果nFin参数不为零,这个函数假设调用者将保持调用incrVacuumStep()直到返回SQLITE_DONE或错误。nFin是数据库文件的页面数量
** 在进程完成之后将被包含。如果nFin是零,它假定incrVacuumStep()将被调用有限次，freelist可能会或可能不会空。
** 一个完整的autovacuum有nFin>0。一个"PRAGMA incremental_vacuum"有nFin==0。
*/
/*
【潘光珍】**单独执行一个渐进的incremental-vacuum。如果成功，返回SQLITE_OK。如果没有工作要做（因此再次调用这个函数没有点），返回SQLITE_DONE。
**更具体地说，这个函数试图重新组织数据库，最后一页的文件正在使用不再使用。
**如果nFin参数不为零，这个函数假定调用者会一直调用incrVacuumStep()直到返回SQLITE_DONE或错误，并且nFin是网页数据库文件将包含在这个过程中的数量是完整的。如果nFin是零，它是假定incrVacuumStep()
将被称为一个有限的时间，可能会或可能不会空自由列表。
*/
static int incrVacuumStep(BtShared *pBt, Pgno nFin, Pgno iLastPg){        //执行一个单独的incremental-vacuum步骤。
  Pgno nFreeList;           /* Number of pages still on the free-list */  //仍在空闲列表的页面数
  int rc;

  assert( sqlite3_mutex_held(pBt->mutex) );
  assert( iLastPg>nFin );

  if( !PTRMAP_ISPAGE(pBt, iLastPg) && iLastPg!=PENDING_BYTE_PAGE(pBt) ){
    u8 eType;
    Pgno iPtrPage;

    nFreeList = get4byte(&pBt->pPage1->aData[36]);
    if( nFreeList==0 ){
      return SQLITE_DONE;
    }

    rc = ptrmapGet(pBt, iLastPg, &eType, &iPtrPage);
    if( rc!=SQLITE_OK ){
      return rc;
    }
    if( eType==PTRMAP_ROOTPAGE ){
      return SQLITE_CORRUPT_BKPT;
    }

    if( eType==PTRMAP_FREEPAGE ){
      if( nFin==0 ){
        /* Remove the page from the files free-list. This is not required
        ** if nFin is non-zero. In that case, the free-list will be
        ** truncated to zero after this function returns, so it doesn't 
        ** matter if it still contains some garbage entries.
		** 删除文件空闲列表的页面。如果nFin是非零的则这不是必需的。在这种情况下,空闲列表在这个函数返回后截断为零,
		** 所以如果它还包含了一些垃圾条目也没有问题。
        */
		   /*
		  从“文件”空闲列表中删除该页。如果nFin是非零。在这种情况下，此函数返回后，空闲列表将被截断为零，
		  因此，如果它仍然包含一些没有用的信息，是没有关系的。
		  */
        Pgno iFreePg;
        MemPage *pFreePg;
        rc = allocateBtreePage(pBt, &pFreePg, &iFreePg, iLastPg, 1);
        if( rc!=SQLITE_OK ){
          return rc;//返回allocateBtreePage()函数
        }
        assert( iFreePg==iLastPg );
        releasePage(pFreePg);//调用释放页的函数
      }
    } else {
      Pgno iFreePg;             /* Index of free page to move pLastPg to */  //移动pLastPg所要到的空闲页的索引
      MemPage *pLastPg;

      rc = btreeGetPage(pBt, iLastPg, &pLastPg, 0);
      if( rc!=SQLITE_OK ){
        return rc;//返回btreeGetPage()函数
      }
/*以上是潘光珍做的*/ 

      /* If nFin is zero, this loop runs exactly once and page pLastPg
      ** is swapped with the first free page pulled off the free list.
      ** 如果nFin是零,这循环正好运行一次和页面pLastPg将与在空闲列表页中的第一个空闲页交换。
      ** On the other hand, if nFin is greater than zero, then keep
      ** looping until a free-page located within the first nFin pages
      ** of the file is found.
	  ** 另一方面,如果nFin大于零,然后继续循环,直到空闲页位于文件的第一个nFin页面被发现。
      */
      do {
        MemPage *pFreePg;
        rc = allocateBtreePage(pBt, &pFreePg, &iFreePg, 0, 0);
        if( rc!=SQLITE_OK ){
          releasePage(pLastPg);
          return rc;
        }
        releasePage(pFreePg);
      }while( nFin!=0 && iFreePg>nFin );
      assert( iFreePg<iLastPg );
      
      rc = sqlite3PagerWrite(pLastPg->pDbPage);
      if( rc==SQLITE_OK ){
        rc = relocatePage(pBt, pLastPg, eType, iPtrPage, iFreePg, nFin!=0);
      }
      releasePage(pLastPg);
      if( rc!=SQLITE_OK ){
        return rc;
      }
    }
  }

  if( nFin==0 ){
    iLastPg--;
    while( iLastPg==PENDING_BYTE_PAGE(pBt)||PTRMAP_ISPAGE(pBt, iLastPg) ){
      if( PTRMAP_ISPAGE(pBt, iLastPg) ){
        MemPage *pPg;
        rc = btreeGetPage(pBt, iLastPg, &pPg, 0);
        if( rc!=SQLITE_OK ){
          return rc;
        }
        rc = sqlite3PagerWrite(pPg->pDbPage);
        releasePage(pPg);
        if( rc!=SQLITE_OK ){
          return rc;
        }
      }
      iLastPg--;
    }
    sqlite3PagerTruncateImage(pBt->pPager, iLastPg);
    pBt->nPage = iLastPg;
  }
  return SQLITE_OK;
}

/*
** A write-transaction must be opened before calling this function.
** It performs a single unit of work towards an incremental vacuum.
** 调用这个程序之前，写事务必须打开。它执行单个工作单元对incremental vacuum。
** If the incremental vacuum is finished after this function has run,
** SQLITE_DONE is returned. If it is not finished, but no error occurred,
** SQLITE_OK is returned. Otherwise an SQLite error code. 
** 如果incremental vacuum在这个函数运行结束后被完成,返回SQLITE_DONE。如果没有完成,但是没有错误发生,返回SQLITE_OK。
** 否则返回一个SQLite错误代码。
*/
/*
调用这个程序之前，写事务必须打开。
*/
int sqlite3BtreeIncrVacuum(Btree *p){
  int rc;
  BtShared *pBt = p->pBt;

  sqlite3BtreeEnter(p);
  assert( pBt->inTransaction==TRANS_WRITE && p->inTrans==TRANS_WRITE );
  if( !pBt->autoVacuum ){
    rc = SQLITE_DONE;
  }else{
    invalidateAllOverflowCache(pBt);
    rc = incrVacuumStep(pBt, 0, btreePagecount(pBt));
    if( rc==SQLITE_OK ){
      rc = sqlite3PagerWrite(pBt->pPage1->pDbPage);
      put4byte(&pBt->pPage1->aData[28], pBt->nPage);
    }
  }
  sqlite3BtreeLeave(p);
  return rc;
}

/*
** This routine is called prior to sqlite3PagerCommit when a transaction
** is commited for an auto-vacuum database.
** 对于一个auto-vacuum数据库，当一个事务被提交之后这个函数将在sqlite3PagerCommit之前被调用。
** If SQLITE_OK is returned, then *pnTrunc is set to the number of pages
** the database file should be truncated to during the commit process. 
** i.e. the database has been reorganized so that only the first *pnTrunc
** pages are in use.
** 如果返回SQLITE_OK,那么*pnTrunc设置页面的数量，数据库文件在提交过程中应该被截断。
** 即数据库已经重组,以便只有第一个*pnTrunc页面在用。
*/
static int autoVacuumCommit(BtShared *pBt){
  int rc = SQLITE_OK;
  Pager *pPager = pBt->pPager;
  VVA_ONLY( int nRef = sqlite3PagerRefcount(pPager) );

  assert( sqlite3_mutex_held(pBt->mutex) );
  invalidateAllOverflowCache(pBt);
  assert(pBt->autoVacuum);
  if( !pBt->incrVacuum ){
    Pgno nFin;         /* Number of pages in database after autovacuuming */  //在走动清理后数据库中的页面数
    Pgno nFree;        /* Number of pages on the freelist initially */        //空闲列表上最初的页面数
    Pgno nPtrmap;      /* Number of PtrMap pages to be freed */               //被释放的PtrMap页面数量
    Pgno iFree;        /* The next page to be freed */                        //被释放的下一个页面
    int nEntry;        /* Number of entries on one ptrmap page */             //在一个ptrmap页上的条目数
    Pgno nOrig;        /* Database size before freeing */                     //释放前的数据库大小

    nOrig = btreePagecount(pBt);
    if( PTRMAP_ISPAGE(pBt, nOrig) || nOrig==PENDING_BYTE_PAGE(pBt) ){
      /* It is not possible to create a database for which the final page
      ** is either a pointer-map page or the pending-byte page. If one
      ** is encountered, this indicates corruption.
	  ** 对于最后一页是一个指针映射页或者是pending-byte类型的页，创建一个数据库是不可能的。如果创建了,这表明是不良的。
      */
      return SQLITE_CORRUPT_BKPT;
    }

    nFree = get4byte(&pBt->pPage1->aData[36]);
    nEntry = pBt->usableSize/5;
    nPtrmap = (nFree-nOrig+PTRMAP_PAGENO(pBt, nOrig)+nEntry)/nEntry;
    nFin = nOrig - nFree - nPtrmap;
    if( nOrig>PENDING_BYTE_PAGE(pBt) && nFin<PENDING_BYTE_PAGE(pBt) ){
      nFin--;
    }
    while( PTRMAP_ISPAGE(pBt, nFin) || nFin==PENDING_BYTE_PAGE(pBt) ){
      nFin--;
    }
    if( nFin>nOrig ) return SQLITE_CORRUPT_BKPT;

    for(iFree=nOrig; iFree>nFin && rc==SQLITE_OK; iFree--){
      rc = incrVacuumStep(pBt, nFin, iFree);
    }
    if( (rc==SQLITE_DONE || rc==SQLITE_OK) && nFree>0 ){
      rc = sqlite3PagerWrite(pBt->pPage1->pDbPage);
      put4byte(&pBt->pPage1->aData[32], 0);
      put4byte(&pBt->pPage1->aData[36], 0);
      put4byte(&pBt->pPage1->aData[28], nFin);
      sqlite3PagerTruncateImage(pBt->pPager, nFin);
      pBt->nPage = nFin;
    }
    if( rc!=SQLITE_OK ){
      sqlite3PagerRollback(pPager);
    }
  }

  assert( nRef==sqlite3PagerRefcount(pPager) );
  return rc;
}
#else /* ifndef SQLITE_OMIT_AUTOVACUUM */
# define setChildPtrmaps(x) SQLITE_OK
#endif

/*
** This routine does the first phase of a two-phase commit.  This routine
** causes a rollback journal to be created (if it does not already exist)
** and populated with enough information so that if a power loss occurs
** the database can be restored to its original state by playing back
** the journal.  Then the contents of the journal are flushed out to
** the disk.  After the journal is safely on oxide, the changes to the
** database are written into the database file and flushed to oxide.
** At the end of this call, the rollback journal still exists on the
** disk and we are still holding all locks, so the transaction has not
** committed.  See sqlite3BtreeCommitPhaseTwo() for the second phase of the
** commit process.
**
** This call is a no-op if no write-transaction is currently active on pBt.
**
** Otherwise, sync the database file for the btree pBt. zMaster points to
** the name of a master journal file that should be written into the
** individual journal file, or is NULL, indicating no master journal file 
** (single database transaction).
**
** When this is called, the master journal should already have been
** created, populated with this journal pointer and synced to disk.
**
** Once this is routine has returned, the only thing required to commit
** the write-transaction for this database file is to delete the journal.
** 这个函数是两阶段提交的第一阶段。这个函数创建回滚日志(如果它不存在)并加入足够的信息以便于如果出现功率损耗，
** 数据库可以通过日志恢复到原来的状态。日志的内容就写回到磁盘。在日志安全写回后,更改的数据库写入数据库文件和写到磁盘。
** 这个调用结束时,回滚日志在磁盘上仍然存在和仍持有所有的锁,所以事务没有提交.提交进程的第二个阶段是sqlite3BtreeCommitPhaseTwo().
** 如果没有写事务目前活跃在pBt上，则这个调用是一个空操作。
** 否则,对btree pBt的数据库文件同步。zMaster指向主日志文件的名字，该主日志文件应该写进单独的日志文件中。
** 或者为空,表示没有主日志文件(单个数据库事务)。当被调用时,该主日志应该已被创建、用日志指针和同步写入到磁盘。
** 一旦该函数返回，唯一事情就是需要提交写事务，数据库文件应该删除日志。
/*提交阶段分为2部分，这是第1部分，成功返回SQLITE_OK。第二部分在sqlite3BtreeCommitPhaseTwo*/
int sqlite3BtreeCommitPhaseOne(Btree *p, const char *zMaster){
  int rc = SQLITE_OK;
  if( p->inTrans==TRANS_WRITE ){/*若没有写事务，此调用为空操作*/
    BtShared *pBt = p->pBt;
    sqlite3BtreeEnter(p);
#ifndef SQLITE_OMIT_AUTOVACUUM
    if( pBt->autoVacuum ){
      rc = autoVacuumCommit(pBt);
      if( rc!=SQLITE_OK ){
        sqlite3BtreeLeave(p);
        return rc;
      }
    }
#endif
    rc = sqlite3PagerCommitPhaseOne(pBt->pPager, zMaster, 0);
    sqlite3BtreeLeave(p);
  }
  return rc;
}

/*
** This function is called from both BtreeCommitPhaseTwo() and BtreeRollback()
** at the conclusion of a transaction.
** 在一个事务的结束，BtreeCommitPhaseTwo()和BtreeRollback()调用这个函数。
*/
static void btreeEndTransaction(Btree *p){
  BtShared *pBt = p->pBt;
  assert( sqlite3BtreeHoldsMutex(p) );

  btreeClearHasContent(pBt);	/*销毁位图对象，回收用过的内存*/
  if( p->inTrans>TRANS_NONE && p->db->activeVdbeCnt>1 ){
    /* If there are other active statements that belong to this database
    ** handle, downgrade to a read-only transaction. The other statements
    ** may still be reading from the database.  
	** 如果有其他活跃的属于这个数据库处理程序的语句,下调一个只读事务。其他语句或许正在从数据库中读。*/
    downgradeAllSharedCacheTableLocks(p);
    p->inTrans = TRANS_READ;
  }else{
    /* If the handle had any kind of transaction open, decrement the 
    ** transaction count of the shared btree. If the transaction count 
    ** reaches 0, set the shared state to TRANS_NONE. The unlockBtreeIfUnused()
    ** call below will unlock the pager.  
	** 如果处理任何事务开放，减量可共享B树的事务数。如果事务数达到0,设置共享状态为TRANS_NONE。
	** unlockBtreeIfUnused()调用下面将解锁pager。*/
    if( p->inTrans!=TRANS_NONE ){ /*有事务*/
      clearAllSharedCacheTableLocks(p);
      pBt->nTransaction--;
      if( 0==pBt->nTransaction ){
        pBt->inTransaction = TRANS_NONE; /*事务处理完了*/
      }
    }

    /* Set the current transaction state to TRANS_NONE and unlock the 
    ** pager if this call closed the only read or write transaction.  
	** 设置当前事务状态TRANS_NONE并如果这个调用关闭唯一读或写事务则解锁pager。*/
    p->inTrans = TRANS_NONE;
    unlockBtreeIfUnused(pBt);/*当关闭了最后的读或写事务，解锁pager*/
  }

  btreeIntegrity(p);
}

/*
** Commit the transaction currently in progress.
** 提交当前在进程中的事务。
** This routine implements the second phase of a 2-phase commit.  The
** sqlite3BtreeCommitPhaseOne() routine does the first phase and should
** be invoked prior to calling this routine.  The sqlite3BtreeCommitPhaseOne()
** routine did all the work of writing information out to disk and flushing the
** contents so that they are written onto the disk platter.  All this
** routine has to do is delete or truncate or zero the header in the
** the rollback journal (which causes the transaction to commit) and
** drop locks.
**
** Normally, if an error occurs while the pager layer is attempting to 
** finalize the underlying journal file, this function returns an error and
** the upper layer will attempt a rollback. However, if the second argument
** is non-zero then this b-tree transaction is part of a multi-file 
** transaction. In this case, the transaction has already been committed 
** (by deleting a master journal file) and the caller will ignore this 
** functions return code. So, even if an error occurs in the pager layer,
** reset the b-tree objects internal state to indicate that the write
** transaction has been closed. This is quite safe, as the pager will have
** transitioned to the error state.
**
** This will release the write lock on the database file.  If there
** are no active cursors, it also releases the read lock.
*/

/*提交阶段分为2部分，这是第2部分。第一部分在sqlite3BtreeCommitPhaseOne
两者关系:
1、第一阶段调用后才能调用第二阶段。
2、第一阶段完成写信息到磁盘。第二阶段释放写锁，若无活动游标，释放读锁。
为什么分2个阶段?
保证所有节点在进行事务提交时保持一致性。在分布式系统中，每个节点虽然可以知晓自己的操作时成功
或者失败，却无法知道其他节点的操作的成功或失败。当一个事务跨越多个节点时，为了保持事务的
ACID特性，需要引入一个作为协调者的组件来统一掌控所有节点(称作参与者)的操作结果并最终指示
这些节点是否要把操作结果进行真正的提交(比如将更新后的数据写入磁盘等等)。因此，二阶段提交
的算法思路可以概括为： 参与者将操作成败通知协调者，再由协调者根据所有参与者的反馈情报决定
各参与者是否要提交操作还是中止操作。
*/

int sqlite3BtreeCommitPhaseTwo(Btree *p, int bCleanup){

  if( p->inTrans==TRANS_NONE ) return SQLITE_OK;
  sqlite3BtreeEnter(p);
  btreeIntegrity(p); /*检查事务处于一致性状态*/

  /* If the handle has a write-transaction open, commit the shared-btrees 
  ** transaction and set the shared state to TRANS_READ.
  ** 如果该句柄有开放性写事务,提交shared-btrees事务并设置事务共享状态为TRANS_READ。*/
  if( p->inTrans==TRANS_WRITE ){
    int rc;
    BtShared *pBt = p->pBt;
    assert( pBt->inTransaction==TRANS_WRITE );
    assert( pBt->nTransaction>0 );
    rc = sqlite3PagerCommitPhaseTwo(pBt->pPager); /*提交事务*/
    if( rc!=SQLITE_OK && bCleanup==0 ){
      sqlite3BtreeLeave(p);
      return rc;
    }
    pBt->inTransaction = TRANS_READ;
  }

  btreeEndTransaction(p);
  sqlite3BtreeLeave(p);
  return SQLITE_OK;
}

/*
** Do both phases of a commit.  两阶段事务提交
*/
int sqlite3BtreeCommit(Btree *p){
  int rc;
  sqlite3BtreeEnter(p);
  rc = sqlite3BtreeCommitPhaseOne(p, 0);
  if( rc==SQLITE_OK ){
    rc = sqlite3BtreeCommitPhaseTwo(p, 0);
  }
  sqlite3BtreeLeave(p);
  return rc;
}

#ifndef NDEBUG
/*
** Return the number of write-cursors open on this handle. This is for use
** in assert() expressions, so it is only compiled if NDEBUG is not
** defined.
** 这个句柄返回开放性写游标数。在assert()语句中使用，因此如果NDEBUG没有定义则它只编译。
** For the purposes of this routine, a write-cursor is any cursor that
** is capable of writing to the databse.  That means the cursor was
** originally opened for writing and the cursor has not be disabled
** by having its state changed to CURSOR_FAULT.
*/
/*返回写游标的数量*/
static int countWriteCursors(BtShared *pBt){
  BtCursor *pCur;
  int r = 0;
  for(pCur=pBt->pCursor; pCur; pCur=pCur->pNext){
    if( pCur->wrFlag && pCur->eState!=CURSOR_FAULT ) r++; /*pCur->eState!=CURSOR_FAULT时，游标处于激活状态*/
  }
  return r;
}
#endif

/*
** This routine sets the state to CURSOR_FAULT and the error
** code to errCode for every cursor on BtShared that pBtree
** references.
** 对于pBtree引用的BtShared上的游标这个函数将状态设置为CURSOR_FAULT和错误代码为errCode。
** Every cursor is tripped, including cursors that belong
** to other database connections that happen to be sharing
** the cache with pBtree.
** 每个游标都被遍历，包括属于其他数据库连接的游标，其中数据库连接正在与pBtree共享缓存。
** This routine gets called when a rollback occurs.
** All cursors using the same cache must be tripped
** to prevent them from trying to use the btree after
** the rollback.  The rollback may have deleted tables
** or moved root pages, so it is not sufficient to
** save the state of the cursor.  The cursor must be
** invalidated.
** 当发生回滚时这个函数被调用。所有使用相同的缓存的游标必须被遍历,以阻止他们在回滚之后试图利用btree。
** 回滚可能删除表或移动根页面,所以保存游标的状态是不够的。游标必须失效。
*/
/*将游标状态设置为 CURSOR_FAULT ，将error code设置为errCode*/
void sqlite3BtreeTripAllCursors(Btree *pBtree, int errCode){
  BtCursor *p;
  if( pBtree==0 ) return;
  sqlite3BtreeEnter(pBtree);
  for(p=pBtree->pBt->pCursor; p; p=p->pNext){
    int i;
    sqlite3BtreeClearCursor(p);
    p->eState = CURSOR_FAULT;
    p->skipNext = errCode;
    for(i=0; i<=p->iPage; i++){
      releasePage(p->apPage[i]);
      p->apPage[i] = 0;
    }
  }
  sqlite3BtreeLeave(pBtree);
}

/*
** Rollback the transaction in progress.  All cursors will be
** invalided by this operation.  Any attempt to use a cursor
** that was open at the beginning of this operation will result
** in an error.
** 回滚进程中的事务。通过该操作是所有游标失效。任何试图使用在这个操作开始时打开的游标都会出错。
** This will release the write lock on the database file.  If there
** are no active cursors, it also releases the read lock.
** 这将释放在数据库文件中的写锁。如果没有活跃的游标,也会释放读锁。
*/
/*回滚事务，使所有游标失效*/
int sqlite3BtreeRollback(Btree *p, int tripCode){
  int rc;
  BtShared *pBt = p->pBt;
  MemPage *pPage1;

  sqlite3BtreeEnter(p);
  if( tripCode==SQLITE_OK ){
    rc = tripCode = saveAllCursors(pBt, 0, 0);
  }else{
    rc = SQLITE_OK;
  }
  if( tripCode ){
    sqlite3BtreeTripAllCursors(p, tripCode);
  }
  btreeIntegrity(p);

  if( p->inTrans==TRANS_WRITE ){/*释放数据库中的写锁*/
    int rc2;

    assert( TRANS_WRITE==pBt->inTransaction );
    rc2 = sqlite3PagerRollback(pBt->pPager);
    if( rc2!=SQLITE_OK ){
      rc = rc2;
    }

    /* The rollback may have destroyed the pPage1->aData value.  So
    ** call btreeGetPage() on page 1 again to make
    ** sure pPage1->aData is set correctly. 
	** 回滚可能已经破坏了pPage1->aData价值。所以在1页上在此调用btreeGetPage()，确定pPage1->aData设置正确。*/
    if( btreeGetPage(pBt, 1, &pPage1, 0)==SQLITE_OK ){
      int nPage = get4byte(28+(u8*)pPage1->aData);
      testcase( nPage==0 );
      if( nPage==0 ) sqlite3PagerPagecount(pBt->pPager, &nPage);
      testcase( pBt->nPage!=nPage );
      pBt->nPage = nPage;
      releasePage(pPage1);
    }
    assert( countWriteCursors(pBt)==0 );
    pBt->inTransaction = TRANS_READ;/*若没有活动游标，释放读锁。*/
  }

  btreeEndTransaction(p);
  sqlite3BtreeLeave(p);
  return rc;
}
/*
** Start a statement subtransaction. The subtransaction can be rolled
** back independently of the main transaction. You must start a transaction 
** before starting a subtransaction. The subtransaction is ended automatically 
** if the main transaction commits or rolls back.
** 开始一个语句子事务。子事务可以被主事务独立的回滚。在开始子事务之前
** 要有一个主事务。如果主要事务提交或回滚，子事务自动结束。
** Statement subtransactions are used around individual SQL statements
** that are contained within a BEGIN...COMMIT block.  If a constraint
** error occurs within the statement, the effect of that one statement
** can be rolled back without having to rollback the entire transaction.
** 语句子事务使用在包含在一个BEGIN...COMMIT块中的单个SQL语句中。
** 如果在语句内出现一个约束错误,这个语句的效果可能是回滚,
** 然而并不需要回滚整个事务。
** A statement sub-transaction is implemented as an anonymous savepoint. The
** value passed as the second parameter is the total number of savepoints,
** including the new anonymous savepoint, open on the B-Tree. i.e. if there
** are no active savepoints and no other statement-transactions open,
** iStatement is 1. This anonymous savepoint can be released or rolled back
** using the sqlite3BtreeSavepoint() function.
** 声明sub-transaction被实现为一个匿名的保存点。作为第二个参数传递
** 的值是保存点的总数,包括新的匿名保存点,在b-ree开放的。即如果没有
** 活跃的保存点和没有其他statement-transactions开放,那么iStatement是1。
** 这个匿名的保存点可以使用sqlite3BtreeSavepoint()释放或回滚。
*/
int sqlite3BtreeBeginStmt(Btree *p, int iStatement){  //开始一个语句子事务
  int rc;
  BtShared *pBt = p->pBt;
  sqlite3BtreeEnter(p);
  assert( p->inTrans==TRANS_WRITE );
  assert( (pBt->btsFlags & BTS_READ_ONLY)==0 );
  assert( iStatement>0 );
  assert( iStatement>p->db->nSavepoint );
  assert( pBt->inTransaction==TRANS_WRITE );
  /* At the pager level, a statement transaction is a savepoint with
  ** an index greater than all savepoints created explicitly using
  ** SQL statements. It is illegal to open, release or rollback any
  ** such savepoints while the statement transaction savepoint is active.
  ** 在pager 水平上,语句事务是一个保存点,带有一个大于明确使用SQL语句创建的所有保存
  ** 点的索引。当语句事务保存点活跃时，开放，释放或回滚任何这样的保存点都是非法的。
  */
  rc = sqlite3PagerOpenSavepoint(pBt->pPager, iStatement);
  sqlite3BtreeLeave(p);
  return rc;
}
/*
** The second argument to this function, op, is always SAVEPOINT_ROLLBACK
** or SAVEPOINT_RELEASE. This function either releases or rolls back the
** savepoint identified by parameter iSavepoint, depending on the value 
** of op.
** 该函数的第二个参数,op,总是SAVEPOINT_ROLLBACK或SAVEPOINT_RELEASE。
** 这个函数释放或回滚被参数iSavepoint识别的保存点,是依赖于op的值。
** Normally, iSavepoint is greater than or equal to zero. However, if op is
** SAVEPOINT_ROLLBACK, then iSavepoint may also be -1. In this case the 
** contents of the entire transaction are rolled back. This is different
** from a normal transaction rollback, as no locks are released and the
** transaction remains open.
** 通常,iSavepoint>=0。然而,如果op是 SAVEPOINT_ROLLBACK,那么iSavepoint也可能是1。在这种
** 情况下, 整个事务的内容回滚。这与正常的事务回滚是不同的,因为没有锁释放,事务仍然开放。
*/
/*op为SAVEPOINT_ROLLBACK或SAVEPOINT_RELEASE，根据此值释放或者回滚保存点*/
int sqlite3BtreeSavepoint(Btree *p, int op, int iSavepoint){    //是释放还是回滚依赖于参数op的值
  int rc = SQLITE_OK;
  if( p && p->inTrans==TRANS_WRITE ){
    BtShared *pBt = p->pBt;
    assert( op==SAVEPOINT_RELEASE || op==SAVEPOINT_ROLLBACK );
    assert( iSavepoint>=0 || (iSavepoint==-1 && op==SAVEPOINT_ROLLBACK) );
    sqlite3BtreeEnter(p);
    rc = sqlite3PagerSavepoint(pBt->pPager, op, iSavepoint);
    if( rc==SQLITE_OK ){
      if( iSavepoint<0 && (pBt->btsFlags & BTS_INITIALLY_EMPTY)!=0 ){
        pBt->nPage = 0;
      }
      rc = newDatabase(pBt);
      pBt->nPage = get4byte(28 + pBt->pPage1->aData);
      /* The database size was written into the offset 28 of the header
      ** when the transaction started, so we know that the value at offset
      ** 28 is nonzero. 
	  ** 在事务开始时，数据库的大小是被写到头部的偏移量28处的，因此在偏移量28的值是非零的。*/
      assert( pBt->nPage>0 );
    }
    sqlite3BtreeLeave(p);
  }
  return rc;
}

/*
** Create a new cursor for the BTree whose root is on the page
** iTable. If a read-only cursor is requested, it is assumed that
** the caller already has at least a read-only transaction open
** on the database already. If a write-cursor is requested, then
** the caller is assumed to have an open write transaction.
**
** If wrFlag==0, then the cursor can only be used for reading.
** If wrFlag==1, then the cursor can be used for reading or for
** writing if other conditions for writing are also met.  These
** are the conditions that must be met in order for writing to
** be allowed:
**
** 1:  The cursor must have been opened with wrFlag==1
**
** 2:  Other database connections that share the same pager cache
**     but which are not in the READ_UNCOMMITTED state may not have
**     cursors open with wrFlag==0 on the same table.  Otherwise
**     the changes made by this write cursor would be visible to
**     the read cursors in the other database connection.
**
** 3:  The database must be writable (not on read-only media)
**
** 4:  There must be an active transaction.
**
** No checking is done to make sure that page iTable really is the
** root page of a b-tree.  If it is not, then the cursor acquired
** will not work correctly.
**
** It is assumed that the sqlite3BtreeCursorZero() has been called
** on pCur to initialize the memory space prior to invoking this routine.
*/
/*
为BTree创建一个新的游标，B树的根在页iTable上。
如果请求一个只读游标，数据库上至少有一个只读事务打开。
如果被请求的是写游标，必须有一打开的写事务。
如wrFlag== 0，则游标仅能用于读取。
如wrFlag== 1，则游标可用于读或者用于写。
1：wrFlag==1游标必须已经打开。
2：共享相同的页缓存， 不是READ_UNCOMMITTED状态，wrFlag==0 时，
游标可能不是打开状态。
3：数据库必须是可写的（而不是只读介质）
4：必须有一个活动的事务。
假设在调用这个程序之前，sqlite3BtreeCursorZero（）被调用，
用pCur初始化内存空间。
*/
static int btreeCursor(
  Btree *p,                              /* The btree */                                  //p为B树
  int iTable,                            /* Root page of table to open */      //开放的表的根页
  int wrFlag                           /* 1 to write. 0 read-only */              //wrFlag为1表示写，0表示只读
  struct KeyInfo *pKeyInfo,              /* First arg to comparison function */     //比较函数的第一个参数
  BtCursor *pCur                         /* Space for new cursor */        //新游标空间
){
  BtShared *pBt = p->pBt;                /* Shared b-tree handle */   //可共享B树句柄

  assert( sqlite3BtreeHoldsMutex(p) );
  assert( wrFlag==0 || wrFlag==1 );

  /* The following assert statements verify that if this is a sharable 
  ** b-tree database, the connection is holding the required table locks, 
  ** and that no other connection has any open cursor that conflicts with 
  ** this lock.  
  ** 下面的断言函数语句验证是不是一个可共享的B树数据库，验证连接持有需要的表锁并且
  ** 没有其他连接与该锁冲突的任何开放的游标。
  */
	/*下面的语句验证，如果这是一个可共享
B树数据库，连接持有所需的表锁，
并没有其他连接具有任何打开的游标与此锁冲突
 */

  assert( hasSharedCacheTableLock(p, iTable, pKeyInfo!=0, wrFlag+1) );
  assert( wrFlag==0 || !hasReadConflicts(p, iTable) );

  /* Assert that the caller has opened the required transaction. */  //断言调用者以开放了所需的事务。
  assert( p->inTrans>TRANS_NONE );
  assert( wrFlag==0 || p->inTrans==TRANS_WRITE );
  assert( pBt->pPage1 && pBt->pPage1->aData );

  if( NEVER(wrFlag && (pBt->btsFlags & BTS_READ_ONLY)!=0) ){
    return SQLITE_READONLY;
  }
  if( iTable==1 && btreePagecount(pBt)==0 ){
    assert( wrFlag==0 );
    iTable = 0;
  }

  /* Now that no other errors can occur, finish filling in the BtCursor
  ** variables and link the cursor into the BtShared list.  
  ** 现在没有其他错误发生,完成给BtCursor变量赋值和链接游标到BtShared列表。*/
  pCur->pgnoRoot = (Pgno)iTable;
  pCur->iPage = -1;
  pCur->pKeyInfo = pKeyInfo;
  pCur->pBtree = p;
  pCur->pBt = pBt;
  pCur->wrFlag = (u8)wrFlag;
  pCur->pNext = pBt->pCursor;
  if( pCur->pNext ){
    pCur->pNext->pPrev = pCur;
  }
  pBt->pCursor = pCur;
  pCur->eState = CURSOR_INVALID;
  pCur->cachedRowid = 0;
  return SQLITE_OK;
}
/*
创建一个指向特定B-tree的游标。游标可以是读游标，也可以是写游标，但是读游标和写游标不能同时在
同一个B-tree中存在。
*/
int sqlite3BtreeCursor(
  Btree *p,                                   /* The btree */                                        //p为B树
  int iTable,                                 /* Root page of table to open */            //开放的表的根页
  int wrFlag,                                 /* 1 to write. 0 read-only */                  //wrFlag为1表示写，0表示只读
  struct KeyInfo *pKeyInfo,                   /* First arg to xCompare() */   //比较函数的第一个参数
  BtCursor *pCur                              /* Write new cursor here */            //写新的游标到这里
){
  int rc;
  sqlite3BtreeEnter(p);
  rc = btreeCursor(p, iTable, wrFlag, pKeyInfo, pCur);
  sqlite3BtreeLeave(p);
  return rc;
}

/*
** Return the size of a BtCursor object in bytes.
** 返回BtCursor对象的字节大小
** This interfaces is needed so that users of cursors can preallocate
** sufficient storage to hold a cursor.  The BtCursor object is opaque
** to users so they cannot do the sizeof() themselves - they must call
** this routine.
** 这个接口是为了游标的用户可以预先分配足够存储空间来存放一个游标。
** BtCursor对象对用户不透明,所以他们不能做sizeof()—必须调用这个函数。
*/
	/*
	**返回一个BtCursor对象的大小(以字节计)。需要该接口使游标可以预先分配	
	**足够的存储空间。该BtCursor对象对用户是不透明的
	**对用户，他们不能用sizeof（）- 他们必须调用此程序。
	*/
int sqlite3BtreeCursorSize(void){  //返回BtCursor对象的字节大小
  return ROUND8(sizeof(BtCursor));
}

/*
** Initialize memory that will be converted into a BtCursor object.
**
** The simple approach here would be to memset() the entire object
** to zero.  But it turns out that the apPage[] and aiIdx[] arrays
** do not need to be zeroed and they are large, so we can save a lot
** of run-time by skipping the initialization of those elements.
*/
/*
** 初始化将被转换成一个BtCursor对象的存储器。
** 这里一个简单方法是用memset()将整个对象置为零。但事实证明，apPage[]和aiIdx[]数组不需要
** 进行调零，他们比较大，所以我们通过跳过这些元素的初始化，可以节省很多的运行时间。
*/
void sqlite3BtreeCursorZero(BtCursor *p){  //初始化将被转换成一个BtCursor对象的存储器
  memset(p, 0, offsetof(BtCursor, iPage)); //用memset()将整个对象置为零
}

/*
** Set the cached rowid value of every cursor in the same database file
** as pCur and having the same root page number as pCur.  The value is
** set to iRowid.
**
** Only positive rowid values are considered valid for this cache.
** The cache is initialized to zero, indicating an invalid cache.
** A btree will work fine with zero or negative rowids.  We just cannot
** cache zero or negative rowids, which means tables that use zero or
** negative rowids might run a little slower.  But in practice, zero
** or negative rowids are very uncommon so this should not be a problem.
*/
	/*
	**相同的数据库文件中设置每个游标的cache行号。
	**该值设置为iRowid。只有正的rowid值被认为是适用于该缓存。
	**高速缓存被初始化为零，表示一个无效的高速缓冲存储器。
	**一个B树有零或负的rowid将正常工作。
	**高速缓存为零或负的rowid不行，这意味着表使用零或
	**负的rowid可能运行慢一点。但在实践中，零
	**或负的rowid非常少见所以这不应该是一个问题。
	*/

void sqlite3BtreeSetCachedRowid(BtCursor *pCur, sqlite3_int64 iRowid){   //设置相同的数据库文件中中每个游标的cache行号
  BtCursor *p;
  for(p=pCur->pBt->pCursor; p; p=p->pNext){
    if( p->pgnoRoot==pCur->pgnoRoot ) p->cachedRowid = iRowid;
  }
  assert( pCur->cachedRowid==iRowid );
}

/*
** Return the cached rowid for the given cursor.  A negative or zero
** return value indicates that the rowid cache is invalid and should be
** ignored.  If the rowid cache has never before been set, then a
** zero is returned.
*/
/*
**返回游标的缓存的rowid。负或为零的
**返回值表示其rowid高速缓存无效，并应
**忽略。如果rowid缓存以前从未被设置，则
**返回零。
*/
sqlite3_int64 sqlite3BtreeGetCachedRowid(BtCursor *pCur){      //返回游标的缓存的rowid
  return pCur->cachedRowid;
}

/*
** Close a cursor.  The read lock on the database file is released
** when the last cursor is closed.
** 关闭B-tree游标。当最后游标关闭时释放数据库上的读锁。
*/   /*关闭B-tree游标*/
int sqlite3BtreeCloseCursor(BtCursor *pCur){  //关闭B-tree游标
  Btree *pBtree = pCur->pBtree;
  if( pBtree ){
    int i;
    BtShared *pBt = pCur->pBt;
    sqlite3BtreeEnter(pBtree);
    sqlite3BtreeClearCursor(pCur);
    if( pCur->pPrev ){
      pCur->pPrev->pNext = pCur->pNext;
    }else{
      pBt->pCursor = pCur->pNext;
    }
    if( pCur->pNext ){
      pCur->pNext->pPrev = pCur->pPrev;
    }
    for(i=0; i<=pCur->iPage; i++){
      releasePage(pCur->apPage[i]);
    }
    unlockBtreeIfUnused(pBt);
    invalidateOverflowCache(pCur);
    /* sqlite3_free(pCur); */
    sqlite3BtreeLeave(pBtree);
  }
  return SQLITE_OK;
}

/*
** Make sure the BtCursor* given in the argument has a valid
** BtCursor.info structure.  If it is not already valid, call
** btreeParseCell() to fill it in.
** 确保在argument中给出的BtCursor 有一个有效的BtCursor.info结构。如果尚未有效,调用btreeParseCell()使之有效。
** BtCursor.info is a cache of the information in the current cell.
** Using this cache reduces the number of calls to btreeParseCell().
** BtCursor.info是一个在当前单元中的信息缓存。使用这个缓存减少调用btreeParseCell()的数量。
** 2007-06-25:  There is a bug in some versions of MSVC that cause the
** compiler to crash when getCellInfo() is implemented as a macro.
** But there is a measureable speed advantage to using the macro on gcc
** (when less compiler optimizations like -Os or -O0 are used and the
** compiler is not doing agressive inlining.)  So we use a real function
** for MSVC and a macro for everything else.  Ticket #2457.
*/
#ifndef NDEBUG
  static void assertCellInfo(BtCursor *pCur){       /*保证BtCursor有有效的BtCursor.info结构。若无效，调用btreeParseCell()去填充*/
    CellInfo info;
    int iPage = pCur->iPage;
    memset(&info, 0, sizeof(info));                        //将info中前sizeof(info)个字节 用0替换并返回info 。
    btreeParseCell(pCur->apPage[iPage], pCur->aiIdx[iPage], &info);        //解析单元内容块
    assert( memcmp(&info, &pCur->info, sizeof(info))==0 );
  }
#else
  #define assertCellInfo(x)
#endif
#ifdef _MSC_VER
  /* Use a real function in MSVC to work around bugs in that compiler. */   //在MSVC中使用一个真正的函数解决编译器错误。
  static void getCellInfo(BtCursor *pCur){
    if( pCur->info.nSize==0 ){
      int iPage = pCur->iPage;
      btreeParseCell(pCur->apPage[iPage],pCur->aiIdx[iPage],&pCur->info);
      pCur->validNKey = 1;
    }else{
      assertCellInfo(pCur);
    }
  }
#else /* if not _MSC_VER */
  /* Use a macro in all other compilers so that the function is inlined */  //在所有其他编译器使用宏,这样函数是联机的。
#define getCellInfo(pCur)                                                      \
  if( pCur->info.nSize==0 ){                                                   \
    int iPage = pCur->iPage;                                                   \
    btreeParseCell(pCur->apPage[iPage],pCur->aiIdx[iPage],&pCur->info); \
    pCur->validNKey = 1;                                                       \
  }else{                                                                       \
    assertCellInfo(pCur);                                                      \
  }
#endif /* _MSC_VER */

#ifndef NDEBUG  /* The next routine used only within assert() statements */  //下一个函数只在assert()语句使用。
/*
** Return true if the given BtCursor is valid.  A valid cursor is one
** that is currently pointing to a row in a (non-empty) table.
** This is a verification routine is used only within assert() statements.
*/
	/*
	**如果给定的BtCursor是有效的,返回true。一个有效的游标是在非空
	**的表中当前指向的行。这是一个验证程序只在assert()语句中使用。
	*/
int sqlite3BtreeCursorIsValid(BtCursor *pCur){          //给定的BtCursor是否有效
  return pCur && pCur->eState==CURSOR_VALID;
}
#endif /* NDEBUG */

/*
** Set *pSize to the size of the buffer needed to hold the value of
** the key for the current entry.  If the cursor is not pointing
** to a valid entry, *pSize is set to 0. 
** pSize为buffer的大小，buffer用来保存当前条目(用pCur指向)的key值。如果游标未指向一个有效的条目,* pSize设置为0。
** For a table with the INTKEY flag set, this routine returns the key
** itself, not the number of bytes in the key.
** 对INTKEY标志的表设置,这个函数返回关键字本身,而不是关键字的字节数。
** The caller must position the cursor prior to invoking this routine.
** 调用者
** This routine cannot fail.  It always returns SQLITE_OK.  
*/
/*pSize为buffer的大小，buffer用来保存当前条目(用pCur指向)的key值。*/
int sqlite3BtreeKeySize(BtCursor *pCur, i64 *pSize){
  assert( cursorHoldsMutex(pCur) );
  assert( pCur->eState==CURSOR_INVALID || pCur->eState==CURSOR_VALID );
  if( pCur->eState!=CURSOR_VALID ){          /*游标指向无效条目，pSize = 0*/
    *pSize = 0;
  }else{
    getCellInfo(pCur);
    *pSize = pCur->info.nKey;
  }
  return SQLITE_OK;
}

/*
** Set *pSize to the number of bytes of data in the entry the
** cursor currently points to.
** 设置*pSize的数据域的字节数，*pSize 在当前游标指向的条目中。
** The caller must guarantee that the cursor is pointing to a non-NULL
** valid entry.  In other words, the calling procedure must guarantee
** that the cursor has Cursor.eState==CURSOR_VALID.
** 调用者必须保证游标指向一个非空有效条目。换句话说,调用程序必须保证游标Cursor.eState = = CURSOR_VALID。
** Failure is not possible.  This function always returns SQLITE_OK.
** It might just as well be a procedure (returning void) but we continue
** to return an integer result code for historical reasons.
** 这个函数始终返回SQLITE_OK。也可能仅仅是一个过程(返回void)但是我们继续返回一个整数结果代码。
*/

int sqlite3BtreeDataSize(BtCursor *pCur, u32 *pSize){      /*设定当前游标所指记录的数据长度（字节）*/
  assert( cursorHoldsMutex(pCur) );
  assert( pCur->eState==CURSOR_VALID );
  getCellInfo(pCur);
  *pSize = pCur->info.nData;/*用pSize返回数据长度*/
  return SQLITE_OK;
}

/*
** Given the page number of an overflow page in the database (parameter
** ovfl), this function finds the page number of the next page in the 
** linked list of overflow pages. If possible, it uses the auto-vacuum
** pointer-map data instead of reading the content of page ovfl to do so. 
** 给出一个在数据库中的溢出页页码(参数为ovfl),这个函数找到下一个溢出页面的链表中
** 的页面的页码。如果可能,它使用auto-vacuum  pointer-map数据而不是读页面ovfl的内容。
** If an error occurs an SQLite error code is returned. Otherwise:
** 如果出现错误一个SQLite返回错误代码。否则：
** The page number of the next overflow page in the linked list is 
** written to *pPgnoNext. If page ovfl is the last page in its linked 
** list, *pPgnoNext is set to zero. 
** 链接列表中的下一个溢出页面的页码被写到* pPgnoNext中。如果ovfl页是最后一页,* pPgnoNext设置为零。
** If ppPage is not NULL, and a reference to the MemPage object corresponding
** to page number pOvfl was obtained, then *ppPage is set to point to that
** reference. It is the responsibility of the caller to call releasePage()
** on *ppPage to free the reference. In no reference was obtained (because
** the pointer-map was used to obtain the value for *pPgnoNext), then
** *ppPage is set to zero.
** 如果ppPage非空,对 MemPage对象相应页码pOvfl的引用被获得,则设置* ppPage指向引用。
** 负责调用者的调用releasePage()在ppPage上释放引用 。在没有引用获得(因为 pointer-map被用来获得* pPgnoNext的值), 
** 那么 * ppPage设置为零。
*/
/*找到下一个溢出页的页号。*/
static int getOverflowPage(
  BtShared *pBt,               /* The database file */                                                       //数据库文件
  Pgno ovfl,                   /* Current overflow page number */                                    //当前溢出页号     
  MemPage **ppPage,            /* OUT: MemPage handle (may be NULL) */         //内存页句柄（可能为NULL）
  Pgno *pPgnoNext              /* OUT: Next overflow page number */                       //下一个溢出页的页号
){
  Pgno next = 0;
  MemPage *pPage = 0;
  int rc = SQLITE_OK;

  assert( sqlite3_mutex_held(pBt->mutex) );
  assert(pPgnoNext);

#ifndef SQLITE_OMIT_AUTOVACUUM
  /* Try to find the next page in the overflow list using the
  ** autovacuum pointer-map pages. Guess that the next page in 
  ** the overflow list is page number (ovfl+1). If that guess turns 
  ** out to be wrong, fall back to loading the data of page 
  ** number ovfl to determine the next page number.
  ** 试图找到溢出列表中的在autovacuum pointer-map页面中使用的下一个页面。猜测溢出列表
  ** 中下一个页面页号为(ovfl + 1)。如果猜测是错的,回到加载页号ovfl的数据来确定下一个页码。
  */
  if( pBt->autoVacuum ){
    Pgno pgno;
    Pgno iGuess = ovfl+1;/*猜测下一个溢出页页号为ovfl+1*/
    u8 eType;

    while( PTRMAP_ISPAGE(pBt, iGuess) || iGuess==PENDING_BYTE_PAGE(pBt) ){
      iGuess++;/*没猜对，页号往后加*/
    }

    if( iGuess<=btreePagecount(pBt) ){
      rc = ptrmapGet(pBt, iGuess, &eType, &pgno);
      if( rc==SQLITE_OK && eType==PTRMAP_OVERFLOW2 && pgno==ovfl ){
        next = iGuess;
        rc = SQLITE_DONE;
      }
    }
  }
#endif

  assert( next==0 || rc==SQLITE_DONE );
  if( rc==SQLITE_OK ){
    rc = btreeGetPage(pBt, ovfl, &pPage, 0);
    assert( rc==SQLITE_OK || pPage==0 );
    if( rc==SQLITE_OK ){
      next = get4byte(pPage->aData);
    }
  }

  *pPgnoNext = next;
  if( ppPage ){/*ppPage不空，ppPage指向reference*/
    *ppPage = pPage;
  }else{
    releasePage(pPage);
  }
  return (rc==SQLITE_DONE ? SQLITE_OK : rc);
}

/*
** Copy data from a buffer to a page, or from a page to a buffer.
** 将数据从缓冲区复制到一个页面,或从一个页面到缓冲区。
** pPayload is a pointer to data stored on database page pDbPage.
** If argument eOp is false, then nByte bytes of data are copied
** from pPayload to the buffer pointed at by pBuf. If eOp is true,
** then sqlite3PagerWrite() is called on pDbPage and nByte bytes
** of data are copied from the buffer pBuf to pPayload.
** pPayload是一个指针指向存储在数据库页pDbPage上的数据。如果参数eOp 为假,那么nByte字节的数据从pPayload复制 到pBuf
** 指向的缓冲区指着。如果eOp为真, 然后在pDbPage上调用sqlite3PagerWrite()并且nByte字节的数据从pBuf复制 到pPayload。
** SQLITE_OK is returned on success, otherwise an error code.
** 成功则返回SQLITE_OK ，否则返回错误代码。
*/
	/*
	**从缓冲区复制数据到一个页面，或者从一个页面复制到缓冲器。
	**
	** pPayload是一个指向存储在数据库页面pDbPage数据的指针。
	**如果参数EOP是假的，那么nByte字节数据被复制
	**通过PBUF从pPayload到缓冲区指向。如果EOP是true，
	**然后sqlite3PagerWrite（）被调用，nByte字节
	**数据从缓冲器 pBuf 到pPayload被复制。
	** 成功返回SQLITE_OK，否则返回错误代码。
	*/

static int copyPayload(                      //将数据从缓冲区复制到一个页面,或从一个页面到缓冲区
  void *pPayload,           /* Pointer to page data */                             //页面数据的指针
  void *pBuf,               /* Pointer to buffer */                                     //缓存区指针
  int nByte,                /* Number of bytes to copy */                          //拷贝的字节数
  int eOp,                  /* 0 -> copy from page, 1 -> copy to page */   //eOp为0从页拷贝到缓存区，为1则从缓存区拷贝到页
  DbPage *pDbPage           /* Page containing pPayload */               //页包含pPayload
){
  if( eOp ){
    /* Copy data from buffer to page (a write operation) */  //为1则从缓存区拷贝到页
    int rc = sqlite3PagerWrite(pDbPage);
    if( rc!=SQLITE_OK ){
      return rc;
    }
    memcpy(pPayload, pBuf, nByte);
  }else{
    /* Copy data from page to buffer (a read operation) */    //eOp为0从页拷贝到缓存区
    memcpy(pBuf, pPayload, nByte);
  }
  return SQLITE_OK;
}

/*
** This function is used to read or overwrite payload information
** for the entry that the pCur cursor is pointing to. If the eOp
** parameter is 0, this is a read operation (data copied into
** buffer pBuf). If it is non-zero, a write (data copied from
** buffer pBuf).
**
** A total of "amt" bytes are read or written beginning at "offset".
** Data is read to or from the buffer pBuf.
**
** The content being read or written might appear on the main page
** or be scattered out on multiple overflow pages.
**
** If the BtCursor.isIncrblobHandle flag is set, and the current
** cursor entry uses one or more overflow pages, this function
** allocates space for and lazily popluates the overflow page-list 
** cache array (BtCursor.aOverflow). Subsequent calls use this
** cache to make seeking to the supplied offset more efficient.
**
** Once an overflow page-list cache has been allocated, it may be
** invalidated if some other cursor writes to the same table, or if
** the cursor is moved to a different row. Additionally, in auto-vacuum
** mode, the following events may invalidate an overflow page-list cache.
**
**   * An incremental vacuum,
**   * A commit in auto_vacuum="full" mode,
**   * Creating a table (may require moving an overflow page).
*/
	/*
	** 对于游标pCur正指向的条目，此功能用于读或覆写有效载荷信息。
	** 如果参数eOp为0，这是一个读操作（数据复制到缓存pBuf）。
	** 如果它不为零，数据从缓存 pBuf中复制到页。
	** 总共有“amt”字节的数据被读或写从"offset"开始.
	** 正在读或写的内容可能出现在主页上或分散在多个溢出页。 如果设置了BtCursor.isIncrblobHandle标志,
	** 且当前游标条目使用一个或多个溢出页,这个函数分配空间和慢慢增加溢出页列表缓存数组(BtCursor.aOverflow)。
	** 后续调用使用这个缓存使寻求提供的偏移更有效率。
	** 一旦溢出页列表缓存已经被分配，如果其他游标写入
	** 同一个表则无效,或者游标移动到不同行。此外,在auto-vacuum 模式,下面的事件可能使一个缓存溢出页列表缓存无效。
	**   * 增量式清理,
	**   * 在 auto_vacuum="full" 模式中的一个提交,
	**   * 创建表 (可能要求移动一个溢出页).
	*/

static int accessPayload(          //读或覆写有效载荷信息
  BtCursor *pCur,      /* Cursor pointing to entry to read from */     //游标指向要读取数据的条目
  u32 offset,          /* Begin reading this far into payload */               //开始进一步读到有效载荷
  u32 amt,             /* Read this many bytes */                                       //读取大量字节
  unsigned char *pBuf, /* Write the bytes into this buffer */             //写这写数据到缓存区
  int eOp              /* zero to read. non-zero to write. */                        //零读，非零写
){
  unsigned char *aPayload;
  int rc = SQLITE_OK;
  u32 nKey;
  int iIdx = 0;
  MemPage *pPage = pCur->apPage[pCur->iPage]; /* Btree page of current entry */  //当前条目的B树页
  BtShared *pBt = pCur->pBt;                  /* Btree this cursor belongs to */                    //该游标所属的B树

  assert( pPage );
  assert( pCur->eState==CURSOR_VALID );
  assert( pCur->aiIdx[pCur->iPage]<pPage->nCell );
  assert( cursorHoldsMutex(pCur) );

  getCellInfo(pCur);
  aPayload = pCur->info.pCell + pCur->info.nHeader;
  nKey = (pPage->intKey ? 0 : (int)pCur->info.nKey);

  if( NEVER(offset+amt > nKey+pCur->info.nData) 
   || &aPayload[pCur->info.nLocal] > &pPage->aData[pBt->usableSize]
  ){
    /* Trying to read or write past the end of the data is an error */  //尝试去读或写数据末端以后的数据将发生一个错误
    return SQLITE_CORRUPT_BKPT;
  }

  /* Check if data must be read/written to/from the btree page itself. */  //检查是否从B树页本身去读取或者写
  if( offset<pCur->info.nLocal ){
    int a = amt;
    if( a+offset>pCur->info.nLocal ){
      a = pCur->info.nLocal - offset;
    }
    rc = copyPayload(&aPayload[offset], pBuf, a, eOp, pPage->pDbPage);
    offset = 0;
    pBuf += a;
    amt -= a;
  }else{
    offset -= pCur->info.nLocal;
  }

  if( rc==SQLITE_OK && amt>0 ){
    const u32 ovflSize = pBt->usableSize - 4;  /* Bytes content per ovfl page */  //每个ovfl页的数个字节内容
    Pgno nextPage;

    nextPage = get4byte(&aPayload[pCur->info.nLocal]);

#ifndef SQLITE_OMIT_INCRBLOB
    /* If the isIncrblobHandle flag is set and the BtCursor.aOverflow[]
    ** has not been allocated, allocate it now. The array is sized at
    ** one entry for each overflow page in the overflow chain. The
    ** page number of the first overflow page is stored in aOverflow[0],
    ** etc. A value of 0 in the aOverflow[] array means "not yet known"
    ** (the cache is lazily populated).
    */
    /*如果isIncrblobHandle标志被设置和BtCursor.aOverflow[]尚未分配，现在分配它。   
	aOverflow[]中的0表示“还发现”。
	*/
    if( pCur->isIncrblobHandle && !pCur->aOverflow ){
      int nOvfl = (pCur->info.nPayload-pCur->info.nLocal+ovflSize-1)/ovflSize;
      pCur->aOverflow = (Pgno *)sqlite3MallocZero(sizeof(Pgno)*nOvfl);
      /* nOvfl is always positive.  If it were zero, fetchPayload would have
      ** been used instead of this routine. */
      if( ALWAYS(nOvfl) && !pCur->aOverflow ){
        rc = SQLITE_NOMEM;
      }
    }

    /* If the overflow page-list cache has been allocated and the
    ** entry for the first required overflow page is valid, skip
    ** directly to it.
    */
    /*如果溢出页列表缓存已分配，第一个溢出页项是有效的，直接跳过它。	*/  
    if( pCur->aOverflow && pCur->aOverflow[offset/ovflSize] ){
      iIdx = (offset/ovflSize);
      nextPage = pCur->aOverflow[iIdx];
      offset = (offset%ovflSize);
    }
#endif

    for( ; rc==SQLITE_OK && amt>0 && nextPage; iIdx++){

#ifndef SQLITE_OMIT_INCRBLOB
      /* If required, populate the overflow page-list cache. */   //如果需要,填充溢出页列表缓存。
      if( pCur->aOverflow ){
        assert(!pCur->aOverflow[iIdx] || pCur->aOverflow[iIdx]==nextPage);
        pCur->aOverflow[iIdx] = nextPage;
      }
#endif

      if( offset>=ovflSize ){
        /* The only reason to read this page is to obtain the page
        ** number for the next page in the overflow chain. The page
        ** data is not required. So first try to lookup the overflow
        ** page-list cache, if any, then fall back to the getOverflowPage()
        ** function.
        */
        /*读此页的唯一原因是为了获得该页面在溢出页链表中的下一个页号。
        页面不需要数据。所以，先试着查找溢出页列表缓存，如果有的话，
        则退回到getOverflowPage()函数。
*/
#ifndef SQLITE_OMIT_INCRBLOB
        if( pCur->aOverflow && pCur->aOverflow[iIdx+1] ){
          nextPage = pCur->aOverflow[iIdx+1];
        } else 
#endif
          rc = getOverflowPage(pBt, nextPage, 0, &nextPage);
        offset -= ovflSize;
      }else{
        /* Need to read this page properly. It contains some of the
        ** range of data that is being read (eOp==0) or written (eOp!=0).
		** 需要正确地读这个页面。它包含的一些正在被读取(eOp==0)或写(eOp! = 0)的数据范围。
        */
#ifdef SQLITE_DIRECT_OVERFLOW_READ
        sqlite3_file *fd;
#endif
        int a = amt;
        if( a + offset > ovflSize ){
          a = ovflSize - offset;
        }

#ifdef SQLITE_DIRECT_OVERFLOW_READ
        /* If all the following are true:
        **
        **   1) this is a read operation, and 
        **   2) data is required from the start of this overflow page, and
        **   3) the database is file-backed, and
        **   4) there is no open write-transaction, and
        **   5) the database is not a WAL database,
        **
        ** then data can be read directly from the database file into the
        ** output buffer, bypassing the page-cache altogether. This speeds
        ** up loading large records that span many overflow pages.
        */
        /*如果所有的下列条件为真：
		1）这是一个读操作，并且
		2）数据被溢出页请求，并且
		3）数据库文件的支持，并且
		4）没有开放性写事务，
		5）数据库不是一个WAL数据库，
		然后数据可以直接从数据库文件读入到
		输出缓冲。这加快了溢出页的大记录的加载。
*/
        if( eOp==0                                             /* (1) */
         && offset==0                                          /* (2) */
         && pBt->inTransaction==TRANS_READ                     /* (4) */
         && (fd = sqlite3PagerFile(pBt->pPager))->pMethods     /* (3) */
         && pBt->pPage1->aData[19]==0x01                       /* (5) */
        ){
          u8 aSave[4];
          u8 *aWrite = &pBuf[-4];
          memcpy(aSave, aWrite, 4);
          rc = sqlite3OsRead(fd, aWrite, a+4, (i64)pBt->pageSize*(nextPage-1));
          nextPage = get4byte(aWrite);
          memcpy(aWrite, aSave, 4);
        }else
#endif

        {
          DbPage *pDbPage;
          rc = sqlite3PagerGet(pBt->pPager, nextPage, &pDbPage);
          if( rc==SQLITE_OK ){
            aPayload = sqlite3PagerGetData(pDbPage);
            nextPage = get4byte(aPayload);
            rc = copyPayload(&aPayload[offset+4], pBuf, a, eOp, pDbPage);
            sqlite3PagerUnref(pDbPage);
            offset = 0;
          }
        }
        amt -= a;
        pBuf += a;
      }
    }
  }

  if( rc==SQLITE_OK && amt>0 ){
    return SQLITE_CORRUPT_BKPT;
  }
  return rc;
}

/*
** Read part of the key associated with cursor pCur.  Exactly
** "amt" bytes will be transfered into pBuf[].  The transfer
** begins at "offset".
** 读与游标pCur关联的关键字."amt"字节数将被传递到数组pBuf[]中.传递从"offset"开始.
** The caller must ensure that pCur is pointing to a valid row
** in the table.
** 调用者必须确保pCur指向表中有效的行。
** Return SQLITE_OK on success or an error code if anything goes
** wrong.  An error is returned if "offset+amt" is larger than
** the available payload.
** 成功则返回SQLITE_OK或如果任何错误发生则返回错误代码。如果 "offset+amt"比可用的有效载荷还大则返回一个错误。
*/
/*若游标pCur指向表中有效的一行，返回SQLITE_OK，若"offset+amt">有效负载，返回error code*/
int sqlite3BtreeKey(BtCursor *pCur, u32 offset, u32 amt, void *pBuf){
  assert( cursorHoldsMutex(pCur) );
  assert( pCur->eState==CURSOR_VALID );/*游标指向表中有效的一行*/
  assert( pCur->iPage>=0 && pCur->apPage[pCur->iPage] );
  assert( pCur->aiIdx[pCur->iPage]<pCur->apPage[pCur->iPage]->nCell );
  return accessPayload(pCur, offset, amt, (unsigned char*)pBuf, 0);/*
  返回当前游标所指记录的关键字。
  */
}

/*
** Read part of the data associated with cursor pCur.  Exactly
** "amt" bytes will be transfered into pBuf[].  The transfer
** begins at "offset".
** 读与游标pCur关联的数据域."amt"字节数将被传递到数组pBuf[]中.传递从"offset"开始.
** Return SQLITE_OK on success or an error code if anything goes
** wrong.  An error is returned if "offset+amt" is larger than
** the available payload.
** 成功则返回SQLITE_OK或如果任何错误发生则返回错误代码。如果 "offset+amt"比可用的有效载荷还大则返回一个错误。
*/
/*
返回当前游标所指记录的数据
*/
int sqlite3BtreeData(BtCursor *pCur, u32 offset, u32 amt, void *pBuf){        
  int rc;

#ifndef SQLITE_OMIT_INCRBLOB
  if ( pCur->eState==CURSOR_INVALID ){
    return SQLITE_ABORT;
  }
#endif

  assert( cursorHoldsMutex(pCur) );
  rc = restoreCursorPosition(pCur);
  if( rc==SQLITE_OK ){
    assert( pCur->eState==CURSOR_VALID );
    assert( pCur->iPage>=0 && pCur->apPage[pCur->iPage] );
    assert( pCur->aiIdx[pCur->iPage]<pCur->apPage[pCur->iPage]->nCell );
    rc = accessPayload(pCur, offset, amt, pBuf, 0);
  }
  return rc;
}

/*
** Return a pointer to payload information from the entry that the 
** pCur cursor is pointing to.  The pointer is to the beginning of
** the key if skipKey==0 and it points to the beginning of data if
** skipKey==1.  The number of bytes of available key/data is written
** into *pAmt.  If *pAmt==0, then the value returned will not be
** a valid pointer.
** 返回从pCur游标正在指向的条目到有效载荷的指针。如果skipKey==0 则该指针指向关键字的开
** 始，并且如果skipKey==1指向数据域的开始。可用关键字或数据域的字节数被写入到*pAmt.如
** 果* pAmt ==0,那么返回的值应是一个有效的指针。
** This routine is an optimization.  It is common for the entire key
** and data to fit on the local page and for there to be no overflow
** pages.  When that is so, this routine can be used to access the
** key and data without making a copy.  If the key and/or data spills
** onto overflow pages, then accessPayload() must be used to reassemble
** the key/data and copy it into a preallocated buffer.
** 此函数是一个优化。对于全部关键字与数据域适应本地页面是常见的,并且对于没有溢出页面的也是常见的。
** 当如此,这个函数可以用作访问没有副本的关键字和数据域。如果键和/或数据在溢出页面上溢出,
** 那么accessPayload()必须用于重组键/数据并将其复制到一个预先分配的缓冲区。
** The pointer returned by this routine looks directly into the cached
** page of the database.  The data might change or move the next time
** any btree routine is called.
** 指针通过这个函数直接查看数据库中的缓存页返回。下次任何btree函数被调用时数据可能会改变或移动。
*/
	/*
	返回一个指向有效载荷的指针。如果skipKey== 0，指针指向key的开始。如果skipKey==1，
	指针指向data的开始。如果* PAMT== 0，则返回的值将为无效指针。此程序是一个优化。
	当任何B树程序被调用，该数据可能会改变或移动。
	*/
static const unsigned char *fetchPayload(       //返回从pCur游标正在指向的条目到有效载荷的指针
  BtCursor *pCur,      /* Cursor pointing to entry to read from */      //指向要读取条目的游标
  int *pAmt,           /* Write the number of available bytes here */   //写可用字节数
  int skipKey          /* read beginning at data if this is true */     //逻辑值为真从数据开始读
){
  unsigned char *aPayload;
  MemPage *pPage;
  u32 nKey;
  u32 nLocal;

  assert( pCur!=0 && pCur->iPage>=0 && pCur->apPage[pCur->iPage]);
  assert( pCur->eState==CURSOR_VALID );
  assert( cursorHoldsMutex(pCur) );
  pPage = pCur->apPage[pCur->iPage];
  assert( pCur->aiIdx[pCur->iPage]<pPage->nCell );
  if( NEVER(pCur->info.nSize==0) ){
    btreeParseCell(pCur->apPage[pCur->iPage], pCur->aiIdx[pCur->iPage],
                   &pCur->info);
  }
  aPayload = pCur->info.pCell;
  aPayload += pCur->info.nHeader;
  if( pPage->intKey ){
    nKey = 0;
  }else{
    nKey = (int)pCur->info.nKey;
  }
  if( skipKey ){
    aPayload += nKey;
    nLocal = pCur->info.nLocal - nKey;
  }else{
    nLocal = pCur->info.nLocal;
    assert( nLocal<=nKey );
  }
  *pAmt = nLocal;
  return aPayload;
}

/*
** For the entry that cursor pCur is point to, return as
** many bytes of the key or data as are available on the local
** b-tree page.  Write the number of available bytes into *pAmt.
** 
** The pointer returned is ephemeral.  The key/data may move
** or be destroyed on the next call to any Btree routine,
** including calls from other threads against the same cache.
** Hence, a mutex on the BtShared should be held prior to calling
** this routine.
** 
** These routines is used to get quick access to key and data
** in the common case where no overflow pages are used.
*/
	/*
	对于游标pCur指向条目，返回key 或者 data的几个字节。写可用的字节数到*pAmt。
	返回的指针是短暂的。在下一次调用任何B树函数的时候，key/data可能移动或被销毁，
	包括其他线程对相同缓存的调用。因此BtShared上的一个互斥锁应该在调用这个函数
	前被持有这个程序在没有溢出页使用的常见情况下，用于快速访问key 和 data。
	*/

const void *sqlite3BtreeKeyFetch(BtCursor *pCur, int *pAmt){  //用于快速访问key
  const void *p = 0;
  assert( sqlite3_mutex_held(pCur->pBtree->db->mutex) );
  assert( cursorHoldsMutex(pCur) );
  if( ALWAYS(pCur->eState==CURSOR_VALID) ){
    p = (const void*)fetchPayload(pCur, pAmt, 0);
  }
  return p;
}

const void *sqlite3BtreeDataFetch(BtCursor *pCur, int *pAmt){  //用于快速访问data
  const void *p = 0;
  assert( sqlite3_mutex_held(pCur->pBtree->db->mutex) );
  assert( cursorHoldsMutex(pCur) );
  if( ALWAYS(pCur->eState==CURSOR_VALID) ){
    p = (const void*)fetchPayload(pCur, pAmt, 1);
  }
  return p;
}

/*
** Move the cursor down to a new child page.  The newPgno argument is the
** page number of the child page to move to.
** 移动游标到下一个新的孩子页面。newPgno参数是所以到的孩子页面的页码。
** This function returns SQLITE_CORRUPT if the page-header flags field of
** the new child page does not match the flags field of the parent (i.e.
** if an intkey page appears to be the parent of a non-intkey page, or
** vice-versa).
** 如果新的孩子页面的页头标志域和父节点的标志域不匹配，则函数返回SQLITE_CORRUPT.
** (例如一个内部关键字页是非内部关键字页的父节点页)
*/
	/*
	下移光标到一个新的子页面。该newPgno参数是子页面移动的页号。
	如果page-header标志与其父节点的标志不匹配，此函数返回SQLITE_CORRUPT。
	*/

static int moveToChild(BtCursor *pCur, u32 newPgno){      //移动游标到下一个新的孩子页面
  int rc;
  int i = pCur->iPage;
  MemPage *pNewPage;
  BtShared *pBt = pCur->pBt;

  assert( cursorHoldsMutex(pCur) );
  assert( pCur->eState==CURSOR_VALID );
  assert( pCur->iPage<BTCURSOR_MAX_DEPTH );
  if( pCur->iPage>=(BTCURSOR_MAX_DEPTH-1) ){
    return SQLITE_CORRUPT_BKPT;
  }
  rc = getAndInitPage(pBt, newPgno, &pNewPage);
  if( rc ) return rc;
  pCur->apPage[i+1] = pNewPage;
  pCur->aiIdx[i+1] = 0;
  pCur->iPage++;

  pCur->info.nSize = 0;
  pCur->validNKey = 0;
  if( pNewPage->nCell<1 || pNewPage->intKey!=pCur->apPage[i]->intKey ){
    return SQLITE_CORRUPT_BKPT;
  }
  return SQLITE_OK;
}

#if 0
/*
** Page pParent is an internal (non-leaf) tree page. This function 
** asserts that page number iChild is the left-child if the iIdx'th
** cell in page pParent. Or, if iIdx is equal to the total number of
** cells in pParent, that page number iChild is the right-child of
** the page.
** 页面pParent是B树内部(非叶)页。这个函数断言如果第iIdx单元在页pParent中则页码iChild
** 是左孩子。或,如果iIdx等于pParent中单元的总数,那么页码iChild是页面的右孩子。
*/
static void assertParentIndex(MemPage *pParent, int iIdx, Pgno iChild){  //判断页pParent的孩子页面是左孩子还是右孩子
  assert( iIdx<=pParent->nCell );
  if( iIdx==pParent->nCell ){
    assert( get4byte(&pParent->aData[pParent->hdrOffset+8])==iChild );
  }else{
    assert( get4byte(findCell(pParent, iIdx))==iChild );
  }
}
#else
#  define assertParentIndex(x,y,z) 
#endif

/*
** Move the cursor up to the parent page.
** 向上移动游标到父节点页面。
** pCur->idx is set to the cell index that contains the pointer
** to the page we are coming from.  If we are coming from the
** right-most child page then pCur->idx is set to one more than
** the largest cell index.
*/
	/*
	将游标移动到父页面.pCur-> IDX被设定为包含指向页的指针的单元索引.如果为最右边的子页面，pCur-> IDX被设置为比最大的单元更大的索引。
	*/
static void moveToParent(BtCursor *pCur){     //向上移动游标到父节点页面。
  assert( cursorHoldsMutex(pCur) );
  assert( pCur->eState==CURSOR_VALID );
  assert( pCur->iPage>0 );
  assert( pCur->apPage[pCur->iPage] );

  /* UPDATE: It is actually possible for the condition tested by the assert
  ** below to be untrue if the database file is corrupt. This can occur if
  ** one cursor has modified page pParent while a reference to it is held 
  ** by a second cursor. Which can only happen if a single page is linked
  ** into more than one b-tree structure in a corrupt database.  
  ** 如果数据库文件中断的话，由assert测试的条件很有可能为假。如果当一个参数
  ** 被另一个游标持有时一个游标已经修改了页面pParent ，那么这可能会发生。
  ** 如果一个页面被链接到不止一个b -树结构在不良的数据库中则以上也会出现。*/
#if 0
  assertParentIndex(
    pCur->apPage[pCur->iPage-1], 
    pCur->aiIdx[pCur->iPage-1], 
    pCur->apPage[pCur->iPage]->pgno
  );
#endif
  testcase( pCur->aiIdx[pCur->iPage-1] > pCur->apPage[pCur->iPage-1]->nCell );

  releasePage(pCur->apPage[pCur->iPage]);
  pCur->iPage--;
  pCur->info.nSize = 0;
  pCur->validNKey = 0;
}

/*
** Move the cursor to point to the root page of its b-tree structure.
** 移动游标指向B树结构的根页。
** If the table has a virtual root page, then the cursor is moved to point
** to the virtual root page instead of the actual root page. A table has a
** virtual root page when the actual root page contains no cells and a 
** single child page. This can only happen with the table rooted at page 1.
** 如果表有一个虚拟根页面,则游标移动指向虚拟根页面而不是实际的根页。当实际根页
** 不包含单元和单一的子页面时表上会有一个虚拟根页面。这只能发生在第1页的表上。
** If the b-tree structure is empty, the cursor state is set to 
** CURSOR_INVALID. Otherwise, the cursor is set to point to the first
** cell located on the root (or virtual root) page and the cursor state
** is set to CURSOR_VALID.
** 如果b-树结构为空,将游标状态设为CURSOR_INVALID。否则,游标将指向位于根(或虚拟根)
** 页面的第一个单元,并且游标状态设为CURSOR_VALID。
** If this function returns successfully, it may be assumed that the
** page-header flags indicate that the [virtual] root-page is the expected 
** kind of b-tree page (i.e. if when opening the cursor the caller did not
** specify a KeyInfo structure the flags byte is set to 0x05 or 0x0D,
** indicating a table b-tree, or if the caller did specify a KeyInfo 
** structure the flags byte is set to 0x02 or 0x0A, indicating an index
** b-tree).
** 如果这个函数返回成功,它可能会假定头部的标志表明[虚拟]根页是b-树页面(即如果
** 开放游标调用者没有指定KeyInfo结构，标记字节设置为0 x05或0 x0d,说明是表b-树
** ,或者如果调用者指定KeyInfo结构标记字节设置为0x02或0x0a,说明是索引b-tree)。
*/
static int moveToRoot(BtCursor *pCur){      //移动游标指向B树结构的根页
  MemPage *pRoot;
  int rc = SQLITE_OK;
  Btree *p = pCur->pBtree;
  BtShared *pBt = p->pBt;

  assert( cursorHoldsMutex(pCur) );
  assert( CURSOR_INVALID < CURSOR_REQUIRESEEK );
  assert( CURSOR_VALID   < CURSOR_REQUIRESEEK );
  assert( CURSOR_FAULT   > CURSOR_REQUIRESEEK );
  if( pCur->eState>=CURSOR_REQUIRESEEK ){
    if( pCur->eState==CURSOR_FAULT ){
      assert( pCur->skipNext!=SQLITE_OK );
      return pCur->skipNext;
    }
    sqlite3BtreeClearCursor(pCur);
  }

  if( pCur->iPage>=0 ){
    int i;
    for(i=1; i<=pCur->iPage; i++){
      releasePage(pCur->apPage[i]);
    }
    pCur->iPage = 0;
  }else if( pCur->pgnoRoot==0 ){
    pCur->eState = CURSOR_INVALID;
    return SQLITE_OK;
  }else{
    rc = getAndInitPage(pBt, pCur->pgnoRoot, &pCur->apPage[0]);
    if( rc!=SQLITE_OK ){
      pCur->eState = CURSOR_INVALID;
      return rc;
    }
    pCur->iPage = 0;

    /* If pCur->pKeyInfo is not NULL, then the caller that opened this cursor
    ** expected to open it on an index b-tree. Otherwise, if pKeyInfo is
    ** NULL, the caller expects a table b-tree. If this is not the case,
    ** return an SQLITE_CORRUPT error.  
	** 如果pCur->pKeyInfo非空,那么调用者打开将在索引B树上打开的游标.否则,如果pKeyInfo为空,
	** 调用者需要表B树。除此之外，返回一个SQLITE_CORRUPT错误。*/
    assert( pCur->apPage[0]->intKey==1 || pCur->apPage[0]->intKey==0 );
    if( (pCur->pKeyInfo==0)!=pCur->apPage[0]->intKey ){
      return SQLITE_CORRUPT_BKPT;
    }
  }

  /* Assert that the root page is of the correct type. This must be the
  ** case as the call to this function that loaded the root-page (either
  ** this call or a previous invocation) would have detected corruption 
  ** if the assumption were not true, and it is not possible for the flags 
  ** byte to have been modified while this cursor is holding a reference
  ** to the page.  
  ** 判断根页有正确的类型。调用此函数加载的根页时一定是这样的（或者这个调用或先前调用）如果假设是不正确的，
  ** 该函数将检测到损坏。并且对于标志字节来说，当游标正在持有页的一个参数时被修改是不可能的。*/
  pRoot = pCur->apPage[0];
  assert( pRoot->pgno==pCur->pgnoRoot );
  assert( pRoot->isInit && (pCur->pKeyInfo==0)==pRoot->intKey );

  pCur->aiIdx[0] = 0;
  pCur->info.nSize = 0;
  pCur->atLast = 0;
  pCur->validNKey = 0;

  if( pRoot->nCell==0 && !pRoot->leaf ){
    Pgno subpage;
    if( pRoot->pgno!=1 ) return SQLITE_CORRUPT_BKPT;
    subpage = get4byte(&pRoot->aData[pRoot->hdrOffset+8]);
    pCur->eState = CURSOR_VALID;
    rc = moveToChild(pCur, subpage);
  }else{
    pCur->eState = ((pRoot->nCell>0)?CURSOR_VALID:CURSOR_INVALID);
  }
  return rc;
}

/*
** Move the cursor down to the left-most leaf entry beneath the
** entry to which it is currently pointing.
** 移动游标到最左叶子条目，游标当前正指向该条目。
** The left-most leaf is the one with the smallest key - the first
** in ascending order.
** 最左的叶子节点拥有最小的键值，几递增有序的第一个关键字。
*/
static int moveToLeftmost(BtCursor *pCur){     //移动游标到最左叶子
  Pgno pgno;
  int rc = SQLITE_OK;
  MemPage *pPage;

  assert( cursorHoldsMutex(pCur) );
  assert( pCur->eState==CURSOR_VALID );
  while( rc==SQLITE_OK && !(pPage = pCur->apPage[pCur->iPage])->leaf ){
    assert( pCur->aiIdx[pCur->iPage]<pPage->nCell );
    pgno = get4byte(findCell(pPage, pCur->aiIdx[pCur->iPage]));
    rc = moveToChild(pCur, pgno);
  }
  return rc;
}

/*
** Move the cursor down to the right-most leaf entry beneath the
** page to which it is currently pointing.  Notice the difference
** between moveToLeftmost() and moveToRightmost().  moveToLeftmost()
** finds the left-most entry beneath the *entry* whereas moveToRightmost()
** finds the right-most entry beneath the *page*.
** 移动游标到最右的叶子节点。注意moveToLeftmost()与moveToRightmost()的不同。moveToLeftmost()
**  是找到最左边*entry*下的条目，而moveToRightmost()是找到最右边*page*下的条目。
** The right-most entry is the one with the largest key - the last
** key in ascending order.
** 最右边的条目是有序递增序列的最大的键值。
*/
static int moveToRightmost(BtCursor *pCur){   //移动游标到最右的叶子节点
  Pgno pgno;
  int rc = SQLITE_OK;
  MemPage *pPage = 0;

  assert( cursorHoldsMutex(pCur) );
  assert( pCur->eState==CURSOR_VALID );
  while( rc==SQLITE_OK && !(pPage = pCur->apPage[pCur->iPage])->leaf ){
    pgno = get4byte(&pPage->aData[pPage->hdrOffset+8]);
    pCur->aiIdx[pCur->iPage] = pPage->nCell;
    rc = moveToChild(pCur, pgno);
  }
  if( rc==SQLITE_OK ){
    pCur->aiIdx[pCur->iPage] = pPage->nCell-1;
    pCur->info.nSize = 0;
    pCur->validNKey = 0;
  }
  return rc;
}

/* Move the cursor to the first entry in the table.  Return SQLITE_OK
** on success.  Set *pRes to 0 if the cursor actually points to something
** or set *pRes to 1 if the table is empty.
** 将游标移动到表中的第一个元素。若成功,返回SQLITE_OK。
** 如果表在指向一些地方，设置*pRes为0或如果表是空的设置*pRes为1。
*/
int sqlite3BtreeFirst(BtCursor *pCur, int *pRes){   //将游标移动到表中的第一个条目
  int rc;

  assert( cursorHoldsMutex(pCur) );
  assert( sqlite3_mutex_held(pCur->pBtree->db->mutex) );
  rc = moveToRoot(pCur);
  if( rc==SQLITE_OK ){
    if( pCur->eState==CURSOR_INVALID ){
      assert( pCur->pgnoRoot==0 || pCur->apPage[pCur->iPage]->nCell==0 );  //表为空
      *pRes = 1;
    }else{
      assert( pCur->apPage[pCur->iPage]->nCell>0 );    //游标正指向某个地方
      *pRes = 0;
      rc = moveToLeftmost(pCur);
    }
  }
  return rc;
}

/* Move the cursor to the last entry in the table.  Return SQLITE_OK
** on success.  Set *pRes to 0 if the cursor actually points to something
** or set *pRes to 1 if the table is empty.
** 将游标移动到表中的最后一个条目。成功则返回SQLITE_OK。若游标正指向某处设*pRes为0或表为空设置*pRes为1。
*/
int sqlite3BtreeLast(BtCursor *pCur, int *pRes){     //将游标移动到表中的最后一个条目
  int rc;
 
  assert( cursorHoldsMutex(pCur) );
  assert( sqlite3_mutex_held(pCur->pBtree->db->mutex) );

  /* If the cursor already points to the last entry, this is a no-op. */   //若游标已经指向最后一个条目，则无操作
  if( CURSOR_VALID==pCur->eState && pCur->atLast ){
#ifdef SQLITE_DEBUG
    /* This block serves to assert() that the cursor really does point 
    ** to the last entry in the b-tree. */    //该块代码是由来判断游标已经指向了B树中的最后的条目。
    int ii;
    for(ii=0; ii<pCur->iPage; ii++){
      assert( pCur->aiIdx[ii]==pCur->apPage[ii]->nCell );
    }
    assert( pCur->aiIdx[pCur->iPage]==pCur->apPage[pCur->iPage]->nCell-1 );
    assert( pCur->apPage[pCur->iPage]->leaf );
#endif
    return SQLITE_OK;
  }

  rc = moveToRoot(pCur);   //返回B树根页
  if( rc==SQLITE_OK ){
    if( CURSOR_INVALID==pCur->eState ){
      assert( pCur->pgnoRoot==0 || pCur->apPage[pCur->iPage]->nCell==0 );
      *pRes = 1;
    }else{
      assert( pCur->eState==CURSOR_VALID );
      *pRes = 0;
      rc = moveToRightmost(pCur);
      pCur->atLast = rc==SQLITE_OK ?1:0;
    }
  }
  return rc;  //rc就是一个状态量，返回的是1或0.
}

/* Move the cursor so that it points to an entry near the key 
** specified by pIdxKey or intKey.   Return a success code.
** 移动游标以便于指向被pIdxKey 或intKey指向的条目附近的关键字。
** For INTKEY tables, the intKey parameter is used.  pIdxKey 
** must be NULL.  For index tables, pIdxKey is used and intKey
** is ignored.
** 对于INTKEY表，用的是参数intKey，pIdxKey必须是空。对于索引表，使用pIdxKey，intKey忽略不用。
** If an exact match is not found, then the cursor is always
** left pointing at a leaf page which would hold the entry if it
** were present.  The cursor might point to an entry that comes
** before or after the key.
** 如果没有找到准确的匹配,则游标总是向左指着叶子页面，如果页面是父节点则将保存条目。游标可能
** 指向一个关键字之前或之后的条目。
** An integer is written into *pRes which is the result of
** comparing the key with the entry to which the cursor is 
** pointing.  The meaning of the integer written into
** *pRes is as follows:
** 一个整数写入*pRes，其结果将比较带有条目的关键字和游标指向的对象。整数写入的意义如下:
**     *pRes<0      The cursor is left pointing at an entry tha                   // 游标离开指向一个小于intKey / pIdxKey的条目
**                  is smaller than intKey/pIdxKey or if the table is empty       //或如果表是空的游标不指向任何地方。
**                  and the cursor is therefore left point to nothing.
**    
**     *pRes==0     The cursor is left pointing at an entry that                  //游标指向了一个intKey/pIdxKey相对应的条目。
**                  exactly matches intKey/pIdxKey.
**    
**     *pRes>0      The cursor is left pointing at an entry that                  //游标指向比intKey/pIdxKey更大的条目
**                  is larger than intKey/pIdxKey.
**
*/
int sqlite3BtreeMovetoUnpacked(              //游标指向一个intKey/pIdxKey相对应的条目
  BtCursor *pCur,          /* The cursor to be moved */                           //该游标将发生移动
  UnpackedRecord *pIdxKey, /* Unpacked index key */                               //解压的索引关键字
  i64 intKey,              /* The table key */                                    //表关键字
  int biasRight,           /* If true, bias the search to the high end */         //该变量为真，偏移到最后
  int *pRes                /* Write search results here */                        //将查找结果写入该变量
){
  int rc;

  assert( cursorHoldsMutex(pCur) );
  assert( sqlite3_mutex_held(pCur->pBtree->db->mutex) );
  assert( pRes );
  assert( (pIdxKey==0)==(pCur->pKeyInfo==0) );

  /* If the cursor is already positioned at the point we are trying
  ** to move to, then just return without doing any work 
  ** 如果游标已经在要移到的点，则返回不作操作*/  
  if( pCur->eState==CURSOR_VALID && pCur->validNKey 
   && pCur->apPage[0]->intKey 
  ){
    if( pCur->info.nKey==intKey ){
      *pRes = 0;
      return SQLITE_OK;
    }
    if( pCur->atLast && pCur->info.nKey<intKey ){
      *pRes = -1;
      return SQLITE_OK;
    }
  }
  rc = moveToRoot(pCur);   //指向B树根页
  if( rc ){
    return rc;
  }
  assert( pCur->pgnoRoot==0 || pCur->apPage[pCur->iPage] );
  assert( pCur->pgnoRoot==0 || pCur->apPage[pCur->iPage]->isInit );
  assert( pCur->eState==CURSOR_INVALID || pCur->apPage[pCur->iPage]->nCell>0 );
  if( pCur->eState==CURSOR_INVALID ){
    *pRes = -1;
    assert( pCur->pgnoRoot==0 || pCur->apPage[pCur->iPage]->nCell==0 );
    return SQLITE_OK;
  }
  assert( pCur->apPage[0]->intKey || pIdxKey );
  for(;;){
    int lwr, upr, idx;
    Pgno chldPg;
    MemPage *pPage = pCur->apPage[pCur->iPage];
    int c;
    /* pPage->nCell must be greater than zero. If this is the root-page
    ** the cursor would have been INVALID above and this for(;;) loop
    ** not run. If this is not the root-page, then the moveToChild() routine
    ** would have already detected db corruption. Similarly, pPage must
    ** be the right kind (index or table) of b-tree page. Otherwise
    ** a moveToChild() or moveToRoot() call would have detected corruption.  
	** pPage->nCell必须比0大。如果它是根页面上面的游标无效并且for(;;) 循环不会执行。
	** 如果不是跟根页，那么 moveToChild()函数将检测db崩溃。同样， pPage必须是正确种类的B树
	** 否则，moveToChild() 或 moveToRoot() 的调用将发现崩溃。*/
    assert( pPage->nCell>0 );
    assert( pPage->intKey==(pIdxKey==0) );
    lwr = 0;
    upr = pPage->nCell-1;
    if( biasRight ){
      pCur->aiIdx[pCur->iPage] = (u16)(idx = upr);
    }else{
      pCur->aiIdx[pCur->iPage] = (u16)(idx = (upr+lwr)/2);             //二分查找
    }
    for(;;){
      u8 *pCell;          /* Pointer to current cell in pPage */       // 指向pPage的当前单元
      assert( idx==pCur->aiIdx[pCur->iPage] );
      pCur->info.nSize = 0;
      pCell = findCell(pPage, idx) + pPage->childPtrSize;
      if( pPage->intKey ){
        i64 nCellKey;
        if( pPage->hasData ){
          u32 dummy;
          pCell += getVarint32(pCell, dummy);
        }
        getVarint(pCell, (u64*)&nCellKey);
        if( nCellKey==intKey ){
          c = 0;
        }else if( nCellKey<intKey ){
          c = -1;
        }else{
          assert( nCellKey>intKey );
          c = +1;
        }
        pCur->validNKey = 1;
        pCur->info.nKey = nCellKey;
      }else{
        /* The maximum supported page-size is 65536 bytes. This means that
        ** the maximum number of record bytes stored on an index B-Tree
        ** page is less than 16384 bytes and may be stored as a 2-byte
        ** varint. This information is used to attempt to avoid parsing 
        ** the entire cell by checking for the cases where the record is 
        ** stored entirely within the b-tree page by inspecting the first 
        ** 2 bytes of the cell.
		** 支持的最大页面大小为65536字节。这意味着储在索引B树页中的记录的最大数字节小于16384字节,可以存储为一个2字节的变量。
		** 此信息用于试图通过检查避免解析整个单元，对于该情况，记录完全存储在B树页通过检查该单元的开始的两个字节。
        */
        int nCell = pCell[0];
        if( nCell<=pPage->max1bytePayload
         /* && (pCell+nCell)<pPage->aDataEnd */
        ){
          /* This branch runs if the record-size field of the cell is a
          ** single byte varint and the record fits entirely on the main
          ** b-tree page.  
		  **如果单元的记录域是一个单字节的变量并且记录完全存储在主B树页上，执行该分支。*/
          testcase( pCell+nCell+1==pPage->aDataEnd );
          c = sqlite3VdbeRecordCompare(nCell, (void*)&pCell[1], pIdxKey);
        }else if( !(pCell[1] & 0x80) 
          && (nCell = ((nCell&0x7f)<<7) + pCell[1])<=pPage->maxLocal
          /* && (pCell+nCell+2)<=pPage->aDataEnd */
        ){
          /* The record-size field is a 2 byte varint and the record  
          ** fits entirely on the main b-tree page. 
		  ** 变量的记录到小域是一个两字节的变量并且记录完全存储在主B树页上。*/
          testcase( pCell+nCell+2==pPage->aDataEnd );
          c = sqlite3VdbeRecordCompare(nCell, (void*)&pCell[2], pIdxKey);
        }else{
          /* The record flows over onto one or more overflow pages. In
          ** this case the whole cell needs to be parsed, a buffer allocated
          ** and accessPayload() used to retrieve the record into the
          ** buffer before VdbeRecordCompare() can be called. 
		  ** 记录溢出是存储在一个或多个溢出页上。在这种情况下整个单元需要解析,分配一个缓冲区和accessPayload()
		  ** 用于检索在VdbeRecordCompare()可以被调用之前进入到缓冲区的记录。*/
          void *pCellKey;
          u8 * const pCellBody = pCell - pPage->childPtrSize;
          btreeParseCellPtr(pPage, pCellBody, &pCur->info);
          nCell = (int)pCur->info.nKey;
          pCellKey = sqlite3Malloc( nCell );
          if( pCellKey==0 ){
            rc = SQLITE_NOMEM;
            goto moveto_finish;
          }
          rc = accessPayload(pCur, 0, nCell, (unsigned char*)pCellKey, 0);
          if( rc ){
            sqlite3_free(pCellKey);
            goto moveto_finish;
          }
          c = sqlite3VdbeRecordCompare(nCell, pCellKey, pIdxKey);
          sqlite3_free(pCellKey);
        }
      }
      if( c==0 ){
        if( pPage->intKey && !pPage->leaf ){
          lwr = idx;
          break;
        }else{
          *pRes = 0;
          rc = SQLITE_OK;
          goto moveto_finish;
        }
      }
      if( c<0 ){
        lwr = idx+1;
      }else{
        upr = idx-1;
      }
      if( lwr>upr ){
        break;
      }
      pCur->aiIdx[pCur->iPage] = (u16)(idx = (lwr+upr)/2);
    }
    assert( lwr==upr+1 || (pPage->intKey && !pPage->leaf) );
    assert( pPage->isInit );
    if( pPage->leaf ){
      chldPg = 0;
    }else if( lwr>=pPage->nCell ){
      chldPg = get4byte(&pPage->aData[pPage->hdrOffset+8]);
    }else{
      chldPg = get4byte(findCell(pPage, lwr));
    }
    if( chldPg==0 ){
      assert( pCur->aiIdx[pCur->iPage]<pCur->apPage[pCur->iPage]->nCell );
      *pRes = c;
      rc = SQLITE_OK;
      goto moveto_finish;
    }
    pCur->aiIdx[pCur->iPage] = (u16)lwr;
    pCur->info.nSize = 0;
    pCur->validNKey = 0;
    rc = moveToChild(pCur, chldPg);
    if( rc ) goto moveto_finish;
  }
moveto_finish:
  return rc;
}


/*
** Return TRUE if the cursor is not pointing at an entry of the table.
** 如果游标没有指向表的一个条目返回true。
** TRUE will be returned after a call to sqlite3BtreeNext() moves
** past the last entry in the table or sqlite3BtreePrev() moves past
** the first entry.  TRUE is also returned if the table is empty.
** 调用sqlite3BtreeNext()后移动到标的而最后一个条目或调用sqlite3BtreePrev()移动到第一个条目则返回True。如果表是空的也返回true。
*/
int sqlite3BtreeEof(BtCursor *pCur){
  /* TODO: What if the cursor is in CURSOR_REQUIRESEEK but all table entries
  ** have been deleted? This API will need to change to return an error code
  ** as well as the boolean result value.
  ** 假使游标在CURSOR_REQUIRESEEK但所有表项都被删除那会怎么样?这个API将需要更改返回一个错误代码以及布尔值。
  */
  return (CURSOR_VALID!=pCur->eState);
}

/*
** Advance the cursor to the next entry in the database.  If
** successful then set *pRes=0.  If the cursor
** was already pointing to the last entry in the database before
** this routine was called, then set *pRes=1.
** 移动游标到数据库中的下一条目，如果成功设置*PRes=0。如果在调用这个函数时游标已经指向了最后一条目则设定*pRes=1。
*/
int sqlite3BtreeNext(BtCursor *pCur, int *pRes){   //移动游标到数据库中的下一条目
  int rc;
  int idx;
  MemPage *pPage;

  assert( cursorHoldsMutex(pCur) );
  rc = restoreCursorPosition(pCur);  
  if( rc!=SQLITE_OK ){
    return rc;
  }
  assert( pRes!=0 );
  if( CURSOR_INVALID==pCur->eState ){
    *pRes = 1;
    return SQLITE_OK;
  }
  if( pCur->skipNext>0 ){
    pCur->skipNext = 0;
    *pRes = 0;
    return SQLITE_OK;
  }
  pCur->skipNext = 0;

  pPage = pCur->apPage[pCur->iPage];
  idx = ++pCur->aiIdx[pCur->iPage];
  assert( pPage->isInit );

  /* If the database file is corrupt, it is possible for the value of idx 
  ** to be invalid here. This can only occur if a second cursor modifies
  ** the page while cursor pCur is holding a reference to it. Which can
  ** only happen if the database is corrupt in such a way as to link the
  ** page into more than one b-tree structure. 
  ** 如果数据库文件是损坏,idx的价值可能无效的。当游标pCur持有一个参数时如果第二个游标修改页面,
  ** 可能会出现文件损害。当连接页到多个B树结构时如果数据库以这样的方式崩溃那么这种情况会发生。*/
  testcase( idx>pPage->nCell );

  pCur->info.nSize = 0;
  pCur->validNKey = 0;
  if( idx>=pPage->nCell ){
    if( !pPage->leaf ){
      rc = moveToChild(pCur, get4byte(&pPage->aData[pPage->hdrOffset+8]));
      if( rc ) return rc;
      rc = moveToLeftmost(pCur);
      *pRes = 0;
      return rc;
    }
    do{
      if( pCur->iPage==0 ){
        *pRes = 1;
        pCur->eState = CURSOR_INVALID;
        return SQLITE_OK;
      }
      moveToParent(pCur);
      pPage = pCur->apPage[pCur->iPage];
    }while( pCur->aiIdx[pCur->iPage]>=pPage->nCell );
    *pRes = 0;
    if( pPage->intKey ){
      rc = sqlite3BtreeNext(pCur, pRes);
    }else{
      rc = SQLITE_OK;
    }
    return rc;
  }
  *pRes = 0;
  if( pPage->leaf ){
    return SQLITE_OK;
  }
  rc = moveToLeftmost(pCur);
  return rc;
}


/*
** Step the cursor to the back to the previous entry in the database.  If
** successful then set *pRes=0.  If the cursor
** was already pointing to the first entry in the database before
** this routine was called, then set *pRes=1.
** 逐步使游标回到数据库中以前的条目。成功，返回*pRes=0。
** 若函数被调用之前已经移到了第一个条目， *pRes=1
*/
/*寻找数据库中以前的条目*/
int sqlite3BtreePrevious(BtCursor *pCur, int *pRes){   //逐步使游标回到数据库中以前的条目
  int rc;
  MemPage *pPage;

  assert( cursorHoldsMutex(pCur) );
  rc = restoreCursorPosition(pCur);
  if( rc!=SQLITE_OK ){
    return rc;
  }
  pCur->atLast = 0;
  if( CURSOR_INVALID==pCur->eState ){
    *pRes = 1;
    return SQLITE_OK;
  }
  if( pCur->skipNext<0 ){
    pCur->skipNext = 0;
    *pRes = 0;
    return SQLITE_OK;
  }
  pCur->skipNext = 0;

  pPage = pCur->apPage[pCur->iPage];
  assert( pPage->isInit );
  if( !pPage->leaf ){
    int idx = pCur->aiIdx[pCur->iPage];
    rc = moveToChild(pCur, get4byte(findCell(pPage, idx)));
    if( rc ){
      return rc;
    }
    rc = moveToRightmost(pCur);
  }else{
    while( pCur->aiIdx[pCur->iPage]==0 ){
      if( pCur->iPage==0 ){
        pCur->eState = CURSOR_INVALID;
        *pRes = 1;
        return SQLITE_OK;
      }
      moveToParent(pCur);
    }
    pCur->info.nSize = 0;
    pCur->validNKey = 0;

    pCur->aiIdx[pCur->iPage]--;
    pPage = pCur->apPage[pCur->iPage];
    if( pPage->intKey && !pPage->leaf ){
      rc = sqlite3BtreePrevious(pCur, pRes);
    }else{
      rc = SQLITE_OK;
    }
  }
  *pRes = 0;
  return rc;
}

/*
** Allocate a new page from the database file.
** 从数据库文件分配一个新页面。
** The new page is marked as dirty.  (In other words, sqlite3PagerWrite()
** has already been called on the new page.)  The new page has also
** been referenced and the calling routine is responsible for calling
** sqlite3PagerUnref() on the new page when it is done.
** 新页被脏字标记，即是sqlite3PagerWrite()已经在新页上被调用。完成分配时，新的页也被引用
** 并且调用函数负责在新页上调用sqlite3PagerUnref().
** SQLITE_OK is returned on success.  Any other return value indicates
** an error.  *ppPage and *pPgno are undefined in the event of an error.
** Do not invoke sqlite3PagerUnref() on *ppPage if an error is returned.
** 成功则返回SQLITE_OK。其他任何返回值表示一个错误。在一个错误事件中* ppPage和* pPgno是未定义的。
** 如果返回一个错误则不在*ppPage上调用sqlite3PagerUnref()。
** If the "nearby" parameter is not 0, then a (feeble) effort is made to 
** locate a page close to the page number "nearby".  This can be used in an
** attempt to keep related pages close to each other in the database file,
** which in turn can make database access faster.
** 如果 "nearby"参数不是0,那么(微弱的)效果是定位接近的页面的页码"nearby"。这可以用于尝试使相关页面在数据库文件中保持接近,
** 反过来可以使数据库访问速度更快。
** If the "exact" parameter is not 0, and the page-number nearby exists 
** anywhere on the free-list, then it is guarenteed to be returned. This
** is only used by auto-vacuum databases when allocating a new table.
** 如果"exact"参数不是0,并且页码附近任何地方都存在在空闲列表,那么它保证了返回。这 只使用在auto-vacuum数据库分配一个新表时。
*/
static int allocateBtreePage(           //从数据库文件分配一个新页面，成功则返回SQLITE_OK
  BtShared *pBt, 
  MemPage **ppPage, 
  Pgno *pPgno, 
  Pgno nearby,
  u8 exact
){
  MemPage *pPage1;
  int rc;
  u32 n;     /* Number of pages on the freelist */                 //空闲列表上的页数
  u32 k;     /* Number of leaves on the trunk of the freelist */   //空闲列表主干的叶子数
  MemPage *pTrunk = 0;
  MemPage *pPrevTrunk = 0;
  Pgno mxPage;     /* Total size of the database file */           //数据库文件总的大小

  assert( sqlite3_mutex_held(pBt->mutex) );
  pPage1 = pBt->pPage1;
  mxPage = btreePagecount(pBt);
  n = get4byte(&pPage1->aData[36]);
  testcase( n==mxPage-1 );
  if( n>=mxPage ){
    return SQLITE_CORRUPT_BKPT;
  }
  if( n>0 ){
    /* There are pages on the freelist.  Reuse one of those pages. */        //空闲列表上有页，重新使用这些页
    Pgno iTrunk;
    u8 searchList = 0; /* If the free-list must be searched for 'nearby' */  //'nearby'可以搜索空闲列表 
    
    /* If the 'exact' parameter was true and a query of the pointer-map
    ** shows that the page 'nearby' is somewhere on the free-list, then
    ** the entire-list will be searched for that page.
	** 如果参数'exact'是true并且一个指针位图查询显示页'nearby'在空闲列表上的某处，那么对于该页整个列表可以被搜索。
    */
#ifndef SQLITE_OMIT_AUTOVACUUM
    if( exact && nearby<=mxPage ){
      u8 eType;
      assert( nearby>0 );
      assert( pBt->autoVacuum );
      rc = ptrmapGet(pBt, nearby, &eType, 0);
      if( rc ) return rc;
      if( eType==PTRMAP_FREEPAGE ){
        searchList = 1;
      }
      *pPgno = nearby;
    }
#endif
    /* Decrement the free-list count by 1. Set iTrunk to the index of the
    ** first free-list trunk page. iPrevTrunk is initially 1.
    ** 递减空闲列表数量到1.设定iTrunk到第一个空闲列表页的主页面索引.iPrevTrunk初始化为1.*/
    rc = sqlite3PagerWrite(pPage1->pDbPage);
    if( rc ) return rc;
    put4byte(&pPage1->aData[36], n-1);

    /* The code within this loop is run only once if the 'searchList' variable
    ** is not true. Otherwise, it runs once for each trunk-page on the
    ** free-list until the page 'nearby' is located.
    ** 如果变量'searchList'为假，则循环内的代码只运行一次。否则对于在空闲列表上的每个主页面都运行一次直到直到页面nearby*/
    do {
      pPrevTrunk = pTrunk;
      if( pPrevTrunk ){
        iTrunk = get4byte(&pPrevTrunk->aData[0]);
      }else{
        iTrunk = get4byte(&pPage1->aData[32]);
      }
      testcase( iTrunk==mxPage );
      if( iTrunk>mxPage ){
        rc = SQLITE_CORRUPT_BKPT;
      }else{
        rc = btreeGetPage(pBt, iTrunk, &pTrunk, 0);
      }
      if( rc ){
        pTrunk = 0;
        goto end_allocate_page;
      }
      assert( pTrunk!=0 );
      assert( pTrunk->aData!=0 );

      k = get4byte(&pTrunk->aData[4]); /* # of leaves on this trunk page */  //主页面上的叶子数
      if( k==0 && !searchList ){
        /* The trunk has no leaves and the list is not being searched. 
        ** So extract the trunk page itself and use it as the newly 
        ** allocated page 
		** 主页面上无叶子并且列表不用被搜索.并且提取主页面本身并用它作为新分配的页。
		*/
        assert( pPrevTrunk==0 );
        rc = sqlite3PagerWrite(pTrunk->pDbPage);
        if( rc ){
          goto end_allocate_page;
        }
        *pPgno = iTrunk;
        memcpy(&pPage1->aData[32], &pTrunk->aData[0], 4);
        *ppPage = pTrunk;
        pTrunk = 0;
        TRACE(("ALLOCATE: %d trunk - %d free pages left\n", *pPgno, n-1));  //跟踪分配了几个页面剩下几个空闲页面
      }else if( k>(u32)(pBt->usableSize/4 - 2) ){
        /* Value of k is out of range.  Database corruption */     //k值超过范围，数据库崩溃
        rc = SQLITE_CORRUPT_BKPT;
        goto end_allocate_page;
#ifndef SQLITE_OMIT_AUTOVACUUM
      }else if( searchList && nearby==iTrunk ){
        /* The list is being searched and this trunk page is the page
        ** to allocate, regardless of whether it has leaves.
		** 列表正被搜索并且这个主页面是分配的页，不管它有什么叶子
       */
        assert( *pPgno==iTrunk );
        *ppPage = pTrunk;
        searchList = 0;
        rc = sqlite3PagerWrite(pTrunk->pDbPage);
        if( rc ){
          goto end_allocate_page;
        }
        if( k==0 ){
          if( !pPrevTrunk ){
            memcpy(&pPage1->aData[32], &pTrunk->aData[0], 4);
          }else{
            rc = sqlite3PagerWrite(pPrevTrunk->pDbPage);
            if( rc!=SQLITE_OK ){
              goto end_allocate_page;
            }
            memcpy(&pPrevTrunk->aData[0], &pTrunk->aData[0], 4);
          }
        }else{
          /* The trunk page is required by the caller but it contains 
          ** pointers to free-list leaves. The first leaf becomes a trunk
          ** page in this case.
		  ** 主页面正在被调用函数需要但是它包含指向空闲列表页的指针。在这种情况下，第一个叶子变成主页面。
          */
          MemPage *pNewTrunk;
          Pgno iNewTrunk = get4byte(&pTrunk->aData[8]);
          if( iNewTrunk>mxPage ){ 
            rc = SQLITE_CORRUPT_BKPT;
            goto end_allocate_page;
          }
          testcase( iNewTrunk==mxPage );
          rc = btreeGetPage(pBt, iNewTrunk, &pNewTrunk, 0);
          if( rc!=SQLITE_OK ){
            goto end_allocate_page;
          }
          rc = sqlite3PagerWrite(pNewTrunk->pDbPage);
          if( rc!=SQLITE_OK ){
            releasePage(pNewTrunk);
            goto end_allocate_page;
          }
          memcpy(&pNewTrunk->aData[0], &pTrunk->aData[0], 4);
          put4byte(&pNewTrunk->aData[4], k-1);
          memcpy(&pNewTrunk->aData[8], &pTrunk->aData[12], (k-1)*4);
          releasePage(pNewTrunk);
          if( !pPrevTrunk ){
            assert( sqlite3PagerIswriteable(pPage1->pDbPage) );
            put4byte(&pPage1->aData[32], iNewTrunk);
          }else{
            rc = sqlite3PagerWrite(pPrevTrunk->pDbPage);
            if( rc ){
              goto end_allocate_page;
            }
            put4byte(&pPrevTrunk->aData[0], iNewTrunk);
          }
        }
        pTrunk = 0;
        TRACE(("ALLOCATE: %d trunk - %d free pages left\n", *pPgno, n-1));
#endif
      }else if( k>0 ){
        /* Extract a leaf from the trunk */  //从主页面提取出一个叶子
        u32 closest;
        Pgno iPage;
        unsigned char *aData = pTrunk->aData;
        if( nearby>0 ){
          u32 i;
          int dist;
          closest = 0;
          dist = sqlite3AbsInt32(get4byte(&aData[8]) - nearby);
          for(i=1; i<k; i++){
            int d2 = sqlite3AbsInt32(get4byte(&aData[8+i*4]) - nearby);
            if( d2<dist ){
              closest = i;
              dist = d2;
            }
          }
        }else{
          closest = 0;
        }

        iPage = get4byte(&aData[8+closest*4]);
        testcase( iPage==mxPage );
        if( iPage>mxPage ){
          rc = SQLITE_CORRUPT_BKPT;
          goto end_allocate_page;
        }
        testcase( iPage==mxPage );
        if( !searchList || iPage==nearby ){
          int noContent;
          *pPgno = iPage;
          TRACE(("ALLOCATE: %d was leaf %d of %d on trunk %d"
                 ": %d more free pages\n",
                 *pPgno, closest+1, k, pTrunk->pgno, n-1));
          rc = sqlite3PagerWrite(pTrunk->pDbPage);
          if( rc ) goto end_allocate_page;
          if( closest<k-1 ){
            memcpy(&aData[8+closest*4], &aData[4+k*4], 4);
          }
          put4byte(&aData[4], k-1);
          noContent = !btreeGetHasContent(pBt, *pPgno);
          rc = btreeGetPage(pBt, *pPgno, ppPage, noContent);
          if( rc==SQLITE_OK ){
            rc = sqlite3PagerWrite((*ppPage)->pDbPage);
            if( rc!=SQLITE_OK ){
              releasePage(*ppPage);
            }
          }
          searchList = 0;
        }
      }
      releasePage(pPrevTrunk);
      pPrevTrunk = 0;
    }while( searchList );
  }else{
    /* There are no pages on the freelist, so create a new page at the
    ** end of the file。
	** 在空闲列表上没有页面，因此在文件的末尾创建新页。
	*/
    rc = sqlite3PagerWrite(pBt->pPage1->pDbPage);
    if( rc ) return rc;
    pBt->nPage++;
    if( pBt->nPage==PENDING_BYTE_PAGE(pBt) ) pBt->nPage++;

#ifndef SQLITE_OMIT_AUTOVACUUM
    if( pBt->autoVacuum && PTRMAP_ISPAGE(pBt, pBt->nPage) ){
      /* If *pPgno refers to a pointer-map page, allocate two new pages
      ** at the end of the file instead of one. The first allocated page
      ** becomes a new pointer-map page, the second is used by the caller.
	  ** 如果*pPgno值的是指针位图页，在文件末尾非配两个新页来替换它。第一个变成指针位图页，第二个用来调用。
      */
      MemPage *pPg = 0;
      TRACE(("ALLOCATE: %d from end of file (pointer-map page)\n", pBt->nPage));
      assert( pBt->nPage!=PENDING_BYTE_PAGE(pBt) );
      rc = btreeGetPage(pBt, pBt->nPage, &pPg, 1);
      if( rc==SQLITE_OK ){
        rc = sqlite3PagerWrite(pPg->pDbPage);
        releasePage(pPg);
      }
      if( rc ) return rc;
      pBt->nPage++;
      if( pBt->nPage==PENDING_BYTE_PAGE(pBt) ){ pBt->nPage++; }
    }
#endif
    put4byte(28 + (u8*)pBt->pPage1->aData, pBt->nPage);
    *pPgno = pBt->nPage;

    assert( *pPgno!=PENDING_BYTE_PAGE(pBt) );
    rc = btreeGetPage(pBt, *pPgno, ppPage, 1);
    if( rc ) return rc;
    rc = sqlite3PagerWrite((*ppPage)->pDbPage);
    if( rc!=SQLITE_OK ){
      releasePage(*ppPage);
    }
    TRACE(("ALLOCATE: %d from end of file\n", *pPgno));
  }

  assert( *pPgno!=PENDING_BYTE_PAGE(pBt) );

end_allocate_page:
  releasePage(pTrunk);
  releasePage(pPrevTrunk);
  if( rc==SQLITE_OK ){
    if( sqlite3PagerPageRefcount((*ppPage)->pDbPage)>1 ){
      releasePage(*ppPage);
      return SQLITE_CORRUPT_BKPT;
    }
    (*ppPage)->isInit = 0;
  }else{
    *ppPage = 0;
  }
  assert( rc!=SQLITE_OK || sqlite3PagerIswriteable((*ppPage)->pDbPage) );
  return rc;
}

/*
** This function is used to add page iPage to the database file free-list. 
** It is assumed that the page is not already a part of the free-list.
** 这个函数用于添加页面iPage到数据库文件空闲列表。假定页面不是空闲列表的一部分。
** The value passed as the second argument to this function is optional.
** If the caller happens to have a pointer to the MemPage object 
** corresponding to page iPage handy, it may pass it as the second value. 
** Otherwise, it may pass NULL.
** 作为第二个参数传递给该函数的值是可选的.如果调用者碰巧有一个指针指向MemPage对象对应 iPage页面,
** 它可能把它作为第二个值.否则,它可能为空。
** If a pointer to a MemPage object is passed as the second argument,
** its reference count is not altered by this function.
** 如果一个指针MemPage对象作为第二个参数传递,那么它引用数不会被这个函数改变。
*/ 
static int freePage2(BtShared *pBt, MemPage *pMemPage, Pgno iPage){       //添加页面iPage到数据库文件空闲列表
  MemPage *pTrunk = 0;                /* Free-list trunk page */                 //空闲列表页的主页面
  Pgno iTrunk = 0;                    /* Page number of free-list trunk page */  //空闲列表页的主页面的页码
  MemPage *pPage1 = pBt->pPage1;      /* Local reference to page 1 */            //内存引用页1
  MemPage *pPage;                     /* Page being freed. May be NULL. */       //页被释放，肯能是空
  int rc;                             /* Return Code */                          //返回代码
  int nFree;                          /* Initial number of pages on free-list */ //空闲列表页上最初的页数量

  assert( sqlite3_mutex_held(pBt->mutex) );
  assert( iPage>1 );
  assert( !pMemPage || pMemPage->pgno==iPage );

  if( pMemPage ){
    pPage = pMemPage;
    sqlite3PagerRef(pPage->pDbPage);
  }else{
    pPage = btreePageLookup(pBt, iPage);
  }

  /* Increment the free page count on pPage1 */    //递增pPage1上的空闲页的数量
  rc = sqlite3PagerWrite(pPage1->pDbPage);
  if( rc ) goto freepage_out;                     //如果rc值为0转到freepage_out
  nFree = get4byte(&pPage1->aData[36]);
  put4byte(&pPage1->aData[36], nFree+1);

  if( pBt->btsFlags & BTS_SECURE_DELETE ){
    /* If the secure_delete option is enabled, then
    ** always fully overwrite deleted information with zeros.
	** 如果secure_delete选项可用，那么总是完全重写删除信息为0.
    */
    if( (!pPage && ((rc = btreeGetPage(pBt, iPage, &pPage, 0))!=0) )
     ||            ((rc = sqlite3PagerWrite(pPage->pDbPage))!=0)
    ){
      goto freepage_out;
    }
    memset(pPage->aData, 0, pPage->pBt->pageSize);
  }

  /* If the database supports auto-vacuum, write an entry in the pointer-map
  ** to indicate that the page is free.
  ** 如果数据库支持自动清理，写一个条目在指针位图来表明也是空闲的。
  */
  if( ISAUTOVACUUM ){
    ptrmapPut(pBt, iPage, PTRMAP_FREEPAGE, 0, &rc);
    if( rc ) goto freepage_out;
  }

  /* Now manipulate the actual database free-list structure. There are two
  ** possibilities. If the free-list is currently empty, or if the first
  ** trunk page in the free-list is full, then this page will become a
  ** new free-list trunk page. Otherwise, it will become a leaf of the
  ** first trunk page in the current free-list. This block tests if it
  ** is possible to add the page as a new free-list leaf.
  ** 现在操作真实的数据库空闲列表结构。有两个的可能性。如果空闲列表当前为空,或如果空闲列表
  ** 上的第一主页面是满的,那么这将成为一个页面新的空闲列表的主页面。否则,它将成为当前空闲列
  ** 中的第一个主页面的一个叶子。如果它可以添加页面作为一个新的空闲列表的叶子则对这个块测试。
  */
  if( nFree!=0 ){
    u32 nLeaf;                /* Initial number of leaf cells on trunk page */  //主页面上最初的叶子单元的数量

    iTrunk = get4byte(&pPage1->aData[32]);
    rc = btreeGetPage(pBt, iTrunk, &pTrunk, 0);
    if( rc!=SQLITE_OK ){
      goto freepage_out;
    }

    nLeaf = get4byte(&pTrunk->aData[4]);
    assert( pBt->usableSize>32 );
    if( nLeaf > (u32)pBt->usableSize/4 - 2 ){
      rc = SQLITE_CORRUPT_BKPT;
      goto freepage_out;
    }
    if( nLeaf < (u32)pBt->usableSize/4 - 8 ){
      /* In this case there is room on the trunk page to insert the page
      ** being freed as a new leaf.
      ** 在这种情况下,在主页面上有空间插入被释放的页面做新叶子。
      ** Note that the trunk page is not really full until it contains
      ** usableSize/4 - 2 entries, not usableSize/4 - 8 entries as we have
      ** coded.  But due to a coding error in versions of SQLite prior to
      ** 3.6.0, databases with freelist trunk pages holding more than
      ** usableSize/4 - 8 entries will be reported as corrupt.  In order
      ** to maintain backwards compatibility with older versions of SQLite,
      ** we will continue to restrict the number of entries to usableSize/4 - 8
      ** for now.  At some point in the future (once everyone has upgraded
      ** to 3.6.0 or later) we should consider fixing the conditional above
      ** to read "usableSize/4-2" instead of "usableSize/4-8".
	  ** 注意,主页面不是真正的满直到他包含(usableSize/4-2)个条目,而不是（usableSize/4-8)个条目。
	  ** 但由于之前版本3.6.0的SQLite的编码错误，有空闲列表主页面的数据库有多于（usableSize/4-8）
	  ** 个条目被报告崩溃。为与老版本的SQLite保持向后兼容性，将继续限制条目的数量usableSize/4-8。
	  ** 在将来的某个时候(每个人都有一次升级3.6.0或之后)我们应该考虑解决上面的条件读“usableSize/4-2”,
	  ** 而不是“usableSize/4-8”。
      */
      rc = sqlite3PagerWrite(pTrunk->pDbPage);
      if( rc==SQLITE_OK ){
        put4byte(&pTrunk->aData[4], nLeaf+1);
        put4byte(&pTrunk->aData[8+nLeaf*4], iPage);
        if( pPage && (pBt->btsFlags & BTS_SECURE_DELETE)==0 ){
          sqlite3PagerDontWrite(pPage->pDbPage);
        }
        rc = btreeSetHasContent(pBt, iPage);
      }
      TRACE(("FREE-PAGE: %d leaf on trunk page %d\n",pPage->pgno,pTrunk->pgno));
      goto freepage_out;
    }
  }

  /* If control flows to this point, then it was not possible to add the
  ** the page being freed as a leaf page of the first trunk in the free-list.
  ** Possibly because the free-list is empty, or possibly because the 
  ** first trunk in the free-list is full. Either way, the page being freed
  ** will become the new first trunk page in the free-list.
  ** 如果控制流达到这一点,那么它是不可能的添加被释放的页面成为空闲列表中的主页面的第一个叶子页面。
  ** 可能是因为空闲列表是空的,或可能是因为空闲列表中的第一个主页面已经满了。无论哪种方式,页面被释放
  ** 将成为新的空闲列表中的第一个主页面。
  */
  if( pPage==0 && SQLITE_OK!=(rc = btreeGetPage(pBt, iPage, &pPage, 0)) ){
    goto freepage_out;
  }
  rc = sqlite3PagerWrite(pPage->pDbPage);
  if( rc!=SQLITE_OK ){
    goto freepage_out;
  }
  put4byte(pPage->aData, iTrunk);
  put4byte(&pPage->aData[4], 0);
  put4byte(&pPage1->aData[32], iPage);
  TRACE(("FREE-PAGE: %d new trunk page replacing %d\n", pPage->pgno, iTrunk));

freepage_out:
  if( pPage ){
    pPage->isInit = 0;
  }
  releasePage(pPage);
  releasePage(pTrunk);
  return rc;
}
static void freePage(MemPage *pPage, int *pRC){
  if( (*pRC)==SQLITE_OK ){
    *pRC = freePage2(pPage->pBt, pPage, pPage->pgno);
  }
}

/*Free any overflow pages associated with the given Cell.*/   
static int clearCell(MemPage *pPage, unsigned char *pCell){     //释放任何与给定单元相关的溢出页
  BtShared *pBt = pPage->pBt;
  CellInfo info;
  Pgno ovflPgno;
  int rc;
  int nOvfl;
  u32 ovflPageSize;

  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  btreeParseCellPtr(pPage, pCell, &info);
  if( info.iOverflow==0 ){
    return SQLITE_OK;  /* No overflow pages. Return without doing anything */   //没有溢出页，不做操作返回
  }
  if( pCell+info.iOverflow+3 > pPage->aData+pPage->maskPage ){
    return SQLITE_CORRUPT;  /* Cell extends past end of page */  //单元超过了页面的范围
  }
  ovflPgno = get4byte(&pCell[info.iOverflow]);
  assert( pBt->usableSize > 4 );
  ovflPageSize = pBt->usableSize - 4;
  nOvfl = (info.nPayload - info.nLocal + ovflPageSize - 1)/ovflPageSize;
  assert( ovflPgno==0 || nOvfl>0 );
  while( nOvfl-- ){
    Pgno iNext = 0;
    MemPage *pOvfl = 0;
    if( ovflPgno<2 || ovflPgno>btreePagecount(pBt) ){
      /* 0 is not a legal page number and page 1 cannot be an 
      ** overflow page. Therefore if ovflPgno<2 or past the end of the 
      ** file the database must be corrupt.
	  0不是合法的页码并且1不可能是溢出页。因此，如果ovflPgno<2或者超过数据库文件一定返回SQLITE_CORRUPT_BKPT*/
      return SQLITE_CORRUPT_BKPT;
    }
    if( nOvfl ){
      rc = getOverflowPage(pBt, ovflPgno, &pOvfl, &iNext);
      if( rc ) return rc;
    }

    if( ( pOvfl || ((pOvfl = btreePageLookup(pBt, ovflPgno))!=0) )
     && sqlite3PagerPageRefcount(pOvfl->pDbPage)!=1
    ){
      /* There is no reason any cursor should have an outstanding reference 
      ** to an overflow page belonging to a cell that is being deleted/updated.
      ** So if there exists more than one reference to this page, then it 
      ** must not really be an overflow page and the database must be corrupt. 
      ** It is helpful to detect this before calling freePage2(), as 
      ** freePage2() may zero the page contents if secure-delete mode is
      ** enabled. If this 'overflow' page happens to be a page that the
      ** caller is iterating through or using in some other way, this
      ** can be problematic.
	  ** 毫无疑问任何游标应该有一突出的引用属于正被删除/更新的单元的溢出页。如果存在对这个页面多个引用,那么它
	  ** 不一定是一个真的溢出页面和数据库一定崩溃。它有助于调用freePage2()之前检测该情况.如果安全删除模式启用，
	  ** freePage2()会清零页面内容。如果这“溢出”页面是一个调用遍历或以其他方式使用的页面,这可能是有问题的。
      */
      rc = SQLITE_CORRUPT_BKPT;
    }else{
      rc = freePage2(pBt, pOvfl, ovflPgno);
    }

    if( pOvfl ){
      sqlite3PagerUnref(pOvfl->pDbPage);
    }
    if( rc ) return rc;
    ovflPgno = iNext;
  }
  return SQLITE_OK;
}

/*
** Create the byte sequence used to represent a cell on page pPage
** and write that byte sequence into pCell[].  Overflow pages are
** allocated and filled in as necessary.  The calling procedure
** is responsible for making sure sufficient space has been allocated
** for pCell[].
** 创建字节序列用来代表一个pPage页上的单元并将字节序列写到pCell[]。溢出页面
** 被分配且在必要时填写。调用程序负责确保足够的空间分配为pCell[]。
** Note that pCell does not necessary need to point to the pPage->aData
** area.  pCell might point to some temporary storage.  The cell will
** be constructed in this temporary area then copied into pPage->aData
** later.
** 注意,pCell并不必要需要指向pPage->aData区域。pCell可能指向一些临时存储区。
** 单元会在这个临时区域被创建然后复制到pPage->aData。
*/
/*创建字节序列写入pCell*/
static int fillInCell(     //创建字节序列用来代表一个pPage页上的单元并将字节序列写到pCell[]
  MemPage *pPage,                /* The page that contains the cell */     //包含该单元的页
  unsigned char *pCell,          /* Complete text of the cell */           //单元的完整文本
  const void *pKey, i64 nKey,    /* The key */                             //关键字
  const void *pData,int nData,   /* The data */                            //数据域
  int nZero,                     /* Extra zero bytes to append to pData */ //附加在pData上的额外0字节
  int *pnSize                    /* Write cell size here */                //将单元的大小写到该变量
){
  int nPayload;
  const u8 *pSrc;
  int nSrc, n, rc;
  int spaceLeft;
  MemPage *pOvfl = 0;
  MemPage *pToRelease = 0;
  unsigned char *pPrior;
  unsigned char *pPayload;
  BtShared *pBt = pPage->pBt;
  Pgno pgnoOvfl = 0;
  int nHeader;
  CellInfo info;

  assert( sqlite3_mutex_held(pPage->pBt->mutex) );

  /* pPage is not necessarily writeable since pCell might be auxiliary
  ** buffer space that is separate from the pPage buffer area 
  ** pPage不一定是可写的因为pCell可能是从pPage缓冲区分出的辅助缓冲区空间.*/
  assert( pCell<pPage->aData || pCell>=&pPage->aData[pBt->pageSize]
            || sqlite3PagerIswriteable(pPage->pDbPage) );

  /* Fill in the header. */     //添加头信息
  nHeader = 0;
  if( !pPage->leaf ){
    nHeader += 4;
  }
  if( pPage->hasData ){
    nHeader += putVarint(&pCell[nHeader], nData+nZero);
  }else{
    nData = nZero = 0;
  }
  nHeader += putVarint(&pCell[nHeader], *(u64*)&nKey);
  btreeParseCellPtr(pPage, pCell, &info);              //解析单元内容块，填在CellInfo结构中
  assert( info.nHeader==nHeader );
  assert( info.nKey==nKey );
  assert( info.nData==(u32)(nData+nZero) );
  
  /* Fill in the payload */      //添加记录
  nPayload = nData + nZero;
  if( pPage->intKey ){
    pSrc = pData;
    nSrc = nData;
    nData = 0;
  }else{ 
    if( NEVER(nKey>0x7fffffff || pKey==0) ){
      return SQLITE_CORRUPT_BKPT;
    }
    nPayload += (int)nKey;
    pSrc = pKey;
    nSrc = (int)nKey;
  }
  *pnSize = info.nSize;
  spaceLeft = info.nLocal;
  pPayload = &pCell[nHeader];
  pPrior = &pCell[info.iOverflow];

  while( nPayload>0 ){
    if( spaceLeft==0 ){
#ifndef SQLITE_OMIT_AUTOVACUUM
      Pgno pgnoPtrmap = pgnoOvfl; /* Overflow page pointer-map entry page */  //溢出页位图指针条目页
      if( pBt->autoVacuum ){
        do{
          pgnoOvfl++;
        } while( 
          PTRMAP_ISPAGE(pBt, pgnoOvfl) || pgnoOvfl==PENDING_BYTE_PAGE(pBt) 
        );
      }
#endif
      rc = allocateBtreePage(pBt, &pOvfl, &pgnoOvfl, pgnoOvfl, 0);   //从数据库文件分配一个新页面，成功则返回SQLITE_OK
#ifndef SQLITE_OMIT_AUTOVACUUM
      /* If the database supports auto-vacuum, and the second or subsequent
      ** overflow page is being allocated, add an entry to the pointer-map
      ** for that page now. 
      ** 如果数据库支持自动清理，且第二个或后继的溢出页被分配，对该页加条目到指针位图.
      ** If this is the first overflow page, then write a partial entry 
      ** to the pointer-map. If we write nothing to this pointer-map slot,
      ** then the optimistic overflow chain processing in clearCell()
      ** may misinterpret the uninitialised values and delete the
      ** wrong pages from the database.
	  ** 如果这是第一个溢出页，那么写一个局部页条目到指针位图.如果不写到指针位图位置，那么
	  ** 客观来说在clearCell()中处理的溢出链接将会弄错未初始化的值并且将从数据库中删除错误页.
      */
      if( pBt->autoVacuum && rc==SQLITE_OK ){
        u8 eType = (pgnoPtrmap?PTRMAP_OVERFLOW2:PTRMAP_OVERFLOW1);
        ptrmapPut(pBt, pgnoOvfl, eType, pgnoPtrmap, &rc);
        if( rc ){
          releasePage(pOvfl);
        }
      }
#endif
      if( rc ){
        releasePage(pToRelease);  //释放内存页
        return rc;
      }

      /* If pToRelease is not zero than pPrior points into the data area
      ** of pToRelease.  Make sure pToRelease is still writeable. 
	  ** 如果pToRelease一不为0，pPrior就指向pToRelease的数据域.确保pToRelease是可写的. */
      assert( pToRelease==0 || sqlite3PagerIswriteable(pToRelease->pDbPage) );

      /* If pPrior is part of the data area of pPage, then make sure pPage
      ** is still writeable 
	  ** 如果pPrior是pPage数据域的一部分，那么确保pPage仍然可写. */
      assert( pPrior<pPage->aData || pPrior>=&pPage->aData[pBt->pageSize]
            || sqlite3PagerIswriteable(pPage->pDbPage) );

      put4byte(pPrior, pgnoOvfl);
      releasePage(pToRelease);
      pToRelease = pOvfl;
      pPrior = pOvfl->aData;
      put4byte(pPrior, 0);
      pPayload = &pOvfl->aData[4];
      spaceLeft = pBt->usableSize - 4;
    }
    n = nPayload;
    if( n>spaceLeft ) n = spaceLeft;

    /* If pToRelease is not zero than pPayload points into the data area
    ** of pToRelease.  Make sure pToRelease is still writeable.
	** 如果pToRelease一不为0，pPrior就指向pToRelease的数据域.确保pToRelease是可写的.  */
    assert( pToRelease==0 || sqlite3PagerIswriteable(pToRelease->pDbPage) );

    /* If pPayload is part of the data area of pPage, then make sure pPage
    ** is still writeable 
	** 如果pPrior是pPage数据域的一部分，那么确保pPage仍然可写.*/
    assert( pPayload<pPage->aData || pPayload>=&pPage->aData[pBt->pageSize]
            || sqlite3PagerIswriteable(pPage->pDbPage) );

    if( nSrc>0 ){
      if( n>nSrc ) n = nSrc;
      assert( pSrc );
      memcpy(pPayload, pSrc, n);
    }else{
      memset(pPayload, 0, n);
    }
    nPayload -= n;
    pPayload += n;
    pSrc += n;
    nSrc -= n;
    spaceLeft -= n;
    if( nSrc==0 ){
      nSrc = nData;
      pSrc = pData;
    }
  }
  releasePage(pToRelease);  //释放内存页
  return SQLITE_OK;
}

/*
** Remove the i-th cell from pPage.  This routine effects pPage only.
** The cell content is not freed or deallocated.  It is assumed that
** the cell content has been copied someplace else.  This routine just
** removes the reference to the cell from pPage.
** 删除pPage的第i个单元.这个函数仅仅对pPage其作用.单元的内容不会释放或消失.
** 假定单元的内容已经拷贝到其他地方.这个函数将只pPage中单元的引用.
** "sz" must be the number of bytes in the cell. //参数sz是单元的字节数.
*/
static void dropCell(MemPage *pPage, int idx, int sz, int *pRC){      //删除pPage的第i个单元.
  u32 pc;         /* Offset to cell content of cell being deleted */  //要被删除的单元内容的偏移量
  u8 *data;       /* pPage->aData */                                  //pPage->aData的数据
  u8 *ptr;        /* Used to move bytes around within data[] */       //在data[]中用于移动字节
  u8 *endPtr;     /* End of loop */                                   //循环结束
  int rc;         /* The return code */                               //返回代码
  int hdr;        /* Beginning of the header.  0 most pages.  100 page 1 */   //头部的开始，为0是其他页，为1 是第一页

  if( *pRC ) return;

  assert( idx>=0 && idx<pPage->nCell );
  assert( sz==cellSize(pPage, idx) );
  assert( sqlite3PagerIswriteable(pPage->pDbPage) );
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  data = pPage->aData;
  ptr = &pPage->aCellIdx[2*idx];
  pc = get2byte(ptr);
  hdr = pPage->hdrOffset;
  testcase( pc==get2byte(&data[hdr+5]) );
  testcase( pc+sz==pPage->pBt->usableSize );
  if( pc < (u32)get2byte(&data[hdr+5]) || pc+sz > pPage->pBt->usableSize ){
    *pRC = SQLITE_CORRUPT_BKPT;
    return;
  }
  rc = freeSpace(pPage, pc, sz);   //释放pPage->aData的部分并写入空闲列表
  if( rc ){
    *pRC = rc;
    return;
  }
  endPtr = &pPage->aCellIdx[2*pPage->nCell - 2];
  assert( (SQLITE_PTR_TO_INT(ptr)&1)==0 );  /* ptr is always 2-byte aligned */  //ptr总是两字节
  while( ptr<endPtr ){
    *(u16*)ptr = *(u16*)&ptr[2];
    ptr += 2;
  }
  pPage->nCell--;
  put2byte(&data[hdr+3], pPage->nCell);
  pPage->nFree += 2;
}

/*
** Insert a new cell on pPage at cell index "i".  pCell points to the
** content of the cell.
** 在pPage的单元索引i处插入一个新单元.pCell指向单元的内容.
** If the cell content will fit on the page, then put it there.  If it
** will not fit, then make a copy of the cell content into pTemp if
** pTemp is not null.  Regardless of pTemp, allocate a new entry
** in pPage->apOvfl[] and make it point to the cell content (either
** in pTemp or the original pCell) and also record its index. 
** Allocating a new entry in pPage->aCell[] implies that 
** pPage->nOverflow is incremented.
** 如果单元内容在页上适合，则将放在此处.如果不适合，那么如果pTemp非空拷贝单元的内容到pTemp.
** 不管pTemp，在pPage->apOvfl[]中分配一个新条目并且是他指向单元内容(或者是pTemp或者是
** 原有的pCell)也记录它的索。在pPage->aCell[]中分配一个新的条目实现pPage->nOverflow的增长.
** If nSkip is non-zero, then do not copy the first nSkip bytes of the
** cell. The caller will overwrite them after this function returns. If
** nSkip is zero, then pCell may not point to an invalid memory location 
** (but pCell+nSkip is always valid).
** 如果nSkip是非零的,那么不要复制单元的第一个nSkip字节。这个函数返回后调用者将覆盖他们。
** 如果nSkip是零,那么pCell并不指向一个无效的内存位置(但pCell + nSkip总是有效)。
*/
/*在页的第i个单元格中插入一个单元格*/
static void insertCell(             //在pPage的单元索引i处插入一个新单元
  MemPage *pPage,   /* Page into which we are copying */                      //存放拷贝内容的页
  int i,            /* New cell becomes the i-th cell of the page */          //新单元将变为页的第i个单元
  u8 *pCell,        /* Content of the new cell */                             //新单元的内容
  int sz,           /* Bytes of content in pCell */                           //pCell中内容的字节
  u8 *pTemp,        /* Temp storage space for pCell, if needed */             //如果需要，它将是pCell的临时存储空间
  Pgno iChild,      /* If non-zero, replace first 4 bytes with this value */  //非零则替换这个值的开始的4个字节.
  int *pRC          /* Read and write return code from here */                //从这读或写返回字节
){
  int idx = 0;      /* Where to write new cell content in data[] */           //在data[]中写新单元的内容
  int j;            /* Loop counter */                                        //循环计数
  int end;          /* First byte past the last cell pointer in data[] */     //data[]中最后一个单元后的第一个字节
  int ins;          /* Index in data[] where new cell pointer is inserted */  //data[]中将要插入新单元地方的索引
  int cellOffset;   /* Address of first cell pointer in data[] */             //data[]中第一个单元指针的地址
  u8 *data;         /* The content of the whole page */                       //整个页的内容
  u8 *ptr;          /* Used for moving information around in data[] */        //data[]中用作移动信息
  u8 *endPtr;       /* End of the loop */                                     //循环的结尾

  int nSkip = (iChild ? 4 : 0);

  if( *pRC ) return;

  assert( i>=0 && i<=pPage->nCell+pPage->nOverflow );
  assert( pPage->nCell<=MX_CELL(pPage->pBt) && MX_CELL(pPage->pBt)<=10921 );
  assert( pPage->nOverflow<=ArraySize(pPage->apOvfl) );
  assert( ArraySize(pPage->apOvfl)==ArraySize(pPage->aiOvfl) );
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  /* The cell should normally be sized correctly.  However, when moving a
  ** malformed(畸形的) cell from a leaf page to an interior page, if the cell size
  ** wanted to be less than 4 but got rounded up to(算到) 4 on the leaf, then size
  ** might be less than 8 (leaf-size + pointer) on the interior node.  Hence
  ** the term after the || in the following assert(). 
  ** 单元大小通常应正确。然而,当移动畸形单元从一片叶子页面内部页,如果单元的大小少于4但在叶上算到了4,
  ** 那么内部节点上大小可能少于8(leaf-size + pointer)。*/
  assert( sz==cellSizePtr(pPage, pCell) || (sz==8 && iChild>0) );
  if( pPage->nOverflow || sz+2>pPage->nFree ){
    if( pTemp ){
      memcpy(pTemp+nSkip, pCell+nSkip, sz-nSkip);
      pCell = pTemp;
    }
    if( iChild ){
      put4byte(pCell, iChild);
    }
    j = pPage->nOverflow++;
    assert( j<(int)(sizeof(pPage->apOvfl)/sizeof(pPage->apOvfl[0])) );
    pPage->apOvfl[j] = pCell;
    pPage->aiOvfl[j] = (u16)i;
  }else{
    int rc = sqlite3PagerWrite(pPage->pDbPage);
    if( rc!=SQLITE_OK ){
      *pRC = rc;
      return;
    }
    assert( sqlite3PagerIswriteable(pPage->pDbPage) );
    data = pPage->aData;
    cellOffset = pPage->cellOffset;
    end = cellOffset + 2*pPage->nCell;
    ins = cellOffset + 2*i;
    rc = allocateSpace(pPage, sz, &idx);/*在pPage上分配sz字节的空间，将索引写入idx中*/
    if( rc ){ *pRC = rc; return; }
    /* The allocateSpace() routine guarantees the following two properties
    ** if it returns success */
    assert( idx >= end+2 );
    assert( idx+sz <= (int)pPage->pBt->usableSize );
    pPage->nCell++;
    pPage->nFree -= (u16)(2 + sz);
    memcpy(&data[idx+nSkip], pCell+nSkip, sz-nSkip);/*将pCell的内容拷贝到data*/
    if( iChild ){
      put4byte(&data[idx], iChild);
    }
    ptr = &data[end];
    endPtr = &data[ins];
    assert( (SQLITE_PTR_TO_INT(ptr)&1)==0 );  /* ptr is always 2-byte aligned */
    while( ptr>endPtr ){
      *(u16*)ptr = *(u16*)&ptr[-2];
      ptr -= 2;
    }
    put2byte(&data[ins], idx);
    put2byte(&data[pPage->hdrOffset+3], pPage->nCell);
#ifndef SQLITE_OMIT_AUTOVACUUM
    if( pPage->pBt->autoVacuum ){
      /* The cell may contain a pointer to an overflow page. If so, write
      ** the entry for the overflow page into the pointer map.
      ** 单元可能包含到溢出页的指针.如果包含，则对于溢出页写条目到指针位图*/
      ptrmapPutOvflPtr(pPage, pCell, pRC);
    }
#endif
  }
}

/*
** Add a list of cells to a page.  The page should be initially empty.
** The cells are guaranteed to fit on the page.
*/
/*
添加一个页上的单元格。该页面应该是最初为空。确保单元格适合页。
*/

static void assemblePage(        //在页上添加单元列表
  MemPage *pPage,   /* The page to be assemblied */                   //装配页
  int nCell,        /* The number of cells to add to this page */     //添加到页上的单元数
  u8 **apCell,      /* Pointers to cell bodies */                     //单元体的指针
  u16 *aSize        /* Sizes of the cells */                          //单元得大小
){
  int i;            /* Loop counter */                                //循环计数变量
  u8 *pCellptr;     /* Address of next cell pointer */                //下一单元的指针地址
  int cellbody;     /* Address of next cell body */                   //下一个单元体的地址
  u8 * const data = pPage->aData;             /* Pointer to data for pPage */     //页中数据的指针
  const int hdr = pPage->hdrOffset;           /* Offset of header on pPage */     //页上头部的偏移量
  const int nUsable = pPage->pBt->usableSize; /* Usable size of page */           //可用页的大小

  assert( pPage->nOverflow==0 );
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  assert( nCell>=0 && nCell<=(int)MX_CELL(pPage->pBt)  //添加的单元数大于0，且小于允许的最大单元数，页上的最大单元数<=10921
            && (int)MX_CELL(pPage->pBt)<=10921);
  assert( sqlite3PagerIswriteable(pPage->pDbPage) );

  /* Check that the page has just been zeroed by zeroPage() */  //检查页是否已经被zeroPage()置零.
  assert( pPage->nCell==0 );
  assert( get2byteNotZero(&data[hdr+5])==nUsable );

  pCellptr = &pPage->aCellIdx[nCell*2];
  cellbody = nUsable;
  for(i=nCell-1; i>=0; i--){
    u16 sz = aSize[i];
    pCellptr -= 2;
    cellbody -= sz;
    put2byte(pCellptr, cellbody);
    memcpy(&data[cellbody], apCell[i], sz);     //从apCell[i]的起始位置开始拷贝sz个字节到&data[cellbody]的起始位置上
  }
  put2byte(&data[hdr+3], nCell);
  put2byte(&data[hdr+5], cellbody);
  pPage->nFree -= (nCell*2 + nUsable - cellbody);
  pPage->nCell = (u16)nCell;
}

/*
** The following parameters determine how many adjacent pages get involved
** in a balancing operation.  NN is the number of neighbors on either side
** of the page that participate in the balancing operation.  NB is the
** total number of pages that participate, including the target page and
** NN neighbors on either side.
** 以下参数确定在一个平衡操作中有多少相邻页面参与进来。NN是参与平衡操作的页面相邻页面的数量。
** NB的所涉及的页面总数,包括目标页面和NN的相邻页面。
** The minimum value of NN is 1 (of course).  Increasing NN above 1
** (to 2 or 3) gives a modest improvement in SELECT and DELETE performance
** in exchange for a larger degradation in INSERT and UPDATE performance.
** The value of NN appears to give the best results overall.
** NN的最小值是1。增加NN使之大于1(2或3)能够改善SELECT和DELETE性能,以换取更大的插入和更新性能的退化。
** NN值似乎给了最好的结果。
*/
/*下面的参数确定在平衡操作里面涉及多少相邻的页面，数量记为NN。NB是参与的页的总数量。
NN的最小值是1。增加NN到1以上（2或3)， 能够改善SELECT和DELETE性能。
*/


#define NN 1             /* Number of neighbors on either side of pPage */   //pPage两侧相邻的页数
#define NB (NN*2+1)      /* Total pages involved in the balance */           //在平衡中涉及的总页数


#ifndef SQLITE_OMIT_QUICKBALANCE
/*
** This version of balance() handles the common special case where
** a new entry is being inserted on the extreme right-end of the
** tree, in other words, when the new entry will become the largest
** entry in the tree.
** 这个balance()版本处理常见的特殊情况，一个新条目被插入到树的最右端.
** 换句话说,当新条目将成为树中最大的条目。
** Instead of trying to balance the 3 right-most leaf pages, just add
** a new page to the right-hand side and put the one new entry in
** that page.  This leaves the right side of the tree somewhat
** unbalanced.  But odds are that we will be inserting new entries
** at the end soon afterwards so the nearly empty page will quickly
** fill up.  On average.
** 而不是试图平衡最右边的3个叶页面,添加一个新页面的右边,放一个新条目在这个页面中。
** 这使得树的右边不平衡的。但奇怪的是,我们将插入新的条目到最后，所以很快将空页面添满。
** pPage is the leaf page which is the right-most page in the tree.
** pParent is its parent.  pPage must have a single overflow entry
** which is also the right-most entry on the page.
** pPage是叶子页面，它是树上最右边的页面。pParent是它的父节点。pPage必须有单独的溢出条目也页面上最右边的条目。
** The pSpace buffer is used to store a temporary copy of the divider
** cell that will be inserted into pParent. Such a cell consists of a 4
** byte page number followed by a variable length integer. In other
** words, at most 13 bytes. Hence the pSpace buffer must be at
** least 13 bytes in size.
** pSpace缓冲区用于存储将插入pParent的临时副本的单元。这样一个单元包含在一个可变长度的整数后的4字节页码组成。
** 换句话说,最多13字节。因此,pSpace缓冲区必须要至少13个字节大小。
*/

/*
此版本的balance()处理常见的特殊情况。新条目被插在树的最右端，
换句话说，新的条目将成为最大的条目。pPage是叶子页，在树中是最右边的页面。
pParent是其父节点。 pPage是一个溢出页的条目。
*/
static int balance_quick(MemPage *pParent, MemPage *pPage, u8 *pSpace){  //处理常见的情况，一个新条目被插入到树的最右端.
  BtShared *const pBt = pPage->pBt;    /* B-Tree Database */             //数据库中B树
  MemPage *pNew;                       /* Newly allocated page */        //新分配的页
  int rc;                              /* Return Code */                 //返回代码
  Pgno pgnoNew;                        /* Page number of pNew */         // pNew的页码

  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  assert( sqlite3PagerIswriteable(pParent->pDbPage) );
  assert( pPage->nOverflow==1 );

  /* This error condition is now caught prior to reaching this function */ 
  if( pPage->nCell<=0 ) return SQLITE_CORRUPT_BKPT;

  /* Allocate a new page. This page will become the right-sibling of 
  ** pPage. Make the parent page writable, so that the new divider cell
  ** may be inserted. If both these operations are successful, proceed.
  ** 分配一个新页，该页将变为pPage右侧的分支，确保父页面时可写的以便于新分出的单元被插入。如果这两个操作都成功，则保护。
  */
  rc = allocateBtreePage(pBt, &pNew, &pgnoNew, 0, 0);  //分配一个新页

  if( rc==SQLITE_OK ){

    u8 *pOut = &pSpace[4];
    u8 *pCell = pPage->apOvfl[0];
    u16 szCell = cellSizePtr(pPage, pCell);  //计算一个单元需要的总的字节数并赋值给szCell
    u8 *pStop;

    assert( sqlite3PagerIswriteable(pNew->pDbPage) );
    assert( pPage->aData[0]==(PTF_INTKEY|PTF_LEAFDATA|PTF_LEAF) );
    zeroPage(pNew, PTF_INTKEY|PTF_LEAFDATA|PTF_LEAF);
    assemblePage(pNew, 1, &pCell, &szCell);

    /* If this is an auto-vacuum database, update the pointer map
    ** with entries for the new page, and any pointer from the 
    ** cell on the page to an overflow page. If either of these
    ** operations fails, the return code is set, but the contents
    ** of the parent page are still manipulated by thh code below.
    ** That is Ok, at this point the parent page is guaranteed to
    ** be marked as dirty. Returning an error code will cause a
    ** rollback, undoing any changes made to the parent page.
    */
	/*如果这是一个自动清空的数据库，更新指向一个新页的条目。如果这些
	操作失败，返回代码被设置，但父页面被thh代码操纵。在这一点上保证父页面
	被标记为脏字。返回错误代码将导致回滚，撤销父页面所做的任何更改。
	*/
    if( ISAUTOVACUUM ){
      ptrmapPut(pBt, pgnoNew, PTRMAP_BTREE, pParent->pgno, &rc);
      if( szCell>pNew->minLocal ){
        ptrmapPutOvflPtr(pNew, pCell, &rc);
      }
    }
  
    /* Create a divider cell to insert into pParent. The divider cell
    ** consists of a 4-byte page number (the page number of pPage) and
    ** a variable length key value (which must be the same value as the
    ** largest key on pPage).
    **
    ** To find the largest key value on pPage, first find the right-most 
    ** cell on pPage. The first two fields of this cell are the 
    ** record-length (a variable length integer at most 32-bits in size)
    ** and the key value (a variable length integer, may have any value).
    ** The first of the while(...) loops below skips over the record-length
    ** field. The second while(...) loop copies the key value from the
    ** cell on pPage into the pSpace buffer.
    */

	/*创建一个除法器单元，插入到pParent。除法器由一个4字节的页号（PPAGE的页号），
	和一个可变长度关键字的值（它必须是相同的值作为PPAGE上最大的键）组成。
	要查找PPAGE最大的键值，先找到最右边的pPage单元格。这个单元的前两个字段是记录长度
	(一个可变长度的整数大小最多32位)和关键字值(一个可变长度的整数,可能有价值)。
	第一个while循环跳过以下记录长度字段。第二个while循环拷贝pPage上的单元关键字值到pSpace缓冲区。 
*/
    pCell = findCell(pPage, pPage->nCell-1);
    pStop = &pCell[9];
    while( (*(pCell++)&0x80) && pCell<pStop );
    pStop = &pCell[9];
    while( ((*(pOut++) = *(pCell++))&0x80) && pCell<pStop );

    /* Insert the new divider cell into pParent. */  //插入新的除法器单元到pParent
    insertCell(pParent, pParent->nCell, pSpace, (int)(pOut-pSpace),
               0, pPage->pgno, &rc);

    /* Set the right-child pointer of pParent to point to the new page. */  //设置pParent右孩子的指针指向新页
    put4byte(&pParent->aData[pParent->hdrOffset+8], pgnoNew);
  
    /* Release the reference to the new page. */     //释放对新页的引用
    releasePage(pNew);
  }

  return rc;
}
#endif /* SQLITE_OMIT_QUICKBALANCE */

#if 0
/*
** This function does not contribute anything to the operation of SQLite.
** it is sometimes activated temporarily while debugging code responsible 
** for setting pointer-map entries.
** 如果函数对SQLite操作内有任何帮助.只是当调试代码设置pointer-map条目时暂时激活。
*/
static int ptrmapCheckPages(MemPage **apPage, int nPage){
  int i, j;
  for(i=0; i<nPage; i++){
    Pgno n;
    u8 e;
    MemPage *pPage = apPage[i];
    BtShared *pBt = pPage->pBt;
    assert( pPage->isInit );

    for(j=0; j<pPage->nCell; j++){
      CellInfo info;
      u8 *z;
     
      z = findCell(pPage, j);
      btreeParseCellPtr(pPage, z, &info);
      if( info.iOverflow ){
        Pgno ovfl = get4byte(&z[info.iOverflow]);
        ptrmapGet(pBt, ovfl, &e, &n);
        assert( n==pPage->pgno && e==PTRMAP_OVERFLOW1 );
      }
      if( !pPage->leaf ){
        Pgno child = get4byte(z);
        ptrmapGet(pBt, child, &e, &n);
        assert( n==pPage->pgno && e==PTRMAP_BTREE );
      }
    }
    if( !pPage->leaf ){
      Pgno child = get4byte(&pPage->aData[pPage->hdrOffset+8]);
      ptrmapGet(pBt, child, &e, &n);
      assert( n==pPage->pgno && e==PTRMAP_BTREE );
    }
  }
  return 1;
}
#endif

/*
** This function is used to copy the contents of the b-tree node stored 
** on page pFrom to page pTo. If page pFrom was not a leaf page, then
** the pointer-map entries for each child page are updated so that the
** parent page stored in the pointer map is page pTo. If pFrom contained
** any cells with overflow page pointers, then the corresponding pointer
** map entries are also updated so that the parent page is page pTo.
**
** If pFrom is currently carrying any overflow cells (entries in the
** MemPage.apOvfl[] array), they are not copied to pTo. 
**
** Before returning, page pTo is reinitialized using btreeInitPage().
**
** The performance of this function is not critical. It is only used by 
** the balance_shallower() and balance_deeper() procedures, neither of
** which are called often under normal circumstances.
*/
	/*
	**此函数用于复制pFrom页上b树节点的存储内容到pTo页。如果页面pFrom不是叶子页，然后
	每个子页指针的映射条目进行更新，以便于存储在指针位图中的父节点是pTo页.如果pFrom包
	含任何带有溢出页指针的单元,那么相应的指针位图也更新使父节点是 pTo页。
	如果pFrom目前有任何溢出单元(在MemPage.apOvfl[]数组中的条目),那么它们没有被复制到pTo。
	返回之前，页面pTo需要用btreeInitPage()重新初始化.此功能的性能不是关键。
	 它仅由balance_shallower（）和balance_deeper（）程序使用。通常情况下，两者都不用。
	*/

static void copyNodeContent(MemPage *pFrom, MemPage *pTo, int *pRC){   //复制pFrom页上b树节点的存储内容到pTo页
  if( (*pRC)==SQLITE_OK ){
    BtShared * const pBt = pFrom->pBt;
    u8 * const aFrom = pFrom->aData;
    u8 * const aTo = pTo->aData;
    int const iFromHdr = pFrom->hdrOffset;
    int const iToHdr = ((pTo->pgno==1) ? 100 : 0);
    int rc;
    int iData;
  
  
    assert( pFrom->isInit );
    assert( pFrom->nFree>=iToHdr );
    assert( get2byte(&aFrom[iFromHdr+5]) <= (int)pBt->usableSize );
  
    /* Copy the b-tree node content from page pFrom to page pTo. */  //从pFrom页拷贝B树节点内容到pTo页
    iData = get2byte(&aFrom[iFromHdr+5]);
    memcpy(&aTo[iData], &aFrom[iData], pBt->usableSize-iData);
    memcpy(&aTo[iToHdr], &aFrom[iFromHdr], pFrom->cellOffset + 2*pFrom->nCell);
  
    /* Reinitialize page pTo so that the contents of the MemPage structure
    ** match the new data. The initialization of pTo can actually fail under
    ** fairly obscure circumstances, even though it is a copy of initialized 
    ** page pFrom.
	** 重新初始化页pTo来使MemPage结构的内容和新的数据匹配。相当模糊的情况下，pTo的初始化可能失败,
	** 即使它是一个已经初始化pFrom页的副本。
    */
    pTo->isInit = 0;
    rc = btreeInitPage(pTo);   //初始化页pTo
    if( rc!=SQLITE_OK ){
      *pRC = rc;
      return;
    }
    /* If this is an auto-vacuum database, update the pointer-map entries
    ** for any b-tree or overflow pages that pTo now contains the pointers to.
    ** 如果这是一个自动清理的数据库，那么对于pTo的指针指向的任何B树或溢出页面更新指针位图条目.*/
    if( ISAUTOVACUUM ){
      *pRC = setChildPtrmaps(pTo);
    }
  }
}

/*
** This routine redistributes cells on the iParentIdx'th child of pParent
** (hereafter "the page") and up to 2 siblings so that all pages have about the
** same amount of free space. Usually a single sibling on either side of the
** page are used in the balancing, though both siblings might come from one
** side if the page is the first or last child of its parent. If the page 
** has fewer than 2 siblings (something which can only happen if the page
** is a root page or a child of a root page) then all available siblings
** participate in the balancing.
** 这个函数在 pParent的第iParentIdx孩子上重新分配单元(以下简称“页面”)和达到2个兄弟节点,
** 这样对所有页面都有相同数量的自由空间。通常在页面两侧的一个兄弟节点是平衡的,
** 如果页面的父节点是第一个或最后一个孩子则兄弟节点可能来自一侧。如果页面已经少于2兄弟
** (如果页面是一个根或根的子页面，有些移动席可能唯一发生)然后所有可用的兄弟姐妹参与平衡。
** The number of siblings of the page might be increased or decreased by 
** one or two in an effort to keep pages nearly full but not over full. 
** 页面的兄弟的数量可能会增加或减少一个或两个，尽量保持页面几乎填满但不完全为满。
** Note that when this routine is called, some of the cells on the page
** might not actually be stored in MemPage.aData[]. This can happen
** if the page is overfull. This routine ensures that all cells allocated
** to the page and its siblings fit into MemPage.aData[] before returning.
** 注意,当调用这个函数,在页面上的一些单元可能不完全是存储在MemPage.aData[]。如果页面过满
** 可能会发生这种情况。这个函数分配给页面的所有单元及返回之前其兄弟会写入MemPage.aData[]。
** In the course of balancing the page and its siblings, cells may be
** inserted into or removed from the parent page (pParent). Doing so
** may cause the parent page to become overfull or underfull. If this
** happens, it is the responsibility of the caller to invoke the correct
** balancing routine to fix this problem (see the balance() routine). 
** 在平衡页面和它的兄弟过程中,单元可能插入或从父页面(pParent)删除。这样做可能导致父页面过度满。
** 如果这发生了,调用函数负责调用正确的平衡函数来解决这个问题(见balance()函数)。
** If this routine fails for any reason, it might leave the database
** in a corrupted state. So if this routine fails, the database should
** be rolled back.
** 如果这个函数失败,它可能使数据库在一个损坏的状态。因此如果这个函数失败,数据库应该回滚。
** The third argument to this function, aOvflSpace, is a pointer to a
** buffer big enough to hold one page. If while inserting cells into the parent
** page (pParent) the parent page becomes overfull, this buffer is
** used to store the parent's overflow cells. Because this function inserts
** a maximum of four divider cells into the parent page, and the maximum
** size of a cell stored within an internal node is always less than 1/4
** of the page-size, the aOvflSpace[] buffer is guaranteed to be large
** enough for all overflow cells.
** 这个函数的第三个参数aOvflSpace是一个指针，指向一个足够存放页的缓冲区。如果单元正
** 插入父页面(pParent)，该父页面变得过度满,那么这个缓冲区用于存储父页面的溢出单元。
** 因为这个函数最多插入四个独立的单元进入父页面,并且存储在一个内部节点中的单元的最大值
** 总是小于1/4的页面大小,这个aOvflSpace[]缓冲区是保证溢出单元足够大。
** If aOvflSpace is set to a null pointer, this function returns SQLITE_NOMEM.
** 如果aOvflSpace没有设定指针，则函数返回SQLITE_NOMEM.
*/
/*
这个程序重新分配单元格到兄弟节点。
*/

#if defined(_MSC_VER) && _MSC_VER >= 1700 && defined(_M_ARM)
#pragma optimize("", off)
#endif
static int balance_nonroot(                                //调整B树的各节点使之保持平衡
  MemPage *pParent,               /* Parent page of siblings being balanced */      //要平衡的兄弟节点的父页面
  int iParentIdx,                 /* Index of "the page" in pParent */              //pParent叶面中页索引
  u8 *aOvflSpace,                 /* page-size bytes of space for parent ovfl */    //双亲叶面的空间大小字节
  int isRoot,                     /* True if pParent is a root-page */              //如果pParent是根页面则为true
  int bBulk                       /* True if this call is part of a bulk load */    //这个调用是块负载的一部分则为true
){
  BtShared *pBt;               /* The whole database */                             //整个数据库
  int nCell = 0;               /* Number of cells in apCell[] */                    //apCell[]中的单元数
  int nMaxCells = 0;           /* Allocated size of apCell, szCell, aFrom. */       //分配给apCell, szCell, aFrom的大小
  int nNew = 0;                /* Number of pages in apNew[] */                     //apNew[]中页的数量
  int nOld;                    /* Number of pages in apOld[] */                     //apOld[]中也得数量
  int i, j, k;                 /* Loop counters */                                  //循环中的变量
  int nxDiv;                   /* Next divider slot in pParent->aCell[] */          //pParent->aCell[]中的下一个分割位置
  int rc = SQLITE_OK;          /* The return code */                                //返回代码
  u16 leafCorrection;          /* 4 if pPage is a leaf.  0 if not */                //如果是叶子节点该值为4，否则为0
  int leafData;                /* True if pPage is a leaf of a LEAFDATA tree */     //如果pPage是LEAFDATA树的叶子节点则为true
  int usableSpace;             /* Bytes in pPage beyond the header */               //pPage中头部后面的字节数，可用空间
  int pageFlags;               /* Value of pPage->aData[0] */                       //pPage->aData[0]的值
  int subtotal;                /* Subtotal of bytes in cells on one page */         //一个页上的单元中的字节数
  int iSpace1 = 0;             /* First unused byte of aSpace1[] */                 // aSpace1[]中第一个不可用字节
  int iOvflSpace = 0;          /* First unused byte of aOvflSpace[] */              //aOvflSpace[]中的不可用字节
  int szScratch;               /* Size of scratch memory requested */               //暂存器需要的大小
  MemPage *apOld[NB];          /* pPage and up to two siblings */                   //pPage并达到两个字节
  MemPage *apCopy[NB];         /* Private copies of apOld[] pages */                //apOld[]的私有副本
  MemPage *apNew[NB+2];        /* pPage and up to NB siblings after balancing */    //平衡后的pPage和NB个兄弟
  u8 *pRight;                  /* Location in parent of right-sibling pointer */    //有兄弟指针的父节点位置
  u8 *apDiv[NB-1];             /* Divider cells in pParent */                       //pParent中的分离的单元
  int cntNew[NB+2];            /* Index in aCell[] of cell after i-th page */       //第i个页面后单元的aCell[]中的索引
  int szNew[NB+2];             /* Combined size of cells place on i-th page */      //第i个页面上的单元的总大小
  u8 **apCell = 0;             /* All cells begin balanced */                       //开始时保持平衡的单元数
  u16 *szCell;                 /* Local size of all cells in apCell[] */            //apCell[]中的所有单元的本地大小
  u8 *aSpace1;                 /* Space for copies of dividers cells */             //分离单元的副本空间
  Pgno pgno;                   /* Temp var to store a page number in */             //在其中存储页码的

  pBt = pParent->pBt;
  assert( sqlite3_mutex_held(pBt->mutex) );
  assert( sqlite3PagerIswriteable(pParent->pDbPage) );

#if 0
  TRACE(("BALANCE: begin page %d child of %d\n", pPage->pgno, pParent->pgno));
#endif

  /* At this point pParent may have at most one overflow cell. And if
  ** this overflow cell is present, it must be the cell with 
  ** index iParentIdx. This scenario comes about when this function
  ** is called (indirectly) from sqlite3BtreeDelete().
  ** 此时pParent可能最多一个溢出单元。如果这中溢出单元出现,他一定是带有iParentIdx索引的。
  ** 这个场景是这个函数被sqlite3BtreeDelete()调用(间接)。
  */
  assert( pParent->nOverflow==0 || pParent->nOverflow==1 );
  assert( pParent->nOverflow==0 || pParent->aiOvfl[0]==iParentIdx );

  if( !aOvflSpace ){
    return SQLITE_NOMEM;
  }

  /* Find the sibling pages to balance. Also locate the cells in pParent 
  ** that divide the siblings. An attempt is made to find NN siblings on 
  ** either side of pPage. More siblings are taken from one side, however, 
  ** if there are fewer than NN siblings on the other side. If pParent
  ** has NB or fewer children then all children of pParent are taken.  
  ** 找到要平衡的兄弟页面.还确定pParent中分开兄弟单元的位置。试图
  ** 找到pPage两侧的NN兄弟。然而,一侧有更多的兄弟节点那么有少于
  ** NN的兄弟在另一边。如果pParent有NB或更少孩子那么pParent的孩子占据。
  ** This loop also drops the divider cells from the parent page. This
  ** way, the remainder of the function does not have to deal with any
  ** overflow cells in the parent page, since if any existed they will
  ** have already been removed.
  ** 这个循环也从父页面删除分离的单元。这样函数的其余部分不需要处理任何在
  **父页面中溢出的单元,因为如果任何存在的都已经被移除。
  */
  /*找到兄弟页以达到平衡。*/
  i = pParent->nOverflow + pParent->nCell;
  if( i<2 ){
    nxDiv = 0;
  }else{
    assert( bBulk==0 || bBulk==1 );
    if( iParentIdx==0 ){                 
      nxDiv = 0;
    }else if( iParentIdx==i ){
      nxDiv = i-2+bBulk;
    }else{
      assert( bBulk==0 );
      nxDiv = iParentIdx-1;
    }
    i = 2-bBulk;
  }
  nOld = i+1;
  if( (i+nxDiv-pParent->nOverflow)==pParent->nCell ){
    pRight = &pParent->aData[pParent->hdrOffset+8];
  }else{
    pRight = findCell(pParent, i+nxDiv-pParent->nOverflow);
  }
  pgno = get4byte(pRight);
  while( 1 ){
    rc = getAndInitPage(pBt, pgno, &apOld[i]);
    if( rc ){
      memset(apOld, 0, (i+1)*sizeof(MemPage*));
      goto balance_cleanup;
    }
    nMaxCells += 1+apOld[i]->nCell+apOld[i]->nOverflow;
    if( (i--)==0 ) break;

    if( i+nxDiv==pParent->aiOvfl[0] && pParent->nOverflow ){
      apDiv[i] = pParent->apOvfl[0];
      pgno = get4byte(apDiv[i]);
      szNew[i] = cellSizePtr(pParent, apDiv[i]);
      pParent->nOverflow = 0;
    }else{
      apDiv[i] = findCell(pParent, i+nxDiv-pParent->nOverflow);
      pgno = get4byte(apDiv[i]);
      szNew[i] = cellSizePtr(pParent, apDiv[i]);

      /* Drop the cell from the parent page. apDiv[i] still points to
      ** the cell within the parent, even though it has been dropped.
      ** This is safe because dropping a cell only overwrites the first
      ** four bytes of it, and this function does not need the first
      ** four bytes of the divider cell. So the pointer is safe to use
      ** later on.  
      ** 从父页面删除单元。apDiv[i]仍然指向父节点内的单元,即使它已经删除。这是安全的,因为单元
	  ** 仅覆盖它的开始的4个字节,该函数不需要其他单元的开始的四个字节。指针可以安全地随后使用。
      ** But not if we are in secure-delete mode. In secure-delete mode,
      ** the dropCell() routine will overwrite the entire cell with zeroes.
      ** In this case, temporarily copy the cell into the aOvflSpace[]
      ** buffer. It will be copied out again as soon as the aSpace[] buffer
      ** is allocated. 
	  ** 但除此之外需要安全删除模式.在安全删除模式下,  dropCell()函数将用0覆盖整个单元.在这
	  ** 种情况下,临时备份单元到aOvflSpace[]缓冲区.一旦aSpace[]缓冲区被分配它将被复制出来。
	  */
      if( pBt->btsFlags & BTS_SECURE_DELETE ){
        int iOff;

        iOff = SQLITE_PTR_TO_INT(apDiv[i]) - SQLITE_PTR_TO_INT(pParent->aData);
        if( (iOff+szNew[i])>(int)pBt->usableSize ){
          rc = SQLITE_CORRUPT_BKPT;
          memset(apOld, 0, (i+1)*sizeof(MemPage*));
          goto balance_cleanup;
        }else{
          memcpy(&aOvflSpace[iOff], apDiv[i], szNew[i]);
          apDiv[i] = &aOvflSpace[apDiv[i]-pParent->aData];
        }
      }
      dropCell(pParent, i+nxDiv-pParent->nOverflow, szNew[i], &rc);
    }
  }

  /* Make nMaxCells a multiple of 4 in order to preserve 8-byte alignment */
  //使nMaxCells为4的倍数为了保持8字节的对齐.
  nMaxCells = (nMaxCells + 3)&~3;

  /* Allocate space for memory structures */                //为内存结构分配空间
  k = pBt->pageSize + ROUND8(sizeof(MemPage));
  szScratch =
       nMaxCells*sizeof(u8*)                       /* apCell */               //开始时保持平衡的单元数
     + nMaxCells*sizeof(u16)                       /* szCell */               //apCell[]中的所有单元的本地大小
     + pBt->pageSize                               /* aSpace1 */              //分离单元的副本空间
     + k*nOld;                                     /* Page copies (apCopy) */ //页副本
  apCell = sqlite3ScratchMalloc( szScratch ); 
  if( apCell==0 ){
    rc = SQLITE_NOMEM;
    goto balance_cleanup;
  }
  szCell = (u16*)&apCell[nMaxCells];
  aSpace1 = (u8*)&szCell[nMaxCells];
  assert( EIGHT_BYTE_ALIGNMENT(aSpace1) );

  /*
  ** Load pointers to all cells on sibling pages and the divider cells
  ** into the local apCell[] array.  Make copies of the divider cells
  ** into space obtained from aSpace1[] and remove the divider cells
  ** from pParent.
  ** 加载指针兄弟页面上的所有单元和分离单元到本地apCell[]数组。复制分分离的单元的
  ** 副本进入从aSpace1[]获得的空间并从pParent删除分离的单元。
  ** If the siblings are on leaf pages, then the child pointers of the
  ** divider cells are stripped from the cells before they are copied
  ** into aSpace1[].  In this way, all cells in apCell[] are without
  ** child pointers.  If siblings are not leaves, then all cell in
  ** apCell[] include child pointers.  Either way, all cells in apCell[]
  ** are alike.
  ** 如果兄弟在叶页面,那么分离单元的孩子指针在它们被拷贝进入aSpace1[]之前从单元上移除.通过这种方式,在apCell[]中的所有的
  ** 单元都没有孩子指针.如果兄弟不是叶子,那么apCell[]中的所有单元都有孩子指针.无论如何,在apCell[]种的所有的单元都是一样的.
  ** leafCorrection:  4 if pPage is a leaf.  0 if pPage is not a leaf.
  ** leafData:  1 if pPage holds key+data and pParent holds only keys.
  ** leafCorrection:如果pPage是叶子，为4，否则为0.    leafData:若pPage有Key和data并且pParent仅有key那么为1.
  */
  leafCorrection = apOld[0]->leaf*4;
  leafData = apOld[0]->hasData;
  for(i=0; i<nOld; i++){
    int limit;
    
    /* Before doing anything else, take a copy of the i'th original sibling
    ** The rest of this function will use data from the copies rather
    ** that the original pages since the original pages will be in the
    ** process of being overwritten. 
	** 在做任何其他操作之前,复制原来的第i个兄弟.这个函数的其余部分将使用来自副本的数据，
	** 而不是源实业的数据,原始页面将在被被覆盖的进程中。
	*/
    MemPage *pOld = apCopy[i] = (MemPage*)&aSpace1[pBt->pageSize + k*i];
    memcpy(pOld, apOld[i], sizeof(MemPage));
    pOld->aData = (void*)&pOld[1];
    memcpy(pOld->aData, apOld[i]->aData, pBt->pageSize);

    limit = pOld->nCell+pOld->nOverflow;
    if( pOld->nOverflow>0 ){
      for(j=0; j<limit; j++){
        assert( nCell<nMaxCells );
        apCell[nCell] = findOverflowCell(pOld, j);
        szCell[nCell] = cellSizePtr(pOld, apCell[nCell]);
        nCell++;
      }
    }else{
      u8 *aData = pOld->aData;
      u16 maskPage = pOld->maskPage;
      u16 cellOffset = pOld->cellOffset;
      for(j=0; j<limit; j++){
        assert( nCell<nMaxCells );
        apCell[nCell] = findCellv2(aData, maskPage, cellOffset, j);
        szCell[nCell] = cellSizePtr(pOld, apCell[nCell]);
        nCell++;
      }
    }       
    if( i<nOld-1 && !leafData){
      u16 sz = (u16)szNew[i];
      u8 *pTemp;
      assert( nCell<nMaxCells );
      szCell[nCell] = sz;
      pTemp = &aSpace1[iSpace1];
      iSpace1 += sz;
      assert( sz<=pBt->maxLocal+23 );
      assert( iSpace1 <= (int)pBt->pageSize );
      memcpy(pTemp, apDiv[i], sz);
      apCell[nCell] = pTemp+leafCorrection;
      assert( leafCorrection==0 || leafCorrection==4 );
      szCell[nCell] = szCell[nCell] - leafCorrection;
      if( !pOld->leaf ){
        assert( leafCorrection==0 );
        assert( pOld->hdrOffset==0 );
        /* The right pointer of the child page pOld becomes the left
        ** pointer of the divider cell 
		** 孩子页面pOld的右指针变成分离单元的左指针*/
        memcpy(apCell[nCell], &pOld->aData[8], 4);
      }else{
        assert( leafCorrection==4 );
        if( szCell[nCell]<4 ){
          /* Do not allow any cells smaller than 4 bytes. */  //不允许任何单元小于4个字节
          szCell[nCell] = 4;
        }
      }
      nCell++;
    }
  }

  /*
  ** Figure out the number of pages needed to hold all nCell cells.
  ** Store this number in "k".  Also compute szNew[] which is the total
  ** size of all cells on the i-th page and cntNew[] which is the index
  ** in apCell[] of the cell that divides page i from page i+1.  
  ** cntNew[k] should equal nCell.
  ** 计算出需要保存所有 nCell 单元的数量，将值付给k.也计算szNew它是在第i个页上但单元的总大小，
  ** 以及cntNew[]它是在分开第i个和第i+1个页的单元的apCell[]中的索引。
  ** Values computed by this block:      //通过这个块计算值
  **
  **           k: The total number of sibling pages                                   //k：兄弟也的总数
  **    szNew[i]: Spaced used on the i-th sibling page.                        //szNew[i]：第i个兄弟页使用的空间大小
  **   cntNew[i]: Index in apCell[] and szCell[] for the first cell to     //cntNew[i]：在apCell[] 和szCell[]中第i个兄弟页的右侧第一个单元的索引
  **              the right of the i-th sibling page.
  ** usableSpace: Number of bytes of space available on each sibling.   //usableSpace:每个兄弟页上可用的空间字节大小
  */
  usableSpace = pBt->usableSize - 12 + leafCorrection;
  for(subtotal=k=i=0; i<nCell; i++){
    assert( i<nMaxCells );
    subtotal += szCell[i] + 2;
    if( subtotal > usableSpace ){
      szNew[k] = subtotal - szCell[i];
      cntNew[k] = i;
      if( leafData ){ i--; }
      subtotal = 0;
      k++;
      if( k>NB+1 ){ rc = SQLITE_CORRUPT_BKPT; goto balance_cleanup; }
    }
  }
  szNew[k] = subtotal;
  cntNew[k] = nCell;
  k++;

  /*
  ** The packing computed by the previous block is biased toward the siblings
  ** on the left side.  The left siblings are always nearly full, while the
  ** right-most sibling might be nearly empty.  This block of code attempts
  ** to adjust the packing of siblings to get a better balance.
  ** 通过前一块打包计算，偏向左兄弟节点。左兄弟总是装得尽可能满,而最右侧兄弟近可能空。
  ** 这段代码的尝试调整使得兄弟节点更好地保持平衡。
  ** This adjustment is more than an optimization.  The packing above might
  ** be so out of balance as to be illegal.  For example, the right-most
  ** sibling might be completely empty.  This adjustment is not optional.
  ** 这种调整更优化。上面的包装可能会是失去平衡,因此是非法的。
  ** 例如,最右边兄弟可能完全是空的，此时这种调整不可选。
  */
  for(i=k-1; i>0; i--){
    int szRight = szNew[i];  /* Size of sibling on the right */                 //右兄弟的大小
    int szLeft = szNew[i-1]; /* Size of sibling on the left */                  //左兄弟的大小
    int r;              /* Index of right-most cell in left sibling */          //左兄弟中最右单元的索引
    int d;              /* Index of first cell to the left of right sibling */  //右兄弟最左侧第一个单元的索引

    r = cntNew[i-1] - 1;
    d = r + 1 - leafData;
    assert( d<nMaxCells );
    assert( r<nMaxCells );
    while( szRight==0 
       || (!bBulk && szRight+szCell[d]+2<=szLeft-(szCell[r]+2)) 
    ){
      szRight += szCell[d] + 2;
      szLeft -= szCell[r] + 2;
      cntNew[i-1]--;
      r = cntNew[i-1] - 1;
      d = r + 1 - leafData;
    }
    szNew[i] = szRight;
    szNew[i-1] = szLeft;
  }

  /* Either we found one or more cells (cntnew[0])>0) or pPage is
  ** a virtual root page.  A virtual root page is when the real root
  ** page is page 1 and we are the only child of that page.
  ** 我们发现一个或更多(cntnew[0])> 0)或pPage是一个虚拟根页面。
  ** 一个虚拟的根页是当真正的根页是第1页的时候和那个页面是唯一的孩子。
  ** UPDATE:  The assert() below is not necessarily true if the database
  ** file is corrupt.  The corruption will be detected and reported later
  ** in this procedure so there is no need to act upon it now.
  */
#if 0
  assert( cntNew[0]>0 || (pParent->pgno==1 && pParent->nCell==0) );
#endif

  TRACE(("BALANCE: old: %d %d %d  ",
    apOld[0]->pgno, 
    nOld>=2 ? apOld[1]->pgno : 0,
    nOld>=3 ? apOld[2]->pgno : 0
  ));

  /*Allocate k new pages.  Reuse old pages where possible. */     //分配k新页。有可能重新使用老页
  if( apOld[0]->pgno<=1 ){
    rc = SQLITE_CORRUPT_BKPT;
    goto balance_cleanup;
  }
  pageFlags = apOld[0]->aData[0];
  for(i=0; i<k; i++){
    MemPage *pNew;
    if( i<nOld ){
      pNew = apNew[i] = apOld[i];
      apOld[i] = 0;
      rc = sqlite3PagerWrite(pNew->pDbPage);
      nNew++;
      if( rc ) goto balance_cleanup;
    }else{
      assert( i>0 );
      rc = allocateBtreePage(pBt, &pNew, &pgno, (bBulk ? 1 : pgno), 0);
      if( rc ) goto balance_cleanup;
      apNew[i] = pNew;
      nNew++;

      /* Set the pointer-map entry for the new sibling page. */  //对于新的兄弟页设置指针位图条目
      if( ISAUTOVACUUM ){
        ptrmapPut(pBt, pNew->pgno, PTRMAP_BTREE, pParent->pgno, &rc);
        if( rc!=SQLITE_OK ){
          goto balance_cleanup;
        }
      }
    }
  }

  /* Free any old pages that were not reused as new pages.*/    //释放没有重新使用作新页的老页
  while( i<nOld ){
    freePage(apOld[i], &rc);
    if( rc ) goto balance_cleanup;
    releasePage(apOld[i]);
    apOld[i] = 0;
    i++;
  }

  /*
  ** Put the new pages in accending order.  This helps to
  ** keep entries in the disk file in order so that a scan
  ** of the table is a linear scan through the file.  That
  ** in turn helps the operating system to deliver pages
  ** from the disk more rapidly.
  ** 使新页递增有序。这有助于保持磁盘文件中的条目顺序以便于对表的进行
  ** 一个线性扫描整个文件.反过来帮助操作系统从磁盘更快的提供页面。
  ** An O(n^2) insertion sort algorithm is used, but since
  ** n is never more than NB (a small constant), that should
  ** not be a problem.
  ** 用一个复杂度为O(n^2)的插入排序算法,但是n不会超过NB，应该不是问题
  ** When NB==3, this one optimization makes the database
  ** about 25% faster for large insertions and deletions.
  ** 当NB==3,这个优化使数据库对于删除插入提高大约25%左右。
  */
  for(i=0; i<k-1; i++){
    int minV = apNew[i]->pgno;
    int minI = i;
    for(j=i+1; j<k; j++){
      if( apNew[j]->pgno<(unsigned)minV ){
        minI = j;
        minV = apNew[j]->pgno;
      }
    }
    if( minI>i ){
      MemPage *pT;
      pT = apNew[i];
      apNew[i] = apNew[minI];
      apNew[minI] = pT;
    }
  }
  TRACE(("new: %d(%d) %d(%d) %d(%d) %d(%d) %d(%d)\n",
    apNew[0]->pgno, szNew[0],
    nNew>=2 ? apNew[1]->pgno : 0, nNew>=2 ? szNew[1] : 0,
    nNew>=3 ? apNew[2]->pgno : 0, nNew>=3 ? szNew[2] : 0,
    nNew>=4 ? apNew[3]->pgno : 0, nNew>=4 ? szNew[3] : 0,
    nNew>=5 ? apNew[4]->pgno : 0, nNew>=5 ? szNew[4] : 0));

  assert( sqlite3PagerIswriteable(pParent->pDbPage) );
  put4byte(pRight, apNew[nNew-1]->pgno);

  /*
  ** Evenly distribute the data in apCell[] across the new pages.
  ** Insert divider cells into pParent as necessary.
  ** 在新的页面的apCell[]中均匀分布数据。插入分隔单元pParent是必要的。
  */
  j = 0;
  for(i=0; i<nNew; i++){
    /* Assemble the new sibling page. */     //组装新兄弟页
    MemPage *pNew = apNew[i];
    assert( j<nMaxCells );
    zeroPage(pNew, pageFlags);
    assemblePage(pNew, cntNew[i]-j, &apCell[j], &szCell[j]);
    assert( pNew->nCell>0 || (nNew==1 && cntNew[0]==0) );
    assert( pNew->nOverflow==0 );

    j = cntNew[i];

    /* If the sibling page assembled above was not the right-most sibling,
    ** insert a divider cell into the parent page. 
	** 如果上面组装的兄弟页面并不是最右边的兄弟,插入隔离单元到父页面。
    */
    assert( i<nNew-1 || j==nCell );
    if( j<nCell ){
      u8 *pCell;
      u8 *pTemp;
      int sz;

      assert( j<nMaxCells );
      pCell = apCell[j];
      sz = szCell[j] + leafCorrection;
      pTemp = &aOvflSpace[iOvflSpace];
      if( !pNew->leaf ){
        memcpy(&pNew->aData[8], pCell, 4);
      }else if( leafData ){
        /* If the tree is a leaf-data tree, and the siblings are leaves, 
        ** then there is no divider cell in apCell[]. Instead, the divider 
        ** cell consists of the integer key for the right-most cell of 
        ** the sibling-page assembled above only.
		** 如果是叶数据的树，并且各节点是叶节点，那么在APCell[]中没有分割单元。
		** 相反分割单元是以上装配的兄弟节点的最右的单元的整形关键字组成。
        */
        CellInfo info;
        j--;
        btreeParseCellPtr(pNew, apCell[j], &info);
        pCell = pTemp;
        sz = 4 + putVarint(&pCell[4], info.nKey);
        pTemp = 0;
      }else{
        pCell -= 4;
        /* Obscure case for non-leaf-data trees: If the cell at pCell was
        ** previously stored on a leaf node, and its reported size was 4
        ** bytes, then it may actually be smaller than this 
        ** (see btreeParseCellPtr(), 4 bytes is the minimum size of
        ** any cell). But it is important to pass the correct size to 
        ** insertCell(), so reparse the cell now.
        ** 另种情况是non-leaf-data树:如果在pCell的单元是以前存储在一个叶节点上的,并且是
		** 4字节大小,事实上它可能会比这个小(见btreeParseCellPtr(),4个字节是任何单元的最小值)。
		** 但重要的是通过正确的大小insertCell(),所以现在重新解析单元。
        ** Note that this can never happen in an SQLite data file, as all
        ** cells are at least 4 bytes. It only happens in b-trees used
        ** to evaluate "IN (SELECT ...)" and similar clauses.
		** 注意,这可能不会发生在一个SQLite数据文件中,所有单元至少有4个字节。
		** 它只发生在b树中用来评估"IN (SELECT ...)"和相关子句。
        */
        if( szCell[j]==4 ){
          assert(leafCorrection==4);
          sz = cellSizePtr(pParent, pCell);
        }
      }
      iOvflSpace += sz;
      assert( sz<=pBt->maxLocal+23 );
      assert( iOvflSpace <= (int)pBt->pageSize );
      insertCell(pParent, nxDiv, pCell, sz, pTemp, pNew->pgno, &rc);
      if( rc!=SQLITE_OK ) goto balance_cleanup;
      assert( sqlite3PagerIswriteable(pParent->pDbPage) );

      j++;
      nxDiv++;
    }
  }
  assert( j==nCell );
  assert( nOld>0 );
  assert( nNew>0 );
  if( (pageFlags & PTF_LEAF)==0 ){
    u8 *zChild = &apCopy[nOld-1]->aData[8];
    memcpy(&apNew[nNew-1]->aData[8], zChild, 4);
  }

  if( isRoot && pParent->nCell==0 && pParent->hdrOffset<=apNew[0]->nFree ){
    /* The root page of the b-tree now contains no cells. The only sibling
    ** page is the right-child of the parent. Copy the contents of the
    ** child page into the parent, decreasing the overall height of the
    ** b-tree structure by one. This is described as the "balance-shallower"
    ** sub-algorithm in some documentation.
    ** B树的根页现在不含单元。唯一的兄弟页面是右孩子的父节点。拷贝孩子页面的内容
	** 到父页面,减少B树结构的整体高度.在一些文档中被描述为“balance-shallower” 子算法。
    ** If this is an auto-vacuum database, the call to copyNodeContent() 
    ** sets all pointer-map entries corresponding to database image pages 
    ** for which the pointer is stored within the content being copied.
    ** 如果是一个自动清理的数据库,调用copyNodeContent() 设定所有pointer-map条目与数据
	** 库镜像页对应，指针存储在被拷贝的内容中。
    ** The second assert below verifies that the child page is defragmented 
    ** (it must be, as it was just reconstructed using assemblePage()). This
    ** is important if the parent page happens to be page 1 of the database
    ** image.  
	** 第二个断言验证子页面被碎片化了(这是必须的,因为它只是使用assemblePage()重建).
	** 这是很重要的,如果父页面是数据库镜像的page 1。*/
    assert( nNew==1 );
    assert( apNew[0]->nFree == 
        (get2byte(&apNew[0]->aData[5])-apNew[0]->cellOffset-apNew[0]->nCell*2) 
    );
    copyNodeContent(apNew[0], pParent, &rc);
    freePage(apNew[0], &rc);
  }else if( ISAUTOVACUUM ){
    /* Fix the pointer-map entries for all the cells that were shifted around. 
    ** There are several different types of pointer-map entries that need to
    ** be dealt with by this routine. Some of these have been set already, but
    ** many have not. The following is a summary:
    ** 对于所有被转移的周围的单元修复pointer-map条目.有几种不同类型的需要pointer-map条目
	**要通过这个函数处理。其中的一些已经设置,但是许多没有设置.下面是总结:
    **   1) The entries associated with new sibling pages that were not
    **      siblings when this function was called. These have already
    **      been set. We don't need to worry about old siblings that were
    **      moved to the free-list - the freePage() code has taken care
    **      of those.
    **      这些条目新的兄弟页面相关,这些兄弟页面当被该函数调用时并不是兄弟节点。
	**      不必担心之前的兄弟节点被移到了空闲列表——freePage() 代码已经考虑到这些。
    **   2) The pointer-map entries associated with the first overflow
    **      page in any overflow chains used by new divider cells. These 
    **      have also already been taken care of by the insertCell() code.
    **     与在任何溢出链表中的第一个溢出页相关的pointer-map条目用作分离单元。
	**     这些也已经在insertCell()代码中考虑。
    **   3) If the sibling pages are not leaves, then the child pages of
    **      cells stored on the sibling pages may need to be updated.
    **      如果兄弟页面不是叶子,那么存储在兄弟页面上的单元的子页面需要更新.
    **   4) If the sibling pages are not internal intkey nodes, then any
    **      overflow pages used by these cells may need to be updated
    **      (internal intkey nodes never contain pointers to overflow pages).
    **      如果兄弟页不是内部intkey节点,那么任何被这些单元使用的溢出页可能需要更新
	**      (内部intkey节点不包含指向溢出页的指针)。
    **   5) If the sibling pages are not leaves, then the pointer-map
    **      entries for the right-child pages of each sibling may need
    **      to be updated.
    **    　如果兄弟页面不是叶子,那么对每个兄弟的右孩子页pointer-map条目可能需要更新。
    ** Cases 1 and 2 are dealt with above by other code. The next
    ** block deals with cases 3 and 4 and the one after that, case 5. Since
    ** setting a pointer map entry is a relatively expensive operation, this
    ** code only sets pointer map entries for child or overflow pages that have
    ** actually moved between pages.  
	** 前两种情况被其他代码处理。下一个块处理情况下3和4,之后5。因为设置一个指针映射条目是一个
	** 相对浪费的操作,所以这个代码只对在页面之间移动的孩子或溢出页设置指针的映射条目。*/
    MemPage *pNew = apNew[0];
    MemPage *pOld = apCopy[0];
    int nOverflow = pOld->nOverflow;
    int iNextOld = pOld->nCell + nOverflow;
    int iOverflow = (nOverflow ? pOld->aiOvfl[0] : -1);
    j = 0;                             /* Current 'old' sibling page */   //当前'old'兄弟页
    k = 0;                             /* Current 'new' sibling page */   //当前'new'兄弟页
    for(i=0; i<nCell; i++){
      int isDivider = 0;
      while( i==iNextOld ){
        /* Cell i is the cell immediately following the last cell on old
        ** sibling page j. If the siblings are not leaf pages of an
        ** intkey b-tree, then cell i was a divider cell. 
		** 单元i是单元立即老兄弟页最后单元后.如果不是intkeyB树的叶子节点那么单元i是一个分割单元*/
        assert( j+1 < ArraySize(apCopy) );
        assert( j+1 < nOld );
        pOld = apCopy[++j];
        iNextOld = i + !leafData + pOld->nCell + pOld->nOverflow;
        if( pOld->nOverflow ){
          nOverflow = pOld->nOverflow;
          iOverflow = i + !leafData + pOld->aiOvfl[0];
        }
        isDivider = !leafData;  
      }

      assert(nOverflow>0 || iOverflow<i );
      assert(nOverflow<2 || pOld->aiOvfl[0]==pOld->aiOvfl[1]-1);
      assert(nOverflow<3 || pOld->aiOvfl[1]==pOld->aiOvfl[2]-1);
      if( i==iOverflow ){
        isDivider = 1;
        if( (--nOverflow)>0 ){
          iOverflow++;
        }
      }

      if( i==cntNew[k] ){
        /* Cell i is the cell immediately following the last cell on new
        ** sibling page k. If the siblings are not leaf pages of an
        ** intkey b-tree, then cell i is a divider cell.  */
        pNew = apNew[++k];
        if( !leafData ) continue;
      }
      assert( j<nOld );
      assert( k<nNew );

      /* If the cell was originally divider cell (and is not now) or
      ** an overflow cell, or if the cell was located on a different sibling
      ** page before the balancing, then the pointer map entries associated
      ** with any child or overflow pages need to be updated.  */
      if( isDivider || pOld->pgno!=pNew->pgno ){
        if( !leafCorrection ){
          ptrmapPut(pBt, get4byte(apCell[i]), PTRMAP_BTREE, pNew->pgno, &rc);
        }
        if( szCell[i]>pNew->minLocal ){
          ptrmapPutOvflPtr(pNew, apCell[i], &rc);
        }
      }
    }

    if( !leafCorrection ){
      for(i=0; i<nNew; i++){
        u32 key = get4byte(&apNew[i]->aData[8]);
        ptrmapPut(pBt, key, PTRMAP_BTREE, apNew[i]->pgno, &rc);
      }
    }

#if 0
    /* The ptrmapCheckPages() contains assert() statements that verify that
    ** all pointer map pages are set correctly. This is helpful while 
    ** debugging. This is usually disabled because a corrupt database may
    ** cause an assert() statement to fail. 
	** ptrmapCheckPages()包含的assert()语句时验证所有的指针映射页正确设定.
	** 这有助于调试.这常不可用，因为一个崩溃的数据库可能造成assert()语句失败.*/
    ptrmapCheckPages(apNew, nNew);
    ptrmapCheckPages(&pParent, 1);
#endif
  }

  assert( pParent->isInit );
  TRACE(("BALANCE: finished: old=%d new=%d cells=%d\n",
          nOld, nNew, nCell));

  /*Cleanup before returning.*/    //返回之前清理
balance_cleanup:
  sqlite3ScratchFree(apCell);
  for(i=0; i<nOld; i++){
    releasePage(apOld[i]);
  }
  for(i=0; i<nNew; i++){
    releasePage(apNew[i]);
  }

  return rc;
}
#if defined(_MSC_VER) && _MSC_VER >= 1700 && defined(_M_ARM)
#pragma optimize("", on)
#endif


/*
** This function is called when the root page of a b-tree structure is
** overfull (has one or more overflow pages).
** 当B树结构的根页过度满时，该函数将被调用。（有一个或多个溢出页）
** A new child page is allocated and the contents of the current root
** page, including overflow cells, are copied into the child. The root
** page is then overwritten to make it an empty page with the right-child 
** pointer pointing to the new page.
** 一个新的孩子页将被分配并且当前根页的内容包括也出单元被拷贝到孩子节点。
** 根页被重写使之为空页，最右孩子指针指向新页。
** Before returning, all pointer-map entries corresponding to pages 
** that the new child-page now contains pointers to are updated. The
** entry corresponding to the new right-child pointer of the root
** page is also updated.
** 在返回之前,所有pointer-map条目对应新子页面包含指针的页面被更新。条目对应的根页的新右子结点指针根也更新。
** If successful, *ppChild is set to contain a reference to the child 
** page and SQLITE_OK is returned. In this case the caller is required
** to call releasePage() on *ppChild exactly once. If an error occurs,
** an error code is returned and *ppChild is set to 0.
** 如果成功,*ppChild将包含一个对孩子页的引用并返回SQLITE_OK。在这种情况下,调用者需要
** 在*ppChild上对releasePage()调用恰好一次。如果出现错误,返回一个错误代码并且ppChild设置为0。
*/
static int balance_deeper(MemPage *pRoot, MemPage **ppChild){            //进一步调整B树的页
  int rc;                        /* Return value from subprocedures */   //子函数的返回值
  MemPage *pChild = 0;           /* Pointer to a new child page */       //新孩子页的指针
  Pgno pgnoChild = 0;            /* Page number of the new child page */ //新孩子页的页码
  BtShared *pBt = pRoot->pBt;    /* The BTree */                         //B树

  assert( pRoot->nOverflow>0 );
  assert( sqlite3_mutex_held(pBt->mutex) );

  /* Make pRoot, the root page of the b-tree, writable. Allocate a new 
  ** page that will become the new right-child of pPage. Copy the contents
  ** of the node stored on pRoot into the new child page.
  ** 使pRoot（B树的根页）可写,分配一个新页使之成为pPage的右孩子.拷贝存储在pRoot上的节点的内容到新的孩子页面。
  */
  rc = sqlite3PagerWrite(pRoot->pDbPage);
  if( rc==SQLITE_OK ){
    rc = allocateBtreePage(pBt,&pChild,&pgnoChild,pRoot->pgno,0);
    copyNodeContent(pRoot, pChild, &rc);
    if( ISAUTOVACUUM ){
      ptrmapPut(pBt, pgnoChild, PTRMAP_BTREE, pRoot->pgno, &rc);
    }
  }
  if( rc ){
    *ppChild = 0;
    releasePage(pChild);
    return rc;
  }
  assert( sqlite3PagerIswriteable(pChild->pDbPage) );
  assert( sqlite3PagerIswriteable(pRoot->pDbPage) );
  assert( pChild->nCell==pRoot->nCell );

  TRACE(("BALANCE: copy root %d into %d\n", pRoot->pgno, pChild->pgno));

  /* Copy the overflow cells from pRoot to pChild */  //将已出单元从pRoot拷贝到pChild
  memcpy(pChild->aiOvfl, pRoot->aiOvfl,
         pRoot->nOverflow*sizeof(pRoot->aiOvfl[0]));
  memcpy(pChild->apOvfl, pRoot->apOvfl,
         pRoot->nOverflow*sizeof(pRoot->apOvfl[0]));
  pChild->nOverflow = pRoot->nOverflow;

  /* Zero the contents of pRoot. Then install pChild as the right-child. */
  //清零pRoot中的内容，将pChild作为右孩子。
  zeroPage(pRoot, pChild->aData[0] & ~PTF_LEAF);
  put4byte(&pRoot->aData[pRoot->hdrOffset+8], pgnoChild);

  *ppChild = pChild;
  return SQLITE_OK;
}

/*
** The page that pCur currently points to has just been modified in
** some way. This function figures out if this modification means the
** tree needs to be balanced, and if so calls the appropriate balancing 
** routine. Balancing routines are:
** 游标pCur当前指向的页面以某种方式被修改。函数将弄明白是否这个修改需要被平衡，
** 是否调用适当的平衡函数。平衡函数如下：
**   balance_quick()
**   balance_deeper()
**   balance_nonroot()
*/
static int balance(BtCursor *pCur){
  int rc = SQLITE_OK;
  const int nMin = pCur->pBt->usableSize * 2 / 3;
  u8 aBalanceQuickSpace[13];
  u8 *pFree = 0;

  TESTONLY( int balance_quick_called = 0 );
  TESTONLY( int balance_deeper_called = 0 );

  do {
    int iPage = pCur->iPage;
    MemPage *pPage = pCur->apPage[iPage];

    if( iPage==0 ){
      if( pPage->nOverflow ){
        /* The root page of the b-tree is overfull. In this case call the
        ** balance_deeper() function to create a new child for the root-page
        ** and copy the current contents of the root-page to it. The
        ** next iteration of the do-loop will balance the child page.
		** B树的根页是过满。在这种情况下,调用balance_deeper()函数为根页创建一个新的孩子
		** 并复制的当前内容根页到该孩子页。下一个迭代循环语句的平衡子页面。
        */ 
        assert( (balance_deeper_called++)==0 );
        rc = balance_deeper(pPage, &pCur->apPage[1]);
        if( rc==SQLITE_OK ){
          pCur->iPage = 1;
          pCur->aiIdx[0] = 0;
          pCur->aiIdx[1] = 0;
          assert( pCur->apPage[1]->nOverflow );
        }
      }else{
        break;
      }
    }else if( pPage->nOverflow==0 && pPage->nFree<=nMin ){
      break;
    }else{
      MemPage * const pParent = pCur->apPage[iPage-1];
      int const iIdx = pCur->aiIdx[iPage-1];

      rc = sqlite3PagerWrite(pParent->pDbPage);
      if( rc==SQLITE_OK ){
#ifndef SQLITE_OMIT_QUICKBALANCE
        if( pPage->hasData
         && pPage->nOverflow==1
         && pPage->aiOvfl[0]==pPage->nCell
         && pParent->pgno!=1
         && pParent->nCell==iIdx
        ){
          /* Call balance_quick() to create a new sibling of pPage on which
          ** to store the overflow cell. balance_quick() inserts a new cell
          ** into pParent, which may cause pParent overflow. If this
          ** happens, the next interation of the do-loop will balance pParent 
          ** use either balance_nonroot() or balance_deeper(). Until this
          ** happens, the overflow cell is stored in the aBalanceQuickSpace[]
          ** buffer. 
          ** 调用balance_quick()来创建一个pPage的新兄弟页，页上存储了溢出单元。balance_quick()插入一个
		  ** 新单元到pParent,这可能会导致pParent溢出.如果这种情况发生,下一个迭代循环语句将用balance_nonroot()
		  ** 或balance_deeper()中的任一个平衡pParent.直到溢出单元存储在aBalanceQuickSpace[]缓冲区。
          ** The purpose of the following assert() is to check that only a
          ** single call to balance_quick() is made for each call to this
          ** function. If this were not verified, a subtle bug involving reuse
          ** of the aBalanceQuickSpace[] might sneak in.
		  ** 下面的assert()的目的是检查,对于每个调用函数只有一个调用balance_quick()。如果这是不验证,
		  ** aBalanceQuickSpace[]重用的时候将发生一个微妙的错误。
          */
          assert( (balance_quick_called++)==0 );
          rc = balance_quick(pParent, pPage, aBalanceQuickSpace);
        }else
#endif
        {
          /* In this case, call balance_nonroot() to redistribute cells
          ** between pPage and up to 2 of its sibling pages. This involves
          ** modifying the contents of pParent, which may cause pParent to
          ** become overfull or underfull. The next iteration of the do-loop
          ** will balance the parent page to correct this.
          ** ** 在这种情况下，调用balance_nonroot()使单元在pPage和其他两个兄弟节点间重分部。这涉及到
		  ** 修改pParent的内容,这可能导致pParent过满或不满。下一次迭代循环语句将平衡父页面。
          ** If the parent page becomes overfull, the overflow cell or cells
          ** are stored in the pSpace buffer allocated immediately below. 
          ** A subsequent iteration of the do-loop will deal with this by
          ** calling balance_nonroot() (balance_deeper() may be called first,
          ** but it doesn't deal with overflow cells - just moves them to a
          ** different page). Once this subsequent call to balance_nonroot() 
          ** has completed, it is safe to release the pSpace buffer used by
          ** the previous call, as the overflow cell data will have been 
          ** copied either into the body of a database page or into the new
          ** pSpace buffer passed to the latter call to balance_nonroot().
		  ** 如果父页面变得过满,溢出单元或存储在pSpace缓冲区的单元将立即分配。
		  ** 后续迭代的循环语句将调用balance_nonroot()来处理(balance_deeper()可能首先被调用,但它不处理
		  ** 溢出单元——只是他们移动到不同的页面)。一旦这个后续调用balance_nonroot()完成,释放之前被使用
		  ** 的pSpace缓冲区将是安全的,此时溢出单元数据将被复制到数据库页面的主体或复制到新的pSpace缓冲区，
		  ** 该实现通过后来调用balance_nonroot()传递。
          */
          u8 *pSpace = sqlite3PageMalloc(pCur->pBt->pageSize);
          rc = balance_nonroot(pParent, iIdx, pSpace, iPage==1, pCur->hints);
          if( pFree ){
            /* If pFree is not NULL, it points to the pSpace buffer used 
            ** by a previous call to balance_nonroot(). Its contents are
            ** now stored either on real database pages or within the 
            ** new pSpace buffer, so it may be safely freed here. 
			** ** 如果pFree不是NULL,它指向之前被balance_nonroot()调用的pSpace缓冲区.它的内容现在存储在实际
			** 数据库页面或新pSpace缓冲区中,所以这里可以安全地释放。*/
            sqlite3PageFree(pFree);
          }

          /* The pSpace buffer will be freed after the next call to
          ** balance_nonroot(), or just before this function returns, whichever
          ** comes first. 
		  ** pSpace缓冲区将在下一次调用时被释放,或在这个函数返回之前。*/
          pFree = pSpace;
        }
      }

      pPage->nOverflow = 0;

      /* The next iteration of the do-loop balances the parent page. */  //下一个迭代循环语句调整父页面平衡
      releasePage(pPage);
      pCur->iPage--;
    }
  }while( rc==SQLITE_OK );

  if( pFree ){
    sqlite3PageFree(pFree);
  }
  return rc;
}


/*
** Insert a new record into the BTree.  The key is given by (pKey,nKey)
** and the data is given by (pData,nData).  The cursor is used only to
** define what table the record should be inserted into.  The cursor
** is left pointing at a random location.
** 插入一条新记录到B树.关键字由(pKey,nKey)给出，数据域有(pData,nData)给出.
** 游标仅仅被用作定义记录应该插入到什么表中，其他游标指向任意位置.
** For an INTKEY table, only the nKey value of the key is used.  pKey is
** ignored.  For a ZERODATA table, the pData and nData are both ignored.
** 对于一个INTKEY表只有关键字的nKey值被用，pKey忽略。对于ZERODATA表pData和nData都不用。
** If the seekResult parameter is non-zero, then a successful call to
** MovetoUnpacked() to seek cursor pCur to (pKey, nKey) has already
** been performed. seekResult is the search result returned (a negative
** number if pCur points at an entry that is smaller than (pKey, nKey), or
** a positive value if pCur points at an etry that is larger than 
** (pKey, nKey)). 
** 如果seekResult参数非零,那么一个成功的调用MovetoUnpacked()寻找(pKey nKey)已经被执行的游标pCur。
** seekResult是搜索返回的结果(如果pCur指向一个比(pKey, nKey)还小的条目则是一个负值,或如果pCur指向
** 一个比(pKey nKey)更大的值则该值为正值.)。
** If the seekResult parameter is non-zero, then the caller guarantees that
** cursor pCur is pointing at the existing copy of a row that is to be
** overwritten.  If the seekResult parameter is 0, then cursor pCur may
** point to any entry or to no entry at all and so this function has to seek
** the cursor before the new key can be inserted.
** 如果seekResult参数是非零,那么调用者保证游标pCur向一行被复写的现有副本。如果seekResult参数是0,那么
** 游标pCur可能指向任何条目或不指向任何条目,所以在插入键值之前这个函数必须寻找游标。
*/
/*
向Btree中插入一个新记录：
关键字按照(pKey,nKey)给定，数据按照(pData,nData)给定。
找到结点插入位置
分配内存空间
插入结点
*/
int sqlite3BtreeInsert(          //插入新记录到B树
  BtCursor *pCur,                /* Insert data into the table of this cursor */  //插入数据到游标指向的表
  const void *pKey, i64 nKey,    /* The key of the new record */                  //新记录的键值
  const void *pData, int nData,  /* The data of the new record */                 //新记录的数据
  int nZero,                     /* Number of extra 0 bytes to append to data */  //附加到数据的额外的0字节数
  int appendBias,                /* True if this is likely an append */           //如果是附加的则为true
  int seekResult                 /* Result of prior MovetoUnpacked() call */      //先前调用MovetoUnpacked()的结果
){
  int rc;
  int loc = seekResult;          /* -1: before desired location  +1: after */     //-1:希望的位置之前  +1:之后
  int szNew = 0;
  int idx;
  MemPage *pPage;
  Btree *p = pCur->pBtree;
  BtShared *pBt = p->pBt;
  unsigned char *oldCell;
  unsigned char *newCell = 0;

  if( pCur->eState==CURSOR_FAULT ){
    assert( pCur->skipNext!=SQLITE_OK );
    return pCur->skipNext;
  }

  assert( cursorHoldsMutex(pCur) );
  assert( pCur->wrFlag && pBt->inTransaction==TRANS_WRITE
              && (pBt->btsFlags & BTS_READ_ONLY)==0 );
  assert( hasSharedCacheTableLock(p, pCur->pgnoRoot, pCur->pKeyInfo!=0, 2) );

  /* Assert that the caller has been consistent. If this cursor was opened
  ** expecting an index b-tree, then the caller should be inserting blob
  ** keys with no associated data. If the cursor was opened expecting an
  ** intkey table, the caller should be inserting integer keys with a
  ** blob of associated data.  
  ** 断言调用者是一致的。如果这个游标被打开的B树索引,那么调用者应该插入没有相关数据
  ** 的blob键。如果游标被开放对于intkey表,调用者应该插入带有相关数据的blob的整数键。*/
  assert( (pKey==0)==(pCur->pKeyInfo==0) );

  /* Save the positions of any other cursors open on this table.
  ** 保存在表上打开的任何其他游标的位置。
  ** In some cases, the call to btreeMoveto() below is a no-op. For
  ** example, when inserting data into a table with auto-generated integer
  ** keys, the VDBE layer invokes sqlite3BtreeLast() to figure out the 
  ** integer key to use. It then calls this function to actually insert the 
  ** data into the intkey B-Tree. In this case btreeMoveto() recognizes
  ** that the cursor is already where it needs to be and returns without
  ** doing any work. To avoid thwarting(妨碍) these optimizations, it is important
  ** not to clear the cursor here.
  ** 在某些情况下,下面对btreeMoveto()的调用是误操作的。例如,当将数据插入自动生成的整数键的表时,
  ** VDBE层调用sqlite3BtreeLast()求出要使用的整数键。然后调用这个函数来插入数据到intkeyB树。在这种情况
  ** 下btreeMoveto()识别游标已经在需要它的地方,并返回。为了避免影响这些优化,不清除这里的游标是很重要的。
  */ 
  rc = saveAllCursors(pBt, pCur->pgnoRoot, pCur);     /*保存所有游标*/
  if( rc ) return rc;

  /* If this is an insert into a table b-tree, invalidate any incrblob 
  ** cursors open on the row being replaced (assuming this is a replace
  ** operation - if it is not, the following is a no-op).  
  ** 如果插入到表B树，使在被替换的行上的任何开放性的递增blob游标.(假设这是一个替换操作,如果不是，则无操作.)*/
  if( pCur->pKeyInfo==0 ){
    invalidateIncrblobCursors(p, nKey, 0);   //使开放的行或行中的一个被修改的一个incrblob游标无效
  }

  if( !loc ){
    rc = btreeMoveto(pCur, pKey, nKey, appendBias, &loc);
    if( rc ) return rc;
  }
  assert( pCur->eState==CURSOR_VALID || (pCur->eState==CURSOR_INVALID && loc) );

  pPage = pCur->apPage[pCur->iPage];
  assert( pPage->intKey || nKey>=0 );
  assert( pPage->leaf || !pPage->intKey );

  TRACE(("INSERT: table=%d nkey=%lld ndata=%d page=%d %s\n",
          pCur->pgnoRoot, nKey, nData, pPage->pgno,
          loc==0 ? "overwrite" : "new entry"));
  assert( pPage->isInit );
  allocateTempSpace(pBt); /*确保pBt指向MX_CELL_SIZE(pBt)字节*/
  newCell = pBt->pTmpSpace;
  if( newCell==0 ) return SQLITE_NOMEM;
  rc = fillInCell(pPage, newCell, pKey, nKey, pData, nData, nZero, &szNew);
  if( rc ) goto end_insert;
  assert( szNew==cellSizePtr(pPage, newCell) );
  assert( szNew <= MX_CELL_SIZE(pBt) );
  idx = pCur->aiIdx[pCur->iPage];
  if( loc==0 ){
    u16 szOld;
    assert( idx<pPage->nCell );
    rc = sqlite3PagerWrite(pPage->pDbPage);
    if( rc ){
      goto end_insert;
    }
    oldCell = findCell(pPage, idx);
    if( !pPage->leaf ){
      memcpy(newCell, oldCell, 4);
    }
    szOld = cellSizePtr(pPage, oldCell);
    rc = clearCell(pPage, oldCell);   //释放任何与给定单元相关的溢出页
    dropCell(pPage, idx, szOld, &rc); //删除pPage的第i个单元.
    if( rc ) goto end_insert;
  }else if( loc<0 && pPage->nCell>0 ){
    assert( pPage->leaf );
    idx = ++pCur->aiIdx[pCur->iPage];
  }else{
    assert( pPage->leaf );
  }
  insertCell(pPage, idx, newCell, szNew, 0, 0, &rc); //在pPage的单元索引i处插入一个新单元.pCell指向单元的内容
  assert( rc!=SQLITE_OK || pPage->nCell>0 || pPage->nOverflow>0 );

  /* If no error has occured and pPage has an overflow cell, call balance() 
  ** to redistribute the cells within the tree. Since balance() may move
  ** the cursor, zero the BtCursor.info.nSize and BtCursor.validNKey
  ** variables.
  ** 如果没有错误发生,且pPage有溢出单元,调用balance()重新分布树内的单元。因为balance()
  ** 可能移动游标,所以清零BtCursor.info.nSize和BtCursor.validNKey两个变量。
  ** Previous versions of SQLite called moveToRoot() to move the cursor
  ** back to the root page as balance() used to invalidate the contents
  ** of BtCursor.apPage[] and BtCursor.aiIdx[]. Instead of doing that,
  ** set the cursor state to "invalid". This makes common insert operations
  ** slightly faster.
  ** 当balance()用于使BtCursor内容无效时，以前SQLite的版本调用moveToRoot()来移动游标回到根页面用来
  ** 使BtCursor.apPage[]和BtCursor.aiIdx[]的内容无效。相反,将游标状态设置为“invalid”。这使得
  ** 常见的插入操作更快。
  ** There is a subtle but important optimization here too. When inserting
  ** multiple records into an intkey b-tree using a single cursor (as can
  ** happen while processing an "INSERT INTO ... SELECT" statement), it
  ** is advantageous to leave the cursor pointing to the last entry in
  ** the b-tree if possible. If the cursor is left pointing to the last
  ** entry in the table, and the next row inserted has an integer key
  ** larger than the largest existing key, it is possible to insert the
  ** row without seeking the cursor. This can be a big performance boost.
  ** 这里有一个微小但重要的优化。当用一个游标插入多个记录到一个intkeyB树时使(在处理一个
  ** "INSERT INTO ... SELECT"语句),如果可能的话,使游标指向表中最后一个条目这是有利的。
  ** 如果游标指向表中最后一个条目,下一行插入有一个比已存在的最后键值要大的整数键值,它可以
  ** 在没有游标的的行插入。这可以极大地提高性能。
  */
  pCur->info.nSize = 0;
  pCur->validNKey = 0;
  if( rc==SQLITE_OK && pPage->nOverflow ){   /*pPage有溢出的单元格，调用balance(pCur)平衡B树*/
    rc = balance(pCur);

    /* Must make sure nOverflow is reset to zero even if the balance()
    ** fails. Internal data structure corruption will result otherwise. 
    ** Also, set the cursor state to invalid. This stops saveCursorPosition()
    ** from trying to save the current position of the cursor.  
	** 必须确保nOverflow复位为零,即使balance()失败.内部数据结构崩溃将导致其他结果。也要设置游标状态为无效。
	** 这将使saveCursorPosition()从试图保存当前光标的位置停止*/
    pCur->apPage[pCur->iPage]->nOverflow = 0;
    pCur->eState = CURSOR_INVALID;
  }
  assert( pCur->apPage[pCur->iPage]->nOverflow==0 );

end_insert:
  return rc;
}

/*
** Delete the entry that the cursor is pointing to.  The cursor
** is left pointing at a arbitrary location. 
** 删除游标指向的条目，使之指着任意位置。
*/
/*删除游标所指记录*/
int sqlite3BtreeDelete(BtCursor *pCur){    //删除游标指向的条目，使之指着任意位置
  Btree *p = pCur->pBtree;
  BtShared *pBt = p->pBt;              
  int rc;                              /* Return code */                     //返回代码
  MemPage *pPage;                      /* Page to delete cell from */        //要删除单元所在的页
  unsigned char *pCell;                /* Pointer to cell to delete */       //将要删除单元的指针
  int iCellIdx;                        /* Index of cell to delete */         //要删除单元的索引
  int iCellDepth;                      /* Depth of node containing pCell */  //包含pCell的单元深度

  assert( cursorHoldsMutex(pCur) );
  assert( pBt->inTransaction==TRANS_WRITE );
  assert( (pBt->btsFlags & BTS_READ_ONLY)==0 );
  assert( pCur->wrFlag );
  assert( hasSharedCacheTableLock(p, pCur->pgnoRoot, pCur->pKeyInfo!=0, 2) );
  assert( !hasReadConflicts(p, pCur->pgnoRoot) );

  if( NEVER(pCur->aiIdx[pCur->iPage]>=pCur->apPage[pCur->iPage]->nCell) 
   || NEVER(pCur->eState!=CURSOR_VALID)
  ){
    return SQLITE_ERROR;  /* Something has gone awry. */   
  }

  iCellDepth = pCur->iPage;
  iCellIdx = pCur->aiIdx[iCellDepth];
  pPage = pCur->apPage[iCellDepth];
  pCell = findCell(pPage, iCellIdx);

  /* If the page containing the entry to delete is not a leaf page, move
  ** the cursor to the largest entry in the tree that is smaller than
  ** the entry being deleted. This cell will replace the cell being deleted
  ** from the internal node. The 'previous' entry is used for this instead
  ** of the 'next' entry, as the previous entry is always a part of the
  ** sub-tree headed by the child page of the cell being deleted. This makes
  ** balancing the tree following the delete operation easier.  
  ** 如果页面包含要删除的条目不是叶子页面,移动游标到树中比要删除条目小的最大的条目。
  ** 这个单元将取代从内部节点被删除的单元。以前的条目用于此代替“next”条目,因为前面
  ** 的条目总是要删除单元的孩子页带领的子树的一部分。这使得删除操作后树的平衡造作更容易。*/
  if( !pPage->leaf ){
    int notUsed;
    rc = sqlite3BtreePrevious(pCur, &notUsed);  //逐步使游标回到数据库中以前的条目
    if( rc ) return rc;
  }

  /* Save the positions of any other cursors open on this table before
  ** making any modifications. Make the page containing the entry to be 
  ** deleted writable. Then free any overflow pages associated with the 
  ** entry and finally remove the cell itself from within the page. 
  ** 在任何修改之前，保存所有其他在此表上开放的游标的位置。使页面包含要删除的可写的条目。
  ** 然后释放任何与条目相关的溢出页,最后删除页面内的单元本身。
  */
  rc = saveAllCursors(pBt, pCur->pgnoRoot, pCur);/*修改之前保存所有打开的游标*/
  if( rc ) return rc;

  /* If this is a delete operation to remove a row from a table b-tree,
  ** invalidate any incrblob cursors open on the row being deleted.  
  ** 如果是从一个B树表中删除一行的删除操作,使在行上被删除的打开的任何incrblob游标无效。*/
  if( pCur->pKeyInfo==0 ){
    invalidateIncrblobCursors(p, pCur->info.nKey, 0);/*如果为删除操作，使所有incrblob游标无效*/
  }

  rc = sqlite3PagerWrite(pPage->pDbPage);
  if( rc ) return rc;
  rc = clearCell(pPage, pCell);
  dropCell(pPage, iCellIdx, cellSizePtr(pPage, pCell), &rc);
  if( rc ) return rc;

  /* If the cell deleted was not located on a leaf page, then the cursor
  ** is currently pointing to the largest entry in the sub-tree headed
  ** by the child-page of the cell that was just deleted from an internal
  ** node. The cell from the leaf node needs to be moved to the internal
  ** node to replace the deleted cell. 
  ** 如果删除单元并不位于叶子页面,然后游标当前指向子树中的最大的条目，这个条目在子树中被从
  ** 一个内部节点中删除的单元的孩子页面跟从。叶节点的单元需要移动到内部节点替换删除的单元。*/
  if( !pPage->leaf ){
    MemPage *pLeaf = pCur->apPage[pCur->iPage];
    int nCell;
    Pgno n = pCur->apPage[iCellDepth+1]->pgno;
    unsigned char *pTmp;

    pCell = findCell(pLeaf, pLeaf->nCell-1);
    nCell = cellSizePtr(pLeaf, pCell);
    assert( MX_CELL_SIZE(pBt) >= nCell );

    allocateTempSpace(pBt);
    pTmp = pBt->pTmpSpace;

    rc = sqlite3PagerWrite(pLeaf->pDbPage);
    insertCell(pPage, iCellIdx, pCell-4, nCell+4, pTmp, n, &rc);/*叶子结点上的单元格移到内部结点代替删除的单元格*/
    dropCell(pLeaf, pLeaf->nCell-1, nCell, &rc);
    if( rc ) return rc;
  }

  /* Balance the tree. If the entry deleted was located on a leaf page,
  ** then the cursor still points to that page. In this case the first
  ** call to balance() repairs the tree, and the if(...) condition is
  ** never true.
  ** 平衡树。如果删除的条目位于叶子页面,那么游标仍指向这个页面。在这种情况下,
  ** 第一次调用balance()调整树，并且if(...)条件不会为true。
  ** Otherwise, if the entry deleted was on an internal node page, then
  ** pCur is pointing to the leaf page from which a cell was removed to
  ** replace the cell deleted from the internal node. This is slightly
  ** tricky as the leaf node may be underfull, and the internal node may
  ** be either under or overfull. In this case run the balancing algorithm
  ** on the leaf node first. If the balance proceeds far enough up the
  ** tree that we can be sure that any problem in the internal node has
  ** been corrected, so be it. Otherwise, after balancing the leaf node,
  ** walk the cursor up the tree to the internal node and balance it as 
  ** well.  
  ** 如果要删除的条目是在一个内部节点页上，那么pCur指向叶子页面，该叶子页面上的单元
  ** 被移除替换内部节点中被删除的单元.这种情况有点棘手，因为叶节点可能不够满并且内部节点
  ** 可能满也可能不满.这时首先在叶节点上运行平衡算法.如果平衡过程足够,我们可以肯定,在内部节
  ** 点任何问题都被修正,所以执行.否则,叶子节点平衡后, 随着游标遍历树的内部节点并平衡它.*/
  rc = balance(pCur);
  if( rc==SQLITE_OK && pCur->iPage>iCellDepth ){
    while( pCur->iPage>iCellDepth ){
      releasePage(pCur->apPage[pCur->iPage--]);
    }
    rc = balance(pCur);
  }

  if( rc==SQLITE_OK ){
    moveToRoot(pCur);
  }
  return rc;
}

/*
** Create a new BTree table.  Write into *piTable the page
** number for the root page of the new table.
** 创建新的B树表。将新表根页的页码写到*piTable中。
** The type of type is determined by the flags parameter.  Only the
** following values of flags are currently in use.  Other values for
** flags might not work:
** 类型由标志参数决定，标志参数只有以下的可用。其他标志可能没有作用.
**     BTREE_INTKEY|BTREE_LEAFDATA     Used for SQL tables with rowid keys   //该标签用于带有列id键值的SQL表
**     BTREE_ZERODATA                  Used for SQL indices                  //该标签用于SQL索引
*/
/*
创建一个btree表格
移动现有数据库为新表的根页面腾出空间
用新根页更新映射寄存器和metadata
新表根页的页号放入PgnoRoot并写入piTable
*/
static int btreeCreateTable(Btree *p, int *piTable, int createTabFlags){ //创建新的B树表.新表根页页码写到*piTable
  BtShared *pBt = p->pBt;
  MemPage *pRoot;
  Pgno pgnoRoot;
  int rc;
  int ptfFlags;          /* Page-type flage for the root page of new table */ //新表根页的页类型标签

  assert( sqlite3BtreeHoldsMutex(p) );
  assert( pBt->inTransaction==TRANS_WRITE );
  assert( (pBt->btsFlags & BTS_READ_ONLY)==0 );

#ifdef SQLITE_OMIT_AUTOVACUUM
  rc = allocateBtreePage(pBt, &pRoot, &pgnoRoot, 1, 0);/*分配一个B树页面*/
  if( rc ){
    return rc;
  }
#else
  if( pBt->autoVacuum ){
    Pgno pgnoMove;      /* Move a page here to make room for the root-page */ //移动一个页到此处为根页腾出空间
    MemPage *pPageMove; /* The page to move to. */    //要移动的页

    /* Creating a new table may probably require moving an existing database
    ** to make room for the new tables root page. In case this page turns
    ** out to be an overflow page, delete all overflow page-map caches
    ** held by open cursors.
	** 创建一个新表可能要移动一个存在的数据库来为新表的根页腾出空间。加入该页是一个溢出页，那么删除开放性游标
	** 拥有的所有溢出页映射缓存。
    */
    invalidateAllOverflowCache(pBt); //在共享B树结构pBt上，对所有打开的游标使溢出页列表无效

    /* Read the value of meta[3] from the database to determine where the
    ** root page of the new table should go. meta[3] is the largest root-page
    ** created so far, so the new root-page is (meta[3]+1).
	** 从数据库中读取meta[3]的值来决定新表的根页应在的位置.meta[3]是目前以创建的最大的根页。因此新根页是meta[3]+1.
    */
    sqlite3BtreeGetMeta(p, BTREE_LARGEST_ROOT_PAGE, &pgnoRoot);/*获取最大根页meta*/
    pgnoRoot++; /*新页=根页+1*/

    /* The new root-page may not be allocated on a pointer-map page, or the
    ** PENDING_BYTE page. //新根页也许没有在指针位图页或PENDING_BYTE页上分配。
    */
    while( pgnoRoot==PTRMAP_PAGENO(pBt, pgnoRoot) ||
        pgnoRoot==PENDING_BYTE_PAGE(pBt) ){
      pgnoRoot++;/*新的根页不能是pointer-map page或者PENDING_BYTE page*/
    }
    assert( pgnoRoot>=3 );

    /* Allocate a page. The page that currently resides at pgnoRoot will
    ** be moved to the allocated page (unless the allocated page happens
    ** to reside at pgnoRoot).
	** 分配一个页.当前驻留在 pgnoRoot上页将移动到分配的页(除非分配的页已经驻留在 pgnoRoot中).
    */
    rc = allocateBtreePage(pBt, &pPageMove, &pgnoMove, pgnoRoot,1);//从数据库文件分配一个新页面,成功则返回SQLITE_OK
    if( rc!=SQLITE_OK ){
      return rc;
    }

    if( pgnoMove!=pgnoRoot ){
      /* pgnoRoot is the page that will be used for the root-page of
      ** the new table (assuming an error did not occur). But we were
      ** allocated pgnoMove. If required (i.e. if it was not allocated
      ** by extending the file), the current page at position pgnoMove
      ** is already journaled.
	  ** pgnoRoot是将被用作新表根页的页面(假设没有发生错误的话)。但分配pgnoMove。
	  ** 如果需要(即如果没有被扩展文件分配),那么当前页面位置pgnoMove记录到日志。
      */
      u8 eType = 0;
      Pgno iPtrPage = 0;

      releasePage(pPageMove);

      /* Move the page currently at pgnoRoot to pgnoMove. */  //移动当前在pgnoRoot的页面到pgnoMove.
      rc = btreeGetPage(pBt, pgnoRoot, &pRoot, 0); //从页对象得到一个页.若需要,则初始化MemPage.pBt和MemPage.aData
      if( rc!=SQLITE_OK ){
        return rc;
      }
      rc = ptrmapGet(pBt, pgnoRoot, &eType, &iPtrPage);  //从指针位图读取
      if( eType==PTRMAP_ROOTPAGE || eType==PTRMAP_FREEPAGE ){
        rc = SQLITE_CORRUPT_BKPT;
      }
      if( rc!=SQLITE_OK ){
        releasePage(pRoot);
        return rc;
      }
      assert( eType!=PTRMAP_ROOTPAGE );
      assert( eType!=PTRMAP_FREEPAGE );
      rc = relocatePage(pBt, pRoot, eType, iPtrPage, pgnoMove, 0); //移动开放数据库页pRoot到要存放位置pgnoMove
      releasePage(pRoot);

      /* Obtain the page at pgnoRoot */  //获得在pgnoRoot上的页
      if( rc!=SQLITE_OK ){
        return rc;
      }
      rc = btreeGetPage(pBt, pgnoRoot, &pRoot, 0);
      if( rc!=SQLITE_OK ){
        return rc;
      }
      rc = sqlite3PagerWrite(pRoot->pDbPage);
      if( rc!=SQLITE_OK ){
        releasePage(pRoot);
        return rc;
      }
    }else{
      pRoot = pPageMove;
    } 

    /* Update the pointer-map and meta-data with the new root-page number. */
	//更新带有新根页的指针位图和元数据
    ptrmapPut(pBt, pgnoRoot, PTRMAP_ROOTPAGE, 0, &rc);
    if( rc ){
      releasePage(pRoot);
      return rc;
    }

    /* When the new root page was allocated, page 1 was made writable in
    ** order either to increase the database filesize, or to decrement the
    ** freelist count.  Hence, the sqlite3BtreeUpdateMeta() call cannot fail.
	** 当新根页面分配时,第一页是可写的为了增加数据库文件大小,或减减少空白列表的计数。
	** 因此,sqlite3BtreeUpdateMeta()调用不能失败。
    */
    assert( sqlite3PagerIswriteable(pBt->pPage1->pDbPage) );
    rc = sqlite3BtreeUpdateMeta(p, 4, pgnoRoot);/*增加数据库文件大小，减少空闲列表的数量*/
    if( NEVER(rc) ){
      releasePage(pRoot);
      return rc;
    }

  }else{
    rc = allocateBtreePage(pBt, &pRoot, &pgnoRoot, 1, 0);  //从数据库文件分配一个新页
    if( rc ) return rc;
  }
#endif
  assert( sqlite3PagerIswriteable(pRoot->pDbPage) );
  if( createTabFlags & BTREE_INTKEY ){
    ptfFlags = PTF_INTKEY | PTF_LEAFDATA | PTF_LEAF;
  }else{
    ptfFlags = PTF_ZERODATA | PTF_LEAF;
  }
  zeroPage(pRoot, ptfFlags);
  sqlite3PagerUnref(pRoot->pDbPage);
  assert( (pBt->openFlags & BTREE_SINGLE)==0 || pgnoRoot==2 );
  *piTable = (int)pgnoRoot; /*将新表根页写入piTable*/
  return SQLITE_OK;
}

/*在数据库中创建一个空B树，采用图格式（B+树）或索引格式（B树）*/
int sqlite3BtreeCreateTable(Btree *p, int *piTable, int flags){ 
  int rc;
  sqlite3BtreeEnter(p);
  rc = btreeCreateTable(p, piTable, flags); //创建新的B树表.新表根页页码写到*piTable
  sqlite3BtreeLeave(p);
  return rc;
}

/*
** Erase the given database page and all its children.  Return
** the page to the freelist.
** 擦除给定的数据库页和其所有孩子节点.返回页到列表页.
*/
static int clearDatabasePage(    //擦除给定的数据库页和其所有孩子节点.返回页到列表页.
  BtShared *pBt,           /* The BTree that contains the table */          //包含表的B树
  Pgno pgno,               /* Page number to clear */                       //清理页码
  int freePageFlag,        /* Deallocate page if true */                    //如果为true,释放页
  int *pnChange            /* Add number of Cells freed to this counter */  //添加单元释放此计数器的数量
){
  MemPage *pPage;
  int rc;
  unsigned char *pCell;
  int i;

  assert( sqlite3_mutex_held(pBt->mutex) );
  if( pgno>btreePagecount(pBt) ){
    return SQLITE_CORRUPT_BKPT;
  }

  rc = getAndInitPage(pBt, pgno, &pPage);  //从页对象中获得一个页面并初始化
  if( rc ) return rc;
  for(i=0; i<pPage->nCell; i++){
    pCell = findCell(pPage, i);
    if( !pPage->leaf ){
      rc = clearDatabasePage(pBt, get4byte(pCell), 1, pnChange);
      if( rc ) goto cleardatabasepage_out;
    }
    rc = clearCell(pPage, pCell);
    if( rc ) goto cleardatabasepage_out;
  }
  if( !pPage->leaf ){
    rc = clearDatabasePage(pBt, get4byte(&pPage->aData[8]), 1, pnChange);
    if( rc ) goto cleardatabasepage_out;
  }else if( pnChange ){
    assert( pPage->intKey );
    *pnChange += pPage->nCell;
  }
  if( freePageFlag ){
    freePage(pPage, &rc);
  }else if( (rc = sqlite3PagerWrite(pPage->pDbPage))==0 ){
    zeroPage(pPage, pPage->aData[0] | PTF_LEAF);
  }

cleardatabasepage_out:
  releasePage(pPage);
  return rc;
}

/*
** Delete all information from a single table in the database.  iTable is
** the page number of the root of the table.  After this routine returns,
** the root page is empty, but still exists.
** 从数据库中的一个表中删除所有信息。iTable是表中根的页号。这个函数返回后,根页面是空的,但仍然存在。
** This routine will fail with SQLITE_LOCKED if there are any open
** read cursors on the table.  Open write cursors are moved to the
** root of the table.
** 如果在表上如果有任何开放性游标那么这个函数失败返回SQLITE_LOCKED.开放的写游标被动到表的根页。
** If pnChange is not NULL, then table iTable must be an intkey table. The
** integer value pointed to by pnChange is incremented by the number of
** entries in the table.
** 如果pnChange非空,那么表ITable一定是一个intkey表.pnChange指向这个整数的值，它是根据表中条目的数量增加的.
*/
/*
删除B-tree中所有的数据，但保持B-tree结构完整。
*/
int sqlite3BtreeClearTable(Btree *p, int iTable, int *pnChange){  //删除B-tree中所有的数据，但保持B-tree结构完整
  int rc;
  BtShared *pBt = p->pBt;
  sqlite3BtreeEnter(p);
  assert( p->inTrans==TRANS_WRITE );

  rc = saveAllCursors(pBt, (Pgno)iTable, 0);

  if( SQLITE_OK==rc ){
    /* Invalidate all incrblob cursors open on table iTable (assuming iTable
    ** is the root of a table b-tree - if it is not, the following call is
    ** a no-op).  
	** 在表ITable上使开放的incrblob游标无效.(假定ITable是B树的根页，如果不是下面的调用无操作)*/
    invalidateIncrblobCursors(p, 0, 1);
    rc = clearDatabasePage(pBt, (Pgno)iTable, 0, pnChange);
  }
  sqlite3BtreeLeave(p);
  return rc;
}

/*
** Erase all information in a table and add the root of the table to
** the freelist.  Except, the root of the principle table (the one on
** page 1) is never added to the freelist.
** 清除表上的所有信息并且添加标的根到空闲列表。除此之外，根源表(一个在页1上的表)的根从不加入到空闲列表.
** This routine will fail with SQLITE_LOCKED if there are any open
** cursors on the table.
** 如果在表上如果有任何开放性游标那么这个函数失败返回SQLITE_LOCKED.
** If AUTOVACUUM is enabled and the page at iTable is not the last
** root page in the database file, then the last root page 
** in the database file is moved into the slot formerly occupied by
** iTable and that last slot formerly occupied by the last root page
** is added to the freelist instead of iTable.  In this say, all
** root pages are kept at the beginning of the database file, which
** is necessary for AUTOVACUUM to work right.  *piMoved is set to the 
** page number that used to be the last root page in the file before
** the move.  If no page gets moved, *piMoved is set to 0.
** The last root page is recorded in meta[3] and the value of
** meta[3] is updated by this procedure.
** 如果AUTOVACUUM可用并且iTable上的页不是数据库文件的最后根页，那么数据库文件中最后的根页将被移动到
** 被iTable占用的位置并且上次被根页占用的加到空闲列表而不是iTbale。也就是说，所有的根页数据库文件的开始，
** 这对于AUTOVACUUM正常工作是有必要的。*piMoved被设置为移动之前文件中是最后根页的页码.如果没有页要移动，
** 则*piMoved设为0.最后的根页是记录在meta[3]中并且meta[3]的值在这个过程中被更新。
*/
static int btreeDropTable(Btree *p, Pgno iTable, int *piMoved){   //清除表上的所有信息并且添加标的根到空闲列表
  int rc;
  MemPage *pPage = 0;
  BtShared *pBt = p->pBt;

  assert( sqlite3BtreeHoldsMutex(p) );
  assert( p->inTrans==TRANS_WRITE );

  /* It is illegal to drop a table if any cursors are open on the
  ** database. This is because in auto-vacuum mode the backend may
  ** need to move another root-page to fill a gap left by the deleted
  ** root page. If an open cursor was using this page a problem would 
  ** occur.
  ** 如果数据库上的任何游标开放了，那么操作是非法的。这是因为在auto-vacum模式下后端可能需要移动
  ** 另外一个根页来填充通过删除根页留下的缝隙。如果一个打开的游标在页上正在使用，那么将会出现问题.
  ** This error is caught long before control reaches this point.
  */
  if( NEVER(pBt->pCursor) ){
    sqlite3ConnectionBlocked(p->db, pBt->pCursor->pBtree->db);
    return SQLITE_LOCKED_SHAREDCACHE;/*游标不能为打开状态*/
  }

  rc = btreeGetPage(pBt, (Pgno)iTable, &pPage, 0);
  if( rc ) return rc;
  rc = sqlite3BtreeClearTable(p, iTable, 0);
  if( rc ){
    releasePage(pPage);
    return rc;
  }

  *piMoved = 0;

  if( iTable>1 ){ /*页必须大于1，才能被删除*/
#ifdef SQLITE_OMIT_AUTOVACUUM
    freePage(pPage, &rc);
    releasePage(pPage);
#else
    if( pBt->autoVacuum ){
      Pgno maxRootPgno;
      sqlite3BtreeGetMeta(p, BTREE_LARGEST_ROOT_PAGE, &maxRootPgno);  //读数据库文件的元数据信息

      if( iTable==maxRootPgno ){
        /* If the table being dropped is the table with the largest root-page
        ** number in the database, put the root page on the free list. 
		** 如果被删除的表是数据库中有最大根页码的表，那么把根页放到空闲列表.
        */
        freePage(pPage, &rc);
        releasePage(pPage);
        if( rc!=SQLITE_OK ){
          return rc;
        }
      }else{
        /* The table being dropped does not have the largest root-page
        ** number in the database. So move the page that does into the 
        ** gap left by the deleted root-page.
		** 在数据库中被删除的表没有最大的根页码，因此移动页到因删除根页而产生的缝隙处.
        */
        MemPage *pMove;
        releasePage(pPage);
        rc = btreeGetPage(pBt, maxRootPgno, &pMove, 0);
        if( rc!=SQLITE_OK ){
          return rc;
        }
        rc = relocatePage(pBt, pMove, PTRMAP_ROOTPAGE, 0, iTable, 0);/*移动页去填补删除的根页。*/
        releasePage(pMove);
        if( rc!=SQLITE_OK ){
          return rc;
        }
        pMove = 0;
        rc = btreeGetPage(pBt, maxRootPgno, &pMove, 0);
        freePage(pMove, &rc);
        releasePage(pMove);
        if( rc!=SQLITE_OK ){
          return rc;
        }
        *piMoved = maxRootPgno;
      }

      /* Set the new 'max-root-page' value in the database header. This
      ** is the old value less one, less one more if that happens to
      ** be a root-page number, less one again if that is the
      ** PENDING_BYTE_PAGE.
	  ** 在数据库头部设置新的max-root-page的值.这是原来的值减一，如果是在根页码上要少不止1.
	  ** 如果是PENDING_BYTE_PAGE再次减1.
      */
      maxRootPgno--;
      while( maxRootPgno==PENDING_BYTE_PAGE(pBt)
             || PTRMAP_ISPAGE(pBt, maxRootPgno) ){
        maxRootPgno--;
      }
      assert( maxRootPgno!=PENDING_BYTE_PAGE(pBt) );

      rc = sqlite3BtreeUpdateMeta(p, 4, maxRootPgno);/*更新最大的根页*/
    }else{
      freePage(pPage, &rc);
      releasePage(pPage);
    }
#endif
  }else{
    /* If sqlite3BtreeDropTable was called on page 1.
    ** This really never should happen except in a corrupt
    ** database. 
	** 如果sqlite3BtreeDropTable在page1上被调用.除了在崩溃的数据库上其他情况不会发生。
    */
    zeroPage(pPage, PTF_INTKEY|PTF_LEAF );
    releasePage(pPage);
  }
  return rc;  
}

int sqlite3BtreeDropTable(Btree *p, int iTable, int *piMoved){  //删除数据库中的一个B树
  int rc;
  sqlite3BtreeEnter(p);
  rc = btreeDropTable(p, iTable, piMoved);
  sqlite3BtreeLeave(p);
  return rc;
}

/*
** This function may only be called if the b-tree connection already
** has a read or write transaction open on the database.
** 如果在数据库上B树已经有一个读或写事务开放，那么这个函数将唯一被调用.
** Read the meta-information out of a database file.  Meta[0]
** is the number of free pages currently in the database.  Meta[1]
** through meta[15] are available for use by higher layers.  Meta[0]
** is read-only, the others are read/write.
** 读数据库文件的元数据信息.Meta[0]是当前数据库中空白页面的数量.Meta[1]到Meta[15]对于更高层时可用的。
** Meta[0]是只读的，其他的都是读写的。
** The schema layer numbers meta values differently.  At the schema
** layer (and the SetCookie and ReadCookie opcodes) the number of
** free pages is not visible.  So Cookie[0] is the same as Meta[1].
** 模式层记录元数据的不同值.在模式层(SetCookie和ReadCookie操作码)空白页的数量是不可见的。
** 因此Cookie[0]和 Meta[1]是相同的.
*/ 
/*如果b-tree连接一个读或写事务，这个函数可能被调用。从数据库文件中读出meta-information。
Meta[0]是数据库中的自由页。Meta[1]可以被用户通过meta[15]访问。Meta[0]为只读，其余为读写。
*/
void sqlite3BtreeGetMeta(Btree *p, int idx, u32 *pMeta){   //读数据库文件的元数据信息
  BtShared *pBt = p->pBt;

  sqlite3BtreeEnter(p);
  assert( p->inTrans>TRANS_NONE );
  assert( SQLITE_OK==querySharedCacheTableLock(p, MASTER_ROOT, READ_LOCK) );
  assert( pBt->pPage1 );
  assert( idx>=0 && idx<=15 );

  *pMeta = get4byte(&pBt->pPage1->aData[36 + idx*4]);

  /* If auto-vacuum is disabled in this build and this is an auto-vacuum
  ** database, mark the database as read-only.  */
  /*
  ** 如果在构建中auto-vacuum为不可用状态且是auto-vacuum数据库，标记数据库为只读。
  */
#ifdef SQLITE_OMIT_AUTOVACUUM
  if( idx==BTREE_LARGEST_ROOT_PAGE && *pMeta>0 ){
    pBt->btsFlags |= BTS_READ_ONLY;
  }
#endif

  sqlite3BtreeLeave(p);  //在B树上退出互斥锁
}

/*
** Write meta-information back into the database.  Meta[0] is
** read-only and may not be written.
*/
/*
** 把meta-information写回数据库。Meta[0]为只读且可能不会被写。
*/

int sqlite3BtreeUpdateMeta(Btree *p, int idx, u32 iMeta){  //把meta-information写回数据库，更新元数据
  BtShared *pBt = p->pBt;
  unsigned char *pP1;
  int rc;
  assert( idx>=1 && idx<=15 );
  sqlite3BtreeEnter(p);
  assert( p->inTrans==TRANS_WRITE );
  assert( pBt->pPage1!=0 );
  pP1 = pBt->pPage1->aData;
  rc = sqlite3PagerWrite(pBt->pPage1->pDbPage); /*标记pBt->pPage1->pDbPage可写*/
  if( rc==SQLITE_OK ){
    put4byte(&pP1[36 + idx*4], iMeta);/*将iMeta分成4个字节放入pP1*/
#ifndef SQLITE_OMIT_AUTOVACUUM
    if( idx==BTREE_INCR_VACUUM ){
      assert( pBt->autoVacuum || iMeta==0 );
      assert( iMeta==0 || iMeta==1 );
      pBt->incrVacuum = (u8)iMeta;
    }
#endif
  }
  sqlite3BtreeLeave(p);  //在B树上退出互斥锁
  return rc;
}

#ifndef SQLITE_OMIT_BTREECOUNT
/*
** The first argument, pCur, is a cursor opened on some b-tree. Count the
** number of entries in the b-tree and write the result to *pnEntry.
**
** SQLITE_OK is returned if the operation is successfully executed. 
** Otherwise, if an error is encountered (i.e. an IO error or database
** corruption) an SQLite error code is returned.
*/
/*
第一个参数pCur，是B树上一个打开的游标。给B树上的条目计数，把结果写到*pnEntry。
如果操作成功执行，则返回 SQLITE_OK，反之返回SQLite错误代码(例如I/O错误或数据库崩溃)。
*/
int sqlite3BtreeCount(BtCursor *pCur, i64 *pnEntry){    //给B树上的条目计数
  i64 nEntry = 0;                      /* Value to return in *pnEntry */   //要写入到*pnEntry的值
  int rc;                              /* Return code */                   //返回代码

  if( pCur->pgnoRoot==0 ){
    *pnEntry = 0;
    return SQLITE_OK;
  }
  rc = moveToRoot(pCur);

  /* Unless an error occurs, the following loop runs one iteration for each
  ** page in the B-Tree structure (not including overflow pages). 
  */
  /*除非错误发生，以下循环在每一个B-Tree结构中执行一次迭代，但不包括溢出页*/
  while( rc==SQLITE_OK ){
    int iIdx;              /* Index of child node in parent */  //父节点的孩子节点的索引
    MemPage *pPage;        /* Current page of the b-tree */     //B树的当前页

    /* If this is a leaf page or the tree is not an int-key tree, then 
    ** this page contains countable entries. Increment the entry counter
    ** accordingly.
    */
    /*
     如果这是一个叶子页，或者B树上关键字不是整型的，那么这个页包含可数的条目。相应地增加
     条目的数量。
	*/
    pPage = pCur->apPage[pCur->iPage];
    if( pPage->leaf || !pPage->intKey ){
      nEntry += pPage->nCell;
    }

    /* pPage is a leaf node. This loop navigates the cursor so that it 
    ** points to the first interior cell that it points to the parent of
    ** the next page in the tree that has not yet been visited. The
    ** pCur->aiIdx[pCur->iPage] value is set to the index of the parent cell
    ** of the page, or to the number of cells in the page if the next page
    ** to visit is the right-child of its parent.
    **
    ** If all pages in the tree have been visited, return SQLITE_OK to the
    ** caller.
    */
    /*pPage是一个叶子页。这个循环使这个游标指向第一个内部的单元格。这个单元格指向树中没有被访问的下一页的
	父节点。pCur->aiIdx[pCur->iPage]的值设置为页中父单元格的索引数，如果下一页将要访问
	父节点的右孩子，pCur->aiIdx[pCur->iPage]的值设置为页中单元格的数量。
	** 若树中的所有页都被访问，返回SQLITE_OK.
	*/
    if( pPage->leaf ){
      do {
        if( pCur->iPage==0 ){
          /* All pages of the b-tree have been visited. Return successfully. */  //B树页都被访问成功返回
          *pnEntry = nEntry;
          return SQLITE_OK;
        }
        moveToParent(pCur);
      }while ( pCur->aiIdx[pCur->iPage]>=pCur->apPage[pCur->iPage]->nCell );

      pCur->aiIdx[pCur->iPage]++;
      pPage = pCur->apPage[pCur->iPage];
    }

    /* Descend to the child node of the cell that the cursor currently 
    ** points at. This is the right-child if (iIdx==pPage->nCell).
    */
    /*下降到当前游标指向结点的孩子结点。如果iIdx==pPage->nCell，取右孩子结点。*/
    iIdx = pCur->aiIdx[pCur->iPage];
    if( iIdx==pPage->nCell ){
      rc = moveToChild(pCur, get4byte(&pPage->aData[pPage->hdrOffset+8]));
    }else{
      rc = moveToChild(pCur, get4byte(findCell(pPage, iIdx)));
    }
  }

  /* An error has occurred. Return an error code. */
  return rc;
}
#endif

/*
** Return the pager associated with a BTree.  This routine is used for
** testing and debugging only.
** 返回与B树相关的页.该函数仅用来测试和调试.
*/
Pager *sqlite3BtreePager(Btree *p){
  return p->pBt->pPager;
}

#ifndef SQLITE_OMIT_INTEGRITY_CHECK
/*
** Append a message to the error message string.
*/
/*把消息附加到错误消息字符串的后面。*/
static void checkAppendMsg(      //把消息附加到错误消息字符串的后面
  IntegrityCk *pCheck,
  char *zMsg1,
  const char *zFormat,
  ...
){
  va_list ap;
  if( !pCheck->mxErr ) return;
  pCheck->mxErr--;
  pCheck->nErr++;
  va_start(ap, zFormat);
  if( pCheck->errMsg.nChar ){
    sqlite3StrAccumAppend(&pCheck->errMsg, "\n", 1);
  }
  if( zMsg1 ){
    sqlite3StrAccumAppend(&pCheck->errMsg, zMsg1, -1);
  }
  sqlite3VXPrintf(&pCheck->errMsg, 1, zFormat, ap);
  va_end(ap);
  if( pCheck->errMsg.mallocFailed ){
    pCheck->mallocFailed = 1;
  }
}
#endif /* SQLITE_OMIT_INTEGRITY_CHECK */

#ifndef SQLITE_OMIT_INTEGRITY_CHECK

/*
** Return non-zero if the bit in the IntegrityCk.aPgRef[] array that
** corresponds to page iPg is already set.
*/
/*
如果在IntegrityCk.aPgRef[]数组中，对应的页iPg已经设置，返回非0。
*/
static int getPageReferenced(IntegrityCk *pCheck, Pgno iPg){
  assert( iPg<=pCheck->nPage && sizeof(pCheck->aPgRef[0])==1 );
  return (pCheck->aPgRef[iPg/8] & (1 << (iPg & 0x07)));
}

/*
** Set the bit in the IntegrityCk.aPgRef[] array that corresponds to page iPg.
** 设定在与页iPg相应的IntegrityCk.aPgRef[]数组中的位.
*/
static void setPageReferenced(IntegrityCk *pCheck, Pgno iPg){  //设定在与页iPg相应的IntegrityCk.aPgRef[]数组中的位
  assert( iPg<=pCheck->nPage && sizeof(pCheck->aPgRef[0])==1 );
  pCheck->aPgRef[iPg/8] |= (1 << (iPg & 0x07));
}


/*
** Add 1 to the reference count for page iPage.  If this is the second
** reference to the page, add an error message to pCheck->zErrMsg.
** Return 1 if there are 2 ore more references to the page and 0 if
** if this is the first reference to the page.
**
** Also check that the page number is in bounds.
*/
/*对页iPage的引用数量加1。如果是对页的第二个引用，加错误消息到pCheck->zErrMsg。
如果对页有两次或者更多次，返回1。如果这个页被第一次引用，返回0。
*/
static int checkRef(IntegrityCk *pCheck, Pgno iPage, char *zContext){  //对页iPage的引用数量加1
  if( iPage==0 ) return 1;
  if( iPage>pCheck->nPage ){
    checkAppendMsg(pCheck, zContext, "invalid page number %d", iPage);
    return 1;
  }
  if( getPageReferenced(pCheck, iPage) ){
    checkAppendMsg(pCheck, zContext, "2nd reference to page %d", iPage);
    return 1;
  }
  setPageReferenced(pCheck, iPage);
  return 0;
}

#ifndef SQLITE_OMIT_AUTOVACUUM
/*
** Check that the entry in the pointer-map for page iChild maps to 
** page iParent, pointer type ptrType. If not, append an error message
** to pCheck.
** 核对从页iChild映射到页iParent的指针位图中的条目，指针类型为ptrType.如果没有，附加错误信息到pCheck.
*/

static void checkPtrmap(           //核对从页iChild映射到页iParent的指针位图中的条目
  IntegrityCk *pCheck,   /* Integrity check context */                    //完整性检查上下文
  Pgno iChild,           /* Child page number */                          //孩子页的页号
  u8 eType,              /* Expected pointer map type */                  //指针映射的类型
  Pgno iParent,          /* Expected pointer map parent page number */    //指针映射的父页码
  char *zContext         /* Context description (used for error msg) */   //上下文描述(用作错误描述)
){
  int rc;
  u8 ePtrmapType;
  Pgno iPtrmapParent;

  rc = ptrmapGet(pCheck->pBt, iChild, &ePtrmapType, &iPtrmapParent);      //从指针位图读取条目
  if( rc!=SQLITE_OK ){
    if( rc==SQLITE_NOMEM || rc==SQLITE_IOERR_NOMEM ) pCheck->mallocFailed = 1;
    checkAppendMsg(pCheck, zContext, "Failed to read ptrmap key=%d", iChild);
    return;
  }

  if( ePtrmapType!=eType || iPtrmapParent!=iParent ){
    checkAppendMsg(pCheck, zContext, 
      "Bad ptr map entry key=%d expected=(%d,%d) got=(%d,%d)", 
      iChild, eType, iParent, ePtrmapType, iPtrmapParent);
  }
}
#endif

/*
** Check the integrity of the freelist or of an overflow page list.
** Verify that the number of pages on the list is N.
** 检查空闲列表或溢出页列表的完整性。查证列表上的页数是N.
*/

static void checkList(        //检查空闲列表或溢出页列表的完整性
  IntegrityCk *pCheck,  /* Integrity checking context */                         //上下文完整性检查
  int isFreeList,       /* True for a freelist.  False for overflow page list */ //空闲列表为true,溢出页列表为false
  int iPage,            /* Page number for first page in the list */             //列表中第一个页的页码
  int N,                /* Expected number of pages in the list */               //在列表中预期的页面数量
  char *zContext        /* Context for error messages */                         //上下文的错误信息
){
  int i;
  int expected = N;
  int iFirst = iPage;
  while( N-- > 0 && pCheck->mxErr ){
    DbPage *pOvflPage;
    unsigned char *pOvflData;
    if( iPage<1 ){
      checkAppendMsg(pCheck, zContext,
         "%d of %d pages missing from overflow list starting at %d",
          N+1, expected, iFirst);
      break;
    }
    if( checkRef(pCheck, iPage, zContext) ) break;
    if( sqlite3PagerGet(pCheck->pPager, (Pgno)iPage, &pOvflPage) ){
      checkAppendMsg(pCheck, zContext, "failed to get page %d", iPage);
      break;
    }
    pOvflData = (unsigned char *)sqlite3PagerGetData(pOvflPage);
    if( isFreeList ){
      int n = get4byte(&pOvflData[4]);
#ifndef SQLITE_OMIT_AUTOVACUUM
      if( pCheck->pBt->autoVacuum ){
        checkPtrmap(pCheck, iPage, PTRMAP_FREEPAGE, 0, zContext);
      }
#endif
      if( n>(int)pCheck->pBt->usableSize/4-2 ){
        checkAppendMsg(pCheck, zContext,
           "freelist leaf count too big on page %d", iPage);
        N--;
      }else{
        for(i=0; i<n; i++){
          Pgno iFreePage = get4byte(&pOvflData[8+i*4]);
#ifndef SQLITE_OMIT_AUTOVACUUM
          if( pCheck->pBt->autoVacuum ){
            checkPtrmap(pCheck, iFreePage, PTRMAP_FREEPAGE, 0, zContext);
          }
#endif
          checkRef(pCheck, iFreePage, zContext);
        }
        N -= n;
      }
    }
#ifndef SQLITE_OMIT_AUTOVACUUM
    else{
      /* If this database supports auto-vacuum and iPage is not the last
      ** page in this overflow list, check that the pointer-map entry for
      ** the following page matches iPage.
      */
      /*
	  ** 如果数据库支持auto-vacuum并且iPage不是溢出链表中的最后一页，检查匹配下一页与
	  ** iPage匹配的指针位图条目。
	  */
      if( pCheck->pBt->autoVacuum && N>0 ){
        i = get4byte(pOvflData);
        checkPtrmap(pCheck, i, PTRMAP_OVERFLOW2, iPage, zContext);
      }
    }
#endif
    iPage = get4byte(pOvflData);
    sqlite3PagerUnref(pOvflPage);
  }
}
#endif /* SQLITE_OMIT_INTEGRITY_CHECK */

#ifndef SQLITE_OMIT_INTEGRITY_CHECK
/*
** Do various sanity checks on a single page of a tree.  Return
** the tree depth.  Root pages return 0.  Parents of root pages
** return 1, and so forth.
** 
** These checks are done:
**
**      1.  Make sure that cells and freeblocks do not overlap
**          but combine to completely cover the page.
**  NO  2.  Make sure cell keys are in order.
**  NO  3.  Make sure no key is less than or equal to zLowerBound.
**  NO  4.  Make sure no key is greater than or equal to zUpperBound.
**      5.  Check the integrity of overflow pages.
**      6.  Recursively call checkTreePage on all children.
**      7.  Verify that the depth of all children is the same.
**      8.  Make sure this page is at least 33% full or else it is
**          the root of the tree.
*/
/*
** 在树的一个单独的页上进行检查。返回树的深度。根页返回0。根页上的父结点返回1，
** 依次类推。以下将被检查:
** 1、确保单元格和自由块不覆盖，但是完全覆盖页。
** 2、确保单元格上的关键字有序。
** 3、确保关键字大于zLowerBound。
** 4、确保关键字小于zUpperBound。
** 5、检查溢出页的完整性。
** 6、在所有孩子结点上递归调用checkTreePage。
** 7、确保所有孩子结点深度相同。
** 8、除了根页，页至少有33%的空间被使用。
*/
static int checkTreePage(    //在树的一个单独的页上进行检查
  IntegrityCk *pCheck,  /* Context for the sanity check */       //核对上下文
  int iPage,            /* Page number of the page to check */   //要核对页的页码
  char *zParentContext, /* Parent context */                     //父节点上下文
  i64 *pnParentMinKey, 
  i64 *pnParentMaxKey
){
  MemPage *pPage;
  int i, rc, depth, d2, pgno, cnt;
  int hdr, cellStart;
  int nCell;
  u8 *data;
  BtShared *pBt;
  int usableSize;
  char zContext[100];
  char *hit = 0;
  i64 nMinKey = 0;
  i64 nMaxKey = 0;

  sqlite3_snprintf(sizeof(zContext), zContext, "Page %d: ", iPage);

  /* Check that the page exists */
  pBt = pCheck->pBt;
  usableSize = pBt->usableSize;
  if( iPage==0 ) return 0;
  if( checkRef(pCheck, iPage, zParentContext) ) return 0;
  if( (rc = btreeGetPage(pBt, (Pgno)iPage, &pPage, 0))!=0 ){
    checkAppendMsg(pCheck, zContext,
       "unable to get the page. error code=%d", rc);
    return 0;
  }

  /* Clear MemPage.isInit to make sure the corruption detection code in
  ** btreeInitPage() is executed.  
  ** 清理MemPage.isInit确保btreeInitPage()中的崩溃检测代码执行 */
  pPage->isInit = 0;
  if( (rc = btreeInitPage(pPage))!=0 ){
    assert( rc==SQLITE_CORRUPT );  /* The only possible error from InitPage */
    checkAppendMsg(pCheck, zContext, 
                   "btreeInitPage() returns error code %d", rc);
    releasePage(pPage);
    return 0;
  }

  /* Check out all the cells.*/
  depth = 0;
  for(i=0; i<pPage->nCell && pCheck->mxErr; i++){
    u8 *pCell;
    u32 sz;
    CellInfo info;

    /* Check payload overflow pages */
    sqlite3_snprintf(sizeof(zContext), zContext,
             "On tree page %d cell %d: ", iPage, i);
    pCell = findCell(pPage,i);
    btreeParseCellPtr(pPage, pCell, &info);   //解析单元内容块，填在CellInfo结构中
    sz = info.nData;
    if( !pPage->intKey ) sz += (int)info.nKey;
    /* For intKey pages, check that the keys are in order. */  //对于intKey，有序核对关键字
    else if( i==0 ) nMinKey = nMaxKey = info.nKey;
    else{
      if( info.nKey <= nMaxKey ){
        checkAppendMsg(pCheck, zContext, 
            "Rowid %lld out of order (previous was %lld)", info.nKey, nMaxKey);
      }
      nMaxKey = info.nKey;
    }
    assert( sz==info.nPayload );
    if( (sz>info.nLocal) 
     && (&pCell[info.iOverflow]<=&pPage->aData[pBt->usableSize])
    ){
      int nPage = (sz - info.nLocal + usableSize - 5)/(usableSize - 4);
      Pgno pgnoOvfl = get4byte(&pCell[info.iOverflow]);
#ifndef SQLITE_OMIT_AUTOVACUUM
      if( pBt->autoVacuum ){
        checkPtrmap(pCheck, pgnoOvfl, PTRMAP_OVERFLOW1, iPage, zContext); //核对从页pgnoOvfl映射到页iPage的指针位图中的条目
      }
#endif
      checkList(pCheck, 0, pgnoOvfl, nPage, zContext);  //检查空闲列表或溢出页列表的完整性
    }

    /* Check sanity of left child page. */    //核对左孩子
    if( !pPage->leaf ){
      pgno = get4byte(pCell);
#ifndef SQLITE_OMIT_AUTOVACUUM
      if( pBt->autoVacuum ){
        checkPtrmap(pCheck, pgno, PTRMAP_BTREE, iPage, zContext);
      }
#endif
      d2 = checkTreePage(pCheck, pgno, zContext, &nMinKey, i==0 ? NULL : &nMaxKey);
      if( i>0 && d2!=depth ){
        checkAppendMsg(pCheck, zContext, "Child page depth differs");
      }
      depth = d2;
    }
  }

  if( !pPage->leaf ){
    pgno = get4byte(&pPage->aData[pPage->hdrOffset+8]);
    sqlite3_snprintf(sizeof(zContext), zContext, 
                     "On page %d at right child: ", iPage);
#ifndef SQLITE_OMIT_AUTOVACUUM
    if( pBt->autoVacuum ){
      checkPtrmap(pCheck, pgno, PTRMAP_BTREE, iPage, zContext);
    }
#endif
    checkTreePage(pCheck, pgno, zContext, NULL, !pPage->nCell ? NULL : &nMaxKey);
  }
 
  /* For intKey leaf pages, check that the min/max keys are in order
  ** with any left/parent/right pages. 
  ** 对于intKey叶子页，对left/parent/right页有序核对min/max关键字.
  */
  if( pPage->leaf && pPage->intKey ){
    /* if we are a left child page */  //若是在左孩子页上
    if( pnParentMinKey ){
      /* if we are the left most child page */  //如果在最左孩子页上
      if( !pnParentMaxKey ){
        if( nMaxKey > *pnParentMinKey ){
          checkAppendMsg(pCheck, zContext, 
              "Rowid %lld out of order (max larger than parent min of %lld)",
              nMaxKey, *pnParentMinKey);
        }
      }else{
        if( nMinKey <= *pnParentMinKey ){
          checkAppendMsg(pCheck, zContext, 
              "Rowid %lld out of order (min less than parent min of %lld)",
              nMinKey, *pnParentMinKey);
        }
        if( nMaxKey > *pnParentMaxKey ){
          checkAppendMsg(pCheck, zContext, 
              "Rowid %lld out of order (max larger than parent max of %lld)",
              nMaxKey, *pnParentMaxKey);
        }
        *pnParentMinKey = nMaxKey;
      }
    /* else if we're a right child page */  //在右孩子页上
    } else if( pnParentMaxKey ){
      if( nMinKey <= *pnParentMaxKey ){
        checkAppendMsg(pCheck, zContext, 
            "Rowid %lld out of order (min less than parent max of %lld)",
            nMinKey, *pnParentMaxKey);
      }
    }
  }

  /* Check for complete coverage of the page .*/ //核对页被完全覆盖
  data = pPage->aData;
  hdr = pPage->hdrOffset;
  hit = sqlite3PageMalloc( pBt->pageSize );  //从缓冲区获得空间分配给页
  if( hit==0 ){
    pCheck->mallocFailed = 1;
  }else{
    int contentOffset = get2byteNotZero(&data[hdr+5]);
    assert( contentOffset<=usableSize );  /* Enforced by btreeInitPage() */ //被btreeInitPage()强制执行
    memset(hit+contentOffset, 0, usableSize-contentOffset);
    memset(hit, 1, contentOffset);
    nCell = get2byte(&data[hdr+3]);
    cellStart = hdr + 12 - 4*pPage->leaf;
    for(i=0; i<nCell; i++){
      int pc = get2byte(&data[cellStart+i*2]);
      u32 size = 65536;
      int j;
      if( pc<=usableSize-4 ){
        size = cellSizePtr(pPage, &data[pc]); //计算一个Cell需要的总的字节数
      }
      if( (int)(pc+size-1)>=usableSize ){
        checkAppendMsg(pCheck, 0, 
            "Corruption detected in cell %d on page %d",i,iPage);
      }else{
        for(j=pc+size-1; j>=pc; j--) hit[j]++;
      }
    }
    i = get2byte(&data[hdr+1]);
    while( i>0 ){
      int size, j;
      assert( i<=usableSize-4 );     /* Enforced by btreeInitPage() */  //被btreeInitPage()强制执行
      size = get2byte(&data[i+2]);
      assert( i+size<=usableSize );  /* Enforced by btreeInitPage() */  //被btreeInitPage()强制执行
      for(j=i+size-1; j>=i; j--) hit[j]++;
      j = get2byte(&data[i]);
      assert( j==0 || j>i+size );  /* Enforced by btreeInitPage() */  //被btreeInitPage()强制执行
      assert( j<=usableSize-4 );   /* Enforced by btreeInitPage() */  //被btreeInitPage()强制执行
      i = j;
    }
    for(i=cnt=0; i<usableSize; i++){
      if( hit[i]==0 ){
        cnt++;
      }else if( hit[i]>1 ){
        checkAppendMsg(pCheck, 0,
          "Multiple uses for byte %d of page %d", i, iPage);
        break;
      }
    }
    if( cnt!=data[hdr+7] ){
      checkAppendMsg(pCheck, 0, 
          "Fragmentation of %d bytes reported as %d on page %d",
          cnt, data[hdr+7], iPage);
    }
  }
  sqlite3PageFree(hit);   //释放从sqlite3PageMalloc()获得的缓冲区
  releasePage(pPage);     //释放内存页
  return depth+1;
}
#endif /* SQLITE_OMIT_INTEGRITY_CHECK */

#ifndef SQLITE_OMIT_INTEGRITY_CHECK
/*
** This routine does a complete check of the given BTree file.  aRoot[] is
** an array of pages numbers were each page number is the root page of
** a table.  nRoot is the number of entries in aRoot.
**
** A read-only or read-write transaction must be opened before calling
** this function.
**
** Write the number of error seen in *pnErr.  Except for some memory
** allocation errors,  an error message held in memory obtained from
** malloc is returned if *pnErr is non-zero.  If *pnErr==0 then NULL is
** returned.  If a memory allocation error occurs, NULL is returned.
*/
/* 该函数对BTree文件做一个完全的检查。aRoot[]是一个页码数组，数组中每一个页码都是表的根页。
** aRoot中的条目数量为nRoot。在调用这个函数之前，一个只读或者读写事务必须为打开状态。
** 写错误数量在*pnErr中可见。除了一些内存错误，如果*pnErr非零，一个从malloc中获得的错误信息保存在内存。*pnErr==0，
** 返回NULL.内存分配错误返回NULL.
*/
char *sqlite3BtreeIntegrityCheck(    //对BTree文件做一个完整性的检查
  Btree *p,     /* The btree to be checked */                             //要被检查的B树
  int *aRoot,   /* An array of root pages numbers for individual trees */ //一个树的根页码数组
  int nRoot,    /* Number of entries in aRoot[] */                        //aRoot[]中的条目数
  int mxErr,    /* Stop reporting errors after this many */               //达到这个数之后停止报错
  int *pnErr    /* Write number of errors seen to this variable */        //错误数赋给该变量
){
  Pgno i;
  int nRef;
  IntegrityCk sCheck;
  BtShared *pBt = p->pBt;
  char zErr[100];

  sqlite3BtreeEnter(p);
  assert( p->inTrans>TRANS_NONE && pBt->inTransaction>TRANS_NONE );
  nRef = sqlite3PagerRefcount(pBt->pPager);
  sCheck.pBt = pBt;
  sCheck.pPager = pBt->pPager;
  sCheck.nPage = btreePagecount(sCheck.pBt);  //返回页中数据库文件(页)的大小
  sCheck.mxErr = mxErr;
  sCheck.nErr = 0;
  sCheck.mallocFailed = 0;
  *pnErr = 0;
  if( sCheck.nPage==0 ){
    sqlite3BtreeLeave(p);
    return 0;
  }

  sCheck.aPgRef = sqlite3MallocZero((sCheck.nPage / 8)+ 1);  //分配并清零
  if( !sCheck.aPgRef ){
    *pnErr = 1;
    sqlite3BtreeLeave(p);
    return 0;
  }
  i = PENDING_BYTE_PAGE(pBt);
  if( i<=sCheck.nPage ) setPageReferenced(&sCheck, i); 
  sqlite3StrAccumInit(&sCheck.errMsg, zErr, sizeof(zErr), 20000);  //初始化一个字符串累加器
  sCheck.errMsg.useMalloc = 2;

  /* Check the integrity of the freelist */   //核对空闲列表的完整性
  checkList(&sCheck, 1, get4byte(&pBt->pPage1->aData[32]),
            get4byte(&pBt->pPage1->aData[36]), "Main freelist: ");

  /* Check all the tables.*/                 //核对所有表
  for(i=0; (int)i<nRoot && sCheck.mxErr; i++){
    if( aRoot[i]==0 ) continue;
#ifndef SQLITE_OMIT_AUTOVACUUM
    if( pBt->autoVacuum && aRoot[i]>1 ){
      checkPtrmap(&sCheck, aRoot[i], PTRMAP_ROOTPAGE, 0, 0);
    }
#endif
    checkTreePage(&sCheck, aRoot[i], "List of tree roots: ", NULL, NULL);  //在树的一个单独的页上进行检查
  }

  /* Make sure every page in the file is referenced .*/  //确保在文件中的每个页都被引用 
  for(i=1; i<=sCheck.nPage && sCheck.mxErr; i++){
#ifdef SQLITE_OMIT_AUTOVACUUM
    if( getPageReferenced(&sCheck, i)==0 ){
      checkAppendMsg(&sCheck, 0, "Page %d is never used", i);
    }
#else
    /* If the database supports auto-vacuum, make sure no tables contain
    ** references to pointer-map pages.
	** 如果数据库支持自动清理，确保没有表包含对指针位图页的引用。
    */
    if( getPageReferenced(&sCheck, i)==0 && 
       (PTRMAP_PAGENO(pBt, i)!=i || !pBt->autoVacuum) ){
      checkAppendMsg(&sCheck, 0, "Page %d is never used", i);
    }
    if( getPageReferenced(&sCheck, i)!=0 && 
       (PTRMAP_PAGENO(pBt, i)==i && pBt->autoVacuum) ){
      checkAppendMsg(&sCheck, 0, "Pointer map page %d is referenced", i);
    }
#endif
  }

  /* Make sure this analysis did not leave any unref() pages.
  ** This is an internal consistency check; an integrity check
  ** of the integrity check.
  */
  /*确保分析不遗漏unref()的页。这是内部一致性检查，完整性检查。*/
  if( NEVER(nRef != sqlite3PagerRefcount(pBt->pPager)) ){
    checkAppendMsg(&sCheck, 0, 
      "Outstanding page count goes from %d to %d during this analysis",
      nRef, sqlite3PagerRefcount(pBt->pPager)
    );
  }

  /* Clean  up and report errors.*/  //清理并报错
  sqlite3BtreeLeave(p);
  sqlite3_free(sCheck.aPgRef);
  if( sCheck.mallocFailed ){
    sqlite3StrAccumReset(&sCheck.errMsg); //重置一个StrAccum类型的字符，并且回收所有分配的内存。
    *pnErr = sCheck.nErr+1;
    return 0;
  }
  *pnErr = sCheck.nErr;
  if( sCheck.nErr==0 ) sqlite3StrAccumReset(&sCheck.errMsg);
  return sqlite3StrAccumFinish(&sCheck.errMsg);
}
#endif /* SQLITE_OMIT_INTEGRITY_CHECK */

/*
** Return the full pathname of the underlying database file.  Return
** an empty string if the database is in-memory or a TEMP database.
**
** The pager filename is invariant as long as the pager is
** open so it is safe to access without the BtShared mutex.
*/
/* 返回底层数据库文件中完整的路径名。如果该数据库为内存数据库，或者为临时数据库，
** 返回空的字符串。pager的文件名是不变的只要pager是开放的，因此没有BtShared互斥锁也能安全访问.
*/
const char *sqlite3BtreeGetFilename(Btree *p){    //返回底层数据库文件中完整的路径名
  assert( p->pBt->pPager!=0 );
  return sqlite3PagerFilename(p->pBt->pPager, 1);
}

/*
** Return the pathname of the journal file for this database. The return
** value of this routine is the same regardless of whether the journal file
** has been created or not.
**
** The pager journal filename is invariant as long as the pager is
** open so it is safe to access without the BtShared mutex.
*/
/*
** 返回数据库中日志文件的路径名。无论日志文件是否被创建，程序返回值相同。
** pager日志文件名是不变的只要pager开放，因此没有BtShared互斥锁也能安全访问.
*/
const char *sqlite3BtreeGetJournalname(Btree *p){  //返回数据库中日志文件的路径名
  assert( p->pBt->pPager!=0 );
  return sqlite3PagerJournalname(p->pBt->pPager);
}

/*
** Return non-zero if a transaction is active.  
** 事务在运行，返回非零
*/
int sqlite3BtreeIsInTrans(Btree *p){     //是否在事务中
  assert( p==0 || sqlite3_mutex_held(p->db->mutex) );
  return (p && (p->inTrans==TRANS_WRITE));
}

#ifndef SQLITE_OMIT_WAL
/*
** Run a checkpoint on the Btree passed as the first argument.
**
** Return SQLITE_LOCKED if this or any other connection has an open 
** transaction on the shared-cache the argument Btree is connected to.
**
** Parameter eMode is one of SQLITE_CHECKPOINT_PASSIVE, FULL or RESTART.
*/
/*
** 执行B树上的检查点作为第一个参数传递。如果这个或任何其他连接
** 有一个开放的事务，返回SQLITE_LOCKED，事务在被B树连接的共享在参数上。
** 参数eMode为SQLITE_CHECKPOINT_PASSIVE, FULL or RESTART之一。
*/
int sqlite3BtreeCheckpoint(Btree *p, int eMode, int *pnLog, int *pnCkpt){ //执行B树上的检查点作为第一个参数传递
  int rc = SQLITE_OK;
  if( p ){
    BtShared *pBt = p->pBt;
    sqlite3BtreeEnter(p);
    if( pBt->inTransaction!=TRANS_NONE ){
      rc = SQLITE_LOCKED;
    }else{
      rc = sqlite3PagerCheckpoint(pBt->pPager, eMode, pnLog, pnCkpt);
    }
    sqlite3BtreeLeave(p);
  }
  return rc;
}
#endif

/*
** Return non-zero if a read (or write) transaction is active.
** 如果读或写事务在活动，返回非零
*/
int sqlite3BtreeIsInReadTrans(Btree *p){
  assert( p );
  assert( sqlite3_mutex_held(p->db->mutex) );
  return p->inTrans!=TRANS_NONE;
}

int sqlite3BtreeIsInBackup(Btree *p){
  assert( p );
  assert( sqlite3_mutex_held(p->db->mutex) );
  return p->nBackup!=0;
}

/*
** This function returns a pointer to a blob of memory associated with
** a single shared-btree. The memory is used by client code for its own
** purposes (for example, to store a high-level schema associated with 
** the shared-btree). The btree layer manages reference counting issues.
** 这个函数返回一个指向与一个单独的共享B树相关的内存中的blob。内存被客户端代码
** 使用(例如,存储一个与shared-btree相关的高级模式)。btree层管理引用计数问题。
** The first time this is called on a shared-btree, nBytes bytes of memory
** are allocated, zeroed, and returned to the caller. For each subsequent 
** call the nBytes parameter is ignored and a pointer to the same blob
** of memory returned. 
** 这个函数是第一次在共享B树上调用。内存的nBytes个字节被分配，清零，并返回到调用者.
** 对于每后来的调用nBytes字节都被忽略并且指针指向内存返回的相同blob对象.
** If the nBytes parameter is 0 and the blob of memory has not yet been
** allocated, a null pointer is returned. If the blob has already been
** allocated, it is returned as normal.
** 如果nBytes参数是0并且内存blob尚未分配,将返回一个空指针。如果该blob已经分配,它是正常返回。
** Just before the shared-btree is closed, the function passed as the 
** xFree argument when the memory allocation was made is invoked on the 
** blob of allocated memory. The xFree function should not call sqlite3_free()
** on the memory, the btree layer does that.
** 在shared-btree关闭之前,内存分配时函数传递作为xFree参数在内存分配的blob上被调用。在内存上，xFree函数
** 不能调用sqlite3_free()，在btree层可以调用。
*/
/*
函数返回一个指向blob型内存。内存用作客户端代码。在B树层管理引用计数的问题。
第一次在所谓的共享B树上被调用，为nbytes字节的内存被分配。
*/
void *sqlite3BtreeSchema(Btree *p, int nBytes, void(*xFree)(void *)){
  BtShared *pBt = p->pBt;
  sqlite3BtreeEnter(p);
  if( !pBt->pSchema && nBytes ){
    pBt->pSchema = sqlite3DbMallocZero(0, nBytes);
    pBt->xFreeSchema = xFree;
  }
  sqlite3BtreeLeave(p);
  return pBt->pSchema;
}

/*
** Return SQLITE_LOCKED_SHAREDCACHE if another user of the same shared 
** btree as the argument handle holds an exclusive lock on the 
** sqlite_master table. Otherwise SQLITE_OK.
*/
/*如果另一个用户在共享B树上有一个排他锁，返回SQLITE_LOCKED_SHAREDCACHE。*/
int sqlite3BtreeSchemaLocked(Btree *p){    //B树模式锁
  int rc;
  assert( sqlite3_mutex_held(p->db->mutex) );
  sqlite3BtreeEnter(p);
  rc = querySharedCacheTableLock(p, MASTER_ROOT, READ_LOCK);
  assert( rc==SQLITE_OK || rc==SQLITE_LOCKED_SHAREDCACHE );
  sqlite3BtreeLeave(p);
  return rc;
}


#ifndef SQLITE_OMIT_SHARED_CACHE
/*
** Obtain a lock on the table whose root page is iTab.  The
** lock is a write lock if isWritelock is true or a read lock
** if it is false.
*/
/*获得表的根页iTab上的锁，isWriteLock=1是写锁，等于0为读锁。*/
int sqlite3BtreeLockTable(Btree *p, int iTab, u8 isWriteLock){   //获得表的根页iTab上的锁
  int rc = SQLITE_OK;
  assert( p->inTrans!=TRANS_NONE );
  if( p->sharable ){
    u8 lockType = READ_LOCK + isWriteLock;
    assert( READ_LOCK+1==WRITE_LOCK );
    assert( isWriteLock==0 || isWriteLock==1 );

    sqlite3BtreeEnter(p);
    rc = querySharedCacheTableLock(p, iTab, lockType);//查看Btree句柄p是否在具有根页iTab的表上获得了lockType类型
    if( rc==SQLITE_OK ){
      rc = setSharedCacheTableLock(p, iTab, lockType); //通过B树句柄p在根页iTable的表上添加锁到共享B树
    }
    sqlite3BtreeLeave(p);
  }
  return rc;
}
#endif

#ifndef SQLITE_OMIT_INCRBLOB
/*
** Argument pCsr must be a cursor opened for writing on an 
** INTKEY table currently pointing at a valid table entry. 
** This function modifies the data stored as part of that entry.
** 参数pCsr必须是一个打开的游标，等待在I当前NTKEY表上指向一个有效的表条目。
** 这个函数修改存储的数据作为条目的一部分。
** Only the data content may only be modified, it is not possible to 
** change the length of the data stored. If this function is called with
** parameters that attempt to write past the end of the existing data,
** no modifications are made and SQLITE_CORRUPT is returned.
** 只有数据内容可能修改,修改存储的数据的长度是不可能的。如果这个函数被调用
** 参数尝试写到现有数据的末尾,没有修改并返回SQLITE_CORRUPT。
*/
/*仅仅数据内容能够被修改，不可能改变数据存储的长度。*/
int sqlite3BtreePutData(BtCursor *pCsr, u32 offset, u32 amt, void *z){  //修改数据内容
  int rc;
  assert( cursorHoldsMutex(pCsr) );
  assert( sqlite3_mutex_held(pCsr->pBtree->db->mutex) );
  assert( pCsr->isIncrblobHandle );

  rc = restoreCursorPosition(pCsr);
  if( rc!=SQLITE_OK ){
    return rc;
  }
  assert( pCsr->eState!=CURSOR_REQUIRESEEK );
  if( pCsr->eState!=CURSOR_VALID ){
    return SQLITE_ABORT;
  }

  /* Check some assumptions: 
  **   (a) the cursor is open for writing,        //等待游标开放
  **   (b) there is a read/write transaction open, //有读或写事务开放
  **   (c) the connection holds a write-lock on the table (if required),  //在表上上句酷连接持有写锁
  **   (d) there are no conflicting read-locks, and                       //没有冲突读锁
  **   (e) the cursor points at a valid row of an intKey table.           //游标指向intKey表的有效行
  */
  if( !pCsr->wrFlag ){
    return SQLITE_READONLY;
  }
  assert( (pCsr->pBt->btsFlags & BTS_READ_ONLY)==0
              && pCsr->pBt->inTransaction==TRANS_WRITE );/*游标打开，写事务*/
  assert( hasSharedCacheTableLock(pCsr->pBtree, pCsr->pgnoRoot, 0, 2) );
  assert( !hasReadConflicts(pCsr->pBtree, pCsr->pgnoRoot) );/*读锁没冲突*/
  assert( pCsr->apPage[pCsr->iPage]->intKey );

  return accessPayload(pCsr, offset, amt, (unsigned char *)z, 1);  //读或覆写有效载荷信息
}

/* 
** Set a flag on this cursor to cache the locations of pages from the 
** overflow list for the current row. This is used by cursors opened
** for incremental blob IO only.
** 对于当前行，在这个游标缓存的页面的位置设置一个标志。标志仅被对增量blob IO开放的游标使用。
** This function sets a flag only. The actual page location cache
** (stored in BtCursor.aOverflow[]) is allocated and used by function
** accessPayload() (the worker function for sqlite3BtreeData() and
** sqlite3BtreePutData()).
** 这个函数只设置一个标志。实际页面位置缓存(存储在BtCursor.aOverflow[])分配和被accessPayload()使用
** (对函数sqlite3BtreeData()和sqlite3BtreePutData()有效)。
*/  /*此函数在游标上设置一个标志，缓存溢出列表上的页*/
void sqlite3BtreeCacheOverflow(BtCursor *pCur){  //此函数在游标上设置一个溢出页缓存标志
  assert( cursorHoldsMutex(pCur) );
  assert( sqlite3_mutex_held(pCur->pBtree->db->mutex) );
  invalidateOverflowCache(pCur);
  pCur->isIncrblobHandle = 1;
}
#endif

/*
** Set both the "read version" (single byte at byte offset 18) and 
** "write version" (single byte at byte offset 19) fields in the database
** header to iVersion.
** 在数据库头部设置"read version"(在偏移量18处的单字节处)和"write version"(在偏移量19处的单字节处)域
*/
int sqlite3BtreeSetVersion(Btree *pBtree, int iVersion){  //在数据库头部设置"读版本"和"写版本"域
  BtShared *pBt = pBtree->pBt;
  int rc;                         /* Return code */
 
  assert( iVersion==1 || iVersion==2 );

  /* If setting the version fields to 1, do not automatically open the
  ** WAL connection, even if the version fields are currently set to 2.
  */
  pBt->btsFlags &= ~BTS_NO_WAL;
  if( iVersion==1 ) pBt->btsFlags |= BTS_NO_WAL;/*没有打开预写日志连接*/

  rc = sqlite3BtreeBeginTrans(pBtree, 0);
  if( rc==SQLITE_OK ){
    u8 *aData = pBt->pPage1->aData;
    if( aData[18]!=(u8)iVersion || aData[19]!=(u8)iVersion ){
      rc = sqlite3BtreeBeginTrans(pBtree, 2);
      if( rc==SQLITE_OK ){
        rc = sqlite3PagerWrite(pBt->pPage1->pDbPage);
        if( rc==SQLITE_OK ){
          aData[18] = (u8)iVersion;/*18是读版本*/
          aData[19] = (u8)iVersion;/*19是写版本*/
        }
      }
    }
  }

  pBt->btsFlags &= ~BTS_NO_WAL;
  return rc;
}

/*
** set the mask of hint flags for cursor pCsr. Currently the only valid
** values are 0 and BTREE_BULKLOAD.
** 设置游标pCsr掩码。当前唯一有效值是0,且BTREE_BULKLOAD。
*/
void sqlite3BtreeCursorHints(BtCursor *pCsr, unsigned int mask){
  assert( mask==BTREE_BULKLOAD || mask==0 );/*设置掩码mask=BTREE_BULKLOAD 或0*/
  pCsr->hints = mask;
}
