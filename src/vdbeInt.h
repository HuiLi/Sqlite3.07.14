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
** This is the header file for information that is private to the
** VDBE.  This information used to all be at the top of the single
** source code file "vdbe.c".  When that file became too big (over
** 6000 lines long) it was split up into several smaller files and
** this header information was factored out.
**这是一个头文件，文件中包含的信息对于VDBE（）是私有的。这些信息全部都**用在vdbe.c这个资源文件之上。当这个文件太大时（超过6000行时）将会被分成几个较小的文件，这个文件就是被分解出来的。
*/
#ifndef _VDBEINT_H_
#define _VDBEINT_H_

/*
** SQL is translated into a sequence of instructions to be
** executed by a virtual machine.  Each instruction is an instance
** of the following structure.
**（为了能够被虚拟机执行SQl语句被翻译成了一些有序的指令。每一条指令都是下面结构的一个实例。）
*/
typedef struct VdbeOp Op;

/*
** Boolean values
*/
typedef unsigned char Bool;

/* Opaque type used by code in vdbesort.c  
**Opaque类型被vdbesort.c文件中的代码使用
*/
typedef struct VdbeSorter VdbeSorter;

/* Opaque type used by the explainer */
typedef struct Explain Explain;

/*
** A cursor is a pointer into a single BTree within a database file.
**一个游标是一个数据库文件中单一BTree的指示器。
** The cursor can seek to a BTree entry with a particular key, or
** loop over all entries of the Btree.  You can also insert new BTree
** entries or retrieve the key or data from the entry that the cursor
** is currently pointing to.
**这个指针能够找到拥有特定值的一个BTree的入口，或者遍历BTree的所有入
**口。你能够从新的BTree入口或者从这个指针通常指向的的入口检索值或者数据。
** Every cursor that the virtual machine has open is represented by an
** instance of the following structure.
**每一个虚拟机已经开启的指针都代表着一个一下结构的常量。
*/
struct VdbeCursor {
  BtCursor *pCursor;    /* The cursor structure of the backend 后台的指针结构*/
  Btree *pBt;           /* Separate file holding temporary table 分开拥有临时表的文件 */
  KeyInfo *pKeyInfo;    /* Info about index keys needed by index cursors 索引指针所需要的索引键的有关信息*/
  int iDb;              /* Index of cursor database in db->aDb[] (or -1) 指针数据库的检索*/
  int pseudoTableReg;   /* Register holding pseudotable content. 记录包含伪随机号码表的目录*/
  int nField;           /* Number of fields in the header 头文件中的区域数量*/
  Bool zeroed;          /* True if zeroed out and ready for reuse如果为零则为真并且准备被复用。 */
  Bool rowidIsValid;    /* True if lastRowid is valid 如果最后一行的行ID是有效地则为真。*/
  Bool atFirst;         /* True if pointing to first entry 如果指向第一个入口则为真*/
  Bool useRandomRowid;  /* Generate new record numbers semi-randomly 半随机的生成新的记录号码*/
  Bool nullRow;         /* True if pointing to a row with no data 如果指向某一没有数据的行则为真*/
  Bool deferredMoveto;  /* A call to sqlite3BtreeMoveto() is needed 对sqlite3BtreeMoveto()方法的访问时需要的*/
  Bool isTable;         /* True if a table requiring integer keys 如果一个表要求有整数键则为真*/
  Bool isIndex;         /* True if an index containing keys only - no data 如果一个索引所包含的键没有数据则为真*/
  Bool isOrdered;       /* True if the underlying table is BTREE_UNORDERED 基础数据表是无顺序的则为真*/
  Bool isSorter;        /* True if a new-style sorter 如果是新的种类则为真*/
  sqlite3_vtab_cursor *pVtabCursor;  /* The cursor for a virtual table 虚拟表的指针*/
  const sqlite3_module *pModule;     /* Module for cursor pVtabCursor 指针pVtabCursor的模块 */
  i64 seqCount;         /* Sequence counter 指令计数器*/
  i64 movetoTarget;     /* Argument to the deferred sqlite3BtreeMoveto() 对推迟的方法sqlite3BtreeMoveto() 的内容提要*/
  i64 lastRowid;        /* Last rowid from a Next or NextIdx operation最后一个行id来自下一个操作 */
  VdbeSorter *pSorter;  /* Sorter object for OP_SorterOpen cursors OP_SorterOpen指针的分类对象*/

