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
** 这个文件里的代码是实现了VDBE的执行方法，vdbeaux.c文件控制着像创建删除VDBE实例的
** 具体细节，这个文件只在乎VDBE程序的执行.
** The code in this file implements execution method of the
** Virtual Database Engine (VDBE).  A separate file ("vdbeaux.c")
** handles housekeeping details such as creating and deleting
** VDBE instances.  This file is solely interested in executing
** the VDBE program.
**
** 在与外部接口处，sqlite3_stmt*是一个指向VDBE的不透明的指针（解释：不透明数据类型隐藏了它们内部格式或结构。在C语言中，它们就像黑盒一样。
** 支持它们的语言不是很多。作为替代，开发者们利用typedef声明一个类型，把它叫做不透明类型，希望其他人别去把它重新转化回对应的那个标准C类型。）
** In the external interface, an "sqlite3_stmt*" is an opaque pointer
** to a VDBE.
**
** SQL解析器生成一个程序然后由VDBE执行SQL语句的工作。VDBE程序在形式上类似于汇编语言。
** VDBC程序由一系列线性操作组成。每个操作都有1个操作码和5个操作数。操作数P1,P2,P3是整数。
** 操作数P4是一个以null结尾的字符串。操作数P5是一个无符号字符(P5的类型是u8，在sqliteInt.h中
** u8类型的含义是“1-byte unsigned integer”)。一些操作码全部使用这5个操作数。
** The SQL parser generates a program which is then executed by
** the VDBE to do the work of the SQL statement.  VDBE programs are
** similar in form to assembly language.  The program consists of
** a linear sequence of operations.  Each operation has an opcode
** and 5 operands.  Operands P1, P2, and P3 are integers.  Operand P4
** is a null-terminated string.  Operand P5 is an unsigned character.
** Few opcodes use all 5 operands.
** 计算结果存储在一组寄存器当中,这组寄存器的编号从1开始,寄存器存放在的内存地址为Vdbe.nMem(nMem是整型)。
** 每个寄存器可以存储一个整数，一个以NULL结尾的字符串，一个浮点数，或者是一个值为“NULL”的SQL。
** 发生这种从一种类型到另一种类型的隐式转换是必要的。
** Computation results are stored on a set of registers numbered beginning
** with 1 and going up to Vdbe.nMem.  Each register can store
** either an integer, a null-terminated string, a floating point
** number, or the SQL "NULL" value.  An implicit conversion from one
** type to the other occurs as necessary.
**
** 这个文件中的大部分代码被sqlite3VdbeExec()函数用于解析VDBE程序。
** 但是要建立一个程序指令的指令还需要其他例程(例程的作用类似于函数，但含义更为丰富一些。
** 例程是某个系统对外提供的功能接口或服务的集合)的帮助和支撑。
** Most of the code in this file is taken up by the sqlite3VdbeExec()
** function which does the work of interpreting a VDBE program.
** But other routines are also provided to help in building up
** a program instruction by instruction.
**
** 各种脚本都会扫描这个源文件以生成HTML文档，头文件，或其他派生文件。
** 因此,该文件中的代码的格式非常重要。请参阅该文件中的其他注释。
** 如果对文档内容有疑问，请在改变或添加代码时，不要违背现有的注释和代码缩进格式。
** Various scripts scan this source file in order to generate HTML
** documentation, headers files, or other derived files.  The formatting
** of the code in this file is, therefore, important.  See other comments
** in this file for details.  If in doubt, do not deviate from existing
** commenting and indentation practices when changing or adding code.
*/

/*
** 引入两个头文件：
** vdbeInt.h中定义了VDBE常用的数据结构；
** sqliteInt.h中定义了SQLite的内部接口和数据结构。
*/
#include "sqliteInt.h"
#include "vdbeInt.h"

/*
** 只有在改变操作数OP_LoadAnalysis的值之前才允许内存单元上调用这个宏。
** 这个宏可以确保潜在的备份不被滥用。
** Invoke this macro on memory cells just prior to changing the
** value of OP_LoadAnalysis the cell.  This macro verifies that shallow copies are
** not misused.
*/
#ifdef SQLITE_DEBUG
# define memAboutToChange(P,M) sqlite3VdbeMemAboutToChange(P,M)
#else
# define memAboutToChange(P,M)
#endif

/*
** The following global variable is incremented every time a cursor
** moves, either by the OP_SeekXX, OP_Next, or OP_Prev opcodes.  The test
** procedures use this information to make sure that indices are
** working correctly.  This variable has no function other than to
** help verify the correct operation of the library.
** 周敏菲补充、修改：
** 这个全局变量…………操作码。测试程序使用这个信息来确保索引的正常工作。这个变量只用于验证库操作
** 的正确性，没有别的功能
*/
#ifdef SQLITE_TEST
int sqlite3_search_count = 0;/*这个全局变量会随着游标的移动而增大，不管是通过OP_SeekXX还是OP_Next还是OP_Prev操作码，它不能帮助核实正确的操作。*/
#endif

/*
** When this global variable is positive, it gets decremented once before
** each instruction in the VDBE.  When it reaches zero, the u1.isInterrupted
** field of the sqlite3 structure is set in order to simulate an interrupt.
** 当全局变量是正值时，在每次使用VDBE执行一条指令，该变量就会递减一次。当该变量值为0，sqlite3结构体中的
** u1.isInterrupted将会被设置为true用来模仿打断状态。
**
** This facility is used for testing purposes only.  It does not function
** in an ordinary build.
** 此工具仅用于测试目的。它在正常的编译中是不起作用的。
*/
#ifdef SQLITE_TEST
int sqlite3_interrupt_count = 0;/*当这个全局变量为正数时，指令在VDBE中执行一次，它就减1，,当它变为0时，sqlite3结构体中的u1.isInterrupted区域会被设置以模拟一个中断发生*/
#endif

/*
** The next global variable is incremented each type the OP_Sort opcode
** is executed.  The test procedures use this information to make sure that
** sorting is occurring or not occurring at appropriate times.   This variable
** has no function other than to help verify the correct operation of the
** library.
** 这个全局变量是在OP_Sort操作码执行后增加1,测试步骤会使用这个信息以确定排序在
        适当的时间发生或没发生,这个变量除了帮助验证正确的库操作没有其他功能
*/
#ifdef SQLITE_TEST
int sqlite3_sort_count = 0;
#endif

/*
** The next global variable records the size of the largest MEM_Blob
** or MEM_Str that has been used by a VDBE opcode.  The test procedures
** use this information to make sure that the zero-blob functionality
** is working correctly.   This variable has no function other than to
** help verify the correct operation of the library.
** 这个全局变量记录了已经被VDBE操作码使用了的最大的MEM_Blob或者MEM_Str,测试
         步骤使用它确定zero-blob功能工作正常,这个变量除了帮助验证正确的库操作没有其他功能
*/
#ifdef SQLITE_TEST
int sqlite3_max_blobsize = 0;
static void updateMaxBlobsize(Mem *p){
  if( (p->flags & (MEM_Str|MEM_Blob))!=0 && p->n > sqlite3_max_blobsize ){
    sqlite3_max_blobsize = p->n;/*如果p指向的结构体的标记'与'上 'MEM_Str和MEM_Blob或运算的值'的结果不等于0,
								p指向的结构体对象内的n的值也大于sqlite3_max_blobsize,那么更新sqlite3_max_blobsize
								为p指向的n的值.n为字符串类型值的字符个数,*/
  }
}
#endif

/*
** The next global variable is incremented each type the OP_Found opcode
** is executed. This is used to test whether or not the foreign key
** operation implemented using OP_FkIsZero is working. This variable
** has no function other than to help verify the correct operation of the
** library.
** 这个全局变量在OP_Found操作码执行后增加1,这个是用来测试外键操作实施时,是否
** 在使用OP_FkIsZero操作码.这个变量除了帮助验证正确的库操作没有其他功能
*/
#ifdef SQLITE_TEST
int sqlite3_found_count = 0;
#endif

/*
** Test a register to see if it exceeds the current maximum blob size.
** If it does, record the new maximum blob size.
** 测试寄存器查看它是否超过当前blob类型最大长度，如果超过了就记录下形成一个新的blob大小
*/
#if defined(SQLITE_TEST) && !defined(SQLITE_OMIT_BUILTIN_TEST)
# define UPDATE_MAX_BLOBSIZE(P)  updateMaxBlobsize(P)
#else
# define UPDATE_MAX_BLOBSIZE(P)
#endif

/*
** Convert the given register into a string if it isn't one
** already. Return non-zero if a malloc() fails.
** 把给的不完整的寄存器值转换为一个字符串,如果malloc()失败,返回非零值.
*/
#define Stringify(P, enc) \
   if(((P)->flags&(MEM_Str|MEM_Blob))==0 && sqlite3VdbeMemStringify(P,enc)) \
     { goto no_mem; }

/*
** An ephemeral string value (signified by the MEM_Ephem flag) contains
** a pointer to a dynamically allocated string where some other entity
** is responsible for deallocating that string.  Because the register
** does not control the string, it might be deleted without the register
** knowing it.
** 一个临时的字符串值(MEM_Ephem标记所指的)包含一个指向动态分配的字符串的指针,一些其他
** 的实体是负责清除这个字符串的.原因是寄存器无法控制这个字符串,寄存器可能都不知道它被删除了.
**
** This routine converts an ephemeral string into a dynamically allocated
** string that the register itself controls.  In other words, it
** converts an MEM_Ephem string into an MEM_Dyn string.
** 寄存器自己控制这个程序把一个临时字符串转换为一个动态分配的字符串,
** 也就是说,它把MEM_Ephem标记所指的字符串转换为EME_Dyn标记所指的字符串.
*/
#define Deephemeralize(P) \
   if( ((P)->flags&MEM_Ephem)!=0 \
       && sqlite3VdbeMemMakeWriteable(P) ){ goto no_mem;}

/* 
** Return true if the cursor was opened using the OP_OpenSorter opcode.
** 如果使用OP_OpenSorter操作码打开了游标,就返回true
*/
#ifdef SQLITE_OMIT_MERGE_SORT
# define isSorter(x) 0
#else
# define isSorter(x) ((x)->pSorter!=0)
#endif

/*
** Argument pMem points at a register that will be passed to a
** user-defined function or returned to the user as the result of a query.
** This routine sets the pMem->type variable used by the sqlite3_value_*() 
** routines.
** 验证寄存器里的pMem指针会被一个自定义方法运行通过还是作为查询结果返回给user,这程序
** 调用sqlite3_value_*()定义了pMem->type变量
** 周敏菲修改：
** 指针变量pMem指向一个寄存器，这个寄存器会被传递给一个用户自定义的功能函数，或者作为
** 查询的结果返回给用户。下面这个函数会给变量pMem->type赋不同的值，pMem->type还会在
** sqlite3_value_*()函数中被调用。
*/
void sqlite3VdbeMemStoreType(Mem *pMem){
  int flags = pMem->flags;
  if( flags & MEM_Null ){
    pMem->type = SQLITE_NULL;
  }
  else if( flags & MEM_Int ){
    pMem->type = SQLITE_INTEGER;
  }
  else if( flags & MEM_Real ){
    pMem->type = SQLITE_FLOAT;
  }
  else if( flags & MEM_Str ){
    pMem->type = SQLITE_TEXT;
  }else{
    pMem->type = SQLITE_BLOB;
  }
}

/*
** Allocate VdbeCursor number iCur.Return a pointer to it.  Return NULL
** if we run out of memory.
** 分配Vdbe游标数 iCur,返回一个指针给它,如果内存用尽就返回NULL.
*/
static VdbeCursor *allocateCursor(
  Vdbe *p,              /* The virtual machine 虚拟机指针*/
  int iCur,             /* Index of the new VdbeCursor 新建的虚拟机游标索引值*/
  int nField,           /* Number of fields in the table or index ** 表中字段或索引的数量*/
  int iDb,              /* Database the cursor belongs to, or -1 ** 这个游标属于哪个数据库，或者iDb = -1 */
  int isBtreeCursor     /* True for B-Tree.False for pseudo-table or vtab B树就为true,虚表或者假表为false*/
){
  /* Find the memory cell that will be used to store the blob of memory
  ** required for this VdbeCursor structure. It is convenient to use a 
  ** vdbe memory cell to manage the memory allocation required for a
  ** VdbeCursor structure for the following reasons:
  **
  **   * Sometimes cursor numbers are used for a couple of different
  **     purposes in a vdbe program. The different uses might require
  **     different sized allocations. Memory cells provide growable
  **     allocations.
  **
  **   * When using ENABLE_MEMORY_MANAGEMENT, memory cell buffers can
  **     be freed lazily via the sqlite3_release_memory() API. This
  **     minimizes the number of malloc calls made by the system.
  **
  ** Memory cells for cursors are allocated at the top of the address
  ** space. Memory cell (p->nMem) corresponds to cursor 0. Space for
  ** cursor 1 is managed by memory cell (p->nMem-1), etc.
  ** 找到这个VdbeCursor结构体需要的会被用来存储二进制大对象的存储单元.使用一个vdbe
  ** 存储单元来管理一个VdbeCursor结构体所需要的内存分配是很方便的,原因如下:
  ** 一,在vdbe程序中,游标数有时被用于两个不同的目的,不同的用途可能会需要不同
  ** 的内存分配,而内存单元提供了可增长的分配机制.
  ** 二,当使用ENBALE_MEMORY_MANAGEMENT时,内存单元缓冲区可以被sqlite3_release_memory()API
  ** 释放,把内存分配数量最小化是系统决定的.
  ** 分配给游标的内存存储单元在地址空间的最顶端。虚拟机P在内存中的存储位置nMem(p->nMem)对应于游标0。
  ** 游标1是由内存单元(p->nMem-1)来管理,等等。
  */
  Mem *pMem = &p->aMem[p->nMem-iCur];

  int nByte;
  VdbeCursor *pCx = 0;
  nByte = 
      ROUND8(sizeof(VdbeCursor)) + 
      (isBtreeCursor?sqlite3BtreeCursorSize():0) + 
      2*nField*sizeof(u32);

  assert( iCur<p->nCursor );
  if( p->apCsr[iCur] ){
    sqlite3VdbeFreeCursor(p, p->apCsr[iCur]);
    p->apCsr[iCur] = 0;
  }
  if( SQLITE_OK==sqlite3VdbeMemGrow(pMem, nByte, 0) ){
    p->apCsr[iCur] = pCx = (VdbeCursor*)pMem->z;
    memset(pCx, 0, sizeof(VdbeCursor));
    pCx->iDb = iDb;
    pCx->nField = nField;
    if( nField ){
      pCx->aType = (u32 *)&pMem->z[ROUND8(sizeof(VdbeCursor))];
    }
    if( isBtreeCursor ){
      pCx->pCursor = (BtCursor*)
          &pMem->z[ROUND8(sizeof(VdbeCursor))+2*nField*sizeof(u32)];
      sqlite3BtreeCursorZero(pCx->pCursor);
    }
  }
  return pCx;
}

/*
** Try to convert a value into a numeric representation if we can
** do so without loss of information.  In other words, if the string
** looks like a number, convert it into a number.  If it does not
** look like a number, leave it alone.
** 在不丢失信息的提前下,尝试把一个像数字的值转换为数字。话句话说，如果一个字符串非常像一个数字，
** 将它转换成一个数字。如果不像数字，保持它的原型
*/
static void applyNumericAffinity(Mem *pRec){
  if( (pRec->flags & (MEM_Real|MEM_Int))==0 ){//满足pRec数据不是Int 或 Real要求
    double rValue;
    i64 iValue;
    u8 enc = pRec->enc;
    if( (pRec->flags&MEM_Str)==0 ) return;//如果字符串为空
    if( sqlite3AtoF(pRec->z, &c, pRec->n, enc)==0 ) return;//先试着转换成Real类型
    if( 0==sqlite3Atoi64(pRec->z, &iValue, pRec->n, enc) ){//调用sqlite3Atoi64函数对数据进行转换，转换后的数据存储在sqlite3Atoi64的参数pResult里，即iValue
      pRec->u.i = iValue;
      pRec->flags |= MEM_Int;
    }else{
      pRec->r = rValue;
      pRec->flags |= MEM_Real;
    }
  }
}

/*
** Processing is determine by the affinity parameter:
** 执行的过程由下面这几个参数决定
** SQLITE_AFF_INTEGER:
** SQLITE_AFF_REAL:
** SQLITE_AFF_NUMERIC:
**    Try to convert pRec to an integer representation or a
**    floating-point representation if an integer representation
**    is not possible.  Note that the integer representation is
**    always preferred, even if the affinity is REAL, because
**    an integer representation is more space efficient on disk.
** 尝试把Mem结构体类型的指针pRec转换为一个整形,如果不能转换为整形就转换为浮点型
** 要注意到整形是优先的,即使最像的参数是一个REAL类型,这是因为在磁盘上整形的空间利用率更高
** SQLITE_AFF_TEXT:
**    Convert pRec to a text representation.
**    把pRec转换为文本
** SQLITE_AFF_NONE:
**    No-op.  pRec is unchanged.
**    无操作,pRec不变
*/
static void applyAffinity(
  Mem *pRec,          /* The value to apply affinity to 应用关联的值*/
  char affinity,      /* The affinity to be applied 被应用的关联*/
  u8 enc              /* Use this text encoding 使用此文本编码*/
){
  if( affinity==SQLITE_AFF_TEXT ){
    /* Only attempt the conversion to TEXT if there is an integer or real
    ** representation (blob and NULL do not get converted) but no string
    ** representation.如果有一个整数或实数则仅尝试转换为文本形式。（blob和null没有被转换）但没有字符串表示形式。
    */
    if( 0==(pRec->flags&MEM_Str) && (pRec->flags&(MEM_Real|MEM_Int)) ){//如果字符数据不是string real int类型数据
      sqlite3VdbeMemStringify(pRec, enc);//转换成string型数据
    }
    pRec->flags &= ~(MEM_Real|MEM_Int);
  }else if( affinity!=SQLITE_AFF_NONE ){
    assert( affinity==SQLITE_AFF_INTEGER || affinity==SQLITE_AFF_REAL
             || affinity==SQLITE_AFF_NUMERIC );
    applyNumericAffinity(pRec);
    if( pRec->flags & MEM_Real ){
      sqlite3VdbeIntegerAffinity(pRec);
    }
  }
}

/*
** Try to convert the type of a function argument or a result column
** into a numeric representation.  Use either INTEGER or REAL whichever
** is appropriate.  But only do the conversion if it is possible without
** loss of information and return the revised type of the argument.
** 周敏菲修改：
** 尝试把一个函数参数或者一个结果行转换为一个数字表示的表达式.使用INTEGER或REAL中的合
** 适的一个。但是只有在没有信息丢失的情况下才进行转换，同时返回一个修改后的type参数。
*/
int sqlite3_value_numeric_type(sqlite3_value *pVal){
  Mem *pMem = (Mem*)pVal;
  if( pMem->type==SQLITE_TEXT ){
    applyNumericAffinity(pMem);
    sqlite3VdbeMemStoreType(pMem);//存储修改后的pMem
  }
  return pMem->type;//返回type类型
}

/*
** Exported version of applyAffinity(). This one works on sqlite3_value*, 
** not the internal Mem* type.
** 使用sqlite3_value*类型参数的applyAffinity()函数,它没使用Mem*类型的参数
** 但是怎么感觉函数体还是调用的使用Mem*类型的applyAffinity函数,而且还是把sqlite3_value*
** 类型强制转换为了Mem*类型.
** 周敏菲修改：
** 函数applyAffinity()的另一种输出版本。这个版本需要一个sqlite3_value*类型的参数，
** 而不是像上面那个applyAffinity()函数一样需要一个Mem*类型的参数。
*/
void sqlite3ValueApplyAffinity(
  sqlite3_value *pVal,
  u8 affinity,
  u8 enc
){
  applyAffinity((Mem *)pVal, affinity, enc);
}

#ifdef SQLITE_DEBUG
/*
** Write a nice string representation of the contents of cell pMem
** into buffer zBuf, length nBuf.
** 将pMem所在内存单元中的字符串表达式的内容写入到缓冲字符串变量zBuf中，字符串长度写入nBuf中。
*/
void sqlite3VdbeMemPrettyPrint(Mem *pMem, char *zBuf){
  char *zCsr = zBuf;
  int f = pMem->flags;

  static const char *const encnames[] = {"(X)", "(8)", "(16LE)", "(16BE)"};

  if( f&MEM_Blob ){
    int i;
    char c;
    if( f & MEM_Dyn ){
      c = 'z';
      assert( (f & (MEM_Static|MEM_Ephem))==0 );
    }else if( f & MEM_Static ){
      c = 't';
      assert( (f & (MEM_Dyn|MEM_Ephem))==0 );
    }else if( f & MEM_Ephem ){
      c = 'e';
      assert( (f & (MEM_Static|MEM_Dyn))==0 );
    }else{
      c = 's';
    }

    sqlite3_snprintf(100, zCsr, "%c", c);
    zCsr += sqlite3Strlen30(zCsr);
    sqlite3_snprintf(100, zCsr, "%d[", pMem->n);
    zCsr += sqlite3Strlen30(zCsr);
    for(i=0; i<16 && i<pMem->n; i++){
      sqlite3_snprintf(100, zCsr, "%02X", ((int)pMem->z[i] & 0xFF));
      zCsr += sqlite3Strlen30(zCsr);
    }
    for(i=0; i<16 && i<pMem->n; i++){
      char z = pMem->z[i];
      if( z<32 || z>126 ) *zCsr++ = '.';
      else *zCsr++ = z;
    }

    sqlite3_snprintf(100, zCsr, "]%s", encnames[pMem->enc]);
    zCsr += sqlite3Strlen30(zCsr);
    if( f & MEM_Zero ){
      sqlite3_snprintf(100, zCsr,"+%dz",pMem->u.nZero);
      zCsr += sqlite3Strlen30(zCsr);
    }
    *zCsr = '\0';
  }else if( f & MEM_Str ){
    int j, k;
    zBuf[0] = ' ';
    if( f & MEM_Dyn ){
      zBuf[1] = 'z';
      assert( (f & (MEM_Static|MEM_Ephem))==0 );
    }else if( f & MEM_Static ){
      zBuf[1] = 't';
      assert( (f & (MEM_Dyn|MEM_Ephem))==0 );
    }else if( f & MEM_Ephem ){
      zBuf[1] = 'e';
      assert( (f & (MEM_Static|MEM_Dyn))==0 );
    }else{
      zBuf[1] = 's';
    }
    k = 2;
    sqlite3_snprintf(100, &zBuf[k], "%d", pMem->n);
    k += sqlite3Strlen30(&zBuf[k]);
    zBuf[k++] = '[';
    for(j=0; j<15 && j<pMem->n; j++){
      u8 c = pMem->z[j];
      if( c>=0x20 && c<0x7f ){
        zBuf[k++] = c;
      }else{
        zBuf[k++] = '.';
      }
    }
    zBuf[k++] = ']';
    sqlite3_snprintf(100,&zBuf[k], encnames[pMem->enc]);
    k += sqlite3Strlen30(&zBuf[k]);
    zBuf[k++] = 0;
  }
}
#endif

#ifdef SQLITE_DEBUG
/*
** Print the value of a register for tracing purposes:
** 打印寄存器的值，便于dubug时追踪程序的运行情况
*/
static void memTracePrint(FILE *out, Mem *p){
  if( p->flags & MEM_Null ){
    fprintf(out, " NULL");
  }else if( (p->flags & (MEM_Int|MEM_Str))==(MEM_Int|MEM_Str) ){
    fprintf(out, " si:%lld", p->u.i);
  }else if( p->flags & MEM_Int ){
    fprintf(out, " i:%lld", p->u.i);
#ifndef SQLITE_OMIT_FLOATING_POINT
  }else if( p->flags & MEM_Real ){
    fprintf(out, " r:%g", p->r);
#endif
  }else if( p->flags & MEM_RowSet ){
    fprintf(out, " (rowset)");
  }else{
    char zBuf[200];
    sqlite3VdbeMemPrettyPrint(p, zBuf);
    fprintf(out, " ");
    fprintf(out, "%s", zBuf);
  }
}
static void registerTrace(FILE *out, int iReg, Mem *p){
  fprintf(out, "REG[%d] = ", iReg);
  memTracePrint(out, p);
  fprintf(out, "\n");
}
#endif

#ifdef SQLITE_DEBUG
#  define REGISTER_TRACE(R,M) if(p->trace)registerTrace(p->trace,R,M)
#else
#  define REGISTER_TRACE(R,M)
#endif


#ifdef VDBE_PROFILE

/*
** hwtime.h contains inline assembler code for implementing
** high-performance timing routines.
** hwtime.h包含了内联汇编代码用来执行高性能时间程序(还不清楚是计时还是定时)
*/
#include "hwtime.h"

#endif

/*
** The CHECK_FOR_INTERRUPT macro defined here looks to see if the
** sqlite3_interrupt() routine has been called.  If it has been, then
** processing of the VDBE program is interrupted.
**
** This macro added to every instruction that does a jump in order to
** implement a loop.  This test used to be on every single instruction,
** but that meant we more testing than we needed.  By only testing the
** flag on jump instructions, we get a (small) speed improvement.
** CHECK_FOR_INTERRUPT宏在这里定义用来监视sqlite3_interrupt()是否已经被调用
** 如果调用了,运行着的VDBE程序就中断.
** 给每一条跳转指令添加这个宏,用来实现循环(翻译有歧义,还要再细看).虽然这个测试被用于每
** 一条单一的指令,但是这意味着我们做了比我们需要的更多的测试.通过仅仅测试跳转指令的标记,
** 我们得到了一些速度的提升.
*/
#define CHECK_FOR_INTERRUPT \
   if( db->u1.isInterrupted ) goto abort_due_to_interrupt;


#ifndef NDEBUG
/*
** This function is only called from within an assert() expression. It
** checks that the sqlite3.nTransaction variable is correctly set to
** the number of non-transaction savepoints currently in the
** linked list starting at sqlite3.pSavepoint.
**
** Usage:
**     assert( checkSavepointCount(db) );
**
**周敏菲修改：
** checkSavepointCount()这个函数仅仅被assert( checkSavepointCount(db) )回调.
** 它用于检测变量sqlite3.nTransaction被正确赋值为当前链表中非事务性存储点的数目，这个链表
** 的起始点为sqlite3.pSavepoint。
**
**
*/
static int checkSavepointCount(sqlite3 *db){
  int n = 0;
  Savepoint *p;
  for(p=db->pSavepoint; p; p=p->pNext) n++;
  assert( n==(db->nSavepoint + db->isTransactionSavepoint) );
  return 1;
}
#endif

/*
** Transfer error message text from an sqlite3_vtab.zErrMsg (text stored
** in memory obtained from sqlite3_malloc) into a Vdbe.zErrMsg (text stored
** in memory obtained from sqlite3DbMalloc).
** 把一个sqlite3_vtab.zErrMsg(文本存储在从sqlite3_malloc分配来的内存中)中的错误消息
** 传送到一个Vdbe.zErrMsg(文本存储在从sqlite3Malloc分配来的内存中)中
*/
static void importVtabErrMsg(Vdbe *p, sqlite3_vtab *pVtab){
  sqlite3 *db = p->db;
  sqlite3DbFree(db, p->zErrMsg);//释放数据库连接关联的内存
  p->zErrMsg = sqlite3DbStrDup(db, pVtab->zErrMsg);//从sqliteMalloc函数拷贝一个字符串
  sqlite3_free(pVtab->zErrMsg);//释放sqlite里的zErrMsg
  pVtab->zErrMsg = 0;
}


/*
** Execute as much of a VDBE program as we can then return.
**
** sqlite3VdbeMakeReady() must be called before this routine in order to
** close the program with a final OP_Halt and to set up the callbacks
** and the error message pointer.
**
** Whenever a row or result data is available, this routine will either
** invoke the result callback (if there is one) or return with
** SQLITE_ROW.
**
** If an attempt is made to open a locked database, then this routine
** will either invoke the busy callback (if there is one) or it will
** return SQLITE_BUSY.
**
** If an error occurs, an error message is written to memory obtained
** from sqlite3_malloc() and p->zErrMsg is made to point to that memory.
** The error code is stored in p->rc and this routine returns SQLITE_ERROR.
**
** If the callback ever returns non-zero, then the program exits
** immediately.  There will be no error message but the p->rc field is
** set to SQLITE_ABORT and this routine will return SQLITE_ERROR.
**
** A memory allocation error causes p->rc to be set to SQLITE_NOMEM and this
** routine to return SQLITE_ERROR.
**
** Other fatal errors return SQLITE_ERROR.
**
** After this routine has finished, sqlite3VdbeFinalize() should be
** used to clean up the mess that was left behind.
**
** sqlite3VdbeExec()运行前一定要调用sqlite3VdbeMakeReady(),这样最后
** 一个OP_Halt能关闭程序,设置回调函数和错误信息指针,无论何时一行数据或者
** 结果已得出,这个程序会调用结果的回调函数,返回SQLITE_ROW.如果一个请求
** 是要打开上锁的数据库,这个程序会调用busy回调函数,返回SQLITE_BUSY.如果
** 一个错误发生,一条错误信息会写入由sqlite3_malloc()分配的内存中,产生
** p->zErrMsg会指向这块内存,错误代码会存放在p->rc,然后返回SQLITE_ERROR
** 如果回调函数一直返回非零值,程序就会立即退出,虽然不会有错误信息,但是
** p->r字段设置为了SQLITE_ABORT,然后程序返回SQLITE_ERROR.内存分配错误会
** 导致p->rc被设置为SQLITE_NOMEM,程序也会返回SQLITE_ERROR,其他一些致命
** 错误也会返回SQLITE_ERROR,这个程序执行结束,sqlite3VdbeFinalize()会执行
** 用来清除刚刚留下的垃圾
*/


