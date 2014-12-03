/*
** 2006 June 7
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file contains code used to dynamically load extensions into
** the SQLite library.本文件的代码常常自动加载到SQLite扩展库中
*/

#ifndef SQLITE_CORE
  #define SQLITE_CORE 1  /* Disable the API redefinition in sqlite3ext.h 
                            避免API在sqlite3ext.h中重定义*/
#endif
#include "sqlite3ext.h"
#include "sqliteInt.h"
#include <string.h>

#ifndef SQLITE_OMIT_LOAD_EXTENSION

/*
** Some API routines are omitted when various features are一些API例程被省略了，当多种功能被排在一个建立的SQLite之外
** excluded from a build of SQLite.  Substitute a NULL pointer对于任何的无效APIs都用空指针代替
** for any missing APIs.
*/
#ifndef SQLITE_ENABLE_COLUMN_METADATA
# define sqlite3_column_database_name   0
# define sqlite3_column_database_name16 0
# define sqlite3_column_table_name      0
# define sqlite3_column_table_name16    0
# define sqlite3_column_origin_name     0
# define sqlite3_column_origin_name16   0
# define sqlite3_table_column_metadata  0
#endif

#ifdef SQLITE_OMIT_AUTHORIZATION
# define sqlite3_set_authorizer         0
#endif

#ifdef SQLITE_OMIT_UTF16
# define sqlite3_bind_text16            0
# define sqlite3_collation_needed16     0
# define sqlite3_column_decltype16      0
# define sqlite3_column_name16          0
# define sqlite3_column_text16          0
# define sqlite3_complete16             0
# define sqlite3_create_collation16     0
# define sqlite3_create_function16      0
# define sqlite3_errmsg16               0
# define sqlite3_open16                 0
# define sqlite3_prepare16              0
# define sqlite3_prepare16_v2           0
# define sqlite3_result_error16         0
# define sqlite3_result_text16          0
# define sqlite3_result_text16be        0
# define sqlite3_result_text16le        0
# define sqlite3_value_text16           0
# define sqlite3_value_text16be         0
# define sqlite3_value_text16le         0
# define sqlite3_column_database_name16 0
# define sqlite3_column_table_name16    0
# define sqlite3_column_origin_name16   0
#endif

#ifdef SQLITE_OMIT_COMPLETE
# define sqlite3_complete 0
# define sqlite3_complete16 0
#endif

#ifdef SQLITE_OMIT_DECLTYPE
# define sqlite3_column_decltype16      0
# define sqlite3_column_decltype        0
#endif

#ifdef SQLITE_OMIT_PROGRESS_CALLBACK
# define sqlite3_progress_handler 0
#endif

#ifdef SQLITE_OMIT_VIRTUALTABLE
# define sqlite3_create_module 0
# define sqlite3_create_module_v2 0
# define sqlite3_declare_vtab 0
# define sqlite3_vtab_config 0
# define sqlite3_vtab_on_conflict 0
#endif

#ifdef SQLITE_OMIT_SHARED_CACHE
# define sqlite3_enable_shared_cache 0
#endif

#ifdef SQLITE_OMIT_TRACE
# define sqlite3_profile       0
# define sqlite3_trace         0
#endif

#ifdef SQLITE_OMIT_GET_TABLE
# define sqlite3_free_table    0
# define sqlite3_get_table     0
#endif

#ifdef SQLITE_OMIT_INCRBLOB
#define sqlite3_bind_zeroblob  0
#define sqlite3_blob_bytes     0
#define sqlite3_blob_close     0
#define sqlite3_blob_open      0
#define sqlite3_blob_read      0
#define sqlite3_blob_write     0
#define sqlite3_blob_reopen    0
#endif

