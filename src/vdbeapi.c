/*
** 2004 May 26
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
** 这个文件包含用于实现api的一部分VDBE的代码。
*/
#include "sqliteInt.h"
#include "vdbeInt.h"

#ifndef SQLITE_OMIT_DEPRECATED
/*
**返回TRUE(非零)作为参数表示提供的声明需要重新编译。
\\无论执行环境在何时被修改，函数声明需要重新编译时，在某种程度上会改变sqlite3_prepare()生成程序执行环境的变化。
例如，如果新的函数或排序序列注册或者授权功能被添加或更改。
*/
int sqlite3_expired(sqlite3_stmt *pStmt){
  Vdbe *p = (Vdbe*)pStmt;
  return p==0 || p->expired;
}
#endif

/*
** Check on a Vdbe to make sure it has not been finalized.  Log
** an error and return true if it has been finalized (or is otherwise
** invalid).  Return false if it is ok.
\\检查Vdbe是否关闭，以确保它尚未最终确定。记录一个错误信息日志，并返回它是否已完成（或无效）。如果已完成则返回假。
*/
static int vdbeSafety(Vdbe *p){
  if( p->db==0 ){
    sqlite3_log(SQLITE_MISUSE, "API called with finalized prepared statement");
    return 1;
  }else{
    return 0;
  }
}
static int vdbeSafetyNotNull(Vdbe *p){
  if( p==0 ){
    sqlite3_log(SQLITE_MISUSE, "API called with NULL prepared statement");
    return 1;
  }else{
    return vdbeSafety(p);
  }
}

/*
** The following routine destroys a virtual machine that is created by
** the sqlite3_compile() routine. The integer returned is an SQLITE_
** success/failure code that describes the result of executing the virtual
** machine.
\\下面的程序将破坏由sqlite3_compile（）创建的虚拟机。返回的整型值是描述在执行该虚拟机的结果的SQLITE_成功/失败的代码。
**
** This routine sets the error code and string returned by
** sqlite3_errcode(), sqlite3_errmsg() and sqlite3_errmsg16().
这个程序设置错误代码和字符串由sqlite3_errcode（），sqlite3_errmsg（）和sqlite3_errmsg16（）返回。
*/
int sqlite3_finalize(sqlite3_stmt *pStmt){
  int rc;
  if( pStmt==0 ){
    /* IMPLEMENTATION-OF: R-57228-12904 Invoking sqlite3_finalize() on a NULL
    ** pointer is a harmless no-op. 
	implementation-of：r-57228-12904调用sqlite3_finalize()对一个空指针是一个无害的空操作。*/
    rc = SQLITE_OK;
  }else{
    Vdbe *v = (Vdbe*)pStmt;
    sqlite3 *db = v->db;
    if( vdbeSafety(v) ) return SQLITE_MISUSE_BKPT;
    sqlite3_mutex_enter(db->mutex);
    rc = sqlite3VdbeFinalize(v);
    rc = sqlite3ApiExit(db, rc);
    sqlite3LeaveMutexAndCloseZombie(db);
  }
  return rc;
}

/*
** Terminate the current execution of an SQL statement and reset it
** back to its starting state so that it can be reused. A success code from
** the prior execution is returned.
\\终止SQL语句的当前执行，并将其复位到其初始状态，以便它可以重复使用。来自优先执行的成功编号被返回.
**
** This routine sets the error code and string returned by
** sqlite3_errcode(), sqlite3_errmsg() and sqlite3_errmsg16().
\\这个程序设置错误代码和字符串由sqlite3_errcode（），sqlite3_errmsg（）和sqlite3_errmsg16（）返回对应的错误信息。
*/
int sqlite3_reset(sqlite3_stmt *pStmt){
  int rc;
  if( pStmt==0 ){
    rc = SQLITE_OK;
  }else{
    Vdbe *v = (Vdbe*)pStmt;
    sqlite3_mutex_enter(v->db->mutex);
    rc = sqlite3VdbeReset(v);
    sqlite3VdbeRewind(v);
    assert( (rc & (v->db->errMask))==rc );
    rc = sqlite3ApiExit(v->db, rc);
    sqlite3_mutex_leave(v->db->mutex);
  }
  return rc;
}

/*
** Set all the parameters in the compiled SQL statement to NULL.
设置所有编译SQL语句中的参数为NULL。
*/
int sqlite3_clear_bindings(sqlite3_stmt *pStmt){
  int i;
  int rc = SQLITE_OK;
  Vdbe *p = (Vdbe*)pStmt;
#if SQLITE_THREADSAFE
  sqlite3_mutex *mutex = ((Vdbe*)pStmt)->db->mutex;
#endif
  sqlite3_mutex_enter(mutex);
  for(i=0; i<p->nVar; i++){
    sqlite3VdbeMemRelease(&p->aVar[i]);
    p->aVar[i].flags = MEM_Null;
  }
  if( p->isPrepareV2 && p->expmask ){
    p->expired = 1;
  }
  sqlite3_mutex_leave(mutex);
  return rc;
}


/**************************** sqlite3_value_  *******************************
** The following routines extract information from a Mem or sqlite3_value
** structure.
以下代码从Mem或sqlite3_value结构中提取信息。
*/
const void *sqlite3_value_blob(sqlite3_value *pVal){
  Mem *p = (Mem*)pVal;
  if( p->flags & (MEM_Blob|MEM_Str) ){
    sqlite3VdbeMemExpandBlob(p);
    p->flags &= ~MEM_Str;
    p->flags |= MEM_Blob;
    return p->n ? p->z : 0;
  }else{
    return sqlite3_value_text(pVal);
  }
}
int sqlite3_value_bytes(sqlite3_value *pVal){
  return sqlite3ValueBytes(pVal, SQLITE_UTF8);
}
int sqlite3_value_bytes16(sqlite3_value *pVal){
  return sqlite3ValueBytes(pVal, SQLITE_UTF16NATIVE);
}
double sqlite3_value_double(sqlite3_value *pVal){
  return sqlite3VdbeRealValue((Mem*)pVal);
}
int sqlite3_value_int(sqlite3_value *pVal){
  return (int)sqlite3VdbeIntValue((Mem*)pVal);
}
sqlite_int64 sqlite3_value_int64(sqlite3_value *pVal){
  return sqlite3VdbeIntValue((Mem*)pVal);
}
const unsigned char *sqlite3_value_text(sqlite3_value *pVal){
  return (const unsigned char *)sqlite3ValueText(pVal, SQLITE_UTF8);
}
#ifndef SQLITE_OMIT_UTF16
const void *sqlite3_value_text16(sqlite3_value* pVal){
  return sqlite3ValueText(pVal, SQLITE_UTF16NATIVE);
}
const void *sqlite3_value_text16be(sqlite3_value *pVal){
  return sqlite3ValueText(pVal, SQLITE_UTF16BE);
}
const void *sqlite3_value_text16le(sqlite3_value *pVal){
  return sqlite3ValueText(pVal, SQLITE_UTF16LE);
}
#endif /* SQLITE_OMIT_UTF16 */
int sqlite3_value_type(sqlite3_value* pVal){
  return pVal->type;
}

