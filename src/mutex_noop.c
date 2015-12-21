/*
** 2008 October 07
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
**这个文件包含实现互斥的C函数。
** This implementation in this file does not provide any mutual
** exclusion and is thus suitable for use only in applications
** that use SQLite in a single thread.  The routines defined
** here are place-holders.  Applications can substitute working
** mutex routines at start-time using the sqlite3_config(SQLITE_CONFIG_MUTEX,...)interface.
**在这个文件中的这个实现不提供任何相互的排斥,因此仅适合单线程使用SQLite的程序使用
**这里定义的例程的占位符。应用程序在开始可以使用sqlite3_config(SQLITE_CONFIG_MUTEX…)接口替代工作互斥动作。
**   
**
** 
**
** If compiled with SQLITE_DEBUG, then additional logic is inserted
** that does error checking on mutexes to make sure they are being
** called correctly.
**如果使用SQLITE_DEBUG编译,然后插入额外的逻辑代替互斥锁上的错误检测来确保他们被叫到是正确的。/
#include "sqliteInt.h"

#ifndef SQLITE_MUTEX_OMIT

#ifndef SQLITE_DEBUG
/*
** Stub routines for all mutex methods.
**所有互斥体实现的基本函数
** This routines provide no mutual exclusion or error checking.
**这个例程提供没有互斥或错误检查
*/
static int noopMutexInit(void){ return SQLITE_OK; }
static int noopMutexEnd(void){ return SQLITE_OK; }
static sqlite3_mutex *noopMutexAlloc(int id){ 
  UNUSED_PARAMETER(id);//指针转化(void)(id)

  return (sqlite3_mutex*)8;//默认返回8
 
}
static void noopMutexFree(sqlite3_mutex *p){ UNUSED_PARAMETER(p); return; }
static void noopMutexEnter(sqlite3_mutex *p){ UNUSED_PARAMETER(p); return; }
static int noopMutexTry(sqlite3_mutex *p){
  UNUSED_PARAMETER(p);
  return SQLITE_OK;
}
static void noopMutexLeave(sqlite3_mutex *p){ UNUSED_PARAMETER(p); return; }

sqlite3_mutex_methods const *sqlite3NoopMutex(void){
  static const sqlite3_mutex_methods sMutex = {
    noopMutexInit,
    noopMutexEnd,
    noopMutexAlloc,
    noopMutexFree,
    noopMutexEnter,
    noopMutexTry,
    noopMutexLeave,

    0,
    0,
  };

  return &sMutex;
}
#endif /* !SQLITE_DEBUG */

#ifdef SQLITE_DEBUG
/*
** In this implementation, error checking is provided for testing
** and debugging purposes.  The mutexes still do not provide any
** mutual exclusion.
*/
//在这个实现中,错误检查提供testingand调试。互斥锁仍然没有提供任何互斥。
/*
** The mutex object 互斥对象
*/
typedef struct sqlite3_debug_mutex { //调试专用互斥体

  int id;     /* The mutex type */
  int cnt;    /* Number of entries without a matching leave 条目的数值不匹配的离开*/
} sqlite3_debug_mutex;

/*
** The sqlite3_mutex_held() and sqlite3_mutex_notheld() routine are
** intended for use inside assert() statements.
**The sqlite3_mutex_held() and sqlite3_mutex_notheld()函数用于assert一些意外情况
*/
static int debugMutexHeld(sqlite3_mutex *pX){
  sqlite3_debug_mutex *p = (sqlite3_debug_mutex*)pX;
  return p==0 || p->cnt>0;
}
static int debugMutexNotheld(sqlite3_mutex *pX){
  sqlite3_debug_mutex *p = (sqlite3_debug_mutex*)pX;
  return p==0 || p->cnt==0;
}

/*
** Initialize and deinitialize the mutex subsystem.
**初始和销毁互斥体系统
*/
static int debugMutexInit(void){ return SQLITE_OK; }
static int debugMutexEnd(void){ return SQLITE_OK; }

/*
** The sqlite3_mutex_alloc() routine allocates a new
** mutex and returns a pointer to it.  If it returns NULL
** that means that a mutex could not be allocated. 
**sqlite3_mutex_alloc()例程分配一个新的互斥对象并返回一个指针指向它。如果返回空这意味着一个互斥锁不能分配。*/
static sqlite3_mutex *debugMutexAlloc(int id){
  static sqlite3_debug_mutex aStatic[6];
  sqlite3_debug_mutex *pNew = 0;
  switch( id ){
    case SQLITE_MUTEX_FAST:
    case SQLITE_MUTEX_RECURSIVE: {
      pNew = sqlite3Malloc(sizeof(*pNew));
      if( pNew ){
        pNew->id = id;
        pNew->cnt = 0;
      }
      break;
    }
    default: {
      assert( id-2 >= 0 );
      assert( id-2 < (int)(sizeof(aStatic)/sizeof(aStatic[0])) );
      pNew = &aStatic[id-2];
      pNew->id = id;
      break;
    }
  }
  return (sqlite3_mutex*)pNew;
}