/*
** The following structure contains pointers to all SQLite API routines.
** A pointer to this structure is passed into extensions when they are
** loaded so that the extension can make calls back into the SQLite
** library.
**该函数的结构包含指向所有SQLite的API例程的指针，这个结构被传递到扩展中以至于可以调用的SQLite库

** When adding new APIs, add them to the bottom of this structure
** in order to preserve backwards compatibility.
**当增加新的API，将它们添加到这个结构的底部为了保持向后兼容。

** Extensions that use newer APIs should first call the
** sqlite3_libversion_number() to make sure that the API they
** intend to use is supported by the library.  Extensions should
** also check to make sure that the pointer to the function is
** not NULL before calling it.
使用新的API扩展，应首先调用sqlite3_libversion_number（）,以确保该API，他们拟将由库支持。
扩展应该还检查以确保该指针的函数不调用它之前NULL。
*/
static const sqlite3_api_routines sqlite3Apis = {
  sqlite3_aggregate_context,
#ifndef SQLITE_OMIT_DEPRECATED
  sqlite3_aggregate_count,
#else
  0,
#endif
  sqlite3_bind_blob,
  sqlite3_bind_double,
  sqlite3_bind_int,
  sqlite3_bind_int64,
  sqlite3_bind_null,
  sqlite3_bind_parameter_count,
  sqlite3_bind_parameter_index,
  sqlite3_bind_parameter_name,
  sqlite3_bind_text,
  sqlite3_bind_text16,
  sqlite3_bind_value,
  sqlite3_busy_handler,
  sqlite3_busy_timeout,
  sqlite3_changes,
  sqlite3_close,
  sqlite3_collation_needed,
  sqlite3_collation_needed16,
  sqlite3_column_blob,
  sqlite3_column_bytes,
  sqlite3_column_bytes16,
  sqlite3_column_count,
  sqlite3_column_database_name,
  sqlite3_column_database_name16,
  sqlite3_column_decltype,
  sqlite3_column_decltype16,
  sqlite3_column_double,
  sqlite3_column_int,
  sqlite3_column_int64,
  sqlite3_column_name,
  sqlite3_column_name16,
  sqlite3_column_origin_name,
  sqlite3_column_origin_name16,
  sqlite3_column_table_name,
  sqlite3_column_table_name16,
  sqlite3_column_text,
  sqlite3_column_text16,
  sqlite3_column_type,
  sqlite3_column_value,
  sqlite3_commit_hook,
  sqlite3_complete,
  sqlite3_complete16,
  sqlite3_create_collation,
  sqlite3_create_collation16,
  sqlite3_create_function,
  sqlite3_create_function16,
  sqlite3_create_module,
  sqlite3_data_count,
  sqlite3_db_handle,
  sqlite3_declare_vtab,
  sqlite3_enable_shared_cache,
  sqlite3_errcode,
  sqlite3_errmsg,
  sqlite3_errmsg16,
  sqlite3_exec,
#ifndef SQLITE_OMIT_DEPRECATED
  sqlite3_expired,
#else
  0,
#endif
  sqlite3_finalize,
  sqlite3_free,
  sqlite3_free_table,
  sqlite3_get_autocommit,
  sqlite3_get_auxdata,
  sqlite3_get_table,
  0,      /* Was sqlite3_global_recover(), but that function is deprecated
		      	是sqlite3全局覆盖函数，但是这个功能不赞成使用*/
  sqlite3_interrupt,
  sqlite3_last_insert_rowid,
  sqlite3_libversion,
  sqlite3_libversion_number,
  sqlite3_malloc,
  sqlite3_mprintf,
  sqlite3_open,
  sqlite3_open16,
  sqlite3_prepare,
  sqlite3_prepare16,
  sqlite3_profile,
  sqlite3_progress_handler,
  sqlite3_realloc,
  sqlite3_reset,
  sqlite3_result_blob,
  sqlite3_result_double,
  sqlite3_result_error,
  sqlite3_result_error16,
  sqlite3_result_int,
  sqlite3_result_int64,
  sqlite3_result_null,
  sqlite3_result_text,
  sqlite3_result_text16,
  sqlite3_result_text16be,
  sqlite3_result_text16le,
  sqlite3_result_value,
  sqlite3_rollback_hook,
  sqlite3_set_authorizer,
  sqlite3_set_auxdata,
  sqlite3_snprintf,
  sqlite3_step,
  sqlite3_table_column_metadata,
#ifndef SQLITE_OMIT_DEPRECATED
  sqlite3_thread_cleanup,
#else
  0,
#endif
  sqlite3_total_changes,
  sqlite3_trace,
#ifndef SQLITE_OMIT_DEPRECATED
  sqlite3_transfer_bindings,
#else
  0,
#endif
  sqlite3_update_hook,
  sqlite3_user_data,
  sqlite3_value_blob,
  sqlite3_value_bytes,
  sqlite3_value_bytes16,
  sqlite3_value_double,
  sqlite3_value_int,
  sqlite3_value_int64,
  sqlite3_value_numeric_type,
  sqlite3_value_text,
  sqlite3_value_text16,
  sqlite3_value_text16be,
  sqlite3_value_text16le,
  sqlite3_value_type,
  sqlite3_vmprintf,
  /*
  ** The original API set ends here.  All extensions can call any
  ** of the APIs above provided that the pointer is not NULL.  But
  ** before calling APIs that follow, extension should check the
  ** sqlite3_libversion_number() to make sure they are dealing with
  ** a library that is new enough to support that API.
  *************************************************************************
  */
  sqlite3_overload_function,

  /*
  ** Added after 3.3.13
  */
  sqlite3_prepare_v2,
  sqlite3_prepare16_v2,
  sqlite3_clear_bindings,

  /*
  ** Added for 3.4.1
  */
  sqlite3_create_module_v2,

  /*
  ** Added for 3.5.0
  */
  sqlite3_bind_zeroblob,
  sqlite3_blob_bytes,
  sqlite3_blob_close,
  sqlite3_blob_open,
  sqlite3_blob_read,
  sqlite3_blob_write,
  sqlite3_create_collation_v2,
  sqlite3_file_control,
  sqlite3_memory_highwater,
  sqlite3_memory_used,
#ifdef SQLITE_MUTEX_OMIT
  0, 
  0, 
  0,
  0,
  0,
#else
  sqlite3_mutex_alloc,
  sqlite3_mutex_enter,
  sqlite3_mutex_free,
  sqlite3_mutex_leave,
  sqlite3_mutex_try,
#endif
  sqlite3_open_v2,
  sqlite3_release_memory,
  sqlite3_result_error_nomem,
  sqlite3_result_error_toobig,
  sqlite3_sleep,
  sqlite3_soft_heap_limit,
  sqlite3_vfs_find,
  sqlite3_vfs_register,
  sqlite3_vfs_unregister,

  /*
  ** Added for 3.5.8
  */
  sqlite3_threadsafe,
  sqlite3_result_zeroblob,
  sqlite3_result_error_code,
  sqlite3_test_control,
  sqlite3_randomness,
  sqlite3_context_db_handle,

  /*
  ** Added for 3.6.0
  */
  sqlite3_extended_result_codes,
  sqlite3_limit,
  sqlite3_next_stmt,
  sqlite3_sql,
  sqlite3_status,

  /*
  ** Added for 3.7.4
  */
  sqlite3_backup_finish,
  sqlite3_backup_init,
  sqlite3_backup_pagecount,
  sqlite3_backup_remaining,
  sqlite3_backup_step,
#ifndef SQLITE_OMIT_COMPILEOPTION_DIAGS
  sqlite3_compileoption_get,
  sqlite3_compileoption_used,
#else
  0,
  0,
#endif
  sqlite3_create_function_v2,
  sqlite3_db_config,
  sqlite3_db_mutex,
  sqlite3_db_status,
  sqlite3_extended_errcode,
  sqlite3_log,
  sqlite3_soft_heap_limit64,
  sqlite3_sourceid,
  sqlite3_stmt_status,
  sqlite3_strnicmp,
#ifdef SQLITE_ENABLE_UNLOCK_NOTIFY
  sqlite3_unlock_notify,
#else
  0,
#endif
#ifndef SQLITE_OMIT_WAL
  sqlite3_wal_autocheckpoint,
  sqlite3_wal_checkpoint,
  sqlite3_wal_hook,
#else
  0,
  0,
  0,
#endif
  sqlite3_blob_reopen,
  sqlite3_vtab_config,
  sqlite3_vtab_on_conflict,
};

