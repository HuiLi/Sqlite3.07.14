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
** to handle SELECT statements in SQLite.
**此文件包含由语法分析器来处理在 SQLite 的 SELECT 语句调用的 C 代码例程
*/

#include "sqliteInt.h"  /*预编译处理器把sqliteInt.h文件中的内容加载到程序中来*/

/*
** Trace output macros
** 跟踪输出宏
*/

#if SELECTTRACE_ENABLED //预编译指令
/***/ int sqlite3SelectTrace = 0;
# define SELECTTRACE(K,P,S,X)  
  if(sqlite3SelectTrace&(K))   
    sqlite3DebugPrintf("%*s%s.%p: ",(P)->nSelectIndent*2-2,"",
        (S)->zSelName,(S)),
    sqlite3DebugPrintf X
#else
# define SELECTTRACE(K,P,S,X)
#endif

/*
** An instance of the following object is used to record information about
** how to process the DISTINCT keyword, to simplify passing that information
** into the selectInnerLoop() routine.
** 下面结构体的一个实例是用于记录有关如何处理DISTINCT关键字的信息，为了简化传递该信息到selectInnerLoop（）事务。
*/

typedef struct DistinctCtx DistinctCtx;
struct DistinctCtx {
	u8 isTnct;      /* 如果DISTINCT关键字存在则真 */ 
		u8 eTnctType;   /* 其中的WHERE_DISTINCT_*运算符*/ 
		int tabTnct;    /* 处理DISTINCT的临时表*/
		int addrTnct;   /* OP_OpenEphemeral操作码的地址*/ 
};

/*
** An instance of the following object is used to record information about
** the ORDER BY (or GROUP BY) clause of query is being coded.
** 下面结构体的一个实例是用于记录ORDER BY(或者 GROUP BY)查询字句的信息
*/

typedef struct SortCtx SortCtx;
struct SortCtx {
	ExprList *pOrderBy;   /* ORDER BY(或者 GROUP BY字句)*/ 
		int nOBSat;           /* ORDER BY语句的数量满足指数*/
		int iECursor;         /* 分类器的游标数目*/
		int regReturn;        /* 寄存器控制块输出返回地址*/
		int labelBkOut;       /* 块输出子程序的启动标签*/  
		int addrSortIndex;    /* OP_SorterOpen或者OP_OpenEphemeral的地址 */ 
		u8 sortFlags;         /* 零或者更多的SORTFLAG_* 位 */ 
};
#define SORTFLAG_UseSorter  0x01   /* 使用sorteropen代替openephemeral */ 

/*
** Delete all the content of a Select structure.  Deallocate the structure
** itself only if bFree is true.
** 删除选择结构的所有内容。仅当bFree是真的时候释放结构本身
*/

static void clearSelect(sqlite3 *db, Select *p, int bFree){
	while (p){
		Select *pPrior = p->pPrior;                  /*将p->pPrior赋值给Select *pPrior*/
			sqlite3ExprListDelete(db, p->pEList);        /*删除整个表达式列表*/
			sqlite3SrcListDelete(db, p->pSrc);           /*删除整个SrcList, 包括所有的子结构*/
			sqlite3ExprDelete(db, p->pWhere);            /*递归删除一个表达式树*/
			sqlite3ExprListDelete(db, p->pGroupBy);      /*删除整个表达式列表*/
			sqlite3ExprDelete(db, p->pHaving);           /*递归删除一个表达式树*/
			sqlite3ExprListDelete(db, p->pOrderBy);      /*删除整个表达式列表*/
			sqlite3ExprDelete(db, p->pLimit);            /*递归删除一个表达式树*/
			sqlite3ExprDelete(db, p->pOffset);           /*递归删除一个表达式树*/
			sqlite3WithDelete(db, p->pWith);			 /*递归删除一个条件树*/
			if (bFree)
				sqlite3DbFree(db, p);					 /*释放*db*/
				p = pPrior;
		bFree = 1;
	}
}

/*
** Initialize a SelectDest structure.
** 初始化一个SelectDest结构.
*/
void sqlite3SelectDestInit(SelectDest *pDest, int eDest, int iParm){/*函数sqlite3SelectDestInit的参数列表为
																	  结构体SelectDest指针pDest，整型指针eDest，
																	  整型指针iParm
																	  */
	pDest->eDest = (u8)eDest; /*把整型eDest强制类型转化为u8型，然后赋值给pDest->eDest*/
	pDest->iSDParm = iParm; /*整型参数iParm赋值为pDest->iSDParm*/
	pDest->affSdst = 0; /*0赋值给pDest->affSdst*/
	pDest->iSdst = 0; /*0赋值给pDest->iSdst*/
	pDest->nSdst = 0; /*0赋值给pDest->nSdst*/
}

/*
** Allocate a new Select structure and return a pointer to that
** structure.
** 分配一个新的选择结构并且返回一个指向该结构的指针.
*/
Select *sqlite3SelectNew(
	Parse *pParse,        /* Parsing context  句法分析*/
	ExprList *pEList,     /* which columns to include in the result  在结果中包含哪些列*/
	SrcList *pSrc,        /* the FROM clause -- which tables to scan  FROM字句--扫描表 */
	Expr *pWhere,         /* the WHERE clause  WHERE字句*/
	ExprList *pGroupBy,   /* the GROUP BY clause   GROUP BY字句*/
	Expr *pHaving,        /* the HAVING clause  havingHAVING字句*/
	ExprList *pOrderBy,   /* the ORDER BY clause  ORDER BY字句*/
	int isDistinct,       /* true if the DISTINCT keyword is present  如果关键字distinct存在，则返回true*/
	Expr *pLimit,         /* LIMIT value.  NULL means not used  limit值，如果值为空意味着limit未使用*/
	Expr *pOffset         /* OFFSET value.  NULL means no offset  offset值，如果值为空意味着offset未使用*/
	){
	Select *pNew;/*定义结构体指针pNew*/
	Select standin;/*定义结构体类型变量standin*/
	sqlite3 *db = pParse->db;/*结构体Parse的成员db赋值给结构体sqlite3指针db*/
	pNew = sqlite3DbMallocZero(db, sizeof(*pNew));  /* 分配和零内存，如果分配失败，使mallocFaied标志在连接指针中。 */
	assert(db->mallocFailed || !pOffset || pLimit); /* 判断分配是否失败,或pOffset值为空,或pLimit值不为空*/
	if (pNew == 0){/*如果结构体指针变量pNew分配失败*/
		assert(db->mallocFailed);/*如果分配失败，使mallocFaied标志在连接指针中*/
		pNew = &standin;/*把standin的存储地址赋给pNew*/
		memset(pNew, 0, sizeof(*pNew));/*将pNew中前sizeof(*pNew)个字节用0替换并且返回pNew*/
	}
	if (pEList == 0){/*如果表达式列表为空*/
		pEList = sqlite3ExprListAppend(pParse, 0, sqlite3Expr(db, TK_ALL, 0)); /*新添加的元素在表达式列表的末尾。新添加元素
																				的地址赋给pEList。如果pList的初始数据为空，
																				那么新建 一个新的表达式列表。如果出现内存
																				分配错误，则整个列表被释放并返回空。如果
																				返回的是非空，则保证新的条目成功追加。
																				*/
	}
	pNew->pEList = pEList;/*pEList指向元素的地址赋给pNew->pEList*/
	if (pSrc == 0) pSrc = sqlite3DbMallocZero(db, sizeof(*pSrc));/*分配并清空内存，分配大小为第二个参数的内存，如果分配失败，会在mallocFailed中做标记*/
	pNew->pSrc = pSrc;/*为Select结构体中FROM子句表达式赋值*/
	pNew->pWhere = pWhere;/*为Select结构体中Where子句表达式赋值*/
	pNew->pGroupBy = pGroupBy;/*为Select结构体中GroupBy子句表达式赋值*/
	pNew->pHaving = pHaving;/*为Select结构体中Having子句表达式赋值*/
	pNew->pOrderBy = pOrderBy;/*为Select结构体中OrderBy子句表达式赋值*/
	pNew->selFlags = isDistinct ? SF_Distinct : 0;/*设置SF_*中的值*/
	pNew->op = TK_SELECT;/*只能设置为TK_UNION TK_ALL TK_INTERSECT TK_EXCEPT 其中一个值*/
	pNew->pLimit = pLimit;/*为Select结构体中Limit子句表达式赋值*/
	pNew->pOffset = pOffset;/*为Select结构体中Offset子句表达式赋值*/
	assert(pOffset == 0 || pLimit != 0);/*如果偏移量为空，Limit不为空，则分配内存*/
	pNew->addrOpenEphm[0] = -1;       /*对地址信息初始化*/
	pNew->addrOpenEphm[1] = -1;		  /*对地址信息初始化*/
	pNew->addrOpenEphm[2] = -1;		  /*对地址信息初始化*/
	if (db->mallocFailed) {          /*如果不能分配内存*/
		clearSelect(db, pNew);          /*清除Select结构体中内容*/
		if (pNew != &standin) sqlite3DbFree(db, pNew);/*如果Select结构体standin的地址未赋值给pNew，则清除pNew，*/
		pNew = 0;                      /*将Select结构体设置为空*/
	}
	else{                           /*能分配内存*/
		assert(pNew->pSrc != 0 || pParse->nErr>0);/*判断是否有From子句或者是否有分析错误*/
	}
	assert(pNew != &standin);/*判断Select结构体是否同替换结构体相同*/
	return pNew;/*返回这个构造好的Select结构体*/
}

/*
** Delete the given Select structure and all of its substructures.
** 删除给定的选择结构和所有的子结构
*/
void sqlite3SelectDelete(sqlite3 *db, Select *p){/*定义数据库db以及Select类型的结构体p作为参数*/
	if (p){/*如果Select结构体指针p存在*/
		clearSelect(db, p);  /*清空Select类型结构体p里面的内容*/
		sqlite3DbFree(db, p);  /*释放掉空间*/
	}
}

/*
** Given 1 to 3 identifiers preceeding the JOIN keyword, determine the
** type of join.  Return an integer constant that expresses that type
** in terms of the following bit values:
** 给定1-3标识符提前加入关键字,确定加入的类型。返回一个整数常数表示该类型的下列值:
**
**     JT_INNER
**     JT_CROSS
**     JT_OUTER
**     JT_NATURAL
**     JT_LEFT
**     JT_RIGHT
**
** A full outer join is the combination of JT_LEFT and JT_RIGHT.
** If an illegal or unsupported join type is seen, then still return
** a join type, but put an error in the pParse structure. 
** 完全外连接的组合JT_LEFT JT_RIGHT。 如果发现非法或不受支持的连接类型,仍然要返回一个连接类型,但是要在pParse结构中保存这个错误信息
*/
int sqlite3JoinType(Parse *pParse, Token *pA, Token *pB, Token *pC){/*定义分析数变量pParse以及三个符文结构体（符文：具有执行某些操作的权利的对象）参数*/
	int jointype = 0;/*临时变量用于标示链接类型*/
	Token *apAll[3];/*定义符文类型结构体指针数组apAll*/
	Token *p;/*定义符文类型结构体指针p*/
	/*   0123456789 123456789 123456789 123 */
	static const char zKeyText[] = "naturaleftouterightfullinnercross";/*定义只读的且只能在当前模块中可见的字符型数组zKeyText，用于标示连接类型，并对其进行赋值*/
	static const struct {/*定义只读的且只能在当前模块可见结构体*/
		u8 i;        /* Beginning of keyword text in zKeyText[]   在KeyText[] 中开始关键字的文本*/
		u8 nChar;    /* Length of the keyword in characters  在字符中关键字的长度*/
		u8 code;     /* Join type mask 标记连接类型*/
	} aKeyword[] = {
		/* natural 自然连接 */{ 0, 7, JT_NATURAL },
		/* left   左连接 */{ 6, 4, JT_LEFT | JT_OUTER },
		/* outer  外连接 */{ 10, 5, JT_OUTER },
		/* right   右连接*/{ 14, 5, JT_RIGHT | JT_OUTER },
		/* full    全连接*/{ 19, 4, JT_LEFT | JT_RIGHT | JT_OUTER },
		/* inner  内连接 */{ 23, 5, JT_INNER },
		/* cross   交叉连接*/{ 28, 5, JT_INNER | JT_CROSS },
	};//定义全部类型的连接，并给出起始位置、长度、连接类型
	int i, j;
	apAll[0] = pA;
	apAll[1] = pB;
	apAll[2] = pC;/*将传人的三个符文结构体类型的参数分别赋值到符文结构体类型的apAll数组中 */
	for (i = 0; i < 3 && apAll[i]; i++){//循环处理apAll数组
		p = apAll[i];/*指针apAll[i]的地址赋给指针p*/
		if (p->n == aKeyword[j].nChar /*如果符文中字符个数等于连接数组中的关键字长度*/
			&& sqlite3StrNICmp((char*)p->z, &zKeyText[aKeyword[j].i], p->n) == 0){/*并且使用比较字符串函数比较*/
			jointype |= aKeyword[j].code;/*如果通过了比较长度和内容，返回连接类型，注意是，使用的是“位或”*/
			break;//跳出本层循环
		}
	}
	testcase(j == 0 || j == 1 || j == 2 || j == 3 || j == 4 || j == 5 || j == 6);/*利用测试代码中testcast，测试j值，是否在这个范围*/
	if (j >= ArraySize(aKeyword)){/*如果j比连接关键字数组还大*/
		jointype |= JT_ERROR;/*那就jointype与JT_ERROR“位或”，返回一个错误*/
		break;//跳出本层循环
	  }
	}
	if (
	(jointype & (JT_INNER | JT_OUTER)) == (JT_INNER | JT_OUTER) ||/*如果连接类型交上(JT_INNER|JT_OUTER)的结果，与JT_INNER和JT_OUTER一种*/
	(jointype & JT_ERROR) != 0/*或者连接关键字是错误连接*/
	){
		const char *zSp = " "; /*只读的字符型指针zSp*/
		assert(pB != 0);/*判断pB是否为空*/
		if (pC == 0){ zSp++; }/*如果指针pC指向的地址为0，则zSp++*/
		sqlite3ErrorMsg(pParse, "unknown or unsupported join type: ""%T %T%s%T", pA, pB, zSp, pC);  /*输出错误消“未知或者不支持的连接类型”*/
		jointype = JT_INNER ;/*默认使用内连接*/
	}
	else if ((jointype & JT_OUTER) != 0 
		&& (jointype & (JT_LEFT | JT_RIGHT)) != JT_LEFT){/*如果连接类型和外连接有交集，并且连接类型和(JT_LEFT|JT_RIGHT)交集，不是左连接*/
		sqlite3ErrorMsg(pParse,"RIGHT and FULL OUTER JOINs are not currently supported");/*输出错误消息“右连接和全外连接不被支持”*/
		jointype = JT_INNER; /*默认使用内连接*/
	}
	return jointype;/*返回连接类型*/
}

/*
** Return the index of a column in a table.  Return -1 if the column
** is not contained in the table.
** 返回一个表中的列的索引。如果该列没有包含在表中返回-1。
*/
static int columnIndex(Table *pTab, const char *zCol){/*定义静态的整型函数columnIndex，参数列表为结构体指针pTab、只读的字符型指针zCol*/
	int i;/*定义临时变量*/
	for (i = 0; i < pTab->nCol; i++){/*对所有的列进行遍历*/
		if (sqlite3StrICmp(pTab->aCol[i].zName, zCol) == 0) return i;/*如果匹配成功，那么返回i*/
	}
	return -1;/*否则，返回-1*/
}

/*
** Search the first N tables in pSrc, from left to right, looking for a
** table that has a column named zCol.
** 在FROM子句中扫描表，从左到右查找前N个表，搜索列中有名为zCol的表。
**
** When found, set *piTab and *piCol to the table index and column index
** of the matching column and return TRUE.
** 当找到以后，设置* piTab和* piCol表索引和匹配列的列索引，并返回TRUE 。
**
** If not found, return FALSE.
** 如果没有找到，返回FALSE。
*/
static int tableAndColumnIndex(
	SrcList *pSrc,       /* Array of tables to search 存放待查找的表的队列*/
	int N,               /* Number of tables in pSrc->a[] to search 表的数目*/
	const char *zCol,    /* Name of the column we are looking for 寻找的列名*/
	int *piTab,          /* Write index of pSrc->a[] here 写入索引*/
	int *piCol           /* Write index of pSrc->a[*piTab].pTab->aCol[] here 写入索引*/
	){
	int i;               /* For looping over tables in pSrc 对pSrc遍历表*/
	int iCol;            /* Index of column matching zCol   匹配上的列的索引，第几列*/

	assert((piTab == 0) == (piCol == 0));  /* Both or neither are NULL  判断表索引和列索引都是或都不是空*/
	for (i = 0; i < N; i++){/*遍历所有的表*/
		iCol = columnIndex(pSrc->a[i].pTab, zCol); /*返回表的列的索引赋给iCol，如果该列没有在表中，iCol的值是-1.*/
		if (iCol >= 0){/*如果列索引存在*/
			if (piTab){/*如果表索引存在*/
				*piTab = i;/*把i 赋给指针piTab 的目标变量*/
				*piCol = iCol;/*把iCol 赋值给指针piCol 的目标变量*/
			}
			return 1;/*否则返回1*/
		}
	}
	return 0;
}

/*
** This function is used to add terms implied by JOIN syntax to the
** WHERE clause expression of a SELECT statement. The new term, which
** is ANDed with the existing WHERE clause, is of the form:
**
**    (tab1.col1 = tab2.col2)
**
** where tab1 is the iSrc'th table in SrcList pSrc and tab2 is the
** (iSrc+1)'th. Column col1 is column iColLeft of tab1, and col2 is
** column iColRight of tab2.

** 此函数功能是用来添加where子句解释含有JOIN语法句,从而解释select语句。
** 是相比现有的WHERE子句的这种形式：
**
** (tab1.col1 = tab2.col2)
**
** tab1是SrcList pSrc的iSrc'th表，tab2是(iSrc+1)'th。列col1是tab1的iColLeft列，col2是
** tab2的iColRight列
*/
static void addWhereTerm(
	Parse *pParse,                  /* Parsing context  语义分析*/
	SrcList *pSrc,                  /* List of tables in FROM clause   from字句中的列表 */
	int iLeft,                      /* Index of first table to join in pSrc  第一个连接的表索引 */
	int iColLeft,                   /* Index of column in first table  第一个表的列索引*/
	int iRight,                     /* Index of second table in pSrc  第二个连接的表索引*/
	int iColRight,                  /* Index of column in second table  第二个表的列索引*/
	int isOuterJoin,                /* True if this is an OUTER join  如果是外连接则返回true*/
	Expr **ppWhere                  /* IN/OUT: The WHERE clause to add to  where子句添加到in/out*/
	){
	sqlite3 *db = pParse->db;/*声明一个数据库连接*/
	Expr *pE1; /*定义结构体指针pE1*/
	Expr *pE2; /*定义结构体指针pE2*/
	Expr *pEq; /*定义结构体指针pEq*/

	assert(iLeft<iRight);/*判断如果第一个表索引值是否小于第二个表索引值*/
	assert(pSrc->nSrc>iRight);/*判断表集合中的表的数目是否大于右表的索引值*/
	assert(pSrc->a[iLeft].pTab);/*判断表集合中表中左表索引的表是否为空*/
	assert(pSrc->a[iRight].pTab);/*判断表集合中表中右表索引的表是否为空*/

	pE1 = sqlite3CreateColumnExpr(db, pSrc, iLeft, iColLeft);/*分配并返回一个表达式指针去加载表集合中左表的一个列索引*/
	pE2 = sqlite3CreateColumnExpr(db, pSrc, iRight, iColRight);/*分配并返回一个表达式指针去加载表集合中右表的一个列索引*/

	pEq = sqlite3PExpr(pParse, TK_EQ, pE1, pE2, 0);/*分配一个额外节点连接这两个子树表达式*/
	if (pEq && isOuterJoin){/*如果pEq表达式是全连接表达式*/
		ExprSetProperty(pEq, EP_FromJoin);/*那么在连接中使用ON或USING子句*/
		assert(!ExprHasAnyProperty(pEq, EP_TokenOnly | EP_Reduced));/*判断pEq表达式是否是EP_TokenOnly或EP_Reduced*/
		ExprSetIrreducible(pEq);/*调试pEq,设置是否可以约束*/
		pEq->iRightJoinTable = (i16)pE2->iTable;/*指定要连接的右表是第二个表达式的表*/
	}
	*ppWhere = sqlite3ExprAnd(db, *ppWhere, pEq);/*对指定数据库的表达式是进行连接*/
}

/*
** Set the EP_FromJoin property on all terms of the given expression.
** And set the Expr.iRightJoinTable to iTable for every term in the
** expression.
** 在给定的表达式中的所有条件进行EP_FromJoin的属性设置。并给iTable的每一种
** 表达形式进行Expr.iRightJoinTable设置。
**
** The EP_FromJoin property is used on terms of an expression to tell
** the LEFT OUTER JOIN processing logic that this term is part of the
** join restriction specified in the ON or USING clause and not a part
** of the more general WHERE clause.  These terms are moved over to the
** WHERE clause during join processing but we need to remember that they
** originated in the ON or USING clause.
** EP_FromJoin 属性用于左外连接处理逻辑的表达形式，这种形式是加入限制指定on或者
** using子句的一部分，不是一般where子句的一部分。这些术语移植到where 子句中
** 使用，但是我们必须记住它们起源于on或者useing子句。
**
** The Expr.iRightJoinTable tells the WHERE clause processing that the
** expression depends on table iRightJoinTable even if that table is not
** explicitly mentioned in the expression.  That information is needed
** for cases like this:
**
**    SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.b AND t1.x=5
**
** The where clause needs to defer the handling of the t1.x=5
** term until after the t2 loop of the join.  In that way, a
** NULL t2 row will be inserted whenever t1.x!=5.  If we do not
** defer the handling of t1.x=5, it will be processed immediately
** after the t1 loop and rows with t1.x!=5 will never appear in
** the output, which is incorrect.
** Expr.iRightJoinTable告诉where子句表达式依靠表iRightJoinTable处理，即使表在
** 表达式中没有明确提到。这些信息需要像这个例子:
** SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.b AND t1.x=5
** where 子句需要推迟处理t1.x=5，直到加入t2循环之后。以这种方式，
** 每当t1.x!=5时，一个NULL t2行将被加入。如果我们不推迟 t1.x=5的处理，
** 将会被立即处理后与t1循环和列t1.x!=5永远不会输出，这是不正确的。
*/
static void setJoinExpr(Expr *p, int iTable){/*函数setJoinExpr的参数列表为结构体指针p，整型iTable*/
	while (p){/*当p为真时循环*/
		ExprSetProperty(p, EP_FromJoin);/*设置join中使用ON和USING子句*/
		assert(!ExprHasAnyProperty(p, EP_TokenOnly | EP_Reduced));/*判断表达式的属性，关于表达式的长度和剩余长度*/
		ExprSetIrreducible(p);/*调试表达式，判读是否错误*/
		p->iRightJoinTable = (i16)iTable;/*连接右表，即参数中传入的表*/
		setJoinExpr(p->pLeft, iTable);/*递归调用自身*/
		p = p->pRight;/*赋值表达式，将p赋值成为原来p的右子节点*/
	}
}

/*
** This routine processes the join information for a SELECT statement.
** ON and USING clauses are converted into extra terms of the WHERE clause.
** NATURAL joins also create extra WHERE clause terms.
** 这个程序处理一个select语句的join信息。on或者using子句转换为额外形式的where子句。
** 自然连接也创建额外的where子句。
**
** The terms of a FROM clause are contained in the Select.pSrc structure.
** The left most table is the first entry in Select.pSrc.  The right-most
** table is the last entry.  The join operator is held in the entry to
** the left.  Thus entry 0 contains the join operator for the join between
** entries 0 and 1.  Any ON or USING clauses associated with the join are
** also attached to the left entry.
**from子句被Select.pSrc(Select结构体中FROM属性)所包含。
**左边的表通常是Select.pSrc的入口。右边的表通常是最后一个入口（entry在hashmap中作为循环的节点入口，此处的理解的表连接遍历的记录入口）
**join操作符在入口的左边。然后入口点0包含的连接操作符在入口0和入口1之间。任何涉及join的ON和USING子句，也将连接操作符放到左边入口。
**
** This routine returns the number of errors encountered.
** 这个程序返回遇到错误的数量
*/
static int sqliteProcessJoin(Parse *pParse, Select *p){/*传入分析树pParse，Select结构体p*/
	SrcList *pSrc;                  /* All tables in the FROM clause   from子句中的所有表*/
	int i, j;                       /* Loop counters  循环计数器*/
	struct SrcList_item *pLeft;     /* Left table being joined   左表被加入*/
	struct SrcList_item *pRight;    /* Right table being joined   右表被加入*/

	pSrc = p->pSrc;/*把p->pSrc赋值给结构体指针pSrc*/
	pLeft = &pSrc->a[0];/*把&pSrc->a[0]放入左表*/
	pRight = &pLeft[1];/*把&pLeft[1]放入右表*/
	for (i = 0; i < pSrc->nSrc - 1; i++, pRight++, pLeft++){
		Table *pLeftTab = pLeft->pTab;/*把pLeft->pTab赋给结构体指针pLeftTab*/
		Table *pRightTab = pRight->pTab;/*把pRight->pTab赋给结构体指针pRightTab*/
		int isOuter;/*用于判断*/

		if (NEVER(pLeftTab == 0 || pRightTab == 0)) continue;/*如果左表或右表有一个为空则跳出本次循环*/
		isOuter = (pRight->jointype & JT_OUTER) != 0;/*右表的连接类型交外连接类型不为空，再赋值给isOute属性值*/

		/* When the NATURAL keyword is present, add WHERE clause terms for
		** every column that the two tables have in common.
		** 当natural关键字存在，并且WHERE子句的条件为两个表中有相同列。
		*/
		if (pRight->jointype & JT_NATURAL){/*如果右表的连接类型是自然连接*/
			if (pRight->pOn || pRight->pUsing){/*如果右表有ON或USING子句*/
				sqlite3ErrorMsg(pParse, "a NATURAL join may not have "
					"an ON or USING clause", 0);/*那么就输出，自然连接中不能含有ON USING子句*/
				return 1;
			}
			for (j = 0; j < pRightTab->nCol; j++){/*循环遍历右表中的列*/
				char *zName;   /* Name of column in the right table 右表中列的名字*/
				int iLeft;     /* Matching left table 匹配左表*/
				int iLeftCol;  /* Matching column in the left table 在左表中匹配列*/

				zName = pRightTab->aCol[j].zName;/*给列名赋值*/
				if (tableAndColumnIndex(pSrc, i + 1, zName, &iLeft, &iLeftCol)){/*如果存在左表的列的索引*/
					addWhereTerm(pParse, pSrc, iLeft, iLeftCol, i + 1, j,
						isOuter, &p->pWhere);/*添加WHERE子句，设置左右表、列和连接方式*/
				}
			}
		}

		/* Disallow both ON and USING clauses in the same join
		** 不允许在同一个连接中使用on和using子句
		*/
		if (pRight->pOn && pRight->pUsing){/*如果结构体指针pRight引用的成员变量錺On和pUsing非空*/
			sqlite3ErrorMsg(pParse, "cannot have both ON and USING "
				"clauses in the same join");/*输出错误信息“cannot have both ON and USING clauses in the same join”*/
			return 1;
		}

		/* Add the ON clause to the end of the WHERE clause, connected by
		** an AND operator.
		** 将on子句添加到where子句的末尾，用and操作符连接
		*/
		if (pRight->pOn){/*如果结构体指针pRight引用的成员变量pOn非空*/
			if (isOuter) setJoinExpr(pRight->pOn, pRight->iCursor);/*如果有外连接，设置连接表达式中ON子句和游标*/
			p->pWhere = sqlite3ExprAnd(pParse->db, p->pWhere, pRight->pOn);/*设置将WHERE子句与ON子句连接一起，赋值给结构体的WHERE*/
			pRight->pOn = 0;/*如果没有外连接，就设置不使用ON子句*/
		}

		/* Create extra terms on the WHERE clause for each column named
		** in the USING clause.  Example: If the two tables to be joined are
		** A and B and the USING clause names X, Y, and Z, then add this
		** to the WHERE clause:    A.X=B.X AND A.Y=B.Y AND A.Z=B.Z
		** Report an error if any column mentioned in the USING clause is
		** not contained in both tables to be joined.
		** 在WHERE子句上为每一列创建一个额外的USING子句条款。例如：
		** 如果两个表的连接是A和B,using列名为X,Y,Z,然后把它们添加到
		** where子句:A.X=B.X AND A.Y=B.Y AND A.Z=B.Z
		** 如果using子句中提到的任何列不包含在表的连接中，就会报告
		** 一个错误。
		*/
		if (pRight->pUsing){/*如果结构体指针pRight引用的成员变量pUsing非空*/
			IdList *pList = pRight->pUsing;/*把pRight->pUsing赋给结构体指针pList*/
			for (j = 0; j < pList->nId; j++){/*遍历标示符列表*/
				char *zName;     /* Name of the term in the USING clause   using子句的名称术语*/
				int iLeft;       /* Table on the left with matching column name   左表与匹配的列名*/
				int iLeftCol;    /* Column number of matching column on the left  左表匹配列的列数*/
				int iRightCol;   /* Column number of matching column on the right  右表匹配列的列数*/

				zName = pList->a[j].zName;/*标示符列表中的标示符*/
				iRightCol = columnIndex(pRightTab, zName);/*根据右表和标示符右表的待匹配的列索引返回列号*/
				if (iRightCol<0
					|| !tableAndColumnIndex(pSrc, i + 1, zName, &iLeft, &iLeftCol)/*如果列不存在*/
					){
					sqlite3ErrorMsg(pParse, "cannot join using column %s - column "
						"not present in both tables", zName);/*输出错误信息*/
					return 1;
				}
				addWhereTerm(pParse, pSrc, iLeft, iLeftCol, i + 1, iRightCol,
					isOuter, &p->pWhere);/*如果存在，添加到WHERE子句中*/
			}
		}
	}
	return 0;/*默认返回0，根据上文分析可得出生成了一个inner连接*/
}

/*
** Insert code into "v" that will push the record on the top of the
** stack into the sorter.
** 插入代码"v"，在分类器将会推进记录到栈的顶部。
*/
static void pushOntoSorter(
	Parse *pParse,         /* Parser context  语义分析*/
	ExprList *pOrderBy,    /* The ORDER BY clause   order by子句*/
	Select *pSelect,       /* The whole SELECT statement  整个select语句*/
	int regData            /* Register holding data to be sorted  保持数据有序*/
	){
	Vdbe *v = pParse->pVdbe;/*声明一个虚拟机*/
	int nExpr = pOrderBy->nExpr;/*声明一个ORDERBY表达式*/
	int regBase = sqlite3GetTempRange(pParse, nExpr + 2); /*分配或释放一块连续的寄存器，返回一个整数值，把该值赋给regBase。*/
	int regRecord = sqlite3GetTempReg(pParse); /*分配一个新的寄存器用于控制中间结果。*/
	int op;
	sqlite3ExprCacheClear(pParse); /*清除所有列的缓存条目*/
	sqlite3ExprCodeExprList(pParse, pOrderBy, regBase, 0);/*生成代码，将给定的表达式列表的每个元素的值放到寄存器开始的目标序列。返回元素评估的数量。*/
	sqlite3VdbeAddOp2(v, OP_Sequence, pOrderBy->iECursor, regBase + nExpr);/*将表达式放到VDBE中，再返回一个新的指令地址*/
	sqlite3ExprCodeMove(pParse, regData, regBase + nExpr + 1, 1);/*更改寄存器中的内容，这样做能及时更新寄存器中的列缓存数据*/
	sqlite3VdbeAddOp3(v, OP_MakeRecord, regBase, nExpr + 2, regRecord);/*将nExpr放到当前使用的VDBE中，再返回一个新的指令的地址*/
	if (pSelect->selFlags & SF_UseSorter){/*如果select结构体中selFlags的值是SF_UseSorter，提一下，selFlags的值全是以SF开头，这个表示使用了分拣器了。*/
		op = OP_SorterInsert;/*因为使用分拣器，所以操作符设置为插入分拣器*/
	}
	else{
		op = OP_IdxInsert;/*否则使用索引方式插入*/
	}
	sqlite3VdbeAddOp2(v, op, pOrderBy->iECursor, regRecord);/*将Orderby表达式放到当前使用的VDBE中，然后返回一个新的指令地址*/
	sqlite3ReleaseTempReg(pParse, regRecord);/*释放regRecord寄存器*/
	sqlite3ReleaseTempRange(pParse, regBase, nExpr + 2);/*释放regBase这个连续寄存器，长度是表达式的长度加2*/
	if (pSelect->iLimit){/*如果使用Limit子句*/
		int addr1, addr2;
		int iLimit;
		if (pSelect->iOffset){/*如果使用了Offset偏移量*/
			iLimit = pSelect->iOffset + 1;/*那么Limit的值为偏移量加1*/
		}
		else{
			iLimit = pSelect->iLimit;/*否则等于默认的，从第一个开始计算*/
		}
		addr1 = sqlite3VdbeAddOp1(v, OP_IfZero, iLimit);/*这个地址是结果限制了返回的条数，给的新的指令地址*/
		sqlite3VdbeAddOp2(v, OP_AddImm, iLimit, -1);/*将指令放到当前使用的VDBE，然后返回一个地址*/
		addr2 = sqlite3VdbeAddOp0(v, OP_Goto);/*这个是使用Goto语句之后返回的地址*/
		sqlite3VdbeJumpHere(v, addr1);/*改变addr1的地址，以便VDBE指向下一条指令的地址*/
		sqlite3VdbeAddOp1(v, OP_Last, pOrderBy->iECursor);/*将ORDERBY指令放到当前使用的虚拟机中，返回Last操作的地址*/
		sqlite3VdbeAddOp1(v, OP_Delete, pOrderBy->iECursor);/*将ORDERBY指令放到当前使用的虚拟机中，返回Delete操作的地址*/
		sqlite3VdbeJumpHere(v, addr2);/*改变addr2的地址，以便VDBE指向下一条指令的地址*/
	}
}