  /* Result of last sqlite3BtreeMoveto() done by an OP_NotExists or 
  ** OP_IsUnique opcode on this cursor.
  **OP_NotExists或者 OP_IsUnique在这个指针上的操作码所形成的的上一个sqlite3BtreeMoveto()方法的结果。*/
  int seekResult;

  /* Cached information about the header for the data record that the
  ** cursor is currently pointing to.  Only valid if cacheStatus matches
  ** Vdbe.cacheCtr.  Vdbe.cacheCtr will never take on the value of
  ** CACHE_STALE and so setting cacheStatus=CACHE_STALE guarantees that
  ** the cache is out of date.
  ** 缓存的的关于头文件的信息，这个头文件正是这个指针现在指向的数据记录。仅在当缓存状态与Vdbe.cacheCtr匹配时才有效。
  ** Vdbe.cacheCtr将永远取不到CACHE_STALE的值，因此设置cacheStatus=CACHE_STALE的保证了缓存已经过期。
  ** aRow might point to (ephemeral) data for the current row, or it might
  ** be NULL.一行可能指向（临时的）当前行的数据，否则它可能为空。
  */
  u32 cacheStatus;      /* Cache is valid if this matches Vdbe.cacheCtr 如果这匹配了Vdbe.cacheCtr，则缓存是是有效的*/
  int payloadSize;      /* Total number of bytes in the record 记录中所有的字节数的总和*/
  u32 *aType;           /* Type values for all entries in the record 记录中所有入口的类型值*/
  u32 *aOffset;         /* Cached offsets to the start of each columns data */
  u8 *aRow;             /* Data for the current row, if all on one page 当前行的数据，如果所有的数据都在同一页*/
};
typedef struct VdbeCursor VdbeCursor;

/*
** When a sub-program is executed (OP_Program), a structure of this type
** is allocated to store the current value of the program counter, as
** well as the current memory cell array and various other frame specific
** values stored in the Vdbe struct. When the sub-program is finished, 
** these values are copied back to the Vdbe from the VdbeFrame structure,
** restoring the state of the VM to as it was before the sub-program
** began executing.
** 当一个子线程被执行，这个类型的结构将被分配存储当前程序计数器的值，而且当前存储单元阵列和各种其它的框架特定值存储在vdbe结构当中。
**当子程序运行结束，这些值将从VDBE框架当中复制回来。存储虚拟机的状态当做它在子程序开始执行之前。
** The memory for a VdbeFrame object is allocated and managed by a memory
** cell in the parent (calling) frame. When the memory cell is deleted or
** overwritten, the VdbeFrame object is not freed immediately. Instead, it
** is linked into the Vdbe.pDelFrame list. The contents of the Vdbe.pDelFrame
** list is deleted when the VM is reset in VdbeHalt(). The reason for doing
** this instead of deleting the VdbeFrame immediately is to avoid recursive
** calls to sqlite3VdbeMemRelease() when the memory cells belonging to the
** child frame are released.
** 一个虚拟机框架对象的内存由父框架的一个存储单元中被分配和管理。当内存单元被删除或者重写，虚拟机框架对象不会被立即释放。相反的，
** 他将被链接到Vdbe.pDelFrame清单中。当虚拟机在中重启后，Vdbe.pDelFrame的目录清单被删除。这么做而不是立刻删除虚拟机框架对象是为了
** 避免当属于子框架的存储单元被释放时循环调用sqlite3VdbeMemRelease()方法。
** The currently executing frame is stored in Vdbe.pFrame. Vdbe.pFrame is
** set to NULL if the currently executing frame is the main program.
** 当前的执行框架被存储在Vdbe.pFrame里面。如果当前的执行框架在主程序当中，Vdbe.pFrame将被置为空。 
*/
typedef struct VdbeFrame VdbeFrame;
struct VdbeFrame {
  Vdbe *v;                /* VM this frame belongs to 这个框架所属的虚拟机*/
  VdbeFrame *pParent;     /* Parent of this frame, or NULL if parent is main 框架的父类是主程序否则为空*/
  Op *aOp;                /* Program instructions for parent frame 针对父框架的程序指令 */
  Mem *aMem;              /* Array of memory cells for parent frame 父框架的存储单元数组*/
  u8 *aOnceFlag;          /* Array of OP_Once flags for parent frame 父框架的OP_Once标记数组*/
  VdbeCursor **apCsr;     /* Array of Vdbe cursors for parent frame  父框架的vdbe指针数组*/
  void *token;            /* Copy of SubProgram.token */
  i64 lastRowid;          /* Last insert rowid (sqlite3.lastRowid) 最后插入的行id*/
  u16 nCursor;            /* Number of entries in apCsr apCsr的入口数目*/
  int pc;                 /* Program Counter in parent (calling) frame 父框架调用的程序计数器*/
  int nOp;                /* Size of aOp array aOp数组的长度*/
  int nMem;               /* Number of entries in aMem aMem的入口数目*/
  int nOnceFlag;          /* Number of entries in aOnceFlag */
  int nChildMem;          /* Number of memory cells for child frame 子框架的内存单元数目*/
  int nChildCsr;          /* Number of cursors for child frame 子框架的指针数目*/
  int nChange;            /* Statement changes (Vdbe.nChanges)     声明改变*/
};