int sqlite3VdbeExec(Vdbe *p/* The VDBE */){
  int pc=0;                  /* The program counter 指令计数器*/
  Op *aOp = p->aOp;          /* Copy of p->aOp  拷贝指令*/
  Op *pOp;                   /* Current operation 当前指令*/
  int rc = SQLITE_OK;        /* Value to return */
  sqlite3 *db = p->db;       /* The database 数据库*/
  u8 resetSchemaOnFault = 0; /* Reset schema after an error if positive 有错误发生重置计划*/
  u8 encoding = ENC(db);     /* The database encoding 数据库编码格式。 */
#ifndef SQLITE_OMIT_PROGRESS_CALLBACK
  int checkProgress;         /* True if progress callbacks are enabled 如果进展回调可用就为真*/
  int nProgressOps = 0;      /* Opcodes executed since progress callback. 操作码将会执行如果进展回调*/
#endif
  Mem *aMem = p->aMem;       /* Copy of p->aMem 复制p->aMem*/
  Mem *pIn1 = 0;             /* 1st input operand 第一次输入的操作数*/
  Mem *pIn2 = 0;             /* 2nd input operand 第二次输入的操作数*/
  Mem *pIn3 = 0;             /* 3rd input operand 第三次输入的操作数*/
  Mem *pOut = 0;             /* Output operand 输出操作数*/
  int iCompare = 0;          /* Result of last OP_Compare operation 存放操作码OP_Compare的操作结果*/
  int *aPermute = 0;         /* Permutation of columns for OP_Compare 操作码OP_Compare使用的数组*/
  i64 lastRowid = db->lastRowid;  /* Saved value of the last insert ROWID 保存最后插入的ROWID值*/
#ifdef VDBE_PROFILE
  u64 start;                 /* CPU clock count at start of opcode 操作码开始的CPU时钟计数*/
  int origPc;              /* Program counter at start of opcode 操作码开始的程序计数器*/
#endif
  /* Program counter at start of opcode操作码开始的程序计数器 */
#endif
  /*** INSERT STACK UNION HERE ***/

  assert( p->magic==VDBE_MAGIC_RUN );  /* sqlite3_step() verifies this sqlite3_step()核实这项*/
  sqlite3VdbeEnter(p);//锁定B树 如果SQLite编译支持共享缓存模式和线程安全,这个程序获得与每个BtShared结构关联的互斥锁,可能被VM访问作为一个参数传递。这样做也可以设置BtShared.db成员的每个BtShared结构,确保如果需要的时候正确的busy-handler被调用。
  if( p->rc==SQLITE_NOMEM ){
    /* This happens if a malloc() inside a call to sqlite3_column_text() or
    ** sqlite3_column_text16() failed.  一个malloc()里的调用sqlite3_column_text()或sqlite3_column_text16()失败则这项发生*/
    goto no_mem;
  }
  assert( p->rc==SQLITE_OK || p->rc==SQLITE_BUSY );
  p->rc = SQLITE_OK;
  assert( p->explain==0 );
  p->pResultSet = 0;
  db->busyHandler.nBusy = 0;
  CHECK_FOR_INTERRUPT;
  sqlite3VdbeIOTraceSql(p);//打印一个IO的跟踪消息，显示SQL内容
#ifndef SQLITE_OMIT_PROGRESS_CALLBACK
  checkProgress = db->xProgress!=0;
#endif
#ifdef SQLITE_DEBUG
  sqlite3BeginBenignMalloc();
  if( p->pc==0  && (p->db->flags & SQLITE_VdbeListing)!=0 ){
    int i;
    printf("VDBE Program Listing:\n");
    sqlite3VdbePrintSql(p);
    for(i=0; i<p->nOp; i++){
      sqlite3VdbePrintOp(stdout, i, &aOp[i]);
    }
  }
  sqlite3EndBenignMalloc();
#endif
  for(pc=p->pc; rc==SQLITE_OK; pc++){
    assert( pc>=0 && pc<p->nOp );
    if( db->mallocFailed ) goto no_mem;
#ifdef VDBE_PROFILE
    origPc = pc;
    start = sqlite3Hwtime();
#endif
    pOp = &aOp[pc];

    /* Only allow tracing if SQLITE_DEBUG is defined.如果SQLITE_DEBUG被定义则仅允许跟踪
    */
#ifdef SQLITE_DEBUG
    if( p->trace ){
      if( pc==0 ){
        printf("VDBE Execution Trace:\n");
        sqlite3VdbePrintSql(p);
      }
      sqlite3VdbePrintOp(p->trace, pc, pOp);
    }
#endif
      

    /* Check to see if we need to simulate an interrupt.  This only happens
    ** if we have a special test build.
    ** 如果我们需要模拟一个中断，可以查看下面的代码。只有定义SQLITE_TEST宏时这段代码才起作用。
    */
#ifdef SQLITE_TEST
    if( sqlite3_interrupt_count>0 ){
      sqlite3_interrupt_count--;
      if( sqlite3_interrupt_count==0 ){
        sqlite3_interrupt(db);
      }
    }
#endif

#ifndef SQLITE_OMIT_PROGRESS_CALLBACK
    /* Call the progress callback if it is configured and the required number
    ** of VDBE ops have been executed (either since this invocation of
    ** sqlite3VdbeExec() or since last time the progress callback was called).
    ** If the progress callback returns non-zero, exit the virtual machine with
    ** a return code SQLITE_ABORT.
    ** 如果VDBE配置过,请求的VDBE操作码都已经执行完(要么是sqlite3VdbeExec()这次的
    ** 调用,要么是上一次程序调用回调函数),如果回调函数返回了非零值,退出虚拟机返回
    ** SQLITE_ABORT
    */
    if( checkProgress ){
      if( db->nProgressOps==nProgressOps ){
        int prc;
        prc = db->xProgress(db->pProgressArg);
        if( prc!=0 ){
          rc = SQLITE_INTERRUPT;
          goto vdbe_error_halt;
        }
        nProgressOps = 0;
      }
      nProgressOps++;
    }
#endif

    /* On any opcode with the "out2-prerelease" tag, free any
    ** external allocations out of mem[p2] and set mem[p2] to be
    ** an undefined integer.  Opcodes will either fill in the integer
    ** value or convert mem[p2] to a different type.
    ** 任何一个有"out2-prerelease"注释标签的操作码都会释放mem[p2]外部分
    ** 配的内存,设置mem[p2]为一个未定义的整形,这些操作码不是被整形值填满
    ** 就是转换mem[p2]为其他的类型.
    */
    assert( pOp->opflags==sqlite3OpcodeProperty[pOp->opcode] );
    if( pOp->opflags & OPFLG_OUT2_PRERELEASE ){
      assert( pOp->p2>0 );
      assert( pOp->p2<=p->nMem );
      pOut = &aMem[pOp->p2];
      memAboutToChange(p, pOut);
      VdbeMemRelease(pOut);
      pOut->flags = MEM_Int;
    }

    /* Sanity checking on other operands 检查其他操作数*/
#ifdef SQLITE_DEBUG
    if( (pOp->opflags & OPFLG_IN1)!=0 ){
      assert( pOp->p1>0 );
      assert( pOp->p1<=p->nMem );
      assert( memIsValid(&aMem[pOp->p1]) );
      REGISTER_TRACE(pOp->p1, &aMem[pOp->p1]);
    }
    if( (pOp->opflags & OPFLG_IN2)!=0 ){
      assert( pOp->p2>0 );
      assert( pOp->p2<=p->nMem );
      assert( memIsValid(&aMem[pOp->p2]) );
      REGISTER_TRACE(pOp->p2, &aMem[pOp->p2]);
    }
    if( (pOp->opflags & OPFLG_IN3)!=0 ){
      assert( pOp->p3>0 );
      assert( pOp->p3<=p->nMem );
      assert( memIsValid(&aMem[pOp->p3]) );
      REGISTER_TRACE(pOp->p3, &aMem[pOp->p3]);
    }
    if( (pOp->opflags & OPFLG_OUT2)!=0 ){
      assert( pOp->p2>0 );
      assert( pOp->p2<=p->nMem );
      memAboutToChange(p, &aMem[pOp->p2]);
    }
    if( (pOp->opflags & OPFLG_OUT3)!=0 ){
      assert( pOp->p3>0 );
      assert( pOp->p3<=p->nMem );
      memAboutToChange(p, &aMem[pOp->p3]);
    }
#endif
  
    switch( pOp->opcode ){

/*****************************************************************************
** What follows is a massive switch statement where each case implements a
** separate instruction in the virtual machine.  If we follow the usual
** indentation conventions, each case should be indented by 6 spaces.  But
** that is a lot of wasted space on the left margin.  So the code within
** the switch statement will break with convention and be flush-left. Another
** big comment (similar to this one) will mark the point in the code where
** we transition back to normal indentation.
** 下面这一大坨switch语句,每一个case都是在VDBE里执行一个单独的指令。
**
** The formatting of each case is important.  The makefile for SQLite
** generates two C files "opcodes.h" and "opcodes.c" by scanning this
** file looking for lines that begin with "case OP_".  The opcodes.h files
** will be filled with #defines that give unique integer values to each
** opcode and the opcodes.c file is filled with an array of strings where
** each string is the symbolic name for the corresponding opcode.  If the
** case statement is followed by a comment of the form "/# same as ... #/"
** that comment is used to determine the particular value of the opcode.
** 每一条case的格式非常重要,对SQLite执行makefile命令时,扫描这些case后生成两个c文件,
** opcodes.h和opcodes.c.opcodes.h文件里是每一个opcode对应的unique整型值的define
** 语句,opcodes.c文件是一个string类型数组,每一个string是一个opcode对应的象征名字
** 如果case语句在一个"/# same as ... #/"评论块里,那这个评价用来判断这条opcode的
** 特殊值.
**
** Other keywords in the comment that follows each case are used to
** construct the OPFLG_INITIALIZER value that initializes opcodeProperty[].
** Keywords include: in1, in2, in3, out2_prerelease, out2, out3.  See
** the mkopcodeh.awk script for additional information.
** 下列的关键字是用来构造初始化opcodeProperty[]数组时OPFLG_INITIALIZER的
** 值,包括:in1, in2, in3, out2_prerelease, out2, out3.
**
** Documentation about VDBE opcodes is generated by scanning this file
** for lines of that contain "Opcode:".  That line and all subsequent
** comment lines are used in the generation of the opcode.html documentation
** file.
** VDBE操作码文档是通过扫描这个文件中包含“Opcode:”的行来生成的。
** 这条线和所有后续注释行都用于生成opcode.html这个文档文件。
**
** SUMMARY:
**
**     Formatting is important to scripts that scan this file.
**     Do not deviate from the formatting style currently in use.
**
*****************************************************************************/

/* Opcode:  Goto * P2 * * *
**
** An unconditional jump to address P2.
** The next instruction executed will be 
** the one at index P2 from the beginning of
** the program.
** 一个无条件跳转到P2地址的操作码,下一条执行的指令会是从程序一开始就有的的索引P2里的
*/
case OP_Goto: {             /* jump */
  CHECK_FOR_INTERRUPT;
  pc = pOp->p2 - 1;
  break;
}

/* Opcode:  Gosub P1 P2 * * *
**
** Write the current address onto register P1
** and then jump to address P2.
** 把当前地址写入寄存器P1,然后跳到P2地址
*/
case OP_Gosub: {            /* jump */
  assert( pOp->p1>0 && pOp->p1<=p->nMem );
  pIn1 = &aMem[pOp->p1];
  assert( (pIn1->flags & MEM_Dyn)==0 );
  memAboutToChange(p, pIn1);
  pIn1->flags = MEM_Int;
  pIn1->u.i = pc;
  REGISTER_TRACE(pOp->p1, pIn1);
  pc = pOp->p2 - 1;
  break;
}

/* Opcode:  Return P1 * * * *
**
** Jump to the next instruction after the address in register P1.
** 跳到寄存器P1后面地址的指令
*/
case OP_Return: {           /* in1 */
  pIn1 = &aMem[pOp->p1];
  assert( pIn1->flags & MEM_Int );
  pc = (int)pIn1->u.i;
  break;
}

/* Opcode:  Yield P1 * * * *
**
** Swap the program counter with the value in register P1.
** 交换程序计数器和寄存器P1里的值
*/
case OP_Yield: {            /* in1 */
  int pcDest;
  pIn1 = &aMem[pOp->p1];
  assert( (pIn1->flags & MEM_Dyn)==0 );
  pIn1->flags = MEM_Int;
  pcDest = (int)pIn1->u.i;
  pIn1->u.i = pc;
  REGISTER_TRACE(pOp->p1, pIn1);
  pc = pcDest;
  break;
}

/* Opcode:  HaltIfNull  P1 P2 P3 P4 *
**
** Check the value in register P3.  If it is NULL then Halt using
** parameter P1, P2, and P4 as if this were a Halt instruction.  If the
** value in register P3 is not NULL, then this routine is a no-op.
** 检查寄存器P3里的值,如果是空的,并且有Halt指令,那么Halt指令使用参数P1,P2
** 和P4,如果寄存器P3里的值不是NULL,这个程序就什么都不做.
*/
case OP_HaltIfNull: {      /* in3 */
  pIn3 = &aMem[pOp->p3];
  if( (pIn3->flags & MEM_Null)==0 ) break;
  /* Fall through into OP_Halt */
}

/* Opcode:  Halt P1 P2 * P4 *
**
** Exit immediately.  All open cursors, etc are closed
** automatically.
**
** P1 is the result code returned by sqlite3_exec(), sqlite3_reset(),
** or sqlite3_finalize().  For a normal halt, this should be SQLITE_OK (0).
** For errors, it can be some other value.  If P1!=0 then P2 will determine
** whether or not to rollback the current transaction.  Do not rollback
** if P2==OE_Fail. Do the rollback if P2==OE_Rollback.  If P2==OE_Abort,
** then back out all changes that have occurred during this execution of the
** VDBE, but do not rollback the transaction. 
**
** If P4 is not null then it is an error message string.
**TODO p1 p2 p4值简介
** There is an implied "Halt 0 0 0" instruction inserted at the very end of
** every program.  So a jump past the last instruction of the program
** is the same as executing Halt.
** 立即退出,所有的游标等自动关闭.P1是sqlite3_exec()函数或sqlite3_reset()或
** sqlite3_finalize()的返回值.对于一个平常的结束,P1应该返回SQLITE_OK(0),对于
** 错误,它会是其他值,如果P1!=0然后P2会决定是否要回滚当前事务.P2为OE_Fail时,
** 不会回滚,P2为OE_Rollback时回滚,如果P2为OE_Abort,那么收回所有VDBE执行过程
** 中的变化,但是不会回滚事务.如果P4不是空的,那它就是一个错误信息字符串.这里
** 有一个隐藏的"Halt 0 0 0"指令,他嵌在每一个程序最后.所以一个跳过上一条指令
** 的跳转也是执行Halt.
*/
case OP_Halt: {
  if( pOp->p1==SQLITE_OK && p->pFrame ){
    /* Halt the sub-program. Return control to the parent frame. */
    VdbeFrame *pFrame = p->pFrame;
    p->pFrame = pFrame->pParent;
    p->nFrame--;
    sqlite3VdbeSetChanges(db, p->nChange);
    pc = sqlite3VdbeFrameRestore(pFrame);
    lastRowid = db->lastRowid;
    if( pOp->p2==OE_Ignore ){
      /* Instruction pc is the OP_Program that invoked the sub-program 
      ** currently being halted. If the p2 instruction of this OP_Halt
      ** instruction is set to OE_Ignore, then the sub-program is throwing
      ** an IGNORE exception. In this case jump to the address specified
      ** as the p2 of the calling OP_Program.  
      ** pc指令是用来唤醒子当前被结束的子程序的OP_Program,如果OP_Halt的p2
      ** 指令设置为OE_Ignore,那么子程序会抛出一个IGNORE异常.在这里跳转到
      ** OP_Program调用的p2的地址.
      */
      pc = p->aOp[pc].p2-1;
    }
    aOp = p->aOp;
    aMem = p->aMem;
    break;
  }

  p->rc = pOp->p1;
  p->errorAction = (u8)pOp->p2;
  p->pc = pc;
  if( pOp->p4.z ){
    assert( p->rc!=SQLITE_OK );
    sqlite3SetString(&p->zErrMsg, db, "%s", pOp->p4.z);
    testcase( sqlite3GlobalConfig.xLog!=0 );
    sqlite3_log(pOp->p1, "abort at %d in [%s]: %s", pc, p->zSql, pOp->p4.z);
  }else if( p->rc ){
    testcase( sqlite3GlobalConfig.xLog!=0 );
    sqlite3_log(pOp->p1, "constraint failed at %d in [%s]", pc, p->zSql);
  }
  rc = sqlite3VdbeHalt(p);
  assert( rc==SQLITE_BUSY || rc==SQLITE_OK || rc==SQLITE_ERROR );
  if( rc==SQLITE_BUSY ){
    p->rc = rc = SQLITE_BUSY;
  }else{
    assert( rc==SQLITE_OK || p->rc==SQLITE_CONSTRAINT );
    assert( rc==SQLITE_OK || db->nDeferredCons>0 );
    rc = p->rc ? SQLITE_ERROR : SQLITE_DONE;
  }
  goto vdbe_return;
}

/* Opcode: Integer P1 P2 * * *
**
** The 32-bit integer value P1 is written into register P2.
** 存放32位整型值的寄存器P1写入到寄存器P2里
*/
case OP_Integer: {         /* out2-prerelease */
  pOut->u.i = pOp->p1;
  break;
}

/* Opcode: Int64 * P2 * P4 *
**
** P4 is a pointer to a 64-bit integer value.
** Write that value into register P2.
** P4是一个指向64位整型值的指针,把P4的值写入到P2里
*/
case OP_Int64: {           /* out2-prerelease */
  assert( pOp->p4.pI64!=0 );
  pOut->u.i = *pOp->p4.pI64;
  break;
}

#ifndef SQLITE_OMIT_FLOATING_POINT
/* Opcode: Real * P2 * P4 *
**
** P4 is a pointer to a 64-bit floating point value.
** Write that value into register P2.
** P4是一个指向64位浮点型值的指针,把这个值写入到P2
*/
case OP_Real: {            /* same as TK_FLOAT, out2-prerelease */
  pOut->flags = MEM_Real;
  assert( !sqlite3IsNaN(*pOp->p4.pReal) );
  pOut->r = *pOp->p4.pReal;
  break;
}
#endif

/* Opcode: String8 * P2 * P4 *
**
** P4 points to a nul terminated UTF-8 string. This opcode is transformed 
** into an OP_String before it is executed for the first time.
** P4是一个UTF-8制式的以nul结尾的字符串,这个操作码在第一次被执行前会转换为
** 一个OP_String.
*/
case OP_String8: {         /* same as TK_STRING, out2-prerelease */
  assert( pOp->p4.z!=0 );
  pOp->opcode = OP_String;
  pOp->p1 = sqlite3Strlen30(pOp->p4.z);

#ifndef SQLITE_OMIT_UTF16
  if( encoding!=SQLITE_UTF8 ){
    rc = sqlite3VdbeMemSetStr(pOut, pOp->p4.z, -1, SQLITE_UTF8, SQLITE_STATIC);
    if( rc==SQLITE_TOOBIG ) goto too_big;
    if( SQLITE_OK!=sqlite3VdbeChangeEncoding(pOut, encoding) ) goto no_mem;
    assert( pOut->zMalloc==pOut->z );
    assert( pOut->flags & MEM_Dyn );
    pOut->zMalloc = 0;
    pOut->flags |= MEM_Static;
    pOut->flags &= ~MEM_Dyn;
    if( pOp->p4type==P4_DYNAMIC ){
      sqlite3DbFree(db, pOp->p4.z);
    }
    pOp->p4type = P4_DYNAMIC;
    pOp->p4.z = pOut->z;
    pOp->p1 = pOut->n;
  }
#endif
  if( pOp->p1>db->aLimit[SQLITE_LIMIT_LENGTH] ){
    goto too_big;
  }
  /* Fall through to the next case, OP_String */
}
  
/* Opcode: String P1 P2 * P4 *
**
** The string value P4 of length P1 (bytes) is stored in register P2.
** 
*/
case OP_String: {          /* out2-prerelease */
  assert( pOp->p4.z!=0 );
  pOut->flags = MEM_Str|MEM_Static|MEM_Term;
  pOut->z = pOp->p4.z;
  pOut->n = pOp->p1;
  pOut->enc = encoding;
  UPDATE_MAX_BLOBSIZE(pOut);
  break;
}

/* Opcode: Null * P2 P3 * *
**
** Write a NULL into registers P2.  If P3 greater than P2, then also write
** NULL into register P3 and ever register in between P2 and P3.  If P3
** is less than P2 (typically P3 is zero) then only register P2 is
** set to NULL
** 往P2里写入一个NULL,如果P3比P2大,那么也要往P3里写入NULL,甚至P2P3之间的
** 寄存器也要写入.如果P3小于P2(特别是P3是zero时),那就只把P2设置为NULL.
*/
case OP_Null: {           /* out2-prerelease */
  int cnt;
  cnt = pOp->p3-pOp->p2;
  assert( pOp->p3<=p->nMem );
  pOut->flags = MEM_Null;
  while( cnt>0 ){
    pOut++;
    memAboutToChange(p, pOut);
    VdbeMemRelease(pOut);
    pOut->flags = MEM_Null;
    cnt--;
  }
  break;
}


/* Opcode: Blob P1 P2 * P4
**
** P4 points to a blob of data P1 bytes long.  Store this
** blob in register P2.
** P4指向一个blob数据P1字节的长度。
** 存储这个blob在寄存器P2中。
*/
case OP_Blob: {                /* out2-prerelease */
  assert( pOp->p1 <= SQLITE_MAX_LENGTH );//当p1字节的长度小于SQLITE_MAX_LENGTH时候执行
  sqlite3VdbeMemSetStr(pOut, pOp->p4.z, pOp->p1, 0, 0);//给pOut和p4分配内存空间
  pOut->enc = encoding;					//输出的结果的编码类型
  UPDATE_MAX_BLOBSIZE(pOut);			//更新最大的blob类型的大小
  break;
} 

/* Opcode: Variable P1 P2 * P4 *
**
** Transfer the values of bound parameter P1 into register P2
**
** If the parameter is named, then its name appears in P4 and P3==1.
** The P4 value is used by sqlite3_bind_parameter_name().
** 把P1参数的边界值传递给P2,如果参数命名过了,那么他的名字也要出现在
** P4中,P3为1.P4的值是被sqlite3_bind_parameter_name()使用的.
*/
case OP_Variable: {            /* out2-prerelease */
  Mem *pVar;       /* Value being transferred */

  assert( pOp->p1>0 && pOp->p1<=p->nVar );
  assert( pOp->p4.z==0 || pOp->p4.z==p->azVar[pOp->p1-1] );
  pVar = &p->aVar[pOp->p1 - 1];
  if( sqlite3VdbeMemTooBig(pVar) ){
    goto too_big;
  }
  sqlite3VdbeMemShallowCopy(pOut, pVar, MEM_Static);
  UPDATE_MAX_BLOBSIZE(pOut);
  break;
}

/* Opcode: Move P1 P2 P3 * *
**
** Move the values in register P1..P1+P3-1 over into
** registers P2..P2+P3-1.  Registers P1..P1+P1-1 are
** left holding a NULL.  It is an error for register ranges
** P1..P1+P3-1 and P2..P2+P3-1 to overlap.
** 把P1..P1+P3-1的值转移到P2..P2+P3-1中.P1..P1+P1-1丢弃并设为NULL,
** P1..P1+P3-1到P2..P2+P3-1的值重复是一个错误
*/
case OP_Move: {
  char *zMalloc;   /* Holding variable for allocated memory */
  int n;           /* Number of registers left to copy */
  int p1;          /* Register to copy from */
  int p2;          /* Register to copy to */

  n = pOp->p3;
  p1 = pOp->p1;
  p2 = pOp->p2;
  assert( n>0 && p1>0 && p2>0 );
  assert( p1+n<=p2 || p2+n<=p1 );

  pIn1 = &aMem[p1];
  pOut = &aMem[p2];
  while( n-- ){
    assert( pOut<=&aMem[p->nMem] );
    assert( pIn1<=&aMem[p->nMem] );
    assert( memIsValid(pIn1) );
    memAboutToChange(p, pOut);
    zMalloc = pOut->zMalloc;
    pOut->zMalloc = 0;
    sqlite3VdbeMemMove(pOut, pIn1);
#ifdef SQLITE_DEBUG
    if( pOut->pScopyFrom>=&aMem[p1] && pOut->pScopyFrom<&aMem[p1+pOp->p3] ){
      pOut->pScopyFrom += p1 - pOp->p2;
    }
#endif
    pIn1->zMalloc = zMalloc;
    REGISTER_TRACE(p2++, pOut);
    pIn1++;
    pOut++;
  }
  break;
}

/* Opcode: Copy P1 P2 * * *
**
** Make a copy of register P1 into register P2.
**
** This instruction makes a deep copy of the value.  A duplicate
** is made of any string or blob constant.  See also OP_SCopy.
*/
case OP_Copy: {             /* in1, out2 */
  pIn1 = &aMem[pOp->p1];
  pOut = &aMem[pOp->p2];
  assert( pOut!=pIn1 );
  sqlite3VdbeMemShallowCopy(pOut, pIn1, MEM_Ephem);
  Deephemeralize(pOut);
  REGISTER_TRACE(pOp->p2, pOut);
  break;
}

/* Opcode: SCopy P1 P2 * * *
**
** Make a shallow copy of register P1 into register P2.
**
** This instruction makes a shallow copy of the value.  If the value
** is a string or blob, then the copy is only a pointer to the
** original and hence if the original changes so will the copy.
** Worse, if the original is deallocated, the copy becomes invalid.
** Thus the program must guarantee that the original will not change
** during the lifetime of the copy.  Use OP_Copy to make a complete
** copy.
** 把P1浅拷贝到P2中去,这个指令只对值进行了浅拷贝,如果值为一个字符串或者
** 二进制大文件,那么拷贝只是一个指向它源的指针,最糟的是,如果源被释放了,
** 这个拷贝就变无效了,因此程序必须保证在有拷贝期间源不会改变.使用OP_Copy
** 生成一个完整的拷贝.
*/
case OP_SCopy: {            /* in1, out2 */
  pIn1 = &aMem[pOp->p1];
  pOut = &aMem[pOp->p2];
  assert( pOut!=pIn1 );
  sqlite3VdbeMemShallowCopy(pOut, pIn1, MEM_Ephem);
#ifdef SQLITE_DEBUG
  if( pOut->pScopyFrom==0 ) pOut->pScopyFrom = pIn1;
#endif
  REGISTER_TRACE(pOp->p2, pOut);
  break;
}

/* Opcode: ResultRow P1 P2 * * *
**
** The registers P1 through P1+P2-1 contain a single row of
** results. This opcode causes the sqlite3_step() call to terminate
** with an SQLITE_ROW return code and it sets up the sqlite3_stmt
** structure to provide access to the top P1 values as the result
** row.
** P1寄存器通过P1+P2-1确定结果的一行,这个操作码使得sqlite3_step()调用
** 至结束,返回一个SQLITE_ROW,它还设置sqlite3_stmt结构体能够提供与结果
** 行里的最上面的P1值的交互.
*/
case OP_ResultRow: {
  Mem *pMem;
  int i;
  assert( p->nResColumn==pOp->p2 );
  assert( pOp->p1>0 );
  assert( pOp->p1+pOp->p2<=p->nMem+1 );

  /* If this statement has violated immediate foreign key constraints, do
  ** not return the number of rows modified. And do not RELEASE the statement
  ** transaction. It needs to be rolled back.  
  ** 如果这个声明违反了最近的外键约束,那就不返回修改过的行的行数,也不发布这个声明的事务,它需要被回滚.
  */
  if( SQLITE_OK!=(rc = sqlite3VdbeCheckFk(p, 0)) ){
    assert( db->flags&SQLITE_CountRows );
    assert( p->usesStmtJournal );
    break;
  }

  /*
  ** If the SQLITE_CountRows flag is set in sqlite3.flags mask, then
  ** DML statements invoke this opcode to return the number of rows 
  ** modified to the user. This is the only way that a VM that
  ** opens a statement transaction may invoke this opcode.
  **
  ** In case this is such a statement, close any statement transaction
  ** opened by this VM before returning control to the user. This is to
  ** ensure that statement-transactions are always nested, not overlapping.
  ** If the open statement-transaction is not closed here, then the user
  ** may step another VM that opens its own statement transaction. This
  ** may lead to overlapping statement transactions.
  **
  ** The statement transaction is never a top-level transaction.  Hence
  ** the RELEASE call below can never fail.
  ** 如果SQLITE_CountRows标示设置在sqlite3.flags码中,那么DML声明调用这个操作码,返回
  ** 修改过的行数给用户.这是一个打开一个声明事务可能调用这个操作码的VM的唯一方法.
  */
  assert( p->iStatement==0 || db->flags&SQLITE_CountRows );
  rc = sqlite3VdbeCloseStatement(p, SAVEPOINT_RELEASE);
  if( NEVER(rc!=SQLITE_OK) ){
    break;
  }

  /* Invalidate all ephemeral cursor row caches */
  p->cacheCtr = (p->cacheCtr + 2)|1;

  /* Make sure the results of the current row are \000 terminated
  ** and have an assigned type.  The results are de-ephemeralized as
  ** a side effect.
  ** 确定当前行的结果以\000结束,并且有一个指定的类型,这个结果有副作用
  */
  pMem = p->pResultSet = &aMem[pOp->p1];
  for(i=0; i<pOp->p2; i++){
    assert( memIsValid(&pMem[i]) );
    Deephemeralize(&pMem[i]);
    assert( (pMem[i].flags & MEM_Ephem)==0
            || (pMem[i].flags & (MEM_Str|MEM_Blob))==0 );
    sqlite3VdbeMemNulTerminate(&pMem[i]);
    sqlite3VdbeMemStoreType(&pMem[i]);
    REGISTER_TRACE(pOp->p1+i, &pMem[i]);
  }
  if( db->mallocFailed ) goto no_mem;

  /* Return SQLITE_ROW
  */
  p->pc = pc + 1;
  rc = SQLITE_ROW;
  goto vdbe_return;
}

/* Opcode: Concat P1 P2 P3 * *
**
** Add the text in register P1 onto the end of the text in
** register P2 and store the result in register P3.
** If either the P1 or P2 text are NULL then store NULL in P3.
**
**   P3 = P2 || P1
**
** It is illegal for P1 and P3 to be the same register. Sometimes,
** if P3 is the same register as P2, the implementation is able
** to avoid a memcpy().
** 添加P1中的文本到P2中文本的末尾,把结果放在P3中,如果P1或P2的文本是空的,那就在P3中存放NULL
*/
case OP_Concat: {           /* same as TK_CONCAT, in1, in2, out3 */
  i64 nByte;

  pIn1 = &aMem[pOp->p1];
  pIn2 = &aMem[pOp->p2];
  pOut = &aMem[pOp->p3];
  assert( pIn1!=pOut );
  if( (pIn1->flags | pIn2->flags) & MEM_Null ){
    sqlite3VdbeMemSetNull(pOut);
    break;
  }
  if( ExpandBlob(pIn1) || ExpandBlob(pIn2) ) goto no_mem;
  Stringify(pIn1, encoding);
  Stringify(pIn2, encoding);
  nByte = pIn1->n + pIn2->n;
  if( nByte>db->aLimit[SQLITE_LIMIT_LENGTH] ){
    goto too_big;
  }
  MemSetTypeFlag(pOut, MEM_Str);
  if( sqlite3VdbeMemGrow(pOut, (int)nByte+2, pOut==pIn2) ){
    goto no_mem;
  }
  if( pOut!=pIn2 ){
    memcpy(pOut->z, pIn2->z, pIn2->n);
  }
  memcpy(&pOut->z[pIn2->n], pIn1->z, pIn1->n);
  pOut->z[nByte] = 0;
  pOut->z[nByte+1] = 0;
  pOut->flags |= MEM_Term;
  pOut->n = (int)nByte;
  pOut->enc = encoding;
  UPDATE_MAX_BLOBSIZE(pOut);
  break;
}

/* Opcode: Add P1 P2 P3 * *
**
** Add the value in register P1 to the value in register P2
** and store the result in register P3.
** If either input is NULL, the result is NULL.
** 把P1的值加到P2中,然后存放在P3中,如果有一个输入是空的,那结果就是空的
*/
/* Opcode: Multiply P1 P2 P3 * *
**
**
** Multiply the value in register P1 by the value in register P2
** and store the result in register P3.
** If either input is NULL, the result is NULL.
** P1乘以P2的结果放在P3中,如果输入有一个空的那结果就是空的
*/
/* Opcode: Subtract P1 P2 P3 * *
**
** Subtract the value in register P1 from the value in register P2
** and store the result in register P3.
** If either input is NULL, the result is NULL.
** P3 = P1-P2,如果有一个是NULL,结果就是null
*/
/* Opcode: Divide P1 P2 P3 * *
**
** Divide the value in register P1 by the value in register P2
** and store the result in register P3 (P3=P2/P1). If the value in 
** register P1 is zero, then the result is NULL. If either input is 
** NULL, the result is NULL.
** P3=P1除以P2,如果P1为0,结果就是null.任意输入为NULL,那结果就是NULL
*/
/* Opcode: Remainder P1 P2 P3 * *
**
** Compute the remainder after integer division of the value in
** register P1 by the value in register P2 and store the result in P3. 
** If the value in register P2 is zero the result is NULL.
** If either operand is NULL, the result is NULL.
** 计算P1/P2的余数放在P3中,如果P2的值是0,那结果为NULL,如果任意一个操作数为NULL,那结果也是NULL
** 
*/
case OP_Add:                   /* same as TK_PLUS, in1, in2, out3 */
case OP_Subtract:              /* same as TK_MINUS, in1, in2, out3 */
case OP_Multiply:              /* same as TK_STAR, in1, in2, out3 */
case OP_Divide:                /* same as TK_SLASH, in1, in2, out3 */
case OP_Remainder: {           /* same as TK_REM, in1, in2, out3 */
  int flags;      /* Combined MEM_* flags from both inputs */
  i64 iA;         /* Integer value of left operand */
  i64 iB;         /* Integer value of right operand */
  double rA;      /* Real value of left operand */
  double rB;      /* Real value of right operand */

  pIn1 = &aMem[pOp->p1];
  applyNumericAffinity(pIn1);
  pIn2 = &aMem[pOp->p2];
  applyNumericAffinity(pIn2);
  pOut = &aMem[pOp->p3];
  flags = pIn1->flags | pIn2->flags;
  if( (flags & MEM_Null)!=0 ) goto arithmetic_result_is_null;
  if( (pIn1->flags & pIn2->flags & MEM_Int)==MEM_Int ){
    iA = pIn1->u.i;
    iB = pIn2->u.i;
    switch( pOp->opcode ){
      case OP_Add:       if( sqlite3AddInt64(&iB,iA) ) goto fp_math;  break;
      case OP_Subtract:  if( sqlite3SubInt64(&iB,iA) ) goto fp_math;  break;
      case OP_Multiply:  if( sqlite3MulInt64(&iB,iA) ) goto fp_math;  break;
      case OP_Divide: {
        if( iA==0 ) goto arithmetic_result_is_null;
        if( iA==-1 && iB==SMALLEST_INT64 ) goto fp_math;
        iB /= iA;
        break;
      }
      default: {
        if( iA==0 ) goto arithmetic_result_is_null;
        if( iA==-1 ) iA = 1;
        iB %= iA;
        break;
      }
    }
    pOut->u.i = iB;
    MemSetTypeFlag(pOut, MEM_Int);
  }else{
fp_math:
    rA = sqlite3VdbeRealValue(pIn1);
    rB = sqlite3VdbeRealValue(pIn2);
    switch( pOp->opcode ){
      case OP_Add:         rB += rA;       break;
      case OP_Subtract:    rB -= rA;       break;
      case OP_Multiply:    rB *= rA;       break;
      case OP_Divide: {
        /* (double)0 In case of SQLITE_OMIT_FLOATING_POINT... */
        if( rA==(double)0 ) goto arithmetic_result_is_null;
        rB /= rA;
        break;
      }
      default: {
        iA = (i64)rA;
        iB = (i64)rB;
        if( iA==0 ) goto arithmetic_result_is_null;
        if( iA==-1 ) iA = 1;
        rB = (double)(iB % iA);
        break;
      }
    }
#ifdef SQLITE_OMIT_FLOATING_POINT
    pOut->u.i = rB;
    MemSetTypeFlag(pOut, MEM_Int);
#else
    if( sqlite3IsNaN(rB) ){
      goto arithmetic_result_is_null;
    }
    pOut->r = rB;
    MemSetTypeFlag(pOut, MEM_Real);
    if( (flags & MEM_Real)==0 ){
      sqlite3VdbeIntegerAffinity(pOut);
    }
#endif
  }
  break;

arithmetic_result_is_null:
  sqlite3VdbeMemSetNull(pOut);
  break;
}

/* Opcode: CollSeq P1 * * P4
**
** P4 is a pointer to a CollSeq struct. If the next call to a user function
** or aggregate calls sqlite3GetFuncCollSeq(), this collation sequence will
** be returned. This is used by the built-in min(), max() and nullif()
** functions.
**
** If P1 is not zero, then it is a register that a subsequent min() or
** max() aggregate will set to 1 if the current row is not the minimum or
** maximum.  The P1 register is initialized to 0 by this instruction.
**
** The interface used by the implementation of the aforementioned functions
** to retrieve the collation sequence set by this opcode is not available
** publicly, only to user functions defined in func.c.
** P4是一个指向CollSeq结构体的指针,如果下一个对用户自定义函数的调用或者对
** sqlite3GetFuncCollSeq()的调用,这个对照顺序就会返回,这是被内嵌的min,max,
** nullif函数使用的.
** 
*/
case OP_CollSeq: {
  assert( pOp->p4type==P4_COLLSEQ );
  if( pOp->p1 ){
    sqlite3VdbeMemSetInt64(&aMem[pOp->p1], 0);
  }
  break;
}

/* Opcode: Function P1 P2 P3 P4 P5
**
** Invoke a user function (P4 is a pointer to a Function structure that
** defines the function) with P5 arguments taken from register P2 and
** successors.  The result of the function is stored in register P3.
** Register P3 must not be one of the function inputs.
**
** P1 is a 32-bit bitmask indicating whether or not each argument to the 
** function was determined to be constant at compile time. If the first
** argument was constant then bit 0 of P1 is set. This is used to determine
** whether meta data associated with a user function argument using the
** sqlite3_set_auxdata() API may be safely retained until the next
** invocation of this opcode.
**
** See also: AggStep and AggFinal
** 在P5的参数(从P2和继承者得到的)的参与下,调用一个自定义函数(P4是一个指向一
** 个定义函数的功能结构的指针).这个函数的结果存储在P3中,P3必须不是方法的输入参数.
**
** P1是一个32位的位掩码,用于指示在编译时每一个传递给这个函数的参数是否是常量.如果
** 第一个参数是常量,那么P1的0位被设置.这是用于决定元数据(与一个使用了
** sqlite3_set-auxdata()API的自定义函数的参数有关联)是否被安全的保留着直到这个
** 操作码下一次被调用.
** 还可以看下: AggStep和AggFinal
*/
case OP_Function: {
  int i;
  Mem *pArg;
  sqlite3_context ctx;
  sqlite3_value **apVal;
  int n;

  n = pOp->p5;
  apVal = p->apArg;
  assert( apVal || n==0 );
  assert( pOp->p3>0 && pOp->p3<=p->nMem );
  pOut = &aMem[pOp->p3];
  memAboutToChange(p, pOut);

  assert( n==0 || (pOp->p2>0 && pOp->p2+n<=p->nMem+1) );
  assert( pOp->p3<pOp->p2 || pOp->p3>=pOp->p2+n );
  pArg = &aMem[pOp->p2];
  for(i=0; i<n; i++, pArg++){
    assert( memIsValid(pArg) );
    apVal[i] = pArg;
    Deephemeralize(pArg);
    sqlite3VdbeMemStoreType(pArg);
    REGISTER_TRACE(pOp->p2+i, pArg);
  }

  assert( pOp->p4type==P4_FUNCDEF || pOp->p4type==P4_VDBEFUNC );
  if( pOp->p4type==P4_FUNCDEF ){
    ctx.pFunc = pOp->p4.pFunc;
    ctx.pVdbeFunc = 0;
  }else{
    ctx.pVdbeFunc = (VdbeFunc*)pOp->p4.pVdbeFunc;
    ctx.pFunc = ctx.pVdbeFunc->pFunc;
  }

  ctx.s.flags = MEM_Null;
  ctx.s.db = db;
  ctx.s.xDel = 0;
  ctx.s.zMalloc = 0;

  /* The output cell may already have a buffer allocated. Move
  ** the pointer to ctx.s so in case the user-function can use
  ** the already allocated buffer instead of allocating a new one.
  ** 输出单元可能已经有了一个分配的缓冲区.移动指向这个缓冲区的指针到
  ** ctx.s,用于让自定义函数可以使用这个已经分配了的缓冲区,而不用再分配一个.
  */
  sqlite3VdbeMemMove(&ctx.s, pOut);
  MemSetTypeFlag(&ctx.s, MEM_Null);

  ctx.isError = 0;
  if( ctx.pFunc->flags & SQLITE_FUNC_NEEDCOLL ){
    assert( pOp>aOp );
    assert( pOp[-1].p4type==P4_COLLSEQ );
    assert( pOp[-1].opcode==OP_CollSeq );
    ctx.pColl = pOp[-1].p4.pColl;
  }
  db->lastRowid = lastRowid;
  (*ctx.pFunc->xFunc)(&ctx, n, apVal); /* IMP: R-24505-23230 */
  lastRowid = db->lastRowid;

  /* If any auxiliary data functions have been called by this user function,
  ** immediately call the destructor for any non-static values.
  ** 如果任意的辅助数据函数已经被自定义函数调用过了,立即为非静态值调用析构函数
  */
  if( ctx.pVdbeFunc ){
    sqlite3VdbeDeleteAuxData(ctx.pVdbeFunc, pOp->p1);
    pOp->p4.pVdbeFunc = ctx.pVdbeFunc;
    pOp->p4type = P4_VDBEFUNC;
  }

  if( db->mallocFailed ){
    /* Even though a malloc() has failed, the implementation of the
    ** user function may have called an sqlite3_result_XXX() function
    ** to return a value. The following call releases any resources
    ** associated with such a value.
    ** 即使malloc()函数运行失败,自定义函数可能已经调用了一个sqlite3_result_XXX()
    ** 函数来返回一个值.它接下来的调用释放了与这个值有关的所有资源.
    */
    sqlite3VdbeMemRelease(&ctx.s);
    goto no_mem;
  }

  /* If the function returned an error, throw an exception 如果函数返回一个错误,抛出异常*/
  if( ctx.isError ){
    sqlite3SetString(&p->zErrMsg, db, "%s", sqlite3_value_text(&ctx.s));
    rc = ctx.isError;
  }

  /* Copy the result of the function into register P3 复制函数的结果到P3*/
  sqlite3VdbeChangeEncoding(&ctx.s, encoding);
  sqlite3VdbeMemMove(pOut, &ctx.s);
  if( sqlite3VdbeMemTooBig(pOut) ){
    goto too_big;
  }

#if 0
  /* The app-defined function has done something that as caused this
  ** statement to expire.  (Perhaps the function called sqlite3_exec()
  ** with a CREATE TABLE statement.)
  ** 功能定义函数做了一些事情导致语句不能再运行了(大概这个函数使用CREATE TABLE
  ** 语句调用sqlite3_exec())
  */
  if( p->expired ) rc = SQLITE_ABORT;
#endif

  REGISTER_TRACE(pOp->p3, pOut);
  UPDATE_MAX_BLOBSIZE(pOut);
  break;
}

/* Opcode: BitAnd P1 P2 P3 * *
**
** Take the bit-wise AND of the values in register P1 and P2 and
** store the result in register P3.
** If either input is NULL, the result is NULL.
** 把位操作AND的值放入P1和P2,把结果放入P3,如果每个输入都是空的,
** 结果也是空的
*/
/* Opcode: BitOr P1 P2 P3 * *
**
** Take the bit-wise OR of the values in register P1 and P2 and
** store the result in register P3.
** If either input is NULL, the result is NULL.
** 把位操作OR的值放入P1和P2中,把结果放入P3,如果输入都是空的,那结果也是空的
*/
/* Opcode: ShiftLeft P1 P2 P3 * *
**
** Shift the integer value in register P2 to the left by the
** number of bits specified by the integer in register P1.
** Store the result in register P3.
** If either input is NULL, the result is NULL.
** 通过P1中的整形确定P2中的整形值左移几位.P3存放移动后的结果
** 如果输入都是空的,那结果也是空的
*/
/* Opcode: ShiftRight P1 P2 P3 * *
**
** Shift the integer value in register P2 to the right by the
** number of bits specified by the integer in register P1.
** Store the result in register P3.
** If either input is NULL, the result is NULL.
** 通过P1中的整形确定P2中的整形值右移几位.P3存放移动后的结果
** 如果输入都是空的,那结果也是空的
*/
case OP_BitAnd:                 /* same as TK_BITAND, in1, in2, out3 */
case OP_BitOr:                  /* same as TK_BITOR, in1, in2, out3 */
case OP_ShiftLeft:              /* same as TK_LSHIFT, in1, in2, out3 */
case OP_ShiftRight: {           /* same as TK_RSHIFT, in1, in2, out3 */
  i64 iA;
  u64 uA;
  i64 iB;
  u8 op;

  pIn1 = &aMem[pOp->p1];

  pIn2 = &aMem[pOp->p2];
  pOut = &aMem[pOp->p3];
  if( (pIn1->flags | pIn2->flags) & MEM_Null ){
    sqlite3VdbeMemSetNull(pOut);
    break;
  }
  iA = sqlite3VdbeIntValue(pIn2);
  iB = sqlite3VdbeIntValue(pIn1);
  op = pOp->opcode;
  if( op==OP_BitAnd ){
    iA &= iB;
  }else if( op==OP_BitOr ){
    iA |= iB;
  }else if( iB!=0 ){
    assert( op==OP_ShiftRight || op==OP_ShiftLeft );

    /* If shifting by a negative amount, shift in the other direction */
    if( iB<0 ){
      assert( OP_ShiftRight==OP_ShiftLeft+1 );
      op = 2*OP_ShiftLeft + 1 - op;
      iB = iB>(-64) ? -iB : 64;
    }

    if( iB>=64 ){
      iA = (iA>=0 || op==OP_ShiftLeft) ? 0 : -1;
    }else{
      memcpy(&uA, &iA, sizeof(uA));
      if( op==OP_ShiftLeft ){
        uA <<= iB;
      }else{
        uA >>= iB;
        /* Sign-extend on a right shift of a negative number */
        if( iA<0 ) uA |= ((((u64)0xffffffff)<<32)|0xffffffff) << (64-iB);
      }
      memcpy(&iA, &uA, sizeof(iA));
    }
  }
  pOut->u.i = iA;
  MemSetTypeFlag(pOut, MEM_Int);
  break;
}

/* Opcode: AddImm  P1 P2 * * *
** 
** Add the constant P2 to the value in register P1.
** The result is always an integer.
**
** To force any register to be an integer, just add 0.
** 把p2常量添加给p1寄存器值,结果是一个整数.
** 把每个寄存器的值改为整形的话就添加0.
*/
case OP_AddImm: {            /* in1 */
  pIn1 = &aMem[pOp->p1];
  memAboutToChange(p, pIn1);
  sqlite3VdbeMemIntegerify(pIn1);
  pIn1->u.i += pOp->p2;
  break;
}

/* Opcode: MustBeInt P1 P2 * * *
** 
** Force the value in register P1 to be an integer.  If the value
** in P1 is not an integer and cannot be converted into an integer
** without data loss, then jump immediately to P2, or if P2==0
** raise an SQLITE_MISMATCH exception.
** 强行将P1寄存器内的值转换成整形数据。如果p1里面的值不是整形而且还不能在数据不丢失的情况下转化成整形数据时，
** 立即跳转到p2，或者说如果p2==0，抛出一个SQLITE_MISMATCH异常。
*/
case OP_MustBeInt: {            /* jump, in1 */
  pIn1 = &aMem[pOp->p1];
  applyAffinity(pIn1, SQLITE_AFF_NUMERIC, encoding);
  if( (pIn1->flags & MEM_Int)==0 ){
    if( pOp->p2==0 ){
      rc = SQLITE_MISMATCH;
      goto abort_due_to_error;
    }else{
      pc = pOp->p2 - 1;
    }
  }else{
    MemSetTypeFlag(pIn1, MEM_Int);
  }
  break;
}

#ifndef SQLITE_OMIT_FLOATING_POINT

/* Opcode: RealAffinity P1 * * * *
**
** If register P1 holds an integer convert it to a real value.
** This opcode is used when extracting information from a column that
** has REAL affinity.  Such column values may still be stored as
** integers, for space efficiency, but after extraction we want them
** to have only a real value.
** Author: 吴小全
** 如果P1寄存器里是整型就把它转换为实数,当从这个操作码用于从拥有REAL affinity的一行
** 提取信息.为了空间利用率,这一行的值可能还是存储为整形,但是提取后我们
** 想让它只有一个浮点数.
*/
case OP_RealAffinity: {                  /* in1 */
  pIn1 = &aMem[pOp->p1];
  if( pIn1->flags & MEM_Int ){
    sqlite3VdbeMemRealify(pIn1);
  }
  break;
}
#endif

#ifndef SQLITE_OMIT_CAST
/* Opcode: ToText P1 * * * *
**
** Force the value in register P1 to be text.
** If the value is numeric, convert it to a string using the
** equivalent of printf().  Blob values are unchanged and
** are afterwards simply interpreted as text.
**
** A NULL value is not changed by this routine.  It remains NULL.
** 强制P1里的值为文本，如果这个值是数字类型，将它转换成与printf()等效的字符串。二进制大文件的值是不变的,只是简单地把它转成文本表示.
** 此程序无法改变NULL值,它就是NULL.
*/
case OP_ToText: {                  /* same as TK_TO_TEXT, in1 */
  pIn1 = &aMem[pOp->p1];
  memAboutToChange(p, pIn1);
  if( pIn1->flags & MEM_Null ) break;
  assert( MEM_Str==(MEM_Blob>>3) );
  pIn1->flags |= (pIn1->flags&MEM_Blob)>>3;
  applyAffinity(pIn1, SQLITE_AFF_TEXT, encoding);
  rc = ExpandBlob(pIn1);
  assert( pIn1->flags & MEM_Str || db->mallocFailed );
  pIn1->flags &= ~(MEM_Int|MEM_Real|MEM_Blob|MEM_Zero);
  UPDATE_MAX_BLOBSIZE(pIn1);
  break;
}

/* Opcode: ToBlob P1 * * * *
**
** Force the value in register P1 to be a BLOB.
** If the value is numeric, convert it to a string first.
** Strings are simply reinterpreted as blobs with no change
** to the underlying data.
**
** A NULL value is not changed by this routine.  It remains NULL.
** 强制P1里的值为一个二进制大文件值，如果这个值正好是数字类型的，
** 转换为字符串,字符串是对二进制大文件简单地进行重新解释,它所代表的数据是不变的
** 此程序无法改变NULL值,它就是NULL.
** 刘志齐修改：
** 把寄存器P1中的值强制转换成BLOB类型，
** 如果这个值是一个数字类型，首先把它转换成一个字符串类型。
** 字符串只是对blobs类型的数据仅仅重新解释，并没有改变基础数据。
** 此程序无法改变NULL值,它就是NULL。
*/
case OP_ToBlob: {                  /* same as TK_TO_BLOB, in1 */
  pIn1 = &aMem[pOp->p1];     
  if( pIn1->flags & MEM_Null ) break;
  if( (pIn1->flags & MEM_Blob)==0 ){
    applyAffinity(pIn1, SQLITE_AFF_TEXT, encoding);
    assert( pIn1->flags & MEM_Str || db->mallocFailed );
    MemSetTypeFlag(pIn1, MEM_Blob);  //将Mem中的任何现有的标记来自Mem清除都并且用MEM_Blob替换
  }else{
    pIn1->flags &= ~(MEM_TypeMask&~MEM_Blob);//把MEM_TypeMask的值与MEM_Blob的取非的结果再取非的结果
											 //与pIn1->flags取非的结果赋给pIn1->flags。
  }	
  UPDATE_MAX_BLOBSIZE(pIn1);      //更新最大的blob类型的大小根据pIn1
  break;
}

/* Opcode: ToNumeric P1 * * * *
**
** Force the value in register P1 to be numeric (either an
** integer or a floating-point number.)
** If the value is text or blob, try to convert it to an using the
** equivalent of atoi() or atof() and store 0 if no such conversion 
** is possible.
**
** A NULL value is not changed by this routine.  It remains NULL.
** 强制P1里的值为一个整形数值，如果这个值正好是浮点数，丢掉它的小数部分.
** 值是一个文本或二进制大文件，使用atoi()的等价物尝试先转换为整数,如果不能
** 就存储为0.就存储为0.0.此程序无法改变NULL值,它就是NULL.
*/
case OP_ToNumeric: {                  /* same as TK_TO_NUMERIC, in1 */
  pIn1 = &aMem[pOp->p1];
  sqlite3VdbeMemNumerify(pIn1);
  break;
}
#endif /* SQLITE_OMIT_CAST */

/* Opcode: ToInt P1 * * * *
**
** Force the value in register P1 to be an integer.  If
** The value is currently a real number, drop its fractional part.
** If the value is text or blob, try to convert it to an integer using the
** equivalent of atoi() and store 0 if no such conversion is possible.
**
** A NULL value is not changed by this routine.  It remains NULL.
** 强制P1里的值为一个整形数值，如果这个值正好是浮点数，丢掉它的小数部分.
** 值是一个文本或二进制大文件，使用atoi()的等价物尝试先转换为整数,如果不能
** 就存储为0.就存储为0.0.此程序无法改变NULL值,它就是NULL.
*/
case OP_ToInt: {                  /* same as TK_TO_INT, in1 */
  pIn1 = &aMem[pOp->p1];
  if( (pIn1->flags & MEM_Null)==0 ){
    sqlite3VdbeMemIntegerify(pIn1);
  }
  break;
}

#if !defined(SQLITE_OMIT_CAST) && !defined(SQLITE_OMIT_FLOATING_POINT)
/* Opcode: ToReal P1 * * * *
**
** Force the value in register P1 to be a floating point number.
** If The value is currently an integer, convert it.
** If the value is text or blob, try to convert it to an integer using the
** equivalent of atoi() and store 0.0 if no such conversion is possible.
**
** A NULL value is not changed by this routine.  It remains NULL.
** 强制P1里的值为一个浮点型指针数值，如果这个值正好是整数，转换他，如果这个
** 值是一个文本或二进制大文件，使用atoi()的等价物尝试先转换为整数,如果不能
** 就存储为0.0.此程序无法改变NULL值,它就是NULL.
* /
case OP_ToReal: {                  /* same as TK_TO_REAL, in1 */
  pIn1 = &aMem[pOp->p1];
  memAboutToChange(p, pIn1);
  if( (pIn1->flags & MEM_Null)==0 ){
    sqlite3VdbeMemRealify(pIn1);
  }
  break;
}
#endif /* !defined(SQLITE_OMIT_CAST) && !defined(SQLITE_OMIT_FLOATING_POINT) */

/* Opcode: Lt P1 P2 P3 P4 P5
**
** 比较寄存器中操作码P1和P3的值。如果 P3<P1 则跳转到地址P2。
** Compare the values in register P1 and P3.  If reg(P3)<reg(P1) then
** jump to address P2.
**
** 如果P5的SQLITE_JUMPIFNULL位被重置，同时P1或P3为null，则进行跳转。
** 如果SQLITE_JUMPIFNULL位被清除，同时P1和P3两者中任何一个是null，那么操作失败。
** If the SQLITE_JUMPIFNULL bit of P5 is set and either reg(P1) or
** reg(P3) is NULL then take the jump.  If the SQLITE_JUMPIFNULL
** bit is clear then fall through if either operand is NULL.
**
** P5的SQLITE_AFF_MASK部分必须是一个相关的字符 —— 例如SQLITE_AFF_TEXT、SQLITE_AFF_INTEGER等等。
** 要根据这种相关性原则来比较两个输入值，这一要求是强制的。如果SQLITE_AFF_MASK是0x00(十六进制0)，
** 那么数值相关性就起作用了。注意，对输入值做相关性转换后会被存储回输入寄存器P1和P3中。
** 所以这个操作码能够导致寄存器P1和P3中对应值的永久改变。
** The SQLITE_AFF_MASK portion of P5 must be an affinity character -
** SQLITE_AFF_TEXT, SQLITE_AFF_INTEGER, and so forth. An attempt is made
** to coerce both inputs according to this affinity before the
** comparison is made. If the SQLITE_AFF_MASK is 0x00, then numeric
** affinity is used. Note that the affinity conversions are stored
** back into the input registers P1 and P3.  So this opcode can cause
** persistent changes to registers P1 and P3.
**
** 一旦任意一种转换发生，同时两个值都不为空，则进行值的比较。
** 如果两个值的字段类型都是blob(二进制大对象，是一个可以存储二进制文件的容器。在计算机中，
** blob常常是数据库中用来存储二进制文件的字段类型)，则调用memcmp()函数来判定比较结果。
** 如果两个值的文本类型，就需要调用P4中适当的排序函数来进行比较。如果没有指定P4，
** 那么memcmp()函数就用于比较文本字符串。如果两个值都是整型,那就使用一个数值型比较方法。
** 如果是两个不同类型的值,比较规则就是：数字小于字符串，字符串小于blob。
** Once any conversions have taken place, and neither value is NULL,
** the values are compared. If both values are blobs then memcmp() is
** used to determine the results of the comparison.  If both values
** are text, then the appropriate collating function specified in
** P4 is  used to do the comparison.  If P4 is not specified then
** memcmp() is used to compare text string.  If both values are
** numeric, then a numeric comparison is used. If the two values
** are of different types, then numbers are considered less than
** strings and strings are considered less than blobs.
**
** 如果P5的SQLITE_STOREP2位被重置，则不会产生跳转。而是将一个布尔类型的结果(值是0或1，或NULL)
** 存至寄存器P2中。
** If the SQLITE_STOREP2 bit of P5 is set, then do not jump.  Instead,
** store a boolean result (either 0, or 1, or NULL) in register P2.
*/

/* Opcode: Ne P1 P2 P3 P4 P5
**
** 这部分操作码的作用与Lt操作码类似，唯一不同的是，当寄存器P1和寄存器P3中的操作对象不相等时，
** 跳转操作会被执行。查看Lt操作码可以获得额外的信息。
** This works just like the Lt opcode except that the jump is taken if
** the operands in registers P1 and P3 are not equal.  See the Lt opcode for
** additional information.
**
** 如果SQLITE_NULLEQ的值被赋予P5，则比较的结果要么是真要么假，不可能是NULL。
** 如果两个操作数都是null，那么比较的结果是假。如果任意一个操作数为空，则结果是真。
** 如果操作数都不是null，同时P5中的SQLITE_NULLEQ标记为省略，那么它的结果与上面一样也是真。
** If SQLITE_NULLEQ is set in P5 then the result of comparison is always either
** true or false and is never NULL.  If both operands are NULL then the result
** of comparison is false.  If either operand is NULL then the result is true.
** If neither operand is NULL the result is the same as it would be if
** the SQLITE_NULLEQ flag were omitted from P5.
*/
/* Opcode: Eq P1 P2 P3 P4 P5
**
** 这部分操作码的作用与Lt操作码类似，唯一不同的是，当寄存器P1和寄存器P3中的操作对象不相等时，
** 跳转操作会被执行。查看Lt操作码可以获得额外的信息。
** This works just like the Lt opcode except that the jump is taken if
** the operands in registers P1 and P3 are equal.
** See the Lt opcode for additional information.
**
** 如果SQLITE_NULLEQ的值被赋予P5，那么比较产生的结果要么是真要么假，不可能是NULL。
** 如果两个操作数都是null，那么比较的结果是真。如果任意一个操作数为空，则结果是假。
** 如果操作数都不是null，同时P5中的SQLITE_NULLEQ标记为省略，那么它的结果与上面一样也是假。
** If SQLITE_NULLEQ is set in P5 then the result of comparison is always either
** true or false and is never NULL.  If both operands are NULL then the result
** of comparison is true.  If either operand is NULL then the result is false.
** If neither operand is NULL the result is the same as it would be if
** the SQLITE_NULLEQ flag were omitted from P5.
*/
/* Opcode: Le P1 P2 P3 P4 P5
**
** 这部分操作码的作用与Lt操作码类似，唯一不同的是，当寄存器P3中内容小于或等于寄存器P1中的内容时，
** 跳转操作会被执行。查看Lt操作码可以获得更多信息。
** This works just like the Lt opcode except that the jump is taken if
** the content of register P3 is less than or equal to the content of
** register P1.  See the Lt opcode for additional information.
*/
/* Opcode: Gt P1 P2 P3 P4 P5
**
** 这部分操作码的作用与Lt操作码类似，唯一不同的是，当寄存器P3中内容大于寄存器P1中的内容时，
** 跳转操作会被执行。查看Lt操作码可以获得更多信息。
** This works just like the Lt opcode except that the jump is taken if
** the content of register P3 is greater than the content of
** register P1.  See the Lt opcode for additional information.
*/
/* Opcode: Ge P1 P2 P3 P4 P5
**
** 这部分操作码的作用与Lt操作码类似，唯一不同的是，当寄存器P3中内容大于或等于寄存器P1中的内容时，
** 跳转操作会被执行。查看Lt操作码可以获得更多信息。
** This works just like the Lt opcode except that the jump is taken if
** the content of register P3 is greater than or equal to the content of
** register P1.  See the Lt opcode for additional information.
*/
case OP_Eq:               /* same as TK_EQ, jump, in1, in3 */
case OP_Ne:               /* same as TK_NE, jump, in1, in3 */
case OP_Lt:               /* same as TK_LT, jump, in1, in3 */
case OP_Le:               /* same as TK_LE, jump, in1, in3 */
case OP_Gt:               /* same as TK_GT, jump, in1, in3 */
case OP_Ge: {             /* same as TK_GE, jump, in1, in3 */
  int res;            /* Result of the comparison of pIn1 against pIn3
                      ** 存放输入操作对象pIn1和pIn3的比较结果
                      */
  char affinity;      /* Affinity to use for comparison
                      ** 用于比较的相关性字符
                      */
  u16 flags1;         /* Copy of initial value of pIn1->flags
                      ** u16数据类型在“sqliteInt.h”文件有定义，代表“2-byte unsigned integer”
                      */
  u16 flags3;         /* Copy of initial value of pIn3->flags */

  pIn1 = &aMem[pOp->p1];
  pIn3 = &aMem[pOp->p3];
  flags1 = pIn1->flags;
  flags3 = pIn3->flags;
  if( (flags1 | flags3)&MEM_Null ){
    /* One or both operands are NULL */
    if( pOp->p5 & SQLITE_NULLEQ ){
      /* If SQLITE_NULLEQ is set (which will only happen if the operator is
      ** OP_Eq or OP_Ne) then take the jump or not depending on whether
      ** or not both operands are null.
      ** 如果SQLITE_NULLEQ已经被设置(如果操作码是OP_Eq或OP_Ne时才会生效)，
      ** 那么进行跳转，或者不取决于这两个操作数是否为空。
      */
      assert( pOp->opcode==OP_Eq || pOp->opcode==OP_Ne );
      res = (flags1 & flags3 & MEM_Null)==0;
    }else{
      /* SQLITE_NULLEQ is clear and at least one operand is NULL,
      ** then the result is always NULL.
      ** The jump is taken if the SQLITE_JUMPIFNULL bit is set.
      ** SQLITE_NULLEQ是空的，同时至少有一个操作数是null，那么结果通常是null。
      ** 如果SQLITE_JUMPIFNULL为被重置，执行跳转。
      */
      if( pOp->p5 & SQLITE_STOREP2 ){
        pOut = &aMem[pOp->p2];
        MemSetTypeFlag(pOut, MEM_Null);
        REGISTER_TRACE(pOp->p2, pOut);
      }else if( pOp->p5 & SQLITE_JUMPIFNULL ){
        pc = pOp->p2-1;
      }
      break;
    }
  }else{
    /* Neither operand is NULL.  Do a comparison.
    ** 两个操作数都不为null，做比较。
    */
    affinity = pOp->p5 & SQLITE_AFF_MASK;
    if( affinity ){
      applyAffinity(pIn1, affinity, encoding);
      applyAffinity(pIn3, affinity, encoding);
      if( db->mallocFailed ) goto no_mem;
    }

    assert( pOp->p4type==P4_COLLSEQ || pOp->p4.pColl==0 );
    ExpandBlob(pIn1);
    ExpandBlob(pIn3);
    res = sqlite3MemCompare(pIn3, pIn1, pOp->p4.pColl);
  }
  switch( pOp->opcode ){
    case OP_Eq:    res = res==0;     break;
    case OP_Ne:    res = res!=0;     break;
    case OP_Lt:    res = res<0;      break;
    case OP_Le:    res = res<=0;     break;
    case OP_Gt:    res = res>0;      break;
    default:       res = res>=0;     break;
  }

  if( pOp->p5 & SQLITE_STOREP2 ){
    pOut = &aMem[pOp->p2];
    memAboutToChange(p, pOut);
    MemSetTypeFlag(pOut, MEM_Int);
    pOut->u.i = res;
    REGISTER_TRACE(pOp->p2, pOut);
  }else if( res ){
    pc = pOp->p2-1;
  }

  /* Undo any changes made by applyAffinity() to the input registers.
  ** 撤销applyAffinity()函数对输入寄存器所做的所有修改。
  */
  pIn1->flags = (pIn1->flags&~MEM_TypeMask) | (flags1&MEM_TypeMask);
  pIn3->flags = (pIn3->flags&~MEM_TypeMask) | (flags3&MEM_TypeMask);
  break;
}

/* Opcode: Permutation * * * P4 *
**
** Set the permutation used by the OP_Compare operator to be the array
** of integers in P4.
** 将P4中操作码OP_Compare使用的数组设置为整型数组。
**
** The permutation is only valid until the next OP_Permutation, OP_Compare,
** OP_Halt, or OP_ResultRow.  Typically the OP_Permutation should occur
** immediately prior to the OP_Compare.
** 这个数组是唯一一个在操作码OP_Permutation OP_Compare,OP_Halt或OP_ResultRow中都有效的值。
** 通常OP_Permutation操作发生在OP_Compare之前。
*/
case OP_Permutation: {
  assert( pOp->p4type==P4_INTARRAY );
  assert( pOp->p4.ai );
  aPermute = pOp->p4.ai;
  break;
}

/* Opcode: Compare P1 P2 P3 P4 *
**
** Compare two vectors of registers in reg(P1)..reg(P1+P3-1) (call this
** vector "A") and in reg(P2)..reg(P2+P3-1) ("B").  Save the result of
** the comparison for use by the next OP_Jump instruct.
** 比较两个向量寄存器，寄存器(P1). .寄存器(P1+P3-1)(称这个向量为“A”)，以及寄存器(P2). .寄存器(P2+P3-1)(“B”)。
** 比较的结果将被保存，并用于下一个操作码OP_Jump。
**
** P4 is a KeyInfo structure that defines collating sequences and sort
** orders for the comparison.  The permutation applies to registers
** only.  The KeyInfo elements are used sequentially.
** P4是一个KeyInfo类型的结构体，它定义了排序序列和排序方式。这个序列只应用于寄存器。
** 这些KeyInfo类型的元素按顺序使用。
** The comparison is a sort comparison, so NULLs compare equal,
** NULLs are less than numbers, numbers are less than strings,
** and strings are less than blobs.
** 这个比较是一种分类比较，所以null等于null，nulls小于数字，数字小于字符串，字符串小于blob。
*/
case OP_Compare: {
  int n;
  int i;
  int p1;
  int p2;
  const KeyInfo *pKeyInfo;
  int idx;
  CollSeq *pColl;    /* Collating sequence to use on this term */
  int bRev;          /* True for DESCENDING sort order */

  n = pOp->p3;
  pKeyInfo = pOp->p4.pKeyInfo;
  assert( n>0 );
  assert( pKeyInfo!=0 );
  p1 = pOp->p1;
  p2 = pOp->p2;
#if SQLITE_DEBUG
  if( aPermute ){
    int k, mx = 0;
    for(k=0; k<n; k++) if( aPermute[k]>mx ) mx = aPermute[k];
    assert( p1>0 && p1+mx<=p->nMem+1 );
    assert( p2>0 && p2+mx<=p->nMem+1 );
  }else{
    assert( p1>0 && p1+n<=p->nMem+1 );
    assert( p2>0 && p2+n<=p->nMem+1 );
  }
#endif /* SQLITE_DEBUG */
  for(i=0; i<n; i++){
    idx = aPermute ? aPermute[i] : i;
    assert( memIsValid(&aMem[p1+idx]) );
    assert( memIsValid(&aMem[p2+idx]) );
    REGISTER_TRACE(p1+idx, &aMem[p1+idx]);
    REGISTER_TRACE(p2+idx, &aMem[p2+idx]);
    assert( i<pKeyInfo->nField );
    pColl = pKeyInfo->aColl[i];
    bRev = pKeyInfo->aSortOrder[i];
    iCompare = sqlite3MemCompare(&aMem[p1+idx], &aMem[p2+idx], pColl);
    if( iCompare ){
      if( bRev ) iCompare = -iCompare;
      break;
    }
  }
  aPermute = 0;
  break;
}

/* Opcode: Jump P1 P2 P3 * *
**
** Jump to the instruction at address P1, P2, or P3 depending on whether
** in the most recent OP_Compare instruction the P1 vector was less than
** equal to, or greater than the P2 vector, respectively.
** 分别跳转到的指令地址P1,P2,P3，这取决于在最近的OP_Compare指令中P1向量是小于等于P2向量，还是大于P2向量。
*/
case OP_Jump: {             /* jump */
  if( iCompare<0 ){
    pc = pOp->p1 - 1;
  }else if( iCompare==0 ){
    pc = pOp->p2 - 1;
  }else{
    pc = pOp->p3 - 1;
  }
  break;
}

/* Opcode: And P1 P2 P3 * *
**
** Take the logical AND of the values in registers P1 and P2 and
** write the result into register P3.
** 逻辑操作码AND中的值取自寄存器P1和P2，最后的结果写入寄存器P3。
**
** If either P1 or P2 is 0 (false) then the result is 0 even if
** the other input is NULL.  A NULL and true or two NULLs give
** a NULL output.
** 如果P1和P2有一个是0(假)，那么结果是0，即使另一个输入为空结果也是0。
** 一个NULL和一个真，或者两个NULL，都输出NULL。
*/
/* Opcode: Or P1 P2 P3 * *
**
** Take the logical OR of the values in register P1 and P2 and
** store the answer in register P3.
** 逻辑操作码OR中的值取自寄存器P1和P2，最后的结果写入寄存器P3。
**
** If either P1 or P2 is nonzero (true) then the result is 1 (true)
** even if the other input is NULL.  A NULL and false or two NULLs
** give a NULL output.
** 如果P1和P2有一个非0(真)，那么结果是1(真)，即使另一个输入为空。
** 一个NULL和一个假，或者两个NULL，都输出NULL。
*/
case OP_And:              /* same as TK_AND, in1, in2, out3 */
case OP_Or: {             /* same as TK_OR, in1, in2, out3 */
  int v1;    /* Left operand:  0==FALSE, 1==TRUE, 2==UNKNOWN or NULL */
  int v2;    /* Right operand: 0==FALSE, 1==TRUE, 2==UNKNOWN or NULL */

  pIn1 = &aMem[pOp->p1];
  if( pIn1->flags & MEM_Null ){
    v1 = 2;
  }else{
    v1 = sqlite3VdbeIntValue(pIn1)!=0;
  }
  pIn2 = &aMem[pOp->p2];
  if( pIn2->flags & MEM_Null ){
    v2 = 2;
  }else{
    v2 = sqlite3VdbeIntValue(pIn2)!=0;
  }
  if( pOp->opcode==OP_And ){
    static const unsigned char and_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };
    v1 = and_logic[v1*3+v2];
  }else{
    static const unsigned char or_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };
    v1 = or_logic[v1*3+v2];
  }
  pOut = &aMem[pOp->p3];
  if( v1==2 ){
    MemSetTypeFlag(pOut, MEM_Null);
  }else{
    pOut->u.i = v1;
    MemSetTypeFlag(pOut, MEM_Int);
  }
  break;
}

