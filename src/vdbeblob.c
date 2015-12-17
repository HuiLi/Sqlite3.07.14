/*
** 2007 May 1
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** BLOB (binary large object),二进制大对象，是一个可以存储二进制文件的容器
** BLOB常常是数据库中用来存储二进制文件的字段类型
** BLOB是一个大文件，典型的BLOB是一张图片或一个声音文件，由于它们的尺寸，必须使用特殊的
** 方式来处理（例如：上传、下载或者存放到一个数据库）
** PS:blob类型知识的补充
** BLOB数据类型是适合用于存储可变长度的二进制大对象数据以及可变长度的音频和视频数据。在
** SQLite的BLOB类型中存储复杂的数据时，长度是不加限制的[2]。使用B-树索引来管理和组织属性数据，
** 通过SQL语句到数据库相关数据中实现增、删、改、查的操作。SQLite不支持静态数据类型，而是使用列
** 关系。当数据记录的字段内容插入到数据库中时，
** SQLite将对该字段内容的类型做检查，若类型不能匹配到相关联的列，则SQLite会将该字段内容转换成列
** 的类型。因此，数据库中BLOB数据类型的合理应用也直接影响存储效率和查询速度的提高。
** This file contains code used to implement incremental BLOB I/O.
** 这个文件包含代码用于实现增量BLOB I/O
*/

//BLOB: 值是数据的二进制对象，如何输入就如何存储，不改变格式。

/** 引入两个头文件：
** vdbeInt.h中定义了VDBE常用的数据结构；
** sqliteInt.h中定义了SQLite的内部接口和数据结构。
**/
#include "sqliteInt.h"
#include "vdbeInt.h"

#ifndef SQLITE_OMIT_INCRBLOB

/*
** Valid sqlite3_blob* handles point to Incrblob structures.
** 有效sqlite3_blob*处理点Incrblob结构
*/
typedef struct Incrblob Incrblob;
struct Incrblob {
  int flags;              /* Copy of "flags" passed to sqlite3_blob_open() 
                           "flags"值是被传递给函数sqlite3_blob_open()的标记值的副本*/
  int nByte ;              /* Size of open blob, in bytes 
                           打开blob的大小，以字节计算*/
  int iOffset;            /* Byte offset of blob in cursor data 
                           在游标数据块的字节偏移量*/
  int iCol;               /* Table column this handle is open on 
                           表列数这个句柄是打开的*/
  BtCursor *pCsr;         /* Cursor pointing at blob row 
                           游标指针在blob行*/
  sqlite3_stmt *pStmt;    /* Statement holding cursor open
                           语句保持游标打开*/
  sqlite3 *db;            /* The associated database 
                           相关的数据库*/
};


