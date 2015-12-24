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
** This file contains the C functions that implement mutexes.
**这个文档包含实现互斥锁的C 函数
**
** This file contains code that is common across all mutex implementations.
**这个文档包含共同所有互斥锁的代码实现。
*/
#include "sqliteInt.h"

#if defined(SQLITE_DEBUG) && !defined(SQLITE_MUTEX_OMIT)
/////*用于调试目的,当互斥锁子系统进行初始化的和未初始化时记录,
////以便如果有试图分配一个互斥锁而系统未初始化时我们可以assert()。
////如果定义了SQLITE_DEBUG且没有定义SQLITE_MUTEX_OMIT，则对mutexIsInit（为静态常量）初始化为零。
*/
/*
** For debugging purposes, record when the mutex subsystem is initialized
** and uninitialized so that we can assert() if there is an attempt to
** allocate a mutex while the system is uninitialized.
**用于调试目的,记录时,互斥锁子系统初始化的和未初始化,
**以便我们可以断言()如果有试图分配一个互斥而系统未初始化。
**
**
*/
static SQLITE_WSD int mutexIsInit = 0;
#endif /* SQLITE_DEBUG */


#ifndef SQLITE_MUTEX_OMIT
/*
** Initialize the mutex system.如果没有定义SQLITE_MUTEX_OMIT就初始化互斥系统
*/
int sqlite3MutexInit(void){ 
  int rc = SQLITE_OK;
  if( !sqlite3GlobalConfig.mutex.xMutexAlloc ){
    /* If the xMutexAlloc method has not been set, then the user did not
    ** install a mutex implementation via sqlite3_config() prior to 
    ** sqlite3_initialize() being called. This block copies pointers to
    ** the default implementation into the sqlite3GlobalConfig structure.
    **如果xMutexAlloc 函数没设置，
    **那么,用户没有安装一个互斥对象实现通过sqlite3_config () 
    **sqlite3_initialize()前被调用。
   ** 这段复制指针向sqlite3GlobalConfig默认实现结构。
    */
/////*如果xMutexAlloc 函数没被设置（还没分配互斥锁），则用户没有安装一个经由先验sqlite3_config()到
////   sqlite3_initialize()的互斥的实现。
////  这一块的副本的指针的默认实现为sqlite3globalconfig结构
*/
    sqlite3_mutex_methods const *pFrom;
    sqlite3_mutex_methods *pTo = &sqlite3GlobalConfig.mutex;

    if( sqlite3GlobalConfig.bCoreMutex ){   //// 启用核心互斥锁
      pFrom = sqlite3DefaultMutex();///// 为多线程调用   sqlite3_mutex_methods const *sqlite3DefaultMutex(void);
    }else{
      pFrom = sqlite3NoopMutex();///// 为单线程调用   sqlite3_mutex_methods const *sqlite3NoopMutex(void);
    }
    memcpy(pTo, pFrom, offsetof(sqlite3_mutex_methods, xMutexAlloc));/////将偏移量为offsetof(sqlite3_mutex_methods, xMutexAlloc)的pFrom赋给pTo的开始部分    xMutexAlloc为一个互斥锁的地址
    memcpy(&pTo->xMutexFree, &pFrom->xMutexFree,
           sizeof(*pTo) - offsetof(sqlite3_mutex_methods, xMutexFree));
    pTo->xMutexAlloc = pFrom->xMutexAlloc;
  }
  rc = sqlite3GlobalConfig.mutex.xMutexInit();//////*The xMutexInit method defined by this structure is invoked
/////// as part of system initialization by the sqlite3_initialize() function. 
///////The xMutexInit routine is called by SQLite exactly once for each effective call to sqlite3_initialize().
//////xMutexInit方法是被sqlite3_initialize()系统初始化的一部分。当每个有效的调用sqlite3_initialize()时调用xMutexInit方法
*/

#ifdef SQLITE_DEBUG
  GLOBAL(int, mutexIsInit) = 1;
#endif

  return rc;
}

/*
** Shutdown the mutex system. This call frees resources allocated by
** sqlite3MutexInit().
**关闭互斥系统。释放sqlite3MutexInit()分配的资源。
*/
int sqlite3MutexEnd(void){
  int rc = SQLITE_OK;
  if( sqlite3GlobalConfig.mutex.xMutexEnd ){
    rc = sqlite3GlobalConfig.mutex.xMutexEnd();///////*The implementation of this method is expected to release all outstanding resources obtained by the mutex methods 
						//////implementation, especially those obtained by the xMutexInit method.
						//////该方法的实现是为了释放所有被互斥方法实现获得的资源，尤其是被xMutexInit（）方法获得的资源。
						*/
  }

#ifdef SQLITE_DEBUG
  GLOBAL(int, mutexIsInit) = 0;
#endif

  return rc;
}

