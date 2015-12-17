       /*
** 2003 September 6
**   
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file contains code used for creating, destroying, and populating
** a VDBE虚拟数据库引擎 (or an "sqlite3_stmt" as it is known to the outside world.)  Prior
** to version 2.8.7, all this code was combined into the vdbe.c source file.
** But that file was getting too big so this subroutines were split out.
此文件包含用于创建、 销毁、 和填充一个VDBE虚拟数据库引擎（或一个已经众所周知的"sqlite3_stmt"。）
之前的 2.8.7版本，所有这些代码被组合成 vdbe.c 源文件。但该文件变得太大了 ； 所以此子例程被拆分出来。
*/
#include "sqliteInt.h"
#include "vdbeInt.h"



/*
** When debugging the code generator in a symbolic debugger, one can
** set the sqlite3VdbeAddopTrace to 1 and all opcodes will be printed
** as they are added to the instruction stream.
当在一个符号调试器中调试代码生成器，可以将 sqlite3VdbeAddopTrace 设置为 1,打印所有操作码当他们添加到指令流中时。
*/
#ifdef SQLITE_DEBUG
int sqlite3VdbeAddopTrace = 0;
#endif


/*
** Create a new virtual database engine.创建一个新的虚拟数据库引擎
*/
Vdbe *sqlite3VdbeCreate(sqlite3 *db){
  Vdbe *p;
  p = sqlite3DbMallocZero(db, sizeof(Vdbe) );
  if( p==0 ) return 0;
  p->db = db;
  if( db->pVdbe ){
    db->pVdbe->pPrev = p;
  }
  p->pNext = db->pVdbe;
  p->pPrev = 0;
  db->pVdbe = p;
  p->magic = VDBE_MAGIC_INIT;
  return p;
}
/*
上面的函数所描述的是：每个数据库连接中可能有多个活动的虚拟机，这些虚拟机被组织成一个双向链表，pPrev和pNext域分别指向链表中的前趋和后继。
在sqlite3结构中有一个域pVdbe，为指向此双向链表的头指针。新创建的虚拟机插在该双向链表的头部。
*/

/*
** Remember the SQL string for a prepared statement.记住事先声明的SQL语句字符串
*/
void sqlite3VdbeSetSql(Vdbe *p, const char *z, int n, int isPrepareV2){
  assert( isPrepareV2==1 || isPrepareV2==0 );
  if( p==0 ) return;
#ifdef SQLITE_OMIT_TRACE
  if( !isPrepareV2 ) return;
#endif
  assert( p->zSql==0 );
  p->zSql = sqlite3DbStrNDup(p->db, z, n);
  p->isPrepareV2 = (u8)isPrepareV2;
}

/*
** Return the SQL associated with a prepared statement返回与事先声明语句关联的 SQL
     返回结果集中的列数
*/
const char *sqlite3_sql(sqlite3_stmt *pStmt){
  Vdbe *p = (Vdbe *)pStmt;
  return (p && p->isPrepareV2) ? p->zSql : 0;
}

/*
** Swap all content between two VDBE structures.
交换两个 VDBE 结构之间的所有内容。
*/
void sqlite3VdbeSwap(Vdbe *pA, Vdbe *pB){
  Vdbe tmp, *pTmp;
  char *zTmp;
  tmp = *pA;
  *pA = *pB;
  *pB = tmp;
  pTmp = pA->pNext;
  pA->pNext = pB->pNext;
  pB->pNext = pTmp;
  pTmp = pA->pPrev;
  pA->pPrev = pB->pPrev;
  pB->pPrev = pTmp;
  zTmp = pA->zSql;
  pA->zSql = pB->zSql;
  pB->zSql = zTmp;
  pB->isPrepareV2 = pA->isPrepareV2;
}

#ifdef SQLITE_DEBUG
/*
** Turn tracing on or off 打开或关闭跟踪功能
*/
void sqlite3VdbeTrace(Vdbe *p, FILE *trace){
  p->trace = trace;
}
#endif

/*
** Resize the Vdbe.aOp array so that it is at least one op larger than 
** it was.调整Vdbe.aOp数组的大小以至于它是比它大至少一个 op 的 aOp 数组
**
** If an out-of-memory error occurs while resizing the array, return
** SQLITE_NOMEM. In this case Vdbe.aOp and Vdbe.nOpAlloc remain 
** unchanged (this is so that any opcodes already allocated can be 
** correctly deallocated along with the rest of the Vdbe).
如果在调整大小的数组，返回出现内存不足的错误
SQLITE_NOMEM。在这种情况下Vdbe.aOp和Vdbe.nOpAlloc保持
不变（这是使已分配的任何操作码可
随着VDBE的其余部分）正确地释放。
*/
static int growOpArray(Vdbe *p){
  VdbeOp *pNew;
  int nNew = (p->nOpAlloc ? p->nOpAlloc*2 : (int)(1024/sizeof(Op)));
  pNew = sqlite3DbRealloc(p->db, p->aOp, nNew*sizeof(Op));
  if( pNew ){
    p->nOpAlloc = sqlite3DbMallocSize(p->db, pNew)/sizeof(Op);
    p->aOp = pNew;
  }
  return (pNew ? SQLITE_OK : SQLITE_NOMEM);
}

/*
** Add a new instruction to the list of instructions current in the
** VDBE.  Return the address of the new instruction.
在VDBE中添加一个新的指令到当前列表中
，返回新的指令的地址。
**
** Parameters://参数
**
**    p               Pointer to the VDBE//参数p用于指向VDBE
**
**    op              The opcode for this instruction//参数op用于指向指令的操作码
**
**    p1, p2, p3      Operands//操作参数p1，p2，p3
**
** Use the sqlite3VdbeResolveLabel() function to fix an address and
** the sqlite3VdbeChangeP4() function to change the value of the P4
** operand.
使用sqlite3VdbeResolveLabel（）函数以固定的地址和
在sqlite3VdbeChangeP4（）函数改变P4的价值
操作数。
*/
int sqlite3VdbeAddOp3(Vdbe *p, int op, int p1, int p2, int p3){
  int i;
  VdbeOp *pOp;

  i = p->nOp;
  assert( p->magic==VDBE_MAGIC_INIT );
  assert( op>0 && op<0xff );
  if( p->nOpAlloc<=i ){
    if( growOpArray(p) ){
      return 1;
    }
  }
  p->nOp++;
  pOp = &p->aOp[i];
  pOp->opcode = (u8)op;
  pOp->p5 = 0;
  pOp->p1 = p1;
  pOp->p2 = p2;
  pOp->p3 = p3;
  pOp->p4.p = 0;
  pOp->p4type = P4_NOTUSED;
#ifdef SQLITE_DEBUG
  pOp->zComment = 0;
  if( sqlite3VdbeAddopTrace ) sqlite3VdbePrintOp(0, i, &p->aOp[i]);
#endif
#ifdef VDBE_PROFILE
  pOp->cycles = 0;
  pOp->cnt = 0;
#endif
  return i;
}
int sqlite3VdbeAddOp0(Vdbe *p, int op){
  return sqlite3VdbeAddOp3(p, op, 0, 0, 0);
}
int sqlite3VdbeAddOp1(Vdbe *p, int op, int p1){
  return sqlite3VdbeAddOp3(p, op, p1, 0, 0);
}
int sqlite3VdbeAddOp2(Vdbe *p, int op, int p1, int p2){
  return sqlite3VdbeAddOp3(p, op, p1, p2, 0);
}


/*
** Add an opcode that includes the p4 value as a pointer.
添加一个包含 p4 值作为指针的操作码
*/
int sqlite3VdbeAddOp4(
  Vdbe *p,            /* Add the opcode to this VM 添加到这个虚拟机的操作码 */
  int op,             /* The new opcode 定义一个新参数op */
  int p1,             /* The P1 operand 操作数p1*/
  int p2,             /* The P2 operand 操作数p2*/
  int p3,             /* The P3 operand 操作数p3*/
  const char *zP4,    /* The P4 operand 操作数p4*/
  int p4type          /* P4 operand type P4 操作数类型为int型*/
){
  int addr = sqlite3VdbeAddOp3(p, op, p1, p2, p3);
  sqlite3VdbeChangeP4(p, addr, zP4, p4type);
  return addr;
}

/*
** Add an OP_ParseSchema opcode.  This routine is broken out from
** sqlite3VdbeAddOp4() since it needs to also needs to mark all btrees
** as having been used.
添加 一个OP_ParseSchema 操作码。这个实例被解决是从它需要的 sqlite3VdbeAddOp4()到也需要标记被使用的所有 btree。
**
** The zWhere string must have been obtained from sqlite3_malloc().
** This routine will take ownership of the allocated memory.
从sqlite3_malloc必须是已获得的zWhere串（）。
这个程序将采取分配的内存的所有权。
*/
void sqlite3VdbeAddParseSchemaOp(Vdbe *p, int iDb, char *zWhere){
  int j;
  int addr = sqlite3VdbeAddOp3(p, OP_ParseSchema, iDb, 0, 0);
  sqlite3VdbeChangeP4(p, addr, zWhere, P4_DYNAMIC);
  for(j=0; j<p->db->nDb; j++) sqlite3VdbeUsesBtree(p, j);
}

/*
** Add an opcode that includes the p4 value as an integer.
添加一个包含 p4 值为整数的操作码
*/
int sqlite3VdbeAddOp4Int(
  Vdbe *p,            /* Add the opcode to this VM 指向虚拟机的操作码p*/
  int op,             /* The new opcode 定义一个新参数op*/
  int p1,             /* The P1 operand 操作数p1*/
  int p2,             /* The P2 operand 操作数p2*/
  int p3,             /* The P3 operand 操作数p3*/
  int p4              /* The P4 operand as an integer 操作数p4作为一个整数*/
){
  int addr = sqlite3VdbeAddOp3(p, op, p1, p2, p3);
  sqlite3VdbeChangeP4(p, addr, SQLITE_INT_TO_PTR(p4), P4_INT32);
  return addr;
}

/*
** Create a new symbolic label for an instruction that has yet to be
** coded.  The symbolic label is really just a negative number.  The
** label can be used as the P2 value of an operation.  Later, when
** the label is resolved to a specific address, the VDBE will scan
** through its operation list and change all values of P2 which match
** the label into the resolved address.
创建一个还没有被编码的新的符号标签的指令，这个符号标签只是仅仅表示一个负数
这个标签可以被用作操作的 P2 值。然后，当标签解析为一个特定的地址，VDBE 将通过其操作列表扫描并更改 P2 的所有值
与之匹配标签的解决地址。
**
** The VDBE knows that a P2 value is a label because labels are
** always negative and P2 values are suppose to be non-negative.
** Hence, a negative P2 value is a label that has yet to be resolved.
VDBE 知道 P2 值是一个标签，因为标签总是负数，P2 值是假设为非负数。
因此，一个负的 P2 值是一个尚未解决的标签。
**
** Zero is returned if a malloc() fails.如果函数malloc () 出现错误则返回零。
*/
int sqlite3VdbeMakeLabel(Vdbe *p){
  int i = p->nLabel++;
  assert( p->magic==VDBE_MAGIC_INIT );
  if( (i & (i-1))==0 ){
    p->aLabel = sqlite3DbReallocOrFree(p->db, p->aLabel, 
                                       (i*2+1)*sizeof(p->aLabel[0]));
  }
  if( p->aLabel ){
    p->aLabel[i] = -1;
  }
  return -1-i;
}

/*
** Resolve label "x" to be the address of the next instruction to
** be inserted.  The parameter "x" must have been obtained from
** a prior call to sqlite3VdbeMakeLabel().
解析标签的“x”是下一个指令的地址
插入。参数“X”必须已经从获得
以sqlite3VdbeMakeLabel先前调用（）。
*/
void sqlite3VdbeResolveLabel(Vdbe *p, int x){
  int j = -1-x;
  assert( p->magic==VDBE_MAGIC_INIT );
  assert( j>=0 && j<p->nLabel );
  if( p->aLabel ){
    p->aLabel[j] = p->nOp;
  }
}

/*
** Mark the VDBE as one that can only be run one time.
标志VDBE并且只能运行一次。
*/
void sqlite3VdbeRunOnlyOnce(Vdbe *p){
  p->runOnlyOnce = 1;
}

#ifdef SQLITE_DEBUG /* sqlite3AssertMayAbort() logic */ //sqlite3AssertMayAbort()逻辑函数

/*
** The following type and function are used to iterate through all opcodes
** in a Vdbe main program and each of the sub-programs (triggers) it may 
** invoke directly or indirectly. It should be used as follows:
下面的类型和函数是用来遍历所有在Vdbe主程序中的操作码和各子程序(触发器)它可能被
直接或间接地调用。它应该按照以下方式使用:
**
**   Op *pOp;
**   VdbeOpIter sIter;
**
**   memset(&sIter, 0, sizeof(sIter));
**   sIter.v = v;                            // v is of type Vdbe* 
**   while( (pOp = opIterNext(&sIter)) ){
**     // Do something with pOp
**   }
**   sqlite3DbFree(v->db, sIter.apSub);
** 
*/
typedef struct VdbeOpIter VdbeOpIter;
struct VdbeOpIter {
  Vdbe *v;                   /* Vdbe to iterate through the opcodes of  Vdbe遍历操作码 */ 
  SubProgram **apSub;        /* Array of subprograms 子程序数组 */
  int nSub;                  /* Number of entries in apSub apSub的条目数量 */
  int iAddr;                 /* Address of next instruction to return  下一个指令的返回地址*/
  int iSub;                  /* 0 = main program, 1 = first sub-program etc. 0表示 主程序，1表示第一个子程序*/
};
static Op *opIterNext(VdbeOpIter *p){
  Vdbe *v = p->v;
  Op *pRet = 0;
  Op *aOp;
  int nOp;

  if( p->iSub<=p->nSub ){

    if( p->iSub==0 ){
      aOp = v->aOp;
      nOp = v->nOp;
    }else{
      aOp = p->apSub[p->iSub-1]->aOp;
      nOp = p->apSub[p->iSub-1]->nOp;
    }
    assert( p->iAddr<nOp );

    pRet = &aOp[p->iAddr];
    p->iAddr++;
    if( p->iAddr==nOp ){
      p->iSub++;
      p->iAddr = 0;
    }
  
    if( pRet->p4type==P4_SUBPROGRAM ){
      int nByte = (p->nSub+1)*sizeof(SubProgram*);
      int j;
      for(j=0; j<p->nSub; j++){
        if( p->apSub[j]==pRet->p4.pProgram ) break;
      }
      if( j==p->nSub ){
        p->apSub = sqlite3DbReallocOrFree(v->db, p->apSub, nByte);
        if( !p->apSub ){
          pRet = 0;
        }else{
          p->apSub[p->nSub++] = pRet->p4.pProgram;
        }
      }
    }
  }

  return pRet;
}

/*
** Check if the program stored in the VM associated with pParse may
** throw an ABORT exception (causing the statement, but not entire transaction
** to be rolled back). This condition is true if the main program or any
** sub-programs contains any of the following:
检查存储在VM程序与pParse关联机构可能会
抛出中止异常（导致的声明，而不是整个事务被回滚）
如果主程序或任何子程序包含下列任一操作，这个条件为真：
**
**   *  OP_Halt with P1=SQLITE_CONSTRAINT and P2=OE_Abort.
**   *  OP_HaltIfNull with P1=SQLITE_CONSTRAINT and P2=OE_Abort.
**   *  OP_Destroy
**   *  OP_VUpdate
**   *  OP_VRename
**   *  OP_FkCounter with P2==0 (immediate foreign key constraint)
**
** Then check that the value of Parse.mayAbort is true if an
** ABORT may be thrown, or false otherwise. Return true if it does
** match, or false otherwise. This function is intended to be used as
** part of an assert statement in the compiler. Similar to:
然后检查分析的值。如果ABORT可能被抛出则mayAbort为真，否则为假。如果是匹配，则返回 true
否则为false。此函数被打算用于作为在编译器 assert 语句中的一部分。类似于：
**
**   assert( sqlite3VdbeAssertMayAbort(pParse->pVdbe, pParse->mayAbort) );
*/
int sqlite3VdbeAssertMayAbort(Vdbe *v, int mayAbort){
  int hasAbort = 0;
  Op *pOp;
  VdbeOpIter sIter;
  memset(&sIter, 0, sizeof(sIter));
  sIter.v = v;

  while( (pOp = opIterNext(&sIter))!=0 ){
    int opcode = pOp->opcode;
    if( opcode==OP_Destroy || opcode==OP_VUpdate || opcode==OP_VRename 
#ifndef SQLITE_OMIT_FOREIGN_KEY
     || (opcode==OP_FkCounter && pOp->p1==0 && pOp->p2==1) 
#endif
     || ((opcode==OP_Halt || opcode==OP_HaltIfNull) 
      && (pOp->p1==SQLITE_CONSTRAINT && pOp->p2==OE_Abort))
    ){
      hasAbort = 1;
      break;
    }
  }
  sqlite3DbFree(v->db, sIter.apSub);

  /* Return true if hasAbort==mayAbort. Or if a malloc failure occured.
  ** If malloc failed, then the while() loop above may not have iterated
  ** through all opcodes and hasAbort may be set incorrectly. Return
  ** true for this case to prevent the assert() in the callers frame
  ** from failing.  
如果hasAbort = = mayAbort返回true，或者malloc产生失败则返回真。如果malloc失败,那么上面的 while() 循环可能不能
遍历所有操作码，并且 hasAbort 可能设置不正确。这种情况下返回true来防止调用框架中的assert()失败。
 */
  return ( v->db->mallocFailed || hasAbort==mayAbort );
}
#endif /* SQLITE_DEBUG - the sqlite3AssertMayAbort() function */

/*
** Loop through the program looking for P2 values that are negative
** on jump instructions.  Each such value is a label.  Resolve the
** label by setting the P2 value to its correct non-zero value.
通过该程序循环寻找P2的值是负
在跳转指令。每个这样的值是一个标签。解析
标签通过设置在P2值到它的正确的非零值。
**
** This routine is called once after all opcodes have been inserted.调用这个例程一旦所有的操作码被插入后。
**
** Variable *pMaxFuncArgs is set to the maximum value of any P2 argument 
** to an OP_Function, OP_AggStep or OP_VFilter opcode. This is used by 
** sqlite3VdbeMakeReady() to size the Vdbe.apArg[] array.
设置传递给 OP_Function、 OP_AggStep 或 OP_VFilter 操作码的变量* pMaxFuncArgs 的 P2 参数的最大值。
这被 sqlite3VdbeMakeReady() 使用来设置Vdbe.apArg[] 数组的大小。
**
** The Op.opflags field is set on all opcodes.//在所有操作码中设置Op.opflags 字段.
*/
static void resolveP2Values(Vdbe *p, int *pMaxFuncArgs){
  int i;
  int nMaxArgs = *pMaxFuncArgs;
  Op *pOp;
  int *aLabel = p->aLabel;
  p->readOnly = 1;
  for(pOp=p->aOp, i=p->nOp-1; i>=0; i--, pOp++){
    u8 opcode = pOp->opcode;

    pOp->opflags = sqlite3OpcodeProperty[opcode];
    if( opcode==OP_Function || opcode==OP_AggStep ){
      if( pOp->p5>nMaxArgs ) nMaxArgs = pOp->p5;
    }else if( (opcode==OP_Transaction && pOp->p2!=0) || opcode==OP_Vacuum ){
      p->readOnly = 0;
#ifndef SQLITE_OMIT_VIRTUALTABLE
    }else if( opcode==OP_VUpdate ){
      if( pOp->p2>nMaxArgs ) nMaxArgs = pOp->p2;
    }else if( opcode==OP_VFilter ){
      int n;
      assert( p->nOp - i >= 3 );
      assert( pOp[-1].opcode==OP_Integer );
      n = pOp[-1].p1;
      if( n>nMaxArgs ) nMaxArgs = n;
#endif
    }else if( opcode==OP_Next || opcode==OP_SorterNext ){
      pOp->p4.xAdvance = sqlite3BtreeNext;
      pOp->p4type = P4_ADVANCE;
    }else if( opcode==OP_Prev ){
      pOp->p4.xAdvance = sqlite3BtreePrevious;
      pOp->p4type = P4_ADVANCE;
    }

    if( (pOp->opflags & OPFLG_JUMP)!=0 && pOp->p2<0 ){
      assert( -1-pOp->p2<p->nLabel );
      pOp->p2 = aLabel[-1-pOp->p2];
    }
  }
  sqlite3DbFree(p->db, p->aLabel);
  p->aLabel = 0;

  *pMaxFuncArgs = nMaxArgs;
}

/*
** Return the address of the next instruction to be inserted.//返回插入下一条指令的地址。
*/
int sqlite3VdbeCurrentAddr(Vdbe *p){
  assert( p->magic==VDBE_MAGIC_INIT );
  return p->nOp;
}

