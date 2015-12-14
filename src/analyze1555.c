/*********************************
student1555修改，因冲突，另名提交，最后整合。
**********************************/
/*
** 2005 July 8
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
**************************************************************************/

/*
** This file contains code associated with the ANALYZE command.
**
** 此文件包含与ANALYZE命令有关的代码。
**
** The ANALYZE command gather statistics about the content of tables
** and indices.  These statistics are made available to the query planner
** to help it make better decisions about how to perform queries.
**
** ANALYZE命令收集统计表和索引的内容。这些统计数据提供给查询计划，以帮助其更好的决定如何执行查询。
**
** The following system tables are or have been supported:
** 支持如下的系统表：
**
**    CREATE TABLE sqlite_stat1(tbl, idx, stat);
**    CREATE TABLE sqlite_stat2(tbl, idx, sampleno, sample);
**    CREATE TABLE sqlite_stat3(tbl, idx, nEq, nLt, nDLt, sample);
**
** Additional tables might be added in future releases of SQLite.
** The sqlite_stat2 table is not created or used unless the SQLite version
** is between 3.6.18 and 3.7.8, inclusive, and unless SQLite is compiled
** with SQLITE_ENABLE_STAT2.  The sqlite_stat2 table is deprecated.
** The sqlite_stat2 table is superceded by sqlite_stat3, which is only
** created and used by SQLite versions 3.7.9 and later and with
** SQLITE_ENABLE_STAT3 defined.  The fucntionality of sqlite_stat3
** is a superset of sqlite_stat2.  
**
** SQLite将来可能会添加额外的表。sqlite_stat2不会被创建并使用，除非SQLite版本介于
** 3.6.18与3.7.8之间，并且SQLite由SQLITE_ENABLE_STAT2编译。sqlite_stat2被放弃。
** sqlite_stat3取代sqlite_stat2，它只用于版本3.7.9及后续版本，且由SQLITE_ENABLE_STAT3
** 定义。sqlite_stat3的功能是sqlite_stat2的扩展。
**
** Format of sqlite_stat1:
** sqlite_stat1的格式：
**
** There is normally one row per index, with the index identified by the
** name in the idx column.  The tbl column is the name of the table to
** which the index belongs.  In each such row, the stat column will be
** a string consisting of a list of integers.  The first integer in this
** list is the number of rows in the index and in the table.  The second
** integer is the average number of rows in the index that have the same
** value in the first column of the index.  The third integer is the average
** number of rows in the index that have the same value for the first two
** columns.  The N-th integer (for N>1) is the average number of rows in 
** the index which have the same value for the first N-1 columns.  For
** a K-column index, there will be K+1 integers in the stat column.  If
** the index is unique, then the last integer will be 1.
**
** 通常，每个索引一行，索引由idx列中的名字识别。tb1列是索引所属表的名字，这样的每一行，
** stat列是由一个整数列表组成的字符串。这个列表中的第一个整数是在这个索引和表中的行数。
** 第二个整数是索引中的行数的平均值，这个索引与第一列的索引有相同的值。第三个整数是索引中
** 的行数的平均值，这个索引与前两列的索引有相同的值。第N-th个整数(N>1)是索引中行数的平均值，
** 这个索引与前N-1列的索引有相同的值。对于K-column索引，在stat列中将有K+1个整数。如果
** 这个索引是唯一的，则最后一个整数将为1。
**
** The list of integers in the stat column can optionally be followed
** by the keyword "unordered".  The "unordered" keyword, if it is present,
** must be separated from the last integer by a single space.  If the
** "unordered" keyword is present, then the query planner assumes that
** the index is unordered and will not use the index for a range query.
**
** stat列中的整数列表可以附加"unordered"关键字。"unordered"关键字如果存在，必须通过
** 一个空格，与最后一个整数分隔开。如果"unordered"关键字存在，那么假定这个索引是无序的，且
** 不会将此索引用于范围查询。
** 
** If the sqlite_stat1.idx column is NULL, then the sqlite_stat1.stat
** column contains a single integer which is the (estimated) number of
** rows in the table identified by sqlite_stat1.tbl.
**
** 如果sqlite_stat1.idx列为空，那么sqlite_stat1.stat列包含一个整数，为sqlite_stat1.tb1
** 识别的行数估计值。
**
** Format of sqlite_stat2:
** sqlite_stat2的格式：
**
** The sqlite_stat2 is only created and is only used if SQLite is compiled
** with SQLITE_ENABLE_STAT2 and if the SQLite version number is between
** 3.6.18 and 3.7.8.  The "stat2" table contains additional information
** about the distribution of keys within an index.  The index is identified by
** the "idx" column and the "tbl" column is the name of the table to which
** the index belongs.  There are usually 10 rows in the sqlite_stat2
** table for each index.
**
** 如果SQLite由SQLITE_ENABLE_STAT2编译，且SQLite版本介于3.6.18与3.7.8之间，
** sqlite_stat2表才会被创建并使用。stat2表包含索引中关键字分配的额外信息。索引由idx列识别，
** tb1列是索引所属表的名字。通常，每个索引在sqlite_stat2表中有10行。
**
** The sqlite_stat2 entries for an index that have sampleno between 0 and 9
** inclusive are samples of the left-most key value in the index taken at
** evenly spaced points along the index.  Let the number of samples be S
** (10 in the standard build) and let C be the number of rows in the index.
** Then the sampled rows are given by:
**
**     rownumber = (i*C*2 + C)/(S*2)
**
** For i between 0 and S-1.  Conceptually, the index space is divided into
** S uniform buckets and the samples are the middle row from each bucket.
**
** The format for sqlite_stat2 is recorded here for legacy reference.  This
** version of SQLite does not support sqlite_stat2.  It neither reads nor
** writes the sqlite_stat2 table.  This version of SQLite only supports sqlite_stat3.
**
** sqlite_stat2的格式在这里记录，用于参考。这个SQLite的版本不支持sqlite_stat2。
** 不会读写sqlite_stat2表。这个版本只支持sqlite-stat3.
**
** Format for sqlite_stat3:
** sqlite_stat3的格式：
**
** The sqlite_stat3 is an enhancement to sqlite_stat2.  A new name is
** used to avoid compatibility problems.  
**
** sqlite_stat3是对sqlite_stat2的加强，使用新的名字用于避免兼容问题。
**
** The format of the sqlite_stat3 table is similar to the format of
** the sqlite_stat2 table.  There are multiple entries for each index.
** The idx column names the index and the tbl column is the table of the
** index.  If the idx and tbl columns are the same, then the sample is
** of the INTEGER PRIMARY KEY.  The sample column is a value taken from
** the left-most column of the index.  The nEq column is the approximate
** number of entires in the index whose left-most column exactly matches
** the sample.  nLt is the approximate number of entires whose left-most
** column is less than the sample.  The nDLt column is the approximate
** number of distinct left-most entries in the index that are less than
** the sample.
**
** sqlite_stat3表的格式类似于sqlite_stat2表的格式。每个索引有多个条目。idx列是索引名，
** tb1列是索引的表。如果idx列与tb1列相同，那么sample是INTEGER PRIMARY KEY。sample列
** 是取自索引的最左边列的值。nEq列是索引中的大约条目数，它的最左边列匹配sample。nLt列是
** 大约条目数，它的最左边列小于sample。nDLt列是索引中不同的最左条目数的大约值，它小于sample。
**
** Future versions of SQLite might change to store a string containing
** multiple integers values in the nDLt column of sqlite_stat3.  The first
** integer will be the number of prior index entires that are distinct in
** the left-most column.  The second integer will be the number of prior index
** entries that are distinct in the first two columns.  The third integer
** will be the number of prior index entries that are distinct in the first
** three columns.  And so forth.  With that extension, the nDLt field is
** similar in function to the sqlite_stat1.stat field.
**
** There can be an arbitrary number of sqlite_stat3 entries per index.
** The ANALYZE command will typically generate sqlite_stat3 tables
** that contain between 10 and 40 samples which are distributed across
** the key space, though not uniformly, and which include samples with
** largest possible nEq values.
*/
#ifndef SQLITE_OMIT_ANALYZE
#include "sqliteInt.h"

