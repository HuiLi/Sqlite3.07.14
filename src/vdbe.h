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
** Header file for the Virtual DataBase Engine (VDBE)
**
** This header defines the interface to the virtual database engine
** or VDBE.  The VDBE implements an abstract machine that runs a
** simple program to access and modify the underlying database.
** vdbe.h定义了VDBE的接口.
** 头文件,不看不行啊
*/
#ifndef _SQLITE_VDBE_H_
#define _SQLITE_VDBE_H_
#include <stdio.h>

/*
** A single VDBE is an opaque structure named "Vdbe".  Only routines
** in the source file sqliteVdbe.c are allowed to see the insides
** of this structure.
** Vdbe结构体是一个不透明的结构体,定义在vdbeInt.h中,只有sqliteVdbe.c源文件里面的程序才被
** 允许进入查看它的结构体内部.
*/
typedef struct Vdbe Vdbe;

/*
** The names of the following types declared in vdbeInt.h are required
** for the VdbeOp definition.
** 下面几个结构体都定义在vdbeInt.h中,他们是VdbeOp结构体需要定义的.
*/
typedef struct VdbeFunc VdbeFunc;//在结构体里面定义了函数和所需的相关变量参数 函数为 FuncDef *pFunc;
typedef struct Mem Mem;
typedef struct SubProgram SubProgram;//一个子程序用于执行一个触发程序

/*
** A single instruction of the virtual machine has an opcode
** and as many as three operands.  The instruction is recorded
** as an instance of the following structure:
** 一个简单的虚拟机指令包含一个操作码和三个操作数。这条指令被记录下来用来对下面的结构体进行实例化。
*/
struct VdbeOp {
	u8 opcode;          /* What operation to perform 决定执行的操作-操作码来决定 然后调用vdbe.c里面对应的操作*/
	signed char p4type; /* One of the P4_xxx constants for 其中一个p4常量*/
	u8 opflags;         /* Mask of the OPFLG_* flags in opcodes.h opcodes.h中OPFLG_*标识*/
	u8 p5;              /* Fifth parameter is an unsigned character 第五个参数是一个无符号字符*/
	int p1;             /* First operand 操作数*/
	int p2;             /* Second parameter (often the jump destination) 第二个参数经常是跳转的目的地*/
	int p3;             /* The third parameter 第三个参数-p3为一个以null结尾的字符串或者其他结构的指针*/
	union {             /* fourth parameter */
		int i;                 /* Integer value if p4type==P4_INT32 */
		void *p;               /* Generic pointer */
		char *z;               /* Pointer to data for string (char array) types 指向数据的指针*/
		i64 *pI64;             /* Used when p4type is P4_INT64 */
		double *pReal;         /* Used when p4type is P4_REAL */
		FuncDef *pFunc;        /* Used when p4type is P4_FUNCDEF */
		VdbeFunc *pVdbeFunc;   /* Used when p4type is P4_VDBEFUNC */
		CollSeq *pColl;        /* Used when p4type is P4_COLLSEQ */
		Mem *pMem;             /* Used when p4type is P4_MEM */
		VTable *pVtab;         /* Used when p4type is P4_VTAB */
		KeyInfo *pKeyInfo;     /* Used when p4type is P4_KEYINFO */
		int *ai;               /* Used when p4type is P4_INTARRAY */
		SubProgram *pProgram;  /* Used when p4type is P4_SUBPROGRAM */
		int(*xAdvance)(BtCursor *, int *);
	} p4;
#ifdef SQLITE_DEBUG          /* SQLITE_DEBUG是个字符型指针*/
	char *zComment;          /* Comment to improve readability     zComment（说明）增加可读性*/
#endif
#ifdef VDBE_PROFILE
	int cnt;                 /* Number of times this instruction was executed 本条指令被执行次数*/
	u64 cycles;              /* Total time spent executing this instruction 执行本条指令花费的总时间*/
#endif
};
typedef struct VdbeOp VdbeOp;