/*
** Add code to implement the OFFSET
** 添加代码来实现offset偏移
*/
static void codeOffset(
	Vdbe *v,          /* Generate code into this VM  在虚拟器中生成代码*/
	Select *p,        /* The SELECT statement being coded  select语句被编码*/
	int iContinue     /* Jump here to skip the current record  跳到这里以跳过当前记录*/
	){
	if (p->iOffset && iContinue != 0){/*如果结构体指针p引用的成员iOffset非空且整型iContinue不等于0*/
		int addr;
		sqlite3VdbeAddOp2(v, OP_AddImm, p->iOffset, -1);/*在VDBE中新添加一条指令，返回一个新指令的地址*/
		addr = sqlite3VdbeAddOp1(v, OP_IfNeg, p->iOffset);/*实质上调用sqlite3VdbeAddOp3（）修改指令的地址*/
		sqlite3VdbeAddOp2(v, OP_Goto, 0, iContinue);/*设置跳往的地址*/
		VdbeComment((v, "skip OFFSET records"));/*输入偏移量记录*/
		sqlite3VdbeJumpHere(v, addr);/*改变指定地址的操作，使其指向下一条指令的地址编码*/
	}
}

/*
** Add code that will check to make sure the N registers starting at iMem
** form a distinct entry.  iTab is a sorting index that holds previously
** seen combinations of the N values.  A new entry is made in iTab
** if the current N values are new.
** 添加代码，将检查确保N个寄存器开始iMem形成一个单独的条目。iTab是一个分类索引，
** 预先见到的N值得组合。如果当前的N值是新的，一个新的条目由在iTab中产生。
**
** A jump to addrRepeat is made and the N+1 values are popped from the
** stack if the top N elements are not distinct.
** 如最上面N个元素不明显，则跳转到addrRepeat，N+1个值从栈中弹出。
*/
static void codeDistinct(
	Parse *pParse,     /* Parsing and code generating context 语义和代码生成*/
	int iTab,          /* A sorting index used to test for distinctness 一个排列索引用于唯一性的测试*/
	int addrRepeat,    /* Jump to here if not distinct 如果没有“去除重复”跳到此处*/
	int N,             /* Number of elements 元素数目*/
	int iMem           /* First element 第一个元素*/
	){
	Vdbe *v;/*定义结构体指针v*/
	int r1;

	v = pParse->pVdbe;/*把结构体成员pParse->pVdbe赋给结构体指针v*/
	r1 = sqlite3GetTempReg(pParse); /*分配一个新的寄存器用于控制中间结果，返回的整数赋给r1.*/
	sqlite3VdbeAddOp4Int(v, OP_Found, iTab, addrRepeat, iMem, N);/*把操作的值看成整数，然后添加这个操作符到虚拟机中*/
	sqlite3VdbeAddOp3(v, OP_MakeRecord, iMem, N, r1);/*调用sqlite3VdbeAddOp3（）修改指令的地址*/
	sqlite3VdbeAddOp2(v, OP_IdxInsert, iTab, r1);/*实际也是使用sqlite3VdbeAddOp3()只是参数变为前4个，修改指令的地址*/
	sqlite3ReleaseTempReg(pParse, r1);/*生成代码，将给定的表达式列表的每个元素的值放到寄存器开始的目标序列。返回元素评估的数量。释放寄存器*/
}

#ifndef SQLITE_OMIT_SUBQUERY/*测试SQLITE_OMIT_SUBQUERY是否被宏定义过*/
/*
** Generate an error message when a SELECT is used within a subexpression
** (example:  "a IN (SELECT * FROM table)") but it has more than 1 result
** column.  We do this in a subroutine because the error used to occur
** in multiple places.  (The error only occurs in one place now, but we
** retain the subroutine to minimize code disruption.)
** 当一个select语句中使用子表达式就产生一个错误的信息(例如:a in(select * from table))，
** 但是它有多于1的结果列。我们在子程序中这样做是因为错误通常发生在多个地方。
** (现在错误只发生在一个地方，但是我们保留中断的子程序将代码错误减少到最小。)
*/
static int checkForMultiColumnSelectError(
	Parse *pParse,       /* Parse context. 语义分析 */
	SelectDest *pDest,   /* Destination of SELECT results   select结果的集合*/
	int nExpr            /* Number of result columns returned by SELECT  结果列的数目由select返回*/
	){
	int eDest = pDest->eDest;/*处理结果集*/
	if (nExpr > 1 && (eDest == SRT_Mem || eDest == SRT_Set)){/*如果结果集大于1并且select的结果集是SRT_Mem或SRT_Set*/
		sqlite3ErrorMsg(pParse, "only a single result allowed for "
			"a SELECT that is part of an expression");/*输出错误信息*/
		return 1;
	}
	else{
		return 0;
	}
}
#endif/*终止if*/

/*
** This routine generates the code for the inside of the inner loop
** of a SELECT.
** 这个程序产生代码为了select内连接的内部代码。
**
** If srcTab and nColumn are both zero, then the pEList expressions
** are evaluated in order to get the data for this row.  If nColumn>0
** then data is pulled from srcTab and pEList is used only to get the
** datatypes for each column.
** 如果srcTab和nColumn都是零，那么pEList表达式为了获得行数据进行赋值。
** 如果nColumn>0 那么数据从srcTab中拉出，pEList只用于从每一列获得数据类型。
*/
static void selectInnerLoop(
	Parse *pParse,          /* The parser context 语义分析*/
	Select *p,              /* The complete select statement being coded 完整的select语句被编码*/
	ExprList *pEList,       /* List of values being extracted  输出结果列的语法树*/
	int srcTab,             /* Pull data from this table 从这个表中提取数据*/
	int nColumn,            /* Number of columns in the source table  源表中列的数目*/
	ExprList *pOrderBy,     /* If not NULL, sort results using this key 如果不是NULL，使用这个key对结果进行排序*/
	int distinct,           /* If >=0, make sure results are distinct 如果>=0，确保结果是不同的*/
	SelectDest *pDest,      /* How to dispose of the results 怎样处理结果*/
	int iContinue,          /* Jump here to continue with next row 跳到这里继续下一行*/
	int iBreak              /* Jump here to break out of the inner loop 跳到这里中断内部循环*/
	){
	Vdbe *v = pParse->pVdbe;/*声明一个虚拟机*/
	int i;
	int hasDistinct;        /* True if the DISTINCT keyword is present 如果distinct关键字存在返回true*/
	int regResult;              /* Start of memory holding result set 开始的内存持有结果集*/
	int eDest = pDest->eDest;   /* How to dispose of results 怎样处理结果*/
	int iParm = pDest->iSDParm; /* First argument to disposal method 第一个参数的处理方法*/
	int nResultCol;             /* Number of result columns 结果列的数目*/

	assert(v);/*判断虚拟机*/
	if (NEVER(v == 0)) return;/*如果虚拟机不存在，直接返回*/
	assert(pEList != 0);/*判断表达式列表是否为空*/
	hasDistinct = distinct >= 0;/*赋值“去除重复”操作符*/
	if (pOrderBy == 0 && !hasDistinct){/*如果使用了ORDERBY交hasDistinct取反值*/
		codeOffset(v, p, iContinue);/*设置偏移量，VDBE和select确定，偏移参数是IContinue*/
	}

	/* Pull the requested columns.
	** 从要求的列中取出数据
	*/
	if (nColumn > 0){/*如果表中列的数目大于0*/
		nResultCol = nColumn;/*把列的数目赋给整型nResultCol*/
	}
	else{
		nResultCol = pEList->nExpr;/*否则，赋值为被提取值得列数*/
	}
	if (pDest->iSdst == 0){/*如果查询数据集的写入结果的基址寄存器的值为0*/
		pDest->iSdst = pParse->nMem + 1;/*那么基址寄存器的值设为分析语法树的下一个地址*/
		pDest->nSdst = nResultCol;/*注册寄存器的数量为结果列的数量*/
		pParse->nMem += nResultCol;/*分析树的地址设为自身的再加上结果列的数量*/
	}
	else{
		assert(pDest->nSdst == nResultCol);/*判断结果集中寄存器的个数是否与结果列的列数相同*/
	}
	regResult = pDest->iSdst;/*再把储存结果集的寄存器的地址设为结果集的起始地址*/
	if (nColumn > 0){/*如果行数大于0*/
		for (i = 0; i < nColumn; i++){/*则遍历每一列*/
			sqlite3VdbeAddOp3(v, OP_Column, srcTab, i, regResult + i);/*将队列操作送入到VDBE再返回新的指令地址*/
		}
	}
	else if (eDest != SRT_Exists){/*如果处理的结果集不存在*/
		/* If the destination is an EXISTS(...) expression, the actual
		** values returned by the SELECT are not required.
		** 如果目标是一个EXISTS(...)表达式，由select返回的实际值是不需要的。
		*/
		sqlite3ExprCacheClear(pParse);  /*清除所有列中的内容*/
		sqlite3ExprCodeExprList(pParse, pEList, regResult, eDest == SRT_Output);/*生成代码，将给定的表达式列表的每个元素的值放到寄存器开始的目标序列。返回元素评估的数量。*/
	}
	nColumn = nResultCol;/*给列数赋值结果列的列数*/

	/* If the DISTINCT keyword was present on the SELECT statement
	** and this row has been seen before, then do not make this row
	** part of the result.
	** 如果distinct关键字在select语句中出现，这行之前已经见过，那么这行不作为结果的一部分。
	*/
	if (hasDistinct){/*如果使用了distinct关键字*/
		assert(pEList != 0);/*做断点，判断被提取的值列表是否为空*/
		assert(pEList->nExpr == nColumn);/*被提取的值列表的列数是否等于列数*/
		codeDistinct(pParse, distinct, iContinue, nColumn, regResult);/*进行“去除重复操作”*/
		if (pOrderBy == 0){/*如果没有使用ORDERBY字句 */
			codeOffset(v, p, iContinue);/*使用codeOffset（），设置逐条进行产生结果*/
		}
	}

	switch (eDest){/*定义switch函数，根据参数eDest判断怎样处理结果*/
		/* In this mode, write each query result to the key of the temporary
		** table iParm.
		** 在这种模式下，给临时表iParm写入每个查询结果。
		*/
#ifndef SQLITE_OMIT_COMPOUND_SELECT/*测试SQLITE_OMIT_COMPOUND_SELECT是否被宏定义过*/
	case SRT_Union: {/*如果eDest为SRT_Union，则结果作为关键字存储在索引*/
		int r1;
		r1 = sqlite3GetTempReg(pParse);/*分配一个新的寄存器控制中间结果，返回值赋给r1*/
		sqlite3VdbeAddOp3(v, OP_MakeRecord, regResult, nColumn, r1);/*把OP_MakeRecord（做记录）操作送入VDBE，再返回一个新指令地址*/
		sqlite3VdbeAddOp2(v, OP_IdxInsert, iParm, r1);/*把OP_IdxInsert（索引插入）操作送入VDBE，再返回一个新指令地址*/
		sqlite3ReleaseTempReg(pParse, r1);/*释放寄存器，使其可以从用于其他目的。如果一个寄存器当前被用于列缓存，则dallocation被推迟，直到使用的列寄存器变的陈旧*/
		break;
	}

		/* Construct a record from the query result, but instead of
		** saving that record, use it as a key to delete elements from
		** the temporary table iParm.
		** 构建一个记录的查询结果，但不是保存该记录，将其作为从临时表iParm删除元素的一个键。
		*/
	case SRT_Except: {/*如果eDest为SRT_Except，则从union索引中移除结果*/
		sqlite3VdbeAddOp3(v, OP_IdxDelete, iParm, regResult, nColumn); /*添加一个新的指令VDBE指示当前的列表。返回新指令的地址。*/
		break;
	}
#endif/*终止if*/

		/* 
		** Store the result as data using a unique key.
		** 存储数据使用唯一关键字的结果
		*/
	case SRT_Table:/*如果eDest为SRT_Table，则结果按照自动的rowid自动保存*/
	case SRT_EphemTab: {/*如果eDest为SRT_EphemTab，则创建临时表并存储为像SRT_Table的表*/
		int r1 = sqlite3GetTempReg(pParse); /*分配一个新的寄存器用于控制中间结果，并把返回值赋给r1*/
		testcase(eDest == SRT_Table);/*测试处理的结果集的表名称*/
		testcase(eDest == SRT_EphemTab);/*测试处理的结果集的表的大小*/
		sqlite3VdbeAddOp3(v, OP_MakeRecord, regResult, nColumn, r1);/*添加一个新的指令VDBE指示当前的列表。返回新指令的地址。*/
		if (pOrderBy){/*如果有orderby字句*/
			pushOntoSorter(pParse, pOrderBy, p, r1);/*插入代码"V"，在分选机将会推进记录到栈的顶部*/
		}
		else{
			int r2 = sqlite3GetTempReg(pParse);/*分配一个新的寄存器用于控制中间结果，并把返回值赋给r2*/
			sqlite3VdbeAddOp2(v, OP_NewRowid, iParm, r2);/*把OP_NewRowid（新建记录）操作送入VDBE，再返回一个新指令地址*/
			sqlite3VdbeAddOp3(v, OP_Insert, iParm, r1, r2);/*把OP_Insert（插入记录）操作送入VDBE，再返回一个新指令地址*/
			sqlite3VdbeChangeP5(v, OPFLAG_APPEND);  /*对于最新添加的操作，改变p5操作数的值。*/
			sqlite3ReleaseTempReg(pParse, r2);/*释放寄存器，使其可以从用于其他目的。如果一个寄存器当前被用于列缓存，则dallocation被推迟，直到使用的列寄存器变的陈旧*/
		}
		sqlite3ReleaseTempReg(pParse, r1);/*释放寄存器*/
		break;
	}

#ifndef SQLITE_OMIT_SUBQUERY/*测试SQLITE_OMIT_SUBQUERY是否被宏定义过*/
		/* 
		** If we are creating a set for an "expr IN (SELECT ...)" construct,
		** then there should be a single item on the stack.  Write this
		** item into the set table with bogus data.
		** 如果我们创建一个"expr IN (SELECT ...)"表达式 ，那么在堆栈上就应该
		** 有一个单独的对象。把这个对象写入虚拟数据表。
		*/
	case SRT_Set: {/*如果eDest为SRT_Set，则结果作为关键字存入索引*/
		assert(nColumn == 1);/*设断点，列数等于1*/
		p->affinity = sqlite3CompareAffinity(pEList->a[0].pExpr, pDest->affSdst);/*根据表和结果集，存储结构体的亲和性结果集*/
		if (pOrderBy){
			/* 
			** At first glance you would think we could optimize out the
			** ORDER BY in this case since the order of entries in the set
			** does not matter.  But there might be a LIMIT clause, in which
			** case the order does matter
			** 一开始看的时候，你会以为我们能在这种情况下优化了order by，
			** 但是，使用了LIMIT字句的时候，排序就重要了
			*/
			pushOntoSorter(pParse, pOrderBy, p, regResult);/*推入栈顶*/
		}
		else{
			int r1 = sqlite3GetTempReg(pParse);/*分配一个寄存器，存储中间计算结果*/
			sqlite3VdbeAddOp4(v, OP_MakeRecord, regResult, 1, r1, &p->affinity, 1);/*把OP_MakeRecord操作送入VDBE，再返回一个新指令地址*/
			sqlite3ExprCacheAffinityChange(pParse, regResult, 1);/*记录亲和类型的数据的改变的计数寄存器的起始地址*/
			sqlite3VdbeAddOp2(v, OP_IdxInsert, iParm, r1);/*把OP_IdxInsert操作送入VDBE，再返回一个新指令地址*/
			sqlite3ReleaseTempReg(pParse, r1);/*释放寄存器*/
		}
		break;
	}

		/* 
		** If any row exist in the result set, record that fact and abort.
		** 如果任何一行在结果集中存在，记录这一事实并中止。
		*/
	case SRT_Exists: {/*如果eDest为SRT_Exists，则结果若不为空存储1*/
		sqlite3VdbeAddOp2(v, OP_Integer, 1, iParm);/*把OP_Integer操作送入VDBE，再返回一个新指令地址*/
		/* 
		** The LIMIT clause will terminate the loop for us
		** limit子句将终止我们的循环
		*/
		break;
	}

		/* 
		** If this is a scalar select that is part of an expression, then
		** store the results in the appropriate memory cell and break out
		** of the scan loop.
		** 这个标量选择是表达式的一部分，然后将结果存储在适当的存储单元并中止扫描循环。
		*/
	case SRT_Mem: {/*如果eDest为SRT_Mem，则将结果存储在存储单元*/
		assert(nColumn == 1);/*做断点，判断被提取的值列表是否为空*/
		if (pOrderBy){
			pushOntoSorter(pParse, pOrderBy, p, regResult);/*推入栈顶*/
		}
		else{
			sqlite3ExprCodeMove(pParse, regResult, iParm, 1);/*释放寄存器中的内容，保持寄存器的内容及时更新*/
			/* 
			** The LIMIT clause will jump out of the loop for us
			** limit子句会为我们跳出循环
			*/
		}
		break;
	}
#endif /* #ifndef SQLITE_OMIT_SUBQUERY */
		/* 
		** Send the data to the callback function or to a subroutine.  In the
		** case of a subroutine, the subroutine itself is responsible for
		** popping the data from the stack.
		** 将数据发送到回调函数或子程序。在子程序的情况下，子程序本身负责从堆栈中弹出数据。
		*/
		testcase(eDest == SRT_Coroutine);/*测试处理结果集是否是协同处理*/
		testcase(eDest == SRT_Output);   /*测试处理结果集是否要输出*/
		if (pOrderBy){/*如果包含了OEDERBY子句*/
			int r1 = sqlite3GetTempReg(pParse);/*分配一个寄存器，存储中间计算结果*/
			sqlite3VdbeAddOp3(v, OP_MakeRecord, regResult, nColumn, r1);/*把OP_MakeRecord操作送入VDBE，再返回一个新指令地址*/
			pushOntoSorter(pParse, pOrderBy, p, r1);/*推入栈顶*/
			sqlite3ReleaseTempReg(pParse, r1);/*释放寄存器*/
		}
		else if (eDest == SRT_Coroutine){/*如果处理结果集是协同处理*/
			sqlite3VdbeAddOp1(v, OP_Yield, pDest->iSDParm);/*把OP_Yield操作送入VDBE，再返回一个新指令地址*/
		}
		else{
			sqlite3VdbeAddOp2(v, OP_ResultRow, regResult, nColumn);/*把OP_ResultRow操作送入VDBE，再返回一个新指令地址*/
			sqlite3ExprCacheAffinityChange(pParse, regResult, nColumn);/*记录亲和类型的数据的改变的计数寄存器的起始地址*/
		}
		break;
		}

#if !defined(SQLITE_OMIT_TRIGGER)/*条件编译*/
		/* 
		** Discard the results.  This is used for SELECT statements inside
		** the body of a TRIGGER.  The purpose of such selects is to call
		** user-defined functions that have side effects.  We do not care
		** about the actual results of the select.
		** 丢弃结果。这是用于触发器的select语句。这样选择的目的是要调用用户定义函数。
		** 我们不必关心实际的选择结果。
		*/
	default: {/*默认条件下*/
		assert(eDest == SRT_Discard);/*如果处理结果集是SRT_Discard（舍弃）*/
		break;
	}
#endif/*条件编译结束*/
	}

	/* Jump to the end of the loop if the LIMIT is reached.  Except, if
	** there is a sorter, in which case the sorter has already limited
	** the output for us.
	** 除了有一个分选机，在这种情况下分选机已经限制了我们的输出。
	** 如果符合limit子句的条件则跳转到循环结束。
	*/
	if (pOrderBy == 0 && p->iLimit){/*如果不包含ORDERBY并且含有limit子句*/
		sqlite3VdbeAddOp3(v, OP_IfZero, p->iLimit, iBreak, -1);/*把OP_IfZero操作送入VDBE，再返回一个新指令地址*/
	}
}

/*
** Given an expression list, generate a KeyInfo structure that records
** the collating sequence for each expression in that expression list.
** 给定一个表达式列表，生成一个KeyInfo结构，记录在该表达式列表中的每个表达式的排序序列。
**
** If the ExprList is an ORDER BY or GROUP BY clause then the resulting
** KeyInfo structure is appropriate for initializing a virtual index to
** implement that clause.  If the ExprList is the result set of a SELECT
** then the KeyInfo structure is appropriate for initializing a virtual
** index to implement a DISTINCT test.
** 如果ExprList是一个order by或者group by子句，那么KeyInfo结构体适合初始化虚拟索引去实现
** 这些子句。如果ExprList是select的结果集，那么KeyInfo结构体适合初始化一个虚拟索引去实
** 现DISTINCT测试。
**
** Space to hold the KeyInfo structure is obtain from malloc.  The calling
** function is responsible for seeing that this structure is eventually
** freed.  Add the KeyInfo structure to the P4 field of an opcode using
** P4_KEYINFO_HANDOFF is the usual way of dealing with this.
** 保存KeyInfo结构体的空间是由malloc获得。调用函数负责看到这个结构体最终释放。
** KeyInfo结构添加到使用P4_KEYINFO_HANDOFF P4的一个操作码是通常的处理方式。
*/
static KeyInfo *keyInfoFromExprList(Parse *pParse, ExprList *pList){/*定义静态的结构体指针函数keyInfoFromExprList*/
	sqlite3 *db = pParse->db;/*把结构体类型是pParse的成员变量db赋给结构体类型是sqlite3的指针db*/
	int nExpr;
	KeyInfo *pInfo;/*定义结构体类型是KeyInfo的指针pInfo*/
	struct ExprList_item *pItem;/*定义结构体类型是ExprList_item的指针pItem*/
	int i;

	nExpr = pList->nExpr;/*声明表达式列表中表达式的个数*/
	pInfo = sqlite3DbMallocZero(db, sizeof(*pInfo) + nExpr*(sizeof(CollSeq*) + 1));/*分配并清空内存，分配大小为第二个参数的内存*/
	if (pInfo){/*如果存在关键字结构体*/
		pInfo->aSortOrder = (u8*)&pInfo->aColl[nExpr];/*设置关键信息结构体的排序为关键信息结构体中表达式中含有的关键字，其中aColl[1]表示为每一个关键字进行整理*/
		pInfo->nField = (u16)nExpr;/*对整型nExpr进行强制类型转换成u16，赋给 pInfo->nField*/
		pInfo->enc = ENC(db);/*关键信息结构体中编码方式为db的编码方式*/
		pInfo->db = db;/*关键信息结构体中数据库为当前使用的数据库*/
		for (i = 0, pItem = pList->a; i < nExpr; i++, pItem++){/*遍历当前的表达式列表*/
			CollSeq *pColl;/*定义结构体类型为CollSeq的指针pColl*/
			pColl = sqlite3ExprCollSeq(pParse, pItem->pExpr); /*为表达式pExpr返回默认排序顺序。如果没有默认排序类型，返回0.*/
			if (!pColl){/*如果没有指定排序的方法*/
				pColl = db->pDfltColl;/*将数据库中默认的排序方法赋值给pColl*/
			}
			pInfo->aColl[i] = pColl;/*关键信息结构体中对关键字排序数组中元素对应表达式中排序的名称*/
			pInfo->aSortOrder[i] = pItem->sortOrder;/*关键信息结构体中排序的顺序为语法分析树中语法项表达式的排序方法*/
			/*备注：做标记，我没有看懂这种排序的方法，个人理解为把指定使用某种排序的方式记下来，如果没有使用系统默认的。再把语法树中表达式记下来，两者应该是一个东西，只是表达的方式不一样*/
		}
	}
	return pInfo;/*返回这个关键信息结构体*/
}

#ifndef SQLITE_OMIT_COMPOUND_SELECT/*测试SQLITE_OMIT_COMPOUND_SELECT是否被宏定义过*/
/*
** Name of the connection operator, used for error messages.
** 连接符的名称，用于表示错误消息。
*/
static const char *selectOpName(int id){/*定义静态且是只读的字符型指针selectOpName*/
	char *z;/*定义字符型指针z*/
	switch (id){/*switch函数，判断id的值*/
	case TK_ALL:       z = "UNION ALL";   break;/*如果参数id为TK_ALL，返回字符"UNION ALL"*/
	case TK_INTERSECT: z = "INTERSECT";   break;/*如果参数id为TK_INTERSECT，返回字符"INTERSECT"*/
	case TK_EXCEPT:    z = "EXCEPT";      break;/*如果参数id为TK_EXCEPT，返回字符"EXCEPT"*/
	default:           z = "UNION";       break;/*默认条件下，返回字符"UNION"*/
	}
	return z;  /*返回连接符的名称*/
}
#endif /* SQLITE_OMIT_COMPOUND_SELECT */

#ifndef SQLITE_OMIT_EXPLAIN/*测试SQLITE_OMIT_EXPLAIN是否被宏定义过*/
/*
** Unless an "EXPLAIN QUERY PLAN" command is being processed, this function
** is a no-op. Otherwise, it adds a single row of output to the EQP result,
** where the caption is of the form:
**
**   "USE TEMP B-TREE FOR xxx"
**
** where xxx is one of "DISTINCT", "ORDER BY" or "GROUP BY". Exactly which
** is determined by the zUsage argument.
** 除非一个"EXPLAIN QUERY PLAN"命令正在处理，否则这个功能就是一个空操作。
** 否则，它增加一个单独的输出行到EQP结果，标题的形式为:
** "USE TEMP B-TREE FOR xxx"
** 其中xxx是"distinct","order by",或者"group by"中的一个。究竟是哪个由
** zUsage参数决定。
*/
static void explainTempTable(Parse *pParse, const char *zUsage){
	if (pParse->explain == 2){/*如果语法分析树中的explain是第二个*/
		Vdbe *v = pParse->pVdbe;/*声明一个虚拟机*/
		char *zMsg = sqlite3MPrintf(pParse->db, "USE TEMP B-TREE FOR %s", zUsage);/*把输出的格式的内容传递给zMsg，其中%S 是传入的参数在Usage*/
		sqlite3VdbeAddOp4(v, OP_Explain, pParse->iSelectId, 0, 0, zMsg, P4_DYNAMIC); /*添加一个操作码，其中包括作为指针的p4值。*/
	}
  }
}

/*
** Assign expression b to lvalue a. A second, no-op, version of this macro
** is provided when SQLITE_OMIT_EXPLAIN is defined. This allows the code
** in sqlite3Select() to assign values to structure member variables that
** only exist if SQLITE_OMIT_EXPLAIN is not defined without polluting the
** code with #ifndef directives.
** 赋值表达式b给左值a。第二，无操作符，宏的版本由SQLITE_OMIT_EXPLAIN 定义提供。这允许
** sqlite3Select()的代码给结构体的成员变量分配值，如果SQLITE_OMIT_EXPLAIN没有
** 定义，没有#ifndef 指令的代码其才会存在。
*/
# define explainSetInteger(a, b) a = b/*宏定义*/

#else
/* No-op versions of the explainXXX() functions and macros. explainXXX() 函数和宏无操作符的版本。*/
# define explainTempTable(y,z)
# define explainSetInteger(y,z)
#endif

#if !defined(SQLITE_OMIT_EXPLAIN) && !defined(SQLITE_OMIT_COMPOUND_SELECT)
/*
** Unless an "EXPLAIN QUERY PLAN" command is being processed, this function
** is a no-op. Otherwise, it adds a single row of output to the EQP result,
** where the caption is of one of the two forms:
**
**   "COMPOSITE SUBQUERIES iSub1 and iSub2 (op)"
**   "COMPOSITE SUBQUERIES iSub1 and iSub2 USING TEMP B-TREE (op)"
**
** where iSub1 and iSub2 are the integers passed as the corresponding
** function parameters, and op is the text representation of the parameter
** of the same name. The parameter "op" must be one of TK_UNION, TK_EXCEPT,
** TK_INTERSECT or TK_ALL. The first form is used if argument bUseTmp is
** false, or the second form if it is true.
**
** 除非一个"EXPLAIN QUERY PLAN"命令正在处理，这个功能就是一个空操作。
** 否则，它增加一个单独的输出行到EQP结果，标题的形式为:
** "COMPOSITE SUBQUERIES iSub1 and iSub2 (op)"
** "COMPOSITE SUBQUERIES iSub1 and iSub2 USING TEMP B-TREE (op)"
** iSub1和iSub2整数作为相应的传递函数参数，运算是相同名称的参数
** 的文本表示。参数"op"必是TK_UNION, TK_EXCEPT,TK_INTERSECT或者TK_ALL之一。
** 如果参数bUseTmp是false就使用第一范式，或者如果是true就使用第二范式。
*/
static void explainComposite(
	Parse *pParse,                  /* Parse context 语义分析*/
	int op,                         /* One of TK_UNION, TK_EXCEPT etc.   TK_UNION, TK_EXCEPT等运算符中的一个*/
	int iSub1,                      /* Subquery id 1 子查询id 1*/
	int iSub2,                      /* Subquery id 2 子查询id 2*/
	int bUseTmp                     /* True if a temp table was used 如果临时表被使用就是true*/
	){
	assert(op == TK_UNION || op == TK_EXCEPT || op == TK_INTERSECT || op == TK_ALL);/*测试op是否有TK_UNION或TK_EXCEPT或TK_INTERSECT或TK_ALL*/
	if (pParse->explain == 2){/*如果pParse->explain与字符z相同*/
		Vdbe *v = pParse->pVdbe;/*声明一个虚拟机*/
		char *zMsg = sqlite3MPrintf(/*设置标记信息*/
			pParse->db, "COMPOUND SUBQUERIES %d AND %d %s(%s)", iSub1, iSub2,
			bUseTmp ? "USING TEMP B-TREE " : "", selectOpName(op)
			);/*将子查询1和子查询2的语法内容赋值给zMsg*/
		sqlite3VdbeAddOp4(v, OP_Explain, pParse->iSelectId, 0, 0, zMsg, P4_DYNAMIC);/*将OP_Explain操作交给虚拟机，然后返回一个地址，地址为P4_DYNAMIC指针中的值*/
	}
}
#else
/* No-op versions of the explainXXX() functions and macros. explainXXX()函数和宏的无操作版本。*/
# define explainComposite(v,w,x,y,z)
#endif