/**************************** sqlite3_result_  *******************************
** The following routines are used by user-defined functions to specify
** the function result.
**下面的例程是由用户定义函数来指定功能结果的。
** The setStrOrError() funtion calls sqlite3VdbeMemSetStr() to store the
** result as a string or blob but if the string or blob is too large, it
** then sets the error code to SQLITE_TOOBIG
setstrorerror()函数调用的sqlite3vdbememsetstr()存储结果为一个字符串或BLOB，但如果字符串或BLOB太大，则设置错误代码SQLITE_TOOBIG
*/
static void setResultStrOrError(
  sqlite3_context *pCtx,  /* Function context 函数的上下文*/
  const char *z,          /* String pointer字符串指针 */
  int n,                  /* Bytes in string, or negative 字节串，或负*/
  u8 enc,                 /* Encoding of z.  0 for BLOBs z的编码。 0的BLOB*/
  void (*xDel)(void*)     /* Destructor function析构函数 */
){
  if( sqlite3VdbeMemSetStr(&pCtx->s, z, n, enc, xDel)==SQLITE_TOOBIG ){
    sqlite3_result_error_toobig(pCtx);
  }
}
void sqlite3_result_blob(
  sqlite3_context *pCtx, 
  const void *z, 
  int n, 
  void (*xDel)(void *)
){
  assert( n>=0 );
  assert( sqlite3_mutex_held(pCtx->s.db->mutex) );
  setResultStrOrError(pCtx, z, n, 0, xDel);
}
void sqlite3_result_double(sqlite3_context *pCtx, double rVal){
  assert( sqlite3_mutex_held(pCtx->s.db->mutex) );
  sqlite3VdbeMemSetDouble(&pCtx->s, rVal);
}
void sqlite3_result_error(sqlite3_context *pCtx, const char *z, int n){
  assert( sqlite3_mutex_held(pCtx->s.db->mutex) );
  pCtx->isError = SQLITE_ERROR;
  sqlite3VdbeMemSetStr(&pCtx->s, z, n, SQLITE_UTF8, SQLITE_TRANSIENT);
}
#ifndef SQLITE_OMIT_UTF16
void sqlite3_result_error16(sqlite3_context *pCtx, const void *z, int n){
  assert( sqlite3_mutex_held(pCtx->s.db->mutex) );
  pCtx->isError = SQLITE_ERROR;
  sqlite3VdbeMemSetStr(&pCtx->s, z, n, SQLITE_UTF16NATIVE, SQLITE_TRANSIENT);
}
#endif
void sqlite3_result_int(sqlite3_context *pCtx, int iVal){
  assert( sqlite3_mutex_held(pCtx->s.db->mutex) );
  sqlite3VdbeMemSetInt64(&pCtx->s, (i64)iVal);
}
void sqlite3_result_int64(sqlite3_context *pCtx, i64 iVal){
  assert( sqlite3_mutex_held(pCtx->s.db->mutex) );
  sqlite3VdbeMemSetInt64(&pCtx->s, iVal);
}
void sqlite3_result_null(sqlite3_context *pCtx){
  assert( sqlite3_mutex_held(pCtx->s.db->mutex) );
  sqlite3VdbeMemSetNull(&pCtx->s);
}
void sqlite3_result_text(
  sqlite3_context *pCtx, 
  const char *z, 
  int n,
  void (*xDel)(void *)
){
  assert( sqlite3_mutex_held(pCtx->s.db->mutex) );
  setResultStrOrError(pCtx, z, n, SQLITE_UTF8, xDel);
}
#ifndef SQLITE_OMIT_UTF16
void sqlite3_result_text16(
  sqlite3_context *pCtx, 
  const void *z, 
  int n, 
  void (*xDel)(void *)
){
  assert( sqlite3_mutex_held(pCtx->s.db->mutex) );
  setResultStrOrError(pCtx, z, n, SQLITE_UTF16NATIVE, xDel);
}
void sqlite3_result_text16be(
  sqlite3_context *pCtx, 
  const void *z, 
  int n, 
  void (*xDel)(void *)
){
  assert( sqlite3_mutex_held(pCtx->s.db->mutex) );
  setResultStrOrError(pCtx, z, n, SQLITE_UTF16BE, xDel);
}
void sqlite3_result_text16le(
  sqlite3_context *pCtx, 
  const void *z, 
  int n, 
  void (*xDel)(void *)
){
  assert( sqlite3_mutex_held(pCtx->s.db->mutex) );
  setResultStrOrError(pCtx, z, n, SQLITE_UTF16LE, xDel);
}
#endif /* SQLITE_OMIT_UTF16 */
void sqlite3_result_value(sqlite3_context *pCtx, sqlite3_value *pValue){
  assert( sqlite3_mutex_held(pCtx->s.db->mutex) );
  sqlite3VdbeMemCopy(&pCtx->s, pValue);
}
void sqlite3_result_zeroblob(sqlite3_context *pCtx, int n){
  assert( sqlite3_mutex_held(pCtx->s.db->mutex) );
  sqlite3VdbeMemSetZeroBlob(&pCtx->s, n);
}
void sqlite3_result_error_code(sqlite3_context *pCtx, int errCode){
  pCtx->isError = errCode;
  if( pCtx->s.flags & MEM_Null ){
    sqlite3VdbeMemSetStr(&pCtx->s, sqlite3ErrStr(errCode), -1, 
                         SQLITE_UTF8, SQLITE_STATIC);
  }
}

/* Force an SQLITE_TOOBIG error.强制SQLITE_TOOBIG错误。 */
void sqlite3_result_error_toobig(sqlite3_context *pCtx){
  assert( sqlite3_mutex_held(pCtx->s.db->mutex) );
  pCtx->isError = SQLITE_TOOBIG;
  sqlite3VdbeMemSetStr(&pCtx->s, "string or blob too big", -1, 
                       SQLITE_UTF8, SQLITE_STATIC);
}

/* An SQLITE_NOMEM error. 一个SQLITE_NOMEM错误*/
void sqlite3_result_error_nomem(sqlite3_context *pCtx){
  assert( sqlite3_mutex_held(pCtx->s.db->mutex) );
  sqlite3VdbeMemSetNull(&pCtx->s);
  pCtx->isError = SQLITE_NOMEM;
  pCtx->s.db->mallocFailed = 1;
}

/*
** This function is called after a transaction has been committed. It 
** invokes callbacks registered with sqlite3_wal_hook() as required.
\\此功能是在事务提交后调用的函数会被唤醒。它会按要求调用与sqlite3_wal_hook()要求的注册回调函数。
*/
static int doWalCallbacks(sqlite3 *db){
  int rc = SQLITE_OK;
#ifndef SQLITE_OMIT_WAL
  int i;
  for(i=0; i<db->nDb; i++){
    Btree *pBt = db->aDb[i].pBt;
    if( pBt ){
      int nEntry = sqlite3PagerWalCallback(sqlite3BtreePager(pBt));
      if( db->xWalCallback && nEntry>0 && rc==SQLITE_OK ){
        rc = db->xWalCallback(db->pWalArg, db, db->aDb[i].zName, nEntry);
      }
    }
  }
#endif
  return rc;
}