/*
** This function is used by both blob_open() and blob_reopen(). It seeks
** the b-tree cursor associated with blob handle p to point to row iRow.
** If successful, SQLITE_OK is returned and subsequent calls to
** sqlite3_blob_read() or sqlite3_blob_write() access the specified row.
** sqlite3_blob_read() or sqlite3_blob_write() access the specified row.
** 这个函数的功能由blob_open()和blob_reopen()使用。它寻找相关的B树游标和blob
** 类型的句柄定义为p去指出行用iRow标记。如果成功，返回SQLITE_OK和随后
** 调用sqlite3_blob_read()或者sqlite3_blob_write()访问特定的行
**
** If an error occurs, or if the specified row does not exist or does not
** contain a value of type TEXT or BLOB in the column nominated when the
** blob handle was opened, then an error code is returned and *pzErr may
** be set to point to a buffer containing an error message. It is the
** responsibility of the caller to free the error message buffer using
** sqlite3DbFree().
** 如果有错误出现，或者如果访问的行不存在或者不包含TEXT或者BLOB类型，
** 当在提名列时，BLOB手柄被打开，然后一个错误代码被返回和*pzErr指针类型
** 可能被指向一个缓冲区其包含一个错误信息。这是调用者自由使用sqlite3DbFree()
** 错误缓冲区的责任。
**
** If an error does occur, then the b-tree cursor is closed. All subsequent
** calls to sqlite3_blob_read(), blob_write() or blob_reopen() will 
** immediately return SQLITE_ABORT.
** 如果一个错误发生，之后B树游标被关闭。之后所有调用sqlite3_blob_read(), blob_write() 
** 或者 blob_reopen()函数立即返回SQLITE_ABORT。
*/
static int blobSeekToRow(Incrblob *p, sqlite3_int64 iRow, char **pzErr){
  int rc;                         /* Error code 错误代码 */
  char *zErr = 0;                 /* Error message 错误信息*/
  Vdbe *v = (Vdbe *)p->pStmt;

  /* Set the value of the SQL statements only variable to integer iRow. 
  ** This is done directly instead of using sqlite3_bind_int64() to avoid 
  ** triggering asserts related to mutexes.
  ** 设置SQL语句的值，唯一的变量为整数iRow
  ** 这是直接完成的，而不是使用sqlite3_bind_int64()，以避免触发断言相关的互斥
  */
  assert( v->aVar[0].flags&MEM_Int );//assert就表示断言，
                                     //断言就是用于在代码中捕捉这些假设，可以将断言看作是异常处理的一种高级形式。
  v->aVar[0].u.i = iRow;

  rc = sqlite3_step(p->pStmt);       //sqlite3_step()表示如果一个模式出现错误，回调sqlite3reprepare()再试，
                                     //再执行一次sqlite3reprepare()
  if( rc==SQLITE_ROW ){
    u32 type = v->apCsr[0]->aType[p->iCol];
    if( type<12 ){                 
      zErr = sqlite3MPrintf(p->db, "cannot open value of type %s",
          type==0?"null": type==7?"real": "integer"
      );
      rc = SQLITE_ERROR;
      sqlite3_finalize(p->pStmt);    //sqlite3_finalize()表示被回调去删除一个已经准备好的语句。
      p->pStmt = 0;
    }else{
      p->iOffset = v->apCsr[0]->aOffset[p->iCol];
      p->nByte = sqlite3VdbeSerialTypeLen(type);
      p->pCsr =  v->apCsr[0]->pCursor;
      sqlite3BtreeEnterCursor(p->pCsr);
      sqlite3BtreeCacheOverflow(p->pCsr);
      sqlite3BtreeLeaveCursor(p->pCsr);
    }
  }

  if( rc==SQLITE_ROW ){
    rc = SQLITE_OK;
  }else if( p->pStmt ){
    rc = sqlite3_finalize(p->pStmt);
    p->pStmt = 0;
    if( rc==SQLITE_OK ){
      zErr = sqlite3MPrintf(p->db, "no such rowid: %lld", iRow);//输出这行的信息
      rc = SQLITE_ERROR;
    }else{
      zErr = sqlite3MPrintf(p->db, "%s", sqlite3_errmsg(p->db));
    }
  }

  assert( rc!=SQLITE_OK || zErr==0 );
  assert( rc!=SQLITE_ROW && rc!=SQLITE_DONE );

  *pzErr = zErr;
  return rc;
}

