/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file contains the sqlite3_get_table() and sqlite3_free_table()
** interface routines.  These are just wrappers around the main
** interface routine of sqlite3_exec().
** 该文件包含了sqlite3_get_table()和sqlite3_free_table()常规接口。它被主接
** 口中sqlite3_exec()调用。
** These routines are in a separate files so that they will not be linked
** if they are not used.
** 这些常规接口分布在不同的文件中，以利于但它们没被使用不会连接。
*/
#include "sqliteInt.h"
#include <stdlib.h>
#include <string.h>

#ifndef SQLITE_OMIT_GET_TABLE  //是否省略SQLite_get_table()接口

/*
** This structure is used to pass data from sqlite3_get_table() through
** to the callback function is uses to build the result.
** 这个结构是用来将sqlite3_get_table()的数据传递调用它的函数来得结果。
*/
typedef struct TabResult {
  char **azResult;   /* Accumulated output 结果输出*/
  char *zErrMsg;     /* Error message text, if an error occurs 错误信息*/
  int nAlloc;        /* Slots allocated for azResult[] 为azResult[]分配空间*/
  int nRow;          /* Number of rows in the result 结果行数*/
  int nColumn;       /* Number of columns in the result 结果列数*/
  int nData;         /* Slots used in azResult[].  (nRow+1)*nColumn 建立表格*/
  int rc;            /* Return code from sqlite3_exec() 
  返回sqlite3_exec()执行结果,是否成功执行*/
} TabResult;

/*
** This routine is called once for each row in the result table.  Its job
** is to fill in the TabResult structure appropriately, allocating new
** memory as necessary.
** 这个函数用来填充结果表，必要时分配存储空间
*/
static int sqlite3_get_table_cb(void *pArg, int nCol, char **argv, char **colv){
  TabResult *p = (TabResult*)pArg;  /* Result accumulator */
  int need;                         /* Slots needed in p->azResult[] 分配表格空间*/
  int i;                            /* Loop counter 循环次数，用来定位表格每个单元*/
  char *z;                          /* A single column of result 结果列*/

  /* Make sure there is enough space in p->azResult to hold everything
  ** we need to remember from this invocation of the callback.
  ** 保证有足够空间来存储结果集。
  */
  if( p->nRow==0 && argv!=0 ){
    need = nCol*2;
  }else{
    need = nCol;
  }
  if( p->nData + need > p->nAlloc ){
    char **azNew;
    p->nAlloc = p->nAlloc*2 + need;
    azNew = sqlite3_realloc( p->azResult, sizeof(char*)*p->nAlloc ); //重分配空间
    if( azNew==0 ) goto malloc_failed;
    p->azResult = azNew;
  }

  /* If this is the first row, then generate an extra row containing
  ** the names of all columns.
  ** 如果是第一行，需要产生额外行来存储没列的列名
  */
  if( p->nRow==0 ){
    p->nColumn = nCol;
    for(i=0; i<nCol; i++){
      z = sqlite3_mprintf("%s", colv[i]);
      if( z==0 ) goto malloc_failed;
      p->azResult[p->nData++] = z;
    }
  }else if( p->nColumn!=nCol ){  //调用出错
    sqlite3_free(p->zErrMsg);
    p->zErrMsg = sqlite3_mprintf(
       "sqlite3_get_table() called with two or more incompatible queries"
    );
    p->rc = SQLITE_ERROR;
    return 1;
  }

  /* Copy over the row data
  ** 复制行数据
  */
  if( argv!=0 ){
    for(i=0; i<nCol; i++){
      if( argv[i]==0 ){
        z = 0;
      }else{
        int n = sqlite3Strlen30(argv[i])+1;
        z = sqlite3_malloc( n );
        if( z==0 ) goto malloc_failed;
        memcpy(z, argv[i], n);
      }
      p->azResult[p->nData++] = z;
    }
    p->nRow++;
  }
  return 0;

malloc_failed:
  p->rc = SQLITE_NOMEM;
  return 1;
}