#define VdbeFrameMem(p) ((Mem *)&((u8 *)p)[ROUND8(sizeof(VdbeFrame))])

/*
** A value for VdbeCursor.cacheValid that means the cache is always invalid.
** vdbe指针的值。cachValid意味着缓存总是无效的。
*/
#define CACHE_STALE 0

/*
** Internally, the vdbe manipulates nearly all SQL values as Mem
** structures. Each Mem struct may cache multiple representations (string,
** integer etc.) of the same value.
** 在内部，vdbe操作几乎所有的sql值都以Mem的结构来实现。每一个Mem结构可以缓存多种同样的代表值（比如字符串，整形的结构）
*/
struct Mem {
  sqlite3 *db;        /* The associated database connection 先关联的数据库连接*/
  char *z;            /* String or BLOB value 字符串或者Blog值*/
  double r;           /* Real value 真值*/
  union {
    i64 i;              /* Integer value used when MEM_Int is set in flags 整形只在当MEM_Int被设置标记时使用*/
    int nZero;          /* Used when bit MEM_Zero is set in flags 大致同上*/
    FuncDef *pDef;      /* Used only when flags==MEM_Agg 仅当标示与MEM_Agg相等时使用*/
    RowSet *pRowSet;    /* Used only when flags==MEM_RowSet大致同上 */
    VdbeFrame *pFrame;  /* Used when flags==MEM_Frame */
  } u;
  int n;              /* Number of characters in string value, excluding '\0' 字符串值中出去/0的的字符个数*/
  u16 flags;          /* Some combination of MEM_Null, MEM_Str, MEM_Dyn, etc. 这几种标示的结合*/
  u8  type;           /* One of SQLITE_NULL, SQLITE_TEXT, SQLITE_INTEGER, etc 其中的一种数据类型*/
  u8  enc;            /* SQLITE_UTF8, SQLITE_UTF16BE, SQLITE_UTF16LE 不同的编码方式*/
#ifdef SQLITE_DEBUG
  Mem *pScopyFrom;    /* This Mem is a shallow copy of pScopyFrom 这个Mem是对pScopyFrom的浅拷贝*/
  void *pFiller;      /* So that sizeof(Mem) is a multiple of 8 以便运算符是八的倍数*/
#endif
  void (*xDel)(void *);  /* If not null, call this function to delete Mem.z 如果为空，则调用方法删除Mem.z*/
  char *zMalloc;      /* Dynamic buffer allocated by sqlite3_malloc() 动态的缓冲区由sqlite3_malloc（）方法去分配*/
};

