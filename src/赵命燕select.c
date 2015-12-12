/*
** 2001 September 15 zhaomingyan
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
**本文件包含SQLite中利用语法分析器处理SLEECT语句的C代码程序。SQLite的语法分析器使用Lemon LALR(1)分析程序生成器来产生，Lemon做的工作与YACC/BISON相同，但它使用不同的输入句法，这种句法更不易出错。Lemon还产生可重入的并且线程安全的语法分析器。Lemon定义了非终结析构器的概念，当遇到语法错误时它不会泄露内存。驱动Lemon的源文件可在parse.y中找到。
    因为lemon是一个在开发机器上不常见的程序，所以lemon的源代码（只是一个C文件）被放在SQLite的"tool"子目录下。 lemon的文档放在"doc"子目录下
*/

#include "sqliteInt.h"  /*对sqlitInt.h文件进行预处理，并交给编译器   sqlitInt.h为SQLite提供内部接口定义*/

/*
** Delete all the content of a Select structure but do not deallocate
** the select structure itself.
**删除查询结构的内容而不释放结构体本身。为了清除表达式
*/
static void clearSelect(sqlite3 *db, Select *p){//清除查询结构
  sqlite3ExprListDelete(db, p->pEList);   /*删除整个表达式列表*/
  sqlite3SrcListDelete(db, p->pSrc);   /*删除表达式列表中的FROM子句*/
  sqlite3ExprDelete(db, p->pWhere);     /*递归删除where子句*/
  sqlite3ExprListDelete(db, p->pGroupBy);   /*删除groupby子句*/
  sqlite3ExprDelete(db, p->pHaving);   /*递归删除having子句*/
  sqlite3ExprListDelete(db, p->pOrderBy);   /*删除orderby*/
  sqlite3SelectDelete(db, p->pPrior);   /*删除优先选择子句*/
  sqlite3ExprDelete(db, p->pLimit);    /*递归删除限制返回数据数量的子句*/
  sqlite3ExprDelete(db, p->pOffset);     /*递归删除偏移量offset子句*/
}

/*
** Initialize a SelectDest structure.
**初始化一个SelectDest结构.为了创建一个SelectDest,传入参数，定制一个结构体
*/
void sqlite3SelectDestInit(SelectDest *pDest, int eDest, int iParm){ 
  pDest->eDest = (u8)eDest; /*把整型eDest 强制类型转化为u8型，eDest是为了处理select操作结果*/
  pDest->iSDParm = iParm; /*整型参数iParm赋值为pDest->iSDParm，eDest的第几个处理方法，相当于设置eDest==SRT_Set，默认为0，表明没有设置*/
  pDest->affSdst = 0; /*0赋值给pDest->affSdst*/
  pDest->iSdst = 0; /*0赋值给pDest->iSdst，结果写在基址寄存器的编号，默认为0*/
  pDest->nSdst = 0; /*0赋值给pDest->nSdst，分配寄存器的数量*/
}


/*
** Allocate a new Select structure and return a pointer to that
** structure.
**分配一个新的选择结构并返回一个指向该结构体的指针.select语法分析最终在sqlite3SelectNew中完成，得到各个语法树汇总到Select结构体，然后根据结构体，进行语义分析生成执行计划。，
*/
Select *sqlite3SelectNew(/*select语法分析最终在sqlite3SelectNew中完成,它主要就是将之前得到的各个子语法树汇总到Select结构体，并根据该结构，进行接下来语义分析及生成执行计划等工作。*/
  Parse *pParse,        /* Parsing context  语义分析*/
  ExprList *pEList,     /* which columns to include in the result  存放表达式列表*/
  SrcList *pSrc,        /* the FROM clause -- which tables to scan  from存放from语法树---扫描表 */
  Expr *pWhere,         /* the WHERE clause  存放where语法树*/
  ExprList *pGroupBy,   /* the GROUP BY clause   存放group by语法树*/
  Expr *pHaving,        /* the HAVING clause  存放having语法树*/
  ExprList *pOrderBy,   /* the ORDER BY clause  存放order by语法树*/
  int isDistinct,       /* true if the DISTINCT keyword is present  如果关键字distinct存在，则返回true*/
  Expr *pLimit,         /* LIMIT value.  NULL means not used  limit值，如果值为空意味着limit未使用*/
  Expr *pOffset         /* OFFSET value.  NULL means no offset  offset值，如果值为空意味着offset未使用*/
){
  Select *pNew;/*创建一个select结构体指针pNew*/
  Select standin;/*创建一个select结构体类型变量standin*/
  sqlite3 *db = pParse->db;/*创建一个sqlite3结构体，这是主数据库的结构体，并将解析上下文中的数据赋值给它的数据指针*/
  pNew = sqlite3DbMallocZero(db, sizeof(*pNew) );  /* 分配和清空内存，如果分配失败，将mallocFaied标志放入连接指针中。*/
  assert( db->mallocFailed || !pOffset || pLimit ); /* 判断分配是否失败,或pOffset值为空,或pLimit值不为空，如果条件为真，则终止当前操作*/
  if( pNew==0 ){/*如果结构体指针变量pNew指向的地址为0，即创建结构体指针失败*/
    assert( db->mallocFailed );/*地址分配失败则中断当前操作*/
    pNew = &standin;/*把standin的存储地址赋给pNew*/
    memset(pNew, 0, sizeof(*pNew));/*将pNew中前sizeof（*pNew）个字节用0替换，并返回pNew*/
  }
  if( pEList==0 ){/*如果结构体指针pEList指向的地址为0,即表达式列表为空*/
    pEList = sqlite3ExprListAppend(pParse, 0, sqlite3Expr(db,TK_ALL,0)); /*新添加的元素在表达式列表的末尾。新添加元素
                                                                            的地址赋给pEList。如果pList的初始数据为空，
                                                                           那么新建 一个新的表达式列表。如果出现内存
                                                                            分配错误，则整个列表被释放并返回空。如果
                                                                            返回的是非空，则保证新的条目成功追加。
	                                                                  */
  }
  pNew->pEList = pEList;/*pEList指向元素的地址赋给pNew->pEList,即赋值Select结构体中的表达式列表属性*/
  if( pSrc==0 ) pSrc = sqlite3DbMallocZero(db, sizeof(*pSrc));/*分配并清空内存，分配大小为第二个参数的内存，如果分配失败，会在mallocFailed中做标记*/
  pNew->pSrc = pSrc;/*from子句*/
  pNew->pWhere = pWhere;
  pNew->pGroupBy = pGroupBy;
  pNew->pHaving = pHaving;
  pNew->pOrderBy = pOrderBy;
  pNew->selFlags = isDistinct ? SF_Distinct : 0;
  pNew->op = TK_SELECT;/*只能设置为TK_UNION TK_ALL TK_INTERSECT TK_EXCEPT 其中一个值*/
  pNew->pLimit = pLimit;
  pNew->pOffset = pOffset;
  assert( pOffset==0 || pLimit!=0 );
/**分配内存/
  pNew->addrOpenEphm[0] = -1;/*把-1赋值给pNew->addrOpenEphm[0] */
  pNew->addrOpenEphm[1] = -1;/*把-1赋值给pNew->addrOpenEphm[1] */
  pNew->addrOpenEphm[2] = -1;/*把-1赋值给pNew->addrOpenEphm[2] */
  if( db->mallocFailed ) {/*如果内存分配失败*/
    clearSelect(db, pNew);  /*删除所有选择的内容结构但不释放选择结构本身*/
    if( pNew!=&standin ) sqlite3DbFree(db, pNew); /*如果pNew没有获得standin的地址，释放相关联的内存。，即数据库连接失败 */
    pNew = 0;/*将select结构体设置为空*/
  }else{
    assert( pNew->pSrc!=0 || pParse->nErr>0 ););/*判断是否有From子句，或者是否有分析错误*/

  }
  assert( pNew!=&standin );/*判断Select结构体是否同替换结构体相同*/

  return pNew;/*返回这个构造好的Select结构体*/
}

/*
** Delete the given Select structure and all of its substructures.
**删除给定的选择结构和所有的子结构
*/
void sqlite3SelectDelete(sqlite3 *db, Select *p){/*传入数据库连接和Select结构体*/

  if( p ){/*如果结构体指针p指向的地址非空*/
    clearSelect(db, p);  /*删除所有选择的内容结构但不释放选择结构本身*/
    sqlite3DbFree(db, p);  /*空闲内存,可能被关联到一个特定的数据库连接。释放结构体*/
  }
}

