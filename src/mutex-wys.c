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
**����ĵ�����ʵ�ֻ�������C ����
**
** This file contains code that is common across all mutex implementations.
**����ĵ�������ͬ���л������Ĵ���ʵ�֡�
*/
#include "sqliteInt.h"

#if defined(SQLITE_DEBUG) && !defined(SQLITE_MUTEX_OMIT)
/*
** For debugging purposes, record when the mutex subsystem is initialized
** and uninitialized so that we can assert() if there is an attempt to
** allocate a mutex while the system is uninitialized.
**���ڵ��ԣ���¼��������ϵͳ��ʼ���ĺ�δ��ʼ����ת��״̬��
**�����ǿ���ȷ�������Ƿ�����ͼ��Ҫ����һ�������ϵͳȴû�г�ʼ����
**
**
*/
static SQLITE_WSD int mutexIsInit = 0;
#endif /* SQLITE_DEBUG */


#ifndef SQLITE_MUTEX_OMIT
/*
** Initialize the mutex system.��ʼ������ϵͳ
*/
int sqlite3MutexInit(void){ 
  int rc = SQLITE_OK;
  if( !sqlite3GlobalConfig.mutex.xMutexAlloc ){
    /* If the xMutexAlloc method has not been set, then the user did not
    ** install a mutex implementation via sqlite3_config() prior to 
    ** sqlite3_initialize() being called. This block copies pointers to
    ** the default implementation into the sqlite3GlobalConfig structure.
    **���xMutexAlloc�������û�����ã�
    **��ô�û�û����ǰ��װһ�����������ʵ��sqlite3GlobalConfig().
    **sqlite3_��ʼ��ǰ�����á�
   **��θ���ָ��ָ����sqlite3GlobalConfig��Ĭ���ڲ��ṹ��
    */
    sqlite3_mutex_methods const *pFrom;zhu
    sqlite3_mutex_methods *pTo = &sqlite3GlobalConfig.mutex;

    if( sqlite3GlobalConfig.bCoreMutex ){
      pFrom = sqlite3DefaultMutex();
    }else{
      pFrom = sqlite3NoopMutex();
    }
    memcpy(pTo, pFrom, offsetof(sqlite3_mutex_methods, xMutexAlloc));
//offsetof�ú�������ṹ����һ����Ա�ڸýṹ���е�ƫ������
    memcpy(&pTo->xMutexFree, &pFrom->xMutexFree,
           sizeof(*pTo) - offsetof(sqlite3_mutex_methods, xMutexFree));
    pTo->xMutexAlloc = pFrom->xMutexAlloc;
//��θ���ָ��ָ����sqlite3GlobalConfig��Ĭ���ڲ��ṹ��
  }
  rc = sqlite3GlobalConfig.mutex.xMutexInit();

#ifdef SQLITE_DEBUG//����ʱ֧�ֿ�д��̬����
  GLOBAL(int, mutexIsInit) = 1;//mutexIsInit��ֵΪ1
#endif

  return rc;
}

/*
** Shutdown the mutex system. This call frees resources allocated by
** sqlite3MutexInit().
** �رջ���ϵͳ���ͷ�sqlite3MutexInit()�������Դ��
*/
int sqlite3MutexEnd(void){
  int rc = SQLITE_OK;
  if( sqlite3GlobalConfig.mutex.xMutexEnd ){
    rc = sqlite3GlobalConfig.mutex.xMutexEnd();
  }
  /**���xMutexAlloc �����������ˣ�
    **��ô,�û���װһ���������ʵ��ͨ��sqlite3_config () ��
    **sqlite3_��ʼ���󱻵��á�
    */  

#ifdef SQLITE_DEBUG
  GLOBAL(int, mutexIsInit) = 0;
#endif

  return rc;
}

/*
** Retrieve a pointer to a static mutex or allocate a new dynamic one.
**��ȡһ��ָ��̬�Ļ������������·���һ���µĶ�̬��������
*/
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
** Free a dynamic mutex.�ͷ�һ����̬�Ļ�����
*/
void sqlite3_mutex_free(sqlite3_mutex *p){
  if( p ){
    sqlite3GlobalConfig.mutex.xMutexFree(p);
  }
}

/*
** Obtain the mutex p. If some other thread already has the mutex, block
** until it can be obtained.
**��û�����P
**��������߳��Ѿ�ӵ���˻�������
**��ô�ͽ���������ֱ����Ҳ���Ϊֹ��
*/
void sqlite3_mutex_enter(sqlite3_mutex *p){
  if( p ){
    sqlite3GlobalConfig.mutex.xMutexEnter(p);
  }
}

/*
** Obtain the mutex p. If successful, return SQLITE_OK. Otherwise, if another
** thread holds the mutex and it cannot be obtained, return SQLITE_BUSY.
**ȥ��û�����P.����ɹ��ͷ���SQLITE_OK��
**�����������һ���̳߳��л����������Ͳ��ܵõ�����ʱ����SQLITE_BUSY��
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
**sqlite3_mutex_leave()�������˳�֮ǰ������ͬ���߳�����һ��������󡡡�
** ���������������ǵ�ǰ����ģ���ô����������ᱻ���塣
**������ݸ��ú����Ĳ�����һ����ָ�룬��ô�Ͳ���ִ���κβ�����
**
*/
void sqlite3_mutex_leave(sqlite3_mutex *p){
  if( p ){
    sqlite3GlobalConfig.mutex.xMutexLeave(p);
  }
}

/*
** Obtain the mutex p. If successful, return SQLITE_OK. Otherwise, if another
** thread holds the mutex and it cannot be obtained, return SQLITE_BUSY.
**ȥ��û�����P.����ɹ��ͷ���SQLITE_OK��
**�����������һ���̳߳��л����������Ͳ��ܵõ�����ʱ����SQLITE_BUSY��
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
**sqlite3_mutex_leave()�������˳�֮ǰ������ͬ���߳�����һ��������󡡡�
** ���������������ǵ�ǰ����ģ���ô����������ᱻ���塣
**������ݸ��ú����Ĳ�����һ����ָ�룬��ô�Ͳ���ִ���κβ�����
**
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
**sqlite3_mutex_held() ��sqlite3_mutex_notheld() �����������ڲ�assert() ���
**
*/
int sqlite3_mutex_held(sqlite3_mutex *p){
  return p==0 || sqlite3GlobalConfig.mutex.xMutexHeld(p);
}
int sqlite3_mutex_notheld(sqlite3_mutex *p){
  return p==0 || sqlite3GlobalConfig.mutex.xMutexNotheld(p);
}
#endif

#endif /* !defined(SQLITE_MUTEX_OMIT) */

