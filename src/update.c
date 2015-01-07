/*
** 这个文件包含由解析器调用的C代码例程来处理UPDATE语句
*/
#include "sqliteInt.h"

#ifndef SQLITE_OMIT_VIRTUALTABLE
/* 前置声明*/
static void updateVirtualTable(
  Parse *pParse,       /* 解析上下文 */
  SrcList *pSrc,       /* 需要修改的虚拟表 */
  Table *pTab,         /* 虚拟表的建立 */
  ExprList *pChanges,  /*处理 UPDATE 语句中的列变化 */
  Expr *pRowidExpr,    /* 定义一个表达式，用来验证 */
  int *aXRef,          /*映射 pTab 和 pChanges */
  Expr *pWhere,        /*UPDATE语句的WHERE子句 */
  int onError          /* 冲突处理策略,有replace,ignore,fail,abort和rollback*/
);
#endif /* SQLITE_OMIT_VIRTUALTABLE */

/*
** 最近编码指令是一个OP_Column 用于检索pTab表中的第i个列
**无论什么情况这个例程将OP_Column的P4参数设置为默认值。
**列的默认值是由一个默认的列定义指定的。
**这个定义可以由用户提供的表时创建,也可是后来修改表定义的ALTER table命令中来添加定义。
**如果是后者,那么在磁盘的btree表中行记录上可能不包含一个列的值和默认值,返回时由P4参数替代列值.
**如果是前者,那么所有row-records保证其列值是用户提供的值,因此P4的值不是必需的。
** ALTER TABLE命令创建的默认列值可能只有这几类值:数字,null或字符串。
**(如果一个更复杂的默认表达式提供了列默认值,当执行ALTER TABLE语句时会将该默认值写入sqlite_master表。)
**因此,P4参数只需要如果列的默认值是一个文字数字,字符串或null。sqlite3ValueFromExpr()函数可以将这些类型的表达式转换为sqlite3_value对象
** 如果参数iReg不是负面的,代码注册iReg OP_RealAffinity指令。这是当一个等价的整数值存储在一个8字节
**浮点值的地方,以节省空间
*/
void sqlite3ColumnDefault(Vdbe *v, Table *pTab, int i, int iReg){  //该函数用来更新列的默认值
  assert( pTab!=0 );
  /*ASSERT()是一个调试程序时经常使用的宏，在程序运行时它计算括号内的表达式，
  **如果表达式为FALSE  (0),  程序将报告错误，并终止执行。如果表达式不为0，则继续执行后面的语句。
  **这个宏通常原来判断程序中是否出现了明显非法的数据，如果出现了终止程序以免导致严重后果，
  **同时也便于查找错误。 
  **这里就是虚拟表建立成功后执行下面的语句*/ 
  if( !pTab->pSelect ){
     //表pTab的select语句,如果该表为基本表,则为空,如果该表为视图,则存放视图定义
    sqlite3_value *pValue;
     //定义一个sqlite3_value类型的指针
    u8 enc = ENC(sqlite3VdbeDb(v));
     //返回数据库v的模式定义,并传给变量enc
    Column *pCol = &pTab->aCol[i];
     //Column是一个结构体不是一个函数,将表pTab的第i列传给变量pCol
    VdbeComment((v, "%s.%s", pTab->zName, pCol->zName));
    //v代表数据库v,pTab->zName表示表pTab表名,pCol->zName表示pTab表中第i列的列名
    assert( i<pTab->nCol );
     //验证i是否小于表pTab的总列数
    sqlite3ValueFromExpr(sqlite3VdbeDb(v), pCol->pDflt, enc, 
                         pCol->affinity, &pValue);
//根据pCol->pDflt语句计算出新的列默认值,该默认值保存在pValue中
//pCol->affinity表示与列pCol相关的关系
    if( pValue ){
      sqlite3VdbeChangeP4(v, -1, (const char *)pValue, P4_MEM);
    }
     //将列的默认值更新为新的默认值pValue
#ifndef SQLITE_OMIT_FLOATING_POINT
    if( iReg>=0 && pTab->aCol[i].affinity==SQLITE_AFF_REAL ){
      sqlite3VdbeAddOp1(v, OP_RealAffinity, iReg);  //数据库v增加新的约束,即列的新默认值
    }
#endif
  }
}

