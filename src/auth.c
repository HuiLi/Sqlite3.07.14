/*
** 2003 January 11
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file contains code used to implement the sqlite3_set_authorizer()
** API.  This facility is an optional feature of the library.  Embedded
** systems that do not need this facility may omit it by recompiling
** the library with -DSQLITE_OMIT_AUTHORIZATION=1
这个文件包含用于实现sqlite3_set_authorizer()API的代码。
此工具是一个可选的特征库。
不需要这个工具的嵌入式系统可能忽略它通过重新编译这个库-DSQLITE_OMIT_AUTHORIZATION = 1
*/
#include "sqliteInt.h"//头文件

/*
** All of the code in this file may be omitted by defining a single
** macro.
所有的代码在这个文件中可以通过定义一个宏来省略。
*/
#ifndef SQLITE_OMIT_AUTHORIZATION

/*
** Set or clear the access authorization function.
**
** The access authorization function is be called during the compilation
** phase to verify that the user has read and/or write access permission on
** various fields of the database.  The first argument to the auth function
** is a copy of the 3rd argument to this routine.  The second argument
** to the auth function is one of these constants:
**
**       SQLITE_CREATE_INDEX
**       SQLITE_CREATE_TABLE
**       SQLITE_CREATE_TEMP_INDEX
**       SQLITE_CREATE_TEMP_TABLE
**       SQLITE_CREATE_TEMP_TRIGGER
**       SQLITE_CREATE_TEMP_VIEW
**       SQLITE_CREATE_TRIGGER
**       SQLITE_CREATE_VIEW
**       SQLITE_DELETE
**       SQLITE_DROP_INDEX
**       SQLITE_DROP_TABLE
**       SQLITE_DROP_TEMP_INDEX
**       SQLITE_DROP_TEMP_TABLE
**       SQLITE_DROP_TEMP_TRIGGER
**       SQLITE_DROP_TEMP_VIEW
**       SQLITE_DROP_TRIGGER
**       SQLITE_DROP_VIEW
**       SQLITE_INSERT
**       SQLITE_PRAGMA
**       SQLITE_READ
**       SQLITE_SELECT
**       SQLITE_TRANSACTION
**       SQLITE_UPDATE
**
** The third and fourth arguments to the auth function are the name of
** the table and the column that are being accessed.  The auth function
** should return either SQLITE_OK, SQLITE_DENY, or SQLITE_IGNORE.  If
** SQLITE_OK is returned, it means that access is allowed.  SQLITE_DENY
** means that the SQL statement will never-run - the sqlite3_exec() call
** will return with an error.  SQLITE_IGNORE means that the SQL statement
** should run but attempts to read the specified column will return NULL
** and attempts to write the column will be ignored.
**
** Setting the auth function to NULL disables this hook.  The default
** setting of the auth function is NULL.
设置或清除访问授权功能。
访问授权函数在编译阶段被称为验证在数据库的各个领域里用户读和/或写访问权限。
授权函数的第一个参数是这个例程的第三参数副本。
授权函数的第二个参数是其中的一个常量:
* * SQLITE_CREATE_INDEX
* * SQLITE_CREATE_TABLE
* * SQLITE_CREATE_TEMP_INDEX
* * SQLITE_CREATE_TEMP_TABLE
* * SQLITE_CREATE_TEMP_TRIGGER
* * SQLITE_CREATE_TEMP_VIEW
* * SQLITE_CREATE_TRIGGER
* * SQLITE_CREATE_VIEW
* * SQLITE_DELETE
* * SQLITE_DROP_INDEX
* * SQLITE_DROP_TABLE
* * SQLITE_DROP_TEMP_INDEX
* * SQLITE_DROP_TEMP_TABLE
* * SQLITE_DROP_TEMP_TRIGGER
* * SQLITE_DROP_TEMP_VIEW
* * SQLITE_DROP_TRIGGER
* * SQLITE_DROP_VIEW
* * SQLITE_INSERT
* * SQLITE_PRAGMA
* * SQLITE_READ
* * SQLITE_SELECT
* * SQLITE_TRANSACTION
* * SQLITE_UPDATE
授权函数的第三和第四参数是正在被访问的表和列的名称。
授权函数应该返回SQLITE_OK,SQLITE_DENY或SQLITE_IGNORE。
如果SQLITE_OK返回,这意味着允许访问。
SQLITE_DENY意味着SQL语句将不会运行——sqlite3_exec()调用将返回一个错误。
SQLITE_IGNORE意味着应该运行SQL语句,但试图阅读指定的列将返回NULL,尝试写列将被忽略。
授权函数设置为空时禁用这个钩子机制。授权函数的默认设置为NULL。
*/
int sqlite3_set_authorizer(
  sqlite3 *db,
  int (*xAuth)(void*,int,const char*,const char*,const char*,const char*),
  void *pArg
){
  sqlite3_mutex_enter(db->mutex);//将db作为互斥体连接
  db->xAuth = xAuth; //访问授权函数
  db->pAuthArg = pArg;//设置授权函数的第一个参数
  sqlite3ExpirePreparedStatements(db);
  sqlite3_mutex_leave(db->mutex);
  return SQLITE_OK;
}

