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
//2014年12月11日 完成前925行的源码阅读与简单分析
/*在SQLite中，这个实现文件内容是实现由解析器调用C代码处理插入操作的代码例程。*/
#include "sqliteInt.h"

////1、sqlite3OpenTable()函数实现打开一张用于读取数据操作的数据库表.
//输入参数依次：指向解析器的指针p，表中游标的数量，在数据库连接实例索引，指向数据库表的指针pTab，读表或是写表操作
void sqlite3OpenTable(Parse *p, int iCur, int iDb, Table *pTab, int opcode ）
{

  Vdbe *v;//指向虚拟机的指针v

  if( IsVirtual(pTab) ) //判断是否虚拟表
    return;

  //在给定的解析器的情况下，获得一个虚拟机指针，必要时创建一个新的，若出现错误，将会把NULL和相关错误信息赋值给v
  v = sqlite3GetVdbe(p);

  assert( opcode==OP_OpenWrite || opcode==OP_OpenRead ); //调用断言函数检查传入参数是否合法

  //在运行时，把表锁定；相应解析器环境；数据库索引包含被锁的表；表的根页面也被锁定；判断是否为写锁；锁定表的表名。
  sqlite3TableLock(p, iDb, pTab->tnum, (opcode==OP_OpenWrite)?1:0, pTab->zName);

  //给当前的虚拟机指令列表中添加一条新的指令，返回新指令的地址。(解析器，指令的操作码，游标操作数，根节点操作数，数据库索引操作数)
  sqlite3VdbeAddOp3(v, opcode, iCur, pTab->tnum, iDb);

  //把操作数P4的值改变为一个特殊的指令。
  //如果P4_INT32>=0，则P4是动态的, 就是说使用sqlite3_malloc()动态分配函数获得一个字符串。即等于0时，复制第三个参数值个字节，包括其第一个空字节；大于0复制其（n+1）个字节。
  //若P4_INT32==P4_KEYINFO,意味着zP4结构是指针，该结构通过sqlite3_malloc()存入到内存中，当虚拟机完成是释放。-1小于0表明改变P4最近添加的指令值。
  sqlite3VdbeChangeP4(v, -1, SQLITE_INT_TO_PTR(pTab->nCol), P4_INT32);

  VdbeComment((v, "%s", pTab->zName));//虚拟机中当前表的注解打印
}

////2、返回一个与索引pIdx相关的列相关字符串的指针的值而不用修改它，
//根据列关联，表中每一个列都有一个字符代表列关联的数据类型。
const char *sqlite3IndexAffinityStr(Vdbe *v, Index *pIdx)
{
  //对于一个特定的索引来说，一个列关联字符串在第一次才需要分配和赋值的。
  //然后对于后续的使用来说，它将会作为一个索引的成员存储.
  //当索引结构本身被清除了的时候，最终这个列关联的字符串通过sqliteDeleteIndex()函数删除。
  if( !pIdx->zColAff )
  {

    int n;
    Table *pTab = pIdx->pTable;//SQL表索引
    sqlite3 *db = sqlite3VdbeDb(v);//定义一个当前虚拟机运行的数据库数据实例的指针

    //对于数据的索引一旦开始分配，失败终止只有当失败分配重置。
    pIdx->zColAff = (char *)sqlite3DbMallocRaw(0, pIdx->nColumn+2);
    if( !pIdx->zColAff )
    {
      db->mallocFailed = 1;
      return 0;
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

////3.该函数主要是用于把表和与列相关的字符关联在一起。
void sqlite3TableAffinityStr(Vdbe *v, Table *pTab)
{
    /* 对于一个特定的索引来说，一个列关联字符串在第一次才需要分配和赋值的。
    **然后对于后续的使用，它将会作为一个索引结构成员存储.
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
**如果表pTab在数据库索引数组中，或是它的索引在任何时候被打开在VDBE程序中的开始位置到结束位置的话，返回非零值。
这是用于检查"INSERT INTO <iDb, pTab> SELECT ..."规则的语句是否对于查询结构没有临时表的情况下可以运行
*/
////4.申明一个静态函数仅限于表文件调用。
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

    /* Push onto the stack, data for all columns of the new entry, beginning
    ** with the first column.
    */
    nHidden = 0;
    for(i=0; i<pTab->nCol; i++){
      int iRegStore = regRowid+1+i;
      if( i==pTab->iPKey ){
        /* The value of the INTEGER PRIMARY KEY column is always a NULL.
        ** Whenever this column is read, the record number will be substituted
        ** in its place.  So will fill this column with a NULL to avoid
        ** taking up data space with information that will never be used. */
        sqlite3VdbeAddOp2(v, OP_Null, 0, iRegStore);
        continue;
      }
      if( pColumn==0 ){
        if( IsHiddenColumn(&pTab->aCol[i]) ){
          assert( IsVirtual(pTab) );
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
        sqlite3ExprCode(pParse, pTab->aCol[i].pDflt, iRegStore);
      }else if( useTempTable ){
        sqlite3VdbeAddOp3(v, OP_Column, srcTab, j, iRegStore); 
      }else if( pSelect ){
        sqlite3VdbeAddOp2(v, OP_SCopy, regFromSelect+j, iRegStore);
      }else{
        sqlite3ExprCode(pParse, pList->a[j].pExpr, iRegStore);
      }
    }

    /* Generate code to check constraints and generate index keys and
    ** do the insertion.
    */
#ifndef SQLITE_OMIT_VIRTUALTABLE
    if( IsVirtual(pTab) ){
      const char *pVTab = (const char *)sqlite3GetVTable(db, pTab);
      sqlite3VtabMakeWritable(pParse, pTab);
      sqlite3VdbeAddOp4(v, OP_VUpdate, 1, pTab->nCol+2, regIns, pVTab, P4_VTAB);
      sqlite3VdbeChangeP5(v, onError==OE_Default ? OE_Abort : onError);
      sqlite3MayAbort(pParse);
    }else
#endif
    {
      int isReplace;    /* Set to true if constraints may cause a replace */
      sqlite3GenerateConstraintChecks(pParse, pTab, baseCur, regIns, aRegIdx,
          keyColumn>=0, 0, onError, endOfLoop, &isReplace
      );
      sqlite3FkCheck(pParse, pTab, 0, regIns);
      sqlite3CompleteInsertion(
          pParse, pTab, baseCur, regIns, aRegIdx, 0, appendFlag, isReplace==0
      );
    }
  }

  /* Update the count of rows that are inserted
  */
  if( (db->flags & SQLITE_CountRows)!=0 ){
    sqlite3VdbeAddOp2(v, OP_AddImm, regRowCount, 1);
  }

  if( pTrigger ){
    /* Code AFTER triggers */
    sqlite3CodeRowTrigger(pParse, pTrigger, TK_INSERT, 0, TRIGGER_AFTER, 
        pTab, regData-2-pTab->nCol, onError, endOfLoop);
  }

  /* The bottom of the main insertion loop, if the data source
  ** is a SELECT statement.
  */
  sqlite3VdbeResolveLabel(v, endOfLoop);
  if( useTempTable ){
    sqlite3VdbeAddOp2(v, OP_Next, srcTab, addrCont);
    sqlite3VdbeJumpHere(v, addrInsTop);
    sqlite3VdbeAddOp1(v, OP_Close, srcTab);
  }else if( pSelect ){
    sqlite3VdbeAddOp2(v, OP_Goto, 0, addrCont);
    sqlite3VdbeJumpHere(v, addrInsTop);
  }

  if( !IsVirtual(pTab) && !isView ){
    /* Close all tables opened */
    sqlite3VdbeAddOp1(v, OP_Close, baseCur);
    for(idx=1, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, idx++){
      sqlite3VdbeAddOp1(v, OP_Close, idx+baseCur);
    }
  }

insert_end:
  /* Update the sqlite_sequence table by storing the content of the
  ** maximum rowid counter values recorded while inserting into
  ** autoincrement tables.
  */
  if( pParse->nested==0 && pParse->pTriggerTab==0 ){
    sqlite3AutoincrementEnd(pParse);
  }

  /*
  ** Return the number of rows inserted. If this routine is 
  ** generating code because of a call to sqlite3NestedParse(), do not
  ** invoke the callback function.
  */
  if( (db->flags&SQLITE_CountRows) && !pParse->nested && !pParse->pTriggerTab ){
    sqlite3VdbeAddOp2(v, OP_ResultRow, regRowCount, 1);
    sqlite3VdbeSetNumCols(v, 1);
    sqlite3VdbeSetColName(v, 0, COLNAME_NAME, "rows inserted", SQLITE_STATIC);
  }

insert_cleanup:
  sqlite3SrcListDelete(db, pTabList);
  sqlite3ExprListDelete(db, pList);
  sqlite3SelectDelete(db, pSelect);
  sqlite3IdListDelete(db, pColumn);
  sqlite3DbFree(db, aRegIdx);
}

/* Make sure "isView" and other macros defined above are undefined. Otherwise
** thely may interfere with compilation of other functions in this file
** (or in another file, if this file becomes part of the amalgamation).  */
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
** Generate code to do constraint checks prior to an INSERT or an UPDATE.
**
** The input is a range of consecutive registers as follows:
**
**    1.  The rowid of the row after the update.
**
**    2.  The data in the first column of the entry after the update.
**
**    i.  Data from middle columns...
**
**    N.  The data in the last column of the entry after the update.
**
** The regRowid parameter is the index of the register containing (1).
**
** If isUpdate is true and rowidChng is non-zero, then rowidChng contains
** the address of a register containing the rowid before the update takes
** place. isUpdate is true for UPDATEs and false for INSERTs. If isUpdate
** is false, indicating an INSERT statement, then a non-zero rowidChng 
** indicates that the rowid was explicitly specified as part of the
** INSERT statement. If rowidChng is false, it means that  the rowid is
** computed automatically in an insert or that the rowid value is not 
** modified by an update.
**
** The code generated by this routine store new index entries into
** registers identified by aRegIdx[].  No index entry is created for
** indices where aRegIdx[i]==0.  The order of indices in aRegIdx[] is
** the same as the order of indices on the linked list of indices
** attached to the table.
**
** This routine also generates code to check constraints.  NOT NULL,
** CHECK, and UNIQUE constraints are all checked.  If a constraint fails,
** then the appropriate action is performed.  There are five possible
** actions: ROLLBACK, ABORT, FAIL, REPLACE, and IGNORE.
**
**  Constraint type  Action       What Happens
**  ---------------  ----------   ----------------------------------------
**  any              ROLLBACK     The current transaction is rolled back and
**                                sqlite3_exec() returns immediately with a
**                                return code of SQLITE_CONSTRAINT.
**
**  any              ABORT        Back out changes from the current command
**                                only (do not do a complete rollback) then
**                                cause sqlite3_exec() to return immediately
**                                with SQLITE_CONSTRAINT.
**
**  any              FAIL         Sqlite3_exec() returns immediately with a
**                                return code of SQLITE_CONSTRAINT.  The
**                                transaction is not rolled back and any
**                                prior changes are retained.
**
**  any              IGNORE       The record number and data is popped from
**                                the stack and there is an immediate jump
**                                to label ignoreDest.
**
**  NOT NULL         REPLACE      The NULL value is replace by the default
**                                value for that column.  If the default value
**                                is NULL, the action is the same as ABORT.
**
**  UNIQUE           REPLACE      The other row that conflicts with the row
**                                being inserted is removed.
**
**  CHECK            REPLACE      Illegal.  The results in an exception.
**
** Which action to take is determined by the overrideError parameter.
** Or if overrideError==OE_Default, then the pParse->onError parameter
** is used.  Or if pParse->onError==OE_Default then the onError value
** for the constraint is used.
**
** The calling routine must open a read/write cursor for pTab with
** cursor number "baseCur".  All indices of pTab must also have open
** read/write cursors with cursor number baseCur+i for the i-th cursor.
** Except, if there is no possibility of a REPLACE action then
** cursors do not need to be open for indices where aRegIdx[i]==0.
*/
void sqlite3GenerateConstraintChecks(
  Parse *pParse,      /* The parser context */
  Table *pTab,        /* the table into which we are inserting */
  int baseCur,        /* Index of a read/write cursor pointing at pTab */
  int regRowid,       /* Index of the range of input registers */
  int *aRegIdx,       /* Register used by each index.  0 for unused indices */
  int rowidChng,      /* True if the rowid might collide with existing entry */
  int isUpdate,       /* True for UPDATE, False for INSERT */
  int overrideError,  /* Override onError to this if not OE_Default */
  int ignoreDest,     /* Jump to this label on an OE_Ignore resolution */
  int *pbMayReplace   /* OUT: Set to true if constraint may cause a replace */
){
  int i;              /* loop counter */
  Vdbe *v;            /* VDBE under constrution */
  int nCol;           /* Number of columns */
  int onError;        /* Conflict resolution strategy */
  int j1;             /* Addresss of jump instruction */
  int j2 = 0, j3;     /* Addresses of jump instructions */
  int regData;        /* Register containing first data column */
  int iCur;           /* Table cursor number */
  Index *pIdx;         /* Pointer to one of the indices */
  sqlite3 *db;         /* Database connection */
  int seenReplace = 0; /* True if REPLACE is used to resolve INT PK conflict */
  int regOldRowid = (rowidChng && isUpdate) ? rowidChng : regRowid;

  db = pParse->db;
  v = sqlite3GetVdbe(pParse);
  assert( v!=0 );
  assert( pTab->pSelect==0 );  /* This table is not a VIEW */
  nCol = pTab->nCol;
  regData = regRowid + 1;

  /* Test all NOT NULL constraints.
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
                              pTab->zName, pTab->aCol[i].zName);
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
        sqlite3ExprCode(pParse, pTab->aCol[i].pDflt, regData+i);
        sqlite3VdbeJumpHere(v, j1);
        break;
      }
    }
  }

  /* Test all CHECK constraints
  */
#ifndef SQLITE_OMIT_CHECK
  if( pTab->pCheck && (db->flags & SQLITE_IgnoreChecks)==0 ){
    ExprList *pCheck = pTab->pCheck;
    pParse->ckBase = regData;
    onError = overrideError!=OE_Default ? overrideError : OE_Abort;
    for(i=0; i<pCheck->nExpr; i++){
      int allOk = sqlite3VdbeMakeLabel(v);
      sqlite3ExprIfTrue(pParse, pCheck->a[i].pExpr, allOk, SQLITE_JUMPIFNULL);
      if( onError==OE_Ignore ){
        sqlite3VdbeAddOp2(v, OP_Goto, 0, ignoreDest);
      }else{
        char *zConsName = pCheck->a[i].zName;
        if( onError==OE_Replace ) onError = OE_Abort; /* IMP: R-15569-63625 */
        if( zConsName ){
          zConsName = sqlite3MPrintf(db, "constraint %s failed", zConsName);
        }else{
          zConsName = 0;
        }
        sqlite3HaltConstraint(pParse, onError, zConsName, P4_DYNAMIC);
      }
      sqlite3VdbeResolveLabel(v, allOk);
    }
  }
#endif /* !defined(SQLITE_OMIT_CHECK) */

  /* If we have an INTEGER PRIMARY KEY, make sure the primary key
  ** of the new record does not previously exist.  Except, if this
  ** is an UPDATE and the primary key is not changing, that is OK.
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
        /* Fall thru into the next case */
      }
      case OE_Rollback:
      case OE_Abort:
      case OE_Fail: {
        sqlite3HaltConstraint(
          pParse, onError, "PRIMARY KEY must be unique", P4_STATIC);
        break;
      }
      case OE_Replace: {
        /* If there are DELETE triggers on this table and the
        ** recursive-triggers flag is set, call GenerateRowDelete() to
        ** remove the conflicting row from the table. This will fire
        ** the triggers and remove both the table and index b-tree entries.
        **
        ** Otherwise, if there are no triggers or the recursive-triggers
        ** flag is not set, but the table has one or more indexes, call 
        ** GenerateRowIndexDelete(). This removes the index b-tree entries 
        ** only. The table b-tree entry will be replaced by the new entry 
        ** when it is inserted.  
        **
        ** If either GenerateRowDelete() or GenerateRowIndexDelete() is called,
        ** also invoke MultiWrite() to indicate that this VDBE may require
        ** statement rollback (if the statement is aborted after the delete
        ** takes place). Earlier versions called sqlite3MultiWrite() regardless,
        ** but being more selective here allows statements like:
        **
        **   REPLACE INTO t(rowid) VALUES($newrowid)
        **
        ** to run without a statement journal if there are no indexes on the
        ** table.
        */
        Trigger *pTrigger = 0;
        if( db->flags&SQLITE_RecTriggers ){
          pTrigger = sqlite3TriggersExist(pParse, pTab, TK_DELETE, 0, 0);
        }
        if( pTrigger || sqlite3FkRequired(pParse, pTab, 0, 0) ){
          sqlite3MultiWrite(pParse);
          sqlite3GenerateRowDelete(
              pParse, pTab, baseCur, regRowid, 0, pTrigger, OE_Replace
          );
        }else if( pTab->pIndex ){
          sqlite3MultiWrite(pParse);
          sqlite3GenerateRowIndexDelete(pParse, pTab, baseCur, 0);
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

  /* Test all UNIQUE constraints by creating entries for each UNIQUE
  ** index and making sure that duplicate entries do not already exist.
  ** Add the new records to the indices as we go.
  */
  for(iCur=0, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, iCur++){
    int regIdx;
    int regR;

    if( aRegIdx[iCur]==0 ) continue;  /* Skip unused indices */

    /* Create a key for accessing the index entry */
    regIdx = sqlite3GetTempRange(pParse, pIdx->nColumn+1);
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
    sqlite3ExprCacheAffinityChange(pParse, regIdx, pIdx->nColumn+1);

    /* Find out what action to take in case there is an indexing conflict */
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
    
    /* Check to see if the new index entry will be unique */
    regR = sqlite3GetTempReg(pParse);
    sqlite3VdbeAddOp2(v, OP_SCopy, regOldRowid, regR);
    j3 = sqlite3VdbeAddOp4(v, OP_IsUnique, baseCur+iCur+1, 0,
                           regR, SQLITE_INT_TO_PTR(regIdx),
                           P4_INT32);
    sqlite3ReleaseTempRange(pParse, regIdx, pIdx->nColumn+1);

    /* Generate code that executes if the new index entry is not unique */
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

        sqlite3StrAccumInit(&errMsg, 0, 0, 200);
        errMsg.db = db;
        zSep = pIdx->nColumn>1 ? "columns " : "column ";
        for(j=0; j<pIdx->nColumn; j++){
          char *zCol = pTab->aCol[pIdx->aiColumn[j]].zName;
          sqlite3StrAccumAppend(&errMsg, zSep, -1);
          zSep = ", ";
          sqlite3StrAccumAppend(&errMsg, zCol, -1);
        }
        sqlite3StrAccumAppend(&errMsg,
            pIdx->nColumn>1 ? " are not unique" : " is not unique", -1);
        zErr = sqlite3StrAccumFinish(&errMsg);
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
** This routine generates code to finish the INSERT or UPDATE operation
** that was started by a prior call to sqlite3GenerateConstraintChecks.
** A consecutive range of registers starting at regRowid contains the
** rowid and the content to be inserted.
**
** The arguments to this routine should be the same as the first six
** arguments to sqlite3GenerateConstraintChecks.
*/
void sqlite3CompleteInsertion(
  Parse *pParse,      /* The parser context */
  Table *pTab,        /* the table into which we are inserting */
  int baseCur,        /* Index of a read/write cursor pointing at pTab */
  int regRowid,       /* Range of content */
  int *aRegIdx,       /* Register used by each index.  0 for unused indices */
  int isUpdate,       /* True for UPDATE, False for INSERT */
  int appendBias,     /* True if this is likely to be an append */
  int useSeekResult   /* True to set the USESEEKRESULT flag on OP_[Idx]Insert */
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
  assert( pTab->pSelect==0 );  /* This table is not a VIEW */
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
  sqlite3TableAffinityStr(v, pTab);
  sqlite3ExprCacheAffinityChange(pParse, regData, pTab->nCol);
  if( pParse->nested ){
    pik_flags = 0;
  }else{
    pik_flags = OPFLAG_NCHANGE;
    pik_flags |= (isUpdate?OPFLAG_ISUPDATE:OPFLAG_LASTROWID);
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
** Generate code that will open cursors for a table and for all
** indices of that table.  The "baseCur" parameter is the cursor number used
** for the table.  Indices are opened on subsequent cursors.
**
** Return the number of indices on the table.
*/
int sqlite3OpenTableAndIndices(
  Parse *pParse,   /* Parsing context */
  Table *pTab,     /* Table to be opened */
  int baseCur,     /* Cursor number assigned to the table */
  int op           /* OP_OpenRead or OP_OpenWrite */
){
  int i;
  int iDb;
  Index *pIdx;
  Vdbe *v;

  if( IsVirtual(pTab) ) return 0;
  iDb = sqlite3SchemaToIndex(pParse->db, pTab->pSchema);
  v = sqlite3GetVdbe(pParse);
  assert( v!=0 );
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
** The following global variable is incremented whenever the
** transfer optimization is used.  This is used for testing
** purposes only - to make sure the transfer optimization really
** is happening when it is suppose to.
*/
int sqlite3_xferopt_count;
#endif /* SQLITE_TEST */


#ifndef SQLITE_OMIT_XFER_OPT
/*
** Check to collation names to see if they are compatible.
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
** Check to see if index pSrc is compatible as a source of data
** for index pDest in an insert transfer optimization.  The rules
** for a compatible index:
**
**    *   The index is over the same set of columns
**    *   The same DESC and ASC markings occurs on all columns
**    *   The same onError processing (OE_Abort, OE_Ignore, etc)
**    *   The same collating sequence on each column
*/
static int xferCompatibleIndex(Index *pDest, Index *pSrc){
  int i;
  assert( pDest && pSrc );
  assert( pDest->pTable!=pSrc->pTable );
  if( pDest->nColumn!=pSrc->nColumn ){
    return 0;   /* Different number of columns */
  }
  if( pDest->onError!=pSrc->onError ){
    return 0;   /* Different conflict resolution strategies */
  }
  for(i=0; i<pSrc->nColumn; i++){
    if( pSrc->aiColumn[i]!=pDest->aiColumn[i] ){
      return 0;   /* Different columns indexed */
    }
    if( pSrc->aSortOrder[i]!=pDest->aSortOrder[i] ){
      return 0;   /* Different sort orders */
    }
    if( !xferCompatibleCollation(pSrc->azColl[i],pDest->azColl[i]) ){
      return 0;   /* Different collating sequences */
    }
  }

  /* If no test above fails then the indices must be compatible */
  return 1;
}

/*
** Attempt the transfer optimization on INSERTs of the form
**
**     INSERT INTO tab1 SELECT * FROM tab2;
**
** The xfer optimization transfers raw records from tab2 over to tab1.  
** Columns are not decoded and reassemblied, which greatly improves
** performance.  Raw index records are transferred in the same way.
**
** The xfer optimization is only attempted if tab1 and tab2 are compatible.
** There are lots of rules for determining compatibility - see comments
** embedded in the code for details.
**
** This routine returns TRUE if the optimization is guaranteed to be used.
** Sometimes the xfer optimization will only work if the destination table
** is empty - a factor that can only be determined at run-time.  In that
** case, this routine generates code for the xfer optimization but also
** does a test to see if the destination table is empty and jumps over the
** xfer optimization code if the test fails.  In that case, this routine
** returns FALSE so that the caller will know to go ahead and generate
** an unoptimized transfer.  This routine also returns FALSE if there
** is no chance that the xfer optimization can be applied.
**
** This optimization is particularly useful at making VACUUM run faster.
*/
static int xferOptimization(
  Parse *pParse,        /* Parser context */
  Table *pDest,         /* The table we are inserting into */
  Select *pSelect,      /* A SELECT statement to use as the data source */
  int onError,          /* How to handle constraint errors */
  int iDbDest           /* The database of pDest */
){
  ExprList *pEList;                /* The result set of the SELECT */
  Table *pSrc;                     /* The table in the FROM clause of SELECT */
  Index *pSrcIdx, *pDestIdx;       /* Source and destination indices */
  struct SrcList_item *pItem;      /* An element of pSelect->pSrc */
  int i;                           /* Loop counter */
  int iDbSrc;                      /* The database of pSrc */
  int iSrc, iDest;                 /* Cursors from source and destination */
  int addr1, addr2;                /* Loop addresses */
  int emptyDestTest;               /* Address of test for empty pDest */
  int emptySrcTest;                /* Address of test for empty pSrc */
  Vdbe *v;                         /* The VDBE we are building */
  KeyInfo *pKey;                   /* Key information for an index */
  int regAutoinc;                  /* Memory register used by AUTOINC */
  int destHasUniqueIdx = 0;        /* True if pDest has a UNIQUE index */
  int regData, regRowid;           /* Registers holding data and rowid */

  if( pSelect==0 ){
    return 0;   /* Must be of the form  INSERT INTO ... SELECT ... */
  }
  if( sqlite3TriggerList(pParse, pDest) ){
    return 0;   /* tab1 must not have triggers */
  }
#ifndef SQLITE_OMIT_VIRTUALTABLE
  if( pDest->tabFlags & TF_Virtual ){
    return 0;   /* tab1 must not be a virtual table */
  }
#endif
  if( onError==OE_Default ){
    if( pDest->iPKey>=0 ) onError = pDest->keyConf;
    if( onError==OE_Default ) onError = OE_Abort;
  }
  assert(pSelect->pSrc);   /* allocated even if there is no FROM clause */
  if( pSelect->pSrc->nSrc!=1 ){
    return 0;   /* FROM clause must have exactly one term */
  }
  if( pSelect->pSrc->a[0].pSelect ){
    return 0;   /* FROM clause cannot contain a subquery */
  }
  if( pSelect->pWhere ){
    return 0;   /* SELECT may not have a WHERE clause */
  }
  if( pSelect->pOrderBy ){
    return 0;   /* SELECT may not have an ORDER BY clause */
  }
  /* Do not need to test for a HAVING clause.  If HAVING is present but
  ** there is no ORDER BY, we will get an error. */
  if( pSelect->pGroupBy ){
    return 0;   /* SELECT may not have a GROUP BY clause */
  }
  if( pSelect->pLimit ){
    return 0;   /* SELECT may not have a LIMIT clause */
  }
  assert( pSelect->pOffset==0 );  /* Must be so if pLimit==0 */
  if( pSelect->pPrior ){
    return 0;   /* SELECT may not be a compound query */
  }
  if( pSelect->selFlags & SF_Distinct ){
    return 0;   /* SELECT may not be DISTINCT */
  }
  pEList = pSelect->pEList;
  assert( pEList!=0 );
  if( pEList->nExpr!=1 ){
    return 0;   /* The result set must have exactly one column */
  }
  assert( pEList->a[0].pExpr );
  if( pEList->a[0].pExpr->op!=TK_ALL ){
    return 0;   /* The result set must be the special operator "*" */
  }

  /* At this point we have established that the statement is of the
  ** correct syntactic form to participate in this optimization.  Now
  ** we have to check the semantics.
  */
  pItem = pSelect->pSrc->a;
  pSrc = sqlite3LocateTable(pParse, 0, pItem->zName, pItem->zDatabase);
  if( pSrc==0 ){
    return 0;   /* FROM clause does not contain a real table */
  }
  if( pSrc==pDest ){
    return 0;   /* tab1 and tab2 may not be the same table */
  }
#ifndef SQLITE_OMIT_VIRTUALTABLE
  if( pSrc->tabFlags & TF_Virtual ){
    return 0;   /* tab2 must not be a virtual table */
  }
#endif
  if( pSrc->pSelect ){
    return 0;   /* tab2 may not be a view */
  }
  if( pDest->nCol!=pSrc->nCol ){
    return 0;   /* Number of columns must be the same in tab1 and tab2 */
  }
  if( pDest->iPKey!=pSrc->iPKey ){
    return 0;   /* Both tables must have the same INTEGER PRIMARY KEY */
  }
  for(i=0; i<pDest->nCol; i++){
    if( pDest->aCol[i].affinity!=pSrc->aCol[i].affinity ){
      return 0;    /* Affinity must be the same on all columns */
    }
    if( !xferCompatibleCollation(pDest->aCol[i].zColl, pSrc->aCol[i].zColl) ){
      return 0;    /* Collating sequence must be the same on all columns */
    }
    if( pDest->aCol[i].notNull && !pSrc->aCol[i].notNull ){
      return 0;    /* tab2 must be NOT NULL if tab1 is */
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
      return 0;    /* pDestIdx has no corresponding index in pSrc */
    }
  }
#ifndef SQLITE_OMIT_CHECK
  if( pDest->pCheck && sqlite3ExprListCompare(pSrc->pCheck, pDest->pCheck) ){
    return 0;   /* Tables have different CHECK constraints.  Ticket #2252 */
  }
#endif
#ifndef SQLITE_OMIT_FOREIGN_KEY
  /* Disallow the transfer optimization if the destination table constains
  ** any foreign key constraints.  This is more restrictive than necessary.
  ** But the main beneficiary of the transfer optimization is the VACUUM 
  ** command, and the VACUUM command disables foreign key constraints.  So
  ** the extra complication to make this rule less restrictive is probably
  ** not worth the effort.  Ticket [6284df89debdfa61db8073e062908af0c9b6118e]
  */
  if( (pParse->db->flags & SQLITE_ForeignKeys)!=0 && pDest->pFKey!=0 ){
    return 0;
  }
#endif
  if( (pParse->db->flags & SQLITE_CountRows)!=0 ){
    return 0;  /* xfer opt does not play well with PRAGMA count_changes */
  }

  /* If we get this far, it means that the xfer optimization is at
  ** least a possibility, though it might only work if the destination
  ** table (tab1) is initially empty.
  */
#ifdef SQLITE_TEST
  sqlite3_xferopt_count++;
#endif
  iDbSrc = sqlite3SchemaToIndex(pParse->db, pSrc->pSchema);
  v = sqlite3GetVdbe(pParse);
  sqlite3CodeVerifySchema(pParse, iDbSrc);
  iSrc = pParse->nTab++;
  iDest = pParse->nTab++;
  regAutoinc = autoIncBegin(pParse, iDbDest, pDest);
  sqlite3OpenTable(pParse, iDest, iDbDest, pDest, OP_OpenWrite);
  if( (pDest->iPKey<0 && pDest->pIndex!=0)          /* (1) */
   || destHasUniqueIdx                              /* (2) */
   || (onError!=OE_Abort && onError!=OE_Rollback)   /* (3) */
  ){
    /* In some circumstances, we are able to run the xfer optimization
    ** only if the destination table is initially empty.  This code makes
    ** that determination.  Conditions under which the destination must
    ** be empty:
    **
    ** (1) There is no INTEGER PRIMARY KEY but there are indices.
    **     (If the destination is not initially empty, the rowid fields
    **     of index entries might need to change.)
    **
    ** (2) The destination has a unique index.  (The xfer optimization 
    **     is unable to test uniqueness.)
    **
    ** (3) onError is something other than OE_Abort and OE_Rollback.
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
    autoIncStep(pParse, regAutoinc, regRowid);
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
      if( xferCompatibleIndex(pDestIdx, pSrcIdx) ) break;
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