/*
**  UPDATE 语句的过程.
**
**   UPDATE OR IGNORE table_wxyz SET a=b, c=d WHERE e<5 AND f NOT NULL;
**          \_______/ \________/     \______/       \________________/
*            onError   pTabList      pChanges             pWhere
*/
void sqlite3Update(
  Parse *pParse,         /* 解析上下文 */
  SrcList *pTabList,     /*  所要改变的表 */
  ExprList *pChanges,    /* 改变的表 */
  Expr *pWhere,          /*  判断是否为空 */
  int onError            /* 错误的处理 */
){
  int i, j;              /*用来循环计数的 */
  Table *pTab;           /*用来进行更新 */
  int addr = 0;          /*  地址的初始化*/
  WhereInfo *pWInfo;     /* 获得Where的信息 */
  Vdbe *v;               /* 虚拟数据库引擎 */
  Index *pIdx;           /* 循环指数*/
  int nIdx;              /*设定当达到指标的时候需要更新 */
  int iCur;              /* 统计VDBE pTab的数量 */
  sqlite3 *db;           /*数据库结构 */
  int *aRegIdx = 0;      /* 一个寄存器分配给每个索引更新 */
  int *aXRef = 0;        /*aXRef[i]的索引pChanges指向一个表达式的第i个列
						 当aXRef[i]==-1时，第i个列没有改变*/
  int chngRowid;         /* 处理被改变记录 */
  Expr *pRowidExpr = 0;  /* 表达式定义的新纪录 */
  int openAll = 0;       /* 如果为真，打开所有索引*/
  AuthContext sContext;  /* 授权上下文 */
  NameContext sNC;       /*  解析表达式*/
  int iDb;               /* 更新数据库表*/
  int okOnePass;         /* 适用于一次通过的算法没有先进先出 */
  int hasFK;             /*  外键处理 */

#ifndef SQLITE_OMIT_TRIGGER
  int isView;            /* True when updating a view (INSTEAD OF trigger) */
  Trigger *pTrigger;     /* List of triggers on pTab, if required */
  int tmask;             /* Mask of TRIGGER_BEFORE|TRIGGER_AFTER */
#endif
  int newmask;           /* Mask of NEW.* columns accessed by BEFORE triggers */

  /* Register Allocations */
  int regRowCount = 0;   /* A count of rows changed */
  int regOldRowid;       /* The old rowid */
  int regNewRowid;       /* The new rowid */
  int regNew;            /* Content of the NEW.* table in triggers */
  int regOld = 0;        /* Content of OLD.* table in triggers */
  int regRowSet = 0;     /* Rowset of rows to be updated */

  memset(&sContext, 0, sizeof(sContext));
  db = pParse->db;
  if( pParse->nErr || db->mallocFailed ){
    goto update_cleanup;
  }
  assert( pTabList->nSrc==1 );

  /* Locate the table which we want to update. 
  */
  pTab = sqlite3SrcListLookup(pParse, pTabList);
  if( pTab==0 ) goto update_cleanup;
  iDb = sqlite3SchemaToIndex(pParse->db, pTab->pSchema);

  /* Figure out if we have any triggers and if the table being
  ** updated is a view.
  */
#ifndef SQLITE_OMIT_TRIGGER
  pTrigger = sqlite3TriggersExist(pParse, pTab, TK_UPDATE, pChanges, &tmask);
  isView = pTab->pSelect!=0;
  assert( pTrigger || tmask==0 );
#else
# define pTrigger 0
# define isView 0
# define tmask 0
#endif
#ifdef SQLITE_OMIT_VIEW
# undef isView
# define isView 0
#endif

  if( sqlite3ViewGetColumnNames(pParse, pTab) ){
    goto update_cleanup;
  }
  if( sqlite3IsReadOnly(pParse, pTab, tmask) ){
    goto update_cleanup;
  }
  aXRef = sqlite3DbMallocRaw(db, sizeof(int) * pTab->nCol );
  if( aXRef==0 ) goto update_cleanup;
  for(i=0; i<pTab->nCol; i++) aXRef[i] = -1;

  /* 分配一个游标主要数据库表和索引。索引游标可能不被使用,
  **但是如果他们使用他们需要发生后数据库游标。所以继续分配足够的空间,以备不时之需。
  */
  pTabList->a[0].iCursor = iCur = pParse->nTab++;
  for(pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext){
    pParse->nTab++;
  }

  /* 初始化name-context */
  memset(&sNC, 0, sizeof(sNC));
  sNC.pParse = pParse;
  sNC.pSrcList = pTabList;

  /* 解决列名在UPDATE语句的所有的表情。
  **也找到每一列的列索引更新pChanges数组。对每一列进行更新,确保我们有授权改变这一列。
  */
  chngRowid = 0;
  for(i=0; i<pChanges->nExpr; i++){
    if( sqlite3ResolveExprNames(&sNC, pChanges->a[i].pExpr) ){
      goto update_cleanup;
    }
    for(j=0; j<pTab->nCol; j++){
      if( sqlite3StrICmp(pTab->aCol[j].zName, pChanges->a[i].zName)==0 ){
        if( j==pTab->iPKey ){
          chngRowid = 1;
          pRowidExpr = pChanges->a[i].pExpr;
        }
        aXRef[j] = i;
        break;
      }
    }
    if( j>=pTab->nCol ){
      if( sqlite3IsRowid(pChanges->a[i].zName) ){
        chngRowid = 1;
        pRowidExpr = pChanges->a[i].pExpr;
      }else{
        sqlite3ErrorMsg(pParse, "no such column: %s", pChanges->a[i].zName);
        pParse->checkSchema = 1;
        goto update_cleanup;
      }
    }
#ifndef SQLITE_OMIT_AUTHORIZATION
    {
      int rc;
      rc = sqlite3AuthCheck(pParse, SQLITE_UPDATE, pTab->zName,
                           pTab->aCol[j].zName, db->aDb[iDb].zName);
      if( rc==SQLITE_DENY ){
        goto update_cleanup;
      }else if( rc==SQLITE_IGNORE ){
        aXRef[j] = -1;
      }
    }
#endif
  }

  hasFK = sqlite3FkRequired(pParse, pTab, aXRef, chngRowid);

  /* 为数组分配内存aRegIdx[]。
  **有一个条目数组中的每个索引与表被更新。填写的值的寄存器数量指数使用和未使用的指数为零。
  */
  for(nIdx=0, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, nIdx++){}
  if( nIdx>0 ){
    aRegIdx = sqlite3DbMallocRaw(db, sizeof(Index*) * nIdx );
    if( aRegIdx==0 ) goto update_cleanup;
  }
  for(j=0, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, j++){
    int reg;
    if( hasFK || chngRowid ){
      reg = ++pParse->nMem;
    }else{
      reg = 0;
      for(i=0; i<pIdx->nColumn; i++){
        if( aXRef[pIdx->aiColumn[i]]>=0 ){
          reg = ++pParse->nMem;
          break;
        }
      }
    }
    aRegIdx[j] = reg;
  }

  /* 开始生成代码. */
  v = sqlite3GetVdbe(pParse);
  if( v==0 ) goto update_cleanup;
  if( pParse->nested==0 ) sqlite3VdbeCountChanges(v);
  sqlite3BeginWriteOperation(pParse, 1, iDb);

#ifndef SQLITE_OMIT_VIRTUALTABLE
  /* 虚表必须单独处理 */
  if( IsVirtual(pTab) ){
    updateVirtualTable(pParse, pTabList, pTab, pChanges, pRowidExpr, aXRef,
                       pWhere, onError);
    pWhere = 0;
    pTabList = 0;
    goto update_cleanup;
  }
#endif

  /* 分配所需的寄存器。 */
  regRowSet = ++pParse->nMem;
  regOldRowid = regNewRowid = ++pParse->nMem;
  if( pTrigger || hasFK ){
    regOld = pParse->nMem + 1;
    pParse->nMem += pTab->nCol;
  }
  if( chngRowid || pTrigger || hasFK ){
    regNewRowid = ++pParse->nMem; 
  }
  regNew = pParse->nMem + 1;
  pParse->nMem += pTab->nCol;

  /* 启动视图上下文.*/
  if( isView ){
    sqlite3AuthContextPush(pParse, &sContext, pTab->zName);
  }

  /* 如果我们尝试更新视图,实现这一观点到一个临时表。
  */
#if !defined(SQLITE_OMIT_VIEW) && !defined(SQLITE_OMIT_TRIGGER)
  if( isView ){
    sqlite3MaterializeView(pParse, pTab, pWhere, iCur);
  }
#endif

  /* 解决所有的列名表达式在WHERE子句中。
  */
  if( sqlite3ResolveExprNames(&sNC, pWhere) ){
    goto update_cleanup;
  }

  /*开始数据库扫描
  */
  sqlite3VdbeAddOp3(v, OP_Null, 0, regRowSet, regOldRowid);
  pWInfo = sqlite3WhereBegin(
      pParse, pTabList, pWhere, 0, 0, WHERE_ONEPASS_DESIRED, 0
  );
  if( pWInfo==0 ) goto update_cleanup;
  okOnePass = pWInfo->okOnePass;

  /*开始数据库扫描.
  */
  sqlite3VdbeAddOp2(v, OP_Rowid, iCur, regOldRowid);
  if( !okOnePass ){
    sqlite3VdbeAddOp2(v, OP_RowSetAdd, regRowSet, regOldRowid);
  }

  /* 数据库扫描循环结束。.
  */
  sqlite3WhereEnd(pWInfo);

  /* Initialize the count of updated rows
  */
  if( (db->flags & SQLITE_CountRows) && !pParse->pTriggerTab ){
    regRowCount = ++pParse->nMem;
    sqlite3VdbeAddOp2(v, OP_Integer, 0, regRowCount);
  }

  if( !isView ){
    /* 
    ** 打开每一个索引,需要更新。注意,如果任何索引可能会调用一个替代解决冲突的行动,
    **那么我们需要打开所有的指数,因为我们可能需要删除一些记录。
    */
    if( !okOnePass ) sqlite3OpenTable(pParse, iCur, iDb, pTab, OP_OpenWrite); 
    if( onError==OE_Replace ){
      openAll = 1;
    }else{
      openAll = 0;
      for(pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext){
        if( pIdx->onError==OE_Replace ){
          openAll = 1;
          break;
        }
      }
    }
    for(i=0, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, i++){
      assert( aRegIdx );
      if( openAll || aRegIdx[i]>0 ){
        KeyInfo *pKey = sqlite3IndexKeyinfo(pParse, pIdx);
        sqlite3VdbeAddOp4(v, OP_OpenWrite, iCur+i+1, pIdx->tnum, iDb,
                       (char*)pKey, P4_KEYINFO_HANDOFF);
        assert( pParse->nTab>iCur+i+1 );
      }
    } 
  }

  /* 更新循环的顶部 */
  if( okOnePass ){
    int a1 = sqlite3VdbeAddOp1(v, OP_NotNull, regOldRowid);
    addr = sqlite3VdbeAddOp0(v, OP_Goto);
    sqlite3VdbeJumpHere(v, a1);
  }else{
    addr = sqlite3VdbeAddOp3(v, OP_RowSetRead, regRowSet, 0, regOldRowid);
  }

  /* 使光标iCur指向的记录更新。如果这个记录不存在由于某种原因
  **(例如,删除由一个触发器RowSet的然后跳转到下一个迭代循环。 */
  sqlite3VdbeAddOp3(v, OP_NotExists, iCur, addr, regOldRowid);

  /* 如果记录编号将会改变,设置寄存器regNewRowid包含新值。
  **如果记录没有被修改,然后regNewRowid regOldRowid注册一样,已经填充。  */
  assert( chngRowid || pTrigger || hasFK || regOldRowid==regNewRowid );
  if( chngRowid ){
    sqlite3ExprCode(pParse, pRowidExpr, regNewRowid);
    sqlite3VdbeAddOp1(v, OP_MustBeInt, regNewRowid);
  }

  /* 如果这个表上有触发,填充数组的寄存器所需的旧。
  ** 列数据。 */
  if( hasFK || pTrigger ){
    u32 oldmask = (hasFK ? sqlite3FkOldmask(pParse, pTab) : 0);
    oldmask |= sqlite3TriggerColmask(pParse, 
        pTrigger, pChanges, 0, TRIGGER_BEFORE|TRIGGER_AFTER, pTab, onError
    );
    for(i=0; i<pTab->nCol; i++){
      if( aXRef[i]<0 || oldmask==0xffffffff || (i<32 && (oldmask & (1<<i))) ){
        sqlite3ExprCodeGetColumnOfTable(v, pTab, iCur, i, regOld+i);
      }else{
        sqlite3VdbeAddOp2(v, OP_Null, 0, regOld+i);
      }
    }
    if( chngRowid==0 ){
      sqlite3VdbeAddOp2(v, OP_Copy, regOldRowid, regNewRowid);
    }
  }

  /* 新的行数据开始在 regNew填充该数组的寄存器 beginning at . 
  **这个数组是用来检查constaints,创建新表和索引记录,作为任何新值。* 引用由触发器。
  ** 之前如果有一个或多个触发器,那么就不要用列填充相关的寄存器(a)
  **不修改这个UPDATE语句和(b)不是由新的访问。*引用。寄存器的值不能修改更新之前
  **必须从数据库重新加载后触发被解雇呢(如触发器可能修改)。
  **所以不加载那些不会使用消除了一些冗余的操作码。
  */
  newmask = sqlite3TriggerColmask(
      pParse, pTrigger, pChanges, 1, TRIGGER_BEFORE, pTab, onError
  );
  sqlite3VdbeAddOp3(v, OP_Null, 0, regNew, regNew+pTab->nCol-1);
  for(i=0; i<pTab->nCol; i++){
    if( i==pTab->iPKey ){
      /*sqlite3VdbeAddOp2(v, OP_Null, 0, regNew+i);*/
    }else{
      j = aXRef[i];
      if( j>=0 ){
        sqlite3ExprCode(pParse, pChanges->a[j].pExpr, regNew+i);
      }else if( 0==(tmask&TRIGGER_BEFORE) || i>31 || (newmask&(1<<i)) ){
        /* 这个分支加载一个列的值,不会变成一个寄存器。这样做是如果没有触发前,或如果有一个或多个触发器之前使用这个值通过一个新的。*参考在触发程序。
        */
        testcase( i==31 );
        testcase( i==32 );
        sqlite3VdbeAddOp3(v, OP_Column, iCur, i, regNew+i);
        sqlite3ColumnDefault(v, pTab, i, regNew+i);
      }
    }
  }

  /* Fire any BEFORE UPDATE triggers. This happens before constraints are verified.
  **火之前任何更新触发器。约束验证之前发生这种情况。
  **  One could argue that this is wrong.有人会说,这是错误的
  */
  if( tmask&TRIGGER_BEFORE ){
    sqlite3VdbeAddOp2(v, OP_Affinity, regNew, pTab->nCol);
    sqlite3TableAffinityStr(v, pTab);
    sqlite3CodeRowTrigger(pParse, pTrigger, TK_UPDATE, pChanges, 
        TRIGGER_BEFORE, pTab, regOldRowid, onError, addr);

    /* The row-trigger may have deleted the row being updated.他row-trigger可能已经删除的行被更新。
    ** In this case, jump to the next row. No updates or AFTER triggers are required. 
    **在这种情况下,跳到下一行。不需要更新或之后触发。
    **This behaviour - what happens when the row being updated is deleted or renamed 
    **by a BEFORE trigger - is left undefined in the documentation.
     这种行为--当行被触发之前更新被删除或重命名,剩下未定义的文档。
    */
    sqlite3VdbeAddOp3(v, OP_NotExists, iCur, addr, regOldRowid);

    /*如果不删除它,row-trigger可能还有修改的一些列的行被更新。加载所有列的值不能修改update语句到寄存器,以防这种情况
    */
    for(i=0; i<pTab->nCol; i++){
      if( aXRef[i]<0 && i!=pTab->iPKey ){
        sqlite3VdbeAddOp3(v, OP_Column, iCur, i, regNew+i);
        sqlite3ColumnDefault(v, pTab, i, regNew+i);
      }
    }
  }

  if( !isView ){
    int j1;                       /* 地址跳转指令 */

    /* 约束检查。*/
    sqlite3GenerateConstraintChecks(pParse, pTab, iCur, regNewRowid,
        aRegIdx, (chngRowid?regOldRowid:0), 1, onError, addr, 0);

    /*FK约束检查队 */
    if( hasFK ){
      sqlite3FkCheck(pParse, pTab, regOldRowid, 0);
    }

    /* 删除当前记录的索引条目  */
    j1 = sqlite3VdbeAddOp3(v, OP_NotExists, iCur, 0, regOldRowid);
    sqlite3GenerateRowIndexDelete(pParse, pTab, iCur, aRegIdx);
  
    /* 如果更改记录数量,删除旧的记录。  */
    if( hasFK || chngRowid ){
      sqlite3VdbeAddOp2(v, OP_Delete, iCur, 0);
    }
    sqlite3VdbeJumpHere(v, j1);

    if( hasFK ){
      sqlite3FkCheck(pParse, pTab, 0, regNewRowid);
    }
  
    /* 插入新的索引条目和新纪录。 */
    sqlite3CompleteInsertion(pParse, pTab, iCur, regNewRowid, aRegIdx, 1, 0, 0);

    /* 做任何在级联,NULL或设置默认操作需要处理行(可能在其他表),通过一个外键引用行更新 */ 
    if( hasFK ){
      sqlite3FkActions(pParse, pTab, pChanges, regOldRowid);
    }
  }

  /*增加行计数器
  */
  if( (db->flags & SQLITE_CountRows) && !pParse->pTriggerTab){
    sqlite3VdbeAddOp2(v, OP_AddImm, regRowCount, 1);
  }

  sqlite3CodeRowTrigger(pParse, pTrigger, TK_UPDATE, pChanges, 
      TRIGGER_AFTER, pTab, regOldRowid, onError, addr);

  /* Repeat the above with the next record to be updated,
  ** until all record selected by the WHERE clause have been updated.
  **重复上面的下一个记录被更新,直到所有记录选择的WHERE子句被更新。
  */
  sqlite3VdbeAddOp2(v, OP_Goto, 0, addr);
  sqlite3VdbeJumpHere(v, addr);

  /* 关闭所有表 */
  for(i=0, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, i++){
    assert( aRegIdx );
    if( openAll || aRegIdx[i]>0 ){
      sqlite3VdbeAddOp2(v, OP_Close, iCur+i+1, 0);
    }
  }
  sqlite3VdbeAddOp2(v, OP_Close, iCur, 0);

  /* 更新sqlite_sequence表通过存储的内容最大rowid计数器值记录插入到自动增量表中。
  */
  if( pParse->nested==0 && pParse->pTriggerTab==0 ){
    sqlite3AutoincrementEnd(pParse);
  }

  /*
  ** Return the number of rows that were changed. If this routine is  generating code because of a call to sqlite3NestedParse(), do not invoke the callback function.
  返回的行数的改变。如果这个例程生成代码,因为调用sqlite3NestedParse(),不调用回调函数。
  */
  if( (db->flags&SQLITE_CountRows) && !pParse->pTriggerTab && !pParse->nested ){
    sqlite3VdbeAddOp2(v, OP_ResultRow, regRowCount, 1);
    sqlite3VdbeSetNumCols(v, 1);
    sqlite3VdbeSetColName(v, 0, COLNAME_NAME, "rows updated", SQLITE_STATIC);
  }

update_cleanup:
  sqlite3AuthContextPop(&sContext);
  sqlite3DbFree(db, aRegIdx);
  sqlite3DbFree(db, aXRef);
  sqlite3SrcListDelete(db, pTabList);
  sqlite3ExprListDelete(db, pChanges);
  sqlite3ExprDelete(db, pWhere);
  return;
}
/* Make sure "isView" and other macros defined above are undefined. 
**Otherwise thely may interfere with compilation of other functions in this file (or in another file, if this file becomes part of the amalgamation).
**确保"isView"和其他上面宏定义是未定义的。
**否则你可能会干扰编译该文件的其他功能(或在另一个文件,如果这个文件成为融合的一部分)。
  */