/*
** If the inner loop was generated using a non-null pOrderBy argument,
** then the results were placed in a sorter.  After the loop is terminated
** we need to run the sorter and output the results.  The following
** routine generates the code needed to do that.
** 如果内部循环使用一个非空pOrderBy生成参数,然后把结果放置在一个分选机。
** 循环终止后我们需要运行分选机和输出结果。下面的例程生成所需的代码。
*/
static void generateSortTail(
	Parse *pParse,    /* Parsing context 语义分析*/
	Select *p,        /* The SELECT statement   select语句*/
	Vdbe *v,          /* Generate code into this VDBE  在VDBE中生成代码**/
	int nColumn,      /* Number of columns of data 数据种列的数目*/
	SelectDest *pDest /* Write the sorted results here 在这里写入排序结果*/
	){
	int addrBreak = sqlite3VdbeMakeLabel(v);     /* Jump here to exit loop 跳转到这里退出循环*/
	int addrContinue = sqlite3VdbeMakeLabel(v);  /* Jump here for next cycle 跳转到这里进行下一个循环*/
	int addr;
	int iTab;
	int pseudoTab = 0;
	ExprList *pOrderBy = p->pOrderBy;/*将Select结构体中ORDERBY赋值到表达式列表中的ORDERBY表达式属性*/

	int eDest = pDest->eDest;/*将查询结果集中处理方式传递给eDest*/
	int iParm = pDest->iSDParm;/*将查询结果集中处理方式中的参数传递给iParm*/

	int regRow;
	int regRowid;

	iTab = pOrderBy->iECursor;/*把pOrderBy->iECursor赋给整型iTab*/
	regRow = sqlite3GetTempReg(pParse);/*为pParse语法树分配一个寄存器,存储计算的中间结果*/
	if (eDest == SRT_Output || eDest == SRT_Coroutine){/*如果处理方式是SRT_Output（输出）或SRT_Coroutine（协同程序）*/
		pseudoTab = pParse->nTab++;/*逐次将分析语法树中表数传给pseudoTab（虚表）*/
		sqlite3VdbeAddOp3(v, OP_OpenPseudo, pseudoTab, regRow, nColumn);/*将OP_Explain操作交给虚拟机*/
		regRowid = 0;
	}
	else{
		regRowid = sqlite3GetTempReg(pParse);/*为pParse语法树分配一个寄存器,存储计算的中间结果*/
	}
	if (p->selFlags & SF_UseSorter){/*如果Select结构体中的selFlags属性值为SF_UseSorter，使用分拣器（排序程序）*/
		int regSortOut = ++pParse->nMem;/*分配寄存器，个数是分析语法树中内存数+1*/
		int ptab2 = pParse->nTab++;/*将分析语法树中表的个数赋值给ptab2*/
		sqlite3VdbeAddOp3(v, OP_OpenPseudo, ptab2, regSortOut, pOrderBy->nExpr + 2);/*将OP_OpenPseudo（打开虚拟操作）交给VDBE，返回表达式列表中表达式个数的值+2*/
		addr = 1 + sqlite3VdbeAddOp2(v, OP_SorterSort, iTab, addrBreak);/*将OP_SorterSort（分拣器进行排序）交给VDBE，返回的地址+1赋值给addr*/
		codeOffset(v, p, addrContinue);/*设置偏移量，其中addrContinue是下一次循环要调到的地址*/
		sqlite3VdbeAddOp2(v, OP_SorterData, iTab, regSortOut);/*将OP_SorterData操作交给虚拟机*/
		sqlite3VdbeAddOp3(v, OP_Column, ptab2, pOrderBy->nExpr + 1, regRow);/*将OP_Column操作交给虚拟机*/
		sqlite3VdbeChangeP5(v, OPFLAG_CLEARCACHE);/*改变OPFLAG_CLEARCACHE（清除缓存）的操作数，因为地址经过sqlite3VdbeAddOp3和sqlite3VdbeAddOp2（）函数改变了地址*/
	}
	else{
		addr = 1 + sqlite3VdbeAddOp2(v, OP_Sort, iTab, addrBreak);/*将OP_Sort操作交给虚拟机，返回的地址+1*/
		codeOffset(v, p, addrContinue);/*设置偏移量，其中addrContinue是下一次循环要调到的地址*/
		sqlite3VdbeAddOp3(v, OP_Column, iTab, pOrderBy->nExpr + 1, regRow);/*将OP_Column操作交给VDBE，再把OP_Column的地址返回*/
	}
	switch (eDest){/*switch函数，参数eDest，选择结果集的处理方法*/
	case SRT_Table:/*如果eDest为SRT_Table，则结果按照自动的rowid自动保存*/
	case SRT_EphemTab: {/*如果eDest为SRT_EphemTab，则创建临时表并存储为像SRT_Table的表*/
		testcase(eDest == SRT_Table);/*处理方式中是否SRT_Table*/
		testcase(eDest == SRT_EphemTab);/*处理方式中是否SRT_EphemTab*/
		sqlite3VdbeAddOp2(v, OP_NewRowid, iParm, regRowid);/*将OP_NewRowid操作交给VDBE，再返回这个操作的地址*/
		sqlite3VdbeAddOp3(v, OP_Insert, iParm, regRow, regRowid);/*将OP_Insert操作交给VDBE，再返回这个操作的地址*/
		sqlite3VdbeChangeP5(v, OPFLAG_APPEND);/*改变OPFLAG_APPEND（设置路径），因为地址经过sqlite3VdbeAddOp2（）和sqlite3VdbeAddOp3（）函数改变了地址*/
		break;
	}
#ifndef SQLITE_OMIT_SUBQUERY/*测试SQLITE_OMIT_SUBQUERY是否被宏定义过*/
	case SRT_Set: {/*如果eDest为SRT_Set，则结果作为关键字存入索引*/
		assert(nColumn == 1);/*加入断点，判断列数是否等于1*/
		sqlite3VdbeAddOp4(v, OP_MakeRecord, regRow, 1, regRowid, &p->affinity, 1);/*添加一个OP_MakeRecord操作，并将它的值作为一个指针*/
		sqlite3ExprCacheAffinityChange(pParse, regRow, 1); /*记录从iStart开始，发生在iCount寄存器中的改变的事实。*/
		sqlite3VdbeAddOp2(v, OP_IdxInsert, iParm, regRowid);/*将OP_IdxInsert（索引插入）操作交给VDBE，再返回这个操作的地址*/
		break;
	}
	case SRT_Mem: {/*如果eDest为SRT_Mem，则将结果存储在存储单元*/
		assert(nColumn == 1);/*加入断点，判断列数是否等于1*/
		sqlite3ExprCodeMove(pParse, regRow, iParm, 1);/*释放寄存器中的内容，保持寄存器的内容及时更新*/
		/* 
		** The LIMIT clause will terminate the loop for us
		** limit子句将为我们终止循环
		*/
		break;
	}
#endif/*终止if*/
	default: {
		int i;
		assert(eDest == SRT_Output || eDest == SRT_Coroutine); /*插入断点，判断结果集处理类型是否有SRT_Output（输出）或SRT_Coroutine（协同处理）*/
		testcase(eDest == SRT_Output);/*测试是否包含SRT_Output*/
		testcase(eDest == SRT_Coroutine);/*测试是否包含SRT_Coroutine*/
		for (i = 0; i<nColumn; i++){/*遍历列*/
			assert(regRow != pDest->iSdst + i);/*插入断点，判断寄存器的编号值不等于基址寄存器的编号值+i*/
			sqlite3VdbeAddOp3(v, OP_Column, pseudoTab, i, pDest->iSdst + i);/*将OP_Column操作交给VDBE，再返回这个操作的地址*/
			if (i == 0){/*如果没有列*/
				sqlite3VdbeChangeP5(v, OPFLAG_CLEARCACHE);/*改变OPFLAG_CLEARCACHE（清除缓存）的操作数，因为地址经过sqlite3VdbeAddOp3（）函数改变了地址*/
			}
		}
		if (eDest == SRT_Output){/*如果结果集的处理方式是SRT_Output*/
			sqlite3VdbeAddOp2(v, OP_ResultRow, pDest->iSdst, nColumn);/*将OP_ResultRow操作交给VDBE，再返回这个操作的地址*/
			sqlite3ExprCacheAffinityChange(pParse, pDest->iSdst, nColumn);/*处理语法树pParse，寄存器中的亲和性数据*/
		}
		else{
			sqlite3VdbeAddOp1(v, OP_Yield, pDest->iSDParm);/*将OP_Yield操作交给VDBE，再返回这个操作的地址*/
		}
		break;
	}
	}
	sqlite3ReleaseTempReg(pParse, regRow);/*释放寄存器*/
	sqlite3ReleaseTempReg(pParse, regRowid);/*释放寄存器*/

	/* The bottom of the loop
	** 循环的底部
	*/
	sqlite3VdbeResolveLabel(v, addrContinue);/*addrContinue作为下一条插入指令的地址，其中addrContinue能优先调用sqlite3VdbeMakeLabel（）*/
	if (p->selFlags & SF_UseSorter){/*selFlags的值是SF_UseSorter*/
		sqlite3VdbeAddOp2(v, OP_SorterNext, iTab, addr);/*将OP_SorterNext操作交给VDBE，再返回这个操作的地址*/
	}
	else{
		sqlite3VdbeAddOp2(v, OP_Next, iTab, addr);/*将OP_Next操作交给VDBE，再返回这个操作的地址*/
	}
	sqlite3VdbeResolveLabel(v, addrBreak);/*addrBreak作为下一条插入指令的地址，其中addrBreak能优先调用sqlite3VdbeMakeLabel（）*/
	if (eDest == SRT_Output || eDest == SRT_Coroutine){/*如果结果集的处理方式SRT_Output或SRT_Coroutine*/
		sqlite3VdbeAddOp2(v, OP_Close, pseudoTab, 0);/*将OP_Close操作交给VDBE，再返回这个操作的地址*/
	}
	}

/*
** Return a pointer to a string containing the 'declaration type' of the
** expression pExpr. The string may be treated as static by the caller.
**
** The declaration type is the exact datatype definition extracted from the
** original CREATE TABLE statement if the expression is a column. The
** declaration type for a ROWID field is INTEGER. Exactly when an expression
** is considered a column can be complex in the presence of subqueries. The
** result-set expression in all of the following SELECT statements is
** considered a column by this function.
**
**   SELECT col FROM tbl;
**   SELECT (SELECT col FROM tbl;
**   SELECT (SELECT col FROM tbl);
**   SELECT abc FROM (SELECT col AS abc FROM tbl);
**
** The declaration type for any expression other than a column is NULL.
**
**
** 返回一个指向表达式pExpr 包含 'declaration type'的字符串。
** 这个字符串可以视为静态调用者。
** 如果表达式是一列，声明类型是确切的数据类型定义从最初的
** create table 语句中获取。ROWID字段的声明类型是整数。当一个表达式
** 被认为作为一列在子查询中是复杂的。在所有下面的SELECT语句
** 的结果集的表达被认为是这个函数的列。
**   SELECT col FROM tbl;
**   SELECT (SELECT col FROM tbl;
**   SELECT (SELECT col FROM tbl);
**   SELECT abc FROM (SELECT col AS abc FROM tbl);
** 声明类型以外的任何表达式列是空的。
*/
static const char *columnType(/*定义静态且是只读的字符型指针columnType*/
	NameContext *pNC, /*声明一个命名上下文结构体（决定表或者列的名字）*/
	Expr *pExpr,
	const char **pzOriginDb,/*定义只读的字符型二级指针pzOriginDb*/
	const char **pzOriginTab,/*定义只读的字符型二级指针pzOriginTab*/
	const char **pzOriginCol,/*定义只读的字符型二级指针pzOriginCol*/
	){
	char const *zType = 0;
	char const *zOriginDb = 0;
	char const *zOriginTab = 0;
	char const *zOriginCol = 0;
	int j;
	if (NEVER(pExpr == 0) || pNC->pSrcList == 0) return 0;

	switch (pExpr->op){/*遍历表达式中的操作*/
	case TK_AGG_COLUMN:
	case TK_COLUMN: {
		/* 
		** The expression is a column. Locate the table the column is being
		** extracted from in NameContext.pSrcList. This table may be real
		** database table or a subquery.
		** 表达式是一个列。被定位的表的列从NameContext.pSrcList中提取。
		** 这个表可能是真实的数据库表，或者是一个子查询。
		*/
		Table *pTab = 0;            /* Table structure column is extracted from 表结构列被提取*/
		Select *pS = 0;             /* Select the column is extracted from 选择列被提取*/
		int iCol = pExpr->iColumn;  /* Index of column in pTab 索引列在pTab中*/
		testcase(pExpr->op == TK_AGG_COLUMN);/*这个表达式的操作是否是TK_AGG_COLUMN（嵌套列）*/
		testcase(pExpr->op == TK_COLUMN);/*这个表达式的操作是否是TK_COLUMN（列索引）*/
		while (pNC && !pTab){/*命名上下文结构体存在，被提取的表结构列（就是一个被提取的列组成的表）不存在*/
			SrcList *pTabList = pNC->pSrcList;/*命名上下文结构体中列表赋值给描述FROM的来源表或子查询结果的列表*/
			for (j = 0; j<pTabList->nSrc && pTabList->a[j].iCursor != pExpr->iTable; j++);/*遍历查询列表*/
			if (j<pTabList->nSrc){/*如果j小于列表中表的总个数*/
				pTab = pTabList->a[j].pTab;/*赋值列表中的第j-1表给Table结构体的实体变量pTab*/
				pS = pTabList->a[j].pSelect;/*赋值列表中的第j-1表的select结构体给ps*/
			}
			else{
				pNC = pNC->pNext;/*否则，将命名上下文结构体的下一个外部命名上下文赋值给pNC变量*/
			}
		}

		if (pTab == 0){
			/* At one time, code such as "SELECT new.x" within a trigger would
			** cause this condition to run.  Since then, we have restructured how
			** trigger code is generated and so this condition is no longer
			** possible. However, it can still be true for statements like
			** the following:
			**
			**   CREATE TABLE t1(col INTEGER);
			**   SELECT (SELECT t1.col) FROM FROM t1;
			**
			** when columnType() is called on the expression "t1.col" in the
			** sub-select. In this case, set the column type to NULL, even
			** though it should really be "INTEGER".
			**
			** This is not a problem, as the column type of "t1.col" is never
			** used. When columnType() is called on the expression
			** "(SELECT t1.col)", the correct type is returned (see the TK_SELECT
			** branch below.
			**
			**
			** 在同一时间，诸如触发器内"SELECT new.x "代码将导致这种状态运行。自那时以来，
			** 我们已重组触发代码是如何生成的，因此该条件不再可能。但是，它仍然可以适用于
			** 像下面的语句：
			** CREATE TABLE t1(col INTEGER);
			** SELECT (SELECT t1.col) FROM FROM t1;
			** 当columnType()调用在子选择中的表达式"t1.col"。在这种情况下，设置列的类型为空，
			** 即使它确实应该"INTEGER"。
			** 这不是一个问题，因为" t1.col "的列类型是从未使用过。当columnType ()被调用的
			** 表达式"(SELECT t1.col)" ，则返回正确的类型(请参阅下面的TK_SELECT分支)。
			*/
			break;
		}

		assert(pTab && pExpr->pTab == pTab);
		if (pS){
			/* The "table" is actually a sub-select or a view in the FROM clause
			** of the SELECT statement. Return the declaration type and origin
			** data for the result-set column of the sub-select.
			** "表"实际上是一个子选择，或者是一个在select语句的from子句的视图。
			** 返回声明类型和来源数据的子选择的结果集列。
			*/
			if (iCol >= 0 && ALWAYS(iCol < pS->pEList->nExpr)){
				/* If iCol is less than zero, then the expression requests the
				** rowid of the sub-select or view. This expression is legal (see
				** test case misc2.2.2) - it always evaluates to NULL.
				** 如果iCol小于零，则表达式请求子选择或视图的rowid。
				** 这种表达式合法的(见测试案例misc2.2.2)-它始终计算为空。
				*/
				NameContext sNC;
				Expr *p = pS->pEList->a[iCol].pExpr;/*被提取的列组成的select结构体中表达式列表中第i个表达式赋值给p*/
				sNC.pSrcList = pS->pSrc;/*被提取的列组成的select结构体中pSrc（FROM子句）赋值给pSrcList（一个或多个表用来命名的属性）*/
				sNC.pNext = pNC;/*命名上下文结构体赋值给当前命名上下文结构体的next指针*/
				sNC.pParse = pNC->pParse;/*命名上下文结构体中的语法解析树赋值给当前命名上下文结构体的语法解析树*/
				zType = columnType(&sNC, p, &zOriginDb, &zOriginTab, &zOriginCol); /*将生成的属性类型赋值给zType*/
			}
		}
		else if (ALWAYS(pTab->pSchema)){/*pTab表的模式存在*/
			/* A real table *//*一个真实的表*/
			assert(!pS);/*插入断点，判断Select结构体是否为空*/
			if (iCol<0) iCol = pTab->iPKey;/*如果列号小于0，将表中的关键字数组的首元素赋值给ICol*/
			assert(iCol == -1 || (iCol >= 0 && iCol<pTab->nCol));/*插入断点，判断ICol正确，在哪个范围*/
			if (iCol<0){/*如果ICol号小于0*/
				zType = "INTEGER";/*将类型定义为整型*/
				zOriginCol = "rowid";/*关键字为rowid*/
			}
			else{
				zType = pTab->aCol[iCol].zType;/*否则，定义类型为类型表中第iCol的类型*/
				zOriginCol = pTab->aCol[iCol].zName;/*类型的名字为型表中第iCol的名字*/
			}
			zOriginTab = pTab->zName;/*使用默认的名字，定义类型*/
			if (pNC->pParse){/*如果命名上下文结构体中的语法分析树存在*/
				int iDb = sqlite3SchemaToIndex(pNC->pParse->db, pTab->pSchema);/*将Schema的指针转化给命名上下文结构体中分析语法树的db*/
				zOriginDb = pNC->pParse->db->aDb[iDb].zName;/*将上下文语法分析树中的db中第i-1个Db的命名赋值给zOriginDb数据库名*/
			}
		}
		break;
	}
#ifndef SQLITE_OMIT_SUBQUERY
	case TK_SELECT: {
		/* The expression is a sub-select. Return the declaration type and
		** origin info for the single column in the result set of the SELECT
		** statement.
		*/
		/*
		** 这个表达式是子查询。返回一个声明类型和初始信息给select结果集中的一列。
		*/
		NameContext sNC;
		Select *pS = pExpr->x.pSelect;/*将表达式中Select结构体赋值给一个SELECT结构体实体变量*/
		Expr *p = pS->pEList->a[0].pExpr;/*将SELECT的表达式列表中第一个表达式赋值给表达式变量p*/
		assert(ExprHasProperty(pExpr, EP_xIsSelect));/*插入断点，测试是否包含EP_xIsSelect表达式*/
		sNC.pSrcList = pS->pSrc;/*将SELECT结构体中FROM子句的属性赋值给命名上下文结构体中FROM子句列表*/
		sNC.pNext = pNC;/*命名上下文结构体赋值给当前命名上下文结构体的next指针*/
		sNC.pParse = pNC->pParse;/*将命名上下文结构体中分析语法树赋值给当前命名结构体的分析语法树属性*/
		zType = columnType(&sNC, p, &zOriginDb, &zOriginTab, &zOriginCol); /*返回属性类型*/
		break;
	}
#endif
	}

	if (pzOriginDb){/*如果存在原始的数据库*/
		assert(pzOriginTab && pzOriginCol);/*插入断点，判断表和列是否存在*/
		*pzOriginDb = zOriginDb;/*文件中数据库赋值给数据库变量pzOriginDb*/
		*pzOriginTab = zOriginTab;/*文件中表赋值给表变量pzOriginTab*/
		*pzOriginCol = zOriginCol;/*文件中列赋值给列变量pzOriginCol*/
	}
	return zType;/*返回列类型*/
}


/*
** Generate code that will tell the VDBE the declaration types of columns
** in the result set.
**生成的代码会告诉VDBE在结果集中的列的声明类型。
*/
static void generateColumnTypes(
	Parse *pParse,      /* Parser context 语义分析*/
	SrcList *pTabList,  /* List of tables 输出表的集合的语法树*/
	ExprList *pEList    /* Expressions defining the result set 输出结果列的语法树*/
	){
#ifndef SQLITE_OMIT_DECLTYPE/*测试SQLITE_OMIT_DECLTYPE是否被宏定义过*/
	Vdbe *v = pParse->pVdbe;
	int i;
	NameContext sNC;
	sNC.pSrcList = pTabList;
	sNC.pParse = pParse;
	for (i = 0; i < pEList->nExpr; i++){
		Expr *p = pEList->a[i].pExpr;
		const char *zType;
#ifdef SQLITE_ENABLE_COLUMN_METADATA/*测试SQLITE_ENABLE_COLUMN_METADATA是否被宏定义过*/
		const char *zOrigDb = 0;
		const char *zOrigTab = 0;
		const char *zOrigCol = 0;
		zType = columnType(&sNC, p, &zOrigDb, &zOrigTab, &zOrigCol);

		/* The vdbe must make its own copy of the column-type and other
		** column specific strings, in case the schema is reset before this
		** virtual machine is deleted.
		**VDBE必须做出自己的列的类型和其他列的特定字符串的副本，在此情况下，
		**虚拟机被删除之前的模式被重置。
		*/
		sqlite3VdbeSetColName(v, i, COLNAME_DATABASE, zOrigDb, SQLITE_TRANSIENT);
		sqlite3VdbeSetColName(v, i, COLNAME_TABLE, zOrigTab, SQLITE_TRANSIENT);
		sqlite3VdbeSetColName(v, i, COLNAME_COLUMN, zOrigCol, SQLITE_TRANSIENT);
#else
		zType = columnType(&sNC, p, 0, 0, 0);
#endif
		sqlite3VdbeSetColName(v, i, COLNAME_DECLTYPE, zType, SQLITE_TRANSIENT);
	}
#endif /* SQLITE_OMIT_DECLTYPE */
}

/*
** Generate code that will tell the VDBE the names of columns
** in the result set.  This information is used to provide the
** azCol[] values in the callback.
**生成的代码，会告诉VDBE结果集中列的名字。这些信息用于提供在回调中azCol[] 的值。
*/
static void generateColumnNames(
	Parse *pParse,      /* Parser context   解析上下文 */
	SrcList *pTabList,  /* List of tables   列表*/
	ExprList *pEList    /* Expressions defining the result set   定义结果集的表达式*/
	){
	Vdbe *v = pParse->pVdbe;
	int i, j;
	sqlite3 *db = pParse->db;
	int fullNames, shortNames;

#ifndef SQLITE_OMIT_EXPLAIN
	/* If this is an EXPLAIN, skip this step    如果这是一个EXPLAIN, 跳过这一步 */
	if (pParse->explain){
		return;
	}
#endif

	if (pParse->colNamesSet || NEVER(v == 0) || db->mallocFailed) return;
	pParse->colNamesSet = 1;
	fullNames = (db->flags & SQLITE_FullColNames) != 0;
	shortNames = (db->flags & SQLITE_ShortColNames) != 0;
	sqlite3VdbeSetNumCols(v, pEList->nExpr);
	for (i = 0; i < pEList->nExpr; i++){
		Expr *p;
		p = pEList->a[i].pExpr;
		if (NEVER(p == 0)) continue;
		if (pEList->a[i].zName){
			char *zName = pEList->a[i].zName;
			sqlite3VdbeSetColName(v, i, COLNAME_NAME, zName, SQLITE_TRANSIENT);
		}
		else if ((p->op == TK_COLUMN || p->op == TK_AGG_COLUMN) && pTabList){
			Table *pTab;
			char *zCol;
			int iCol = p->iColumn;
			for (j = 0; ALWAYS(j < pTabList->nSrc); j++){
				if (pTabList->a[j].iCursor == p->iTable) break;
			}
			assert(j < pTabList->nSrc);
			pTab = pTabList->a[j].pTab;
			if (iCol < 0) iCol = pTab->iPKey;
			assert(iCol == -1 || (iCol >= 0 && iCol < pTab->nCol));
			if (iCol < 0){
				zCol = "rowid";
			}
			else{
				zCol = pTab->aCol[iCol].zName;
			}
			if (!shortNames && !fullNames){
				sqlite3VdbeSetColName(v, i, COLNAME_NAME,
					sqlite3DbStrDup(db, pEList->a[i].zSpan), SQLITE_DYNAMIC);
			}
			else if (fullNames){
				char *zName = 0;
				zName = sqlite3MPrintf(db, "%s.%s", pTab->zName, zCol);
				sqlite3VdbeSetColName(v, i, COLNAME_NAME, zName, SQLITE_DYNAMIC);
			}
			else{
				sqlite3VdbeSetColName(v, i, COLNAME_NAME, zCol, SQLITE_TRANSIENT);
			}
		}
		else{
			sqlite3VdbeSetColName(v, i, COLNAME_NAME,
				sqlite3DbStrDup(db, pEList->a[i].zSpan), SQLITE_DYNAMIC);
		}
	}
	generateColumnTypes(pParse, pTabList, pEList);
}

/*
** Given a an expression list (which is really the list of expressions
** that form the result set of a SELECT statement) compute appropriate
** column names for a table that would hold the expression list.
**给定一个表达式列表(真正的形成一个SELECT语句结果集的表达式列表)，能计算出一个拥有这些表达式列表的表的合适列名称
** All column names will be unique.
**所有列名将是唯一的
** Only the column names are computed.  Column.zType, Column.zColl,
** and other fields of Column are zeroed.
**只计算列名称。zType、zColl列和其他字段的列都被置零。
** Return SQLITE_OK on success.  If a memory allocation error occurs,
** store NULL in *paCol and 0 in *pnCol and return SQLITE_NOMEM.
**返回SQLITE_OK成功。如果内存分配发生错误,在*paCol里存储空，在*pnCol里存储0，返回SQLITE_NOMEM
*/
static int selectColumnsFromExprList(
	Parse *pParse,          /* Parsing context 解析上下文 */
	ExprList *pEList,       /* Expr list from which to derive column names 源于列名的Expr列表*/
	int *pnCol,             /* Write the number of columns here 把列的数量写在这里*/
	Column **paCol          /* Write the new column list here 把新列列表写在这里*/
	){
	sqlite3 *db = pParse->db;   /* Database connection 数据库连接*/
	int i, j;                   /* Loop counters 循环计数器*/
	int cnt;                    /* Index added to make the name unique 索引的添加使得名字唯一*/
	Column *aCol, *pCol;        /* For looping over result columns 循环结束得到的结果列*/
	int nCol;                   /* Number of columns in the result set 在结果集中的列数*/
	Expr *p;                    /* Expression for a single result column 一个结果列的表达式*/
	char *zName;                /* Column name 列名*/
	int nName;                  /* Size of name in zName[]  在zName[]中名称的大小*/

	if (pEList){
		nCol = pEList->nExpr;
		aCol = sqlite3DbMallocZero(db, sizeof(aCol[0])*nCol);
		testcase(aCol == 0);
	}
	else{
		nCol = 0;
		aCol = 0;
	}
	*pnCol = nCol;
	*paCol = aCol;

	for (i = 0, pCol = aCol; i < nCol; i++, pCol++){
		/* Get an appropriate name for the column
		** 得到一个适当的列的名称
		*/
		p = pEList->a[i].pExpr;
		assert(p->pRight == 0 || ExprHasProperty(p->pRight, EP_IntValue)
			|| p->pRight->u.zToken == 0 || p->pRight->u.zToken[0] != 0);
		if ((zName = pEList->a[i].zName) != 0){
			/* If the column contains an "AS <name>" phrase, use <name> as the name
			**如果列包含一个“AS <name>”短语,使用<name>作为名称
			*/
			zName = sqlite3DbStrDup(db, zName);
		}
		else{
			Expr *pColExpr = p;  /* The expression that is the result column name 结果列名称的表达式*/
			Table *pTab;         /* Table associated with this expression 与表达式相关的表*/
			while (pColExpr->op == TK_DOT){
				pColExpr = pColExpr->pRight;
				assert(pColExpr != 0);
			}
			if (pColExpr->op == TK_COLUMN && ALWAYS(pColExpr->pTab != 0)){
				/* For columns use the column name name 对于列使用列名的名字*/
				int iCol = pColExpr->iColumn;
				pTab = pColExpr->pTab;
				if (iCol < 0) iCol = pTab->iPKey;
				zName = sqlite3MPrintf(db, "%s",
					iCol >= 0 ? pTab->aCol[iCol].zName : "rowid");
			}
			else if (pColExpr->op == TK_ID){
				assert(!ExprHasProperty(pColExpr, EP_IntValue));
				zName = sqlite3MPrintf(db, "%s", pColExpr->u.zToken);
			}
			else{
				/* Use the original text of the column expression as its name 使用的列表达式的原始文本作为它的名字*/
				zName = sqlite3MPrintf(db, "%s", pEList->a[i].zSpan);
			}
		}
		if (db->mallocFailed){
			sqlite3DbFree(db, zName);
			break;
		}

		/* Make sure the column name is unique.  If the name is not unique,
		** append a integer to the name so that it becomes unique.
		** 确保列名是唯一的。如果名字不是唯一的,给名称附加一个整数,这样就变得唯一了
		*/
		nName = sqlite3Strlen30(zName);
		for (j = cnt = 0; j < i; j++){
			if (sqlite3StrICmp(aCol[j].zName, zName) == 0){
				char *zNewName;
				zName[nName] = 0;
				zNewName = sqlite3MPrintf(db, "%s:%d", zName, ++cnt);
				sqlite3DbFree(db, zName);
				zName = zNewName;
				j = -1;
				if (zName == 0) break;
			}
		}
		pCol->zName = zName;
	}
	if (db->mallocFailed){
		for (j = 0; j < i; j++){
			sqlite3DbFree(db, aCol[j].zName);
		}
		sqlite3DbFree(db, aCol);
		*paCol = 0;
		*pnCol = 0;
		return SQLITE_NOMEM;
	}
	return SQLITE_OK;
}

/*
** Add type and collation information to a column list based on
** a SELECT statement.
** 给列列表添加类型和排序信息，基于一个SELECT语句。
**
** The column list presumably came from selectColumnNamesFromExprList().
** The column list has only names, not types or collations.  This
** routine goes through and adds the types and collations.
**列列表大概来自于selectColumnNamesFromExprList()。列列表只有名字,没有类型或排序。这个程序遍历并添加类型和排序。
**
** This routine requires that all identifiers in the SELECT
** statement be resolved.
**这个程序在SELECT语句中要求的所有标识符被决定。
*/
static void selectAddColumnTypeAndCollation(
	Parse *pParse,        /* Parsing contexts 解析上下文*/
	int nCol,             /* Number of columns 列数*/
	Column *aCol,         /* List of columns 列列表*/
	Select *pSelect       /* SELECT used to determine types and collations      SELECT用于确定类型和排序*/
	){
	sqlite3 *db = pParse->db;
	NameContext sNC;
	Column *pCol;
	CollSeq *pColl;
	int i;
	Expr *p;
	struct ExprList_item *a;

	assert(pSelect != 0);
	assert((pSelect->selFlags & SF_Resolved) != 0);
	assert(nCol == pSelect->pEList->nExpr || db->mallocFailed);
	if (db->mallocFailed) return;
	memset(&sNC, 0, sizeof(sNC));
	sNC.pSrcList = pSelect->pSrc;
	a = pSelect->pEList->a;
	for (i = 0, pCol = aCol; i < nCol; i++, pCol++){
		p = a[i].pExpr;
		pCol->zType = sqlite3DbStrDup(db, columnType(&sNC, p, 0, 0, 0));
		pCol->affinity = sqlite3ExprAffinity(p);
		if (pCol->affinity == 0) pCol->affinity = SQLITE_AFF_NONE;
		pColl = sqlite3ExprCollSeq(pParse, p);
		if (pColl){
			pCol->zColl = sqlite3DbStrDup(db, pColl->zName);
		}
	}
}

/*
** Given a SELECT statement, generate a Table structure that describes
** the result set of that SELECT.
**给定一个SELECT语句,生成一个表结构,来描述那个SELECT的结果集
*/
Table *sqlite3ResultSetOfSelect(Parse *pParse, Select *pSelect){
	Table *pTab;
	sqlite3 *db = pParse->db;
	int savedFlags;

	savedFlags = db->flags;
	db->flags &= ~SQLITE_FullColNames;
	db->flags |= SQLITE_ShortColNames;
	sqlite3SelectPrep(pParse, pSelect, 0);
	if (pParse->nErr) return 0;
	while (pSelect->pPrior) pSelect = pSelect->pPrior;
	db->flags = savedFlags;
	pTab = sqlite3DbMallocZero(db, sizeof(Table));
	if (pTab == 0){
		return 0;
	}
	/* The sqlite3ResultSetOfSelect() is only used n contexts where lookaside
	** is disabled
	**sqlite3ResultSetOfSelect()只被使用n上下文,在那里后备是不能用的*/
	assert(db->lookaside.bEnabled == 0);
	pTab->nRef = 1;
	pTab->zName = 0;
	pTab->nRowEst = 1000000;
	selectColumnsFromExprList(pParse, pSelect->pEList, &pTab->nCol, &pTab->aCol);
	selectAddColumnTypeAndCollation(pParse, pTab->nCol, pTab->aCol, pSelect);
	pTab->iPKey = -1;
	if (db->mallocFailed){
		sqlite3DeleteTable(db, pTab);
		return 0;
	}
	return pTab;
}

/*
** Get a VDBE for the given parser context.  Create a new one if necessary.
** If an error occurs, return NULL and leave a message in pParse.
** 对给定的解析器上下文获得一个VDBE，如果需要，创建一个新的VDBE。
** 如果发生一个错误，返回空并且在pParse里留下信息。
*/
Vdbe *sqlite3GetVdbe(Parse *pParse){
	Vdbe *v = pParse->pVdbe;
	if (v == 0){
		v = pParse->pVdbe = sqlite3VdbeCreate(pParse->db);
#ifndef SQLITE_OMIT_TRACE
		if (v){
			sqlite3VdbeAddOp0(v, OP_Trace);
		}
#endif
	}
	return v;
}


/*
** Compute the iLimit and iOffset fields of the SELECT based on the
** pLimit and pOffset expressions.  pLimit and pOffset hold the expressions
** that appear in the original SQL statement after the LIMIT and OFFSET
** keywords.  Or NULL if those keywords are omitted. iLimit and iOffset
** are the integer memory register numbers for counters used to compute
** the limit and offset.  If there is no limit and/or offset, then
** iLimit and iOffset are negative.
**基于pLimit和pOffset表达式计算SELECT中的iLimit和iOffset字段。
**nLimit和nOffset持有这些表达式，这些表达式出现在原始的SQL语句中，在LIMIT和OFFSET关键字之后。
**或者为空如果这些关键词被省略。
**iLimit和iOffset是用来计算限制和偏移量的整数存储寄存器数据计数器
**如果没有限制和/或偏移,然后iLimit和iOffset是负的。
**
**
** This routine changes the values of iLimit and iOffset only if
** a limit or offset is defined by pLimit and pOffset.  iLimit and
** iOffset should have been preset to appropriate default values
** (usually but not always -1) prior to calling this routine.
** Only if pLimit!=0 or pOffset!=0 do the limit registers get
** redefined.  The UNION ALL operator uses this property to force
** the reuse of the same limit and offset registers across multiple
** SELECT statements.
** 这个程序改变了iLimit和 iOffset的值，只有限制或偏移由nLimit和nOffset定义。
**iLimit和iOffset应该是预设到适当的默认值(通常但不总是-1)之前调用这个程序。
**只有nLimit>=0或者nOffset>0做限制寄存器得重新定义。
**这个UNION ALL操作符使用这个属性来迫使相同的限制和偏移暂存器的重用通过多个SELECT语句。
*/
static void computeLimitRegisters(Parse *pParse, Select *p, int iBreak){
	Vdbe *v = 0;
	int iLimit = 0;
	int iOffset;
	int addr1, n;
	if (p->iLimit) return;

	/*
	** "LIMIT -1" always shows all rows.  There is some
	** contraversy about what the correct behavior should be.
	** The current implementation interprets "LIMIT 0" to mean
	** no rows.
	** "LIMIT -1" 总是显示所有行。有一些争议关于什么应该是正确的行为。当前实现解释"LIMIT 0"意味着没有行。
	*/
	sqlite3ExprCacheClear(pParse);
	assert(p->pOffset == 0 || p->pLimit != 0);
	if (p->pLimit){
		p->iLimit = iLimit = ++pParse->nMem;
		v = sqlite3GetVdbe(pParse);
		if (NEVER(v == 0)) return;  /* VDBE should have already been allocated VDBE应该已经分配*/
		if (sqlite3ExprIsInteger(p->pLimit, &n)){
			sqlite3VdbeAddOp2(v, OP_Integer, n, iLimit);
			VdbeComment((v, "LIMIT counter"));
			if (n == 0){
				sqlite3VdbeAddOp2(v, OP_Goto, 0, iBreak);
			}
			else{
				if (p->nSelectRow > (double)n) p->nSelectRow = (double)n;
			}
		}
		else{
			sqlite3ExprCode(pParse, p->pLimit, iLimit);
			sqlite3VdbeAddOp1(v, OP_MustBeInt, iLimit);
			VdbeComment((v, "LIMIT counter"));
			sqlite3VdbeAddOp2(v, OP_IfZero, iLimit, iBreak);
		}
		if (p->pOffset){
			p->iOffset = iOffset = ++pParse->nMem;
			pParse->nMem++;   /* Allocate an extra register for limit+offset 给限制+偏移量分配额外的注册*/
			sqlite3ExprCode(pParse, p->pOffset, iOffset);
			sqlite3VdbeAddOp1(v, OP_MustBeInt, iOffset);
			VdbeComment((v, "OFFSET counter"));
			addr1 = sqlite3VdbeAddOp1(v, OP_IfPos, iOffset);
			sqlite3VdbeAddOp2(v, OP_Integer, 0, iOffset);
			sqlite3VdbeJumpHere(v, addr1);
			sqlite3VdbeAddOp3(v, OP_Add, iLimit, iOffset, iOffset + 1);
			VdbeComment((v, "LIMIT+OFFSET"));
			addr1 = sqlite3VdbeAddOp1(v, OP_IfPos, iLimit);
			sqlite3VdbeAddOp2(v, OP_Integer, -1, iOffset + 1);
			sqlite3VdbeJumpHere(v, addr1);
		}
	}
}