/* One or more of the following flags are set to indicate the validOK
** representations of the value stored in the Mem struct.
**一个或者更多的下面的标示被设置是为了证明它有效的正确的代表了Mem结构中存储的值。
** If the MEM_Null flag is set, then the value is an SQL NULL value.
** No other flags may be set in this case.
**如果MEM_Null标记被设置，那么这个值是一个空的sql值。当没有其他标记被设置时这种情况才会发生。
** If the MEM_Str flag is set then Mem.z points at a string representation.
**如果MEM——Str这个表示被设置，那么Mem.z指向一个字符串类型的代表。
** Usually this is encoded in the same unicode encoding as the main
** database (see below for exceptions). If the MEM_Term flag is also
** set, then the string is nul terminated. The MEM_Int and MEM_Real 
** flags may coexist with the MEM_Str flag.
**通常这将被编译成和主数据库相同的编码格式。（下面的是特例）如果MEM_Term标示也被设置，那么这个字符串是以空结尾的。
** MEM_Int和MEM_Real标示可能与MEM_Str标示同时存在。
*/
#define MEM_Null      0x0001   /* Value is NULL */
#define MEM_Str       0x0002   /* Value is a string */
#define MEM_Int       0x0004   /* Value is an integer */
#define MEM_Real      0x0008   /* Value is a real number 值为一个实数*/
#define MEM_Blob      0x0010   /* Value is a BLOB 只为一个块儿*/
#define MEM_RowSet    0x0020   /* Value is a RowSet object 值为一个行设置对象*/
#define MEM_Frame     0x0040   /* Value is a VdbeFrame object 值为一个Vdbe框架对象*/
#define MEM_Invalid   0x0080   /* Value is undefined */
#define MEM_TypeMask  0x00ff   /* Mask of type bits 隐藏类型的二进制数*/

/* Whenever Mem contains a valid string or blob representation, one of
** the following flags must be set to determine the memory management
** policy for Mem.z.  The MEM_Term flag tells us whether or not the
** string is \000 or \u0000 terminated
**任何时候Mem包含一个字符串或者块儿代表，下面其中之一的标志必须被设置以决定Mem.z的内存管理策略
**Mem_Term标记告诉我们无论是否这个字符串是\00还是\u000都将停止。
*/
#define MEM_Term      0x0200   /* String rep is nul terminated 字符串代替符以空结尾*/
#define MEM_Dyn       0x0400   /* Need to call sqliteFree() on Mem.z 在Mem.z中需要调用sqliteFree()方法*/
#define MEM_Static    0x0800   /* Mem.z points to a static string  Mem.z指向一个静态的字符串*/
#define MEM_Ephem     0x1000   /* Mem.z points to an ephemeral string  Mem.z指向一个短字符串*/
#define MEM_Agg       0x2000   /* Mem.z points to an agg function context Mem.z指向一个自增方法的上下文*/
#define MEM_Zero      0x4000   /* Mem.i contains count of 0s appended to blob Mem.i包含块儿中附加0s的数目*/
#ifdef SQLITE_OMIT_INCRBLOB
  #undef MEM_Zero
  #define MEM_Zero 0x0000
#endif

/*
** Clear any existing type flags from a Mem and replace them with f
**将Mem中的任何存在的标记类型都清空然后用f替换。
*/
#define MemSetTypeFlag(p, f) \
   ((p)->flags = ((p)->flags&~(MEM_TypeMask|MEM_Zero))|f)

/*
** Return true if a memory cell is not marked as invalid.  This macro
** is for use inside assert() statements only.
**如果内存单元没有被标记为空则返回真。这个宏只是用来实现assert（）函数的内部声明。
*/
#ifdef SQLITE_DEBUG
#define memIsValid(M)  ((M)->flags & MEM_Invalid)==0
#endif


