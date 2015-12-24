/*
** 2007 August 28
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file contains the C functions that implement mutexes for pthreads
**该文件包含执行互斥体pthreads的C函数
*/
#include "sqliteInt.h"

/*
** The code in this file is only used if we are compiling threadsafe
** under unix with pthreads.
**在这个文件中的代码，如果我们在unix系统编译多线程时才使用。
**
** Note that this implementation requires a version of pthreads that
** supports recursive mutexes.
**注意，此实现需要一个pthreads的版本支持递归互斥体。
*/
#ifdef SQLITE_MUTEX_PTHREADS

#include <pthread.h>

/*
** The sqlite3_mutex.id, sqlite3_mutex.nRef, and sqlite3_mutex.owner fields
** are necessary under two condidtions:  (1) Debug builds and (2) using
** home-grown mutexes.  Encapsulate these conditions into a single #define.

**在两个条件下sqlite3_mutex.id，sqlite3_mutex.nref和sqlite3_mutex.owner是必要：
**1）调试版本（2）使用本地的互斥。封装这些条件成单一的#define。
*/
#if defined(SQLITE_DEBUG) || defined(SQLITE_HOMEGROWN_RECURSIVE_MUTEX)
# define SQLITE_MUTEX_NREF 1
#else
# define SQLITE_MUTEX_NREF 0
#endif

/*
** Each recursive mutex is an instance of the following structure.每个递归互斥是以下结构的一个实例。
*/
struct sqlite3_mutex {
  pthread_mutex_t mutex;     /* Mutex controlling the lock */
#if SQLITE_MUTEX_NREF
  int id;                    /* Mutex type */
  volatile int nRef;         /* Number of entrances */
  volatile pthread_t owner;  /* Thread that is within this mutex */
  int trace;                 /* True to trace changes */
#endif
};
#if SQLITE_MUTEX_NREF
#define SQLITE3_MUTEX_INITIALIZER { PTHREAD_MUTEX_INITIALIZER, 0, 0, (pthread_t)0, 0 }
#else
#define SQLITE3_MUTEX_INITIALIZER { PTHREAD_MUTEX_INITIALIZER }
#endif

/*
** The sqlite3_mutex_held() and sqlite3_mutex_notheld() routine are
** intended for use only inside assert() statements.  On some platforms,
** there might be race conditions that can cause these routines to
** deliver incorrect results.  In particular, if pthread_equal() is
** not an atomic operation, then these routines might delivery
** incorrect results.  On most platforms, pthread_equal() is a 
** comparison of two integers and is therefore atomic.  But we are
** told that HPUX is not such a platform.  If so, then these routines
** will not always work correctly on HPUX.
**sqlite3_mutex_held（）和sqlite3_mutex_notheld（）函数的目的仅用于内部的assert（）语句。
**在某些平台上，可能有一类条件，可能会导致这些例程来提供不正确的结果。
**特别是，如果pthread_equal（）不是一个原子操作，然后这些例程可能递送不正确的结果。
**在大多数平台上，pthread_equal（）是两个整数的比较的原子操作。
**但有人告诉我们，HPUX不是这样一个平台。如果是这样，则这些例程将不总是正确地在HPUX工作。
** On those platforms where pthread_equal() is not atomic, SQLite
** should be compiled without -DSQLITE_DEBUG and with -DNDEBUG to
** make sure no assert() statements are evaluated and hence these
** routines are never called.
**在这些平台上，其中pthread_equal（）不是原子时，SQLite应该没有-DSQLITE_DEBUG和-DNDEBUG进行编译，
**以确保没有assert（）语句进行评估，因此这些例程不会被调用。
*/
#if !defined(NDEBUG) || defined(SQLITE_DEBUG)
static int pthreadMutexHeld(sqlite3_mutex *p){
  return (p->nRef!=0 && pthread_equal(p->owner, pthread_self()));
}
static int pthreadMutexNotheld(sqlite3_mutex *p){
  return p->nRef==0 || pthread_equal(p->owner, pthread_self())==0;
}
#endif

/*
** Initialize and deinitialize the mutex subsystem.初始化和取消初始化互斥子系统。
*/
static int pthreadMutexInit(void){ return SQLITE_OK; }
static int pthreadMutexEnd(void){ return SQLITE_OK; }