/*
** Execute the statement pStmt, either until a row of data is ready, the
** statement is completely executed or an error occurs.
**执行该语句pStmt，或者直到一行数据已经准备好，该语句执行完毕或发生错误。
** This routine implements the bulk of the logic behind the sqlite_step()
** API.  The only thing omitted is the automatic recompile if a 
** schema change has occurred.  That detail is handled by the
** outer sqlite3_step() wrapper procedure.
\\此例程实现大部分sqlite_step（）API背后的逻辑。唯一省略的是如果发生了架构更改就会产生自动重新编译。这个细节被外sqlite3_step（）包装过程处理。
*/
static int sqlite3Step(Vdbe *p){
  sqlite3 *db;
  int rc;

  assert(p);
  if( p->magic!=VDBE_MAGIC_RUN ){
    /* We used to require that sqlite3_reset() be called before retrying
    ** sqlite3_step() after any error or after SQLITE_DONE.  But beginning
    ** with version 3.7.0, we changed this so that sqlite3_reset() would
    ** be called automatically instead of throwing the SQLITE_MISUSE error.
    ** This "automatic-reset" change is not technically an incompatibility, 
    ** since any application that receives an SQLITE_MISUSE is broken by
    ** definition.
    **\\我们曾经要求sqlite3_reset（）被重试sqlite3_step（）之前之后的任何错误或SQLITE_DONE后唤醒。
	但是，随着3.7.0版本开始，我们改变了这一点，以便sqlite3_reset（）将被自动唤醒，而不是抛出的SQLITE_MISUSE错误。
	这种“自动复位”的变化是不是技术上的不兼容，因为其接收SQLITE_MISUSE被定义打破任何应用程序。
    ** Nevertheless, some published applications that were originally written
    ** for version 3.6.23 or earlier do in fact depend on SQLITE_MISUSE 
    ** returns, and those were broken by the automatic-reset change.  As a
    ** a work-around, the SQLITE_OMIT_AUTORESET compile-time restores the
    ** legacy behavior of returning SQLITE_MISUSE for cases where the 
    ** previous sqlite3_step() returned something other than a SQLITE_LOCKED
    ** or SQLITE_BUSY error.
	\\然而,一些出版的最初是写给3.6.23或更早版本的应用程序做实际上取决于SQLITE_MISUSE返回,和那些被自动重置改变。
	一个变通方法,SQLITE_OMIT_AUTORESET编译时恢复传统行为的情况下返回SQLITE_MISUSE前sqlite3_step()返回SQLITE_LOCKED或SQLITE_BUSY错误以外的东西。
    */
#ifdef SQLITE_OMIT_AUTORESET
    if( p->rc==SQLITE_BUSY || p->rc==SQLITE_LOCKED ){
      sqlite3_reset((sqlite3_stmt*)p);
    }else{
      return SQLITE_MISUSE_BKPT;
    }
#else
    sqlite3_reset((sqlite3_stmt*)p);
#endif
  }

  /* Check that malloc() has not failed. If it has, return early. 检查的malloc（）并没有失败。如果是，提前返回。*/
  db = p->db;
  if( db->mallocFailed ){
    p->rc = SQLITE_NOMEM;
    return SQLITE_NOMEM;
  }

  if( p->pc<=0 && p->expired ){
    p->rc = SQLITE_SCHEMA;
    rc = SQLITE_ERROR;
    goto end_of_step;
  }
  if( p->pc<0 ){
    /* If there are no other statements currently running, then
    ** reset the interrupt flag.  This prevents a call to sqlite3_interrupt
    ** from interrupting a statement that has not yet started.
	如果没有其他语句，当前正在运行，则重置中断标志。这可以防止调用sqlite3_interrupt打断尚未开始的声明。
    */
    if( db->activeVdbeCnt==0 ){
      db->u1.isInterrupted = 0;
    }

    assert( db->writeVdbeCnt>0 || db->autoCommit==0 || db->nDeferredCons==0 );

#ifndef SQLITE_OMIT_TRACE
    if( db->xProfile && !db->init.busy ){
      sqlite3OsCurrentTimeInt64(db->pVfs, &p->startTime);
    }
#endif

    db->activeVdbeCnt++;
    if( p->readOnly==0 ) db->writeVdbeCnt++;
    p->pc = 0;
  }
#ifndef SQLITE_OMIT_EXPLAIN
  if( p->explain ){
    rc = sqlite3VdbeList(p);
  }else
#endif /* SQLITE_OMIT_EXPLAIN */
  {
    db->vdbeExecCnt++;
    rc = sqlite3VdbeExec(p);
    db->vdbeExecCnt--;
  }

#ifndef SQLITE_OMIT_TRACE
  /* Invoke the profile callback if there is one如果有一个配置文件，则调用
  */
  if( rc!=SQLITE_ROW && db->xProfile && !db->init.busy && p->zSql ){
    sqlite3_int64 iNow;
    sqlite3OsCurrentTimeInt64(db->pVfs, &iNow);
    db->xProfile(db->pProfileArg, p->zSql, (iNow - p->startTime)*1000000);
  }
#endif

  if( rc==SQLITE_DONE ){
    assert( p->rc==SQLITE_OK );
    p->rc = doWalCallbacks(db);
    if( p->rc!=SQLITE_OK ){
      rc = SQLITE_ERROR;
    }
  }

  db->errCode = rc;
  if( SQLITE_NOMEM==sqlite3ApiExit(p->db, p->rc) ){
    p->rc = SQLITE_NOMEM;
  }
end_of_step:
  /* At this point local variable rc holds the value that should be 
  ** returned if this statement was compiled using the legacy 
  ** sqlite3_prepare() interface. According to the docs, this can only
  ** be one of the values in the first assert() below. Variable p->rc 
  ** contains the value that would be returned if sqlite3_finalize() 
  ** were called on statement p.
  \\如果使用传统的sqlite3_prepare（）接口编译这个声明返回的值，此时局部变量rc有效，这个值就会被返回。
  根据文档，这只能是在第一个assert()中的一个值。如果sqlite3_finalize()访问声明p，变量p->rc的值将会返回。
  */
  assert( rc==SQLITE_ROW  || rc==SQLITE_DONE   || rc==SQLITE_ERROR 
       || rc==SQLITE_BUSY || rc==SQLITE_MISUSE
  );
  assert( p->rc!=SQLITE_ROW && p->rc!=SQLITE_DONE );
  if( p->isPrepareV2 && rc!=SQLITE_ROW && rc!=SQLITE_DONE ){
    /* If this statement was prepared using sqlite3_prepare_v2(), and an
    ** error has occured, then return the error code in p->rc to the
    ** caller. Set the error code in the database handle to the same value.
	如果准备使用sqlite3_prepare_v2()这样的语句，并发生了一个错误，则返回错误代码给P＞rc的调用者。
	在数据库句柄错误代码设置为相同的值。
    */ 
    rc = sqlite3VdbeTransferError(p);
  }
  return (rc&db->errMask);
}

/*
** The maximum number of times that a statement will try to reparse
** itself before giving up and returning SQLITE_SCHEMA.
\\一个语句将尝试放弃并返回SQLITE_SCHEMA之前重新编译次数超过最大重新分析次数。
*/
#ifndef SQLITE_MAX_SCHEMA_RETRY
# define SQLITE_MAX_SCHEMA_RETRY 5
#endif