/*
** Attempt to load an SQLite extension library contained in the file   尝试在zFile文件中加载一个SQLite扩展库，这个入口点是zProc。
** zFile.  The entry point is zProc.  zProc may be 0 in which case a   在一个默认入口点名称（sqlite3_extension_init）被使用的情况下，zProc可能是0
** default entry point name (sqlite3_extension_init) is used.  Use     建议使用默认的名称
** of the default name is recommended.
**
** Return SQLITE_OK on success and SQLITE_ERROR if something goes wrong.返回SQLITE_OK成功，或者如果出现错误，返回SQLITE_ERROR
**
** If an error occurs and pzErrMsg is not 0, then fill *pzErrMsg with 如果发生错误并且pzErrMsg不为0，然后填写* pzErrMsg与错误消息文本。
** error message text.  The calling function should free this memory  调用函数应该通过调用sqlite3DbFree来释放此内存
** by calling sqlite3DbFree(db, ).
*/
static int sqlite3LoadExtension(
  sqlite3 *db,          //建立连接
  const char *zFile,    //共享库的名称
  const char *zProc,   //入口点，如果zProc=0，则使用  sqlite3_extension_init
  char **pzErrMsg       //存放错误信息
){
  sqlite3_vfs *pVfs = db->pVfs;
  void *handle;
  int (*xInit)(sqlite3*,char**,const sqlite3_api_routines*);
  char *zErrmsg = 0;
  void **aHandle;
  int nMsg = 300 + sqlite3Strlen30(zFile);

  if( pzErrMsg ) *pzErrMsg = 0;

  /* Ticket #1863.  To avoid a creating security problems for older
  ** applications that relink against newer versions of SQLite, the
  ** ability to run load_extension is turned off by default.  One
  ** must call sqlite3_enable_load_extension() to turn on extension
  ** loading.  Otherwise you get the following error.
  为了避免旧的应用程序重新链接对新版本的SQLite的造成安全问题，运行load_extension，它的功能是默认关闭的。
  必须调用sqlite3_enable_load_extension（）打开扩展负载。否则，你得到下面的错误。
  */
  if( (db->flags & SQLITE_LoadExtension)==0 ){
    if( pzErrMsg ){
      *pzErrMsg = sqlite3_mprintf("not authorized");
    }
    return SQLITE_ERROR;
  }//产生未授权的错误

  if( zProc==0 ){
    zProc = "sqlite3_extension_init";
  }

  handle = sqlite3OsDlOpen(pVfs, zFile);
  if( handle==0 ){
    if( pzErrMsg ){
      *pzErrMsg = zErrmsg = sqlite3_malloc(nMsg);
      if( zErrmsg ){
        sqlite3_snprintf(nMsg, zErrmsg, 
            "unable to open shared library [%s]", zFile);
        sqlite3OsDlError(pVfs, nMsg-1, zErrmsg);
      }
    }
    return SQLITE_ERROR;
  }
  xInit = (int(*)(sqlite3*,char**,const sqlite3_api_routines*))
                   sqlite3OsDlSym(pVfs, handle, zProc);
  if( xInit==0 ){
    if( pzErrMsg ){
      nMsg += sqlite3Strlen30(zProc);
      *pzErrMsg = zErrmsg = sqlite3_malloc(nMsg);
      if( zErrmsg ){
        sqlite3_snprintf(nMsg, zErrmsg,
            "no entry point [%s] in shared library [%s]", zProc,zFile);
        sqlite3OsDlError(pVfs, nMsg-1, zErrmsg);
      }
      sqlite3OsDlClose(pVfs, handle);
    }
    return SQLITE_ERROR;
  }else if( xInit(db, &zErrmsg, &sqlite3Apis) ){
    if( pzErrMsg ){
      *pzErrMsg = sqlite3_mprintf("error during initialization: %s", zErrmsg);
    }
    sqlite3_free(zErrmsg);
    sqlite3OsDlClose(pVfs, handle);
    return SQLITE_ERROR;
  }

  /* Append the new shared library handle to the db->aExtension array. 
  追加新的共享库句柄DB->扩展阵列*/
  aHandle = sqlite3DbMallocZero(db, sizeof(handle)*(db->nExtension+1));
  if( aHandle==0 ){
    return SQLITE_NOMEM;
  }
  if( db->nExtension>0 ){
    memcpy(aHandle, db->aExtension, sizeof(handle)*db->nExtension);
  }
  sqlite3DbFree(db, db->aExtension);
  db->aExtension = aHandle;

  db->aExtension[db->nExtension++] = handle;
  return SQLITE_OK;
}
int sqlite3_load_extension(
  sqlite3 *db,          /* Load the extension into this database connection建立连接 */
  const char *zFile,    /* Name of the shared library containing extension 共享库的名称*/
  const char *zProc,    /* Entry point.  Use "sqlite3_extension_init" if 0 *入口点，如果zProc=0，则使用  sqlite3_extension_init/
  char **pzErrMsg       /* Put error message here if not 0 存放错误信息*/
){
  int rc;
  sqlite3_mutex_enter(db->mutex);
  rc = sqlite3LoadExtension(db, zFile, zProc, pzErrMsg);
  rc = sqlite3ApiExit(db, rc);
  sqlite3_mutex_leave(db->mutex);
  return rc;
}

