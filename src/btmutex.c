/*
** 2007 August 27
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
** This file contains code used to implement mutexes on Btree objects.
** This code really belongs in btree.c.  But btree.c is getting too
** big and we want to break it down some.  This packaged seemed like
** a good breakout.
** 该文件包含的代码是用来在B树对象上实现锁机制。这个代码是属于btree.c文件。
** 但是btree.c文件变得太大，我们想从中分出一部分。这个包装看起来一个好的突破。
*/
#include "btreeInt.h"
#ifndef SQLITE_OMIT_SHARED_CACHE
#if SQLITE_THREADSAFE

/*
** Obtain the BtShared mutex associated with B-Tree handle p. Also,
** set BtShared.db to the database handle associated with p and the
** p->locked boolean to true.
** 获得与B树句柄p相关联的BtShared互斥量。设定BtShared.db与p关联的数据库句柄并且设置布尔变量p->locked为真。
*/
static void lockBtreeMutex(Btree *p){
  assert( p->locked==0 );
  assert( sqlite3_mutex_notheld(p->pBt->mutex) );
  assert( sqlite3_mutex_held(p->db->mutex) );

  sqlite3_mutex_enter(p->pBt->mutex);
  p->pBt->db = p->db;
  p->locked = 1;
}

/*
** Release the BtShared mutex associated with B-Tree handle p and
** clear the p->locked boolean.
** 释放与B树句柄p相关联的BtShared互斥量并且将布尔变量p->locked清零。
*/
static void unlockBtreeMutex(Btree *p){
  BtShared *pBt = p->pBt;
  assert( p->locked==1 );
  assert( sqlite3_mutex_held(pBt->mutex) );
  assert( sqlite3_mutex_held(p->db->mutex) );
  assert( p->db==pBt->db );

  sqlite3_mutex_leave(pBt->mutex);
  p->locked = 0;
}

/*
** Enter a mutex on the given BTree object.
** 在给定的B树对象上输入一个互斥锁。
** If the object is not sharable, then no mutex is ever required
** and this routine is a no-op.  The underlying mutex is non-recursive.
** But we keep a reference count in Btree.wantToLock so the behavior
** of this interface is recursive.
**
** To avoid deadlocks, multiple Btrees are locked in the same order
** by all database connections.  The p->pNext is a list of other
** Btrees belonging to the same database connection as the p Btree
** which need to be locked after p.  If we cannot get a lock on
** p, then first unlock all of the others on p->pNext, then wait
** for the lock to become available on p, then relock all of the
** subsequent Btrees that desire a lock.
** 如果对象不可共享，那么不需要互斥锁并且这个程序是无操作的。这个潜在的互斥锁是非递归的。
** 但是在Btree.wantToLock中保持对参数的计数，因此这歌接口的操作是递归的。
** 
** 为了避免死锁，多个btree以相同的顺序被所有数据库连接锁定。p->pNext是其他属于相同数据库连接的B树列表
** 作为B树p，p->pNext需要在p后被锁。如果在p上不能的锁，那么首先在p->pNext上解除其他所有锁，然后等待
** 直到在p上的锁可用，接着对需要加锁的所有后续B树重新加锁。
*/
void sqlite3BtreeEnter(Btree *p){
  Btree *pLater;

  /* Some basic sanity checking on the Btree.  The list of Btrees
  ** connected by pNext and pPrev should be in sorted order by
  ** Btree.pBt value. All elements of the list should belong to
  ** the same connection. Only shared Btrees are on the list. 
  ** 在B树上进行基本的完整性检查。通过pNext和pPrev连接的B树列表应该
  ** 按Btree.pBt的值进行排序。列表的所有元素都应属于相同的连接。只有共享的B树在列表中。
  */
  assert( p->pNext==0 || p->pNext->pBt>p->pBt );
  assert( p->pPrev==0 || p->pPrev->pBt<p->pBt );
  assert( p->pNext==0 || p->pNext->db==p->db );
  assert( p->pPrev==0 || p->pPrev->db==p->db );
  assert( p->sharable || (p->pNext==0 && p->pPrev==0) );

  /* Check for locking consistency 检查锁的一致性*/
  assert( !p->locked || p->wantToLock>0 );
  assert( p->sharable || p->wantToLock==0 );

  /* We should already hold a lock on the database connection 在数据库连接上应该已经持有一个锁*/
  assert( sqlite3_mutex_held(p->db->mutex) );

  /* Unless the database is sharable and unlocked, then BtShared.db
  ** should already be set correctly. 
  ** 如果数据库是可共享的并且没有被锁，那么BtShared.db应该已经被正确设置。
  */
  assert( (p->locked==0 && p->sharable) || p->pBt->db==p->db );

  if( !p->sharable ) return;
  p->wantToLock++;
  if( p->locked ) return;

  /* In most cases, we should be able to acquire the lock we
  ** want without having to go throught the ascending lock
  ** procedure that follows.  Just be sure not to block.
  ** 在大多数情况下，我们应该能得到锁，我们想要的这个锁不必通过追溯在其后的锁进程。只是对块不确定。
  */
  if( sqlite3_mutex_try(p->pBt->mutex)==SQLITE_OK ){
    p->pBt->db = p->db;
    p->locked = 1;
    return;
  }

  /* To avoid deadlock, first release all locks with a larger
  ** BtShared address.  Then acquire our lock.  Then reacquire
  ** the other BtShared locks that we used to hold in ascending
  ** order.
  ** 为了避免死锁，首先释放所有更大BtShared地址的锁。然后获得我们的锁。
  ** 接着再次获取我们曾在提升顺序中持有的其他锁。
  */
  for(pLater=p->pNext; pLater; pLater=pLater->pNext){
    assert( pLater->sharable );
    assert( pLater->pNext==0 || pLater->pNext->pBt>pLater->pBt );
    assert( !pLater->locked || pLater->wantToLock>0 );
    if( pLater->locked ){
      unlockBtreeMutex(pLater);
    }
  }
  lockBtreeMutex(p);
  for(pLater=p->pNext; pLater; pLater=pLater->pNext){
    if( pLater->wantToLock ){
      lockBtreeMutex(pLater);
    }
  }
}