/*
** This function returns a pointer to the array of opcodes associated with
** the Vdbe passed as the first argument. It is the callers responsibility
** to arrange for the returned array to be eventually freed using the 
** vdbeFreeOpArray() function.
这个函数返回一个指向数组的指针操作码与Vdbe相关联作为第一个参数传递。这是
调用者的职责来安排最终使用vdbeFreeOpArray函数释放的返回数组。
**
** Before returning, *pnOp is set to the number of entries in the returned
** array. Also, *pnMaxArg is set to the larger of its current value and 
** the number of entries in the Vdbe.apArg[] array required to execute the 
** returned program.
在返回之前,* pnOp在返回数组中被设置为条目的数量。同时,* pnMaxArg设置为比当前值更大的
值和在Vdbe.apArg[]数组中条目的数量要求必须执行返回程序。
*/
VdbeOp *sqlite3VdbeTakeOpArray(Vdbe *p, int *pnOp, int *pnMaxArg){
  VdbeOp *aOp = p->aOp;
  assert( aOp && !p->db->mallocFailed );

/* Check that sqlite3VdbeUsesBtree() was not called on this VM 检查并不参与在这个VM中的sqlite3VdbeUsesBtree()*/
  assert( p->btreeMask==0 );

  resolveP2Values(p, pnMaxArg);
  *pnOp = p->nOp;
  p->aOp = 0;
  return aOp;
}

/*
** Add a whole list of operations to the operation stack.  Return the
** address of the first operation added.
添加整个操作列表到操作堆栈。返回第一个添加操作的地址。
*/
int sqlite3VdbeAddOpList(Vdbe *p, int nOp, VdbeOpList const *aOp){
  int addr;
  assert( p->magic==VDBE_MAGIC_INIT );
  if( p->nOp + nOp > p->nOpAlloc && growOpArray(p) ){
    return 0;
  }
  addr = p->nOp;
  if( ALWAYS(nOp>0) ){
    int i;
    VdbeOpList const *pIn = aOp;
    for(i=0; i<nOp; i++, pIn++){
      int p2 = pIn->p2;
      VdbeOp *pOut = &p->aOp[i+addr];
      pOut->opcode = pIn->opcode;
      pOut->p1 = pIn->p1;
      if( p2<0 && (sqlite3OpcodeProperty[pOut->opcode] & OPFLG_JUMP)!=0 ){
        pOut->p2 = addr + ADDR(p2);
      }else{
        pOut->p2 = p2;
      }
      pOut->p3 = pIn->p3;
      pOut->p4type = P4_NOTUSED;
      pOut->p4.p = 0;
      pOut->p5 = 0;
#ifdef SQLITE_DEBUG
      pOut->zComment = 0;
      if( sqlite3VdbeAddopTrace ){
        sqlite3VdbePrintOp(0, i+addr, &p->aOp[i+addr]);
      }
#endif
    }
    p->nOp += nOp;
  }
  return addr;
}

/*
** Change the value of the P1 operand for a specific instruction.
** This routine is useful when a large program is loaded from a
** static array using sqlite3VdbeAddOpList but we want to make a
** few minor changes to the program.
为一个特定的指令改变操作数P1的值。当一个大的程序被一个使用
sqlite3VdbeAddOpList的静态数组加载时这是非常有用的，但是我们想对这个程序做
一个细微的改变。
*/
void sqlite3VdbeChangeP1(Vdbe *p, u32 addr, int val){
  assert( p!=0 );
  if( ((u32)p->nOp)>addr ){
    p->aOp[addr].p1 = val;
  }
}

/*
** Change the value of the P2 operand for a specific instruction.
** This routine is useful for setting a jump destination.
为一个特定的指令改变操作数P2的值。
在用于设置跳转目标，时这个例程非常有用。
*/
void sqlite3VdbeChangeP2(Vdbe *p, u32 addr, int val){
  assert( p!=0 );
  if( ((u32)p->nOp)>addr ){
    p->aOp[addr].p2 = val;
  }
}

/*
** Change the value of the P3 operand for a specific instruction.
为一个特定的指令改变操作数P3的值。
*/
void sqlite3VdbeChangeP3(Vdbe *p, u32 addr, int val){
  assert( p!=0 );
  if( ((u32)p->nOp)>addr ){
    p->aOp[addr].p3 = val;
  }
}

/*
** Change the value of the P5 operand for the most recently
** added operation.
为最新添加的操作更改操作数 P5 的值。
*/
void sqlite3VdbeChangeP5(Vdbe *p, u8 val){
  assert( p!=0 );
  if( p->aOp ){
    assert( p->nOp>0 );
    p->aOp[p->nOp-1].p5 = val;
  }
}

/*
** Change the P2 operand of instruction addr so that it points to
** the address of the next instruction to be coded.
更改的P2操作数的指令地址，以便它指向下一条指令的地址编码，进行编码。
*/
void sqlite3VdbeJumpHere(Vdbe *p, int addr){
  assert( addr>=0 || p->db->mallocFailed );
  if( addr>=0 ) sqlite3VdbeChangeP2(p, addr, p->nOp);
}


/*
** If the input FuncDef structure is ephemeral, then free it.  If
** the FuncDef is not ephermal, then do nothing.
如果输入的 FuncDef 结构是短暂的那么释放它。如果FuncDef 不是短暂的，那么什么都不做。
*/
static void freeEphemeralFunction(sqlite3 *db, FuncDef *pDef){
  if( ALWAYS(pDef) && (pDef->flags & SQLITE_FUNC_EPHEM)!=0 ){
    sqlite3DbFree(db, pDef);
  }
}

static void vdbeFreeOpArray(sqlite3 *, Op *, int);

/*
** Delete a P4 value if necessary.如果必要删除p4的值
*/
static void freeP4(sqlite3 *db, int p4type, void *p4){
  if( p4 ){
    assert( db );
    switch( p4type ){
      case P4_REAL:
      case P4_INT64:
      case P4_DYNAMIC:
      case P4_KEYINFO:
      case P4_INTARRAY:
      case P4_KEYINFO_HANDOFF: {
        sqlite3DbFree(db, p4);
        break;
      }
      case P4_MPRINTF: {
        if( db->pnBytesFreed==0 ) sqlite3_free(p4);
        break;
      }
      case P4_VDBEFUNC: {
        VdbeFunc *pVdbeFunc = (VdbeFunc *)p4;
        freeEphemeralFunction(db, pVdbeFunc->pFunc);
        if( db->pnBytesFreed==0 ) sqlite3VdbeDeleteAuxData(pVdbeFunc, 0);
        sqlite3DbFree(db, pVdbeFunc);
        break;
      }
      case P4_FUNCDEF: {
        freeEphemeralFunction(db, (FuncDef*)p4);
        break;
      }
      case P4_MEM: {
        if( db->pnBytesFreed==0 ){
          sqlite3ValueFree((sqlite3_value*)p4);
        }else{
          Mem *p = (Mem*)p4;
          sqlite3DbFree(db, p->zMalloc);
          sqlite3DbFree(db, p);
        }
        break;
      }
      case P4_VTAB : {
        if( db->pnBytesFreed==0 ) sqlite3VtabUnlock((VTable *)p4);
        break;
      }
    }
  }
}

/*
** Free the space allocated for aOp and any p4 values allocated for the
** opcodes contained within. If aOp is not NULL it is assumed to contain 
** nOp entries. 
免费为 aOp 分配空间和为任何内部的操作码分配 p4 的值。如果 aOp 不是 NULL 那么则被假定包含为 nOp 条目。
*/
static void vdbeFreeOpArray(sqlite3 *db, Op *aOp, int nOp){
  if( aOp ){
    Op *pOp;
    for(pOp=aOp; pOp<&aOp[nOp]; pOp++){
      freeP4(db, pOp->p4type, pOp->p4.p);
#ifdef SQLITE_DEBUG
      sqlite3DbFree(db, pOp->zComment);
#endif     
    }
  }
  sqlite3DbFree(db, aOp);
}

/*
** Link the SubProgram object passed as the second argument into the linked
** list at Vdbe.pSubProgram. This list is used to delete all sub-program
** objects when the VM is no longer required.
链接作为第二个参数的子程序对象传递到Vdbe.pSubProgram链表。这个列表用来删除所有子程序当VM对象不再是必需的时。
*/
void sqlite3VdbeLinkSubProgram(Vdbe *pVdbe, SubProgram *p){
  p->pNext = pVdbe->pProgram;
  pVdbe->pProgram = p;
}

/*
** Change the opcode at addr into OP_Noop 改变操作码的地址为OP_Noop
*/
void sqlite3VdbeChangeToNoop(Vdbe *p, int addr){
  if( p->aOp ){
    VdbeOp *pOp = &p->aOp[addr];
    sqlite3 *db = p->db;
    freeP4(db, pOp->p4type, pOp->p4.p);
    memset(pOp, 0, sizeof(pOp[0]));
    pOp->opcode = OP_Noop;
  }
}

/*
** Change the value of the P4 operand for a specific instruction.
** This routine is useful when a large program is loaded from a
** static array using sqlite3VdbeAddOpList but we want to make a
** few minor changes to the program.
为一个特定的指令改变操作数P1的值。当一个大的程序被从一个使用
sqlite3VdbeAddOpList的静态数组加载时是非常有用的，但是我们想对这个程序做
一个细微的改变。
**
** If n>=0 then the P4 operand is dynamic, meaning that a copy of
** the string is made into memory obtained from sqlite3_malloc().
** A value of n==0 means copy bytes of zP4 up to and including the
** first null byte.  If n>0 then copy n+1 bytes of zP4.
如果n > = 0那么操作数P4是动态的,意思是从sqlite3_malloc()获得的字符串被复制到内存中。
n = = 0意味着复制zP4的字节，包括它的第一个空字节。如果n > 0那么复制n + 1个zP4字节。
**
** If n==P4_KEYINFO it means that zP4 is a pointer to a KeyInfo structure.
** A copy is made of the KeyInfo structure into memory obtained from
** sqlite3_malloc, to be freed when the Vdbe is finalized.
** n==P4_KEYINFO_HANDOFF indicates that zP4 points to a KeyInfo structure
** stored in memory that the caller has obtained from sqlite3_malloc. The 
** caller should not free the allocation, it will be freed when the Vdbe is
** finalized.
如果 n = = P4_KEYINFO，这意味着，zP4 是一个指针，指向 KeyInfo结构。复制是由KeyInfo 结构到从sqlite3_malloc获得的内存组成，
Vdbe最后完成的时候它将会被释放。n = = P4_KEYINFO_HANDOFF 表示，zP4 指向 KeyInfo 结构并存储在内存中，调用者从 sqlite3_malloc 
获得。调用者不应释放分配，当Vdbe是最后完成的时候它将会被释放。
** 
** Other values of n (P4_STATIC, P4_COLLSEQ etc.) indicate that zP4 points
** to a string or structure that is guaranteed to exist for the lifetime of
** the Vdbe. In these cases we can just copy the pointer.
其它 n值 （P4_STATIC，P4_COLLSEQ 等等。） 表示zP4指向字符串或结构，可以确保存在 Vdbe 的生存期。在这些情况下，
我们可以只复制指针
**
** If addr<0 then change P4 on the most recently inserted instruction.
　如果addr < 0那么改变P4在最近插入的指令。
*/
void sqlite3VdbeChangeP4(Vdbe *p, int addr, const char *zP4, int n){
  Op *pOp;
  sqlite3 *db;
  assert( p!=0 );
  db = p->db;
  assert( p->magic==VDBE_MAGIC_INIT );
  if( p->aOp==0 || db->mallocFailed ){
    if ( n!=P4_KEYINFO && n!=P4_VTAB ) {
      freeP4(db, n, (void*)*(char**)&zP4);
    }
    return;
  }
  assert( p->nOp>0 );
  assert( addr<p->nOp );
  if( addr<0 ){
    addr = p->nOp - 1;
  }
  pOp = &p->aOp[addr];
  freeP4(db, pOp->p4type, pOp->p4.p);
  pOp->p4.p = 0;
  if( n==P4_INT32 ){
    /* Note: this cast is safe, because the origin data point was an int
    ** that was cast to a (const char *).注意：这个转换是安全的,因为原点数据点是int型
　　被强制转换成(const char *) */
    pOp->p4.i = SQLITE_PTR_TO_INT(zP4);
    pOp->p4type = P4_INT32;
  }else if( zP4==0 ){
    pOp->p4.p = 0;
    pOp->p4type = P4_NOTUSED;
  }else if( n==P4_KEYINFO ){
    KeyInfo *pKeyInfo;
    int nField, nByte;

    nField = ((KeyInfo*)zP4)->nField;
    nByte = sizeof(*pKeyInfo) + (nField-1)*sizeof(pKeyInfo->aColl[0]) + nField;
    pKeyInfo = sqlite3DbMallocRaw(0, nByte);
    pOp->p4.pKeyInfo = pKeyInfo;
    if( pKeyInfo ){
      u8 *aSortOrder;
      memcpy((char*)pKeyInfo, zP4, nByte - nField);
      aSortOrder = pKeyInfo->aSortOrder;
      if( aSortOrder ){
        pKeyInfo->aSortOrder = (unsigned char*)&pKeyInfo->aColl[nField];
        memcpy(pKeyInfo->aSortOrder, aSortOrder, nField);
      }
      pOp->p4type = P4_KEYINFO;
    }else{
      p->db->mallocFailed = 1;
      pOp->p4type = P4_NOTUSED;
    }
  }else if( n==P4_KEYINFO_HANDOFF ){
    pOp->p4.p = (void*)zP4;
    pOp->p4type = P4_KEYINFO;
  }else if( n==P4_VTAB ){
    pOp->p4.p = (void*)zP4;
    pOp->p4type = P4_VTAB;
    sqlite3VtabLock((VTable *)zP4);
    assert( ((VTable *)zP4)->db==p->db );
  }else if( n<0 ){
    pOp->p4.p = (void*)zP4;
    pOp->p4type = (signed char)n;
  }else{
    if( n==0 ) n = sqlite3Strlen30(zP4);
    pOp->p4.z = sqlite3DbStrNDup(p->db, zP4, n);
    pOp->p4type = P4_DYNAMIC;
  }
}

#ifndef NDEBUG
/*
** Change the comment on the most recently coded instruction.  Or
** insert a No-op and add the comment to that new instruction.  This
** makes the code easier to read during debugging.  None of this happens
** in a production build.
更改最近编码指令的注释。或插入无操作并为该新的指令添加注释。这使代码在调试过程中易于阅读。在调试过程中不会
发生任何这种情况。
*/
static void vdbeVComment(Vdbe *p, const char *zFormat, va_list ap){
  assert( p->nOp>0 || p->aOp==0 );
  assert( p->aOp==0 || p->aOp[p->nOp-1].zComment==0 || p->db->mallocFailed );
  if( p->nOp ){
    assert( p->aOp );
    sqlite3DbFree(p->db, p->aOp[p->nOp-1].zComment);
    p->aOp[p->nOp-1].zComment = sqlite3VMPrintf(p->db, zFormat, ap);
  }
}
void sqlite3VdbeComment(Vdbe *p, const char *zFormat, ...){
  va_list ap;
  if( p ){
    va_start(ap, zFormat);
    vdbeVComment(p, zFormat, ap);
    va_end(ap);
  }
}
void sqlite3VdbeNoopComment(Vdbe *p, const char *zFormat, ...){
  va_list ap;
  if( p ){
    sqlite3VdbeAddOp0(p, OP_Noop);
    va_start(ap, zFormat);
    vdbeVComment(p, zFormat, ap);
    va_end(ap);
  }
}
#endif  /* NDEBUG */

/*
** Return the opcode for a given address.  If the address is -1, then
** return the most recently inserted opcode.
返回给定地址的操作码。如果地址是-1，那么返回最近插入的操作码
**
** If a memory allocation error has occurred prior to the calling of this
** routine, then a pointer to a dummy VdbeOp will be returned.  That opcode
** is readable but not writable, though it is cast to a writable value.
** The return of a dummy opcode allows the call to continue functioning
** after a OOM fault without having to check to see if the return from 
** this routine is a valid pointer.  But because the dummy.opcode is 0,
** dummy will never be written to.  This is verified by code inspection and
** by running with Valgrind.
之前调用的例程如果发生内存分配错误,那么指向虚拟的 VdbeOp的指针将会被返回。操作码是可读的,但是不是可写的,虽然它强制转换为
可写入的值。如果从这个例程返回是一个有效的指针，返回的虚拟操作码允许在OOM 发生故障后继续调用，而无需检查从该程序返回的
是否是一个有效的指针。但由于虚拟操作码为0,虚拟将永远不会被写入。这是通过代码检查,通过运行验证的。
**
** About the #ifdef SQLITE_OMIT_TRACE:  Normally, this routine is never called
** unless p->nOp>0.  This is because in the absense of SQLITE_OMIT_TRACE,
** an OP_Trace instruction is always inserted by sqlite3VdbeGet() as soon as
** a new VDBE is created.  So we are free to set addr to p->nOp-1 without
** having to double-check to make sure that the result is non-negative. But
** if SQLITE_OMIT_TRACE is defined, the OP_Trace is omitted and we do need to
** check the value of p->nOp-1 before continuing.
关于 #ifdef SQLITE_OMIT_TRACE： 通常情况下，它永远不会被调用，除非 p-> nOp > 0。这是因为缺乏
SQLITE_OMIT_TRACE，OP_Trace 指令总是被插入sqlite3VdbeGet()，一旦一个新的VDBE被创建。我们就可以自由地将地址设置为 p-> nOp 1，
而无须双重检查以确保其结果为非负值。但是如果定义了 SQLITE_OMIT_TRACE，则省略了 OP_Trace，那么我们需要在继续之前检查 p-> nOp 1 的值。
*/
VdbeOp *sqlite3VdbeGetOp(Vdbe *p, int addr){
  /* C89 specifies that the constant "dummy" will be initialized to all
  ** zeros, which is correct.  MSVC generates a warning, nevertheless. 
 C89指定常数“dummy”将被初始化为零,这是正确的。然而，MSVC将会生成一个警告*/
  static VdbeOp dummy;  /* Ignore the MSVC warning about no initializer 忽略关于没有初始化设定项的 MSVC 警告*/
  assert( p->magic==VDBE_MAGIC_INIT );
  if( addr<0 ){
#ifdef SQLITE_OMIT_TRACE
    if( p->nOp==0 ) return (VdbeOp*)&dummy;
#endif
    addr = p->nOp - 1;
  }
  assert( (addr>=0 && addr<p->nOp) || p->db->mallocFailed );
  if( p->db->mallocFailed ){
    return (VdbeOp*)&dummy;
  }else{
    return &p->aOp[addr];
  }
}

#if !defined(SQLITE_OMIT_EXPLAIN) || !defined(NDEBUG) \
     || defined(VDBE_PROFILE) || defined(SQLITE_DEBUG)
/*
** Compute a string that describes the P4 parameter for an opcode.
** Use zTemp for any required temporary buffer space.
计算一个字符串，描述的是参数P4 的操作码。使用 zTemp 的任何所需的临时缓冲区空间。
*/
static char *displayP4(Op *pOp, char *zTemp, int nTemp){
  char *zP4 = zTemp;
  assert( nTemp>=20 );
  switch( pOp->p4type ){
    case P4_KEYINFO_STATIC:
    case P4_KEYINFO: {
      int i, j;
      KeyInfo *pKeyInfo = pOp->p4.pKeyInfo;
      sqlite3_snprintf(nTemp, zTemp, "keyinfo(%d", pKeyInfo->nField);
      i = sqlite3Strlen30(zTemp);
      for(j=0; j<pKeyInfo->nField; j++){
        CollSeq *pColl = pKeyInfo->aColl[j];
        if( pColl ){
          int n = sqlite3Strlen30(pColl->zName);
          if( i+n>nTemp-6 ){
            memcpy(&zTemp[i],",...",4);
            break;
          }
          zTemp[i++] = ',';
          if( pKeyInfo->aSortOrder && pKeyInfo->aSortOrder[j] ){
            zTemp[i++] = '-';
          }
          memcpy(&zTemp[i], pColl->zName,n+1);
          i += n;
        }else if( i+4<nTemp-6 ){
          memcpy(&zTemp[i],",nil",4);
          i += 4;
        }
      }
      zTemp[i++] = ')';
      zTemp[i] = 0;
      assert( i<nTemp );
      break;
    }
    case P4_COLLSEQ: {
      CollSeq *pColl = pOp->p4.pColl;
      sqlite3_snprintf(nTemp, zTemp, "collseq(%.20s)", pColl->zName);
      break;
    }
    case P4_FUNCDEF: {
      FuncDef *pDef = pOp->p4.pFunc;
      sqlite3_snprintf(nTemp, zTemp, "%s(%d)", pDef->zName, pDef->nArg);
      break;
    }
    case P4_INT64: {
      sqlite3_snprintf(nTemp, zTemp, "%lld", *pOp->p4.pI64);
      break;
    }
    case P4_INT32: {
      sqlite3_snprintf(nTemp, zTemp, "%d", pOp->p4.i);
      break;
    }
    case P4_REAL: {
      sqlite3_snprintf(nTemp, zTemp, "%.16g", *pOp->p4.pReal);
      break;
    }
    case P4_MEM: {
      Mem *pMem = pOp->p4.pMem;
      if( pMem->flags & MEM_Str ){
        zP4 = pMem->z;
      }else if( pMem->flags & MEM_Int ){
        sqlite3_snprintf(nTemp, zTemp, "%lld", pMem->u.i);
      }else if( pMem->flags & MEM_Real ){
        sqlite3_snprintf(nTemp, zTemp, "%.16g", pMem->r);
      }else if( pMem->flags & MEM_Null ){
        sqlite3_snprintf(nTemp, zTemp, "NULL");
      }else{
        assert( pMem->flags & MEM_Blob );
        zP4 = "(blob)";
      }
      break;
    }
#ifndef SQLITE_OMIT_VIRTUALTABLE
    case P4_VTAB: {
      sqlite3_vtab *pVtab = pOp->p4.pVtab->pVtab;
      sqlite3_snprintf(nTemp, zTemp, "vtab:%p:%p", pVtab, pVtab->pModule);
      break;
    }
#endif
    case P4_INTARRAY: {
      sqlite3_snprintf(nTemp, zTemp, "intarray");
      break;
    }
    case P4_SUBPROGRAM: {
      sqlite3_snprintf(nTemp, zTemp, "program");
      break;
    }
    case P4_ADVANCE: {
      zTemp[0] = 0;
      break;
    }
    default: {
      zP4 = pOp->p4.z;
      if( zP4==0 ){
        zP4 = zTemp;
        zTemp[0] = 0;
      }
    }
  }
  assert( zP4!=0 );
  return zP4;
}
#endif