/* A VdbeFunc is just a FuncDef (defined in sqliteInt.h) that contains
** additional information about auxiliary information bound to arguments
** of the function.
** Vdbefunc仅仅是一个Funcdef结构体类型，它包含了方法参数的辅助信息绑定的有关信息。
** This is used to implement the sqlite3_get_auxdata()
** and sqlite3_set_auxdata() APIs.  The "auxdata" is some auxiliary data
** that can be associated with a constant argument to a function. 
** 这是一个用于实现sqlite3_get_auxdata()和sqlite3_set_auxdata()两个方法的程序接口。
** auxdata是是一些辅助数据，这些辅助数据可被用于让一个常量和方法联系起来。
** This allows functions such as "regexp" to compile their constant regular
** expression argument once and reused the compiled code for multiple
** invocations.
** 这就允许了像regexp的方法马上编译其恒定的正则表达式的参数并且再重复调用方法时可以重复利用这些编译代码。
*/
struct VdbeFunc {
  FuncDef *pFunc;               /* The definition of the function */
  int nAux;                     /* Number of entries allocated for apAux[] apAux[]方法的入口分配数量*/
  struct AuxData {
    void *pAux;                   /* Aux data for the i-th argument i-th的参数结构体*/
    void (*xDelete)(void *);      /* Destructor for the aux data 这个结构体的析构函数*/
  } apAux[1];                   /* One slot for each function argument 对每一个函数的参数的分配一个位置*/
};

/*
** The "context" argument for a installable function.  A pointer to an
** instance of this structure is the first argument to the routines used
** implement the SQL functions.
** 一个可安装函数的上下文参数。这个结构体实例的指针是这个程序实现SQL语句查询方法的第一个参数。
** There is a typedef for this structure in sqlite.h.  So all routines,
** even the public interface to SQLite, can use a pointer to this structure.
** But this file is the only place where the internal details of this
** structure are known.
** 对于这个结构体在sqlite.h有里面有一个定义类型。因此所有的程序，即便是sqlite的公有接口也能够对这个结构体用指针。
** This structure is defined inside of vdbeInt.h because it uses substructures
** (Mem) which are only defined there.
**这个结构体在本文件中的内部被定义，因为使用了Mem的底部构造，这些构造仅被在这里定义。
*/
struct sqlite3_context {
  FuncDef *pFunc;       /* Pointer to function information.  MUST BE FIRST */
  VdbeFunc *pVdbeFunc;  /* Auxilary data, if created. 辅助数据*/
  Mem s;                /* The return value is stored here 返回值被存在这里*/
  Mem *pMem;            /* Memory cell used to store aggregate context 用来存储上下文集合的内存空间*/
  CollSeq *pColl;       /* Collating sequence 核对结果*/
  int isError;          /* Error code returned by the function. 方法返回的错误代码*/
  int skipFlag;         /* Skip skip accumulator loading if true 如果为真则跳过累加器加载*/
};

/*
** An Explain object accumulates indented output which is helpful
** in describing recursive data structures.
** 一个解释对象积聚收约束的缩进，这有利于描述递归的数据结构。
*/
struct Explain {
  Vdbe *pVdbe;       /* Attach the explanation to this Vdbe 附加上对VDBE的解释*/
  StrAccum str;      /* The string being accumulated 被累积的字符串*/
  int nIndent;       /* Number of elements in aIndent缩进中的元素数目 */
  u16 aIndent[100];  /* Levels of indentation 缩进级别*/
  char zBase[100];   /* Initial space 初始空间*/
};

