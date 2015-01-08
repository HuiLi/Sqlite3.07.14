/*
** 2003 April 6
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
* * 2003年4月6日
* *
* *作者放弃这个源代码的版权。不再是
* *一个合法的通知,而是一个祝福:
* *
* *祝你做好事,而不是邪恶的。
* *愿你可以原谅自己,原谅他人。
* *愿你自由地分享,永远不会超过你分享的。
* *

*************************************************************************
** This file contains code used to implement the VACUUM command.
**
** Most of the code in this file may be omitted by defining the
** SQLITE_OMIT_VACUUM macro.
* *这个文件包含代码用于实现VACCUM命令。
* *
* *在这个文件中大多数的代码通过定义的SQLITE_OMIT_VACUUM宏可以省略。

*/
#include "sqliteInt.h"
#include "vdbeInt.h"

#if !defined(SQLITE_OMIT_VACUUM) && !defined(SQLITE_OMIT_ATTACH)
/*
** Finalize a prepared statement.  If there was an error, store the
** text of the error message in *pzErrMsg.  Return the result code.
**完成一份事先准备好的声明中。如果有一个错误,在* pzErrMsg当中，存储
**错误消息的文本。返回结果代码。

*/
static int vacuumFinalize(sqlite3 *db, sqlite3_stmt *pStmt, char **pzErrMsg){
  int rc;
  rc = sqlite3VdbeFinalize((Vdbe*)pStmt);
  if( rc ){
    sqlite3SetString(pzErrMsg, db, sqlite3_errmsg(db));
  }
  return rc;
}

/* 
** Execute zSql on database db. Return an error code.
**在数据库db执行zSql。返回一个错误代码。

*/
static int execSql(sqlite3 *db, char **pzErrMsg, const char *zSql){
  sqlite3_stmt *pStmt;
  VVA_ONLY( int rc; )
  if( !zSql ){
    return SQLITE_NOMEM;
  }
  if( SQLITE_OK!=sqlite3_prepare(db, zSql, -1, &pStmt, 0) ){
    sqlite3SetString(pzErrMsg, db, sqlite3_errmsg(db));
    return sqlite3_errcode(db);
  }
  VVA_ONLY( rc = ) sqlite3_step(pStmt);
  assert( rc!=SQLITE_ROW || (db->flags&SQLITE_CountRows) );
  return vacuumFinalize(db, pStmt, pzErrMsg);
}

/*
** Execute zSql on database db. The statement returns exactly
** one column. Execute this as SQL on the same database.
**在数据库db上执行zSql。在同一个数据库执行此SQL。该语句返回到底一列。

*/
static int execExecSql(sqlite3 *db, char **pzErrMsg, const char *zSql){
  sqlite3_stmt *pStmt;
  int rc;

  rc = sqlite3_prepare(db, zSql, -1, &pStmt, 0);
  if( rc!=SQLITE_OK ) return rc;

  while( SQLITE_ROW==sqlite3_step(pStmt) ){
    rc = execSql(db, pzErrMsg, (char*)sqlite3_column_text(pStmt, 0));
    if( rc!=SQLITE_OK ){
      vacuumFinalize(db, pStmt, pzErrMsg);
      return rc;
    }
  }

  return vacuumFinalize(db, pStmt, pzErrMsg);
}

/*
** The non-standard VACUUM command is used to clean up the database,
** collapse free space, etc.  It is modelled after the VACUUM command
** in PostgreSQL.
**
** In version 1.0.x of SQLite, the VACUUM command would call
** gdbm_reorganize() on all the database tables.  But beginning
** with 2.0.0, SQLite no longer uses GDBM so this command has
** become a no-op.
**非标准空命令是用来清理数据库,在自由空间释放等等。这是在PostgreSQL中，仿效真空的命令在SQLite的版本1.0x,真空命令调用gdbm_reorganize()在所有数据库
**表。但开始与2.0.0 SQLite不再使用GDBM所以这个命令成为一个空操作。

*/
void sqlite3Vacuum(Parse *pParse){
  Vdbe *v = sqlite3GetVdbe(pParse);
  if( v ){
    sqlite3VdbeAddOp2(v, OP_Vacuum, 0, 0);
  }
  return;
}

