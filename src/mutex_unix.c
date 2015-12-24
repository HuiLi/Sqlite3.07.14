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
**���ļ�����ִ�л�����pthreads��C����
*/
#include "sqliteInt.h"

/*
** The code in this file is only used if we are compiling threadsafe
** under unix with pthreads.
**������ļ��еĴ��룬���������unixϵͳ������߳�ʱ��ʹ�á�
**
** Note that this implementation requires a version of pthreads that
** supports recursive mutexes.
**ע�⣬��ʵ����Ҫһ��pthreads�İ汾֧�ֵݹ黥���塣
*/
#ifdef SQLITE_MUTEX_PTHREADS

#include <pthread.h>

/*
** The sqlite3_mutex.id, sqlite3_mutex.nRef, and sqlite3_mutex.owner fields
** are necessary under two condidtions:  (1) Debug builds and (2) using
** home-grown mutexes.  Encapsulate these conditions into a single #define.

**������������sqlite3_mutex.id��sqlite3_mutex.nref��sqlite3_mutex.owner�Ǳ�Ҫ��
**1�����԰汾��2��ʹ�ñ��صĻ��⡣��װ��Щ�����ɵ�һ��#define��
*/
#if defined(SQLITE_DEBUG) || defined(SQLITE_HOMEGROWN_RECURSIVE_MUTEX)
# define SQLITE_MUTEX_NREF 1
#else
# define SQLITE_MUTEX_NREF 0
#endif

/*
** Each recursive mutex is an instance of the following structure.ÿ���ݹ黥�������½ṹ��һ��ʵ����
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
**sqlite3_mutex_held������sqlite3_mutex_notheld����������Ŀ�Ľ������ڲ���assert������䡣
**��ĳЩƽ̨�ϣ�������һ�����������ܻᵼ����Щ�������ṩ����ȷ�Ľ����
**�ر��ǣ����pthread_equal��������һ��ԭ�Ӳ�����Ȼ����Щ���̿��ܵ��Ͳ���ȷ�Ľ����
**�ڴ����ƽ̨�ϣ�pthread_equal���������������ıȽϵ�ԭ�Ӳ�����
**�����˸������ǣ�HPUX��������һ��ƽ̨�����������������Щ���̽���������ȷ����HPUX������
** On those platforms where pthread_equal() is not atomic, SQLite
** should be compiled without -DSQLITE_DEBUG and with -DNDEBUG to
** make sure no assert() statements are evaluated and hence these
** routines are never called.
**����Щƽ̨�ϣ�����pthread_equal��������ԭ��ʱ��SQLiteӦ��û��-DSQLITE_DEBUG��-DNDEBUG���б��룬
**��ȷ��û��assert���������������������Щ���̲��ᱻ���á�
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
** Initialize and deinitialize the mutex subsystem.��ʼ����ȡ����ʼ��������ϵͳ��
*/
static int pthreadMutexInit(void){ return SQLITE_OK; }
static int pthreadMutexEnd(void){ return SQLITE_OK; }