/*
** This routine deallocates a previously allocated mutex.
** 这个函数用于重新分配之前分配的互斥
*/
static void debugMutexFree(sqlite3_mutex *pX){
  sqlite3_debug_mutex *p = (sqlite3_debug_mutex*)pX;
  assert( p->cnt==0 );
  assert( p->id==SQLITE_MUTEX_FAST || p->id==SQLITE_MUTEX_RECURSIVE );
  sqlite3_free(p);
}

/*
** The sqlite3_mutex_enter() and sqlite3_mutex_try() routines attempt
** to enter a mutex.  If another thread is already within the mutex,
** sqlite3_mutex_enter() will block and sqlite3_mutex_try() will return
** SQLITE_BUSY.  The sqlite3_mutex_try() interface returns SQLITE_OK
** upon successful entry.  Mutexes created using SQLITE_MUTEX_RECURSIVE can
** be entered multiple times by the same thread.  In such cases the,
** mutex must be exited an equal number of times before another thread
** can enter.  If the same thread tries to enter any other kind of mutex
** more than once, the behavior is undefined.
**The sqlite3_mutex_enter() and sqlite3_mutex_try()函数分配锁，忙则返回SQLITE_BUSY 否则分配锁 ，
**锁分配成功The sqlite3_mutex_try()接口返回SQLITE_OK。
**可以多次进入互斥使用SQLITE_MUTEX_RECURSIVE创建相同的线程。
**在这种情况下,互斥锁之前必须退出相同次数的另一个线程可以进入。
**如果相同的线程试图进入任何其他类型的互斥不止一次,这种行为是未定义的。
**
*/
static void debugMutexEnter(sqlite3_mutex *pX){
  sqlite3_debug_mutex *p = (sqlite3_debug_mutex*)pX;
  assert( p->id==SQLITE_MUTEX_RECURSIVE || debugMutexNotheld(pX) );
  p->cnt++;
}
static int debugMutexTry(sqlite3_mutex *pX){
  sqlite3_debug_mutex *p = (sqlite3_debug_mutex*)pX;
  assert( p->id==SQLITE_MUTEX_RECURSIVE || debugMutexNotheld(pX) );
  p->cnt++;
  return SQLITE_OK;
}

/*
** The sqlite3_mutex_leave() routine exits a mutex that was
** previously entered by the same thread.  The behavior
** is undefined if the mutex is not currently entered or
** is not currently allocated.  SQLite will never do either.
sqlite3_mutex_leave()函数退出一个互斥锁该锁是以前输入的相同的线程。
如果互斥对象当前不是目前进入口的不分配的行为是未定义的,SQLiteSQLite永远不会做。*/
static void debugMutexLeave(sqlite3_mutex *pX){
  sqlite3_debug_mutex *p = (sqlite3_debug_mutex*)pX;
  assert( debugMutexHeld(pX) );
  p->cnt--;
  assert( p->id==SQLITE_MUTEX_RECURSIVE || debugMutexNotheld(pX) );
}

sqlite3_mutex_methods const *sqlite3NoopMutex(void){
  static const sqlite3_mutex_methods sMutex = {
    debugMutexInit,
    debugMutexEnd,
    debugMutexAlloc,
    debugMutexFree,
    debugMutexEnter,
    debugMutexTry,
    debugMutexLeave,

    debugMutexHeld,
    debugMutexNotheld
  };

  return &sMutex;
}
#endif /* SQLITE_DEBUG */

/*
** If compiled with SQLITE_MUTEX_NOOP, then the no-op mutex implementation
** is used regardless of the run-time threadsafety setting.
如果使用SQLITE_MUTEX_NOOP编译,那么无操作互斥对象的实现被使用不顾运行时线程安全设置*/
#ifdef SQLITE_MUTEX_NOOP
sqlite3_mutex_methods const *sqlite3DefaultMutex(void){
  return sqlite3NoopMutex();
}
#endif /* defined(SQLITE_MUTEX_NOOP) */
#endif /* !defined(SQLITE_MUTEX_OMIT) */