/*
** A sub-routine used to implement a trigger program.一个用来实现触发程序的子程序
*/
struct SubProgram {
	VdbeOp *aOp;                  /* Array of opcodes for sub-program 子程序里的一系列操作码*/
	int nOp;                      /* Elements in aOp[] aOp[]数组里的元素*/
	int nMem;                     /* Number of memory cells required 需要的内存单元数量*/
	int nCsr;                     /* Number of cursors required 需要的游标数量*/
	int nOnce;                    /* Number of OP_Once instructions 需要的OP_Once指令数*/
	void *token;                  /* id that may be used to recursive triggers id可用于递归触发器*/
	SubProgram *pNext;            /* Next sub-program already visited 下一个已经被访问的子程序*/
};

/*
** A smaller version of VdbeOp used for the VdbeAddOpList() function because
** it takes up less space.
**VdbeOp的缩小版用于VdbeAddOpList()函数，因为它占用更少的空间。只有四个参数,包含操作码、P1、P2和P3。
*/
struct VdbeOpList {
	u8 opcode;          /* What operation to perform 选择操作*/
	signed char p1;     /* First operand */
	signed char p2;     /* Second parameter (often the jump destination) */
	signed char p3;     /* Third parameter */
};
typedef struct VdbeOpList VdbeOpList;

/*
** Allowed values of VdbeOp.p4type
*/
#define P4_NOTUSED    0   /* The P4 parameter is not used P4参数没有被用*/
#define P4_DYNAMIC  (-1)  /* Pointer to a string obtained from sqliteMalloc() 指向从sqliteMalloc()函数分配的字符串的指针*/
#define P4_STATIC   (-2)  /* Pointer to a static string 指向一个静态字符串的指针*/
#define P4_COLLSEQ  (-4)  /* P4 is a pointer to a CollSeq structure P4是指向CollSeq结构体的指针*/
#define P4_FUNCDEF  (-5)  /* P4 is a pointer to a FuncDef structure P4是指向FuncDef结构体的指针*/
#define P4_KEYINFO  (-6)  /* P4 is a pointer to a KeyInfo structure P4是指向KeyInfo结构体的指针*/
#define P4_VDBEFUNC (-7)  /* P4 is a pointer to a VdbeFunc structure P4是指向VdbeFunc结构体的指针*/
#define P4_MEM      (-8)  /* P4 is a pointer to a Mem*structure P4是指向Mem*结构体的指针,指向指针结构体的指针?????*/
#define P4_TRANSIENT  0   /* P4 is a pointer to a transient string P4是指向transient字符串的指针*/
#define P4_VTAB     (-10) /* P4 is a pointer to an sqlite3_vtab structure P4是指向sqlite3_vtab结构体的指针*/
#define P4_MPRINTF  (-11) /* P4 is a string obtained from sqlite3_mprintf() P4是从sqlite3_mprintf()函数得到的一个字符串*/
#define P4_REAL     (-12) /* P4 is a 64-bit floating point value P4是一个64位浮点数指针*/
#define P4_INT64    (-13) /* P4 is a 64-bit signed integer P4是一个64位有符号整形指针*/
#define P4_INT32    (-14) /* P4 is a 32-bit signed integer P4是一个32位有符号整形指针*/
#define P4_INTARRAY (-15) /* P4 is a vector of 32-bit integers P4是一个32位整形的vector(C++里的???怎么跑这里了?调用STL库了????)*/
#define P4_SUBPROGRAM  (-18) /* P4 is a pointer to a SubProgram structure P4是指向SubProgram结构体的指针*/
#define P4_ADVANCE  (-19) /* P4 is a pointer to BtreeNext() or BtreePrev() P4是指向BtreeNext()函数或者BtreePrev()函数的指针*/

/* When adding a P4 argument using P4_KEYINFO, a copy of the KeyInfo structure
** is made.  That copy is freed when the Vdbe is finalized.  But if the
** argument is P4_KEYINFO_HANDOFF, the passed in pointer is used.  It still
** gets freed when the Vdbe is finalized so it still should be obtained
** from a single sqliteMalloc().  But no copy is made and the calling
** function should *not* try to free the KeyInfo.TODO
** 当使用P4_KEYINFO添加一个P4参数时,KeyInfo结构体的一个副本就被建立.当VDBE执行结束时这个副本就被释放了.但是如果参数是使用P4_KEYINFO_HANDOFF添加的
** 而且在指针中使用过了,当Vdbe执行结束后,它还是会被释放,所以它仍然需要一个单独的sqliteMalloc()函数来生成一下,但它就没有副本,调用函数也就不用释放KeyInfo了
*/
#define P4_KEYINFO_HANDOFF (-16)
#define P4_KEYINFO_STATIC  (-17)