/*
** This routine generates code that opens the sqlite_stat1 table for
** writing with cursor iStatCur. If the library was built with the
** SQLITE_ENABLE_STAT3 macro defined, then the sqlite_stat3 table is
** opened for writing using cursor (iStatCur+1)
**
** 该函数的是用于打开 sqlite_stat1 表，在iStatCur游标位置进行写操作，如果库中
** 有SQLITE_ENABLE_STAT3的宏定义，那么sqlite_stat3 表将被打开从iStatCur+1位置开始写。
**
** If the sqlite_stat1 tables does not previously exist, it is created.
** Similarly, if the sqlite_stat3 table does not exist and the library
** is compiled with SQLITE_ENABLE_STAT3 defined, it is created. 
** 
** 如果sqlite_stat1表之前不存在并且库中以SQLITE_ENABLE_STAT3宏定义编译的，那么该表被创建。
** 
** Argument zWhere may be a pointer to a buffer containing a table name,
** or it may be a NULL pointer. If it is not NULL, then all entries in
** the sqlite_stat1 and (if applicable) sqlite_stat3 tables associated
** with the named table are deleted. If zWhere==0, then code is generated
** to delete all stat table entries.
** 
** 参数zWhere可能是一个指向包含一个表名的缓存的指针，或者是一个空指针，如果不为空，那么
** 所有在表sqlite_stat1和sqlite_stat3之中相关联的表的条目将被删除。如果zWhere==0,那么将
** 删除所有stat表中的条目。
*/
static void openStatTable(
  Parse *pParse,          /* Parsing context */ /*解析上下文*/
  int iDb,                /* The database we are looking in */ /*操作的数据库*/
  int iStatCur,           /* Open the sqlite_stat1 table on this cursor */ /*打开sqlite_stat1表，游标停留在iStatCur*/
  const char *zWhere,     /* Delete entries for this table or index*/ /* 删除这个表或索引的条目*/
  const char *zWhereType  /* Either "tbl" or "idx" */ /*类型是"tbl" 或者 "idx"*/
){
  static const struct {
    const char *zName;  /*表的名字*/
    const char *zCols;  /*表中列编号*/
  } aTable[] = {
    { "sqlite_stat1", "tbl,idx,stat" },
#ifdef SQLITE_ENABLE_STAT3
    { "sqlite_stat3", "tbl,idx,neq,nlt,ndlt,sample" },
#endif
  };

  int aRoot[] = {0, 0};
  u8 aCreateTbl[] = {0, 0};

  int i;
  sqlite3 *db = pParse->db;  /*定义数据库句柄*/
  Db *pDb;  /*表示数据库*/
  Vdbe *v = sqlite3GetVdbe(pParse);  /*建立的虚拟机*/
  if( v==0 ) return;
  assert( sqlite3BtreeHoldsAllMutexes(db) );//互斥判断
  assert( sqlite3VdbeDb(v)==db );  /*sqlite3Vdbe方法 返回与该vdbe相关联的数据库*/
  pDb = &db->aDb[iDb];  /*aDb表示所有后端*/

  /* Create new statistic tables if they do not exist, or clear them
  ** if they do already exist.
  ** 若这些表不存在，则新建表；否则，若这些表存在，则清空它们。
  */
  for(i=0; i<ArraySize(aTable); i++){
    const char *zTab = aTable[i].zName; 
    Table *pStat;
	/*sqlite3FindTable方法 定位描述一个特定的数据库表的内存结构，给出这个特殊的表的名字和（可选）包含这个表的数据库的名称，如果没有找到返回NULL。 出自build.c*/
    if( (pStat = sqlite3FindTable(db, zTab, pDb->zName))==0 ){
      /* The sqlite_stat[12] table does not exist. Create it. Note that a 
      ** side-effect of the CREATE TABLE statement is to leave the rootpage 
      ** of the new table in register pParse->regRoot. This is important 
      ** because the OpenWrite opcode below will be needing it. 
      ** 
      ** 表sqlite_stat1或sqlite_stat2不存在，就创建。注意的是CREATE TABLE语句的
      ** 副作用，即当离开注册新表的根页的时候pParse->regRoot，这点很重要因为之后的
      ** 打开写操作会需要它。
      */
      sqlite3NestedParse(pParse,   /*递归运行解析器和代码生成器是为了生成给定SQL语句的代码，用于终止目前正在构造的pParse上下文。出自build.c*/
          "CREATE TABLE %Q.%s(%s)", pDb->zName, zTab, aTable[i].zCols
      );  
      aRoot[i] = pParse->regRoot;  /*regRoot表示存储新对象的根页码的寄存器*/
      aCreateTbl[i] = OPFLAG_P2ISREG;
    }else{
      /* The table already exists. If zWhere is not NULL, delete all entries 
      ** associated with the table zWhere. If zWhere is NULL, delete the
      ** entire contents of the table. 
      **
      ** 这个表已经存在。如果zWhere不为空，删除所有与表中zWhere相关联的条目。如果zWhere
      ** 为空，那么删除表中的所有条目。*/

      aRoot[i] = pStat->tnum;  /*tnum表示表的B树根节点*/
      sqlite3TableLock(pParse, iDb, aRoot[i], 1, zTab);//表上锁 /*sqlite3TableLock方法 记录信息，在运行时间我们想锁住一个表*/
      if( zWhere ){
        sqlite3NestedParse(pParse,
           "DELETE FROM %Q.%s WHERE %s=%Q", pDb->zName, zTab, zWhereType, zWhere
        );
      }else{
        /* The sqlite_stat[12] table already exists.  Delete all rows. 
        ** 如果表sqlite_stat1或sqlite_stat2已经存在，删除所有的行。
		*/
        sqlite3VdbeAddOp2(v, OP_Clear, aRoot[i], iDb);
      }
    }
  }

  /* Open the sqlite_stat[13] tables for writing. 
  ** 打开表sqlite_stat1和表sqlite_stat3 去写
  */
  for(i=0; i<ArraySize(aTable); i++){
    sqlite3VdbeAddOp3(v, OP_OpenWrite, iStatCur+i, aRoot[i], iDb);
    sqlite3VdbeChangeP4(v, -1, (char *)3, P4_INT32);
    sqlite3VdbeChangeP5(v, aCreateTbl[i]);
  }
}

/*
** Recommended number of samples for sqlite_stat3
** 荐sqlite_stat3的采样数
*/
#ifndef SQLITE_STAT3_SAMPLES
# define SQLITE_STAT3_SAMPLES 24
#endif

/*
** Three SQL functions - stat3_init(), stat3_push(), and stat3_pop() -
** share an instance of the following structure to hold their state
** information.
**
** 三个SQL函数-stat3_init(), stat3_push(), 和 stat3_pop()，分享一个如下结构体的
** 实例来保存它们的状态信息。
*/
typedef struct Stat3Accum Stat3Accum;
struct Stat3Accum {
  tRowcnt nRow;             /* Number of rows in the entire table */ /*整个表的行数*/
  tRowcnt nPSample;         /* How often to do a periodic sample */ /*多久做一次定期抽样*/
  int iMin;                 /* Index of entry with minimum nEq and hash */ /*最小nEq和hash条目的索引*/
  int mxSample;             /* Maximum number of samples to accumulate */ /*样本累计的最大数目*/
  int nSample;              /* Current number of samples */ /*当前的样本数目*/
  u32 iPrn;                 /* Pseudo-random number used for sampling */ /*用于抽样的随机数*/  /*u32表示4位无符号整数*/
  struct Stat3Sample {  /*定义stat3表*/
    i64 iRowid;                /* Rowid in main table of the key */ /*主表中关键字的ROWID*/  /*i64表示8位有符号整数*/
    tRowcnt nEq;               /* sqlite_stat3.nEq */  
    tRowcnt nLt;               /* sqlite_stat3.nLt */
    tRowcnt nDLt;              /* sqlite_stat3.nDLt */
    u8 isPSample;              /* True if a periodic sample */  /*如果是定期样本，则为true*/  /*u8表示1位无符号整数*/
    u32 iHash;                 /* Tiebreaker hash */  
  } *a;                     /* An array of samples */  /*样本数组*/
};