/*
** Given 1 to 3 identifiers preceeding the JOIN keyword, determine the
** type of join.  Return an integer constant that expresses that type
** in terms of the following bit values:
**
**     JT_INNER
**     JT_CROSS
**     JT_OUTER
**     JT_NATURAL
**     JT_LEFT
**     JT_RIGHT
**
** A full outer join is the combination of JT_LEFT and JT_RIGHT.
**
** If an illegal or unsupported join type is seen, then still return
** a join type, but put an error in the pParse structure.
**将1 - 3标识符加入连接关键字前,确定连接方式。
**返回一个整数常数,表示使用以下的哪种连接类型: 
**JT_INNER 
**JT_CROSS 
**JT_OUTER 
**JT_NATURAL 
**JT_LEFT 
**JT_RIGHT 
**外连接是JT_LEFT和JT_RIGHT结合。
。
**如果存在一个非法或不受支持的连接类型,仍然返回一个
**连接类型,但在pParse结构中放入一个错误信息
*/
int sqlite3JoinType(Parse *pParse, Token *pA, Token *pB, Token *pC){/*传入分析树，三个令牌结构体*/

  int jointype = 0;/*确定连接类型*/
  Token *apAll[3];/*定义结构体指针数组apAll，存放令牌*/ 
  Token *p;/*定义结构体指针p，临时令牌*/
                             /*   0123456789 123456789 123456789 123 */
  static const char zKeyText[] = "naturaleftouterightfullinnercross";/*定义只读的且只能在当前模块中可见的字符型数组zKeyText，并对其进行赋值。存放连接类型*/
  static const struct {/*定义只读的且只能在当前模块可见结构体*/
    u8 i;        /* Beginning of keyword text in zKeyText[]   在KeyText[] 中的起始关键字*/
    u8 nChar;    /* Length of the keyword in characters  在字符中关键字的长度*/
    u8 code;     /* Join type mask 连接类型标记*/
  } aKeyword[] ={/*声明一个关键字数组，根据下标和长度找到连接类型*/
    /* natural 自然连接 */ { 0,  7, JT_NATURAL                }，/*下标从0开始，长度为7，自然连接*/

    /* left   左连接 */ { 6,  4, JT_LEFT|JT_OUTER          },/*下标从6开始，长度为4，左连接或外连接*/

    /* outer  外连接 */ { 10, 5, JT_OUTER                  },/*下标从10开始，长度为5，外连接*/

    /* right   右连接*/ { 14, 5, JT_RIGHT|JT_OUTER         },/*下标从14开始，长度为5，右连接或外连接*/

    /* full    全连接*/ { 19, 4, JT_LEFT|JT_RIGHT|JT_OUTER },/*下标从19开始，长度为4，左连接或右连接或外连接，实质是个全连接*/

    /* inner  内连接 */ { 23, 5, JT_INNER               },/*下标从23开始，长度为5，内连接*/
	
    /* cross   交叉连接*/ { 28, 5, JT_INNER|JT_CROSS    },/*下标从28开始，长度为5，内连接或CROSS连接，实质是个CROSS join*/

  };
  int i, j;
  apAll[0] = pA;/*指针pA指向的地址赋给apAll[0]，存放函数参数中的令牌pA*/
  apAll[1] = pB;/*指针pB指向的地址赋给apAll[1] */
  apAll[2] = pC;/*指针pc指向的地址赋给apAll[2] */
  for(i=0; i<3 && apAll[i]; i++){
    p = apAll[i];/*指针apAll[i]的地址赋给指针p*/
    for(j=0; j<ArraySize(aKeyword); j++){
      if( p->n==aKeyword[j].nChar  && sqlite3StrNICmp((char*)p->z, &zKeyText[aKeyword[j].i], p->n)==0 ){/*如果令牌中字符个数等于连接数组中的关键字长度且令牌中的字符与连接数组的字符相同*/
         
        jointype |= aKeyword[j].code;/*如果通过了比较长度和内容，返回连接类型，注意是，使用的是“位或”*/

        break;
      }
    }
    testcase( j==0 || j==1 || j==2 || j==3 || j==4 || j==5 || j==6 );/*利用测试代码中testcast，测试j值，是否在这个范围*/
    if( j>=ArraySize(aKeyword) ){/*如果j比连接关键字数组长度大*/
      jointype |= JT_ERROR;/*那就jointype与JT_ERROR“位或”，返回一个错误*/
      break;
    }
  }
  if(
     (jointype & (JT_INNER|JT_OUTER))==(JT_INNER|JT_OUTER) ||
     (jointype & JT_ERROR)!=0/*连接错误*/
  ){
    const char *zSp = " "; /*只读的字符型指针zSp*/
    assert( pB!=0 );/*判断令牌pB不为空*/
    if( pC==0 ){ zSp++; }/*如果指针pC指向的地址为0，那么指针zSp指向的地址前移一个存储单元*/
    sqlite3ErrorMsg(pParse, "unknown or unsupported join type: "  
       "%T %T%s%T", pA, pB, zSp, pC);  /*在语法分析树中存放错误消息unknown or unsupported join type*/
    jointype = JT_INNER;/*默认使用内连接*/
  }else if( (jointype & JT_OUTER)!=0 
         && (jointype & (JT_LEFT|JT_RIGHT))!=JT_LEFT ){
    sqlite3ErrorMsg(pParse, 
      "RIGHT and FULL OUTER JOINs are not currently supported");/*那么返回一个右连接和全外连接不被支持*/
    jointype = JT_INNER;/*默认使用内连接*/
  }
  return jointype;/*返回连接类型*/
}

/*
** Return the index of a column in a table.  Return -1 if the column
** is not contained in the table.
**返回一个表中的列的索引。如果该列没有包含在表中返回-1。
*/
static int columnIndex(Table *pTab, const char *zCol){/*定义静态的整型函数columnIndex，参数列表为结构体指针pTab、只读的字符型指针zCol，即表名及列名*/
  int i;
  for(i=0; i<pTab->nCol; i++){//遍历全表的所有列
    if( sqlite3StrICmp(pTab->aCol[i].zName, zCol)==0 ) return i;/*如果匹配成功，那么返回i*/
  }
  return -1;/*否则，返回-1*/
}

/*
** Search the first N tables in pSrc, from left to right, looking for a
** table that has a column named zCol.  
**在from语法树c中搜索第一个N表，然后从左向右，查找列中有名为zCol的表。
**
** When found, set *piTab and *piCol to the table index and column index
** of the matching column and return TRUE.
**当找到以后，设置* piTab和* piCol给表索引和匹配列的列索引，并返回TRUE 。
**
** If not found, return FALSE.
**如果没有找到，返回FALSE。
*/
static int tableAndColumnIndex(
  SrcList *pSrc,       /* Array of tables to search 存放待查找的表的数组*/
  int N,               /* Number of tables in pSrc->a[] to search 存放在pSrc->a[] 中搜索表的数目*/
  const char *zCol,    /* Name of the column we are looking for 存放需要查找的列的名字*/
  int *piTab,          /* Write index of pSrc->a[] here 存放表数组的索引*/
  int *piCol           /* Write index of pSrc->a[*piTab].pTab->aCol[] here 存放表数组中列的索引*/
){
  int i;               /* For looping over tables in pSrc 存放当前遍历的表数*/
  int iCol;            /* Index of column matching zCol   匹配成功的列的索引*/

  assert( (piTab==0)==(piCol==0) );  /* Both or neither are NULL  判断表索引和列索引都是或都不是为空*/
  for(i=0; i<N; i++){
    iCol = columnIndex(pSrc->a[i].pTab, zCol); /*返回表的列的索引赋给iCol，如果该列没有在表中，iCol的值是-1.*/
    if( iCol>=0 ){如果索列引存在
      if( piTab ){/*如果表索引存在*/
        *piTab = i;/*设置表索引*/
        *piCol = iCol;/*设置列索引*/
      }
      return 1;
    }
  }
  return 0;;/*空表返回0*/
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
	**    (tab1.col1 = tab2.col2)
	** tab1是表集合pSrc中的第i个表，tab2是第i+1个表。col1是tab1中索引，col2 是tab2d的索引。
	*/
static void addWhereTerm(
  Parse *pParse,                  /* Parsing context  语义分析*/
  SrcList *pSrc,                  /* List of tables in FROM clause   from字句中的列表 */
  int iLeft,                      /* Index of first table to join in pSrc  第一个连接的表的索引 */
  int iColLeft,                   /* Index of column in first table  第一个表的列索引*/
  int iRight,                     /* Index of second table in pSrc  第二个连接的表的索引*/
  int iColRight,                  /* Index of column in second table  第二个表的列索引*/
  int isOuterJoin,                /* True if this is an OUTER join  如果是外部连接返回true*/
  Expr **ppWhere                  /* IN/OUT: The WHERE clause to add to  where子句添加到in/out*/
){
  sqlite3 *db = pParse->db;   /*声明一个数据库连接*/
  Expr *pE1; /*定义结构体指针pE1，表达式*/
  Expr *pE2; /*定义结构体指针pE2*/
  Expr *pEq; /*定义结构体指针pEq*/

  assert( iLeft<iRight );/*判断如果第一个表的索引值是否小于第二个表的索引值*/
  assert( pSrc->nSrc>iRight );/*判断表集合中的表的数目是否大于右表的索引值*/
  assert( pSrc->a[iLeft].pTab);/*判断表集合中表中左表索引的表是否为空*/
  assert( pSrc->a[iRight].pTab );/*判断表集合中表中右表索引的表是否为空*/


  pE1 = sqlite3CreateColumnExpr(db, pSrc, iLeft, iColLeft);/*分配并返回一个表达式的指针，来加载表集合中左表的一个列索引*/
  pE2 = sqlite3CreateColumnExpr(db, pSrc, iRight, iColRight);/*分配并返回一个表达式的指针，来加载表集合中右表的一个列索引*/
  pEq = sqlite3PExpr(pParse, TK_EQ, pE1, pE2, 0););/*分配一个额外节点连接这两个子树表达式*/
  if( pEq && isOuterJoin ){/*如果定义结构体指针pEq指向的地址非空且为全连接*/
    ExprSetProperty(pEq, EP_FromJoin);/*那么在连接中使用ON或USING子句*/
    assert( !ExprHasAnyProperty(pEq, EP_TokenOnly|EP_Reduced) );
    ExprSetVVAProperty(pEq, EP_NoReduce);/*验证pEq是否可约束*/
    pEq->iRightJoinTable = (i16)pE2->iTable;
  }
  *ppWhere = sqlite3ExprAnd(db, *ppWhere, pEq);/*连接两个数据库*/
}

