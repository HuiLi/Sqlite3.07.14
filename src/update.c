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
** This file contains C code routines that are called by the parser
** to handle UPDATE statements.
*/
#include "sqliteInt.h"

#ifndef SQLITE_OMIT_VIRTUALTABLE
/* Forward declaration */
static void updateVirtualTable(
  Parse *pParse,       /* The parsing context 解析指针 */
  SrcList *pSrc,       /* The virtual table to be modified 要修改的虚拟表 */
  Table *pTab,         /* The virtual table 虚拟表*/
  ExprList *pChanges,  /* The columns to change in the UPDATE statement  处理UPDATE语句中的列变化 */
  Expr *pRowidExpr,    /* Expression used to recompute the rowid   定义的一个表达式，用来验证*/
  int *aXRef,          /* Mapping from columns of pTab to entries in pChanges     映射pTab和pChanges*/
  Expr *pWhere,        /* WHERE clause of the UPDATE statement       where语句*/
  int onError          /* ON CONFLICT strategy    错误的处理*/
);
#endif /* SQLITE_OMIT_VIRTUALTABLE */

/*
** The most recently coded instruction was an OP_Column to retrieve the i-th column of table pTab.  
** This routine sets the P4 parameter of the OP_Column to the default value, if any. 
** The default value of a column is specified by a DEFAULT clause in the 
** column definition. This was either supplied by the user when the table
** was created, or added later to the table definition by an ALTER TABLE
** command.If the latter, then the row-records in the table btree on disk
** may not contain a value for the column and the default value, taken
** from the P4 parameter of the OP_Column instruction, is returned instead.
   最近编码指令是一个OP_Column来检索pTab 所指表中第i 列， 
   这个过程将OP_Column的P4  参数(如果有的话) 设定为默认值，
   在列定义的DEFAULT 子句中指定列的默认值
   当表建立起来，通过用户提供，或通过ALTER TABLE 命令后来添加到表定义中
   如果是后者，那么磁盘上btree表的行记录可能不包含列值和默认值，从 OP_Column 指令的P4 参数所取的值，则返回代替
** If the former, then all row-records are guaranteed to include a value
** for the column and the P4 value is not required.
** 如果是前者，那么所有行记录对于列有一个值，并且P4的值不是必须的。 

** Column definitions created by an ALTER TABLE command may only have 
** literal default values specified: a number, null or a string. 
   ALTER TABLE命令创建的列定义可能只有文字指定默认值:一个数字,null或一个字符串。

** （If a more complicated default expression value was provided, it is evaluated 
** when the ALTER TABLE is executed and one of the literal values written
** into the sqlite_master table.)
   如果提供了一个更复杂的默认表达式值,当ALTER TABLE语句执行，一个文字值写入sqlite_master table的时候评估。

** Therefore, the P4 parameter is only required if the default value for
** the column is a literal number, string or null. The sqlite3ValueFromExpr()
** function is capable of transforming these types of expressions into
** sqlite3_value objects.
   因此,如果列的默认值是一个文本数字,字符串或null，P4参数是必要的。 
   sqlite3ValueFromExpr()函数可以将这些类型的表达式转换为sqlite3_value对象。

** If parameter iReg is not negative, code an OP_RealAffinity instruction
** on register iReg. This is used when an equivalent integer value is 
** stored in place of an 8-byte floating point value in order to save 
** space.
   如果参数iReg非负,在寄存器iReg上编码一个OP RealAffinity指令。
   这是当一个相等的整数值存储在一个8字节浮点值的地方,以节省空间。
*/
void sqlite3ColumnDefault(Vdbe *v, Table *pTab, int i, int iReg){
  assert( pTab!=0 );
  if( !pTab->pSelect ){
    sqlite3_value *pValue;
    u8 enc = ENC(sqlite3VdbeDb(v));
    Column *pCol = &pTab->aCol[i];
    VdbeComment((v, "%s.%s", pTab->zName, pCol->zName));
    assert( i<pTab->nCol );
    sqlite3ValueFromExpr(sqlite3VdbeDb(v), pCol->pDflt, enc, 
                         pCol->affinity, &pValue);
    if( pValue ){
      sqlite3VdbeChangeP4(v, -1, (const char *)pValue, P4_MEM);
    }
#ifndef SQLITE_OMIT_FLOATING_POINT
    if( iReg>=0 && pTab->aCol[i].affinity==SQLITE_AFF_REAL ){
      sqlite3VdbeAddOp1(v, OP_RealAffinity, iReg);
    }
#endif
  }
}