/* Opcode: Not P1 P2 * * *
**
** Interpret the value in register P1 as a boolean value.  Store the
** boolean complement in register P2.  If the value in register P1 is
** NULL, then a NULL is stored in P2.
** 将寄存器中P1的值解释为一个布尔值。将这个布尔值取反后存储在寄存器P2中。
** 如果寄存器P1中的值是NULL,则把NULL存储在P2。
*/
case OP_Not: {                /* same as TK_NOT, in1, out2 */
  pIn1 = &aMem[pOp->p1];
  pOut = &aMem[pOp->p2];
  if( pIn1->flags & MEM_Null ){
    sqlite3VdbeMemSetNull(pOut);
  }else{
    sqlite3VdbeMemSetInt64(pOut, !sqlite3VdbeIntValue(pIn1));
  }
  break;
}

/* Opcode: BitNot P1 P2 * * *
**
** Interpret the content of register P1 as an integer.  Store the
** ones-complement of the P1 value into register P2.  If P1 holds
** a NULL then store a NULL in P2.
** 将寄存器P1的内容转换为一个整数。将P1的值按位取反后值存储到寄存器P2内。
** 如果寄存器P1中的值是NULL,则把NULL存储在P2。
*/
case OP_BitNot: {             /* same as TK_BITNOT, in1, out2 */
  pIn1 = &aMem[pOp->p1];
  pOut = &aMem[pOp->p2];
  if( pIn1->flags & MEM_Null ){
    sqlite3VdbeMemSetNull(pOut);
  }else{
    sqlite3VdbeMemSetInt64(pOut, ~sqlite3VdbeIntValue(pIn1));
  }
  break;
}

/* Opcode: Once P1 P2 * * *
**
** Check if OP_Once flag P1 is set. If so, jump to instruction P2. Otherwise,
** set the flag and fall through to the next instruction.
** 如果P1中flag已经被赋值，跳转到指令P2。否则，设置flag的值，执行下一个指令。
**
** See also: JumpOnce
*/
case OP_Once: {             /* jump */
  assert( pOp->p1<p->nOnceFlag );
  if( p->aOnceFlag[pOp->p1] ){
    pc = pOp->p2-1;
  }else{
    p->aOnceFlag[pOp->p1] = 1;
  }
  break;
}

/* Opcode: If P1 P2 P3 * *
**
** Jump to P2 if the value in register P1 is true.  The value
** is considered true if it is numeric and non-zero.  If the value
** in P1 is NULL tAR-hen take the jump if P3 is non-zero.
** 如果寄存器P1的值是真就跳转到P2。如果P1的值是数字和非零值，就会被认定为真。
** 如果P1中的值是NULL，同时P3非零，就会跳转到P2。
*/
/* Opcode: IfNot P1 P2 P3 * *
**
** Jump to P2 if the value in register P1 is False.  The value
** is considered false if it has a numeric value of zero.  If the value
** in P1 is NULL then take the jump if P3 is zero.
** 如果寄存器P1的值是假就跳转到P2。如果P1的值是零，就会被认定为假。
** 如果P1中的值是NULL，同时P3非零，就会跳转到P2。
*/
case OP_If:                 /* jump, in1 */
case OP_IfNot: {            /* jump, in1 */
  int c;
  pIn1 = &aMem[pOp->p1];
  if( pIn1->flags & MEM_Null ){
    c = pOp->p3;
  }else{
#ifdef SQLITE_OMIT_FLOATING_POINT
    c = sqlite3VdbeIntValue(pIn1)!=0;
#else
    c = sqlite3VdbeRealValue(pIn1)!=0.0;
#endif
    if( pOp->opcode==OP_IfNot ) c = !c;
  }
  if( c ){
    pc = pOp->p2-1;
  }
  break;
}

/* Opcode: IsNull P1 P2 * * *
**
** Jump to P2 if the value in register P1 is NULL.
** 如果P1中的值是NULL则跳转到P2。
*/
case OP_IsNull: {            /* same as TK_ISNULL, jump, in1 */
  pIn1 = &aMem[pOp->p1];
  if( (pIn1->flags & MEM_Null)!=0 ){
    pc = pOp->p2 - 1;
  }
  break;
}

/* Opcode: NotNull P1 P2 * * *
**
** Jump to P2 if the value in register P1 is not NULL.
** 如果P1中的值是不是NULL则跳转到P2。
*/
case OP_NotNull: {            /* same as TK_NOTNULL, jump, in1 */
  pIn1 = &aMem[pOp->p1];
  if( (pIn1->flags & MEM_Null)==0 ){
    pc = pOp->p2 - 1;
  }
  break;
}