/*
** This is the top-level implementation of sqlite3_step().  Call
** sqlite3Step() to do most of the work.  If a schema error occurs,
** call sqlite3Reprepare() and try again.
这是sqlite3_step()顶层实现。调用sqlite3step()做大部分的工作。
如果一个模式出现错误，调用sqlite3reprepare()重试。
*/
int sqlite3_step(sqlite3_stmt *pStmt){
  int rc = SQLITE_OK;      /* Result from sqlite3Step() sqlite3step()的结果*/
  int rc2 = SQLITE_OK;     /* Result from sqlite3Reprepare() sqlite3reprepare()的结果*/
  Vdbe *v = (Vdbe*)pStmt;  /* the prepared statement 预处理语句*/
  int cnt = 0;             /* Counter to prevent infinite loop of reprepares 计数器，以防止reprepares的死循环*/
  sqlite3 *db;             /* The database connection 数据库连接*/

  if( vdbeSafetyNotNull(v) ){
    return SQLITE_MISUSE_BKPT;
  }
  db = v->db;
  sqlite3_mutex_enter(db->mutex);
  while( (rc = sqlite3Step(v))==SQLITE_SCHEMA
         && cnt++ < SQLITE_MAX_SCHEMA_RETRY
         && (rc2 = rc = sqlite3Reprepare(v))==SQLITE_OK ){
    sqlite3_reset(pStmt);
    assert( v->expired==0 );
  }
  if( rc2!=SQLITE_OK && ALWAYS(v->isPrepareV2) && ALWAYS(db->pErr) ){
    /* This case occurs after failing to recompile an sql statement. 当一条sql语句编译出错时这个事件发生。
    ** The error message from the SQL compiler has already been loaded 
    ** into the database handle. This block copies the error message 这个SQL错误已经加载在数据库句柄中。这个块从数据库句柄中复制错误信息。
	并且设置语句程序计数器为0以确保当这个语句完成或者重置分析器错误信息可以通过sqlite3_errmsg()和sqlite3_errcode()获得
    ** from the database handle into the statement and sets the statement
    ** program counter to 0 to ensure that when the statement is 
    ** finalized or reset the parser error message is available via
    ** sqlite3_errmsg() and sqlite3_errcode().
    */
    const char *zErr = (const char *)sqlite3_value_text(db->pErr); 
    sqlite3DbFree(db, v->zErrMsg);
    if( !db->mallocFailed ){
      v->zErrMsg = sqlite3DbStrDup(db, zErr);
      v->rc = rc2;
    } else {
      v->zErrMsg = 0;
      v->rc = rc = SQLITE_NOMEM;
    }
  }
  rc = sqlite3ApiExit(db, rc);
  sqlite3_mutex_leave(db->mutex);
  return rc;
}

/*
** Extract the user data from a sqlite3_context structure and return a
** pointer to it.从一个sqlite3_context结构提取用户信息并返回一个指向它的指针
*/
void *sqlite3_user_data(sqlite3_context *p){
  assert( p && p->pFunc );
  return p->pFunc->pUserData;
}

/*
** Extract the user data from a sqlite3_context structure and return a
** pointer to it.从一个sqlite3_context结构提取用户信息并返回一个指向它的指针
**
** IMPLEMENTATION-OF: R-46798-50301 The sqlite3_context_db_handle() interface
** returns a copy of the pointer to the database connection (the 1st
** parameter) of the sqlite3_create_function() and
** sqlite3_create_function16() routines that originally registered the
** application defined function.
sqlite3_context_db_handle()接口返回连接数据库指针的副本到（第一个参数）sqlite3_create_function()和sqlite3_create_function16()最初
注册的应用程序中定义的功能
*/
sqlite3 *sqlite3_context_db_handle(sqlite3_context *p){
  assert( p && p->pFunc );
  return p->s.db;
}

/*
** The following is the implementation of an SQL function that always
** fails with an error message stating that the function is used in the
** wrong context.  The sqlite3_overload_function() API might construct
** SQL function that use this routine so that the functions will exist
** for name resolution but are actually overloaded by the xFindFunction
** method of virtual tables.\\以下是总是失败并显示错误消息，指出该功能用于在错误的情况下SQL函数的实现。sqlite3_overload_function()API可以构成一个功能，使用这个程序使得这些功能
会存在域名解析，但实际上是重载的虚拟表的xfindfunction方法。
*/
void sqlite3InvalidFunction(
  sqlite3_context *context,  /* The function calling context 函数调用上下文*/
  int NotUsed,               /* Number of arguments to the function函数的参数数 */
  sqlite3_value **NotUsed2   /* Value of each argument每个参数值 */
){
  const char *zName = context->pFunc->zName;
  char *zErr;
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  zErr = sqlite3_mprintf(
      "unable to use function %s in the requested context", zName);
  sqlite3_result_error(context, zErr, -1);
  sqlite3_free(zErr);
}

/*
** Allocate or return the aggregate context for a user function.  A new
** context is allocated on the first call.  Subsequent calls return the
** same context that was returned on prior calls.
为用户函数定位并返回上下文集合。一个新的上下文被分配给一个新的调用，随后的调用返回与之前调用一样的上下文
*/
void *sqlite3_aggregate_context(sqlite3_context *p, int nByte){
  Mem *pMem;
  assert( p && p->pFunc && p->pFunc->xStep );
  assert( sqlite3_mutex_held(p->s.db->mutex) );
  pMem = p->pMem;
  testcase( nByte<0 );
  if( (pMem->flags & MEM_Agg)==0 ){
    if( nByte<=0 ){
      sqlite3VdbeMemReleaseExternal(pMem);
      pMem->flags = MEM_Null;
      pMem->z = 0;
    }else{
      sqlite3VdbeMemGrow(pMem, nByte, 0);
      pMem->flags = MEM_Agg;
      pMem->u.pDef = p->pFunc;
      if( pMem->z ){
        memset(pMem->z, 0, nByte);
      }
    }
  }
  return (void*)pMem->z;
}

/*
** Return the auxilary data pointer, if any, for the iArg'th argument to
** the user-function defined by pCtx.返回辅助数据指针，通过pCtx为iArg参数的用户函数下定义
*/
void *sqlite3_get_auxdata(sqlite3_context *pCtx, int iArg){
  VdbeFunc *pVdbeFunc;

  assert( sqlite3_mutex_held(pCtx->s.db->mutex) );
  pVdbeFunc = pCtx->pVdbeFunc;
  if( !pVdbeFunc || iArg>=pVdbeFunc->nAux || iArg<0 ){
    return 0;
  }
  return pVdbeFunc->apAux[iArg].pAux;
}

/*
** Set the auxilary data pointer and delete function, for the iArg'th
** argument to the user-function defined by pCtx. Any previous value is
** deleted by calling the delete function specified when it was set.
设置辅助数据的指针并且删除函数，通过pCtx为iArg参数的用户函数下定义。
当它被设置的时候，调用指定的删除函数删除任何先前的值
*/
void sqlite3_set_auxdata(
  sqlite3_context *pCtx, 
  int iArg, 
  void *pAux, 
  void (*xDelete)(void*)
){
  struct AuxData *pAuxData;
  VdbeFunc *pVdbeFunc;
  if( iArg<0 ) goto failed;

  assert( sqlite3_mutex_held(pCtx->s.db->mutex) );
  pVdbeFunc = pCtx->pVdbeFunc;
  if( !pVdbeFunc || pVdbeFunc->nAux<=iArg ){
    int nAux = (pVdbeFunc ? pVdbeFunc->nAux : 0);
    int nMalloc = sizeof(VdbeFunc) + sizeof(struct AuxData)*iArg;
    pVdbeFunc = sqlite3DbRealloc(pCtx->s.db, pVdbeFunc, nMalloc);
    if( !pVdbeFunc ){
      goto failed;
    }
    pCtx->pVdbeFunc = pVdbeFunc;
    memset(&pVdbeFunc->apAux[nAux], 0, sizeof(struct AuxData)*(iArg+1-nAux));
    pVdbeFunc->nAux = iArg+1;
    pVdbeFunc->pFunc = pCtx->pFunc;
  }

  pAuxData = &pVdbeFunc->apAux[iArg];
  if( pAuxData->pAux && pAuxData->xDelete ){
    pAuxData->xDelete(pAuxData->pAux);
  }
  pAuxData->pAux = pAux;
  pAuxData->xDelete = xDelete;
  return;

failed:
  if( xDelete ){
    xDelete(pAux);
  }
}

#ifndef SQLITE_OMIT_DEPRECATED
/*
** Return the number of times the Step function of a aggregate has been 
** called.\\返回一个被调用的聚集函数的次数
**
** This function is deprecated.  Do not use it for new code.  It is
** provide only to avoid breaking legacy code.  New aggregate function
** implementations should keep their own counts within their aggregate
** context.这个功能是被反对的。不能它在新的代码上。它只在避免打破遗留的代码被提供。
\\新的聚集函数的执行可以保持他们自己的总数在他们上下文集合内
*/
int sqlite3_aggregate_count(sqlite3_context *p){
  assert( p && p->pMem && p->pFunc && p->pFunc->xStep );
  return p->pMem->n;
}
#endif