/*
** This routine implements the OP_Vacuum opcode of the VDBE.
**这个例程实现OP_Vacuum VDBE的操作码。

*/
int sqlite3RunVacuum(char **pzErrMsg, sqlite3 *db){
  int rc = SQLITE_OK;     /* 从服务例程返回的代码 */
  Btree *pMain;           /* 数据库被用去除 */
  Btree *pTemp;           /* 我们清空临时数据库 */
  char *zSql = 0;         /* SQL语句 */
  int saved_flags;        /* 保存db - >标记的值 */
  int saved_nChange;      /* 保存db - > nChange的值 */
  int saved_nTotalChange; /*保存db - > nTotalChange的值 */
  void (*saved_xTrace)(void*,const char*);  /* 保存db - > xTrace */
  Db *pDb = 0;            /*数据库在vaccum结束处分离 */
  int isMemDb;            /* 真的如果vaccum:内存:数据库 */
  int nRes;               /* 在每个页面结尾有字节的预留空间 */
  int nDb;                /*一定数量的附加数据库 */

  if( !db->autoCommit ){
    sqlite3SetString(pzErrMsg, db, "cannot VACUUM from within a transaction");
    return SQLITE_ERROR;
  }
  if( db->activeVdbeCnt>1 ){
    sqlite3SetString(pzErrMsg, db,"cannot VACUUM - SQL statements in progress");
    return SQLITE_ERROR;
  }

  /* Save the current value of the database flags so that it can be 
  ** restored before returning. Then set the writable-schema flag, and
  ** disable CHECK and foreign key constraints. 
  **保存当前值数据库标志的当前值,可以
  **恢复之前存储。然后设置writable-schema标志,
  **不设置外键约束。

  */
  
  saved_flags = db->flags;
  saved_nChange = db->nChange;
  saved_nTotalChange = db->nTotalChange;
  saved_xTrace = db->xTrace;
  db->flags |= SQLITE_WriteSchema | SQLITE_IgnoreChecks | SQLITE_PreferBuiltin;
  db->flags &= ~(SQLITE_ForeignKeys | SQLITE_ReverseOrder);
  db->xTrace = 0;

  pMain = db->aDb[0].pBt;
  isMemDb = sqlite3PagerIsMemdb(sqlite3BtreePager(pMain));

  /* Attach the temporary database as 'vacuum_db'. The synchronous pragma
  ** can be set to 'off' for this file, as it is not recovered if a crash
  ** occurs anyway. The integrity of the database is maintained by a
  ** (possibly synchronous) transaction opened on the main database before
  ** sqlite3BtreeCopyFile() is called.
  **附加的临时数据库作为“vacuum_db”。对这个文件同步编译指示可以设置为“关闭”,因为如果崩溃随时发生
  **它不能恢复.sqlite3BtreeCopyFile()被调用之前数据库的完整性被在主数据库上打开的(可能是同步的)事务维持。

  **
  ** An optimisation would be to use a non-journaled pager.
  ** (Later:) I tried setting "PRAGMA vacuum_db.journal_mode=OFF" but
  ** that actually made the VACUUM run slower.  Very little journalling
  ** actually occurs when doing a vacuum since the vacuum_db is initially
  ** empty.  Only the journal header is written.  Apparently it takes more
  ** time to parse and run the PRAGMA to turn journalling off than it does
  ** to write the journal header file.
  * *一种优化是使用non-journaled页。
* *(后:)我试着设置"PRAGMA vacuum_db.journal_mode=OFF"但
* *实际上使vaccum运行得更慢。vacuum_db最初是空的很少的日志
* *会实际上发生。只写日志头会被写。显然需要更多的
* *时间解析和运行编译指示关闭日志而不是编写头文件。
  */
  nDb = db->nDb;
  if( sqlite3TempInMemory(db) ){
    zSql = "ATTACH ':memory:' AS vacuum_db;";
  }else{
    zSql = "ATTACH '' AS vacuum_db;";
  }
  rc = execSql(db, pzErrMsg, zSql);
  if( db->nDb>nDb ){
    pDb = &db->aDb[db->nDb-1];
    assert( strcmp(pDb->zName,"vacuum_db")==0 );
  }
  if( rc!=SQLITE_OK ) goto end_of_vacuum;
  pTemp = db->aDb[db->nDb-1].pBt;

  /* The call to execSql() to attach the temp database has left the file
  ** locked (as there was more than one active statement when the transaction
  ** to read the schema was concluded. Unlock it here so that this doesn't
  ** cause problems for the call to BtreeSetPageSize() below. 
  **将execSql()的调用到这个临时数据库已经残存文件
  * *锁(阅读模式得出结论时，有多个事务时主动声明
  * *在这里解锁,这并不造成以后问题的调用BtreeSetPageSize()。
  */
  sqlite3BtreeCommit(pTemp);

  nRes = sqlite3BtreeGetReserve(pMain);

  /*VACCUM不能改变一个加密数据库的页大小。 */
#ifdef SQLITE_HAS_CODEC
  if( db->nextPagesize ){
    extern void sqlite3CodecGetKey(sqlite3*, int, void**, int*);
    int nKey;
    char *zKey;
    sqlite3CodecGetKey(db, 0, (void**)&zKey, &nKey);
    if( nKey ) db->nextPagesize = 0;
  }
#endif

  rc = execSql(db, pzErrMsg, "PRAGMA vacuum_db.synchronous=OFF");
  if( rc!=SQLITE_OK ) goto end_of_vacuum;

  /* Begin a transaction and take an exclusive lock on the main database
  ** file. This is done before the sqlite3BtreeGetPageSize(pMain) call below,
  ** to ensure that we do not try to change the page-size on a WAL database。
  * *开始一个事务并将独占锁在主数据库中
  * *文件。这样做是在sqlite3BtreeGetPageSize(pMain)调用下面,
  * *,以确保WAL数据库上我们不试图改变页面大小。
  */
  rc = execSql(db, pzErrMsg, "BEGIN;");
  if( rc!=SQLITE_OK ) goto end_of_vacuum;
  rc = sqlite3BtreeBeginTrans(pMain, 2);
  if( rc!=SQLITE_OK ) goto end_of_vacuum;

  /* 不要试图改变WAL数据库的页面大小*/
  if( sqlite3PagerGetJournalMode(sqlite3BtreePager(pMain))
                                               ==PAGER_JOURNALMODE_WAL ){
    db->nextPagesize = 0;
  }

  if( sqlite3BtreeSetPageSize(pTemp, sqlite3BtreeGetPageSize(pMain), nRes, 0)
   || (!isMemDb && sqlite3BtreeSetPageSize(pTemp, db->nextPagesize, nRes, 0))
   || NEVER(db->mallocFailed)
  ){
    rc = SQLITE_NOMEM;
    goto end_of_vacuum;
  }

#ifndef SQLITE_OMIT_AUTOVACUUM
  sqlite3BtreeSetAutoVacuum(pTemp, db->nextAutovac>=0 ? db->nextAutovac :
                                           sqlite3BtreeGetAutoVacuum(pMain));
#endif

  /* Query the schema of the main database. Create a mirror schema
  ** in the temporary database.
  **主要数据库的查询模式。在临时数据库中创建一个镜像模式。
  */
  rc = execExecSql(db, pzErrMsg,
      "SELECT 'CREATE TABLE vacuum_db.' || substr(sql,14) "
      "  FROM sqlite_master WHERE type='table' AND name!='sqlite_sequence'"
      "   AND rootpage>0"
  );
  if( rc!=SQLITE_OK ) goto end_of_vacuum;
  rc = execExecSql(db, pzErrMsg,
      "SELECT 'CREATE INDEX vacuum_db.' || substr(sql,14)"
      "  FROM sqlite_master WHERE sql LIKE 'CREATE INDEX %' ");
  if( rc!=SQLITE_OK ) goto end_of_vacuum;
  rc = execExecSql(db, pzErrMsg,
      "SELECT 'CREATE UNIQUE INDEX vacuum_db.' || substr(sql,21) "
      "  FROM sqlite_master WHERE sql LIKE 'CREATE UNIQUE INDEX %'");
  if( rc!=SQLITE_OK ) goto end_of_vacuum;

  /* Loop through the tables in the main database. For each, do
  ** an "INSERT INTO vacuum_db.xxx SELECT * FROM main.xxx;" to copy
  ** the contents to the temporary database.
  **在主数据库遍历表。对于每一个, 做"INSERT INTO vacuum_db.xxx SELECT * FROM main.xxx;"
  **去复制内容到临时数据库。
  */
  rc = execExecSql(db, pzErrMsg,
      "SELECT 'INSERT INTO vacuum_db.' || quote(name) "
      "|| ' SELECT * FROM main.' || quote(name) || ';'"
      "FROM main.sqlite_master "
      "WHERE type = 'table' AND name!='sqlite_sequence' "
      "  AND rootpage>0"
  );
  if( rc!=SQLITE_OK ) goto end_of_vacuum;

  /* Copy over the sequence table
  */
  rc = execExecSql(db, pzErrMsg,
      "SELECT 'DELETE FROM vacuum_db.' || quote(name) || ';' "
      "FROM vacuum_db.sqlite_master WHERE name='sqlite_sequence' "
  );
  if( rc!=SQLITE_OK ) goto end_of_vacuum;
  rc = execExecSql(db, pzErrMsg,
      "SELECT 'INSERT INTO vacuum_db.' || quote(name) "
      "|| ' SELECT * FROM main.' || quote(name) || ';' "
      "FROM vacuum_db.sqlite_master WHERE name=='sqlite_sequence';"
  );
  if( rc!=SQLITE_OK ) goto end_of_vacuum;


  /* Copy the triggers, views, and virtual tables from the main database
  ** over to the temporary database.  None of these objects has any
  ** associated storage, so all we have to do is copy their entries
  ** from the SQLITE_MASTER table.
  **复制触发器、视图和虚拟表从主数据库中
* *到临时数据库。这些对象没有和任何
* *存储有关,所以我们要做的就是从SQLITE_MASTER表复制他们的条目。
  */
  rc = execSql(db, pzErrMsg,
      "INSERT INTO vacuum_db.sqlite_master "
      "  SELECT type, name, tbl_name, rootpage, sql"
      "    FROM main.sqlite_master"
      "   WHERE type='view' OR type='trigger'"
      "      OR (type='table' AND rootpage=0)"
  );
  if( rc ) goto end_of_vacuum;

  /* At this point, there is a write transaction open on both the 
  ** vacuum database and the main database. Assuming no error occurs,
  ** both transactions are closed by this block - the main database
  ** transaction by sqlite3BtreeCopyFile() and the other by an explicit
  ** call to sqlite3BtreeCommit().
  **在这一点上,有一个写事务上在
* *vaccum数据库和主要数据库都开放。如果没有错误发生,
* *交易都关闭这个块——主要的数据库
* *事务由sqlite3BtreeCopyFile()和另一个显式的
* *调用sqlite3BtreeCommit()。
  */
  {
    u32 meta;
    int i;

    /* This array determines which meta meta values are preserved in the
    ** vacuum.  Even entries are the meta value number and odd entries
    ** are an increment to apply to the meta value after the vacuum.
    ** The increment is used to increase the schema cookie so that other
    ** connections to the same database will know to reread the schema.
    这个数组保存在确定哪些meta元值
* *被保留在vaccum上。在vaccum后，甚至条目元值和奇怪的条目数量
* *是一个增量应用到的元价值。
* *使用增量增加主题cookie,这样其他的模式
* *连接到同一个数据库会知道重读模式。
    */
    static const unsigned char aCopy[] = {
       BTREE_SCHEMA_VERSION,     1,  /* 添加一个旧模式的cookie*/
       BTREE_DEFAULT_CACHE_SIZE, 0,  /* 保持默认的页面缓存大小 */
       BTREE_TEXT_ENCODING,      0,  /* 保存文本编码 */
       BTREE_USER_VERSION,       0,  /* 保存用户版本 */
    };

    assert( 1==sqlite3BtreeIsInTrans(pTemp) );
    assert( 1==sqlite3BtreeIsInTrans(pMain) );

    /* 复制Btree元值 */
    for(i=0; i<ArraySize(aCopy); i+=2){
      /* 在这个上下文GetMeta()和UpdateMeta()不能失败,
  * *因为我们已经有第1页加载到缓存和标记为废弃。 */
      sqlite3BtreeGetMeta(pMain, aCopy[i], &meta);
      rc = sqlite3BtreeUpdateMeta(pTemp, aCopy[i], meta+aCopy[i+1]);
      if( NEVER(rc!=SQLITE_OK) ) goto end_of_vacuum;
    }

    rc = sqlite3BtreeCopyFile(pMain, pTemp);
    if( rc!=SQLITE_OK ) goto end_of_vacuum;
    rc = sqlite3BtreeCommit(pTemp);
    if( rc!=SQLITE_OK ) goto end_of_vacuum;
#ifndef SQLITE_OMIT_AUTOVACUUM
    sqlite3BtreeSetAutoVacuum(pMain, sqlite3BtreeGetAutoVacuum(pTemp));
#endif
  }

  assert( rc==SQLITE_OK );
  rc = sqlite3BtreeSetPageSize(pMain, sqlite3BtreeGetPageSize(pTemp), nRes,1);

end_of_vacuum:
  /* 恢复原始db - >标记的值 */
  db->flags = saved_flags;
  db->nChange = saved_nChange;
  db->nTotalChange = saved_nTotalChange;
  db->xTrace = saved_xTrace;
  sqlite3BtreeSetPageSize(pMain, -1, -1, 1);

  /* Currently there is an SQL level transaction open on the vacuum
  ** database. No locks are held on any other files (since the main file
  ** was committed at the btree level). So it safe to end the transaction
  ** by manually setting the autoCommit flag to true and detaching the
  ** vacuum database. The vacuum_db journal file is deleted when the pager
  ** is closed by the DETACH.
  **目前有一个SQL级事务在vaccum数据库中打开。没有锁在任何其他文件
  * *(因为没有任何锁的主要文件承诺在btree级别)。所以安全结束事务
  * *通过手动将autoCommit标志设置为true,分离
  * *vaccum数据库。当分离关闭，vacuum_db日志文件被删除。
  */
  db->autoCommit = 1;

  if( pDb ){
    sqlite3BtreeClose(pDb->pBt);
    pDb->pBt = 0;
    pDb->pSchema = 0;
  }

  /* 都清除了模式和减少了db->aDb[]数组的大小 */ 
 

  return rc;
}

#endif  /* SQLITE_OMIT_VACUUM && SQLITE_OMIT_ATTACH */