/*
** Open a blob handle.表示打开一个blob类型的句柄(句柄表示一个32位的整数，它代表一个对象)
*/
int sqlite3_blob_open(
  sqlite3* db,            /* The database connection 
                              数据库连接*/
  const char *zDb,        /* The attached database containing the blob 
                          附加的数据库包含blob*/
  const char *zTable,     /* The table containing the blob 
                          数据库表包含blob*/
  const char *zColumn,    /* The column containing the blob 
                          数据库表列包含blob*/
  sqlite_int64 iRow,      /* The row containing the blob 
                          数据库行包含blob*/
  int flags,              /* True -> read/write access, false -> read-only 
                          标记如果True则有有read/write的权限，如果错误仅仅有read的权限*/
  sqlite3_blob **ppBlob   /* Handle for accessing the blob returned here 
                          处理访问这里的blob返回*/
){
  int nAttempt = 0;
  int iCol;               /* Index of zColumn in row-record
                          zColumn的行记录的索引 */

  /* This VDBE program seeks a btree cursor to the identified 
  ** db/table/row entry. The reason for using a vdbe program instead
  ** of writing code to use the b-tree layer directly is that the
  ** vdbe program will take advantage of the various transaction,
  ** locking and error handling infrastructure built into the vdbe.
  **
  ** 这VDBE程序旨在B树游标 移动到所确定的DB/表/行条目。
  ** 之所以使用一个VDBE程序，而不是写代码直接使用b树层是VDBE程序将利用各种事务，
  ** 锁定和错误处理基础设施内置于VDBE。
  ** After seeking the cursor, the vdbe executes an OP_ResultRow.
  ** Code external to the Vdbe then "borrows" the b-tree cursor and
  ** uses it to implement the blob_read(), blob_write() and 
  ** blob_bytes() functions.
  ** 寻求光标之后，VDBE执行的OP_ResultRow。
  ** 类外部的代码VDBE再“借”B-树游标和用它来实现blob_read(),blob_write()和blob_bytes()函数

  ** The sqlite3_blob_close() function finalizes the vdbe program,
  ** which closes the b-tree cursor and (possibly) commits the 
  ** transaction.
  ** 该sqlite3_blob_close()函数最终确定VDBE程序，该程序关闭B树光标(可能)提交事务
  */
  static const VdbeOpList openBlob[] = {   //定义一个静态数组结构，对vdbe操作码的定义
    {OP_Transaction, 0, 0, 0},     /* 0: Start a transaction 开始一个事务*/
    {OP_VerifyCookie, 0, 0, 0},    /* 1: Check the schema cookie 检查模式的cookie*/
    {OP_TableLock, 0, 0, 0},       /* 2: Acquire a read or write lock 获取一个读或者写的加锁*/

    /* One of the following two instructions is replaced by an OP_Noop. 
    被一个OP_Noop所取代下面二个指令*/
    {OP_OpenRead, 0, 0, 0},        /* 3: Open cursor 0 for reading 
                                    打开一个游标去读*/
    {OP_OpenWrite, 0, 0, 0},       /* 4: Open cursor 0 for read/write 
                                    打开游标去读或写*/

    {OP_Variable, 1, 1, 1},        /* 5: Push the rowid to the stack 
                                    把一个rowid压入到栈中*/
    {OP_NotExists, 0, 10, 1},      /* 6: Seek the cursor 
                                    查询游标 */
    {OP_Column, 0, 0, 1},          /* 7  */
    {OP_ResultRow, 1, 0, 0},       /* 8  */
    {OP_Goto, 0, 5, 0},            /* 9  */
    {OP_Close, 0, 0, 0},           /* 10 */
    {OP_Halt, 0, 0, 0},            /* 11 */
  };

  int rc = SQLITE_OK;
  char *zErr = 0;
  Table *pTab;
  Parse *pParse = 0;
  Incrblob *pBlob = 0;

  flags = !!flags;                /* flags = (flags ? 1 : 0); */
  *ppBlob = 0;

  sqlite3_mutex_enter(db->mutex);

  pBlob = (Incrblob *)sqlite3DbMallocZero(db, sizeof(Incrblob));
  if( !pBlob ) goto blob_open_out;
  pParse = sqlite3StackAllocRaw(db, sizeof(*pParse));
  if( !pParse ) goto blob_open_out;

  do {
    memset(pParse, 0, sizeof(Parse));//给pParse分配sizeof(Parse)大小的空间
    pParse->db = db;
    sqlite3DbFree(db, zErr);  //释放数据库db的内存
    zErr = 0;

    sqlite3BtreeEnterAll(db);
    pTab = sqlite3LocateTable(pParse, 0, zTable, zDb);
    if( pTab && IsVirtual(pTab) ){
      pTab = 0;
      sqlite3ErrorMsg(pParse, "cannot open virtual table: %s", zTable);
    }
#ifndef SQLITE_OMIT_VIEW
    if( pTab && pTab->pSelect ){
      pTab = 0;
      sqlite3ErrorMsg(pParse, "cannot open view: %s", zTable);
    }
#endif
    if( !pTab ){
      if( pParse->zErrMsg ){
        sqlite3DbFree(db, zErr);
        zErr = pParse->zErrMsg;
        pParse->zErrMsg = 0;
      }
      rc = SQLITE_ERROR;
      sqlite3BtreeLeaveAll(db);
      goto blob_open_out;
    }

    /* Now search pTab for the exact column. 
       现在查询表pTab目的为了外部列
    */
    for(iCol=0; iCol<pTab->nCol; iCol++) {

      if( sqlite3StrICmp(pTab->aCol[iCol].zName, zColumn)==0 ){
        break;
      }
    }
    if( iCol==pTab->nCol ){
      sqlite3DbFree(db, zErr);
      zErr = sqlite3MPrintf(db, "no such column: \"%s\"", zColumn);
      rc = SQLITE_ERROR;
      sqlite3BtreeLeaveAll(db);
      goto blob_open_out;
    }

    /* If the value is being opened for writing, check that the
    ** column is not indexed, and that it is not part of a foreign key. 
    ** It is against the rules to open a column to which either of these
    ** descriptions applies for writing.  
  ** 如果该值被打开允许去写，检查列没有被索引，并且它不是一个外键的一部分。
    ** 这是违反规定开一列到以下任一说明适用于写作。
  */
    if( flags ){
      const char *zFault = 0;
      Index *pIdx;
#ifndef SQLITE_OMIT_FOREIGN_KEY
      if( db->flags&SQLITE_ForeignKeys ){
        /* Check that the column is not part of an FK child key definition. It
        ** is not necessary to check if it is part of a parent key, as parent
        ** key columns must be indexed. The check below will pick up this 
        ** case. 
        ** 检查列不是一个FK子键定义的一部分,
        ** 它没有必要检查,如果是父母key的一部分
        ** 作为父母键一定必须被索引，下面的检查将会拿起这个案例
    */
        FKey *pFKey;
        for(pFKey=pTab->pFKey; pFKey; pFKey=pFKey->pNextFrom){
          int j;
          for(j=0; j<pFKey->nCol; j++){
            if( pFKey->aCol[j].iFrom==iCol ){
              zFault = "foreign key";
            }
          }
        }
      }
#endif
      for(pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext){
        int j;
        for(j=0; j<pIdx->nColumn; j++){
          if( pIdx->aiColumn[j]==iCol ){
            zFault = "indexed";
          }
        }
      }
      if( zFault ){
        sqlite3DbFree(db, zErr);
        zErr = sqlite3MPrintf(db, "cannot open %s column for writing", zFault);
        rc = SQLITE_ERROR;
        sqlite3BtreeLeaveAll(db);
        goto blob_open_out;
      }
    }

    pBlob->pStmt = (sqlite3_stmt *)sqlite3VdbeCreate(db); //数据库引擎的创建
    assert( pBlob->pStmt || db->mallocFailed );
    if( pBlob->pStmt ){
      Vdbe *v = (Vdbe *)pBlob->pStmt;
      int iDb = sqlite3SchemaToIndex(db, pTab->pSchema);

      sqlite3VdbeAddOpList(v, sizeof(openBlob)/sizeof(VdbeOpList), openBlob);


      /* Configure the OP_Transaction 
        配置操作码事务Transaction
      */
      sqlite3VdbeChangeP1(v, 0, iDb);
      sqlite3VdbeChangeP2(v, 0, flags);

      /* Configure the OP_VerifyCookie 
        配置操作码VerifyCookie
      */
      sqlite3VdbeChangeP1(v, 1, iDb);
      sqlite3VdbeChangeP2(v, 1, pTab->pSchema->schema_cookie);
      sqlite3VdbeChangeP3(v, 1, pTab->pSchema->iGeneration);

      /* Make sure a mutex is held on the table to be accessed 
        确保一个互斥表持有被访问
      */
      sqlite3VdbeUsesBtree(v, iDb); 

      /* Configure the OP_TableLock instruction 
        配置操作码TableLock指令
      */
#ifdef SQLITE_OMIT_SHARED_CACHE
      sqlite3VdbeChangeToNoop(v, 2);//改变操作码
#else
      sqlite3VdbeChangeP1(v, 2, iDb);
      sqlite3VdbeChangeP2(v, 2, pTab->tnum);
      sqlite3VdbeChangeP3(v, 2, flags);
      sqlite3VdbeChangeP4(v, 2, pTab->zName, P4_TRANSIENT);
#endif

      /* Remove either the OP_OpenWrite or OpenRead. Set the P2 
      ** parameter of the other to pTab->tnum. 
    ** 删除OP_OpenWrite或OpenRead,P2参数设置的其他pTab->tnum
    */
      sqlite3VdbeChangeToNoop(v, 4 - flags);
      sqlite3VdbeChangeP2(v, 3 + flags, pTab->tnum);
      sqlite3VdbeChangeP3(v, 3 + flags, iDb);

      /* Configure the number of columns. Configure the cursor to
      ** think that the table has one more column than it really
      ** does. An OP_Column to retrieve this imaginary column will
      ** always return an SQL NULL. This is useful because it means
      ** we can invoke OP_Column to fill in the vdbe cursors type 
      ** and offset cache without causing any IO.
      ** 配置列数,配置这个游标去确认这个表有至少一列确实这样做了。
      ** 一个OP_Column检索这个虚构列将始终返回SQL NULL
      ** 因为这意味着我们可以调用OP_Column填写的VDBE游标类型和偏移的缓存，
      ** 而不会造成任何IO这是非常有用
      */
      sqlite3VdbeChangeP4(v, 3+flags, SQLITE_INT_TO_PTR(pTab->nCol+1),P4_INT32);
      sqlite3VdbeChangeP2(v, 7, pTab->nCol);
      if( !db->mallocFailed ){
        pParse->nVar = 1;
        pParse->nMem = 1;
        pParse->nTab = 1;
        sqlite3VdbeMakeReady(v, pParse);  //为创建vdbe做准备
      }
    }
   
    pBlob->flags = flags;
    pBlob->iCol = iCol;
    pBlob->db = db;
    sqlite3BtreeLeaveAll(db);
    if( db->mallocFailed ){
      goto blob_open_out;
    }
    sqlite3_bind_int64(pBlob->pStmt, 1, iRow);
    rc = blobSeekToRow(pBlob, iRow, &zErr);
  } while( (++nAttempt)<5 && rc==SQLITE_SCHEMA );

blob_open_out:
  if( rc==SQLITE_OK && db->mallocFailed==0 ){
    *ppBlob = (sqlite3_blob *)pBlob;
  }else{
    if( pBlob && pBlob->pStmt ) sqlite3VdbeFinalize((Vdbe *)pBlob->pStmt);
    sqlite3DbFree(db, pBlob);
  }
  sqlite3Error(db, rc, (zErr ? "%s" : 0), zErr);
  sqlite3DbFree(db, zErr);
  sqlite3StackFree(db, pParse);
  rc = sqlite3ApiExit(db, rc);
  sqlite3_mutex_leave(db->mutex);
  return rc;
}