/*
** Return the number of columns in the result set for the statement pStmt.
返回语句pstmt的结果集列数。
*/
int sqlite3_column_count(sqlite3_stmt *pStmt){
  Vdbe *pVm = (Vdbe *)pStmt;
  return pVm ? pVm->nResColumn : 0;
}

/*
** Return the number of values available from the current row of the
** currently executing statement pStmt.
返回从当前执行的语句pStmt的当前行获取值得数量
*/
int sqlite3_data_count(sqlite3_stmt *pStmt){
  Vdbe *pVm = (Vdbe *)pStmt;
  if( pVm==0 || pVm->pResultSet==0 ) return 0;
  return pVm->nResColumn;
}


/*
** Check to see if column iCol of the given statement is valid.  If
** it is, return a pointer to the Mem for the value of that column.
** If iCol is not valid, return a pointer to a Mem which has a value
** of NULL.
检查给定的列iCol声明是有效的。如果是,返回一个指针指向Mem的这一列的值。如果iCol无效,返回一个指针指向一个Mem的值为NULL。
*/
static Mem *columnMem(sqlite3_stmt *pStmt, int i){
  Vdbe *pVm;
  Mem *pOut;

  pVm = (Vdbe *)pStmt;
  if( pVm && pVm->pResultSet!=0 && i<pVm->nResColumn && i>=0 ){
    sqlite3_mutex_enter(pVm->db->mutex);
    pOut = &pVm->pResultSet[i];
  }else{
    /* If the value passed as the second argument is out of range, return
    ** a pointer to the following static Mem object which contains the
    ** value SQL NULL. Even though the Mem structure contains an element
    ** of type i64, on certain architectures (x86) with certain compiler
    ** switches (-Os), gcc may align this Mem object on a 4-byte boundary
    ** instead of an 8-byte one. This all works fine, except that when
    ** running with SQLITE_DEBUG defined the SQLite code sometimes assert()s
    ** that a Mem structure is located on an 8-byte boundary. To prevent
    ** these assert()s from failing, when building with SQLITE_DEBUG defined
    ** using gcc, we force nullMem to be 8-byte aligned using the magical
    ** __attribute__((aligned(8))) macro.  
	如果一个值作为第二个依据被通过是在范围之外的，返回一个指针指向后面的包含值SQL NULL的静态Mem对象。
	尽管Mem结构包含一个i64类型元素，在某一结构（x86）和某一编辑器开关（-Os），编译器可能排列这个Mem
	对象在一个4-byte范围内代替8-byte。这一切都是好的，除了有时当运行assert（）与SQLITE_DEBUG规定的一个Mem
	结构的位于一个8-byte范围内的SQLite代码。为了防止这些assert()出错，当构建SQLITE_DEBUG规定使用的编译器
	我们强制nullMem是8-byte对齐使用不可思议的__attribute__((aligned(8)))宏指令*/
    static const Mem nullMem 
#if defined(SQLITE_DEBUG) && defined(__GNUC__)
      __attribute__((aligned(8))) 
#endif
      = {0, "", (double)0, {0}, 0, MEM_Null, SQLITE_NULL, 0,
#ifdef SQLITE_DEBUG
         0, 0,  /* pScopyFrom, pFiller */
#endif
         0, 0 };

    if( pVm && ALWAYS(pVm->db) ){
      sqlite3_mutex_enter(pVm->db->mutex);
      sqlite3Error(pVm->db, SQLITE_RANGE, 0);
    }
    pOut = (Mem*)&nullMem;
  }
  return pOut;
}

/*
** This function is called after invoking an sqlite3_value_XXX function on a 
** column value (i.e. a value returned by evaluating an SQL expression in the
** select list of a SELECT statement) that may cause a malloc() failure. If 
** malloc() has failed, the threads mallocFailed flag is cleared and the result
** code of statement pStmt set to SQLITE_NOMEM.
在一个列值上执行一个sqlite3_value_XXX功能之后这个函数被调用（i.e. a值被返回通过评价一个SQL语句
在选择SELECT语句列表中。）这可能造成一个malloc()失败，如果malloc()出错，线程mallocFailed标志被
清除并且pStmt语句的结果代码被设置为SQLITE_NOMEM。
**
** Specifically, this is called from within具体而言，这是从内部调用:
**
**     sqlite3_column_int()
**     sqlite3_column_int64()
**     sqlite3_column_text()
**     sqlite3_column_text16()
**     sqlite3_column_real()
**     sqlite3_column_bytes()
**     sqlite3_column_bytes16()
**     sqiite3_column_blob()
*/
static void columnMallocFailure(sqlite3_stmt *pStmt)
{
  /* If malloc() failed during an encoding conversion within an
  ** sqlite3_column_XXX API, then set the return code of the statement to
  ** SQLITE_NOMEM. The next call to _step() (if any) will return SQLITE_ERROR
  ** and _finalize() will return NOMEM.
  如果malloc()失败，在一个sqlite3_column_XXX API之内一个编码转换期间，这时设置返回语句SQLITE_NOMEM代码。
  下一次调用_step()（如果真的有的话）将返回SQLITE_ERROR并且_finalize()将返回NOMEM。
  */
  Vdbe *p = (Vdbe *)pStmt;
  if( p ){
    p->rc = sqlite3ApiExit(p->db, p->rc);
    sqlite3_mutex_leave(p->db->mutex);
  }
}

/**************************** sqlite3_column_  *******************************
** The following routines are used to access elements of the current row
** in the result set.
下面的程序用于结果集中的当前行的元素入口。
*/
const void *sqlite3_column_blob(sqlite3_stmt *pStmt, int i){
  const void *val;
  val = sqlite3_value_blob( columnMem(pStmt,i) );
  /* Even though there is no encoding conversion, value_blob() might
  ** need to call malloc() to expand the result of a zeroblob() 
  ** expression. 
  尽管没有编码转换，value_blob()可能需要调用malloc()扩大一个zeroblob()表达式的结果。 
  */
  columnMallocFailure(pStmt);
  return val;
}
int sqlite3_column_bytes(sqlite3_stmt *pStmt, int i){
  int val = sqlite3_value_bytes( columnMem(pStmt,i) );
  columnMallocFailure(pStmt);
  return val;
}
int sqlite3_column_bytes16(sqlite3_stmt *pStmt, int i){
  int val = sqlite3_value_bytes16( columnMem(pStmt,i) );
  columnMallocFailure(pStmt);
  return val;
}
double sqlite3_column_double(sqlite3_stmt *pStmt, int i){
  double val = sqlite3_value_double( columnMem(pStmt,i) );
  columnMallocFailure(pStmt);
  return val;
}
int sqlite3_column_int(sqlite3_stmt *pStmt, int i){
  int val = sqlite3_value_int( columnMem(pStmt,i) );
  columnMallocFailure(pStmt);
  return val;
}
sqlite_int64 sqlite3_column_int64(sqlite3_stmt *pStmt, int i){
  sqlite_int64 val = sqlite3_value_int64( columnMem(pStmt,i) );
  columnMallocFailure(pStmt);
  return val;
}
const unsigned char *sqlite3_column_text(sqlite3_stmt *pStmt, int i){
  const unsigned char *val = sqlite3_value_text( columnMem(pStmt,i) );
  columnMallocFailure(pStmt);
  return val;
}
sqlite3_value *sqlite3_column_value(sqlite3_stmt *pStmt, int i){
  Mem *pOut = columnMem(pStmt, i);
  if( pOut->flags&MEM_Static ){
    pOut->flags &= ~MEM_Static;
    pOut->flags |= MEM_Ephem;
  }
  columnMallocFailure(pStmt);
  return (sqlite3_value *)pOut;
}
#ifndef SQLITE_OMIT_UTF16
const void *sqlite3_column_text16(sqlite3_stmt *pStmt, int i){
  const void *val = sqlite3_value_text16( columnMem(pStmt,i) );
  columnMallocFailure(pStmt);
  return val;
}
#endif /* SQLITE_OMIT_UTF16 */
int sqlite3_column_type(sqlite3_stmt *pStmt, int i){
  int iType = sqlite3_value_type( columnMem(pStmt,i) );
  columnMallocFailure(pStmt);
  return iType;
}

