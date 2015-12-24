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
** This file contains the C functions that implement mutexes for win32
**该文件包含执行互斥体为Win32的C函数。
*/
#include "sqliteInt.h"

/*
** The code in this file is only used if we are compiling multithreaded
** on a win32 system.
**在这个文件中的代码，如果我们在Win32系统编译多线程时才使用。
*/
#ifdef SQLITE_MUTEX_W32

/*
** Each recursive mutex is an instance of the following structure.
**每个递归互斥是以下结构的一个实例。
*/
struct sqlite3_mutex {
  CRITICAL_SECTION mutex;    /* Mutex controlling the lock 互斥控制锁*/
  int id;                    /* Mutex type 互斥类型*/
#ifdef SQLITE_DEBUG
  volatile int nRef;         /* Number of enterances 入口数*/
  volatile DWORD owner;      /* Thread holding this mutex 线程持有这个互斥*/
  int trace;                 /* True to trace changes 真正的跟踪变更*/
#endif
};
#define SQLITE_W32_MUTEX_INITIALIZER { 0 }
#ifdef SQLITE_DEBUG
#define SQLITE3_MUTEX_INITIALIZER { SQLITE_W32_MUTEX_INITIALIZER, 0, 0L, (DWORD)0, 0 }
#else
#define SQLITE3_MUTEX_INITIALIZER { SQLITE_W32_MUTEX_INITIALIZER, 0 }
#endif

/*
** Return true (non-zero) if we are running under WinNT, Win2K, WinXP,
** or WinCE.  Return false (zero) for Win95, Win98, or WinME.
**
**如果我们在WINNT，Win2K，WINXP，或WinCE下运行，返回真（不为零）。
**如果在Win95，Win98，或WinME下运行，返回FALSE（零）。

** Here is an interesting observation:  Win95, Win98, and WinME lack
** the LockFileEx() API.  But we can still statically link against that
** API as long as we don't call it win running Win95/98/ME.  A call to
** this routine is used to determine if the host is Win95/98/ME or
** WinNT/2K/XP so that we will know whether or not we can safely call
** the LockFileEx() API.
**这里是一个有趣的观察：Win95、Win98和WinMe，缺乏lockfileex() API。
**但是，我们仍然可以对静态的API链接，只要我们不把它运行在WIN95 /98/ ME中。
**调用该例程用于确定主机是Win95/98/ ME还是WinNT/2K/ XP，
**这样我们就知道我们是否可以安全地调用LockFileEx（）API。
**
** mutexIsNT() is only used for the TryEnterCriticalSection() API call,
** which is only available if your application was compiled with 
** _WIN32_WINNT defined to a value >= 0x0400.  Currently, the only
** call to TryEnterCriticalSection() is #ifdef'ed out, so #ifdef 
** this out as well.
**mutexIsNT（）只用于TryEnterCriticalSection（）API调用，
**如果你的应用程序与定义的值>=0x0400，_WIN32_WINNT编译这是唯一可用的。
**目前，以TryEnterCriticalSection（）唯一的的调用是＃ifdef'ed了，
**所以#ifdef这一点好。
*/
#if 0
#if SQLITE_OS_WINCE || SQLITE_OS_WINRT
# define mutexIsNT()  (1)
#else
  static int mutexIsNT(void){
    static int osType = 0;
    if( osType==0 ){
      OSVERSIONINFO sInfo;
      sInfo.dwOSVersionInfoSize = sizeof(sInfo);
      GetVersionEx(&sInfo);
      osType = sInfo.dwPlatformId==VER_PLATFORM_WIN32_NT ? 2 : 1;
    }
    return osType==2;
  }
#endif /* SQLITE_OS_WINCE */
#endif

#ifdef SQLITE_DEBUG
/*
** The sqlite3_mutex_held() and sqlite3_mutex_notheld() routine are
** intended for use only inside assert() statements.
sqlite3_mutex_held（）和sqlite3_mutex_notheld（）函数的目的仅用于内部的assert（）语句。
*/
static int winMutexHeld(sqlite3_mutex *p){
  return p->nRef!=0 && p->owner==GetCurrentThreadId();
}
static int winMutexNotheld2(sqlite3_mutex *p, DWORD tid){
  return p->nRef==0 || p->owner!=tid;
}
static int winMutexNotheld(sqlite3_mutex *p){
  DWORD tid = GetCurrentThreadId(); 
  return winMutexNotheld2(p, tid);
}
#endif


/*
** Initialize and deinitialize the mutex subsystem.
**初始化和取消初始化互斥子系统。
*/
static sqlite3_mutex winMutex_staticMutexes[6] = {
  SQLITE3_MUTEX_INITIALIZER,
  SQLITE3_MUTEX_INITIALIZER,
  SQLITE3_MUTEX_INITIALIZER,
  SQLITE3_MUTEX_INITIALIZER,
  SQLITE3_MUTEX_INITIALIZER,
  SQLITE3_MUTEX_INITIALIZER
};
static int winMutex_isInit = 0;
/* As winMutexInit() and winMutexEnd() are called as part
** of the sqlite3_initialize and sqlite3_shutdown()
** processing, the "interlocked" magic is probably not
** strictly necessary.
**至于winMutexInit（）和winMutexEnd（）称为sqlite3_initialize和sqlite3_shutdown（）
**处理的一部分，在“互锁”中可能不是绝对必要的。
*/
static long winMutex_lock = 0;

void sqlite3_win32_sleep(DWORD milliseconds); /* os_win.c */