#ifdef SQLITE_ENABLE_STAT3
/*
** Implementation of the stat3_init(C,S) SQL function.  The two parameters
** are the number of rows in the table or index (C) and the number of samples
** to accumulate (S).
**
** SQL函数stat3_init(C,S)的实现。这两个参数分别是在表或者索引中的行数和
** 累计的样本数。 
** This routine allocates the Stat3Accum object.
** 
** 这个过程分配并初始化Stat3Accum 对象的每一个属性。
** 
** The return value is the Stat3Accum object (P).
**
** 返回值是 Stat3Accum 对象.
*/
static void stat3Init(
  sqlite3_context *context,  /*定义上下文*/
  int argc,
  sqlite3_value **argv
){
  Stat3Accum *p;  //定义结构体指针
  tRowcnt nRow;  /*宏定义：typedef u32 tRowcnt， 32-bit is the default 32位是默认的  */
  int mxSample;
  int n;

  UNUSED_PARAMETER(argc);  /* UNUSED_PARAMETER宏被用来抑制编译器警告，*出自sqliteInt.h 631行 */
  nRow = (tRowcnt)sqlite3_value_int64(argv[0]);
  mxSample = sqlite3_value_int(argv[1]);
  n = sizeof(*p) + sizeof(p->a[0])*mxSample;
  p = sqlite3MallocZero( n );  /*分配0内存*/
  if( p==0 ){
    sqlite3_result_error_nomem(context);
    return;
  }
  //初始化每一个变量
  p->a = (struct Stat3Sample*)&p[1];
  p->nRow = nRow;
  p->mxSample = mxSample;
  p->nPSample = p->nRow/(mxSample/3+1) + 1;
  sqlite3_randomness(sizeof(p->iPrn), &p->iPrn);
  sqlite3_result_blob(context, p, sizeof(p), sqlite3_free);
}
static const FuncDef stat3InitFuncdef = {  /*FuncDef为一结构体，每个SQL函数由该结构体的一个实例来定义。*/
  2,                /* nArg */
  SQLITE_UTF8,      /* iPrefEnc */
  0,                /* flags */
  0,                /* pUserData */
  0,                /* pNext */
  stat3Init,        /* xFunc */
  0,                /* xStep */
  0,                /* xFinalize */
  "stat3_init",     /* zName */
  0,                /* pHash */
  0                 /* pDestructor */
};


/*
** Implementation of the stat3_push(nEq,nLt,nDLt,rowid,P) SQL function.  The
** arguments describe a single key instance.  This routine makes the 
** decision about whether or not to retain this key for the sqlite_stat3
** table.
**
** The return value is NULL.
**
** SQL函数stat3_push(nEq,nLt,nDLt,rowid,P)的实现。 这些参数描述了一个关键实例。
** 这个过程做出决定，关于是否保留sqlite_stat3表的关键字。
**
** 返回值为空。
*/
static void stat3Push(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  Stat3Accum *p = (Stat3Accum*)sqlite3_value_blob(argv[4]);
  tRowcnt nEq = sqlite3_value_int64(argv[0]);  /*宏定义：typedef u32 tRowcnt， 32-bit is the default 32位是默认的  */
  tRowcnt nLt = sqlite3_value_int64(argv[1]);
  tRowcnt nDLt = sqlite3_value_int64(argv[2]);
  i64 rowid = sqlite3_value_int64(argv[3]);  /*i64表示8位有符号整数*/
  u8 isPSample = 0;  /*u8表示1位无符号整数*/
  u8 doInsert = 0;
  int iMin = p->iMin;
  struct Stat3Sample *pSample;
  int i;
  u32 h;

  UNUSED_PARAMETER(context);  /* UNUSED_PARAMETER宏被用来抑制编译器警告，*出自sqliteInt.h 631行 */
  UNUSED_PARAMETER(argc);
  if( nEq==0 ) return;
  h = p->iPrn = p->iPrn*1103515245 + 12345;
  if( (nLt/p->nPSample)!=((nEq+nLt)/p->nPSample) ){
    doInsert = isPSample = 1;
  }else if( p->nSample<p->mxSample ){
    doInsert = 1;
  }else{
    if( nEq>p->a[iMin].nEq || (nEq==p->a[iMin].nEq && h>p->a[iMin].iHash) ){
      doInsert = 1;
    }
  }
  if( !doInsert ) return;
  if( p->nSample==p->mxSample ){
    assert( p->nSample - iMin - 1 >= 0 );
    memmove(&p->a[iMin], &p->a[iMin+1], sizeof(p->a[0])*(p->nSample-iMin-1));
    pSample = &p->a[p->nSample-1];
  }else{
    pSample = &p->a[p->nSample++];
  }
  pSample->iRowid = rowid;
  pSample->nEq = nEq;
  pSample->nLt = nLt;
  pSample->nDLt = nDLt;
  pSample->iHash = h;
  pSample->isPSample = isPSample;

  /* Find the new minimum 
  ** 找到新的最小值
  */
  if( p->nSample==p->mxSample ){
    pSample = p->a;
    i = 0;
    while( pSample->isPSample ){
      i++;
      pSample++;
      assert( i<p->nSample );
    }
    nEq = pSample->nEq;
    h = pSample->iHash;
    iMin = i;
    for(i++, pSample++; i<p->nSample; i++, pSample++){
      if( pSample->isPSample ) continue;
      if( pSample->nEq<nEq
       || (pSample->nEq==nEq && pSample->iHash<h)
      ){
        iMin = i;
        nEq = pSample->nEq;
        h = pSample->iHash;
      }
    }
    p->iMin = iMin;
  }
}
static const FuncDef stat3PushFuncdef = {
  5,                /* nArg */
  SQLITE_UTF8,      /* iPrefEnc */
  0,                /* flags */
  0,                /* pUserData */
  0,                /* pNext */
  stat3Push,        /* xFunc */
  0,                /* xStep */
  0,                /* xFinalize */
  "stat3_push",     /* zName */
  0,                /* pHash */
  0                 /* pDestructor */
};

/*
** Implementation of the stat3_get(P,N,...) SQL function.  This routine is
** used to query the results.  Content is returned for the Nth sqlite_stat3
** row where N is between 0 and S-1 and S is the number of samples.  The
** value returned depends on the number of arguments.
**
** SQL函数stat3_get(P,N,...)的实现。这个过程用于查询结果。返回的是sqlite_stat3的第N行
** N是在0 到 S-1之间，s是样本数。返回的值取决于的是参数的个数。 
**   argc==2    result:  rowid
**   argc==3    result:  nEq
**   argc==4    result:  nLt
**   argc==5    result:  nDLt
*/
static void stat3Get(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  int n = sqlite3_value_int(argv[1]);
  Stat3Accum *p = (Stat3Accum*)sqlite3_value_blob(argv[0]);

  assert( p!=0 );
  if( p->nSample<=n ) return;
  //根据参数的不同，返回不同的值
  switch( argc ){
    case 2:  sqlite3_result_int64(context, p->a[n].iRowid); break;
    case 3:  sqlite3_result_int64(context, p->a[n].nEq);    break;
    case 4:  sqlite3_result_int64(context, p->a[n].nLt);    break;
    default: sqlite3_result_int64(context, p->a[n].nDLt);   break;
  }
}
static const FuncDef stat3GetFuncdef = {
  -1,               /* nArg */
  SQLITE_UTF8,      /* iPrefEnc */
  0,                /* flags */
  0,                /* pUserData */
  0,                /* pNext */
  stat3Get,         /* xFunc */
  0,                /* xStep */
  0,                /* xFinalize */
  "stat3_get",     /* zName */
  0,                /* pHash */
  0                 /* pDestructor */
};
#endif /* SQLITE_ENABLE_STAT3 */