/*
** Set the EP_FromJoin property on all terms of the given expression.
** And set the Expr.iRightJoinTable to iTable for every term in the
** expression.
**
** The EP_FromJoin property is used on terms of an expression to tell
** the LEFT OUTER JOIN processing logic that this term is part of the
** join restriction specified in the ON or USING clause and not a part
** of the more general WHERE clause.  These terms are moved over to the
** WHERE clause during join processing but we need to remember that they
** originated in the ON or USING clause.
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
**所有给出的表达式的属性中都设定EP_FromJoin（FROM连接表达式）属性。并且给表达式中的每个属性都设置Expr.iRightJoinTable（连接第二个表）
**EP_FromJoin属性作为表达式的条款，表达左外连接的处理逻辑。它是ON或者USING特定限制连接中的一部分，
	**但通常不作为WHERE子句一部分。这些术语在表连接处理中移植到where中使用，我们需要记住它的来源于on或using子句中
	**Expr.iRightJoinTable告诉where子句处理要依靠iRightJoinTable表，即使这个表不能明确的表达。所需要的信息如下：
	** SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.b AND t1.x=5
	**where子句需要延时处理t1.x=5，直到t2循环连接完毕。通过这种方式，只要t1不等于5，也会向t2中插入空列。如果我们没有延时处理t1.x=5，将会在t1循环完后立即处理.t1不等于5的列就永远不会出现在输出，这是不正确的	*/
  static void setJoinExpr(Expr *p, int iTable){/*传入一个表达式，一个待连接的表*/
  while( p ){
    ExprSetProperty(p, EP_FromJoin);/*设置join中使用ON和USING子句*/

    assert( !ExprHasAnyProperty(p, EP_TokenOnly|EP_Reduced) );/*判断表达式的属性，关于表达式的长度和剩余长度*/

    ExprSetIrreducible(p);/*调试表达式，判读是否错误*/
    p->iRightJoinTable = (i16)iTable;/*把整型iTable强制类型转换为i16型，并将其赋值给p->iRightJoinTable。连接右表，即参数中传入的表*/
    setJoinExpr(p->pLeft, iTable);/*递归调用自身*/
    p = p->pRight;/*赋值表达式p为原来p的右子节点*/
  } 
}

/*
** This routine processes the join information for a SELECT statement.
** ON and USING clauses are converted into extra terms of the WHERE clause.
** NATURAL joins also create extra WHERE clause terms.
** The terms of a FROM clause are contained in the Select.pSrc structure.
** The left most table is the first entry in Select.pSrc.  The right-most
** table is the last entry.  The join operator is held in the entry to
** the left.  Thus entry 0 contains the join operator for the join between
** entries 0 and 1.  Any ON or USING clauses associated with the join are
** also attached to the left entry.
** This routine returns the number of errors encountered.
**这个函数用来处理在select语句中join信息。
	**ON和USING子句被转化为WHERE额外的子句。
	**自然连接也会创建额外的where子句。 
	**
	**from子句被Select.pSrc(Select结构体中FROM属性)所包含。
	**左边的表通常是Select.pSrc的入口。右边的表通常是最后一个入口（entry在hashmap中作为循环的节点入口，此处的理解的表连接遍历的记录入口）
	**join操作符在入口的左边。然后入口点0包含的连接操作符在入口0和入口1之间。任何涉及join的ON和USING子句，也将连接操作符放到左边入口。
	**
	**这段程序返回遇到错误的个数
*/
static int sqliteProcessJoin(Parse *pParse, Select *p){){/*传入分析树，Select结构体*/
  SrcList *pSrc;                  /* All tables in the FROM clause  from子句中的所有表*/
  int i, j;                       /* Loop counters  循环计数器*/
  struct SrcList_item *pLeft;     /* Left table being joined   被添加的左表*/
  struct SrcList_item *pRight;    /* Right table being joined   被添加的右表*/

  pSrc = p->pSrc;/*把p->pSrc赋值给结构体指针pSrc，from子句属性*/
  pLeft = &pSrc->a[0];/*把&pSrc->a[0]放入左表，表数组中的第一个表作为左表*/
  pRight = &pLeft[1];/*把&pLeft[1]放入右表，表数组中的第二个表作为右表*/
  for(i=0; i<pSrc->nSrc-1; i++, pRight++, pLeft++){/*遍历所有表*/
    Table *pLeftTab = pLeft->pTab;/*把pLeft->pTab赋给结构体指针pLeftTab*/
    Table *pRightTab = pRight->pTab;/*把pRight->pTab赋给结构体指针pRightTab*/
    int isOuter;

    if( NEVER(pLeftTab==0 || pRightTab==0) ) /*如果左表或右表有一个不为空*/
    continue;
    isOuter = (pRight->jointype & JT_OUTER)!=0;

    /* When the NATURAL keyword is present, add WHERE clause terms for
    ** every column that the two tables have in common.
    **当natural关键字存在，向where子句中加入两个表的共同列
    */
    if( pRight->jointype & JT_NATURAL ){/*如果右表的连接类型是自然连接*/

      if( pRight->pOn || pRight->pUsing ){/*如果右表有ON或USING子句*/
        sqlite3ErrorMsg(pParse, "a NATURAL join may not have "
           "an ON or USING clause", 0);/*输出错误信息，自然连接中不能含有ON USING子句*/
        return 1;
      }
      for(j=0; j<pRightTab->nCol; j++){/*循环右表的行*/
        char *zName;   /* Name of column in the right table 右表中列的名字*/
        int iLeft;     /* Matching left table 匹配左表*/
        int iLeftCol;  /* Matching column in the left table 在左表中匹配的列*/

        zName = pRightTab->aCol[j].zName;/*设置列名*/
        if( tableAndColumnIndex(pSrc, i+1, zName, &iLeft, &iLeftCol）){/*如果存在左表的列的索引*/
          addWhereTerm(pParse, pSrc, iLeft, iLeftCol, i+1, j,
                       isOuter, &p->pWhere);/*添加WHERE子句，设置左右表，列和连接方式*/
        }
      }
    }

    /* Disallow both ON and USING clauses in the same join
    **不允许on和using子句在同一个连接中
    */
    if( pRight->pOn && pRight->pUsing ){/*如果结构体指针pRight引用的成员变量On和pUsing非空，右表中既有ON又有USING*/
      sqlite3ErrorMsg(pParse, "cannot have both ON and USING "
        "clauses in the same join");/*输出错误信息*/
      return 1;
    }

    /* Add the ON clause to the end of the WHERE clause, connected by
    ** an AND operator.
    **on子句添加到where子句的末尾，由一个and操作符相连。
    */
    if( pRight->pOn ){/*如果结构体指针pRight引用的成员变量pOn非空，右表中有on关键字*/
      if( isOuter ) setJoinExpr(pRight->pOn, pRight->iCursor);/*若果是外连接，则将右表中的on子句与右表中的游标连接*/
      p->pWhere = sqlite3ExprAnd(pParse->db, p->pWhere, pRight->pOn);/*设置将WHERE子句与ON子句连接一起，赋值给结构体的WHERE*/
      pRight->pOn = 0;
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
    **如果using子句中提到的任何列不包含在连接的两个表中，就会报告
    **一个错误。
    */
    if( pRight->pUsing ){/*如果结构体指针pRight引用的成员变量pUsing非空，右表中有using子句*/
      IdList *pList = pRight->pUsing;/*把pRight->pUsing赋给结构体指针pList，将右表中的using子句赋值给标示符列表*/
      for(j=0; j<pList->nId; j++){/*遍历标识符列表*/
        char *zName;     /* Name of the term in the USING clause   using子句的名称*/
        int iLeft;       /* Table on the left with matching column name   左表的匹配列表*/
        int iLeftCol;    /* Column number of matching column on the left  左边匹配列的列数*/
        int iRightCol;   /* Column number of matching column on the right  右边匹配列的列数*/

        zName = pList->a[j].zName;/*标示符列表中的标示符*/
        iRightCol = columnIndex(pRightTab, zName); /*返回右表中与标识符匹配的列索引。如果该列没有在表中返回-1。返回值赋给iRightCol。
*/
        if( iRightCol<0
         || !tableAndColumnIndex(pSrc, i+1, zName, &iLeft, &iLeftCol)
        ){
          sqlite3ErrorMsg(pParse, "cannot join using column %s - column "
            "not present in both tables", zName);
          return 1;
        }
        addWhereTerm(pParse, pSrc, iLeft, iLeftCol, i+1, iRightCol,
                     isOuter, &p->pWhere)；/*添加到where子句*/
      }
    }
  }
  return 0;/*默认返回0，根据上文分析可得出生成了一个inner连接*/

}

/*
** Insert code into "v" that will push the record on the top of the
** stack into the sorter.
**插入代码"v"，在分选机将会推进记录到栈的顶部。
*/
static void pushOntoSorter(
  Parse *pParse,         /* Parser context  语义分析*/
  ExprList *pOrderBy,    /* The ORDER BY clause   order by语句语法树*/
  Select *pSelect,       /* The whole SELECT statement  整个select语句*/
  int regData            /* Register holding data to be sorted  注册数据进行排序*/
){
  Vdbe *v = pParse->pVdbe;/*声明一个虚拟机*/
  int nExpr = pOrderBy->nExpr;/*声明一个ORDERBY表达式*/
  int regBase = sqlite3GetTempRange(pParse, nExpr+2); /*分配一块连续的寄存器，大小为表达式的个数加2，返回一个整数值，
 */
  int regRecord = sqlite3GetTempReg(pParse); /*分配一个新的寄存器用于控制中间结果。*/
  int op;
  sqlite3ExprCacheClear(pParse); /*清除所有列的缓存条目*/
  sqlite3ExprCodeExprList(pParse, pOrderBy, regBase, 0);/*生成代码，将给定的表达式列表的每个元素的值放到寄存器开始的目标序列。返回元素评估的数量。*/
  sqlite3VdbeAddOp2(v, OP_Sequence, pOrderBy->iECursor, regBase+nExpr););/*将表达式放到VDBE中，再返回一个新的指令地址*/
  sqlite3ExprCodeMove(pParse, regData, regBase+nExpr+1, 1););/*更改寄存器中的内容，这样做能及时更新寄存器中的列缓存数据*/
  sqlite3VdbeAddOp3(v, OP_MakeRecord, regBase, nExpr + 2, regRecord);/*将nExpr放到当前使用的VDBE中，再返回一个新的指令的地址*/
  if( pSelect->selFlags & SF_UseSorter){/*如果select结构体中selFlags的值是SF_UseSorter，提一下，selFlags的值全是以SF开头，这个表示使用了分拣器了。*/
    op = OP_SorterInsert;/*因为使用分拣器，所以操作符设置为插入分拣器*/
  }else{
    op = OP_IdxInsert;/*否则使用索引方式插入*/
  }
  sqlite3VdbeAddOp2(v, op，, pOrderBy->iECursor, regRecord);/*将Orderby表达式放到当前使用的VDBE中，然后返回一个新的指令地址*/

  sqlite3ReleaseTempReg(pParse, regRecord); /*释放寄存器，使其可以从用于其他目的。如果一个寄存器当前被用于列缓存，则dallocation被推迟，直到使用的列寄存器变的陈旧*/ 
  sqlite3ReleaseTempRange(pParse, regBase, nExpr+2);/*释放regBase这个连续寄存器，长度是表达式的长度加2*/
  if( pSelect->iLimit ){/*如果使用Limit子句*/
    int addr1, addr2;
    int iLimit;
    if( pSelect->iOffset ){/*如果使用了Offset偏移量*/
      iLimit = pSelect->iOffset+1;/*那么Limit的值为偏移量加1*/
    }else{
      iLimit = pSelect->iLimit;/*否则等于默认值，从第一个开始计算*/
    }
    addr1 = sqlite3VdbeAddOp1(v, OP_IfZero, iLimit);/*这个地址是结果限制了返回的条数，给的新的指令地址*/
    sqlite3VdbeAddOp2(v, OP_AddImm, iLimit, -1);/*将指令放到当前使用的VDBE，然后返回一个地址*/
    addr2 = sqlite3VdbeAddOp0(v, OP_Goto);/*这个是使用Goto语句之后，返回的地址*/
    sqlite3VdbeJumpHere(v, addr1);/*改变addr1的地址，以便VDBE指向下一条指令的地址*/
    sqlite3VdbeAddOp1(v, OP_Last, pOrderBy->iECursor);/*将orderby指令放到当前使用的VDBE，然后返回last操作的地址*/
    sqlite3VdbeAddOp1(v, OP_Delete, pOrderBy->iECursor);/*将ORDERBY指令放到当前使用的虚拟机中，返回Delete操作的地址*/
    sqlite3VdbeJumpHere(v, addr2);/*改变addr2的地址，使其指向下一条指令的地址编码*/
  }
}