/* Opcode: Column P1 P2 P3 P4 P5
**
** Interpret the data that cursor P1 points to as a structure built using
** the MakeRecord instruction.  (See the MakeRecord opcode for additional
** information about the format of the data.)  Extract the P2-th column
** from this record.  If there are less that (P2+1)
** values in the record, extract a NULL.
** 将指针P1指向的数据转换为可以被MakeRecord指令使用的结构类型。
** (参见MakeRecord操作码可以获得更多有关这个数据格式的信息。)
** 从记录中提取第P2列的数据。如果记录中的数据少于(P2+1)中的值，就提取一个NULL值。
**
** The value extracted is stored in register P3.
** 这个提取的值被存储在寄存器P3中。
**
** If the column contains fewer than P2 fields, then extract a NULL.  Or,
** if the P4 argument is a P4_MEM use the value of the P4 argument as
** the result.
** 如果存取数据的列少于P2中的字段，则提取一个NULL值。如果参数P4的值是P4_MEM
** (P4定义在vdbe.c文件里“P4 is a pointer to a Mem* structure”)，就使用参数P4的值作为结果。
**
** If the OPFLAG_CLEARCACHE bit is set on P5 and P1 is a pseudo-table cursor,
** then the cache of the cursor is reset prior to extracting the column.
** The first OP_Column against a pseudo-table after the value of the content
** register has changed should have this bit set.
** 如果伪表指针位OPFLAG_CLEARCACHE被设置在P5和P1中，那么在提取列内数据之前，要重置这个指针所指的缓存。
** 内容寄存器的值改变之后，第一个针对伪表的OP_Column操作码应该拥有这个二进制位。
**
** If the OPFLAG_LENGTHARG and OPFLAG_TYPEOFARG bits are set on P5 when
** the result is guaranteed to only be used as the argument of a length()
** or typeof() function, respectively.  The loading of large blobs can be
** skipped for length() and all content loading can be skipped for typeof().
** 如果二进制位OPFLAG_LENGTHARG和OPFLAG_TYPEOFARG都设置在P5中，要保证它们只能作为函数length()
** 和函数typeof()的参数。函数length()可以忽略正在加载的二进制大对象以及所有正在加载的内容。
*/
case OP_Column: {
  u32 payloadSize;   /* Number of bytes in the record */
  i64 payloadSize64; /* Number of bytes in the record */
  int p1;            /* P1 value of the opcode */
  int p2;            /* column number to retrieve
                     ** 用于检索的列号
                     */
  VdbeCursor *pC;    /* The VDBE cursor */
  char *zRec;        /* Pointer to complete record-data
                     ** 指向完整数据的指针
                     */
  BtCursor *pCrsr;   /* The BTree cursor */
  u32 *aType;        /* aType[i] holds the numeric type of the i-th column
                     ** aType[i]存储了第i列的数值类型
                     */
  u32 *aOffset;      /* aOffset[i] is offset to start of data for i-th column
                     ** 
                     */
  int nField;        /* number of fields in the record
                     ** 记录中的字段个数
                     */
  int len;           /* The length of the serialized data for the column
                     ** 序列化数据列的长度
                     */
  int i;             /* Loop counter */
  char *zData;       /* Part of the record being decoded */
  Mem *pDest;        /* Where to write the extracted value
                     ** 被提取的值卸载*pDest所指的内存位置
                     */
  Mem sMem;          /* For storing the record being decoded
                     ** 说sMem用于存贮正在被解码的数据
                     */
  u8 *zIdx;          /* Index into header
                     ** 索引头
                     */
  u8 *zEndHdr;       /* Pointer to first byte after the header
                     ** *zEndHdr指向索引头后的第一个字节
                     */
  u32 offset;        /* Offset into the data */
  u32 szField;       /* Number of bytes in the content of a field
                     ** 
                     */
  int szHdr;         /* Size of the header size field at start of record */
  int avail;         /* Number of bytes of available data */
  u32 t;             /* A type code from the record header */
  Mem *pReg;         /* PseudoTable input register */


  p1 = pOp->p1;
  p2 = pOp->p2;
  pC = 0;
  memset(&sMem, 0, sizeof(sMem));
  assert( p1<p->nCursor );
  assert( pOp->p3>0 && pOp->p3<=p->nMem );
  pDest = &aMem[pOp->p3];
  memAboutToChange(p, pDest);
  zRec = 0;

  /* This block sets the variable payloadSize to be the total number of
  ** bytes in the record.
  ** 这部分代码将变量的有效长度设置为记录的总的字节数量，换句话说就是，记录有多长，这个变量就有长。
  **
  ** zRec is set to be the complete text of the record if it is available.
  ** The complete record text is always available for pseudo-tables
  ** If the record is stored in a cursor, the complete record text
  ** might be available in the  pC->aRow cache.  Or it might not be.
  ** If the data is unavailable,  zRec is set to NULL.
  ** 如果zRec可用，将完整的记录文本赋值给zRec。完整的记录文本总是可以用于伪表。
  ** 如果记录存储在指针里，整个记录文本在pC->aRow缓存中可能是可用的，也可能不是。
  ** 如果数据不可用时，zRec被设置为NULL。
  **
  ** We also compute the number of columns in the record.  For cursors,
  ** the number of columns is stored in the VdbeCursor.nField element.
  ** 我们还要计算记录中有多少列。列数存储在VdbeCursor.nField字段上。
  */
  pC = p->apCsr[p1];
  assert( pC!=0 );
#ifndef SQLITE_OMIT_VIRTUALTABLE
  assert( pC->pVtabCursor==0 );
#endif
  pCrsr = pC->pCursor;
  if( pCrsr!=0 ){
    /* The record is stored in a B-Tree */
    rc = sqlite3VdbeCursorMoveto(pC);
    if( rc ) goto abort_due_to_error;
    if( pC->nullRow ){
      payloadSize = 0;
    }else if( pC->cacheStatus==p->cacheCtr ){
      payloadSize = pC->payloadSize;
      zRec = (char*)pC->aRow;
    }else if( pC->isIndex ){
      assert( sqlite3BtreeCursorIsValid(pCrsr) );
      VVA_ONLY(rc =) sqlite3BtreeKeySize(pCrsr, &payloadSize64);
      assert( rc==SQLITE_OK );   /* True because of CursorMoveto() call above */
      /* sqlite3BtreeParseCellPtr() uses getVarint32() to extract the
      ** payload size, so it is impossible for payloadSize64 to be
      ** larger than 32 bits.
      ** 函数sqlite3BtreeParseCellPtr()使用函数getVarint32()提取有效的文本大小，因此
      ** 有效长度可能是64位大小，要比32位大。
      */
      assert( (payloadSize64 & SQLITE_MAX_U32)==(u64)payloadSize64 );
      payloadSize = (u32)payloadSize64;
    }else{
      assert( sqlite3BtreeCursorIsValid(pCrsr) );
      VVA_ONLY(rc =) sqlite3BtreeDataSize(pCrsr, &payloadSize);
      assert( rc==SQLITE_OK );   /* DataSize() cannot fail */
    }
  }else if( ALWAYS(pC->pseudoTableReg>0) ){
    pReg = &aMem[pC->pseudoTableReg];
    assert( pReg->flags & MEM_Blob );
    assert( memIsValid(pReg) );
    payloadSize = pReg->n;
    zRec = pReg->z;
    pC->cacheStatus = (pOp->p5&OPFLAG_CLEARCACHE) ? CACHE_STALE : p->cacheCtr;
    assert( payloadSize==0 || zRec!=0 );
  }else{
    /* Consider the row to be NULL */
    payloadSize = 0;
  }

  /* If payloadSize is 0, then just store a NULL.  This can happen because of
  ** nullRow or because of a corrupt database. 
  ** 如果有效长度是0，就存储一个NULL值。这种情况发生在数据库没有数据行，或数据库损坏的情况下。
  */
  if( payloadSize==0 ){
    MemSetTypeFlag(pDest, MEM_Null);
    goto op_column_out;
  }
  assert( db->aLimit[SQLITE_LIMIT_LENGTH]>=0 );
  if( payloadSize > (u32)db->aLimit[SQLITE_LIMIT_LENGTH] ){
    goto too_big;
  }

  nField = pC->nField;
  assert( p2<nField );

  /* Read and parse the table header.  Store the results of the parse
  ** into the record header cache fields of the cursor.
  ** 读取、解析表头。将解析的结果存储到指针所指的缓存记录的头部。
  */
  aType = pC->aType;
  if( pC->cacheStatus==p->cacheCtr ){
    aOffset = pC->aOffset;
  }else{
    assert(aType);
    avail = 0;
    pC->aOffset = aOffset = &aType[nField];
    pC->payloadSize = payloadSize;
    pC->cacheStatus = p->cacheCtr;

    /* Figure out how many bytes are in the header
    ** 计算
    */
    if( zRec ){
      zData = zRec;
    }else{
      if( pC->isIndex ){
        zData = (char*)sqlite3BtreeKeyFetch(pCrsr, &avail);
      }else{
        zData = (char*)sqlite3BtreeDataFetch(pCrsr, &avail);
      }
      /* If KeyFetch()/DataFetch() managed to get the entire payload,
      ** save the payload in the pC->aRow cache.  That will save us from
      ** having to make additional calls to fetch the content portion of
      ** the record.
      ** 如果函数KeyFetch()或ataFetch()能够得到全部的有效长度，将这个有效长度存储在pC->aRow缓存中。
      ** 这能够让我们不必额外调用函数来获取所需要的记录内容。
      */
      assert( avail>=0 );
      if( payloadSize <= (u32)avail ){
        zRec = zData;
        pC->aRow = (u8*)zData;
      }else{
        pC->aRow = 0;
      }
    }
    /* The following assert is true in all cases except when
    ** the database file has been corrupted externally.
    **    assert( zRec!=0 || avail>=payloadSize || avail>=9 );
    ** 除非数据库被损坏，否则这个assert语句的结果在任何情况下都是真。
    */
    szHdr = getVarint32((u8*)zData, offset);

    /* Make sure a corrupt database has not given us an oversize header.
    ** Do this now to avoid an oversize memory allocation.
    ** 确保损坏的数据库没有还没有生成超过长度限制的头文件。
    ** 现在这样做(下面的代码)能够避免超额的内存分配。
    **
    ** Type entries can be between 1 and 5 bytes each.  But 4 and 5 byte
    ** types use so much data space that there can only be 4096 and 32 of
    ** them, respectively.  So the maximum header length results from a
    ** 3-byte type for each of the maximum of 32768 columns plus three
    ** extra bytes for the header length itself.  32768*3 + 3 = 98307.
    ** 类型的总条目可以介于1到5个字节。但是4和5字节类型使用如此多的数据空间,他们是32位并且每行有4096列
    ** @author wxq
    ** 所以，最大的头长度结果是3-字节类型，每行最大有32768列加上每个头自己额外占用字节，结果就是32768*3 + 3 = 98307
    **
    */
    if( offset > 98307 ){
      rc = SQLITE_CORRUPT_BKPT;
      goto op_column_out;
    }

    /* Compute in len the number of bytes of data we need to read in order
    ** to get nField type values.  offset is an upper bound on this.  But
    ** nField might be significantly less than the true number of columns
    ** in the table, and in that case, 5*nField+3 might be smaller than offset.
    ** We want to minimize len in order to limit the size of the memory
    ** allocation, especially if a corrupt database file has caused offset
    ** to be oversized. Offset is limited to 98307 above.  But 98307 might
    ** still exceed Robson memory allocation limits on some configurations.
    ** On systems that cannot tolerate large memory allocations, nField*5+3
    ** will likely be much smaller since nField will likely be less than
    ** 20 or so.  This insures that Robson memory allocation limits are
    ** not exceeded even for corrupt database files.
    ** @wxq 修改
    ** 为了获取变量nFiedld的类型值，需要计算我们读取的数据的字节长度。在这里，
    ** 变量offset是一个上限值。但变量nField的值可能明显少于数据表中真正的列数，
    ** 如果真是那样，5*nField+3可能小于变量offset的值。为了限制内存分配的大小，我们要
    ** 使len(数据长度)尽量小，特别是当损坏的数据库文件已经引起offset的值过大的时候。
    ** 变量Offset的极限值是98307。但98307可能还是超过了Robson内存分配在某些配置上的限制。
    ** 由于系统不允许过大的内存分配，因此nField的值可能会小于20，nField*5+3的值可能也会比较小。
    ** 这就确保即使在数据库文件损坏的情况下，也不会超过Robson内存分配限制。
    */
    len = nField*5 + 3;
    if( len > (int)offset ) len = (int)offset;

    /* The KeyFetch() or DataFetch() above are fast and will get the entire
    ** record header in most cases.  But they will fail to get the complete
    ** record header if the record header does not fit on a single page
    ** in the B-Tree.  When that happens, use sqlite3VdbeMemFromBtree() to
    ** acquire the complete header text.
    ** 上面的函数KeyFetch()和DataFetch()在大多数情况下会快速获取整个记录的头文件。但如果记录头
    ** 不适合单个页面的b树，那这两个函数就无法获得完整的记录头文件。当这种情况发生时，
    ** 使用sqlite3VdbeMemFromBtree()函数来获取完整的记录的头文本。
    */
    if( !zRec && avail<len ){
      sMem.flags = 0;
      sMem.db = 0;
      rc = sqlite3VdbeMemFromBtree(pCrsr, 0, len, pC->isIndex, &sMem);
      if( rc!=SQLITE_OK ){
        goto op_column_out;
      }
      zData = sMem.z;
    }
    zEndHdr = (u8 *)&zData[len];
    zIdx = (u8 *)&zData[szHdr];

    /* Scan the header and use it to fill in the aType[] and aOffset[]
    ** arrays.  aType[i] will contain the type integer for the i-th
    ** column and aOffset[i] will contain the offset from the beginning
    ** of the record to the start of the data for the i-th column
    ** 通过扫描头文件以获取数组aType[]和aOffset[]的值。aType[i]存储了第i个列的整型数据，
    ** aOffset[i]存储了从记录的起始地址到第i个列中数据存储的首地址的偏移量。
    */
    for(i=0; i<nField; i++){
      if( zIdx<zEndHdr ){
        aOffset[i] = offset;
        if( zIdx[0]<0x80 ){
          t = zIdx[0];
          zIdx++;
        }else{
          zIdx += sqlite3GetVarint32(zIdx, &t);
        }
        aType[i] = t;
        szField = sqlite3VdbeSerialTypeLen(t);
        offset += szField;
        if( offset<szField ){  /* True if offset overflows */
          zIdx = &zEndHdr[1];  /* Forces SQLITE_CORRUPT return below */
          break;
        }
      }else{
        /* If i is less that nField, then there are fewer fields in this
        ** record than SetNumColumns indicated there are columns in the
        ** table. Set the offset for any extra columns not present in
        ** the record to 0. This tells code below to store the default value
        ** for the column instead of deserializing a value from the record.
        ** 如果i小于nField，那么记录中的字段是比SetNumColumns小，SetNumColumns是表中的列数。
        ** 将offset设置为一个额外的、在记录总不存在的列，并赋值为0。也就是说，下面的代码将
        ** 这一列赋值为默认值，而不是将记录中反序列化后的值赋值给它。
        */
        aOffset[i] = 0;
      }
    }
    sqlite3VdbeMemRelease(&sMem);
    sMem.flags = MEM_Null;

    /* If we have read more header data than was contained in the header,
    ** or if the end of the last field appears to be past the end of the
    ** record, or if the end of the last field appears to be before the end
    ** of the record (when all fields present), then we must be dealing
    ** with a corrupt database.
    ** 如果我们读取的数据比头文件中包含的还要多，或者最后一个字段的结束地址超出了数据记录的
    ** 结束地址，又或者最后一个字段的结束地址出现在数据记录的结束地址之前(所有字段多出现过了)，
    ** 那么我们必须对损坏的数据库进行处理。
    */
    if( (zIdx > zEndHdr) || (offset > payloadSize)
         || (zIdx==zEndHdr && offset!=payloadSize) ){
      rc = SQLITE_CORRUPT_BKPT;
      goto op_column_out;
    }
  }

  /* Get the column information. If aOffset[p2] is non-zero, then
  ** deserialize the value from the record. If aOffset[p2] is zero,
  ** then there are not enough fields in the record to satisfy the
  ** request.  In this case, set the value NULL or to P4 if P4 is
  ** a pointer to a Mem object.
  ** 获得列的信息。如果aOffset[p2]是非零值，那么反序列化记录中的值。如果aOffset[p2]为0，
  ** 那就意味着记录中没有足够的字段来满足需求。在这种情况下，将值设置为NULL，如果P4是一个
  ** 指向Mem类型对象的指针，将值设置为P4。
  */
  if( aOffset[p2] ){
    assert( rc==SQLITE_OK );
    if( zRec ){
      /* This is the common case where the whole row fits on a single page
      ** 所有的行都符合单页面是一种常见的情况。
      */
      VdbeMemRelease(pDest);
      sqlite3VdbeSerialGet((u8 *)&zRec[aOffset[p2]], aType[p2], pDest);
    }else{
      /* This branch happens only when the row overflows onto multiple pages
      ** 只有在行溢出到多个页面时，else中出现的情况才会发生
      */
      t = aType[p2];
      if( (pOp->p5 & (OPFLAG_LENGTHARG|OPFLAG_TYPEOFARG))!=0
       && ((t>=12 && (t&1)==0) || (pOp->p5 & OPFLAG_TYPEOFARG)!=0)
      ){
        /* Content is irrelevant for the typeof() function and for
        ** the length(X) function if X is a blob.  So we might as well use
        ** bogus content rather than reading content from disk.  NULL works
        ** for text and blob and whatever is in the payloadSize64 variable
        ** will work for everything else.
        ** 对于函数typeof()和参数X是blob类型的函数length(X)来说，内容是无关紧要的。
        ** 因此，我们不妨使用虚拟数据而不是从磁盘读取数据。NULL值适用于文本类型、blob类型，
        ** 以及任何64位有效长度的变量，在这些情况下NULL都能够正常工作
        */
        zData = t<12 ? (char*)&payloadSize64 : 0;
      }else{
        len = sqlite3VdbeSerialTypeLen(t);
        sqlite3VdbeMemMove(&sMem, pDest);
        rc = sqlite3VdbeMemFromBtree(pCrsr, aOffset[p2], len,  pC->isIndex,
                                     &sMem);
        if( rc!=SQLITE_OK ){
          goto op_column_out;
        }
        zData = sMem.z;
      }
      sqlite3VdbeSerialGet((u8*)zData, t, pDest);
    }
    pDest->enc = encoding;
  }else{
    if( pOp->p4type==P4_MEM ){
      sqlite3VdbeMemShallowCopy(pDest, pOp->p4.pMem, MEM_Static);
    }else{
      MemSetTypeFlag(pDest, MEM_Null);
    }
  }

  /* If we dynamically allocated space to hold the data (in the
  ** sqlite3VdbeMemFromBtree() call above) then transfer control of that
  ** dynamically allocated space over to the pDest structure.
  ** This prevents a memory copy.
  ** 如果我们通过动态分配空间来保存数据(在上面调用的sqlite3VdbeMemFromBtree()函数中)，
  ** 然后再用动态分配的空间来存储pDest结构类型的数据。这可以防止内存复制。
  */
  if( sMem.zMalloc ){
    assert( sMem.z==sMem.zMalloc );
    assert( !(pDest->flags & MEM_Dyn) );
    assert( !(pDest->flags & (MEM_Blob|MEM_Str)) || pDest->z==sMem.z );
    pDest->flags &= ~(MEM_Ephem|MEM_Static);
    pDest->flags |= MEM_Term;
    pDest->z = sMem.z;
    pDest->zMalloc = sMem.zMalloc;
  }

  rc = sqlite3VdbeMemMakeWriteable(pDest);

op_column_out:
  UPDATE_MAX_BLOBSIZE(pDest);
  REGISTER_TRACE(pOp->p3, pDest);
  break;
}

/* Opcode: Affinity P1 P2 * P4 *
**
** Apply affinities to a range of P2 registers starting with P1.
** 对从P1开始到P2的一系列寄存器进行相关性比较。
**
** P4 is a string that is P2 characters long. The nth character of the
** string indicates the column affinity that should be used for the nth
** memory cell in the range.
** P4是一个长度与P2中字符相等的字符串。字符串的第n个字符表示列的关联性，这个关联性
** 只用于第n个存储单元的范围内。
*/
case OP_Affinity: {
  const char *zAffinity;   /* The affinity to be applied */
  char cAff;               /* A single character of affinity */

  zAffinity = pOp->p4.z;
  assert( zAffinity!=0 );
  assert( zAffinity[pOp->p2]==0 );
  pIn1 = &aMem[pOp->p1];
  while( (cAff = *(zAffinity++))!=0 ){
    assert( pIn1 <= &p->aMem[p->nMem] );
    assert( memIsValid(pIn1) );
    ExpandBlob(pIn1);
    applyAffinity(pIn1, cAff, encoding);
    pIn1++;
  }
  break;
}

/* Opcode: MakeRecord P1 P2 P3 P4 *
**
** Convert P2 registers beginning with P1 into the [record format]
** use as a data record in a database table or as a key
** in an index.  The OP_Column opcode can decode the record later.
** 将从P1开始到P2的寄存器转换成[记录 格式]形式的数据记录格式，以用作数据库表的数据记录，
** 或索引的键。OP_Column操作码可以解码这种样式的数据记录。
**
** P4 may be a string that is P2 characters long.  The nth character of the
** string indicates the column affinity that should be used for the nth
** field of the index key.
** P4是一个长度与P2中字符相等的字符串。这个字符串的第n个字符表示列的关联性，这个关联性
** 应该用作第n个转的索引键。
**
** The mapping from character to affinity is given by the SQLITE_AFF_
** macros defined in sqliteInt.h.
** sqliteInt.h中通过“SQLITE_AFF_”格式的宏，定义了字符与关联性变量的映射关系。
**
** If P4 is NULL then all index fields have the affinity NONE.
** 如果P4是NULL，那么所有索引字段都与NONE相关联。
*/
case OP_MakeRecord: {
  u8 *zNewRecord;        /* A buffer to hold the data for the new record
                         ** *zNewRecord作为缓冲变量，存放新纪录的数据
                         */
  Mem *pRec;             /* The new record */
  u64 nData;             /* Number of bytes of data space */
  int nHdr;              /* Number of bytes of header space */
  i64 nByte;             /* Data space required for this record */
  int nZero;             /* Number of zero bytes at the end of the record */
  int nVarint;           /* Number of bytes in a varint */
  u32 serial_type;       /* Type field */
  Mem *pData0;           /* First field to be combined into the record
                         ** 第一个字段组合成的记录
                         */
  Mem *pLast;            /* Last field of the record */
  int nField;            /* Number of fields in the record */
  char *zAffinity;       /* The affinity string for the record */
  int file_format;       /* File format to use for encoding
                         ** 文件的编码格式
                         */
  int i;                 /* Space used in zNewRecord[] */
  int len;               /* Length of a field */

  /* Assuming the record contains N fields, the record format looks
  ** like this:
  ** 假设记录中含有N个字段，记录的格式就像这样：
  **
  ** ------------------------------------------------------------------------
  ** | hdr-size | type 0 | type 1 | ... | type N-1 | data0 | ... | data N-1 |
  ** ------------------------------------------------------------------------
  **
  ** Data(0) is taken from register P1.  Data(1) comes from register P1+1
  ** and so froth.
  ** Data(0)取自寄存器P1。 Data(1)取自寄存器P2，以此类推。
  **
  ** Each type field is a varint representing the serial type of the
  ** corresponding data element (see sqlite3VdbeSerialType()). The
  ** hdr-size field is also a varint which is the offset from the beginning
  ** of the record to data0.
  ** 每种类型字段都是由一个varint(varint的意思应该是整形变量)，varint代表一系列与它对应的
  ** 数据元素类型(见sqlite3VdbeSerialType()，这个函数在vdbeaux.c文件中，它将返回值存储在
  ** 参数pMem中返回给调用者)。hdr-size字段也是一个varint，这个varint是从记录的开始位置
  ** 到data0的偏移量。
  */
  nData = 0;         /* Number of bytes of data space
                     ** 数据空间的字节数
                     */
  nHdr = 0;          /* Number of bytes of header space */
  nZero = 0;         /* Number of zero bytes at the end of the record */
  nField = pOp->p1;
  zAffinity = pOp->p4.z;
  assert( nField>0 && pOp->p2>0 && pOp->p2+nField<=p->nMem+1 );
  pData0 = &aMem[nField];
  nField = pOp->p2;
  pLast = &pData0[nField-1];
  file_format = p->minWriteFileFormat;

  /* Identify the output register */
  assert( pOp->p3<pOp->p1 || pOp->p3>=pOp->p1+pOp->p2 );
  pOut = &aMem[pOp->p3];
  memAboutToChange(p, pOut);

  /* Loop through the elements that will make up the record to figure
  ** out how much space is required for the new record.
  ** 遍历新纪录中的所有元素，以计算需要多少空间来存放这个新纪录。
  */
  for(pRec=pData0; pRec<=pLast; pRec++){
    assert( memIsValid(pRec) );
    if( zAffinity ){
      applyAffinity(pRec, zAffinity[pRec-pData0], encoding);
    }
    if( pRec->flags&MEM_Zero && pRec->n>0 ){
      sqlite3VdbeMemExpandBlob(pRec);
    }
    serial_type = sqlite3VdbeSerialType(pRec, file_format);
    len = sqlite3VdbeSerialTypeLen(serial_type);
    nData += len;
    nHdr += sqlite3VarintLen(serial_type);
    if( pRec->flags & MEM_Zero ){
      /* Only pure zero-filled BLOBs can be input to this Opcode.
      ** We do not allow blobs with a prefix and a zero-filled tail.
      ** 只有所有数据位都是0的Blob类型的值才能被输入到这个操作中。我们不允许blobs类型的
      ** 的数据中出现前缀和全部是0的后缀。
      */
      nZero += pRec->u.nZero;
    }else if( len ){
      nZero = 0;
    }
  }

  /* Add the initial header varint and total the size */
  nHdr += nVarint = sqlite3VarintLen(nHdr);
  if( nVarint<sqlite3VarintLen(nHdr) ){
    nHdr++;
  }
  nByte = nHdr+nData-nZero;
  if( nByte>db->aLimit[SQLITE_LIMIT_LENGTH] ){
    goto too_big;
  }

  /* Make sure the output register has a buffer large enough to store
  ** the new record. The output register (pOp->p3) is not allowed to
  ** be one of the input registers (because the following call to
  ** sqlite3VdbeMemGrow() could clobber the value before it is used).
  ** 确保输出寄存器有足够大的缓冲区来存储新的记录。输出寄存器(pOp->p3)不允许的输入寄存器
  ** (因为在使用寄存器之前，代码会调用sqlite3VdbeMemGrow()函数来强制改写寄存器中的值)。
  */
  if( sqlite3VdbeMemGrow(pOut, (int)nByte, 0) ){
    goto no_mem;
  }
  zNewRecord = (u8 *)pOut->z;

  /* Write the record */
  i = putVarint32(zNewRecord, nHdr);
  for(pRec=pData0; pRec<=pLast; pRec++){
    serial_type = sqlite3VdbeSerialType(pRec, file_format);
    i += putVarint32(&zNewRecord[i], serial_type);      /* serial type */
  }
  for(pRec=pData0; pRec<=pLast; pRec++){  /* serial data */
    i += sqlite3VdbeSerialPut(&zNewRecord[i], (int)(nByte-i), pRec,file_format);
  }
  assert( i==nByte );

  assert( pOp->p3>0 && pOp->p3<=p->nMem );
  pOut->n = (int)nByte;
  pOut->flags = MEM_Blob | MEM_Dyn;
  pOut->xDel = 0;
  if( nZero ){
    pOut->u.nZero = nZero;
    pOut->flags |= MEM_Zero;
  }
  pOut->enc = SQLITE_UTF8;  /* In case the blob is ever converted to text */
  REGISTER_TRACE(pOp->p3, pOut);
  UPDATE_MAX_BLOBSIZE(pOut);
  break;
}

/* Opcode: Count P1 P2 **
**
** Store the number of entries (an integer value) in the table or index
** opened by cursor P1 in register P2
** @author wxq
** 把记录的数量或者是已经被p1游标打开的索引存储在p2里面
*/
#ifndef SQLITE_OMIT_BTREECOUNT
case OP_Count: {         /* out2-prerelease */
  i64 nEntry;
  BtCursor *pCrsr;

  pCrsr = p->apCsr[pOp->p1]->pCursor;
  if( ALWAYS(pCrsr) ){
    rc = sqlite3BtreeCount(pCrsr, &nEntry);
  }else{
    nEntry = 0;
  }
  pOut->u.i = nEntry;
  break;
}
#endif

/* Opcode: Savepoint P1 * * P4 *
**
** Open, release or rollback the savepoint named by parameter P4, depending
** on the value of P1. To open a new savepoint, P1==0. To release (commit) an
** existing savepoint, P1==1, or to rollback an existing savepoint P1==2.
** 保存点的打开，释放或回滚由参数P4来指定，依赖于P1的值。当P1==0时，打开一个新的保存点。
** 当P1==1时，释放(提交)现有的保存点。当P1==2时，回滚现有保存点。
*/
case OP_Savepoint: {
  int p1;                         /* Value of P1 operand */
  char *zName;                    /* Name of savepoint */
  int nName;
  Savepoint *pNew;
  Savepoint *pSavepoint;
  Savepoint *pTmp;
  int iSavepoint;
  int ii;

  p1 = pOp->p1;
  zName = pOp->p4.z;

  /* Assert that the p1 parameter is valid. Also that if there is no open
  ** transaction, then there cannot be any savepoints.
  ** 断言参数p1是有效的。如果还没有打开事务，那么就不需要任何保存点。
  */
  assert( db->pSavepoint==0 || db->autoCommit==0 );
  assert( p1==SAVEPOINT_BEGIN||p1==SAVEPOINT_RELEASE||p1==SAVEPOINT_ROLLBACK );
  assert( db->pSavepoint || db->isTransactionSavepoint==0 );
  assert( checkSavepointCount(db) );

  if( p1==SAVEPOINT_BEGIN ){
    if( db->writeVdbeCnt>0 ){
      /* A new savepoint cannot be created if there are active write
      ** statements (i.e. open read/write incremental blob handles).
      ** 如果有写操作在活动，就无法创建一个新的保存点(也就是，打开读/写增量blob的操作)。
      */
      sqlite3SetString(&p->zErrMsg, db, "cannot open savepoint - "
        "SQL statements in progress");
      rc = SQLITE_BUSY;
    }else{
      nName = sqlite3Strlen30(zName);

#ifndef SQLITE_OMIT_VIRTUALTABLE
      /* This call is Ok even if this savepoint is actually a transaction
      ** savepoint (and therefore should not prompt xSavepoint()) callbacks.
      ** If this is a transaction savepoint being opened, it is guaranteed
      ** that the db->aVTrans[] array is empty.
      ** 即使这个保存点是一个事务的保存点(因此不应该提示xSavepoint()函数)回调，这个调用也是正确的。
      ** 如果这是一个事务的保存点正被打开，要保证数组db->aVTrans[]为空。
      */
      assert( db->autoCommit==0 || db->nVTrans==0 );
      rc = sqlite3VtabSavepoint(db, SAVEPOINT_BEGIN,
                                db->nStatement+db->nSavepoint);
      if( rc!=SQLITE_OK ) goto abort_due_to_error;
#endif

      /* Create a new savepoint structure. */
      pNew = sqlite3DbMallocRaw(db, sizeof(Savepoint)+nName+1);
      if( pNew ){
        pNew->zName = (char *)&pNew[1];
        memcpy(pNew->zName, zName, nName+1);

        /* If there is no open transaction, then mark this as a special
        ** "transaction savepoint".
        ** 如果没有打开事务，那么将这个保存点标记为一个特殊的“事务保存点”。
        */
        if( db->autoCommit ){
          db->autoCommit = 0;
          db->isTransactionSavepoint = 1;
        }else{
          db->nSavepoint++;
        }

        /* Link the new savepoint into the database handle's list.
        ** 将新的保存点链接到数据库处理列表中。
        */
        pNew->pNext = db->pSavepoint;
        db->pSavepoint = pNew;
        pNew->nDeferredCons = db->nDeferredCons;
      }
    }
  }else{
    iSavepoint = 0;

    /* Find the named savepoint. If there is no such savepoint, then an
    ** an error is returned to the user.
    ** 找到指定的保存点。如果没有这样的保存点，则需要向用户返回一个错误信息。
    */
    for(
      pSavepoint = db->pSavepoint;
      pSavepoint && sqlite3StrICmp(pSavepoint->zName, zName);
      pSavepoint = pSavepoint->pNext
    ){
      iSavepoint++;
    }
    if( !pSavepoint ){
      sqlite3SetString(&p->zErrMsg, db, "no such savepoint: %s", zName);
      rc = SQLITE_ERROR;
    }else if( db->writeVdbeCnt>0 && p1==SAVEPOINT_RELEASE ){
      /* It is not possible to release (commit) a savepoint if there are
      ** active write statements.
      ** 如果有写操作在活动，就不能释放(提交)保存点。
      */
      sqlite3SetString(&p->zErrMsg, db,
        "cannot release savepoint - SQL statements in progress"
      );
      rc = SQLITE_BUSY;
    }else{

      /* Determine whether or not this is a transaction savepoint. If so,
      ** and this is a RELEASE command, then the current transaction
      ** is committed. 
      ** 判断pSavepoint->pNext是不是一个事务的保存点。如果是，而且这是一个发布命令，那么提交当前事务。
      */
      int isTransaction = pSavepoint->pNext==0 && db->isTransactionSavepoint;
      if( isTransaction && p1==SAVEPOINT_RELEASE ){
        if( (rc = sqlite3VdbeCheckFk(p, 1))!=SQLITE_OK ){
          goto vdbe_return;
        }
        db->autoCommit = 1;
        if( sqlite3VdbeHalt(p)==SQLITE_BUSY ){
          p->pc = pc;
          db->autoCommit = 0;
          p->rc = rc = SQLITE_BUSY;
          goto vdbe_return;
        }
        db->isTransactionSavepoint = 0;
        rc = p->rc;
      }else{
        iSavepoint = db->nSavepoint - iSavepoint - 1;
        if( p1==SAVEPOINT_ROLLBACK ){
          for(ii=0; ii<db->nDb; ii++){
            sqlite3BtreeTripAllCursors(db->aDb[ii].pBt, SQLITE_ABORT);
          }
        }
        for(ii=0; ii<db->nDb; ii++){
          rc = sqlite3BtreeSavepoint(db->aDb[ii].pBt, p1, iSavepoint);
          if( rc!=SQLITE_OK ){
            goto abort_due_to_error;
          }
        }
        if( p1==SAVEPOINT_ROLLBACK && (db->flags&SQLITE_InternChanges)!=0 ){
          sqlite3ExpirePreparedStatements(db);
          sqlite3ResetAllSchemasOfConnection(db);
          db->flags = (db->flags | SQLITE_InternChanges);
        }
      }

      /* Regardless of whether this is a RELEASE or ROLLBACK, destroy all
      ** savepoints nested inside of the savepoint being operated on.
      ** 不管是释放还是回滚，都需要销毁所有的保存点，包括与被操作点相嵌套的点，而不仅仅是
      ** 只操作那个被操作的保存点。
      */
      while( db->pSavepoint!=pSavepoint ){
        pTmp = db->pSavepoint;
        db->pSavepoint = pTmp->pNext;
        sqlite3DbFree(db, pTmp);
        db->nSavepoint--;
      }

      /* If it is a RELEASE, then destroy the savepoint being operated on
      ** too. If it is a ROLLBACK TO, then set the number of deferred
      ** constraint violations present in the database to the value stored
      ** when the savepoint was created.
      ** 如果它是释放操作，也要销毁被操作的保存点。如果是回滚，
      ** 然后设置延迟约束违反的数量目前在数据库中存储的值保存点时创建的。
      */
      if( p1==SAVEPOINT_RELEASE ){
        assert( pSavepoint==db->pSavepoint );
        db->pSavepoint = pSavepoint->pNext;
        sqlite3DbFree(db, pSavepoint);
        if( !isTransaction ){
          db->nSavepoint--;
        }
      }else{
        db->nDeferredCons = pSavepoint->nDeferredCons;
      }

      if( !isTransaction ){
        rc = sqlite3VtabSavepoint(db, p1, iSavepoint);
        if( rc!=SQLITE_OK ) goto abort_due_to_error;
      }
    }
  }

  break;
}

/* Opcode: AutoCommit P1 P2 * * *
**
** Set the database auto-commit flag to P1 (1 or 0). If P2 is true, roll
** back any currently active btree transactions. If there are any active
** VMs (apart from this one), then a ROLLBACK fails.  A COMMIT fails if
** there are active writing VMs or active VMs that use shared cache.
** 设置数据库自动提交的标志值flag为P1(1或0)。如果P2是真，回退到任何一个当前正在活动的btree事务。
** 如果有任何一个正在活动的vm(除了当前这个)，那么回滚失败。如果存在一个进程正在对vm进行写操作，
** 或者某个虚拟机使用了共享缓存，那么提交操作就会失败。
**
** This instruction causes the VM to halt.
** 这个指令会导致虚拟机停止。
*/
case OP_AutoCommit: {
  int desiredAutoCommit;
  int iRollback;
  int turnOnAC;

  desiredAutoCommit = pOp->p1;
  iRollback = pOp->p2;
  turnOnAC = desiredAutoCommit && !db->autoCommit;
  assert( desiredAutoCommit==1 || desiredAutoCommit==0 );
  assert( desiredAutoCommit==1 || iRollback==0 );
  assert( db->activeVdbeCnt>0 );  /* At least this one VM is active */

#if 0
  if( turnOnAC && iRollback && db->activeVdbeCnt>1 ){
    /* If this instruction implements a ROLLBACK and other VMs are
    ** still running, and a transaction is active, return an error indicating
    ** that the other VMs must complete first.
    ** 如果这个指令实现了一个回滚操作，而且其他虚拟机仍在运行，同时事务是活跃的，
    ** 那么返回一个错误信息，这个信息表明需要让其他vm先执行。
    */
    sqlite3SetString(&p->zErrMsg, db, "cannot rollback transaction - "
        "SQL statements in progress");
    rc = SQLITE_BUSY;
  }else
#endif
  if( turnOnAC && !iRollback && db->writeVdbeCnt>0 ){
    /* If this instruction implements a COMMIT and other VMs are writing
    ** return an error indicating that the other VMs must complete first.
    ** 如果该指令执行提交操作，同时其他vm正在进行写操作，那么就要返回一个错误信息，
    ** 这个信息表明必须让其他vm先完成。
    */
    sqlite3SetString(&p->zErrMsg, db, "cannot commit transaction - "
        "SQL statements in progress");
    rc = SQLITE_BUSY;
  }else if( desiredAutoCommit!=db->autoCommit ){
    if( iRollback ){
      assert( desiredAutoCommit==1 );
      sqlite3RollbackAll(db, SQLITE_ABORT_ROLLBACK);
      db->autoCommit = 1;
    }else if( (rc = sqlite3VdbeCheckFk(p, 1))!=SQLITE_OK ){
      goto vdbe_return;
    }else{
      db->autoCommit = (u8)desiredAutoCommit;
      if( sqlite3VdbeHalt(p)==SQLITE_BUSY ){
        p->pc = pc;
        db->autoCommit = (u8)(1-desiredAutoCommit);
        p->rc = rc = SQLITE_BUSY;
        goto vdbe_return;
      }
    }
    assert( db->nStatement==0 );
    sqlite3CloseSavepoints(db);
    if( p->rc==SQLITE_OK ){
      rc = SQLITE_DONE;
    }else{
      rc = SQLITE_ERROR;
    }
    goto vdbe_return;
  }else{
    sqlite3SetString(&p->zErrMsg, db,
        (!desiredAutoCommit)?"cannot start a transaction within a transaction":(
        (iRollback)?"cannot rollback - no transaction is active":
                   "cannot commit - no transaction is active"));

    rc = SQLITE_ERROR;
  }
  break;
}