/*
** The Vdbe.aColName array contains 5n Mem structures, where n is the
** number of columns of data returned by the statement.
** Vdbe.aColName数组里有5N个Mem结构体,N是由语句返回来的数据行的行数.
*/
#define COLNAME_NAME     0
#define COLNAME_DECLTYPE 1     //decltype作为操作符，用于查询表达式的数据类型
#define COLNAME_DATABASE 2
#define COLNAME_TABLE    3
#define COLNAME_COLUMN   4
#ifdef SQLITE_ENABLE_COLUMN_METADATA
# define COLNAME_N        5      /* Number of COLNAME_xxx symbols */
#else
# ifdef SQLITE_OMIT_DECLTYPE
#   define COLNAME_N      1      /* Store only the name 仅存名字*/
# else
#   define COLNAME_N      2      /* Store the name and decltype 存名字和姓名*/
# endif
#endif

/*
** The following macro converts a relative address in the p2 field
** of a VdbeOp structure into a negative number so that
** sqlite3VdbeAddOpList() knows that the address is relative.  Calling
** the macro again restores the address.
** 这个宏把VdbeOp结构体里的p2字节里的相对地址转换为绝对地址,这样sqlite3VdbeAddOpList()函数就识别出
** 这个地址是相对的,然后再次调用这个宏储存这个地址.
*/
#define ADDR(X)  (-1-(X))

/*
** The makefile scans the vdbe.c source file and creates the "opcodes.h"
** header file that defines a number for each opcode used by the VDBE.
** 生成文件扫描vdbe.c源文件并使用VDBE（虚拟机）生成头文件opcodes.h，
** 这个头文件为每个操作码定义一个数字。
*/
#include "opcodes.h"