#ifndef SQLITE_OMIT_COMPOUND_SELECT
/*
** Return the appropriate collating sequence for the iCol-th column of
** the result set for the compound-select statement "p".  Return NULL if
** the column has no default collating sequence.
** 返回适当的排序序列，这个排序是针对compound-select语句“p”中的iCol-th列的结果集。
** 如果列没有默认的排序序列，返回NULL。
**
** The collating sequence for the compound select is taken from the
** left-most term of the select that has a collating sequence.
** 复合选择的排序序列来自选择最左边的排序序列。
*/
static CollSeq *multiSelectCollSeq(Parse *pParse, Select *p, int iCol){
	CollSeq *pRet;
	if (p->pPrior){
		pRet = multiSelectCollSeq(pParse, p->pPrior, iCol);
	}
	else{
		pRet = 0;
	}
	assert(iCol >= 0);
	if (pRet == 0 && iCol < p->pEList->nExpr){
		pRet = sqlite3ExprCollSeq(pParse, p->pEList->a[iCol].pExpr);
	}
	return pRet;
}
#endif /* SQLITE_OMIT_COMPOUND_SELECT */

/* Forward reference 向前引用*/
static int multiSelectOrderBy(
	Parse *pParse,        /* Parsing context 解析上下文*/
	Select *p,            /* The right-most of SELECTs to be coded SELECTs的最右边被编码*/
	SelectDest *pDest     /* What to do with query results 如何处理查询结果*/
	);


#ifndef SQLITE_OMIT_COMPOUND_SELECT
/*
** This routine is called to process a compound query form from
** two or more separate queries using UNION, UNION ALL, EXCEPT, or
** INTERSECT
**调用这个程序来处理一个真正的两个的并集或交集查询或者两个以上单独的查询。
**
** "p" points to the right-most of the two queries.  the query on the
** left is p->pPrior.  The left query could also be a compound query
** in which case this routine will be called recursively.
**“p”指向最右边的两个查询。左边的查询是p->pPrior.在递归地将调用这个程序的情况下，左边的查询也可以是复合查询。
**
** The results of the total query are to be written into a destination
** of type eDest with parameter iParm.
**总查询的结果将被写入一个带有参数iParm的eDest类型目标。
**
** Example 1:  Consider a three-way compound SQL statement.  例1:  考虑一个三向复合SQL语句。
**
**     SELECT a FROM t1 UNION SELECT b FROM t2 UNION SELECT c FROM t3
**
** This statement is parsed up as follows:    这句话是解析如下:
**
**     SELECT c FROM t3
**      |
**      `----->  SELECT b FROM t2
**                |
**                `------>  SELECT a FROM t1
**
** The arrows in the diagram above represent the Select.pPrior pointer.
** So if this routine is called with p equal to the t3 query, then
** pPrior will be the t2 query.  p->op will be TK_UNION in this case.
**上面图中的箭头代表Select.pPrior指针。所以如果以p等于t3查询来调用这个程序，然后pPrior将会是t2 查询。
**这种情况下，p- >op将会是 TK_UNION
**
** Notice that because of the way SQLite parses compound SELECTs, the
** individual selects always group from left to right.
** 注意,因为这种方式的SQLite是解析复合选择,单个选择总是从左到右。
*/
static int multiSelect(
	Parse *pParse,        /* Parsing context 解析的上下文 */
	Select *p,            /* The right-most of SELECTs to be coded 最右边的SELECTs将会被编码*/
	SelectDest *pDest     /* What to do with query results 如何处理查询结果*/
	){
	int rc = SQLITE_OK;   /* Success code from a subroutine 来自子程序的成功编码 */
	Select *pPrior;       /* Another SELECT immediately to our left 另一个SELECT立即到我们的左边*/
	Vdbe *v;              /* Generate code to this VDBE 这个VDBE生成代码*/
	SelectDest dest;      /* Alternative data destination 供选择的数据目的地*/
	Select *pDelete = 0;  /* Chain of simple selects to delete 删除简单选择的链*/
	sqlite3 *db;          /* Database connection 数据库连接*/
#ifndef SQLITE_OMIT_EXPLAIN
	int iSub1;            /* EQP id of left-hand query     EQP id左侧的查询*/
	int iSub2;            /* EQP id of right-hand query    EQP id右侧的查询*/
#endif

	/* Make sure there is no ORDER BY or LIMIT clause on prior SELECTs.  Only
	** the last (right-most) SELECT in the series may have an ORDER BY or LIMIT.
	**确保没有任何ORDER BY 或者LIMIT子句在prior SELECTs中。只有在系列中的最后一个(最右)SELECT可能有ORDER BY或者LIMIT。
	*/
	assert(p && p->pPrior);  /* Calling function guarantees this much 调用函数保证这么多*/
	db = pParse->db;
	pPrior = p->pPrior;
	assert(pPrior->pRightmost != pPrior);
	assert(pPrior->pRightmost == p->pRightmost);
	dest = *pDest;
	if (pPrior->pOrderBy){
		sqlite3ErrorMsg(pParse, "ORDER BY clause should come after %s not before",
			selectOpName(p->op));
		rc = 1;
		goto multi_select_end;
	}
	if (pPrior->pLimit){
		sqlite3ErrorMsg(pParse, "LIMIT clause should come after %s not before",
			selectOpName(p->op));
		rc = 1;
		goto multi_select_end;
	}

	v = sqlite3GetVdbe(pParse);
	assert(v != 0);  /* The VDBE already created by calling function    VDBE已经由调用函数创建*/

	/* Create the destination temporary table if necessary   在必要时创建目标临时表
	*/
	if (dest.eDest == SRT_EphemTab){
		assert(p->pEList);
		sqlite3VdbeAddOp2(v, OP_OpenEphemeral, dest.iSDParm, p->pEList->nExpr);
		sqlite3VdbeChangeP5(v, BTREE_UNORDERED);
		dest.eDest = SRT_Table;
	}

	/* Make sure all SELECTs in the statement have the same number of elements
	** in their result sets.
	** 确保所有在声明中SELECTs在他们的结果集中具有相同数量的元素。
	*/
	assert(p->pEList && pPrior->pEList);
	if (p->pEList->nExpr != pPrior->pEList->nExpr){
		if (p->selFlags & SF_Values){
			sqlite3ErrorMsg(pParse, "all VALUES must have the same number of terms");
		}
		else{
			sqlite3ErrorMsg(pParse, "SELECTs to the left and right of %s"
				" do not have the same number of result columns", selectOpName(p->op));
		}
		rc = 1;
		goto multi_select_end;
	}

	/* Compound SELECTs that have an ORDER BY clause are handled separately.
	** 有ORDER BY子句的复合的SELECTs被分别处理。
	*/
	if (p->pOrderBy){
		return multiSelectOrderBy(pParse, p, pDest);
	}

	/* Generate code for the left and right SELECT statements.
	**为左边和右边的SELECT语句生成代码。
	*/
	switch (p->op){
	case TK_ALL: {
		int addr = 0;
		int nLimit;
		assert(!pPrior->pLimit);
		pPrior->pLimit = p->pLimit;
		pPrior->pOffset = p->pOffset;
		explainSetInteger(iSub1, pParse->iNextSelectId);
		rc = sqlite3Select(pParse, pPrior, &dest);
		p->pLimit = 0;
		p->pOffset = 0;
		if (rc){
			goto multi_select_end;
		}
		p->pPrior = 0;
		p->iLimit = pPrior->iLimit;
		p->iOffset = pPrior->iOffset;
		if (p->iLimit){
			addr = sqlite3VdbeAddOp1(v, OP_IfZero, p->iLimit);
			VdbeComment((v, "Jump ahead if LIMIT reached"));
		}
		explainSetInteger(iSub2, pParse->iNextSelectId);
		rc = sqlite3Select(pParse, p, &dest);
		testcase(rc != SQLITE_OK);
		pDelete = p->pPrior;
		p->pPrior = pPrior;
		p->nSelectRow += pPrior->nSelectRow;
		if (pPrior->pLimit
			&& sqlite3ExprIsInteger(pPrior->pLimit, &nLimit)
			&& p->nSelectRow > (double)nLimit
			){
			p->nSelectRow = (double)nLimit;
		}
		if (addr){
			sqlite3VdbeJumpHere(v, addr);
		}
		break;
	}
	case TK_EXCEPT:
	case TK_UNION: {
		int unionTab;    /* Cursor number of the temporary table holding result   有结果的临时表的光标数*/
		u8 op = 0;       /* One of the SRT_ operations to apply to self        SRT_ operations中的一个指定给自己*/
		int priorOp;     /* The SRT_ operation to apply to prior selects   这个SRT_ operation 指定给prior selects*/
		Expr *pLimit, *pOffset; /* Saved values of p->nLimit and p->nOffset    保存p->nLimit和p->nOffset的值*/
		int addr;
		SelectDest uniondest;

		testcase(p->op == TK_EXCEPT);
		testcase(p->op == TK_UNION);
		priorOp = SRT_Union;
		if (dest.eDest == priorOp && ALWAYS(!p->pLimit &&!p->pOffset)){
			/* We can reuse a temporary table generated by a SELECT to our
			** right.
			**我们可以重用一个我们右边的选择生成的临时表
			*/
			assert(p->pRightmost != p);  /* Can only happen for leftward elements
										 ** of a 3-way or more compound    左侧的元素只能发生3种方式或者以上复合*/
			assert(p->pLimit == 0);      /* Not allowed on leftward elements 不允许在左侧的元素*/
			assert(p->pOffset == 0);     /* Not allowed on leftward elements 不允许在左侧的元素*/
			unionTab = dest.iSDParm;
		}
		else{
			/* We will need to create our own temporary table to hold the
			** intermediate results.   我们需要创建自己的临时表来保存中间结果。
			*/
			unionTab = pParse->nTab++;
			assert(p->pOrderBy == 0);
			addr = sqlite3VdbeAddOp2(v, OP_OpenEphemeral, unionTab, 0);
			assert(p->addrOpenEphm[0] == -1);
			p->addrOpenEphm[0] = addr;
			p->pRightmost->selFlags |= SF_UsesEphemeral;
			assert(p->pEList);
		}

		/* Code the SELECT statements to our left  给左边的SELECT语句编码
		*/
		assert(!pPrior->pOrderBy);
		sqlite3SelectDestInit(&uniondest, priorOp, unionTab);
		explainSetInteger(iSub1, pParse->iNextSelectId);
		rc = sqlite3Select(pParse, pPrior, &uniondest);
		if (rc){
			goto multi_select_end;
		}

		/* Code the current SELECT statement    给当前SELECT语句编码
		*/
		if (p->op == TK_EXCEPT){
			op = SRT_Except;
		}
		else{
			assert(p->op == TK_UNION);
			op = SRT_Union;
		}
		p->pPrior = 0;
		pLimit = p->pLimit;
		p->pLimit = 0;
		pOffset = p->pOffset;
		p->pOffset = 0;
		uniondest.eDest = op;
		explainSetInteger(iSub2, pParse->iNextSelectId);
		rc = sqlite3Select(pParse, p, &uniondest);
		testcase(rc != SQLITE_OK);
		/* Query flattening in sqlite3Select() might refill p->pOrderBy.
		** Be sure to delete p->pOrderBy, therefore, to avoid a memory leak.
		** 确保要删除p - > pOrderBy,如此这样,才能避免内存泄漏。
		*/
		sqlite3ExprListDelete(db, p->pOrderBy);
		pDelete = p->pPrior;
		p->pPrior = pPrior;
		p->pOrderBy = 0;
		if (p->op == TK_UNION) p->nSelectRow += pPrior->nSelectRow;
		sqlite3ExprDelete(db, p->pLimit);
		p->pLimit = pLimit;
		p->pOffset = pOffset;
		p->iLimit = 0;
		p->iOffset = 0;

		/* Convert the data in the temporary table into whatever form
		** it is that we currently need.
		**将临时表中的数据转换成任何我们目前所需要的形式那就是。
		*/
		assert(unionTab == dest.iSDParm || dest.eDest != priorOp);
		if (dest.eDest != priorOp){
			int iCont, iBreak, iStart;
			assert(p->pEList);
			if (dest.eDest == SRT_Output){
				Select *pFirst = p;
				while (pFirst->pPrior) pFirst = pFirst->pPrior;
				generateColumnNames(pParse, 0, pFirst->pEList);
			}
			iBreak = sqlite3VdbeMakeLabel(v);
			iCont = sqlite3VdbeMakeLabel(v);
			computeLimitRegisters(pParse, p, iBreak);
			sqlite3VdbeAddOp2(v, OP_Rewind, unionTab, iBreak);
			iStart = sqlite3VdbeCurrentAddr(v);
			selectInnerLoop(pParse, p, p->pEList, unionTab, p->pEList->nExpr,
				0, -1, &dest, iCont, iBreak);
			sqlite3VdbeResolveLabel(v, iCont);
			sqlite3VdbeAddOp2(v, OP_Next, unionTab, iStart);
			sqlite3VdbeResolveLabel(v, iBreak);
			sqlite3VdbeAddOp2(v, OP_Close, unionTab, 0);
		}
		break;
	}
	default: assert(p->op == TK_INTERSECT); {
		int tab1, tab2;
		int iCont, iBreak, iStart;
		Expr *pLimit, *pOffset;
		int addr;
		SelectDest intersectdest;
		int r1;

		/* INTERSECT is different from the others since it requires
		** two temporary tables.  Hence it has its own case.  Begin
		** by allocating the tables we will need.
		**由于INTERSECT需要两个临时表，所以它是不同于其它的。因此它有自己的案例。
		**分配我们需要的表为开始。
		*/
		tab1 = pParse->nTab++;
		tab2 = pParse->nTab++;
		assert(p->pOrderBy == 0);

		addr = sqlite3VdbeAddOp2(v, OP_OpenEphemeral, tab1, 0);
		assert(p->addrOpenEphm[0] == -1);
		p->addrOpenEphm[0] = addr;
		p->pRightmost->selFlags |= SF_UsesEphemeral;
		assert(p->pEList);

		/* Code the SELECTs to our left into temporary table "tab1".   把我们左边的SELECTs 编码到临时表"tab1"中
		*/
		sqlite3SelectDestInit(&intersectdest, SRT_Union, tab1);
		explainSetInteger(iSub1, pParse->iNextSelectId);
		rc = sqlite3Select(pParse, pPrior, &intersectdest);
		if (rc){
			goto multi_select_end;
		}

		/* Code the current SELECT into temporary table "tab2"    把当前的SELECT编码到临时表"tab2"中
		*/
		addr = sqlite3VdbeAddOp2(v, OP_OpenEphemeral, tab2, 0);
		assert(p->addrOpenEphm[1] == -1);
		p->addrOpenEphm[1] = addr;
		p->pPrior = 0;
		pLimit = p->pLimit;
		p->pLimit = 0;
		pOffset = p->pOffset;
		p->pOffset = 0;
		intersectdest.iSDParm = tab2;
		explainSetInteger(iSub2, pParse->iNextSelectId);
		rc = sqlite3Select(pParse, p, &intersectdest);
		testcase(rc != SQLITE_OK);
		pDelete = p->pPrior;
		p->pPrior = pPrior;
		if (p->nSelectRow > pPrior->nSelectRow) p->nSelectRow = pPrior->nSelectRow;
		sqlite3ExprDelete(db, p->pLimit);
		p->pLimit = pLimit;
		p->pOffset = pOffset;

		/* Generate code to take the intersection of the two temporary
		** tables.   给两个临时表的交集生成代码。
		*/
		assert(p->pEList);
		if (dest.eDest == SRT_Output){
			Select *pFirst = p;
			while (pFirst->pPrior) pFirst = pFirst->pPrior;
			generateColumnNames(pParse, 0, pFirst->pEList);
		}
		iBreak = sqlite3VdbeMakeLabel(v);
		iCont = sqlite3VdbeMakeLabel(v);
		computeLimitRegisters(pParse, p, iBreak);
		sqlite3VdbeAddOp2(v, OP_Rewind, tab1, iBreak);
		r1 = sqlite3GetTempReg(pParse);
		iStart = sqlite3VdbeAddOp2(v, OP_RowKey, tab1, r1);
		sqlite3VdbeAddOp4Int(v, OP_NotFound, tab2, iCont, r1, 0);
		sqlite3ReleaseTempReg(pParse, r1);
		selectInnerLoop(pParse, p, p->pEList, tab1, p->pEList->nExpr,
			0, -1, &dest, iCont, iBreak);
		sqlite3VdbeResolveLabel(v, iCont);
		sqlite3VdbeAddOp2(v, OP_Next, tab1, iStart);
		sqlite3VdbeResolveLabel(v, iBreak);
		sqlite3VdbeAddOp2(v, OP_Close, tab2, 0);
		sqlite3VdbeAddOp2(v, OP_Close, tab1, 0);
		break;
	}
	}

	explainComposite(pParse, p->op, iSub1, iSub2, p->op != TK_ALL);

	/* Compute collating sequences used by
	** temporary tables needed to implement the compound select.
	** Attach the KeyInfo structure to all temporary tables.
	**计算排序序列既使用了ORDER by子句，也使用了需要实现复合选择的任何临时表。把KeyInfo结构附加到所有临时表。
	**如果有ORDER BY子句，调用ORDER BY处理。
	**
	** This section is run by the right-most SELECT statement only.
	** SELECT statements to the left always skip this part.  The right-most
	** SELECT might also skip this part if it has no ORDER BY clause and
	** no temp tables are required.
	**这部分是仅仅通过最右边的SELECT语句运行的。
	** 左边的SELECT语句总是跳过这个部分。
	**最右边的SELECT也可能跳过这个部分，如果它没有ORDER BY 子句并且没有临时需要的表的话
	*/
	if (p->selFlags & SF_UsesEphemeral){
		int i;                        /* Loop counter 循环计数器 */
		KeyInfo *pKeyInfo;            /* Collating sequence for the result set 结果集的排序序列 */
		Select *pLoop;                /* For looping through SELECT statements 遍历SELECT语句 */
		CollSeq **apColl;             /* For looping through pKeyInfo->aColl[] 遍历pKeyInfo - > aColl[]*/
		int nCol;                     /* Number of columns in result set 在结果集的列数*/

		assert(p->pRightmost == p);
		nCol = p->pEList->nExpr;
		pKeyInfo = sqlite3DbMallocZero(db,
			sizeof(*pKeyInfo) + nCol*(sizeof(CollSeq*) + 1));
		if (!pKeyInfo){
			rc = SQLITE_NOMEM;
			goto multi_select_end;
		}

		pKeyInfo->enc = ENC(db);
		pKeyInfo->nField = (u16)nCol;

		for (i = 0, apColl = pKeyInfo->aColl; i < nCol; i++, apColl++){
			*apColl = multiSelectCollSeq(pParse, p, i);
			if (0 == *apColl){
				*apColl = db->pDfltColl;
			}
		}

		for (pLoop = p; pLoop; pLoop = pLoop->pPrior){
			for (i = 0; i < 2; i++){
				int addr = pLoop->addrOpenEphm[i];
				if (addr < 0){
					/* If [0] is unused then [1] is also unused.  So we can
					** always safely abort as soon as the first unused slot is found
					**如果[0]没有被使用，所以[1]也没有被使用. 所以我们总能尽快安全地终止第一个发现未使用的位置
					*/
					assert(pLoop->addrOpenEphm[1] < 0);
					break;
				}
				sqlite3VdbeChangeP2(v, addr, nCol);
				sqlite3VdbeChangeP4(v, addr, (char*)pKeyInfo, P4_KEYINFO);
				pLoop->addrOpenEphm[i] = -1;
			}
		}
		sqlite3DbFree(db, pKeyInfo);
	}

multi_select_end:
	pDest->iSdst = dest.iSdst;
	pDest->nSdst = dest.nSdst;
	sqlite3SelectDelete(db, pDelete);
	return rc;
}
#endif /* SQLITE_OMIT_COMPOUND_SELECT */

/*
** Code an output subroutine for a coroutine implementation of a
** SELECT statment.
**
** The data to be output is contained in pIn->iSdst.  There are
** pIn->nSdst columns to be output.  pDest is where the output should
** be sent.
**
** regReturn is the number of the register holding the subroutine
** return address.
**
** If regPrev>0 then it is the first register in a vector that
** records the previous output.  mem[regPrev] is a flag that is false
** if there has been no previous output.  If regPrev>0 then code is
** generated to suppress duplicates.  pKeyInfo is used for comparing
** keys.
**
** If the LIMIT found in p->iLimit is reached, jump immediately to
** iBreak.
*/
static int generateOutputSubroutine(
	Parse *pParse,          /* Parsing context */
	Select *p,              /* The SELECT statement */
	SelectDest *pIn,        /* Coroutine supplying data */
	SelectDest *pDest,      /* Where to send the data */
	int regReturn,          /* The return address register */
	int regPrev,            /* Previous result register.  No uniqueness if 0 */
	KeyInfo *pKeyInfo,      /* For comparing with previous entry */
	int p4type,             /* The p4 type for pKeyInfo */
	int iBreak              /* Jump here if we hit the LIMIT */
	){
	Vdbe *v = pParse->pVdbe;
	int iContinue;
	int addr;

	addr = sqlite3VdbeCurrentAddr(v);
	iContinue = sqlite3VdbeMakeLabel(v);

	/* Suppress duplicates for UNION, EXCEPT, and INTERSECT
	*/
	if (regPrev){
		int j1, j2;
		j1 = sqlite3VdbeAddOp1(v, OP_IfNot, regPrev);
		j2 = sqlite3VdbeAddOp4(v, OP_Compare, pIn->iSdst, regPrev + 1, pIn->nSdst,
			(char*)pKeyInfo, p4type);
		sqlite3VdbeAddOp3(v, OP_Jump, j2 + 2, iContinue, j2 + 2);
		sqlite3VdbeJumpHere(v, j1);
		sqlite3ExprCodeCopy(pParse, pIn->iSdst, regPrev + 1, pIn->nSdst);
		sqlite3VdbeAddOp2(v, OP_Integer, 1, regPrev);
	}
	if (pParse->db->mallocFailed) return 0;

	/* Suppress the first OFFSET entries if there is an OFFSET clause
	*/
	codeOffset(v, p, iContinue);

	switch (pDest->eDest){
		/* Store the result as data using a unique key.
		*/
	case SRT_Table:
	case SRT_EphemTab: {
		int r1 = sqlite3GetTempReg(pParse);
		int r2 = sqlite3GetTempReg(pParse);
		testcase(pDest->eDest == SRT_Table);
		testcase(pDest->eDest == SRT_EphemTab);
		sqlite3VdbeAddOp3(v, OP_MakeRecord, pIn->iSdst, pIn->nSdst, r1);
		sqlite3VdbeAddOp2(v, OP_NewRowid, pDest->iSDParm, r2);
		sqlite3VdbeAddOp3(v, OP_Insert, pDest->iSDParm, r1, r2);
		sqlite3VdbeChangeP5(v, OPFLAG_APPEND);
		sqlite3ReleaseTempReg(pParse, r2);
		sqlite3ReleaseTempReg(pParse, r1);
		break;
	}

#ifndef SQLITE_OMIT_SUBQUERY
		/* If we are creating a set for an "expr IN (SELECT ...)" construct,
		** then there should be a single item on the stack.  Write this
		** item into the set table with bogus data.
		*/
	case SRT_Set: {
		int r1;
		assert(pIn->nSdst == 1);
		p->affinity =
			sqlite3CompareAffinity(p->pEList->a[0].pExpr, pDest->affSdst);
		r1 = sqlite3GetTempReg(pParse);
		sqlite3VdbeAddOp4(v, OP_MakeRecord, pIn->iSdst, 1, r1, &p->affinity, 1);
		sqlite3ExprCacheAffinityChange(pParse, pIn->iSdst, 1);
		sqlite3VdbeAddOp2(v, OP_IdxInsert, pDest->iSDParm, r1);
		sqlite3ReleaseTempReg(pParse, r1);
		break;
	}

#if 0  /* Never occurs on an ORDER BY query */
		/* If any row exist in the result set, record that fact and abort.
		*/
	case SRT_Exists: {
		sqlite3VdbeAddOp2(v, OP_Integer, 1, pDest->iSDParm);
		/* The LIMIT clause will terminate the loop for us */
		break;
	}
#endif

		/* If this is a scalar select that is part of an expression, then
		** store the results in the appropriate memory cell and break out
		** of the scan loop.
		*/
	case SRT_Mem: {
		assert(pIn->nSdst == 1);
		sqlite3ExprCodeMove(pParse, pIn->iSdst, pDest->iSDParm, 1);
		/* The LIMIT clause will jump out of the loop for us */
		break;
	}
#endif /* #ifndef SQLITE_OMIT_SUBQUERY */

		/* The results are stored in a sequence of registers
		** starting at pDest->iSdst.  Then the co-routine yields.
		*/
	case SRT_Coroutine: {
		if (pDest->iSdst == 0){
			pDest->iSdst = sqlite3GetTempRange(pParse, pIn->nSdst);
			pDest->nSdst = pIn->nSdst;
		}
		sqlite3ExprCodeMove(pParse, pIn->iSdst, pDest->iSdst, pDest->nSdst);
		sqlite3VdbeAddOp1(v, OP_Yield, pDest->iSDParm);
		break;
	}

		/* If none of the above, then the result destination must be
		** SRT_Output.  This routine is never called with any other
		** destination other than the ones handled above or SRT_Output.
		**
		** For SRT_Output, results are stored in a sequence of registers.
		** Then the OP_ResultRow opcode is used to cause sqlite3_step() to
		** return the next row of result.
		*/
	default: {
		assert(pDest->eDest == SRT_Output);
		sqlite3VdbeAddOp2(v, OP_ResultRow, pIn->iSdst, pIn->nSdst);
		sqlite3ExprCacheAffinityChange(pParse, pIn->iSdst, pIn->nSdst);
		break;
	}
	}

	/* Jump to the end of the loop if the LIMIT is reached.
	*/
	if (p->iLimit){
		sqlite3VdbeAddOp3(v, OP_IfZero, p->iLimit, iBreak, -1);
	}

	/* Generate the subroutine return
	*/
	sqlite3VdbeResolveLabel(v, iContinue);
	sqlite3VdbeAddOp1(v, OP_Return, regReturn);

	return addr;
}

