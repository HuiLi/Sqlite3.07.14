﻿/*
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
*/

/*********************************************************************
2001年9月15日
**作者放弃版权这个源代码。在的地方一个合法的通知,这是一个祝福:
**愿你做好事,而不是邪恶的。
**愿你找到原谅自己,原谅他人。
**可能你分享自由,永远不会超过你给。
**
*************************************************************************
**此文件包含由语法分析器来处理在 SQLite 的 SELECT 语句调用的 C 代码例程
***********************************************************************/

/*
** This file contains C code routines that are called by the parser
** to handle SELECT statements in SQLite.
**本文件包含SQLite中利用语法分析器处理SLEECT语句的C代码程序。SQLite的语法分析器使用Lemon LALR(1)分析程序生成器来产生，Lemon做的工作与YACC/BISON相同，但它使用不同的输入句法，这种句法更不易出错。Lemon还产生可重入的并且线程安全的语法分析器。Lemon定义了非终结析构器的概念，当遇到语法错误时它不会泄露内存。驱动Lemon的源文件可在parse.y中找到。
  因为lemon是一个在开发机器上不常见的程序，所以lemon的源代码（只是一个C文件）被放在SQLite的"tool"子目录下。 lemon的文档放在"doc"子目录下
*/
#include "sqliteInt.h" 

/*
** Trace output macros
** 跟踪输出宏
*/

#if SELECTTRACE_ENABLED //预编译指令                                                            /*预编译处理器把预编译指令包含进来*/                                                          
/***/ int sqlite3SelectTrace = 0;                                                               /*定义整型且赋值为0*/
# define SELECTTRACE(K,P,S,X)                                                                   /*定义宏命令*/  
  if(sqlite3SelectTrace&(K))                                                                    /*判断选择跟踪栈*/   
    sqlite3DebugPrintf("%*s%s.%p: ",(P)->nSelectIndent*2-2,"",                        
        (S)->zSelName,(S)),                                                                     /*给出输出值*/
    sqlite3DebugPrintf X                                                                        /*定义常量X*/
#else
# define SELECTTRACE(K,P,S,X)                                                                   /*定义宏选择有四个参数*/
#endif

/*
** An instance of the following object is used to record information about
** how to process the DISTINCT keyword, to simplify passing that information
** into the selectInnerLoop() routine.
** 下面结构体的一个实例是用于记录有关如何处理DISTINCT关键字的信息，为了简化传递该信息到selectInnerLoop（）事务。
*/

typedef struct DistinctCtx DistinctCtx;                                  /*定义结构体类型*/
struct DistinctCtx {                                                     /*定义结构体名称*/
	u8 isTnct;      /* 如果DISTINCT关键字存在则真  */                /*定义结构体关键字*/
		u8 eTnctType;   /* 其中的WHERE_DISTINCT_*运算符*/        /*定义结构体中要使用的整型运算符*/
		int tabTnct;    /* 处理DISTINCT的临时表*/                /*定义处理临时表的整型*/
		int addrTnct;   /* OP_OpenEphemeral操作码的地址*/ 
};

/*
** An instance of the following object is used to record information about
** the ORDER BY (or GROUP BY) clause of query is being coded.
** 下面结构体的一个实例是用于记录ORDER BY(或者 GROUP BY)查询字句的信息
*/

typedef struct SortCtx SortCtx;                                                        /*定义结构体用来记录order by 查询子句的信息*/                                                       
struct SortCtx {                                                                       /*定义结构体名称类型*/
	ExprList *pOrderBy;   /* ORDER BY(或者 GROUP BY字句)*/                         /*定义排序中用到的查询列表*/
		int nOBSat;           /* ORDER BY语句的数量满足指数*/                  /*定义查询的条件与要求*/
		int iECursor;         /* 分类器的游标数目*/                            /*定义结构体分类器的游标数目*/
		int regReturn;        /* 寄存器控制块输出返回地址*/                    /*定义结构体寄存器控制块输出的返回地址*/
		int labelBkOut;       /* 块输出子程序的启动标签*/                      /*定义结构体中块输出子程序的启动标签*/  
		int addrSortIndex;    /* OP_SorterOpen或者OP_OpenEphemeral的地址 */    /*定义结构体中查询时要用到的地址*/
		u8 sortFlags;         /* 零或者更多的SORTFLAG_* 位 */                  /*定义结构体中更多需要查找到的位*/
};
#define SORTFLAG_UseSorter  0x01   /* 使用sorteropen代替openephemeral */               /*定义结构体用户编号*/
/*
** Delete all the content of a Select structure.  Deallocate the structure
** itself only if bFree is true.
**删除查询结构的内容而不释放结构体本身。为了清除表达式
*/
static void clearSelect(sqlite3 *db, Select *p, int bFree){  /*函数的用处是用于清除*//*清除查询结构*/                                             
	while (p){
		Select *pPrior = p->pPrior;                  /*将p->pPrior赋值给Select *pPrior*/                                                  
			sqlite3ExprListDelete(db, p->pEList);        /*清除select结构体中的查询结果*/ /*删除整个表达式列表*/                      
			sqlite3SrcListDelete(db, p->pSrc);           /*从表达式列表中清除FROM子句表达式*//*删除表达式列表中的FROM子句*/            
			sqlite3ExprDelete(db, p->pWhere);           /*从表达式列表中清除where子句表达式*//*递归删除where子句*/                     
			sqlite3ExprListDelete(db, p->pGroupBy);      /*从表达式列表中清除group by子句表达式*/ /*删除groupby子句*/                  
			sqlite3ExprDelete(db, p->pHaving);            /*从表达式列表中清除Having子句表达式*//*递归删除having子句*/                 
			sqlite3ExprListDelete(db, p->pOrderBy);    /*从表达式列表中清除Order by子句表达式*/ /*删除orderby*/                       
			sqlite3ExprDelete(db, p->pLimit);           /*从表达式列表中清除Limit子句表达式*//*删除优先选择子句*/                     
			sqlite3ExprDelete(db, p->pOffset);          /*从表达式列表中清除偏移量Offset子句表达式*//*递归删除限制返回数据数量的子句*/ 
			sqlite3WithDelete(db, p->pWith);			 /*递归删除一个条件树*//*递归删除偏移量offset子句*/               

			if (bFree)                                     /*如果树不为空*/                                                           
				sqlite3DbFree(db, p);					 /*释放*db*/                                                
			p = pPrior;                                      /*pPrior赋值给p*/                                                 
			bFree = 1;               /*树不为空*/                                                                                    
	}
}

/*
** Initialize a SelectDest structure.
**初始化一个SelectDest结构.为了创建一个SelectDest,传入参数，定制一个结构体
*/
void sqlite3SelectDestInit(SelectDest *pDest, int eDest, int iParm){/*函数sqlite3SelectDestInit的参数列表为 结构体SelectDest指针pDest，整型指针eDest， 整型指针iParm*/
  pDest->eDest = (u8)eDest; /*把整型eDest 强制类型转化为u8型，然后赋值给pDest->eDest*/
  pDest->iSDParm = iParm; /*整型参数iParm赋值为pDest->iSDParm*/
  pDest->affSdst = 0; /*0赋值给pDest->affSdst*/
  pDest->iSdst = 0; /*0赋值给pDest->iSdst*/
  pDest->nSdst = 0; /*0赋值给pDest->nSdst*/
}


/*
** Allocate a new Select structure and return a pointer to that
** structure.
**分配一个新的选择结构并返回一个指向该结构体的指针.select语法分析最终在sqlite3SelectNew中完成，得到各个语法树汇总到Select结构体，然后根据结构体，进行语义分析生成执行计划。，
*/
Select *sqlite3SelectNew(
  Parse *pParse,        /* Parsing context  语义分析*/
  ExprList *pEList,     /* which columns to include in the result  输出结果列的语法树*/
  SrcList *pSrc,        /* the FROM clause -- which tables to scan  from语法树---扫描表 */
  Expr *pWhere,         /* the WHERE clause  where部分的语法树*/
  ExprList *pGroupBy,   /* the GROUP BY clause   group by语句的语法树*/
  Expr *pHaving,        /* the HAVING clause  having语句的语法树*/
  ExprList *pOrderBy,   /* the ORDER BY clause  order by语句的语法树*/
  int isDistinct,       /* true if the DISTINCT keyword is present  如果关键字distinct存在，则返回true*/
  Expr *pLimit,         /* LIMIT value.  NULL means not used  limit值，如果值为空意味着limit未使用*/
  Expr *pOffset         /* OFFSET value.  NULL means no offset  offset值，如果值为空意味着offset未使用*/
){
  Select *pNew;/*定义结构体指针pNew*/
  Select standin;/*定义结构体类型变量standin*/
  sqlite3 *db = pParse->db;/*结构体Parse的成员db赋值给结构体sqlite3指针db*/
  pNew = sqlite3DbMallocZero(db, sizeof(*pNew) );  /* 分配和清空内存，分配大小为第二个参数的内存 */
  assert( db->mallocFailed || !pOffset || pLimit ); /* 判断分配是否失败,或pOffset值为空,或pLimit值不为空*/
  if( pNew==0 ){/*如果结构体指针变量pNew指向的地址为0*/
    assert( db->mallocFailed );
    pNew = &standin;/*把standin的存储地址赋给pNew*/
    memset(pNew, 0, sizeof(*pNew));　/*将PNew中前sizeof(*pNew)个字节用0替换*/
  }
  if( pEList==0 ){/*如果指针EList指向的地址为空，即表达式列表为空*/
    pEList = sqlite3ExprListAppend(pParse, 0, sqlite3Expr(db,TK_ALL,0)); /* 在表达式列表后追加一条表达式，
                                                                                           如果没有表达式列表会新建一条再加入
	                                                                  */
  }
  pNew->pEList = pEList;/*pEList指向元素的地址赋给pNew->pEList*/
  if( pSrc==0 ) pSrc = sqlite3DbMallocZero(db, sizeof(*pSrc));/*如果PSrc的地址为空，则分配大小为第二个参数的内存*/
  pNew->pSrc = pSrc;/*为SELECT结构体中FROM子句表达式赋值*/
  pNew->pWhere = pWhere;/*为SELECT结构体中WHERE表达式赋值*/
  pNew->pGroupBy = pGroupBy;/*为SELECT结构体中Groupby表达式赋值*/
  pNew->pHaving = pHaving;/*为SELECT结构体中HAVING达式赋值*/
  pNew->pOrderBy = pOrderBy;/*为SELECT结构体中ORDERBY表达式赋值*/
  pNew->selFlags = isDistinct ? SF_Distinct : 0;
  pNew->op = TK_SELECT;
  pNew->pLimit = pLimit;/*为SELECT结构体中Limit表达式赋值*/
  pNew->pOffset = pOffset;/*为SELECT结构体中POFFSET表达式赋值*/
  assert( pOffset==0 || pLimit!=0 );/*如果偏移的记录数为空，指定返回记录的最大数量不为空，则分配内存*/
  pNew->addrOpenEphm[0] = -1;/*把-1赋值给pNew->addrOpenEphm[0] */
  pNew->addrOpenEphm[1] = -1;/*把-1赋值给pNew->addrOpenEphm[1] */
  pNew->addrOpenEphm[2] = -1;/*把-1赋值给pNew->addrOpenEphm[2] */
  if( db->mallocFailed ) {/*如果内存分配失败*/
    clearSelect(db, pNew);  /*删除所有选择的内容结构但不释放选择结构本身*/
    if( pNew!=&standin ) sqlite3DbFree(db, pNew); /*如果pNew没有获得standin的地址，释放相关联的内存。 */
    pNew = 0;/*将SELECT的结构体设置为空*/
  }else{/*能分配内存*/
    assert( pNew->pSrc!=0 || pParse->nErr>0 );/*判断是否有from子句，或者是否有分析错误*/
  }
  assert( pNew!=&standin );/*判断Select结构体是否同替换结构体相同*/
  return pNew;/*返回这个构造好的Select结构体*/
}

/*
** Delete the given Select structure and all of its substructures.
** 删除给定的选择结构和所有的子结构
*/
void sqlite3SelectDelete(sqlite3 *db, Select *p){
  if( p ){/*如果结构体指针p指向的地址非空*/
    clearSelect(db, p);  /*删除所有选择的内容结构但不释放选择结构本身*/
    sqlite3DbFree(db, p);  /*空闲内存,可能被关联到一个特定的数据库连接。*/
  }
}

/*
** Given 1 to 3 identifiers preceeding the JOIN keyword, determine the
** type of join.  Return an integer constant that expresses that type
** in terms of the following bit values:
** 在连接关键字前加一到三个标示符，决定使用何种连接方式。返回一个整数，表示使用以下的何种连接类型:
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
** 全外连接是JT_LEFT和JT_RIGHT结合。 如果检测到是非法字符或者不支持的连接类型，
** 也会返回一个连接类型，但是会在pParse结构中放入一个错误信息。
*/
int sqlite3JoinType(Parse *pParse, Token *pA, Token *pB, Token *pC){ /*定义连接函数*/
/*传入分析树，三个令牌结构体pA、pB、pC*/
	int jointype = 0;/*定义连接整数类型*/
/*默认连接类型为0*/
	Token *apAll[3]; /*定义结构体指针数组apAll*/
/*定义结构体指针数组apAll存放令牌*/
	Token *p; /*定义结构体指针p*/
/*声明一个临时令牌*/
	/*   0123456789 123456789 123456789 123 */
	static const char zKeyText[] = "naturaleftouterightfullinnercross";  /*定义只读的且只能在当前模块中可见的字符型数组zKeyText，并对其进行赋值*/
/*存放字符数组，里面装的是连接类型，在下文中进行调用*/
	static const struct { /*定义只读的且只能在当前模块可见结构体*/
/*声明一个内部结构体*/
		u8 i;        /* 定义KeyText[] 中开始关键字的文本*/
/* Beginning of keyword text in zKeyText[]   zKeyText数组的起始关键字*/
		u8 nChar;    /*定义字符中关键字的长度*/
/* Length of the keyword in characters  关键字的字符长度*/
		u8 code;    /* 定义连接标识符类型 */
 /* Join type mask 标记连接类型*/
	} aKeyword[] = { /*给定义的数组赋值*/
/*声明一个关键字数组，根据下标和长度找到连接类型*/
		/* natural 下标从0开始，长度为7，自然连接 */{ 0, 7, JT_NATURAL },/* natural 自然连接 */
		/* left   下标从6开始，长度为4，左连接 */{ 6, 4, JT_LEFT | JT_OUTER },/* left   左连接 */
		/* outer  下标从10开始，长度为5，外连接 */{ 10, 5, JT_OUTER },/* outer  外连接 *
		/* right   下标从14开始，长度为5，右连接*/{ 14, 5, JT_RIGHT | JT_OUTER },/* right   右连接*/
		/* full    下标从19开始，长度为4，全连接*/{ 19, 4, JT_LEFT | JT_RIGHT | JT_OUTER },/* full    全连接*/
		/* inner  下标从23开始，长度为5，内连接 */{ 23, 5, JT_INNER },/* inner  内连接 */
		/* cross   下标从28开始，长度为5，内连接或CROSS连接*/{ 28, 5, JT_INNER | JT_CROSS },/* cross   交叉连接*/
	};//定义全部类型的连接，并给出起始位置、长度、连接类型
	int i, j;/*定义两个整型数*/
	apAll[0] = pA;/*令牌pA指向的地址赋给apAll[0]*/  /*指针pA指向的地址赋给apAll[0] */
	apAll[1] = pB;/*指针pB指向的地址赋给apAll[1]*/  /*指针pB指向的地址赋给apAll[1] */
	apAll[2] = pC;/*指针pC指向的地址赋给apAll[2] */  /*指针pc指向的地址赋给apAll[2] */
	for (i = 0; i < 3 && apAll[i]; i++){ /*定义循环结构*/
//循环处理apAll数组
		p = apAll[i];/*指针apAll[i]的地址赋给指针p*/  /*指针apAll[i]的地址赋给指针p*/
		if (p->n == aKeyword[j].nChar /*如果令牌中字符个数等于连接数组中的关键字长度*/
			&& sqlite3StrNICmp((char*)p->z, &zKeyText[aKeyword[j].i], p->n) == 0){/*定义条件语句进行判断*
/*并且p->z和p->n以及zKeyText[aKeyword[j].i]相等*/
			jointype |= aKeyword[j].code; /*统计连接的个数*/
/*如果通过了比较长度和内容，返回连接类型，注意是，使用的是“位或”*/
			break;  /*跳出循环*/   //跳出本层循环
		}
	}
	testcase(j == 0 || j == 1 || j == 2 || j == 3 || j == 4 || j == 5 || j == 6);  /*调用函数testcase()进行选择*/  /*利用测试代码中testcast，测试j值，是否在这个范围*/
	if (j >= ArraySize(aKeyword)){  /*给定义的数组赋值*/  /*如果j大于等于连接关键字数组*/
		jointype |= JT_ERROR;  /*统计连接的个数*/  /*那就jointype与JT_ERROR“位或”，返回一个错误*/
		break;  /*跳出循环*/   //跳出本层循环
	  }
	}
	if (
	(jointype & (JT_INNER | JT_OUTER)) == (JT_INNER | JT_OUTER) ||/*如果连接类型交上(JT_INNER|JT_OUTER)的结果，与JT_INNER和JT_OUTER一样*/
	(jointype & JT_ERROR) != 0/*或者连接类型交上JT_ERROR不等于0*/
	){  /*选择条件句判断*/
		const char *zSp = " ";  /*只读的字符型指针zSp*/ /*空值赋给只读的字符型指针zSp*/
		assert(pB != 0);  /*调用函数assert()*/  /*判断pB是否为空*/
		if (pC == 0){ zSp++; }  /*如果指针pC指向的地址为0，那么指针zSp指向的地址前移一个存储单元*/  /*如果指针pC指向的地址为0，则zSp++*/
		sqlite3ErrorMsg(pParse, "unknown or unsupported join type: ""%T %T%s%T", pA, pB, zSp, pC);  /*抛出错误消息*/  /*在Parse分析树中，存放一个错误信息”*/
		jointype = JT_INNER ;  /*给返回类型赋值*/  /*默认使用内连接*/
	}
	else if ((jointype & JT_OUTER) != 0/* 如果连接类型和外连接有交集*/
		&& (jointype & (JT_LEFT | JT_RIGHT)) != JT_LEFT){/*如果连接类型和外连接有交集，并且连接类型和(JT_LEFT|JT_RIGHT)交集，不是左连接*/
		sqlite3ErrorMsg(pParse,"RIGHT and FULL OUTER JOINs are not currently supported");  /*考虑另一情况的判断*/  /*那么在Parse分析树中返回一个“右连接和全外连接当前不被支持”的错误消息”*/
		jointype = JT_INNER; /*给返回类型赋值*/  /*默认使用内连接*/
	}
	return jointype;   /*返回连接数的个数*/  /*返回连接类型*/

/*
** Return the index of a column in a table.  Return -1 if the column
** is not contained in the table.
** 返回一个表中的列的索引。如果该列没有包含在表中返回-1。
*/
static int columnIndex(Table *pTab, const char *zCol){/*定义静态的整型函数columnIndex，参数列表为列名——结构体指针pTab、表名——只读的字符型指针zCol*/
	int i;/*定义临时变量*/
	for (i = 0; i < pTab->nCol; i++){/*对所有的列进行遍历*/
		if (sqlite3StrICmp(pTab->aCol[i].zName, zCol) == 0) return i;/*如果pTab->aCol[i].zName和zCol指向的地址是同一个地址，那么返回i*/
	}
	return -1;/*否则，返回-1*/
}

/*
** Search the first N tables in pSrc, from left to right, looking for a
** table that has a column named zCol.
** 在FROM子句中扫描表，从左到右,查找前N个表,找一个含有列名为zCol的表 。
**
** When found, set *piTab and *piCol to the table index and column index
** of the matching column and return TRUE.
** 找到之后,设置*piTab给表索引，设置*piCol给需要匹配的列索引，再返回TRUE
**
** If not found, return FALSE.
** 如果没有找到，返回FALSE。
*/
static int tableAndColumnIndex(
	SrcList *pSrc,       /* Array of tables to search 定义对数据库中表进行搜索*/
	int N,               /* Number of tables in pSrc->a[] to search 对所有的列进行遍历*/
	const char *zCol,    /* Name of the column we are looking for 定义字符常量，标记寻找的列名*/
	int *piTab,          /* Write index of pSrc->a[] here 定义写入索引的数据类型表名*/
	int *piCol           /* Write index of pSrc->a[*piTab].pTab->aCol[] here 定义写入索引的数据类型列名*/
	){
	int i;               /* For looping over tables in pSrc 为循环表中的遍历定义整型数据类型*/
	int iCol;            /* Index of column matching zCol   定义索引要查找的列名*/

	assert((piTab == 0) == (piCol == 0));  /* Both or neither are NULL  判断表索引和列索引是否都为空*/
	for (i = 0; i < N; i++){/*定义循环结构进行遍历表中元素*/  /*遍历所有的表*/
		iCol = columnIndex(pSrc->a[i].pTab, zCol); /*搜索定位列中的元素*/  /*返回表的列的索引赋给iCol，如果该列没有在表中，iCol的值是-1..*/
		if (iCol >= 0){/*判断列索引是否存在*/  /*如果列索引存在*/
			if (piTab){/*判断表索引是否存在*/  /*如果表索引存在*/
				*piTab = i;/*把i的值赋给表中的项*/ /*把i 赋给指针piTab 的目标变量*/
				*piCol = iCol;/*把目标变量赋给指针变量*/ /*把iCol 赋值给指针piCol 的目标变量*/
			}
			return 1;/*执行结束返回1*/  /*否则返回1*/
		}
	}
	return 0;/*执行结束返回0*/
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

** 这个函数用来添加where子句解释含有JOIN语法句,从而解释select语句。
** 这个新条款添加到含有where子句中的，格式如下:
**
** (tab1.col1 = tab2.col2)
**
** tab1是SrcList pSrc的iSrc'th表，tab2是(iSrc+1)'th。列col1是tab1的iColLeft列，col2是
** tab2的iColRight列
*/
static void addWhereTerm(/*定义增加查询语句函数*/
	Parse *pParse,                  /* Parsing context  定义数组类型的指针元素*/
	SrcList *pSrc,                  /* List of tables in FROM clause   定义SrcList类型指针 */
	int iLeft,                      /* Index of first table to join in pSrc  定义整型的左连接 */
	int iColLeft,                   /* Index of column in first table  定义整型的左连接行*/
	int iRight,                     /* Index of second table in pSrc  定义整型的右连接*/
	int iColRight,                  /* Index of column in second table  定义整型的右连接行*/
	int isOuterJoin,                /* True if this is an OUTER join  定义整型的外连接*/
	Expr **ppWhere                  /* IN/OUT: The WHERE clause to add to  定义添加where子句的内存地址*/
	){
	sqlite3 *db = pParse->db;/*给数据库中的元素赋值*/
	Expr *pE1; /*定义结构体中的指针元素*pE1*/
	Expr *pE2; /*定义结构体中的指针元素*pE2*/
	Expr *pEq; /*定义结构体中的指针元素*pE3*/

	assert(iLeft<iRight);/*调用assert函数，所有元素向左移*/ /*判断如果第一个表索引值是否小于第二个表索引值*/
	assert(pSrc->nSrc>iRight);/*调用assert函数，所有元素向右移*/ /*定义结构体中的指针元素*pE1*/ /*判断表集合中的表的数目是否大于右表的索引值*/
	assert(pSrc->a[iLeft].pTab);/*调用assert函数，所有元素向左移进表中*/ /*判断表集合中表中左表索引的表是否为空*/
	assert(pSrc->a[iRight].pTab);/*调用assert函数，所有元素向右移进表中*/ /*判断表集合中表中右表索引的表是否为空*/

	pE1 = sqlite3CreateColumnExpr(db, pSrc, iLeft, iColLeft);/*分配并返回一个表达式指针去加载表集合中左表的一个列索引*/
	pE2 = sqlite3CreateColumnExpr(db, pSrc, iRight, iColRight);/*分配并返回一个表达式指针去加载表集合中右表的一个列索引*/

	pEq = sqlite3PExpr(pParse, TK_EQ, pE1, pE2, 0);/*分配一个额外节点连接这两个子树表达式*/
	if (pEq && isOuterJoin){/*如果定义结构体指针pEq指向的地址非空且isOuterJoin为真*/
		ExprSetProperty(pEq, EP_FromJoin);/*那么在连接中使用ON或USING子句*/
		assert(!ExprHasAnyProperty(pEq, EP_TokenOnly | EP_Reduced));/*判断pEq表达式是否是EP_TokenOnly或EP_Reduced*/
		ExprSetIrreducible(pEq);/*调试pEq,设置是否可以约束*/
		pEq->iRightJoinTable = (i16)pE2->iTable;/*指定第二个表达式的表赋值给要连接的右表*/
	}
	*ppWhere = sqlite3ExprAnd(db, *ppWhere, pEq);/*对指定数据库的表达式是进行连接*/
}


/*
** Set the EP_FromJoin property on all terms of the given expression.
** And set the Expr.iRightJoinTable to iTable for every term in the
** expression.
**  The EP_FromJoin property is used on terms of an expression to tell
** the LEFT OUTER JOIN processing logic that this term is part of the
** join restriction specified in the ON or USING clause and not a part
** of the more general WHERE clause.  These terms are moved over to the
** WHERE clause during join processing but we need to remember that they
** originated in the ON or USING clause.
**  The Expr.iRightJoinTable tells the WHERE clause processing that the
** expression depends on table iRightJoinTable even if that table is not
** explicitly mentioned in the expression.  That information is needed
** for cases like this:
**SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.b AND t1.x=5
** The where clause needs to defer the handling of the t1.x=5
** term until after the t2 loop of the join.  In that way, a
** NULL t2 row will be inserted whenever t1.x!=5.  If we do not
** defer the handling of t1.x=5, it will be processed immediately
** after the t1 loop and rows with t1.x!=5 will never appear in
** the output, which is incorrect.
** Expr.iRightJoinTable告诉where子句表达式依靠表iRightJoinTable处理，即使表在
** 表达式中没有明确提到。这些信息需要像这个例子:
** SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.b AND t1.x=5
** where 子句需要推迟处理t1.x=5，直到t2循环连接完毕。通过这种方式,每当t1.x!=5时，一个NULL t2行将被加入。
**如果我们没有延时处理t1.x=5，将会在t1循环完后立刻被处理，列t1.x!=5永远不会输出，这是不正确的。
*/
static void setJoinExpr(Expr *p, int iTable){/*传入一个表达式，一个待连接的表*/
  while( p ){
    ExprSetProperty(p, EP_FromJoin);/*设定join中使用ON和USING子句*/
    assert( !ExprHasAnyProperty(p, EP_TokenOnly|EP_Reduced) );/*判断表达式的属性*/
    ExprSetIrreducible(p);/*调试表达式，判断读是否错误*/
    p->iRightJoinTable = (i16)iTable;/*连接右表，即参数中传入的表*/
    setJoinExpr(p->pLeft, iTable);/*递归调用自身*/
    p = p->pRight;/*赋值表达式p为原来p的右子节点*/
  } 
}

/*
** This routine processes the join information for a SELECT statement.
** ON and USING clauses are converted into extra terms of the WHERE clause.
** NATURAL joins also create extra WHERE clause terms.
**
** The terms of a FROM clause are contained in the Select.pSrc structure.
** The left most table is the first entry in Select.pSrc.  The right-most
** table is the last entry.  The join operator is held in the entry to
** the left.  Thus entry 0 contains the join operator for the join between
** entries 0 and 1.  Any ON or USING clauses associated with the join are
** also attached to the left entry.
**This routine returns the number of errors encountered.
**
** 这个函数用来处理在Select语句中join信息，
**ON和USING子句被转化为WHERE额外的子句，自然连接也创造额外的where子句
**from子句被Select.pSrc所包含，左边的表通常是Select.pSrc的入口，
**右边的表通常是最后一个入口，join操作符在入口的左边，然后链接操作符在入口0和入口1之间，
**任何涉及join的ON和USING子句，也将连接操作符放在左边的入口。
**这个函数返回遇到的错误的个数
**
** 自己的理解:根据表达式列表中表达式的关键字，判断是何种连接，是否符合语义
**，如果检测到错误，会生成一个错误信息到语法解析树。
**最后默认返回的连接类型是inner join信息。
*/
static int sqliteProcessJoin(Parse *pParse, Select *p){/*传入分析树,Select结构体*/
  SrcList *pSrc;                  /* All tables in the FROM clause   from子句中的所有表*/
  int i, j;                       /* Loop counters  循环计数器*/
  struct SrcList_item *pLeft;     /* Left table being joined   被添加的左表*/
  struct SrcList_item *pRight;    /* Right table being joined   被添加的右表*/

  pSrc = p->pSrc;/*把Select结构体中FROM属性赋值给 pSrc*/
  pLeft = &pSrc->a[0];/*把&pSrc->a[0]放入左表,即表数组中第一个表作为左表*/
  pRight = &pLeft[1];/*把&pLeft[1]放入右表，即表数组中第二个表作为右表*/
  for(i=0; i<pSrc->nSrc-1; i++, pRight++, pLeft++){/*遍历表数组中所有表*/
    Table *pLeftTab = pLeft->pTab;/*把pLeft->pTab赋给结构体指针pLeftTab,作为左表*/
    Table *pRightTab = pRight->pTab;/*把pRight->pTab赋给结构体指针pRightTab，作为右表*/
    int isOuter;

    if( NEVER(pLeftTab==0 || pRightTab==0) ) continue;/*左表或者右表有一个不为空*/
    isOuter = (pRight->jointype & JT_OUTER)!=0;/*右连接类型交外连接不为空,再赋值给 isOuter*/

    /* When the NATURAL keyword is present, add WHERE clause terms for
    ** every column that the two tables have in common.
    **当natural关键字存在，在Where子句中，两个表的共同列也会同时存在。
    */
    if( pRight->jointype & JT_NATURAL ){/*右连接类型交上自然连接不为空*/
      if( pRight->pOn || pRight->pUsing ){/*右表右ON或者USING子句*/
        sqlite3ErrorMsg(pParse, "a NATURAL join may not have "
           "an ON or USING clause", 0);?*输出自然连接中不能有ON或者USING子句*/
        return 1;
      }
      for(j=0; j<pRightTab->nCol; j++){/*遍历右表的列*/
        char *zName;   /* Name of column in the right table 右表中列的名字*/
        int iLeft;     /* Matching left table 匹配左表*/
        int iLeftCol;  /* Matching column in the left table 在左表中匹配列*/

        zName = pRightTab->aCol[j].zName;/*将右表中列的名字赋值给 zName*/
        if( tableAndColumnIndex(pSrc, i+1, zName, &iLeft, &iLeftCol) ){/*如果左表的列的索引存在*/
          addWhereTerm(pParse, pSrc, iLeft, iLeftCol, i+1, j,
                       isOuter, &p->pWhere);/*添加Where子句，设置左右表，列和连接方式*/
        }
      }
    }

    /* Disallow both ON and USING clauses in the same join
    **不允许on和using子句有相同的连接
    */
    if( pRight->pOn && pRight->pUsing ){/*如果右表中既有On又有Using*/
      sqlite3ErrorMsg(pParse, "cannot have both ON and USING "
        "clauses in the same join");/*语法树将会报错*/
      return 1;
    }

    /* Add the ON clause to the end of the WHERE clause, connected by
    ** an AND operator.
    **on子句添加到where子句的末尾，由一个and操作符相连。
    */
    if( pRight->pOn ){/*如果结构体指针pRight引用的成员变量pOn非空,即右表中有ON关键字*/
      if( isOuter ) setJoinExpr(pRight->pOn, pRight->iCursor);?*如果有外连接，设置连接表达式中ON子句和游标*/
      p->pWhere = sqlite3ExprAnd(pParse->db, p->pWhere, pRight->pOn);/*设置将WHERE子句与ON子句连接一起，赋值给结构体的Where*/
      pRight->pOn = 0;/*如果没有外连接，就设置不适用ON*/
    }

    /* Create extra terms on the WHERE clause for each column named
    ** in the USING clause.  Example: If the two tables to be joined are 
    ** A and B and the USING clause names X, Y, and Z, then add this
    ** to the WHERE clause:    A.X=B.X AND A.Y=B.Y AND A.Z=B.Z
    ** Report an error if any column mentioned in the USING clause is
    ** not contained in both tables to be joined.
    **在using子句中每一个命名的列创建额外的where子句条件。例如:
    **如果两个表的连接是A和B,using列名为X,Y,Z,然后把它们添加到
    **where子句:A.X=B.X AND A.Y=B.Y AND A.Z=B.Z
    **如果using子句中提到的任何列不包含在表的连接中，就会报告
    **一个错误。
    */
    if( pRight->pUsing ){/*如果结构体指针pRight引用的成员变量pUsing非空*/
      IdList *pList = pRight->pUsing;/*把pRight->pUsing赋给结构体指针pList*/
      for(j=0; j<pList->nId; j++){
        char *zName;     /* Name of the term in the USING clause   using子句的名称术语*/
        int iLeft;       /* Table on the left with matching column name   左表的匹配列名*/
        int iLeftCol;    /* Column number of matching column on the left  左边匹配列的列数*/
        int iRightCol;   /* Column number of matching column on the right  右边匹配列的列数*/

        zName = pList->a[j].zName;
        iRightCol = columnIndex(pRightTab, zName); /*返回一个表中的列的索引。根据如果该列没有在表中返回-1。
		                                     返回值赋给iRightCol。
		                                   */
        if( iRightCol<0
         || !tableAndColumnIndex(pSrc, i+1, zName, &iLeft, &iLeftCol)/*如果列不存在*/
        ){
          sqlite3ErrorMsg(pParse, "cannot join using column %s - column "
            "not present in both tables", zName);/*添加一条报错信息*/
          return 1;
        }
        addWhereTerm(pParse, pSrc, iLeft, iLeftCol, i+1, iRightCol,
                     isOuter, &p->pWhere);/*如果列存在则添加到Where子句中*/
      }
    }
  }
  return 0;
}

/*
** Insert code into "v" that will push the record on the top of the
** stack into the sorter.

**把代码插入到"v"，在分选机将会推进记录到栈的顶部。
*/
static void pushOntoSorter(
  Parse *pParse,         /* Parser context  语义分析*/
  ExprList *pOrderBy,    /* The ORDER BY clause   order by语句语法树*/
  Select *pSelect,       /* The whole SELECT statement  整个select语句*/
  int regData            /* Register holding data to be sorted  保持数据有序*/
){
  Vdbe *v = pParse->pVdbe;
  int nExpr = pOrderBy->nExpr;?*声明一个OrderBy子句*/
  int regBase = sqlite3GetTempRange(pParse, nExpr+2); /*分配或释放一块连续的寄存器，返回一个整数值，
                                                        把该值赋给regBase。
                                                       */
  int regRecord = sqlite3GetTempReg(pParse); /*分配一个新的寄存器用于控制中间结果。*/
  int op;
  sqlite3ExprCacheClear(pParse); /*清除所有列的缓存条目*/
  sqlite3ExprCodeExprList(pParse, pOrderBy, regBase, 0);/*生成代码，将给定的表达式列表的每个元素的值放到寄存器开始的
                                                          目标序列。返回元素评估的数量。*/
  sqlite3VdbeAddOp2(v, OP_Sequence, pOrderBy->iECursor, regBase+nExpr);/*将表达式放在VDBE中，再返回一个新的指令地址*/
  sqlite3ExprCodeMove(pParse, regData, regBase+nExpr+1, 1);/*更改寄存器中的内容，这样做能及时更新寄存器中的列缓存数据*/
  sqlite3VdbeAddOp3(v, OP_MakeRecord, regBase, nExpr + 2, regRecord);/*将nExpr放到当前使用的VDBE中，再返回一个新的指令地址*/
  if( pSelect->selFlags & SF_UseSorter ){/*如果select结构体中selFlags的值是 SF_UseSorter*/
    op = OP_SorterInsert;
  }else{
    op = OP_IdxInsert;
  }
  sqlite3VdbeAddOp2(v, op, pOrderBy->iECursor, regRecord);/*将OrderBy表达式放在当前使用的vdbe中，然后返回一个新的指令地址*/
  sqlite3ReleaseTempReg(pParse, regRecord); /*释放寄存器，使其可以从用于其他目的。如果一个寄存器当前被用于列缓存，
                                              则dallocation被推迟，直到使用的列寄存器变的陈旧*/ 
  sqlite3ReleaseTempRange(pParse, regBase, nExpr+2);/*释放regBase这个寄存器，长度是表达式的长度邋+2*/
  if( pSelect->iLimit ){/*如果使用Limit子句*/
    int addr1, addr2;
    int iLimit;
    if( pSelect->iOffset ){/*如果使用了Offset 偏移量*/
      iLimit = pSelect->iOffset+1;/*Limit的值为偏移量加1*/
    }else{
      iLimit = pSelect->iLimit;/*否则Limit的值就为当初的值*/
    }
    addr1 = sqlite3VdbeAddOp1(v, OP_IfZero, iLimit);/*这个结果限制了返回的条数ｅ*/
    sqlite3VdbeAddOp2(v, OP_AddImm, iLimit, -1);/*将指令放到VDBE中，然后返回一个地址*/
    addr2 = sqlite3VdbeAddOp0(v, OP_Goto);/*使用Goto语句之后返回的地址*/
    sqlite3VdbeJumpHere(v, addr1); /*改变指令地址的p2操作，使其指向下一条指令的地址编码。*/
    sqlite3VdbeAddOp1(v, OP_Last, pOrderBy->iECursor);/*将pOrderBy指令放到当前使用的虚拟机中，返回Last操作的最后地址*/
    sqlite3VdbeAddOp1(v, OP_Delete, pOrderBy->iECursor);
    sqlite3VdbeJumpHere(v, addr2);/*改变指定地址的操作，使其指向下一条指令的地址编码*/
  }
}

/*
** Add code to implement the OFFSET
**添加代码来实现offset的功能。
*/
static void codeOffset(
  Vdbe *v,          /* Generate code into this VM  在虚拟机中生成代码*/
  Select *p,        /* The SELECT statement being coded  声明SELECT结构体*/
  int iContinue     /* Jump here to skip the current record  从这里跳过当前记录*/
){
  if( p->iOffset && iContinue!=0 ){/*如果结构体指针p引用的成员iOffset非空且整型iContinue不等于0*/
    int addr;
    sqlite3VdbeAddOp2(v, OP_AddImm, p->iOffset, -1);/*在VDBE中新添加一条指令，返回新指令的地址*/
    addr = sqlite3VdbeAddOp1(v, OP_IfNeg, p->iOffset);
    sqlite3VdbeAddOp2(v, OP_Goto, 0, iContinue);/*设置眺往的地址*/
    VdbeComment((v, "skip OFFSET records"));/*输入偏移量*/
    sqlite3VdbeJumpHere(v, addr);/*改变指定地址的操作，使其指向下一条指令的地址编码*/
  }
}

/*
** Add code that will check to make sure the N registers starting at iMem
** form a distinct entry.  iTab is a sorting index that holds previously
** seen combinations of the N values.  A new entry is made in iTab
** if the current N values are new.
** A jump to addrRepeat is made and the N+1 values are popped from the
** stack if the top N elements are not distinct.
**添加代码检查去确定一个iMem表中N个注册者在一个单独的入口，
** iTab是一个分类索引，预先能看到N个值的组合，如果在 iTab中新存在一个N值 ，那么将会在 iTab中产生一个新的入口
**如果N+1个值突然从栈中弹出，其中有N个值不是唯一的，那么将会产生大量重复地址。
*/
static void codeDistinct(
  Parse *pParse,     /* Parsing and code generating context 语义和代码生成*/
  int iTab,          /* A sorting index used to test for distinctness 排序索引使用去检测唯一性*/
  int addrRepeat,    /* Jump to here if not distinct 如果不唯一跳转到此处*/
  int N,             /* Number of elements 元素的个数*/
  int iMem           /* First element 第一个元素*/
){
  Vdbe *v;/*定义结构体指针v*/
  int r1;

  v = pParse->pVdbe;/*把结构体成员pParse->pVdbe赋给结构体指针v*/
  r1 = sqlite3GetTempReg(pParse); /*分配一个新的寄存器用于控制中间结果，返回的整数赋给r1.*/
  sqlite3VdbeAddOp4Int(v, OP_Found, iTab, addrRepeat, iMem, N); /*把操作的值看成整数，然后添加这个操作符到虚拟机*/
  sqlite3VdbeAddOp3(v, OP_MakeRecord, iMem, N, r1);/*调用sqlite3VdbeAddOp3()修改指令的地址*/
  sqlite3VdbeAddOp2(v, OP_IdxInsert, iTab, r1);
  sqlite3ReleaseTempReg(pParse, r1);/*生成代码，将给定的表达式列表的每个元素的值放到寄存器开始的目标序列。返回元素评估的数量。*/
}

#ifndef SQLITE_OMIT_SUBQUERY/*测试SQLITE_OMIT_SUBQUERY是否被宏定义过*/
/*
** Generate an error message when a SELECT is used within a subexpression
** (example:  "a IN (SELECT * FROM table)") but it has more than 1 result
** column.  We do this in a subroutine because the error used to occur
** in multiple places.  (The error only occurs in one place now, but we
** retain the subroutine to minimize code disruption.)

**如果SELECT中使用这样的子表达式(a IN (SELECT * FROM table))将会产生错误，因为它的结果列不止一个，
**我们在子程序中这样做是因为错误通常发生在多个地方。 
**(现在错误只发生在一个地方，但是我们保留中断的子程序将代码错误减少到最小。)
*/
static int checkForMultiColumnSelectError(
  Parse *pParse,       /* Parse context. 语义分析 */
  SelectDest *pDest,   /* Destination of SELECT results   select结果的最终目的*/
  int nExpr            /* Number of result columns returned by SELECT *//*SELECT返回的结果列数*/
){
  int eDest = pDest->eDest;/*处理结果集*/
  if( nExpr>1 && (eDest==SRT_Mem || eDest==SRT_Set) ){/*如果结果集大于1并且SELECT的结果集是SRT_Mem或者SRT_Set*/
    sqlite3ErrorMsg(pParse, "only a single result allowed for "
       "a SELECT that is part of an expression");/*为pParse- > zErrMsg和增量pParse- > NERR添加一条错误消息 */

    return 1;/*此处返回1，因为有SELECT结果集，只不过结果集可能大于1*/
  }else{
    return 0;/*如果没有满足条件，只返回0*/
  }
}
#endif /*结束函数*/ /*终止if*/ 

/*
** This routine generates the code for the inside  dssvfcsafg  of the inner loop
** of a SELECT.
** 这段程序产生的代码是为了select内连接循环。
**
** If srcTab and nColumn are both zero, then the pEList expressions
** are evaluated in order to get the data for this row.  If nColumn>0
** then data is pulled from srcTab and pEList is used only to get the
** datatypes for each column.
**如果srcTab和nColumn都是零，那么pEList表达式为了获得行数据进行赋值。
**如果nColumn>0 那么数据从srcTab中拿出，pEList只用于从每一列获得数据类型。
**
**自己的理解:该函数主要生成输出结果列的opcode，先查找结果列，默认时索引优先
**查找后存放寄存器，进行循环，这段程序产生的代码是为了select内循环。
*/
static void  selectInnerLoop(
  Parse *pParse,          /* The parser context 语义分析*/
  Select *p,              /* The complete select statement being coded *//*SELECT查询结构体*/
  ExprList *pEList,       /* List of values being extracted  输出结果列的语法树*/
  int srcTab,             /* Pull data from this table 从这个表中提取数据*/
  int nColumn,            /* Number of columns in the source table  源表中列的数目*/
  ExprList *pOrderBy,     /* If not NULL, sort results using this key 如果不是NULL，使用这个key对结果进行排序*/
  int distinct,           /* If >=0, make sure results are distinct 如果>=0  对结果进行"去除重复操作"*/
  SelectDest *pDest,      /* How to dispose of the results 怎样处理结果集*/
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

  assert( v );/*判断虚拟机*/
  if( NEVER(v==0) ) return;/*如果虚拟机不存在，直接返回*/
  assert( pEList!=0 );/*判断表达式列表是否为空*/
  hasDistinct = distinct>=0;/*赋值去除重复操作符*/
  if( pOrderBy==0 && !hasDistinct ){
    codeOffset(v, p, iContinue); /*添加代码来实现offset,；即设置偏移量，偏移量参数是iContinue*/
  }

  /* Pull the requested columns.
  **从需要的列中取数据
  */
  if( nColumn>0 ){/*如果表中列的数目大于0*/
    nResultCol = nColumn;/*把列的数目赋给整型nResultCol*/
  }else{hj89yuiyog  
    nResultCol = pEList->nExpr;/*把结构体pEList的成员nExpr的值赋给nResultCol*/
  }
  if( pDest->iSdst==0 ){/*如果结构体pDest引用的成员iSdst的值为0*/
    pDest->iSdst = pParse->nMem+1;/*把pParse->nMem+1赋给pDest->iSdst*/
    pDest->nSdst = nResultCol;/*把nResultCo l赋给pDest->nSdst*/
    pParse->nMem += nResultCol;/*分析树的地址设为自身的再加上结果列的数量*/
  }else{ 
    assert( pDest->nSdst==nResultCol );/*判断结果集中寄存器的个数是否与结果列的列数相同*/
  }
  regResult = pDest->iSdst;/*再把存储结果集的寄存器的地址设为结果集的起始地址*/
  if( nColumn>0 ){/*列数大于0*/
    for(i=0; i<nColumn; i++){/*遍历列*/
      sqlite3VdbeAddOp3(v, OP_Column, srcTab, i, regResult+i)/*将队列操作送入到VDBE再返回新的指令地址*/
    }
  }else if( eDest!=SRT_Exists ){/*如果处理的结果集不存在*/
    /* If the destination is an EXISTS(...) expression, the actual
    ** values returned by the SELECT are not required.
    **如果最终是一个EXISTS(...) 表达式，事实上通过SELECT返回的值就不需要啦
    */
    sqlite3ExprCacheClear(pParse);  /*清除所有列的缓存条目。*/
    sqlite3ExprCodeExprList(pParse, pEList, regResult, eDest==SRT_Output);/*生成代码，将给定的表达式列表的每个元素的值
                                                                       放到寄存器开始的目标序列。返回元素评估的数量。*/
  }
  nColumn = nResultCol;/*赋值列数等于结果列的列数*/

  /* If the DISTINCT keyword was present on the SELECT statement
  ** and this row has been seen before, then do not make this row
  ** part of the result.
  **如果在select语句中出现distinct关键字，那么在行输出之前，那么这行不作为结果的一部分。
  */
  if( hasDistinct ){/*如果使用了Distinct关键字*/
    assert( pEList!=0 );/*判断被提取的值列表是否为空*/
    assert( pEList->nExpr==nColumn );/*判断被提取的值列表的列数是否等于列数*/
    codeDistinct(pParse, distinct, iContinue, nColumn, regResult);/*进行去除重复操作*/
    if( pOrderBy==0 ){/*如果没有使用OrderBy*/
      codeOffset(v, p, iContinue); /*使用codeOffset()逐条产生结果*/
    }
  }

  switch( eDest ){/*定义switch函数，根据参数eDest判断怎样处理结果*/
    /* In this mode, write each query result to the key of the temporary
    ** table iParm.
    **在这种模式下，写出每个临时表的查询结果
    */
#ifndef SQLITE_OMIT_COMPOUND_SELECT/*测试SQLITE_OMIT_COMPOUND_SELECT是否被宏定义过*/
    case SRT_Union: {/*如果eDest为SRT_Union，则结果作为关键字存储在索引*/
  int r1;
      r1 = sqlite3GetTempReg(pParse);/*为语法分析树分配一个寄存器*/
      sqlite3VdbeAddOp3(v, OP_MakeRecord, regResult, nColumn, r1);/*把OP_MakeRecord操作送入VDBE，再返回一个新的指令地址*/
      sqlite3VdbeAddOp2(v, OP_IdxInsert, iParm, r1);/*把 OP_IdxInsert操作送入VDBE，再返回一个新的指令地址*/
      sqlite3ReleaseTempReg(pParse, r1);/*释放寄存器，使其可以从用于其他目的。如果一个寄存器当前被用于列缓存，
                                          则dallocation被推迟，直到使用的列寄存器变的陈旧*/
      break;
    }

    /* Construct a record from the query result, but instead of
    ** saving that record, use it as a key to delete elements from
    ** the temporary table iParm.
    **构建一个记录的查询结果，而不是保存记录，将其作为删除临时表iParm的关键字。
    */
    case SRT_Except: {/*如果eDest为SRT_Except，则从union索引中移除结果*/
      sqlite3VdbeAddOp3(v, OP_IdxDelete, iParm, regResult, nColumn); /*添加一个新的指令VDBE指示当前的列表。返回新指令的地址。*/
      break;
    }
#endif/*终止if*/

    /* Store the result as data using a unique key.
    **使用唯一关键字存储结果
    */
    case SRT_Table:/*如果eDest为SRT_Table，则结果按照自动的rowid自动保存*/
    case SRT_EphemTab: {/*如果eDest为SRT_EphemTab，则创建临时表并存储为像SRT_Table的表*/
      int r1 = sqlite3GetTempReg(pParse); /*分配一个新的寄存器用于控制中间结果，并把返回值赋给r1*/
      testcase( eDest==SRT_Table );/*测试处理结果集的表名称*/
      testcase( eDest==SRT_EphemTab );/*测试处理结果集的表的大小*/
      sqlite3VdbeAddOp3(v, OP_MakeRecord, regResult, nColumn, r1);/*添加一个新的指令VDBE指示当前的列表。返回新指令的地址。*/
      if( pOrderBy ){
        pushOntoSorter(pParse, pOrderBy, p, r1);/*插入代码"V"，在分选机将会推进记录到栈的顶部*/
      }else{
        int r2 = sqlite3GetTempReg(pParse);/*分配一个新的寄存器用于控制中间结果，并把返回值赋给r2*/
        sqlite3VdbeAddOp2(v, OP_NewRowid, iParm, r2);/*把OP_NewRowid操作送入VDBE，再返回一个新的指令地址*/
        sqlite3VdbeAddOp3(v, OP_Insert, iParm, r1, r2);/*把 OP_Insert操作送入VDBE，再返回一个新的指令地址*/
        sqlite3VdbeChangeP5(v, OPFLAG_APPEND);  /*对于最新添加的操作，改变p5操作数的值。*/
        sqlite3ReleaseTempReg(pParse, r2);/*释放寄存器，使其可以从用于其他目的。如果一个寄存器当前被用于列缓存，则dallocation被推迟，直到使用的列寄存器变的陈旧*/
      }
      sqlite3ReleaseTempReg(pParse, r1);/*释放这个寄存器*/
      break;
    }

#ifndef SQLITE_OMIT_SUBQUERY/*测试SQLITE_OMIT_SUBQUERY是否被宏定义过*/
    /* If we are creating a set for an "expr IN (SELECT ...)" construct,
    ** then there should be a single item on the stack.  Write this
    ** item into the set table with bogus data.
    **如果创建一个expr IN (SELECT ...)表达式，
    **然后堆中应该只有一个对象，把这个表达式写到没有数据库的表中
    */
    case SRT_Set: {/*如果eDest为SRT_Set，则结果作为关键字存入索引*/
      assert( nColumn==1 );
      p->affinity = sqlite3CompareAffinity(pEList->a[0].pExpr, pDest->affSdst);
      if( pOrderBy ){
        /* At first glance you would think we could optimize out the
        ** ORDER BY in this case since the order of entries in the set
        ** does not matter.  But there might be a LIMIT clause, in which
        ** case the order does matter 
        **一开始看的时候，你会以为对整个集合的顺序进行ORDER BY优化是不重要的，
        **但是，使用了LIMIT关键字的时候，排序就重要了
        */
        pushOntoSorter(pParse, pOrderBy, p, regResult);/*进行OrderBy操作，然后放到分拣器的栈顶*/
      }else{
        int r1 = sqlite3GetTempReg(pParse);/*分配一个寄存器，存储中间的计算结果*/
        sqlite3VdbeAddOp4(v, OP_MakeRecord, regResult, 1, r1, &p->affinity, 1); /*把OP_MakeRecord操作放入VDBE，再返回一个新的指令地址*/
        sqlite3ExprCacheAffinityChange(pParse, regResult, 1);/*记录从istart开始发生icount寄存器中的改变的事实*/
        sqlite3VdbeAddOp2(v, OP_IdxInsert, iParm, r1); /*把OP_IdxInsert操作放入VDBE，再返回一个新的指令地址*/
        sqlite3ReleaseTempReg(pParse, r1);/*释放这个寄存器*/
      }
      break;
    }

    /* If any row exist in the result set, record that fact and abort.
    **如果任何一行在结果集中存在，记录这一事实并中止。
    */
    case SRT_Exists: {/*如果eDest为SRT_Exists，则结果若不为空存储1*/
      sqlite3VdbeAddOp2(v, OP_Integer, 1, iParm);/*把OP_Integer操作放入VDBE，再返回一个新的指令地址*/
      /* The LIMIT clause will terminate the loop for us 
      limit子句将终止我们的循环
      */
      break;
    }

    /* If this is a scalar select that is part of an expression, then
    ** store the results in the appropriate memory cell and break out
    ** of the scan loop.
    **如果标量查询是表达式的一部分，然后将结果存储在适当的存储单元，中止扫描循环。
    */
    case SRT_Mem: {/*如果eDest为SRT_Mem，则将结果存储在存储单元*/
      assert( nColumn==1 );/*判断被提取的值列表是否为空*/
      if( pOrderBy ){
        pushOntoSorter(pParse, pOrderBy , p, regResult);/*把pOrderBy操作记录放到分拣器的栈顶*/
      }else{
        sqlite3ExprCodeMove(pParse, regResult, iParm, 1);/*释放寄存器中的内容，保持寄存器中的内容及时更新*/
        /* The LIMIT clause will jump out of the loop for us 
        **limit子句会跳出我们的循环
        */
      }
      break;
    }
#endif /* #ifndef SQLITE_OMIT_SUBQUERY */

    /* Send the data to the callback function or to a subroutine.  In the
    ** case of a subroutine, the subroutine itself is responsible for
    ** popping the data from the stack.
    **将数据发送到回调函数或子程序。在子程序的情况下，子程序本身负责从堆栈中弹出的数据。
    */
    case SRT_Coroutine:/*如果eDest为SRT_Coroutine，则结果生成一个单列*/
    case SRT_Output: {/*如果eDest为SRT_Output，则输出结果的每一列*/
      testcase( eDest==SRT_Coroutine );/*测试处理结果集是否是协同处理*/
      testcase( eDest==SRT_Output );/*测试处理结果集是否要输出*/
      if( pOrderBy ){/*如果包含了OrderBy */
        int r1 = sqlite3GetTempReg(pParse);/*分配一个寄存器，存储中间结果*/
        sqlite3VdbeAddOp3(v, OP_MakeRecord, regResult, nColumn, r1);/*把OP_MakeRecord操作送入VDBE，再返回一个新的指令地址*/
        pushOntoSorter(pParse, pOrderBy, p, r1);/*把OrderBy操作记录放到分拣器的栈顶*/
        sqlite3ReleaseTempReg(pParse, r1);/*释放这个寄存器*/
      }else if( eDest==SRT_Coroutine ){/*如果处理结果集是协同处理*/
        sqlite3VdbeAddOp1(v, OP_Yield, pDest->iSDParm);/*把 OP_Yield操作送入VDBE，再返回一个新的指令地址*/
      }else{
        sqlite3VdbeAddOp2(v, OP_ResultRow, regResult, nColumn);/*把 OP_ResultRow操作送入VDBE，再返回一个新的指令地址*/
        sqlite3ExprCacheAffinityChange(pParse, regResult, nColumn);
      }
      break;
    }

#if !defined(SQLITE_OMIT_TRIGGER)/*条件编译*/
    /* Discard the results.  This is used for SELECT statements inside
    ** the body of a TRIGGER.  The purpose of such selects is to call
    ** user-defined functions that have side effects.  We do not care
    ** about the actual results of the select.
    **丢弃结果。这是用于触发器的select语句。这样选择的目的是要调用用户定义函数。
    **我们不关心实际的查询结果。
    */
    default: {/*默认条件下*/
      assert( eDest==SRT_Discard );/*如果处理结果集是SRT_Discard(舍弃)*/

#endif/*条件编译结束*/
	}

  /* Jump to the end of the loop if the LIMIT is reached.  Except, if
  ** there is a sorter, in which case the sorter has already limited
  ** the output for us.
  **如果到了  LIMIT的限制处，直接跳转到循环的结尾，此外，如果有分拣器。
  **在这种情况下分选机已经限制了我们的输出。
  */
  if( pOrderBy==0 && p->iLimit ){/*如果不包含OrderBy和Limit子句*/
    sqlite3VdbeAddOp3(v, OP_IfZero, p->iLimit, iBreak, -1);/*把 OP_IfZero操作送入VDBE，再返回一个新的指令地址*/
  }
}

/*
** Given an expression list, generate a KeyInfo structure that records
** the collating sequence for each expression in that expression list.

**给定一个表达式列表，生成一个在表达式列表中为每个表达式的排列顺序的关键字信息结构
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

**隔离关键信息结构已分配的内存，调用函数负责最后释放这个结构。
**添加关键信息结构给P4操作符代码块，通常使用P4_KEYINFO_HANDOFF 
*/
static KeyInfo *keyInfoFromExprList(Parse *pParse, ExprList *pList){/*定义静态的结构体指针函数keyInfoFromExprList，传入2个参数，一个为语法分析树，一个为表达式列表*/
  sqlite3 *db = pParse->db;/*把结构体类型是pParse的成员变量db赋给结构体类型是sqlite3的指针db*/
  int nExpr;
  KeyInfo *pInfo;/*定义结构体类型是KeyInfo的指针pInfo,声明关键字结构体*/
  struct ExprList_item *pItem;/*定义结构体类型是ExprList_item的指针pItem,声明表达式项结构体*/
  int i;

  nExpr = pList->nExpr;/*声明表达式列表中表达式的个数*/
  pInfo = sqlite3DbMallocZero(db, sizeof(*pInfo) + nExpr*(sizeof(CollSeq*)+1) );/*分配并清空内存，分配大小为第二个参数的内存*/
  if( pInfo ){/*如果存在关键字结构体*/
    pInfo->aSortOrder = (u8*)&pInfo->aColl[nExpr];/*设置关键信息结构体的排序为关键信息结构体中表达式中所含的关键字*/
    pInfo->nField = (u16)nExpr;/*对整型nExpr进行强制类型转换成u16，赋给 pInfo->nField*/
    pInfo->enc = ENC(db);/*关键信息结构体中编码方式为DB的编码方式*/
    pInfo->db = db;/*关键信息结构体中数据库为当前使用的数据库*/
    for(i=0, pItem=pList->a; i<nExpr; i++, pItem++){/*遍历当前的表达式列表*/
      CollSeq *pColl;/*定义结构体类型为CollSeq的指针pColl*/
      pColl = sqlite3ExprCollSeq(pParse, pItem->pExpr); /*为表达式pExpr返回默认排序顺序。如果没有默认排序 类型，返回0.*/
      if( !pColl ){/*如果没有指定排序的方法*/
        pColl = db->pDfltColl;/*将数据库中默认的排序方法赋值给  pColl */
      }
      pInfo->aColl[i] = pColl;
      pInfo->aSortOrder[i] = pItem->sortOrder;/*关键信息结构体中排序的顺序为语法分析树中语法项表达式的排序方法*/
    }
  }
  return pInfo;/*返回这个关键信息结构体*/
}

#ifndef SQLITE_OMIT_COMPOUND_SELECT/*测试SQLITE_OMIT_COMPOUND_SELECT是否被宏定义过*/
/*
** Name of the connection operator, used for error messages.

**连接符的名称，用于反馈错误消息。
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

**除非一个"EXPLAIN QUERY PLAN"命令正在处理，这个功能就是一个空操作。
**否则，它增加一个单独的输出行到EQP结果，形式为:
**"USE TEMP B-TREE FOR xxx"
**其中xxx是"distinct","order by",或者"group by"中的一个。究竟是哪个由
**zUsage参数决定。
*/
static void explainTempTable(Parse *pParse, const char *zUsage){
  if( pParse->explain==2 ){/*如果pParse->explain与字符z相同*/
    Vdbe *v = pParse->pVdbe;/*声明一个虚拟机*/
    char *zMsg = sqlite3MPrintf(pParse->db, "USE TEMP B-TREE FOR %s", zUsage);/*把输出的格式的内容传递给zMsg*/
    sqlite3VdbeAddOp4(v, OP_Explain, pParse->iSelectId, 0, 0, zMsg, P4_DYNAMIC); /*添加一个操作码，其中包括作为指针的p4值。*/
  }
}

/*
** Assign expression b to lvalue a. A second, no-op, version of this macro
** is provided when SQLITE_OMIT_EXPLAIN is defined. This allows the code
** in sqlite3Select() to assign values to structure member variables that
** only exist if SQLITE_OMIT_EXPLAIN is not defined without polluting the
** code with #ifndef directives.

**把表达式A赋值给B，当定义了SQLITE_OMIT_EXPLAIN ，这个宏指令的版本就被提供
**只有在没有更改代码和没有定义SQLITE_OMIT_EXPLAIN 的情况下，才为sqlite3Select() 中成员变量赋值
*/
# define explainSetInteger(a, b) a = b/*宏定义*/

#else

/* No-op versions of the explainXXX() functions and macros.      explainXXX() 函数和宏的无操作符版本*/
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

**除非一个"EXPLAIN QUERY PLAN"命令正在处理，否则这个功能就是一个空操作。
**否则，它增加一个单独的输出行到EQP结果，标题的形式为:

**"COMPOSITE SUBQUERIES iSub1 and iSub2 (op)"
**"COMPOSITE SUBQUERIES iSub1 and iSub2 USING TEMP B-TREE (op)"

**iSub1和iSub2整数作为相应的传递函数参数，运算是相同名称的参数
**的文本表示。参数"op"必是TK_UNION, TK_EXCEPT,TK_INTERSECT或者TK_ALL之一。
**如果参数bUseTmp是false就使用第一种形式，或者如果是true就使用第二种形式。
*/
static void explainComposite(
  Parse *pParse,                  /* Parse context 语义分析*/
  int op,                         /* One of TK_UNION, TK_EXCEPT etc.   TK_UNION, TK_EXCEPT等之一*/
  int iSub1,                      /* Subquery id 1 子查询id 1*/
  int iSub2,                      /* Subquery id 2 子查询id 2*/
  int bUseTmp                     /* True if a temp table was used 如果临时表被使用就是true*/
){
  assert( op==TK_UNION || op==TK_EXCEPT || op==TK_INTERSECT || op==TK_ALL );/*测试op是否等于TK_UNION，TK_EXCEPT，TK_INTERSECT ，TK_ALL*/
  if( pParse->explain==2 ){/*如果pParse->explain与字符z相同*/
    Vdbe *v = pParse->pVdbe;
    char *zMsg = sqlite3MPrintf(/*设置标志信息*/
        pParse->db, "COMPOUND SUBQUERIES %d AND %d %s(%s)", iSub1, iSub2,
        bUseTmp?"USING TEMP B-TREE ":"", selectOpName(op)/*将子查询1和子查询2的内容赋值给zMsg*/
    );
    sqlite3VdbeAddOp4(v, OP_Explain, pParse->iSelectId, 0, 0, zMsg, P4_DYNAMIC);/*将 OP_Explain提交给虚拟机，然后返回一个地址，地址为 P4_DYNAMIC指针中的值*/
  }
}
#else
/* No-op versions of the explainXXX() functions and macros. 无操作explainXXX()函数和宏版本的。*/
# define explainComposite(v,w,x,y,z)
#endif

/*
** If the inner loop was generated using a non-null pOrderBy argument,
** then the results were placed in a sorter.  After the loop is terminated
** we need to run the sorter and output the results.  The following
** routine generates the code needed to do that.

**如果内部循环中生成了一个非空的pOrderBy命令,然后把结果放置在一个分选机。 
**循环终止后我们需要运行分选机和输出结果。下面的例程生成所需的代码。 
*/
static void generateSortTail(
  Parse *pParse,    /* Parsing context 语义分析*/
  Select *p,        /* The SELECT statement   select查询结构体*/
  Vdbe *v,          /* Generate code into this VDBE  在VDBE中生成代码**/
  int nColumn,      /* Number of columns of data 数据的列数目*/
  SelectDest *pDest /* Write the sorted results here 在这里写入排序结果*/
){
  int addrBreak = sqlite3VdbeMakeLabel(v);     /* Jump here to exit loop 从循环中跳到此处*/
  int addrContinue = sqlite3VdbeMakeLabel(v);  /* Jump here for next cycle 跳转到这里进行下一个周期*/
  int addr;
  int iTab;
  int pseudoTab = 0;
  ExprList *pOrderBy = p->pOrderBy;/*将SELECT结构体中OrderBy赋值给表达式列表的OrderBy*/

  int eDest = pDest->eDest;/*将查询结果集中处理方式传递给 eDest*/
  int iParm = pDest->iSDParm;/*将查询结果集中处理方式中的参数传递给iParm*/

  int regRow;
  int regRowid;

  iTab = pOrderBy->iECursor;/*把pOrderBy->iECursor赋给整型iTab*/
  regRow = sqlite3GetTempReg(pParse);/*为pParse语法树分配一个寄存器，存储计算的中间结果*/
  if( eDest==SRT_Output || eDest==SRT_Coroutine ){/*如果输出结果的每一列等于eDest，或者结果生成的一个单列等于eDest*/
    pseudoTab = pParse->nTab++;/*将分析语法树中表树传递给pseudoTab*/
    sqlite3VdbeAddOp3(v, OP_OpenPseudo, pseudoTab, regRow, nColumn);/*将OP_OpenPseudo操作交给虚拟机*/
    regRowid = 0;
  }else{
    regRowid = sqlite3GetTempReg(pParse);/*为pParse语法树分配一个寄存器，存储计算的中间结果*/
  }
  if( p->selFlags & SF_UseSorter ){/*如果SELECT结构体中的selFlags属性值为SF_UseSorter,使用分拣器 */
    int regSortOut = ++pParse->nMem;/*分配寄存器，个数是分析语法树中内存数+1*/
    int ptab2 = pParse->nTab++;/*将分析语法树中表的个数赋值给ptab2*/
    sqlite3VdbeAddOp3(v, OP_OpenPseudo, ptab2, regSortOut, pOrderBy->nExpr+2);/*将 OP_OpenPseudo交给VDBE，返回表达式列表中表达式列表的个数+2*/
    addr = 1 + sqlite3VdbeAddOp2(v, OP_SorterSort, iTab, addrBreak);/*将 OP_SorterSort的值赋给VDBE，返回的地址+1赋给  addr*/
    codeOffset(v, p, addrContinue);/*设置偏移量，其中addrContinue是下一次循环要调到的地址*/
    sqlite3VdbeAddOp2(v, OP_SorterData, iTab, regSortOut);/*将 OP_SorterData操作交给虚拟机*/
    sqlite3VdbeAddOp3(v, OP_Column, ptab2, pOrderBy->nExpr+1, regRow);/*将 OP_Column操作交给虚拟机*/
    sqlite3VdbeChangeP5(v, OPFLAG_CLEARCACHE); /*对于最近添加的操作改变p5操作数的值。*/
  }else{
    addr = 1 + sqlite3VdbeAddOp2(v, OP_Sort, iTab, addrBreak);/*将 OP_Sort交给VDBE，返回地址加1*/
    codeOffset(v, p, addrContinue);/*添加代码来实现offset。*/
    sqlite3VdbeAddOp3(v, OP_Column,iTab,pOrderBy->nExpr+1,regRow);/*将 OP_Column操作交给VDBE，再把OP_Column的地址返回*/
  }
  switch( eDest ){/*switch函数，参数eDest*/
    case SRT_Table:/*如果eDest为SRT_Table，则结果按照自动的rowid自动保存*/
    case SRT_EphemTab: {/*如果eDest为SRT_EphemTab，则创建临时表并存储为像SRT_Table的表*/
      testcase( eDest==SRT_Table );/*处理方式中是否SRT_Table */
      testcase( eDest==SRT_EphemTab );/*处理方式中是否SRT_EphemTab */
      sqlite3VdbeAddOp2(v,OP_NewRowid,iParm, regRowid) ;/*将OP_NewRowid提交给VDBE，再返回这个操作的地址*/
      sqlite3VdbeAddOp3(v, OP_Insert, iParm, regRow, regRowid);/*将OP_Insert提交给VDBE，再返回这个操作的地址*/
      sqlite3VdbeChangeP5(v, OPFLAG_APPEND);
      break;
    }
#ifndef SQLITE_OMIT_SUBQUERY/*测试SQLITE_OMIT_SUBQUERY是否被宏定义过*/
    case SRT_Set: {/*如果eDest为SRT_Set，则结果作为关键字存入索引*/
      assert( nColumn==1 );/*加入断点，判断列数是否等于1*/
      sqlite3VdbeAddOp4(v, OP_MakeRecord, regRow, 1, regRowid, &p->affinity, 1);
      sqlite3ExprCacheAffinityChange(pParse, regRow, 1); /*记录从iStart开始，发生在iCount寄存器中的改变的事实。*/
      sqlite3VdbeAddOp2(v, OP_IdxInsert, iParm, regRowid);
      break;
    }
    case SRT_Mem: {/*如果eDest为SRT_Mem，则将结果存储在存储单元*/
      assert( nColumn==1 );/*加入断点，判断列数是否等于1*/
      sqlite3ExprCodeMove(pParse, regRow, iParm, 1);/*释放寄存器中的内容，保存寄存器的内容及时更新*/
      /* The LIMIT clause will terminate the loop for us 
      **limit子句将终止我们的循环
      */
		break;
	}
#endif/*终止if*/
    default: {/*默认条件*/
      int i;
      assert( eDest==SRT_Output || eDest==SRT_Coroutine ); /*插入断点，判断结果集处理集类型是否是SRT_Output或SRT_Coroutine */
      testcase( eDest==SRT_Output );/*测试是否包含SRT_Output*/
      testcase( eDest==SRT_Coroutine );
      for(i=0; i<nColumn; i++){/*遍历列*/
        assert( regRow!=pDest->iSdst+i );/*插入断点，判断寄存器的编号值不等于基止寄存器的编号值*/
        sqlite3VdbeAddOp3(v, OP_Column, pseudoTab, i, pDest->iSdst+i);/*将OP_Column操作交给VDBE，再返回这个操作的地址*/
        if( i==0 ){/*如果没有列*/
          sqlite3VdbeChangeP5(v, OPFLAG_CLEARCACHE);
        }
      }
      if( eDest==SRT_Output ){/*如果结果集的处理方式是SRT_Output*/
        sqlite3VdbeAddOp2(v, OP_ResultRow, pDest->iSdst, nColumn);
        sqlite3ExprCacheAffinityChange(pParse, pDest->iSdst, nColumn);
      }else{
        sqlite3VdbeAddOp1(v, OP_Yield, pDest->iSDParm);
      }
      break;
    }
  }
  sqlite3ReleaseTempReg(pParse, regRow);/*释放寄存器*/
  sqlite3ReleaseTempReg(pParse, regRowid);

  /* The bottom of the loop               
  **循环的底部
  */
  sqlite3VdbeResolveLabel(v, addrContinue);/*addrContinue作为下一条插入指令的地址，其中addrContinue能优先调用sqlite3VdbeResolveLabel*/
  if( p->selFlags & SF_UseSorter ){/*selFlags的值是SF_UseSorter*/
    sqlite3VdbeAddOp2(v, OP_SorterNext, iTab, addr);
  }else{
    sqlite3VdbeAddOp2(v, OP_Next, iTab, addr);
  }
  sqlite3VdbeResolveLabel(v, addrBreak);
  if( eDest==SRT_Output || eDest==SRT_Coroutine ){/*如果结果集的处理方式是SRT_Output或者SRT_Coroutine*/
    sqlite3VdbeAddOp2(v, OP_Close, pseudoTab, 0);
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
C F GVDS** The declaration type for any expression other than a column is NULL.
**返回一个指向表达式pExpr 包含 'declaration type'的字符串。
**这个字符串可以被静态调用，
**如果表达式是一列，声明类型是确切的数据类型定义从最初的
**create table 语句中获取。ROWID字段的声明类型是整数。当一个表达式
**被认为作为一列在子查询中是复杂的。在所有下面的SELECT语句
**的结果集的表达被认为是这个函数的列。

**   SELECT col FROM tbl;
**   SELECT (SELECT col FROM tbl;
**   SELECT (SELECT col FROM tbl);
**   SELECT abc FROM (SELECT col AS abc FROM tbl);


**声明的类型可以用在任何表达式上，除了是空的列
*/
static const char *columnType(/*定义静态且是只读的字符型指针columnType*/
  NameContext *pNC, /*声明一个命名上下文结构体*/
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
  if( NEVER(pExpr==0) || pNC->pSrcList==0 ) return 0;/*如果一个表达式为空或决定名字的列表是空，直接返回0*/

  switch( pExpr->op ){/*遍历表达式中的操作*/
    case TK_AGG_COLUMN:
    case TK_COLUMN: {
      /* The expression is a column. Locate the table the column is being
      ** extracted from in NameContext.pSrcList. This table may be real
      ** database table or a subquery.
      **表达式是一个列。被定位的表的列从NameContext.pSrcList中提取。
      **这个表可能是真实的数据库表，或者是一个子查询。
      */
      Table *pTab = 0;            /* Table structure column is extracted from*//*被提取的列组成的表*/
      Select *pS = 0;             /* Select the column is extracted from 选择列被提取*/
      int iCol = pExpr->iColumn;  /* Index of column in pTab在pTab中的列索引*/
      testcase( pExpr->op==TK_AGG_COLUMN );/*这个表达式的操作是否是TK_AGG_COLUMN(嵌套列)*/
      testcase( pExpr->op==TK_COLUMN );/*这个表达式的操作是否是TK_AGG_COLUMN(列索引)*/
      while( pNC && !pTab ){/*命名上下文结构体存在，被提取的列组成的表不存在*/
        SrcList *pTabList = pNC->pSrcList;/*命名上下文结构体中列表的赋值给描述FROM的来源表或者子查询结果的列表*/
        for(j=0;j<pTabList->nSrc && pTabList->a[j].iCursor!=pExpr->iTable;j++);/*遍历查询列表*/
        if( j<pTabList->nSrc ){/*如果j小于列表中表的个数*/
          pTab = pTabList->a[j].pTab;/*赋值列表中的第j-1 表给Table结构体的实体变量pTab*/
          pS = pTabList->a[j].pSelect;/*赋值列表中的第j-1 表的SELECT结构体给PS*/
        }else{
          pNC = pNC->pNext;/*将命名上下文结构体的下一个外部命名上下文赋值给pNC变量*/
        }
      }

      if( pTab==0 ){/*如果表为空*/
        /* At one time, code such as "SELECT new.x" within a trigger would
        ** cause this condition to run.  Since then, we have restructured how
        ** trigger code is generated and so this condition is no longer 
        ** possible. However, it can still be true for statements like
        ** the following:
        **
        **   CREATE TABLE t1(col INTEGER);
        **   SELECT (SELECT t1.col) FROM FROM t1;
        **
        ** when columnType() is called on the expression ", even
        ** though it should really be "INTEGER".
        **
        ** This is not a problem, as the column type of "t1.col" is never
        ** used. When columnType() is called on the expression 
        ** "(SELECT t1.col)", the correct type is returned (see the TK_SELECT
        ** branch below.  
        **如果表为空的时候，使用"SELECT NEW.X",声明的触发器将会执行
        **然后重新构造了触发器代码，并且这种情况不可再发生。但是，它仍然可以适用于
        **像下面的语句：
	
	      **CREATE TABLE t1(col INTEGER);
        **SELECT (SELECT t1.col) FROM FROM t1;
        
        **当columnType()调用在子选择中的表达式"t1.col"。在这种情况下，设置列的类型为空，
        **即使它确实应该"INTEGER"。

        **这不是一个问题，因为" t1.col "的列类型是从未使用过。当columnType ()被调用的
        **表达式"(SELECT t1.col)" ，则返回正确的类型(请参阅下面的TK_SELECT分支)。 
        */
        break;
      }

      assert( pTab && pExpr->pTab==pTab );/*插入断点，判断被提取的列组成的表存在或者表达式中的列表存在*/
      if( pS ){/*如果被提取的列组成的select结构体存在*/
        /* The "table" is actually a sub-select or a view in the FROM clause
        ** of the SELECT statement. Return the declaration type and origin
        ** data for the result-set column of the sub-select.
        **"表"实际上是一个子选择，或者是一个在select语句的from子句的视图。
        **返回声明类型和来源数据的子选择的结果集列。
        */
        if( iCol>=0 && ALWAYS(iCol<pS->pEList->nExpr) ){
          /* If iCol is less than zero, then the expression requests the
          ** rowid of the sub-select or view. This expression is legal (see 
          ** test case misc2.2.2) - it always evaluates to NULL.
          **如果iCol小于零，则表达式请求子选择或视图的rowid。
          **这种表达式合法的(见测试案例misc2.2.2)-它始终计算为空。
          */
          NameContext sNC;
          Expr *p = pS->pEList->a[iCol].pExpr;/*被提取的列组成的select结构体中表达式列表中第i个表达式赋值给p*/
          sNC.pSrcList = pS->pSrc;/*被提取的列组成的select结构体中pSrc赋值给pSrcList*/
          sNC.pNext = pNC;/*命名上下文结构体赋值给当前命名上下文结构体的next指针*/
          sNC.pParse = pNC->pParse;/*命名上下文结构体中的语法解析树赋值给当前命名上下文结构体的语法解析树*/
          zType = columnType(&sNC, p, &zOriginDb, &zOriginTab, &zOriginCol); /*将生成的属性类型赋值给  zType*/
        }
      }else if( ALWAYS(pTab->pSchema) ){/*如果pTab表的模式存在*/
        /* A real table 一个真实的表*/
        assert( !pS );/*插入断点，判断select结构体是否为空*/
        if( iCol<0 ) iCol = pTab->iPKey;/*如果列号小于0，将表中的关键字数组的首元素赋值给 iCol*/
        assert( iCol==-1 || (iCol>=0 && iCol<pTab->nCol) );/*插入断点，判断iCol在哪个范围*/
        if( iCol<0 ){/*如果列号小于0*/
          zType = "INTEGER";/*将类型定义为整形*/
          zOriginCol = "rowid";/*关键字为rowid*/
        }else{
          zType = pTab->aCol[iCol].zType;/*定义类型为类型表中第iCol的类型*/
          zOriginCol = pTab->aCol[iCol].zName;/*类型的名字为型表中第iCol的名字*/
        }
        zOriginTab = pTab->zName;/*定义类型，使用默认的名字*/
        if( pNC->pParse ){/*如果命名上下文结构体中的语法分析树存在*/
          int iDb = sqlite3SchemaToIndex(pNC->pParse->db, pTab->pSchema);/*将pSchema的指针转化给命名上下文结构体中分析语法树的db*/
          zOriginDb = pNC->pParse->db->aDb[iDb].zName;/*将上下文语法分析树中的db第i-1个DB的命名赋值给zOriginDb数据库名*/
        }
      }
      break;
    }
#ifndef SQLITE_OMIT_SUBQUERY/*测试SQLITE_OMIT_SUBQUERY是被宏定义过*/
    case TK_SELECT: {
      /* The expression is a sub-select. Return the declaration type and
      ** origin info for the single column in the result set of the SELECT
      ** statement.
      **表达式是一个子选择。返回声明类型和初始信息给select结构集中的一列*/
      */
      NameContext sNC;
      Select *pS = pExpr->x.pSelect;/*将表达式中select结构体赋值给一个select结构体实体变量*/
      Expr *p = pS->pEList->a[0].pExpr;/*将select的表达式列表中的第一个表达式赋值给表达式变量p*/
      assert( ExprHasProperty(pExpr, EP_xIsSelect) );/*插入断点，测试是否包含EP_xIsSelect表达式*/
      sNC.pSrcList = pS->pSrc;/*将SELECT结构体中FROM子句的属性赋值给命名上下文结构体中FROM子句列表*/
      sNC.pNext = pNC;/*命名上下文结构体赋值给当前命名上下文结构体的next指针*/
      sNC.pParse = pNC->pParse;/*将命名上下文结构体中分析语法树赋值给当前命名结构体的分析语法树属性*/
      zType = columnType(&sNC, p, &zOriginDb, &zOriginTab, &zOriginCol); /*返回属性类型*/
      break;
    }
#endif
  }
  
  if( pzOriginDb ){/*如果存在原始的数据库*/
    assert( pzOriginTab && pzOriginCol );/*插入断点，判断表和列是否存在*/
    *pzOriginDb = zOriginDb;/*文件中数据库赋值给数据库变量pzOriginDb*/
    *pzOriginTab = zOriginTab;
    *pzOriginCol = zOriginCol;
  }
  return zType;/*返回列类型*/
}

/////////
// 郭香丽
/*
** Generate code that will tell the VDBE the declaration types of columns
** in the result set.
** 生成代码，告诉VDBE在结果集中的列的声明类型的。
*/
static void generateColumnTypes(

  Parse *pParse,      /* Parser context 语义分析*/
  SrcList *pTabList,  /* List of tables 输出表的集合的语法树*/
  ExprList *pEList    /* Expressions defining the result set 输出结果列的语法树*/
){
#ifndef SQLITE_OMIT_DECLTYPE/*测试SQLITE_OMIT_DECLTYPE是否被宏定义过*/
  Vdbe *v = pParse->pVdbe;/*将分析语法树中的VDBE赋值给VDBE变量*/
  int i;
  NameContext sNC;
  sNC.pSrcList = pTabList;/*将表集合赋值给命名上下文结构体中的表集合属性*/
  sNC.pParse = pParse;/*将分析语法树赋值给命名上下文语法树*/
  for(i=0; i<pEList->nExpr; i++){/*遍历表达式列表*/
    Expr *p = pEList->a[i].pExpr;/*将表达式变量赋值给P*/
    const char *zType;
#ifdef SQLITE_ENABLE_COLUMN_METADATA/*测试SQLITE_ENABLE_COLUMN_METADATA是否被宏定义过*/
    const char *zOrigDb = 0;
    const char *zOrigTab = 0;
    const char *zOrigCol = 0;
    zType = columnType(&sNC, p, &zOrigDb, &zOrigTab, &zOrigCol);

    /* The vdbe must make its own copy of the column-type and other 
    ** column specific strings, in case the schema is reset before this
    ** virtual machine is deleted.
    **VDBE必须能复制列类型和其它的自定义类型，在虚拟机删除之前必须重置这个模式*/
   */
    sqlite3VdbeSetColName(v, i, COLNAME_DATABASE, zOrigDb, SQLITE_TRANSIENT);/*返回数据库声明中的第i-1个名*/
    sqlite3VdbeSetColName(v, i, COLNAME_TABLE, zOrigTab, SQLITE_TRANSIENT);/*返回表声明中的第i-1个名*/
    sqlite3VdbeSetColName(v, i, COLNAME_COLUMN, zOrigCol, SQLITE_TRANSIENT);/*返回列声明中的第i-1个名*/
#else
    zType = columnType(&sNC, p, 0, 0, 0);/*返回列属性类型*/
#endif
    sqlite3VdbeSetColName(v, i, COLNAME_DECLTYPE, zType, SQLITE_TRANSIENT);/*返回类型获取声明中的第i-1个名*/
  }
#endif /* SQLITE_OMIT_DECLTYPE */
}

/*
** Generate code that will tell the VDBE the names of columns
** in the result set.  This information is used to provide the
** azCol[] values in the callback.
** 生成代码，告诉 VDBE 在结果集中的列的名称。这些信息被用于提供在回调中azCol[]的值。
*/
static void generateColumnNames(

  Parse *pParse,      /* Parser context   解析上下文 */
  SrcList *pTabList,  /* List of tables  *//*表的集合*/
  ExprList *pEList    /* Expressions defining the result set   定义结果集的表达式列表*/
){
  Vdbe *v = pParse->pVdbe;/*将语法解析树的VDBE属性赋值给VDBE变量*/
  int i, j;
  sqlite3 *db = pParse->db;/*将语法分析树的数据库赋值给数据库连接变量db*/
  int fullNames, shortNames;

#ifndef SQLITE_OMIT_EXPLAIN
  /* If this is an EXPLAIN, skip this step    如果这是一个EXPLAIN, 跳过这一步 */
  if( pParse->explain ){/*如果语法分析树中的explain属性存在，直接返回*/
    return;
  }
#endif

  if( pParse->colNamesSet || NEVER(v==0) || db->mallocFailed ) return;/*如果存在语法分析树的列名集合或者不存在VDBE变量或分配内存失败都将直接返回*/
  pParse->colNamesSet = 1;/*设置语法分析树中列名集合为1*/
  fullNames = (db->flags & SQLITE_FullColNames)!=0;/*如果数据库连接的名字交SQLITE全称列名不为空，交集返回给变量为fullNames*/
  shortNames = (db->flags & SQLITE_ShortColNames)!=0;/*如果数据库连接的名字交SQLITE简称列名不为空，交集返回给变量为fullNames*/
  sqlite3VdbeSetNumCols(v, pEList->nExpr);/*根据表达式返回结果集列的数量*/
  for(i=0; i<pEList->nExpr; i++){/*遍历标示符列表*/
    Expr *p;
    p = pEList->a[i].pExpr;/*将表达式列表中第i-1个赋值给p*/
    if( NEVER(p==0) ) continue;/*如果p为0，直接跳过*/
    if( pEList->a[i].zName ){/*如果第i-1个表达式的名字存在*/
      char *zName = pEList->a[i].zName;/*将第i-1个表达式的名字赋值给zName*/
      sqlite3VdbeSetColName(v, i, COLNAME_NAME, zName, SQLITE_TRANSIENT);/*设置SQL执行后返回结果集的列数*/
    }else if( (p->op==TK_COLUMN || p->op==TK_AGG_COLUMN) && pTabList ){/*如果表达式中操作为TK_COLUMN 或TK_AGG_COLUMN，并且与表的集合有交集*/
      Table *pTab;
      char *zCol;
      int iCol = p->iColumn;/*将表达式中iColumn赋值给iCol*/
      for(j=0; ALWAYS(j<pTabList->nSrc); j++){/*遍历表集合*/
        if( pTabList->a[j].iCursor==p->iTable ) break;/*如果表中的游标指向iTable ，则中断*/
      }
      assert( j<pTabList->nSrc );/*插入断点，判断i小于表集合的数目*/
      pTab = pTabList->a[j].pTab;/*将表集合中表赋值给变量pTab*/
      if( iCol<0 ) iCol = pTab->iPKey;/*如果列小于0，设置iCol为当前表变量的主键*/
      assert( iCol==-1 || (iCol>=0 && iCol<pTab->nCol) );/*插入断点，判断iCol的范围*/
      if( iCol<0 ){
        zCol = "rowid";
      }else{
        zCol = pTab->aCol[iCol].zName;/* zCol为当前表第i-1列的名字*/
      }
      if( !shortNames && !fullNames ){/*如果即不是简称又不是全称*/
        sqlite3VdbeSetColName(v, i, COLNAME_NAME, 
            sqlite3DbStrDup(db, pEList->a[i].zSpan), SQLITE_DYNAMIC);/*返回一个已分配给数据库连接中pEList->a[i].zSpan值的内存，再返回计算结果集的列数*/
      }else if( fullNames ){/*如果是全称*/
        char *zName = 0;
        zName = sqlite3MPrintf(db, "%s.%s", pTab->zName, zCol);/*列名就为 pTab中的zCol名*/
        sqlite3VdbeSetColName(v, i, COLNAME_NAME, zName, SQLITE_DYNAMIC);/*计算结果集的列数*/
      }else{
        sqlite3VdbeSetColName(v, i, COLNAME_NAME, zCol, SQLITE_TRANSIENT);
      }
    }else{
      sqlite3VdbeSetColName(v, i, COLNAME_NAME, 
          sqlite3DbStrDup(db, pEList->a[i].zSpan), SQLITE_DYNAMIC);/*返回一个已分配给数据库连接中 pEList->a[i].zSpan的值的内存，再返回结果集的列数*/
    }
  }
  generateColumnTypes(pParse, pTabList, pEList);/*根据语法分析树，表的列表和表达式的列表产生列类型*/
}

/*
** Given a an expression list (which is really the list of expressions
** that form the result set of a SELECT statement) compute appropriate
** column names for a table that would hold the expression list.
**All column names will be unique.
** Only the column names are computed.  Column.zType, Column.zColl,
** and other fields of Column are zeroed.
**Return SQLITE_OK on success.  If a memory allocation error occurs,
** store NULL in *paCol and 0 in *pnCol and return SQLITE_NOMEM.
** 给出一个表达式列表(这个列表是一个真正的select结果集的表达式列表)，为保存表达式列表的表计算适当的列名，
**所有的列都是唯一的，只有计算过的列名，列的类型，列的长度和列的其它字段才能进行初始化。
** 成功的返回SQLITE_OK，如果发生内存分配错误，将存储NULL在paCol和0在pnCol，并且返回SQLITE_NOMEM.
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
  int nName;                  /* Size of name in zName[] 存储列名的数组的长度*/

  if( pEList ){/*来自列名的表达式列表存在*/
    nCol = pEList->nExpr;/*结果集中列的总数等于表达式列表中表达式的个数*/
    aCol = sqlite3DbMallocZero(db, sizeof(aCol[0])*nCol);/*对循环结束的结果了的初始化，分配并清空内存，分配大小为第二个参数的内存*/
    testcase( aCol==0 );/*测试总数是否等于0*/
  }else{
    nCol = 0;/*否则定义列数为0*/
    aCol = 0;/*循环结束的结果集的列数为0*/
  }
  *pnCol = nCol;
  *paCol = aCol;/*写列表名等于结果列名*/

  for(i=0, pCol=aCol; i<nCol; i++, pCol++){/*遍历所有的结果列*/
    /* Get an appropriate name for the column   
    ** 得到一个适当的列的名称 
    */
    p = pEList->a[i].pExpr;/*表达式中第i-1个表达式为p*/
    assert( p->pRight==0 || ExprHasProperty(p->pRight, EP_IntValue)/*插入断点，表达式的右子节点是否为空或表达式的右子节点值是否为int*/
               || p->pRight->u.zToken==0 || p->pRight->u.zToken[0]!=0 );/*表达式的右子节点的标记值无0，或者右子节点的第一个标记值不为0 */
    if( (zName = pEList->a[i].zName)!=0 ){/*如果列名和表达式中的列名不同*/
      /* If the column contains an "AS <name>" phrase, use <name> as the name 
      **如果列包含一个“AS <name>”短语,使用<name>作为名称
      */
      zName = sqlite3DbStrDup(db, zName);/*数据库连接中的列名赋值给列名*/
    }else{
      Expr *pColExpr = p;  /* The expression that is the result column name 结果列名称的表达式*/
      Table *pTab;         /* Table associated with this expression 与表达式相关的表*/
      while( pColExpr->op==TK_DOT ){/*如果结果列名的表达式操作为TK_DOT*/
        pColExpr = pColExpr->pRight;/*赋值结果列名的右子节点给结构列名，这是一个递归的过程*/
        assert( pColExpr!=0 );/*插入断点，判断结果列名不为0*/
      }
      if( pColExpr->op==TK_COLUMN && ALWAYS(pColExpr->pTab!=0) ){/*如果结果列名表达式的操作为TK_COLUMN，并且表达式对应的表不为空*/
        /* For columns use the column name name 对于列使用列名的名字*/
        int iCol = pColExpr->iColumn;/*将表达式的列赋值给 iCol*/
        pTab = pColExpr->pTab;/*表达式对应的表赋值给  pTab*/
        if( iCol<0 ) iCol = pTab->iPKey;/*如果iCol的值小于0，令iCol的值为表的主键*/
        zName = sqlite3MPrintf(db, "%s",
                 iCol>=0 ? pTab->aCol[iCol].zName : "rowid");/*打印表的列名并赋值给zName*/
      }else if( pColExpr->op==TK_ID ){/*如果表达式的操作为TK_ID */
        assert( !ExprHasProperty(pColExpr, EP_IntValue) );/*插入断点，判断结果列名的表达式是否有int值*/ 
        zName = sqlite3MPrintf(db, "%s", pColExpr->u.zToken);/*打印结果列的表达式的标记值，并赋值给zName*/
      }else{
        /* Use the original text of the column expression as its name 使用的最初的列表达式文本进行命名*/
        zName = sqlite3MPrintf(db, "%s", pEList->a[i].zSpan);/*打印来自于列名的表达式列表的原文，并赋值给  zName*/
      }
    }
    if( db->mallocFailed ){/*分配内存出错*/
      sqlite3DbFree(db, zName);/*释放数据库连接中的列名*/
      break;
    }

    /* Make sure the column name is unique.  If the name is not unique,
    ** append a integer to the name so that it becomes unique.
    ** 确保列名是唯一的。如果名字不是唯一的,给名称附加一个整数,这样就变得唯一了
    */
    nName = sqlite3Strlen30(zName);/*限制列名的长度不超过30*/
    for(j=cnt=0; j<i; j++){
      if( sqlite3StrICmp(aCol[j].zName, zName)==0 ){/*如果初始化后的结果列中的列名字和在zName相同*/
        char *zNewName;
        zName[nName] = 0;
        zNewName = sqlite3MPrintf(db, "%s:%d", zName, ++cnt);/*格式化输出来连接 ++cnt个zName字符串，并赋值给zNewName */
        sqlite3DbFree(db, zName);/*释放可能关联的数据库连接的内存*/
        zName = zNewName;/*令zName的值为格式化处理后的zNewName值*/
        j = -1;
        if( zName==0 ) break;/*如果zName值为0，说明没有zName字符串，就不用再赋值*/
      }
    }
    pCol->zName = zName;/*将各种情况的zName值写到结果列中*/
  }
  if( db->mallocFailed ){/*如果分配失败*/
    for(j=0; j<i; j++){
      sqlite3DbFree(db, aCol[j].zName);/*释放关联列中名字为zName的内存块*/
    }
    sqlite3DbFree(db, aCol);/*释放关联结果列的数据库连接*/
    *paCol = 0;/*令写列表为0*/
    *pnCol = 0;/*令列名为0*/
    return SQLITE_NOMEM;
  }
  return SQLITE_OK;
}

/*
** Add type and collation information to a column list based on
** a SELECT statement.

** 在SELECT指令的基础上添加一个类型和查询信息到一个列。
** 
** The column list presumably came from selectColumnNamesFromExprList().
** The column list has only names, not types or collations.  This
** routine goes through and adds the types and collations.
**这个列表假设来自于selectColumnNamesFromExprList()，这个列有唯一的列名，,没有类型或排序。这个程序遍历并添加类型和排序。
** 
** This routine requires that all identifiers in the SELECT

** statement be resolved.

**这个程序要求select指令中所有标示符都是确定的。
*/
static void selectAddColumnTypeAndCollation(
  Parse *pParse,        /* Parsing contexts 解析上下文*/
  int nCol,             /* Number of columns 列数*/
  Column *aCol,         /* List of columns 列表*/
  Select *pSelect       /* SELECT used to determine types and collations      SELECT用于确定类型和排序*/
){
  sqlite3 *db = pParse->db;/*声明一个数据库连接*/
  NameContext sNC;/*声明一个命名上下文结构体*/
  Column *pCol;/*声明一个列表*/
  CollSeq *pColl;/*声明一个排序队列*/
  int i;
  Expr *p;/*声明一个表达式变量*/
  struct ExprList_item *a;/*声明一个表达式列表项变量*/

  assert( pSelect!=0 );/*插入一个断点，测试用来确定类型和排序规则的select结构体是否为空*/
  assert( (pSelect->selFlags & SF_Resolved)!=0 );/*插入断点,select结构体中的selFlags为SF_Resolved*/
  assert( nCol==pSelect->pEList->nExpr || db->mallocFailed );/*插入断点，判断列数是否等于表达式个数或者数据库连接是否分配内存失败*/
  if( db->mallocFailed ) return;
  memset(&sNC, 0, sizeof(sNC));/*将sNC中前 sizeof(sNC)个字节用0替换*/
  sNC.pSrcList = pSelect->pSrc; /*将select结构体的列表赋值给命名结构体的列表*/
  a = pSelect->pEList->a;/*将select结构体中表达式列表中的较大的表达式给a*/
  for(i=0, pCol=aCol; i<nCol; i++, pCol++){/*遍历列*/
    p = a[i].pExpr;/*令p等于列中表达式*/
    pCol->zType = sqlite3DbStrDup(db, columnType(&sNC, p, 0, 0, 0));/*将命名上下文中的表达式的类型赋值给zType*/
    pCol->affinity = sqlite3ExprAffinity(p);/*将命名上下文中的表达式的类型进行亲和性处理给affinity*/
    if( pCol->affinity==0 ) pCol->affinity = SQLITE_AFF_NONE;/*如果列表的亲和性处理值为0，则把SQLITE_AFF_NONE赋给affinity*/
    pColl = sqlite3ExprCollSeq(pParse, p);/*根据分析语法树和列表达式生成一个排序队列*/
    if( pColl ){/*如果存在排序队列*/
      pCol->zColl = sqlite3DbStrDup(db, pColl->zName);/*根据数据库连接和排序队列的列名生成一个排序队列*/
    }
  }
}

/*
** Given a SELECT statement, generate a Table structure that describes
** the result set of that SELECT.

**给定一个SELECT语句,生成一个表结构,来描述那个SELECT的结果集

*/ 
Table *sqlite3ResultSetOfSelect(Parse *pParse, Select *pSelect){
  Table *pTab;/*声明一个Table结构体*/
  sqlite3 *db = pParse->db;/*声明一个数据库连接*/
  int savedFlags;

  savedFlags = db->flags;/*保存数据库连接标记*/
  db->flags &= ~SQLITE_FullColNames;/*位于SQLite中列名的全称*/
  db->flags |= SQLITE_ShortColNames;/*或与SQLite中列名的简称*/
  sqlite3SelectPrep(pParse, pSelect, 0);/*根据语法分析树和SELECT结构体建立一个SELECT指令*/
  if( pParse->nErr ) return 0;/*如果语法分析树含有错误信息，直接返回0*/
  while( pSelect->pPrior ) pSelect = pSelect->pPrior;/*遍历select结构体的优先查找*/
  db->flags = savedFlags;/*将数据库连接标记放在数据库连接的flags属性中*/
  pTab = sqlite3DbMallocZero(db, sizeof(Table) );/*分配并清空内存，分配大小为第二个参数的内存*/
  if( pTab==0 ){/*如果表为空*/
    return 0;
  }
  /* The sqlite3ResultSetOfSelect() is only used n contexts where lookaside
  ** is disabled      
  **sqlite3ResultSetOfSelect()只被使用n上下文,在那里后备是不能用的*/
  assert( db->lookaside.bEnabled==0 );/*设置断点，判断后备上下文是否可用*/
  pTab->nRef = 1;/*令表中指针的数量为1*/
  pTab->zName = 0;/*表的名字为0*/
  pTab->nRowEst = 1000000;/*估计表和行数*/
  selectColumnsFromExprList(pParse, pSelect->pEList, &pTab->nCol, &pTab->aCol);
  selectAddColumnTypeAndCollation(pParse, pTab->nCol, pTab->aCol, pSelect);/*添加列的类型和校队信息*/
  pTab->iPKey = -1;/*令表的关键字为-1*/
  if( db->mallocFailed ){/*如果内存分配失败*/
    sqlite3DeleteTable(db, pTab);/*删除这个表*/
    return 0;
  }
  return pTab;
}

/*
** Get a VDBE for the given parser context.  Create a new one if necessary.
** If an error occurs, return NULL and leave a message in pParse.
** 从给定的解析器获得一个VDBE，在必要的情况下创建一个新的VDBE。
** 如果发生错误，返回NULL并且在pParse里留下信息。
*/
/*得到一个虚拟数据库引擎处理给的上下文语法解析器，如果需要创建一个新的。
 如果发生错误，返回空值并在分析语法树中留一个报错信息*/
Vdbe *sqlite3GetVdbe(Parse *pParse){

  Vdbe *v = pParse->pVdbe;/*声明一个虚拟数据库引擎*/
  if( v==0 ){/*如果虚拟数据库引擎为空*/
    v = pParse->pVdbe = sqlite3VdbeCreate(pParse->db);/*使用当前的VDBE根据语法分析树创建一个虚拟数据库引擎，并赋值给语法分析树中的PVDBE和局部变量*/
#ifndef SQLITE_OMIT_TRACE
    if( v ){/*如果V非空*/
      sqlite3VdbeAddOp0(v, OP_Trace);/*添加这个指令OP_Trace到V地址，并返回这个指令的地址*/
    }
#endif
  }
  return v;/*返回创建成功的VDBE*/
}
/*
** Compute the iLimit and iOffset fields of the SELECT based on the
** pLimit and pOffset expressions.  pLimit and pOffset hold the expressions
** that appear in the original SQL statement after the LIMIT and OFFSET
** keywords.  Or NULL if those keywords are omitted. iLimit and iOffset
** are the integer memory register numbers for counters used to compute
** the limit and offset.  If there is no limit and/or offset, then
** iLimit and iOffset are negative.
** 基于pLimit和pOffset表达式计算SELECT中的iLimit和iOffset字段。
** nLimit和nOffset持有这些出现在原始的SQL语句中LIMIT和OFFSET关键字之后的表达式。
** 如果这些关键词被省略的话,它的值为NULL。
** iLimit和iOffset是用来计算限制和偏移量的整数存储寄存器数据计数器。
** 如果没有限制和/或偏移,那么iLimit和iOffset是负的。
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
** 这个程序只有在限制或偏移由nLimit和nOffset定义的时候改变iLimit和 iOffset的值。
** 调用这个程序之前iLimit和iOffset应该是预设到适当的默认值(通常但不总是-1)。
** 只有nLimit>=0或者nOffset>0限制寄存器重新定义。
** UNION ALL操作符使用这个属性通过多个SELECT语句来迫使相同的限制和偏移暂存器的重用。
/*
	** 基于SELECT中Limit和Offset表达式计算limit和offset的域。pLimit和pOffset出现在原始的SQL语句的LIMIT和OFFSET关键字后。
	** 如果这些关键字是空，那就忽略。iLimit和iOffset整数寄存器编号用来计算limit和offset。如果没有limit和（或）offset，
	** iLimit和iOffset为负数。
	**
	** 如果iLimit和iOffset被pLimit和pOffset定义，那么这个程序能改变iLimit和iOffset的值。iLimit和iOffset应该被预先定义一个值
	** （应该但不总是为-1）优先使用这个程序。如果只是pLimit!=0或pOffset!=0，那么iLimit寄存器需被重新定义。
	** 在多重查询中UNION ALL操作符使用一种性质，强制重新使用limit和offset的相同的寄存器。
	*/
*/
static void computeLimitRegisters(Parse *pParse, Select *p, int iBreak){

  Vdbe *v = 0;/*初始化VSDE变量*/
  int iLimit = 0;/*初始化 iLimit变量*/
  int iOffset;/*声明iOffset*/
  int addr1, n;
  if( p->iLimit ) return;/*如果SELECT结构体中含有iLimit偏移量直接返回*/

  /* 
  ** "LIMIT -1" always shows all rows.  There is some
  ** contraversy about what the correct behavior should be.
  ** The current implementation interprets "LIMIT 0" to mean
  ** no rows.
  ** "LIMIT -1" 总是出现在所有行。这是正确行为的相反清况，当前应用解析LIMIT 0为空
  */
  sqlite3ExprCacheClear(pParse);/*清除缓冲区中的语法解析树*/
  assert( p->pOffset==0 || p->pLimit!=0 );/*插入断点，判断偏移量是否为0，返回行不为0*/
  if( p->pLimit ){/*如果返回行存在*/
    p->iLimit = iLimit = ++pParse->nMem;/*返回值为语法解析数中的占内存的行数*/
    v = sqlite3GetVdbe(pParse);/*根据语法解析树生成一个虚拟数据库引擎*/
    if( NEVER(v==0) ) return;  /* VDBE should have already been allocated VDBE应该已经分配*/
    if( sqlite3ExprIsInteger(p->pLimit, &n) ){/*判断这个Limit是否大于32，如果小于把n置为1，否则置为0*/
      sqlite3VdbeAddOp2(v, OP_Integer, n, iLimit);/*将 OP_Integer交给虚拟机，然后返回这个操作的地址*/
      VdbeComment((v, "LIMIT counter"));/*在VDBE中存入 "LIMIT counter"，对应上句的地址*/
      if( n==0 ){
        sqlite3VdbeAddOp2(v, OP_Goto, 0, iBreak);/*将 OP_Goto交给虚拟机，然后返回这个操作的地址*/
      }else{
        if( p->nSelectRow > (double)n ) p->nSelectRow = (double)n;/*如果SELECT结构体中的行数大于n，直接设置nSelectRow为n*/
      }
    }else{
      sqlite3ExprCode(pParse, p->pLimit, iLimit);/*存放pLimit表达式和iLimit的值*/
      sqlite3VdbeAddOp1(v, OP_MustBeInt, iLimit);/*将 OP_MustBeInt交给虚拟机，然后返回这个操作的地址*/
      VdbeComment((v, "LIMIT counter"));/*在VDBE中存入 "LIMIT counter"，对应上句的地址*/
      sqlite3VdbeAddOp2(v, OP_IfZero, iLimit, iBreak);/*将  OP_IfZero交给虚拟机，然后返回这个操作的地址*/
    }
    if( p->pOffset ){/*如果结构体中偏移量存在*/
      p->iOffset = iOffset = ++pParse->nMem;/*返回值为语法解析数中的占内存的行数*/
      pParse->nMem++;   /* Allocate an extra register for limit+offset*//*为limit+offset分配一个额外的寄存器*/
      sqlite3ExprCode(pParse, p->pOffset, iOffset);/*存放pOffset表达式和iOffset的值*/
      sqlite3VdbeAddOp1(v, OP_MustBeInt, iOffset);/*将  OP_MustBeInt交给虚拟机，然后返回这个操作的地址*/
      VdbeComment((v, "OFFSET counter"));/*在VDBE中存入 "OFFSET counter"，对应上句的地址*/
      addr1 = sqlite3VdbeAddOp1(v, OP_IfPos, iOffset);/*将OP_IfPost交给虚拟机，然后返回这个操作的地址,并赋值给 addr1*/
      sqlite3VdbeAddOp2(v, OP_Integer, 0, iOffset);/*将 OP_Integert交给虚拟机，然后返回这个操作的地址*/
      sqlite3VdbeJumpHere(v, addr1);/*运行地址跳到addr1*/
      sqlite3VdbeAddOp3(v, OP_Add, iLimit, iOffset, iOffset+1);/*将 OP_Add交给虚拟机，然后返回这个操作的地址*/
      VdbeComment((v, "LIMIT+OFFSET"));/*在VDBE中存入 "LIMIT+OFFSET"，对应上句的地址*/
      sqlite3VdbeAddOp2(v, OP_Integer, -1, iOffset+1);/*将 OP_Integer交给虚拟机，然后返回这个操作的地址*/
      addr1 = sqlite3VdbeAddOp1(v, OP_IfPos, iLimit);/*将OP_IfPost交给虚拟机，然后返回这个操作的地址,并赋值给 addr1*/
      sqlite3VdbeJumpHere(v, addr1);/*运行地址跳到addr1*/
    }
  }
}

#ifndef SQLITE_OMIT_COMPOUND_SELECT
/*
** Return the appropriate collating sequence for the iCol-th column of
** the result set for the compound-select statement "p".  Return NULL if
** the column has no default collating sequence.
** 为iCol-th列中针对复合查询语句"p"的结果集返回适当的排序序列.
** 如果列没有默认的排序序列，返回NULL。
**
** The collating sequence for the compound select is taken from the
** left-most term of the select that has a collating sequence.
** 复合查询的排序序列来自具有排序序列的最左边的查询。
**为符合查询指令p的结果集的第i列返回一个恰当的排序序列
**这个为符合查询的排序序列选自最左边有排序序列的条款。
*/
static CollSeq *multiSelectCollSeq(Parse *pParse, Select *p, int iCol){
  CollSeq *pRet;/*声明一个排序序列*/
  if( p->pPrior ){/*如果有优先查找*/
    pRet = multiSelectCollSeq(pParse, p->pPrior, iCol);/*调用函数自身，使用优先查找，返回一个排序序列*/
  }else{
    pRet = 0;/*否则排序序列为0*/
  }
  assert( iCol>=0 );/*插入断点，判断列号是否大于等于0*/
  if( pRet==0 && iCol<p->pEList->nExpr ){/*如果没有优先查找和列号小于表达式的个数*/
    pRet = sqlite3ExprCollSeq(pParse, p->pEList->a[iCol].pExpr);/*调用自身，返回一个默认的排序序列*/
  }
  return pRet;/*根据各种情况，返回最终的排序序列*/
}
#endif /* SQLITE_OMIT_COMPOUND_SELECT */

/* Forward reference 向前引用*/　
static int multiSelectOrderBy(

  Parse *pParse,        /* Parsing context 解析上下文*/
  Select *p,            /* The right-most of SELECTs to be coded SELECTs的最右边被编码*/
  SelectDest *pDest     /* What to do with query results 如何处理查询结果集*/
);

#ifndef SQLITE_OMIT_COMPOUND_SELECT
/*
** This routine is called to process a compound query form from
** two or more separate queries using UNION, UNION ALL, EXCEPT, or
** INTERSECT

**"p" points to the right-most of the two queries.  the query on the
** left is p->pPrior.  The left query could also be a compound query
** in which case this routine will be called recursively. 
** The results of the total query are to be written into a destination
** of type eDest with parameter iParm.
**Example 1:  Consider a three-way compound SQL statement.  例1:  考虑一个三向复合SQL语句。
**The arrows in the diagram above represent the Select.pPrior pointer.
** So if this routine is called with p equal to the t3 query, then
** pPrior will be the t2 query.  p->op will be TK_UNION in this case.
**Notice that because of the way SQLite parses compound SELECTs, the
** individual selects always group from left to right.
** 
** 调用这个程序来处理一个真正的两个的并集或交集查询或者两个以上单独的查询?
**“p”指向最右边的两个查询。左边的查询是p->pPrior.在递归地将调用这个程序的情况下，左边的查询也可以是复合查询。
** 总查询的结果将被写入一个带有参数iParm的eDest类型目标。
** 例1:  考虑一个三向复合SQL语句。
** SELECT a FROM t1 UNION SELECT b FROM t2 UNION SELECT c FROM t3
**This statement is parsed up as follows:    这句话是解析如下:
**
**     SELECT c FROM t3
**      |
**      `----->  SELECT b FROM t2
**                |
**                `------>  SELECT a FROM t1
**
** 上面图中的箭头代表Select.pPrior指针。所以如果以p等于t3查询来调用这个程序，然后pPrior将会是t2 查询。
** 这种情况下，p- >op将会是 TK_UNION
**
** Notice that because of the way SQLite parses compound SELECTs, the
** individual selects always group from left to right.
** 注意,因为SQLite解析复合查询语句的逻辑问题,单个查询总是从左到右。
××注意SQLite的解析复合查询的方式，这个独特的查询，总是从左到右。
*/
static int multiSelect(
	Parse *pParse,        /* Parsing context 解析的上下文 */
	Select *p,            /* The right-most of SELECTs to be coded 最右边的SELECTs将会被编码*//*最右边的SELECT结构体*/
	SelectDest *pDest     /* What to do with query results 如何处理查询结果*//*定义如何处理结果集*/
	){
	int rc = SQLITE_OK;   /* Success code from a subroutine 从子程序传递来的成功参数 *//*子程序成功执行*/
	Select *pPrior;       /* Another SELECT immediately to our left 另一个SELECT指向左边*//*一个在左边的优先立即查询的SELECT*/
	Vdbe *v;              /* Generate code to this VDBE 为这个VDBE生成代码*//*声明VDBE*/
	SelectDest dest;      /* Alternative data destination 可变动的数据目的地*///*供选择的目标数据集*/
	Select *pDelete = 0;  /* Chain of simple selects to delete 用来删除的简单查询链*//*简单的链式删除*/
	sqlite3 *db;          /* Database connection 连接数据库*/
#ifndef SQLITE_OMIT_EXPLAIN
	int iSub1;            /* EQP id of left-hand query     EQP id左侧的查询*//*左查询的id*/
	int iSub2;            /* EQP id of right-hand query    EQP id右侧的查询*//*右查询的id*/
#endif

	/* Make sure there is no ORDER BY or LIMIT clause on prior SELECTs.  Only
	** the last (right-most) SELECT in the series may have an ORDER BY or LIMIT.
	**确保之前的查询语句中没有任何ORDER BY 或者LIMIT。只有在序列中的最后一个(或最右)查询语句可能有ORDER BY或者LIMIT。
	*/
	assert(p && p->pPrior);  /* Calling function guarantees this much 插入断点判断是否有优先SELECT*/
	db = pParse->db;	/*将语法解析树中的数据库连接赋值给db*/
	pPrior = p->pPrior;	/*将SELECT中优先级赋值给pPrior*/
	assert(pPrior->pRightmost != pPrior);	/*插入断点，判断优先级的最右边表达式不是当前优先SELECT*/
	assert(pPrior->pRightmost == p->pRightmost);	/*插入断点，判断优先级的最右边表达式是当前优先SELECT的最右边SELECT*/
	dest = *pDest;	/*将如何处理结果集的结构体赋值给dest*/
	if (pPrior->pOrderBy){/*如果优先SELECT中含ORDERBY*/
		sqlite3ErrorMsg(pParse, "ORDER BY clause should come after %s not before",
			selectOpName(p->op));/*在语法解析树中添加一条报错信息*/
		rc = 1;/*做执行完毕标记*/
		goto multi_select_end;/*跳到multi_select_end（执行结束）*/
	}
	if (pPrior->pLimit){/*如果优先SELECT含LIMIT*/
		sqlite3ErrorMsg(pParse, "LIMIT clause should come after %s not before",
			selectOpName(p->op));/*在语法解析树中添加一条报错信息*/
		rc = 1;/*做执行完毕标记*/
		goto multi_select_end;/*跳到multi_select_end（执行结束）*/
	}

	v = sqlite3GetVdbe(pParse);	/*根据语法解析树生成一个虚拟数据库引擎*/
	assert(v != 0);  /* The VDBE already created by calling function   虚拟数据库引擎已经被函数创建*/

	/* Create the destination temporary table if necessary   如果需要，创建一个临时目标表*/
	if (dest.eDest == SRT_EphemTab){/*如果数据集是SRT_EphemTab*/
		assert(p->pEList);/*插入断点，判断是否有结果域*/
		sqlite3VdbeAddOp2(v, OP_OpenEphemeral, dest.iSDParm, p->pEList->nExpr);/*将OP_OpenEphemeral操作交给vdbe，然后返回这个操作的地址*/
		sqlite3VdbeChangeP5(v, BTREE_UNORDERED);/*把BTREE_UNORDERED设置为最近最多使用的值*/
		dest.eDest = SRT_Table;/*令数据集为SRT_Table*/
	}

	/* Make sure all SELECTs in the statement have the same number of elements
	** in their result sets.
	** 确定所有声明的SELECT有相同结果集元素的个数
	*/
	assert(p->pEList && pPrior->pEList);/*插入断点，SELECT结果域和优先SELECT的结果域有交集*/
	if (p->pEList->nExpr != pPrior->pEList->nExpr){/*如果结果域的表达式和优先SELECT的结果域的表达式不同*/
		if (p->selFlags & SF_Values){/*如果selFlags的标记为SF_Values*/
			sqlite3ErrorMsg(pParse, "all VALUES must have the same number of terms");/*在语法解析树中添加一条报错信息*/
		}
		else{
			sqlite3ErrorMsg(pParse, "SELECTs to the left and right of %s"
				" do not have the same number of result columns", selectOpName(p->op));/*否则在语法解析树中添加一条这样的信息其中%S为p->op*/
		}
		rc = 1;/*做执行完毕标记*/
		goto multi_select_end;/*跳到multi_select_end（执行结束）*/
	}

	/* Compound SELECTs that have an ORDER BY clause are handled separately.
	** 含有ORDERBY的复合SELECT要分开处理
	*/
	if (p->pOrderBy){/*如果含有ORDERBY*/
		return multiSelectOrderBy(pParse, p, pDest);/*递归使用本身，其实在递归p*/
	}

	/* Generate code for the left and right SELECT statements.
	** 为左SELECT和右SELECT声明编写代码
	*/
	switch (p->op){/*判断SELECT中操作符*/
	case TK_ALL: {
		int addr = 0;/*定义地址*/
		int nLimit;/*定义limit*/
		assert(!pPrior->pLimit);/*插入断点，如果优先SELECT中不含Limit*/
		pPrior->pLimit = p->pLimit;/*将SELECT中Limit赋值给优先SELECT的Limit*/
		pPrior->pOffset = p->pOffset;/*将SELECT中Offset赋值给优先SELECT的Offset*/
		explainSetInteger(iSub1, pParse->iNextSelectId);/*将下一个SELECT的ID赋值个iSub1*/
		rc = sqlite3Select(pParse, pPrior, &dest);/*做执行完毕标记,其中sqlite3Select根据dest生成的SELECT*/
		p->pLimit = 0;/*SELECT中没有LIMIT*/
		p->pOffset = 0;/*SELECT没有OFFSET*/
		if (rc){/*如果有结束标记*/
			goto multi_select_end;/*跳到multi_select_end（执行结束）*/
		}
		p->pPrior = 0;/*设置优先查询SELECT为0*/
		p->iLimit = pPrior->iLimit;/*优先查询SELECT中的LIMIT赋值给SELECT的LIMIT*/
		p->iOffset = pPrior->iOffset;/*优先查询SELECT中的OFFSET赋值给SELECT的OFFSET*/
		if (p->iLimit){/*如果SELECT中含LIMIT*/
			addr = sqlite3VdbeAddOp1(v, OP_IfZero, p->iLimit);/*将OP_Integer操作交给vdbe，然后返回这个操作的地址复制给addr*/
			VdbeComment((v, "Jump ahead if LIMIT reached"));/*运行地址跳到addr1*/
		}
		explainSetInteger(iSub2, pParse->iNextSelectId);/*将下一个SELECT的ID赋值个iSub2*/
		rc = sqlite3Select(pParse, p, &dest);/*做执行完毕标记,其中sqlite3Select根据dest生成的SELECT*/
		testcase(rc != SQLITE_OK);/*测试结束标记为SQLITE_OK（执行完毕）*/
		pDelete = p->pPrior;/*将SELECT中优先查询结构体赋值给pDelete删除SELECT结构*/
		p->pPrior = pPrior;/*将优先SELECT赋值给SELECT的优先查询查询属性*/
		p->nSelectRow += pPrior->nSelectRow;/*最右边的结构体等于优先SELECT的再加上当前的*/
		if (pPrior->pLimit
			&& sqlite3ExprIsInteger(pPrior->pLimit, &nLimit)
			&& p->nSelectRow > (double)nLimit/*如果优先查询SELECT含LIMIT，并最右边的SELECT结构体也含LIMIT*/
			){
			p->nSelectRow = (double)nLimit;/*SELECT的行数为nLimit(没想通啊，为什么呢)*/
		}
		if (addr){
			sqlite3VdbeJumpHere(v, addr);/*如果addr存在，运行地址跳到addr*/
		}
		break;
	}
	case TK_EXCEPT:
	case TK_UNION: {
		int unionTab;    /* Cursor number of the temporary table holding result   临时结果表中游标号*/
		u8 op = 0;       /* One of the SRT_ operations to apply to self        应用与自身的一个SRT_ operations操作*/
		int priorOp;     /* The SRT_ operation to apply to prior selects   应用于优先SELECT的SRT_ operations操作*/
		Expr *pLimit, *pOffset; /* Saved values of p->nLimit and p->nOffset    保存p->nLimit和p->nOffset的值*/
		int addr;	/*定义一个地址*/
		SelectDest uniondest;/*定义一个处理数据集的操作*/

		testcase(p->op == TK_EXCEPT);/*测试SELECT操作含TK_EXCEPT*/
		testcase(p->op == TK_UNION);/*测试SELECT操作含TK_UNION*/
		priorOp = SRT_Union;/*令优先查询的SELECT操作符为SRT_Union*/
		if (dest.eDest == priorOp && ALWAYS(!p->pLimit &&!p->pOffset)){/*数据集中操作符与优先查询的SELECT操作符相同，并且不含LIMIT OFFSET*/
			/* We can reuse a temporary table generated by a SELECT to our
			** right.
			**我们可以重用一个由右端查询语句生成的临时表.
			*/
			assert(p->pRightmost != p);  /* Can only happen for leftward elements
										 ** of a 3-way or more compound    只能在左侧元素是3种或以上复合的时候触发*/
			assert(p->pLimit == 0);      /* Not allowed on leftward elements 不允许在左侧的元素*/
			assert(p->pOffset == 0);     /* Not allowed on leftward elements 不允许在左侧的元素*/
			unionTab = dest.iSDParm;/*将用来处理数据集的方法属性赋值给unionTab*/
		}
		else{
			/* We will need to create our own temporary table to hold the
			** intermediate results.   我们需要创建自己的临时表来保存中间结果。
			*/
			unionTab = pParse->nTab++;/*插入断点判断是否含有ORDERBY*/
			assert(p->pOrderBy == 0);/*插入断点判断是否含有ORDERBY*/
			addr = sqlite3VdbeAddOp2(v, OP_OpenEphemeral, unionTab, 0);/*将OP_OpenEphemeral操作交给vdbe，然后返回这个操作的地址再赋值给addr*/
			assert(p->addrOpenEphm[0] == -1);/*插入断点，没有新打开路径*/
			p->addrOpenEphm[0] = addr;/*将地址赋值给打开路径的第一个元素*/
			p->pRightmost->selFlags |= SF_UsesEphemeral;/*selFlags位或SF_UsesEphemeral*/
			assert(p->pEList);/*插入断点，判断SELECT中结果域*/
		}

		/* Code the SELECT statements to our left */ 
		/*为左边的子树编写SELECT声明*/
		assert(!pPrior->pOrderBy);/*插入断点，判断优先查询的SELECT的Orderby*/
		sqlite3SelectDestInit(&uniondest, priorOp, unionTab);/*初始化SelectDest结构体*/
		explainSetInteger(iSub1, pParse->iNextSelectId);/*把语法分析树的pParse的下一个SelectID赋值给ISub1*/
		rc = sqlite3Select(pParse, pPrior, &uniondest);/*生成Select结构体，然后把返回标记给rc，是否成功*/
		if (rc){/*如果rc结束标记存在*/
			goto multi_select_end;/*跳到multi_select_end（执行结束）*/
		}

		/* Code the current SELECT statement   /
		/* 生成当前SELECT语句编码*/
	
		if (p->op == TK_EXCEPT){/*如果结构体的操作符是“TK_EXCEPT”*/
			op = SRT_Except;/*设置操作符为SRT_Except*/
		}
		else{
			assert(p->op == TK_UNION);/*插入断点，判断SELECT结构体的操作符是否为“TK_UNION”*/
			op = SRT_Union;/*设置操作符为SRT_Union*/
		}
		p->pPrior = 0;/*将SELECT结构体中的优先查询置0*/
		pLimit = p->pLimit;/*将当前SELECT的左子树设置为当前SELECT结构体，递归的过程，遍历左子树了。*/
		p->pLimit = 0;/*将SELECT结构体中limit置0*/
		pOffset = p->pOffset;/*将SELECT结构体中的Offset偏移量复制给变量pOffset*/
		p->pOffset = 0;/*再将SELECT结构体中的Offset偏移量置0，上一条指令已经拷贝走Offset的值。*/
		uniondest.eDest = op;/*将op操作符赋值给处理结果集中的“处理方式”属性*/
		explainSetInteger(iSub2, pParse->iNextSelectId);/*把语法分析树的pParse的下一个SelectID赋值给iSub2*/
		rc = sqlite3Select(pParse, p, &uniondest);/*生成Select结构体，然后把返回标记给rc，是否成功*/
		testcase(rc != SQLITE_OK);/*测试结果标记的值是否为完成*/
		/* Query flattening in sqlite3Select() might refill p->pOrderBy.
		** Be sure to delete p->pOrderBy, therefore, to avoid a memory leak.
		**在sqlite3Select()中的扁平化处理可能会重新填充p->pOrderBy.
		** 确保要删除p - > pOrderBy,如此这样,才能避免内存泄漏。
		*/
		sqlite3ExprListDelete(db, p->pOrderBy);/*删除数据库连接中的ORDERBY*/
		pDelete = p->pPrior;/*将SELECT中优先查询结构体赋值给pDelete删除SELECT结构*/
		p->pPrior = pPrior;/*将变量中的pPrior优先值赋值给SELECT结构体中的优先属性*/
		p->pOrderBy = 0;/*将SELECT中的ORDERBY置0，使用默认方法排序*/
		if (p->op == TK_UNION) p->nSelectRow += pPrior->nSelectRow;
		/*如果SELECT中的操作为TK_UNION，将当前pPrior优先查找中的查找行加上SELECT的查找行。个人以为：节点中的值加上子节点中属性的值，才要查到的行，类似基值加偏移量*/
		sqlite3ExprDelete(db, p->pLimit);/*删除数据库连接中的，SELECT中的Limit表达式*/
		p->pLimit = pLimit;/*再将当前limit中的变量赋值给SELECT的limit变量，刚才在上面置0了。*/
		p->pOffset = pOffset;/*再将当前Offset中的变量赋值给SELECT的Offset变量，刚才在上面也置0了。*/
		p->iLimit = 0;/*再将SELECT的limit变量置0*/
		p->iOffset = 0;/*将SELECT的Offset变量置0*/

		/* Convert the data in the temporary table into whatever form
		** it is that we currently need.
		**将临时表中的任何数据转换成我们目前数据。
		*/
		assert(unionTab == dest.iSDParm || dest.eDest != priorOp);/*插入断点，操作参数是否等于游标号或怎样操作是否等于操作符。*/
		if (dest.eDest != priorOp){/*怎样操作不等于操作符*/
			int iCont, iBreak, iStart;
			assert(p->pEList);/*插入断点，判断结果域是否为空*/
			if (dest.eDest == SRT_Output){/*如果操作的方式为输出*/
				Select *pFirst = p;/*将复合SELECT中的一个SELECT设置为p*/
				while (pFirst->pPrior) pFirst = pFirst->pPrior;/*递归，找到复合SELECT中第一个SELECET，并赋值给pFrist*/
				generateColumnNames(pParse, 0, pFirst->pEList);/*根据语法分析树，表编号和列表达式列表生成列名*/
			}
			iBreak = sqlite3VdbeMakeLabel(v);/*为VDBE创建一个标签，返回值赋值给iBreak。注：这个标签是个负值。所有的标签都是负值。0的话就返回分配内存错误*/
			iCont = sqlite3VdbeMakeLabel(v);/*为VDBE创建一个标签，返回值赋值给iCont。*/
			computeLimitRegisters(pParse, p, iBreak);/*根据返回的的专用地址，计算出limit结果域*/
			sqlite3VdbeAddOp2(v, OP_Rewind, unionTab, iBreak);/*将OP_Rewind操作交给vdbe，然后返回这个操作的地址*/
			iStart = sqlite3VdbeCurrentAddr(v);/*将vdbe的下一条指令地址赋值给iStart*/
			selectInnerLoop(pParse, p, p->pEList, unionTab, p->pEList->nExpr,
				0, -1, &dest, iCont, iBreak);/*根据表达式p->pEList与p->pEList->nExpr建立内连接*/
			sqlite3VdbeResolveLabel(v, iCont);/*解析ICont，此时ICont是一个标签，并作为下一条被插入的地址*/
			sqlite3VdbeAddOp2(v, OP_Next, unionTab, iStart);/*将OP_Next操作交给vdbe，然后返回这个操作的地址*/
			sqlite3VdbeResolveLabel(v, iBreak);/*解析iBreak，此时iBreak也是一个标签，代表的是地址。并作为下一条被插入的地址*/
			sqlite3VdbeAddOp2(v, OP_Close, unionTab, 0);/*将OP_Close操作交给vdbe，然后返回这个操作的地址*/
		}
		break;
	}
	default: assert(p->op == TK_INTERSECT); {/*插入一个断点，判断SELECT中的操作是否为TK_INTERSECT*/
		int tab1, tab2;/*声明两个表*/
		int iCont, iBreak, iStart;/*声明3个标签，存地址*/
		Expr *pLimit, *pOffset;/*声明两个表达式*/
		int addr;/*声明一个地址标签*/
		SelectDest intersectdest;/*声明一个处理的结果的结构体*/
		int r1;

		/* INTERSECT is different from the others since it requires
		** two temporary tables.  Hence it has its own case.  Begin
		** by allocating the tables we will need.

		**INTERSECT与其他的不同因为它需要两个临时表.因此它有自己的案例。
		**从分配我们需要的表开始
		*/
		tab1 = pParse->nTab++;/*将语法分析树中表的个数加一赋值给tab1*/
		tab2 = pParse->nTab++;/*将语法分析树中表的个数加一赋值给tab2*/
		assert(p->pOrderBy == 0);/*插入断点，判断SELECT中是否有ORDERBY子句*/

		addr = sqlite3VdbeAddOp2(v, OP_OpenEphemeral, tab1, 0);/*将OP_OpenEphemeral操作交给vdbe，然后返回这个操作的地址*/
		assert(p->addrOpenEphm[0] == -1);/*插入断点，判断SELECT结构体OP_OpenEphem操作数组第一个元素是否为-1*/
		p->addrOpenEphm[0] = addr;/*将变量addr的地址赋值给OP_OpenEphem操作数组的第一个元素*/
		p->pRightmost->selFlags |= SF_UsesEphemeral;/*SELECT最右子树的selFlags与SF_UsesEphemeral（使用临时目录节点）位或，在赋值给selFlags*/
		assert(p->pEList);/*插入断点，判断SELECT的结果域是否为空*/

		/* Code the SELECTs to our left into temporary table "tab1".  */
		/* 编写左边的SELECT到临时表"tab1"中*/
		sqlite3SelectDestInit(&intersectdest, SRT_Union, tab1);/*初始化处理结果集*/
		explainSetInteger(iSub1, pParse->iNextSelectId);/*把语法分析树的pParse的下一个SelectID赋值给iSub1*/
		rc = sqlite3Select(pParse, pPrior, &intersectdest);/*根据语法解析树，SELECT结构体和处理结果集结构体生成SELECT语法树，并返回一个结束标记*/
		if (rc){/*如果结束标记存在*/
			goto multi_select_end;/*跳到multi_select_end（执行结束）*/
		}

		/* Code the current SELECT into temporary table "tab2"  */
		/*  编写当前SELECT到临时表"tab2"中*/
		addr = sqlite3VdbeAddOp2(v, OP_OpenEphemeral, tab2, 0);/*将OP_OpenEphemeral操作交给vdbe，然后返回这个操作的地址*/
		assert(p->addrOpenEphm[1] == -1);/*插入断点，判断SELECT结构体OP_OpenEphem操作数组第二个元素是否为-1*/
		p->addrOpenEphm[1] = addr;/*将变量addr的地址赋值给OP_OpenEphem操作数组的第二个元素*/
		p->pPrior = 0;/*将SELECT结构体中优先查询SELECT置为0*/
		pLimit = p->pLimit;/*再将当前limit中的变量赋值给SELECT的limit变量*/
		p->pLimit = 0;/*再将当前SELECT的limit变量值0*/
		pOffset = p->pOffset;/*再将当前Offset中的变量赋值给SELECT的Offset变量*/
		p->pOffset = 0;/*再将当前SELECT的Offset变量值0*/
		intersectdest.iSDParm = tab2;/*将处理结果集结构体中处理表的参数置为tab2*/
		explainSetInteger(iSub2, pParse->iNextSelectId);/*把语法分析树的pParse的下一个SelectID赋值给iSub2*/
		rc = sqlite3Select(pParse, p, &intersectdest);/*根据语法解析树，SELECT结构体和处理结果集结构体生成SELECT语法树，并返回一个结束标记*/
		testcase(rc != SQLITE_OK);/*测试结束标记是否为SQLITE_OK*/
		pDelete = p->pPrior;/*将SELECT结构体中优先查询的SELECT赋值给待删除的SELECT*/
		p->pPrior = pPrior;/*将当前优先查询的SELECT变量赋值给SELECT中优先查询的pPrior属性*/
		if (p->nSelectRow > pPrior->nSelectRow) p->nSelectRow = pPrior->nSelectRow;
		/*如果SELECT中的查找行大于优先查询SELECT的查找行，将优先查询SELECT的查找行赋值给SELECT*/
		sqlite3ExprDelete(db, p->pLimit);/*删除数据库连接中的limit表达式*/
		p->pLimit = pLimit;/*将当前limit中的变量赋值给SELECT的limit变量/
		p->pOffset = pOffset;/*将当前Offset中的变量赋值给SELECT的Offset变量*/

		/* Generate code to take the intersection of the two temporary
		** tables.   为两个临时表的交叉口进行编码
		*/
		assert(p->pEList);/*插入断点，判断SELECT中结果域是否为空*/
		if (dest.eDest == SRT_Output){/*如果处理结果集的处理方式为SRT_Output（输出）*/
			Select *pFirst = p;/*声明结构体pFirst，赋值为p*/
			while (pFirst->pPrior) pFirst = pFirst->pPrior;/*一直递归，找到叶子节点上的最优先的SELECT*/
			generateColumnNames(pParse, 0, pFirst->pEList);/*生成列名*/
		}
		iBreak = sqlite3VdbeMakeLabel(v);/*为VDBE创建一个标签，返回值赋值给iBreak.*/
		iCont = sqlite3VdbeMakeLabel(v);/*为VDBE创建一个标签，返回值赋值给iCont*/
		computeLimitRegisters(pParse, p, iBreak);/*根据返回的的专用地址，计算出limit结果域*/
		sqlite3VdbeAddOp2(v, OP_Rewind, tab1, iBreak);/*将OP_Rewind操作交给vdbe，然后返回这个操作的地址*/
		r1 = sqlite3GetTempReg(pParse);/*得到语法解析树中临时寄存器中的值给r1*/
		iStart = sqlite3VdbeAddOp2(v, OP_RowKey, tab1, r1);/*将OP_Rewind操作交给vdbe，然后返回这个操作的地址*/
		sqlite3VdbeAddOp4Int(v, OP_NotFound, tab2, iCont, r1, 0);/*把OP_NotFound（未查到）操作的值看成整数，然后添加这个操作符到虚拟机中*/
		sqlite3ReleaseTempReg(pParse, r1);/*释放语法解析树中临时寄存器r1的值*/
		selectInnerLoop(pParse, p, p->pEList, tab1, p->pEList->nExpr,
			0, -1, &dest, iCont, iBreak);/*根据表达式p->pEList与p->pEList->nExpr建立内连接*/
		sqlite3VdbeResolveLabel(v, iCont);/*解析ICont，iCont是一个标签，并作为下一条被插入的地址*/
		sqlite3VdbeAddOp2(v, OP_Next, tab1, iStart);/*将OP_Next操作交给vdbe，然后返回这个操作的地址*/
		sqlite3VdbeResolveLabel(v, iBreak);/*解析ICont，iBreak是一个标签，并作为下一条被插入的地址*/
		sqlite3VdbeAddOp2(v, OP_Close, tab2, 0);/*将OP_Close操作交给vdbe，然后返回这个操作的地址*/
		sqlite3VdbeAddOp2(v, OP_Close, tab1, 0);/*将OP_Close操作交给vdbe，然后返回这个操作的地址*/
		break;
	}
	}

	explainComposite(pParse, p->op, iSub1, iSub2, p->op != TK_ALL);
  /*如果没有执行select，那么就会输出"COMPOSITE SUBQUERIES iSub1 and iSub2 (op)"*/
	/* Compute collating sequences used by
	** temporary tables needed to implement the compound select.
	** Attach the KeyInfo structure to all temporary tables.
	**临时表所使用的计算排序序列需要实现复合查询。把KeyInfo结构附加到所有临时表。
** 注意,因为这种方式的SQLite是解析复合选择,单个选择总是从左到右。
*/
static int multiSelect(
  Parse *pParse,        /* Parsing context 解析的上下文 */
  Select *p,            /* The right-most of SELECTs to be coded 最右边的SELECTs将会被编码*/
  SelectDest *pDest     /* What to do with query results 如何处理查询结果*/
){
  int rc = SQLITE_OK;   /* Success code from a subroutine 来自子程序的成功编码 */
  Select *pPrior;       /* Another SELECT immediately to our left *//*左边的查询优先执行*/
  Vdbe *v;              /* Generate code to this VDBE 这个VDBE生成代码*/
  SelectDest dest;      /* Alternative data destination *//*供选择的目标数据集*/
  Select *pDelete = 0;  /* Chain of simple selects to delete*//*简单查询的链式删除*/
  sqlite3 *db;          /* Database connection 数据库连接*/
#ifndef SQLITE_OMIT_EXPLAIN
  int iSub1;            /* EQP id of left-hand query    *//*左查询的id*/
  int iSub2;            /* EQP id of right-hand query    *//*右查询的id*/
#endif

  /* Make sure there is no ORDER BY or LIMIT clause on prior SELECTs.  Only
  ** the last (right-most) SELECT in the series may have an ORDER BY or LIMIT.
  **确保没有任何ORDER BY 或者LIMIT子句在prior SELECTs中。只有在系列中的最后一个(最右)SELECT可能有ORDER BY或者LIMIT。
  */
  assert( p && p->pPrior );  /* Calling function guarantees this much *//*插入断点，判断是否有优先的查询*/
  db = pParse->db;/*将语法解析树中的数据库连接赋值给db*/
  pPrior = p->pPrior;/*将SELECT中优先级赋值给  pPrior*/
  assert( pPrior->pRightmost!=pPrior );/*插入断点判断优先级最右边表达式不是当前优先查询*/
  assert( pPrior->pRightmost==p->pRightmost );
  dest = *pDest;/*将结果集的结构体赋值给dest*/
  if( pPrior->pOrderBy ){/*如果优先查询中含有OrderBy */
    sqlite3ErrorMsg(pParse,"ORDER BY clause should come after %s not before",/*在语法解析树中添加一条报错信息*/
      selectOpName(p->op));
    rc = 1;/*做执行完毕标记*/
    goto multi_select_end;/*跳到multi_select_end执行完毕*/
  }
  if( pPrior->pLimit ){/*如果优先查询中含有Limit  */
    sqlite3ErrorMsg(pParse,"LIMIT clause should come after %s not before",
      selectOpName(p->op));/*在语法解析树中添加一条报错信息*/
    rc = 1;/*做执行完毕标记*/
    goto multi_select_end;/*跳到multi_select_end执行完毕*/
  }

  v = sqlite3GetVdbe(pParse);/*根据语法解析树生成一个虚拟数据库引擎*/
  assert( v!=0 );  /* The VDBE already created by calling function    VDBE已经由调用函数创建*/

  /* Create the destination temporary table if necessary   在必要时创建目标临时表
  */
  if( dest.eDest==SRT_EphemTab ){/*如果数据集是SRT_EphemTab*/
    assert( p->pEList );/*插入断点，判断是否有结果域*/
    sqlite3VdbeAddOp2(v, OP_OpenEphemeral, dest.iSDParm, p->pEList->nExpr);/*将  OP_OpenEphemeral交给虚拟机，然后返回这个操作的地址*/
    sqlite3VdbeChangeP5(v, BTREE_UNORDERED);/*把BTREE_UNORDERED设置为最近最多使用的值*/
    dest.eDest = SRT_Table;/*令数据集为SRT_Table*/
  }

  /* Make sure all SELECTs in the statement have the same number of elements
  ** in their result sets.
  ** 确保所有在声明中SELECTs在他们的结果集中具有相同数量的元素。
  */
  assert( p->pEList && pPrior->pEList );/*插入断点，p的结果域和优先结果域的交集不为空*/
  if( p->pEList->nExpr!=pPrior->pEList->nExpr ){/*如果p结果域表达式和优先结果域表达式不相同*/
    if( p->selFlags & SF_Values ){/*如果selFlags的标记为SF_Values*/
      sqlite3ErrorMsg(pParse, "all VALUES must have the same number of terms");/*在语法解析树中添加一条报错信息*/
    }else{
      sqlite3ErrorMsg(pParse, "SELECTs to the left and right of %s"
        " do not have the same number of result columns", selectOpName(p->op));/*否则在语法解析树中添加这条信息，其中 %s为p->op*/
    }
    rc = 1;/*做执行完毕标记*/
    goto multi_select_end;/*跳到multi_select_end执行完毕*/
  }

  /* Compound SELECTs that have an ORDER BY clause are handled separately.
  ** 有ORDER BY子句的复合的SELECTs被分别处理。
  */
  if( p->pOrderBy ){/*如果含有pOrderBy*/
    return multiSelectOrderBy(pParse, p, pDest);/*递归调用自身*/
  }

  /* Generate code for the left and right SELECT statements.
  **为左边和右边的SELECT语句生成代码。
  */
  switch( p->op ){
    case TK_ALL: {
      int addr = 0;/*定义地址*/
      int nLimit;/*定义Limit*/
      assert( !pPrior->pLimit );/*插入断点，如果优先中不含Limit*/
      pPrior->pLimit = p->pLimit;/*将SELECT中的Limit赋值给优先查询的pLimit*/
      pPrior->pOffset = p->pOffset;/*将select中的pOffset赋值给优先查询的pOffset*/
      explainSetInteger(iSub1, pParse->iNextSelectId);/*将下一个Select的id赋值给iSub1*/
      rc = sqlite3Select(pParse, pPrior, &dest);/*做执行完毕标记，其中sqlite3Select根据dest生成select*/
      p->pLimit = 0;/*select中没有LIMIT*/
      p->pOffset = 0;/*select中没有Offset*/
      if( rc ){/*如果rc有结束标记*/
        goto multi_select_end;/*跳到multi_select_end执行完毕*/
      }
      p->pPrior = 0;/*设置优先查询为0*/
      p->iLimit = pPrior->iLimit;/*将优先查询中的Limit赋值给select中的Limit*/
      p->iOffset = pPrior->iOffset;/*将优先查询中的Offset赋值给select中的Offset*/
      if( p->iLimit ){/*如果SELECT中含有LIMIT*/
        addr = sqlite3VdbeAddOp1(v, OP_IfZero, p->iLimit);/*将  OP_OpenEphemeral交给虚拟机，然后返回这个操作的地址,并赋值给  addr*/
        VdbeComment((v, "Jump ahead if LIMIT reached"));/*运行地址跳转到addr*/
      }
      explainSetInteger(iSub2, pParse->iNextSelectId);/*将下一个SELECT的ID赋值给iSub2*/
      rc = sqlite3Select(pParse, p, &dest);/*做执行完毕标记，其中sqlite3Select根据dest生成的SELECT*/
      testcase( rc!=SQLITE_OK );/*做测试结束标记为SQLITE_OK*/
      pDelete = p->pPrior;/*将SELECT中优先查询结构体赋值给pDELETE删除SELECT结构*/
      p->pPrior = pPrior;/*将优先SELELT赋值给select的优先查询属性*/
      p->nSelectRow += pPrior->nSelectRow;/*最右边的结构体等于优先SELECT的再加上当前的*/
      if( pPrior->pLimit
       && sqlite3ExprIsInteger(pPrior->pLimit, &nLimit)
       && p->nSelectRow > (double)nLimit /*如果优先查询含有Limit，并且最右边的SELECT结构体也含有Limit*/
      ){
        p->nSelectRow = (double)nLimit;/*SELECT的行数为nLimit*/
      }
      if( addr ){
        sqlite3VdbeJumpHere(v, addr);/*如果addr存在，运行地址跳转到addr*/
      }
      break;
    }
    case TK_EXCEPT:
    case TK_UNION: {
      int unionTab;    /* Cursor number of the temporary table holding result *//*临时结果表中游标号*/
      u8 op = 0;       /* One of the SRT_ operations to apply to self       *//*应用与自身的一个SRT_ operations操作*/
      int priorOp;     /* The SRT_ operation to apply to prior selects   这个SRT_ operation 指定给prior selects*/
      Expr *pLimit, *pOffset; /* Saved values of p->nLimit and p->nOffset    保存p->nLimit和p->nOffset的值*/
      int addr;/*定义一个地址*/
      SelectDest uniondest;/*定义一个处理结果集的操作*/

      testcase( p->op==TK_EXCEPT );/*测试SELECT中复合查询的关系操作符是否是EXCEPT*/
      testcase( p->op==TK_UNION );/*测试SELECT中复合查询的关系操作符是否是UNION*/
      priorOp = SRT_Union;/*优先查询的操作符为Union*/
      if( dest.eDest==priorOp && ALWAYS(!p->pLimit &&!p->pOffset) ){/*数据集中操作符与优先查询的操作符相同，并且不含Limit和Offset*/
        /* We can reuse a temporary table generated by a SELECT to our
        ** right.
        **我们可以由右边的SELECT重新使用一个临时表
        */
        assert( p->pRightmost!=p );  /* Can only happen for leftward elements   
                                     ** of a 3-way or more compound    左侧的元素只能发生3种方式或者以上复合*/
        assert( p->pLimit==0 );      /* Not allowed on leftward elements 不允许在左侧的元素*/
        assert( p->pOffset==0 );     /* Not allowed on leftward elements 不允许在左侧的元素*/
        unionTab = dest.iSDParm;/*用来处理结果集的方法属性赋值给unionTab */
      }else{
        /* We will need to create our own temporary table to hold the
        ** intermediate results.   我们需要创建自己的临时表来保存中间结果。
        */
        unionTab = pParse->nTab++;/*将语法分析树中的表赋值给 unionTab*/
        assert( p->pOrderBy==0 );/*插入断点判断OrderBy是否为0*/
        addr = sqlite3VdbeAddOp2(v, OP_OpenEphemeral, unionTab, 0);/*将OP_OpenEphemeral交给虚拟机，然后返回这个操作的地址再赋值给addr*/
        assert( p->addrOpenEphm[0] == -1 );/*插入断点，新路径没有被打开*/
        p->addrOpenEphm[0] = addr;/*将地址赋值给打开路径的第一个元素*/
        p->pRightmost->selFlags |= SF_UsesEphemeral;
        assert( p->pEList );/*插入断点，判断查询中的结果域*/
      }

      /* Code the SELECT statements to our left  给左边的SELECT语句编码  
      */
      assert( !pPrior->pOrderBy );/*插入断点，判断优先查询的OrderBy*/
      sqlite3SelectDestInit(&uniondest, priorOp, unionTab);/*初始化SelectDest结构体*/
      explainSetInteger(iSub1, pParse->iNextSelectId);/*把语法分析树的Parse的下一个查询的id赋值给id*/
      rc = sqlite3Select(pParse, pPrior, &uniondest);/*生成SELECT结构体，然后把返回标记给 rc */
      if( rc ){/*如果rc结束标记存在*/
        goto multi_select_end;/*跳转到multi_select_end*/
      }

      /* Code the current SELECT statement    给当前SELECT语句编码 
      */
      if( p->op==TK_EXCEPT ){/*如果结构体的操作符是TK_EXCEPT*/
        op = SRT_Except;/*设置操作符为SRT_Except*/
      }else{
        assert( p->op==TK_UNION );/*插入断点，判断查询结构体的操作符是否是UNION*/
        op = SRT_Union;/*设置操作符为Union*/
      }
      p->pPrior = 0;/*将查询结构体中的优先查询置为0*/
      pLimit = p->pLimit;
      p->pLimit = 0;/*将查询结构体中Limit置为0*/
      pOffset = p->pOffset;
      p->pOffset = 0;/*将查询结构体中Offset置为0*/
      uniondest.eDest = op;/*将op操作符赋值给处理结果集中的属性*/
      explainSetInteger(iSub2, pParse->iNextSelectId);/*把语法分析树的Parse的下一个查询的id赋值给id*/
      rc = sqlite3Select(pParse, p, &uniondest);/*生成SELECT结构体，然后把返回标记给 rc */
      testcase( rc!=SQLITE_OK );
      /* Query flattening in sqlite3Select() might refill p->pOrderBy.
      ** Be sure to delete p->pOrderBy, therefore, to avoid a memory leak.
      ** 确保要删除p - > pOrderBy,如此这样,才能避免内存泄漏。
      */
      sqlite3ExprListDelete(db, p->pOrderBy);/*删除数据库连接中的OrderBy*/
      pDelete = p->pPrior;/*将查询中优先查询结构体赋值给 pDelete*/
      p->pPrior = pPrior;/*将变量中的pPrior优先值赋值给查询结构体中的优先属性*/
      p->pOrderBy = 0;/*将查询中的OrderBy置为0，使用默认的排序方式*/
      if( p->op==TK_UNION ) p->nSelectRow += pPrior->nSelectRow;/*如果查询中的操作为UNION，则查找行为优先查找中的查找行加上查询中的查找行*/
      sqlite3ExprDelete(db, p->pLimit);/*删除SELECT中的LIMIT表达式*/
      p->pLimit = pLimit;/*把当前limit中的变量赋值给查询中的limit变量*/
      p->pOffset = pOffset;/*把当前Offset中的变量赋值给查询中的Offset变量*/
      p->iLimit = 0;/*再将查询中的变量iLimit置为0*/
      p->iOffset = 0;

      /* Convert the data in the temporary table into whatever form
      ** it is that we currently need.
      **将临时表中的数据转换成任何我们目前所需要的形式那就是。
      */
      assert( unionTab==dest.iSDParm || dest.eDest!=priorOp );
      if( dest.eDest!=priorOp ){
        int iCont, iBreak, iStart;
        assert( p->pEList );/*插入断点，判断结果域是否为空*/
        if( dest.eDest==SRT_Output ){/*如果操作的方式为输出*/
          Select *pFirst = p;/*将复合查询中的一个查询置为p*/
          while( pFirst->pPrior ) pFirst = pFirst->pPrior;/*利用循环找到复合查询中的第一个查询，并赋值给pFirst*/
          generateColumnNames(pParse, 0, pFirst->pEList);/*根据语法分析树，表编号和列表达式生成列名*/
        }
        iBreak = sqlite3VdbeMakeLabel(v);/*为VDBE创建一个标签，返回值赋给  iBreak*/
        iCont = sqlite3VdbeMakeLabel(v);/*为VDBE创建一个标签，返回值赋给   iCont*/
        computeLimitRegisters(pParse, p, iBreak);/*根据返回的专用地址，计算出Limit*/
        sqlite3VdbeAddOp2(v, OP_Rewind, unionTab, iBreak);/*将 OP_Rewind操作赋值给VDBE，然后返回这个操作的地址*/
        iStart = sqlite3VdbeCurrentAddr(v);/*将VDBE的下一条指令地址赋值给 iStart*/
        selectInnerLoop(pParse, p, p->pEList, unionTab, p->pEList->nExpr,
                        0, -1, &dest, iCont, iBreak);/*根据表达式 p->pEList和 p->pEList->nExpr建立内连接*/
        sqlite3VdbeResolveLabel(v, iCont);/*解析iCont，此时iCont是一个标签，把它作为下一条被被插入的地址*/
        sqlite3VdbeAddOp2(v, OP_Next, unionTab, iStart);/*将 OP_Next操作赋值给VDBE，然后返回这个操作的地址*/
        sqlite3VdbeResolveLabel(v, iBreak);/*解析iBreak，此时iBreak是一个标签，把它作为下一条被被插入的地址*/
        sqlite3VdbeAddOp2(v, OP_Close, unionTab, 0);/*将OP_Close操作赋值给VDBE，然后返回这个操作的地址*/
      }
      break;
    }
    default: assert( p->op==TK_INTERSECT ); {/*插入断点，判断SELECT中的操作符是否为INTERSECT*/
      int tab1, tab2;/*声明2个表*/
      int iCont, iBreak, iStart;/*声明3个标签，存地址*/
      Expr *pLimit, *pOffset;/*声明2个表达式*/
      int addr;
      SelectDest intersectdest;/*声明一个处理结果的结构体*/
      int r1;

      /* INTERSECT is different from the others since it requires
      ** two temporary tables.  Hence it has its own case.  Begin
      ** by allocating the tables we will need.
      **由于INTERSECT需要两个临时表，所以它是不同于其它的,要根据自己的情况分配需要的表
      **
      */
      tab1 = pParse->nTab++;/*将语法解析树中的表赋值给tab1*/
      tab2 = pParse->nTab++;
      assert( p->pOrderBy==0 );/*插入断点，判断查询中是否有OrderBy子句*/

      addr = sqlite3VdbeAddOp2(v, OP_OpenEphemeral, tab1, 0);/*将 OP_OpenEphemeral操作赋值给VDBE，然后返回这个操作的地址,并赋值给  addr*/
      assert( p->addrOpenEphm[0] == -1 );/*插入断点，判断查询结构体中addrOpenEphm操作数组第一个元素是否为-1*/
      p->addrOpenEphm[0] = addr;/*将变量地址赋值给OP_OpenEphemeral操作数组的第一个元素*/
      p->pRightmost->selFlags |= SF_UsesEphemeral;
      assert( p->pEList );/*插入断点，判断SELECT的结果域是否为空*/

      /* Code the SELECTs to our left into temporary table "tab1".   把我们左边的SELECTs 编码到临时表"tab1"中 
      */
      sqlite3SelectDestInit(&intersectdest, SRT_Union, tab1);/*初始化处理结果集*/
      explainSetInteger(iSub1, pParse->iNextSelectId);/*把语法分析树的下一个查询ID赋值给iSub1*/
      rc = sqlite3Select(pParse, pPrior, &intersectdest);/*根据语法解析树，SELECT结构体和处理结果集结构体生成SELECT语法树，并返回一个结束标记*/
      if( rc ){/*如果rc这个结束标记存在*/
        goto multi_select_end;/*跳转到multi_select_end*/
      }

      /* Code the current SELECT into temporary table "tab2"    把当前的SELECT编码到临时表"tab2"中 
      */
      addr = sqlite3VdbeAddOp2(v, OP_OpenEphemeral, tab2, 0);/*将 OP_OpenEphemeral操作赋值给VDBE，然后返回这个操作的地址,并赋值给  addr*/
      assert( p->addrOpenEphm[1] == -1 );/*插入断点，判断SELECT结构体 OP_OpenEphemeral操作数组第二个元素是否为-1*/
      p->addrOpenEphm[1] = addr;/*将变量addr的地址赋值给 OP_OpenEphemeral操作数组的第二个元素*/
      p->pPrior = 0;/*将SELECT结构体中优先查询置为0*/
      pLimit = p->pLimit;/*将当前limit中的变量赋值给SELECT的limit变量*/
      p->pLimit = 0;/*将当前limit变量置为0*/
      pOffset = p->pOffset;/*将当前Offset中的变量赋值给SELECT的Offset变量*/
      p->pOffset = 0;/*将当前SELECT的Offset变量置为0*/
      intersectdest.iSDParm = tab2;/*将处理结果集结构体中处理表的参数置为tab2*/
      explainSetInteger(iSub2, pParse->iNextSelectId);/*把语法分析树的下一个查询ID赋值给iSub2*/
      rc = sqlite3Select(pParse, p, &intersectdest);/*根据语法解析树，SELECT结构体和处理结果集结构体生成SELECT语法树，并返回一个结束标记*/
      testcase( rc!=SQLITE_OK );/*测试结束标记是否为SQLITE_OK */
      pDelete = p->pPrior;/*将SELECT结构体中优先查询的SELECT赋值给待删除的SELECT*/
      p->pPrior = pPrior;/*将当前优先查询的SELECT变量赋值给SELECT中优先查询的pPrior*/
      if( p->nSelectRow>pPrior->nSelectRow ) p->nSelectRow = pPrior->nSelectRow;/*如果SELECT中的查找行大于优先查询的查找行，将优先查询的查找行赋值给SELECT*/
      sqlite3ExprDelete(db, p->pLimit);/*删除数据库连接中的limit表达式*/
      p->pLimit = pLimit;/*将当前limit中 的变量赋值给SELECT的limit变量*/
      p->pOffset = pOffset;

      /* Generate code to take the intersection of the two temporary
      ** tables.   给两个临时表的交集生成代码。 
      */
      assert( p->pEList );/*插入断点，判断SELECT中的Limit是否为空*/
      if( dest.eDest==SRT_Output ){/*如果结果集的处理方式为SRT_Output*/
        Select *pFirst = p;/*声明结构体，赋值为p*/
        while( pFirst->pPrior ) pFirst = pFirst->pPrior;/*循环，找到语法树中最优先的SELECT*/
        generateColumnNames(pParse, 0, pFirst->pEList);/*生成列名*/
      }
      iBreak = sqlite3VdbeMakeLabel(v);/*为VDBE创建一个标签，返回值赋给 iBreak*/
      iCont = sqlite3VdbeMakeLabel(v);
      computeLimitRegisters(pParse, p, iBreak);/*根据返回的专用地址，计算limit结果域*/
      sqlite3VdbeAddOp2(v, OP_Rewind, tab1, iBreak);/*将OP_Rewind操作交给vdbe,然后返回这个操作的地址*/
      r1 = sqlite3GetTempReg(pParse);/*将语法解析树中临时寄存器的值给r1*/
      iStart = sqlite3VdbeAddOp2(v, OP_RowKey, tab1, r1);/*将 OP_RowKey操作交给vdbe,然后返回这个操作的地址,并赋值给iStart*/
      sqlite3VdbeAddOp4Int(v, OP_NotFound, tab2, iCont, r1, 0);/*将OP_NotFound操作交给vdbe,然后返回这个操作的地址*/
      sqlite3ReleaseTempReg(pParse, r1);/*释放语法解析树中临时寄存器r1的值*/
      selectInnerLoop(pParse, p, p->pEList, tab1, p->pEList->nExpr,/*根据表达式p->pEList和p->pEList->nExpr创建内连接*/
                      0, -1, &dest, iCont, iBreak);
      sqlite3VdbeResolveLabel(v, iCont);/*解析iCont，iCont是一个标签，并作为下一条被插入的地址*/
      sqlite3VdbeAddOp2(v, OP_Next, tab1, iStart);/*将OP_Next操作交给vdbe,然后返回这个操作的地址*/
      sqlite3VdbeResolveLabel(v, iBreak);/*解析iBreak,iBreak是一个标签，并作为下一条被插入的地址*/
      sqlite3VdbeAddOp2(v, OP_Close, tab2, 0);/*将OP_Close操作交给vdbe,然后返回这个操作的地址*/
      sqlite3VdbeAddOp2(v, OP_Close, tab1, 0);/*将 OP_Close操作交给vdbe,然后返回这个操作的地址*/
      break;
    }
  }

  explainComposite(pParse, p->op, iSub1, iSub2, p->op!=TK_ALL);/*如果没有执行SELECT，那么就输出Composite subqueries  iSub1和 iSub2*/
  
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
	** 临时表使用计算排序序列去实现复合查询。给所有的临时表关联关键信息结构
	** 这部分是仅仅通过最右边的SELECT语句运行的。
	** 左边的SELECT语句总是跳过这个部分。
	**最右边的SELECT如果没有ORDER BY也不需要临时表的话也可能跳过这个部分
	*/
	if (p->selFlags & SF_UsesEphemeral){/*如果SELECT中selFlags为SF_UsesEphemeral*/
		int i;                        /* Loop counter 循环计数器 */
		KeyInfo *pKeyInfo;            /* Collating sequence for the result set 结果集的排序序列 */
		Select *pLoop;                /* For looping through SELECT statements 遍历SELECT语句 *//*通过SELECT语句进行循环*/
		CollSeq **apColl;             /* For looping through pKeyInfo->aColl[] 遍历pKeyInfo - > aColl[]*//*通过关键信息结构体中aColl[]数组循环*/
		int nCol;                     /* Number of columns in result set 结果集的列数*/

		assert(p->pRightmost == p);/*插入断点判断最右边的SELECT是否为p*/
		nCol = p->pEList->nExpr;/*表达式列表中表达式的个数赋值给nCol*/
		pKeyInfo = sqlite3DbMallocZero(db,
			sizeof(*pKeyInfo) + nCol*(sizeof(CollSeq*) + 1));/*分配并清空内存，分配大小为第二个参数的内存*/
		if (!pKeyInfo){/*如果关键信息结构体不存在*/
			rc = SQLITE_NOMEM;/*结束标记为SQLITE_NOMEM*/
			goto multi_select_end;/*跳到multi_select_end（执行结束）*/
		}

		pKeyInfo->enc = ENC(db);/*设置关键结构体的环境变量为db*/
		pKeyInfo->nField = (u16)nCol;/*设置关键结构体的域大小为结果集的列数*/

		for (i = 0, apColl = pKeyInfo->aColl; i < nCol; i++, apColl++){/*根据结果集的列数循环*/
			*apColl = multiSelectCollSeq(pParse, p, i);/*为列生成一个排序序列*/
			if (0 == *apColl){/*如果排序序列为空*/
				*apColl = db->pDfltColl;/*那么使用默认的排序序列*/
			}
		}

		for (pLoop = p; pLoop; pLoop = pLoop->pPrior){/*遍历优先SELECT*/
			for (i = 0; i < 2; i++){
				int addr = pLoop->addrOpenEphm[i];/*将OP_OpenEphem操作数组的元素赋值给addr地址*/
				if (addr < 0){/*如果addr小于0*/
					/* If [0] is unused then [1] is also unused.  So we can
					** always safely abort as soon as the first unused slot is found
					**如果[0]没有被使用，那么[1]也同样没有被使用. 所以我们在发现了第一个未使用的之后能够安全的中止它.
					*/
					assert(pLoop->addrOpenEphm[1] < 0);/*插入断点，判断循环中OP_OpenEphem操作数组有小于0的情况*/
					break;
				}
				sqlite3VdbeChangeP2(v, addr, nCol);/*将nCol的值设置到虚拟机*/
				sqlite3VdbeChangeP4(v, addr, (char*)pKeyInfo, P4_KEYINFO);/*如果关键信息结构中地址值大于addr,将P4_KEYINFO的值设置到虚拟机*/
				pLoop->addrOpenEphm[i] = -1;/*将OP_OpenEphem操作数组的元素设为-1*/
			}
		}
		sqlite3DbFree(db, pKeyInfo);/*释放数据库连接中关联关键字结构体的内存*/
	}

multi_select_end:/*上文的goto跳到这个部分*/
	pDest->iSdst = dest.iSdst;/*写入到基址寄存器的值赋值给处理结果集的存储基址寄存器的值*/
	pDest->nSdst = dest.nSdst;/*已分配基址寄存器的数量赋值给处理结果集的存储基址寄存器的数量*/
	sqlite3SelectDelete(db, pDelete);/*释放Select结构体pDelete*/
	return rc;
}
#endif /* SQLITE_OMIT_COMPOUND_SELECT */

/*
** Code an output subroutine for a coroutine implementation of a
** SELECT statment.
** 为协同执行查询语句编写一个输出子程序.
**
** The data to be output is contained in pIn->iSdst.  There are
** pIn->nSdst columns to be output.  pDest is where the output should
** be sent.
** 待输出的数据包含在pIn->iSdst中,还有pIn->nSdst列等待输出.这些输出应该以pDest为目的地.
**
** regReturn is the number of the register holding the subroutine
** return address.
** regReturn是存储了子程序返回地址的寄存器数量.
**
** If regPrev>0 then it is the first register in a vector that
** records the previous output.  mem[regPrev] is a flag that is false
** if there has been no previous output.  If regPrev>0 then code is
** generated to suppress duplicates.  pKeyInfo is used for comparing
** keys.
** 如果regPrev>0,那么这是向量中记录了之前输出的第一个寄存器. 
** mem[regPrev]是如果之前没有输出的失败标记. 如果regPrev>0那么生成的代码就是用来禁止重复.
** pKeyInfo被用来比较键值.
**
** If the LIMIT found in p->iLimit is reached, jump immediately to
** iBreak.
** 为协作程序写一个输出子程序实现SELECT语句。
** 输出的数据来自pIn->iSdst（存放结果的基址寄存器），pIn->iSdst的列被输出，pDest（处理结果集结构体）进行输出。
** regReturn是存储子程序地址的寄存器的数量
** 如果regPrev > 0,那么它是第一个向量寄存器,其中记录这前面的输出数据
** 如果在SELECT结构体中找到limit，立即跳到iBreak
*/
static int generateOutputSubroutine(
	Parse *pParse,          /* Parsing context 解析上下文*/
	Select *p,              /* The SELECT statement 声明SELECT结构体*/
	SelectDest *pIn,        /* Coroutine supplying data 声明处理数据集结构体，协同程序提供数据*/
	SelectDest *pDest,      /* Where to send the data 发送数据*//*发送数据的，发送的数据在哪里*/
	int regReturn,          /* The return address register 返回地址寄存器*/
	int regPrev,            /* Previous result register.  No uniqueness if 0 以前的结果寄存器，没有唯一的为0*/
	KeyInfo *pKeyInfo,      /* For comparing with previous entry 与以前的条目比较*/
	int p4type,             /* The p4 type for pKeyInfo pKeyInfo的p4类型*/
	int iBreak              /* Jump here if we hit the LIMIT 如果发现LIMIT跳到此处*/
	){
	Vdbe *v = pParse->pVdbe;/*将语法解析树的VDBE属性赋值给VDBE变量v*/
	int iContinue;
	int addr;

	addr = sqlite3VdbeCurrentAddr(v);/*将vdbe的下一条指令地址赋值给addr*/
	iContinue = sqlite3VdbeMakeLabel(v);/*为VDBE创建一个标签，返回值赋值给iContinue*/

	/* Suppress duplicates for UNION, EXCEPT, and INTERSECT	  禁止UNION,EXCEPT,INTERSECT副本*/
	if (regPrev){/*如果以前的寄存器存在*/
		int j1, j2;
		j1 = sqlite3VdbeAddOp1(v, OP_IfNot, regPrev);/*将OP_IfNot操作交给vdbe，然后返回这个操作的地址复制给addr*/
		j2 = sqlite3VdbeAddOp4(v, OP_Compare, pIn->iSdst, regPrev + 1, pIn->nSdst,
			(char*)pKeyInfo, p4type);/*在操作中添加p4type,并返回一个指针*/
		sqlite3VdbeAddOp3(v, OP_Jump, j2 + 2, iContinue, j2 + 2);/*将OP_Jump操作交给vdbe，然后返回这个操作的地址*/
		sqlite3VdbeJumpHere(v, j1);/*如果j1存在，运行地址跳到当前地址*/
		sqlite3ExprCodeCopy(pParse, pIn->iSdst, regPrev + 1, pIn->nSdst);/*copy参数pIn->iSdst中内容到pIn->nSdst，其实均是寄存器*/
		sqlite3VdbeAddOp2(v, OP_Integer, 1, regPrev);/*将OP_Integer操作交给vdbe，然后返回这个操作的地址*/
	}
	if (pParse->db->mallocFailed) return 0;/*如果发生内存分配失败，就直接返回0*/

	/* Suppress the first OFFSET entries if there is an OFFSET clause  如果有OFFSET子句，禁止OFFSET多个入口*/
	codeOffset(v, p, iContinue);/*设置偏移量，其中iContinue是下一次循环要调到的地址*/

	switch (pDest->eDest){
		/* Store the result as data using a unique key.*/
		/*  使用一个唯一键存储结果*/
	case SRT_Table:
	case SRT_EphemTab: {
		int r1 = sqlite3GetTempReg(pParse);/*分配一个寄存器，存储中间计算结果*//*得到语法解析树中临时寄存器中的值给r1*/
		int r2 = sqlite3GetTempReg(pParse);/*得到语法解析树中临时寄存器中的值给r2*/
		testcase(pDest->eDest == SRT_Table);/*测试处理的结果集的表名称*//*测试处理结果集中的处理结果的方式是否为SRT_Table*/
		testcase(pDest->eDest == SRT_EphemTab);/*测试处理的结果集的表的大小*//*测试处理结果集中的处理结果的方式是否为SRT_EphemTab*/
		sqlite3VdbeAddOp3(v, OP_MakeRecord, pIn->iSdst, pIn->nSdst, r1);/*把OP_MakeRecord（做记录）操作送入VDBE，再返回一个新指令地址*//*将OP_MakeRecord操作交给vdbe，然后返回这个操作的地址*/
		sqlite3VdbeAddOp2(v, OP_NewRowid, pDest->iSDParm, r2);/*将OP_NewRowid操作交给vdbe，然后返回这个操作的地址*/
		sqlite3VdbeAddOp3(v, OP_Insert, pDest->iSDParm, r1, r2);/*将OP_Insert操作交给vdbe，然后返回这个操作的地址*/
		sqlite3VdbeChangeP5(v, OPFLAG_APPEND);/*把OPFLAG_APPEND设置为最近最多使用的值*/
		sqlite3ReleaseTempReg(pParse, r2);/*释放语法解析树中临时寄存器r2的值*/
		sqlite3ReleaseTempReg(pParse, r1);/*释放语法解析树中临时寄存器r1的值*/
		break;
	}

#ifndef SQLITE_OMIT_SUBQUERY
		/* If we are creating a set for an "expr IN (SELECT ...)" construct,
		** then there should be a single item on the stack.  Write this
		** item into the set table with bogus data.

		** 如果我们为"expr IN (SELECT ...)"创建一个结构集合,那么栈里面应该只有一个元素.
		** 将这个元素伪装后写入表集中.
		*/
	case SRT_Set: {
		int r1;
		assert(pIn->nSdst == 1);/*如果协同处理数据集的寄存器数为1*/
		p->affinity =
			sqlite3CompareAffinity(p->pEList->a[0].pExpr, pDest->affSdst);/*将表达式列表中的表达式pExpr与处理结果集中的affSdst亲和性比较，并返回比较后的类型*/
		r1 = sqlite3GetTempReg(pParse);/*得到语法解析树中临时寄存器中的值给r1*/
		sqlite3VdbeAddOp4(v, OP_MakeRecord, pIn->iSdst, 1, r1, &p->affinity, 1);/*添加一个OP_MakeRecord操作符，并将它的值作为一个指针*/
		sqlite3ExprCacheAffinityChange(pParse, pIn->iSdst, 1);/*处理语法树pParse，寄存器中的亲和性数据*/
		sqlite3VdbeAddOp2(v, OP_IdxInsert, pDest->iSDParm, r1);/*把OP_IdxInsert操作送入VDBE，再返回一个新指令地址*/
		sqlite3ReleaseTempReg(pParse, r1);/*释放这个寄存器*//*释放语法解析树中临时寄存器r1的值*/
		break;
	}

#if 0  /* Never occurs on an ORDER BY query */
		/* If any row exist in the result set, record that fact and abort.

		** 如果有任何未处理的数据存在于结果集中,记录它然后中止
		*/
	case SRT_Exists: {
		sqlite3VdbeAddOp2(v, OP_Integer, 1, pDest->iSDParm);
		/* The LIMIT clause will terminate the loop for us  Limit子句将会终止循环*/
		break;
	}
#endif

		/* If this is a scalar select that is part of an expression, then
		** store the results in the appropriate memory cell and break out
		** of the scan loop.
		** 如果一个标量选择是一个表达式的一部分，将结果存储在一个合适的内存中，并且搭配扫描循环

		*/
	case SRT_Mem: {
		assert(pIn->nSdst == 1);/*插入断点，判断协同处理数据集的寄存器数是否为1*/
		sqlite3ExprCodeMove(pParse, pIn->iSdst, pDest->iSDParm, 1);/*释放寄存器中的内容，保持寄存器的内容及时更新*/
		/* The LIMIT clause will jump out of the loop for us  Limit子句将会从循环中跳出*/
		break;
	}
#endif /* #ifndef SQLITE_OMIT_SUBQUERY */

		/* The results are stored in a sequence of registers
		** starting at pDest->iSdst.  Then the co-routine yields.
		** 结果存储在一个连续的寄存器中，起始位置为写入数据的基址寄存器的地址，然后执行协同代码块

		*/
	case SRT_Coroutine: {
		if (pDest->iSdst == 0){/*如果基址寄存器为0*/
			pDest->iSdst = sqlite3GetTempRange(pParse, pIn->nSdst);/*分配一串连续的寄存器，长度为已分配寄存器的个数*/
			pDest->nSdst = pIn->nSdst;/*将分配的寄存器的个数记录到nSdst中*/
		}
		sqlite3ExprCodeMove(pParse, pIn->iSdst, pDest->iSdst, pDest->nSdst);/*释放寄存器中的内容，保持寄存器的内容及时更新*/
		sqlite3VdbeAddOp1(v, OP_Yield, pDest->iSDParm);/*将OP_Yield操作交给vdbe，然后返回这个操作的地址返回*/
		break;
	}

		/* If none of the above, then the result destination must be
		** SRT_Output.  This routine is never called with any other
		** destination other than the ones handled above or SRT_Output.
		**
		** For SRT_Output, results are stored in a sequence of registers.
		** Then the OP_ResultRow opcode is used to cause sqlite3_step() to
		** return the next row of result.
		** 如果没有以上的部分，那么结果集必须进行输出。这个程序不使用在其他任何处理条目或输出结果之上
		** 对于结果输出来说，结果存储在一个寄存器序列中。然后OP_ResultRow操作用在sqlite3_step()中，返回下一行结果
		*/
	default: {
		assert(pDest->eDest == SRT_Output);/*插入断点，判断处理结果集中处理方式是否为SRT_Output（输出）*/
		sqlite3VdbeAddOp2(v, OP_ResultRow, pIn->iSdst, pIn->nSdst);/*将OP_ResultRow操作交给vdbe，然后返回这个操作的地址*/
		sqlite3ExprCacheAffinityChange(pParse, pIn->iSdst, pIn->nSdst);/*处理语法树pParse，寄存器中的iSdst与nSdst亲和性数据*/
		break;
	}
	}

	/* Jump to the end of the loop if the LIMIT is reached.
	** 如果到Limit的限制处，直接跳到结尾
	*/
	if (p->iLimit){
		sqlite3VdbeAddOp3(v, OP_IfZero, p->iLimit, iBreak, -1);/*将OP_IfZero操作交给vdbe，然后返回这个操作的地址*/
	}

	/* Generate the subroutine return
	** 生成子程序返回
	*/
	sqlite3VdbeResolveLabel(v, iContinue);/*解析iContinue，iContinue是一个标签，并作为下一条被插入的地址*/
	sqlite3VdbeAddOp1(v, OP_Return, regReturn);/*将OP_Return操作交给vdbe，然后返回这个操作的地址返回*/

	return addr;/*返回这个地址*/
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
** 我们能调用AltB, AeqB, AgtB, EofA 和 EofB子程序，但是实际上他们没有返回的时候，我们不能调用他们。（按顺序执行）
** 直到所有的数据都已经使用过了（注释叫耗尽），EofA 和 EofB的循环才跳到"end"标签处。 AltB, AeqB,and AgtB
** 跳到L2或 跳到EofA 和 EofB的其中一个之中
*/
#ifndef SQLITE_OMIT_COMPOUND_SELECT
static int multiSelectOrderBy(
	Parse *pParse,        /* Parsing context 解析上下文*/
	Select *p,            /* The right-most of SELECTs to be coded 编写最右边的SELECT*/
	SelectDest *pDest     /* What to do with query results 处理结果集结构体*/
	){
	int i, j;             /* Loop counters 循环计数器*/
	Select *pPrior;       /* Another SELECT immediately to our left 在左边的立即查询*/
	Vdbe *v;              /* Generate code to this VDBE 为VEBE生成代码*/
	SelectDest destA;     /* Destination for coroutine A 协同程序A，处理结果集*/
	SelectDest destB;     /* Destination for coroutine B 协同程序B，处理结果集*/
	int regAddrA;         /* Address register for select-A coroutine select-A程序的基址寄存器*/
	int regEofA;          /* Flag to indicate when select-A is complete select-A程序的完成标记*/
	int regAddrB;         /* Address register for select-B coroutine select-B程序的基址寄存器*/
	int regEofB;          /* Flag to indicate when select-B is complete select-B程序的完成标记*/
	int addrSelectA;      /* Address of the select-A coroutine select-A程序的地址*/
	int addrSelectB;      /* Address of the select-B coroutine select-B程序的地址*/
	int regOutA;          /* Address register for the output-A subroutine output-A程序的基址寄存器*/
	int regOutB;          /* Address register for the output-B subroutine output-B程序的基址寄存器*/
	int addrOutA;         /* Address of the output-A subroutine select-A程序的地址*/
	int addrOutB = 0;     /* Address of the output-B subroutine select-B程序的地址*/
	int addrEofA;         /* Address of the select-A-exhausted subroutine select-A-exhausted程序的地址*/
	int addrEofB;         /* Address of the select-B-exhausted subroutine select-B-exhausted程序的地址*/
	int addrAltB;         /* Address of the A<B subroutine A<B子程序的地址*/
	int addrAeqB;         /* Address of the A==B subroutine A=B子程序的地址*/
	int addrAgtB;         /* Address of the A>B subroutine A>B子程序的地址*/
	int regLimitA;        /* Limit register for select-A select-A的Limit寄存器（存放Limit值的）*/
	int regLimitB;        /* Limit register for select-A select-B的Limit寄存器（存放Limit值的）*/
	int regPrev;          /* A range of registers to hold previous output 一系列保存之前的输出的寄存器（用来比较重复）*/
	int savedLimit;       /* Saved value of p->iLimit 保存p->iLimit的值*/
	int savedOffset;      /* Saved value of p->iOffset 保存p->iOffset的值*/
	int labelCmpr;        /* Label for the start of the merge algorithm 合并算法的开始标签*/
	int labelEnd;         /* Label for the end of the overall SELECT stmt 全部SELECT结束的标签*/
	int j1;               /* Jump instructions that get retargetted 得到新指令的跳转指令*/
	int op;               /* One of TK_ALL, TK_UNION, TK_EXCEPT, TK_INTERSECT TK_ALL, TK_UNION, TK_EXCEPT, TK_INTERSECT中的一个操作*/
	KeyInfo *pKeyDup = 0; /* Comparison information for duplicate removal 比较后，删除重复信息*/
	KeyInfo *pKeyMerge;   /* Comparison information for merging rows 比较后，合并行*/
	sqlite3 *db;          /* Database connection 数据库连接*/
	ExprList *pOrderBy;   /* The ORDER BY clause ORDERBY子句*/
	int nOrderBy;         /* Number of terms in the ORDER BY clause ORDERBY子句的个数*/
	int *aPermute;        /* Mapping from ORDER BY terms to result set columns *做从ORDERBY到结果集列的映射/
#ifndef SQLITE_OMIT_EXPLAIN
	int iSub1;            /* EQP id of left-hand query *//*左查询的唯一ID*/
	int iSub2;            /* EQP id of right-hand query *//*右查询的唯一ID*/
#endif

	assert(p->pOrderBy != 0);/*插入断点，判断含有ORDERBY*/
	assert(pKeyDup == 0); /* "Managed" code needs this.  Ticket #3382. *//* "Managed" code needs this.  Ticket #3382. *//*Managed（托管）代码需要，Ticket（加标签）#3382*/
	db = pParse->db;/*声明一个数据库连接*/
	v = pParse->pVdbe;/*声明一个VEBE*/
	 /*终于看到了assert!!括号里面是true，就正常，如果括号里面是FALSE就抛出异常*/
	assert(v != 0);       /* Already thrown the error if VDBE alloc failed *//*如果VDBE分配错误会抛出异常*/
	labelEnd = sqlite3VdbeMakeLabel(v);/*为VDBE创建一个标签，返回值赋值给labelEnd（全部SELECT结束的标签）*/
	labelCmpr = sqlite3VdbeMakeLabel(v);/*为VDBE创建一个标签，返回值赋值给labelCmpr（合并算法的开始标签）*/


	/* Patch up the ORDER BY clause
	/*补充ORDER BY子句*/
	op = p->op;/*将SELECT中操作符赋值给变量op*/ 
	pPrior = p->pPrior;/*SELECT结构体的优先查询的SELECT赋值给变量pPrior*/
	assert(pPrior->pOrderBy == 0);/*插入断点，如果优先查询的SELECT没有OrderBy，便抛出警告信息*/
	pOrderBy = p->pOrderBy;/*将SELECT中的ORDERBY子句赋值给变量pOrderBy*/
	assert(pOrderBy);/*插入断点，如果没有ORDERBY，便抛出警告信息*/
	nOrderBy = pOrderBy->nExpr;/*pOrderBy的表达式个数赋值给nOrderBy*/

	/* For operators other than UNION ALL we have to make sure that
	** the ORDER BY clause covers every term of the result set.  Add
	** terms to the ORDER BY clause as necessary.
/*除了UNION ALL，ORDERBY必须能覆盖到结果集的每一个条款中，添加条款到ORDERBY子句是必须的*/
	if (op != TK_ALL){/*如果操作符不是TK_ALL*/
		for (i = 1; db->mallocFailed == 0 && i <= p->pEList->nExpr; i++){/*并且遍历表达式*/
			struct ExprList_item *pItem;/*声明一个表示列表项*/
			for (j = 0, pItem = pOrderBy->a; j < nOrderBy; j++, pItem++){/*遍历条目*/
				assert(pItem->iOrderByCol > 0);/*插入断点，如果没有ORDERBY，就抛出错误信息*/
				if (pItem->iOrderByCol == i) break;/*如果ORDERBY条目为i，便返回，跳出当前的循环。*/
			}
			if (j == nOrderBy){/*如果此时上个循环结束，j的值为ORDERBE的个数*/
				Expr *pNew = sqlite3Expr(db, TK_INTEGER, 0);/*将TK_INTEGER给表达式pNew，sqlite3Expr将操作值转化为表达式*/
				if (pNew == 0) return SQLITE_NOMEM;/*如果表达式为0，说明没有操作。直接返回SQLITE_NOMEM*/
				pNew->flags |= EP_IntValue;/*flags标签位或EP_IntValue再赋值给flags标记*/
				pNew->u.iValue = i;/*将当前i的值，赋值给表达式中的i值*/
				pOrderBy = sqlite3ExprListAppend(pParse, pOrderBy, pNew);/*将pNew表达式添加到pParse中的pOrderBy后面*/
				if (pOrderBy) pOrderBy->a[nOrderBy++].iOrderByCol = (u16)i;/*如果pOrderBy存在，将iOrderByCol的值设为i*/
			}
		}
	}

	/* Compute the comparison permutation and keyinfo that is used with
	** the permutation used to determine if the next
	** row of results comes from selectA or selectB.  Also add explicit
	** collations to the ORDER BY clause terms so that when the subqueries
	** to the right and the left are evaluated, they use the correct
	** collation.
*//*用排列计算比较排列和关键信息结构决定下一行结果来自selectA还是selectB.
	  ** 添加对ORDERBY子句的显式排列，以便评估右边和左边的子程序，让其使用正确的序列。*/
	aPermute = sqlite3DbMallocRaw(db, sizeof(int)*nOrderBy);/*分配并清空内存，再分配大小为ORDERBY的个数*/
	if (aPermute){/*如果aPermute不为0*/
		struct ExprList_item *pItem;/*声明一个表达式列表项*/
		for (i = 0, pItem = pOrderBy->a; i < nOrderBy; i++, pItem++){/*遍历表达式列表项*/
			assert(pItem->iOrderByCol>0 && pItem->iOrderByCol <= p->pEList->nExpr);/*插入断点，判断根据列ORDERBY个数大于0并且小于等于表达式的个数*/
			aPermute[i] = pItem->iOrderByCol - 1;/*将根据列ORDERBY减一（下标从0开始）存到aPermute数组中*/
		}
		pKeyMerge =
			sqlite3DbMallocRaw(db, sizeof(*pKeyMerge) + nOrderBy*(sizeof(CollSeq*) + 1));/*分配并清空内存，再分配大小为第二个参数*/
		if (pKeyMerge){/*如果上句程序中pKeyMerge不为空*/
			pKeyMerge->aSortOrder = (u8*)&pKeyMerge->aColl[nOrderBy];/*关键信息结构体中存储对列排序的方法，值为关键信息机构体中aColl数组存储的方式*/
			pKeyMerge->nField = (u16)nOrderBy;/*ORDERBY的个数存入为关键信息结构体的aColl[]的条目个数*/
			pKeyMerge->enc = ENC(db);/*设定编码方式（SQLITE_UTF*）*/
			for (i = 0; i < nOrderBy; i++){/*根据ORDERBY的个数遍历*/
				CollSeq *pColl;/*声明一个排序序列*/
				Expr *pTerm = pOrderBy->a[i].pExpr;/*将ORDERBY表达式中第i个表达式赋值给pTerm*/
				if (pTerm->flags & EP_ExpCollate){/*如果flags的值为EP_ExpCollate*/
					pColl = pTerm->pColl;/*将表达式pTerm的排序序列赋值给变量pColl*/
				}
				else{
					pColl = multiSelectCollSeq(pParse, p, aPermute[i]);/*为列生成一个排序序列（说明aPermute数组是从ORDERBY到结果集列的映射）*/
					pTerm->flags |= EP_ExpCollate;/*flags标签位或EP_ExpCollate再赋值给flags标记*/
					pTerm->pColl = pColl;/*将已经存入数据的pColl赋值给表达式中的排序序列*/
				}
				pKeyMerge->aColl[i] = pColl;/*将当前循环中排序序列放到关键信息结构中序列组中，并对应在i下，这时循环也在i,一一对应*/
				pKeyMerge->aSortOrder[i] = pOrderBy->a[i].sortOrder;/*将pOrderBy表达式列表中的第i-1个排序方法放到关键信息结构体中的排序数组中*/
			}
		}
	}
	else{/*如果上句程序中pKeyMerge不为空，说明表达式为空，内存中没有*/
		pKeyMerge = 0;/*将关键信息结构体置为0*/
	}

	/* Reattach the ORDER BY clause to the query.
	*//*再次将ORDER BY子句加到查询中*/
	p->pOrderBy = pOrderBy;/*将当前的pOrderBy表达式列表赋值给SELECT的pOrderBy属性*/
	pPrior->pOrderBy = sqlite3ExprListDup(pParse->db, pOrderBy, 0);/*深度copy属性pOrderBy给优先查找SELECT的pOrderBy*/

	/* Allocate a range of temporary registers and the KeyInfo needed
	** for the logic that removes duplicate result rows when the
	** operator is UNION, EXCEPT, or INTERSECT (but not UNION ALL).
	*//*当操作符是UNION, EXCEPT, or INTERSECT (除了UNION ALL)时，分配一套临时寄存器和逻辑上需要的关键信息结构体，删除重复的结果*/
	
	if (op == TK_ALL){/*当操作符是TK_ALL*/
		regPrev = 0;/*将存储以前的数据的寄存器置0*/
	}
	else{
		int nExpr = p->pEList->nExpr;/*将表达式列表中表达式的个数赋值给nExpr*/
		assert(nOrderBy >= nExpr || db->mallocFailed);/*插入断点，如果nOrderBy（OrderBy个数）大于等于nExpr（表达式个数）或分配内存失败*/
		regPrev = sqlite3GetTempRange(pParse, nExpr + 1);/*分配一串连续的寄存器，长度为表达式的个数加1*/
		sqlite3VdbeAddOp2(v, OP_Integer, 0, regPrev);/*将OP_Integer操作交给vdbe，然后返回这个操作的地址*/
		pKeyDup = sqlite3DbMallocZero(db,
			sizeof(*pKeyDup) + nExpr*(sizeof(CollSeq*) + 1));/*分配并清空内存，分配内存大小为第二个参数*/
		if (pKeyDup){/*如果上一句分配内存的大小不为0*/
			pKeyDup->aSortOrder = (u8*)&pKeyDup->aColl[nExpr];/*将排序序列的表达式赋值给关键信息结构中的排序属性*/
			pKeyDup->nField = (u16)nExpr;/*将表达式个数赋值给aColl数组中总的条目数*/
			pKeyDup->enc = ENC(db);/*设定编码方式（SQLITE_UTF*）*/
			for (i = 0; i < nExpr; i++){/*遍历表达式*/
				pKeyDup->aColl[i] = multiSelectCollSeq(pParse, p, i);/*multiSelectCollSeq设置排序的方式，再赋值给aColl*/
				pKeyDup->aSortOrder[i] = 0;/*设置完排序序列之后，将排序方式设为0*/
			}
		}
	}

	/* Separate the left and the right query from one another
	*//*单独的左边和右边查询*/
	  /*上下的两个区别是，一个是SELECT中的ORDER BY*/
	p->pPrior = 0;/*令SELECT中的优先查询SELECT为空*/
	sqlite3ResolveOrderGroupBy(pParse, p, p->pOrderBy, "ORDER");/*解析GROUP中ORDERBY，检测ORDER关键字。返回0表示成功！*/
	if (pPrior->pPrior == 0){/*如果SELECT中的优先查询SELECT为空*/
		sqlite3ResolveOrderGroupBy(pParse, pPrior, pPrior->pOrderBy, "ORDER");/*解析GROUP中ORDERBY，检测ORDER关键字。返回0表示成功！*/
	}

	/* Compute the limit registers *//*计算limit寄存器*/
	computeLimitRegisters(pParse, p, labelEnd);/*根据返回的的专用地址，计算出limit结果域，并且结束*/
	if (p->iLimit && op == TK_ALL){/*如果SELECT中limit不为空，并且操作符TK_ALL*/
		regLimitA = ++pParse->nMem;/*将语法解析树中分配内存的数++之后赋值给select-A的Limit寄存器*/
		regLimitB = ++pParse->nMem;/*将语法解析树中分配内存的数++之后赋值给select-B的Limit寄存器*/
		sqlite3VdbeAddOp2(v, OP_Copy, p->iOffset ? p->iOffset + 1 : p->iLimit,
			regLimitA);/*将OP_Integer操作交给vdbe，然后返回这个操作的地址.如果iOffset为空，将iOffset加一，不为空设置参数为limit值*/
		sqlite3VdbeAddOp2(v, OP_Copy, regLimitA, regLimitB);;/*将OP_Copy操作交给vdbe，然后返回这个操作的地址*/
	}
	else{
		regLimitA = regLimitB = 0;/*否则设置select-A的Limit寄存器和select-A的Limit寄存器值为0*/
	}
	sqlite3ExprDelete(db, p->pLimit);/*删除数据库连接中的limit表达式*/
	p->pLimit = 0;/*令SELECT结构体中的limit为0*/
	sqlite3ExprDelete(db, p->pOffset);/*删除数据库连接中的offset表达式*/
	p->pOffset = 0;/*令SELECT结构体中的offset为0*/

	regAddrA = ++pParse->nMem;/*select-A程序的基址寄存器为语法解析树中分配内存的数目加1*/
	regEofA = ++pParse->nMem;/*select-A程序的完成标记为语法解析树中分配内存的数目加1*/
	regAddrB = ++pParse->nMem;/*select-B程序的基址寄存器为语法解析树中分配内存的数目加1*/
	regEofB = ++pParse->nMem;/*select-B程序的完成标记为语法解析树中分配内存的数目加1*/
	regOutA = ++pParse->nMem;/*output-A程序的基址寄存器为语法解析树中分配内存的数目加1*/
	regOutB = ++pParse->nMem;/*output-B程序的基址寄存器为语法解析树中分配内存的数目加1*/
	sqlite3SelectDestInit(&destA, SRT_Coroutine, regAddrA);/*初始化处理结果集存储到regAddrA，标记为SRT_Coroutine*/
	sqlite3SelectDestInit(&destB, SRT_Coroutine, regAddrB);/*初始化处理结果集存储到regAddrB，标记为SRT_Coroutine*/

	/* Jump past the various subroutines and coroutines to the main
	** merge loop
	 *//*跳过各个子程序和协同程序，进入主函数合并循环*/
	j1 = sqlite3VdbeAddOp0(v, OP_Goto);/*这个是使用Goto语句之后，返回的地址*/
	addrSelectA = sqlite3VdbeCurrentAddr(v);/*将vdbe的下一条指令地址赋值给addrSelectA(select-A程序的地址)*/


	/* Generate a coroutine to evaluate the SELECT statement to the
	** left of the compound operator - the "A" select.
	*//*生成一个协同程序评估SELECT的左边复杂操作符，"A"SELECT*/
	VdbeNoopComment((v, "Begin coroutine for left SELECT"));/*输出提示信息*/
	pPrior->iLimit = regLimitA;/*select-A的Limit寄存器中的值存放到优先查询SELECT中的iLimit属性*/
	explainSetInteger(iSub1, pParse->iNextSelectId);/*把语法分析树的pParse的下一个SelectID赋值给iSub1*/
	sqlite3Select(pParse, pPrior, &destA);/*根据语法解析树，SELECT结构体和处理结果集结构体生成SELECT语法树*/
	sqlite3VdbeAddOp2(v, OP_Integer, 1, regEofA);/*将OP_Integer操作交给vdbe，然后返回这个操作的地址*/
	sqlite3VdbeAddOp1(v, OP_Yield, regAddrA);/*将OP_Yield操作交给vdbe，然后返回这个操作的地址返回*/
	VdbeNoopComment((v, "End coroutine for left SELECT"));/*输出提示信息*/

	/* Generate a coroutine to evaluate the SELECT statement on
	** the right - the "B" select
	*//*生成一个协同程序评估SELECT的左边复杂操作符，"B" SELECT**/
	addrSelectB = sqlite3VdbeCurrentAddr(v);/*将vdbe的下一条指令地址赋值给addrSelectB(select-B程序的地址)*/
	VdbeNoopComment((v, "Begin coroutine for right SELECT"));/*输出提示信息*/
	savedLimit = p->iLimit;/*将SELECT中iLimit属性值保存到变量savedLimit的值*/
	savedOffset = p->iOffset;/*将SELECT中iOffset属性值保存到变量savedOffset的值*/
	p->iLimit = regLimitB;/*将select-B的Limit寄存器中值存放到SELECT中iLimit属性值*/
	p->iOffset = 0;/*将SELECT中iOffset属性值置为0*/
	explainSetInteger(iSub2, pParse->iNextSelectId);/*把语法分析树的pParse的下一个SelectID赋值给iSub2*/
	sqlite3Select(pParse, p, &destB);/*根据语法解析树，SELECT结构体和处理结果集结构体生成SELECT语法树*/
	p->iLimit = savedLimit;/*再将变量savedLimit赋值给SELECT中iLimit属性*/
	p->iOffset = savedOffset;/*将变量savedOffset赋值给SELECT中iOffset属性*/
	sqlite3VdbeAddOp2(v, OP_Integer, 1, regEofB);/*将OP_Integer操作交给vdbe，然后返回这个操作的地址*/
	sqlite3VdbeAddOp1(v, OP_Yield, regAddrB);/*将OP_Yield操作交给vdbe，然后返回这个操作的地址*/
	VdbeNoopComment((v, "End coroutine for right SELECT"));/*输出提示信息*/

	/* Generate a subroutine that outputs the current row of the A
	** select as the next output row of the compound select.
	*//*生成一个子程序输出A select的结果作为下一个复合选查询的输出行*/
	VdbeNoopComment((v, "Output routine for A"));/*输出提示信息*/
	addrOutA = generateOutputSubroutine(pParse,
		p, &destA, pDest, regOutA,
		regPrev, pKeyDup, P4_KEYINFO_HANDOFF, labelEnd);/*输出A select的结果，递归调用generateOutputSubroutine（）*/

	/* Generate a subroutine that outputs the current row of the B
	** select as the next output row of the compound select.
	*/
	if (op == TK_ALL || op == TK_UNION){/*如果操作符为TK_ALL或TK_UNION*/
		VdbeNoopComment((v, "Output routine for B"));/*输出提示信息*/
		addrOutB = generateOutputSubroutine(pParse,
			p, &destB, pDest, regOutB,
			regPrev, pKeyDup, P4_KEYINFO_STATIC, labelEnd);/*输出B select的结果，递归调用generateOutputSubroutine（）*/
	}

	/* Generate a subroutine to run when the results from select A
	** are exhausted and only data in select B remains.
 *//*当A select循环完，只有select B的数据时，运行该子程序*/
	VdbeNoopComment((v, "eof-A subroutine"));/*输出提示信息*/
	if (op == TK_EXCEPT || op == TK_INTERSECT){/*如果操作符为TK_EXCEPT或TK_INTERSECT*/
		addrEofA = sqlite3VdbeAddOp2(v, OP_Goto, 0, labelEnd);/*将OP_Goto操作交给vdbe，然后返回这个操作的地址*/
	}
	else{
		addrEofA = sqlite3VdbeAddOp2(v, OP_If, regEofB, labelEnd);/*将OP_If操作交给vdbe，然后返回这个操作的地址*/
		sqlite3VdbeAddOp2(v, OP_Gosub, regOutB, addrOutB);/*将OP_Integer操作交给vdbe，然后返回这个操作的地址*/
		sqlite3VdbeAddOp1(v, OP_Yield, regAddrB);/*将OP_Yield操作交给vdbe，然后返回这个操作的地址*/
		sqlite3VdbeAddOp2(v, OP_Goto, 0, addrEofA);/*将OP_Goto操作交给vdbe，然后返回这个操作的地址*/
		p->nSelectRow += pPrior->nSelectRow;/*如果SELECT中的操作为TK_UNION，将当前pPrior优先查找中的查找行加上SELECT的查找行。注：是不是基址加上偏移。*/
	}

	/* Generate a subroutine to run when the results from select B
	** are exhausted and only data in select A remains.
	 *//*当select B循环完，只有select A的数据时，运行该子程序*/
	if (op == TK_INTERSECT){/*如果操作符为TK_INTERSECT*/
		addrEofB = addrEofA;/*将select-A-exhausted程序的地址（select A结束地址）赋值给select-B-exhausted程序的地址*/
		if (p->nSelectRow > pPrior->nSelectRow) p->nSelectRow = pPrior->nSelectRow;/*如果SELECT中的查找行大于优先查询SELECT的查找行，将优先查询SELECT的查找行赋值给SELECT*/
	}
	else{
		VdbeNoopComment((v, "eof-B subroutine"));/*输出提示信息*/
		addrEofB = sqlite3VdbeAddOp2(v, OP_If, regEofA, labelEnd);/*将OP_If操作交给vdbe，然后返回这个操作的地址*/
		sqlite3VdbeAddOp2(v, OP_Gosub, regOutA, addrOutA);/*将OP_Gosub操作交给vdbe，然后返回这个操作的地址*/
		sqlite3VdbeAddOp1(v, OP_Yield, regAddrA);/*将OP_Yield操作交给vdbe，然后返回这个操作的地址*/
		sqlite3VdbeAddOp2(v, OP_Goto, 0, addrEofB);/*将OP_Goto操作交给vdbe，然后返回这个操作的地址*/
	  }
	}

	/* Generate code to handle the case of A<B
	 *//*生成代码来处理A < B时的情况*/
	VdbeNoopComment((v, "A-lt-B subroutine"));/*输出提示信息*/
	addrAltB = sqlite3VdbeAddOp2(v, OP_Gosub, regOutA, addrOutA);/*将OP_Gosub操作交给vdbe，然后返回这个操作的地址*/
	sqlite3VdbeAddOp1(v, OP_Yield, regAddrA);/*将OP_Yield操作交给vdbe，然后返回这个操作的地址*/
	sqlite3VdbeAddOp2(v, OP_If, regEofA, addrEofA);/*将OP_If操作交给vdbe，然后返回这个操作的地址*/
	sqlite3VdbeAddOp2(v, OP_Goto, 0, labelCmpr);/*将OP_Goto操作交给vdbe，然后返回这个操作的地址*/

	/* Generate code to handle the case of A==B
*//*生成代码来处理A = B时的情况*/
	if (op == TK_ALL){/*如果操作符为TK_ALL*/
		addrAeqB = addrAltB;/*上面程序中虚拟数据库引擎返回的地址赋值给A=B子程序的地址*/
	}
	else if (op == TK_INTERSECT){/*如果操作符为TK_INTERSECT*/
		addrAeqB = addrAltB;/*虚拟数据库引擎返回的地址赋值给A=B子程序的地址*/
		addrAltB++;/*返回地址加加*/
	}
	else{
		VdbeNoopComment((v, "A-eq-B subroutine"));/*输出提示信息*/
		addrAeqB =
			sqlite3VdbeAddOp1(v, OP_Yield, regAddrA);/*将OP_Yield操作交给vdbe，然后返回这个操作的地址*/
		sqlite3VdbeAddOp2(v, OP_If, regEofA, addrEofA);/*将OP_If,操作交给vdbe，然后返回这个操作的地址*/
		sqlite3VdbeAddOp2(v, OP_Goto, 0, labelCmpr);/*将OP_Goto操作交给vdbe，然后返回这个操作的地址*/
	}

	/* Generate code to handle the case of A>B
	*//*生成代码来处理A = B时的情况*/
	VdbeNoopComment((v, "A-gt-B subroutine"));/*输出提示信息*/
	addrAgtB = sqlite3VdbeCurrentAddr(v);/*将vdbe的下一条指令地址赋值给addrAgtB(A>B子程序的地址)*/
	if (op == TK_ALL || op == TK_UNION){/*如果操作符为TK_ALL或TK_UNION*/
		sqlite3VdbeAddOp2(v, OP_Gosub, regOutB, addrOutB);/*将OP_Gosub操作交给vdbe，然后返回这个操作的地址*/
	}
	sqlite3VdbeAddOp1(v, OP_Yield, regAddrB);/*将OP_Yield操作交给vdbe，然后返回这个操作的地址*/
	sqlite3VdbeAddOp2(v, OP_If, regEofB, addrEofB);/*将OP_If操作交给vdbe，然后返回这个操作的地址*/
	sqlite3VdbeAddOp2(v, OP_Goto, 0, labelCmpr);/*将OP_Goto操作交给vdbe，然后返回这个操作的地址*/

	/* This code runs once to initialize everything.
	  *//*这段代码运行一次初始化所有的对象*/
	sqlite3VdbeJumpHere(v, j1);/*如果j1存在，运行地址跳到当前地址*/
	sqlite3VdbeAddOp2(v, OP_Integer, 0, regEofA);/*将OP_Integer操作交给vdbe，然后返回这个操作的地址*/
	sqlite3VdbeAddOp2(v, OP_Integer, 0, regEofB);/*将OP_Integer操作交给vdbe，然后返回这个操作的地址*/
	sqlite3VdbeAddOp2(v, OP_Gosub, regAddrA, addrSelectA);/*将OP_Gosub操作交给vdbe，然后返回这个操作的地址*/
	sqlite3VdbeAddOp2(v, OP_Gosub, regAddrB, addrSelectB);/*将OP_Gosub操作交给vdbe，然后返回这个操作的地址*/
	sqlite3VdbeAddOp2(v, OP_If, regEofA, addrEofA);/*将OP_If操作交给vdbe，然后返回这个操作的地址*/
	sqlite3VdbeAddOp2(v, OP_If, regEofB, addrEofB);/*将OP_If操作交给vdbe，然后返回这个操作的地址*/

	/* Implement the main merge loop
	 *//*实现主合并循环*/
	sqlite3VdbeResolveLabel(v, labelCmpr);/*解析labelCmpr，labelCmpr是一个标签，并作为下一条被插入的地址*/
	sqlite3VdbeAddOp4(v, OP_Permutation, 0, 0, 0, (char*)aPermute, P4_INTARRAY);/*添加一个OP_Permutation操作符，并将它的值作为一个指针*/
	sqlite3VdbeAddOp4(v, OP_Compare, destA.iSdst, destB.iSdst, nOrderBy,
		(char*)pKeyMerge, P4_KEYINFO_HANDOFF);/*添加一个OP_Compare操作符，并将它的值作为一个指针*/
	sqlite3VdbeAddOp3(v, OP_Jump, addrAltB, addrAeqB, addrAgtB);/*将OP_Jump操作交给vdbe，然后返回这个操作的地址*/

	/* Release temporary registers
 *//*释放临时寄存器*/
	if (regPrev){
		sqlite3ReleaseTempRange(pParse, regPrev, nOrderBy + 1);/*释放regPrev这个连续寄存器，长度是ORDERBY表达式的长度加1*/
	}

	/* Jump to the this point in order to terminate the query.
	 *//*跳转到这指针,终止这个查询。*/
	sqlite3VdbeResolveLabel(v, labelEnd);/*解析labelEnd，labelEnd是一个标签，并作为下一条被插入的地址*/

	/* Set the number of output columns
	  *//*设置输出列的数量*/
	if (pDest->eDest == SRT_Output){{/*如果处理结果集中处理方式是否为SRT_Output（输出）*/
		Select *pFirst = pPrior;/*声明结构体pFirst，赋值为当前SELECT中优先查找pPrior*/
		while (pFirst->pPrior) pFirst = pFirst->pPrior;/*一直递归，找到叶子节点上的最优先的SELECT*/
		generateColumnNames(pParse, 0, pFirst->pEList);/*生成列名*/
	}

	/* Reassembly the compound query so that it will be freed correctly
	** by the calling function */
	/*重装复合查询,这样它将被调用的函数正确释放*/
	if (p->pPrior){/*如果SELECT中优先查找SELECT*/
		sqlite3SelectDelete(db, p->pPrior);/*释放Select结构体pPrior*/
	}
	p->pPrior = pPrior;/*将当前的pPrior赋值给SELECT中的pPrior*/

	/*** TBD:  Insert subroutine calls to close cursors on incomplete
	**** subqueries ****//*插入子程序调用游标关闭未完成的子查询*/
	explainComposite(pParse, p->op, iSub1, iSub2, 0);/*如果没有执行select，那么就会输出"COMPOSITE SUBQUERIES iSub1 and iSub2 (op)*/
	return SQLITE_OK;/*返回查询结束标记*/
}
#endif
//郭香丽结束

#if !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW)//宏定义
/* Forward Declarations *//*预先声明*/
//声明静态函数
static void substExprList(sqlite3*, ExprList*, int, ExprList*);
static void substSelect(sqlite3*, Select *, int, ExprList *);

/*
** Scan through the expression pExpr.  Replace every reference to
** a column in table number iTable with a copy of the iColumn-th
** entry in pEList.  (But leave references to the ROWID column unchanged。
** unchanged.)
**
** This routine is part of the flattening procedure.  A subquery
** whose result set is defined by pEList appears as entry in the
** FROM clause of a SELECT such that the VDBE cursor assigned to that
** FORM clause entry is iTable.  This routine make the necessary
** changes to pExpr so that it refers directly to the source table
** of the subquery rather the result set of the subquery.
*/



/*
** 通过表达式pExpr进行扫描，从表达式列表中复制第iColumn-th个条目，替换iTable中每个参照的列。
** （ROWID不做任何改变）
** 这个例程是拼合过程的一部分。子查询的结果集被表达式定义，表现为一个在SELECT中FROM子句的条目，
** 因此VDBE分配给FROM子句的游标是一个表。这个程序必须对表达式做出改变，才能直接引用子查询的源表而不是
** 子查询的结果集。
*/

static Expr *substExpr(
	sqlite3 *db,        /* Report malloc errors to this connection *//*定义数据库db,报告malloc错误连接*/
	Expr *pExpr,        /* Expr in which substitution occurs *//*定义pExpr变量，当替换发生*/
	int iTable,         /* Table to be substituted *//*替换的表号*/
	ExprList *pEList    /* Substitute expressions *//*替换的表达式列表，输出结果列的语法树*/
	)
{
	///*判断条件，若表达式为空，返回*/
	if (pExpr == 0) return 0;

	if (pExpr->op == TK_COLUMN && pExpr->iTable == iTable){

		if (pExpr->iColumn<0){//如果表达式列数小于0
			pExpr->op = TK_NULL;//返回TK_NULL（标记列为空）
		}
		else{
			Expr *pNew;//声明一个新的指向表达式的指针
			assert(pEList != 0 && pExpr->iColumn<pEList->nExpr);//插入断点，判断表达式不为空，并且列数小于表达式列表中表达式的个数，条件不成立则抛出
			assert(pExpr->pLeft == 0 && pExpr->pRight == 0);//异常处理，表达式的左右子树为空，条件不成立抛出断点
			pNew = sqlite3ExprDup(db, pEList->a[pExpr->iColumn].pExpr, 0);//深度copy表达式返回给pNew
			if (pNew && pExpr->pColl){//若pNew不为空且表达式的排序序列不为空成立，执行
				pNew->pColl = pExpr->pColl;//令新表达式的排序序列等于当前全局表达式的排序序列
			}
			sqlite3ExprDelete(db, pExpr);//删除数据库连接中的pExpr表达式
			pExpr = pNew;//将pNew赋值给当前全局表达式的排序序列
		}
	}
	else{
		pExpr->pLeft = substExpr(db, pExpr->pLeft, iTable, pEList);//递归调用自身函数，将表达式中左节点赋值给表达式pExpr的左节点
		pExpr->pRight = substExpr(db, pExpr->pRight, iTable, pEList);//递归调用自身函数，将表达式中右节点赋值给表达式pExpr的右节点
		if (ExprHasProperty(pExpr, EP_xIsSelect)){//调用函数ExprHasProperty判断表达式pExpr是否进行EP_xIsSelect查询
			substSelect(db, pExpr->x.pSelect, iTable, pEList);//调用substSelect函数，对pExpr->x.pSelect中Select语句进行处理
		}
		else{
			substExprList(db, pExpr->x.pList, iTable, pEList);//调用substExprList函数，对pExpr->x.pList表达式列表处理
		}
	}
	return pExpr;//返回相应的表达式指针
}
static void substExprList(
	sqlite3 *db,         /* Report malloc errors here *//*声明sqlite的对象，报告分配内存错误*/
	ExprList *pList,     /* List to scan and in which to make substitutes *//*定义pExpr变量，当替换发生扫描列表*/
	int iTable,          /* Table to be substituted *//*声明要替换的表号*/
	ExprList *pEList     /* Substitute values *//*替换的表达式列表，输出结果列的语法树*/
	){
	int i;//声明整变量i
	if (pList == 0) return;//表达式列表为空，返回
	//遍历表达式列表
	for (i = 0; i<pList->nExpr; i++){
		pList->a[i].pExpr = substExpr(db, pList->a[i].pExpr, iTable, pEList);//递归调用，重新赋值pList->a[i].pExpr
	}
}

static void substSelect(
	sqlite3 *db,         /* Report malloc errors here *//*声明sqlite的对象，报告分配内存错误*/
	Select *p,           /* SELECT statement in which to make substitutions *//*声明一个Select对象*/
	int iTable,          /* Table to be replaced *//*声明要替换的表号*/
	ExprList *pEList     /* Substitute values *//*替换的表达式列表，输出结果列的语法树*/
	){
	SrcList *pSrc;//声明描述SELECT中的FROM子句的对象，from子句语法树
	struct SrcList_item *pItem;//声明SrcList_item结构体的一个FROM子句对象
	int i;//声明整变量i
	if (!p) return;//若p不空，返回

	substExprList(db, p->pEList, iTable, pEList);	//调用substExprList函数，对 p->pEList表达式列表处理
	substExprList(db, p->pGroupBy, iTable, pEList);	//调用substExprList函数，对 p->pGroupBy表达式列表处理
	substExprList(db, p->pOrderBy, iTable, pEList);	//调用substExprList函数，对 p->pOrderBy表达式列表处理
	p->pHaving = substExpr(db, p->pHaving, iTable, pEList);//调用substExpr函数，赋值给SELECT中pHaving属性
	p->pWhere = substExpr(db, p->pWhere, iTable, pEList);//调用substExpr函数，赋值给SELECT中pWhere属性
	substSelect(db, p->pPrior, iTable, pEList);//递归调用substSelect函数
	pSrc = p->pSrc;//获取当前from子句中的p->pSrc
	assert(pSrc);  /* Even for (SELECT 1) we have: pSrc!=0 but pSrc->nSrc==0 *///满足条件，插入断点，用于异常处理
	if (ALWAYS(pSrc)){
		//遍历from子句表中的对象
		for (i = pSrc->nSrc, pItem = pSrc->a; i>0; i--, pItem++){
			substSelect(db, pItem->pSelect, iTable, pEList);//调用substSelect函数，对 pItem->x.pSelect中Select语句进行处理
		}
	}
}
#endif // !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW) 

#if !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW)
/*
** This routine attempts to flatten subqueries as a performance optimization.
** This routine returns 1 if it makes changes and 0 if no flattening occurs.
**
** To understand the concept of flattening, consider the following
** query:
**
**     SELECT a FROM (SELECT x+y AS a FROM t1 WHERE z<100) WHERE a>5
**
** The default way of implementing this query is to execute the
** subquery first and store the results in a temporary table, then
** run the outer query on that temporary table.  This requires two
** passes over the data.  Furthermore, because the temporary table
** has no indices, the WHERE clause on the outer query cannot be
** optimized.
**
** This routine attempts to rewrite queries such as the above into
** a single flat select, like this:
**
**     SELECT x+y AS a FROM t1 WHERE z<100 AND a>5
**
** The code generated for this simpification gives the same result
** but only has to scan the data once.  And because indices might
** exist on the table t1, a complete scan of the data might be
** avoided.
**
** Flattening is only attempted if all of the following are true:
**
**   (1)  The subquery and the outer query do not both use aggregates.
**
**   (2)  The subquery is not an aggregate or the outer query is not a join.
**
**   (3)  The subquery is not the right operand of a left outer join
**        (Originally ticket #306.  Strengthened by ticket #3300)
**
**   (4)  The subquery is not DISTINCT.
**
**   (5)  At one point restrictions (4) and (5) defined a subset of DISTINCT
**        sub-queries that were excluded from this optimization. Restriction
**        (4) has since been expanded to exclude all DISTINCT subqueries.
**
**   (6)  The subquery does not use aggregates or the outer query is not
**        DISTINCT.
**
**   (7)  The subquery has a FROM clause.  TODO:  For subqueries without
**        A FROM clause, consider adding a FROM close with the special
**        table sqlite_once that consists of a single row containing a
**        single NULL.
**
**   (8)  The subquery does not use LIMIT or the outer query is not a join.
**
**   (9)  The subquery does not use LIMIT or the outer query does not use
**        aggregates.
**
**  (10)  The subquery does not use aggregates or the outer query does not
**        use LIMIT.
**
**  (11)  The subquery and the outer query do not both have ORDER BY clauses.
**
**  (**)  Not implemented.  Subsumed into restriction (3).  Was previously
**        a separate restriction deriving from ticket #350.
**
**  (13)  The subquery and outer query do not both use LIMIT.
**
**  (14)  The subquery does not use OFFSET.
**
**  (15)  The outer query is not part of a compound select or the
**        subquery does not have a LIMIT clause.
**        (See ticket #2339 and ticket [02a8e81d44]).
**
**  (16)  The outer query is not an aggregate or the subquery does
**        not contain ORDER BY.  (Ticket #2942)  This used to not matter
**        until we introduced the group_concat() function.
**
**  (17)  The sub-query is not a compound select, or it is a UNION ALL
**        compound clause made up entirely of non-aggregate queries, and
**        the parent query:
**
**          * is not itself part of a compound select,
**          * is not an aggregate or DISTINCT query, and
**          * is not a join
**
**        The parent and sub-query may contain WHERE clauses. Subject to
**        rules (11), (13) and (14), they may also contain ORDER BY,
**        LIMIT and OFFSET clauses.  The subquery cannot use any compound
**        operator other than UNION ALL because all the other compound
**        operators have an implied DISTINCT which is disallowed by
**        restriction (4).
**
**        Also, each component of the sub-query must return the same number
**        of result columns. This is actually a requirement for any compound
**        SELECT statement, but all the code here does is make sure that no
**        such (illegal) sub-query is flattened. The caller will detect the
**        syntax error and return a detailed message.
**
**  (18)  If the sub-query is a compound select, then all terms of the
**        ORDER by clause of the parent must be simple references to
**        columns of the sub-query.
**
**  (19)  The subquery does not use LIMIT or the outer query does not
**        have a WHERE clause.
**
**  (20)  If the sub-query is a compound select, then it must not use
**        an ORDER BY clause.  Ticket #3773.  We could relax this constraint
**        somewhat by saying that the terms of the ORDER BY clause must
**        appear as unmodified result columns in the outer query.  But we
**        have other optimizations in mind to deal with that case.
**
**  (21)  The subquery does not use LIMIT or the outer query is not
**        DISTINCT.  (See ticket [752e1646fc]).
**
** In this routine, the "p" parameter is a pointer to the outer query.
** The subquery is p->pSrc->a[iFrom].  isAgg is true if the outer query
** uses aggregates and subqueryIsAgg is true if the subquery uses aggregates.
**
** If flattening is not attempted, this routine is a no-op and returns 0.
** If flattening is attempted this routine returns 1.
**
** All of the expression analysis must occur on both the outer query and
** the subquery before this routine runs.
*/
/* 此例程尝试以扁平化子查询作为一种性能优化。如果程序发生了改变，程序返回1，若没有扁平化操作，则程序返回0.
**
** 要理解扁平化的概念，要考虑以下的查询：
**   SELECT a FROM (SELECT x+y AS a FROM t1 WHERE z<100) WHERE a>5
**默认先执行内查询，把结果放到一个临时表，再对这个表进行外部查询，这就要对数据进行两次处理，
**由于该临时表无索引，所以对外部查询就不能进行优化了
** 这个程序尝试重写查询，根据上面的SQL语句，作为像这样的一个单独的扁平化查询：
**   SELECT a FROM (SELECT x+y AS a FROM t1 WHERE z<100) WHERE a>5
** 代码给出同样的结果但只需要扫描一次数据。因为索引可能在表t1上存在，可能避免一次完成的数据扫描。
**
** 若能满足以下规则，就能实现扁平化查询：
**（1）子查询和外查询不能同时使用聚合函数
**（2）子查询不是一个聚合或外部查询不是一个联接
**（3）子查询不是左外连接的右操作集
**（4）子查询不使用“DISTINCT”
**（5）规则（4）和（5）定义了DISTINCT子查询的子集，排除了这种优化的子查询。规则（4）已经排除了所有的DISTINCT子查询
**（6）子查询不能使用聚集或外部查询不能使用DISTINCT
**（7）子查询有一个FROM子句。TODO：子查询没有一个FROM子句，考虑添加一个特殊的表sqlite_once，它只包含了一个单一的空值的行。
**（8）子查询不使用限制或外部查询不是一个联接
**（9）子查询不使用限制或外部查询不使用聚集
**（10）子查询不使用聚合或外部查询不使用限制
**（11）子查询和外部查询不能同时使用ORDER BY子句
**（12）没有实现，归入到限制条件（3）。
**（13）子查询和外部查询不能同时使用LIMIT子句
**（14）子查询不能使用OFFSET子句
**（15）外部查询不能是一个复合查询的一部分或者子查询不能有LIMIT子句
**（16）外部查询不能是一个聚集查询或子查询不能包含orderby子句。除非有group_concat（）函数才能实现匹配。
**（17）子查询不是一个复合查询，或者UNION ALL复合查询有非聚集查询组成的查询语句，另外有以下的父查询：
**      *本身不是复合查询
**      *不是一个聚集查询或DISTINCT查询
**      *没有连接操作
** 父查询或子查询可能包含一个WHERE子句。这些服从规则（11），（13）和（14），它可能也包含 ORDER BY, LIMIT 和OFFSET 子句.
** 子查询不能使用任何符合操作除了UNION ALL,因为所有的复合操作是都是实现了DISTINCT，规则（4）不允许的。
** 此外，每一个子查询的组件必须返回数量的结果列。这个实际上要求复合查询中子查询没有扁平化的。调用将检测到语法错误和返回详细信息
**
**（18）如果子查询是复合查询，父查询中ORDERBY子句所有规则必须简单依赖引用子查询的列。
**（19）子查询不能使用LIMIT或外查询不能使用WHERE子句
**（20）如果子查询是复合查询，不能使用ORDER BY 子句。释放这些约束条件，ORDER BY 子句必须显示为外部查询中的未修改的结果列。
在打算如何处理这种情况下有其他优化。
**（21）子查询不能使用LIMIT或者外查询不能使用DISTINCT。
**
**在这个程序中，参数p是一个指针，指向外查询，子查询p->pSrc->a[iFrom].如果外查询使用了聚集isAgg为TRUE，如果
**子查询使用了聚集，subqueryIsAgg为TRUE。
**
**如果不能进行扁平化，那么这个程序不执行，并返回0。
**如果使用扁平化了，返回1.
**
*表达式分析必须发生在程序运行之前的外部查询和子查询中。
*/
static int flattenSubquery(
	Parse *pParse,       /* Parsing context *//*声明解析上下文的变量（解析器）语义分析*/
	Select *p,           /* The parent or outer SELECT statement *//*声明一个父查询或外部查询的变量*/
	int iFrom,           /* Index in p->pSrc->a[] of the inner subquery *//*内部查询中p->pSrc->a[]中的索引*/
	int isAgg,           /* True if outer SELECT uses aggregate functions *//*外部查询如果用了聚集函数，则为TRUE*/
	int subqueryIsAgg    /* True if the subquery uses aggregate functions *//*子查询如果使用了聚集函数，则为TRUE*/
	){
	const char *zSavedAuthContext = pParse->zAuthContext;/*将语法解析树中的上下文赋值给zSavedAuthContext*/
	Select *pParent;
	Select *pSub;       /* The inner query or "subquery" *//*声明变量 pSub用来表明内查询或子查询变量*/
	Select *pSub1;      /* Pointer to the rightmost select in sub-query *//*声明pSub1变量，用来表示指向子查询中最右边的查询*/
	SrcList *pSrc;      /* The FROM clause of the outer query *//*声明Srclist的对象，表明外部查询的FROM子句*/
	SrcList *pSubSrc;   /* The FROM clause of the subquery *//*声明Srclist的对象，用来表示子查询的FROM子句*/
	ExprList *pList;    /* The result set of the outer query *//*声明表达式Exprlist的变量，表示外部查询的结果集*/
	int iParent;        /* VDBE cursor number of the pSub result set temp table *//*声明整数类型的变量，用来上表示VDBE游标号，指向内查询的临时表*/
	int i;              /* Loop counter *//*循环计数*/
	Expr *pWhere;                    /* The WHERE clause *//*用来表示WHERE子句，where部分语法树*/
	struct SrcList_item *pSubitem;   /* The subquery *//*声明一个SrcList_item类型的子查询结构体*/
	sqlite3 *db = pParse->db;//声明一个sqlite3的对象，表示数据库连接

	/* Check to see if flattening is permitted.  Return 0 if not.
	*//*检查是否允许扁平化，不允许则返回0*/
	assert(p != 0);//SELECT结构体为空，抛出异常，这里是设置了断点，用于处理异常
	assert(p->pPrior == 0);  // Unable to flatten compound queries *//*不能扁平化的复合查询，加入断点，异常处理
	if (db->flags & SQLITE_QueryFlattener) return 0;//如果数据连接中值为SQLITE_QueryFlattener且标记变量db->flags均符合条件，执行下一步 
	pSrc = p->pSrc;/*获取查询结构体中from子句，子句的外部查询*/
	assert(pSrc && iFrom >= 0 && iFrom<pSrc->nSrc);//插入断点，如果pSrc为空或FROM索引小于0或FROM索引大于等于FROM个数，就抛出警告信息
	pSubitem = &pSrc->a[iFrom];//FROM子句中数组索引地址赋值给子查询pSubitem
	iParent = pSubitem->iCursor;//获取子查询的游标号
	pSub = pSubitem->pSelect;//获取当前的子查询或内查询
	assert(pSub != 0);//异常处理，加入断点，如果子查询结构体为空，抛出警告信息
	if (isAgg && subqueryIsAgg) return 0;                 /* Restriction (1)  *//*规则（1）子查询和外部查询使用了聚集函数，程序直接返回*/
	if (subqueryIsAgg && pSrc->nSrc>1) return 0;          /* Restriction (2)  *//*规则（2）*///使用聚集函数或者from的子句个数>1
	pSubSrc = pSub->pSrc;//获取子查询的from语句，子句的子查询
	assert(pSubSrc);/*异常处理，加入断点，如果pSubSrc为空，抛出错误信息*/
	/* Prior to version 3.1.2, when LIMIT and OFFSET had to be simple constants,
	** not arbitrary expresssions, we allowed some combining of LIMIT and OFFSET
	** because they could be computed at compile-time.  But when LIMIT and OFFSET
	** became arbitrary expressions, we were forced to add restrictions (13)
	** and (14). */
	/*之前版本 3.1.2，限制和偏移量必须是简单的常量而不是任意的表达式，我们允许联合LIMIT 和 OFFSET
	**因为他们可以在编译时计算。但是当LIMIT 和 OFFSET为任意的表达式，我们被迫使用规则（13）和
	**规则（14）。
	*/
	if (pSub->pLimit && p->pLimit) return 0;              /* Restriction (13) *//*规则（13）*///子查询和外部查询不能使用limit
	if (pSub->pOffset) return 0;                          /* Restriction (14) *//*规则（14）*///子查询不能使用offset语句
	if (p->pRightmost && pSub->pLimit){/**/
		return 0;                                            /* Restriction (15) *//*规则（15）*///子查询不能有limit子句
	}
	if (pSubSrc->nSrc == 0) return 0;                       /* Restriction (7)  *//*规则（7）*///判断子查询是否有from子句
	if (pSub->selFlags & SF_Distinct) return 0;           /* Restriction (5)  *//*规则（5）*///含有distinct子句，直接返回
	if (pSub->pLimit && (pSrc->nSrc>1 || isAgg)){
		return 0;         /* Restrictions (8)(9) *//*规则（8）（9）*///子查询不使用聚集或连接操作，或limit子句
	}
	if ((p->selFlags & SF_Distinct) != 0 && subqueryIsAgg){
		return 0;         /* Restriction (6)  *//*规则（6）*///子查询不使用聚集函数，外部查询不使用distinct子句
	}
	if (p->pOrderBy && pSub->pOrderBy){
		return 0;                                           /* Restriction (11) *//*规则（11）*///子查询和外部查询不能同时使用orderby子句
	}
	if (isAgg && pSub->pOrderBy) return 0;                /* Restriction (16) *//*规则（16）*///外部查询不能使聚集查询，子查询不能含有orderby子句
	if (pSub->pLimit && p->pWhere) return 0;              /* Restriction (19) *//*规则（19）*///子查询不含limit子句，外部查询不含有where子句
	if (pSub->pLimit && (p->selFlags & SF_Distinct) != 0){
		return 0;         /* Restriction (21) *//*规则（21）*///子查询不能含有limit，外部查询不能有distinct语句
	}

  /* OBSOLETE COMMENT 1:过时注释1：
	  ** Restriction 3:  If the subquery is a join, make sure the subquery is 
	  ** not used as the right operand of an outer join.  Examples of why this
	  ** is not allowed:
	  **
	  **         t1 LEFT OUTER JOIN (t2 JOIN t3)
	  **
	  ** If we flatten the above, we would get
	  **
	  **         (t1 LEFT OUTER JOIN t2) JOIN t3
	  **
	  ** which is not at all the same thing.
	  **
	  ** OBSOLETE COMMENT 2:
	  ** Restriction 12:  If the subquery is the right operand of a left outer
	  ** join, make sure the subquery has no WHERE clause.
	  ** An examples of why this is not allowed:
	  **
	  **         t1 LEFT OUTER JOIN (SELECT * FROM t2 WHERE t2.x>0)
	  **
	  ** If we flatten the above, we would get
	  **
	  **         (t1 LEFT OUTER JOIN t2) WHERE t2.x>0
	  **
	  ** But the t2.x>0 test will always fail on a NULL row of t2, which
	  ** effectively converts the OUTER JOIN into an INNER JOIN.
	  **
	  ** THIS OVERRIDES OBSOLETE COMMENTS 1 AND 2 ABOVE:
	  ** Ticket #3300 shows that flattening the right term of a LEFT JOIN
	  ** is fraught with danger.  Best to avoid the whole thing.  If the
	  ** subquery is the right term of a LEFT JOIN, then do not flatten.
	  */
	  /* 过时的注释 1：
	  ** 规则3：如果子查询是一种联接，请确保子查询是不用作外部联接的右操作数。
	  **     t1 LEFT OUTER JOIN (t2 JOIN t3)
	  ** 若对表达式进行扁平化，我们得到
	  **      (t1 LEFT OUTER JOIN t2) JOIN t3
	  ** 这两个表达式表示是不相同了。
	  ** 过时的注释 2：
	  ** 规则（12），如果子查询是左外部的右操作数，请确保子查询有没有 WHERE 子句。
	  ** 下面的例子不能使用：
	  **  1 LEFT OUTER JOIN (SELECT * FROM t2 WHERE t2.x>0)
	  **若对上面的语句扁平化：
	  **  (t1 LEFT OUTER JOIN t2) WHERE t2.x>0
	  ** 测试将始终失败在 t2，因为在一个NULL或者t2的行，有效的将OUTER JOIN转化为INNER JOIN.
	  **
	  ** 重写过时的注释1，2，扁平化左连接的右子树是不安全的。。
	  ** 若子查询是左连接的右子树，将不会扁平化。
	  */
	  if( (pSubitem->jointype & JT_OUTER)!=0 ){/*if判断条件，若外查询使用jointype*/
		return  0;/*直接返回*/
	  }

	  /* Restriction 17: If the sub-query is a compound SELECT, then it must
	  ** use only the UNION ALL operator. And none of the simple select queries
	  ** that make up the compound SELECT are allowed to be aggregate or distinct
	  ** queries.规则17：


	  *//*如果子查询是一个复合的选择，那么它必须使用只有 UNION ALL 运算符，没有一个简单的选择查询组成在这个复合查询的子查询中，没有使用聚集函数和去除重复*/
	  if( pSub->pPrior ){//判断子查询是否有优先查询
		if( pSub->pOrderBy ){/*若子查询含有OrderBy子句*/
		  return 0;  /* 规则 20 直接返回*/
		}
		if( isAgg || (p->selFlags & SF_Distinct)!=0 || pSrc->nSrc!=1 ){/*如果外部查询使用了聚集函数，没有重复排序或者FROM表不等于1*/
		  return 0;/*直接返回*/
		}
		for(pSub1=pSub; pSub1; pSub1=pSub1->pPrior){/*遍历子查询中最右边的查询*/
		  testcase( (pSub1->selFlags & (SF_Distinct|SF_Aggregate))==SF_Distinct );/*测试Distinct的使用*/
		  testcase( (pSub1->selFlags & (SF_Distinct|SF_Aggregate))==SF_Aggregate );/*测试Aggregate的使用*/
		  assert( pSub->pSrc!=0 );/*异常处理，加入断点*/
		  if( (pSub1->selFlags & (SF_Distinct|SF_Aggregate))!=0//若子查询含有Distinct或Aggregate（聚集函数）且标记变量=1
		   || (pSub1->pPrior && pSub1->op!=TK_ALL) //子查询中含优先查询SELECT并且操作为TK_ALL
		   || pSub1->pSrc->nSrc<1//子查询中FROM子句中表达式且个数<1*/
		   || pSub->pEList->nExpr!=pSub1->pEList->nExpr//子查询中表达式的个数！=子查询中右边查询的表达式个数
		  ){
			return 0;//返回0
		  }
		  testcase( pSub1->pSrc->nSrc>1 );//调用testcase对子查询的FROM子句中表达式个数>1的测试
		}

		/* Restriction 18. *//*规则（18）*/
		if( p->pOrderBy ){//若ORDERBY子句为真
		  int ii;//声明变量ii
		  for(ii=0; ii<p->pOrderBy->nExpr; ii++){//遍历orderby表达式个数
			if( p->pOrderBy->a[ii].iOrderByCol==0 ) return 0;//值为空，返回。
		  }
		}
	  }

	  /***** If we reach this point, flattening is permitted. *****/
      /*若能执行到这一步，允许扁平化*/
	  /* Authorize the subquery *//*授权允许子查询*/
	  pParse->zAuthContext = pSubitem->zName;//子查询的名字赋值给语法解析树的已经授权的上下文属性，进行授权验证
	  TESTONLY(i =) sqlite3AuthCheck(pParse, SQLITE_SELECT, 0, 0, 0);/**///检查授权属性
	  testcase( i==SQLITE_DENY );//调用testcase测试i是否为SQLITE_DENY，错误类型将不被授权，错误数和错误pParse里适当修改的消息
	  pParse->zAuthContext = zSavedAuthContext;//语法解析树中的上下文赋值给语法解析树的授权上下文属性

	  /* If the sub-query is a compound SELECT statement, then (by restrictions
	  ** 17 and 18 above) it must be a UNION ALL and the parent query must 
	  ** be of the form:
	  **
	  **     SELECT <expr-list> FROM (<sub-query>) <where-clause> 
	  **
	  ** followed by any ORDER BY, LIMIT and/or OFFSET clauses. This block
	  ** creates N-1 copies of the parent query without any ORDER BY, LIMIT or 
	  ** OFFSET clauses and joins them to the left-hand-side of the original
	  ** using UNION ALL operators. In this case N is the number of simple
	  ** select statements in the compound sub-query.
	  **
	  ** Example:
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
	  ** We call this the "compound-subquery flattening".
	  */
	  /*如果子查询是一个复合 SELECT 语句，然后根据规则17和规则18必须是UNION ALL 并且父查询的格式如下：
	  **    SELECT <expr-list> FROM (<sub-query>) <where-clause> 
	  **其次是任何 ORDER BY, LIMIT 或 OFFSET 子句。这一块创建了n-1个副本的不含ORDER BY, LIMIT 或 OFFSET父查询对象 
	  **并且连接到原始使用的UNION ALL操作对象。N是复合查询中简单查询的编号。
	  **请看例子：
	  **     SELECT a+1 FROM (
	  **        SELECT x FROM tab
	  **        UNION ALL
	  **        SELECT y FROM tab
	  **        UNION ALL
	  **        SELECT abs(z*2) FROM tab2
	  **     ) WHERE a!=5 ORDER BY 1
	  **转化成：
	  **     SELECT x+1 FROM tab WHERE x+1!=5
	  **     UNION ALL
	  **     SELECT y+1 FROM tab WHERE y+1!=5
	  **     UNION ALL
	  **     SELECT abs(z*2)+1 FROM tab2 WHERE abs(z*2)+1!=5
	  **     ORDER BY 1
	  **我们称这为“扁平化复合子查询”
	  */
	  //循环遍历子查询
	  for(pSub=pSub->pPrior; pSub; pSub=pSub->pPrior){//遍历子查询中的优先查询
		Select *pNew;//声明一个SELECT结构体的对象
		ExprList *pOrderBy = p->pOrderBy;//S表达式列表pOrderBy的赋值
		Expr *pLimit = p->pLimit;//对表达式的pLimit属性的赋值（select结构体）
		Select *pPrior = p->pPrior;//优先查找重新赋值给新的变量
	
		p->pOrderBy = 0;//初始化
		p->pSrc = 0;//初始化
		p->pPrior = 0;//初始化
		p->pLimit = 0;//初始化
		pNew = sqlite3SelectDup(db, p, 0);//深拷贝
		//变量赋值给select结构体成员
		p->pLimit = pLimit;//获取limit语句
		p->pOrderBy = pOrderBy;//获取orderby语句
		p->pSrc = pSrc;//获取from子句
		p->op = TK_ALL;//操作符属性设置为tk_all
		p->pRightmost = 0;//最右边查询的初始化操作
                     //如果pnew为空
		if( pNew==0 ){
		  pNew = pPrior;//SELECT结构体中优先查询的赋值给pNew
		}
		
		else
		{
		  pNew->pPrior = pPrior;//select->pPrior赋值给pNew->pPrior
		  pNew->pRightmost = 0;//pNew指向的最右边置为0
		}
		p->pPrior = pNew;//完成最终赋值，优先查找
		if( db->mallocFailed ) return 1;//内存分配失败
	  }

	  /* Begin flattening the iFrom-th entry of the FROM clause 
	  ** in the outer query.
	  *//*在外查询中压缩FROM子句的第iFrom个条目*/
	  pSub = pSub1 = pSubitem->pSelect;//赋值语句，子查询
	  /* Delete the transient table structure associated with the
	  ** subquery
	  *//*删除和子查询相关联的临时表结构*/
	  //内存的释放
	  sqlite3DbFree(db, pSubitem->zDatabase);//数据库模块的内存
	  sqlite3DbFree(db, pSubitem->zName);//子查询名字属性
	  sqlite3DbFree(db, pSubitem->zAlias);//子查询依赖关系
	 //将临时表中的数据清零
	  pSubitem->zDatabase = 0;//清零
	  pSubitem->zName = 0;//清零
	  pSubitem->zAlias = 0;//清零
	  pSubitem->pSelect = 0;//清零

	  /* Defer deleting the Table object associated with the
	  ** subquery until code generation is
	  ** complete, since there may still exist Expr.pTab entries that
	  ** refer to the subquery even after flattening.  Ticket #3346.
	  **
	  ** pSubitem->pTab is always non-NULL by test restrictions and tests above.
	  *//*延迟删除与关联的表对象，直到子查询生成代码完成，因为那里可能仍然存在 Expr.pTab 扁平化后的子查询*/

	  if( ALWAYS(pSubitem->pTab!=0) ){//子查询项的pTab不空
		Table *pTabToDel = pSubitem->pTab;//获取子查询的pTab
		if( pTabToDel->nRef==1 ){
		  Parse *pToplevel = sqlite3ParseToplevel(pParse);//最顶层解析
		  pTabToDel->pNextZombie = pToplevel->pZombieTab;//解析后的域赋值
		  pToplevel->pZombieTab = pTabToDel;//类似链表结构赋值
		}else{
		  pTabToDel->nRef--;//自减
		}
		pSubitem->pTab = 0;//置0
	  }

	  /* The following loop runs once for each term in a compound-subquery
	  ** flattening (as described above).  If we are doing a different kind
	  ** of flattening - a flattening other than a compound-subquery flattening -
	  ** then this loop only runs once.
	  **
	  ** This loop moves all of the FROM elements of the subquery into the
	  ** the FROM clause of the outer query.  Before doing this, remember
	  ** the cursor number for the original outer query FROM element in
	  ** iParent.  The iParent cursor will never be used.  Subsequent code
	  ** will scan expressions looking for iParent references and replace
	  ** those references with expressions that resolve to the subquery FROM
	  ** elements we are now copying in.
	  */
	  /* 下面的循环一次性的将符合查询中每个条款都进行扁平化操作。若我们做了另一种方式的扁平化操作--
	  ** 这种扁平化操作并非复合子查询的扁平化，该循环仅仅运行一次。
	  **
	  ** 这个循环将子查询中的FROM子句的所有元素都移动到外部查询的FROM子句中。在这之前，记住父查询中原始外部查询
	  ** FROM子句的游标号。父查询的游标号没有用过。后面的代码 将扫描查找 iParent 引用的表达式和替换，这些引用解析我们现在正在复制的子查询的表达式元素。
	  */
	  //遍历外部查询，将子查询中的FROM子句的所有元素都移动到外部查询的FROM子句中
	  for(pParent=p; pParent; pParent=pParent->pPrior, pSub=pSub->pPrior){
		int nSubSrc;//声明整型变量nsubsrc
		u8 jointype = 0; //自定义类型变量的声明
		pSubSrc = pSub->pSrc;     /* FROM clause of subquery *//*from子查询，子句的子查询*/
		nSubSrc = pSubSrc->nSrc;  /* Number of terms in subquery FROM clause *//*from子句的数目*/
		pSrc = pParent->pSrc;     /* FROM clause of the outer query *//*外部查询的FROM子句*/

		if( pSrc ){//外部查询有from子句
		  assert( pParent==p );  // First time through the loop *//*第一次循环，插入断点，若有异常，则做相关处理*/
		  jointype = pSubitem->jointype;//子查询的连接
		}else{
		  assert( pParent!=p );  // 2nd and subsequent times through the loop *//*条件不成立，同样插入断点，异常处理*/
		  pSrc = pParent->pSrc = sqlite3SrcListAppend(db, 0, 0, 0);//追加from子句
		  if( pSrc==0 ){//如果外部查询的from子句为0
			assert( db->mallocFailed );//异常处理，检查内存问题
			break;//跳出本次循环
		  }
		}

		/* The subquery uses a single slot of the FROM clause of the outer
		** query.  If the subquery has more than one element in its FROM clause,
		** then expand the outer query to make space for it to hold all elements
		** of the subquery.
		**
		** Example:
		**
		**    SELECT * FROM tabA, (SELECT * FROM sub1, sub2), tabB;
		**
		** The outer query has 3 slots in its FROM clause.  One slot of the
		** outer query (the middle slot) is used by the subquery.  The next
		** block of code will expand the out query to 4 slots.  The middle
		** slot is expanded to two slots in order to make space for the
		** two elements in the FROM clause of the subquery.
		*/
		/*
		** 子查询使用 FROM 子句的外部查询。 如果子查询在其 FROM 子句中有多个元素，
		展开该外部查询，以使它持有的所有元素的空间 。**
		** 例子：
		**      SELECT * FROM tabA, (SELECT * FROM sub1, sub2), tabB;
		** 外部有3个入口在from子句中。中间的入口是子查询。下一个代码块将外查询扩大为4个入口。
		** 为确保子查询的空间，中间入口扩展位2个。
		*/
		if( nSubSrc>1 ){//子查询from子句超过一个
		  pParent->pSrc = pSrc = sqlite3SrcListEnlarge(db, pSrc, nSubSrc-1,iFrom+1);//扩充外部查询入口
		  if( db->mallocFailed ){//内存分配失败
			break;
		  }
		}

		/* Transfer the FROM clause terms from the subquery into the
		** outer query.
		*//*子查询中的FROM子句转到外查询*/
		//遍历所有子查询中的from子句
		for(i=0; i<nSubSrc; i++){
		  sqlite3IdListDelete(db, pSrc->a[i+iFrom].pUsing);//从数组删除已经处理过的from子句
		  pSrc->a[i+iFrom] = pSubSrc->a[i];//补全数组
		  memset(&pSubSrc->a[i], 0, sizeof(pSubSrc->a[i]));//从pSubSrc->a[i]所指的地址开始，将pSubSrc->a[i]的前sizeof(pSubSrc->a[i])个字节用0替换
		}
		pSrc->a[iFrom].jointype = jointype;//获取当前from子句连接
	  
		/* Now begin substituting subquery result set expressions for 
		** references to the iParent in the outer query.
		** 
		** Example:
		**
		**   SELECT a+5, b*10 FROM (SELECT x*3 AS a, y+10 AS b FROM t1) WHERE a>b;
		**   \                     \_____________ subquery __________/          /
		**    \_____________________ outer query ______________________________/
		**
		** We look at every expression in the outer query and every place we see
		** "a" we substitute "x*3" and every place we see "b" we substitute "y+10".
		*/
		/* 现在开始取代子查询结果集表达式，引用外部查询的iParent（父查询）。
		** 例如：
		** SELECT a+5, b*10 FROM (SELECT x*3 AS a, y+10 AS b FROM t1) WHERE a>b;
		**   \                     \_____________ subquery __________/          /
		**    \_____________________ outer query ______________________________/
		**我们看看每个外部查询中的表达式，我们看到每个地方  'a' 代替 'x * 3'，每个地方，我们看到 'b' 我们代替' y+10'
		*/
		pList = pParent->pEList;//外部查询表达式列表的获取
		//遍历所有的表达式列表
		for(i=0; i<pList->nExpr; i++){
		  if( pList->a[i].zName==0 ){//若表达式不存在
			const char *zSpan = pList->a[i].zSpan;//赋值
			if( ALWAYS(zSpan) ){
			  pList->a[i].zName = sqlite3DbStrDup(db, zSpan);//深拷贝
			}
		  }
		}
		substExprList(db, pParent->pEList, iParent, pSub->pEList);//获取pParent->pEList给pSub->pEList
		if( isAgg ){//如果外部查询使用了聚集函数
		  substExprList(db, pParent->pGroupBy, iParent, pSub->pEList);//获取pParent->pGroupBy给pSub->pEList
		  pParent->pHaving = substExpr(db, pParent->pHaving, iParent, pSub->pEList);//获取pParent->pHaving表达式给pSub->pEList
		}
		if( pSub->pOrderBy ){//子查询中含OrderBy子句
		  assert( pParent->pOrderBy==0 );//插入断点，异常处理
		  pParent->pOrderBy = pSub->pOrderBy;//子查询中pOrderBy赋值给父查询中的pOrderBy子句进行处理
		  pSub->pOrderBy = 0;//处理完毕清空pSub->pOrderBy
		}else if( pParent->pOrderBy ){//若父查询有pOrderBy子句
		  substExprList(db, pParent->pOrderBy, iParent, pSub->pEList);//获取pParent->pOrderBy给pSub->pEList
		}
		if( pSub->pWhere ){//含有where子句
		  pWhere = sqlite3ExprDup(db, pSub->pWhere, 0);//深拷贝
		}else{
		  pWhere = 0;//没有where子句，置0
		}
		if( subqueryIsAgg ){//若含有聚集函数
			//聚集函数的处理
		  assert( pParent->pHaving==0 );//异常断点处理
		  pParent->pHaving = pParent->pWhere;//where子句在having中处理
		  pParent->pWhere = pWhere;//当前的where子句赋值给父查询中
		  pParent->pHaving = substExpr(db, pParent->pHaving, iParent, pSub->pEList);//获取pParent->pHaving给SELECT中pWhere
		  pParent->pHaving = sqlite3ExprAnd(db, pParent->pHaving,
									  sqlite3ExprDup(db, pSub->pHaving, 0));//父查询pHaving子句与子查询pHaving子句，赋值给父查询的pHaving
		  assert( pParent->pGroupBy==0 );//插入断点，如果父查询中含GroupBy抛出异常
		  pParent->pGroupBy = sqlite3ExprListDup(db, pSub->pGroupBy, 0);//深度拷贝
		}else{
		  pParent->pWhere = substExpr(db, pParent->pWhere, iParent, pSub->pEList);//获取pParent->pWhere给pSub->pEList
		  pParent->pWhere = sqlite3ExprAnd(db, pParent->pWhere, pWhere);//where子句相连接
		}
	  
		/* The flattened query is distinct if either the inner or the
		** outer query is distinct. 
		*//*如果内查询和外查询都是唯一的，扁平化查询也是唯一的。
		pParent->selFlags |= pSub->selFlags & SF_Distinct;
	  /*位运算*/
		/*
		** SELECT ... FROM (SELECT ... LIMIT a OFFSET b) LIMIT x OFFSET y;
		**
		** One is tempted to try to add a and b to combine the limits.  But this
		** does not work if either limit is negative.
		*//*结合limit中使用a和b，但limit是负数时，不起作用
		if( pSub->pLimit ){//子查询中含有limit子句
		  pParent->pLimit = pSub->pLimit;//子查询赋给父查询的limit
		  pSub->pLimit = 0;//置0
		}
	  }
	  
	  /* Finially, delete what is left of the subquery and return
	  ** success.
	  *//*最后删除左边的子查询，成功返回*/
	  sqlite3SelectDelete(db, pSub1);/*调用数据库删除函数*/

	  return 1;//表示执行通过
	}
	#endif /* !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW) */

/*
** Analyze the SELECT statement passed as an argument to see if it
** is a min() or max() query. Return WHERE_ORDERBY_MIN or WHERE_ORDERBY_MAX if 
** it is, or 0 otherwise. At present, a query is considered to be
** a min()/max() query if:
**分析参数传递的 SELECT语句来看它是否是一个最小值或最大值查询。如果是，则返回WHERE_ORDERBY_MIN或WHERE_ORDERBY_MAX，
否则返回0.目前，一个查询如果满足下列条件则认为它是一个最小或最大值查询：
**   1. There is a single object in the FROM clause.
**   2. There is a single expression in the result set, and it is
**      either min(x) or max(x), where x is a column reference.
**   1.在From子句中有一个单一对象。
**   2.在结果集中有一个单一表达式，并且它要么最小值，要么最大值，其中x为一个列参考。*/
static u8 minMaxQuery(Select *p){
  Expr *pExpr; //初始化一个表达式
  ExprList *pEList = p->pEList; //初始化表达式列表

  if( pEList->nExpr!=1 ) return WHERE_ORDERBY_NORMAL;  //列表中不是只有一个表达式返回0，即并非单一对象，不做min()处理也不做max()处理
  pExpr = pEList->a[0].pExpr; //将表达式列表中的第一个表达式赋给pExpr
  if( pExpr->op != TK_AGG_FUNCTION ) return 0; //如果表达式的操作码不等于TK_AGG_FUNCTION，则返回0
/*
  if( NEVER((pExpr->flags & EP_xIsSelect) == EP_xIsSelect))
  pExpr->flags == EP_*
*/
  if( NEVER(ExprHasProperty(pExpr, EP_xIsSelect)) ) return 0; //判断表达式的flags是否满足x.pSelect是有效的
  pEList = pExpr->x.pList;
  if( pEList==0 || pEList->nExpr!=1 ) return 0;
  if( pEList->a[0].pExpr->op!=TK_AGG_COLUMN ) return WHERE_ORDERBY_NORMAL; //如果新的表达式的操作码不等于TK_AGG_COLUMN，则返回0
  assert( !ExprHasProperty(pExpr, EP_IntValue) ); //设置断点，判断ExprHasProperty(pExpr, EP_IntValue) == false是否成立
  if( sqlite3StrICmp(pExpr->u.zToken,"min")==0 ){ //如果表达式的标记值等于min则返回WHERE_ORDERBY_MIN
    return WHERE_ORDERBY_MIN;
  }else if( sqlite3StrICmp(pExpr->u.zToken,"max")==0 ){//如果表达式的标记值等于max则返回WHERE_ORDERBY_MAX
    return WHERE_ORDERBY_MAX;
  }
  return WHERE_ORDERBY_NORMAL; //否则返回0，不做min()处理也不做max()处理
}

	/*
	** The select statement passed as the first argument is an aggregate query.
	** The second argument is the associated aggregate-info object. This 
	** function tests if the SELECT is of the form:
	**
	**   SELECT count(*) FROM <tbl>
	**
	** where table is a database table, not a sub-select or view. If the query
	** does match this pattern, then a pointer to the Table object representing
	** <tbl> is returned. Otherwise, 0 is returned.
	*/
        /* 查询语句传递的第一个参数是聚集查询，第二个参数是相关的聚集信息对象。
	** 这个功能的作用是测试查询语句是否是如下格式：
	**   SELECT count(*) FROM <tbl>
	** 当这个表是数据库中的表，不是子查询结果或视图。如果查询和模式匹配，
	** 那么就有一个指向该Table对象表示<tb1>的指针返回，否则返回0。
	*/
	static Table *isSimpleCount(Select *p, AggInfo *pAggInfo){//两个参数，其中p是查询语法树，pAggInfo是聚集函数结构体
	  Table *pTab;//初始化一个表
	  Expr *pExpr;//初始化一个表达式

	  assert( !p->pGroupBy );//断言判断 !p->pGroupBy

	  if( p->pWhere || p->pEList->nExpr!=1 //查询语句含WHERE子句或表达式的个数不为1
	   || p->pSrc->nSrc!=1 || p->pSrc->a[0].pSelect)//或查询语句FROM子句不等于1，或者至少包含一个有嵌套子查询
	  {
		return 0;//返回0
	  }
	  pTab = p->pSrc->a[0].pTab;//将查询语句FROM中的第一个SQL表赋给pTab
	  pExpr = p->pEList->a[0].pExpr;//将查询语句中的第一个表达式赋给pExpr
	  assert( pTab && !pTab->pSelect && pExpr );//加入断言，判断pTab、pExpr是否赋值成功，且pTab为表而非视图
												//pTab是表则pTab->pSelect == null，是视图则pTab->pSelect为相应定义

	  if( IsVirtual(pTab) ) return 0;//pTab若是虚表，则返回0
	  if( pExpr->op!=TK_AGG_FUNCTION ) return 0;//若表达式为非聚集操作，则返回0
	  if( NEVER(pAggInfo->nFunc==0) ) return 0;//如果没有聚集函数，则返回0
	  if( (pAggInfo->aFunc[0].pFunc->flags&SQLITE_FUNC_COUNT)==0 ) return 0;//如果聚集函数的标记变量和SQLITE_FUNC_COUNT位运算不等，则返回0
	  if( pExpr->flags&EP_Distinct ) return 0;//如果表达式带有DISTINCT标记，则返回0

	  return pTab;//返回表pTab
	}

	/*
	** If the source-list item passed as an argument was augmented with an
	** INDEXED BY clause, then try to locate the specified index. If there
	** was such a clause and the named index cannot be found, return 
	** SQLITE_ERROR and leave an error in pParse. Otherwise, populate 
	** pFrom->pIndex and return SQLITE_OK.
	*/
	/* 如果源列表中的项传递的参数带有索引子句，则尝试定位指定索引。
	** 如果存在这样的子句且被指定的索引找不到，则返回SQLITE_ERROR，并在解析时显示错误。
	** 否则，填入pFrom->pIndex 并返回 SQLITE_OK。
	** 
	*/
	//索引项的处理
	int sqlite3IndexedByLookup(Parse *pParse, struct SrcList_item *pFrom){ //参数1:解析器 参数2:FROM的结构体
	  if( pFrom->pTab && pFrom->zIndex ){//如果pFrom->pTab非空，且pFrom->zIndex非空
		Table *pTab = pFrom->pTab;//将pFrom->pTab赋给一个新表
		char *zIndex = pFrom->zIndex;//将pFrom->zIndex赋给一个新标识符
		Index *pIdx;//初始化一个索引指针

		//遍历表中的索引项，查找指定索引
		for(pIdx=pTab->pIndex; 
			pIdx && sqlite3StrICmp(pIdx->zName, zIndex); 
			pIdx=pIdx->pNext
		);
		if( !pIdx ){//如果没有找到索引项
		  sqlite3ErrorMsg(pParse, "no such index: %s", zIndex, 0);//输出错误信息
		  pParse->checkSchema = 1;//解析器模式识别标志设为1
		  return SQLITE_ERROR;//返回SQLITE_ERROR
		}
		pFrom->pIndex = pIdx;//将找到的索引项添加到FROM表达式项的索引结构体中
	  }
	  return SQLITE_OK;//执行完毕，返回SQLITE_OK
	}

	/*
	** This routine is a Walker callback for "expanding" a SELECT statement.
	** "Expanding" means to do the following:
	**
	**    (1)  Make sure VDBE cursor numbers have been assigned to every
	**         element of the FROM clause.
	**
	**    (2)  Fill in the pTabList->a[].pTab fields in the SrcList that 
	**         defines FROM clause.  When views appear in the FROM clause,
	**         fill pTabList->a[].pSelect with a copy of the SELECT statement
	**         that implements the view.  A copy is made of the view's SELECT
	**         statement so that we can freely modify or delete that statement
	**         without worrying about messing up the presistent representation
	**         of the view.
	**
	**    (3)  Add terms to the WHERE clause to accomodate the NATURAL keyword
	**         on joins and the ON and USING clause of joins.
	**
	**    (4)  Scan the list of columns in the result set (pEList) looking
	**         for instances of the "*" operator or the TABLE.* operator.
	**         If found, expand each "*" to be every column in every table
	**         and TABLE.* to be every column in TABLE.
	**
	*/
	/* 这个程序用于Walker回调"expanding" SELECT 语句的，
	** "Expanding"是指如下操作：
	**  (1)确保VDBE已经被分配到每一个FROM子句的元素。
	**  (2)在定义FROM子句的表或子查询中，填充对应zName的表。
	**     当FROM子句中有视图时，在pTabList->a[].pSelect中填入实现该视图的SELECT语句的副本。
	**     因为它是视图的SELECT语句的副本，所以我们可以自由的修改、删除而不必担心视图受到影响。
	**  (3)在连接操作中以及涉及ON和USING的连接子句中，增加WHERE子句以适应NATURAL关键字。
	**  (4)在结果集中每一列查找"*"操作符或"TABLE.*"操作符，如果找到，在每张表的每列扩展"*"或"TABLE.*"。
	*/
	static int selectExpander(Walker *pWalker, Select *p){
	  Parse *pParse = pWalker->pParse;//声明一个语法解析树
	  int i, j, k;//自定义变量
	  SrcList *pTabList;//from子句列表指针的声明，扫描的所有表
	  ExprList *pEList;//表达式列表的声明，输出结果列的语法树
	  struct SrcList_item *pFrom;//声明一个FROM子句列表项
	  sqlite3 *db = pParse->db;//声明一个数据库连接，结构体Parse的成员db赋值给结构体sqlite3指针db

	  if( db->mallocFailed  ){//若动态内存分配失败
		return WRC_Abort;//放弃树的遍历，并终止程序
	  }
	  if( NEVER(p->pSrc==0) || (p->selFlags & SF_Expanded)!=0 ){//没有from子句或标记符和SF_Expanded位运算相等
		return WRC_Prune;//返回WRC_Prune（删除）
	  }
	  p->selFlags |= SF_Expanded;//将selFlags属性与SF_Expanded位或，再赋值给selFlags
	  pTabList = p->pSrc;//将FROM子句赋值给FROM子句列表
	  pEList = p->pEList;//再将SELECT结构体中表达式列表赋值给pEList


	  /* Make sure cursor numbers have been assigned to all entries in
	  ** the FROM clause of the SELECT statement.
	  */
	  /*确保游标号已经分配给所有的在SELECT语句里中FROM子句里。*/
	  sqlite3SrcListAssignCursors(pParse, pTabList);/*分配vdbe游标索引号到所有pTabList表中*/

	  /* Look up every table named in the FROM clause of the select.  If
	  ** an entry of the FROM clause is a subquery instead of a table or view,
	  ** then create a transient table structure to describe the subquery.
	  */
	  /* 查找查询语句中FROM子句里面每一个命名的表，如果FROM子句的入口是一个子查询而不是表或视图，
	  ** 就创建一个瞬态表结构描述这个子查询。
	  */
	  for(i=0, pFrom=pTabList->a; i<pTabList->nSrc; i++, pFrom++){ //遍历pFrom中表达式列表
		Table *pTab;//初始化一个table类型的指针变量
		if( pFrom->pTab!=0 ){//如果pFrom对应的zName语句存在
		  /* This statement has already been prepared.  There is no need
		  ** to go further. */
		  /* 
		  ** 这种逻辑已经具备，无需继续。
		  */
		  assert( i==0 );//设置断言
		  return WRC_Prune;//忽略孩子结点但继续访问兄弟结点，终止这段程序
		}
		if( pFrom->zName==0 ){ //如果表名为空
			#ifndef SQLITE_OMIT_SUBQUERY
			  Select *pSel = pFrom->pSelect;//将pFrom中代替表名的SELECT语句赋给一个新的Select变量
			  /* A sub-query in the FROM clause of a SELECT */
			  /*SELECT中FROM子句的子查询*/
			  assert( pSel!=0 );//异常处理，加入断言
			  assert( pFrom->pTab==0 );//异常处理，加入断言
			  sqlite3WalkSelect(pWalker, pSel);//对pSel中每一个语句调用sqlite3WalkExpr()方法
			  pFrom->pTab = pTab = sqlite3DbMallocZero(db, sizeof(Table));//重新分配数据库内存，并赋给pTab和pFrom->pTab
			  if( pTab==0 ) return WRC_Abort;//如果分配出错，则终止程序
			  pTab->nRef = 1;//指向表pTab的指针数目置零
			  pTab->zName = sqlite3MPrintf(db, "sqlite_subquery_%p_", (void*)pTab);//从内存获取相关信息打印出来赋给pTab名称
			  //遍历，查找优先的select赋给pSel
			  while( pSel->pPrior ){ //当pSel复合SELECT语句中有较优先的Select语句时
				  pSel = pSel->pPrior; //将较优先的Select语句赋给pSel
			  }
			  selectColumnsFromExprList(pParse, pSel->pEList, &pTab->nCol, &pTab->aCol);//在表达式列表中计算一个合适的列名
			  pTab->iPKey = -1;//将pTab->iPKey的值置为负，不用它做主键
			  pTab->nRowEst = 1000000;//将pTab的预算行数设为1000000
			  pTab->tabFlags |= TF_Ephemeral;//位运算，设置表需要屏蔽的TF_值，二进制特定位上的无条件赋值
			#endif

		}else{
		  /* An ordinary table or view name in the FROM clause */
		  /*FROM子句中的常规表或视图名*/
		  assert( pFrom->pTab==0 );//异常处理，pFrom中对应zName的SQL表是否存在，加入断言
		  pFrom->pTab = pTab = 
			sqlite3LocateTable(pParse,0,pFrom->zName,pFrom->zDatabase);//由语法解析，定位描述表pFrom的数据库内存结构
		  if( pTab==0 ) return WRC_Abort;//内存中未找到该表则终止程序
		  pTab->nRef++;//指向该表的指针数目+1
	#if !defined(SQLITE_OMIT_VIEW) || !defined (SQLITE_OMIT_VIRTUALTABLE) //如果没有定义忽略视图或没有定义忽略虚表
		  if( pTab->pSelect || IsVirtual(pTab) ){//若pTab为视图或虚表
			/* We reach here if the named table is a really a view */
			/*当pTab确实是一个视图时，程序可以运行到这一步*/
			if( sqlite3ViewGetColumnNames(pParse, pTab) ) //将pTab名装入Tabel表结构中，是视图则返回相应错误编号，运行if里面的代码
				return WRC_Abort;//若出现错误，终止程序
			assert( pFrom->pSelect==0 );//插入断言，如果pFrom为表而不是视图，抛出错误信息
			pFrom->pSelect = sqlite3SelectDup(db, pTab->pSelect, 0);//将pTabTable结构体pSelect copy 给pFrom
			sqlite3WalkSelect(pWalker, pFrom->pSelect);//表达式列表pFrom中每个表达式都调用sqlite3WalkExpr()
		  }
	#endif
		}

		/* Locate the index named by the INDEXED BY clause, if any. */
		/* 如果在INDEXED BY子句中存在索引名，则定位它 */
		if( sqlite3IndexedByLookup(pParse, pFrom) ){//在pFrom的表达式列表中查找索引名，如果没找到则在pParse中留下错误信息，并执行if中的代码
		  return WRC_Abort;//没找到索引名，终止程序
		}
	  }

	  /* Process NATURAL keywords, and ON and USING clauses of joins. */
	  /*处理NATURAL关键字，以及连接中的ON和USING子句 */
	  if( db->mallocFailed || sqliteProcessJoin(pParse, p) ){//内存分配失败或者处理连接操作时发现没有NATURAL关键字，也没有ON和USING子句
		return WRC_Abort;//终止程序
	  }

	  /* For every "*" that occurs in the column list, insert the names of
	  ** all columns in all tables.  And for every TABLE.* insert the names
	  ** of all columns in TABLE.  The parser inserted a special expression
	  ** with the TK_ALL operator for each "*" that it found in the column list.
	  ** The following code just has to locate the TK_ALL expressions and expand
	  ** each one to the list of all columns in all tables.
	  **
	  ** The first loop just checks to see if there are any "*" operators
	  ** that need expanding.
	  */
	  /*
	  ** 在列表中出现的每一个“*”，在所有的表和所有的列中插入名字。对于每个表的左连接运算，插入表中所有列的名字
	  ** 对于任何的“*”，在语法解析器中，插入一个聚集操作操作符的特殊表达式。
	  ** 剩下的代码必须找到聚集表达式并且扩展所有表所有列的列表
	  */

	  //遍历表达式列表
	  for(k=0; k<pEList->nExpr; k++){
		Expr *pE = pEList->a[k].pExpr;//表达式列表数组中的每个元素赋值给pe
		if( pE->op==TK_ALL ) break;//如果操作为聚集操作直接跳出循环体
		assert( pE->op!=TK_DOT || pE->pRight!=0 );//异常处理，加入断点
		assert( pE->op!=TK_DOT || (pE->pLeft!=0 && pE->pLeft->op==TK_ID) );//异常处理，加入断点
		if( pE->op==TK_DOT && pE->pRight->op==TK_ALL ) break;//满足条件，跳出循环体
	  }
	  if( k<pEList->nExpr ){//若k的值小于表达式列表的个数
		/*
		** If we get here it means the result set contains one or more "*"
		** operators that need to be expanded.  Loop through each expression
		** in the result set and expand them one by one.
		//到了这一步，表明结果集包含一个或多个的*操作，这些操作是需要扩展的。循环处理每一个结果集中的表达式并且扩展它们*/
	
		struct ExprList_item *a = pEList->a;//将表达式列表中放表达式的数组赋值给表达式列表项的表达式数组
		ExprList *pNew = 0;//*定义一个表达式列表
		int flags = pParse->db->flags;//将语法解析树中数据库连接的标记赋值给flags
		int longNames = (flags & SQLITE_FullColNames)!=0//如果flags为SQLITE_FullColNames并且不包含SQLITE_ShortColNames，赋值给longNames
						  && (flags & SQLITE_ShortColNames)==0;

		//遍历表达式列表
		for(k=0; k<pEList->nExpr; k++){
		  Expr *pE = a[k].pExpr;/*表达式列表数组中的每个元素赋值给pe
		  assert( pE->op!=TK_DOT || pE->pRight!=0 );//异常处理，加入断点
		  if( pE->op!=TK_ALL && (pE->op!=TK_DOT || pE->pRight->op!=TK_ALL) ){
			/* This particular expression does not need to be expanded.
			//这种特殊的表达不需要扩展*/
			pNew = sqlite3ExprListAppend(pParse, pNew, a[k].pExpr);//追加,返回的表达式给pnew
			if( pNew ){//pnew非空
			  pNew->a[pNew->nExpr-1].zName = a[k].zName;//数组元素表名属性赋值
			  pNew->a[pNew->nExpr-1].zSpan = a[k].zSpan;//数组元素的zSpan属性赋值
			  a[k].zName = 0;//置0
			  a[k].zSpan = 0;//置0
			}
			a[k].pExpr = 0;//将整个表达式置为0
		  }else{
			/* This expression is a "*" or a "TABLE.*" and needs to be
			** expanded.这个表达式是一个*或者是左连接，它们是需要被扩展的  */
			int tableSeen = 0;      /* Set to 1 when TABLE matches *///正确匹配的时候设置为1
			char *zTName;            /* text of name of TABLE *///表的文本名
			if( pE->op==TK_DOT ){/*如果操作为TK_DOT*/
			  assert( pE->pLeft!=0 );//插入断点，如果表达式左子树为空，抛出错误信息
			  assert( !ExprHasProperty(pE->pLeft, EP_IntValue) );//插入断点，如果表达式左子树没有EP_IntValue属性，抛出错误信息
			  zTName = pE->pLeft->u.zToken;//将表达式左子树的标记赋值给表的文本名
			}else{
			  zTName = 0;//否则文本清空
			}
			//遍历表达式列表
			for(i=0, pFrom=pTabList->a; i<pTabList->nSrc; i++, pFrom++){
			  Table *pTab = pFrom->pTab;//定义表,临时变量，存储表
			  char *zTabName = pFrom->zAlias;//获取表的别名
			  if( zTabName==0 ){//如果该表没有别名
				zTabName = pTab->zName;//直接获取表名
			  }
			  if( db->mallocFailed ) break;//内存分配失败，跳出循环体
			  if( zTName && sqlite3StrICmp(zTName, zTabName)!=0 ){//如果zTName不为空，zTName与zTabName不一致
				continue;//跳出本次循环
			  }
			  tableSeen = 1;//设置标记变量
			  //遍历表中的列，也就是所有的field
			  for(j=0; j<pTab->nCol; j++){
				Expr *pExpr, *pRight;//声明表达式
				char *zName = pTab->aCol[j].zName;//获取每一列包含的表的名字
				char *zColname;  // The computed column name //计算的列名
				char *zToFree;   /* Malloced string that needs to be freed *//*释放已分配字符串的空间*/
				Token sColname;  /* Computed column name as a token *//*标记列名，临时令牌

				/* If a column is marked as 'hidden' (currently only possible
				** for virtual tables), do not include it in the expanded result-set list.
				** result-set list.
				*//*如果有一列标记为隐藏，当前只能对虚表，在扩展的结果集中不包括它。*/
				if( IsHiddenColumn(&pTab->aCol[j]) ){//判断列是否被隐藏
				  assert(IsVirtual(pTab));//（判断该表是否是虚表）异常处理，加入断点
				  continue;//跳出本次循环
				}

				if( i>0 && zTName==0 ){//如果表名为空且已经执行过循环
				  if( (pFrom->jointype & JT_NATURAL)!=0/*连接类型为JT_NATURAL*/
					&& tableAndColumnIndex(pTabList, i, zName, 0, 0)/*并且表与列索引不为空*/
				  ){
					/* In a NATURAL join, omit the join columns from the 
					** table to the right of the join *//*(注：使用自然连接，从右侧的连接表中省略连接列)*/
					continue;//跳过本次循环
				  }
				  if( sqlite3IdListIndex(pFrom->pUsing, zName)>=0 ){//列索引
					/* In a join with a USING clause, omit columns in the
					** using clause from the table on the right. *//*用USING子句的连接，从右表中省略在using子句中的列*/
					continue;//跳过本次循环
				  }
				}
				pRight = sqlite3Expr(db, TK_ID, zName);//调用sqlite3Expr函数，分配列zName一个TK_ID标记
				zColname = zName;//对列名的赋值
				zToFree = 0;/*释放已分配的字符串*/
				if( longNames || pTabList->nSrc>1 ){/*如果longNames（全称）不为空或表大于1*/
				  Expr *pLeft;//声明表达式
				  pLeft = sqlite3Expr(db, TK_ID, zTabName); //sqlite3Expr分配给表名zTabName的TK_ID标记一个表达式，并返回给pLeft
				  pExpr = sqlite3PExpr(pParse, TK_DOT, pLeft, pRight, 0);//分配两个表达式的标记TK_DOT一个表达式，并赋值给pExpr
				  if( longNames ){//如果全称路径存在
					zColname = sqlite3MPrintf(db, "%s.%s", zTabName, zName);//打印出列名返回给zColname
					zToFree = zColname;//已分配的字符串zToFree值为zColname
				  }
				}else{
				  pExpr = pRight;//pright赋值给最后的表达式
				}
				pNew = sqlite3ExprListAppend(pParse, pNew, pExpr);//将表达式pExpr添加到pParse中的pNew中，并赋值给pNew
				sColname.z = zColname;//将赋值zColname给列名sColname.z
				sColname.n = sqlite3Strlen30(zColname);//限制列名的位数不能超多30
				sqlite3ExprListSetName(pParse, pNew, &sColname, 0);//设置pNew中列名sColname
				sqlite3DbFree(db, zToFree);//释放数据库连接中存放zToFree标记的内存
			  }
			}
			if( !tableSeen ){/*如果没有匹配上*/
			  if( zTName ){//列名不为空
				sqlite3ErrorMsg(pParse, "no such table: %s", zTName);//打印错误信息
			  }else{
				sqlite3ErrorMsg(pParse, "no tables specified");//打印错误信息
			  }
			}
		  }
		}
		sqlite3ExprListDelete(db, pEList);//删除数据库连接中的pEList表达式列表
		p->pEList = pNew;//将pNew赋值给表达式列表pEList
	  }
	#if SQLITE_MAX_COLUMN
	  if( p->pEList && p->pEList->nExpr>db->aLimit[SQLITE_LIMIT_COLUMN] ){//列溢出，表达式不为空
		sqlite3ErrorMsg(pParse, "too many columns in result set");//打印错误信息
	  }
	#endif
	  return WRC_Continue;//继续执行
	}

	/*
	** No-op routine for the parse-tree walker.
	**
	** When this routine is the Walker.xExprCallback then expression trees
	** are walked without any actions being taken at each node.  Presumably,
	** when this routine is used for Walker.xExprCallback then 
	** Walker.xSelectCallback is set to do something useful for every 
	** subquery in the parser tree.
	*/
	/* 关于解析树walker的例程
	**
	** 当程序是Walker.xExprCallback回调函数，表达式树在没有任何操作情况下占据每个节点。假设，当程序用来Walker.xExprCallback
	** 然后Walker.xSelectCallback 为语法解析树中的每一个子查询提供帮助。
	*/
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
	
	/*
	** 这是关于解析树Walker的无操作例程
	**
	** 当这个例程是Walker.xExprCallback时， 表达式树在每个节点上即使不采取行动
	** 也可以。因此，当此例程被用于Walker.xExprCallback时，Walker.xSelectCallback 
	** 被设置成解析树中每个子查询都有用。
	*/
	static int exprWalkNoop(Walker *NotUsed, Expr *NotUsed2){
	  UNUSED_PARAMETER2(NotUsed, NotUsed2);/*如果NotUsed2没有使用，并驻留在函数中。输出警告信息。*/
	  return WRC_Continue;/*返回继续执行标识符*/
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
	/*
	** This routine "expands" a SELECT statement and all of its subqueries.
	** For additional information on what it means to "expand" a SELECT
	** statement, see the comment on the selectExpand worker callback above.
	**
	** Expanding a SELECT statement is the first step in processing a
	** SELECT statement.  The SELECT statement must be expanded before
	** name resolution is performed.
	**
	** If anything goes wrong, an error message is written into pParse.
	** The calling function can detect the problem by looking at pParse->nErr
	** and/or pParse->db->mallocFailed.
	*/
	/* 这个例程“expands”一个SELECT声明和所有的子查询，它的附加信息是指它是"expand" 语句的SELECT的声明
	**
	** SELECT声明中的第一步是扩展一个SELECT语句。在解析执行前，这个select语句必须被扩展
	**
	** 若有错误出现，那这条错误信息就写到解析器中。回调函数可以在pParse->nErr 或 pParse->db->mallocFailed中找出错误信息
	*/
	
	/*
	** 这个例程扩展了一个SELECT语句和它所有的子查询。
	** 想要查询更多有关扩展了一个SELECT语句的信息，请查看上面selectExpand 
	** 回调的注释。
	**
	** 扩展一个SELECT语句是处理一个SELECT语句的第一步。 
	** SELECT语句必须在名称解析之前完成扩展操作。
	**
	** 如果发生了任何错误，则会有一条错误信息返回到pParse。
	** 通过查看pParse->nErr和/或pParse->db->mallocFailed指针，调用方法可以检测
	** 问题。
	*/
	static void sqlite3SelectExpand(Parse *pParse, Select *pSelect){
	  Walker w;//Walker结构体的声明
	  w.xSelectCallback = selectExpander;/*调用一个selectExpander函数给回调函数xSelectCallback（调用SELECT的函数）*/
	  w.xExprCallback = exprWalkNoop;//若表达式含有未使用的信息，调用回调函数
	  w.pParse = pParse;//将参数中语法解析树pParse赋值给w.pParse
	  sqlite3WalkSelect(&w, pSelect);//pSelect中每个表达式均调用sqlite3WalkExpr，表达式使用回调函数
	}


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
	/*
	** This is a Walker.xSelectCallback callback for the sqlite3SelectTypeInfo()
	** interface.
	**
	** For each FROM-clause subquery, add Column.zType and Column.zColl
	** information to the Table structure that represents the result set
	** of that subquery.
	**
	** The Table structure that represents the result set was constructed
	** by selectExpander() but the type and collation information was omitted
	** at that point because identifiers had not yet been resolved.  This
	** routine is called after identifier resolution.
	*/
	/* 这是一个Walker.xSelectCallback回调sqlite3SelectTypeInfo()函数的接口。
	**
	** 对于任何一个FROM子查询，添加Column.zType和 Column.zColl信息到表结构中，他代表子查询的结果集。
	**
	** 表结构代表结果集由selectExpander（）构建，但在这种情况下类型和排列信息会被省略，因为没有解析标识符。
	** 这个例程实在解析标识之后调用的
	*/
	
	/*
	** 这是一个用于sqlite3SelectTypeInfo()接口的Walker.xSelectCallback方法的回调模块。
	** 对于每个FROM语句子查询，添加Column.zType和Column.zColl信息到Table结构中，
	** Table结构代表了该子查询的结果集。
	** 
	** 表示该子查询结果集的Table结构是由selectExpander()建立的，但类型和排序信息
	** 在这个时候被遗漏了，因为还没有解析标识符。这个例程在解析标识符之后调用。 
	*/
	static int selectAddSubqueryTypeInfo(Walker *pWalker, Select *p){
	  Parse *pParse;//声明一个语法解析树
	  int i;//声明一个整型变量
	  SrcList *pTabList;//声明from子句的表达式列表
	  struct SrcList_item *pFrom;//声明表达式列表项

	  assert( p->selFlags & SF_Resolved );//异常处理，加入断点
	  if( (p->selFlags & SF_HasTypeInfo)==0 ){//检查标记变量和类型信息
		p->selFlags |= SF_HasTypeInfo;//位运算
		pParse = pWalker->pParse;//语法解析树
		pTabList = p->pSrc;/*将SELECT中FROM子句赋值给FROM子句表达式列表*/
		//遍历列表的表达式
		for(i=0, pFrom=pTabList->a; i<pTabList->nSrc; i++, pFrom++){
		  Table *pTab = pFrom->pTab;//将FROM子句表达式列表中表赋值给pTab
		  if( ALWAYS(pTab!=0) && (pTab->tabFlags & TF_Ephemeral)!=0 ){//如果pTab不为空，并且tabFlags不是TF_Ephemeral（临时表）
			/* A sub-query in the FROM clause of a SELECT 在from子句中的子查询*/
			      /* A sub-query in the FROM clause of a SELECT  
        **
        **select中的from 子句的一个子查询
        **
        */
			Select *pSel = pFrom->pSelect;//获取select语句
			assert( pSel );//插入断点，如果pSel为空，抛出错误信息
			while( pSel->pPrior ) pSel = pSel->pPrior;//递归得出SELECT中最优先查询的子查询SELECT
			selectAddColumnTypeAndCollation(pParse, pTab->nCol, pTab->aCol, pSel);//添加列的类型和相关的信息
		  }
		}
	  }
	  return WRC_Continue;//继续执行
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

	/*
	** This routine adds datatype and collating sequence information to
	** the Table structures of all FROM-clause subqueries in a
	** SELECT statement.
	**
	** Use this routine after name resolution.
	*/
        /* 这段程序添加datatype 和 排列序列信息到SELECT中子查询的所有FROM子句的TABLE结构中。
	** 在名字解析后使用这段程序*/
	static void sqlite3SelectAddTypeInfo(Parse *pParse, Select *pSelect){
	#ifndef SQLITE_OMIT_SUBQUERY
	  Walker w;//声明一个Walker结构体
	  w.xSelectCallback = selectAddSubqueryTypeInfo;//调用查询类型信息给回调函数/*select回叫*/
	  w.xExprCallback = exprWalkNoop;//表达式信息给回调函数/*表达式回叫*/
	  w.pParse = pParse;//解析器
	  sqlite3WalkSelect(&w, pSelect);//回调函数，调用sqlite3WalkSelect
	#endif
	}

	/*
	** This routine sets up a SELECT statement for processing.  The
	** following is accomplished:
	**
	**     *  VDBE Cursor numbers are assigned to all FROM-clause terms.
	**     *  Ephemeral Table objects are created for all FROM-clause subqueries.
	**     *  ON and USING clauses are shifted into WHERE statements
	**     *  Wildcards "*" and "TABLE.*" in result sets are expanded.
	**     *  Identifiers in expression are matched to tables.
	**
	** This routine acts recursively on all subqueries within the SELECT.
	*/
	/* 这个例程设置 SELECT 语句进行处理。
	**   * VDBE游标号分配给所有的FROM子句
	**   * 为所有 FROM 子句的子查询创建临时表对象
	**   * ON 和 USING 子句切换到WHERE语句
	**   * 结果集中扩展通配符"*" 和 "TABLE左连接"
	**   * 在表达式中的标识符相匹配的表
	** 在SELECT中递归执行所有的子查询
	*/
	/*start update by 
* 2015-11-24 韦付芝
** This routine sets up a SELECT statement for processing.  The
** following is accomplished:
**本程序建立一个SELECT语句的处理
**     *  VDBE Cursor numbers are assigned to all FROM-clause terms.             为from子句分配的vdbe游标数
**     *  Ephemeral Table objects are created for all FROM-clause subqueries    为from子句的子查询建立的临时表对象.
**     *  ON and USING clauses are shifted into WHERE statements
**     *  Wildcards "*" and "TABLE.*" in result sets are expanded.                     通配符"*" 和"TABLE.*" 在结果集中被扩展
**     *  Identifiers in expression are matched to tables.                                   表达式的标识符被匹配到相应的表
**
** This routine acts recursively on all subqueries within the SELECT.
**对select语句中的子句进行处理
*/
	void sqlite3SelectPrep(
	  Parse *pParse,         /* The parser context *///定义解析器/*  解析器的上下文 */
	  Select *p,             /* The SELECT statement being coded. *///声明select类型的指针/*  被编码的select表达式 */
	  NameContext *pOuterNC  /* Name context for container *///为容器命名/*  命名上下文 */
	){
	  sqlite3 *db;//声明一个sqlite类型的数据库连接/* 定义数据库指针变量*/
	  if( NEVER(p==0) ) return;//空指针，直接返回/*判断传递过来的参数select表达式指针是否为空*/
	  db = pParse->db;//获取语法解析器中的数据库/*数据库指针变量指向解析器上下文对象的数据库存储域*/
	  if( p->selFlags & SF_HasTypeInfo ) return;//标识变量和类型信息的判断/*判断传递过来的参数select表达式指针与SF_HasTypeInfo进行与运算后的结果*/
	  sqlite3SelectExpand(pParse, p);//扩展，查找SELECT中的扩展符号/*调用sqlite3SelectExpand方法，传递两个参数：解析器和表达式指针变量*/
	  if( pParse->nErr || db->mallocFailed ) return;//解析错误或者分配内存失败/*判断解析器的是否出错或数据库分配内存是否成功*/
	  sqlite3ResolveSelectNames(pParse, p, pOuterNC);//解析上下文/*调用sqlite3ResolveSelectNames方法，传递三个参数：解析器，表达式指针，命名上下文指针*/
	  if( pParse->nErr || db->mallocFailed ) return;//解析错误或者分配内存失败/*再次判断解析器的是否出错或数据库分配内存是否成功*/
	  sqlite3SelectAddTypeInfo(pParse, p);//向解析树添加语法信息/*调用sqlite3SelectAddTypeInfo方法，传递两个参数：解析器，表达式。将查询语句添加进解析器。*/
	}

	/*
	** Reset the aggregate accumulator.
	**
	** The aggregate accumulator is a set of memory cells that hold
	** intermediate results while calculating an aggregate.  This
	** routine generates code that stores NULLs in all of those memory
	** cells.
	*/
	/* 聚集函数的重新设置
	** 聚集累加器是一组记忆单元，它能在计算一个聚集的时候保存中间结果集。这段程序生成的代码在所有的记忆单元中存储了空值
	*/

	/*
**
**在计算一个聚集函数时，聚合累加器是保持中间结果集的内存单元集
**这个程序生成代码,这些代码将null存储在所有的内存单元中
**
*/
	
	static void resetAccumulator(Parse *pParse, AggInfo *pAggInfo){/*重置聚合累加器，传递两个参数：解析器指针变量，AggInfo指针变量*/
	  Vdbe *v = pParse->pVdbe;//获取语法解析器中的pVdbe属性/*定义一个Vdbe指针变量，指向解析器的pVdbe域*/
	  int i;//声明一个变量/*定义变量i*/
	  struct AggInfo_func *pFunc;//声明一个AggInfo_func结构体对象/*定义一个结构体变量指针pFunc，指向AggInfo_func类型*/
	  if( pAggInfo->nFunc+pAggInfo->nColumn==0 ){/*判断参数的两个域内容和是否为0*/
		return;
	  }

	  //遍历所有的列
	  for(i=0; i<pAggInfo->nColumn; i++){/*循环调用sqlite3VdbeAddOp2方法*/
		sqlite3VdbeAddOp2(v, OP_Null, 0, pAggInfo->aCol[i].iMem);//操作处理
	  }
	  //聚集函数的遍历
	  for(pFunc=pAggInfo->aFunc, i=0; i<pAggInfo->nFunc; i++, pFunc++){/*循环调用sqlite3VdbeAddOp2方法，参数和上面不同*/
    sqlite3VdbeAddOp2(v, OP_Null, 0, pFunc->iMem); /*将OP_Null操作交给vdbe，然后返回这个操作的地址*/
		sqlite3VdbeAddOp2(v, OP_Null, 0, pFunc->iMem);//操作处理
		if( pFunc->iDistinct>=0 )/*判断是否去重*/
		  Expr *pE = pFunc->pExpr;//获取AggInfo_func中的表达式/*表达式指针指向pFunc的pExpr域*/
		  assert( !ExprHasProperty(pE, EP_xIsSelect) );//异常处理，加入断点
		  if( pE->x.pList==0 || pE->x.pList->nExpr!=1 ){/*判断表达式的x域的pList属性值是否为0或pList的下一表达式域是否为1*/
			sqlite3ErrorMsg(pParse, "DISTINCT aggregates must have exactly one "
			   "argument");//打印错误信息/*调用sqlite3ErrorMsg方法弹出错误信息*/
			pFunc->iDistinct = -1;//临时表置为-1（无效）/*将pFunc的iDistinct域置为-1*/
		  }else{
			KeyInfo *pKeyInfo = keyInfoFromExprList(pParse, pE->x.pList);//从表达式列表中获取信息/*调用keyInfoFromExprList方法，传递两个参数，将返回值赋值给KeyInfo类型的指针变量*/
			sqlite3VdbeAddOp4(v, OP_OpenEphemeral, pFunc->iDistinct, 0, 0,
							  (char*)pKeyInfo, P4_KEYINFO_HANDOFF);//添加了一个操作符OP_OpenEphemeral/*调用sqlite3VdbeAddOp4方法*/
		  }
		}
	  }
	}
	
	/*
	** Invoke the OP_AggFinalize opcode for every aggregate function 
	** in the AggInfo structure.
	*/
	/*
	** 先处理聚集函数，再为AggInfo结构体中每一个聚集函数调用OP_AggFinalize（完成）操作*/

	static void finalizeAggFunctions(Parse *pParse, AggInfo *pAggInfo){
	  Vdbe *v = pParse->pVdbe;//获取语法解析器中的pVdbe属性
	  int i;//声明一个变量
	  struct AggInfo_func *pF;//声明一个AggInfo_func结构体对象
	  //聚集函数的遍历
	  for(i=0, pF=pAggInfo->aFunc; i<pAggInfo->nFunc; i++, pF++){/*遍历聚集函数*/
		ExprList *pList = pF->pExpr->x.pList;//聚集函数的表达式赋值给pList
		assert( !ExprHasProperty(pF->pExpr, EP_xIsSelect) );//异常处理，加入断点，pE不含EP_xIsSelect，抛出错误信息
		sqlite3VdbeAddOp4(v, OP_AggFinal, pF->iMem, pList ? pList->nExpr : 0, 0,
						  (void*)pF->pFunc, P4_FUNCDEF);//添加了一个操作符OP_OpenEphemeral，将它的值作为一个指针
	  }
	}	
/*
** Update the accumulator memory cells for an aggregate based on
** the current cursor position.
**
**为一个基于当前所处的位置上的聚集函数更新累加器的内存单元
*/
static void updateAccumulator(Parse *pParse, AggInfo *pAggInfo){//两个参数，其中parse是分析树，pAggInfo是聚集函数结构体
	Vdbe *v = pParse->pVdbe;    /*声明一个分析树中执行数据库字节码的引擎（PVdbe）*/
	int i;
	int regHit = 0;//用于表示语法分析树内存单元个数 
	int addrHitTest = 0;
	struct AggInfo_func *pF; //声明一个聚集函数功能结构体（AggInfo_func为AggInfo子结构体）
	struct AggInfo_col *pC; //声明一个聚集函数列结构体                                

	pAggInfo->directMode = 1;	 //允许直接从源表中取数据
	sqlite3ExprCacheClear(pParse);/*清除缓存中的语法分析树*/
	for (i = 0, pF = pAggInfo->aFunc; i < pAggInfo->nFunc; i++, pF++){/*开始进行聚集函数的遍历,其中aFunc是在聚集函数结构体中定义的一个数组，而nFunc指该数组中元素的个数*/
		int nArg;//用来记录索引列表中表达式的数目 
		int addrNext = 0;// 
		int regAgg; //用来表示为聚集函数表达式分配的寄存器数目                                                                                                                                                                                                           
		ExprList *pList = pF->pExpr->x.pList;//声明一个分析树的编码功能表达式的列索引表 
		assert(!ExprHasProperty(pF->pExpr, EP_xIsSelect));/*插入一个断点，如果pE包含EP_xIsSelect，那么就抛出错误的信息，两个参数，第一个是聚集函数的编码功能表达式*/
		if (pList){//若创建索引列表成功，则 
			nArg = pList->nExpr;//将列表中的表达式数目赋给nArg 
			regAgg = sqlite3GetTempRange(pParse, nArg);//为列表中的表达式分配连续的寄存器 ，并返回第一个寄存器的编号赋给regAgg
			sqlite3ExprCodeExprList(pParse, pList, regAgg, 1);//把表达式列表中的值存放到一系列的寄存器中 
		}
		else{//若创建索引列表失败 
			nArg = 0;//将列表的表达式数目置0 
			regAgg = 0;//为表达式分配的寄存器数目置0
		}
		if (pF->iDistinct >= 0){//若存在聚集函数用于执行DISTINCT操作的临时表 
			addrNext = sqlite3VdbeMakeLabel(v);//创建一个还没有被编码的新的符号标签，并将其赋给addrNext 
			assert(nArg == 1);//判断索引列表中是不是只有一个表达式 
			codeDistinct(pParse, pF->iDistinct, addrNext, 1, regAgg);// ??????
		}
		if (pF->pFunc->flags & SQLITE_FUNC_NEEDCOLL){//若聚集函数中的聚合函数已经实现，并且sqlite3GetFuncCollSeq()函数很可能被调用
			CollSeq *pColl = 0;//声明一个排序序列，指向第一个元素 
			struct ExprList_item *pItem;
			int j;
			assert(pList != 0);  /* 判断索引列表是否在开始位置 */
			for (j = 0, pItem = pList->a; !pColl && j < nArg; j++, pItem++){// 遍历表达式索引列表 
				pColl = sqlite3ExprCollSeq(pParse, pItem->pExpr);//根据语法分析树，为表达式pExpr返回默认排序顺序。如果没有默认排序类型，返回0
			}
			if (!pColl){//如果没有默认排序类型 
				pColl = pParse->db->pDfltColl;//那么将数据库连接中的默认的排序序列赋值给pColl                   
			}
			if (regHit == 0 && pAggInfo->nAccumulator) regHit = ++pParse->nMem;//如果regHit为0并且累加器不为0，语法分析树内存单元个数自加之后再赋值给regHit 
			sqlite3VdbeAddOp4(v, OP_CollSeq, regHit, 0, 0, (char *)pColl, P4_COLLSEQ);//向虚拟机V中添加操作码为OP_CollSeq的指令，pColl是P4_COLLSEQ类型的指针，并修改它的值，返回新指令的地址
			                                                                           
		}
		sqlite3VdbeAddOp4(v, OP_AggStep, 0, regAgg, pF->iMem,
			(void*)pF->pFunc, P4_FUNCDEF);//向虚拟机V中添加操作码为OP_AggStep的指令，pF->pFunc是P4_FUNCDEF)类型的指针，并修改它的值，返回新指令的地址 
		sqlite3VdbeChangeP5(v, (u8)nArg);//将虚拟机中的新指令的第5个操作数改为 nArg（寄存器数目 ） 
		sqlite3ExprCacheAffinityChange(pParse, regAgg, nArg);//在语法分析树中记录最后一个寄存器中，从regAgg开始发生的亲和性（affinity）变化 
		sqlite3ReleaseTempRange(pParse, regAgg, nArg);//释放为表达式分配的寄存器 
		if (addrNext){// 如果有下一个执行地址
			sqlite3VdbeResolveLabel(v, addrNext);//解析addrNext，此时addrNext是一个标签，并作为下一条被插入的地址
			sqlite3ExprCacheClear(pParse);//清除缓存中的语法解析树
		} 
	}

	/* Before populating the accumulator registers, clear the column cache.
	** Otherwise, if any of the required column values are already present
	** in registers, sqlite3ExprCode() may use OP_SCopy to copy the value
	** to pC->iMem. But by the time the value is used, the original register
	** may have been used, invalidating the underlying buffer holding the
	** text or blob value. See ticket [883034dcb5].
	**
	**在获取累加寄存器的内存之前,清空列缓存。
	**否则,如果任何所需的列值已经存在于寄存器中,
	**sqlite3ExprCode()函数会执行OP_SCopy操作，将这个值复制到pC->iMem（内存）中。
	**但当这个值被使用时,初始寄存器可能被使用
	**这就会使得底层缓存区中保存的文本和二进制值失效。
	**
	** Another solution would be to change the OP_SCopy used to copy cached
	** values to an OP_Copy.
	**
	**另一个解决方案是用OP_Copy操作来替换OP_Copy操作，实现将缓存中的数据复制到内存中。
	**
	*/
	if (regHit){//若有内存单元 
		addrHitTest = sqlite3VdbeAddOp1(v, OP_If, regHit);//在虚拟机中添加操作码为OP_If的指令，并将返回的指令地址指令赋给addrHitTest。该指令的第一个操作数是当前内存单元的个数，还有两个默认的操作数0 
	}
	sqlite3ExprCacheClear(pParse);//清除缓存中的语法解析树
	for (i = 0, pC = pAggInfo->aCol; i < pAggInfo->nAccumulator; i++, pC++){//遍历累加器.其中acol是在聚集函数结构体（pAggInfo）定义的一个子结构体，相当于一个数组，pAggInfo->aCol数组的首址
		sqlite3ExprCode(pParse, pC->pExpr, pC->iMem);//计算分析树的列索引表中的每个表达式（pC->pExpr），并将结果存储在寄存器(pC->iMem)中 
	}
	pAggInfo->directMode = 0;// 禁止直接从源表中取数据 
	sqlite3ExprCacheClear(pParse);//清除缓存中的语法解析树 
	if (addrHitTest){//如果addrHitTest不为空,即向虚拟机中添加OP_If的指令失败 
		sqlite3VdbeJumpHere(v, addrHitTest);//将addrHitTest设置成当前地址
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
	**     SRT_Mem          。存储第一个结果行的第一列在寄存器pDest->iSDParm
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
	Parse *pParse,         /* The parser context *//*解析上下文*/
	Select *p,             /* The SELECT statement being coded. SELECT语句被编码*/
	SelectDest *pDest      /* What to do with the query results 如何处理查询结果*/
	){
	int i, j;              /* Loop counters 循环计数器*/
	WhereInfo *pWInfo;     /* Return from sqlite3WhereBegin() 从sqlite3WhereBegin()返回*/
	Vdbe *v;               /* The virtual machine under construction 创建中的虚拟机*/
	int isAgg;             /* True for select lists like "count(*)" 选择是否是聚集*/
	ExprList *pEList;      /* List of columns to extract. 提取的列列表*/
	SrcList *pTabList;     /* List of tables to select from 查询表列表 */
	Expr *pWhere;          /* The WHERE clause.  May be NULL  where子句 可能为空*/
	ExprList *pOrderBy;    /* The ORDER BY clause.  May be NULL order by子句，可能为空*/
	ExprList *pGroupBy;    /* The GROUP BY clause.  May be NULL group by子句，可能为空*/
	Expr *pHaving;         /* The HAVING clause.  May be NULL having子句，可能为空*/
	int isDistinct;        /* True if the DISTINCT keyword is present  如果存在不同的关键字*/
	int distinct;          /* Table to use for the distinct set 对表使用distinct 设置*/
	int rc = 1;            /* Value to return from this function 从函数返回值*/
	int addrSortIndex;     /* Address of an OP_OpenEphemeral instruction OP_OpenEphemeral指令的地址*/
	int addrDistinctIndex; /* Address of an OP_OpenEphemeral instruction OP_OpenEphemeral指令的地址*/
	AggInfo sAggInfo;      /* Information used by aggregate queries 聚集信息*/
	int iEnd;              /* Address of the end of the query 查询结束地址*/
	sqlite3 *db;           /* The database connection 数据库连接*/

#ifndef SQLITE_OMIT_EXPLAIN
	int iRestoreSelectId = pParse->iSelectId; /*将语法解析树中查找ID存储在iRestoreSelectId中*/
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
		p->pOrderBy = 0; /*将pOrderBy置为0*/
		p->selFlags &= ~SF_Distinct;/*selFlags与~SF_Distinct再赋值给selFlags*/
	}
	sqlite3SelectPrep(pParse, p, 0);/*执行SELECT p前预处理*/
	pOrderBy = p->pOrderBy;/*将SELECT中pOrderBy赋值给当前变量pOrderBy*/
	pTabList = p->pSrc;/*将SELECT中FROM子句赋值给pTabList*/
	pEList = p->pEList;/*将SELECT中表达式列表赋值给pEList*/
	if (pParse->nErr || db->mallocFailed){/*如果语法解析树中有错误，或分配内存出错*/
		goto select_end;/*如果语法解析树失败或出错，就跳转到select_end*/
	}
	isAgg = (p->selFlags & SF_Aggregate) != 0;/*根据selFlags判断是否是聚集函数，如果有isAgg为true，否则为FALSE*/
	assert(pEList != 0);/*根据selFlags判断是否是聚集函数，如果有isAgg为true，否则为FALSE；然后插入断点，如果表达式列表为空，就抛出错误信息*/

	/* Begin generating code.
	**开始生成代码
	*/
	v = sqlite3GetVdbe(pParse);/*根据语法解析树生成一个虚拟数据库引擎*/
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

		struct SrcList_item *pItem = &pTabList->a[i];/*将表达式列表中表达式项赋值给pItem*/
		SelectDest dest;/*声明一个处理结果集结构体*/
		Select *pSub = pItem->pSelect;/*将表达式列表项中SELECT结构体赋值给pSub*/
		int isAggSub;/*声明聚集函数信息*/
		if (pSub == 0) continue;/*如果SELECT结构体pSub为0，跳过此次循环到下次循环*/
		if (pItem->addrFillSub){/*如果添加子查询地址*/
			sqlite3VdbeAddOp2(v, OP_Gosub, pItem->regReturn, pItem->addrFillSub);/*将OP_Gosub操作交给vdbe，然后返回这个操作的地址*/
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
			int topAddr;/*声明起始地址*/
			int onceAddr = 0;
			int retAddr;/*声明重置地址*/
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
			explainSetInteger(pItem->iSelectId, (u8)pParse->iNextSelectId);/*把语法分析树的pParse的iNextSelectId赋值给pItem->iSelectId*/
			sqlite3Select(pParse, pSub, &dest);/*为子查询语句生成代码,/*使用自身函数处理查询*/
			pItem->pTab->nRowEst = (unsigned)pSub->nSelectRow;/*将SELECT结构体中查找行结果赋值给表达式列表项中的表的行数*/
			if (onceAddr) sqlite3VdbeJumpHere(v, onceAddr);/*如果onceAddr不为0，运行地址跳到当前地址*/
			retAddr = sqlite3VdbeAddOp1(v, OP_Return, pItem->regReturn);/*将OP_Return操作交给vdbe，然后返回这个操作的地址并赋值給retAddr*/
			VdbeComment((v, "end %s", pItem->pTab->zName)); /*将“end pItem->pTab->zName”存入到数据库中*/
			sqlite3VdbeChangeP1(v, topAddr, retAddr);/*将topAddr地址改为retAddr*/
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
			if (mxSelect && cnt > mxSelect){/*如果mxSelect存在，并且深度大于SELECT个数*/
				sqlite3ErrorMsg(pParse, "too many terms in compound SELECT");/*在语法解析树中存储too many terms in compound SELECT*/
				goto select_end;/*如果mxSelect存在，并且深度大于SELECT个数的话，那么在语法解析树中存储too many...Select,然后跳到查询结束*/
			}
		}
		rc = multiSelect(pParse, p, pDest);/*调用自身函数，将处理结果返回给rc*/
		explainSetInteger(pParse->iSelectId, iRestoreSelectId);/*把iRestoreSelectId(下一个SELECT的ID)赋值给pParse->iSelectId*/
		return rc;/*返回执行完标记*/
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
		p->selFlags &= ~SF_Distinct;/*selFlags与~SF_Distinct再赋值给selFlags*/
		p->pGroupBy = sqlite3ExprListDup(db, p->pEList, 0);/*深度copy p->pEList给p->pGroupBy*/
		pGroupBy = p->pGroupBy;/*将SELECT中pGroupBy赋值给pGroupBy*/
		pOrderBy = 0;/*将pOrderBy置0*/
	}

	/* If there is an ORDER BY clause, then this sorting
	** index might end up being unused if the data can be
	** extracted in pre-sorted order.  If that is the case, then the
	** OP_OpenEphemeral instruction will be changed to an OP_Noop once
	** we figure out that the sorting index is not needed.  The addrSortIndex
	** variable is used to facilitate that change.
	**如果有ORDER BY子句如果数据被提前排序，排序索引可能不用。如果这种情况OP_OpenEphemeral指令会改变
	**OP_Noop，一旦决定不需要排序索引。addrSortIndex变量用于帮助改变。*/
	
	if (pOrderBy){/*如果存在ORDERBY子句*/
		KeyInfo *pKeyInfo;/*声明一个关键信息结构体*/
		pKeyInfo = keyInfoFromExprList(pParse, pOrderBy);/*将表达式pOrderBy放到关键信息结构体中*/
		pOrderBy->iECursor = pParse->nTab++;/*将表的数量加1放到游标中*/
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
	p->nSelectRow = (double)LARGEST_INT64;/*将SELECT行最大设为64位*/
	computeLimitRegisters(pParse, p, iEnd);/*计算iLimit和iOffset字段*/
	if (p->iLimit == 0 && addrSortIndex >= 0){/*如果limit为0并且排序索引大于等于0*/
		sqlite3VdbeGetOp(v, addrSortIndex)->opcode = OP_SorterOpen;/*将操作OP_SorterOpen（打开分拣器），将排序索引交给VDBE*/
		p->selFlags |= SF_UseSorter;/*将selFlags位与SF_UseSorter，再赋值给selFlags*/
	}

	/* Open a virtual index to use for the distinct set.
	  **为distinct集合打开一个虚拟索引
	  */
	if (p->selFlags & SF_Distinct){/*如果selFlags的值为SF_Distinct*/
		KeyInfo *pKeyInfo;/*声明一个关键信息结构体*/
		distinct = pParse->nTab++;/*将表的数量加1赋值给distinct*/
		pKeyInfo = keyInfoFromExprList(pParse, p->pEList);/*将表达式列表p->pEList放到关键信息结构体中*/
		addrDistinctIndex = sqlite3VdbeAddOp4(v, OP_OpenEphemeral, distinct, 0, 0,
			(char*)pKeyInfo, P4_KEYINFO_HANDOFF);/*添加一个OP_OpenEphemeral操作，并将它的值作为一个指针并赋值给addrDistinctIndex*/
		sqlite3VdbeChangeP5(v, BTREE_UNORDERED);/*把BTREE_UNORDERED设置为最近最多使用的值*/
	}
	else{
		distinct = addrDistinctIndex = -1;/*否则将distinct和addrDistinctIndex置为0*/
	}

	/* Aggregate and non-aggregate queries are handled differently
	  **聚合和非聚合查询被不同方式处理
	  */
	if (!isAgg && pGroupBy == 0){/*如果isAgg没有GroupBy*/
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
		if (addrSortIndex >= 0 && pOrderBy == 0){/*如果排序索引存在并且ORDERBY为0*/
			sqlite3VdbeChangeToNoop(v, addrSortIndex);/*将addrSortIndex中操作改为OP_Noop*/
			p->addrOpenEphm[2] = -1;/*将SELECT结构体打开临时表的下标为2的元素设为-1*/
		}

		if (pWInfo->eDistinct){/*如果返回的WHERE信息中含DISTINCT查询*/
			VdbeOp *pOp;                /* 不再需要OpenEphemeral（打开临时表）instr*/

			assert(addrDistinctIndex >= 0);/*插入断点，如果addrDistinctIndex小于0，抛出错误信息*/
			pOp = sqlite3VdbeGetOp(v, addrDistinctIndex);/*返回addrDistinctIndex 指向的操作码*/

			assert(isDistinct);/*插入断点，判断是否含有DISTINCT查询，如没有抛出错误信息*/
			assert(pWInfo->eDistinct == WHERE_DISTINCT_ORDERED/*插入断点，如果eDistinct为WHERE_DISTINCT_ORDERED*/
				|| pWInfo->eDistinct == WHERE_DISTINCT_UNIQUE/*或者为WHERE_DISTINCT_UNIQUE，否则就抛出错误信息*/
				);
			distinct = -1;/*将distinct置为-1，不进行取消重复操作*/
			if (pWInfo->eDistinct == WHERE_DISTINCT_ORDERED){/*如果WHERE返回信息中eDistinct为WHERE_DISTINCT_ORDERED*/
				int iJump;
				int iExpr;
				int iFlag = ++pParse->nMem;/*将语法解析树中分配内存的个数加加再赋值给iFlag*/
				int iBase = pParse->nMem + 1;/*将语法解析树中分配内存的个数加1再赋值给iBase*/
				int iBase2 = iBase + pEList->nExpr;/*将基址iBase+表达式个数再赋值给iBase2*/
				pParse->nMem += (pEList->nExpr * 2);/*将表达式乘2加上分配的内存数再赋值给pParse->nMem*/

				/* Change the OP_OpenEphemeral coded earlier to an OP_Integer. The
				** OP_Integer initializes the "first row" flag.  */
				pOp->opcode = OP_Integer;/*将OP_Integer操作赋值给pOp->opcode*/
				pOp->p1 = 1;/*将操作中p1操作置1（p1为打开临时表表达式）*/
				pOp->p2 = iFlag;/*再将iFlag标记赋值给p2*/

				sqlite3ExprCodeExprList(pParse, pEList, iBase, 1);/*把表达式列表pEList中的值iBase放到一系列的寄存器中*/
				iJump = sqlite3VdbeCurrentAddr(v) + 1 + pEList->nExpr + 1 + 1;/*<设置跳转地址>*/
				sqlite3VdbeAddOp2(v, OP_If, iFlag, iJump - 1);/*将OP_If操作交给vdbe，然后返回这个操作的地址*/
				for (iExpr = 0; iExpr < pEList->nExpr; iExpr++){/*遍历表达式*/
					CollSeq *pColl = sqlite3ExprCollSeq(pParse, pEList->a[iExpr].pExpr);/*那么根据分析语法树和列表达式生成一个排序队列，如果没有设置，返回默认*/
					sqlite3VdbeAddOp3(v, OP_Ne, iBase + iExpr, iJump, iBase2 + iExpr);/*将OP_Ne操作交给vdbe，然后返回这个操作的地址*/
					sqlite3VdbeChangeP4(v, -1, (const char *)pColl, P4_COLLSEQ);/*如果关键信息结构中地址值大于pColl,将P4_COLLSEQ的值设置到虚拟机*/
					sqlite3VdbeChangeP5(v, SQLITE_NULLEQ);/*把SQLITE_NULLEQ设置为最近最多使用的值*/
				}
				sqlite3VdbeAddOp2(v, OP_Goto, 0, pWInfo->iContinue);/*将OP_Goto操作交给vdbe，然后返回这个操作的地址*/

				sqlite3VdbeAddOp2(v, OP_Integer, 0, iFlag);/*将OP_Integer操作交给vdbe，然后返回这个操作的地址*/
				assert(sqlite3VdbeCurrentAddr(v) == iJump);/*插入断点，如果跳转的地址不为iJump，就抛出错误信息*/
				sqlite3VdbeAddOp3(v, OP_Move, iBase, iBase2, pEList->nExpr);/*将OP_Move操作交给vdbe，然后返回这个操作的地址*/
			}
			else{
				pOp->opcode = OP_Noop;/*将OP_Noop赋值给opcode操作符*/
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
			struct ExprList_item *pItem;  /* For looping over expression in a list *//*在一个列表中循环表达式*/

			for (k = p->pEList->nExpr, pItem = p->pEList->a; k > 0; k--, pItem++){/*遍历表达式*/
				pItem->iAlias = 0;/*将表达式列表中的关联依赖置为0*/
			}
			for (k = pGroupBy->nExpr, pItem = pGroupBy->a; k > 0; k--, pItem++){/*遍历GROUP BY子句*/
				pItem->iAlias = 0;/*将表达式列表中的关联依赖置为0*/
			}
			if (p->nSelectRow > (double)100) p->nSelectRow = (double)100;/*如果SELECT结果行大于100，赋值给SELECT的行数为100，限定了大小，超过100即为100*/
		}
		else{
			p->nSelectRow = (double)1;/*否知置为1*/
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
			int j1;             /* A-vs-B comparision jump *//*跳转到A与B进行比较*/
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
			
			sAggInfo.sortingIdx = pParse->nTab++;/*将语法解析树中的表（实际是表达式）的个数，赋值给聚集函数中排序索引*/
			pKeyInfo = keyInfoFromExprList(pParse, pGroupBy);/*将表达式列表中的pGroupBy提取关键信息给pKeyInfo*/
			addrSortingIdx = sqlite3VdbeAddOp4(v, OP_SorterOpen,
				sAggInfo.sortingIdx, sAggInfo.nSortingColumn,
				0, (char*)pKeyInfo, P4_KEYINFO_HANDOFF);/*添加一个OP_SorterOpen操作，并将它的值作为一个指针并赋值给addrSortingIdx*/

			/* Initialize memory locations used by GROUP BY aggregate processing
			**初始化被groupby 聚合处理的内存单元
			*/
			iUseFlag = ++pParse->nMem;/*将语法解析树中的内存空间加1赋值给iUseFlag*/
			iAbortFlag = ++pParse->nMem;/*将语法解析树中的内存空间加1赋值给iAbortFlag*/
			regOutputRow = ++pParse->nMem;/*将语法解析树中的内存空间加1赋值给regOutputRow*/
			addrOutputRow = sqlite3VdbeMakeLabel(v);/*为VDBE创建一个标签（指出继续运行的地址），返回值赋值给addrOutputRow*/
			regReset = ++pParse->nMem;/*将语法解析树中的内存空间加1赋值给regReset*/
			addrReset = sqlite3VdbeMakeLabel(v);/*为VDBE创建一个标签（指出继续运行的地址），返回值赋值给addrReset*/
			iAMem = pParse->nMem + 1;/*将语法解析树中的内存空间加1赋值给iAMem*/
			pParse->nMem += pGroupBy->nExpr;/*pGroupBy中表达式个数加语法解析树中的内存空间再赋值给语法解析树中的内存空间*/
			iBMem = pParse->nMem + 1;/*将语法解析树中的内存空间加1赋值iBMem*/
			pParse->nMem += pGroupBy->nExpr;/*pGroupBy中表达式个数加语法解析树中的内存空间再赋值给语法解析树中的内存空间*/
			sqlite3VdbeAddOp2(v, OP_Integer, 0, iAbortFlag);/*将OP_Integer操作交给vdbe，然后返回这个操作的地址*/
			VdbeComment((v, "clear abort flag"));;/*将“clear abort flag”放入到VDBE中*/
			sqlite3VdbeAddOp2(v, OP_Integer, 0, iUseFlag);/*将OP_Integer操作交给vdbe，然后返回这个操作的地址*/
			VdbeComment((v, "indicate accumulator empty"));/*将“indicate accumulator empty"放入到VDBE中*/
			sqlite3VdbeAddOp3(v, OP_Null, 0, iAMem, iAMem + pGroupBy->nExpr - 1);/*将OP_Null操作交给vdbe，然后返回这个操作的地址*/


			/* Begin a loop that will extract all source rows in GROUP BY order.
			** This might involve two separate loops with an OP_Sort in between, or
			** it might be a single loop that uses an index to extract information
			** in the right order to begin with.
			**启动一个循环，提取GROUP BY命令的所有的原列。
			*/
			sqlite3VdbeAddOp2(v, OP_Gosub, regReset, addrReset);/*将OP_Gosub操作交给vdbe，然后返回这个操作的地址*/
			pWInfo = sqlite3WhereBegin(pParse, pTabList, pWhere, &pGroupBy, 0, 0, 0);/*开始循环执行pGroupBy中语句，并返回结束指针*/
			if (pWInfo == 0) goto select_end;/*如果返回指针为0，跳到查询结束*/
			if (pGroupBy == 0){/*如果pGroupBy中不含group by子句*/
				/* The optimizer is able to deliver rows in group by order so
				** we do not have to sort.  The OP_OpenEphemeral table will be
				** cancelled later because we still need to use the pKeyInfo
				**
				**优化器能够发送出GROUP BY命令，不用再进行排序。
				**这个临时表OP_OpenEphemeral将会被取消，因为我们使用pKeyInfo
				*/
				pGroupBy = p->pGroupBy;/*将SELECT中pGroupBy赋值给pGroupBy*/
				groupBySort = 0;/*将GROUP BY中排序设为0*/
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


				groupBySort = 1;/*将GROUP BY排序置为1*/
				nGroupBy = pGroupBy->nExpr;/*将GROUP BY中的表达式个数赋值给nGroupBy*/
				nCol = nGroupBy + 1;/*将nGroupBy+1赋值给列数nCol*/
				j = nGroupBy + 1;/*将nGroupBy+1赋值给j*/
				for (i = 0; i < sAggInfo.nColumn; i++){/*如果排序的列号大于等于j*/
					if (sAggInfo.aCol[i].iSorterColumn >= j){
						nCol++;
						j++;
					}
				}
				regBase = sqlite3GetTempRange(pParse, nCol);/*为处理列分配寄存器，把寄存器起始值返回给regBase*/
				sqlite3ExprCacheClear(pParse);/*清除缓存中的语法解析树*/
				sqlite3ExprCodeExprList(pParse, pGroupBy, regBase, 0);/*把表达式列表pEList中的值起始值regBase放到已分配一系列的寄存器中*/
				sqlite3VdbeAddOp2(v, OP_Sequence, sAggInfo.sortingIdx, regBase + nGroupBy);/*将OP_Gosub操作交给vdbe，然后返回这个操作的地址*/
				j = nGroupBy + 1;/*将nGroupBy+1赋值给j*/
				for (i = 0; i < sAggInfo.nColumn; i++){/*遍历聚集函数信息中的列*/
					struct AggInfo_col *pCol = &sAggInfo.aCol[i]; /*声明一个聚集函数列结构体*/
					if (pCol->iSorterColumn >= j){/*如果排序的列号大于等于j*/
						int r1 = j + regBase;/*将基址寄存器的基址加上j赋值给r1*/
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
				sqlite3ReleaseTempReg(pParse, regRecord);/*释放语法解析树中临时寄存器regRecord的值*/
				sqlite3ReleaseTempRange(pParse, regBase, nCol);/*释放起始地址为regBase连续寄存器，长度是nCol*/
				sqlite3WhereEnd(pWInfo);/*判断WHERE返回信息，再结束WHERE子句*/
				sAggInfo.sortingIdxPTab = sortPTab = pParse->nTab++;/*将语法解析树中表个数赋值给聚集函数信息结构体中排序索引表*/
				sortOut = sqlite3GetTempReg(pParse);/*得到语法解析树中临时寄存器中的值给sortOut*/
				sqlite3VdbeAddOp3(v, OP_OpenPseudo, sortPTab, sortOut, nCol);/*将OP_OpenPseudo操作交给vdbe，然后返回这个操作的地址*/
				sqlite3VdbeAddOp2(v, OP_SorterSort, sAggInfo.sortingIdx, addrEnd);/*将OP_SorterSort操作交给vdbe，然后返回这个操作的地址*/
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
					sqlite3VdbeAddOp3(v, OP_Column, sortPTab, j, iBMem + j);/*将OP_Column操作交给vdbe，然后返回这个操作的地址*/
					if (j == 0) sqlite3VdbeChangeP5(v, OPFLAG_CLEARCACHE);/*把OPFLAG_CLEARCACHE设置为最近最多使用的值*/
				}
				else{
					sAggInfo.directMode = 1;
					sqlite3ExprCode(pParse, pGroupBy->a[j].pExpr, iBMem + j);/*将pExpr表达式和iBMem+j偏移量存放到语法解析树中*/
				}
			}
			sqlite3VdbeAddOp4(v, OP_Compare, iAMem, iBMem, pGroupBy->nExpr,
				(char*)pKeyInfo, P4_KEYINFO); /*添加一个OP_Compare操作，并将它的值作为一个指针*/
			j1 = sqlite3VdbeCurrentAddr(v);/*得到当前VDBE的地址，赋值给j1*/
			sqlite3VdbeAddOp3(v, OP_Jump, j1 + 1, 0, j1 + 1);/*将OP_Jump操作交给vdbe，然后返回这个操作的地址*/

			
			
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
			sqlite3VdbeAddOp2(v, OP_Gosub, regOutputRow, addrOutputRow);/*将OP_Gosub操作交给vdbe，然后返回这个操作的地址*/
			VdbeComment((v, "output final row"));/*将"output final row"放入到VDBE中*/

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
			sqlite3VdbeAddOp2(v, OP_IfPos, iUseFlag, addrOutputRow + 2);/*将OP_IfPos操作交给vdbe，然后返回这个操作的地址*/
			VdbeComment((v, "Groupby result generator entry point"));
			sqlite3VdbeAddOp1(v, OP_Return, regOutputRow);
			finalizeAggFunctions(pParse, &sAggInfo);
			sqlite3ExprIfFalse(pParse, pHaving, addrOutputRow + 1, SQLITE_JUMPIFNULL);
			selectInnerLoop(pParse, p, p->pEList, 0, 0, pOrderBy,
				distinct, pDest,
				addrOutputRow + 1, addrSetAbort); /*根据表达式p->pEList建立内连接，附带参数为OrderBy，distinct和pDest处理结果集*/
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
				sqlite3VdbeAddOp3(v, OP_OpenRead, iCsr, iRoot, iDb); /*将OP_OpenRead操作交给vdbe，然后返回这个操作的地址*/
				if (pKeyInfo){/*如果关键信息存在*/
					sqlite3VdbeChangeP4(v, -1, (char *)pKeyInfo, P4_KEYINFO_HANDOFF);/*如果关键信息结构中地址值大于pKeyInfo,将P4_KEYINFO_HANDOFF的值设置到虚拟机*/
				}
				sqlite3VdbeAddOp2(v, OP_Count, iCsr, sAggInfo.aFunc[0].iMem);/*将OP_Count操作交给vdbe，然后返回这个操作的地址*/
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
				u8 flag = minMaxQuery(p);/*对SELECT结构体p进行最大值或最小值查询，并赋值给flag*/
				if (flag){/*如果flag存在*/
					assert(!ExprHasProperty(p->pEList->a[0].pExpr, EP_xIsSelect));/*插入断点，如果p->pEList->a[0].pExpr中包含EP_xIsSelect属性不为空，抛出错误信息*/
					pMinMax = sqlite3ExprListDup(db, p->pEList->a[0].pExpr->x.pList, 0);/*插入断点，如果p->pEList->a[0].pExpr中包含EP_xIsSelect属性不为空，
					                                                                      抛出错误信息*/
					pDel = pMinMax;/*将查询结果赋值给pDel*/
					if (pMinMax && !db->mallocFailed){/*如果pMinMax存在并且分配内存成功*/
						pMinMax->a[0].sortOrder = flag != WHERE_ORDERBY_MIN ? 1 : 0;/*如果flag为WHERE_ORDERBY_MIN将1赋值给排序标记，否则赋值0*/
						pMinMax->a[0].pExpr->op = TK_COLUMN;/*将表达式中操作符赋值为TK_COLUMN*/
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
					sqlite3ExprListDelete(db, pDel);/*删除执行最值的表达式列表*/
					goto select_end;/*跳到查询结束*/
				}
				updateAccumulator(pParse, &sAggInfo);/*更新累加器内存单元*//*为当前游标位置上的聚集函数，更新累加器内存单元*/
				if (!pMinMax && flag){/*如果最值的表达式为空并且flag标记不为空*/
					sqlite3VdbeAddOp2(v, OP_Goto, 0, pWInfo->iBreak);/*将OP_Goto操作交给vdbe，跳到 pWInfo->iBreak执行语句，然后返回这个操作的地址*/
					VdbeComment((v, "%s() by index",
						(flag == WHERE_ORDERBY_MIN ? "min" : "max")));/*将"%s() by index"放入到VDBE中放入到VDBE中,其中flag=WHERE_ORDERBY_MIN，%s为min，否者为max*/
				}
				sqlite3WhereEnd(pWInfo);/*结束where 循环*/
				finalizeAggFunctions(pParse, &sAggInfo);/*结束聚合函数*/
			}

			pOrderBy = 0;
			sqlite3ExprIfFalse(pParse, pHaving, addrEnd, SQLITE_JUMPIFNULL);/*如果为true继续执行，如果为FALSE跳到addrEnd*/
			selectInnerLoop(pParse, p, p->pEList, 0, 0, 0, -1,
				pDest, addrEnd, addrEnd);/*根据表达式p->pEList建立内连接，附带参数addrEnd和pDest处理结果集*/
			sqlite3ExprListDelete(db, pDel);/*删除执行最值的表达式列表*/
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
		explainTempTable(pParse, "ORDER BY");/*输出信息"ORDER BY"到语法解析树*/
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
	explainSetInteger(pParse->iSelectId, iRestoreSelectId);/*结束语句，执行SELECT结束，并存储SELECT的ID*/

 /* Identify column names if results of the SELECT are to be output.
  **如果select 结果集要被输出，则标出列名
  */
	if (rc == SQLITE_OK && pDest->eDest == SRT_Output){
		generateColumnNames(pParse, pTabList, pEList);/*生成列名*/
	}

	sqlite3DbFree(db, sAggInfo.aCol);/*释放数据库连接中存储聚集函数信息的列的内存*/
	sqlite3DbFree(db, sAggInfo.aFunc);/*释放数据库连接中存储聚集函数信息的聚集函数名的内存*/
	return rc;/*返回执行结束标记*/
}

#if defined(SQLITE_ENABLE_TREE_EXPLAIN)
/*
** Generate a human-readable description of a the Select object.
**生成一个可读的select 对象的描述
*//*
** Generate a human-readable description of a the Select object.
**生成一个可读的select 对象的描述
*/
static void explainOneSelect(Vdbe *pVdbe, Select *p){
	sqlite3ExplainPrintf(pVdbe, "SELECT ");/*实际上调用sqlite3VXPrintf（），进行格式化输出"SELECT "*/
	if (p->selFlags & (SF_Distinct | SF_Aggregate)){/*有Distinct 或者Aggregate*/
		if (p->selFlags & SF_Distinct){/*有Distinct *//*如果selFlags为SF_Distinct*/
			sqlite3ExplainPrintf(pVdbe, "DISTINCT ");/*实际上调用sqlite3VXPrintf（），进行格式化输出"DISTINCT"*/
		}
		if (p->selFlags & SF_Aggregate){/*有Aggregate *//*如果selFlags为SF_Aggregate*/
			sqlite3ExplainPrintf(pVdbe, "agg_flag ");/*实际上调用sqlite3VXPrintf（），进行格式化输出"agg_flag "*/
		}
		sqlite3ExplainNL(pVdbe);/*附加上'|n' *//*添加一个换行符（'\n',前提是如果结尾没有）*/
		sqlite3ExplainPrintf(pVdbe, "   ");/*遍历FROM子句表达式列表*/
	}
	sqlite3ExplainExprList(pVdbe, p->pEList);/*为表达式列表p->pEList生成一个易读的描述信息*/
	sqlite3ExplainNL(pVdbe);/*添加一个换行符（'\n',前提是如果结尾没有）*/
	  if( p->pSrc && p->pSrc->nSrc ){/*遍历FROM子句表达式列表*/
		int i;
		sqlite3ExplainPrintf(pVdbe, "FROM ");/*实际上调用sqlite3VXPrintf（），进行格式化输出"FROM "*/
		sqlite3ExplainPush(pVdbe);/*在pVdbe中推出一个新的缩进级别，从光标开始的位置进行，后续的行都缩进*//*推进*/ 
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
		  if( pItem->zAlias ){/*如果表达式列表中有关联依赖*//*别名*/
			sqlite3ExplainPrintf(pVdbe, " (AS %s)", pItem->zAlias);/*实际上调用sqlite3VXPrintf（），进行格式化输出" (AS %s)"*/
		  }
		  if( pItem->jointype & JT_LEFT ){/*如果表达式列表中连接类型为JT_LEFT*//*左连接的解释*/
			sqlite3ExplainPrintf(pVdbe, " LEFT-JOIN");/*实际上调用sqlite3VXPrintf（），进行格式化输出" LEFT-JOIN"*/
		  }
		  sqlite3ExplainNL(pVdbe);/*添加一个换行符（'\n',前提是如果结尾没有）*/
		}
		sqlite3ExplainPop(pVdbe);/*弹出刚才压进栈的缩进级别*/
	  }
	  if( p->pWhere ){/*如果SELECT结构体p中含不为空where子句*//*where子句的解释*/
		sqlite3ExplainPrintf(pVdbe, "WHERE ");/*实际上调用sqlite3VXPrintf（），进行格式化输出"WHERE "*/
		sqlite3ExplainExpr(pVdbe, p->pWhere);/*为表达式树pWhere生成一个易读说明*/
		sqlite3ExplainNL(pVdbe);/*添加一个换行符（'\n',前提是如果结尾没有）*/
	  }
	  if( p->pGroupBy ){/*如果SELECT结构体p中含不为空where子句*//*groupby 子句的解释*/
		sqlite3ExplainPrintf(pVdbe, "GROUPBY ");/*实际上调用sqlite3VXPrintf（），进行格式化输出"GROUPBY "*/
		sqlite3ExplainExprList(pVdbe, p->pGroupBy);/*为表达式列表p->pGroupBy生成一个易读的描述信息*/
		sqlite3ExplainNL(pVdbe);/*添加一个换行符（'\n',前提是如果结尾没有）*/
	  }
	  if( p->pHaving ){/*如果SELECT结构体p中含不为空having子句*//*having 子句的解释*/
		sqlite3ExplainPrintf(pVdbe, "HAVING ");/*实际上调用sqlite3VXPrintf（），进行格式化输出 "HAVING "*/
		sqlite3ExplainExpr(pVdbe, p->pHaving);/*为表达式树pHaving生成一个易读说明*/
		sqlite3ExplainNL(pVdbe);/*添加一个换行符（'\n',前提是如果结尾没有）*/
	  }
	  if( p->pOrderBy ){/*如果SELECT结构体p中含不为空order by子句*//*orderby 子句的解释*/
		sqlite3ExplainPrintf(pVdbe, "ORDERBY ");/*实际上调用sqlite3VXPrintf（），进行格式化输出"ORDERBY "*/
		sqlite3ExplainExprList(pVdbe, p->pOrderBy);/*为表达式列表p->pOrderBy生成一个易读的描述信息*/
		sqlite3ExplainNL(pVdbe);/*添加一个换行符（'\n',前提是如果结尾没有）*/
	  }
	  if( p->pLimit ){/*如果SELECT结构体p中含不为空limit子句*//*Limit子句的解释*/
		sqlite3ExplainPrintf(pVdbe, "LIMIT ");/*实际上调用sqlite3VXPrintf（），进行格式化输出"LIMIT "*/
		sqlite3ExplainExpr(pVdbe, p->pLimit);/*为表达式树p->pLimit生成一个易读说明*/
		sqlite3ExplainNL(pVdbe);/*添加一个换行符（'\n',前提是如果结尾没有）*/
	  }
	  if( p->pOffset ){/*如果SELECT结构体p中含不为空offset子句*//*Offset 子句的解释*/
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
	  while( p->pPrior ) p = p->pPrior;/*递归，找到最优先查询的SELECT*//*如果select语句一直不为空，那么就将select语句的最前一个select赋给p*/
	  sqlite3ExplainPush(pVdbe);/*在pVdbe中推出一个新的缩进级别，从光标开始的位置进行，后续的行都缩进*//*推动一个新的缩进级别。将后续行缩进,这样他们在当前光标位置开始。*/
	  while( p ){/*只要p 不为空，就对p 进行*/
		explainOneSelect(pVdbe, p);/*生成一个易读描述SELECET的对象*//*生成一个可读的select 对象的描述*/
		p = p->pNext;/*将p的子树，赋值，给当前p*//*下一个select*/
		if( p==0 ) break;/*已经循环到最后一个了，没有了子节点*//*如果已经没有下一个select ，则break,并附加上'|n'*/
		sqlite3ExplainNL(pVdbe);/*添加一个换行符（'\n',前提是如果结尾没有）*//*附加上'|n'的函数的调用*/
		sqlite3ExplainPrintf(pVdbe, "%s\n", selectOpName(p->op));/*实际上是调用sqlite3VXPrintf（），并进行格式化输出为"%s\n"*/
	  }
	  sqlite3ExplainPrintf(pVdbe, "END");/*实际上是调用sqlite3VXPrintf（），进行格式化大的输出为"END"*/
	  sqlite3ExplainPop(pVdbe);/*对刚才压进栈的缩进级别进行弹出*/
	}
	/* End of the structure debug printing code 结束调试
	*****************************************************************************/
	#endif /* defined(SQLITE_ENABLE_TREE_EXPLAIN) */