/*
** The sqlite3_mutex_alloc() routine allocates a new
** mutex and returns a pointer to it.  If it returns NULL
** that means that a mutex could not be allocated.  SQLite
** will unwind its stack and return an error.  The argument
** to sqlite3_mutex_alloc() is one of these integer constants:
**sqlite3_mutex_alloc������������һ���µĻ�������
**������һ��ָ�롣�������NULL������ζ��һ���������޷����䡣
**SQLite�Ľ��������ջ������һ������
**�ò���sqlite3_mutex_alloc��������Щ���ͳ���֮һ��
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
**ǰ������������sqlite3_mutex_alloc����������һ���µĻ��⡣
**�µĻ������ǵݹ�SQLITE_MUTEX_RECURSIVEʹ�ã�������һ��SQLITE_MUTEX_FASTʹ�á�
**�����ʵ�ֲ�����ҪSQLITE_MUTEX_RECURSIVE��SQLITE_MUTEX_FAST֮���������֣��������ϣ����
**���ǣ�SQLite��������һ���ݹ黥�⵱��ȷʵ��Ҫ��
**���һ������ķǵݹ黥�����ʵ���ǿ���������ƽ̨�ϣ�
**������ϵͳ������ӦSQLITE_MUTEX_FAST��������һ�������塣
** The other allowed parameters to sqlite3_mutex_alloc() each return
** a pointer to a static preexisting mutex.  Six static mutexes are
** used by the current version of SQLite.  Future versions of SQLite
** may add additional static mutexes.  Static mutexes are for internal
** use by SQLite only.  Applications that use SQLite mutexes should
** use only the dynamic mutexes returned by SQLITE_MUTEX_FAST or
** SQLITE_MUTEX_RECURSIVE.
**����sqlite3_mutex_alloc���������Ŀ�ѡ����������һ��ָ��һ����̬��Ԥ�ȴ��ڻ����С�
**������̬��������ʹ�õ���SQLite�ĵ�ǰ�汾��
**SQLite��δ���汾���ܻ����Ӷ���ľ�̬���⡣
**��̬�������SQLite���ڲ�ʹ�á�
**ʹ��SQLite�Ļ������Ӧ�ó���ֻ��ʹ����SQLITE_MUTEX_FAST��SQLITE_MUTEX_RECURSIVE���صĶ�̬���⡣
** Note that if one of the dynamic mutex parameters (SQLITE_MUTEX_FAST
** or SQLITE_MUTEX_RECURSIVE) is used then sqlite3_mutex_alloc()
** returns a different mutex on every call.  But for the static 
** mutex types, the same mutex is returned on every call that has
** the same type number.
**ע�⣬�����̬���������SQLITE_MUTEX_FAST��SQLITE_MUTEX_RECURSIVE���е�һ����ʹ�ã�
**��ôsqlite3_mutex_alloc������ÿ�ε���ʱ����һ����ͬ�Ļ��⡣
**�����ھ�̬�Ļ��������ͣ���ͬ�Ļ�������ÿ�ε��þ�����ͬ���ͺŷ��ء�
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
		/*����ݹ黥���岻���ã����ǽ�Ҫ���������Լ��ġ� �����ġ�*/
        pthread_mutex_init(&p->mutex, 0);
#else
        /* Use a recursive mutex if it is available ʹ�õݹ黥��������ǿ���*/
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
**�������ȡ��������ǰ����Ļ��⡣SQLite�������ͷ�ÿһ���������以�⡣
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
**sqlite3_mutex_enter������sqlite3_mutex_try����������ͼ����һ�����⡣
**�����һ���߳��Ѿ��ǻ����壬sqlite3_mutex_enter������������sqlite3_mutex_try����
**������SQLITE_BUSY��
**sqlite3_mutex_try�����ӿڷ���SQLITE_OK�ɹ����롣
**ʹ��SQLITE_MUTEX_RECURSIVE�����Ļ��������ͬһ���̱߳������Ρ�
**����������£���һ���߳̿��Խ���֮ǰ����������˳�һ����ͬ�Ĵ�����
**���ͬһ���߳���ͼ�����κ��������͵Ļ��ⲻֹһ�Σ�����Ϊ��δ����ġ�
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
  **����ݹ黥���岻���ã���ô���Ǿͱ��뽨���Լ��ġ�
  **�������ٶ�pthread_equal()��ԭ�Ӳ��������������Ƚ�ʱ�����p->owner�ڲ�����self��
  **����ֵ֮��仯��������ʹ������Ϊself��p->owner����ȵġ�
  **��ʵ�ֻ�����һ�����ٻ��桪���������Ĺ��̲�����ͬһʱ���ȡ��ͬһ��ַ��ͬ��ֵ��
  **��������������������㣬��û��⽫ʧ�ܣ��������֡�
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
  /* Use the built-in recursive mutexes if they are available.ʹ�����õĵݹ黥���壨����У���
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
  **����ݹ黥���岻���ã���ô���Ǿͱ��뽨�������Լ��ġ�
  **�������ٶ�pthread_equal()��ԭ�Ӳ��������������Ƚ�ʱ�����p->owner�ڲ�����self��
  **����ֵ֮��仯��������ʹ������Ϊself��p->owner����ȵġ�
  **��ʵ�ֻ�����һ�����ٻ��桪���������Ĺ��̲�����ͬһʱ���ȡ��ͬһ��ַ��ͬ��ֵ��
  **��������������������㣬��û��⽫ʧ�ܣ�����Ҳ����֮���֡�
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
  /* Use the built-in recursive mutexes if they are available.ʹ�����õĵݹ黥���壨����У���
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
**sqlite3_mutex_leave���������˳���ǰ��ͬһ���߳̽����һ�����⡣
**�����������ǰδ�����ǰδ�������Ϊ�ǲ���ȷ�ģ�SQLite����Զ�������κ����顣
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