/*
** Declare to the Vdbe that the BTree object at db->aDb[i] is used.//声明的Vdbe的BTree对象在db - >aDb[i]中被使用。
**
** The prepared statements need to know in advance the complete set of
** attached databases that will be use.  A mask of these databases
** is maintained in p->btreeMask.  The p->lockMask value is the subset of
** p->btreeMask of databases that will require a lock.
预先声明需要提前知道将被使用的附加数据库的完整集合。这些数据库的掩码在p-> btreeMask中被维护。p - > lockMask
值是数据库p - > btreeMask的子集而它需要一个锁。
*/
void sqlite3VdbeUsesBtree(Vdbe *p, int i){
  assert( i>=0 && i<p->db->nDb && i<(int)sizeof(yDbMask)*8 );
  assert( i<(int)sizeof(p->btreeMask)*8 );
  p->btreeMask |= ((yDbMask)1)<<i;
  if( i!=1 && sqlite3BtreeSharable(p->db->aDb[i].pBt) ){
    p->lockMask |= ((yDbMask)1)<<i;
  }
}

#if !defined(SQLITE_OMIT_SHARED_CACHE) && SQLITE_THREADSAFE>0
/*
** If SQLite is compiled to support shared-cache mode and to be threadsafe,
** this routine obtains the mutex associated with each BtShared structure
** that may be accessed by the VM passed as an argument. In doing so it also
** sets the BtShared.db member of each of the BtShared structures, ensuring
** that the correct busy-handler callback is invoked if required.
如果SQLite编译支持共享缓存模式和线程安全,这个例程获得与每个BtShared结构关联的互斥锁,可能被VM访问作为
一个参数传递。这样做也可以设置BtShared.db成员的每个BtShared结构,确保如果需要的时候正确的busy-handler被调用。
**
** If SQLite is not threadsafe but does support shared-cache mode, then
** sqlite3BtreeEnter() is invoked to set the BtShared.db variables
** of all of BtShared structures accessible via the database handle 
** associated with the VM.
如果SQLite不是线程安全的但是支持支持共享缓存模式，则会调用 sqlite3BtreeEnter() 来设置 BtShared.db
所有的与虚拟机相关联的数据库句柄访问的BtShared 结构的变量。
**
** If SQLite is not threadsafe and does not support shared-cache mode, this
** function is a no-op.
如果SQLite既不是线程安全的也不支持缓存共享模式，那么这个功能函数是无操作的。
**
** The p->btreeMask field is a bitmask of all btrees that the prepared 
** statement p will ever use.  Let N be the number of bits in p->btreeMask
** corresponding to btrees that use shared cache.  Then the runtime of
** this routine is N*N.  But as N is rarely more than 1, this should not
** be a problem.
p-> BtreeMask字段是所有 预先声明的 p 将永远使用的btree 的位掩码，。
设 N 是 p-> btreeMask 对应使用的共享的缓存的 btree 的位数。那么这个
例程的运行时间是 N * N。但当 N 很少超过 1时，这也不是问题。
*/
void sqlite3VdbeEnter(Vdbe *p){
  int i;
  yDbMask mask;
  sqlite3 *db;
  Db *aDb;
  int nDb;
  if( p->lockMask==0 ) return;  /* The common case  一般情况*/
  db = p->db;
  aDb = db->aDb;
  nDb = db->nDb;
  for(i=0, mask=1; i<nDb; i++, mask += mask){
    if( i!=1 && (mask & p->lockMask)!=0 && ALWAYS(aDb[i].pBt!=0) ){
      sqlite3BtreeEnter(aDb[i].pBt);
    }
  }
}
#endif

#if !defined(SQLITE_OMIT_SHARED_CACHE) && SQLITE_THREADSAFE>0
/*
** Unlock all of the btrees previously locked by a call to sqlite3VdbeEnter().
解锁所有以前通过调用 sqlite3VdbeEnter() 来锁定的btree。
*/
void sqlite3VdbeLeave(Vdbe *p){
  int i;
  yDbMask mask;
  sqlite3 *db;
  Db *aDb;
  int nDb;
  if( p->lockMask==0 ) return;  /* The common case */
  db = p->db;
  aDb = db->aDb;
  nDb = db->nDb;
  for(i=0, mask=1; i<nDb; i++, mask += mask){
    if( i!=1 && (mask & p->lockMask)!=0 && ALWAYS(aDb[i].pBt!=0) ){
      sqlite3BtreeLeave(aDb[i].pBt);
    }
  }
}
#endif

#if defined(VDBE_PROFILE) || defined(SQLITE_DEBUG)
/*
** Print a single opcode.  This routine is used for debugging only.
打印单个操作码，这个例子仅仅是用来调试的
*/
void sqlite3VdbePrintOp(FILE *pOut, int pc, Op *pOp){
  char *zP4;
  char zPtr[50];
  static const char *zFormat1 = "%4d %-13s %4d %4d %4d %-4s %.2X %s\n";
  if( pOut==0 ) pOut = stdout;
  zP4 = displayP4(pOp, zPtr, sizeof(zPtr));
  fprintf(pOut, zFormat1, pc, 
      sqlite3OpcodeName(pOp->opcode), pOp->p1, pOp->p2, pOp->p3, zP4, pOp->p5,
#ifdef SQLITE_DEBUG
      pOp->zComment ? pOp->zComment : ""
#else
      ""
#endif
  );
  fflush(pOut);
}
#endif

/*
** Release an array of N Mem elements
释放 N Mem 元素的数组
*/
static void releaseMemArray(Mem *p, int N){
  if( p && N ){
    Mem *pEnd;
    sqlite3 *db = p->db;
    u8 malloc_failed = db->mallocFailed;
    if( db->pnBytesFreed ){
      for(pEnd=&p[N]; p<pEnd; p++){
        sqlite3DbFree(db, p->zMalloc);
      }
      return;
    }
    for(pEnd=&p[N]; p<pEnd; p++){
      assert( (&p[1])==pEnd || p[0].db==p[1].db );

      /* This block is really an inlined version of sqlite3VdbeMemRelease()
      ** that takes advantage of the fact that the memory cell value is 
      ** being set to NULL after releasing any dynamic resources.
	  这一块确实是一个sqlite3VdbeMemRelease()的内联版本，实际上好处是
　　  在释放任何动态资源后存储单元值被设置为NULL。
      **
      ** The justification for duplicating code is that according to a
      ** callgrind, this causes a certain test case to hit the CPU 4.7 
      ** percent less (x86 linux, gcc version 4.1.2, -O6) than if 
      ** sqlite3MemRelease() were called from here. With -O2, this jumps
      ** to 6.6 percent. The test case is inserting 1000 rows into a table 
      ** with no indexes using a single prepared INSERT statement, bind() 
      ** and reset(). Inserts are grouped into a transaction.
	  复制代码的理由是，根据callgrind，这将导致某些测试用例击中 CPU 4.7
      的百分点 (86 linux，gcc 版本 4.1.2，x-O6）少于如果 sqlite3MemRelease() 
	  从这里被调用。使用-O2，跳转至 6.6%。测试用例使用单一的准备好的 INSERT 语句，bind() 和 reset ()
	  插入1000行到没有使用索引的表中。插入被分组到一个事务。
      */
      if( p->flags&(MEM_Agg|MEM_Dyn|MEM_Frame|MEM_RowSet) ){
        sqlite3VdbeMemRelease(p);
      }else if( p->zMalloc ){
        sqlite3DbFree(db, p->zMalloc);
        p->zMalloc = 0;
      }

      p->flags = MEM_Invalid;
    }
    db->mallocFailed = malloc_failed;
  }
}

/*
** Delete a VdbeFrame object and its contents. VdbeFrame objects are
** allocated by the OP_Program opcode in sqlite3VdbeExec().
删除vdbe结构体及其内容，vdbe结构体被OP_Program操作码放在sqlite3VdbeExec()函数中。
*/
void sqlite3VdbeFrameDelete(VdbeFrame *p){//删除vdbe结构体
  int i;
  Mem *aMem = VdbeFrameMem(p);
  VdbeCursor **apCsr = (VdbeCursor **)&aMem[p->nChildMem];
  for(i=0; i<p->nChildCsr; i++){
    sqlite3VdbeFreeCursor(p->v, apCsr[i]);
  }
  releaseMemArray(aMem, p->nChildMem);
  sqlite3DbFree(p->v->db, p);
}

#ifndef SQLITE_OMIT_EXPLAIN //定义一个SQLITE_OMIT_EXPLAIN
/*
** Give a listing of the program in the virtual machine.
**
** The interface is the same as sqlite3VdbeExec().  But instead of
** running the code, it invokes the callback once for each instruction.
** This feature is used to implement "EXPLAIN".
**
** When p->explain==1, each instruction is listed.  When
** p->explain==2, only OP_Explain instructions are listed and these
** are shown in a different format.  p->explain==2 is used to implement
** EXPLAIN QUERY PLAN.
**
** When p->explain==1, first the main program is listed, then each of
** the trigger subprograms are listed one by one.

**在虚拟机中给出程序的清单。接口和sqlite3VdbeExec()的接口一样。但不是运行代码，
而是将每条指令回调一次。这个功能用来实现解释。
当p->explain==1时，每条指令都被列举出来。当p->explain==2时，只有OP_Explain指令被列举
并且使用不同的格式展示。p->explain==2被用来解释查询计划。
当p->explain==1时，首先，主程序被列举出来，然后每一个触发子程序被依次列举出来。
*/
int sqlite3VdbeList(
  Vdbe *p                   /* The VDBE 定义vdbe指针*/
){
  int nRow;                            /* Stop when row count reaches this 设置行数上限*/
  int nSub = 0;                        /* Number of sub-vdbes seen so far 截止目前的子vdbe数量*/
  SubProgram **apSub = 0;              /* Array of sub-vdbes 子vdbe数组*/
  Mem *pSub = 0;                       /* Memory cell hold array of subprogs 存储subprogs数组*/
  sqlite3 *db = p->db;                 /* The database connection 数据库连接*/
  int i;                               /* Loop counter 循环计数器*/
  int rc = SQLITE_OK;                  /* Return code 返回值*/
  Mem *pMem = &p->aMem[1];             /* First Mem of result set 第一个Mem的结果集*/

  assert( p->explain );
  assert( p->magic==VDBE_MAGIC_RUN );
  assert( p->rc==SQLITE_OK || p->rc==SQLITE_BUSY || p->rc==SQLITE_NOMEM );

  /* Even though this opcode does not use dynamic strings for
  ** the result, result columns may become dynamic if the user calls
  ** sqlite3_column_text16(), causing a translation to UTF-16 encoding.
  尽管这个操作码不使用动态字符串表示结果，但是当用户调用了sqlite3_column_text16()函数后
  结果列可能变为动态的，这是因为使用了UTF-16编码。
  */
  releaseMemArray(pMem, 8);
  p->pResultSet = 0;

  if( p->rc==SQLITE_NOMEM ){//当vdbe的返回值是SQLITE_NOMEM
    /* This happens if a malloc() inside a call to sqlite3_column_text() or
    ** sqlite3_column_text16() failed.  
	这种情况发生在如果malloc()调用内部sqlite3_column_text()或sqlite3_column_text16()失败时。
	*/
    db->mallocFailed = 1;
    return SQLITE_ERROR;
  }

  /* When the number of output rows reaches nRow, that means the
  ** listing has finished and sqlite3_step() should return SQLITE_DONE.
  ** nRow is the sum of the number of rows in the main program, plus
  ** the sum of the number of rows in all trigger subprograms encountered
  ** so far.  The nRow value will increase as new trigger subprograms are
  ** encountered, but p->pc will eventually catch up to nRow.
  当输出的行数达到最大值nRow时，意味着列举工作已经完成并且sqlite3_step()应该返回SQLITE_DONE。
  nRow是主程序中的行数的总和加上到目前为止所遇到的触法子程序的数量。当出现新的触发子程序时
  nRow值将增加，但是p->pc最终会赶上nRow
  */
  nRow = p->nOp;
  if( p->explain==1 ){
    /* The first 8 memory cells are used for the result set.  So we will
    ** commandeer the 9th cell to use as storage for an array of pointers
    ** to trigger subprograms.  The VDBE is guaranteed to have at least 9
    ** cells. 
		开始的8个存储单元用于结果集，所以我们将征用第九个单元用来存储触发子程序的指针数组。
		所以vdbe是保证最少有9个存储单元的。
	*/
    assert( p->nMem>9 );
    pSub = &p->aMem[9];
    if( pSub->flags&MEM_Blob ){
      /* On the first call to sqlite3_step(), pSub will hold a NULL.  It is
      ** initialized to a BLOB by the P4_SUBPROGRAM processing logic below 
	  在第一次调用sqlite3_step()时，psub将为空。这是初始化一个BLOB的P4_SUBPROGRAM下面的处理逻辑。
	  */
      nSub = pSub->n/sizeof(Vdbe*);
      apSub = (SubProgram **)pSub->z;
    }
    for(i=0; i<nSub; i++){
      nRow += apSub[i]->nOp;
    }
  }

  do{
    i = p->pc++;
  }while( i<nRow && p->explain==2 && p->aOp[i].opcode!=OP_Explain );
  if( i>=nRow ){
    p->rc = SQLITE_OK;
    rc = SQLITE_DONE;
  }else if( db->u1.isInterrupted ){
    p->rc = SQLITE_INTERRUPT;
    rc = SQLITE_ERROR;
    sqlite3SetString(&p->zErrMsg, db, "%s", sqlite3ErrStr(p->rc));
  }else{
    char *z;
    Op *pOp;
    if( i<p->nOp ){
      /* The output line number is small enough that we are still in the
      ** main program.
		输出的行号足够小以至于我们依然处在主程序中。
	  */
      pOp = &p->aOp[i];
    }else{
      /* We are currently listing subprograms.  Figure out which one and
      ** pick up the appropriate opcode. 
	  我们列出当前的子程序清单，找出是哪一个获得了适当的操作码。
	  */
      int j;
      i -= p->nOp;
      for(j=0; i>=apSub[j]->nOp; j++){
        i -= apSub[j]->nOp;
      }
      pOp = &apSub[j]->aOp[i];
    }
    if( p->explain==1 ){
      pMem->flags = MEM_Int;
      pMem->type = SQLITE_INTEGER;
      pMem->u.i = i;                                /* Program counter 程序计数器*/
      pMem++;
  
      pMem->flags = MEM_Static|MEM_Str|MEM_Term;
      pMem->z = (char*)sqlite3OpcodeName(pOp->opcode);  /* Opcode 操作码*/
      assert( pMem->z!=0 );
      pMem->n = sqlite3Strlen30(pMem->z);
      pMem->type = SQLITE_TEXT;
      pMem->enc = SQLITE_UTF8;
      pMem++;

      /* When an OP_Program opcode is encounter (the only opcode that has
      ** a P4_SUBPROGRAM argument), expand the size of the array of subprograms
      ** kept in p->aMem[9].z to hold the new program - assuming this subprogram
      ** has not already been seen.
	  当遇到一个OP_Program操作码（唯一的操作吗，具有一个P4_SUBPROGRAM参数），扩展
	  子程序数组的大小在p->aMem[9].z来存储新程序--假设这个子程序从未出现过。
      */
      if( pOp->p4type==P4_SUBPROGRAM ){
        int nByte = (nSub+1)*sizeof(SubProgram*);
        int j;
        for(j=0; j<nSub; j++){
          if( apSub[j]==pOp->p4.pProgram ) break;
        }
        if( j==nSub && SQLITE_OK==sqlite3VdbeMemGrow(pSub, nByte, nSub!=0) ){
          apSub = (SubProgram **)pSub->z;
          apSub[nSub++] = pOp->p4.pProgram;
          pSub->flags |= MEM_Blob;
          pSub->n = nSub*sizeof(SubProgram*);
        }
      }
    }

    pMem->flags = MEM_Int;
    pMem->u.i = pOp->p1;                          /* P1 */
    pMem->type = SQLITE_INTEGER;
    pMem++;

    pMem->flags = MEM_Int;
    pMem->u.i = pOp->p2;                          /* P2 */
    pMem->type = SQLITE_INTEGER;
    pMem++;

    pMem->flags = MEM_Int;
    pMem->u.i = pOp->p3;                          /* P3 */
    pMem->type = SQLITE_INTEGER;
    pMem++;

    if( sqlite3VdbeMemGrow(pMem, 32, 0) ){            /* P4 */
      assert( p->db->mallocFailed );
      return SQLITE_ERROR;
    }
    pMem->flags = MEM_Dyn|MEM_Str|MEM_Term;
    z = displayP4(pOp, pMem->z, 32);
    if( z!=pMem->z ){
      sqlite3VdbeMemSetStr(pMem, z, -1, SQLITE_UTF8, 0);
    }else{
      assert( pMem->z!=0 );
      pMem->n = sqlite3Strlen30(pMem->z);
      pMem->enc = SQLITE_UTF8;
    }
    pMem->type = SQLITE_TEXT;
    pMem++;

    if( p->explain==1 ){
      if( sqlite3VdbeMemGrow(pMem, 4, 0) ){
        assert( p->db->mallocFailed );
        return SQLITE_ERROR;
      }
      pMem->flags = MEM_Dyn|MEM_Str|MEM_Term;
      pMem->n = 2;
      sqlite3_snprintf(3, pMem->z, "%.2x", pOp->p5);   /* P5 */
      pMem->type = SQLITE_TEXT;
      pMem->enc = SQLITE_UTF8;
      pMem++;
  
#ifdef SQLITE_DEBUG
      if( pOp->zComment ){
        pMem->flags = MEM_Str|MEM_Term;
        pMem->z = pOp->zComment;
        pMem->n = sqlite3Strlen30(pMem->z);
        pMem->enc = SQLITE_UTF8;
        pMem->type = SQLITE_TEXT;
      }else
#endif
      {
        pMem->flags = MEM_Null;                       /* Comment */
        pMem->type = SQLITE_NULL;
      }
    }

    p->nResColumn = 8 - 4*(p->explain-1);
    p->pResultSet = &p->aMem[1];
    p->rc = SQLITE_OK;
    rc = SQLITE_ROW;
  }
  return rc;
}
#endif /* SQLITE_OMIT_EXPLAIN */

#ifdef SQLITE_DEBUG
/*
** Print the SQL that was used to generate a VDBE program.
打印用来生成vdbe程序的SQL
*/
void sqlite3VdbePrintSql(Vdbe *p){
  int nOp = p->nOp;
  VdbeOp *pOp;
  if( nOp<1 ) return;
  pOp = &p->aOp[0];
  if( pOp->opcode==OP_Trace && pOp->p4.z!=0 ){
    const char *z = pOp->p4.z;
    while( sqlite3Isspace(*z) ) z++;
    printf("SQL: [%s]\n", z);
  }
}
#endif

#if !defined(SQLITE_OMIT_TRACE) && defined(SQLITE_ENABLE_IOTRACE)
/*
** Print an IOTRACE message showing SQL content.
打印一个IOTRACE消息显示SQL内容
*/
void sqlite3VdbeIOTraceSql(Vdbe *p){
  int nOp = p->nOp;
  VdbeOp *pOp;
  if( sqlite3IoTrace==0 ) return;
  if( nOp<1 ) return;
  pOp = &p->aOp[0];
  if( pOp->opcode==OP_Trace && pOp->p4.z!=0 ){
    int i, j;
    char z[1000];
    sqlite3_snprintf(sizeof(z), z, "%s", pOp->p4.z);
    for(i=0; sqlite3Isspace(z[i]); i++){}
    for(j=0; z[i]; i++){
      if( sqlite3Isspace(z[i]) ){
        if( z[i-1]!=' ' ){
          z[j++] = ' ';
        }
      }else{
        z[j++] = z[i];
      }
    }
    z[j] = 0;
    sqlite3IoTrace("SQL %s\n", z);
  }
}
#endif /* !SQLITE_OMIT_TRACE && SQLITE_ENABLE_IOTRACE */

