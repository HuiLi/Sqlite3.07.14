/*

	** ��ΰ�����C���Գ��򣬴���SQLite�е�select��䡣
	*/
	#include "sqliteInt.h"


	/*
	** Delete all the content of a Select structure but do not deallocate
	** the select structure itself.
	*/

	/*
	**ɾ����ѯ�ṹ���е��������ݣ������ͷŲ�ѯ�ṹ�塣����������ṹ���е����ݡ�
	**
	**�Լ�ע��ÿ�����ݿ����ӱذ���sqlite3.����������У�����Ϊ�����Select���������е����ݣ�ʵ����������ʽ�б��еı��ʽ
	*/
	static void clearSelect(sqlite3 *db, Select *p){
	  sqlite3ExprListDelete(db, p->pEList);/*���select�ṹ���еĲ�ѯ���*/
	  sqlite3SrcListDelete(db, p->pSrc);/*�ӱ��ʽ�б������FROM�Ӿ���ʽ*/
	  sqlite3ExprDelete(db, p->pWhere);/*�ӱ��ʽ�б������where�Ӿ���ʽ*/
	  sqlite3ExprListDelete(db, p->pGroupBy);/*�ӱ��ʽ�б������group by�Ӿ���ʽ*/
	  sqlite3ExprDelete(db, p->pHaving);/*�ӱ��ʽ�б������Having�Ӿ���ʽ*/
	  sqlite3ExprListDelete(db, p->pOrderBy);/*�ӱ��ʽ�б������Order by�Ӿ���ʽ*/
	  sqlite3SelectDelete(db, p->pPrior);/*�ӱ��ʽ�б����������ѡ���Ӿ���ʽ*/
	  sqlite3ExprDelete(db, p->pLimit);/*�ӱ��ʽ�б����������ѡ���Ӿ���ʽ*/
	  sqlite3ExprDelete(db, p->pOffset);/*�ӱ��ʽ�б������ƫ����Offset�Ӿ���ʽ*/
	}

	/*
	** Initialize a SelectDest structure.
	*/

	/*
	** ��ʼ����ѯ�ṹ
	**
	**�Լ�ע��Ϊ�˴���һ��SelectDest,�������������һ���ṹ��
	*/

	void sqlite3SelectDestInit(SelectDest *pDest, int eDest, int iParm){
	  pDest->eDest = (u8)eDest;/*u8��һ���޷������ͣ�eDest��Ϊ�˴���select�������*/
	  pDest->iSDParm = iParm;/*eDest�ĵڼ���������*/
	  pDest->affSdst = 0;/*	�൱������eDest==SRT_Set��Ĭ��Ϊ0������û������*/
	  pDest->iSdst = 0;/*���д�ڻ�ַ�Ĵ����ı�ţ�Ĭ��Ϊ0*/
	  pDest->nSdst = 0;/*����Ĵ���������*/
	}


	/*
	** Allocate a new Select structure and return a pointer to that
	** structure.
	*/

	/*
	** ����һ���µĲ�ѯ�ṹ������һ��ָ�롣
	**
	**�Լ�ע��select�﷨������������sqlite3SelectNew����ɣ��õ������﷨�����ܵ�Select�ṹ�壬Ȼ����ݽṹ�壬���������������ִ�мƻ���
	*/

	Select *sqlite3SelectNew(
	  Parse *pParse,        /* Parsing context *//*����������*/
	  ExprList *pEList,     /* which columns to include in the result *//*����һ�����ʽ�б������ű��ʽ */
	  SrcList *pSrc,        /* the FROM clause -- which tables to scan *//*��FROM�Ӿ䣬ɨ������Щ�� */
	  Expr *pWhere,         /* the WHERE clause *//*�����Ӿ���ʽ��where�Ӿ�*/
	  ExprList *pGroupBy,   /* the GROUP BY clause *//*�������ʽ�б��Group by�Ӿ���ʽ*/
	  Expr *pHaving,        /* the HAVING clause *//*����һ�����ʽ��Having���ʽ*/
	  ExprList *pOrderBy,   /* the ORDER BY clause *//*�������ʽ�б��Order by�Ӿ���ʽ*/
	  int isDistinct,       /* true if the DISTINCT keyword is present *//*�����Ƿ�ʹ��distinct�ؼ��֣��ٷ��ĵ�˵Ĭ�ϲ���*/
	  Expr *pLimit,         /* LIMIT value.  NULL means not used *//*����һ�����ʽ��Limit�Ӿ���ʽ */
	  Expr *pOffset         /* OFFSET value.  NULL means no offset *//*����һ�����ʽ��Offsetƫ�����Ӿ���ʽ*/
	){
	  Select *pNew;         /*����һ��Select�ṹ��*/
	  Select standin;       /*����һ��Select�ṹ�壬�����滻*/
	  sqlite3 *db = pParse->db;/*����һ��sqlite3�ṹ�壬���������ݿ�Ľṹ��*/
	  pNew = sqlite3DbMallocZero(db, sizeof(*pNew) );/*���䲢����ڴ棬�����СΪ�ڶ����������ڴ�*/
	  assert( db->mallocFailed || !pOffset || pLimit ); /* OFFSET implies LIMIT *//*ƫ�����а�����Limit*/
	  if( pNew==0 ){          /*���û�д���*/
		assert( db->mallocFailed );/*�����ڴ�*/
		pNew = &standin;           /*��pNewָ���滻��Select�ṹ��ĵ�ַ	*/
		memset(pNew, 0, sizeof(*pNew)); /*��pNew��ǰsizeof(*pNew)���ֽ���0�滻*/
	  }
	  if( pEList==0 ){ /*������ʽ�б�Ϊ��*/
		pEList = sqlite3ExprListAppend(pParse, 0, sqlite3Expr(db,TK_ALL,0));/*�ڱ��ʽ�б��׷��һ�����ʽ�����û�б��ʽ�б���½�һ���ټ���*/
	  }
	  pNew->pEList = pEList;/*��ֵSelect�ṹ���еı��ʽ�б�����*/
	  if( pSrc==0 ) pSrc = sqlite3DbMallocZero(db, sizeof(*pSrc));/*���䲢����ڴ棬�����СΪ�ڶ����������ڴ棬�������ʧ�ܣ�����mallocFailed�������*/
	  pNew->pSrc = pSrc;/*ΪSelect�ṹ����FROM�Ӿ���ʽ��ֵ*/
	  pNew->pWhere = pWhere;/*ΪSelect�ṹ����Where�Ӿ���ʽ��ֵ*/
	  pNew->pGroupBy = pGroupBy;/*ΪSelect�ṹ����GroupBy�Ӿ���ʽ��ֵ*/
	  pNew->pHaving = pHaving;/*ΪSelect�ṹ����Having�Ӿ���ʽ��ֵ*/
	  pNew->pOrderBy = pOrderBy;/*ΪSelect�ṹ����OrderBy�Ӿ���ʽ��ֵ*/
	  pNew->selFlags = isDistinct ? SF_Distinct : 0;/*����SF_*�е�ֵ*/
	  pNew->op = TK_SELECT;/*ֻ������ΪTK_UNION TK_ALL TK_INTERSECT TK_EXCEPT ����һ��ֵ*/
	  pNew->pLimit = pLimit;/*ΪSelect�ṹ����Limit�Ӿ���ʽ��ֵ*/
	  pNew->pOffset = pOffset;/*ΪSelect�ṹ����Offset�Ӿ���ʽ��ֵ*/
	  assert( pOffset==0 || pLimit!=0 );/*���ƫ����Ϊ�գ�Limit��Ϊ�գ������ڴ�*/
	  pNew->addrOpenEphm[0] = -1;       /* ���ú�Select�ṹ����ص�OP_OpenEphem op������*/
	  pNew->addrOpenEphm[1] = -1;
	  pNew->addrOpenEphm[2] = -1;
	  if( db->mallocFailed ) {          /*������ܷ����ڴ�*/
		clearSelect(db, pNew);          /*���Select�ṹ��������*/      
		if( pNew!=&standin ) sqlite3DbFree(db, pNew);/*���Select�ṹ�����滻Select�Ľṹ�壬������ܱ�������������ݿ����ӵ��ڴ棬*/
													 /*�����Ϊ������ݿ����ӵ��²����������ӣ������ͷŵ�*/
		pNew = 0;                      /*��Select�ṹ������Ϊ��*/
	  }else{                           /*�ܷ����ڴ�*/
		assert( pNew->pSrc!=0 || pParse->nErr>0 );/*�ж��Ƿ���From�Ӿ䣬�����Ƿ��з�������*/
	  }
	  assert( pNew!=&standin );/*�ж�Select�ṹ���Ƿ�ͬ�滻�ṹ����ͬ*/
	  return pNew;/*�����������õ�Select�ṹ��*/
	}

	/*
	** Delete the given Select structure and all of its substructures.
	*/

	/*
	** ɾ���ѷ���Ĳ�ѯ�ṹ�������е��ӽṹ��
	*/

	void sqlite3SelectDelete(sqlite3 *db, Select *p){/*�������ݿ����Ӻ�Select�ṹ��*/
	  if( p ){ /*���Select�ṹ�����*/
		clearSelect(db, p);/*�����Select�ṹ��������*/
		sqlite3DbFree(db, p);/*���ͷ�Select�ṹ��*/
	  }
	}


	/*
	** �����ӹؼ���ǰ��һ��������ʾ��������ʹ�ú������ӷ�ʽ������һ����������ʾʹ�����µĺ����������͡�
	**
	**     JT_INNER
	**     JT_CROSS
	**     JT_OUTER
	**     JT_NATURAL
	**     JT_LEFT
	**     JT_RIGHT
	**
	** ȫ��������JT_LEFT��JT_RIGHT��ϡ�
	**
	** �����⵽�ǷǷ��ַ����߲�֧�ֵ��������ͣ�Ҳ�᷵��һ���������ͣ�
	** ���ǻ���pParse�ṹ�м���һ��������Ϣ��
	*/.

	int sqlite3JoinType(Parse *pParse, Token *pA, Token *pB, Token *pC){/*������������������ƽṹ��*/
	  int jointype = 0;/*ȷ����������*/
	  Token *apAll[3];/*�������*/
	  Token *p;/*����һ����ʱ����*/
								 /*   0123456789 123456789 123456789 123 */
	  static const char zKeyText[] = "naturaleftouterightfullinnercross";/*����ַ����飬����װ�����������ͣ��������н��е���*/
	  static const struct {/*����һ���ڲ��ṹ��*/
		u8 i;        /* Beginning of keyword text in zKeyText[] *//*zKeyText�������ʼ�ؼ���*/
		u8 nChar;    /* Length of the keyword in characters *//*�ؼ��ֵ��ַ�����*/
		u8 code;     /* Join type mask *//*�������ͱ��*/
	  } aKeyword[] = {/*����һ���ؼ������飬�����±�ͳ����ҵ���������*/
		/* natural */ { 0,  7, JT_NATURAL                },/*�±��0��ʼ������Ϊ7����Ȼ����*/
		/* left    */ { 6,  4, JT_LEFT|JT_OUTER          },/*�±��6��ʼ������Ϊ4�������ӻ�������*/
		/* outer   */ { 10, 5, JT_OUTER                  },/*�±��10��ʼ������Ϊ5��������*/
		/* right   */ { 14, 5, JT_RIGHT|JT_OUTER         },/*�±��14��ʼ������Ϊ5�������ӻ�������*/
		/* full    */ { 19, 4, JT_LEFT|JT_RIGHT|JT_OUTER },/*�±��19��ʼ������Ϊ4�������ӻ������ӻ������ӣ�ʵ���Ǹ�ȫ����*/
		/* inner   */ { 23, 5, JT_INNER                  },/*�±��23��ʼ������Ϊ5��������*/
		/* cross   */ { 28, 5, JT_INNER|JT_CROSS         },/*�±��28��ʼ������Ϊ5�������ӻ�CROSS���ӣ�ʵ���Ǹ�CROSS join*/
	  };
	  int i, j;
	  apAll[0] = pA;/*��ź��������е�����pA*/
	  apAll[1] = pB;
	  apAll[2] = pC;
	  for(i=0; i<3 && apAll[i]; i++){/*ѭ�����������������*/
		p = apAll[i];
		for(j=0; j<ArraySize(aKeyword); j++){/*ѭ�����ӹؼ�������*/
		  if( p->n==aKeyword[j].nChar /*����������ַ������������������еĹؼ��ֳ���*/
			  && sqlite3StrNICmp((char*)p->z, &zKeyText[aKeyword[j].i], p->n)==0 ){/*����ʹ�ñȽ��ַ��������Ƚ�*/
			jointype |= aKeyword[j].code;/*���ͨ���˱Ƚϳ��Ⱥ����ݣ������������ͣ�ע���ǣ�ʹ�õ��ǡ�λ��*/
			break;
		  }
		}
		testcase( j==0 || j==1 || j==2 || j==3 || j==4 || j==5 || j==6 );/*���ò��Դ�����testcast������jֵ���Ƿ��������Χ*/
		if( j>=ArraySize(aKeyword) ){/*���j�����ӹؼ������黹��*/
		  jointype |= JT_ERROR;/*�Ǿ�jointype��JT_ERROR��λ�򡱣�����һ������*/
		  break;
		}
	  }
	  if(
		 (jointype & (JT_INNER|JT_OUTER))==(JT_INNER|JT_OUTER) ||/*����������ͽ���(JT_INNER|JT_OUTER)�Ľ������JT_INNER��JT_OUTERһ��*/
		 (jointype & JT_ERROR)!=0/*�������ӹؼ����Ǵ�������*/
	  ){
		const char *zSp = " ";
		assert( pB!=0 );/*�ж�����pB��Ϊ��*/
		if( pC==0 ){ zSp++; }/*�������pC�ĳ����ǿ���zSp++*/
		sqlite3ErrorMsg(pParse, "unknown or unsupported join type: "
		   "%T %T%s%T", pA, pB, zSp, pC);/*��Parse�������У����һ��������Ϣ*/
		jointype = JT_INNER;/*Ĭ��ʹ��������*/
	  }else if( (jointype & JT_OUTER)!=0 /*����������ͺ��������н���*/
			 && (jointype & (JT_LEFT|JT_RIGHT))!=JT_LEFT ){/*���ң��������ͺ�(JT_LEFT|JT_RIGHT)����������������*/
		sqlite3ErrorMsg(pParse, 
		  "RIGHT and FULL OUTER JOINs are not currently supported");/*��ô����һ�������Ӻ�ȫ�����Ӳ���֧��*/
		jointype = JT_INNER;/*Ĭ��ʹ��������*/
	  }
	  return jointype;/*����������������*/
	}

	/*
	** Return the index of a column in a table.  Return -1 if the column
	** is not contained in the table.
	*/

	/*
	** ���ر��е�һ�е��±꣬������в��ڱ��У�����-1.
	*/

	static int columnIndex(Table *pTab, const char *zCol){//����Ϊ����������
	  int i;
	  for(i=0; i<pTab->nCol; i++){//����ȫ���������
		if( sqlite3StrICmp(pTab->aCol[i].zName, zCol)==0 ) return i;//���ƥ���ϣ��ͷ������±�
	  }
	  return -1;//�����꣬û��ƥ���ϣ�����-1
	}

	/*
	** Search the first N tables in pSrc, from left to right, looking for a
	** table that has a column named zCol.  
	**
	** When found, set *piTab and *piCol to the table index and column index
	** of the matching column and return TRUE.
	**
	** If not found, return FALSE.
	*/

	/*
	** ��FROM�Ӿ���ɨ���������,����ǰN����,��һ����������ΪzCol�ı� ��
	**
	** �ҵ�֮��,����*piTab��������������*piCol����Ҫƥ������������ٷ���TRUE������Ҳ����ͷ���FALSE.
	*/

	static int tableAndColumnIndex(
	  SrcList *pSrc,       /* Array of tables to search *//*��Ŵ����ҵı������*/
	  int N,               /* Number of tables in pSrc->a[] to search *//*�����Ŀ*/
	  const char *zCol,    /* Name of the column we are looking for *//*�����ҵ�����*/
	  int *piTab,          /* Write index of pSrc->a[] here *//*Ϊ������д����*/
	  int *piCol           /* Write index of pSrc->a[*piTab].pTab->aCol[] here *//*Ϊ��������ĳ�������д����*/
	){
	  int i;               /* For looping over tables in pSrc *//*ѭ���������еı��ڼ�����*/
	  int iCol;            /* Index of column matching zCol *//*ƥ���ϵ��е��������ڼ���*/

	  assert( (piTab==0)==(piCol==0) );  /* Both or neither are NULL *//*�жϱ��������������Ƿ�Ϊ�գ��򶼲�Ϊ�� */
	  for(i=0; i<N; i++){
		iCol = columnIndex(pSrc->a[i].pTab, zCol);/* �����������е���*/
		if( iCol>=0 ){/*�������������*/
		  if( piTab ){/*���������Ҳ����*/
			*piTab = i;/*��ô���ñ������*/
			*piCol = iCol;/*����������*/
		  }
		  return 1;/*���򷵻�1*/
		}
	  }
	  return 0;/*�ձ���0*/
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
	*/

	/*
	** ��������������where�Ӿ���ͺ���JOIN�﷨��,�Ӷ�����select��䡣
	** �����������ӵ�����where�Ӿ��еģ���ʽ����:
	**    (tab1.col1 = tab2.col2)
	** tab1�Ǳ���pSrc�еĵ�i����tab2�ǵ�i+1����col1��tab1��������col2 ��tab2d��������
	*/

	static void addWhereTerm(
	  Parse *pParse,                  /* Parsing context *//*����������*/
	  SrcList *pSrc,                  /* List of tables in FROM clause *//*����*/
	  int iLeft,                      /* Index of first table to join in pSrc *//*��һ�����ӵı�����*/
	  int iColLeft,                   /* Index of column in first table *//*��һ�����������*/
	  int iRight,                     /* Index of second table in pSrc *//*�����еڶ����������*/
	  int iColRight,                  /* Index of column in second table *//*�ڶ������������*/
	  int isOuterJoin,                /* True if this is an OUTER join *//*������Ϊtrue*/
	  Expr **ppWhere                  /* IN/OUT: The WHERE clause to add to *//*where�Ӿ���ʽ*/
	){
	  sqlite3 *db = pParse->db;       /*����һ�����ݿ�����*/
	  Expr *pE1;                      /*����һ�����ʽ*/
	  Expr *pE2;
	  Expr *pEq;

	  assert( iLeft<iRight );/*�ж������һ���������ֵ�Ƿ�С�ڵڶ����������ֵ*/
	  assert( pSrc->nSrc>iRight );/*�жϱ����еı����Ŀ�Ƿ�����ұ������ֵ*/
	  assert( pSrc->a[iLeft].pTab );/*�жϱ����б�����������ı��Ƿ�Ϊ��*/
	  assert( pSrc->a[iRight].pTab );/*�жϱ����б����ұ������ı��Ƿ�Ϊ��*/

	  pE1 = sqlite3CreateColumnExpr(db, pSrc, iLeft, iColLeft);/*���䲢����һ�����ʽָ��ȥ���ر���������һ��������*/ 
	  pE2 = sqlite3CreateColumnExpr(db, pSrc, iRight, iColRight);/*���䲢����һ�����ʽָ��ȥ���ر������ұ��һ��������*/ 

	  pEq = sqlite3PExpr(pParse, TK_EQ, pE1, pE2, 0);/*����һ������ڵ������������������ʽ*/
	  if( pEq && isOuterJoin ){/*���pEq���ʽ��ȫ���ӱ��ʽ*/
		ExprSetProperty(pEq, EP_FromJoin);/*��ô��������ʹ��ON��USING�Ӿ�*/
		assert( !ExprHasAnyProperty(pEq, EP_TokenOnly|EP_Reduced) );/*�ж�pEq���ʽ�Ƿ���EP_TokenOnly��EP_Reduced*/
		ExprSetIrreducible(pEq);/*����pEq,�����Ƿ����Լ��*/
		pEq->iRightJoinTable = (i16)pE2->iTable;/*ָ��Ҫ���ӵ��ұ��ǵڶ������ʽ�ı�*/
	  }
	  *ppWhere = sqlite3ExprAnd(db, *ppWhere, pEq);/*��ָ�����ݿ�ı��ʽ�ǽ�������*/
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
	** for  like this:
	**
	**    SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.b AND t1.x=5
	**
	** The where clause needs to defer the handling of the t1.x=5
	** term until after the t2 loop of the join.  In that way, a
	** NULL t2 row will be inserted whenever t1.x!=5.  If we do not
	** defer the handling of t1.x=5, it will be processed immediately
	** after the t1 loop and rows with t1.x!=5 will never appear in
	** the output, which is incorrect.
	*/

	/*
	**�趨EP_FromJoin��FROM���ӱ��ʽ�����ԣ��ڸ��������еı��ʽ�������С������趨Expr.iRightJoinTable�����ӵڶ�����
	**�ڱ��ʽ�С�
	**EP_FromJoin������Ϊ���ʽ���������������ӵĴ����߼�������ON����USING�ض����������е�һ���֣�
	**��ͨ������ΪWHERE�Ӿ�һ���֡���Щ�����ڱ����Ӵ�������ֲ��where��ʹ�ã�������Ҫ��ס������Դ��on��using�Ӿ���
	**Expr.iRightJoinTable����where�Ӿ䴦��Ҫ����iRightJoinTable����ʹ���������ȷ�ı�����Ҫ����Ϣ���£�
	** SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.b AND t1.x=5
	**where�Ӿ���Ҫ��ʱ����t1.x=5��ֱ��t2ѭ��������ϡ�ͨ�����ַ�ʽ,t2��һ����ֵ����ʹ t1.x!=5Ҳ�ᱻѡ���������û�ж���
	**����t1.x=5��������t1ѭ������ٴ�������t1.x��=5������Ҫ��Ͳ��������
	*/

	static void setJoinExpr(Expr *p, int iTable){/*����һ�����ʽ��һ�������ӵı�*/
	  while( p ){
		ExprSetProperty(p, EP_FromJoin);/*����join��ʹ��ON��USING�Ӿ�*/
		assert( !ExprHasAnyProperty(p, EP_TokenOnly|EP_Reduced) );/*�жϱ��ʽ�����ԣ����ڱ��ʽ�ĳ��Ⱥ�ʣ�೤��*/
		ExprSetIrreducible(p);/*���Ա��ʽ���ж��Ƿ����*/
		p->iRightJoinTable = (i16)iTable;/*�����ұ��������д���ı�*/
		setJoinExpr(p->pLeft, iTable);/*�ݹ��������*/
		p = p->pRight;/*��ֵ���ʽpΪԭ��p�����ӽڵ�*/
	  } 
	}

	
	/*
	**�����������������select�����join��Ϣ��
	**ON��USING�Ӿ䱻ת��ΪWHERE������Ӿ䡣
	**��Ȼ����Ҳ�ᴴ�������where�Ӿ䡣 
	**
	**from�Ӿ䱻Select.pSrc(Select�ṹ����FROM����)��������
	**��ߵı�ͨ����Select.pSrc����ڡ��ұߵı�ͨ�������һ����ڣ�entry��hashmap����Ϊѭ���Ľڵ���ڣ��˴������ı����ӱ����ļ�¼��ڣ�
	**join����������ڵ���ߡ�Ȼ����ڵ�0���������Ӳ����������0�����1֮�䡣�κ��漰join��ON��USING�Ӿ䣬Ҳ�����Ӳ������ŵ������ڡ�
	**
	**��γ��򷵻���������ĸ���
	*/
	static int sqliteProcessJoin(Parse *pParse, Select *p){/*�����������Select�ṹ��*/
	  SrcList *pSrc;                  /* All tables in the FROM clause *//*������еı�������*/
	  int i, j;                       /* Loop counters *//*ѭ������*/
	  struct SrcList_item *pLeft;     /* Left table being joined *//*����ӵ����*/
	  struct SrcList_item *pRight;    /* Right table being joined *//*����ӵ��ұ�*/

	  pSrc = p->pSrc;/*��ֵpSrcΪSelect�ṹ����FROM�Ӿ�����*/
	  pLeft = &pSrc->a[0];/*���������е�һ������Ϊ���*/
	  pRight = &pLeft[1];/*���������еڶ�������Ϊ�ұ�*/
	  for(i=0; i<pSrc->nSrc-1; i++, pRight++, pLeft++){/*ѭ�������������б�*/
		Table *pLeftTab = pLeft->pTab;/*��ֵpLeftTabΪpLeft�������ɱ�ĺ�������Ϊ���*/
		Table *pRightTab = pRight->pTab;/*��ֵpRightTabΪpRight�������ɱ�ĺ�������Ϊ�ұ�*/
		int isOuter;
		
		if( NEVER(pLeftTab==0 || pRightTab==0) ) continue;/*��������ұ���һ����Ϊ��*/
		isOuter = (pRight->jointype & JT_OUTER)!=0;/*�ұ���������ͽ����������Ͳ�Ϊ�գ��ٸ�ֵ��isOute����ֵ*/

		/* When the NATURAL keyword is present, add WHERE clause terms for
		** every column that the two tables have in common.
		*//*������NATURAL�ؼ��֣�WHERE�Ӿ��У��������еĹ�ͬ��Ҳ��ͬʱ����*/
		if( pRight->jointype & JT_NATURAL ){/*����ұ��������������Ȼ����*/
		  if( pRight->pOn || pRight->pUsing ){/*����ұ���ON��USING�Ӿ�*/
			sqlite3ErrorMsg(pParse, "a NATURAL join may not have "
			   "an ON or USING clause", 0);/*��ô���������Ȼ�����в��ܺ���ON USING�Ӿ�*/
			return 1;
		  }
		  for(j=0; j<pRightTab->nCol; j++){/*ѭ���ұ����*/
			char *zName;   /* Name of column in the right table *//*�ұ��е�����*/
			int iLeft;     /* Matching left table *//*ƥ������*/
			int iLeftCol;  /* Matching column in the left table *//*ƥ��������*/

			zName = pRightTab->aCol[j].zName;/*��������*/
			if( tableAndColumnIndex(pSrc, i+1, zName, &iLeft, &iLeftCol) ){/*������������е�����*/
			  addWhereTerm(pParse, pSrc, iLeft, iLeftCol, i+1, j,/*���WHERE�Ӿ䣬�������ұ��к����ӷ�ʽ*/
						   isOuter, &p->pWhere);
			}
		  }
		}

		/* Disallow both ON and USING clauses in the same join
		*/
		/*������ʹ��ON USING��ͬһ��������*/
		if( pRight->pOn && pRight->pUsing ){ /*����ұ��м���ON����USING*/
		  sqlite3ErrorMsg(pParse, "cannot have both ON and USING "
			"clauses in the same join");/*�﷨���н��ᱨ��*/
		  return 1;
		}

		/* Add the ON clause to the end of the WHERE clause, connected by
		** an AND operator.
		*/
		/*���ON�Ӿ���WHERE�Ӿ��ĩβ��ʹ��and����������*/
		if( pRight->pOn ){/*����ұ�����ON�ؼ���*/
		  if( isOuter ) setJoinExpr(pRight->pOn, pRight->iCursor);/*����������ӣ��������ӱ��ʽ��ON�Ӿ���α�*/
		  p->pWhere = sqlite3ExprAnd(pParse->db, p->pWhere, pRight->pOn);/*���ý�WHERE�Ӿ���ON�Ӿ�����һ�𣬸�ֵ���ṹ���WHERE*/
		  pRight->pOn = 0;/*���û�������ӣ������ò�ʹ��ON�Ӿ�*/
		}

		/* Create extra terms on the WHERE clause for each column named
		** in the USING clause.  Example: If the two tables to be joined are 
		** A and B and the USING clause names X, Y, and Z, then add this
		** to the WHERE clause:    A.X=B.X AND A.Y=B.Y AND A.Z=B.Z
		** Report an error if any column mentioned in the USING clause is
		** not contained in both tables to be joined.
		*/
		/*
		** ��WHERE�Ӿ���Ϊÿһ�д���һ�������USING�Ӿ����
		** �����A��Bʹ��USING�Ӿ䣬USING�а�������X Y Z Ȼ������ӵ�WHERE�Ӿ��У�
		**  A.X=B.X AND A.Y=B.Y AND A.Z=B.Z
		** �����������û�а���һ�������У���ʹ����USING����ô���ᱨ��
		*/
		if( pRight->pUsing ){/*����ұ��к���USING*/
		  IdList *pList = pRight->pUsing;/*���ұ��е�USING��ֵ����ʾ���б�*/
		  for(j=0; j<pList->nId; j++){/*������ʾ���б�*/
			char *zName;     /* Name of the term in the USING clause *//*USING�Ӿ��ڱ�ʾ���б�������*/
			int iLeft;       /* Table on the left with matching column name *//*����ƥ������*/
			int iLeftCol;    /* Column number of matching column on the left *//*����ƥ������*/
			int iRightCol;   /* Column number of matching column on the right *//*�ұ��ƥ������*/

			zName = pList->a[j].zName;/*��ʾ���б��еı�ʾ��*/
			iRightCol = columnIndex(pRightTab, zName);/*�����ұ�ͱ�ʾ���ұ�Ĵ�ƥ��������������к�*/
			if( iRightCol<0
			 || !tableAndColumnIndex(pSrc, i+1, zName, &iLeft, &iLeftCol)/*����в�����*/
			){
			  sqlite3ErrorMsg(pParse, "cannot join using column %s - column "
				"not present in both tables", zName);/*���﷨�����������һ��������Ϣ*/
			  return 1;
			}
			addWhereTerm(pParse, pSrc, iLeft, iLeftCol, i+1, iRightCol,/*������ڣ���ӵ�WHERE�Ӿ���*/
						 isOuter, &p->pWhere);
		  }
		}
	  }
	  return 0;/*Ĭ�Ϸ���0���������ķ����ɵó�������һ��inner����*/
	}

	/*
	** Insert code into "v" that will push the record on the top of the
	** stack into the sorter.
	*/
	/*
	** ������뵽"v",����Ѽ�¼��������ջ����
	*/
	static void pushOntoSorter(
	  Parse *pParse,         /* Parser context *//*����������*/
	  ExprList *pOrderBy,    /* The ORDER BY clause *//*ORDER BY�Ӿ�*/
	  Select *pSelect,       /* The whole SELECT statement *//*Select�ṹ��*/
	  int regData            /* Register holding data to be sorted *//*������������*/
	){
	  Vdbe *v = pParse->pVdbe;/*����һ�������*/
	  int nExpr = pOrderBy->nExpr;/*����һ��ORDERBY���ʽ*/
	  int regBase = sqlite3GetTempRange(pParse, nExpr+2);/*����һ�������ļĴ�������С�Ǳ��ʽ�ĸ�����2*/
	  int regRecord = sqlite3GetTempReg(pParse);/*����һ���Ĵ������洢�м������*/
	  int op;
	  sqlite3ExprCacheClear(pParse);/*������е��л���*/
	  sqlite3ExprCodeExprList(pParse, pOrderBy, regBase, 0);/*�����ʽ�б���ÿ��Ԫ�ص�ÿ��ֵ���ŵ�һ�������У�����һ��Ԫ�صĹ��Ƹ���*/
	  sqlite3VdbeAddOp2(v, OP_Sequence, pOrderBy->iECursor, regBase+nExpr);/*�����ʽ�ŵ�VDBE�У��ٷ���һ���µ�ָ���ַ*/
	  sqlite3ExprCodeMove(pParse, regData, regBase+nExpr+1, 1);/*���ļĴ����е����ݣ��������ܼ�ʱ���¼Ĵ����е��л�������*/
	  sqlite3VdbeAddOp3(v, OP_MakeRecord, regBase, nExpr + 2, regRecord);/*��nExpr�ŵ���ǰʹ�õ�VDBE�У��ٷ���һ���µ�ָ��ĵ�ַ*/
	  if( pSelect->selFlags & SF_UseSorter ){/*���select�ṹ����selFlags��ֵ��SF_UseSorter����һ�£�selFlags��ֵȫ����SF��ͷ�������ʾʹ���˷ּ����ˡ�*/
		op = OP_SorterInsert;/*��Ϊʹ�÷ּ��������Բ���������Ϊ����ּ���*/
	  }else{
		op = OP_IdxInsert;/*����ʹ��������ʽ����*/
	  }
	  sqlite3VdbeAddOp2(v, op, pOrderBy->iECursor, regRecord);/*��Orderby���ʽ�ŵ���ǰʹ�õ�VDBE�У�Ȼ�󷵻�һ���µ�ָ���ַ*/
	  sqlite3ReleaseTempReg(pParse, regRecord);/*�ͷ�regRecord�Ĵ���*/
	  sqlite3ReleaseTempRange(pParse, regBase, nExpr+2);/*�ͷ�regBase��������Ĵ����������Ǳ��ʽ�ĳ��ȼ�2*/
	  if( pSelect->iLimit ){/*���ʹ��Limit�Ӿ�*/
		int addr1, addr2;
		int iLimit;
		if( pSelect->iOffset ){/*���ʹ����Offsetƫ����*/
		  iLimit = pSelect->iOffset+1;/*��ôLimit��ֵΪƫ������1*/
		}else{
		  iLimit = pSelect->iLimit;/*�������Ĭ�ϵģ��ӵ�һ����ʼ����*/
		}
		addr1 = sqlite3VdbeAddOp1(v, OP_IfZero, iLimit);/*�����ַ�ǽ�������˷��ص������������µ�ָ���ַ*/
		sqlite3VdbeAddOp2(v, OP_AddImm, iLimit, -1);/*��ָ��ŵ���ǰʹ�õ�VDBE��Ȼ�󷵻�һ����ַ*/
		addr2 = sqlite3VdbeAddOp0(v, OP_Goto);/*�����ʹ��Goto���֮�󣬷��صĵ�ַ*/
		sqlite3VdbeJumpHere(v, addr1);/*�ı�addr1�ĵ�ַ���Ա�VDBEָ����һ��ָ��ĵ�ַ*/
		sqlite3VdbeAddOp1(v, OP_Last, pOrderBy->iECursor);/*��ORDERBYָ��ŵ���ǰʹ�õ�������У�����Last�����ĵ�ַ*/
		sqlite3VdbeAddOp1(v, OP_Delete, pOrderBy->iECursor);/*��ORDERBYָ��ŵ���ǰʹ�õ�������У�����Delete�����ĵ�ַ*/
		sqlite3VdbeJumpHere(v, addr2);/*�ı�addr2�ĵ�ַ���Ա�VDBEָ����һ��ָ��ĵ�ַ*/
	  }
	}

	/*
	** Add code to implement the OFFSET
	*/
	/*
	**��д����ʵ��offsetƫ��������
	*/
	static void codeOffset(
	  Vdbe *v,          /* Generate code into this VM *//*���ɴ���������*/
	  Select *p,        /* The SELECT statement being coded *//*����Select�ṹ��*/
	  int iContinue     /* Jump here to skip the current record *//*������������ǰ��¼*/
	){
	  if( p->iOffset && iContinue!=0 ){/*���Select�ṹ���к���IOffset����ֵ����������������ǰ��¼*/
		int addr;
		sqlite3VdbeAddOp2(v, OP_AddImm, p->iOffset, -1);/*��VDBE�������һ��ָ�����һ����ָ��ĵ�ַ*/
		addr = sqlite3VdbeAddOp1(v, OP_IfNeg, p->iOffset);/*ʵ���ϵ���sqlite3VdbeAddOp3�����޸�ָ��ĵ�ַ*/
		sqlite3VdbeAddOp2(v, OP_Goto, 0, iContinue);/*���������ĵ�ַ*/
		VdbeComment((v, "skip OFFSET records"));/*����ƫ����*/
		sqlite3VdbeJumpHere(v, addr);/*����ƫ�Ƶ�ַ���ı���һ����ִ�е�ַ*/
	  }
	}

	/*
	** Add code that will check to make sure the N registers starting at iMem
	** form a distinct entry.  iTab is a sorting index that holds previously
	** seen combinations of the N values.  A new entry is made in iTab
	** if the current N values are new.
	**
	** A jump to addrRepeat is made and the N+1 values are popped from the
	** stack if the top N elements are not distinct.
	*/
	/*
	** ��д������ȷ��һ��iMem����N��ע������һ����������ڡ�
	** iTab��һ������������Ԥ���ܿ���N��ֵ����ϡ������iTab���´���һ��Nֵ����ô�������һ���µ������iTab �С�
	**
	** ���N+1��ֵͻȻ��ջ�е���������N��ֵ�ǲ�Ψһ�ģ���ô������������ظ��ĵ�ַ��addrRepeat��
	*/
	static void codeDistinct(
	  Parse *pParse,     /* Parsing and code generating context *//*�����﷨����������������*/
	  int iTab,          /* A sorting index used to test for distinctness *//*���������������Ψһ��*/
	  int addrRepeat,    /* Jump to here if not distinct *//*���û�С�ȥ���ظ��������˴�*/
	  int N,             /* Number of elements *//*Ԫ�صĸ���*/
	  int iMem           /* First element *//*��Ԫ��*/
	){
	  Vdbe *v;
	  int r1;

	  v = pParse->pVdbe;/*����һ���������ݿ��ֽ��������*/
	  r1 = sqlite3GetTempReg(pParse);/*����һ���Ĵ����洢�м���*/
	  sqlite3VdbeAddOp4Int(v, OP_Found, iTab, addrRepeat, iMem, N);/*�Ѳ�����ֵ����������Ȼ�����������������������*/
	  sqlite3VdbeAddOp3(v, OP_MakeRecord, iMem, N, r1);/*����sqlite3VdbeAddOp3�����޸�ָ��ĵ�ַ*/
	  sqlite3VdbeAddOp2(v, OP_IdxInsert, iTab, r1);/*ʵ��Ҳ��ʹ��sqlite3VdbeAddOp3()ֻ�ǲ�����Ϊǰ4�����޸�ָ��ĵ�ַ*/
	  sqlite3ReleaseTempReg(pParse, r1);/*����Ĵ���,sqlite3GetTempReg()�����*/
	}

	#ifndef SQLITE_OMIT_SUBQUERY
	/*
	** Generate an error message when a SELECT is used within a subexpression
	** (example:  "a IN (SELECT * FROM table)") but it has more than 1 result
	** column.  We do this in a subroutine because the error used to occur
	** in multiple places.  (The error only occurs in one place now, but we
	** retain the subroutine to minimize code disruption.)
	*/
	/*
	** ���select��ʹ��һ���������Ӿ䣨��a IN (SELECT * FROM table)���������������
	** ��Ϊ���в�ֹһ������С����������ӳ�������ԭ������Ϊ�����ڶദ�������С�
	** �����������ֻ��һ������������������Ȼ��������ӳ�����С�������жϡ�
	** �Լ�ע:���м�ʹ����һ������Ҳ���ش���
	*/
	static int checkForMultiColumnSelectError(
	  Parse *pParse,       /* Parse context. *//*����������*/
	  SelectDest *pDest,   /* Destination of SELECT results *//*Select�Ľ���� */
	  int nExpr            /* Number of result columns returned by SELECT *//*select���صĽ������*/
	){
	  int eDest = pDest->eDest;/*��������*/
	  if( nExpr>1 && (eDest==SRT_Mem || eDest==SRT_Set) ){/*������������1����select�Ľ������SRT_Mem��SRT_Set*/
		sqlite3ErrorMsg(pParse, "only a single result allowed for "
		   "a SELECT that is part of an expression");/*���﷨��������дһ��������Ϣ*/
		return 1;/*�˴�����1����Ϊ��select�������ֻ�������ܴ���1*/
	  }else{
		return 0;/*���û������������ֻ����0*/
	  }
	}
	#endif

	/*
	** This routine generates the code for the inside of the inner loop
	** of a SELECT.
	**
	** If srcTab and nColumn are both zero, then the pEList expressions
	** are evaluated in order to get the data for this row.  If nColumn>0
	** then data is pulled from srcTab and pEList is used only to get the
	** datatypes for each column.
	*/

	/*
	** ��γ�������Ĵ�����Ϊ��select������ѭ���� 
	**
	** ���ȡ���ݵı���ж���0��Ȼ����ȡ���ݵ��б��ʽ�б���Ϊ��Ҫ�õ���һ�е����ݡ�
	** ���srcTab��Դ��������>0������ֻ��srcTab��Դ����pElist������ȡֵ�б��õ�ÿһ�е��������͡�
	*/
	static void selectInnerLoop(
	  Parse *pParse,          /* The parser context *//*����������*/
	  Select *p,              /* The complete select statement being coded *//*����select��ѯ�ṹ��*/
	  ExprList *pEList,       /* List of values being extracted *//*����ȡ��ֵ�б�*/
	  int srcTab,             /* Pull data from this table *//*��ȡ���ݵı�*/
	  int nColumn,            /* Number of columns in the source table *//*Դ�������*/
	  ExprList *pOrderBy,     /* If not NULL, sort results using this key *//*������ǿգ��Խ������ʹ������ؼ���*/
	  int distinct,           /* If >=0, make sure results are distinct *//*���distinct > 0,�Խ�����С�ȥ���ظ�������*/
	  SelectDest *pDest,      /* How to dispose of the results *//*��������*/
	  int iContinue,          /* Jump here to continue with next row *//**//*�����˴�����һ��*/
	){
	  Vdbe *v = pParse->pVdbe;/*����һ�������*/
	  int i;
	  int hasDistinct;        /* True if the DISTINCT keyword is present *//*���hasDistinct��True��ʹ��DISTINCT��ȥ���ظ����ؼ���*/
	  int regResult;              /* Start of memory holding result set *//*���������ʼ��*/
	  int eDest = pDest->eDest;   /* How to dispose of results *//*ȷ����δ�������*/
	  int iParm = pDest->iSDParm; /* First argument to disposal method *//*�������ĵ�һ������*/
	  int nResultCol;             /* Number of result columns *//*����е�����*/

	  assert( v );/*�ж������*/
	  if( NEVER(v==0) ) return;/*�������������ڣ�ֱ�ӷ���*/
	  assert( pEList!=0 );/*�жϱ��ʽ�б��Ƿ�Ϊ��*/
	  hasDistinct = distinct>=0;/*��ֵ��ȥ���ظ���������*/
	  if( pOrderBy==0 && !hasDistinct ){/*���ʹ����ORDERBY��hasDistinctȡ��ֵ*/
		codeOffset(v, p, iContinue);/*����ƫ������VDBE��selectȷ����ƫ�Ʋ�����IContinue*/
	  }

	  /* Pull the requested columns.
	  */
	  /*����Ҫ������ȡ����*/
	  if( nColumn>0 ){/*�����������0*/
		nResultCol = nColumn;/*��ֵ����е�ֵΪԴ�������*/
	  }else{
		nResultCol = pEList->nExpr;/*���򣬸�ֵΪ����ȡֵ������*/
	  }
	  if( pDest->iSdst==0 ){/*�����ѯ���ݼ���д�����Ļ�ַ�Ĵ�����ֵΪ0*/
		pDest->iSdst = pParse->nMem+1;/*��ô��ַ�Ĵ�����ֵ��Ϊ�����﷨������һ����ַ*/
		pDest->nSdst = nResultCol;/*ע��Ĵ���������Ϊ����е�����*/
		pParse->nMem += nResultCol;/*�������ĵ�ַ��Ϊ������ټ��Ͻ���е�����*/
	  }else{ 
		assert( pDest->nSdst==nResultCol );/*�жϽ�����мĴ����ĸ����Ƿ������е�������ͬ���ϵ㡣*/
	  }
	  regResult = pDest->iSdst;/*�ٰѴ��������ļĴ����ĵ�ַ��Ϊ���������ʼ��ַ*/
	  if( nColumn>0 ){/*��������0*/
		for(i=0; i<nColumn; i++){/*������*/
		  sqlite3VdbeAddOp3(v, OP_Column, srcTab, i, regResult+i);/*�����в������뵽VDBE�ٷ����µ�ָ���ַ*/
		}
	  }else if( eDest!=SRT_Exists ){/*�������Ľ����������*/
		/* If the destination is an EXISTS(...) expression, the actual
		** values returned by the SELECT are not required.
		*/
		/*
		** ���������ڣ�select���÷���ֵ��
		*/
		sqlite3ExprCacheClear(pParse);/*����﷨�������Ļ���*/
		sqlite3ExprCodeExprList(pParse, pEList, regResult, eDest==SRT_Output);/*�ѱ��ʽ�б��е�ֵ�ŵ�һϵ�еļĴ�����*/
	  }
	  nColumn = nResultCol;/*��ֵ�������ڽ���е�����*/

	  /* If the DISTINCT keyword was present on the SELECT statement
	  ** and this row has been seen before, then do not make this row
	  ** part of the result.
	  */
	  /*���ʹ���ˡ�ȥ���ظ����ؼ�����SELECT����У���ô�����֮ǰ������ʹ���г�Ϊ����е�һ����*/
	  if( hasDistinct ){/*���ʹ����DISTINCT�ؼ���*/
		assert( pEList!=0 );/*���ϵ㣬�жϱ���ȡ��ֵ�б��Ƿ�Ϊ��*/
		assert( pEList->nExpr==nColumn );/*����ȡ��ֵ�б�������Ƿ��������*/
		codeDistinct(pParse, distinct, iContinue, nColumn, regResult);/*���С�ȥ���ظ�������*/
		if( pOrderBy==0 ){/*���û��ʹ��ORDERBY */
		  codeOffset(v, p, iContinue);/*ʹ��codeOffset�����������������в������*/
		}
	  }

	  switch( eDest ){/*ȷ����δ�������*/
		/* In this mode, write each query result to the key of the temporary
		** table iParm.
		*/
		/*�ڸ�ģ�飬д��ÿ����ʱ��Ĳ�ѯ���
		**
		*/
	#ifndef SQLITE_OMIT_COMPOUND_SELECT
		case SRT_Union: {
		  int r1;
		  r1 = sqlite3GetTempReg(pParse);/*Ϊ�﷨����������һ���Ĵ���*/
		  sqlite3VdbeAddOp3(v, OP_MakeRecord, regResult, nColumn, r1);/*��OP_MakeRecord������¼����������VDBE���ٷ���һ����ָ���ַ*/
		  sqlite3VdbeAddOp2(v, OP_IdxInsert, iParm, r1);/*��OP_IdxInsert���������룩��������VDBE���ٷ���һ����ָ���ַ*/
		  sqlite3ReleaseTempReg(pParse, r1);/*�ͷ�����Ĵ���*/
		  break;
		}

		/* Construct a record from the query result, but instead of
		** saving that record, use it as a key to delete elements from
		** the temporary table iParm.
		*/
		/*����һ�ֲ�ѯ������ļ�¼��ʽ�������Ǳ����¼��������Ϊɾ����ʱ��IParm�Ĺؼ���*/
		case SRT_Except: {
		  sqlite3VdbeAddOp3(v, OP_IdxDelete, iParm, regResult, nColumn);/*��OP_IdxDelete������ɾ������������VDBE���ٷ���һ����ָ���ַ*/
		  break;
		}
	#endif

		/* Store the result as data using a unique key.
		*/
		/*�洢ʹ����Ψһ�ؼ��ֵĽ��*/
		case SRT_Table:
		case SRT_EphemTab: {
		  int r1 = sqlite3GetTempReg(pParse);/*����һ���Ĵ������洢�м������*/*/
		  testcase( eDest==SRT_Table );/*���Դ���Ľ�����ı�����*/
		  testcase( eDest==SRT_EphemTab );/*���Դ���Ľ�����ı�Ĵ�С*/
		  sqlite3VdbeAddOp3(v, OP_MakeRecord, regResult, nColumn, r1);/*��OP_MakeRecord������¼����������VDBE���ٷ���һ����ָ���ַ*/
		  if( pOrderBy ){
			pushOntoSorter(pParse, pOrderBy, p, r1);/*��OrderBy�͵��ּ�����ջ��*/
		  }else{
			int r2 = sqlite3GetTempReg(pParse);/*����һ���Ĵ������洢�м������*/
			sqlite3VdbeAddOp2(v, OP_NewRowid, iParm, r2);/*��OP_NewRowid���½���¼����������VDBE���ٷ���һ����ָ���ַ*/
			sqlite3VdbeAddOp3(v, OP_Insert, iParm, r1, r2);/*��OP_Insert�������¼����������VDBE���ٷ���һ����ָ���ַ*/
			sqlite3VdbeChangeP5(v, OPFLAG_APPEND);/*��OPFLAG_APPEND������·������������VDBE���ٷ���һ����ָ���ַ*/
			sqlite3ReleaseTempReg(pParse, r2);/*�ͷ�����Ĵ���*/
		  }
		  sqlite3ReleaseTempReg(pParse, r1);/*�ͷ�����Ĵ���*/
		  break;
		}

	#ifndef SQLITE_OMIT_SUBQUERY
		/* If we are creating a set for an "expr IN (SELECT ...)" construct,
		** then there should be a single item on the stack.  Write this
		** item into the set table with bogus data.
		*/
		/* ������Ǵ���һ�������ı��ʽ"expr IN (SELECT ...)"
		** Ȼ�����Ӧ��ֻ��һ�����󣬰�������ʽд��û�����ݵı��С�
		*/
		case SRT_Set: {
		  assert( nColumn==1 );/*��ϵ㣬��������1*/
		  p->affinity = sqlite3CompareAffinity(pEList->a[0].pExpr, pDest->affSdst);/*���ݱ�ͽ�������洢�ṹ����׺��Խ����*/
		  if( pOrderBy ){
			/* At first glance you would think we could optimize out the
			** ORDER BY in this case since the order of entries in the set
			** does not matter.  But there might be a LIMIT clause, in which
			** case the order does matter */
			/**/
			/*
			**�տ�ʼ����Ϊ���������ϵ�˳�����ORDERBY�Ż���Ϊ����Ҫ.���ǣ�ʹ����LIMIT�ؼ���ʱ���������Ҫ�ˡ�
			*/
			pushOntoSorter(pParse, pOrderBy, p, regResult);/*����ORDERBY������Ȼ��ŵ��ּ�����ջ��*/
		  }else{
			int r1 = sqlite3GetTempReg(pParse);/*����һ���Ĵ������洢�м������*/
			sqlite3VdbeAddOp4(v, OP_MakeRecord, regResult, 1, r1, &p->affinity, 1);/*��OP_MakeRecord��������VDBE���ٷ���һ����ָ���ַ*/
			sqlite3ExprCacheAffinityChange(pParse, regResult, 1);/*��¼�׺����͵����ݵĸı�ļ����Ĵ�������ʼ��ַ*/
			sqlite3VdbeAddOp2(v, OP_IdxInsert, iParm, r1);/*��OP_IdxInsert��������VDBE���ٷ���һ����ָ���ַ*/
			sqlite3ReleaseTempReg(pParse, r1);/*�ͷ�����Ĵ���*/
		  }
		  break;
		}

		/* If any row exist in the result set, record that fact and abort.
		*/
		/*������ڽ�����е�һ�У���¼������ݺ��ж�*/
		case SRT_Exists: {
		  sqlite3VdbeAddOp2(v, OP_Integer, 1, iParm);/*��OP_Integer��������VDBE���ٷ���һ����ָ���ַ*/
		  /* The LIMIT clause will terminate the loop for us *//*Limit�Ӿ佫����ֹѭ��*/
		  break;
		}

		/* If this is a scalar select that is part of an expression, then
		** store the results in the appropriate memory cell and break out
		** of the scan loop.
		*//*���������ѯ��������ʽ��һ���֣�Ȼ��洢���������ʵ����ڴ浥Ԫ�У���ѭ��ɨ�����ͷų�*/
		case SRT_Mem: {
		  assert( nColumn==1 );/*���ϵ㣬�жϱ���ȡ��ֵ�б��Ƿ�Ϊ��*/
		  if( pOrderBy ){
			pushOntoSorter(pParse, pOrderBy, p, regResult);/*��ORDERBY������¼�ŵ��ּ�����ջ��*/
		  }else{
			sqlite3ExprCodeMove(pParse, regResult, iParm, 1);/*�ͷżĴ����е����ݣ����ּĴ��������ݼ�ʱ����*/
			/* The LIMIT clause will jump out of the loop for us */
			/* Limit�Ӿ佫���ѭ��������*/
		  }
		  break;
		}
	#endif /* #ifndef SQLITE_OMIT_SUBQUERY */

		/* Send the data to the callback function or to a subroutine.  In the
		** case of a subroutine, the subroutine itself is responsible for
		** popping the data from the stack.
		*/
		/*���ص��������ӳ��������ݣ����ӳ����У��ӳ���Ӷ��е�������*/
		case SRT_Coroutine:
		case SRT_Output: {
		  testcase( eDest==SRT_Coroutine );/*���Դ��������Ƿ���Эͬ����*/
		  testcase( eDest==SRT_Output );   /*���Դ��������Ƿ�Ҫ���*/
		  if( pOrderBy ){/*���������OEDERBY*/
			int r1 = sqlite3GetTempReg(pParse);/*����һ���Ĵ������洢�м������*/
			sqlite3VdbeAddOp3(v, OP_MakeRecord, regResult, nColumn, r1);/*��OP_MakeRecord��������VDBE���ٷ���һ����ָ���ַ*/
			pushOntoSorter(pParse, pOrderBy, p, r1);/*��ORDERBY������¼�ŵ��ּ�����ջ��*/
			sqlite3ReleaseTempReg(pParse, r1);/*�ͷ�����Ĵ���*/
		  }else if( eDest==SRT_Coroutine ){/*�������������Эͬ����*/
			sqlite3VdbeAddOp1(v, OP_Yield, pDest->iSDParm);/*��OP_Yield��������VDBE���ٷ���һ����ָ���ַ*/
		  }else{
			sqlite3VdbeAddOp2(v, OP_ResultRow, regResult, nColumn);/*��OP_ResultRow��������VDBE���ٷ���һ����ָ���ַ*/
			sqlite3ExprCacheAffinityChange(pParse, regResult, nColumn);/*��¼�׺����͵����ݵĸı�ļ����Ĵ�������ʼ��ַ*/
		  }
		  break;
		}

	#if !defined(SQLITE_OMIT_TRIGGER)
		/* Discard the results.  This is used for SELECT statements inside
		** the body of a TRIGGER.  The purpose of such selects is to call
		** user-defined functions that have side effects.  We do not care
		** about the actual results of the select.
		*//*��һ�����������ͷŽ����ʹ�����SELECT�����Զ��庯�������ǲ����������Ĵ�����*/
		default: {
		  assert( eDest==SRT_Discard );/*�������������SRT_Discard��������*/
		  break;
		}
	#endif
	  }

	  /* Jump to the end of the loop if the LIMIT is reached.  Except, if
	  ** there is a sorter, in which case the sorter has already limited
	  ** the output for us.
	  *//*�����Limit�����ƴ���ֱ��������β���������Ƿּ������ּ�������������ġ�*/
	  if( pOrderBy==0 && p->iLimit ){/*���������ORDERBY��limit�Ӿ�*/
		sqlite3VdbeAddOp3(v, OP_IfZero, p->iLimit, iBreak, -1);/*��OP_IfZero��������VDBE���ٷ���һ����ָ���ַ*/
	  }
	}

	/*
	** Given an expression list, generate a KeyInfo structure that records
	** the collating sequence for each expression in that expression list.
	**
	** If the ExprList is an ORDER BY or GROUP BY clause then the resulting
	** KeyInfo structure is appropriate for initializing a virtual index to
	** implement that clause.  If the ExprList is the result set of a SELECT
	** then the KeyInfo structure is appropriate for initializing a virtual
	** index to implement a DISTINCT test.
	**
	** Space to hold the KeyInfo structure is obtain from malloc.  The calling
	** function is responsible for seeing that this structure is eventually
	** freed.  Add the KeyInfo structure to the P4 field of an opcode using
	** P4_KEYINFO_HANDOFF is the usual way of dealing with this.
	*/
	/*
	** �������ʽ�б�����һ���ؼ���Ϣ�ṹ����¼���ʽ�б���ÿһ�����ʽ������˳��
	**
	** ���������ʽ��ORDERBY��GROUPBY�Ӿ䣬Ȼ�����ؼ���Ϣ�ṹ�ʺ��ڳ�ʼ����������ʵ������Ӿ䡣
	** ���������ʽ�б�ʱSELECT�Ľ������Ȼ��ؼ���Ϣ�ṹ���ʺ��ڳ�ʼ����������ʵ��DISTINCT��
	**
	** ����ؼ���Ϣ�ṹ�ѷ�����ڴ档�ص�������������ͷ�����ṹ����ӹؼ���Ϣ�ṹ��P4����������飬ͨ��ʹ��P4_KEYINFO_HANDOFF����
	**
	*/
	static KeyInfo *keyInfoFromExprList(Parse *pParse, ExprList *pList){/*��������������һ��Ϊ�﷨��������һ��Ϊ���ʽ�б�*/
	  sqlite3 *db = pParse->db;/*�������ݿ�����*/
	  int nExpr;
	  KeyInfo *pInfo;/*�����ؼ��ֽṹ��*/
	  struct ExprList_item *pItem;/*�������ʽ��ṹ��*/
	  int i;

	  nExpr = pList->nExpr;/*�������ʽ�б��б��ʽ�ĸ���*/
	  pInfo = sqlite3DbMallocZero(db, sizeof(*pInfo) + nExpr*(sizeof(CollSeq*)+1) );/*���䲢����ڴ棬�����СΪ�ڶ����������ڴ�*/
	  if( pInfo ){/*������ڹؼ��ֽṹ��*/
		pInfo->aSortOrder = (u8*)&pInfo->aColl[nExpr];/*���ùؼ���Ϣ�ṹ�������Ϊ�ؼ���Ϣ�ṹ���б��ʽ�к��еĹؼ��֣�����aColl[1]��ʾΪÿһ���ؼ��ֽ�������*/
		pInfo->nField = (u16)nExpr;/*�ؼ���Ϣ�ṹ����aColl�����ĳ���Ϊ���ʽ�ĸ���*/
		pInfo->enc = ENC(db);/*�ؼ���Ϣ�ṹ���б��뷽ʽΪdb�ı��뷽ʽ*/
		pInfo->db = db;/*�ؼ���Ϣ�ṹ�������ݿ�Ϊ��ǰʹ�õ����ݿ�*/
		for(i=0, pItem=pList->a; i<nExpr; i++, pItem++){/*������ǰ�ı��ʽ�б�*/
		  CollSeq *pColl;/*����һ������˳��ĳ���ʹ�������Զ������������*/
		  pColl = sqlite3ExprCollSeq(pParse, pItem->pExpr);/*���﷨�������ֱ��ʽ���еı��ʽ��ֵ���������*/
		  if( !pColl ){/*���û��ָ������ķ���*/
			pColl = db->pDfltColl;/*�����ݿ���Ĭ�ϵ����򷽷���ֵ��pColl*/
		  }
		  pInfo->aColl[i] = pColl;/*�ؼ���Ϣ�ṹ���жԹؼ�������������Ԫ�ض�Ӧ���ʽ�����������*/
		  pInfo->aSortOrder[i] = pItem->sortOrder;/*�ؼ���Ϣ�ṹ���������˳��Ϊ�﷨���������﷨����ʽ�����򷽷�*/
		  /*��ע������ǣ���û�п�����������ķ������������Ϊ��ָ��ʹ��ĳ������ķ�ʽ�����������û��ʹ��ϵͳĬ�ϵġ��ٰ��﷨���б��ʽ������������Ӧ����һ��������ֻ�Ǳ��ķ�ʽ��һ��*/
		}
	  }
	  return pInfo;/*��������ؼ���Ϣ�ṹ��*/
	}

	#ifndef SQLITE_OMIT_COMPOUND_SELECT
	/*
	** Name of the connection operator, used for error messages.
	*//*���Ӳ����������֣���������������Ϣ*/
	static const char *selectOpName(int id){/*����һ��int����ʵ����id*/
	  char *z;
	  switch( id ){/*�ж�id��ֵ*/
		case TK_ALL:       z = "UNION ALL";   break;/*�����TK_ALL������z = "UNION ALL"*/
		case TK_INTERSECT: z = "INTERSECT";   break;/*�����TK_INTERSECT������z = "INTERSECT"*/
		case TK_EXCEPT:    z = "EXCEPT";      break;/*�����TK_EXCEPT������z = "EXCEPT"*/
		default:           z = "UNION";       break;/*Ĭ������z = "UNION"*/
	  }
	  return z;
	}
	#endif /* SQLITE_OMIT_COMPOUND_SELECT */

	#ifndef SQLITE_OMIT_EXPLAIN
	/*
	** Unless an "EXPLAIN QUERY PLAN" command is being processed, this function
	** is a no-op. Otherwise, it adds a single row of output to the EQP result,
	** where the caption is of the form:
	**
	**   "USE TEMP B-TREE FOR xxx"
	**
	** where xxx is one of "DISTINCT", "ORDER BY" or "GROUP BY". Exactly which
	** is determined by the zUsage argument.
	*/
	/*���ǲ�ѯ�ƻ��ܹ�ִ�У���������Ų���ִ�У���������һ���������н������������ĸ�ʽ��
	**
	**USE TEMP B-TREE FOR xxx
	**
	**��XXX��"ȥ���ظ�"�����������򡱻��ߡ����顱������׼ȷ��ʹ�����ǡ�
	*/
	static void explainTempTable(Parse *pParse, const char *zUsage){
	  if( pParse->explain==2 ){/*����﷨�������е�explain�ǵڶ���*/
		Vdbe *v = pParse->pVdbe;/*����һ�������*/
		char *zMsg = sqlite3MPrintf(pParse->db, "USE TEMP B-TREE FOR %s", zUsage);/*������ĸ�ʽ�����ݴ��ݸ�zMsg������%S �Ǵ���Ĳ�����Usage*/
		sqlite3VdbeAddOp4(v, OP_Explain, pParse->iSelectId, 0, 0, zMsg, P4_DYNAMIC);/*���һ�������룬��P4_DYNAMIC��ֵ����ָ��*/
	  }
	}

	/*
	** Assign expression b to lvalue a. A second, no-op, version of this macro
	** is provided when SQLITE_OMIT_EXPLAIN is defined. This allows the code
	** in sqlite3Select() to assign values to structure member variables that
	** only exist if SQLITE_OMIT_EXPLAIN is not defined without polluting the
	** code with #ifndef directives.
	*/
	/*
	** ��ֵ���ʽb����ֵa����������SQLITE_OMIT_EXPLAINÿ���ж��ܶ�Ŀղ�����ֻ����û�и��Ĵ����û�ж���SQLITE_OMIT_EXPLAIN
	** ������£���Ϊsqlite3Select()�г�Ա������ֵ��
	*/
	# define explainSetInteger(a, b) a = b

	#else
	/* No-op versions of the explainXXX() functions and macros. */
	/* û�н���ĳ�������ĺ����ͺ궨��Ĳ������汾*/
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
	*/
	/*
	**����ִ���ˡ����Ͳ�ѯ�ƻ�����������������û�в������������һ���������н������EQP�����
	**�������ĸ�ʽΪ��
	**   "COMPOSITE SUBQUERIES iSub1 and iSub2 (op)"
	**   "COMPOSITE SUBQUERIES iSub1 and iSub2 USING TEMP B-TREE (op)"
	**
	** ���Ӳ�ѯiSub1���Ӳ�ѯiSub2����������ô��Ϊһֱ�ĺ�������,���Ҳ������ı���Ҳ��һ���ġ�
	** �������������TK_UNION, TK_EXCEPT,TK_INTERSECT �� TK_ALL������һ�֡�
	** ���ʹ����һ����ʱ������ʹ�õ�һ��ʽ������ʹ�õڶ���ʽ��
	**
	*/
	static void explainComposite(
	  Parse *pParse,                  /* Parse context *//*����������*/
	  int op,                         /* One of TK_UNION, TK_EXCEPT etc. *//*һ��TK_UNION��TK_EXCEPT�����Ƶ�����*/
	  int iSub1,                      /* Subquery id 1 *//*�Ӳ�ѯ1*/
	  int iSub2,                      /* Subquery id 2 *//*�Ӳ�ѯ2*/
	  int bUseTmp                     /* True if a temp table was used *//*�����ʹ����һ����ʱ����ôֵΪtrue*/
	){
	  assert( op==TK_UNION || op==TK_EXCEPT || op==TK_INTERSECT || op==TK_ALL );/*����op�Ƿ����TK_UNION��TK_EXCEPT��TK_INTERSECT��TK_ALL*/
	  if( pParse->explain==2 ){/*���ʹ���˽��ͷ�ʽ���﷨�������еڶ��ַ�ʽ*/
		Vdbe *v = pParse->pVdbe;/*����һ�������*/
		char *zMsg = sqlite3MPrintf(/*���ñ����Ϣ*/
			pParse->db, "COMPOUND SUBQUERIES %d AND %d %s(%s)", iSub1, iSub2,
			bUseTmp?"USING TEMP B-TREE ":"", selectOpName(op)
		);/*���Ӳ�ѯ1���Ӳ�ѯ2���﷨���ݸ�ֵ��zMsg*/
		sqlite3VdbeAddOp4(v, OP_Explain, pParse->iSelectId, 0, 0, zMsg, P4_DYNAMIC);/*��OP_Explain���������������Ȼ�󷵻�һ����ַ����ַΪP4_DYNAMICָ���е�ֵ*/
	  }
	}
	#else
	/* No-op versions of the explainXXX() functions and macros. */
	/* û�н���ĳ�������ĺ����ͺ궨��Ĳ������汾*/
	# define explainComposite(v,w,x,y,z)
	#endif

	/*
	** If the inner loop was generated using a non-null pOrderBy argument,
	** then the results were placed in a sorter.  After the loop is terminated
	** we need to run the sorter and output the results.  The following
	** routine generates the code needed to do that.
	*/
	/*
	** �����������������һ���ǿյ�ORDERBY���Ȼ��������Ҫ����һ���ּ�����һ��������򣩡�
	** ��ѭ��������������Ҫ���зּ��������������������ĳ�������������Ҫ�Ĵ��롣
	*/
	static void generateSortTail(
	  Parse *pParse,    /* Parsing context *//*����������*/
	  Select *p,        /* The SELECT statement *//*����select��ѯ�ṹ��*/
	  Vdbe *v,          /* Generate code into this VDBE *//*����һ������ŵ��������*/
	  int nColumn,      /* Number of columns of data *//*���ݵ�����*/
	  SelectDest *pDest /* Write the sorted results here *//*д��������*/
	){
	  int addrBreak = sqlite3VdbeMakeLabel(v);     /* Jump here to exit loop *//*��ѭ���������˴�*/
	  int addrContinue = sqlite3VdbeMakeLabel(v);  /* Jump here for next cycle *//*��һ��ѭ�������˴�*/
	  int addr;
	  int iTab;
	  int pseudoTab = 0;
	  ExprList *pOrderBy = p->pOrderBy;/*��Select�ṹ����ORDERBY��ֵ�����ʽ�б��е�ORDERBY���ʽ����*/

	  int eDest = pDest->eDest;/*����ѯ������д���ʽ���ݸ�eDest*/
	  int iParm = pDest->iSDParm;/*����ѯ������д���ʽ�еĲ������ݸ�iParm*/

	  int regRow;
	  int regRowid;

	  iTab = pOrderBy->iECursor;/*������ExprList��VDBE�α괫��iTab*/
	  regRow = sqlite3GetTempReg(pParse);/*ΪpParse�﷨������һ���Ĵ���,�洢������м���*/
	  if( eDest==SRT_Output || eDest==SRT_Coroutine ){/*�������ʽ��SRT_Output���������SRT_Coroutine��Эͬ����*/
		pseudoTab = pParse->nTab++;/*�������﷨���б�������pseudoTab�������ΪʲôҪ++û����*/
		sqlite3VdbeAddOp3(v, OP_OpenPseudo, pseudoTab, regRow, nColumn);/*��OP_Explain�������������*/
		regRowid = 0;
	  }else{
		regRowid = sqlite3GetTempReg(pParse);/*ΪpParse�﷨������һ���Ĵ���,�洢������м���*/
	  }
	  if( p->selFlags & SF_UseSorter ){/*���Select�ṹ���е�selFlags����ֵΪSF_UseSorter��ʹ�÷ּ������������*/
		int regSortOut = ++pParse->nMem;/*����Ĵ����������Ƿ����﷨�����ڴ���+1*/
		int ptab2 = pParse->nTab++;/*�������﷨���б�ĸ�����ֵ��ptab2*/
		sqlite3VdbeAddOp3(v, OP_OpenPseudo, ptab2, regSortOut, pOrderBy->nExpr+2);/*��OP_OpenPseudo�����������������VDBE�����ر��ʽ�б��б��ʽ������ֵ+2*/
		addr = 1 + sqlite3VdbeAddOp2(v, OP_SorterSort, iTab, addrBreak);/*��OP_SorterSort���ּ����������򣩽���VDBE�����صĵ�ַ+1��ֵ��addr*/
		codeOffset(v, p, addrContinue);/*����ƫ����������addrContinue����һ��ѭ��Ҫ�����ĵ�ַ*/
		sqlite3VdbeAddOp2(v, OP_SorterData, iTab, regSortOut);/*��OP_SorterData�������������*/
		sqlite3VdbeAddOp3(v, OP_Column, ptab2, pOrderBy->nExpr+1, regRow);/*��OP_Column�������������*/
		sqlite3VdbeChangeP5(v, OPFLAG_CLEARCACHE);/*�ı�OPFLAG_CLEARCACHE��������棩�Ĳ���������Ϊ��ַ����sqlite3VdbeAddOp3��sqlite3VdbeAddOp2���������ı��˵�ַ*/
	  }else{
		addr = 1 + sqlite3VdbeAddOp2(v, OP_Sort, iTab, addrBreak);/*��OP_Sort������������������صĵ�ַ+1*/
		codeOffset(v, p, addrContinue);/*����ƫ����������addrContinue����һ��ѭ��Ҫ�����ĵ�ַ*/
		sqlite3VdbeAddOp3(v, OP_Column, iTab, pOrderBy->nExpr+1, regRow);/*��OP_Column��������VDBE���ٰ�OP_Column�ĵ�ַ����*/
	  }
	  switch( eDest ){/*����������Ĵ�����*/
		case SRT_Table:
		case SRT_EphemTab: {/*����SRT_EphemTab*/
		  testcase( eDest==SRT_Table );/*����ʽ���Ƿ�SRT_Table*/
		  testcase( eDest==SRT_EphemTab );/*����ʽ���Ƿ�SRT_EphemTab*/
		  sqlite3VdbeAddOp2(v, OP_NewRowid, iParm, regRowid);/*��OP_NewRowid��������VDBE���ٷ�����������ĵ�ַ*/
		  sqlite3VdbeAddOp3(v, OP_Insert, iParm, regRow, regRowid);/*��OP_Insert��������VDBE���ٷ�����������ĵ�ַ*/
		  sqlite3VdbeChangeP5(v, OPFLAG_APPEND);/*�ı�OPFLAG_APPEND������·��������Ϊ��ַ����sqlite3VdbeAddOp2������sqlite3VdbeAddOp3���������ı��˵�ַ*/
		  break;
		}
	#ifndef SQLITE_OMIT_SUBQUERY
		case SRT_Set: {
		  assert( nColumn==1 );/*����ϵ㣬�ж������Ƿ����1*/
		  sqlite3VdbeAddOp4(v, OP_MakeRecord, regRow, 1, regRowid, &p->affinity, 1);/*���һ��OP_MakeRecord��������������ֵ��Ϊһ��ָ��*/
		  sqlite3ExprCacheAffinityChange(pParse, regRow, 1);/*�����﷨��pParse���Ĵ����е��׺�������*/
		  sqlite3VdbeAddOp2(v, OP_IdxInsert, iParm, regRowid);/*��OP_IdxInsert���������룩��������VDBE���ٷ�����������ĵ�ַ*/
		  break;
		}
		case SRT_Mem: {
		  assert( nColumn==1 );/*����ϵ㣬�ж������Ƿ����1*/
		  sqlite3ExprCodeMove(pParse, regRow, iParm, 1);/*�ͷżĴ����е����ݣ����ּĴ��������ݼ�ʱ����*/
		  /* The LIMIT clause will terminate the loop for us */
		  /*Limit�Ӿ佫����ֹѭ��*/
		  break;
		}
	#endif
		default: {
		  int i;
		  assert( eDest==SRT_Output || eDest==SRT_Coroutine ); /*����ϵ㣬�жϽ�������������Ƿ���SRT_Output���������SRT_Coroutine��Эͬ����*/
		  testcase( eDest==SRT_Output );/*�����Ƿ����SRT_Output*/
		  testcase( eDest==SRT_Coroutine );/*�����Ƿ����SRT_Coroutine*/
		  for(i=0; i<nColumn; i++){/*������*/
			assert( regRow!=pDest->iSdst+i );/*����ϵ㣬�жϼĴ����ı��ֵ�����ڻ�ַ�Ĵ����ı��ֵ+i*/
			sqlite3VdbeAddOp3(v, OP_Column, pseudoTab, i, pDest->iSdst+i);/*��OP_Column��������VDBE���ٷ�����������ĵ�ַ*/
			if( i==0 ){/*���û����*/
			  sqlite3VdbeChangeP5(v, OPFLAG_CLEARCACHE);/*�ı�OPFLAG_CLEARCACHE��������棩�Ĳ���������Ϊ��ַ����sqlite3VdbeAddOp3���������ı��˵�ַ*/
			}
		  }
		  if( eDest==SRT_Output ){/*���������Ĵ���ʽ��SRT_Output*/
			sqlite3VdbeAddOp2(v, OP_ResultRow, pDest->iSdst, nColumn);/*��OP_ResultRow��������VDBE���ٷ�����������ĵ�ַ*/
			sqlite3ExprCacheAffinityChange(pParse, pDest->iSdst, nColumn);/*�����﷨��pParse���Ĵ����е��׺�������*/
		  }else{
			sqlite3VdbeAddOp1(v, OP_Yield, pDest->iSDParm);/*��OP_Yield��������VDBE���ٷ�����������ĵ�ַ*/
		  }
		  break;
		}
	  }
	  sqlite3ReleaseTempReg(pParse, regRow);/*�ͷżĴ���*/
	  sqlite3ReleaseTempReg(pParse, regRowid);/*�ͷżĴ���*/

	  /* The bottom of the loop
	  */
	  /*ѭ���ĵײ�*/
	  sqlite3VdbeResolveLabel(v, addrContinue);/*addrContinue��Ϊ��һ������ָ��ĵ�ַ������addrContinue�����ȵ���sqlite3VdbeMakeLabel����*/
	  if( p->selFlags & SF_UseSorter ){/*selFlags��ֵ��SF_UseSorter*/
		sqlite3VdbeAddOp2(v, OP_SorterNext, iTab, addr);/*��OP_SorterNext��������VDBE���ٷ�����������ĵ�ַ*/
	  }else{
		sqlite3VdbeAddOp2(v, OP_Next, iTab, addr);/*��OP_Next��������VDBE���ٷ�����������ĵ�ַ*/
	  }
	  sqlite3VdbeResolveLabel(v, addrBreak);/*addrBreak��Ϊ��һ������ָ��ĵ�ַ������addrBreak�����ȵ���sqlite3VdbeMakeLabel����*/
	  if( eDest==SRT_Output || eDest==SRT_Coroutine ){/*���������Ĵ���ʽSRT_Output��SRT_Coroutine*/
		sqlite3VdbeAddOp2(v, OP_Close, pseudoTab, 0);/*��OP_Close��������VDBE���ٷ�����������ĵ�ַ*/
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
	*/
	/*
	** ����һ������'declaration type'���������ͣ����ʽ�ַ���������ַ���Ӧ���ܱ���̬���õġ�
	**
	** ���������ʽ�Ǹ��У���ô�е���������Ӧ��������������ʱ��׼ȷ���塣���������IDӦ���Ǹ�������һ��׼ȷ���ʽ����Ϊ
	** һ���ܺϳ���һ���Ӳ�ѯ�С������������ʽ�����е�Select����Ϊ��һ���С�
	**
	**   SELECT col FROM tbl;
	**   SELECT (SELECT col FROM tbl;
	**   SELECT (SELECT col FROM tbl);
	**   SELECT abc FROM (SELECT col AS abc FROM tbl);
	**
	** ���������Ϳ��������κα��ʽ�ϣ������ǿյ��С�
	*/
	static const char *columnType(
	  NameContext *pNC, /*����һ�����������Ľṹ�壨����������е����֣�*/
	  Expr *pExpr,
	  const char **pzOriginDb,
	  const char **pzOriginTab,
	  const char **pzOriginCol
	){
	  char const *zType = 0;
	  char const *zOriginDb = 0;
	  char const *zOriginTab = 0;
	  char const *zOriginCol = 0;
	  int j;
	  if( NEVER(pExpr==0) || pNC->pSrcList==0 ) return 0;/*���һ�����ʽΪ�ջ�������ֵ��б��ǿյģ�ֱ�ӷ���0*/

	  switch( pExpr->op ){/*�������ʽ�еĲ���*/
		case TK_AGG_COLUMN:
		case TK_COLUMN: {
		  /* The expression is a column. Locate the table the column is being
		  ** extracted from in NameContext.pSrcList. This table may be real
		  ** database table or a subquery.
		  */
		  /*
		  **������ʽ��һ�У�����������һ�д������������е�pSrcList��һ��������������������������ȡ�����ġ�
		  **������������ʵ�����ݿ�������һ���Ӳ�ѯ��
		  */
		  Table *pTab = 0;            /* Table structure column is extracted from *//*����ȡ������ɵı�*/
		  Select *pS = 0;             /* Select the column is extracted from *//*����ȡ������ɵ�select�ṹ��*/
		  int iCol = pExpr->iColumn;  /* Index of column in pTab *//*pTab�е�������*/
		  testcase( pExpr->op==TK_AGG_COLUMN );/*������ʽ�Ĳ����Ƿ���TK_AGG_COLUMN��Ƕ���У�*/
		  testcase( pExpr->op==TK_COLUMN );/*������ʽ�Ĳ����Ƿ���TK_COLUMN����������*/
		  while( pNC && !pTab ){/*���������Ľṹ����ڣ�����ȡ�ı�ṹ�У�����һ������ȡ������ɵı�������*/
			SrcList *pTabList = pNC->pSrcList;/*���������Ľṹ�����б�ֵ������FROM����Դ����Ӳ�ѯ������б�*/
			for(j=0;j<pTabList->nSrc && pTabList->a[j].iCursor!=pExpr->iTable;j++);/*������ѯ�б�*/
			if( j<pTabList->nSrc ){/*���jС���б��б���ܸ���*/
			  pTab = pTabList->a[j].pTab;/*��ֵ�б��еĵ�j-1���Table�ṹ���ʵ�����pTab*/
			  pS = pTabList->a[j].pSelect;/*��ֵ�б��еĵ�j-1���select�ṹ���ps*/
			}else{
			  pNC = pNC->pNext;/*���򣬽����������Ľṹ�����һ���ⲿ���������ĸ�ֵ��pNC����*/
			}
		  }

		  if( pTab==0 ){/*�����Ϊ��*/
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
			** branch below.  */
			/*
			** �����Ϊ�յ�ʱ��ʹ�� "SELECT new.x" �������Ĵ���������ִ�С�
			** Ȼ���������¹����˴��������룬��������������ٿ��ܷ�����������Σ�������﷨����ȷ�ģ�
			**
			**   CREATE TABLE t1(col INTEGER);
			**   SELECT (SELECT t1.col) FROM FROM t1;
			**
			** ��columnType�������ͣ������ʽ"t1.col"���Ӳ�ѯ�е��á�����������£�����������Ϊ��NULL������ʹ
			** �����Ӧ������Ϊ��INTEGER���������͡�
			**
			** �ⲻ��һ�����⣬�����"t1.col"����û��ʹ�á������"t1.col"�����ͱ�"(SELECT t1.col)"���ʽ���ã������ȷ������
			** ���ᱻ���أ��μ�����TK_SELECT��֧����
			**
			*/
			break;
		  }

		  assert( pTab && pExpr->pTab==pTab );/*����ϵ㣬�жϱ���ȡ������ɵı���ڻ��߱��ʽ�е��б����*/
		  if( pS ){/*�������ȡ������ɵ�select�ṹ�����*/
			/* The "table" is actually a sub-select or a view in the FROM clause
			** of the SELECT statement. Return the declaration type and origin
			** data for the result-set column of the sub-select.
			*/
			/*
			**�����ʵ������һ���Ӳ�ѯ�ṹ������һ��SELECT�﷨��FROM�Ӿ����ͼ������������������ͻ���ԭʼ�����ݸ��Ӳ�ѯ��
			**������С�
			*/
			if( iCol>=0 && ALWAYS(iCol<pS->pEList->nExpr) ){
			  /* If iCol is less than zero, then the expression requests the
			  ** rowid of the sub-select or view. This expression is legal (see 
			  ** test case misc2.2.2) - it always evaluates to NULL.
			  */
			  /*�������к�С��0��Ȼ��������ʽ�����Ӳ�ѯ�������ͼ�Ĺؼ��֡�������ʽ�ǺϷ��ģ��μ�����misc2.2.2�����
			  **���ܱ���Ϊ�ǿ�ֵ�� 
			  */
			  NameContext sNC;
			  Expr *p = pS->pEList->a[iCol].pExpr;/*����ȡ������ɵ�select�ṹ���б��ʽ�б��е�i�����ʽ��ֵ��p*/
			  sNC.pSrcList = pS->pSrc;/*����ȡ������ɵ�select�ṹ����pSrc��FROM�Ӿ䣩��ֵ��pSrcList��һ���������������������ԣ�*/
			  sNC.pNext = pNC;/*���������Ľṹ�帳ֵ����ǰ���������Ľṹ���nextָ��*/
			  sNC.pParse = pNC->pParse;/*���������Ľṹ���е��﷨��������ֵ����ǰ���������Ľṹ����﷨������*/
			  zType = columnType(&sNC, p, &zOriginDb, &zOriginTab, &zOriginCol); /*�����ɵ��������͸�ֵ��zType*/
			}
		  }else if( ALWAYS(pTab->pSchema) ){/*pTab���ģʽ����*/
			/* A real table *//*һ����ʵ�ı�*/
			assert( !pS );/*����ϵ㣬�ж�Select�ṹ���Ƿ�Ϊ��*/
			if( iCol<0 ) iCol = pTab->iPKey;/*����к�С��0�������еĹؼ����������Ԫ�ظ�ֵ��ICol*/
			assert( iCol==-1 || (iCol>=0 && iCol<pTab->nCol) );/*����ϵ㣬�ж�ICol��ȷ�����ĸ���Χ*/
			if( iCol<0 ){/*���ICol��С��0*/
			  zType = "INTEGER";/*�����Ͷ���Ϊ����*/
			  zOriginCol = "rowid";/*�ؼ���Ϊrowid*/
			}else{
			  zType = pTab->aCol[iCol].zType;/*���򣬶�������Ϊ���ͱ��е�iCol������*/
			  zOriginCol = pTab->aCol[iCol].zName;/*���͵�����Ϊ�ͱ��е�iCol������*/
			}
			zOriginTab = pTab->zName;/*ʹ��Ĭ�ϵ����֣���������*/
			if( pNC->pParse ){/*������������Ľṹ���е��﷨����������*/
			  int iDb = sqlite3SchemaToIndex(pNC->pParse->db, pTab->pSchema);/*��Schema��ָ��ת�������������Ľṹ���з����﷨����db*/
			  zOriginDb = pNC->pParse->db->aDb[iDb].zName;/*���������﷨�������е�db�е�i-1��Db��������ֵ��zOriginDb���ݿ���*/
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
		  ** ������ʽ���Ӳ�ѯ������һ���������ͺͳ�ʼ��Ϣ��select������е�һ�С�
		  */
		  NameContext sNC;
		  Select *pS = pExpr->x.pSelect;/*�����ʽ��Select�ṹ�帳ֵ��һ��SELECT�ṹ��ʵ�����*/
		  Expr *p = pS->pEList->a[0].pExpr;/*��SELECT�ı��ʽ�б��е�һ�����ʽ��ֵ�����ʽ����p*/
		  assert( ExprHasProperty(pExpr, EP_xIsSelect) );/*����ϵ㣬�����Ƿ����EP_xIsSelect���ʽ*/
		  sNC.pSrcList = pS->pSrc;/*��SELECT�ṹ����FROM�Ӿ�����Ը�ֵ�����������Ľṹ����FROM�Ӿ��б�*/
		  sNC.pNext = pNC;/*���������Ľṹ�帳ֵ����ǰ���������Ľṹ���nextָ��*/
		  sNC.pParse = pNC->pParse;/*�����������Ľṹ���з����﷨����ֵ����ǰ�����ṹ��ķ����﷨������*/
		  zType = columnType(&sNC, p, &zOriginDb, &zOriginTab, &zOriginCol); /*������������*/
		  break;
		}
	#endif
	  }
	  
	  if( pzOriginDb ){/*�������ԭʼ�����ݿ�*/
		assert( pzOriginTab && pzOriginCol );/*����ϵ㣬�жϱ�����Ƿ����*/
		*pzOriginDb = zOriginDb;/*�ļ������ݿ⸳ֵ�����ݿ����pzOriginDb*/
		*pzOriginTab = zOriginTab;/*�ļ��б�ֵ�������pzOriginTab*/
		*pzOriginCol = zOriginCol;/*�ļ����и�ֵ���б���pzOriginCol*/
	  }
	  return zType;/*����������*/
	}

	/*
	** Generate code that will tell the VDBE the declaration types of columns
	** in the result set.
	*/
	/*���ɴ������VDBE�������������������*/
	static void generateColumnTypes(
	  Parse *pParse,      /* Parser context *//*����������*/
	  SrcList *pTabList,  /* List of tables *//*����*/
	  ExprList *pEList    /* Expressions defining the result set *//*���������ı��ʽ�б�*/
	){
	#ifndef SQLITE_OMIT_DECLTYPE
	  Vdbe *v = pParse->pVdbe;/*�������﷨����VDBE��ֵ��VDBE����v*/
	  int i;
	  NameContext sNC;
	  sNC.pSrcList = pTabList;/*�����ϸ�ֵ�����������Ľṹ���еı�������*/
	  sNC.pParse = pParse;/*�������﷨����ֵ�������������﷨��*/
	  for(i=0; i<pEList->nExpr; i++){/*�������ʽ�б�*/
		Expr *p = pEList->a[i].pExpr;/*�����ʽ������ֵ��p*/
		const char *zType;
	#ifdef SQLITE_ENABLE_COLUMN_METADATA
		const char *zOrigDb = 0;
		const char *zOrigTab = 0;
		const char *zOrigCol = 0;
		zType = columnType(&sNC, p, &zOrigDb, &zOrigTab, &zOrigCol);/*����ǰ�ı��ʽ����columnType����������������*/

		/* The vdbe must make its own copy of the column-type and other 
		** column specific strings, in case the schema is reset before this
		** virtual machine is deleted.
		*/
		/*VDBE�����ܸ��������ͺ��������Զ��������ͣ��������ɾ��֮ǰ����Ҫ�������ģʽ*/
		sqlite3VdbeSetColName(v, i, COLNAME_DATABASE, zOrigDb, SQLITE_TRANSIENT);/*�������ݿ������еĵ�i-1����*/
		sqlite3VdbeSetColName(v, i, COLNAME_TABLE, zOrigTab, SQLITE_TRANSIENT);/*���ر������еĵ�i-1����*/
		sqlite3VdbeSetColName(v, i, COLNAME_COLUMN, zOrigCol, SQLITE_TRANSIENT);/*�����������еĵ�i-1����*/
	#else
		zType = columnType(&sNC, p, 0, 0, 0);/*��������������*/
	#endif
		sqlite3VdbeSetColName(v, i, COLNAME_DECLTYPE, zType, SQLITE_TRANSIENT);/*�������ͻ�ȡ�����еĵ�i-1����*/
	  }
	#endif /* SQLITE_OMIT_DECLTYPE */
	}

	/*
	** Generate code that will tell the VDBE the names of columns
	** in the result set.  This information is used to provide the
	** azCol[] values in the callback.
	*/
	/*
	**���ɴ������VDBE�����������������.�����Ϣ�ǻص������ṩ��azCol�����ֵ��
	*/
	static void generateColumnNames(
	  Parse *pParse,      /* Parser context *//*����������*/
	  SrcList *pTabList,  /* List of tables *//*��ļ���*/
	  ExprList *pEList    /* Expressions defining the result set *//*���������ı��ʽ�б�*/
	){
	  Vdbe *v = pParse->pVdbe;/*���﷨��������VDBE���Ը�ֵ��VDBE����v*/
	  int i, j;
	  sqlite3 *db = pParse->db;/*���﷨�����������ݿ⸳ֵ�����ݿ����ӱ���db*/
	  int fullNames, shortNames;/*����������������һ����ȫ�ƣ��ڶ����Ǽ�д*/

	#ifndef SQLITE_OMIT_EXPLAIN
	  /* If this is an EXPLAIN, skip this step *//*�������һ�����ʽ�������˲�*/
	  if( pParse->explain ){/*����﷨�������е�explain���Դ��ڣ�ֱ�ӷ���*/
		return;
	  }
	#endif

	  if( pParse->colNamesSet || NEVER(v==0) || db->mallocFailed ) return;/*��������﷨���������������ϻ򲻴���VDBE����������ڴ�ʧ�ܶ���ֱ�ӷ���*/
	  pParse->colNamesSet = 1;/*�����﷨����������������Ϊ1*/
	  fullNames = (db->flags & SQLITE_FullColNames)!=0;/*������ݿ����ӵ����ֽ�SQLIteȫ��������Ϊ�գ��������ظ�����ΪfullNames*/
	  shortNames = (db->flags & SQLITE_ShortColNames)!=0;/*������ݿ����ӵ����ֽ�SQLIte���������Ϊ�գ��������ظ�����ΪshortNames*/
	  sqlite3VdbeSetNumCols(v, pEList->nExpr);/*���ݱ��ʽ���ؽ�����е�����*/
	  for(i=0; i<pEList->nExpr; i++){/*������ʾ���б�*/
		Expr *p;
		p = pEList->a[i].pExpr;/*�����ʽ�б��е�i-1����ֵ��p*/
		if( NEVER(p==0) ) continue;/*���pΪ0��ֱ������*/
		if( pEList->a[i].zName ){/*�����i-1�����ʽ�����ִ���*/
		  char *zName = pEList->a[i].zName;/*����i-1�����ʽ�����ִ��ڸ�ֵ��zName*/
		  sqlite3VdbeSetColName(v, i, COLNAME_NAME, zName, SQLITE_TRANSIENT);/*����SQLִ�к󷵻ؽ����������*/
		}else if( (p->op==TK_COLUMN || p->op==TK_AGG_COLUMN) && pTabList ){/*������ʽ�в���ΪTK_COLUMN��TK_AGG_COLUMN���������ļ����н���*/
		  Table *pTab;
		  char *zCol;
		  int iCol = p->iColumn;/*�����ʽ��iColumn��ֵ��ICol*/
		  for(j=0; ALWAYS(j<pTabList->nSrc); j++){/*��������*/
			if( pTabList->a[j].iCursor==p->iTable ) break;/*������е��α�ָ��ITable���ж�*/
		  }
		  assert( j<pTabList->nSrc );/*����ϵ㣬�ж�jС�ڱ��ϵ���Ŀ*/
		  pTab = pTabList->a[j].pTab;/*�������б�ֵ������pTab*/
		  if( iCol<0 ) iCol = pTab->iPKey;/*�����С��0������iColΪ��ǰ�����������*/
		  assert( iCol==-1 || (iCol>=0 && iCol<pTab->nCol) );/*����ϵ㣬�ж�iCol�ķ�Χ*/
		  if( iCol<0 ){/*���iColС��0*/
			zCol = "rowid";/*����zColΪ��������rowid��*/
		  }else{
			zCol = pTab->aCol[iCol].zName;/*������zColΪ��ǰ���i-1�е�����*/
		  }
		  if( !shortNames && !fullNames ){/*����Ȳ��Ǽ���ֲ���ȫ��*/
			sqlite3VdbeSetColName(v, i, COLNAME_NAME, 
				sqlite3DbStrDup(db, pEList->a[i].zSpan), SQLITE_DYNAMIC);
	/*����һ���ѷ�������ݿ�������pEList->a[i].zSpan��ֵ���ڴ棬�ٷ��ظ�sqlite3VdbeSetColName������������������*/
		  }else if( fullNames ){/*�����ȫ��*/
			char *zName = 0;
			zName = sqlite3MPrintf(db, "%s.%s", pTab->zName, zCol);/*����ΪpTab�е�zCol��*/
			sqlite3VdbeSetColName(v, i, COLNAME_NAME, zName, SQLITE_DYNAMIC);/*��������������*/
		  }else{
			sqlite3VdbeSetColName(v, i, COLNAME_NAME, zCol, SQLITE_TRANSIENT);/*��������������*/
		  }
		}else{
		  sqlite3VdbeSetColName(v, i, COLNAME_NAME, 
			  sqlite3DbStrDup(db, pEList->a[i].zSpan), SQLITE_DYNAMIC);
			  /*����һ���ѷ�������ݿ�������pEList->a[i].zSpan��ֵ���ڴ棬�ٷ��ظ�sqlite3VdbeSetColName������������������*/
		}
	  }
	  generateColumnTypes(pParse, pTabList, pEList);/*�����﷨������������б�ͱ��ʽ�б����������*/
	}

	/*
	** Given a an expression list (which is really the list of expressions
	** that form the result set of a SELECT statement) compute appropriate
	** column names for a table that would hold the expression list.
	**
	** All column names will be unique.
	**
	** Only the column names are computed.  Column.zType, Column.zColl,
	** and other fields of Column are zeroed.
	**
	** Return SQLITE_OK on success.  If a memory allocation error occurs,
	** store NULL in *paCol and 0 in *pnCol and return SQLITE_NOMEM.
	*/
	/*
	** ����һ�����ʽ�б�����б���һ��������SELECT������ı��ʽ�б���Ϊ������ʽ�б�ı�����ʵ�������
	** 
	** ���õ��ж���Ψһ��
	** 
	** ֻ�м�������������е����ͣ��еĳ��Ⱥ��е������ֶβ��ܽ��г�ʼ����
	** 
	** �ɹ��ķ���SQLITE_OK����������ڴ������󣬽��洢NULL��paCol��0��pnCol�����ҷ���SQLITE_NOMEM��δ�洢��ֵ
	*/
	static int selectColumnsFromExprList(
	  Parse *pParse,          /* Parsing context *//*����������*/
	  ExprList *pEList,       /* Expr list from which to derive column names *//*�����������ı��ʽ�б�*/
	  int *pnCol,             /* Write the number of columns here *//*д����*/
	  Column **paCol          /* Write the new column list here *//*д���µ��б�*/
	){
	  sqlite3 *db = pParse->db;   /* Database connection *//*����һ�����ݿ�����*/
	  int i, j;                   /* Loop counters *//*ѭ������*/
	  int cnt;                    /* Index added to make the name unique *//*������Ӳ���Ψһ������*/
	  Column *aCol, *pCol;        /* For looping over result columns *//*ѭ�������Ľ����*/
	  int nCol;                   /* Number of columns in the result set *//*��������е�����*/
	  Expr *p;                    /* Expression for a single result column *//*һ����������еı��ʽ*/
	  char *zName;                /* Column name *//*����*/
	  int nName;                  /* Size of name in zName[] *//*�������������ĳ���*/

	  if( pEList ){/*��������������ı��ʽ�б����*/
		nCol = pEList->nExpr;/*��������е��������ڱ��ʽ�б��б��ʽ�ĸ���*/
		aCol = sqlite3DbMallocZero(db, sizeof(aCol[0])*nCol);/*��ѭ�������Ľ���еĳ�ʼ�������䲢����ڴ棬�����СΪ�ڶ����������ڴ�*/
		testcase( aCol==0 );/*���������Ƿ�Ϊ0*/
	  }else{
		nCol = 0;/*����������Ϊ0*/
		aCol = 0;/*ѭ�������Ľ����������Ϊ0*/
	  }
	  *pnCol = nCol;/*д������������*/
	  *paCol = aCol;/*д�б������ڽ������*/

	  for(i=0, pCol=aCol; i<nCol; i++, pCol++){/*�������еĽ����*/
		/* Get an appropriate name for the column
		*/
		/*����ȥһ�����ʵ�����*/
		p = pEList->a[i].pExpr;/*���ʽ�е�i-1�����ʽΪp*/
		assert( p->pRight==0 || ExprHasProperty(p->pRight, EP_IntValue)
				   || p->pRight->u.zToken==0 || p->pRight->u.zToken[0]!=0 );
		/*����ϵ㣬���ʽ�����ӽڵ��Ƿ�Ϊ�ջ���ʽ�����ӽڵ�ֵ�Ƿ�Ϊint����ʽ�����ӽڵ�ı��ֵΪ0�����ӽڵ�ĵ�һ�����ֵ��Ϊ0*/
		if( (zName = pEList->a[i].zName)!=0 ){/*��������ͱ��ʽ��������ͬ*/
		  /* If the column contains an "AS <name>" phrase, use <name> as the name */
		  /* �������а�����һ����AS <name>�Ķ����ʹ��<name>��Ϊ����*/
		  zName = sqlite3DbStrDup(db, zName);/*copy���ݿ������е�������ʵ����copy��ʱ���ڴ��������ַ�������ֵ������*/
		}else{
		  Expr *pColExpr = p;  /* The expression that is the result column name *//*��������ı��ʽ*/
		  Table *pTab;         /* Table associated with this expression *//*��Ӧ������ʽ�ı�*/
		  while( pColExpr->op==TK_DOT ){/*ֱ����������ı��ʽ�Ĳ���ΪTK_DOT*/
			pColExpr = pColExpr->pRight;/*��ֵ������������ӽڵ���������������ǵݹ�Ĺ���*/
			assert( pColExpr!=0 );/*����ϵ㣬�жϽ��������Ϊ0*/
		  }
		  if( pColExpr->op==TK_COLUMN && ALWAYS(pColExpr->pTab!=0) ){/*�������������ʽ�Ĳ���ΪTK_COLUMN�����ұ��ʽ��Ӧ�ı�Ϊ��*/
			/* For columns use the column name name */
			/* ��ʹ����������*/
			int iCol = pColExpr->iColumn;/*�����ʽ���и�ֵ��iCol*/
			pTab = pColExpr->pTab;/*���ʽ��Ӧ�ı�ֵ��pTab*/
			if( iCol<0 ) iCol = pTab->iPKey;/*���iColС��0����iCol��ֵΪ�������*/
			zName = sqlite3MPrintf(db, "%s",
					 iCol>=0 ? pTab->aCol[iCol].zName : "rowid");/*��ӡ�������������ֵ��zName*/
		  }else if( pColExpr->op==TK_ID ){/*������ʽ�Ĳ���ΪTK_ID*/
			assert( !ExprHasProperty(pColExpr, EP_IntValue) );/*����ϵ㣬�жϽ�������ı��ʽ�Ƿ���Intֵ����Ϊʹ���ˡ�ID��*/
			zName = sqlite3MPrintf(db, "%s", pColExpr->u.zToken);/*��ӡ��������ı��ʽ�ı��ֵ������ֵ��zName*/
		  }else{
			/* Use the original text of the column expression as its name */
			/* ʹ��������б��ʽ�ı���������*/
			zName = sqlite3MPrintf(db, "%s", pEList->a[i].zSpan);/*��ӡ�����������ı��ʽ�б��ԭ�ģ�����ֵ��zName*/
		  }
		}
		if( db->mallocFailed ){/*�����ڴ����*/
		  sqlite3DbFree(db, zName);/*�ͷ����ݿ������е�����*/
		  break;
		}

		/* Make sure the column name is unique.  If the name is not unique,
		** append a integer to the name so that it becomes unique.
		*/
		/*ȷ��������Ψһ�ġ����������Ψһ�����һ������������ΪΨһ�ġ�*/
		nName = sqlite3Strlen30(zName);/*���������ĳ��Ȳ��ܳ���30*/
		for(j=cnt=0; j<i; j++){
		  if( sqlite3StrICmp(aCol[j].zName, zName)==0 ){/*�����ʼ����Ľ�����е��е����ֺ���Name��ͬ*/
			char *zNewName;
			zName[nName] = 0;
			zNewName = sqlite3MPrintf(db, "%s:%d", zName, ++cnt);/*��ʽ�������zName����++cnt���ַ�������ֵ��zNewName*/
			sqlite3DbFree(db, zName);/*�ͷſ��ܹ��������ݿ����ӵ��ڴ�*/
			zName = zNewName;/*��zName��ֵΪ��ʽ��������zNewNameֵ*/
			j = -1;/*�´�ѭ��jֵΪ0*/
			if( zName==0 ) break;/*���zNameΪ0��˵��û��zName�ַ������Ͳ����ٸ�ֵ*/
		  }
		}
		pCol->zName = zName;/*����������У����õ�zNameֵд���������*/
	  }
	  if( db->mallocFailed ){/*�������ʧ��*/
		for(j=0; j<i; j++){
		  sqlite3DbFree(db, aCol[j].zName);/*�ͷŹ�����������ΪzName���ڴ��*/
		}
		sqlite3DbFree(db, aCol);/*�ͷŹ�������е����ݿ�����*/
		*paCol = 0;/*��д�б���Ϊ0*/
		*pnCol = 0;/*������Ϊ0*/
		return SQLITE_NOMEM;/*����SQLITE_NOMEM��û�з��䣩*/
	  }
	  return SQLITE_OK;/*����SQLITE_OKֵ*/
	}

	/*
	** Add type and collation information to a column list based on
	** a SELECT statement.
	** 
	** The column list presumably came from selectColumnNamesFromExprList().
	** The column list has only names, not types or collations.  This
	** routine goes through and adds the types and collations.
	**
	** This routine requires that all identifiers in the SELECT
	** statement be resolved.
	*/
	/*
	** ��selectָ��Ļ��������һ�����ͺ�У����Ϣ���б�
	** 
	** ����б��������selectColumnNamesFromExprList�������������Ψһ�����������ͺ��������Ψһ��
	** �������ͨ����������ͺ��������
	** 
	** �������Ҫ��selectָ�������б�ʾ������ȷ���ġ�
	*/
	static void selectAddColumnTypeAndCollation(
	  Parse *pParse,        /* Parsing contexts *//*����������*/
	  int nCol,             /* Number of columns *//*�еĸ���*/
	  Column *aCol,         /* List of columns *//*�б�*/
	  Select *pSelect       /* SELECT used to determine types and collations *//*����ȷ�����ͺ���������select�ṹ��*/
	){
	  sqlite3 *db = pParse->db;/*����һ�����ݿ�����*/
	  NameContext sNC;/*����һ�����������Ľṹ�壨����������е����֣�*/
	  Column *pCol;/*����һ���б�*/
	  CollSeq *pColl;/*����һ���������*/
	  int i;
	  Expr *p;/*����һ�����ʽ����*/
	  struct ExprList_item *a;/*����һ�����ʽ�б������*/

	  assert( pSelect!=0 );/*����ϵ㣬��������ȷ�����ͺ���������select�ṹ���Ƿ�Ϊ��*/
	  assert( (pSelect->selFlags & SF_Resolved)!=0 );/*����ϵ㣬���select�ṹ���е�selFlagsΪSF_Resolved*/
	  assert( nCol==pSelect->pEList->nExpr || db->mallocFailed );/*����ϵ㣬�ж������Ƿ���ڱ��ʽ�����������ݿ������Ƿ�����ڴ�ʧ��*/
	  if( db->mallocFailed ) return;/*���ݿ������Ƿ�����ڴ�ʧ��*/
	  memset(&sNC, 0, sizeof(sNC));/*��sNC��ǰsizeof(sNC)���ֽ���0�滻*/
	  sNC.pSrcList = pSelect->pSrc;/*��SELECT�ṹ����б�ֵ�������ṹ����б�/
	  a = pSelect->pEList->a;/*��SELECT�ṹ���б��ʽ�б��еĽϴ�ı��ʽ��a*/
	  for(i=0, pCol=aCol; i<nCol; i++, pCol++){/*������*/
		p = a[i].pExpr;/*��p�������б��ʽ*/
		pCol->zType = sqlite3DbStrDup(db, columnType(&sNC, p, 0, 0, 0));/*�������������еı��ʽ�����͸�ֵ��zType*/
		pCol->affinity = sqlite3ExprAffinity(p);/*�������������еı��ʽ�����ͽ����׺��Դ����affinity*/
		if( pCol->affinity==0 ) pCol->affinity = SQLITE_AFF_NONE;/*����б���׺��Դ���ֵΪ0��SQLITE_AFF_NONE��û�У�*/
		pColl = sqlite3ExprCollSeq(pParse, p);/*��ô���ݷ����﷨�����б��ʽ����һ���������*/
		if( pColl ){/*��������������*/
		  pCol->zColl = sqlite3DbStrDup(db, pColl->zName);/*�������ݿ����Ӻ�������е���������һ���������*/
		}
	  }
	}

	/*
	** Given a SELECT statement, generate a Table structure that describes
	** the result set of that SELECT.
	*/
	/*
	** ��һ��SELECTָ�����һ������SELECT������ı�ṹ
	*/
	Table *sqlite3ResultSetOfSelect(Parse *pParse, Select *pSelect){
	  Table *pTab; /*����һ��Table�ṹ��*/
	  sqlite3 *db = pParse->db; /*����һ�����ݿ�����*/
	  int savedFlags; 

	  savedFlags = db->flags; /*�������ݿ����ӱ��*/
	  db->flags &= ~SQLITE_FullColNames; /*λ��SQLite��������ȫ��*/
	  db->flags |= SQLITE_ShortColNames; /*����SQLite�������ļ��*/
	  sqlite3SelectPrep(pParse, pSelect, 0); /*�����﷨��������SELECT�ṹ�彨��һ��SELECTָ��*/
	  if( pParse->nErr ) return 0; /*����﷨���������д���ֱ�ӷ���0*/
	  while( pSelect->pPrior ) pSelect = pSelect->pPrior; /*����SELECT�ṹ�������Ȳ���*/
	  db->flags = savedFlags; /*�����ݿ����ӱ�Ƿŵ����ݿ����ӵ�flags������*/
	  pTab = sqlite3DbMallocZero(db, sizeof(Table) ); /*���䲢����ڴ棬�����СΪ��Table�Ĵ�С���ڴ�*/
	  if( pTab==0 ){ /*�����Ϊ��*/
		return 0;/*ֱ�ӷ���0*/
	  }
	  /* The sqlite3ResultSetOfSelect() is only used n contexts where lookaside
	  ** is disabled */
	  /* sqlite3ResultSetOfSelect��������ֻ������n�������ģ�����ֻ����һ���󱸣�lookaside����������*/
	  assert( db->lookaside.bEnabled==0 ); /*����ϵ㣬�жϺ󱸣�lookaside���������Ƿ����*/
	  pTab->nRef = 1; /*�����ָ�������Ϊ1*/
	  pTab->zName = 0; /*�������Ϊ0*/
	  pTab->nRowEst = 1000000; /*���Ʊ������*/
	  selectColumnsFromExprList(pParse, pSelect->pEList, &pTab->nCol, &pTab->aCol); 
	  /*�����﷨��������SELECT�ṹ���б��ʽ�б�����������б������*/
	  selectAddColumnTypeAndCollation(pParse, pTab->nCol, pTab->aCol, pSelect); /*����е����ͺ�У����Ϣ*/
	  pTab->iPKey = -1; /*��������Ϊ-1*/
	  if( db->mallocFailed ){ /*��������ڴ�ʧ��*/
		sqlite3DeleteTable(db, pTab); /*ɾ�������*/
		return 0;/*����0*/
	  }
	  return pTab;/*��������SELECT������ı�ṹ*/
	}

	/*
	** Get a VDBE for the given parser context.  Create a new one if necessary.
	** If an error occurs, return NULL and leave a message in pParse.
	*/
	/*�õ�һ���������ݿ����洦������������﷨�������������Ҫ����һ���µġ�����������󣬷��ؿ�ֵ���ڷ����﷨������һ��������Ϣ*/
	Vdbe *sqlite3GetVdbe(Parse *pParse){
	  Vdbe *v = pParse->pVdbe;/*����һ���������ݿ�����*/
	  if( v==0 ){/*����������ݿ�����Ϊ��*/
		v = pParse->pVdbe = sqlite3VdbeCreate(pParse->db);
	/*��ôʹ�õ�ǰ��VDBE�����﷨����������һ���������ݿ����棬����ֵ���﷨�������е�pVdbe�;ֲ�����v*/
	#ifndef SQLITE_OMIT_TRACE
		if( v ){/*����������ݿ����治Ϊ��*/
		  sqlite3VdbeAddOp0(v, OP_Trace);/*���OP_Traceָ�VDBE�����������ָ��ĵ�ַ*/
		}
	#endif
	  }
	  return v;/*���ش����ɹ���VDBE*/
	}


	/*
	** Compute the iLimit and iOffset fields of the SELECT based on the
	** pLimit and pOffset expressions.  pLimit and pOffset hold the expressions
	** that appear in the original SQL statement after the LIMIT and OFFSET
	** keywords.  Or NULL if those keywords are omitted. iLimit and iOffset 
	** are the integer memory register numbers for counters used to compute 
	** the limit and offset.  If there is no limit and/or offset, then 
	** iLimit and iOffset are negative.
	**
	** This routine changes the values of iLimit and iOffset only if
	** a limit or offset is defined by pLimit and pOffset.  iLimit and
	** iOffset should have been preset to appropriate default values
	** (usually but not always -1) prior to calling this routine.
	** Only if pLimit!=0 or pOffset!=0 do the limit registers get
	** redefined.  The UNION ALL operator uses this property to force
	** the reuse of the same limit and offset registers across multiple
	** SELECT statements.
	*/
	/*
	** ����SELECT��Limit��Offset���ʽ����limit��offset����pLimit��pOffset������ԭʼ��SQL����LIMIT��OFFSET�ؼ��ֺ�
	** �����Щ�ؼ����ǿգ��Ǿͺ��ԡ�iLimit��iOffset�����Ĵ��������������limit��offset�����û��limit�ͣ���offset��
	** iLimit��iOffsetΪ������
	**
	** ���iLimit��iOffset��pLimit��pOffset���壬��ô��������ܸı�iLimit��iOffset��ֵ��iLimit��iOffsetӦ�ñ�Ԥ�ȶ���һ��ֵ
	** ��Ӧ�õ�������Ϊ-1������ʹ������������ֻ��pLimit!=0��pOffset!=0����ôiLimit�Ĵ����豻���¶��塣
	** �ڶ��ز�ѯ��UNION ALL������ʹ��һ�����ʣ�ǿ������ʹ��limit��offset����ͬ�ļĴ�����
	*/
	static void computeLimitRegisters(Parse *pParse, Select *p, int iBreak){
	  Vdbe *v = 0;/*��ʼ��vdbe����*/
	  int iLimit = 0;/*��ʼ��iLimit*/
	  int iOffset;/*����iOffset*/
	  int addr1, n;
	  if( p->iLimit ) return;/*���SELECT�ṹ���к���limitƫ����ֱ�ӷ���*/

	  /* 
	  ** "LIMIT -1" always shows all rows.  There is some
	  ** contraversy about what the correct behavior should be.
	  ** The current implementation interprets "LIMIT 0" to mean
	  ** no rows.
	  */
	  /*
	  **"LIMIT -1"���ǳ������������С�������ȷ����Ϊ���෴�����
	  ** ��ǰӦ�ý���"LIMIT 0"Ϊ���С�
	  */
	  sqlite3ExprCacheClear(pParse);/*��������е��﷨������*/
	  assert( p->pOffset==0 || p->pLimit!=0 );/*����ϵ㣬�ж�ƫ�����Ƿ�Ϊ0�������в�Ϊ0*/
	  if( p->pLimit ){/*��������д���*/
		p->iLimit = iLimit = ++pParse->nMem;/*����ֵΪ�﷨�������е�ռ�ڴ��������û�ҵ�nMem,�����Ƕ��ٸ�Memory���˴�Ĭ��Ϊȫ�����أ�*/
		v = sqlite3GetVdbe(pParse);/*�����﷨����������һ���������ݿ�����*/
		if( NEVER(v==0) ) return;  /* VDBE should have already been allocated *//*vdbeӦ���Ѿ�������*/
		if( sqlite3ExprIsInteger(p->pLimit, &n) ){/*�ж����limit�Ƿ����32�����С�ڰ�n��Ϊ1��������0*/
		  sqlite3VdbeAddOp2(v, OP_Integer, n, iLimit);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  VdbeComment((v, "LIMIT counter"));/*��vdbe�д���"LIMIT counter"�ַ�������Ӧ�Ͼ�ĵ�ַ*/
		  if( n==0 ){/*���nΪ0*/
			sqlite3VdbeAddOp2(v, OP_Goto, 0, iBreak);/*��OP_Integer��������vdbe��Ȼ�󲻷��ص�ַ��iBreak�趨��*/
		  }else{
			if( p->nSelectRow > (double)n ) p->nSelectRow = (double)n;
			/*���SELECT�ṹ���е�������nSelectRow������n��ֱ������nSelectRowΪn*/
		  }
		}else{
		  sqlite3ExprCode(pParse, p->pLimit, iLimit);/*���pLimit���ʽ��iLimit��ֵ*/
		  sqlite3VdbeAddOp1(v, OP_MustBeInt, iLimit);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  VdbeComment((v, "LIMIT counter"));/*��vdbe�д���"LIMIT counter"�ַ�������Ӧ�Ͼ�ĵ�ַ*/
		  sqlite3VdbeAddOp2(v, OP_IfZero, iLimit, iBreak);/*��OP_Integer��������vdbe��Ȼ�󲻷��ص�ַ��iBreak�趨��*/
		}
		if( p->pOffset ){/*����ṹ����ƫ����*/
		  p->iOffset = iOffset = ++pParse->nMem;/*����ֵΪ�﷨�������е�ռ�ڴ��������û�ҵ�nMem,�����Ƕ��ٸ�Memory���˴�Ĭ�Ͽ���ƫ�Ƶ����ֵ��*/
		  pParse->nMem++;   /* Allocate an extra register for limit+offset *//*����һ������ļĴ���Ϊlimit+offset*/
		  sqlite3ExprCode(pParse, p->pOffset, iOffset);/*���pOffset���ʽ��iOffset��ֵ*/
		  sqlite3VdbeAddOp1(v, OP_MustBeInt, iOffset);/*��OP_MustBeInt��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  VdbeComment((v, "OFFSET counter"));/*��vdbe�д���"OFFSET counter"�ַ�������Ӧ�Ͼ�ĵ�ַ*/
		  addr1 = sqlite3VdbeAddOp1(v, OP_IfPos, iOffset);/*��OP_IfPos��������vdbe��Ȼ�󷵻���������ĵ�ַ����ֵ��addr1*/
		  sqlite3VdbeAddOp2(v, OP_Integer, 0, iOffset);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  sqlite3VdbeJumpHere(v, addr1);/*���е�ַ����addr1*/
		  sqlite3VdbeAddOp3(v, OP_Add, iLimit, iOffset, iOffset+1);/*��OP_Add��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  VdbeComment((v, "LIMIT+OFFSET"));/*��vdbe�д���"LIMIT+OFFSET"�ַ�������Ӧ�Ͼ�ĵ�ַ*/
		  addr1 = sqlite3VdbeAddOp1(v, OP_IfPos, iLimit);/*��OP_IfPos��������vdbe��Ȼ�󷵻���������ĵ�ַ����ֵ��addr1*/
		  sqlite3VdbeAddOp2(v, OP_Integer, -1, iOffset+1);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  sqlite3VdbeJumpHere(v, addr1);/*���е�ַ����addr1*/
		}
	  }
	}

	#ifndef SQLITE_OMIT_COMPOUND_SELECT
	/*
	** Return the appropriate collating sequence for the iCol-th column of
	** the result set for the compound-select statement "p".  Return NULL if
	** the column has no default collating sequence.
	**
	** The collating sequence for the compound select is taken from the
	** left-most term of the select that has a collating sequence.
	*/
	/*
	** Ϊ���ϲ�ѯָ��p�Ľ�����ĵ�i�з���һ��ǡ������������
	**
	** ���Ϊ���ϲ�ѯ����������ѡ����������������е����
	*/
	static CollSeq *multiSelectCollSeq(Parse *pParse, Select *p, int iCol){
	  CollSeq *pRet;/*����һ����������*/
	  if( p->pPrior ){/*��������Ȳ���*/
		pRet = multiSelectCollSeq(pParse, p->pPrior, iCol);/*��ʹ�����Ȳ��ҵģ�����һ����������*/
	  }else{
		pRet = 0;/*��������Ϊ0*/
	  }
	  assert( iCol>=0 );/*����ϵ㣬�ж��к��Ƿ���ڵ���0*/
	  if( pRet==0 && iCol<p->pEList->nExpr ){/*���û�����Ȳ��Һ��к�С�ڱ��ʽ�ĸ������ڷ�Χ�ڣ�*/
		pRet = sqlite3ExprCollSeq(pParse, p->pEList->a[iCol].pExpr);/*����һ��Ĭ�ϵ���������*/
	  }
	  return pRet;/*���ݸ���������������յ���������*/
	}
	#endif /* SQLITE_OMIT_COMPOUND_SELECT */

	/* Forward reference */
	static int multiSelectOrderBy(
	  Parse *pParse,        /* Parsing context */ /*����������*/
	  Select *p,            /* The right-most of SELECTs to be coded */ /*SELECT�������ҵ�select����*/
	  SelectDest *pDest     /* What to do with query results */ /*select�����*/
	);


	#ifndef SQLITE_OMIT_COMPOUND_SELECT
	/*
	** This routine is called to process a compound query form from
	** two or more separate queries using UNION, UNION ALL, EXCEPT, or
	** INTERSECT
	**
	** "p" points to the right-most of the two queries.  the query on the
	** left is p->pPrior.  The left query could also be a compound query
	** in which case this routine will be called recursively. 
	**  
	** The results of the total query are to be written into a destination
	** of type eDest with parameter iParm.
	** 
	** Example 1:  Consider a three-way compound SQL statement.
	** 
	**     SELECT a FROM t1 UNION SELECT b FROM t2 UNION SELECT c FROM t3
	**
	** This statement is parsed up as follows:
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
	**
	** Notice that because of the way SQLite parses compound SELECTs, the
	** individual selects always group from left to right.
	*/
	/*
	** ������򱻵���ȥ����һ�������������ֿ���ʹ��UNION UNION ALL EXCEPT INTERSE�Ĳ�ѯ��ɸ��ϲ�ѯ��
	**
	** pָ�����ұߵ������Ӳ�ѯ�����Ĳ�ѯ���ȼ��ߣ���ߵĲ�ѯҲ����һ�����ϲ�ѯ���ݹ�����������
	**
	** ���յĲ�ѯ���ʹ��IParma����д��eDest�С�
	**
	** ����1����һ����·����SQL���
	** 	SELECT a FROM t1 UNION SELECT b FROM t2 UNION SELECT c FROM t3
	**  �����������������·�����
	** SELECT c FROM t3
	**      |
	**      `----->  SELECT b FROM t2
	**                |
	**                `------>  SELECT a FROM t1
	** ��ͷ����������Ȳ�ѯ��˳������������p����t3�Ĳ�ѯ�����Ȳ�ѯt2. p�Ĳ���������TK_UNION
	**
	** ע��SQLite�Ľ������ϲ�ѯ�ķ�ʽ��������صĲ�ѯ�����Ǵ����ҡ�
	*/
	static int multiSelect(
	  Parse *pParse,        /* Parsing context *//*����������*/
	  Select *p,            /* The right-most of SELECTs to be coded *//*���ұߵ�SELECT�ṹ��*/
	  SelectDest *pDest     /* What to do with query results *//*������δ�������*/
	){
	  int rc = SQLITE_OK;   /* Success code from a subroutine *//*�ӳ���ɹ�ִ��*/
	  Select *pPrior;       /* Another SELECT immediately to our left *//*һ������ߵ�����������ѯ��SELECT*/
	  Vdbe *v;              /* Generate code to this VDBE *//*����VDBE*/
	  SelectDest dest;      /* Alternative data destination *//*��ѡ���Ŀ�����ݼ�*/
	  Select *pDelete = 0;  /* Chain of simple selects to delete *//*�򵥵���ʽɾ��*/
	  sqlite3 *db;          /* Database connection *//*���ݿ�����*/
	#ifndef SQLITE_OMIT_EXPLAIN
	  int iSub1;            /* EQP id of left-hand query *//*���ѯ��id*/
	  int iSub2;            /* EQP id of right-hand query *//*�Ҳ�ѯ��id*/
	#endif

	  /* Make sure there is no ORDER BY or LIMIT clause on prior SELECTs.  Only
	  ** the last (right-most) SELECT in the series may have an ORDER BY or LIMIT.
	  */
	  /*ȷ��û��ORDERBY��LIMIT�Ӿ�������SELECT�С�ֻ�����ұߵ�SELECT������ORDERBY��LIMIT*/
	  assert( p && p->pPrior );  /* Calling function guarantees this much *//*����ϵ��ж��Ƿ�������SELECT*/
	  db = pParse->db;/*���﷨�������е����ݿ����Ӹ�ֵ��db*/
	  pPrior = p->pPrior;/*��SELECT�����ȼ���ֵ��pPrior*/
	  assert( pPrior->pRightmost!=pPrior );/*����ϵ㣬�ж����ȼ������ұ߱��ʽ���ǵ�ǰ����SELECT*/
	  assert( pPrior->pRightmost==p->pRightmost );/*����ϵ㣬�ж����ȼ������ұ߱��ʽ�ǵ�ǰ����SELECT�����ұ�SELECT*/
	  dest = *pDest;/*����δ��������Ľṹ�帳ֵ��dest*/
	  if( pPrior->pOrderBy ){/*�������SELECT�к�ORDERBY*/
		sqlite3ErrorMsg(pParse,"ORDER BY clause should come after %s not before",
		  selectOpName(p->op));/*���﷨�����������һ��������Ϣ*/
		rc = 1;/*��ִ����ϱ��*/
		goto multi_select_end;/*����multi_select_end��ִ�н�����*/
	  }
	  if( pPrior->pLimit ){/*�������SELECT��LIMIT*/
		sqlite3ErrorMsg(pParse,"LIMIT clause should come after %s not before",
		  selectOpName(p->op));/*���﷨�����������һ��������Ϣ*/
		rc = 1;/*��ִ����ϱ��*/
		goto multi_select_end;/*����multi_select_end��ִ�н�����*/
	  }

	  v = sqlite3GetVdbe(pParse);/*�����﷨����������һ���������ݿ�����*/
	  assert( v!=0 );  /* The VDBE already created by calling function */
		/*�������ݿ������Ѿ�����������*/
	  /* Create the destination temporary table if necessary
	  */
	  /*�����Ҫ������һ����ʱĿ���*/
	  if( dest.eDest==SRT_EphemTab ){/*������ݼ���SRT_EphemTab*/
		assert( p->pEList );/*����ϵ㣬�ж��Ƿ��н����*/
		sqlite3VdbeAddOp2(v, OP_OpenEphemeral, dest.iSDParm, p->pEList->nExpr);/*��OP_OpenEphemeral��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		sqlite3VdbeChangeP5(v, BTREE_UNORDERED);/*��BTREE_UNORDERED����Ϊ������ʹ�õ�ֵ*/
		dest.eDest = SRT_Table;/*�����ݼ�ΪSRT_Table*/
	  }

	  /* Make sure all SELECTs in the statement have the same number of elements
	  ** in their result sets.
	  *//*ȷ������������SELECT����ͬ�����Ԫ�صĸ���*/
	  assert( p->pEList && pPrior->pEList );/*����ϵ㣬SELECT����������SELECT�Ľ�����н���*/
	  if( p->pEList->nExpr!=pPrior->pEList->nExpr ){/*��������ı��ʽ������SELECT�Ľ����ı��ʽ��ͬ*/
		if( p->selFlags & SF_Values ){/*���selFlags�ı��ΪSF_Values*/
		  sqlite3ErrorMsg(pParse, "all VALUES must have the same number of terms");/*���﷨�����������һ��������Ϣ*/
		}else{
		  sqlite3ErrorMsg(pParse, "SELECTs to the left and right of %s"
			" do not have the same number of result columns", selectOpName(p->op));/*�������﷨�����������һ����������Ϣ����%SΪp->op*/
		}
		rc = 1;/*��ִ����ϱ��*/
		goto multi_select_end;/*����multi_select_end��ִ�н�����*/
	  }

	  /* Compound SELECTs that have an ORDER BY clause are handled separately.
	  *//*����ORDERBY�ĸ���SELECTҪ�ֿ�����*/
	  if( p->pOrderBy ){/*�������ORDERBY*/
		return multiSelectOrderBy(pParse, p, pDest);/*�ݹ�ʹ�ñ�����ʵ�ڵݹ�p*/
	  }

	  /* Generate code for the left and right SELECT statements.
	  *//*Ϊ��SELECT����SELECT������д����*/
	  switch( p->op ){/*�ж�SELECT�в�����*/
		case TK_ALL: {
		  int addr = 0;/*�����ַ*/
		  int nLimit;/*����limit*/
		  assert( !pPrior->pLimit );/*����ϵ㣬�������SELECT�в���Limit*/
		  pPrior->pLimit = p->pLimit;/*��SELECT��Limit��ֵ������SELECT��Limit*/
		  pPrior->pOffset = p->pOffset;/*��SELECT��Offset��ֵ������SELECT��Offset*/
		  explainSetInteger(iSub1, pParse->iNextSelectId);/*����һ��SELECT��ID��ֵ��iSub1*/
		  rc = sqlite3Select(pParse, pPrior, &dest);/*��ִ����ϱ��,����sqlite3Select����dest���ɵ�SELECT*/
		  p->pLimit = 0;/*SELECT��û��LIMIT*/
		  p->pOffset = 0;/*SELECTû��OFFSET*/
		  if( rc ){/*����н������*/
			goto multi_select_end;/*����multi_select_end��ִ�н�����*/
		  }
		  p->pPrior = 0;/*�������Ȳ�ѯSELECTΪ0*/
		  p->iLimit = pPrior->iLimit;/*���Ȳ�ѯSELECT�е�LIMIT��ֵ��SELECT��LIMIT*/
		  p->iOffset = pPrior->iOffset;/*���Ȳ�ѯSELECT�е�OFFSET��ֵ��SELECT��OFFSET*/
		  if( p->iLimit ){/*���SELECT�к�LIMIT*/
			addr = sqlite3VdbeAddOp1(v, OP_IfZero, p->iLimit);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ���Ƹ�addr*/
			VdbeComment((v, "Jump ahead if LIMIT reached"));/*���е�ַ����addr1*/
		  }
		  explainSetInteger(iSub2, pParse->iNextSelectId);/*����һ��SELECT��ID��ֵ��iSub2*/
		  rc = sqlite3Select(pParse, p, &dest);/*��ִ����ϱ��,����sqlite3Select����dest���ɵ�SELECT*/
		  testcase( rc!=SQLITE_OK );/*���Խ������ΪSQLITE_OK��ִ����ϣ�*/
		  pDelete = p->pPrior;/*��SELECT�����Ȳ�ѯ�ṹ�帳ֵ��pDeleteɾ��SELECT�ṹ*/
		  p->pPrior = pPrior;/*������SELECT��ֵ��SELECT�����Ȳ�ѯ��ѯ����*/
		  p->nSelectRow += pPrior->nSelectRow;/*���ұߵĽṹ���������SELECT���ټ��ϵ�ǰ��*/
		  if( pPrior->pLimit
		   && sqlite3ExprIsInteger(pPrior->pLimit, &nLimit
		   && p->nSelectRow > (double)nLimit /*������Ȳ�ѯSELECT��LIMIT�������ұߵ�SELECT�ṹ��Ҳ��LIMIT*/
		  ){
			p->nSelectRow = (double)nLimit;/*SELECT������ΪnLimit(û��ͨ����Ϊʲô��)*/
		  }
		  if( addr ){
			sqlite3VdbeJumpHere(v, addr);/*���addr���ڣ����е�ַ����addr*/
		  }
		  break;
		}
		case TK_EXCEPT:
		case TK_UNION: {
		  int unionTab;    /* Cursor number of the temporary table holding result *//*��ʱ��������α��*/
		  u8 op = 0;       /* One of the SRT_ operations to apply to self *//*Ӧ���������һ��SRT_ operations����*/
		  int priorOp;     /* The SRT_ operation to apply to prior selects *//*Ӧ��������SELECT��SRT_ operations����*/
		  Expr *pLimit, *pOffset; /* Saved values of p->nLimit and p->nOffset *//**/
		  int addr;/*����һ����ַ*/
		  SelectDest uniondest;/*����һ���������ݼ��Ĳ���*/

		  testcase( p->op==TK_EXCEPT );/*����SELECT������TK_EXCEPT*/
		  testcase( p->op==TK_UNION );/*����SELECT������TK_UNION*/
		  priorOp = SRT_Union;/*�����Ȳ�ѯ��SELECT������ΪSRT_Union*/
		  if( dest.eDest==priorOp && ALWAYS(!p->pLimit &&!p->pOffset) ){/*���ݼ��в����������Ȳ�ѯ��SELECT��������ͬ�����Ҳ���LIMIT OFFSET*/
			/* We can reuse a temporary table generated by a SELECT to our
			** right.
			*//*���������ұߵ�SELECT����ʹ��һ����ʱ��*/
			assert( p->pRightmost!=p );  /* Can only happen for leftward elements
										 ** of a 3-way or more compound *//*����������3·�����SELECT*/
			assert( p->pLimit==0 );      /* Not allowed on leftward elements *//*�����������*/
			assert( p->pOffset==0 );     /* Not allowed on leftward elements *//*�����������*/
			unionTab = dest.iSDParm;/*�������������ݼ��ķ������Ը�ֵ��unionTab*/
		  }else{
			/* We will need to create our own temporary table to hold the
			** intermediate results.
			*//*������Ҫ����һ�������Լ�����ʱ�����м���*/
			unionTab = pParse->nTab++;/*���﷨�������еı�����ֵ��unionTab*/
			assert( p->pOrderBy==0 );/*����ϵ��ж��Ƿ���ORDERBY*/
			addr = sqlite3VdbeAddOp2(v, OP_OpenEphemeral, unionTab, 0);/*��OP_OpenEphemeral��������vdbe��Ȼ�󷵻���������ĵ�ַ�ٸ�ֵ��addr*/
			assert( p->addrOpenEphm[0] == -1 );/*����ϵ㣬û���´�·��*/
			p->addrOpenEphm[0] = addr;/*����ַ��ֵ����·���ĵ�һ��Ԫ��*/
			p->pRightmost->selFlags |= SF_UsesEphemeral;/*selFlagsλ��SF_UsesEphemeral*/
			assert( p->pEList );/*����ϵ㣬�ж�SELECT�н����*/
		  }

		  /* Code the SELECT statements to our left
		  *//*Ϊ��ߵ�������дSELECT����*/
		  assert( !pPrior->pOrderBy );/*����ϵ㣬�ж����Ȳ�ѯ��SELECT��Orderby*/
		  sqlite3SelectDestInit(&uniondest, priorOp, unionTab);/*��ʼ��SelectDest�ṹ��*/
		  explainSetInteger(iSub1, pParse->iNextSelectId);/*���﷨��������pParse����һ��SelectID��ֵ��ISub1*/
		  rc = sqlite3Select(pParse, pPrior, &uniondest);/*����Select�ṹ�壬Ȼ��ѷ��ر�Ǹ�rc���Ƿ�ɹ�*/
		  if( rc ){/*���rc������Ǵ���*/
			goto multi_select_end;/*����multi_select_end��ִ�н�����*/
		  }

		  /* Code the current SELECT statement
		  *//*���ɵ�ǰ��SELECT���*/
		  if( p->op==TK_EXCEPT ){/*����ṹ��Ĳ������ǡ�TK_EXCEPT��*/
			op = SRT_Except;/*���ò�����ΪSRT_Except*/
		  }else{
			assert( p->op==TK_UNION );/*����ϵ㣬�ж�SELECT�ṹ��Ĳ������Ƿ�Ϊ��TK_UNION��*/
			op = SRT_Union;/*���ò�����ΪSRT_Union*/
		  }
		  p->pPrior = 0;/*��SELECT�ṹ���е����Ȳ�ѯ��0*/
		  pLimit = p->pLimit;/*����ǰSELECT������������Ϊ��ǰSELECT�ṹ�壬�ݹ�Ĺ��̣������������ˡ�*/
		  p->pLimit = 0;/*��SELECT�ṹ����limit��0*/
		  pOffset = p->pOffset;/*��SELECT�ṹ���е�Offsetƫ�������Ƹ�����pOffset*/
		  p->pOffset = 0;/*�ٽ�SELECT�ṹ���е�Offsetƫ������0����һ��ָ���Ѿ�������Offset��ֵ��*/
		  uniondest.eDest = op;/*��op��������ֵ�����������еġ�����ʽ������*/
		  explainSetInteger(iSub2, pParse->iNextSelectId);/*���﷨��������pParse����һ��SelectID��ֵ��iSub2*/
		  rc = sqlite3Select(pParse, p, &uniondest);/*����Select�ṹ�壬Ȼ��ѷ��ر�Ǹ�rc���Ƿ�ɹ�*/
		  testcase( rc!=SQLITE_OK );/*���Խ����ǵ�ֵ�Ƿ�Ϊ���*/
		  /* Query flattening in sqlite3Select() might refill p->pOrderBy.
		  ** Be sure to delete p->pOrderBy, therefore, to avoid a memory leak. */
		  /* sqlite3Select()�в�ѯ���ܻ���װ��ORDERBYָ�ȷ��ɾ��ORDERBY���Է��ڴ�й¶��*/
		  sqlite3ExprListDelete(db, p->pOrderBy);/*ɾ�����ݿ������е�ORDERBY*/
		  pDelete = p->pPrior;/*��SELECT�����Ȳ�ѯ�ṹ�帳ֵ��pDeleteɾ��SELECT�ṹ*/
		  p->pPrior = pPrior;/*�������е�pPrior����ֵ��ֵ��SELECT�ṹ���е���������*/
		  p->pOrderBy = 0;/*��SELECT�е�ORDERBY��0��ʹ��Ĭ�Ϸ�������*/
		  if( p->op==TK_UNION ) p->nSelectRow += pPrior->nSelectRow;
		  /*���SELECT�еĲ���ΪTK_UNION������ǰpPrior���Ȳ����еĲ����м���SELECT�Ĳ����С�������Ϊ���ڵ��е�ֵ�����ӽڵ������Ե�ֵ����Ҫ�鵽���У����ƻ�ֵ��ƫ����*/
		  sqlite3ExprDelete(db, p->pLimit);/*ɾ�����ݿ������еģ�SELECT�е�Limit���ʽ*/
		  p->pLimit = pLimit;/*�ٽ���ǰlimit�еı�����ֵ��SELECT��limit�������ղ���������0�ˡ�*/
		  p->pOffset = pOffset;/*�ٽ���ǰOffset�еı�����ֵ��SELECT��Offset�������ղ�������Ҳ��0�ˡ�*/
		  p->iLimit = 0;/*�ٽ�SELECT��limit������0*/
		  p->iOffset = 0;/*��SELECT��Offset������0*/

		  /* Convert the data in the temporary table into whatever form
		  ** it is that we currently need.
		  */
		  /*����ʱ���е�����ת��Ϊ��ǰ��Ҫ�ĸ�ʽ*/
		  assert( unionTab==dest.iSDParm || dest.eDest!=priorOp );/*����ϵ㣬���������Ƿ�����α�Ż����������Ƿ���ڲ�������*/
		  if( dest.eDest!=priorOp ){/*�������������ڲ�����*/
			int iCont, iBreak, iStart;
			assert( p->pEList );/*����ϵ㣬�жϽ�����Ƿ�Ϊ��*/
			if( dest.eDest==SRT_Output ){/*��������ķ�ʽΪ���*/
			  Select *pFirst = p;/*������SELECT�е�һ��SELECT����Ϊp*/
			  while( pFirst->pPrior ) pFirst = pFirst->pPrior;/*�ݹ飬�ҵ�����SELECT�е�һ��SELECET������ֵ��pFrist*/
			  generateColumnNames(pParse, 0, pFirst->pEList);/*�����﷨�����������ź��б��ʽ�б���������*/
			}
			iBreak = sqlite3VdbeMakeLabel(v);/*ΪVDBE����һ����ǩ������ֵ��ֵ��iBreak��ע�������ǩ�Ǹ���ֵ�����еı�ǩ���Ǹ�ֵ��0�Ļ��ͷ��ط����ڴ����*/
			iCont = sqlite3VdbeMakeLabel(v);/*ΪVDBE����һ����ǩ������ֵ��ֵ��iCont��*/
			computeLimitRegisters(pParse, p, iBreak);/*���ݷ��صĵ�ר�õ�ַ�������limit�����*/
			sqlite3VdbeAddOp2(v, OP_Rewind, unionTab, iBreak);/*��OP_Rewind��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
			iStart = sqlite3VdbeCurrentAddr(v);/*��vdbe����һ��ָ���ַ��ֵ��iStart*/
			selectInnerLoop(pParse, p, p->pEList, unionTab, p->pEList->nExpr,
							0, -1, &dest, iCont, iBreak);/*���ݱ��ʽp->pEList��p->pEList->nExpr����������*/
			sqlite3VdbeResolveLabel(v, iCont);/*����ICont����ʱICont��һ����ǩ������Ϊ��һ��������ĵ�ַ*/
			sqlite3VdbeAddOp2(v, OP_Next, unionTab, iStart);/*��OP_Next��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
			sqlite3VdbeResolveLabel(v, iBreak);/*����iBreak����ʱiBreakҲ��һ����ǩ��������ǵ�ַ������Ϊ��һ��������ĵ�ַ*/
			sqlite3VdbeAddOp2(v, OP_Close, unionTab, 0);/*��OP_Close��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  }
		  break;
		}
		default: assert( p->op==TK_INTERSECT ); {/*����һ���ϵ㣬�ж�SELECT�еĲ����Ƿ�ΪTK_INTERSECT*/
		  int tab1, tab2;/*����������*/
		  int iCont, iBreak, iStart;/*����3����ǩ�����ַ*/
		  Expr *pLimit, *pOffset;/*�����������ʽ*/
		  int addr;/*����һ����ַ��ǩ*/
		  SelectDest intersectdest;/*����һ������Ľ���Ľṹ��*/
		  int r1;/**/

		  /* INTERSECT is different from the others since it requires
		  ** two temporary tables.  Hence it has its own case.  Begin
		  ** by allocating the tables we will need.
		  */
		  /* ���벻ͬ��������������Ҫ��������ʱ����������Լ���������佫��Ҫ�ı�*/
		  tab1 = pParse->nTab++;/*���﷨�������б�ĸ�����һ��ֵ��tab1*/
		  tab2 = pParse->nTab++;/*���﷨�������б�ĸ�����һ��ֵ��tab2*/
		  assert( p->pOrderBy==0 );/*����ϵ㣬�ж�SELECT���Ƿ���ORDERBY�Ӿ�*/

		  addr = sqlite3VdbeAddOp2(v, OP_OpenEphemeral, tab1, 0);/*��OP_OpenEphemeral��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  assert( p->addrOpenEphm[0] == -1 );/*����ϵ㣬�ж�SELECT�ṹ��OP_OpenEphem���������һ��Ԫ���Ƿ�Ϊ-1*/
		  p->addrOpenEphm[0] = addr;/*������addr�ĵ�ַ��ֵ��OP_OpenEphem��������ĵ�һ��Ԫ��*/
		  p->pRightmost->selFlags |= SF_UsesEphemeral;/*SELECT����������selFlags��SF_UsesEphemeral��ʹ����ʱĿ¼�ڵ㣩λ���ڸ�ֵ��selFlags*/
		  assert( p->pEList );/*����ϵ㣬�ж�SELECT�Ľ�����Ƿ�Ϊ��*/

		  /* Code the SELECTs to our left into temporary table "tab1".
		  */
		  /*��д��ߵ�SELECT����ʱ��"tab1"*/
		  sqlite3SelectDestInit(&intersectdest, SRT_Union, tab1);/*��ʼ����������*/
		  explainSetInteger(iSub1, pParse->iNextSelectId);/*���﷨��������pParse����һ��SelectID��ֵ��iSub1*/
		  rc = sqlite3Select(pParse, pPrior, &intersectdest);/*�����﷨��������SELECT�ṹ��ʹ��������ṹ������SELECT�﷨����������һ���������*/
		  if( rc ){/*���������Ǵ���*/
			goto multi_select_end;/*����multi_select_end��ִ�н�����*/
		  }

		  /* Code the current SELECT into temporary table "tab2"
		  */
		  /*��д��ǰSELECT����ʱ��"tab2"*/
		  addr = sqlite3VdbeAddOp2(v, OP_OpenEphemeral, tab2, 0);/*��OP_OpenEphemeral��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  assert( p->addrOpenEphm[1] == -1 );/*����ϵ㣬�ж�SELECT�ṹ��OP_OpenEphem��������ڶ���Ԫ���Ƿ�Ϊ-1*/
		  p->addrOpenEphm[1] = addr;/*������addr�ĵ�ַ��ֵ��OP_OpenEphem��������ĵڶ���Ԫ��*/
		  p->pPrior = 0;/*��SELECT�ṹ�������Ȳ�ѯSELECT��Ϊ0*/
		  pLimit = p->pLimit;/*�ٽ���ǰlimit�еı�����ֵ��SELECT��limit����*/
		  p->pLimit = 0;/*�ٽ���ǰSELECT��limit����ֵ0*/
		  pOffset = p->pOffset;/*�ٽ���ǰOffset�еı�����ֵ��SELECT��Offset����*/
		  p->pOffset = 0;/*�ٽ���ǰSELECT��Offset����ֵ0*/
		  intersectdest.iSDParm = tab2;/*�����������ṹ���д����Ĳ�����Ϊtab2*/
		  explainSetInteger(iSub2, pParse->iNextSelectId);/*���﷨��������pParse����һ��SelectID��ֵ��iSub2*/
		  rc = sqlite3Select(pParse, p, &intersectdest);/*�����﷨��������SELECT�ṹ��ʹ��������ṹ������SELECT�﷨����������һ���������*/
		  testcase( rc!=SQLITE_OK );/*���Խ�������Ƿ�ΪSQLITE_OK*/
		  pDelete = p->pPrior;/*��SELECT�ṹ�������Ȳ�ѯ��SELECT��ֵ����ɾ����SELECT*/
		  p->pPrior = pPrior;/*����ǰ���Ȳ�ѯ��SELECT������ֵ��SELECT�����Ȳ�ѯ��pPrior����*/
		  if( p->nSelectRow>pPrior->nSelectRow ) p->nSelectRow = pPrior->nSelectRow;/*���SELECT�еĲ����д������Ȳ�ѯSELECT�Ĳ����У������Ȳ�ѯSELECT�Ĳ����и�ֵ��SELECT*/
		  sqlite3ExprDelete(db, p->pLimit);/*ɾ�����ݿ������е�limit���ʽ*/
		  p->pLimit = pLimit;/*����ǰlimit�еı�����ֵ��SELECT��limit����/
		  p->pOffset = pOffset;/*����ǰOffset�еı�����ֵ��SELECT��Offset����*/

		  /* Generate code to take the intersection of the two temporary tables.
		  ** tables.
		  */
		  /* Ϊ������ʱ��Ľ���ڽ��б���*/
		  assert( p->pEList );/*����ϵ㣬�ж�SELECT�н�����Ƿ�Ϊ��*/
		  if( dest.eDest==SRT_Output ){/*������������Ĵ���ʽΪSRT_Output�������*/
			Select *pFirst = p;/*�����ṹ��pFirst����ֵΪp*/
			while( pFirst->pPrior ) pFirst = pFirst->pPrior;/*һֱ�ݹ飬�ҵ�Ҷ�ӽڵ��ϵ������ȵ�SELECT*/
			generateColumnNames(pParse, 0, pFirst->pEList);/*��������*/
		  }
		  iBreak = sqlite3VdbeMakeLabel(v);/*ΪVDBE����һ����ǩ������ֵ��ֵ��iBreak.*/
		  iCont = sqlite3VdbeMakeLabel(v);/*ΪVDBE����һ����ǩ������ֵ��ֵ��iCont*/
		  computeLimitRegisters(pParse, p, iBreak);/*���ݷ��صĵ�ר�õ�ַ�������limit�����*/
		  sqlite3VdbeAddOp2(v, OP_Rewind, tab1, iBreak);/*��OP_Rewind��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  r1 = sqlite3GetTempReg(pParse);/*�õ��﷨����������ʱ�Ĵ����е�ֵ��r1*/
		  iStart = sqlite3VdbeAddOp2(v, OP_RowKey, tab1, r1);/*��OP_Rewind��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  sqlite3VdbeAddOp4Int(v, OP_NotFound, tab2, iCont, r1, 0);/*��OP_NotFound��δ�鵽��������ֵ����������Ȼ�����������������������*/
		  sqlite3ReleaseTempReg(pParse, r1);/*�ͷ��﷨����������ʱ�Ĵ���r1��ֵ*/
		  selectInnerLoop(pParse, p, p->pEList, tab1, p->pEList->nExpr,
						  0, -1, &dest, iCont, iBreak);/*���ݱ��ʽp->pEList��p->pEList->nExpr����������*/
		  sqlite3VdbeResolveLabel(v, iCont);/*����ICont��iCont��һ����ǩ������Ϊ��һ��������ĵ�ַ*/
		  sqlite3VdbeAddOp2(v, OP_Next, tab1, iStart);/*��OP_Next��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  sqlite3VdbeResolveLabel(v, iBreak);/*����ICont��iBreak��һ����ǩ������Ϊ��һ��������ĵ�ַ*/
		  sqlite3VdbeAddOp2(v, OP_Close, tab2, 0);/*��OP_Close��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  sqlite3VdbeAddOp2(v, OP_Close, tab1, 0);/*��OP_Close��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  break;
		}
	  }

	  explainComposite(pParse, p->op, iSub1, iSub2, p->op!=TK_ALL);
		/*���û��ִ��select����ô�ͻ����"COMPOSITE SUBQUERIES iSub1 and iSub2 (op)"*/
	  /* Compute collating sequences used by 
	  ** temporary tables needed to implement the compound select.
	  ** Attach the KeyInfo structure to all temporary tables.
	  **
	  ** This section is run by the right-most SELECT statement only.
	  ** SELECT statements to the left always skip this part.  The right-most
	  ** SELECT might also skip this part if it has no ORDER BY clause and
	  ** no temp tables are required.
	  */
	  /* ��ʱ��ʹ�ü�����������ȥʵ�ָ��ϲ�ѯ�������е���ʱ������ؼ���Ϣ�ṹ.
	  ** 
	  ** �ⲿ�ֽ��������ұߵ�SELECT��䣬��߻������˲��֡����ұߵ�SELECTҲ������������������û��ORDERBY��û��Ҫ�����ʱ��*/
	  if( p->selFlags & SF_UsesEphemeral ){/*���SELECT��selFlagsΪSF_UsesEphemeral*/
		int i;                        /* Loop counter *//*ѭ��������*/
		KeyInfo *pKeyInfo;            /* Collating sequence for the result set *//*���������������*/
		Select *pLoop;                /* For looping through SELECT statements *//*ͨ��SELECT������ѭ��*/
		CollSeq **apColl;             /* For looping through pKeyInfo->aColl[] *//*ͨ���ؼ���Ϣ�ṹ����aColl[]����ѭ��*/
		int nCol;                     /* Number of columns in result set *//*�����������*/

		assert( p->pRightmost==p );/*����ϵ��ж����ұߵ�SELECT�Ƿ�Ϊp*/
		nCol = p->pEList->nExpr;/*���ʽ�б��б��ʽ�ĸ�����ֵ��nCol*/
		pKeyInfo = sqlite3DbMallocZero(db,
						   sizeof(*pKeyInfo)+nCol*(sizeof(CollSeq*) + 1));/*���䲢����ڴ棬�����СΪ�ڶ����������ڴ�*/
		if( !pKeyInfo ){/*����ؼ���Ϣ�ṹ�岻����*/
		  rc = SQLITE_NOMEM;/*�������ΪSQLITE_NOMEM*/
		  goto multi_select_end;/*����multi_select_end��ִ�н�����*/
		}

		pKeyInfo->enc = ENC(db);/*���ùؼ��ṹ��Ļ�������Ϊdb*/
		pKeyInfo->nField = (u16)nCol;/*���ùؼ��ṹ������СΪ�����������*/

		for(i=0, apColl=pKeyInfo->aColl; i<nCol; i++, apColl++){/*���ݽ����������ѭ��*/
		  *apColl = multiSelectCollSeq(pParse, p, i);/*Ϊ������һ����������*/
		  if( 0==*apColl ){/*�����������Ϊ��*/
			*apColl = db->pDfltColl;/*��ôʹ��Ĭ�ϵ���������*/
		  }
		}

		for(pLoop=p; pLoop; pLoop=pLoop->pPrior){/*��������SELECT*/
		  for(i=0; i<2; i++){
			int addr = pLoop->addrOpenEphm[i];/*��OP_OpenEphem���������Ԫ�ظ�ֵ��addr��ַ*/
			if( addr<0 ){/*���addrС��0*/
			  /* If [0] is unused then [1] is also unused.  So we can
			  ** always safely abort as soon as the first unused slot is found */
			  /*���С��0˵������һ������ʹ�á�����ֻҪ��һ���Ҳ����ĵ�ַ�����Ǿ��ܰ�ȫ��ֹ��*/
			  assert( pLoop->addrOpenEphm[1]<0 );/*����ϵ㣬�ж�ѭ����OP_OpenEphem����������С��0�����*/
			  break;
			}
			sqlite3VdbeChangeP2(v, addr, nCol);/*��nCol��ֵ���õ������*/
			sqlite3VdbeChangeP4(v, addr, (char*)pKeyInfo, P4_KEYINFO);/*����ؼ���Ϣ�ṹ�е�ֵַ����addr,��P4_KEYINFO��ֵ���õ������*/
			pLoop->addrOpenEphm[i] = -1;/*��OP_OpenEphem���������Ԫ����Ϊ-1*/
		  }
		}
		sqlite3DbFree(db, pKeyInfo);/*�ͷ����ݿ������й����ؼ��ֽṹ����ڴ�*/
	  }

	multi_select_end:/*���ĵ�goto�����������*/
	  pDest->iSdst = dest.iSdst;/*д�뵽��ַ�Ĵ�����ֵ��ֵ�����������Ĵ洢��ַ�Ĵ�����ֵ*/
	  pDest->nSdst = dest.nSdst;/*�ѷ����ַ�Ĵ�����������ֵ�����������Ĵ洢��ַ�Ĵ���������*/
	  sqlite3SelectDelete(db, pDelete);/*�ͷ�Select�ṹ��pDelete*/
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
	/*
	** ΪЭ������дһ������ӳ���ʵ��SELECT��䡣
	**
	** �������������pIn->iSdst����Ž���Ļ�ַ�Ĵ�������pIn->iSdst���б������pDest�����������ṹ�壩���������
	**
	** regReturn�Ǵ洢�ӳ����ַ�ļĴ���������
	**
	** ���regPrev > 0,��ô���ǵ�һ�������Ĵ���,���м�¼��ǰ���������ݡ�
	**
	** �����SELECT�ṹ�����ҵ�limit����������iBreak.
	*/
	static int generateOutputSubroutine(
	  Parse *pParse,          /* Parsing context *//*����������*/
	  Select *p,              /* The SELECT statement *//*����SELECT�ṹ��*/
	  SelectDest *pIn,        /* Coroutine supplying data *//*�����������ݼ��ṹ�壬Эͬ�����ṩ����*/
	  SelectDest *pDest,      /* Where to send the data *//*�������ݵģ����͵�����������*/
	  int regReturn,          /* The return address register *//*���ص�ַ�Ĵ���*/
	  int regPrev,            /* Previous result register.  No uniqueness if 0 *//*��ǰ�Ľ���Ĵ�����û��Ψһ��Ϊ0*/
	  KeyInfo *pKeyInfo,      /* For comparing with previous entry *//*����ǰ����Ŀ�Ƚ�*/
	  int p4type,             /* The p4 type for pKeyInfo *//*pKeyInfo��p4����*/
	  int iBreak              /* Jump here if we hit the LIMIT *//*�������LIMIT�����˴�*/
	){
	  Vdbe *v = pParse->pVdbe;/*���﷨��������VDBE���Ը�ֵ��VDBE����v*/
	  int iContinue;
	  int addr;

	  addr = sqlite3VdbeCurrentAddr(v);/*��vdbe����һ��ָ���ַ��ֵ��addr*/
	  iContinue = sqlite3VdbeMakeLabel(v);/*ΪVDBE����һ����ǩ������ֵ��ֵ��iContinue*/

	  /* Suppress duplicates for UNION, EXCEPT, and INTERSECT 
	  *//*����UNION,EXCEPT,INTERSECT����*/
	  if( regPrev ){/*�����ǰ�ļĴ�������*/
		int j1, j2;
		j1 = sqlite3VdbeAddOp1(v, OP_IfNot, regPrev);/*��OP_IfNot��������vdbe��Ȼ�󷵻���������ĵ�ַ���Ƹ�addr*/
		j2 = sqlite3VdbeAddOp4(v, OP_Compare, pIn->iSdst, regPrev+1, pIn->nSdst,
								  (char*)pKeyInfo, p4type);/*�ڲ��������p4type,������һ��ָ��*/
		sqlite3VdbeAddOp3(v, OP_Jump, j2+2, iContinue, j2+2);/*��OP_Jump��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		sqlite3VdbeJumpHere(v, j1);/*���j1���ڣ����е�ַ������ǰ��ַ*/
		sqlite3ExprCodeCopy(pParse, pIn->iSdst, regPrev+1, pIn->nSdst);/*copy����pIn->iSdst�����ݵ�pIn->nSdst����ʵ���ǼĴ���*/
		sqlite3VdbeAddOp2(v, OP_Integer, 1, regPrev);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  }
	  if( pParse->db->mallocFailed ) return 0;/*��������ڴ����ʧ�ܣ���ֱ�ӷ���0*/

	  /* Suppress the first OFFSET entries if there is an OFFSET clause
	  *//*�����OFFSET�Ӿ䣬����OFFSET������*/
	  codeOffset(v, p, iContinue);/*����ƫ����������iContinue����һ��ѭ��Ҫ�����ĵ�ַ*/

	  switch( pDest->eDest ){
		/* Store the result as data using a unique key.
		*//*ʹ��һ��Ψһ���洢���*/
		case SRT_Table:
		case SRT_EphemTab: {
		  int r1 = sqlite3GetTempReg(pParse);/*�õ��﷨����������ʱ�Ĵ����е�ֵ��r1*/
		  int r2 = sqlite3GetTempReg(pParse);/*�õ��﷨����������ʱ�Ĵ����е�ֵ��r2*/
		  testcase( pDest->eDest==SRT_Table );/*���Դ��������еĴ������ķ�ʽ�Ƿ�ΪSRT_Table*/
		  testcase( pDest->eDest==SRT_EphemTab );/*���Դ��������еĴ������ķ�ʽ�Ƿ�ΪSRT_EphemTab*/
		  sqlite3VdbeAddOp3(v, OP_MakeRecord, pIn->iSdst, pIn->nSdst, r1);/*��OP_MakeRecord��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  sqlite3VdbeAddOp2(v, OP_NewRowid, pDest->iSDParm, r2);/*��OP_NewRowid��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  sqlite3VdbeAddOp3(v, OP_Insert, pDest->iSDParm, r1, r2);/*��OP_Insert��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  sqlite3VdbeChangeP5(v, OPFLAG_APPEND);/*��OPFLAG_APPEND����Ϊ������ʹ�õ�ֵ*/
		  sqlite3ReleaseTempReg(pParse, r2);/*�ͷ��﷨����������ʱ�Ĵ���r2��ֵ*/
		  sqlite3ReleaseTempReg(pParse, r1);/*�ͷ��﷨����������ʱ�Ĵ���r1��ֵ*/
		  break;
		}

	#ifndef SQLITE_OMIT_SUBQUERY
		/* If we are creating a set for an "expr IN (SELECT ...)" construct,
		** then there should be a single item on the stack.  Write this 
		** item into the set table with bogus data.
		*//*���������һ��"expr IN (SELECT ...)"�ṹ���ϣ�Ȼ����һ����������Ŀ�ڶ�ջ�ϡ��������Ŀд��������ݵı���*/
		case SRT_Set: {
		  int r1;
		  assert( pIn->nSdst==1 );/*���Эͬ�������ݼ��ļĴ�����Ϊ1*/
		  p->affinity = 
			 sqlite3CompareAffinity(p->pEList->a[0].pExpr, pDest->affSdst);/*�����ʽ�б��еı��ʽpExpr�봦�������е�affSdst�׺��ԱȽϣ������رȽϺ������*/
		  r1 = sqlite3GetTempReg(pParse);/*�õ��﷨����������ʱ�Ĵ����е�ֵ��r1*/
		  sqlite3VdbeAddOp4(v, OP_MakeRecord, pIn->iSdst, 1, r1, &p->affinity, 1);/*���һ��OP_MakeRecord����������������ֵ��Ϊһ��ָ��*/
		  sqlite3ExprCacheAffinityChange(pParse, pIn->iSdst, 1);/*�����﷨��pParse���Ĵ����е��׺�������*/
		  sqlite3VdbeAddOp2(v, OP_IdxInsert, pDest->iSDParm, r1);/*��OP_IdxInsert��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  sqlite3ReleaseTempReg(pParse, r1);/*�ͷ��﷨����������ʱ�Ĵ���r1��ֵ*/
		  break;
		}

	#if 0  /* Never occurs on an ORDER BY query */
		/* If any row exist in the result set, record that fact and abort.
		*//*�κ�һ�д����ڽ�����У���Ҫ����ʵ��ֵ���ж�*/
		case SRT_Exists: {
		  sqlite3VdbeAddOp2(v, OP_Integer, 1, pDest->iSDParm);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  /* The LIMIT clause will terminate the loop for us *//*Limit�Ӿ佫����ֹѭ��*/
		  break;
		}
	#endif

		/* If this is a scalar select that is part of an expression, then
		** store the results in the appropriate memory cell and break out of the scan loop.
		** of the scan loop.
		*//*���һ������ѡ����һ�����ʽ��һ���֣�������洢��һ�����ʵ��ڴ��У����Ҵ���ɨ��ѭ��*/
		case SRT_Mem: {
		  assert( pIn->nSdst==1 );/*����ϵ㣬�ж�Эͬ�������ݼ��ļĴ������Ƿ�Ϊ1*/
		  sqlite3ExprCodeMove(pParse, pIn->iSdst, pDest->iSDParm, 1);/*�ͷżĴ����е����ݣ����ּĴ��������ݼ�ʱ����*/
		  /* The LIMIT clause will jump out of the loop for us *//*Limit�Ӿ佫���ѭ��������*/
		  break;
		}
	#endif /* #ifndef SQLITE_OMIT_SUBQUERY */

		/* The results are stored in a sequence of registers
		** starting at pDest->iSdst.  Then the co-routine yields.
		*//*����洢��һ�������ļĴ����У���ʼλ��Ϊд�����ݵĻ�ַ�Ĵ����ĵ�ַ��Ȼ��ִ��Эͬ����顣*/
		case SRT_Coroutine: {
		  if( pDest->iSdst==0 ){/*�����ַ�Ĵ���Ϊ0*/
			pDest->iSdst = sqlite3GetTempRange(pParse, pIn->nSdst);/*����һ�������ļĴ���������Ϊ�ѷ���Ĵ����ĸ���*/
			pDest->nSdst = pIn->nSdst;/*������ļĴ����ĸ�����¼��nSdst��*/
		  }
		  sqlite3ExprCodeMove(pParse, pIn->iSdst, pDest->iSdst, pDest->nSdst);/*�ͷżĴ����е����ݣ����ּĴ��������ݼ�ʱ����*/
		  sqlite3VdbeAddOp1(v, OP_Yield, pDest->iSDParm);/*��OP_Yield��������vdbe��Ȼ�󷵻���������ĵ�ַ����*/
		  break;
		}

		/* If none of the above, then the result destination must be
		** SRT_Output.  This routine is never called with any other 
		** destination other than the ones handled above or SRT_Output.
		**
		** For SRT_Output, results are stored in a sequence of registers.  
		** Then the OP_ResultRow opcode is used to cause sqlite3_step() to
		** return the next row of result.
		*//*
		**���û�����ϵĲ��֣���ô������������������������ʹ���������κδ�����Ŀ��������֮�ϡ�
		**
		**���ڽ�������˵������洢��һ���Ĵ��������С�Ȼ��OP_ResultRow��������sqlite3_step()�У�������һ�н����
		*/
		default: {
		  assert( pDest->eDest==SRT_Output );/*����ϵ㣬�жϴ��������д���ʽ�Ƿ�ΪSRT_Output�������*/
		  sqlite3VdbeAddOp2(v, OP_ResultRow, pIn->iSdst, pIn->nSdst);/*��OP_ResultRow��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  sqlite3ExprCacheAffinityChange(pParse, pIn->iSdst, pIn->nSdst);/*�����﷨��pParse���Ĵ����е�iSdst��nSdst�׺�������*/
		  break;
		}
	  }

	  /* Jump to the end of the loop if the LIMIT is reached.
	  *//*�����Limit�����ƴ���ֱ��������β��*/
	  if( p->iLimit ){/**/
		sqlite3VdbeAddOp3(v, OP_IfZero, p->iLimit, iBreak, -1);/*��OP_IfZero��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  }

	  /* Generate the subroutine return
	  *//*�����ӳ��򷵻�*/
	  sqlite3VdbeResolveLabel(v, iContinue);/*����iContinue��iContinue��һ����ǩ������Ϊ��һ��������ĵ�ַ*/
	  sqlite3VdbeAddOp1(v, OP_Return, regReturn);/*��OP_Return��������vdbe��Ȼ�󷵻���������ĵ�ַ����*/

	  return addr;/*���������ַ*/
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
	/*
	**����ORDERBY�Ӿ�ʱ���ṩһ���ɹ�ѡ��ĸ��ϲ�ѯ��
	**���Ǽ���һ�����µĲ�ѯ��ʽ��
	**  <selectA>  <operator>  <selectB>  ORDER BY <orderbylist>
	**��������UNION ALL, UNION, EXCEPT, ��INTERSECT���е�һ��������뷨�ǽ�����ORDERBY��<selectA>��<selectB>
	**��ΪЭͬ���򡣲�������Эͬ���򣬺ϳɽ�������������������Эͬ�������滹��7���ӳ���
	** outA: ��selectA���������ƶ������ϲ�ѯ����С�
	** outB: ��selectB���������ƶ������ϲ�ѯ�����(ֻ������UNION�� UNION ALL.  EXCEPT �� INSERTSECT ).�������ֻ��B�в��е��С�
	** AltB: �������������Эͬ���򣬲���A<Bʱ�����á�
	** AeqB: �������������Эͬ���򣬲���A=Bʱ�����á�
	** AgtB: �������������Эͬ���򣬲���A>Bʱ�����á�
	** EofA: ���þ�A�е����ݣ�ʵ����ѭ��A��ѭ����ϣ���
	** EofB: ���þ�B�е����ݣ�ʵ����ѭ��B��ѭ����ϣ���
	** ʵ�����µ�5���ӳ�����Ҫ�Ĳ����������µı���У�
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
	** ��AltB, AeqB, ��AgtB �ӳ����У�һ��EOF��������ʾ����������A�У�Ȼ��nextA����ֱ������EofA
	** һ��EOF��������ʾ����������B�У�Ȼ��nextB����ֱ������EofB
	** ��EofA��EofB���棬һ��EOF��������Ŀ�ϻ��߽�����һ��nextXһ����ת��SELECT����Ľ�β��
	** 
	**��ȥ���ظ��Ĳ��֡�
	** ������ӳ����д���ɾ����UNION, EXCEPT, ��INTERSECT�ĸ������������������ʱ��ɾ��������
	** ��regPrev�Ĵ����б�������������ݡ�������洢��ֵ�������ֵ���бȽϣ��Ƚ��Ƿ���ͬ��
	**
	** �����ʵ�ֵķ�������ʵ������Эͬ������߸��ӳ���Ȼ���ڽ�β����ʹ�ÿ����߼������£�
	**              goto Init
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
	**     End: ...��
	** �����ܵ���AltB, AeqB, AgtB, EofA �� EofB�ӳ��򣬵���ʵ��������û�з��ص�ʱ�����ǲ��ܵ������ǡ�����˳��ִ�У�
	** ֱ�����е����ݶ��Ѿ�ʹ�ù��ˣ�ע�ͽкľ�����EofA �� EofB��ѭ��������"end"��ǩ���� AltB, AeqB,and AgtB
	**  ����L2�� ����EofA �� EofB������һ��֮�С�
	**
	*/
	#ifndef SQLITE_OMIT_COMPOUND_SELECT
	static int multiSelectOrderBy(
	  Parse *pParse,        /* Parsing context *//*����������*/
	  Select *p,            /* The right-most of SELECTs to be coded *//*��д���ұߵ�SELECT*/
	  SelectDest *pDest     /* What to do with query results *//*���������ṹ��*/
	){
	  int i, j;             /* Loop counters *//*ѭ��������*/
	  Select *pPrior;       /* Another SELECT immediately to our left *//*����ߵ�������ѯ*/
	  Vdbe *v;              /* Generate code to this VDBE *//*ΪVEBE���ɴ���*/
	  SelectDest destA;     /* Destination for coroutine A *//*Эͬ����A����������*/
	  SelectDest destB;     /* Destination for coroutine B *//*Эͬ����B����������*/
	  int regAddrA;         /* Address register for select-A coroutine *//*select-A����Ļ�ַ�Ĵ���*/
	  int regEofA;          /* Flag to indicate when select-A is complete *//*select-A�������ɱ��*/
	  int regAddrB;         /* Address register for select-B coroutine *//*select-B����Ļ�ַ�Ĵ���*/
	  int regEofB;          /* Flag to indicate when select-B is complete *//*select-B�������ɱ��*/
	  int addrSelectA;      /* Address of the select-A coroutine *//*select-A����ĵ�ַ*/
	  int addrSelectB;      /* Address of the select-B coroutine *//*select-B����ĵ�ַ*/
	  int regOutA;          /* Address register for the output-A subroutine *//*output-A����Ļ�ַ�Ĵ���*/
	  int regOutB;          /* Address register for the output-B subroutine *//*output-B����Ļ�ַ�Ĵ���*/
	  int addrOutA;         /* Address of the output-A subroutine *//*select-A����ĵ�ַ*/
	  int addrOutB = 0;     /* Address of the output-B subroutine *//*select-B����ĵ�ַ*/
	  int addrEofA;         /* Address of the select-A-exhausted subroutine *//*select-A-exhausted����ĵ�ַ*/
	  int addrEofB;         /* Address of the select-B-exhausted subroutine *//*select-B-exhausted����ĵ�ַ*/
	  int addrAltB;         /* Address of the A<B subroutine *//*A<B�ӳ���ĵ�ַ*/
	  int addrAeqB;         /* Address of the A==B subroutine *//*A=B�ӳ���ĵ�ַ*/
	  int addrAgtB;         /* Address of the A>B subroutine *//*A>B�ӳ���ĵ�ַ*/
	  int regLimitA;        /* Limit register for select-A *//*select-A��Limit�Ĵ��������Limitֵ�ģ�*/
	  int regLimitB;        /* Limit register for select-B *//*select-B��Limit�Ĵ��������Limitֵ�ģ�*/
	  int regPrev;          /* A range of registers to hold previous output *//*һϵ�б���֮ǰ������ļĴ����������Ƚ��ظ���*/
	  int savedLimit;       /* Saved value of p->iLimit *//*����p->iLimit��ֵ*/
	  int savedOffset;      /* Saved value of p->iOffset *//*����p->iOffset��ֵ*/
	  int labelCmpr;        /* Label for the start of the merge algorithm *//*�ϲ��㷨�Ŀ�ʼ��ǩ*/
	  int labelEnd;         /* Label for the end of the overall SELECT stmt *//*ȫ��SELECT�����ı�ǩ*/
	  int j1;               /* Jump instructions that get retargetted *//*�õ���ָ�����תָ��*/
	  int op;               /* One of TK_ALL, TK_UNION, TK_EXCEPT, TK_INTERSECT *//*TK_ALL, TK_UNION, TK_EXCEPT, TK_INTERSECT�е�һ������*/
	  KeyInfo *pKeyDup = 0; /* Comparison information for duplicate removal *//*�ȽϺ�ɾ���ظ���Ϣ*/
	  KeyInfo *pKeyMerge;   /* Comparison information for merging rows *//*�ȽϺ󣬺ϲ���*/
	  sqlite3 *db;          /* Database connection *//*���ݿ�����*/
	  ExprList *pOrderBy;   /* The ORDER BY clause *//*ORDERBY�Ӿ�*/
	  int nOrderBy;         /* Number of terms in the ORDER BY clause *//*ORDERBY�Ӿ�ĸ���*/
	  int *aPermute;        /* Mapping from ORDER BY terms to result set columns *//*����ORDERBY��������е�ӳ��*/
	#ifndef SQLITE_OMIT_EXPLAIN
	  int iSub1;            /* EQP id of left-hand query *//*���ѯ��ΨһID*/
	  int iSub2;            /* EQP id of right-hand query *//*�Ҳ�ѯ��ΨһID*/
	#endif

	  assert( p->pOrderBy!=0 );/*����ϵ㣬�жϺ���ORDERBY*/
	  assert( pKeyDup==0 ); /* "Managed" code needs this.  Ticket #3382. *//*Managed���йܣ�������Ҫ��Ticket���ӱ�ǩ��#3382*/
	  db = pParse->db;/*����һ�����ݿ�����*/
	  v = pParse->pVdbe;/*����һ��VEBE*/
	  /*���ڿ�����assert!!����������true�����������������������FALSE���׳��쳣*/
	  assert( v!=0 );       /* Already thrown the error if VDBE alloc failed *//*���VDBE���������׳��쳣*/
	  labelEnd = sqlite3VdbeMakeLabel(v);/*ΪVDBE����һ����ǩ������ֵ��ֵ��labelEnd��ȫ��SELECT�����ı�ǩ��*/
	  labelCmpr = sqlite3VdbeMakeLabel(v);/*ΪVDBE����һ����ǩ������ֵ��ֵ��labelCmpr���ϲ��㷨�Ŀ�ʼ��ǩ��*/


	  /* Patch up the ORDER BY clause
	  *//*����ORDER BY�Ӿ�*/
	  op = p->op; /*��SELECT�в�������ֵ������op*/ 
	  pPrior = p->pPrior;/*SELECT�ṹ������Ȳ�ѯ��SELECT��ֵ������pPrior*/
	  assert( pPrior->pOrderBy==0 );/*����ϵ㣬������Ȳ�ѯ��SELECTû��OrderBy�����׳�������Ϣ*/
	  pOrderBy = p->pOrderBy;/*��SELECT�е�ORDERBY�Ӿ丳ֵ������pOrderBy*/
	  assert( pOrderBy );/*����ϵ㣬���û��ORDERBY�����׳�������Ϣ*/
	  nOrderBy = pOrderBy->nExpr;/*pOrderBy�ı��ʽ������ֵ��nOrderBy*/

	  /* For operators other than UNION ALL we have to make sure that
	  ** the ORDER BY clause covers every term of the result set.  Add
	  ** terms to the ORDER BY clause as necessary.
	  *//*����UNION ALL��ORDERBY�����ܸ��ǵ��������ÿһ�������У�������ORDERBY�Ӿ��Ǳ����*/
	  if( op!=TK_ALL ){/*�������������TK_ALL*/
		for(i=1; db->mallocFailed==0 && i<=p->pEList->nExpr; i++){/*���ұ������ʽ*/
		  struct ExprList_item *pItem;/*����һ����ʾ�б���*/
		  for(j=0, pItem=pOrderBy->a; j<nOrderBy; j++, pItem++){/*������Ŀ*/
			assert( pItem->iOrderByCol>0 );/*����ϵ㣬���û��ORDERBY�����׳�������Ϣ*/
			if( pItem->iOrderByCol==i ) break;/*���ORDERBY��ĿΪi���㷵�أ�������ǰ��ѭ����*/
		  }
		  if( j==nOrderBy ){/*�����ʱ�ϸ�ѭ��������j��ֵΪORDERBE�ĸ���*/
			Expr *pNew = sqlite3Expr(db, TK_INTEGER, 0);/*��TK_INTEGER�����ʽpNew��sqlite3Expr������ֵת��Ϊ���ʽ*/
			if( pNew==0 ) return SQLITE_NOMEM;/*������ʽΪ0��˵��û�в�����ֱ�ӷ���SQLITE_NOMEM*/
			pNew->flags |= EP_IntValue;/*flags��ǩλ��EP_IntValue�ٸ�ֵ��flags���*/
			pNew->u.iValue = i;/*����ǰi��ֵ����ֵ�����ʽ�е�iֵ*/
			pOrderBy = sqlite3ExprListAppend(pParse, pOrderBy, pNew);/*��pNew���ʽ��ӵ�pParse�е�pOrderBy����*/
			if( pOrderBy ) pOrderBy->a[nOrderBy++].iOrderByCol = (u16)i;/*���pOrderBy���ڣ���iOrderByCol��ֵ��Ϊi*/
		  }
		}
	  }

	  /* Compute the comparison permutation and keyinfo that is used with
	  ** the permutation used to determine if the next
	  ** row of results comes from selectA or selectB.  Also add explicit
	  ** collations to the ORDER BY clause terms so that when the subqueries
	  ** to the right and the left are evaluated, they use the correct
	  ** collation.
	  *//*�����м���Ƚ����к͹ؼ���Ϣ�ṹ������һ�н������selectA����selectB.
	  ** ��Ӷ�ORDERBY�Ӿ����ʽ���У��Ա������ұߺ���ߵ��ӳ�������ʹ����ȷ�����С�*/
	  aPermute = sqlite3DbMallocRaw(db, sizeof(int)*nOrderBy);/*���䲢����ڴ棬�ٷ����СΪORDERBY�ĸ���*/
	  if( aPermute ){/*���aPermute��Ϊ0*/
		struct ExprList_item *pItem;/*����һ�����ʽ�б���*/
		for(i=0, pItem=pOrderBy->a; i<nOrderBy; i++, pItem++){/*�������ʽ�б���*/
		  assert( pItem->iOrderByCol>0  && pItem->iOrderByCol<=p->pEList->nExpr );/*����ϵ㣬�жϸ�����ORDERBY��������0����С�ڵ��ڱ��ʽ�ĸ���*/
		  aPermute[i] = pItem->iOrderByCol - 1;/*��������ORDERBY��һ���±��0��ʼ���浽aPermute������*/
		}
		pKeyMerge =
		  sqlite3DbMallocRaw(db, sizeof(*pKeyMerge)+nOrderBy*(sizeof(CollSeq*)+1));/*���䲢����ڴ棬�ٷ����СΪ�ڶ�������*/
		if( pKeyMerge ){/*����Ͼ������pKeyMerge��Ϊ��*/
		  pKeyMerge->aSortOrder = (u8*)&pKeyMerge->aColl[nOrderBy];/*�ؼ���Ϣ�ṹ���д洢��������ķ�����ֵΪ�ؼ���Ϣ��������aColl����洢�ķ�ʽ*/
		  pKeyMerge->nField = (u16)nOrderBy;/*ORDERBY�ĸ�������Ϊ�ؼ���Ϣ�ṹ���aColl[]����Ŀ����*/
		  pKeyMerge->enc = ENC(db);/*�趨���뷽ʽ��SQLITE_UTF*��*/
		  for(i=0; i<nOrderBy; i++){/*����ORDERBY�ĸ�������*/
			CollSeq *pColl;/*����һ����������*/
			Expr *pTerm = pOrderBy->a[i].pExpr;/*��ORDERBY���ʽ�е�i�����ʽ��ֵ��pTerm*/
			if( pTerm->flags & EP_ExpCollate ){/*���flags��ֵΪEP_ExpCollate*/
			  pColl = pTerm->pColl;/*�����ʽpTerm���������и�ֵ������pColl*/
			}else{
			  pColl = multiSelectCollSeq(pParse, p, aPermute[i]);/*Ϊ������һ���������У�˵��aPermute�����Ǵ�ORDERBY��������е�ӳ�䣩*/
			  pTerm->flags |= EP_ExpCollate;/*flags��ǩλ��EP_ExpCollate�ٸ�ֵ��flags���*/
			  pTerm->pColl = pColl;/*���Ѿ��������ݵ�pColl��ֵ�����ʽ�е���������*/
			}
			pKeyMerge->aColl[i] = pColl;/*����ǰѭ�����������зŵ��ؼ���Ϣ�ṹ���������У�����Ӧ��i�£���ʱѭ��Ҳ��i,һһ��Ӧ*/
			pKeyMerge->aSortOrder[i] = pOrderBy->a[i].sortOrder;/*��pOrderBy���ʽ�б��еĵ�i-1�����򷽷��ŵ��ؼ���Ϣ�ṹ���е�����������*/
		  }
		}
	  }else{/*����Ͼ������pKeyMerge��Ϊ�գ�˵�����ʽΪ�գ��ڴ���û��*/
		pKeyMerge = 0;/*���ؼ���Ϣ�ṹ����Ϊ0*/
	  }

	  /* Reattach the ORDER BY clause to the query.
	  *//*�ٴν�ORDER BY�Ӿ�ӵ���ѯ��*/
	  p->pOrderBy = pOrderBy;/*����ǰ��pOrderBy���ʽ�б�ֵ��SELECT��pOrderBy����*/
	  pPrior->pOrderBy = sqlite3ExprListDup(pParse->db, pOrderBy, 0);/*���copy����pOrderBy�����Ȳ���SELECT��pOrderBy*/

	  /* Allocate a range of temporary registers and the KeyInfo needed
	  ** for the logic that removes duplicate result rows when the
	  ** operator is UNION, EXCEPT, or INTERSECT (but not UNION ALL).
	  *//*����������UNION, EXCEPT, or INTERSECT (����UNION ALL)ʱ������һ����ʱ�Ĵ������߼�����Ҫ�Ĺؼ���Ϣ�ṹ�壬ɾ���ظ��Ľ��*/
	  if( op==TK_ALL ){/*����������TK_ALL*/
		regPrev = 0;/*���洢��ǰ�����ݵļĴ�����0*/
	  }else{
		int nExpr = p->pEList->nExpr;/*�����ʽ�б��б��ʽ�ĸ�����ֵ��nExpr*/
		assert( nOrderBy>=nExpr || db->mallocFailed );/*����ϵ㣬���nOrderBy��OrderBy���������ڵ���nExpr�����ʽ������������ڴ�ʧ��*/
		regPrev = sqlite3GetTempRange(pParse, nExpr+1);/*����һ�������ļĴ���������Ϊ���ʽ�ĸ�����1*/
		sqlite3VdbeAddOp2(v, OP_Integer, 0, regPrev);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		pKeyDup = sqlite3DbMallocZero(db,
					  sizeof(*pKeyDup) + nExpr*(sizeof(CollSeq*)+1) );/*���䲢����ڴ棬�����ڴ��СΪ�ڶ�������*/
		if( pKeyDup ){/*�����һ������ڴ�Ĵ�С��Ϊ0*/
		  pKeyDup->aSortOrder = (u8*)&pKeyDup->aColl[nExpr];/*���������еı��ʽ��ֵ���ؼ���Ϣ�ṹ�е���������*/
		  pKeyDup->nField = (u16)nExpr;/*�����ʽ������ֵ��aColl�������ܵ���Ŀ��*/
		  pKeyDup->enc = ENC(db);/*�趨���뷽ʽ��SQLITE_UTF*��*/
		  for(i=0; i<nExpr; i++){/*�������ʽ*/
			pKeyDup->aColl[i] = multiSelectCollSeq(pParse, p, i);/*multiSelectCollSeq��������ķ�ʽ���ٸ�ֵ��aColl*/
			pKeyDup->aSortOrder[i] = 0;/*��������������֮�󣬽�����ʽ��Ϊ0*/
		  }
		}
	  }
	 
	  /* Separate the left and the right query from one another
	  *//*��������ߺ��ұ߲�ѯ*/
	  /*���µ����������ǣ�һ����SELECT�е�ORDER BY*/
	  p->pPrior = 0;/*��SELECT�е����Ȳ�ѯSELECTΪ��*/
	  sqlite3ResolveOrderGroupBy(pParse, p, p->pOrderBy, "ORDER");/*����GROUP��ORDERBY�����ORDER�ؼ��֡�����0��ʾ�ɹ���*/
	  if( pPrior->pPrior==0 ){/*���SELECT�е����Ȳ�ѯSELECTΪ��*/
		sqlite3ResolveOrderGroupBy(pParse, pPrior, pPrior->pOrderBy, "ORDER");/*����GROUP��ORDERBY�����ORDER�ؼ��֡�����0��ʾ�ɹ���*/
	  }

	  /* Compute the limit registers *//*����limit�Ĵ���*/
	  computeLimitRegisters(pParse, p, labelEnd);/*���ݷ��صĵ�ר�õ�ַ�������limit����򣬲��ҽ���*/
	  if( p->iLimit && op==TK_ALL ){/*���SELECT��limit��Ϊ�գ����Ҳ�����TK_ALL*/
		regLimitA = ++pParse->nMem;/*���﷨�������з����ڴ����++֮��ֵ��select-A��Limit�Ĵ���*/
		regLimitB = ++pParse->nMem;/*���﷨�������з����ڴ����++֮��ֵ��select-B��Limit�Ĵ���*/
		sqlite3VdbeAddOp2(v, OP_Copy, p->iOffset ? p->iOffset+1 : p->iLimit,
									  regLimitA);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ.���iOffsetΪ�գ���iOffset��һ����Ϊ�����ò���Ϊlimitֵ*/
		sqlite3VdbeAddOp2(v, OP_Copy, regLimitA, regLimitB);/*��OP_Copy��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  }else{
		regLimitA = regLimitB = 0;/*��������select-A��Limit�Ĵ�����select-A��Limit�Ĵ���ֵΪ0*/
	  }
	  sqlite3ExprDelete(db, p->pLimit);/*ɾ�����ݿ������е�limit���ʽ*/
	  p->pLimit = 0;/*��SELECT�ṹ���е�limitΪ0*/
	  sqlite3ExprDelete(db, p->pOffset);/*ɾ�����ݿ������е�offset���ʽ*/
	  p->pOffset = 0;/*��SELECT�ṹ���е�offsetΪ0*/

	  regAddrA = ++pParse->nMem;/*select-A����Ļ�ַ�Ĵ���Ϊ�﷨�������з����ڴ����Ŀ��1*/
	  regEofA = ++pParse->nMem;/*select-A�������ɱ��Ϊ�﷨�������з����ڴ����Ŀ��1*/
	  regAddrB = ++pParse->nMem;/*select-B����Ļ�ַ�Ĵ���Ϊ�﷨�������з����ڴ����Ŀ��1*/
	  regEofB = ++pParse->nMem;/*select-B�������ɱ��Ϊ�﷨�������з����ڴ����Ŀ��1*/
	  regOutA = ++pParse->nMem;/*output-A����Ļ�ַ�Ĵ���Ϊ�﷨�������з����ڴ����Ŀ��1*/
	  regOutB = ++pParse->nMem;/*output-B����Ļ�ַ�Ĵ���Ϊ�﷨�������з����ڴ����Ŀ��1*/
	  sqlite3SelectDestInit(&destA, SRT_Coroutine, regAddrA);/*��ʼ�����������洢��regAddrA�����ΪSRT_Coroutine*/
	  sqlite3SelectDestInit(&destB, SRT_Coroutine, regAddrB);/*��ʼ�����������洢��regAddrB�����ΪSRT_Coroutine*/

	  /* Jump past the various subroutines and coroutines to the main
	  ** merge loop
	  *//*���������ӳ����Эͬ���򣬽����������ϲ�ѭ��*/
	  j1 = sqlite3VdbeAddOp0(v, OP_Goto);/*�����ʹ��Goto���֮�󣬷��صĵ�ַ*/
	  addrSelectA = sqlite3VdbeCurrentAddr(v);/*��vdbe����һ��ָ���ַ��ֵ��addrSelectA(select-A����ĵ�ַ)*/


	  /* Generate a coroutine to evaluate the SELECT statement to the
	  ** left of the compound operator - the "A" select.
	  *//*����һ��Эͬ��������SELECT����߸��Ӳ�������"A"SELECT*/
	  VdbeNoopComment((v, "Begin coroutine for left SELECT"));/*�����ʾ��Ϣ*/
	  pPrior->iLimit = regLimitA;/*select-A��Limit�Ĵ����е�ֵ��ŵ����Ȳ�ѯSELECT�е�iLimit����*/
	  explainSetInteger(iSub1, pParse->iNextSelectId);/*���﷨��������pParse����һ��SelectID��ֵ��iSub1*/
	  sqlite3Select(pParse, pPrior, &destA);/*�����﷨��������SELECT�ṹ��ʹ��������ṹ������SELECT�﷨��*/
	  sqlite3VdbeAddOp2(v, OP_Integer, 1, regEofA);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  sqlite3VdbeAddOp1(v, OP_Yield, regAddrA);/*��OP_Yield��������vdbe��Ȼ�󷵻���������ĵ�ַ����*/
	  VdbeNoopComment((v, "End coroutine for left SELECT"));/*�����ʾ��Ϣ*/

	  /* Generate a coroutine to evaluate the SELECT statement on 
	  ** the right - the "B" select
	  *//*����һ��Эͬ��������SELECT����߸��Ӳ�������"B" SELECT**/
	  addrSelectB = sqlite3VdbeCurrentAddr(v);/*��vdbe����һ��ָ���ַ��ֵ��addrSelectB(select-B����ĵ�ַ)*/
	  VdbeNoopComment((v, "Begin coroutine for right SELECT"));/*�����ʾ��Ϣ*/
	  savedLimit = p->iLimit;/*��SELECT��iLimit����ֵ���浽����savedLimit��ֵ*/
	  savedOffset = p->iOffset;/*��SELECT��iOffset����ֵ���浽����savedOffset��ֵ*/
	  p->iLimit = regLimitB;/*��select-B��Limit�Ĵ�����ֵ��ŵ�SELECT��iLimit����ֵ*/
	  p->iOffset = 0;  /*��SELECT��iOffset����ֵ��Ϊ0*/
	  explainSetInteger(iSub2, pParse->iNextSelectId);/*���﷨��������pParse����һ��SelectID��ֵ��iSub2*/
	  sqlite3Select(pParse, p, &destB);/*�����﷨��������SELECT�ṹ��ʹ��������ṹ������SELECT�﷨��*/
	  p->iLimit = savedLimit;/*�ٽ�����savedLimit��ֵ��SELECT��iLimit����*/
	  p->iOffset = savedOffset;/*������savedOffset��ֵ��SELECT��iOffset����*/
	  sqlite3VdbeAddOp2(v, OP_Integer, 1, regEofB);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  sqlite3VdbeAddOp1(v, OP_Yield, regAddrB);/*��OP_Yield��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  VdbeNoopComment((v, "End coroutine for right SELECT"));/*�����ʾ��Ϣ*/

	  /* Generate a subroutine that outputs the current row of the A
	  ** select as the next output row of the compound select.
	  *//*����һ���ӳ������A select�Ľ����Ϊ��һ������ѡ��ѯ�������*/
	  VdbeNoopComment((v, "Output routine for A"));/*�����ʾ��Ϣ*/
	  addrOutA = generateOutputSubroutine(pParse,
					 p, &destA, pDest, regOutA,
					 regPrev, pKeyDup, P4_KEYINFO_HANDOFF, labelEnd);/*���A select�Ľ�����ݹ����generateOutputSubroutine����*/
	  
	  /* Generate a subroutine that outputs the current row of the B
	  ** select as the next output row of the compound select.
	  *//**/
	  if( op==TK_ALL || op==TK_UNION ){/*���������ΪTK_ALL��TK_UNION*/
		VdbeNoopComment((v, "Output routine for B"));/*�����ʾ��Ϣ*/
		addrOutB = generateOutputSubroutine(pParse,
					 p, &destB, pDest, regOutB,
					 regPrev, pKeyDup, P4_KEYINFO_STATIC, labelEnd);/*���B select�Ľ�����ݹ����generateOutputSubroutine����*/
	  }

	  /* Generate a subroutine to run when the results from select A
	  ** are exhausted and only data in select B remains.
	  *//*��A selectѭ���ֻ꣬��select B������ʱ�����и��ӳ���*/
	  VdbeNoopComment((v, "eof-A subroutine"));/*�����ʾ��Ϣ*/
	  if( op==TK_EXCEPT || op==TK_INTERSECT ){/*���������ΪTK_EXCEPT��TK_INTERSECT*/
		addrEofA = sqlite3VdbeAddOp2(v, OP_Goto, 0, labelEnd);/*��OP_Goto��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  }else{  
		addrEofA = sqlite3VdbeAddOp2(v, OP_If, regEofB, labelEnd);/*��OP_If��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		sqlite3VdbeAddOp2(v, OP_Gosub, regOutB, addrOutB);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		sqlite3VdbeAddOp1(v, OP_Yield, regAddrB);/*��OP_Yield��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		sqlite3VdbeAddOp2(v, OP_Goto, 0, addrEofA);/*��OP_Goto��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		p->nSelectRow += pPrior->nSelectRow;/*���SELECT�еĲ���ΪTK_UNION������ǰpPrior���Ȳ����еĲ����м���SELECT�Ĳ����С�ע���ǲ��ǻ�ַ����ƫ�ơ�*/
	  }

	  /* Generate a subroutine to run when the results from select B
	  ** are exhausted and only data in select A remains.
	  *//*��select Bѭ���ֻ꣬��select A������ʱ�����и��ӳ���*/
	  if( op==TK_INTERSECT ){/*���������ΪTK_INTERSECT*/
		addrEofB = addrEofA;/*��select-A-exhausted����ĵ�ַ��select A������ַ����ֵ��select-B-exhausted����ĵ�ַ*/
		if( p->nSelectRow > pPrior->nSelectRow ) p->nSelectRow = pPrior->nSelectRow;/*���SELECT�еĲ����д������Ȳ�ѯSELECT�Ĳ����У������Ȳ�ѯSELECT�Ĳ����и�ֵ��SELECT*/
	  }else{  
		VdbeNoopComment((v, "eof-B subroutine"));/*�����ʾ��Ϣ*/
		addrEofB = sqlite3VdbeAddOp2(v, OP_If, regEofA, labelEnd);/*��OP_If��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		sqlite3VdbeAddOp2(v, OP_Gosub, regOutA, addrOutA);/*��OP_Gosub��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		sqlite3VdbeAddOp1(v, OP_Yield, regAddrA);/*��OP_Yield��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		sqlite3VdbeAddOp2(v, OP_Goto, 0, addrEofB);/*��OP_Goto��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  }

	  /* Generate code to handle the case of A<B
	  *//*���ɴ���������A < Bʱ�����*/
	  VdbeNoopComment((v, "A-lt-B subroutine"));/*�����ʾ��Ϣ*/
	  addrAltB = sqlite3VdbeAddOp2(v, OP_Gosub, regOutA, addrOutA);/*��OP_Gosub��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  sqlite3VdbeAddOp1(v, OP_Yield, regAddrA);/*��OP_Yield��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  sqlite3VdbeAddOp2(v, OP_If, regEofA, addrEofA);/*��OP_If��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  sqlite3VdbeAddOp2(v, OP_Goto, 0, labelCmpr);/*��OP_Goto��������vdbe��Ȼ�󷵻���������ĵ�ַ*/

	  /* Generate code to handle the case of A==B
	  *//*���ɴ���������A = Bʱ�����*/
	  if( op==TK_ALL ){/*���������ΪTK_ALL*/
		addrAeqB = addrAltB;/*����������������ݿ����淵�صĵ�ַ��ֵ��A=B�ӳ���ĵ�ַ*/
	  }else if( op==TK_INTERSECT ){/*���������ΪTK_INTERSECT*/
		addrAeqB = addrAltB;/*�������ݿ����淵�صĵ�ַ��ֵ��A=B�ӳ���ĵ�ַ*/
		addrAltB++;/*���ص�ַ�Ӽ�*/
	  }else{
		VdbeNoopComment((v, "A-eq-B subroutine"));/*�����ʾ��Ϣ*/
		addrAeqB =
		sqlite3VdbeAddOp1(v, OP_Yield, regAddrA);/*��OP_Yield��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		sqlite3VdbeAddOp2(v, OP_If, regEofA, addrEofA);/*��OP_If,��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		sqlite3VdbeAddOp2(v, OP_Goto, 0, labelCmpr);/*��OP_Goto��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  }

	  /* Generate code to handle the case of A>B
	  *//*���ɴ���������A = Bʱ�����*/
	  VdbeNoopComment((v, "A-gt-B subroutine"));/*�����ʾ��Ϣ*/
	  addrAgtB = sqlite3VdbeCurrentAddr(v);/*��vdbe����һ��ָ���ַ��ֵ��addrAgtB(A>B�ӳ���ĵ�ַ)*/
	  if( op==TK_ALL || op==TK_UNION ){/*���������ΪTK_ALL��TK_UNION*/
		sqlite3VdbeAddOp2(v, OP_Gosub, regOutB, addrOutB);/*��OP_Gosub��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  }
	  sqlite3VdbeAddOp1(v, OP_Yield, regAddrB);/*��OP_Yield��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  sqlite3VdbeAddOp2(v, OP_If, regEofB, addrEofB);/*��OP_If��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  sqlite3VdbeAddOp2(v, OP_Goto, 0, labelCmpr);/*��OP_Goto��������vdbe��Ȼ�󷵻���������ĵ�ַ*/

	  /* This code runs once to initialize everything.
	  *//*��δ�������һ�γ�ʼ�����еĶ���*/
	  sqlite3VdbeJumpHere(v, j1);/*���j1���ڣ����е�ַ������ǰ��ַ*/
	  sqlite3VdbeAddOp2(v, OP_Integer, 0, regEofA);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  sqlite3VdbeAddOp2(v, OP_Integer, 0, regEofB);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  sqlite3VdbeAddOp2(v, OP_Gosub, regAddrA, addrSelectA);/*��OP_Gosub��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  sqlite3VdbeAddOp2(v, OP_Gosub, regAddrB, addrSelectB);/*��OP_Gosub��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  sqlite3VdbeAddOp2(v, OP_If, regEofA, addrEofA);/*��OP_If��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  sqlite3VdbeAddOp2(v, OP_If, regEofB, addrEofB);/*��OP_If��������vdbe��Ȼ�󷵻���������ĵ�ַ*/

	  /* Implement the main merge loop
	  *//*ʵ�����ϲ�ѭ��*/
	  sqlite3VdbeResolveLabel(v, labelCmpr);/*����labelCmpr��labelCmpr��һ����ǩ������Ϊ��һ��������ĵ�ַ*/
	  sqlite3VdbeAddOp4(v, OP_Permutation, 0, 0, 0, (char*)aPermute, P4_INTARRAY);/*���һ��OP_Permutation����������������ֵ��Ϊһ��ָ��*/
	  sqlite3VdbeAddOp4(v, OP_Compare, destA.iSdst, destB.iSdst, nOrderBy,
							 (char*)pKeyMerge, P4_KEYINFO_HANDOFF);/*���һ��OP_Compare����������������ֵ��Ϊһ��ָ��*/
	  sqlite3VdbeAddOp3(v, OP_Jump, addrAltB, addrAeqB, addrAgtB);/*��OP_Jump��������vdbe��Ȼ�󷵻���������ĵ�ַ*/

	  /* Release temporary registers
	  *//*�ͷ���ʱ�Ĵ���*/
	  if( regPrev ){/**/
		sqlite3ReleaseTempRange(pParse, regPrev, nOrderBy+1);/*�ͷ�regPrev��������Ĵ�����������ORDERBY���ʽ�ĳ��ȼ�1*/
	  }

	  /* Jump to the this point in order to terminate the query.
	  *//*��ת����ָ��,��ֹ�����ѯ��*/
	  sqlite3VdbeResolveLabel(v, labelEnd);/*����labelEnd��labelEnd��һ����ǩ������Ϊ��һ��������ĵ�ַ*/

	  /* Set the number of output columns
	  *//*��������е�����*/
	  if( pDest->eDest==SRT_Output ){/*������������д���ʽ�Ƿ�ΪSRT_Output�������*/
		Select *pFirst = pPrior;/*�����ṹ��pFirst����ֵΪ��ǰSELECT�����Ȳ���pPrior*/
		while( pFirst->pPrior ) pFirst = pFirst->pPrior;/*һֱ�ݹ飬�ҵ�Ҷ�ӽڵ��ϵ������ȵ�SELECT*/
		generateColumnNames(pParse, 0, pFirst->pEList);/*��������*/
	  }

	  /* Reassembly the compound query so that it will be freed correctly 
	  ** by the calling function *//*��װ���ϲ�ѯ,�������������õĺ�����ȷ�ͷ�*/
	  if( p->pPrior ){/*���SELECT�����Ȳ���SELECT*/
		sqlite3SelectDelete(db, p->pPrior);/*�ͷ�Select�ṹ��pPrior*/
	  }
	  p->pPrior = pPrior;/*����ǰ��pPrior��ֵ��SELECT�е�pPrior*/

	  /*** TBD:  Insert subroutine calls to close cursors on incomplete 
	  **** subqueries ****//*�����ӳ�������α�ر�δ��ɵ��Ӳ�ѯ*/
	  explainComposite(pParse, p->op, iSub1, iSub2, 0);/*���û��ִ��select����ô�ͻ����"COMPOSITE SUBQUERIES iSub1 and iSub2 (op)*/
	  return SQLITE_OK;/*���ز�ѯ�������*/
	}
	#endif

	#if !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW)
	/* Forward Declarations *//*Ԥ������*/
	static void substExprList(sqlite3*, ExprList*, int, ExprList*);/*��ȡ���ʽ�б�*/
	static void substSelect(sqlite3*, Select *, int, ExprList *);/*��ȡSelect���*/

	/*
	** Scan through the expression pExpr.  Replace every reference to
	** a column in table number iTable with a copy of the iColumn-th
	** entry in pEList.  (But leave references to the ROWID column unchanged��
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
	** ͨ�����ʽpExprɨ�裬�ӱ��ʽ�б���copy��iColumn����Ŀ��Ȼ���滻ÿ���ڱ��ΪiTable�����漰���С�
	** ��ʣ�µĲο�����ROWID���䣩
	** 
	** ��������Ǳ�ƽ���ĳ����һ���֡�һ���ӳ���Ľ���������ʽ���壬����Ϊһ����SELECT��FROM�Ӿ����Ŀ��
	** ���VDBE�����FROM�Ӿ���α���һ��������������Ա��ʽ�����ı䣬����ֱ�������Ӳ�ѯ��Դ�������
	** �Ӳ�ѯ�Ľ������
	*/
	static Expr *substExpr(
	  sqlite3 *db,        /* Report malloc errors to this connection *//*����malloc��������*/
	  Expr *pExpr,        /* Expr in which substitution occurs *//*����ȡ���ı��ʽ*/
	  int iTable,         /* Table to be substituted *//*��ȡ���ı�*/
	  ExprList *pEList    /* Substitute expressions *//*����ı��ʽ�б�*/
	){
	  if( pExpr==0 ) return 0;/*������ʽΪ�գ�ֱ�ӷ���*/
	  if( pExpr->op==TK_COLUMN && pExpr->iTable==iTable ){/*������ʽ�Ĳ�����ΪTK_COLUMN�����ʽ���漰��ΪiTable*/
		if( pExpr->iColumn<0 ){/*������ʽ����С��0*/
		  pExpr->op = TK_NULL;/*����TK_NULL�������Ϊ�գ�*/
		}else{
		  Expr *pNew;/*����һ���µı��ʽ*/
		  assert( pEList!=0 && pExpr->iColumn<pEList->nExpr );/*����ϵ㣬�жϱ��ʽ��Ϊ�գ����ұ��ʽ������С�ڱ��ʽ�б��б��ʽ�ĸ����������׳��ϵ���Ϣ*/
		  assert( pExpr->pLeft==0 && pExpr->pRight==0 );/*����ϵ㣬�жϱ��ʽ��������Ϊ�գ�������ҲΪ��*/
		  pNew = sqlite3ExprDup(db, pEList->a[pExpr->iColumn].pExpr, 0);/*���copy���ʽ���ظ�pNew*/
		  if( pNew && pExpr->pColl ){/*���pNew��Ϊ�գ����ұ��ʽ���������в�Ϊ��*/
			pNew->pColl = pExpr->pColl;/*���±��ʽ���������е��ڵ�ǰȫ�ֱ��ʽ����������*/
		  }
		  sqlite3ExprDelete(db, pExpr);/*ɾ�����ݿ������е�pExpr���ʽ*/
		  pExpr = pNew;/*��pNew��ֵ����ǰȫ�ֱ��ʽ����������*/
		}
	  }else{
		pExpr->pLeft = substExpr(db, pExpr->pLeft, iTable, pEList);/*�ݹ�����������������ʽ����ڵ㸳ֵ�����ʽpExpr����ڵ�*/
		pExpr->pRight = substExpr(db, pExpr->pRight, iTable, pEList);/*�ݹ�����������������ʽ���ҽڵ㸳ֵ�����ʽpExpr���ҽڵ�*/
		if( ExprHasProperty(pExpr, EP_xIsSelect) ){/*�жϱ��ʽpExpr�Ƿ����EP_xIsSelect��ѯ*/
		  substSelect(db, pExpr->x.pSelect, iTable, pEList);/*��ȡpExpr->x.pSelect��Select���*/
		}else{
		  substExprList(db, pExpr->x.pList, iTable, pEList);/*��ȡpExpr->x.pList���ʽ�б�*/
		}
	  }
	  return pExpr;/*�������յı��ʽ*/
	}
	static void substExprList(
	  sqlite3 *db,         /* Report malloc errors here *//*��������ڴ�����������Ǹ����ݿ�����*/
	  ExprList *pList,     /* List to scan and in which to make substitutes *//*����������б�ɨ����ʽ�б�*/
	  int iTable,          /* Table to be substituted *//*������ı�*/
	  ExprList *pEList     /* Substitute values *//*�������ֵ*/
	){
	  int i;
	  if( pList==0 ) return;/*�������������б���ʽΪ��Ϊ�գ�ֱ�ӷ���*/
	  for(i=0; i<pList->nExpr; i++){/*�������ʽ*/
		pList->a[i].pExpr = substExpr(db, pList->a[i].pExpr, iTable, pEList);/*�ݹ�������������������ݿ����ӣ����ʽ�б���pList->a[i].pExpr���ʽ��ֵ�����ʽpList->a[i].pExpr*/
	  }
	}
	static void substSelect(
	  sqlite3 *db,         /* Report malloc errors here *//*��������ڴ�����������Ǹ����ݿ�����*/
	  Select *p,           /* SELECT statement in which to make substitutions *//*�����滻SELECT���*/
	  int iTable,          /* Table to be replaced *//*������ı�*/
	  ExprList *pEList     /* Substitute values *//*�������ֵ*/
	){
	  SrcList *pSrc;/*����SELECT�е�FROM�Ӿ�*/
	  struct SrcList_item *pItem;/*����һ��FROM�Ӿ���*/
	  int i;
	  if( !p ) return;/*���p��Ϊ�գ�ֱ�ӷ���*/
	  substExprList(db, p->pEList, iTable, pEList);/*��ȡp->pEList���ʽ�б�*/
	  substExprList(db, p->pGroupBy, iTable, pEList);/*��ȡp->pGroupBy���ʽ�б�*/
	  substExprList(db, p->pOrderBy, iTable, pEList);/*��ȡp->pGroupBy���ʽ�б�*/
	  p->pHaving = substExpr(db, p->pHaving, iTable, pEList);/*��ȡp->pHaving���ʽ��ֵ��SELECT��pHaving����*/
	  p->pWhere = substExpr(db, p->pWhere, iTable, pEList);/*��ȡp->pWhere���ʽ��ֵ��SELECT��pWhere����*/
	  substSelect(db, p->pPrior, iTable, pEList);/*����������substSelect����ȡp->pPrior��Select���*/
	  pSrc = p->pSrc;/*��SELECT��FROM�Ӿ丳ֵ����ǰȫ�ֱ�����pSrc*/
	  assert( pSrc );  /* Even for (SELECT 1) we have: pSrc!=0 but pSrc->nSrc==0 *//*����ʹ��SELECT 1�����ǵı�����Ϊ0�����ӱ��еı���Ϊ0*/
	  if( ALWAYS(pSrc) ){/*���pSrc����*/
		for(i=pSrc->nSrc, pItem=pSrc->a; i>0; i--, pItem++){/*�������ʽ��FROM�Ӿ��еı�*/
		  substSelect(db, pItem->pSelect, iTable, pEList);/*����������substSelect����ȡpItem->pSelect��Select���*/
		}
	  }
	}
	#endif /* !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW) */

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
	/* ���������ͼ��ƽ���ӳ�����Ϊһ�������Ż�����������˸ı䣬���򷵻�1�����û��������ƽ����������0.
	**
	** Ϊ������ƽ���ĸ���뿴���µĲ�ѯ��
	**   SELECT a FROM (SELECT x+y AS a FROM t1 WHERE z<100) WHERE a>5
	** �����ѯ��ִ���Ӳ�ѯ�����ѽ���ŵ���ʱ���У�Ȼ�����������Ĳ�ѯ�������ʱ���С���Ҫ�������β�����
	** ��һ��˵����Ϊ��ʱ��û�����������WHERE�Ӿ����ⲿ��ѯ�����Ż���
	** ���������ͼ��д��ѯ�����������SQL��䣬��Ϊ���µ�һ�������ı�ƽ����ѯ��
	**   SELECT a FROM (SELECT x+y AS a FROM t1 WHERE z<100) WHERE a>5
	** ������벻�����ˣ�������ͬ���Ľ������һ����ɨ�����ݡ���Ϊ�������ܴ����ڱ�t1�����ܱ���һ����ɵ�ɨ�衣
	**
	** ����������������еĹ��򣬾���ʵ�ֱ�ƽ����
	**��1���Ӳ�ѯ�����ѯ����ʹ�ù�ͬ�ľۼ�
	**��2���Ӳ�ѯ���þۼ��������ѯ��������
	**��3���Ӳ�ѯ�����������ӵ��Ҳ�����
	**��4���Ӳ�ѯ���á�DISTINCT��
	**��5������������4���ͣ�5��������DISTINCT�Ӳ�ѯ���Ӽ����ų�������Ż�������������4���Ѿ��ų������е�DISTINCT�Ӳ�ѯ
	**��6���Ӳ�ѯ����ʹ�þۼ����ⲿ��ѯ����ʹ��DISTINCT
	**��7���Ӳ�ѯ��һ��FROM�Ӿ䡣TODO���Ӳ�ѯû��һ��FROM�Ӿ䣬���һ������ı�sqlite_once����ֻ������һ����ֵ���С�
	**��8���Ӳ�ѯ��Ҫʹ��LIMIT���ⲿ��ѯ��ʹ�����Ӳ���
	**��9���Ӳ�ѯ��Ҫʹ��LIMIT���ⲿ��ѯ��ʹ�þۼ�
	**��10���Ӳ�ѯ��Ҫʹ�þۼ����ⲿ��ѯ��ʹ��LIMIT
	**��11���Ӳ�ѯ���ⲿ��ѯ����ͬʱʹ��ORDER BY�Ӿ�
	**��12��û��ʵ�֣����뵽����������3����
	**��13���Ӳ�ѯ���ⲿ��ѯ����ͬʱʹ��LIMIT�Ӿ�
	**��14���Ӳ�ѯ����ʹ��OFFSET�Ӿ�
	**��15���ⲿ��ѯ������һ�����ϲ�ѯ��һ���ֻ����Ӳ�ѯ������LIMIT�Ӿ�
	**��16���ⲿ��ѯ������һ���ۼ���ѯ���Ӳ�ѯ���ܰ���OEDERBY��ֱ������group_concat������������ƥ�䡣
	**��17���Ӳ�ѯ����һ�����ϲ�ѯ������UNION ALL���ϲ�ѯȫ���зǾۼ���ѯ��ɣ��������µĸ���ѯ��
	**      *�����Ƿ��ϲ�ѯ
	**      *����һ���ۼ���ѯ��DISTINCT��ѯ
	**      *Ҳ�������Ӳ���
	** ����ѯ���Ӳ�ѯ���ܰ���һ��WHERE�Ӿ䡣��Щ��������������11������13���ͣ�14����������Ҳ���� ORDER BY, LIMIT ��OFFSET �Ӿ�.
	** �Ӳ�ѯ����ʹ���κη��ϲ�������UNION ALL,��Ϊ���еĸ��ϲ����Ƕ���ʵ����DISTINCT����������������4��������ġ�
	** ���⣬ÿһ���Ӳ�ѯ��������뷵�ؽ���е���ͬ��������ʵ��Ҫ�󸴺ϲ�ѯ���Ӳ�ѯû�б�ƽ���ġ������߼�⵽�﷨���󣬻᷵��ϸ����Ϣ
	** 
	**��18������Ӳ�ѯ�Ǹ��ϲ�ѯ������ѯ��ORDERBY�Ӿ������������������Ӳ�ѯ���С�
	**��19���Ӳ�ѯ����ʹ��LIMIT�����ѯ����ʹ��WHERE�Ӿ�
	**��20������Ӳ�ѯ�Ǹ��ϲ�ѯ������ʹ��ORDER BY �Ӿ䡣����Ҫ�ͷ���ЩԼ������ODERRBY�е������Ӿ�Ϊ���ѯ��δ�������С��������ǻ����������Ż�������
	**��21���Ӳ�ѯ����ʹ��LIMIT�������ѯ����ʹ��DISTINCT��
	**
	**����������У�����p�����ѯ��һ��ָ�룬����Ӳ�ѯ��p->pSrc->a[iFrom].������ѯʹ���˾ۼ�isAggΪTRUE�����
	**�Ӳ�ѯʹ���˾ۼ���subqueryIsAggΪTRUE��
	**
	**������ܽ��б�ƽ������ô�������ִ�У�������0��
	**���ʹ�ñ�ƽ���ˣ�����1.
	**
	**���еı��ʽ�������뷢���ڣ������������֮ǰ���ⲿ��ѯ���Ӳ�ѯ�С�
	*/  
	static int flattenSubquery(
	  Parse *pParse,       /* Parsing context *//*����������*/
	  Select *p,           /* The parent or outer SELECT statement *//*����ѯ�����ѯ*/
	  int iFrom,           /* Index in p->pSrc->a[] of the inner subquery *//*�ڲ���ѯ��p->pSrc->a[]�е�����*/
	  int isAgg,           /* True if outer SELECT uses aggregate functions *//*����ⲿ��ѯʹ���˾ۼ�����ΪTRUE*/
	  int subqueryIsAgg    /* True if the subquery uses aggregate functions *//*����Ӳ�ѯʹ���˾ۼ�����ΪTRUE*/
	){
	  const char *zSavedAuthContext = pParse->zAuthContext;/*���﷨�������е������ĸ�ֵ��zSavedAuthContext*/
	  Select *pParent;/*����һ������ѯ�ṹ��*/
	  Select *pSub;       /* The inner query or "subquery" *//*�ڲ�ѯ���Ӳ�ѯ�ṹ��*/
	  Select *pSub1;      /* Pointer to the rightmost select in sub-query *//*ָ���Ӳ�ѯ�����ұߵĲ�ѯ*/
	  SrcList *pSrc;      /* The FROM clause of the outer query *//*�ⲿ��ѯ��FROM�Ӿ�*/
	  SrcList *pSubSrc;   /* The FROM clause of the subquery *//*�Ӳ�ѯ��FROM�Ӿ�*/
	  ExprList *pList;    /* The result set of the outer query *//*�ⲿ��ѯ�Ľ����*/
	  int iParent;        /* VDBE cursor number of the pSub result set temp table *//*VDBE�α�ţ�ָ���ڲ�ѯ����ʱ��*/
	  int i;              /* Loop counter *//*ѭ��������*/
	  Expr *pWhere;                    /* The WHERE clause *//*WHERE�Ӿ�*/
	  struct SrcList_item *pSubitem;   /* The subquery *//*�Ӳ�ѯ*/
	  sqlite3 *db = pParse->db;/*����һ�����ݿ�����*/

	  /* Check to see if flattening is permitted.  Return 0 if not.
	  *//*����Ƿ������ƽ���������������0*/
	  assert( p!=0 );/*����ϵ㣬���SELECT�ṹ��Ϊ���׳��쳣*/
	  assert( p->pPrior==0 );  /* Unable to flatten compound queries *//*���ܱ�ƽ�����ϲ�ѯ*/
	  if( db->flags & SQLITE_QueryFlattener ) return 0;/*�������������ֵΪSQLITE_QueryFlattener�����Ϊtrue��ֱ�ӷ���0*/ 
	  pSrc = p->pSrc;/*��SELECT�ṹ���е�FROM�Ӿ丳ֵ����ǰ����pSrc*/
	  assert( pSrc && iFrom>=0 && iFrom<pSrc->nSrc );/*����ϵ㣬���pSrcΪ�ջ�FROM����С��0��FROM�������ڵ���FROM���������׳�������Ϣ*/
	  pSubitem = &pSrc->a[iFrom];/*��FROM�Ӿ���a�����iFrom������ֵ��pSubitem���Ӳ�ѯ�*/
	  iParent = pSubitem->iCursor;/*���Ӳ�ѯ���α긳ֵ��VDBE�α�š�����α��ָ��һ���ڲ�ѯ���Ӳ�ѯ*/
	  pSub = pSubitem->pSelect;/*���Ӳ�ѯ�е�SELECT�ṹ�帳ֵ���Ӳ�ѯ�ṹ��*/
	  assert( pSub!=0 );/*����ϵ㣬����Ӳ�ѯ�ṹ��Ϊ�գ��׳�������Ϣ*/
	  if( isAgg && subqueryIsAgg ) return 0;                 /* Restriction (1)  *//*����������1��ʹ���˾ۼ�����*/
	  if( subqueryIsAgg && pSrc->nSrc>1 ) return 0;          /* Restriction (2)  *//*����������2��*/
	  pSubSrc = pSub->pSrc;/*���Ӳ�ѯ��FROM�Ӿ���ʽ��ֵ��ȫ�ֱ���pSubSrc���Ӳ�ѯ��FROM�Ӿ䣩*/
	  assert( pSubSrc );/*����ϵ㣬���pSubSrcΪ�գ��׳�������Ϣ*/
	  /* Prior to version 3.1.2, when LIMIT and OFFSET had to be simple constants,
	  ** not arbitrary expresssions, we allowed some combining of LIMIT and OFFSET
	  ** because they could be computed at compile-time.  But when LIMIT and OFFSET
	  ** became arbitrary expressions, we were forced to add restrictions (13)
	  ** and (14). */
	  /*��3.1.2֮ǰ����LIMIT �� OFFSET �и��򵥵ĳ�������������ı��ʽ��������������LIMIT �� OFFSET
	  **��Ϊ�����ܹ��ڱ���ʱ�����㡣���ǵ�LIMIT �� OFFSETΪ����ı��ʽ������ǿ��ʹ������������13����
	  **����������14����
	  */
	  if( pSub->pLimit && p->pLimit ) return 0;              /* Restriction (13) *//*����������13��*/
	  if( pSub->pOffset ) return 0;                          /* Restriction (14) *//*����������14��*/
	  if( p->pRightmost && pSub->pLimit ){/**/
		return 0;                                            /* Restriction (15) *//*����������15��*/
	  }
	  if( pSubSrc->nSrc==0 ) return 0;                       /* Restriction (7)  *//*����������7��*/
	  if( pSub->selFlags & SF_Distinct ) return 0;           /* Restriction (5)  *//*����������5��*/
	  if( pSub->pLimit && (pSrc->nSrc>1 || isAgg) ){
		 return 0;         /* Restrictions (8)(9) *//*����������8����9��*/
	  }
	  if( (p->selFlags & SF_Distinct)!=0 && subqueryIsAgg ){
		 return 0;         /* Restriction (6)  *//*����������6��*/
	  }
	  if( p->pOrderBy && pSub->pOrderBy ){
		 return 0;                                           /* Restriction (11) *//*����������11��*/
	  }
	  if( isAgg && pSub->pOrderBy ) return 0;                /* Restriction (16) *//*����������16��*/
	  if( pSub->pLimit && p->pWhere ) return 0;              /* Restriction (19) *//*����������19��*/
	  if( pSub->pLimit && (p->selFlags & SF_Distinct)!=0 ){
		 return 0;         /* Restriction (21) *//*����������21��*/
	  }

	  /* OBSOLETE COMMENT 1:
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
	  /* ��ʱ��ע�� 1��
	  ** ��������3���������Ӳ�ѯʹ�����ӣ�ȷ���Ӳ�ѯû����Ϊ�����ӵ��Ҳ�����һ�µ����Ӳ�����
	  **     t1 LEFT OUTER JOIN (t2 JOIN t3)
	  ** ���������ı��ʽ���б�ƽ�������ǵõ�
	  **      (t1 LEFT OUTER JOIN t2) JOIN t3
	  ** ���������ʽ��ʾ��ͬ�ˡ�
	  ** ��ʱ��ע�� 2��
	  ** ����������12��������Ӳ�ѯ���������Ӳ�ѯ���Ҳ�������ȷ���Ӳ�ѯ��û��WHERE�Ӿ䡣
	  ** ��������Ӳ���ʹ�ã�
	  **  1 LEFT OUTER JOIN (SELECT * FROM t2 WHERE t2.x>0)
	  ** ������Ƕ����������ƽ����
	  **  (t1 LEFT OUTER JOIN t2) WHERE t2.x>0
	  ** ����t2.x>0�ܻ�ʧ�ܣ���Ϊ��һ��NULL����t2���У��ܸ�Ч�Ľ�OUTER JOINת��ΪINNER JOIN.
	  **
	  ** ������д��ʱ��ע��1��ע��2����ƽ�������ӵ��������ǲ���ȫ�ġ�Ϊ����õı������������顣
	  ** ����Ӳ�ѯ���������ӵ����������������ƽ����
	  */
	  if( (pSubitem->jointype & JT_OUTER)!=0 ){/*������ѯʹ��jointype*/
		return  0;/*ֱ�ӷ���*/
	  }

	  /* Restriction 17: If the sub-query is a compound SELECT, then it must
	  ** use only the UNION ALL operator. And none of the simple select queries
	  ** that make up the compound SELECT are allowed to be aggregate or distinct
	  ** queries.
	  *//*������ϲ�ѯ���ӳ������ʹ��UNION ALL �����������������ϲ�ѯ���Ӳ�ѯ�У�û��ʹ�þۼ�������ȥ���ظ�*/
	  if( pSub->pPrior ){/*�Ӳ�ѯ�����Ȳ�ѯ��SELECT*/
		if( pSub->pOrderBy ){/*�Ӳ�ѯ����OrderBy*/
		  return 0;  /* Restriction 20 *//*����������20�� ֱ�ӷ���*/
		}
		if( isAgg || (p->selFlags & SF_Distinct)!=0 || pSrc->nSrc!=1 ){/*����ⲿ��ѯʹ���˾ۼ�������ʹ��ȥ���ظ�����FROM������1*/
		  return 0;/*ֱ�ӷ���*/
		}
		for(pSub1=pSub; pSub1; pSub1=pSub1->pPrior){/*�����Ӳ�ѯ�����ұߵĲ�ѯ���ҵ����ұߵĲ�ѯ*/
		  testcase( (pSub1->selFlags & (SF_Distinct|SF_Aggregate))==SF_Distinct );/*�����Ƿ�ʹ����Distinct*/
		  testcase( (pSub1->selFlags & (SF_Distinct|SF_Aggregate))==SF_Aggregate );/*�����Ƿ�ʹ����Aggregate*/
		  assert( pSub->pSrc!=0 );/*����ϵ㣬�ж��Ӳ�ѯ��FROM�Ӿ��Ƿ�Ϊ��*/
		  if( (pSub1->selFlags & (SF_Distinct|SF_Aggregate))!=0/*����Ӳ�ѯ����Distinct��Aggregate*/
		   || (pSub1->pPrior && pSub1->op!=TK_ALL) /*���Ӳ�ѯ�к������Ȳ�ѯSELECT���Ҳ���ΪTK_ALL*/
		   || pSub1->pSrc->nSrc<1/*���Ӳ�ѯ��FROM�Ӿ��б��ʽ�ĸ���С��1*/
		   || pSub->pEList->nExpr!=pSub1->pEList->nExpr/*���Ӳ�ѯ�б��ʽ�ĸ����������Ӳ�ѯ���ұ߲�ѯ�ı��ʽ����*/
		  ){
			return 0;/*ֱ�ӷ���*/
		  }
		  testcase( pSub1->pSrc->nSrc>1 );/*�����Ӳ�ѯ��FROM�Ӿ��б��ʽ��������1*/
		}

		/* Restriction 18. *//*����������18��*/
		if( p->pOrderBy ){/*�������ORDERBY�Ӿ�*/
		  int ii;
		  for(ii=0; ii<p->pOrderBy->nExpr; ii++){/*�������ʽ*/
			if( p->pOrderBy->a[ii].iOrderByCol==0 ) return 0;/*������и����������ֵΪ�գ�ֱ�ӷ��ء�*/
		  }
		}
	  }

	  /***** If we reach this point, flattening is permitted. *****/
      /*�����ִ�е���һ����˵�������ƽ��*/
	  /* Authorize the subquery *//*��Ȩ�ܹ��Ӳ�ѯ*/
	  pParse->zAuthContext = pSubitem->zName;/*���Ӳ�ѯ�����ָ�ֵ���﷨����������Ȩ����������*/
	  TESTONLY(i =) sqlite3AuthCheck(pParse, SQLITE_SELECT, 0, 0, 0);/**/
	  testcase( i==SQLITE_DENY );/*����i�Ƿ�ΪSQLITE_DENY*/
	  pParse->zAuthContext = zSavedAuthContext;/*�﷨�������е��������Ѹ�ֵ��zSavedAuthContext���ٸ�ֵ���﷨����������Ȩ����������*/

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
	  /*����Ӳ�ѯ��һ�����ϲ�ѯ��Ȼ�������������17����������18������UNION ALL ���Ҹ���ѯ�ĸ�ʽ���£�
	  **    SELECT <expr-list> FROM (<sub-query>) <where-clause> 
	  **������Ը�ORDER BY, LIMIT �� OFFSET �Ӿ䡣��һ�鴴����n-1��copy�Ĳ���ORDER BY, LIMIT �� OFFSET����ѯ���� 
	  **���������ϸտ�ʼʹ�õ�UNION ALL��������N�Ǹ��ϲ�ѯ�м򵥲�ѯ�ı�š�
	  **�뿴���ӣ�
	  **     SELECT a+1 FROM (
	  **        SELECT x FROM tab
	  **        UNION ALL
	  **        SELECT y FROM tab
	  **        UNION ALL
	  **        SELECT abs(z*2) FROM tab2
	  **     ) WHERE a!=5 ORDER BY 1
	  **ת���ɣ�
	  **     SELECT x+1 FROM tab WHERE x+1!=5
	  **     UNION ALL
	  **     SELECT y+1 FROM tab WHERE y+1!=5
	  **     UNION ALL
	  **     SELECT abs(z*2)+1 FROM tab2 WHERE abs(z*2)+1!=5
	  **     ORDER BY 1
	  **���ǳ���Ϊ����ƽ�������Ӳ�ѯ��
	  */
	  for(pSub=pSub->pPrior; pSub; pSub=pSub->pPrior){/*�ݹ�����Ӳ�ѯ�����Ȳ�ѯ*/
		Select *pNew;/*����һ��SELECT�ṹ��*/
		ExprList *pOrderBy = p->pOrderBy;/*��SELECT�ṹ����ORDERBY���ʽ��ֵ�����ʽ�б��е�pOrderBy*/
		Expr *pLimit = p->pLimit;/*��SELECT�ṹ����LIMIT���ʽ��ֵ�����ʽ��pLimit����*/
		Select *pPrior = p->pPrior;/*��SELECT���Ȳ��ҵ�SELECT��ֵ����������pPrior*/
		p->pOrderBy = 0;/*��SELECT��pOrderBy��0*/
		p->pSrc = 0;/*��SELECT��pSrc��0*/
		p->pPrior = 0;/*��SELECT��pPrior��0*/
		p->pLimit = 0;/*��SELECT��pLimit��0*/
		pNew = sqlite3SelectDup(db, p, 0);/*��SELECT�ṹ��p copy ��pNew*/
		p->pLimit = pLimit;/*������pLimit��ֵ��SELECT�ṹ��p��pLimit����*/
		p->pOrderBy = pOrderBy;/*������pOrderBy��ֵ��SELECT�ṹ��p��pOrderBy����*/
		p->pSrc = pSrc;/*������pSrc��ֵ��SELECT�ṹ��p��pSrc����*/
		p->op = TK_ALL;/*��SELECT�ṹ��p��op�����������Ե�ֵ����ΪTK_ALL*/
		p->pRightmost = 0;/*��SELECT�ṹ��p��pRightmost�����Ҳ�ѯ����0*/
		if( pNew==0 ){/*�����������SELECT�ṹ��pNewΪ0*/
		  pNew = pPrior;/*�����Ȳ�ѯ��SELECT�ṹ�帳ֵ��pNew*/
		}else{
		  pNew->pPrior = pPrior;/*�����Ϊ�գ������Ȳ�ѯ��SELECT�ṹ�帳ֵ��pNew�����Ȳ�ѯ�ṹ�塣*/
		  pNew->pRightmost = 0;/*��SELECT�ṹ�������ұߵĲ�ѯ��0*/
		}
		p->pPrior = pNew;/*���SELECT�ṹ��pNew��ֵ��ȫ��SELECT�ṹ������Ȳ�����*/
		if( db->mallocFailed ) return 1;/*��������ڴ�ʧ�ܣ�����1*/
	  }

	  /* Begin flattening the iFrom-th entry of the FROM clause 
	  ** in the outer query.
	  *//*��ʼ���ѯ��FROM�Ӿ�ĵ�iFrom����Ŀ*/
	  pSub = pSub1 = pSubitem->pSelect;/*���Ӳ�ѯ��SELECT���ʽ��ֵ��pSub1��pSub*/
	  /* Delete the transient table structure associated with the
	  ** subquery
	  *//*ɾ�������Ӳ�ѯ����ʱ��ṹ*/
	  sqlite3DbFree(db, pSubitem->zDatabase);/*�ͷ����ݿ��������Ӳ�ѯ�������ݿ�ģ����ڴ�*/
	  sqlite3DbFree(db, pSubitem->zName);/*�ͷ����ݿ������д洢�Ӳ�ѯ���ֵ��ڴ�*/
	  sqlite3DbFree(db, pSubitem->zAlias);/*�ͷ����ݿ������д洢�Ӳ�ѯ������ϵ��������������ѯ�����ڴ�*/
	  pSubitem->zDatabase = 0;/*���Ӳ�ѯ�������ݿ��������0�������ÿգ�*/
	  pSubitem->zName = 0;/*���Ӳ�ѯ��������0*/
	  pSubitem->zAlias = 0;/*���Ӳ�ѯ��������ϵ��������������ѯ����0*/
	  pSubitem->pSelect = 0;/*���Ӳ�ѯ��SELECT���ʽ��0*/

	  /* Defer deleting the Table object associated with the
	  ** subquery until code generation is
	  ** complete, since there may still exist Expr.pTab entries that
	  ** refer to the subquery even after flattening.  Ticket #3346.
	  **
	  ** pSubitem->pTab is always non-NULL by test restrictions and tests above.
	  *//*�ӳ�ɾ�������Ӳ�ѯ�ı����ֱ�����ɴ�����ɣ�һֱ���ڵ�Expr.pTab��Ŀָ���Ǳ�ƽ������Ӳ�ѯ*/
	  if( ALWAYS(pSubitem->pTab!=0) ){/*����Ӳ�ѯ��ı�Ϊ��*/
		Table *pTabToDel = pSubitem->pTab;/*���Ӳ�ѯ�ı�ֵ��Table�ṹ��pTabToDel*/
		if( pTabToDel->nRef==1 ){/*���pTabToDel�������е�ָ����Ϊ1*/
		  Parse *pToplevel = sqlite3ParseToplevel(pParse);/*���﷨������pParse��ֵ��pToplevel*/
		  pTabToDel->pNextZombie = pToplevel->pZombieTab;/*�������﷨���ж�Ӧ��ֵ��Table�ṹ�����һ����Ӧ��*/
		  pToplevel->pZombieTab = pTabToDel;/*��Table�ṹ�帳ֵ�������﷨����pZombieTab*/
		}else{
		  pTabToDel->nRef--;/*����Table�ṹ�������ָ����--*/
		}
		pSubitem->pTab = 0;/*���Ӳ�ѯ�Ĺ�����ֵΪ��*/
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
	  /* ��������ѭ��һ���ԵĽ����ϲ�ѯ��ÿ��������б�ƽ���������������������һ�ַ�ʽ�ı�ƽ������--
	  ** ���ֱ�ƽ���������Ǹ����Ӳ�ѯ�ı�ƽ�������ѭ��ֻ����һ�Ρ�
	  **
	  ** ���ѭ�����Ӳ�ѯ�е�FROM�Ӿ������Ԫ�ض��ƶ����ⲿ��ѯ��FROM�Ӿ��С������֮ǰ����ס����ѯ������ⲿ��ѯ
	  ** FROM�Ӿ���α������������ѯ���α�������û���ù���������ɨ����ʽ���Ҹ���ѯ��ص������صı��ʽ������
	  ** �Ӳ�ѯ��FROMԪ�ء�
	  */
	  for(pParent=p; pParent; pParent=pParent->pPrior, pSub=pSub->pPrior){/*��������ѯ�����Ȳ��ҵ�SELECT*/
		int nSubSrc;
		u8 jointype = 0;
		pSubSrc = pSub->pSrc;     /* FROM clause of subquery *//*�Ӳ�ѯ��FROM�Ӿ�*/
		nSubSrc = pSubSrc->nSrc;  /* Number of terms in subquery FROM clause *//*�Ӳ�ѯ��FROM�Ӿ��������*/
		pSrc = pParent->pSrc;     /* FROM clause of the outer query *//*���ѯ��FROM�Ӿ�*/

		if( pSrc ){/*����Ӳ�ѯ��FROM�Ӿ䲻Ϊ��*/
		  assert( pParent==p );  /* First time through the loop *//*��һ��ѭ��������ϵ㣬�жϸ���ѯ�Ƿ�ΪP*/
		  jointype = pSubitem->jointype;/*���Ӳ�ѯ�е��������͸�ֵ����ǰ���������ͱ���jointype*/
		}else{
		  assert( pParent!=p );  /* 2nd and subsequent times through the loop *//*�ڶ���ѭ��*/
		  pSrc = pParent->pSrc = sqlite3SrcListAppend(db, 0, 0, 0);/*��ӱ��ʽ����ֵ������ѯ��pSrc*/
		  if( pSrc==0 ){/*���FROM�Ӿ�Ϊ��*/
			assert( db->mallocFailed );/*����ϵ㣬�жϷ����ڴ��Ƿ�ʧ�ܣ����ʧ�ܣ��׳�������Ϣ*/
			break;
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
		** ����Ӳ�ѯʹ��һ�����������ѯ��FROM�Ӿ�Ľ��ڡ��������Ӳ�ѯ��FROM�Ӿ��У�������ֹһ��Ԫ�أ�
		** Ȼ���������ѯ�������е�Ԫ��װ�뵽�Ӳ�ѯ�С�
		** ���ӣ�
		**      SELECT * FROM tabA, (SELECT * FROM sub1, sub2), tabB;
		** ���ѯ��3��������FROM�Ӿ��С�һ�����ѯ�Ľ��ڱ��Ӳ�ѯʹ�á���һ������齫���ѯ����Ϊ4�����ڡ����
		** �м������չΪ�������ڣ�Ϊ��װ�Ӳ�ѯFROM�Ӿ��е�����Ԫ�ء�
		*/
		if( nSubSrc>1 ){/*����Ӳ�ѯ��FROMԪ�ظ�������1*/
		  pParent->pSrc = pSrc = sqlite3SrcListEnlarge(db, pSrc, nSubSrc-1,iFrom+1);/*�����ʽ�б�����Ϊ��󣬲���ֵ��pSrc��SELECT�е�pSrc��SrcָFROM�Ӿ䣩*/
		  if( db->mallocFailed ){/*��������ڴ�ʧ��*/
			break;
		  }
		}

		/* Transfer the FROM clause terms from the subquery into the
		** outer query.
		*//*���Ӳ�ѯ�е�FROM�Ӿ�ת�������ѯ*/
		for(i=0; i<nSubSrc; i++){/*�����Ӳ�ѯ��FROM�Ӿ��Ԫ��*/
		  sqlite3IdListDelete(db, pSrc->a[i+iFrom].pUsing);/*����ID��Ž����ʽa�����еı��ʽɾ��*/
		  pSrc->a[i+iFrom] = pSubSrc->a[i];/*���Ӳ�ѯ��FROM�Ӿ丳ֵ��SELECT��FROM�Ӿ�*/
		  memset(&pSubSrc->a[i], 0, sizeof(pSubSrc->a[i]));/*��pSubSrc->a[i]��ǰsizeof(pSubSrc->a[i])���ֽ���0�滻*/
		}
		pSrc->a[iFrom].jointype = jointype;/*���������ͱ���jointype��ֵ��pSrc->a[iFrom].jointype*/
	  
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
		/* ���ڿ�ʼȡ���Ӳ�ѯ��������ʽ�������ⲿ��ѯ��iParent������ѯ����
		** �������£�
		** SELECT a+5, b*10 FROM (SELECT x*3 AS a, y+10 AS b FROM t1) WHERE a>b;
		**   \                     \_____________ subquery __________/          /
		**    \_____________________ outer query ______________________________/
		**���������ѯ����ÿһ�����ʽ������a��ȡ"x*3"������b��ȡ"y+10".
		*/
		pList = pParent->pEList;/*������ѯ�ı��ʽ�б�ֵ��pList*/
		for(i=0; i<pList->nExpr; i++){/*�������ʽ*/
		  if( pList->a[i].zName==0 ){/*������ʽ������Ϊ��*/
			const char *zSpan = pList->a[i].zSpan;
			if( ALWAYS(zSpan) ){/*�����һ�и�ֵ����ⲿ��ѯ�Ľ����pList����*/
			  pList->a[i].zName = sqlite3DbStrDup(db, zSpan);/*copy �ⲿ��ѯ�Ľ����pList�����ָ�ֵ��������б��ʽ������*/
			}
		  }
		}
		substExprList(db, pParent->pEList, iParent, pSub->pEList);/*��ȡpParent->pEList���ʽ�б�pSub->pEList*/
		if( isAgg ){/*����ⲿ��ѯʹ���˾ۼ�����*/
		  substExprList(db, pParent->pGroupBy, iParent, pSub->pEList);/*��ȡpParent->pGroupBy���ʽ�б�pSub->pEList*/
		  pParent->pHaving = substExpr(db, pParent->pHaving, iParent, pSub->pEList);/*��pParent->pHaving���ʽ��pSub->pEList*/
		}
		if( pSub->pOrderBy ){/*����Ӳ�ѯ�к�OrderBy*/
		  assert( pParent->pOrderBy==0 );/*����ϵ㣬���Ӳ�ѯ��OrderBy�ÿ�*/
		  pParent->pOrderBy = pSub->pOrderBy;/*���Ӳ�ѯ��pOrderBy��ֵ������ѯ�е�pOrderBy*/
		  pSub->pOrderBy = 0;/*���Ӳ�ѯpOrderBy�ÿ�*/
		}else if( pParent->pOrderBy ){/*�������ѯ�е�pOrderBy��Ϊ��*/
		  substExprList(db, pParent->pOrderBy, iParent, pSub->pEList);/*��ȡpParent->pOrderBy���ʽ�б�pSub->pEList*/
		}
		if( pSub->pWhere ){/*����Ӳ�ѯ��pWhere��Ϊ��*/
		  pWhere = sqlite3ExprDup(db, pSub->pWhere, 0);/*���copy���ʽpSub->pWhere���ظ�pWhere*/
		}else{
		  pWhere = 0;/*����pWhere�ÿ�*/
		}
		if( subqueryIsAgg ){/*����Ӳ�ѯʹ���˾ۼ�����*/
		  assert( pParent->pHaving==0 );/*����ϵ㣬���pHaving��Ϊ���׳�������Ϣ*/
		  pParent->pHaving = pParent->pWhere;/*������ѯ��WHERE�Ӿ丳ֵ��pHaving*/
		  pParent->pWhere = pWhere;/*��SELECT�е�pWhere��ֵ������ѯ��pWhere*/
		  pParent->pHaving = substExpr(db, pParent->pHaving, iParent, pSub->pEList);/*��ȡpParent->pHaving���ʽ��ֵ��SELECT��pWhere����*/
		  pParent->pHaving = sqlite3ExprAnd(db, pParent->pHaving,
									  sqlite3ExprDup(db, pSub->pHaving, 0));/*���ý�����ѯpHaving�Ӿ����Ӳ�ѯpHaving�Ӿ�����һ�𣬸�ֵ������ѯ��pHaving*/
		  assert( pParent->pGroupBy==0 );/*����ϵ㣬�������ѯ�к�GroupBy�׳��쳣*/
		  pParent->pGroupBy = sqlite3ExprListDup(db, pSub->pGroupBy, 0);/*���copy�Ӳ�ѯpSub->pGroupBy���Ը�����ѯ��pSub->pGroupBy*/
		}else{
		  pParent->pWhere = substExpr(db, pParent->pWhere, iParent, pSub->pEList);/*��ȡpParent->pWhere���ʽ��ֵ��pSub->pEList����*/
		  pParent->pWhere = sqlite3ExprAnd(db, pParent->pWhere, pWhere);/*���ý�����ѯpWhere�����뵱ǰ��ѯ��pWhere��������һ��Ӧ������һ�����ϵı��ʽ������ֵ������ѯ��pWhere*/
		}
	  
		/* The flattened query is distinct if either the inner or the
		** outer query is distinct. 
		*//*����ڲ�ѯ�����ѯ����Ψһ�ģ���ô��ƽ����ѯҲ��Ψһ�ġ�*/
		pParent->selFlags |= pSub->selFlags & SF_Distinct;
	  /*��selFlags��ǩ��SF_Distinct��ȡ���ظ����븸��ѯselFlagsλ���Ժ�ֵ������ѯselFlags���*/
		/*
		** SELECT ... FROM (SELECT ... LIMIT a OFFSET b) LIMIT x OFFSET y;
		**
		** One is tempted to try to add a and b to combine the limits.  But this
		** does not work if either limit is negative.
		*//*��limit��ʹ��a��b��limit��offset������limit�Ǹ���ʱ������ʹ��*/
		if( pSub->pLimit ){/*����Ӳ�ѯ�к���limit*/
		  pParent->pLimit = pSub->pLimit;/*���Ӳ�ѯ��limit��ֵ������ѯ��limit*/
		  pSub->pLimit = 0;/*���Ӳ�ѯ��limit��0�������Ѿ���ֵ������ѯ��limit��*/
		}
	  }

	  /* Finially, delete what is left of the subquery and return
	  ** success.
	  *//*���ɾ����ߵ��Ӳ�ѯ�����سɹ�*/
	  sqlite3SelectDelete(db, pSub1);/*ɾ���Ӳ�ѯ*/

	  return 1;/*����1������0��û�гɹ�ִ��*/
	}
	#endif /* !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW) */

	/*
	** Analyze the SELECT statement passed as an argument to see if it
	** is a min() or max() query. Return WHERE_ORDERBY_MIN or WHERE_ORDERBY_MAX if 
	** it is, or 0 otherwise. At present, a query is considered to be
	** a min()/max() query if:
	**
	**   1. There is a single object in the FROM clause.
	**
	**   2. There is a single expression in the result set, and it is
	**      either min(x) or max(x), where x is a column reference.
	*/
	/* �������SELECT����������һ��a min() �� max() .����WHERE_ORDERBY_MIN �� WHERE_ORDERBY_MAX ��0.
	** ���ڣ����һ����ѯ����Ϊ��һ��min() �� max() ��ѯ��
	** 1.����FROM�Ӿ��е�һ����������
	** 2.����һ���������һ���������ʽ��������min(x) �� max(x)��x���漰���С�
	*//*������ע������﷨������ȷ���Ǿͽ��������û�а����ؼ��֣���Ĭ�ϣ������max��min������Ӧ�������*/
	static u8 minMaxQuery(Select *p){
	  Expr *pExpr; /*����һ�����ʽ*/
	  ExprList *pEList = p->pEList;/*��SELECT�ı��ʽ�б�ֵ�����ʽ�б�pEList*/

	  if( pEList->nExpr!=1 ) return WHERE_ORDERBY_NORMAL;/*������ʽ�б��еı��ʽ������Ϊ1������WHERE_ORDERBY_NORMAL��Ĭ�ϣ�*/
	  pExpr = pEList->a[0].pExpr;/*�����ʽ���еĵ�һ�����ʽ��ֵ��pExpr*/
	  if( pExpr->op!=TK_AGG_FUNCTION ) return 0;/*������ʽ�Ĳ�������TK_AGG_FUNCTION��ֱ�ӷ���0*/
	  if( NEVER(ExprHasProperty(pExpr, EP_xIsSelect)) ) return 0;/*������ʽpExpr�к���EP_xIsSelect������0*/
	  pEList = pExpr->x.pList;/*�����ʽ��x�ı��ʽ�б�ֵ��pEList*/
	  if( pEList==0 || pEList->nExpr!=1 ) return 0;/*���pEListΪ�ջ��߱��ʽ�б��б��ʽ������Ϊ1��ֱ�ӷ���0*/
	  if( pEList->a[0].pExpr->op!=TK_AGG_COLUMN ) return WHERE_ORDERBY_NORMAL;/*������ʽ�в�������TK_AGG_COLUMN������WHERE_ORDERBY_NORMAL��Ĭ�ϣ�*/
	  assert( !ExprHasProperty(pExpr, EP_IntValue) );/*����ϵ㣬������ʽpExpr�в���EP_xIsSelect���׳�������Ϣ*/
	  if( sqlite3StrICmp(pExpr->u.zToken,"min")==0 ){/*����������ʽ�м��ַ���Ϊmin������WHERE_ORDERBY_MIN����Сֵ��*/
		return WHERE_ORDERBY_MIN;
	  }else if( sqlite3StrICmp(pExpr->u.zToken,"max")==0 ){/*����������ʽ�м��ַ���Ϊmax������WHERE_ORDERBY_MAX�����ֵ��*/
		return WHERE_ORDERBY_MAX;
	  }
	  return WHERE_ORDERBY_NORMAL;/*��󷵻�WHERE_ORDERBY_NORMAL��Ĭ�ϣ�*/
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
	/* ������ע����count������һ��������˵����һ���ۼ���������tab�����ڶ���������˵���Ǵ洢���ݵĵط���
	** �����뽲����������mark,ѧϰ����
	** select�ѵ�һ�����������ۺϲ�ѯ���ڶ��������Ǿۼ���Ϣ����
	** SELECT count(*) FROM <tbl>
	** �������һ�����ݿ������һ���Ӳ�ѯ�Ľ��Ҳ����һ����ͼ�������ѯ��ƥ�䣬ָ��Table����<tab>��ָ�뷵�ء�
	** ���򷵻�0
	*/
	static Table *isSimpleCount(Select *p, AggInfo *pAggInfo){/*��һ������Ϊ��ѯ�﷨�����ڶ���Ϊ����ۼ������ṹ��*/
	  Table *pTab;
	  Expr *pExpr;

	  assert( !p->pGroupBy );/*����ϵ㣬�ж��������GROUPBY�������׳�������Ϣ*/

	  if( p->pWhere || p->pEList->nExpr!=1 /*�������WHERE�Ӿ����ʽ�ĸ�����Ϊ1*/
	   || p->pSrc->nSrc!=1 || p->pSrc->a[0].pSelect/*��FROM�Ӿ���Ԫ�أ�����Ϊ1����Ƕ�ײ�ѯ*/
	  ){
		return 0;/*ֱ�ӷ���0*/
	  }
	  pTab = p->pSrc->a[0].pTab;/*��SELECT�ṹ��p�еı��ֵ��Table�ṹ��pTab*/
	  pExpr = p->pEList->a[0].pExpr;/*��SELECT�ṹ��p�еı��ʽ��ֵ�����ʽpExpr*/
	  assert( pTab && !pTab->pSelect && pExpr );/*����ϵ㣬�ж����pTab��Ϊ�գ�SELECT���޸��Ϻ�pExprû��ͬʱ���������׳�������Ϣ*/

	  if( IsVirtual(pTab) ) return 0;/*���pTab�����һ���м����������һ�����������ݿ����Ҳ����0*/
	  if( pExpr->op!=TK_AGG_FUNCTION ) return 0;/*������ʽ�еĲ�������TK_AGG_FUNCTIONҲ����0*/
	  if( NEVER(pAggInfo->nFunc==0) ) return 0;/*����ۼ������ṹ���оۼ�����Ϊ0��Ҳ����0*/
	  if( (pAggInfo->aFunc[0].pFunc->flags&SQLITE_FUNC_COUNT)==0 ) return 0;/*����ۼ������ṹ���оۼ���������SQLITE_FUNC_COUNTҲ����0*/
	  if( pExpr->flags&EP_Distinct ) return 0;/*������ʽ�йؼ��ֱ����EP_Distinct��ֵ��Ҳ����0*/

	  return pTab;/*�������յĽ��pTab*/
	}

	/*
	** If the source-list item passed as an argument was augmented with an
	** INDEXED BY clause, then try to locate the specified index. If there
	** was such a clause and the named index cannot be found, return 
	** SQLITE_ERROR and leave an error in pParse. Otherwise, populate 
	** pFrom->pIndex and return SQLITE_OK.
	*/
	/* ������ע������һ���������൱�ڲ��ҵĲ��ң�Ҫ�����ڱ��ʽ�б������������ң�
	** ���Դ�б�����Ϊһ������������������һ��INDEXED BY�Ӿ䣬Ȼ�����ҵ���������������
	** �������Ӿ����������û���ҵ�������SQLITE_ERROR�������﷨��������дһ��������Ϣ��
	** ������䵽 pFrom->pIndex���ҷ���һ�� SQLITE_OK���ɹ���ɱ�ǣ�.
	*/
	int sqlite3IndexedByLookup(Parse *pParse, struct SrcList_item *pFrom){
	  if( pFrom->pTab && pFrom->zIndex ){/*���FROM���ʽ�SrcList_itemΪFROM�нṹ�壩��pTabΪ�ղ��Һ���INDEXED BY�ı�ʾ��Ϊ��*/
		Table *pTab = pFrom->pTab;/*��FROM���ʽ���pTab��ֵ��һ����������Table�ṹ��pTab*/
		char *zIndex = pFrom->zIndex;/*��FROM���ʽ���������ʾ����ֵ��zIndex*/
		Index *pIdx;/*����һ������*/
		for(pIdx=pTab->pIndex; 
			pIdx && sqlite3StrICmp(pIdx->zName, zIndex); 
			pIdx=pIdx->pNext/*�������������ֺ�������ʶ��������FROM���ʽ�������*/
		);
		if( !pIdx ){/*����������Ϊ��*/
		  sqlite3ErrorMsg(pParse, "no such index: %s", zIndex, 0);/*����������������*/
		  pParse->checkSchema = 1;/*���﷨�������м�������Ϣ��1*/
		  return SQLITE_ERROR;/*����SQLITE_ERROR*/
		}
		pFrom->pIndex = pIdx;/*ִ�е���һ����˵����Ϊ�գ���������ӵ�FROM���ʽ��������ṹ����*/
	  }
	  return SQLITE_OK;/*����ִ�гɹ����*/
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
	/* ���������Walker�ص���expanding����һ��SELECT��䣬��Expanding����˼���£�
	**   ��1��ȷ��VEBE�α���Ѿ������FROM�Ӿ��ÿ��Ԫ��   
	**   ��2�����pTabList->a[].pTab�У�����SrcList�����FROM�Ӿ䡣����ͼ������FROM�Ӿ��У����pTabList->a[].pSelect
	**        ����ʵ����ͼ��һ��SELECT���ĸ�����ʹ��SELECT������ɵĸ��������������ɵĸ���ɾ�������䣬���õ���
	**        �Ὣ��ǰ����ͼ�޸ġ�
	**   ��3�����һ��WHERE�Ӿ�����NATURAL�ؼ����������ϣ�����ON USINGҲ�����������ϡ�
	**   ��4��ɨ���б��ڽ������pEList���У����ҡ�*����������ʵ����TABLE.*������������ҵ��ˣ���չÿ����*��
	**        ��ÿ�����ÿһ�У�����TABLE.*��ΪTABLE���С�
	*/
	static int selectExpander(Walker *pWalker, Select *p){
	  Parse *pParse = pWalker->pParse;/*����һ���﷨������*/
	  int i, j, k;
	  SrcList *pTabList;/*����һ��FROM�Ӿ��б�*/
	  ExprList *pEList;/*����һ�����ʽ�б�*/
	  struct SrcList_item *pFrom;/*����һ��FROM�Ӿ��б���*/
	  sqlite3 *db = pParse->db;/*����һ�����ݿ�����*/

	  if( db->mallocFailed  ){/*��������ڴ�ʧ��*/
		return WRC_Abort;/*����WRC_Abort����ֹ��*/
	  }
	  if( NEVER(p->pSrc==0) || (p->selFlags & SF_Expanded)!=0 ){/*���FROM�Ӿ�Ϊ�ջ�selFlags��Ƿ�ΪSF_Expanded*/
		return WRC_Prune;/*����WRC_Prune��ɾ����*/
	  }
	  p->selFlags |= SF_Expanded;/*��selFlags������SF_Expandedλ���ٸ�ֵ��selFlags*/
	  pTabList = p->pSrc;/*��FROM�Ӿ丳ֵ��FROM�Ӿ��б�*/
	  pEList = p->pEList;/*�ٽ�SELECT�ṹ���б��ʽ�б�ֵ��pEList*/

	  /* Make sure cursor numbers have been assigned to all entries in
	  ** the FROM clause of the SELECT statement.
	  *//*ȷ�����е��α���Ѿ������SELECT�����FROM�Ӿ��������Ŀ��*/
	  sqlite3SrcListAssignCursors(pParse, pTabList);/*Ϊ���ʽ�б�pTabList�����б�����α��*/

	  /* Look up every table named in the FROM clause of the select.  If
	  ** an entry of the FROM clause is a subquery instead of a table or view,
	  ** then create a transient table structure to describe the subquery.
	  *//*����SELECT��FROM�Ӿ���ÿһ�����������FROM�Ӿ��һ����Ŀ���ӳ��򣬾����Ϊһ�������ͼ��Ȼ��
        **Ȼ�󴴽�һ�������ṹ��������ӳ���	  */
	  for(i=0, pFrom=pTabList->a; i<pTabList->nSrc; i++, pFrom++){/*�������б�*/
		Table *pTab;/*����һ��TABLE�ṹ��pTab*/
		if( pFrom->pTab!=0 ){/*�����ŵı�Ϊ��*/
		  /* This statement has already been prepared.  There is no need
		  ** to go further. *//*��������Ѿ�������ִ�У����ﲻ��Ҫ�ٽ�һ��ִ���ˡ�*/
		  assert( i==0 );/*����ϵ㣬���i��=0���׳�������Ϣ��*/
		  return WRC_Prune;/*����WRC_Prune����ɾ����ǩ��*/
		}
		if( pFrom->zName==0 ){/*�������Ϊ��*/
	#ifndef SQLITE_OMIT_SUBQUERY
		  Select *pSel = pFrom->pSelect;/*��pFrom��ֵ��pSel����ʵ������SELECT����滻����*/
		  /* A sub-query in the FROM clause of a SELECT *//*SELECT��FROM���Ӳ�ѯ*/
		  assert( pSel!=0 );/*����ϵ㣬���pSel=0���׳�������Ϣ*/
		  assert( pFrom->pTab==0 );/*����ϵ㣬���SQL��Ϊ�գ��׳�������Ϣ��������Ϊ��ʱzNameΪ�գ�pTab��zNameһ�£������Ϊ�գ�˵�����ݲ�һ��*/
		  sqlite3WalkSelect(pWalker, pSel);/*pSel�ṹ����ÿ�����ʽ������sqlite3WalkExpr*/
		  pFrom->pTab = pTab = sqlite3DbMallocZero(db, sizeof(Table));/*���䲢����ڴ棬�����ڴ��СΪ���С*/
		  if( pTab==0 ) return WRC_Abort;/*�����ʱpTabΪ0��˵��û�з��䵽��������ֹ����WRC_Abort*/
		  pTab->nRef = 1;/*������ָ������Ϊ1*/
		  pTab->zName = sqlite3MPrintf(db, "sqlite_subquery_%p_", (void*)pTab);/*����pTab���ַ����ϲ��ŵ�pTab��Ϊ����*/
		  while( pSel->pPrior ){ pSel = pSel->pPrior; }/*�����ҵ�������ִ�е�SELECT*/
		  selectColumnsFromExprList(pParse, pSel->pEList, &pTab->nCol, &pTab->aCol);/*�����﷨��������SELECT�ṹ���б��ʽ�б�����������б�����������*/
		  pTab->iPKey = -1;/*����iPKey=-1��˵����ʱ����aCol���±�ΪiPKey����������*/
		  pTab->nRowEst = 1000000;/*������������Ϊ1000000*/
		  pTab->tabFlags |= TF_Ephemeral;/*��������ΪTF_Ephemeral����ʱ��*/
	#endif
		}else{
		  /* An ordinary table or view name in the FROM clause *//*FROM�Ӿ����Դ��������ͼ��*/
		  assert( pFrom->pTab==0 );/*����ϵ㣬���FROM�Ӿ��б�Ϊ�գ��׳��쳣*/
		  pFrom->pTab = pTab = 
			sqlite3LocateTable(pParse,0,pFrom->zName,pFrom->zDatabase);/*�������ݿ����Ӻ�FROM�б����ҵ���*/
		  if( pTab==0 ) return WRC_Abort;/*���û�ҵ���������ֹ��ǩ��WRC_Abort��*/
		  pTab->nRef++;/*��pTab�е�ָ������1*/
	#if !defined(SQLITE_OMIT_VIEW) || !defined (SQLITE_OMIT_VIRTUALTABLE)
		  if( pTab->pSelect || IsVirtual(pTab) ){/*�������SELECT��Ϊ�ջ�pTabΪ���*/
			/* We reach here if the named table is a really a view *//*ִ�е���һ����˵���������һ����������ͼ*/
			if( sqlite3ViewGetColumnNames(pParse, pTab) ) return WRC_Abort;/*����ͼ����װ��Tabel��ṹ�У����ص���ִ�д������������д��󣬷�����ִֹ�б��*/
			assert( pFrom->pSelect==0 );/*����ϵ㣬���pFfom���ʽ�б���SELECT��Ϊ�գ��׳�������Ϣ*/
			pFrom->pSelect = sqlite3SelectDup(db, pTab->pSelect, 0);/*��Table�ṹ��pSelect copy ��pSelect*/
			sqlite3WalkSelect(pWalker, pFrom->pSelect);/*���ʽ�б�pFrom��ÿ�����ʽ������sqlite3WalkExpr*/
		  }
	#endif
		}

		/* Locate the index named by the INDEXED BY clause, if any. *//*�������Ӿ��в���������*/
		if( sqlite3IndexedByLookup(pParse, pFrom) ){/*�ڱ��ʽ�б��в���������������������󣬷��ش������*/
		  return WRC_Abort;/*�ٷ�����ִֹ�б��*/
		}
	  }

	  /* Process NATURAL keywords, and ON and USING clauses of joins.
	  *//*���������е�NATURAL�ؼ��� ON USING*/
	  if( db->mallocFailed || sqliteProcessJoin(pParse, p) ){/*��������ڴ������ߴ�����select�����join��Ϣ�����ش���ִ�и�����*/
		return WRC_Abort;/*�ٷ�����ִֹ�б��*/
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
	  ** ���ڳ������б���ÿһ����*�������뵽���б�������е����֡����Ҷ���ÿ�� ����.* �����������������
	  ** ���ÿ�����б��з��ֵġ�*�������﷨�������У�����һ����TK_ALL��������������ʽ��
	  ** ����������ֻ������TK_ALL���ʽ������չÿһ�����ʽ�����б���������С�
	  ** ��һ��ѭ��ֻ�Ǽ���Ƿ����Ҫ��չ��"*"�Ĳ�����
	  */
	  for(k=0; k<pEList->nExpr; k++){/*�������ʽ�б�*/
		Expr *pE = pEList->a[k].pExpr;/*���б���ʽ�б��ʽ��ֵ��pE*/
		if( pE->op==TK_ALL ) break;/*������ʽ�в���ΪTK_ALL*/
		assert( pE->op!=TK_DOT || pE->pRight!=0 );/*����ϵ㣬�������ΪTK_DOT���߱��ʽ�����ұ߱��ʽ�����ڣ��׳�������Ϣ*/
		assert( pE->op!=TK_DOT || (pE->pLeft!=0 && pE->pLeft->op==TK_ID) );/*����ϵ㣬�������ΪTK_DOT�������ұ߱��ʽ��ID�����ڣ��׳�������Ϣ*/
		if( pE->op==TK_DOT && pE->pRight->op==TK_ALL ) break;/*�������ΪTK_DOT�����������Ĳ���ΪTK_ALL��ֱ��break*/
	  }
	  if( k<pEList->nExpr ){/*���kС�ڱ��ʽ�б��б��ʽ�ĸ���*/
		/*
		** If we get here it means the result set contains one or more "*"
		** operators that need to be expanded.  Loop through each expression
		** in the result set and expand them one by one.
		*//*������������һ������ "*"��������Ҫ��չ���ڽ����������ͨ��������һ����һ����չ*/
		struct ExprList_item *a = pEList->a;/*�����ʽ�б��зű��ʽ�����鸳ֵ�����ʽ�б���ı��ʽ����*/
		ExprList *pNew = 0;/*����һ�����ʽ�б�*/
		int flags = pParse->db->flags;/*���﷨�����������ݿ����ӵı�Ǹ�ֵ��flags*/
		int longNames = (flags & SQLITE_FullColNames)!=0/*���flagsΪSQLITE_FullColNames���Ҳ�����SQLITE_ShortColNames����ֵ��longNames*/
						  && (flags & SQLITE_ShortColNames)==0;/**/

		for(k=0; k<pEList->nExpr; k++){/*�������ʽ�б�*/
		  Expr *pE = a[k].pExpr;/*����ű��ʽ������a�еı��ʽ��ֵ��pE*/
		  assert( pE->op!=TK_DOT || pE->pRight!=0 );/*����ϵ㣬������ʽ�Ĳ���ΪTK_DOT�����������ʽΪ�գ��׳�������Ϣ*/
		  if( pE->op!=TK_ALL && (pE->op!=TK_DOT || pE->pRight->op!=TK_ALL) ){/*������ʽ�Ĳ�����ΪTK_ALL�����ұ��ʽ�Ĳ�����ΪTK_DOT��ΪTK_ALL*/
			/* This particular expression does not need to be expanded.
			*//*����ض��ı��ʽ����Ҫ��չ*/
			pNew = sqlite3ExprListAppend(pParse, pNew, a[k].pExpr);/*�����ʽpExpr��ӵ�pParse�е�pNew�У�����ֵ��pNew*/
			if( pNew ){/*����Ͼֵĸ�ֵ��Ϊ��*/
			  pNew->a[pNew->nExpr-1].zName = a[k].zName;/*�����ʽ�����е�Ԫ�صı�����ֵ���ʽ�б��еı��ʽ�еı���*/
			  pNew->a[pNew->nExpr-1].zSpan = a[k].zSpan;/*�����ʽ�����е�Ԫ�صķ�Χ��ֵ��pNew->a[pNew->nExpr-1].zSpan*/
			  a[k].zName = 0;/*�ٽ����ʽ��ExprList_item�еı��ʽ�ı�����Ϊ0*/
			  a[k].zSpan = 0;/*�����ʽ��ExprList_item�еı��ʽ�ķ�Χ��Ϊ0*/
			}
			a[k].pExpr = 0;/*���������ʽ��Ϊ0*/
		  }else{
			/* This expression is a "*" or a "TABLE.*" and needs to be
			** expanded. *//*��"*" �� "TABLE.*"����Ҫ��չ�ı��ʽ*/
			int tableSeen = 0;      /* Set to 1 when TABLE matches *//*����ƥ���ϣ���Ϊ1*/
			char *zTName;            /* text of name of TABLE *//*����ı������ַ�������*/
			if( pE->op==TK_DOT ){/*�������ΪTK_DOT*/
			  assert( pE->pLeft!=0 );/*����ϵ㣬������ʽ������Ϊ�գ��׳�������Ϣ*/
			  assert( !ExprHasProperty(pE->pLeft, EP_IntValue) );/*����ϵ㣬������ʽ������û��EP_IntValue���ԣ��׳�������Ϣ*/
			  zTName = pE->pLeft->u.zToken;/*�����ʽ�������ı�Ǹ�ֵ������ı���*/
			}else{
			  zTName = 0;/*���򽫱���ı�����0*/
			}
			for(i=0, pFrom=pTabList->a; i<pTabList->nSrc; i++, pFrom++){/*�������ʽ�б�*/
			  Table *pTab = pFrom->pTab;/*��FROM�Ӿ��б�����SQL��ֵ��pTab*/
			  char *zTabName = pFrom->zAlias;/*��FROM�Ӿ��б�����������ֵ��zTabName*/
			  if( zTabName==0 ){/*����Ͼ丳ֵ�Ľ��Ϊ��*/
				zTabName = pTab->zName;/*��������ֵ��zTabName����Ϊ����zName��pFrom->pTab��SQL����һ�£�*/
			  }
			  if( db->mallocFailed ) break;/*��������ڴ�ʧ�ܣ�ֱ��break*/
			  if( zTName && sqlite3StrICmp(zTName, zTabName)!=0 ){/*���zTName��Ϊ�գ�zTName��zTabName��һ��*/
				continue;/*��������ϵ������������˴�ѭ��*/
			  }
			  tableSeen = 1;/*ƥ���ϣ���������Ϊ1*/
			  for(j=0; j<pTab->nCol; j++){/*����������*/
				Expr *pExpr, *pRight;
				char *zName = pTab->aCol[j].zName;/*��������ֵ��zName*/
				char *zColname;  /* The computed column name *//*��������*/
				char *zToFree;   /* Malloced string that needs to be freed *//*�ͷ��ѷ�����ַ���*/
				Token sColname;  /* Computed column name as a token *//*��������Ϊ���*/

				/* If a column is marked as 'hidden' (currently only possible
				** for virtual tables), do not include it in the expanded result-set list.
				** result-set list.
				*//*���һ�б����'hidden'�����أ�����ֻ���ܶ�����������Ľ�����б��в���������*/
				if( IsHiddenColumn(&pTab->aCol[j]) ){/*����б�����*/
				  assert(IsVirtual(pTab));/*����ϵ㣬�����������׳�������Ϣ*/
				  continue;/*�����˴�ѭ��*/
				}

				if( i>0 && zTName==0 ){/*���i>0���ұ���ı���Ϊ��*/
				  if( (pFrom->jointype & JT_NATURAL)!=0/*������ʽ����������ΪJT_NATURAL*/
					&& tableAndColumnIndex(pTabList, i, zName, 0, 0)/*���ұ�����������Ϊ��*/
				  ){
					/* In a NATURAL join, omit the join columns from the 
					** table to the right of the join *//*(ע��ʹ����Ȼ���ӣ�ʡ�Ա��������ұ�Ĺ�����)*/
					continue;/*�����˴�ѭ����ִ���¸�ѭ��*/
				  }
				  if( sqlite3IdListIndex(pFrom->pUsing, zName)>=0 ){/*���ر�ʾ��ΪzName��pFrom->pUsing������*/
					/* In a join with a USING clause, omit columns in the
					** using clause from the table on the right. *//*ʹ��USING�Ӿ�����ӣ������ұ�ʹ��USING����*/
					continue;/*�����˴�ѭ����ִ���¸�ѭ��*/
				  }
				}
				pRight = sqlite3Expr(db, TK_ID, zName);/*sqlite3Expr�������zName��TK_ID���һ�����ʽ�������ظ�pRight*/
				zColname = zName;/*��zName��ֵ������zColname*/
				zToFree = 0;/*�ͷ��ѷ�����ַ���*/
				if( longNames || pTabList->nSrc>1 ){/*���longNames��ȫ�ƣ���Ϊ�ջ�����1*/
				  Expr *pLeft;
				  pLeft = sqlite3Expr(db, TK_ID, zTabName);/*sqlite3Expr���������zTabName��TK_ID���һ�����ʽ�������ظ�pLeft*/
				  pExpr = sqlite3PExpr(pParse, TK_DOT, pLeft, pRight, 0);/*�����������ʽ�ı��TK_DOTһ�����ʽ������ֵ��pExpr*/
				  if( longNames ){/*���ȫ��·������*/
					zColname = sqlite3MPrintf(db, "%s.%s", zTabName, zName);/*��ӡ���������ظ�zColname*/
					zToFree = zColname;/*�ѷ�����ַ���zToFreeֵΪzColname*/
				  }
				}else{
				  pExpr = pRight;/*����pRight��ֵ�����ձ��ʽ*/
				}
				pNew = sqlite3ExprListAppend(pParse, pNew, pExpr);/*�����ʽpExpr��ӵ�pParse�е�pNew�У�����ֵ��pNew*/
				sColname.z = zColname;/*����ֵzColname������sColname.z*/
				sColname.n = sqlite3Strlen30(zColname);/*����������λ�����ܳ���30*/
				sqlite3ExprListSetName(pParse, pNew, &sColname, 0);/*����pNew������sColname*/
				sqlite3DbFree(db, zToFree);/*�ͷ����ݿ������д��zToFree��ǵ��ڴ�*/
			  }
			}
			if( !tableSeen ){/*���û��ƥ����*/
			  if( zTName ){/*�������������Ϊ��*/
				sqlite3ErrorMsg(pParse, "no such table: %s", zTName);/*���������Ϣ���﷨��������no such table��zTName*/
			  }else{
				sqlite3ErrorMsg(pParse, "no tables specified");/*���������Ϣ���﷨��������no tables specified*/
			  }
			}
		  }
		}
		sqlite3ExprListDelete(db, pEList);/*ɾ�����ݿ������е�pEList���ʽ�б�*/
		p->pEList = pNew;/*��pNew��ֵ�����ʽ�б�pEList*/
	  }
	#if SQLITE_MAX_COLUMN
	  if( p->pEList && p->pEList->nExpr>db->aLimit[SQLITE_LIMIT_COLUMN] ){/*�������������������*/
		sqlite3ErrorMsg(pParse, "too many columns in result set");/*���������Ϣ���﷨��������too many columns in result set*/
	  }
	#endif
	  return WRC_Continue;/*���ؼ�����ʶ��WRC_Continue*/
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
	/* ��Խ�����walker�Ĳ���������
	**
	** �����������Walker.xExprCallback�ص����������ʽ��û���κβ���Ҳռ��ÿ���ڵ㡣���裬����������Walker.xExprCallback
	** Ȼ��Walker.xSelectCallback Ϊ�﷨����������ÿһ���Ӳ�ѯ�ṩ������
	*/
	static int exprWalkNoop(Walker *NotUsed, Expr *NotUsed2){
	  UNUSED_PARAMETER2(NotUsed, NotUsed2);/*���NotUsed2û��ʹ�ã���פ���ں����С����������Ϣ��*/
	  return WRC_Continue;/*���ؼ���ִ�б�ʶ��*/
	}

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
	/* SELECT�����"expands" ����������е��ӳ������ĸ�����Ϣ˵������"expand" ��䣬�ᱻworker�ص���
	**
	** SELECT�������չ�Ǵ���SELECT���ĵ�һ�������SELECT�������ڽ�������ִ��ǰ������չ��
	**
	** ������ִ������﷨��������дһ��������Ϣ�����ú�����pParse->nErr �� pParse->db->mallocFailed����������Ϣ
	*/
	static void sqlite3SelectExpand(Parse *pParse, Select *pSelect){
	  Walker w;/*����һ��Walker�ṹ��*/
	  w.xSelectCallback = selectExpander;/*����һ��selectExpander�������ص�����xSelectCallback������SELECT�ĺ�����*/
	  w.xExprCallback = exprWalkNoop;/*�����������פ��δʹ�õ���Ϣ�����������Ϣ�����ظ�xExprCallback����*/
	  w.pParse = pParse;/*���������﷨������pParse��ֵ��w.pParse*/
	  sqlite3WalkSelect(&w, pSelect);/*pSelect��ÿ�����ʽ������sqlite3WalkExpr�����ʽʹ�ûص�����*/
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
	** The Table structure that represents the result set was constructed
	** by selectExpander() but the type and collation information was omitted
	** at that point because identifiers had not yet been resolved.  This
	** routine is called after identifier resolution.
	*/
	/* ����Walker.xSelectCallback�ص�sqlite3SelectTypeInfo()�ӿڵġ�
	**
	** ����ÿ��FROM�Ӳ�ѯ��TABLE�ṹ��Column.zType Column.zColl�������Ӳ�ѯ�Ľ������
	**
	** TABLE�ṹ�����Ľ������selectExpander�����������������ͺ�������Ϣ�ᱻ������Ϊû�н�����ʶ����
	** ��������ڱ�ʶ��������ŵ��á�
	*/
	static int selectAddSubqueryTypeInfo(Walker *pWalker, Select *p){
	  Parse *pParse;/*����һ���﷨������*/
	  int i;
	  SrcList *pTabList;/*����һ��FROM�Ӿ���ʽ�б�*/
	  struct SrcList_item *pFrom;/*�������ʽ�б���*/

	  assert( p->selFlags & SF_Resolved );/*����ϵ㣬���SELECT��selFlags��ΪSF_Resolved���ѽ��������׳�������Ϣ*/
	  if( (p->selFlags & SF_HasTypeInfo)==0 ){/*���selFlags��ΪSF_HasTypeInfo*/
		p->selFlags |= SF_HasTypeInfo;/*��selFlags��SF_HasTypeInfoλ���ٸ�ֵ��selFlags*/
		pParse = pWalker->pParse;/*�������õ��﷨������*/
		pTabList = p->pSrc;/*��SELECT��FROM�Ӿ丳ֵ��FROM�Ӿ���ʽ�б�*/
		for(i=0, pFrom=pTabList->a; i<pTabList->nSrc; i++, pFrom++){/*����FROM�Ӿ���ʽ�б�*/
		  Table *pTab = pFrom->pTab;/*��FROM�Ӿ���ʽ�б��б�ֵ��pTab*/
		  if( ALWAYS(pTab!=0) && (pTab->tabFlags & TF_Ephemeral)!=0 ){/*���pTab��Ϊ�գ�����tabFlags����TF_Ephemeral����ʱ��*/
			/* A sub-query in the FROM clause of a SELECT *//*SELECT��FROM�Ӿ���Ӳ�ѯ*/
			Select *pSel = pFrom->pSelect;/*FROM�Ӿ���ʽ�б���SELECT��ֵ��pSel*/
			assert( pSel );/*����ϵ㣬���pSelΪ�գ��׳�������Ϣ*/
			while( pSel->pPrior ) pSel = pSel->pPrior;/*�ݹ�ó�SELECT�������Ȳ�ѯ���Ӳ�ѯSELECT*/
			selectAddColumnTypeAndCollation(pParse, pTab->nCol, pTab->aCol, pSel);/*����е����ͺ�У����Ϣ��SELECT*/
		  }
		}
	  }
	  return WRC_Continue;/*���ؼ���ִ�б��*/
	}
	#endif


	/*
	** This routine adds datatype and collating sequence information to
	** the Table structures of all FROM-clause subqueries in a
	** SELECT statement.
	**
	** Use this routine after name resolution.
	*/
	/* ��γ������datatype �� ����������Ϣ��SELECT���Ӳ�ѯ������FROM�Ӿ��TABLE�ṹ�С�
	** �����ֽ�����ʹ����γ���*/
	static void sqlite3SelectAddTypeInfo(Parse *pParse, Select *pSelect){
	#ifndef SQLITE_OMIT_SUBQUERY
	  Walker w;/*����һ��Walker�ṹ��*/
	  w.xSelectCallback = selectAddSubqueryTypeInfo;/*���ò�ѯ������Ϣ���ص�����*/
	  w.xExprCallback = exprWalkNoop;/*�����ʽ��Ϣ��ֵ���ص������ı��ʽ��*/
	  w.pParse = pParse;/*���﷨��������ֵ��w.pParse*/
	  sqlite3WalkSelect(&w, pSelect);/*pSelect�ṹ����ÿ�����ʽ������sqlite3WalkExpr��ÿ�����ʽ����ʹ�ûص�������*/
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
	/* ���������������SELECT�����������:
	**   * ��������е�FROM�Ӿ�Ԫ��VDBE�α��
	**   * ΪFROM�Ӿ��Ӳ�ѯ������ʱ�����
	**   * ON �� USING �Ӿ��л���WHERE���
	**   * �ڽ��������չͨ���"*" �� "TABLE.*"
	**   * �ñ��ʽ�ı�ʶ�����ֱ�
	** ��SELECT�еݹ�ִ�����е��Ӳ�ѯ
	*/
	void sqlite3SelectPrep(
	  Parse *pParse,         /* The parser context *//*����������*/
	  Select *p,             /* The SELECT statement being coded. *//*��дSELECT���*/
	  NameContext *pOuterNC  /* Name context for container *//*�������������������*/
	){
	  sqlite3 *db;/*����һ�����ݿ�����*/
	  if( NEVER(p==0) ) return;/*���SELECTΪ�գ�ֱ�ӷ���*/
	  db = pParse->db;/*���﷨�������е����ݿ⸳ֵ��db*/
	  if( p->selFlags & SF_HasTypeInfo ) return;/*���SELECT�ṹ���е�selFlagsΪSF_HasTypeInfo��ֱ�ӷ��ء���Ϊ�Ѻ�������Ϣ*/
	  sqlite3SelectExpand(pParse, p);/*����SELECT�е���չ����*/
	  if( pParse->nErr || db->mallocFailed ) return;/*������������д�����߷����ڴ����Ҳ����*/
	  sqlite3ResolveSelectNames(pParse, p, pOuterNC);/*����SELECT������������*/
	  if( pParse->nErr || db->mallocFailed ) return;/*�����������������д�����߷����ڴ����Ҳ����*/
	  sqlite3SelectAddTypeInfo(pParse, p);/*���SELECT������Ϣ���﷨��������*/
	}

	/*
	** Reset the aggregate accumulator.
	**
	** The aggregate accumulator is a set of memory cells that hold
	** intermediate results while calculating an aggregate.  This
	** routine generates code that stores NULLs in all of those memory
	** cells.
	*/
	/* ���þۼ�����
	** �ۼ��ۼ�����һϵ�б����м������ڴ浥Ԫ�ۼ�����Ľ������γ���Ϊ�˴洢�ڴ浥Ԫ�����еĿ�ֵ��
	*/
	static void resetAccumulator(Parse *pParse, AggInfo *pAggInfo){
	  Vdbe *v = pParse->pVdbe;/*���﷨�������е�pVdbe��ֵ��v*/
	  int i;
	  struct AggInfo_func *pFunc;/*����һ��AggInfo_func�ṹ�壨AggInfoΪ����SELECT�ۼ�������Ϣ�ģ�*/
	  if( pAggInfo->nFunc+pAggInfo->nColumn==0 ){/*���pAggInfo�ĺ�����������Ϊ0��ֱ�ӷ���*/
		return;
	  }
	  for(i=0; i<pAggInfo->nColumn; i++){/*������*/
		sqlite3VdbeAddOp2(v, OP_Null, 0, pAggInfo->aCol[i].iMem);/*��OP_Null��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  }
	  for(pFunc=pAggInfo->aFunc, i=0; i<pAggInfo->nFunc; i++, pFunc++){/*�����ۼ�����*/
		sqlite3VdbeAddOp2(v, OP_Null, 0, pFunc->iMem);/*��OP_Null��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		if( pFunc->iDistinct>=0 ){/*���ִ��DISTINCT����ʱ����ڵ���0*/
		  Expr *pE = pFunc->pExpr;/*��AggInfo_func�еı��ʽ��ֵ��pE*/
		  assert( !ExprHasProperty(pE, EP_xIsSelect) );/*����ϵ㣬���pE����EP_xIsSelect���׳�������Ϣ*/
		  if( pE->x.pList==0 || pE->x.pList->nExpr!=1 ){/*��������в���Ϊ0�����б��ʽ������Ϊ1*/
			sqlite3ErrorMsg(pParse, "DISTINCT aggregates must have exactly one "
			   "argument");/*�����������Ϣ���﷨��������DISTINCT aggregates must have exactly one*/
			pFunc->iDistinct = -1;/*��ִ��DISTINCT����ʱ��ֵΪ-1*/
		  }else{
			KeyInfo *pKeyInfo = keyInfoFromExprList(pParse, pE->x.pList);/*����һ�����ʽ�б�����һ���ؼ���Ϣ�ṹ�帳ֵ��pKeyInfo*/
			sqlite3VdbeAddOp4(v, OP_OpenEphemeral, pFunc->iDistinct, 0, 0,
							  (char*)pKeyInfo, P4_KEYINFO_HANDOFF);/*���һ��OP_OpenEphemeral����������������ֵ��Ϊһ��ָ��*/
		  }
		}
	  }
	}
	
	/*
	** Invoke the OP_AggFinalize opcode for every aggregate function 
	** in the AggInfo structure.
	*/
	/*
	** ΪAggInfo�ṹ����ÿһ���ۼ���������OP_AggFinalize����ɣ�����*/
	static void finalizeAggFunctions(Parse *pParse, AggInfo *pAggInfo){
	  Vdbe *v = pParse->pVdbe;/*���﷨�������е�pVdbe��ֵ��v*/
	  int i;
	  struct AggInfo_func *pF;/*����һ���ۼ��������ܽṹ�壨AggInfo_funcΪAggInfo�ӽṹ�壩*/
	  for(i=0, pF=pAggInfo->aFunc; i<pAggInfo->nFunc; i++, pF++){/*�����ۼ�����*/
		ExprList *pList = pF->pExpr->x.pList;/*���ۼ������ı��ʽ��ֵ��pList*/
		assert( !ExprHasProperty(pF->pExpr, EP_xIsSelect) );/*����ϵ㣬���pE����EP_xIsSelect���׳�������Ϣ*/
		sqlite3VdbeAddOp4(v, OP_AggFinal, pF->iMem, pList ? pList->nExpr : 0, 0,
						  (void*)pF->pFunc, P4_FUNCDEF);/*���һ��OP_OpenEphemeral����������������ֵ��Ϊһ��ָ��*/
	  }
	}

	/*
	** Update the accumulator memory cells for an aggregate based on
	** the current cursor position.
	*//*Ϊ��ǰ�α�λ���ϵľۼ����������ۼ����ڴ浥Ԫ*/
	static void updateAccumulator(Parse *pParse, AggInfo *pAggInfo){
	  Vdbe *v = pParse->pVdbe;/*���﷨�������е�pVdbe��ֵ��v*/
	  int i;
	  int regHit = 0
	  int addrHitTest = 0
	  struct AggInfo_func *pF;
	  struct AggInfo_col *pC

	  pAggInfo->directMode = 1;/*��ֱ�Ӵ�Դ���ȡ����������Ϊ1*/
	  sqlite3ExprCacheClear(pParse);/*��������е��﷨������*/
	  for(i=0, pF=pAggInfo->aFunc; i<pAggInfo->nFunc; i++, pF++){/*�����ۼ�����*/
		int nArg;/*�ۼ������ĸ���*/
		int addrNext = 0;/*��һ����ַ*/
		int regAgg;/*�洢�ۼ������Ĵ������*/
		ExprList *pList = pF->pExpr->x.pList;/*���ۼ��������ܵı��ʽ�б�ֵ��pList*/
		assert( !ExprHasProperty(pF->pExpr, EP_xIsSelect) );/*����ϵ㣬���pE��EP_xIsSelect���׳�������Ϣ*/
		if( pList ){/*����洢���ʽ�б�Ϊ��*/
		  nArg = pList->nExpr;/*�����ʽ������ֵ���ۼ���������*/
		  regAgg = sqlite3GetTempRange(pParse, nArg);/*Ϊ�ۼ���������Ĵ���*/
		  sqlite3ExprCodeExprList(pParse, pList, regAgg, 1);/*�ѱ��ʽ�б��е�ֵ�ŵ�һϵ�еļĴ�����*/
		}else{
		  nArg = 0;/*���ۼ�����������0*/
		  regAgg = 0;/*�洢�ۼ������Ĵ�����0*/
		}
		if( pF->iDistinct>=0 ){/*���ֱ�Ӵ�Դ����ȡֵ�ĸ���Ϊ0*/
		  addrNext = sqlite3VdbeMakeLabel(v);/*ΪVDBE����һ����ǩ������ֵ��ֵ��addrNext����һ�����е�ַ��*/
		  assert( nArg==1 );/*����ϵ㣬����ۼ�������Ϊ1���׳�������Ϣ*/
		  codeDistinct(pParse, pF->iDistinct, addrNext, 1, regAgg);/*�����ʽ�б�ֻ��һ�������Ŀ*/
		}
		if( pF->pFunc->flags & SQLITE_FUNC_NEEDCOLL ){/*����ۼ�������flagsΪSQLITE_FUNC_NEEDCOLL*/
		  CollSeq *pColl = 0;
		  struct ExprList_item *pItem;
		  int j;
		  assert( pList!=0 );  /* pList!=0 if pF->pFunc has NEEDCOLL *//*���pF->pFunc��NEEDCOLL����pList��Ϊ0*/
		  for(j=0, pItem=pList->a; !pColl && j<nArg; j++, pItem++){/*�����ۼ�����*/
			pColl = sqlite3ExprCollSeq(pParse, pItem->pExpr);/*����һ��Ĭ�ϵ���������*/
		  }
		  if( !pColl ){/*����������в�Ϊ0*/
			pColl = pParse->db->pDfltColl;/*�����ݿ������е�Ĭ�ϵ��������и�ֵ��pColl*/
		  }
		  if( regHit==0 && pAggInfo->nAccumulator ) regHit = ++pParse->nMem;/*���regHitΪ0�����ۼ�����Ϊ0���Ӽ��ڴ浥Ԫ����֮���ٸ�ֵ��regHit*/
		  sqlite3VdbeAddOp4(v, OP_CollSeq, regHit, 0, 0, (char *)pColl, P4_COLLSEQ);/*���һ��OP_CollSeq��������������ֵ��Ϊһ��ָ��*/
		}
		sqlite3VdbeAddOp4(v, OP_AggStep, 0, regAgg, pF->iMem,
						  (void*)pF->pFunc, P4_FUNCDEF);/*���һ��OP_AggStep��������������ֵ��Ϊһ��ָ��*/
		sqlite3VdbeChangeP5(v, (u8)nArg);/*��nArg����Ϊ������ʹ�õ�ֵ*/
		sqlite3ExprCacheAffinityChange(pParse, regAgg, nArg);/*�����﷨��pParse���Ĵ����оۼ��������׺�������*/
		sqlite3ReleaseTempRange(pParse, regAgg, nArg);/*�ͷ�regPrev��������Ĵ�����������nArg*/
		if( addrNext ){/*�������һ��ִ�е�ַ*/
		  sqlite3VdbeResolveLabel(v, addrNext);/*����addrNext������Ϊ��һ��������ĵ�ַ*/
		  sqlite3ExprCacheClear(pParse);/*��������е��﷨������*/
		}
	  }

	  /* Before populating the accumulator registers, clear the column cache.
	  ** Otherwise, if any of the required column values are already present 
	  ** in registers, sqlite3ExprCode() may use OP_SCopy to copy the value
	  ** to pC->iMem. But by the time the value is used, the original register
	  ** may have been used, invalidating the underlying buffer holding the
	  ** text or blob value. See ticket [883034dcb5].
	  **
	  ** Another solution would be to change the OP_SCopy used to copy cached
	  ** values to an OP_Copy.
	  *//*�ڻ�ȡ�ۼӼĴ������ڴ�֮ǰ������л��档���������Ҫ����ֵ�Ѿ��洢�ڼĴ����У�sqlite3ExprCode
	  ** ���ܻ�ʹ��OP_SCopy����copy���е�ֵ��pC->iMem�������ֵ�Ѿ���ʹ�ù�������ļĴ���Ҳ���ù���Ǳ��
	  ** �������б�����ı���ֵ����Ч��*/
	  if( regHit ){
		addrHitTest = sqlite3VdbeAddOp1(v, OP_If, regHit);/*��OP_Yield��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  }
	  sqlite3ExprCacheClear(pParse);/*��������е��﷨������*/
	  for(i=0, pC=pAggInfo->aCol; i<pAggInfo->nAccumulator; i++, pC++){/*�����ۼ���*/
		sqlite3ExprCode(pParse, pC->pExpr, pC->iMem);/*��pExpr���ʽ��iMemֵ��ŵ��﷨��������*/
	  }
	  pAggInfo->directMode = 0;/*��ֱ�Ӵ�Դ���ȡ����������Ϊ0*/
	  sqlite3ExprCacheClear(pParse);/*��������е��﷨������*/
	  if( addrHitTest ){/*���addrHitTest��Ϊ��*/
		sqlite3VdbeJumpHere(v, addrHitTest);/*���addrHitTest��Ϊ0�����е�ַ������ǰ��ַ*/
	  }
	}

	/*
	** Add a single OP_Explain instruction to the VDBE to explain a simple
	** count(*) query ("SELECT count(*) FROM pTab").
	*//*���һ��������OP_Explainָ���VDBE����һ��������count(*)��ѯ("SELECT count(*) FROM pTab").*/
	#ifndef SQLITE_OMIT_EXPLAIN
	static void explainSimpleCount(
	  Parse *pParse,                  /* Parse context *//*����������*/
	  Table *pTab,                    /* Table being queried *//*����ѯ�ı�*/
	  Index *pIdx                     /* Index used to optimize scan, or NULL *//*�����Ż�ɨ�������*/
	){
	  if( pParse->explain==2 ){/*����﷨��������explain���ʽΪ2*/
		char *zEqp = sqlite3MPrintf(pParse->db, "SCAN TABLE %s %s%s(~%d rows)",
			pTab->zName, 
			pIdx ? "USING COVERING INDEX " : "",
			pIdx ? pIdx->zName : "",
			pTab->nRowEst
		);/*��ӡ���������ֵ��zEqp������%s%s%sΪ����ı�����*/
		sqlite3VdbeAddOp4(
			pParse->pVdbe, OP_Explain, pParse->iSelectId, 0, 0, zEqp, P4_DYNAMIC
		);/*���һ��OP_Explain��������������ֵ��Ϊһ��ָ��*/
	  }
	}
	#else
	# define explainSimpleCount(a,b,c)
	#endif

	/*
	** Generate code for the SELECT statement given in the p argument.  
	**
	** The results are distributed in various ways depending on the
	** contents of the SelectDest structure pointed to by argument pDest
	** as follows:
	**
	**     pDest->eDest    Result
	**     ------------    -------------------------------------------
	**     SRT_Output      Generate a row of output (using the OP_ResultRow
	**                     opcode) for each row in the result set.
	**
	**     SRT_Mem         Only valid if the result is a single column.
	**                     Store the first column of the first result row
	**                     in register pDest->iSDParm then abandon the rest
	**                     of the query.  This destination implies "LIMIT 1".
	**
	**     SRT_Set         The result must be a single column.  Store each
	**                     row of result as the key in table pDest->iSDParm. 
	**                     Apply the affinity pDest->affSdst before storing
	**                     results.  Used to implement "IN (SELECT ...)".
	**
	**     SRT_Union       Store results as a key in a temporary table 
	**                     identified by pDest->iSDParm.
	**
	**     SRT_Except      Remove results from the temporary table pDest->iSDParm.
	**
	**     SRT_Table       Store results in temporary table pDest->iSDParm.
	**                     This is like SRT_EphemTab except that the table
	**                     is assumed to already be open.
	**
	**     SRT_EphemTab    Create an temporary table pDest->iSDParm and store
	**                     the result there. The cursor is left open after
	**                     returning.  This is like SRT_Table except that
	**                     this destination uses OP_OpenEphemeral to create
	**                     the table first.
	**
	**     SRT_Coroutine   Generate a co-routine that returns a new row of
	**                     results each time it is invoked.  The entry point
	**                     of the co-routine is stored in register pDest->iSDParm.
	**
	**     SRT_Exists      Store a 1 in memory cell pDest->iSDParm if the result
	**                     set is not empty.
	**
	**     SRT_Discard     Throw the results away.  This is used by SELECT
	**                     statements within triggers whose only purpose is
	**                     the side-effects of functions.
	**
	** This routine returns the number of errors.  If any errors are
	** encountered, then an appropriate error message is left in
	** pParse->zErrMsg.
	**
	** This routine does NOT free the Select structure passed in.  The
	** calling function needs to do that.
	*/
	/* Ϊ����p������дSELECT��䡣 
	** ����ֲ��ڲ�ͬ��SelectDest�ṹĿ¼ָ���У����£�
	**     pDest->eDest    Result
	**     ------------    -------------------------------------------
	**     SRT_Output      Ϊ�������ÿһ�в���һ�����(ʹ��OP_ResultRow����
	**                     opcode)
	**
	**     SRT_Mem         ������ֻ��һ��������Ҳ�ǿ��õġ��洢��һ������еĵ�һ���ڼĴ���pDest->iSDParm
	**                     Ȼ������ʣ��Ĳ�ѯ��Ϊ��ʵ��"LIMIT 1".
	**                     
	**                    
	**
	**     SRT_Set         ���������һ�������С��洢�����ÿһ����Ϊ���ڱ�pDest->iSDParm�С�
	**                     Ӧ��������pDest->affSdst�ڴ洢���ǰ��Ϊ��ʵ��"IN (SELECT ...)".
	**                   
	**     SRT_Union       �洢�����Ϊһ���ɱ�ʶ��ļ�����ʱ�� pDest->iSDParm �С�
	**                     
	**
	**     SRT_Except      ����ʱ��pDest->iSDParmɾ�����
	**
	**     SRT_Table       �洢�������ʱ��pDest->iSDParm�С������ SRT_EphemTab���˼���������Ѿ�
	**                     ���򿪡�
	**                    
	**     SRT_EphemTab    ����һ����ʱ��pDest->iSDParm����������洢���������α��Ƿ���֮����ߴ򿪡�
	**                     �����SRT_Table����Ҫʹ��OP_OpenEphemeral�ȴ���һ����
	**                     
	**     SRT_Coroutine   ����һ��Э������ÿ�α����õ�ʱ�򣬶�����һ���µĽ�������Э���������Ŀָ��
	**                     ���ڼĴ���pDest->iSDParm�С�
	**                    
	**     SRT_Exists      �洢һ��1���ڴ浥ԪpDest->iSDParm �У�����������Ϊ�ա�
	**                     
	**     SRT_Discard     �׵����������һ������������SELECT���ʹ�ã�����������������ĸ������ء�
	**                     
	** ��γ��򷵻ش���ĸ�������������κδ���һ��������Ϣ�������pParse->zErrMsg�С�                   
	**
	** ��γ��򲢲��ͷ�SELECT�ṹ�塣���õĺ�����Ҫ�ͷ�SELECT�ṹ�塣
	*/
	int sqlite3Select(
	  Parse *pParse,         /* The parser context *//*����������*/
	  Select *p,             /* The SELECT statement being coded. *//*��дSELECT���*/
	  SelectDest *pDest      /* What to do with the query results *//*�����ѯ���*/
	){
	  int i, j;              /* Loop counters *//*ѭ��������*/
	  WhereInfo *pWInfo;     /* Return from sqlite3WhereBegin() *//*��sqlite3WhereBegin����*/
	  Vdbe *v;               /* The virtual machine under construction *//*���ڹ����������*/
	  int isAgg;             /* True for select lists like "count(*)" *//*�������count(*)�Ĳ�ѯ�б�Ϊtrue*/
	  ExprList *pEList;      /* List of columns to extract. *//*��ȡ��������ɵ��б�*/
	  SrcList *pTabList;     /* List of tables to select from *//*SELECT����Դ��*/
	  Expr *pWhere;          /* The WHERE clause.  May be NULL *//*WHERE�Ӿ䣬�����ǿ�*/
	  ExprList *pOrderBy;    /* The ORDER BY clause.  May be NULL *//* ORDER BY �Ӿ䣬�����ǿ�*/
	  ExprList *pGroupBy;    /* The GROUP BY clause.  May be NULL *//* GROUP BY �Ӿ䣬�����ǿ�*/
	  Expr *pHaving;         /* The HAVING clause.  May be NULL *//*HAVING�Ӿ䣬�����ǿ�*/
	  int isDistinct;        /* True if the DISTINCT keyword is present *//*����DISTINCT�ؼ���Ϊtrue*/
	  int distinct;          /* Table to use for the distinct set *//*���в��ظ�����*/
	  int rc = 1;            /* Value to return from this function *//*�����ķ���ֵ*/
	  int addrSortIndex;     /* Address of an OP_OpenEphemeral instruction *//*OP_OpenEphemeralָ���ַ*/
	  int addrDistinctIndex; /* Address of an OP_OpenEphemeral instruction *//*OP_OpenEphemeralָ���ַ*/
	  AggInfo sAggInfo;      /* Information used by aggregate queries *//*�ۼ�������Ϣ*/
	  int iEnd;              /* Address of the end of the query *//*������ѯ�ĵ�ַ*/
	  sqlite3 *db;           /* The database connection *//*���ݿ�����*/

	#ifndef SQLITE_OMIT_EXPLAIN
	  int iRestoreSelectId = pParse->iSelectId;/*���﷨�������в���ID�洢��iRestoreSelectId��*/
	  pParse->iSelectId = pParse->iNextSelectId++;/*�ٽ��﷨����������һ������ID�洢��iSelectId��*/
	#endif

	  db = pParse->db;/*����һ�����ݿ�����*/
	  if( p==0 || db->mallocFailed || pParse->nErr ){/*���SELECTΪ�ջ�����ڴ�ʧ�ܻ��﷨���������д���*/
		return 1;/*ֱ�ӷ���1*/
	  }
	  if( sqlite3AuthCheck(pParse, SQLITE_SELECT, 0, 0, 0) ) return 1;/*���Ȩ�޼�⣬�д���Ҳ�Ƿ���1*/
	  memset(&sAggInfo, 0, sizeof(sAggInfo));/*��sAggInfo��ǰsizeof(sAggInfo)���ֽ���0�滻*/

	  if( IgnorableOrderby(pDest) ){/*��������ѯ�������û��ORDERBY*/
		assert(pDest->eDest==SRT_Exists || pDest->eDest==SRT_Union || /*����ϵ㣬������������д���ʽΪSRT_Exists��SRT_Union*/
			   pDest->eDest==SRT_Except || pDest->eDest==SRT_Discard);/*��SRT_Except��SRT_Discard�������û�У����׳�������Ϣ*/
		/* If ORDER BY makes no difference in the output then neither does
		** DISTINCT so it can be removed too. *//*���ORDER BY��Ӱ�����Ȼ��DISTINCT�ᱻ�Ƴ�*/
		sqlite3ExprListDelete(db, p->pOrderBy);/*ɾ�����ݿ������е�pOrderBy���ʽ*/
		p->pOrderBy = 0;/*��pOrderBy��Ϊ0*/
		p->selFlags &= ~SF_Distinct;/*selFlags��~SF_Distinct�ٸ�ֵ��selFlags*/
	  }
	  sqlite3SelectPrep(pParse, p, 0);/*ִ��SELECT pǰԤ����*/
	  pOrderBy = p->pOrderBy;/*��SELECT��pOrderBy��ֵ����ǰ����pOrderBy*/
	  pTabList = p->pSrc;/*��SELECT��FROM�Ӿ丳ֵ��pTabList*/
	  pEList = p->pEList;/*��SELECT�б��ʽ�б�ֵ��pEList*/
	  if( pParse->nErr || db->mallocFailed ){/*����﷨���������д��󣬻�����ڴ����*/
		goto select_end;/*����select_end����ѯ������*/
	  }
	  isAgg = (p->selFlags & SF_Aggregate)!=0;/*����selFlags�ж��Ƿ��Ǿۼ������������isAggΪtrue������ΪFALSE*/
	  assert( pEList!=0 );/*����ϵ㣬������ʽ�б�Ϊ�գ��׳�������Ϣ*/

	  /* Begin generating code.
	  *//*��ʼ���ɴ���*/
	  v = sqlite3GetVdbe(pParse);/*�����﷨����������һ���������ݿ�����*/
	  if( v==0 ) goto select_end;/*���û�����ɣ�����select_end����ѯ������*/

	  /* If writing to memory or generating a set
	  ** only a single column may be output.
	  *//*���д���ڴ������ֻ��һ�������м���Ҳ���ܱ����*/
	#ifndef SQLITE_OMIT_SUBQUERY
	  if( checkForMultiColumnSelectError(pParse, pDest, pEList->nExpr) ){/*�����⵽��ά�в�ѯ����*/
		goto select_end;/*����select_end����ѯ������*/
	  }
	#endif

	  /* Generate code for all sub-queries in the FROM clause
	  *//*��дFROM�Ӿ��������Ӳ�ѯ����*/
	#if !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW)
	  for(i=0; !p->pPrior && i<pTabList->nSrc; i++){/*����FROM�Ӿ���ʽ�б�*/
		struct SrcList_item *pItem = &pTabList->a[i];/*�����ʽ�б��б��ʽ�ֵ��pItem*/
		SelectDest dest;/*����һ�����������ṹ��*/
		Select *pSub = pItem->pSelect;/*�����ʽ�б�����SELECT�ṹ�帳ֵ��pSub*/
		int isAggSub;/*�����ۼ�������Ϣ*/

		if( pSub==0 ) continue;/*���SELECT�ṹ��pSubΪ0�������˴�ѭ�����´�ѭ��*/
		if( pItem->addrFillSub ){/*�������Ӳ�ѯ��ַ*/
		  sqlite3VdbeAddOp2(v, OP_Gosub, pItem->regReturn, pItem->addrFillSub);/*��OP_Gosub��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  continue;
		}

		/* Increment Parse.nHeight by the height of the largest expression
		** tree refered to by this, the parent select. The child select
		** may contain expression trees of at most
		** (SQLITE_MAX_EXPR_DEPTH-Parse.nHeight) height. This is a bit
		** more conservative than necessary, but much easier than enforcing
		** an exact limit.
		*/
		/*����Parse.nHeight���߶ȣ��������Ը���ѯ�ĵ������ʽ���ĸ߶ȣ�Ҳ���Ӳ�ѯ��Ҳ�С�
		**����ֻѡ����ѯ�ģ��������Щ���أ����Ǹ����ס�
		*/
		pParse->nHeight += sqlite3SelectExprHeight(p);/*�鵽p�����ģ��ŵ�pParse->nHeight��*/

		isAggSub = (pSub->selFlags & SF_Aggregate)!=0;/*���selFlagsΪSF_Aggregate�����ۼ�������Ϣ����isAggSub*/
		if( flattenSubquery(pParse, p, i, isAgg, isAggSub) ){/*���Ա�ƽ�����ʽ*/
		  /* This subquery can be absorbed into its parent. *//*����Ӳ�ѯ�ܲ��뵽����ѯ��*/
		  if( isAggSub ){/*����ۼ��Ӻ�����Ϣ����*/
			isAgg = 1;/*���ۼ�������Ϊ1*/
			p->selFlags |= SF_Aggregate;/*��selFlagsλ��SF_Aggregate�ٸ�ֵ��selFlags*/
		  }
		  i = -1;/*��i��Ϊ-1����һ��ѭ��i��0��ʼ*/
		}else{
		  /* Generate a subroutine that will fill an ephemeral table with
		  ** the content of this subquery.  pItem->addrFillSub will point
		  ** to the address of the generated subroutine.  pItem->regReturn
		  ** is a register allocated to hold the subroutine return address
		  *//*����һ���ӳ���д�����Ӳ�ѯ����ʱ���С�pItem->addrFillSub����ָ���ӳ���ĵ�ַ��
		  **pItem->regReturn��һ���Ĵ��������ӳ���ķ��ص�ַ*/
		  int topAddr;/*������ʼ��ַ*/
		  int onceAddr = 0;
		  int retAddr;/*�������õ�ַ*/
		  assert( pItem->addrFillSub==0 );/*����ϵ㣬���pItem->addrFillSub��Ϊ�գ��׳�������Ϣ*/
		  pItem->regReturn = ++pParse->nMem;/*��������ڴ��С��1��ֵ��regReturn*/
		  topAddr = sqlite3VdbeAddOp2(v, OP_Integer, 0, pItem->regReturn);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ����ֵ��topAddr*/
		  pItem->addrFillSub = topAddr+1;/*��ʼ��ַ��1�ٸ�ֵ��addrFillSub��ָ���ӳ����ַ��*/
		  VdbeNoopComment((v, "materialize %s", pItem->pTab->zName));/*�����ʾ��Ϣ*/
		  if( pItem->isCorrelated==0 ){/*���û�й�����*/
			/* If the subquery is no correlated and if we are not inside of 
			** a trigger, then we only need to compute the value of the subquery
			** once. *//*�������Ӳ�ѯû�й���������ѯ����û���ڲ���������Ȼ������ֻ��Ҫһ���Լ����Ӳ�ѯ��ֵ*/
			onceAddr = sqlite3CodeOnce(pParse);/*���﷨��������������һ���Լ����Ӳ�ѯ��ֵ�ĵ�ַ�ŵ�onceAddr*/
		  }
		  sqlite3SelectDestInit(&dest, SRT_EphemTab, pItem->iCursor);/*��ʼ�����������洢��SRT_EphemTab���α��ΪiCursor*/
		  explainSetInteger(pItem->iSelectId, (u8)pParse->iNextSelectId);/*���﷨��������pParse��iNextSelectId��ֵ��pItem->iSelectId*/
		  sqlite3Select(pParse, pSub, &dest);/*ʹ�������������ѯ*/
		  pItem->pTab->nRowEst = (unsigned)pSub->nSelectRow;/*��SELECT�ṹ���в����н����ֵ�����ʽ�б����еı������*/
		  if( onceAddr ) sqlite3VdbeJumpHere(v, onceAddr);/*���onceAddr��Ϊ0�����е�ַ������ǰ��ַ*/
		  retAddr = sqlite3VdbeAddOp1(v, OP_Return, pItem->regReturn);/*��OP_Return��������vdbe��Ȼ�󷵻���������ĵ�ַ����ֵ�oretAddr*/
		  VdbeComment((v, "end %s", pItem->pTab->zName));/*����end pItem->pTab->zName�����뵽���ݿ���*/
		  sqlite3VdbeChangeP1(v, topAddr, retAddr);/*��topAddr��ַ��ΪretAddr*/
		  sqlite3ClearTempRegCache(pParse);/*����Ĵ������﷨������*/
		}
		if( /*pParse->nErr ||*/ db->mallocFailed ){/*��������ڴ����*/
		  goto select_end;/*����select_end����ѯ������*/
		}
		pParse->nHeight -= sqlite3SelectExprHeight(p);/*�鵽p���������߶ȣ��ŵ�pParse->nHeight��*/
		pTabList = p->pSrc;/*��FROM�Ӿ���ʽ�б�ŵ�pTabList*/
		if( !IgnorableOrderby(pDest) ){/*������������в�����Orderby*/
		  pOrderBy = p->pOrderBy;/*��SELECT�ṹ����Orderby���Ը�ֵ��pOrderBy*/
		}
	  }
	  pEList = p->pEList;/*���ṹ����ʽ�б�ֵ��pEList*/
	#endif
	  pWhere = p->pWhere;/*SELECT�ṹ����WHERE�Ӿ丳ֵ��pWhere*/
	  pGroupBy = p->pGroupBy;/*ELECT�ṹ����GROUP BY�Ӿ丳ֵ��pGroupBy*/
	  pHaving = p->pHaving;/*ELECT�ṹ����Having�Ӿ丳ֵ��pHaving*/
	  isDistinct = (p->selFlags & SF_Distinct)!=0;/*�������DISTINCT�ؼ�����Ϊtrue*/

	#ifndef SQLITE_OMIT_COMPOUND_SELECT
	  /* If there is are a sequence of queries, do the earlier ones first.
	  *//*�����һ����ѯ���У���ִ�бȽ����׵Ĳ�ѯ����*/
	  if( p->pPrior ){/*���SELECT�������Ȳ�ѯSELECT*/
		if( p->pRightmost==0 ){/*���SELECT��û��������*/
		  Select *pLoop, *pRight = 0;
		  int cnt = 0;
		  int mxSelect
		  for(pLoop=p; pLoop; pLoop=pLoop->pPrior, cnt++){/*����SELECT�����Ȳ���SELECT���ҵ������Ȳ���SELECT*/
			pLoop->pRightmost = p;/*��SELECT��ֵ��������*/
			pLoop->pNext = pRight;/*����������ֵ����һ���ڵ�*/
			pRight = pLoop;/*���м��ӽڵ㸳ֵ��������*/
		  }
		  mxSelect = db->aLimit[SQLITE_LIMIT_COMPOUND_SELECT];/*�����ϲ�ѯ�Ĳ�ѯ�������ظ�mxSelect*/
		  if( mxSelect && cnt>mxSelect ){/*���mxSelect���ڣ�������ȴ���SELECT����*/
			sqlite3ErrorMsg(pParse, "too many terms in compound SELECT");/*���﷨�������д洢too many terms in compound SELECT*/
			goto select_end;/*����select_end����ѯ������*/
		  }
		}
		rc = multiSelect(pParse, p, pDest);/*�����������������������ظ�rc*/
		explainSetInteger(pParse->iSelectId, iRestoreSelectId);/*��iRestoreSelectId(��һ��SELECT��ID)��ֵ��pParse->iSelectId*/
		return rc;/*����ִ������*/
	  }
	#endif

	  /* If there is both a GROUP BY and an ORDER BY clause and they are
	  ** identical, then disable the ORDER BY clause since the GROUP BY
	  ** will cause elements to come out in the correct order.  This is
	  ** an optimization - the correct answer should result regardless.
	  ** Use the SQLITE_GroupByOrder flag with SQLITE_TESTCTRL_OPTIMIZER
	  ** to disable this optimization for testing purposes.
	  *//*�����GROUP BY �� ORDER BY�Ӿ䣬����������һ�µģ���ִ��GROUP BY��ִ��ORDER BY.
	  ** ����һ���Ż�����Ӱ����ȷ�Ľ����ʹ�ô�SQLITE_TESTCTRL_OPTIMIZER��SQLITE_GroupByOrder���
	  ** �ڲ���Ŀ���в�ʹ���Ż���
	  */
	  if( sqlite3ExprListCompare(p->pGroupBy, pOrderBy)==0/*������������ʽһ�²�flagsֵΪSQLITE_GroupByOrder*/
			 && (db->flags & SQLITE_GroupByOrder)==0 ){
		pOrderBy = 0;/*��pOrderBy������Ϊ0*/
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
	  *//*�����һ��ORDER BY��DISTINCT�����ۼ������Ĳ�ѯ��Ȼ�������ѯ�б��ORDER BY�б���ͬ��Ȼ�������ѯ��
	  ** ��дΪGROUP BY�����仰˵��
	  **    SELECT DISTINCT xyz FROM ... ORDER BY xyz
	  ** ת��Ϊ��
	  **    SELECT xyz FROM ... GROUP BY xyz
	  ** �ڶ�����ʽ���ã�һ��������������ʱ�����������ܴ��� ORDER BY �� DISTINCT�����д��ѯ����ʹ����ʱ����
	  ** ���ORDER BY �� DISTINCT������һ��������һ��������ֿ���һ����ʱ�������һ����*/
	  if( (p->selFlags & (SF_Distinct|SF_Aggregate))==SF_Distinct 
	   && sqlite3ExprListCompare(pOrderBy, p->pEList)==0/*���SELECT��selFlagsΪSF_Distinct��SF_Aggregate�����ұ��ʽһֱ*/
	  ){
		p->selFlags &= ~SF_Distinct;/*selFlags��~SF_Distinct�ٸ�ֵ��selFlags*/
		p->pGroupBy = sqlite3ExprListDup(db, p->pEList, 0);/*���copy p->pEList��p->pGroupBy*/
		pGroupBy = p->pGroupBy;/*��SELECT��pGroupBy��ֵ��pGroupBy*/
		pOrderBy = 0;/*��pOrderBy��0*/
	  }

	  /* If there is an ORDER BY clause, then this sorting
	  ** index might end up being unused if the data can be 
	  ** extracted in pre-sorted order.  If that is the case, then the
	  ** OP_OpenEphemeral instruction will be changed to an OP_Noop once
	  ** we figure out that the sorting index is not needed.  The addrSortIndex
	  ** variable is used to facilitate that change.
	  *//*�����ORDER BY�Ӿ�������ݱ���ǰ���������������ܲ��á�����������OP_OpenEphemeralָ���ı�
	  **OP_Noop��һ����������Ҫ����������addrSortIndex�������ڰ����ı䡣*/
	  if( pOrderBy ){/*�������ORDERBY�Ӿ�*/
		KeyInfo *pKeyInfo;/*����һ���ؼ���Ϣ�ṹ��*/
		pKeyInfo = keyInfoFromExprList(pParse, pOrderBy);/*�����ʽpOrderBy�ŵ��ؼ���Ϣ�ṹ����*/
		pOrderBy->iECursor = pParse->nTab++;/*�����������1�ŵ��α���*/
		p->addrOpenEphm[2] = addrSortIndex =
		  sqlite3VdbeAddOp4(v, OP_OpenEphemeral,
							   pOrderBy->iECursor, pOrderBy->nExpr+2, 0,
							   (char*)pKeyInfo, P4_KEYINFO_HANDOFF);/*���һ��OP_Explain��������������ֵ��Ϊһ��ָ��*/
	  }else{
		addrSortIndex = -1;/*��������������Ϊ-1*/
	  }

	  /* If the output is destined for a temporary table, open that table.
	  *//*������Ԥ�ȵ�һ����ʱ��Ҫ�򿪱�*/
	  if( pDest->eDest==SRT_EphemTab ){/*�������Ķ���ΪSRT_EphemTab*/
		sqlite3VdbeAddOp2(v, OP_OpenEphemeral, pDest->iSDParm, pEList->nExpr);/*��OP_OpenEphemeral��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
	  }

	  /* Set the limiter.
	  *//*����������*/
	  iEnd = sqlite3VdbeMakeLabel(v);/*ΪVDBE����һ����ǩ��ָ���������еĵ�ַ��������ֵ��ֵ��iEnd*/
	  p->nSelectRow = (double)LARGEST_INT64;/*��SELECT�������Ϊ64λ*/
	  computeLimitRegisters(pParse, p, iEnd);/*����Limit�Ĵ������洢SELECT*/
	  if( p->iLimit==0 && addrSortIndex>=0 ){/*���limitΪ0���������������ڵ���0*/
		sqlite3VdbeGetOp(v, addrSortIndex)->opcode = OP_SorterOpen;/*������OP_SorterOpen���򿪷ּ���������������������VDBE*/
		p->selFlags |= SF_UseSorter;/*��selFlagsλ��SF_UseSorter���ٸ�ֵ��selFlags*/
	  }

	  /* Open a virtual index to use for the distinct set.
	  *//*ʹ�ò��ظ����ϴ�һ����������*/
	  if( p->selFlags & SF_Distinct ){/*���selFlags��ֵΪSF_Distinct*/
		KeyInfo *pKeyInfo;/*����һ���ؼ���Ϣ�ṹ��*/
		distinct = pParse->nTab++;/*�����������1��ֵ��distinct*/
		pKeyInfo = keyInfoFromExprList(pParse, p->pEList);/*�����ʽ�б�p->pEList�ŵ��ؼ���Ϣ�ṹ����*/
		addrDistinctIndex = sqlite3VdbeAddOp4(v, OP_OpenEphemeral, distinct, 0, 0,
			(char*)pKeyInfo, P4_KEYINFO_HANDOFF);/*���һ��OP_OpenEphemeral��������������ֵ��Ϊһ��ָ�벢��ֵ��addrDistinctIndex*/
		sqlite3VdbeChangeP5(v, BTREE_UNORDERED);/*��BTREE_UNORDERED����Ϊ������ʹ�õ�ֵ*/
	  }else{
		distinct = addrDistinctIndex = -1;/*����distinct��addrDistinctIndex��Ϊ0*/
	  }
          /*������ע���ۼ��ͷǾۼ���ѯ����ʽ��ͬ��*/
	  /* Aggregate and non-aggregate queries are handled differently *//*�ۼ��ͷǾۼ���ѯ����ʽ��ͬ*/
	  if( !isAgg && pGroupBy==0 ){/*���isAggû��GroupBy*/
		ExprList *pDist = (isDistinct ? p->pEList : 0);/*�ж�isDistinct�Ƿ�Ϊtrue������p->pEList��ֵ��pDist*/

		/* Begin the database scan. *//*��ʼ���ݿ�ɨ��*/
		pWInfo = sqlite3WhereBegin(pParse, pTabList, pWhere, &pOrderBy, pDist, 0,0);/*��ʼѭ��ִ��WHERE����䣬�����ؽ���ָ��*/
		if( pWInfo==0 ) goto select_end;/*�������ָ��Ϊ0��������ѯ����*/
		if( pWInfo->nRowOut < p->nSelectRow ) p->nSelectRow = pWInfo->nRowOut;/*�����ѯ����ָ���н�����С�ڽṹ���в�ѯ�У��������и�ֵ����ѯ��*/

		/* If sorting index that was created by a prior OP_OpenEphemeral 
		** instruction ended up not being needed, then change the OP_OpenEphemeral
		** into an OP_Noop.
		*//*����������������� OP_OpenEphemeral ָ������������ǲ���Ҫ�ģ�Ȼ��ı�OP_OpenEphemeralΪOP_Noop*/
		if( addrSortIndex>=0 && pOrderBy==0 ){/*��������������ڲ���ORDERBYΪ0*/
		  sqlite3VdbeChangeToNoop(v, addrSortIndex);/*��addrSortIndex�в�����ΪOP_Noop*/
		  p->addrOpenEphm[2] = -1;/*��SELECT�ṹ�����ʱ����±�Ϊ2��Ԫ����Ϊ-1*/
		}

		if( pWInfo->eDistinct ){/*������ص�WHERE��Ϣ�к�DISTINCT��ѯ*/
		  VdbeOp *pOp;                /* No longer required OpenEphemeral instr. */
		 /*������ҪOpenEphemeral������ʱ��instr��*/
		  assert( addrDistinctIndex>=0 );/*����ϵ㣬���addrDistinctIndexС��0���׳�������Ϣ*/
		  pOp = sqlite3VdbeGetOp(v, addrDistinctIndex);/*������OP_SorterOpen���򿪷ּ���������ȥ���ظ���������VDBE*/

		  assert( isDistinct );/*����ϵ㣬�ж��Ƿ���DISTINCT��ѯ����û���׳�������Ϣ*/
		  assert( pWInfo->eDistinct==WHERE_DISTINCT_ORDERED /*����ϵ㣬���eDistinctΪWHERE_DISTINCT_ORDERED*/
			   || pWInfo->eDistinct==WHERE_DISTINCT_UNIQUE /*����ΪWHERE_DISTINCT_UNIQUE��������׳�������Ϣ*/
		  );
		  distinct = -1;/*��distinct��Ϊ-1��������ȡ���ظ�����*/
		  if( pWInfo->eDistinct==WHERE_DISTINCT_ORDERED ){/*���WHERE������Ϣ��eDistinctΪWHERE_DISTINCT_ORDERED*/
			int iJump;
			int iExpr;
			int iFlag = ++pParse->nMem;/*���﷨�������з����ڴ�ĸ����Ӽ��ٸ�ֵ��iFlag*/
			int iBase = pParse->nMem+1;/*���﷨�������з����ڴ�ĸ�����1�ٸ�ֵ��iBase*/
			int iBase2 = iBase + pEList->nExpr;/*����ַiBase+���ʽ�����ٸ�ֵ��iBase2*/
			pParse->nMem += (pEList->nExpr*2);/*�����ʽ��2���Ϸ�����ڴ����ٸ�ֵ��pParse->nMem*/

			/* Change the OP_OpenEphemeral coded earlier to an OP_Integer. The
			** OP_Integer initializes the "first row" flag.  *//*��OP_OpenEphemeral��ΪOP_Integer�����OP_Integer��ʼ��Ϊ����һ�С����*/
			pOp->opcode = OP_Integer;/*��OP_Integer������ֵ��pOp->opcode*/
			pOp->p1 = 1;/*��������p1������1��p1Ϊ����ʱ����ʽ��*/
			pOp->p2 = iFlag;/*�ٽ�iFlag��Ǹ�ֵ��p2*/

			sqlite3ExprCodeExprList(pParse, pEList, iBase, 1);/*�ѱ��ʽ�б�pEList�е�ֵiBase�ŵ�һϵ�еļĴ�����*/
			iJump = sqlite3VdbeCurrentAddr(v) + 1 + pEList->nExpr + 1 + 1;/*<������ת��ַ>*/
			sqlite3VdbeAddOp2(v, OP_If, iFlag, iJump-1);/*��OP_If��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
			for(iExpr=0; iExpr<pEList->nExpr; iExpr++){/*�������ʽ*/
			  CollSeq *pColl = sqlite3ExprCollSeq(pParse, pEList->a[iExpr].pExpr);/*��ô���ݷ����﷨�����б��ʽ����һ��������У����û�����ã�����Ĭ��*/
			  sqlite3VdbeAddOp3(v, OP_Ne, iBase+iExpr, iJump, iBase2+iExpr);/*��OP_Ne��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
			  sqlite3VdbeChangeP4(v, -1, (const char *)pColl, P4_COLLSEQ);/*����ؼ���Ϣ�ṹ�е�ֵַ����pColl,��P4_COLLSEQ��ֵ���õ������*/
			  sqlite3VdbeChangeP5(v, SQLITE_NULLEQ);/*��SQLITE_NULLEQ����Ϊ������ʹ�õ�ֵ*/
			}
			sqlite3VdbeAddOp2(v, OP_Goto, 0, pWInfo->iContinue);/*��OP_Goto��������vdbe��Ȼ�󷵻���������ĵ�ַ*/

			sqlite3VdbeAddOp2(v, OP_Integer, 0, iFlag);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
			assert( sqlite3VdbeCurrentAddr(v)==iJump );/*����ϵ㣬�����ת�ĵ�ַ��ΪiJump�����׳�������Ϣ*/
			sqlite3VdbeAddOp3(v, OP_Move, iBase, iBase2, pEList->nExpr);/*��OP_Move��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  }else{
			pOp->opcode = OP_Noop;/*��OP_Noop��ֵ��opcode������*/
		  }
		}

		/* Use the standard inner loop. *//*ʹ�ñ�׼���ڲ�ѭ��*/
		selectInnerLoop(pParse, p, pEList, 0, 0, pOrderBy, distinct, pDest,
						pWInfo->iContinue, pWInfo->iBreak);/*���ݱ��ʽpWInfo->iContinue��pWInfo->iBreak����������*/

		/* End the database scan loop.
		*//*���ݿ�ɨ��ѭ��������*/
		sqlite3WhereEnd(pWInfo);/*�ж�WHERE������Ϣ���ٽ���WHERE�Ӿ�*/
	  }else{
		/* This is the processing for aggregate queries *//*����ۺϲ�ѯ*/
		NameContext sNC;    /* Name context for processing aggregate information *//*���������Ĵ���ۺ���Ϣ*/
		int iAMem;          /* First Mem address for storing current GROUP BY *//*��һ���ڴ��ַ�洢GROUP BY*/
		int iBMem;          /* First Mem address for previous GROUP BY *//*�洢��ǰGROUP BY�ڵ�һ���ڴ��ַ*/
		int iUseFlag;       /* Mem address holding flag indicating that at least
							** one row of the input to the aggregator has been
							** processed *//*���б�ǩ�ڴ��ַ��������һ��������ִ�оۼ�*/
		int iAbortFlag;     /* Mem address which causes query abort if positive *//*����������������ֹ��ѯ*/
		int groupBySort;    /* Rows come from source in GROUP BY order *//*��Դ��GROUP BY����Ľ����*/
		int addrEnd;        /* End of processing for this SELECT *//*����SELECT�Ľ������*/
		int sortPTab = 0;   /* Pseudotable used to decode sorting results *//*Pseudotable���ڽ�������Ľ��*/
		int sortOut = 0;    /* Output register from the sorter *//*�ӷּ�������Ĵ���*/

		/* Remove any and all aliases between the result set and the
		** GROUP BY clause.
		*//*ɾ���������GROUP BY֮�����еĹ�������*/
		if( pGroupBy ){
		  int k;                        /* Loop counter *//*ѭ��������*/
		  struct ExprList_item *pItem;  /* For looping over expression in a list *//*��һ���б���ѭ�����ʽ*/

		  for(k=p->pEList->nExpr, pItem=p->pEList->a; k>0; k--, pItem++){/*�������ʽ*/
			pItem->iAlias = 0;/*�����ʽ�б��еĹ���������Ϊ0*/
		  }
		  for(k=pGroupBy->nExpr, pItem=pGroupBy->a; k>0; k--, pItem++){/*����GROUP BY�Ӿ�*/
			pItem->iAlias = 0;/*�����ʽ�б��еĹ���������Ϊ0*/
		  }
		  if( p->nSelectRow>(double)100 ) p->nSelectRow = (double)100;/*���SELECT����д���100����ֵ��SELECT������Ϊ100���޶��˴�С������100��Ϊ100*/
		}else{
		  p->nSelectRow = (double)1;/*��֪��Ϊ1*/
		}

	 
		/* Create a label to jump to when we want to abort the query *//*�������ֹ��ѯ�������ڴ˴�����һ����ǩ*/
		addrEnd = sqlite3VdbeMakeLabel(v);/*ΪVDBE����һ����ǩ��ָ���������еĵ�ַ��������ֵ��ֵ��addrEnd*/

		/* Convert TK_COLUMN nodes into TK_AGG_COLUMN and make entries in
		** sAggInfo for all TK_AGG_FUNCTION nodes in expressions of the
		** SELECT statement.
		*//*��TK_COLUMN�ڵ�ת��ΪTK_AGG_COLUMN������ʹ����sAggInfo�е���Ŀת��ΪSELECT�еı��ʽ��TK_AGG_FUNCTION�ڵ�*/
		memset(&sNC, 0, sizeof(sNC));/*��sNC��ǰsizeof(*sNc)���ֽ���0�滻*/
		sNC.pParse = pParse;/*���﷨��������ֵ�������������е��﷨������*/
		sNC.pSrcList = pTabList;/*��SELECT��Դ���ϸ�ֵ�����������ĵ�Դ���ϣ�FROM�Ӿ��б�*/
		sNC.pAggInfo = &sAggInfo;/*���ۼ�������ǩ��ֵ�����������ĵľۼ�������ǩ*/
		sAggInfo.nSortingColumn = pGroupBy ? pGroupBy->nExpr+1 : 0;/*�������GROUPBY��ֵ���ۼ�������ǩ��nSortingColumn���û�н�pGroupBy����һ�����ʽ��ֵ����*/
		sAggInfo.pGroupBy = pGroupBy;/*��SELECT�е�GROUPBY��ֵ���ۼ������е�GROUPBY*/
		sqlite3ExprAnalyzeAggList(&sNC, pEList);/*���ý����ۼ��������ʽ����sqlite3ExprAnalyzeAggregates����pEList*/
		sqlite3ExprAnalyzeAggList(&sNC, pOrderBy);/*���ý����ۼ��������ʽ����sqlite3ExprAnalyzeAggregates����pOrderBy*/
		if( pHaving ){/*�������Having*/
		  sqlite3ExprAnalyzeAggregates(&sNC, pHaving);/*���ý����ۼ��������ʽ����sqlite3ExprAnalyzeAggregates����pHaving*/
		}
		sAggInfo.nAccumulator = sAggInfo.nColumn;/*���ۼ������е�������ֵ���ۼ��������ۼ����ĸ���*/
		for(i=0; i<sAggInfo.nFunc; i++){/*�����ۼ�����*/
		  assert( !ExprHasProperty(sAggInfo.aFunc[i].pExpr, EP_xIsSelect) );/*����ϵ㣬������ʽ�а���EP_xIsSelect���׳�������Ϣ*/
		  sNC.ncFlags |= NC_InAggFunc;/*��FROMԴ����ncFlagsλ��FROMԴ���еľۼ��������NC_InAggFunc���ٸ�ֵ��ncFlags*/
		  sqlite3ExprAnalyzeAggList(&sNC, sAggInfo.aFunc[i].pExpr->x.pList);/*���ý����ۼ��������ʽ����sqlite3ExprAnalyzeAggregates����sAggInfo.aFunc[i].pExpr->x.pList*/
		  sNC.ncFlags &= ~NC_InAggFunc;/*��FROMԴ����ncFlagsλ��FROMԴ���еľۼ��������NC_InAggFunc�ķǣ��ٸ�ֵ��ncFlags*/
		}
		if( db->mallocFailed ) goto select_end;/*��������ڴ����ֱ����ת����ѯ�������*/

		/* Processing for aggregates with GROUP BY is very different and
		** much more complex than aggregates without a GROUP BY.
		*//*�������GROUP BY�ľۼ�������ͬ�ڲ����ģ����Ҹ�Ϊ����*/
		if( pGroupBy ){/*(����ע��KeyInfo�����洢�������صĹؼ���Ϣ)*/
		  KeyInfo *pKeyInfo;  /* Keying information for the group by clause *//*����group by�Ĺؼ���Ϣ*/
		  int j1;             /* A-vs-B comparision jump *//*��ת��A��B���бȽ�*/
		  int addrOutputRow;  /* Start of subroutine that outputs a result row *//*�ӳ������ʼ��ַ�����һ�������*/
		  int regOutputRow;   /* Return address register for output subroutine *//*����ӳ���ļĴ�����ַ*/
		  int addrSetAbort;   /* Set the abort flag and return *//*���û򷵻��жϱ��*/
		  int addrTopOfLoop;  /* Top of the input loop *//*����ѭ������ʼ��ַ*/
		  int addrSortingIdx; /* The OP_OpenEphemeral for the sorting index *//*����������ַ����ʱ�򿪣�*/
		  int addrReset;      /* Subroutine for resetting the accumulator *//*�����ۼ������ӳ���*/
		  int regReset;       /* Return address register for reset subroutine *//*���������ӳ���ļĴ�����ַ*/

		  /* If there is a GROUP BY clause we might need a sorting index to
		  ** implement it.  Allocate that sorting index now.  If it turns out
		  ** that we do not need it after all, the OP_SorterOpen instruction
		  ** will be converted into a Noop.  
		  *//*������ע����ʵ�ַ��飬�Ƚ������������һ��GROUP BY�Ӿ䣬���ǿ�����Ҫһ����������ȥʵ������
		  ** ���������������������һ�������Ѿ�ʵ�ֵģ����OP_SorterOpenָ���תΪNoop*/
		  
		  sAggInfo.sortingIdx = pParse->nTab++;/*���﷨�������еı�ʵ���Ǳ��ʽ���ĸ�������ֵ���ۼ���������������*/
		  pKeyInfo = keyInfoFromExprList(pParse, pGroupBy);/*�����ʽ�б��е�pGroupBy��ȡ�ؼ���Ϣ��pKeyInfo*/
		  addrSortingIdx = sqlite3VdbeAddOp4(v, OP_SorterOpen, 
			  sAggInfo.sortingIdx, sAggInfo.nSortingColumn, 
			  0, (char*)pKeyInfo, P4_KEYINFO_HANDOFF);/*���һ��OP_SorterOpen��������������ֵ��Ϊһ��ָ�벢��ֵ��addrSortingIdx*/

		  /* Initialize memory locations used by GROUP BY aggregate processing
		  *//*��ʼ�� GROUP BY�ۼ��������ڴ�ռ�*/
		  iUseFlag = ++pParse->nMem;/*���﷨�������е��ڴ�ռ��1��ֵ��iUseFlag*/
		  iAbortFlag = ++pParse->nMem;/*���﷨�������е��ڴ�ռ��1��ֵ��iAbortFlag*/
		  regOutputRow = ++pParse->nMem;/*���﷨�������е��ڴ�ռ��1��ֵ��regOutputRow*/
		  addrOutputRow = sqlite3VdbeMakeLabel(v);/*ΪVDBE����һ����ǩ��ָ���������еĵ�ַ��������ֵ��ֵ��addrOutputRow*/
		  regReset = ++pParse->nMem;/*���﷨�������е��ڴ�ռ��1��ֵ��regReset*/
		  addrReset = sqlite3VdbeMakeLabel(v);/*ΪVDBE����һ����ǩ��ָ���������еĵ�ַ��������ֵ��ֵ��addrReset*/
		  iAMem = pParse->nMem + 1;/*���﷨�������е��ڴ�ռ��1��ֵ��iAMem*/
		  pParse->nMem += pGroupBy->nExpr;/*pGroupBy�б��ʽ�������﷨�������е��ڴ�ռ��ٸ�ֵ���﷨�������е��ڴ�ռ�*/
		  iBMem = pParse->nMem + 1;/*���﷨�������е��ڴ�ռ��1��ֵiBMem*/
		  pParse->nMem += pGroupBy->nExpr;/*pGroupBy�б��ʽ�������﷨�������е��ڴ�ռ��ٸ�ֵ���﷨�������е��ڴ�ռ�*/
		  sqlite3VdbeAddOp2(v, OP_Integer, 0, iAbortFlag);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  VdbeComment((v, "clear abort flag"));/*����clear abort flag�����뵽VDBE��*/
		  sqlite3VdbeAddOp2(v, OP_Integer, 0, iUseFlag);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  VdbeComment((v, "indicate accumulator empty"));/*����indicate accumulator empty"���뵽VDBE��*/
		  sqlite3VdbeAddOp3(v, OP_Null, 0, iAMem, iAMem+pGroupBy->nExpr-1);/*��OP_Null��������vdbe��Ȼ�󷵻���������ĵ�ַ*/

		  /* Begin a loop that will extract all source rows in GROUP BY order.
		  ** This might involve two separate loops with an OP_Sort in between, or
		  ** it might be a single loop that uses an index to extract information
		  ** in the right order to begin with.
		  *//*����һ��ѭ������ȡGROUP BY��������е�ԭ�С�������ע��ѭ����ȡGROUP BY�Ӿ�ָ�����У�*/
		  sqlite3VdbeAddOp2(v, OP_Gosub, regReset, addrReset);/*��OP_Gosub��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  pWInfo = sqlite3WhereBegin(pParse, pTabList, pWhere, &pGroupBy, 0, 0, 0);/*��ʼѭ��ִ��pGroupBy����䣬�����ؽ���ָ��*/
		  if( pWInfo==0 ) goto select_end;/*�������ָ��Ϊ0��������ѯ����*/
		  if( pGroupBy==0 ){/*���pGroupBy�в���group by�Ӿ�*/
			/* The optimizer is able to deliver rows in group by order so
			** we do not have to sort.  The OP_OpenEphemeral table will be
			** cancelled later because we still need to use the pKeyInfo
			*//*�Ż����ܹ����ͳ�GROUP BY��������ٽ������������ʱ��OP_OpenEphemeral���ᱻȡ������Ϊ����ʹ��pKeyInfo*/
			pGroupBy = p->pGroupBy;/*��SELECT��pGroupBy��ֵ��pGroupBy*/
			groupBySort = 0;/*��GROUP BY��������Ϊ0*/
		  }else{
			/* Rows are coming out in undetermined order.  We have to push
			** each row into a sorting index, terminate the first loop,
			** then loop over the sorting index in order to get the output
			** in sorted order
			*//*��ȷ����˳������С�������Ҫ��ÿһ�����뵽������������ֹ��һ��ѭ����Ȼ�������������Ϊ�˵ó���������С�*/
			int regBase;/*��ַ�Ĵ���*/
			int regRecord;/*�Ĵ����м�¼*/
			int nCol;/*����*/
			int nGroupBy;/*GROUP BY�ĸ���*/

			explainTempTable(pParse, 
				isDistinct && !(p->selFlags&SF_Distinct)?"DISTINCT":"GROUP BY");/*ִ�г���Ż�ʹ�øú��������������Ϣ���﷨��������*/

			groupBySort = 1;/*��GROUP BY������Ϊ1*/
			nGroupBy = pGroupBy->nExpr;/*��GROUP BY�еı��ʽ������ֵ��nGroupBy*/
			nCol = nGroupBy + 1;/*��nGroupBy+1��ֵ������nCol*/
			j = nGroupBy+1;/*��nGroupBy+1��ֵ��j*/
			for(i=0; i<sAggInfo.nColumn; i++){/*������*/
			  if( sAggInfo.aCol[i].iSorterColumn>=j ){/*���������кŴ��ڵ���j*/
				nCol++;/*������1*/
				j++;/*j��1*/
			  }
			}
			regBase = sqlite3GetTempRange(pParse, nCol);/*Ϊ�����з���Ĵ������ѼĴ�����ʼֵ���ظ�regBase*/
			sqlite3ExprCacheClear(pParse);/*��������е��﷨������*/
			sqlite3ExprCodeExprList(pParse, pGroupBy, regBase, 0);/*�ѱ��ʽ�б�pEList�е�ֵ��ʼֵregBase�ŵ��ѷ���һϵ�еļĴ�����*/
			sqlite3VdbeAddOp2(v, OP_Sequence, sAggInfo.sortingIdx,regBase+nGroupBy);/*��OP_Gosub��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
			j = nGroupBy+1;/*��nGroupBy+1��ֵ��j*/
			for(i=0; i<sAggInfo.nColumn; i++){/*�����ۼ�������Ϣ�е���*/
			  struct AggInfo_col *pCol = &sAggInfo.aCol[i];/*����һ���ۼ������нṹ��*/
			  if( pCol->iSorterColumn>=j ){/*���������кŴ��ڵ���j*/
				int r1 = j + regBase;/*����ַ�Ĵ����Ļ�ַ����j��ֵ��r1*/
				int r2;

				r2 = sqlite3ExprCodeGetColumn(pParse, 
								   pCol->pTab, pCol->iColumn, pCol->iTable, r1, 0);/*�ӱ�����ȡ��iColumn�У��������е�ֵ�洢��ַ���ظ�r2*/
				if( r1!=r2 ){/*���������ַ����*/
				  sqlite3VdbeAddOp2(v, OP_SCopy, r2, r1);/*��OP_SCopy����VEBE����r1,r2����VDBE������copy���ٷ��ش���õĵ�ַ*/
				}
				j++;
			  }
			}
			regRecord = sqlite3GetTempReg(pParse);/*�õ��﷨����������ʱ�Ĵ����е�ֵ��regRecord*/
			sqlite3VdbeAddOp3(v, OP_MakeRecord, regBase, nCol, regRecord);/*��OP_MakeRecord��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
			sqlite3VdbeAddOp2(v, OP_SorterInsert, sAggInfo.sortingIdx, regRecord);/*��OP_SorterInsert��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
			sqlite3ReleaseTempReg(pParse, regRecord);/*�ͷ��﷨����������ʱ�Ĵ���regRecord��ֵ*/
			sqlite3ReleaseTempRange(pParse, regBase, nCol);/*�ͷ���ʼ��ַΪregBase�����Ĵ�����������nCol*/
			sqlite3WhereEnd(pWInfo);/*�ж�WHERE������Ϣ���ٽ���WHERE�Ӿ�*/
			sAggInfo.sortingIdxPTab = sortPTab = pParse->nTab++;/*���﷨�������б������ֵ���ۼ�������Ϣ�ṹ��������������*/
			sortOut = sqlite3GetTempReg(pParse);/*�õ��﷨����������ʱ�Ĵ����е�ֵ��sortOut*/
			sqlite3VdbeAddOp3(v, OP_OpenPseudo, sortPTab, sortOut, nCol);/*��OP_OpenPseudo��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
			sqlite3VdbeAddOp2(v, OP_SorterSort, sAggInfo.sortingIdx, addrEnd);/*��OP_SorterSort��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
			VdbeComment((v, "GROUP BY sort"));/*��"GROUP BY sort"���뵽VDBE��*/
			sAggInfo.useSortingIdx = 1;/*���ۼ�����ʹ������������Ϊ1*/
			sqlite3ExprCacheClear(pParse);/*��������е��﷨������*/
		  }

		  /* Evaluate the current GROUP BY terms and store in b0, b1, b2...
		  ** (b0 is memory location iBMem+0, b1 is iBMem+1, and so forth)
		  ** Then compare the current GROUP BY terms against the GROUP BY terms
		  ** from the previous row currently stored in a0, a1, a2...
		  *//*���㵱ǰGROUP BY������Ҵ洢��b0,b1,b2����b0���ڴ��ַΪiBMem+0��b1���ڴ��ַΪiBMem+1���������ƣ�
		  **Ȼ�󽫵�ǰGROUP BY��������洢��a0,a1,a2����ǰ���е�the GROUP BY ������*/
		  addrTopOfLoop = sqlite3VdbeCurrentAddr(v);/*�õ���ǰVDBE�ĵ�ַ����ֵ��addrTopOfLoop*/
		  sqlite3ExprCacheClear(pParse);/*��������е��﷨������*/
		  if( groupBySort ){/*����з�������*/
			sqlite3VdbeAddOp2(v, OP_SorterData, sAggInfo.sortingIdx, sortOut);/*��OP_SorterSort��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  }
		  for(j=0; j<pGroupBy->nExpr; j++){/*����pGroupBy�еı��ʽ*/
			if( groupBySort ){/*������ڷ�������*/
			  sqlite3VdbeAddOp3(v, OP_Column, sortPTab, j, iBMem+j);/*��OP_Column��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
			  if( j==0 ) sqlite3VdbeChangeP5(v, OPFLAG_CLEARCACHE);/*��OPFLAG_CLEARCACHE����Ϊ������ʹ�õ�ֵ*/
			}else{
			  sAggInfo.directMode = 1;/*���ۼ�������Ϣ��ֱ�Ӵ�Դ������ȡ������Ϊ1*/
			  sqlite3ExprCode(pParse, pGroupBy->a[j].pExpr, iBMem+j);/*��pExpr���ʽ��iBMem+jƫ������ŵ��﷨��������*/
			}
		  }
		  sqlite3VdbeAddOp4(v, OP_Compare, iAMem, iBMem, pGroupBy->nExpr,
							  (char*)pKeyInfo, P4_KEYINFO);/*���һ��OP_Compare��������������ֵ��Ϊһ��ָ��*/
		  j1 = sqlite3VdbeCurrentAddr(v);/*�õ���ǰVDBE�ĵ�ַ����ֵ��j1*/
		  sqlite3VdbeAddOp3(v, OP_Jump, j1+1, 0, j1+1);/*��OP_Jump��������vdbe��Ȼ�󷵻���������ĵ�ַ*/

		  /* Generate code that runs whenever the GROUP BY changes.
		  ** Changes in the GROUP BY are detected by the previous code
		  ** block.  If there were no changes, this block is skipped.
		  **
		  ** This code copies current group by terms in b0,b1,b2,...
		  ** over to a0,a1,a2.  It then calls the output subroutine
		  ** and resets the aggregate accumulator registers in preparation
		  ** for the next GROUP BY batch.
		  *//*��д���ۺ�ʱ����GROUP BY�ı�Ĵ��롣�ı�GROUP BY�������ǰ�Ĵ���顣���û�иı䣬����������顣
		  ** ��δ���copy��ǰGROUP BY��b0,b1,b2������� a0,a1,a2.Ȼ���������ӳ��������Ϊ��һ��GROUP BY��׼���ľۼ��ۼ����Ĵ���*/
		  sqlite3ExprCodeMove(pParse, iBMem, iAMem, pGroupBy->nExpr);/*�ͷżĴ����е����ݣ����ּĴ��������ݼ�ʱ����*/
		  sqlite3VdbeAddOp2(v, OP_Gosub, regOutputRow, addrOutputRow);/*��OP_Gosub��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  VdbeComment((v, "output one row"));/*��"output one row"���뵽VDBE��*/
		  sqlite3VdbeAddOp2(v, OP_IfPos, iAbortFlag, addrEnd);/*��OP_IfPos��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  VdbeComment((v, "check abort flag"));/*��"check abort flag"���뵽VDBE��*/
		  sqlite3VdbeAddOp2(v, OP_Gosub, regReset, addrReset);/*��OP_Gosub��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  VdbeComment((v, "reset accumulator"));/*��"reset accumulator"���뵽VDBE��*/

		  /* Update the aggregate accumulators based on the content of
		  ** the current row
		  *//*���Ļ��ڵ�ǰ�е����ݵľۼ��Ĵ���*/
		  sqlite3VdbeJumpHere(v, j1);/*���j1��Ϊ0�����е�ַ������ǰ��ַ*/
		  updateAccumulator(pParse, &sAggInfo);/*Ϊ��ǰ�α�λ���ϵľۼ������������ۼ����ڴ浥Ԫ*/
		  sqlite3VdbeAddOp2(v, OP_Integer, 1, iUseFlag);/*��OP_SorterSort��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  VdbeComment((v, "indicate data in accumulator"));/*��"indicate data in accumulator"���뵽VDBE��*/

		  /* End of the loop
		  *//*����ѭ��*/
		  if( groupBySort ){/*�������������*/
			sqlite3VdbeAddOp2(v, OP_SorterNext, sAggInfo.sortingIdx, addrTopOfLoop);/*��OP_SorterNext��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  }else{
			sqlite3WhereEnd(pWInfo);/*�ж�WHERE������Ϣ���ٽ���WHERE�Ӿ�*/
			sqlite3VdbeChangeToNoop(v, addrSortingIdx);/*��addrSortingIdx�в�����ΪOP_Noop*/
		  }

		  /* Output the final row of result
		  *//*������յĽ����*/
		  sqlite3VdbeAddOp2(v, OP_Gosub, regOutputRow, addrOutputRow);/*��OP_Gosub��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  VdbeComment((v, "output final row"));/*��"output final row"���뵽VDBE��*/

		  /* Jump over the subroutines
		  *//*��������ӳ���*/
		  sqlite3VdbeAddOp2(v, OP_Goto, 0, addrEnd);/*��OP_Goto��������vdbe�����������ǣ�Ȼ�󷵻���������ĵ�ַ*/

		  /* Generate a subroutine that outputs a single row of the result
		  ** set.  This subroutine first looks at the iUseFlag.  If iUseFlag
		  ** is less than or equal to zero, the subroutine is a no-op.  If
		  ** the processing calls for the query to abort, this subroutine
		  ** increments the iAbortFlag memory location before returning in
		  ** order to signal the caller to abort.
		  *//*��дһ���ӳ������һ�е����Ľ����������ӳ������Ȳ���iUseFlag�����iUseFlagС�ڵ���0������ӳ���Ϊno-op.
		  **����������Ԫ�������ֹ��ѯ������ӳ������ڷ���֮ǰ������iAbortFlag�ڴ�Ϊ����ֹ�źŵ����ߡ�*/
		  addrSetAbort = sqlite3VdbeCurrentAddr(v);/*�õ���ǰVDBE�ĵ�ַ����ֵ��addrSetAbort*/
		  sqlite3VdbeAddOp2(v, OP_Integer, 1, iAbortFlag);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  VdbeComment((v, "set abort flag"));/*��"set abort flag"���뵽VDBE��*/
		  sqlite3VdbeAddOp1(v, OP_Return, regOutputRow);/*��OP_Return��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  sqlite3VdbeResolveLabel(v, addrOutputRow);/*����addrOutputRow������Ϊ��һ��������ĵ�ַ*/
		  addrOutputRow = sqlite3VdbeCurrentAddr(v);/*�õ���ǰVDBE�ĵ�ַ����ֵ��addrOutputRow*/
		  sqlite3VdbeAddOp2(v, OP_IfPos, iUseFlag, addrOutputRow+2);/*��OP_IfPos��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  VdbeComment((v, "Groupby result generator entry point"));/*��"set abort flag"���뵽VDBE�з��뵽VDBE��*/
		  sqlite3VdbeAddOp1(v, OP_Return, regOutputRow);/*��OP_Return��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  finalizeAggFunctions(pParse, &sAggInfo);/*ΪAggInfo�ṹ��sAggInfo��ÿһ���ۼ���������OP_AggFinalize����ɣ�����*/
		  sqlite3ExprIfFalse(pParse, pHaving, addrOutputRow+1, SQLITE_JUMPIFNULL);/*���Ϊtrue����ִ�У����ΪFALSE����addrOutputRow+1*/
		  selectInnerLoop(pParse, p, p->pEList, 0, 0, pOrderBy,
						  distinct, pDest,
						  addrOutputRow+1, addrSetAbort);/*���ݱ��ʽp->pEList���������ӣ���������ΪOrderBy��distinct��pDest��������*/
		  sqlite3VdbeAddOp1(v, OP_Return, regOutputRow);/*��OP_Return��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		  VdbeComment((v, "end groupby result generator"));/*��"end groupby result generator"���뵽VDBE�з��뵽VDBE��*/

		  /* Generate a subroutine that will reset the group-by accumulator
		  *//*��һ���ӳ�����������group-by�ۼ���*/
		  sqlite3VdbeResolveLabel(v, addrReset);/*����addrReset������Ϊ��һ��������ĵ�ַ*/
		  resetAccumulator(pParse, &sAggInfo);/*����sAggInfo������*/
		  sqlite3VdbeAddOp1(v, OP_Return, regReset);/*��OP_Return��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
		 
		} /* endif pGroupBy.  Begin aggregate queries without GROUP BY: *//*�������pGroupBy����ʼʹ�ò��� GROUP BY�ľۼ���ѯ*/
		else {
		  ExprList *pDel = 0;/*����һ�����ʽ�б�*/
	#ifndef SQLITE_OMIT_BTREECOUNT
		  Table *pTab;
		  if( (pTab = isSimpleCount(p, &sAggInfo))!=0 ){/*�������Ϊ��*/
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
			*/
			/* ���isSimpleCount() ����һ��ָ��TABLE�ṹ��ָ�룬Ȼ��SQL����ʽ���£�
			** 
			**	SELECT count(*) FROM <tbl>
			** 
			** Table�ṹ���ص�table<tbl>���£�
			** 
			** ������ܳ������ʽ��������Ż��ˡ����OP_Countָ��ִ����intkey���������ݵ�table<tbl>������������
			** ���ȽϺõ�ִ��op��������������Ƶ�����ݱ����һ�£����������Ӧ�ı����ٵ�ҳ��
			*/
			const int iDb = sqlite3SchemaToIndex(pParse->db, pTab->pSchema);/*Ϊ��ģʽ��������������ֵ��iDb*/
			const int iCsr = pParse->nTab++;     /* Cursor to scan b-tree *//*ɨ��b-tree��ָ��*/
			Index *pIdx;                         /* Iterator variable *//*����������*/
			KeyInfo *pKeyInfo = 0;               /* Keyinfo for scanned index *//*ɨ�������Ĺؼ���Ϣ�ṹ��*/
			Index *pBest = 0;                    /* Best index found so far *//*��õ�����*/
			int iRoot = pTab->tnum;              /* Root page of scanned b-tree *//*ɨ��b-tree�ĸ�ҳ*/

			sqlite3CodeVerifySchema(pParse, iDb);/*ȷ��schema cookie������һ�������������ݿ��ļ���*/
			sqlite3TableLock(pParse, iDb, pTab->tnum, 0, pTab->zName);/*��¼��Ҫ�����ı�*/

			/* Search for the index that has the least amount of columns. If
			** there is such an index, and it has less columns than the table
			** does, then we can assume that it consumes less space on disk and
			** will therefore be cheaper to scan to determine the query result.
			** In this case set iRoot to the root page number of the index b-tree
			** and pKeyInfo to the KeyInfo structure required to navigate the
			** index.
			**
			** (2011-04-15) Do not do a full scan of an unordered index.
			**
			** In practice the KeyInfo structure will not be used. It is only 
			** passed to keep OP_OpenRead happy.
			*//*�������ٳ��ֵ��е���������������и����������бȽ��ٵ��У�Ȼ�����Ǽ����������˽�С�Ŀռ���Ӳ���ϲ���
			** ɨ���Ѿ�ȷ����ѯ��������Ƚϵ͡�����������£�����iRoot��b-tree�����ĸ�ҳ�Ų���pKeyInfo��KeyInfo�ṹ����Ҫ�ĵ�������*/
			for(pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext){/*��������*/
			  if( pIdx->bUnordered==0 && (!pBest || pIdx->nColumn<pBest->nColumn) ){/*�������û��������û����õ��������������е���С������������е���*/
				pBest = pIdx;/*����ǰ��������ֵΪ��õ���������Ϊ��ǰû����ã�*/
			  }
			}
			if( pBest && pBest->nColumn<pTab->nCol ){/*������������Ϊ�ջ����������������С�ڱ������*/
			  iRoot = pBest->tnum;/*����������и�������ֵ��iRoot*/
			  pKeyInfo = sqlite3IndexKeyinfo(pParse, pBest);/*�������������Ϣ��ȡ���ؼ���Ϣ�ṹ��pKeyInfo��*/
			}

			/* Open a read-only cursor, execute the OP_Count, close the cursor. *//*��ֻ���αִ꣬��OP_Count�������ر��α�*/
			sqlite3VdbeAddOp3(v, OP_OpenRead, iCsr, iRoot, iDb);/*��OP_OpenRead��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
			if( pKeyInfo ){/*����ؼ���Ϣ����*/
			  sqlite3VdbeChangeP4(v, -1, (char *)pKeyInfo, P4_KEYINFO_HANDOFF);/*����ؼ���Ϣ�ṹ�е�ֵַ����pKeyInfo,��P4_KEYINFO_HANDOFF��ֵ���õ������*/
			}
			sqlite3VdbeAddOp2(v, OP_Count, iCsr, sAggInfo.aFunc[0].iMem);/*��OP_Count��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
			sqlite3VdbeAddOp1(v, OP_Close, iCsr);/*��OP_Close��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
			explainSimpleCount(pParse, pTab, pBest);/*ִ��һ��count(*)��ѯ*/
		  }else
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
			/*�����ѯ�����µĸ�ʽ���ͽ��м�⣺
			**   SELECT min(x) FROM ...
			**   SELECT max(x) FROM ...
			**����ǣ�Ȼ����where.c�������������������һ��"ORDER ON x" �� "ORDER ON x DESC" �Ӿ䡣
			**���where.c�ܲ�����������Ȼ���ڵ�һ������������һ������ѭ����ִ֤��x����С�����ֵ��ֻ��һ�У�֮�����VDBE�����ж�ִ��ѭ��
			**
			**Ӧ�ô���sqlite3WhereBeginһ������ı����΢�޸�һ�µ���Ϊ��
			**   ��������ѯ��"SELECT min(x)"��Ȼ��where.c��ѭ�����벻�ܵ����κ�x�еĿ�ֵ��
			**   where.c�Ż������루����ʹ��ʹ����Щ������Ӧ������'ORDER BY'�Ӿ䣬����Ĵ����ע��ϸ����where.c�С�
			*/
			ExprList *pMinMax = 0;/*����һ�����ʽ�б������С�����ֵ�ı��ʽ*/
			u8 flag = minMaxQuery(p);/*��SELECT�ṹ��p�������ֵ����Сֵ��ѯ������ֵ��flag*/
			if( flag ){/*���flag����*/
			  assert( !ExprHasProperty(p->pEList->a[0].pExpr, EP_xIsSelect) );/*����ϵ㣬���p->pEList->a[0].pExpr�а���EP_xIsSelect���Բ�Ϊ�գ��׳�������Ϣ*/
			  pMinMax = sqlite3ExprListDup(db, p->pEList->a[0].pExpr->x.pList,0);/*���copy  p->pEList->a[0].pExpr->x.pList��pMinMax*/
			  pDel = pMinMax;/*����ѯ�����ֵ��pDel*/
			  if( pMinMax && !db->mallocFailed ){/*���pMinMax���ڲ��ҷ����ڴ�ɹ�*/
				pMinMax->a[0].sortOrder = flag!=WHERE_ORDERBY_MIN ?1:0;/*���flagΪWHERE_ORDERBY_MIN��1��ֵ�������ǣ�����ֵ0*/
				pMinMax->a[0].pExpr->op = TK_COLUMN;/*�����ʽ�в�������ֵΪTK_COLUMN*/
			  }
			}
	  
			/* This case runs if the aggregate has no GROUP BY clause.  The
			** processing is much simpler since there is only a single row
			** of output.
			*//*����ۼ�������û�� GROUP BY���������������ܼ򵥣�ֻ��һ���������������*/
			resetAccumulator(pParse, &sAggInfo);/*���þۼ������ۼ���*/
			pWInfo = sqlite3WhereBegin(pParse, pTabList, pWhere, &pMinMax,0,flag,0);/*��ʼѭ��ִ��pMinMax����䣬�����ؽ���ָ��*/
			if( pWInfo==0 ){/*�����һ�еķ���ֵΪ��*/
			  sqlite3ExprListDelete(db, pDel);/*ɾ��ִ����ֵ�ı��ʽ�б�*/
			  goto select_end;/*������ѯ����*/
			}
			updateAccumulator(pParse, &sAggInfo);/*Ϊ��ǰ�α�λ���ϵľۼ������������ۼ����ڴ浥Ԫ*/
			if( !pMinMax && flag ){/*�����ֵ�ı��ʽΪ�ղ���flag��ǲ�Ϊ��*/
			  sqlite3VdbeAddOp2(v, OP_Goto, 0, pWInfo->iBreak);/*��OP_Goto��������vdbe������ pWInfo->iBreakִ����䣬Ȼ�󷵻���������ĵ�ַ*/
			  VdbeComment((v, "%s() by index",
					(flag==WHERE_ORDERBY_MIN?"min":"max")));/*��"%s() by index"���뵽VDBE�з��뵽VDBE��,����flag=WHERE_ORDERBY_MIN��%sΪmin������Ϊmax*/
			}
			sqlite3WhereEnd(pWInfo);/*�ж�WHERE������Ϣ���ٽ���WHERE�Ӿ�*/
			finalizeAggFunctions(pParse, &sAggInfo);/*ΪAggInfo�ṹ��sAggInfo��ÿһ���ۼ���������OP_AggFinalize����ɣ�����*/
		  }

		  pOrderBy = 0;/*����ǰ��������ʽ�б�pOrderBy��Ϊ0*/
		  sqlite3ExprIfFalse(pParse, pHaving, addrEnd, SQLITE_JUMPIFNULL);/*���Ϊtrue����ִ�У����ΪFALSE����addrEnd*/
		  selectInnerLoop(pParse, p, p->pEList, 0, 0, 0, -1, 
						  pDest, addrEnd, addrEnd);/*���ݱ��ʽp->pEList���������ӣ���������addrEnd��pDest��������*/
		  sqlite3ExprListDelete(db, pDel);/*ɾ��ִ����ֵ�ı��ʽ�б�*/
		}
		sqlite3VdbeResolveLabel(v, addrEnd);/*����addrEnd������Ϊ��һ��������ĵ�ַ*/
		
	  } /* endif aggregate query *//*����Ǿۼ���ѯ*/

	  if( distinct>=0 ){/*ȡ���ظ����ʽ��ֵ���ڵ���0*/
		explainTempTable(pParse, "DISTINCT");/*ִ�г���Ż�ʹ�øú��������������Ϣ"DISTINCT"���﷨������*/
	  }

	  /* If there is an ORDER BY clause, then we need to sort the results
	  ** and send them to the callback one by one.
	  *//*�������һ��ORDERBY�Ӿ䣬������Ҫ���������ҷ��ͽ��һ����һ���ĸ��ص�����*/
	  if( pOrderBy ){/*���������ʽ�б�pOrderBy��ֵ��Ϊ0*/
		explainTempTable(pParse, "ORDER BY");/*�����Ϣ"ORDER BY"���﷨������*/
		generateSortTail(pParse, p, v, pEList->nExpr, pDest);/*���������������ORDER BY���*/
	  }

	  /* Jump here to skip this query
	  *//*�����˴������������ѯ*/
	  sqlite3VdbeResolveLabel(v, iEnd);/*����iEnd������Ϊ��һ��������ĵ�ַ*/

	  /* The SELECT was successfully coded.   Set the return code to 0
	  ** to indicate no errors.
	  *//*SELECT���ɹ�ִ�У�����0��˵���޴���*/
	  rc = 0;/*��ִ�н�����Ϊ0*/

	  /* Control jumps to here if an error is encountered above, or upon
	  ** successful coding of the SELECT.
	  *//*��ֵ��ת�����������淢���˴������SELECT�ɹ���ִ����*/
	select_end:
	  explainSetInteger(pParse->iSelectId, iRestoreSelectId);/*������䣬ִ��SELECT���������洢SELECT��ID*/

	  /* Identify column names if results of the SELECT are to be output.
	  *//*���SELECT�Ľ�����������ʶ �е�����*/
	  if( rc==SQLITE_OK && pDest->eDest==SRT_Output ){/*����������ΪSQLITE_OK���Ҵ���Ľ�����ķ�ʽΪSRT_Output*/
		generateColumnNames(pParse, pTabList, pEList);/*���ݱ�ͱ��ʽ�б���������*/
	  }

	  sqlite3DbFree(db, sAggInfo.aCol);/*�ͷ����ݿ������д洢�ۼ�������Ϣ���е��ڴ�*/
	  sqlite3DbFree(db, sAggInfo.aFunc);/*�ͷ����ݿ������д洢�ۼ�������Ϣ�ľۼ����������ڴ�*/
	  return rc;/*����ִ�н������*/
	}

	#if defined(SQLITE_ENABLE_TREE_EXPLAIN)
	/*
	** Generate a human-readable description of a the Select object.
	*//*����һ���׶�����SELECET�Ķ���*/
	static void explainOneSelect(Vdbe *pVdbe, Select *p){
	  sqlite3ExplainPrintf(pVdbe, "SELECT ");/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����"SELECT "*/
	  if( p->selFlags & (SF_Distinct|SF_Aggregate) ){/*���SELECT��selFlags�����SF_Distinct��SF_Aggregate*/
		if( p->selFlags & SF_Distinct ){/*���selFlagsΪSF_Distinct*/
		  sqlite3ExplainPrintf(pVdbe, "DISTINCT ");/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����"DISTINCT"*/
		}
		if( p->selFlags & SF_Aggregate ){/*���selFlagsΪSF_Aggregate*/
		  sqlite3ExplainPrintf(pVdbe, "agg_flag ");/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����"agg_flag "*/
		}
		sqlite3ExplainNL(pVdbe);/*���һ�����з���'\n',ǰ���������βû�У�*/
		sqlite3ExplainPrintf(pVdbe, "   ");/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����"  "*/
	  }
	  sqlite3ExplainExprList(pVdbe, p->pEList);/*Ϊ���ʽ�б�p->pEList����һ���׶���������Ϣ*/
	  sqlite3ExplainNL(pVdbe);/*���һ�����з���'\n',ǰ���������βû�У�*/
	  if( p->pSrc && p->pSrc->nSrc ){/*����FROM�Ӿ���ʽ�б�*/
		int i;
		sqlite3ExplainPrintf(pVdbe, "FROM ");/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����"FROM "*/
		sqlite3ExplainPush(pVdbe);/*��pVdbe���Ƴ�һ���µ��������𣬴ӹ�꿪ʼ��λ�ý��У��������ж�����*/
		for(i=0; i<p->pSrc->nSrc; i++){/*����FROM�Ӿ���ʽ�б�**/
		  struct SrcList_item *pItem = &p->pSrc->a[i];/*����һ��FROM�Ӿ��б���ṹ��*/
		  sqlite3ExplainPrintf(pVdbe, "{%d,*} = ", pItem->iCursor);/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ����� "{%d,*} = "*/
		  if( pItem->pSelect ){/*������ʽ�б��к���SELECT*/
			sqlite3ExplainSelect(pVdbe, pItem->pSelect);/*����SELECT�ṹ���pSelect*/
			if( pItem->pTab ){/*������ʽ�б��к���pTab*/
			  sqlite3ExplainPrintf(pVdbe, " (tabname=%s)", pItem->pTab->zName);/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����" (tabname=%s)"*/
			}
		  }else if( pItem->zName ){/*������ʽ�б��б��ʽ��������*/
			sqlite3ExplainPrintf(pVdbe, "%s", pItem->zName);/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����pItem->zName*/
		  }
		  if( pItem->zAlias ){/*������ʽ�б����й�������*/
			sqlite3ExplainPrintf(pVdbe, " (AS %s)", pItem->zAlias);/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����" (AS %s)"*/
		  }
		  if( pItem->jointype & JT_LEFT ){/*������ʽ�б�����������ΪJT_LEFT*/
			sqlite3ExplainPrintf(pVdbe, " LEFT-JOIN");/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����" LEFT-JOIN"*/
		  }
		  sqlite3ExplainNL(pVdbe);/*���һ�����з���'\n',ǰ���������βû�У�*/
		}
		sqlite3ExplainPop(pVdbe);/*�����ղ�ѹ��ջ����������*/
	  }
	  if( p->pWhere ){/*���SELECT�ṹ��p�к���Ϊ��where�Ӿ�*/
		sqlite3ExplainPrintf(pVdbe, "WHERE ");/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����"WHERE "*/
		sqlite3ExplainExpr(pVdbe, p->pWhere);/*Ϊ���ʽ��pWhere����һ���׶�˵��*/
		sqlite3ExplainNL(pVdbe);/*���һ�����з���'\n',ǰ���������βû�У�*/
	  }
	  if( p->pGroupBy ){/*���SELECT�ṹ��p�к���Ϊ��where�Ӿ�*/
		sqlite3ExplainPrintf(pVdbe, "GROUPBY ");/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����"GROUPBY "*/
		sqlite3ExplainExprList(pVdbe, p->pGroupBy);/*Ϊ���ʽ�б�p->pGroupBy����һ���׶���������Ϣ*/
		sqlite3ExplainNL(pVdbe);/*���һ�����з���'\n',ǰ���������βû�У�*/
	  }
	  if( p->pHaving ){/*���SELECT�ṹ��p�к���Ϊ��having�Ӿ�*/
		sqlite3ExplainPrintf(pVdbe, "HAVING ");/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ����� "HAVING "*/
		sqlite3ExplainExpr(pVdbe, p->pHaving);/*Ϊ���ʽ��pHaving����һ���׶�˵��*/
		sqlite3ExplainNL(pVdbe);/*���һ�����з���'\n',ǰ���������βû�У�*/
	  }
	  if( p->pOrderBy ){/*���SELECT�ṹ��p�к���Ϊ��order by�Ӿ�*/
		sqlite3ExplainPrintf(pVdbe, "ORDERBY ");/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����"ORDERBY "*/
		sqlite3ExplainExprList(pVdbe, p->pOrderBy);/*Ϊ���ʽ�б�p->pOrderBy����һ���׶���������Ϣ*/
		sqlite3ExplainNL(pVdbe);/*���һ�����з���'\n',ǰ���������βû�У�*/
	  }
	  if( p->pLimit ){/*���SELECT�ṹ��p�к���Ϊ��limit�Ӿ�*/
		sqlite3ExplainPrintf(pVdbe, "LIMIT ");/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����"LIMIT "*/
		sqlite3ExplainExpr(pVdbe, p->pLimit);/*Ϊ���ʽ��p->pLimit����һ���׶�˵��*/
		sqlite3ExplainNL(pVdbe);/*���һ�����з���'\n',ǰ���������βû�У�*/
	  }
	  if( p->pOffset ){/*���SELECT�ṹ��p�к���Ϊ��offset�Ӿ�*/
		sqlite3ExplainPrintf(pVdbe, "OFFSET ");/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����"OFFSET "*/
		sqlite3ExplainExpr(pVdbe, p->pOffset);/*Ϊ���ʽ��p->pOffset����һ���׶�˵��*/
		sqlite3ExplainNL(pVdbe);/*���һ�����з���'\n',ǰ���������βû�У�*/
	  }
	}
	void sqlite3ExplainSelect(Vdbe *pVdbe, Select *p){
	  if( p==0 ){/*���SELECT�ṹ��pΪ��*/
		sqlite3ExplainPrintf(pVdbe, "(null-select)");/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����"(null-select)"*/
		return;
	  }
	  while( p->pPrior ) p = p->pPrior;/*�ݹ飬�ҵ������Ȳ�ѯ��SELECT*/
	  sqlite3ExplainPush(pVdbe);/*��pVdbe���Ƴ�һ���µ��������𣬴ӹ�꿪ʼ��λ�ý��У��������ж�����*/
	  while( p ){
		explainOneSelect(pVdbe, p);/*����һ���׶�����SELECET�Ķ���*/
		p = p->pNext;/*��p����������ֵ����ǰp*/
		if( p==0 ) break;/*ѭ�������һ���ˣ��Ѿ�û�����ӽڵ�*/
		sqlite3ExplainNL(pVdbe);/*���һ�����з���'\n',ǰ���������βû�У�*/
		sqlite3ExplainPrintf(pVdbe, "%s\n", selectOpName(p->op));/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����"%s\n"*/
	  }
	  sqlite3ExplainPrintf(pVdbe, "END");/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����"END"*/
	  sqlite3ExplainPop(pVdbe);/*�����ղ�ѹ��ջ����������*/
	}
	/* End of the structure debug printing code ������ӡ���Դ���Ľṹ
	*****************************************************************************/
	#endif /* defined(SQLITE_ENABLE_TREE_EXPLAIN) */