/*
** Write an error message into pParse->zErrMsg that explains that the
** user-supplied authorization function returned an illegal value.
写一个错误的信息为pParse -> zerrmsg说明用户提供授权函数返回了一个非法的值。
*/
static void sqliteAuthBadReturnCode(Parse *pParse){
  sqlite3ErrorMsg(pParse, "authorizer malfunction");
  pParse->rc = SQLITE_ERROR;
}

/*
** Invoke the authorization callback for permission to read column zCol from
** table zTab in database zDb. This function assumes that an authorization
** callback has been registered (i.e. that sqlite3.xAuth is not NULL).
**
** If SQLITE_IGNORE is returned and pExpr is not NULL, then pExpr is changed
** to an SQL NULL expression. Otherwise, if pExpr is NULL, then SQLITE_IGNORE
** is treated as SQLITE_DENY. In this case an error is left in pParse.
调用授权回调允许数据库zDb中从表zTab中读取列zCol。
这个函数假设授权回调注册(即,sqlite3.xAuth 不为 NULL)。
如果返回SQLITE_IGNORE和pExpr指针不为NULL,那么pExpr改为一个SQL空表达式。
否则,如果pExpr为空,则SQLITE_IGNORE视为SQLITE_DENY 。
在这种情况下在pParse中一个错误被丢弃。
*/
int sqlite3AuthReadCol(
  Parse *pParse,                  /* The parser context 语法解析上下文*/
  const char *zTab,               /* Table name 表名*/
  const char *zCol,               /* Column name列名 */
  int iDb                         /* Index of containing database. 包含数据库的索引。*/
){
  sqlite3 *db = pParse->db;       /* Database handle 数据库句柄*/
  char *zDb = db->aDb[iDb].zName; /* Name of attached database 连接数据库的名称*/
  int rc;                         /* Auth callback return code 授权回调返回的代码*/

  rc = db->xAuth(db->pAuthArg, SQLITE_READ, zTab,zCol,zDb,pParse->zAuthContext);
  if( rc==SQLITE_DENY ){
    if( db->nDb>2 || iDb!=0 ){
      sqlite3ErrorMsg(pParse, "access to %s.%s.%s is prohibited",zDb,zTab,zCol);
    }else{
      sqlite3ErrorMsg(pParse, "access to %s.%s is prohibited", zTab, zCol);
    }
    pParse->rc = SQLITE_AUTH;
  }else if( rc!=SQLITE_IGNORE && rc!=SQLITE_OK ){
    sqliteAuthBadReturnCode(pParse);
  }
  return rc;// 授权回调返回的代码
}

