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
<<<<<<< HEAD
** 这个模块包括用于处理SQL语句中的WHERE子句的VDBE代码生成的C语言代码。
** 该模块负责生成用于遍历整个表来寻找有效行的VDBE代码。
** 如果有可用索引的话可以通过索引加快搜索，因为该模块负责选择索引，你也可
** 以认为该模块是“查询优化器”。。。
=======
** This module(模块) contains C code that generates VDBE code used to process
** the WHERE clause of SQL statements.  This module is responsible for(对...负责；是...的原因)
** generating the code that loops through(依次通过) a table looking for applicable(可适用的，可应用的)
** rows.  Indices(索引) are selected and used to speed the search when doing
** so is applicable.  Because this module is responsible for selecting
** indices(index的复数), you might also think of this module as the "query optimizer".
** 这个模块包含生成VDBE编码来执行SQL命令中的WHERE子句。这个模块式是产生一个依次扫描一个表来查找合适的行。
** 当合适的时候，选择索引并用它来加快查询。因为这个模块是用来选择索引，你可以把这个模块当做"查询优化"
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
*/
#include "sqliteInt.h"


/*
<<<<<<< HEAD
** 跟踪输出宏，大概是用于当定义了SQLITE_TEST时查看调试信息之用。
=======
** Trace output macros
** 定义跟踪输出宏,用于测试和调试
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
*/
#if defined(SQLITE_TEST) || defined(SQLITE_DEBUG)
int sqlite3WhereTrace = 0;
#endif
#if defined(SQLITE_TEST) && defined(SQLITE_DEBUG)
# define WHERETRACE(X)  if(sqlite3WhereTrace) sqlite3DebugPrintf X
#else
# define WHERETRACE(X)
#endif

<<<<<<< HEAD
/* 前置引用
=======
/* Forward reference(向前引用)
** 具体结构体在下面
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
*/
typedef struct WhereClause WhereClause;
typedef struct WhereMaskSet WhereMaskSet;
typedef struct WhereOrInfo WhereOrInfo;
typedef struct WhereAndInfo WhereAndInfo;
typedef struct WhereCost WhereCost;

<<<<<<< HEAD
/*  ****************** WhereTerm定义说明 *********************
**
** 查询生成器使用一个该结构体的数组用于分析WHERE语句的子表达式。
** 通常每一个WHERE语句子表达式用AND隔开,有时使用OR操作符隔开。
**
** 所有WhereTerms汇总到一个单一的WhereClause结构体中.  
** 使用以下方式保存:
=======
/*
** The query generator uses an array of instances of this structure to
** help it analyze the subexpressions(子语句) of the WHERE clause.  Each WHERE
** clause subexpression is separated from the others by AND operators,
** usually, or sometimes subexpressions separated by OR.
**
** 查询生成器使用一组这个数据结构的实例来帮助它分析WHERE子句的子表达式。
** 每个WHERE子句子表达式通常是根据AND运算符分隔的，有时也会根据OR分隔。
**
** All WhereTerms are collected into a single WhereClause structure.  
** The following identity holds:
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
**
**        WhereTerm.pWC->a[WhereTerm.idx] == WhereTerm
**
** 如果term如下形式:
**
**              X <op> <expr>
**
<<<<<<< HEAD
** 这里X是一个列名并且<op>是某些操作符。
** leftCursor和WhereTerm.u.leftColumn记录了X的指针数。
** WhereTerm.eOperator使用下面定义的WO_xxx和位掩码（Bitmask）记录了<op>
** 操作符的位掩码编码允许我们使用快速搜索匹配几种不同的操作符。
**
** 一个WhereTerm也可能是两个或多个子项通过OR连接，如下:
**
**         (t1.X <op> <expr>) OR (t1.Y <op> <expr>) OR ....
**
** 在第二种情况下, wtFlag等于TERM_ORINFO并且eOperator==WO_OR，
** WhereTerm.u.pOrInfo字段指针指向与收集有关的信息。
**
** 如果一个WHERE语句中的term不匹配前面两种情形，则eOperator==0。
** WhereTerm.pExpr字段对于原有子表达式内容和wtFlags还是恰当的，
** 但是WhereTerm对象中的其他字段是无效的。
**
** 当eOperator!=0, prereqRight和prereqAll记录了光标号,但是是间接的。
** 单个WhereMaskSet结构体转换光标号为比特和翻译的比特存储在prereq字段。
** The translation is used in order to maximize the number of
** bits that will fit in a Bitmask.  The VDBE cursor numbers might be
** spread out over the non-negative integers.  For example, the cursor
=======
** where X is a column name and <op> is one of certain operators,
** then WhereTerm.leftCursor(左游标) and WhereTerm.u.leftColumn record the
** cursor number and column number for X.  WhereTerm.eOperator records
** the <op> using a bitmask(位掩码) encoding defined by WO_xxx below.  The
** use of a bitmask encoding for the operator allows us to search
** quickly for terms that match any of several different operators.
**
** 所有的WhereTerms被搜集到一个单独的WhereClause数据结构。
** 下面恒等式:
** 		WhereTerm.pWC->a[WhereTerm.idx] == WhereTerm
** 当一个term是下面这种形式:
** 		X <op> <expr>
** X是列名，<op>是一个正确的运算符，WhereTerm.leftCursorand WhereTerm.u.leftColumn记录X的游标数和列数。
** WhereTerm.eOperator使用由下面的WO_xxx定义的位掩码记录<op>。
** 运算符使用位掩码使得我们能快速在terms中查找匹配任何不同的运算符
**
** A WhereTerm might also be two or more subterms connected by OR:
**
**         (t1.X <op> <expr>) OR (t1.Y <op> <expr>) OR ....
**
** In this second case, wtFlag as the TERM_ORINFO set and eOperator==WO_OR
** and the WhereTerm.u.pOrInfo field points to auxiliary information(辅助信息) that
** is collected about the OR clause.
**
** 一个WhereTerm也可能有由OR连接的2个或多个subterms
**         (t1.X <op> <expr>) OR (t1.Y <op> <expr>) OR ....
** 在这种情况下，wtFlag设为TERM_ORINFO，eOperator==WO_OR并且WhereTerm.u.pOrInfo指向收集的关于OR子句的辅助信息。
**
** If a term in the WHERE clause does not match either of the two previous
** categories(前两类), then eOperator==0.  The WhereTerm.pExpr field is still set
** to the original subexpression content and wtFlags is set up(建立) appropriately(适当的)
** but no other fields in the WhereTerm object are meaningful.
**
** 如果在WHERE子句中的一个term与前面两类都不相同，那么eOperator==0.WhereTerm.pExpr仍旧设为原始子表达式的内容，
** 并且wtFlags设置为适当的值，但是,whereTerm对象中的其他字段都是无意义的。
**
** When eOperator!=0, prereqRight and prereqAll record sets of cursor numbers,
** but they do so indirectly(间接地).  
** A single WhereMaskSet structure translates
** cursor number into bits and the translated bit is stored in the prereq
** fields(域、字段).  The translation is used in order to(为了) maximize the number of
** bits that will fit in(适应、装配好) a Bitmask(位掩码).  The VDBE cursor numbers might be
** spread out(分为) over the non-negative(非负的) integers.  For example, the cursor
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
** numbers might be 3, 8, 9, 10, 20, 23, 41, and 45.  The WhereMaskSet
** translates these sparse(稀疏的，稀少的) cursor numbers into consecutive integers(连续整型)
** beginning with 0 in order to make the best possible use of the available
** bits in the Bitmask.  So, in the example above, the cursor numbers
<<<<<<< HEAD
** would be mapped into integers 0 through 7.
**
** Term的一个连接的数量受prereqRight和prereqAll比特数的限制。
** 默认是64位, 因此SQLite仅能够处理64个以下的表的连接。
*/
typedef struct WhereTerm WhereTerm;
struct WhereTerm {
  Expr *pExpr;            /* 指向这个term的子表达式的指针 */
  int iParent;            /* Disable pWC->a[iParent] when this term disabled */
  int leftCursor;         /* "X <op> <expr>"中X的数量 */
  union {
    int leftColumn;         /* "X <op> <expr>"中X的数量 */
    WhereOrInfo *pOrInfo;   /* 当eOperator==WO_OR时的额外信息 */
    WhereAndInfo *pAndInfo; /* 当eOperator==WO_AND时的额外信息 */
  } u;
  u16 eOperator;          /* WO_xx宏的值，用于描述<op> */
  u8 wtFlags;             /* TERM_xxx标志.  见下面定义 */
  u8 nChild;              /* Number of children that must disable us */
  WhereClause *pWC;       /* The clause this term is part of */
  Bitmask prereqRight;    /* Bitmask of tables used by pExpr->pRight */
  Bitmask prereqAll;      /* Bitmask of tables referenced by pExpr */
};

/*
** WhereTerm.wtFlags的可用值
*/
#define TERM_DYNAMIC    0x01   /* 需要调用 sqlite3ExprDelete(db, pExpr) */
#define TERM_VIRTUAL    0x02   /* 优化器添加的.  Do not code */
#define TERM_CODED      0x04   /* 这个term已经生成代码 */
#define TERM_COPIED     0x08   /* Has a child */
#define TERM_ORINFO     0x10   /* Need to free the WhereTerm.u.pOrInfo object */
#define TERM_ANDINFO    0x20   /* Need to free the WhereTerm.u.pAndInfo obj */
#define TERM_OR_OK      0x40   /* Used during OR-clause processing */
=======
** would be mapped into(映入) integers 0 through 7.
**
** 当eOperator != 0,prereqRight和prereqAll间接地记录游标数集。
** 一个单独的WhereMaskSet结构体把游标数转化为bits并且转化后的bit存储在prereq域中。
** 使用转化是为了最大化bits的位掩码。VDBE游标数应该是非负的整数。
** 例如，游标数可能是3,8,9,10,20,23,41,45.
** WhereMaskSet把这些稀疏的游标数转换为从0开始的连续整型，是为了最好的利用在Bitmask中的bits。
** 所以上面的例子中，游标数被映射为从0-7的整数。
**
** The number of terms(项数) in a join is limited by(受限于) the number of bits
** in prereqRight and prereqAll.  The default is 64 bits, hence(因此) SQLite
** is only able to process(处理) joins with 64 or fewer tables.
** 在一个连接中的terms数目是受prereqRight and prereqAll中的bits数限制的。
** 默认的是64bits，因此SQLite只能处理64个或更少的表连接。
*/
typedef struct WhereTerm WhereTerm;
struct WhereTerm {
  Expr *pExpr;            /* Pointer to the subexpression(子表达式) that is this term 指向这个term的子表达式 */
  int iParent;            /* Disable pWC->a[iParent] when this term disabled 当这个term销毁时禁用pWC->a[iParent] */
  int leftCursor;         /* Cursor number of X in "X <op> <expr>" 在"X <op> <expr>"中的X的游标数 */
  union {
    int leftColumn;         /* Column number of X in "X <op> <expr>" 在"X <op> <expr>"中的X的列数 */
    WhereOrInfo *pOrInfo;   /* Extra information if eOperator==WO_OR 如果eOperator==WO_OR时的额外信息 */
    WhereAndInfo *pAndInfo; /* Extra information if eOperator==WO_AND 如果eOperator==WO_AND时的额外信息 */
  } u;
  u16 eOperator;          /* A WO_xx value describing(描述) <op> 一个WO_xx值的描述 */
  u8 wtFlags;             /* TERM_xxx bit flags.  See below   TERM_xxx bit标志，下面的TERM_xxx定义了具体值 */
  u8 nChild;              /* Number of children that must disable us 我们必须禁用的孩子数 */
  WhereClause *pWC;       /* The clause this term is part of(一部分)  这个term是哪个子句的一部分  */
  Bitmask prereqRight;    /* Bitmask of tables used by pExpr->pRight (pExpr->pRight使用的表位掩码) */
  Bitmask prereqAll;      /* Bitmask of tables referenced by pExpr(由pExpr引用的表位掩码) */
};

/*
** Allowed values of WhereTerm.wtFlags(wtFlags允许的值)
*/
#define TERM_DYNAMIC    0x01   /* Need to call sqlite3ExprDelete(db, pExpr)  需要调用sqlite3ExprDelete(db, pExpr) */
#define TERM_VIRTUAL    0x02   /* Added by the optimizer.  Do not code (由优化程序添加，不需要编码)*/
#define TERM_CODED      0x04   /* This term(项) is already coded 这个term已经被编码了 */
#define TERM_COPIED     0x08   /* Has a child 有一个子term */
#define TERM_ORINFO     0x10   /* Need to free the WhereTerm.u.pOrInfo object 需要释放WhereTerm.u.pOrInfo对象 */
#define TERM_ANDINFO    0x20   /* Need to free the WhereTerm.u.pAndInfo obj 需要释放WhereTerm.u.pAndInfo对象 */
#define TERM_OR_OK      0x40   /* Used during OR-clause processing 当OR子句执行时使用 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
#ifdef SQLITE_ENABLE_STAT3
#  define TERM_VNULL    0x80   /* Manufactured x>NULL or x<=NULL term (生产x>NULL or x<=NULL) */
#else
#  define TERM_VNULL    0x00   /* Disabled if not using stat3 (如果不用stat3则被禁用) */
#endif

/* ****************** WhereClause定义说明 *********************
** 该结构体的一个实例用于保存整个WHERE语法树的信息。
** 大部分时候它是一个或多个WhereTerms的容器
**
<<<<<<< HEAD
** pOuter说明:  对于如下形式的WHERE语法树
=======
** 下面的数据结构的实例包含一个WHERE子句的所有信息。通常它包含一个或多个WhereTerms
**
** Explanation of pOuter:  For a WHERE clause of the form
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
**
**           a AND ((b AND c) OR (d AND e)) AND f
**
** There are separate WhereClause objects for the whole clause and for
<<<<<<< HEAD
** the subclauses "(b AND c)" and "(d AND e)".
** 这里有很多独立的WhereClause对象组成整个Where语句和类似"(b AND c)"以及"(d AND e)"这样的子语句。
** pOuter字段指针指向整个Where语句的WhereClause对象的子语句。
*/
struct WhereClause {
  Parse *pParse;           /* 语法分析器上下文 */
  WhereMaskSet *pMaskSet;  /* 表游标数到位掩码的映射 Mapping of table cursor numbers to bitmasks */
  Bitmask vmask;           /* Bitmask identifying virtual table cursors */
  WhereClause *pOuter;     /* Outer conjunction */
  u8 op;                   /* Split operator.  TK_AND or TK_OR */
  u16 wctrlFlags;          /* Might include WHERE_AND_ONLY */
  int nTerm;               /* term的数量 */
  int nSlot;               /* Number of entries in a[] */
  WhereTerm *a;            /* 每一个 a[] 表示WHERE语句中的term */
=======
** the subclauses "(b AND c)" and "(d AND e)".  
** The pOuter field of the subclauses points to the WhereClause object for the whole clause.
**
** 解释一个pOuter:对于如下结构的WHERE子句:
** 			a AND ((b AND c) OR (d AND e)) AND f
** 对于整个子句和子句 "(b AND c)" and "(d AND e)"，有许多分开的WhereClause对象
** 子句的pOuter域指向整个子句的WhereClause对象
**
*/
struct WhereClause {
  Parse *pParse;           /* The parser context(解析器上下文) */
  WhereMaskSet *pMaskSet;  /* Mapping of table cursor numbers to bitmasks 表的游标数和位掩码之间的映射 */
  Bitmask vmask;           /* Bitmask identifying(识别) virtual table cursors 识别虚表游标的位掩码 */
  WhereClause *pOuter;     /* Outer conjunction(外连接) */
  u8 op;                   /* Split operator.  TK_AND or TK_OR 由什么运算符分离 TK_AND or TK_OR  */
  u16 wctrlFlags;          /* Might include WHERE_AND_ONLY 可能包含WHERE_AND_ONLY */
  int nTerm;               /* Number of terms terms的数目 */
  int nSlot;               /* Number of entries(条目数) in a[](153行) 在一个WhereTerm中的记录数  */
  WhereTerm *a;            /* Each a[] describes(描述) a term of the WHERE cluase 每个a[]描述WHERE子句中的一个term */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
#if defined(SQLITE_SMALL_STACK)
  WhereTerm aStatic[1];    /* Initial static space for a[] 给a[]的初始静态空间 */
#else
<<<<<<< HEAD
  WhereTerm aStatic[8];    /* 初始化a[]静态空间 */
=======
  WhereTerm aStatic[8];    /* Initial static space for a[] 给a[]的初始静态空间 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
#endif
};

/* ****************** WhereOrInfo 定义说明 *********************
** A WhereTerm with eOperator==WO_OR has its u.pOrInfo pointer set to
** a dynamically allocated instance of the following structure.
<<<<<<< HEAD
** 若WhereTerm的eOperator字段等于WO_OR，则该WhereTerm里u.pOrInfo指针指向
** 该结构体的一个动态分配的实例。
*/
struct WhereOrInfo {
  WhereClause wc;          /* 子term的分解 Decomposition into subterms */
  Bitmask indexable;       /* Bitmask of all indexable tables in the clause */
=======
** 一个eOperator==WO_OR的WhereTerm会让它的u.pOrInfo指针设置为一个下面数据结构动态分配的实例
*/
struct WhereOrInfo {
  WhereClause wc;          /* Decomposition(分解) into subterms 分解为subterms */
  Bitmask indexable;       /* Bitmask of all indexable tables in the clause 在子句中的所有可加索引的表的位掩码 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
};

/* ****************** WhereAndInfo 定义说明 *********************
** A WhereTerm with eOperator==WO_AND has its u.pAndInfo pointer set to
** a dynamically allocated instance of the following structure.
<<<<<<< HEAD
** 若WhereTerm的eOperator字段等于WO_AND，则该WhereTerm里u.pAndInfo指针指向
** 该结构体的一个动态分配的实例。
=======
** 一个eOperator==WO_AND的WhereTerm会让它的u.pAndInfo指针设置为一个下面数据结构动态分配的实例
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
*/
struct WhereAndInfo {
  WhereClause wc;          /* The subexpression broken out(子表达式分解) */
};

<<<<<<< HEAD
/* ****************** WhereMaskSet 定义说明 *********************
** An instance of the following structure keeps track of a mapping
=======
/*
** An instance of the following structure keeps track of(记录) a mapping
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
** between VDBE cursor numbers and bits of the bitmasks in WhereTerm.
** 该结构体实例用于跟踪WhereTerm中 VDBE 光标数量以及位掩码的映射
**
** 下面的数据结构的实例记录VDBE游标数和在WhereTerm中的位掩码nits之间的映射
**
** The VDBE cursor numbers are small integers contained in 
<<<<<<< HEAD
** VDBE 光标数量为small integer类型包含在 SrcList_item.iCursor 或 Expr.iTable 字段中。
** 对于任何给定的WHERE子句，光标数据可能不是从0开始，他们可能包含空白的编号的顺序。
** But we want to make maximum use of the bits in our bitmasks.  This structure provides a mapping
** from the sparse cursor numbers into consecutive integers beginning with 0.
** 但是我们想让我们的掩码位最大的使用。这种结构提供一个映射的稀疏光标的数字从0开始连续的整数。
=======
** SrcList_item.iCursor and Expr.iTable fields.  For any given WHERE 
** clause, the cursor numbers might not begin with 0 and they might
** contain gaps in the numbering sequence.  But we want to make maximum
** use of the bits in our bitmasks.  This structure provides a mapping
** from the sparse(稀少的) cursor numbers into consecutive integers(连续整数) beginning
** with 0.
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
**
** 包含在SrcList_item.iCursor and Expr.iTable域的VDBE游标数是small整型。
** 对于任何给定的WHERE子句，游标数可能不是有0开始的并且可能在数列中有空白。
** 但我们希望最大化的使用位掩码中的bits。这个数据结构提供了一个从稀疏游标数到从0开始的连续整数之间的映射
**
** If WhereMaskSet.ix[A]==B it means that The A-th bit of a Bitmask
** corresponds(符合) VDBE cursor number B.  The A-th bit of a bitmask is 1<<A.
**
** 如果WhereMaskSet.ix[A]==B，它意味着位掩码的A-th bit与VDBE游标数B相对应。
** 位掩码A-th bit是1<<A
**
** For example, if the WHERE clause expression used these VDBE
** cursors:  4, 5, 8, 29, 57, 73.  Then the  WhereMaskSet structure
** would map those cursor numbers into bits 0 through 5.
**
<<<<<<< HEAD
** 映射是无顺序的
=======
** 例如，如果WHERE子句表达式使用这些VDBE游标: 4, 5, 8, 29, 57, 73.
** 那么WhereMaskSet会把这些游标数映射为0到5bits
**
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
** Note that the mapping is not necessarily ordered.  In the example
** above, the mapping might go like this:  4->3, 5->1, 8->2, 29->0,
** 57->5, 73->4.  Or one of 719 other combinations(组合) might be used. It
** does not really matter.  What is important is that sparse cursor
** numbers all get mapped into bit numbers that begin with 0 and contain
** no gaps.
**
** 注意:映射不需要有序的。在上面的例子中映射可能是这样的:4->3, 5->1, 8->2, 29->0, 57->5, 73->4.
** 或着是其他719种组合中的一种。这并不很重要。
** 重要的是稀疏游标数能全部映射到从0开始的bit数，并且不能包含空白.
** 
*/
struct WhereMaskSet {
  int n;                        /* Number of assigned(已分配的) cursor values 已分配游标值的数目 */
  int ix[BMS];                  /* Cursor assigned to each bit 游标被分配的bit */
};

/*
<<<<<<< HEAD
** A WhereCost object records a lookup strategy and the estimated
** cost of pursuing that strategy.
** WhereCost 对象记录一个查询策略以及预估的进行该策略的成本。
*/
struct WhereCost {
  WherePlan plan;    /* 策略计划 */
  double rCost;      /* 该查询计划的总成本 */
  Bitmask used;      /* Bitmask of cursors used by this plan */
=======
** A WhereCost object records a lookup strategy(查找策略) and the estimated
** cost of pursuing(从事) that strategy.
**
** 一个WhereCost对象记录一个查找侧率以及执行这个策略的代价评估
**
*/
struct WhereCost {
  WherePlan plan;    /* The lookup strategy 查找策略 */
  double rCost;      /* Overall cost(总成本) of pursuing this search strategy 执行这个策略的总成本 */
  Bitmask used;      /* Bitmask of cursors used by this plan 这个策略使用的游标的位掩码 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
};

/*
** Bitmasks for the operators that indices(index复数，目录) are able to exploit(开发).  An
** OR-ed combination of(...的组合) these values can be used when searching for
** terms in the where clause.(OR-ed这些值的组合可以在搜索where子句的项时使用)
** 运算符可以利用索引的位掩码。这些值的OR-ed组合可以在where子句中查找terms时被使用.
*/
<<<<<<< HEAD
#define WO_IN     0x001
#define WO_EQ     0x002
#define WO_LT     (WO_EQ<<(TK_LT-TK_EQ))
#define WO_LE     (WO_EQ<<(TK_LE-TK_EQ))
#define WO_GT     (WO_EQ<<(TK_GT-TK_EQ))
#define WO_GE     (WO_EQ<<(TK_GE-TK_EQ))
#define WO_MATCH  0x040
#define WO_ISNULL 0x080
#define WO_OR     0x100       /* 两个或多个OR相连的项 Two or more OR-connected terms */
#define WO_AND    0x200       /* 两个或多个AND相连的项 Two or more AND-connected terms */
#define WO_NOOP   0x800       /* This term does not restrict search space */

#define WO_ALL    0xfff       /* Mask of all possible WO_* values */
#define WO_SINGLE 0x0ff       /* Mask of all non-compound WO_* values */
=======
#define WO_IN     0x001	//16进制0000 0000 0001
#define WO_EQ     0x002	//16进制0000 0000 0010
#define WO_LT     (WO_EQ<<(TK_LT-TK_EQ))	//(WO_EQ 左移 TK_LT-TK_EQ)
#define WO_LE     (WO_EQ<<(TK_LE-TK_EQ))	//(WO_EQ 左移 TK_LE-TK_EQ)
#define WO_GT     (WO_EQ<<(TK_GT-TK_EQ))	//(WO_EQ 左移 TK_GT-TK_EQ)
#define WO_GE     (WO_EQ<<(TK_GE-TK_EQ))	//(WO_EQ 左移 TK_GE-TK_EQ)
#define WO_MATCH  0x040	//16进制0000 0100 0000
#define WO_ISNULL 0x080	//16进制0000 1000 0000
#define WO_OR     0x100 //16进制0001 0000 0000      /* Two or more OR-connected terms 两个或多个OR-connected terms */
#define WO_AND    0x200 //16进制0010 0000 0000      /* Two or more AND-connected terms 两个或多个AND-connected terms */
#define WO_NOOP   0x800 //16进制1000 0000 0000      /* This term does not restrict(限制) search space 这个term不限制搜索空间 */

#define WO_ALL    0xfff //16进制1111 1111 1111      /* Mask of all possible WO_* values 所有可能的WO_*取值的掩码 */
#define WO_SINGLE 0x0ff //16进制0000 1111 1111      /* Mask of all non-compound(不混合的) WO_* values 所有不混合的WO_*取值的掩码 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949

/*
** Value for wsFlags returned by bestIndex() and stored in
** WhereLevel.wsFlags.  These flags determine which search
** strategies are appropriate.
<<<<<<< HEAD
** bestIndex()函数返回的存储在WhereLevel.wsFlags中的值，这个标志表示哪个查询策略更合适
**
** The least significant 12 bits is reserved as a mask for WO_ values above.
** WhereLevel.wsFlags 字段的值通常是 WO_IN|WO_EQ|WO_ISNULL.
** 但如果表是左连接的右表, WhereLevel.wsFlags 设置为 WO_IN|WO_EQ。
** The WhereLevel.wsFlags field can then be used as
** the "op" parameter to findTerm when we are resolving equality constraints.
** ISNULL constraints will then not be used on the right table of a left
** join.  Tickets #2177 and #2189.
=======
** 
** wsFlags的值由bestIndex()返回，并且存储在WhereLevel.wsFlags中。
** 这些标志决哪些查询策略是合适的。
**
** The least significant(最低有效的) 12 bits is reserved as(被保留的) a mask for WO_ values above(上文).
** The WhereLevel.wsFlags field is usually set to WO_IN|WO_EQ|WO_ISNULL.
** But if the table is the right table of a left join, WhereLevel.wsFlags
** is set to WO_IN|WO_EQ. 
**
** 低位的12bits被保留为上文中的WO_ 值的掩码。
** WhereLevel.wsFlags域经常设置为WO_IN|WO_EQ|WO_ISNULL.
** 但是如果表示左联接的右表WhereLevel.wsFlags设置为WO_IN|WO_EQ.
**
** The WhereLevel.wsFlags field can then be used as
** the "op" parameter(参数) to findTerm when we are resolving(分析) equality constraints(等式约束).
** ISNULL constraints(约束) will then not be used on the right table of a left
** join.
**
** 当我们分析等式约束时，WhereLevel.wsFlags域能被当做"op"参数来findTerm
** 在有左联接的右表上不会使用ISNULL约束
** Tickets #2177 and #2189.
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
*/
#define WHERE_ROWID_EQ     0x00001000  /* rowid=EXPR or rowid IN (...)  rowid=EXPR或rowid IN(...) */
#define WHERE_ROWID_RANGE  0x00002000  /* rowid<EXPR and/or rowid>EXPR rowid<EXPR 且/或 rowid>EXPR */
#define WHERE_COLUMN_EQ    0x00010000  /* x=EXPR or x IN (...) or x IS NULL  x=EXPR 或 x IN (...) 或 x IS NULL  */
#define WHERE_COLUMN_RANGE 0x00020000  /* x<EXPR and/or x>EXPR */
#define WHERE_COLUMN_IN    0x00040000  /* x IN (...) */
#define WHERE_COLUMN_NULL  0x00080000  /* x IS NULL */
<<<<<<< HEAD
#define WHERE_INDEXED      0x000f0000  /* Anything that uses an index */
#define WHERE_NOT_FULLSCAN 0x100f3000  /* 不做全表扫描 */
#define WHERE_IN_ABLE      0x000f1000  /* Able to support an IN operator */
#define WHERE_TOP_LIMIT    0x00100000  /* x<EXPR or x<=EXPR constraint */
#define WHERE_BTM_LIMIT    0x00200000  /* x>EXPR or x>=EXPR constraint */
#define WHERE_BOTH_LIMIT   0x00300000  /* Both x>EXPR and x<EXPR */
#define WHERE_IDX_ONLY     0x00800000  /* Use index only - omit table */
#define WHERE_ORDERBY      0x01000000  /* Output will appear in correct order */
#define WHERE_REVERSE      0x02000000  /* Scan in reverse order */
#define WHERE_UNIQUE       0x04000000  /* Selects no more than one row */
#define WHERE_VIRTUALTABLE 0x08000000  /* Use virtual-table processing */
#define WHERE_MULTI_OR     0x10000000  /* OR using multiple indices */
#define WHERE_TEMP_INDEX   0x20000000  /* Uses an ephemeral index */
#define WHERE_DISTINCT     0x40000000  /* Correct order for DISTINCT */

/*
** 初始化一个预先分配的WhereClause结构
*/
static void whereClauseInit(
  WhereClause *pWC,        /* 将要初始化的WhereClause */
  Parse *pParse,           /* 语法分析器上下文 */
  WhereMaskSet *pMaskSet,  /* Mapping from table cursor numbers to bitmasks */
  u16 wctrlFlags           /* Might include WHERE_AND_ONLY */
){
  pWC->pParse = pParse;    /* 初始化pWC的pParse */
  pWC->pMaskSet = pMaskSet;  /* 初始化pWC的pMaskSet */
  pWC->pOuter = 0;
  pWC->nTerm = 0;
  pWC->nSlot = ArraySize(pWC->aStatic);
  pWC->a = pWC->aStatic;
  pWC->vmask = 0;
  pWC->wctrlFlags = wctrlFlags;
}

/* 前置引用 */
static void whereClauseClear(WhereClause*);

/*
** 释放一个 WhereOrInfo 对象相关联的存储空间
=======
#define WHERE_INDEXED      0x000f0000  /* Anything that uses an index(任何使用索引) */
#define WHERE_NOT_FULLSCAN 0x100f3000  /* Does not do a full table scan(不需要全表扫描) */
#define WHERE_IN_ABLE      0x000f1000  /* Able to support an IN operator(能支持in操作) */
#define WHERE_TOP_LIMIT    0x00100000  /* x<EXPR or x<=EXPR constraint(约束) x<EXPR or x<=EXPR约束 */
#define WHERE_BTM_LIMIT    0x00200000  /* x>EXPR or x>=EXPR constraint x>EXPR or x>=EXPR约束 */
#define WHERE_BOTH_LIMIT   0x00300000  /* Both x>EXPR and x<EXPR x>EXPR and x<EXPR */
#define WHERE_IDX_ONLY     0x00800000  /* Use index only - omit table(只用索引，省略表) */
#define WHERE_ORDERBY      0x01000000  /* Output will appear in correct order 恰当的顺序输出 */
#define WHERE_REVERSE      0x02000000  /* Scan in reverse order(倒序) 倒序扫描 */
#define WHERE_UNIQUE       0x04000000  /* Selects no more than(只是，仅仅) one row Selects只是一行 */
#define WHERE_VIRTUALTABLE 0x08000000  /* Use virtual-table processing(处理) 使用虚拟表处理 */
#define WHERE_MULTI_OR     0x10000000  /* OR using multiple indices(OR使用多重索引) */
#define WHERE_TEMP_INDEX   0x20000000  /* Uses an ephemeral index(使用临时索引) */
#define WHERE_DISTINCT     0x40000000  /* Correct order for DISTINCT DISTINCT的正确的顺序 */

/*
** Initialize a preallocated(预分配) WhereClause structure.
** 初始化一个预分配的WhereClause数据结构
*/
static void whereClauseInit(
  WhereClause *pWC,        /* The WhereClause to be initialized WhereClause被初始化 */
  Parse *pParse,           /* The parsing context(解析器上下文) */
  WhereMaskSet *pMaskSet,  /* Mapping from table cursor numbers to bitmasks(表游标数映射到位掩码) */
  u16 wctrlFlags           /* Might include WHERE_AND_ONLY 可能包含WHERE_AND_ONLY */
){
  pWC->pParse = pParse;	//初始化解析器上下文
  pWC->pMaskSet = pMaskSet;	//初始化pMaskSet
  pWC->pOuter = 0;	//外连接为空
  pWC->nTerm = 0;	//项数为0
  pWC->nSlot = ArraySize(pWC->aStatic);	//在a[]中的条目数初始化a[]静态空间的元素个数。ArraySize返回数组的元素个数。
  pWC->a = pWC->aStatic;	//初始化where子句中的WhereTerm数据结构
  pWC->vmask = 0;	//初始化虚表游标的位掩码
  pWC->wctrlFlags = wctrlFlags;		//初始化wctrlFlags
}

/* Forward reference(向前引用) 具体函数在下面 */
static void whereClauseClear(WhereClause*);

/*
** Deallocate(解除分配) all memory associated with a WhereOrInfo object.
** 解除WhereOrInfo对象的所有内存分配
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
*/
static void whereOrInfoDelete(sqlite3 *db, WhereOrInfo *p){
  whereClauseClear(&p->wc);	//解除分配
  sqlite3DbFree(db, p);	//释放可能被关联到一个特定数据库连接的内存
}

/*
<<<<<<< HEAD
** 释放一个 WhereAndInfo 对象WhereAndInfo相关联的存储空间
=======
** Deallocate all memory associated with a WhereAndInfo object.
** 解除WhereAndInfo对象的所有内存分配
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
*/
static void whereAndInfoDelete(sqlite3 *db, WhereAndInfo *p){
  whereClauseClear(&p->wc);	//解除分配
  sqlite3DbFree(db, p);	//释放可能被关联到一个特定数据库连接的内存
}

/*
<<<<<<< HEAD
** 释放一个 WhereClause 结构体.  WhereClause 结构体自身没有释放。
** 这是一个 whereClauseInit() 的逆向操作。
=======
** Deallocate a WhereClause structure.  The WhereClause structure
** itself is not freed.  This routine is the inverse of whereClauseInit().
** 解除一个WhereClause数据结构的分配。WhereClause本身不被释放。这个程序与whereClauseInit()相反
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
*/
static void whereClauseClear(WhereClause *pWC){
  int i;
  WhereTerm *a;
  sqlite3 *db = pWC->pParse->db;	//数据库连接的实例设为where子句所在的数据库
  for(i=pWC->nTerm-1, a=pWC->a; i>=0; i--, a++){	//循环遍历where子句的每个term项
    if( a->wtFlags & TERM_DYNAMIC ){	//如果当前的term是动态的
      sqlite3ExprDelete(db, a->pExpr);	//递归删除term中的表达式树
    }
    if( a->wtFlags & TERM_ORINFO ){		//如果当前的term存储的是OR子句的信息
      whereOrInfoDelete(db, a->u.pOrInfo);	//解除当前term中的pOrInfo对象的所有内存分配
    }else if( a->wtFlags & TERM_ANDINFO ){		//如果当前的term存储的是AND子句的信息
      whereAndInfoDelete(db, a->u.pAndInfo);	//解除当前term中的pAndInfo对象的所有内存分配
    }
  }
  if( pWC->a!=pWC->aStatic ){	//如果where子句的term项不是初始静态空间
    sqlite3DbFree(db, pWC->a);	//释放可能被关联到一个特定数据库连接的内存
  }
}

/*
** 添加单个新 WhereTerm 对象到 WhereClause 对象pWC中。
** The new WhereTerm object is constructed from Expr p and with wtFlags.
** 新的 WhereTerm 对象由Expr p和wtFlags构造而成。
** The index in pWC->a[] of the new WhereTerm is returned on success.
** 新 WhereTerm 对象在 pWC->a[] 数组中的索引值会在加入成功后由函数返回。
** 0 is returned if the new WhereTerm could not be added due to a memory
** allocation error.  The memory allocation failure will be recorded in
** the db->mallocFailed flag so that higher-level functions can detect it.
** 返回0表示新 WhereTerm 由于空间分配错误无法添加。
** 内存分配失败将记录在 DB—> mallocfailed 标志中使更高级别的函数可以检测到它。
**
** 增加一个单独的新的WhereTerm到WhereClause对象。新的WhereTerm对象是根据Expr和wtFlags构建的。
** 成功返回新WhereTerm中的索引。
** 如果由于内存分配错误，新的WhereTerm不能添加到WhereClause中，则返回0.
** 内存分配失败会被记录在db->mallocFailed标志中，为了让高级的函数检测到它。
**
** This routine will increase the size of the pWC->a[] array as necessary.
** 通常这会增加 pWC->a[] 数组的大小。
**
** 如果需要的话，程序会增加pWC->a[]的大小。
**
** If the wtFlags argument includes TERM_DYNAMIC, then responsibility
** for freeing the expression p is assumed by the WhereClause object pWC.
** This is true even if this routine fails to allocate a new WhereTerm.
**
** 如果wtFlags包含TERM_DYNAMIC，那么WhereClause对象会承担释放表达式p的责任。
** 尽管这个程序没能成功分配一个新的WhereTerm，这也是正确的。
**
** WARNING:  This routine might reallocate the space used to store
** WhereTerms.  All pointers to WhereTerms should be invalidated after
** calling this routine.  Such pointers may be reinitialized by referencing
** the pWC->a[] array.
**
** 警告:这个程序可能分配用于存储WhereTerms的空间。所有指向WhereTerms的指针在调用这个程序后都需要设定失效。
** 一些索引可能通过引用pWC->a[]被重新启用
**
*/
static int whereClauseInsert(WhereClause *pWC, Expr *p, u8 wtFlags){
<<<<<<< HEAD
  WhereTerm *pTerm;   /* 新建一个WhereTerm */
=======
  WhereTerm *pTerm; //WHERE子句的term
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  int idx;
  testcase( wtFlags & TERM_VIRTUAL );  /* EV: R-00211-15100 */
  if( pWC->nTerm>=pWC->nSlot ){	//如果WhereClause中的term数大于或等于在一个WhereTerms中的记录数
    WhereTerm *pOld = pWC->a;	//记录当前的term为pOld
    sqlite3 *db = pWC->pParse->db;	//记录当前where子句的数据库连接
    pWC->a = sqlite3DbMallocRaw(db, sizeof(pWC->a[0])*pWC->nSlot*2 );	//分配内存
    if( pWC->a==0 ){	//如果分配内存失败
      if( wtFlags & TERM_DYNAMIC ){	//如果当前term是动态分配的
        sqlite3ExprDelete(db, p);	//递归删除term中的表达式树
      }
      pWC->a = pOld;	//把term设置为以前记录的term
      return 0;	//返回0
    }
    memcpy(pWC->a, pOld, sizeof(pWC->a[0])*pWC->nTerm); //
    if( pOld!=pWC->aStatic ){
      sqlite3DbFree(db, pOld);	//释放可能被关联到一个特定数据库连接的内存
    }
    pWC->nSlot = sqlite3DbMallocSize(db, pWC->a)/sizeof(pWC->a[0]);
  }
  pTerm = &pWC->a[idx = pWC->nTerm++];
  pTerm->pExpr = p;
  pTerm->wtFlags = wtFlags;
  pTerm->pWC = pWC;
  pTerm->iParent = -1;
  return idx;
}

/*
** This routine identifies subexpressions in the WHERE clause where
** each subexpression is separated by the AND operator or some other
** operator specified in the op parameter.  The WhereClause structure
** is filled with pointers to subexpressions.  
**
** 这个程序识别在WHERE子句中的子表达式，这些子表达式是根据AND运算符或在op参数中定义的其他的运算符分隔的。
** WhereClause数据结构充满了指向子表达式的指针。
**
** For example:
**
**    WHERE  a=='hello' AND coalesce(b,11)<10 AND (c+12!=d OR c==22)
**           \________/     \_______________/     \________________/
**            slot[0]            slot[1]               slot[2]
** 
** 分离以AND隔开的WHERE子句
** The original WHERE clause in pExpr is unaltered.  All this routine
** does is make slot[] entries point to substructure within pExpr.
**
** 例如:
**    WHERE  a=='hello' AND coalesce(b,11)<10 AND (c+12!=d OR c==22)
**           \________/     \_______________/     \________________/
**            slot[0]            slot[1]               slot[2]
**
** 在pExpr中的原始的WHERE子句是不变的。这个程序就是让slot[]指向在pExpr中的substructure
**
** In the previous sentence and in the diagram, "slot[]" refers to
** the WhereClause.a[] array.  The slot[] array grows as needed to contain
** all terms of the WHERE clause.
**
** 在前面的句子和在图表中，"slot[]"引用了WhereClause.a[].slot[]根据需要包含所有的WHERE子句的terms
**
*/
static void whereSplit(WhereClause *pWC, Expr *pExpr, int op){
  pWC->op = (u8)op; //初始化WHERE子句进行分割的运算符
  if( pExpr==0 ) return;
<<<<<<< HEAD
  if( pExpr->op!=op ){
    whereClauseInsert(pWC, pExpr, 0);  /* 子句不为AND分开的子句，插入新WhereTerm到WhereClause中 */
=======
  if( pExpr->op!=op ){ //如果表达式的操作符不是指定的运算符
    whereClauseInsert(pWC, pExpr, 0); //向pWC子句中插入一个WhereTerm
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  }else{
    whereSplit(pWC, pExpr->pLeft, op);	//递归分隔WHERE子句的左表达式
    whereSplit(pWC, pExpr->pRight, op);	//递归分隔WHERE子句的右表达式
  }
}

/*
** Initialize an expression mask set (a WhereMaskSet object)
<<<<<<< HEAD
** 初始化表达式掩码
=======
**
** 初始化一个表达式掩码设置(一个WhereMaskSet对象)
**
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
*/
#define initMaskSet(P)  memset(P, 0, sizeof(*P))

/*
** 根据给出的光标值返回位掩码bitmask，不存在则返回0
** Return the bitmask for the given cursor number.  Return 0 if
** iCursor is not in the set.、
**
** 返回给出的游标数的位掩码。如果iCursor未设置，则返回0
**
*/
static Bitmask getMask(WhereMaskSet *pMaskSet, int iCursor){
  int i;
  assert( pMaskSet->n<=(int)sizeof(Bitmask)*8 );
  for(i=0; i<pMaskSet->n; i++){
    if( pMaskSet->ix[i]==iCursor ){
      return ((Bitmask)1)<<i;
    }
  }
  return 0;
}

/*
** Create a new mask for cursor iCursor.
** 为ICursor创建一个新的mask
**
** 在sqlite3WhereBegin()中已经限制了FROM语句的表个数
** There is one cursor per table in the FROM clause.  The number of
** tables in the FROM clause is limited by a test early in the
** sqlite3WhereBegin() routine.  So we know that the pMaskSet->ix[]
** array will never overflow.
**
** 为游标iCursor创建一个新的掩码。
** 在FROM子句中每个表有一个游标。在FROM子句中的表的个数是在sqlite3WhereBegin()程序的一个测试中限制了的。
** 所以我们知道pMaskSet->ix[]永远不会溢出。
**
*/
static void createMask(WhereMaskSet *pMaskSet, int iCursor){
  assert( pMaskSet->n < ArraySize(pMaskSet->ix) );
  pMaskSet->ix[pMaskSet->n++] = iCursor;
}

/*
** This routine walks (recursively) an expression tree and generates
** a bitmask indicating which tables are used in that expression
** tree.
** 这个程序（递归）一个表达式树和产生的位掩码指示哪些表中使用的表达式树。
**
** 这个程序(递归地)访问一个表达式树并且生成一个位掩码指示哪些表中使用表达式树。
**
** In order for this routine to work, the calling function must have
** previously invoked sqlite3ResolveExprNames() on the expression.  See
** the header comment on that routine for additional information.
** The sqlite3ResolveExprNames() routines looks for column names and
** sets their opcodes to TK_COLUMN and their Expr.iTable fields to
** the VDBE cursor number of the table.  This routine just has to
** translate the cursor numbers into bitmask values and OR all
** the bitmasks together.
**
** 为了让这个程序工作，调用函数必须预先唤醒表达式中的sqlite3ResolveExprNames()。
** 查看sqlite3ResolveExprNames()程序的头部附加的注释信息。
** sqlite3ResolveExprNames()程序查找列名和把他们的opcodes设置为TK_COLUMN并且把他们的Expr.iTable设为表的VDBE游标数。
** 这个程序只是把游标数转换为位掩码值和OR所有的位掩码
** 
**
*/
static Bitmask exprListTableUsage(WhereMaskSet*, ExprList*);
static Bitmask exprSelectTableUsage(WhereMaskSet*, Select*);
static Bitmask exprTableUsage(WhereMaskSet *pMaskSet, Expr *p){
  Bitmask mask = 0;
  if( p==0 ) return 0;
  if( p->op==TK_COLUMN ){
    mask = getMask(pMaskSet, p->iTable);
    return mask;
  }
  mask = exprTableUsage(pMaskSet, p->pRight);
  mask |= exprTableUsage(pMaskSet, p->pLeft);
  if( ExprHasProperty(p, EP_xIsSelect) ){
    mask |= exprSelectTableUsage(pMaskSet, p->x.pSelect);
  }else{
    mask |= exprListTableUsage(pMaskSet, p->x.pList);
  }
  return mask;
}
static Bitmask exprListTableUsage(WhereMaskSet *pMaskSet, ExprList *pList){
  int i;
  Bitmask mask = 0;
  if( pList ){
    for(i=0; i<pList->nExpr; i++){
      mask |= exprTableUsage(pMaskSet, pList->a[i].pExpr);
    }
  }
  return mask;
}
static Bitmask exprSelectTableUsage(WhereMaskSet *pMaskSet, Select *pS){
  Bitmask mask = 0;
  while( pS ){
    SrcList *pSrc = pS->pSrc;
    mask |= exprListTableUsage(pMaskSet, pS->pEList);
    mask |= exprListTableUsage(pMaskSet, pS->pGroupBy);
    mask |= exprListTableUsage(pMaskSet, pS->pOrderBy);
    mask |= exprTableUsage(pMaskSet, pS->pWhere);
    mask |= exprTableUsage(pMaskSet, pS->pHaving);
    if( ALWAYS(pSrc!=0) ){
      int i;
      for(i=0; i<pSrc->nSrc; i++){
        mask |= exprSelectTableUsage(pMaskSet, pSrc->a[i].pSelect);
        mask |= exprTableUsage(pMaskSet, pSrc->a[i].pOn);
      }
    }
    pS = pS->pPrior;
  }
  return mask;
}

/*
** Return TRUE if the given operator is one of the operators that is
** allowed for an indexable WHERE clause term.  The allowed operators are
** "=", "<", ">", "<=", ">=", and "IN".
** 若操作符使用在一个可可以使用索引的WhereClause term上则返回True
**
** 如果给出的运算符是一个带索引的WHERE子句term中可以使用的运算符，那么就返回TRUE.
** 允许的运算符有"=", "<", ">", "<=", ">=", and "IN"
**
** IMPLEMENTATION-OF: R-59926-26393 To be usable by an index a term must be
** of one of the following forms: column = expression column > expression
** column >= expression column < expression column <= expression
** expression = column expression > column expression >= column
** expression < column expression <= column column IN
** (expression-list) column IN (subquery) column IS NULL
**
** IMPLEMENTATION-OF: R-59926-26393 一个term可用索引必须是一下情况:
** column = expression column > expression
** column >= expression column < expression column <= expression
** expression = column expression > column expression >= column
** expression < column expression <= column column IN
** (expression-list) column IN (subquery) column IS NULL
**
*/
static int allowedOp(int op){
  assert( TK_GT>TK_EQ && TK_GT<TK_GE );
  assert( TK_LT>TK_EQ && TK_LT<TK_GE );
  assert( TK_LE>TK_EQ && TK_LE<TK_GE );
  assert( TK_GE==TK_EQ+4 );
  return op==TK_IN || (op>=TK_EQ && op<=TK_GE) || op==TK_ISNULL;
}

/*
** Swap two objects of type TYPE.
<<<<<<< HEAD
** 转换A，B值
=======
**
** 交换TYPE类型的两个对象
**
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
*/
#define SWAP(TYPE,A,B) {TYPE t=A; A=B; B=t;}

/*
** Commute a comparison operator.  Expressions of the form "X op Y"
** are converted into "Y op X".
** 转换对照操作符。
**
** 计算一个比较表达式."X op Y"表达式转化为"Y op X"
**
** If a collation sequence is associated with either the left or right
** side of the comparison, it remains associated with the same side after
** the commutation. So "Y collate NOCASE op X" becomes 
** "X collate NOCASE op Y". This is because any collation sequence on
** the left hand side of a comparison overrides any collation sequence 
** attached to the right. For the same reason the EP_ExpCollate flag
** is not commuted.
**
** 如果一个对照顺序与比较式的左边或右边相关，交换后它仍与相同边有关。
** 索引"Y核对NOCASE op X"变成了"X核对NOCASE op Y"。这是因为任何在比较式左边的对照顺序重写任何左边的对照序列。
** 由于相同的原因，EP_ExpCollate标志不是交换。
**
*/
static void exprCommute(Parse *pParse, Expr *pExpr){
  u16 expRight = (pExpr->pRight->flags & EP_ExpCollate);
  u16 expLeft = (pExpr->pLeft->flags & EP_ExpCollate);
  assert( allowedOp(pExpr->op) && pExpr->op!=TK_IN );
  pExpr->pRight->pColl = sqlite3ExprCollSeq(pParse, pExpr->pRight);
  pExpr->pLeft->pColl = sqlite3ExprCollSeq(pParse, pExpr->pLeft);
  SWAP(CollSeq*,pExpr->pRight->pColl,pExpr->pLeft->pColl);
  pExpr->pRight->flags = (pExpr->pRight->flags & ~EP_ExpCollate) | expLeft;
  pExpr->pLeft->flags = (pExpr->pLeft->flags & ~EP_ExpCollate) | expRight;
  SWAP(Expr*,pExpr->pRight,pExpr->pLeft);
  if( pExpr->op>=TK_GT ){
    assert( TK_LT==TK_GT+2 );
    assert( TK_GE==TK_LE+2 );
    assert( TK_GT>TK_EQ );
    assert( TK_GT<TK_LE );
    assert( pExpr->op>=TK_GT && pExpr->op<=TK_GE );
    pExpr->op = ((pExpr->op-TK_GT)^2)+TK_GT;
  }
}

/*
** Translate from TK_xx operator to WO_xx bitmask.
<<<<<<< HEAD
** 转换TK_xx操作符到WO_xx位掩码
=======
** 把TK_xx运算符转化为WO_xx位掩码
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
*/
static u16 operatorMask(int op){
  u16 c;
  assert( allowedOp(op) );
  if( op==TK_IN ){
    c = WO_IN;
  }else if( op==TK_ISNULL ){
    c = WO_ISNULL;
  }else{
    assert( (WO_EQ<<(op-TK_EQ)) < 0x7fff );
    c = (u16)(WO_EQ<<(op-TK_EQ));
  }
  assert( op!=TK_ISNULL || c==WO_ISNULL );
  assert( op!=TK_IN || c==WO_IN );
  assert( op!=TK_EQ || c==WO_EQ );
  assert( op!=TK_LT || c==WO_LT );
  assert( op!=TK_LE || c==WO_LE );
  assert( op!=TK_GT || c==WO_GT );
  assert( op!=TK_GE || c==WO_GE );
  return c;
}

/*
** Search for a term in the WHERE clause that is of the form "X <op> <expr>"
** where X is a reference to the iColumn of table iCur and <op> is one of
** the WO_xx operator codes specified by the op parameter.
** Return a pointer to the term.  Return 0 if not found.
<<<<<<< HEAD
** 在WhereClause中搜索通过WO_xx指定操作符，iColumn和iCur指定表和列的WhereTerm
** 搜索成功则返回WhereTerm的指针，否则返回0
**
*/
static WhereTerm *findTerm(
  WhereClause *pWC,     /* 需要搜索的WhereClause The WHERE clause to be searched */
  int iCur,             /* Cursor number of LHS */
  int iColumn,          /* Column number of LHS */
  Bitmask notReady,     /* RHS must not overlap with this mask */
  u32 op,               /* 通过WO_xx描述的操作符 Mask of WO_xx values describing operator */
  Index *pIdx           /* Must be compatible with this index, if not NULL */
=======
**
** 在WHERE子句中查找一个term，这个term由"X <op> <expr>"这种形式组成。
** 其中X是与表iCur的iColumn相关的,<op>是用op参数来说明的WO_xx运算符编码的一种。
** 返回一个指向term的指针。如果没有找到就返回0。
**
*/
static WhereTerm *findTerm(
  WhereClause *pWC,     /* The WHERE clause to be searched 要被查找的WHERE子句 */
  int iCur,             /* Cursor number of LHS LHS(等式的左边)的游标数 */
  int iColumn,          /* Column number of LHS LHS(等式的左边)的列数 */
  Bitmask notReady,     /* RHS must not overlap with this mask (等式的右边不能与掩码重叠) */
  u32 op,               /* Mask of WO_xx values describing operator 描述运算符的WO_xx值的掩码 */
  Index *pIdx           /* Must be compatible with this index, if not NULL 必须与索引相一致，不能为空 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
){
  WhereTerm *pTerm;
  int k;
  assert( iCur>=0 );
  op &= WO_ALL;
  for(; pWC; pWC=pWC->pOuter){
    for(pTerm=pWC->a, k=pWC->nTerm; k; k--, pTerm++){
      if( pTerm->leftCursor==iCur
         && (pTerm->prereqRight & notReady)==0
         && pTerm->u.leftColumn==iColumn
         && (pTerm->eOperator & op)!=0
      ){
        if( iColumn>=0 && pIdx && pTerm->eOperator!=WO_ISNULL ){
          Expr *pX = pTerm->pExpr;
          CollSeq *pColl;
          char idxaff;
          int j;
          Parse *pParse = pWC->pParse;
  
          idxaff = pIdx->pTable->aCol[iColumn].affinity;
          if( !sqlite3IndexAffinityOk(pX, idxaff) ) continue;
  
          /* Figure out the collation sequence required from an index for
          ** it to be useful for optimising expression pX. Store this
          ** value in variable pColl.
          */
          assert(pX->pLeft);
          pColl = sqlite3BinaryCompareCollSeq(pParse, pX->pLeft, pX->pRight);
          assert(pColl || pParse->nErr);
  
          for(j=0; pIdx->aiColumn[j]!=iColumn; j++){
            if( NEVER(j>=pIdx->nColumn) ) return 0;
          }
          if( pColl && sqlite3StrICmp(pColl->zName, pIdx->azColl[j]) ) continue;
        }
        return pTerm;
      }
    }
  }
  return 0;
}

/* Forward reference 向前引用 */
static void exprAnalyze(SrcList*, WhereClause*, int);

/*
** Call exprAnalyze on all terms in a WHERE clause.  
<<<<<<< HEAD
** 对所有WhereClause中的WhereTerm执行exprAnalyze
**
*/
static void exprAnalyzeAll(
  SrcList *pTabList,       /* the FROM clause */
  WhereClause *pWC         /* 待分析的WhereClause the WHERE clause to be analyzed */
=======
**
** 在一个WHERE子句的所有terms上调用exprAnalyze
*/
static void exprAnalyzeAll(
  SrcList *pTabList,       /* the FROM clause FROM子句 */
  WhereClause *pWC         /* the WHERE clause to be analyzed 被分析的WHERE子句 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
){
  int i;
  for(i=pWC->nTerm-1; i>=0; i--){
    exprAnalyze(pTabList, pWC, i);
  }
}

#ifndef SQLITE_OMIT_LIKE_OPTIMIZATION
/*
** Check to see if the given expression is a LIKE or GLOB operator that
** can be optimized using inequality constraints.  Return TRUE if it is
** so and false if not.
** 检查给定的表达式是否是可以使用不等式优化的LIKE或GLOB操作符。
**
** 查看表达式是否是可以使用不等式约束进行优化的LIKE或GLOB运算符。
** 如果它是则返回TRUE，如果不是则返回FALSE.
**
** In order for the operator to be optimizible, the RHS must be a string
** literal that does not begin with a wildcard.  
**
** 为了能够优化运算符，等式的右边必须是一个字符串文字且不能以通配符开头。
**
*/
static int isLikeOrGlob(
<<<<<<< HEAD
  Parse *pParse,    /* 词法分析及代码生成器上下文 Parsing and code generating context */
  Expr *pExpr,      /* 待测试表达式 Test this expression */
  Expr **ppPrefix,  /* Pointer to TK_STRING expression with pattern prefix */
  int *pisComplete, /* True if the only wildcard is % in the last character */
  int *pnoCase      /* True if uppercase is equivalent to lowercase */
=======
  Parse *pParse,    /* Parsing and code generating context 分析并且编码产生上下文 */
  Expr *pExpr,      /* Test this expression 测试这个表达式 */
  Expr **ppPrefix,  /* Pointer to TK_STRING expression with pattern prefix 指向有模式前缀的TK_STRING表达式 */
  int *pisComplete, /* True if the only wildcard is % in the last character 如果只有一个通配符"%"且在最后，则返回True */
  int *pnoCase      /* True if uppercase is equivalent to lowercase 如果不分大小写则返回True */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
){
  const char *z = 0;         /* String on RHS of LIKE operator 在LIKE运算符的右边的字符串 */
  Expr *pRight, *pLeft;      /* Right and left size of LIKE operator LIKE运算符右边和左边的大小 */
  ExprList *pList;           /* List of operands to the LIKE operator LIKE运算符的运算对象 */
  int c;                     /* One character in z[] 在字符串z[]中的一个字符 */
  int cnt;                   /* Number of non-wildcard prefix characters 无通配符前缀的字符的数目 */
  char wc[3];                /* Wildcard characters 通配符 */
  sqlite3 *db = pParse->db;  /* Database connection 数据库连接 */
  sqlite3_value *pVal = 0;
  int op;                    /* Opcode of pRight pRight的Opcode */

  if( !sqlite3IsLikeFunction(db, pExpr, pnoCase, wc) ){
    return 0;
  }
#ifdef SQLITE_EBCDIC
  if( *pnoCase ) return 0;
#endif
  pList = pExpr->x.pList;
  pLeft = pList->a[1].pExpr;
  if( pLeft->op!=TK_COLUMN 
   || sqlite3ExprAffinity(pLeft)!=SQLITE_AFF_TEXT 
   || IsVirtual(pLeft->pTab)
  ){
    /* IMP: R-02065-49465 The left-hand side of the LIKE or GLOB operator must
    ** be the name of an indexed column with TEXT affinity. */
    //IMP: R-02065-49465 LIKE或GLOB运算符的左边必须是一个TEXT亲和性的带索引的列名
    return 0;
  }
  assert( pLeft->iColumn!=(-1) ); /* Because IPK never has AFF_TEXT 因为IPK从没有TEXT亲和性 */

  pRight = pList->a[0].pExpr;
  op = pRight->op;
  if( op==TK_REGISTER ){
    op = pRight->op2;
  }
  if( op==TK_VARIABLE ){
    Vdbe *pReprepare = pParse->pReprepare;
    int iCol = pRight->iColumn;
    pVal = sqlite3VdbeGetValue(pReprepare, iCol, SQLITE_AFF_NONE);
    if( pVal && sqlite3_value_type(pVal)==SQLITE_TEXT ){
      z = (char *)sqlite3_value_text(pVal);
    }
    sqlite3VdbeSetVarmask(pParse->pVdbe, iCol);
    assert( pRight->op==TK_VARIABLE || pRight->op==TK_REGISTER );
  }else if( op==TK_STRING ){
    z = pRight->u.zToken;
  }
  if( z ){
    cnt = 0;
    while( (c=z[cnt])!=0 && c!=wc[0] && c!=wc[1] && c!=wc[2] ){
      cnt++;
    }
    if( cnt!=0 && 255!=(u8)z[cnt-1] ){
      Expr *pPrefix;
      *pisComplete = c==wc[0] && z[cnt+1]==0;
      pPrefix = sqlite3Expr(db, TK_STRING, z);
      if( pPrefix ) pPrefix->u.zToken[cnt] = 0;
      *ppPrefix = pPrefix;
      if( op==TK_VARIABLE ){
        Vdbe *v = pParse->pVdbe;
        sqlite3VdbeSetVarmask(v, pRight->iColumn);
        if( *pisComplete && pRight->u.zToken[1] ){
          /* If the rhs of the LIKE expression is a variable, and the current
          ** value of the variable means there is no need to invoke the LIKE
          ** function, then no OP_Variable will be added to the program.
          ** This causes problems for the sqlite3_bind_parameter_name()
          ** API. To workaround them, add a dummy OP_Variable here.
          **
          ** 如果LIKE表达式的右边是一个变量，并且当前变量的值代表没有必要唤醒LIKE函数。那么OP_Variable不会被添加到程序中。
          ** 这会引起sqlite3_bind_parameter_name() API的一些问题。为了解决它，在这添加一个虚拟的OP_Variable.
          **
          */ 
          int r1 = sqlite3GetTempReg(pParse);
          sqlite3ExprCodeTarget(pParse, pRight, r1);
          sqlite3VdbeChangeP3(v, sqlite3VdbeCurrentAddr(v)-1, 0);
          sqlite3ReleaseTempReg(pParse, r1);
        }
      }
    }else{
      z = 0;
    }
  }

  sqlite3ValueFree(pVal);
  return (z!=0);
}
#endif /* SQLITE_OMIT_LIKE_OPTIMIZATION */


#ifndef SQLITE_OMIT_VIRTUALTABLE
/*
** Check to see if the given expression is of the form
**
**         column MATCH expr
**
** If it is then return TRUE.  If not, return FALSE.
**
** 检查表达式是否是column MATCH expr形式，如果是则返回TRUE，否则返回FALSE
**
*/
static int isMatchOfColumn(
  Expr *pExpr      /* Test this expression */
){
  ExprList *pList;

  if( pExpr->op!=TK_FUNCTION ){
    return 0;
  }
  if( sqlite3StrICmp(pExpr->u.zToken,"match")!=0 ){
    return 0;
  }
  pList = pExpr->x.pList;
  if( pList->nExpr!=2 ){
    return 0;
  }
  if( pList->a[1].pExpr->op != TK_COLUMN ){
    return 0;
  }
  return 1;
}
#endif /* SQLITE_OMIT_VIRTUALTABLE */

/*
** If the pBase expression originated in the ON or USING clause of
** a join, then transfer the appropriate markings over to derived.
**
** 如果pBase表达式起源于一个连接的ON或USING子句，那么推导出适当的标记。
**
*/
static void transferJoinMarkings(Expr *pDerived, Expr *pBase){
  pDerived->flags |= pBase->flags & EP_FromJoin;
  pDerived->iRightJoinTable = pBase->iRightJoinTable;
}

#if !defined(SQLITE_OMIT_OR_OPTIMIZATION) && !defined(SQLITE_OMIT_SUBQUERY)
/*
** 分析一个包含多个OR连接的子term的term
** Analyze a term that consists of two or more OR-connected
** subterms.  例如下面这种形式:
**
**     ... WHERE  (a=5) AND (b=7 OR c=9 OR d=13) AND (d=13)
**                          ^^^^^^^^^^^^^^^^^^^^
** 分析一个包含两个或更多OR-connected子terms的term。
**
** This routine analyzes terms such as the middle term in the above example.
** A WhereOrTerm object is computed and attached to the term under
** analysis, regardless of the outcome of the analysis.  Hence:
**
**     WhereTerm.wtFlags   |=  TERM_ORINFO
**     WhereTerm.u.pOrInfo  =  a dynamically allocated WhereOrTerm object  动态地分配WhereOrTerm对象
**
** 这个程序分析诸如上面中部term的terms。
** 一个WhereOrTerm对象被计算和附加到term的分析下，不管分析的结果如何。
**
** The term being analyzed must have two or more of OR-connected subterms.
** 分析的term必须包含多个OR连接的子term，一个term应该是AND连接的子term
** A single subterm might be a set of AND-connected sub-subterms.
**
** term被分析必须有两个或多个OR-connected子terms.一个单独的子term应该是一组AND-connected sub-subterms.
**
** Examples of terms under analysis:
**
**     (A)     t1.x=t2.y OR t1.x=t2.z OR t1.y=15 OR t1.z=t3.a+5
**     (B)     x=expr1 OR expr2=x OR x=expr3
**     (C)     t1.x=t2.y OR (t1.x=t2.z AND t1.y=15)
**     (D)     x=expr1 OR (y>11 AND y<22 AND z LIKE '*hello*')
**     (E)     (p.a=1 AND q.b=2 AND r.c=3) OR (p.x=4 AND q.y=5 AND r.z=6)
**
** CASE 1:
**
** If all subterms are of the form T.C=expr for some single column of C
** a single table T (as shown in example B above) then create a new virtual
** term that is an equivalent IN expression.  In other words, if the term
** being analyzed is:
**
** 同一个属性多个等值条件用OR连接的处理
**
**      x = expr1  OR  expr2 = x  OR  x = expr3
**
** then create a new virtual term like this:
**
**      x IN (expr1,expr2,expr3)
**
** 如果是同一列的等式表达式，则创建一个新的虚拟的IN表达式的term替换。
**
** CASE 2:
**
** If all subterms are indexable by a single table T, then set
**
**     WhereTerm.eOperator              =  WO_OR
**     WhereTerm.u.pOrInfo->indexable  |=  the cursor number for table T
**
** A subterm is "indexable" if it is of the form
** "T.C <op> <expr>" where C is any column of table T and 
** <op> is one of "=", "<", "<=", ">", ">=", "IS NULL", or "IN".
** A subterm is also indexable if it is an AND of two or more
** subsubterms at least one of which is indexable.  Indexable AND 
** subterms have their eOperator set to WO_AND and they have
** u.pAndInfo set to a dynamically allocated WhereAndTerm object.
**
** 如果所有的子terms都是可加索引的，并且都属于同一个表T，那么设置
**     WhereTerm.eOperator              =  WO_OR
**     WhereTerm.u.pOrInfo->indexable  |=  the cursor number for table T
** 如果一个term是"T.C <op> <expr>"类型其中C是表T中的列，
** 并且<op>是"=", "<", "<=", ">", ">=", "IS NULL", or "IN"中的一个，那么
** 一个子term是"indexable"。
** 如果一个子term是由一个AND连接的两个或更多子子terms，并且其中的子子terms至少由一个可加索引的，
** 那么一个子term也是可加索引的。
** 可加索引的AND子terms把他们的eOperator设置为WO_AND并且u.pAndInfo设置为一个动态分配的WhereAndTerm对象。
**
** From another point of view, "indexable" means that the subterm could
** potentially be used with an index if an appropriate index exists.
** This analysis does not consider whether or not the index exists; that
** is something the bestIndex() routine will determine.  This analysis
** only looks at whether subterms appropriate for indexing exist.
**
** 从另一方面看，"可加索引的"意味着如果一个适当的索引存在，那么子term可能与一个索引使用。
** 这个分析不考虑索引是否存在:这是bestIndex()程序考虑的事。这个分析只查看子terms是否有适用的索引存在。
**
** All examples A through E above all satisfy case 2.  But if a term
** also statisfies case 1 (such as B) we know that the optimizer will
** always prefer case 1, so in that case we pretend that case 2 is not
** satisfied.
**
** 如果是同一表中的列，且如果是"=", "<", "<=", ">", ">=", "IS NULL", or "IN"，则可以使用索引优化
** 如果情况1和情况2都满足，则默认使用情况1优化
**
** It might be the case that multiple tables are indexable.  For example,
** (E) above is indexable on tables P, Q, and R.  多表也可以
**
** Terms that satisfy case 2 are candidates for lookup by using
** separate indices to find rowids for each subterm and composing
** the union of all rowids using a RowSet object.  This is similar
** to "bitmap indices" in other database engines.
**
** 满足情况2的terms可以使用各自的索引为每个subterm找到rowid并且使用RowSet对象组合所有rowid。
**
** OTHERWISE:
**
** If neither case 1 nor case 2 apply, then leave the eOperator set to
** zero.  This term is not useful for search.
*/
static void exprAnalyzeOrTerm(
  SrcList *pSrc,            /* the FROM clause FROM子句 */
  WhereClause *pWC,         /* the complete WHERE clause 完整的WHERE子句 */
  int idxTerm               /* Index of the OR-term to be analyzed 需要分析的OR-term索引 */
){
  Parse *pParse = pWC->pParse;            /* Parser context 解析上下文 */
  sqlite3 *db = pParse->db;               /* Database connection 数据库连接 */
  WhereTerm *pTerm = &pWC->a[idxTerm];    /* The term to be analyzed 要分析的term */
  Expr *pExpr = pTerm->pExpr;             /* The expression of the term term的表达式 */
  WhereMaskSet *pMaskSet = pWC->pMaskSet; /* Table use masks 表使用的掩码 */
  int i;                                  /* Loop counters 循环计数器 */
  WhereClause *pOrWc;       /* Breakup of pTerm into subterms 把pTerm分解成子terms */
  WhereTerm *pOrTerm;       /* A Sub-term within the pOrWc 一个有pOrWc的子term */
  WhereOrInfo *pOrInfo;     /* Additional information associated with pTerm 分配各pTerm的附加信息 */
  Bitmask chngToIN;         /* Tables that might satisfy case 1 满足情况1的表 */
  Bitmask indexable;        /* Tables that are indexable, satisfying case 2 可加索引且满足情况2的表 */

  /*
  ** Break the OR clause into its separate subterms.  The subterms are
  ** stored in a WhereClause structure containing within the WhereOrInfo
  ** object that is attached to the original OR clause term.
  **
  ** 把OR子句分解成单独的子terms.子terms存储在一个包含WhereOrInfo对象的WhereClause数据结构中，并且附加到原始的OR子句term中
  **
  */
  assert( (pTerm->wtFlags & (TERM_DYNAMIC|TERM_ORINFO|TERM_ANDINFO))==0 );
  assert( pExpr->op==TK_OR );
  pTerm->u.pOrInfo = pOrInfo = sqlite3DbMallocZero(db, sizeof(*pOrInfo));
  if( pOrInfo==0 ) return;
  pTerm->wtFlags |= TERM_ORINFO;
  pOrWc = &pOrInfo->wc;
  whereClauseInit(pOrWc, pWC->pParse, pMaskSet, pWC->wctrlFlags);
  whereSplit(pOrWc, pExpr, TK_OR);
  exprAnalyzeAll(pSrc, pOrWc);
  if( db->mallocFailed ) return;
  assert( pOrWc->nTerm>=2 );

  /*
  ** Compute the set of tables that might satisfy cases 1 or 2.
  **
  ** 计算可能满足情况1或情况2的表
  */
  indexable = ~(Bitmask)0;
  chngToIN = ~(pWC->vmask);
  for(i=pOrWc->nTerm-1, pOrTerm=pOrWc->a; i>=0 && indexable; i--, pOrTerm++){
    if( (pOrTerm->eOperator & WO_SINGLE)==0 ){
      WhereAndInfo *pAndInfo;
      assert( pOrTerm->eOperator==0 );
      assert( (pOrTerm->wtFlags & (TERM_ANDINFO|TERM_ORINFO))==0 );
      chngToIN = 0;
      pAndInfo = sqlite3DbMallocRaw(db, sizeof(*pAndInfo));
      if( pAndInfo ){
        WhereClause *pAndWC;
        WhereTerm *pAndTerm;
        int j;
        Bitmask b = 0;
        pOrTerm->u.pAndInfo = pAndInfo;
        pOrTerm->wtFlags |= TERM_ANDINFO;
        pOrTerm->eOperator = WO_AND;
        pAndWC = &pAndInfo->wc;
        whereClauseInit(pAndWC, pWC->pParse, pMaskSet, pWC->wctrlFlags);
        whereSplit(pAndWC, pOrTerm->pExpr, TK_AND);
        exprAnalyzeAll(pSrc, pAndWC);
        pAndWC->pOuter = pWC;
        testcase( db->mallocFailed );
        if( !db->mallocFailed ){
          for(j=0, pAndTerm=pAndWC->a; j<pAndWC->nTerm; j++, pAndTerm++){
            assert( pAndTerm->pExpr );
            if( allowedOp(pAndTerm->pExpr->op) ){
              b |= getMask(pMaskSet, pAndTerm->leftCursor);
            }
          }
        }
        indexable &= b;
      }
    }else if( pOrTerm->wtFlags & TERM_COPIED ){
      /* Skip this term for now.  We revisit it when we process the 暂时跳过这个term.当执行同意的TERM_VIRTUAL term时会重新访问它。
      ** corresponding TERM_VIRTUAL term */
    }else{
      Bitmask b;
      b = getMask(pMaskSet, pOrTerm->leftCursor);
      if( pOrTerm->wtFlags & TERM_VIRTUAL ){
        WhereTerm *pOther = &pOrWc->a[pOrTerm->iParent];
        b |= getMask(pMaskSet, pOther->leftCursor);
      }
      indexable &= b;
      if( pOrTerm->eOperator!=WO_EQ ){
        chngToIN = 0;
      }else{
        chngToIN &= b;
      }
    }
  }

  /*
  ** Record the set of tables that satisfy case 2.  The set might be
  ** empty. 记录满足情况2的表。这个可能为空。
  */
  pOrInfo->indexable = indexable;
  pTerm->eOperator = indexable==0 ? 0 : WO_OR;

  /*
  ** chngToIN holds a set of tables that *might* satisfy case 1.  But
  ** we have to do some additional checking to see if case 1 really
  ** is satisfied.
  ** 
  ** 出现第一种情况时的处理
  **
  ** chngToIN保存可能满足情况1的表。但我们需要做一些附加检查看看是不是真的满足情况1
  **
  ** chngToIN will hold either 0, 1, or 2 bits.  The 0-bit case means
  ** that there is no possibility of transforming the OR clause into an
  ** IN operator because one or more terms in the OR clause contain
  ** something other than == on a column in the single table.  The 1-bit
  ** case means that every term of the OR clause is of the form
  ** "table.column=expr" for some single table.  The one bit that is set
  ** will correspond to the common table.  We still need to check to make
  ** sure the same column is used on all terms.  The 2-bit case is when
  ** the all terms are of the form "table1.column=table2.column".  It
  ** might be possible to form an IN operator with either table1.column
  ** or table2.column as the LHS if either is common to every term of
  ** the OR clause.
  **
  ** chngToIN将保存0,1或2bits.
  ** 0-bit情况意味着因为OR自己种的一个或多个terms在一个表的一列上包含出==以外的其他东西，因此无法把OR子句转化为IN运算符。
  ** 1-bit情况意味着OR子句的每个term都是"table.column=expr"形式。我们还需要进一步验证所有的terms的列都是相同的。
  ** 2-bit情况意味着所有的terms都是"table1.column=table2.column"形式。它可能形成一个table1.column或table2.column作为左边的IN运算符。如果任何一个都是对OR子句的每个term都是共同的。
  **
  ** Note that terms of the form "table.column1=table.column2" (the
  ** same table on both sizes of the ==) cannot be optimized.
  ** 
  ** 注意:"table.column1=table.column2"(在=的两边都是同一个表)形式是不能优化的
  */
  if( chngToIN ){
    int okToChngToIN = 0;     /* True if the conversion to IN is valid 如果可以转换为IN则为TRUE */
    int iColumn = -1;         /* Column index on lhs of IN operator IN运算符左边的列索引 */
    int iCursor = -1;         /* Table cursor common to all terms 所有terms的共同的表游标 */
    int j = 0;                /* Loop counter 循环计数器 */

    /* Search for a table and column that appears on one side or the
    ** other of the == operator in every subterm.  That table and column
    ** will be recorded in iCursor and iColumn.  There might not be any
    ** such table and column.  Set okToChngToIN if an appropriate table
    ** and column is found but leave okToChngToIN false if not found.
    **
    ** 查找一个在每个term中的==运算符出现的表和列。那个表和列会被记录在iCursor和iColumn中。
    ** 有可能没有任何表和列被记录。如果一个适当的表和列被查找到，则设置okToChngToIN，但是如果没有找到，则设置okToChngToIN为FALSE
    **
    */
    for(j=0; j<2 && !okToChngToIN; j++){
      pOrTerm = pOrWc->a;
      for(i=pOrWc->nTerm-1; i>=0; i--, pOrTerm++){
        assert( pOrTerm->eOperator==WO_EQ );
        pOrTerm->wtFlags &= ~TERM_OR_OK;
        if( pOrTerm->leftCursor==iCursor ){
          /* This is the 2-bit case and we are on the second iteration and   这是2-bit的情况并且我们是在第二次迭代下并且当前term是来自于第一次迭代。所以跳过这个term
          ** current term is from the first iteration.  So skip this term. */
          assert( j==1 );
          continue;
        }
        if( (chngToIN & getMask(pMaskSet, pOrTerm->leftCursor))==0 ){
          /* This term must be of the form t1.a==t2.b where t2 is in the 这个term必须是t1.a==t2.b这种形式，且t2是在chngToIN中设置，但t1不是。
          ** chngToIN set but t1 is not.  This term will be either preceeded 这个term将被执行或进行反转复制(t2.b==t1.a)
          ** or follwed by an inverted copy (t2.b==t1.a).  Skip this term  跳过这个term并且使用它的反转式
          ** and use its inversion. */
          testcase( pOrTerm->wtFlags & TERM_COPIED );
          testcase( pOrTerm->wtFlags & TERM_VIRTUAL );
          assert( pOrTerm->wtFlags & (TERM_COPIED|TERM_VIRTUAL) );
          continue;
        }
        iColumn = pOrTerm->u.leftColumn;
        iCursor = pOrTerm->leftCursor;
        break;
      }
      if( i<0 ){
        /* No candidate table+column was found.  This can only occur 没有候选表和列被发现。这只能发生在第二次循环
        ** on the second iteration */
        assert( j==1 );
        assert( (chngToIN&(chngToIN-1))==0 );
        assert( chngToIN==getMask(pMaskSet, iCursor) );
        break;
      }
      testcase( j==1 );

      /* We have found a candidate table and column.  Check to see if that 已经发现了一个候选表和列。查看表和列是否对每个OR子句中的term都是共同的。
      ** table and column is common to every term in the OR clause */
      okToChngToIN = 1;
      for(; i>=0 && okToChngToIN; i--, pOrTerm++){
        assert( pOrTerm->eOperator==WO_EQ );
        if( pOrTerm->leftCursor!=iCursor ){
          pOrTerm->wtFlags &= ~TERM_OR_OK;
        }else if( pOrTerm->u.leftColumn!=iColumn ){
          okToChngToIN = 0;
        }else{
          int affLeft, affRight;
          /* If the right-hand side is also a column, then the affinities  如果右边也是一个列，那么左右两边的亲和性是必须的。
          ** of both right and left sides must be such that no type 在右边必须不能有类型转换。
          ** conversions are required on the right.  (Ticket #2249)
          */
          affRight = sqlite3ExprAffinity(pOrTerm->pExpr->pRight);
          affLeft = sqlite3ExprAffinity(pOrTerm->pExpr->pLeft);
          if( affRight!=0 && affRight!=affLeft ){
            okToChngToIN = 0;
          }else{
            pOrTerm->wtFlags |= TERM_OR_OK;
          }
        }
      }
    }

    /* At this point, okToChngToIN is true if original pTerm satisfies
    ** case 1.  In that case, construct a new virtual term that is 
    ** pTerm converted into an IN operator.
    **
    ** 这时，如果原始的pTerm满足情况1则okToChngToIN为TRUE.如果那样，需要构造一个把pTerm转换为IN操作的新的虚拟的term.
    **
    ** EV: R-00211-15100
    */
    if( okToChngToIN ){
      Expr *pDup;            /* A transient duplicate expression 一个临时的复制的表达式 */
      ExprList *pList = 0;   /* The RHS of the IN operator IN操作符右边 */
      Expr *pLeft = 0;       /* The LHS of the IN operator IN操作符左边 */
      Expr *pNew;            /* The complete IN operator 完整的IN操作符 */

      for(i=pOrWc->nTerm-1, pOrTerm=pOrWc->a; i>=0; i--, pOrTerm++){
        if( (pOrTerm->wtFlags & TERM_OR_OK)==0 ) continue;
        assert( pOrTerm->eOperator==WO_EQ );
        assert( pOrTerm->leftCursor==iCursor );
        assert( pOrTerm->u.leftColumn==iColumn );
        pDup = sqlite3ExprDup(db, pOrTerm->pExpr->pRight, 0);
        pList = sqlite3ExprListAppend(pWC->pParse, pList, pDup);
        pLeft = pOrTerm->pExpr->pLeft;
      }
      assert( pLeft!=0 );
      pDup = sqlite3ExprDup(db, pLeft, 0);
      pNew = sqlite3PExpr(pParse, TK_IN, pDup, 0, 0);
      if( pNew ){
        int idxNew;
        transferJoinMarkings(pNew, pExpr);
        assert( !ExprHasProperty(pNew, EP_xIsSelect) );
        pNew->x.pList = pList;
        idxNew = whereClauseInsert(pWC, pNew, TERM_VIRTUAL|TERM_DYNAMIC);
        testcase( idxNew==0 );
        exprAnalyze(pSrc, pWC, idxNew);
        pTerm = &pWC->a[idxTerm];
        pWC->a[idxNew].iParent = idxTerm;
        pTerm->nChild = 1;
      }else{
        sqlite3ExprListDelete(db, pList);
      }
      pTerm->eOperator = WO_NOOP;  /* case 1 trumps case 2 情况1胜过情况2 */
    }
  }
}
#endif /* !SQLITE_OMIT_OR_OPTIMIZATION && !SQLITE_OMIT_SUBQUERY */


/*
** The input to this routine is an WhereTerm structure with only the
** "pExpr" field filled in.  The job of this routine is to analyze the
** subexpression and populate all the other fields of the WhereTerm
** structure.
**
** 这个程序的输入是一个只有"pExpr"字段被填充的WhereTerm数据结构。
** 这个程序的作用是分析子表达式和填充WhereTerm数据结构的其他字段。
**
** If the expression is of the form "<expr> <op> X" it gets commuted
** to the standard form of "X <op> <expr>".
**
** 如果表达式是"<expr> <op> X"形式，把他转化为标准形式"X <op> <expr>".
**
** If the expression is of the form "X <op> Y" where both X and Y are
** columns, then the original expression is unchanged and a new virtual
** term of the form "Y <op> X" is added to the WHERE clause and
** analyzed separately.  The original term is marked with TERM_COPIED
** and the new term is marked with TERM_DYNAMIC (because it's pExpr
** needs to be freed with the WhereClause) and TERM_VIRTUAL (because it
** is a commuted copy of a prior term.)  The original term has nChild=1
** and the copy has idxParent set to the index of the original term.
**
** 如果表达式是"X <op> Y"形式，其中X和Y都是列，
** 然后，原始的表达式不会改变并且会在WHERE子句中添加一个新的虚拟的term--"Y <op> X"并且分别被分析。
** 原始的term被标上TERM_COPIED，并且新的term被标上TERM_DYNAMIC(因为它是pExpr，需要在WhereClause上释放)
** 和TERM_VIRTUAL(因为它一个先前的term的复制)。
** 原始的term有nChild=1，并且复本有idxParent，并且把idxParent设置为原始term的下标
*/
static void exprAnalyze(
  SrcList *pSrc,            /* the FROM clause FROM子句 */
  WhereClause *pWC,         /* the WHERE clause WHERE子句 */
  int idxTerm               /* Index of the term to be analyzed 需要分析的term下标 */
){
  WhereTerm *pTerm;                /* The term to be analyzed 需要分析的term  */
  WhereMaskSet *pMaskSet;          /* Set of table index masks 设置表索引掩码 */
  Expr *pExpr;                     /* The expression to be analyzed 需要分析的表达式 */
  Bitmask prereqLeft;              /* Prerequesites of the pExpr->pLeft pExpr->pLeft的前提条件  */
  Bitmask prereqAll;               /* Prerequesites of pExpr pExpr的前提条件 */
  Bitmask extraRight = 0;          /* Extra dependencies on LEFT JOIN 在左联接中的额外的依赖 */
  Expr *pStr1 = 0;                 /* RHS of LIKE/GLOB operator LIKE/GLOB运算符的右边 */
  int isComplete = 0;              /* RHS of LIKE/GLOB ends with wildcard LIKE/GLOB右边是由通配符结束 */
  int noCase = 0;                  /* LIKE/GLOB distinguishes case LIKE/GLOB区分大小写 */
  int op;                          /* Top-level operator.  pExpr->op */
  Parse *pParse = pWC->pParse;     /* Parsing context 分析上下文 */
  sqlite3 *db = pParse->db;        /* Database connection 数据库连接 */

  if( db->mallocFailed ){
    return;
  }
  pTerm = &pWC->a[idxTerm];
  pMaskSet = pWC->pMaskSet;
  pExpr = pTerm->pExpr;
  prereqLeft = exprTableUsage(pMaskSet, pExpr->pLeft);
  op = pExpr->op;
  if( op==TK_IN ){
    assert( pExpr->pRight==0 );
    if( ExprHasProperty(pExpr, EP_xIsSelect) ){
      pTerm->prereqRight = exprSelectTableUsage(pMaskSet, pExpr->x.pSelect);
    }else{
      pTerm->prereqRight = exprListTableUsage(pMaskSet, pExpr->x.pList);
    }
  }else if( op==TK_ISNULL ){
    pTerm->prereqRight = 0;
  }else{
    pTerm->prereqRight = exprTableUsage(pMaskSet, pExpr->pRight);
  }
  prereqAll = exprTableUsage(pMaskSet, pExpr);
  if( ExprHasProperty(pExpr, EP_FromJoin) ){
    Bitmask x = getMask(pMaskSet, pExpr->iRightJoinTable);
    prereqAll |= x;
    extraRight = x-1;  /* ON clause terms may not be used with an index 在左连接的左表中的ON子句terms可能不能与索引一起被使用
                       ** on left table of a LEFT JOIN.  Ticket #3015 */
  }
  pTerm->prereqAll = prereqAll;
  pTerm->leftCursor = -1;
  pTerm->iParent = -1;
  pTerm->eOperator = 0;
  if( allowedOp(op) && (pTerm->prereqRight & prereqLeft)==0 ){
    Expr *pLeft = pExpr->pLeft;
    Expr *pRight = pExpr->pRight;
    if( pLeft->op==TK_COLUMN ){
      pTerm->leftCursor = pLeft->iTable;
      pTerm->u.leftColumn = pLeft->iColumn;
      pTerm->eOperator = operatorMask(op);
    }
    if( pRight && pRight->op==TK_COLUMN ){
      WhereTerm *pNew;
      Expr *pDup;
      if( pTerm->leftCursor>=0 ){
        int idxNew;
        pDup = sqlite3ExprDup(db, pExpr, 0);
        if( db->mallocFailed ){
          sqlite3ExprDelete(db, pDup);
          return;
        }
        idxNew = whereClauseInsert(pWC, pDup, TERM_VIRTUAL|TERM_DYNAMIC);
        if( idxNew==0 ) return;
        pNew = &pWC->a[idxNew];
        pNew->iParent = idxTerm;
        pTerm = &pWC->a[idxTerm];
        pTerm->nChild = 1;
        pTerm->wtFlags |= TERM_COPIED;
      }else{
        pDup = pExpr;
        pNew = pTerm;
      }
      exprCommute(pParse, pDup);
      pLeft = pDup->pLeft;
      pNew->leftCursor = pLeft->iTable;
      pNew->u.leftColumn = pLeft->iColumn;
      testcase( (prereqLeft | extraRight) != prereqLeft );
      pNew->prereqRight = prereqLeft | extraRight;
      pNew->prereqAll = prereqAll;
      pNew->eOperator = operatorMask(pDup->op);
    }
  }

#ifndef SQLITE_OMIT_BETWEEN_OPTIMIZATION
  /* If a term is the BETWEEN operator, create two new virtual terms
  ** that define the range that the BETWEEN implements.  For example:
  **
  **      a BETWEEN b AND c
  **
  ** is converted into:
  **
  **      (a BETWEEN b AND c) AND (a>=b) AND (a<=c)
  **
  ** The two new terms are added onto the end of the WhereClause object.
  ** The new terms are "dynamic" and are children of the original BETWEEN
  ** term.  That means that if the BETWEEN term is coded, the children are
  ** skipped.  Or, if the children are satisfied by an index, the original
  ** BETWEEN term is skipped.
<<<<<<< HEAD
  ** BETWEEN语句处理部分
=======
  **
  ** 如果一个term是BETWEEN运算符，创建两个新的虚拟terms来定义BETWEEN的范围。
  ** 例如:
  **      a BETWEEN b AND c
  ** 转化为:
  **      (a BETWEEN b AND c) AND (a>=b) AND (a<=c)
  ** 两个新的terms被添加到WhereClause对象的最后。
  ** 新的terms是"dynamic"并且是原始BETWEEN term的子term.
  ** 这意味着如果BETWEEN term已经编码，那么孩子term将被跳过.
  ** 或者，如果孩子term满足可以使用索引，那么原始的BETWEEN term将被跳过。
  **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  else if( pExpr->op==TK_BETWEEN && pWC->op==TK_AND ){
    ExprList *pList = pExpr->x.pList;
    int i;
    static const u8 ops[] = {TK_GE, TK_LE};
    assert( pList!=0 );
    assert( pList->nExpr==2 );
    for(i=0; i<2; i++){
      Expr *pNewExpr; //用于保存转换后的新表达式
      int idxNew;  //新插入的表达式的在WhereClause中的下标
      pNewExpr = sqlite3PExpr(pParse, ops[i], 	//创建>=和<=表达式
                             sqlite3ExprDup(db, pExpr->pLeft, 0),
                             sqlite3ExprDup(db, pList->a[i].pExpr, 0), 0);
      idxNew = whereClauseInsert(pWC, pNewExpr, TERM_VIRTUAL|TERM_DYNAMIC);
      testcase( idxNew==0 ); //检测是否转换成功
      exprAnalyze(pSrc, pWC, idxNew);
      pTerm = &pWC->a[idxTerm];
      pWC->a[idxNew].iParent = idxTerm; //表示新建的子句是between的子句的转化
    }
    pTerm->nChild = 2;
  }
#endif /* SQLITE_OMIT_BETWEEN_OPTIMIZATION */

#if !defined(SQLITE_OMIT_OR_OPTIMIZATION) && !defined(SQLITE_OMIT_SUBQUERY)
  /* Analyze a term that is composed of two or more subterms connected by
  ** an OR operator.  分析由OR运算符连接的两个或更多的子terms组成
  */
  else if( pExpr->op==TK_OR ){  //如果该表达式是由OR运算符连接
    assert( pWC->op==TK_AND ); //如果where子句是由and分隔的
    exprAnalyzeOrTerm(pSrc, pWC, idxTerm); //优化该子句
    pTerm = &pWC->a[idxTerm];
  }
#endif /* SQLITE_OMIT_OR_OPTIMIZATION */

#ifndef SQLITE_OMIT_LIKE_OPTIMIZATION
  /* Add constraints to reduce the search space on a LIKE or GLOB
  ** operator.
  **
  ** A like pattern of the form "x LIKE 'abc%'" is changed into constraints
  **
  **          x>='abc' AND x<'abd' AND x LIKE 'abc%'
  **
  ** The last character of the prefix "abc" is incremented to form the
  ** termination condition "abd".
<<<<<<< HEAD
  ** LIKE语句处理部分
=======
  **
  ** 添加约束来减少LIKE或GLOB运算符的搜索空间。
  ** like模式"x LIKE 'abc%'"可以转变为约束
  **          x>='abc' AND x<'abd' AND x LIKE 'abc%'
  ** 前缀"abc"的最后一个字符一直增加，结束条件为"abd".
  **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  if( pWC->op==TK_AND 
   && isLikeOrGlob(pParse, pExpr, &pStr1, &isComplete, &noCase) //判断是否是可以优化的LIKE或GLOB操作
  ){
    Expr *pLeft;       /* LHS of LIKE/GLOB operator LIKE/GLOB运算符的左边 */
    Expr *pStr2;       /* Copy of pStr1 - RHS of LIKE/GLOB operator LIKE/GLOB运算符的pStr1 - RHS的复本 */
    Expr *pNewExpr1;
    Expr *pNewExpr2;
    int idxNew1;
    int idxNew2;
    CollSeq *pColl;    /* Collating sequence to use 使用排序序列 */

    pLeft = pExpr->x.pList->a[1].pExpr;
    pStr2 = sqlite3ExprDup(db, pStr1, 0);
    if( !db->mallocFailed ){ //如果初始化成功
      u8 c, *pC;       /* Last character before the first wildcard 在第一个通配符前的最后一个字符 */
      pC = (u8*)&pStr2->u.zToken[sqlite3Strlen30(pStr2->u.zToken)-1];
      c = *pC;
      if( noCase ){  //如果like或glob区分大小写
        /* The point is to increment the last character before the first
        ** wildcard.  But if we increment '@', that will push it into the
        ** alphabetic range where case conversions will mess up the 
        ** inequality.  To avoid this, make sure to also run the full
        ** LIKE on all candidate expressions by clearing the isComplete flag
        **
        ** 目的是在第一个通配符前增加最后一个字符。
        ** 但如果我们增加'@',那将会把它移出字母表的范围，那么字符转换将陷入不平等的混乱。
        ** 为了避免这种情况，使用清除isComplete标志来确保在所有候选表达式上也运行完整的LIKE
        **
        */
        if( c=='A'-1 ) isComplete = 0; /* EV: R-64339-08207 */


        c = sqlite3UpperToLower[c];
      }
      *pC = c + 1;	//设置<表达式的字符串的最后一个字符
    }
    pColl = sqlite3FindCollSeq(db, SQLITE_UTF8, noCase ? "NOCASE" : "BINARY",0); //排序的方式
    pNewExpr1 = sqlite3PExpr(pParse, TK_GE, 
                     sqlite3ExprSetColl(sqlite3ExprDup(db,pLeft,0), pColl),
                     pStr1, 0);  //创建>=表达式
    idxNew1 = whereClauseInsert(pWC, pNewExpr1, TERM_VIRTUAL|TERM_DYNAMIC); //插入>=表达式到where子句中
    testcase( idxNew1==0 ); //测试是否插入成功
    exprAnalyze(pSrc, pWC, idxNew1); //分析表达式格式是否为x <op> <expr>，如果不是则转化为这种形式
    pNewExpr2 = sqlite3PExpr(pParse, TK_LT,
                     sqlite3ExprSetColl(sqlite3ExprDup(db,pLeft,0), pColl),
                     pStr2, 0);  //创建<表达式
    idxNew2 = whereClauseInsert(pWC, pNewExpr2, TERM_VIRTUAL|TERM_DYNAMIC); //插入<表达式到where子句中
    testcase( idxNew2==0 ); //测试是否插入成功
    exprAnalyze(pSrc, pWC, idxNew2); //分析表达式格式是否为x <op> <expr>，如果不是则转化为这种形式
    pTerm = &pWC->a[idxTerm];
    if( isComplete ){  //如果like或glob右边是由通配符结束，设置新创建的子句属于like语句
      pWC->a[idxNew1].iParent = idxTerm;
      pWC->a[idxNew2].iParent = idxTerm;
      pTerm->nChild = 2;
    }
  }
#endif /* SQLITE_OMIT_LIKE_OPTIMIZATION */

#ifndef SQLITE_OMIT_VIRTUALTABLE
  /* Add a WO_MATCH auxiliary term to the constraint set if the
  ** current expression is of the form:  column MATCH expr.
  ** This information is used by the xBestIndex methods of
  ** virtual tables.  The native query optimizer does not attempt
  ** to do anything with MATCH functions.
  **
  ** 如果当前表达式是column MATCH expr形式时，添加一个WO_MATCH辅助term约束集合。
  ** 这个信息是通过虚表的xBestIndex方法使用的。本地的查询优化不尝试使用MATCH函数做任何事情
  */
  if( isMatchOfColumn(pExpr) ){
    int idxNew;
    Expr *pRight, *pLeft;
    WhereTerm *pNewTerm;
    Bitmask prereqColumn, prereqExpr;

    pRight = pExpr->x.pList->a[0].pExpr;
    pLeft = pExpr->x.pList->a[1].pExpr;
    prereqExpr = exprTableUsage(pMaskSet, pRight);
    prereqColumn = exprTableUsage(pMaskSet, pLeft);
    if( (prereqExpr & prereqColumn)==0 ){
      Expr *pNewExpr;
      pNewExpr = sqlite3PExpr(pParse, TK_MATCH, 
                              0, sqlite3ExprDup(db, pRight, 0), 0);
      idxNew = whereClauseInsert(pWC, pNewExpr, TERM_VIRTUAL|TERM_DYNAMIC);
      testcase( idxNew==0 );
      pNewTerm = &pWC->a[idxNew];
      pNewTerm->prereqRight = prereqExpr;
      pNewTerm->leftCursor = pLeft->iTable;
      pNewTerm->u.leftColumn = pLeft->iColumn;
      pNewTerm->eOperator = WO_MATCH;
      pNewTerm->iParent = idxTerm;
      pTerm = &pWC->a[idxTerm];
      pTerm->nChild = 1;
      pTerm->wtFlags |= TERM_COPIED;
      pNewTerm->prereqAll = pTerm->prereqAll;
    }
  }
#endif /* SQLITE_OMIT_VIRTUALTABLE */

#ifdef SQLITE_ENABLE_STAT3
  /* When sqlite_stat3 histogram data is available an operator of the
  ** form "x IS NOT NULL" can sometimes be evaluated more efficiently
  ** as "x>NULL" if x is not an INTEGER PRIMARY KEY.  So construct a
  ** virtual term of that form.
  **
  ** Note that the virtual term must be tagged with TERM_VNULL.  This
  ** TERM_VNULL tag will suppress the not-null check at the beginning
  ** of the loop.  Without the TERM_VNULL flag, the not-null check at
  ** the start of the loop will prevent any results from being returned.
  **
  ** 当sqlite_stat3直方图数据是有效的一个"x IS NOT NULL"形式的运算符，
  ** 如果x不是INTEGER PRIMARY KEY，那么"x IS NOT NULL"形式被认为'x>NULL'是更加有效的。所以构建一个那个形式的虚拟term
  ** 注意:虚拟的term必须标记为TERM_VNULL.TERM_VNULL标记将在循环一开始就废止not-null检查。
  ** 没有TERM_VNULL标志，在循环开始的not-null检查将阻止如何结果的返回。
  **
  */
  if( pExpr->op==TK_NOTNULL
   && pExpr->pLeft->op==TK_COLUMN
   && pExpr->pLeft->iColumn>=0
  ){
    Expr *pNewExpr;
    Expr *pLeft = pExpr->pLeft;
    int idxNew;
    WhereTerm *pNewTerm;

    pNewExpr = sqlite3PExpr(pParse, TK_GT,
                            sqlite3ExprDup(db, pLeft, 0),
                            sqlite3PExpr(pParse, TK_NULL, 0, 0, 0), 0);

    idxNew = whereClauseInsert(pWC, pNewExpr,
                              TERM_VIRTUAL|TERM_DYNAMIC|TERM_VNULL);
    if( idxNew ){
      pNewTerm = &pWC->a[idxNew];
      pNewTerm->prereqRight = 0;
      pNewTerm->leftCursor = pLeft->iTable;
      pNewTerm->u.leftColumn = pLeft->iColumn;
      pNewTerm->eOperator = WO_GT;
      pNewTerm->iParent = idxTerm;
      pTerm = &pWC->a[idxTerm];
      pTerm->nChild = 1;
      pTerm->wtFlags |= TERM_COPIED;
      pNewTerm->prereqAll = pTerm->prereqAll;
    }
  }
#endif /* SQLITE_ENABLE_STAT */

  /* Prevent ON clause terms of a LEFT JOIN from being used to drive
  ** an index for tables to the left of the join.
  **
  ** 防止一个左连接的ON子句terms用于驱动一个表的索引到连接的左边
  */
  pTerm->prereqRight |= extraRight;
}

/*
** Return TRUE if any of the expressions in pList->a[iFirst...] contain
** a reference to any table other than the iBase table.
**
** 如果在pList->a[iFirst...]中的任何的表达式与除了iBase表之外的任何表有关联，那么返回TRUE
**
*/
static int referencesOtherTables(
  ExprList *pList,          /* Search expressions in ths list 在这个list中查找表达式 */
  WhereMaskSet *pMaskSet,   /* Mapping from tables to bitmaps 表和位图之间的映射 */
  int iFirst,               /* Be searching with the iFirst-th expression 与iFirst-th一起搜索表达式 */
  int iBase                 /* Ignore references to this table 忽略对这个表的引用 */
){
  Bitmask allowed = ~getMask(pMaskSet, iBase);
  while( iFirst<pList->nExpr ){
    if( (exprTableUsage(pMaskSet, pList->a[iFirst++].pExpr)&allowed)!=0 ){
      return 1;
    }
  }
  return 0;
}

/*
** This function searches the expression list passed as the second argument
** for an expression of type TK_COLUMN that refers to the same column and
** uses the same collation sequence as the iCol'th column of index pIdx.
** Argument iBase is the cursor number used for the table that pIdx refers
** to.
**
** 这个函数查询表达式列表作为第二个参数传递给类型TK_COLUMN的表达式，
** 表达式引用相同的列并且使用相同的排序序列作为索引pIdx的iCol'th列。
** 参数iBase代表游标数被用在pIdx引用的表上。
**
** If such an expression is found, its index in pList->a[] is returned. If
** no expression is found, -1 is returned.
**
** 若一个表达式被查到，返回它的在pList->a[]下标。如果没有查到，则返回-1.
*/
static int findIndexCol(
  Parse *pParse,                  /* Parse context 分析上下文 */
  ExprList *pList,                /* Expression list to search 用于查找的表达式列表 */
  int iBase,                      /* Cursor for table associated with pIdx 与pIdx相关的表游标 */
  Index *pIdx,                    /* Index to match column of 相匹配的索引列 */
  int iCol                        /* Column of index to match 匹配的索引列 */
){
  int i;
  const char *zColl = pIdx->azColl[iCol];

  for(i=0; i<pList->nExpr; i++){
    Expr *p = pList->a[i].pExpr;
    if( p->op==TK_COLUMN
     && p->iColumn==pIdx->aiColumn[iCol]
     && p->iTable==iBase
    ){
      CollSeq *pColl = sqlite3ExprCollSeq(pParse, p);
      if( ALWAYS(pColl) && 0==sqlite3StrICmp(pColl->zName, zColl) ){
        return i;
      }
    }
  }

  return -1;
}

/*
** This routine determines if pIdx can be used to assist in processing a
** DISTINCT qualifier. In other words, it tests whether or not using this
** index for the outer loop guarantees that rows with equal values for
** all expressions in the pDistinct list are delivered grouped together.
**
** For example, the query 
**
**   SELECT DISTINCT a, b, c FROM tbl WHERE a = ?
**
** can benefit from any index on columns "b" and "c".
**
** 这个程序决定如果pIdx能被用于辅助在程序执行中的DISTINCT限定。
** 换句话说，它测试是否为了外部循环使用这个索引来保证在pDistinct列表中所有表达式中等值的行是组合在一起交付的
*/
static int isDistinctIndex(
  Parse *pParse,                  /* Parsing context 分析上下文 */
  WhereClause *pWC,               /* The WHERE clause WHERE子句 */
  Index *pIdx,                    /* The index being considered 被考虑的索引 */
  int base,                       /* Cursor number for the table pIdx is on pIdx使用的表游标数 */
  ExprList *pDistinct,            /* The DISTINCT expressions DISTINCT表达式 */
  int nEqCol                      /* Number of index columns with == ==中索引列的数目 */
){
  Bitmask mask = 0;               /* Mask of unaccounted for pDistinct exprs 未解释的pDistinct exprs掩码 */
  int i;                          /* Iterator variable 迭代变量 */

  if( pIdx->zName==0 || pDistinct==0 || pDistinct->nExpr>=BMS ) return 0;
  testcase( pDistinct->nExpr==BMS-1 );

  /* Loop through all the expressions in the distinct list. If any of them
  ** are not simple column references, return early. Otherwise, test if the
  ** WHERE clause contains a "col=X" clause. If it does, the expression
  ** can be ignored. If it does not, and the column does not belong to the
  ** same table as index pIdx, return early. Finally, if there is no
  ** matching "col=X" expression and the column is on the same table as pIdx,
  ** set the corresponding bit in variable mask.
  **
  ** 循环遍历在distinct list中的所有的表达式。如果他们中的任何一个不是简单的列引用，则提前返回。
  ** 否则，如果WHERE子句包含"col=X"子句，那么就测试。
  ** 如果包含，表达式就能被忽略。如果没有包含，并且列作为索引pIdx也不属于同一表，提前返回。
  ** 最后，如果没有与"col=X"表达式匹配并且列作为pIdx不在同一表中，在变量掩码中设置相应的位
  **
  */
  for(i=0; i<pDistinct->nExpr; i++){
    WhereTerm *pTerm;
    Expr *p = pDistinct->a[i].pExpr;
    if( p->op!=TK_COLUMN ) return 0;
    pTerm = findTerm(pWC, p->iTable, p->iColumn, ~(Bitmask)0, WO_EQ, 0);
    if( pTerm ){
      Expr *pX = pTerm->pExpr;
      CollSeq *p1 = sqlite3BinaryCompareCollSeq(pParse, pX->pLeft, pX->pRight);
      CollSeq *p2 = sqlite3ExprCollSeq(pParse, p);
      if( p1==p2 ) continue;
    }
    if( p->iTable!=base ) return 0;
    mask |= (((Bitmask)1) << i);
  }

  for(i=nEqCol; mask && i<pIdx->nColumn; i++){
    int iExpr = findIndexCol(pParse, pDistinct, base, pIdx, i);
    if( iExpr<0 ) break;
    mask &= ~(((Bitmask)1) << iExpr);
  }

  return (mask==0);
}



/*毕赣斌开始
** Return true if the DISTINCT expression-list passed as the third argument
** is redundant. A DISTINCT list is redundant if the database contains a
** UNIQUE index that guarantees that the result of the query will be distinct
** anyway.
**
** 如果DISTINCT表达式list当做第三个参数是多余的，就返回true.
** 如果数据库包含一个UNIQUE索引保证查询的结果也总是唯一的，那么一个DISTINCT list是多余的，
*/
/* 如果DISTINCT表达式列表是作为第三参数是传递冗余，则返回1。如果数据库中包含一
** 个保证查询结果完全不同的唯一索引存在，则这个DISTINCT列表是冗余的。
*/
static int isDistinctRedundant(
  Parse *pParse,
  SrcList *pTabList,
  WhereClause *pWC,
  ExprList *pDistinct
){
  Table *pTab;
  Index *pIdx;
  int i;                          
  int iBase;

  /* If there is more than one table or sub-select in the FROM clause of
  ** this query, then it will not be possible to show that the DISTINCT 
<<<<<<< HEAD
  ** clause is redundant. */
  /* 如果此次查询的FROM子句中有多个表格或子查询，则不会显示DISTINCT子句是
  ** 冗余的。
  */
=======
  ** clause is redundant.
  ** 如果在这个查询的FORM子句中有多余一个表或sub-select
  ** 那么它将不可能指示DISTINCT子句是多余的 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  if( pTabList->nSrc!=1 ) return 0;
  iBase = pTabList->a[0].iCursor;
  pTab = pTabList->a[0].pTab;

  /* If any of the expressions is an IPK column on table iBase, then return 
  ** true. Note: The (p->iTable==iBase) part of this test may be false if the
  ** current SELECT is a correlated sub-query.
  ** 如果任何一个表达式在表iBase中是一个IPK列，那么就返回true.
  ** 注意:如果当前的SELECT是一个有相互关系的子查询，那么这个测试的部分(p->iTable==iBase)可能是错的。
  */
  /* 如果表格iBase中有任意表达式是IPK容量，则返回真。注：如果当前SELECT是一个
  ** 相关子查询，则此次测试中（p->iTable==iBase）部分可能为假。
  */
  for(i=0; i<pDistinct->nExpr; i++){
    Expr *p = pDistinct->a[i].pExpr;
    if( p->op==TK_COLUMN && p->iTable==iBase && p->iColumn<0 ) return 1;
  }

  /* Loop through all indices on the table, checking each to see if it makes
  ** the DISTINCT qualifier redundant. It does so if:
  **
  **   1. The index is itself UNIQUE, and
  **
  **   2. All of the columns in the index are either part of the pDistinct
  **      list, or else the WHERE clause contains a term of the form "col=X",
  **      where X is a constant value. The collation sequences of the
  **      comparison and select-list expressions must match those of the index.
  **
  **   3. All of those index columns for which the WHERE clause does not
  **      contain a "col=X" term are subject to a NOT NULL constraint.
  **
  ** 循环遍历在表中的所有索引，检查每个索引查看它是否会使DISTINCT限制成为多余的。
  ** 如果满足下面条件那么就会使DISTINCT限制成为多余的:
  ** 	1.索引本身是UNIQUE,并且
  **		2.在索引中的所有的列要么是pDistinct列表的一部分，要么是包含形式为"col=X"的term的WHERE子句(其中X是一个常量)。
  **		  比较关系的排序序列和select-list表达式必须也这些索引相匹配
  **		3.WHERE子句的所有的这些索引列不包含有一个NOT NULL约束的"col=X"term
  */
  /* 遍历表上的所有指标,检查每个指针是否使DISTINCT限定符冗余。当且仅当满足以下
  ** 条件:
  **
  ** 1、这个索引本身含UNIQUE约束，并且
  **
  ** 2、这个索引中所有的列或者是pDistinct列表中的部分，或者WHERE子句中包含形如
  ** “col=X”的项，其中X是常量。比较和选择列表的排序序列表达式必须匹配这些索引
  ** 。
  ** 
  ** 3、所有不包含“col=X”项的WHERE子句的索引列都满足NOT NULL约束条件。
  */
  for(pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext){
    if( pIdx->onError==OE_None ) continue;
    for(i=0; i<pIdx->nColumn; i++){
      int iCol = pIdx->aiColumn[i];
      if( 0==findTerm(pWC, iBase, iCol, ~(Bitmask)0, WO_EQ, pIdx) ){
        int iIdxCol = findIndexCol(pParse, pDistinct, iBase, pIdx, i);
        if( iIdxCol<0 || pTab->aCol[pIdx->aiColumn[i]].notNull==0 ){
          break;
        }
      }
    }
    if( i==pIdx->nColumn ){
<<<<<<< HEAD
      /* This index implies that the DISTINCT qualifier is redundant. */
	  /* 这个索引意味着DISTINCT限定符是冗余的。*/
=======
      /* This index implies that the DISTINCT qualifier is redundant. 这个索引暗示DISTINCT是多余的 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
      return 1;
    }
  }

  return 0;
}

/*
** This routine decides if pIdx can be used to satisfy the ORDER BY
** clause.  If it can, it returns 1.  If pIdx cannot satisfy the
** ORDER BY clause, this routine returns 0.
**
** 这个程序决定ORDER BY子句是否能使用pIdx.如果可以则返回1，如果不行则返回0。
**
** pOrderBy is an ORDER BY clause from a SELECT statement.  pTab is the
** left-most table in the FROM clause of that same SELECT statement and
** the table has a cursor number of "base".  pIdx is an index on pTab.
**
** pOrderBy是SELECT命令中的一个ORDER BY子句.
** pTab是在相同的SELECT命令中的FROM子句中最左边的表。并且这个表有一个基本的游标数--"base".
** pIdx是在pTab上的一个索引
**
** nEqCol is the number of columns of pIdx that are used as equality
** constraints.  Any of these columns may be missing from the ORDER BY
** clause and the match can still be a success.
**
** nEqCol是被用于等式表达式中的pIdx的列数.任何的这些列都可能在ORDER BY子句中消失，但匹配依旧可以成功。
**
** All terms of the ORDER BY that match against the index must be either
** ASC or DESC.  (Terms of the ORDER BY clause past the end of a UNIQUE
** index do not need to satisfy this constraint.)  The *pbRev value is
** set to 1 if the ORDER BY clause is all DESC and it is set to 0 if
** the ORDER BY clause is all ASC.
**
** ORDER BY的所有terms匹配索引必须是ASC or DESC.(ORDER BY子句的terms过了UNIQUE索引末尾之后不需要满足这个约束)
** 如果ORDER BY子句是DESC，则*pbRev值设置为1，如果是ASC则设为0.
*/
/* 这个例程决定于p索引是否满足ORDER BY的子句。如果满足，则返回1，如果不满
** 足，则返回0。
**
** pOrderBy是SELECT语句中的一个ORDER BY子句形式。pTab是同一个SELECT语句中
** 的FROM子句的最左边一个表格，并且这个表格在库中有指针。p索引是pTab上的
** 一个索引。
**
** nEqCol是用作等式约束的p索引的容量数字。这些ORDER BY子句中的任意容量都有
** 可能丢失，但匹配仍然可以成功。
**
** 所有ORDER BY子句中不匹配索引的项都必须是ASC或者DESC码。(ORDER BY子句的
** 过去的唯一索引不需要满足这个约束。)如果ORDER BY子句全部是DESC码则*pbRev
** 的值设为1，如果ORDER BY子句全部是ASC码则*pbRev的值设为0。
*/
static int isSortingIndex(
<<<<<<< HEAD
	Parse *pParse,          /* Parsing context *//*解析上下文*/
	WhereMaskSet *pMaskSet, /* Mapping from table cursor numbers to bitmaps *//* 从表格指针映射到位图*/
	Index *pIdx,            /* The index we are testing *//* 正在测试的索引*/
	int base,               /* Cursor number for the table to be sorted *//* 排序用表的指针*/
	ExprList *pOrderBy,     /* The ORDER BY clause *//*ORDER BY子句*/
	int nEqCol,             /* Number of index columns with == constraints *//* 满足==约束条件的索引容量数字*/
	int wsFlags,            /* Index usages flags *//*索引使用标记*/
	int *pbRev              /* Set to 1 if ORDER BY is DESC *//* 如果ORDER BY子句全部是DESC码则设值为1*/
){
	int i, j;                       /* Loop counters *//* 循环计数器*/
	int sortOrder = 0;              /* XOR of index and ORDER BY sort direction *//* 索引中的或运算以及ORDER BY排序方向*/
  int nTerm;                      /* Number of ORDER BY terms *//* ORDER BY项的数字*/
  struct ExprList_item *pTerm;    /* A term of the ORDER BY clause *//* ORDER BY子句中的一个项*/
=======
  Parse *pParse,          /* Parsing context 分析上下文 */
  WhereMaskSet *pMaskSet, /* Mapping from table cursor numbers to bitmaps 表游标数到位图的映射 */
  Index *pIdx,            /* The index we are testing 我们测试的索引 */
  int base,               /* Cursor number for the table to be sorted 需要排序的表游标数 */
  ExprList *pOrderBy,     /* The ORDER BY clause ORDER BY子句 */
  int nEqCol,             /* Number of index columns with == constraints ==约束的索引列数 */
  int wsFlags,            /* Index usages flags 索引使用标志 */
  int *pbRev              /* Set to 1 if ORDER BY is DESC 如果ORDER BY是DESC则设为1 */
){
  int i, j;                       /* Loop counters 循环计数器 */
  int sortOrder = 0;              /* XOR of index and ORDER BY sort direction 索引的XOR和ORDER BY排序趋势 */
  int nTerm;                      /* Number of ORDER BY terms ORDER BY terms数 */
  struct ExprList_item *pTerm;    /* A term of the ORDER BY clause ORDER BY子句的一个term */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  sqlite3 *db = pParse->db;

  if( !pOrderBy ) return 0;
  if( wsFlags & WHERE_COLUMN_IN ) return 0;
  if( pIdx->bUnordered ) return 0;

  nTerm = pOrderBy->nExpr;
  assert( nTerm>0 );

  /* Argument pIdx must either point to a 'real' named index structure, 
  ** or an index structure allocated on the stack by bestBtreeIndex() to
<<<<<<< HEAD
  ** represent the rowid index that is part of every table.  */
  /* p索引参数必须或者指向一个“真实的”命名索引结构，或者一个分配到
  ** bestBtreeIndex栈的索引结构，来表示rowid指数是每个表中的一部分。
  */
=======
  ** represent the rowid index that is part of every table.  
  ** 参数pIdx必须指向一个'real'命名的索引数据结构
  ** 或者在堆栈中由bestBtreeIndex()分配一个索引数据结构用于表示每个表的rowid索引这个部分 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  assert( pIdx->zName || (pIdx->nColumn==1 && pIdx->aiColumn[0]==-1) );

  /* Match terms of the ORDER BY clause against columns of
  ** the index.
  **
  ** 与ORDER BY子句terms相匹配的索引列
  **
  ** Note that indices have pIdx->nColumn regular columns plus
  ** one additional column containing the rowid.  The rowid column
  ** of the index is also allowed to match against the ORDER BY
  ** clause.
  ** 注意:索引有pIdx->nColumn规则列加上一个包含rowid的附加列.
  ** 索引rowid列也是能够与ORDER BY子句相匹配
  */
  /* 匹配与索引中的列不同的ORDER BY子句中的项。
  **
  ** 注：指数pIdx - > nColumn常规列加一个额外的列包含rowid。rowid
  ** 列的索引也允许匹配ORDER BY子句。
  */
  for(i=j=0, pTerm=pOrderBy->a; j<nTerm && i<=pIdx->nColumn; i++){
<<<<<<< HEAD
	  Expr *pExpr;       /* The expression of the ORDER BY pTerm *//* ORDER BY子句中的表达式pTerm*/
	  CollSeq *pColl;    /* The collating sequence of pExpr *//* 排序序列pExpr*/
	  int termSortOrder; /* Sort order for this term *//*对此项进行排序*/
	  int iColumn;       /* The i-th column of the index.  -1 for rowid *//* 索引中的第i列*/
	  int iSortOrder;    /* 1 for DESC, 0 for ASC on the i-th index term *//* 第i个索引项中，DESC码为1，ASC码为0*/
	  const char *zColl; /* Name of the collating sequence for i-th index term *//* 第i个索引项中排序序列的名称*/
=======
    Expr *pExpr;       /* The expression of the ORDER BY pTerm ORDER BY表达式pTerm */
    CollSeq *pColl;    /* The collating sequence of pExpr pExpr的排序序列 */
    int termSortOrder; /* Sort order for this term 这个term的排序次序 */
    int iColumn;       /* The i-th column of the index.  -1 for rowid 索引的i-th列 */
    int iSortOrder;    /* 1 for DESC, 0 for ASC on the i-th index term 在i-th索引term中，0表示ASC，1表示DESC */
    const char *zColl; /* Name of the collating sequence for i-th index term i-th索引term的排序序列名 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949

    pExpr = pTerm->pExpr;
    if( pExpr->op!=TK_COLUMN || pExpr->iTable!=base ){
      /* Can not use an index sort on anything that is not a column in the
<<<<<<< HEAD
      ** left-most table of the FROM clause */
	  /*  如果不是FROM子句中最左边表格的一个列则不能对任意项使用索引排序。
	  */
=======
      ** left-most table of the FROM clause
      ** 不能在不是FROM子句中的最左边的表的一列上使用一个索引排序 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
      break;
    }
    pColl = sqlite3ExprCollSeq(pParse, pExpr);
    if( !pColl ){
      pColl = db->pDfltColl;
    }
    if( pIdx->zName && i<pIdx->nColumn ){
      iColumn = pIdx->aiColumn[i];
      if( iColumn==pIdx->pTable->iPKey ){
        iColumn = -1;
      }
      iSortOrder = pIdx->aSortOrder[i];
      zColl = pIdx->azColl[i];
    }else{
      iColumn = -1;
      iSortOrder = 0;
      zColl = pColl->zName;
    }
    if( pExpr->iColumn!=iColumn || sqlite3StrICmp(pColl->zName, zColl) ){
<<<<<<< HEAD
      /* Term j of the ORDER BY clause does not match column i of the index */
      /* ORDER BY子句中的j项不匹配索引中的i列。
	  */
=======
      /* Term j of the ORDER BY clause does not match column i of the index ORDER BY子句的Term j不匹配索引列i */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
      if( i<nEqCol ){
        /* If an index column that is constrained by == fails to match an
        ** ORDER BY term, that is OK.  Just ignore that column of the index
        ** 如果一个由==约束的索引列不匹配一个ORDER BYterm，是可以的。只要忽略这个索引列
        */
		/* 如果被==所约束的索引列不能匹配ORDER BY项，那么这也是可以的，只需要
		** 忽略这个索引。
		*/
        continue;
      }else if( i==pIdx->nColumn ){
<<<<<<< HEAD
        /* Index column i is the rowid.  All other terms match. */
		/* 索引列i是rowid变量，所有其他项匹配。
		*/
=======
        /* Index column i is the rowid.  All other terms match. 索引列i是rowid.所有其他terms匹配 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
        break;
      }else{
        /* If an index column fails to match and is not constrained by ==
        ** then the index cannot satisfy the ORDER BY constraint.
        ** 如果一个索引列未能匹配并且不是由==约束的，那么索引不能适用于ORDER BY约束
        */
	    /* 如果一个索引列不匹配，并且没有==约束条件，则这个索引不满足ORDER
		** BY约束条件。
		*/
        return 0;
      }
    }
    assert( pIdx->aSortOrder!=0 || iColumn==-1 );
    assert( pTerm->sortOrder==0 || pTerm->sortOrder==1 );
    assert( iSortOrder==0 || iSortOrder==1 );
    termSortOrder = iSortOrder ^ pTerm->sortOrder;
    if( i>nEqCol ){
      if( termSortOrder!=sortOrder ){
        /* Indices can only be used if all ORDER BY terms past the 索引只能在所有的ORDER BYterms过去的等式约束是DESC或ASC是才被使用
        ** equality constraints are all either DESC or ASC. */
		  /* 只有ORDER BY子句满足等式约束条件的项都是DESC码或者ASC码
		  ** 才可以使用指数。
		  */
        return 0;
      }
    }else{
      sortOrder = termSortOrder;
    }
    j++;
    pTerm++;
    if( iColumn<0 && !referencesOtherTables(pOrderBy, pMaskSet, j, base) ){
      /* If the indexed column is the primary key and everything matches
      ** so far and none of the ORDER BY terms to the right reference other
      ** tables in the join, then we are assured that the index can be used 
      ** to sort because the primary key is unique and so none of the other
      ** columns will make any difference
      **
      ** 如果索引列是主键并且到目前为止都匹配并且没有ORDER BY terms与连接的其他表右相关，
      ** 那么我们保证索引可以被排序所使用，因为主键是唯一的所以其他的列没有任何影响
      */
	  /* 如果索引列是主键，并且目前为止所有项都匹配，并且ORDER BY到右边的项
	  ** 在联合中指向其他的表，那么可以保证索引可以被用来排序，因为主键是唯
	  ** 一的而且没有其他的列会起作用。
	  */
      j = nTerm;
    }
  }

  *pbRev = sortOrder!=0;
  if( j>=nTerm ){
    /* All terms of the ORDER BY clause are covered by this index so ORDER BY子句的所有terms被这个索引覆盖，所以这个索引可以用于排序
    ** this index can be used for sorting. */
	  /* 所有ORDER BY子句中的项都被这个这个索引包含，所以这个索引可以
	  ** 用来排序。
	  */
    return 1;
  }
  if( pIdx->onError!=OE_None && i==pIdx->nColumn
      && (wsFlags & WHERE_COLUMN_NULL)==0
      && !referencesOtherTables(pOrderBy, pMaskSet, j, base) 
  ){
    Column *aCol = pIdx->pTable->aCol;

    /* All terms of this index match some prefix of the ORDER BY clause,
    ** the index is UNIQUE, and no terms on the tail of the ORDER BY
    ** refer to other tables in a join. So, assuming that the index entries
    ** visited contain no NULL values, then this index delivers rows in
    ** the required order.
    **
    ** 这个索引的所有terms与一些ORDER BY子句前缀相匹配，索引是UNIQUE,
    ** 并且在ORDER BY尾部没有terms与连接中的其他表相关。
    ** 所以，假定索引项访问包含无NULL值，那么这个索引提供行所需的顺序
    **
    ** It is not possible for any of the first nEqCol index fields to be
    ** NULL (since the corresponding "=" operator in the WHERE clause would 
    ** not be true). So if all remaining index columns have NOT NULL 
    ** constaints attached to them, we can be confident that the visited
<<<<<<< HEAD
    ** index entries are free of NULLs.  */
	/* 这个索引中的所有项都满足ORDER BY子句的一些前缀、索引是唯一的、没有
	** ORDER BY子句中尾部的项指向其他的表。所以，假设索引条目访问不包含NULL
	** 值,那么这个索引提供行所需的顺序。
	*/
=======
    ** index entries are free of NULLs.  
    **
    ** 任何的第一个nEqCol索引字段为NULL值是不可能的(因为在WHERE子句中相应"="运算符不会正确)。
    ** 所以，如果所有身下的索引列有NOT NULL约束，我们可以确信索引项是没有NULL值的
    */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    for(i=nEqCol; i<pIdx->nColumn; i++){
      if( aCol[pIdx->aiColumn[i]].notNull==0 ) break;
    }
    return (i==pIdx->nColumn);
  }
  return 0;
}

/*
** Prepare a crude estimate of the logarithm of the input value.
** The results need not be exact.  This is only used for estimating
** the total cost of performing operations with O(logN) or O(NlogN)
** complexity.  Because N is just a guess, it is no great tragedy if
** logN is a little off.
**
** 粗糙的估计一个输入值的对数，结果不需要确切地。
** 这只是用于评价执行复杂度为O(logN)或O(NlogN)的操作的总成本。
*/
/* 准备一个输入值的对数的粗略估计。结果不需要很精确。这仅仅是用于估
** 计执行操作的总代价与O(logN)或O(NlogN)复杂性。因为N仅仅是一个猜测值
** 即使logN有些误差也问题不大。
*/
static double estLog(double N){
  double logN = 1;
  double x = 10;
  while( N>x ){
    logN += 1;
    x *= 10;
  }
  return logN;
}

/*
** Two routines for printing the content of an sqlite3_index_info
** structure.  Used for testing and debugging only.  If neither
** SQLITE_TEST or SQLITE_DEBUG are defined, then these routines
** are no-ops.
**
** 两个用于输出一个sqlite3_index_info数据结构的程序。
** 只是用于测试和调试。如果SQLITE_TEST和SQLITE_DEBUG都没定义，那么这些程序是no-ops(无操作)。
**
*/
/* sqlite3索引信息结构中用于打印目录的两个例程。只用于测试和调试。
** 如果SQLITE_TEST和SQLITE_DEBUG都被定义了，则这两个例程执行空操作。
*/
#if !defined(SQLITE_OMIT_VIRTUALTABLE) && defined(SQLITE_DEBUG)
static void TRACE_IDX_INPUTS(sqlite3_index_info *p){
  int i;
  if( !sqlite3WhereTrace ) return;
  for(i=0; i<p->nConstraint; i++){
    sqlite3DebugPrintf("  constraint[%d]: col=%d termid=%d op=%d usabled=%d\n",
       i,
       p->aConstraint[i].iColumn,
       p->aConstraint[i].iTermOffset,
       p->aConstraint[i].op,
       p->aConstraint[i].usable);
  }
  for(i=0; i<p->nOrderBy; i++){
    sqlite3DebugPrintf("  orderby[%d]: col=%d desc=%d\n",
       i,
       p->aOrderBy[i].iColumn,
       p->aOrderBy[i].desc);
  }
}
static void TRACE_IDX_OUTPUTS(sqlite3_index_info *p){
  int i;
  if( !sqlite3WhereTrace ) return;
  for(i=0; i<p->nConstraint; i++){
    sqlite3DebugPrintf("  usage[%d]: argvIdx=%d omit=%d\n",
       i,
       p->aConstraintUsage[i].argvIndex,
       p->aConstraintUsage[i].omit);
  }
  sqlite3DebugPrintf("  idxNum=%d\n", p->idxNum);
  sqlite3DebugPrintf("  idxStr=%s\n", p->idxStr);
  sqlite3DebugPrintf("  orderByConsumed=%d\n", p->orderByConsumed);
  sqlite3DebugPrintf("  estimatedCost=%g\n", p->estimatedCost);
}
#else
#define TRACE_IDX_INPUTS(A)
#define TRACE_IDX_OUTPUTS(A)
#endif

/* 
** Required because bestIndex() is called by bestOrClauseIndex() 
** 因为bestIndex()被bestOrClauseIndex()调用而被请求
*/
/* 以下是需要的，因为bstIndex()被bestOrClauseIndex()调用。
*/
static void bestIndex(
    Parse*, WhereClause*, struct SrcList_item*,
    Bitmask, Bitmask, ExprList*, WhereCost*);

/*
** This routine attempts to find an scanning strategy that can be used 
** to optimize an 'OR' expression that is part of a WHERE clause. 
**
** 这个程序企图发现一个能用来优化一个WHERE子句中的一个'OR'表达式的扫描策略
**
** The table associated with FROM clause term pSrc may be either a
** regular B-Tree table or a virtual table.
**
** 与FROM子句term pSrc有关的表可能是一个规则的B-Tree表或一个虚拟表
*/
/* 此例程用于找到一个扫描策略，这个策略可以用来优化一个WHERE子句中的
** “或”表达式。
**
** 与子句FROM中的pScr项相关的表可能是个普通的B树表或者是一个虚表。
*/
static void bestOrClauseIndex(
<<<<<<< HEAD
	Parse *pParse,              /* The parsing context *//* 解析文档*/
	WhereClause *pWC,           /* The WHERE clause *//* WHERE子句*/
struct SrcList_item *pSrc,  /* The FROM clause term to search *//* 用于搜索的FROM子句项*/
	Bitmask notReady,           /* Mask of cursors not available for indexing *//* 不能用来索引的游标掩码*/
	Bitmask notValid,           /* Cursors not available for any purpose *//* 不用于任何目的的光标*/
  ExprList *pOrderBy,         /* The ORDER BY clause *//* ORDER BY子句*/
  WhereCost *pCost            /* Lowest cost query plan *//* 最小化查询计划的代价*/
){
#ifndef SQLITE_OMIT_OR_OPTIMIZATION
	const int iCur = pSrc->iCursor;   /* The cursor of the table to be accessed *//* 可使用的表格光标*/
	const Bitmask maskSrc = getMask(pWC->pMaskSet, iCur);  /* Bitmask for pSrc *//* pSrc的位掩码*/
	WhereTerm * const pWCEnd = &pWC->a[pWC->nTerm];        /* End of pWC->a[] *//* pWC->a[]结束*/
	WhereTerm *pTerm;                 /* A single term of the WHERE clause *//* WHERE子句的一个单一项*/

  /* The OR-clause optimization is disallowed if the INDEXED BY or
  ** NOT INDEXED clauses are used or if the WHERE_AND_ONLY bit is set. */
  /* 如果INDEXED BY子句或者NOT INDEXED子句已经设置了WHERE_AND_ONLY位，那么
  ** OR子句的优化无效。
=======
  Parse *pParse,              /* The parsing context 解析上下文 */
  WhereClause *pWC,           /* The WHERE clause WHERE子句 */
  struct SrcList_item *pSrc,  /* The FROM clause term to search 要搜索的FROM子句term */
  Bitmask notReady,           /* Mask of cursors not available for indexing 游标的掩码对于索引无效 */
  Bitmask notValid,           /* Cursors not available for any purpose 游标在任何用途下都无效 */
  ExprList *pOrderBy,         /* The ORDER BY clause ORDER BY子句 */
  WhereCost *pCost            /* Lowest cost query plan 最小代价的查询计划 */
){
#ifndef SQLITE_OMIT_OR_OPTIMIZATION
  const int iCur = pSrc->iCursor;   /* The cursor of the table to be accessed 需要存取的表游标 */
  const Bitmask maskSrc = getMask(pWC->pMaskSet, iCur);  /* Bitmask for pSrc pSrc的位掩码 */
  WhereTerm * const pWCEnd = &pWC->a[pWC->nTerm];        /* End of pWC->a[] pWC->a[]的末尾 */
  WhereTerm *pTerm;                 /* A single term of the WHERE clause WHERE子句的一个单独term */

  /* The OR-clause optimization is disallowed if the INDEXED BY or
  ** NOT INDEXED clauses are used or if the WHERE_AND_ONLY bit is set.
  ** 如果使用INDEXED BY或NOT INDEXED子句或设置了WHERE_AND_ONLY bit，那么OR子句是不允许优化的
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  if( pSrc->notIndexed || pSrc->pIndex!=0 ){
    return;
  }
  if( pWC->wctrlFlags & WHERE_AND_ONLY ){
    return;
  }

<<<<<<< HEAD
  /* Search the WHERE clause terms for a usable WO_OR term. *//* 为一个不可用的WO_OR项搜索WHERE子句项*/
=======
  /* Search the WHERE clause terms for a usable WO_OR term. 查找WHERE子句terms */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  for(pTerm=pWC->a; pTerm<pWCEnd; pTerm++){
    if( pTerm->eOperator==WO_OR 
     && ((pTerm->prereqAll & ~maskSrc) & notReady)==0
     && (pTerm->u.pOrInfo->indexable & maskSrc)!=0 
    ){
      WhereClause * const pOrWC = &pTerm->u.pOrInfo->wc;
      WhereTerm * const pOrWCEnd = &pOrWC->a[pOrWC->nTerm];
      WhereTerm *pOrTerm;
      int flags = WHERE_MULTI_OR;
      double rTotal = 0;
      double nRow = 0;
      Bitmask used = 0;

      for(pOrTerm=pOrWC->a; pOrTerm<pOrWCEnd; pOrTerm++){
        WhereCost sTermCost;
        WHERETRACE(("... Multi-index OR testing for term %d of %d....\n", 
          (pOrTerm - pOrWC->a), (pTerm - pWC->a)
        ));
        if( pOrTerm->eOperator==WO_AND ){
          WhereClause *pAndWC = &pOrTerm->u.pAndInfo->wc;
          bestIndex(pParse, pAndWC, pSrc, notReady, notValid, 0, &sTermCost);
        }else if( pOrTerm->leftCursor==iCur ){
          WhereClause tempWC;
          tempWC.pParse = pWC->pParse;
          tempWC.pMaskSet = pWC->pMaskSet;
          tempWC.pOuter = pWC;
          tempWC.op = TK_AND;
          tempWC.a = pOrTerm;
          tempWC.wctrlFlags = 0;
          tempWC.nTerm = 1;
          bestIndex(pParse, &tempWC, pSrc, notReady, notValid, 0, &sTermCost);
        }else{
          continue;
        }
        rTotal += sTermCost.rCost;
        nRow += sTermCost.plan.nRow;
        used |= sTermCost.used;
        if( rTotal>=pCost->rCost ) break;
      }

      /* If there is an ORDER BY clause, increase the scan cost to account 
      ** for the cost of the sort. */
	  /* 如果存在一个ORDER BY子句，则为此次排序的代价账户增加一次浏览代价。
	  */
      if( pOrderBy!=0 ){
        WHERETRACE(("... sorting increases OR cost %.9g to %.9g\n",
                    rTotal, rTotal+nRow*estLog(nRow)));
        rTotal += nRow*estLog(nRow);
      }

      /* If the cost of scanning using this OR term for optimization is
      ** less than the current cost stored in pCost, replace the contents
<<<<<<< HEAD
      ** of pCost. */
	  /* 如果用于优化而使用此次OR项的浏览代价少于pCost中当前的排序代价，
	  ** 则替换pCost中的内容。
	  */
=======
      ** of pCost. 
      ** 如果使用优化的ORterm的扫描代价比存储在pCost的当前代价更少，替换pCost的内容
      */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
      WHERETRACE(("... multi-index OR cost=%.9g nrow=%.9g\n", rTotal, nRow));
      if( rTotal<pCost->rCost ){
        pCost->rCost = rTotal;
        pCost->used = used;
        pCost->plan.nRow = nRow;
        pCost->plan.wsFlags = flags;
        pCost->plan.u.pTerm = pTerm;
      }
    }
  }
#endif /* SQLITE_OMIT_OR_OPTIMIZATION *//* SQLITE_OMIT_OR_OPTIMIZATION结束*/
}

#ifndef SQLITE_OMIT_AUTOMATIC_INDEX
/*
** Return TRUE if the WHERE clause term pTerm is of a form where it
** could be used with an index to access pSrc, assuming an appropriate
** index existed.
**
** 如果WHERE子句term pTerm是这样一种形式，可以与一个索引一起使用来访问pSrc，假设存在一个适当的索引，则返回TRUE
**
*/
/* 如果WHERE子句项pTerm是可以用于和一个索引共同链接到pSrc的一种形式则
** 返回真，假设一个合适的索引是存在的。
*/
static int termCanDriveIndex(
<<<<<<< HEAD
	WhereTerm *pTerm,              /* WHERE clause term to check *//* 用于检查的WHERE子句项*/
struct SrcList_item *pSrc,     /* Table we are trying to access *//* 尝试登陆的表格*/
	Bitmask notReady               /* Tables in outer loops of the join *//* 连接的外部循环表格*/
=======
  WhereTerm *pTerm,              /* WHERE clause term to check 需要检测的WHERE子句 */
  struct SrcList_item *pSrc,     /* Table we are trying to access 我们试图访问的表 */
  Bitmask notReady               /* Tables in outer loops of the join 在连接的外部循环的表 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
){
  char aff;
  if( pTerm->leftCursor!=pSrc->iCursor ) return 0;
  if( pTerm->eOperator!=WO_EQ ) return 0;
  if( (pTerm->prereqRight & notReady)!=0 ) return 0;
  aff = pSrc->pTab->aCol[pTerm->u.leftColumn].affinity;
  if( !sqlite3IndexAffinityOk(pTerm->pExpr, aff) ) return 0;
  return 1;
}
#endif

#ifndef SQLITE_OMIT_AUTOMATIC_INDEX
/*
** If the query plan for pSrc specified in pCost is a full table scan
** and indexing is allows (if there is no NOT INDEXED clause) and it
** possible to construct a transient index that would perform better
** than a full table scan even when the cost of constructing the index
** is taken into account, then alter the query plan to use the
** transient index.
**
** 如果在pCost中指定的pSrc的查询计划是一个全表扫描并且可以使用索引(如果没有INDEXED子句)
** 并且他可能创建一个临时的索引，即使当创建索引的代价也被考虑进去依旧比全表扫描更好，
** 那么改变查询计划，使用临时索引。
*/
/* 如果pCost中pSrc详细说明的查询计划是一个满表查询并且索引是允许的
** （如果没有NOT INDEXED子句存在）并且可以构建一个瞬态指数,将执行比全
** 表扫描即使构建索引的代价考虑进去,然后改变查询计划使用瞬态指数。
*/
static void bestAutomaticIndex(
<<<<<<< HEAD
	Parse *pParse,              /* The parsing context *//* 解析文档*/
	WhereClause *pWC,           /* The WHERE clause *//* WHERE子句*/
struct SrcList_item *pSrc,  /* The FROM clause term to search *//* 用于搜索的FROM子句项*/
	Bitmask notReady,           /* Mask of cursors that are not available *//* 不能用来索引的游标掩码*/
	WhereCost *pCost            /* Lowest cost query plan *//* 最小化查询计划的代价*/
){
	double nTableRow;           /* Rows in the input table *//* 输入表的行*/
	double logN;                /* log(nTableRow) */
	double costTempIdx;         /* per-query cost of the transient index *//* 瞬态指数的每个查询代价*/
	WhereTerm *pTerm;           /* A single term of the WHERE clause *//* 一个WHERE子句的单一项*/
	WhereTerm *pWCEnd;          /* End of pWC->a[] *//*pWC->a[]结束*/
	Table *pTable;              /* Table tht might be indexed *//* tht表可能被索引了*/

  if( pParse->nQueryLoop<=(double)1 ){
    /* There is no point in building an automatic index for a single scan */
    /* 为单词扫描建立一个自动索引是没有意义的。
    */
    return;
  }
  if( (pParse->db->flags & SQLITE_AutoIndex)==0 ){
    /* Automatic indices are disabled at run-time */
	  /* 在运行时自动指数是禁用的。
	  */
    return;
  }
  if( (pCost->plan.wsFlags & WHERE_NOT_FULLSCAN)!=0 ){
    /* We already have some kind of index in use for this query. */
	  /* 这类查询中我们已经有了一些在使用中的索引。
	  */
    return;
  }
  if( pSrc->notIndexed ){
    /* The NOT INDEXED clause appears in the SQL. */
	  /* SQL中出现的NOT INDEXED子句。
	  */
    return;
  }
  if( pSrc->isCorrelated ){
    /* The source is a correlated sub-query. No point in indexing it. */
	  /* 这个源是一个相关子查询，对其索引是没有意义的。
	  */
=======
  Parse *pParse,              /* The parsing context 解析上下文 */
  WhereClause *pWC,           /* The WHERE clause WHERE子句 */
  struct SrcList_item *pSrc,  /* The FROM clause term to search 用于查询的FROM子句term */
  Bitmask notReady,           /* Mask of cursors that are not available 游标的掩码是无效的 */
  WhereCost *pCost            /* Lowest cost query plan 最小代价查询计划 */
){
  double nTableRow;           /* Rows in the input table 在输入表中的行 */
  double logN;                /* log(nTableRow) */
  double costTempIdx;         /* per-query cost of the transient index 临时索引的per-query代价 */
  WhereTerm *pTerm;           /* A single term of the WHERE clause WHERE子句的一个单独term */
  WhereTerm *pWCEnd;          /* End of pWC->a[] pWC->a[]的末尾 */
  Table *pTable;              /* Table tht might be indexed 可能有索引的表tht */

  if( pParse->nQueryLoop<=(double)1 ){
    /* There is no point in building an automatic index for a single scan 为一个单一的扫描构建一个自动索引是不必要的 */
    return;
  }
  if( (pParse->db->flags & SQLITE_AutoIndex)==0 ){
    /* Automatic indices are disabled at run-time 在运行时间内自动索引将无效的 */
    return;
  }
  if( (pCost->plan.wsFlags & WHERE_NOT_FULLSCAN)!=0 ){
    /* We already have some kind of index in use for this query. 在使用这个查询时已经有不错的索引 */
    return;
  }
  if( pSrc->notIndexed ){
    /* The NOT INDEXED clause appears in the SQL. 在SQL中出现NOT INDEXED子句 */
    return;
  }
  if( pSrc->isCorrelated ){
    /* The source is a correlated sub-query. No point in indexing it. 来源是一个有关联的子查询。不需要使用索引 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    return;
  }

  assert( pParse->nQueryLoop >= (double)1 );
  pTable = pSrc->pTab;
  nTableRow = pTable->nRowEst;
  logN = estLog(nTableRow); //评价执行复杂度
  costTempIdx = 2*logN*(nTableRow/pParse->nQueryLoop + 1); //临时索引的代价
  if( costTempIdx>=pCost->rCost ){//创建临时表的代价大于全表扫描的代价
    /* The cost of creating the transient table would be greater than 创建临时表的代价大于全表扫描的代价
    ** doing the full table scan */
	  /* 创建临时表的代价会比全表扫描高。
	  */
    return;
  }

<<<<<<< HEAD
  /* Search for any equality comparison term */
  /* 搜索任意的等值比较项。
  */
=======
  /* Search for any equality comparison term 查找任何等式比较的term */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  pWCEnd = &pWC->a[pWC->nTerm];
  for(pTerm=pWC->a; pTerm<pWCEnd; pTerm++){ //循环遍历where子句中的每个term
    if( termCanDriveIndex(pTerm, pSrc, notReady) ){	//如果term可以使用索引
      WHERETRACE(("auto-index reduces cost from %.1f to %.1f\n",
                    pCost->rCost, costTempIdx));
      pCost->rCost = costTempIdx;
      pCost->plan.nRow = logN + 1;
      pCost->plan.wsFlags = WHERE_TEMP_INDEX;
      pCost->used = pTerm->prereqRight;
      break;
    }
  }
}
#else
# define bestAutomaticIndex(A,B,C,D,E)  /* no-op *//* 空操作*/
#endif /* SQLITE_OMIT_AUTOMATIC_INDEX *//* SQLITE_OMIT_AUTOMATIC_INDEX结束。*/


#ifndef SQLITE_OMIT_AUTOMATIC_INDEX
/*
** Generate code to construct the Index object for an automatic index
** and to set up the WhereLevel object pLevel so that the code generator
** makes use of the automatic index.
**
** 生成代码来创建索引对象用于自动索引并且用于设置WhereLevel对象pLevel以便代码生成器使用自动索引
*/
/* 为索引对象生成代码构造自动索引，并且设置WhereLevel对象pLevel，这样代
** 码生成器可以利用自动索引。
*/
static void constructAutomaticIndex(
<<<<<<< HEAD
	Parse *pParse,              /* The parsing context *//* 解析文档*/
	WhereClause *pWC,           /* The WHERE clause *//* WHERE子句*/
struct SrcList_item *pSrc,  /* The FROM clause term to get the next index *//* 用于连接下一个索引的FROM子句项*/
  Bitmask notReady,           /* Mask of cursors that are not available *//* 不能用来索引的游标掩码*/
  WhereLevel *pLevel          /* Write new index here *//* 在此写新索引*/
){
	int nColumn;                /* Number of columns in the constructed index *//* 建立索引的列数*/
	WhereTerm *pTerm;           /* A single term of the WHERE clause *//* WHERE子句的一个单一项*/
	WhereTerm *pWCEnd;          /* End of pWC->a[] *//*pWC->a[]结束*/
	int nByte;                  /* Byte of memory needed for pIdx *//* pIdx所需的内存大小*/
	Index *pIdx;                /* Object describing the transient index *//* 描述瞬态索引的目标*/
	Vdbe *v;                    /* Prepared statement under construction *//* 准备好处于建立当中的声明*/
	int addrInit;               /* Address of the initialization bypass jump *//* 跳开初始化的地址*/
	Table *pTable;              /* The table being indexed *//* 正被索引的表*/
	KeyInfo *pKeyinfo;          /* Key information for the index */   /* 索引中的关键信息*/
	int addrTop;                /* Top of the index fill loop *//* 索引填充循环的顶部*/
	int regRecord;              /* Register holding an index record *//* 注册保留一个索引记录*/
	int n;                      /* Column counter *//* 列数计数器*/
	int i;                      /* Loop counter *//* 循环计数器*/
	int mxBitCol;               /* Maximum column in pSrc->colUsed *//* pSrc-》colUsed的最大列数*/
	CollSeq *pColl;             /* Collating sequence to on a column *//* 按列排序*/
	Bitmask idxCols;            /* Bitmap of columns used for indexing *//* 用于索引的列的位图*/
	Bitmask extraCols;          /* Bitmap of additional columns *//* 添加列的位图*/

  /* Generate code to skip over the creation and initialization of the
  ** transient index on 2nd and subsequent iterations of the loop. */
	/* 生成代码来跳过循环第二个及之后的迭代中瞬态索引的创建和初始化。
	*/
=======
  Parse *pParse,              /* The parsing context 解析上下文 */
  WhereClause *pWC,           /* The WHERE clause WHERE子句 */
  struct SrcList_item *pSrc,  /* The FROM clause term to get the next index FROM子句term为了得到下一个索引 */
  Bitmask notReady,           /* Mask of cursors that are not available 游标的掩码是无效的 */
  WhereLevel *pLevel          /* Write new index here 写入新的索引 */
){
  int nColumn;                /* Number of columns in the constructed index 在构造的索引中的列数 */
  WhereTerm *pTerm;           /* A single term of the WHERE clause WHERE子句的一个单一的term */
  WhereTerm *pWCEnd;          /* End of pWC->a[] pWC->a[]的末尾 */
  int nByte;                  /* Byte of memory needed for pIdx pIdx需要的内存字节 */
  Index *pIdx;                /* Object describing the transient index 描述临时索引的对象 */
  Vdbe *v;                    /* Prepared statement under construction 在建造中准备好的命令 */
  int addrInit;               /* Address of the initialization bypass jump 初始化地址忽略跳过 */
  Table *pTable;              /* The table being indexed 有索引的表 */
  KeyInfo *pKeyinfo;          /* Key information for the index 索引的关键信息 */   
  int addrTop;                /* Top of the index fill loop 填充循环的索引顶部 */
  int regRecord;              /* Register holding an index record 记录保存一个索引记录 */
  int n;                      /* Column counter 列计数器 */
  int i;                      /* Loop counter 循环计数器 */
  int mxBitCol;               /* Maximum column in pSrc->colUsed 在pSrc->colUsed中的最大的列 */
  CollSeq *pColl;             /* Collating sequence to on a column 在一个列中的排序序列 */
  Bitmask idxCols;            /* Bitmap of columns used for indexing 用于索引的列的位掩码 */
  Bitmask extraCols;          /* Bitmap of additional columns 附加列的位掩码 */

  /* Generate code to skip over the creation and initialization of the
  ** transient index on 2nd and subsequent iterations of the loop. 
  ** 生成代码用于跳过在循环的2nd和连续迭代时的临时索引的创建和初始化
  */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  v = pParse->pVdbe;
  assert( v!=0 );
  addrInit = sqlite3CodeOnce(pParse);

  /* Count the number of columns that will be added to the index 计算将要添加到索引的列数和用于匹配WHERE子句的约束
  ** and used to match WHERE clause constraints */
  /* 对将要加入索引和用于满足WHERE子句条件的列数计数。
  */
  nColumn = 0;
  pTable = pSrc->pTab;
  pWCEnd = &pWC->a[pWC->nTerm];
  idxCols = 0;
  for(pTerm=pWC->a; pTerm<pWCEnd; pTerm++){
    if( termCanDriveIndex(pTerm, pSrc, notReady) ){
      int iCol = pTerm->u.leftColumn;
      Bitmask cMask = iCol>=BMS ? ((Bitmask)1)<<(BMS-1) : ((Bitmask)1)<<iCol;
      testcase( iCol==BMS );
      testcase( iCol==BMS-1 );
      if( (idxCols & cMask)==0 ){
        nColumn++;
        idxCols |= cMask;
      }
    }
  }
  assert( nColumn>0 );
  pLevel->plan.nEq = nColumn;

  /* Count the number of additional columns needed to create a
  ** covering index.  A "covering index" is an index that contains all
  ** columns that are needed by the query.  With a covering index, the
  ** original table never needs to be accessed.  Automatic indices must
  ** be a covering index because the index will not be updated if the
  ** original table changes and the index and table cannot both be used
  ** if they go out of sync.
  **
  ** 计算需要创建一个覆盖索引的附加列的数值。一个"覆盖索引"是一个包含所有被查询的列的索引。
  ** 有了一个覆盖索引，原始表将不需要再被访问。
  ** 因为如果原始表变化,索引将不会更新，自动索引必需是一个覆盖索引，并且如果索引和表不同步的话，他们都将不会被使用。
  **
  */
  /* 计算额外的列的数量需要创建一个覆盖索引。“覆盖索引”是指一个包含所
  ** 有查询所需的列的索引。有了覆盖索引，那么原始表就不需要被访问。自动
  ** 索引必须是一个覆盖索引，因为如果它们被用于同步的话，原始表格就会变
  ** 化，表也同时不能被使用，那么这个索引就不能被更新。
  */
  extraCols = pSrc->colUsed & (~idxCols | (((Bitmask)1)<<(BMS-1)));
  mxBitCol = (pTable->nCol >= BMS-1) ? BMS-1 : pTable->nCol;
  testcase( pTable->nCol==BMS-1 );
  testcase( pTable->nCol==BMS-2 );
  for(i=0; i<mxBitCol; i++){
    if( extraCols & (((Bitmask)1)<<i) ) nColumn++;
  }
  if( pSrc->colUsed & (((Bitmask)1)<<(BMS-1)) ){
    nColumn += pTable->nCol - BMS + 1;
  }
  pLevel->plan.wsFlags |= WHERE_COLUMN_EQ | WHERE_IDX_ONLY | WO_EQ;

<<<<<<< HEAD
  /* Construct the Index object to describe this index */
  /* 构建索引对象来描述这个索引。
  */
=======
  /* Construct the Index object to describe this index 创建索引对象来描述这个索引 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  nByte = sizeof(Index);
  nByte += nColumn*sizeof(int);     /* Index.aiColumn */
  nByte += nColumn*sizeof(char*);   /* Index.azColl */
  nByte += nColumn;                 /* Index.aSortOrder */
  pIdx = sqlite3DbMallocZero(pParse->db, nByte);
  if( pIdx==0 ) return;
  pLevel->plan.u.pIdx = pIdx;
  pIdx->azColl = (char**)&pIdx[1];
  pIdx->aiColumn = (int*)&pIdx->azColl[nColumn];
  pIdx->aSortOrder = (u8*)&pIdx->aiColumn[nColumn];
  pIdx->zName = "auto-index";
  pIdx->nColumn = nColumn;
  pIdx->pTable = pTable;
  n = 0;
  idxCols = 0;
  for(pTerm=pWC->a; pTerm<pWCEnd; pTerm++){
    if( termCanDriveIndex(pTerm, pSrc, notReady) ){
      int iCol = pTerm->u.leftColumn;
      Bitmask cMask = iCol>=BMS ? ((Bitmask)1)<<(BMS-1) : ((Bitmask)1)<<iCol;
      if( (idxCols & cMask)==0 ){
        Expr *pX = pTerm->pExpr;
        idxCols |= cMask;
        pIdx->aiColumn[n] = pTerm->u.leftColumn;
        pColl = sqlite3BinaryCompareCollSeq(pParse, pX->pLeft, pX->pRight);
        pIdx->azColl[n] = ALWAYS(pColl) ? pColl->zName : "BINARY";
        n++;
      }
    }
  }
  assert( (u32)n==pLevel->plan.nEq );

  /* Add additional columns needed to make the automatic index into 需要添加附加列使自动索引变为覆盖索引
  ** a covering index */
  /* 添加一个额外但必须的列来保证自动索引是一个覆盖索引。
  */
  for(i=0; i<mxBitCol; i++){
    if( extraCols & (((Bitmask)1)<<i) ){
      pIdx->aiColumn[n] = i;
      pIdx->azColl[n] = "BINARY";
      n++;
    }
  }
  if( pSrc->colUsed & (((Bitmask)1)<<(BMS-1)) ){
    for(i=BMS-1; i<pTable->nCol; i++){
      pIdx->aiColumn[n] = i;
      pIdx->azColl[n] = "BINARY";
      n++;
    }
  }
  assert( n==nColumn );

<<<<<<< HEAD
  /* Create the automatic index */
  /* 生成一个自动索引*/
=======
  /* Create the automatic index 创建自动索引 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  pKeyinfo = sqlite3IndexKeyinfo(pParse, pIdx);
  assert( pLevel->iIdxCur>=0 );
  sqlite3VdbeAddOp4(v, OP_OpenAutoindex, pLevel->iIdxCur, nColumn+1, 0,
                    (char*)pKeyinfo, P4_KEYINFO_HANDOFF);
  VdbeComment((v, "for %s", pTable->zName));

<<<<<<< HEAD
  /* Fill the automatic index with content */
  /* 填上自动索引的内容*/
=======
  /* Fill the automatic index with content 填充自动索引的内容 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  addrTop = sqlite3VdbeAddOp1(v, OP_Rewind, pLevel->iTabCur);
  regRecord = sqlite3GetTempReg(pParse);
  sqlite3GenerateIndexKey(pParse, pIdx, pLevel->iTabCur, regRecord, 1);
  sqlite3VdbeAddOp2(v, OP_IdxInsert, pLevel->iIdxCur, regRecord);
  sqlite3VdbeChangeP5(v, OPFLAG_USESEEKRESULT);
  sqlite3VdbeAddOp2(v, OP_Next, pLevel->iTabCur, addrTop+1);
  sqlite3VdbeChangeP5(v, SQLITE_STMTSTATUS_AUTOINDEX);
  sqlite3VdbeJumpHere(v, addrTop);
  sqlite3ReleaseTempReg(pParse, regRecord);
  
<<<<<<< HEAD
  /* Jump here when skipping the initialization */
  /* 跳过初始化时则跳到此处*/
=======
  /* Jump here when skipping the initialization 当跳过初始化时跳过这里 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  sqlite3VdbeJumpHere(v, addrInit);
}
#endif /* SQLITE_OMIT_AUTOMATIC_INDEX *//* SQLITE_OMIT_AUTOMATIC_INDEX结束*/

#ifndef SQLITE_OMIT_VIRTUALTABLE
/*
** Allocate and populate an sqlite3_index_info structure. It is the 
** responsibility of the caller to eventually release the structure
** by passing the pointer returned by this function to sqlite3_free().
**
** 分配和填充一个sqlite3_index_info数据结构。
** 它的作用是让调用者通过这个函数返回给sqlite3_free()的指针最终释放数据结构
**
*/
/* 分配和填充一个sqlite3_index_info结构。通过sqlite3_free()函数返回的
** 指针的最终释放是由它的调用者负责的。
*/
static sqlite3_index_info *allocateIndexInfo(
  Parse *pParse, 
  WhereClause *pWC,
  struct SrcList_item *pSrc,
  ExprList *pOrderBy
){
  int i, j;
  int nTerm;
  struct sqlite3_index_constraint *pIdxCons;
  struct sqlite3_index_orderby *pIdxOrderBy;
  struct sqlite3_index_constraint_usage *pUsage;
  WhereTerm *pTerm;
  int nOrderBy;
  sqlite3_index_info *pIdxInfo;

  WHERETRACE(("Recomputing index info for %s...\n", pSrc->pTab->zName));

  /* Count the number of possible WHERE clause constraints referring 统计与这个虚拟表可能相关联的WHERE子句的个数
  ** to this virtual table */
  /* 计算指向这个虚表的可能的WHERE子句约束的数量。
  */
  for(i=nTerm=0, pTerm=pWC->a; i<pWC->nTerm; i++, pTerm++){
    if( pTerm->leftCursor != pSrc->iCursor ) continue;
    assert( (pTerm->eOperator&(pTerm->eOperator-1))==0 );
    testcase( pTerm->eOperator==WO_IN );
    testcase( pTerm->eOperator==WO_ISNULL );
    if( pTerm->eOperator & (WO_IN|WO_ISNULL) ) continue;
    if( pTerm->wtFlags & TERM_VNULL ) continue;
    nTerm++;
  }

  /* If the ORDER BY clause contains only columns in the current 
  ** virtual table then allocate space for the aOrderBy part of
  ** the sqlite3_index_info structure.
  **
  ** 如果ORDER BY子句只包含在当前虚表的列，那么为sqlite3_index_info数据结构的aOrderBy部分分配空间
  */
  /* 如果ORDER BY子句只包含当前虚表的列，那么为sqlite3_index_info
  ** 结构中的aOrderby部分分配空间。
  */
  nOrderBy = 0;
  if( pOrderBy ){
    for(i=0; i<pOrderBy->nExpr; i++){
      Expr *pExpr = pOrderBy->a[i].pExpr;
      if( pExpr->op!=TK_COLUMN || pExpr->iTable!=pSrc->iCursor ) break;
    }
    if( i==pOrderBy->nExpr ){
      nOrderBy = pOrderBy->nExpr;
    }
  }

  /* Allocate the sqlite3_index_info structure 分配sqlite3_index_info数据结构
  */
  /* 分配sqlite3_index_info结构。
  */
  pIdxInfo = sqlite3DbMallocZero(pParse->db, sizeof(*pIdxInfo)
                           + (sizeof(*pIdxCons) + sizeof(*pUsage))*nTerm
                           + sizeof(*pIdxOrderBy)*nOrderBy );
  if( pIdxInfo==0 ){
    sqlite3ErrorMsg(pParse, "out of memory");
    /* (double)0 In case of SQLITE_OMIT_FLOATING_POINT... */
    return 0;
  }

  /* Initialize the structure.  The sqlite3_index_info structure contains
  ** many fields that are declared "const" to prevent xBestIndex from
  ** changing them.  We have to do some funky casting in order to
  ** initialize those fields.
  **
  ** 初始化数据结构。sqlite3_index_info数据结构包含许多被声明为"const"从而用于阻止xBestIndex来改变的字段。
  ** 为了初始化这些字段，我们不得不做一些特别的转化。
  */
  /* 初始化结构。sqlite3_index_info结构包含许多字段声明“常量”,以防止
  ** xBestIndex改变他们。我们必须做一些操作来初始化这些字段。
  */
  pIdxCons = (struct sqlite3_index_constraint*)&pIdxInfo[1];
  pIdxOrderBy = (struct sqlite3_index_orderby*)&pIdxCons[nTerm];
  pUsage = (struct sqlite3_index_constraint_usage*)&pIdxOrderBy[nOrderBy];
  *(int*)&pIdxInfo->nConstraint = nTerm;
  *(int*)&pIdxInfo->nOrderBy = nOrderBy;
  *(struct sqlite3_index_constraint**)&pIdxInfo->aConstraint = pIdxCons;
  *(struct sqlite3_index_orderby**)&pIdxInfo->aOrderBy = pIdxOrderBy;
  *(struct sqlite3_index_constraint_usage**)&pIdxInfo->aConstraintUsage =
                                                                   pUsage;

  for(i=j=0, pTerm=pWC->a; i<pWC->nTerm; i++, pTerm++){
    if( pTerm->leftCursor != pSrc->iCursor ) continue;
    assert( (pTerm->eOperator&(pTerm->eOperator-1))==0 );
    testcase( pTerm->eOperator==WO_IN );
    testcase( pTerm->eOperator==WO_ISNULL );
    if( pTerm->eOperator & (WO_IN|WO_ISNULL) ) continue;
    if( pTerm->wtFlags & TERM_VNULL ) continue;
    pIdxCons[j].iColumn = pTerm->u.leftColumn;
    pIdxCons[j].iTermOffset = i;
    pIdxCons[j].op = (u8)pTerm->eOperator;
    /* The direct assignment in the previous line is possible only because
    ** the WO_ and SQLITE_INDEX_CONSTRAINT_ codes are identical.  The
<<<<<<< HEAD
    ** following asserts verify this fact. */
	/* 在前一行直接赋值可能是由于WO_和SQLITE_INDEX_CONSTRAINT_代码都是相同
	** 的。以下假设用于验证这个额情况。
	*/
=======
    ** following asserts verify this fact. 
    **
    ** 只因为WO_和SQLITE_INDEX_CONSTRAINT_代码是完全相同的，所以在前一行直接分配是可能的。
    ** 下面asserts验证这个事实
    */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    assert( WO_EQ==SQLITE_INDEX_CONSTRAINT_EQ );
    assert( WO_LT==SQLITE_INDEX_CONSTRAINT_LT );
    assert( WO_LE==SQLITE_INDEX_CONSTRAINT_LE );
    assert( WO_GT==SQLITE_INDEX_CONSTRAINT_GT );
    assert( WO_GE==SQLITE_INDEX_CONSTRAINT_GE );
    assert( WO_MATCH==SQLITE_INDEX_CONSTRAINT_MATCH );
    assert( pTerm->eOperator & (WO_EQ|WO_LT|WO_LE|WO_GT|WO_GE|WO_MATCH) );
    j++;
  }
  for(i=0; i<nOrderBy; i++){
    Expr *pExpr = pOrderBy->a[i].pExpr;
    pIdxOrderBy[i].iColumn = pExpr->iColumn;
    pIdxOrderBy[i].desc = pOrderBy->a[i].sortOrder;
  }

  return pIdxInfo;
}

/*
** The table object reference passed as the second argument to this function
** must represent a virtual table. This function invokes the xBestIndex()
** method of the virtual table with the sqlite3_index_info pointer passed
** as the argument.
**
** 在这个函数中的第二个参数--表对象引用必须表示一个虚拟表。
** 这个函数唤醒虚拟表中有sqlite3_index_info指针的xBestIndex()方法作为参数
**
** If an error occurs, pParse is populated with an error message and a
** non-zero value is returned. Otherwise, 0 is returned and the output
** part of the sqlite3_index_info structure is left populated.
**
** 如果一个错误出现，pParse用一个错误信息填充并且一个非0值将会被返回。
** 否则，就返回0并且填充剩下的sqlite3_index_info数据结构的输出部分。
**
** Whether or not an error is returned, it is the responsibility of the
** caller to eventually free p->idxStr if p->needToFreeIdxStr indicates
** that this is required.
**
** 不管是否返回一个错误信息，如果p->needToFreeIdxStr索引是必须的，那么它最终会用户调用者释放p->idxStr
*/
/* 表对象引用作为第二个参数传递给该函数必须代表一个虚表。这个函数调用
** xBestIndex()方法的虚表sqlite3_index_info指针作为参数传递。
**
** 如果出现错误，pParse填充一个错误消息,则返回非零值。并且
** sqlite3_index_info中输出的部分被留下填充。
**
** 如果p->needToFreeIdxStr这是必须的话，那么不管是否返回了一个错误，那
** 么都应该由调用者最终释放p->idxStr指针。
*/
static int vtabBestIndex(Parse *pParse, Table *pTab, sqlite3_index_info *p){
  sqlite3_vtab *pVtab = sqlite3GetVTable(pParse->db, pTab)->pVtab;
  int i;
  int rc;

  WHERETRACE(("xBestIndex for %s\n", pTab->zName));
  TRACE_IDX_INPUTS(p);
  rc = pVtab->pModule->xBestIndex(pVtab, p);
  TRACE_IDX_OUTPUTS(p);

  if( rc!=SQLITE_OK ){
    if( rc==SQLITE_NOMEM ){
      pParse->db->mallocFailed = 1;
    }else if( !pVtab->zErrMsg ){
      sqlite3ErrorMsg(pParse, "%s", sqlite3ErrStr(rc));
    }else{
      sqlite3ErrorMsg(pParse, "%s", pVtab->zErrMsg);
    }
  }
  sqlite3_free(pVtab->zErrMsg);
  pVtab->zErrMsg = 0;

  for(i=0; i<p->nConstraint; i++){
    if( !p->aConstraint[i].usable && p->aConstraintUsage[i].argvIndex>0 ){
      sqlite3ErrorMsg(pParse, 
          "table %s: xBestIndex returned an invalid plan", pTab->zName);
    }
  }

  return pParse->nErr;
}


/*
** Compute the best index for a virtual table.
**
** 计算虚拟表的最佳索引
**
** The best index is computed by the xBestIndex method of the virtual
** table module.  This routine is really just a wrapper that sets up
** the sqlite3_index_info structure that is used to communicate with
** xBestIndex.
**
** 通过虚拟表模块的xBestIndex方法来计算最佳索引。
** 这个程序实际上只是一个包装，创立sqlite3_index_info数据结构，并且被用于与xBestIndex相联系
**
** In a join, this routine might be called multiple times for the
** same virtual table.  The sqlite3_index_info structure is created
** and initialized on the first invocation and reused on all subsequent
** invocations.  The sqlite3_index_info structure is also used when
** code is generated to access the virtual table.  The whereInfoDelete() 
** routine takes care of freeing the sqlite3_index_info structure after
** everybody has finished with it.
**
** 在一个连接中，这个程序可能被相同的虚拟表调用多次。
** 在第一次调用和创建和初始化sqlite3_index_info数据结构并且在所有的随后的调用中重用。
** 当生成访问虚拟表的代码时，也会使用sqlite3_index_info数据结构，
** 当所有的人都执行完成，那么whereInfoDelete()程序会释放sqlite3_index_info数据结构
*/
/*
** 计算虚表的最佳索引。
** 
** 最佳索引是使用虚表模块的xBestIndex函数来计算的。这个例程实际上只是建立
** sqlite3_index_info结构的包装，这个结构用来与xBestIndex通信。
** 
** 在一次连接当中，这个例程可能会被同一个虚表调用若干次。sqlite3_index_info
** 结构是在第一次调用时创建和初始化的，并且在所有的子调用中都会重复使用。
** sqlite3_index_info结构同样也在生成代码来进入虚表时被使用。
** whereInfoDelete()例程用来处理在所有操作都完成后对sqlite3_index_info结构的
** 释放。
*/
static void bestVirtualIndex(
<<<<<<< HEAD
	Parse *pParse,                  /* The parsing context *//* 解析上下文*/
	WhereClause *pWC,               /* The WHERE clause *//* WHERE子句*/
struct SrcList_item *pSrc,      /* The FROM clause term to search *//* 用于搜索的FROM子句项*/
	Bitmask notReady,               /* Mask of cursors not available for index *//* 索引不可用的指针掩码*/
	Bitmask notValid,               /* Cursors not valid for any purpose *//* 所有无效的指针*/
	ExprList *pOrderBy,             /* The order by clause *//* 子句排序*/
	WhereCost *pCost,               /* Lowest cost query plan *//* 查询计划的最小代价*/
	sqlite3_index_info **ppIdxInfo  /* Index information passed to xBestIndex *//* 传送到xBestIndex的索引信息*/
=======
  Parse *pParse,                  /* The parsing context 分析上下文 */
  WhereClause *pWC,               /* The WHERE clause WHERE子句 */
  struct SrcList_item *pSrc,      /* The FROM clause term to search 需要查询的FROM子句 */
  Bitmask notReady,               /* Mask of cursors not available for index 游标掩码对于索引无效 */
  Bitmask notValid,               /* Cursors not valid for any purpose 游标对于任何用途都无效 */
  ExprList *pOrderBy,             /* The order by clause ORDER BY子句 */
  WhereCost *pCost,               /* Lowest cost query plan 最小代价插叙计划 */
  sqlite3_index_info **ppIdxInfo  /* Index information passed to xBestIndex 传人xBestIndex的索引信息 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
){
  Table *pTab = pSrc->pTab; //初始化表结构
  sqlite3_index_info *pIdxInfo; //用于存储选出的索引信息
  struct sqlite3_index_constraint *pIdxCons;  //用于存储索引约束信息
  struct sqlite3_index_constraint_usage *pUsage; //用于有用的索引约束
  WhereTerm *pTerm;
  int i, j; //i是循环计数器，j用于存储
  int nOrderBy; //Order By中的terms数
  double rCost; //所需代价

  /* Make sure wsFlags is initialized to some sane value. Otherwise, if the 
  ** malloc in allocateIndexInfo() fails and this function returns leaving
  ** wsFlags in an uninitialized state, the caller may behave unpredictably.
  **
  ** 确保初始化wsFlags为一些理想的值。另外，如果在allocateIndexInfo()中分配内存失败，
  ** 并且这个函数返回剩余的处于未初始化状态的wsFlags，调用者的行为是不可预见地。
  */
<<<<<<< HEAD
  /*
  ** 确保wsFlags是以有意义的值初始化的。否则的话，如果allocateIndexInfo()的
  ** 内存失败，并且这个函数返回值，wsFlags处于未初始化状态，那么调用者可能会
  ** 有不可预计的操作。
  */
  memset(pCost, 0, sizeof(*pCost));
  pCost->plan.wsFlags = WHERE_VIRTUALTABLE;
=======
  memset(pCost, 0, sizeof(*pCost)); //分配内存
  pCost->plan.wsFlags = WHERE_VIRTUALTABLE; //标志计划是使用虚拟表处理
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949

  /* If the sqlite3_index_info structure has not been previously
  ** allocated and initialized, then allocate and initialize it now.
  ** 如果sqlite3_index_info结构体没被预先分配和初始化，那么现在就分配和初始化。
  */
  /*
  ** 如果sqlite3_index_info结构没有被预先分配和初始化，那么可以现在进行分配
  ** 和初始化。
  */
  pIdxInfo = *ppIdxInfo;
  if( pIdxInfo==0 ){//如果索引信息为被初始化
    *ppIdxInfo = pIdxInfo = allocateIndexInfo(pParse, pWC, pSrc, pOrderBy);//分配和初始化索引信息
  }
  if( pIdxInfo==0 ){//如果分配和初始化索引信息失败
    return;
  }

  /* At this point, the sqlite3_index_info structure that pIdxInfo points
  ** to will have been initialized, either during the current invocation or
  ** during some prior invocation.  Now we just have to customize the
  ** details of pIdxInfo for the current invocation and pass it to
  ** xBestIndex.
  ** 
  ** 此时，在当前调用或在一些先前的调用期间，pIdxInfo指向的sqlite3_index_info数据结构将被初始化。
  ** 现在需要为当前调用自定义pIdxInfo的详情，并且把它传递给xBestIndex
  */
  /* 此时，pIdxInfo指向的sqlite3_index_info结构已被初始化，可能是在当前调用
  ** 或者在之前的调用中完成初始化。我们现在只需要为当前调用定制pIdxInfo定制
  ** 细节，并把它传送到xBestIndex。
  */

  /* The module name must be defined. Also, by this point there must
  ** be a pointer to an sqlite3_vtab structure. Otherwise
  ** sqlite3ViewGetColumnNames() would have picked up the error. 
  **
  ** 必须定义模块名。通过这点，必须有个指针指向sqlite3_vtab数据结构。
  ** 另外sqlite3ViewGetColumnNames()将会处理错误
  */
<<<<<<< HEAD
  /*
  ** 定义模块名。同时，必须有一个指针指向sqlite3_vtab结构，否则
  ** sqlite3ViewColumnNames()可能会出现错误。
  */
  assert( pTab->azModuleArg && pTab->azModuleArg[0] );
=======
  assert( pTab->azModuleArg && pTab->azModuleArg[0] ); //检验是否定义了模块名
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  assert( sqlite3GetVTable(pParse->db, pTab) );

  /* Set the aConstraint[].usable fields and initialize all 
  ** output variables to zero.
  **
  ** 设置aConstraint[].usable字段并且所有输出变量初始化为0
  **
  ** aConstraint[].usable is true for constraints where the right-hand
  ** side contains only references to tables to the left of the current
  ** table.  In other words, if the constraint is of the form:
  **
  **           column = expr
  **
  ** and we are evaluating a join, then the constraint on column is 
  ** only valid if all tables referenced in expr occur to the left
  ** of the table containing column.
  **
<<<<<<< HEAD
  ** The aConstraints[] array contains for all constraints
=======
  ** aConstraint[].usable对于右边只包含引用表到当前表的左边的约束是TRUE.
  ** 换句话说，如果约束是column = expr这种形式并且我们评估一个连接，
  ** 那么在列上的约束只在所有的表与在expr出现的表包含的左边的列相关时才有效。
  **
  ** The aConstraints[] array contains entries for all constraints
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  ** on the current table.  That way we only have to compute it once
  ** even though we might try to pick the best index multiple times.
  ** For each attempt at picking an index, the order of tables in the
  ** join might be different so we have to recompute the usable flag
  ** each time.
  **
  ** aConstraints[]数组包含在当前表上的所有约束的记录。
  ** 那样即使我们应该尝试多次去选择最好的索引，但我们只要计算一次。
  ** 对于每次尝试选取一个索引，因为在连接中的表的顺序可能不同，所以每次我们需要重复计算可用的计算
  */
<<<<<<< HEAD
  /*
  ** 设置aConstraint[].usable域并初始化所有输出变量为0。
  **
  ** 当右边只包含指向当前表中指向左边的表时，满足约束条件的
  ** aConstraint[].usable为真。换句话说，如果约束条件是如下形式：
  **
  **           column = expr
  ** 并且正在评估一个连接，那么如果所有expr中提及的表指向表的左边包含列
  ** 那么列的约束条件为有效的。
  **
  ** aConstraints数组包含所有当前当前表的约束条件。这样一来，即使我们多
  ** 次试图只取最优索引，也只需要计算一次。每次取索引的过程中，连接的表
  ** 的顺序可能有所不同，所以我们需要每次都重复计算可用标志。
  */
  pIdxCons = *(struct sqlite3_index_constraint**)&pIdxInfo->aConstraint;
  pUsage = pIdxInfo->aConstraintUsage;
  for(i=0; i<pIdxInfo->nConstraint; i++, pIdxCons++){
    j = pIdxCons->iTermOffset;
=======
  pIdxCons = *(struct sqlite3_index_constraint**)&pIdxInfo->aConstraint; //初始化pIdxCons
  pUsage = pIdxInfo->aConstraintUsage; //初始化pUsage
  for(i=0; i<pIdxInfo->nConstraint; i++, pIdxCons++){ //循环遍历
    j = pIdxCons->iTermOffset; 
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    pTerm = &pWC->a[j];
    pIdxCons->usable = (pTerm->prereqRight&notReady) ? 0 : 1;
  }
  memset(pUsage, 0, sizeof(pUsage[0])*pIdxInfo->nConstraint);
  if( pIdxInfo->needToFreeIdxStr ){
    sqlite3_free(pIdxInfo->idxStr);
  }
  pIdxInfo->idxStr = 0;
  pIdxInfo->idxNum = 0;
  pIdxInfo->needToFreeIdxStr = 0;
  pIdxInfo->orderByConsumed = 0;
  /* ((double)2) In case of SQLITE_OMIT_FLOATING_POINT... */
  pIdxInfo->estimatedCost = SQLITE_BIG_DBL / ((double)2);
  nOrderBy = pIdxInfo->nOrderBy;
  if( !pOrderBy ){
    pIdxInfo->nOrderBy = 0;
  }

  if( vtabBestIndex(pParse, pTab, pIdxInfo) ){
    return;
  }

  pIdxCons = *(struct sqlite3_index_constraint**)&pIdxInfo->aConstraint;
  for(i=0; i<pIdxInfo->nConstraint; i++){
    if( pUsage[i].argvIndex>0 ){
      pCost->used |= pWC->a[pIdxCons[i].iTermOffset].prereqRight;
    }
  }

  /* If there is an ORDER BY clause, and the selected virtual table index
  ** does not satisfy it, increase the cost of the scan accordingly. This
  ** matches the processing for non-virtual tables in bestBtreeIndex().
  ** 如果有一个ORDER BY子句，并且选出的虚拟表索引不能被ORDER BY使用，查找的代价相应的增加。
  ** 这与在bestBtreeIndex()中的非虚拟表的处理过程相匹配。
  */
  /* 如果存在一个ORDER BY子句，并且选择的虚表索引不满足它，则根据情况增加
  ** 扫描的代价。这个过程匹配bestBtreeIndex()中的非虚表中的过程。
  */
  rCost = pIdxInfo->estimatedCost;
  if( pOrderBy && pIdxInfo->orderByConsumed==0 ){
    rCost += estLog(rCost)*rCost;
  }

  /* The cost is not allowed to be larger than SQLITE_BIG_DBL (the
  ** inital value of lowestCost in this loop. If it is, then the
  ** (cost<lowestCost) test below will never be true.
  ** 
  ** 代价不允许大于SQLITE_BIG_DBL(在这个循环中最低代价的初始值)。
  ** 如果它比SQLITE_BIG_DBL大，那么下面的比较(cost<lowestCost)将永远是错的
  **
  ** Use "(double)2" instead of "2.0" in case OMIT_FLOATING_POINT 
  ** is defined.
  **
  ** 加入定义了OMIT_FLOATING_POINT，使用"(double)2"代替"2.0"
  */
  /* 代价不允许大于SQLITE_BIG_DBL(此循环中的lowestCost的初始值)。
  ** 如果大于，则下面的(cost<lowestCost)测试值永远不会为真。
  **
  ** 在定义OMIT_FLOATING_POINT时使用"(double)2"来替换"2.0"。
  */
  if( (SQLITE_BIG_DBL/((double)2))<rCost ){
    pCost->rCost = (SQLITE_BIG_DBL/((double)2));
  }else{
    pCost->rCost = rCost;
  }
  pCost->plan.u.pVtabIdx = pIdxInfo;
  if( pIdxInfo->orderByConsumed ){
    pCost->plan.wsFlags |= WHERE_ORDERBY;
  }
  pCost->plan.nEq = 0;
  pIdxInfo->nOrderBy = nOrderBy;

  /* Try to find a more efficient access pattern by using multiple indexes
  ** to optimize an OR expression within the WHERE clause. 
  **
  ** 尝试通过多重索引查找一个更加有效的访问模式去优化一个WHERE子句中的OR表达式
  */
  /* 使用多索引来试图查找一个更有效率的模式，以优化WHERE子句中的OR表达式。
  */
  bestOrClauseIndex(pParse, pWC, pSrc, notReady, notValid, pOrderBy, pCost);
}
#endif /* SQLITE_OMIT_VIRTUALTABLE */

#ifdef SQLITE_ENABLE_STAT3
/*
** Estimate the location of a particular key among all keys in an
** index.  Store the results in aStat as follows:
**
**    aStat[0]      Est. number of rows less than pVal
**    aStat[1]      Est. number of rows equal to pVal
**
** Return SQLITE_OK on success.
** 估计在一个索引的所有键中的一个特别键的位置。在aStat中像下面这样保存结果:
**    aStat[0]      行的Est. number小于pVal
**    aStat[1]      行的Est. number等于pVal
** 
*/
/* 估计一个索引中所有关键字中某个特殊关键字的位置。用以下格式来储存结果：
**    aStat[0]      估计小于pVal的行数
**    aStat[1]      估计等于pVal的行数
**
** 返回成功的SQLITE_OK值。
*/
static int whereKeyStats(
<<<<<<< HEAD
	Parse *pParse,              /* Database connection *//*数据库连接*/
	Index *pIdx,                /* Index to consider domain of *//*待考虑域的索引*/
	sqlite3_value *pVal,        /* Value to consider *//*待考虑的值*/
	int roundUp,                /* Round up if true.  Round down if false *//*如果为真则上舍入，如果为假则下舍入*/
	tRowcnt *aStat              /* OUT: stats written here *//*输出:数据写在此处*/
=======
  Parse *pParse,              /* Database connection 数据库连接 */
  Index *pIdx,                /* Index to consider domain of 需要考虑的索引域 */
  sqlite3_value *pVal,        /* Value to consider 需要考虑的值 */
  int roundUp,                /* Round up if true.  Round down if false 如果TRUE，则上舍入，如果FALSE，则下舍入 */
  tRowcnt *aStat              /* OUT: stats written here */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
){
  tRowcnt n;
  IndexSample *aSample;
  int i, eType;
  int isEq = 0;
  i64 v;
  double r, rS;

  assert( roundUp==0 || roundUp==1 );
  assert( pIdx->nSample>0 );
  if( pVal==0 ) return SQLITE_ERROR;
  n = pIdx->aiRowEst[0];
  aSample = pIdx->aSample;
  eType = sqlite3_value_type(pVal);

  if( eType==SQLITE_INTEGER ){
    v = sqlite3_value_int64(pVal);
    r = (i64)v;
    for(i=0; i<pIdx->nSample; i++){
      if( aSample[i].eType==SQLITE_NULL ) continue;
      if( aSample[i].eType>=SQLITE_TEXT ) break;
      if( aSample[i].eType==SQLITE_INTEGER ){
        if( aSample[i].u.i>=v ){
          isEq = aSample[i].u.i==v;
          break;
        }
      }else{
        assert( aSample[i].eType==SQLITE_FLOAT );
        if( aSample[i].u.r>=r ){
          isEq = aSample[i].u.r==r;
          break;
        }
      }
    }
  }else if( eType==SQLITE_FLOAT ){
    r = sqlite3_value_double(pVal);
    for(i=0; i<pIdx->nSample; i++){
      if( aSample[i].eType==SQLITE_NULL ) continue;
      if( aSample[i].eType>=SQLITE_TEXT ) break;
      if( aSample[i].eType==SQLITE_FLOAT ){
        rS = aSample[i].u.r;
      }else{
        rS = aSample[i].u.i;
      }
      if( rS>=r ){
        isEq = rS==r;
        break;
      }
    }
  }else if( eType==SQLITE_NULL ){
    i = 0;
    if( aSample[0].eType==SQLITE_NULL ) isEq = 1;
  }else{
    assert( eType==SQLITE_TEXT || eType==SQLITE_BLOB );
    for(i=0; i<pIdx->nSample; i++){
      if( aSample[i].eType==SQLITE_TEXT || aSample[i].eType==SQLITE_BLOB ){
        break;
      }
    }
    if( i<pIdx->nSample ){      
      sqlite3 *db = pParse->db;
      CollSeq *pColl;
      const u8 *z;
      if( eType==SQLITE_BLOB ){
        z = (const u8 *)sqlite3_value_blob(pVal);
        pColl = db->pDfltColl;
        assert( pColl->enc==SQLITE_UTF8 );
      }else{
        pColl = sqlite3GetCollSeq(db, SQLITE_UTF8, 0, *pIdx->azColl);
        if( pColl==0 ){
          sqlite3ErrorMsg(pParse, "no such collation sequence: %s",
                          *pIdx->azColl);
          return SQLITE_ERROR;
        }
        z = (const u8 *)sqlite3ValueText(pVal, pColl->enc);
        if( !z ){
          return SQLITE_NOMEM;
        }
        assert( z && pColl && pColl->xCmp );
      }
      n = sqlite3ValueBytes(pVal, pColl->enc);
  
      for(; i<pIdx->nSample; i++){
        int c;
        int eSampletype = aSample[i].eType;
        if( eSampletype<eType ) continue;
        if( eSampletype!=eType ) break;
#ifndef SQLITE_OMIT_UTF16
        if( pColl->enc!=SQLITE_UTF8 ){
          int nSample;
          char *zSample = sqlite3Utf8to16(
              db, pColl->enc, aSample[i].u.z, aSample[i].nByte, &nSample
          );
          if( !zSample ){
            assert( db->mallocFailed );
            return SQLITE_NOMEM;
          }
          c = pColl->xCmp(pColl->pUser, nSample, zSample, n, z);
          sqlite3DbFree(db, zSample);	//释放可能被关联到一个特定数据库连接的内存
        }else
#endif
        {
          c = pColl->xCmp(pColl->pUser, aSample[i].nByte, aSample[i].u.z, n, z);
        }
        if( c>=0 ){
          if( c==0 ) isEq = 1;
          break;
        }
      }
    }
  }

  /* At this point, aSample[i] is the first sample that is greater than
  ** or equal to pVal.  Or if i==pIdx->nSample, then all samples are less
  ** than pVal.  If aSample[i]==pVal, then isEq==1.
  ** 
  ** 这时，aSample[i]是第一个大于或等于pVal的样本。
  ** 或者如果i==pIdx->nSample，那么所有的样本都比pVal小。
  ** 如果aSample[i]==pVal，那么isEq==1.
  */
  /* 此时，aSample[i]是大于或者等于pVal值的第一个样本。或者当指针
  ** i==pIdx->nSample时，所有的样本值都小于pVal。如果aSample[i]=pVal，则
  ** isEq的值为1.
  */
  if( isEq ){
    assert( i<pIdx->nSample );
    aStat[0] = aSample[i].nLt;
    aStat[1] = aSample[i].nEq;
  }else{
    tRowcnt iLower, iUpper, iGap;
    if( i==0 ){
      iLower = 0;
      iUpper = aSample[0].nLt;
    }else{
      iUpper = i>=pIdx->nSample ? n : aSample[i].nLt;
      iLower = aSample[i-1].nEq + aSample[i-1].nLt;
    }
    aStat[1] = pIdx->avgEq;
    if( iLower>=iUpper ){
      iGap = 0;
    }else{
      iGap = iUpper - iLower;
    }
    if( roundUp ){
      iGap = (iGap*2)/3;
    }else{
      iGap = iGap/3;
    }
    aStat[0] = iLower + iGap;
  }
  return SQLITE_OK;
}
#endif /* SQLITE_ENABLE_STAT3 */

/*
** If expression pExpr represents a literal value, set *pp to point to
** an sqlite3_value structure containing the same value, with affinity
** aff applied to it, before returning. It is the responsibility of the 
** caller to eventually release this structure by passing it to 
** sqlite3ValueFree().
**
** 如果pExpr表达式代表一个文字值，在返回前，设置*pp指向一个包含相同值的带有亲和性aff应用于sqlite3_value数据结构
** 这是为了让调用者通过把它传递给sqlite3ValueFree()并最终释放这个数据结构。
**
** If the current parse is a recompile (sqlite3Reprepare()) and pExpr
** is an SQL variable that currently has a non-NULL value bound to it,
** create an sqlite3_value structure containing this value, again with
** affinity aff applied to it, instead.
**
** 如果当前分析是一个重新编译(sqlite3Reprepare())并且pExpr是一个当前没有非NULL值绑定的SQL变量，
** 创建一个包含这个值的带有亲和性aff应用于sqlite3_value数据结构。
**
** If neither of the above apply, set *pp to NULL.
**
** 如果上述都不满足，把*pp设置为NULL值
**
** If an error occurs, return an error code. Otherwise, SQLITE_OK.
**
** 如果出现一个错误，那么返回一个错误代码，否则，SQLITE_OK.
*/
/* 如果表达式pExpr表示一个文本值，那么设*pp是指向包含相同值的sqlite3_value
** 结构的指针，并且在返回之前与它紧密相关。最终把它传递到sqlite3ValueFree()
** 中时，由调用者来释放此结构。
**
** 如果当前语法分析是重新编译的(sqlite3Reprepare())并且pExpr是一个SQL变量，
** 当前没有非空值与之相关，则生成一个包含此值的sqlite3_value结构，同样的要
** 与之紧密相关。
**
** 如果没有以上应用，则设*pp指针为空。
**
** 如果发生错误，则返回错误代码。否则返回SQLITE_OK。
*/
#ifdef SQLITE_ENABLE_STAT3
static int valueFromExpr(
  Parse *pParse, 
  Expr *pExpr, 
  u8 aff, 
  sqlite3_value **pp
){
  if( pExpr->op==TK_VARIABLE
   || (pExpr->op==TK_REGISTER && pExpr->op2==TK_VARIABLE)
  ){
    int iVar = pExpr->iColumn;
    sqlite3VdbeSetVarmask(pParse->pVdbe, iVar);
    *pp = sqlite3VdbeGetValue(pParse->pReprepare, iVar, aff);
    return SQLITE_OK;
  }
  return sqlite3ValueFromExpr(pParse->db, pExpr, SQLITE_UTF8, aff, pp);
}
#endif

/*
** This function is used to estimate the number of rows that will be visited
** by scanning an index for a range of values. The range may have an upper
** bound, a lower bound, or both. The WHERE clause terms that set the upper
** and lower bounds are represented by pLower and pUpper respectively. For
** example, assuming that index p is on t1(a):
**
**   ... FROM t1 WHERE a > ? AND a < ? ...
**                    |_____|   |_____|
**                       |         |
**                     pLower    pUpper
**
** If either of the upper or lower bound is not present, then NULL is passed in
** place of the corresponding WhereTerm.
**
** 这个函数用于估计通过扫描一个索引获取一系列值而需要访问的行数。
** 这个范围可能有上限，下限或者都有。WHERE子句terms通过pLower和pUpper分别设置上限和下限的值。
** 列入，假设索引p是在t1(a)上:
**   ... FROM t1 WHERE a > ? AND a < ? ...
**                    |_____|   |_____|
**                       |         |
**                     pLower    pUpper
** 如果没给出上限或下限，那么传递NULL值代替相应的WhereTerm.
**
** The nEq parameter is passed the index of the index column subject to the
** range constraint. Or, equivalently, the number of equality constraints
** optimized by the proposed index scan. For example, assuming index p is
** on t1(a, b), and the SQL query is:
**
**   ... FROM t1 WHERE a = ? AND b > ? AND b < ? ...
**
** then nEq should be passed the value 1 (as the range restricted column,
** b, is the second left-most column of the index). Or, if the query is:
**
**   ... FROM t1 WHERE a > ? AND a < ? ...
**
** then nEq should be passed 0.
**
** nEq参数需要传递为索引列的下标服从范围约束。或等价地，通过推荐的索引扫描优化等式约束的个数。
** 例如，假设索引p是在t1(a, b)中，SQL查询是:
**   ... FROM t1 WHERE a = ? AND b > ? AND b < ? ...
** 那么nEq传递为值1(作为范围受限的列，b，是索引第二个最左边的列).或，如果查询是:
**   ... FROM t1 WHERE a > ? AND a < ? ...
** 那么nEq传递为值0。
**
** The returned value is an integer divisor to reduce the estimated
** search space.  A return value of 1 means that range constraints are
** no help at all.  A return value of 2 means range constraints are
** expected to reduce the search space by half.  And so forth...
**
** 返回值是一个整数因子来减少预计的搜索空间。
** 一个值为1的返回值意味着范围约束是没有帮助的。
** 一个值为2的返回值意味着范围约束是期望减少一半的搜索空间。等等...
**
** In the absence of sqlite_stat3 ANALYZE data, each range inequality
** reduces the search space by a factor of 4.  Hence a single constraint (x>?)
** results in a return of 4 and a range constraint (x>? AND x<?) results
** in a return of 16.
**
** 缺少sqlite_stat3 ANALYZE数据，每个范围不等式将减少4倍的搜索空间。
** 因此一个单独的约束(x>?)导致返回4并且一个范围约束(x>? AND x<?)导致返回16。
*/
/* 此函数用来估计扫描一定范围数值的索引中，将会访问到的行数。这个范围可能会有
** 一个上限，一个下限，或者两者都有。WHERE子句项中，用pLower和pUpper来表示设定
** 的上下边界。例如，假设p索引在t1(a):
**
**   ... FROM t1 WHERE a > ? AND a < ? ...
**                    |_____|   |_____|
**                       |         |
**                     pLower    pUpper
**
** 如果上限和下限中的任意一个不存在，则WhereTerm中对应的项用空值来替换。
**
** 参数nEq是通过索引列的索引范围来约束的。或者同样地，由提出的索引扫描来优化等
** 值约束。例如，假设索引p在t1(a,b),则SQL查询为：
**
**   ... FROM t1 WHERE a = ? AND b > ? AND b < ? ...
**
** 那么nEq的值为1(范围限制列b，是第二个指数最左边的列)。或者，如果查询为：
**
**   ... FROM t1 WHERE a > ? AND a < ? ...
**
** 那么nEq的值为0。
**
** 返回值是一个整数因子用来减少估计的搜索空间。返回值为1意味着范围约束是没有意义。
** 返回值为2意味着范围约束预计将减少一半的搜索空间。等等……
**
** 在缺乏sqlite_stat3分析数据的情况下，每个不平等因素减少了搜索空间的范围4。因此
** 一个约束(x>?)结果的返回4和一系列约束(x>?和x<?)结果返回16。
*/
static int whereRangeScanEst(
<<<<<<< HEAD
	Parse *pParse,       /* Parsing & code generating context *//*解析上下文并且生成代码*/
	Index *p,            /* The index containing the range-compared column; "x" *//*索引包含对比范围列"x"*/
	int nEq,             /* index into p->aCol[] of the range-compared column *//*指向对比范围列的索引指针p_>aCol[]*/
	WhereTerm *pLower,   /* Lower bound on the range. ex: "x>123" Might be NULL *//*范围的下界：例如"x>123"可能为空*/
	WhereTerm *pUpper,   /* Upper bound on the range. ex: "x<455" Might be NULL *//*范围的上界：例如"x<455"可能为空*/
	double *pRangeDiv   /* OUT: Reduce search space by this divisor *//*输出：减少这个因子引起的搜索空间*/
=======
  Parse *pParse,       /* Parsing & code generating context 分析并且代码生成上下文 */
  Index *p,            /* The index containing the range-compared column; "x" 索引包含范围对照的列:"x" */
  int nEq,             /* index into p->aCol[] of the range-compared column 索引指向范围对照列的p->aCol[] */
  WhereTerm *pLower,   /* Lower bound on the range. ex: "x>123" Might be NULL 在范围中的下限:ex: "x>123"可能是NULL */
  WhereTerm *pUpper,   /* Upper bound on the range. ex: "x<455" Might be NULL 在范围中的上限:ex: "x<455"可能是NULL */
  double *pRangeDiv   /* OUT: Reduce search space by this divisor OUT:通过这个因子来减少搜索空间 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
){
  int rc = SQLITE_OK;

#ifdef SQLITE_ENABLE_STAT3

  if( nEq==0 && p->nSample ){
    sqlite3_value *pRangeVal;
    tRowcnt iLower = 0;
    tRowcnt iUpper = p->aiRowEst[0];
    tRowcnt a[2];
    u8 aff = p->pTable->aCol[p->aiColumn[0]].affinity;

    if( pLower ){
      Expr *pExpr = pLower->pExpr->pRight;
      rc = valueFromExpr(pParse, pExpr, aff, &pRangeVal);
      assert( pLower->eOperator==WO_GT || pLower->eOperator==WO_GE );
      if( rc==SQLITE_OK
       && whereKeyStats(pParse, p, pRangeVal, 0, a)==SQLITE_OK
      ){
        iLower = a[0];
        if( pLower->eOperator==WO_GT ) iLower += a[1];
      }
      sqlite3ValueFree(pRangeVal);
    }
    if( rc==SQLITE_OK && pUpper ){
      Expr *pExpr = pUpper->pExpr->pRight;
      rc = valueFromExpr(pParse, pExpr, aff, &pRangeVal);
      assert( pUpper->eOperator==WO_LT || pUpper->eOperator==WO_LE );
      if( rc==SQLITE_OK
       && whereKeyStats(pParse, p, pRangeVal, 1, a)==SQLITE_OK
      ){
        iUpper = a[0];
        if( pUpper->eOperator==WO_LE ) iUpper += a[1];
      }
      sqlite3ValueFree(pRangeVal);
    }
    if( rc==SQLITE_OK ){
      if( iUpper<=iLower ){
        *pRangeDiv = (double)p->aiRowEst[0];
      }else{
        *pRangeDiv = (double)p->aiRowEst[0]/(double)(iUpper - iLower);
      }
      WHERETRACE(("range scan regions: %u..%u  div=%g\n",
                  (u32)iLower, (u32)iUpper, *pRangeDiv));
      return SQLITE_OK;
    }
  }
#else
  UNUSED_PARAMETER(pParse);
  UNUSED_PARAMETER(p);
  UNUSED_PARAMETER(nEq);
#endif
  assert( pLower || pUpper );
  *pRangeDiv = (double)1;
  if( pLower && (pLower->wtFlags & TERM_VNULL)==0 ) *pRangeDiv *= (double)4;
  if( pUpper ) *pRangeDiv *= (double)4;
  return rc;
}

#ifdef SQLITE_ENABLE_STAT3
/*
** Estimate the number of rows that will be returned based on
** an equality constraint x=VALUE and where that VALUE occurs in
** the histogram data.  This only works when x is the left-most
** column of an index and sqlite_stat3 histogram data is available
** for that index.  When pExpr==NULL that means the constraint is
** "x IS NULL" instead of "x=VALUE".
**
** 估计将会返回的行数，它是基于一个等式约束x=VALUE并且VALUE出现在直方图中的.
** 这只是在当x是一个索引的最左边的列并且sqlite_stat3直方图数据对于索引是有效时才起作用。
** 当pExpr==NULL意味着约束是用"x IS NULL"代替"x=VALUE".
**
** Write the estimated row count into *pnRow and return SQLITE_OK. 
** If unable to make an estimate, leave *pnRow unchanged and return
** non-zero.
**
** 把估计的行数写入到*pnRow中并且返回SQLITE_OK.
** 如果不能做一个估计，保持*pnRow不变并且返回非0值。
**
** This routine can fail if it is unable to load a collating sequence
** required for string comparison, or if unable to allocate memory
** for a UTF conversion required for comparison.  The error is stored
** in the pParse structure.
**
** 如果这个程序不能从字符串比较中请求加载到一个顺序序列，
** 或者如果不能从比较中请求一个UTF(Unicode转化模式)转化，那么这个程序会失败。
** 错误信息会存储在pParse数据结构中
*/
/* 
** 估计基于等值约束条件x=VALUE返回的列数，此VALUE值在柱状图数据中出现。
** 这个只在x是索引中最左边的列并且sqlite_stat3柱状数据对于此列表是可用
** 的情况下才起作用。当pExpr值为空时，意味着约束条件是"x为空"而不是"x的
** 值为VALUE"。
** 
** 记录估计的行数，将数值写书*pnRow然后返回SQLITE_OK。如果不能做出估计，
** 则保持*pnRow原数值并且返回非零。
**
** 如果不能够载入字符串比较所需的排序序列，或者不能够比较时所需的UTF会话
** 分配内存空间，则此例程可以算做失败。错误结果储存在pParse结构中。
*/
static int whereEqualScanEst(
<<<<<<< HEAD
	Parse *pParse,       /* Parsing & code generating context *//*解析上下文并且生成代码*/
	Index *p,            /* The index whose left-most column is pTerm *//*最左列是pTerm的索引*/
	Expr *pExpr,         /* Expression for VALUE in the x=VALUE constraint *//*x=VALUE约束条件中VALUE的表达式*/
	double *pnRow        /* Write the revised row estimate here *//*在此写下最终修改的行的估计值*/
){
	sqlite3_value *pRhs = 0;  /* VALUE on right-hand side of pTerm *//*pTerm右边项的值*/
	u8 aff;                   /* Column affinity *//*同类列*/
	int rc;                   /* Subfunction return code *//*子函数返回代码*/
	tRowcnt a[2];             /* Statistics *//*统计数据*/
=======
  Parse *pParse,       /* Parsing & code generating context 分析上下文和生成代码 */
  Index *p,            /* The index whose left-most column is pTerm pTerm的最左列的索引 */
  Expr *pExpr,         /* Expression for VALUE in the x=VALUE constraint 在x=VALUE约束中的VALUE表达式 */
  double *pnRow        /* Write the revised row estimate here 写入修改后的估计的行 */
){
  sqlite3_value *pRhs = 0;  /* VALUE on right-hand side of pTerm 在pTerm右边的VALUE */
  u8 aff;                   /* Column affinity 列亲和性 */
  int rc;                   /* Subfunction return code 返回代码的子函数 */
  tRowcnt a[2];             /* Statistics 统计信息 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949

  assert( p->aSample!=0 );
  assert( p->nSample>0 );
  aff = p->pTable->aCol[p->aiColumn[0]].affinity;
  if( pExpr ){
    rc = valueFromExpr(pParse, pExpr, aff, &pRhs);
    if( rc ) goto whereEqualScanEst_cancel;
  }else{
    pRhs = sqlite3ValueNew(pParse->db);
  }
  if( pRhs==0 ) return SQLITE_NOTFOUND;
  rc = whereKeyStats(pParse, p, pRhs, 0, a);
  if( rc==SQLITE_OK ){
    WHERETRACE(("equality scan regions: %d\n", (int)a[1]));
    *pnRow = a[1];
  }
whereEqualScanEst_cancel:
  sqlite3ValueFree(pRhs);
  return rc;
}
#endif /* defined(SQLITE_ENABLE_STAT3) *//*定义(SQLITE_ENABLE_STAT3)函数*/

#ifdef SQLITE_ENABLE_STAT3
/*
** Estimate the number of rows that will be returned based on
** an IN constraint where the right-hand side of the IN operator
** is a list of values.  Example:
**
**        WHERE x IN (1,2,3,4)
**
** Write the estimated row count into *pnRow and return SQLITE_OK. 
** If unable to make an estimate, leave *pnRow unchanged and return
** non-zero.
**
** 估计将要返回的行数，是基于一个IN约束，IN预算符的右边是一系列值。例如:
**        WHERE x IN (1,2,3,4)
** 把估计的行数写入到*pnRow并且返回SQLITE_OK。
** 如果不能做一个估计，保持*pnRow不变并且返回非0值。
**
** This routine can fail if it is unable to load a collating sequence
** required for string comparison, or if unable to allocate memory
** for a UTF conversion required for comparison.  The error is stored
** in the pParse structure.
**
** 如果这个程序不能从字符串比较中请求加载到一个顺序序列，
** 或者如果不能从比较中请求一个UTF(Unicode转化模式)转化，那么这个程序会失败。
** 错误信息会存储在pParse数据结构中

*/
/*
** 估计基于IN约束条件的返回的行数，这个约束条件是IN操作码的右边为一串
** 值。例如：
**
**        WHERE x IN (1,2,3,4)
**
** 对估计行数计数并且写入*pnRow，然后返回SQLITE_OK。如果不能作出估计，
** 则保持*pnRow为原值并且返回非零。
**
** 如果不能够载入字符串比较所需的排序序列，或者不能够比较时所需的UTF会话
** 分配内存空间，则此例程可以算做失败。错误结果储存在pParse结构中。
*/
static int whereInScanEst(
<<<<<<< HEAD
	Parse *pParse,       /* Parsing & code generating context *//*解析上下文并且生成代码*/
	Index *p,            /* The index whose left-most column is pTerm *//*最左边列为pTerm的索引*/
	ExprList *pList,     /* The value list on the RHS of "x IN (v1,v2,v3,...)" *//*x取值(v1,v2,v3...)的RHS的值的列表*/
  double *pnRow        /* Write the revised row estimate here *//*在此写下最终修改的行的估计值*/
){
	int rc = SQLITE_OK;         /* Subfunction return code *//*子函数返回代码*/
	double nEst;                /* Number of rows for a single term *//*单一项的行数*/
	double nRowEst = (double)0; /* New estimate of the number of rows *//*行数的新估计值*/
	int i;                      /* Loop counter *//*循环计数器*/
=======
  Parse *pParse,       /* Parsing & code generating context 解析和生成代码上下文 */
  Index *p,            /* The index whose left-most column is pTerm pTerm的最左列的索引 */
  ExprList *pList,     /* The value list on the RHS of "x IN (v1,v2,v3,...)" "x IN (v1,v2,v3,...)"右边的一系列值 */
  double *pnRow        /* Write the revised row estimate here 写入修改后的估计的行 */
){
  int rc = SQLITE_OK;         /* Subfunction return code 返回代码的子函数 */
  double nEst;                /* Number of rows for a single term 对于一个term的行数 */
  double nRowEst = (double)0; /* New estimate of the number of rows 新估计的行数 */
  int i;                      /* Loop counter 循环计数器 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949

  assert( p->aSample!=0 );
  for(i=0; rc==SQLITE_OK && i<pList->nExpr; i++){
    nEst = p->aiRowEst[0];
    rc = whereEqualScanEst(pParse, p, pList->a[i].pExpr, &nEst);
    nRowEst += nEst;
  }
  if( rc==SQLITE_OK ){
    if( nRowEst > p->aiRowEst[0] ) nRowEst = p->aiRowEst[0];
    *pnRow = nRowEst;
    WHERETRACE(("IN row estimate: est=%g\n", nRowEst));
  }
  return rc;
}
//毕赣斌结束

/*王秀超 从此开始
** Find the best query plan for accessing a particular table.  Write the
** best query plan and its cost into the WhereCost object supplied as the
** last parameter.
<<<<<<< HEAD
**为某个特定的表寻找最佳查询计划。确定最好的查询计划和成本，
写入WhereCost对象作为最后一个参数提供
=======
**
** 选择访问一个特别表的最好的查询计划。
** 把最好的查询优化和它的代价写入WhereCost对象，并且作为最后的参数提供。
**
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
** The lowest cost plan wins.  The cost is an estimate of the amount of
** CPU and disk I/O needed to process the requested result.
最低代价原则，代价是的一个估计的 CPU 和磁盘 I/O 处理过程的结果所需总和
** Factors that influence cost include:
影响代价的因素有:
**
**    *  The estimated number of rows that will be retrieved.  (The
**       fewer the better.)
**查询读取的记录数 (越少越好)
**    *  Whether or not sorting must occur.
** 结果是否排序.
**    *  Whether or not there must be separate lookups in the
**       index and in the main table.
<<<<<<< HEAD
**是否需要访问索引和原表
=======
**
** 最小代价的计划被采用。代价是对需要处理请求的CPU和磁盘I/O的总量的估算。
** 影响代价的因素包括:
**    *  估算将重新取回的行数
**
**    *  排序是否应该发生
**
**    *  在索引和主表中是否应该分开查找
**
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
** If there was an INDEXED BY clause (pSrc->pIndex) attached to the table in
** the SQL statement, then this function only considers plans using the 
** named index. If no such plan is found, then the returned cost is
** SQLITE_BIG_DBL. If a plan is found that uses the named index, 
** then the cost is calculated in the usual way.
<<<<<<< HEAD
**如果没有索引 BY 子句 （pSrc->pIndex） 附加到 SQL 语句中的表，
此函数只考虑计划使用指定的索引。
如果没有这样的计划找到，那么返回的成本就是 SQLITE_BIG_DBL。
如果一项计划，找到使用指定的索引，然后按通常方式计算代价。
=======
**
** 如果在SQL命令中表包含一个INDEXED BY(pSrc->pIndex)，那么这个函数值只考虑使用指定的索引。
** 如果没有找到这样的计划，那么返回的代价是SQLITE_BIG_DBL.
** 如果找到了使用指定索引的计划，那么用平常的方式计算代价。
**
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
** If a NOT INDEXED clause (pSrc->notIndexed!=0) was attached to the table 
** in the SELECT statement, then no indexes are considered. However, the 
** selected plan may still take advantage of the built-in rowid primary key
** index.
<<<<<<< HEAD
如果没有索引子句 （pSrc->notIndexed!=0） 附加到的表中的 SELECT 语句，认为没有索引。
然而，所选的计划仍然可以利用内置的 rowid 主键索引。
=======
**
** 如果在SELECT命令中表包含一个NOT INDEXED子句(pSrc->notIndexed!=0)，那么认为是没有索引的。
** 然而，查询计划可能仍旧利用创建在rowid上的主键索引
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
*/
//输出pCost，bestBtreeIndex函数包含查询策略信息及相应的代价
static void bestBtreeIndex(
<<<<<<< HEAD
  Parse *pParse,              /* 解析上下文 */
  WhereClause *pWC,           /* where子句  */
  struct SrcList_item *pSrc,  /*  FROM子句的搜索项 */
  Bitmask notReady,           /* 不适用于索引的游标掩码 */
  Bitmask notValid,           /* 不适用于任何用途的游标 */
  ExprList *pOrderBy,         /* ORDER BY 子句 */
  ExprList *pDistinct,        /* 定义DISTINCT的选择列表  */
  WhereCost *pCost            /* 代价最小查询方法 */
){
  int iCur = pSrc->iCursor;   /* 要访问表的游标 */
  Index *pProbe;              /* 我们正在测试的索引 */
  Index *pIdx;                /* pProbe的副本，或者是IPK索引*/
  int eqTermMask;             /* 当前掩码的有效的相同操作符 */
  int idxEqTermMask;          /* 有效的相同操作符的索引掩码 */
  Index sPk;                  /* 一个虚索引对象的主键 */
  tRowcnt aiRowEstPk[2];      /* sPk 索引的aiRowEst[] 值*/
  int aiColumnPk = -1;        /* sPk 索引的aColumn[]值 */
  int wsFlagMask;             /* pCost->plan.wsFlag中的允许标志 */

  /* Initialize the cost to a worst-case value 
  初始化开销的最坏情况值
  */
=======
  Parse *pParse,              /* The parsing context 解析上下文 */
  WhereClause *pWC,           /* The WHERE clause WHERE子句 */
  struct SrcList_item *pSrc,  /* The FROM clause term to search 进行查找的FROM子句term */
  Bitmask notReady,           /* Mask of cursors not available for indexing 游标掩码对于索引是无效的 */
  Bitmask notValid,           /* Cursors not available for any purpose 对于任何目的游标都无效 */
  ExprList *pOrderBy,         /* The ORDER BY clause ORDER BY子句 */
  ExprList *pDistinct,        /* The select-list if query is DISTINCT 如果查询是DISTINCT时，select-list */
  WhereCost *pCost            /* Lowest cost query plan 最小的代价查询计划 */
){
  int iCur = pSrc->iCursor;   /* The cursor of the table to be accessed 存取的表游标 */
  Index *pProbe;              /* An index we are evaluating 我们要评估的一个索引 */
  Index *pIdx;                /* Copy of pProbe, or zero for IPK index 把pProbe的复本或0给IPK索引 */
  int eqTermMask;             /* Current mask of valid equality operators 当前有效等式运算符的掩码 */
  int idxEqTermMask;          /* Index mask of valid equality operators 当前有效等式运算符的索引掩码 */
  Index sPk;                  /* A fake index object for the primary key 一个主键的伪造的索引对象 */
  tRowcnt aiRowEstPk[2];      /* The aiRowEst[] value for the sPk index sPk索引的aiRowEst[]值 */
  int aiColumnPk = -1;        /* The aColumn[] value for the sPk index sPk索引的aColumn[]值 */
  int wsFlagMask;             /* Allowed flags in pCost->plan.wsFlag 在pCost->plan.wsFlag允许的标志 */

  /* Initialize the cost to a worst-case value 初始化成本为worst-case值 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  memset(pCost, 0, sizeof(*pCost));
  pCost->rCost = SQLITE_BIG_DBL;

  /* If the pSrc table is the right table of a LEFT JOIN then we may not
  ** use an index to satisfy IS NULL constraints on that table.  This is
  ** because columns might end up being NULL if the table does not match -
  ** a circumstance which the index cannot help us discover.  Ticket #2177.
<<<<<<< HEAD
  如果pSrc表是左连接，我们就不能让索引为空在这个表上，这是因为如果表不匹配，
  列可能最终会被赋NULL值，这种情况下索引就不能帮助我们进行查找。
=======
  **
  ** 如果pSrc表示一个左连接的右表，那么我们在那个表上可能不使用索引在IS NULL约束上。
  ** 这是因为如果表与索引不能帮助我们发现一种情况向匹配是，那么列可能以NULL结尾。 Ticket #2177.
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  //
  if( pSrc->jointype & JT_LEFT ){
    idxEqTermMask = WO_EQ|WO_IN;
  }else{
    idxEqTermMask = WO_EQ|WO_IN|WO_ISNULL;
  }

  if( pSrc->pIndex ){
<<<<<<< HEAD
    /* 索引BY子句指定了一个特定的索引来使用 */
=======
    /* An INDEXED BY clause specifies a particular index to use 一个INDEXED BY子句指定使用一个特别的索引 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    pIdx = pProbe = pSrc->pIndex;
    wsFlagMask = ~(WHERE_ROWID_EQ|WHERE_ROWID_RANGE);
    eqTermMask = idxEqTermMask;
  }else{
    /* There is no INDEXED BY clause.  Create a fake Index object in local
    ** variable sPk to represent the rowid primary key index.  Make this
    ** fake index the first in a chain of Index objects with all of the real
    ** indices to follow 
<<<<<<< HEAD
    这里没有索引BY子句。在局部变量创建一个虚的索引对象
    sPk代表rowid主键索引。
    使这个虚的索引对象的第一个索引对象的真实。
    */
    Index *pFirst;                  /* 表中第一个真的索引对象 */
=======
    ** 
    ** 没有INDEXED BY子句。在局部变量sPk中创建一个表示rowid主键的伪索引。
    ** 使这个伪索引处于一连串真实索引对象的第一个位置
    */
    Index *pFirst;                  /* First of real indices on the table 在表中真实索引的第一个 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    memset(&sPk, 0, sizeof(Index));
    sPk.nColumn = 1;
    sPk.aiColumn = &aiColumnPk;
    sPk.aiRowEst = aiRowEstPk;
    sPk.onError = OE_Replace;
    sPk.pTable = pSrc->pTab;
    aiRowEstPk[0] = pSrc->pTab->nRowEst;
    aiRowEstPk[1] = 1;
    pFirst = pSrc->pTab->pIndex;
    if( pSrc->notIndexed==0 ){
      /* The real indices of the table are only considered if the
      ** NOT INDEXED qualifier is omitted from the FROM clause
<<<<<<< HEAD
      如果FROM子句没有限定符，则只考虑实际的索引 */
=======
      ** 表的真实索引只是在邋NOT INDEXED限定在FROM子句中被删除时才考虑。
      */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
      sPk.pNext = pFirst;
    }
    pProbe = &sPk;
    wsFlagMask = ~(
        WHERE_COLUMN_IN|WHERE_COLUMN_EQ|WHERE_COLUMN_NULL|WHERE_COLUMN_RANGE
    );
    eqTermMask = WO_EQ|WO_IN;
    pIdx = 0;
  }

  /* Loop over all indices looking for the best one to use
<<<<<<< HEAD
  遍历其所有索引,找到一个代价最小的索引
=======
  ** 循环所有的索引来查找最好的一个来使用
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  for(; pProbe; pIdx=pProbe=pProbe->pNext){ //循环所有索引
    const tRowcnt * const aiRowEst = pProbe->aiRowEst;
<<<<<<< HEAD
    double cost;                /* 使用pProbe的代价 */
    double nRow;                /* 预计结果集中的记录数 */
    double log10N = (double)1;  /*十进制汇总 (不精确) */
    int rev;                    /* 逆向正确扫描 */
=======
    double cost;                /* Cost of using pProbe pProbe使用的代价 */
    double nRow;                /* Estimated number of rows in result set 在结果集中估计行数 */
    double log10N = (double)1;  /* base-10 logarithm of nRow (inexact) nRow的以10为底的对数(不精确的) */
    int rev;                    /* True to scan in reverse order 在倒序中正确的扫描 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    int wsFlags = 0;
    Bitmask used = 0;

    /* The following variables are populated based on the properties of
    ** index being evaluated. They are then used to determine the expected
    ** cost and number of rows returned.
<<<<<<< HEAD
    **填充下面的变量被评估的基于索引的属性。
    他们然后使用确定的预期代价和返回的记录数。
=======
    **
    ** 下列的变量是基于已评估的索引的性能来填充的。他们经常决定返回预期的代价和行数。
    **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    **  nEq: 
    **    Number of equality terms that can be implemented using the index.
    **    In other words, the number of initial fields in the index that
    **    are used in == or IN or NOT NULL constraints of the WHERE clause.
<<<<<<< HEAD
    **数字相等可以用于索引来实现。
    换句话说,最初的数字引用于==
    或在WHERE子句或NOT NULL约束
=======
    **
    **  nEq:
    **	 能使用索引的等式terms的个数。换句话说，在索引中的初始化的字段数用于WHERE子句中的==或IN或NOT NULL
    **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    **  nInMul:  
    **    The "in-multiplier". This is an estimate of how many seek operations 
    **    SQLite must perform on the index in question. For example, if the 
    **    WHERE clause is:
    **“in-multiplier”。这是一个估计有多少操作的SQLite必须执行索引的问题，
    例如 where子句
    **      WHERE a IN (1, 2, 3) AND b IN (4, 5, 6)
    **
    **    SQLite must perform 9 lookups on an index on (a, b), so nInMul is 
    **    set to 9. Given the same schema and either of the following WHERE 
    **    clauses:
    **SQLite必须执行9次查找在吧，a，b表中。所以nInMul设为9。
    下面给出一个相同查找的where子句
    **      WHERE a =  1
    **      WHERE a >= 2
    **
    **    nInMul is set to 1.
    **
    **    If there exists a WHERE term of the form "x IN (SELECT ...)", then 
    **    the sub-select is assumed to return 25 rows for the purposes of 
    **    determining nInMul.
<<<<<<< HEAD
    **如果这里存在一个where子句（x IN (SELECT ...))。那么子查询将设为25行,即nInMul的值
=======
    **
    **
    **  nInMul:
    **    "in-multiplier".这是估计SQLite在讨论中的索引上执行了多少搜索操作。
    **	 例如，如果WHERE子句是;
    **      WHERE a IN (1, 2, 3) AND b IN (4, 5, 6)
    **    SQLite必须在(a, b)上的索引执行9次查找，因此nInMul设为9.下面给一个相似的例子;
    **      WHERE a =  1
    **      WHERE a >= 2
    **    nInMul设置为1.
    **
    **    如果存在一个形式为"x IN (SELECT ...)"的WHERE term，那么子查询为了确定nInMul就假定返回25行。
    **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    **  bInEst:  
    **    Set to true if there was at least one "x IN (SELECT ...)" term used 
    **    in determining the value of nInMul.  Note that the RHS of the
    **    IN operator must be a SELECT, not a value list, for this variable
    **    to be true.
<<<<<<< HEAD
    **设置为真,如果至少有一个查询子句术语在决定nInMul的值。
    注意,在操作符必须是一个选择,而不是一个值列表,这个变量是真实的。
=======
    **
    **  bInEst:  
    **    如果至少有一个"x IN (SELECT ...)" term用于决定nInMul的值，那么就设置为TRUE.
    **    注意:IN运算符的右边对于这个变量必须是一个真实的SELECT，而不是一个值列表。
    **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    **  rangeDiv:
    **    An estimate of a divisor by which to reduce the search space due
    **    to inequality constraints.  In the absence of sqlite_stat3 ANALYZE
    **    data, a single inequality reduces the search space to 1/4rd its
    **    original size (rangeDiv==4).  Two inequalities reduce the search
    **    space to 1/16th of its original size (rangeDiv==16).
<<<<<<< HEAD
    **估计的一个因子,减少搜索空间取决于不等式约束。
    在缺乏sqlite_stat3分析数据的情况下,
    单一不平等的降低搜索空间是原始大小1/4(rangeDiv = = 4)。
    两个不平等降低搜索空间大概的原始大小的1/16(rangeDiv = = 16)
=======
    **
    **  rangeDiv:
    **    一个由于不等式约束而减少搜索空间的因子评估。
    **    缺少sqlite_stat3 ANALYZE的数据，一个单独的不等式把搜索空间减少到原始的大小的1/4(rangeDiv==4).
    **    两个不等式把搜索空间减少到原始大小的1/16(rangeDiv==16).
    **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    **  bSort:   
    **    Boolean. True if there is an ORDER BY clause that will require an 
    **    external sort (i.e. scanning the index being evaluated will not 
    **    correctly order records).
<<<<<<< HEAD
    **布尔值为真，如果有一个ORDER BY子句,需要一个外部排序(即扫描索引被评估不正确排序记录)。
=======
    **
    **  bSort:  
    **    Boolean类型.如果有一个ORDER BY子句要求一个外部排序(也就是说，扫描评估的索引将不能正确地排序记录)则返回TRUE
    **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    **  bLookup: 
    **    Boolean. True if a table lookup is required for each index entry
    **    visited.  In other words, true if this is not a covering index.
    布尔值为真，如果一个表查找需要访问每个索引项。
    换句话说,如果这不是一个真正的覆盖索引。
    **    This is always false for the rowid primary key index of a table.
    **    For other indexes, it is true unless all the columns of the table
    **    used by the SELECT statement are present in the index (such an
    **    index is sometimes described as a covering index).
    这总是错误的rowid主键索引表。
    其他索引,这是真的,除非使用的表的所有列的SELECT语句存在索引
    (索引有时被描述为一个覆盖索引)。
    **    For example, given the index on (a, b), the second of the following 
    **    two queries requires table b-tree lookups in order to find the value
    **    of column c, but the first does not because columns a and b are
    **    both available in the index.
    **例如，给出表a，b。第二个查询需要建立b树查找，为了找到列c的值,
    但第一查询并不需要。因为列a和b都是可用的索引。
    **             SELECT a, b    FROM tbl WHERE a = 1;
    **             SELECT a, b, c FROM tbl WHERE a = 1;
    **
    **  bLookup: 
    **    Boolean类型.如果一个表查询需要访问每一个索引条目，则bLookup就是TRUE.
    **    换句话说，如果这不是一个覆盖索引，则返回TRUE.对于一个表的rowid主键索引总是错误的。
    **    对于其他的索引，
    **    除非在SELECT命令中表的所有列都出现在索引中(这种索引有时被说成是一个覆盖索引)，那么bLookup就是TRUE
    **    例如，在(a, b)上有索引，下面两个查询语句请求表b-tree查找为了查找列c的值，
    **    并且因为列a和b都是在索引中的变量，所以第一个语句不能请求到。
    **             SELECT a, b    FROM tbl WHERE a = 1;
    **             SELECT a, b, c FROM tbl WHERE a = 1;
    **
    **
    */
<<<<<<< HEAD
    int nEq;                      /* 可以使用索引的等值表达式的个数*/
    int bInEst = 0;               /* 如果存在 x IN (SELECT...),则设为true*/
    int nInMul = 1;               /* 处理in子句 */
    double rangeDiv = (double)1;  /* 估计减少搜索空间 */
    int nBound = 0;               /* 估计需要扫描的表 */
    int bSort = !!pOrderBy;       /* 当需要外部排序时为真 */
    int bDist = !!pDistinct;      /* 当索引不是distinct时为真 */
    int bLookup = 0;              /* 当不是覆盖索引为真 */
    WhereTerm *pTerm;             /* 一个WHERE子句 */
#ifdef SQLITE_ENABLE_STAT3
    WhereTerm *pFirstTerm = 0;    /* 第一个查询匹配的索引*/
#endif

    /* Determine the values of nEq and nInMul 
    计算nEq和nInMul值
    */
=======
    int nEq;                      /* Number of == or IN terms matching index 匹配索引的==或IN terms数目 */
    int bInEst = 0;               /* True if "x IN (SELECT...)" seen 如果发现"x IN (SELECT...)"则为TRUE */
    int nInMul = 1;               /* Number of distinct equalities to lookup 查找的DISTINCT等式的数目 */
    double rangeDiv = (double)1;  /* Estimated reduction in search space 估计在搜索空间上的减少量 */
    int nBound = 0;               /* Number of range constraints seen 发现的范围约束数目 */
    int bSort = !!pOrderBy;       /* True if external sort required 如果需要外部查询则为TRUE */
    int bDist = !!pDistinct;      /* True if index cannot help with DISTINCT 如果索引对DISTINCT没有帮助，则为TRUE */
    int bLookup = 0;              /* True if not a covering index 如果不是一个覆盖索引则为TRUE */
    WhereTerm *pTerm;             /* A single term of the WHERE clause WHERE子句的一个单独的term */
#ifdef SQLITE_ENABLE_STAT3
    WhereTerm *pFirstTerm = 0;    /* First term matching the index 匹配索引的第一个term */
#endif

    /* Determine the values of nEq and nInMul 确定nEq和nInMul的值  */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    for(nEq=0; nEq<pProbe->nColumn; nEq++){
      int j = pProbe->aiColumn[nEq];
      pTerm = findTerm(pWC, iCur, j, notReady, eqTermMask, pIdx);
      if( pTerm==0 ) break;
      wsFlags |= (WHERE_COLUMN_EQ|WHERE_ROWID_EQ);
      testcase( pTerm->pWC!=pWC );
      if( pTerm->eOperator & WO_IN ){
        Expr *pExpr = pTerm->pExpr;
        wsFlags |= WHERE_COLUMN_IN;
        if( ExprHasProperty(pExpr, EP_xIsSelect) ){
          /* "x IN (SELECT ...)":  Assume the SELECT returns 25 rows "x IN (SELECT ...)": 假定SELECT返回25行 */
          nInMul *= 25;
          bInEst = 1;
        }else if( ALWAYS(pExpr->x.pList && pExpr->x.pList->nExpr) ){
          /* "x IN (value, value, ...)" */
          nInMul *= pExpr->x.pList->nExpr;
        }
      }else if( pTerm->eOperator & WO_ISNULL ){
        wsFlags |= WHERE_COLUMN_NULL;
      }
#ifdef SQLITE_ENABLE_STAT3
      if( nEq==0 && pProbe->aSample ) pFirstTerm = pTerm;
#endif
      used |= pTerm->prereqRight;
    }
 
    /* If the index being considered is UNIQUE, and there is an equality 
    ** constraint for all columns in the index, then this search will find
    ** at most a single row. In this case set the WHERE_UNIQUE flag to 
    ** indicate this to the caller.
<<<<<<< HEAD
    **如果是唯一索引,有一个相等约束在所有索引集,
    那么这个搜索最多发现单行。
    在这种情况下设置WHERE_UNIQUE标志来表示这给调用者
    ** Otherwise, if the search may find more than one row, test to see if
    ** there is a range constraint on indexed column (nEq+1) that can be 
    ** optimized using the index. 
  否则,如果搜索可能会超过一行,
  测试是否有一系列限制索引列(nEq + 1),可以使用索引进行了优化。
=======
    **
    ** 如果索引是UNIQUE索引，并且对于在索引中的所有列都有一个等式约束，那么这个索引至多将发现一个单独的行
    ** 在这种情况下设置WHERE_UNIQUE标志给调用者指出这种情况
    **
    ** Otherwise, if the search may find more than one row, test to see if
    ** there is a range constraint on indexed column (nEq+1) that can be 
    ** optimized using the index. 
    **
    ** 否则，如果查询可能查找到的结果不只一行，测试是否在索引列上有一个范围索引可以使用索引进行优化。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    //计算nBound值
    if( nEq==pProbe->nColumn && pProbe->onError!=OE_None ){
      testcase( wsFlags & WHERE_COLUMN_IN );
      testcase( wsFlags & WHERE_COLUMN_NULL );
      if( (wsFlags & (WHERE_COLUMN_IN|WHERE_COLUMN_NULL))==0 ){
        wsFlags |= WHERE_UNIQUE;
      }
    }else if( pProbe->bUnordered==0 ){
      int j = (nEq==pProbe->nColumn ? -1 : pProbe->aiColumn[nEq]);
      if( findTerm(pWC, iCur, j, notReady, WO_LT|WO_LE|WO_GT|WO_GE, pIdx) ){
        WhereTerm *pTop = findTerm(pWC, iCur, j, notReady, WO_LT|WO_LE, pIdx);
        WhereTerm *pBtm = findTerm(pWC, iCur, j, notReady, WO_GT|WO_GE, pIdx);
        //估计范围条件的代价
        whereRangeScanEst(pParse, pProbe, nEq, pBtm, pTop, &rangeDiv);
        if( pTop ){
          nBound = 1;
          wsFlags |= WHERE_TOP_LIMIT;
          used |= pTop->prereqRight;
          testcase( pTop->pWC!=pWC );
        }
        if( pBtm ){
          nBound++;
          wsFlags |= WHERE_BTM_LIMIT;
          used |= pBtm->prereqRight;
          testcase( pBtm->pWC!=pWC );
        }
        wsFlags |= (WHERE_COLUMN_RANGE|WHERE_ROWID_RANGE);
      }
    }

    /* If there is an ORDER BY clause and the index being considered will
    ** naturally scan rows in the required order, set the appropriate flags
    ** in wsFlags. Otherwise, if there is an ORDER BY clause but the index
    ** will scan rows in a different order, set the bSort variable.  
<<<<<<< HEAD
    如果有一个ORDER BY子句和索引正在考虑自然会扫描行所需的顺序,
    在wsFlags设置相应的标志。
    否则,如果有一个ORDER BY子句但该索引将扫描行顺序不同,设置bSort变量。
=======
    **
    ** 如果有一个ORDER BY子句并且索引将在相应的序列中扫描行，在wsFlags中设置适当的标志。
    ** 否则如果有一个ORDER BY子句但是索引将在其他序列上扫描行，设置bSort变量
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    if( isSortingIndex(
          pParse, pWC->pMaskSet, pProbe, iCur, pOrderBy, nEq, wsFlags, &rev)
    ){
      bSort = 0;
      wsFlags |= WHERE_ROWID_RANGE|WHERE_COLUMN_RANGE|WHERE_ORDERBY;
      wsFlags |= (rev ? WHERE_REVERSE : 0);
    }

    /* If there is a DISTINCT qualifier and this index will scan rows in
    ** order of the DISTINCT expressions, clear bDist and set the appropriate
    ** flags in wsFlags.
<<<<<<< HEAD
    如果有限定符DISTINCT,该所索引将扫描行用不同的DISTINCT表达式,
    明确wsFlags bDist和设置适当的标志。
     */
=======
    **
    ** 如果有一个DISTINCT限定符并且这个索引将在DISTINCT表达式的序列中扫描行，
    ** 清除bDist并且在wsFlags中设定适当的标志。
    */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    if( isDistinctIndex(pParse, pWC, pProbe, iCur, pDistinct, nEq)
     && (wsFlags & WHERE_COLUMN_IN)==0
    ){
      bDist = 0;
      wsFlags |= WHERE_ROWID_RANGE|WHERE_COLUMN_RANGE|WHERE_DISTINCT;
    }

    /* If currently calculating the cost of using an index (not the IPK
    ** index), determine if all required column data may be obtained without 
    ** using the main table (i.e. if the index is a covering
    ** index for this query). If it is, set the WHERE_IDX_ONLY flag in
<<<<<<< HEAD
    ** wsFlags. Otherwise, set the bLookup variable to true.  
    //如果目前的计算使用索引(不是IPK索引)代价,
    确定所需的所有列数据可以获得不使用主表(即如果该查询的索引是一个覆盖索引)。
    如果是,在wsFlags设置 WHERE_IDX_ONLY标志。
    否则,设置bLookup变量为true。
    */
    if( pIdx && wsFlags ){
      Bitmask m = pSrc->colUsed;
      int j;
      for(j=0; j<pIdx->nColumn; j++){
=======
    ** wsFlags. Otherwise, set the bLookup variable to true.
    **
    ** 如果计算当前使用一个索引的代价(不是IPK索引)，
    ** 决定如果不通过使用主表而获得所有的请求列数据(也就是说，如果对于这个查询索引是一个覆盖索引)。
    ** 如果它是一个覆盖索引，在wsFlags中设置WHERE_IDX_ONLY标志。
    ** 否则，把变量bLookup设置为TRUE.
    */
    if( pIdx && wsFlags ){ //如果索引存在
      Bitmask m = pSrc->colUsed; //表中使用索引的列
      int j; //循环计数器
      for(j=0; j<pIdx->nColumn; j++){  
	  	//遍历所有使用该索引的列，判断是否所有列都在索引中
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
        int x = pIdx->aiColumn[j];
        if( x<BMS-1 ){
          m &= ~(((Bitmask)1)<<x);
        }
      }
      if( m==0 ){	//如果所有都在索引中
        wsFlags |= WHERE_IDX_ONLY; //设置WHERE_IDX_ONLY，标志是一个覆盖索引
      }else{
        bLookup = 1; //不是一个覆盖索引
      }
    }

    /*
    ** Estimate the number of rows of output.  For an "x IN (SELECT...)"
    ** constraint, do not let the estimate exceed half the rows in the table.
<<<<<<< HEAD
    估计的输出的行数。对于一个“x(选择…)“约束,不要让估计超过一半的表中的行。
=======
    **
    ** 估计数据的行数。对于一个"x IN (SELECT...)"约束，不要让估计值超过表中行的一半。
    **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    nRow = (double)(aiRowEst[nEq] * nInMul);
    if( bInEst && nRow*2>aiRowEst[0] ){
      nRow = aiRowEst[0]/2;
      nInMul = (int)(nRow / aiRowEst[nEq]);
    }

#ifdef SQLITE_ENABLE_STAT3
    /* If the constraint is of the form x=VALUE or x IN (E1,E2,...)
    ** and we do not think that values of x are unique and if histogram
    ** data is available for column x, then it might be possible
    ** to get a better estimate on the number of rows based on
    ** VALUE and how common that value is according to the histogram.
<<<<<<< HEAD
    如果表的约束是x = x值或(E1,E2,…),我们不认为x的值是唯一的,
    如果直方图数据可列x,那么它可能得到更好的估计基于值的行数,
    以及常见的值是根据直方图。
=======
    **
    ** 如果约束是x=VALUE or x IN (E1,E2,...)这种形式
    ** 并且我们不认为x的值是唯一的并且如果对于x列来说直方图数据是变量，
    ** 那么它可能基于VALLUE在行数上获得一个更好的估计值，如何根据直方图得到共同的值。
    **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    if( nRow>(double)1 && nEq==1 && pFirstTerm!=0 && aiRowEst[1]>1 ){
      assert( (pFirstTerm->eOperator & (WO_EQ|WO_ISNULL|WO_IN))!=0 );
      if( pFirstTerm->eOperator & (WO_EQ|WO_ISNULL) ){
        testcase( pFirstTerm->eOperator==WO_EQ );
        testcase( pFirstTerm->eOperator==WO_ISNULL );
        whereEqualScanEst(pParse, pProbe, pFirstTerm->pExpr->pRight, &nRow);
      }else if( bInEst==0 ){
        assert( pFirstTerm->eOperator==WO_IN );
        whereInScanEst(pParse, pProbe, pFirstTerm->pExpr->x.pList, &nRow);
      }
    }
#endif /* SQLITE_ENABLE_STAT3 */

    /* Adjust the number of output rows and downward to reflect rows
    ** that are excluded by range constraints.
<<<<<<< HEAD
    调整输出行和降序行所排除的范围限制。
=======
    **
    ** 调整输出行的数目并且向下反映通过范围约束拒绝的行。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    nRow = nRow/rangeDiv;
    if( nRow<1 ) nRow = 1;

    /* Experiments run on real SQLite databases show that the time needed
    ** to do a binary search to locate a row in a table or index is roughly
    ** log10(N) times the time to move from one row to the next row within
    ** a table or index.  The actual times can vary, with the size of
    ** records being an important factor.  Both moves and searches are
    ** slower with larger records, presumably because fewer records fit
    ** on one page and hence more pages have to be fetched.
<<<<<<< HEAD
    **实验运行在真正的SQLite数据库显示：
    所需的时间做一个二进制搜索定位表中的一行或索引log10(N)倍时间从一行到下一行在一个表或索引。
    实际时间可能会有所不同,记录的大小是一个重要因素。
    记录多则移动和搜索都是慢的,大概是因为少记录适合在一个页面上,因此必须获取更多页。
=======
    **
    ** 试验运行在真实的SQLite数据显示在表或索引中做一个二分查找来定位一行所需的时间，
    ** 在表或索引中，从一行移动到下一行的时间大致为log10(N).
    ** 随着记录的数据成为一个重要因素，实际时间可以变化。
    ** 对于数量多的记录，移动和查找都比较慢，
    ** 可能是因为在一个页面中装入的记录较少，因此需要获取很多的页面
    **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    ** The ANALYZE command and the sqlite_stat1 and sqlite_stat3 tables do
    ** not give us data on the relative sizes of table and index records.
    ** So this computation assumes table records are about twice as big
    ** as index records
<<<<<<< HEAD
    分析命令和sqlite_stat1 sqlite_stat3表不给我们数据表和索引记录的相对大小。
    所以这个假定表记录大约两倍索引记录
=======
    **
    ** ANALYZE命令和sqlite_stat1，sqlite_stat3表在表和索引的记录上没有给我们提供相对大小。
    ** 所以这个计算假设表记录大概是索引记录的两倍。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    if( (wsFlags & WHERE_NOT_FULLSCAN)==0 ){
      /* The cost of a full table scan is a number of move operations equal
      ** to the number of rows in the table.
<<<<<<< HEAD
      **全表扫描的代价是一个数据移动操作相当于表中的行数。
=======
      **
      ** 一个全表扫描的代价是一定数量的移动操作相当于在表中的行数
      **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
      ** We add an additional 4x penalty to full table scans.  This causes
      ** the cost function to err on the side of choosing an index over
      ** choosing a full scan.  This 4x full-scan penalty is an arguable
      ** decision and one which we expect to revisit in the future.  But
      ** it seems to be working well enough at the moment.
<<<<<<< HEAD
      我们添加一个额外的4倍代价来全表扫描。
      这导致的代价函数宁在选择一个索引在选择一个完整的扫描。
      这4倍全扫描代价是一个有争议的决定,我们希望重新考虑未来。
      但目前似乎工作得足够好
=======
      **
      ** 添加一个附加的4x惩罚用于全表扫描。这会引起代价函数宁可选择一个索引来代替全表扫描。
      ** 这个4x全表扫描惩罚是一个可论证的决定并且希望在函数中再访问。
      ** 但是它看起来好像此时运行的还不错。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
      */
      cost = aiRowEst[0]*4;
    }else{
      log10N = estLog(aiRowEst[0]);
      cost = nRow;
      if( pIdx ){
        if( bLookup ){
          /* For an index lookup followed by a table lookup:
          **    nInMul index searches to find the start of each index range
          **  + nRow steps through the index
          **  + nRow table searches to lookup the table entry using the rowid
<<<<<<< HEAD
          一个索引查找在表查找后：
          nInMul索引搜索是每个索引范围
          + nRow步骤通过索引+ nRow表搜索查找使用rowid的表条目
=======
          **
          ** 对于一个表查找后的索引查找:
          ** + nRow逐句通过索引
          ** + nRow表检索来查使用rowid找表项目
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
          */
          cost += (nInMul + nRow)*log10N;
        }else{
          /* For a covering index:
          **     nInMul index searches to find the initial entry 
          **   + nRow steps through the index
<<<<<<< HEAD
          一个覆盖索引：
          nInMul索引搜索是最初的入口通过索引+ nRow步骤
=======
          **
          ** 对于一个覆盖索引:
          **     nInMul索引检索来查找最初的项
          **   + nRow逐句通过索引
          **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
          */
          cost += nInMul*log10N;
        }
      }else{
        /* For a rowid primary key lookup:
        **    nInMult table searches to find the initial entry for each range
        **  + nRow steps through the table
<<<<<<< HEAD
        rowid主键查找：
        nInMult表搜索是初始条目通过表范围+ nRow每个步骤
=======
        **
        ** 对于一个rowid主键查找:
        **	nInMul表检索来查找每个范围的最初的项
        **  + nRow逐句通过表
        **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
        */
        cost += nInMul*log10N;
      }
    }

    /* Add in the estimated cost of sorting the result.  Actual experimental
    ** measurements of sorting performance in SQLite show that sorting time
    ** adds C*N*log10(N) to the cost, where N is the number of rows to be 
    ** sorted and C is a factor between 1.95 and 4.3.  We will split the
    ** difference and select C of 3.0.
<<<<<<< HEAD
    添加排序结果的代价估算。
    在SQLite排序性能的实际实验测量表明,排序时间增加了C * N * log10(N)的代价,
    其中N是要排序的行数和C是一个因素在1.95和4.3之间。我们将忽略区别，选择C 3.0。
=======
    **
    ** 添加排序结果的估计成本。
    ** 在SQLite中排序性能的实际实验的测量表明排序时间添加C*N*log10(N)到代价中，
    ** 其中N是需要排序的行数，C是一个在1.95到4.3之间的因素。
    ** 我们将分离区别和选择值为3.0的C
    **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    if( bSort ){
      cost += nRow*estLog(nRow)*3;
    }
    if( bDist ){
      cost += nRow*estLog(nRow)*3;
    }

<<<<<<< HEAD
    /**** Cost of using this index has now been computed ****/
    //使用该索引的代价已经是可计算的
=======
    /**** Cost of using this index has now been computed 现在计算使用这个索引的代价 ****/

>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    /* If there are additional constraints on this table that cannot
    ** be used with the current index, but which might lower the number
    ** of output rows, adjust the nRow value accordingly.  This only 
    ** matters if the current index is the least costly, so do not bother
    ** with this step if we already know this index will not be chosen.
    ** Also, never reduce the output row count below 2 using this step.
<<<<<<< HEAD
    **如果有额外的限制这个表,不能用于当前索引,
    但这可能会降低输出的行数,相应地调整nRow值。
    这只很重要如果当前索引是最昂贵的,
    所以不要打扰这一步如果我们已经知道这个索引不会被选中。
    也从来没有减少输出行数低于2使用这个步骤。
=======
    **
    ** 如果在这个表上有附加的约束不能被用在当前索引上，
    ** 但是这个约束可能会减少输出行的数目，那么就调整相应的nRow值。
    ** 这只是因为当前索引是最小代价，索引如果我们都知道这个索引都不会被选中，那么就不要理会这一步。
    ** 另外，使用这步从不把输出行数减少到低于2。
    **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    ** It is critical that the notValid mask be used here instead of
    ** the notReady mask.  When computing an "optimal" index, the notReady
    ** mask will only have one bit set - the bit for the current table.
    ** The notValid mask, on the other hand, always has all bits set for
    ** tables that are not in outer loops.  If notReady is used here instead
    ** of notValid, then a optimal index that depends on inner joins loops
    ** might be selected even when there exists an optimal index that has
    ** no such dependency.
<<<<<<< HEAD
    关键是也许notValid掩码在这里使用的notReady掩码。
    当寻找一个最佳索引,notReady掩码也许只会有一个设置为当前表。
    notValid掩码,另一方面,总是有一些设置表,不在外循环。
    如果也许是这里使用notReady而不是notValid,
    那么最优索引这取决于内在连接循环甚至可能被选中时,
    当存在一个最佳的指数,没有这样的依赖。
    */
    if( nRow>2 && cost<=pCost->rCost ){
      int k;                       /* 循环计数器*/
      int nSkipEq = nEq;           /* =约束跳跃*/
      int nSkipRange = nBound;     /* <约束跳跃*/
      Bitmask thisTab;             /* 设置pSrc */
=======
    **
    ** 使用无效的掩码来替换未准备好的掩码是很紧要的。
    ** 当计算一个"最优的"索引，未准备好的掩码只有一个位设置为当前表。
    ** 另一方面，无效掩码总是所有位设置为没在外部循环的表。
    ** 如果用未准备好的掩码来代替无效的掩码，
    ** 那么选择一个依赖内部连接循环的最佳的索引即使当存在一个没有依赖的最佳的索引
    */
    if( nRow>2 && cost<=pCost->rCost ){
      int k;                       /* Loop counter 循环计数器 */
      int nSkipEq = nEq;           /* Number of == constraints to skip 需要跳过的==约束数 */
      int nSkipRange = nBound;     /* Number of < constraints to skip 需要跳过的<约束数 */
      Bitmask thisTab;             /* Bitmap for pSrc 用于pSrc的位图 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949

      thisTab = getMask(pWC->pMaskSet, iCur);
      for(pTerm=pWC->a, k=pWC->nTerm; nRow>2 && k; k--, pTerm++){
        if( pTerm->wtFlags & TERM_VIRTUAL ) continue;
        if( (pTerm->prereqAll & notValid)!=thisTab ) continue;
        if( pTerm->eOperator & (WO_EQ|WO_IN|WO_ISNULL) ){
          if( nSkipEq ){
            /* Ignore the first nEq equality matches since the index
<<<<<<< HEAD
            ** has already accounted for these 
            忽略第一个nEq相等匹配指数因为索引已经占了这些
=======
            ** has already accounted for these
            **
            ** 忽略第一个nEq等式匹配自如果索引已经说明了这些
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
            */
            nSkipEq--;
          }else{
            /* Assume each additional equality match reduces the result
<<<<<<< HEAD
            ** set size by a factor of 10 
            假设每个额外的相等匹配结果集大小降低10倍
=======
            ** set size by a factor of 10
            **
            ** 假设每个附加的等式匹配结果集大小降低10倍
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
            */
            nRow /= 10;
          }
        }else if( pTerm->eOperator & (WO_LT|WO_LE|WO_GT|WO_GE) ){
          if( nSkipRange ){
            /* Ignore the first nSkipRange range constraints since the index
<<<<<<< HEAD
            ** has already accounted for these
            忽略第一个nSkipRange相等匹配指数因为索引已经占了这些
             */
=======
            ** has already accounted for these 
            **
            ** 忽略第一个nSkipRange范围约束如果索引已经说明了这些
            */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
            nSkipRange--;
          }else{
            /* Assume each additional range constraint reduces the result
            ** set size by a factor of 3.  Indexed range constraints reduce
            ** the search space by a larger factor: 4.  We make indexed range
            ** more selective intentionally because of the subjective 
            ** observation that indexed range constraints really are more
            ** selective in practice, on average. 
<<<<<<< HEAD
            假设每个额外的约束范围减少了结果集大小的3倍。
            索引范围约束减少搜索空间较大的因素4倍。
            我们故意使索引范围更有选择性,
            因为索引范围约束的主观观察通常更有选择性的在实践中。
=======
            **
            ** 假设每个附加的范围约束把结果集减少了3倍。
            ** 有索引的范围约束把搜索空间减少了4倍。
            ** 我们故意地使范围索引更有选择性，因为平均而言，索引范围约束的主观观察在选择中真的更具有选择性。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
            */
            nRow /= 3;
          }
        }else if( pTerm->eOperator!=WO_NOOP ){
          /* Any other expression lowers the output row count by half 其他的表达式把输出行数减少了一半 */
          nRow /= 2;
        }
      }
      if( nRow<2 ) nRow = 2;
    }


    WHERETRACE((
      "%s(%s): nEq=%d nInMul=%d rangeDiv=%d bSort=%d bLookup=%d wsFlags=0x%x\n"
      "         notReady=0x%llx log10N=%.1f nRow=%.1f cost=%.1f used=0x%llx\n",
      pSrc->pTab->zName, (pIdx ? pIdx->zName : "ipk"), 
      nEq, nInMul, (int)rangeDiv, bSort, bLookup, wsFlags,
      notReady, log10N, nRow, cost, used
    ));

    /* If this index is the best we have seen so far, then record this
    ** index and its cost in the pCost structure.
<<<<<<< HEAD
    如果这个索引是最好的我们已经看到迄今为止,
    然后在pCost记录这个索引和它的代价
=======
    **
    ** 如果这个索引是到目前为止最好的，那么在pCost数据结构中记录这个索引和它的代价。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    if( (!pIdx || wsFlags)
     && (cost<pCost->rCost || (cost<=pCost->rCost && nRow<pCost->plan.nRow))
    ){
      pCost->rCost = cost;
      pCost->used = used;
      pCost->plan.nRow = nRow;
      pCost->plan.wsFlags = (wsFlags&wsFlagMask);
      pCost->plan.nEq = nEq;
      pCost->plan.u.pIdx = pIdx;
    }

    /* If there was an INDEXED BY clause, then only that one index is
    ** considered. 
<<<<<<< HEAD
    如果有一个索引BY子句,那么被认为是唯一索引。
    */
    if( pSrc->pIndex ) break;

    /* Reset masks for the next index in the loop */
    //重置循环中的下一个索引的掩码
=======
    **
    ** 如果有一个INDEXED BY子句，那么只有考虑一个索引。
    */
    if( pSrc->pIndex ) break;

    /* Reset masks for the next index in the loop 为了循环中的下一个索引而重新设置掩码 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    wsFlagMask = ~(WHERE_ROWID_EQ|WHERE_ROWID_RANGE);
    eqTermMask = idxEqTermMask;
  }

  /* If there is no ORDER BY clause and the SQLITE_ReverseOrder flag
  ** is set, then reverse the order that the index will be scanned
  ** in. This is used for application testing, to help find cases
  ** where application behaviour depends on the (undefined) order that
  ** SQLite outputs rows in in the absence of an ORDER BY clause.  
<<<<<<< HEAD
  如果没有设置ORDER BY子句但设置SQLITE_ReverseOrder标志,
  然后反向索引的顺序将被扫描。
  这是用于应用程序测试,帮助找到的情况下,
  这是应用程序的行为取决于(未定义)，SQLite输出行没有一个order BY子句。
=======
  **
  ** 如果没有ORDER BY子句并且设置了SQLITE_ReverseOrder标志，那么索引的反向顺序将被扫描。
  ** 这只用来应用测试，用于帮助查找这种情况--应用程序的行为取决于SQLite在缺少ORDER BY子句的输出行的(未定义的)序列。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  if( !pOrderBy && pParse->db->flags & SQLITE_ReverseOrder ){
    pCost->plan.wsFlags |= WHERE_REVERSE;
  }

  assert( pOrderBy || (pCost->plan.wsFlags&WHERE_ORDERBY)==0 );
  assert( pCost->plan.u.pIdx==0 || (pCost->plan.wsFlags&WHERE_ROWID_EQ)==0 );
  assert( pSrc->pIndex==0 
       || pCost->plan.u.pIdx==0 
       || pCost->plan.u.pIdx==pSrc->pIndex 
  );

  WHERETRACE(("best index is: %s\n", 
    ((pCost->plan.wsFlags & WHERE_NOT_FULLSCAN)==0 ? "none" : 
         pCost->plan.u.pIdx ? pCost->plan.u.pIdx->zName : "ipk")
  ));
  
  bestOrClauseIndex(pParse, pWC, pSrc, notReady, notValid, pOrderBy, pCost);
  bestAutomaticIndex(pParse, pWC, pSrc, notReady, pCost);
  pCost->plan.wsFlags |= eqTermMask;
}

/*
** Find the query plan for accessing table pSrc->pTab. Write the
** best query plan and its cost into the WhereCost object supplied 
** as the last parameter. This function may calculate the cost of
** both real and virtual table scans.
**
** 查找访问表pSrc->pTab的查询计划。在WhereCost对象中写入最好的查询计划和它的代价，并且作为最好的参数传递给bestIndex函数。
** 这个函数可能会计算表扫描和虚拟表扫描的代价
**
*/
static void bestIndex(
<<<<<<< HEAD
  Parse *pParse,              /* 解析上下文*/
  WhereClause *pWC,           /* WHERE子句*/
  struct SrcList_item *pSrc,  /* form查询子句 */
  Bitmask notReady,           /* 不适用于索引的游标掩码 */
  Bitmask notValid,           /* 不可用于任何用途的游标 */
  ExprList *pOrderBy,         /* ORDER BY 子句 */
  WhereCost *pCost            /* 开销最小的查询方法 */
=======
  Parse *pParse,              /* The parsing context 分析上下文 */
  WhereClause *pWC,           /* The WHERE clause WHERE子句 */
  struct SrcList_item *pSrc,  /* The FROM clause term to search 用于查找的FROM子句term */
  Bitmask notReady,           /* Mask of cursors not available for indexing 对于索引无效的游标掩码 */
  Bitmask notValid,           /* Cursors not available for any purpose 游标对于任何情况都无效 */
  ExprList *pOrderBy,         /* The ORDER BY clause ORDER BY子句 */
  WhereCost *pCost            /* Lowest cost query plan 最低代价的查询计划 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
){
#ifndef SQLITE_OMIT_VIRTUALTABLE
  if( IsVirtual(pSrc->pTab) ){ //如果是虚表
    sqlite3_index_info *p = 0;
    bestVirtualIndex(pParse, pWC, pSrc, notReady, notValid, pOrderBy, pCost,&p);
    if( p->needToFreeIdxStr ){
      sqlite3_free(p->idxStr);
    }
    sqlite3DbFree(pParse->db, p);	//释放可能被关联到一个特定数据库连接的内存
  }else //如果不是虚表
#endif
  {
    bestBtreeIndex(pParse, pWC, pSrc, notReady, notValid, pOrderBy, 0, pCost);
  }
}

/*
** Disable a term in the WHERE clause.  Except, do not disable the term
** if it controls a LEFT OUTER JOIN and it did not originate in the ON
** or USING clause of that join.
<<<<<<< HEAD
**禁用一个术语在WHERE子句中。
除了,不要禁用词如果控制左外连接,
它并不是来源于ON或USING子句使用。
=======
**
** 在WHERE子句中禁止一个term.如果它控制一个LEFT OUTER JOIN并且它不来源于那个连接的ON或USING子句时不禁止term.
**
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
** Consider the term t2.z='ok' in the following queries:
**考虑这个连接t2.z=“ok”在下面的查询中
**   (1)  SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.x WHERE t2.z='ok'
**   (2)  SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.x AND t2.z='ok'
**   (3)  SELECT * FROM t1, t2 WHERE t1.a=t2.x AND t2.z='ok'
**
** The t2.z='ok' is disabled in the in (2) because it originates
** in the ON clause.  The term is disabled in (3) because it is not part
** of a LEFT OUTER JOIN.  In (1), the term is not disabled.
<<<<<<< HEAD
**t2.z='ok'是错误的在（2）中，因为它初次出现不在on子句中。
t2.z='ok'是错误的在（3）中，因为它不是左连接的一部分。
在（1）中。它是正确的
** IMPLEMENTATION-OF: R-24597-58655 No tests are done for terms that are
** completely satisfied by indices.
**不做测试的条款完全满意指数
=======
**
** 考虑下面查询中的t2.z='ok'
**   (1)  SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.x WHERE t2.z='ok'
**   (2)  SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.x AND t2.z='ok'
**   (3)  SELECT * FROM t1, t2 WHERE t1.a=t2.x AND t2.z='ok'
** 因为t2.z='ok'起源于ON子句，所以在(2)中禁用t2.z='ok'.
** 因为t2.z='ok'不是LEFT OUTER JOIN的一部分，所以在(3)中禁用t2.z='ok'.
** 在(1)中，term未被禁止。
** 
** IMPLEMENTATION-OF: R-24597-58655 No tests are done for terms that are
** completely satisfied by indices. 
**
** 不做测试的terms是完全可以使用索引的。
**
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
** Disabling a term causes that term to not be tested in the inner loop
** of the join.  Disabling is an optimization.  When terms are satisfied
** by indices, we disable them to prevent redundant tests in the inner
** loop.  We would get the correct results if nothing were ever disabled,
** but joins might run a little slower.  The trick is to disable as much
** as we can without disabling too much.  If we disabled in (1), we'd get
** the wrong answer.  See ticket #813.
<<<<<<< HEAD
禁用一个术语使这个词不会加入的内循环测试。禁用是一个优化。
当条件满足索引,我们禁用它们,防止内部循环冗余测试。
我们可以得到正确的结果如果没有被禁用,但连接可能运行有点慢。
诀窍是禁用一样我们可以没有禁用太多。
如果我们在(1)禁用,我们会得到错误的答案，详见813行
=======
**
** 禁止一个term会引起在连接中的内部循环中不测试term.
** 禁止是一种优化策略。当terms可以使用索引时，我们禁用它来阻止在内部循环中的多余测试。
** 如果没有永远被禁用的term，我们会得到正确的结构，但是连接可能会运行的较慢。
** 诀窍是禁用我们可以禁用的，但不要禁用太多。如果我们在(1)中禁用了，那么我们可能会得到错误的结果。
**
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
*/
static void disableTerm(WhereLevel *pLevel, WhereTerm *pTerm){
  if( pTerm
      && (pTerm->wtFlags & TERM_CODED)==0
      && (pLevel->iLeftJoin==0 || ExprHasProperty(pTerm->pExpr, EP_FromJoin))
  ){
    pTerm->wtFlags |= TERM_CODED;
    if( pTerm->iParent>=0 ){
      WhereTerm *pOther = &pTerm->pWC->a[pTerm->iParent];
      if( (--pOther->nChild)==0 ){
        disableTerm(pLevel, pOther);
      }
    }
  }
}

/*
** Code an OP_Affinity opcode to apply the column affinity string zAff
** to the n registers starting at base. 
<<<<<<< HEAD
**代码一个OP_Affinity操作码和字符串zAff寄存器从基础开始有这密切关系
** As an optimization, SQLITE_AFF_NONE entries (which are no-ops) at the
** beginning and end of zAff are ignored.  If all entries in zAff are
** SQLITE_AFF_NONE, then no code gets generated.
**作为一个优化,SQLITE_AFF_NONE条目(无操作)的开始和结束zAff被忽略。
如果所有条目zAff 状态是SQLITE_AFF_NONE,那么没有代码生成。
** This routine makes its own copy of zAff so that the caller is free
** to modify zAff after this routine returns.
这个例程zAff自己的副本,
以便调用者可以自由地修改后zAff这个例程返回。
=======
**
** 编写一个OP_Affinity操作码应用于把列亲和string类型的zAff映射到从基址到n的寄存器上。
**
** As an optimization, SQLITE_AFF_NONE entries (which are no-ops) at the
** beginning and end of zAff are ignored.  If all entries in zAff are
** SQLITE_AFF_NONE, then no code gets generated.
**
** 作为一个优化，忽略在zAff的开始和结尾中的SQLITE_AFF_NONE条目。
** 如果在zAff所有条目都是SQLITE_AFF_NONE，那么不会生成代码。
**
** This routine makes its own copy of zAff so that the caller is free
** to modify zAff after this routine returns.
**
** 这个程序复制它自己的zAff以便在这个程序返回后调用者可以自由修改zAff。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
*/
static void codeApplyAffinity(Parse *pParse, int base, int n, char *zAff){
  Vdbe *v = pParse->pVdbe;
  if( zAff==0 ){
    assert( pParse->db->mallocFailed );
    return;
  }
  assert( v!=0 );

  /* Adjust base and n to skip over SQLITE_AFF_NONE entries at the beginning
  ** and end of the affinity string.
<<<<<<< HEAD
  调整基础和n,跳过SQLITE_AFF_NONE条目在开始和结束。
=======
  **
  ** 调整基址和n跳过在string亲和性的开始和结尾处的SQLITE_AFF_NONE条目
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  while( n>0 && zAff[0]==SQLITE_AFF_NONE ){
    n--;
    base++;
    zAff++;
  }
  while( n>1 && zAff[n-1]==SQLITE_AFF_NONE ){
    n--;
  }

<<<<<<< HEAD
  /* Code the OP_Affinity opcode if there is anything left to do. 
  代码OP_Affinity操作码,如果有什么需要去做
  */
=======
  /* Code the OP_Affinity opcode if there is anything left to do. 编写OP_Affinity操作码 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  if( n>0 ){
    sqlite3VdbeAddOp2(v, OP_Affinity, base, n);
    sqlite3VdbeChangeP4(v, -1, zAff, n);
    sqlite3ExprCacheAffinityChange(pParse, base, n);
  }
}


/*
** Generate code for a single equality term of the WHERE clause.  An equality
** term can be either X=expr or X IN (...).   pTerm is the term to be 
** coded.
<<<<<<< HEAD
**生成代码为一个相等的WHERE子句。一个相等的术语可以是X = expr或X IN(…)。pTerm是被编译的
** The current value for the constraint is left in register iReg.
**当前值的约束被留在iReg寄存器中
** For a constraint of the form X=expr, the expression is evaluated and its
** result is left on the stack.  For constraints of the form X IN (...)
** this routine sets up a loop that will iterate over all values of X.
X = expr约束的形式,计算表达式,其结果是在堆栈上。
X IN (...)　约束的形式为X这个例程设置一个循环遍历所有的X值
*/
static int codeEqualityTerm(
  Parse *pParse,      /* 解析上下文t */
  WhereTerm *pTerm,   /* TWHERE子句被编译 */
  WhereLevel *pLevel, /* 当前正在运行的ｆｏｒｍ子句*/
  int iTarget         /* 试图把结果离开这个寄存器 */
){
  Expr *pX = pTerm->pExpr;
  Vdbe *v = pParse->pVdbe;
  int iReg;                  /* 寄存器存放结果 */
=======
**
** 为一个WHERE子句的等式term生成代码。一个等式term可以是X=expr或X IN (...).
** pTerm是需要编码的term.
**
** The current value for the constraint is left in register iReg.
**
** 在寄存器iReg中记录约束的当前值
**
** For a constraint of the form X=expr, the expression is evaluated and its
** result is left on the stack.  For constraints of the form X IN (...)
** this routine sets up a loop that will iterate over all values of X.
**
** 对于一个形式为X=expr的约束，计算表达式并且在堆栈中存储它的结果。
** 对于X IN (...)形式的约束，这个程序创建一个循环来遍历X的所有值。
*/
static int codeEqualityTerm(
  Parse *pParse,      /* The parsing context 分析上下文 */
  WhereTerm *pTerm,   /* The term of the WHERE clause to be coded 需要编码的WHERE子句的term */
  WhereLevel *pLevel, /* When level of the FROM clause we are working on FROM子句的等级 */
  int iTarget         /* Attempt to leave results in this register 尝试在这个寄存器中存储结果 */
){
  Expr *pX = pTerm->pExpr;
  Vdbe *v = pParse->pVdbe;
  int iReg;                  /* Register holding results 寄存器保存结果 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949

  assert( iTarget>0 );
  if( pX->op==TK_EQ ){
    iReg = sqlite3ExprCodeTarget(pParse, pX->pRight, iTarget);
  }else if( pX->op==TK_ISNULL ){
    iReg = iTarget;
    sqlite3VdbeAddOp2(v, OP_Null, 0, iReg);
#ifndef SQLITE_OMIT_SUBQUERY
  }else{
    int eType;
    int iTab;
    struct InLoop *pIn;

    assert( pX->op==TK_IN );
    iReg = iTarget;
    eType = sqlite3FindInIndex(pParse, pX, 0);
    iTab = pX->iTable;
    sqlite3VdbeAddOp2(v, OP_Rewind, iTab, 0);
    assert( pLevel->plan.wsFlags & WHERE_IN_ABLE );
    if( pLevel->u.in.nIn==0 ){
      pLevel->addrNxt = sqlite3VdbeMakeLabel(v);
    }
    pLevel->u.in.nIn++;
    pLevel->u.in.aInLoop =
       sqlite3DbReallocOrFree(pParse->db, pLevel->u.in.aInLoop,
                              sizeof(pLevel->u.in.aInLoop[0])*pLevel->u.in.nIn);
    pIn = pLevel->u.in.aInLoop;
    if( pIn ){
      pIn += pLevel->u.in.nIn - 1;
      pIn->iCur = iTab;
      if( eType==IN_INDEX_ROWID ){
        pIn->addrInTop = sqlite3VdbeAddOp2(v, OP_Rowid, iTab, iReg);
      }else{
        pIn->addrInTop = sqlite3VdbeAddOp3(v, OP_Column, iTab, 0, iReg);
      }
      sqlite3VdbeAddOp1(v, OP_IsNull, iReg);
    }else{
      pLevel->u.in.nIn = 0;
    }
#endif
  }
  disableTerm(pLevel, pTerm);
  return iReg;
}
/*
** Generate code that will evaluate all == and IN constraints for an
** index.
<<<<<<< HEAD
**生成代码，这个代码将会为一个索引评价所有==和IN约束
**
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
** For example, consider table t1(a,b,c,d,e,f) with index i1(a,b,c).
** Suppose the WHERE clause is this:  a==5 AND b IN (1,2,3) AND c>5 AND c<10
** The index has as many as three equality constraints, but in this
** example, the third "c" value is an inequality.  So only two 
** constraints are coded.  This routine will generate code to evaluate
** a==5 and b IN (1,2,3).  The current values for a and b will be stored
** in consecutive registers and the index of the first register is returned.
<<<<<<< HEAD
**例如，考虑带有索引i1(a,b,c)的表t1(a,b,c,d,e,f)
**假设WHERE子句是： a==5 AND b IN (1,2,3) AND c>5 AND c<10
**这个索引有3个等式约束。但是在这里例子中。第三个"c"的值是不等关系。
**因此，只会生成两个约束的代码。这个程序将生成代码计算a==5 and b IN(1,2,3)。
**目前，a和b的值将存储在连续的寄存器中，而且返回第一个寄存器的指针。
**
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
** In the example above nEq==2.  But this subroutine works for any value
** of nEq including 0.  If nEq==0, this routine is nearly a no-op.
** The only thing it does is allocate the pLevel->iMem memory cell and
** compute the affinity string.
<<<<<<< HEAD
**在上面示例中nEq==2.但是这个子程序适合于任何nEq值，包含0。
**如果nEq==0，这个程序几乎是空操作。
**而它的唯一的操作就是分配pLevel - > iMem的存储单元和计算关联的字符串
**
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
** This routine always allocates at least one memory cell and returns
** the index of that memory cell. The code that
** calls this routine will use that memory cell to store the termination
** key value of the loop.  If one or more IN operators appear, then
** this routine allocates an additional nEq memory cells for internal
** use.
<<<<<<< HEAD
**这个程序总是分配至少一个内存单元和返回内存单元的指针。
**调用这个程序的代码将使用那块内存单元存储循环的最终键值
**如果出现一个或多个IN操作符，那么这个程序会分配一个额外的nEq内存单元供内部使用。
**
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
** Before returning, *pzAff is set to point to a buffer containing a
** copy of the column affinity string of the index allocated using
** sqlite3DbMalloc(). Except, entries in the copy of the string associated
** with equality constraints that use NONE affinity are set to
** SQLITE_AFF_NONE. This is to deal with SQL such as the following:
**在返回之前，设置*pzAff指向缓冲区，这个缓冲区包含索引的
**列关联的字符串的副本，这个索引是使用sqlite3DbMalloc()分配的。
**除了条目相关联的字符串的副本具有等式约束，这里使用将SQLITE_AFF_NONE表示没关联。
**这是SQL处理，如以下:
**   CREATE TABLE t1(a TEXT PRIMARY KEY, b);
**   SELECT ... FROM t1 AS t2, t1 WHERE t1.a = t2.b;
**
** In the example above, the index on t1(a) has TEXT affinity. But since
** the right hand side of the equality constraint (t2.b) has NONE affinity,
** no conversion should be attempted before using a t2.b value as part of
** a key to search the index. Hence the first byte in the returned affinity
** string in this example would be set to SQLITE_AFF_NONE.
<<<<<<< HEAD
**在上面的例子中。表t1(a)有一个TEXT类型索引。但是因为等式约束右边t2.b没有亲和性。
**在使用t2.b值作为搜索索引的关键的一部分，我们不应该尝试转换。
**因此在这个例子，返回关联的字符串中第一个字节将会设置为SQLITE_AFF_NONE。
*/
static int codeAllEqualityTerms(
  Parse *pParse,        /* 解析上下文 */
  WhereLevel *pLevel,   /*编译FROM嵌套循环 */
  WhereClause *pWC,     /* where子句*/
  Bitmask notReady,     /* 哪一部分FROM没有编译 */
  int nExtraReg,        /* 分配大量额外的寄存器 */
  char **pzAff          /*设置为指向关联的字符串 */
){
  int nEq = pLevel->plan.nEq;   /* 编码 含有== 或者 IN约束的子句 */
  Vdbe *v = pParse->pVdbe;      /*建立vm */
  Index *pIdx;                  /* 索引被用于这个循环*/
  int iCur = pLevel->iTabCur;   /* 表的游标 */
  WhereTerm *pTerm;             /* 一个简单的约束条件 */
  int j;                        /* 循环计数器 */
  int regBase;                  /* 基址寄存器*/
  int nReg;                     /* 分配寄存器数目 */
  char *zAff;                   /* 返回关联字符串 */

  /* This module is only called on query plans that use an index. 
  **这个模块只是用于建议使用索引的查询计划。
  */
  assert( pLevel->plan.wsFlags & WHERE_INDEXED );
  pIdx = pLevel->plan.u.pIdx;

  /* Figure out how many memory cells we will need then allocate them.
  **弄明白我们需要多少存储单元,然后分配它们
  */
  regBase = pParse->nMem + 1;
  nReg = pLevel->plan.nEq + nExtraReg;
  pParse->nMem += nReg;

  zAff = sqlite3DbStrDup(pParse->db, sqlite3IndexAffinityStr(v, pIdx));
  if( !zAff ){
    pParse->db->mallocFailed = 1;
  }

  /* Evaluate the equality constraints
  ** 计算等式约束
  */
  assert( pIdx->nColumn>=nEq );
  for(j=0; j<nEq; j++){
    int r1;
    int k = pIdx->aiColumn[j];
    pTerm = findTerm(pWC, iCur, k, notReady, pLevel->plan.wsFlags, pIdx);
    if( pTerm==0 ) break;
    /* The following true for indices with redundant columns. 
<<<<<<< HEAD
    **对于有冗余的列的索引来说，下面是正确的
=======
    ** Ex: CREATE INDEX i1 ON t1(a,b,a); SELECT * FROM t1 WHERE a=0 AND b=0; 
    **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    ** Ex: CREATE INDEX i1 ON t1(a,b,a); SELECT * FROM t1 WHERE a=0 AND b=0; 
    */
    testcase( (pTerm->wtFlags & TERM_CODED)!=0 );
    testcase( pTerm->wtFlags & TERM_VIRTUAL ); /* EV: R-30575-11662 */
    r1 = codeEqualityTerm(pParse, pTerm, pLevel, regBase+j);
    if( r1!=regBase+j ){
      if( nReg==1 ){
        sqlite3ReleaseTempReg(pParse, regBase);
        regBase = r1;
      }else{
        sqlite3VdbeAddOp2(v, OP_SCopy, r1, regBase+j);
      }
    }
    testcase( pTerm->eOperator & WO_ISNULL );
    testcase( pTerm->eOperator & WO_IN );
    if( (pTerm->eOperator & (WO_ISNULL|WO_IN))==0 ){
      Expr *pRight = pTerm->pExpr->pRight;
      sqlite3ExprCodeIsNullJump(v, pRight, regBase+j, pLevel->addrBrk);
      if( zAff ){
        if( sqlite3CompareAffinity(pRight, zAff[j])==SQLITE_AFF_NONE ){
          zAff[j] = SQLITE_AFF_NONE;
        }
        if( sqlite3ExprNeedsNoAffinityChange(pRight, zAff[j]) ){
          zAff[j] = SQLITE_AFF_NONE;
        }
      }
    }
  }
  *pzAff = zAff;
  return regBase;
}

#ifndef SQLITE_OMIT_EXPLAIN
/*
** This routine is a helper for explainIndexRange() below
<<<<<<< HEAD
**这个的程度是对下面explainIndexRange()函数的一个辅助
**
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
** pStr holds the text of an expression that we are building up one term at a time
** at a time.  This routine adds a new term to the end of the expression.
** Terms are separated by AND so add the "AND" text for second and subsequent
** terms only.
<<<<<<< HEAD
**pStr保存表达式的文本，即我们每次建立的一个术语。
**这个程序给表达式的末尾增加了一个新的术语。
**这些术语由AND分割。因此，只为第二个和后续的术语增加"AND"文本
*/
static void explainAppendTerm(
  StrAccum *pStr,             /* 建立文本表达 */
  int iTerm,                  /*这个例程的索引。从0开始 */
  const char *zColumn,        /* 列名 */
  const char *zOp             /* 操作名 */
=======
**
*/
static void explainAppendTerm(
  StrAccum *pStr,             /* The text expression being built 建立的文本表达式 */
  int iTerm,                  /* Index of this term.  First is zero 这个术语的索引，第一个是0 */
  const char *zColumn,        /* Name of the column 列名 */
  const char *zOp             /* Name of the operator 操作者名 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
){
  if( iTerm ) sqlite3StrAccumAppend(pStr, " AND ", 5);
  sqlite3StrAccumAppend(pStr, zColumn, -1);
  sqlite3StrAccumAppend(pStr, zOp, 1);
  sqlite3StrAccumAppend(pStr, "?", 1);
}

/*
** Argument pLevel describes a strategy for scanning table pTab. This 
** function returns a pointer to a string buffer containing a description
** of the subset of table rows scanned by the strategy in the form of an
** SQL expression. Or, if all rows are scanned, NULL is returned.
<<<<<<< HEAD
** 参数pLevel描述一个扫描表pTab的策略。这个函数返回一个指针，指向一个字符串缓冲区。
** 其中字符串缓冲区包含一个通过以一个SQL表达式形式的策略扫描表的行的子集的描述。
** 或者，如果扫描所有行，返回NULL.
**
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
** For example, if the query:
**
**   SELECT * FROM t1 WHERE a=1 AND b>2;
**
** is run and there is an index on (a, b), then this function returns a
** string similar to:
**
**   "a=? AND b>?"
**
** The returned pointer points to memory obtained from sqlite3DbMalloc().
** It is the responsibility of the caller to free the buffer when it is
** no longer required.
<<<<<<< HEAD
**
** 例如，如果查询:
**   SELECT * FROM t1 WHERE a=1 AND b>2;
** 运行并且有一个在(a, b)上的索引，那么这个函数返回一个类似"a=? AND b>?"的字符串。
** 返回的指针指向从sqlite3DbMalloc()得到的内存。调用者的责任就是当它不再被请求时就释放缓冲区。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
*/
static char *explainIndexRange(sqlite3 *db, WhereLevel *pLevel, Table *pTab){
  WherePlan *pPlan = &pLevel->plan;
  Index *pIndex = pPlan->u.pIdx;
  int nEq = pPlan->nEq;
  int i, j;
  Column *aCol = pTab->aCol;
  int *aiColumn = pIndex->aiColumn;
  StrAccum txt;

  if( nEq==0 && (pPlan->wsFlags & (WHERE_BTM_LIMIT|WHERE_TOP_LIMIT))==0 ){
    return 0;
  }
  sqlite3StrAccumInit(&txt, 0, 0, SQLITE_MAX_LENGTH);
  txt.db = db;
  sqlite3StrAccumAppend(&txt, " (", 2);
  for(i=0; i<nEq; i++){
    explainAppendTerm(&txt, i, aCol[aiColumn[i]].zName, "=");
  }

  j = i;
  if( pPlan->wsFlags&WHERE_BTM_LIMIT ){
    char *z = (j==pIndex->nColumn ) ? "rowid" : aCol[aiColumn[j]].zName;
    explainAppendTerm(&txt, i++, z, ">");
  }
  if( pPlan->wsFlags&WHERE_TOP_LIMIT ){
    char *z = (j==pIndex->nColumn ) ? "rowid" : aCol[aiColumn[j]].zName;
    explainAppendTerm(&txt, i, z, "<");
  }
  sqlite3StrAccumAppend(&txt, ")", 1);
  return sqlite3StrAccumFinish(&txt);
}

/*
** This function is a no-op unless currently processing an EXPLAIN QUERY PLAN
** command. If the query being compiled is an EXPLAIN QUERY PLAN, a single
** record is added to the output to describe the table scan strategy in 
** pLevel.
<<<<<<< HEAD
**如果当前不是处理一个解释查询计划指令，这个函数就是空操作的.
**这个函数是无操作,除非目前处理解释查询计划命令。
**如果查询编译是一个解释查询计划,
**输出是会添加一个记录来描述在pLevel中的表扫描策略
*/
static void explainOneScan(
  Parse *pParse,                  /* 解析上下文*/
  SrcList *pTabList,              /* 这个循环是表的列循环*/
  WhereLevel *pLevel,             /* 扫描并写OP_Explain操作码*/
  int iLevel,                     /* 值得记录输出 */
  int iFrom,                      /* 值得form集输出 */
  u16 wctrlFlags                  /* sqlite3WhereBegin() 的标记*/
){
  if( pParse->explain==2 ){
    u32 flags = pLevel->plan.wsFlags;
    struct SrcList_item *pItem = &pTabList->a[pLevel->iFrom];
<<<<<<< HEAD
    Vdbe *v = pParse->pVdbe;      /* 建立VM  */
    sqlite3 *db = pParse->db;     /* 数据库句柄 */
    char *zMsg;                   /* 文本添加到EQP输出 */
    sqlite3_int64 nRow;           /* 预期的访问通过扫描的行数 */
    int iId = pParse->iSelectId;  /* Select id (left-most output column) */
    int isSearch;                 /* True for a SEARCH. False for SCAN. */
=======
    Vdbe *v = pParse->pVdbe;      /* VM being constructed 创建VM */
    sqlite3 *db = pParse->db;     /* Database handle 数据库句柄 */
    char *zMsg;                   /* Text to add to EQP output 添加到EQP输出的文本 */
    sqlite3_int64 nRow;           /* Expected number of rows visited by scan 通过扫描访问的预期的行数 */
    int iId = pParse->iSelectId;  /* Select id (left-most output column) 选中的id(最左边的输出列) */
    int isSearch;                 /* True for a SEARCH. False for SCAN. 是一个查找则为TRUE，扫描则为FALSE */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949

    if( (flags&WHERE_MULTI_OR) || (wctrlFlags&WHERE_ONETABLE_ONLY) ) return;

    isSearch = (pLevel->plan.nEq>0)
             || (flags&(WHERE_BTM_LIMIT|WHERE_TOP_LIMIT))!=0
             || (wctrlFlags&(WHERE_ORDERBY_MIN|WHERE_ORDERBY_MAX));

    zMsg = sqlite3MPrintf(db, "%s", isSearch?"SEARCH":"SCAN");
    if( pItem->pSelect ){
      zMsg = sqlite3MAppendf(db, zMsg, "%s SUBQUERY %d", zMsg,pItem->iSelectId);
    }else{
      zMsg = sqlite3MAppendf(db, zMsg, "%s TABLE %s", zMsg, pItem->zName);
    }

    if( pItem->zAlias ){
      zMsg = sqlite3MAppendf(db, zMsg, "%s AS %s", zMsg, pItem->zAlias);
    }
    if( (flags & WHERE_INDEXED)!=0 ){
      char *zWhere = explainIndexRange(db, pLevel, pItem->pTab);
      zMsg = sqlite3MAppendf(db, zMsg, "%s USING %s%sINDEX%s%s%s", zMsg, 
          ((flags & WHERE_TEMP_INDEX)?"AUTOMATIC ":""),
          ((flags & WHERE_IDX_ONLY)?"COVERING ":""),
          ((flags & WHERE_TEMP_INDEX)?"":" "),
          ((flags & WHERE_TEMP_INDEX)?"": pLevel->plan.u.pIdx->zName),
          zWhere
      );
      sqlite3DbFree(db, zWhere);
    }else if( flags & (WHERE_ROWID_EQ|WHERE_ROWID_RANGE) ){
      zMsg = sqlite3MAppendf(db, zMsg, "%s USING INTEGER PRIMARY KEY", zMsg);

      if( flags&WHERE_ROWID_EQ ){
        zMsg = sqlite3MAppendf(db, zMsg, "%s (rowid=?)", zMsg);
      }else if( (flags&WHERE_BOTH_LIMIT)==WHERE_BOTH_LIMIT ){
        zMsg = sqlite3MAppendf(db, zMsg, "%s (rowid>? AND rowid<?)", zMsg);
      }else if( flags&WHERE_BTM_LIMIT ){
        zMsg = sqlite3MAppendf(db, zMsg, "%s (rowid>?)", zMsg);
      }else if( flags&WHERE_TOP_LIMIT ){
        zMsg = sqlite3MAppendf(db, zMsg, "%s (rowid<?)", zMsg);
      }
    }
#ifndef SQLITE_OMIT_VIRTUALTABLE
    else if( (flags & WHERE_VIRTUALTABLE)!=0 ){
      sqlite3_index_info *pVtabIdx = pLevel->plan.u.pVtabIdx;
      zMsg = sqlite3MAppendf(db, zMsg, "%s VIRTUAL TABLE INDEX %d:%s", zMsg,
                  pVtabIdx->idxNum, pVtabIdx->idxStr);
    }
#endif
    if( wctrlFlags&(WHERE_ORDERBY_MIN|WHERE_ORDERBY_MAX) ){
      testcase( wctrlFlags & WHERE_ORDERBY_MIN );
      nRow = 1;
    }else{
      nRow = (sqlite3_int64)pLevel->plan.nRow;
    }
    zMsg = sqlite3MAppendf(db, zMsg, "%s (~%lld rows)", zMsg, nRow);
    sqlite3VdbeAddOp4(v, OP_Explain, iId, iLevel, iFrom, zMsg, P4_DYNAMIC);
  }
}
#else
# define explainOneScan(u,v,w,x,y,z)
#endif /* SQLITE_OMIT_EXPLAIN */


/*
** Generate code for the start of the iLevel-th loop in the WHERE clause
** implementation described by pWInfo.
<<<<<<< HEAD
**生成代码开始由pWInfo描述的WHERE子句的iLevel-th循环
*/
static Bitmask codeOneLoopStart(
  WhereInfo *pWInfo,   /* Complete information about the WHERE clause WHERE子句的完整信息 */
  int iLevel,          /* Which level of pWInfo->a[] should be coded 需要编码的pWInfo->a[]的等级 */
  u16 wctrlFlags,      /* One of the WHERE_* flags defined in sqliteInt.h 在sqliteInt.h中定义的WHERE_*标志中的一个 */
  Bitmask notReady     /* Which tables are currently available 哪个表示当前有效的 */
){
  int j, k;            /* Loop counters 循环计数器 */
  int iCur;            /* The VDBE cursor for the table 表的VDBE游标 */
  int addrNxt;         /* Where to jump to continue with the next IN case 跳转继续下一个IN */
  int omitTable;       /* True if we use the index only 如果我们只使用索引则为TRUE */
  int bRev;            /* True if we need to scan in reverse order 如果我们需要在倒序中扫描则为TRUE */
  WhereLevel *pLevel;  /* The where level to be coded 被编码的where等级 */
  WhereClause *pWC;    /* Decomposition of the entire WHERE clause 分解整个where子句 */
  WhereTerm *pTerm;               /* A WHERE clause term 一个WHERE子句的term */
  Parse *pParse;                  /* Parsing context 分析上下文 */
  Vdbe *v;                        /* The prepared stmt under constructions 在构建中准备好的stmt */
  struct SrcList_item *pTabItem;  /* FROM clause term being coded 正在编码的FROM子句term */
  int addrBrk;                    /* Jump here to break out of the loop 跳出循环时的位置 */
  int addrCont;                   /* Jump here to continue with next cycle 继续下一次循环的位置 */
  int iRowidReg = 0;        /* Rowid is stored in this register, if not zero 如果不为0，Rowid存储在这个寄存器中 */
  int iReleaseReg = 0;      /* Temp register to free before returning 在返回前释放临时寄存器 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949

  pParse = pWInfo->pParse;
  v = pParse->pVdbe;
  pWC = pWInfo->pWC;
  pLevel = &pWInfo->a[iLevel];
  pTabItem = &pWInfo->pTabList->a[pLevel->iFrom];
  iCur = pTabItem->iCursor;
  bRev = (pLevel->plan.wsFlags & WHERE_REVERSE)!=0;
  omitTable = (pLevel->plan.wsFlags & WHERE_IDX_ONLY)!=0 
           && (wctrlFlags & WHERE_FORCE_TABLE)==0;

  /* Create labels for the "break" and "continue" instructions
  ** for the current loop.  Jump to addrBrk to break out of a loop.
  ** Jump to cont to go immediately to the next iteration of the
  ** loop.
<<<<<<< HEAD
  ** 为当前循环的"break"和"continue"指令的创建标签。跳到addrBrk来跳出循环。
  ** 跳到cont就立即执行下一个迭代循环
  **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  ** When there is an IN operator, we also have a "addrNxt" label that
  ** means to continue with the next IN value combination.  When
  ** there are no IN operators in the constraints, the "addrNxt" label
  ** is the same as "addrBrk".
<<<<<<< HEAD
  **当有一个IN operator,我们也有一个“addrNxt”的标签,
  **这意味着,继续处理下一个IN value组合。
  **当没有IN operators的约束,"addrNxt"标签和"addrBrk"作用相同
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  addrBrk = pLevel->addrBrk = pLevel->addrNxt = sqlite3VdbeMakeLabel(v);
  addrCont = pLevel->addrCont = sqlite3VdbeMakeLabel(v);

  /* If this is the right table of a LEFT OUTER JOIN, allocate and
  ** initialize a memory cell that records if this table matches any
  ** row of the left table of the join.
<<<<<<< HEAD
  **如果这是正确的左外连接表,分配和初始化一个存储单元,
  **这个内存单元来记录是否此表与任何左连接表的行匹配。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  if( pLevel->iFrom>0 && (pTabItem[0].jointype & JT_LEFT)!=0 ){
    pLevel->iLeftJoin = ++pParse->nMem;
    sqlite3VdbeAddOp2(v, OP_Integer, 0, pLevel->iLeftJoin);
    VdbeComment((v, "init LEFT JOIN no-match flag"));
  }

#ifndef SQLITE_OMIT_VIRTUALTABLE
  if(  (pLevel->plan.wsFlags & WHERE_VIRTUALTABLE)!=0 ){
    /* Case 0:  The table is a virtual-table.  Use the VFilter and VNext
    **          to access the data.
<<<<<<< HEAD
    **
    ** 情况0:该表是一个虚拟表。使用VFilter和VNext来访问数据。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    int iReg;   /* P3 Value for OP_VFilter OP_VFilter的P3值 */
    sqlite3_index_info *pVtabIdx = pLevel->plan.u.pVtabIdx;
    int nConstraint = pVtabIdx->nConstraint;
    struct sqlite3_index_constraint_usage *aUsage =
                                                pVtabIdx->aConstraintUsage;
    const struct sqlite3_index_constraint *aConstraint =
                                                pVtabIdx->aConstraint;

    sqlite3ExprCachePush(pParse);
    iReg = sqlite3GetTempRange(pParse, nConstraint+2);
    for(j=1; j<=nConstraint; j++){
      for(k=0; k<nConstraint; k++){
        if( aUsage[k].argvIndex==j ){
          int iTerm = aConstraint[k].iTermOffset;
          sqlite3ExprCode(pParse, pWC->a[iTerm].pExpr->pRight, iReg+j+1);
          break;
        }
      }
      if( k==nConstraint ) break;
    }
    sqlite3VdbeAddOp2(v, OP_Integer, pVtabIdx->idxNum, iReg);
    sqlite3VdbeAddOp2(v, OP_Integer, j-1, iReg+1);
    sqlite3VdbeAddOp4(v, OP_VFilter, iCur, addrBrk, iReg, pVtabIdx->idxStr,
                      pVtabIdx->needToFreeIdxStr ? P4_MPRINTF : P4_STATIC);
    pVtabIdx->needToFreeIdxStr = 0;
    for(j=0; j<nConstraint; j++){
      if( aUsage[j].omit ){
        int iTerm = aConstraint[j].iTermOffset;
        disableTerm(pLevel, &pWC->a[iTerm]);
      }
    }
    pLevel->op = OP_VNext;
    pLevel->p1 = iCur;
    pLevel->p2 = sqlite3VdbeCurrentAddr(v);
    sqlite3ReleaseTempRange(pParse, iReg, nConstraint+2);
    sqlite3ExprCachePop(pParse, 1);
  }else
#endif /* SQLITE_OMIT_VIRTUALTABLE */

  if( pLevel->plan.wsFlags & WHERE_ROWID_EQ ){
    /* Case 1:  We can directly reference a single row using an
    **          equality comparison against the ROWID field.  Or
    **          we reference multiple rows using a "rowid IN (...)"
    **          construct.
    **
    ** 情况1:我们可以直接使用等式与ROWID字段比较来引用一个单行。
    **       或引用多行使用"rowid IN (...)"结构。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    iReleaseReg = sqlite3GetTempReg(pParse);
    pTerm = findTerm(pWC, iCur, -1, notReady, WO_EQ|WO_IN, 0);
    assert( pTerm!=0 );
    assert( pTerm->pExpr!=0 );
    assert( pTerm->leftCursor==iCur );
    assert( omitTable==0 );
    testcase( pTerm->wtFlags & TERM_VIRTUAL ); /* EV: R-30575-11662 */
    iRowidReg = codeEqualityTerm(pParse, pTerm, pLevel, iReleaseReg);
    addrNxt = pLevel->addrNxt;
    sqlite3VdbeAddOp2(v, OP_MustBeInt, iRowidReg, addrNxt);
    sqlite3VdbeAddOp3(v, OP_NotExists, iCur, addrNxt, iRowidReg);
    sqlite3ExprCacheStore(pParse, iCur, -1, iRowidReg);
    VdbeComment((v, "pk"));
    pLevel->op = OP_Noop;
  }else if( pLevel->plan.wsFlags & WHERE_ROWID_RANGE ){
    /* Case 2:  We have an inequality comparison against the ROWID field.
<<<<<<< HEAD
    **情况2:我们对ROWID字段有不相等的比较
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    int testOp = OP_Noop;
    int start;
    int memEndValue = 0;
    WhereTerm *pStart, *pEnd;

    assert( omitTable==0 );
    pStart = findTerm(pWC, iCur, -1, notReady, WO_GT|WO_GE, 0);
    pEnd = findTerm(pWC, iCur, -1, notReady, WO_LT|WO_LE, 0);
    if( bRev ){
      pTerm = pStart;
      pStart = pEnd;
      pEnd = pTerm;
    }
    if( pStart ){
<<<<<<< HEAD
      Expr *pX;             /* 表达式,定义了开始*/
      int r1, rTemp;        /* 寄存器开始容纳边界 */

      /* The following constant maps TK_xx codes into corresponding 
      ** seek opcodes.  It depends on a particular ordering of TK_xx
      **下列常数映射TK_xx代码到相应的操作码。
      **这取决于一个特定TK_x排序

      Expr *pX;             /* The expression that defines the start bound 表达式定义了开始范围 */
      int r1, rTemp;        /* Registers for holding the start boundary 保存开始范围的寄存器 */
      */
      const u8 aMoveOp[] = {
           /* TK_GT */  OP_SeekGt,
           /* TK_LE */  OP_SeekLe,
           /* TK_LT */  OP_SeekLt,
           /* TK_GE */  OP_SeekGe
      };
      assert( TK_LE==TK_GT+1 );      /* Make sure the ordering.. 确保顺序 */
      assert( TK_LT==TK_GT+2 );      /*  ... of the TK_xx values...  */
      assert( TK_GE==TK_GT+3 );      /*  ... is correcct. */

      testcase( pStart->wtFlags & TERM_VIRTUAL ); /* EV: R-30575-11662 */
      pX = pStart->pExpr;
      assert( pX!=0 );
      assert( pStart->leftCursor==iCur );
      r1 = sqlite3ExprCodeTemp(pParse, pX->pRight, &rTemp);
      sqlite3VdbeAddOp3(v, aMoveOp[pX->op-TK_GT], iCur, addrBrk, r1);
      VdbeComment((v, "pk"));
      sqlite3ExprCacheAffinityChange(pParse, r1, 1);
      sqlite3ReleaseTempReg(pParse, rTemp);
      disableTerm(pLevel, pStart);
    }else{
      sqlite3VdbeAddOp2(v, bRev ? OP_Last : OP_Rewind, iCur, addrBrk);
    }
    if( pEnd ){
      Expr *pX;
      pX = pEnd->pExpr;
      assert( pX!=0 );
      assert( pEnd->leftCursor==iCur );
      testcase( pEnd->wtFlags & TERM_VIRTUAL ); /* EV: R-30575-11662 */
      memEndValue = ++pParse->nMem;
      sqlite3ExprCode(pParse, pX->pRight, memEndValue);
      if( pX->op==TK_LT || pX->op==TK_GT ){
        testOp = bRev ? OP_Le : OP_Ge;
      }else{
        testOp = bRev ? OP_Lt : OP_Gt;
      }
      disableTerm(pLevel, pEnd);
    }
    start = sqlite3VdbeCurrentAddr(v);
    pLevel->op = bRev ? OP_Prev : OP_Next;
    pLevel->p1 = iCur;
    pLevel->p2 = start;
    if( pStart==0 && pEnd==0 ){
      pLevel->p5 = SQLITE_STMTSTATUS_FULLSCAN_STEP;
    }else{
      assert( pLevel->p5==0 );
    }
    if( testOp!=OP_Noop ){
      iRowidReg = iReleaseReg = sqlite3GetTempReg(pParse);
      sqlite3VdbeAddOp2(v, OP_Rowid, iCur, iRowidReg);
      sqlite3ExprCacheStore(pParse, iCur, -1, iRowidReg);
      sqlite3VdbeAddOp3(v, testOp, memEndValue, addrBrk, iRowidReg);
      sqlite3VdbeChangeP5(v, SQLITE_AFF_NUMERIC | SQLITE_JUMPIFNULL);
    }
  }else if( pLevel->plan.wsFlags & (WHERE_COLUMN_RANGE|WHERE_COLUMN_EQ) ){
    /* Case 3: A scan using an index.
    **
    **         The WHERE clause may contain zero or more equality 
    **         terms ("==" or "IN" operators) that refer to the N
    **         left-most columns of the index. It may also contain
    **         inequality constraints (>, <, >= or <=) on the indexed
    **         column that immediately follows the N equalities. Only 
    **         the right-most column can be an inequality - the rest must
    **         use the "==" and "IN" operators. For example, if the 
    **         index is on (x,y,z), then the following clauses are all 
    **         optimized:
    **
    **            x=5
    **            x=5 AND y=10
    **            x=5 AND y<10
    **            x=5 AND y>5 AND y<10
    **            x=5 AND y=5 AND z<=10
    **
    **         The z<10 term of the following cannot be used, only
    **         the x=5 term:
    **
    **            x=5 AND z<10
    **
    **         N may be zero if there are inequality constraints.
    **         If there are no inequality constraints, then N is at
    **         least one.
    **
    **         This case is also used when there are no WHERE clause
    **         constraints but an index is selected anyway, in order
    **         to force the output order to conform to an ORDER BY.
<<<<<<< HEAD
    **
    ** 情况3:使用索引的扫描
    **         WHERE子句可能包含0或多个等式条件("=="或"IN"运算符)
	**			这些条件涉及到N个索引最左边的列.
    **         它可能在索引列上也包含不等式约束(>, <, >= or <=)，紧随其后有N个等式。
    **         只有最右边的列可以为一个不等式--其余的必须使用"=="和"IN"运算符。
    **         例如，如过有一个在(x,y,z)的索引，下面的子句都可以进行优化:
    **			下面的例子中z<10不能优化。只有x=5可以:
    **			N可能是0如果有不等约束。
    **			如果这里没有不等约束，那么N至少为1.
	** 			当没有WHERE子句约束，但选出了一个索引并且为了强行让输出符合ORDER BY顺序时，这种情况也是适用的。
    **        
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */  
    static const u8 aStartOp[] = {
      0,
      0,
      OP_Rewind,           /* 2: (!start_constraints && startEq &&  !bRev) */
      OP_Last,             /* 3: (!start_constraints && startEq &&   bRev) */
      OP_SeekGt,           /* 4: (start_constraints  && !startEq && !bRev) */
      OP_SeekLt,           /* 5: (start_constraints  && !startEq &&  bRev) */
      OP_SeekGe,           /* 6: (start_constraints  &&  startEq && !bRev) */
      OP_SeekLe            /* 7: (start_constraints  &&  startEq &&  bRev) */
    };
    static const u8 aEndOp[] = {
      OP_Noop,             /* 0: (!end_constraints) */
      OP_IdxGE,            /* 1: (end_constraints && !bRev) */
      OP_IdxLT             /* 2: (end_constraints && bRev) */
    };
<<<<<<< HEAD
    int nEq = pLevel->plan.nEq;  /* Number of == or IN terms */
    int isMinQuery = 0;          /* 如果这是一个 SELECT min(x)..的优化 */
    int regBase;                 /* 基址寄存器约束值 */
    int r1;                      /* 临时寄存器  */
    WhereTerm *pRangeStart = 0;  /* 不等约束开始*/
    WhereTerm *pRangeEnd = 0;    /* 不等约束的范围 */
    int startEq;                 /* True if range start uses ==, >= or <= */
    int endEq;                   /* True if range end uses ==, >= or <= */
    int start_constraints;       /* 受限范围的开始 */
    int nConstraint;             /* 约束条件数量*/
    Index *pIdx;                 /* 我们将使用索引 */
    int iIdxCur;                 /* VDBE游标的索引*/
    int nExtraReg = 0;           /* 所需的额外的寄存器数量 */
    int op;                      /* 指令操作码  */
    char *zStartAff;             /*Affinity约束范围的开始 */
    char *zEndAff;               /* Affinity约束范围的结束 */
=======
    int nEq = pLevel->plan.nEq;  /* Number of == or IN terms ==或INterms的数目 */
    int isMinQuery = 0;          /* If this is an optimized SELECT min(x).. 如果这是一个优化的SELECT min(x)语句 */
    int regBase;                 /* Base register holding constraint values 基址寄存器保存约束值 */
    int r1;                      /* Temp register 临时寄存器 */
    WhereTerm *pRangeStart = 0;  /* Inequality constraint at range start 在范围初始的不等式约束 */
    WhereTerm *pRangeEnd = 0;    /* Inequality constraint at range end 在范围末尾的不等式约束 */
    int startEq;                 /* True if range start uses ==, >= or <= 如果范围初始使用==, >= or <=,那么为TRUE */
    int endEq;                   /* True if range end uses ==, >= or <= 如果范围末尾使用==, >= or <=,那么为TRUE */
    int start_constraints;       /* Start of range is constrained 范围初始是有约束的 */
    int nConstraint;             /* Number of constraint terms 约束terms的数目 */
    Index *pIdx;                 /* The index we will be using 将使用的索引 */
    int iIdxCur;                 /* The VDBE cursor for the index 索引的VDBE游标 */
    int nExtraReg = 0;           /* Number of extra registers needed 需要的额外的寄存器数量 */
    int op;                      /* Instruction opcode 指示操作码 */
    char *zStartAff;             /* Affinity for start of range constraint 范围初始的约束的亲和性 */
    char *zEndAff;               /* Affinity for end of range constraint 范围末尾的约束的亲和性 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949

    pIdx = pLevel->plan.u.pIdx;
    iIdxCur = pLevel->iIdxCur;
    k = (nEq==pIdx->nColumn ? -1 : pIdx->aiColumn[nEq]);

    /* If this loop satisfies a sort order (pOrderBy) request that 
    ** was passed to this function to implement a "SELECT min(x) ..." 
    ** query, then the caller will only allow the loop to run for
    ** a single iteration. This means that the first row returned
    ** should not have a NULL value stored in 'x'. If column 'x' is
    ** the first one after the nEq equality constraints in the index,
    ** this requires some special handling.
<<<<<<< HEAD
    如果这个循环满足排序(pOrderBy)请求，将这个请求传递给这个函数实现一个"SELECT min(x) ..."查询，
    那么调用者将只允许为一个迭代循环运行。
    这意味着返回的第一行不应该有存储在“x”的NULL值。
    如果列'x'是索引中nEq个等式约束后第一个,这需要一些特殊的处理。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    if( (wctrlFlags&WHERE_ORDERBY_MIN)!=0
     && (pLevel->plan.wsFlags&WHERE_ORDERBY)
     && (pIdx->nColumn>nEq)
    ){
      /* assert( pOrderBy->nExpr==1 ); */
      /* assert( pOrderBy->a[0].pExpr->iColumn==pIdx->aiColumn[nEq] ); */
      isMinQuery = 1;
      nExtraReg = 1;
    }

    /* Find any inequality constraint terms for the start and end 
    ** of the range. 
<<<<<<< HEAD
    ** 为范围的开始和结尾寻找不等式约束条件
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    if( pLevel->plan.wsFlags & WHERE_TOP_LIMIT ){
      pRangeEnd = findTerm(pWC, iCur, k, notReady, (WO_LT|WO_LE), pIdx);
      nExtraReg = 1;
    }
    if( pLevel->plan.wsFlags & WHERE_BTM_LIMIT ){
      pRangeStart = findTerm(pWC, iCur, k, notReady, (WO_GT|WO_GE), pIdx);
      nExtraReg = 1;
    }

    /* Generate code to evaluate all constraint terms using == or IN
    ** and store the values of those terms in an array of registers
    ** starting at regBase.
<<<<<<< HEAD
    **
    ** 生成代码来计算所有使用==或IN约束terms并且把这些terms的值存储在由regBase开始的一组寄存器中。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    regBase = codeAllEqualityTerms(
        pParse, pLevel, pWC, notReady, nExtraReg, &zStartAff
    );
    zEndAff = sqlite3DbStrDup(pParse->db, zStartAff);
    addrNxt = pLevel->addrNxt;

    /* If we are doing a reverse order scan on an ascending index, or
    ** a forward order scan on a descending index, interchange the 
    ** start and end terms (pRangeStart and pRangeEnd).
<<<<<<< HEAD
    如果我们逆序扫描一个升序索引,
    或先序扫描降序索引,
    交换开始和结束条件(pRangeStart和pRangeEnd)
    **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    if( (nEq<pIdx->nColumn && bRev==(pIdx->aSortOrder[nEq]==SQLITE_SO_ASC))
     || (bRev && pIdx->nColumn==nEq)
    ){
      SWAP(WhereTerm *, pRangeEnd, pRangeStart);
    }

    testcase( pRangeStart && pRangeStart->eOperator & WO_LE );
    testcase( pRangeStart && pRangeStart->eOperator & WO_GE );
    testcase( pRangeEnd && pRangeEnd->eOperator & WO_LE );
    testcase( pRangeEnd && pRangeEnd->eOperator & WO_GE );
    startEq = !pRangeStart || pRangeStart->eOperator & (WO_LE|WO_GE);
    endEq =   !pRangeEnd || pRangeEnd->eOperator & (WO_LE|WO_GE);
    start_constraints = pRangeStart || nEq>0;

<<<<<<< HEAD
    /* Seek the index cursor to the start of the range.
    寻求索引游标范围的开始。
     */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    nConstraint = nEq;
    if( pRangeStart ){
      Expr *pRight = pRangeStart->pExpr->pRight;
      sqlite3ExprCode(pParse, pRight, regBase+nEq);
      if( (pRangeStart->wtFlags & TERM_VNULL)==0 ){
        sqlite3ExprCodeIsNullJump(v, pRight, regBase+nEq, addrNxt);
      }
      if( zStartAff ){
        if( sqlite3CompareAffinity(pRight, zStartAff[nEq])==SQLITE_AFF_NONE){
          /* Since the comparison is to be performed with no conversions
          ** applied to the operands, set the affinity to apply to pRight to SQLITE_AFF_NONE
<<<<<<< HEAD
          ** SQLITE_AFF_NONE.
          由于执行的比较是没有转换就应用于操作数,
          设置pRight的亲和性为SQLITE_AFF_NONE
            */        
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
          zStartAff[nEq] = SQLITE_AFF_NONE;
        }
        if( sqlite3ExprNeedsNoAffinityChange(pRight, zStartAff[nEq]) ){
          zStartAff[nEq] = SQLITE_AFF_NONE;
        }
      }  
      nConstraint++;
      testcase( pRangeStart->wtFlags & TERM_VIRTUAL ); /* EV: R-30575-11662 */
    }else if( isMinQuery ){
      sqlite3VdbeAddOp2(v, OP_Null, 0, regBase+nEq);
      nConstraint++;
      startEq = 0;
      start_constraints = 1;
    }
    codeApplyAffinity(pParse, regBase, nConstraint, zStartAff);
    op = aStartOp[(start_constraints<<2) + (startEq<<1) + bRev];
    assert( op!=0 );
    testcase( op==OP_Rewind );
    testcase( op==OP_Last );
    testcase( op==OP_SeekGt );
    testcase( op==OP_SeekGe );
    testcase( op==OP_SeekLe );
    testcase( op==OP_SeekLt );
    sqlite3VdbeAddOp4Int(v, op, iIdxCur, addrNxt, regBase, nConstraint);

    /* Load the value for the inequality constraint at the end of the
    ** range (if any).
<<<<<<< HEAD
    在范围的结尾加载不等式约束的值(如果有的话)
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    nConstraint = nEq;
    if( pRangeEnd ){
      Expr *pRight = pRangeEnd->pExpr->pRight;
      sqlite3ExprCacheRemove(pParse, regBase+nEq, 1);
      sqlite3ExprCode(pParse, pRight, regBase+nEq);
      if( (pRangeEnd->wtFlags & TERM_VNULL)==0 ){
        sqlite3ExprCodeIsNullJump(v, pRight, regBase+nEq, addrNxt);
      }
      if( zEndAff ){
        if( sqlite3CompareAffinity(pRight, zEndAff[nEq])==SQLITE_AFF_NONE){
          /* Since the comparison is to be performed with no conversions
          ** applied to the operands, set the affinity to apply to pRight to 
<<<<<<< HEAD
          ** SQLITE_AFF_NONE. 
          由于执行的比较是没有转换应用于操作数,
          设置pRight的亲和性为SQLITE_AFF_NONE
           */
=======
          ** SQLITE_AFF_NONE.  
          **
          ** 由于应用在运算对象上的比较是未转换的，设置pRight的亲和性为SQLITE_AFF_NONE
          */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
          zEndAff[nEq] = SQLITE_AFF_NONE;
        }
        if( sqlite3ExprNeedsNoAffinityChange(pRight, zEndAff[nEq]) ){
          zEndAff[nEq] = SQLITE_AFF_NONE;
        }
      }  
      codeApplyAffinity(pParse, regBase, nEq+1, zEndAff);
      nConstraint++;
      testcase( pRangeEnd->wtFlags & TERM_VIRTUAL ); /* EV: R-30575-11662 */
    }
    sqlite3DbFree(pParse->db, zStartAff);
    sqlite3DbFree(pParse->db, zEndAff);

    /* Top of the loop body 循环体的顶部 */
    pLevel->p2 = sqlite3VdbeCurrentAddr(v);

<<<<<<< HEAD
    /* Check if the index cursor is past the end of the range. 
    检查索引指针是否超过范围的结束。
    */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    op = aEndOp[(pRangeEnd || nEq) * (1 + bRev)];
    testcase( op==OP_Noop );
    testcase( op==OP_IdxGE );
    testcase( op==OP_IdxLT );
    if( op!=OP_Noop ){
      sqlite3VdbeAddOp4Int(v, op, iIdxCur, addrNxt, regBase, nConstraint);
      sqlite3VdbeChangeP5(v, endEq!=bRev ?1:0);
    }

    /* If there are inequality constraints, check that the value
    ** of the table column that the inequality contrains is not NULL.
    ** If it is, jump to the next iteration of the loop.
<<<<<<< HEAD
    **如果有不等式约束,检查表中不等约束非NULL的列的值,
    **如果是,跳转到下一个迭代的循环。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    r1 = sqlite3GetTempReg(pParse);
    testcase( pLevel->plan.wsFlags & WHERE_BTM_LIMIT );
    testcase( pLevel->plan.wsFlags & WHERE_TOP_LIMIT );
    if( (pLevel->plan.wsFlags & (WHERE_BTM_LIMIT|WHERE_TOP_LIMIT))!=0 ){
      sqlite3VdbeAddOp3(v, OP_Column, iIdxCur, nEq, r1);
      sqlite3VdbeAddOp2(v, OP_IsNull, r1, addrCont);
    }
    sqlite3ReleaseTempReg(pParse, r1);

<<<<<<< HEAD
    /* Seek the table cursor, if required 
    寻找表指针,如果需要的话
    */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    disableTerm(pLevel, pRangeStart);
    disableTerm(pLevel, pRangeEnd);
    if( !omitTable ){
      iRowidReg = iReleaseReg = sqlite3GetTempReg(pParse);
      sqlite3VdbeAddOp2(v, OP_IdxRowid, iIdxCur, iRowidReg);
      sqlite3ExprCacheStore(pParse, iCur, -1, iRowidReg);
      sqlite3VdbeAddOp2(v, OP_Seek, iCur, iRowidReg);  /* Deferred seek */
    }

    /* Record the instruction used to terminate the loop. Disable 
    ** WHERE clause terms made redundant by the index range scan.
<<<<<<< HEAD
    **
    ** 记录用于终止循环的指令。禁用WHERE子句由索引范围扫描造成的冗余。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    if( pLevel->plan.wsFlags & WHERE_UNIQUE ){
      pLevel->op = OP_Noop;
    }else if( bRev ){
      pLevel->op = OP_Prev;
    }else{
      pLevel->op = OP_Next;
    }
    pLevel->p1 = iIdxCur;
  }else

#ifndef SQLITE_OMIT_OR_OPTIMIZATION
  if( pLevel->plan.wsFlags & WHERE_MULTI_OR ){
    /* Case 4:  Two or more separately indexed terms connected by OR
    ** Example:
    **
    **   CREATE TABLE t1(a,b,c,d);
    **   CREATE INDEX i1 ON t1(a);
    **   CREATE INDEX i2 ON t1(b);
    **   CREATE INDEX i3 ON t1(c);
    **
    **   SELECT * FROM t1 WHERE a=5 OR b=7 OR (c=11 AND d=13)
    **
    ** In the example, there are three indexed terms connected by OR.
    ** The top of the loop looks like this:
    **          Null       1                # Zero the rowset in reg 1
    **
    ** Then, for each indexed term, the following. The arguments to
    ** RowSetTest are such that the rowid of the current row is inserted
    ** into the RowSet. If it is already present, control skips the
    ** Gosub opcode and jumps straight to the code generated by WhereEnd().
    **        sqlite3WhereBegin(<term>)
    **          RowSetTest                  # Insert rowid into rowset
    **          Gosub      2 A
    **        sqlite3WhereEnd()
    **
    ** Following the above, code to terminate the loop. Label A, the target
    ** of the Gosub above, jumps to the instruction right after the Goto.
    **          Null       1                # Zero the rowset in reg 1
    **          Goto       B                # The loop is finished.
    **
    **       A: <loop body>                 # Return data, whatever.
    **
    **          Return     2                # Jump back to the Gosub
    **
    **       B: <after the loop>
    **
    ** 情况4:  由OR连接的两个或更多的独立索引terms.
    ** 例如:
    **
    **   CREATE TABLE t1(a,b,c,d);
    **   CREATE INDEX i1 ON t1(a);
    **   CREATE INDEX i2 ON t1(b);
    **   CREATE INDEX i3 ON t1(c);
    **
    **   SELECT * FROM t1 WHERE a=5 OR b=7 OR (c=11 AND d=13)
    **
    ** 在这个例子中，有3个带索引的terms被OR连接。
    ** 循环的顶部像这样:
    **
    **          Null       1                # 把寄存器1中的rowset清零
    **
    ** 那么，对于接下来的每个有索引的term.传递给RowSetTest的参数是当前插入到RowSet的行的rowid. 
    ** 如果已经存在，控制跳过这个Gosub操作码并且直接跳到由WhereEnd()直接生成的代码。
    **
    **        sqlite3WhereBegin(<term>)
    **          RowSetTest                  # 把rowid插入到rowset
    **          Gosub      2 A
    **        sqlite3WhereEnd()
    **
    ** 接下来，生成终止循环的代码。Label A, 上面Gosub的目标, 在Goto之后跳到相应的指令。
    **          Null       1                # 在寄存器1中把rowset清零
    **          Goto       B                # 循环结束.
    **
    **       A: <loop body>                 # 不管怎样，返回数据
    **
    **          Return     2                # 跳回到Gosub
    **
    **       B: <after the loop>
    **
    */
<<<<<<< HEAD
    WhereClause *pOrWc;    /* The OR-clause broken out into subterms OR子句分裂的子terms */
    SrcList *pOrTab;       /* Shortened table list or OR-clause generation 缩小表列表或OR子句的生产 */
    Index *pCov = 0;             /* Potential covering index (or NULL) 可能的覆盖索引(或NULL) */
    int iCovCur = pParse->nTab++;  /* Cursor used for index scans (if any) 用于索引扫描的游标 */

    int regReturn = ++pParse->nMem;           /* Register used with OP_Gosub 寄存器使用OP_Gosub */
    int regRowset = 0;                        /* Register for RowSet object 用于RowSet对象的寄存器 */
    int regRowid = 0;                         /* Register holding rowid 保存rowid的寄存器 */
    int iLoopBody = sqlite3VdbeMakeLabel(v);  /* Start of loop body 开始循环体 */
    int iRetInit;                             /* Address of regReturn init regReturn地址初始化 */
    int untestedTerms = 0;             /* Some terms not completely tested 一些没完全测试的terms */
    int ii;                            /* Loop counter 循环计数器 */
    Expr *pAndExpr = 0;                /* An ".. AND (...)" expression 一个".. AND (...)"表达式 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
   
    pTerm = pLevel->plan.u.pTerm;
    assert( pTerm!=0 );
    assert( pTerm->eOperator==WO_OR );
    assert( (pTerm->wtFlags & TERM_ORINFO)!=0 );
    pOrWc = &pTerm->u.pOrInfo->wc;
    pLevel->op = OP_Return;
    pLevel->p1 = regReturn;

    /* Set up a new SrcList in pOrTab containing the table being scanned
    ** by this loop in the a[0] slot and all notReady tables in a[1..] slots.
    ** This becomes the SrcList in the recursive call to sqlite3WhereBegin().
<<<<<<< HEAD
    建立一个新的SrcList pOrTab包含表这个表通过a[0]和a[1..]进行扫描。
    这就是SrcList递归调用sqlite3WhereBegin()。
    */
    if( pWInfo->nLevel>1 ){
      int nNotReady;                 /* notReady 表的数目 */
      struct SrcList_item *origSrc;     /* 原始的表列表 */
=======
    **
    ** 在pOrTab中创建新的SrcList，这个SrcList包含通过这个循环扫描到在a[0]位置的表和在a[1..]位置所有notReady的表
    */
    if( pWInfo->nLevel>1 ){
      int nNotReady;                 /* The number of notReady tables notReady表的数目 */
      struct SrcList_item *origSrc;     /* Original list of tables 表的原始列表 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
      nNotReady = pWInfo->nLevel - iLevel - 1;
      pOrTab = sqlite3StackAllocRaw(pParse->db,
                            sizeof(*pOrTab)+ nNotReady*sizeof(pOrTab->a[0]));
      if( pOrTab==0 ) return notReady;
      pOrTab->nAlloc = (i16)(nNotReady + 1);
      pOrTab->nSrc = pOrTab->nAlloc;
      memcpy(pOrTab->a, pTabItem, sizeof(*pTabItem));
      origSrc = pWInfo->pTabList->a;
      for(k=1; k<=nNotReady; k++){
        memcpy(&pOrTab->a[k], &origSrc[pLevel[k].iFrom], sizeof(pOrTab->a[k]));
      }
    }else{
      pOrTab = pWInfo->pTabList;
    }

    /* Initialize the rowset register to contain NULL. An SQL NULL is 
    ** equivalent to an empty rowset.
<<<<<<< HEAD
    **初始化行集寄存器包含NULL。一个SQL NULL是等价于一个空的行集。
    **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    ** Also initialize regReturn to contain the address of the instruction 
    ** immediately following the OP_Return at the bottom of the loop. This
    ** is required in a few obscure LEFT JOIN cases where control jumps
    ** over the top of the loop into the body of it. In this case the 
    ** correct response for the end-of-loop code (the OP_Return) is to 
    ** fall through to the next instruction, just as an OP_Next does if
    ** called on an uninitialized cursor.
<<<<<<< HEAD
    **
    ** 同时也初始化regReturn包含指令地址，这个regReturn是紧跟着循环底部的OP_Return.
    ** 在这种情况对于循环结束编码的正确响应是要跳过下一个指令，
    ** 就像在一个为初始化的游标中调用一个OP_Next那样
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    if( (wctrlFlags & WHERE_DUPLICATES_OK)==0 ){
      regRowset = ++pParse->nMem;
      regRowid = ++pParse->nMem;
      sqlite3VdbeAddOp2(v, OP_Null, 0, regRowset);
    }
    iRetInit = sqlite3VdbeAddOp2(v, OP_Integer, 0, regReturn);

    /* If the original WHERE clause is z of the form:  (x1 OR x2 OR ...) AND y
    ** Then for every term xN, evaluate as the subexpression: xN AND z
    ** That way, terms in y that are factored into the disjunction will
    ** be picked up by the recursive calls to sqlite3WhereBegin() below.
<<<<<<< HEAD
    **如果原始WHERE子句是z形式:(x1或x2或…)和y。然后每个子句xN,
    计算子表达式:xN和z,那样，在y中的terms通过递归调用下面的sqlite3WhereBegin()来进行分解
    **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    ** Actually, each subexpression is converted to "xN AND w" where w is
    ** the "interesting" terms of z - terms that did not originate in the
    ** ON or USING clause of a LEFT JOIN, and terms that are usable as 
    ** indices.
<<<<<<< HEAD
    **
    ** 事实上，每个子表达式转化为"xN AND w"，
    ** 其中w是z的"interesting" terms-terms不是源于LEFT JOIN的ON或USING子句,并且terms可以被当做索引使用。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    if( pWC->nTerm>1 ){
      int iTerm;
      for(iTerm=0; iTerm<pWC->nTerm; iTerm++){
        Expr *pExpr = pWC->a[iTerm].pExpr;
        if( ExprHasProperty(pExpr, EP_FromJoin) ) continue;
        if( pWC->a[iTerm].wtFlags & (TERM_VIRTUAL|TERM_ORINFO) ) continue;
        if( (pWC->a[iTerm].eOperator & WO_ALL)==0 ) continue;
        pExpr = sqlite3ExprDup(pParse->db, pExpr, 0);
        pAndExpr = sqlite3ExprAnd(pParse->db, pAndExpr, pExpr);
      }
      if( pAndExpr ){
        pAndExpr = sqlite3PExpr(pParse, TK_AND, 0, pAndExpr, 0);
      }
    }

    for(ii=0; ii<pOrWc->nTerm; ii++){
      WhereTerm *pOrTerm = &pOrWc->a[ii];
      if( pOrTerm->leftCursor==iCur || pOrTerm->eOperator==WO_AND ){
        WhereInfo *pSubWInfo;          /* Info for single OR-term scan OR-term扫描的信息 */
        Expr *pOrExpr = pOrTerm->pExpr;
        if( pAndExpr ){
          pAndExpr->pLeft = pOrExpr;
          pOrExpr = pAndExpr;
        }
<<<<<<< HEAD
        /* Loop through table entries that match term pOrTerm.
        遍历与术语pOrTerm相匹配的表项目。
         */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
        pSubWInfo = sqlite3WhereBegin(pParse, pOrTab, pOrExpr, 0, 0,
                        WHERE_OMIT_OPEN_CLOSE | WHERE_AND_ONLY |
                        WHERE_FORCE_TABLE | WHERE_ONETABLE_ONLY, iCovCur);
        assert( pSubWInfo || pParse->nErr || pParse->db->mallocFailed );
        if( pSubWInfo ){
          WhereLevel *pLvl;
          explainOneScan(
              pParse, pOrTab, &pSubWInfo->a[0], iLevel, pLevel->iFrom, 0
          );
          if( (wctrlFlags & WHERE_DUPLICATES_OK)==0 ){
            int iSet = ((ii==pOrWc->nTerm-1)?-1:ii);
            int r;
            r = sqlite3ExprCodeGetColumn(pParse, pTabItem->pTab, -1, iCur, 
                                         regRowid, 0);
            sqlite3VdbeAddOp4Int(v, OP_RowSetTest, regRowset,
                                 sqlite3VdbeCurrentAddr(v)+2, r, iSet);
          }
          sqlite3VdbeAddOp2(v, OP_Gosub, regReturn, iLoopBody);

          /* The pSubWInfo->untestedTerms flag means that this OR term
          ** contained one or more AND term from a notReady table.  The
          ** terms from the notReady table could not be tested and will
          ** need to be tested later.
<<<<<<< HEAD
          pSubWInfo - > untestedTerms标志意味着这OR连接词包含一个或多个来自notReady表的AND term。
          来自notReady表的terms不能被测试并且稍后需要测试。      
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
          */
          if( pSubWInfo->untestedTerms ) untestedTerms = 1;

          /* If all of the OR-connected terms are optimized using the same
          ** index, and the index is opened using the same cursor number
          ** by each call to sqlite3WhereBegin() made by this loop, it may
          ** be possible to use that index as a covering index.
<<<<<<< HEAD
          **如果所有的OR连接词使用相同的索引都是已经优化的,
          并且通过每次调用由循环产生的sqlite3WhereBegin()来使用相同的游标数据打开索引
          使用该索引作为覆盖索引是可能的
          **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
          ** If the call to sqlite3WhereBegin() above resulted in a scan that
          ** uses an index, and this is either the first OR-connected term
          ** processed or the index i       s the same as that used by all previous
          ** terms, set pCov to the candidate covering index. Otherwise, set 
          ** pCov to NULL to indicate that no candidate covering index will 
          ** be available.
<<<<<<< HEAD
          **
          ** 如果调用上面的sqlite3WhereBegin()导致使用索引扫描，
          ** 并且这是第一次执行OR-connected term或使用的索引与前面所有的terms形态一样,把pCov设置为候选的覆盖索引。
          ** 否则，把pCov设置为NULL来说明没有候选的覆盖索引可供使用。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
          */
          pLvl = &pSubWInfo->a[0];
          if( (pLvl->plan.wsFlags & WHERE_INDEXED)!=0
           && (pLvl->plan.wsFlags & WHERE_TEMP_INDEX)==0
           && (ii==0 || pLvl->plan.u.pIdx==pCov)
          ){
            assert( pLvl->iIdxCur==iCovCur );
            pCov = pLvl->plan.u.pIdx;
          }else{
            pCov = 0;
          }

<<<<<<< HEAD
          /* Finish the loop through table entries that match term pOrTerm.
          完成遍历表与术语pOrTerm相匹配的条目。
           */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
          sqlite3WhereEnd(pSubWInfo);
        }
      }
    }
    pLevel->u.pCovidx = pCov;
    if( pCov ) pLevel->iIdxCur = iCovCur;
    if( pAndExpr ){
      pAndExpr->pLeft = 0;
      sqlite3ExprDelete(pParse->db, pAndExpr);
    }
    sqlite3VdbeChangeP1(v, iRetInit, sqlite3VdbeCurrentAddr(v));
    sqlite3VdbeAddOp2(v, OP_Goto, 0, pLevel->addrBrk);
    sqlite3VdbeResolveLabel(v, iLoopBody);

    if( pWInfo->nLevel>1 ) sqlite3StackFree(pParse->db, pOrTab);
    if( !untestedTerms ) disableTerm(pLevel, pTerm);
  }else
#endif /* SQLITE_OMIT_OR_OPTIMIZATION */

  {
    /* Case 5:  There is no usable index.  We must do a complete
    **          scan of the entire table.
<<<<<<< HEAD
    情况5:没有可用的索引。我们必须对整个表做一个完整的扫描。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    static const u8 aStep[] = { OP_Next, OP_Prev };
    static const u8 aStart[] = { OP_Rewind, OP_Last };
    assert( bRev==0 || bRev==1 );
    assert( omitTable==0 );
    pLevel->op = aStep[bRev];
    pLevel->p1 = iCur;
    pLevel->p2 = 1 + sqlite3VdbeAddOp2(v, aStart[bRev], iCur, addrBrk);
    pLevel->p5 = SQLITE_STMTSTATUS_FULLSCAN_STEP;
  }
  notReady &= ~getMask(pWC->pMaskSet, iCur);

  /* Insert code to test every subexpression that can be completely
  ** computed using the current set of tables.
<<<<<<< HEAD
	**插入代码测试每个子表达式，这些子表达式可以完全使用当前的表集合计算
  ** IMPLEMENTATION-OF: R-49525-50935 Terms that cannot be satisfied through
  ** the use of indices become tests that are evaluated against each row of
  ** the relevant input tables.
  **条件不能满足通过使用指数成为测试,评估这些测试与相关输入表的每一行
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  for(pTerm=pWC->a, j=pWC->nTerm; j>0; j--, pTerm++){
    Expr *pE;
    testcase( pTerm->wtFlags & TERM_VIRTUAL ); /* IMP: R-30575-11662 */
    testcase( pTerm->wtFlags & TERM_CODED );
    if( pTerm->wtFlags & (TERM_VIRTUAL|TERM_CODED) ) continue;
    if( (pTerm->prereqAll & notReady)!=0 ){
      testcase( pWInfo->untestedTerms==0
               && (pWInfo->wctrlFlags & WHERE_ONETABLE_ONLY)!=0 );
      pWInfo->untestedTerms = 1;
      continue;
    }
    pE = pTerm->pExpr;
    assert( pE!=0 );
    if( pLevel->iLeftJoin && !ExprHasProperty(pE, EP_FromJoin) ){
      continue;
    }
    sqlite3ExprIfFalse(pParse, pE, addrCont, SQLITE_JUMPIFNULL);
    pTerm->wtFlags |= TERM_CODED;
  }

  /* For a LEFT OUTER JOIN, generate code that will record the fact that
<<<<<<< HEAD
  ** at least one row of the right table has matched the left table.  
  **对于一个左外连接,生成的代码将记录一个事实，即右表中至少一行与左表匹配
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  if( pLevel->iLeftJoin ){
    pLevel->addrFirst = sqlite3VdbeCurrentAddr(v);
    sqlite3VdbeAddOp2(v, OP_Integer, 1, pLevel->iLeftJoin);
    VdbeComment((v, "record LEFT JOIN hit"));
    sqlite3ExprCacheClear(pParse);
    for(pTerm=pWC->a, j=0; j<pWC->nTerm; j++, pTerm++){
      testcase( pTerm->wtFlags & TERM_VIRTUAL );  /* IMP: R-30575-11662 */
      testcase( pTerm->wtFlags & TERM_CODED );
      if( pTerm->wtFlags & (TERM_VIRTUAL|TERM_CODED) ) continue;
      if( (pTerm->prereqAll & notReady)!=0 ){
        assert( pWInfo->untestedTerms );
        continue;
      }
      assert( pTerm->pExpr );
      sqlite3ExprIfFalse(pParse, pTerm->pExpr, addrCont, SQLITE_JUMPIFNULL);
      pTerm->wtFlags |= TERM_CODED;
    }
  }
  sqlite3ReleaseTempReg(pParse, iReleaseReg);

  return notReady;
}

#if defined(SQLITE_TEST)
/*
** The following variable holds a text description of query plan generated
** by the most recent call to sqlite3WhereBegin().  Each call to WhereBegin
** overwrites the previous.  This information is used for testing and
** analysis only.
<<<<<<< HEAD
以下变量保存的文本描述生成查询计划　　
通过最近的调用qlite3WhereBegin()。
每个调用WhereBegin覆盖前面的。
这些信息仅用于测试和分析。
*/
char sqlite3_query_plan[BMS*2*40];  /*加入文本 */
static int nQPlan = 0;              /*释放下一个in _query_plan[] */
//王秀超 从此结束
=======
**
** 下面的变量保存一个描述通过最新的调用sqlite3WhereBegin()生成的查询计划的文本。
** 每次调用WhereBegin重写先前的信息。这个信息只用于测试和分析。
*/
char sqlite3_query_plan[BMS*2*40];  /* Text of the join 连接的内容 */
static int nQPlan = 0;              /* Next free slow in _query_plan[] */

>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
#endif /* SQLITE_TEST */


/*

** Free a WhereInfo structure
**
** 释放一个WhereInfo数据结构
*/
static void whereInfoFree(sqlite3 *db, WhereInfo *pWInfo){
  if( ALWAYS(pWInfo) ){
    int i;
    for(i=0; i<pWInfo->nLevel; i++){
      sqlite3_index_info *pInfo = pWInfo->a[i].pIdxInfo;
      if( pInfo ){
        /* assert( pInfo->needToFreeIdxStr==0 || db->mallocFailed ); */
        if( pInfo->needToFreeIdxStr ){
          sqlite3_free(pInfo->idxStr);
        }
        sqlite3DbFree(db, pInfo);
      }
      if( pWInfo->a[i].plan.wsFlags & WHERE_TEMP_INDEX ){
        Index *pIdx = pWInfo->a[i].plan.u.pIdx;
        if( pIdx ){
          sqlite3DbFree(db, pIdx->zColAff);
          sqlite3DbFree(db, pIdx);
        }
      }
    }
    whereClauseClear(pWInfo->pWC);
    sqlite3DbFree(db, pWInfo);
  }
}


/*
** Generate the beginning of the loop used for WHERE clause processing.
** The return value is a pointer to an opaque structure that contains
** information needed to terminate the loop.  Later, the calling routine
** should invoke sqlite3WhereEnd() with the return value of this function
** in order to complete the WHERE clause processing.
产生用于循环的开始WHERE子句工艺加工的返回值是一个指向一个不透明的结构，它包含
?所需的信息，以终止该循环。后来，主叫例程应该为了完成WHERE子句处理调用sqlite3WhereEnd（）这个函数的返回值。
**
** 生成循环的开始用于WHERE子句的处理。
** 返回值是一个指针,它指向一个包含终止循环所需的信息的不透明的结构体。
** 稍后，调用程序会根据这个函数的返回值唤醒sqlite3WhereEnd()来完成WHERE子句的处理。
**
** If an error occurs, this routine returns NULL.
如果出现错误，这个例程返回null。
**
** 如果发生错误，这个程序将返回NULL.
**
** The basic idea is to do a nested loop, one loop for each table in
** the FROM clause of a select.  (INSERT and UPDATE statements are the
** same as a SELECT with only a single table in the FROM clause.)  For
** example, if the SQL is this:
其基本思路是做一个嵌套的循环，一个循环在每个表从一个选择像。
（INSERT和UPDATE语句相同的SELECT与在仅单个表FROM子句）。
例如，如果SQL是这样的：
**
**       SELECT * FROM t1, t2, t3 WHERE ...;
**
** Then the code generated is conceptually like the following:
**
**      foreach row1 in t1 do       \    Code generated
**        foreach row2 in t2 do      |-- by sqlite3WhereBegin()
**          foreach row3 in t3 do   /
**            ...
**          end                     \    Code generated
**        end                        |-- by sqlite3WhereEnd()
**      end                         /
**
**
** 基本的思路是进行循环嵌套，为一个查询语句的FROM子句中的每个表做一个循环。
** (INSERT和UPDATE命令与在FROM语句中只有一个表的SELECT相同)。例如:如果SQL是:
**       SELECT * FROM t1, t2, t3 WHERE ...;
** 那么会概念地生成以下代码:
**      foreach row1 in t1 do       \
**        foreach row2 in t2 do      |-- 由sqlite3WhereBegin()生成
**          foreach row3 in t3 do   /
**            ...
**          end                     \
**        end                        |-- 由sqlite3WhereEnd()生成
**      end                         /
**
**
** Note that the loops might not be nested in the order in which they
** appear in the FROM clause if a different order is better able to make
** use of indices.  Note also that when the IN operator appears in
** the WHERE clause, it might result in additional nested loops for
** scanning through all values on the right-hand side of the IN.
注意，该环可能不会被嵌套在它们出现在FROM子句如果以不同的顺序是
能够更好地利用索引的顺序。还要注意的是，当在操作者显示的WHERE子句中，
它可能会导致透过的IN的右手侧的所有值的附加的嵌套循环进行扫描。
**
** 注意:循环可能不是按FROM子句中他们出现的顺序进行嵌套，因为可能一个其他的嵌套顺序更适合使用索引。
** 还要注意;当WHERE子句中出现了IN操作符，它可能导致添加嵌套循环来扫描IN右边的所有值。
**
** There are Btree cursors associated with each table.  t1 uses cursor
** number pTabList->a[0].iCursor.  t2 uses the cursor pTabList->a[1].iCursor.
** And so forth.  This routine generates code to open those VDBE cursors
** and sqlite3WhereEnd() generates the code to close them.
有与每个表相关联的B树游标。T1使用光标号pTabList->一个[0].iCursor。
T2使用光标pTabList->一[1].iCursor。
这个程序生成代码来打开这些VDBE光标和sqlite3WhereEnd（）生成的代码来关闭它们。
**
** 有Btree游标与每个表相关联。t1使用游标数pTabList->a[0].iCursor.t2使用游标pTabList->a[1].iCursor.等等
** 这个程序生成代码来打开这些VDBE游标，sqlite3WhereEnd()生成代码来关闭他们。
**
** The code that sqlite3WhereBegin() generates leaves the cursors named
** in pTabList pointing at their appropriate entries.  The [...] code
** can use OP_Column and OP_Rowid opcodes on these cursors to extract
** data from the various tables of the loop.
这sqlite3WhereBegin（）生成的代码叶pTabList命名指着自己的相
应条目的光标。的[...]的代码可以使用OP_Column和OP_Rowid操作码
对这些光标从环路的各种表中提取数据
**
** sqlite3WhereBegin()生成的代码在pTabList中留下指定的游标指向他们恰当的条目。
** [...]编码可以使用在这些游标中的OP_Column和OP_Rowid操作码来从循环的各个表中提取数据。
**
** If the WHERE clause is empty, the foreach loops must each scan their
** entire tables.  Thus a three-way join is an O(N^3) operation.  But if
** the tables have indices and there are terms in the WHERE clause that
** refer to those indices, a complete table scan can be avoided and the
** code will run much faster.  Most of the work of this routine is checking
** to see if there are indices that can be used to speed up the loop.
<<<<<<< HEAD
**如果WHERE子句是空的，foreach循环必须在每次扫描他们的整个表。
因此，一个三路连接是O（N^3）操作。但是，如果表有索引并有在
WHERE集中像指那些索引，一个完整的表扫描，可避免和代码将
运行得更快。大部分该程序的工作是检查
以查看是否有索引可以用来加速循环。
=======
**
** 如果WHERE子句是空的，每一个循环必须每次扫描整个表。因此一个3表连接是一个O(N^3)操作。
** 但是如果表有索引并且在WHERE子句中有terms与那些索引相关联，一个可以避免完整的表扫描并且代码运行的更快。
** 这个程序大部分的工作是检查是否有可以使用的索引来加速循环。
**
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
** Terms of the WHERE clause are also used to limit which rows actually
** make it to the "..." in the middle of the loop.  After each "foreach",
** terms of the WHERE clause that use only terms in that loop and outer
** loops are evaluated and if false a jump is made around all subsequent
** inner loops (or around the "..." if the test occurs within the inner-
** most loop)
像WHER集也用来限制哪些行实际上使它的“...”，在循环的中间。
每一个“的foreach”后，使用在循环和外循环只计算WHERE子句中的项
进行评估，如果假跳转围绕所有后续内环取得（或围绕“......”如果
测试中出现的inner-最回路）

**
** WHERE子句的terms也被用于限制在循环的中部哪些行使它成为"...".
** 每次循环后，WHERE子句的terms只使用在那个循环和外部循环评估过的terms.
** 并且如果错误就跳过所有后续的内部循环(或如果测试发生在最内部循环中，那么就跳过"...")
**
** OUTER JOINS
**
** An outer join of tables t1 and t2 is conceptally coded as follows:
**
**    foreach row1 in t1 do
**      flag = 0
**      foreach row2 in t2 do
**        start:
**          ...
**          flag = 1
**      end
**      if flag==0 then
**        move the row2 cursor to a null row
**        goto start
**      fi
**    end
**
**
** OUTER JOINS
** 一个表t1和t2的外部链接会在概念上生成如下代码:
**    foreach row1 in t1 do
**      flag = 0
**      foreach row2 in t2 do
**        start:
**          ...
**          flag = 1
**      end
**      if flag==0 then
**        move the row2 cursor to a null row
**        goto start
**      fi
**    end
**
**
** ORDER BY CLAUSE PROCESSING
ORDER BY集处理
**
** *ppOrderBy is a pointer to the ORDER BY clause of a SELECT statement,
** if there is one.  If there is no ORDER BY clause or if this routine
** is called from an UPDATE or DELETE statement, then ppOrderBy is NULL.
**ppOrderBy是一个指向ORDER BY的SELECT语句的WHERE集，
如果有一个。如果没有ORDER BY子句，或者如果这个程序被
称为从UPDATE或DELETE语句，然后ppOrderBy为NULL。
** If an index can be used so that the natural output order of the table
** scan is correct for the ORDER BY clause, then that index is used and
** *ppOrderBy is set to NULL.  This is an optimization that prevents an
** unnecessary sort of the result set if an index appropriate for the
** ORDER BY clause already exists.
如果一个索引可以用来使表扫描的自然输出顺序是正确的ORDER BY集，
则该索引的使用和ppOrderBy设置为NULL。这是阻止的结果，如果已经存
在的指数适合于ORDER BY子句中设置的不必要的排序优化..
**
** If the where clause loops cannot be arranged to provide the correct
** output order, then the *ppOrderBy is unchanged.
<<<<<<< HEAD
如果where子句循环无法安排提供正确的
输出顺序，那么* ppOrderBy不变。
*/
WhereInfo *sqlite3WhereBegin(
  Parse *pParse,        /* The parser context  解析器的环境*/
  SrcList *pTabList,    /* A list of all tables to be scanned  要扫描的所有表的列表*/
  Expr *pWhere,         /* The WHERE clause  WHERE集*/
  ExprList **ppOrderBy, /* An ORDER BY clause, or NULL  ORDER BY集，或NULL*/
  ExprList *pDistinct,  /* The select-list for DISTINCT queries - or NULL  选择列表中的DISTINCT查询 - 或NULL*/
  u16 wctrlFlags,       /* One of the WHERE_* flags defined in sqliteInt.h  一个在sqliteInt.h定义的WHERE_*标志 */
  int iIdxCur           /* If WHERE_ONETABLE_ONLY is set, index cursor number 如果WHERE_ONETABLE_ONLY被设置，索引光标号*/
){
  int i;                     /* Loop counter  循环计数器*/
  int nByteWInfo;            /* Num. bytes allocated for WhereInfo struct 分配给WhereInfo结构字节*/
  int nTabList;              /* Number of elements in pTabList  在pTabList元素的数量*/
  WhereInfo *pWInfo;         /* Will become the return value of this function  将成为该函数的返回值*/
  Vdbe *v = pParse->pVdbe;   /* The virtual database engine  虚拟数据库引擎*/
  Bitmask notReady;          /* Cursors that are not yet positioned  那些尚未定位游标*/
  WhereMaskSet *pMaskSet;    /* The expression mask set  表达mask组*/
  WhereClause *pWC;               /* Decomposition of the WHERE clause  WHERE集分解*/
  struct SrcList_item *pTabItem;  /* A single entry from pTabList  从pTabList单个条目*/
  WhereLevel *pLevel;             /* A single level in the pWInfo list  在pWInfo列表中的单个水平*/
  int iFrom;                      /* First unused FROM clause element 第一个未使用FROM子句中的元素 */
  int andFlags;              /* AND-ed combination of all pWC->a[].wtFlags */
  sqlite3 *db;               /* Database connection  数据库连接*/

  /* The number of tables in the FROM clause is limited by the number of
  ** bits in a Bitmask 
  在from子集中表的数量被bitmask中的比特数量
=======
**
** ORDER BY子句处理
** 如果有ORDER BY子句，那么*ppOrderBy是一个指针，指向一个SELECT命令的ORDER BY子句。
** 如果没有ORDER BY子句或是UPDATE，DELETE调用的这个程序，那么ppOrderBy为NULL.
** 如果可以使用索引以便扫描的自然输出顺序是根据ORDER BY子句矫正的，那么使用索引并且*ppOrderBy设置为NULL.
** 如果一个适用于ORDER BY子句的索引已经存在，是一个防止结果集不必要的排序的优化
** 如果不能安排WHERE子句循环提供正确的输出顺序，那么不改变*ppOrderBy的值。
**
*/
WhereInfo *sqlite3WhereBegin(
  Parse *pParse,        /* The parser context 分析上下文 */
  SrcList *pTabList,    /* A list of all tables to be scanned 被扫描的所有表的一个列表 */
  Expr *pWhere,         /* The WHERE clause WHERE子句 */
  ExprList **ppOrderBy, /* An ORDER BY clause, or NULL 一个ORDER BY子句或NULL*/
  ExprList *pDistinct,  /* The select-list for DISTINCT queries - or NULL DISTINCT查询的查询列表或NULL */
  u16 wctrlFlags,       /* One of the WHERE_* flags defined in sqliteInt.h 在sqliteInt.h中定义的WHERE_*中的一个 */
  int iIdxCur           /* If WHERE_ONETABLE_ONLY is set, index cursor number 如果设置了WHERE_ONETABLE_ONLY，则为索引游标数 */
){
  int i;                     /* Loop counter 循环计数器 */
  int nByteWInfo;            /* Num. bytes allocated for WhereInfo struct 为WhereInfo结构分配的字节数 */
  int nTabList;              /* Number of elements in pTabList 在pTabList中的元素数 */
  WhereInfo *pWInfo;         /* Will become the return value of this function 将变成这个函数的返回值 */
  Vdbe *v = pParse->pVdbe;   /* The virtual database engine 虚拟数据库引擎 */
  Bitmask notReady;          /* Cursors that are not yet positioned 还未确定位置的游标 */
  WhereMaskSet *pMaskSet;    /* The expression mask set 设置表达式掩码 */
  WhereClause *pWC;               /* Decomposition of the WHERE clause WHERE子句的分解 */
  struct SrcList_item *pTabItem;  /* A single entry from pTabList 来自于pTabList的一个条目 */
  WhereLevel *pLevel;             /* A single level in the pWInfo list 在pWInfo列表中的一个登记 */
  int iFrom;                      /* First unused FROM clause element 第一个未使用的FROM子句元素 */
  int andFlags;              /* AND-ed combination of all pWC->a[].wtFlags AND-ed所有的pWC->a[].wtFlags组合 */
  sqlite3 *db;               /* Database connection 数据库连接 */

  /* The number of tables in the FROM clause is limited by the number of
  ** bits in a Bitmask 
  **
  ** 在一个位掩码中的位数限制了在FROM子句中的表数目
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  testcase( pTabList->nSrc==BMS );	//用于覆盖测试
  if( pTabList->nSrc>BMS ){	//如果在FROM中的表数目大于位掩码中的位数
    sqlite3ErrorMsg(pParse, "at most %d tables in a join", BMS);	//提示连接中最多只能有BMS个表
    return 0;
  }

  /* This function normally generates a nested loop for all tables in 
  ** pTabList.  But if the WHERE_ONETABLE_ONLY flag is set, then we should
  ** only generate code for the first table in pTabList and assume that
  ** any cursors associated with subsequent tables are uninitialized.
<<<<<<< HEAD
  该功能通常会产生一个嵌套循环中pTabList所有表。
  但如果WHERE_ONETABLE_ONLY标志设置，那么我们应该只
  生成代码为pTabList第一个表，并假定随后表相关的任何游标初始化。
=======
  **
  ** 这个函数一般是为在pTabList中的所有表生成一个嵌套循环。
  ** 但是如果设置了WHERE_ONETABLE_ONLY标志，
  ** 那么我们只需要为pTabList中的第一个表生成代码并且假设任何与后续的表相关的游标都是未初始化的。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  nTabList = (wctrlFlags & WHERE_ONETABLE_ONLY) ? 1 : pTabList->nSrc;//如果只有一个表，那么nTabList就为1，否则等于pTabList->nSrc

  /* Allocate and initialize the WhereInfo structure that will become the
  ** return value. A single allocation is used to store the WhereInfo
  ** struct, the contents of WhereInfo.a[], the WhereClause structure
  ** and the WhereMaskSet structure. Since WhereClause contains an 8-byte
  ** field (type Bitmask) it must be aligned on an 8-byte boundary on
  ** some architectures. Hence the ROUND8() below.
<<<<<<< HEAD
  分配和初始化WhereInfo结构将成为返回值。
  单个分配用于存储所述WhereInfo结构，WhereInfo.a[]中，
  WhereClause结构的内容和WhereMaskSet结构。
  因为WhereClause包含一个8字节的字段（类型位掩码）
  它必须对某些体系结构的8字节边界对齐。因此ROUND8（）以下。
=======
  **
  ** 分配和初始化WhereInfo数据结构将变成返回值。
  ** 一个单独的分配被用于存储WhereInfo结构，WhereInfo.a[]的内容，WhereClause数据结构和WhereMaskSet数据结构。
  ** 因为WhereClause包含一个8字节的字段必须在结构上与一个8字节边界对齐。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  db = pParse->db;	//连接数据库
  nByteWInfo = ROUND8(sizeof(WhereInfo)+(nTabList-1)*sizeof(WhereLevel));	//为WhereInfo分配相应的字节数
  pWInfo = sqlite3DbMallocZero(db, 
      nByteWInfo + 
      sizeof(WhereClause) +
      sizeof(WhereMaskSet)
  );	//为WhereInfo分配空间
  if( db->mallocFailed ){	//如果数据库内存分配失败
    sqlite3DbFree(db, pWInfo);	//释放数据库连接的内存
    pWInfo = 0;	//清空分配的空间。
    goto whereBeginError;	//跳转到whereBeginError处理错误
  }
  pWInfo->nLevel = nTabList;//循环嵌套数为表的数目
  pWInfo->pParse = pParse;
  pWInfo->pTabList = pTabList;	//WhereInfo中的表列表与pTabList相同
  pWInfo->iBreak = sqlite3VdbeMakeLabel(v);	//终止循环的标志
  pWInfo->pWC = pWC = (WhereClause *)&((u8 *)pWInfo)[nByteWInfo];
  pWInfo->wctrlFlags = wctrlFlags;
  pWInfo->savedNQueryLoop = pParse->nQueryLoop;	//一个查询的迭代数
  pMaskSet = (WhereMaskSet*)&pWC[1];

  /* Disable the DISTINCT optimization if SQLITE_DistinctOpt is set via
  ** sqlite3_test_ctrl(SQLITE_TESTCTRL_OPTIMIZATIONS,...) 
<<<<<<< HEAD
  禁用DISTINCT的优化，如果SQLITE_DistinctOpt通过sqlite3_test_ctrl设置
  （SQLITE_TESTCTRL_OPTIMIZATIONS，...）*/
=======
  **
  ** 如果通过sqlite3_test_ctrl(SQLITE_TESTCTRL_OPTIMIZATIONS,...)设置SQLITE_DistinctOpt那么就禁用DISTINCT优化
  */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  if( db->flags & SQLITE_DistinctOpt ) pDistinct = 0;

  /* Split the WHERE clause into separate subexpressions where each
  ** subexpression is separated by an AND operator.
<<<<<<< HEAD

  分裂WHERE集成独立的集表达式，其中每个集表达式由一个AND运算分离。
=======
  **
  ** 把WHERE子句通过AND运算符分割成多个子表达式。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  initMaskSet(pMaskSet);	//初始化WhereMaskSet对象
  whereClauseInit(pWC, pParse, pMaskSet, wctrlFlags);	//初始化pWC
  sqlite3ExprCodeConstants(pParse, pWhere); //预先计算在pWhere中的常量字表达式
  whereSplit(pWC, pWhere, TK_AND);  //把WHERE子句通过AND运算符分割成多个子表达式。 /* IMP: R-15842-53296 */
    
  /* Special case: a WHERE clause that is constant.  Evaluate the
  ** expression and either jump over all of the code or fall thru.
<<<<<<< HEAD
  特殊情况：一个WHERE子集是恒定的。计算表达式，要么跳过所有的代码或下降。
=======
  **
  ** 特殊情况:一个WHERE子句是恒定的。对表达式求值时，要么跳过所有的代码，要么通过
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  if( pWhere && (nTabList==0 || sqlite3ExprIsConstantNotJoin(pWhere)) ){
    sqlite3ExprIfFalse(pParse, pWhere, pWInfo->iBreak, SQLITE_JUMPIFNULL);
    pWhere = 0;
  }

  /* Assign a bit from the bitmask to every term in the FROM clause.
<<<<<<< HEAD
  **分配一个位的掩码在每个象FROM子集。
=======
  **
  ** 将位掩码中的一bit分配给FROM子句的每个term。
  **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  ** When assigning bitmask values to FROM clause cursors, it must be
  ** the case that if X is the bitmask for the N-th FROM clause term then
  ** the bitmask for all FROM clause terms to the left of the N-th term
  ** is (X-1).   An expression from the ON clause of a LEFT JOIN can use
  ** its Expr.iRightJoinTable value to find the bitmask of the right table
  ** of the join.  Subtracting one from the right table bitmask gives a
  ** bitmask for all tables to the left of the join.  Knowing the bitmask
  ** for all tables to the left of a left join is important.  Ticket #3015.
  当指定位掩码值FROM子句光标，它一定是这样的，
  如果X是位掩码N个FROM子集中短期的，则掩码为所有FROM子集条件的N个任期的左
为（X-1）。从LEFT的ON子句表达式JOIN可以使用它Expr.iRightJoinTable
价值发现的加入右表的掩码。来自右表位掩码中减去1给出了一个
位掩码为所有表的加盟左侧。知道位掩码对于所有表左连接的左侧
是重要的。票务＃3015
  **
  ** 当把位掩码值分配给FROM子句游标时，如果X是N-th FROM子句项的位掩码，
  ** 那么所有FROM子句terms的左边的第N项的位掩码是(X-1)。
  ** 一个来自于LEFT JOIN的ON子句的表达式可以使用它自己的Expr.iRightJoinTable值来查找这个连接的右表的位掩码。
  ** 从右边表的位掩码中减去一，把这个位掩码给连接的左边的所有的表。
  ** 要知道一个左联接左边的所有表的位掩码是很重要的。
  **
  ** Configure the WhereClause.vmask variable so that bits that correspond
  ** to virtual table cursors are set. This is used to selectively disable 
  ** the OR-to-IN transformation in exprAnalyzeOrTerm(). It is not helpful
  ** with virtual tables.
  配置WhereClause.vmask变量，以便对应于虚表指针位设置。
  这是用来选择性地禁用exprAnalyzeOrTerm的或到IN变换（）。
  这对虚拟表是没有帮助的。
  **
  ** 设置WhereClause.vmask变量以便bits与设置好的虚拟表的游标相一致。
  ** 这用于在exprAnalyzeOrTerm()中有选择性地禁用OR-to-IN转化。它对虚拟表示无用的。
  **
  ** Note that bitmasks are created for all pTabList->nSrc tables in
  ** pTabList, not just the first nTabList tables.  nTabList is normally
  ** equal to pTabList->nSrc but might be shortened to 1 if the
  ** WHERE_ONETABLE_ONLY flag is set.
<<<<<<< HEAD
  需要注意的是位掩码为所有pTabList-> NSRC表中pTabList，
  不只是第一个nTabList表创建。nTabList通常等于pTabList-> NSRC但可能缩短为1，
  如果该WHERE_ONETABLE_ONLY标志被设置。
=======
  **
  ** 注意:不只是为第一个nTabList表创建位掩码，而是为在pTabList中的所有pTabList->nSrc表创建。
  ** nTabList一般等同于pTabList->nSrc，但如果设置了WHERE_ONETABLE_ONLY标志，那么它可能缩短为1。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  assert( pWC->vmask==0 && pMaskSet->n==0 );
  for(i=0; i<pTabList->nSrc; i++){	//遍历表，为FROM中的每个表游标iCursor创建掩码
    createMask(pMaskSet, pTabList->a[i].iCursor);
#ifndef SQLITE_OMIT_VIRTUALTABLE
    if( ALWAYS(pTabList->a[i].pTab) && IsVirtual(pTabList->a[i].pTab) ){	//如果表是虚表
      pWC->vmask |= ((Bitmask)1 << i);	//设置识别虚表游标的的位掩码
    }
#endif
  }
#ifndef NDEBUG
  {
    Bitmask toTheLeft = 0;
    for(i=0; i<pTabList->nSrc; i++){ //循环遍历FROM子句中的表或子查询
      Bitmask m = getMask(pMaskSet, pTabList->a[i].iCursor);
      assert( (m-1)==toTheLeft );
      toTheLeft |= m; //设置toTheLeft
    }
  }
#endif

  /* Analyze all of the subexpressions.  Note that exprAnalyze() might
  ** add new virtual terms onto the end of the WHERE clause.  We do not
  ** want to analyze these virtual terms, so start analyzing at the end
  ** and work forward so that the added virtual terms are never processed.
<<<<<<< HEAD
  分析所有的子表达式。需要注意的是exprAnalyze（）可能会添加新的虚拟
  条件到WHERE子句的末尾。我们不希望来分析这些虚拟象，所以开始在端
  分析和工作向前，以使增加了从未处理的虚拟象。
=======
  **
  ** 分析所有的子表达式。注意exprAnalyze()可能添加新的虚拟项到WHERE子句的末尾。
  ** 我们不想分析这些虚拟项，所以最后开始分析并且较早的使用，所以被添加的虚拟项从未被处理。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  exprAnalyzeAll(pTabList, pWC);//分析where子句中的所有terms
  if( db->mallocFailed ){  //如果数据库内存分配错误
    goto whereBeginError; //跳转到whereBeginError处理错误
  }

  /* Check if the DISTINCT qualifier, if there is one, is redundant. 
  ** If it is, then set pDistinct to NULL and WhereInfo.eDistinct to
  ** WHERE_DISTINCT_UNIQUE to tell the caller to ignore the DISTINCT.
<<<<<<< HEAD
  检查是否DISTINCT限定符，如果有一个，是多余的。如果是，则设置pDistinct为NULL，
  WhereInfo.eDistinct到WHERE_DISTINCT_UNIQUE告诉调用者忽略DISTINCT。
=======
  **
  ** 检查DISTINCT限定词是否是多余的。如果是，
  ** 那么把pDistinct设置为NULL并且把WhereInfo.eDistinct设置为WHERE_DISTINCT_UNIQUE来告诉调用者忽略DISTINCT.
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  if( pDistinct && isDistinctRedundant(pParse, pTabList, pWC, pDistinct) ){ //如果Distinct是多余的
    pDistinct = 0; //清空pDistinct
    pWInfo->eDistinct = WHERE_DISTINCT_UNIQUE; //设置WhereInfo，告诉调用者忽略DISTINCT
  }

  /* Chose the best index to use for each table in the FROM clause.
<<<<<<< HEAD
  **选择在为每个表使用FROM子集中的最好索引。
=======
  **
  ** 在FROM子句中为每个表选择使用最好的索引
  **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  ** This loop fills in the following fields:
  循环填写以下字段：
  **
  **   pWInfo->a[].pIdx      The index to use for this level of the loop.该指数用于循环的这个水平
  **   pWInfo->a[].wsFlags   WHERE_xxx flags associated with pIdx WHERE_xxx与PIDX相关的标志
  **   pWInfo->a[].nEq       The number of == and IN constraints ==和IN限制数量
  **   pWInfo->a[].iFrom     Which term of the FROM clause is being coded 其中的FROM子集中被编码的象
  **   pWInfo->a[].iTabCur   The VDBE cursor for the database table 该VDBE光标数据库表
  **   pWInfo->a[].iIdxCur   The VDBE cursor for the index 该VDBE光标索引
  **   pWInfo->a[].pTerm     When wsFlags==WO_OR, the OR-clause term 当wsFlags== WO_OR时，OR-象
  **
  ** This loop also figures out the nesting order of tables in the FROM
  ** clause.
<<<<<<< HEAD
  这个循环也可以计算表在FROM子集中的嵌套顺序。
=======
  **
  ** 这个循环由以下域填充:
  **   pWInfo->a[].pIdx      为循环等级使用的索引
  **   pWInfo->a[].wsFlags   与pIdx有关的WHERE_xxx标志
  **   pWInfo->a[].nEq       ==和IN约束的数目
  **   pWInfo->a[].iFrom     被编译的FROM子句项
  **   pWInfo->a[].iTabCur   由于数据库表的VDBE游标
  **   pWInfo->a[].iIdxCur   用于索引的VDBE游标
  **   pWInfo->a[].pTerm     当wsFlags==WO_OR时，OR子句项
  ** 这个循环也解决在FROM子句中的表的嵌套顺序。
  **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  notReady = ~(Bitmask)0;//~取反，
  andFlags = ~0;
  WHERETRACE(("*** Optimizer Start ***\n"));
<<<<<<< HEAD
  for(i=iFrom=0, pLevel=pWInfo->a; i<nTabList; i++, pLevel++){
    WhereCost bestPlan;         /* Most efficient plan seen so far  迄今看到最有效的计划*/
    Index *pIdx;                /* Index for FROM table at pTabItem  从表中pTabItem索引*/
    int j;                      /* For looping over FROM tables  对于遍历FROM表*/
    int bestJ = -1;             /* The value of j j的值*/
    Bitmask m;                  /* Bitmask value for j or bestJ  对于J或bestJ位掩码值*/
    int isOptimal;              /* Iterator for optimal/non-optimal search  迭代最佳/非最佳搜索*/
    int nUnconstrained;         /* Number tables without INDEXED BY  INT nUnconstrained; /*号码表没有收录*/*/
    Bitmask notIndexed;         /* Mask of tables that cannot use an index  mask的表可以不使用索引*/

    memset(&bestPlan, 0, sizeof(bestPlan));
    bestPlan.rCost = SQLITE_BIG_DBL;
=======
  for(i=iFrom=0, pLevel=pWInfo->a; i<nTabList; i++, pLevel++){	//循环遍历FROM子句中的表列表
    WhereCost bestPlan;         /* Most efficient plan seen so far 到目前为止找到的最有效的计划 */
    Index *pIdx;                /* Index for FROM table at pTabItem 在pTabItem中FROM子句中表使用的索引 */
    int j;                      /* For looping over FROM tables 循环遍历FROM子句中的表 */
    int bestJ = -1;             /* The value of j j的值 */
    Bitmask m;                  /* Bitmask value for j or bestJ j或bestJ的位掩码的值 */
    int isOptimal;              /* Iterator for optimal/non-optimal search 最佳/非最佳搜索的迭代 */
    int nUnconstrained;         /* Number tables without INDEXED BY 没有INDEXED BY的表数目 */
    Bitmask notIndexed;         /* Mask of tables that cannot use an index 不能使用一个索引的表掩码 */

    memset(&bestPlan, 0, sizeof(bestPlan)); //为bestPlan分配内存
    bestPlan.rCost = SQLITE_BIG_DBL; //初始化执行bestPlan的总体成本
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    WHERETRACE(("*** Begin search for loop %d ***\n", i));

    /* Loop through the remaining entries in the FROM clause to find the
    ** next nested loop. The loop tests all FROM clause entries
    ** either once or twice. 
	通过在FROM子句中的剩余项循环寻找下一个嵌套循环。
	循环测试所有的FROM子句中的象一次或两次。
    **
    ** 循环通过在FROM子句的项来查找下一个嵌套循环。循环测试一次或两次所有的FROM子句项。
    **
    ** The first test is always performed if there are two or more entries
    ** remaining and never performed if there is only one FROM clause entry
    ** to choose from.  The first test looks for an "optimal" scan.  In
    ** this context an optimal scan is one that uses the same strategy
    ** for the given FROM clause entry as would be selected if the entry
    ** were used as the innermost nested loop.  In other words, a table
    ** is chosen such that the cost of running that table cannot be reduced
    ** by waiting for other tables to run first.  This "optimal" test works
    ** by first assuming that the FROM clause is on the inner loop and finding
    ** its query plan, then checking to see if that query plan uses any
    ** other FROM clause terms that are notReady.  If no notReady terms are
    ** used then the "optimal" query plan works.
<<<<<<< HEAD
    **总是执行第一测试，如果有剩余的两个或更多个条目，
	并从未执行，如果仅存在一个FROM子集条目选择。第一个
	测试寻找一个“最佳”的扫描。在这种情况下的最佳的扫描
	是一个使用相同的策略用于给定FROM子集的象作为会如果
	条目被用作最内层嵌套循环来选择。换言之，一个表被选
	择为使得在运行该表的成本无法通过等待其它表先运行减少。
	首先假设FROM子句是在内部循环，并找到自己的查询计划，
	然后检查，看看是否能查询计划使用任何其他FROM子集条
	件是未就绪这个“最佳”的测试工作。如果没有使用未就绪条款，
	然后在“最佳”的查询计划的运作。
=======
    **
    ** 如果有两个或更多的项剩余那么总是执行第一次测试，如果只有一个FROM子句项可供选择，那么就从不执行。
    ** 第一次测试查找一个"最佳的"扫描。在这种情况下如果条目被用作最内层嵌套循环，
    ** 那么最优扫描会为给定的FROM子句使用相同的策略条目。
    ** 换句话说，选中一个表，该表不能通过等待其他表先运行来减少运行代价。
    ** 这个"最佳的"测试通过第一次假定FROM子句是在内部循环和查找它的查询计划时起作用，
    ** 那么检查是否查询优化使用的其他的所有FROM子句是未准备的。
    ** 如果没有使用未准备的条目，那么"最佳的"查询计划会起作用。
    **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    ** Note that the WhereCost.nRow parameter for an optimal scan might
    ** not be as small as it would be if the table really were the innermost
    ** join.  The nRow value can be reduced by WHERE clause constraints
    ** that do not use indices.  But this nRow reduction only happens if the
    ** table really is the innermost join.  
	注意，对于一个最佳扫描WhereCost.nRow参数可能不小，因为它是，
	如果表真的是最里面的联接。该nRow值可以通过WHERE子句约束不使
	用指数减少。但这nRow降低只发生如果表真的是最里面的加入的情况下。
    **
    ** 注意:对于一个最佳查询的WhereCost.nRow参数可能不会像表在最里层连接时那样小。
    ** nRow值可以通过不使用索引的WHERE子句约束来减小。但是这个减小的nRow值发生在表真的是最里面的连接。
    **
    ** The second loop iteration is only performed if no optimal scan
    ** strategies were found by the first iteration. This second iteration
    ** is used to search for the lowest cost scan overall.
	仅执行第二循环迭代如果由第一迭代没有找到最佳的扫描策略。
	这第二次迭代被用于搜索成本最低扫描整体。

    **
    ** 如果第一次没有发现最优扫描策略那么才执行第二次循环迭代。第二次迭代用于查找全体扫描的最低代价。
    **
    ** Previous versions of SQLite performed only the second iteration -
    ** the next outermost loop was always that with the lowest overall
    ** cost. However, this meant that SQLite could select the wrong plan
    ** for scripts such as the following:
    **   以前版本的SQLite只进行了第二次迭代-The接下来最外层循环总是与最低的总成本。但是，这意味着SQLite的可能选择了错误的计划
为脚本，如以下几点：
    **   CREATE TABLE t1(a, b); 创建表t1（a,b）
    **   CREATE TABLE t2(c, d);创建表t2(c,d)
    **   SELECT * FROM t2, t1 WHERE t2.rowid = t1.a;
	从t1，t2中查询，条件是t2的rowid的值等于t1的a值
    **
    ** The best strategy is to iterate through table t1 first. However it
    ** is not possible to determine this with a simple greedy algorithm.
    ** Since the cost of a linear scan through table t2 is the same 
    ** as the cost of a linear scan through table t1, a simple greedy 
    ** algorithm may choose to use t2 for the outer loop, which is a much
    ** costlier approach.
<<<<<<< HEAD
	最好的策略是通过表T1迭代第一。然而，它是无法确定这用一个简单的贪婪算法。
	由于通过表t2线性扫描的成本是相同的，通过表t1线性扫描的成本，
	简单的贪婪算法可以选择使用t2的用于外循环，这是一种更昂贵的方法。
=======
    **
    ** SQLite以前的版本只执行第二次迭代--下一个最外层的循环总是总成本最低的。
    ** 然而，这意味着SQLite可能选择错误的计划，例如下面这个例子:
    **   CREATE TABLE t1(a, b); 
    **   CREATE TABLE t2(c, d);
    **   SELECT * FROM t2, t1 WHERE t2.rowid = t1.a;
    ** 最好的策略是先循环访问表t1。然而，使用一个简单的贪心算法是不能决定这个的。
    ** 由于线性扫描表t2的代价和线性扫描表t1的代价相同，所以一个简单的贪心算法可能把表t2放在外部循环。(这种策略代价较高)
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    nUnconstrained = 0; //初始化nUnconstrained
    notIndexed = 0; //初始化notIndexed
    for(isOptimal=(iFrom<nTabList-1); isOptimal>=0 && bestJ<0; isOptimal--){ //循环一次或两次来测试所有的FROM子句项
      Bitmask mask;             /* Mask of tables not yet ready 还未准备的表掩码 */
      for(j=iFrom, pTabItem=&pTabList->a[j]; j<nTabList; j++, pTabItem++){ //循环遍历FROM子句中的表
        int doNotReorder;    /* True if this table should not be reordered 如果不能记录这个表，那么为TRUE */
        WhereCost sCost;     /* Cost information from best[Virtual]Index() best[Virtual]Index()中的代价信息 */
        ExprList *pOrderBy;  /* ORDER BY clause for index to optimize 索引优化的ORDER BY子句 */
        ExprList *pDist;     /* DISTINCT clause for index to optimize 索引优化的DISTINCT子句 */
  
        doNotReorder =  (pTabItem->jointype & (JT_LEFT|JT_CROSS))!=0; //如果是左连接或CROSS连接，则记录这个表
        if( j!=iFrom && doNotReorder ) break;
        m = getMask(pMaskSet, pTabItem->iCursor);
        if( (m & notReady)==0 ){
          if( j==iFrom ) iFrom++;
          continue;
        }
        mask = (isOptimal ? m : notReady);
        pOrderBy = ((i==0 && ppOrderBy )?*ppOrderBy:0);
        pDist = (i==0 ? pDistinct : 0);
        if( pTabItem->pIndex==0 ) nUnconstrained++;  //计算没有使用INDEXED BY子句的表数目
  
        WHERETRACE(("=== trying table %d with isOptimal=%d ===\n",
                    j, isOptimal));
        assert( pTabItem->pTab ); //判断pTabItem->pTab是否为NULL
#ifndef SQLITE_OMIT_VIRTUALTABLE
        if( IsVirtual(pTabItem->pTab) ){ //判断表是否为虚表
          sqlite3_index_info **pp = &pWInfo->a[j].pIdxInfo; //初始化**pp
          bestVirtualIndex(pParse, pWC, pTabItem, mask, notReady, pOrderBy,
                           &sCost, pp); //获得虚表的最佳索引
        }else 
#endif
        {
          bestBtreeIndex(pParse, pWC, pTabItem, mask, notReady, pOrderBy,
              pDist, &sCost); //最好的Btree索引
        }
        assert( isOptimal || (sCost.used&notReady)==0 );

        /* If an INDEXED BY clause is present, then the plan must use that
        ** index if it uses any index at all 
        **
        ** 如果出现一个INDEXED BY子句，那么计划必须使用ORDER BY子句使用的索引。
        */
        assert( pTabItem->pIndex==0 
                  || (sCost.plan.wsFlags & WHERE_NOT_FULLSCAN)==0
                  || sCost.plan.u.pIdx==pTabItem->pIndex );

        if( isOptimal && (sCost.plan.wsFlags & WHERE_NOT_FULLSCAN)==0 ){
          notIndexed |= m;
        }

        /* Conditions under which this table becomes the best so far:
		条件下这个表成为迄今为止最好的

        **
        **   (1) The table must not depend on other tables that have not
        **       yet run.该表必须不依赖于有没有其他表
?但运行。
        **
        **   (2) A full-table-scan plan cannot supercede indexed plan unless
        **       the full-table-scan is an "optimal" plan as defined above.
		全表扫描计划无法supercede索引的计划，除非
全表扫描是“最优”的计划定义如上。
	
        **
        **   (3) All tables have an INDEXED BY clause or this table lacks an
        **       INDEXED BY clause or this table uses the specific
        **       index specified by its INDEXED BY clause.  This rule ensures
        **       that a best-so-far is always selected even if an impossible
        **       combination of INDEXED BY clauses are given.  The error
        **       will be detected and relayed back to the application later.
        **       The NEVER() comes about because rule (2) above prevents
        **       An indexable full-table-scan from reaching rule (3).
        **所有的表都索引BY子集或该表中没有索引BY子句或该表使用其收录子句
		指定的特定索引。此规则确保了最佳的那么远总是选择即使一个不可能的
		组合收录条款给出。将被检测并传回给应用程序后的误差。
该NEVER（）来约，因为规则（2）上述防止可转位全表扫描到达规则（3）。
        **   (4) The plan cost must be lower than prior plans or else the
        **       cost must be the same and the number of rows must be lower.
<<<<<<< HEAD
		该计划的成本必须比现有的计划是低级否则成本必须是相同的，并行的数目必须是较低的。
=======
        **
        **
        **  下面是说明这个表到目前为止是最好的的条件:
        **   (1) 表不能依赖于其他还未运行的表
        **   (2) 一个全表扫描计划不能取代有索引的计划，除非全表扫描是一个上面定义的"最佳的"计划
        **   (3) 所有的表都有一个INDEXED BY子句或这个表缺乏一个INDEXED BY子句或这个表使用特殊的通过它的INDEXED BY子句说明的特殊索引。
        **       这个规定确保总是选定一个目前为止最好的，即便是给出一个不可能的INDEXED BY子句组合。
        **       检查到错误并且稍后将传送回应用。NEVER()出现是因为上面的规则(2)阻止一个可加索引的全表扫描来满足规则(3)。
        **   (4) 计划的代价必须小于前一个计划，或代价相同并且行数比较少。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
        */
        if( (sCost.used&notReady)==0                       /* (1) */
            && (bestJ<0 || (notIndexed&m)!=0               /* (2) */
                || (bestPlan.plan.wsFlags & WHERE_NOT_FULLSCAN)==0
                || (sCost.plan.wsFlags & WHERE_NOT_FULLSCAN)!=0)
            && (nUnconstrained==0 || pTabItem->pIndex==0   /* (3) */
                || NEVER((sCost.plan.wsFlags & WHERE_NOT_FULLSCAN)!=0))
            && (bestJ<0 || sCost.rCost<bestPlan.rCost      /* (4) */
                || (sCost.rCost<=bestPlan.rCost 
                 && sCost.plan.nRow<bestPlan.plan.nRow))
        ){
          WHERETRACE(("=== table %d is best so far"
                      " with cost=%g and nRow=%g\n",
                      j, sCost.rCost, sCost.plan.nRow));
          bestPlan = sCost; //最有效的计划的代价
          bestJ = j;
        }
        if( doNotReorder ) break;
      }
    }
    assert( bestJ>=0 );
    assert( notReady & getMask(pMaskSet, pTabList->a[bestJ].iCursor) );
    WHERETRACE(("*** Optimizer selects table %d for loop %d"
                " with cost=%g and nRow=%g\n",
                bestJ, pLevel-pWInfo->a, bestPlan.rCost, bestPlan.plan.nRow));
    /* The ALWAYS() that follows was added to hush up clang scan-build 下面添加ALWAYS()用于掩盖clang scan-build */
    if( (bestPlan.plan.wsFlags & WHERE_ORDERBY)!=0 && ALWAYS(ppOrderBy) ){
      *ppOrderBy = 0;
    }
    if( (bestPlan.plan.wsFlags & WHERE_DISTINCT)!=0 ){
      assert( pWInfo->eDistinct==0 );
      pWInfo->eDistinct = WHERE_DISTINCT_ORDERED;
    }
    andFlags &= bestPlan.plan.wsFlags;
    pLevel->plan = bestPlan.plan;
    testcase( bestPlan.plan.wsFlags & WHERE_INDEXED );
    testcase( bestPlan.plan.wsFlags & WHERE_TEMP_INDEX );
    if( bestPlan.plan.wsFlags & (WHERE_INDEXED|WHERE_TEMP_INDEX) ){
      if( (wctrlFlags & WHERE_ONETABLE_ONLY) 
       && (bestPlan.plan.wsFlags & WHERE_TEMP_INDEX)==0 
      ){
        pLevel->iIdxCur = iIdxCur;
      }else{
        pLevel->iIdxCur = pParse->nTab++;
      }
    }else{
      pLevel->iIdxCur = -1;
    }
    notReady &= ~getMask(pMaskSet, pTabList->a[bestJ].iCursor);
    pLevel->iFrom = (u8)bestJ;
    if( bestPlan.plan.nRow>=(double)1 ){
      pParse->nQueryLoop *= bestPlan.plan.nRow;
    }

    /* Check that if the table scanned by this loop iteration had an
    ** INDEXED BY clause attached to it, that the named index is being
    ** used for the scan. If not, then query compilation has failed.
    ** Return an error.
<<<<<<< HEAD
	检查，如果通过此循环迭代扫描的表已索引BY子句连接到它，
	已命名的索引被用于扫描。如果没有，那么在查询编译失败。
?返回一个错误。
=======
    **
    ** 表通过这个循环迭代扫描时，检查它是否有一个INDEXED BY子句，如果有，那么就使用这个指定的索引来用于扫描。
    ** 如果没有，那么查询编辑将失败。返回一个错误。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    pIdx = pTabList->a[bestJ].pIndex;
    if( pIdx ){
      if( (bestPlan.plan.wsFlags & WHERE_INDEXED)==0 ){
        sqlite3ErrorMsg(pParse, "cannot use index: %s", pIdx->zName);
        goto whereBeginError;
      }else{
        /* If an INDEXED BY clause is used, the bestIndex() function is
        ** guaranteed to find the index specified in the INDEXED BY clause
        ** if it find an index at all. 
<<<<<<< HEAD
		如果一个索引BY子集，该bestIndex（）函数保证找到指定的索引
		收录条款如果找到一个索引的。
		*/
=======
        **
        ** 如果使用一个INDEXED BY子句，如果bestIndex()函数能找到一个索引，
        ** 那么保证它能找到在INDEXED BY子句中的指定索引。
        */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
        assert( bestPlan.plan.u.pIdx==pIdx );
      }
    }
  }
  WHERETRACE(("*** Optimizer Finished ***\n"));
  if( pParse->nErr || db->mallocFailed ){
    goto whereBeginError;
  }

  /* If the total query only selects a single row, then the ORDER BY
  ** clause is irrelevant.
<<<<<<< HEAD
  如果总查询仅选择一个行，然后在ORDER BY
子集是不相关的。
=======
  **
  ** 如果查询只是选择一行，那么ORDER BY子句就是无关痛痒的。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  if( (andFlags & WHERE_UNIQUE)!=0 && ppOrderBy ){
    *ppOrderBy = 0;
  }

  /* If the caller is an UPDATE or DELETE statement that is requesting
  ** to use a one-pass algorithm, determine if this is appropriate.
  ** The one-pass algorithm only works if the WHERE clause constraints
  ** the statement to update a single row.
<<<<<<< HEAD
  如果调用者是一个UPDATE或DELETE语句请求，使用一个通算法，
  确定这是否是适量。一个合格的算法只能如果WHERE子句限制的语句更新一行。
=======
  **
  ** 如果调用方式一个UPDATE或DELETE命令请求一个一次通过的算法，确定这是合适的。
  ** 一次通过算法值在WHERE子子句约束命令去更新一行时才起作用。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  assert( (wctrlFlags & WHERE_ONEPASS_DESIRED)==0 || pWInfo->nLevel==1 );
  if( (wctrlFlags & WHERE_ONEPASS_DESIRED)!=0 && (andFlags & WHERE_UNIQUE)!=0 ){
    pWInfo->okOnePass = 1;
    pWInfo->a[0].plan.wsFlags &= ~WHERE_IDX_ONLY;
  }

  /* Open all tables in the pTabList and any indices selected for
  ** searching those tables.
<<<<<<< HEAD
  打开所选搜索这些表中的pTabList所有表和索引的任何
=======
  **
  ** 在pTabList打开所有的表和用于搜索这些表而挑选出的所有索引。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  sqlite3CodeVerifySchema(pParse, -1); /* Insert the cookie verifier Goto 验证者Goto插入cookie */
  notReady = ~(Bitmask)0;
  pWInfo->nRowOut = (double)1;
  for(i=0, pLevel=pWInfo->a; i<nTabList; i++, pLevel++){
<<<<<<< HEAD
    Table *pTab;     /* Table to open 打开表*/
    int iDb;         /* Index of database containing table/index  数据库包含的表/索引的索引*/
=======
    Table *pTab;     /* Table to open 需要打开的表 */
    int iDb;         /* Index of database containing table/index 数据库索引包含表/索引 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949

    pTabItem = &pTabList->a[pLevel->iFrom];
    pTab = pTabItem->pTab;
    pLevel->iTabCur = pTabItem->iCursor;
    pWInfo->nRowOut *= pLevel->plan.nRow;
    iDb = sqlite3SchemaToIndex(db, pTab->pSchema);
    if( (pTab->tabFlags & TF_Ephemeral)!=0 || pTab->pSelect ){
      /* Do nothing 什么都不做 */
    }else
#ifndef SQLITE_OMIT_VIRTUALTABLE
    if( (pLevel->plan.wsFlags & WHERE_VIRTUALTABLE)!=0 ){
      const char *pVTab = (const char *)sqlite3GetVTable(db, pTab);
      int iCur = pTabItem->iCursor;
      sqlite3VdbeAddOp4(v, OP_VOpen, iCur, 0, 0, pVTab, P4_VTAB);
    }else
#endif
    if( (pLevel->plan.wsFlags & WHERE_IDX_ONLY)==0
         && (wctrlFlags & WHERE_OMIT_OPEN_CLOSE)==0 ){
      int op = pWInfo->okOnePass ? OP_OpenWrite : OP_OpenRead;
      sqlite3OpenTable(pParse, pTabItem->iCursor, iDb, pTab, op);
      testcase( pTab->nCol==BMS-1 );
      testcase( pTab->nCol==BMS );
      if( !pWInfo->okOnePass && pTab->nCol<BMS ){
        Bitmask b = pTabItem->colUsed;
        int n = 0;
        for(; b; b=b>>1, n++){}
        sqlite3VdbeChangeP4(v, sqlite3VdbeCurrentAddr(v)-1, 
                            SQLITE_INT_TO_PTR(n), P4_INT32);
        assert( n<=pTab->nCol );
      }
    }else{
      sqlite3TableLock(pParse, iDb, pTab->tnum, 0, pTab->zName);
    }
#ifndef SQLITE_OMIT_AUTOMATIC_INDEX
    if( (pLevel->plan.wsFlags & WHERE_TEMP_INDEX)!=0 ){
      constructAutomaticIndex(pParse, pWC, pTabItem, notReady, pLevel);
    }else
#endif
    if( (pLevel->plan.wsFlags & WHERE_INDEXED)!=0 ){
      Index *pIx = pLevel->plan.u.pIdx;
      KeyInfo *pKey = sqlite3IndexKeyinfo(pParse, pIx);
      int iIndexCur = pLevel->iIdxCur;
      assert( pIx->pSchema==pTab->pSchema );
      assert( iIndexCur>=0 );
      sqlite3VdbeAddOp4(v, OP_OpenRead, iIndexCur, pIx->tnum, iDb,
                        (char*)pKey, P4_KEYINFO_HANDOFF);
      VdbeComment((v, "%s", pIx->zName));
    }
    sqlite3CodeVerifySchema(pParse, iDb);
    notReady &= ~getMask(pWC->pMaskSet, pTabItem->iCursor);
  }
  pWInfo->iTop = sqlite3VdbeCurrentAddr(v);
  if( db->mallocFailed ) goto whereBeginError;

  /* Generate the code to do the search.  Each iteration of the for
  ** loop below generates code for a single nested loop of the VM
  ** program.
  **
  ** 生成代码来进行搜索。下面的for循环每次迭代为VM程序的一个嵌套循环生成代码。
  */
  notReady = ~(Bitmask)0;
  for(i=0; i<nTabList; i++){
    pLevel = &pWInfo->a[i];
    explainOneScan(pParse, pTabList, pLevel, i, pLevel->iFrom, wctrlFlags);
    notReady = codeOneLoopStart(pWInfo, i, wctrlFlags, notReady);
    pWInfo->iContinue = pLevel->addrCont;
  }

#ifdef SQLITE_TEST  /* For testing and debugging use only 只用于测试和调试 */
  /* Record in the query plan information about the current table
  ** and the index used to access it (if any).  If the table itself
  ** is not used, its name is just '{}'.  If no index is used
  ** the index is listed as "{}".  If the primary key is used the
  ** index name is '*'.
<<<<<<< HEAD
  对于测试和调试只用记录在有关当前表的查询计划信息
和所用的索引来访问它（如果有的话）。如果表本身不使用时，
它的名字只是“{}”。如果没有使用索引的索引被列为“{}”。
如果主键使用的索引的名字是'*'。
=======
  **
  ** 记录在查询计划中有关当前表和索引访问的信息。如果未使用表本身，那么它的名字只是'{}'。
  ** 如果没有索引被使用，索引被列为"{}".如果使用了主键，那么索引名是'*'
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  for(i=0; i<nTabList; i++){
    char *z;
    int n;
    pLevel = &pWInfo->a[i];
    pTabItem = &pTabList->a[pLevel->iFrom];
    z = pTabItem->zAlias;
    if( z==0 ) z = pTabItem->pTab->zName;
    n = sqlite3Strlen30(z);
    if( n+nQPlan < sizeof(sqlite3_query_plan)-10 ){
      if( pLevel->plan.wsFlags & WHERE_IDX_ONLY ){
        memcpy(&sqlite3_query_plan[nQPlan], "{}", 2);
        nQPlan += 2;
      }else{
        memcpy(&sqlite3_query_plan[nQPlan], z, n);
        nQPlan += n;
      }
      sqlite3_query_plan[nQPlan++] = ' ';
    }
    testcase( pLevel->plan.wsFlags & WHERE_ROWID_EQ );
    testcase( pLevel->plan.wsFlags & WHERE_ROWID_RANGE );
    if( pLevel->plan.wsFlags & (WHERE_ROWID_EQ|WHERE_ROWID_RANGE) ){
      memcpy(&sqlite3_query_plan[nQPlan], "* ", 2);
      nQPlan += 2;
    }else if( (pLevel->plan.wsFlags & WHERE_INDEXED)!=0 ){
      n = sqlite3Strlen30(pLevel->plan.u.pIdx->zName);
      if( n+nQPlan < sizeof(sqlite3_query_plan)-2 ){
        memcpy(&sqlite3_query_plan[nQPlan], pLevel->plan.u.pIdx->zName, n);
        nQPlan += n;
        sqlite3_query_plan[nQPlan++] = ' ';
      }
    }else{
      memcpy(&sqlite3_query_plan[nQPlan], "{} ", 3);
      nQPlan += 3;
    }
  }
  while( nQPlan>0 && sqlite3_query_plan[nQPlan-1]==' ' ){
    sqlite3_query_plan[--nQPlan] = 0;
  }
  sqlite3_query_plan[nQPlan] = 0;
  nQPlan = 0;
#endif /* SQLITE_TEST // Testing and debugging use only 只用于测试和调试 */

  /* Record the continuation address in the WhereInfo structure.  Then
  ** clean up and return.
<<<<<<< HEAD
  记录在WhereInfo结构延续的地址。然后清理，并返回。
  */
  return pWInfo;

  /* Jump here if malloc fails  如果malloc失败，跳转到这里*/
=======
  **
  ** 记录在WhereInfo数据结构中的连续地址。然后清除并返回。
  */
  return pWInfo;

  /* Jump here if malloc fails 如果分配内存失败就跳出 */
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
whereBeginError:
  if( pWInfo ){
    pParse->nQueryLoop = pWInfo->savedNQueryLoop;
    whereInfoFree(db, pWInfo);
  }
  return 0;
}

/*
** Generate the end of the WHERE loop.  See comments on 
** sqlite3WhereBegin() for additional information.
<<<<<<< HEAD
生成在WHERE循环的结束。查看sqlite3WhereBegin评论（）了解更多信息。
=======
**
** 生成WHERE循环的结束代码。查看在sqlite3WhereBegin()上的附加信息的评论。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
*/
void sqlite3WhereEnd(WhereInfo *pWInfo){
  Parse *pParse = pWInfo->pParse;
  Vdbe *v = pParse->pVdbe;
  int i;
  WhereLevel *pLevel;
  SrcList *pTabList = pWInfo->pTabList;
  sqlite3 *db = pParse->db;

  /* Generate loop termination code.
<<<<<<< HEAD
  产生循环终止代码
=======
  ** 生成循环终止代码
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  sqlite3ExprCacheClear(pParse);
  for(i=pWInfo->nLevel-1; i>=0; i--){
    pLevel = &pWInfo->a[i];
    sqlite3VdbeResolveLabel(v, pLevel->addrCont);
    if( pLevel->op!=OP_Noop ){
      sqlite3VdbeAddOp2(v, pLevel->op, pLevel->p1, pLevel->p2);
      sqlite3VdbeChangeP5(v, pLevel->p5);
    }
    if( pLevel->plan.wsFlags & WHERE_IN_ABLE && pLevel->u.in.nIn>0 ){
      struct InLoop *pIn;
      int j;
      sqlite3VdbeResolveLabel(v, pLevel->addrNxt);
      for(j=pLevel->u.in.nIn, pIn=&pLevel->u.in.aInLoop[j-1]; j>0; j--, pIn--){
        sqlite3VdbeJumpHere(v, pIn->addrInTop+1);
        sqlite3VdbeAddOp2(v, OP_Next, pIn->iCur, pIn->addrInTop);
        sqlite3VdbeJumpHere(v, pIn->addrInTop-1);
      }
      sqlite3DbFree(db, pLevel->u.in.aInLoop);
    }
    sqlite3VdbeResolveLabel(v, pLevel->addrBrk);
    if( pLevel->iLeftJoin ){
      int addr;
      addr = sqlite3VdbeAddOp1(v, OP_IfPos, pLevel->iLeftJoin);
      assert( (pLevel->plan.wsFlags & WHERE_IDX_ONLY)==0
           || (pLevel->plan.wsFlags & WHERE_INDEXED)!=0 );
      if( (pLevel->plan.wsFlags & WHERE_IDX_ONLY)==0 ){
        sqlite3VdbeAddOp1(v, OP_NullRow, pTabList->a[i].iCursor);
      }
      if( pLevel->iIdxCur>=0 ){
        sqlite3VdbeAddOp1(v, OP_NullRow, pLevel->iIdxCur);
      }
      if( pLevel->op==OP_Return ){
        sqlite3VdbeAddOp2(v, OP_Gosub, pLevel->p1, pLevel->addrFirst);
      }else{
        sqlite3VdbeAddOp2(v, OP_Goto, 0, pLevel->addrFirst);
      }
      sqlite3VdbeJumpHere(v, addr);
    }
  }

  /* The "break" point is here, just past the end of the outer loop.
  ** Set it.
<<<<<<< HEAD
  在“break”的一点是在这里，刚刚过去的外循环的结束。
设置它。
=======
  ** "break"指针。刚刚结束外循环。设置它。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  sqlite3VdbeResolveLabel(v, pWInfo->iBreak);

  /* Close all of the cursors that were opened by sqlite3WhereBegin.
<<<<<<< HEAD
  关闭所有sqlite3WhereBegin被打开的游标。
=======
  ** 关闭所有由sqlite3WhereBegin打开的游标
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  assert( pWInfo->nLevel==1 || pWInfo->nLevel==pTabList->nSrc );
  for(i=0, pLevel=pWInfo->a; i<pWInfo->nLevel; i++, pLevel++){
    Index *pIdx = 0;
    struct SrcList_item *pTabItem = &pTabList->a[pLevel->iFrom];
    Table *pTab = pTabItem->pTab;
    assert( pTab!=0 );
    if( (pTab->tabFlags & TF_Ephemeral)==0
     && pTab->pSelect==0
     && (pWInfo->wctrlFlags & WHERE_OMIT_OPEN_CLOSE)==0
    ){
      int ws = pLevel->plan.wsFlags;
      if( !pWInfo->okOnePass && (ws & WHERE_IDX_ONLY)==0 ){
        sqlite3VdbeAddOp1(v, OP_Close, pTabItem->iCursor);
      }
      if( (ws & WHERE_INDEXED)!=0 && (ws & WHERE_TEMP_INDEX)==0 ){
        sqlite3VdbeAddOp1(v, OP_Close, pLevel->iIdxCur);
      }
    }

    /* If this scan uses an index, make code substitutions to read data
    ** from the index in preference to the table. Sometimes, this means
    ** the table need never be read from. This is a performance boost,
    ** as the vdbe level waits until the table is read before actually
    ** seeking the table cursor to the record corresponding to the current
    ** position in the index.
<<<<<<< HEAD
    ** 如果该扫描使用索引，使代码替换以从表中的索引优先读取数据。
	有时，这意味着该表不需要永远不会被从读出。这是一个性能提升，
	因为VDBE水平等待直到表实际进行求表，光标对应于索引的当前位置记录之前读出。
=======
    ** 
    ** 如果这个扫描使用了一个索引，编写代码从表中读取数据取代优先从索引中读取数据。
    ** 有时，这意味着表用于不需要被读取。
    ** 这是一个性能推进，在寻找对应的表的游标来记录当前索引的位置之前，vdbe水平一直等待读取表。
    **
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    ** Calls to the code generator in between sqlite3WhereBegin and
    ** sqlite3WhereEnd will have created code that references the table
    ** directly.  This loop scans all that code looking for opcodes
    ** that reference the table and converts them into opcodes that
    ** reference the index.
<<<<<<< HEAD
	调用代码生成器在sqlite3WhereBegin和sqlite3WhereEnd之间将有直接引用的表创建的代码。
	这个循环扫描所有的代码寻找操作码引用表，将它们转换成引用索引操作码。
=======
    **
    ** 在sqlite3WhereBegin和sqlite3WhereEnd之间调用代码生成器直接地创建与表相关的代码。
    ** 这个循环扫描所有这些代码来寻找与表相关的操作码并将它们转化为与索引相关的操作码。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
    */
    if( pLevel->plan.wsFlags & WHERE_INDEXED ){
      pIdx = pLevel->plan.u.pIdx;
    }else if( pLevel->plan.wsFlags & WHERE_MULTI_OR ){
      pIdx = pLevel->u.pCovidx;
    }
    if( pIdx && !db->mallocFailed){
      int k, j, last;
      VdbeOp *pOp;

      pOp = sqlite3VdbeGetOp(v, pWInfo->iTop);
      last = sqlite3VdbeCurrentAddr(v);
      for(k=pWInfo->iTop; k<last; k++, pOp++){
        if( pOp->p1!=pLevel->iTabCur ) continue;
        if( pOp->opcode==OP_Column ){
          for(j=0; j<pIdx->nColumn; j++){
            if( pOp->p2==pIdx->aiColumn[j] ){
              pOp->p2 = j;
              pOp->p1 = pLevel->iIdxCur;
              break;
            }
          }
          assert( (pLevel->plan.wsFlags & WHERE_IDX_ONLY)==0
               || j<pIdx->nColumn );
        }else if( pOp->opcode==OP_Rowid ){
          pOp->p1 = pLevel->iIdxCur;
          pOp->opcode = OP_IdxRowid;
        }
      }
    }
  }

<<<<<<< HEAD
  /* Final cleanup 最后清理
=======
  /* Final cleanup 最后清除数据
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  pParse->nQueryLoop = pWInfo->savedNQueryLoop;
  whereInfoFree(db, pWInfo);
  return;
}