/* The following function is experimental and subject to change or
** removal 
下列函数是实验性的，并能改变或移除*/
/*int sqlite3_column_numeric_type(sqlite3_stmt *pStmt, int i){
**  return sqlite3_value_numeric_type( columnMem(pStmt,i) );
**}
*/

/*
** Convert the N-th element of pStmt->pColName[] into a string using
** xFunc() then return that string.  If N is out of range, return 0.
**使用xFunc()转换pStmt->pColName[]的N-th元素到一个字符串，然后返回这个字符串。
如果N超出范围，返回0。
** There are up to 5 names for each column.  useType determines which
** name is returned.  Here are the names:
这里有五个列名。useType决定哪个名字被返回。这就是这些名字：
**
**    0      The column name as it should be displayed for output为输出显示的列名称
**    1      The datatype name for the column该列的数据类型的名称
**    2      The name of the database that the column derives from列从中派生的数据库的名称
**    3      The name of the table that the column derives from列从中派生的表的名称
**    4      The name of the table column that the result column derives from表列的名称来源于结果列
**
** If the result is not a simple column reference (if it is an expression
** or a constant) then useTypes 2, 3, and 4 return NULL.
如果结果不是一个简单的列引用，（如果是一个语句或常量）那么useTypes类型的2、3和4返回NULL。
*/
static const void *columnName(
  sqlite3_stmt *pStmt,
  int N,
  const void *(*xFunc)(Mem*),
  int useType
){
  const void *ret = 0;
  Vdbe *p = (Vdbe *)pStmt;
  int n;
  sqlite3 *db = p->db;
  
  assert( db!=0 );
  n = sqlite3_column_count(pStmt);
  if( N<n && N>=0 ){
    N += useType*n;
    sqlite3_mutex_enter(db->mutex);
    assert( db->mallocFailed==0 );
    ret = xFunc(&p->aColName[N]);
     /* A malloc may have failed inside of the xFunc() call. If this
    ** is the case, clear the mallocFailed flag and return NULL.
	一个malloc可能失败而不调用xFunc()。如果这是一个事件，清除mallocFailed标志并返回NULL。
    */
    if( db->mallocFailed ){
      db->mallocFailed = 0;
      ret = 0;
    }
    sqlite3_mutex_leave(db->mutex);
  }
  return ret;
}

/*
** Return the name of the Nth column of the result set returned by SQL
** statement pStmt.
返回SQL语句pStmt的结果的Nth列的名字*/
const char *sqlite3_column_name(sqlite3_stmt *pStmt, int N){
  return columnName(
      pStmt, N, (const void*(*)(Mem*))sqlite3_value_text, COLNAME_NAME);
}
#ifndef SQLITE_OMIT_UTF16
const void *sqlite3_column_name16(sqlite3_stmt *pStmt, int N){
  return columnName(
      pStmt, N, (const void*(*)(Mem*))sqlite3_value_text16, COLNAME_NAME);
}
#endif

/*
** Constraint:  If you have ENABLE_COLUMN_METADATA then you must
** not define OMIT_DECLTYPE.
约束：如果有ENABLE_COLUMN_METADATA那么禁止定义OMIT_DECLTYPE
*/
#if defined(SQLITE_OMIT_DECLTYPE) && defined(SQLITE_ENABLE_COLUMN_METADATA)
# error "Must not define both SQLITE_OMIT_DECLTYPE \
         and SQLITE_ENABLE_COLUMN_METADATA"
#endif

#ifndef SQLITE_OMIT_DECLTYPE
/*
** Return the column declaration type (if applicable) of the 'i'th column
** of the result set of SQL statement pStmt.
返回SQL语句pStmt的结果的第i列的列声明类型（如果是可用的）
*/
const char *sqlite3_column_decltype(sqlite3_stmt *pStmt, int N){
  return columnName(
      pStmt, N, (const void*(*)(Mem*))sqlite3_value_text, COLNAME_DECLTYPE);
}
#ifndef SQLITE_OMIT_UTF16
const void *sqlite3_column_decltype16(sqlite3_stmt *pStmt, int N){
  return columnName(
      pStmt, N, (const void*(*)(Mem*))sqlite3_value_text16, COLNAME_DECLTYPE);
}
#endif /* SQLITE_OMIT_UTF16 */
#endif /* SQLITE_OMIT_DECLTYPE */

#ifdef SQLITE_ENABLE_COLUMN_METADATA
/*
** Return the name of the database from which a result column derives.
** NULL is returned if the result column is an expression or constant or
** anything else which is not an unabiguous reference to a database column.
\\返回结果列派生的数据库的表名。
如果结果列是一个语句、常量或任何其他一个数据库列中不明确的引用的东西，则NULL被返回。
*/
const char *sqlite3_column_database_name(sqlite3_stmt *pStmt, int N){
  return columnName(
      pStmt, N, (const void*(*)(Mem*))sqlite3_value_text, COLNAME_DATABASE);
}
#ifndef SQLITE_OMIT_UTF16
const void *sqlite3_column_database_name16(sqlite3_stmt *pStmt, int N){
  return columnName(
      pStmt, N, (const void*(*)(Mem*))sqlite3_value_text16, COLNAME_DATABASE);
}
#endif /* SQLITE_OMIT_UTF16 */

/*
** Return the name of the table from which a result column derives.
** NULL is returned if the result column is an expression or constant or
** anything else which is not an unabiguous reference to a database column.
返回结果列派生的表的名字。
如果结果列是一个语句、常量或任何其他一个数据库列中不明确的引用的东西，则NULL被返回。
*/
const char *sqlite3_column_table_name(sqlite3_stmt *pStmt, int N){
  return columnName(
      pStmt, N, (const void*(*)(Mem*))sqlite3_value_text, COLNAME_TABLE);
}
#ifndef SQLITE_OMIT_UTF16
const void *sqlite3_column_table_name16(sqlite3_stmt *pStmt, int N){
  return columnName(
      pStmt, N, (const void*(*)(Mem*))sqlite3_value_text16, COLNAME_TABLE);
}
#endif /* SQLITE_OMIT_UTF16 */

/*
** Return the name of the table column from which a result column derives.
** NULL is returned if the result column is an expression or constant or
** anything else which is not an unabiguous reference to a database column.
返回结果列派生的表的列的名字
如果结果列是一个语句、常量或任何其他一个数据库列中不明确的引用的东西，则NULL被返回。
*/
const char *sqlite3_column_origin_name(sqlite3_stmt *pStmt, int N){
  return columnName(
      pStmt, N, (const void*(*)(Mem*))sqlite3_value_text, COLNAME_COLUMN);
}
#ifndef SQLITE_OMIT_UTF16
const void *sqlite3_column_origin_name16(sqlite3_stmt *pStmt, int N){
  return columnName(
      pStmt, N, (const void*(*)(Mem*))sqlite3_value_text16, COLNAME_COLUMN);
}
#endif /* SQLITE_OMIT_UTF16 */
#endif /* SQLITE_ENABLE_COLUMN_METADATA */