/*
** An instance of the virtual machine.  This structure contains the complete
** state of the virtual machine.
** 一个虚拟机的实体。这个结构包含了虚拟机完整的状态。
** The "sqlite3_stmt" structure pointer that is returned by sqlite3_prepare()
** is really a pointer to an instance of this structure.
** sqlite3_stmt这个结构的指针由sqlite3_prepare()方法返回，它是这个结构实体的一个真实指针。
** The Vdbe.inVtabMethod variable is set to non-zero for the duration of
** any virtual table method invocations made by the vdbe program. It is
** set to 2 for xDestroy method calls and 1 for all other methods.
** Vdbe.inVtab方法的变量在vdbe程序产生的任何虚拟表方法调用期间被置为零。
**  This variable is used for two purposes: to allow xDestroy methods to execute
** "DROP TABLE" statements and to prevent some nasty side effects of
** malloc failure when SQLite is invoked recursively by a virtual table 
** method function.
** 这个变量主要用于两个目的：允许xDestroy方法实现删除表的声明以及为了防止分配内存失败产生的副作用。
*/
struct Vdbe {
  sqlite3 *db;            /* The database connection that owns this statement 数据库连接拥有这个声明。*/
  Op *aOp;                /* Space to hold the virtual machine's program 存储虚拟机的空间*/
  Mem *aMem;              /* The memory locations 内存地址存储单元*/
  Mem **apArg;            /* Arguments to currently executing user function 实现当前用户方法需要的参数*/
  Mem *aColName;          /* Column names to return 返回的列名*/
  Mem *pResultSet;        /* Pointer to an array of results 一个数组类结果的指针*/
  int nMem;               /* Number of memory locations currently allocated 通常分配的内存单元数目*/
  int nOp;                /* Number of instructions in the program 程序中的指令数目*/
  int nOpAlloc;           /* Number of slots allocated for aOp[] 为这个方法分配的扩充单元*/
  int nLabel;             /* Number of labels used 使用的标签数目*/
  int *aLabel;            /* Space to hold the labels 保存标签的位置*/
  u16 nResColumn;         /* Number of columns in one row of the result set 列数目在某一行的结果设置*/
  u16 nCursor;            /* Number of slots in apCsr[] 这个方法中的扩充单元数目*/
  u32 magic;              /* Magic number for sanity checking 用于版本健全性检查的幻数*/
  char *zErrMsg;          /* Error message written here 错误信息*/
  Vdbe *pPrev,*pNext;     /* Linked list of VDBEs with the same Vdbe.db 把vdbes的列表与同一个Vdbe.db联系起来*/
  VdbeCursor **apCsr;     /* One element of this array for each open cursor对每一个开放的游标的数组其中之一的元素 */
  Mem *aVar;              /* Values for the OP_Variable opcode. 这个操作码的值*/
  char **azVar;           /* Name of variables 变量名*/
  ynVar nVar;             /* Number of entries in aVar[]方法入口数目 */
  ynVar nzVar;            /* Number of entries in azVar[] */
  u32 cacheCtr;           /* VdbeCursor row cache generation counter Vdbe游标行缓存生成计数器*/
  int pc;                 /* The program counter 程序计数器*/
  int rc;                 /* Value to return 返回值*/
  u8 errorAction;         /* Recovery action to do in case of an error防止错误的恢复操作 */
  u8 explain;             /* True if EXPLAIN present on SQL command 如果解释当前的sql命令则为真。*/
  u8 changeCntOn;         /* True to update the change-counter 更新改变数目为真*/
  u8 expired;             /* True if the VM needs to be recompiled 如果虚拟机需要被编译则为真。*/
  u8 runOnlyOnce;         /* Automatically expire on reset 重置时自动失效*/
  u8 minWriteFileFormat;  /* Minimum file format for writable database files 对于可写数据库文件的最小的文件格式*/
  u8 inVtabMethod;        /* See comments above */
  u8 usesStmtJournal;     /* True if uses a statement journal 如果使用这个声明日志则为真*/
  u8 readOnly;            /* True for read-only statements 只读声明则为真*/
  u8 isPrepareV2;         /* True if prepared with prepare_v2()用此方法准备则为真 */
  int nChange;            /* Number of db changes made since last reset 自上一次重置数据库引起的数据库变化数目*/
  yDbMask btreeMask;      /* Bitmask of db->aDb[] entries referenced 被引用的数组入口的位掩码*/
  yDbMask lockMask;       /* Subset of btreeMask that requires a lock 位掩码的子集需要一个锁。*/
  int iStatement;         /* Statement number (or 0 if has not opened stmt) 声明编号（如果没被公开声明则为0）*/
  int aCounter[3];        /* Counters used by sqlite3_stmt_status() sqlite状态声明方法使用的计数器*/
#ifndef SQLITE_OMIT_TRACE
  i64 startTime;          /* Time when query started - used for profiling 查询开始的时间，用于程序分析*/
#endif
  i64 nFkConstraint;      /* Number of imm. FK constraints this VM 虚拟机的约束条件*/
  i64 nStmtDefCons;       /* Number of def. constraints when stmt started 声明开始的约束条件*/
  char *zSql;             /* Text of the SQL statement that generated this sql语句声明的文档生成这些。*/
  void *pFree;            /* Free this when deleting the vdbe 删除vdbe时释放它*/
#ifdef SQLITE_DEBUG
  FILE *trace;            /* Write an execution trace here, if not NULL如果不为空则写一个实现追踪的函数 */
#endif
#ifdef SQLITE_ENABLE_TREE_EXPLAIN
  Explain *pExplain;      /* The explainer解释器 */
  char *zExplain;         /* Explanation of data structures 数据结构的解释*/
#endif
  VdbeFrame *pFrame;      /* Parent frame 父框架*/
  VdbeFrame *pDelFrame;   /* List of frame objects to free on VM reset 虚拟机重置时需要释放的框架对象列表*/
  int nFrame;             /* Number of frames in pFrame list 在pFrame列表中的框架数目*/
  u32 expmask;            /* Binding to these vars invalidates VM 虚拟机绑定这些无效变量*/
  SubProgram *pProgram;   /* Linked list of all sub-programs used by VM虚拟机使用的所有的子程序的关联列表 */
  int nOnceFlag;          /* Size of array aOnceFlag[] 数组的大小*/
  u8 *aOnceFlag;          /* Flags for OP_Once OP_Once的标记*/
};

