/*********************************
student1555修改，因冲突，另名提交，最后整合。
**********************************/
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
**************************************************************************/
/*
** This module contains C code that generates VDBE code used to process
** the WHERE clause of SQL statements.  This module is responsible for
** generating the code that loops through a table looking for applicable
** rows.  Indices are selected and used to speed the search when doing
** so is applicable.  Because this module is responsible for selecting
** indices, you might also think of this module as the "query optimizer".
**
** 这个模块包含C语言代码，该代码生成用来执行SQL语句的WHERE子句的VDBE编码。
** 这个模块负责生成代码，来依次扫描一个表来查找合适的行。当索引可用时，
** 选择并使用索引来加快查询。因为这个模块负责索引选择，你也可以把这个模块当做"查询优化"。
*/
#include "sqliteInt.h"

/*
** Trace output macros
** 跟踪输出宏,用于测试和调试
*/
#if defined(SQLITE_TEST) || defined(SQLITE_DEBUG)
int sqlite3WhereTrace = 0;
#endif
#if defined(SQLITE_TEST) && defined(SQLITE_DEBUG)
# define WHERETRACE(X)  if(sqlite3WhereTrace) sqlite3DebugPrintf X
#else
# define WHERETRACE(X)
#endif

/* Forward reference
** 前置引用 
** 具体结构体在下面
*/
typedef struct WhereClause WhereClause;
typedef struct WhereMaskSet WhereMaskSet;
typedef struct WhereOrInfo WhereOrInfo;
typedef struct WhereAndInfo WhereAndInfo;
typedef struct WhereCost WhereCost;

/*  ****************** WhereTerm定义说明 *********************
**
** The query generator uses an array of instances of this structure to
** help it analyze the subexpressions of the WHERE clause.  Each WHERE
** clause subexpression is separated from the others by AND operators,
** usually, or sometimes subexpressions separated by OR.
**
** 查询生成器使用一组这个数据结构的实例来帮助它分析WHERE子句的子表达式。
** 每个WHERE子句子表达式通常是根据AND运算符分隔的，有时也会根据OR分隔。
**
** All WhereTerms are collected into a single WhereClause structure.  
** The following identity holds:
** 
** 所有WhereTerms汇总到一个单一的WhereClause结构体中.  
** 使用以下方式保存: 
**
**        WhereTerm.pWC->a[WhereTerm.idx] == WhereTerm
**
** 如果term如下形式:
**
**              X <op> <expr>
**
** where X is a column name and <op> is one of certain operators,
** then WhereTerm.leftCursor and WhereTerm.u.leftColumn record the
** cursor number and column number for X.  WhereTerm.eOperator records
** the <op> using a bitmask encoding defined by WO_xxx below.  The
** use of a bitmask encoding for the operator allows us to search
** quickly for terms that match any of several different operators.
**
** X是一个列名，<op>是某个运算符，WhereTerm.leftCursorand和WhereTerm.u.leftColumn
** 记录X的游标数和列数。WhereTerm.eOperator使用由下面的WO_xxx定义的位掩码记录<op>。
** 运算符使用位掩码使得我们能快速查找能够匹配任意不同运算符的terms。
**
** A WhereTerm might also be two or more subterms connected by OR:
**
**         (t1.X <op> <expr>) OR (t1.Y <op> <expr>) OR ....
**
** In this second case, wtFlag as the TERM_ORINFO set and eOperator==WO_OR
** and the WhereTerm.u.pOrInfo field points to auxiliary information that
** is collected about the OR clause.
**
** 一个WhereTerm结构也可以是由OR连接的2个或多个subterms
**         (t1.X <op> <expr>) OR (t1.Y <op> <expr>) OR ....
** 在这种情况下，wtFlag设为TERM_ORINFO，eOperator==WO_OR并且WhereTerm.u.pOrInfo指向
** 收集的关于OR子句的辅助信息。 
**
** If a term in the WHERE clause does not match either of the two previous
** categories, then eOperator==0.  The WhereTerm.pExpr field is still set
** to the original subexpression content and wtFlags is set up appropriately
** but no other fields in the WhereTerm object are meaningful.
**
** 如果在WHERE子句中的一个term与前面两类都不匹配，那么eOperator==0。WhereTerm.pExpr仍旧设为
** 原始子表达式的内容，并且wtFlags设置为适当的值，但是,whereTerm对象中的其他字段都是无意义的。
**
** When eOperator!=0, prereqRight and prereqAll record sets of cursor numbers,
** but they do so indirectly. A single WhereMaskSet structure translates  
** cursor number into bits and the translated bit is stored in the prereq fields. 
** The translation is used in order to maximize the number of bits that will
** fit in a Bitmask. The VDBE cursor numbers might be spread out over the non-negative 
** integers. For example, the cursor numbers might be 3, 8, 9, 10, 20, 23, 41, and 45.    
** The WhereMaskSet translates these sparse cursor numbers into consecutive integers
** beginning with 0 in order to make the best possible use of the available bits in the Bitmask.  
** So, in the example above, the cursor numbers would be mapped into integers 0 through 7.
** 
** 当eOperator != 0,prereqRight和prereqAll间接地记录游标数集。一个单独的WhereMaskSet结构
** 把游标数转化为bits并且转化后的bit存储在prereq字段中。使用转化是为了最大化bits使之适应
** 一个位掩码。VDBE游标数分散为非负整数。例如，游标数可能是3,8,9,10,20,23,41,45。
** WhereMaskSet把这些分散的游标数转换为从0开始的连续整数，是为了尽最大可能利用在位掩码中的可用位。
** 所以，在上面的例子中，游标数被映射为从0-7的整数。
**
** The number of terms in a join is limited by the number of bits
** in prereqRight and prereqAll.  The default is 64 bits, hence SQLite
** is only able to process joins with 64 or fewer tables.
** 
** 一个连接中的terms数目受prereqRight and prereqAll中的bits数限制。
** 默认是64位，因此SQLite只能处理64个或更少个表的连接。 
*/

/* WhereTerm 结构体用来存储 where 子句根据 <op>(AND、OR)分隔后的各个子句 */
/*Expr 结构体，表示 语法分析树中的一个表达式的每个节点是该结构的一个实例。 出自sqliteInit.h 1829行*/
typedef struct WhereTerm WhereTerm;
struct WhereTerm {
  Expr *pExpr;            /* Pointer to the subexpression(子表达式) that is this term 指向这个子表达式的指针 */
  int iParent;            /* Disable pWC->a[iParent] when this term disabled 当这个term销毁时禁用pWC->a[iParent] */
  int leftCursor;         /* Cursor number of X in "X <op> <expr>"  在"X <op> <expr>"中的X的游标数 */
  union {
    int leftColumn;         /* Column number of X in "X <op> <expr>"  在"X <op> <expr>"中的X的列数 */
    WhereOrInfo *pOrInfo;   /* Extra information if eOperator==WO_OR 如果eOperator==WO_OR时的额外信息 */
    WhereAndInfo *pAndInfo; /* Extra information if eOperator==WO_AND 如果eOperator==WO_AND时的额外信息 */
  } u;
  u16 eOperator;          /* A WO_xx value describing <op>  描述<op>的一个 WO_xx */
  u8 wtFlags;             /* TERM_xxx bit flags.  See below   TERM_xxx bit标志，下面的TERM_xxx定义了具体值 */
  u8 nChild;              /* Number of children that must disable us 必须禁用的孩子数 */
  WhereClause *pWC;       /* The clause this term is part of  这个term是哪个子句的一部分  */
  Bitmask prereqRight;    /* Bitmask of tables used by pExpr->pRight  pExpr->pRight使用的表的位掩码 */
  Bitmask prereqAll;      /* Bitmask of tables referenced by pExpr  由pExpr引用的表的位掩码 */
};

/*
** Allowed values of WhereTerm.wtFlags
** wtFlags的可用值
*/
#define TERM_DYNAMIC    0x01   /* Need to call sqlite3ExprDelete(db, pExpr)  需要调用sqlite3ExprDelete(db, pExpr) */
#define TERM_VIRTUAL    0x02   /* Added by the optimizer.  Do not code  由优化器添加，不需要编码*/
#define TERM_CODED      0x04   /* This term(项) is already coded  这个term已经被编码 */
#define TERM_COPIED     0x08   /* Has a child  有一个子term */
#define TERM_ORINFO     0x10   /* Need to free the WhereTerm.u.pOrInfo object  需要释放WhereTerm.u.pOrInfo对象 */
#define TERM_ANDINFO    0x20   /* Need to free the WhereTerm.u.pAndInfo obj  需要释放WhereTerm.u.pAndInfo对象 */
#define TERM_OR_OK      0x40   /* Used during OR-clause processing  当OR子句执行时使用 */

#ifdef SQLITE_ENABLE_STAT3
#  define TERM_VNULL    0x80   /* Manufactured x>NULL or x<=NULL term  生成 x>NULL or x<=NULL 的term */
#else
#  define TERM_VNULL    0x00   /* Disabled if not using stat3  如果不使用stat3则禁用 */
#endif

/* ****************** WhereClause定义说明 *********************
** Explanation of pOuter:  For a WHERE clause of the form
**
**           a AND ((b AND c) OR (d AND e)) AND f
**
** There are separate WhereClause objects for the whole clause and for
** the subclauses "(b AND c)" and "(d AND e)". The pOuter field of the 
** subclauses points to the WhereClause object for the whole clause.
**
** 解释一下pOuter: 对于如下形式的WHERE子句:
** 			a AND ((b AND c) OR (d AND e)) AND f
** 
** 对于整个where子句和分子句 "(b AND c)" and "(d AND e)"，存在分开的WhereClause对象。
** 子句的pOuter字段指向整个where子句的WhereClause对象
*/

/*WhereClause结构体用于存储sql语句中的整个的where子句，可能包含一个或多个WhereTerm。*/
struct WhereClause {
  Parse *pParse;           /* The parser context  解析器上下文 */
  WhereMaskSet *pMaskSet;  /* Mapping of table cursor numbers to bitmasks  表的游标数和位掩码之间的映射 */
  Bitmask vmask;           /* Bitmask identifying(识别) virtual table cursors  识别虚表游标的位掩码 */
  WhereClause *pOuter;     /* Outer conjunction  外连接 */
  u8 op;                   /* Split operator. TK_AND or TK_OR  分隔符如TK_AND or TK_OR  */
  u16 wctrlFlags;          /* Might include WHERE_AND_ONLY  可能包含WHERE_AND_ONLY */
  int nTerm;               /* Number of terms  terms的数目 */
  int nSlot;               /* Number of entries(条目数) in a[](153行)  在一个WhereTerm中的记录数  */
  WhereTerm *a;            /* Each a[] describes(描述) a term of the WHERE cluase  每个a[]描述WHERE子句中的一个term */
  
#if defined(SQLITE_SMALL_STACK)
  WhereTerm aStatic[1];    /* Initial static space for a[] 初始化a[]静态空间 */
#else
  WhereTerm aStatic[8];    /* Initial static space for a[] 初始化a[]静态空间 */
#endif
};

/* ****************** WhereOrInfo 定义说明 *********************
** A WhereTerm with eOperator==WO_OR has its u.pOrInfo pointer set to
** a dynamically allocated instance of the following structure.
**
** WhereTerm的eOperator字段为WO_OR，则该WhereTerm的u.pOrInfo指针设置为
** 下列结构体的一个动态分配的实例。
*/
struct WhereOrInfo {
  WhereClause wc;          /* Decomposition(分解) into subterms  分解为子terms */
  Bitmask indexable;       /* Bitmask of all indexable tables in the clause  where子句中的所有可加索引的表的位掩码 */
};

/* ****************** WhereAndInfo 定义说明 *********************
** A WhereTerm with eOperator==WO_AND has its u.pAndInfo pointer set to
** a dynamically allocated instance of the following structure.
**
** 若WhereTerm的eOperator字段为WO_AND，则该WhereTerm的u.pAndInfo指针设置为
** 下列结构体的一个动态分配的实例。
*/
struct WhereAndInfo {
  WhereClause wc;          /* The subexpression broken out 分解的子表达式*/
};

/* ****************** WhereMaskSet 定义说明 *********************
**
** An instance of the following structure keeps track of a mapping
** between VDBE cursor numbers and bits of the bitmasks in WhereTerm.
**
** 下列数据结构的实例记录VDBE游标数与WhereTerm位掩码bits之间的映射
**
** SrcList_item.iCursor and Expr.iTable fields.  For any given WHERE 
** clause, the cursor numbers might not begin with 0 and they might
** contain gaps in the numbering sequence.  But we want to make maximum
** use of the bits in our bitmasks.  This structure provides a mapping
** from the sparse cursor numbers into consecutive integers beginning
** with 0.
**
** 包含在SrcList_item.iCursor 和 Expr.iTable字段的VDBE游标数是small整型。
** 对于任何给定的WHERE子句，游标数可能不是从0开始的，而且可能在编号数列中有空白。
** 但我们希望最大化的使用位掩码中的bits。这个数据结构提供了一种由分散的游标数到
** 从0开始的连续整数之间的映射。  
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
** 例如，如果WHERE子句表达式使用这些VDBE游标: 4, 5, 8, 29, 57, 73.
** 那么WhereMaskSet结构会把这些游标数映射为0到5。
**
** Note that the mapping is not necessarily ordered.  In the example
** above, the mapping might go like this:  4->3, 5->1, 8->2, 29->0,
** 57->5, 73->4.  Or one of 719 other combinations(组合) might be used. It
** does not really matter.  What is important is that sparse cursor
** numbers all get mapped into bit numbers that begin with 0 and contain
** no gaps.
**
** 注意:映射不一定有序。在上面的例子中映射可能是这样的:4->3, 5->1, 8->2, 29->0, 57->5, 73->4.
** 或着是其他719种组合中的一种。实际上，这并不重要。
** 重要的是分散的游标数能全部映射到从0开始的bit数，而且不能包含空白.
*/
struct WhereMaskSet {
  int n;                        /* Number of assigned(已分配的) cursor values  已分配游标值的数目 */
  int ix[BMS];                  /* Cursor assigned to each bit  游标被分配给每一个bit */
};

/*
** A WhereCost object records a lookup strategy and the estimated
** cost of pursuing that strategy.
** WhereCost对象记录一个查询策略以及执行该策略的预估成本。
*/
struct WhereCost {
  WherePlan plan;    /* The lookup strategy  查询策略 */
  double rCost;      /* Overall cost(总成本) of pursuing this search strategy  执行该查询策略的总成本 */
  Bitmask used;      /* Bitmask of cursors used by this plan  这个策略使用的游标的位掩码 */
};

/*
** Bitmasks for the operators that indices(index复数，目录) are able to exploit(开发).  
** An OR-ed combination of(...的组合) these values can be used when searching for
** terms in the where clause.
**
** 运算符的位掩码，索引可以进行开发。这些值的OR-ed组合可以在查找where子句的terms时被使用.
*/
#define WO_IN     0x001	//16进制0000 0000 0001
#define WO_EQ     0x002	//16进制0000 0000 0010
#define WO_LT     (WO_EQ<<(TK_LT-TK_EQ))	//(WO_EQ 左移 TK_LT-TK_EQ)
#define WO_LE     (WO_EQ<<(TK_LE-TK_EQ))	//(WO_EQ 左移 TK_LE-TK_EQ)
#define WO_GT     (WO_EQ<<(TK_GT-TK_EQ))	//(WO_EQ 左移 TK_GT-TK_EQ)
#define WO_GE     (WO_EQ<<(TK_GE-TK_EQ))	//(WO_EQ 左移 TK_GE-TK_EQ)
#define WO_MATCH  0x040	//16进制0000 0100 0000
#define WO_ISNULL 0x080	//16进制0000 1000 0000
#define WO_OR     0x100 //16进制0001 0000 0000  /* Two or more OR-connected terms  两个或更多个OR-connected terms */
#define WO_AND    0x200 //16进制0010 0000 0000  /* Two or more AND-connected terms  两个或更多个AND-connected terms */
#define WO_NOOP   0x800 //16进制1000 0000 0000  /* This term does not restrict(限制) search space 这个term不限制搜索空间 */
#define WO_ALL    0xfff //16进制1111 1111 1111  /* Mask of all possible WO_* values  所有可能的WO_*值的掩码 */
#define WO_SINGLE 0x0ff //16进制0000 1111 1111  /* Mask of all non-compound(不混合的) WO_* values  所有不混合的WO_*值的掩码 */

/*
** Value for wsFlags returned by bestIndex() and stored in WhereLevel.wsFlags.  
** These flags determine which search strategies are appropriate.
** 
** wsFlags的值由bestIndex()返回，并且存储在WhereLevel.wsFlags中。
** 这些wsFlags值决定哪些查询策略是合适的。
**
** The least significant 12 bits is reserved as a mask for WO_ values above.
** The WhereLevel.wsFlags field is usually set to WO_IN|WO_EQ|WO_ISNULL.
** But if the table is the right table of a left join, WhereLevel.wsFlags
** is set to WO_IN|WO_EQ. 
**
** 低位的12位保留上文中的WO_ 值的掩码。
** WhereLevel.wsFlags字段通常设置为WO_IN|WO_EQ|WO_ISNULL.
** 但是如果这个表示左联接的右表，WhereLevel.wsFlags设置为WO_IN|WO_EQ.
**
** The WhereLevel.wsFlags field can then be used as the "op" parameter
**  to findTerm when we are resolving equality constraints.
** ISNULL constraints will then not be used on the right table of a left join.
**
** 当我们分析等式约束时，WhereLevel.wsFlags字段当做findTerm的"op"参数使用
** 在左联接的右表上不会使用ISNULL约束。
*/
#define WHERE_ROWID_EQ     0x00001000  /* rowid=EXPR or rowid IN (...)  rowid=EXPR或rowid IN(...) */
#define WHERE_ROWID_RANGE  0x00002000  /* rowid<EXPR and/or rowid>EXPR  rowid<EXPR 且/或 rowid>EXPR */
#define WHERE_COLUMN_EQ    0x00010000  /* x=EXPR or x IN (...) or x IS NULL  x=EXPR 或 x IN (...) 或 x IS NULL  */
#define WHERE_COLUMN_RANGE 0x00020000  /* x<EXPR and/or x>EXPR */
#define WHERE_COLUMN_IN    0x00040000  /* x IN (...) */
#define WHERE_COLUMN_NULL  0x00080000  /* x IS NULL */
#define WHERE_INDEXED      0x000f0000  /* Anything that uses an index  任何使用索引 */
#define WHERE_NOT_FULLSCAN 0x100f3000  /* Does not do a full table scan  不需要全表扫描 */
#define WHERE_IN_ABLE      0x000f1000  /* Able to support an IN operator  支持IN操作 */
#define WHERE_TOP_LIMIT    0x00100000  /* x<EXPR or x<=EXPR constraint  x<EXPR or x<=EXPR约束 */
#define WHERE_BTM_LIMIT    0x00200000  /* x>EXPR or x>=EXPR constraint  x>EXPR or x>=EXPR约束 */
#define WHERE_BOTH_LIMIT   0x00300000  /* Both x>EXPR and x<EXPR */
#define WHERE_IDX_ONLY     0x00800000  /* Use index only - omit table  只用索引，省略表 */
#define WHERE_ORDERBY      0x01000000  /* Output will appear in correct order  以恰当的顺序输出 */
#define WHERE_REVERSE      0x02000000  /* Scan in reverse order  倒序扫描 */
#define WHERE_UNIQUE       0x04000000  /* Selects no more than(只是，仅仅) one row  只选择一行 */
#define WHERE_VIRTUALTABLE 0x08000000  /* Use virtual-table processing(处理)  使用虚拟表处理 */
#define WHERE_MULTI_OR     0x10000000  /* OR using multiple indices  OR使用多重索引 */
#define WHERE_TEMP_INDEX   0x20000000  /* Uses an ephemeral index  使用临时索引 */
#define WHERE_DISTINCT     0x40000000  /* Correct order for DISTINCT  DISTINCT的正确顺序 */

/*
** Initialize a preallocated WhereClause structure.
** 初始化一个预分配的WhereClause数据结构
*/
static void whereClauseInit(
  WhereClause *pWC,        /* The WhereClause to be initialized  将被初始化Where子句 */
  Parse *pParse,           /* The parsing context  解析器上下文 */
  WhereMaskSet *pMaskSet,  /* Mapping from table cursor numbers to bitmasks  表的游标数到位掩码的映射 */
  u16 wctrlFlags           /* Might include WHERE_AND_ONLY  可能包含WHERE_AND_ONLY */
){
  pWC->pParse = pParse;	 /*初始化解析器上下文*/
  pWC->pMaskSet = pMaskSet;	 /*初始化pMaskSet*/
  pWC->pOuter = 0;	 /*外连接为空*/
  pWC->nTerm = 0;	/*where子句的项数为0*/
  pWC->nSlot = ArraySize(pWC->aStatic);	//初始化a[]静态空间的元素个数。ArraySize返回数组的元素个数。
  pWC->a = pWC->aStatic;	/*初始化where子句中的WhereTerm数据结构*/
  pWC->vmask = 0;	/*初始化虚表游标的位掩码*/
  pWC->wctrlFlags = wctrlFlags;		/*初始化wctrlFlags*/
}

/* Forward reference(前置引用) 具体函数在下面 */
static void whereClauseClear(WhereClause*);

/*
** Deallocate all memory associated with a WhereAndInfo object.
** 释放与WhereAndInfo对象相关联的所有内存
*/
static void whereAndInfoDelete(sqlite3 *db, WhereAndInfo *p){
  whereClauseClear(&p->wc);	 /*解除分配*/
  sqlite3DbFree(db, p);	 /*释放被关联到一个特定数据库连接的内存*/
}

/*
** Deallocate a WhereClause structure.  The WhereClause structure
** itself is not freed.  This routine is the inverse of whereClauseInit().
** 解除一个WhereClause数据结构的分配。WhereClause本身不被释放。这个程序是whereClauseInit()的反向执行。
*/
static void whereClauseClear(WhereClause *pWC){
  int i;
  WhereTerm *a;
  sqlite3 *db = pWC->pParse->db;	/*数据库连接的实例设为where子句所在的数据库*/
  for(i=pWC->nTerm-1, a=pWC->a; i>=0; i--, a++){	/*循环遍历where子句的每个term项*/
    if( a->wtFlags & TERM_DYNAMIC ){	/*如果当前的term是动态的*/
      sqlite3ExprDelete(db, a->pExpr);	/*调用sqlite3ExprDelete()递归删除term中的表达式树*/
    }
    if( a->wtFlags & TERM_ORINFO ){		/*如果当前的term存储的是OR子句的信息*/
      whereOrInfoDelete(db, a->u.pOrInfo);	/*解除当前term中的pOrInfo对象的所有内存分配*/
    }else if( a->wtFlags & TERM_ANDINFO ){	/*如果当前的term存储的是AND子句的信息*/
      whereAndInfoDelete(db, a->u.pAndInfo);  /*解除当前term中的pAndInfo对象的所有内存分配*/
    }
  }
  if( pWC->a!=pWC->aStatic ){	/*如果where子句的term项不是初始静态空间*/
    sqlite3DbFree(db, pWC->a);	/*释放被关联到一个特定数据库连接的内存*/
  }
}