/*
** Allocate space from a fixed size buffer and return a pointer to
** that space.  If insufficient space is available, return NULL.
**从一个固定大小的缓冲区分配空间并返回一个指向该空间的指针。如果可用空间不足，返回空。
** The pBuf parameter is the initial value of a pointer which will
** receive the new memory.  pBuf is normally NULL.  If pBuf is not
** NULL, it means that memory space has already been allocated and that
** this routine should not allocate any new memory.  When pBuf is not
** NULL simply return pBuf.  Only allocate new memory space when pBuf
** is NULL.
**pBuf参数是一个指针的初始值用来接收新的内存，pbuf一般都为空。如果pbuf不为空，意味着
存储空间已经被指派了，并且这个程序不应该分配新的内存。当pbuf不为空直接返回pbuf，只有pbuf
为空的时候才分配存储空间。
** nByte is the number of bytes of space needed.
**nByte是所需的空间的字节数。
** *ppFrom points to available space and pEnd points to the end of the
** available space.  When space is allocated, *ppFrom is advanced past
** the end of the allocated space.
** *ppFrom指向可用的空间，pEnd指向可用空间的末尾。当空间被分配好后，*ppFrom
先进的越过该分配空间的结尾。
** *pnByte is a counter of the number of bytes of space that have failed
** to allocate.  If there is insufficient space in *ppFrom to satisfy the
** request, then increment *pnByte by the amount of the request.
	*pnByte是分配失败的空间的字节数的计数器。如果*ppFrom没有足够的空间来满足请求，
	那么增加*pnByte请求的数量。
*/
static void *allocSpace(
  void *pBuf,          /* Where return pointer will be stored ；存储返回指针*/
  int nByte,           /* Number of bytes to allocate ；分配的字节数*/
  u8 **ppFrom,         /* IN/OUT: Allocate from *ppFrom ；从* ppFrom分配*/
  u8 *pEnd,            /* Pointer to 1 byte past the end of *ppFrom buffer ；1字节指针越过* ppFrom缓冲区的末尾*/
  int *pnByte          /* If allocation cannot be made, increment *pnByte ；如果不能分配，增加*pnByte*/
){
  assert( EIGHT_BYTE_ALIGNMENT(*ppFrom) );
  if( pBuf ) return pBuf;
  nByte = ROUND8(nByte);
  if( &(*ppFrom)[nByte] <= pEnd ){
    pBuf = (void*)*ppFrom;
    *ppFrom += nByte;
  }else{
    *pnByte += nByte;
  }
  return pBuf;
}

/*
** Rewind the VDBE back to the beginning in preparation for
** running it.
将VDBE倒回为VDBE准备运行时的状态
*/
void sqlite3VdbeRewind(Vdbe *p){
#if defined(SQLITE_DEBUG) || defined(VDBE_PROFILE)
  int i;
#endif
  assert( p!=0 );
  assert( p->magic==VDBE_MAGIC_INIT );

  /* There should be at least one opcode.
  应该至少有一个操作码
  */
  assert( p->nOp>0 );

  /* Set the magic to VDBE_MAGIC_RUN sooner rather than later. 
  尽早设置VDBE_MAGIC_RUN的magic（魔法？）*/
  p->magic = VDBE_MAGIC_RUN;

#ifdef SQLITE_DEBUG
  for(i=1; i<p->nMem; i++){
    assert( p->aMem[i].db==p->db );
  }
#endif
  p->pc = -1;
  p->rc = SQLITE_OK;
  p->errorAction = OE_Abort;
  p->magic = VDBE_MAGIC_RUN;
  p->nChange = 0;
  p->cacheCtr = 1;
  p->minWriteFileFormat = 255;
  p->iStatement = 0;
  p->nFkConstraint = 0;
#ifdef VDBE_PROFILE
  for(i=0; i<p->nOp; i++){
    p->aOp[i].cnt = 0;
    p->aOp[i].cycles = 0;
  }
#endif
}

/*
** Prepare a virtual machine for execution for the first time after
** creating the virtual machine.  This involves things such
** as allocating stack space and initializing the program counter.
** After the VDBE has be prepped, it can be executed by one or more
** calls to sqlite3VdbeExec().  
**在创建虚拟机后，为第一次执行准备一个虚拟机。涉及到例如分配栈空间和初始化程序计数器。
在VDBE准备好后，可以被一个或多个sqlite3VdbeExec()函数调用执行。
** This function may be called exact once on a each virtual machine.
** After this routine is called the VM has been "packaged" and is ready
** to run.  After this routine is called, futher calls to 
** sqlite3VdbeAddOp() functions are prohibited.  This routine disconnects
** the Vdbe from the Parse object that helped generate it so that the
** the Vdbe becomes an independent entity and the Parse object can be
** destroyed.
**这个函数可能被精确调用一次在每一个虚拟机上。在程序被调用之后，VM被“打包”，准备好用来运行。
程序被调用后，禁止进一步调用sqlite3VdbeAddOp()功能。改程序将VDBE从解析对象断开来帮助生成它
以便VDBE成为一个独立的实体并且解析对象能够被销毁。
** Use the sqlite3VdbeRewind() procedure to restore a virtual machine back
** to its initial state after it has been run.
使用sqlite3VdbeRewind()程序来将虚拟机恢复到初始状态。
*/
void sqlite3VdbeMakeReady(
  Vdbe *p,                       /* The VDBE */
  Parse *pParse                  /* Parsing context 解析内容*/
){
  sqlite3 *db;                   /* The database connection 数据库连接*/
  int nVar;                      /* Number of parameters 参数个数*/
  int nMem;                      /* Number of VM memory registers 虚拟机记忆指针存储器个数 */
  int nCursor;                   /* Number of cursors required 所需的游标数*/
  int nArg;                      /* Number of arguments in subprograms 子程序的参数个数 */
  int nOnce;                     /* Number of OP_Once instructions ；OP_Once指令的个数*/
  int n;                         /* Loop counter 循环计数器*/
  u8 *zCsr;                      /* Memory available for allocation 可用内存的分配*/
  u8 *zEnd;                      /* First byte past allocated memory 第一个字节分配的内存*/
  int nByte;                     /* How much extra memory is needed 需要的额外内存*/

  assert( p!=0 );
  assert( p->nOp>0 );
  assert( pParse!=0 );
  assert( p->magic==VDBE_MAGIC_INIT );
  db = p->db;
  assert( db->mallocFailed==0 );
  nVar = pParse->nVar;
  nMem = pParse->nMem;
  nCursor = pParse->nTab;
  nArg = pParse->nMaxArg;
  nOnce = pParse->nOnce;
  if( nOnce==0 ) nOnce = 1; /* 保证 p->aOnceFlag[]中至少有一个字节 */
  
  /* For each cursor required, also allocate a memory cell. Memory
  ** cells (nMem+1-nCursor)..nMem, inclusive, will never be used by
  ** the vdbe program. Instead they are used to allocate space for
  ** VdbeCursor/BtCursor structures. The blob of memory associated with 
  ** cursor 0 is stored in memory cell nMem. Memory cell (nMem-1)
  ** stores the blob of memory associated with cursor 1, etc.
  **对每一个需要的游标也分配一个存储单元，存储单元(nMem+1-nCursor)..nMem不会被
  vdbe程序使用，他们被用来为VdbeCursor/BtCursor结构分配空间。blob的存储与游标0
  联系在一起存储在存储在存储单元nMem中。存储单元（nMem-1）用来存储与blob存储有关的
  游标1，等等。
  ** See also: allocateCursor().
  参见: allocateCursor()
  */
  nMem += nCursor;

  /* Allocate space for memory registers, SQL variables, VDBE cursors and 
  ** an array to marshal SQL function arguments in.
  为记忆指针存储器，SQL变量，VDBE游标和一个数组分配空间来整理SQL函数自变量。
  */
  zCsr = (u8*)&p->aOp[p->nOp];       /* Memory avaliable for allocation 分配可用内存*/
  zEnd = (u8*)&p->aOp[p->nOpAlloc];  /* First byte past end of zCsr[] 通过zCsr[]的第一个字节*/

  resolveP2Values(p, &nArg);
  p->usesStmtJournal = (u8)(pParse->isMultiWrite && pParse->mayAbort);
  if( pParse->explain && nMem<10 ){
    nMem = 10;
  }
  memset(zCsr, 0, zEnd-zCsr);
  zCsr += (zCsr - (u8*)0)&7;
  assert( EIGHT_BYTE_ALIGNMENT(zCsr) );
  p->expired = 0;

  /* Memory for registers, parameters, cursor, etc, is allocated in two
  ** passes.  On the first pass, we try to reuse unused space at the 
  ** end of the opcode array.  If we are unable to satisfy all memory
  ** requirements by reusing the opcode array tail, then the second
  ** pass will fill in the rest using a fresh allocation.  
  ** 内存寄存器，参数寄存器，指针寄存器等使用两种途径分配。第一种途径，
	 我们试图重用在操作码数组结束时的闲置空间。如果我们无法通过重用
	 操作码数组尾部满足所有的存储要求，那么第二种途径将使用新的分配来
	 填满余下的部分。
  ** This two-pass approach that reuses as much memory as possible from
  ** the leftover space at the end of the opcode array can significantly
  ** reduce the amount of memory held by a prepared statement.
	 这两种途径可以重用操作码数组尾部剩余空间的的尽可能多的内存，能够
	 显著地减少已就绪的声明所占用的内存数。
  */
  do {
    nByte = 0;
    p->aMem = allocSpace(p->aMem, nMem*sizeof(Mem), &zCsr, zEnd, &nByte);
    p->aVar = allocSpace(p->aVar, nVar*sizeof(Mem), &zCsr, zEnd, &nByte);
    p->apArg = allocSpace(p->apArg, nArg*sizeof(Mem*), &zCsr, zEnd, &nByte);
    p->azVar = allocSpace(p->azVar, nVar*sizeof(char*), &zCsr, zEnd, &nByte);
    p->apCsr = allocSpace(p->apCsr, nCursor*sizeof(VdbeCursor*),
                          &zCsr, zEnd, &nByte);
    p->aOnceFlag = allocSpace(p->aOnceFlag, nOnce, &zCsr, zEnd, &nByte);
    if( nByte ){
      p->pFree = sqlite3DbMallocZero(db, nByte);
    }
    zCsr = p->pFree;
    zEnd = &zCsr[nByte];
  }while( nByte && !db->mallocFailed );

  p->nCursor = (u16)nCursor;
  p->nOnceFlag = nOnce;
  if( p->aVar ){
    p->nVar = (ynVar)nVar;
    for(n=0; n<nVar; n++){
      p->aVar[n].flags = MEM_Null;
      p->aVar[n].db = db;
    }
  }
  if( p->azVar ){
    p->nzVar = pParse->nzVar;
    memcpy(p->azVar, pParse->azVar, p->nzVar*sizeof(p->azVar[0]));
    memset(pParse->azVar, 0, pParse->nzVar*sizeof(pParse->azVar[0]));
  }
  if( p->aMem ){
    p->aMem--;                      /* aMem[] 取值从 1..nMem */
    p->nMem = nMem;                 /*       不是从 0..nMem-1 */
    for(n=1; n<=nMem; n++){
      p->aMem[n].flags = MEM_Invalid;
      p->aMem[n].db = db;
    }
  }
  p->explain = pParse->explain;
  sqlite3VdbeRewind(p);
}

/*
** Close a VDBE cursor and release all the resources that cursor 
** happens to hold.
	关闭一个VDBE游标并且释放该游标恰好占用的所有资源。
*/
void sqlite3VdbeFreeCursor(Vdbe *p, VdbeCursor *pCx){
  if( pCx==0 ){
    return;
  }
  sqlite3VdbeSorterClose(p->db, pCx);
  if( pCx->pBt ){
    sqlite3BtreeClose(pCx->pBt);
    /* The pCx->pCursor will be close automatically, if it exists, by
    ** the call above.
		pCx->pCursor将自动关闭，如果存在，通过上面的调用来关闭。
	*/
  }else if( pCx->pCursor ){
    sqlite3BtreeCloseCursor(pCx->pCursor);
  }
#ifndef SQLITE_OMIT_VIRTUALTABLE
  if( pCx->pVtabCursor ){
    sqlite3_vtab_cursor *pVtabCursor = pCx->pVtabCursor;
    const sqlite3_module *pModule = pCx->pModule;
    p->inVtabMethod = 1;
    pModule->xClose(pVtabCursor);
    p->inVtabMethod = 0;
  }
#endif
}

/*
** Copy the values stored in the VdbeFrame structure to its Vdbe. This
** is used, for example, when a trigger sub-program is halted to restore
** control to the main program.
   将vdbe结构中的值复制到它的vdbe中。例如当一个触发子程序被停止来恢复控制主程序。
*/
int sqlite3VdbeFrameRestore(VdbeFrame *pFrame){
  Vdbe *v = pFrame->v;
  v->aOnceFlag = pFrame->aOnceFlag;
  v->nOnceFlag = pFrame->nOnceFlag;
  v->aOp = pFrame->aOp;
  v->nOp = pFrame->nOp;
  v->aMem = pFrame->aMem;
  v->nMem = pFrame->nMem;
  v->apCsr = pFrame->apCsr;
  v->nCursor = pFrame->nCursor;
  v->db->lastRowid = pFrame->lastRowid;
  v->nChange = pFrame->nChange;
  return pFrame->pc;
}

/*
** Close all cursors.
	关闭所有游标
**
** Also release any dynamic memory held by the VM in the Vdbe.aMem memory 
** cell array. This is necessary as the memory cell array may contain
** pointers to VdbeFrame objects, which may in turn contain pointers to
** open cursors.
	释放vdbe中任何被虚拟机占用的动态内存。这在存储单元数组可能包含了vdbe对象的
	指针，该对象可能包含了依次打开游标的指针时是很必要的，
*/
static void closeAllCursors(Vdbe *p){
  if( p->pFrame ){
    VdbeFrame *pFrame;
    for(pFrame=p->pFrame; pFrame->pParent; pFrame=pFrame->pParent);
    sqlite3VdbeFrameRestore(pFrame);
  }
  p->pFrame = 0;
  p->nFrame = 0;

  if( p->apCsr ){
    int i;
    for(i=0; i<p->nCursor; i++){
      VdbeCursor *pC = p->apCsr[i];
      if( pC ){
        sqlite3VdbeFreeCursor(p, pC);
        p->apCsr[i] = 0;
      }
    }
  }
  if( p->aMem ){
    releaseMemArray(&p->aMem[1], p->nMem);
  }
  while( p->pDelFrame ){
    VdbeFrame *pDel = p->pDelFrame;
    p->pDelFrame = pDel->pParent;
    sqlite3VdbeFrameDelete(pDel);
  }
}

/*
** Clean up the VM after execution.
**在执行后清理虚拟机
** This routine will automatically close any cursors, lists, and/or
** sorters that were left open.  It also deletes the values of
** variables in the aVar[] array.
	该程序能够自动关闭任何被打开的游标，列表，和/或分类器。同时，改程序
	能够删除aVar[]数组里变量的值。
*/
static void Cleanup(Vdbe *p){
  sqlite3 *db = p->db;

#ifdef SQLITE_DEBUG
  /* Execute assert() statements to ensure that the Vdbe.apCsr[] and 
  ** Vdbe.aMem[] arrays have already been cleaned up.  
	 执行assert()声明来保证vdbe.apCsr[]和vdbe.aMem[]数组已被清空。
  */
  int i;
  if( p->apCsr ) for(i=0; i<p->nCursor; i++) assert( p->apCsr[i]==0 );
  if( p->aMem ){
    for(i=1; i<=p->nMem; i++) assert( p->aMem[i].flags==MEM_Invalid );
  }
#endif

  sqlite3DbFree(db, p->zErrMsg);
  p->zErrMsg = 0;
  p->pResultSet = 0;
}

/*
** Set the number of result columns that will be returned by this SQL
** statement. This is now set at compile time, rather than during
** execution of the vdbe program so that sqlite3_column_count() can
** be called on an SQL statement before sqlite3_step().
	设置结果列的数目，结果列通过SQL声明返回。要在编译时设置，而不是在
	vdbe的处理过程中设置，如此以便sqlite3_column_count()能够在
	sqlite3_step()之前被一个SQL声明所调用。
*/
void sqlite3VdbeSetNumCols(Vdbe *p, int nResColumn){
  Mem *pColName;
  int n;
  sqlite3 *db = p->db;

  releaseMemArray(p->aColName, p->nResColumn*COLNAME_N);
  sqlite3DbFree(db, p->aColName);
  n = nResColumn*COLNAME_N;
  p->nResColumn = (u16)nResColumn;
  p->aColName = pColName = (Mem*)sqlite3DbMallocZero(db, sizeof(Mem)*n );
  if( p->aColName==0 ) return;
  while( n-- > 0 ){
    pColName->flags = MEM_Null;
    pColName->db = p->db;
    pColName++;
  }
}

/*
** Set the name of the idx'th column to be returned by the SQL statement.
** zName must be a pointer to a nul terminated string.
**设置通过SQL声明返回的第idx列的名称。zName必须是一个空字符结尾的字符串的指针。
** This call must be made after a call to sqlite3VdbeSetNumCols().
**该调用必须要在sqlite3VdbeSetNumCols()之后调用。
** The final parameter, xDel, must be one of SQLITE_DYNAMIC, SQLITE_STATIC
** or SQLITE_TRANSIENT. If it is SQLITE_DYNAMIC, then the buffer pointed
** to by zName will be freed by sqlite3DbFree() when the vdbe is destroyed.
	最后的参数，xDel，必须是SQLITE_DYNAMIC, SQLITE_STATIC或 SQLITE_TRANSIENT中的一个。
	如果是SQLITE_DYNAMIC，那么当vdbe被销毁的时候，由zName指向的缓冲区将被sqlite3DbFree()释放。
*/
int sqlite3VdbeSetColName(
  Vdbe *p,                         /* Vdbe being configured ；vdbe配置*/
  int idx,                         /* Index of column zName applies to ；列zName的索引*/
  int var,                         /* One of the COLNAME_* constants ；COLNAME_*常量中的一个*/
  const char *zName,               /* Pointer to buffer containing name；名称缓冲区指针 */
  void (*xDel)(void*)              /* Memory management strategy for zName ；zName内存管理策略*/
){
  int rc;
  Mem *pColName;
  assert( idx<p->nResColumn );
  assert( var<COLNAME_N );
  if( p->db->mallocFailed ){
    assert( !zName || xDel!=SQLITE_DYNAMIC );
    return SQLITE_NOMEM;
  }
  assert( p->aColName!=0 );
  pColName = &(p->aColName[idx+var*p->nResColumn]);
  rc = sqlite3VdbeMemSetStr(pColName, zName, -1, SQLITE_UTF8, xDel);
  assert( rc!=0 || !zName || (pColName->flags&MEM_Term)!=0 );
  return rc;
}

