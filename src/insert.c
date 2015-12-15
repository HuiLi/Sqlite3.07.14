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
/*
** This file contains C code routines that are called by the parser
** to handle INSERT statements in SQLite.
** 在SQLite中，这个实现文件内容是实现由解析器调用C代码处理插入操作的代码例程。*/
#include "sqliteInt.h"

/*
** Generate code that will open a table for reading.
*/
//1、sqlite3OpenTable()函数实现打开一张用于读取数据操作的数据库表.
//输入参数依次：指向解析器的指针p，表中游标的数量，在数据库连接实例索引，指向数据库表的指针pTab，读表或是写表操作
void sqlite3OpenTable(
	Parse *p,   /* Generate code into this VDBE  语法解析生成代码给VDBE*/
	int iCur,   /* The cursor number of the table 表的游标数目 */
	int iDb,      /* The database index in sqlite3.aDb[]  在sqlite3.aDb[]上的数据库索引*/
	Table *pTab,  /* The table to be opened  被打开的表*/
	int opcode    /* OP_OpenRead or OP_OpenWrite  打开之后读操作或者是写操作*/
	）
{
	Vdbe *v;  //指向VDBE引擎的指针v

  if( IsVirtual(pTab) ) //判断是否虚拟表
    return;

  //在给定的解析器的情况下，获得一个VDBE引擎
  v = sqlite3GetVdbe(p);

  assert( opcode==OP_OpenWrite || opcode==OP_OpenRead ); //断言进行的操作要么是读要么是写

  //锁定表函数，其中参数p是语法解析上下文，iDb索引，(opcode==OP_OpenWrite)?1:0确定打开的究竟是否是写操作，pTab->zName参数是被打开的表的表名
  sqlite3TableLock(p, iDb, pTab->tnum, (opcode==OP_OpenWrite)?1:0, pTab->zName);

  //给当前的VDBE引擎添加一条新的指令，返回新指令的地址。(VDBE引擎，opcode读或者写操作，iCur游标操作数，pTab->tnum被打开的表的数目，iDb参数是索引)
  sqlite3VdbeAddOp3(v, opcode, iCur, pTab->tnum, iDb);

  //把操作数P4的值改变为一个特殊的指令。
  //如果P4_INT32>=0，则P4是动态的, 就是说使用sqlite3_malloc()动态分配函数获得一个字符串。即等于0时，复制第三个参数值个字节，包括其第一个空字节；大于0复制其（n+1）个字节。
  //若P4_INT32==P4_KEYINFO,意味着zP4结构是指针，该结构通过sqlite3_malloc()存入到内存中，当虚拟机完成是释放。-1小于0表明改变P4最近添加的指令值。
  sqlite3VdbeChangeP4(v, -1, SQLITE_INT_TO_PTR(pTab->nCol), P4_INT32);

  VdbeComment((v, "%s", pTab->zName));//VDBE引擎打印当前被打开的表的表名
}

/*
** Return a pointer to the column affinity string associated with index
** pIdx. A column affinity string has one character for each column in
** 对于与索引相关的列返回一个指针，对于在这个表中的每一列都有一个字符，按照下面的规则：
** the table, according to the affinity of the column:
**
**  Character      Column affinity
**  ------------------------------
**  'a'            TEXT
**  'b'            NONE
**  'c'            NUMERIC
**  'd'            INTEGER
**  'e'            REAL
**
** An extra 'd' is appended to the end of the string to cover the
** rowid that appears as the last column in every index.
** 一个附加的'd'被追加在字符串的末尾，这样做是想要去覆盖rowid，rowid出现在每一个索引的最后一列
** Memory for the buffer containing the column index affinity string
** is managed along with the rest of the Index structure. It will be
** released when sqlite3DeleteIndex() is called.
** 内存被设置去管理剩余的索引结构，当sqlite3DeleteIndex()被调用的时候这个缓冲区将会被删除
*/
const char *sqlite3IndexAffinityStr(Vdbe *v, Index *pIdx)
{
  //当索引结构本身被清除了的时候，最终这个列关联的字符串通过sqliteDeleteIndex()函数删除。
  if( !pIdx->zColAff )
  {
	  /* The first time a column affinity string for a particular index is
	  ** required, it is allocated and populated here. It is then stored as
	  ** a member of the Index structure for subsequent use.
	  ** 对于一个特定的索引来说，一个列关联字符串在第一次才需要分配和赋值的。接着它将被存储作为一个索引结构的成员，这样做是为了给子序列使用
	  ** The column affinity string will eventually be deleted by
	  ** sqliteDeleteIndex() when the Index structure itself is cleaned up.
	  ** 当索引结构被删除的时候，一个列关联字符串最后将会调用sqliteDeleteIndex()函数删除
	  */
    int n;
    Table *pTab = pIdx->pTable;//索引所指向的包指针
    sqlite3 *db = sqlite3VdbeDb(v);//定义一个当前VDBE引擎运行的数据库数据实例的指针

    //对于数据的索引一旦开始分配，失败终止只有当失败分配重置。
    pIdx->zColAff = (char *)sqlite3DbMallocRaw(0, pIdx->nColumn+2);
    if( !pIdx->zColAff )   //如果没有定义每一列关联字符串的话
    {
      db->mallocFailed = 1;   //将内存分配失败
      return 0; //退出整个函数的执行
    }
    for(n=0; n < pIdx->nColumn; n++)
    {
      pIdx->zColAff[n] = pTab->aCol[pIdx->aiColumn[n]].affinity;
    }
    pIdx->zColAff[n++] = SQLITE_AFF_INTEGER;
    pIdx->zColAff[n] = 0;
  }

  return pIdx->zColAff;
}

/*
** Set P4 of the most recently inserted opcode to a column affinity
** string for table pTab.
** 对于最常插入列关联一个字符串
** A column affinity string has one character
** for each column indexed by the index, according to the affinity of the
** column:
** 一个列关联字符串
**  Character      Column affinity
**  ------------------------------
**  'a'            TEXT
**  'b'            NONE
**  'c'            NUMERIC
**  'd'            INTEGER
**  'e'            REAL
*/
// 该函数主要是用于把表和与列相关的字符关联在一起。
void sqlite3TableAffinityStr(Vdbe *v, Table *pTab)
{
	/* The first time a column affinity string for a particular table
	** is required, it is allocated and populated here. It is then
	** stored as a member of the Table structure for subsequent use.
	**
	** The column affinity string will eventually be deleted by
	** sqlite3DeleteTable() when the Table structure itself is cleaned up.
	*/
    /* 对于一个特定的索引来说，一个列关联字符串在第一次才需要分配和赋值的。
    ** 然后对于后续的使用，它将会作为一个索引结构成员存储.
    ** 当索引结构本身被清除了的时候，最终这个列关联的字符串通过sqliteDeleteIndex()函数删除。
    */
  if( !pTab->zColAff )
    {
    char *zColAff;
    int i;
    sqlite3 *db = sqlite3VdbeDb(v);

    zColAff = (char *)sqlite3DbMallocRaw(0, pTab->nCol+1);
    if( !zColAff ){
      db->mallocFailed = 1;
      return;
    }

    for(i=0; i<pTab->nCol; i++){
      zColAff[i] = pTab->aCol[i].affinity;
    }
    zColAff[pTab->nCol] = '\0';

    pTab->zColAff = zColAff;
  }

  sqlite3VdbeChangeP4(v, -1, pTab->zColAff, P4_TRANSIENT);//同上
}


/*
** Return non-zero if the table pTab in database iDb or any of its indices
** have been opened at any point in the VDBE program beginning at location
** iStartAddr throught the end of the program.  This is used to see if
** a statement of the form  "INSERT INTO <iDb, pTab> SELECT ..." can
** run without using temporary table for the results of the SELECT.
*/

/*
**如果表pTab在数据库索引数组中，或是它的索引在任何时候被打开在VDBE程序中的开始位置到结束位置的话，返回非零值。
这是用于检查"INSERT INTO <iDb, pTab> SELECT ..."规则的语句是否对于查询结构没有临时表的情况下可以运行
*/
//4.申明一个静态函数仅限于表文件调用。
static int readsTable(Parse *p, int iStartAddr, int iDb, Table *pTab)
{
  Vdbe *v = sqlite3GetVdbe(p);
  int i;
  int iEnd = sqlite3VdbeCurrentAddr(v);//获得下一条指令的地址

  //判断表示符SQLITE_OMIT_VIRTUALTABLE是否被定义，若未被则编译该段程序
#ifndef SQLITE_OMIT_VIRTUALTABLE
  VTable *pVTab = IsVirtual(pTab) ? sqlite3GetVTable(p->db, pTab) : 0;
#endif

  for(i=iStartAddr; i<iEnd; i++)//获取操作地址
    {
    VdbeOp *pOp = sqlite3VdbeGetOp(v, i);//返回给定地址的指令
    assert( pOp!=0 );
    if( pOp->opcode==OP_OpenRead && pOp->p3==iDb )//参数匹配
    {
      Index *pIndex;
      int tnum = pOp->p2;
      if( tnum==pTab->tnum )
      {
        return 1;
      }
      for(pIndex=pTab->pIndex; pIndex; pIndex=pIndex->pNext)
      {//循环查找表的索引
        if( tnum==pIndex->tnum )
        {
          return 1;//如果查找成功就返回1
        }
      }
    }
#ifndef SQLITE_OMIT_VIRTUALTABLE
    if( pOp->opcode==OP_VOpen && pOp->p4.pVtab==pVTab )
    {
      assert( pOp->p4.pVtab!=0 );
      assert( pOp->p4type==P4_VTAB );
      return 1;
    }
#endif
  }
  return 0;
}

////5.定义SQLITE_OMIT_AUTOINCREMENT
#ifndef SQLITE_OMIT_AUTOINCREMENT
//在数据库中，找到或是创建一个与表pTab关联的AutoincInfo的结构。
//尽管相同的表用于插入到触发器中自动增加多次，但是每一张表至多有一个AutoincInfo结构。
//如果是第一次使用表pTab，将会新建一个AutoincInfo结构。在原始的AutoincInfo结构将在以后继续使用。
//分配三个内存位置：（1）第一个注册保存表的名字。（2）第一个注册保存表最大的行号。
//（3）第三个注册保存表的sqlite序列的行号。所有插入程序必须知道第二个寄存器将作为返回。
//返回int数据库类型的静态函数。传入参数依次为解析器环境，数据库保存表的索引，我们需要的表。
static int autoIncBegin(Parse *pParse, int iDb, Table *pTab )
{
  int memId = 0; //注册保存最大的行号

  if( pTab->tabFlags & TF_Autoincrement )//获取TF_Autoincrement的地址
  {
    Parse *pToplevel = sqlite3ParseToplevel(pParse);
    AutoincInfo *pInfo;

    pInfo = pToplevel->pAinc;
    while( pInfo && pInfo->pTab!=pTab )
    {
        pInfo = pInfo->pNext;
    }
    if( pInfo==0 )
    {
      pInfo = sqlite3DbMallocRaw(pParse->db, sizeof(*pInfo));
      if( pInfo==0 )
        return 0;
      pInfo->pNext = pToplevel->pAinc;
      pToplevel->pAinc = pInfo;
      pInfo->pTab = pTab;
      pInfo->iDb = iDb;
      pToplevel->nMem++;   //注册保存表的名称
      pInfo->regCtr = ++pToplevel->nMem; //最大行寄存器
      pToplevel->nMem++;  //序列中行号
    }
    memId = pInfo->regCtr;
  }
  return memId;
}

//6、使用自动增量跟踪初始化的所有的注册
void sqlite3AutoincrementBegin(Parse *pParse)
{
  AutoincInfo *p;//自动增量的信息
  sqlite3 *db = pParse->db;  //数据库连接
  Db *pDb;                   //数据库只有原子表
  int memId;                 //注册保存最大的行号
  int addr;                  //虚拟数据库引擎地址
  Vdbe *v = pParse->pVdbe;   //创建一个指向虚拟数据库引擎的虚拟机

  //这个实例仅仅从顶层调用，所以在触发器生成是从来不会调用。
  assert( pParse->pTriggerTab==0 );
  assert( pParse==sqlite3ParseToplevel(pParse) );

  assert( v );  //如果事实不是那样的话，我们创建虚拟机失败很久了
  for(p = pParse->pAinc; p; p = p->pNext)
  {
    pDb = &db->aDb[p->iDb];
    memId = p->regCtr;
    assert( sqlite3SchemaMutexHeld(db, 0, pDb->pSchema) );//正常的互斥访问
    sqlite3OpenTable(pParse, 0, p->iDb, pDb->pSchema->pSeqTab, OP_OpenRead);
    sqlite3VdbeAddOp3(v, OP_Null, 0, memId, memId+1);
    addr = sqlite3VdbeCurrentAddr(v);
    sqlite3VdbeAddOp4(v, OP_String8, 0, memId-1, 0, p->pTab->zName, 0);
    sqlite3VdbeAddOp2(v, OP_Rewind, 0, addr+9);
    sqlite3VdbeAddOp3(v, OP_Column, 0, 0, memId);
    sqlite3VdbeAddOp3(v, OP_Ne, memId-1, addr+7, memId);
    sqlite3VdbeChangeP5(v, SQLITE_JUMPIFNULL);
    sqlite3VdbeAddOp2(v, OP_Rowid, 0, memId+1);
    sqlite3VdbeAddOp3(v, OP_Column, 0, 1, memId);
    sqlite3VdbeAddOp2(v, OP_Goto, 0, addr+9);
    sqlite3VdbeAddOp2(v, OP_Next, 0, addr+2);
    sqlite3VdbeAddOp2(v, OP_Integer, 0, memId);
    sqlite3VdbeAddOp0(v, OP_Close);
  }
}

//7、为一个自增计算器跟新最大的行Id
//这个例程当前这个堆栈的顶端即将插入一个新的行id时运行。如果这个新的行Id比在MemId内存单元中的最大行id大时，
//那么这个内存单元将被更新。这个堆栈是不可改变的。
static void autoIncStep(Parse *pParse, int memId, int regRowid)
{
  if( memId>0 )
  {
    sqlite3VdbeAddOp2(pParse->pVdbe, OP_MemMax, memId, regRowid);//调用Vdbeaux.c文件中的sqlite3VdbeAddOp2函数
  }
}

/*
** This routine generates the code needed to write autoincrement
** maximum rowid values back into the sqlite_sequence register.
** Every statement that might do an INSERT into an autoincrement
** table (either directly or through triggers) needs to call this
** routine just before the "exit" code.
*/
//8、这个例程产生的代码必须把自增的最大行id的值写回到sqlite_sequence寄存器中。
//仅在这个“exit”代码之前，每一条可能把一个INSERT写入到一个自增表（直接或间接地通过寄存器）中需要调用这个例程。
void sqlite3AutoincrementEnd(Parse *pParse)
{
  AutoincInfo *p;
  Vdbe *v = pParse->pVdbe;
  sqlite3 *db = pParse->db;

  assert( v );
  for(p = pParse->pAinc; p; p = p->pNext)
  {
    Db *pDb = &db->aDb[p->iDb];
    int j1, j2, j3, j4, j5;
    int iRec;
    int memId = p->regCtr;

    iRec = sqlite3GetTempReg(pParse);
    assert( sqlite3SchemaMutexHeld(db, 0, pDb->pSchema) );
    sqlite3OpenTable(pParse, 0, p->iDb, pDb->pSchema->pSeqTab, OP_OpenWrite);
    j1 = sqlite3VdbeAddOp1(v, OP_NotNull, memId+1);
    j2 = sqlite3VdbeAddOp0(v, OP_Rewind);
    j3 = sqlite3VdbeAddOp3(v, OP_Column, 0, 0, iRec);
    j4 = sqlite3VdbeAddOp3(v, OP_Eq, memId-1, 0, iRec);
    sqlite3VdbeAddOp2(v, OP_Next, 0, j3);
    sqlite3VdbeJumpHere(v, j2);
    sqlite3VdbeAddOp2(v, OP_NewRowid, 0, memId+1);
    j5 = sqlite3VdbeAddOp0(v, OP_Goto);
    sqlite3VdbeJumpHere(v, j4);
    sqlite3VdbeAddOp2(v, OP_Rowid, 0, memId+1);
    sqlite3VdbeJumpHere(v, j1);
    sqlite3VdbeJumpHere(v, j5);
    sqlite3VdbeAddOp3(v, OP_MakeRecord, memId-1, 2, iRec);
    sqlite3VdbeAddOp3(v, OP_Insert, 0, iRec, memId+1);
    sqlite3VdbeChangeP5(v, OPFLAG_APPEND);
    sqlite3VdbeAddOp0(v, OP_Close);
    sqlite3ReleaseTempReg(pParse, iRec);
  }
}

#else
//如果SQLITE_OMIT_AUTOINCREMENT已经被定义了，那么上面的这三个例程都无操作。
# define autoIncBegin(A,B,C) (0)
# define autoIncStep(A,B,C)
#endif


// 9、xferOptimization（）函数的前置声明
//传入参数依次为：解析器环境，要插入的表，SELECT语句数据源，处理约束的错误，Pdest数据库
static int xferOptimization(Parse *pParse, Table *pDest, Select *pSelect, int onError, int iDbDest );

//10、这个函数例程完成的工作有：
//这个例程调用的时候，SQL是遵循以下形式处理的：
// (1)insert into TABLE (IDLIST) values(EXPRLIST)/插入值
// (2)insert into TABLE (IDLIST) select/复制表，此时的目标表示存在的
//表名后的IDLIST通常是可选的。如果省略不写，那么表的所有列的列表替代。IDLIST出现在pColumn的参
//数中。如果IDLIST省略不写，那么pColumn是空值。
//pList参数处理(1)式中的EXPRLIST,pSelect是可选的。对于（2）式，pList是空的，pSelect是一个
//用于插入产生数据的查询语句的指针。

//代码的生成遵循以下四个当中的一个模板。对于一个简单的查询语句来说，数据来自一个子句的值，代码的//执

//行一次通过。伪代码如下（我们称之为1号模板）：
//	open write cursor to <table> and its indices/打开游标写到表中和索引中
//	puts VALUES clause expressions onto the stack/把子句值表达式压入栈中
//	write the resulting record into <table> cleanup/把结果记录到表中 清除

//剩下的3个模板假设语句是下面这种形式的：
//	INSERT INTO <table> SELECT ...
//如果查询子句内容是"SELECT * FROM <table2>"这种约束形式，也就是说，如果从一个表中获取所有列的查//询,

//就是没有“where”、“limit”、“group by”和“group by 子句”。如果表1和表2是有相同模式（包//括所有相

//同的索引）的不同的表，那么从表2中复制原始的记录到表1中涉及到一个特殊的优化。为了实现这//个模板，可以

//参看这个xferOptimization()函数。这是第二个模板：
//	open a write cursor to <table>
//	open read cursor on <table2>
//	transfer all records in <table2> over to <table>
//	close cursors
//	foreach index on <table>
//	open a write cursor on the <table> index
//	open a read cursor on the corresponding <table2> index
//	transfer all records from the read to the write cursors
//	close cursors
//	end foreach
//第三个模板是当第2个模板不能应用和查询子句在任何时候不能读表时。以以下的模板生成代码：
//     EOF <- 0
//       X <- A
//        goto B
//     A: setup for the SELECT
//        loop over the rows in the SELECT
//          load values into registers R..R+n
//         yield X
//        end loop
//        cleanup after the SELECT
//          EOF <- 1
//         yield X
//         goto A
//       B: open write cursor to <table> and its indices
//      C: yield X
//         if EOF goto D
//        insert the select result into <table> from R..R+n
//         goto C
//      D: cleanup
//第四个模板，如果插入语句从一个查询语句获取值，但是数据要查入到表中，同时表也是查询语句读取的一部
//分。在第三种形式中，我们不得不使用中间表存储这个查询的结果。模板如下：
//  EOF <- 0
//          X <- A
//         goto B
//       A: setup for the SELECT
//          loop over the tables in the SELECT
//           load value into register R..R+n
//            yield X
//          end loop
//         cleanup after the SELECT
//          EOF <- 1
//          yield X
//         halt-error
//       B: open temp table
//       L: yield X
//          if EOF goto M
//          insert row from R..R+n into temp table
//          goto L
//      M: open write cursor to <table> and its indices
//          rewind temp table
//       C: loop over rows of intermediate table
//            transfer values form intermediate table into <table>
//         end loop
//      D: cleanup
//插入函数所带参数为：解析器环境，我们要插入的表的表名，要插入的列表值，用作数据源的查询语句，与IDLIST相关的列名，处理连续错误。
void sqlite3Insert(Parse *pParse, SrcList *pTabList, ExprList *pList, Select *pSelect, IdList *pColumn, int onError )
{
  sqlite3 *db;          //主数据库的结构
  Table *pTab;          //要插入的表啊
  char *zTab;           //我们插入表的表名
  const char *zDb;      //表所在的数据库的名称
  int i, j, idx;        //循环计数器
  Vdbe *v;              //生成代码所在的虚拟机
  Index *pIdx;          //用于循环的表索引
  int nColumn;          //数据的列名
  int nHidden = 0;      //若是虚拟表的隐藏的列名
  int baseCur = 0;      //用于PTab的VBDE的游标数量
  int keyColumn = -1;   //插入主键的列
  int endOfLoop;        //用于循环插入的标签
  int useTempTable = 0; //存储查询结果的中间表
  int srcTab = 0;       //来自与临时游标大于0的数据
  int addrInsTop = 0;   //跳出的标签“D”
  int addrCont = 0;     //插入循环的顶端模板3和4的标签“C”
  int addrSelect = 0;   //实现查询语句的协同程序的地址
  SelectDest dest;      //查询插入语句的目的
  int iDb;              //处理表所在的数据库索引
  Db *pDb;              //包含要用于插入的表的数据库
  int appendFlag = 0;   //如果插入可能是一个附加，则为真

  //分配寄存器
  int regFromSelect = 0;//来自查询数据的基础寄存器
  int regAutoinc = 0;   //拥有自增计数器的寄存器
  int regRowCount = 0;  //行计数器的内存单元
  int regIns;           //处理插入的行号和数据的块寄存器
  int regRowid;         //插入行号的寄存器
  int regData;          //处理插入首列的寄存器
  int regEof = 0;       //记录查询数据末端的寄存器
  int *aRegIdx = 0;     //一个寄存器分配给每一个索引

#ifndef SQLITE_OMIT_TRIGGER
  int isView;                //如果插入到视图中时为真
  Trigger *pTrigger;         //如果需要，在表中的触发器列表
  int tmask;                 //触发器时间的屏蔽
#endif

  db = pParse->db;
  memset(&dest, 0, sizeof(dest));
  if( pParse->nErr || db->mallocFailed )
  {
    goto insert_cleanup;
  }

  //定位到我们将要插入新信息的表
  assert( pTabList->nSrc==1 );
  zTab = pTabList->a[0].zName;
  if( NEVER(zTab==0) ) goto insert_cleanup;
  pTab = sqlite3SrcListLookup(pParse, pTabList);
  if( pTab==0 )
  {
    goto insert_cleanup;
  }
  iDb = sqlite3SchemaToIndex(db, pTab->pSchema);
  assert( iDb<db->nDb );
  pDb = &db->aDb[iDb];
  zDb = pDb->zName;
  if( sqlite3AuthCheck(pParse, SQLITE_INSERT, pTab->zName, 0, zDb) )
  {
    goto insert_cleanup;
  }

  //如果我们有任何触发器和表要插入一个视图时需指明
#ifndef SQLITE_OMIT_TRIGGER
  pTrigger = sqlite3TriggersExist(pParse, pTab, TK_INSERT, 0, &tmask);
  isView = pTab->pSelect!=0;
#else
# define pTrigger 0
# define tmask 0
# define isView 0
#endif
#ifdef SQLITE_OMIT_VIEW
# undef isView
# define isView 0
#endif
  assert( (pTrigger && tmask) || (pTrigger==0 && tmask==0) );

//如果PTab确实是一个视图，确保它已经初始化。如果PTab不是视图或者说是虚拟模块表，
//那么ViewGetColumnNames()函数不做任何处理。
  if( sqlite3ViewGetColumnNames(pParse, pTab) )
  {
    goto insert_cleanup;
  }

  //保证两点：第一是不是只读的，第二是如果它是视图，那么一个插入触发器存在。
  if( sqlite3IsReadOnly(pParse, pTab, tmask) )
  {
    goto insert_cleanup;
  }

  //分配一个触发器
  v = sqlite3GetVdbe(pParse);
  if( v==0 ) goto insert_cleanup;
  if( pParse->nested==0 ) sqlite3VdbeCountChanges(v);
  sqlite3BeginWriteOperation(pParse, pSelect || pTrigger, iDb);

#ifndef SQLITE_OMIT_XFER_OPT
  //如果语句是 INSERT INTO <table1> SELECT * FROM <table2>这种形式，那么应用特殊的优化是的转换非常快，然后
  //减少索引的数据分片。
  if( pColumn==0 && xferOptimization(pParse, pTab, pSelect, onError, iDb) )
  {
    assert( !pTrigger );
    assert( pList==0 );
    goto insert_end;
  }
#endif // SQLITE_OMIT_XFER_OPT

  //如果这是一个自增的表，那么查找sqlite_sequence表中的序列数量和存储在regAutoinc的内存单元中
  regAutoinc = autoIncBegin(pParse, iDb, pTab);

  //指出支持的数据类有多少。如果数据是来自一个查询语句，那么生成一个协同程序在每一个调用中产生一个的查询。
  //对于第3、4个模板，协同程序是一个公共的头结点。
  if( pSelect )
  {
    //来自一条查询的数据。实现查询作为协同程序而生成代码。这个代码对于3、4模板是公共的。
    /*        EOF <- 0
    **         X <- A
    **         goto B
    **      A: setup for the SELECT
    **         loop over the tables in the SELECT
    **           load value into register R..R+n
    **           yield X
    **         end loop
    **         cleanup after the SELECT
    **         EOF <- 1
    **         yield X
    **         halt-error
    */
    //在协同程序的每一次调用中，它把查询结果单独的一行放入到registers dest.iMem...dest.iMem+dest.nMem-1中
    //（sqlite3Select()分配寄存器）。当查询完成的时候，把EOF标志存储在refEof中。
    int rc, j1;

    regEof = ++pParse->nMem;
    sqlite3VdbeAddOp2(v, OP_Integer, 0, regEof);      /* EOF <- 0 */
    VdbeComment((v, "SELECT eof flag"));
    sqlite3SelectDestInit(&dest, SRT_Coroutine, ++pParse->nMem);
    addrSelect = sqlite3VdbeCurrentAddr(v)+2;
    sqlite3VdbeAddOp2(v, OP_Integer, addrSelect-1, dest.iSDParm);
    j1 = sqlite3VdbeAddOp2(v, OP_Goto, 0, 0);
    VdbeComment((v, "Jump over SELECT coroutine"));

    //解析在查询语句中的表达式然后执行它。
    rc = sqlite3Select(pParse, pSelect, &dest);
    assert( pParse->nErr==0 || rc );
    if( rc || NEVER(pParse->nErr) || db->mallocFailed )
    {
      goto insert_cleanup;
    }
    sqlite3VdbeAddOp2(v, OP_Integer, 1, regEof);         /* EOF <- 1 */
    sqlite3VdbeAddOp1(v, OP_Yield, dest.iSDParm);   /* yield X */
    sqlite3VdbeAddOp2(v, OP_Halt, SQLITE_INTERNAL, OE_Abort);
    VdbeComment((v, "End of SELECT coroutine"));
    sqlite3VdbeJumpHere(v, j1);                          /* label B: */

    regFromSelect = dest.iSdst;
    assert( pSelect->pEList );
    nColumn = pSelect->pEList->nExpr;
    assert( dest.nSdst==nColumn );

    //如果查询语句的结果必须写入到一个临时表中，那么设置useTempTable为Ture（模板4）。
    //如果每一个查询所有行的结果直接写入到目的表中，那么设置useTempTable为FALSE。
    //如果一个表更新的同时还是查询语句需要读的表，那么要使用一个临时表。那种情况下的行触发器也要使用临时表。
    if( pTrigger || readsTable(pParse, addrSelect, iDb, pTab) )
    {
      useTempTable = 1;
    }

    if( useTempTable )
    {
      //调用协同程序从查询语句中提取信息和它是一个瞬间使用的表srcTab。从第4个模板生成此处的代码。
      /*      B: open temp table
      **      L: yield X
      **         if EOF goto M
      **         insert row from R..R+n into temp table
      **         goto L
      **      M: ...
      */
      int regRec;          /* 保存记录的寄存器 */
      int regTempRowid;    /*保存临时表行ID的寄存器 */
      int addrTop;         /* 标签“L”*/
      int addrIf;          /* 跳转到M的地址 */

      srcTab = pParse->nTab++;
      regRec = sqlite3GetTempReg(pParse);
      regTempRowid = sqlite3GetTempReg(pParse);
      sqlite3VdbeAddOp2(v, OP_OpenEphemeral, srcTab, nColumn);
      addrTop = sqlite3VdbeAddOp1(v, OP_Yield, dest.iSDParm);
      addrIf = sqlite3VdbeAddOp1(v, OP_If, regEof);
      sqlite3VdbeAddOp3(v, OP_MakeRecord, regFromSelect, nColumn, regRec);
      sqlite3VdbeAddOp2(v, OP_NewRowid, srcTab, regTempRowid);
      sqlite3VdbeAddOp3(v, OP_Insert, srcTab, regRec, regTempRowid);
      sqlite3VdbeAddOp2(v, OP_Goto, 0, addrTop);
      sqlite3VdbeJumpHere(v, addrIf);
      sqlite3ReleaseTempReg(pParse, regRec);
      sqlite3ReleaseTempReg(pParse, regTempRowid);
    }
  }
  else
  {
    //这是另一种情形，如果用于插入的数据是来自子句的值。
    NameContext sNC;
    memset(&sNC, 0, sizeof(sNC));
    sNC.pParse = pParse;
    srcTab = -1;
    assert( useTempTable==0 );
    nColumn = pList ? pList->nExpr : 0;
    for(i=0; i<nColumn; i++)
    {
      if( sqlite3ResolveExprNames(&sNC, pList->a[i].pExpr) )
      {
        goto insert_cleanup;
      }
    }
  }
  //保证在资源数据中列的数量匹配要插入表中的列数量。
  if( IsVirtual(pTab) )
  {
    for(i=0; i<pTab->nCol; i++)
    {
      nHidden += (IsHiddenColumn(&pTab->aCol[i]) ? 1 : 0);
    }
  }
  if( pColumn==0 && nColumn && nColumn!=(pTab->nCol-nHidden) )
  {
    sqlite3ErrorMsg(pParse,
       "table %S has %d columns but %d values were supplied",
       pTabList, 0, pTab->nCol-nHidden, nColumn);
    goto insert_cleanup;
  }
  if( pColumn!=0 && nColumn!=pColumn->nId )
  {
    sqlite3ErrorMsg(pParse, "%d values for %d columns", nColumn, pColumn->nId);
    goto insert_cleanup;
  }

  //如果一个插入语句包括一个IDLIST项，那么保证IDLIST所有的元素是这个表中列和记住列索引。
  //如果表有一个插入主键的列和列在IDLIST中是已经命名，那么把在keyColumn变量中的值记录在主键列的IDLIST的索引中。
  //当它出现在IDLIST中时而不是出现原始表中，keyColumn是主键的索引(在原始表中的主键索引是pTab->iPKey)。
  if( pColumn
  {
    for(i=0; i<pColumn->nId; i++)
    {
      pColumn->a[i].idx = -1;
    }
    for(i=0; i<pColumn->nId; i++)
    {
      for(j=0; j<pTab->nCol; j++)
      {
        if( sqlite3StrICmp(pColumn->a[i].zName, pTab->aCol[j].zName)==0 )
        {
          pColumn->a[i].idx = j;
          if( j==pTab->iPKey )
          {
            keyColumn = i;
          }
          break;
        }
      }
      if( j>=pTab->nCol )
      {
        if( sqlite3IsRowid(pColumn->a[i].zName) )
        {
          keyColumn = i;
        }
        else
        {
          sqlite3ErrorMsg(pParse, "table %S has no column named %s",
              pTabList, 0, pColumn->a[i].zName);
          pParse->checkSchema = 1;
          goto insert_cleanup;
        }
      }
    }
  }

  //但如果表有一个整型主键而没有IDLIST此项，
  //那么要在原始表来定义设置keyColumn变量的主键索引
  if( pColumn==0 && nColumn>0 )
  {
    keyColumn = pTab->iPKey;
  }

  //初始化要插入的行的数量
  if( db->flags & SQLITE_CountRows ){
    regRowCount = ++pParse->nMem;
    sqlite3VdbeAddOp2(v, OP_Integer, 0, regRowCount);
  }

  //如果这不是视图，打开表，和说有索引
  if( !isView )
  {
    int nIdx;

    baseCur = pParse->nTab;
    nIdx = sqlite3OpenTableAndIndices(pParse, pTab, baseCur, OP_OpenWrite);
    aRegIdx = sqlite3DbMallocRaw(db, sizeof(int)*(nIdx+1));
    if( aRegIdx==0 )
    {
      goto insert_cleanup;
    }
    for(i=0; i<nIdx; i++)
    {
      aRegIdx[i] = ++pParse->nMem;
    }
  }

  //这是主要插入循环的顶端部分。
  if( useTempTable )
  {
    //这部分代码仅仅是循环顶部的代码。完整的循环伪代码是如下面（模板4）：
    /*         rewind temp table
    **      C: loop over rows of intermediate table
    **           transfer values form intermediate table into <table>
    **         end loop
    **      D: ...
    */

    addrInsTop = sqlite3VdbeAddOp1(v, OP_Rewind, srcTab);
    addrCont = sqlite3VdbeCurrentAddr(v);
  }
  else if( pSelect )
  {
    //这部分代码仅仅是循环顶部的代码。完整的循环伪代码是如下面（模板3）：
    /*      C: yield X
    **         if EOF goto D
    **         insert the select result into <table> from R..R+n
    **         goto C
    **      D: ...
    */
    addrCont = sqlite3VdbeAddOp1(v, OP_Yield, dest.iSDParm);
    addrInsTop = sqlite3VdbeAddOp1(v, OP_If, regEof);
  }

  //给赋值新行的行号分配寄存器
  regRowid = regIns = pParse->nMem+1;
  pParse->nMem += pTab->nCol + 1;
  if( IsVirtual(pTab) )E
  {
    regRowid++;
    pParse->nMem++;
  }
  regData = regRowid+1;

  //运行BEFORE和INSTEA OF触发器如果有的话
  endOfLoop = sqlite3VdbeMakeLabel(v);
  if( tmask & TRIGGER_BEFORE )
  {
    int regCols = sqlite3GetTempRange(pParse, pTab->nCol+1);

    //构建新的。*表示相关的行。注意，如果这有一个要插入整型的主键是空的，
    //那么NULL将会转换成一个唯一的ID给这个行。但是在一个BEFORE触发器中，我们
    //不知道这唯一的ＩＤ是什么（因为插入还没发生），因此我们用－１代替行号。
    if( keyColumn<0 )
    {
      sqlite3VdbeAddOp2(v, OP_Integer, -1, regCols);
    }
    else
    {
      int j1;
      if( useTempTable )
      {
        sqlite3VdbeAddOp3(v, OP_Column, srcTab, keyColumn, regCols);
      }
      else
      {
        assert( pSelect==0 ); //否则useTempTable为真
        sqlite3ExprCode(pParse, pList->a[keyColumn].pExpr, regCols);
      }
      j1 = sqlite3VdbeAddOp1(v, OP_NotNull, regCols);
      sqlite3VdbeAddOp2(v, OP_Integer, -1, regCols);
      sqlite3VdbeJumpHere(v, j1);
      sqlite3VdbeAddOp1(v, OP_MustBeInt, regCols);
    }

    //不可能有触发器在一个虚拟表中。如果有的话，这块代码必须为隐藏列考虑。
    assert( !IsVirtual(pTab) );

    //创建一个新的列数据
    for(i=0; i<pTab->nCol; i++)
    {
      if( pColumn==0 )
      {
        j = i;
      }
      else
      {
        for(j=0; j<pColumn->nId; j++)
        {
          if( pColumn->a[j].idx==i )
            break;
        }
      }
      if( (!useTempTable && !pList) || (pColumn && j>=pColumn->nId) )
      {
        sqlite3ExprCode(pParse, pTab->aCol[i].pDflt, regCols+i+1);
      }
      else if( useTempTable )
      {
        sqlite3VdbeAddOp3(v, OP_Column, srcTab, j, regCols+i+1);
      }
      else{
        assert( pSelect==0 ); //否则useTempTable为真
        sqlite3ExprCodeAndCache(pParse, pList->a[j].pExpr, regCols+i+1);
      }
    }
    //如果这是一条在带有INSTEAD OF INSERT触发器的视图上插入，
    //在装配记录之前不尝试任何转换。如果这是一个真实的表，通过表的列相关必须尝试转换。
    if( !isView )
    {
      sqlite3VdbeAddOp2(v, OP_Affinity, regCols+1, pTab->nCol);
      sqlite3TableAffinityStr(v, pTab);
    }

    //激发BEFORE 或者 INSTEAD OF 触发器
    sqlite3CodeRowTrigger(pParse, pTrigger, TK_INSERT, 0, TRIGGER_BEFORE,
        pTab, regCols-pTab->nCol-1, onError, endOfLoop);

    sqlite3ReleaseTempRange(pParse, regCols, pTab->nCol+1);
  }

  //为新的实体把记录编号压入到堆栈中。记录编号是一个由NewRowid随机生成的整数，
  //除非当这个表有一个INTEGER PRIMARY KEY的列，在这中情况中，这个记录编号和列是相同的。
  if( !isView )
  {
    if( IsVirtual(pTab) )
    {
      //关于行的VUpdate操作代码没有的话，将会删除。
      sqlite3VdbeAddOp2(v, OP_Null, 0, regIns);
    }
    if( keyColumn>=0 )
    {
      if( useTempTable )
      {
        sqlite3VdbeAddOp3(v, OP_Column, srcTab, keyColumn, regRowid);
      }
      else if( pSelect )
      {
        sqlite3VdbeAddOp2(v, OP_SCopy, regFromSelect+keyColumn, regRowid);
      }
      else
      {
        VdbeOp *pOp;
        sqlite3ExprCode(pParse, pList->a[keyColumn].pExpr, regRowid);
        pOp = sqlite3VdbeGetOp(v, -1);
        if( ALWAYS(pOp) && pOp->opcode==OP_Null && !IsVirtual(pTab) )
        {
          appendFlag = 1;
          pOp->opcode = OP_NewRowid;
          pOp->p1 = baseCur;
          pOp->p2 = regRowid;
          pOp->p3 = regAutoinc;
        }
      }
      //如果这个主键表达式是空的，那么使用VUpdate生成一个唯一的主键值。
      if( !appendFlag )
      {
        int j1;
        if( !IsVirtual(pTab) )
        {
          j1 = sqlite3VdbeAddOp1(v, OP_NotNull, regRowid);
          sqlite3VdbeAddOp3(v, OP_NewRowid, baseCur, regRowid, regAutoinc);
          sqlite3VdbeJumpHere(v, j1);
        }
        else
        {
          j1 = sqlite3VdbeCurrentAddr(v);
          sqlite3VdbeAddOp2(v, OP_IsNull, regRowid, j1+2);
        }
        sqlite3VdbeAddOp1(v, OP_MustBeInt, regRowid);
      }
    }
    else if( IsVirtual(pTab) )
    {
      sqlite3VdbeAddOp2(v, OP_Null, 0, regRowid);
    }
    else
    {
      sqlite3VdbeAddOp3(v, OP_NewRowid, baseCur, regRowid, regAutoinc);
      appendFlag = 1;
    }
    autoIncStep(pParse, regAutoinc, regRowid);



    /* 新记录的所有列的数据，
    ** 从第一列开始写入栈中
    */
    nHidden = 0;
	//循环数据库列数
    for(i=0; i<pTab->nCol; i++){
      int iRegStore = regRowid+1+i;
      if( i==pTab->iPKey ){//在原始表中的主键索引是pTab->iPKey（列是主键列时）
        /* INTEGER主键列的值总是为NULL,时sqlite将会使用默认的替代方法代替此NULL，
        ** 使用自增长的策略产生值，这里是iRegStore的值
        ** 所以将填补这一列空来避免出错
		*/
        sqlite3VdbeAddOp2(v, OP_Null, 0, iRegStore);//调用Vdbeaux.c文件中的sqlite3VdbeAddOp2函数
        continue;
      }
      if( pColumn==0 ){
        if( IsHiddenColumn(&pTab->aCol[i]) ){//判断是否是隐藏列
          assert( IsVirtual(pTab) );//调用断言函数pTab是否是一个虚拟表
          j = -1;
          nHidden++;
        }else{
          j = i - nHidden;
        }
      }else{
        for(j=0; j<pColumn->nId; j++){
          if( pColumn->a[j].idx==i ) break;
        }
      }
      if( j<0 || nColumn==0 || (pColumn && j>=pColumn->nId) ){
        sqlite3ExprCode(pParse, pTab->aCol[i].pDflt, iRegStore);//将表达式解析并存储到对应的寄存器中，sqlite3ExprCode函数的三个参数是，1解析，2表达式，3目标寄存器
      }else if( useTempTable ){
        sqlite3VdbeAddOp3(v, OP_Column, srcTab, j, iRegStore); //Sqlite3vdbeAddOp函数有三个参数：（1）VDBE实例（它将添加指令），（2）操作码（一条指令），（3）两个操作数。
      }else if( pSelect ){
        sqlite3VdbeAddOp2(v, OP_SCopy, regFromSelect+j, iRegStore);
      }else{
        sqlite3ExprCode(pParse, pList->a[j].pExpr, iRegStore);
      }
    }

    /* 此代码生成检查约束并产生索引并进行插入。
    ** 如果不存在则定义如下
    */
#ifndef SQLITE_OMIT_VIRTUALTABLE
    if( IsVirtual(pTab) ){//是否是虚拟表（视图）
      const char *pVTab = (const char *)sqlite3GetVTable(db, pTab);  //产生一个常量指针指定pTab（pTab放入到相应的数组中）
      sqlite3VtabMakeWritable(pParse, pTab);  //pTab写入到内存数组中， 判定pParse->apVirtualLock[]是否存在，然后进行处理
      sqlite3VdbeAddOp4(v, OP_VUpdate, 1, pTab->nCol+2, regIns, pVTab, P4_VTAB);//向虚拟机代码中添加新的执行语句
      sqlite3VdbeChangeP5(v, onError==OE_Default ? OE_Abort : onError);//p5操作，将改变最多的加入其中
      sqlite3MayAbort(pParse);//改变解析，通过标志位来避免事务的交叉。
    }else
#endif
    {
      int isReplace;  //如果约束可能导致替换等其他操作则设置为true
      sqlite3GenerateConstraintChecks(pParse, pTab, baseCur, regIns, aRegIdx,
          keyColumn>=0, 0, onError, endOfLoop, &isReplace
      );//约束检查
      sqlite3FkCheck(pParse, pTab, 0, regIns);//外键检查
      sqlite3CompleteInsertion(
          pParse, pTab, baseCur, regIns, aRegIdx, 0, appendFlag, isReplace==0
      );//来完成更新或插入操作
    }
  }

  /* 更新插入的行数				
  */
  if( (db->flags & SQLITE_CountRows)!=0 ){
    sqlite3VdbeAddOp2(v, OP_AddImm, regRowCount, 1);
  }

  if( pTrigger ){
    //这段代码在触发器之后执行
    sqlite3CodeRowTrigger(pParse, pTrigger, TK_INSERT, 0, TRIGGER_AFTER, 
        pTab, regData-2-pTab->nCol, onError, endOfLoop);//行触发器，参数是（解析，触发代码，触发器的表，注册的值（有的话就是用老的，没有就新建，这里给出0），冲突策略，结束跳转）
  }

  /*  如果数据源是一条select语句,则这是insert循环的底部
  **  意味着不需要去循环读取insert语句之后的列（因为没有列名）
  */
  sqlite3VdbeResolveLabel(v, endOfLoop);//确定下一步的指令地址
  if( useTempTable ){
    sqlite3VdbeAddOp2(v, OP_Next, srcTab, addrCont);
    sqlite3VdbeJumpHere(v, addrInsTop);//改变指令操作数p2，下一个指令的地址编码
    sqlite3VdbeAddOp1(v, OP_Close, srcTab);
  }else if( pSelect ){
    sqlite3VdbeAddOp2(v, OP_Goto, 0, addrCont);
    sqlite3VdbeJumpHere(v, addrInsTop);
  }

  if( !IsVirtual(pTab) && !isView ){
    //关闭所有打开的表
    sqlite3VdbeAddOp1(v, OP_Close, baseCur);
    for(idx=1, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, idx++){
      sqlite3VdbeAddOp1(v, OP_Close, idx+baseCur);
    }
  }

insert_end:

  /* 通过存储rowid的最大值来更新sqlite_sequence表相关字段
  ** 这也属于自增长的表（其字段是自增长的）
  */
  if( pParse->nested==0 && pParse->pTriggerTab==0 ){
    sqlite3AutoincrementEnd(pParse);//最大rowid回sqlite_sequence中
  }

  /*
  ** 返回插入的行数。如果这个程序返回值由于调用了
  ** 函数sqlite3NestedParse()，将不会唤醒回调函数。
  ** 
  */
  if( (db->flags&SQLITE_CountRows) && !pParse->nested && !pParse->pTriggerTab ){//判断解析嵌套，触发器，行数等，来判定生成操作语句
    sqlite3VdbeAddOp2(v, OP_ResultRow, regRowCount, 1);
    sqlite3VdbeSetNumCols(v, 1);
    sqlite3VdbeSetColName(v, 0, COLNAME_NAME, "rows inserted", SQLITE_STATIC);
  }

insert_cleanup://清理
  sqlite3SrcListDelete(db, pTabList);//删除整个SrcList包括所有的子结构
  sqlite3ExprListDelete(db, pList); //删除整个表达式结构
  sqlite3SelectDelete(db, pSelect);//删除整个select包括所有的子结构
  sqlite3IdListDelete(db, pColumn);//删除IdList
  sqlite3DbFree(db, aRegIdx);//空闲内存,可能会被关联到一个特定的数据库
}

/* 确保“isView”和其他上面宏定义是未定义的
** 否则在这个文件中编辑其他功能会产生干扰
** （或在另一个文件,如果这个文件成为融合的一部分）
#ifdef isView
 #undef isView
#endif
#ifdef pTrigger
 #undef pTrigger
#endif
#ifdef tmask
 #undef tmask
#endif


/*
**  此代码做约束检查当一个插入或更新操作。
**
**  输入的范围的连贯性表现为以下：
**
**    1.   行ID更新在行更新之后
**
**    2.   数据的第一列在整个记录更新之后更新
**
**    i.   数据来自中间的列的...
**
**    N.  最后一列的数据在整个记录更新之后更新
**
**  那个regRowid参数是寄存器包含的索引
**
** 如果参数isUpdate是TRUE并且参数rowidChng不为零
** 则rowidChng包含一个寄存器地址包含rowid 在更新之前发生
** isUpdate参数是true表示更新操作，false表示插入操作，
** 如果isUpdate是false，表明一个insert语句，然而非零的rowidChng
** 参数表明rowid被显示的为insert语句的一部分，如果rowidChng是false
** 意味着rowid是自动计算的在一个插入或是rowid不需要修改的更新操作。
** 
**
**
** 这些代码产生由程序，存储新的索引记录到寄存器中通过
** aRegIdx[]数组来识别。没有记录索引被创建在aRegIdx[i]==0
** 时。指示的顺序在aRegIdx[]与链表（依附于表）的顺序一致，
** 
**
** 这个程序也会产生代码去检查约束。非空，
** 检查，唯一约束都会被检查。如果约束失效，
** 然后适当的动作被执行。有五种可能的动作
**  ROLLBACK（回滚）, ABORT（终止）, FAIL（失败）, REPLACE（代替）, and IGNORE（忽略）.
**
**        约束类型      动作       说明
**    ---------------------------------------------------------  
**     任何           回滚     当前的事务被回滚并且 sqlite3_exec()
**                                返回立即QLITE_CONSTRAINT的代码
**                                
**
**     任何           终止     撤销更改从当前的命令中，只（对于已完成的不进行回滚）
**                                然而 由于sqlite3_exec()会立即返回SQLITE_CONSTRAINT代码
**                               
**
**     任何          失败      Sqlite3_exec()立即返回一个SQLITE_CONSTRAINT
**                                   代码，事务不会回滚，并且任何之前的改变都会保留 
**                                
**
**     任何           忽略      记录的数量和数据出现在堆中，并且
**                                立即跳到标签 忽略其他。 
**                              
**     非空          代替       空值被默认值代替在那列值中，如果默认值为空   
**                                  这动作与终止相同。     
**                               
**     唯一           代替       其他行的冲突是被查入行被删掉
**                               
**
**     检查          代替        非法的，结果集是一个异常。 
**
** 哪一个动作被执行取决于被重写的错误的参数。
** 或者如果overrideError==OE_Default，则pParse->onError 参数被使用
** 或者如果pParse->onError==OE_Default则onError的值将被约束使用
**
** 调用程序必须打开读/写游标，pTab随着游标的数目计入到baseCur中。
** 所有的pTab指标必须打开读/写游标随着游标数目baseCur+i对第i个游标
*   
** 此外，如果不可能有一个代替动作，则游标不需要打开，在aRegIdx[i]==0。
.
*/
void sqlite3GenerateConstraintChecks(
  Parse *pParse,      //从词法分析器中获取指针，取词用
  Table *pTab,        //要进行插入的表
  int baseCur,        //读/写游标的索引在pTab
  int regRowid,       //输入寄存器的下标的范围
  int *aRegIdx,       //寄存器使用的下标，0是未被使用的指标
  int rowidChng,     //true如果rowid和存在记录冲突时
  int isUpdate,       //true是更新操作，false是插入操作
  int overrideError,  //重写onError到这，如果不是OE_Default
  int ignoreDest,     //跳到这个标签在一个OE_Ignore解决
  int *pbMayReplace   //输出，如果限制可能引起一个代替动作则设置真
){
  int i;              //循环计数器i
  Vdbe *v;            //虚拟数据库引擎的构建
  int nCol;           //列的数量 
  int onError;        //冲突的解决策略
  int j1;             //地址跳跃指令
  int j2 = 0, j3;     //地址跳跃指令  
  int regData;        //寄存器包含第一个数据列
  int iCur;           //表游标数量
  Index *pIdx;         //指针的一个指示   
  sqlite3 *db;         //数据库连接
  int seenReplace = 0;//如果代替被使用解决INT主键冲突则设为真值
  int regOldRowid = (rowidChng && isUpdate) ? rowidChng : regRowid;

  db = pParse->db;
  v = sqlite3GetVdbe(pParse);//得到虚拟数据库引擎
  assert( v!=0 );
  assert( pTab->pSelect==0 );  //这个表不是视图（通过assert判定）
  nCol = pTab->nCol;
  regData = regRowid + 1;

  /* 测试所有非空约束    
  */
  for(i=0; i<nCol; i++){
    if( i==pTab->iPKey ){
      continue;
    }
    onError = pTab->aCol[i].notNull;
    if( onError==OE_None ) continue;
    if( overrideError!=OE_Default ){
      onError = overrideError;
    }else if( onError==OE_Default ){
      onError = OE_Abort;
    }
    if( onError==OE_Replace && pTab->aCol[i].pDflt==0 ){
      onError = OE_Abort;
    }
    assert( onError==OE_Rollback || onError==OE_Abort || onError==OE_Fail
        || onError==OE_Ignore || onError==OE_Replace );
    switch( onError ){
      case OE_Abort:
        sqlite3MayAbort(pParse);
      case OE_Rollback:
      case OE_Fail: {
        char *zMsg;
        sqlite3VdbeAddOp3(v, OP_HaltIfNull,
                                  SQLITE_CONSTRAINT, onError, regData+i);
        zMsg = sqlite3MPrintf(db, "%s.%s may not be NULL",
                              pTab->zName, pTab->aCol[i].zName);//查看从sqliteMalloc()中取得的内存大小
        sqlite3VdbeChangeP4(v, -1, zMsg, P4_DYNAMIC);
        break;
      }
      case OE_Ignore: {
        sqlite3VdbeAddOp2(v, OP_IsNull, regData+i, ignoreDest);
        break;
      }
      default: {
        assert( onError==OE_Replace );
        j1 = sqlite3VdbeAddOp1(v, OP_NotNull, regData+i);
        sqlite3ExprCode(pParse, pTab->aCol[i].pDflt, regData+i);//评估解析到的语句，并存储到内存中
        sqlite3VdbeJumpHere(v, j1);
        break;
      }
    }
  }

  /* 测试所有检查约束
  */
#ifndef SQLITE_OMIT_CHECK
  if( pTab->pCheck && (db->flags & SQLITE_IgnoreChecks)==0 ){
    ExprList *pCheck = pTab->pCheck; //表达式列表（需要check的）
    pParse->ckBase = regData;
    onError = overrideError!=OE_Default ? overrideError : OE_Abort;
    for(i=0; i<pCheck->nExpr; i++){
      int allOk = sqlite3VdbeMakeLabel(v);
      sqlite3ExprIfTrue(pParse, pCheck->a[i].pExpr, allOk, SQLITE_JUMPIFNULL);
      if( onError==OE_Ignore ){
        sqlite3VdbeAddOp2(v, OP_Goto, 0, ignoreDest);
      }else{
        char *zConsName = pCheck->a[i].zName;
        if( onError==OE_Replace ) onError = OE_Abort; /* IMP: R-15569-63625 */        //
        if( zConsName ){
          zConsName = sqlite3MPrintf(db, "constraint %s failed", zConsName);
        }else{
          zConsName = 0;
        }
        sqlite3HaltConstraint(pParse, onError, zConsName, P4_DYNAMIC);//OP_Halt 使得虚拟数据库引擎返回一个SQLITE_CONSTRAINT的错误
      }
      sqlite3VdbeResolveLabel(v, allOk);
    }
  }
#endif /* !defined(SQLITE_OMIT_CHECK) */

  /* 如果我们有INTEGER主键，确定新记录的主键之前是不存在
  ** 除了，如果这是一个更新并且主键不改变这是可以的
  ** 
  */
  if( rowidChng ){
    onError = pTab->keyConf;
    if( overrideError!=OE_Default ){
      onError = overrideError;
    }else if( onError==OE_Default ){
      onError = OE_Abort;
    }
    
    if( isUpdate ){
      j2 = sqlite3VdbeAddOp3(v, OP_Eq, regRowid, 0, rowidChng);
    }
    j3 = sqlite3VdbeAddOp3(v, OP_NotExists, baseCur, 0, regRowid);
    switch( onError ){
      default: {
        onError = OE_Abort;
         //执行下面的分支语句找到适合的执行
		 }
      case OE_Rollback:
      case OE_Abort:
      case OE_Fail: {
        sqlite3HaltConstraint(
          pParse, onError, "PRIMARY KEY must be unique", P4_STATIC);
        break;
      }
      case OE_Replace: {
        /* 如果有删除触发器在这个表上并且
        ** 循环触发器标志设置了，调用GenerateRowDelete()
        ** 从表中删除冲突的数据行，这将启动触发器并且
        ** 删除表和b树索引的记录
        **
        ** 另外，如果没有触发器或是循环触发器标志没有设置
        ** 但是表有一个或多个索引，调用GenerateRowIndexDelete()
        ** 这将只删除b树索引记录，
        ** 当记录被插入时，表的b树记录将被新记录替换
        ** 
        **
        ** 如果GenerateRowDelete()或者GenerateRowIndexDelete()被调用
        ** 并且调用MultiWrite()去指出哪一个虚拟数据库引擎要求声明
        ** 回滚（如果那个声明在删除发生后被终止）
        ** 早期版本调用sqlite3MultiWrite()无论什么情况，
        ** 但是存在更多的可选择在这，允许一下声明： 
        **
        **   REPLACE INTO t(rowid) VALUES($newrowid)                                    
        **
        ** 运行没有声明日志，如果没有索引在表上。
        ** 
        */
        Trigger *pTrigger = 0;
        if( db->flags&SQLITE_RecTriggers ){
          pTrigger = sqlite3TriggersExist(pParse, pTab, TK_DELETE, 0, 0);//返回这张表上的触发器的列表
        }
        if( pTrigger || sqlite3FkRequired(pParse, pTab, 0, 0) ){
          sqlite3MultiWrite(pParse);
          sqlite3GenerateRowDelete(
              pParse, pTab, baseCur, regRowid, 0, pTrigger, OE_Replace
          );
        }else if( pTab->pIndex ){
          sqlite3MultiWrite(pParse);//多重操作写记录
          sqlite3GenerateRowIndexDelete(pParse, pTab, baseCur, 0);//引起vdbe删除所有单一索引，邮编所在的当前记录
        }
        seenReplace = 1;
        break;
      }
      case OE_Ignore: {
        assert( seenReplace==0 );
        sqlite3VdbeAddOp2(v, OP_Goto, 0, ignoreDest);
        break;
      }
    }
    sqlite3VdbeJumpHere(v, j3);
    if( isUpdate ){
      sqlite3VdbeJumpHere(v, j2);
    }
  }

  /* 测试所有唯一约束，通过创建记录为每一个唯一的
  ** 索引并且确定， 重复的记录不会存在  
  ** 添加新的记录
  */
  for(iCur=0, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, iCur++){
    int regIdx;
    int regR;

    if( aRegIdx[iCur]==0 ) continue;     //跳过不用的指标

   //创建一个键来访问索引记录
    regIdx = sqlite3GetTempRange(pParse, pIdx->nColumn+1);//连续分配或释放一块pIdx->nColumn+1寄存器给pParse
    for(i=0; i<pIdx->nColumn; i++){
      int idx = pIdx->aiColumn[i];
      if( idx==pTab->iPKey ){
        sqlite3VdbeAddOp2(v, OP_SCopy, regRowid, regIdx+i);
      }else{
        sqlite3VdbeAddOp2(v, OP_SCopy, regData+idx, regIdx+i);
      }
    }
    sqlite3VdbeAddOp2(v, OP_SCopy, regRowid, regIdx+i);
    sqlite3VdbeAddOp3(v, OP_MakeRecord, regIdx, pIdx->nColumn+1, aRegIdx[iCur]);
    sqlite3VdbeChangeP4(v, -1, sqlite3IndexAffinityStr(v, pIdx), P4_TRANSIENT);
    sqlite3ExprCacheAffinityChange(pParse, regIdx, pIdx->nColumn+1);//记录变化的寄存器

    //找出什么动作会引起索引冲突
    onError = pIdx->onError;
    if( onError==OE_None ){ 
      sqlite3ReleaseTempRange(pParse, regIdx, pIdx->nColumn+1);
      continue;  /* pIdx is not a UNIQUE index */
    }
    if( overrideError!=OE_Default ){
      onError = overrideError;
    }else if( onError==OE_Default ){
      onError = OE_Abort;
    }
    if( seenReplace ){
      if( onError==OE_Ignore ) onError = OE_Replace;
      else if( onError==OE_Fail ) onError = OE_Abort;
    }
    
    // 查看是新的索引记录是否是唯一的
    regR = sqlite3GetTempReg(pParse);//分配一个寄存器使其暂存相关的中间结果
    sqlite3VdbeAddOp2(v, OP_SCopy, regOldRowid, regR);
    j3 = sqlite3VdbeAddOp4(v, OP_IsUnique, baseCur+iCur+1, 0,
                           regR, SQLITE_INT_TO_PTR(regIdx),
                           P4_INT32);
    sqlite3ReleaseTempRange(pParse, regIdx, pIdx->nColumn+1);//释放所占用的寄存器

    // 如果新索引记录不是唯一的，此代码将执行
    assert( onError==OE_Rollback || onError==OE_Abort || onError==OE_Fail
        || onError==OE_Ignore || onError==OE_Replace );
    switch( onError ){
      case OE_Rollback:
      case OE_Abort:
      case OE_Fail: {
        int j;
        StrAccum errMsg;
        const char *zSep;
        char *zErr;

        sqlite3StrAccumInit(&errMsg, 0, 0, 200);  //初始化字符串池，（在此应该是用来生成一些错误信息）
        errMsg.db = db;
        zSep = pIdx->nColumn>1 ? "columns " : "column ";
        for(j=0; j<pIdx->nColumn; j++){
          char *zCol = pTab->aCol[pIdx->aiColumn[j]].zName;
          sqlite3StrAccumAppend(&errMsg, zSep, -1);  //构造错误信息
          zSep = ", ";
          sqlite3StrAccumAppend(&errMsg, zCol, -1);
        }
        sqlite3StrAccumAppend(&errMsg,
            pIdx->nColumn>1 ? " are not unique" : " is not unique", -1);
        zErr = sqlite3StrAccumFinish(&errMsg);  //构造完成，返回准确地错误信息
        sqlite3HaltConstraint(pParse, onError, zErr, 0);
        sqlite3DbFree(errMsg.db, zErr);
        break;
      }
      case OE_Ignore: {
        assert( seenReplace==0 );
        sqlite3VdbeAddOp2(v, OP_Goto, 0, ignoreDest);
        break;
      }
      default: {
        Trigger *pTrigger = 0;
        assert( onError==OE_Replace );
        sqlite3MultiWrite(pParse);
        if( db->flags&SQLITE_RecTriggers ){
          pTrigger = sqlite3TriggersExist(pParse, pTab, TK_DELETE, 0, 0);
        }
        sqlite3GenerateRowDelete(
            pParse, pTab, baseCur, regR, 0, pTrigger, OE_Replace
        );
        seenReplace = 1;
        break;
      }
    }
    sqlite3VdbeJumpHere(v, j3);
    sqlite3ReleaseTempReg(pParse, regR);
  }
  
  if( pbMayReplace ){
    *pbMayReplace = seenReplace;
  }
}

/*
** 程序产生代码来完成插入或者更新操作
** 开始通过之前称为sqlite3GenerateConstraintChecks
** 寄存器连续的范围开始在regRowid	
** 包含rowid和要被插入的数据
**
** 程序的参数应该和sqlite3GenerateConstraintChecks的前六个参数一样
** 
*/
void sqlite3CompleteInsertion(
  Parse *pParse,      //从词法分析器获取词
  Table *pTab,       //我们要插入的表
  int baseCur,        //读/写指针指向pTab指数
  int regRowid,       //内容的范围
  int *aRegIdx,       //寄存器使用的每一个索引，0表示未被使用索引 
  int isUpdate,       //TRUE是更新，FALSE是插入  
  int appendBias,     //如果这可能是一个附加，则值为TRUE
  int useSeekResult   //在OP_[Idx]插入，TRUE设置USESEEKRESULT标记
){
  int i;
  Vdbe *v;
  int nIdx;
  Index *pIdx;
  u8 pik_flags;
  int regData;
  int regRec;

  v = sqlite3GetVdbe(pParse);
  assert( v!=0 );
  assert( pTab->pSelect==0 );  /* 这不是视图（此标志判断）*/
  for(nIdx=0, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, nIdx++){}
  for(i=nIdx-1; i>=0; i--){
    if( aRegIdx[i]==0 ) continue;
    sqlite3VdbeAddOp2(v, OP_IdxInsert, baseCur+i+1, aRegIdx[i]);
    if( useSeekResult ){
      sqlite3VdbeChangeP5(v, OPFLAG_USESEEKRESULT);
    }
  }
  regData = regRowid + 1;
  regRec = sqlite3GetTempReg(pParse);
  sqlite3VdbeAddOp3(v, OP_MakeRecord, regData, pTab->nCol, regRec);
  sqlite3TableAffinityStr(v, pTab);  //表和与列相关的字符关联在一起。
  sqlite3ExprCacheAffinityChange(pParse, regData, pTab->nCol);//记录一些真实地变化
  if( pParse->nested ){
    pik_flags = 0;
  }else{
    pik_flags = OPFLAG_NCHANGE;
    pik_flags |= (isUpdate?OPFLAG_ISUPDATE:OPFLAG_LASTROWID);//更新标志，如果是更新则是更新的rowid，否者是最后的rowid表示插入
  }
  if( appendBias ){
    pik_flags |= OPFLAG_APPEND;
  }
  if( useSeekResult ){
    pik_flags |= OPFLAG_USESEEKRESULT;
  }
  sqlite3VdbeAddOp3(v, OP_Insert, baseCur, regRec, regRowid);
  if( !pParse->nested ){
    sqlite3VdbeChangeP4(v, -1, pTab->zName, P4_TRANSIENT);
  }
  sqlite3VdbeChangeP5(v, pik_flags);
}

/*
** 此代码将打开一个表的游标并且表的所有指标
** baseCur参数是表所使用的游标的数量
** 在随后的游标索引被打开。
**
** 返回表的指标的数量
*/
int sqlite3OpenTableAndIndices(
  Parse *pParse,   //从词法分析器获取词
  Table *pTab,     //表被打开
  int baseCur,     //游标数量分配到表
  int op           //OP_OpenRead 或者 OP_OpenWrite
){
  int i;
  int iDb;
  Index *pIdx;
  Vdbe *v;

  if( IsVirtual(pTab) ) return 0;
  iDb = sqlite3SchemaToIndex(pParse->db, pTab->pSchema);//转变指针模式，变成pParse->db的模式
  v = sqlite3GetVdbe(pParse);
  assert( v!=0 );//判断得到vdbe
  sqlite3OpenTable(pParse, baseCur, iDb, pTab, op);
  for(i=1, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, i++){
    KeyInfo *pKey = sqlite3IndexKeyinfo(pParse, pIdx);
    assert( pIdx->pSchema==pTab->pSchema );
    sqlite3VdbeAddOp4(v, op, i+baseCur, pIdx->tnum, iDb,
                      (char*)pKey, P4_KEYINFO_HANDOFF);
    VdbeComment((v, "%s", pIdx->zName));
  }
  if( pParse->nTab<baseCur+i ){
    pParse->nTab = baseCur+i;
  }
  return i-1;
}


#ifdef SQLITE_TEST
/*
** 下一个全局变量被增加，当转移优化被使用时
** 在这被用来测试目标
** 确保转移最优真正存在这假设的时候出现
** 
*/
int sqlite3_xferopt_count;
#endif /* SQLITE_TEST */


#ifndef SQLITE_OMIT_XFER_OPT
/*
** 检查校对名字看他们是否可兼容的
*/
static int xferCompatibleCollation(const char *z1, const char *z2){
  if( z1==0 ){
    return z2==0;
  }
  if( z2==0 ){
    return 0;
  }
  return sqlite3StrICmp(z1, z2)==0;
}


/*
** 查看索引pSrc是否是可兼容的作为数据源
** 为了索引pDest在一个插入转移最优中。 
** 可兼容的索引规则： 
**
**    *   该索引是在相同的列集合中
**    *   DESC 和 ASC 相同标记在所有列上
**    *   相同的onError加工过程  
**    *   相同的校准顺序在每一列上
*/
static int xferCompatibleIndex(Index *pDest, Index *pSrc){
  int i;
  assert( pDest && pSrc );
  assert( pDest->pTable!=pSrc->pTable );
  if( pDest->nColumn!=pSrc->nColumn ){
    return 0;   //不同数量的列
  }
  if( pDest->onError!=pSrc->onError ){
    return 0;   //不同的碰撞解决策略
  }
  for(i=0; i<pSrc->nColumn; i++){
    if( pSrc->aiColumn[i]!=pDest->aiColumn[i] ){
      return 0;   //不同的列索引
    }
    if( pSrc->aSortOrder[i]!=pDest->aSortOrder[i] ){
      return 0;   //不同的排序
    }
    if( !xferCompatibleCollation(pSrc->azColl[i],pDest->azColl[i]) ){
      return 0;   //不同的核对序列 
    }
  }

  //如果以上的测试不出错则 索引是可兼容的  
  return 1;
}

/*
** 试图以最优化的方法插入表单中
**
**     INSERT INTO tab1 SELECT * FROM tab2; 例如语句
**
** 从tab2中传行记录较优
** 列不用解析和重新聚集，这样有很大的性能提升。  
** 未加索引的也是这样进行记录插入的
**
** 这种最优化只是试图tab1与tab2可兼容的
** 有许多规则决定兼容性（详细请看代码详解）
**
** 如果最优化被使用，这套程序返回true
** 有时转移（xfer）最优化只会在目标table为空时才工作，
** 一个因素是只有在运行时才能被检测到。由于这个原因，本程序
** 编码为了转移（xfer）最优,但是做一个测试来看是否目标table
** 为空，并且跳过xfer最优的代码，表明测试失败。在这种情，程序返回
** FALSE，所以调用者将知道往下执行，调用一个不优化的转移。这个程序返回  
** FALSE，是没有机会让xfer最优被使用。
**
** 这种最优在making VACUUM执行快速尤其有用
*/
static int xferOptimization(
  Parse *pParse,        //从词法分析器获取词
  Table *pDest,         //插入的table
  Select *pSelect,      //一个select语句用来作为数据源
  int onError,          //怎样控制约束错误
  int iDbDest           //pdest数据库，（即插入表的数据库）  
){
  ExprList *pEList;                //select的结果集
  Table *pSrc;                     //from子句的select table 
  Index *pSrcIdx, *pDestIdx;       //源和目标目录
  struct SrcList_item *pItem;      //元素，pSelect->pSrc（from子句的select table ）
  int i;                           //遍历变量计数i
  int iDbSrc;                      //psrc的数据库
  int iSrc, iDest;                 //游标从源到目标
  int addr1, addr2;                //遍历地址  
  int emptyDestTest;               //空pDest的测试地址
  int emptySrcTest;                //空pSrc的测试地址
  Vdbe *v;                         //虚拟数据库引擎建立
  KeyInfo *pKey;                   //索引的关键信息
  int regAutoinc;                  //存储寄存器使用AUTOINC
  int destHasUniqueIdx = 0;        //true 如果pDest有唯一索引
  int regData, regRowid;           //寄存器持有数据和行id（data和rowid）

  if( pSelect==0 ){
    return 0;   //必须是INSERT INTO ... SELECT ...的形式
  }
  if( sqlite3TriggerList(pParse, pDest) ){
    return 0;   //tab1不可以有触发器
  }
#ifndef SQLITE_OMIT_VIRTUALTABLE
  if( pDest->tabFlags & TF_Virtual ){
    return 0;   //tab1不可以是虚表
  }
#endif
  if( onError==OE_Default ){
    if( pDest->iPKey>=0 ) onError = pDest->keyConf;
    if( onError==OE_Default ) onError = OE_Abort;
  }
  assert(pSelect->pSrc);   //分配即使没有from子句 
  if( pSelect->pSrc->nSrc!=1 ){
    return 0;   //from子句必须有有一项
  }
  if( pSelect->pSrc->a[0].pSelect ){
    return 0;   //from子句不含有子查询
  }
  if( pSelect->pWhere ){
    return 0;   //select 没有一个where子句
  }
  if( pSelect->pOrderBy ){
    return 0;   //select 没有一个order by子句
  }
  /* 不需要测试having子句，如果having子句出现，但是没有orderby
  ** 我们将得到一个错误。*/                                
  if( pSelect->pGroupBy ){
    return 0;   select 没有一个group by子句
  }
  if( pSelect->pLimit ){
    return 0;   //select 没有一个limit子句
  }
  assert( pSelect->pOffset==0 );  //一定如此如果pLimit==0
  if( pSelect->pPrior ){
    return 0;   //select 不可能有复合查询
  }
  if( pSelect->selFlags & SF_Distinct ){
    return 0;   //select 不可能有distinct
  }
  pEList = pSelect->pEList;
  assert( pEList!=0 );
  if( pEList->nExpr!=1 ){
    return 0;   //结果集必须恰好只有一列
  }
  assert( pEList->a[0].pExpr );
  if( pEList->a[0].pExpr->op!=TK_ALL ){
    return 0;   //结果集一定有特别的操作符“*”
  }

  /* 此时我们建立的声明正确的语法形式
  ** 添加到这种最优，现在我们必须检查语义。
  ** 
  */
  pItem = pSelect->pSrc->a;
  pSrc = sqlite3LocateTable(pParse, 0, pItem->zName, pItem->zDatabase);
  if( pSrc==0 ){
    return 0;       //from子句不含有真实的表
  }
  if( pSrc==pDest ){
    return 0;      //tab1和tab2可能不是相同的表
  }
#ifndef SQLITE_OMIT_VIRTUALTABLE
  if( pSrc->tabFlags & TF_Virtual ){
    return 0;      //tab2一定不能是虚拟表
  }
#endif
  if( pSrc->pSelect ){
    return 0;   //tab2不可能是视图
  }
  if( pDest->nCol!=pSrc->nCol ){
    return 0;   //tab1和tab2的列的数目一定是相同的
  }
  if( pDest->iPKey!=pSrc->iPKey ){
    return 0;   //两张表一定有相同的INTEGER PRIMARY KEY（INTEGER主键）
  }
  for(i=0; i<pDest->nCol; i++){
    if( pDest->aCol[i].affinity!=pSrc->aCol[i].affinity ){
      return 0;    //类型，一定是相同在所有的列上对应相同
    }
    if( !xferCompatibleCollation(pDest->aCol[i].zColl, pSrc->aCol[i].zColl) ){
      return 0;    //核对列的序列一定是在所有列上是一致的
    }
    if( pDest->aCol[i].notNull && !pSrc->aCol[i].notNull ){
      return 0;    //tab2一定是非空如果tab1是    
    }
  }
  for(pDestIdx=pDest->pIndex; pDestIdx; pDestIdx=pDestIdx->pNext){
    if( pDestIdx->onError!=OE_None ){
      destHasUniqueIdx = 1;
    }
    for(pSrcIdx=pSrc->pIndex; pSrcIdx; pSrcIdx=pSrcIdx->pNext){
      if( xferCompatibleIndex(pDestIdx, pSrcIdx) ) break;
    }
    if( pSrcIdx==0 ){
      return 0;      //目标目录没有相应的索引在源处
    }
  }
#ifndef SQLITE_OMIT_CHECK
  if( pDest->pCheck && sqlite3ExprListCompare(pSrc->pCheck, pDest->pCheck) ){
    return 0;     //表有不同的的检查约束，Ticket #2252
  }
#endif
#ifndef SQLITE_OMIT_FOREIGN_KEY
  /* 如果目标表中含有外键约束，不允许传递优化。
  ** 这是必要的限制
  ** 但是主要的转移优化的受益者是VACUUM指令，
  ** 但是VACUUM指令不能够有外键约束。 
  ** 所以额外的复杂使得这个规则少的限制可能不值得这样的花费
  ** Ticket [6284df89debdfa61db8073e062908af0c9b6118e]
  */
  if( (pParse->db->flags & SQLITE_ForeignKeys)!=0 && pDest->pFKey!=0 ){
    return 0;
  }
#endif
  if( (pParse->db->flags & SQLITE_CountRows)!=0 ){
    return 0;  //xfer最优化不会表现好的 随着 PRAGMA count_changes
  }

  /* 如果我们达到这个，这意味着xfer最优化至少是有可能的，
  ** 尽管可能生效如果目标表（tab1）最初是空。
  ** 
  */
#ifdef SQLITE_TEST
  sqlite3_xferopt_count++;
#endif
  iDbSrc = sqlite3SchemaToIndex(pParse->db, pSrc->pSchema);
  v = sqlite3GetVdbe(pParse);
  sqlite3CodeVerifySchema(pParse, iDbSrc);//模式验证在顶级的vdbe验证iDbSrc
  iSrc = pParse->nTab++;
  iDest = pParse->nTab++;
  regAutoinc = autoIncBegin(pParse, iDbDest, pDest);//找到或创建一个表pTab AutoincInfo结构联系在一起,在数据库iDb。返回注册的注册号码,即最大rowid。
  sqlite3OpenTable(pParse, iDest, iDbDest, pDest, OP_OpenWrite);
  if( (pDest->iPKey<0 && pDest->pIndex!=0)          /* (1) */
   || destHasUniqueIdx                              /* (2) */
   || (onError!=OE_Abort && onError!=OE_Rollback)   /* (3) */
  ){
    /* 在某些环境，我们能够使用xfer来最优化，只要目标表初始化为空即可，
    ** 由这些编码作出决定.但是条件是目标表必须是空。      
    **
    ** (1) 没有INTEGER主键但是有其他索引
    **     （如果目标初始化不是为空，行的ID字段的索引可能需要改变）
    **
    ** (2) 目标有唯一索引（xfer最优化不能检测唯一性）
    **
    ** (3) 出错是有一些东西除了OE_Abort 和 OE_Rollback
    */
    addr1 = sqlite3VdbeAddOp2(v, OP_Rewind, iDest, 0);
    emptyDestTest = sqlite3VdbeAddOp2(v, OP_Goto, 0, 0);
    sqlite3VdbeJumpHere(v, addr1);
  }else{
    emptyDestTest = 0;
  }
  sqlite3OpenTable(pParse, iSrc, iDbSrc, pSrc, OP_OpenRead);
  emptySrcTest = sqlite3VdbeAddOp2(v, OP_Rewind, iSrc, 0);
  regData = sqlite3GetTempReg(pParse);
  regRowid = sqlite3GetTempReg(pParse);
  if( pDest->iPKey>=0 ){
    addr1 = sqlite3VdbeAddOp2(v, OP_Rowid, iSrc, regRowid);
    addr2 = sqlite3VdbeAddOp3(v, OP_NotExists, iDest, 0, regRowid);
    sqlite3HaltConstraint(
        pParse, onError, "PRIMARY KEY must be unique", P4_STATIC);
    sqlite3VdbeJumpHere(v, addr2);
    autoIncStep(pParse, regAutoinc, regRowid);//更新的最大rowid自动增量计算
  }else if( pDest->pIndex==0 ){
    addr1 = sqlite3VdbeAddOp2(v, OP_NewRowid, iDest, regRowid);
  }else{
    addr1 = sqlite3VdbeAddOp2(v, OP_Rowid, iSrc, regRowid);
    assert( (pDest->tabFlags & TF_Autoincrement)==0 );
  }
  sqlite3VdbeAddOp2(v, OP_RowData, iSrc, regData);
  sqlite3VdbeAddOp3(v, OP_Insert, iDest, regData, regRowid);
  sqlite3VdbeChangeP5(v, OPFLAG_NCHANGE|OPFLAG_LASTROWID|OPFLAG_APPEND);
  sqlite3VdbeChangeP4(v, -1, pDest->zName, 0);
  sqlite3VdbeAddOp2(v, OP_Next, iSrc, addr1);
  for(pDestIdx=pDest->pIndex; pDestIdx; pDestIdx=pDestIdx->pNext){
    for(pSrcIdx=pSrc->pIndex; ALWAYS(pSrcIdx); pSrcIdx=pSrcIdx->pNext){
      if( xferCompatibleIndex(pDestIdx, pSrcIdx) ) break; //判断两种索引是否是可以兼容的
    }
    assert( pSrcIdx );
    sqlite3VdbeAddOp2(v, OP_Close, iSrc, 0);
    sqlite3VdbeAddOp2(v, OP_Close, iDest, 0);
    pKey = sqlite3IndexKeyinfo(pParse, pSrcIdx);
    sqlite3VdbeAddOp4(v, OP_OpenRead, iSrc, pSrcIdx->tnum, iDbSrc,
                      (char*)pKey, P4_KEYINFO_HANDOFF);
    VdbeComment((v, "%s", pSrcIdx->zName));
    pKey = sqlite3IndexKeyinfo(pParse, pDestIdx);
    sqlite3VdbeAddOp4(v, OP_OpenWrite, iDest, pDestIdx->tnum, iDbDest,
                      (char*)pKey, P4_KEYINFO_HANDOFF);
    VdbeComment((v, "%s", pDestIdx->zName));
    addr1 = sqlite3VdbeAddOp2(v, OP_Rewind, iSrc, 0);
    sqlite3VdbeAddOp2(v, OP_RowKey, iSrc, regData);
    sqlite3VdbeAddOp3(v, OP_IdxInsert, iDest, regData, 1);
    sqlite3VdbeAddOp2(v, OP_Next, iSrc, addr1+1);
    sqlite3VdbeJumpHere(v, addr1);
  }
  sqlite3VdbeJumpHere(v, emptySrcTest);
  sqlite3ReleaseTempReg(pParse, regRowid);
  sqlite3ReleaseTempReg(pParse, regData);
  sqlite3VdbeAddOp2(v, OP_Close, iSrc, 0);
  sqlite3VdbeAddOp2(v, OP_Close, iDest, 0);
  if( emptyDestTest ){
    sqlite3VdbeAddOp2(v, OP_Halt, SQLITE_OK, 0);
    sqlite3VdbeJumpHere(v, emptyDestTest);
    sqlite3VdbeAddOp2(v, OP_Close, iDest, 0);
    return 0;
  }else{
    return 1;
  }
}
#endif /* SQLITE_OMIT_XFER_OPT */