/*
** Exit the recursive mutex on a Btree.  //在B树上退出互斥锁
*/
void sqlite3BtreeLeave(Btree *p){
  if( p->sharable ){
    assert( p->wantToLock>0 );
    p->wantToLock--;
    if( p->wantToLock==0 ){
      unlockBtreeMutex(p);
    }
  }
}

#ifndef NDEBUG
/*
** Return true if the BtShared mutex is held on the btree, or if the
** B-Tree is not marked as sharable.
**
** This routine is used only from within assert() statements.
** 如果在B树上持有BtShared互斥锁或者B树没有被标记为可共享则返回真。
** 这个程序只被用在assert()语句中。
*/
int sqlite3BtreeHoldsMutex(Btree *p){
  assert( p->sharable==0 || p->locked==0 || p->wantToLock>0 );
  assert( p->sharable==0 || p->locked==0 || p->db==p->pBt->db );
  assert( p->sharable==0 || p->locked==0 || sqlite3_mutex_held(p->pBt->mutex) );
  assert( p->sharable==0 || p->locked==0 || sqlite3_mutex_held(p->db->mutex) );

  return (p->sharable==0 || p->locked);
}
#endif


#ifndef SQLITE_OMIT_INCRBLOB
/*
** Enter and leave a mutex on a Btree given a cursor owned by that
** Btree.  These entry points are used by incremental I/O and can be
** omitted if that module is not used.
** 输入和离开一个Btree上的互斥锁，在Btree上有一个游标。这些入口指针被增量I/O使用。
** 而且如果这个模块未使用可以忽略。
*/
void sqlite3BtreeEnterCursor(BtCursor *pCur){
  sqlite3BtreeEnter(pCur->pBtree);
}
void sqlite3BtreeLeaveCursor(BtCursor *pCur){
  sqlite3BtreeLeave(pCur->pBtree);
}
#endif /* SQLITE_OMIT_INCRBLOB */