/* Opcode: Transaction P1 P2 * * *
**
** Begin a transaction.  The transaction ends when a Commit or Rollback
** opcode is encountered.  Depending on the ON CONFLICT setting, the
** transaction might also be rolled back if an error is encountered.
** 开始一个事务。当遇到提交或回滚操作码时，事务结束。根据冲突设置，如果出现错误，事务也可能回滚。
**
** P1 is the index of the database file on which the transaction is
** started.  Index 0 is the main database file and index 1 is the
** file used for temporary tables.  Indices of 2 or more are used for
** attached databases.
** P1是数据库的索引文件，事务从这个索引开始执行。索引0是主数据库文件，索引1用于临时表文件。
** 有不少于2个的指数用于连接数据库。
**
** If P2 is non-zero, then a write-transaction is started.  A RESERVED lock is
** obtained on the database file when a write-transaction is started.  No
** other process can start another write transaction while this transaction is
** underway.  Starting a write transaction also creates a rollback journal. A
** write transaction must be started before any changes can be made to the
** database.  If P2 is 2 or greater then an EXCLUSIVE lock is also obtained
** on the file.
** 如果P2是非零值，那么写-事务开始执行。写-事务开始时，会给被操作的数据库文件会加一个保留锁。
** 在该事务进行中，其他进程不能够开始另一个写-事务。写-事务开始执行的同时还会创建一个回滚日志。
** 只有启动了写-事务才能对数据库进行更改。如果P2的值是2或更大的值，会给被操作文件加一个独占锁。
**
** If a write-transaction is started and the Vdbe.usesStmtJournal flag is
** true (this flag is set if the Vdbe may modify more than one row and may
** throw an ABORT exception), a statement transaction may also be opened.
** More specifically, a statement transaction is opened iff the database
** connection is currently not in autocommit mode, or if there are other
** active statements. A statement transaction allows the changes made by this
** VDBE to be rolled back after an error without having to roll back the
** entire transaction. If no error is encountered, the statement transaction
** will automatically commit when the VDBE halts.
** 如果写-事务开始，同时Vdbe.usesStmtJournal标记为真(如果Vdbe可以修改多个行，又能够抛出ABORT异常，
** 这个标记值就被设置)，那么一个声明-事务也可能被打开。更具体地说，如果目前的数据库连接不是处在
** 自动提交模式，后者有其他声明-事务在活动，那么声明事务就会被打开。发生错误之后，声明-事务允许
** 只回滚VDBE所做的修改，而不必回滚整个事务。如果没有遇到错误，该声明-事务在VDBE停止时会自动提交。
**
** If P2 is zero, then a read-lock is obtained on the database file.
** 如果P2是0，则会给数据库文件加上只读锁。
*/
case OP_Transaction: {
  Btree *pBt;

  assert( pOp->p1>=0 && pOp->p1<db->nDb );
  assert( (p->btreeMask & (((yDbMask)1)<<pOp->p1))!=0 );
  pBt = db->aDb[pOp->p1].pBt;

  if( pBt ){
    rc = sqlite3BtreeBeginTrans(pBt, pOp->p2);
    if( rc==SQLITE_BUSY ){
      p->pc = pc;
      p->rc = rc = SQLITE_BUSY;
      goto vdbe_return;
    }
    if( rc!=SQLITE_OK ){
      goto abort_due_to_error;
    }

    if( pOp->p2 && p->usesStmtJournal
     && (db->autoCommit==0 || db->activeVdbeCnt>1)
    ){
      assert( sqlite3BtreeIsInTrans(pBt) );
      if( p->iStatement==0 ){
        assert( db->nStatement>=0 && db->nSavepoint>=0 );
        db->nStatement++;
        p->iStatement = db->nSavepoint + db->nStatement;
      }

      rc = sqlite3VtabSavepoint(db, SAVEPOINT_BEGIN, p->iStatement-1);
      if( rc==SQLITE_OK ){
        rc = sqlite3BtreeBeginStmt(pBt, p->iStatement);
      }

      /* Store the current value of the database handles deferred constraint
      ** counter. If the statement transaction needs to be rolled back,
      ** the value of this counter needs to be restored too.
      ** 存储数据库处理延迟约束计数器的当前值。如果声明-事务需要回滚，该计数器的值也需要恢复。
      */
      p->nStmtDefCons = db->nDeferredCons;
    }
  }
  break;
}

/* Opcode: ReadCookie P1 P2 P3 * *
**
** Read cookie number P3 from database P1 and write it into register P2.
** P3==1 is the schema version.  P3==2 is the database format.
** P3==3 is the recommended pager cache size, and so forth.  P1==0 is
** the main database file and P1==1 is the database file used to store
** temporary tables.
** 从P1指代的数据库中读取cookie的数量P3，把它写进寄存器P2。P3==1代表架构版本。P3==2代表数据库格式。
** P3==3代表推荐页的缓存大小，等等。P1==0代表主数据库文件，P1==1代表这个数据库文件曾被用于存储临时表。

** There must be a read-lock on the database (either a transaction
** must be started or there must be an open cursor) before
** executing this instruction.
** 执行该指令之前，必须给数据库加一个只读-锁(要么必须启动一个事务，要么必须有一个打开的游标)。
*/
case OP_ReadCookie: {               /* out2-prerelease */
  int iMeta;
  int iDb;
  int iCookie;

  iDb = pOp->p1;
  iCookie = pOp->p3;
  assert( pOp->p3<SQLITE_N_BTREE_META );
  assert( iDb>=0 && iDb<db->nDb );
  assert( db->aDb[iDb].pBt!=0 );
  assert( (p->btreeMask & (((yDbMask)1)<<iDb))!=0 );

  sqlite3BtreeGetMeta(db->aDb[iDb].pBt, iCookie, (u32 *)&iMeta);
  pOut->u.i = iMeta;
  break;
}

/* Opcode: SetCookie P1 P2 P3 * *
**
** Write the content of register P3 (interpreted as an integer)
** into cookie number P2 of database P1.  P2==1 is the schema version.
** P2==2 is the database format. P2==3 is the recommended pager cache
** size, and so forth.  P1==0 is the main database file and P1==1 is the
** database file used to store temporary tables.
** 将寄存器P3的内容写入到P1数据库里的cookie(也就P2)中。P2==1代表架构版本。P2==2代表数据库格式。
** P2==3代表推荐页的缓存大小，等等。P1==0代表主数据库文件，P1==1代表这个数据库文件曾用于存储临时表。
**
** A transaction must be started before executing this opcode.
** 执行这个操作码之前必须有一个事务已经开始执行。
*/
case OP_SetCookie: {       /* in3 */
  Db *pDb;
  assert( pOp->p2<SQLITE_N_BTREE_META );
  assert( pOp->p1>=0 && pOp->p1<db->nDb );
  assert( (p->btreeMask & (((yDbMask)1)<<pOp->p1))!=0 );
  pDb = &db->aDb[pOp->p1];
  assert( pDb->pBt!=0 );
  assert( sqlite3SchemaMutexHeld(db, pOp->p1, 0) );
  pIn3 = &aMem[pOp->p3];
  sqlite3VdbeMemIntegerify(pIn3);
  /* See note about index shifting on OP_ReadCookie */
  rc = sqlite3BtreeUpdateMeta(pDb->pBt, pOp->p2, (int)pIn3->u.i);
  if( pOp->p2==BTREE_SCHEMA_VERSION ){
    /* When the schema cookie changes, record the new cookie internally */
    pDb->pSchema->schema_cookie = (int)pIn3->u.i;
    db->flags |= SQLITE_InternChanges;
  }else if( pOp->p2==BTREE_FILE_FORMAT ){
    /* Record changes in the file format */
    pDb->pSchema->file_format = (u8)pIn3->u.i;
  }
  if( pOp->p1==1 ){
    /* Invalidate all prepared statements whenever the TEMP database
    ** schema is changed.  Ticket #1644
    ** 每当临时数据库模式被更改，所有准备好的语句就无效了。
    */
    sqlite3ExpirePreparedStatements(db);
    p->expired = 0;
  }
  break;
}

/* Opcode: VerifyCookie P1 P2 P3 * *
**
** Check the value of global database parameter number 0 (the
** schema version) and make sure it is equal to P2 and that the
** generation counter on the local schema parse equals P3.
** 检索整个数据库中值为0的参数的个数(模式版本)，并确保这个数量等于P2，这个迭代计数器
** 在本地模式(local schema这个东东不知道该翻译成什么好)中解析为P3。
**
** P1 is the database number which is 0 for the main database file
** and 1 for the file holding temporary tables and some higher number
** for auxiliary databases.
** P1是数据库编号，0代表主数据库文件，1代表这个数据库文件持有临时表，更大的数代表辅助数据库。
**
** The cookie changes its value whenever the database schema changes.
** This operation is used to detect when that the cookie has changed
** and that the current process needs to reread the schema.
** 每当数据库模式改变，cookie也会更改为对应的值。当cookie已经改变，以及当前进程需要重读模式的时候，
** 这个操作通常起检测作用。
**
** Either a transaction needs to have been started or an OP_Open needs
** to be executed (to establish a read lock) before this opcode is
** invoked.
** 在调用此操作码之前，要么需要启动一个事务，要么操作码Open需要被执行(这两个操作都是为了建立一个只读锁)。
*/
case OP_VerifyCookie: {
  int iMeta;
  int iGen;
  Btree *pBt;

  assert( pOp->p1>=0 && pOp->p1<db->nDb );
  assert( (p->btreeMask & (((yDbMask)1)<<pOp->p1))!=0 );
  assert( sqlite3SchemaMutexHeld(db, pOp->p1, 0) );
  pBt = db->aDb[pOp->p1].pBt;
  if( pBt ){
    sqlite3BtreeGetMeta(pBt, BTREE_SCHEMA_VERSION, (u32 *)&iMeta);
    iGen = db->aDb[pOp->p1].pSchema->iGeneration;
  }else{
    iGen = iMeta = 0;
  }
  if( iMeta!=pOp->p2 || iGen!=pOp->p3 ){
    sqlite3DbFree(db, p->zErrMsg);
    p->zErrMsg = sqlite3DbStrDup(db, "database schema has changed");
    /* If the schema-cookie from the database file matches the cookie
    ** stored with the in-memory representation of the schema, do
    ** not reload the schema from the database file.
    ** 如果数据库文件里的模式cookie与储存在内存中的模式cookie匹配，就不需要重新加载数据库文件中模式。
    **
    ** If virtual-tables are in use, this is not just an optimization.
    ** Often, v-tables store their data in other SQLite tables, which
    ** are queried from within xNext() and other v-table methods using
    ** prepared queries. If such a query is out-of-date, we do not want to
    ** discard the database schema, as the user code implementing the
    ** v-table would have to be ready for the sqlite3_vtab structure itself
    ** to be invalidated whenever sqlite3_step() is called from within
    ** a v-table method.
    ** 如果使用虚拟表，这不仅仅是一个优化。v-tables的数据通常储存在其他SQLite表中，需要调用
    ** xNext()函数和其他v-table函数来查询这些数据，这些函数使用了已经准备好的查询语句。如果这样
    ** 如果一个查询过期了，但我们不打算退出数据库模式，
    */
    if( db->aDb[pOp->p1].pSchema->schema_cookie!=iMeta ){
      sqlite3ResetOneSchema(db, pOp->p1);
    }

    p->expired = 1;
    rc = SQLITE_SCHEMA;
  }
  break;
}

/* Opcode: OpenRead P1 P2 P3 P4 P5
**
** Open a read-only cursor for the database table whose root page is
** P2 in a database file.  The database file is determined by P3.
** P3==0 means the main database, P3==1 means the database used for
** temporary tables, and P3>1 means used the corresponding attached
** database.  Give the new cursor an identifier of P1.  The P1
** values need not be contiguous but all P1 values should be small integers.
** It is an error for P1 to be negative.
** 在数据库文件中，打开一个只读游标，这个指针指向根页面为P2的数据库表。数据库文件由P3来决定。
** P3==0代表主数据库，P3==1意味着这个数据库文件曾用于存储临时表，和P3>1表示使用相应的附加数据库。
** 给这个新指针一个标示符P1。P1值不必是连续的，但P1的所有值应该小整数。如果P1是负数，将产生错误。
**
** If P5!=0 then use the content of register P2 as the root page, not
** the value of P2 itself.
** 如果P5!=0，那么将寄存器P2中的内容作为跟页，而不是使用P2本身的值。
**
** There will be a read lock on the database whenever there is an
** open cursor.  If the database was unlocked prior to this instruction
** then a read lock is acquired as part of this instruction.  A read
** lock allows other processes to read the database but prohibits
** any other process from modifying the database.  The read lock is
** released when all cursors are closed.  If this instruction attempts
** to get a read lock but fails, the script terminates with an
** SQLITE_BUSY error code.
** 只要有一个打开的游标，就需要给数据库加上一个读锁。如果在执行这个指令之前数据库无锁状态，
** 那么这个指令中必须包含给数据库加读锁的操作。读锁允许其他进程读取数据库，但禁止任何其他进程
** 修改数据库。所有游标(指针)都关闭后读锁才会释放。如果该指令试图获得读锁但失败了，
** 脚本(程序)就会终止，同时生成一个SQLITE_BUSY错误代码。
**
** The P4 value may be either an integer (P4_INT32) or a pointer to
** a KeyInfo structure (P4_KEYINFO). If it is a pointer to a KeyInfo
** structure, then said structure defines the content and collating
** sequence of the index being opened. Otherwise, if P4 is an integer
** value, it is set to the number of columns in the table.
** P4的值可以是一个整数(P4_INT32)或一个指向KeyInfo结构类型的指针(P4_KEYINFO)。如果它是一个
** KeyInfo结构体指针，也就意味着结构体定义的内容和索引的排序序列正在被打开。否则，
** 如果P4是一个整型数值，它的值会被设置为表的列数。。
**
** See also OpenWrite.
*/
/* Opcode: OpenWrite P1 P2 P3 P4 P5
**
** Open a read/write cursor named P1 on the table or index whose root
** page is P2.  Or if P5!=0 use the content of register P2 to find the
** root page.
** 在表上或者跟页面为P2的索引上打开一个名为P1的读/写游标。如果P5!=0，使用寄存器P2的内容来寻找根页面。
**
** The P4 value may be either an integer (P4_INT32) or a pointer to
** a KeyInfo structure (P4_KEYINFO). If it is a pointer to a KeyInfo
** structure, then said structure defines the content and collating
** sequence of the index being opened. Otherwise, if P4 is an integer
** value, it is set to the number of columns in the table, or to the
** largest index of any column of the table that is actually used.
** P4的值可以是一个整数(P4_INT32)或一个指向KeyInfo结构类型的指针(P4_KEYINFO)。如果它是一个
** KeyInfo结构体指针，也就意味着结构体定义的内容和索引的排序序列正在被打开。否则，
** 如果P4是一个整型数值，它的值会被设置为表的列数，或者设置为表中实际使用的任何列的做大索引值。
**
** This instruction works just like OpenRead except that it opens the cursor
** in read/write mode.  For a given table, there can be one or more read-only
** cursors or a single read/write cursor but not both.
** 这个指令的工作流程与操作码OpenRead相似，除了它以读/写模式打开游标。对于一个给定的表，
** 可以有一个或多个只读指针，或者有不多于一个的读/写游标。
**
** See also OpenRead.
*/
case OP_OpenRead:
case OP_OpenWrite: {
  int nField;
  KeyInfo *pKeyInfo;
  int p2;
  int iDb;
  int wrFlag;
  Btree *pX;
  VdbeCursor *pCur;
  Db *pDb;

  assert( (pOp->p5&(OPFLAG_P2ISREG|OPFLAG_BULKCSR))==pOp->p5 );
  assert( pOp->opcode==OP_OpenWrite || pOp->p5==0 );

  if( p->expired ){
    rc = SQLITE_ABORT;
    break;
  }

  nField = 0;
  pKeyInfo = 0;
  p2 = pOp->p2;
  iDb = pOp->p3;
  assert( iDb>=0 && iDb<db->nDb );
  assert( (p->btreeMask & (((yDbMask)1)<<iDb))!=0 );
  pDb = &db->aDb[iDb];
  pX = pDb->pBt;
  assert( pX!=0 );
  if( pOp->opcode==OP_OpenWrite ){
    wrFlag = 1;
    assert( sqlite3SchemaMutexHeld(db, iDb, 0) );
    if( pDb->pSchema->file_format < p->minWriteFileFormat ){
      p->minWriteFileFormat = pDb->pSchema->file_format;
    }
  }else{
    wrFlag = 0;
  }
  if( pOp->p5 & OPFLAG_P2ISREG ){
    assert( p2>0 );
    assert( p2<=p->nMem );
    pIn2 = &aMem[p2];
    assert( memIsValid(pIn2) );
    assert( (pIn2->flags & MEM_Int)!=0 );
    sqlite3VdbeMemIntegerify(pIn2);
    p2 = (int)pIn2->u.i;
    /* The p2 value always comes from a prior OP_CreateTable opcode and
    ** that opcode will always set the p2 value to 2 or more or else fail.
    ** If there were a failure, the prepared statement would have halted
    ** before reaching this instruction.
    ** P2的值通常由前面的操作码OP_CreateTable来设定，p2值一般被设置为2或更大的数，否者设置失败。
    ** 如果失败了，预先准备好的语句将会在到达这个指令之前停止执行。
    */
    if( NEVER(p2<2) ) {
      rc = SQLITE_CORRUPT_BKPT;
      goto abort_due_to_error;
    }
  }
  if( pOp->p4type==P4_KEYINFO ){
    pKeyInfo = pOp->p4.pKeyInfo;
    pKeyInfo->enc = ENC(p->db);
    nField = pKeyInfo->nField+1;
  }else if( pOp->p4type==P4_INT32 ){
    nField = pOp->p4.i;
  }
  assert( pOp->p1>=0 );
  pCur = allocateCursor(p, pOp->p1, nField, iDb, 1);
  if( pCur==0 ) goto no_mem;
  pCur->nullRow = 1;
  pCur->isOrdered = 1;
  rc = sqlite3BtreeCursor(pX, p2, wrFlag, pKeyInfo, pCur->pCursor);
  pCur->pKeyInfo = pKeyInfo;
  assert( OPFLAG_BULKCSR==BTREE_BULKLOAD );
  sqlite3BtreeCursorHints(pCur->pCursor, (pOp->p5 & OPFLAG_BULKCSR));

  /* Since it performs no memory allocation or IO, the only value that
  ** sqlite3BtreeCursor() may return is SQLITE_OK.
  ** 因为这个操作码在执行工程中没有内存分配或IO，因此sqlite3BtreeCursor()函数会返回唯一的值SQLITE_OK。
  */
  assert( rc==SQLITE_OK );

  /* Set the VdbeCursor.isTable and isIndex variables. Previous versions of
  ** SQLite used to check if the root-page flags were sane at this point
  ** and report database corruption if they were not, but this check has
  ** since moved into the btree layer.
  ** 设置变量VdbeCursor.isTable和isIndex变量。在以前的SQLite版本中，如果根页的标记值
  ** 在执行到此处时是完整的，程序会执行检查操作，如果标记值不完整，则会报告数据库故障，
  ** 但是现在这个检查操作已经移到了btree层。
  */
  pCur->isTable = pOp->p4type!=P4_KEYINFO;
  pCur->isIndex = !pCur->isTable;
  break;
}
/*翻译到这里，就ok了*/


/* Opcode: OpenEphemeral P1 P2 * P4 P5
**
** Open a new cursor P1 to a transient table.
** The cursor is always opened read/write even if
** the main database is read-only.  The ephemeral
** table is deleted automatically when the cursor is closed.
**打开新的索引P1,P1指向一个事务表。
**此索引经常打开去读取和更改。
** P2 is the number of columns in the ephemeral table.
** The cursor points to a BTree table if P4==0 and to a BTree index
** if P4 is not 0.  If P4 is not NULL, it points to a KeyInfo structure
** that defines the format of keys in the index.
**
** This opcode was once called OpenTemp.  But that created
** confusion because the term "temp table", might refer either
** to a TEMP table at the SQL level, or to a table opened by
** this opcode.  Then this opcode was call OpenVirtual.  But
** that created confusion with the whole virtual-table idea.
**
** The P5 parameter can be a mask of the BTREE_* flags defined
** in btree.h.  These flags control aspects of the operation of
** the btree.  The BTREE_OMIT_JOURNAL and BTREE_SINGLE flags are
** added automatically.
*/
/* Opcode: OpenAutoindex P1 P2 * P4 *
**
** This opcode works the same as OP_OpenEphemeral.  It has a
** different name to distinguish its use.  Tables created using
** by this opcode will be used for automatically created transient
** indices in joins.
** 第一次使用github
*/

case OP_OpenAutoindex:
case OP_OpenEphemeral: {
  VdbeCursor *pCx;
  static const int vfsFlags =
      SQLITE_OPEN_READWRITE |
      SQLITE_OPEN_CREATE |
      SQLITE_OPEN_EXCLUSIVE |
      SQLITE_OPEN_DELETEONCLOSE |
      SQLITE_OPEN_TRANSIENT_DB;

  assert( pOp->p1>=0 );
  pCx = allocateCursor(p, pOp->p1, pOp->p2, -1, 1);
  if( pCx==0 ) goto no_mem;
  pCx->nullRow = 1;
  rc = sqlite3BtreeOpen(db->pVfs, 0, db, &pCx->pBt,
                        BTREE_OMIT_JOURNAL | BTREE_SINGLE | pOp->p5, vfsFlags);
  if( rc==SQLITE_OK ){
    rc = sqlite3BtreeBeginTrans(pCx->pBt, 1);
  }
  if( rc==SQLITE_OK ){
    /* If a transient index is required, create it by calling
    ** sqlite3BtreeCreateTable() with the BTREE_BLOBKEY flag before
    ** opening it. If a transient table is required, just use the
    ** automatically created table with root-page 1 (an BLOB_INTKEY table).
    */
    if( pOp->p4.pKeyInfo ){
      int pgno;
      assert( pOp->p4type==P4_KEYINFO );
      rc = sqlite3BtreeCreateTable(pCx->pBt, &pgno, BTREE_BLOBKEY | pOp->p5);
      if( rc==SQLITE_OK ){
        assert( pgno==MASTER_ROOT+1 );
        rc = sqlite3BtreeCursor(pCx->pBt, pgno, 1,
                                (KeyInfo*)pOp->p4.z, pCx->pCursor);
        pCx->pKeyInfo = pOp->p4.pKeyInfo;
        pCx->pKeyInfo->enc = ENC(p->db);
      }
      pCx->isTable = 0;
    }else{
      rc = sqlite3BtreeCursor(pCx->pBt, MASTER_ROOT, 1, 0, pCx->pCursor);
      pCx->isTable = 1;
    }
  }
  pCx->isOrdered = (pOp->p5!=BTREE_UNORDERED);
  pCx->isIndex = !pCx->isTable;
  break;
}

/* Opcode: OpenSorter P1 P2 * P4 *
**
** This opcode works like OP_OpenEphemeral except that it opens
** a transient index that is specifically designed to sort large
** tables using an external merge-sort algorithm.
*/
case OP_SorterOpen: {
  VdbeCursor *pCx;
#ifndef SQLITE_OMIT_MERGE_SORT
  pCx = allocateCursor(p, pOp->p1, pOp->p2, -1, 1);
  if( pCx==0 ) goto no_mem;
  pCx->pKeyInfo = pOp->p4.pKeyInfo;
  pCx->pKeyInfo->enc = ENC(p->db);
  pCx->isSorter = 1;
  rc = sqlite3VdbeSorterInit(db, pCx);
#else
  pOp->opcode = OP_OpenEphemeral;
  pc--;
#endif
  break;
}

/* Opcode: OpenPseudo P1 P2 P3 * *
**
** Open a new cursor that points to a fake table that contains a single
** row of data.  The content of that one row in the content of memory
** register P2.  In other words, cursor P1 becomes an alias for the 
** MEM_Blob content contained in register P2.
**
** A pseudo-table created by this opcode is used to hold a single
** row output from the sorter so that the row can be decomposed into
** individual columns using the OP_Column opcode.  The OP_Column opcode
** is the only cursor opcode that works with a pseudo-table.
**
** P3 is the number of fields in the records that will be stored by
** the pseudo-table.
*/
case OP_OpenPseudo: {
  VdbeCursor *pCx;

  assert( pOp->p1>=0 );
  pCx = allocateCursor(p, pOp->p1, pOp->p3, -1, 0);
  if( pCx==0 ) goto no_mem;
  pCx->nullRow = 1;
  pCx->pseudoTableReg = pOp->p2;
  pCx->isTable = 1;
  pCx->isIndex = 0;
  break;
}

/* Opcode: Close P1 * * * *
**
** Close a cursor previously opened as P1.  If P1 is not
** currently open, this instruction is a no-op.
*/
case OP_Close: {
  assert( pOp->p1>=0 && pOp->p1<p->nCursor );
  sqlite3VdbeFreeCursor(p, p->apCsr[pOp->p1]);
  p->apCsr[pOp->p1] = 0;
  break;
}

/* Opcode: SeekGe P1 P2 P3 P4 *
**
** If cursor P1 refers to an SQL table (B-Tree that uses integer keys), 
** use the value in register P3 as the key.  If cursor P1 refers 
** to an SQL index, then P3 is the first in an array of P4 registers 
** that are used as an unpacked index key. 
**
** Reposition cursor P1 so that  it points to the smallest entry that 
** is greater than or equal to the key value. If there are no records 
** greater than or equal to the key and P2 is not zero, then jump to P2.
**
** See also: Found, NotFound, Distinct, SeekLt, SeekGt, SeekLe
*/
/* Opcode: SeekGt P1 P2 P3 P4 *
**
** If cursor P1 refers to an SQL table (B-Tree that uses integer keys), 
** use the value in register P3 as a key. If cursor P1 refers 
** to an SQL index, then P3 is the first in an array of P4 registers 
** that are used as an unpacked index key. 
**
** Reposition cursor P1 so that  it points to the smallest entry that 
** is greater than the key value. If there are no records greater than 
** the key and P2 is not zero, then jump to P2.
**
** See also: Found, NotFound, Distinct, SeekLt, SeekGe, SeekLe
*/
/* Opcode: SeekLt P1 P2 P3 P4 * 
**
** If cursor P1 refers to an SQL table (B-Tree that uses integer keys), 
** use the value in register P3 as a key. If cursor P1 refers 
** to an SQL index, then P3 is the first in an array of P4 registers 
** that are used as an unpacked index key. 
**
** Reposition cursor P1 so that  it points to the largest entry that 
** is less than the key value. If there are no records less than 
** the key and P2 is not zero, then jump to P2.
**
** See also: Found, NotFound, Distinct, SeekGt, SeekGe, SeekLe
*/
/* Opcode: SeekLe P1 P2 P3 P4 *
**
** If cursor P1 refers to an SQL table (B-Tree that uses integer keys), 
** use the value in register P3 as a key. If cursor P1 refers 
** to an SQL index, then P3 is the first in an array of P4 registers 
** that are used as an unpacked index key. 
**
** Reposition cursor P1 so that it points to the largest entry that 
** is less than or equal to the key value. If there are no records 
** less than or equal to the key and P2 is not zero, then jump to P2.
**
** See also: Found, NotFound, Distinct, SeekGt, SeekGe, SeekLt
*/
case OP_SeekLt:         /* jump, in3 */
case OP_SeekLe:         /* jump, in3 */
case OP_SeekGe:         /* jump, in3 */
case OP_SeekGt: {       /* jump, in3 */
  int res;
  int oc;
  VdbeCursor *pC;
  UnpackedRecord r;
  int nField;
  i64 iKey;      /* The rowid we are to seek to */

  assert( pOp->p1>=0 && pOp->p1<p->nCursor );
  assert( pOp->p2!=0 );
  pC = p->apCsr[pOp->p1];
  assert( pC!=0 );
  assert( pC->pseudoTableReg==0 );
  assert( OP_SeekLe == OP_SeekLt+1 );
  assert( OP_SeekGe == OP_SeekLt+2 );
  assert( OP_SeekGt == OP_SeekLt+3 );
  assert( pC->isOrdered );
  if( ALWAYS(pC->pCursor!=0) ){
    oc = pOp->opcode;
    pC->nullRow = 0;
    if( pC->isTable ){
      /* The input value in P3 might be of any type: integer, real, string,
      ** blob, or NULL.  But it needs to be an integer before we can do
      ** the seek, so covert it. */
      pIn3 = &aMem[pOp->p3];
      applyNumericAffinity(pIn3);
      iKey = sqlite3VdbeIntValue(pIn3);
      pC->rowidIsValid = 0;

      /* If the P3 value could not be converted into an integer without
      ** loss of information, then special processing is required... */
      if( (pIn3->flags & MEM_Int)==0 ){
        if( (pIn3->flags & MEM_Real)==0 ){
          /* If the P3 value cannot be converted into any kind of a number,
          ** then the seek is not possible, so jump to P2 */
          pc = pOp->p2 - 1;
          break;
        }
        /* If we reach this point, then the P3 value must be a floating
        ** point number. */
        assert( (pIn3->flags & MEM_Real)!=0 );

        if( iKey==SMALLEST_INT64 && (pIn3->r<(double)iKey || pIn3->r>0) ){
          /* The P3 value is too large in magnitude to be expressed as an
          ** integer. */
          res = 1;
          if( pIn3->r<0 ){
            if( oc>=OP_SeekGe ){  assert( oc==OP_SeekGe || oc==OP_SeekGt );
              rc = sqlite3BtreeFirst(pC->pCursor, &res);
              if( rc!=SQLITE_OK ) goto abort_due_to_error;
            }
          }else{
            if( oc<=OP_SeekLe ){  assert( oc==OP_SeekLt || oc==OP_SeekLe );
              rc = sqlite3BtreeLast(pC->pCursor, &res);
              if( rc!=SQLITE_OK ) goto abort_due_to_error;
            }
          }
          if( res ){
            pc = pOp->p2 - 1;
          }
          break;
        }else if( oc==OP_SeekLt || oc==OP_SeekGe ){
          /* Use the ceiling() function to convert real->int */
          if( pIn3->r > (double)iKey ) iKey++;
        }else{
          /* Use the floor() function to convert real->int */
          assert( oc==OP_SeekLe || oc==OP_SeekGt );
          if( pIn3->r < (double)iKey ) iKey--;
        }
      } 
      rc = sqlite3BtreeMovetoUnpacked(pC->pCursor, 0, (u64)iKey, 0, &res);
      if( rc!=SQLITE_OK ){
        goto abort_due_to_error;
      }
      if( res==0 ){
        pC->rowidIsValid = 1;
        pC->lastRowid = iKey;
      }
    }else{
      nField = pOp->p4.i;
      assert( pOp->p4type==P4_INT32 );
      assert( nField>0 );
      r.pKeyInfo = pC->pKeyInfo;
      r.nField = (u16)nField;

      /* The next line of code computes as follows, only faster:
      **   if( oc==OP_SeekGt || oc==OP_SeekLe ){
      **     r.flags = UNPACKED_INCRKEY;
      **   }else{
      **     r.flags = 0;
      **   }
      */
      r.flags = (u16)(UNPACKED_INCRKEY * (1 & (oc - OP_SeekLt)));
      assert( oc!=OP_SeekGt || r.flags==UNPACKED_INCRKEY );
      assert( oc!=OP_SeekLe || r.flags==UNPACKED_INCRKEY );
      assert( oc!=OP_SeekGe || r.flags==0 );
      assert( oc!=OP_SeekLt || r.flags==0 );

      r.aMem = &aMem[pOp->p3];
#ifdef SQLITE_DEBUG
      { int i; for(i=0; i<r.nField; i++) assert( memIsValid(&r.aMem[i]) ); }
#endif
      ExpandBlob(r.aMem);
      rc = sqlite3BtreeMovetoUnpacked(pC->pCursor, &r, 0, 0, &res);
      if( rc!=SQLITE_OK ){
        goto abort_due_to_error;
      }
      pC->rowidIsValid = 0;
    }
    pC->deferredMoveto = 0;
    pC->cacheStatus = CACHE_STALE;
#ifdef SQLITE_TEST
    sqlite3_search_count++;
#endif
    if( oc>=OP_SeekGe ){  assert( oc==OP_SeekGe || oc==OP_SeekGt );
      if( res<0 || (res==0 && oc==OP_SeekGt) ){
        rc = sqlite3BtreeNext(pC->pCursor, &res);
        if( rc!=SQLITE_OK ) goto abort_due_to_error;
        pC->rowidIsValid = 0;
      }else{
        res = 0;
      }
    }else{
      assert( oc==OP_SeekLt || oc==OP_SeekLe );
      if( res>0 || (res==0 && oc==OP_SeekLt) ){
        rc = sqlite3BtreePrevious(pC->pCursor, &res);
        if( rc!=SQLITE_OK ) goto abort_due_to_error;
        pC->rowidIsValid = 0;
      }else{
        /* res might be negative because the table is empty.  Check to
        ** see if this is the case.
        */
        res = sqlite3BtreeEof(pC->pCursor);
      }
    }
    assert( pOp->p2>0 );
    if( res ){
      pc = pOp->p2 - 1;
    }
  }else{
    /* This happens when attempting to open the sqlite3_master table
    ** for read access returns SQLITE_EMPTY. In this case always
    ** take the jump (since there are no records in the table).
    */
    pc = pOp->p2 - 1;
  }
  break;
}

/* Opcode: Seek P1 P2 * * *
**
** P1 is an open table cursor and P2 is a rowid integer.  Arrange
** for P1 to move so that it points to the rowid given by P2.
**
** This is actually a deferred seek.  Nothing actually happens until
** the cursor is used to read a record.  That way, if no reads
** occur, no unnecessary I/O happens.
*/
case OP_Seek: {    /* in2 */
  VdbeCursor *pC;

  assert( pOp->p1>=0 && pOp->p1<p->nCursor );
  pC = p->apCsr[pOp->p1];
  assert( pC!=0 );
  if( ALWAYS(pC->pCursor!=0) ){
    assert( pC->isTable );
    pC->nullRow = 0;
    pIn2 = &aMem[pOp->p2];
    pC->movetoTarget = sqlite3VdbeIntValue(pIn2);
    pC->rowidIsValid = 0;
    pC->deferredMoveto = 1;
  }
  break;
}
  