/*
** Generate code to do an analysis of all indices associated with
** a single table.
** 
**对所关联的单一表的所有索引进行分析
*/
static void analyzeOneTable(
  Parse *pParse,   /* Parser context */  /* 解析器上下文 */
  Table *pTab,     /* Table whose indices are to be analyzed */ /* 要分析索引的表*/
  Index *pOnlyIdx, /* If not NULL, only analyze this one index */ /*如果非空, 只分析这一个索引*/
  int iStatCur,    /* Index of VdbeCursor that writes the sqlite_stat1 table */ /* VdbeCursor的索引，用于写sqlite_stat1 表 */
  int iMem         /* Available memory locations begin here */  /*可用内存起始位置 */
){
  sqlite3 *db = pParse->db;    /* Database handle */ /*数据库句柄 */
  Index *pIdx;                 /* An index to being analyzed */ /* 一个正在被分析的索引*/
  int iIdxCur;                 /* Cursor open on index being analyzed */ /* 正在被分析的索引上打开的下标*/
  Vdbe *v;                     /* The virtual machine being built up *//*建立的虚拟机 */
  int i;                       /* Loop counter */ /*循环计数 */
  int topOfLoop;               /* The top of the loop */  /* 循环的开始 */
  int endOfLoop;               /* The end of the loop */  /* 循环的结束 */
  int jZeroRows = -1;          /* Jump from here if number of rows is zero */ /* 如果行数为0从此跳转*/
  int iDb;                     /* Index of database containing pTab */ /* 包含要分析表的数据库的索引*/
  int regTabname = iMem++;     /* Register containing table name *//* 包含表名的寄存器 */
  int regIdxname = iMem++;     /* Register containing index name *//* 包含索引名的寄存器 */
  int regStat1 = iMem++;       /* The stat column of sqlite_stat1 *//* sqlite_stat1表的stat列*/
#ifdef SQLITE_ENABLE_STAT3
  int regNumEq = regStat1;     /* Number of instances.  Same as regStat1 */ /*实例的数量，类似Stat列regStat1*/
  int regNumLt = iMem++;       /* Number of keys less than regSample */ /*小于实例的关键字数目*/ 
  int regNumDLt = iMem++;      /* Number of distinct keys less than regSample */ /*小于实例的不同关键字的数目*/
  int regSample = iMem++;      /* The next sample value */ /*下一个实例的值*/
  int regRowid = regSample;    /* Rowid of a sample */ /*样例表的Rowid*/
  int regAccum = iMem++;       /* Register to hold Stat3Accum object */ /*保存Stat3Accum对象的寄存器*/
  int regLoop = iMem++;        /* Loop counter */ /*循环计数器*/
  int regCount = iMem++;       /* Number of rows in the table or index */ /*表或索引中的行数*/
  int regTemp1 = iMem++;       /* Intermediate register */ /*中间寄存器*/
  int regTemp2 = iMem++;       /* Intermediate register */ /*中间寄存器*/
  int once = 1;                /* One-time initialization */ /*一次性初始化*/
  int shortJump = 0;           /* Instruction address */ /*指令地址*/
  int iTabCur = pParse->nTab++; /* Table cursor */ /*表的游标*/
#endif
  int regCol = iMem++;         /* Content of a column in analyzed table *//* 被分析的表中一列的内容 */
  int regRec = iMem++;         /* Register holding completed record */ /* 持有完整记录的记录器 */
  int regTemp = iMem++;        /* Temporary use register *//* 临时用到的记录器*/
  int regNewRowid = iMem++;    /* Rowid for the inserted record */ /* 插入记录的rowid*/


  v = sqlite3GetVdbe(pParse);  /*获得虚拟机*/
  if( v==0 || NEVER(pTab==0) ){  /*没有获得虚拟机或没有表*/
    return;
  }
  if( pTab->tnum==0 ){   /*tnum表示 表的B树的根节点*/
    /* Do not gather statistics on views or virtual tables 
	** 不收集视图或虚拟表的统计信息。
	*/
    return;
  }
  if( memcmp(pTab->zName, "sqlite_", 7)==0 ){  /*判定以 sqlite_ 匹配的表名*/
    /* Do not gather statistics on system tables 
	** 不收集系统表的统计信息
	*/
    return;
  }
  assert( sqlite3BtreeHoldsAllMutexes(db) );  /*判断 B树拥有所有互斥*/
  iDb = sqlite3SchemaToIndex(db, pTab->pSchema);  /*将模式指针转换为数据库索引，该索引指出 在db->aDb[]中模式指向的数据库文件*/
  assert( iDb>=0 );  /*判断 存在数据库*/
  assert( sqlite3SchemaMutexHeld(db, iDb, 0) );  /*判断 模式拥有互斥 */
#ifndef SQLITE_OMIT_AUTHORIZATION
  if( sqlite3AuthCheck(pParse, SQLITE_ANALYZE, pTab->zName, 0,
      db->aDb[iDb].zName ) ){   /*做一个使用给定代码和参数的授权检查*/
    return;
  }
#endif

  /* Establish a read-lock on the table at the shared-cache level. 
  ** 在共享cache等级上的表上建立读锁。
  */
  sqlite3TableLock(pParse, iDb, pTab->tnum, 0, pTab->zName);

  iIdxCur = pParse->nTab++;   /*nTab表示事先分配的VDBE光标的数量*/
  sqlite3VdbeAddOp4(v, OP_String8, 0, regTabname, 0, pTab->zName, 0);  /*添加一个包含p4值作为指针的操作码*/
  for(pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext){  /*pIndex表示该表的SQL索引列表，pNext表示相关联的同一个表的下一个索引*/
    int nCol;  /*定义表的列编号*/
    KeyInfo *pKey;    /*KeyInfo结构体，定义索引关键字*/
    int addrIfNot = 0;           /* address of OP_IfNot */ /*OP_IfNot的地址*/
    int *aChngAddr;              /* Array of jump instruction addresses */  /*跳转指令地址的数组*/

    if( pOnlyIdx && pOnlyIdx!=pIdx ) continue;
    VdbeNoopComment((v, "Begin analysis of %s", pIdx->zName));  /*提示开始分析pIdx指定表名的表*/
    nCol = pIdx->nColumn;    /*nColumn表示通过该索引使用的表的列数*/
    aChngAddr = sqlite3DbMallocRaw(db, sizeof(int)*nCol);  /*调用sqlite3DbMallocRaw方法，分配内存。出自malloc.c*/
    if( aChngAddr==0 ) continue;
	/*调用sqlite3IndexKeyinfo方法，返回一个动态分配的密钥信息的结构，可以用于OP_OpenRead或OP_OpenWrite访问数据库索引pIdx。出自build.c*/
    pKey = sqlite3IndexKeyinfo(pParse, pIdx);  
    if( iMem+1+(nCol*2)>pParse->nMem ){  /*nMem表示到目前为止使用的内存单元的数量*/
      pParse->nMem = iMem+1+(nCol*2);
    }

    /* Open a cursor to the index to be analyzed. 
    ** 打开将被分析的索引的游标
	*/
    assert( iDb==sqlite3SchemaToIndex(db, pIdx->pSchema) );
    sqlite3VdbeAddOp4(v, OP_OpenRead, iIdxCur, pIdx->tnum, iDb,
        (char *)pKey, P4_KEYINFO_HANDOFF);
    VdbeComment((v, "%s", pIdx->zName));

    /* Populate the register containing the index name. 
	** 移植包含该索引名字的寄存器
	*/
    sqlite3VdbeAddOp4(v, OP_String8, 0, regIdxname, 0, pIdx->zName, 0);

#ifdef SQLITE_ENABLE_STAT3  /*定义宏*/
    if( once ){
      once = 0;
      sqlite3OpenTable(pParse, iTabCur, iDb, pTab, OP_OpenRead);  /*根据游标iTabCur、数据库索引iDb、表定义pTab，打开表*/
    }
    sqlite3VdbeAddOp2(v, OP_Count, iIdxCur, regCount);  
    sqlite3VdbeAddOp2(v, OP_Integer, SQLITE_STAT3_SAMPLES, regTemp1);  /*SQLITE_STAT3_SAMPLES表示sqlite_stat3的采样数*/
    sqlite3VdbeAddOp2(v, OP_Integer, 0, regNumEq);
    sqlite3VdbeAddOp2(v, OP_Integer, 0, regNumLt);
    sqlite3VdbeAddOp2(v, OP_Integer, -1, regNumDLt);
    sqlite3VdbeAddOp3(v, OP_Null, 0, regSample, regAccum);  /*在VDBE中添加一个新的指令到当前列表中，返回新的指令的地址。出自vdbeaux.c*/
    sqlite3VdbeAddOp4(v, OP_Function, 1, regCount, regAccum,
                      (char*)&stat3InitFuncdef, P4_FUNCDEF);
    sqlite3VdbeChangeP5(v, 2);  /*为最近添加的操作更改操作数 P5 的值*/
#endif /* SQLITE_ENABLE_STAT3 */

    /* The block of memory cells initialized here is used as follows.
    **
    **    iMem:                
    **        The total number of rows in the table.
    **
    **    iMem+1 .. iMem+nCol: 
    **        Number of distinct entries in index considering the 
    **        left-most N columns only, where N is between 1 and nCol, 
    **        inclusive.
    **
    **    iMem+nCol+1 .. Mem+2*nCol:  
    **        Previous value of indexed columns, from left to right.
    **
    ** Cells iMem through iMem+nCol are initialized to 0. The others are 
    ** initialized to contain an SQL NULL.
    **
    ** 被初始化的内存块如下.
    **
    **    iMem:                
    **        表的总行数.
    **
    **    iMem+1 .. iMem+nCol: 
    **        索引中不同的条目数只考虑最左边的的N列,N 在 1 到 nCol之间。
    **
    **    iMem+nCol+1 .. Mem+2*nCol:  
    **        被索引的列之前的值, 从左到右.
    **
    ** 单元 iMem 到 iMem+nCol 被初始化为 0. 其他被初始化为 
    ** 包含一个空的 SQL.
    */
    for(i=0; i<=nCol; i++){
      sqlite3VdbeAddOp2(v, OP_Integer, 0, iMem+i);
    }
    for(i=0; i<nCol; i++){
      sqlite3VdbeAddOp2(v, OP_Null, 0, iMem+nCol+i+1);
    }

    /* Start the analysis loop. This loop runs through all the entries in
    ** the index b-tree.  
    ** 开始循环分析. 这个循环运行了在索引 b-树中的所有条目
	*/
    endOfLoop = sqlite3VdbeMakeLabel(v);  /*创建一个还没有被编码的指令的新符号标签，这个符号标签仅表示一个负数。出自vdbeaux.c*/
    sqlite3VdbeAddOp2(v, OP_Rewind, iIdxCur, endOfLoop);
    topOfLoop = sqlite3VdbeCurrentAddr(v);  /*返回插入下一条指令的地址。*/
    sqlite3VdbeAddOp2(v, OP_AddImm, iMem, 1);  /* 行递增计数器 */

    for(i=0; i<nCol; i++){
      CollSeq *pColl;  /*定义一个排序序列*/
      sqlite3VdbeAddOp3(v, OP_Column, iIdxCur, i, regCol);
      if( i==0 ){
        /* Always record the very first row */
        /* 总是记录第一行*/
        addrIfNot = sqlite3VdbeAddOp1(v, OP_IfNot, iMem+1);
      }
      assert( pIdx->azColl!=0 );  /*azColl表示以索引名字进行排序的排序序列数组*/
      assert( pIdx->azColl[i]!=0 );
      pColl = sqlite3LocateCollSeq(pParse, pIdx->azColl[i]);  /*返回数据库本地文件编码的排序序列*/
      aChngAddr[i] = sqlite3VdbeAddOp4(v, OP_Ne, regCol, 0, iMem+nCol+i+1,
                                      (char*)pColl, P4_COLLSEQ);
      sqlite3VdbeChangeP5(v, SQLITE_NULLEQ);  /*定义SQLITE_NULLEQ，NULL=NULL*/
      VdbeComment((v, "jump if column %d changed", i));  /*提示*/
#ifdef SQLITE_ENABLE_STAT3
      if( i==0 ){
        sqlite3VdbeAddOp2(v, OP_AddImm, regNumEq, 1);
        VdbeComment((v, "incr repeat count"));
      }
#endif
    }
    sqlite3VdbeAddOp2(v, OP_Goto, 0, endOfLoop);
    for(i=0; i<nCol; i++){
      sqlite3VdbeJumpHere(v, aChngAddr[i]);  /* Set jump dest for the OP_Ne */ /*为OP_Ne设置跳转的目的地*/
      if( i==0 ){
        sqlite3VdbeJumpHere(v, addrIfNot);   /* Jump dest for OP_IfNot */ /*为OP_IfNot跳转目的地*/
#ifdef SQLITE_ENABLE_STAT3
        sqlite3VdbeAddOp4(v, OP_Function, 1, regNumEq, regTemp2,
                          (char*)&stat3PushFuncdef, P4_FUNCDEF);
        sqlite3VdbeChangeP5(v, 5);
        sqlite3VdbeAddOp3(v, OP_Column, iIdxCur, pIdx->nColumn, regRowid);
        sqlite3VdbeAddOp3(v, OP_Add, regNumEq, regNumLt, regNumLt);
        sqlite3VdbeAddOp2(v, OP_AddImm, regNumDLt, 1);
        sqlite3VdbeAddOp2(v, OP_Integer, 1, regNumEq);
#endif        
      }
      sqlite3VdbeAddOp2(v, OP_AddImm, iMem+i+1, 1);
      sqlite3VdbeAddOp3(v, OP_Column, iIdxCur, i, iMem+nCol+i+1);
    }
    sqlite3DbFree(db, aChngAddr);  /*释放数据库连接关联的内存*/

    /* Always jump here after updating the iMem+1...iMem+1+nCol counters 
    ** 当更新完iMem+1...iMem+1+nCol记录之后总是跳转到此
	*/
    sqlite3VdbeResolveLabel(v, endOfLoop);  /*释放标签“endOfLoop”的地址给将要插入的下一条指令，endOfLoop必须从之前调用的函数sqlite3VdbeMakeLabel()中获得。*/

    sqlite3VdbeAddOp2(v, OP_Next, iIdxCur, topOfLoop);
    sqlite3VdbeAddOp1(v, OP_Close, iIdxCur);
#ifdef SQLITE_ENABLE_STAT3
    sqlite3VdbeAddOp4(v, OP_Function, 1, regNumEq, regTemp2,
                      (char*)&stat3PushFuncdef, P4_FUNCDEF);
    sqlite3VdbeChangeP5(v, 5);
    sqlite3VdbeAddOp2(v, OP_Integer, -1, regLoop);
    shortJump = 
    sqlite3VdbeAddOp2(v, OP_AddImm, regLoop, 1);
    sqlite3VdbeAddOp4(v, OP_Function, 1, regAccum, regTemp1,
                      (char*)&stat3GetFuncdef, P4_FUNCDEF);
    sqlite3VdbeChangeP5(v, 2);
    sqlite3VdbeAddOp1(v, OP_IsNull, regTemp1);
    sqlite3VdbeAddOp3(v, OP_NotExists, iTabCur, shortJump, regTemp1);
    sqlite3VdbeAddOp3(v, OP_Column, iTabCur, pIdx->aiColumn[0], regSample);
    sqlite3ColumnDefault(v, pTab, pIdx->aiColumn[0], regSample);
    sqlite3VdbeAddOp4(v, OP_Function, 1, regAccum, regNumEq,
                      (char*)&stat3GetFuncdef, P4_FUNCDEF);
    sqlite3VdbeChangeP5(v, 3);
    sqlite3VdbeAddOp4(v, OP_Function, 1, regAccum, regNumLt,
                      (char*)&stat3GetFuncdef, P4_FUNCDEF);
    sqlite3VdbeChangeP5(v, 4);
    sqlite3VdbeAddOp4(v, OP_Function, 1, regAccum, regNumDLt,
                      (char*)&stat3GetFuncdef, P4_FUNCDEF);
    sqlite3VdbeChangeP5(v, 5);
    sqlite3VdbeAddOp4(v, OP_MakeRecord, regTabname, 6, regRec, "bbbbbb", 0);
    sqlite3VdbeAddOp2(v, OP_NewRowid, iStatCur+1, regNewRowid);
    sqlite3VdbeAddOp3(v, OP_Insert, iStatCur+1, regRec, regNewRowid);
    sqlite3VdbeAddOp2(v, OP_Goto, 0, shortJump);
    sqlite3VdbeJumpHere(v, shortJump+2);
#endif        

    /* Store the results in sqlite_stat1.
    **
    ** The result is a single row of the sqlite_stat1 table.  The first
    ** two columns are the names of the table and index.  The third column
    ** is a string composed of a list of integer statistics about the
    ** index.  The first integer in the list is the total number of entries
    ** in the index.  There is one additional integer in the list for each
    ** column of the table.  This additional integer is a guess of how many
    ** rows of the table the index will select.  If D is the count of distinct
    ** values and K is the total number of rows, then the integer is computed
    ** as:
    **
    **        I = (K+D-1)/D
    **
    ** If K==0 then no entry is made into the sqlite_stat1 table.  
    ** If K>0 then it is always the case the D>0 so division by zero
    ** is never possible.
    **
    ** 将结果存在 sqlite_stat1 表中.
    **
    ** 结果是 sqlite_stat1 表的一行.  前两列是表和索引的名字。
    ** 第三列是一个包含一系列关于索引的整形数据的字符串。
    ** 在其中第一个整数是在索引中条目的总数，一个附加的整数针对表中的每一列，
    ** 这个附加整数是对表中有多少行会被索引选择的猜测，如果D是不同值的个数，k是总行数，那么这个整数
    ** 可以计算为：
    **        I = (K+D-1)/D
    **
    ** 如果k == 0 那么在 sqlite_stat1 表中没有条目.  
    ** 如果k > 0 将总是有这种情况  D>0 因此被0除就不能。
    */
    sqlite3VdbeAddOp2(v, OP_SCopy, iMem, regStat1);
    if( jZeroRows<0 ){
      jZeroRows = sqlite3VdbeAddOp1(v, OP_IfNot, iMem);
    }
    for(i=0; i<nCol; i++){
      sqlite3VdbeAddOp4(v, OP_String8, 0, regTemp, 0, " ", 0);
      sqlite3VdbeAddOp3(v, OP_Concat, regTemp, regStat1, regStat1);
      sqlite3VdbeAddOp3(v, OP_Add, iMem, iMem+i+1, regTemp);
      sqlite3VdbeAddOp2(v, OP_AddImm, regTemp, -1);
      sqlite3VdbeAddOp3(v, OP_Divide, iMem+i+1, regTemp, regTemp);
      sqlite3VdbeAddOp1(v, OP_ToInt, regTemp);
      sqlite3VdbeAddOp3(v, OP_Concat, regTemp, regStat1, regStat1);
    }
    sqlite3VdbeAddOp4(v, OP_MakeRecord, regTabname, 3, regRec, "aaa", 0);
    sqlite3VdbeAddOp2(v, OP_NewRowid, iStatCur, regNewRowid);
    sqlite3VdbeAddOp3(v, OP_Insert, iStatCur, regRec, regNewRowid);
    sqlite3VdbeChangeP5(v, OPFLAG_APPEND);
  }

  /* If the table has no indices, create a single sqlite_stat1 entry
  ** containing NULL as the index name and the row count as the content.
  **
  ** 如果表没有索引, 创建一个 包含NULL的 sqlite_stat1 条目作为索引名，并且行数作为内容.
  */
  if( pTab->pIndex==0 ){
    sqlite3VdbeAddOp3(v, OP_OpenRead, iIdxCur, pTab->tnum, iDb);
    VdbeComment((v, "%s", pTab->zName));
    sqlite3VdbeAddOp2(v, OP_Count, iIdxCur, regStat1);
    sqlite3VdbeAddOp1(v, OP_Close, iIdxCur);
    jZeroRows = sqlite3VdbeAddOp1(v, OP_IfNot, regStat1);
  }else{
    sqlite3VdbeJumpHere(v, jZeroRows);
    jZeroRows = sqlite3VdbeAddOp0(v, OP_Goto);
  }
  sqlite3VdbeAddOp2(v, OP_Null, 0, regIdxname);
  sqlite3VdbeAddOp4(v, OP_MakeRecord, regTabname, 3, regRec, "aaa", 0);
  sqlite3VdbeAddOp2(v, OP_NewRowid, iStatCur, regNewRowid);
  sqlite3VdbeAddOp3(v, OP_Insert, iStatCur, regRec, regNewRowid);
  sqlite3VdbeChangeP5(v, OPFLAG_APPEND);
  if( pParse->nMem<regRec ) pParse->nMem = regRec;
  sqlite3VdbeJumpHere(v, jZeroRows);  /*更改 P2 操作数的指令地址，以便它指向将要编码的下一条指令的地址。*/
}