/*
** A read or write transaction may or may not be active on database handle
** db. If a transaction is active, commit it. If there is a
** write-transaction spanning more than one database file, this routine
** takes care of the master journal trickery.
	一个读或写事务可能或者不可能是活跃的在数据库句柄db上。如果一个事务是活跃的，
	就把它提交。如果存在一个写事务跨越多个数据库文件，那么改程序关心主要的欺骗日志。
*/
static int vdbeCommit(sqlite3 *db, Vdbe *p){
  int i;
  int nTrans = 0;  /* Number of databases with an active write-transaction ；活跃的写事务的数据库数量*/
  int rc = SQLITE_OK;
  int needXcommit = 0;

#ifdef SQLITE_OMIT_VIRTUALTABLE
  /* With this option, sqlite3VtabSync() is defined to be simply 
  ** SQLITE_OK so p is not used. 
	 通过该选项，sqlite3VtabSync()将被简单地定义为SQLITE_OK，所以p将不会被使用。
  */
  UNUSED_PARAMETER(p);
#endif

  /* Before doing anything else, call the xSync() callback for any
  ** virtual module tables written in this transaction. This has to
  ** be done before determining whether a master journal file is 
  ** required, as an xSync() callback may add an attached database
  ** to the transaction.
	 在做其他事情之前，为所任何被写入该事务的虚拟模块表回调xSnc()函数。
	 这是在决定一个朱日志文件是否是必需的之前不得不做的，就像一个xSync()
	 回调可能会对事务添加一个附件的数据库。
  */
  rc = sqlite3VtabSync(db, &p->zErrMsg);

  /* This loop determines (a) if the commit hook should be invoked and
  ** (b) how many database files have open write transactions, not 
  ** including the temp database. (b) is important because if more than 
  ** one database file has an open write transaction, a master journal
  ** file is required for an atomic commit.
	 这个循环决定了提交钩是否应该被调用以及有多少数据库文件被打开来写入事务，
	 不包括临时数据库。（a）：提交钩是否应该被调用;（b）：有多少数据库文件被打开来写入事务。
	 （b）非常重要，因为如果不止一个数据库文件被打开来写入事务，一个原子提交需要一个主要的日志文件。
  */ 
  for(i=0; rc==SQLITE_OK && i<db->nDb; i++){ 
    Btree *pBt = db->aDb[i].pBt;
    if( sqlite3BtreeIsInTrans(pBt) ){
      needXcommit = 1;
      if( i!=1 ) nTrans++;
      rc = sqlite3PagerExclusiveLock(sqlite3BtreePager(pBt));
    }
  }
  if( rc!=SQLITE_OK ){
    return rc;
  }

  /* If there are any write-transactions at all, invoke the commit hook */
  /* 如果有任何写事务，调用提交钩*/
  if( needXcommit && db->xCommitCallback ){
    rc = db->xCommitCallback(db->pCommitArg);
    if( rc ){
      return SQLITE_CONSTRAINT;
    }
  }

  /* The simple case - no more than one database file (not counting the
  ** TEMP database) has a transaction active.   There is no need for the
  ** master-journal.
  ** 简单的例子，至多一个数据库文件（不计算临时数据库）具有事务处理交易（操作）。
	 不需要主日志。
  ** If the return value of sqlite3BtreeGetFilename() is a zero length
  ** string, it means the main database is :memory: or a temp file.  In 
  ** that case we do not support atomic multi-file commits, so use the 
  ** simple case then too.
	 如果sqlite3BtreeGetFilename()的返回值是一个长度为0的字符串，这就意味着
	 主数据库是内存或者一个临时文件。在那种情况下，我们不支持原子多文件提交，
	 所以后面也使用这个简单的例子。
  */
  if( 0==sqlite3Strlen30(sqlite3BtreeGetFilename(db->aDb[0].pBt))
   || nTrans<=1
  ){
    for(i=0; rc==SQLITE_OK && i<db->nDb; i++){
      Btree *pBt = db->aDb[i].pBt;
      if( pBt ){
        rc = sqlite3BtreeCommitPhaseOne(pBt, 0);
      }
    }

    /* Do the commit only if all databases successfully complete phase 1. 
    ** If one of the BtreeCommitPhaseOne() calls fails, this indicates an
    ** IO error while deleting or truncating a journal file. It is unlikely,
    ** but could happen. In this case abandon processing and return the error.
		只有当所有数据库成功完成阶段1后才执行提交操作。如果BtreeCommitPhaseOne()
		的一个调用失败，表明在删除或者截断一个日志文件的时候出现了IO错误。
		这种情况虽然不太可能，但也有可能发生。在这种情况下，放弃处理并返回错误。
    */
    for(i=0; rc==SQLITE_OK && i<db->nDb; i++){
      Btree *pBt = db->aDb[i].pBt;
      if( pBt ){
        rc = sqlite3BtreeCommitPhaseTwo(pBt, 0);
      }
    }
    if( rc==SQLITE_OK ){
      sqlite3VtabCommit(db);
    }
  }

  /* The complex case - There is a multi-file write-transaction active.
  ** This requires a master journal file to ensure the transaction is
  ** committed atomicly.
	复杂的情况：存在一个多文件写事务交易。这需要一个主日志文件来保证
	该事务被正确提交。
  */
#ifndef SQLITE_OMIT_DISKIO
  else{
    sqlite3_vfs *pVfs = db->pVfs;
    int needSync = 0;
    char *zMaster = 0;   /* File-name for the master journal ；主日志文件的文件名*/
    char const *zMainFile = sqlite3BtreeGetFilename(db->aDb[0].pBt);
    sqlite3_file *pMaster = 0;
    i64 offset = 0;
    int res;
    int retryCount = 0;
    int nMainFile;

    /* Select a master journal file name； 选择一个主日志文件名*/
    nMainFile = sqlite3Strlen30(zMainFile);
    zMaster = sqlite3MPrintf(db, "%s-mjXXXXXX9XXz", zMainFile);
    if( zMaster==0 ) return SQLITE_NOMEM;
    do {
      u32 iRandom;
      if( retryCount ){
        if( retryCount>100 ){
          sqlite3_log(SQLITE_FULL, "MJ delete: %s", zMaster);
          sqlite3OsDelete(pVfs, zMaster, 0);
          break;
        }else if( retryCount==1 ){
          sqlite3_log(SQLITE_FULL, "MJ collide: %s", zMaster);
        }
      }
      retryCount++;
      sqlite3_randomness(sizeof(iRandom), &iRandom);
      sqlite3_snprintf(13, &zMaster[nMainFile], "-mj%06X9%02X",
                               (iRandom>>8)&0xffffff, iRandom&0xff);
      /* The antipenultimate character of the master journal name must
      ** be "9" to avoid name collisions when using 8+3 filenames. 
		 主日志文件名字符必须是9以避免当使用8+3的文件名时产生冲突。
	  */
      assert( zMaster[sqlite3Strlen30(zMaster)-3]=='9' );
      sqlite3FileSuffix3(zMainFile, zMaster);
      rc = sqlite3OsAccess(pVfs, zMaster, SQLITE_ACCESS_EXISTS, &res);
    }while( rc==SQLITE_OK && res );
    if( rc==SQLITE_OK ){
      /* Open the master journal. 打开主日志文件*/
      rc = sqlite3OsOpenMalloc(pVfs, zMaster, &pMaster, 
          SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|
          SQLITE_OPEN_EXCLUSIVE|SQLITE_OPEN_MASTER_JOURNAL, 0
      );
    }
    if( rc!=SQLITE_OK ){
      sqlite3DbFree(db, zMaster);
      return rc;
    }
 
    /* Write the name of each database file in the transaction into the new
    ** master journal file. If an error occurs at this point close
    ** and delete the master journal file. All the individual journal files
    ** still have 'null' as the master journal pointer, so they will roll
    ** back independently if a failure occurs.
		将事务中的每一个数据库文件的名字写入新的日志文件中。如果这时候出现错误，
		关闭并且删除这个日志文件。所有个人的日志文件作为主日志指针的时候依然有“空”，
		所以当出现错误的时候，它们将独立的进行回滚。
    */
    for(i=0; i<db->nDb; i++){
      Btree *pBt = db->aDb[i].pBt;
      if( sqlite3BtreeIsInTrans(pBt) ){
        char const *zFile = sqlite3BtreeGetJournalname(pBt);
        if( zFile==0 ){
          continue;  /* Ignore TEMP and :memory: databases ；忽略临时和内存数据库*/
        }
        assert( zFile[0]!=0 );
        if( !needSync && !sqlite3BtreeSyncDisabled(pBt) ){
          needSync = 1;
        }
        rc = sqlite3OsWrite(pMaster, zFile, sqlite3Strlen30(zFile)+1, offset);
        offset += sqlite3Strlen30(zFile)+1;
        if( rc!=SQLITE_OK ){
          sqlite3OsCloseFree(pMaster);
          sqlite3OsDelete(pVfs, zMaster, 0);
          sqlite3DbFree(db, zMaster);
          return rc;
        }
      }
    }

    /* Sync the master journal file. If the IOCAP_SEQUENTIAL device
    ** flag is set this is not required.
		同步主日志文件。如果IOCAP_SEQUENTIAL表示已经被设置了，那么就不需要这个操作了。
    */
    if( needSync 
     && 0==(sqlite3OsDeviceCharacteristics(pMaster)&SQLITE_IOCAP_SEQUENTIAL)
     && SQLITE_OK!=(rc = sqlite3OsSync(pMaster, SQLITE_SYNC_NORMAL))
    ){
      sqlite3OsCloseFree(pMaster);
      sqlite3OsDelete(pVfs, zMaster, 0);
      sqlite3DbFree(db, zMaster);
      return rc;
    }

    /* Sync all the db files involved in the transaction. The same call
    ** sets the master journal pointer in each individual journal. If
    ** an error occurs here, do not delete the master journal file.
    ** 同步所有包含事务的db文件。同样的调用设置主日志指针在每个单独的日志上。
		如果这里出现了错误，不要删除主日志文件。
    ** If the error occurs during the first call to
    ** sqlite3BtreeCommitPhaseOne(), then there is a chance that the
    ** master journal file will be orphaned. But we cannot delete it,
    ** in case the master journal file name was written into the journal
    ** file before the failure occurred.
		如果第一次调用sqlite3BtreeCommitPhaseOne()时出现错误，则还有一次机会，
		主日志文件将被孤立。我们不能删除它，以防在失败出现前主日志文件名被
		写进了日志文件中。
    */
    for(i=0; rc==SQLITE_OK && i<db->nDb; i++){ 
      Btree *pBt = db->aDb[i].pBt;
      if( pBt ){
        rc = sqlite3BtreeCommitPhaseOne(pBt, zMaster);
      }
    }
    sqlite3OsCloseFree(pMaster);
    assert( rc!=SQLITE_BUSY );
    if( rc!=SQLITE_OK ){
      sqlite3DbFree(db, zMaster);
      return rc;
    }

    /* Delete the master journal file. This commits the transaction. After
    ** doing this the directory is synced again before any individual
    ** transaction files are deleted.
		删除主日志文件。提交了事务之后，在任何单独的事务文件
		被删除之前，目录将被同步。
    */
    rc = sqlite3OsDelete(pVfs, zMaster, 1);
    sqlite3DbFree(db, zMaster);
    zMaster = 0;
    if( rc ){
      return rc;
    }

    /* All files and directories have already been synced, so the following
    ** calls to sqlite3BtreeCommitPhaseTwo() are only closing files and
    ** deleting or truncating journals. If something goes wrong while
    ** this is happening we don't really care. The integrity of the
    ** transaction is already guaranteed, but some stray 'cold' journals
    ** may be lying around. Returning an error code won't help matters.
		所有的文件和目录已经被同步，因此接下来对sqlite3BtreeCommitPhaseTwo()的
		调用就只是关闭文件，删除或截断日志。如果在执行这些操作的是时候出现了问题，
		我们不用关心。事务的完整性已经有保证了，但是一些游离的“冷”日志可能会到处撒谎。
		这时返回错误代码是无济于事的。
    */
    disable_simulated_io_errors();
    sqlite3BeginBenignMalloc();
    for(i=0; i<db->nDb; i++){ 
      Btree *pBt = db->aDb[i].pBt;
      if( pBt ){
        sqlite3BtreeCommitPhaseTwo(pBt, 1);
      }
    }
    sqlite3EndBenignMalloc();
    enable_simulated_io_errors();

    sqlite3VtabCommit(db);
  }
#endif

  return rc;
}

/* 
** This routine checks that the sqlite3.activeVdbeCnt count variable
** matches the number of vdbe's in the list sqlite3.pVdbe that are
** currently active. An assertion fails if the two counts do not match.
** This is an internal self-check only - it is not an essential processing
** step.
** 这个程序检查sqlite3.activeVdbeCnt计数变量匹配sqlite3.pVdbe列表中当前活动的vdbe数量。
	如果两项不匹配，那么断言失败。这仅仅是一个内部自检而不是一个基本的处理阶段。
** This is a no-op if NDEBUG is defined.
	如果NDEBUG被定义了，这就是一个no-op操作。
*/
#ifndef NDEBUG
static void checkActiveVdbeCnt(sqlite3 *db){
  Vdbe *p;
  int cnt = 0;
  int nWrite = 0;
  p = db->pVdbe;
  while( p ){
    if( p->magic==VDBE_MAGIC_RUN && p->pc>=0 ){
      cnt++;
      if( p->readOnly==0 ) nWrite++;
    }
    p = p->pNext;
  }
  assert( cnt==db->activeVdbeCnt );
  assert( nWrite==db->writeVdbeCnt );
}
#else
#define checkActiveVdbeCnt(x)
#endif

/*
** If the Vdbe passed as the first argument opened a statement-transaction,
** close it now. Argument eOp must be either SAVEPOINT_ROLLBACK or
** SAVEPOINT_RELEASE. If it is SAVEPOINT_ROLLBACK, then the statement
** transaction is rolled back. If eOp is SAVEPOINT_RELEASE, then the 
** statement transaction is commtted.
**	如果vdbe作为第一个参数打开了声明事务，那就关闭它。参数eOp必须是
	SAVEPOINT_ROLLBACK或者SAVEPOINT_RELEASE中的一个。如果是SAVEPOINT_ROLLBACK，
	那么接下来声明事务将被回滚。如果是SAVEPOINT_RELEASE，那么声明事务将被提交。
** If an IO error occurs, an SQLITE_IOERR_XXX error code is returned. 
** Otherwise SQLITE_OK.
	如果出现IO错误，就返回SQLITE_IOERR_XXX错误代码。否则返回SQLITE_OK.
*/
int sqlite3VdbeCloseStatement(Vdbe *p, int eOp){
  sqlite3 *const db = p->db;
  int rc = SQLITE_OK;

  /* If p->iStatement is greater than zero, then this Vdbe opened a 
  ** statement transaction that should be closed here. The only exception
  ** is that an IO error may have occured, causing an emergency rollback.
  ** In this case (db->nStatement==0), and there is nothing to do.
	 如果p->iStatement大于零，那么该vdbe打开的声明事务应该被关闭。
	 唯一的例外就是出现的IO错误可能导致一个紧急回滚。在这种情况下
	 （db->nStatement==0），什么都不用做。
  */
  if( db->nStatement && p->iStatement ){
    int i;
    const int iSavepoint = p->iStatement-1;

    assert( eOp==SAVEPOINT_ROLLBACK || eOp==SAVEPOINT_RELEASE);
    assert( db->nStatement>0 );
    assert( p->iStatement==(db->nStatement+db->nSavepoint) );

    for(i=0; i<db->nDb; i++){ 
      int rc2 = SQLITE_OK;
      Btree *pBt = db->aDb[i].pBt;
      if( pBt ){
        if( eOp==SAVEPOINT_ROLLBACK ){
          rc2 = sqlite3BtreeSavepoint(pBt, SAVEPOINT_ROLLBACK, iSavepoint);
        }
        if( rc2==SQLITE_OK ){
          rc2 = sqlite3BtreeSavepoint(pBt, SAVEPOINT_RELEASE, iSavepoint);
        }
        if( rc==SQLITE_OK ){
          rc = rc2;
        }
      }
    }
    db->nStatement--;
    p->iStatement = 0;

    if( rc==SQLITE_OK ){
      if( eOp==SAVEPOINT_ROLLBACK ){
        rc = sqlite3VtabSavepoint(db, SAVEPOINT_ROLLBACK, iSavepoint);
      }
      if( rc==SQLITE_OK ){
        rc = sqlite3VtabSavepoint(db, SAVEPOINT_RELEASE, iSavepoint);
      }
    }

    /* If the statement transaction is being rolled back, also restore the 
    ** database handles deferred constraint counter to the value it had when 
    ** the statement transaction was opened. 
		如果声明的事务被回滚，当声明事务被打开时也要恢复数据库处理延迟约束计数器的值。
	*/
    if( eOp==SAVEPOINT_ROLLBACK ){
      db->nDeferredCons = p->nStmtDefCons;
    }
  }
  return rc;
}

/*
** This function is called when a transaction opened by the database 
** handle associated with the VM passed as an argument is about to be 
** committed. If there are outstanding deferred foreign key constraint
** violations, return SQLITE_ERROR. Otherwise, SQLITE_OK.
**	当一个事务被与VM有关的数据库句柄打开的时候，该函数被作为一个参数
	调用进行提交。如果有未处理的延迟的外键约束违规，返回SQLITE_ERROR，
	否则返回SQLITE_OK。
** If there are outstanding FK violations and this function returns 
** SQLITE_ERROR, set the result of the VM to SQLITE_CONSTRAINT and write
** an error message to it. Then return SQLITE_ERROR.
	如果有未处理的FK违规，该函数就返回SQLITE_ERROR，并将VM的结果设置为
	SQLITE_CONSTRAINT，同时写入错误信息，然后返回SQLITE_ERROR。
*/
#ifndef SQLITE_OMIT_FOREIGN_KEY
int sqlite3VdbeCheckFk(Vdbe *p, int deferred){
  sqlite3 *db = p->db;
  if( (deferred && db->nDeferredCons>0) || (!deferred && p->nFkConstraint>0) ){
    p->rc = SQLITE_CONSTRAINT;
    p->errorAction = OE_Abort;
    sqlite3SetString(&p->zErrMsg, db, "foreign key constraint failed");
    return SQLITE_ERROR;
  }
  return SQLITE_OK;
}
#endif


/* *************************************华丽的分割线******************************************** */