/*
** The pExpr should be a TK_COLUMN expression.  The table referred to
** is in pTabList or else it is the NEW or OLD table of a trigger.  
** Check to see if it is OK to read this pa rticular column.
**
** If the auth function returns SQLITE_IGNORE, change the TK_COLUMN 
** instruction into a TK_NULL.  If the auth function returns SQLITE_DENY,
** then generate an error.
pExpr应该是一个TK_COLUMN表达式。
这个表所指的是pTabList否则它是在新或旧的触发器的表
检查是否可以读取这个特定列。
如果授权函数返回SQLITE_IGNORE,将TK_COLUMN指令改变成TK_NULL。
如果授权函数返回SQLITE_DENY,然后生成一个错误。
*/
void sqlite3AuthRead(
  Parse *pParse,        /* The parser context 语法解析上下文*/
  Expr *pExpr,          /* The expression to check authorization on 检查授权表达式*/
  Schema *pSchema,      /* The schema of the expression表达式的模式 */
  SrcList *pTabList     /* All table that pExpr might refer to pExpr可能引用的所有表*/
){
  sqlite3 *db = pParse->db;
  Table *pTab = 0;      /* The table being read 表正在被读*/
  const char *zCol;     /* Name of the column of the table 表的列的名称*/
  int iSrc;             /* Index in pTabList->a[] of table being read 表索引pTabList->a[] 正在被读*/
  int iDb;              /* The index of the database the expression refers to 表达式是指数据库的索引*/
  int iCol;             /* Index of column in table 在表中列的索引*/

  if( db->xAuth==0 ) return;
  iDb = sqlite3SchemaToIndex(pParse->db, pSchema);
  if( iDb<0 ){
    /* An attempt to read a column out of a subquery or other
    ** temporary table.试图读取一个子查询或其他临时表的列。 */
    return;
  }

  assert( pExpr->op==TK_COLUMN || pExpr->op==TK_TRIGGER );
  if( pExpr->op==TK_TRIGGER ){
    pTab = pParse->pTriggerTab;
  }else{
    assert( pTabList );
    for(iSrc=0; ALWAYS(iSrc<pTabList->nSrc); iSrc++){
      if( pExpr->iTable==pTabList->a[iSrc].iCursor ){
        pTab = pTabList->a[iSrc].pTab;
        break;
      }
    }
  }
  iCol = pExpr->iColumn;
  if( NEVER(pTab==0) ) return;

  if( iCol>=0 ){
    assert( iCol<pTab->nCol );
    zCol = pTab->aCol[iCol].zName;
  }else if( pTab->iPKey>=0 ){
    assert( pTab->iPKey<pTab->nCol );
    zCol = pTab->aCol[pTab->iPKey].zName;
  }else{
    zCol = "ROWID";
  }
  assert( iDb>=0 && iDb<db->nDb );
  if( SQLITE_IGNORE==sqlite3AuthReadCol(pParse, pTab->zName, zCol, iDb) ){
    pExpr->op = TK_NULL;
  }
}

/*
** Do an authorization check using the code and arguments given.  Return
** either SQLITE_OK (zero) or SQLITE_IGNORE or SQLITE_DENY.  If SQLITE_DENY
** is returned, then the error count and error message in pParse are
** modified appropriately.
做一个使用给定的代码和参数的授权检查。
返回要么是SQLITE_OK(零)要么是SQLITE_IGNORE 或SQLITE_DENY。
如果SQLITE_DENY返回,那么错误数和错误pParse里适当修改的消息。
*/
int sqlite3AuthCheck(
  Parse *pParse,
  int code,
  const char *zArg1,
  const char *zArg2,
  const char *zArg3
){
  sqlite3 *db = pParse->db;
  int rc;

  /* Don't do any authorization checks if the database is initialising
  ** or if the parser is being invoked from within sqlite3_declare_vtab.
  不做任何授权检查
  如果数据库初始化或解析器从sqlite3_declare_vtab内部调用。
  */
  if( db->init.busy || IN_DECLARE_VTAB ){
    return SQLITE_OK;
  }

  if( db->xAuth==0 ){
    return SQLITE_OK;
  }
  rc = db->xAuth(db->pAuthArg, code, zArg1, zArg2, zArg3, pParse->zAuthContext);
  if( rc==SQLITE_DENY ){
    sqlite3ErrorMsg(pParse, "not authorized");//错误类型将不被授权
    pParse->rc = SQLITE_AUTH;
  }else if( rc!=SQLITE_OK && rc!=SQLITE_IGNORE ){
    rc = SQLITE_DENY;
    sqliteAuthBadReturnCode(pParse);//调用sqliteAuthBadReturnCode(pParse);函数，返回一个非法值
  }
  return rc;
}

/*
** Push an authorization context.  After this routine is called, the
** zArg3 argument to authorization callbacks will be zContext until
** popped.  Or if pParse==0, this routine is a no-op.
入栈授权上下文。调用这个例程后,授权回调的zArg3参数将是zContext直到突然破裂。
或者如果pParse = = 0,这个例程是一个空操作。
*/
void  sqlite3AuthContextPush(
  Parse *pParse,
  AuthContext *pContext, 
  const char *zContext
){
  assert( pParse );
  pContext->pParse = pParse;
  pContext->zAuthContext = pParse->zAuthContext;
  pParse->zAuthContext = zContext;
}

/*
** Pop an authorization context that was previously pushed
** by sqlite3AuthContextPush
出栈解析授权上下文，
这个上下文被sqlite3AuthContextPush预先推出
*/
void sqlite3AuthContextPop(AuthContext *pContext){
  if( pContext->pParse ){
    pContext->pParse->zAuthContext = pContext->zAuthContext;
    pContext->pParse = 0;
  }
}

#endif /* SQLITE_OMIT_AUTHORIZATION */