/*
** Query the database.  But instead of invoking a callback for each row,
** malloc() for space to hold the result and return the entire results
** at the conclusion of the call.
** 数据库执行。分配空间来保存结果并在调用结束时返回整个结果，而不是对每行都进行调用
** The result that is written to ***pazResult is held in memory obtained
** from malloc().  But the caller cannot free this memory directly.  
** Instead, the entire table should be passed to sqlite3_free_table() when
** the calling procedure is finished using it.
** 分配空间给pazResult指针所指单元中，并用它来保存结果。
** 分配的空间不是直接释放的，而是在函数执行结束后调用sqlite3_free_table()函数来进行统一释放。
*/
int sqlite3_get_table(
  sqlite3 *db,                /* The database on which the SQL executes 执行查询的数据库*/
  const char *zSql,           /* The SQL to be executed 执行语句*/
  char ***pazResult,          /* Write the result table here 需要写入记录的表*/
  int *pnRow,                 /* Write the number of rows in the result here 结果集行数*/
  int *pnColumn,              /* Write the number of columns of result here 结果集列数*/
  char **pzErrMsg             /* Write error messages here 错误信息*/
){
  int rc;
  TabResult res;
  /*开始分配空间*/ 
  *pazResult = 0;
  if( pnColumn ) *pnColumn = 0;
  if( pnRow ) *pnRow = 0;
  if( pzErrMsg ) *pzErrMsg = 0;
  res.zErrMsg = 0;
  res.nRow = 0;
  res.nColumn = 0;
  res.nData = 1;
  res.nAlloc = 20;
  res.rc = SQLITE_OK;
  res.azResult = sqlite3_malloc(sizeof(char*)*res.nAlloc );
  if( res.azResult==0 ){
     db->errCode = SQLITE_NOMEM;
     return SQLITE_NOMEM;
  }
  res.azResult[0] = 0;
  rc = sqlite3_exec(db, zSql, sqlite3_get_table_cb, &res, pzErrMsg);
  assert( sizeof(res.azResult[0])>= sizeof(res.nData) );
  res.azResult[0] = SQLITE_INT_TO_PTR(res.nData);
  if( (rc&0xff)==SQLITE_ABORT ){
    sqlite3_free_table(&res.azResult[1]);  //释放表空间
    if( res.zErrMsg ){
      if( pzErrMsg ){
        sqlite3_free(*pzErrMsg);
        *pzErrMsg = sqlite3_mprintf("%s",res.zErrMsg);
      }
      sqlite3_free(res.zErrMsg);
    }
    db->errCode = res.rc;  /* Assume 32-bit assignment is atomic */
    return res.rc;
  }
  sqlite3_free(res.zErrMsg);
  if( rc!=SQLITE_OK ){
    sqlite3_free_table(&res.azResult[1]);
    return rc;
  }
  if( res.nAlloc>res.nData ){
    char **azNew;
    azNew = sqlite3_realloc( res.azResult, sizeof(char*)*res.nData );
    if( azNew==0 ){
      sqlite3_free_table(&res.azResult[1]);
      db->errCode = SQLITE_NOMEM;
      return SQLITE_NOMEM;
    }
    res.azResult = azNew;
  }
  *pazResult = &res.azResult[1];
  if( pnColumn ) *pnColumn = res.nColumn;
  if( pnRow ) *pnRow = res.nRow;
  return rc;
}

/*
** This routine frees the space the sqlite3_get_table() malloced.
** 用于释放sqlite3_get_table（）结果集所占用的空间
*/
void sqlite3_free_table(
  char **azResult            /* Result returned from from sqlite3_get_table() */
){
  if( azResult ){
    int i, n;
    azResult--;
    assert( azResult!=0 );
    n = SQLITE_PTR_TO_INT(azResult[0]);
    for(i=1; i<n; i++){ if( azResult[i] ) sqlite3_free(azResult[i]); }
    sqlite3_free(azResult);
  }
}

#endif /* SQLITE_OMIT_GET_TABLE */