/******************************* sqlite3_bind_  ***************************
** 
** Routines used to attach values to wildcards in a compiled SQL statement.
这些程序被用来在编译SQL语句中为通配符附加值
*/
/*
** Unbind the value bound to variable i in virtual machine p. This is the 
** the same as binding a NULL value to the column. If the "i" parameter is
** out of range, then SQLITE_RANGE is returned. Othewise SQLITE_OK.
\\在虚拟机p中取消绑定变量i值得界限。这是一个类似于在数据库的某列上绑定一个NULL值。如果“i”参数是超出范围的，
则SQLITE_RANGE被返回。否则返回SQLITE_OK
**
** A successful evaluation of this routine acquires the mutex on p.
** the mutex is released if any kind of error occurs.
\\一个成功的程序评估需要获取虚拟机p的互斥变量（锁），如果有任何一种错误发生，这个互斥变量（锁）就会被释放。
**
** The error code stored in database p->db is overwritten with the return
** value in any case.
\\在任何情况下，储存在数据库p->db的错误代码被任何情况下的返回值所覆写
*/
static int vdbeUnbind(Vdbe *p, int i){
  Mem *pVar;
  if( vdbeSafetyNotNull(p) ){
    return SQLITE_MISUSE_BKPT;
  }
  sqlite3_mutex_enter(p->db->mutex);
  if( p->magic!=VDBE_MAGIC_RUN || p->pc>=0 ){
    sqlite3Error(p->db, SQLITE_MISUSE, 0);
    sqlite3_mutex_leave(p->db->mutex);
    sqlite3_log(SQLITE_MISUSE, 
        "bind on a busy prepared statement: [%s]", p->zSql);
    return SQLITE_MISUSE_BKPT;
  }
  if( i<1 || i>p->nVar ){
    sqlite3Error(p->db, SQLITE_RANGE, 0);
    sqlite3_mutex_leave(p->db->mutex);
    return SQLITE_RANGE;
  }
  i--;
  pVar = &p->aVar[i];
  sqlite3VdbeMemRelease(pVar);
  pVar->flags = MEM_Null;
  sqlite3Error(p->db, SQLITE_OK, 0);

  /* If the bit corresponding to this variable in Vdbe.expmask is set, then 
  ** binding a new value to this variable invalidates the current query plan.
  如果对应于该变量在Vdbe.expmask该位被设置，则将一个新值绑定到该变量，当前的查询计划将会失效。
  **
  ** IMPLEMENTATION-OF: R-48440-37595 If the specific value bound to host
  ** parameter in the WHERE clause might influence the choice of query plan
  ** for a statement, then the statement will be automatically recompiled,
  ** as if there had been a schema change, on the first sqlite3_step() call
  ** following any change to the bindings of that parameter.
  IMPLEMENTATION-OF: R-48440-37595 如果在WHERE语句中特定值绑定到主机参数可能影响到查询计划的选择，
  那么这个语句将自动的编译，犹如有一个计划改变，在第一个sqlite3_step()中调用下面任何该参数绑定的变化。
  */
  if( p->isPrepareV2 &&
     ((i<32 && p->expmask & ((u32)1 << i)) || p->expmask==0xffffffff)
  ){
    p->expired = 1;
  }
  return SQLITE_OK;
}

/*
** Bind a text or BLOB value.
绑定一个文本或BLOB值
*/
static int bindText(
  sqlite3_stmt *pStmt,   /* The statement to bind against 绑定反对声明*/
  int i,                 /* Index of the parameter to bind 参数的索引绑定*/
  const void *zData,     /* Pointer to the data to be bound 数据指针被绑定*/
  int nData,             /* Number of bytes of data to be bound 受约束的数据的字节数*/
  void (*xDel)(void*),   /* Destructor for the data 析构数据*/
  u8 encoding            /* Encoding for the data 数据编码*/
){
  Vdbe *p = (Vdbe *)pStmt;
  Mem *pVar;
  int rc;

  rc = vdbeUnbind(p, i); \\解绑vdbe
  if( rc==SQLITE_OK ){
    if( zData!=0 ){
      pVar = &p->aVar[i-1];  \\操作码的值 这里就是绑定值，而释放的时候也是释放操作码就行
      rc = sqlite3VdbeMemSetStr(pVar, zData, nData, encoding, xDel); \\把Mem结构中的数据转换成string或者BLOB类型
      if( rc==SQLITE_OK && encoding!=0 ){
        rc = sqlite3VdbeChangeEncoding(pVar, ENC(p->db));   \\改变编码方式
      }
      sqlite3Error(p->db, rc, 0);
      rc = sqlite3ApiExit(p->db, rc);    \\退出API调用的任何函数
    }
    sqlite3_mutex_leave(p->db->mutex);
  }else if( xDel!=SQLITE_STATIC && xDel!=SQLITE_TRANSIENT ){
    xDel((void*)zData);  \\释放zData
  }
  return rc;
}


/*
** Bind a blob value to an SQL statement variable.
绑定一个blob值到一个SQL语句的变量
*/
int sqlite3_bind_blob(
  sqlite3_stmt *pStmt, 
  int i, 
  const void *zData, 
  int nData, 
  void (*xDel)(void*)
){
  return bindText(pStmt, i, zData, nData, xDel, 0);
}
int sqlite3_bind_double(sqlite3_stmt *pStmt, int i, double rValue){
  int rc;
  Vdbe *p = (Vdbe *)pStmt;
  rc = vdbeUnbind(p, i);
  if( rc==SQLITE_OK ){
    sqlite3VdbeMemSetDouble(&p->aVar[i-1], rValue);
    sqlite3_mutex_leave(p->db->mutex);
  }
  return rc;
}
int sqlite3_bind_int(sqlite3_stmt *p, int i, int iValue){
  return sqlite3_bind_int64(p, i, (i64)iValue);
}
int sqlite3_bind_int64(sqlite3_stmt *pStmt, int i, sqlite_int64 iValue){
  int rc;
  Vdbe *p = (Vdbe *)pStmt;
  rc = vdbeUnbind(p, i);
  if( rc==SQLITE_OK ){
    sqlite3VdbeMemSetInt64(&p->aVar[i-1], iValue);
    sqlite3_mutex_leave(p->db->mutex);
  }
  return rc;
}
int sqlite3_bind_null(sqlite3_stmt *pStmt, int i){
  int rc;
  Vdbe *p = (Vdbe*)pStmt;
  rc = vdbeUnbind(p, i);
  if( rc==SQLITE_OK ){
    sqlite3_mutex_leave(p->db->mutex);
  }
  return rc;
}
int sqlite3_bind_text( 
  sqlite3_stmt *pStmt, 
  int i, 
  const char *zData, 
  int nData, 
  void (*xDel)(void*)
){
  return bindText(pStmt, i, zData, nData, xDel, SQLITE_UTF8);
}
#ifndef SQLITE_OMIT_UTF16
int sqlite3_bind_text16(
  sqlite3_stmt *pStmt, 
  int i, 
  const void *zData, 
  int nData, 
  void (*xDel)(void*)
){
  return bindText(pStmt, i, zData, nData, xDel, SQLITE_UTF16NATIVE);
}
#endif /* SQLITE_OMIT_UTF16 */
int sqlite3_bind_value(sqlite3_stmt *pStmt, int i, const sqlite3_value *pValue){
  int rc;
  switch( pValue->type ){
    case SQLITE_INTEGER: {
      rc = sqlite3_bind_int64(pStmt, i, pValue->u.i);
      break;
    }
    case SQLITE_FLOAT: {
      rc = sqlite3_bind_double(pStmt, i, pValue->r);
      break;
    }
    case SQLITE_BLOB: {
      if( pValue->flags & MEM_Zero ){
        rc = sqlite3_bind_zeroblob(pStmt, i, pValue->u.nZero);
      }else{
        rc = sqlite3_bind_blob(pStmt, i, pValue->z, pValue->n,SQLITE_TRANSIENT);
      }
      break;
    }
    case SQLITE_TEXT: {
      rc = bindText(pStmt,i,  pValue->z, pValue->n, SQLITE_TRANSIENT,
                              pValue->enc);
      break;
    }
    default: {
      rc = sqlite3_bind_null(pStmt, i);
      break;
    }
  }
  return rc;
}
int sqlite3_bind_zeroblob(sqlite3_stmt *pStmt, int i, int n){
  int rc;
  Vdbe *p = (Vdbe *)pStmt;
  rc = vdbeUnbind(p, i);
  if( rc==SQLITE_OK ){
    sqlite3VdbeMemSetZeroBlob(&p->aVar[i-1], n);
    sqlite3_mutex_leave(p->db->mutex);
  }
  return rc;
}