/*
** Prototypes for the VDBE interface.  See comments on the implementation
** for a description of what each of these routines does.
** 下面是VDBE接口的原形，可以查看每个程序怎么实现的描述
*/
Vdbe *sqlite3VdbeCreate(sqlite3*);//创建一个新的虚拟数据库引擎
int sqlite3VdbeAddOp0(Vdbe*, int);
int sqlite3VdbeAddOp1(Vdbe*, int, int);
int sqlite3VdbeAddOp2(Vdbe*, int, int, int);
int sqlite3VdbeAddOp3(Vdbe*, int, int, int, int);
int sqlite3VdbeAddOp4(Vdbe*, int, int, int, int, const char *zP4, int);
int sqlite3VdbeAddOp4Int(Vdbe*, int, int, int, int, int);//添加一个包含 p4 值为整数的操作码
int sqlite3VdbeAddOpList(Vdbe*, int nOp, VdbeOpList const *aOp);//添加整个操作列表到操作堆栈。返回第一个操作添加的地址。
void sqlite3VdbeAddParseSchemaOp(Vdbe*, int, char*);//这个实例被解决是从它需要的 sqlite3VdbeAddOp4()到也需要标记被使用的所有 btree
void sqlite3VdbeChangeP1(Vdbe*, u32 addr, int P1);//为一个特定的指令改变操作数P1的值
void sqlite3VdbeChangeP2(Vdbe*, u32 addr, int P2);//为一个特定的指令改变操作数P2的值
void sqlite3VdbeChangeP3(Vdbe*, u32 addr, int P3);//为一个特定的指令改变操作数P3的值
void sqlite3VdbeChangeP5(Vdbe*, u8 P5);//为一个特定的指令改变操作数P5的值
void sqlite3VdbeJumpHere(Vdbe*, int addr);//更改的 P2 操作数的指令地址，以便它指向下一条指令进行编码的地址。
void sqlite3VdbeChangeToNoop(Vdbe*, int addr);//改变操作码的地址为OP_Noop
void sqlite3VdbeChangeP4(Vdbe*, int addr, const char *zP4, int N);//为一个特定的指令改变操作数P1的值
void sqlite3VdbeUsesBtree(Vdbe*, int);//声明的Vdbe的BTree对象在db - >aDb[i]中被使用
VdbeOp *sqlite3VdbeGetOp(Vdbe*, int);//创建一个还没有被编码的新的符号标签的指令
int sqlite3VdbeMakeLabel(Vdbe*);//创建一个还没有被编码的新的符号标签的指令
void sqlite3VdbeRunOnlyOnce(Vdbe*);//标志VDBE并且只能运行一次
void sqlite3VdbeDelete(Vdbe*);//删除整个VDBE
void sqlite3VdbeDeleteObject(sqlite3*, Vdbe*);//将函数sqlite3VdbeDeleteObject的第二个参数关联Vdbe结构相关的资源全部释放掉
void sqlite3VdbeMakeReady(Vdbe*, Parse*);//在创建虚拟机后，为第一次执行准备一个虚拟机
int sqlite3VdbeFinalize(Vdbe*);//在程序执行过后清除VDBE占用的资源并删除这个VDBE
void sqlite3VdbeResolveLabel(Vdbe*, int);
int sqlite3VdbeCurrentAddr(Vdbe*);//将VDBE倒回为VDBE准备运行时的状态
#ifdef SQLITE_DEBUG
int sqlite3VdbeAssertMayAbort(Vdbe *, int);
void sqlite3VdbeTrace(Vdbe*, FILE*);
#endif
void sqlite3VdbeResetStepResult(Vdbe*);
void sqlite3VdbeRewind(Vdbe*);//将VDBE倒回为VDBE准备运行时的状态
int sqlite3VdbeReset(Vdbe*);//一次运行处理后清除这个VDBE的内存资源等信息但是并不是直接将其删除
void sqlite3VdbeSetNumCols(Vdbe*, int);//设置通过SQL声明返回的第idx列的名称。zName必须是一个空字符结尾的字符串的指针。
int sqlite3VdbeSetColName(Vdbe*, int, int, const char *, void(*)(void*));//设置通过SQL声明返回的第idx列的名称。zName必须是一个空字符结尾的字符串的指针。
void sqlite3VdbeCountChanges(Vdbe*);//当数据库结束或者被重置的时候，设置一个标志在vdbe更新计数器
sqlite3 *sqlite3VdbeDb(Vdbe*);//移除数据库和Vdbe的连接关系
void sqlite3VdbeSetSql(Vdbe*, const char *z, int n, int);//记住事先声明的SQL语句字符串
void sqlite3VdbeSwap(Vdbe*, Vdbe*);//交换两个 VDBE 结构之间的所有内容
VdbeOp *sqlite3VdbeTakeOpArray(Vdbe*, int*, int*);//返回一个指向sqlite3_value结构体
sqlite3_value *sqlite3VdbeGetValue(Vdbe*, int, u8);//返回一个指向sqlite3_value结构体
void sqlite3VdbeSetVarmask(Vdbe*, int);//这个函数可以重复准备SQL语句使其能够导致一个更好的执行计划
#ifndef SQLITE_OMIT_TRACE
char *sqlite3VdbeExpandSql(Vdbe*, const char*);
#endif

void sqlite3VdbeRecordUnpack(KeyInfo*, int, const void*, UnpackedRecord*);//给定nKey字节大小的一条记录的二进制数据存在pKey[]，通过解码记录的第四个参数来填充UnpackedRecord结构实例。
int sqlite3VdbeRecordCompare(int, const void*, UnpackedRecord*);//这个函数主要用来比较两个表的行数或者指定的索引记录
UnpackedRecord *sqlite3VdbeAllocUnpackedRecord(KeyInfo *, char *, int, char **);//这个函数被用于给UnpackedRecord结构分配一个足够大的内存空间

#ifndef SQLITE_OMIT_TRIGGER
void sqlite3VdbeLinkSubProgram(Vdbe *, SubProgram *);//链接子程序对象作为第二个参数传递到Vdbe.pSubProgram链表
#endif


#ifndef NDEBUG
void sqlite3VdbeComment(Vdbe*, const char*, ...);
# define VdbeComment(X)  sqlite3VdbeComment X
void sqlite3VdbeNoopComment(Vdbe*, const char*, ...);
# define VdbeNoopComment(X)  sqlite3VdbeNoopComment X
#else
# define VdbeComment(X)
# define VdbeNoopComment(X)
#endif

#endif