/*
** Add code to implement the OFFSET
**添加代码来实现offset偏移量功能。
*/
static void codeOffset(
  Vdbe *v,          /* Generate code into this VM  在VM中生成代码*/
  Select *p,        /* The SELECT statement being coded  select语句被编码*/
  int iContinue     /* Jump here to skip the current record  跳到这里而跳过当前记录*/
){
  if( p->iOffset && iContinue!=0 ){/*如果结构体指针p引用的成员iOffset非空且整型iContinue不等于0，select结构体中含有偏移量且设置了跳过当前记录*/
    int addr;
    sqlite3VdbeAddOp2(v, OP_AddImm, p->iOffset, -1);/*在VDBE中新添加一条指令，返回一个新指令的地址*/
    addr = sqlite3VdbeAddOp1(v, OP_IfNeg, p->iOffset);/*将偏移量放到当前使用的虚拟机中，返回OP_IfNeg的地址*/
    sqlite3VdbeAddOp2(v, OP_Goto, 0, iContinue);/*设置跳往的地址*/
    VdbeComment((v, "skip OFFSET records"));/*输入偏移量*/
    sqlite3VdbeJumpHere(v, addr);/*改变addr的地址，使其指向下一条指令的地址编码*/
  }
}

/*
** Add code that will check to make sure the N registers starting at iMem
** form a distinct entry.  iTab is a sorting index that holds previously
** seen combinations of the N values.  A new entry is made in iTab
** if the current N values are new.
** A jump to addrRepeat is made and the N+1 values are popped from the
** stack if the top N elements are not distinct.
** 编写代码检查确定一个iMem表中N个注册者在一个单独的入口。
** iTab是一个分类索引，预先能看到N个值的组合。如果在iTab中新存在一个N值，那么将会产生一个新的入口在iTab 中。
** 如果N+1个值突然从栈中弹出，其中N个值是不唯一的，那么将会产生大量重复的地址（addrRepeat）
*/

static void codeDistinct(
  Parse *pParse,     /* Parsing and code generating context 语义和代码生成*/
  int iTab,          /* A sorting index used to test for distinctness 排序索引用于唯一性测试*/
  int addrRepeat,    /* Jump to here if not distinct 如果不明显跳转到这里*/
  int N,             /* Number of elements 元素数目*/
  int iMem           /* First element 第一个元素*/
){
  Vdbe *v;/*定义结构体指针v*/
  int r1;

  v = pParse->pVdbe;/*把结构体成员pParse->pVdbe赋给结构体指针v，声明一个处理数据库字节码的引擎*/
  r1 = sqlite3GetTempReg(pParse); /*分配一个新的寄存器用于存储中间结果，返回的整数赋给r1.*/
  sqlite3VdbeAddOp4Int(v, OP_Found, iTab, addrRepeat, iMem, N); /*添加一个操作值，包括整型的p4值。*/
  sqlite3VdbeAddOp3(v, OP_MakeRecord, iMem, N, r1);/*修改指令地址*/
  sqlite3VdbeAddOp2(v, OP_IdxInsert, iTab, r1);/*修改指令地址*/
  sqlite3ReleaseTempReg(pParse, r1);/*生成代码，将给定的表达式列表的每个元素的值放到寄存器开始的目标序列。返回元素评估的数量。*/
}

#ifndef SQLITE_OMIT_SUBQUERY/*测试SQLITE_OMIT_SUBQUERY是否被宏定义过*/
/*
** Generate an error message when a SELECT is used within a subexpression
** (example:  "a IN (SELECT * FROM table)") but it has more than 1 result
** column.  We do this in a subroutine because the error used to occur
** in multiple places.  (The error only occurs in one place now, but we
** retain the subroutine to minimize code disruption.)
**当一个select语句中使用子表达式就产生一个错误的信息(例如:a in(select * from table))，
**因为它有多于1的结果列。我们在子程序中这样做是因为错误通常发生在多个地方。 
**(现在错误只发生在一个地方，但是我们保留子程序来降低代码中断。)
*/
static int checkForMultiColumnSelectError(
  Parse *pParse,       /* Parse context. 语义分析 */
  SelectDest *pDest,   /* Destination of SELECT results   select的结果集*/
  int nExpr            /* Number of result columns returned by SELECT  select返回的结果列数*/
){
  int eDest = pDest->eDest;/*处理结果集*/
  if( nExpr>1 && (eDest==SRT_Mem || eDest==SRT_Set)){/*如果结果集大于1并且select的结果集是SRT_Mem或SRT_Set*/
    sqlite3ErrorMsg(pParse, "only a single result allowed for "
       "a SELECT that is part of an expression");/*为pParse- > zErrMsg和增量pParse- > NERR添加一条错误消息，即在语法分析树中写一个错误信息 */

    return 1;/*有select结果集*/
  }else{
    return 0;
  }
}
#endif/*终止if*/