/*
** The following are allowed values for Vdbe.magic
**下面的是vdbe标示符被允许取的值
*/
#define VDBE_MAGIC_INIT     0x26bceaa5    /* Building a VDBE program 构造一个vdbe程序*/
#define VDBE_MAGIC_RUN      0xbdf20da3    /* VDBE is ready to execute 准备被实现的vdbe*/
#define VDBE_MAGIC_HALT     0x519c2973    /* VDBE has completed execution vdbe完成实现*/
#define VDBE_MAGIC_DEAD     0xb606c3c8    /* The VDBE has been deallocated vdbe内存空间被释放*/

/*
** Function prototypes函数原型
*/
void sqlite3VdbeFreeCursor(Vdbe *, VdbeCursor*);
void sqliteVdbePopStack(Vdbe*,int);
int sqlite3VdbeCursorMoveto(VdbeCursor*);
#if defined(SQLITE_DEBUG) || defined(VDBE_PROFILE)
void sqlite3VdbePrintOp(FILE*, int, Op*);
#endif
u32 sqlite3VdbeSerialTypeLen(u32);
u32 sqlite3VdbeSerialType(Mem*, int);
u32 sqlite3VdbeSerialPut(unsigned char*, int, Mem*, int);
u32 sqlite3VdbeSerialGet(const unsigned char*, u32, Mem*);
void sqlite3VdbeDeleteAuxData(VdbeFunc*, int);

int sqlite2BtreeKeyCompare(BtCursor *, const void *, int, int, int *);
int sqlite3VdbeIdxKeyCompare(VdbeCursor*,UnpackedRecord*,int*);
int sqlite3VdbeIdxRowid(sqlite3*, BtCursor *, i64 *);
int sqlite3MemCompare(const Mem*, const Mem*, const CollSeq*);
int sqlite3VdbeExec(Vdbe*);
int sqlite3VdbeList(Vdbe*);
int sqlite3VdbeHalt(Vdbe*);
int sqlite3VdbeChangeEncoding(Mem *, int);
int sqlite3VdbeMemTooBig(Mem*);
int sqlite3VdbeMemCopy(Mem*, const Mem*);
void sqlite3VdbeMemShallowCopy(Mem*, const Mem*, int);
void sqlite3VdbeMemMove(Mem*, Mem*);
int sqlite3VdbeMemNulTerminate(Mem*);
int sqlite3VdbeMemSetStr(Mem*, const char*, int, u8, void(*)(void*));
void sqlite3VdbeMemSetInt64(Mem*, i64);
#ifdef SQLITE_OMIT_FLOATING_POINT
# define sqlite3VdbeMemSetDouble sqlite3VdbeMemSetInt64
#else
  void sqlite3VdbeMemSetDouble(Mem*, double);
