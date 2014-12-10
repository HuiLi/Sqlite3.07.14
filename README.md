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
返回结果集的列数
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
如果发生内存不足的错误时调整数组的大小，返回 SQLITE_NOMEM。在这种情况下 Vdbe.aOp 和 Vdbe.nOpAlloc
保持不变 （这样有利于任何代码已分配的操作码可以正确释放剩下的 Vdbe）。
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
使用 sqlite3VdbeResolveLabel() 函数功能来处理地址和
sqlite3VdbeChangeP4() 函数功能来更改操作数p4的值。
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
ZWhere 字符串必须从 sqlite3_malloc()中获得。此例程将持有分配内存的所有权。
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
解决标签“x”给下一条指令的地址插入。参数“x”必须从
之前调用的函数sqlite3VdbeMakeLabel()中。获得.
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
检查存储在虚拟机中与 pParse 相关联的程序是否可能抛出一个ABORT异常 （导致该声明，但不是整个事务被回滚）。
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
循环遍历程序寻找 P2 值，这个值在跳转指令上是负的。每一个这样的值是一个标签。
通过设置对其正确的 P2 值非零值解决标签。
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
添加整个操作列表到操作堆栈。返回第一个操作添加的地址。
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
为一个特定的指令改变操作数P1的值。当一个大的程序被从一个使用
sqlite3VdbeAddOpList的静态数组加载时是非常有用的，但是我们想对这个程序做
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
用于设置跳转目标时这个例程非常有用。
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
为最近添加的操作更改操作数 P5 的值。
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
更改的 P2 操作数的指令地址，以便它指向下一条指令进行编码的地址。
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
免费为 aOp 分配空间和为任何内部的操作码分配 p4 的值。如果 aOp 不是 NULL 那么它被假定包含 nOp 条目。
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
链接的子程序对象作为第二个参数传递到Vdbe.pSubProgram链表。这个列表用来删除所有子程序当VM对象不再是必需的时。
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
SQLITE_OMIT_TRACE，OP_Trace 指令总是被插入sqlite3VdbeGet()一旦一个新的VDBE被创建。所以我们可以自由地将地址设置为 p-> nOp 1，
而无须双重检查以确保其结果为非负值。但是如果定义了 SQLITE_OMIT_TRACE，则省略了 OP_Trace，我们需要在继续之前检查 p-> nOp 1 的值。
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