/*
** This routine generates the code for the inside of the inner loop
** of a SELECT.
**这个程序生成一个select 的内循环的内部代码。
**
** If srcTab and nColumn are both zero, then the pEList expressions
** are evaluated in order to get the data for this row.  If nColumn>0
** then data is pulled from srcTab and pEList is used only to get the
** datatypes for each column.
**如果数据表和列都是零，那么pEList表达式为了获得行数据进行赋值。
**如果nColumn>0 ，即srcTab中的列数>0，那么数据从srcTab中拉出，pEList只用于从每一列获得数据类型。
*/
static void selectInnerLoop(
  Parse *pParse,          /* The parser context 语义分析*/
  Select *p,              /* The complete select statement being coded 完整的被编码select语句*/
  ExprList *pEList,       /* List of values being extracted  输出结果列的语法树*/
  int srcTab,             /* Pull data from this table 从这个表中提取数据*/
  int nColumn,            /* Number of columns in the source table  源表中列的数目*/
  ExprList *pOrderBy,     /* If not NULL, sort results using this key 如果不是NULL，使用这个关键字对结果进行排序*/
  int distinct,           /* If >=0, make sure results are distinct 如果>=0，对结果进行“去重复操作”*/
  SelectDest *pDest,      /* How to dispose of the results 怎样处理结果*/
  int iContinue,          /* Jump here to continue with next row 跳到这里继续下一行*/
  int iBreak              /* Jump here to break out of the inner loop 跳到这里中断内部循环*/
){
  Vdbe *v = pParse->pVdbe;/*声明一个虚拟机*/
  int i;
  int hasDistinct;        /* True if the DISTINCT keyword is present 如果distinct关键字存在返回true*/
  int regResult;              /* Start of memory holding result set 结果集的起始处*/
  int eDest = pDest->eDest;   /* How to dispose of results 怎样处理结果*/
  int iParm = pDest->iSDParm; /* First argument to disposal method 第一个参数的处理方法*/
  int nResultCol;             /* Number of result columns 结果列的数目*/

  assert( v );/*判断是否存在虚拟机*/
  if( NEVER(v==0) ) return;/*如果虚拟机不存在，直接返回*/
  assert( pEList!=0);/*判断表达式列表是否为空*/
  hasDistinct = distinct>=0;/*赋值“去除重复”操作符*/
  if( pOrderBy==0 && !hasDistinct ){
    codeOffset(v, p, iContinue); /*添加代码来实现offset。*/
  }

  /* Pull the requested columns.
  **取出请求列
  */
  if( nColumn>0 ){/*如果表中列的数目大于0*/
    nResultCol = nColumn;/*赋值结果列的值为源表的列数*/
  }else{
    nResultCol = pEList->nExpr;/*把结构体pEList的成员nExpr的值赋给nResultCol*/
  }
  if( pDest->iSdst==0 ){/*如果结构体pDest引用的成员iSdst的值为0，即查询数据集的写入结果的基址寄存器的值为0*/
*/
    pDest->iSdst = pParse->nMem+1;/*那么基址寄存器的值设为分析语法树的下一个地址*/
    pDest->nSdst = nResultCol;/*注册寄存器的数量为结果列的数量*/
    pParse->nMem += nResultCol;/*分析树的地址设为自身的再加上结果列的数量*/
  }else{ 
    assert( pDest->nSdst==nResultCol);/*判断结果集中寄存器的个数是否与结果列的列数相同*/
  }
  regResult = pDest->iSdst;/*再把储存结果集的寄存器的地址设为结果集的起始地址*/
  if( nColumn>0 ){
    for(i=0; i<nColumn; i++){
      sqlite3VdbeAddOp3(v, OP_Column, srcTab, i, regResult+i);/*将队列操作送入到VDBE再返回新的指令地址*/
    }
  }else if( eDest!=SRT_Exists ){/*如果处理的结果集不存在*/
    /* If the destination is an EXISTS(...) expression, the actual
    ** values returned by the SELECT are not required.
    **如果目标是一个EXISTS(...)表达式，由select返回的实际值是不需要的。
    */
    sqlite3ExprCacheClear(pParse);/*清除语法分析树的缓存*/
    sqlite3ExprCodeExprList(pParse, pEList, regResult, eDest==SRT_Output);/*生成代码，将给定的表达式列表的每个元素的值放到寄存器开始的目标序列。返回元素评估的数量。*/
  }
  nColumn = nResultCol;/*赋值列数等于结果列的列数*/

  /* If the DISTINCT keyword was present on the SELECT statement
  ** and this row has been seen before, then do not make this row
  ** part of the result.
  **如果distinct关键字在select语句中出现，这行之前已经见过，那么这行不作为结果的一部分。
  */
  if( hasDistinct ){
    assert( pEList!=0 );
    assert( pEList->nExpr==nColumn );
    codeDistinct(pParse, distinct, iContinue, nColumn, regResult);/*进行“去除重复操作”*/
    if( pOrderBy==0 ){
      codeOffset(v, p, iContinue); /*添加代码来实现offset。*/
    }
  }

  switch( eDest ){/*定义switch函数，根据参数eDest判断怎样处理结果*/
    /* In this mode, write each query result to the key of the temporary
    ** table iParm.
    **在这种模式下，给临时表iParm写入每个查询结果。
    */
#ifndef SQLITE_OMIT_COMPOUND_SELECT/*测试SQLITE_OMIT_COMPOUND_SELECT是否被宏定义过*/
    case SRT_Union: {/*如果eDest为SRT_Union，则结果作为关键字存储在索引*/
      int r1;
      r1 = sqlite3GetTempReg(pParse);/*分配一个新的寄存器控制中间结果，返回值赋给r1*/
     sqlite3VdbeAddOp3(v, OP_MakeRecord, regResult, nColumn, r1);/*把OP_MakeRecord（做记录）操作送入VDBE，再返回一个新指令地址*/
     sqlite3VdbeAddOp2(v, OP_IdxInsert, iParm, r1);/*把OP_IdxInsert（索引插入）操作送入VDBE，再返回一个新指令地址*/
      sqlite3ReleaseTempReg(pParse, r1);/*释放寄存器，使其可以从用于其他目的。如果一个寄存器当前被用于列缓存， 则dallocation被推迟，直到使用的列寄存器变的陈旧*/
      break;
    }

    /* Construct a record from the query result, but instead of
    ** saving that record, use it as a key to delete elements from
    ** the temporary table iParm.
    **构建一种查询结果集的记录格式，但不是保存该记录，将其作为从临时表iParm删除元素的一个关键字。
    */
    case SRT_Except: {/*如果eDest为SRT_Except，则从union索引中移除结果*/
      sqlite3VdbeAddOp3(v, OP_IdxDelete, iParm, regResult, nColumn); /*添加一个新的指令VDBE指示当前的列表。返回新指令的地址。*/
      break;
    }
#endif/*终止if*/

    /* Store the result as data using a unique key.
    **数据使用唯一关键字存储结果
    */
    case SRT_Table:/*如果eDest为SRT_Table，则结果按照自动的rowid自动保存*/
    case SRT_EphemTab: {/*如果eDest为SRT_EphemTab，则创建临时表并存储为像SRT_Table的表*/
      int r1 = sqlite3GetTempReg(pParse); /*分配一个新的寄存器用于控制中间结果，并把返回值赋给r1*/
      testcase( eDest==SRT_Table);/*测试处理的结果集的表名称*/
      testcase( eDest==SRT_EphemTab );/*测试处理的结果集的表的大小*/
      sqlite3VdbeAddOp3(v, OP_MakeRecord, regResult, nColumn, r1);/*添加一个新的指令VDBE指示当前的列表。返回新指令的地址。*/
      if( pOrderBy ){
        pushOntoSorter(pParse, pOrderBy, p, r1);/*插入代码"V"，在分选机将会推进记录到栈的顶部，即把orderby送到分拣器的栈顶*/
      }else{
        int r2 = sqlite3GetTempReg(pParse);/*分配一个新的寄存器用于控制中间结果，并把返回值赋给r2*/
        sqlite3VdbeAddOp2(v, OP_NewRowid, iParm, r2);/*把OP_NewRowid（新建记录）操作送入VDBE，再返回一个新指令地址*/
        sqlite3VdbeAddOp3(v, OP_Insert, iParm, r1, r2);/*把OP_Insert（插入记录）操作送入VDBE，再返回一个新指令地址*/
        sqlite3VdbeChangeP5(v, OPFLAG_APPEND);  /*对于最新添加的操作，改变p5操作数的值。*/
        sqlite3ReleaseTempReg(pParse, r2);/*释放寄存器，使其可以从用于其他目的。如果一个寄存器当前被用于列缓存，则dallocation被推迟，直到使用的列寄存器变的陈旧*/
      }
      sqlite3ReleaseTempReg(pParse, r1);
      break;
    }

#ifndef SQLITE_OMIT_SUBQUERY/*测试SQLITE_OMIT_SUBQUERY是否被宏定义过*/
    /* If we are creating a set for an "expr IN (SELECT ...)" construct,
    ** then there should be a single item on the stack.  Write this
    ** item into the set table with bogus data.
    **如果我们为"expr IN (SELECT ...)" 创建一组构造，那么在堆栈上就应该
    **有一个单独的对象。把这个对象写入虚假数据表。
    */
    case SRT_Set: {/*如果eDest为SRT_Set，则结果作为关键字存入索引*/
      assert( nColumn==1 );
      p->affinity = sqlite3CompareAffinity(pEList->a[0].pExpr, pDest->affSdst);/*根据表和结果集，存储结构体的亲和性结果集*/
      if( pOrderBy ){
        /* At first glance you would think we could optimize out the
        ** ORDER BY in this case since the order of entries in the set
        ** does not matter.  But there might be a LIMIT clause, in which
        ** case the order does matter 
        **刚开始会认为对整个集合的顺序进行ORDERBY优化认为不重要.但是，使用了LIMIT关键字时候，排序就重要了。
        */
        pushOntoSorter(pParse, pOrderBy, p, regResult);/*进行ORDERBY操作，然后放到分拣器的栈顶*/
      }else{
        int r1 = sqlite3GetTempReg(pParse);/*分配一个寄存器，存储中间计算结果*/
        sqlite3VdbeAddOp4(v, OP_MakeRecord, regResult, 1, r1, &p->affinity, 1); /*添加一个操作码，其中包括作为指针的p4值。*/
        sqlite3ExprCacheAffinityChange(pParse, regResult, 1);/*记录从istart开始发生icount寄存器中的改变的事实*/
        sqlite3VdbeAddOp2(v, OP_IdxInsert, iParm, r1);
        sqlite3ReleaseTempReg(pParse, r1);
      }
      break;
    }

    /* If any row exist in the result set, record that fact and abort.
    **如果任何一行在结果集中存在，记录这一事实并中止。
    */
    case SRT_Exists: {/*如果eDest为SRT_Exists，则结果若不为空存储1*/
      sqlite3VdbeAddOp2(v, OP_Integer, 1, iParm);
      /* The LIMIT clause will terminate the loop for us 
      limit子句将终止循环
      */
      break;
    }

    /* If this is a scalar select that is part of an expression, then
    ** store the results in the appropriate memory cell and break out
    ** of the scan loop.*/
   /*如果标量查询是这个表达式的一部分，然后存储这个结果在适当的内存单元中，
      从循环扫描中释放出
*/
    case SRT_Mem: {/*如果eDest为SRT_Mem，则将结果存储在存储单元*/
      assert( nColumn==1 );
      if( pOrderBy ){
        pushOntoSorter(pParse, pOrderBy, p, regResult);/*把ORDERBY操作记录放到分拣器的栈顶*/
      }else{
        sqlite3ExprCodeMove(pParse, regResult, iParm, 1);/*释放寄存器中的内容，保持寄存器的内容及时更新*/
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
      testcase( eDest==SRT_Output ); /*测试处理结果集是否要输出*/
      if( pOrderBy ){
        int r1 = sqlite3GetTempReg(pParse);/*分配一个寄存器，存储中间计算结果*/
        sqlite3VdbeAddOp3(v, OP_MakeRecord, regResult, nColumn, r1);/*把OP_MakeRecord操作送入VDBE，再返回一个新指令地址*/
        pushOntoSorter(pParse, pOrderBy, p, r1);/*把ORDERBY操作记录放到分拣器的栈顶*/
        sqlite3ReleaseTempReg(pParse, r1);/*释放这个寄存器*/
      }else if( eDest==SRT_Coroutine ){/*如果处理结果集是协同处理*/
        sqlite3VdbeAddOp1(v, OP_Yield, pDest->iSDParm);/*把OP_Yield操作送入VDBE，再返回一个新指令地址*/
}else{
        sqlite3VdbeAddOp2(v, OP_ResultRow, regResult, nColumn);/*把OP_ResultRow操作送入VDBE，再返回一个新指令地址*/
        sqlite3ExprCacheAffinityChange(pParse, regResult, nColumn);/*记录亲和类型的数据的改变的计数寄存器的起始地址*/
      }
      break;
    }

#if !defined(SQLITE_OMIT_TRIGGER)/*条件编译*/
    /* Discard the results.  This is used for SELECT statements inside
    ** the body of a TRIGGER.  The purpose of such selects is to call
    ** user-defined functions that have side effects.  We do not care
    ** about the actual results of the select.
    **丢弃结果。这是用于触发器的select语句。这样选择的目的是要调用用户定义函数。
    **我们不关心实际的选择结果。
    */
    default: {/*默认条件下*/
      assert( eDest==SRT_Discard);/*如果处理结果集是SRT_Discard（舍弃）*/
      break;
    }
#endif/*条件编译结束*/
  }

  /* Jump to the end of the loop if the LIMIT is reached.  Except, if
  ** there is a sorter, in which case the sorter has already limited
  ** the output for us.
  **如果到达limit处，跳转到循环结束处。如果这是一个分拣器，这个分拣器会限制我们的输出结果
  */
  if( pOrderBy==0 && p->iLimit ){/*如果不包含ORDERBY和limit子句*/
    sqlite3VdbeAddOp3(v, OP_IfZero, p->iLimit, iBreak, -1);/*把OP_IfZero操作送入VDBE，再返回一个新指令地址*/
  }
}

/*
** Given an expression list, generate a KeyInfo structure that records
** the collating sequence for each expression in that expression list.
**给定一个表达式列表，生成一个关键信息结构，记录在该表达式列表中的每个表达式的排序序列。
**
** If the ExprList is an ORDER BY or GROUP BY clause then the resulting
** KeyInfo structure is appropriate for initializing a virtual index to
** implement that clause.  If the ExprList is the result set of a SELECT
** then the KeyInfo structure is appropriate for initializing a virtual
** index to implement a DISTINCT test.
**如果表达式是一个order by或者group by子句，那么关键信息结构体适合初始化虚拟索引去实现
**这些子句。如果表达式是select的结果集，那么关键信息结构体适合初始化一个虚拟索引去实
**现DISTINCT测试。
**
** Space to hold the KeyInfo structure is obtain from malloc.  The calling
** function is responsible for seeing that this structure is eventually
** freed.  Add the KeyInfo structure to the P4 field of an opcode using
** P4_KEYINFO_HANDOFF is the usual way of dealing with this.
**保存关键信息结构体的空间是由malloc获得。回调函数负责最后释放
**关键信息结构添加到使用P4_KEYINFO_HANDOFF P4的一个操作码是通常的处理方式。
*/
static KeyInfo *keyInfoFromExprList(Parse *pParse, ExprList *pList){/*定义静态的结构体指针函数keyInfoFromExprList，传入两个参数，一个为语法分析树，一个为表达式列表*/
  sqlite3 *db = pParse->db;/*把结构体类型是pParse的成员变量db赋给结构体类型是sqlite3的指针db，声明数据库连接*/
  int nExpr;
  KeyInfo *pInfo;/*定义结构体类型是KeyInfo的指针pInfo，声明关键字结构体*/
  struct ExprList_item *pItem;/*定义结构体类型是ExprList_item的指针pItem，声明表达式项结构体*/
  int i;

  nExpr = pList->nExpr){/*传入两个参数，一个为语法分析树，一个为表达式列表*/
  pInfo = sqlite3DbMallocZero(db, sizeof(*pInfo) + nExpr*(sizeof(CollSeq*)+1) );/*分配并清空内存，分配大小为第二个参数的内存*/
  if( pInfo ){/*如果存在关键字结构体*/
    pInfo->aSortOrder = (u8*)&pInfo->aColl[nExpr];/*设置关键信息结构体的排序为关键信息结构体中表达式中含有的关键字，其中aColl[1]表示为每一个关键字进行整理*/
    pInfo->nField = (u16)nExpr;/*对整型nExpr进行强制类型转换成u16，赋给 pInfo->nField,关键信息结构体中aColl的长度为表达式的个数*/
    pInfo->enc = ENC(db);/*关键信息结构体中编码方式为db的编码方式*/
    pInfo->db = db;/*关键信息结构体中数据库为当前使用的数据库*/
    for(i=0, pItem=pList->a; i<nExpr; i++, pItem++){/*遍历当前的表达式列表*/
      CollSeq *pColl;/*声明一个整理顺序的程序，使用它可以定义排序的名字*/
      pColl = sqlite3ExprCollSeq(pParse, pItem->pExpr); /*为表达式pExpr返回默认排序顺序。如果没有默认排序 类型，返回0.*/
      if( !pColl ){/*如果结构体指针pColl非空,没有指定排序的方法*/
        pColl = db->pDfltColl;/*将数据库中默认的排序方法赋值给pColl*/
}
      pInfo->aColl[i] = pColl;/*关键信息结构体中对关键字排序数组中元素对应表达式中排序的名称*/
      pInfo->aSortOrder[i] = pItem->sortOrder;/*把引用的成员pItem->sortOrder赋给pInfo->aSortOrder[i],关键信息结构体中排序的顺序为语法分析树中语法项表达式的排序方法*/
*/
    }
  }
  return pInfo;
}

#ifndef SQLITE_OMIT_COMPOUND_SELECT/*测试SQLITE_OMIT_COMPOUND_SELECT是否被宏定义过*/
/*
** Name of the connection operator, used for error messages.
**连接符的名称，用于错误消息。
*/
static const char *selectOpName(int id){/*定义静态且是只读的字符型指针selectOpName*/
  char *z;/*定义字符型指针z*/
  switch( id ){/*switch函数*/
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
**否则，它增加一个单独的输出行到EQP结果，标题的形式为:
**"USE TEMP B-TREE FOR xxx"
**其中xxx是"distinct","order by",或者"group by"中的一个。究竟是哪个由
**zUsage参数决定。
*/
static void explainTempTable(Parse *pParse, const char *zUsage){
  if( pParse->explain==2 ){/*如果pParse->explain与字符z相同,语法分析树中的explain是第二个*/
    Vdbe *v = pParse->pVdbe;
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
**赋值表达式b给左值a，当定义了SQLITE_OMIT_EXPLAIN每秒中都很多的空操作。只有在没有更改代码和没有定义SQLITE_OMIT_EXPLAIN
** 的情况下，才为sqlite3Select()中成员变量赋值。
# define explainSetInteger(a, b) a = b/*宏定义*/

#else
/* No-op versions of the explainXXX() functions and macros. 无操作符版本的explainXXX() 函数和宏。*/
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
**除非一个"EXPLAIN QUERY PLAN"命令正在处理，这个功能就是一个空操作。
**否则，它增加一个单独的输出行到EQP结果，标题的形式为:

**"COMPOSITE SUBQUERIES iSub1 and iSub2 (op)"
**"COMPOSITE SUBQUERIES iSub1 and iSub2 USING TEMP B-TREE (op)"

**iSub1和iSub2整数作为相应的传递函数参数，运算是相同名称的参数
**的文本表示。参数"op"必是TK_UNION, TK_EXCEPT,TK_INTERSECT或者TK_ALL之一。
**如果使用了一张临时表，将会使用第一范式，或者使用第二范式。
*/
static void explainComposite(
  Parse *pParse,                  /* Parse context 语义分析*/
  int op,                         /* One of TK_UNION, TK_EXCEPT etc.   TK_UNION, TK_EXCEPT等之一*/
  int iSub1,                      /* Subquery id 1 子查询id 1*/
  int iSub2,                      /* Subquery id 2 子查询id 2*/
  int bUseTmp                     /* True if a temp table was used 如果临时表被使用就是true*/
){
  assert( op==TK_UNION || op==TK_EXCEPT || op==TK_INTERSECT || op==TK_ALL );
  if( pParse->explain==2 ){/*如果pParse->explain与字符z相同，使用了解释方式是语法分析树中第二种方式*/
    Vdbe *v = pParse->pVdbe;/*声明一个虚拟机*/
    char *zMsg = sqlite3MPrintf(/*设置标记信息*/
        pParse->db, "COMPOUND SUBQUERIES %d AND %d %s(%s)", iSub1, iSub2,
        bUseTmp?"USING TEMP B-TREE ":"", selectOpName(op)
   );/*将子查询1和子查询2的语法内容赋值给zMsg*/
    sqlite3VdbeAddOp4(v, OP_Explain, pParse->iSelectId, 0, 0, zMsg, P4_DYNAMIC);/*将OP_Explain操作交给虚拟机，然后返回一个地址，地址为P4_DYNAMIC指针中的值*/
  }
}
#else
/* No-op versions of the explainXXX() functions and macros. 无操作版本的explainXXX()函数和宏。*/
# define explainComposite(v,w,x,y,z)
#endif

/*
** If the inner loop was generated using a non-null pOrderBy argument,
** then the results were placed in a sorter.  After the loop is terminated
** we need to run the sorter and output the results.  The following
** routine generates the code needed to do that.
**如果内部循环使用一个非空pOrderBy生成参数,然后把结果放置在一个分选机。 
**循环终止后我们需要运行分选机和输出结果。下面的例程生成所需的代码。 
*/
static void generateSortTail(
  Parse *pParse,    /* Parsing context 语义分析*/
  Select *p,        /* The SELECT statement   select语句*/
  Vdbe *v,          /* Generate code into this VDBE  在VDBE中生成代码**/
  int nColumn,      /* Number of columns of data 数据的列数目*/
  SelectDest *pDest /* Write the sorted results here 在这里写入排序结果*/
){
  int addrBreak = sqlite3VdbeMakeLabel(v);     /* Jump here to exit loop 跳转到这里退出循环*/
  int addrContinue = sqlite3VdbeMakeLabel(v);  /* Jump here for next cycle 跳转到这里进行下一个周期*/
  int addr;
  int iTab;
  int pseudoTab = 0;
  ExprList *pOrderBy = p->pOrderBy;/*将Select结构体中ORDERBY赋值到表达式列表中的ORDERBY表达式属性*/

  int eDest = pDest->eDest;/*将查询结果集中处理方式传递给eDest*/
  int iParm = pDest->iSDParm;/*将查询结果集中处理方式中的参数传递给iParm*/

  int regRow;
  int regRowid;

  iTab = pOrderBy->iECursor;/*将关联ExprList的VDBE游标传给iTab*/
  regRow = sqlite3GetTempReg(pParse);/*为pParse语法树分配一个寄存器,存储计算的中间结果*/
  if( eDest==SRT_Output || eDest==SRT_Coroutine ){/*如果处理方式是SRT_Output（输出）或SRT_Coroutine（协同程序）*/
    pseudoTab = pParse->nTab++;/*将分析语法树中表数传给pseudoTab
    sqlite3VdbeAddOp3(v, OP_OpenPseudo, pseudoTab, regRow, nColumn);/*将OP_Explain操作交给虚拟机*/
    regRowid = 0;
  }else{
    regRowid = sqlite3GetTempReg(pParse);/*为pParse语法树分配一个寄存器,存储计算的中间结果*/
  }
  if( p->selFlags & SF_UseSorter){/*如果Select结构体中的selFlags属性值为SF_UseSorter，使用分拣器（排序程序）*/
    int regSortOut = ++pParse->nMem;/*分配寄存器，个数是分析语法树中内存数+1*/
    int ptab2 = pParse->nTab++;/*将分析语法树中表的个数赋值给ptab2*/
    sqlite3VdbeAddOp3(v, OP_OpenPseudo, ptab2, regSortOut, pOrderBy->nExpr+2);/*将OP_OpenPseudo（打开虚拟操作）交给VDBE，返回表达式列表中表达式个数的值+2*/
    addr = 1 + sqlite3VdbeAddOp2(v, OP_SorterSort, iTab, addrBreak);/*将OP_SorterSort（分拣器进行排序）交给VDBE，返回的地址+1赋值给addr*/
    codeOffset(v, p, addrContinue);/*添加代码来实现offset，其中addrContinue是下一次循环要调到的地址*/
    sqlite3VdbeAddOp2(v, OP_SorterData, iTab, regSortOut);/*将OP_SorterData操作交给虚拟机*/
    sqlite3VdbeAddOp3(v, OP_Column, ptab2, pOrderBy->nExpr+1, regRow);/*将OP_Column操作交给虚拟机*/
    sqlite3VdbeChangeP5(v, OPFLAG_CLEARCACHE);/*改变OPFLAG_CLEARCACHE（清除缓存）的操作数，因为地址经过sqlite3VdbeAddOp3和sqlite3VdbeAddOp2（）函数改变了地址*/
  }else{
    addr = 1 + sqlite3VdbeAddOp2(v, OP_Sort, iTab, addrBreak);/*将OP_Sort操作交给虚拟机，返回的地址+1*/
    codeOffset(v, p, addrContinue);/*设置偏移量，其中addrContinue是下一次循环要调到的地址*/
    sqlite3VdbeAddOp3(v, OP_Column, iTab, pOrderBy->nExpr+1, regRow);/*将OP_Column操作交给VDBE，再把OP_Column的地址返回*/
  }
  switch( eDest ){/*switch函数，参数eDest*/
    case SRT_Table:/*如果eDest为SRT_Table，则结果按照自动的rowid自动保存*/
    case SRT_EphemTab: {/*如果eDest为SRT_EphemTab，则创建临时表并存储为像SRT_Table的表*/
      testcase( eDest==SRT_Table );
      testcase( eDest==SRT_EphemTab );
      sqlite3VdbeAddOp2(v, OP_NewRowid, iParm, regRowid);/*将OP_NewRowid操作交给VDBE，再返回这个操作的地址*/
      sqlite3VdbeAddOp3(v, OP_Insert, iParm, regRow, regRowid);/*将OP_Insert操作交给VDBE，再返回这个操作的地址*/
      sqlite3VdbeChangeP5(v, OPFLAG_APPEND);/*改变OPFLAG_APPEND（设置路径），因为地址经过sqlite3VdbeAddOp2（）和sqlite3VdbeAddOp3（）函数改变了地址*/
      break;
    }
#ifndef SQLITE_OMIT_SUBQUERY/*测试SQLITE_OMIT_SUBQUERY是否被宏定义过*/
    case SRT_Set: {/*如果eDest为SRT_Set，则结果作为关键字存入索引*/
      assert( nColumn==1 );
      sqlite3VdbeAddOp4(v, OP_MakeRecord, regRow, 1, regRowid, &p->affinity, 1);/*添加一个OP_MakeRecord操作，并将它的值作为一个指针*/
      sqlite3ExprCacheAffinityChange(pParse, regRow, 1); /*记录从iStart开始，发生在iCount寄存器中的改变的事实。*/
      sqlite3VdbeAddOp2(v, OP_IdxInsert, iParm, regRowid);/*将OP_IdxInsert（索引插入）操作交给VDBE，再返回这个操作的地址*/
      break;
    }
    case SRT_Mem: {/*如果eDest为SRT_Mem，则将结果存储在存储单元*/
      assert( nColumn==1 );
      sqlite3ExprCodeMove(pParse, regRow, iParm, 1);/*释放寄存器中的内容，保持寄存器的内容及时更新*/
      /* The LIMIT clause will terminate the loop for us 
      **limit子句将终止我们的循环
      */
      break;
    }
#endif/*终止if*/
    default: {/*默认条件*/
      int i;
      assert( eDest==SRT_Output || eDest==SRT_Coroutine ); /*判断结果集处理类型是否有SRT_Output（输出）或SRT_Coroutine（协同处理）*/
      testcase( eDest==SRT_Output);/*测试是否包含SRT_Output*/
      testcase( eDest==SRT_Coroutine );/*测试是否包含SRT_Coroutine*/
      for(i=0; i<nColumn; i++){
        assert( regRow!=pDest->iSdst+i );/*判断寄存器的编号值不等于基址寄存器的编号值+i*/
        sqlite3VdbeAddOp3(v, OP_Column, pseudoTab, i, pDest->iSdst+i);/*将OP_Column操作交给VDBE，再返回这个操作的地址*/
        if( i==0 ){/*如果没有列*/
          sqlite3VdbeChangeP5(v, OPFLAG_CLEARCACHE);/*改变OPFLAG_CLEARCACHE（清除缓存）的操作数，因为地址经过sqlite3VdbeAddOp3（）函数改变了地址*/
        }
      }
      if( eDest==SRT_Output ){/*如果结果集的处理方式是输出*/
        sqlite3VdbeAddOp2(v, OP_ResultRow, pDest->iSdst, nColumn);/*将OP_ResultRow操作交给VDBE，再返回这个操作的地址*/
        sqlite3ExprCacheAffinityChange(pParse, pDest->iSdst, nColumn);/*处理语法树pParse，寄存器中的亲和性数据*/
      }else{
        sqlite3VdbeAddOp1(v, OP_Yield, pDest->iSDParm);/*将OP_Yield操作交给VDBE，再返回这个操作的地址*/
      }
      break;
    }
  }
  sqlite3ReleaseTempReg(pParse, regRow);/*释放寄存器*/
  sqlite3ReleaseTempReg(pParse, regRowid);/*释放寄存器*/

  /* The bottom of the loop               
  **循环的底部
  */
  sqlite3VdbeResolveLabel(v, addrContinue);/*addrContinue作为下一条插入指令的地址，其中addrContinue能优先调用sqlite3VdbeMakeLabel（）*/
  if( p->selFlags & SF_UseSorter ){
    sqlite3VdbeAddOp2(v, OP_SorterNext, iTab, addr);/*将OP_SorterNext操作交给VDBE，再返回这个操作的地址*/
  }else{
    sqlite3VdbeAddOp2(v, OP_Next, iTab, addr);/*将OP_Next操作交给VDBE，再返回这个操作的地址*/
  }
  sqlite3VdbeResolveLabel(v, addrBreak);/*addrBreak作为下一条插入指令的地址，其中addrBreak能优先调用sqlite3VdbeMakeLabel（）*/
  if( eDest==SRT_Output || eDest==SRT_Coroutine ){/*如果结果集的处理方式SRT_Output或SRT_Coroutine*/
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
**返回一个包含'declaration type'（声明类型）表达式字符串。
**这个字符串可以视为静态调用者。

** 如果这个表达式是个列，那么列的声明类型应该在最初创建表格时被准确定义。这**个声明的ID应该是个整数.当一个表达式
**被认为作为一列在子查询中是复杂的。在所有下面的SELECT语句
**的结果集的表达被认为是这个函数的列。

**   SELECT col FROM tbl;
**   SELECT (SELECT col FROM tbl;
**   SELECT (SELECT col FROM tbl);
**   SELECT abc FROM (SELECT col AS abc FROM tbl);

**声明类型以外的任何表达式列是空的。
*/
static const char *columnType(/*定义静态且是只读的字符型指针columnType*/
  NameContext *pNC/*声明一个命名上下文结构体（决定表或者列的名字）*/
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
  if( NEVER(pExpr==0) || pNC->pSrcList==0 ) return 0;/*如果一个表达式为空或决定名字的列表是空的，直接返回0*/

  switch( pExpr->op){/*遍历表达式中的操作*/
    case TK_AGG_COLUMN:
    case TK_COLUMN: {
      /* The expression is a column. Locate the table the column is being
      ** extracted from in NameContext.pSrcList. This table may be real
      ** database table or a subquery.
      **表达式是一个列。被定位的表的列从从命名上下文中的pSrcList（一个或多个表用来命名）属性中提取出来的。
      **这个表可能是真实的数据库表，或者是一个子查询。
      */
      Table *pTab = 0;            /* Table structure column is extracted from 表结构列被提取*/
      Select *pS = 0;             /* Select the column is extracted from 选择列被提取*/
      int iCol = pExpr->iColumn;  /* Index of column in pTab 索引列在pTab中*/
      testcase( pExpr->op==TK_AGG_COLUMN);/*这个表达式的操作是否是TK_AGG_COLUMN（嵌套列）*/
      testcase( pExpr->op==TK_COLUMN );/*这个表达式的操作是否是TK_COLUMN（列索引）*/
      while( pNC && !pTab ){/*命名上下文结构体存在，被提取的表结构列（就是一个被提取的列组成的表）不存在*/
        SrcList *pTabList = pNC->pSrcList;/*命名上下文结构体中列表赋值给描述FROM的来源表或子查询结果的列表*/
        for(j=0;j<pTabList->nSrc && pTabList->a[j].iCursor!=pExpr->iTable;j++);/*遍历查询列表*/
        if( j<pTabList->nSrc){/*如果j小于列表中表的总个数*/
          pTab = pTabList->a[j].pTab;/*赋值列表中的第j-1表给Table结构体的实体变量pTab*/
          pS = pTabList->a[j].pSelect;/*赋值列表中的第j-1表的select结构体给ps*/
        }else{
          pNC = pNC->pNext;/*否则，将命名上下文结构体的下一个外部命名上下文赋值给pNC变量*/
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
        ** when columnType() is called on the expression "t1.col" in the 
        ** sub-select. In this case, set the column type to NULL, even
        ** though it should really be "INTEGER".
        **
        ** This is not a problem, as the column type of "t1.col" is never
        ** used. When columnType() is called on the expression 
        ** "(SELECT t1.col)", the correct type is returned (see the TK_SELECT
        ** branch below.  
        **在同一时间，诸如触发器内"SELECT new.x "代码将导致这种状态运行。自那时以来，
        **我们已重组触发代码是如何生成的，因此该条件不再可能。但是，它仍然可以适用于
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

      assert( pTab && pExpr->pTab==pTab );/*判断被提取的列组成的表存在或者表达式中的列表存在*/
      if( pS ){
        /* The "table" is actually a sub-select or a view in the FROM clause
        ** of the SELECT statement. Return the declaration type and origin
        ** data for the result-set column of the sub-select.
        **"表"实际上是一个子选择，或者是一个在select语句中from子句的视图。
        **返回声明类型和来源数据的子选择的结果集列。
        */
        if( iCol>=0 && ALWAYS(iCol<pS->pEList->nExpr) ){
          /* If iCol is less than zero, then the expression requests the
          ** rowid of the sub-select or view. This expression is legal (see 
          ** test case misc2.2.2) - it always evaluates to NULL.
          **如果iCol列号小于零，则表达式请求子选择或视图的物理地址。
          **这种表达式合法的(见测试案例misc2.2.2)-它始终计算为空。
          */
          NameContext sNC;
          Expr *p = pS->pEList->a[iCol].pExpr;/*被提取的列组成的select结构体中表达式列表中第i个表达式赋值给p*/
          sNC.pSrcList = pS->pSrc;/*被提取的列组成的select结构体中pSrc（FROM子句）赋值给pSrcList（一个或多个表用来命名的属性）*/
          sNC.pNext = pNC;/*命名上下文结构体赋值给当前命名上下文结构体的next指针*/
          sNC.pParse = pNC->pParse;/*命名上下文结构体中的语法解析树赋值给当前命名上下文结构体的语法解析树*/
          zType = columnType(&sNC, p, &zOriginDb, &zOriginTab, &zOriginCol); /*将生成的属性类型赋值给zType*/
        }
      }else if( ALWAYS(pTab->pSchema) ){/*pTab表的模式存在*/
        /* A real table 一个真实的表*/
        assert( !pS );/*判断Select结构体是否为空*/
        if( iCol<0 ) iCol = pTab->iPKey;/*如果列号小于0，将表中的关键字数组的首元素赋值给ICol*/
        assert( iCol==-1 || (iCol>=0 && iCol<pTab->nCol) );/*判断ICol正确，在哪个范围*/
        if( iCol<0 ){
          zType = "INTEGER";/*将类型定义为整型*/
          zOriginCol = "rowid";/*关键字为rowid*/
        }else{
          zType = pTab->aCol[iCol].zType;/*否则，定义类型为类型表中第iCol的类型*/
          zOriginCol = pTab->aCol[iCol].zName;/*类型的名字为型表中第iCol的名字*/}
        zOriginTab = pTab->zName;/*使用默认的名字，定义类型*/
        if( pNC->pParse){/*如果命名上下文结构体中的语法分析树存在*/
          int iDb = sqlite3SchemaToIndex(pNC->pParse->db, pTab->pSchema);/*将Schema的指针转化给命名上下文结构体中分析语法树的db*/
          zOriginDb = pNC->pParse->db->aDb[iDb].zName;/*将上下文语法分析树中的db中第i-1个Db的命名赋值给zOriginDb数据库名*/
}
      }
      break;
    }
#ifndef SQLITE_OMIT_SUBQUERY/*测试SQLITE_OMIT_SUBQUERY是被宏定义过*/
    case TK_SELECT: {
      /* The expression is a sub-select. Return the declaration type and
      ** origin info for the single column in the result set of the SELECT
      ** statement.
      **表达式是一个子选择。返回一个声明类型和初始信息给select结果集中的一列。
      */
      NameContext sNC;
      Select *pS = pExpr->x.pSelect;/*将表达式中Select结构体赋值给一个SELECT结构体实体变量*/
      Expr *p = pS->pEList->a[0].pExpr;/*将SELECT的表达式列表中第一个表达式赋值给表达式变量p*/
      assert( ExprHasProperty(pExpr, EP_xIsSelect) );/*测试是否包含EP_xIsSelect表达式*/
      sNC.pSrcList = pS->pSrc;/*将SELECT结构体中FROM子句的属性赋值给命名上下文结构体中FROM子句列表*/
      sNC.pNext = pNC;/*命名上下文结构体赋值给当前命名上下文结构体的next指针*/
      sNC.pParse = pNC->pParse;/*将命名上下文结构体中分析语法树赋值给当前命名结构体的分析语法树属性*/
      zType = columnType(&sNC, p, &zOriginDb, &zOriginTab, &zOriginCol); /*返回属性类型*/ 
      break;
    }
#endif
  }
  
  if( pzOriginDb ){/*如果存在原始的数据库*/
    assert( pzOriginTab && pzOriginCol );/*判断表和列是否存在*/
    *pzOriginDb = zOriginDb;/*文件中数据库赋值给数据库变量pzOriginDb*/
    *pzOriginTab = zOriginTab;/*文件中表赋值给表变量pzOriginTab*/
    *pzOriginCol = zOriginCol;/*文件中列赋值给列变量pzOriginCol*/
  }
  return zType;/*返回列类型*/
}