/*
** Generate code that will cause the most recent index analysis to
** be loaded into internal hash tables where is can be used.
**
** 生成代码，使最近分析的索引被载入到可用的内部哈希表。
*/
static void loadAnalysis(Parse *pParse, int iDb){
  Vdbe *v = sqlite3GetVdbe(pParse);
  if( v ){
    sqlite3VdbeAddOp1(v, OP_LoadAnalysis, iDb);
  }
}

/*
** Generate code that will do an analysis of an entire database
** 该函数用于分析一个完整数据库
*/
static void analyzeDatabase(Parse *pParse, int iDb){
  sqlite3 *db = pParse->db;  /*数据库句柄 */
  Schema *pSchema = db->aDb[iDb].pSchema;    /* Schema of database iDb */ /*根据数据库索引，定义数据库模式*/
  HashElem *k;  /*定义一个哈希结构体*/
  int iStatCur;  /* VdbeCursor的索引，用于写sqlite_stat1 表 */
  int iMem;  /*定义可用内存的起始位置*/

  sqlite3BeginWriteOperation(pParse, 0, iDb);  /*开始写操作，指定数据库索引。该方法出自 build.c*/
  iStatCur = pParse->nTab;  /*nTab表示事先分配的VDBE光标的数量*/
  pParse->nTab += 3;  /* ？+3？ */
  openStatTable(pParse, iDb, iStatCur, 0, 0);  /*调用openStatTable方法，打开存储索引的表sqlite_stat1*/
  iMem = pParse->nMem+1;  /*nMem表示到目前为止使用的内存数量*/
  assert( sqlite3SchemaMutexHeld(db, iDb, 0) );  /*判定模式存在互斥*/

  /*for循环，对数据库中的每一个表进行分析*/
  /*哈希表宏定义：sqliteHashFirst、sqliteHashNext、sqliteHashData，用于遍历哈希表中的所有元素。*/
  for(k=sqliteHashFirst(&pSchema->tblHash); k; k=sqliteHashNext(k)){ 
    Table *pTab = (Table*)sqliteHashData(k);  /*得到要分析的表*/
    analyzeOneTable(pParse, pTab, 0, iStatCur, iMem);  /*调用analyzeOneTable方法，完成分析单一表的具体过程*/
  }
  loadAnalysis(pParse, iDb);  /*将最近分析的数据库的索引载入内部哈希表*/
}