/*
** The sqlite3_mutex_alloc() routine allocates a new
** mutex and returns a pointer to it.  If it returns NULL
** that means that a mutex could not be allocated.  SQLite
** will unwind its stack and return an error.  The argument
** to sqlite3_mutex_alloc() is one of these integer constants:
**sqlite3_mutex_alloc（）函数分配一个新的互斥锁，
**并返回一个指针。如果返回NULL，这意味着一个互斥体无法分配。
**SQLite的将放松其堆栈并返回一个错误。
**该参数sqlite3_mutex_alloc（）是这些整型常量之一：
** <ul>
** <li>  SQLITE_MUTEX_FAST
** <li>  SQLITE_MUTEX_RECURSIVE
** <li>  SQLITE_MUTEX_STATIC_MASTER
** <li>  SQLITE_MUTEX_STATIC_MEM
** <li>  SQLITE_MUTEX_STATIC_MEM2
** <li>  SQLITE_MUTEX_STATIC_PRNG
** <li>  SQLITE_MUTEX_STATIC_LRU
** <li>  SQLITE_MUTEX_STATIC_PMEM
** </ul>
**
** The first two constants cause sqlite3_mutex_alloc() to create
** a new mutex.  The new mutex is recursive when SQLITE_MUTEX_RECURSIVE
** is used but not necessarily so when SQLITE_MUTEX_FAST is used.
** The mutex implementation does not need to make a distinction
** between SQLITE_MUTEX_RECURSIVE and SQLITE_MUTEX_FAST if it does
** not want to.  But SQLite will only request a recursive mutex in
** cases where it really needs one.  If a faster non-recursive mutex
** implementation is available on the host platform, the mutex subsystem
** might return such a mutex in response to SQLITE_MUTEX_FAST.
**前两个常量引起sqlite3_mutex_alloc（）来创建一个新的互斥。
**新的互斥体是递归SQLITE_MUTEX_RECURSIVE使用，但并不一定SQLITE_MUTEX_FAST使用。
**互斥的实现并不需要SQLITE_MUTEX_RECURSIVE和SQLITE_MUTEX_FAST之间作出区分，如果它不希望。
**但是，SQLite将仅请求一个递归互斥当它确实需要。
**如果一个更快的非递归互斥体的实现是可以在主机平台上，
**互斥子系统可能响应SQLITE_MUTEX_FAST返回这样一个互斥体。
** The other allowed parameters to sqlite3_mutex_alloc() each return
** a pointer to a static preexisting mutex.  Six static mutexes are
** used by the current version of SQLite.  Future versions of SQLite
** may add additional static mutexes.  Static mutexes are for internal
** use by SQLite only.  Applications that use SQLite mutexes should
** use only the dynamic mutexes returned by SQLITE_MUTEX_FAST or
** SQLITE_MUTEX_RECURSIVE.
**对于sqlite3_mutex_alloc（）其他的可选参数都返回一个指向一个静态的预先存在互斥中。
**六个静态互斥体所使用的是SQLite的当前版本。
**SQLite的未来版本可能会增加额外的静态互斥。
**静态互斥仅供SQLite的内部使用。
**使用SQLite的互斥体的应用程序只能使用由SQLITE_MUTEX_FAST或SQLITE_MUTEX_RECURSIVE返回的动态互斥。
** Note that if one of the dynamic mutex parameters (SQLITE_MUTEX_FAST
** or SQLITE_MUTEX_RECURSIVE) is used then sqlite3_mutex_alloc()
** returns a different mutex on every call.  But for the static 
** mutex types, the same mutex is returned on every call that has
** the same type number.
**注意，如果动态互斥参数（SQLITE_MUTEX_FAST或SQLITE_MUTEX_RECURSIVE）中的一个被使用，
**那么sqlite3_mutex_alloc（）在每次调用时返回一个不同的互斥。
**但对于静态的互斥锁类型，相同的互斥体在每次调用具有相同类型号返回。
*/
static sqlite3_mutex *pthreadMutexAlloc(int iType){
  static sqlite3_mutex staticMutexes[] = {
    SQLITE3_MUTEX_INITIALIZER,
    SQLITE3_MUTEX_INITIALIZER,
    SQLITE3_MUTEX_INITIALIZER,
    SQLITE3_MUTEX_INITIALIZER,
    SQLITE3_MUTEX_INITIALIZER,
    SQLITE3_MUTEX_INITIALIZER
  };
  sqlite3_mutex *p;
  switch( iType ){
    case SQLITE_MUTEX_RECURSIVE: {
      p = sqlite3MallocZero( sizeof(*p) );
      if( p ){
#ifdef SQLITE_HOMEGROWN_RECURSIVE_MUTEX
        /* If recursive mutexes are not available, we will have to
        ** build our own.  See below. */
		/*如果递归互斥体不可用，我们将要建立我们自己的。 见下文。*/
        pthread_mutex_init(&p->mutex, 0);
#else
        /* Use a recursive mutex if it is available 使用递归互斥如果它是可用*/
        pthread_mutexattr_t recursiveAttr;
        pthread_mutexattr_init(&recursiveAttr);
        pthread_mutexattr_settype(&recursiveAttr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&p->mutex, &recursiveAttr);
        pthread_mutexattr_destroy(&recursiveAttr);
#endif
#if SQLITE_MUTEX_NREF
        p->id = iType;
#endif
      }
      break;
    }
    case SQLITE_MUTEX_FAST: {
      p = sqlite3MallocZero( sizeof(*p) );
      if( p ){
#if SQLITE_MUTEX_NREF
        p->id = iType;
#endif
        pthread_mutex_init(&p->mutex, 0);
      }
      break;
    }
    default: {
      assert( iType-2 >= 0 );
      assert( iType-2 < ArraySize(staticMutexes) );
      p = &staticMutexes[iType-2];
#if SQLITE_MUTEX_NREF
      p->id = iType;
#endif
      break;
    }
  }
  return p;
}