/*
** Close a blob handle that was previously created using
** sqlite3_blob_open().
** 关闭一个blob句柄以前创建的利用sqlite3_blob_open()函数
*/
int sqlite3_blob_close(sqlite3_blob *pBlob){
  Incrblob *p = (Incrblob *)pBlob;
  int rc;
  sqlite3 *db;

  if( p ){
    db = p->db;
    sqlite3_mutex_enter(db->mutex);
    rc = sqlite3_finalize(p->pStmt);
    sqlite3DbFree(db, p);
    sqlite3_mutex_leave(db->mutex);
  }else{
    rc = SQLITE_OK;
  }
  return rc;
}

/*
** Perform a read or write operation on a blob
** 对一个blob执行读或写操作
*/
static int blobReadWrite(
  sqlite3_blob *pBlob, 
  void *z, 
  int n, 
  int iOffset, 
  int (*xCall)(BtCursor*, u32, u32, void*)
){
  int rc;
  Incrblob *p = (Incrblob *)pBlob;
  Vdbe *v;
  sqlite3 *db;

  if( p==0 ) return SQLITE_MISUSE_BKPT;
  db = p->db;
  sqlite3_mutex_enter(db->mutex);
  v = (Vdbe*)p->pStmt;

  if( n<0 || iOffset<0 || (iOffset+n)>p->nByte ){
    /* Request is out of range. Return a transient error. 
       请求不在范围内，返回一个临时的错误
    */
    rc = SQLITE_ERROR;
    sqlite3Error(db, SQLITE_ERROR, 0);
  }else if( v==0 ){
    /* If there is no statement handle, then the blob-handle has
    ** already been invalidated. Return SQLITE_ABORT in this case.
    如果没有语句句柄,然后blob-handle已经失效。在这种情况下返回SQLITE_ABORT
    */
    rc = SQLITE_ABORT;
  }else{
    /* Call either BtreeData() or BtreePutData(). If SQLITE_ABORT is
    ** returned, clean-up the statement handle.
    调用btreedata()或btreeputdata()。如果sqlite_abort返回，清理语句句柄。
    */
    assert( db == v->db );
    sqlite3BtreeEnterCursor(p->pCsr);
    rc = xCall(p->pCsr, iOffset+p->iOffset, n, z);
    sqlite3BtreeLeaveCursor(p->pCsr);
    if( rc==SQLITE_ABORT ){
      sqlite3VdbeFinalize(v);
      p->pStmt = 0;
    }else{
      db->errCode = rc;
      v->rc = rc;
    }
  }
  rc = sqlite3ApiExit(db, rc);
  sqlite3_mutex_leave(db->mutex);
  return rc;
}