/*
** Generate code that will do an analysis of a single table in
** a database.  If pOnlyIdx is not NULL then it is a single index
** in pTab that should be analyzed.
**
** 对数据库中的一个表进行分析，如果pOnlyIdx不为空，那么在pTab表中的
** 一个索引将被分析。
*/
static void analyzeTable(Parse *pParse, Table *pTab, Index *pOnlyIdx){
  int iDb;  /*数据库索引*/
  int iStatCur;  /* VdbeCursor的索引，用于写sqlite_stat1 表 */

  assert( pTab!=0 );  /*判定存在数据库表*/
  assert( sqlite3BtreeHoldsAllMutexes(pParse->db) );  /*互斥判断*/
  iDb = sqlite3SchemaToIndex(pParse->db, pTab->pSchema);  /*将模式指针转换为数据库索引，该索引指出 在db->aDb[]中模式指向的数据库文件*/
  sqlite3BeginWriteOperation(pParse, 0, iDb);  /*开始写操作*/
  iStatCur = pParse->nTab;  /*nTab表示事先分配的VDBE光标的数量*/
  pParse->nTab += 3;  /* ？+3？ */
  if( pOnlyIdx ){  /*pOnlyIdx是表的唯一索引*/
    openStatTable(pParse, iDb, iStatCur, pOnlyIdx->zName, "idx");  /*调用openStatTable方法，打开存储索引的表sqlite_stat1。类型是“idx”*/
  }else{
    openStatTable(pParse, iDb, iStatCur, pTab->zName, "tbl");  /*调用openStatTable方法，打开存储索引的表sqlite_stat1。类型是“tb1”*/
  }
  analyzeOneTable(pParse, pTab, pOnlyIdx, iStatCur, pParse->nMem+1);  /*调用analyzeOneTable方法，完成分析单一表的具体过程*/
  loadAnalysis(pParse, iDb);  /*将最近分析的数据库的索引载入内部哈希表*/
}

/*
** Generate code for the ANALYZE command.  The parser calls this routine
** when it recognizes an ANALYZE command.
**
** 生成ANALYZE命令的代码.  当解析器识别出 ANALYZE 命令时调用此过程。
** 根据Token的数据pName1、pName2，确定分析范围  
**        ANALYZE                            -- 1
**        ANALYZE  <database>                -- 2
**        ANALYZE  ?<database>.?<tablename>  -- 3
**
** Form 1 causes all indices in all attached databases to be analyzed.
** Form 2 analyzes all indices the single database named.
** Form 3 analyzes all indices associated with the named table.
** 
** 格式1 所有数据库中的所有索引将被分析。
** 格式2 分析给出名字的数据库的所有索引。
** 格式3 分析给出表名的表的所有索引. 
*/
void sqlite3Analyze(Parse *pParse, Token *pName1, Token *pName2){
  sqlite3 *db = pParse->db;  /*数据库句柄 */
  int iDb;    /*指定要分析的数据库；iDb<0表示数据库不存在*/
  int i;   /*定义i编号，表示数据库索引*/
  char *z, *zDb;   /*zDb表示数据库名字*/
  Table *pTab;   /*指定表名字*/
  Index *pIdx;  /*一个正在被分析的索引*/
  Token *pTableName;  /*标识符 指定表的名字*/

  /* Read the database schema. If an error occurs, leave an error message
  ** and code in pParse and return NULL. 
  **
  ** 读数据库的模式。如果出现一个错误, 在解析器中留下一个错误信息并返回空。
  */
  assert( sqlite3BtreeHoldsAllMutexes(pParse->db) );   /*互斥判断*/
  if( SQLITE_OK!=sqlite3ReadSchema(pParse) ){
    return;
  }

  assert( pName2!=0 || pName1==0 );
  if( pName1==0 ){
    /* 格式1:  分析所有的 */
    for(i=0; i<db->nDb; i++){   /* nDb表示目前所使用的后端数，即数据库数*/
      if( i==1 ) continue;  /* Do not analyze the TEMP database */ /*不分析临时数据库*/
      analyzeDatabase(pParse, i);   /*调用analyzeDatabase方法，分析指定数据库*/
    }
  }else if( pName2->n==0 ){
    /* Form 2:  Analyze the database or table named */

   /* 格式2:  分析给出名字的数据库或者表 */
    iDb = sqlite3FindDb(db, pName1);   /*通过Token的数据名字pNaame1 返回数据库的索引*/
    if( iDb>=0 ){  /*存在要分析的数据库*/
      analyzeDatabase(pParse, iDb);  /*调用analyzeDatabase方法，分析指定数据库*/
    }else{
      z = sqlite3NameFromToken(db, pName1);  /*输入一个Token的数据返回一个字符串*/
      if( z ){
		/*sqlite3FindIndex方法，定位 描述一个特定的索引的内存结构，给出这个特殊的索引的名字和数据库的名称，
		  这个数据库包含这个索引。如果没有找到返回0*/  
        if( (pIdx = sqlite3FindIndex(db, z, 0))!=0 ){  /*参数0表示数据库*/
          analyzeTable(pParse, pIdx->pTable, pIdx);  /*调用analyzeTable方法，对数据库中的一个表进行分析*/
		/*sqlite3LocateTable方法，定位 描述一个特定的数据库表的内存结构，给出这个特殊的表的名字和（可选）数据库的名称，
		  这个数据库包含这个表格。如果没有找到返回0.在pParse->zErrMsg中留下一个错误信息。*/  
        }else if( (pTab = sqlite3LocateTable(pParse, 0, z, 0))!=0 ){  /*第一个0是参数isView，表示是否是视图；第二个0是参数zDbase,表示数据库*/
          analyzeTable(pParse, pTab, 0);
        }
        sqlite3DbFree(db, z);  /*释放数据库连接关联的内存*/
      }
    }
  }else{
    /* Form 3: Analyze the fully qualified table name */

    /* 格式 3: 分析所有对应表名的表 */
	/*调用过程同上*/    
    iDb = sqlite3TwoPartName(pParse, pName1, pName2, &pTableName);  /*根据Token的数据pName1、pName2，和给出的表名，返回数据库索引*/
    if( iDb>=0 ){  /*数据库存在*/
      zDb = db->aDb[iDb].zName;  /*根据索引得到数据库名字*/
      z = sqlite3NameFromToken(db, pTableName);
      if( z ){
        if( (pIdx = sqlite3FindIndex(db, z, zDb))!=0 ){
          analyzeTable(pParse, pIdx->pTable, pIdx);
        }else if( (pTab = sqlite3LocateTable(pParse, 0, z, zDb))!=0 ){
          analyzeTable(pParse, pTab, 0);
        }
        sqlite3DbFree(db, z);  /*释放数据库连接关联的内存*/
      }
    }   
  }
}