/*
** Alternative compound select code generator for cases when there
** is an ORDER BY clause.
**
** We assume a query of the following form:
**
**      <selectA>  <operator>  <selectB>  ORDER BY <orderbylist>
**
** <operator> is one of UNION ALL, UNION, EXCEPT, or INTERSECT.  The idea
** is to code both <selectA> and <selectB> with the ORDER BY clause as
** co-routines.  Then run the co-routines in parallel and merge the results
** into the output.  In addition to the two coroutines (called selectA and
** selectB) there are 7 subroutines:
**
**    outA:    Move the output of the selectA coroutine into the output
**             of the compound query.
**
**    outB:    Move the output of the selectB coroutine into the output
**             of the compound query.  (Only generated for UNION and
**             UNION ALL.  EXCEPT and INSERTSECT never output a row that
**             appears only in B.)
**
**    AltB:    Called when there is data from both coroutines and A<B.
**
**    AeqB:    Called when there is data from both coroutines and A==B.
**
**    AgtB:    Called when there is data from both coroutines and A>B.
**
**    EofA:    Called when data is exhausted from selectA.
**
**    EofB:    Called when data is exhausted from selectB.
**
** The implementation of the latter five subroutines depend on which
** <operator> is used:
**
**
**             UNION ALL         UNION            EXCEPT          INTERSECT
**          -------------  -----------------  --------------  -----------------
**   AltB:   outA, nextA      outA, nextA       outA, nextA         nextA
**
**   AeqB:   outA, nextA         nextA             nextA         outA, nextA
**
**   AgtB:   outB, nextB      outB, nextB          nextB            nextB
**
**   EofA:   outB, nextB      outB, nextB          halt             halt
**
**   EofB:   outA, nextA      outA, nextA       outA, nextA         halt
**
** In the AltB, AeqB, and AgtB subroutines, an EOF on A following nextA
** causes an immediate jump to EofA and an EOF on B following nextB causes
** an immediate jump to EofB.  Within EofA and EofB, and EOF on entry or
** following nextX causes a jump to the end of the select processing.
**
** Duplicate removal in the UNION, EXCEPT, and INTERSECT cases is handled
** within the output subroutine.  The regPrev register set holds the previously
** output value.  A comparison is made against this value and the output
** is skipped if the next results would be the same as the previous.
**
** The implementation plan is to implement the two coroutines and seven
** subroutines first, then put the control logic at the bottom.  Like this:
**
**          goto Init
**     coA: coroutine for left query (A)
**     coB: coroutine for right query (B)
**    outA: output one row of A
**    outB: output one row of B (UNION and UNION ALL only)
**    EofA: ...
**    EofB: ...
**    AltB: ...
**    AeqB: ...
**    AgtB: ...
**    Init: initialize coroutine registers
**          yield coA
**          if eof(A) goto EofA
**          yield coB
**          if eof(B) goto EofB
**    Cmpr: Compare A, B
**          Jump AltB, AeqB, AgtB
**     End: ...
**
** We call AltB, AeqB, AgtB, EofA, and EofB "subroutines" but they are not
** actually called using Gosub and they do not Return.  EofA and EofB loop
** until all data is exhausted then jump to the "end" labe.  AltB, AeqB,
** and AgtB jump to either L2 or to one of EofA or EofB.
*/
#ifndef SQLITE_OMIT_COMPOUND_SELECT
static int multiSelectOrderBy(
	Parse *pParse,        /* Parsing context */
	Select *p,            /* The right-most of SELECTs to be coded */
	SelectDest *pDest     /* What to do with query results */
	){
	int i, j;             /* Loop counters */
	Select *pPrior;       /* Another SELECT immediately to our left */
	Vdbe *v;              /* Generate code to this VDBE */
	SelectDest destA;     /* Destination for coroutine A */
	SelectDest destB;     /* Destination for coroutine B */
	int regAddrA;         /* Address register for select-A coroutine */
	int regEofA;          /* Flag to indicate when select-A is complete */
	int regAddrB;         /* Address register for select-B coroutine */
	int regEofB;          /* Flag to indicate when select-B is complete */
	int addrSelectA;      /* Address of the select-A coroutine */
	int addrSelectB;      /* Address of the select-B coroutine */
	int regOutA;          /* Address register for the output-A subroutine */
	int regOutB;          /* Address register for the output-B subroutine */
	int addrOutA;         /* Address of the output-A subroutine */
	int addrOutB = 0;     /* Address of the output-B subroutine */
	int addrEofA;         /* Address of the select-A-exhausted subroutine */
	int addrEofB;         /* Address of the select-B-exhausted subroutine */
	int addrAltB;         /* Address of the A<B subroutine */
	int addrAeqB;         /* Address of the A==B subroutine */
	int addrAgtB;         /* Address of the A>B subroutine */
	int regLimitA;        /* Limit register for select-A */
	int regLimitB;        /* Limit register for select-A */
	int regPrev;          /* A range of registers to hold previous output */
	int savedLimit;       /* Saved value of p->iLimit */
	int savedOffset;      /* Saved value of p->iOffset */
	int labelCmpr;        /* Label for the start of the merge algorithm */
	int labelEnd;         /* Label for the end of the overall SELECT stmt */
	int j1;               /* Jump instructions that get retargetted */
	int op;               /* One of TK_ALL, TK_UNION, TK_EXCEPT, TK_INTERSECT */
	KeyInfo *pKeyDup = 0; /* Comparison information for duplicate removal */
	KeyInfo *pKeyMerge;   /* Comparison information for merging rows */
	sqlite3 *db;          /* Database connection */
	ExprList *pOrderBy;   /* The ORDER BY clause */
	int nOrderBy;         /* Number of terms in the ORDER BY clause */
	int *aPermute;        /* Mapping from ORDER BY terms to result set columns */
#ifndef SQLITE_OMIT_EXPLAIN
	int iSub1;            /* EQP id of left-hand query */
	int iSub2;            /* EQP id of right-hand query */
#endif

	assert(p->pOrderBy != 0);
	assert(pKeyDup == 0); /* "Managed" code needs this.  Ticket #3382. */
	db = pParse->db;
	v = pParse->pVdbe;
	assert(v != 0);       /* Already thrown the error if VDBE alloc failed */
	labelEnd = sqlite3VdbeMakeLabel(v);
	labelCmpr = sqlite3VdbeMakeLabel(v);


	/* Patch up the ORDER BY clause
	*/
	op = p->op;
	pPrior = p->pPrior;
	assert(pPrior->pOrderBy == 0);
	pOrderBy = p->pOrderBy;
	assert(pOrderBy);
	nOrderBy = pOrderBy->nExpr;

	/* For operators other than UNION ALL we have to make sure that
	** the ORDER BY clause covers every term of the result set.  Add
	** terms to the ORDER BY clause as necessary.
	*/
	if (op != TK_ALL){
		for (i = 1; db->mallocFailed == 0 && i <= p->pEList->nExpr; i++){
			struct ExprList_item *pItem;
			for (j = 0, pItem = pOrderBy->a; j < nOrderBy; j++, pItem++){
				assert(pItem->iOrderByCol > 0);
				if (pItem->iOrderByCol == i) break;
			}
			if (j == nOrderBy){
				Expr *pNew = sqlite3Expr(db, TK_INTEGER, 0);
				if (pNew == 0) return SQLITE_NOMEM;
				pNew->flags |= EP_IntValue;
				pNew->u.iValue = i;
				pOrderBy = sqlite3ExprListAppend(pParse, pOrderBy, pNew);
				if (pOrderBy) pOrderBy->a[nOrderBy++].iOrderByCol = (u16)i;
			}
		}
	}

	/* Compute the comparison permutation and keyinfo that is used with
	** the permutation used to determine if the next
	** row of results comes from selectA or selectB.  Also add explicit
	** collations to the ORDER BY clause terms so that when the subqueries
	** to the right and the left are evaluated, they use the correct
	** collation.
	*/
	aPermute = sqlite3DbMallocRaw(db, sizeof(int)*nOrderBy);
	if (aPermute){
		struct ExprList_item *pItem;
		for (i = 0, pItem = pOrderBy->a; i < nOrderBy; i++, pItem++){
			assert(pItem->iOrderByCol>0 && pItem->iOrderByCol <= p->pEList->nExpr);
			aPermute[i] = pItem->iOrderByCol - 1;
		}
		pKeyMerge =
			sqlite3DbMallocRaw(db, sizeof(*pKeyMerge) + nOrderBy*(sizeof(CollSeq*) + 1));
		if (pKeyMerge){
			pKeyMerge->aSortOrder = (u8*)&pKeyMerge->aColl[nOrderBy];
			pKeyMerge->nField = (u16)nOrderBy;
			pKeyMerge->enc = ENC(db);
			for (i = 0; i < nOrderBy; i++){
				CollSeq *pColl;
				Expr *pTerm = pOrderBy->a[i].pExpr;
				if (pTerm->flags & EP_ExpCollate){
					pColl = pTerm->pColl;
				}
				else{
					pColl = multiSelectCollSeq(pParse, p, aPermute[i]);
					pTerm->flags |= EP_ExpCollate;
					pTerm->pColl = pColl;
				}
				pKeyMerge->aColl[i] = pColl;
				pKeyMerge->aSortOrder[i] = pOrderBy->a[i].sortOrder;
			}
		}
	}
	else{
		pKeyMerge = 0;
	}

	/* Reattach the ORDER BY clause to the query.
	*/
	p->pOrderBy = pOrderBy;
	pPrior->pOrderBy = sqlite3ExprListDup(pParse->db, pOrderBy, 0);

	/* Allocate a range of temporary registers and the KeyInfo needed
	** for the logic that removes duplicate result rows when the
	** operator is UNION, EXCEPT, or INTERSECT (but not UNION ALL).
	*/
	if (op == TK_ALL){
		regPrev = 0;
	}
	else{
		int nExpr = p->pEList->nExpr;
		assert(nOrderBy >= nExpr || db->mallocFailed);
		regPrev = sqlite3GetTempRange(pParse, nExpr + 1);
		sqlite3VdbeAddOp2(v, OP_Integer, 0, regPrev);
		pKeyDup = sqlite3DbMallocZero(db,
			sizeof(*pKeyDup) + nExpr*(sizeof(CollSeq*) + 1));
		if (pKeyDup){
			pKeyDup->aSortOrder = (u8*)&pKeyDup->aColl[nExpr];
			pKeyDup->nField = (u16)nExpr;
			pKeyDup->enc = ENC(db);
			for (i = 0; i < nExpr; i++){
				pKeyDup->aColl[i] = multiSelectCollSeq(pParse, p, i);
				pKeyDup->aSortOrder[i] = 0;
			}
		}
	}

	/* Separate the left and the right query from one another
	*/
	p->pPrior = 0;
	sqlite3ResolveOrderGroupBy(pParse, p, p->pOrderBy, "ORDER");
	if (pPrior->pPrior == 0){
		sqlite3ResolveOrderGroupBy(pParse, pPrior, pPrior->pOrderBy, "ORDER");
	}

	/* Compute the limit registers */
	computeLimitRegisters(pParse, p, labelEnd);
	if (p->iLimit && op == TK_ALL){
		regLimitA = ++pParse->nMem;
		regLimitB = ++pParse->nMem;
		sqlite3VdbeAddOp2(v, OP_Copy, p->iOffset ? p->iOffset + 1 : p->iLimit,
			regLimitA);
		sqlite3VdbeAddOp2(v, OP_Copy, regLimitA, regLimitB);
	}
	else{
		regLimitA = regLimitB = 0;
	}
	sqlite3ExprDelete(db, p->pLimit);
	p->pLimit = 0;
	sqlite3ExprDelete(db, p->pOffset);
	p->pOffset = 0;

	regAddrA = ++pParse->nMem;
	regEofA = ++pParse->nMem;
	regAddrB = ++pParse->nMem;
	regEofB = ++pParse->nMem;
	regOutA = ++pParse->nMem;
	regOutB = ++pParse->nMem;
	sqlite3SelectDestInit(&destA, SRT_Coroutine, regAddrA);
	sqlite3SelectDestInit(&destB, SRT_Coroutine, regAddrB);

	/* Jump past the various subroutines and coroutines to the main
	** merge loop
	*/
	j1 = sqlite3VdbeAddOp0(v, OP_Goto);
	addrSelectA = sqlite3VdbeCurrentAddr(v);


	/* Generate a coroutine to evaluate the SELECT statement to the
	** left of the compound operator - the "A" select.
	*/
	VdbeNoopComment((v, "Begin coroutine for left SELECT"));
	pPrior->iLimit = regLimitA;
	explainSetInteger(iSub1, pParse->iNextSelectId);
	sqlite3Select(pParse, pPrior, &destA);
	sqlite3VdbeAddOp2(v, OP_Integer, 1, regEofA);
	sqlite3VdbeAddOp1(v, OP_Yield, regAddrA);
	VdbeNoopComment((v, "End coroutine for left SELECT"));

	/* Generate a coroutine to evaluate the SELECT statement on
	** the right - the "B" select
	*/
	addrSelectB = sqlite3VdbeCurrentAddr(v);
	VdbeNoopComment((v, "Begin coroutine for right SELECT"));
	savedLimit = p->iLimit;
	savedOffset = p->iOffset;
	p->iLimit = regLimitB;
	p->iOffset = 0;
	explainSetInteger(iSub2, pParse->iNextSelectId);
	sqlite3Select(pParse, p, &destB);
	p->iLimit = savedLimit;
	p->iOffset = savedOffset;
	sqlite3VdbeAddOp2(v, OP_Integer, 1, regEofB);
	sqlite3VdbeAddOp1(v, OP_Yield, regAddrB);
	VdbeNoopComment((v, "End coroutine for right SELECT"));

	/* Generate a subroutine that outputs the current row of the A
	** select as the next output row of the compound select.
	*/
	VdbeNoopComment((v, "Output routine for A"));
	addrOutA = generateOutputSubroutine(pParse,
		p, &destA, pDest, regOutA,
		regPrev, pKeyDup, P4_KEYINFO_HANDOFF, labelEnd);

	/* Generate a subroutine that outputs the current row of the B
	** select as the next output row of the compound select.
	*/
	if (op == TK_ALL || op == TK_UNION){
		VdbeNoopComment((v, "Output routine for B"));
		addrOutB = generateOutputSubroutine(pParse,
			p, &destB, pDest, regOutB,
			regPrev, pKeyDup, P4_KEYINFO_STATIC, labelEnd);
	}

	/* Generate a subroutine to run when the results from select A
	** are exhausted and only data in select B remains.
	*/
	VdbeNoopComment((v, "eof-A subroutine"));
	if (op == TK_EXCEPT || op == TK_INTERSECT){
		addrEofA = sqlite3VdbeAddOp2(v, OP_Goto, 0, labelEnd);
	}
	else{
		addrEofA = sqlite3VdbeAddOp2(v, OP_If, regEofB, labelEnd);
		sqlite3VdbeAddOp2(v, OP_Gosub, regOutB, addrOutB);
		sqlite3VdbeAddOp1(v, OP_Yield, regAddrB);
		sqlite3VdbeAddOp2(v, OP_Goto, 0, addrEofA);
		p->nSelectRow += pPrior->nSelectRow;
	}

	/* Generate a subroutine to run when the results from select B
	** are exhausted and only data in select A remains.
	*/
	if (op == TK_INTERSECT){
		addrEofB = addrEofA;
		if (p->nSelectRow > pPrior->nSelectRow) p->nSelectRow = pPrior->nSelectRow;
	}
	else{
		VdbeNoopComment((v, "eof-B subroutine"));
		addrEofB = sqlite3VdbeAddOp2(v, OP_If, regEofA, labelEnd);
		sqlite3VdbeAddOp2(v, OP_Gosub, regOutA, addrOutA);
		sqlite3VdbeAddOp1(v, OP_Yield, regAddrA);
		sqlite3VdbeAddOp2(v, OP_Goto, 0, addrEofB);
	}

	/* Generate code to handle the case of A<B
	*/
	VdbeNoopComment((v, "A-lt-B subroutine"));
	addrAltB = sqlite3VdbeAddOp2(v, OP_Gosub, regOutA, addrOutA);
	sqlite3VdbeAddOp1(v, OP_Yield, regAddrA);
	sqlite3VdbeAddOp2(v, OP_If, regEofA, addrEofA);
	sqlite3VdbeAddOp2(v, OP_Goto, 0, labelCmpr);

	/* Generate code to handle the case of A==B
	*/
	if (op == TK_ALL){
		addrAeqB = addrAltB;
	}
	else if (op == TK_INTERSECT){
		addrAeqB = addrAltB;
		addrAltB++;
	}
	else{
		VdbeNoopComment((v, "A-eq-B subroutine"));
		addrAeqB =
			sqlite3VdbeAddOp1(v, OP_Yield, regAddrA);
		sqlite3VdbeAddOp2(v, OP_If, regEofA, addrEofA);
		sqlite3VdbeAddOp2(v, OP_Goto, 0, labelCmpr);
	}

	/* Generate code to handle the case of A>B
	*/
	VdbeNoopComment((v, "A-gt-B subroutine"));
	addrAgtB = sqlite3VdbeCurrentAddr(v);
	if (op == TK_ALL || op == TK_UNION){
		sqlite3VdbeAddOp2(v, OP_Gosub, regOutB, addrOutB);
	}
	sqlite3VdbeAddOp1(v, OP_Yield, regAddrB);
	sqlite3VdbeAddOp2(v, OP_If, regEofB, addrEofB);
	sqlite3VdbeAddOp2(v, OP_Goto, 0, labelCmpr);

	/* This code runs once to initialize everything.
	*/
	sqlite3VdbeJumpHere(v, j1);
	sqlite3VdbeAddOp2(v, OP_Integer, 0, regEofA);
	sqlite3VdbeAddOp2(v, OP_Integer, 0, regEofB);
	sqlite3VdbeAddOp2(v, OP_Gosub, regAddrA, addrSelectA);
	sqlite3VdbeAddOp2(v, OP_Gosub, regAddrB, addrSelectB);
	sqlite3VdbeAddOp2(v, OP_If, regEofA, addrEofA);
	sqlite3VdbeAddOp2(v, OP_If, regEofB, addrEofB);

	/* Implement the main merge loop
	*/
	sqlite3VdbeResolveLabel(v, labelCmpr);
	sqlite3VdbeAddOp4(v, OP_Permutation, 0, 0, 0, (char*)aPermute, P4_INTARRAY);
	sqlite3VdbeAddOp4(v, OP_Compare, destA.iSdst, destB.iSdst, nOrderBy,
		(char*)pKeyMerge, P4_KEYINFO_HANDOFF);
	sqlite3VdbeAddOp3(v, OP_Jump, addrAltB, addrAeqB, addrAgtB);

	/* Release temporary registers
	*/
	if (regPrev){
		sqlite3ReleaseTempRange(pParse, regPrev, nOrderBy + 1);
	}

	/* Jump to the this point in order to terminate the query.
	*/
	sqlite3VdbeResolveLabel(v, labelEnd);

	/* Set the number of output columns
	*/
	if (pDest->eDest == SRT_Output){
		Select *pFirst = pPrior;
		while (pFirst->pPrior) pFirst = pFirst->pPrior;
		generateColumnNames(pParse, 0, pFirst->pEList);
	}

	/* Reassembly the compound query so that it will be freed correctly
	** by the calling function */
	if (p->pPrior){
		sqlite3SelectDelete(db, p->pPrior);
	}
	p->pPrior = pPrior;

	/*** TBD:  Insert subroutine calls to close cursors on incomplete
	**** subqueries ****/
	explainComposite(pParse, p->op, iSub1, iSub2, 0);
	return SQLITE_OK;
}
#endif

///start update by zhaoyanqi 赵艳琪
#if !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW)
/* Forward Declarations */
static void substExprList(sqlite3*, ExprList*, int, ExprList*);
static void substSelect(sqlite3*, Select *, int, ExprList *);

/*
** Scan through the expression pExpr.  Replace every reference to
** a column in table number iTable with a copy of the iColumn-th
** entry in pEList.  (But leave references to the ROWID column
** unchanged.)
**扫描pexpr表达式。用pEList的 iColumn-th条目的一个副本来更换每一个引用到名为iTable的表的
一列。（但保留到 ROWID列的引用不变）
** This routine is part of the flattening procedure.  A subquery
** whose result set is defined by pEList appears as entry in the
** FROM clause of a SELECT such that the VDBE cursor assigned to that
** FORM clause entry is iTable.  This routine make the necessary
** changes to pExpr so that it refers directly to the source table
** of the subquery rather the result set of the subquery.
次例程是扁平化程序的一部分。一个结果集被在一个SELECT的FROM子句出现的条目定义的子查询，比如
VDBE光标指派给FROM子句条目是iTable。此例程对pExpr进行必要更改以便于它直接指到子查询的源表，
而不是子查询的结果集。*/
static Expr *substExpr(
	sqlite3 *db,        /* Report malloc errors to this connection 对此链接报告内存分配错误*/
	Expr *pExpr,        /* Expr in which substitution occurs在Expr表达式出现替代 */
	int iTable,         /* Table to be substituted 表取代*/
	ExprList *pEList    /* Substitute expressions 替代表达式*/
	){
	if (pExpr == 0) return 0;
	if (pExpr->op == TK_COLUMN && pExpr->iTable == iTable){
		if (pExpr->iColumn < 0){
			pExpr->op = TK_NULL;
		}
		else{
			Expr *pNew;
			assert(pEList != 0 && pExpr->iColumn < pEList->nExpr);
			assert(pExpr->pLeft == 0 && pExpr->pRight == 0);
			pNew = sqlite3ExprDup(db, pEList->a[pExpr->iColumn].pExpr, 0);
			if (pNew && pExpr->pColl){
				pNew->pColl = pExpr->pColl;
			}
			sqlite3ExprDelete(db, pExpr);
			pExpr = pNew;
		}
	}
	else{
		pExpr->pLeft = substExpr(db, pExpr->pLeft, iTable, pEList);
		pExpr->pRight = substExpr(db, pExpr->pRight, iTable, pEList);
		if (ExprHasProperty(pExpr, EP_xIsSelect)){
			substSelect(db, pExpr->x.pSelect, iTable, pEList);
		}
		else{
			substExprList(db, pExpr->x.pList, iTable, pEList);
		}
	}
	return pExpr;
}
static void substExprList(
	sqlite3 *db,         /* Report malloc errors here 在这里报告内存分配错误*/
	ExprList *pList,     /* List to scan and in which to make substitutes 扫描列表并替代*/
	int iTable,          /* Table to be substituted 表取代*/
	ExprList *pEList     /* Substitute values 替代值*/
	){
	int i;
	if (pList == 0) return;
	for (i = 0; i < pList->nExpr; i++){
		pList->a[i].pExpr = substExpr(db, pList->a[i].pExpr, iTable, pEList);
	}
}
static void substSelect(
	sqlite3 *db,         /* Report malloc errors here 在这里报告内存分配错误*/
	Select *p,           /* SELECT statement in which to make substitutions 在这里选择语句替换*/
	int iTable,          /* Table to be replaced 表替换为*/
	ExprList *pEList     /* Substitute values 替代值*/
	){
	SrcList *pSrc;
	struct SrcList_item *pItem;
	int i;
	if (!p) return;
	substExprList(db, p->pEList, iTable, pEList);
	substExprList(db, p->pGroupBy, iTable, pEList);
	substExprList(db, p->pOrderBy, iTable, pEList);
	p->pHaving = substExpr(db, p->pHaving, iTable, pEList);
	p->pWhere = substExpr(db, p->pWhere, iTable, pEList);
	substSelect(db, p->pPrior, iTable, pEList);
	pSrc = p->pSrc;
	assert(pSrc);  /* Even for (SELECT 1) we have: pSrc!=0 but pSrc->nSrc==0 尽管我们有 pSrc!=0，但是pSrc->nSrc==0*/
	if (ALWAYS(pSrc)){
		for (i = pSrc->nSrc, pItem = pSrc->a; i > 0; i--, pItem++){
			substSelect(db, pItem->pSelect, iTable, pEList);
		}
	}
}
#endif /* !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW) */

#if !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW)
/*
** This routine attempts to  subqueries as a performance optimization.该例程尝试展平子查询使其性能优化
** This routine returns 1 if it makes changes and 0 if no flattening occurs.如果该例程改变并且没有扁平化的发生，则该例程返回1
**
** To understand the concept of flattening, consider the following
** query:为了理解扁平化的概念，请考虑以下查询
**
**     SELECT a FROM (SELECT x+y AS a FROM t1 WHERE z<100) WHERE a>5
**选择一个对象（选择 x+y 作为一个对象t1，其中z<100）其中a>5
** The default way of implementing this query is to execute the
** subquery first and store the results in a temporary table, then
** run the outer query on that temporary table.  This requires two
** passes over the data.  Furthermore, because the temporary table
** has no indices, the WHERE clause on the outer query cannot be
** optimized.
** 执行此查询的的默认方式是先执行子查询，并将其结果存储在一个临时表中,然后运行在临时表的外部查询。
这就需要两个通过数据。 此外,由于临时表没有索引，外部查询的WHERE子句不能优化。
** This routine attempts to rewrite queries such as the above into
** a single flat select, like this:
** 此例程试图将如上文所述的查询重写进一个单独平面选择，比如：
**     SELECT x+y AS a FROM t1 WHERE z<100 AND a>5
** 选择x+y作为一个对象t1，其中 z<100并且 a>5
** The code generated for this simpification gives the same result
** but only has to scan the data once.  And because indices might
** exist on the table t1, a complete scan of the data might be
** avoided.
** 为了这个simpification生成的代码给出了相同的结果，但只需扫描一次数据。而且由于指数可能存在与表t1，这可以避免扫描所有数据。
** Flattening is only attempted if all of the following are true:
** 如果满足以下所有条件，那么扁平化就只是一个尝试
**   (1)  The subquery and the outer query do not both use aggregates.
**       子查询和外部查询都不使用聚合。
**   (2)  The subquery is not an aggregate or the outer query is not a join.
**       子查询不是一个聚合或者外部查询不是一个连接。
**   (3)  The subquery is not the right operand of a left outer join
**        (Originally ticket #306.  Strengthened by ticket #3300)
**       子查询不是一个左外连接的右操作。
**   (4)  The subquery is not DISTINCT.
**       子查询是不一样的
**  (**)  At one point restrictions (4) and (5) defined a subset of DISTINCT
**        sub-queries that were excluded from this optimization. Restriction
**        (4) has since been expanded to exclude all DISTINCT subqueries.
**       这时限制（4）和（5）定义一个从这个优化被排除在外的不同的子查询的子集
**   (6)  The subquery does not use aggregates or the outer query is not
**        DISTINCT.
**        这个子查询不使用聚合或外部查询是不同的
**   (7)  The subquery has a FROM clause.  TODO:  For subqueries without
**        A FROM clause, consider adding a FROM close with the special
**        table sqlite_once that consists of a single row containing a
**        single NULL.
**        这个子查询有一个FROM字句。TODO:没有一个FROM字句的子查询，可以考虑添加一个
带有从关闭的特别表组成的单个行的FROM字句，其中包含一个单个空。
**   (8)  The subquery does not use LIMIT or the outer query is not a join.
**   子查询不使用限制或外查询并不是一种链接。
**   (9)  The subquery does not use LIMIT or the outer query does not use
**        aggregates.
**    子查询不使用限制或外部查询不使用聚合
**  (10)  The subquery does not use aggregates or the outer query does not
**        use LIMIT.
**   子查询不使用聚合或外部查询不使用限制。
**  (11)  The subquery and the outer query do not both have ORDER BY clauses.
**   子查询和外部查询不都有 ORDER 子句。
**  (**)  Not implemented.  Subsumed into restriction (3).  Was previously
**        a separate restriction deriving from ticket #350.
**    不执行。 归入限制(3)，以前是一个来自票#350的单独限制。
**  (13)  The subquery and outer query do not both use LIMIT.
**   子查询和外部查询都不使用限制
**  (14)  The subquery does not use OFFSET.
**  子查询不使用偏移
**  (15)  The outer query is not part of a compound select or the
**        subquery does not have a LIMIT clause.
**        (See ticket #2339 and ticket [02a8e81d44]).
**    外部查询并不是一个复合选择的一部分，或者说子查询没有限制子句。
**  (16)  The outer query is not an aggregate or the subquery does
**        not contain.  (Ticket #2942)  This used to not matter
**        until we introduced the group_concat() function.
**    外查询并不是一个聚合或者说子查询不包含 ORDER BY。(票 #2942)这在我们介绍 group_concat()
函数之前无关紧要。
**  (17)  The sub-query is not a compound select, or it is a UNION ALL
**        compound clause made up entirely of non-aggregate queries, and
**        the parent query:
** 子查询并不是一种复合选择,或者说它是一个全部由非聚合查询组成的复合句，父查询：
**          * is not itself part of a compound select,本身并不是一个复合选择的一部分,
**          * is not an aggregate or DISTINCT query, and
**          * is not a join
**  不是一个聚合或DISTINCT查询，而且没有一个链接
**        The parent and sub-query may contain WHERE clauses. Subject to
**        rules (11), (13) and (14), they may also contain ORDER BY,
**        LIMIT and OFFSET clauses.  The subquery cannot use any compound
**        operator other than UNION ALL because all the other compound
**        operators have an implied DISTINCT which is disallowed by
**        restriction (4).
** 父和子查询可能包含Where子句。由规则(11), (13)和 (14)可知，他们也可能包含ORDER BY， LIMIT
和 OFFSET 子句。子查询不能使用任何UNION ALL以外的任何复合操作，因为所有其它复合操作有一个隐含
DISTINCT，这由于限制而不被允许。
**        Also, each component of the sub-query must return the same number
**        of result columns. This is actually a requirement for any compound
**        SELECT statement, but all the code here does is make sure that no
**        such (illegal) sub-query is flattened. The caller will detect the
**        syntax error and return a detailed message.
**  另外,每个子查询的组件必须返还相同数量的结果列。这实际上是一个对于任何复合SELECT语句的要求，
但在这里的所有代码是确保没有这种(非法的)子查询是扁平化的。主叫方将会检测到语法错误并返回一个
详细的讯息。
**  (18)  If the sub-query is a compound select, then all terms of the
**        ORDER by clause of the parent must be simple references to
**        columns of the sub-query.
** 如果子查询是一个复合选择,然后所有父子句的 ORDER条目必须简单提到列的子查询。
**  (19)  The subquery does not use LIMIT or the outer query does not
**        have a WHERE clause.
** 子查询不使用限制或外部查询没有一个WHERE 子句。
**  (20)  If the sub-query is a compound select, then it must not use
**        an ORDER BY clause.  Ticket #3773.  We could relax this constraint
**        somewhat by saying that the terms of the ORDER BY clause must
**        appear as unmodified result columns in the outer query.  But we
**        have other optimizations in mind to deal with that case.
** 如果子查询是一个复合选择,那么它就必须不能使用一个ORDER子句。票#3773.我们可以通过说这个ORDER子句
条目显示为在外部查询未修改结果列来放宽这一限制。但是在这种情况下，我们有其他的方法优化。
**  (21)  The subquery does not use LIMIT or the outer query is not
**        DISTINCT.  (See ticket [752e1646fc]).
** 该子查询不使用限制或外部查询是不同的。
** In this routine, the "p" parameter is a pointer to the outer query.
** The subquery is p->pSrc->a[iFrom].  isAgg is true if the outer query
** uses aggregates and subqueryIsAgg is true if the subquery uses aggregates.
**在该例程,“P”参数是一个指向外部查询的指针。子查询是p->pSrc->a[iFrom].如果外查询用聚合和子查询，
则为真
** If flattening is not attempted, this routine is a no-op and returns 0.
** If flattening is attempted this routine returns 1.
**如果没有扁平化，此例程是一个无操作并返回0
如果扁平化，则返回1.
** All of the expression analysis must occur on both the outer query and
** the subquery before this routine runs.
在此例程运行之前，所有的表达式的分析必须发生在外部查询和子查询中。*/
static int flattenSubquery(
	Parse *pParse,       /* Parsing context 解析环境*/
	Select *p,           /* The parent or outer SELECT statement父选择或外选择声明 */
	int iFrom,           /* Index in p->pSrc->a[] of the inner subquery在内子查询的 p->pSrc->a[]中的索引*/
	int isAgg,           /* True if outer SELECT uses aggregate functions 如果外选择使用聚合函数，则返回True*/
	int subqueryIsAgg    /* True if the subquery uses aggregate functions 如果子查询使用聚合函数，则返回True*/
	){
	const char *zSavedAuthContext = pParse->zAuthContext;
	Select *pParent;
	Select *pSub;       /* The inner query or "subquery" 内部查询或“子查询”*/
	Select *pSub1;      /* Pointer to the rightmost select in sub-query指针移到子查询最右边的选择 */
	SrcList *pSrc;      /* The FROM clause of the outer query 子句的外部查询*/
	SrcList *pSubSrc;   /* The FROM clause of the subquery 子句的子查询*/
	ExprList *pList;    /* The result set of the outer query 外部查询的结果集*/
	int iParent;        /* VDBE cursor number of the pSub result set temp table 结果集临时表的VDBE光标数字*/
	int i;              /* Loop counter循环计数器 */
	Expr *pWhere;                    /* The WHERE clause  WHERE子句*/
	struct SrcList_item *pSubitem;   /* The subquery 子查询*/
	sqlite3 *db = pParse->db;

	/* Check to see if flattening is permitted.  Return 0 if not.检查是否允许扁平化，否则返回0
	*/
	assert(p != 0);
	assert(p->pPrior == 0);  /* Unable to flatten compound queries无法展平复合查询 */
	if (db->flags & SQLITE_QueryFlattener) return 0;
	pSrc = p->pSrc;
	assert(pSrc && iFrom >= 0 && iFrom < pSrc->nSrc);
	pSubitem = &pSrc->a[iFrom];
	iParent = pSubitem->iCursor;
	pSub = pSubitem->pSelect;
	assert(pSub != 0);
	if (isAgg && subqueryIsAgg) return 0;                 /* Restriction (1)  限制（1）*/
	if (subqueryIsAgg && pSrc->nSrc>1) return 0;          /* Restriction (2)  限制（2）*/
	pSubSrc = pSub->pSrc;
	assert(pSubSrc);
	/* Prior to version 3.1.2, when LIMIT and OFFSET had to be simple constants,
	** not arbitrary expresssions, we allowed some combining of LIMIT and OFFSET
	** because they could be computed at compile-time.  But when LIMIT and OFFSET
	** became arbitrary expressions, we were forced to add restrictions (13)
	** and (14).
	在3.1.2版之前，当限制和偏移量必须是简单常量，并非任意表达式时，我们允许一些合并的限制和偏移
	量，因为它们可以在编译时计算。但是当限制和偏移量成为任意表达式时,我们不得不限制*/
	if (pSub->pLimit && p->pLimit) return 0;              /* Restriction (13) */
	if (pSub->pOffset) return 0;                          /* Restriction (14) */
	if (p->pRightmost && pSub->pLimit){
		return 0;                                            /* Restriction (15) */
	}
	if (pSubSrc->nSrc == 0) return 0;                       /* Restriction (7)  */
	if (pSub->selFlags & SF_Distinct) return 0;           /* Restriction (5)  */
	if (pSub->pLimit && (pSrc->nSrc > 1 || isAgg)){
		return 0;         /* Restrictions (8)(9) */
	}
	if ((p->selFlags & SF_Distinct) != 0 && subqueryIsAgg){
		return 0;         /* Restriction (6)  */
	}
	if (p->pOrderBy && pSub->pOrderBy){
		return 0;                                           /* Restriction (11) */
	}
	if (isAgg && pSub->pOrderBy) return 0;                /* Restriction (16) */
	if (pSub->pLimit && p->pWhere) return 0;              /* Restriction (19) */
	if (pSub->pLimit && (p->selFlags & SF_Distinct) != 0){
		return 0;         /* Restriction (21) */
	}

	/* OBSOLETE COMMENT 1:过时注释1：
	** Restriction 3:  If the subquery is a join, make sure the subquery is
	** not used as the right operand of an outer join.  Examples of why this
	** is not allowed:
	** 限制3:如果子查询是一个连接,请确保子查询并非用作右操作数的一个外部链接。
	此种情况不允许的原因示例：
	**         t1 LEFT OUTER JOIN (t2 JOIN t3)
	**        T1左外部联接(T2和T3)
	** If we flatten the above, we would get
	**  如果我们将上述扁平化,我们将会得到
	**         (t1 LEFT OUTER JOIN t2) JOIN t3
	**       (T1左外部联接T2)连接到T3
	** which is not at all the same thing.
	**并不是在所有的是同一回事
	** OBSOLETE COMMENT 2: 过时评论2:
	** Restriction 12:  If the subquery is the right operand of a left outer
	** join, make sure the subquery has no WHERE clause.
	限制12:如果子查询是一个左外连接的右操作，请确保将子查询没有Where子句。
	** An examples of why this is not allowed:
	**一个例子：为什么不允许存在这种情况:
	**         t1 LEFT OUTER JOIN (SELECT * FROM t2 WHERE t2.x>0)
	**       T1左外部联接(select*FROM T2,T2.x>0)
	** If we flatten the above, we would get
	** 如果我们将上述扁平化,我们将会得到
	**         (t1 LEFT OUTER JOIN t2) WHERE t2.x>0
	**         (T1左外部联接T2),T2.x>0
	** But the t2.x>0 test will always fail on a NULL row of t2, which
	** effectively converts the OUTER JOIN into an INNER JOIN.
	**但T2.x>0测试将总是在T2的一个空行失败,它有效地将外部连接到一个内部链接。
	** THIS OVERRIDES OBSOLETE COMMENTS 1 AND 2 ABOVE:此操作会覆盖已过时注释1和2段:
	** Ticket #3300 shows that flattening the right term of a LEFT JOIN
	** is fraught with danger.  Best to avoid the whole thing.  If the
	** subquery is the right term of a LEFT JOIN, then do not flatten.
	票#3300显示,扁平化一个左联接正确术语是充满危险的。最好避免整个东西。 如果
	子查询是一个左联接正确术语,则不要将其扁平化。*/
	if ((pSubitem->jointype & JT_OUTER) != 0){
		return 0;
	}

	/* Restriction 17: If the sub-query is a compound SELECT, then it must
	** use only the UNION ALL operator. And none of the simple select queries
	** that make up the compound SELECT are allowed to be aggregate or distinct
	** queries.限制17:如果子查询是一个复合选择,那么它就必须仅使用UNION所有的操作符。而且不存在允许
	复合选择被聚合或不同查询的简单选择查询。*/
	if (pSub->pPrior){
		if (pSub->pOrderBy){
			return 0;  /* Restriction 20 */
		}
		if (isAgg || (p->selFlags & SF_Distinct) != 0 || pSrc->nSrc != 1){
			return 0;
		}
		for (pSub1 = pSub; pSub1; pSub1 = pSub1->pPrior){
			testcase((pSub1->selFlags & (SF_Distinct | SF_Aggregate)) == SF_Distinct);
			testcase((pSub1->selFlags & (SF_Distinct | SF_Aggregate)) == SF_Aggregate);
			assert(pSub->pSrc != 0);
			if ((pSub1->selFlags & (SF_Distinct | SF_Aggregate)) != 0
				|| (pSub1->pPrior && pSub1->op != TK_ALL)
				|| pSub1->pSrc->nSrc < 1
				|| pSub->pEList->nExpr != pSub1->pEList->nExpr
				){
				return 0;
			}
			testcase(pSub1->pSrc->nSrc > 1);
		}

		/* Restriction 18. */
		if (p->pOrderBy){
			int ii;
			for (ii = 0; ii < p->pOrderBy->nExpr; ii++){
				if (p->pOrderBy->a[ii].iOrderByCol == 0) return 0;
			}
		}
	}

	/***** If we reach this point, flattening is permitted.
	   如果达到这一点，允许扁平化*****/

	/* Authorize the subquery 授权子查询*/
	pParse->zAuthContext = pSubitem->zName;
	TESTONLY(i = ) sqlite3AuthCheck(pParse, SQLITE_SELECT, 0, 0, 0);
	testcase(i == SQLITE_DENY);
	pParse->zAuthContext = zSavedAuthContext;

	/* If the sub-query is a compound SELECT statement, then (by restrictions
	** 17 and 18 above) it must be a UNION ALL and the parent query must
	** be of the form:
	**如果子查询是一个复合SELECT语句,然后(以上限制17和18)它必须是一个UNION ALL，并且父查询必须
	在这个form中。
	**     SELECT <expr-list> FROM (<sub-query>) <where-clause>
	**followed by any ORDER BY, LIMIT and/or OFFSET clauses. This block
	** creates N-1 copies of the parent query without any ORDER BY, LIMIT or
	** OFFSET clauses and joins them to the left-hand-side of the original
	** using UNION ALL operators. In this case N is the number of simple
	** select statements in the compound sub-query.
	**从(<子查询>)<Where子句>中选择<expr-列表>，接着是任何顺序、限制或偏移量子句。
	这一块创建N1的副本的父查询,且无需任何顺序、限制或偏移量子句，而且把它们用原始的UNION ALL 操作
	的左侧。在这种情况下,n是复合子查询中的简单查询语句。
	** Example 示例：
	**
	**     SELECT a+1 FROM (
	**        SELECT x FROM tab
	**        UNION ALL
	**        SELECT y FROM tab
	**        UNION ALL
	**        SELECT abs(z*2) FROM tab2
	**     ) WHERE a!=5 ORDER BY 1
	**
	** Transformed into:
	**
	**     SELECT x+1 FROM tab WHERE x+1!=5
	**     UNION ALL
	**     SELECT y+1 FROM tab WHERE y+1!=5
	**     UNION ALL
	**     SELECT abs(z*2)+1 FROM tab2 WHERE abs(z*2)+1!=5
	**     ORDER BY 1
	**
	** We call this the "compound-subquery flattening".我们将其称为“复合型子查询的扁平化”。
	*/
	for (pSub = pSub->pPrior; pSub; pSub = pSub->pPrior){
		Select *pNew;
		ExprList *pOrderBy = p->pOrderBy;
		Expr *pLimit = p->pLimit;
		Select *pPrior = p->pPrior;
		p->pOrderBy = 0;
		p->pSrc = 0;
		p->pPrior = 0;
		p->pLimit = 0;
		pNew = sqlite3SelectDup(db, p, 0);
		p->pLimit = pLimit;
		p->pOrderBy = pOrderBy;
		p->pSrc = pSrc;
		p->op = TK_ALL;
		p->pRightmost = 0;
		if (pNew == 0){
			pNew = pPrior;
		}
		else{
			pNew->pPrior = pPrior;
			pNew->pRightmost = 0;
		}
		p->pPrior = pNew;
		if (db->mallocFailed) return 1;
	}

	/* Begin flattening the iFrom-th entry of the FROM clause
	** in the outer query.开始在外部查询中将FROM clause中的iFrom-th entry扁平化
	*/
	pSub = pSub1 = pSubitem->pSelect;

	/* Delete the transient table structure associated with the
	** subquery删除与子查询相关联的瞬态表结构
	*/
	sqlite3DbFree(db, pSubitem->zDatabase);
	sqlite3DbFree(db, pSubitem->zName);
	sqlite3DbFree(db, pSubitem->zAlias);
	pSubitem->zDatabase = 0;
	pSubitem->zName = 0;
	pSubitem->zAlias = 0;
	pSubitem->pSelect = 0;

	/* Defer deleting the Table object associated with the
	** subquery until code generation is
	** complete, since there may still exist Expr.pTab entries that
	** refer to the subquery even after flattening.  Ticket #3346.
	**推迟删除与子查询相关连的表格对象，直到代码完全生成，因为仍可能存在指向甚至扁平化的子查询的
	EXPR.ptab条目。票#3346。
	**请参阅后的子查询甚至整平滤板。
	** pSubitem->pTab is always non-NULL by test restrictions and tests above.
	通过测试限制和上述测试，psubitem->ptab总是非空的。
	*/
	if (ALWAYS(pSubitem->pTab != 0)){
		Table *pTabToDel = pSubitem->pTab;
		if (pTabToDel->nRef == 1){
			Parse *pToplevel = sqlite3ParseToplevel(pParse);
			pTabToDel->pNextZombie = pToplevel->pZombieTab;
			pToplevel->pZombieTab = pTabToDel;
		}
		else{
			pTabToDel->nRef--;
		}
		pSubitem->pTab = 0;
	}

	/* The following loop runs once for each term in a compound-subquery
	** flattening (as described above).  If we are doing a different kind
	** of flattening - a flattening other than a compound-subquery flattening -
	** then this loop only runs once.
	**以下循环运行时,每次在一个复合子查询的扁平化。（如上所描述）。如果我们这样做一个不同种类扁平化-一个
	不同于复合子查询的扁平化，然后这个循环仅运行一次。
	** This loop moves all of the FROM elements of the subquery into the
	** the FROM clause of the outer query.  Before doing this, remember
	** the cursor number for the original outer query FROM element in
	** iParent.  The iParent cursor will never be used.  Subsequent code
	** will scan expressions looking for iParent references and replace
	** those references with expressions that resolve to the subquery FROM
	** elements we are now copying in.
	此环路将所有的子查询的FROM元素移动到外查询的FROM子句中。在执行此操作之前,请记住在iParent的
	用于原始外部查询中的元素的光标数量。iparent的光标将不再使用。后续代码将扫描查找表达式来寻找iparent引用和
	更换引用有解决我们现在复制的子查询FROM元素的表达式。*/
	for (pParent = p; pParent; pParent = pParent->pPrior, pSub = pSub->pPrior){
		int nSubSrc;
		u8 jointype = 0;
		pSubSrc = pSub->pSrc;     /* FROM clause of subquery  FROM子句的子查询*/
		nSubSrc = pSubSrc->nSrc;  /* Number of terms in subquery FROM clause 在FROM子句的子查询
		的数字*/
		pSrc = pParent->pSrc;     /* FROM clause of the outer query 外部查询的FROM子句*/

		if (pSrc){
			assert(pParent == p);  /* First time through the loop 第一次执行循环*/
			jointype = pSubitem->jointype;
		}
		else{
			assert(pParent != p);  /* 2nd and subsequent times through the loop
			第二次和随后的执行循环*/
			pSrc = pParent->pSrc = sqlite3SrcListAppend(db, 0, 0, 0);
			if (pSrc == 0){
				assert(db->mallocFailed);
				break;
			}
		}

		/* The subquery uses a single slot of the FROM clause of the outer
		** query.  If the subquery has more than one element in its FROM clause,
		** then expand the outer query to make space for it to hold all elements
		** of the subquery.
		**该子查询用一个单个的对外查询的FROM子句的跟踪。如果子查询在它的From子句有多个元素,那么展开外部查询来为
		保持子查询的所有元素而创造空间。
		** Example:示例：
		**
		**    SELECT * FROM tabA, (SELECT * FROM sub1, sub2), tabB;
		** The outer query has 3 slots in its FROM clause.  One slot of the
		** outer query (the middle slot) is used by the subquery.  The next
		** block of code will expand the out query to 4 slots.  The middle
		** slot is expanded to two slots in order to make space for the
		** two elements in the FROM clause of the subquery.
		外查询在它的FROM子句有三个跟踪。外查询的一个跟踪（中间的跟踪）用于子查询。下一个代码块将扩大外查询
		到4个跟踪。中间那个扩大成两个是为了为子查询的FROM子句创造空间。*/
		if (nSubSrc > 1){
			pParent->pSrc = pSrc = sqlite3SrcListEnlarge(db, pSrc, nSubSrc - 1, iFrom + 1);
			if (db->mallocFailed){
				break;
			}
		}

		/* Transfer the FROM clause terms from the subquery into the
		** outer query.从子查询中把FROM字句转移到外部查询中
		*/
		for (i = 0; i < nSubSrc; i++){
			sqlite3IdListDelete(db, pSrc->a[i + iFrom].pUsing);
			pSrc->a[i + iFrom] = pSubSrc->a[i];
			memset(&pSubSrc->a[i], 0, sizeof(pSubSrc->a[i]));
		}
		pSrc->a[iFrom].jointype = jointype;

		/* Now begin substituting subquery result set expressions for
		** references to the iParent in the outer query.
		** 现在开始把提到的子查询结果集表达式替换到外查询的iParent中
		** Example:比如：
		**
		**   SELECT a+5, b*10 FROM (SELECT x*3 AS a, y+10 AS b FROM t1) WHERE a>b;
		**   \                     \_____________ subquery __________/          /
		**    \_____________________ outer query ______________________________/
		**
		** We look at every expression in the outer query and every place we see
		** "a" we substitute "x*3" and every place we see "b" we substitute "y+10".
		我们看外部查询中的每个表达式和每个我们看到"a"，我们取代"x*3的地方，以及每个我们看到"B"
		我们替代"y+10"的地方。*/
		pList = pParent->pEList;
		for (i = 0; i < pList->nExpr; i++){
			if (pList->a[i].zName == 0){
				const char *zSpan = pList->a[i].zSpan;
				if (ALWAYS(zSpan)){
					pList->a[i].zName = sqlite3DbStrDup(db, zSpan);
				}
			}
		}
		substExprList(db, pParent->pEList, iParent, pSub->pEList);
		if (isAgg){
			substExprList(db, pParent->pGroupBy, iParent, pSub->pEList);
			pParent->pHaving = substExpr(db, pParent->pHaving, iParent, pSub->pEList);
		}
		if (pSub->pOrderBy){
			assert(pParent->pOrderBy == 0);
			pParent->pOrderBy = pSub->pOrderBy;
			pSub->pOrderBy = 0;
		}
		else if (pParent->pOrderBy){
			substExprList(db, pParent->pOrderBy, iParent, pSub->pEList);
		}
		if (pSub->pWhere){
			pWhere = sqlite3ExprDup(db, pSub->pWhere, 0);
		}
		else{
			pWhere = 0;
		}
		if (subqueryIsAgg){
			assert(pParent->pHaving == 0);
			pParent->pHaving = pParent->pWhere;
			pParent->pWhere = pWhere;
			pParent->pHaving = substExpr(db, pParent->pHaving, iParent, pSub->pEList);
			pParent->pHaving = sqlite3ExprAnd(db, pParent->pHaving,
				sqlite3ExprDup(db, pSub->pHaving, 0));
			assert(pParent->pGroupBy == 0);
			pParent->pGroupBy = sqlite3ExprListDup(db, pSub->pGroupBy, 0);
		}
		else{
			pParent->pWhere = substExpr(db, pParent->pWhere, iParent, pSub->pEList);
			pParent->pWhere = sqlite3ExprAnd(db, pParent->pWhere, pWhere);
		}

		/* The flattened query is distinct if either the inner or the
		** outer query is distinct.如果内部或外部查询是不同的，那么扁平化查询也不一样。
		*/
		pParent->selFlags |= pSub->selFlags & SF_Distinct;

		/*
		** SELECT ... FROM (SELECT ... LIMIT a OFFSET b) LIMIT x OFFSET y;
		**搜索... (搜索... 限制一个偏移量b)限制X偏移量Y;
		** One is tempted to try to add a and b to combine the limits.  But this
		** does not work if either limit is negative.一个是想方设法要添加A和B来与限制相结合，
		但是如果限制是负值就不行了。
		*/
		if (pSub->pLimit){
			pParent->pLimit = pSub->pLimit;
			pSub->pLimit = 0;
		}
	}

	/* Finially, delete what is left of the subquery and return
	** success. 最后，删除子查询的剩余部分并返回
	*/
	sqlite3SelectDelete(db, pSub1);

	return 1;
}
#endif /* !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW) */