/*
** Call this routine when the database connection is closing in order
** to clean up loaded extensions该函数用来清理加载扩展项，当数据库连接被关闭时
*/
void sqlite3CloseExtensions(sqlite3 *db){
  int i;
  assert( sqlite3_mutex_held(db->mutex) );
  for(i=0; i<db->nExtension; i++){
    sqlite3OsDlClose(db->pVfs, db->aExtension[i]);
  }
  sqlite3DbFree(db, db->aExtension);
}

/*
** Enable or disable extension loading.  Extension loading is disabled by
** default so as not to open security holes in older applications.
调用这个函数来启用或禁用扩展负载。扩展加载默认是禁用的，以免在旧的应用程序打开安全漏洞。
*/
int sqlite3_enable_load_extension(sqlite3 *db, int onoff){
  sqlite3_mutex_enter(db->mutex);
  if( onoff ){                      //onoff是用来判断的是否禁用的标志
    db->flags |= SQLITE_LoadExtension;
  }else{
    db->flags &= ~SQLITE_LoadExtension;
  }
  sqlite3_mutex_leave(db->mutex);
  return SQLITE_OK;
}

#endif /* SQLITE_OMIT_LOAD_EXTENSION */

/*
** The auto-extension code added regardless of whether or not extension
** loading is supported.  We need a dummy sqlite3Apis pointer for that
** code if regular extension loading is not available.  This is that
** dummy pointer.
自动扩展代码必须加上无论扩展装载是否支持。如果经常扩展装载不可用，我们需要一个虚拟sqlite3Apis指针的代码。
*/
#ifdef SQLITE_OMIT_LOAD_EXTENSION
static const sqlite3_api_routines sqlite3Apis = { 0 };
#endif