/*
** Used to pass information from the analyzer reader through to the
** callback routine.
** 用于传递信息，从读分析器到回调过程
*/
typedef struct analysisInfo analysisInfo;
struct analysisInfo {
  sqlite3 *db;
  const char *zDatabase;
};

/*
** This callback is invoked once for each index when reading the
** sqlite_stat1 table.  
**
**     argv[0] = name of the table
**     argv[1] = name of the index (might be NULL)
**     argv[2] = results of analysis - on integer for each column
**
** Entries for which argv[1]==NULL simply record the number of rows in
** the table.
**
**当读sqlite_stat1 表时，每个索引调用一次这个回调过程。
**
**     argv[0] = 表名
**     argv[1] = 索引名 (可能为空)
**     argv[2] = 分析的结果 - 对于每一列的整数
**
**  argv[1]==NULL 仅仅记录表中的行数
*/
static int analysisLoader(void *pData, int argc, char **argv, char **NotUsed){
  analysisInfo *pInfo = (analysisInfo*)pData;  /*结构体analysisInfo，用于传递信息，从读分析器到回调过程*/
  Index *pIndex;
  Table *pTable;
  int i, c, n;
  tRowcnt v;  /*宏定义：typedef u32 tRowcnt， 32-bit is the default 32位是默认的  */
  const char *z;

  assert( argc==3 );
  UNUSED_PARAMETER2(NotUsed, argc);  /* UNUSED_PARAMETER2宏被用来抑制编译器警告，*出自sqliteInt.h 632行 */

  if( argv==0 || argv[0]==0 || argv[2]==0 ){
    return 0;
  }
  pTable = sqlite3FindTable(pInfo->db, argv[0], pInfo->zDatabase);
  if( pTable==0 ){
    return 0;
  }
  if( argv[1] ){
    pIndex = sqlite3FindIndex(pInfo->db, argv[1], pInfo->zDatabase);
  }else{
    pIndex = 0;
  }
  n = pIndex ? pIndex->nColumn : 0;
  z = argv[2];
  for(i=0; *z && i<=n; i++){
    v = 0;
    while( (c=z[0])>='0' && c<='9' ){
      v = v*10 + c - '0';
      z++;
    }
    if( i==0 ) pTable->nRowEst = v;
    if( pIndex==0 ) break;
    pIndex->aiRowEst[i] = v;
    if( *z==' ' ) z++;
    if( memcmp(z, "unordered", 10)==0 ){
      pIndex->bUnordered = 1;
      break;
    }
  }
  return 0;
}

/*
** If the Index.aSample variable is not NULL, delete the aSample[] array
** and its contents.
** 如果索引变量aSample不为空，删除aSample数组和它的内容
*/
void sqlite3DeleteIndexSamples(sqlite3 *db, Index *pIdx){
#ifdef SQLITE_ENABLE_STAT3
  if( pIdx->aSample ){
    int j;
    for(j=0; j<pIdx->nSample; j++){
      IndexSample *p = &pIdx->aSample[j];  /*结构体IndexSample，sqlite_stat3表中的每个样本值均使用这种类型的结构表示在内存中表示。*/
      if( p->eType==SQLITE_TEXT || p->eType==SQLITE_BLOB ){
        sqlite3DbFree(db, p->u.z);  /* 释放数据库连接关联的内存 */
      }
    }
    sqlite3DbFree(db, pIdx->aSample);
  }
  if( db && db->pnBytesFreed==0 ){  /*整数型指针pnBytesFreed，若不为空，将其加入函数DbFree()中*/
    pIdx->nSample = 0;  /*nSample当前的样本数目*/
    pIdx->aSample = 0;
  }
#else
  UNUSED_PARAMETER(db);
  UNUSED_PARAMETER(pIdx);
#endif
}