/*
这一部分功能用于处理当程序VDBE出现停机的情况，如果VDBE处于开启自动模式时，会自动提交对数据库的改写。
如果需要事物回滚，就会回滚。
**这是唯一的方式能够将一个VM的状态从SQLITE_MAGIC_RUN转为SQLITE_MAGIC_HALT
**因为竞争所的原因造成无法完成数据提交，将返回SQLITE_BUSY，那么将意味着数据库没有关闭需要对前面的代码重复执行一次。
** This routine is called the when a VDBE tries to halt.  If the VDBE
** has made changes and is in autocommit mode, then commit those
** changes.  If a rollback is needed, then do the rollback.

** This routine is the only way to move the state of a VM from
** SQLITE_MAGIC_RUN to SQLITE_MAGIC_HALT.  It is harmless to
** call this on a VM that is in the SQLITE_MAGIC_HALT state.

** Return an error code.  If the commit could not complete because of
** lock contention, return SQLITE_BUSY.  If SQLITE_BUSY is returned, it
** means the close did not happen and needs to be repeated.
*/
int sqlite3VdbeHalt(Vdbe *p){
  int rc;                         /* Used to store transient return codes rc用于存储执行一个事务的返回值*/
  sqlite3 *db = p->db;

  /* 在方法中会包含处理逻辑用于判断一个语句或一个事务被虚拟机处理后返回是提交成功还是事务回滚。
   **如果一下四种任何一种错误发生的话（SQLITE_NOMEM，SQLITE_IOERR，SQLITE_FULL，SQLITE_INTERRUPT），那将会导
  致内部缓存出现不一致的状态，如果没有完整的事务声明，那么我们需要回滚事务。
  This function contains the logic that determines if a statement or
  ** transaction will be committed or rolled back as a result of the
  ** execution of this virtual machine. 
 
  ** If any of the following errors occur:
  **
  **     SQLITE_NOMEM
  **     SQLITE_IOERR
  **     SQLITE_FULL
  **     SQLITE_INTERRUPT
  **
  ** Then the internal cache might have been left in an inconsistent
  ** state.  We need to rollback the statement transaction, if there is
  ** one, or the complete transaction if there is no statement transaction.
  */

  if( p->db->mallocFailed ){//数据库分配内存失败
    p->rc = SQLITE_NOMEM;
  }
  if( p->aOnceFlag ) memset(p->aOnceFlag, 0, p->nOnceFlag);
  closeAllCursors(p);
  if( p->magic!=VDBE_MAGIC_RUN ){
    return SQLITE_OK;
  }
  checkActiveVdbeCnt(db);

  /* No commit or rollback needed if the program never started 
  如果程序没有开始执行，就不需要提交或者回滚
  */
  if( p->pc>=0 ){
    int mrc;   /* Primary error code from p->rc */
    int eStatementOp = 0;
    int isSpecialError;            /* Set to true if a 'special' error */

    /* Lock all btrees used by the statement 
	对所有使用btrees的语句加锁
	*/
    sqlite3VdbeEnter(p);

    /* Check for one of the special errors 
	检查是否存在special errors
	*/
    mrc = p->rc & 0xff;
    assert( p->rc!=SQLITE_IOERR_BLOCKED );  /* This error no longer exists */
	//mrc出现SQLITE_NOMEM，SQLITE_IOERR，SQLITE_INTERRUPT，SQLITE_FULL这些错误的一个或多个时isSpecialError的值为True
    isSpecialError = mrc==SQLITE_NOMEM || mrc==SQLITE_IOERR
                     || mrc==SQLITE_INTERRUPT || mrc==SQLITE_FULL;
    if( isSpecialError ){
      /* 如果发生isSpecialError是True时，则会分到更细的情况来处理。
	  当查询语句是只读的时候而且错误代码是SQLITE_INTERRUPT不需要回滚，因为没有对数据库有写操作。其他的情况会发生对数据库
	  改变需要执行数据库事务回滚来达到数据库的一致性。即为写操作回滚到原来数据库一致的状态。
	  **对于一个简单的读取数据的语句，更重要的是关心一个语句或者事务的回滚操作。如果错误发生在写日志或者一个数据文件做为
	  释放缓存页面的一个操作时，事务的回滚可以保持页的数据一致性。
	  If the query was read-only and the error code is SQLITE_INTERRUPT, 
      ** no rollback is necessary. Otherwise, at least a savepoint 
      ** transaction must be rolled back to restore the database to a 
      ** consistent state.
   
      ** Even if the statement is read-only, it is important to perform
      ** a statement or transaction rollback operation. If the error 
      ** occured while writing to the journal, sub-journal or database
      ** file as part of an effort to free up cache space (see function
      ** pagerStress() in pager.c), the rollback is required to restore 
      ** the pager to a consistent state.
      */
      if( !p->readOnly || mrc!=SQLITE_INTERRUPT ){//只读或者mrc!=SQLITE_INTERRUPT
        if( (mrc==SQLITE_NOMEM || mrc==SQLITE_FULL) && p->usesStmtJournal ){//mrc没有内存空间或者SQLITE_FULL并且p指向usesStmtJournal
          eStatementOp = SAVEPOINT_ROLLBACK;//事务回滚到保存点
        }else{
          /* 
		  We are forced to roll back the active transaction. Before doing
          ** so, abort any other statements this handle currently has active.
          */
          sqlite3RollbackAll(db, SQLITE_ABORT_ROLLBACK);
          sqlite3CloseSavepoints(db);
          db->autoCommit = 1;
        }
      }
    }

    /*检查外键约束是否满足约束条件
	Check for immediate foreign key violations. */
    if( p->rc==SQLITE_OK ){
      sqlite3VdbeCheckFk(p, 0);
    }
  
    /* 如果设置了自动提交，而且这是一个处于活动状态的写操作，我们将提交或者是回滚当前的事务。
	# define sqlite3VtabInSync(db) 0
	这个功能模块在上面所说的special errors的处理时会用到
	If the auto-commit flag is set and this is the only active writer 
    ** VM, then we do either a commit or rollback of the current transaction. 
    **
    ** Note: This block also runs if one of the special errors handled 
    ** above has occurred. 
    */
    if( !sqlite3VtabInSync(db) 
     && db->autoCommit 
     && db->writeVdbeCnt==(p->readOnly==0) 
    ){
      if( p->rc==SQLITE_OK || (p->errorAction==OE_Fail && !isSpecialError) ){
        rc = sqlite3VdbeCheckFk(p, 1);
        if( rc!=SQLITE_OK ){
          if( NEVER(p->readOnly) ){
            sqlite3VdbeLeave(p);
            return SQLITE_ERROR;
          }
          rc = SQLITE_CONSTRAINT;
        }else{ 
          /* 当设置了自动提交事务市，那么数据库虚拟引擎中的程序会成功提交或者返回一个错误，如果没有额外的外键约束的事务。
		  这个时候事务的提交是必须的。
		  The auto-commit flag is true, the vdbe program was successful 
          ** or hit an 'OR FAIL' constraint and there are no deferred foreign
          ** key constraints to hold up the transaction. This means a commit 
          ** is required. */
          rc = vdbeCommit(db, p);
        }
        if( rc==SQLITE_BUSY && p->readOnly ){//rc返回值为SQLITE_BUSY并且对数据库操作为只读执行下面代码，先释放当前的查询语句，
			//并返回SQLITE_BUSY
          sqlite3VdbeLeave(p);
          return SQLITE_BUSY;
        }else if( rc!=SQLITE_OK ){//数据库返回值不等于SQLITE_OK，p->rc赋值为当前rc值，并且执行事务回滚语句
          p->rc = rc;
          sqlite3RollbackAll(db, SQLITE_OK);
        }else{//rc==SQLITE_BUSY && p->readOnly 和rc!=SQLITE_OK这两个判断条件都不满足时将提交事务
          db->nDeferredCons = 0;
          sqlite3CommitInternalChanges(db);
        }
      }else{//( p->rc==SQLITE_OK || (p->errorAction==OE_Fail && !isSpecialError)这个判断条件不满足是回滚当前数据库事务
        sqlite3RollbackAll(db, SQLITE_OK);
      }
      db->nStatement = 0;
    }else if( eStatementOp==0 ){ //!sqlite3VtabInSync(db) && db->autoCommit && db->writeVdbeCnt==(p->readOnly==0)不满足上述判断，满足eStatementOp==0
      if( p->rc==SQLITE_OK || p->errorAction==OE_Fail ){//在p->rc==SQLITE_OK || p->errorAction==OE_Fail
        eStatementOp = SAVEPOINT_RELEASE;
      }else if( p->errorAction==OE_Abort ){
        eStatementOp = SAVEPOINT_ROLLBACK;
      }else{
        sqlite3RollbackAll(db, SQLITE_ABORT_ROLLBACK);
        sqlite3CloseSavepoints(db);
        db->autoCommit = 1;
      }
    }
  
    /* 如果eStatementOp的值不等于0，那么对于一个声明事务要么提交，要么回滚事务，调用函数sqlite3VdbeCloseStatement(p,eOp)
	来完成上述描述功能当p->iStatement 的值大于0会关闭当前的声明事务.其中形参eOp必须是SAVEPOINT_ROLLBACK或者
	SAVEPOINT_RELEASE，不可以取其他的值。如果eOp取值为SAVEPOINT_ROLLBACK，当前的事务会回滚；如果eOp取值为SAVEPOINT_RELEASE
	会提交当前的事务。如果执行的过程中出现了IO日常错误，形如SQLITE_IOERR_XXX的错误代码会返回给程序的调用，否则程序返回True
	If eStatementOp is non-zero, then a statement transaction needs to
    ** be committed or rolled back. Call sqlite3VdbeCloseStatement() to
    ** do so. If this operation returns an error, and the current statement
    ** error code is SQLITE_OK or SQLITE_CONSTRAINT, then promote the
    ** current statement error code.
    */
    if( eStatementOp ){
      rc = sqlite3VdbeCloseStatement(p, eStatementOp);//局部变量rc接受调用sqlite3VdbeCloseStatement(p, eStatementOp)的值，
      if( rc ){//rc=调用sqlite3VdbeCloseStatement返回的
        if( p->rc==SQLITE_OK || p->rc==SQLITE_CONSTRAINT ){
          p->rc = rc;//将p->rc的值都赋为sqlite3VdbeCloseStatement返回值
          sqlite3DbFree(db, p->zErrMsg);
          p->zErrMsg = 0;
        }
		//sqlite3VdbeCloseStatement返回值不等于SQLITE_OK或者SQLITE_CONSTRAINT
        sqlite3RollbackAll(db, SQLITE_ABORT_ROLLBACK);
        sqlite3CloseSavepoints(db);//关闭数据库所有的事务保存点，这个操作仅仅关闭数据库句柄对象，
		//不会关闭任何在Btree和pages中的保存点
        db->autoCommit = 1;
      }
    }
  
    /* 如果这是一个插入，更新，删除和没有任何声明式事务回滚，那么就可以对更新数据库连接改变计数器
	If this was an INSERT, UPDATE or DELETE and no statement transaction
    ** has been rolled back, update the database connection change-counter. 
    */
    if( p->changeCntOn ){//changeCntOn表示可以更新改变计数器的值
      if( eStatementOp!=SAVEPOINT_ROLLBACK ){//数据库不处于回滚到保存点
        sqlite3VdbeSetChanges(db, p->nChange);//更改数据库连接次数
      }else{
        sqlite3VdbeSetChanges(db, 0);
      }
      p->nChange = 0;//从上一次重置开始，数据库改变的次数
    }

    /* 释放数据库锁
	Release the locks */
    sqlite3VdbeLeave(p);
  }

  /* We have successfully halted and closed the VM.  Record this fact. */
  if( p->pc>=0 ){
    db->activeVdbeCnt--;
    if( !p->readOnly ){
      db->writeVdbeCnt--;
    }
    assert( db->activeVdbeCnt>=db->writeVdbeCnt );
  }
  p->magic = VDBE_MAGIC_HALT;
  checkActiveVdbeCnt(db);
  if( p->db->mallocFailed ){
    p->rc = SQLITE_NOMEM;
  }

  /* 
  如果自动提交的属性被设置为True，同时对于所有加锁的数据连接都被释放，在这种情况下调用函数sqlite3ConnectionUnlocked来
  回调所有的释放所功能，得到释放所有的资源。
  If the auto-commit flag is set to true, then any locks that were held
  ** by connection db have now been released. Call sqlite3ConnectionUnlocked() 
  ** to invoke any required unlock-notify callbacks.
  */
  if( db->autoCommit ){
    sqlite3ConnectionUnlocked(db);
  }

  assert( db->activeVdbeCnt>0 || db->autoCommit==0 || db->nStatement==0 );
  return (p->rc==SQLITE_BUSY ? SQLITE_BUSY : SQLITE_OK);
}


/*对于每一个VDBE都会保持最近的sqlite3_step（执行sql语句，得到返回结果的一行）函数得到的p->rc的值，
在qlite3VdbeResetStepResult这个函数的执行会将p->rc的值重置为SQLITE_OK
** Each VDBE holds the result of the most recent sqlite3_step() call
** in p->rc.  This routine sets that result back to SQLITE_OK.
*/
void sqlite3VdbeResetStepResult(Vdbe *p){
  p->rc = SQLITE_OK;
}

/*
将属于VDBE相关的错误代码和错误信息作为第一个参数一起传递给数据库句柄处理函数（因此在调用sqlite3_errcode() 和
sqlite3_errmsg()两个函数能够得到相应的信息）。sqlite3VdbeTransferError函数并没有处理任何错误的代码和错误信息
仅仅是将他们复制然后传递给数据库句柄
** Copy the error code and error message belonging to the VDBE passed
** as the first argument to its database handle (so that they will be 
** returned by calls to sqlite3_errcode() and sqlite3_errmsg()).
**
** This function does not clear the VDBE error code or message, just
** copies them to the database handle.
*/
int sqlite3VdbeTransferError(Vdbe *p){
  sqlite3 *db = p->db;//在Vdbe 结构中有一个sqlite3类型的变量属性
  int rc = p->rc;
  if( p->zErrMsg ){
    u8 mallocFailed = db->mallocFailed;
    sqlite3BeginBenignMalloc();
    sqlite3ValueSetStr(db->pErr, -1, p->zErrMsg, SQLITE_UTF8, SQLITE_TRANSIENT);
    sqlite3EndBenignMalloc();
    db->mallocFailed = mallocFailed;
    db->errCode = rc;
  }else{
    sqlite3Error(db, rc, 0);
  }
  return rc;
}

/*在一次运行处理后清除这个VDBE的内存资源等信息但是并不是直接将其删除。将任何出现的错误信息
赋值给参数*pzErrMsg。最后返回结果代码。在这个程序运行过后，VDBE已经准备好被下一次的执行。
从另一个方面来看，sqlite3VdbeReset函数重新将虚拟机的状态从VDBE_MAGIC_RUN或者VDBE_MAGIC_HALT设置为
VDBE_MAGIC_INIT状态。
** Clean up a VDBE after execution but do not delete the VDBE just yet.
** Write any error messages into *pzErrMsg.  Return the result code.
**
** After this routine is run, the VDBE should be ready to be executed
** again.
**
** To look at it another way, this routine resets the state of the
** virtual machine from VDBE_MAGIC_RUN or VDBE_MAGIC_HALT back to
** VDBE_MAGIC_INIT.
*/
int sqlite3VdbeReset(Vdbe *p){
  sqlite3 *db;
  db = p->db;

  /* 如果说VM程序没有完全执行结束或者在执行的过程中遇到了一个错误异常，那么可以认为当前VM并没有被很好的
  停止当前的状态。那么，我们马上将其停机。
  If the VM did not run to completion or if it encountered an
  ** error, then it might not have been halted properly.  So halt
  ** it now.
  */
  sqlite3VdbeHalt(p);

  /* 如果VDBE处于部分运行的状态，那么需要将错误代码和错误信息从VDBE中转到主数据库结构中。虽然VDBE已经被
  设置为运行状态但是没有处理任何程序指令，那么保持主数据库错误信息。
  If the VDBE has be run even partially, then transfer the error code
  ** and error message from the VDBE into the main database structure.  But
  ** if the VDBE has just been set to run but has not actually executed any
  ** instructions yet, leave the main database error information unchanged.
  */
  if( p->pc>=0 ){
    sqlite3VdbeTransferError(p);
    sqlite3DbFree(db, p->zErrMsg);
    p->zErrMsg = 0;
    if( p->runOnlyOnce ) p->expired = 1;
  }else if( p->rc && p->expired ){
    /* 在第一次调用sqlite3_step()函数之前需要设置数据结构VDBE中的属性expired值。为了数据库设置的一致性
	因为已经调用了sqlite3_step，同样也需要设置数据库错误的相关信息
	The expired flag was set on the VDBE before the first call
    ** to sqlite3_step(). For consistency (since sqlite3_step() was
    ** called), set the database error in this case as well.
    */
    sqlite3Error(db, p->rc, 0);
    sqlite3ValueSetStr(db->pErr, -1, p->zErrMsg, SQLITE_UTF8, SQLITE_TRANSIENT);
    sqlite3DbFree(db, p->zErrMsg);
    p->zErrMsg = 0;
  }

  /* 最后回收所有的被VDBE使用的内存资源
  Reclaim all memory used by the VDBE
  */
  Cleanup(p);

  /* 保存在VDBE运行是产生的分析信息
  opcode：表示具体执行什么样的操作
  cnt：指令会被执行多少次
  cycles：执行当前执行所花费的所有时间
  nOp：所有操作指令的个数
  Save profiling information from this VDBE run.
  */
#ifdef VDBE_PROFILE
  {
    FILE *out = fopen("vdbe_profile.out", "a");//打开一个只写文件，a 以附加的方式打开只写文件。
	//若文件不存在，则会建立该文件，如果文件存在，写入的数据会被加到文件尾，即文件原先的内容会被保留
    if( out ){
      int i;
      fprintf(out, "---- ");//向out中写入信息
      for(i=0; i<p->nOp; i++){
        fprintf(out, "%02x", p->aOp[i].opcode);
      }
      fprintf(out, "\n");
      for(i=0; i<p->nOp; i++){
        fprintf(out, "%6d %10lld %8lld ",
           p->aOp[i].cnt,
           p->aOp[i].cycles,
           p->aOp[i].cnt>0 ? p->aOp[i].cycles/p->aOp[i].cnt : 0
        );
        sqlite3VdbePrintOp(out, i, &p->aOp[i]);
      }
      fclose(out);//关闭文件输出流
    }
  }
#endif
  p->magic = VDBE_MAGIC_INIT;
  return p->rc & db->errMask;
}
 
/*在程序执行过后清除VDBE占用的资源并删除这个VDBE。最后返回的代码是一个实数。将整个过程中所有的错误信息
传递给指针参数pzErrMsg。
** Clean up and delete a VDBE after execution.  Return an integer which is
** the result code.  Write any error message text into *pzErrMsg.
*/
int sqlite3VdbeFinalize(Vdbe *p){
  int rc = SQLITE_OK;
  if( p->magic==VDBE_MAGIC_RUN || p->magic==VDBE_MAGIC_HALT ){//如果处于DBE_MAGIC_RUN或者VDBE_MAGIC_HALT状态的VDBE重新设置状态
    rc = sqlite3VdbeReset(p);
    assert( (rc & p->db->errMask)==rc );
  }
  sqlite3VdbeDelete(p);//删除当前的VDBE
  return rc;
}

/*
struct VdbeFunc {
  FuncDef *pFunc;               The definition of the function 
  int nAux;                      Number of entries allocated for apAux[] 
  struct AuxData {
    void *pAux;                   第i个参数的额外数据 
    void (*xDelete)(void *);       额外参数的析构函数
  } apAux[1];                    One slot for each function argument 
}
为每一个VdbeFunc中的AuxData调用析构函数释放资源而且在mask中有与之对应的位是清楚的。如果VdbeFunc中nAux
的值超过31时就会调用xDelete函数来删除实例。当mask参数值为0时会删除所有的AuxData
** Call the destructor for each auxdata entry in pVdbeFunc for which
** the corresponding bit in mask is clear.  Auxdata entries beyond 31
** are always destroyed.  To destroy all auxdata entries, call this
** routine with mask==0.
*/
void sqlite3VdbeDeleteAuxData(VdbeFunc *pVdbeFunc, int mask){
  int i;
  for(i=0; i<pVdbeFunc->nAux; i++){
    struct AuxData *pAux = &pVdbeFunc->apAux[i];
    if( (i>31 || !(mask&(((u32)1)<<i))) && pAux->pAux ){
      if( pAux->xDelete ){
        pAux->xDelete(pAux->pAux);
      }
      pAux->pAux = 0;
    }
  }
}

/*
将函数sqlite3VdbeDeleteObject的第二个参数关联Vdbe结构相关的资源全部释放掉。sqlite3VdbeDeleteObject和
sqlite3VdbeDelete最大的不同在于：VdbeDelete仅仅是将从VMs列表和与之对应的数据库连接断开
** Free all memory associated with the Vdbe passed as the second argument.
** The difference between this function and sqlite3VdbeDelete() is that
** VdbeDelete() also unlinks the Vdbe from the list of VMs associated with
** the database connection.
*/
void sqlite3VdbeDeleteObject(sqlite3 *db, Vdbe *p){
  SubProgram *pSub, *pNext;
  int i;
  assert( p->db==0 || p->db==db );
  //释放Vdbe所占用的内存空间
  releaseMemArray(p->aVar, p->nVar);
  releaseMemArray(p->aColName, p->nResColumn*COLNAME_N);
  for(pSub=p->pProgram; pSub; pSub=pNext){//pSub,和pNext都是SubProgram类型，开始pSub指向p->pProgram的起始节点，
	  //一直遍历整个链表，vdbeFreeOpArray和sqlite3DbFree释放资源
    pNext = pSub->pNext;
    vdbeFreeOpArray(db, pSub->aOp, pSub->nOp);
    sqlite3DbFree(db, pSub);
  }
  for(i=p->nzVar-1; i>=0; i--) sqlite3DbFree(db, p->azVar[i]);//nzVar表示azVar[]数组长度，便利数组azVar[]释放azVar[i]占有的资源
  vdbeFreeOpArray(db, p->aOp, p->nOp);
  sqlite3DbFree(db, p->aLabel);
  sqlite3DbFree(db, p->aColName);
  sqlite3DbFree(db, p->zSql);
  sqlite3DbFree(db, p->pFree);
#if defined(SQLITE_ENABLE_TREE_EXPLAIN)
  sqlite3DbFree(db, p->zExplain);
  sqlite3DbFree(db, p->pExplain);
#endif
  sqlite3DbFree(db, p);//释放数据库运行占用的空间
}

/*
** Delete an entire VDBE.
*/
void sqlite3VdbeDelete(Vdbe *p){
  sqlite3 *db;

  if( NEVER(p==0) ) return;//数据库已经为空，不为空时将p->db赋值给db
  db = p->db;
  assert( sqlite3_mutex_held(db->mutex) );//程序互斥
  
  if( p->pPrev ){//如果当前的Vdbe的前驱借点存在，将当前节点的后驱与其前驱链接在一起
    p->pPrev->pNext = p->pNext;
  }else{
    assert( db->pVdbe==p );
    db->pVdbe = p->pNext;
  }
  if( p->pNext ){
    p->pNext->pPrev = p->pPrev;
  }
  p->magic = VDBE_MAGIC_DEAD;
  p->db = 0;
  sqlite3VdbeDeleteObject(db, p);//释放资源
}

/*确保游标P已经准备好读或者写的行是最近的定位。如果遇到OOM错误或者I/O错误是返回错误代码，阻止我们定位
光标移动到正确的位置。如果说一个MoveTo指令在给定的光标之前出现，那么我们执行MoveTo指令。如果没有先出现
p->deferredMoveto指令那么检查在当前游标下的行是否已经被删除了，如果当前行被删除了标记当前行为NULL，
p->nullRow = 1。
如果说游标已经指向正确的行并且也没有被删除，那么这个程序就是一个no-op
** Make sure the cursor p is ready to read or write the row to which it
** was last positioned.  Return an error code if an OOM fault or I/O error
** prevents us from positioning the cursor to its correct position.
**
** If a MoveTo operation is pending on the given cursor, then do that
** MoveTo now.  If no move is pending, check to see if the row has been
** deleted out from under the cursor and if it has, mark the row as
** a NULL row.
**
** If the cursor is already pointing to the correct row and that row has
** not been deleted out from under the cursor, then this routine is a no-op.
*/
int sqlite3VdbeCursorMoveto(VdbeCursor *p){
  if( p->deferredMoveto ){
    int res, rc;
#ifdef SQLITE_TEST
    extern int sqlite3_search_count;
#endif
    assert( p->isTable );
    rc = sqlite3BtreeMovetoUnpacked(p->pCursor, 0, p->movetoTarget, 0, &res);
    if( rc ) return rc;
    p->lastRowid = p->movetoTarget;
    if( res!=0 ) return SQLITE_CORRUPT_BKPT;
    p->rowidIsValid = 1;
#ifdef SQLITE_TEST
    sqlite3_search_count++;
#endif
    p->deferredMoveto = 0;
    p->cacheStatus = CACHE_STALE;
  }else if( ALWAYS(p->pCursor) ){
    int hasMoved;
    int rc = sqlite3BtreeCursorHasMoved(p->pCursor, &hasMoved);
    if( rc ) return rc;
    if( hasMoved ){
      p->cacheStatus = CACHE_STALE;
      p->nullRow = 1;
    }
  }
  return SQLITE_OK;
}