#endif
void sqlite3VdbeMemSetNull(Mem*);
void sqlite3VdbeMemSetZeroBlob(Mem*,int);
void sqlite3VdbeMemSetRowSet(Mem*);
int sqlite3VdbeMemMakeWriteable(Mem*);
int sqlite3VdbeMemStringify(Mem*, int);
i64 sqlite3VdbeIntValue(Mem*);
int sqlite3VdbeMemIntegerify(Mem*);
double sqlite3VdbeRealValue(Mem*);
void sqlite3VdbeIntegerAffinity(Mem*);
int sqlite3VdbeMemRealify(Mem*);
int sqlite3VdbeMemNumerify(Mem*);
int sqlite3VdbeMemFromBtree(BtCursor*,int,int,int,Mem*);
void sqlite3VdbeMemRelease(Mem *p);
void sqlite3VdbeMemReleaseExternal(Mem *p);
#define VdbeMemRelease(X)  \
  if((X)->flags&(MEM_Agg|MEM_Dyn|MEM_RowSet|MEM_Frame)) \
    sqlite3VdbeMemReleaseExternal(X);
int sqlite3VdbeMemFinalize(Mem*, FuncDef*);
const char *sqlite3OpcodeName(int);
int sqlite3VdbeMemGrow(Mem *pMem, int n, int preserve);
int sqlite3VdbeCloseStatement(Vdbe *, int);
void sqlite3VdbeFrameDelete(VdbeFrame*);
int sqlite3VdbeFrameRestore(VdbeFrame *);
void sqlite3VdbeMemStoreType(Mem *pMem);
int sqlite3VdbeTransferError(Vdbe *p);

#ifdef SQLITE_OMIT_MERGE_SORT
# define sqlite3VdbeSorterInit(Y,Z)      SQLITE_OK
# define sqlite3VdbeSorterWrite(X,Y,Z)   SQLITE_OK
# define sqlite3VdbeSorterClose(Y,Z)
# define sqlite3VdbeSorterRowkey(Y,Z)    SQLITE_OK
# define sqlite3VdbeSorterRewind(X,Y,Z)  SQLITE_OK
# define sqlite3VdbeSorterNext(X,Y,Z)    SQLITE_OK
# define sqlite3VdbeSorterCompare(X,Y,Z) SQLITE_OK
#else
int sqlite3VdbeSorterInit(sqlite3 *, VdbeCursor *);
void sqlite3VdbeSorterClose(sqlite3 *, VdbeCursor *);
int sqlite3VdbeSorterRowkey(const VdbeCursor *, Mem *);
int sqlite3VdbeSorterNext(sqlite3 *, const VdbeCursor *, int *);
int sqlite3VdbeSorterRewind(sqlite3 *, const VdbeCursor *, int *);
int sqlite3VdbeSorterWrite(sqlite3 *, const VdbeCursor *, Mem *);
int sqlite3VdbeSorterCompare(const VdbeCursor *, Mem *, int *);
#endif

#if !defined(SQLITE_OMIT_SHARED_CACHE) && SQLITE_THREADSAFE>0
  void sqlite3VdbeEnter(Vdbe*);
  void sqlite3VdbeLeave(Vdbe*);
#else
# define sqlite3VdbeEnter(X)
# define sqlite3VdbeLeave(X)
#endif

#ifdef SQLITE_DEBUG
void sqlite3VdbeMemAboutToChange(Vdbe*,Mem*);
#endif

#ifndef SQLITE_OMIT_FOREIGN_KEY
int sqlite3VdbeCheckFk(Vdbe *, int);
#else
# define sqlite3VdbeCheckFk(p,i) 0
#endif

int sqlite3VdbeMemTranslate(Mem*, u8);
#ifdef SQLITE_DEBUG
  void sqlite3VdbePrintSql(Vdbe*);
  void sqlite3VdbeMemPrettyPrint(Mem *pMem, char *zBuf);
#endif
int sqlite3VdbeMemHandleBom(Mem *pMem);

#ifndef SQLITE_OMIT_INCRBLOB
  int sqlite3VdbeMemExpandBlob(Mem *);
  #define ExpandBlob(P) (((P)->flags&MEM_Zero)?sqlite3VdbeMemExpandBlob(P):0)
#else
  #define sqlite3VdbeMemExpandBlob(x) SQLITE_OK
  #define ExpandBlob(P) SQLITE_OK
#endif

#endif /* !defined(_VDBEINT_H_) */