#ifdef isView
 #undef isView
#endif
#ifdef pTrigger
 #undef pTrigger
#endif

#ifndef SQLITE_OMIT_VIRTUALTABLE
/*
** 为虚拟表的更新生成代码；
**
** 该方法是我们创建一个ephemerial表，其中包含要改变的每一行：
**
**   (A)  该行的原始ROWID
**   (B) 修订后的ROWID
**   (C)  该行中列的内容
**
** 然后,我们将遍历这个临时表和ephermeral表中的每一行用VUpdate
**
**结束后，删除临时表.
**
**(注一)实际上,如果我们提前知道,(A)和(B)总是一样的,
我们只存储(A),然后重复(A)当拉出来的临时表调用VUpdate之前
*/
static void updateVirtualTable(
  Parse *pParse,       /*解析上下文t */
  SrcList *pSrc,       /* 修改虚拟表处理*/
  Table *pTab,         /* 虚拟表的建立 */
  ExprList *pChanges,  /* 处理UPDATE语句中的列变化*/
  Expr *pRowid,        /*  定义的一个表达式，用来验证 */
  int *aXRef,          /* 获取pTab pChanges条目，之后进行判断是否为0的if语句处*/
  Expr *pWhere,        /*UPDATE语句中的一种定义 */
  int onError          /* 错误处理 */
){
  Vdbe *v = pParse->pVdbe;  /*  搭建一个虚拟机*/
  ExprList *pEList = 0;     /*存放SELECT语句的结果集合 */
  Select *pSelect = 0;      /* SELECT定义 */
  Expr *pExpr;              /* 用来临时存放表达式的 */
  int ephemTab;             /* 存放表的结果 */
  int i;                    /*循环计数 */
  int addr;                 /* 地址的重复使用*/
  int iReg;                 /*存放++pParse->nMem的值，之后用来做函数中的定值处 */
  sqlite3 *db = pParse->db; /**数据库的连接*/
  const char *pVTab = (const char*)sqlite3GetVTable(db, pTab);
  SelectDest dest;

  /* 构建SELECT语句,会发现新的值
    * *所有更新的行。 
  */
  pEList = sqlite3ExprListAppend(pParse, 0, sqlite3Expr(db, TK_ID, "_rowid_"));
  if( pRowid ){
    pEList = sqlite3ExprListAppend(pParse, pEList,
                                   sqlite3ExprDup(db, pRowid, 0));
  }
  assert( pTab->iPKey<0 );
  for(i=0; i<pTab->nCol; i++){
    if( aXRef[i]>=0 ){
      pExpr = sqlite3ExprDup(db, pChanges->a[aXRef[i]].pExpr, 0);
    }else{
      pExpr = sqlite3Expr(db, TK_ID, pTab->aCol[i].zName);
    }
    pEList = sqlite3ExprListAppend(pParse, pEList, pExpr);
  }
  pSelect = sqlite3SelectNew(pParse, pEList, pSrc, pWhere, 0, 0, 0, 0, 0, 0);
  
  /*创建临时表,更新结果将存储。
  */
  assert( v );
  ephemTab = pParse->nTab++;
  sqlite3VdbeAddOp2(v, OP_OpenEphemeral, ephemTab, pTab->nCol+1+(pRowid!=0));
  sqlite3VdbeChangeP5(v, BTREE_UNORDERED);

  /*填补这一短暂的表 
  */
  sqlite3SelectDestInit(&dest, SRT_Table, ephemTab);
  sqlite3Select(pParse, pSelect, &dest);

  /* 生成代码扫描临时表和调用VUpdate。 */
  iReg = ++pParse->nMem;
  pParse->nMem += pTab->nCol+1;
  addr = sqlite3VdbeAddOp2(v, OP_Rewind, ephemTab, 0);
  sqlite3VdbeAddOp3(v, OP_Column,  ephemTab, 0, iReg);
  sqlite3VdbeAddOp3(v, OP_Column, ephemTab, (pRowid?1:0), iReg+1);
  for(i=0; i<pTab->nCol; i++){
    sqlite3VdbeAddOp3(v, OP_Column, ephemTab, i+1+(pRowid!=0), iReg+2+i);
  }
  sqlite3VtabMakeWritable(pParse, pTab);
  sqlite3VdbeAddOp4(v, OP_VUpdate, 0, pTab->nCol+2, iReg, pVTab, P4_VTAB);
  sqlite3VdbeChangeP5(v, onError==OE_Default ? OE_Abort : onError);
  sqlite3MayAbort(pParse);
  sqlite3VdbeAddOp2(v, OP_Next, ephemTab, addr+1);
  sqlite3VdbeJumpHere(v, addr);
  sqlite3VdbeAddOp2(v, OP_Close, ephemTab, 0);

  /* 清扫工作 */
  sqlite3SelectDelete(db, pSelect);  
}
#endif /* SQLITE_OMIT_VIRTUALTABLE */