/*下面的函数用于封装代码的序列化值存储在SQLite的数据和索引记录中。
每一个序列化的值包括序列化类型和blob类型的数据。序列化类型是一个8-byte无符号整型，被存储为一个变异体。
在一个数据库索引记录结构中，与序列化类型对应的blob数据类型会首先存储。在一个表存储结构中所有的序列化
类型数据会在记录的开始就存储，所有的blob类型的数据在记录的最后存储。因此在这些函数允许调用者分开处理
序列化数据类型和blob类型数据。下面的表详细介绍了不同的数据存储类

** The following functions:
**
** sqlite3VdbeSerialType()
** sqlite3VdbeSerialTypeLen()
** sqlite3VdbeSerialLen()
** sqlite3VdbeSerialPut()
** sqlite3VdbeSerialGet()
**
** encapsulate the code that serializes values for storage in SQLite
** data and index records. Each serialized value consists of a
** 'serial-type' and a blob of data. The serial type is an 8-byte unsigned
** integer, stored as a varint.
** 这几个函数封装了这样的功：将存储在SQLite数据库中和索引记录中的数据序列化为对应的值。
** 每个序列化的值由连续的类型和一个二进制大对象数据组成。这种连续的类型是一个8字节的无符号整形，
** 存储为varint类型。
**
** In an SQLite index record, the serial type is stored directly before
** the blob of data that it corresponds to. In a table record, all serial
** types are stored at the start of the record, and the blobs of data at
** the end. Hence these functions allow the caller to handle the
** serial-type and data blob seperately.
** 在SQLite索引记录中，序列类型直接存储在与它相对应的blob类型数据之前。在表的记录中，
** 所有序列类型都存储的记录的开始位置，blob类型的数据在数据的末尾。因此这些函数允许调用者
** 将串行类型和blob类型的数据分开操作。
**
** The following table describes the various storage classes for data:
** 下表描述了数据的各种存储类型:
**
**   serial type        bytes of data      type
**   --------------     ---------------    ---------------
**      0                     0            NULL
**      1                     1            signed integer
**      2                     2            signed integer
**      3                     3            signed integer
**      4                     4            signed integer
**      5                     6            signed integer
**      6                     8            signed integer
**      7                     8            IEEE float
**      8                     0            Integer constant 0
**      9                     0            Integer constant 1
**     10,11                               reserved for expansion
**    N>=12 and even       (N-12)/2        BLOB
**    N>=13 and odd        (N-13)/2        text
**序列化类型值N大于等于12且为偶数的，对应的字节数据使用(N-12)/2，存储类型为BLOB
  序列化类型值N大于等于13且为奇数的，对应的字节数据使用(N-13)/2，存储类型为text
  序列化类型为8，9的在3.3.0的版本中使用，以前的版本不会识别这两个值
** The 8 and 9 types were added in 3.3.0, file format 4.  Prior versions
** of SQLite will not understand those serial types.
*/

/*返回的序列化值存储在pMem中
** Return the serial-type for the value stored in pMem.
** 返回序列类型的值，存储在参数pMem中。
*/
u32 sqlite3VdbeSerialType(Mem *pMem, int file_format){
  int flags = pMem->flags;
  int n;

  if( flags&MEM_Null ){
    return 0;
  }
  if( flags&MEM_Int ){//flags&MEM_Int的值大于0，执行if里面代码
    /* 找出1, 2, 4, 6 ， 8中使用的一个
	Figure out whether to use 1, 2, 4, 6 or 8 bytes. */
#   define MAX_6BYTE ((((i64)0x00008000)<<32)-1)
    i64 i = pMem->u.i;
    u64 u;
    if( file_format>=4 && (i&1)==i ){//形参file_format>=4 并且（(i&1)==i）
      return 8+(u32)i;
    }
	
    if( i<0 ){//i<0是得到u64 u的值
      if( i<(-MAX_6BYTE) ) return 6;
      /* Previous test prevents:  u = -(-9223372036854775808) */
      u = -i;
    }else{
      u = i;
    }
    if( u<=127 ) return 1;//u<=127
    if( u<=32767 ) return 2;//127<u<=32767
    if( u<=8388607 ) return 3;//32767<u<=8388607
    if( u<=2147483647 ) return 4;//8388607<u<=2147483647
    if( u<=MAX_6BYTE ) return 5;//2147483647< u<=MAX_6BYTE
    return 6;//如果前面关于u值的判断都不符合是返回6
  }
  if( flags&MEM_Real ){//flags&MEM_Real的值大于0
    return 7;
  }
  assert( pMem->db->mallocFailed || flags&(MEM_Str|MEM_Blob) );
  n = pMem->n;
  if( flags & MEM_Zero ){
    n += pMem->u.nZero;
  }
  assert( n>=0 );
  return ((n*2) + 12 + ((flags&MEM_Str)!=0));
}

/*返回对应的序列化类型的数据长度，对于u32类型的值大于12时，返回(serial_type-12)/2；其他情况返回
aSize[] = { 0, 1, 2, 3, 4, 6, 8, 8, 0, 0, 0, 0 }中的aSize[serial_type]
** Return the length of the data corresponding to the supplied serial-type.
*/
u32 sqlite3VdbeSerialTypeLen(u32 serial_type){
  if( serial_type>=12 ){
    return (serial_type-12)/2;
  }else{
    static const u8 aSize[] = { 0, 1, 2, 3, 4, 6, 8, 8, 0, 0, 0, 0 };
    return aSize[serial_type];
  }
}

/*如果说我们需要在一个架构上面实现一个混合浮点数（例如：ARM7）那么我们需要讲低四位字节和高四位字节
  交换。然后在返回我们的结果。对于大多数硬件架构来说，不需要这样处理。
  混合浮点数问题在ARM7架构上只会出现在使用GCC时，在ARM7芯片上面不会出现异常。出现这样问题的原因是早期
  GCC版本在存储两个字的64位浮点数出现错误序列。这个错误一直从早先的GCC版本延伸到今。当然也不能全部归
  于当前的GCC问题，也可能是仅仅是从过去的编译器中复制出来的错误。在新的GCC版本中会使用不同的应用二进制
  程序接口来获取正确的混合浮点数的二进制序列。
  sqlite开发者在ARM7的架构上面编译和运行他们的程序，他们多次使用-DSQLITE_DEBUG来调试代码。根据开发者
  调试的经验来看在DEBUG模式下，一些断言能够保证混合浮点数的二进制序列是正确的。
  (2007-08-30)  Frank
** If we are on an architecture with mixed-endian floating 
** points (ex: ARM7) then swap the lower 4 bytes with the 
** upper 4 bytes.  Return the result.
**
** For most architectures, this is a no-op.
**
** (later):  It is reported to me that the mixed-endian problem
** on ARM7 is an issue with GCC, not with the ARM7 chip.  It seems
** that early versions of GCC stored the two words of a 64-bit
** float in the wrong order.  And that error has been propagated
** ever since.  The blame is not necessarily with GCC, though.
** GCC might have just copying the problem from a prior compiler.
** I am also told that newer versions of GCC that follow a different
** ABI get the byte order right.
**
** Developers using SQLite on an ARM7 should compile and run their
** application using -DSQLITE_DEBUG=1 at least once.  With DEBUG
** enabled, some asserts below will ensure that the byte order of
** floating point values is correct.
**Frank van Vugt对着这个问题做了深入的研究，他向SQLite开发者发来了他的研究成果，他在邮件中写道
一些Linux内核可以提供浮点数的硬件模拟那么我们可以使用由IEEE标准化定义的32位浮点类型来代替全48位。但是
在这样的系统中，浮点数字节位的交换会变得异常麻烦。为了避免这样的麻烦对于必须要做的字节交换会使用一个64位
的整型来代替64位的浮点数来进行交换。
** (2007-08-30)  Frank van Vugt has studied this problem closely
** and has send his findings to the SQLite developers.  Frank
** writes that some Linux kernels offer floating point hardware
** emulation that uses only 32-bit mantissas instead of a full 
** 48-bits as required by the IEEE standard.  (This is the
** CONFIG_FPE_FASTFPE option.)  On such systems, floating point
** byte swapping becomes very complicated.  To avoid problems,
** the necessary byte swapping is carried out using a 64-bit integer
** rather than a 64-bit float.  Frank assures us that the code here
** works for him.  We, the developers, have no way to independently
** verify this, but Frank seems to know what he is talking about
** so we trust him.
*/
#ifdef SQLITE_MIXED_ENDIAN_64BIT_FLOAT
static u64 floatSwap(u64 in){//64位字节交换
  union {
    u64 r;
    u32 i[2];
  } u;
  u32 t;

  u.r = in;
  t = u.i[0];
  u.i[0] = u.i[1];
  u.i[1] = t;
  return u.r;
}
# define swapMixedEndianFloat(X)  X = floatSwap(X)
#else
# define swapMixedEndianFloat(X)
#endif

/*写二进制序列化类型，将这些数据存储在pMem（Mem结构体类型）数据缓存中。这个功能使用的前提是假设调用者
已经分配了足够的内存空间。返回已经写入的字节数。形参nBuf指定数组buf[]中可以使用的空间大小，同事需要注意
nBuf的值必须足够大，能够存储所有的数据。如果说存储的数据是一个blob类型数据并且这个数据带有0维数的时候，
buf[]数组只需要有合适的空间来存储非零的数据。如果数组足够大的空间，那么只能存储二进制数据前面非零的部分
相应的也只会将前面非零数据写入buf[]缓冲区中。如果说buf[]缓冲区数组空间足够大能够存储前面非零数据和零尾
我们只需要将前面非零写入缓冲区数组中，后面的数据直接设置为0.
返回的值是已经全部写入缓冲区buf[]数组中的数据量的大小。
** Write the serialized data blob for the value stored in pMem into 
** buf. It is assumed that the caller has allocated sufficient space.
** Return the number of bytes written.
**
** nBuf is the amount of space left in buf[].  nBuf must always be
** large enough to hold the entire field.  Except, if the field is
** a blob with a zero-filled tail, then buf[] might be just the right
** size to hold everything except for the zero-filled tail.  If buf[]
** is only big enough to hold the non-zero prefix, then only write that
** prefix into buf[].  But if buf[] is large enough to hold both the
** prefix and the tail then write the prefix and set the tail to all
** zeros.
**在buf[]数组中0填充的尾部字节数被包含在返回值中，只有这些字节在数组中都是0.
** Return the number of bytes actually written into buf[].  The number
** of bytes in the zero-filled tail is included in the return value only
** if those bytes were zeroed in buf[].
*/ 
u32 sqlite3VdbeSerialPut(u8 *buf, int nBuf, Mem *pMem, int file_format){
  u32 serial_type = sqlite3VdbeSerialType(pMem, file_format);
  u32 len;

  /* Integer and Real
  serial_type的值是整型值
  */
  if( serial_type<=7 && serial_type>0 ){
    u64 v;
    u32 i;
    if( serial_type==7 ){//serial_type的值等于7时
      assert( sizeof(v)==sizeof(pMem->r) );
      memcpy(&v, &pMem->r, sizeof(v));
      swapMixedEndianFloat(v);//交换混合浮点数的高位和低位字节
    }else{//serial_type的值不等于7时，u64 v = pMem->u.i
      v = pMem->u.i;
    }
    len = i = sqlite3VdbeSerialTypeLen(serial_type);//serial_type的长度
    assert( len<=(u32)nBuf );
    while( i-- ){/*将数据写入buf[i]数组中，数组的长度为sqlite3VdbeSerialTypeLen(serial_type)*/
      buf[i] = (u8)(v&0xFF);
      v >>= 8;
    }
    return len;/*返回 u32 len=sqlite3VdbeSerialTypeLen(serial_type)*/
  }

  /* 如果序列化结构类型是字符串或者二进制文件数据
  String or blob */
  if( serial_type>=12 ){
    assert( pMem->n + ((pMem->flags & MEM_Zero)?pMem->u.nZero:0)
             == (int)sqlite3VdbeSerialTypeLen(serial_type) );
    assert( pMem->n<=nBuf );
    len = pMem->n;/*n用于存放字符串变量里字符的个数，不包括'\0'，将n的值赋给len*/
    memcpy(buf, pMem->z, len);
	/*pMem->z是指字符串类型或者二进制文件，memcpy(void *dest, void *src, unsigned int count)
	由src所指内存区域复制count个字节到dest所指内存区域*/
    if( pMem->flags & MEM_Zero ){
     /*flags在结构体Mem中起到标记作用，可取MEM_Null, MEM_Str, MEM_Dyn（Mem.z上的内存释放）三者中的一个*/
      len += pMem->u.nZero;
      assert( nBuf>=0 );
      if( len > (u32)nBuf ){
        len = (u32)nBuf;
      }
      memset(&buf[pMem->n], 0, len-pMem->n);/*对&buf[pMem->n]这一段内存空间全部设置为0*/
    }
    return len;
  }

  /* serial_type的取值为NULL或者0或者1
  NULL or constants 0 or 1 */
  return 0;
}