/*
** Enter the mutex on every Btree associated with a database
** connection.  This is needed (for example) prior to parsing
** a statement since we will be comparing table and column names
** against all schemas and we do not want those schemas being
** reset out from under us.
**
** There is a corresponding leave-all procedures.
**
** Enter the mutexes in accending order by BtShared pointer address
** to avoid the possibility of deadlock when two threads with
** two or more btrees in common both try to lock all their btrees
** at the same instant.
** 在与数据库连接相关的每个B树上输入互斥锁。这需要提前分析语句，
** 因为我们将对所有模式比较表和列的名称。我们不希望这些模式手动重置。
** 有一个相应的leave-all程序。
** 通过BtShared指针地址在挂起序列中输入互斥量来避免死锁的可能性，
** 当两个线程使用共同的两个或多个B树在同一时刻都试图锁定所有的B树。
*/
void sqlite3BtreeEnterAll(sqlite3 *db){
  int i;
  Btree *p;
  assert( sqlite3_mutex_held(db->mutex) );
  for(i=0; i<db->nDb; i++){
    p = db->aDb[i].pBt;
    if( p ) sqlite3BtreeEnter(p);
  }
}
void sqlite3BtreeLeaveAll(sqlite3 *db){
  int i;
  Btree *p;
  assert( sqlite3_mutex_held(db->mutex) );
  for(i=0; i<db->nDb; i++){
    p = db->aDb[i].pBt;
    if( p ) sqlite3BtreeLeave(p);
  }
}

/*
** Return true if a particular Btree requires a lock.  Return FALSE if
** no lock is ever required since it is not sharable.
** 如果一个某个特定的B树需要一个锁，则返回true。如果因为不是可共享的而不需要锁时则返回false。
*/
int sqlite3BtreeSharable(Btree *p){
  return p->sharable;
}

#ifndef NDEBUG
/*
** Return true if the current thread holds the database connection
** mutex and all required BtShared mutexes.
**
** This routine is used inside assert() statements only.
** 如果当前线程持有数据库连接互斥锁并且都需要BtShared互斥锁则返回true.
** 这个这个程序仅被用在assert()语句中。
*/
int sqlite3BtreeHoldsAllMutexes(sqlite3 *db){
  int i;
  if( !sqlite3_mutex_held(db->mutex) ){
    return 0;
  }
  for(i=0; i<db->nDb; i++){
    Btree *p;
    p = db->aDb[i].pBt;
    if( p && p->sharable &&
         (p->wantToLock==0 || !sqlite3_mutex_held(p->pBt->mutex)) ){
      return 0;
    }
  }
  return 1;
}
#endif /* NDEBUG */

#ifndef NDEBUG
/*
** Return true if the correct mutexes are held for accessing the
** db->aDb[iDb].pSchema structure.  The mutexes required for schema
** access are:
**
**   (1) The mutex on db
**   (2) if iDb!=1, then the mutex on db->aDb[iDb].pBt.
**
** If pSchema is not NULL, then iDb is computed from pSchema and
** db using sqlite3SchemaToIndex().
** 如果对于正在访问db->aDb[iDb].pSchema结构的量正确持有互斥锁，则返回true。
** 互斥锁对模式访问的要求如下：
**  1）互斥变量在db上
**  2）如果iDb!=1，则互斥锁在 db->aDb[iDb].pBt上。
** 如果pSchema不为空那么从pSchema计算iDb并且db使用sqlite3SchemaToIndex().
*/
int sqlite3SchemaMutexHeld(sqlite3 *db, int iDb, Schema *pSchema){
  Btree *p;
  assert( db!=0 );
  if( pSchema ) iDb = sqlite3SchemaToIndex(db, pSchema);
  assert( iDb>=0 && iDb<db->nDb );
  if( !sqlite3_mutex_held(db->mutex) ) return 0;
  if( iDb==1 ) return 1;
  p = db->aDb[iDb].pBt;
  assert( p!=0 );
  return p->sharable==0 || p->locked==1;
}
#endif /* NDEBUG */

#else /* SQLITE_THREADSAFE>0 above.  SQLITE_THREADSAFE==0 below */
/*
** The following are special cases for mutex enter routines for use
** in single threaded applications that use shared cache.  Except for
** these two routines, all mutex operations are no-ops in that case and
** are null #defines in btree.h.
**
** If shared cache is disabled, then all btree mutex routines, including
** the ones below, are no-ops and are null #defines in btree.h.
** 下表面是互斥输入程序的特殊情况，这个程序使用共享缓存的单线程的应用中。
** 除了这两个程序，在那种情况下的所有的互斥操作都是无操作的，并且在btree.h头文件中无#defines.
*/

void sqlite3BtreeEnter(Btree *p){
  p->pBt->db = p->db;
}
void sqlite3BtreeEnterAll(sqlite3 *db){
  int i;
  for(i=0; i<db->nDb; i++){
    Btree *p = db->aDb[i].pBt;
    if( p ){
      p->pBt->db = p->db;
    }
  }
}
#endif /* if SQLITE_THREADSAFE */
#endif /* ifndef SQLITE_OMIT_SHARED_CACHE */