/* Opcode: Found P1 P2 P3 P4 *
**
** If P4==0 then register P3 holds a blob constructed by MakeRecord.  If
** P4>0 then register P3 is the first of P4 registers that form an unpacked
** record.
**
** Cursor P1 is on an index btree.  If the record identified by P3 and P4
** is a prefix of any entry in P1 then a jump is made to P2 and
** P1 is left pointing at the matching entry.
*/
/* Opcode: NotFound P1 P2 P3 P4 *
**
** If P4==0 then register P3 holds a blob constructed by MakeRecord.  If
** P4>0 then register P3 is the first of P4 registers that form an unpacked
** record.
** 
** Cursor P1 is on an index btree.  If the record identified by P3 and P4
** is not the prefix of any entry in P1 then a jump is made to P2.  If P1 
** does contain an entry whose prefix matches the P3/P4 record then control
** falls through to the next instruction and P1 is left pointing at the
** matching entry.
**
** See also: Found, NotExists, IsUnique
*/
case OP_NotFound:       /* jump, in3 */
case OP_Found: {        /* jump, in3 */
  int alreadyExists;
  VdbeCursor *pC;
  int res;
  char *pFree;
  UnpackedRecord *pIdxKey;
  UnpackedRecord r;
  char aTempRec[ROUND8(sizeof(UnpackedRecord)) + sizeof(Mem)*3 + 7];

#ifdef SQLITE_TEST
  sqlite3_found_count++;
#endif

  alreadyExists = 0;
  assert( pOp->p1>=0 && pOp->p1<p->nCursor );
  assert( pOp->p4type==P4_INT32 );
  pC = p->apCsr[pOp->p1];
  assert( pC!=0 );
  pIn3 = &aMem[pOp->p3];
  if( ALWAYS(pC->pCursor!=0) ){

    assert( pC->isTable==0 );
    if( pOp->p4.i>0 ){
      r.pKeyInfo = pC->pKeyInfo;
      r.nField = (u16)pOp->p4.i;
      r.aMem = pIn3;
#ifdef SQLITE_DEBUG
      { int i; for(i=0; i<r.nField; i++) assert( memIsValid(&r.aMem[i]) ); }
#endif
      r.flags = UNPACKED_PREFIX_MATCH;
      pIdxKey = &r;
    }else{
      pIdxKey = sqlite3VdbeAllocUnpackedRecord(
          pC->pKeyInfo, aTempRec, sizeof(aTempRec), &pFree
      ); 
      if( pIdxKey==0 ) goto no_mem;
      assert( pIn3->flags & MEM_Blob );
      assert( (pIn3->flags & MEM_Zero)==0 );  /* zeroblobs already expanded */
      sqlite3VdbeRecordUnpack(pC->pKeyInfo, pIn3->n, pIn3->z, pIdxKey);
      pIdxKey->flags |= UNPACKED_PREFIX_MATCH;
    }
    rc = sqlite3BtreeMovetoUnpacked(pC->pCursor, pIdxKey, 0, 0, &res);
    if( pOp->p4.i==0 ){
      sqlite3DbFree(db, pFree);
    }
    if( rc!=SQLITE_OK ){
      break;
    }
    alreadyExists = (res==0);
    pC->deferredMoveto = 0;
    pC->cacheStatus = CACHE_STALE;
  }
  if( pOp->opcode==OP_Found ){
    if( alreadyExists ) pc = pOp->p2 - 1;
  }else{
    if( !alreadyExists ) pc = pOp->p2 - 1;
  }
  break;
}

/* Opcode: IsUnique P1 P2 P3 P4 *
**
** Cursor P1 is open on an index b-tree - that is to say, a btree which
** no data and where the key are records generated by OP_MakeRecord with
** the list field being the integer ROWID of the entry that the index
** entry refers to.
**
** The P3 register contains an integer record number. Call this record 
** number R. Register P4 is the first in a set of N contiguous registers
** that make up an unpacked index key that can be used with cursor P1.
** The value of N can be inferred from the cursor. N includes the rowid
** value appended to the end of the index record. This rowid value may
** or may not be the same as R.
**
** If any of the N registers beginning with register P4 contains a NULL
** value, jump immediately to P2.
**
** Otherwise, this instruction checks if cursor P1 contains an entry
** where the first (N-1) fields match but the rowid value at the end
** of the index entry is not R. If there is no such entry, control jumps
** to instruction P2. Otherwise, the rowid of the conflicting index
** entry is copied to register P3 and control falls through to the next
** instruction.
**
** See also: NotFound, NotExists, Found
*/
case OP_IsUnique: {        /* jump, in3 */
  u16 ii;
  VdbeCursor *pCx;
  BtCursor *pCrsr;
  u16 nField;
  Mem *aMx;
  UnpackedRecord r;                  /* B-Tree index search key */
  i64 R;                             /* Rowid stored in register P3 */

  pIn3 = &aMem[pOp->p3];
  aMx = &aMem[pOp->p4.i];
  /* Assert that the values of parameters P1 and P4 are in range. */
  assert( pOp->p4type==P4_INT32 );
  assert( pOp->p4.i>0 && pOp->p4.i<=p->nMem );
  assert( pOp->p1>=0 && pOp->p1<p->nCursor );

  /* Find the index cursor. */
  pCx = p->apCsr[pOp->p1];
  assert( pCx->deferredMoveto==0 );
  pCx->seekResult = 0;
  pCx->cacheStatus = CACHE_STALE;
  pCrsr = pCx->pCursor;

  /* If any of the values are NULL, take the jump. */
  nField = pCx->pKeyInfo->nField;
  for(ii=0; ii<nField; ii++){
    if( aMx[ii].flags & MEM_Null ){
      pc = pOp->p2 - 1;
      pCrsr = 0;
      break;
    }
  }
  assert( (aMx[nField].flags & MEM_Null)==0 );

  if( pCrsr!=0 ){
    /* Populate the index search key. */
    r.pKeyInfo = pCx->pKeyInfo;
    r.nField = nField + 1;
    r.flags = UNPACKED_PREFIX_SEARCH;
    r.aMem = aMx;
#ifdef SQLITE_DEBUG
    { int i; for(i=0; i<r.nField; i++) assert( memIsValid(&r.aMem[i]) ); }
#endif

    /* Extract the value of R from register P3. */
    sqlite3VdbeMemIntegerify(pIn3);
    R = pIn3->u.i;

    /* Search the B-Tree index. If no conflicting record is found, jump
    ** to P2. Otherwise, copy the rowid of the conflicting record to
    ** register P3 and fall through to the next instruction.  */
    rc = sqlite3BtreeMovetoUnpacked(pCrsr, &r, 0, 0, &pCx->seekResult);
    if( (r.flags & UNPACKED_PREFIX_SEARCH) || r.rowid==R ){
      pc = pOp->p2 - 1;
    }else{
      pIn3->u.i = r.rowid;
    }
  }
  break;
}

/* Opcode: NotExists P1 P2 P3 * *
**
** Use the content of register P3 as an integer key.  If a record 
** with that key does not exist in table of P1, then jump to P2. 
** If the record does exist, then fall through.  The cursor is left 
** pointing to the record if it exists.
**
** The difference between this operation and NotFound is that this
** operation assumes the key is an integer and that P1 is a table whereas
** NotFound assumes key is a blob constructed from MakeRecord and
** P1 is an index.
**
** See also: Found, NotFound, IsUnique
*/
case OP_NotExists: {        /* jump, in3 */
  VdbeCursor *pC;
  BtCursor *pCrsr;
  int res;
  u64 iKey;

  pIn3 = &aMem[pOp->p3];
  assert( pIn3->flags & MEM_Int );
  assert( pOp->p1>=0 && pOp->p1<p->nCursor );
  pC = p->apCsr[pOp->p1];
  assert( pC!=0 );
  assert( pC->isTable );
  assert( pC->pseudoTableReg==0 );
  pCrsr = pC->pCursor;
  if( ALWAYS(pCrsr!=0) ){
    res = 0;
    iKey = pIn3->u.i;
    rc = sqlite3BtreeMovetoUnpacked(pCrsr, 0, iKey, 0, &res);
    pC->lastRowid = pIn3->u.i;
    pC->rowidIsValid = res==0 ?1:0;
    pC->nullRow = 0;
    pC->cacheStatus = CACHE_STALE;
    pC->deferredMoveto = 0;
    if( res!=0 ){
      pc = pOp->p2 - 1;
      assert( pC->rowidIsValid==0 );
    }
    pC->seekResult = res;
  }else{
    /* This happens when an attempt to open a read cursor on the 
    ** sqlite_master table returns SQLITE_EMPTY.
    */
    pc = pOp->p2 - 1;
    assert( pC->rowidIsValid==0 );
    pC->seekResult = 0;
  }
  break;
}

/* Opcode: Sequence P1 P2 * * *
**
** Find the next available sequence number for cursor P1.
** Write the sequence number into register P2.
** The sequence number on the cursor is incremented after this
** instruction.  
*/
case OP_Sequence: {           /* out2-prerelease */
  assert( pOp->p1>=0 && pOp->p1<p->nCursor );
  assert( p->apCsr[pOp->p1]!=0 );
  pOut->u.i = p->apCsr[pOp->p1]->seqCount++;
  break;
}


/* Opcode: NewRowid P1 P2 P3 * *
**
** Get a new integer record number (a.k.a "rowid") used as the key to a table.
** The record number is not previously used as a key in the database
** table that cursor P1 points to.  The new record number is written
** written to register P2.
**
** If P3>0 then P3 is a register in the root frame of this VDBE that holds 
** the largest previously generated record number. No new record numbers are
** allowed to be less than this value. When this value reaches its maximum, 
** an SQLITE_FULL error is generated. The P3 register is updated with the '
** generated record number. This P3 mechanism is used to help implement the
** AUTOINCREMENT feature.
*/
case OP_NewRowid: {           /* out2-prerelease */
  i64 v;                 /* The new rowid */
  VdbeCursor *pC;        /* Cursor of table to get the new rowid */
  int res;               /* Result of an sqlite3BtreeLast() */
  int cnt;               /* Counter to limit the number of searches */
  Mem *pMem;             /* Register holding largest rowid for AUTOINCREMENT */
  VdbeFrame *pFrame;     /* Root frame of VDBE */

  v = 0;
  res = 0;
  assert( pOp->p1>=0 && pOp->p1<p->nCursor );
  pC = p->apCsr[pOp->p1];
  assert( pC!=0 );
  if( NEVER(pC->pCursor==0) ){
    /* The zero initialization above is all that is needed */
  }else{
    /* The next rowid or record number (different terms for the same
    ** thing) is obtained in a two-step algorithm.
    **
    ** First we attempt to find the largest existing rowid and add one
    ** to that.  But if the largest existing rowid is already the maximum
    ** positive integer, we have to fall through to the second
    ** probabilistic algorithm
    **
    ** The second algorithm is to select a rowid at random and see if
    ** it already exists in the table.  If it does not exist, we have
    ** succeeded.  If the random rowid does exist, we select a new one
    ** and try again, up to 100 times.
    */
    assert( pC->isTable );

#ifdef SQLITE_32BIT_ROWID
#   define MAX_ROWID 0x7fffffff
#else
    /* Some compilers complain about constants of the form 0x7fffffffffffffff.
    ** Others complain about 0x7ffffffffffffffffLL.  The following macro seems
    ** to provide the constant while making all compilers happy.
    */
#   define MAX_ROWID  (i64)( (((u64)0x7fffffff)<<32) | (u64)0xffffffff )
#endif

    if( !pC->useRandomRowid ){
      v = sqlite3BtreeGetCachedRowid(pC->pCursor);
      if( v==0 ){
        rc = sqlite3BtreeLast(pC->pCursor, &res);
        if( rc!=SQLITE_OK ){
          goto abort_due_to_error;
        }
        if( res ){
          v = 1;   /* IMP: R-61914-48074 */
        }else{
          assert( sqlite3BtreeCursorIsValid(pC->pCursor) );
          rc = sqlite3BtreeKeySize(pC->pCursor, &v);
          assert( rc==SQLITE_OK );   /* Cannot fail following BtreeLast() */
          if( v>=MAX_ROWID ){
            pC->useRandomRowid = 1;
          }else{
            v++;   /* IMP: R-29538-34987 */
          }
        }
      }

#ifndef SQLITE_OMIT_AUTOINCREMENT
      if( pOp->p3 ){
        /* Assert that P3 is a valid memory cell. */
        assert( pOp->p3>0 );
        if( p->pFrame ){
          for(pFrame=p->pFrame; pFrame->pParent; pFrame=pFrame->pParent);
          /* Assert that P3 is a valid memory cell. */
          assert( pOp->p3<=pFrame->nMem );
          pMem = &pFrame->aMem[pOp->p3];
        }else{
          /* Assert that P3 is a valid memory cell. */
          assert( pOp->p3<=p->nMem );
          pMem = &aMem[pOp->p3];
          memAboutToChange(p, pMem);
        }
        assert( memIsValid(pMem) );

        REGISTER_TRACE(pOp->p3, pMem);
        sqlite3VdbeMemIntegerify(pMem);
        assert( (pMem->flags & MEM_Int)!=0 );  /* mem(P3) holds an integer */
        if( pMem->u.i==MAX_ROWID || pC->useRandomRowid ){
          rc = SQLITE_FULL;   /* IMP: R-12275-61338 */
          goto abort_due_to_error;
        }
        if( v<pMem->u.i+1 ){
          v = pMem->u.i + 1;
        }
        pMem->u.i = v;
      }
#endif

      sqlite3BtreeSetCachedRowid(pC->pCursor, v<MAX_ROWID ? v+1 : 0);
    }
    if( pC->useRandomRowid ){
      /* IMPLEMENTATION-OF: R-07677-41881 If the largest ROWID is equal to the
      ** largest possible integer (9223372036854775807) then the database
      ** engine starts picking positive candidate ROWIDs at random until
      ** it finds one that is not previously used. */
      assert( pOp->p3==0 );  /* We cannot be in random rowid mode if this is
                             ** an AUTOINCREMENT table. */
      /* on the first attempt, simply do one more than previous */
      v = lastRowid;
      v &= (MAX_ROWID>>1); /* ensure doesn't go negative */
      v++; /* ensure non-zero */
      cnt = 0;
      while(   ((rc = sqlite3BtreeMovetoUnpacked(pC->pCursor, 0, (u64)v,
                                                 0, &res))==SQLITE_OK)
            && (res==0)
            && (++cnt<100)){
        /* collision - try another random rowid */
        sqlite3_randomness(sizeof(v), &v);
        if( cnt<5 ){
          /* try "small" random rowids for the initial attempts */
          v &= 0xffffff;
        }else{
          v &= (MAX_ROWID>>1); /* ensure doesn't go negative */
        }
        v++; /* ensure non-zero */
      }
      if( rc==SQLITE_OK && res==0 ){
        rc = SQLITE_FULL;   /* IMP: R-38219-53002 */
        goto abort_due_to_error;
      }
      assert( v>0 );  /* EV: R-40812-03570 */
    }
    pC->rowidIsValid = 0;
    pC->deferredMoveto = 0;
    pC->cacheStatus = CACHE_STALE;
  }
  pOut->u.i = v;
  break;
}

/* Opcode: Insert P1 P2 P3 P4 P5
**
** Write an entry into the table of cursor P1.  A new entry is
** created if it doesn't already exist or the data for an existing
** entry is overwritten.  The data is the value MEM_Blob stored in register
** number P2. The key is stored in register P3. The key must
** be a MEM_Int.
**
** If the OPFLAG_NCHANGE flag of P5 is set, then the row change count is
** incremented (otherwise not).  If the OPFLAG_LASTROWID flag of P5 is set,
** then rowid is stored for subsequent return by the
** sqlite3_last_insert_rowid() function (otherwise it is unmodified).
**
** If the OPFLAG_USESEEKRESULT flag of P5 is set and if the result of
** the last seek operation (OP_NotExists) was a success, then this
** operation will not attempt to find the appropriate row before doing
** the insert but will instead overwrite the row that the cursor is
** currently pointing to.  Presumably, the prior OP_NotExists opcode
** has already positioned the cursor correctly.  This is an optimization
** that boosts performance by avoiding redundant seeks.
**
** If the OPFLAG_ISUPDATE flag is set, then this opcode is part of an
** UPDATE operation.  Otherwise (if the flag is clear) then this opcode
** is part of an INSERT operation.  The difference is only important to
** the update hook.
**
** Parameter P4 may point to a string containing the table-name, or
** may be NULL. If it is not NULL, then the update-hook 
** (sqlite3.xUpdateCallback) is invoked following a successful insert.
**
** (WARNING/TODO: If P1 is a pseudo-cursor and P2 is dynamically
** allocated, then ownership of P2 is transferred to the pseudo-cursor
** and register P2 becomes ephemeral.  If the cursor is changed, the
** value of register P2 will then change.  Make sure this does not
** cause any problems.)
**
** This instruction only works on tables.  The equivalent instruction
** for indices is OP_IdxInsert.
*/
/* Opcode: InsertInt P1 P2 P3 P4 P5
**
** This works exactly like OP_Insert except that the key is the
** integer value P3, not the value of the integer stored in register P3.
*/
case OP_Insert: 
case OP_InsertInt: {
  Mem *pData;       /* MEM cell holding data for the record to be inserted */
  Mem *pKey;        /* MEM cell holding key  for the record */
  i64 iKey;         /* The integer ROWID or key for the record to be inserted */
  VdbeCursor *pC;   /* Cursor to table into which insert is written */
  int nZero;        /* Number of zero-bytes to append */
  int seekResult;   /* Result of prior seek or 0 if no USESEEKRESULT flag */
  const char *zDb;  /* database name - used by the update hook */
  const char *zTbl; /* Table name - used by the opdate hook */
  int op;           /* Opcode for update hook: SQLITE_UPDATE or SQLITE_INSERT */

  pData = &aMem[pOp->p2];
  assert( pOp->p1>=0 && pOp->p1<p->nCursor );
  assert( memIsValid(pData) );
  pC = p->apCsr[pOp->p1];
  assert( pC!=0 );
  assert( pC->pCursor!=0 );
  assert( pC->pseudoTableReg==0 );
  assert( pC->isTable );
  REGISTER_TRACE(pOp->p2, pData);

  if( pOp->opcode==OP_Insert ){
    pKey = &aMem[pOp->p3];
    assert( pKey->flags & MEM_Int );
    assert( memIsValid(pKey) );
    REGISTER_TRACE(pOp->p3, pKey);
    iKey = pKey->u.i;
  }else{
    assert( pOp->opcode==OP_InsertInt );
    iKey = pOp->p3;
  }

  if( pOp->p5 & OPFLAG_NCHANGE ) p->nChange++;
  if( pOp->p5 & OPFLAG_LASTROWID ) db->lastRowid = lastRowid = iKey;
  if( pData->flags & MEM_Null ){
    pData->z = 0;
    pData->n = 0;
  }else{
    assert( pData->flags & (MEM_Blob|MEM_Str) );
  }
  seekResult = ((pOp->p5 & OPFLAG_USESEEKRESULT) ? pC->seekResult : 0);
  if( pData->flags & MEM_Zero ){
    nZero = pData->u.nZero;
  }else{
    nZero = 0;
  }
  sqlite3BtreeSetCachedRowid(pC->pCursor, 0);
  rc = sqlite3BtreeInsert(pC->pCursor, 0, iKey,
                          pData->z, pData->n, nZero,
                          pOp->p5 & OPFLAG_APPEND, seekResult
  );
  pC->rowidIsValid = 0;
  pC->deferredMoveto = 0;
  pC->cacheStatus = CACHE_STALE;

  /* Invoke the update-hook if required. */
  if( rc==SQLITE_OK && db->xUpdateCallback && pOp->p4.z ){
    zDb = db->aDb[pC->iDb].zName;
    zTbl = pOp->p4.z;
    op = ((pOp->p5 & OPFLAG_ISUPDATE) ? SQLITE_UPDATE : SQLITE_INSERT);
    assert( pC->isTable );
    db->xUpdateCallback(db->pUpdateArg, op, zDb, zTbl, iKey);
    assert( pC->iDb>=0 );
  }
  break;
}

/* Opcode: Delete P1 P2 * P4 *
**
** Delete the record at which the P1 cursor is currently pointing.
**
** The cursor will be left pointing at either the next or the previous
** record in the table. If it is left pointing at the next record, then
** the next Next instruction will be a no-op.  Hence it is OK to delete
** a record from within an Next loop.
**
** If the OPFLAG_NCHANGE flag of P2 is set, then the row change count is
** incremented (otherwise not).
**
** P1 must not be pseudo-table.  It has to be a real table with
** multiple rows.
**
** If P4 is not NULL, then it is the name of the table that P1 is
** pointing to.  The update hook will be invoked, if it exists.
** If P4 is not NULL then the P1 cursor must have been positioned
** using OP_NotFound prior to invoking this opcode.
*/
case OP_Delete: {
  i64 iKey;
  VdbeCursor *pC;

  iKey = 0;
  assert( pOp->p1>=0 && pOp->p1<p->nCursor );
  pC = p->apCsr[pOp->p1];
  assert( pC!=0 );
  assert( pC->pCursor!=0 );  /* Only valid for real tables, no pseudotables */

  /* If the update-hook will be invoked, set iKey to the rowid of the
  ** row being deleted.
  */
  if( db->xUpdateCallback && pOp->p4.z ){
    assert( pC->isTable );
    assert( pC->rowidIsValid );  /* lastRowid set by previous OP_NotFound */
    iKey = pC->lastRowid;
  }

  /* The OP_Delete opcode always follows an OP_NotExists or OP_Last or
  ** OP_Column on the same table without any intervening operations that
  ** might move or invalidate the cursor.  Hence cursor pC is always pointing
  ** to the row to be deleted and the sqlite3VdbeCursorMoveto() operation
  ** below is always a no-op and cannot fail.  We will run it anyhow, though,
  ** to guard against future changes to the code generator.
  **/
  assert( pC->deferredMoveto==0 );
  rc = sqlite3VdbeCursorMoveto(pC);
  if( NEVER(rc!=SQLITE_OK) ) goto abort_due_to_error;

  sqlite3BtreeSetCachedRowid(pC->pCursor, 0);
  rc = sqlite3BtreeDelete(pC->pCursor);
  pC->cacheStatus = CACHE_STALE;

  /* Invoke the update-hook if required. */
  if( rc==SQLITE_OK && db->xUpdateCallback && pOp->p4.z ){
    const char *zDb = db->aDb[pC->iDb].zName;
    const char *zTbl = pOp->p4.z;
    db->xUpdateCallback(db->pUpdateArg, SQLITE_DELETE, zDb, zTbl, iKey);
    assert( pC->iDb>=0 );
  }
  if( pOp->p2 & OPFLAG_NCHANGE ) p->nChange++;
  break;
}
/* Opcode: ResetCount * * * * *
**
** The value of the change counter is copied to the database handle
** change counter (returned by subsequent calls to sqlite3_changes()).
** Then the VMs internal change counter resets to 0.
** This is used by trigger programs.
*/
case OP_ResetCount: {
  sqlite3VdbeSetChanges(db, p->nChange);
  p->nChange = 0;
  break;
}

/* Opcode: SorterCompare P1 P2 P3
**
** P1 is a sorter cursor. This instruction compares the record blob in 
** register P3 with the entry that the sorter cursor currently points to.
** If, excluding the rowid fields at the end, the two records are a match,
** fall through to the next instruction. Otherwise, jump to instruction P2.
*/
case OP_SorterCompare: {
  VdbeCursor *pC;
  int res;

  pC = p->apCsr[pOp->p1];
  assert( isSorter(pC) );
  pIn3 = &aMem[pOp->p3];
  rc = sqlite3VdbeSorterCompare(pC, pIn3, &res);
  if( res ){
    pc = pOp->p2-1;
  }
  break;
};

/* Opcode: SorterData P1 P2 * * *
**
** Write into register P2 the current sorter data for sorter cursor P1.
*/
case OP_SorterData: {
  VdbeCursor *pC;
#ifndef SQLITE_OMIT_MERGE_SORT
  pOut = &aMem[pOp->p2];
  pC = p->apCsr[pOp->p1];
  assert( pC->isSorter );
  rc = sqlite3VdbeSorterRowkey(pC, pOut);
#else
  pOp->opcode = OP_RowKey;
  pc--;
#endif
  break;
}

/* Opcode: RowData P1 P2 * * *
**
** Write into register P2 the complete row data for cursor P1.
** There is no interpretation of the data.  
** It is just copied onto the P2 register exactly as 
** it is found in the database file.
**
** If the P1 cursor must be pointing to a valid row (not a NULL row)
** of a real table, not a pseudo-table.
*/
/* Opcode: RowKey P1 P2 * * *
**
** Write into register P2 the complete row key for cursor P1.
** There is no interpretation of the data.  
** The key is copied onto the P3 register exactly as 
** it is found in the database file.
**
** If the P1 cursor must be pointing to a valid row (not a NULL row)
** of a real table, not a pseudo-table.
*/
case OP_RowKey:
case OP_RowData: {
  VdbeCursor *pC;
  BtCursor *pCrsr;
  u32 n;
  i64 n64;

  pOut = &aMem[pOp->p2];
  memAboutToChange(p, pOut);

  /* Note that RowKey and RowData are really exactly the same instruction */
  assert( pOp->p1>=0 && pOp->p1<p->nCursor );
  pC = p->apCsr[pOp->p1];
  assert( pC->isSorter==0 );
  assert( pC->isTable || pOp->opcode!=OP_RowData );
  assert( pC->isIndex || pOp->opcode==OP_RowData );
  assert( pC!=0 );
  assert( pC->nullRow==0 );
  assert( pC->pseudoTableReg==0 );
  assert( pC->pCursor!=0 );
  pCrsr = pC->pCursor;
  assert( sqlite3BtreeCursorIsValid(pCrsr) );

  /* The OP_RowKey and OP_RowData opcodes always follow OP_NotExists or
  ** OP_Rewind/Op_Next with no intervening instructions that might invalidate
  ** the cursor.  Hence the following sqlite3VdbeCursorMoveto() call is always
  ** a no-op and can never fail.  But we leave it in place as a safety.
  */
  assert( pC->deferredMoveto==0 );
  rc = sqlite3VdbeCursorMoveto(pC);
  if( NEVER(rc!=SQLITE_OK) ) goto abort_due_to_error;

  if( pC->isIndex ){
    assert( !pC->isTable );
    VVA_ONLY(rc =) sqlite3BtreeKeySize(pCrsr, &n64);
    assert( rc==SQLITE_OK );    /* True because of CursorMoveto() call above */
    if( n64>db->aLimit[SQLITE_LIMIT_LENGTH] ){
      goto too_big;
    }
    n = (u32)n64;
  }else{
    VVA_ONLY(rc =) sqlite3BtreeDataSize(pCrsr, &n);
    assert( rc==SQLITE_OK );    /* DataSize() cannot fail */
    if( n>(u32)db->aLimit[SQLITE_LIMIT_LENGTH] ){
      goto too_big;
    }
  }
  if( sqlite3VdbeMemGrow(pOut, n, 0) ){
    goto no_mem;
  }
  pOut->n = n;
  MemSetTypeFlag(pOut, MEM_Blob);
  if( pC->isIndex ){
    rc = sqlite3BtreeKey(pCrsr, 0, n, pOut->z);
  }else{
    rc = sqlite3BtreeData(pCrsr, 0, n, pOut->z);
  }
  pOut->enc = SQLITE_UTF8;  /* In case the blob is ever cast to text */
  UPDATE_MAX_BLOBSIZE(pOut);
  break;
}

/* Opcode: Rowid P1 P2 * * *
**
** Store in register P2 an integer which is the key of the table entry that
** P1 is currently point to.
**
** P1 can be either an ordinary table or a virtual table.  There used to
** be a separate OP_VRowid opcode for use with virtual tables, but this
** one opcode now works for both table types.
*/
case OP_Rowid: {                 /* out2-prerelease */
  VdbeCursor *pC;
  i64 v;
  sqlite3_vtab *pVtab;
  const sqlite3_module *pModule;

  assert( pOp->p1>=0 && pOp->p1<p->nCursor );
  pC = p->apCsr[pOp->p1];
  assert( pC!=0 );
  assert( pC->pseudoTableReg==0 );
  if( pC->nullRow ){
    pOut->flags = MEM_Null;
    break;
  }else if( pC->deferredMoveto ){
    v = pC->movetoTarget;
#ifndef SQLITE_OMIT_VIRTUALTABLE
  }else if( pC->pVtabCursor ){
    pVtab = pC->pVtabCursor->pVtab;
    pModule = pVtab->pModule;
    assert( pModule->xRowid );
    rc = pModule->xRowid(pC->pVtabCursor, &v);
    importVtabErrMsg(p, pVtab);
#endif /* SQLITE_OMIT_VIRTUALTABLE */
  }else{
    assert( pC->pCursor!=0 );
    rc = sqlite3VdbeCursorMoveto(pC);
    if( rc ) goto abort_due_to_error;
    if( pC->rowidIsValid ){
      v = pC->lastRowid;
    }else{
      rc = sqlite3BtreeKeySize(pC->pCursor, &v);
      assert( rc==SQLITE_OK );  /* Always so because of CursorMoveto() above */
    }
  }
  pOut->u.i = v;
  break;
}

/* Opcode: NullRow P1 * * * *
**
** Move the cursor P1 to a null row.  Any OP_Column operations
** that occur while the cursor is on the null row will always
** write a NULL.
*/
case OP_NullRow: {
  VdbeCursor *pC;

  assert( pOp->p1>=0 && pOp->p1<p->nCursor );
  pC = p->apCsr[pOp->p1];
  assert( pC!=0 );
  pC->nullRow = 1;
  pC->rowidIsValid = 0;
  assert( pC->pCursor || pC->pVtabCursor );
  if( pC->pCursor ){
    sqlite3BtreeClearCursor(pC->pCursor);
  }
  break;
}

/* Opcode: Last P1 P2 * * *
**
** The next use of the Rowid or Column or Next instruction for P1 
** will refer to the last entry in the database table or index.
** If the table or index is empty and P2>0, then jump immediately to P2.
** If P2 is 0 or if the table or index is not empty, fall through
** to the following instruction.
*/
case OP_Last: {        /* jump */
  VdbeCursor *pC;
  BtCursor *pCrsr;
  int res;

  assert( pOp->p1>=0 && pOp->p1<p->nCursor );
  pC = p->apCsr[pOp->p1];
  assert( pC!=0 );
  pCrsr = pC->pCursor;
  res = 0;
  if( ALWAYS(pCrsr!=0) ){
    rc = sqlite3BtreeLast(pCrsr, &res);
  }
  pC->nullRow = (u8)res;
  pC->deferredMoveto = 0;
  pC->rowidIsValid = 0;
  pC->cacheStatus = CACHE_STALE;
  if( pOp->p2>0 && res ){
    pc = pOp->p2 - 1;
  }
  break;
}


/* Opcode: Sort P1 P2 * * *
**
** This opcode does exactly the same thing as OP_Rewind except that
** it increments an undocumented global variable used for testing.
**
** Sorting is accomplished by writing records into a sorting index,
** then rewinding that index and playing it back from beginning to
** end.  We use the OP_Sort opcode instead of OP_Rewind to do the
** rewinding so that the global variable will be incremented and
** regression tests can determine whether or not the optimizer is
** correctly optimizing out sorts.
*/
case OP_SorterSort:    /* jump */
#ifdef SQLITE_OMIT_MERGE_SORT
  pOp->opcode = OP_Sort;
#endif
case OP_Sort: {        /* jump */
#ifdef SQLITE_TEST
  sqlite3_sort_count++;
  sqlite3_search_count--;
#endif
  p->aCounter[SQLITE_STMTSTATUS_SORT-1]++;
  /* Fall through into OP_Rewind */
}
/* Opcode: Rewind P1 P2 * * *
**
** The next use of the Rowid or Column or Next instruction for P1 
** will refer to the first entry in the database table or index.
** If the table or index is empty and P2>0, then jump immediately to P2.
** If P2 is 0 or if the table or index is not empty, fall through
** to the following instruction.
*/
/*移动当前游标到表或索引的第一条记录.
**如果表为空且p2>0,则跳到p2处;如果p2为0且表不空，则执行下一条指令.
*/
case OP_Rewind: {        /* jump */
  VdbeCursor *pC;
  BtCursor *pCrsr;
  int res;

  assert( pOp->p1>=0 && pOp->p1<p->nCursor );
  pC = p->apCsr[pOp->p1];
  assert( pC!=0 );
  assert( pC->isSorter==(pOp->opcode==OP_SorterSort) );
  res = 1;
  if( isSorter(pC) ){
    rc = sqlite3VdbeSorterRewind(db, pC, &res);
  }else{
    pCrsr = pC->pCursor;
    assert( pCrsr );
    rc = sqlite3BtreeFirst(pCrsr, &res);
    pC->atFirst = res==0 ?1:0;
    pC->deferredMoveto = 0;
    pC->cacheStatus = CACHE_STALE;
    pC->rowidIsValid = 0;
  }
  pC->nullRow = (u8)res;
  assert( pOp->p2>0 && pOp->p2<p->nOp );
  if( res ){
    pc = pOp->p2 - 1;
  }
  break;
}

/* Opcode: Next P1 P2 * P4 P5
**
** Advance cursor P1 so that it points to the next key/data pair in its
** table or index.  If there are no more key/value pairs then fall through
** to the following instruction.  But if the cursor advance was successful,
** jump immediately to P2.
**
** The P1 cursor must be for a real table, not a pseudo-table.
**
** P4 is always of type P4_ADVANCE. The function pointer points to
** sqlite3BtreeNext().
**
** If P5 is positive and the jump is taken, then event counter
** number P5-1 in the prepared statement is incremented.
**
** See also: Prev
*/
/* Opcode: Prev P1 P2 * * P5
**
** Back up cursor P1 so that it points to the previous key/data pair in its
** table or index.  If there is no previous key/value pairs then fall through
** to the following instruction.  But if the cursor backup was successful,
** jump immediately to P2.
**
** The P1 cursor must be for a real table, not a pseudo-table.
**
** P4 is always of type P4_ADVANCE. The function pointer points to
** sqlite3BtreePrevious().
**
** If P5 is positive and the jump is taken, then event counter
** number P5-1 in the prepared statement is incremented.
*/
case OP_SorterNext:    /* jump */
#ifdef SQLITE_OMIT_MERGE_SORT
  pOp->opcode = OP_Next;
#endif
case OP_Prev:          /* jump */
case OP_Next: {        /* jump */
  VdbeCursor *pC;
  int res;

  CHECK_FOR_INTERRUPT;
  assert( pOp->p1>=0 && pOp->p1<p->nCursor );
  assert( pOp->p5<=ArraySize(p->aCounter) );
  pC = p->apCsr[pOp->p1];
  if( pC==0 ){
    break;  /* See ticket #2273 */
  }
  assert( pC->isSorter==(pOp->opcode==OP_SorterNext) );
  if( isSorter(pC) ){
    assert( pOp->opcode==OP_SorterNext );
    rc = sqlite3VdbeSorterNext(db, pC, &res);
  }else{
    res = 1;
    assert( pC->deferredMoveto==0 );
    assert( pC->pCursor );
    assert( pOp->opcode!=OP_Next || pOp->p4.xAdvance==sqlite3BtreeNext );
    assert( pOp->opcode!=OP_Prev || pOp->p4.xAdvance==sqlite3BtreePrevious );
    rc = pOp->p4.xAdvance(pC->pCursor, &res);
  }
  pC->nullRow = (u8)res;
  pC->cacheStatus = CACHE_STALE;
  if( res==0 ){
    pc = pOp->p2 - 1;
    if( pOp->p5 ) p->aCounter[pOp->p5-1]++;
#ifdef SQLITE_TEST
    sqlite3_search_count++;
#endif
  }
  pC->rowidIsValid = 0;
  break;
}

/* Opcode: IdxInsert P1 P2 P3 * P5
**
** Register P2 holds an SQL index key made using the
** MakeRecord instructions.  This opcode writes that key
** into the index P1.  Data for the entry is nil.
**
** P3 is a flag that provides a hint to the b-tree layer that this
** insert is likely to be an append.
**
** This instruction only works for indices.  The equivalent instruction
** for tables is OP_Insert.
*/
case OP_SorterInsert:       /* in2 */
#ifdef SQLITE_OMIT_MERGE_SORT
  pOp->opcode = OP_IdxInsert;
#endif
case OP_IdxInsert: {        /* in2 */
  VdbeCursor *pC;
  BtCursor *pCrsr;
  int nKey;
  const char *zKey;

  assert( pOp->p1>=0 && pOp->p1<p->nCursor );
  pC = p->apCsr[pOp->p1];
  assert( pC!=0 );
  assert( pC->isSorter==(pOp->opcode==OP_SorterInsert) );
  pIn2 = &aMem[pOp->p2];
  assert( pIn2->flags & MEM_Blob );
  pCrsr = pC->pCursor;
  if( ALWAYS(pCrsr!=0) ){
    assert( pC->isTable==0 );
    rc = ExpandBlob(pIn2);
    if( rc==SQLITE_OK ){
      if( isSorter(pC) ){
        rc = sqlite3VdbeSorterWrite(db, pC, pIn2);
      }else{
        nKey = pIn2->n;
        zKey = pIn2->z;
        rc = sqlite3BtreeInsert(pCrsr, zKey, nKey, "", 0, 0, pOp->p3, 
            ((pOp->p5 & OPFLAG_USESEEKRESULT) ? pC->seekResult : 0)
            );
        assert( pC->deferredMoveto==0 );
        pC->cacheStatus = CACHE_STALE;
      }
    }
  }
  break;
}