/*反序列化数据块所指向的缓冲区的序列化类型，并且存储反序列化的结果在Mem结构中。返回我们读取的数据字节数
** Deserialize the data blob pointed to by buf as serial type serial_type
** and store the result in pMem.  Return the number of bytes read.
*/ 
u32 sqlite3VdbeSerialGet(
  const unsigned char *buf,     /* 从第一个形参*buf进行反序列化 Buffer to deserialize from */
  u32 serial_type,              /* 序列化类型Serial type to deserialize */
  Mem *pMem                     /* 反序列化结果存储Memory cell to write value into */
){
  switch( serial_type ){/*serial_type的值为10，11是供将来使用，serial_type的值为0表示NULL，对于
						serial_type的值等于１０，１１，０时Mem->flags置为MEM_Null*/

    case 10:   /* Reserved for future use */
    case 11:   /* Reserved for future use */
    case 0: {  /* NULL */
      pMem->flags = MEM_Null;
      break;
    }
    case 1: { /*serial_type为一个字节的无符号整型值 
			  1-byte signed integer */
      pMem->u.i = (signed char)/*buf[0];将buf[0]的值转化为signed char，存储在pMem->u.i 中*/
      pMem->flags = MEM_Int;/*pMem->flags设置为MEM_Int类型*/
      return 1;
    }
    case 2: { /*serial_type为２个字节的无符号整型值  2-byte signed integer */
      pMem->u.i = (((signed char)buf[0])<<8) | buf[1];/*pMem->u.i的值为buf[0]左移８位并转化为(signed char)类型或buf[1]*/
      pMem->flags = MEM_Int;
      return 2;
    }
    case 3: { /* serial_type为３个字节的无符号整型值 3-byte signed integer */
      pMem->u.i = (((signed char)buf[0])<<16) | (buf[1]<<8) | buf[2];/*buf[0])<<16的值左移１６位，buf[1]<<8左移８位*/
      pMem->flags = MEM_Int;
      return 3;
    }
    case 4: { /* serial_type为４个字节的无符号整型值4-byte signed integer */
      pMem->u.i = (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | buf[3];/*buf[0])<<16的值左移２４位*/
      pMem->flags = MEM_Int;
      return 4;
    }
    case 5: { /*serial_type为６个字节的无符号整型值 6-byte signed integer */
      u64 x = (((signed char)buf[0])<<8) | buf[1];
      u32 y = (buf[2]<<24) | (buf[3]<<16) | (buf[4]<<8) | buf[5];
      x = (x<<32) | y;
      pMem->u.i = *(i64*)&x;
      pMem->flags = MEM_Int;
      return 6;
    }
    case 6:   /*serial_type为８个字节的无符号整型值 8-byte signed integer */
    case 7: { /* serial_type是IEEE定义的标准浮点型数　IEEE floating point */
      u64 x;
      u32 y;
#if !defined(NDEBUG) && !defined(SQLITE_OMIT_FLOATING_POINT)
      /*在验证整型数据和浮点数时可以使用相同的字节验证序列。只有当SQLITE_MIXED_ENDIAN_64BIT_FLOAT被
	  开始定义了这种类型时，64位浮点数才会被表示成混合浮点数类型。
	  Verify that integers and floating point values use the same
      ** byte order.  Or, that if SQLITE_MIXED_ENDIAN_64BIT_FLOAT is
      ** defined that 64-bit floating point values really are mixed
      ** endian.
      */
      static const u64 t1 = ((u64)0x3ff00000)<<32;
      static const double r1 = 1.0;
      u64 t2 = t1;
      swapMixedEndianFloat(t2);//交换t2的高位和低位字节
      assert( sizeof(r1)==sizeof(t2) && memcmp(&r1, &t2, sizeof(r1))==0 );
#endif

      x = (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | buf[3];/*buf[0]左移24位或运算buf[1]左移16位或运算buf[2]左移8位或运算buf[3]*/
      y = (buf[4]<<24) | (buf[5]<<16) | (buf[6]<<8) | buf[7];
      x = (x<<32) | y;/*x等于X左移32位后与y取或运算*/
      if( serial_type==6 ){/*serial_type为８个字节的无符号整型值 */
        pMem->u.i = *(i64*)&x;
        pMem->flags = MEM_Int;
      }else{
        assert( sizeof(x)==8 && sizeof(pMem->r)==8 );
        swapMixedEndianFloat(x);//交换x的高位和低位字节
        memcpy(&pMem->r, &x, sizeof(x));
		/*memcpy(void *dest, void *src, unsigned int count)
	由src所指内存区域复制count个字节到dest所指内存区域,将&x中的数据拷贝到&pMem->r*/
        pMem->flags = sqlite3IsNaN(pMem->r) ? MEM_Null : MEM_Real;/*pMem->r是实数还是空，通过二目运算符判断*/
      }
      return 8;
    }
    case 8:    /* serial_type为8，9时pMem->u.i = serial_type-8Integer 0 */
    case 9: {  /* Integer 1 */
      pMem->u.i = serial_type-8;
      pMem->flags = MEM_Int;
      return 0;
    }
    default: {/*switch的默认部分，当上门的case判断都为false时执行此部分代码*/
      u32 len = (serial_type-12)/2;
      pMem->z = (char *)buf;
      pMem->n = len;
      pMem->xDel = 0;
	  /*根据serial_type&0x01的值为真时pMem->flags = MEM_Str | MEM_Ephem，反之pMem->flags = MEM_Blob | MEM_Ephem*/
      if( serial_type&0x01 ){
        pMem->flags = MEM_Str | MEM_Ephem;
      }else{
        pMem->flags = MEM_Blob | MEM_Ephem;
      }
      return len;/*返回(serial_type-12)/2的值*/
    }
  }
  return 0;
}

/* 这个函数被用于给UnpackedRecord结构分配一个足够大的内存空间，分配足够大的空间可以被函数
sqlite3VdbeRecordUnpack()使用，如果这个函数的第一个形参是KeyInfo的结构体pKeyInfo。
这个空间的分配也可以使用sqlite3DbMallocRaw()函数或者通过第二个（*pSpace）和第三个形参（szSpace）
从未被使用的缓存空间中分配（例如空闲栈空间）。如果按照前者的分配内存空间方式形参*ppFree被设置为一个指针
类型变量。这个变量的最后垃圾回由调用者使用sqlite3DbFree函数来回收垃圾。如果内存空间的分配来自于
pSpace/szSpace两个参数指定的缓冲空间，*ppFree在程序结束之前被设置为NULL。
如果程序的运行期间发生了OOM异常，会返回NULL。
** This routine is used to allocate sufficient space for an UnpackedRecord
** structure large enough to be used with sqlite3VdbeRecordUnpack() if
** the first argument is a pointer to KeyInfo structure pKeyInfo.
**
** The space is either allocated using sqlite3DbMallocRaw() or from within
** the unaligned buffer passed via the second and third arguments (presumably
** stack space). If the former, then *ppFree is set to a pointer that should
** be eventually freed by the caller using sqlite3DbFree(). Or, if the 
** allocation comes from the pSpace/szSpace buffer, *ppFree is set to NULL
** before returning.
**
** If an OOM error occurs, NULL is returned.
*/
UnpackedRecord *sqlite3VdbeAllocUnpackedRecord(
  KeyInfo *pKeyInfo,              /* 用于描述数据记录　Description of the record */
  char *pSpace,                   /* 可用于分配的空间Unaligned space available */
  int szSpace,                    /* pSpace中可用于存储的字节数Size of pSpace[] in bytes */
  char **ppFree                   /* 程序调用者需要释放这个指针指向的资源OUT: Caller should free this pointer */
){
  UnpackedRecord *p;              /* 返回UnpackedRecord结构体类型变量　Unpacked record to return */
  int nOff;                       /* 使用nOff来增加pSpace使其对其　Increment pSpace by nOff to align it */
  int nByte;                      /* UnpackedRecord类型变量所需要的字节数　Number of bytes required for *p */

  /* 我们想要移动指针pSpace，使其和８字节对其。因此我们需要计算这样的转换需要移动的位数。如果说pSpace
  已经是８字节对其的，那么nOff的值为０。
  We want to shift the pointer pSpace up such that it is 8-byte aligned.
  ** Thus, we need to calculate a value, nOff, between 0 and 7, to shift 
  ** it by.  If pSpace is already 8-byte aligned, nOff should be zero.
  */
  nOff = (8 - (SQLITE_PTR_TO_INT(pSpace) & 7)) & 7;//计算pSpace成为8字节对其应该移动的值
  nByte = ROUND8(sizeof(UnpackedRecord)) + sizeof(Mem)*(pKeyInfo->nField+1);
  if( nByte>szSpace+nOff ){
    p = (UnpackedRecord *)sqlite3DbMallocRaw(pKeyInfo->db, nByte);
    *ppFree = (char *)p;
    if( !p ) return 0;
  }else{
    p = (UnpackedRecord*)&pSpace[nOff];
    *ppFree = 0;
  }

  p->aMem = (Mem*)&((char*)p)[ROUND8(sizeof(UnpackedRecord))];
  p->pKeyInfo = pKeyInfo;
  p->nField = pKeyInfo->nField + 1;
  return p;
}

/* 给定nKey字节大小的一条记录的二进制数据存在pKey[]，通过解码记录的第四个参数来填充UnpackedRecord结构
实例。
** Given the nKey-byte encoding of a record in pKey[], populate the 
** UnpackedRecord structure indicated by the fourth argument with the
** contents of the decoded record.
*/ 
void sqlite3VdbeRecordUnpack(
  KeyInfo *pKeyInfo,     /* 关于数据格式的信息 Information about the record format */
  int nKey,              /* 二进制记录的大小Size of the binary record */
  const void *pKey,      /* 存储二进制记录The binary record */
  UnpackedRecord *p      /* 在返回结果之前填充数据Populate this structure before returning. */
){
  const unsigned char *aKey = (const unsigned char *)pKey;
  int d; 
  u32 idx;                        /* aKey[]数组中读取数据的偏移量Offset in aKey[] to read from */
  u16 u;                          /* 循环计数 Unsigned loop counter */
  u32 szHdr;
  Mem *pMem = p->aMem;//将UnpackedRecord中的aMem实例赋值给 Mem *pMem

  p->flags = 0;
  assert( EIGHT_BYTE_ALIGNMENT(pMem) );
  idx = getVarint32(aKey, szHdr);//获取32位尾整型，Varint是一种紧凑的表示数字的方法。它用一个或多个字节来表示一个数字，值越小的数字使用越少的字节数
  d = szHdr;
  u = 0;
  while( idx<szHdr && u<p->nField && d<=nKey ){
    u32 serial_type;

    idx += getVarint32(&aKey[idx], serial_type);
    pMem->enc = pKeyInfo->enc;
    pMem->db = pKeyInfo->db;
    /*sqlite3VdbeSerialGet()函数能够帮助我们设置pMem->flags = 0
	pMem->flags = 0; // sqlite3VdbeSerialGet() will set this for us */
    pMem->zMalloc = 0;
    d += sqlite3VdbeSerialGet(&aKey[d], serial_type, pMem);
    pMem++;
    u++;
  }
  assert( u<=pKeyInfo->nField + 1 );
  p->nField = u;
}

/* 这个函数主要用来比较两个表的行数或者指定的索引记录（例如{nKey1, pKey1} 和 pPKey2）。如果key1小于key2
   返回时返回一个负数，key1等于key2返回值为0，key1大于key2时返回值为一个正数。{nKey1, pKey1}必须是由
   OP_MakeRecord关于VDBE的操作码生成的二进制文件数据。pPKey2必须由一个可以被解析的key值这个key值从遵守
   sqlite3VdbeParseRecord约束。
   Key1和Key2没有必要包含相同数目的域值。通常来说具有更少的域值的key要比具有更多的域值的key比较的次数
   更少。如果pPKey2能够满足UNPACKED_INCRKEY能够设置为真并且通用前缀相等，那么key1小于key2的值。
   或者来说UNPACKED_MATCH_PREFIX flag被设置为真而且前缀相等，那么key1和key2被认为是相等的，超过通用
   前缀的部分可以认为是忽略掉。
** This function compares the two table rows or index records
** specified by {nKey1, pKey1} and pPKey2.  It returns a negative, zero
** or positive integer if key1 is less than, equal to or 
** greater than key2.  The {nKey1, pKey1} key must be a blob
** created by th OP_MakeRecord opcode of the VDBE.  The pPKey2
** key must be a parsed key such as obtained from
** sqlite3VdbeParseRecord.
**
** Key1 and Key2 do not have to contain the same number of fields.
** The key with fewer fields is usually compares less than the 
** longer key.  However if the UNPACKED_INCRKEY flags in pPKey2 is set
** and the common prefixes are equal, then key1 is less than key2.
** Or if the UNPACKED_MATCH_PREFIX flag is set and the prefixes are
** equal, then the keys are considered to be equal and
** the parts beyond the common prefix are ignored.
*/
int sqlite3VdbeRecordCompare(
  int nKey1, const void *pKey1, /* 左key Left key */
  UnpackedRecord *pPKey2        /* 右key Right key */
){
  int d1;            /* 下一个数据元素在aKey[]数组中的位置偏移量Offset into aKey[] of next data element */
  u32 idx1;          /* 下一个头元素在aKey[]数组中的偏移量 Offset into aKey[] of next header element */
  u32 szHdr1;        /* 在header中的字节数目 Number of bytes in header */
  int i = 0;
  int nField;
  int rc = 0;
  const unsigned char *aKey1 = (const unsigned char *)pKey1;/*这定义了一个指针pKey1，pKey1可以指向任意类型的值但是必须是一个const*/
  KeyInfo *pKeyInfo;
  Mem mem1;

  pKeyInfo = pPKey2->pKeyInfo;
  mem1.enc = pKeyInfo->enc;
  mem1.db = pKeyInfo->db;
  /* 在sqlite3VdbeSerialGet函数初始化中会设置mem1.flags = 0
  mem1.flags = 0;  // Will be initialized by sqlite3VdbeSerialGet() */
  VVA_ONLY( mem1.zMalloc = 0; ) /*只有在assert()语句中会用到 Only needed by assert() statements */

  /* 编译器可能会报mem1.u.i未被初始化的异常信息。我们可以初始化这个值，也可以采用我们在这个程序中的处理
  对这个异常信息不做任何处理，直接忽略掉。但是在实际中，mem1.u.i的中从来不会出现在未被初始化的情况下使用
  这个变量，做这些其实并不需要的初始化步骤会造成一定的负面影响，因为这个程序的使用会非常的频繁。基于以上
  的原因，我们直接忽略掉编译器的异常警告，不必去初始化这个变量。

  Compilers may complain that mem1.u.i is potentially uninitialized.
  ** We could initialize it, as shown here, to silence those complaints.
  ** But in fact, mem1.u.i will never actually be used uninitialized, and doing 
  ** the unnecessary initialization has a measurable negative performance
  ** impact, since this routine is a very high runner.  And so, we choose
  ** to ignore the compiler warnings and leave this variable uninitialized.
  */
  /*  mem1.u.i = 0;  // not needed, here to silence compiler warning */
  
  idx1 = getVarint32(aKey1, szHdr1);
  d1 = szHdr1;
  nField = pKeyInfo->nField;
  while( idx1<szHdr1 && i<pPKey2->nField ){
    u32 serial_type1;

    /*为每一个key值读取序列化类型的下一个元素值
	Read the serial types for the next element in each key. */
    idx1 += getVarint32( aKey1+idx1, serial_type1 );
    if( d1>=nKey1 && sqlite3VdbeSerialTypeLen(serial_type1)>0 ) break;

    /* 提取比较的值
	Extract the values to be compared.
    */
    d1 += sqlite3VdbeSerialGet(&aKey1[d1], serial_type1, &mem1);

    /* 
	做比较
	Do the comparison
    */
    rc = sqlite3MemCompare(&mem1, &pPKey2->aMem[i],
                           i<nField ? pKeyInfo->aColl[i] : 0);
    if( rc!=0 ){
      assert( mem1.zMalloc==0 );  /* See comment below */

      /* 我们使用降序的顺序时需要反转最后结果
	  Invert the result if we are using DESC sort order. */
      if( pKeyInfo->aSortOrder && i<nField && pKeyInfo->aSortOrder[i] ){
        rc = -rc;
      }
    
      /* 如果PREFIX_SEARCH标志被设置为真并且除了最后的rowid域值外其他的值都是相等的，对于这样的情况我们
	  只需要清空PREFIX_SEARCH的设置同时设置pPKey2->rowid的值等于rowid在(pKey1, nKey1)中的值。
	  这样的处理方式被用作OP_IsUnique操作码。
	  If the PREFIX_SEARCH flag is set and all fields except the final
      ** rowid field were equal, then clear the PREFIX_SEARCH flag and set 
      ** pPKey2->rowid to the value of the rowid field in (pKey1, nKey1).
      ** This is used by the OP_IsUnique opcode.
      */
      if( (pPKey2->flags & UNPACKED_PREFIX_SEARCH) && i==(pPKey2->nField-1) ){
        assert( idx1==szHdr1 && rc );
        assert( mem1.flags & MEM_Int );
        pPKey2->flags &= ~UNPACKED_PREFIX_SEARCH;
        pPKey2->rowid = mem1.u.i;
      }
    
      return rc;
    }
    i++;
  }

  /* 可以使用函数assert()来证明是否有给mem1分配过内存空间。如果assert()语句判断为错，那么预示着出现了
  内存泄漏了需要调用sqlite3VdbeMemRelease释放内存空间。
  No memory allocation is ever used on mem1.  Prove this using
  ** the following assert().  If the assert() fails, it indicates a
  ** memory leak and a need to call sqlite3VdbeMemRelease(&mem1).
  */
  assert( mem1.zMalloc==0 );

  /*rc==0意味着有一个key值越过所有领域值和所有领域值到这点是相等的。如果UNPACKED_INCRKEY是设置为真了，
  那么需要将key2的值变大来打破这种联系。如果UPACKED_PREFIX_MATCH标志被设置为真，那些具有相同前缀的keys
  就可以被认为是相等的。其他的情况下， pPKey2的值会变得越来越大，如果存在差异的话。
      rc==0 here means that one of the keys ran out of fields and
  ** all the fields up to that point were equal. If the UNPACKED_INCRKEY
  ** flag is set, then break the tie by treating key2 as larger.
  ** If the UPACKED_PREFIX_MATCH flag is set, then keys with common prefixes
  ** are considered to be equal.  Otherwise, the longer key is the 
  ** larger.  As it happens, the pPKey2 will always be the longer
  ** if there is a difference.
  */
  assert( rc==0 );
  if( pPKey2->flags & UNPACKED_INCRKEY ){
    rc = -1;
  }else if( pPKey2->flags & UNPACKED_PREFIX_MATCH ){
    /* Leave rc==0 */
  }else if( idx1<szHdr1 ){
    rc = 1;
  }
  return rc;
}
 

/*指针pCur指向一个由OP_MakeRecord操作码创造的索引项。读取rowid的值（记录中的最后一个域）并且将这个
rowid的值存在*rowid中。如果所有事情都正常工作返回SQLITE_OK，否则返回一个错误代码。pCur也可能会指向
一个从损坏的数据库文件中获取的文本。因此这个文本数据并不可信。需要对着个文本进行合适的检查。
** pCur points at an index entry created using the OP_MakeRecord opcode.
** Read the rowid (the last field in the record) and store it in *rowid.
** Return SQLITE_OK if everything works, or an error code otherwise.
**
** pCur might be pointing to text obtained from a corrupt database file.
** So the content cannot be trusted.  Do appropriate checks on the content.
*/
int sqlite3VdbeIdxRowid(sqlite3 *db, BtCursor *pCur, i64 *rowid){
  i64 nCellKey = 0;
  int rc;
  u32 szHdr;        /* 头的长度Size of the header */
  u32 typeRowid;    /* rowid的序列化值 Serial type of the rowid */
  u32 lenRowid;     /* rowid的大小 Size of the rowid */
  Mem m, v;

  UNUSED_PARAMETER(db);

  /* 获得索引项的大小。sqlite数据库仅仅支持大小在2GiB以下的索引项，任何大于2GiB索引项的存在可以肯定的说
  这个数据库是损坏的。任何有关数据库损坏的信息可以在sqlite3BtreeParseCellPtr()函数中被发现，nCellKey是
  32位数据时可以说这个代码是安全的。
     Get the size of the index entry.  Only indices entries of less
  ** than 2GiB are support - anything large must be database corruption.
  ** Any corruption is detected in sqlite3BtreeParseCellPtr(), though, so
  ** this code can safely assume that nCellKey is 32-bits  
  */
  assert( sqlite3BtreeCursorIsValid(pCur) );
  VVA_ONLY(rc =) sqlite3BtreeKeySize(pCur, &nCellKey);
  assert( rc==SQLITE_OK );     /* pCur总是有效的因此不会发生KeySize失效的情况。pCur is always valid so KeySize cannot fail */
  assert( (nCellKey & SQLITE_MAX_U32)==(u64)nCellKey );

  /* 读取索引项的所有完整的内容。Read in the complete content of the index entry */
  memset(&m, 0, sizeof(m));
  rc = sqlite3VdbeMemFromBtree(pCur, 0, (int)nCellKey, 1, &m);
  if( rc ){
    return rc;
  }

  /* 索引项的开始必须有一个头大小。The index entry must begin with a header size */
  (void)getVarint32((u8*)m.z, szHdr);
  testcase( szHdr==3 );
  testcase( szHdr==m.n );
  if( unlikely(szHdr<3 || (int)szHdr>m.n) ){
    goto idx_rowid_corruption;
  }

  /* 索引项的最后一个域值必须是一个整型数（ROWID）。验证索引项的最后一个域值是否为一个整型值。
  The last field of the index should be an integer - the ROWID.
  ** Verify that the last entry really is an integer. */
  (void)getVarint32((u8*)&m.z[szHdr-1], typeRowid);
  testcase( typeRowid==1 );
  testcase( typeRowid==2 );
  testcase( typeRowid==3 );
  testcase( typeRowid==4 );
  testcase( typeRowid==5 );
  testcase( typeRowid==6 );
  testcase( typeRowid==8 );
  testcase( typeRowid==9 );
  if( unlikely(typeRowid<1 || typeRowid>9 || typeRowid==7) ){
    goto idx_rowid_corruption;
  }
  lenRowid = sqlite3VdbeSerialTypeLen(typeRowid);
  testcase( (u32)m.n==szHdr+lenRowid );
  if( unlikely((u32)m.n<szHdr+lenRowid) ){
    goto idx_rowid_corruption;
  }

  /* 提取索引项的最后一个整型数据。
  Fetch the integer off the end of the index record */
  sqlite3VdbeSerialGet((u8*)&m.z[m.n-lenRowid], typeRowid, &v);
  *rowid = v.u.i;
  sqlite3VdbeMemRelease(&m);
  return SQLITE_OK;

  /* 在m被分配内存数据库空间后检测到数据库损坏的异常情况，程序将跳到idx_rowid_corruption:这里来处理异常
  释放实例m占有的内存空间并返回SQLITE_CORRUPT。
  Jump here if database corruption is detected after m has been
  ** allocated.  Free the m object and return SQLITE_CORRUPT. */
idx_rowid_corruption:
  testcase( m.zMalloc!=0 );
  sqlite3VdbeMemRelease(&m);
  return SQLITE_CORRUPT_BKPT;
}

/*比较索引项的key值，VdbeCursor *pC和一个指向一个在pUnpacked中的字符串类型的两个key值做比较。
如果pC小于pUnpacked的时候向*pRes的值赋值为一个负数，等于的时候赋值为0，大于的时候赋值为一个正数。最后
在成功时返回SQLITE_OK。pUnpacked是在一个rowid或者末端的时候被创造的，因此他可以咋最后省略rowid。rowid
在索引项的最后部分会不我们直接忽视掉。所以说这个程序只是比较了keys的前缀到最后的rowid之前部分，不会比较
整个key。
** Compare the key of the index entry that cursor pC is pointing to against
** the key string in pUnpacked.  Write into *pRes a number
** that is negative, zero, or positive if pC is less than, equal to,
** or greater than pUnpacked.  Return SQLITE_OK on success.
**
** pUnpacked is either created without a rowid or is truncated so that it
** omits the rowid at the end.  The rowid at the end of the index entry
** is ignored as well.  Hence, this routine only compares the prefixes 
** of the keys prior to the final rowid, not the entire key.
*/
int sqlite3VdbeIdxKeyCompare(
  VdbeCursor *pC,             /* 用于比较The cursor to compare against */
  UnpackedRecord *pUnpacked,  /* Unpacked version of key to compare against */
  int *res                    /* 将最后的比较结果写在这个变量中。Write the comparison result here */
){
  i64 nCellKey = 0;
  int rc;
  BtCursor *pCur = pC->pCursor;
  Mem m;

  assert( sqlite3BtreeCursorIsValid(pCur) );
  VVA_ONLY(rc =) sqlite3BtreeKeySize(pCur, &nCellKey);
  assert( rc==SQLITE_OK );    /* pCur总是有效的所以不会出现KeySize失效的情况。pCur is always valid so KeySize cannot fail */
  /* nCellKey的值总是在0和0xffffffff，因为是由btreeParseCellPtr()和sqlite3GetVarint32()这两个函数实现的。
  nCellKey will always be between 0 and 0xffffffff because of the say
  ** that btreeParseCellPtr() and sqlite3GetVarint32() are implemented */
  if( nCellKey<=0 || nCellKey>0x7fffffff ){
    *res = 0;
    return SQLITE_CORRUPT_BKPT;
  }
  memset(&m, 0, sizeof(m));
  rc = sqlite3VdbeMemFromBtree(pC->pCursor, 0, (int)nCellKey, 1, &m);
  if( rc ){
    return rc;
  }
  assert( pUnpacked->flags & UNPACKED_PREFIX_MATCH );
  *res = sqlite3VdbeRecordCompare(m.n, m.z, pUnpacked);
  sqlite3VdbeMemRelease(&m);
  return SQLITE_OK;
}

/*这个函数可以设置由于后续调用sqlite3_changes()改变数据库句柄产生的值
** This routine sets the value to be returned by subsequent calls to
** sqlite3_changes() on the database handle 'db'. 
*/
void sqlite3VdbeSetChanges(sqlite3 *db, int nChange){
  assert( sqlite3_mutex_held(db->mutex) );
  db->nChange = nChange;
  db->nTotalChange += nChange;
}

/*设置一个标志在vdbe来更新计数器的变化当数据库结束或者被重置的时候。
** Set a flag in the vdbe to update the change counter when it is finalised
** or reset.
*/
void sqlite3VdbeCountChanges(Vdbe *v){
  v->changeCntOn = 1;
}

/*将每一个和数据库连接的处于准备状态的语句标记为过期的状态。一个过期状态的语句意味着重新编译这个语句是被推荐的。
一些事情的发生使得数据库语句过期。移除用户自定义的函数或者排序序列，改变一个授权功能这些都可以使
准备状态的语句变成过期状态
** Mark every prepared statement associated with a database connection
** as expired.
**
** An expired statement means that recompilation of the statement is
** recommend.  Statements expire when things happen that make their
** programs obsolete.  Removing user-defined functions or collating
** sequences, or changing an authorization function are the types of
** things that make prepared statements obsolete.
*/
void sqlite3ExpirePreparedStatements(sqlite3 *db){
  Vdbe *p;
  for(p = db->pVdbe; p; p=p->pNext){
    p->expired = 1;
  }
}

/*返回移除数据库和Vdbe的连接关系。
** Return the database associated with the Vdbe.
*/
sqlite3 *sqlite3VdbeDb(Vdbe *v){
  return v->db;
}

/*返回一个指向sqlite3_value结构体，这个结构体包括由一个VM的v实例iVar的参数值。如果这个值是一个SQL NULL
最后返回0代替。除了这个值是一个NULL，在最后返回的时候是一个SQLITE_AFF_*constants。
最后的返回值必须由函数调用者使用sqlite3ValueFree()来释放内存空间
** Return a pointer to an sqlite3_value structure containing the value bound
** parameter iVar of VM v. Except, if the value is an SQL NULL, return 
** 0 instead. Unless it is NULL, apply affinity aff (one of the SQLITE_AFF_*
** constants) to the value before returning it.
**
** The returned value must be freed by the caller using sqlite3ValueFree().
*/
sqlite3_value *sqlite3VdbeGetValue(Vdbe *v, int iVar, u8 aff){
  assert( iVar>0 );
  if( v ){
    Mem *pMem = &v->aVar[iVar-1];
    if( 0==(pMem->flags & MEM_Null) ){
      sqlite3_value *pRet = sqlite3ValueNew(v->db);
      if( pRet ){
        sqlite3VdbeMemCopy((Mem *)pRet, pMem);
        sqlite3ValueApplyAffinity(pRet, aff, SQLITE_UTF8);
        sqlite3VdbeMemStoreType((Mem *)pRet);
      }
      return pRet;
    }
  }
  return 0;
}

/*配置SQL变量iVar的值使得绑定一个新值来唤醒函数sqlite3_reoptimize()这个函数可以重复准备SQL语句使其能够
导致一个更好的执行计划
** Configure SQL variable iVar so that binding a new value to it signals
** to sqlite3_reoptimize() that re-preparing the statement may result
** in a better query plan.
*/
void sqlite3VdbeSetVarmask(Vdbe *v, int iVar){
  assert( iVar>0 );
  if( iVar>32 ){
    v->expmask = 0xffffffff;
  }else{
    v->expmask |= ((u32)1 << (iVar-1));
  }
}