/*
** Read data from a blob handle.
    从数据库表中读取数据
*/
int sqlite3_blob_read(sqlite3_blob *pBlob, void *z, int n, int iOffset){
  return blobReadWrite(pBlob, z, n, iOffset, sqlite3BtreeData);
}

/*
** Write data to a blob handle.
    写数据到数据库表中
*/
int sqlite3_blob_write(sqlite3_blob *pBlob, const void *z, int n, int iOffset){
  return blobReadWrite(pBlob, (void *)z, n, iOffset, sqlite3BtreePutData);
}

/*
** Query a blob handle for the size of the data.
** 查询一个blob处理的数据的大小
** The Incrblob.nByte field is fixed for the lifetime of the Incrblob
** so no mutex is required for access.
   在Incrblob结构中。
   因为nByte字段是一直固定在内存中，所以Incrblob不需要互斥访问
*/
int sqlite3_blob_bytes(sqlite3_blob *pBlob){
  Incrblob *p = (Incrblob *)pBlob;
  return (p && p->pStmt) ? p->nByte : 0;
}

/*
** Move an existing blob handle to point to a different row of the same
** database table.
** 在同一个数据库表中移动一个现有的blob类型的句柄指向一个不同行
**
** If an error occurs, or if the specified row does not exist or does not
** contain a blob or text value, then an error code is returned and the
** database handle error code and message set. If this happens, then all 
** subsequent calls to sqlite3_blob_xxx() functions (except blob_close()) 
** immediately return SQLITE_ABORT.
** 如果有错误出现,或者如果这个特定的行不存在或者不包含blob和text的值,然后一个
** 错误代码被返回，和数据库处理的错误代码和消息集。
** 如果这些发生了，接着所有调用sqlite3_blob_xxx()函数(除了blob_close()函数)，
** 被立即返回SQLITE_ABORT
*/
int sqlite3_blob_reopen(sqlite3_blob *pBlob, sqlite3_int64 iRow){
  int rc;
  Incrblob *p = (Incrblob *)pBlob;
  sqlite3 *db;

  if( p==0 ) return SQLITE_MISUSE_BKPT;
  db = p->db;
  sqlite3_mutex_enter(db->mutex);

  if( p->pStmt==0 ){
    /* If there is no statement handle, then the blob-handle has
    ** already been invalidated. Return SQLITE_ABORT in this case.
    ** 如果这里没有语句要处理，并且blob_handle已经失效，
    ** 在这个最后则返回一个SQLITE_ABORT。
    */
    rc = SQLITE_ABORT;
  }else{
    char *zErr;
    rc = blobSeekToRow(p, iRow, &zErr);
    if( rc!=SQLITE_OK ){
      sqlite3Error(db, rc, (zErr ? "%s" : 0), zErr);
      sqlite3DbFree(db, zErr);
    }
    assert( rc!=SQLITE_SCHEMA );
  }

  rc = sqlite3ApiExit(db, rc);
  assert( rc==SQLITE_OK || p->pStmt==0 );
  sqlite3_mutex_leave(db->mutex);
  return rc;
}

#endif /* #ifndef SQLITE_OMIT_INCRBLOB */