/*
** Process an UPDATE statement.
**
**   UPDATE OR IGNORE table_wxyz SET a=b, c=d WHERE e<5 AND f NOT NULL;
**          \_______/ \________/     \______/       \________________/
*            onError   pTabList      pChanges             pWhere
*/
void sqlite3Update(
  Parse *pParse,         /* The parser context 解析指针*/
  SrcList *pTabList,     /* The table in which we should change things  指向需要修改的表*/
  ExprList *pChanges,    /* Things to be changed         改变的表*/
  Expr *pWhere,          /* The WHERE clause.  May be null     WHERE语句，可以为空*/
  int onError            /* How to handle constraint errors      错误的处理*/
){
  int i, j;              /* Loop counters      循环计数器*/
  Table *pTab;           /* The table to be updated      要被更新的表*/
  int addr = 0;          /* VDBE instruction address of the start of the loop    循环开始的地址*/
  WhereInfo *pWInfo;     /* Information about the WHERE clause WHERE    WHERE语句的信息*/
  Vdbe *v;               /* The virtual database engine   虚拟数据库引擎*/
  Index *pIdx;           /* For looping over indices   循环指数*/
  int nIdx;              /* Number of indices that need updating 设定当达到指标的时候需要更新*/
  int iCur;              /* VDBE Cursor number of pTab        pTab的VDBE的指针数量*/
  sqlite3 *db;           /* The database structure   数据库结构 */
  int *aRegIdx = 0;      /* One register assigned to each index to be updated   一个寄存器分配给每个索引更新*/
  int *aXRef = 0;        /* aXRef[i] is the index in pChanges->a[] of the        
                                             ** an expression for the i-th column of the table.
                                             ** aXRef[i]==-1 if the i-th column is not changed.                       
                                             aXRef[i]的索引pChanges指向一个表达式的第i个列，当aXRef[i]==-1时，第i个列没有改变 
                                            */
  int chngRowid;         /* True if the record number is being changed    处理被改变记录?/
  Expr *pRowidExpr = 0;  /* Expression defining the new record number  定义新的记录号*/
  int openAll = 0;       /* True if all indices need to be opened      如果所有需要的目录被打开，则为真*/
  AuthContext sContext;  /* The authorization context    授权上下文*/
  NameContext sNC;       /* The name-context to resolve expressions in  name-context  解析表达式*/
  int iDb;               /* Database containing the table being updated      被修改的表所在的数据库*/
  int okOnePass;         /* True for one-pass algorithm without the FIFO     适用于一次通过的没有先进先出的算法*/
  int hasFK;             /* True if foreign key processing is required           为真如果外键处理是必需的*/

#ifndef SQLITE_OMIT_TRIGGER
  int isView;            /* True when updating a view (INSTEAD OF trigger)为真当更新视图(而不是触发) */
  Trigger *pTrigger;     /* List of triggers on pTab, if required 表上的触发器列表，如果需要*/
  int tmask;             /* Mask of TRIGGER_BEFORE|TRIGGER_AFTER   BEFORE/AFTER触发器的MASK*/
#endif
  int newmask;           /* Mask of NEW.* columns accessed by BEFORE triggers */

  /* Register Allocations 寄存器分配*/
  int regRowCount = 0;   /* A count of rows changed  行的计数变化*/
  int regOldRowid;       /* The old rowid 旧的Rowid*/
  int regNewRowid;       /* The new rowid 新的Rowid*/
  int regNew;            /* Content of the NEW.* table in triggers */
  int regOld = 0;        /* Content of OLD.* table in triggers */
  int regRowSet = 0;     /* Rowset of rows to be updated */

  memset(&sContext, 0, sizeof(sContext));
  db = pParse->db;
  if( pParse->nErr || db->mallocFailed ){
    goto update_cleanup; 
  }
  assert( pTabList->nSrc==1 );

  /* Locate the table which we want to update找到我们想要更新的表
  */
  pTab = sqlite3SrcListLookup(pParse, pTabList);
  if( pTab==0 ) goto update_cleanup; 
  iDb = sqlite3SchemaToIndex(pParse->db, pTab->pSchema);
//索引模式 
  /* Figure out if we have any triggers and if the table being
  ** updated is a view.
     找出如果有触发器并且更新的这个表是一个视图 
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

  if( sqlite3ViewGetColumnNames(pParse, pTab) ){//视图获得列名 
    goto update_cleanup;
  }
  if( sqlite3IsReadOnly(pParse, pTab, tmask) ){
    goto update_cleanup;
  }
  aXRef = sqlite3DbMallocRaw(db, sizeof(int) * pTab->nCol );//分配内存 
  if( aXRef==0 ) goto update_cleanup;//如果分配内存失败，aXRef是一个映射,从pTab的列映射到pChange 
  for(i=0; i<pTab->nCol; i++) aXRef[i] = -1; 

  /* Allocate a cursors for the main database table and for all indices.给主要的数据库表和所有的目录分配指针 
  ** The index cursors might not be used, but if they are used they索引的指针可能不被使用，但是如果使用索引的指针， 
  ** need to occur right after the database cursor.  So go ahead and他们应该发生在数据库指针的右面 
  ** allocate enough space, just in case.所以继续分配足够的空间以备不时之需。 
  */
  pTabList->a[0].iCursor = iCur = pParse->nTab++;
  for(pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext){
    pParse->nTab++;
  }

  /* Initialize the name-context初始化上下文的名称 */
  memset(&sNC, 0, sizeof(sNC));
  sNC.pParse = pParse;
  sNC.pSrcList = pTabList;

  /* Resolve the column names in all the expressions of the解决更新语句所有表达式的列名 
  ** of the UPDATE statement.  Also find the column index并且在pChange数组中找到要更新的每一个列的列索引 
  ** for each column to be updated in the pChanges array.  For each
  ** column to be updated, make sure we have authorization to change
  ** that column.对于每一个要更新的列，确保我们有权限改变列 
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

  /* Allocate memory for the array aRegIdx[].为数组aRegIdx[]分配内存 
    There is one entry in the array for each index associated with table being updated. 
    对于被更新表的每个索引，在这个数组中都有一个入口 
     Fill in the value with a register number for indices that are to be used
  ** and with zero for unused indices.填写的值的寄存器数量指数使用和未使用的指数为零。
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

  /* Begin generating code. 开始生成代码*/
  v = sqlite3GetVdbe(pParse);
  if( v==0 ) goto update_cleanup;
  if( pParse->nested==0 ) sqlite3VdbeCountChanges(v);
  sqlite3BeginWriteOperation(pParse, 1, iDb);

#ifndef SQLITE_OMIT_VIRTUALTABLE
  /* Virtual tables must be handled separately 虚表必须单独处理*/
  if( IsVirtual(pTab) ){
    updateVirtualTable(pParse, pTabList, pTab, pChanges, pRowidExpr, aXRef,
                       pWhere, onError);
    pWhere = 0;
    pTabList = 0;
    goto update_cleanup;
  }
#endif

  /* Allocate required registers. 分配所需的寄存器*/
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

  /* Start the view context.开始解析上下文 */
  if( isView ){
    sqlite3AuthContextPush(pParse, &sContext, pTab->zName);
  }

  /* If we are trying to update a view, realize that view into
  ** a ephemeral table.
     如果我们尝试更新一个视图，必须要注意到视图是一个临时表 
  */
#if !defined(SQLITE_OMIT_VIEW) && !defined(SQLITE_OMIT_TRIGGER)
  if( isView ){
    sqlite3MaterializeView(pParse, pTab, pWhere, iCur);
  }
#endif

  /* Resolve the column names in all the expressions in the
  ** WHERE clause.
  解决where子句所有表达式的列名 
  */
  if( sqlite3ResolveExprNames(&sNC, pWhere) ){
    goto update_cleanup;
  }

  /* Begin the database scan 开始数据库扫描 
  */
  sqlite3VdbeAddOp3(v, OP_Null, 0, regRowSet, regOldRowid);
  pWInfo = sqlite3WhereBegin(
      pParse, pTabList, pWhere, 0, 0, WHERE_ONEPASS_DESIRED, 0
  );
  if( pWInfo==0 ) goto update_cleanup;
  okOnePass = pWInfo->okOnePass;

  /* Remember the rowid of every item to be updated. 记着要被更新的每一项的rowid 
  */
  sqlite3VdbeAddOp2(v, OP_Rowid, iCur, regOldRowid);
  if( !okOnePass ){
    sqlite3VdbeAddOp2(v, OP_RowSetAdd, regRowSet, regOldRowid);
  }

  /* End the database scan loop结束数据库的扫描循环.
  */
  sqlite3WhereEnd(pWInfo);

  /* Initialize the count of updated rows初始化更新的行数 
  */
  if( (db->flags & SQLITE_CountRows) && !pParse->pTriggerTab ){
    regRowCount = ++pParse->nMem;
    sqlite3VdbeAddOp2(v, OP_Integer, 0, regRowCount);
  }

  if( !isView ){
    /* 
    ** Open every index that needs updating.  Note that if any
    ** index could potentially invoke a REPLACE conflict resolution 
    ** action, then we need to open all indices because we might need
    ** to be deleting some records.
    //打开每个需要更新的索引，需要注意的是如果任何索引都能够潜在的调用REPLACE冲突，那么我们需要打开所有的指标，因为我们可能需要删除一些记录。
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

  /* Top of the update loop */
  if( okOnePass ){
    int a1 = sqlite3VdbeAddOp1(v, OP_NotNull, regOldRowid);
    addr = sqlite3VdbeAddOp0(v, OP_Goto);
    sqlite3VdbeJumpHere(v, a1);
  }else{
    addr = sqlite3VdbeAddOp3(v, OP_RowSetRead, regRowSet, 0, regOldRowid);
  }

  /* Make cursor iCur point to the record that is being updated. If
  ** this record does not exist for some reason (deleted by a trigger,
  ** for example, then jump to the next iteration of the RowSet loop. 
  //游标iCur 指向要被更新的记录，如果此记录出于某种原因不存在(触发器程序引起的删除，比如跳转到RowSet循环的下一次迭代)
  */
  sqlite3VdbeAddOp3(v, OP_NotExists, iCur, addr, regOldRowid);

  /* If the record number will change, set register regNewRowid to
  ** contain the new value. If the record number is not being modified,
  ** then regNewRowid is the same register as regOldRowid, which is
  ** already populated.  
 //如果记录号码改变，设置寄存器regNewRowid 包含新值。如果记录号没有被修改，那么regNewRowid 与regOldRowid 是同一寄存器，并且已经被填充
  */
  assert( chngRowid || pTrigger || hasFK || regOldRowid==regNewRowid );
  if( chngRowid ){
    sqlite3ExprCode(pParse, pRowidExpr, regNewRowid);
    sqlite3VdbeAddOp1(v, OP_MustBeInt, regNewRowid);
  }

  /* If there are triggers on this table, populate an array of registers 
  ** with the required old.* column data.
  //如果该表上存在触发器，用old.* column 的数据来填充寄存器数组
  */
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

  /* Populate the array of registers beginning at regNew with the new
  ** row data. This array is used to check constaints, create the new
  ** table and index records, and as the values for any new.* references
  ** made by triggers.
  ** regNew 赋予了新的行数据，开始填充寄存器数组，这个数组用于检查约束，创建新表和索引记录
  ** If there are one or more BEFORE triggers, then do not populate the
  ** registers associated with columns that are (a) not modified by
  ** this UPDATE statement and (b) not accessed by new.* references. The
  ** values for registers not modified by the UPDATE must be reloaded from 
  ** the database after the BEFORE triggers are fired anyway (as the trigger 
  ** may have modified them). So not loading those that are not going to
  ** be used eliminates some redundant opcodes.
  //如果有一个或者多个BEFORE 触发器，(a)UPDATE语句中没有修改的列(b)没有被new.* references 访问的列都不填充寄存器
  //没有被UPDATE修改的寄存器的值在BEFORE 触发器以任何方式触发后 (作为触发器可能修改寄存器的值)必须从数据库中重新加载，不加载那些不用的多余的操作码
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
        /* This branch loads the value of a column that will not be changed 
        ** into a register. This is done if there are no BEFORE triggers, or
        ** if there are one or more BEFORE triggers that use this value via
        ** a new.* reference in a trigger program.
        //这个分支加载的列值不会在寄存器中改变，这种情况在以下条件下发生:没有BEFORE 触发器，或者有一个/多个BEFORE 触发器通过触发器程序使用这个值
        */
        testcase( i==31 );
        testcase( i==32 );
        sqlite3VdbeAddOp3(v, OP_Column, iCur, i, regNew+i);
        sqlite3ColumnDefault(v, pTab, i, regNew+i);
      }
    }
  }

  /* Fire any BEFORE UPDATE triggers. This happens before constraints are
  ** verified. One could argue that this is wrong.
  //解除BEFORE触发器，这发生在约束验证之前，有人认为这是错误的。
  */
  if( tmask&TRIGGER_BEFORE ){
    sqlite3VdbeAddOp2(v, OP_Affinity, regNew, pTab->nCol);
    sqlite3TableAffinityStr(v, pTab);
    sqlite3CodeRowTrigger(pParse, pTrigger, TK_UPDATE, pChanges, 
        TRIGGER_BEFORE, pTab, regOldRowid, onError, addr);

    /* The row-trigger may have deleted the row being updated. In this
    ** case, jump to the next row. No updates or AFTER triggers are 
    ** required. This behaviour - what happens when the row being updated
    ** is deleted or renamed by a BEFORE trigger - is left undefined in the
    ** documentation.
    //行触发器可能已经删除了要更新的行，在这种情况下跳转到下一行。不更新或者AFTER 触发器是必须的。当被更新的行被删除或者被BEFORE 触发器重新命名，这种行为在文档中未定义。
    */
    sqlite3VdbeAddOp3(v, OP_NotExists, iCur, addr, regOldRowid);

    /* If it did not delete it, the row-trigger may still have modified 
    ** some of the columns of the row being updated. Load the values for 
    ** all columns not modified by the update statement into their 
    ** registers in case this has happened.
	//如果不删除它，行触发器仍可能修改被修改行的某些列，如果出现这种情况，加载所有列值不会被UPDATE 语句修改进他们的寄存器

    */   
    for(i=0; i<pTab->nCol; i++){
      if( aXRef[i]<0 && i!=pTab->iPKey ){
        sqlite3VdbeAddOp3(v, OP_Column, iCur, i, regNew+i);
        sqlite3ColumnDefault(v, pTab, i, regNew+i);
      }
    }
  }

  if( !isView ){
    int j1;                       /* Address of jump instruction */

    /* Do constraint checks. 执行约束检查*/
    sqlite3GenerateConstraintChecks(pParse, pTab, iCur, regNewRowid,
        aRegIdx, (chngRowid?regOldRowid:0), 1, onError, addr, 0);

    /* Do FK constraint checks. 执行外键约束检查 */
    if( hasFK ){
      sqlite3FkCheck(pParse, pTab, regOldRowid, 0);
    }

    /* Delete the index entries associated with the current record. 删除与当前记录相关联的索引条目。 */
    j1 = sqlite3VdbeAddOp3(v, OP_NotExists, iCur, 0, regOldRowid);
    sqlite3GenerateRowIndexDelete(pParse, pTab, iCur, aRegIdx);
  
    /* If changing the record number, delete the old record. 如果更新记录号，删除旧的记录 */
    if( hasFK || chngRowid ){
      sqlite3VdbeAddOp2(v, OP_Delete, iCur, 0);
    }
    sqlite3VdbeJumpHere(v, j1);

    if( hasFK ){
      sqlite3FkCheck(pParse, pTab, 0, regNewRowid);
    }
  
    /* Insert the new index entries and the new record. 插入新的索引条目和新记录*/
    sqlite3CompleteInsertion(pParse, pTab, iCur, regNewRowid, aRegIdx, 1, 0, 0);

    /* Do any ON CASCADE, SET NULL or SET DEFAULT operations required to
    ** handle rows (possibly in other tables) that refer via a foreign key
    ** to the row just updated. 
    //通 过引用被删除行的外键执行任何处理行 (可能在其他表中)  的CASCADE,SET NULL 或SET DEFAULT 操作
    */ 
    if( hasFK ){
      sqlite3FkActions(pParse, pTab, pChanges, regOldRowid);
    }
  }

  /* Increment the row counter 递增行计数器
  */
  if( (db->flags & SQLITE_CountRows) && !pParse->pTriggerTab){
    sqlite3VdbeAddOp2(v, OP_AddImm, regRowCount, 1);
  }

  sqlite3CodeRowTrigger(pParse, pTrigger, TK_UPDATE, pChanges, 
      TRIGGER_AFTER, pTab, regOldRowid, onError, addr);

  /* Repeat the above with the next record to be updated, until
  ** all record selected by the WHERE clause have been updated.
  //对下一个要更行的记录重复上述操作，直到被WHERE 子句中选择的所有的记录被更新
  */
  sqlite3VdbeAddOp2(v, OP_Goto, 0, addr);
  sqlite3VdbeJumpHere(v, addr);

  /* Close all tables 关闭所有表 */
  for(i=0, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, i++){
    assert( aRegIdx );
    if( openAll || aRegIdx[i]>0 ){
      sqlite3VdbeAddOp2(v, OP_Close, iCur+i+1, 0);
    }
  }
  sqlite3VdbeAddOp2(v, OP_Close, iCur, 0);

  /* Update the sqlite_sequence table by storing the content of the
  ** maximum rowid counter values recorded while inserting into
  ** autoincrement tables.
  //通过存储在插入到自动增量的表记录到的最大的rowid计数器值的内容更新sqlite_sequence表。
  */
  if( pParse->nested==0 && pParse->pTriggerTab==0 ){
    sqlite3AutoincrementEnd(pParse);
  }

  /*
  ** Return the number of rows that were changed. If this routine is 
  ** generating code because of a call to sqlite3NestedParse(), do not
  ** invoke the callback function.
  //返回被删除行的编号，如果这个过程正在生成代码是因为调用了函数sqlite3NestedParse(),没有调用回滚函数
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
/* Make sure "isView" and other macros defined above are undefined. Otherwise
** thely may interfere with compilation of other functions in this file
** (or in another file, if this file becomes part of the amalgamation).
//确保它是一个视图以及它上边没有定义的其他宏定义，要不然它们这个文件中其他功能的汇编(或者在另一文件中，如果这个文件成为合并的一部分)
*/
#ifdef isView
 #undef isView