/* Opcode: Destroy P1 P2 P3 * *
**
**
** Delete an entire database table or index whose root page in the database
** file is given by P1.删除整个数据库表或在寄存器P1的数据库中文件的索引的根页。
**
** The table being destroyed is in the main database file if P3==0.  If
** P3==1 then the table to be clear is in the auxiliary database file
** that is used to store tables create using CREATE TEMPORARY TABLE.
**如果P3 = = 0,主数据库文件中的表将被破坏。
**如果P3 = = 1那么辅助数据库文件中的表将被清除，
这是用于存储表创建使用创建临时表。

 
** If AUTOVACUUM is enabled then it is possible that another root page
** might be moved into the newly deleted root page in order to keep all
** root pages contiguous at the beginning of the database.  The former
** value of the root page that moved - its value before the move occurred -
** is stored in register P2.  
**如果启用了空间然后有可能是另一个根页面
**可能进入新页面删除根为了保持根页相邻的数据库。
**前根页面的价值——其价值在移动之前发生存储在寄存器P2。



** If no page movement was required (because the table being dropped was already 
** the last one in the database) then a zero is stored in register P2.
** If AUTOVACUUM is disabled then a zero is stored in register P2.
**如果不需要页面运动(因为表被删除数据库中已经是最后一个)则存储在寄存器P2 0。
如果禁用AUTOVACUUM那么零存储在寄3存器P2。
** See also: Clear
*/
case OP_Destroy: {     /* out2-prerelease */
  int iMoved;
  int iCnt;
  Vdbe *pVdbe;
  int iDb;
#ifndef SQLITE_OMIT_VIRTUALTABLE
  iCnt = 0;
  for(pVdbe=db->pVdbe; pVdbe; pVdbe = pVdbe->pNext){
    if( pVdbe->magic==VDBE_MAGIC_RUN && pVdbe->inVtabMethod<2 && pVdbe->pc>=0 ){
      iCnt++;
    }
  }
#else
  iCnt = db->activeVdbeCnt;
#endif
  pOut->flags = MEM_Null;
  if( iCnt>1 ){
    rc = SQLITE_LOCKED;
    p->errorAction = OE_Abort;
  }else{
    iDb = pOp->p3;
    assert( iCnt==1 );
    assert( (p->btreeMask & (((yDbMask)1)<<iDb))!=0 );
    rc = sqlite3BtreeDropTable(db->aDb[iDb].pBt, pOp->p1, &iMoved);
    pOut->flags = MEM_Int;
    pOut->u.i = iMoved;
#ifndef SQLITE_OMIT_AUTOVACUUM
    if( rc==SQLITE_OK && iMoved!=0 ){
      sqlite3RootPageMoved(db, iDb, iMoved, pOp->p1);
      /* All OP_Destroy operations occur on the same btree */
      assert( resetSchemaOnFault==0 || resetSchemaOnFault==iDb+1 );
      resetSchemaOnFault = iDb+1;
    }
#endif
  }
  break;
}

/* Opcode: Clear P1 P2 P3
**
** Delete all contents of the database table or index whose root page
** in the database file is given by P1.  But, unlike Destroy, do not
** remove the table or index from the database file.
**删除所有数据库表的内容或寄存器P1的数据库中文件的索引的根页。但是,与破坏,不要从数据库中删除表或索引文件。

** The table being clear is in the main database file if P2==0.  If
** P2==1 then the table to be clear is in the auxiliary database file
** that is used to store tables create using CREATE TEMPORARY TABLE.
**明确的表是在主数据库文件如果P2 = = 0。如果P2 = = 1那么表必须清楚的是在辅助数据库文件这是用于存储表创建使用创建临时表。

** If the P3 value is non-zero, then the table referred to must be an
** intkey table (an SQL table, not an index). In this case the row change 
** count is incremented by the number of rows in the table being cleared. 
如果P3值为非0,那么表必须是一个引用intkey表(SQL表,而不是一个索引)。在这种情况下,行修改数增加的行数的表被清除。

** If P3 is greater than zero, then the value stored in register P3 is
** also incremented by the number of rows in the table being cleared.
**如果P3大于零,则该值存储在寄存器P3也增加了清除表中的行数。

** See also: Destroy
*/
case OP_Clear: {
  int nChange;
 
  nChange = 0;
  assert( (p->btreeMask & (((yDbMask)1)<<pOp->p2))!=0 );
  rc = sqlite3BtreeClearTable(
      db->aDb[pOp->p2].pBt, pOp->p1, (pOp->p3 ? &nChange : 0)
  );
  if( pOp->p3 ){
    p->nChange += nChange;
    if( pOp->p3>0 ){
      assert( memIsValid(&aMem[pOp->p3]) );
      memAboutToChange(p, &aMem[pOp->p3]);
      aMem[pOp->p3].u.i += nChange;
    }
  }
  break;
}

/* Opcode: CreateTable P1 P2 * * *
**
** Allocate a new table in the main database file if P1==0 or in the
** auxiliary database file if P1==1 or in an attached database if
** P1>1.  Write the root page number of the new table into
** register P2

分配一个新表在主数据库文件如果P1 = = 0或辅助数据库文件如果P1 = = 1或者附加数据库中P1 > 1。把新表的根页号写入寄存器P2
**
** The difference between a table and an index is this:  A table must
** have a 4-byte integer key and can have arbitrary data.  An index
** has an arbitrary key but no data.
**表和索引的区别是:一个表有一个4字节的整数键和可以任意数据。一个索引任意键但没有数据。

** See also: CreateIndex
*/
/* Opcode: CreateIndex P1 P2 * * *
** out2-prerelease
** Allocate a new index in the main database file if P1==0 or in the
** auxiliary database file if P1==1 or in an attached database if
** P1>1.  Write the root page number of the new table into
** register P2.
分配一个新的索引在主数据库文件如果P1 = = 0或辅助数据库文件如果P1 = = 1或者附加数据库中
P1 > 1。把新表的根页号写入寄存器P2。
**
** See documentation on OP_CreateTable for additional information.
*/
case OP_CreateIndex:            /* out2-prerelease */
case OP_CreateTable: {          /* out2-prerelease */
  int pgno;
  int flags;
  Db *pDb;

  pgno = 0;
  assert( pOp->p1>=0 && pOp->p1<db->nDb );
  assert( (p->btreeMask & (((yDbMask)1)<<pOp->p1))!=0 );
  pDb = &db->aDb[pOp->p1];
  assert( pDb->pBt!=0 );
  if( pOp->opcode==OP_CreateTable ){
    /* flags = BTREE_INTKEY; */
    flags = BTREE_INTKEY;
  }else{
    flags = BTREE_BLOBKEY;
  }
  rc = sqlite3BtreeCreateTable(pDb->pBt, &pgno, flags);
  pOut->u.i = pgno;
  break;
}

/* Opcode: ParseSchema P1 * * P4 *
**
** Read and parse all entries from the SQLITE_MASTER table of database P1
** that match the WHERE clause P4. 从SQLITE_MASTER表读取和解析所有条目的数据库P1
相匹配的WHERE子句P4。
**
** This opcode invokes the parser to create a new virtual machine,
** then runs the new virtual machine.  It is thus a re-entrant opcode.
这个操作码调用解析器创建一个新的虚拟机,然后运行新的虚拟机。因此一个凹角操作码


*/
case OP_ParseSchema: {
  int iDb;
  const char *zMaster;
  char *zSql;
  InitData initData;

  /* Any prepared statement that invokes this opcode will hold mutexes
  ** on every btree.  This is a prerequisite for invoking 
  任何事先准备好的声明中调用这个操作码将在每个btree互斥。这是一个调用的先决条件
  ** sqlite3InitCallback().
  */
#ifdef SQLITE_DEBUG
  for(iDb=0; iDb<db->nDb; iDb++){
    assert( iDb==1 || sqlite3BtreeHoldsMutex(db->aDb[iDb].pBt) );
  }
#endif

  iDb = pOp->p1;
  assert( iDb>=0 && iDb<db->nDb );
  assert( DbHasProperty(db, iDb, DB_SchemaLoaded) );
  /* Used to be a conditional */ {
    zMaster = SCHEMA_TABLE(iDb);
    initData.db = db;
    initData.iDb = pOp->p1;
    initData.pzErrMsg = &p->zErrMsg;
    zSql = sqlite3MPrintf(db,
       "SELECT name, rootpage, sql FROM '%q'.%s WHERE %s ORDER BY rowid",
       db->aDb[iDb].zName, zMaster, pOp->p4.z);
    if( zSql==0 ){
      rc = SQLITE_NOMEM;
    }else{
      assert( db->init.busy==0 );
      db->init.busy = 1;
      initData.rc = SQLITE_OK;
      assert( !db->mallocFailed );
      rc = sqlite3_exec(db, zSql, sqlite3InitCallback, &initData, 0);
      if( rc==SQLITE_OK ) rc = initData.rc;
      sqlite3DbFree(db, zSql);
      db->init.busy = 0;
    }
  }
  if( rc ) sqlite3ResetAllSchemasOfConnection(db);
  if( rc==SQLITE_NOMEM ){
    goto no_mem;
  }
  break;  
}

#if !defined(SQLITE_OMIT_ANALYZE)
/* Opcode: LoadAnalysis P1 * * * *
**
** Read the sqlite_stat1 table for database P1 and load the content
** of that table into the internal index hash table.  This will cause
** the analysis to be used when preparing all subsequent queries.
读数据库P1的表sqlite_stat1和把那张表的内容加载到内部索引hash表。这将导致分析准备所有后续查询时使用。

*/
case OP_LoadAnalysis: {
  assert( pOp->p1>=0 && pOp->p1<db->nDb );
  rc = sqlite3AnalysisLoad(db, pOp->p1);
  break;  
}
#endif /* !defined(SQLITE_OMIT_ANALYZE) */

/* Opcode: DropTable P1 * * P4 *
**
** Remove the internal (in-memory) data structures that describe
** the table named P4 in database P1.  This is called after a table
** is dropped in order to keep the internal representation of the
** schema consistent with what is on disk.
拆卸内部的描述数据库P1的P4表的数据结构(内存)。这就是以降序的索引命名，是为了保持内部表示的模式与什么是磁盘上的一致。
*/
case OP_DropTable: {
  sqlite3UnlinkAndDeleteTable(db, pOp->p1, pOp->p4.z);
  break;
}

/* Opcode: DropIndex P1 * * P4 *
**
** Remove the internal (in-memory) data structures that describe
** the index named P4 in database P1.  This is called after an index
** is dropped in order to keep the internal representation of the
** schema consistent with what is on disk.
拆卸内部的描述数据库P1的指数P4的数据结构(内存)。这就是以降序的索引命名，是为了保持内部表示的模式与什么是磁盘上的一致。
*/
case OP_DropIndex: {
  sqlite3UnlinkAndDeleteIndex(db, pOp->p1, pOp->p4.z);
  break;
}

/* Opcode: DropTrigger P1 * * P4 *
**
** Remove the internal (in-memory) data structures that describe
** the trigger named P4 in database P1.  This is called after a trigger
** is dropped in order to keep the internal representation of the
** schema consistent with what is on disk.
拆卸内部的描述数据库P1的P4触发器的数据结构(内存)。这就是以降序的索引命名，是为了保持内部表示的模式与什么是磁盘上的一致。

*/
case OP_DropTrigger: {
  sqlite3UnlinkAndDeleteTrigger(db, pOp->p1, pOp->p4.z);
  break;
}


#ifndef SQLITE_OMIT_INTEGRITY_CHECK
/* Opcode: IntegrityCk P1 P2 P3 * P5
**
** Do an analysis of the currently open database.  Store in
** register P1 the text of an error message describing any problems.
** If no problems are found, store a NULL in register P1.
**对当前打开数据库做一个分析，存储在寄存器P1中的描述的任何问题错误消息的文本。如果没有发现问题,存储空值在寄存器P1。

** The register P3 contains the maximum number of allowed errors.
** At most reg(P3) errors will be reported.
** In other words, the analysis stops as soon as reg(P1) errors are 
** seen.  Reg(P1) is updated with the number of errors remaining.
**寄存器P3包含可允许的最大数量错误。
寄存器(P3)中的最多的错误将被报告。
换句话说,尽快分析停止寄存器(P1)的错误。
寄存器(P1)更新剩余的许多错误。


** The root page numbers of all tables in the database are integer
** stored in reg(P1), reg(P1+1), reg(P1+2), ....  There are P2 tables
** total.
**数据库中的所有表的根页码是整数
存储在寄存器(P1),寄存器(P1 + 1),寄存器(P1 + 2),....总共有P2表。

** If P5 is not zero, the check is done on the auxiliary database
** file, not the main database file.
**如果P5不为零,辅助数据库文件的检查结束,而不是主要的数据库文件。
** This opcode is used to implement the integrity_check pragma.
这个操作码是用来实现integrity_check程序的编译指示。
*/
case OP_IntegrityCk: {
  int nRoot;      /* Number of tables to check.  (Number of root pages.) */
  int *aRoot;     /* Array of rootpage numbers for tables to be checked */
  int j;          /* Loop counter */
  int nErr;       /* Number of errors reported */
  char *z;        /* Text of the error report */
  Mem *pnErr;     /* Register keeping track of errors remaining */
  
  nRoot = pOp->p2;
  assert( nRoot>0 );
  aRoot = sqlite3DbMallocRaw(db, sizeof(int)*(nRoot+1) );
  if( aRoot==0 ) goto no_mem;
  assert( pOp->p3>0 && pOp->p3<=p->nMem );
  pnErr = &aMem[pOp->p3];
  assert( (pnErr->flags & MEM_Int)!=0 );
  assert( (pnErr->flags & (MEM_Str|MEM_Blob))==0 );
  pIn1 = &aMem[pOp->p1];
  for(j=0; j<nRoot; j++){
    aRoot[j] = (int)sqlite3VdbeIntValue(&pIn1[j]);
  }
  aRoot[j] = 0;
  assert( pOp->p5<db->nDb );
  assert( (p->btreeMask & (((yDbMask)1)<<pOp->p5))!=0 );
  z = sqlite3BtreeIntegrityCheck(db->aDb[pOp->p5].pBt, aRoot, nRoot,
                                 (int)pnErr->u.i, &nErr);
  sqlite3DbFree(db, aRoot);
  pnErr->u.i -= nErr;
  sqlite3VdbeMemSetNull(pIn1);
  if( nErr==0 ){
    assert( z==0 );
  }else if( z==0 ){
    goto no_mem;
  }else{
    sqlite3VdbeMemSetStr(pIn1, z, -1, SQLITE_UTF8, sqlite3_free);
  }
  UPDATE_MAX_BLOBSIZE(pIn1);
  sqlite3VdbeChangeEncoding(pIn1, encoding);
  break;
}
#endif /* SQLITE_OMIT_INTEGRITY_CHECK */

/* Opcode: RowSetAdd P1 P2 * * *
**
** Insert the integer value held by register P2 into a boolean index
** held in register P1.
**将寄存器P2持有的整数值插入到寄存器P1的一个布尔指数。
** An assertion fails if P2 is not an integer.
 如果P2不是整数这个断言将会失败。
*/
case OP_RowSetAdd: {       /* in1, in2 */
  pIn1 = &aMem[pOp->p1];
  pIn2 = &aMem[pOp->p2];
  assert( (pIn2->flags & MEM_Int)!=0 );
  if( (pIn1->flags & MEM_RowSet)==0 ){
    sqlite3VdbeMemSetRowSet(pIn1);
    if( (pIn1->flags & MEM_RowSet)==0 ) goto no_mem;
  }
  sqlite3RowSetInsert(pIn1->u.pRowSet, pIn2->u.i);
  break;
}

/* Opcode: RowSetRead P1 P2 P3 * *
**
** Extract the smallest value from boolean index P1 and put that value into
** register P3.  Or, if boolean index P1 is initially empty, leave P3
** unchanged and jump to instruction P2.
从布尔指数P1里提取出最小值,把该值放进寄存器P3。或者,如果布尔指数P1最初是空的,离开P3不变然后跳转到指令P2。
*/
case OP_RowSetRead: {       /* jump, in1, out3 */
  i64 val;
  CHECK_FOR_INTERRUPT;
  pIn1 = &aMem[pOp->p1];
  if( (pIn1->flags & MEM_RowSet)==0 
   || sqlite3RowSetNext(pIn1->u.pRowSet, &val)==0
  ){
    /* The boolean index is empty */
    sqlite3VdbeMemSetNull(pIn1);
    pc = pOp->p2 - 1;
  }else{
    /* A value was pulled from the index */
    sqlite3VdbeMemSetInt64(&aMem[pOp->p3], val);
  }
  break;
}

/* Opcode: RowSetTest P1 P2 P3 P4
**
** Register P3 is assumed to hold a 64-bit integer value. If register P1
** contains a RowSet object and that RowSet object contains
** the value held in P3, jump to register P2. Otherwise, insert the
** integer in P3 into the RowSet and continue on to the
** next opcode.
假设寄存器P3持有一个64位整数的值。如果寄存器P1包含一个行集RowSet对象，这个行集对象包含的值在寄存器P3,
跳转到寄存器P2。否则,把寄存器P3的整数插入行集,继续到下一个码。
**
** The RowSet object is optimized for the case where successive sets
** of integers, where each set contains no duplicates. Each set
** of values is identified by a unique P4 value. The first set
** must have P4==0, the final set P4=-1.  P4 must be either -1 or
** non-negative.  For non-negative values of P4 only the lower 4
** bits are significant.
**RowSet对象连续的情况下设置进行了优化的整数,每组不包含重复。
每组的值是由独特的P4标识值。第一组必须P4 = = 0,最后一组P4 = 1。
必须1或P4非负。为非负的值只P4低4位意义重大。


** This allows optimizations: (a) when P4==0 there is no need to test
** the rowset object for P3, as it is guaranteed not to contain it,
** (b) when P4==-1 there is no need to insert the value, as it will
** never be tested for, and (c) when a value that is part of set X is
** inserted, there is no need to search to see if the same value was
** previously inserted as part of set X (only if it was previously
** inserted as part of some other set).
这允许优化:(a)当P4 = = 0不需要测试 P3的rowset对象,因为它是保证不含有,
* *(b)当P4 = = 1不需要插入的值,因为它会
* *无法检测,(c)当一个值,它是集X的一部分
* *插入,不需要搜索,看看相同的值
* *之前插入的一部分设置X(只有在它之前
* *插入其他组)的一部分。
*/
case OP_RowSetTest: {                     /* jump, in1, in3 */
  int iSet;
  int exists;

  pIn1 = &aMem[pOp->p1];
  pIn3 = &aMem[pOp->p3];
  iSet = pOp->p4.i;
  assert( pIn3->flags&MEM_Int );

  /* If there is anything other than a rowset object in memory cell P1,
  ** delete it now and initialize P1 with an empty rowset
  如果有任何东西除了rowset对象在内存中细胞P1,
删除它现在P1和初始化一个空行集
  */
  if( (pIn1->flags & MEM_RowSet)==0 ){
    sqlite3VdbeMemSetRowSet(pIn1);
    if( (pIn1->flags & MEM_RowSet)==0 ) goto no_mem;
  }

  assert( pOp->p4type==P4_INT32 );
  assert( iSet==-1 || iSet>=0 );
  if( iSet ){
    exists = sqlite3RowSetTest(pIn1->u.pRowSet, 
                               (u8)(iSet>=0 ? iSet & 0xf : 0xff),
                               pIn3->u.i);
    if( exists ){
      pc = pOp->p2 - 1;
      break;
    }
  }
  if( iSet>=0 ){
    sqlite3RowSetInsert(pIn1->u.pRowSet, pIn3->u.i);
  }
  break;
}


#ifndef SQLITE_OMIT_TRIGGER

/* Opcode: Program P1 P2 P3 P4 *
**
** Execute the trigger program passed as P4 (type P4_SUBPROGRAM). 
**
** P1 contains the address of the memory cell that contains the first memory 
** cell in an array of values used as arguments to the sub-program. P2 
** contains the address to jump to if the sub-program throws an IGNORE 
** exception using the RAISE() function. Register P3 contains the address 
** of a memory cell in this (the parent) VM that is used to allocate the 
** memory required by the sub-vdbe at runtime.
**
** P4 is a pointer to the VM containing the trigger program.
执行触发器程序作为P4传递(P4_SUBPROGRAM型)。
* *
* * P1包含了存储单元的地址,包含第一个记忆
* *细胞在一个数组的值作为参数传递给子程序。p2
* *包含地址跳转到如果子程序抛出一个忽视
* *例外使用提高()函数。P3包含注册地址
* *的一个存储单元(父)VM分配使用
在运行时* * sub-vdbe所需的内存。
* *
* * P4是一个指向包含触发程序的虚拟机。
*/
case OP_Program: {        /* jump */
  int nMem;               /* Number of memory registers for sub-program */
  int nByte;              /* Bytes of runtime space required for sub-program */
  Mem *pRt;               /* Register to allocate runtime space */
  Mem *pMem;              /* Used to iterate through memory cells */
  Mem *pEnd;              /* Last memory cell in new array */
  VdbeFrame *pFrame;      /* New vdbe frame to execute in */
  SubProgram *pProgram;   /* Sub-program to execute */
  void *t;                /* Token identifying trigger */

  pProgram = pOp->p4.pProgram;
  pRt = &aMem[pOp->p3];
  assert( pProgram->nOp>0 );
  
  /* If the p5 flag is clear, then recursive invocation of triggers is 
  ** disabled for backwards compatibility (p5 is set if this sub-program
  ** is really a trigger, not a foreign key action, and the flag set
  ** and cleared by the "PRAGMA recursive_triggers" command is clear).
  **如果p5标志是明确的,那么递归调用触发器
* *禁用向后兼容(p5设置如果这个子程序
* *真是一个触发器,没有外键操作,标记集
* *,通过“杂注recursive_triggers”命令是显而易见的)。

  ** It is recursive invocation of triggers, at the SQL level, that is 
  ** disabled. In some cases a single trigger may generate more than one 
  ** SubProgram (if the trigger may be executed with more than one different 
  ** ON CONFLICT algorithm). SubProgram structures associated with a
  ** single trigger all have the same value for the SubProgram.token 
  ** variable.  
  如果p5标志是明确的,那么递归调用触发器
* *禁用向后兼容(p5设置如果这个子程序
* *真是一个触发器,没有外键操作,标记集
* *,通过“杂注recursive_triggers”命令是显而易见的)。
  */


  if( pOp->p5 ){
    t = pProgram->token;
    for(pFrame=p->pFrame; pFrame && pFrame->token!=t; pFrame=pFrame->pParent);
    if( pFrame ) break;
  }

  if( p->nFrame>=db->aLimit[SQLITE_LIMIT_TRIGGER_DEPTH] ){
    rc = SQLITE_ERROR;
    sqlite3SetString(&p->zErrMsg, db, "too many levels of trigger recursion");
    break;
  }

  /* Register pRt is used to store the memory required to save the state
  ** of the current program, and the memory required at runtime to execute
  ** the trigger program. If this trigger has been fired before, then pRt 
  ** is already allocated. Otherwise, it must be initialized. 
  寄存器pRt用于存储所需的内存保存状态
当前程序的* *,在运行时执行所需的内存
* *触发程序。如果这个触发器之前被解雇,然后pRt
* *已经分配。否则,它必须被初始化。
*/
  if( (pRt->flags&MEM_Frame)==0 ){
    /* SubProgram.nMem is set to the number of memory cells used by the 
    ** program stored in SubProgram.aOp. As well as these, one memory
    ** cell is required for each cursor used by the program. Set local
    ** variable nMem (and later, VdbeFrame.nChildMem) to this value.
	子程序。nMem被设置为使用的记忆细胞的数量
* *项目存储在SubProgram.aOp。 除了这些,一段记忆
* *细胞需要每个游标使用的程序。设置本地
* *变量nMem(后来,VdbeFrame.nChildMem)这个值。
    */
    nMem = pProgram->nMem + pProgram->nCsr;
    nByte = ROUND8(sizeof(VdbeFrame))
              + nMem * sizeof(Mem)
              + pProgram->nCsr * sizeof(VdbeCursor *)
              + pProgram->nOnce * sizeof(u8);
    pFrame = sqlite3DbMallocZero(db, nByte);
    if( !pFrame ){
      goto no_mem;
    }
    sqlite3VdbeMemRelease(pRt);
    pRt->flags = MEM_Frame;
    pRt->u.pFrame = pFrame;

    pFrame->v = p;
    pFrame->nChildMem = nMem;
    pFrame->nChildCsr = pProgram->nCsr;
    pFrame->pc = pc;
    pFrame->aMem = p->aMem;
    pFrame->nMem = p->nMem;
    pFrame->apCsr = p->apCsr;
    pFrame->nCursor = p->nCursor;
    pFrame->aOp = p->aOp;
    pFrame->nOp = p->nOp;
    pFrame->token = pProgram->token;
    pFrame->aOnceFlag = p->aOnceFlag;
    pFrame->nOnceFlag = p->nOnceFlag;

    pEnd = &VdbeFrameMem(pFrame)[pFrame->nChildMem];
    for(pMem=VdbeFrameMem(pFrame); pMem!=pEnd; pMem++){
      pMem->flags = MEM_Invalid;
      pMem->db = db;
    }
  }else{
    pFrame = pRt->u.pFrame;
    assert( pProgram->nMem+pProgram->nCsr==pFrame->nChildMem );
    assert( pProgram->nCsr==pFrame->nChildCsr );
    assert( pc==pFrame->pc );
  }

  p->nFrame++;
  pFrame->pParent = p->pFrame;
  pFrame->lastRowid = lastRowid;
  pFrame->nChange = p->nChange;
  p->nChange = 0;
  p->pFrame = pFrame;
  p->aMem = aMem = &VdbeFrameMem(pFrame)[-1];
  p->nMem = pFrame->nChildMem;
  p->nCursor = (u16)pFrame->nChildCsr;
  p->apCsr = (VdbeCursor **)&aMem[p->nMem+1];
  p->aOp = aOp = pProgram->aOp;
  p->nOp = pProgram->nOp;
  p->aOnceFlag = (u8 *)&p->apCsr[p->nCursor];
  p->nOnceFlag = pProgram->nOnce;
  pc = -1;
  memset(p->aOnceFlag, 0, p->nOnceFlag);

  break;
}

/* Opcode: Param P1 P2 * * *
**
** This opcode is only ever present in sub-programs called via the 
** OP_Program instruction. Copy a value currently stored in a memory 
** cell of the calling (parent) frame to cell P2 in the current frames 
** address space. This is used by trigger programs to access the new.* 
** and old.* values.
这个操作码只出现在子程序通过调用
* * OP_Program指令。 复制当前值存储在内存中
* *单元的调用(父)框架细胞P2在当前帧
* *地址空间。这是使用触发器程序访问新。*
* *老。*值。
**
** The address of the cell in the parent frame is determined by adding
** the value of the P1 argument to the value of the P1 argument to the
** calling OP_Program instruction.
在父框架单元的地址是通过添加决定的
* *的值P1参数P1参数的值
* *叫OP_Program指令。
*/
case OP_Param: {           /* out2-prerelease */
  VdbeFrame *pFrame;
  Mem *pIn;
  pFrame = p->pFrame;
  pIn = &pFrame->aMem[pOp->p1 + pFrame->aOp[pFrame->pc].p1];   
  sqlite3VdbeMemShallowCopy(pOut, pIn, MEM_Ephem);
  break;
}

#endif /* #ifndef SQLITE_OMIT_TRIGGER */

#ifndef SQLITE_OMIT_FOREIGN_KEY
/* Opcode: FkCounter P1 P2 * * *
**
** Increment a "constraint counter" by P2 (P2 may be negative or positive).
** If P1 is non-zero, the database constraint counter is incremented 
** (deferred foreign key constraints). Otherwise, if P1 is zero, the 
** statement counter is incremented (immediate foreign key constraints).
增加一个“约束计数器”P2(P2可能是积极的或消极的)。
* *如果P1非零,数据库约束计数器递增
* *(递延外键约束)。否则,如果P1为零,
* *声明计数器递增(直接的外键约束)。
*/
case OP_FkCounter: {
  if( pOp->p1 ){
    db->nDeferredCons += pOp->p2;
  }else{
    p->nFkConstraint += pOp->p2;
  }
  break;
}

/* Opcode: FkIfZero P1 P2 * * *
**
** This opcode tests if a foreign key constraint-counter is currently zero.
** If so, jump to instruction P2. Otherwise, fall through to the next 
** instruction.
**
** If P1 is non-zero, then the jump is taken if the database constraint-counter
** is zero (the one that counts deferred constraint violations). If P1 is
** zero, the jump is taken if the statement constraint-counter is zero
** (immediate foreign key constraint violations).
这个操作码测试如果一个外键constraint-counter目前零。
* *如果是这样,跳转到指令P2。否则,失败
* *指令。* *
* *如果P1非0,那么如果数据库constraint-counter跳了
* *是零(计数延迟约束违反的)。如果P1
* *零,采取跳如果声明constraint-counter是零
* *(直接侵犯外键约束)。
*/
case OP_FkIfZero: {         /* jump */
  if( pOp->p1 ){
    if( db->nDeferredCons==0 ) pc = pOp->p2-1;
  }else{
    if( p->nFkConstraint==0 ) pc = pOp->p2-1;
  }
  break;
}
#endif /* #ifndef SQLITE_OMIT_FOREIGN_KEY */

#ifndef SQLITE_OMIT_AUTOINCREMENT
/* Opcode: MemMax P1 P2 * * *
**
** P1 is a register in the root frame of this VM (the root frame is
** different from the current frame if this instruction is being executed
** within a sub-program). Set the value of register P1 to the maximum of 
** its current value and the value in register P2.
**
** This instruction throws an error if the memory cell is not initially
** an integer.
根肋骨的P1是一个注册这个VM(根肋骨
* *不同于当前帧是否正在执行该指令
* *在子程序)。设置注册P1的最大的价值
* *其当前值和寄存器中的值P2。
* *
* *该指令将抛出一个错误如果没有最初记忆细胞
* *一个整数。
*/
case OP_MemMax: {        /* in2 */
  Mem *pIn1;
  VdbeFrame *pFrame;
  if( p->pFrame ){
    for(pFrame=p->pFrame; pFrame->pParent; pFrame=pFrame->pParent);
    pIn1 = &pFrame->aMem[pOp->p1];
  }else{
    pIn1 = &aMem[pOp->p1];
  }
  assert( memIsValid(pIn1) );
  sqlite3VdbeMemIntegerify(pIn1);
  pIn2 = &aMem[pOp->p2];
  sqlite3VdbeMemIntegerify(pIn2);
  if( pIn1->u.i<pIn2->u.i){
    pIn1->u.i = pIn2->u.i;
  }
  break;
}
#endif /* SQLITE_OMIT_AUTOINCREMENT */

/* Opcode: IfPos P1 P2 * * *
**
** If the value of register P1 is 1 or greater, jump to P2.
**
** It is illegal to use this instruction on a register that does
** not contain an integer.  An assertion fault will result if you try.
如果寄存器的值P1是1或更高,跳到P2。
* *
* *是违法使用这个指令寄存器,它
* *不包含一个整数。断言故障会如果你试一试。
*/
case OP_IfPos: {        /* jump, in1 */
  pIn1 = &aMem[pOp->p1];
  assert( pIn1->flags&MEM_Int );
  if( pIn1->u.i>0 ){
     pc = pOp->p2 - 1;
  }
  break;
}

/* Opcode: IfNeg P1 P2 * * *
**
** If the value of register P1 is less than zero, jump to P2. 
**
** It is illegal to use this instruction on a register that does
** not contain an integer.  An assertion fault will result if you try.
如果寄存器的值P1小于零,跳到P2。
* *
* *是违法使用这个指令寄存器,它
* *不包含一个整数。断言故障会如果你试一试。
*/
case OP_IfNeg: {        /* jump, in1 */
  pIn1 = &aMem[pOp->p1];
  assert( pIn1->flags&MEM_Int );
  if( pIn1->u.i<0 ){
     pc = pOp->p2 - 1;
  }
  break;
}

/* Opcode: IfZero P1 P2 P3 * *
**
** The register P1 must contain an integer.  Add literal P3 to the
** value in register P1.  If the result is exactly 0, jump to P2. 
**
** It is illegal to use this instruction on a register that does
** not contain an integer.  An assertion fault will result if you try.
寄存器P1必须包含一个整数。添加文字P3
* *价值登记P1。如果结果是0,跳到P2。
* *
* *是违法使用这个指令寄存器,它
* *不包含一个整数。断言故障会如果你试一试。
*/
case OP_IfZero: {        /* jump, in1 */
  pIn1 = &aMem[pOp->p1];
  assert( pIn1->flags&MEM_Int );
  pIn1->u.i += pOp->p3;
  if( pIn1->u.i==0 ){
     pc = pOp->p2 - 1;
  }
  break;
}

/* Opcode: AggStep * P2 P3 P4 P5
**
** Execute the step function for an aggregate.  The
** function has P5 arguments.   P4 is a pointer to the FuncDef
** structure that specifies the function.  Use register
** P3 as the accumulator.
**
** The P5 arguments are taken from register P2 and its
** successors.
一个聚合函数执行步骤。的
* *函数P5参数。P4 FuncDef是一个指针
* *结构,指定的函数。使用注册
* * P3蓄电池。
* *
* * P5参数从P2和登记
* *的继任者。
*/
case OP_AggStep: {
  int n;
  int i;
  Mem *pMem;
  Mem *pRec;
  sqlite3_context ctx;
  sqlite3_value **apVal;

  n = pOp->p5;
  assert( n>=0 );
  pRec = &aMem[pOp->p2];
  apVal = p->apArg;
  assert( apVal || n==0 );
  for(i=0; i<n; i++, pRec++){
    assert( memIsValid(pRec) );
    apVal[i] = pRec;
    memAboutToChange(p, pRec);
    sqlite3VdbeMemStoreType(pRec);
  }
  ctx.pFunc = pOp->p4.pFunc;
  assert( pOp->p3>0 && pOp->p3<=p->nMem );
  ctx.pMem = pMem = &aMem[pOp->p3];
  pMem->n++;
  ctx.s.flags = MEM_Null;
  ctx.s.z = 0;
  ctx.s.zMalloc = 0;
  ctx.s.xDel = 0;
  ctx.s.db = db;
  ctx.isError = 0;
  ctx.pColl = 0;
  ctx.skipFlag = 0;
  if( ctx.pFunc->flags & SQLITE_FUNC_NEEDCOLL ){
    assert( pOp>p->aOp );
    assert( pOp[-1].p4type==P4_COLLSEQ );
    assert( pOp[-1].opcode==OP_CollSeq );
    ctx.pColl = pOp[-1].p4.pColl;
  }
  (ctx.pFunc->xStep)(&ctx, n, apVal); /* IMP: R-24505-23230 */
  if( ctx.isError ){
    sqlite3SetString(&p->zErrMsg, db, "%s", sqlite3_value_text(&ctx.s));
    rc = ctx.isError;
  }
  if( ctx.skipFlag ){
    assert( pOp[-1].opcode==OP_CollSeq );
    i = pOp[-1].p1;
    if( i ) sqlite3VdbeMemSetInt64(&aMem[i], 1);
  }

  sqlite3VdbeMemRelease(&ctx.s);

  break;
}