/*
** The following object holds the list of automatically loaded
** extensions.
**定义一个具有自动加载的扩展列表的对象。

** This list is shared across threads.  The SQLITE_MUTEX_STATIC_MASTER
** mutex must be held while accessing this list.
这个列表是跨线程共享的。其中的SQLITE_MUTEX_STATIC_MASTER互斥必须在访问这个列表时进行
*/
typedef struct sqlite3AutoExtList sqlite3AutoExtList;
static SQLITE_WSD struct sqlite3AutoExtList {
  int nExt;              /* Number of entries in aExt[] */          
  void (**aExt)(void);   /* Pointers to the extension init functions */
} sqlite3Autoext = { 0, 0 };

/* The "wsdAutoext" macro will resolve to the autoextension
** state vector.  If writable static data is unsupported on the target,
** we have to locate the state vector at run-time.  In the more common
** case where writable static data is supported, wsdStat can refer directly
** to the "sqlite3Autoext" state vector declared above.
*/
#ifdef SQLITE_OMIT_WSD
# define wsdAutoextInit \
  sqlite3AutoExtList *x = &GLOBAL(sqlite3AutoExtList,sqlite3Autoext)
# define wsdAutoext x[0]
#else
# define wsdAutoextInit
# define wsdAutoext sqlite3Autoext
#endif