/*
** Return the number of wildcards that can be potentially bound to.
返回返回的通配符可绑定的数量。
** This routine is added to support DBD::SQLite.  
这个程序被添加到支持DBD::SQLite
*/
int sqlite3_bind_parameter_count(sqlite3_stmt *pStmt){
  Vdbe *p = (Vdbe*)pStmt;
  return p ? p->nVar : 0;
}

/*
** Return the name of a wildcard parameter.  Return NULL if the index
返回通配符参数的名字，如果索引不在范围之内或者通配符没有命名，返回空。
** is out of range or if the wildcard is unnamed.
**
** The result is always UTF-8.结果通常是UTF-8
*/
const char *sqlite3_bind_parameter_name(sqlite3_stmt *pStmt, int i){
  Vdbe *p = (Vdbe*)pStmt;
  if( p==0 || i<1 || i>p->nzVar ){
    return 0;
  }
  return p->azVar[i-1];
}

/*
** Given a wildcard parameter name, return the index of the variable
** with that name.  If there is no variable with the given name,
** return 0.
给定一个通配符参数名称，返回该具有该名称的变量索引。如果没有给定名称的变量，返回0。
*/
int sqlite3VdbeParameterIndex(Vdbe *p, const char *zName, int nName){
  int i;
  if( p==0 ){
    return 0;
  }
  if( zName ){
    for(i=0; i<p->nzVar; i++){
      const char *z = p->azVar[i];
      if( z && memcmp(z,zName,nName)==0 && z[nName]==0 ){
        return i+1;
      }
    }
  }
  return 0;
}
int sqlite3_bind_parameter_index(sqlite3_stmt *pStmt, const char *zName){
  return sqlite3VdbeParameterIndex((Vdbe*)pStmt, zName, sqlite3Strlen30(zName));
}

/*
** Transfer all bindings from the first statement over to the second.
\\将第一条语句的绑定转移到绑定第二条语句
*/
int sqlite3TransferBindings(sqlite3_stmt *pFromStmt, sqlite3_stmt *pToStmt){
  Vdbe *pFrom = (Vdbe*)pFromStmt;
  Vdbe *pTo = (Vdbe*)pToStmt;
  int i;
  assert( pTo->db==pFrom->db );
  assert( pTo->nVar==pFrom->nVar );
  sqlite3_mutex_enter(pTo->db->mutex);
  for(i=0; i<pFrom->nVar; i++){
    sqlite3VdbeMemMove(&pTo->aVar[i], &pFrom->aVar[i]);
  }
  sqlite3_mutex_leave(pTo->db->mutex);
  return SQLITE_OK;
}

#ifndef SQLITE_OMIT_DEPRECATED
/*
** Deprecated external interface.  Internal/core SQLite code
** should call sqlite3TransferBindings.
\\不赞成使用的外部接口。内核/核心SQLite代码应该调用sqlite3TransferBindings。
**
** Is is misuse to call this routine with statements from different
** database connections.  But as this is a deprecated interface, we
** will not bother to check for that condition.
使用来自不同数据库连接的声明调用这个例程是误用。但是，因为这是一个弃用的接口，将不会检查那个条件。
**
** If the two statements contain a different number of bindings, then
** an SQLITE_ERROR is returned.  Nothing else can go wrong, so otherwise
** SQLITE_OK is returned.
如果两个声明包含不同数目的绑定，则返回SQLITE。不出错的，返回SQLITE_OK。
*/
int sqlite3_transfer_bindings(sqlite3_stmt *pFromStmt, sqlite3_stmt *pToStmt){
  Vdbe *pFrom = (Vdbe*)pFromStmt;
  Vdbe *pTo = (Vdbe*)pToStmt;
  if( pFrom->nVar!=pTo->nVar ){
    return SQLITE_ERROR;
  }
  if( pTo->isPrepareV2 && pTo->expmask ){
    pTo->expired = 1;
  }
  if( pFrom->isPrepareV2 && pFrom->expmask ){
    pFrom->expired = 1;
  }
  return sqlite3TransferBindings(pFromStmt, pToStmt);
}
#endif

/*
** Return the sqlite3* database handle to which the prepared statement given
** in the argument belongs.  This is the same database handle that was
** the first argument to the sqlite3_prepare() that was used to create
** the statement in the first place.
返回对在参数声明中给出的预处理声明的sqlite3* 的数据库处理。这是同样的数据库处理，
该数据库处理是sqlite3_prepare()的第一个参数。其中sqlite3_prepare()被用于在首位置建立声明。
*/
sqlite3 *sqlite3_db_handle(sqlite3_stmt *pStmt){
  return pStmt ? ((Vdbe*)pStmt)->db : 0;
}

/*
** Return true if the prepared statement is guaranteed to not modify the
** database.如果预声明没有修改数据库，返回真。
*/
int sqlite3_stmt_readonly(sqlite3_stmt *pStmt){
  return pStmt ? ((Vdbe*)pStmt)->readOnly : 1;
}

/*
** Return true if the prepared statement is in need of being reset.\\如果预编译语句需要重置，返回真。
*/
int sqlite3_stmt_busy(sqlite3_stmt *pStmt){
  Vdbe *v = (Vdbe*)pStmt;
  return v!=0 && v->pc>0 && v->magic==VDBE_MAGIC_RUN;
}

/*
** Return a pointer to the next prepared statement after pStmt associated
** with database connection pDb.  If pStmt is NULL, return the first
** prepared statement for the database connection.  Return NULL if there
** are no more.
pStmt与数据库连接pDb相关联后，返回指向下一个预声明的指针。如果 pStmt 为空，
返回用于数据库连接的第一个预声明。若无更多，返回空。
*/
sqlite3_stmt *sqlite3_next_stmt(sqlite3 *pDb, sqlite3_stmt *pStmt){
  sqlite3_stmt *pNext;
  sqlite3_mutex_enter(pDb->mutex);
  if( pStmt==0 ){
    pNext = (sqlite3_stmt*)pDb->pVdbe;
  }else{
    pNext = (sqlite3_stmt*)((Vdbe*)pStmt)->pNext;
  }
  sqlite3_mutex_leave(pDb->mutex);
  return pNext;
}

/*
** Return the value of a status counter for a prepared statement
返回用于对预声明的状态计数的值。
*/
int sqlite3_stmt_status(sqlite3_stmt *pStmt, int op, int resetFlag){
  Vdbe *pVdbe = (Vdbe*)pStmt;
  int v = pVdbe->aCounter[op-1];
  if( resetFlag ) pVdbe->aCounter[op-1] = 0;
  return v;
}