/*
** Analyze the SELECT statement passed as an argument to see if it
** is a min() or max() query. Return WHERE_ORDERBY_MIN or WHERE_ORDERBY_MAX if
** it is, or 0 otherwise. At present, a query is considered to be
** a min()/max() query if:
**分析参数传递的 SELECT语句来看它是否是一个最小值或最大值查询。如果是，则返回WHERE_ORDERBY_MIN或WHERE_ORDERBY_MAX，
否则返回0.目前，一个查询被认为是一个最小或最大值查询，如果：
**   1. There is a single object in the FROM clause.
** 在From子句中有一个单一对象。
**   2. There is a single expression in the result set, and it is
**      either min(x) or max(x), where x is a column reference.
在结果集中有一个单一表达式，并且它要么最小值，要么最大值，其中x为一个列参考。*/
static u8 minMaxQuery(Select *p){
	Expr *pExpr;
	ExprList *pEList = p->pEList;

	if (pEList->nExpr != 1) return WHERE_ORDERBY_NORMAL;
	pExpr = pEList->a[0].pExpr;
	if (pExpr->op != TK_AGG_FUNCTION) return 0;
	if (NEVER(ExprHasProperty(pExpr, EP_xIsSelect))) return 0;
	pEList = pExpr->x.pList;
	if (pEList == 0 || pEList->nExpr != 1) return 0;
	if (pEList->a[0].pExpr->op != TK_AGG_COLUMN) return WHERE_ORDERBY_NORMAL;
	assert(!ExprHasProperty(pExpr, EP_IntValue));
	if (sqlite3StrICmp(pExpr->u.zToken, "min") == 0){
		return WHERE_ORDERBY_MIN;
	}
	else if (sqlite3StrICmp(pExpr->u.zToken, "max") == 0){
		return WHERE_ORDERBY_MAX;
	}
	return WHERE_ORDERBY_NORMAL;
}

/*
** The select statement passed as the first argument is an aggregate query.
** The second argment is the associated aggregate-info object. This
** function tests if the SELECT is of the form:
** 选择语句传递作为第一个参数是一个汇总查询。第二个参数是关联聚合的信息对象如果 SELECT 是以下子句，
这个功能可以测试：
**   SELECT count(*) FROM <tbl>
**
** where table is a database table, not a sub-select or view. If the query
** does match this pattern, then a pointer to the Table object representing
** <tbl> is returned. Otherwise, 0 is returned.
其中表是一个数据库表,而不是一个子选择或视图。 如果该查询不匹配这一模式,那么返回代表表对象的指针,
否则，返回0.*/
static Table *isSimpleCount(Select *p, AggInfo *pAggInfo){
	Table *pTab;
	Expr *pExpr;

	assert(!p->pGroupBy);

	if (p->pWhere || p->pEList->nExpr != 1
		|| p->pSrc->nSrc != 1 || p->pSrc->a[0].pSelect
		){
		return 0;
	}
	pTab = p->pSrc->a[0].pTab;
	pExpr = p->pEList->a[0].pExpr;
	assert(pTab && !pTab->pSelect && pExpr);

	if (IsVirtual(pTab)) return 0;
	if (pExpr->op != TK_AGG_FUNCTION) return 0;
	if (NEVER(pAggInfo->nFunc == 0)) return 0;
	if ((pAggInfo->aFunc[0].pFunc->flags&SQLITE_FUNC_COUNT) == 0) return 0;
	if (pExpr->flags&EP_Distinct) return 0;

	return pTab;
}

/*
** If the source-list item passed as an argument was augmented with an
** INDEXED BY clause, then try to locate the specified index. If there
** was such a clause and the named index cannot be found, return
** SQLITE_ERROR and leave an error in pParse. Otherwise, populate
** pFrom->pIndex and return SQLITE_OK.
如果源列表中的项目作为参数传递,增加了一个索引的子句,则尝试找到指定的索引。如果有这样一个子句，并且这个已命名
索引不能找到，返回SQLITE_ERROR并在 pParse中留下一个错误。否则，填充pFrom->pIndex并返回SQLITE_OK.
?*/
int sqlite3IndexedByLookup(Parse *pParse, struct SrcList_item *pFrom){
	if (pFrom->pTab && pFrom->zIndex){
		Table *pTab = pFrom->pTab;
		char *zIndex = pFrom->zIndex;
		Index *pIdx;
		for (pIdx = pTab->pIndex;
			pIdx && sqlite3StrICmp(pIdx->zName, zIndex);
			pIdx = pIdx->pNext
			);
		if (!pIdx){
			sqlite3ErrorMsg(pParse, "no such index: %s", zIndex, 0);
			pParse->checkSchema = 1;
			return SQLITE_ERROR;
		}
		pFrom->pIndex = pIdx;
	}
	return SQLITE_OK;
}

/*
** This routine is a Walker callback for "expanding" a SELECT statement.
** "Expanding" means to do the following:
**此例程是一个 Walker回调为“扩大”一个SELECT语句。
**    (1)  Make sure VDBE cursor numbers have been assigned to every
**         element of the FROM clause.
**    确保vdbe光标号已分配给每个 FROM子句的元素。
**    (2)  Fill in the pTabList->a[].pTab fields in the SrcList that
**         defines FROM clause.  When views appear in the FROM clause,
**         fill pTabList->a[].pSelect with a copy of the SELECT statement
**         that implements the view.  A copy is made of the view's SELECT
**         statement so that we can freely modify or delete that statement
**         without worrying about messing up the presistent representation
**         of the view.
**   在定义FROM子句的SrcList中填写 pTabList->a[].pTab字段。当视图出现在From子句时，填写
ptablist->[].pselect，用SELECT语句的副本实现了视图。一个副本是由视图的SELECT语句形成的，
以便我们可以自由地修改或删除该语句，而不用担心弄砸了这个视图的持久代表性。
**    (3)  Add terms to the WHERE clause to accomodate the NATURAL keyword
**         on joins and the ON and USING clause of joins.
**  添加到WHERE子句以适应在链接上的自然关键字和ON以及USING链接子句。
**    (4)  Scan the list of columns in the result set (pEList) looking
**         for instances of the "*" operator or the TABLE.* operator.
**         If found, expand each "*" to be every column in every table
**         and TABLE.* to be every column in TABLE.
**  扫描结果集(pelist)的列表来查找"*"操作或TABLE.*操作的实例。如果找到,展开每个"*"作为每个表格
的每一列，每一个TABLE.*作为TABLE的每一列。
*/
static int selectExpander(Walker *pWalker, Select *p){
	Parse *pParse = pWalker->pParse;
	int i, j, k;
	SrcList *pTabList;
	ExprList *pEList;
	struct SrcList_item *pFrom;
	sqlite3 *db = pParse->db;

	if (db->mallocFailed){
		return WRC_Abort;
	}
	if (NEVER(p->pSrc == 0) || (p->selFlags & SF_Expanded) != 0){
		return WRC_Prune;
	}
	p->selFlags |= SF_Expanded;
	pTabList = p->pSrc;
	pEList = p->pEList;

	/* Make sure cursor numbers have been assigned to all entries in
	** the FROM clause of the SELECT statement.
	确保光标编号是否已指定SELECT语句的FROM子句中的所有条目*/
	sqlite3SrcListAssignCursors(pParse, pTabList);

	/* Look up every table named in the FROM clause of the select.  If
	** an entry of the FROM clause is a subquery instead of a table or view,
	** then create a transient table structure to describe the subquery.
	查找select的From子句中中每一个已命名的表。如果一个From子句的一个条目是一个子查询而不是一个表
	或视图,那么创建一个瞬态表结构来描述这个子查询。*/
	for (i = 0, pFrom = pTabList->a; i < pTabList->nSrc; i++, pFrom++){
		Table *pTab;
		if (pFrom->pTab != 0){
			/* This statement has already been prepared.  There is no need
			** to go further. 此声明已做好准备，不需要更进一步。*/
			assert(i == 0);
			return WRC_Prune;
		}
		if (pFrom->zName == 0){
#ifndef SQLITE_OMIT_SUBQUERY
			Select *pSel = pFrom->pSelect;
			/* A sub-query in the FROM clause of a SELECT 一个SELECT的From子句的一个子查询。 */
			assert(pSel != 0);
			assert(pFrom->pTab == 0);
			sqlite3WalkSelect(pWalker, pSel);
			pFrom->pTab = pTab = sqlite3DbMallocZero(db, sizeof(Table));
			if (pTab == 0) return WRC_Abort;
			pTab->nRef = 1;
			pTab->zName = sqlite3MPrintf(db, "sqlite_subquery_%p_", (void*)pTab);
			while (pSel->pPrior){ pSel = pSel->pPrior; }
			selectColumnsFromExprList(pParse, pSel->pEList, &pTab->nCol, &pTab->aCol);
			pTab->iPKey = -1;
			pTab->nRowEst = 1000000;
			pTab->tabFlags |= TF_Ephemeral;
#endif
		}
		else{
			/* An ordinary table or view name in the FROM clause From子句中的一个普通表或视图名称*/
			assert(pFrom->pTab == 0);
			pFrom->pTab = pTab =
				sqlite3LocateTable(pParse, 0, pFrom->zName, pFrom->zDatabase);
			if (pTab == 0) return WRC_Abort;
			pTab->nRef++;
#if !defined(SQLITE_OMIT_VIEW) || !defined (SQLITE_OMIT_VIRTUALTABLE)
			if (pTab->pSelect || IsVirtual(pTab)){
				/* We reach here if the named table is a really a view
				如果达成的已命名的表格是一个真正的视图，我们可以到达这里。*/
				if (sqlite3ViewGetColumnNames(pParse, pTab)) return WRC_Abort;
				assert(pFrom->pSelect == 0);
				pFrom->pSelect = sqlite3SelectDup(db, pTab->pSelect, 0);
				sqlite3WalkSelect(pWalker, pFrom->pSelect);
			}
#endif
		}

		/* Locate the index named by the INDEXED BY clause, if any.
		定位以INDEXED BY clause来命名的索引,*/
		if (sqlite3IndexedByLookup(pParse, pFrom)){
			return WRC_Abort;
		}
	}

	/* Process NATURAL keywords, and ON and USING clauses of joins.
	  自然关键字的进程，以及ON和 USING的子句链接。*/
	if (db->mallocFailed || sqliteProcessJoin(pParse, p)){
		return WRC_Abort;
	}

	/* For every "*" that occurs in the column list, insert the names of
	** all columns in all tables.  And for every TABLE.* insert the names
	** of all columns in TABLE.  The parser inserted a special expression
	** with the TK_ALL operator for each "*" that it found in the column list.
	** The following code just has to locate the TK_ALL expressions and expand
	** each one to the list of all columns in all tables.
	** 对于每个出现在“列”列表中的"*",在所有表格中插入所有列的名称，解析器插入有在“列”
	列表出现的每个"*"的TK_ALL操作的特殊表达式。下面的代码就可以找到tk_all表达式和扩大每一个到所有
	表格的所有列。
	** The first loop just checks to see if there are any "*" operators
	** that need expanding.第一个循环就会进行检查,以查看是否有任何需要扩大的"*"运算符。
	*/
	for (k = 0; k < pEList->nExpr; k++){
		Expr *pE = pEList->a[k].pExpr;
		if (pE->op == TK_ALL) break;
		assert(pE->op != TK_DOT || pE->pRight != 0);
		assert(pE->op != TK_DOT || (pE->pLeft != 0 && pE->pLeft->op == TK_ID));
		if (pE->op == TK_DOT && pE->pRight->op == TK_ALL) break;
	}
	if (k < pEList->nExpr){
		/*
		** If we get here it means the result set contains one or more "*"
		** operators that need to be expanded.  Loop through each expression
		** in the result set and expand them one by one.
		如果我们到了这里就意味着结果集包含一个或多个需要扩大的"*"。循环每个结果集中的表达式并
		一个接一个的扩大它们。*/
		struct ExprList_item *a = pEList->a;
		ExprList *pNew = 0;
		int flags = pParse->db->flags;
		int longNames = (flags & SQLITE_FullColNames) != 0
			&& (flags & SQLITE_ShortColNames) == 0;

		for (k = 0; k < pEList->nExpr; k++){
			Expr *pE = a[k].pExpr;
			assert(pE->op != TK_DOT || pE->pRight != 0);
			if (pE->op != TK_ALL && (pE->op != TK_DOT || pE->pRight->op != TK_ALL)){
				/* This particular expression does not need to be expanded.
				此特定表达式不需要扩大*/
				pNew = sqlite3ExprListAppend(pParse, pNew, a[k].pExpr);
				if (pNew){
					pNew->a[pNew->nExpr - 1].zName = a[k].zName;
					pNew->a[pNew->nExpr - 1].zSpan = a[k].zSpan;
					a[k].zName = 0;
					a[k].zSpan = 0;
				}
				a[k].pExpr = 0;
			}
			else{
				/* This expression is a "*" or a "TABLE.*" and needs to be
				** expanded. 此表达式是一个“*”或一个"table.*",并需要扩大。*/
				int tableSeen = 0;      /* Set to 1 when TABLE matches 当表匹配时，设置为1*/
				char *zTName;            /* text of name of TABLE 文本的表名*/
				if (pE->op == TK_DOT){
					assert(pE->pLeft != 0);
					assert(!ExprHasProperty(pE->pLeft, EP_IntValue));
					zTName = pE->pLeft->u.zToken;
				}
				else{
					zTName = 0;
				}
				for (i = 0, pFrom = pTabList->a; i < pTabList->nSrc; i++, pFrom++){
					Table *pTab = pFrom->pTab;
					char *zTabName = pFrom->zAlias;
					if (zTabName == 0){
						zTabName = pTab->zName;
					}
					if (db->mallocFailed) break;
					if (zTName && sqlite3StrICmp(zTName, zTabName) != 0){
						continue;
					}
					tableSeen = 1;
					for (j = 0; j < pTab->nCol; j++){
						Expr *pExpr, *pRight;
						char *zName = pTab->aCol[j].zName;
						char *zColname;  /* The computed column name 计算列名称*/
						char *zToFree;   /* Malloced string that needs to be freed 访问内存需要
						释放的字符串 */
						Token sColname;  /* Computed column name as a token 计算列名称用作令牌*/

						/* If a column is marked as 'hidden' (currently only possible
						** for virtual tables), do not include it in the expanded
						** result-set list.如果一个列被标记为“隐藏”(目前仅对虚拟表有可能），不要把它包含在
						扩大的结果集列表中。
						*/
						if (IsHiddenColumn(&pTab->aCol[j])){
							assert(IsVirtual(pTab));
							continue;
						}

						if (i>0 && zTName == 0){
							if ((pFrom->jointype & JT_NATURAL) != 0
								&& tableAndColumnIndex(pTabList, i, zName, 0, 0)
								){
								/* In a NATURAL join, omit the join columns from the
								** table to the right of the join 在一个NATURAL链接中，省略从这个表到右链接
								的列链接*/
								continue;
							}
							if (sqlite3IdListIndex(pFrom->pUsing, zName) >= 0){
								/* In a join with a USING clause, omit columns in the
								** using clause from the table on the right.在有同一个Using子句的链接中，
								省略从这个表到右链接Using子句的列链接
								*/
								continue;
							}
						}
						pRight = sqlite3Expr(db, TK_ID, zName);
						zColname = zName;
						zToFree = 0;
						if (longNames || pTabList->nSrc > 1){
							Expr *pLeft;
							pLeft = sqlite3Expr(db, TK_ID, zTabName);
							pExpr = sqlite3PExpr(pParse, TK_DOT, pLeft, pRight, 0);
							if (longNames){
								zColname = sqlite3MPrintf(db, "%s.%s", zTabName, zName);
								zToFree = zColname;
							}
						}
						else{
							pExpr = pRight;
						}
						pNew = sqlite3ExprListAppend(pParse, pNew, pExpr);
						sColname.z = zColname;
						sColname.n = sqlite3Strlen30(zColname);
						sqlite3ExprListSetName(pParse, pNew, &sColname, 0);
						sqlite3DbFree(db, zToFree);
					}
				}
				if (!tableSeen){
					if (zTName){
						sqlite3ErrorMsg(pParse, "no such table: %s", zTName);
					}
					else{
						sqlite3ErrorMsg(pParse, "no tables specified");
					}
				}
			}
		}
		sqlite3ExprListDelete(db, pEList);
		p->pEList = pNew;
	}
#if SQLITE_MAX_COLUMN
	if (p->pEList && p->pEList->nExpr > db->aLimit[SQLITE_LIMIT_COLUMN]){
		sqlite3ErrorMsg(pParse, "too many columns in result set");
	}
#endif
	return WRC_Continue;
}

/*
** No-op routine for the parse-tree walker.
**对于parse-tree walker的无操作例程
** When this routine is the Walker.xExprCallback then expression trees
** are walked without any actions being taken at each node.  Presumably,
** when this routine is used for Walker.xExprCallback then
** Walker.xSelectCallback is set to do something useful for every
** subquery in the parser tree.当这个例程是Walker.xExprCallback ，那么表达树在每个节点上不采取
任何行动都可以。由此可以推断,当此例程被用于Walker.xExprCallback时，Walker.xSelectCallback
被设置对为解析树中的每一个子查询有用。
*/
static int exprWalkNoop(Walker *NotUsed, Expr *NotUsed2){
	UNUSED_PARAMETER2(NotUsed, NotUsed2);
	return WRC_Continue;
}

/*
** This routine "expands" a SELECT statement and all of its subqueries.
** For additional information on what it means to "expand" a SELECT
** statement, see the comment on the selectExpand worker callback above.
**此例程可以扩展一个SELECT语句及其所有子查询。对于意味着“扩大”一个SELECT语句的附加信息,
请参阅上面selectexpand工人回调的评论。
** Expanding a SELECT statement is the first step in processing a
** SELECT statement.  The SELECT statement must be expanded before
** name resolution is performed.扩大一个SELECT语句是处理SELECT语句的第一步。SELECT语句在执行名称
解析之前必须扩大。

** If anything goes wrong, an error message is written into pParse.
** The calling function can detect the problem by looking at pParse->nErr
** and/or pParse->db->mallocFailed.如果出现任何错误,错误消息被写入pparse，
调用函数可以通过看pParse->nErr和/或pParse->db->mallocFailed来检测问题。
*/
static void sqlite3SelectExpand(Parse *pParse, Select *pSelect){
	Walker w;
	w.xSelectCallback = selectExpander;
	w.xExprCallback = exprWalkNoop;
	w.pParse = pParse;
	sqlite3WalkSelect(&w, pSelect);
}

///end update by zhaoyanqi 赵艳琪

#ifndef SQLITE_OMIT_SUBQUERY
/*
** This is a Walker.xSelectCallback callback for the sqlite3SelectTypeInfo()
** interface.
**
** For each FROM-clause subquery, add Column.zType and Column.zColl
** information to the Table structure that represents the result set
** of that subquery.
**
**为from子句的每个子查询，增加Column.zType 和Column.zColl 信息到表结构?
**以代表 子查询的结果集
**
** The Table structure that represents the result set was constructed
** by selectExpander() but the type and collation information was omitted
** at that point because identifiers had not yet been resolved.  This
** routine is called after identifier resolution.
**
**这个表结构代表被selectExpander()创建的结果集、、、
**这个程序被叫做标识符解析后
*/
static int selectAddSubqueryTypeInfo(Walker *pWalker, Select *p){
	Parse *pParse; ? *SQL解析器的上下文* /
		int i;
	SrcList *pTabList;
	struct SrcList_item *pFrom;

	assert(p->selFlags & SF_Resolved);
	if ((p->selFlags & SF_HasTypeInfo) == 0){
		p->selFlags |= SF_HasTypeInfo;
		pParse = pWalker->pParse;
		pTabList = p->pSrc;
		for (i = 0, pFrom = pTabList->a; i < pTabList->nSrc; i++, pFrom++){
			Table *pTab = pFrom->pTab;
			if (ALWAYS(pTab != 0) && (pTab->tabFlags & TF_Ephemeral) != 0){
				/* A sub-query in the FROM clause of a SELECT
				**
				**select中的from 子句的一个子查询
				**
				*/
				Select *pSel = pFrom->pSelect;
				assert(pSel);
				while (pSel->pPrior) pSel = pSel->pPrior;
				selectAddColumnTypeAndCollation(pParse, pTab->nCol, pTab->aCol, pSel);
			}
		}
	}
	return WRC_Continue;
}
#endif


/*
** This routine adds datatype and collating sequence information to
** the Table structures of all FROM-clause subqueries in a
** SELECT statement.
**这个程序将数据类型和排序序列信息
添加到表结构所有from子句的子查询
在一个SELECT语句。
** Use this routine after name resolution.
**在name resolution之后使用这个程序
*/
static void sqlite3SelectAddTypeInfo(Parse *pParse, Select *pSelect){
#ifndef SQLITE_OMIT_SUBQUERY
	Walker w;
	w.xSelectCallback = selectAddSubqueryTypeInfo;/*select回叫*/
	w.xExprCallback = exprWalkNoop;/*表达式回叫*/
	w.pParse = pParse;
	sqlite3WalkSelect(&w, pSelect);
#endif
}


/*
** This routine sets up a SELECT statement for processing.  The
** following is accomplished:
**本程序建立一个SELECT语句的处理
**     *  VDBE Cursor numbers are assigned to all FROM-clause terms.             为from子句分配的vdbe指针数
**     *  Ephemeral Table objects are created for all FROM-clause subqueries    为from子句的子查询建立的临时表对象.
**     *  ON and USING clauses are shifted into WHERE statements
**     *  Wildcards "*" and "TABLE.*" in result sets are expanded.                     通配符"*" 和"TABLE.*" 在结果集中被扩展
**     *  Identifiers in expression are matched to tables.                                   表达式的标识符被匹配到相应的表
**
** This routine acts recursively on all subqueries within the SELECT.
**这个程序递归地运行在select中的子查询中
*/
void sqlite3SelectPrep(
	Parse *pParse,         /* The parser context 解析器的上下文*/
	Select *p,             /* The SELECT statement being coded. 被编码的select表达式*/
	NameContext *pOuterNC  /* Name context for container 为容器命名上下文*/
	){
	sqlite3 *db;
	if (NEVER(p == 0)) return;
	db = pParse->db;
	if (p->selFlags & SF_HasTypeInfo) return;
	sqlite3SelectExpand(pParse, p);
	if (pParse->nErr || db->mallocFailed) return;
	sqlite3ResolveSelectNames(pParse, p, pOuterNC);
	if (pParse->nErr || db->mallocFailed) return;
	sqlite3SelectAddTypeInfo(pParse, p);
}