/*
** Register a statically linked extension that is automatically
** loaded by every new database connection.
此接口使xInit（）函数用于创建的每个新的数据库连接调用。
xInit（）为入口点的静态链接SQLite的扩展，将被自动加载到所有新的数据库连接
*/
int sqlite3_auto_extension(void (*xInit)(void)){
  int rc = SQLITE_OK;
#ifndef SQLITE_OMIT_AUTOINIT
  rc = sqlite3_initialize();
  if( rc ){
    return rc;
  }else
#endif
  {
    int i;
#if SQLITE_THREADSAFE
    sqlite3_mutex *mutex = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER);
#endif
    wsdAutoextInit;
    sqlite3_mutex_enter(mutex);
    for(i=0; i<wsdAutoext.nExt; i++){
      if( wsdAutoext.aExt[i]==xInit ) break;
    }
    if( i==wsdAutoext.nExt ){
      int nByte = (wsdAutoext.nExt+1)*sizeof(wsdAutoext.aExt[0]);
      void (**aNew)(void);
      aNew = sqlite3_realloc(wsdAutoext.aExt, nByte);
      if( aNew==0 ){
        rc = SQLITE_NOMEM;
      }else{
        wsdAutoext.aExt = aNew;
        wsdAutoext.aExt[wsdAutoext.nExt] = xInit;
        wsdAutoext.nExt++;
      }
    }
    sqlite3_mutex_leave(mutex);
    assert( (rc&0xff)==rc );
    return rc;
  }
}

/*
** Reset the automatic extension loading mechanism.重置自动扩展加载机制。
*/
void sqlite3_reset_auto_extension(void){
#ifndef SQLITE_OMIT_AUTOINIT
  if( sqlite3_initialize()==SQLITE_OK )
#endif
  {
#if SQLITE_THREADSAFE
    sqlite3_mutex *mutex = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER);
#endif
    wsdAutoextInit;
    sqlite3_mutex_enter(mutex);
    sqlite3_free(wsdAutoext.aExt);
    wsdAutoext.aExt = 0;
    wsdAutoext.nExt = 0;
    sqlite3_mutex_leave(mutex);
  }
}

/*
** Load all automatic extensions.
**加载所有自动扩展

** If anything goes wrong, set an error in the database connection.
如果出现任何错误，在数据库连接设置错误。
*/
void sqlite3AutoLoadExtensions(sqlite3 *db){
  int i;
  int go = 1;
  int rc;
  int (*xInit)(sqlite3*,char**,const sqlite3_api_routines*);

  wsdAutoextInit;
  if( wsdAutoext.nExt==0 ){
    /* Common case: early out without every having to acquire a mutex   常见情况：早期的无需获取互斥锁 */
    return;
  }
  for(i=0; go; i++){
    char *zErrmsg;
#if SQLITE_THREADSAFE
    sqlite3_mutex *mutex = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER);
#endif
    sqlite3_mutex_enter(mutex);
    if( i>=wsdAutoext.nExt ){
      xInit = 0;
      go = 0;
    }else{
      xInit = (int(*)(sqlite3*,char**,const sqlite3_api_routines*))
              wsdAutoext.aExt[i];
    }
    sqlite3_mutex_leave(mutex);
    zErrmsg = 0;
    if( xInit && (rc = xInit(db, &zErrmsg, &sqlite3Apis))!=0 ){
      sqlite3Error(db, rc,
            "automatic extension loading failed: %s", zErrmsg);
      go = 0;
    }
    sqlite3_free(zErrmsg);
  }
}