/* Opcode: AggFinal P1 P2 * P4 *
**
** Execute the finalizer function for an aggregate.  P1 is
** the memory location that is the accumulator for the aggregate.
**
** P2 is the number of arguments that the step function takes and
** P4 is a pointer to the FuncDef for this function.  The P2
** argument is not used by this opcode.  It is only there to disambiguate
** functions that can take varying numbers of arguments.  The
** P4 argument is only needed for the degenerate case where
** the step function was not previously called.
执行终结器一个聚合函数。P1是
* *的内存位置聚合的蓄电池。
* *
* * P2是阶跃函数需要的参数和数量
* * P4 FuncDef这个函数指针。P2
* *的论点是不习惯操作码。只有消除歧义
* *函数可以有不同数量的参数。的
* *P4的论点只是所需的退化情况
* *阶梯函数不是之前调用。
*/
case OP_AggFinal: {
  Mem *pMem;
  assert( pOp->p1>0 && pOp->p1<=p->nMem );
  pMem = &aMem[pOp->p1];
  assert( (pMem->flags & ~(MEM_Null|MEM_Agg))==0 );
  rc = sqlite3VdbeMemFinalize(pMem, pOp->p4.pFunc);
  if( rc ){
    sqlite3SetString(&p->zErrMsg, db, "%s", sqlite3_value_text(pMem));
  }
  sqlite3VdbeChangeEncoding(pMem, encoding);
  UPDATE_MAX_BLOBSIZE(pMem);
  if( sqlite3VdbeMemTooBig(pMem) ){
    goto too_big;
  }
  break;
}

#ifndef SQLITE_OMIT_WAL
/* Opcode: Checkpoint P1 P2 P3 * *
**
** Checkpoint database P1. This is a no-op if P1 is not currently in
** WAL mode. Parameter P2 is one of SQLITE_CHECKPOINT_PASSIVE, FULL
** or RESTART.  Write 1 or 0 into mem[P3] if the checkpoint returns
** SQLITE_BUSY or not, respectively.  Write the number of pages in the
** WAL after the checkpoint into mem[P3+1] and the number of pages
** in the WAL that have been checkpointed after the checkpoint
** completes into mem[P3+2].  However on an error, mem[P3+1] and
** mem[P3+2] are initialized to -1.
数据库检查点P1。这是一个空操作如果不目前在P1
* * WAL模式。参数P2是SQLITE_CHECKPOINT_PASSIVE之一,满了
* *或重启。1或0写入mem(P3)如果检查站返回
* * SQLITE_BUSY与否,分别。写的页面数量
* * WAL检查点之后的mem(P3 + 1)和页面的数量
* *后的WAL设置检查点的检查站
* *完成mem(P3 + 2)。然而在一个错误,mem(P3 + 1)
* * mem(P3 + 2)初始化为1。
*/
case OP_Checkpoint: {
  int i;                          /* Loop counter */
  int aRes[3];                    /* Results */
  Mem *pMem;                      /* Write results here */

  aRes[0] = 0;
  aRes[1] = aRes[2] = -1;
  assert( pOp->p2==SQLITE_CHECKPOINT_PASSIVE
       || pOp->p2==SQLITE_CHECKPOINT_FULL
       || pOp->p2==SQLITE_CHECKPOINT_RESTART
  );
  rc = sqlite3Checkpoint(db, pOp->p1, pOp->p2, &aRes[1], &aRes[2]);
  if( rc==SQLITE_BUSY ){
    rc = SQLITE_OK;
    aRes[0] = 1;
  }
  for(i=0, pMem = &aMem[pOp->p3]; i<3; i++, pMem++){
    sqlite3VdbeMemSetInt64(pMem, (i64)aRes[i]);
  }    
  break;
};  
#endif

#ifndef SQLITE_OMIT_PRAGMA
/* Opcode: JournalMode P1 P2 P3 * P5
**
** Change the journal mode of database P1 to P3. P3 must be one of the
** PAGER_JOURNALMODE_XXX values. If changing between the various rollback
** modes (delete, truncate, persist, off and memory), this is a simple
** operation. No IO is required.
**
** If changing into or out of WAL mode the procedure is more complicated.
**
** Write a string containing the final journal-mode to register P2.
改变数据库的日志模式P1 P3。必须的一个P3
* * PAGER_JOURNALMODE_XXX值。 如果改变之间的各种各样的回滚
* *模式(删除、截断、持续和内存),这是一个简单的
* *操作。不需要IO。
* *
* *如果换上或WAL模式过程更为复杂。
* *
* *写一个字符串包含最后journal方式注册P2。
*/
case OP_JournalMode: {    /* out2-prerelease */
  Btree *pBt;                     /* Btree to change journal mode of */
  Pager *pPager;                  /* Pager associated with pBt */
  int eNew;                       /* New journal mode */
  int eOld;                       /* The old journal mode */
  const char *zFilename;          /* Name of database file for pPager */

  eNew = pOp->p3;
  assert( eNew==PAGER_JOURNALMODE_DELETE 
       || eNew==PAGER_JOURNALMODE_TRUNCATE 
       || eNew==PAGER_JOURNALMODE_PERSIST 
       || eNew==PAGER_JOURNALMODE_OFF
       || eNew==PAGER_JOURNALMODE_MEMORY
       || eNew==PAGER_JOURNALMODE_WAL
       || eNew==PAGER_JOURNALMODE_QUERY
  );
  assert( pOp->p1>=0 && pOp->p1<db->nDb );

  pBt = db->aDb[pOp->p1].pBt;
  pPager = sqlite3BtreePager(pBt);
  eOld = sqlite3PagerGetJournalMode(pPager);
  if( eNew==PAGER_JOURNALMODE_QUERY ) eNew = eOld;
  if( !sqlite3PagerOkToChangeJournalMode(pPager) ) eNew = eOld;

#ifndef SQLITE_OMIT_WAL
  zFilename = sqlite3PagerFilename(pPager, 1);

  /* Do not allow a transition to journal_mode=WAL for a database
  ** in temporary storage or if the VFS does not support shared memory 
  不允许一个过渡journal_mode = WAL数据库
* *在临时存储或者VFS不支持共享内存
  */
  if( eNew==PAGER_JOURNALMODE_WAL
   && (sqlite3Strlen30(zFilename)==0           /* Temp file */
       || !sqlite3PagerWalSupported(pPager))   /* No shared-memory support */
  ){
    eNew = eOld;
  }

  if( (eNew!=eOld)
   && (eOld==PAGER_JOURNALMODE_WAL || eNew==PAGER_JOURNALMODE_WAL)
  ){
    if( !db->autoCommit || db->activeVdbeCnt>1 ){
      rc = SQLITE_ERROR;
      sqlite3SetString(&p->zErrMsg, db, 
          "cannot change %s wal mode from within a transaction",
          (eNew==PAGER_JOURNALMODE_WAL ? "into" : "out of")
      );
      break;
    }else{
 
      if( eOld==PAGER_JOURNALMODE_WAL ){
        /* If leaving WAL mode, close the log file. If successful, the call
        ** to PagerCloseWal() checkpoints and deletes the write-ahead-log 
        ** file. An EXCLUSIVE lock may still be held on the database file 
        ** after a successful return. 
		如果离开WAL模式,关闭日志文件。如果成功,调用
        ** PagerCloseWal()write-ahead-log检查点和删除
        **文件。独占锁可能仍在数据库文件中
        **成功后返回。
        */
        rc = sqlite3PagerCloseWal(pPager);
        if( rc==SQLITE_OK ){
          sqlite3PagerSetJournalMode(pPager, eNew);
        }
      }else if( eOld==PAGER_JOURNALMODE_MEMORY ){
        /* Cannot transition directly from MEMORY to WAL.  Use mode OFF
        ** as an intermediate
		不能直接从Memory模式过渡到WAL模式。使用模式OFF
        **作为中间
		*/
        sqlite3PagerSetJournalMode(pPager, PAGER_JOURNALMODE_OFF);
      }
  
      /* Open a transaction on the database file. Regardless of the journal
      ** mode, this transaction always uses a rollback journal.
	  打开事务在数据库文件。无论日志模式,这个事务总是使用一个回滚日志。
      */
      assert( sqlite3BtreeIsInTrans(pBt)==0 );
      if( rc==SQLITE_OK ){
        rc = sqlite3BtreeSetVersion(pBt, (eNew==PAGER_JOURNALMODE_WAL ? 2 : 1));
      }
    }
  }
#endif /* ifndef SQLITE_OMIT_WAL */

  if( rc ){
    eNew = eOld;
  }
  eNew = sqlite3PagerSetJournalMode(pPager, eNew);

  pOut = &aMem[pOp->p2];
  pOut->flags = MEM_Str|MEM_Static|MEM_Term;
  pOut->z = (char *)sqlite3JournalModename(eNew);
  pOut->n = sqlite3Strlen30(pOut->z);
  pOut->enc = SQLITE_UTF8;
  sqlite3VdbeChangeEncoding(pOut, encoding);
  break;
};
#endif /* SQLITE_OMIT_PRAGMA */

#if !defined(SQLITE_OMIT_VACUUM) && !defined(SQLITE_OMIT_ATTACH)
/* Opcode: Vacuum * * * * *
**
** Vacuum the entire database.  This opcode will cause other virtual
** machines to be created and run.  It may not be called from within
** a transaction.
真空整个数据库。这个操作码会导致其他虚拟机器创建和运行。它可能不是从内部一个事务。
*/
case OP_Vacuum: {
  rc = sqlite3RunVacuum(&p->zErrMsg, db);
  break;
}
#endif

#if !defined(SQLITE_OMIT_AUTOVACUUM)
/* Opcode: IncrVacuum P1 P2 * * *
**
** Perform a single step of the incremental vacuum procedure on
** the P1 database. If the vacuum has finished, jump to instruction
** P2. Otherwise, fall through to the next instruction.
执行增量真空过程的一个步骤P1数据库。如果真空完成后,跳转到指令 P2。否则,下降到下一个指令。
*/
case OP_IncrVacuum: {        /* jump */
  Btree *pBt;

  assert( pOp->p1>=0 && pOp->p1<db->nDb );
  assert( (p->btreeMask & (((yDbMask)1)<<pOp->p1))!=0 );
  pBt = db->aDb[pOp->p1].pBt;
  rc = sqlite3BtreeIncrVacuum(pBt);
  if( rc==SQLITE_DONE ){
    pc = pOp->p2 - 1;
    rc = SQLITE_OK;
  }
  break;
}
#endif

/* Opcode: Expire P1 * * * *
**
** Cause precompiled statements to become expired. An expired statement
** fails with an error code of SQLITE_SCHEMA if it is ever executed 
** (via sqlite3_step()).
导致预编译语句变得过期了。一个过期的语句
**失败的错误代码SQLITE_SCHEMA如果它执行
**(通过sqlite3_step())。

** 
** If P1 is 0, then all SQL statements become expired. If P1 is non-zero,
** then only the currently executing statement is affected. 
如果P1为0,那么所有SQL语句变得过期。如果非零P1,
* *然后只影响当前执行语句。

*/
case OP_Expire: {
  if( !pOp->p1 ){
    sqlite3ExpirePreparedStatements(db);
  }else{
    p->expired = 1;
  }
  break;
}

#ifndef SQLITE_OMIT_SHARED_CACHE
/* Opcode: TableLock P1 P2 P3 P4 *
**
** Obtain a lock on a particular table. This instruction is only used when
** the shared-cache feature is enabled. 
**获得一个锁在一个特定的表。这个指令时才使用
* *启用共享缓存特性。

** P1 is the index of the database in sqlite3.aDb[] of the database
** on which the lock is acquired.  A readlock is obtained if P3==0 or
** a write lock if P3==1.
**P1的索引数据库sqlite3。亚洲开发银行数据库的[]
加了锁的* *。一个readlock如果P3 = = 0或获得
* *一个写锁如果P3 = = 1。

** P2 contains the root-page of the table to lock.
P2包含锁表的根页。
**
** P4 contains a pointer to the name of the table being locked. This is only
** used to generate an error message if the lock cannot be obtained.
* * P4包含一个指针表被锁的名称。这只是
* *用于生成一个错误消息,如果不能获得锁。
*/
case OP_TableLock: {
  u8 isWriteLock = (u8)pOp->p3;
  if( isWriteLock || 0==(db->flags&SQLITE_ReadUncommitted) ){
    int p1 = pOp->p1; 
    assert( p1>=0 && p1<db->nDb );
    assert( (p->btreeMask & (((yDbMask)1)<<p1))!=0 );
    assert( isWriteLock==0 || isWriteLock==1 );
    rc = sqlite3BtreeLockTable(db->aDb[p1].pBt, pOp->p2, isWriteLock);
    if( (rc&0xFF)==SQLITE_LOCKED ){
      const char *z = pOp->p4.z;
      sqlite3SetString(&p->zErrMsg, db, "database table is locked: %s", z);
    }
  }
  break;
}
#endif /* SQLITE_OMIT_SHARED_CACHE */

#ifndef SQLITE_OMIT_VIRTUALTABLE
/* Opcode: VBegin * * * P4 *
**
** P4 may be a pointer to an sqlite3_vtab structure. If so, call the 
** xBegin method for that table.
**
** Also, whether or not P4 is set, check that this is not being called from
** within a callback to a virtual table xSync() method. If it is, the error
** code will be set to SQLITE_LOCKED.
寄存器P4对 sqlite3_vtab结构可能是一个指针。 如果是这样,对那张表调用xBegin方法。
* *
* *另外,P4是否设置,检查这不是被称为
* *在一个回调到一个虚拟表xSync()方法。如果是,这个错误
代码将被设置为SQLITE_LOCKED * *。
*/
case OP_VBegin: {
  VTable *pVTab;
  pVTab = pOp->p4.pVtab;
  rc = sqlite3VtabBegin(db, pVTab);
  if( pVTab ) importVtabErrMsg(p, pVTab->pVtab);
  break;
}
#endif /* SQLITE_OMIT_VIRTUALTABLE */

#ifndef SQLITE_OMIT_VIRTUALTABLE
/* Opcode: VCreate P1 * * P4 *
**
** P4 is the name of a virtual table in database P1. Call the xCreate method
** for that table.P4的名称是一个虚拟表在数据库P1。调用xCreate方法表
*/
case OP_VCreate: {
  rc = sqlite3VtabCallCreate(db, pOp->p1, pOp->p4.z, &p->zErrMsg);
  break;
}
#endif /* SQLITE_OMIT_VIRTUALTABLE */

#ifndef SQLITE_OMIT_VIRTUALTABLE
/* Opcode: VDestroy P1 * * P4 *
**
** P4 is the name of a virtual table in database P1.  Call the xDestroy method
** of that table.
    P4是在数据库P1中的一个虚拟表的名字。调用那个表的xDestroy方法
*/
case OP_VDestroy: {
  p->inVtabMethod = 2;
  rc = sqlite3VtabCallDestroy(db, pOp->p1, pOp->p4.z);
  p->inVtabMethod = 0;
  break;
}
#endif /* SQLITE_OMIT_VIRTUALTABLE */

#ifndef SQLITE_OMIT_VIRTUALTABLE
/* Opcode: VOpen P1 * * P4 *
**
** P4 is a pointer to a virtual table object, an sqlite3_vtab structure.
** P1 is a cursor number.  This opcode opens a cursor to the virtual
** table and stores that cursor in P1.
P4是一个虚表指针对象,一个sqlite3_vtab结构。
* * P1是游标的数字。这个操作码打开虚拟光标
* *表和商店,光标在P1。
*/
case OP_VOpen: {
  VdbeCursor *pCur;
  sqlite3_vtab_cursor *pVtabCursor;
  sqlite3_vtab *pVtab;
  sqlite3_module *pModule;

  pCur = 0;
  pVtabCursor = 0;
  pVtab = pOp->p4.pVtab->pVtab;
  pModule = (sqlite3_module *)pVtab->pModule;
  assert(pVtab && pModule);
  rc = pModule->xOpen(pVtab, &pVtabCursor);
  importVtabErrMsg(p, pVtab);
  if( SQLITE_OK==rc ){
    /* Initialize sqlite3_vtab_cursor base class */
    pVtabCursor->pVtab = pVtab;

    /* Initialise vdbe cursor object */
    pCur = allocateCursor(p, pOp->p1, 0, -1, 0);
    if( pCur ){
      pCur->pVtabCursor = pVtabCursor;
      pCur->pModule = pVtabCursor->pVtab->pModule;
    }else{
      db->mallocFailed = 1;
      pModule->xClose(pVtabCursor);
    }
  }
  break;
}
#endif /* SQLITE_OMIT_VIRTUALTABLE */

#ifndef SQLITE_OMIT_VIRTUALTABLE
/* Opcode: VFilter P1 P2 P3 P4 *
**
** P1 is a cursor opened using VOpen.  P2 is an address to jump to if
** the filtered result set is empty.
**P1是使用VOpen打开游标。P2是跳转到一个地址
**过滤结果集是空的。

** P4 is either NULL or a string that was generated by the xBestIndex
** method of the module.  The interpretation of the P4 string is left
** to the module implementation.
P4 NULL或一个字符串,该字符串是由xBestIndex生成
* *模块的方法。P4的解释字符串是离开了
* *模块实现。
**
** This opcode invokes the xFilter method on the virtual table specified
** by P1.  The integer query plan parameter to xFilter is stored in register
** P3. Register P3+1 stores the argc parameter to be passed to the
** xFilter method. Registers P3+2..P3+1+argc are the argc
** additional parameters which are passed to
** xFilter as argv. Register P3+2 becomes argv[0] when passed to xFilter.

**这个操作码调用xFilter方法指定的虚拟表
* * P1。整数xFilter查询计划参数存储在寄存器中
* * P3。注册P3 + 1存储命令行参数个数参数被传递到
* * xFilter方法。寄存器P3 + 2 . .命令行参数个数P3 + 1 +命令行参数个数
* *额外的参数传递给
* * xFilter argv。 注册P3 + 2时传递给xFilter成了argv[0]。
* *

** A jump is made to P2 if the result set after filtering would be empty.

* *跳转了P2如果过滤后的结果集将是空的。
*/
case OP_VFilter: {   /* jump */
  int nArg;
  int iQuery;
  const sqlite3_module *pModule;
  Mem *pQuery;
  Mem *pArgc;
  sqlite3_vtab_cursor *pVtabCursor;
  sqlite3_vtab *pVtab;
  VdbeCursor *pCur;
  int res;
  int i;
  Mem **apArg;

  pQuery = &aMem[pOp->p3];
  pArgc = &pQuery[1];
  pCur = p->apCsr[pOp->p1];
  assert( memIsValid(pQuery) );
  REGISTER_TRACE(pOp->p3, pQuery);
  assert( pCur->pVtabCursor );
  pVtabCursor = pCur->pVtabCursor;
  pVtab = pVtabCursor->pVtab;
  pModule = pVtab->pModule;

  /* Grab the index number and argc parameters */
  assert( (pQuery->flags&MEM_Int)!=0 && pArgc->flags==MEM_Int );
  nArg = (int)pArgc->u.i;
  iQuery = (int)pQuery->u.i;

  /* Invoke the xFilter method */
  {
    res = 0;
    apArg = p->apArg;
    for(i = 0; i<nArg; i++){
      apArg[i] = &pArgc[i+1];
      sqlite3VdbeMemStoreType(apArg[i]);
    }

    p->inVtabMethod = 1;
    rc = pModule->xFilter(pVtabCursor, iQuery, pOp->p4.z, nArg, apArg);
    p->inVtabMethod = 0;
    importVtabErrMsg(p, pVtab);
    if( rc==SQLITE_OK ){
      res = pModule->xEof(pVtabCursor);
    }

    if( res ){
      pc = pOp->p2 - 1;
    }
  }
  pCur->nullRow = 0;

  break;
}
#endif /* SQLITE_OMIT_VIRTUALTABLE */

#ifndef SQLITE_OMIT_VIRTUALTABLE
/* Opcode: VColumn P1 P2 P3 * *
**
** Store the value of the P2-th column of
** the row of the virtual-table that the 
** P1 cursor is pointing to into register P3.
存储P2-th列的值
* *的虚拟表的行
* * P1游标指向到寄存器P3。
*/
case OP_VColumn: {
  sqlite3_vtab *pVtab;
  const sqlite3_module *pModule;
  Mem *pDest;
  sqlite3_context sContext;

  VdbeCursor *pCur = p->apCsr[pOp->p1];
  assert( pCur->pVtabCursor );
  assert( pOp->p3>0 && pOp->p3<=p->nMem );
  pDest = &aMem[pOp->p3];
  memAboutToChange(p, pDest);
  if( pCur->nullRow ){
    sqlite3VdbeMemSetNull(pDest);
    break;
  }
  pVtab = pCur->pVtabCursor->pVtab;
  pModule = pVtab->pModule;
  assert( pModule->xColumn );
  memset(&sContext, 0, sizeof(sContext));

  /* The output cell may already have a buffer allocated. Move
  ** the current contents to sContext.s so in case the user-function 
  ** can use the already allocated buffer instead of allocating a 
  ** new one.
  细胞可能已经分配一个缓冲的输出。移动
* * sContext的当前内容。所以,以防用户函数
* *可以使用已分配的缓冲区,而不是分配
* *新的。
  */
  sqlite3VdbeMemMove(&sContext.s, pDest);
  MemSetTypeFlag(&sContext.s, MEM_Null);

  rc = pModule->xColumn(pCur->pVtabCursor, &sContext, pOp->p2);
  importVtabErrMsg(p, pVtab);
  if( sContext.isError ){
    rc = sContext.isError;
  }

  /* Copy the result of the function to the P3 register. We
  ** do this regardless of whether or not an error occurred to ensure any
  ** dynamic allocation in sContext.s (a Mem struct) is  released.
      函数的结果复制到P3登记。我们这样做无论是否发生错误,以确保任何
  **在sContext动态分配。年代(Mem结构)。
  */
  sqlite3VdbeChangeEncoding(&sContext.s, encoding);
  sqlite3VdbeMemMove(pDest, &sContext.s);
  REGISTER_TRACE(pOp->p3, pDest);
  UPDATE_MAX_BLOBSIZE(pDest);

  if( sqlite3VdbeMemTooBig(pDest) ){
    goto too_big;
  }
  break;
}
#endif /* SQLITE_OMIT_VIRTUALTABLE */

#ifndef SQLITE_OMIT_VIRTUALTABLE
/* Opcode: VNext P1 P2 * * *
**
** Advance virtual table P1 to the next row in its result set and
** jump to instruction P2.  Or, if the virtual table has reached
** the end of its result set, then fall through to the next instruction.
推进虚拟表P1在结果集和下一行
* *跳转指令P2。或者,如果已经达到虚拟表
* *的结果集,然后下降到下一个指令。
*/
case OP_VNext: {   /* jump */
  sqlite3_vtab *pVtab;
  const sqlite3_module *pModule;
  int res;
  VdbeCursor *pCur;

  res = 0;
  pCur = p->apCsr[pOp->p1];
  assert( pCur->pVtabCursor );
  if( pCur->nullRow ){
    break;
  }
  pVtab = pCur->pVtabCursor->pVtab;
  pModule = pVtab->pModule;
  assert( pModule->xNext );

  /* Invoke the xNext() method of the module. There is no way for the
  ** underlying implementation to return an error if one occurs during
  ** xNext(). Instead, if an error occurs, true is returned (indicating that 
  ** data is available) and the error code returned when xColumn or
  ** some other method is next invoked on the save virtual table cursor.
  模块的调用xNext()方法。没有办法的
* *底层实现返回一个错误,如果期间出现
* * xNext()。相反,如果真的发生错误,返回(表明
* *数据可用),当xColumn或返回的错误代码
* *下一个调用其他方法保存虚拟表游标。
  */
  p->inVtabMethod = 1;
  rc = pModule->xNext(pCur->pVtabCursor);
  p->inVtabMethod = 0;
  importVtabErrMsg(p, pVtab);
  if( rc==SQLITE_OK ){
    res = pModule->xEof(pCur->pVtabCursor);
  }

  if( !res ){
    /* If there is data, jump to P2 */
    pc = pOp->p2 - 1;
  }
  break;
}
#endif /* SQLITE_OMIT_VIRTUALTABLE */

#ifndef SQLITE_OMIT_VIRTUALTABLE
/* Opcode: VRename P1 * * P4 *
**
** P4 is a pointer to a virtual table object, an sqlite3_vtab structure.
** This opcode invokes the corresponding xRename method. The value
** in register P1 is passed as the zName argument to the xRename method.
P4是一个虚表指针对象,一个sqlite3_vtab结构。
* *此操作码调用相应的xRename方法。的价值
* *在P1通过注册为xRename zName参数的方法。
*/
case OP_VRename: {
  sqlite3_vtab *pVtab;
  Mem *pName;

  pVtab = pOp->p4.pVtab->pVtab;
  pName = &aMem[pOp->p1];
  assert( pVtab->pModule->xRename );
  assert( memIsValid(pName) );
  REGISTER_TRACE(pOp->p1, pName);
  assert( pName->flags & MEM_Str );
  testcase( pName->enc==SQLITE_UTF8 );
  testcase( pName->enc==SQLITE_UTF16BE );
  testcase( pName->enc==SQLITE_UTF16LE );
  rc = sqlite3VdbeChangeEncoding(pName, SQLITE_UTF8);
  if( rc==SQLITE_OK ){
    rc = pVtab->pModule->xRename(pVtab, pName->z);
    importVtabErrMsg(p, pVtab);
    p->expired = 0;
  }
  break;
}
#endif

#ifndef SQLITE_OMIT_VIRTUALTABLE
/* Opcode: VUpdate P1 P2 P3 P4 *
**
** P4 is a pointer to a virtual table object, an sqlite3_vtab structure.
** This opcode invokes the corresponding xUpdate method. P2 values
** are contiguous memory cells starting at P3 to pass to the xUpdate 
** invocation. The value in register (P3+P2-1) corresponds to the 
** p2th element of the argv array passed to xUpdate.
   P4是一个虚表指针对象,一个sqlite3_vtab结构。
* *此操作码调用相应的xUpdate方法。P2值
* *是连续的记忆细胞从xUpdate P3通过
* *调用。寄存器中的值(P3 + P2-1)对应
* * p2th argv数组的元素传递给xUpdate。

**
** The xUpdate method will do a DELETE or an INSERT or both.
** The argv[0] element (which corresponds to memory cell P3)
** is the rowid of a row to delete.  If argv[0] is NULL then no 
** deletion occurs.  The argv[1] element is the rowid of the new 
** row.  This can be NULL to have the virtual table select the new 
** rowid for itself.  The subsequent elements in the array are 
** the values of columns in the new row.
xUpdate方法将删除或插入或两者兼而有之。
* * argv[0]元素(对应于存储单元P3)
* *的rowid行删除。如果argv[0]是NULL,那么没有
* *删除发生。argv[1]rowid的新元素
* *行。这可以为空表选择新的虚拟
* * rowid。随后的数组中的元素
* *新行中的列的值。
**
** If P2==1 then no insert is performed.  argv[0] is the rowid of
** a row to delete.
如果P2 = = 1然后不执行插入。argv[0]rowid
* *删除一行。
**
** P1 is a boolean flag. If it is set to true and the xUpdate call
** is successful, then the value returned by sqlite3_last_insert_rowid() 
** is set to the value of the rowid for the row just inserted.
P1是一个布尔标志。如果它被设置为true,xUpdate电话
* *是成功,那么返回的值sqlite3_last_insert_rowid()
* *的值被设置为rowid刚刚插入的行。
*/

case OP_VUpdate: {
  sqlite3_vtab *pVtab;
  sqlite3_module *pModule;
  int nArg;
  int i;
  sqlite_int64 rowid;
  Mem **apArg;
  Mem *pX;

  assert( pOp->p2==1        || pOp->p5==OE_Fail   || pOp->p5==OE_Rollback 
       || pOp->p5==OE_Abort || pOp->p5==OE_Ignore || pOp->p5==OE_Replace
  );
  pVtab = pOp->p4.pVtab->pVtab;
  pModule = (sqlite3_module *)pVtab->pModule;
  nArg = pOp->p2;
  assert( pOp->p4type==P4_VTAB );
  if( ALWAYS(pModule->xUpdate) ){
    u8 vtabOnConflict = db->vtabOnConflict;
    apArg = p->apArg;
    pX = &aMem[pOp->p3];
    for(i=0; i<nArg; i++){
      assert( memIsValid(pX) );
      memAboutToChange(p, pX);
      sqlite3VdbeMemStoreType(pX);
      apArg[i] = pX;
      pX++;
    }
    db->vtabOnConflict = pOp->p5;
    rc = pModule->xUpdate(pVtab, nArg, apArg, &rowid);
    db->vtabOnConflict = vtabOnConflict;
    importVtabErrMsg(p, pVtab);
    if( rc==SQLITE_OK && pOp->p1 ){
      assert( nArg>1 && apArg[0] && (apArg[0]->flags&MEM_Null) );
      db->lastRowid = lastRowid = rowid;
    }
    if( rc==SQLITE_CONSTRAINT && pOp->p4.pVtab->bConstraint ){
      if( pOp->p5==OE_Ignore ){
        rc = SQLITE_OK;
      }else{
        p->errorAction = ((pOp->p5==OE_Replace) ? OE_Abort : pOp->p5);
      }
    }else{
      p->nChange++;
    }
  }
  break;
}
#endif /* SQLITE_OMIT_VIRTUALTABLE */

#ifndef  SQLITE_OMIT_PAGER_PRAGMAS
/* Opcode: Pagecount P1 P2 * * *
**
** Write the current number of pages in database P1 to memory cell P2.
*/
case OP_Pagecount: {            /* out2-prerelease */
  pOut->u.i = sqlite3BtreeLastPage(db->aDb[pOp->p1].pBt);
  break;
}
#endif


#ifndef  SQLITE_OMIT_PAGER_PRAGMAS
/* Opcode: MaxPgcnt P1 P2 P3 * *
**
** Try to set the maximum page count for database P1 to the value in P3.
** Do not let the maximum page count fall below the current page count and
** do not change the maximum page count value if P3==0.
**
** Store the maximum page count after the change in register P2.
试图设置的最大页数数据库P1 P3的价值。
* *不要让最大计数低于当前页面数和页面
* *不改变的最大页数值如果P3 = = 0。
* *
* *店的最大页数后注册P2的变化。
*/
case OP_MaxPgcnt: {            /* out2-prerelease */
  unsigned int newMax;
  Btree *pBt;

  pBt = db->aDb[pOp->p1].pBt;
  newMax = 0;
  if( pOp->p3 ){
    newMax = sqlite3BtreeLastPage(pBt);
    if( newMax < (unsigned)pOp->p3 ) newMax = (unsigned)pOp->p3;
  }
  pOut->u.i = sqlite3BtreeMaxPageCount(pBt, newMax);
  break;
}
#endif


#ifndef SQLITE_OMIT_TRACE
/* Opcode: Trace * * * P4 *
**
** If tracing is enabled (by the sqlite3_trace()) interface, then
** the UTF-8 string contained in P4 is emitted on the trace callback.
如果启用了跟踪(通过sqlite3_trace())接口,然后
* * utf - 8编码的字符串包含在P4上发出跟踪回调。
*/
case OP_Trace: {
  char *zTrace;
  char *z;

  if( db->xTrace && (zTrace = (pOp->p4.z ? pOp->p4.z : p->zSql))!=0 ){
    z = sqlite3VdbeExpandSql(p, zTrace);
    db->xTrace(db->pTraceArg, z);
    sqlite3DbFree(db, z);
  }
#ifdef SQLITE_DEBUG
  if( (db->flags & SQLITE_SqlTrace)!=0
   && (zTrace = (pOp->p4.z ? pOp->p4.z : p->zSql))!=0
  ){
    sqlite3DebugPrintf("SQL-trace: %s\n", zTrace);
  }
#endif /* SQLITE_DEBUG */
  break;
}
#endif


/* Opcode: Noop * * * * *
**
** Do nothing.  This instruction is often useful as a jump
** destination.什么也不做。这个指令通常是有用的作为一个跳
* *的目的地。
*/
/*
** The magic Explain opcode are only inserted when explain==2 (which
** is to say when the EXPLAIN QUERY PLAN syntax is used.)
** This opcode records information from the optimizer.  It is the
** the same as a no-op.  This opcodesnever appears in a real VM program.
神奇的解释操作码时才插入解释= = 2(
* *也就是说语法解释查询计划时使用)。
从优化器* *这个操作码记录信息。这是
* *无为法相同。这个opcodesnever出现在一个真实的虚拟机程序。
*/
default: {          /* This is really OP_Noop and OP_Explain */
  assert( pOp->opcode==OP_Noop || pOp->opcode==OP_Explain );
  break;
}

/*****************************************************************************
** The cases of the switch statement above this line should all be indented
** by 6 spaces.  But the left-most 6 spaces have been removed to improve the
** readability.  From this point on down, the normal indentation rules are
** restored.
switch语句的情况下上面这一行,都应当缩进
6 * *的空间。但最左边6提高空间已被移除
* *可读性。从这个观点上看,正常的缩进规则
* *恢复。
*****************************************************************************/
    }

#ifdef VDBE_PROFILE
    {
      u64 elapsed = sqlite3Hwtime() - start;
      pOp->cycles += elapsed;
      pOp->cnt++;
#if 0
        fprintf(stdout, "%10llu ", elapsed);
        sqlite3VdbePrintOp(stdout, origPc, &aOp[origPc]);
#endif
    }
#endif

    /* The following code adds nothing to the actual functionality
    ** of the program.  It is only here for testing and debugging.
    ** On the other hand, it does burn CPU cycles every time through
    ** the evaluator loop.  So we can leave it out when NDEBUG is defined.
	下面的代码还没有实际功能
程序的* *。只有在测试和调试。
* *在另一方面,它每次通过消耗CPU周期
* *的求值程序循环。所以我们可以把它当NDEBUG定义。
    */
#ifndef NDEBUG
    assert( pc>=-1 && pc<p->nOp );

#ifdef SQLITE_DEBUG
    if( p->trace ){
      if( rc!=0 ) fprintf(p->trace,"rc=%d\n",rc);
      if( pOp->opflags & (OPFLG_OUT2_PRERELEASE|OPFLG_OUT2) ){
        registerTrace(p->trace, pOp->p2, &aMem[pOp->p2]);
      }
      if( pOp->opflags & OPFLG_OUT3 ){
        registerTrace(p->trace, pOp->p3, &aMem[pOp->p3]);
      }
    }
#endif  /* SQLITE_DEBUG */
#endif  /* NDEBUG */
  }  /* The end of the for(;;) loop the loops through opcodes */

  /* If we reach this point, it means that execution is finished with
  ** an error of some kind.如果我们做到这一点,这意味着执行完成
* *一个错误。
  */
vdbe_error_halt:
  assert( rc );
  p->rc = rc;
  testcase( sqlite3GlobalConfig.xLog!=0 );
  sqlite3_log(rc, "statement aborts at %d: [%s] %s", 
                   pc, p->zSql, p->zErrMsg);
  sqlite3VdbeHalt(p);
  if( rc==SQLITE_IOERR_NOMEM ) db->mallocFailed = 1;
  rc = SQLITE_ERROR;
  if( resetSchemaOnFault>0 ){
    sqlite3ResetOneSchema(db, resetSchemaOnFault-1);
  }

  /* This is the only way out of this procedure.  We have to
  ** release the mutexes on btrees that were acquired at the
  ** top.这是这个过程的唯一出路。我们必须
* *释放互斥锁来获得的
* *。
  */
vdbe_return:
  db->lastRowid = lastRowid;
  sqlite3VdbeLeave(p);
  return rc;

  /* Jump to here if a string or blob larger than SQLITE_MAX_LENGTH
  ** is encountered.跳转到这里如果一个字符串或blob大于SQLITE_MAX_LENGTH
遇到* *。
  */
too_big:
  sqlite3SetString(&p->zErrMsg, db, "string or blob too big");
  rc = SQLITE_TOOBIG;
  goto vdbe_error_halt;

  /* Jump to here if a malloc() fails.
  */
no_mem:
  db->mallocFailed = 1;
  sqlite3SetString(&p->zErrMsg, db, "out of memory");
  rc = SQLITE_NOMEM;
  goto vdbe_error_halt;

  /* Jump to here for any other kind of fatal error.  The "rc" variable
  ** should hold the error number.在这里跳转到其它类型的致命错误。“钢筋混凝土”变量
* *应持有错误的号码。
  */
abort_due_to_error:
  assert( p->zErrMsg==0 );
  if( db->mallocFailed ) rc = SQLITE_NOMEM;
  if( rc!=SQLITE_IOERR_NOMEM ){
    sqlite3SetString(&p->zErrMsg, db, "%s", sqlite3ErrStr(rc));
  }
  goto vdbe_error_halt;

  /* Jump to here if the sqlite3_interrupt() API sets the interrupt
  ** flag.跳转到这里如果sqlite3_interrupt()API设置中断
* *旗帜。
  */
abort_due_to_interrupt:
  assert( db->u1.isInterrupted );
  rc = SQLITE_INTERRUPT;
  p->rc = rc;
  sqlite3SetString(&p->zErrMsg, db, "%s", sqlite3ErrStr(rc));
  goto vdbe_error_halt;
}