/*
** Reset the aggregate accumulator.
**
**重置聚合累加器
**
**The aggregate accumulator is a set of memory cells that hold
** intermediate results while calculating an aggregate.  This
** routine generates code that stores NULLs in all of those memory
** cells.
**
**在计算一个聚集函数时，聚合累加器是保持中间结果集的内存单元集
**这个程序生成代码,这些代码将null存储在所有的内存单元中
**
*/
static void resetAccumulator(Parse *pParse, AggInfo *pAggInfo){
	Vdbe *v = pParse->pVdbe;
	int i;
	struct AggInfo_func *pFunc;
	if (pAggInfo->nFunc + pAggInfo->nColumn == 0){
		return;
	}
	for (i = 0; i < pAggInfo->nColumn; i++){
		sqlite3VdbeAddOp2(v, OP_Null, 0, pAggInfo->aCol[i].iMem);
	}
	for (pFunc = pAggInfo->aFunc, i = 0; i < pAggInfo->nFunc; i++, pFunc++){
		sqlite3VdbeAddOp2(v, OP_Null, 0, pFunc->iMem);
		if (pFunc->iDistinct >= 0){
			Expr *pE = pFunc->pExpr;
			assert(!ExprHasProperty(pE, EP_xIsSelect));
			if (pE->x.pList == 0 || pE->x.pList->nExpr != 1){
				sqlite3ErrorMsg(pParse, "DISTINCT aggregates must have exactly one "
					"argument");
				pFunc->iDistinct = -1;
			}
			else{
				KeyInfo *pKeyInfo = keyInfoFromExprList(pParse, pE->x.pList);
				sqlite3VdbeAddOp4(v, OP_OpenEphemeral, pFunc->iDistinct, 0, 0,
					(char*)pKeyInfo, P4_KEYINFO_HANDOFF);
			}
		}
	}
}

/*
** Invoke the OP_AggFinalize opcode for every aggregate function
** in the AggInfo structure.
**
**为在AggInfo 结构中的每个聚合函数调用
**OP_AggFinalize操作码
*/
static void finalizeAggFunctions(Parse *pParse, AggInfo *pAggInfo){
	Vdbe *v = pParse->pVdbe;
	int i;
	struct AggInfo_func *pF;
	for (i = 0, pF = pAggInfo->aFunc; i < pAggInfo->nFunc; i++, pF++){
		ExprList *pList = pF->pExpr->x.pList;
		assert(!ExprHasProperty(pF->pExpr, EP_xIsSelect));
		sqlite3VdbeAddOp4(v, OP_AggFinal, pF->iMem, pList ? pList->nExpr : 0, 0,
			(void*)pF->pFunc, P4_FUNCDEF);
	}
}

/*
** Update the accumulator memory cells for an aggregate based on
** the current cursor position.
**
**为一个基于当前所处的位置上的聚集函数update累加器的内存单元
*/
static void updateAccumulator(Parse *pParse, AggInfo *pAggInfo){
	Vdbe *v = pParse->pVdbe;
	int i;
	int regHit = 0;
	int addrHitTest = 0;
	struct AggInfo_func *pF;
	struct AggInfo_col *pC;/*将语法解析树中的pVdbe赋值给v，并进行一些初始化操作*/

	pAggInfo->directMode = 1;/*数据属性置为1*/
	sqlite3ExprCacheClear(pParse);/*清除解析树*/
	for (i = 0, pF = pAggInfo->aFunc; i < pAggInfo->nFunc; i++, pF++){/*开始进行聚集函数的遍历*/
		int nArg;
		int addrNext = 0;
		int regAgg;
		ExprList *pList = pF->pExpr->x.pList;
		assert(!ExprHasProperty(pF->pExpr, EP_xIsSelect));/*插入一个断点，如果pE包含EP_xIsSelect，那么久抛出错误的信息*/
		if (pList){
			nArg = pList->nExpr;
			regAgg = sqlite3GetTempRange(pParse, nArg);
			sqlite3ExprCodeExprList(pParse, pList, regAgg, 1);/*开始为聚集函数分配寄存器，把表达式中的值寄存在寄存器中*/
		}
		else{
			nArg = 0;
			regAgg = 0;/*将聚集函数的个数和存储聚集函数寄存器进行置0操作*/
		}
		if (pF->iDistinct >= 0){
			addrNext = sqlite3VdbeMakeLabel(v);
			assert(nArg == 1);
			codeDistinct(pParse, pF->iDistinct, addrNext, 1, regAgg);/*如果从源表中取值的个数为0，为VDBE创建一个标签，返回值赋值给下一个运行地址，
			                                                       并且插入断点，如果聚集函数不为1，抛出错误信息*/
		}
		if (pF->pFunc->flags & SQLITE_FUNC_NEEDCOLL){
			CollSeq *pColl = 0;
			struct ExprList_item *pItem;
			int j;
			assert(pList != 0);  /* pList!=0 if pF->pFunc has NEEDCOLL */
			for (j = 0, pItem = pList->a; !pColl && j < nArg; j++, pItem++){
				pColl = sqlite3ExprCollSeq(pParse, pItem->pExpr);/*如果pF->pFunc含NEEDCOLL，则pList不为0,那么遍历聚集函数，返回一个默认的排序序列*/
			}
			if (!pColl){
				pColl = pParse->db->pDfltColl;/*如果排序序列不为0，那么将数据库连接中的默认的排序序列赋值给pColl*/
			}
			if (regHit == 0 && pAggInfo->nAccumulator) regHit = ++pParse->nMem;
			sqlite3VdbeAddOp4(v, OP_CollSeq, regHit, 0, 0, (char *)pColl, P4_COLLSEQ);/*如果regHit为0并且累加器不为0，内存单元个数自加之后再赋值给regHit，
			                                                                           添加一个OP_CollSeq操作，并将它的值作为一个指针*/
		}
		sqlite3VdbeAddOp4(v, OP_AggStep, 0, regAgg, pF->iMem,
			(void*)pF->pFunc, P4_FUNCDEF);
		sqlite3VdbeChangeP5(v, (u8)nArg);
		sqlite3ExprCacheAffinityChange(pParse, regAgg, nArg);
		sqlite3ReleaseTempRange(pParse, regAgg, nArg);
		if (addrNext){
			sqlite3VdbeResolveLabel(v, addrNext);
			sqlite3ExprCacheClear(pParse);/*如果有下一个执行地址，解析addrNext，并作为下一条被插入的地址，并且清楚缓存中的语法解析树*/
		}，
	}

	/* Before populating the accumulator registers, clear the column cache.
	** Otherwise, if any of the required column values are already present
	** in registers, sqlite3ExprCode() may use OP_SCopy to copy the value
	** to pC->iMem. But by the time the value is used, the original register
	** may have been used, invalidating the underlying buffer holding the
	** text or blob value. See ticket [883034dcb5].
	**
	**在获取累加寄存器的内存之前,清空列缓存。
	**否则,如果任何所需的列值已经存在于寄存器,
	**sqlite3ExprCode()可以使用OP_SCopy值复制到pC->iMem。
	**但如果同时这个值也被使用,初始寄存器可能被使用,
	**潜在缓存区中保存的文本和值是无效的。
	**
	** Another solution would be to change the OP_SCopy used to copy cached
	** values to an OP_Copy.
	**
	**另一个解决方案是改变OP_SCopy用来复制缓存值到OP_Copy。
	**
	*/
	if (regHit){
		addrHitTest = sqlite3VdbeAddOp1(v, OP_If, regHit);
	}
	sqlite3ExprCacheClear(pParse);/*将OP_Yield操作交给vdbe，然后返回这个操作的地址，清除缓存中的语法解析树*/
	for (i = 0, pC = pAggInfo->aCol; i < pAggInfo->nAccumulator; i++, pC++){/*遍历累加器*/
		sqlite3ExprCode(pParse, pC->pExpr, pC->iMem);
	}
	pAggInfo->directMode = 0;
	sqlite3ExprCacheClear(pParse);
	if (addrHitTest){/*如果addrHitTest不为空*/
		sqlite3VdbeJumpHere(v, addrHitTest);/*如果addrHitTest不为空，并且如果addrHitTest不为0，那个运行地址就跳到当前地址*/
	}
}

/*
** Add a single OP_Explain instruction to the VDBE to explain a simple
** count(*) query ("SELECT count(*) FROM pTab").
**添加一个单一的OP_Explain 结构到VDBE ，用来解释一个单独的count(*)查询.
*/
#ifndef SQLITE_OMIT_EXPLAIN
static void explainSimpleCount(
	Parse *pParse,                  /* 解析上下文 */
	Table *pTab,                    /* 正在查询的表*/
	Index *pIdx                     /* 用于优化扫描的索引 */
	){
	if (pParse->explain == 2){/*如果语法解析树中explain表达式为2*/
		char *zEqp = sqlite3MPrintf(pParse->db, "SCAN TABLE %s %s%s(~%d rows)",
			pTab->zName,
			pIdx ? "USING COVERING INDEX " : "",
			pIdx ? pIdx->zName : "",
			pTab->nRowEst
			);/*打印输出，并赋值给zEqp（其中%s%s%s为传入的变量）*/
		sqlite3VdbeAddOp4(
			pParse->pVdbe, OP_Explain, pParse->iSelectId, 0, 0, zEqp, P4_DYNAMIC
			);/*添加名为OP_Explain的操作，并将它的值作为一个指针*/
	}
}
#else
# define explainSimpleCount(a,b,c)
#endif

/*
** Generate code for the SELECT statement given in the p argument.
**
**
**
** The results are distributed in various ways depending on the
** contents of the SelectDest structure pointed to by argument pDest
** as follows:
**
**为给出p参数编写SELECT语句。 
**结果分布在不同的SelectDest结构目录指针中，如下：
**     pDest->eDest    Result
	**     ------------    -------------------------------------------
	**     SRT_Output      为结果集中每一行产生一行输出(使用OP_ResultRow操作
	**                     opcode)
	**
	**     SRT_Mem         如果结果只是一个单独列也是可用的。存储第一个结果行的第一列在寄存器pDest->iSDParm
	**                     然后舍弃剩余的查询。为了实现"LIMIT 1".
	**                     
	**                    
	**
	**     SRT_Set         结果必须是一个单独列。存储结果的每一行作为键在表pDest->iSDParm中。
	**                     应用相似性pDest->affSdst在存储结果前。为了实现"IN (SELECT ...)".
	**                   
	**     SRT_Union       存储结果作为一个可被识别的键在临时表 pDest->iSDParm 中。
	**                     
	**
	**     SRT_Except      从临时表pDest->iSDParm删除结果
	**
	**     SRT_Table       存储结果在临时表pDest->iSDParm中。这就像 SRT_EphemTab除了假设这个表已经
	**                     被打开。
	**                    
	**     SRT_EphemTab    创建一个临时表pDest->iSDParm并且在里面存储结果。这个游标是返回之后左边打开。
	**                     这就像SRT_Table除了要使用OP_OpenEphemeral先创建一个表
	**                     
	**     SRT_Coroutine   创建一个协作程序，每次被调用的时候，都返回一行新的结果。这个协作程序的条目指针
	**                     存在寄存器pDest->iSDParm中。
	**                    
	**     SRT_Exists      存储一个1在内存单元pDest->iSDParm 中，如果结果集不为空。
	**                     
	**     SRT_Discard     抛掉结果。它被一个带触发器的SELECT语句使用，触发器是这个函数的附带因素。
	**                     
	** 这段程序返回错误的个数。如果存现任何错误，一个错误信息将会放在pParse->zErrMsg中。                   
	**
	** 这段程序并不释放SELECT结构体。调用的函数需要释放SELECT结构体。
	*/