/*
** This routine deallocates a previously
** allocated mutex.  SQLite is careful to deallocate every
** mutex that it allocates.
**这个程序取消分配以前分配的互斥。SQLite谨慎地释放每一个它所分配互斥。
*/
static void pthreadMutexFree(sqlite3_mutex *p){
  assert( p->nRef==0 );
  assert( p->id==SQLITE_MUTEX_FAST || p->id==SQLITE_MUTEX_RECURSIVE );
  pthread_mutex_destroy(&p->mutex);
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
**sqlite3_mutex_enter（）和sqlite3_mutex_try（）程序试图进入一个互斥。
**如果另一个线程已经是互斥体，sqlite3_mutex_enter（）将阻塞，sqlite3_mutex_try（）
**将返回SQLITE_BUSY。
**sqlite3_mutex_try（）接口返回SQLITE_OK成功进入。
**使用SQLITE_MUTEX_RECURSIVE创建的互斥可以由同一个线程被输入多次。
**在这种情况下，另一个线程可以进入之前，互斥必须退出一个相同的次数。
**如果同一个线程试图进入任何其他类型的互斥不止一次，该行为是未定义的。
*/
static void pthreadMutexEnter(sqlite3_mutex *p){
  assert( p->id==SQLITE_MUTEX_RECURSIVE || pthreadMutexNotheld(p) );

#ifdef SQLITE_HOMEGROWN_RECURSIVE_MUTEX
  /* If recursive mutexes are not available, then we have to grow
  ** our own.  This implementation assumes that pthread_equal()
  ** is atomic - that it cannot be deceived into thinking self
  ** and p->owner are equal if p->owner changes between two values
  ** that are not equal to self while the comparison is taking place.
  ** This implementation also assumes a coherent cache - that 
  ** separate processes cannot read different values from the same
  ** address at the same time.  If either of these two conditions
  ** are not met, then the mutexes will fail and problems will result.
  **如果递归互斥体不可用，那么我们就必须建立自己的。
  **这个程序假定pthread_equal()是原子操作——当发生比较时，如果p->owner在不等于self的
  **两个值之间变化，它不能使程序认为self和p->owner是相等的。
  **此实现还假设一个高速缓存——即独立的过程不能在同一时间读取从同一地址不同的值。
  **如果任这两个条件不满足，则该互斥将失败，问题会出现。
  */
  {
    pthread_t self = pthread_self();
    if( p->nRef>0 && pthread_equal(p->owner, self) ){
      p->nRef++;
    }else{
      pthread_mutex_lock(&p->mutex);
      assert( p->nRef==0 );
      p->owner = self;
      p->nRef = 1;
    }
  }
#else
  /* Use the built-in recursive mutexes if they are available.使用内置的递归互斥体（如果有）。
  */
  pthread_mutex_lock(&p->mutex);
#if SQLITE_MUTEX_NREF
  assert( p->nRef>0 || p->owner==0 );
  p->owner = pthread_self();
  p->nRef++;
#endif
#endif

#ifdef SQLITE_DEBUG
  if( p->trace ){
    printf("enter mutex %p (%d) with nRef=%d\n", p, p->trace, p->nRef);
  }
#endif
}
static int pthreadMutexTry(sqlite3_mutex *p){
  int rc;
  assert( p->id==SQLITE_MUTEX_RECURSIVE || pthreadMutexNotheld(p) );

#ifdef SQLITE_HOMEGROWN_RECURSIVE_MUTEX
  /* If recursive mutexes are not available, then we have to grow
  ** our own.  This implementation assumes that pthread_equal()
  ** is atomic - that it cannot be deceived into thinking self
  ** and p->owner are equal if p->owner changes between two values
  ** that are not equal to self while the comparison is taking place.
  ** This implementation also assumes a coherent cache - that 
  ** separate processes cannot read different values from the same
  ** address at the same time.  If either of these two conditions
  ** are not met, then the mutexes will fail and problems will result.
  **如果递归互斥体不可用，那么我们就必须建立我们自己的。
  **这个程序假定pthread_equal()是原子操作——当发生比较时，如果p->owner在不等于self的
  **两个值之间变化，它不能使程序认为self和p->owner是相等的。
  **此实现还假设一个高速缓存——即独立的过程不能在同一时间读取从同一地址不同的值。
  **如果任这两个条件不满足，则该互斥将失败，问题也会随之出现。
  */
  {
    pthread_t self = pthread_self();
    if( p->nRef>0 && pthread_equal(p->owner, self) ){
      p->nRef++;
      rc = SQLITE_OK;
    }else if( pthread_mutex_trylock(&p->mutex)==0 ){
      assert( p->nRef==0 );
      p->owner = self;
      p->nRef = 1;
      rc = SQLITE_OK;
    }else{
      rc = SQLITE_BUSY;
    }
  }
#else
  /* Use the built-in recursive mutexes if they are available.使用内置的递归互斥体（如果有）。
  */
  if( pthread_mutex_trylock(&p->mutex)==0 ){
#if SQLITE_MUTEX_NREF
    p->owner = pthread_self();
    p->nRef++;
#endif
    rc = SQLITE_OK;
  }else{
    rc = SQLITE_BUSY;
  }
#endif

#ifdef SQLITE_DEBUG
  if( rc==SQLITE_OK && p->trace ){
    printf("enter mutex %p (%d) with nRef=%d\n", p, p->trace, p->nRef);
  }
#endif
  return rc;
}

/*
** The sqlite3_mutex_leave() routine exits a mutex that was
** previously entered by the same thread.  The behavior
** is undefined if the mutex is not currently entered or
** is not currently allocated.  SQLite will never do either.
**sqlite3_mutex_leave（）函数退出先前由同一个线程进入的一个互斥。
**如果互斥锁当前未输入或当前未分配的行为是不明确的，SQLite将永远不会做任何事情。
*/
static void pthreadMutexLeave(sqlite3_mutex *p){
  assert( pthreadMutexHeld(p) );
#if SQLITE_MUTEX_NREF
  p->nRef--;
  if( p->nRef==0 ) p->owner = 0;
#endif
  assert( p->nRef==0 || p->id==SQLITE_MUTEX_RECURSIVE );

#ifdef SQLITE_HOMEGROWN_RECURSIVE_MUTEX
  if( p->nRef==0 ){
    pthread_mutex_unlock(&p->mutex);
  }
#else
  pthread_mutex_unlock(&p->mutex);
#endif

#ifdef SQLITE_DEBUG
  if( p->trace ){
    printf("leave mutex %p (%d) with nRef=%d\n", p, p->trace, p->nRef);
  }
#endif
}

sqlite3_mutex_methods const *sqlite3DefaultMutex(void){
  static const sqlite3_mutex_methods sMutex = {
    pthreadMutexInit,
    pthreadMutexEnd,
    pthreadMutexAlloc,
    pthreadMutexFree,
    pthreadMutexEnter,
    pthreadMutexTry,
    pthreadMutexLeave,
#ifdef SQLITE_DEBUG
    pthreadMutexHeld,
    pthreadMutexNotheld
#else
    0,
    0
#endif
  };

  return &sMutex;
}

#endif /* SQLITE_MUTEX_PTHREADS */