#endif
#ifdef pTrigger
 #undef pTrigger
#endif

#ifndef SQLITE_OMIT_VIRTUALTABLE
/*
** Generate code for an UPDATE of a virtual table.生成虚拟表更新代码
**
** The strategy is that we create an ephemerial table that contains
** for each row to be changed:
**创建一个临时表，该表包含要修改的每一行
**   (A)  The original rowid of that row.原始的行rowid
**   (B)  The revised rowid for the row. (note1) 修正后的行ROWID
**   (C)  The content of every column in the row.行中每一列的信息
**
** Then we loop over this ephemeral table and for each row in
** the ephermeral table call VUpdate.
**然后我们遍历这个临时表，这个临时表中的每一行调用VUpdate
** When finished, drop the ephemeral table.当完成遍历，删除这个临时表
**
** (note1) Actually, if we know in advance that (A) is always the same
** as (B) we only store (A), then duplicate (A) when pulling
** it out of the ephemeral table before calling VUpdate.
//(注1)其实，如果我们事先知道(A) 始终与(B) 是一样的，我们仅存储(A)，在调用VUpdate之前，当把它从临时表中弄出来的时候复制A
*/
static void updateVirtualTable(
  Parse *pParse,       /* The parsing context 解析上下文*/
  SrcList *pSrc,       /* The virtual table to be modified 修改虚拟表处理*/
  Table *pTab,         /* The virtual table 虚拟表的建立 */
  ExprList *pChanges,  /* The columns to change in the UPDATE statement 处理UPDATE语句中的列变化 */
  Expr *pRowid,        /* Expression used to recompute the rowid 定义的一个表达式，用来验证 */
  int *aXRef,          /* Mapping from columns of pTab to entries in pChanges        获取pTab pChanges条目，之后进行判断是否为0的if语句处*/
  Expr *pWhere,        /* WHERE clause of the UPDATE statement     UPDATE语句中的一种定义*/
  int onError          /* ON CONFLICT strategy    错误处理 */
){
  Vdbe *v = pParse->pVdbe;  /* Virtual machine under construction  搭建一个虚拟机 */
  ExprList *pEList = 0;     /* The result set of the SELECT statement   存放SELECT语句的结果集合*/
  Select *pSelect = 0;      /* The SELECT statement    SELECT定义*/
  Expr *pExpr;              /* Temporary expression    用来临时存放表达式的*/
  int ephemTab;             /* Table holding the result of the SELECT    存放表的结果*/
  int i;                    /* Loop counter  循环计数 */
  int addr;                 /* Address of top of loop   地址的重复使用 */
  int iReg;                 /* First register in set passed to OP_VUpdate    存放++pParse->nMem的值，之后用来做函数中的定值处 */
  sqlite3 *db = pParse->db; /* Database connection   数据库的连接*/
  const char *pVTab = (const char*)sqlite3GetVTable(db, pTab);
  SelectDest dest;

  /* Construct the SELECT statement that will find the new values for
  ** all updated rows. 
  //构建SELECT语句会发现所有更新行的新的值
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
  
  /* Create the ephemeral table into which the update results will
  ** be stored.
  //创建临时表到更新结果被保存
  */
  assert( v );
  ephemTab = pParse->nTab++;
  sqlite3VdbeAddOp2(v, OP_OpenEphemeral, ephemTab, pTab->nCol+1+(pRowid!=0));
  sqlite3VdbeChangeP5(v, BTREE_UNORDERED);

  /* fill the ephemeral table 填补临时表
  */
  sqlite3SelectDestInit(&dest, SRT_Table, ephemTab);
  sqlite3Select(pParse, pSelect, &dest);

  /* Generate code to scan the ephemeral table and call VUpdate. 生成代码来扫描临时表和调用VUpdate*/
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

  /* Cleanup  */
  sqlite3SelectDelete(db, pSelect);  
}
#endif /* SQLITE_OMIT_VIRTUALTABLE */