int sqlite3Select(
	Parse *pParse,         /* The parser context */
	Select *p,             /* The SELECT statement being coded. SELECT语句被编码*/
	SelectDest *pDest      /* What to do with the query results 如何处理查询结果*/
	){
	int i, j;              /* Loop counters 循环计数器*/
	WhereInfo *pWInfo;     /* Return from sqlite3WhereBegin() 从sqlite3WhereBegin()返回*/
	Vdbe *v;               /* The virtual machine under construction 创建中的虚拟机*/
	int isAgg;             /* True for select lists like "count(*)" 选择是否是聚集*/
	ExprList *pEList;      /* List of columns to extract. 提取的列列表*/
	SrcList *pTabList;     /* List of tables to select from */
	Expr *pWhere;          /* The WHERE clause.  May be NULL */
	ExprList *pOrderBy;    /* The ORDER BY clause.  May be NULL */
	ExprList *pGroupBy;    /* The GROUP BY clause.  May be NULL */
	Expr *pHaving;         /* The HAVING clause.  May be NULL */
	int isDistinct;        /* True if the DISTINCT keyword is present */
	int distinct;          /* Table to use for the distinct set */
	int rc = 1;            /* Value to return from this function */
	int addrSortIndex;     /* Address of an OP_OpenEphemeral instruction */
	int addrDistinctIndex; /* Address of an OP_OpenEphemeral instruction */
	AggInfo sAggInfo;      /* Information used by aggregate queries 聚集信息*/
	int iEnd;              /* Address of the end of the query 查询结束地址*/
	sqlite3 *db;           /* The database connection 数据库连接*/

#ifndef SQLITE_OMIT_EXPLAIN
	int iRestoreSelectId = pParse->iSelectId;
	pParse->iSelectId = pParse->iNextSelectId++;/*将语法解析树中查找的ID存储在iRestoreSelectId中，然后再将语法解析树中下一个查找ID存储在iSelectId中*/
#endif

	db = pParse->db;
	if (p == 0 || db->mallocFailed || pParse->nErr){
	return 1;  
	}  /*申明一个数据库连接，如果SELECT为空或分配内存失败或语法解析树中有错误，那么直接返回1*/
   
	if (sqlite3AuthCheck(pParse, SQLITE_SELECT, 0, 0, 0)) return 1;/*授权检查,有错误，也返回1*/
	memset(&sAggInfo, 0, sizeof(sAggInfo));/*将sAggInfo的前sizeof(sAggInfo)个字节用0替换*/

	if (IgnorableOrderby(pDest)){
		assert(pDest->eDest == SRT_Exists || pDest->eDest == SRT_Union ||
			pDest->eDest == SRT_Except || pDest->eDest == SRT_Discard);
		/*如果查询结果中的聚集函数没有oderby函数，那么插入断点，跟据判断处理结果集中处理方式为SRT_Exists或SRT_Union异或SRT_Except或SRT_Discar，
		如果都没有，就抛错*/
		sqlite3ExprListDelete(db, p->pOrderBy);/*删除数据库中的orderby子句表达式*/
		p->pOrderBy = 0;
		p->selFlags &= ~SF_Distinct;
	}
	sqlite3SelectPrep(pParse, p, 0);
	pOrderBy = p->pOrderBy;
	pTabList = p->pSrc;
	pEList = p->pEList;
	if (pParse->nErr || db->mallocFailed){
		goto select_end;/*如果语法解析树失败或出错，就跳转到select_end*/
	}
	isAgg = (p->selFlags & SF_Aggregate) != 0;
	assert(pEList != 0);/*根据selFlags判断是否是聚集函数，如果有isAgg为true，否则为FALSE；然后插入断点，如果表达式列表为空，就抛出错误信息*/

	/* Begin generating code.
	**开始生成代码
	*/
	v = sqlite3GetVdbe(pParse);
	if (v == 0) goto select_end;/*根据语法解析树生成一个虚拟的Database引擎，如果vdbe获取失败，就跳到查询结束*/

	/* If writing to memory or generating a set
	** only a single column may be output.
	**如果写入内存或者生成一个集合，
	**那么仅有一个单独的列可能被输出
	*/
#ifndef SQLITE_OMIT_SUBQUERY
	if (checkForMultiColumnSelectError(pParse, pDest, pEList->nExpr)){
		goto select_end;/*如果检测到多维列查询错误，就跳转到查询结束*/
	}
#endif

	/* Generate code for all sub-queries in the FROM clause
	   *  为from语句中的子查询生成代码
	   */
#if !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW)
	for (i = 0; !p->pPrior && i < pTabList->nSrc; i++){/*遍历FROM子句表达式列表*/

		struct SrcList_item *pItem = &pTabList->a[i];
		SelectDest dest;
		Select *pSub = pItem->pSelect;
		int isAggSub;
		if (pSub == 0) continue;/*如果SELECT结构体pSub为0，跳过此次循环到下次循环*/
		if (pItem->addrFillSub){
			sqlite3VdbeAddOp2(v, OP_Gosub, pItem->regReturn, pItem->addrFillSub);
			continue;
		}

		/* Increment Parse.nHeight by the height of the largest expression
		** tree refered to by this, the parent select. The child select
		** may contain expression trees of at most
		** (SQLITE_MAX_EXPR_DEPTH-Parse.nHeight) height. This is a bit
		** more conservative than necessary, but much easier than enforcing
		** an exact limit.
		**
		**增加最大表达式树的高度Parse.nHeight，父选择。
		**子选择可能包含表达式树最多
		**(SQLITE_MAX_EXPR_DEPTH-Parse.nHeight)高度。
		**这可能保守一些,但比强制
		**执行一个精确的限制更容易些
		*/
		pParse->nHeight += sqlite3SelectExprHeight(p);

		isAggSub = (pSub->selFlags & SF_Aggregate) != 0;/*如果selFlags为SF_Aggregate，将聚集函数信息存入isAggSub*/
		if (flattenSubquery(pParse, p, i, isAgg, isAggSub)){/*销毁子查询*/
			/*这个子查询可以并入其父查询中。*/
			if (isAggSub){/*如果子查询中有聚集查询，也就是信息存在*/
				isAgg = 1;/*信息位置1*/
				p->selFlags |= SF_Aggregate;
			}
			i = -1;/*将i置为-1，下一轮循环i从0开始*/
		}
		else{
			/* Generate a subroutine that will fill an ephemeral table with
			** the content of this subquery.  pItem->addrFillSub will point
			** to the address of the generated subroutine.  pItem->regReturn
			** is a register allocated to hold the subroutine return address
			**生成一个子程序，这个子程序将
			**填补一个临时表与该子查询的内容。
			**pItem - > addrFillSub将指向生成的子程序地址。
			**pItem - > regReturn是一个寄存器分配给子程序返回地址
			**
			*/
			int topAddr;
			int onceAddr = 0;
			int retAddr;
			assert(pItem->addrFillSub == 0);/*插入断点*/
			pItem->regReturn = ++pParse->nMem;/*将分配的内存空间大小加1后赋值给regReturn*/
			topAddr = sqlite3VdbeAddOp2(v, OP_Integer, 0, pItem->regReturn);/*将OP_Integer操作交给vdbe，然后返回这个操作的地址并赋值给topAddr*/
			pItem->addrFillSub = topAddr + 1;/*起始地址加1再赋值给addrFillSub，这也是指向子程序地址的*/
			VdbeNoopComment((v, "materialize %s", pItem->pTab->zName));//输出相关的提示帮助信息
			if (pItem->isCorrelated == 0){
				/* If the subquery is no correlated and if we are not inside of
				** a trigger, then we only need to compute the value of the subquery
				** once.
				**如果子查询没有关联到
				**一个触发器,那么我们只需要计算
				**子查询的值一次。*/
				onceAddr = sqlite3CodeOnce(pParse);/*生成一个一次操作指令并为其分配空间*/
			}
			sqlite3SelectDestInit(&dest, SRT_EphemTab, pItem->iCursor);/*初始化一个SelectDest结构，并且把处理结果集合存储到SRT_EphmTab*/
			explainSetInteger(pItem->iSelectId, (u8)pParse->iNextSelectId);
			sqlite3Select(pParse, pSub, &dest);/*为子查询语句生成代码,/*使用自身函数处理查询*/*/
			pItem->pTab->nRowEst = (unsigned)pSub->nSelectRow;
			if (onceAddr) sqlite3VdbeJumpHere(v, onceAddr);
			retAddr = sqlite3VdbeAddOp1(v, OP_Return, pItem->regReturn);
			VdbeComment((v, "end %s", pItem->pTab->zName));
			sqlite3VdbeChangeP1(v, topAddr, retAddr);
			sqlite3ClearTempRegCache(pParse);/*清除寄存器中语法解析树*/
		}
		if ( /*pParse->nErr ||*/ db->mallocFailed){
			goto select_end;/*如果分配内存出错,跳到select_end（查询结束）*/
		}
		pParse->nHeight -= sqlite3SelectExprHeight(p);/*返回表达式树的最大高度*/
		pTabList = p->pSrc;/*表列表*/
		if (!IgnorableOrderby(pDest)){/*如果处理结果集中不含有Orderby,就将SELECT结构体中Orderby属性赋值给pOrderBy*/
			pOrderBy = p->pOrderBy;
		}
	}
	pEList = p->pEList;/*将结构体表达式列表赋值给pEList*/
#endif
	pWhere = p->pWhere;/*SELECT结构体中WHERE子句赋值给pWhere*/
	pGroupBy = p->pGroupBy;/*SELECT结构体中GROUP BY子句赋值给pGroupBy*/
	pHaving = p->pHaving;/*SELECT结构体中Having子句赋值给pHaving*/
	isDistinct = (p->selFlags & SF_Distinct) != 0;/*如果出现DISTINCT关键字设为true*/

#ifndef SQLITE_OMIT_COMPOUND_SELECT
	/* If there is are a sequence of queries, do the earlier ones first.
	  **如果有一系列的查询,先做前面的。
	  */
	if (p->pPrior){
		if (p->pRightmost == 0){
			Select *pLoop, *pRight = 0;
			int cnt = 0;
			int mxSelect;
			for (pLoop = p; pLoop; pLoop = pLoop->pPrior, cnt++){/*遍历SELECT中优先查找SELECT，找到最优先查找SELECT*/
				pLoop->pRightmost = p;/*将SELECT赋值给右子树*/
				pLoop->pNext = pRight;/*将右子树赋值给下一个节点*/
				pRight = pLoop;/*讲中间子节点赋值给右子树*/
			}
			mxSelect = db->aLimit[SQLITE_LIMIT_COMPOUND_SELECT];/*将符合查询的查询个数值返回给mxSelect*/
			if (mxSelect && cnt > mxSelect){
				sqlite3ErrorMsg(pParse, "too many terms in compound SELECT");
				goto select_end;/*如果mxSelect存在，并且深度大于SELECT个数的话，那么在语法解析树中存储too many...Select,然后跳到查询结束*/
			}
		}
		rc = multiSelect(pParse, p, pDest);
		explainSetInteger(pParse->iSelectId, iRestoreSelectId);
		return rc;
	}
#endif

	/* If there is both a GROUP BY and an ORDER BY clause and they are
	** identical, then disable the ORDER BY clause since the GROUP BY
	** will cause elements to come out in the correct order.  This is
	** an optimization - the correct answer should result regardless.
	** Use the SQLITE_GroupByOrder flag with SQLITE_TESTCTRL_OPTIMIZER
	** to disable this optimization for testing purposes.
	**如果有GROUP BY 和 ORDER BY子句，然后如果它们是一致的，那么先执行GROUP BY然后再执行ORDER BY.
	** 这是一种优化方式，对最后的结果没有任何影响。使用带SQLITE_TESTCTRL_OPTIMIZER的SQLITE_GroupByOrder标记
	** 在日常测试中不断优化。
	*/
	if (sqlite3ExprListCompare(p->pGroupBy, pOrderBy) == 0/*如果两个表达式值相同*/
		&& (db->flags & SQLITE_GroupByOrder) == 0){
		pOrderBy = 0;
	}

	/* If the query is DISTINCT with an ORDER BY but is not an aggregate, and
	** if the select-list is the same as the ORDER BY list, then this query
	** can be rewritten as a GROUP BY. In other words, this:
	**
	**     SELECT DISTINCT xyz FROM ... ORDER BY xyz
	**
	** is transformed to:
	**
	**     SELECT xyz FROM ... GROUP BY xyz
	**
	** The second form is preferred as a single index (or temp-table) may be
	** used for both the ORDER BY and DISTINCT processing. As originally
	** written the query must use a temp-table for at least one of the ORDER
	** BY and DISTINCT, and an index or separate temp-table for the other.
	**
	**如果查询是DISTINCT但不是一个聚合查询,
	**如果选择列表（select-list）与ORDERBY列表是一样的
	**那么，该查询可以重写为一个GROUP BY，
	**也就是说:
	**
	** SELECT DISTINCT xyz FROM ... ORDER BY xyz
	**
	**转换为
	**
	**SELECT xyz FROM ... GROUP BY xyz
	**
	**第二个格式更好，一个单独索引（临时表）可能用来能处理 ORDER BY 和 DISTINCT。最初写查询必须使用临时表在
	** 针对ORDER BY 和 DISTINCT的其中一个，并且一个索引或分开的一个临时表给另外一个。*/
	
	if ((p->selFlags & (SF_Distinct | SF_Aggregate)) == SF_Distinct
		&& sqlite3ExprListCompare(pOrderBy, p->pEList) == 0/*如果SELECT中selFlags为SF_Distinct或SF_Aggregate，并且表达式一直*/
	  ){
		){
		p->selFlags &= ~SF_Distinct;
		p->pGroupBy = sqlite3ExprListDup(db, p->pEList, 0);
		pGroupBy = p->pGroupBy;
		pOrderBy = 0;
	}

	/* If there is an ORDER BY clause, then this sorting
	** index might end up being unused if the data can be
	** extracted in pre-sorted order.  If that is the case, then the
	** OP_OpenEphemeral instruction will be changed to an OP_Noop once
	** we figure out that the sorting index is not needed.  The addrSortIndex
	** variable is used to facilitate that change.
	**如果有ORDER BY子句如果数据被提前排序，排序索引可能不用。如果这种情况OP_OpenEphemeral指令会改变
	**OP_Noop，一旦决定不需要排序索引。addrSortIndex变量用于帮助改变。*/
	
	if (pOrderBy){
		KeyInfo *pKeyInfo;
		pKeyInfo = keyInfoFromExprList(pParse, pOrderBy);
		pOrderBy->iECursor = pParse->nTab++;
		p->addrOpenEphm[2] = addrSortIndex =
			sqlite3VdbeAddOp4(v, OP_OpenEphemeral,
			pOrderBy->iECursor, pOrderBy->nExpr + 2, 0,
			(char*)pKeyInfo, P4_KEYINFO_HANDOFF);  /*这段意思就是，如果存在ORDERBY子句,那么声明一个关键信息结构体，并将表达式pOrderBy放到关键信息结构体中*/
	}
	else{
		addrSortIndex = -1;/*否则那么将排序索引的标志置为-1*/
	}

	/* If the output is destined for a temporary table, open that table.
	  **如果输出被指定到一个临时表时候，那么就要打开这个表
	  */
	if (pDest->eDest == SRT_EphemTab){
		sqlite3VdbeAddOp2(v, OP_OpenEphemeral, pDest->iSDParm, pEList->nExpr);
	}/*如果处理的对象为SRT_EphemTab，那么将OP_OpenEphemeral操作交给vdbe，然后返回这个操作的地址*/

	/* Set the limiter.
	   **设置限制器
	   */
	iEnd = sqlite3VdbeMakeLabel(v);/*生成一个新标签，返回值赋值给iEnd*/
	p->nSelectRow = (double)LARGEST_INT64;
	computeLimitRegisters(pParse, p, iEnd);/*计算iLimit和iOffset字段*/
	if (p->iLimit == 0 && addrSortIndex >= 0){
		sqlite3VdbeGetOp(v, addrSortIndex)->opcode = OP_SorterOpen;/*将操作OP_SorterOpen（打开分拣器），将排序索引交给VDBE*/
		p->selFlags |= SF_UseSorter;
	}

	/* Open a virtual index to use for the distinct set.
	  **为distinct集合打开一个虚拟索引
	  */
	if (p->selFlags & SF_Distinct){/*如果selFlags的值为SF_Distinct*/
		KeyInfo *pKeyInfo;/*声明一个关键信息结构体*/
		distinct = pParse->nTab++;
		pKeyInfo = keyInfoFromExprList(pParse, p->pEList);
		addrDistinctIndex = sqlite3VdbeAddOp4(v, OP_OpenEphemeral, distinct, 0, 0,
			(char*)pKeyInfo, P4_KEYINFO_HANDOFF);
		sqlite3VdbeChangeP5(v, BTREE_UNORDERED);
	}
	else{
		distinct = addrDistinctIndex = -1;
	}

	/* Aggregate and non-aggregate queries are handled differently
	  **聚合和非聚合查询被不同方式处理
	  */
	if (!isAgg && pGroupBy == 0){
		ExprList *pDist = (isDistinct ? p->pEList : 0);/*如果isAgg没有GroupBy，判断isDistinct是否为true，否则将p->pEList赋值给pDist*/

		/* Begin the database scan.
		**开始数据库扫描
		*/
		pWInfo = sqlite3WhereBegin(pParse, pTabList, pWhere, &pOrderBy, pDist, 0, 0);/*生成用于WHERE子句处理的循环语句，并返回结束指针*/
		if (pWInfo == 0) goto select_end;/*如果返回指针为0，跳到查询结束*/
		if (pWInfo->nRowOut < p->nSelectRow) p->nSelectRow = pWInfo->nRowOut;/*输出行数*/

		/* If sorting index that was created by a prior OP_OpenEphemeral
		** instruction ended up not being needed, then change the OP_OpenEphemeral
		** into an OP_Noop.
		**
		**如果排序索引被优先 OP_OpenEphemeral 指令创建
		**创建不需要而被结束，那么OP_OpenEphemeral 变为OP_Noop
		*/
		if (addrSortIndex >= 0 && pOrderBy == 0){
			sqlite3VdbeChangeToNoop(v, addrSortIndex);
			p->addrOpenEphm[2] = -1;
		}

		if (pWInfo->eDistinct){
			VdbeOp *pOp;                /* 不再需要OpenEphemeral（打开临时表）instr*/

			assert(addrDistinctIndex >= 0);
			pOp = sqlite3VdbeGetOp(v, addrDistinctIndex);/*返回addrDistinctIndex 指向的操作码*/

			assert(isDistinct);
			assert(pWInfo->eDistinct == WHERE_DISTINCT_ORDERED
				|| pWInfo->eDistinct == WHERE_DISTINCT_UNIQUE
				);
			distinct = -1;
			if (pWInfo->eDistinct == WHERE_DISTINCT_ORDERED){
				int iJump;
				int iExpr;
				int iFlag = ++pParse->nMem;
				int iBase = pParse->nMem + 1;
				int iBase2 = iBase + pEList->nExpr;
				pParse->nMem += (pEList->nExpr * 2);

				/* Change the OP_OpenEphemeral coded earlier to an OP_Integer. The
				** OP_Integer initializes the "first row" flag.  */
				pOp->opcode = OP_Integer;
				pOp->p1 = 1;
				pOp->p2 = iFlag;

				sqlite3ExprCodeExprList(pParse, pEList, iBase, 1);
				iJump = sqlite3VdbeCurrentAddr(v) + 1 + pEList->nExpr + 1 + 1;
				sqlite3VdbeAddOp2(v, OP_If, iFlag, iJump - 1);
				for (iExpr = 0; iExpr < pEList->nExpr; iExpr++){
					CollSeq *pColl = sqlite3ExprCollSeq(pParse, pEList->a[iExpr].pExpr);
					sqlite3VdbeAddOp3(v, OP_Ne, iBase + iExpr, iJump, iBase2 + iExpr);
					sqlite3VdbeChangeP4(v, -1, (const char *)pColl, P4_COLLSEQ);
					sqlite3VdbeChangeP5(v, SQLITE_NULLEQ);
				}
				sqlite3VdbeAddOp2(v, OP_Goto, 0, pWInfo->iContinue);

				sqlite3VdbeAddOp2(v, OP_Integer, 0, iFlag);
				assert(sqlite3VdbeCurrentAddr(v) == iJump);
				sqlite3VdbeAddOp3(v, OP_Move, iBase, iBase2, pEList->nExpr);
			}
			else{
				pOp->opcode = OP_Noop;
			}
		}

		/* Use the standard inner loop. 使用标准的内部循环*/
		selectInnerLoop(pParse, p, pEList, 0, 0, pOrderBy, distinct, pDest,
			pWInfo->iContinue, pWInfo->iBreak);

		/* End the database scan loop.结束数据库扫描循环。
		*/
		sqlite3WhereEnd(pWInfo);/*生成一个where循环的结束*/
	}
	else{
		/* This is the processing for aggregate queries *//*处理聚合查询*/
		NameContext sNC;    /* Name context for processing aggregate information *//*命名上下文处理聚合信息*/
		int iAMem;          /* First Mem address for storing current GROUP BY *//*第一个内存地址存储GROUP BY*/
		int iBMem;          /* First Mem address for previous GROUP BY *//*存储以前GROUP BY在第一个内存地址*/
		int iUseFlag;       /* Mem address holding flag indicating that at least
							** one row of the input to the aggregator has been
							** processed *//*含有标签内存地址表明至少一个输入行执行聚集*/
		int iAbortFlag;     /* Mem address which causes query abort if positive *//*如果是整数会造成中止查询*/
		int groupBySort;    /* Rows come from source in GROUP BY order *//*来源于GROUP BY命令的结果行*/
		int addrEnd;        /* End of processing for this SELECT *//*处理SELECT的结束标记*/
		int sortPTab = 0;   /* Pseudotable used to decode sorting results *//*Pseudotable用于解码排序的结果*/
		int sortOut = 0;    /* Output register from the sorter *//*从分拣器输出寄存器*/

		/* Remove any and all aliases between the result set and the
		** GROUP BY clause.
		*//*删除结果集与GROUP BY之间所有的关联依赖*/
		if (pGroupBy){/*groupby 子句非空*/
			int k;                        /* Loop counter   循环计数器*/
			struct ExprList_item *pItem;  /* For looping over expression in a list */

			for (k = p->pEList->nExpr, pItem = p->pEList->a; k > 0; k--, pItem++){
				pItem->iAlias = 0;
			}
			for (k = pGroupBy->nExpr, pItem = pGroupBy->a; k > 0; k--, pItem++){
				pItem->iAlias = 0;
			}
			if (p->nSelectRow > (double)100) p->nSelectRow = (double)100;/*如果SELECT结果行大于100，赋值给SELECT的行数为100，限定了大小，超过100即为100*/
		}
		else{
			p->nSelectRow = (double)1;
		}


		/* Create a label to jump to when we want to abort the query 、
		**当我们想取消查询时创建一个标签来跳转
		*/
		addrEnd = sqlite3VdbeMakeLabel(v);

		/* Convert TK_COLUMN nodes into TK_AGG_COLUMN and make entries in
		** sAggInfo for all TK_AGG_FUNCTION nodes in expressions of the
		** SELECT statement.
		*//*将TK_COLUMN节点转化为TK_AGG_COLUMN，并且使所有sAggInfo中的条目转化为SELECT中的表达式中TK_AGG_FUNCTION节点*/
		memset(&sNC, 0, sizeof(sNC));/*将sNC中前sizeof(*sNc)个字节用0替换*/
		sNC.pParse = pParse;
		sNC.pSrcList = pTabList;/*将SELECT的源表集合赋值给命名上下文的源表集合（FROM子句列表）*/
		sNC.pAggInfo = &sAggInfo;
		sAggInfo.nSortingColumn = pGroupBy ? pGroupBy->nExpr + 1 : 0;
		sAggInfo.pGroupBy = pGroupBy;
		sqlite3ExprAnalyzeAggList(&sNC, pEList);/*分析表达式的聚合函数并返回错误数*/
		sqlite3ExprAnalyzeAggList(&sNC, pOrderBy);
		if (pHaving){
			sqlite3ExprAnalyzeAggregates(&sNC, pHaving);/*分析表达式的聚合函数*/
		}
		sAggInfo.nAccumulator = sAggInfo.nColumn;/*将聚合信息的列数赋给其累加器*/
		for (i = 0; i < sAggInfo.nFunc; i++){
			assert(!ExprHasProperty(sAggInfo.aFunc[i].pExpr, EP_xIsSelect));
			sNC.ncFlags |= NC_InAggFunc;
			sqlite3ExprAnalyzeAggList(&sNC, sAggInfo.aFunc[i].pExpr->x.pList);
			sNC.ncFlags &= ~NC_InAggFunc;
		}
		if (db->mallocFailed) goto select_end;

		/* Processing for aggregates with GROUP BY is very different and
		** much more complex than aggregates without a GROUP BY.
		**
		**处理带有GROUP BY的聚集函数不同于不带的，而且更为复杂，
		**而且有很大的不同，带groupby的要复杂得多。
		*/
		if (pGroupBy){
			KeyInfo *pKeyInfo;  /* Keying information for the group by clause 子句groupby 的关键信息*/
			int j1;             /* A-vs-B comparision jump */
			int addrOutputRow;  /* Start of subroutine that outputs a result row 开始一个输出结果行子程序*/
			int regOutputRow;   /* Return address register for output subroutine  为输出子程序返回地址寄存器*/
			int addrSetAbort;   /* Set the abort flag and return  设置终止标志并返回*/
			int addrTopOfLoop;  /* Top of the input loop  输入循环的顶端*/
			int addrSortingIdx; /* The OP_OpenEphemeral for the sorting index */
			int addrReset;      /* Subroutine for resetting the accumulator   重置累加器的子程序*/
			int regReset;       /* Return address register for reset subroutine  为重置子程序返回地址寄存器*/

			/* If there is a GROUP BY clause we might need a sorting index to
			** implement it.  Allocate that sorting index now.  If it turns out
			** that we do not need it after all, the OP_SorterOpen instruction
			** will be converted into a Noop.
			**（本人注：若实现分组，先进行排序）如果有一个GROUP BY子句，我们可能需要一个排序索引去实现它。
		    ** 锁定排序索引。如果返回一个我们已经实现的，这个OP_SorterOpen指令将会转为Noop*/
			*/
			sAggInfo.sortingIdx = pParse->nTab++;
			pKeyInfo = keyInfoFromExprList(pParse, pGroupBy);/*将表达式列表中的pGroupBy提取关键信息给pKeyInfo*/
			addrSortingIdx = sqlite3VdbeAddOp4(v, OP_SorterOpen,
				sAggInfo.sortingIdx, sAggInfo.nSortingColumn,
				0, (char*)pKeyInfo, P4_KEYINFO_HANDOFF);

			/* Initialize memory locations used by GROUP BY aggregate processing
			**初始化被groupby 聚合处理的内存单元
			*/
			iUseFlag = ++pParse->nMem;
			iAbortFlag = ++pParse->nMem;
			regOutputRow = ++pParse->nMem;
			addrOutputRow = sqlite3VdbeMakeLabel(v);
			regReset = ++pParse->nMem;
			addrReset = sqlite3VdbeMakeLabel(v);
			iAMem = pParse->nMem + 1;
			pParse->nMem += pGroupBy->nExpr;
			iBMem = pParse->nMem + 1;
			pParse->nMem += pGroupBy->nExpr;
			sqlite3VdbeAddOp2(v, OP_Integer, 0, iAbortFlag);
			VdbeComment((v, "clear abort flag"));
			sqlite3VdbeAddOp2(v, OP_Integer, 0, iUseFlag);
			VdbeComment((v, "indicate accumulator empty"));
			sqlite3VdbeAddOp3(v, OP_Null, 0, iAMem, iAMem + pGroupBy->nExpr - 1);

			/* Begin a loop that will extract all source rows in GROUP BY order.
			** This might involve two separate loops with an OP_Sort in between, or
			** it might be a single loop that uses an index to extract information
			** in the right order to begin with.
			**启动一个循环，提取GROUP BY命令的所有的原列。
			*/
			sqlite3VdbeAddOp2(v, OP_Gosub, regReset, addrReset);
			pWInfo = sqlite3WhereBegin(pParse, pTabList, pWhere, &pGroupBy, 0, 0, 0);
			if (pWInfo == 0) goto select_end;
			if (pGroupBy == 0){
				/* The optimizer is able to deliver rows in group by order so
				** we do not have to sort.  The OP_OpenEphemeral table will be
				** cancelled later because we still need to use the pKeyInfo
				**
				**优化器能够发送出GROUP BY命令，不用再进行排序。
				**这个临时表OP_OpenEphemeral将会被取消，因为我们使用pKeyInfo
				*/
				pGroupBy = p->pGroupBy;
				groupBySort = 0;
			}
			else{
				/* Rows are coming out in undetermined order.  We have to push
				** each row into a sorting index, terminate the first loop,
				** then loop over the sorting index in order to get the output
				** in sorted order
				**
				**以确定的顺序输出行。我们需要将每一行输入到排序索引，
				**中止第一个循环，然后遍历排序索引为了得出排序的序列
				*/
				int regBase;/*基址寄存器*/
				int regRecord;/*寄存器中记录*/
				int nCol;/*列数*/
				int nGroupBy;/*GROUP BY的个数*/

				explainTempTable(pParse,
					isDistinct && !(p->selFlags&SF_Distinct) ? "DISTINCT" : "GROUP BY");/*执行出错才会使用该函数，输出错误信息到语法解析树中*/


				groupBySort = 1;
				nGroupBy = pGroupBy->nExpr;
				nCol = nGroupBy + 1;
				j = nGroupBy + 1;
				for (i = 0; i < sAggInfo.nColumn; i++){
					if (sAggInfo.aCol[i].iSorterColumn >= j){
						nCol++;
						j++;
					}
				}
				regBase = sqlite3GetTempRange(pParse, nCol);
				sqlite3ExprCacheClear(pParse);
				sqlite3ExprCodeExprList(pParse, pGroupBy, regBase, 0);
				sqlite3VdbeAddOp2(v, OP_Sequence, sAggInfo.sortingIdx, regBase + nGroupBy);
				j = nGroupBy + 1;
				for (i = 0; i < sAggInfo.nColumn; i++){
					struct AggInfo_col *pCol = &sAggInfo.aCol[i];
					if (pCol->iSorterColumn >= j){
						int r1 = j + regBase;
						int r2;

						r2 = sqlite3ExprCodeGetColumn(pParse,
							pCol->pTab, pCol->iColumn, pCol->iTable, r1, 0);
						if (r1 != r2){
							sqlite3VdbeAddOp2(v, OP_SCopy, r2, r1);
						}
						j++;
					}
				}
				regRecord = sqlite3GetTempReg(pParse);
				sqlite3VdbeAddOp3(v, OP_MakeRecord, regBase, nCol, regRecord);
				sqlite3VdbeAddOp2(v, OP_SorterInsert, sAggInfo.sortingIdx, regRecord);
				sqlite3ReleaseTempReg(pParse, regRecord);
				sqlite3ReleaseTempRange(pParse, regBase, nCol);
				sqlite3WhereEnd(pWInfo);
				sAggInfo.sortingIdxPTab = sortPTab = pParse->nTab++;
				sortOut = sqlite3GetTempReg(pParse);
				sqlite3VdbeAddOp3(v, OP_OpenPseudo, sortPTab, sortOut, nCol);
				sqlite3VdbeAddOp2(v, OP_SorterSort, sAggInfo.sortingIdx, addrEnd);
				VdbeComment((v, "GROUP BY sort"));
				sAggInfo.useSortingIdx = 1;
				sqlite3ExprCacheClear(pParse);
			}

			 /* Evaluate the current GROUP BY terms and store in b0, b1, b2...
		  ** (b0 is memory location iBMem+0, b1 is iBMem+1, and so forth)
		  ** Then compare the current GROUP BY terms against the GROUP BY terms
		  ** from the previous row currently stored in a0, a1, a2...
		  *//*计算当前GROUP BY的条款并且存储在b0,b1,b2…（b0的内存地址为iBMem+0，b1的内存地址为iBMem+1…依次类推）
		  **然后将当前GROUP BY的条款与存储在a0,a1,a2的以前的行的the GROUP BY 的条款*/
			addrTopOfLoop = sqlite3VdbeCurrentAddr(v);/*返回下一个被插入的指令的地址*/
			sqlite3ExprCacheClear(pParse);/*清除所有列缓存条目*/
			if (groupBySort){
				sqlite3VdbeAddOp2(v, OP_SorterData, sAggInfo.sortingIdx, sortOut);
			}
			for (j = 0; j < pGroupBy->nExpr; j++){
				if (groupBySort){
					sqlite3VdbeAddOp3(v, OP_Column, sortPTab, j, iBMem + j);
					if (j == 0) sqlite3VdbeChangeP5(v, OPFLAG_CLEARCACHE);
				}
				else{
					sAggInfo.directMode = 1;
					sqlite3ExprCode(pParse, pGroupBy->a[j].pExpr, iBMem + j);
				}
			}
			sqlite3VdbeAddOp4(v, OP_Compare, iAMem, iBMem, pGroupBy->nExpr,
				(char*)pKeyInfo, P4_KEYINFO);
			j1 = sqlite3VdbeCurrentAddr(v);
			sqlite3VdbeAddOp3(v, OP_Jump, j1 + 1, 0, j1 + 1);

			
			
		  /* Generate a subroutine that outputs a single row of the result
		  ** set.  This subroutine first looks at the iUseFlag.  If iUseFlag
		  ** is less than or equal to zero, the subroutine is a no-op.  If
		  ** the processing calls for the query to abort, this subroutine
		  ** increments the iAbortFlag memory location before returning in
		  ** order to signal the caller to abort.
		  ** 编一个子程序，用来重置group-by累加器
		  ** 如果这个处理单元是针对中止查询，这个子程序子在返回之前，增大iAbortFlag内存为了中止信号调用者。*/
			
			sqlite3ExprCodeMove(pParse, iBMem, iAMem, pGroupBy->nExpr);
			sqlite3VdbeAddOp2(v, OP_Gosub, regOutputRow, addrOutputRow);
			VdbeComment((v, "output one row"));
			sqlite3VdbeAddOp2(v, OP_IfPos, iAbortFlag, addrEnd);
			VdbeComment((v, "check abort flag"));
			sqlite3VdbeAddOp2(v, OP_Gosub, regReset, addrReset);
			VdbeComment((v, "reset accumulator"));

			/* Update the aggregate accumulators based on the content of
			** the current row
			**
			**基于当前内容的当前行，更新聚合累加器
			*/
			sqlite3VdbeJumpHere(v, j1);
			updateAccumulator(pParse, &sAggInfo);
			sqlite3VdbeAddOp2(v, OP_Integer, 1, iUseFlag);
			VdbeComment((v, "indicate data in accumulator"));

			/* End of the loop   循环结尾
			*/
			if (groupBySort){
				sqlite3VdbeAddOp2(v, OP_SorterNext, sAggInfo.sortingIdx, addrTopOfLoop);
			}
			else{
				sqlite3WhereEnd(pWInfo);
				sqlite3VdbeChangeToNoop(v, addrSortingIdx);
			}

			/* Output the final row of result   输出结果的最后一行
			*/
			sqlite3VdbeAddOp2(v, OP_Gosub, regOutputRow, addrOutputRow);
			VdbeComment((v, "output final row"));

			/* Jump over the subroutines   跳过子程序
			*/
			sqlite3VdbeAddOp2(v, OP_Goto, 0, addrEnd);

			/* Generate a subroutine that outputs a single row of the result
			** set.  This subroutine first looks at the iUseFlag.  If iUseFlag
			** is less than or equal to zero, the subroutine is a no-op.  If
			** the processing calls for the query to abort, this subroutine
			** increments the iAbortFlag memory location before returning in
			** order to signal the caller to abort.
			**
			**生成一个输出结果集的单一行的子程序
			**这个子程序首先检查iUseFlag
			**如果iUseFlag比0 小或者等于0 ，那么子程序不
			**做任何操作。如果对查询的处理调用终止，
			**那么该子程序在返回之前增加iAbortFlag 的内存单元
			**这样做是为了告诉调用者终止调用
			*/
			addrSetAbort = sqlite3VdbeCurrentAddr(v);
			sqlite3VdbeAddOp2(v, OP_Integer, 1, iAbortFlag);
			VdbeComment((v, "set abort flag"));
			sqlite3VdbeAddOp1(v, OP_Return, regOutputRow);
			sqlite3VdbeResolveLabel(v, addrOutputRow);
			addrOutputRow = sqlite3VdbeCurrentAddr(v);
			sqlite3VdbeAddOp2(v, OP_IfPos, iUseFlag, addrOutputRow + 2);
			VdbeComment((v, "Groupby result generator entry point"));
			sqlite3VdbeAddOp1(v, OP_Return, regOutputRow);
			finalizeAggFunctions(pParse, &sAggInfo);
			sqlite3ExprIfFalse(pParse, pHaving, addrOutputRow + 1, SQLITE_JUMPIFNULL);
			selectInnerLoop(pParse, p, p->pEList, 0, 0, pOrderBy,
				distinct, pDest,
				addrOutputRow + 1, addrSetAbort);
			sqlite3VdbeAddOp1(v, OP_Return, regOutputRow);
			VdbeComment((v, "end groupby result generator"));

			/* Generate a subroutine that will reset the group-by accumulator
			**生成一个子程序，这个子程序将重置groupby 累加器
			*/
			sqlite3VdbeResolveLabel(v, addrReset);
			resetAccumulator(pParse, &sAggInfo);
			sqlite3VdbeAddOp1(v, OP_Return, regReset);

		} /* endif pGroupBy.  Begin aggregate queries without GROUP BY:
											   **开始一个无groupby的聚合查询
											   */
		else {
			ExprList *pDel = 0;/*声明一个表达式列表*/
#ifndef SQLITE_OMIT_BTREECOUNT
			Table *pTab;
			if ((pTab = isSimpleCount(p, &sAggInfo)) != 0){
				/* If isSimpleCount() returns a pointer to a Table structure, then
				** the SQL statement is of the form:
				**
				**   SELECT count(*) FROM <tbl>
				**
				** where the Table structure returned represents table <tbl>.
				**
				** This statement is so common that it is optimized specially. The
				** OP_Count instruction is executed either on the intkey table that
				** contains the data for table <tbl> or on one of its indexes. It
				** is better to execute the op on an index, as indexes are almost
				** always spread across less pages than their corresponding tables.
				**
				/* 如果isSimpleCount() 返回一个指向TABLE结构的指针，然后SQL语句格式如
			   **	SELECT count(*) FROM <tbl>
			   ** 
			   ** Table结构返回的table<tbl>如下：
			   ** 这个语句很常见，故进行特俗优化了。这个OP_Count指令执行在intkey表或包含数据的table<tbl>或它的索引。
			   ** 它比较好的执行op或索引，当索引频繁传递比与表一致（这个索引对应的表）的少的页。
			   */
				const int iDb = sqlite3SchemaToIndex(pParse->db, pTab->pSchema);/*为表模式建立索引，并赋值给iDb*/
				const int iCsr = pParse->nTab++;     /*  扫描b-tree 的指针*/
				Index *pIdx;                         /* Iterator variable 迭代变量*/
				KeyInfo *pKeyInfo = 0;               /* Keyinfo for scanned index 已扫描索引的关键信息*/
				Index *pBest = 0;                    /* Best index found so far 到目前为止找到的最好的索引*/
				int iRoot = pTab->tnum;              /* Root page of scanned b-tree 扫描表b-tree 的根页*/

				sqlite3CodeVerifySchema(pParse, iDb);
				sqlite3TableLock(pParse, iDb, pTab->tnum, 0, pTab->zName);

				/* Search for the index that has the least amount of columns. If
				** there is such an index, and it has less columns than the table
				** does, then we can assume that it consumes less space on disk and
				** will therefore be cheaper to scan to determine the query result.
				** In this case set iRoot to the root page number of the index b-tree
				** and pKeyInfo to the KeyInfo structure required to navigate the
				** index.
				**
				**查找最少出现的列的索引，如果这里有个索引，它有比较少的列，然后我们假设他消耗了较小的空间在硬盘上并且
			    ** 扫描已经确定查询结果开销比较低。在这种情况下，设置iRoot到b-tree索引的根页号并且pKeyInfo是KeyInfo结构体需要的导航索引*/
			
				for (pIdx = pTab->pIndex; pIdx; pIdx = pIdx->pNext){/*遍历索引*/
					if (pIdx->bUnordered == 0 && (!pBest || pIdx->nColumn < pBest->nColumn)){/*如果索引没有排序并且没有最好的索引或者所有中的列小于最好索引的中的列*/
						pBest = pIdx;
					}
				}
				if (pBest && pBest->nColumn < pTab->nCol){
					iRoot = pBest->tnum;/*将最好索引中根索引赋值给iRoot*/
					pKeyInfo = sqlite3IndexKeyinfo(pParse, pBest);/*将最好索引的信息提取到关键信息结构体pKeyInfo中*/
				}

				/* Open a read-only cursor, execute the OP_Count, close the cursor.
					 **
					 **  打开只读游标，执行OP_Count操作，关闭游标
					 */
				sqlite3VdbeAddOp3(v, OP_OpenRead, iCsr, iRoot, iDb);
				if (pKeyInfo){
					sqlite3VdbeChangeP4(v, -1, (char *)pKeyInfo, P4_KEYINFO_HANDOFF);
				}
				sqlite3VdbeAddOp2(v, OP_Count, iCsr, sAggInfo.aFunc[0].iMem);
				sqlite3VdbeAddOp1(v, OP_Close, iCsr);
				explainSimpleCount(pParse, pTab, pBest);
			}
			else
#endif /* SQLITE_OMIT_BTREECOUNT */
		  {
			/* Check if the query is of one of the following forms:
			**
			**   SELECT min(x) FROM ...
			**   SELECT max(x) FROM ...
			**
			** If it is, then ask the code in where.c to attempt to sort results
			** as if there was an "ORDER ON x" or "ORDER ON x DESC" clause. 
			** If where.c is able to produce results sorted in this order, then
			** add vdbe code to break out of the processing loop after the 
			** first iteration (since the first iteration of the loop is 
			** guaranteed to operate on the row with the minimum or maximum 
			** value of x, the only row required).
			**
			** A special flag must be passed to sqlite3WhereBegin() to slightly
			** modify behaviour as follows:
			**
			**   + If the query is a "SELECT min(x)", then the loop coded by
			**     where.c should not iterate over any values with a NULL value
			**     for x.
			**
			**   + The optimizer code in where.c (the thing that decides which
			**     index or indices to use) should place a different priority on 
			**     satisfying the 'ORDER BY' clause than it does in other cases.
			**     Refer to code and comments in where.c for details.
			*/
			/*如果查询是以下的格式，就进行检测：
			**   SELECT min(x) FROM ...
			**   SELECT max(x) FROM ...
			**如果是，然后让where.c尝试排序结果，如果这是一个"ORDER ON x" 或 "ORDER ON x DESC" 子句。
			**如果where.c能产生排序结果，然后在第一个迭代器（第一个迭代循环保证执行x的最小或最大值，只有一行）之后添加VDBE代码中断执行循环
			**
			**应该传给sqlite3WhereBegin一个特殊的标记稍微修改一下的行为：
			**   如果这个查询是"SELECT min(x)"，然后where.c中循环代码不能迭代任何x列的空值。
			**   where.c优化器代码（决定使用使用那些索引）应该优先'ORDER BY'子句，更多的代码和注释细节在where.c中。
			*/
				ExprList *pMinMax = 0;/*声明一个表达式列表，存放最小或最大值的表达式*/
				u8 flag = minMaxQuery(p);
				if (flag){
					assert(!ExprHasProperty(p->pEList->a[0].pExpr, EP_xIsSelect));
					pMinMax = sqlite3ExprListDup(db, p->pEList->a[0].pExpr->x.pList, 0);/*插入断点，如果p->pEList->a[0].pExpr中包含EP_xIsSelect属性不为空，
					                                                                      抛出错误信息*/
					pDel = pMinMax;
					if (pMinMax && !db->mallocFailed){
						pMinMax->a[0].sortOrder = flag != WHERE_ORDERBY_MIN ? 1 : 0;
						pMinMax->a[0].pExpr->op = TK_COLUMN;
					}
				}

				/* This case runs if the aggregate has no GROUP BY clause.  The
				** processing is much simpler since there is only a single row
				** of output.
				**
				**处理聚集函数中没有 GROUP BY情况。这个处理程序很简单
				**只有一个单独的行输出。
				*/
				resetAccumulator(pParse, &sAggInfo);/*重置聚合累加器*/
				pWInfo = sqlite3WhereBegin(pParse, pTabList, pWhere, &pMinMax, 0, flag, 0);/*生成处理where子句的循环的开始*/
				if (pWInfo == 0){/*若为空，则删除并结束select*/
					sqlite3ExprListDelete(db, pDel);
					goto select_end;
				}
				updateAccumulator(pParse, &sAggInfo);/*更新累加器内存单元*/
				if (!pMinMax && flag){
					sqlite3VdbeAddOp2(v, OP_Goto, 0, pWInfo->iBreak);
					VdbeComment((v, "%s() by index",
						(flag == WHERE_ORDERBY_MIN ? "min" : "max")));
				}
				sqlite3WhereEnd(pWInfo);/*结束where 循环*/
				finalizeAggFunctions(pParse, &sAggInfo);/*结束聚合函数*/
			}

			pOrderBy = 0;
			sqlite3ExprIfFalse(pParse, pHaving, addrEnd, SQLITE_JUMPIFNULL);
			selectInnerLoop(pParse, p, p->pEList, 0, 0, 0, -1,
				pDest, addrEnd, addrEnd);
			sqlite3ExprListDelete(db, pDel);
		}
		sqlite3VdbeResolveLabel(v, addrEnd);

	} /* endif aggregate query *//*如果是聚集查询*/

	if (distinct >= 0){
		explainTempTable(pParse, "DISTINCT");/*取消重复表达式的值大于等于0，执行出错才会使用该函数，输出错误信息"DISTINCT"到语法解析树*/
	}

	/* If there is an ORDER BY clause, then we need to sort the results
	** and send them to the callback one by one.
	**
	**如果这是一个ORDERBY子句，
	**我们需要排序结果并且发送结果一个接一个的给回调函数
	*/
	if (pOrderBy){
		explainTempTable(pParse, "ORDER BY");
		generateSortTail(pParse, p, v, pEList->nExpr, pDest);/*调用自身函数，输出ORDER BY结果*/
	}

	/* Jump here to skip this query
	  **为了跳过查询，直接跳转到这儿
	  */
	sqlite3VdbeResolveLabel(v, iEnd);/*解析iEnd，并作为下一条被插入的地址*/

	/* The SELECT was successfully coded.   Set the return code to 0
	** to indicate no errors.
	**
	**  select 语句被成功编码，设置如果返还为0，表明没有出错
	*/
	rc = 0;/*将执行结果标记为0*/
	/* Control jumps to here if an error is encountered above, or upon
	** successful coding of the SELECT.
	**控制跳跃到这里如果上面遇
	**  到一个错误,或对select成功编码。
	*/
select_end:
	explainSetInteger(pParse->iSelectId, iRestoreSelectId);

	/* Identify column names if results of the SELECT are to be output.
	**如果select 结果集要被输出，则标出列名
	*/
	if (rc == SQLITE_OK && pDest->eDest == SRT_Output){
		generateColumnNames(pParse, pTabList, pEList);/*生成列名*/
	}

	sqlite3DbFree(db, sAggInfo.aCol);
	sqlite3DbFree(db, sAggInfo.aFunc);
	return rc;
}

#if defined(SQLITE_ENABLE_TREE_EXPLAIN)
/*
** Generate a human-readable description of a the Select object.
**生成一个可读的select 对象的描述
*/
static void explainOneSelect(Vdbe *pVdbe, Select *p){
	sqlite3ExplainPrintf(pVdbe, "SELECT ");/*实际上调用sqlite3VXPrintf（），进行格式化输出"SELECT "*/
	if (p->selFlags & (SF_Distinct | SF_Aggregate)){/*有Distinct 或者Aggregate*/
		if (p->selFlags & SF_Distinct){/*有Distinct */
			sqlite3ExplainPrintf(pVdbe, "DISTINCT ");
		}
		if (p->selFlags & SF_Aggregate){/*有Aggregate */
			sqlite3ExplainPrintf(pVdbe, "agg_flag ");
		}
		sqlite3ExplainNL(pVdbe);/*附加上'|n' */
		sqlite3ExplainPrintf(pVdbe, "   ");
	}
	sqlite3ExplainExprList(pVdbe, p->pEList);/*为表达式列表p->pEList生成一个易读的描述信息*/
	sqlite3ExplainNL(pVdbe);/*添加一个换行符（'\n',前提是如果结尾没有）*/
	  if( p->pSrc && p->pSrc->nSrc ){/*遍历FROM子句表达式列表*/
		int i;
		sqlite3ExplainPrintf(pVdbe, "FROM ");/*实际上调用sqlite3VXPrintf（），进行格式化输出"FROM "*/
		sqlite3ExplainPush(pVdbe);/*在pVdbe中推出一个新的缩进级别，从光标开始的位置进行，后续的行都缩进*/
		for(i=0; i<p->pSrc->nSrc; i++){/*遍历FROM子句表达式列表**/
		  struct SrcList_item *pItem = &p->pSrc->a[i];/*声明一个FROM子句列表项结构体*/
		  sqlite3ExplainPrintf(pVdbe, "{%d,*} = ", pItem->iCursor);/*实际上调用sqlite3VXPrintf（），进行格式化输出 "{%d,*} = "*/
		  if( pItem->pSelect ){/*如果表达式列表中含有SELECT*/
			sqlite3ExplainSelect(pVdbe, pItem->pSelect);/*解析SELECT结构体的pSelect*/
			if( pItem->pTab ){/*如果表达式列表中含有pTab*/
			  sqlite3ExplainPrintf(pVdbe, " (tabname=%s)", pItem->pTab->zName);/*实际上调用sqlite3VXPrintf（），进行格式化输出" (tabname=%s)"*/
			}
		  }else if( pItem->zName ){/*如果表达式列表中表达式项名存在*/
			sqlite3ExplainPrintf(pVdbe, "%s", pItem->zName);/*实际上调用sqlite3VXPrintf（），进行格式化输出pItem->zName*/
		  }
		  if( pItem->zAlias ){/*如果表达式列表中有关联依赖*/
			sqlite3ExplainPrintf(pVdbe, " (AS %s)", pItem->zAlias);/*实际上调用sqlite3VXPrintf（），进行格式化输出" (AS %s)"*/
		  }
		  if( pItem->jointype & JT_LEFT ){/*如果表达式列表中连接类型为JT_LEFT*/
			sqlite3ExplainPrintf(pVdbe, " LEFT-JOIN");/*实际上调用sqlite3VXPrintf（），进行格式化输出" LEFT-JOIN"*/
		  }
		  sqlite3ExplainNL(pVdbe);/*添加一个换行符（'\n',前提是如果结尾没有）*/
		}
		sqlite3ExplainPop(pVdbe);/*弹出刚才压进栈的缩进级别*/
	  }
	  if( p->pWhere ){/*如果SELECT结构体p中含不为空where子句*/
		sqlite3ExplainPrintf(pVdbe, "WHERE ");/*实际上调用sqlite3VXPrintf（），进行格式化输出"WHERE "*/
		sqlite3ExplainExpr(pVdbe, p->pWhere);/*为表达式树pWhere生成一个易读说明*/
		sqlite3ExplainNL(pVdbe);/*添加一个换行符（'\n',前提是如果结尾没有）*/
	  }
	  if( p->pGroupBy ){/*如果SELECT结构体p中含不为空where子句*/
		sqlite3ExplainPrintf(pVdbe, "GROUPBY ");/*实际上调用sqlite3VXPrintf（），进行格式化输出"GROUPBY "*/
		sqlite3ExplainExprList(pVdbe, p->pGroupBy);/*为表达式列表p->pGroupBy生成一个易读的描述信息*/
		sqlite3ExplainNL(pVdbe);/*添加一个换行符（'\n',前提是如果结尾没有）*/
	  }
	  if( p->pHaving ){/*如果SELECT结构体p中含不为空having子句*/
		sqlite3ExplainPrintf(pVdbe, "HAVING ");/*实际上调用sqlite3VXPrintf（），进行格式化输出 "HAVING "*/
		sqlite3ExplainExpr(pVdbe, p->pHaving);/*为表达式树pHaving生成一个易读说明*/
		sqlite3ExplainNL(pVdbe);/*添加一个换行符（'\n',前提是如果结尾没有）*/
	  }
	  if( p->pOrderBy ){/*如果SELECT结构体p中含不为空order by子句*/
		sqlite3ExplainPrintf(pVdbe, "ORDERBY ");/*实际上调用sqlite3VXPrintf（），进行格式化输出"ORDERBY "*/
		sqlite3ExplainExprList(pVdbe, p->pOrderBy);/*为表达式列表p->pOrderBy生成一个易读的描述信息*/
		sqlite3ExplainNL(pVdbe);/*添加一个换行符（'\n',前提是如果结尾没有）*/
	  }
	  if( p->pLimit ){/*如果SELECT结构体p中含不为空limit子句*/
		sqlite3ExplainPrintf(pVdbe, "LIMIT ");/*实际上调用sqlite3VXPrintf（），进行格式化输出"LIMIT "*/
		sqlite3ExplainExpr(pVdbe, p->pLimit);/*为表达式树p->pLimit生成一个易读说明*/
		sqlite3ExplainNL(pVdbe);/*添加一个换行符（'\n',前提是如果结尾没有）*/
	  }
	  if( p->pOffset ){/*如果SELECT结构体p中含不为空offset子句*/
		sqlite3ExplainPrintf(pVdbe, "OFFSET ");/*实际上调用sqlite3VXPrintf（），进行格式化输出"OFFSET "*/
		sqlite3ExplainExpr(pVdbe, p->pOffset);/*为表达式树p->pOffset生成一个易读说明*/
		sqlite3ExplainNL(pVdbe);/*添加一个换行符（'\n',前提是如果结尾没有）*/
	  }
	}
	void sqlite3ExplainSelect(Vdbe *pVdbe, Select *p){
	  if( p==0 ){/*如果SELECT结构体p为空*/
		sqlite3ExplainPrintf(pVdbe, "(null-select)");/*实际上调用sqlite3VXPrintf（），进行格式化输出"(null-select)"*/
		return;
	  }
	  while( p->pPrior ) p = p->pPrior;/*递归，找到最优先查询的SELECT*/
	  sqlite3ExplainPush(pVdbe);/*在pVdbe中推出一个新的缩进级别，从光标开始的位置进行，后续的行都缩进*/
	  while( p ){
		explainOneSelect(pVdbe, p);/*生成一个易读描述SELECET的对象*/
		p = p->pNext;/*将p的子树，赋值，给当前p*/
		if( p==0 ) break;/*已经循环到最后一个了，没有了子节点*/
		sqlite3ExplainNL(pVdbe);/*添加一个换行符（'\n',前提是如果结尾没有）*/
		sqlite3ExplainPrintf(pVdbe, "%s\n", selectOpName(p->op));/*实际上是调用sqlite3VXPrintf（），并进行格式化输出为"%s\n"*/
	  }
	  sqlite3ExplainPrintf(pVdbe, "END");/*实际上是调用sqlite3VXPrintf（），进行格式化大的输出为"END"*/
	  sqlite3ExplainPop(pVdbe);/*对刚才压进栈的缩进级别进行弹出*/
	}
	/* End of the structure debug printing code 结束调试
	*****************************************************************************/
	#endif /* defined(SQLITE_ENABLE_TREE_EXPLAIN) */