#ifdef SQLITE_ENABLE_STAT3
/*
** Load content from the sqlite_stat3 table into the Index.aSample[]
** arrays of all indices.
** 将sqlite_stat3表的内容加载到所有索引的 aSample 数组中
*/
static int loadStat3(sqlite3 *db, const char *zDb){
  int rc;                       /* Result codes from subroutines */ /*子过程返回值*/
  sqlite3_stmt *pStmt = 0;      /* An SQL statement being run */ /*正在运行的一个SQl语句*/
  char *zSql;                   /* Text of the SQL statement */ /*SQL语句的内容*/
  Index *pPrevIdx = 0;          /* Previous index in the loop */ /*循环中的前一个索引*/
  int idx = 0;                  /* slot in pIdx->aSample[] for next sample */ /*下一个样本在pIdx->aSample[]的痕迹*/
  int eType;                    /* Datatype of a sample */ /*样本的数据类型*/
  IndexSample *pSample;         /* A slot in pIdx->aSample[] */  /*pIdx->aSample[]中的一个痕迹*/

  assert( db->lookaside.bEnabled==0 );  /*Lookaside结构体定义lookaside，表示后背动态内存分配配置*/
  if( !sqlite3FindTable(db, "sqlite_stat3", zDb) ){  /*sqlite3FindTable方法定位描述一个特定的数据库表的内存结构*/
    return SQLITE_OK;
  }

  zSql = sqlite3MPrintf(db,  /*sqlite3MPrintf()函数用于将从sqliteMalloc()函数获得的内容打印到内存中去，并且使用内部%转换扩展。*/
      "SELECT idx,count(*) FROM %Q.sqlite_stat3"
      " GROUP BY idx", zDb);
  if( !zSql ){
    return SQLITE_NOMEM;
  }
  rc = sqlite3_prepare(db, zSql, -1, &pStmt, 0);
  sqlite3DbFree(db, zSql);
  if( rc ) return rc;

  while( sqlite3_step(pStmt)==SQLITE_ROW ){
    char *zIndex;   /* Index name */ /*索引名*/
    Index *pIdx;    /* Pointer to the index object */ /*指向索引对象的指针*/
    int nSample;    /* Number of samples */ /*样本数*/

    zIndex = (char *)sqlite3_column_text(pStmt, 0);
    if( zIndex==0 ) continue;
    nSample = sqlite3_column_int(pStmt, 1);
    pIdx = sqlite3FindIndex(db, zIndex, zDb);  /*sqlite3FindIndex方法定位描述一个特定的索引的内存结构*/
    if( pIdx==0 ) continue;
    assert( pIdx->nSample==0 );
    pIdx->nSample = nSample;
    pIdx->aSample = sqlite3DbMallocZero(db, nSample*sizeof(IndexSample));  /*分配0内存 如果分配失败 确保在连接指针中分配失败的标志*/
    pIdx->avgEq = pIdx->aiRowEst[1];  /*aiRowEst表示ANALYZE命令的结果:Est. rows由每一列选择*/
    if( pIdx->aSample==0 ){
      db->mallocFailed = 1;  /*mallocFailed表示若动态内存分配失败即为真*/
      sqlite3_finalize(pStmt);
      return SQLITE_NOMEM;
    }
  }
  rc = sqlite3_finalize(pStmt);
  if( rc ) return rc;

  zSql = sqlite3MPrintf(db,  /*/*sqlite3MPrintf()函数用于将从sqliteMalloc()函数获得的内容打印到内存中去，并且使用内部%转换扩展。*/
      "SELECT idx,neq,nlt,ndlt,sample FROM %Q.sqlite_stat3", zDb);
  if( !zSql ){
    return SQLITE_NOMEM;
  }
  rc = sqlite3_prepare(db, zSql, -1, &pStmt, 0);
  sqlite3DbFree(db, zSql);
  if( rc ) return rc;

  while( sqlite3_step(pStmt)==SQLITE_ROW ){
    char *zIndex;   /* Index name */  /*索引名字*/
    Index *pIdx;    /* Pointer to the index object */  /*指向索引对象的指针*/
    int i;          /* Loop counter */  /*循环计数器*/
    tRowcnt sumEq;  /* Sum of the nEq values */  /*nEq值的总数*/

    zIndex = (char *)sqlite3_column_text(pStmt, 0);
    if( zIndex==0 ) continue;
    pIdx = sqlite3FindIndex(db, zIndex, zDb);  /*定位描述一个特定的索引的内存结构*/
    if( pIdx==0 ) continue;
    if( pIdx==pPrevIdx ){
      idx++;
    }else{
      pPrevIdx = pIdx;
      idx = 0;
    }
    assert( idx<pIdx->nSample );
    pSample = &pIdx->aSample[idx];
    pSample->nEq = (tRowcnt)sqlite3_column_int64(pStmt, 1);
    pSample->nLt = (tRowcnt)sqlite3_column_int64(pStmt, 2);
    pSample->nDLt = (tRowcnt)sqlite3_column_int64(pStmt, 3);
    if( idx==pIdx->nSample-1 ){
      if( pSample->nDLt>0 ){
        for(i=0, sumEq=0; i<=idx-1; i++) sumEq += pIdx->aSample[i].nEq;
        pIdx->avgEq = (pSample->nLt - sumEq)/pSample->nDLt;
      }
      if( pIdx->avgEq<=0 ) pIdx->avgEq = 1;
    }
    eType = sqlite3_column_type(pStmt, 4);
    pSample->eType = (u8)eType;
    switch( eType ){
      case SQLITE_INTEGER: {
        pSample->u.i = sqlite3_column_int64(pStmt, 4);
        break;
      }
      case SQLITE_FLOAT: {
        pSample->u.r = sqlite3_column_double(pStmt, 4);
        break;
      }
      case SQLITE_NULL: {
        break;
      }
      default: assert( eType==SQLITE_TEXT || eType==SQLITE_BLOB ); {
        const char *z = (const char *)(
              (eType==SQLITE_BLOB) ?
              sqlite3_column_blob(pStmt, 4):
              sqlite3_column_text(pStmt, 4)
           );
        int n = z ? sqlite3_column_bytes(pStmt, 4) : 0;
        pSample->nByte = n;
        if( n < 1){
          pSample->u.z = 0;
        }else{
          pSample->u.z = sqlite3DbMallocRaw(db, n);
          if( pSample->u.z==0 ){
            db->mallocFailed = 1;
            sqlite3_finalize(pStmt);
            return SQLITE_NOMEM;
          }
          memcpy(pSample->u.z, z, n);
        }
      }
    }
  }
  return sqlite3_finalize(pStmt);
}
#endif /* SQLITE_ENABLE_STAT3 */

/*
** Load the content of the sqlite_stat1 and sqlite_stat3 tables. The
** contents of sqlite_stat1 are used to populate the Index.aiRowEst[]
** arrays. The contents of sqlite_stat3 are used to populate the
** Index.aSample[] arrays.
**
** 载入表sqlite_stat1和sqlite_stat3的内容.表sqlite_stat1的内容用于填充
** aiRowEst索引数组。表sqlite_stat3用于填充aSample索引数组。
**  
** If the sqlite_stat1 table is not present in the database, SQLITE_ERROR
** is returned. In this case, even if SQLITE_ENABLE_STAT3 was defined 
** during compilation and the sqlite_stat3 table is present, no data is 
** read from it.
** 
** 如果sqlite_stat1 表当前不在数据库中, 返回 SQLITE_ERROR。此种情况下，
** 在编译过程中即使 SQLITE_ENABLE_STAT3 被定义了，并且 sqlite_stat3表当前
** 也存在，也不能从中读出任何数据。
** 
** If SQLITE_ENABLE_STAT3 was defined during compilation and the 
** sqlite_stat3 table is not present in the database, SQLITE_ERROR is
** returned. However, in this case, data is read from the sqlite_stat1
** table (if it is present) before returning.
**
** 如果在编译过程中 SQLITE_ENABLE_STAT3 被定义了 且 sqlite_stat3表当前
** 不在数据库中, SQLITE_ERROR 被返回，但是在这种情况下，在返回前数据可以从
** sqlite_stat1 表中读出。 
**
** If an OOM error occurs, this function always sets db->mallocFailed.
** This means if the caller does not care about other errors, the return
** code may be ignored.
**
** 如果一个 OOM 错误出现, 此函数将总是设置为 db->mallocFailed.
** 这意味着调用器不关注其他错误,返回代码将被忽略。
*/
int sqlite3AnalysisLoad(sqlite3 *db, int iDb){
  analysisInfo sInfo;
  HashElem *i;
  char *zSql;
  int rc;

  assert( iDb>=0 && iDb<db->nDb );
  assert( db->aDb[iDb].pBt!=0 );

  /* Clear any prior statistics 
  ** 清空之前的所有数据
  */
  assert( sqlite3SchemaMutexHeld(db, iDb, 0) );
  for(i=sqliteHashFirst(&db->aDb[iDb].pSchema->idxHash);i;i=sqliteHashNext(i)){  
	/*sqliteHashFirst、sqliteHashNext为哈希表的宏定义，pSchema表示指向数据库模式的指针(可能是共享的)*/
    Index *pIdx = sqliteHashData(i);  /*sqliteHashData为哈希表的宏定义*/
    sqlite3DefaultRowEst(pIdx);  /*用默认的信息填充Index.aiRowEst[]数组，当我们不运行ANALYZE指令时，就使用这些信息。*/
#ifdef SQLITE_ENABLE_STAT3
    sqlite3DeleteIndexSamples(db, pIdx);
    pIdx->aSample = 0;
#endif
  }

  /* Check to make sure the sqlite_stat1 table exists 
  ** 检查确定sqlite_stat1表存在
  */
  sInfo.db = db;
  sInfo.zDatabase = db->aDb[iDb].zName;
  if( sqlite3FindTable(db, "sqlite_stat1", sInfo.zDatabase)==0 ){  /*定位描述一个特定的数据库表的内存结构*/
    return SQLITE_ERROR;
  }

  /* Load new statistics out of the sqlite_stat1 table 
  ** 从sqlite_stat1表外载出新数据
  */
  zSql = sqlite3MPrintf(db, 
      "SELECT tbl,idx,stat FROM %Q.sqlite_stat1", sInfo.zDatabase);
  if( zSql==0 ){
    rc = SQLITE_NOMEM;
  }else{
    rc = sqlite3_exec(db, zSql, analysisLoader, &sInfo, 0);  /*/* sqlite3执行函数，执行SQL代码。出自legacy.c 36行*/*/
    sqlite3DbFree(db, zSql);
  }


  /* Load the statistics from the sqlite_stat3 table. 
  ** 从sqlite_stat3表载出数据
  */
#ifdef SQLITE_ENABLE_STAT3
  if( rc==SQLITE_OK ){
    int lookasideEnabled = db->lookaside.bEnabled;  /*lookaside表示后备动态内存分配配置。bEnabled是一个标志位，占用两个字节的无符号整数，表示可以进行新的后备内存区的分配。*/
    db->lookaside.bEnabled = 0;
    rc = loadStat3(db, sInfo.zDatabase);
    db->lookaside.bEnabled = lookasideEnabled;
  }
#endif

  if( rc==SQLITE_NOMEM ){
    db->mallocFailed = 1;  /*mallocFailed表示若动态内存分配失败即为真*/
  }
  return rc;
}


#endif /* SQLITE_OMIT_ANALYZE */