/*
** Retrieve a pointer to a static mutex or allocate a new dynamic one.
**获取一个指向静态互斥锁或分配一个新的动态互斥锁。
*/
//////*重新为静态互斥锁获取一个指针，或者分配一个新歌动态互斥锁
sqlite3_mutex *sqlite3_mutex_alloc(int id){
#ifndef SQLITE_OMIT_AUTOINIT
  if( sqlite3_initialize() ) return 0;
#endif
  return sqlite3GlobalConfig.mutex.xMutexAlloc(id);
}

sqlite3_mutex *sqlite3MutexAlloc(int id){
  if( !sqlite3GlobalConfig.bCoreMutex ){
    return 0;
  }
  assert( GLOBAL(int, mutexIsInit) );
  return sqlite3GlobalConfig.mutex.xMutexAlloc(id);
}

/*
** Free a dynamic mutex.释放一个动态互斥锁
*/
void sqlite3_mutex_free(sqlite3_mutex *p){
  if( p ){
    sqlite3GlobalConfig.mutex.xMutexFree(p);
  }
}

/*
** Obtain the mutex p. If some other thread already has the mutex, block
** until it can be obtained.
**获得互斥锁p。
**如果其他线程已经拥有互斥锁,
**那么就将其阻塞,直到它可以获得。
*/
////*获取互斥锁p
//// 如果其他线程已经拥有该互斥锁，则将其阻塞，直到其他线程将该锁释放
////  且请求该锁的优先级较高才能获得该锁
*/
void sqlite3_mutex_enter(sqlite3_mutex *p){
  if( p ){
    sqlite3GlobalConfig.mutex.xMutexEnter(p);
  }
}

/*
** Obtain the mutex p. If successful, return SQLITE_OK. Otherwise, if another
** thread holds the mutex and it cannot be obtained, return SQLITE_BUSY.
**获得互斥锁p。如果成功,返回SQLITE_OK。
**否则,如果另一个线程持有互斥锁,它不能得到,就返回SQLITE_BUSY。
*/
////*获取互斥锁p
//// 如果能获得该锁，则返回SQLITE_OK。
//// 否则，如果其他线程持有该锁，则该请求者不能获得该锁，返回SQLITE_BUSY
int sqlite3_mutex_try(sqlite3_mutex *p){
  int rc = SQLITE_OK;
  if( p ){
    return sqlite3GlobalConfig.mutex.xMutexTry(p);
  }
  return rc;
}

/*
** The sqlite3_mutex_leave() routine exits a mutex that was previously
** entered by the same thread.  The behavior is undefined if the mutex 
** is not currently entered. If a NULL pointer is passed as an argument
** this function is a no-op.
**sqlite3_mutex_leave() 程序退出之前由相同的线程输入的互斥对象。
**这个行为没被定义，如果该互斥锁不是当前输入的。
**如果一个空指针作为参数传递给该函数，那么就不做任何操作
**
*/
////*sqlite3_mutex_leave()函数释放一个互斥锁，该锁之前有同一个线程引入的。
//// 如果该互斥锁不是当前输入的，则这个行为是没定义的。
//// 如果传入的参数p为空指针，则该函数不做任何操作
*/
void sqlite3_mutex_leave(sqlite3_mutex *p){
  if( p ){
    sqlite3GlobalConfig.mutex.xMutexLeave(p);
  }
}

#ifndef NDEBUG
/*
** The sqlite3_mutex_held() and sqlite3_mutex_notheld() routine are
** intended for use inside assert() statements.
**sqlite3_mutex_held() 和sqlite3_mutex_notheld() 函数用于内部assert() 语句
**
*/
////* 当持有互斥锁时，返回 p==0 或 sqlite3GlobalConfig.mutex.xMutexHeld(p);
*/
int sqlite3_mutex_held(sqlite3_mutex *p){
  return p==0 || sqlite3GlobalConfig.mutex.xMutexHeld(p);
}

////* 当不持有互斥锁时，返回 p==0 或 sqlite3GlobalConfig.mutex.xMutexNotheld(p);
*/
int sqlite3_mutex_notheld(sqlite3_mutex *p){
  return p==0 || sqlite3GlobalConfig.mutex.xMutexNotheld(p);
}
#endif

#endif /* !defined(SQLITE_MUTEX_OMIT) */