/*
** 添加单个新 WhereTerm 对象到 WhereClause 对象 pWC中。
**
** The new WhereTerm object is constructed from Expr p and with wtFlags.
** The index in pWC->a[] of the new WhereTerm is returned on success.
** 0 is returned if the new WhereTerm could not be added due to a memory
** allocation error.  The memory allocation failure will be recorded in
** the db->mallocFailed flag so that higher-level functions can detect it.
**
** 新的WhereTerm对象根据Expr和wtFlags构建。
** 若成功，返回新WhereTerm在pWC->a[]中的索引。
** 如果由于内存分配错误，新的WhereTerm不能添加到WhereClause中，则返回0.
** 内存分配失败会被记录在db->mallocFailed标志中，以便更高级的函数检测到它。
**
** This routine will increase the size of the pWC->a[] array as necessary.
**
** 如果需要的话，程序会增加pWC->a[]数组的大小。
**
** If the wtFlags argument includes TERM_DYNAMIC, then responsibility
** for freeing the expression p is assumed by the WhereClause object pWC.
** This is true even if this routine fails to allocate a new WhereTerm.
**
** 如果wtFlags包含TERM_DYNAMIC，那么WhereClause的对象pWC会负责释放表达式p。
** 尽管这个程序没能成功分配一个新的WhereTerm，这也是正确的。
**
** WARNING:  This routine might reallocate the space used to store
** WhereTerms.  All pointers to WhereTerms should be invalidated after
** calling this routine.  Such pointers may be reinitialized by referencing
** the pWC->a[] array.
**
** 警告:这个程序可能会重新分配用于存储WhereTerms的空间。所有指向WhereTerms的指针
** 在调用这个程序后都会失效。这些指针可能会通过引用pWC->a[]被重新启用
*/
static int whereClauseInsert(WhereClause *pWC, Expr *p, u8 wtFlags){
  WhereTerm *pTerm;   /* 新建一个WhereTerm */
  int idx;
  testcase( wtFlags & TERM_VIRTUAL );  /* EV: R-00211-15100 */
  if( pWC->nTerm>=pWC->nSlot ){	 /*如果WhereClause中的term数大于或等于在一个WhereTerms中的记录数*/
    WhereTerm *pOld = pWC->a;	/*定义当前的term为pOld*/
    sqlite3 *db = pWC->pParse->db;	/*定义当前where子句的数据库连接*/
    pWC->a = sqlite3DbMallocRaw(db, sizeof(pWC->a[0])*pWC->nSlot*2 );	/*分配内存*/
    if( pWC->a==0 ){	/*如果分配内存失败*/
      if( wtFlags & TERM_DYNAMIC ){	 /*如果当前term是动态分配的*/
        sqlite3ExprDelete(db, p);	/*递归删除term中的表达式树*/
      }
      pWC->a = pOld;	/*把term设置为之前记录的term*/
      return 0;	 /*返回0*/
    }
    memcpy(pWC->a, pOld, sizeof(pWC->a[0])*pWC->nTerm); 
    if( pOld!=pWC->aStatic ){
      sqlite3DbFree(db, pOld);	/*释放可能被关联到一个特定数据库连接的内存*/
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
** 这个程序识别在WHERE子句中的子表达式，这些子表达式根据AND运算符或在op参数中定义的其他运算符分隔。
** WhereClause数据结构存储指向各子表达式的指针。
**
** For example:
**
**    WHERE  a=='hello' AND coalesce(b,11)<10 AND (c+12!=d OR c==22)
**           \________/     \_______________/     \________________/
**            slot[0]            slot[1]               slot[2]
** 
** The original WHERE clause in pExpr is unaltered.  All this routine
** does is make slot[] entries point to substructure within pExpr.
**
** 例如:
**    WHERE  a=='hello' AND coalesce(b,11)<10 AND (c+12!=d OR c==22)
**           \________/     \_______________/     \________________/
**            slot[0]            slot[1]               slot[2]
**
** 在pExpr中的原始的WHERE子句是不变的。这个程序就是让slot[]的记录指向在pExpr中的子结构
**
** In the previous sentence and in the diagram, "slot[]" refers to
** the WhereClause.a[] array.  The slot[] array grows as needed to contain
** all terms of the WHERE clause.
**
** 在前面的句子和在图表中，"slot[]"指的是WhereClause.a[]数组。slot数组根据需要扩大，
** 要包含WHERE子句的所有terms。
*/
static void whereSplit(WhereClause *pWC, Expr *pExpr, int op){
  pWC->op = (u8)op;  /*初始化WHERE子句进行分割的运算符*/
  if( pExpr==0 ) return;
  if( pExpr->op!=op ){  /*如果表达式的操作符不是指定的运算符*/
    whereClauseInsert(pWC, pExpr, 0); /*向pWC子句中插入一个WhereTerm*/
  }else{
    whereSplit(pWC, pExpr->pLeft, op);	/*递归分隔WHERE子句的左表达式*/
    whereSplit(pWC, pExpr->pRight, op);	 /*递归分隔WHERE子句的右表达式*/
  }
}

/*
** Initialize an expression mask set (a WhereMaskSet object)
**
** 初始化一个表达式掩码设置(一个WhereMaskSet对象)
*/
#define initMaskSet(P)  memset(P, 0, sizeof(*P))

/*
** Return the bitmask for the given cursor number.  Return 0 if
** iCursor is not in the set.
**
** 返回给出的游标数的位掩码。如果iCursor未设置，则返回0
*/
static Bitmask getMask(WhereMaskSet *pMaskSet, int iCursor){
  int i;
  assert( pMaskSet->n<=(int)sizeof(Bitmask)*8 );  /*判定*/
  for(i=0; i<pMaskSet->n; i++){
    if( pMaskSet->ix[i]==iCursor ){
      return ((Bitmask)1)<<i;
    }
  }
  return 0;
}

/*
** Create a new mask for cursor iCursor.
** 为游标iCursor创建一个新的掩码
** 
** There is one cursor per table in the FROM clause.  The number of
** tables in the FROM clause is limited by a test early in the
** sqlite3WhereBegin() routine.  So we know that the pMaskSet->ix[]
** array will never overflow.
**
** 在FROM子句中的每个表有一个游标。在FROM子句中的表的个数由sqlite3WhereBegin()程序的一个测试限制。
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
**
** 这个程序(递归地)访问一个表达式树，并生成一个位掩码指示该表达式树使用了哪些表。
**
** In order for this routine to work, the calling function must have
** previously invoked sqlite3ResolveExprNames() on the expression.  
** See the header comment on that routine for additional information.
** The sqlite3ResolveExprNames() routines looks for column names and
** sets their opcodes to TK_COLUMN and their Expr.iTable fields to
** the VDBE cursor number of the table.  This routine just has to
** translate the cursor numbers into bitmask values and OR all
** the bitmasks together.
**
** 为了让这个程序工作，调用函数必须预先唤醒表达式中的sqlite3ResolveExprNames()。
** 查看sqlite3ResolveExprNames()程序头部的注释信息。
** sqlite3ResolveExprNames()程序查找列名，把这些列的opcodes设置为TK_COLUMN，
** 并把它们的Expr.iTable字段设为表的VDBE游标数。 
** 这个程序只是把游标数转换为位掩码，将所有的位掩码集合起来。
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
**
** 如果给出的运算符是一个带索引的WHERE子句的term可以使用的运算符，那么就返回TRUE.
** 允许的运算符有"=", "<", ">", "<=", ">=", and "IN"
**
** IMPLEMENTATION-OF: R-59926-26393 To be usable by an index a term must be
** of one of the following forms: column = expression column > expression
** column >= expression column < expression column <= expression
** expression = column expression > column expression >= column
** expression < column expression <= column column IN
** (expression-list) column IN (subquery) column IS NULL
**
** IMPLEMENTATION-OF: R-59926-26393 一个term可使用索引必须是以下情况之一:
** column = expression 
** column > expression
** column >= expression 
** column < expression 
** column <= expression
** expression = column 
** expression > column 
** expression >= column
** expression < column 
** expression <= column 
** column IN (expression-list) 
** column IN (subquery) 
** column IS NULL
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
**
** 交换TYPE类型的两个对象
*/
#define SWAP(TYPE,A,B) {TYPE t=A; A=B; B=t;}

/*
** Commute a comparison operator.  Expressions of the form "X op Y"
** are converted into "Y op X".
** 交换一个比较操作符两边。"X op Y"表达式转换为"Y op X"
**
** If a collation sequence is associated with either the left or right
** side of the comparison, it remains associated with the same side after
** the commutation. So "Y collate NOCASE op X" becomes 
** "X collate NOCASE op Y". This is because any collation sequence on
** the left hand side of a comparison overrides any collation sequence 
** attached to the right. For the same reason the EP_ExpCollate flag
** is not commuted.
**
** 如果一个排序序列与比较式的左边或右边相关，交换后它仍与相同边有关。
** 因此 "Y collate NOCASE op X" 变成了 "X collate NOCASE op Y" 。
** 这是因为任何在比较式左边的排序序列重写右边的任何排序序列。
** 也由此，EP_ExpCollate标志不交换。
*/
static void exprCommute(Parse *pParse, Expr *pExpr){
  u16 expRight = (pExpr->pRight->flags & EP_ExpCollate);  /*pLeft表示左子节点，pRight表示右子节点。*/
  u16 expLeft = (pExpr->pLeft->flags & EP_ExpCollate);  /*EP_ExpCollate表示明确规定的排序序列*/
  assert( allowedOp(pExpr->op) && pExpr->op!=TK_IN );  /*判定表达式的运算符，"=", "<", ">", "<=", ">=", and "IN"的一种*/
  pExpr->pRight->pColl = sqlite3ExprCollSeq(pParse, pExpr->pRight);  /*pColl表示列的排序类型或者为0*/
  pExpr->pLeft->pColl = sqlite3ExprCollSeq(pParse, pExpr->pLeft);  /*sqlite3ExprCollSeq()方法返回表达式pExpr的默认排序序列。如果没有默认排序类型，返回0。*/
  SWAP(CollSeq*,pExpr->pRight->pColl,pExpr->pLeft->pColl);  /*交换左右两个节点的排序类型*/
  pExpr->pRight->flags = (pExpr->pRight->flags & ~EP_ExpCollate) | expLeft;  
  pExpr->pLeft->flags = (pExpr->pLeft->flags & ~EP_ExpCollate) | expRight;
  SWAP(Expr*,pExpr->pRight,pExpr->pLeft);  /*交换两个左右子节点*/
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
**
** 把TK_xx操作符转化为WO_xx位掩码
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
**
** 在WHERE子句中查找一个term，这个term由"X <op> <expr>"形式构成。
** 其中X是与表iCur的iColumn相关的,<op>是由op参数指定的WO_xx运算符的一种。
** 返回一个指向term的指针。如果没有找到就返回0。
*/
static WhereTerm *findTerm(
  WhereClause *pWC,     /* The WHERE clause to be searched  要被查找的WHERE子句 */
  int iCur,             /* Cursor number of LHS  LHS(等式的左边)的游标编号 */
  int iColumn,          /* Column number of LHS  LHS(等式的左边)的列标号 */
  Bitmask notReady,     /* RHS must not overlap with this mask  等式的右边不能与掩码重叠 */
  u32 op,               /* Mask of WO_xx values describing operator  描述运算符的WO_xx值的掩码 */
  Index *pIdx           /* Must be compatible with this index, if not NULL  如果非空，必须与此索引相一致 */
){
  WhereTerm *pTerm;  /* 新建一个WhereTerm */
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
		  **
		  ** 从一个索引中找出所需的排序序列，用于优化表达式pX。 将这个值存储在变量pColl中。 
          */
          assert(pX->pLeft);
		  /*sqlite3BinaryCompareCollSeq()返回指向排序序列的指针，该排序序列由一个二进制比较运算符使用，来比较pLeft和pRight。出自expr.c 222行*/
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

/* Forward reference 前置引用 */
static void exprAnalyze(SrcList*, WhereClause*, int);

/*
** Call exprAnalyze on all terms in a WHERE clause.  
**
** 在一个WHERE子句的所有terms上调用exprAnalyze
*/
static void exprAnalyzeAll(
  SrcList *pTabList,       /* the FROM clause  FROM子句 */
  WhereClause *pWC         /* the WHERE clause to be analyzed  被分析的WHERE子句 */
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
**
** 查看表达式是否是可以使用不等式约束进行优化的LIKE或GLOB运算符。
** 如果它是则返回TRUE，如果不是则返回FALSE.
**
** In order for the operator to be optimizible, the RHS must be a string
** literal that does not begin with a wildcard.  
**
** 为了能够优化此运算符，等式的右边必须是一个字符串且不能以通配符开头。
**
*/

/*Expr结构体，语法分析树中的一个表达式的每个节点是该结构的一个实例。 出自sqliteInit.h 1829行*/
static int isLikeOrGlob(
  Parse *pParse,    /* Parsing and code generating context 分析和编码生成上下文 */
  Expr *pExpr,      /* Test this expression  将测试这个表达式 */
  Expr **ppPrefix,  /* Pointer to TK_STRING expression with pattern prefix  指针指向有模式前缀的TK_STRING表达式 */
  int *pisComplete, /* True if the only wildcard is % in the last character  如果只有一个通配符"%"且在最后，则返回True */
  int *pnoCase      /* True if uppercase is equivalent to lowercase  如果不分大小写则返回True */
){
  const char *z = 0;         /* String on RHS of LIKE operator  LIKE运算符右边的字符串 */
  Expr *pRight, *pLeft;      /* Right and left size of LIKE operator  LIKE运算符右边和左边的大小 */
  ExprList *pList;           /* List of operands to the LIKE operator  LIKE运算符的操作对象列表 */
  int c;                     /* One character in z[]  z[]数组中的一个字符 */
  int cnt;                   /* Number of non-wildcard prefix characters  无通配符前缀的字符的数目 */
  char wc[3];                /* Wildcard characters  通配符 */
  sqlite3 *db = pParse->db;  /* Database connection  数据库连接 */
  sqlite3_value *pVal = 0;
  int op;                    /* Opcode of pRight  pRight的Opcode */

  if( !sqlite3IsLikeFunction(db, pExpr, pnoCase, wc) ){  /*判断是否是LIKE函数*/
    return 0;
  }
#ifdef SQLITE_EBCDIC
  if( *pnoCase ) return 0;
#endif
  pList = pExpr->x.pList;  
  pLeft = pList->a[1].pExpr;  
  if( pLeft->op!=TK_COLUMN 
	/*sqlite3ExprAffinity()方法返回表达式pExpr存在关联性 'affinity'*/	  
   || sqlite3ExprAffinity(pLeft)!=SQLITE_AFF_TEXT   /*SQLITE_AFF_TEXT表示列关联类型。*/
   || IsVirtual(pLeft->pTab)  /*测试该表是否是一个虚表。 pTab表示表达式TK_COLUMN的表*/
  ){
    /* IMP: R-02065-49465 The left-hand side of the LIKE or GLOB operator must
    ** be the name of an indexed column with TEXT affinity. 
    **
	**IMP: R-02065-49465 LIKE或GLOB运算符的左边必须是一个TEXT亲和性的带索引的列名
	*/
    return 0;
  }
  assert( pLeft->iColumn!=(-1) );  /* Because IPK never has AFF_TEXT 因为IPK从没有TEXT亲和性 */

  pRight = pList->a[0].pExpr; 
  op = pRight->op;
  /*
  ** int iTable;            // TK_COLUMN: cursor number of table holding column	  TK_COLUMN:表列持有光标数
  **				           TK_REGISTER: register number			TK_REGISTER:寄存器号码
  **				           TK_TRIGGER: 1 -> new, 0 -> old 			TK_TRIGGER:1->新,0->旧
  ** ynVar iColumn;         // TK_COLUMN: column index.  -1 for rowid.		TK_COLUMN:列索引。-1为ROWID
  **				           TK_VARIABLE: variable number (always >= 1). 	TK_VARIABLE: 变量数(总是大于等于1)
  ** u8 op2;                // TK_REGISTER: original value of Expr.op		TK_REGISTER:Expr.op的原始值
  **				           TK_COLUMN: the value of p5 for OP_Column		TK_COLUMN: 对于OP_Column，P5的值
  **				           TK_AGG_FUNCTION: nesting depth 			TK_AGG_FUNCTION: 嵌套深度
  ** AggInfo *pAggInfo;     // Used by TK_AGG_COLUMN and TK_AGG_FUNCTION 		TK_AGG_COLUMN和TK_AGG_FUNCTION使用
  ** Table *pTab;           // Table for TK_COLUMN expressions. 			表达式TK_COLUMN的表
  */
  if( op==TK_REGISTER ){
    op = pRight->op2;
  }
  if( op==TK_VARIABLE ){
    Vdbe *pReprepare = pParse->pReprepare;  /*pRepreparevia表示VM被重修，sqlite3Reprepare()函数*/
    int iCol = pRight->iColumn;
	/*sqlite3VdbeGetValue()方法返回一个指向sqlite3_value结构体，这个结构体包括由一个虚拟机v实例的iVar参数值。*/
    pVal = sqlite3VdbeGetValue(pReprepare, iCol, SQLITE_AFF_NONE);  /*SQLITE_AFF_NONE表示列关联类型*/
    if( pVal && sqlite3_value_type(pVal)==SQLITE_TEXT ){
      z = (char *)sqlite3_value_text(pVal);
    }
	/* sqlite3VdbeSetVarmask()方法配置SQL变量iVar的值使得绑定一个新值来唤醒函数sqlite3_reoptimize(),
	这个函数可以重复准备SQL语句产生一个更好的查询计划。*/  
    sqlite3VdbeSetVarmask(pParse->pVdbe, iCol);         
    assert( pRight->op==TK_VARIABLE || pRight->op==TK_REGISTER );
  }else if( op==TK_STRING ){
    z = pRight->u.zToken;  /*zToken表示 标记值，零终止，未引用*/
  }
  if( z ){
    cnt = 0;
    while( (c=z[cnt])!=0 && c!=wc[0] && c!=wc[1] && c!=wc[2] ){
      cnt++;
    }
    if( cnt!=0 && 255!=(u8)z[cnt-1] ){
      Expr *pPrefix;
      *pisComplete = c==wc[0] && z[cnt+1]==0;
      pPrefix = sqlite3Expr(db, TK_STRING, z);  /*SQL数据类型(TK_INTEGER,TK_FLOAT,TK_BLOB或TK_STRING)*/
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
          ** 这会引起sqlite3_bind_parameter_name() API的一些问题。为了解决这些问题，在这添加一个虚拟的OP_Variable.
          **
          */ 
          int r1 = sqlite3GetTempReg(pParse);  /*分配一个新的寄存器，来存储一些中间结果。*/
		  /*生成代码到当前VDBE，评估给定表达式。将结果存储在“target”寄存器中。返回存储结果的寄存器。*/
          sqlite3ExprCodeTarget(pParse, pRight, r1);
		  /*sqlite3VdbeChangeP3()方法为一个特定的指令改变操作数P3的值。*/
		  /*sqlite3VdbeCurrentAddr()方法返回插入下一条指令的地址*/
          sqlite3VdbeChangeP3(v, sqlite3VdbeCurrentAddr(v)-1, 0);
		  /*释放一个寄存器，使它能被其他目的重新使用。*/
          sqlite3ReleaseTempReg(pParse, r1);
        }
      }
    }else{
      z = 0;
    }
  }

  sqlite3ValueFree(pVal);  /*释放sqlite3_value对象*/
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
** 查看表达式是否是column MATCH expr形式，如果是则返回TRUE，否则返回FALSE
**
*/
static int isMatchOfColumn(
  Expr *pExpr      /* Test this expression 测试这个表达式*/
){
  ExprList *pList;  /*定义一个表达式列表结构体的实例*/

  if( pExpr->op!=TK_FUNCTION ){   /*TK_FUNCTION表示表达式是一个SQL函数*/
    return 0;
  }
  /*sqlite3StrICmp为宏定义，表示内部函数原型*/
  if( sqlite3StrICmp(pExpr->u.zToken,"match")!=0 ){  /*zToken表示 标记值。零终止，未引用*/
    return 0;
  }
  /*
  union {
    ExprList *pList;      Function arguments or in "<expr> IN (<expr-list)" 	函数参数或者"<表达式>IN(<表达式列表>)
    Select *pSelect;      Used for sub-selects and "<expr> IN (<select>)" 	用于子选择和"<表达式>IN(<选择>)"
  } x;
  x为Expr结构体定义中的共同体，出自sqliteInt.h 1845行
  */
  pList = pExpr->x.pList;
  if( pList->nExpr!=2 ){   /*nExpr表示列表中表达式的数目*/
    return 0;
  }
  if( pList->a[1].pExpr->op != TK_COLUMN ){  /*TK_COLUMN表示表达式是一个列*/
    return 0;
  }
  return 1;
}
#endif /* SQLITE_OMIT_VIRTUALTABLE */

/*
** If the pBase expression originated in the ON or USING clause of
** a join, then transfer the appropriate markings over to derived.
**
** 如果pBase表达式起源于一个连接的ON或USING子句，那么将适当的连接标记转移给派生的表达式 pDerived。
**
*/
static void transferJoinMarkings(Expr *pDerived, Expr *pBase){
  pDerived->flags |= pBase->flags & EP_FromJoin;  /*EP_FromJoin表示起源于连接的ON或USING子句*/
  pDerived->iRightJoinTable = pBase->iRightJoinTable;  /*iRightJoinTable表示右连接表编号*/
}

#if !defined(SQLITE_OMIT_OR_OPTIMIZATION) && !defined(SQLITE_OMIT_SUBQUERY)
/*
** Analyze a term that consists of two or more OR-connected
** subterms.  
** 例如下面这种形式:
**
**     ... WHERE  (a=5) AND (b=7 OR c=9 OR d=13) AND (d=13)
**                          ^^^^^^^^^^^^^^^^^^^^
** 分析一个包含两个或更多OR连接的子term的term。
**
** This routine analyzes terms such as the middle term in the above example.
** A WhereOrTerm object is computed and attached to the term under
** analysis, regardless of the outcome of the analysis.  Hence:
**
**     WhereTerm.wtFlags |= TERM_ORINFO
**     WhereTerm.u.pOrInfo = a dynamically allocated WhereOrTerm object  动态分配WhereOrTerm对象
**
** 这个程序分析如上面例子的中部term的terms。
** 一个WhereOrTerm对象被计算，并附加到正被分析的term中，不管分析的结果如何。
**
** The term being analyzed must have two or more of OR-connected subterms.
** A single subterm might be a set of AND-connected sub-subterms.
**
** 分析的term必须包含多个OR连接的子term，一个单独的子term可能是一组AND连接的子term（如下面的例子 C、D、E）
**
** Examples of terms under analysis(例子如下):
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
** 如果所有子terms是对于单表T的同一列 C的等式表达式 T.C=expr形式（如上面的例子 B），
** 则创建一个新的虚拟term，等同于IN表达式。换句话说，如果被分析的term如下：  
**
**      x = expr1  OR  expr2 = x  OR  x = expr3
**
** then create a new virtual term like this:
** 则创建一个新的虚拟term如下： 
**
**      x IN (expr1,expr2,expr3)
**
** CASE 2:  如果是同一表中的列，且如果是"=", "<", "<=", ">", ">=", "IS NULL", or "IN"，则可以使用索引优化
**
** If all subterms are indexable by a single table T, then set
**
**     WhereTerm.eOperator              =  WO_OR
**     WhereTerm.u.pOrInfo->indexable  |=  the cursor number for table T   //T表的游标数
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
**     WhereTerm.u.pOrInfo->indexable  |=  the cursor number for table T   //T表的游标数
**  
** 如果一个term是"T.C <op> <expr>"的形式，其中C是表T中的任意列，
** <op>是"=", "<", "<=", ">", ">=", "IS NULL", or "IN"中的一个，
** 那么这个子term是"indexable"。如果一个子term是由一个AND连接的
** 两个或更多子子terms，并且其中的子terms至少有一个是indexable，
** 那么这个子term也是可加索引的。可加索引的AND子terms把它们的eOperator设置
** 为WO_AND，把u.pAndInfo设置为一个动态分配的WhereAndTerm对象。
**
** From another point of view, "indexable" means that the subterm could
** potentially be used with an index if an appropriate index exists.
** This analysis does not consider whether or not the index exists; that
** is something the bestIndex() routine will determine.  This analysis
** only looks at whether subterms appropriate for indexing exist.
**
** 从另一方面看，"可加索引的"意味着如果一个适当的索引存在，那么子term可以通过一个索引被使用。
** 这个分析不考虑索引是否存在，这是bestIndex()程序考虑的事。这个分析只关注子terms是否有合适的索引存在。
**
** All examples A through E above all satisfy case 2.  But if a term
** also statisfies case 1 (such as B) we know that the optimizer will
** always prefer case 1, so in that case we pretend that case 2 is not
** satisfied.
** 
** 上面的A到E所有例子都满足CASE 2。但是，如果一个term也满足CASE 1（如例子 B），
** 由于优化器总是倾向于CASE 1，因此，在这种情况下，我们认为CASE 2不适合。  
** （如果情况1和情况2都满足，则默认使用情况1优化）
**
** It might be the case that multiple tables are indexable.  For example,
** (E) above is indexable on tables P, Q, and R.  
** 
** 可能有这种情况，在多个表上是可索引的，如例子 E 在表P、Q、R上都可索引。
** 
** Terms that satisfy case 2 are candidates for lookup by using
** separate indices to find rowids for each subterm and composing
** the union of all rowids using a RowSet object.  This is similar
** to "bitmap indices" in other database engines.
**
** 满足情况2的terms可以使用各自的索引为每个subterm找到rowid并且使用RowSet对象组合所有rowid。
** 这类似于其他数据库引擎的“bitmap indices” 
**
** OTHERWISE:
** If neither case 1 nor case 2 apply, then leave the eOperator set to
** zero.  This term is not useful for search.
**
** 否则： 如果既不满足CASE 1，也不满足CASE 2，则将eOperator设置为0. 这个term对于查询是没有用的。  
*/
static void exprAnalyzeOrTerm(
  SrcList *pSrc,            /* the FROM clause  FROM子句 */
  WhereClause *pWC,         /* the complete WHERE clause  完整的WHERE子句 */
  int idxTerm               /* Index of the OR-term to be analyzed  将被分析的OR-term索引 */
){
  Parse *pParse = pWC->pParse;            /* Parser context 解析器上下文 */
  sqlite3 *db = pParse->db;               /* Database connection 数据库连接 */
  WhereTerm *pTerm = &pWC->a[idxTerm];    /* The term to be analyzed 要分析的term */
  Expr *pExpr = pTerm->pExpr;             /* The expression of the term  term的表达式 */
  WhereMaskSet *pMaskSet = pWC->pMaskSet; /* Table use masks  表使用的掩码 */
  int i;                                  /* Loop counters   循环计数器 */
  WhereClause *pOrWc;       /* Breakup of pTerm into subterms  （OR运算符）分解成子terms的where子句 pTerm */
  WhereTerm *pOrTerm;       /* A Sub-term within the pOrWc  一个在pOrWc中的子term */
  WhereOrInfo *pOrInfo;     /* Additional information associated with pTerm  与（OR运算符）pTerm相关的附加信息 */
  Bitmask chngToIN;         /* Tables that might satisfy case 1  满足情况1的表 */
  Bitmask indexable;        /* Tables that are indexable, satisfying case 2  可加索引且满足情况2的表 */

  /*
  ** Break the OR clause into its separate subterms.  The subterms are
  ** stored in a WhereClause structure containing within the WhereOrInfo
  ** object that is attached to the original OR clause term.
  **
  ** 把OR子句分解成单独的子terms。子terms存储在一个包含在WhereOrInfo对象的WhereClause结构中，
  ** WhereOrInfo对象附加到原始的OR子句term中。
  */
  /*
  ** wtFlags表示TERM_xxx bit标志，TERM_DYNAMIC表示需要调用sqlite3ExprDelete(db, pExpr)，
  ** TERM_ORINFO表示需要释放WhereTerm.u.pOrInfo对象，TERM_ANDINFO表示需要释放WhereTerm.u.pAndInfo对象。
  */
  assert( (pTerm->wtFlags & (TERM_DYNAMIC|TERM_ORINFO|TERM_ANDINFO))==0 );
  assert( pExpr->op==TK_OR );  /*判定为 OR表达式*/
  pTerm->u.pOrInfo = pOrInfo = sqlite3DbMallocZero(db, sizeof(*pOrInfo));  /*分配内存*/
  if( pOrInfo==0 ) return;  /*分配内存失败，结束*/
  pTerm->wtFlags |= TERM_ORINFO;
  pOrWc = &pOrInfo->wc;  /* wc表示where子句分解为子terms */
  whereClauseInit(pOrWc, pWC->pParse, pMaskSet, pWC->wctrlFlags);  /*初始化一个预分配的WhereClause数据结构*/
  whereSplit(pOrWc, pExpr, TK_OR);  /*识别在WHERE子句中的子表达式，根据OR运算符*/
  exprAnalyzeAll(pSrc, pOrWc);  /*分析所有子句*/
  if( db->mallocFailed ) return;  /*动态内存分配失败，结束*/
  assert( pOrWc->nTerm>=2 );  /*判定子句的子term大于等于2个，OR运算符*/
  /*
  ** Compute the set of tables that might satisfy cases 1 or 2.
  **
  ** 计算可能满足情况1或情况2的表
  */
  indexable = ~(Bitmask)0;  /*可加索引且满足情况2的表*/
  chngToIN = ~(pWC->vmask);  /*满足情况1的表*/
  /*考虑例子 E 形式*/
  for(i=pOrWc->nTerm-1, pOrTerm=pOrWc->a; i>=0 && indexable; i--, pOrTerm++){  /*循环where子句的每个子term，子term（OR运算符）的每一项*/
	/*WO_XX表示运算符的位掩码，索引可以进行开发。这些值的OR-ed组合可以在查找where子句的子terms时被使用.*/  
    if( (pOrTerm->eOperator & WO_SINGLE)==0 ){  /*WO_SINGLE表示所有单一的WO_XX值的掩码*/
      WhereAndInfo *pAndInfo;  /*AND运算符*/
      assert( pOrTerm->eOperator==0 );  /*eOperator表示描述<op>的一个WO_XX*/
      assert( (pOrTerm->wtFlags & (TERM_ANDINFO|TERM_ORINFO))==0 );
      chngToIN = 0;  
      pAndInfo = sqlite3DbMallocRaw(db, sizeof(*pAndInfo));  /*分配内存*/
      if( pAndInfo ){
        WhereClause *pAndWC;  /*（AND运算符）分解成子terms的pTerm，pTerm为OR运算符的子term*/
        WhereTerm *pAndTerm;  /*一个在pOrWc中的子term*/
        int j;  /*循环计数器*/
        Bitmask b = 0;  
        pOrTerm->u.pAndInfo = pAndInfo;  /*与（AND运算符）pTerm相关的附加信息*/
        pOrTerm->wtFlags |= TERM_ANDINFO;  /*赋值为TERM_ANDINFO标志*/
        pOrTerm->eOperator = WO_AND;  /*运算符赋值为WO_AND*/
        pAndWC = &pAndInfo->wc;
        whereClauseInit(pAndWC, pWC->pParse, pMaskSet, pWC->wctrlFlags);   /*初始化一个预分配的WhereClause数据结构*/
        whereSplit(pAndWC, pOrTerm->pExpr, TK_AND);  /*识别在WHERE子句中的子表达式，根据AND运算符*/
        exprAnalyzeAll(pSrc, pAndWC);  /*分析所有子句*/
        pAndWC->pOuter = pWC;  /*外连接*/
        testcase( db->mallocFailed );  /*宏testcase()用于帮助覆盖测试。*/
        if( !db->mallocFailed ){
          for(j=0, pAndTerm=pAndWC->a; j<pAndWC->nTerm; j++, pAndTerm++){  /*循环OR运算符的每个子term，子term（AND运算符）的每一项*/
            assert( pAndTerm->pExpr );  
            if( allowedOp(pAndTerm->pExpr->op) ){  /*运算符"=", "<", "<=", ">", ">=", "IS NULL", or "IN"的一种*/
              b |= getMask(pMaskSet, pAndTerm->leftCursor);  /*返回给出的游标数的位掩码，AND运算符的左游标*/
            }
          }
        }
        indexable &= b;  /*获得满足情况2的表*/
      }
    }else if( pOrTerm->wtFlags & TERM_COPIED ){  /*TERM_COPIED表示有一个child*/
      /* Skip this term for now.  We revisit it when we process the 
      ** corresponding TERM_VIRTUAL term 
	  **
	  ** 暂时跳过这个term.当执行对应的的TERM_VIRTUAL term时会重新访问它。	
	  */
    }else{
      Bitmask b;
      b = getMask(pMaskSet, pOrTerm->leftCursor);  /*返回给出的游标数的位掩码，OR运算符的左游标*/
      if( pOrTerm->wtFlags & TERM_VIRTUAL ){  /*TERM_VIRTUAL表示由优化器添加，不需要编码*/
        WhereTerm *pOther = &pOrWc->a[pOrTerm->iParent];  /*定义where子句的子term*/
        b |= getMask(pMaskSet, pOther->leftCursor);
      }
      indexable &= b;  /*获得满足情况2的表*/
      if( pOrTerm->eOperator!=WO_EQ ){  /*OR运算符子term的运算符的位掩码不是WO_EQ*/
        chngToIN = 0;
      }else{
        chngToIN &= b;  /*获得满足情况1的表*/
      }
    }
  }

  /*
  ** Record the set of tables that satisfy case 2.  The set might be
  ** empty. 
  ** 记录满足情况2的表。这个可能为空。
  */
  pOrInfo->indexable = indexable;
  pTerm->eOperator = indexable==0 ? 0 : WO_OR;  
  /*
  ** chngToIN holds a set of tables that *might* satisfy case 1.  But
  ** we have to do some additional checking to see if case 1 really
  ** is satisfied.
  ** 
  ** 出现第一种情况时的处理
  ** chngToIN保存可能满足情况1的表。但我们需要做一些附加检查看看是不是真的满足情况1
  **
  ** chngToIN will hold either 0, 1, or 2 bits.  
  ** The 0-bit case means that there is no possibility of transforming 
  ** the OR clause into an IN operator because one or more terms in the 
  ** OR clause contain something other than == on a column in the single table.  
  ** The 1-bit case means that every term of the OR clause is of the form
  ** "table.column=expr" for some single table.  The one bit that is set
  ** will correspond to the common table.  We still need to check to make
  ** sure the same column is used on all terms.  
  ** The 2-bit case is when the all terms are of the form "table1.column=table2.column".  
  ** It might be possible to form an IN operator with either table1.column or table2.column 
  ** as the LHS if either is common to every term of the OR clause.
  **
  ** chngToIN将保存0,1或2 bits.
  ** 0-bit情况意味着无法把OR子句转化为IN运算符(如例子B),因为OR子句中的一个或多个terms在
  ** 单个表的一列上包含除==运算符以外的其他东西。 
  ** 1-bit情况意味着OR子句的每个term对于某个单表都是"table.column=expr"形式。
  ** 我们还需要进一步验证所有terms使用的同一列。 
  ** 2-bit情况意味着所有的terms都是"table1.column=table2.column"形式。它可能形成
  ** 一个table1.column或table2.column作为左边操作数的IN运算符。
  ** 如果任何一个（table1.column或table2.column）对OR子句的每个term都是共用的。
  **
  ** Note that terms of the form "table.column1=table.column2" (the
  ** same table on both sizes of the ==) cannot be optimized.
  ** 
  ** 注意:"table.column1=table.column2"(在==的两边都是同一个表)形式是不能优化的
  */
  if( chngToIN ){  /*满足情况1*/
    int okToChngToIN = 0;     /* True if the conversion to IN is valid  如果可以转换为IN操作符则为TRUE */
    int iColumn = -1;         /* Column index on lhs of IN operator  IN运算符左边的列索引 */
    int iCursor = -1;         /* Table cursor common to all terms  所有terms共同的表游标 */
    int j = 0;                /* Loop counter  循环计数器 */

    /* Search for a table and column that appears on one side or the
    ** other of the == operator in every subterm.  That table and column
    ** will be recorded in iCursor and iColumn.  There might not be any
    ** such table and column.  Set okToChngToIN if an appropriate table
    ** and column is found but leave okToChngToIN false if not found.
    **
    ** 查找一个表和列，它出现在每个子term中==运算符的其中一边。这个表和列被记录在iCursor和iColumn中。
    ** 也可能没有任何这样的表和列。如果一个适当的表和列被查找到，则设置okToChngToIN为TRUE，否则，设置okToChngToIN为FALSE。
    */
    for(j=0; j<2 && !okToChngToIN; j++){
      pOrTerm = pOrWc->a;  /*where子句OR运算符分隔的一个term*/
      for(i=pOrWc->nTerm-1; i>=0; i--, pOrTerm++){
        assert( pOrTerm->eOperator==WO_EQ );  /*判定term运算符为WO_EQ*/
        pOrTerm->wtFlags &= ~TERM_OR_OK;  /*OR运算符*/
        if( pOrTerm->leftCursor==iCursor ){  /*iCursor表示满足情况1的表*/
          /* This is the 2-bit case and we are on the second iteration and   	
          ** current term is from the first iteration.  So skip this term. 
		  **
		  ** 这是2-bit的情况，我们是在第二次迭代下，当前term是来自于第一次迭代。所以跳过这个term。	
		  */
          assert( j==1 );
          continue;
        }
        if( (chngToIN & getMask(pMaskSet, pOrTerm->leftCursor))==0 ){
          /* This term must be of the form t1.a==t2.b where t2 is in the 
          ** chngToIN set but t1 is not.  This term will be either preceeded or
          ** follwed by an inverted copy (t2.b==t1.a).  Skip this term and use its inversion. 
		  **
		  ** 这个term必须是t1.a==t2.b形式，其中t2是chngToIN中的表，但t1不是。
		  ** 这个term将被执行或进行反转复制(t2.b==t1.a)。跳过这个term，使用它的反转形式。	
		  */
          testcase( pOrTerm->wtFlags & TERM_COPIED );  /*宏testcase()用于帮助覆盖测试。*/
          testcase( pOrTerm->wtFlags & TERM_VIRTUAL );
          assert( pOrTerm->wtFlags & (TERM_COPIED|TERM_VIRTUAL) );
          continue;
        }
        iColumn = pOrTerm->u.leftColumn;  /*确定列*/
        iCursor = pOrTerm->leftCursor;  /*确定表*/
        break;
      }
      if( i<0 ){
        /* No candidate table+column was found.  This can only occur on the second iteration.
        **  
		** 没有找到如上所述的候选表和列。这只能发生在第二次循环。
		*/
        assert( j==1 );
        assert( (chngToIN&(chngToIN-1))==0 );
        assert( chngToIN==getMask(pMaskSet, iCursor) );
        break;
      }
      testcase( j==1 );

      /* We have found a candidate table and column.  Check to see if that 
      ** table and column is common to every term in the OR clause 
	  ** 
	  ** 已经发现了一个候选表和列。再查看表和列是否对OR子句中的所有term都是共同的。
	  */
      okToChngToIN = 1;
      for(; i>=0 && okToChngToIN; i--, pOrTerm++){
        assert( pOrTerm->eOperator==WO_EQ );
        if( pOrTerm->leftCursor!=iCursor ){  /*表不是*/
          pOrTerm->wtFlags &= ~TERM_OR_OK;  /*非TERM_OR_OK*/
        }else if( pOrTerm->u.leftColumn!=iColumn ){  /*列不是*/
          okToChngToIN = 0;  /*候选表和列不是，不能转换为IN操作符*/
        }else{
          int affLeft, affRight;
          /* If the right-hand side is also a column, then the affinities  
          ** of both right and left sides must be such that no type 
          ** conversions are required on the right.  (Ticket #2249)
          ** 
		  ** 如果右边也是一个列，那么左右两边的关联性是必须的这样的，右边不需要类型转换。
		  */
          affRight = sqlite3ExprAffinity(pOrTerm->pExpr->pRight);  /*返回表达式pExpr的右边存在的关联性 'affinity'*/
          affLeft = sqlite3ExprAffinity(pOrTerm->pExpr->pLeft);  /*返回表达式pExpr的左边存在的关联性 'affinity'*/
          if( affRight!=0 && affRight!=affLeft ){  /*两边五关联性*/
            okToChngToIN = 0;
          }else{
            pOrTerm->wtFlags |= TERM_OR_OK;  /*有关联性*/
          }
        }
      }
    }

    /* At this point, okToChngToIN is true if original pTerm satisfies case 1.
    ** In that case, construct a new virtual term that is pTerm converted into an IN operator.
    **
    ** 这时，如果原始的pTerm满足情况1，则okToChngToIN为TRUE。这种情况下，需要构造一个新的虚拟的term，把pTerm转换为IN操作符。
    */
    if( okToChngToIN ){
      Expr *pDup;            /* A transient duplicate expression  一个临时的复制表达式 */
      ExprList *pList = 0;   /* The RHS of the IN operator  IN操作符右边 */
      Expr *pLeft = 0;       /* The LHS of the IN operator  IN操作符左边 */
      Expr *pNew;            /* The complete IN operator  完整的IN操作符 */

      for(i=pOrWc->nTerm-1, pOrTerm=pOrWc->a; i>=0; i--, pOrTerm++){
        if( (pOrTerm->wtFlags & TERM_OR_OK)==0 ) continue;
        assert( pOrTerm->eOperator==WO_EQ );
        assert( pOrTerm->leftCursor==iCursor );
        assert( pOrTerm->u.leftColumn==iColumn );  /*判定是满足情况1的表和列，操作符*/
        pDup = sqlite3ExprDup(db, pOrTerm->pExpr->pRight, 0);  /*复制表达式右边。 出自expr.c 900行*/
		/*在表达式列表末尾添加一个新的元素。如果pList最初是空的，则创建一个新的表达式列表。*/
        pList = sqlite3ExprListAppend(pWC->pParse, pList, pDup);
        pLeft = pOrTerm->pExpr->pLeft;
      }
      assert( pLeft!=0 );
      pDup = sqlite3ExprDup(db, pLeft, 0);  /*复制表达式左边*/
      pNew = sqlite3PExpr(pParse, TK_IN, pDup, 0, 0);  /*分配一个Expr表达式节点，连接两个子树节点。 出自expr.c 496行*/
      if( pNew ){  /*完整的IN操作符复制成功*/
        int idxNew;  /*用于索引*/
        transferJoinMarkings(pNew, pExpr);
        assert( !ExprHasProperty(pNew, EP_xIsSelect) );  /*ExprHasProperty宏在Expr.flags字段用于测试*/
        pNew->x.pList = pList;
        idxNew = whereClauseInsert(pWC, pNew, TERM_VIRTUAL|TERM_DYNAMIC);  /*返回新WhereTerm中pWC->a[]的索引*/
        testcase( idxNew==0 );  /*覆盖测试*/
        exprAnalyze(pSrc, pWC, idxNew);  /*分析子表达式，即whereterm*/
        pTerm = &pWC->a[idxTerm];  
        pWC->a[idxNew].iParent = idxTerm;
        pTerm->nChild = 1;
      }else{
        sqlite3ExprListDelete(db, pList);  /*删除拷贝列表*/
      }
      pTerm->eOperator = WO_NOOP;   /* case 1 trumps case 2  情况1胜过情况2。  WO_NOOP表示这个term不限制搜索空间*/
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
** 这个程序的输入是一个只填充"pExpr"字段的WhereTerm数据结构。
** 这个程序用于分析子表达式和填充WhereTerm数据结构的其他字段。
**
** If the expression is of the form "<expr> <op> X" it gets commuted
** to the standard form of "X <op> <expr>".
**
** 如果表达式是"<expr> <op> X"形式，把它转化为标准形式"X <op> <expr>".
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
** 如果表达式是"X <op> Y"形式，其中X和Y都是列，那么，原始表达式不会改变，
** 并在WHERE子句中添加一个新的虚拟term为"Y <op> X"形式，两个term分别被分析。
** 原始term被标上TERM_COPIED标志，新term被标上TERM_DYNAMIC标志(因为它的pExpr
** 需要能够在where子句上释放)，和TERM_VIRTUAL标志(因为它是一个先前term的转化复制)。
** 原始term有nChild=1，复本有idxParent，并把idxParent设置为原始term的下标。
*/
static void exprAnalyze(
  SrcList *pSrc,            /* the FROM clause  FROM子句 */
  WhereClause *pWC,         /* the WHERE clause  WHERE子句 */
  int idxTerm               /* Index of the term to be analyzed  需要分析的term下标 */
){
  WhereTerm *pTerm;                /* The term to be analyzed  需要分析的term  */
  WhereMaskSet *pMaskSet;          /* Set of table index masks  表索引位掩码集合 */
  Expr *pExpr;                     /* The expression to be analyzed  需要分析的表达式 */
  Bitmask prereqLeft;              /* Prerequesites of the pExpr->pLeft  pExpr->pLeft的前提条件，位掩码类型。 */
  Bitmask prereqAll;               /* Prerequesites of pExpr   pExpr的前提条件，位掩码类型 */
  Bitmask extraRight = 0;          /* Extra dependencies on LEFT JOIN  在左连接中的额外依赖 */
  Expr *pStr1 = 0;                 /* RHS of LIKE/GLOB operator  LIKE/GLOB运算符的右边 */
  int isComplete = 0;              /* RHS of LIKE/GLOB ends with wildcard  LIKE/GLOB右边由通配符结束 */
  int noCase = 0;                  /* LIKE/GLOB distinguishes case  LIKE/GLOB区分大小写 */
  int op;                          /* Top-level operator.  pExpr->op运算符 */
  Parse *pParse = pWC->pParse;     /* Parsing context  分析WHERE子句上下文 */
  sqlite3 *db = pParse->db;        /* Database connection  数据库连接 */

  if( db->mallocFailed ){  /*数据库动态内存分配失败*/
    return;
  }
  pTerm = &pWC->a[idxTerm];  /*根据下标得到要分析的term*/
  pMaskSet = pWC->pMaskSet;
  pExpr = pTerm->pExpr;  /*定义要分析的term的表达式*/
  prereqLeft = exprTableUsage(pMaskSet, pExpr->pLeft);  /*(递归地)访问一个表达式树，并生成一个位掩码指示该表达式树使用了哪些表。*/
  op = pExpr->op;
  if( op==TK_IN ){  /*IN运算符*/ 
    assert( pExpr->pRight==0 );  
	/*宏在Expr.flags字段用于测试，ExprHasProperty(E,P) (((E)->flags&(P))==(P))*/
    if( ExprHasProperty(pExpr, EP_xIsSelect) ){   /*EP_xIsSelect表示x.pSelect是有效的(否则x.pList是有效的)*/
      pTerm->prereqRight = exprSelectTableUsage(pMaskSet, pExpr->x.pSelect);  /*得到引用的Select表的位掩码*/ 
    }else{
      pTerm->prereqRight = exprListTableUsage(pMaskSet, pExpr->x.pList);  /*得到引用的List表的位掩码*/
    }
  }else if( op==TK_ISNULL ){  /*运算符不符合要求*/
    pTerm->prereqRight = 0;
  }else{
    pTerm->prereqRight = exprTableUsage(pMaskSet, pExpr->pRight);  /*得到pExpr表达式右边节点的位掩码*/
  }
  prereqAll = exprTableUsage(pMaskSet, pExpr);  /*得到pExpr表达式引用的表的位掩码*/
  if( ExprHasProperty(pExpr, EP_FromJoin) ){  /*EP_FromJoin表示起源于join连接的ON或USING语句*/
    Bitmask x = getMask(pMaskSet, pExpr->iRightJoinTable);  /*定义连接的右表的位掩码*/
    prereqAll |= x;
    extraRight = x-1;  /* ON clause terms may not be used with an index  在左连接的左表中的ON子句terms可能不能与索引一起使用
                       ** on left table of a LEFT JOIN.  Ticket #3015 */
  }
  pTerm->prereqAll = prereqAll;  /*定义要分析的term引用的表的位掩码*/
  pTerm->leftCursor = -1;  /*设置要分析的term*/
  pTerm->iParent = -1;
  pTerm->eOperator = 0;
  if( allowedOp(op) && (pTerm->prereqRight & prereqLeft)==0 ){  /*"=", "<", ">", "<=", ">=", and "IN"运算符 */
    Expr *pLeft = pExpr->pLeft;  /*定义表达式左右两边*/
    Expr *pRight = pExpr->pRight;
    if( pLeft->op==TK_COLUMN ){   /*表达式左边*/
      pTerm->leftCursor = pLeft->iTable;  /*表索引*/
      pTerm->u.leftColumn = pLeft->iColumn;  /*列索引*/
      pTerm->eOperator = operatorMask(op);
    }
    if( pRight && pRight->op==TK_COLUMN ){   /*表达式右边*/ 
      WhereTerm *pNew;  /*声明一个新term*/
      Expr *pDup;  /*声明拷贝*/
      if( pTerm->leftCursor>=0 ){  /*leftCursor表示在"X <op> <expr>"中的X的游标数*/
        int idxNew;
        pDup = sqlite3ExprDup(db, pExpr, 0);  /*拷贝表达式*/
        if( db->mallocFailed ){  /*动态内存分配错误*/
          sqlite3ExprDelete(db, pDup);  /*递归删除拷贝的表达式树*/
          return;
        }
		/*添加单个新 WhereTerm 对象到 WhereClause 对象 pWC中。*/
        idxNew = whereClauseInsert(pWC, pDup, TERM_VIRTUAL|TERM_DYNAMIC);  /*将pDup添加到pWC中，返回pDup在pWC->a[]中的索引。*/
        if( idxNew==0 ) return;  /*内存分配错误，添加失败，返回0*/
        pNew = &pWC->a[idxNew];  /*在pWC中得到拷贝的term*/
        pNew->iParent = idxTerm;  /*表示新term是由原始term拷贝得到*/
        pTerm = &pWC->a[idxTerm];  /*要分析的原始term*/
        pTerm->nChild = 1;
        pTerm->wtFlags |= TERM_COPIED;  /*原始term被标上TERM_COPIED标志*/
      }else{
        pDup = pExpr;
        pNew = pTerm;
      }
      exprCommute(pParse, pDup);  /*交换一个比较操作符两边*/
      pLeft = pDup->pLeft;  /*得到拷贝的term交换后的左边*/
      pNew->leftCursor = pLeft->iTable;  /*表索引*/
      pNew->u.leftColumn = pLeft->iColumn;  /*列索引*/
      testcase( (prereqLeft | extraRight) != prereqLeft );  /*覆盖测试*/
      pNew->prereqRight = prereqLeft | extraRight;  /*设置新term，pExpr->pRight引用的表的位掩码*/
      pNew->prereqAll = prereqAll;  /*pExpr引用的表的位掩码*/
      pNew->eOperator = operatorMask(pDup->op);  /*term的运算符*/
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
  **
  ** BETWEEN语句处理部分
  **
  ** 如果一个term是BETWEEN运算符，创建两个新的虚拟terms来定义BETWEEN的范围。
  ** 例如:
  **      a BETWEEN b AND c
  ** 转化为:
  **      (a BETWEEN b AND c) AND (a>=b) AND (a<=c)
  ** 
  ** 两个新的terms添加到WhereClause对象的末尾。
  ** 新的terms是"dynamic"并且是原始BETWEEN term的孩子term。
  ** 这意味着如果BETWEEN term已经编码，那么孩子term将被跳过。
  ** 或者，如果孩子term可以使用索引，那么原始BETWEEN term将被跳过。
  */
  else if( pExpr->op==TK_BETWEEN && pWC->op==TK_AND ){  
    ExprList *pList = pExpr->x.pList;  /*声明新建表达式列表，表示要添加的terms*/
    int i;
    static const u8 ops[] = {TK_GE, TK_LE};  /*定义两个运算符*/
    assert( pList!=0 );
    assert( pList->nExpr==2 );  /*新建两个terms*/
    for(i=0; i<2; i++){
      Expr *pNewExpr;  /*用于保存转换后的新表达式*/
      int idxNew;   /*新插入的表达式的在WhereClause中的下标*/
      pNewExpr = sqlite3PExpr(pParse, ops[i], 	/*创建>=和<=表达式*/
                             sqlite3ExprDup(db, pExpr->pLeft, 0),
                             sqlite3ExprDup(db, pList->a[i].pExpr, 0), 0);
      idxNew = whereClauseInsert(pWC, pNewExpr, TERM_VIRTUAL|TERM_DYNAMIC);  /*得到转换后term在WhereClause中的下标*/
      testcase( idxNew==0 );  /*检测是否转换成功*/
      exprAnalyze(pSrc, pWC, idxNew);  //分析表达式格式是否为x <op> <expr>，如果不是则转化为这种形式
      pTerm = &pWC->a[idxTerm];  /*根据下标得到要分析的term*/ 
      pWC->a[idxNew].iParent = idxTerm;  /*表示新建的子句是between运算符表达式的转化*/
    }
    pTerm->nChild = 2;  /*表示添加了a>=b和a<=c两个*/
  }
#endif /* SQLITE_OMIT_BETWEEN_OPTIMIZATION */

#if !defined(SQLITE_OMIT_OR_OPTIMIZATION) && !defined(SQLITE_OMIT_SUBQUERY)
  /* Analyze a term that is composed of two or more subterms connected by
  ** an OR operator.  
  ** 
  ** 分析的term由OR运算符连接的两个或更多的子terms组成。
  */
  else if( pExpr->op==TK_OR ){   /*表达式由OR运算符连接*、
    assert( pWC->op==TK_AND );  /*where子句由and分隔*/
    exprAnalyzeOrTerm(pSrc, pWC, idxTerm);  /*调用OR运算符分析方法exprAnalyzeOrTerm()进行分析*/
    pTerm = &pWC->a[idxTerm];  /*根据下标得到要分析的term*/
  }
#endif /* SQLITE_OMIT_OR_OPTIMIZATION */

#ifndef SQLITE_OMIT_LIKE_OPTIMIZATION
  /* Add constraints to reduce the search space on a LIKE or GLOB
  ** operator.
  ** A like pattern of the form "x LIKE 'abc%'" is changed into constraints
  **
  **          x>='abc' AND x<'abd' AND x LIKE 'abc%'
  **
  ** The last character of the prefix "abc" is incremented to form the
  ** termination condition "abd".
  **
  ** LIKE语句处理部分
  **
  ** 添加约束来减少LIKE或GLOB运算符的搜索空间。
  ** like运算符的"x LIKE 'abc%'"形式可以转变为约束
  **          x>='abc' AND x<'abd' AND x LIKE 'abc%'
  ** 前缀"abc"的最后一个字符递增，形成结束条件"abd".
  */
  if( pWC->op==TK_AND 
   && isLikeOrGlob(pParse, pExpr, &pStr1, &isComplete, &noCase)  //判断是否是可以优化的LIKE或GLOB运算符
  ){
    Expr *pLeft;       /* LHS of LIKE/GLOB operator   LIKE/GLOB运算符的左边 */
    Expr *pStr2;       /* Copy of pStr1 - RHS of LIKE/GLOB operator  LIKE/GLOB运算符的pStr1 - RHS的拷贝 */
    Expr *pNewExpr1;   /*声明两个新表达式，用于定义添加的约束*/
    Expr *pNewExpr2;
    int idxNew1;  /*用于定义添加约束的下标*/
    int idxNew2;
    CollSeq *pColl;    /* Collating sequence to use  使用排序序列 */

    pLeft = pExpr->x.pList->a[1].pExpr;
    pStr2 = sqlite3ExprDup(db, pStr1, 0);  /*拷贝*/
    if( !db->mallocFailed ){  //如果动态内存分配成功
      u8 c, *pC;       /* Last character before the first wildcard  在第一个通配符前的最后一个字符 */
      pC = (u8*)&pStr2->u.zToken[sqlite3Strlen30(pStr2->u.zToken)-1];  /*得到最后一个字符*/
      c = *pC;
      if( noCase ){  /*如果like或glob区分大小写*/
        /* The point is to increment the last character before the first
        ** wildcard.  But if we increment '@', that will push it into the
        ** alphabetic range where case conversions will mess up the 
        ** inequality.  To avoid this, make sure to also run the full
        ** LIKE on all candidate expressions by clearing the isComplete flag
        **
        ** 目的是在第一个通配符前递增最后一个字符。
        ** 但如果我们增加'@',那将会把它移出字母表的范围，那么字符转换将陷入不平等的混乱。
        ** 为了避免这种情况，通过清除isComplete标志来确保在所有候选表达式上也运行完整的LIKE运算符。
        **
        */
        if( c=='A'-1 ) isComplete = 0;  /* EV: R-64339-08207 */


        c = sqlite3UpperToLower[c];  /*宏sqlite3UpperToLower模仿了标准库函数 toupper()，将大写字母转为小写*/
      }
      *pC = c + 1;	//设置<表达式的字符串的最后一个字符
    }
    pColl = sqlite3FindCollSeq(db, SQLITE_UTF8, noCase ? "NOCASE" : "BINARY",0);  //排序的方式
    pNewExpr1 = sqlite3PExpr(pParse, TK_GE, 
                     sqlite3ExprSetColl(sqlite3ExprDup(db,pLeft,0), pColl),
                     pStr1, 0);   /*创建>=表达式*/
    idxNew1 = whereClauseInsert(pWC, pNewExpr1, TERM_VIRTUAL|TERM_DYNAMIC);  /*将>=表达式插入到where子句中，返回下标*/
    testcase( idxNew1==0 );  /*测试是否插入成功*/
    exprAnalyze(pSrc, pWC, idxNew1);  /*分析表达式格式是否为x <op> <expr>，如果不是则转化为这种形式*/
    pNewExpr2 = sqlite3PExpr(pParse, TK_LT,
                     sqlite3ExprSetColl(sqlite3ExprDup(db,pLeft,0), pColl),
                     pStr2, 0);  /*创建<表达式*/
    idxNew2 = whereClauseInsert(pWC, pNewExpr2, TERM_VIRTUAL|TERM_DYNAMIC);  /*将<表达式插入到where子句中，返回下标*/
    testcase( idxNew2==0 ); //测试是否插入成功
    exprAnalyze(pSrc, pWC, idxNew2); //分析表达式格式是否为x <op> <expr>，如果不是则转化为这种形式
    pTerm = &pWC->a[idxTerm];  /*根据下标得到要分析的term*/ 
    if( isComplete ){   /*如果like或glob右边是由通配符结束，表示新创建的子句属于like语句*/
      pWC->a[idxNew1].iParent = idxTerm;
      pWC->a[idxNew2].iParent = idxTerm;
      pTerm->nChild = 2;  /*表示添加了x>=abc和a<abd两个*/
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
  ** 如果当前表达式是column MATCH expr形式，在约束集合中添加一个WO_MATCH辅助term。
  ** 这个信息由虚表的xBestIndex方法使用。本地的查询优化器不使用MATCH函数做任何事情。
  */
  if( isMatchOfColumn(pExpr) ){
    int idxNew;  /*表示term下标*/
    Expr *pRight, *pLeft;  /*用于表示运算符的左右两边*/
    WhereTerm *pNewTerm;  /*声明一个新term，表示添加的WO_MATCH辅助term*/
    Bitmask prereqColumn, prereqExpr;  /*左边column引用的表的位掩码，右边expr引用的表的位掩码*/

    pRight = pExpr->x.pList->a[0].pExpr;  /*得到表达式右边 expr*/
    pLeft = pExpr->x.pList->a[1].pExpr;  /*得到表达式左边 column*/
    prereqExpr = exprTableUsage(pMaskSet, pRight);  /*得到右边expr引用的表的位掩码*/
    prereqColumn = exprTableUsage(pMaskSet, pLeft);  /*得到左边column引用的表的位掩码*/
    if( (prereqExpr & prereqColumn)==0 ){  /*左右两边引用的不是同一个表*/
      Expr *pNewExpr;
      pNewExpr = sqlite3PExpr(pParse, TK_MATCH,   /*创建MATCH表达式*/
                              0, sqlite3ExprDup(db, pRight, 0), 0);
      idxNew = whereClauseInsert(pWC, pNewExpr, TERM_VIRTUAL|TERM_DYNAMIC);  /*将>=表达式插入到where子句中，返回下标*/ 
      testcase( idxNew==0 );  /*测试是否插入成功*/
      pNewTerm = &pWC->a[idxNew];  /*用插入的表达式定义新term*/
      pNewTerm->prereqRight = prereqExpr;  /*新term的pExpr->pRight引用的表的位掩码，定义为原始表达式右边expr引用的表的位掩码*/ 
      pNewTerm->leftCursor = pLeft->iTable;  /*表索引*/
      pNewTerm->u.leftColumn = pLeft->iColumn;  /*列索引*/
      pNewTerm->eOperator = WO_MATCH;  /*定义运算符*/
      pNewTerm->iParent = idxTerm;  /*表示依附于原始term*/
      pTerm = &pWC->a[idxTerm];  /*根据下标得到要分析的term*/ 
      pTerm->nChild = 1;  /*原始term有一个延伸term*/
      pTerm->wtFlags |= TERM_COPIED;  
      pNewTerm->prereqAll = pTerm->prereqAll;  /*新term引用的表的位掩码*/
    }
  }
#endif /* SQLITE_OMIT_VIRTUALTABLE */

#ifdef SQLITE_ENABLE_STAT3
  /* When sqlite_stat3 histogram data is available an operator of the
  ** form "x IS NOT NULL" can sometimes be evaluated more efficiently
  ** as "x>NULL" if x is not an INTEGER PRIMARY KEY.  So construct a
  ** virtual term of that form.
  **
  ** 当sqlite_stat3柱状图数据是有效的一个"x IS NOT NULL"形式的运算符，
  ** 如果x不是INTEGER PRIMARY KEY，那么"x IS NOT NULL"形式被认为比'x>NULL'是更加有效的。
  ** 所以构建一个该形式的虚拟term。
  **
  ** Note that the virtual term must be tagged with TERM_VNULL.  This
  ** TERM_VNULL tag will suppress the not-null check at the beginning
  ** of the loop.  Without the TERM_VNULL flag, the not-null check at
  ** the start of the loop will prevent any results from being returned.
  **
  ** 注意:构建的虚拟term必须标记为TERM_VNULL。TERM_VNULL标记将在循环的开始禁止非空检查。
  ** 没有TERM_VNULL标志，在循环开始的非空检查将阻止返回的任何结果。
  **
  */
  if( pExpr->op==TK_NOTNULL
   && pExpr->pLeft->op==TK_COLUMN
   && pExpr->pLeft->iColumn>=0
  ){
    Expr *pNewExpr;  /*声明一个表达式，用于构建新表达式*/
    Expr *pLeft = pExpr->pLeft;  /*定义原始表达式的左边*/ 
    int idxNew;  /*声明下标*/
    WhereTerm *pNewTerm;  /*声明一个新term，表示构建的虚拟term*/

    pNewExpr = sqlite3PExpr(pParse, TK_GT,  /*构建x IS NOT NULL表达式*/
                            sqlite3ExprDup(db, pLeft, 0),
                            sqlite3PExpr(pParse, TK_NULL, 0, 0, 0), 0);

    idxNew = whereClauseInsert(pWC, pNewExpr,  /*将新建表达式插入Where子句中，返回下标*/
                              TERM_VIRTUAL|TERM_DYNAMIC|TERM_VNULL);  
    if( idxNew ){  /*插入成功*/
      pNewTerm = &pWC->a[idxNew];  /*设置新建term*/
      pNewTerm->prereqRight = 0;  /*新term右边不引用表*/
      pNewTerm->leftCursor = pLeft->iTable;  /*左边表索引*/
      pNewTerm->u.leftColumn = pLeft->iColumn;  /*列索引*/
      pNewTerm->eOperator = WO_GT;  /*设置新term运算符*/
      pNewTerm->iParent = idxTerm;  /*表示新term依附于原始term*/
      pTerm = &pWC->a[idxTerm];  /*根据下标得到要分析的term*/ 
      pTerm->nChild = 1;  /*原始term有一个延伸term*/
      pTerm->wtFlags |= TERM_COPIED;  /*|=类似+=，表示按位或*/
      pNewTerm->prereqAll = pTerm->prereqAll;  /*新term引用的表的位掩码*/
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
** 如果在pList->a[iFirst...]中的任何表达式与除了iBase表之外的任何表有关联，那么返回TRUE
*/
static int referencesOtherTables(
  ExprList *pList,          /* Search expressions in ths list  在这个表达式列表中查找表达式 */
  WhereMaskSet *pMaskSet,   /* Mapping from tables to bitmaps  表和位图之间的映射 */
  int iFirst,               /* Be searching with the iFirst-th expression   用于搜索表达式的编号 */
  int iBase                 /* Ignore references to this table   忽略对这个表的引用 */
){
  Bitmask allowed = ~getMask(pMaskSet, iBase);  /*得到iBase表的位掩码，表示为不引用*/
  while( iFirst<pList->nExpr ){  /*在允许范围内*/
    if( (exprTableUsage(pMaskSet, pList->a[iFirst++].pExpr)&allowed)!=0 ){  /*得到表达式引用的其他表的位掩码*/
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
** 这个函数在作为第二个参数传递的表达式列表中，查找TK_COLUMN类型的表达式，
** 这个表达式引用相同的列并且使用相同的排序序列作为索引pIdx的iCol'th列。
** 参数iBase表示游标号，用于pIdx引用的表。
**
** If such an expression is found, its index in pList->a[] is returned. If
** no expression is found, -1 is returned.
**
** 若这样的一个表达式被找到，返回它在pList->a[]的索引。如果没有找到，则返回-1.
*/
static int findIndexCol(
  Parse *pParse,                  /* Parse context  分析器上下文 */
  ExprList *pList,                /* Expression list to search  用于查找的表达式列表 */
  int iBase,                      /* Cursor for table associated with pIdx  与pIdx相关的表游标 */
  Index *pIdx,                    /* Index to match column of  匹配列的索引 */
  int iCol                        /* Column of index to match  匹配的索引pIdx的列 */
){
  int i;
  const char *zColl = pIdx->azColl[iCol];

  for(i=0; i<pList->nExpr; i++){  /*for循环查找*/
    Expr *p = pList->a[i].pExpr;  /*定义列表中表达式*/
    if( p->op==TK_COLUMN    /*判定条件*/
     && p->iColumn==pIdx->aiColumn[iCol]
     && p->iTable==iBase
    ){
      CollSeq *pColl = sqlite3ExprCollSeq(pParse, p);  /*得到表达式的排序序列*/
      if( ALWAYS(pColl) && 0==sqlite3StrICmp(pColl->zName, zColl) ){   /*满足条件*/
        return i;  /*返回表达式在pList->a[]的索引*/
      }
    }
  }
  return -1;  /*没有找到*/
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
** 这个程序判定pIdx是否能用于处理DISTINCT标识符。
** 换句话说，它测试是否能将该索引用于外部循环使用，来保证pDistinct列表中的所有表达式中有相等值的行组合在一起传递（去重）。
**
** 如下查询
**   SELECT DISTINCT a, b, c FROM tbl WHERE a = ?
** 会受益于列"b"和"c"的任何索引。	
*/
static int isDistinctIndex(
  Parse *pParse,                  /* Parsing context  解析器上下文 */
  WhereClause *pWC,               /* The WHERE clause  WHERE子句 */
  Index *pIdx,                    /* The index being considered  被考虑的索引 */
  int base,                       /* Cursor number for the table pIdx is on  pIdx所在表的游标号 */
  ExprList *pDistinct,            /* The DISTINCT expressions  DISTINCT表达式列表 */
  int nEqCol                      /* Number of index columns with ==   存在==运算符的索引列的数目 */
){
  Bitmask mask = 0;               /* Mask of unaccounted for pDistinct exprs 未说明的pDistinct exprs的掩码 */
  int i;                          /* Iterator variable   迭代变量 */

  if( pIdx->zName==0 || pDistinct==0 || pDistinct->nExpr>=BMS ) return 0;  /*索引名字为0，无DISTINCT表达式，BMS表示位掩码大小*/
  testcase( pDistinct->nExpr==BMS-1 );  /*测试DISTINCT表达式列表中的表达式数目*/

  /* Loop through all the expressions in the distinct list. If any of them
  ** are not simple column references, return early. Otherwise, test if the
  ** WHERE clause contains a "col=X" clause. If it does, the expression
  ** can be ignored. If it does not, and the column does not belong to the
  ** same table as index pIdx, return early. Finally, if there is no
  ** matching "col=X" expression and the column is on the same table as pIdx,
  ** set the corresponding bit in variable mask.
  **
  ** 循环遍历DISTINCT表达式列表中的所有表达式。如果他们中的任意一个不是单一列引用，则提前返回。
  ** 否则，查看WHERE子句包含"col=X"语句。
  ** 如果包含，表达式被忽略。如果没有，并且作为索引pIdx的列不属于同一个表，提前返回。
  ** 最后，如果没有与"col=X"形式匹配的表达式，也没有作为索引pIdx的在同一个表中的列，则在变量位掩码中设置相应的位。
  */
  for(i=0; i<pDistinct->nExpr; i++){  /*循环遍历*/
    WhereTerm *pTerm;  
    Expr *p = pDistinct->a[i].pExpr;  /*定义列表中表达式*/
    if( p->op!=TK_COLUMN ) return 0;  /*运算符不符*/
    pTerm = findTerm(pWC, p->iTable, p->iColumn, ~(Bitmask)0, WO_EQ, 0);  /*在WHERE子句中查找该term，这个term由"X <op> <expr>"形式构成。*/
    if( pTerm ){  /*查找成功*/
      Expr *pX = pTerm->pExpr;  /*定义term表达式*/
	  /*返回指向排序序列的指针，该排序序列由一个二进制比较运算符使用，来比较pLeft和pRight。*/
      CollSeq *p1 = sqlite3BinaryCompareCollSeq(pParse, pX->pLeft, pX->pRight);  /*term的左右两边*/
      CollSeq *p2 = sqlite3ExprCollSeq(pParse, p);  /*返回表达式pExpr的排序序列*/
      if( p1==p2 ) continue;  /*DISTINCT表达式与whereterm的排序序列相同*/
    }
    if( p->iTable!=base ) return 0;  /* 表达式引用的表不是pIdx所在表*/
    mask |= (((Bitmask)1) << i);  /*按位或*/
  }

  for(i=nEqCol; mask && i<pIdx->nColumn; i++){
    int iExpr = findIndexCol(pParse, pDistinct, base, pIdx, i);  /*查找DISTINCT列表中的表达式，返回表达式在表中的索引*/
    if( iExpr<0 ) break;
    mask &= ~(((Bitmask)1) << iExpr);  /*按位与*/
  }

  return (mask==0);
}

/*
** Return true if the DISTINCT expression-list passed as the third argument
** is redundant. A DISTINCT list is redundant if the database contains a
** UNIQUE index that guarantees that the result of the query will be distinct
** anyway.
**
** 如果DISTINCT表达式列表作为第三个参数传递是多余的，返回true.
** 如果数据库包含一个UNIQUE索引保证查询的结果总是唯一的，则一个DISTINCT list是多余的，
*/
static int isDistinctRedundant(
  Parse *pParse,         /*解析器上下文*/
  SrcList *pTabList,     /*FROM子句中引用的表*/
  WhereClause *pWC,      /*WHERE子句*/
  ExprList *pDistinct    /*DISTINCT表达式列表*/
){
	Table *pTab;   /*声明表*/
    Index *pIdx;   /*声明索引*/ 
    int i;         /*循环计数器*/                       
    int iBase;     /*表的游标*/

    /* If there is more than one table or sub-select in the FROM clause of
    ** this query, then it will not be possible to show that the DISTINCT 
    ** clause is redundant.
    ** 
    ** 如果此次查询的FROM子句中有多个表或子查询，则不会显示DISTINCT子句是冗余的。
    */
    if( pTabList->nSrc!=1 ) return 0;  /*FROM子句中的表数*/
    iBase = pTabList->a[0].iCursor;  /*得到列表中第一个表的索引*/
    pTab = pTabList->a[0].pTab;   /*定义为第一个表*/

    /* If any of the expressions is an IPK column on table iBase, then return 
    ** true. Note: The (p->iTable==iBase) part of this test may be false if the
    ** current SELECT is a correlated sub-query.
    **
    ** 如果表iBase中任意一个表达式是一个IPK列，返回true.
    ** 注意:如果当前的SELECT是一个有相互关系的子查询，那么这个测试的(p->iTable==iBase)部分可能是错的。
    */
    for(i=0; i<pDistinct->nExpr; i++){
	    Expr *p = pDistinct->a[i].pExpr;  /*定义DISTINCT列表中表达式*/
	    if( p->op==TK_COLUMN && p->iTable==iBase && p->iColumn<0 ) return 1;  /*表达式引用的表与FROM子句中的表相同*/
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
    ** 循环遍历表中的所有索引，查看每个索引是否使DISTINCT限定符多余。
    ** 当且仅当满足如下条件:
    ** 	1.索引本身是UNIQUE索引；
    **	2.在索引中的所有列要么是pDistinct列表的一部分，要么是WHERE子句包含"col=X"形式的term
    **	  (其中X是一个常量)。比较关系的排序序列和select-list表达式必须与这些索引相匹配；
    **	3.不包含"col=X"term的WHERE子句的所有这些索引列属于一个NOT NULL约束。
    */
    for(pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext){  /*循环表中的索引*/
		if( pIdx->onError==OE_None ) continue;
        for(i=0; i<pIdx->nColumn; i++){  /*循环索引中的列*/
			int iCol = pIdx->aiColumn[i];
            if( 0==findTerm(pWC, iBase, iCol, ~(Bitmask)0, WO_EQ, pIdx) ){  /*在WHERE子句没有找到term*/
				int iIdxCol = findIndexCol(pParse, pDistinct, iBase, pIdx, i);  /*返回DISTINCT列表中符合条件表达式的索引*/
                if( iIdxCol<0 || pTab->aCol[pIdx->aiColumn[i]].notNull==0 ){  /*没有找到，或者FROM引用的表中列为空*/
					break;
                }
            }
        }
        if( i==pIdx->nColumn ){   /*索引中的列符合*/
           /* This index implies that the DISTINCT qualifier is redundant.
	       **	
	       ** 这个索引意味着DISTINCT限定符是冗余的。
	       */
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
** 这个程序判定pIdx索引是否能满足ORDER BY子句。如果能则返回1，如果不能则返回0。
**
** pOrderBy is an ORDER BY clause from a SELECT statement.  pTab is the
** left-most table in the FROM clause of that same SELECT statement and
** the table has a cursor number of "base".  pIdx is an index on pTab.
**
** pOrderBy是SELECT语句中的一个ORDER BY子句。pTab是同一个SELECT语句中的
** FROM子句中最左边的表，并且这个表有一个"base"游标号。pIdx是pTab上的一个索引。
**
** nEqCol is the number of columns of pIdx that are used as equality
** constraints.  Any of these columns may be missing from the ORDER BY
** clause and the match can still be a success.
**
** nEqCol是用于等式约束的pIdx的列的数目。任何这些列都可能在ORDER BY子句中丢失，但匹配依旧可以成功。
**
** All terms of the ORDER BY that match against the index must be either
** ASC or DESC.  (Terms of the ORDER BY clause past the end of a UNIQUE
** index do not need to satisfy this constraint.)  The *pbRev value is
** set to 1 if the ORDER BY clause is all DESC and it is set to 0 if
** the ORDER BY clause is all ASC.
**
** 与索引匹配的ORDER BY子句的所有terms必须是ASC or DESC.(ORDER BY子句的terms在UNIQUE索引之后，
** 不需要满足这个约束。)如果ORDER BY子句全是DESC，则*pbRev值设置为1，如果全是ASC则设为0。
*/
static int isSortingIndex(
  Parse *pParse,          /* Parsing context  分析上下文 */
  WhereMaskSet *pMaskSet, /* Mapping from table cursor numbers to bitmaps  表游标数到位图的映射 */
  Index *pIdx,            /* The index we are testing  将要测试的索引 */
  int base,               /* Cursor number for the table to be sorted  将要排序的表的游标号 */
  ExprList *pOrderBy,     /* The ORDER BY clause   ORDER BY子句 */
  int nEqCol,             /* Number of index columns with == constraints   ==约束的索引列数 */
  int wsFlags,            /* Index usages flags  索引使用标志 */
  int *pbRev              /* Set to 1 if ORDER BY is DESC   如果ORDER BY是DESC则设为1 */
){
  int i, j;                       /* Loop counters  循环计数器 */
  int sortOrder = 0;              /* XOR of index and ORDER BY sort direction  索引的XOR和ORDER BY排序方向 */
  int nTerm;                      /* Number of ORDER BY terms   ORDER BY子句的terms数 */
  struct ExprList_item *pTerm;    /* A term of the ORDER BY clause   ORDER BY子句的一个term */
  sqlite3 *db = pParse->db;  

  if( !pOrderBy ) return 0;  /*没有ORDER BY子句*/
  if( wsFlags & WHERE_COLUMN_IN ) return 0;  /*WHERE_COLUMN_IN标志查询策略不符，应该WHERE_ORDERBY*/
  if( pIdx->bUnordered ) return 0;

  nTerm = pOrderBy->nExpr;   /*得到ORDER BY子句的terms数*/
  assert( nTerm>0 );  /*判定存在term*/

  /* Argument pIdx must either point to a 'real' named index structure, 
  ** or an index structure allocated on the stack by bestBtreeIndex() to
  ** represent the rowid index that is part of every table. 
  **
  ** 参数pIdx必须指向一个“real”命名的索引结构，或者指向在堆栈中由bestBtreeIndex()分配的一个索引数据结构，
  ** 来表示每个表中的rowid索引部分。
  */
  /*zName表示索引的名字，nColumn表示该索引使用的表的列数，aiColumn表示该索引使用的列，0是第一个*/
  assert( pIdx->zName || (pIdx->nColumn==1 && pIdx->aiColumn[0]==-1) );  

  /* Match terms of the ORDER BY clause against columns of
  ** the index.
  **
  ** 匹配ORDER BY子句中的terms对应的索引的列。
  **
  ** Note that indices have pIdx->nColumn regular columns plus
  ** one additional column containing the rowid.  The rowid column
  ** of the index is also allowed to match against the ORDER BY
  ** clause.
  **
  ** 注意:索引有pIdx->nColumn常规列，另加一个包含rowid的列。
  ** 索引的这个rowid列也能够匹配ORDER BY子句。
  */
  for(i=j=0, pTerm=pOrderBy->a; j<nTerm && i<=pIdx->nColumn; i++){  /*j记录ORDER BY子句的term数，i记录索引的列数*/
    Expr *pExpr;        /* The expression of the ORDER BY pTerm   ORDER BY子句pTerm的表达式 */
    CollSeq *pColl;     /* The collating sequence of pExpr   pExpr的排序序列 */
    int termSortOrder;  /* Sort order for this term   这个term的排序次序 */
    int iColumn;        /* The i-th column of the index. -1 for rowid   索引的第i列，-1表示rowid列 */
    int iSortOrder;     /* 1 for DESC, 0 for ASC on the i-th index term   第i个索引term中，0表示ASC，1表示DESC */
    const char *zColl;  /* Name of the collating sequence for i-th index term  第i个索引term的排序序列的名字 */

    pExpr = pTerm->pExpr;  /*得到ORDER BY 子句中的term表达式*/
    if( pExpr->op!=TK_COLUMN || pExpr->iTable!=base ){
      /* Can not use an index sort on anything that is not a column in the
      ** left-most table of the FROM clause.
	  **	
	  ** 如果不是FROM子句中最左边的表的一个列，则不能使用索引排序。
	  */
      break;
    }
    pColl = sqlite3ExprCollSeq(pParse, pExpr);  /*得到pExpr的排序序列*/
    if( !pColl ){  
      pColl = db->pDfltColl;  /*数据库默认排序顺序*/
    }
    if( pIdx->zName && i<pIdx->nColumn ){ 
      iColumn = pIdx->aiColumn[i];  /*索引的第i列*/
      if( iColumn==pIdx->pTable->iPKey ){  /*Table.iPKey表示主键所在列的索引*/
        iColumn = -1;
      }
      iSortOrder = pIdx->aSortOrder[i];  /*得到第i个索引term的排序序列*/
      zColl = pIdx->azColl[i];  /*得到第i个索引term的排序序列的名字*/
    }else{ 
      iColumn = -1;
      iSortOrder = 0;  /*默认排序序列，0表示ASC*/
      zColl = pColl->zName;  /*默认序列的名字*/
    }
	/*sqlite3StrICmp宏比较两个字符串，若参数s1 和s2 字符串相同则返回0。s1 若大于s2 则返回大于0 的值。s1 若小于s2 则返回小于0 的值*/
    if( pExpr->iColumn!=iColumn || sqlite3StrICmp(pColl->zName, zColl) ){
      /* Term j of the ORDER BY clause does not match column i of the index 
      ** 
	  ** ORDER BY子句中的term j与索引中的i列不匹配。
	  */
      if( i<nEqCol ){
        /* If an index column that is constrained by == fails to match an
        ** ORDER BY term, that is OK.  Just ignore that column of the index.
		**  
        ** 如果一个由==运算符约束的索引列不能匹配ORDER BY子句的一个term，是可以的。只要忽略这个索引列。
        */
        continue;
      }else if( i==pIdx->nColumn ){
        /* Index column i is the rowid.  All other terms match. 
		** 
		** 索引列i是rowid，所有其他terms匹配。
		*/
        break;
      }else{
        /* If an index column fails to match and is not constrained by ==
        ** then the index cannot satisfy the ORDER BY constraint.
        ** 如果一个索引列不能匹配，且不是由==运算符约束的，那么索引不能满足ORDER BY约束条件。
        */
        return 0;
      }
    }
	/*aSortOrder表示Index.nColumn大小的数组，True==DESC, False==ASC*/
    assert( pIdx->aSortOrder!=0 || iColumn==-1 );
    assert( pTerm->sortOrder==0 || pTerm->sortOrder==1 );  /*ORDER BY子句的term排序序列，ASC或DESC*/
    assert( iSortOrder==0 || iSortOrder==1 );  /*第i个索引term的排序序列，ASC或DESC*/
    termSortOrder = iSortOrder ^ pTerm->sortOrder;  /*^异或运算符*/
    if( i>nEqCol ){
      if( termSortOrder!=sortOrder ){
        /* Indices can only be used if all ORDER BY terms past the 
        ** equality constraints are all either DESC or ASC. 
		** 
		** 只有满足等式约束的所有ORDER BY terms都是DESC或ASC，才可以使用索引。  
		*/
        return 0;
      }
    }else{
      sortOrder = termSortOrder;  /*得到ORDER BY子句的排序序列*/
    }
    j++;  /*ORDER BY子句term数+1*/
    pTerm++;  /*ORDER BY 子句下一个term*/
    if( iColumn<0 && !referencesOtherTables(pOrderBy, pMaskSet, j, base) ){
      /* If the indexed column is the primary key and everything matches
      ** so far and none of the ORDER BY terms to the right reference other
      ** tables in the join, then we are assured that the index can be used 
      ** to sort because the primary key is unique and so none of the other
      ** columns will make any difference.
      **
	  ** 如果索引列是主键，并且到目前为止所有项都匹配，并且没有ORDER BY terms与连接的其他表右相关，
	  ** 那么可以保证这个索引可以用来排序，因为主键是唯一的，而且没有其他的列会产生影响。
	  */
      j = nTerm;
    }
  }

  *pbRev = sortOrder!=0;  /*如果ORDER BY是DESC则设为1*/
  if( j>=nTerm ){
    /* All terms of the ORDER BY clause are covered by this index so 
	** this index can be used for sorting.
	**   
	** ORDER BY子句的所有terms被这个索引覆盖，所以这个索引可以用于排序。  
	*/  
    return 1;
  }
  if( pIdx->onError!=OE_None && i==pIdx->nColumn
      && (wsFlags & WHERE_COLUMN_NULL)==0
      && !referencesOtherTables(pOrderBy, pMaskSet, j, base) 
  ){
    Column *aCol = pIdx->pTable->aCol;  /*列信息*/

    /* All terms of this index match some prefix of the ORDER BY clause,
    ** the index is UNIQUE, and no terms on the tail of the ORDER BY refer to
    ** other tables in a join. So, assuming that the index entries visited
    ** contain no NULL values, then this index delivers rows in the required order.
    **
    ** 这个索引的所有terms与ORDER BY子句的一些前缀相匹配，索引是UNIQUE,
    ** 并且没有ORDER BY尾部的terms与连接中的其他表相关。
    ** 所以，假定访问的索引项包含no NULL值，那么这个索引在所需的顺序中提供rows。
    **
    ** It is not possible for any of the first nEqCol index fields to be
    ** NULL (since the corresponding "=" operator in the WHERE clause would 
    ** not be true). So if all remaining index columns have NOT NULL 
    ** constaints attached to them, we can be confident that the visited
    ** index entries are free of NULLs. 
	** 
    ** 任何的第一个nEqCol索引字段不可能为NULL(因为在WHERE子句中相应的"="运算符不会正确)。
    ** 所以，如果所有存在的索引列有NOT NULL约束，我们可以确信访问的索引项是没有NULL值的。
    */
    for(i=nEqCol; i<pIdx->nColumn; i++){
      if( aCol[pIdx->aiColumn[i]].notNull==0 ) break;  /*nEqCol索引字段为空*/
    }
    return (i==pIdx->nColumn);  /*索引的列是否全部是==约束的列 */ 
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
** 粗略估计一个输入值的对数，结果不需要精确。只是用于评估存在O(logN)或O(NlogN)复杂性的
** 执行操作的总代价。因为N只是一个猜测值，即使logN有些误差也问题不大。
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
** 两个程序用于输出一个sqlite3_index_info数据结构的内容。只是用于测试和调试。
** 如果SQLITE_TEST和SQLITE_DEBUG都没有定义，那么这些程序是no-ops(无操作)。
*/
#if !defined(SQLITE_OMIT_VIRTUALTABLE) && defined(SQLITE_DEBUG)  /*定义了SQLITE_DEBUG*/
static void TRACE_IDX_INPUTS(sqlite3_index_info *p){
  int i;
  if( !sqlite3WhereTrace ) return;  /*跟踪输出宏,用于测试和调试*/
  for(i=0; i<p->nConstraint; i++){  /*索引的约束数内循环*/
    sqlite3DebugPrintf("  constraint[%d]: col=%d termid=%d op=%d usabled=%d\n",
       i,
       p->aConstraint[i].iColumn,
       p->aConstraint[i].iTermOffset,
       p->aConstraint[i].op,
       p->aConstraint[i].usable);
  }
  for(i=0; i<p->nOrderBy; i++){  /*索引的ORDER BY子句数*/
    sqlite3DebugPrintf("  orderby[%d]: col=%d desc=%d\n",
       i,
       p->aOrderBy[i].iColumn,
       p->aOrderBy[i].desc);
  }
}

static void TRACE_IDX_OUTPUTS(sqlite3_index_info *p){
  int i;
  if( !sqlite3WhereTrace ) return;  /*跟踪输出宏,用于测试和调试*/
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
** bestIndex()被bestOrClauseIndex()调用而被请求
*/
static void bestIndex(
    Parse*, WhereClause*, struct SrcList_item*,
    Bitmask, Bitmask, ExprList*, WhereCost*);

/*
** This routine attempts to find an scanning strategy that can be used 
** to optimize an 'OR' expression that is part of a WHERE clause. 
**
** 这个程序查找一个扫描策略，用于优化WHERE子句的“OR”表达式
**
** The table associated with FROM clause term pSrc may be either a
** regular B-Tree table or a virtual table.
**
** 与FROM子句term pSrc相关联的表可能是一个一般的B-Tree表，或者是一个虚拟表。
*/
static void bestOrClauseIndex(
  Parse *pParse,              /* The parsing context  解析上下文 */
  WhereClause *pWC,           /* The WHERE clause  WHERE子句 */
  struct SrcList_item *pSrc,  /* The FROM clause term to search  要搜索的FROM子句term */
  Bitmask notReady,           /* Mask of cursors not available for indexing   不用于索引的游标的掩码 */
  Bitmask notValid,           /* Cursors not available for any purpose  任何用途下都无效的游标 */
  ExprList *pOrderBy,         /* The ORDER BY clause   ORDER BY子句 */
  WhereCost *pCost            /* Lowest cost query plan   最小代价的查询计划 */
){
#ifndef SQLITE_OMIT_OR_OPTIMIZATION
  const int iCur = pSrc->iCursor;   /* The cursor of the table to be accessed  需要存取的表的游标 */
  const Bitmask maskSrc = getMask(pWC->pMaskSet, iCur);  /* Bitmask for pSrc   pSrc的位掩码 */
  WhereTerm * const pWCEnd = &pWC->a[pWC->nTerm];        /* End of pWC->a[]   pWC->a[]的末尾 */
  WhereTerm *pTerm;                 /* A single term of the WHERE clause   WHERE子句的一个单独term */

  /* The OR-clause optimization is disallowed if the INDEXED BY or
  ** NOT INDEXED clauses are used or if the WHERE_AND_ONLY bit is set.
  **
  ** 如果使用了INDEXED BY子句或NOT INDEXED子句，或者设置了WHERE_AND_ONLY bit，那么OR子句是不允许优化的
  */
  if( pSrc->notIndexed || pSrc->pIndex!=0 ){
    return;
  }
  if( pWC->wctrlFlags & WHERE_AND_ONLY ){
    return;
  }

  /* Search the WHERE clause terms for a usable WO_OR term. 
  ** 
  ** 为一个可用的WO_OR term查找WHERE子句terms 
  */
  for(pTerm=pWC->a; pTerm<pWCEnd; pTerm++){  /*循环遍历WHERE子句的所有terms*/
    if( pTerm->eOperator==WO_OR 
     && ((pTerm->prereqAll & ~maskSrc) & notReady)==0
     && (pTerm->u.pOrInfo->indexable & maskSrc)!=0 
    ){
      WhereClause * const pOrWC = &pTerm->u.pOrInfo->wc;   /*OR运算符子句*/
      WhereTerm * const pOrWCEnd = &pOrWC->a[pOrWC->nTerm];  /*OR运算符子句的末尾*/
      WhereTerm *pOrTerm;  /*OR运算符子句的term*/
      int flags = WHERE_MULTI_OR;
      double rTotal = 0;  /*OR运算符子句的总代价*/
      double nRow = 0;
      Bitmask used = 0;   /*这个策略使用的游标的位掩码*/

      for(pOrTerm=pOrWC->a; pOrTerm<pOrWCEnd; pOrTerm++){  /*循环遍历OR运算符子句的所有terms*/
        WhereCost sTermCost;  /*声明term所需代价*/
        WHERETRACE(("... Multi-index OR testing for term %d of %d....\n", 
          (pOrTerm - pOrWC->a), (pTerm - pWC->a)
        ));   /*跟踪term*/
        if( pOrTerm->eOperator==WO_AND ){  /*OR运算符term中含有AND运算符*/
          WhereClause *pAndWC = &pOrTerm->u.pAndInfo->wc;  /*定义一个AND运算符子句*/
          bestIndex(pParse, pAndWC, pSrc, notReady, notValid, 0, &sTermCost);  /*得到pAndWC的扫描策略（？）*/
        }else if( pOrTerm->leftCursor==iCur ){   /*OR运算符term引用的表*/
          WhereClause tempWC;
          tempWC.pParse = pWC->pParse;
          tempWC.pMaskSet = pWC->pMaskSet;
          tempWC.pOuter = pWC;
          tempWC.op = TK_AND;
          tempWC.a = pOrTerm;
          tempWC.wctrlFlags = 0;
          tempWC.nTerm = 1;
          bestIndex(pParse, &tempWC, pSrc, notReady, notValid, 0, &sTermCost);  /*得到当前子句的扫描策略（？）*/
        }else{
          continue;
        }
        rTotal += sTermCost.rCost;   /*rCost表示执行该查询策略的成本*/
        nRow += sTermCost.plan.nRow;
        used |= sTermCost.used;
        if( rTotal>=pCost->rCost ) break;
      }

      /* If there is an ORDER BY clause, increase the scan cost to account 
      ** for the cost of the sort. 
	  **
	  ** 如果存在一个ORDER BY子句，则为此次排序的代价计算增加一次扫描代价。
	  */
      if( pOrderBy!=0 ){
        WHERETRACE(("... sorting increases OR cost %.9g to %.9g\n",
                    rTotal, rTotal+nRow*estLog(nRow)));
        rTotal += nRow*estLog(nRow);
      }

      /* If the cost of scanning using this OR term for optimization is
      ** less than the current cost stored in pCost, replace the contents of pCost.
      ** 
	  ** 如果用于优化该OR term的扫描代价小于pCost的当前值，则替换pCost的值。
	  */
      WHERETRACE(("... multi-index OR cost=%.9g nrow=%.9g\n", rTotal, nRow));
      if( rTotal<pCost->rCost ){   /*得到最小代价扫描策略*/
        pCost->rCost = rTotal;
        pCost->used = used;
        pCost->plan.nRow = nRow;
        pCost->plan.wsFlags = flags;
        pCost->plan.u.pTerm = pTerm;
      }
    }
  }
#endif /* SQLITE_OMIT_OR_OPTIMIZATION*/
}

#ifndef SQLITE_OMIT_AUTOMATIC_INDEX
/*
** Return TRUE if the WHERE clause term pTerm is of a form where it could be
** used with an index to access pSrc, assuming an appropriate index existed.
** 
** 如果WHERE子句term pTerm是这样一种形式，即可以使用一个索引来访问pSrc(FROM子句term)，
** 假设存在一个合适的索引，则返回TRUE。 
*/
static int termCanDriveIndex(
  WhereTerm *pTerm,              /* WHERE clause term to check   用于检测的WHERE子句 */
  struct SrcList_item *pSrc,     /* Table we are trying to access   将要访问的表 */
  Bitmask notReady               /* Tables in outer loops of the join  连接的外部循环中的表 */
){
  char aff;
  if( pTerm->leftCursor!=pSrc->iCursor ) return 0;   /*FROM子句中的表与pTerm的左边引用的表不符*/
  if( pTerm->eOperator!=WO_EQ ) return 0;  /*pTerm的运算符不符*/
  if( (pTerm->prereqRight & notReady)!=0 ) return 0;   /*pTerm的右边引用的表与连接的外部循环中的表不符*/
  aff = pSrc->pTab->aCol[pTerm->u.leftColumn].affinity;   /*FROM子句中的表的列*/
  if( !sqlite3IndexAffinityOk(pTerm->pExpr, aff) ) return 0;  /*与要分析的term引用的表的列没有关联性*/
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
** 如果pCost指定的pSrc的查询计划是一个全表扫描并且可以使用索引(如果没有INDEXED子句)，
** 如果它能创建一个临时索引，即使把创建该索引的代价也考虑进去，依旧比全表扫描更好，
** 那么改变查询计划，使用临时索引。
*/
static void bestAutomaticIndex(
  Parse *pParse,              /* The parsing context   解析上下文 */
  WhereClause *pWC,           /* The WHERE clause   WHERE子句 */
  struct SrcList_item *pSrc,  /* The FROM clause term to search   用于查询的FROM子句term */
  Bitmask notReady,           /* Mask of cursors that are not available   游标的掩码是无效的 */
  WhereCost *pCost            /* Lowest cost query plan    最小代价查询计划 */
){
  double nTableRow;           /* Rows in the input table   输入表中的行 */
  double logN;                /* log(nTableRow)   行数的对数*/
  double costTempIdx;         /* per-query cost of the transient index   临时索引的per-query代价 */
  WhereTerm *pTerm;           /* A single term of the WHERE clause   WHERE子句的一个单独term */
  WhereTerm *pWCEnd;          /* End of pWC->a[]   WHERE子句pWC->a[]的末尾 */
  Table *pTable;              /* Table tht might be indexed   可能有索引的表tht */

  if( pParse->nQueryLoop<=(double)1 ){
    /* There is no point in building an automatic index for a single scan. 
	** 为一个单一的扫描构建一个自动索引没有意义。 
	*/
    return;
  }
  if( (pParse->db->flags & SQLITE_AutoIndex)==0 ){
    /* Automatic indices are disabled at run-time. 
	** 运行时自动索引不可用。 
	*/
    return;
  }
  if( (pCost->plan.wsFlags & WHERE_NOT_FULLSCAN)!=0 ){
    /* We already have some kind of index in use for this query. 
	** 已经有用于这个查询的索引 
	*/
    return;
  }
  if( pSrc->notIndexed ){
    /* The NOT INDEXED clause appears in the SQL. 
	** 在SQL语句中存在NOT INDEXED子句 
	*/
    return;
  }
  if( pSrc->isCorrelated ){
    /* The source is a correlated sub-query. No point in indexing it. 
	** 来源是一个相关的子查询。不需要使用索引 */
    return;
  }

  assert( pParse->nQueryLoop >= (double)1 );
  pTable = pSrc->pTab;   /*FROM子句中的表*/
  nTableRow = pTable->nRowEst;  /*表中的行数*/
  logN = estLog(nTableRow);   /*评价执行复杂度*/
  costTempIdx = 2*logN*(nTableRow/pParse->nQueryLoop + 1);   /*临时索引的代价*/
  if( costTempIdx>=pCost->rCost ){   
    /* The cost of creating the transient table would be greater than 
    ** doing the full table scan. 
	** 创建临时表的代价大于全表扫描的代价。
	*/
    return;
  }

  /* Search for any equality comparison term 
  ** 搜索任意的等值比较项。
  */
  pWCEnd = &pWC->a[pWC->nTerm];
  for(pTerm=pWC->a; pTerm<pWCEnd; pTerm++){   /*循环遍历where子句中的每个term*/
    if( termCanDriveIndex(pTerm, pSrc, notReady) ){	  /*如果term可以使用索引*/
      WHERETRACE(("auto-index reduces cost from %.1f to %.1f\n",
                    pCost->rCost, costTempIdx));
      pCost->rCost = costTempIdx;   /*改变查询计划，更小代价*/
      pCost->plan.nRow = logN + 1;
      pCost->plan.wsFlags = WHERE_TEMP_INDEX;
      pCost->used = pTerm->prereqRight;
      break;
    }
  }
}
#else
# define bestAutomaticIndex(A,B,C,D,E)  /* no-op  空操作*/
#endif  /* SQLITE_OMIT_AUTOMATIC_INDEX结束。*/


#ifndef SQLITE_OMIT_AUTOMATIC_INDEX
/*
** Generate code to construct the Index object for an automatic index
** and to set up the WhereLevel object pLevel so that the code generator
** makes use of the automatic index.
**
** 生成代码创建自动索引的索引对象，并设置WhereLevel对象pLevel，以便代码生成器可以使用这个自动索引。
*/
static void constructAutomaticIndex(
  Parse *pParse,              /* The parsing context  解析上下文 */
  WhereClause *pWC,           /* The WHERE clause   WHERE子句 */
  struct SrcList_item *pSrc,  /* The FROM clause term to get the next index   为得到下一个索引的FROM子句term*/
  Bitmask notReady,           /* Mask of cursors that are not available   游标的掩码是无效的 */
  WhereLevel *pLevel          /* Write new index here  在此处写入新的索引 */
){
  int nColumn;                /* Number of columns in the constructed index   在构造的索引中的列数 */
  WhereTerm *pTerm;           /* A single term of the WHERE clause  WHERE子句的一个单一term */
  WhereTerm *pWCEnd;          /* End of pWC->a[]   WHERE子句pWC->a[]的末尾 */
  int nByte;                  /* Byte of memory needed for pIdx   pIdx需要的内存字节 */
  Index *pIdx;                /* Object describing the transient index   描述临时索引的对象 */
  Vdbe *v;                    /* Prepared statement under construction   构造的预处理语句 */
  int addrInit;               /* Address of the initialization bypass jump  初始化跳转地址*/
  Table *pTable;              /* The table being indexed   有索引的表 */
  KeyInfo *pKeyinfo;          /* Key information for the index   索引的关键信息 */   
  int addrTop;                /* Top of the index fill loop   填充循环的索引顶部 */
  int regRecord;              /* Register holding an index record   记录保存一个索引记录 */
  int n;                      /* Column counter   列计数器 */
  int i;                      /* Loop counter   循环计数器 */
  int mxBitCol;               /* Maximum column in pSrc->colUsed   在pSrc->colUsed中的最大列 */
  CollSeq *pColl;             /* Collating sequence to on a column   一个列的排序序列 */
  Bitmask idxCols;            /* Bitmap of columns used for indexing   用于索引的列的位掩码 */
  Bitmask extraCols;          /* Bitmap of additional columns   附加列的位掩码 */

  /* Generate code to skip over the creation and initialization of the
  ** transient index on 2nd and subsequent iterations of the loop. 
  ** 
  ** 生成代码用于跳过循环的2nd和后续迭代中的临时索引的创建和初始化。
  */
  v = pParse->pVdbe;    
  assert( v!=0 );
  addrInit = sqlite3CodeOnce(pParse);  /*编码一个OP_Once指令，返回该指令的地址*/

  /* Count the number of columns that will be added to the index 
  ** and used to match WHERE clause constraints。
  **
  ** 计算将要添加到索引的列数，用于匹配WHERE子句的约束。 
  */
  nColumn = 0;
  pTable = pSrc->pTab;  /*FROM子句中的表*/
  pWCEnd = &pWC->a[pWC->nTerm];   /*WHERE子句的末尾term*/
  idxCols = 0;   
  for(pTerm=pWC->a; pTerm<pWCEnd; pTerm++){   /*循环遍历WHERE子句的每个term*/
    if( termCanDriveIndex(pTerm, pSrc, notReady) ){  /*pTerm可以使用索引访问FROM子句的表*/
      int iCol = pTerm->u.leftColumn;   /*pTerm运算符左边的列*/  
      Bitmask cMask = iCol>=BMS ? ((Bitmask)1)<<(BMS-1) : ((Bitmask)1)<<iCol;  /*BMS表示位掩码的bit数*/
      testcase( iCol==BMS );  /*测试*/
      testcase( iCol==BMS-1 );
      if( (idxCols & cMask)==0 ){  /*?*/
        nColumn++;
        idxCols |= cMask;
      }
    }
  }
  assert( nColumn>0 );
  pLevel->plan.nEq = nColumn;  /*定义新建索引的查询策略中==约束的数目，即需要添加的列数*/

  /* Count the number of additional columns needed to create a
  ** covering index.  A "covering index" is an index that contains all
  ** columns that are needed by the query.  With a covering index, the
  ** original table never needs to be accessed.  Automatic indices must
  ** be a covering index because the index will not be updated if the
  ** original table changes and the index and table cannot both be used
  ** if they go out of sync.
  **
  ** 计算需要创建一个覆盖索引的附加列数。一个"覆盖索引"包含查询中的所有列。
  ** 有了一个覆盖索引，原始表将不再被访问。自动索引必须是一个覆盖索引，因为如果原始表改变，
  ** 索引不会更新，并且如果这个索引和表不同步的话，它们不会全被使用。
  */
  extraCols = pSrc->colUsed & (~idxCols | (((Bitmask)1)<<(BMS-1)));  /*定义附加列，colUsed表示FROM中的列被使用*/
  mxBitCol = (pTable->nCol >= BMS-1) ? BMS-1 : pTable->nCol;  /*定义pSrc->colUsed中的最大列*/
  testcase( pTable->nCol==BMS-1 );
  testcase( pTable->nCol==BMS-2 );
  for(i=0; i<mxBitCol; i++){
    if( extraCols & (((Bitmask)1)<<i) ) nColumn++;
  }
  if( pSrc->colUsed & (((Bitmask)1)<<(BMS-1)) ){  
    nColumn += pTable->nCol - BMS + 1;
  }
  pLevel->plan.wsFlags |= WHERE_COLUMN_EQ | WHERE_IDX_ONLY | WO_EQ;  /*定义新建索引的标志*/

  /* Construct the Index object to describe this index 
  ** 构建索引对象来描述这个索引。
  */
  nByte = sizeof(Index);   /*分配内存字节*/
  nByte += nColumn*sizeof(int);     /* Index.aiColumn   索引使用的列*/
  nByte += nColumn*sizeof(char*);   /* Index.azColl   索引的排序序列名称数组*/
  nByte += nColumn;                 /* Index.aSortOrder   索引的列数大小排序数组*/
  pIdx = sqlite3DbMallocZero(pParse->db, nByte);  /*内存分配*/
  if( pIdx==0 ) return;  /*分配失败*/
  pLevel->plan.u.pIdx = pIdx;   /*定义新建索引的内存地址*/
  pIdx->azColl = (char**)&pIdx[1];  /*设置索引各项*/
  pIdx->aiColumn = (int*)&pIdx->azColl[nColumn];
  pIdx->aSortOrder = (u8*)&pIdx->aiColumn[nColumn];
  pIdx->zName = "auto-index";
  pIdx->nColumn = nColumn;
  pIdx->pTable = pTable;
  n = 0;
  idxCols = 0;
  for(pTerm=pWC->a; pTerm<pWCEnd; pTerm++){  /*遍历循环WHERE子句的所有terms*/
    if( termCanDriveIndex(pTerm, pSrc, notReady) ){  /*pTerm可以使用索引访问FROM子句的表*/
      int iCol = pTerm->u.leftColumn;  /*pTerm运算符左边的列*/ 
      Bitmask cMask = iCol>=BMS ? ((Bitmask)1)<<(BMS-1) : ((Bitmask)1)<<iCol;
      if( (idxCols & cMask)==0 ){
        Expr *pX = pTerm->pExpr;  /*pTerm的表达式*/
        idxCols |= cMask;  /*按位或*/
        pIdx->aiColumn[n] = pTerm->u.leftColumn;  /*定义索引使用的列*/
        pColl = sqlite3BinaryCompareCollSeq(pParse, pX->pLeft, pX->pRight);  /*表达式两边的排序序列，比较pLeft和pRight*/
        pIdx->azColl[n] = ALWAYS(pColl) ? pColl->zName : "BINARY";  /*新建索引的排序序列名称数组*/
        n++;  /*记录用于索引的列数*/
      }
    }
  }
  assert( (u32)n==pLevel->plan.nEq );  /*判定索引的列*/

  /* Add additional columns needed to make the automatic index into 
  ** a covering index 
  ** 添加需要的附加列，使自动索引变为覆盖索引。
  */
  for(i=0; i<mxBitCol; i++){
    if( extraCols & (((Bitmask)1)<<i) ){
      pIdx->aiColumn[n] = i;  /*定义索引的附加咧*/
      pIdx->azColl[n] = "BINARY";  /*索引的排序序列名称数组*/
      n++;
    }
  }
  if( pSrc->colUsed & (((Bitmask)1)<<(BMS-1)) ){  /**FROM子句中的列*/
    for(i=BMS-1; i<pTable->nCol; i++){
      pIdx->aiColumn[n] = i;
      pIdx->azColl[n] = "BINARY";
      n++;
    }
  }
  assert( n==nColumn );  /*判定是否是该索引使用的列数*/

  /* Create the automatic index 
  ** 创建自动索引 
  */
  pKeyinfo = sqlite3IndexKeyinfo(pParse, pIdx);  /*记录索引的关键信息*/
  assert( pLevel->iIdxCur>=0 );
  sqlite3VdbeAddOp4(v, OP_OpenAutoindex, pLevel->iIdxCur, nColumn+1, 0,
                    (char*)pKeyinfo, P4_KEYINFO_HANDOFF);   /*添加索引*/
  VdbeComment((v, "for %s", pTable->zName));  /*该索引所属的表*/

  /* Fill the automatic index with content 
  ** 填充自动索引的内容。 
  */
  addrTop = sqlite3VdbeAddOp1(v, OP_Rewind, pLevel->iTabCur);
  regRecord = sqlite3GetTempReg(pParse);
  sqlite3GenerateIndexKey(pParse, pIdx, pLevel->iTabCur, regRecord, 1);
  sqlite3VdbeAddOp2(v, OP_IdxInsert, pLevel->iIdxCur, regRecord);
  sqlite3VdbeChangeP5(v, OPFLAG_USESEEKRESULT);
  sqlite3VdbeAddOp2(v, OP_Next, pLevel->iTabCur, addrTop+1);
  sqlite3VdbeChangeP5(v, SQLITE_STMTSTATUS_AUTOINDEX);
  sqlite3VdbeJumpHere(v, addrTop);
  sqlite3ReleaseTempReg(pParse, regRecord);
  
  /* Jump here when skipping the initialization 
  ** 跳过初始化时则跳到此处
  */
  sqlite3VdbeJumpHere(v, addrInit);
}
#endif  /* SQLITE_OMIT_AUTOMATIC_INDEX结束*/

#ifndef SQLITE_OMIT_VIRTUALTABLE
/*
** Allocate and populate an sqlite3_index_info structure. It is the 
** responsibility of the caller to eventually release the structure
** by passing the pointer returned by this function to sqlite3_free().
**
** 分配并填充一个sqlite3_index_info数据结构。
** 调用者的任务是通过函数sqlite3_free()返回的指针最终释放这个结构。
*/
static sqlite3_index_info *allocateIndexInfo(
  Parse *pParse,                 /* The parsing context  解析上下文 */
  WhereClause *pWC,              /* The WHERE clause  WHERE子句 */ 
  struct SrcList_item *pSrc,     /* The FROM clause term to search  要搜索的FROM子句term */
  ExprList *pOrderBy             /* The ORDER BY clause   ORDER BY子句 */
){
  int i, j;
  int nTerm;
  struct sqlite3_index_constraint *pIdxCons;  /*约束索引结构*/
  struct sqlite3_index_orderby *pIdxOrderBy;  /*ORDER BY子句索引*/
  struct sqlite3_index_constraint_usage *pUsage;  /*约束使用索引*/
  WhereTerm *pTerm;   
  int nOrderBy;  /*记录ORDER BY 子句的列数*/
  sqlite3_index_info *pIdxInfo;  /*声明该结构对象*/

  WHERETRACE(("Recomputing index info for %s...\n", pSrc->pTab->zName));  /*索引信息集中到FROM子句中的表*/

  /* Count the number of possible WHERE clause constraints referring 
  ** to this virtual table. 
  ** 计算指向这个虚表的可能的WHERE子句约束的数量。
  */
  for(i=nTerm=0, pTerm=pWC->a; i<pWC->nTerm; i++, pTerm++){   /*循环遍历WHERE子句的所有terms*/
    if( pTerm->leftCursor != pSrc->iCursor ) continue;   /*pTerm的左边的表与FROM子句中的表不一致*/
    assert( (pTerm->eOperator&(pTerm->eOperator-1))==0 );  
    testcase( pTerm->eOperator==WO_IN );  /*测试pTerm的操作符*/
    testcase( pTerm->eOperator==WO_ISNULL );
    if( pTerm->eOperator & (WO_IN|WO_ISNULL) ) continue;  /*运算符符合*/
    if( pTerm->wtFlags & TERM_VNULL ) continue;  /*TERM_VNULL标志表示 生成 x>NULL 或 x<=NULL 的term*/
    nTerm++;
  }

  /* If the ORDER BY clause contains only columns in the current virtual table
  ** then allocate space for the aOrderBy part of the sqlite3_index_info structure.
  ** 
  ** 如果ORDER BY子句只包含当前虚表的列，那么为sqlite3_index_info结构中的aOrderby部分分配空间。
  */
  nOrderBy = 0;
  if( pOrderBy ){
    for(i=0; i<pOrderBy->nExpr; i++){  /*循环遍历ORDER BY子句的所有表达式*/
      Expr *pExpr = pOrderBy->a[i].pExpr;
      if( pExpr->op!=TK_COLUMN || pExpr->iTable!=pSrc->iCursor ) break;
    }
    if( i==pOrderBy->nExpr ){  /*ORDER BY子句只包含当前表的列*/
      nOrderBy = pOrderBy->nExpr;  
    }
  }

  /* Allocate the sqlite3_index_info structure.
  ** 分配sqlite3_index_info结构内存。
  */
  pIdxInfo = sqlite3DbMallocZero(pParse->db, sizeof(*pIdxInfo)
                           + (sizeof(*pIdxCons) + sizeof(*pUsage))*nTerm
                           + sizeof(*pIdxOrderBy)*nOrderBy );
  if( pIdxInfo==0 ){   /*分配失败*/
    sqlite3ErrorMsg(pParse, "out of memory");
    /* (double)0 In case of SQLITE_OMIT_FLOATING_POINT... */
    return 0;
  }

  /* Initialize the structure. The sqlite3_index_info structure contains
  ** many fields that are declared "const" to prevent xBestIndex from
  ** changing them.  We have to do some funky casting in order to
  ** initialize those fields.
  **
  ** 初始化该结构。sqlite3_index_info结构包含许多声明为“const”的字段，以防止
  ** xBestIndex改变它们。我们必须做一些操作来初始化这些字段。
  */
  pIdxCons = (struct sqlite3_index_constraint*)&pIdxInfo[1];
  pIdxOrderBy = (struct sqlite3_index_orderby*)&pIdxCons[nTerm];
  pUsage = (struct sqlite3_index_constraint_usage*)&pIdxOrderBy[nOrderBy];
  *(int*)&pIdxInfo->nConstraint = nTerm;
  *(int*)&pIdxInfo->nOrderBy = nOrderBy;
  *(struct sqlite3_index_constraint**)&pIdxInfo->aConstraint = pIdxCons;
  *(struct sqlite3_index_orderby**)&pIdxInfo->aOrderBy = pIdxOrderBy;
  *(struct sqlite3_index_constraint_usage**)&pIdxInfo->aConstraintUsage = pUsage;

  for(i=j=0, pTerm=pWC->a; i<pWC->nTerm; i++, pTerm++){   /*遍历WHERE子句的所有terms。同2673行for循环*/
    if( pTerm->leftCursor != pSrc->iCursor ) continue;
    assert( (pTerm->eOperator&(pTerm->eOperator-1))==0 );
    testcase( pTerm->eOperator==WO_IN );
    testcase( pTerm->eOperator==WO_ISNULL );
    if( pTerm->eOperator & (WO_IN|WO_ISNULL) ) continue;
    if( pTerm->wtFlags & TERM_VNULL ) continue;
    pIdxCons[j].iColumn = pTerm->u.leftColumn;  /*设置pIdxInfo字段*/
    pIdxCons[j].iTermOffset = i;
    pIdxCons[j].op = (u8)pTerm->eOperator;
	
    /* The direct assignment in the previous line is possible only because
    ** the WO_ and SQLITE_INDEX_CONSTRAINT_ codes are identical.  The
    ** following asserts verify this fact. 
	** 
	** 在前一行的直接分配是可能的，因为WO_和SQLITE_INDEX_CONSTRAINT_代码是相同的。
	** 以下判断来验证这个事实。
	*/
    assert( WO_EQ==SQLITE_INDEX_CONSTRAINT_EQ );
    assert( WO_LT==SQLITE_INDEX_CONSTRAINT_LT );
    assert( WO_LE==SQLITE_INDEX_CONSTRAINT_LE );
    assert( WO_GT==SQLITE_INDEX_CONSTRAINT_GT );
    assert( WO_GE==SQLITE_INDEX_CONSTRAINT_GE );
    assert( WO_MATCH==SQLITE_INDEX_CONSTRAINT_MATCH );
    assert( pTerm->eOperator & (WO_EQ|WO_LT|WO_LE|WO_GT|WO_GE|WO_MATCH) );
    j++;
  }
  for(i=0; i<nOrderBy; i++){  /*遍历ORDER BY子句所有项*/  /*设置pIdxInfo字段*/
    Expr *pExpr = pOrderBy->a[i].pExpr;
    pIdxOrderBy[i].iColumn = pExpr->iColumn;  /*表达式引用的列*/  
    pIdxOrderBy[i].desc = pOrderBy->a[i].sortOrder;  /*记录每项的排序序列*/
  }

  return pIdxInfo;  /*返回一个sqlite3_index_info数据结构的对象*/
}

/*
** The table object reference passed as the second argument to this function
** must represent a virtual table. This function invokes the xBestIndex()
** method of the virtual table with the sqlite3_index_info pointer passed
** as the argument.
**
** 表对象引用作为第二个参数传递给这个函数必须表示一个虚拟表。
** 这个函数使用sqlite3_index_info指针作为参数传递，唤醒虚拟表的xBestIndex()方法。
**
** If an error occurs, pParse is populated with an error message and a
** non-zero value is returned. Otherwise, 0 is returned and the output
** part of the sqlite3_index_info structure is left populated.
**
** 如果发生一个错误，pParse用一个错误信息填充，并返回一个non-zero值。
** 否则，返回0，并填充剩下的sqlite3_index_info数据结构的输出部分。
**
** Whether or not an error is returned, it is the responsibility of the
** caller to eventually free p->idxStr if p->needToFreeIdxStr indicates
** that this is required.
**
** 无论是否返回一个错误信息，如果p->needToFreeIdxStr表明这是必须的，那么调用者最终会释放p->idxStr。
*/
static int vtabBestIndex(Parse *pParse, Table *pTab, sqlite3_index_info *p){
  sqlite3_vtab *pVtab = sqlite3GetVTable(pParse->db, pTab)->pVtab;  /*返回虚拟表对象的指针*/
  int i;
  int rc;

  WHERETRACE(("xBestIndex for %s\n", pTab->zName));
  TRACE_IDX_INPUTS(p);  /*输出一个sqlite3_index_info数据结构的内容，只是用于测试和调试。*/
  rc = pVtab->pModule->xBestIndex(pVtab, p);   /* ? */
  TRACE_IDX_OUTPUTS(p);  /*输出一个sqlite3_index_info数据结构的内容，只是用于测试和调试。*/

  if( rc!=SQLITE_OK ){  /*表示有错误*/
    if( rc==SQLITE_NOMEM ){
      pParse->db->mallocFailed = 1;  /*动态内存分配失败*/
    }else if( !pVtab->zErrMsg ){
      sqlite3ErrorMsg(pParse, "%s", sqlite3ErrStr(rc));  /*sqlite3ErrStr()根据错位类型码返回对应的错误信息。出自main.c 1119行*/
    }else{
      sqlite3ErrorMsg(pParse, "%s", pVtab->zErrMsg);
    }
  }
  sqlite3_free(pVtab->zErrMsg);  /*释放错误信息内存*/
  pVtab->zErrMsg = 0;

  for(i=0; i<p->nConstraint; i++){
    if( !p->aConstraint[i].usable && p->aConstraintUsage[i].argvIndex>0 ){
      sqlite3ErrorMsg(pParse, 
          "table %s: xBestIndex returned an invalid plan", pTab->zName);  /*返回错误信息，*/
    }
  }

  return pParse->nErr;  /*pParse用一个错误信息填充，返回一个non-zero值*/
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
** 由虚拟表的xBestIndex方法来计算最佳索引。这个程序实际上只是一个封装，
** 设置sqlite3_index_info数据结构，用于与xBestIndex传递信息。
**
** In a join, this routine might be called multiple times for the
** same virtual table.  The sqlite3_index_info structure is created
** and initialized on the first invocation and reused on all subsequent
** invocations.  The sqlite3_index_info structure is also used when
** code is generated to access the virtual table.  The whereInfoDelete() 
** routine takes care of freeing the sqlite3_index_info structure after
** everybody has finished with it.
**
** 在一次连接当中，这个程序可能会被同一个虚表多次调用。sqlite3_index_info结构在第一次调用时
** 被创建和初始化，并在所有的后续调用中重复使用。sqlite3_index_info结构也在生成代码访问虚表时
** 被使用。在所有程序对sqlite3_index_info的使用完成后，whereInfoDelete()释放结构。
*/
static void bestVirtualIndex(
  Parse *pParse,                  /* The parsing context   解析上下文 */
  WhereClause *pWC,               /* The WHERE clause   WHERE子句 */
  struct SrcList_item *pSrc,      /* The FROM clause term to search   需要查询的FROM子句 */
  Bitmask notReady,               /* Mask of cursors not available for index   游标掩码对于索引无效 */
  Bitmask notValid,               /* Cursors not valid for any purpose   游标对于任何用途都无效 */
  ExprList *pOrderBy,             /* The order by clause   ORDER BY子句 */
  WhereCost *pCost,               /* Lowest cost query plan   最小代价查询计划 */
  sqlite3_index_info **ppIdxInfo  /* Index information passed to xBestIndex   传入xBestIndex的索引信息 */
){
  Table *pTab = pSrc->pTab;   /*定义FROM子句中的表*/
  sqlite3_index_info *pIdxInfo;  /*用于存储选出的索引信息*/
  struct sqlite3_index_constraint *pIdxCons;   /*用于存储索引约束信息*/
  struct sqlite3_index_constraint_usage *pUsage;  /*用于存储有用的索引约束*/
  WhereTerm *pTerm;
  int i, j;   /*i是循环计数器，j用于记录*/
  int nOrderBy;  /*Order By子句中的terms数*/
  double rCost;  /*查询策略代价*/

  /* Make sure wsFlags is initialized to some sane value. Otherwise, if the 
  ** malloc in allocateIndexInfo() fails and this function returns leaving
  ** wsFlags in an uninitialized state, the caller may behave unpredictably.
  **
  ** 确保wsFlags初始化为某个合理的值。否则，如果allocateIndexInfo()内存分配失败，
  ** 并且这个函数返回，使得wsFlags处于未初始化状态的，调用者可能会有不可预见的操作。
  */
  memset(pCost, 0, sizeof(*pCost));   /*分配内存*/
  pCost->plan.wsFlags = WHERE_VIRTUALTABLE;   /*WHERE_VIRTUALTABLE标志表示查询计划使用虚拟表处理*/

  /* If the sqlite3_index_info structure has not been previously
  ** allocated and initialized, then allocate and initialize it now.
  **
  ** 如果sqlite3_index_info结构没有预先分配和初始化，那么就现在进行分配和初始化。
  */
  pIdxInfo = *ppIdxInfo;
  if( pIdxInfo==0 ){   /*没有分配*/
    *ppIdxInfo = pIdxInfo = allocateIndexInfo(pParse, pWC, pSrc, pOrderBy);  /*分配并填充一个索引结构*/
  }
  if( pIdxInfo==0 ){   /*分配失败*/
    return;
  }

  /* At this point, the sqlite3_index_info structure that pIdxInfo points
  ** to will have been initialized, either during the current invocation or
  ** during some prior invocation.  Now we just have to customize the
  ** details of pIdxInfo for the current invocation and pass it to
  ** xBestIndex.
  ** 
  ** 此时，在当前调用或在某个先前的调用中，pIdxInfo指向的sqlite3_index_info数据结构已被初始化。
  ** 现在只需要为当前调用自定义pIdxInfo的细节，并且把它传递给xBestIndex。
  **
  ** The module name must be defined. Also, by this point there must
  ** be a pointer to an sqlite3_vtab structure. Otherwise
  ** sqlite3ViewGetColumnNames() would have picked up the error. 
  **
  ** 必须定义模块名。同时，必须有个指针指向sqlite3_vtab数据结构。
  ** 否则sqlite3ViewGetColumnNames()会出现错误。
  */
  assert( pTab->azModuleArg && pTab->azModuleArg[0] );  /*检验是否定义了模块名*/
  assert( sqlite3GetVTable(pParse->db, pTab) );  /*返回指向虚表的指针*/

  /* Set the aConstraint[].usable fields and initialize all 
  ** output variables to zero.
  **
  ** 设置aConstraint[].usable字段,并将所有输出变量初始化为0。
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
  ** 当右边只包含对当前表中左边的表的联系时，满足约束条件的aConstraint[].usable为真。
  ** 换句话说，如果约束条件是如下形式：
  **
  **           column = expr
  ** 并且正在评估一个连接，只有当当前表的左边的expr中涉及的所有表都包含列，列的约束条件才有效。
  **
  ** The aConstraints[] array contains entries for all constraints on the
  ** current table. That way we only have to compute it once even though
  ** we might try to pick the best index multiple times. For each attempt
  ** at picking an index, the order of tables in the join might be different
  ** so we have to recompute the usable flag each time.
  **
  ** aConstraints[]数组包含当前表的所有约束。这样的话，即使我们多次选择最优索引，也只需要计算一次。
  ** 对于每次选取一个索引，连接中的表的顺序可能不同，所以我们需要每次都重复计算可用flag。
  */
  pIdxCons = *(struct sqlite3_index_constraint**)&pIdxInfo->aConstraint;   /*初始化pIdxCons*/
  pUsage = pIdxInfo->aConstraintUsage;   /*初始化pUsage*/
  for(i=0; i<pIdxInfo->nConstraint; i++, pIdxCons++){   /*循环遍历索引约束*/
    j = pIdxCons->iTermOffset; 
    pTerm = &pWC->a[j];
    pIdxCons->usable = (pTerm->prereqRight&notReady) ? 0 : 1;
  }
  memset(pUsage, 0, sizeof(pUsage[0])*pIdxInfo->nConstraint);  /*分配内存*/
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
  **
  ** 如果有一个ORDER BY子句，并且选出的虚拟表索引不能被ORDER BY使用，查找的代价相应的增加。
  ** 这与在bestBtreeIndex()中的非虚拟表的处理过程相匹配。
  */
  rCost = pIdxInfo->estimatedCost;
  if( pOrderBy && pIdxInfo->orderByConsumed==0 ){
    rCost += estLog(rCost)*rCost;
  }

  /* The cost is not allowed to be larger than SQLITE_BIG_DBL (the
  ** inital value of lowestCost in this loop). If it is, then the
  ** (cost<lowestCost) test below will never be true.
  ** 
  ** 代价不允许大于SQLITE_BIG_DBL(在这个循环中最低代价的初始值)。
  ** 如果它比SQLITE_BIG_DBL大，那么下面的(cost<lowestCost)测试值将永远不为真。
  **
  ** Use "(double)2" instead of "2.0" in case OMIT_FLOATING_POINT 
  ** is defined.
  **
  ** 如果定义了OMIT_FLOATING_POINT，则使用"(double)2"代替"2.0"
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
  ** 通过使用多重索引查找一个更加有效的访问模式，来优化WHERE子句中的一个OR表达式
  */
  bestOrClauseIndex(pParse, pWC, pSrc, notReady, notValid, pOrderBy, pCost);  /*得到最优扫描策略*/
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
**
** 估计一个索引的所有关键字中某个特殊关键字的位置。用以下格式来储存结果：
**    aStat[0]      估计小于pVal的行数
**    aStat[1]      估计等于pVal的行数
**
** 如果成功，返回SQLITE_OK。
*/
static int whereKeyStats(
  Parse *pParse,              /* Database connection   数据库连接 */
  Index *pIdx,                /* Index to consider domain of   需要考虑的索引域 */
  sqlite3_value *pVal,        /* Value to consider   需要考虑的值 */
  int roundUp,                /* Round up if true.  Round down if false   如果TRUE，则上舍入，如果FALSE，则下舍入 */
  tRowcnt *aStat              /* OUT: stats written here   在此写入结果*/
){
  tRowcnt n;  /*存储一个表或者索引中行数的估计值*/
  IndexSample *aSample;  /*定义sqlite_stat3表中的一个样本*/
  int i, eType;
  int isEq = 0;  /*记录==*/
  i64 v;
  double r, rS;

  assert( roundUp==0 || roundUp==1 );
  assert( pIdx->nSample>0 );
  if( pVal==0 ) return SQLITE_ERROR;
  n = pIdx->aiRowEst[0];
  aSample = pIdx->aSample;
  eType = sqlite3_value_type(pVal);

  if( eType==SQLITE_INTEGER ){  /*整型*/
    v = sqlite3_value_int64(pVal);  /*64位*/
    r = (i64)v;  /*8字节有符号整型*/
    for(i=0; i<pIdx->nSample; i++){  /*循环遍历sqlite_stat3表中的全部索引*/
      if( aSample[i].eType==SQLITE_NULL ) continue;  /*往下执行*/
      if( aSample[i].eType>=SQLITE_TEXT ) break;  /*类型不符*/
      if( aSample[i].eType==SQLITE_INTEGER ){
        if( aSample[i].u.i>=v ){
          isEq = aSample[i].u.i==v;  /*记录是否==*/
          break;
        }
      }else{
        assert( aSample[i].eType==SQLITE_FLOAT );  /*判定为浮点型*/
        if( aSample[i].u.r>=r ){
          isEq = aSample[i].u.r==r;  /*记录是否==*/
          break;
        }
      }
    }
  }else if( eType==SQLITE_FLOAT ){  /*浮点型*/
    r = sqlite3_value_double(pVal);
    for(i=0; i<pIdx->nSample; i++){   /*循环遍历sqlite_stat3表中的全部索引*/
      if( aSample[i].eType==SQLITE_NULL ) continue;  /*往下执行*/
      if( aSample[i].eType>=SQLITE_TEXT ) break;  /*类型不符*/
      if( aSample[i].eType==SQLITE_FLOAT ){
        rS = aSample[i].u.r;  /*赋值*/
      }else{
        rS = aSample[i].u.i;  /*整型处理*/
      }
      if( rS>=r ){
        isEq = rS==r;  /*记录是否==*/
        break;
      }
    }
  }else if( eType==SQLITE_NULL ){   /*空*/
    i = 0;
    if( aSample[0].eType==SQLITE_NULL ) isEq = 1;  
  }else{  /*其他类型*/
    assert( eType==SQLITE_TEXT || eType==SQLITE_BLOB );
    for(i=0; i<pIdx->nSample; i++){
      if( aSample[i].eType==SQLITE_TEXT || aSample[i].eType==SQLITE_BLOB ){  /*类型不符*/
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
          sqlite3DbFree(db, zSample);	/*释放与某个特定的数据库连接相关联的内存*/
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
  if( isEq ){    /*aSample[i]==pVal情况*/
    assert( i<pIdx->nSample );
    aStat[0] = aSample[i].nLt;  /*nLt表示<约束的数量*/
    aStat[1] = aSample[i].nEq;  /*nEq表示==约束的数量*/
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
** 如果pExpr表达式表示一个文字值，在返回前，设置*pp指向一个包含相同值的sqlite3_value数据结构，
** 且具有关联性aff。调用者通过把它传递给sqlite3ValueFree()最终释放这个数据结构。
**
** If the current parse is a recompile (sqlite3Reprepare()) and pExpr
** is an SQL variable that currently has a non-NULL value bound to it,
** create an sqlite3_value structure containing this value, again with
** affinity aff applied to it, instead.
**
** 如果当前分析是一个重新编译(sqlite3Reprepare())，并且pExpr是一个SQL变量，当前绑定了
** 一个非空值，则创建一个包含这个值的sqlite3_value数据结构，同样具有关联性aff。
**
** If neither of the above apply, set *pp to NULL.
**
** 如果上述情况都不满足，把*pp设置为NULL值。
**
** If an error occurs, return an error code. Otherwise, SQLITE_OK.
**
** 如果发生一个错误，返回一个错误代码，否则，返回SQLITE_OK.
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
    sqlite3VdbeSetVarmask(pParse->pVdbe, iVar);  /*配置SQL变量iVar使得绑定一个新值来唤醒函数sqlite3_reoptimize()*/
	/*创建sqlite3_value数据结构，返回指向该结构的指针，这个结构包括一个VM v的iVar参数的值*/
    *pp = sqlite3VdbeGetValue(pParse->pReprepare, iVar, aff);  
    return SQLITE_OK;  /*创建成功*/
  }
  return sqlite3ValueFromExpr(pParse->db, pExpr, SQLITE_UTF8, aff, pp);  /*创建一个新的包含pExpr值的sqlite3_value对象*/
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
** 这个函数用于估计通过扫描一个索引获取一系列值而需要访问的行数。这个范围可能有一个上限，或者有一个下限，
** 或者两者都有。WHERE子句的terms通过pLower和pUpper分别设置上限和下限的值。
** 例如，假设索引p在t1(a)上:
**   ... FROM t1 WHERE a > ? AND a < ? ...
**                    |_____|   |_____|
**                       |         |
**                     pLower    pUpper
** 如果没有给出上限或下限，那么传递NULL值来代替相应的WhereTerm.
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
** nEq参数传递索引列中受范围约束的索引。或者说是，通过索引扫描优化的等式约束的个数。
** 例如，假设索引p在t1(a, b)，SQL查询是:
**   ... FROM t1 WHERE a = ? AND b > ? AND b < ? ...
** 那么nEq传递为值1(作为范围受限的列，b，是索引第二个最左边的列)。或者，如果查询是:
**   ... FROM t1 WHERE a > ? AND a < ? ...
** 那么nEq传递为值0。
**
** The returned value is an integer divisor to reduce the estimated
** search space.  A return value of 1 means that range constraints are
** no help at all.  A return value of 2 means range constraints are
** expected to reduce the search space by half.  And so forth...
**
** 返回值是一个用于减少评估的搜索空间的整型约数。一个值为1的返回值意味着范围约束没有意义。
** 一个值为2的返回值意味着范围约束用于减少一半的搜索空间。等等...
**
** In the absence of sqlite_stat3 ANALYZE data, each range inequality
** reduces the search space by a factor of 4.  Hence a single constraint (x>?)
** results in a return of 4 and a range constraint (x>? AND x<?) results
** in a return of 16.
**
** 在缺少sqlite_stat3 ANALYZE数据的情况下，每个范围不等式减少了4倍的搜索空间。
** 因此一个单独的约束(x>?)将返回4，一个范围约束(x>? AND x<?)将返回16。
*/
static int whereRangeScanEst(
  Parse *pParse,       /* Parsing & code generating context   分析和代码生成上下文 */
  Index *p,            /* The index containing the range-compared column; "x"   包含范围比较列的索引:"x" */
  int nEq,             /* index into p->aCol[] of the range-compared column   指向范围比较列的p->aCol[]的指针 */
  WhereTerm *pLower,   /* Lower bound on the range. ex: "x>123" Might be NULL   范围的下限，ex: "x>123"，可能是NULL */
  WhereTerm *pUpper,   /* Upper bound on the range. ex: "x<455" Might be NULL   范围的上限，ex: "x<455"，可能是NULL */
  double *pRangeDiv    /* OUT: Reduce search space by this divisor   OUT:通过这个约数来减少搜索空间 */
){
  int rc = SQLITE_OK;

#ifdef SQLITE_ENABLE_STAT3

  if( nEq==0 && p->nSample ){  
    sqlite3_value *pRangeVal;
	/*tRowcnt数据类型用于存储一个表或者索引中行数的估计值*/
    tRowcnt iLower = 0;  /*记录下限值*/
    tRowcnt iUpper = p->aiRowEst[0];  /*记录上限值，aiRowEst表示每列选择的行数*/
    tRowcnt a[2];   /*统计信息*/
    u8 aff = p->pTable->aCol[p->aiColumn[0]].affinity;  /*定义索引列的关联性*/

    if( pLower ){  /*存在下限*/
      Expr *pExpr = pLower->pExpr->pRight;  /*得到下限表达式的右边，ex: "x>123"中的‘123’ */
      rc = valueFromExpr(pParse, pExpr, aff, &pRangeVal);  /*成功创建一个包含pExpr值的sqlite3_value对象，返回SQLITE_OK*/
      assert( pLower->eOperator==WO_GT || pLower->eOperator==WO_GE );  /*判定下限运算符>或>=*/
      if( rc==SQLITE_OK
       && whereKeyStats(pParse, p, pRangeVal, 0, a)==SQLITE_OK   /*成功返回索引p的所有关键字中某个特殊关键字的位置*/
      ){
        iLower = a[0];
        if( pLower->eOperator==WO_GT ) iLower += a[1];  /*记录下限数*/
      }
      sqlite3ValueFree(pRangeVal);  /*释放sqlite3_value对象*/
    }
    if( rc==SQLITE_OK && pUpper ){  /*存在上限*/
      Expr *pExpr = pUpper->pExpr->pRight;  /*得到上限表达式的右边，ex: "x<455"中的455 */
      rc = valueFromExpr(pParse, pExpr, aff, &pRangeVal);  /*成功创建一个包含pExpr值的sqlite3_value对象，返回SQLITE_OK*/
      assert( pUpper->eOperator==WO_LT || pUpper->eOperator==WO_LE );  /*判定上限运算符<或<=*/
      if( rc==SQLITE_OK
       && whereKeyStats(pParse, p, pRangeVal, 1, a)==SQLITE_OK  /*成功返回索引p的所有关键字中某个特殊关键字的位置*/
      ){
        iUpper = a[0];
        if( pUpper->eOperator==WO_LE ) iUpper += a[1];  /*记录上限数*/
      }
      sqlite3ValueFree(pRangeVal);  /*释放sqlite3_value对象*/
    }
    if( rc==SQLITE_OK ){  /*计算减少搜索空间的约数*/
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
  *pRangeDiv = (double)1;  /*确定减少搜索空间的约数*/
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
** 估计基于等值约束x=VALUE返回的行数，其中VALUE在直方图数据中出现。这只在x是一个
** 索引的最左边的列，并且sqlite_stat3直方图数据对于这个索引是有效的情况下，才起作用。
** 当pExpr==NULL时，意味着约束是"x IS NULL"，而不是"x=VALUE"。
**
** Write the estimated row count into *pnRow and return SQLITE_OK. If 
** unable to make an estimate, leave *pnRow unchanged and return non-zero.
** 
** 将估计的行数写入*pnRow，返回SQLITE_OK。如果不能做一个估计，保持*pnRow不变，返回非0值。
**
** This routine can fail if it is unable to load a collating sequence required
** for string comparison, or if unable to allocate memory for a UTF conversion
** required for comparison.  The error is stored in the pParse structure.
** 
** 如果这个程序不能为字符串比较载入所需的排序序列，或者不能为比较所需的一个UTF(Unicode转化模式)
** 转化分配内存，那么这个程序会失败。错误信息存储在pParse数据结构中。
*/
static int whereEqualScanEst(
  Parse *pParse,       /* Parsing & code generating context   解析和生成代码上下文 */
  Index *p,            /* The index whose left-most column is pTerm    索引的最左边列为pTerm*/
  Expr *pExpr,         /* Expression for VALUE in the x=VALUE constraint   在x=VALUE约束中VALUE的表达式 */
  double *pnRow        /* Write the revised row estimate here   写入修改后的行数估计值 */
){
  sqlite3_value *pRhs = 0;  /* VALUE on right-hand side of pTerm   在pTerm右边的VALUE */
  u8 aff;                   /* Column affinity   列关联性 */
  int rc;                   /* Subfunction return code   子函数的返回代码 */
  tRowcnt a[2];             /* Statistics   统计信息 */

  assert( p->aSample!=0 );
  assert( p->nSample>0 );
  aff = p->pTable->aCol[p->aiColumn[0]].affinity;  /*索引引用的列的关联性*/
  if( pExpr ){   /*VALUE的表达式存在*/
    rc = valueFromExpr(pParse, pExpr, aff, &pRhs);  /*成功创建一个sqlite3_value数据结构*/
    if( rc ) goto whereEqualScanEst_cancel;
  }else{
    pRhs = sqlite3ValueNew(pParse->db);  /*创建一个新的sqlite3_value对象*/
  }
  if( pRhs==0 ) return SQLITE_NOTFOUND;
  rc = whereKeyStats(pParse, p, pRhs, 0, a);  /*成功估计索引的所有关键字中某个特殊关键字的位置*/
  if( rc==SQLITE_OK ){
	WHERETRACE(("equality scan regions: %d\n", (int)a[1]));
    *pnRow = a[1];  /*得到最终行数*/
  }
  whereEqualScanEst_cancel:
	sqlite3ValueFree(pRhs);  /*释放pTerm右边的VALUE*/
  return rc;
}
#endif /* defined(SQLITE_ENABLE_STAT3) */

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
** 估计基于IN约束条件返回的行数，其中IN运算符的右边是一系列值。例如:
**        WHERE x IN (1,2,3,4)
** 将估计的行数写入*pnRow，返回SQLITE_OK。如果不能做一个估计，保持*pnRow不变，返回非0值。
**
** This routine can fail if it is unable to load a collating sequence required
** for string comparison, or if unable to allocate memory for a UTF conversion
** required for comparison.  The error is stored in the pParse structure.
** 
** 如果这个程序不能为字符串比较载入所需的排序序列，或者不能为比较所需的一个UTF(Unicode转化模式)
** 转化分配内存，那么这个程序会失败。错误信息存储在pParse数据结构中。
*/
static int whereInScanEst(
  Parse *pParse,       /* Parsing & code generating context   解析和生成代码上下文 */
  Index *p,            /* The index whose left-most column is pTerm   索引的最左边列为pTerm */
  ExprList *pList,     /* The value list on the RHS of "x IN (v1,v2,v3,...)"   "x IN (v1,v2,v3,...)"右边的一系列值 */
  double *pnRow        /* Write the revised row estimate here   写入修改后的行数估计值 */
){
  int rc = SQLITE_OK;         /* Subfunction return code   子函数的返回代码 */
  double nEst;                /* Number of rows for a single term   一个term的行数 */
  double nRowEst = (double)0; /* New estimate of the number of rows  行数的新估计值 */
  int i;                      /* Loop counter   循环计数器 */

  assert( p->aSample!=0 );
  for(i=0; rc==SQLITE_OK && i<pList->nExpr; i++){  /*循环遍历IN运算符的右边是一系列值*/
    nEst = p->aiRowEst[0];  /*记录一个term的行数*/
    rc = whereEqualScanEst(pParse, p, pList->a[i].pExpr, &nEst);  /*成功得到等值约束x=VALUE返回的行数*/
    nRowEst += nEst;  /*记录符合条件的行数*/
  }
  if( rc==SQLITE_OK ){
    if( nRowEst > p->aiRowEst[0] ) nRowEst = p->aiRowEst[0];
    *pnRow = nRowEst;  /*得到最终行数*/
    WHERETRACE(("IN row estimate: est=%g\n", nRowEst));
  }
  return rc;
}

/*
** Find the best query plan for accessing a particular table.  Write the best
** query plan and its cost into the WhereCost object supplied as the last parameter.
** 
** 查找最优查询计划，来访问一个特定表。将最优查询计划和它的代价写入WhereCost对象，并作为最后一个参数。
**
** The lowest cost plan wins.  The cost is an estimate of the amount of
** CPU and disk I/O needed to process the requested result.
**  
** 最小代价的查询计划被采用。代价是一个估计值，估计处理请求的结果所需要的CPU和磁盘I/O代价总和。
** 
** Factors that influence cost include:
**   * The estimated number of rows that will be retrieved. (The fewer the better.)
**   * Whether or not sorting must occur.
**   * Whether or not there must be separate lookups in the index and in the main table.
**
** 影响代价的因素有:
**   * 查询读取的记录数 (越少越好)
**   * 结果是否排序 
**   * 是否需要分别在索引和主表中查找  
**
** If there was an INDEXED BY clause (pSrc->pIndex) attached to the table in
** the SQL statement, then this function only considers plans using the named index. 
** If no such plan is found, then the returned cost is SQLITE_BIG_DBL. If a plan
** is found that uses the named index, then the cost is calculated in the usual way.
**
** 此函数只考虑使用指定索引的计划。
** 如果没有找到这样的计划，那么返回的代价是SQLITE_BIG_DBL。
** 如果找到使用指定索引的计划，则按通常方式计算代价。
** 
** If a NOT INDEXED clause (pSrc->notIndexed!=0) was attached to the table 
** in the SELECT statement, then no indexes are considered. However, the 
** selected plan may still take advantage of the built-in rowid primary key index.
** 
** 如果一个NOT INDEX子句(pSrc->notIndexed!=0)附加到SQL语句中的表，则认为没有索引。  
** 然而，查询计划仍然可以利用创建在rowid上的主键索引。
*/
static void bestBtreeIndex(
  Parse *pParse,              /* The parsing context  解析上下文 */
  WhereClause *pWC,           /* The WHERE clause   WHERE子句 */
  struct SrcList_item *pSrc,  /* The FROM clause term to search   要查找的FROM子句term */
  Bitmask notReady,           /* Mask of cursors not available for indexing   游标掩码对索引无效*/
  Bitmask notValid,           /* Cursors not available for any purpose  对于任何用途游标都无效 */
  ExprList *pOrderBy,         /* The ORDER BY clause   ORDER BY子句 */
  ExprList *pDistinct,        /* The select-list if query is DISTINCT  查询是DISTINCT时的select-list */
  WhereCost *pCost            /* Lowest cost query plan  最小代价查询计划 */
){
  int iCur = pSrc->iCursor;   /* The cursor of the table to be accessed   要访问的表的游标 */
  Index *pProbe;              /* An index we are evaluating   要评估的索引 */
  Index *pIdx;                /* Copy of pProbe, or zero for IPK index   pProbe的拷贝，或为0的IPK索引 */
  int eqTermMask;             /* Current mask of valid equality operators   有效等值运算符的当前掩码 */
  int idxEqTermMask;          /* Index mask of valid equality operators   有效等值运算符的索引掩码 */
  Index sPk;                  /* A fake index object for the primary key  一个主键的虚拟索引对象 */
  tRowcnt aiRowEstPk[2];      /* The aiRowEst[] value for the sPk index  sPk索引的aiRowEst[]值 */
  int aiColumnPk = -1;        /* The aColumn[] value for the sPk index  sPk索引的aColumn[]值 */
  int wsFlagMask;             /* Allowed flags in pCost->plan.wsFlag  pCost->plan.wsFlag允许的标志 */

  /* 
  ** Initialize the cost to a worst-case value 
  ** 初始化代价为worst-case值 
  */
  memset(pCost, 0, sizeof(*pCost));
  pCost->rCost = SQLITE_BIG_DBL;

  /* 
  ** If the pSrc table is the right table of a LEFT JOIN then we may not
  ** use an index to satisfy IS NULL constraints on that table.  This is
  ** because columns might end up being NULL if the table does not match -
  ** a circumstance which the index cannot help us discover.  Ticket #2177.
  **
  ** 如果pSrc表是一个左连接的右表，那么不能使用索引来满足该表中的IS NULL约束。这是因为
  ** 如果表不匹配索引不能查找的一种情况，列可能最终会赋值为NULL。
  */
  if( pSrc->jointype & JT_LEFT ){
    idxEqTermMask = WO_EQ|WO_IN;
  }else{
    idxEqTermMask = WO_EQ|WO_IN|WO_ISNULL;
  }

  if( pSrc->pIndex ){
	  
    /* 
	** An INDEXED BY clause specifies a particular index to use. 
	** 一个INDEXED BY子句指定使用一个特定的索引。 
	*/
    pIdx = pProbe = pSrc->pIndex;
    wsFlagMask = ~(WHERE_ROWID_EQ|WHERE_ROWID_RANGE);
    eqTermMask = idxEqTermMask;
  }else{
    
	/* 
	** There is no INDEXED BY clause.  Create a fake Index object in local
    ** variable sPk to represent the rowid primary key index.  Make this fake index
    ** the first in a chain of Index objects with all of the real indices to follow.
    **
    ** 没有INDEXED BY子句。在局部变量sPk中创建一个表示rowid主键索引的虚拟索引对象。
    ** 使这个虚拟索引处于一连串真实索引对象的第一个位置。
    */
    Index *pFirst;     /* First of real indices on the table  表中真实索引的第一个 */
	
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
      /* 
      ** The real indices of the table are only considered if the
      ** NOT INDEXED qualifier is omitted from the FROM clause.
      **
      ** 如果FROM子句没有NOT INDEXED限定符，则只考虑表的真是索引 
	  */
      sPk.pNext = pFirst;
    }
    pProbe = &sPk;
    wsFlagMask = ~(
        WHERE_COLUMN_IN|WHERE_COLUMN_EQ|WHERE_COLUMN_NULL|WHERE_COLUMN_RANGE
    );
    eqTermMask = WO_EQ|WO_IN;
    pIdx = 0;
  }

  /* 
  ** Loop over all indices looking for the best one to use.
  ** 遍历其所有索引，查找最优索引来使用。
  */
  for(; pProbe; pIdx=pProbe=pProbe->pNext){   /*循环所有索引*/
    const tRowcnt * const aiRowEst = pProbe->aiRowEst;
    double cost;                /* Cost of using pProbe   使用pProbe的代价 */
    double nRow;                /* Estimated number of rows in result set   结果集中行数估计值 */
    double log10N = (double)1;  /* base-10 logarithm of nRow (inexact)   nRow的以10为底的对数(不精确的) */
    int rev;                    /* True to scan in reverse order   倒序正确扫描 */
    int wsFlags = 0;
    Bitmask used = 0;

    /* 
	** The following variables are populated based on the properties of
    ** index being evaluated. They are then used to determine the expected
    ** cost and number of rows returned.
    **
    ** 下列的变量基于已评估索引的性质来填充。这些变量用于决定返回的预期代价和行数。
    **
    ** nEq: 
    **   Number of equality terms that can be implemented using the index.
    **   In other words, the number of initial fields in the index that
    **   are used in == or IN or NOT NULL constraints of the WHERE clause.
	**
    ** 可以用索引实现的等式terms的个数。换句话说，是索引中初始字段的数量，
	** 此索引用于WHERE子句的==或IN或NOT NULL约束。
    **===================
    ** nInMul:  
    **   The "in-multiplier". This is an estimate of how many seek operations SQLite 
    **   must perform on the index in question. For example, if the WHERE clause is:
    **      WHERE a IN (1, 2, 3) AND b IN (4, 5, 6)
    **   SQLite must perform 9 lookups on an index on (a, b), so nInMul is 
    **   set to 9. Given the same schema and either of the following WHERE clauses:
    **      WHERE a =  1
    **      WHERE a >= 2
    **   nInMul is set to 1.
    **
    **   If there exists a WHERE term of the form "x IN (SELECT ...)", then the
    **   sub-select is assumed to return 25 rows for the purposes of determining nInMul.
	**
	** “in-multiplier”，这是估计SQLite在问题中的索引上执行了多少查询操作。
    ** 例如，如果where子句如下：
	**      WHERE a IN (1, 2, 3) AND b IN (4, 5, 6)
	** SQLite必须在(a,b)的索引上执行9次查询，所以nInMul设为9。给定相同的模式或如下WHERE子句：
	**      WHERE a =  1
    **      WHERE a >= 2 
	** 如果存在一个WHERE子句如下形式（x IN (SELECT ...))，那么子查询假设返回25行，确定nInMul的值。   
	**====================
    ** bInEst:  
    **   Set to true if there was at least one "x IN (SELECT ...)" term used 
    **   in determining the value of nInMul.  Note that the RHS of the IN
    **   operator must be a SELECT, not a value list, for this variable to be true.
    **
    ** 如果至少有一个"x IN (SELECT ...)"子查询term用于决定nInMul的值，则设为true。
    ** 注意，IN操作符的右边必须是一个SELECT子查询,而不是一个值列表,这个变量才能为true。
    **====================
    ** rangeDiv:
    **   An estimate of a divisor by which to reduce the search space due
    **   to inequality constraints.  In the absence of sqlite_stat3 ANALYZE
    **   data, a single inequality reduces the search space to 1/4rd its
    **   original size (rangeDiv==4).  Two inequalities reduce the search
    **   space to 1/16th of its original size (rangeDiv==16).
    **
    ** 一个由于不等式约束而减少搜索空间的因子评估。在缺少sqlite_stat3分析数据的情况下,
    ** 一个不等式约束可以减少搜索空间为原来的1/4(rangeDiv==4)。
    ** 两个不等式约束减少搜索空间为原来的1/16(rangeDiv==16)。
    **====================
    ** bSort:   
    **   Boolean. True if there is an ORDER BY clause that will require an external 
    **   sort (i.e. scanning the index being evaluated will not correctly order records)
    **
	** 布尔类型，如果有一个ORDER BY子句需要一个外部排序(用于评估的索引扫描不能不准确地排序记录)，则为真。
    **====================
    ** bLookup: 
    **   Boolean. True if a table lookup is required for each index entry
    **   visited.  In other words, true if this is not a covering index.
    ** 
	** 布尔类型，如果访问每个索引项都需要一个表查询，则为真。换句话说，如果这不是一个覆盖索引，则为真。
    **
    **   This is always false for the rowid primary key index of a table.
    **   For other indexes, it is true unless all the columns of the table
    **   used by the SELECT statement are present in the index (such an
    **   index is sometimes described as a covering index).
    ** 
	** 对于一个标的rowid主键索引，总是false。对于其他索引，除非SELECT语句使用的表的所有列都
	** 出现在这个索引中(这样的索引有时描述为一个覆盖索引)，才为真。
    **
    **   For example, given the index on (a, b), the second of the following 
    **   two queries requires table b-tree lookups in order to find the value
    **   of column c, but the first does not because columns a and b are
    **   both available in the index.
    ** 
	** 例如，给出(a,b)上的索引，下面的第二个查询需要表的b树查找，来找到列c的值,
    ** 但第一查询不需要，因为列a和b在索引中是可用的。
    **             SELECT a, b    FROM tbl WHERE a = 1;
    **             SELECT a, b, c FROM tbl WHERE a = 1;
    */
    int nEq;                      /* Number of == or IN terms matching index   匹配索引的==或IN terms数目 */
    int bInEst = 0;               /* True if "x IN (SELECT...)" seen   如果存在"x IN (SELECT...)"则为TRUE */
    int nInMul = 1;               /* Number of distinct equalities to lookup   查找的DISTINCT等式的数目 */
    double rangeDiv = (double)1;  /* Estimated reduction in search space   估计搜索空间的减少量 */
    int nBound = 0;               /* Number of range constraints seen   发现的范围约束数目 */
    int bSort = !!pOrderBy;       /* True if external sort required   如果需要外部查询则为TRUE */
    int bDist = !!pDistinct;      /* True if index cannot help with DISTINCT  如果索引对DISTINCT约束没有帮助，则为TRUE */
    int bLookup = 0;              /* True if not a covering index   如果不是一个覆盖索引则为TRUE */
    WhereTerm *pTerm;             /* A single term of the WHERE clause  WHERE子句的一个term */
#ifdef SQLITE_ENABLE_STAT3
    WhereTerm *pFirstTerm = 0;    /* First term matching the index  匹配索引的第一个term */
#endif

    /* 
	** Determine the values of nEq and nInMul 
    ** 计算nEq和nInMul值
    */
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
          /* "x IN (SELECT ...)": Assume the SELECT returns 25 rows.  "x IN (SELECT ...)": 假定SELECT返回25行 */
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
 
    /* 
	** If the index being considered is UNIQUE, and there is an equality 
    ** constraint for all columns in the index, then this search will find
    ** at most a single row. In this case set the WHERE_UNIQUE flag to 
    ** indicate this to the caller.
    **
    ** 如果在考虑的索引是UNIQUE索引,且对于索引的所有列都有一个等式约束,则这个搜索至多查找到一行。
	** 在这种情况下，设置WHERE_UNIQUE标志，来给调用者表明情况。
	**
    ** Otherwise, if the search may find more than one row, test to see if
    ** there is a range constraint on indexed column (nEq+1) that can be 
    ** optimized using the index. 
	**
    ** 否则,如果搜索查找超过一行，测试在索引列(nEq+1)上是否有范围约束,可以使用该索引优化此约束。
    */
    /*计算nBound值*/
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
        
        whereRangeScanEst(pParse, pProbe, nEq, pBtm, pTop, &rangeDiv);  /*估计范围约束的代价*/
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

    /* 
	** If there is an ORDER BY clause and the index being considered will
    ** naturally scan rows in the required order, set the appropriate flags
    ** in wsFlags. Otherwise, if there is an ORDER BY clause but the index
    ** will scan rows in a different order, set the bSort variable.  
    **
    ** 如果有一个ORDER BY子句，且在考虑的索引将按规定的顺序扫描行，则在wsFlags中设置适当的标志。
    ** 否则，如果有一个ORDER BY子句，但索引将按不同的顺序扫描行，则设置bSort变量。
    */
    if( isSortingIndex(
          pParse, pWC->pMaskSet, pProbe, iCur, pOrderBy, nEq, wsFlags, &rev)
    ){
      bSort = 0;
      wsFlags |= WHERE_ROWID_RANGE|WHERE_COLUMN_RANGE|WHERE_ORDERBY;
      wsFlags |= (rev ? WHERE_REVERSE : 0);
    }

    /* If there is a DISTINCT qualifier and this index will scan rows in order of
    ** the DISTINCT expressions, clear bDist and set the appropriate flags in wsFlags.
    ** 
    ** 如果DISTINCT限定符,且索引将按DISTINCT表达式的顺序扫描行，清除bDist并在wsFlags中设置适当的标志。
    */
    if( isDistinctIndex(pParse, pWC, pProbe, iCur, pDistinct, nEq)
     && (wsFlags & WHERE_COLUMN_IN)==0
    ){
      bDist = 0;
      wsFlags |= WHERE_ROWID_RANGE|WHERE_COLUMN_RANGE|WHERE_DISTINCT;
    }

    /* 
	** If currently calculating the cost of using an index (not the IPK index),
    ** determine if all required column data may be obtained without using the 
    ** main table (i.e. if the index is a covering index for this query). If it is,
    ** set the WHERE_IDX_ONLY flag in wsFlags. Otherwise, set the bLookup variable to true.
    ** 
    ** 如果计算当前使用一个索引(不是IPK索引)的代价，确定是否能不使用主表就可以获得所有需要的列数据(即
	** 如果该查询的索引是一个覆盖索引)。如果能，在wsFlags设置 WHERE_IDX_ONLY标志。否则,设置bLookup变量为true。
    */
    if( pIdx && wsFlags ){  /*索引存在*/
      Bitmask m = pSrc->colUsed;  /*表中使用索引的列*/
      int j;  /*循环计数器*/
      for(j=0; j<pIdx->nColumn; j++){  /*遍历所有使用索引的列，判断是否所有列都在索引中*/
        int x = pIdx->aiColumn[j];
        if( x<BMS-1 ){
          m &= ~(((Bitmask)1)<<x);
        }
      }
      if( m==0 ){	/*如果所有列都在索引中*/
        wsFlags |= WHERE_IDX_ONLY;  /*设置WHERE_IDX_ONLY，标志是一个覆盖索引*/
      }else{
        bLookup = 1;  /*不是一个覆盖索引*/
      }
    }

    /*
    ** Estimate the number of rows of output.  For an "x IN (SELECT...)"
    ** constraint, do not let the estimate exceed half the rows in the table.
    **
    ** 估计输出的行数。对于一个"x IN (SELECT...)"约束，不要让估计超过表中一半的行。
    */
    nRow = (double)(aiRowEst[nEq] * nInMul);
    if( bInEst && nRow*2>aiRowEst[0] ){
      nRow = aiRowEst[0]/2;
      nInMul = (int)(nRow / aiRowEst[nEq]);
    }

#ifdef SQLITE_ENABLE_STAT3
    /* 
	** If the constraint is of the form x=VALUE or x IN (E1,E2,...)
    ** and we do not think that values of x are unique and if histogram
    ** data is available for column x, then it might be possible
    ** to get a better estimate on the number of rows based on
    ** VALUE and how common that value is according to the histogram.
    **
    ** 如果约束是x=VALUE or x IN (E1,E2,...)形式,我们认为x的值不是唯一的,
    ** 如果直方图数据对于列x是可用的，那么基于VALUE可能得到更好的行数估计,
    ** 并根据直方图得到VALUE的频率。
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
	**
    ** 调整输出行的数量，以反映范围约束排出的行数。
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
    **
    ** 运行在真实SQLite数据库上的实例显示，在表或索引中做一个二分查找来定位一行所需的时间，
    ** 大概是在表或索引中从一行移动到下一行所需时间的log10(N)倍。随着记录的大小成为一个
    ** 重要因素，实际时间会有所不同。记录越多，移动和查找都越慢，可能是因为在一个页面中装入
	** 的记录越少，因此需要加载越多的页面。
    **
    ** The ANALYZE command and the sqlite_stat1 and sqlite_stat3 tables do
    ** not give us data on the relative sizes of table and index records.
    ** So this computation assumes table records are about twice as big
    ** as index records
    **
    ** 分析命令和sqlite_stat1，sqlite_stat3表在表的大小和索引记录方面没有提供数据。
    ** 因此，这个计算假定表记录大约是索引记录的两倍。
    */
    if( (wsFlags & WHERE_NOT_FULLSCAN)==0 ){
      /* The cost of a full table scan is a number of move operations equal
      ** to the number of rows in the table.
      **
      ** 一个全表扫描的代价是等值于表中的行数的一系列移动操作。
      **
      ** We add an additional 4x penalty to full table scans.  This causes
      ** the cost function to err on the side of choosing an index over
      ** choosing a full scan.  This 4x full-scan penalty is an arguable
      ** decision and one which we expect to revisit in the future.  But
      ** it seems to be working well enough at the moment.
      **
      ** 我们添加一个额外的4倍代价来完成全表扫描。这导致代价函数宁可选择一个索引，而不选择全表扫描。
      ** 4倍全表扫描代价是一个有争议的决定，我们希望在将来重新考虑它。但目前似乎工作得很好。
      */
      cost = aiRowEst[0]*4;
    }else{
      log10N = estLog(aiRowEst[0]);
      cost = nRow;
      if( pIdx ){
        if( bLookup ){
          /* For an index lookup followed by a table lookup:
          **   nInMul index searches to find the start of each index range
          **   + nRow steps through the index
          **   + nRow table searches to lookup the table entry using the rowid
          **
          ** 对于索引查找，随后表查找：
          ** nInMul索引搜索来查找每个索引范围的起始位置
          ** + nRow逐句遍历索引       
	      ** + nRow表搜索来查找使用rowid的表条目
          */
          cost += (nInMul + nRow)*log10N;
        }else{
          /* For a covering index:
          **   nInMul index searches to find the initial entry 
          **   + nRow steps through the index
          **
          ** 对于一个覆盖索引：
          ** nInMul索引搜索来查找初始项
		  ** + nRow逐句遍历索引
          */
          cost += nInMul*log10N;
        }
      }else{
        /* For a rowid primary key lookup:
        **  nInMult table searches to find the initial entry for each range
        **  + nRow steps through the table
        **
        ** 对于一个rowid主键查找：
        ** nInMult表搜索来查找每个范围的初始项
		** + nRow逐句遍历表
        */
        cost += nInMul*log10N;
      }
    }

    /* Add in the estimated cost of sorting the result.  Actual experimental
    ** measurements of sorting performance in SQLite show that sorting time adds
    ** C*N*log10(N) to the cost, where N is the number of rows to be sorted and
    ** C is a factor between 1.95 and 4.3.  We will split the difference and select C of 3.0. 
    **
    ** 添加排序结果的代价估算。SQLite排序性能的实际实验测量表明，排序时间增加了C*N*log10(N)，
    ** 其中N是要排序的行数，C是一个在1.95和4.3之间的因素。我们采取折中值，设C为3.0。
    */
    if( bSort ){
      cost += nRow*estLog(nRow)*3;
    }
    if( bDist ){
      cost += nRow*estLog(nRow)*3;
    }

    /**** Cost of using this index has now been computed 
	***** 现在计算使用这个索引的代价。 
	****/

    /* If there are additional constraints on this table that cannot
    ** be used with the current index, but which might lower the number
    ** of output rows, adjust the nRow value accordingly.  This only 
    ** matters if the current index is the least costly, so do not bother
    ** with this step if we already know this index will not be chosen.
    ** Also, never reduce the output row count below 2 using this step.
    **
    ** 如果这个表有附加的约束不能用于当前索引，然而这个约束可能会减少输出的行数，则相应地
    ** 调整nRow值。只有当当前索引是否是代价最低的，这步才重要。所以如果我们已经知道这个
	** 索引不会被选择，就不要理会这一步。另外，使用这一步，不能把输出行数减少到低于2。
    **
    ** It is critical that the notValid mask be used here instead of the
    ** notReady mask.  When computing an "optimal" index, the notReady mask 
    ** will only have one bit set - the bit for the current table. The notValid 
    ** mask, on the other hand, always has all bits set for tables that are not 
    ** in outer loops.  If notReady is used here instead of notValid, then a 
    ** optimal index that depends on inner joins loops might be selected even when 
    ** there exists an optimal index that has no such dependency.
    ** 
    ** 关键是在此处使用notValid掩码替代notReady掩码。当计算一个"最优"索引，notReady掩码
    ** 也许只有一个位设置为当前表。另一方面，notValid掩码总是将所有位设置为不在外循环中的表。
    ** 如果在这里使用notReady而不是notValid，那么选择一个基于内部连接循环的最优索引，
	** 即使存在一个独立的最优索引。
    */
    if( nRow>2 && cost<=pCost->rCost ){
      int k;                       /* Loop counter   循环计数器 */
      int nSkipEq = nEq;           /* Number of == constraints to skip   需要跳过的==约束数 */
      int nSkipRange = nBound;     /* Number of < constraints to skip   需要跳过的<约束数 */
      Bitmask thisTab;             /* Bitmap for pSrc   用于pSrc的位图 */

      thisTab = getMask(pWC->pMaskSet, iCur);
      for(pTerm=pWC->a, k=pWC->nTerm; nRow>2 && k; k--, pTerm++){
        if( pTerm->wtFlags & TERM_VIRTUAL ) continue;
        if( (pTerm->prereqAll & notValid)!=thisTab ) continue;
        if( pTerm->eOperator & (WO_EQ|WO_IN|WO_ISNULL) ){
          if( nSkipEq ){
            /* Ignore the first nEq equality matches since the index
            ** has already accounted for these. 
            **
            ** 忽略第一个nEq等式匹配，因为索引已经说明了这些。
            */
            nSkipEq--;
          }else{
            /* Assume each additional equality match reduces the result
            ** set size by a factor of 10. 
            **
            ** 假设每个附加的等式匹配将结果集大小减小10倍。
            */
            nRow /= 10;
          }
        }else if( pTerm->eOperator & (WO_LT|WO_LE|WO_GT|WO_GE) ){
          if( nSkipRange ){
            /* Ignore the first nSkipRange range constraints since the index
            ** has already accounted for these
            ** 
			** 忽略第一个nSkipRange范围约束，因为索引已经说明了这些。
            */
            nSkipRange--;
          }else{
            /* Assume each additional range constraint reduces the result
            ** set size by a factor of 3.  Indexed range constraints reduce
            ** the search space by a larger factor: 4.  We make indexed range
            ** more selective intentionally because of the subjective 
            ** observation that indexed range constraints really are more
            ** selective in practice, on average. 
            **
            ** 假设每个额外的约束范围将结果集大小减小3倍。索引范围约束将搜索空间减小4倍。
            ** 我们有意使索引范围更有选择性，是因为在主观观察中，通常情况下索引范围约束确实更有选择性。
            */
            nRow /= 3;
          }
        }else if( pTerm->eOperator!=WO_NOOP ){
          /* Any other expression lowers the output row count by half.
		  **
	      ** 任何其他表达式将输出行数减少了一半。 
	      */
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
    **
    ** 如果这个索引是目前为止最优的，在pCost结构中记录这个索引及它的代价。
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

    /* If there was an INDEXED BY clause, then only that one index is considered. 
    **
    ** 如果有一个INDEXED BY子句，那么只考虑这个索引。
    */
    if( pSrc->pIndex ) break;

    /* Reset masks for the next index in the loop.
	** 
    ** 为循环中的下一个索引重置掩码。
	*/
    wsFlagMask = ~(WHERE_ROWID_EQ|WHERE_ROWID_RANGE);
    eqTermMask = idxEqTermMask;
  }

  /* If there is no ORDER BY clause and the SQLITE_ReverseOrder flag
  ** is set, then reverse the order that the index will be scanned
  ** in. This is used for application testing, to help find cases
  ** where application behaviour depends on the (undefined) order that
  ** SQLite outputs rows in in the absence of an ORDER BY clause.  
  **
  ** 如果没有ORDER BY子句，且设置了SQLITE_ReverseOrder标志，那么逆反扫描这个
  ** 索引的顺序。这用于应用程序测试，帮助找到如下情况，在缺少ORDER BY子句的情况下，
  ** 应用程序的行为取决于SQLite输出行的(未定义)顺序。
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
** 查找访问表pSrc->pTab的查询计划。在WhereCost对象中写入最优的查询计划及它的代价，
** 并且作为函数的最后一个参数。这个函数可能会计算实表和虚表扫描的代价。
*/
static void bestIndex(
  Parse *pParse,              /* The parsing context   解析上下文 */
  WhereClause *pWC,           /* The WHERE clause   WHERE子句 */
  struct SrcList_item *pSrc,  /* The FROM clause term to search   要查找的FROM子句term */
  Bitmask notReady,           /* Mask of cursors not available for indexing   对索引无效的游标掩码 */
  Bitmask notValid,           /* Cursors not available for any purpose   游标对于任何情况都无效 */
  ExprList *pOrderBy,         /* The ORDER BY clause   ORDER BY子句 */
  WhereCost *pCost            /* Lowest cost query plan   最小代价查询计划 */
){
#ifndef SQLITE_OMIT_VIRTUALTABLE
  if( IsVirtual(pSrc->pTab) ){  /*如果是虚表*/
    sqlite3_index_info *p = 0;  /*定义索引对象*/
    bestVirtualIndex(pParse, pWC, pSrc, notReady, notValid, pOrderBy, pCost,&p);  /*得到最优查询计划*/
    if( p->needToFreeIdxStr ){
      sqlite3_free(p->idxStr);  /*释放索引分配的内存*/
    }
    sqlite3DbFree(pParse->db, p);	/*释放与某个特定的数据库连接相关联的内存*/
  }else   /*实表*/
#endif
  {
    bestBtreeIndex(pParse, pWC, pSrc, notReady, notValid, pOrderBy, 0, pCost);  /*得到最优查询计划*/
  }
}

/*
** Disable a term in the WHERE clause.  Except, do not disable the term
** if it controls a LEFT OUTER JOIN and it did not originate in the ON
** or USING clause of that join.
**
** 在WHERE子句中禁止一个term。如果它控制一个LEFT OUTER JOIN，并且它不来源于这个
** 连接的ON或USING子句，则不禁止term。
**
** Consider the term t2.z='ok' in the following queries:
** 考虑下面查询中的t2.z='ok'term: 
**   (1)  SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.x WHERE t2.z='ok'
**   (2)  SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.x AND t2.z='ok'
**   (3)  SELECT * FROM t1, t2 WHERE t1.a=t2.x AND t2.z='ok'
**
** The t2.z='ok' is disabled in the in (2) because it originates
** in the ON clause.  The term is disabled in (3) because it is not part
** of a LEFT OUTER JOIN.  In (1), the term is not disabled.
**
** 在(2)中禁止t2.z='ok'，因为它来源于ON子句；
** 在(3)中禁止t2.z='ok'，因为它不是LEFT OUTER JION的一部分；
** 在(1)中，t2.z='ok'可以使用。   
**
** IMPLEMENTATION-OF: R-24597-58655 No tests are done for terms that are
** completely satisfied by indices.
**
** 对于terms不做测试，这些terms通过索引完全可以满足。
**
** Disabling a term causes that term to not be tested in the inner loop
** of the join.  Disabling is an optimization.  When terms are satisfied
** by indices, we disable them to prevent redundant tests in the inner
** loop.  We would get the correct results if nothing were ever disabled,
** but joins might run a little slower.  The trick is to disable as much
** as we can without disabling too much.  If we disabled in (1), we'd get
** the wrong answer.  See ticket #813.
**
** 禁用一个term子句使得这个term在连接的内部循环不被测试。禁用是一个优化。当索引满足terms，
** 则禁用这些terms，以防止内部循环的冗余测试。如果没有terms被禁用，我们将得到正确结果，
** 但连接运行会有点慢。关键是尽可能多的禁用，不要禁用太多。如果我们在(1)中禁用，则将得到错误答案。
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
**
** 编码一个OP_Affinity操作码，把列关联性字符串类型的zAff应用到从基址开始的n寄存器。
**  
** As an optimization, SQLITE_AFF_NONE entries (which are no-ops) at the
** beginning and end of zAff are ignored.  If all entries in zAff are
** SQLITE_AFF_NONE, then no code gets generated.
** 
** 作为一个优化，忽略zAff的开始和结尾的SQLITE_AFF_NONE条目(no-ops)。
** 如果zAff中的所有条目都是SQLITE_AFF_NONE，那么没有代码生成。
** 
** This routine makes its own copy of zAff so that the caller is free
** to modify zAff after this routine returns.
**
** 这个程序复制zAff自己的副本，以便在这个程序返回后调用者可以自由修改zAff。
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
  **
  ** 调整基址和n寄存器，跳过关联性字符串的开始和结尾处的SQLITE_AFF_NONE条目。
  */
  while( n>0 && zAff[0]==SQLITE_AFF_NONE ){
    n--;
    base++;
    zAff++;
  }
  while( n>1 && zAff[n-1]==SQLITE_AFF_NONE ){
    n--;
  }

  /* Code the OP_Affinity opcode if there is anything left to do. 
  ** 
  ** 如果还有事情可做，编码OP_Affinity操作码。
  */
  if( n>0 ){
    sqlite3VdbeAddOp2(v, OP_Affinity, base, n);
    sqlite3VdbeChangeP4(v, -1, zAff, n);
    sqlite3ExprCacheAffinityChange(pParse, base, n);
  }
}


/*
** Generate code for a single equality term of the WHERE clause.  An equality
** term can be either X=expr or X IN (...).   pTerm is the term to be coded.
**
** 为WHERE子句中的一个等式term生成代码。一个等式term可以是X=expr or X IN (...)。pTerm是需要编码的term。
** 
** The current value for the constraint is left in register iReg.
**
** 约束的当前值存储在寄存器iReg中。
** 
** For a constraint of the form X=expr, the expression is evaluated and its
** result is left on the stack.  For constraints of the form X IN (...)
** this routine sets up a loop that will iterate over all values of X.
** 
** 对于X=expr形式的约束，计算表达式,其结果存储在堆栈上。
** 对于X IN (...)形式的约束，这个程序设置一个循环遍历X的所有值。
*/
static int codeEqualityTerm(
  Parse *pParse,      /* The parsing context   解析上下文 */
  WhereTerm *pTerm,   /* The term of the WHERE clause to be coded  需要编码的WHERE子句的term */
  WhereLevel *pLevel, /* When level of the FROM clause we are working on   正在运行的FROM子句*/
  int iTarget         /* Attempt to leave results in this register   尝试在这个寄存器中存储结果 */
){
  Expr *pX = pTerm->pExpr;
  Vdbe *v = pParse->pVdbe;
  int iReg;                  /* Register holding results  存储结果的寄存器 */

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
** Generate code that will evaluate all == and IN constraints for an index.
**
** 生成代码评估一个索引的所有==和IN约束。
**
** For example, consider table t1(a,b,c,d,e,f) with index i1(a,b,c).
** Suppose the WHERE clause is this:  a==5 AND b IN (1,2,3) AND c>5 AND c<10
** The index has as many as three equality constraints, but in this
** example, the third "c" value is an inequality.  So only two 
** constraints are coded.  This routine will generate code to evaluate
** a==5 and b IN (1,2,3).  The current values for a and b will be stored
** in consecutive registers and the index of the first register is returned.
**
** 例如，考虑带有索引i1(a,b,c)的表t1(a,b,c,d,e,f)。假设where子句是：a==5 AND b IN (1,2,3) AND c>5 AND c<10。
** 这个索引至多有3个等式约束，但是在这里例子中。第三个“c”是不等式。所以只有两个等式约束被编码。
** 这个程序生成代码评估a==5 and b IN (1,2,3)。a和b的当前值存储在连续的寄存器中，返回第一个寄存器的索引。
**
** In the example above nEq==2.  But this subroutine works for any value
** of nEq including 0.  If nEq==0, this routine is nearly a no-op.
** The only thing it does is allocate the pLevel->iMem memory cell and
** compute the affinity string.
**
** 在上面示例中nEq==2。但是这个子程序作用于nEq的任意值，包含0。如果nEq==0，这个程序
** 几乎是无操作。它唯一的操作是分配pLevel->iMem的存储单元，并计算关联性字符串。
** 
** This routine always allocates at least one memory cell and returns
** the index of that memory cell. The code that calls this routine will 
** use that memory cell to store the termination key value of the loop. 
** If one or more IN operators appear, then this routine allocates an 
** additional nEq memory cells for internal use.
**
** 这个程序总是至少分配一个存储单元并返回存储单元的索引。这个编码调用这个程序，
** 将使用该存储单元来存储循环的终止键值。如果出现一个或多个IN操作符，
** 那么这个程序分配一个附加的nEq存储单元供内部使用。
**
** Before returning, *pzAff is set to point to a buffer containing a
** copy of the column affinity string of the index allocated using
** sqlite3DbMalloc(). Except, entries in the copy of the string associated
** with equality constraints that use NONE affinity are set to
** SQLITE_AFF_NONE. This is to deal with SQL such as the following:
**
** 在返回前，*pzAff设置为指向一个包含索引的列关联性字符串拷贝的缓冲区，该缓冲区使用
** sqlite3DbMalloc()分配。关联性字符串的拷贝中的条目与使用NONE关联性的等式约束有关，
** 则设置为SQLITE_AFF_NONE。如下方式处理SQL:
** 
**   CREATE TABLE t1(a TEXT PRIMARY KEY, b);
**   SELECT ... FROM t1 AS t2, t1 WHERE t1.a = t2.b;
**
** In the example above, the index on t1(a) has TEXT affinity. But since
** the right hand side of the equality constraint (t2.b) has NONE affinity,
** no conversion should be attempted before using a t2.b value as part of
** a key to search the index. Hence the first byte in the returned affinity
** string in this example would be set to SQLITE_AFF_NONE.
**
** 在上面的例子中。表t1(a)上的索引有TEXT关联性。但是因为等式约束的右边(t2.b)有NONE关联性，
** 在使用t2.b值作为搜索索引的主键的一部分之前，不用进行转换。
** 因此在这个例子中，返回关联性字符串的第一个字节将会设置为SQLITE_AFF_NONE。
*/
static int codeAllEqualityTerms(
  Parse *pParse,        /* Parsing context  解析上下文 */
  WhereLevel *pLevel,   /* Which nested loop of the FROM we are coding  对FROM的嵌套循环进行编码 */
  WhereClause *pWC,     /* The WHERE clause   WHERE子句 */
  Bitmask notReady,     /* Which parts of FROM have not yet been coded   FROM中还未编码的部分 */
  int nExtraReg,        /* Number of extra registers to allocate   需要额外分配的寄存器数 */
  char **pzAff          /* OUT: Set to point to affinity string   设置指向关联性字符串 */
){
  int nEq = pLevel->plan.nEq;   /* The number of == or IN constraints to code  需要编码的==或IN约束数目 */
  Vdbe *v = pParse->pVdbe;      /* The vm under construction  正在创建的VM */
  Index *pIdx;                  /* The index being used for this loop  用于这个循环的索引 */
  int iCur = pLevel->iTabCur;   /* The cursor of the table  表游标 */
  WhereTerm *pTerm;             /* A single constraint term  一个约束term */
  int j;                        /* Loop counter  循环计数器 */
  int regBase;                  /* Base register  基址寄存器 */
  int nReg;                     /* Number of registers to allocate  分配的寄存器数 */
  char *zAff;                   /* Affinity string to return  返回的关联性字符串 */

  /* This module is only called on query plans that use an index. 
  ** 这个模块只是用于使用索引的查询计划。
  */
  assert( pLevel->plan.wsFlags & WHERE_INDEXED );
  pIdx = pLevel->plan.u.pIdx;

  /* Figure out how many memory cells we will need then allocate them.
  **
  ** 算出需要多少存储单元，然后分配它们。
  */
  regBase = pParse->nMem + 1;
  nReg = pLevel->plan.nEq + nExtraReg;
  pParse->nMem += nReg;

  zAff = sqlite3DbStrDup(pParse->db, sqlite3IndexAffinityStr(v, pIdx));
  if( !zAff ){
    pParse->db->mallocFailed = 1;
  }

  /* Evaluate the equality constraints
  **
  ** 评估等式约束
  */
  assert( pIdx->nColumn>=nEq );
  for(j=0; j<nEq; j++){
    int r1;
    int k = pIdx->aiColumn[j];
    pTerm = findTerm(pWC, iCur, k, notReady, pLevel->plan.wsFlags, pIdx);
    if( pTerm==0 ) break;
    /* The following true for indices with redundant columns. 
    **
    ** 以下情况对于有冗余列的索引是正确的。
	**
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
**
** 这个程序辅助下面的explainIndexRange()程序。
**
** pStr holds the text of an expression that we are building up one term at a time.
** This routine adds a new term to the end of the expression. Terms are separated
** by AND so add the "AND" text for second and subsequent terms only.
** 
** 当我们每次构建一个term时，用pStr保存表达式的内容。这个程序在表达式的末尾增添一个新term。
** Terms是根据AND分隔的，因此只为第二个和后续的terms添加"AND"文本.
*/
static void explainAppendTerm(
  StrAccum *pStr,             /* The text expression being built  构建的文本表达式 */
  int iTerm,                  /* Index of this term.  First is zero  这个term的索引，第一个是0 */
  const char *zColumn,        /* Name of the column   列名 */
  const char *zOp             /* Name of the operator   操作符名 */
){
  if( iTerm ) sqlite3StrAccumAppend(pStr, " AND ", 5);  /* 从"AND添加5个字节的文本到pStr。*/
  sqlite3StrAccumAppend(pStr, zColumn, -1);  /* 从zColumn添加-1个字节的文本到pStr。*/
  sqlite3StrAccumAppend(pStr, zOp, 1);   /* 从zOp添加1个字节的文本到pStr。*/
  sqlite3StrAccumAppend(pStr, "?", 1);   /* 从"?"添加1个字节的文本到pStr。*/
}

/*
** Argument pLevel describes a strategy for scanning table pTab. This 
** function returns a pointer to a string buffer containing a description
** of the subset of table rows scanned by the strategy in the form of an
** SQL expression. Or, if all rows are scanned, NULL is returned.
**
** 参数pLevel描述一个扫描表pTab的策略。这个函数返回一个指针，指向一个字符串缓冲区,
** 这个字符串缓冲区包含一个关于表的行的子集的描述，这个子集通过某个策略扫描得到。这个
** 策略是一个SQL表达式的形式。或者，如果扫描所有行，返回NULL.
**
** For example, if the query:
** 例如，如果如下查询： 
**   SELECT * FROM t1 WHERE a=1 AND b>2;
** 
** is run and there is an index on (a, b), then this function returns a
** string similar to:
** 在运行并且在(a,b)上有索引，那么这个函数返回如下形式的字符串：  
**   "a=? AND b>?"
**
** The returned pointer points to memory obtained from sqlite3DbMalloc().
** It is the responsibility of the caller to free the buffer when it is
** no longer required.
**
** 返回的指针指向从sqlite3DbMalloc()获得的内存。当它不再被请求时，调用者负责释放缓冲区。
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

/* This function is a no-op unless currently processing an EXPLAIN QUERY PLAN
** command. If the query being compiled is an EXPLAIN QUERY PLAN, a single
** record is added to the output to describe the table scan strategy in pLevel.
**
** 这个函数无操作，除非当前执行一个EXPLAIN QUERY PLAN命令。如果查询被编译为一个
** EXPLAIN QUERY PLAN，则添加一个记录到输出，来描述在pLevel中的表扫描策略。
*/
static void explainOneScan(
  Parse *pParse,                  /* Parse context  解析上下文 */
  SrcList *pTabList,              /* Table list this loop refers to  这个循环引用的表列表 */
  WhereLevel *pLevel,             /* Scan to write OP_Explain opcode for  在此扫描写入OP_Explain操作码 */
  int iLevel,                     /* Value for "level" column of output   输出的"level"列的值 */
  int iFrom,                      /* Value for "from" column of output   输出的"from"列的值 */
  u16 wctrlFlags                  /* Flags passed to sqlite3WhereBegin()   传给sqlite3WhereBegin()的标志 */
){
  if( pParse->explain==2 ){
    u32 flags = pLevel->plan.wsFlags;
    struct SrcList_item *pItem = &pTabList->a[pLevel->iFrom];
    Vdbe *v = pParse->pVdbe;      /* VM being constructed  创建的VM */
    sqlite3 *db = pParse->db;     /* Database handle   数据库句柄 */
    char *zMsg;                   /* Text to add to EQP output  添加到EQP输出的文本 */
    sqlite3_int64 nRow;           /* Expected number of rows visited by scan  通过扫描访问的行数 */
    int iId = pParse->iSelectId;  /* Select id (left-most output column)   选中的id(最左边的输出列) */
    int isSearch;                 /* True for a SEARCH. False for SCAN.  是查找则为TRUE，扫描则为FALSE */

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
**
** 为由pWInfo描述的WHERE子句执行中的第i级循环的起始生成代码。
*/
static Bitmask codeOneLoopStart(
  WhereInfo *pWInfo,   /* Complete information about the WHERE clause   WHERE子句的完整信息 */
  int iLevel,          /* Which level of pWInfo->a[] should be coded  需要编码的pWInfo->a[]的level */
  u16 wctrlFlags,      /* One of the WHERE_* flags defined in sqliteInt.h  在sqliteInt.h中定义的WHERE_*标志中的一个 */
  Bitmask notReady     /* Which tables are currently available   哪个表当前是有效的 */
){
  int j, k;            /* Loop counters  循环计数器 */
  int iCur;            /* The VDBE cursor for the table  表的VDBE游标 */
  int addrNxt;         /* Where to jump to continue with the next IN case   在此跳转继续下一个IN的地址 */
  int omitTable;       /* True if we use the index only  如果只使用索引则为TRUE */
  int bRev;            /* True if we need to scan in reverse order   如果需要倒序扫描则为TRUE */
  WhereLevel *pLevel;  /* The where level to be coded   要编码的where中写入新索引的地方 */
  WhereClause *pWC;    /* Decomposition of the entire WHERE clause   整个WHERE子句的分解 */
  WhereTerm *pTerm;               /* A WHERE clause term   一个WHERE子句的term */
  Parse *pParse;                  /* Parsing context   解析上下文 */
  Vdbe *v;                        /* The prepared stmt under constructions  在构建中的stmt */
  struct SrcList_item *pTabItem;  /* FROM clause term being coded  正在编码的FROM子句term */
  int addrBrk;                    /* Jump here to break out of the loop  跳出循环时的位置 */
  int addrCont;                   /* Jump here to continue with next cycle  继续下一次循环的位置 */
  int iRowidReg = 0;        /* Rowid is stored in this register, if not zero  如果不为0，Rowid存储在这个寄存器中 */
  int iReleaseReg = 0;      /* Temp register to free before returning  在返回前要释放的临时寄存器 */

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
  ** Jump to cont to go immediately to the next iteration of the loop.
  **
  ** 为当前循环的"break"和"continue"指令创建标签。跳到addrBrk来跳出循环。
  ** 跳到addrCont就立即进入循环的下一轮。
  **
  ** When there is an IN operator, we also have a "addrNxt" label that  means
  ** to continue with the next IN value combination.  When there are no IN
  ** operators in the constraints, the "addrNxt" label is the same as "addrBrk".
  ** 	   
  ** 当有一个IN运算符，也有一个"addrNxt"标签表示继续下一个IN值组合。
  ** 当约束中没有IN运算符时，"addrNxt"标签和"addrBrk"作用相同。
  */
  addrBrk = pLevel->addrBrk = pLevel->addrNxt = sqlite3VdbeMakeLabel(v);
  addrCont = pLevel->addrCont = sqlite3VdbeMakeLabel(v);

  /* If this is the right table of a LEFT OUTER JOIN, allocate and
  ** initialize a memory cell that records if this table matches any
  ** row of the left table of the join.
  **
  ** 如果这是一个LEFT OUTER JOIN的右表，如果此表匹配JION中左表的任意行，
  ** 则分配并初始化一个内存单元来记录。
  */
  if( pLevel->iFrom>0 && (pTabItem[0].jointype & JT_LEFT)!=0 ){
    pLevel->iLeftJoin = ++pParse->nMem;
    sqlite3VdbeAddOp2(v, OP_Integer, 0, pLevel->iLeftJoin);
    VdbeComment((v, "init LEFT JOIN no-match flag"));
  }

#ifndef SQLITE_OMIT_VIRTUALTABLE
  if(  (pLevel->plan.wsFlags & WHERE_VIRTUALTABLE)!=0 ){
    /* Case 0: The table is a virtual-table.  Use the VFilter and VNext
    **         to access the data.
    **
    ** Case 0: 表是一个虚表。使用VFilter和VNext访问数据。
    */
    int iReg;   /* P3 Value for OP_VFilter    OP_VFilter的P3值 */
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
    /* Case 1: We can directly reference a single row using an
    **         equality comparison against the ROWID field.  Or we
    **         reference multiple rows using a "rowid IN (...)" construct.
    **          
    **
    ** Case 1: 我们可以通过使用等式比较与ROWID字段进行比较来引用一个单一行。
    **         或通过使用"rowid IN (...)"结构引用多行。
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
    /* Case 2: We have an inequality comparison against the ROWID field.
    **
    ** Case 2: 有一个与ROWID字段进行比较的不等式比较。
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
      Expr *pX;             /* The expression that defines the start bound  定义了开始范围的表达式 */
      int r1, rTemp;        /* Registers for holding the start boundary   保存开始范围的寄存器 */

      /* The following constant maps TK_xx codes into corresponding 
      ** seek opcodes.  It depends on a particular ordering of TK_xx。
      **
      ** 下面的常量将TK_xx映射到相应的搜索操作码。这取决于TK_xx的一个特定顺序。
      */
      const u8 aMoveOp[] = {
           /* TK_GT */  OP_SeekGt,
           /* TK_LE */  OP_SeekLe,
           /* TK_LT */  OP_SeekLt,
           /* TK_GE */  OP_SeekGe
      };
      assert( TK_LE==TK_GT+1 );      /* Make sure the ordering.. 确保顺序 */
      assert( TK_LT==TK_GT+2 );      /*  ... of the TK_xx values...  */
      assert( TK_GE==TK_GT+3 );      /*  ... is correcct.   判定是正确的*/

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
	  
    /* Case 3: A scan using an index.  使用索引的扫描
    **
    **         The WHERE clause may contain zero or more equality 
    **         terms ("==" or "IN" operators) that refer to the N
    **         left-most columns of the index. It may also contain
    **         inequality constraints (>, <, >= or <=) on the indexed
    **         column that immediately follows the N equalities. Only 
    **         the right-most column can be an inequality - the rest must
    **         use the "==" and "IN" operators. For example, if the index
    **         is on (x,y,z), then the following clauses are all optimized:
    **         
	**         where子句可能包含0个或多个等式terms("==" or "IN"操作符)涉及到N个索引。
    **         最左边的列在索引列上可能也包含不等式约束(>, <, >= or <=)，紧随N个等式之后。
    **         只有最右边的列可以是一个不等式，其余的必须使用"=="和"IN"运算符。
    **         例如，如过(x,y,z)上有索引，下面的子句都可以进行优化:    
    **            x=5
    **            x=5 AND y=10
    **            x=5 AND y<10
    **            x=5 AND y>5 AND y<10
    **            x=5 AND y=5 AND z<=10
    **
    **         The z<10 term of the following cannot be used, only
    **         the x=5 term:
	**   
    **         下面的例子中z<10不能优化，只有x=5可以:      
    **            x=5 AND z<10
    **
    **         N may be zero if there are inequality constraints.
    **         If there are no inequality constraints, then N is at
    **         least one.
	**
	**         如果有不等式约束，那么N可能为0。如果没有不等式约束，N至少为1.   
    **
    **         This case is also used when there are no WHERE clause
    **         constraints but an index is selected anyway, in order
    **         to force the output order to conform to an ORDER BY.
    **                  
    **         当没有WHERE子句约束，但选出了一个索引，这种情况也适用，为了使输出顺序符合ORDER BY。
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
    int nEq = pLevel->plan.nEq;  /* Number of == or IN terms   ==或IN terms的数目 */
    int isMinQuery = 0;          /* If this is an optimized SELECT min(x)..  如果这是一个优化的SELECT min(x).. */
    int regBase;                 /* Base register holding constraint values  基址寄存器存储约束值 */
    int r1;                      /* Temp register   临时寄存器 */
    WhereTerm *pRangeStart = 0;  /* Inequality constraint at range start   范围初始的不等式约束 */
    WhereTerm *pRangeEnd = 0;    /* Inequality constraint at range end   范围末尾的不等式约束 */
    int startEq;                 /* True if range start uses ==, >= or <=   如果范围起始使用==, >= or <=，那则为TRUE */
    int endEq;                   /* True if range end uses ==, >= or <=   如果范围结尾使用==, >= or <=，则为TRUE */
    int start_constraints;       /* Start of range is constrained   范围起始是有约束的 */
    int nConstraint;             /* Number of constraint terms   约束terms的数目 */
    Index *pIdx;                 /* The index we will be using   要使用的索引 */
    int iIdxCur;                 /* The VDBE cursor for the index   索引的VDBE游标 */
    int nExtraReg = 0;           /* Number of extra registers needed   需要的额外寄存器的数量 */
    int op;                      /* Instruction opcode  指令操作码 */
    char *zStartAff;             /* Affinity for start of range constraint  范围约束起始的关联性 */
    char *zEndAff;               /* Affinity for end of range constraint  范围约束结尾的关联性性 */

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
    **
    ** 如果这个循环满足一个排序顺序(pOrderBy)的请求，则传递给这个函数实现一个"SELECT min(x) ..."查询，
    ** 那么调用者只允许循环运行一次迭代。这意味着返回的第一行不能有存储在'x'中的NULL值。
    ** 如果列'x'是索引中nEq个等式约束后第一个，这会请求一些特别处理。
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

    /* Find any inequality constraint terms for the start and end of the range. 
    **
    ** 为范围的起始和结尾查找不等式约束terms
    */
    if( pLevel->plan.wsFlags & WHERE_TOP_LIMIT ){
      pRangeEnd = findTerm(pWC, iCur, k, notReady, (WO_LT|WO_LE), pIdx);
      nExtraReg = 1;
    }
    if( pLevel->plan.wsFlags & WHERE_BTM_LIMIT ){
      pRangeStart = findTerm(pWC, iCur, k, notReady, (WO_GT|WO_GE), pIdx);
      nExtraReg = 1;
    }

    /* Generate code to evaluate all constraint terms using == or IN and
    ** store the values of those terms in an array of registers starting at regBase.
    ** 
    ** 生成代码来计算所有使用==或IN约束的terms，并把这些terms的值存储在由regBase开始的一组寄存器中。
    */
    regBase = codeAllEqualityTerms(
        pParse, pLevel, pWC, notReady, nExtraReg, &zStartAff
    );
    zEndAff = sqlite3DbStrDup(pParse->db, zStartAff);
    addrNxt = pLevel->addrNxt;

    /* If we are doing a reverse order scan on an ascending index, or
    ** a forward order scan on a descending index, interchange the 
    ** start and end terms (pRangeStart and pRangeEnd).
    **
    ** 如果我们在升序索引上做一个后序扫描，或者在一个降序索引上做一个先序扫描，
	** 交换开始和结束terms(pRangeStart and pRangeEnd).
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

    /* Seek the index cursor to the start of the range.
    **
	** 查找范围起始的索引游标。
	*/
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
          ** applied to the operands, set the affinity to apply to pRight to 
          ** SQLITE_AFF_NONE.
		  **	
          ** 由于应用在运算对象上的比较没有转换，设置pRight的关联性为SQLITE_AFF_NONE。
          */
          zStartAff[nEq] = SQLITE_AFF_NONE;
        }
        if( sqlite3ExprNeedsNoAffinityChange(pRight, zStartAff[nEq]) ){
          zStartAff[nEq] = SQLITE_AFF_NONE;
        }
      }  
      nConstraint++;
      testcase( pRangeStart->wtFlags & TERM_VIRTUAL );  /* EV: R-30575-11662 */
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

    /* Load the value for the inequality constraint at the end of the range (if any).
    **
    ** 在范围的结尾加载不等式约束的值(若存在)。
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
          ** SQLITE_AFF_NONE. 
		  **	
          ** 由于应用在运算对象上的比较没有转换，设置pRight的关联性为SQLITE_AFF_NONE。
          */
          zEndAff[nEq] = SQLITE_AFF_NONE;
        }
        if( sqlite3ExprNeedsNoAffinityChange(pRight, zEndAff[nEq]) ){
          zEndAff[nEq] = SQLITE_AFF_NONE;
        }
      }  
      codeApplyAffinity(pParse, regBase, nEq+1, zEndAff);
      nConstraint++;
      testcase( pRangeEnd->wtFlags & TERM_VIRTUAL );  /* EV: R-30575-11662 */
    }
    sqlite3DbFree(pParse->db, zStartAff);
    sqlite3DbFree(pParse->db, zEndAff);

    /* Top of the loop body   循环体的顶部 */
    pLevel->p2 = sqlite3VdbeCurrentAddr(v);

    /* Check if the index cursor is past the end of the range. 
    ** 检查索引游标是否经过范围的结尾。 
	*/
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
    **
    ** 如果有不等式约束，检查表中有不等式约束的列的值是否为NULL.
    ** 如果是，跳到循环的下一个迭代。
    */
    r1 = sqlite3GetTempReg(pParse);
    testcase( pLevel->plan.wsFlags & WHERE_BTM_LIMIT );
    testcase( pLevel->plan.wsFlags & WHERE_TOP_LIMIT );
    if( (pLevel->plan.wsFlags & (WHERE_BTM_LIMIT|WHERE_TOP_LIMIT))!=0 ){
      sqlite3VdbeAddOp3(v, OP_Column, iIdxCur, nEq, r1);
      sqlite3VdbeAddOp2(v, OP_IsNull, r1, addrCont);
    }
    sqlite3ReleaseTempReg(pParse, r1);

    /* Seek the table cursor, if required 
    **  如果需要，查询表游标 
	*/
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
    ** 
    ** 记录用于结束循环的指令。禁用由于使用索引范围扫描而造成冗余的WHERE子句terms。
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
    /* Case 4: Two or more separately indexed terms connected by OR
    **         由OR连接的两个或更多的独立索引terms。
    ** Example: 例如
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
    ** 在这个例子中，有三个由OR连接的索引terms。循环的顶部如下:
    **          Null       1                # Zero the rowset in reg 1  在寄存器1中把rowset清零
    **
    ** Then, for each indexed term, the following. The arguments to
    ** RowSetTest are such that the rowid of the current row is inserted
    ** into the RowSet. If it is already present, control skips the
    ** Gosub opcode and jumps straight to the code generated by WhereEnd().
	**   
    ** 那么，对于接下来的每个索引term。传递给RowSetTest的参数是当前插入到RowSet的行的rowid. 
    ** 如果已经存在，控制跳过这个Gosub操作码，并直接跳到由WhereEnd()直接生成的代码。
    **        sqlite3WhereBegin(<term>)
    **          RowSetTest                  # Insert rowid into rowset  把rowid插入到rowset
    **          Gosub      2 A
    **        sqlite3WhereEnd()
    **
    ** Following the above, code to terminate the loop. Label A, the target
    ** of the Gosub above, jumps to the instruction right after the Goto.
	**  
    ** 接下来，编码来终止循环。Label A，上面Gosub的目标, 在Goto之后跳到相应的指令。
    **          Null       1                # Zero the rowset in reg 1  在寄存器1中把rowset清零
    **          Goto       B                # The loop is finished.   循环结束.
    **
    **       A: <loop body>                 # Return data, whatever.  无论如何，返回data,
    **
    **          Return     2                # Jump back to the Gosub  跳回到Gosub
    **
    **       B: <after the loop>
    */
    WhereClause *pOrWc;    /* The OR-clause broken out into subterms   分解为子terms的OR子句 */
    SrcList *pOrTab;       /* Shortened table list or OR-clause generation   缩短的表列表或OR子句 */
    Index *pCov = 0;             /* Potential covering index (or NULL)  可能的覆盖索引(或NULL) */
    int iCovCur = pParse->nTab++;  /* Cursor used for index scans (if any)  用于索引扫描的游标 */

    int regReturn = ++pParse->nMem;           /* Register used with OP_Gosub  使用OP_Gosub的寄存器 */
    int regRowset = 0;                        /* Register for RowSet object  用于RowSet对象的寄存器 */
    int regRowid = 0;                         /* Register holding rowid  保存rowid的寄存器 */
    int iLoopBody = sqlite3VdbeMakeLabel(v);  /* Start of loop body  循环体的开始 */
    int iRetInit;                             /* Address of regReturn init  regReturn地址初始化 */
    int untestedTerms = 0;             /* Some terms not completely tested 一些没完全测试的terms */
    int ii;                            /* Loop counter  循环计数器 */
    Expr *pAndExpr = 0;                /* An ".. AND (...)" expression  一个".. AND (...)"表达式 */
   
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
    **
    ** 在pOrTab中创建新的SrcList，这个SrcList包含通过这个循环扫描到在a[0]位置的表和
	** 在a[1..]位置所有notReady的表。
    */
    if( pWInfo->nLevel>1 ){
      int nNotReady;                 /* The number of notReady tables  notReady表的数目 */
      struct SrcList_item *origSrc;     /* Original list of tables  表的初始列表 */
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
    **
    ** 初始化rowset寄存器为NULL。一个SQL NULL等价于一个空rowset.
    **
    ** Also initialize regReturn to contain the address of the instruction 
    ** immediately following the OP_Return at the bottom of the loop. This
    ** is required in a few obscure LEFT JOIN cases where control jumps
    ** over the top of the loop into the body of it. In this case the 
    ** correct response for the end-of-loop code (the OP_Return) is to 
    ** fall through to the next instruction, just as an OP_Next does if
    ** called on an uninitialized cursor.
    **
    ** 同时初始化regReturn包含指令地址，这个regReturn紧跟循环底部的OP_Return之后。
    ** 这应用于一些麻烦的LEFT JOIN情况，要控制跳过循环的开头进入循环内部。在这种情况下，
	** 对于end-of-loop编码的正确响应是跳过下一个指令，就像在一个未初始化的游标中调用一个OP_Next一样。
    */
    if( (wctrlFlags & WHERE_DUPLICATES_OK)==0 ){
      regRowset = ++pParse->nMem;
      regRowid = ++pParse->nMem;
      sqlite3VdbeAddOp2(v, OP_Null, 0, regRowset);
    }
    iRetInit = sqlite3VdbeAddOp2(v, OP_Integer, 0, regReturn);

    /* If the original WHERE clause is z of the form:  (x1 OR x2 OR ...) AND y.
    ** Then for every term xN, evaluate as the subexpression: xN AND z
    ** That way, terms in y that are factored into the disjunction will
    ** be picked up by the recursive calls to sqlite3WhereBegin() below.
    **
    ** 如果原始的WHERE子句z是这种形式:(x1 OR x2 OR ...) AND y。那么对于每一个term xN,计算子表达式：xN AND z。
    ** 那样，在y中的terms通过递归调用下面的sqlite3WhereBegin()来进行分解。
    **
    ** Actually, each subexpression is converted to "xN AND w" where w is
    ** the "interesting" terms of z - terms that did not originate in the
    ** ON or USING clause of a LEFT JOIN, and terms that are usable as indices.
    **
    ** 实际上，每个子表达式转化为"xN AND w"，其中w是z-terms的"interesting"terms，
    ** 不是源于LEFT JOIN的ON或USING子句，并且terms可以用作索引。
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
        WhereInfo *pSubWInfo;          /* Info for single OR-term scan  一个OR-term扫描的信息 */
        Expr *pOrExpr = pOrTerm->pExpr;
        if( pAndExpr ){
          pAndExpr->pLeft = pOrExpr;
          pOrExpr = pAndExpr;
        }
		
        /* Loop through table entries that match term pOrTerm.
        ** 循环遍历表中匹配pOrTerm的条目。  
		*/
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
		  **
          ** pSubWInfo->untestedTerms标志表示这个OR term包含了来自一个notReady表的
          ** 一个或多个AND term。来自notReady表的terms不能被测试，并且稍后将需要测试。
          */
          if( pSubWInfo->untestedTerms ) untestedTerms = 1;

          /* If all of the OR-connected terms are optimized using the same
          ** index, and the index is opened using the same cursor number
          ** by each call to sqlite3WhereBegin() made by this loop, it may
          ** be possible to use that index as a covering index.
          ** 
          ** 如果所有OR-connected terms使用相同的索引优化，并且通过每次调用由循环产生的
          ** sqlite3WhereBegin()来使用相同的游标编号打开索引，那么可以把这个索引用作覆盖索引。
          ** 
          ** If the call to sqlite3WhereBegin() above resulted in a scan that
          ** uses an index, and this is either the first OR-connected term
          ** processed or the index is the same as that used by all previous
          ** terms, set pCov to the candidate covering index. Otherwise, set 
          ** pCov to NULL to indicate that no candidate covering index will be available.
          **
          ** 如果上面的调用sqlite3WhereBegin()产生一个使用索引的扫描，并且这是处理的第一个
          ** OR-connected term，或者使用的索引与前面所有的terms使用的相同，则把pCov设置为
          ** 候选的覆盖索引。否则，把pCov设置为NULL，来表示没有候选的覆盖索引可供使用。
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

          /* Finish the loop through table entries that match term pOrTerm.
          ** 完成对表中匹配pOrTerm的条目的循环遍历 
		  */
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
    /* Case 5: There is no usable index.  We must do a complete
    **         scan of the entire table.
    **
    ** Case 5: 没有可用的索引。必须做一个全表扫描。
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
  **
  ** 插入代码来测试所有子表达式，是否可以使用当前一组表来完整计算。
  **
  ** IMPLEMENTATION-OF: R-49525-50935 Terms that cannot be satisfied through
  ** the use of indices become tests that are evaluated against each row of
  ** the relevant input tables.
  **
  ** 不能通过使用索引来满足的terms成为评估相关输入表的每一行的测试。
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
  ** at least one row of the right table has matched the left table. 
  **
  ** 对于一个LEFT OUTER JOIN，生成代码来记录右表中至少有一行与左表相匹配的事实。
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
** overwrites the previous.  This information is used for testing and analysis only.
**
** 下面的变量保存一个描述查询计划的文本，该计划通过最新的调用sqlite3WhereBegin()生成。
** 每次调用WhereBegin重写先前的信息。这个信息只用于测试和分析。
*/
char sqlite3_query_plan[BMS*2*40];  /* Text of the join  连接的内容 */
static int nQPlan = 0;              /* Next free slow in _query_plan[]  释放下一个in _query_plan[]*/

#endif /* SQLITE_TEST */


/* Free a WhereInfo structure
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
**
** 生成循环的开始用于处理WHERE子句。返回值是一个指针,它指向一个包含终止循环所需信息的结构体。
** 然后，调用程序会根据这个函数的返回值唤醒sqlite3WhereEnd()来完成WHERE子句的处理。
**
** If an error occurs, this routine returns NULL.
**
** 如果发生错误，这个程序将返回NULL.
**
** The basic idea is to do a nested loop, one loop for each table in
** the FROM clause of a select.  (INSERT and UPDATE statements are the
** same as a SELECT with only a single table in the FROM clause.)  For
** example, if the SQL is this:
**
** 基本的思路是进行循环嵌套，查询语句的FROM子句中的每个表都做一个循环。
** (INSERT和UPDATE语句与在FROM语句中只有一个表的SELECT相同)。例如，如果SQL如下:
**       SELECT * FROM t1, t2, t3 WHERE ...;
**
** Then the code generated is conceptually like the following:
** 那么代码生成算法如下:
**      foreach row1 in t1 do       \    Code generated
**        foreach row2 in t2 do      |-- by sqlite3WhereBegin()
**          foreach row3 in t3 do   /
**            ...
**          end                     \    Code generated
**        end                        |-- by sqlite3WhereEnd()
**      end                         /
**
** Note that the loops might not be nested in the order in which they
** appear in the FROM clause if a different order is better able to make
** use of indices.  Note also that when the IN operator appears in
** the WHERE clause, it might result in additional nested loops for
** scanning through all values on the right-hand side of the IN.
**
** 注意:循环可能不是按它们在FROM子句中出现的顺序进行嵌套，因为可能有一个其他的嵌套顺序更适合使用索引。
** 还要注意;当WHERE子句中出现了IN操作符，它可能导致添加嵌套循环来扫描IN右边的所有值。
**
** There are Btree cursors associated with each table.  t1 uses cursor
** number pTabList->a[0].iCursor.  t2 uses the cursor pTabList->a[1].iCursor.
** And so forth.  This routine generates code to open those VDBE cursors
** and sqlite3WhereEnd() generates the code to close them.
**
** 有与每个表相关联Btree游标。t1使用游标pTabList->a[0].iCursor。t2使用游标pTabList->a[1].iCursor。等等
** 这个程序生成代码来打开这些VDBE游标，sqlite3WhereEnd()生成代码来关闭它们。
**
** The code that sqlite3WhereBegin() generates leaves the cursors named
** in pTabList pointing at their appropriate entries.  The [...] code
** can use OP_Column and OP_Rowid opcodes on these cursors to extract
** data from the various tables of the loop.
**
** sqlite3WhereBegin()生成的代码在pTabList中保存指定的游标指向它们适当的条目。
** [...]编码可以在这些游标上使用OP_Column和OP_Rowid操作码，来从循环的各个表中提取数据。
**
** If the WHERE clause is empty, the foreach loops must each scan their
** entire tables.  Thus a three-way join is an O(N^3) operation.  But if
** the tables have indices and there are terms in the WHERE clause that
** refer to those indices, a complete table scan can be avoided and the
** code will run much faster.  Most of the work of this routine is checking
** to see if there are indices that can be used to speed up the loop.
**
** 如果WHERE子句是空的，每一个循环必须每次扫描整个表。因此一个three-way连接是一个O(N^3)操作。
** 但是如果表有索引并且在WHERE子句中的terms与那些索引相关联，可以避免全表扫描，而代码运行的更快。
** 这个程序大部分的工作就是查看是否有可以使用的索引来加速循环。
**
** Terms of the WHERE clause are also used to limit which rows actually
** make it to the "..." in the middle of the loop.  After each "foreach",
** terms of the WHERE clause that use only terms in that loop and outer
** loops are evaluated and if false a jump is made around all subsequent
** inner loops (or around the "..." if the test occurs within the inner-most loop)
**
** WHERE子句的terms也被用于限制在循环的中部哪些行使它成为"...".
** 每次循环后，WHERE子句的terms只使用在那个循环和外部循环评估过的terms.
** 并且如果错误就跳过所有后续的内部循环(或如果测试发生在最内部循环中，那么就跳过"...")
**
** OUTER JOINS   外部链接
** 
** An outer join of tables t1 and t2 is conceptally coded as follows:
** 一个表t1和t2的外部连接算法如下: 
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
** ORDER BY CLAUSE PROCESSING   ORDER BY子句处理
**
** *ppOrderBy is a pointer to the ORDER BY clause of a SELECT statement,
** if there is one.  If there is no ORDER BY clause or if this routine
** is called from an UPDATE or DELETE statement, then ppOrderBy is NULL.
** 
** 如果有ORDER BY子句，那么*ppOrderBy是一个指针，指向一个SELECT语句的ORDER BY子句。
** 如果没有ORDER BY子句，或者是UPDATE或DELETE语句调用的这个程序，那么ppOrderBy为NULL。
** 
** If an index can be used so that the natural output order of the table
** scan is correct for the ORDER BY clause, then that index is used and
** *ppOrderBy is set to NULL.  This is an optimization that prevents an
** unnecessary sort of the result set if an index appropriate for the
** ORDER BY clause already exists.
** 
** 如果可以使用索引以便表扫描的原本输出顺序是根据ORDER BY子句矫正的，那么使用这个索引并将*ppOrderBy设置为NULL.
** 如果一个适用于ORDER BY子句的索引已经存在，可以进行优化防止结果集不必要的排序。 
**
** If the where clause loops cannot be arranged to provide the correct
** output order, then the *ppOrderBy is unchanged.
**
** 如果不能安排WHERE子句循环提供正确的输出顺序，那么不改变*ppOrderBy的值。
*/
WhereInfo *sqlite3WhereBegin(
  Parse *pParse,        /* The parser context   解析器上下文 */
  SrcList *pTabList,    /* A list of all tables to be scanned   被扫描的所有表的列表 */
  Expr *pWhere,         /* The WHERE clause   WHERE子句 */
  ExprList **ppOrderBy, /* An ORDER BY clause, or NULL   一个ORDER BY子句或NULL*/
  ExprList *pDistinct,  /* The select-list for DISTINCT queries - or NULL    DISTINCT查询的查询列表或NULL */
  u16 wctrlFlags,       /* One of the WHERE_* flags defined in sqliteInt.h   在sqliteInt.h中定义的WHERE_*中的一个 */
  int iIdxCur           /* If WHERE_ONETABLE_ONLY is set, index cursor number   如果设置了WHERE_ONETABLE_ONLY，索引游标编号 */
){
  int i;                     /* Loop counter   循环计数器 */
  int nByteWInfo;            /* Num. bytes allocated for WhereInfo struct   为WhereInfo结构分配的字节数 */
  int nTabList;              /* Number of elements in pTabList   在pTabList中的元素数 */
  WhereInfo *pWInfo;         /* Will become the return value of this function   这个函数的返回值 */
  Vdbe *v = pParse->pVdbe;   /* The virtual database engine   虚拟数据库引擎 */
  Bitmask notReady;          /* Cursors that are not yet positioned   还未确定位置的游标 */
  WhereMaskSet *pMaskSet;    /* The expression mask set   设置表达式掩码 */
  WhereClause *pWC;               /* Decomposition of the WHERE clause   WHERE子句的分解 */
  struct SrcList_item *pTabItem;  /* A single entry from pTabList    pTabList的一个条目 */
  WhereLevel *pLevel;             /* A single level in the pWInfo list   pWInfo列表中的一个level */
  int iFrom;                      /* First unused FROM clause element   第一个未使用的FROM子句元素 */
  int andFlags;              /* AND-ed combination of all pWC->a[].wtFlags  AND-ed所有的pWC->a[].wtFlags组合 */
  sqlite3 *db;               /* Database connection   数据库连接 */

  /* The number of tables in the FROM clause is limited by the number of
  ** bits in a Bitmask 
  **
  ** 由位掩码中的位数限制的FROM子句中的表的数目
  */
  testcase( pTabList->nSrc==BMS );	 /*用于覆盖测试*/
  if( pTabList->nSrc>BMS ){	  /*如果在FROM中的表数目大于位掩码中的位数*/
    sqlite3ErrorMsg(pParse, "at most %d tables in a join", BMS);	/*提示连接中最多只能有BMS个表*/
    return 0;
  }

  /* This function normally generates a nested loop for all tables in 
  ** pTabList.  But if the WHERE_ONETABLE_ONLY flag is set, then we should
  ** only generate code for the first table in pTabList and assume that
  ** any cursors associated with subsequent tables are uninitialized.
  **
  ** 通常情况下，这个函数为pTabList中的所有表生成一个嵌套循环。但是如果设置了WHERE_ONETABLE_ONLY标志，
  ** 那么我们只需要为pTabList中的第一个表生成代码，并且假设任何与后续的表相关的游标都是未初始化的。
  */
  nTabList = (wctrlFlags & WHERE_ONETABLE_ONLY) ? 1 : pTabList->nSrc;  /*如果只有一个表，那么nTabList就为1，否则等于pTabList->nSrc*/

  /* Allocate and initialize the WhereInfo structure that will become the
  ** return value. A single allocation is used to store the WhereInfo
  ** struct, the contents of WhereInfo.a[], the WhereClause structure
  ** and the WhereMaskSet structure. Since WhereClause contains an 8-byte
  ** field (type Bitmask) it must be aligned on an 8-byte boundary on
  ** some architectures. Hence the ROUND8() below.
  **
  ** 分配并初始化将成为返回值的WhereInfo数据结构。一个单独的分配被用于存储WhereInfo结构，
  ** WhereInfo.a[]的内容，WhereClause数据结构和WhereMaskSet数据结构。因为WhereClause包含
  ** 一个8字节的字段(type Bitmask)，它必须在结构上与一个8字节边界对齐。因此，ROUND8()如下。
  */
  db = pParse->db;	 /*连接数据库*/
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
  **
  ** 如果通过sqlite3_test_ctrl(SQLITE_TESTCTRL_OPTIMIZATIONS,...)设置了SQLITE_DistinctOpt，
  ** 那么就禁用DISTINCT优化。
  */
  if( db->flags & SQLITE_DistinctOpt ) pDistinct = 0;

  /* Split the WHERE clause into separate subexpressions where each
  ** subexpression is separated by an AND operator.
  **
  ** 把WHERE子句通过AND运算符分割成多个子表达式。
  */
  initMaskSet(pMaskSet);	//初始化WhereMaskSet对象
  whereClauseInit(pWC, pParse, pMaskSet, wctrlFlags);	//初始化pWC
  sqlite3ExprCodeConstants(pParse, pWhere); //预先计算在pWhere中的常量字表达式
  whereSplit(pWC, pWhere, TK_AND);  //把WHERE子句通过AND运算符分割成多个子表达式。 /* IMP: R-15842-53296 */
    
  /* Special case: a WHERE clause that is constant.  Evaluate the
  ** expression and either jump over all of the code or fall thru.
  **
  ** 特殊情况:一个WHERE子句是恒定的。评估表达式时，要么跳过所有的代码，要么通过。
  */
  if( pWhere && (nTabList==0 || sqlite3ExprIsConstantNotJoin(pWhere)) ){
    sqlite3ExprIfFalse(pParse, pWhere, pWInfo->iBreak, SQLITE_JUMPIFNULL);
    pWhere = 0;
  }

  /* Assign a bit from the bitmask to every term in the FROM clause.
  **
  ** 从位掩码中为FROM子句中的每个term分配一位。
  **
  ** When assigning bitmask values to FROM clause cursors, it must be
  ** the case that if X is the bitmask for the N-th FROM clause term then
  ** the bitmask for all FROM clause terms to the left of the N-th term
  ** is (X-1).   An expression from the ON clause of a LEFT JOIN can use
  ** its Expr.iRightJoinTable value to find the bitmask of the right table
  ** of the join.  Subtracting one from the right table bitmask gives a
  ** bitmask for all tables to the left of the join.  Knowing the bitmask
  ** for all tables to the left of a left join is important.  Ticket #3015.
  **
  ** 当把位掩码值分配给FROM子句游标时，如果X是N-th FROM子句term的位掩码，那么N-th term
  ** 左边的所有FROM子句terms的位掩码是(X-1)。一个LEFT JOIN的ON子句的表达式可以使用它自己的
  ** Expr.iRightJoinTable值来查找这个连接的右表的位掩码。从右表的位掩码中减去一，把这个位掩码
  ** 给连接的左边的所有表。要知道一个左连接左边的所有表的位掩码是很重要的。
  **
  ** Configure the WhereClause.vmask variable so that bits that correspond
  ** to virtual table cursors are set. This is used to selectively disable 
  ** the OR-to-IN transformation in exprAnalyzeOrTerm(). It is not helpful
  ** with virtual tables.
  **
  ** 配置WhereClause.vmask变量，设置与虚表游标相一致的bits。这用于有选择性地禁用
  ** exprAnalyzeOrTerm()中的OR-to-IN转化。它对虚表示无用的。
  **
  ** Note that bitmasks are created for all pTabList->nSrc tables in
  ** pTabList, not just the first nTabList tables.  nTabList is normally
  ** equal to pTabList->nSrc but might be shortened to 1 if the
  ** WHERE_ONETABLE_ONLY flag is set.
  **
  ** 注意:不只是为第一个nTabList表创建位掩码，而是为pTabList中的所有pTabList->nSrc表创建。
  ** 通常情况下，nTabList等价于pTabList->nSrc，但如果设置了WHERE_ONETABLE_ONLY标志，那么它可能缩短为1。
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
  **
  ** 分析所有的子表达式。注意exprAnalyze()可能添加新的虚拟项到WHERE子句的末尾。
  ** 我们不想分析这些虚拟项，因此从末尾往前分析，使被添加的虚拟项不被处理。
>>>>>>> 91288352e83e9763d493ed84aec377d15ced3949
  */
  exprAnalyzeAll(pTabList, pWC);//分析where子句中的所有terms
  if( db->mallocFailed ){  //如果数据库内存分配错误
    goto whereBeginError; //跳转到whereBeginError处理错误
  }

  /* Check if the DISTINCT qualifier, if there is one, is redundant. 
  ** If it is, then set pDistinct to NULL and WhereInfo.eDistinct to
  ** WHERE_DISTINCT_UNIQUE to tell the caller to ignore the DISTINCT.
  **
  ** 如果存在DISTINCT限定符，检查它是否是多余的。如果是，那么将pDistinct设置为NULL，
  ** 将WhereInfo.eDistinct设置为WHERE_DISTINCT_UNIQUE，来告诉调用者忽略DISTINCT限定符。
  */
  if( pDistinct && isDistinctRedundant(pParse, pTabList, pWC, pDistinct) ){ //如果Distinct是多余的
    pDistinct = 0; //清空pDistinct
    pWInfo->eDistinct = WHERE_DISTINCT_UNIQUE; //设置WhereInfo，告诉调用者忽略DISTINCT
  }

  /* Chose the best index to use for each table in the FROM clause.
  **
  ** 在FROM子句中为每个表选择最优索引。
  **
  ** This loop fills in the following fields:
  ** 这个循环填写以下字段：
  **
  **   pWInfo->a[].pIdx      The index to use for this level of the loop.  循环中用于此level的索引
  **   pWInfo->a[].wsFlags   WHERE_xxx flags associated with pIdx   与pIdx相关的WHERE_xxx标志
  **   pWInfo->a[].nEq       The number of == and IN constraints   ==和IN约束的数量
  **   pWInfo->a[].iFrom     Which term of the FROM clause is being coded   正被的FROM子句中的term
  **   pWInfo->a[].iTabCur   The VDBE cursor for the database table   该数据库表的VDBE游标
  **   pWInfo->a[].iIdxCur   The VDBE cursor for the index   该索引的VDBE光标
  **   pWInfo->a[].pTerm     When wsFlags==WO_OR, the OR-clause term   当wsFlags== WO_OR时，OR-clause term
  **
  ** This loop also figures out the nesting order of tables in the FROM clause.
  **
  ** 这个循环也解决在FROM子句中的表的嵌套顺序。
  */
  notReady = ~(Bitmask)0;//~取反，
  andFlags = ~0;
  WHERETRACE(("*** Optimizer Start ***\n"));
  for(i=iFrom=0, pLevel=pWInfo->a; i<nTabList; i++, pLevel++){	//循环遍历FROM子句中的表列表
    WhereCost bestPlan;         /* Most efficient plan seen so far  目前为止找到的最优计划 */
    Index *pIdx;                /* Index for FROM table at pTabItem   pTabItem中FROM引用的表的索引 */
    int j;                      /* For looping over FROM tables  循环遍历FROM子句中的表 */
    int bestJ = -1;             /* The value of j   j的值 */
    Bitmask m;                  /* Bitmask value for j or bestJ  j或bestJ的位掩码值 */
    int isOptimal;              /* Iterator for optimal/non-optimal search  最佳/非最佳搜索的迭代 */
    int nUnconstrained;         /* Number tables without INDEXED BY  没有INDEXED BY的表数目 */
    Bitmask notIndexed;         /* Mask of tables that cannot use an index  不能使用索引的表的掩码 */

    memset(&bestPlan, 0, sizeof(bestPlan)); //为bestPlan分配内存
    bestPlan.rCost = SQLITE_BIG_DBL; //初始化执行bestPlan的总体成本
    WHERETRACE(("*** Begin search for loop %d ***\n", i));

    /* Loop through the remaining entries in the FROM clause to find the
    ** next nested loop. The loop tests all FROM clause entries either once or twice. 
    **
    ** 循环遍历FROM子句的剩余term来查找下一个嵌套循环。这个循环测试所有的FROM子句项一次或两次。
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
    **
    ** 如果有两个或更多的项剩余，那么总是执行第一次测试，如果只有一个FROM子句项可供选择，那么
    ** 就从不执行。第一次测试查找一个"最佳的"扫描。在这种情况下，最优扫描会为给定的FROM子句条目
	** 使用相同的策略，如果这些条目被用作最内层嵌套循环。换句话说，选中一个表，该表不能通过等待
    ** 其他表先运行来减少自己的运行代价。这个"最佳的"测试起作用，是通过第一次假定FROM子句在
    ** 内部循环中，并且查找它的查询计划，然后检查这个查询计划是否使用其他未准备的FROM子句terms。
    ** 如果没有使用未准备的terms，那么"最佳的"查询计划会起作用。
    **
    ** Note that the WhereCost.nRow parameter for an optimal scan might
    ** not be as small as it would be if the table really were the innermost
    ** join.  The nRow value can be reduced by WHERE clause constraints
    ** that do not use indices.  But this nRow reduction only happens if the
    ** table really is the innermost join.  
    **
    ** 注意:对于一个最佳查询的WhereCost.nRow参数可能不会像表在最内层连接时那样小。
    ** nRow值可以通过不使用索引的WHERE子句约束来减小。但是这个nRow值减小只在表真的是最内连接时发生。
    **
    ** The second loop iteration is only performed if no optimal scan
    ** strategies were found by the first iteration. This second iteration
    ** is used to search for the lowest cost scan overall.
    **
    ** 如果第一次迭代没有发现最优扫描策略，那么才执行第二次循环迭代。第二次迭代全面查找最小代价的扫描策略。
    **
    ** Previous versions of SQLite performed only the second iteration -
    ** the next outermost loop was always that with the lowest overall
    ** cost. However, this meant that SQLite could select the wrong plan
    ** for scripts such as the following:
	**
    ** SQLite以前的版本只执行第二次迭代--下一个外层循环总是总成本最低的。
    ** 然而，这意味着SQLite可能选择错误的计划，例如下面这个例子:
    **   CREATE TABLE t1(a, b);   创建表t1（a,b）
    **   CREATE TABLE t2(c, d);   创建表t2(c,d)
    **   SELECT * FROM t2, t1 WHERE t2.rowid = t1.a;  从t1，t2中查找，条件是t2的rowid的值等于t1的a值
	** 
    ** The best strategy is to iterate through table t1 first. However it
    ** is not possible to determine this with a simple greedy algorithm.
    ** Since the cost of a linear scan through table t2 is the same as the
    ** cost of a linear scan through table t1, a simple greedy algorithm
    ** may choose to use t2 for the outer loop, which is a much costlier approach.
    **
    ** 最好的策略是先迭代遍历表t1。然而，使用一个简单的贪心算法不能决定这个。由于线性扫描遍历表t2的代价
    ** 和线性扫描遍历表t1的代价相同，所以一个简单的贪心算法可能把表t2放在外部循环，这是一个代价较高的策略。
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
        **
        **   (1) The table must not depend on other tables that have not
        **       yet run.
        **
        **   (2) A full-table-scan plan cannot supercede indexed plan unless
        **       the full-table-scan is an "optimal" plan as defined above.
        **
        **   (3) All tables have an INDEXED BY clause or this table lacks an
        **       INDEXED BY clause or this table uses the specific
        **       index specified by its INDEXED BY clause.  This rule ensures
        **       that a best-so-far is always selected even if an impossible
        **       combination of INDEXED BY clauses are given.  The error
        **       will be detected and relayed back to the application later.
        **       The NEVER() comes about because rule (2) above prevents
        **       An indexable full-table-scan from reaching rule (3).
        **
        **   (4) The plan cost must be lower than prior plans or else the
        **       cost must be the same and the number of rows must be lower.
        **
        **
        **  下面是说明这个表到目前为止是最佳表的条件:
        **   (1) 表不能依赖于其他还未运行的表。
        **   (2) 一个全表扫描计划不能取代有索引的计划，除非全表扫描是一个如上面定义的"最佳的"计划
        **   (3) 所有的表都有一个INDEXED BY子句，或这个表缺少一个INDEXED BY子句，或这个表使用由
        **       它的INDEXED BY子句指定的特殊索引。这个规则确保一个best-so-far总是能构找到，
        **       即使是给出一个不可能的INDEXED BY子句组合。检查到错误并稍后返回给应用程序。
 		**       NEVER()发生是因为上面的规则(2)阻止一个可加索引的全表扫描来满足规则(3)。
        **   (4) 计划的代价必须小于前一个计划，或代价相同而行数比较少。
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
    **
    ** 查看通过这个循环迭代扫描的表，是否有一个INDEXED BY子句，它指定的索引用于这个扫描。
    ** 如果没有，那么查询编辑失败。返回一个错误。
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
        **
        ** 如果使用了一个INDEXED BY子句，并且bestIndex()函数能找到一个索引，
        ** 那么确保它能找到在INDEXED BY子句中指定的索引。
        */
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
  **
  ** 如果整个查询只选择一行，那么ORDER BY子句无关紧要。
  */
  if( (andFlags & WHERE_UNIQUE)!=0 && ppOrderBy ){
    *ppOrderBy = 0;
  }

  /* If the caller is an UPDATE or DELETE statement that is requesting
  ** to use a one-pass algorithm, determine if this is appropriate.
  ** The one-pass algorithm only works if the WHERE clause constraints
  ** the statement to update a single row.
  **
  ** 如果调用方式一个UPDATE或DELETE命令请求一个一次通过的算法，确定这是合适的。
  ** 一次通过算法值在WHERE子子句约束命令去更新一行时才起作用。
  */
  assert( (wctrlFlags & WHERE_ONEPASS_DESIRED)==0 || pWInfo->nLevel==1 );
  if( (wctrlFlags & WHERE_ONEPASS_DESIRED)!=0 && (andFlags & WHERE_UNIQUE)!=0 ){
    pWInfo->okOnePass = 1;
    pWInfo->a[0].plan.wsFlags &= ~WHERE_IDX_ONLY;
  }

  /* Open all tables in the pTabList and any indices selected for
  ** searching those tables.
  **
  ** 打开pTabList中的所有表和用于搜索这些表而选择的所有索引。
  */
  sqlite3CodeVerifySchema(pParse, -1);   /* Insert the cookie verifier Goto  验证者Goto插入cookie */
  notReady = ~(Bitmask)0;
  pWInfo->nRowOut = (double)1;
  for(i=0, pLevel=pWInfo->a; i<nTabList; i++, pLevel++){
    Table *pTab;     /* Table to open 需要打开的表 */
    int iDb;         /* Index of database containing table/index  包含表/索引的数据库的索引 */

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
  ** 生成代码来进行搜索。下面的for循环的每次迭代为VM程序的一个嵌套循环生成代码。
  */
  notReady = ~(Bitmask)0;
  for(i=0; i<nTabList; i++){
    pLevel = &pWInfo->a[i];
    explainOneScan(pParse, pTabList, pLevel, i, pLevel->iFrom, wctrlFlags);
    notReady = codeOneLoopStart(pWInfo, i, wctrlFlags, notReady);
    pWInfo->iContinue = pLevel->addrCont;
  }

#ifdef SQLITE_TEST  /* For testing and debugging use only   只用于测试和调试 */
  /* Record in the query plan information about the current table
  ** and the index used to access it (if any).  If the table itself
  ** is not used, its name is just '{}'.  If no index is used
  ** the index is listed as "{}".  If the primary key is used the
  ** index name is '*'.
  **
  ** 记录查询计划中当前表和访问该表的索引的信息。如果未使用表本身，那么它的名字只是'{}'。
  ** 如果没有使用索引，索引被列为"{}"。如果使用了主键，那么索引名是'*'。
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
#endif 
  /* SQLITE_TEST // Testing and debugging use only   只用于测试和调试 */

  /* Record the continuation address in the WhereInfo structure.  Then
  ** clean up and return.
  **
  ** 记录在WhereInfo数据结构中的连续地址。然后清除并返回。
  */
  return pWInfo;

  /* Jump here if malloc fails  如果内存分配失败，跳转到这里*/
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
**
** 生成结束WHERE循环的代码。查看在sqlite3WhereBegin()上的附加信息。
*/
void sqlite3WhereEnd(WhereInfo *pWInfo){
  Parse *pParse = pWInfo->pParse;
  Vdbe *v = pParse->pVdbe;
  int i;
  WhereLevel *pLevel;
  SrcList *pTabList = pWInfo->pTabList;
  sqlite3 *db = pParse->db;

  /* Generate loop termination code.
  ** 生成循环终止代码
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
  **
  ** "break"指针，刚刚结束外循环。设置它。
  */
  sqlite3VdbeResolveLabel(v, pWInfo->iBreak);

  /* Close all of the cursors that were opened by sqlite3WhereBegin.
  **
  ** 关闭sqlite3WhereBegin打开的所有游标
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
    ** 
    ** 如果这个扫描使用了一个索引，编写代码优先从索引中读取数据，取代从表中读取数据。
    ** 这意味着有时表不被读取。这是一个性能优化，在寻找对应的表的游标来记录
    ** 当前索引的位置之前，vdbe level一直等待读取表。
    **
    ** Calls to the code generator in between sqlite3WhereBegin and
    ** sqlite3WhereEnd will have created code that references the table
    ** directly.  This loop scans all that code looking for opcodes
    ** that reference the table and converts them into opcodes that
    ** reference the index.
    **
    ** 在sqlite3WhereBegin和sqlite3WhereEnd之间调用代码生成器，创建直接与表相关的代码。
    ** 这个循环扫描所有这些代码，来寻找与表相关的操作码，并将它们转化为与索引相关的opcodes。
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
  /* Final cleanup 
  ** 最后清除数据
  */
  pParse->nQueryLoop = pWInfo->savedNQueryLoop;
  whereInfoFree(db, pWInfo);
  return;
}
