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
**此文件包含实现互斥的C函数。
** This implementation in this file does not provide any mutual
** exclusion and is thus suitable for use only in applications
** that use SQLite in a single thread.  The routines defined
** here are place-holders.  Applications can substitute working
** mutex routines at start-time using the
**
**     sqlite3_config(SQLITE_CONFIG_MUTEX,...)
**
** interface.
**
** If compiled with SQLITE_DEBUG, then additional logic is inserted
** that does error checking on mutexes to make sure they are being
** called correctly.
*/
/*在这个文件中的此实现不提供任何互斥，因此适合于用在仅在单线程中使用SQLite的应用中。
**定义在这里的程序是占位符。
**应用程序使用sqlite3_config（SQLITE_CONFIG_MUTEX，...）接口
**在启动时代替工作互斥程序。如果使用SQLITE_DEBUG进行编译，然后额外的逻辑插入，
**做对互斥的错误检查，以确保它们被正确地调用。
**
*/
#include "sqliteInt.h"

#ifndef SQLITE_MUTEX_OMIT

#ifndef SQLITE_DEBUG
/*
** Stub routines for all mutex methods.
**存贮所有的互斥方法
** This routines provide no mutual exclusion or error checking.
**这个程序没有提供相互排斥或错误检查。
*/
static int noopMutexInit(void){ return SQLITE_OK; }
static int noopMutexEnd(void){ return SQLITE_OK; }
static sqlite3_mutex *noopMutexAlloc(int id){ 
  UNUSED_PARAMETER(id);
  return (sqlite3_mutex*)8; 
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
**在该实现中，对于测试和调试提供了一种错误检查。该互斥量仍然没有提供任何互斥。
*/

/*
** The mutex object
**互斥对象
*/
typedef struct sqlite3_debug_mutex {
  int id;     /* The mutex type 互斥类型*/
  int cnt;    /* Number of entries without a matching leave */
} sqlite3_debug_mutex;

/*
** The sqlite3_mutex_held() and sqlite3_mutex_notheld() routine are
** intended for use inside assert() statements.
**sqlite3_mutex_held（）和sqlite3_mutex_notheld（）函数则主要用于内部的assert()语句。
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
**初始化和取消初始化互斥子系统。
*/
static int debugMutexInit(void){ return SQLITE_OK; }
static int debugMutexEnd(void){ return SQLITE_OK; }

/*
** The sqlite3_mutex_alloc() routine allocates a new
** mutex and returns a pointer to it.  If it returns NULL
** that means that a mutex could not be allocated. 
**该sqlite3_mutex_alloc（）函数分配一个新的互斥锁，并返回一个指针。
**如果返回NULL，这意味着互斥没有被分配。
*/
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
**这个程序取消分配以前分配的互斥。
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
**
** sqlite3_mutex_enter（）和sqlite3_mutex_try（）程序试图进入一个互斥中。
** 如果另一个线程已经是互斥体，sqlite3_mutex_enter（）将阻塞，sqlite3_mutex_try（）将返回SQLITE_BUSY。
** sqlite3_mutex_try（）接口返回SQLITE_OK成功进入。
** 创建的互斥使用sqlite_mutex_recursive可以进入多次相同的线程。
** 在这种情况下，另一个线程可以进入之前，互斥必须退出一个相同的次数。
** 如果同一个线程试图进入任何其他类型的互斥不止一次，该行为是未定义的。
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
**sqlite3_mutex_leave（）函数退出先前由同一个线程进入的一个互斥。
**如果互斥锁当前未输入或当前未分配的行为是不明确的。 
**SQLite永远不会做任何事。
*/
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
**如果编译sqlite_mutex_noop，no-op互斥
*/
#ifdef SQLITE_MUTEX_NOOP
sqlite3_mutex_methods const *sqlite3DefaultMutex(void){
  return sqlite3NoopMutex();
}
#endif /* defined(SQLITE_MUTEX_NOOP) */
#endif /* !defined(SQLITE_MUTEX_OMIT) */