static int winMutexInit(void){ 
  /* The first to increment to 1 does actual initialization 第一个增量为1的实际初始化*/
  if( InterlockedCompareExchange(&winMutex_lock, 1, 0)==0 ){
    int i;
    for(i=0; i<ArraySize(winMutex_staticMutexes); i++){
#if SQLITE_OS_WINRT
      InitializeCriticalSectionEx(&winMutex_staticMutexes[i].mutex, 0, 0);
#else
      InitializeCriticalSection(&winMutex_staticMutexes[i].mutex);
#endif
    }
    winMutex_isInit = 1;
  }else{
    /* Someone else is in the process of initing the static mutexes 另一个进程是在初始化静态互斥进程*/
    while( !winMutex_isInit ){
      sqlite3_win32_sleep(1);
    }
  }
  return SQLITE_OK; 
}

static int winMutexEnd(void){ 
  /* The first to decrement to 0 does actual shutdown 第一个递减到0的实际关机
  ** (which should be the last to shutdown.) */
  if( InterlockedCompareExchange(&winMutex_lock, 0, 1)==1 ){
    if( winMutex_isInit==1 ){
      int i;
      for(i=0; i<ArraySize(winMutex_staticMutexes); i++){
        DeleteCriticalSection(&winMutex_staticMutexes[i].mutex);
      }
      winMutex_isInit = 0;
    }
  }
  return SQLITE_OK; 
}

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
static sqlite3_mutex *winMutexAlloc(int iType){
  sqlite3_mutex *p;

  switch( iType ){
    case SQLITE_MUTEX_FAST:
    case SQLITE_MUTEX_RECURSIVE: {
      p = sqlite3MallocZero( sizeof(*p) );
      if( p ){  
#ifdef SQLITE_DEBUG
        p->id = iType;
#endif
#if SQLITE_OS_WINRT
        InitializeCriticalSectionEx(&p->mutex, 0, 0);
#else
        InitializeCriticalSection(&p->mutex);
#endif
      }
      break;
    }
    default: {
      assert( winMutex_isInit==1 );
      assert( iType-2 >= 0 );
      assert( iType-2 < ArraySize(winMutex_staticMutexes) );
      p = &winMutex_staticMutexes[iType-2];
#ifdef SQLITE_DEBUG
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
static void winMutexFree(sqlite3_mutex *p){
  assert( p );
  assert( p->nRef==0 && p->owner==0 );
  assert( p->id==SQLITE_MUTEX_FAST || p->id==SQLITE_MUTEX_RECURSIVE );
  DeleteCriticalSection(&p->mutex);
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
static void winMutexEnter(sqlite3_mutex *p){
#ifdef SQLITE_DEBUG
  DWORD tid = GetCurrentThreadId(); 
  assert( p->id==SQLITE_MUTEX_RECURSIVE || winMutexNotheld2(p, tid) );
#endif
  EnterCriticalSection(&p->mutex);
#ifdef SQLITE_DEBUG
  assert( p->nRef>0 || p->owner==0 );
  p->owner = tid; 
  p->nRef++;
  if( p->trace ){
    printf("enter mutex %p (%d) with nRef=%d\n", p, p->trace, p->nRef);
  }
#endif
}
static int winMutexTry(sqlite3_mutex *p){
#ifndef NDEBUG
  DWORD tid = GetCurrentThreadId(); 
#endif
  int rc = SQLITE_BUSY;
  assert( p->id==SQLITE_MUTEX_RECURSIVE || winMutexNotheld2(p, tid) );
  /*
  ** The sqlite3_mutex_try() routine is very rarely used, and when it
  ** is used it is merely an optimization.  So it is OK for it to always
  ** fail.  
  **sqlite3_mutex_try（）例程是很少使用，并且当使用它仅仅是一个优化。
  **因此，即使它总是失败也是可以的。
  ** The TryEnterCriticalSection() interface is only available on WinNT.
  ** And some windows compilers complain if you try to use it without
  ** first doing some #defines that prevent SQLite from building on Win98.
  ** For that reason, we will omit this optimization for now.  See
  ** ticket #2685.
  **TryEnterCriticalSection（）接口仅适用于WINNT。
  **而一些Windows编译器抱怨，如果你没有首先定义一些#define语句而尝试使用它，那将会阻止SQLite的构建在Win98平台上。
	**因为这个原因，我们现在将省略这个优化。参见#2685.
  */
#if 0
  if( mutexIsNT() && TryEnterCriticalSection(&p->mutex) ){
    p->owner = tid;
    p->nRef++;
    rc = SQLITE_OK;
  }
#else
  UNUSED_PARAMETER(p);
#endif
#ifdef SQLITE_DEBUG
  if( rc==SQLITE_OK && p->trace ){
    printf("try mutex %p (%d) with nRef=%d\n", p, p->trace, p->nRef);
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
static void winMutexLeave(sqlite3_mutex *p){
#ifndef NDEBUG
  DWORD tid = GetCurrentThreadId();
  assert( p->nRef>0 );
  assert( p->owner==tid );
  p->nRef--;
  if( p->nRef==0 ) p->owner = 0;
  assert( p->nRef==0 || p->id==SQLITE_MUTEX_RECURSIVE );
#endif
  LeaveCriticalSection(&p->mutex);
#ifdef SQLITE_DEBUG
  if( p->trace ){
    printf("leave mutex %p (%d) with nRef=%d\n", p, p->trace, p->nRef);
  }
#endif
}

sqlite3_mutex_methods const *sqlite3DefaultMutex(void){
  static const sqlite3_mutex_methods sMutex = {
    winMutexInit,
    winMutexEnd,
    winMutexAlloc,
    winMutexFree,
    winMutexEnter,
    winMutexTry,
    winMutexLeave,
#ifdef SQLITE_DEBUG
    winMutexHeld,
    winMutexNotheld
#else
    0,
    0
#endif
  };

  return &sMutex;
}
#endif /* SQLITE_MUTEX_W32 */
