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
**���ļ��������﷨�������������� SQLite �� SELECT �����õ� C ��������
*/

#include "sqliteInt.h"  /*Ԥ���봦������sqliteInt.h�ļ��е����ݼ��ص���������*/

/*
** Trace output macros
** ���������
*/

#if SELECTTRACE_ENABLED //Ԥ����ָ��
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
** ����ṹ���һ��ʵ�������ڼ�¼�й���δ���DISTINCT�ؼ��ֵ���Ϣ��Ϊ�˼򻯴��ݸ���Ϣ��selectInnerLoop��������
*/

typedef struct DistinctCtx DistinctCtx;
struct DistinctCtx {
	u8 isTnct;      /* ���DISTINCT�ؼ��ִ������� */ 
		u8 eTnctType;   /* ���е�WHERE_DISTINCT_*�����*/ 
		int tabTnct;    /* ����DISTINCT����ʱ��*/
		int addrTnct;   /* OP_OpenEphemeral������ĵ�ַ*/ 
};

/*
** An instance of the following object is used to record information about
** the ORDER BY (or GROUP BY) clause of query is being coded.
** ����ṹ���һ��ʵ�������ڼ�¼ORDER BY(���� GROUP BY)��ѯ�־����Ϣ
*/

typedef struct SortCtx SortCtx;
struct SortCtx {
	ExprList *pOrderBy;   /* ORDER BY(���� GROUP BY�־�)*/ 
		int nOBSat;           /* ORDER BY������������ָ��*/
		int iECursor;         /* ���������α���Ŀ*/
		int regReturn;        /* �Ĵ������ƿ�������ص�ַ*/
		int labelBkOut;       /* ������ӳ����������ǩ*/  
		int addrSortIndex;    /* OP_SorterOpen����OP_OpenEphemeral�ĵ�ַ */ 
		u8 sortFlags;         /* ����߸����SORTFLAG_* λ */ 
};
#define SORTFLAG_UseSorter  0x01   /* ʹ��sorteropen����openephemeral */ 

/*
** Delete all the content of a Select structure.  Deallocate the structure
** itself only if bFree is true.
** ɾ��ѡ��ṹ���������ݡ�����bFree�����ʱ���ͷŽṹ����
*/

static void clearSelect(sqlite3 *db, Select *p, int bFree){
	while (p){
		Select *pPrior = p->pPrior;                  /*��p->pPrior��ֵ��Select *pPrior*/
			sqlite3ExprListDelete(db, p->pEList);        /*ɾ���������ʽ�б�*/
			sqlite3SrcListDelete(db, p->pSrc);           /*ɾ������SrcList, �������е��ӽṹ*/
			sqlite3ExprDelete(db, p->pWhere);            /*�ݹ�ɾ��һ�����ʽ��*/
			sqlite3ExprListDelete(db, p->pGroupBy);      /*ɾ���������ʽ�б�*/
			sqlite3ExprDelete(db, p->pHaving);           /*�ݹ�ɾ��һ�����ʽ��*/
			sqlite3ExprListDelete(db, p->pOrderBy);      /*ɾ���������ʽ�б�*/
			sqlite3ExprDelete(db, p->pLimit);            /*�ݹ�ɾ��һ�����ʽ��*/
			sqlite3ExprDelete(db, p->pOffset);           /*�ݹ�ɾ��һ�����ʽ��*/
			sqlite3WithDelete(db, p->pWith);			 /*�ݹ�ɾ��һ��������*/
			if (bFree)
				sqlite3DbFree(db, p);					 /*�ͷ�*db*/
				p = pPrior;
		bFree = 1;
	}
}

/*
** Initialize a SelectDest structure.
** ��ʼ��һ��SelectDest�ṹ.
*/
void sqlite3SelectDestInit(SelectDest *pDest, int eDest, int iParm){/*����sqlite3SelectDestInit�Ĳ����б�Ϊ
																	  �ṹ��SelectDestָ��pDest������ָ��eDest��
																	  ����ָ��iParm
																	  */
	pDest->eDest = (u8)eDest; /*������eDestǿ������ת��Ϊu8�ͣ�Ȼ��ֵ��pDest->eDest*/
	pDest->iSDParm = iParm; /*���Ͳ���iParm��ֵΪpDest->iSDParm*/
	pDest->affSdst = 0; /*0��ֵ��pDest->affSdst*/
	pDest->iSdst = 0; /*0��ֵ��pDest->iSdst*/
	pDest->nSdst = 0; /*0��ֵ��pDest->nSdst*/
}

/*
** Allocate a new Select structure and return a pointer to that
** structure.
** ����һ���µ�select�ṹ,���ҷ���һ��ָ��ýṹ��ָ��.
*/
Select *sqlite3SelectNew(
	Parse *pParse,        /* Parsing context  �䷨����*/
	ExprList *pEList,     /* which columns to include in the result  �ڽ���а�����Щ��*/
	SrcList *pSrc,        /* the FROM clause -- which tables to scan  FROM�־�--ɨ��� */
	Expr *pWhere,         /* the WHERE clause  WHERE�־�*/
	ExprList *pGroupBy,   /* the GROUP BY clause   GROUP BY�־�*/
	Expr *pHaving,        /* the HAVING clause  havingHAVING�־�*/
	ExprList *pOrderBy,   /* the ORDER BY clause  ORDER BY�־�*/
	int isDistinct,       /* true if the DISTINCT keyword is present  ����ؼ���distinct���ڣ��򷵻�true*/
	Expr *pLimit,         /* LIMIT value.  NULL means not used  limitֵ�����ֵΪ����ζ��limitδʹ��*/
	Expr *pOffset         /* OFFSET value.  NULL means no offset  offsetֵ�����ֵΪ����ζ��offsetδʹ��*/
	){
	Select *pNew;/*����ṹ��ָ��pNew*/
	Select standin;/*����ṹ�����ͱ���standin*/
	sqlite3 *db = pParse->db;/*�ṹ��Parse�ĳ�Աdb��ֵ���ṹ��sqlite3ָ��db*/
	pNew = sqlite3DbMallocZero(db, sizeof(*pNew));  /* ��������ڴ棬�������ʧ�ܣ�ʹmallocFaied��־������ָ���С� */
	assert(db->mallocFailed || !pOffset || pLimit); /* �жϷ����Ƿ�ʧ��,��pOffsetֵΪ��,��pLimitֵ��Ϊ��*/
	if (pNew == 0){/*����ṹ��ָ�����pNew����ʧ��*/
		assert(db->mallocFailed);/*�������ʧ�ܣ�ʹmallocFaied��־������ָ����*/
		pNew = &standin;/*��standin�Ĵ洢��ַ����pNew*/
		memset(pNew, 0, sizeof(*pNew));/*��pNew��ǰsizeof(*pNew)���ֽ���0�滻���ҷ���pNew*/
	}
	if (pEList == 0){/*������ʽ�б�Ϊ��*/
		pEList = sqlite3ExprListAppend(pParse, 0, sqlite3Expr(db, TK_ALL, 0)); /*����ӵ�Ԫ���ڱ��ʽ�б��ĩβ�������Ԫ��
																				�ĵ�ַ����pEList�����pList�ĳ�ʼ����Ϊ�գ�
																				��ô�½� һ���µı��ʽ�б���������ڴ�
																				��������������б��ͷŲ����ؿա����
																				���ص��Ƿǿգ���֤�µ���Ŀ�ɹ�׷�ӡ�
																				*/
	}
	pNew->pEList = pEList;/*pEListָ��Ԫ�صĵ�ַ����pNew->pEList*/
	if (pSrc == 0) pSrc = sqlite3DbMallocZero(db, sizeof(*pSrc));/*���䲢����ڴ棬�����СΪ�ڶ����������ڴ棬�������ʧ�ܣ�����mallocFailed�������*/
	pNew->pSrc = pSrc;/*ΪSelect�ṹ����FROM�Ӿ���ʽ��ֵ*/
	pNew->pWhere = pWhere;/*ΪSelect�ṹ����Where�Ӿ���ʽ��ֵ*/
	pNew->pGroupBy = pGroupBy;/*ΪSelect�ṹ����GroupBy�Ӿ���ʽ��ֵ*/
	pNew->pHaving = pHaving;/*ΪSelect�ṹ����Having�Ӿ���ʽ��ֵ*/
	pNew->pOrderBy = pOrderBy;/*ΪSelect�ṹ����OrderBy�Ӿ���ʽ��ֵ*/
	pNew->selFlags = isDistinct ? SF_Distinct : 0;/*����SF_*�е�ֵ*/
	pNew->op = TK_SELECT;/*ֻ������ΪTK_UNION TK_ALL TK_INTERSECT TK_EXCEPT ����һ��ֵ*/
	pNew->pLimit = pLimit;/*ΪSelect�ṹ����Limit�Ӿ���ʽ��ֵ*/
	pNew->pOffset = pOffset;/*ΪSelect�ṹ����Offset�Ӿ���ʽ��ֵ*/
	assert(pOffset == 0 || pLimit != 0);/*���ƫ����Ϊ�գ�Limit��Ϊ�գ�������ڴ�*/
	pNew->addrOpenEphm[0] = -1;       /*�Ե�ַ��Ϣ��ʼ��*/
	pNew->addrOpenEphm[1] = -1;		  /*�Ե�ַ��Ϣ��ʼ��*/
	pNew->addrOpenEphm[2] = -1;		  /*�Ե�ַ��Ϣ��ʼ��*/
	if (db->mallocFailed) {          /*������ܷ����ڴ�*/
		clearSelect(db, pNew);          /*���Select�ṹ��������*/
		if (pNew != &standin) sqlite3DbFree(db, pNew);/*���Select�ṹ��standin�ĵ�ַδ��ֵ��pNew�������pNew��*/
		pNew = 0;                      /*��Select�ṹ������Ϊ��*/
	}
	else{                           /*�ܷ����ڴ�*/
		assert(pNew->pSrc != 0 || pParse->nErr>0);/*�ж��Ƿ���From�Ӿ�����Ƿ��з�������*/
	}
	assert(pNew != &standin);/*�ж�Select�ṹ���Ƿ�ͬ�滻�ṹ����ͬ*/
	return pNew;/*�����������õ�Select�ṹ��*/
}

/*
** Delete the given Select structure and all of its substructures.
** ɾ��������ѡ��ṹ�����е��ӽṹ
*/
void sqlite3SelectDelete(sqlite3 *db, Select *p){/*�������ݿ�db�Լ�Select���͵Ľṹ��p��Ϊ����*/
	if (p){/*���Select�ṹ��ָ��p����*/
		clearSelect(db, p);  /*���Select���ͽṹ��p���������*/
		sqlite3DbFree(db, p);  /*�ͷŵ��ռ�*/
	}
}

/*
** Given 1 to 3 identifiers preceeding the JOIN keyword, determine the
** type of join.  Return an integer constant that expresses that type
** in terms of the following bit values:
** �����ӹؼ���ǰ��һ��������ʾ��������ʹ�ú������ӷ�ʽ������һ����������ʾʹ�����µĺ�����������:
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
** ȫ��������JT_LEFT��JT_RIGHT��ϡ� �����⵽�ǷǷ��ַ����߲�֧�ֵ��������ͣ�
** Ҳ�᷵��һ���������ͣ����ǻ���pParse�ṹ�з���һ��������Ϣ��
*/
int sqlite3JoinType(Parse *pParse, Token *pA, Token *pB, Token *pC){/*�������������pParse�Լ��������Ľṹ�壨���ģ�����ִ��ĳЩ������Ȩ���Ķ��󣩲���*/
	int jointype = 0;/*��ʱ�������ڱ�ʾ��������*/
	Token *apAll[3];/*����������ͽṹ��ָ������apAll*/
	Token *p;/*����������ͽṹ��ָ��p*/
	/*   0123456789 123456789 123456789 123 */
	static const char zKeyText[] = "naturaleftouterightfullinnercross";/*����ֻ������ֻ���ڵ�ǰģ���пɼ����ַ�������zKeyText�����ڱ�ʾ�������ͣ���������и�ֵ*/
	static const struct {/*����ֻ������ֻ���ڵ�ǰģ��ɼ��ṹ��*/
		u8 i;        /* Beginning of keyword text in zKeyText[]   ��KeyText[] �п�ʼ�ؼ��ֵ��ı�*/
		u8 nChar;    /* Length of the keyword in characters  ���ַ��йؼ��ֵĳ���*/
		u8 code;     /* Join type mask �����������*/
	} aKeyword[] = {
		/* natural �±��0��ʼ������Ϊ7����Ȼ���� */{ 0, 7, JT_NATURAL },
		/* left   �±��6��ʼ������Ϊ4�������� */{ 6, 4, JT_LEFT | JT_OUTER },
		/* outer  �±��10��ʼ������Ϊ5�������� */{ 10, 5, JT_OUTER },
		/* right   �±��14��ʼ������Ϊ5��������*/{ 14, 5, JT_RIGHT | JT_OUTER },
		/* full    �±��19��ʼ������Ϊ4��ȫ����*/{ 19, 4, JT_LEFT | JT_RIGHT | JT_OUTER },
		/* inner  �±��23��ʼ������Ϊ5�������� */{ 23, 5, JT_INNER },
		/* cross   �±��28��ʼ������Ϊ5�������ӻ�CROSS����*/{ 28, 5, JT_INNER | JT_CROSS },
	};//����ȫ�����͵����ӣ���������ʼλ�á����ȡ���������
	int i, j;
	apAll[0] = pA;
	apAll[1] = pB;
	apAll[2] = pC;/*�����˵��������Ľṹ�����͵Ĳ����ֱ�ֵ�����Ľṹ�����͵�apAll������ */
	for (i = 0; i < 3 && apAll[i]; i++){//ѭ������apAll����
		p = apAll[i];/*ָ��apAll[i]�ĵ�ַ����ָ��p*/
		if (p->n == aKeyword[j].nChar /*����������ַ������������������еĹؼ��ֳ���*/
			&& sqlite3StrNICmp((char*)p->z, &zKeyText[aKeyword[j].i], p->n) == 0){/*����ʹ�ñȽ��ַ��������Ƚ�*/
			jointype |= aKeyword[j].code;/*���ͨ���˱Ƚϳ��Ⱥ����ݣ������������ͣ�ע���ǣ�ʹ�õ��ǡ�λ��*/
			break;//��������ѭ��
		}
	}
	testcase(j == 0 || j == 1 || j == 2 || j == 3 || j == 4 || j == 5 || j == 6);/*���ò��Դ�����testcast������jֵ���Ƿ��������Χ*/
	if (j >= ArraySize(aKeyword)){/*���j�����ӹؼ������黹��*/
		jointype |= JT_ERROR;/*�Ǿ�jointype��JT_ERROR��λ�򡱣�����һ������*/
		break;//��������ѭ��
	  }
	}
	if (
	(jointype & (JT_INNER | JT_OUTER)) == (JT_INNER | JT_OUTER) ||/*����������ͽ���(JT_INNER|JT_OUTER)�Ľ������JT_INNER��JT_OUTERһ��*/
	(jointype & JT_ERROR) != 0/*�������ӹؼ����Ǵ�������*/
	){
		const char *zSp = " "; /*ֻ�����ַ���ָ��zSp*/
		assert(pB != 0);/*�ж�pB�Ƿ�Ϊ��*/
		if (pC == 0){ zSp++; }/*���ָ��pCָ��ĵ�ַΪ0����zSp++*/
		sqlite3ErrorMsg(pParse, "unknown or unsupported join type: ""%T %T%s%T", pA, pB, zSp, pC);  /*�����������δ֪���߲�֧�ֵ��������͡�*/
		jointype = JT_INNER ;/*Ĭ��ʹ��������*/
	}
	else if ((jointype & JT_OUTER) != 0 
		&& (jointype & (JT_LEFT | JT_RIGHT)) != JT_LEFT){/*����������ͺ��������н����������������ͺ�(JT_LEFT|JT_RIGHT)����������������*/
		sqlite3ErrorMsg(pParse,"RIGHT and FULL OUTER JOINs are not currently supported");/*���������Ϣ�������Ӻ�ȫ�����Ӳ���֧�֡�*/
		jointype = JT_INNER; /*Ĭ��ʹ��������*/
	}
	return jointype;/*������������*/
}

/*
** Return the index of a column in a table.  Return -1 if the column
** is not contained in the table.
** ���ر��е�һ�е��±꣬������в��ڱ��У�����-1.
*/
static int columnIndex(Table *pTab, const char *zCol){/*���徲̬�����ͺ���columnIndex�������б�Ϊ�ṹ��ָ��pTab��ֻ�����ַ���ָ��zCol*/
	int i;/*������ʱ����*/
	for (i = 0; i < pTab->nCol; i++){/*�����е��н��б���*/
		if (sqlite3StrICmp(pTab->aCol[i].zName, zCol) == 0) return i;/*���ƥ��ɹ�����ô����i*/
	}
	return -1;/*���򣬷���-1*/
}

/*
** Search the first N tables in pSrc, from left to right, looking for a
** table that has a column named zCol.
** ��FROM�Ӿ���ɨ���������,����ǰN����,��һ����������ΪzCol�ı� ��
**
** When found, set *piTab and *piCol to the table index and column index
** of the matching column and return TRUE.
** �ҵ�֮��,����*piTab��������������*piCol����Ҫƥ������������ٷ���TRUE
**
** If not found, return FALSE.
** ���û���ҵ�������FALSE��
*/
static int tableAndColumnIndex(
	SrcList *pSrc,       /* Array of tables to search ��Ŵ����ҵı�Ķ���*/
	int N,               /* Number of tables in pSrc->a[] to search �����Ŀ*/
	const char *zCol,    /* Name of the column we are looking for Ѱ�ҵ�����*/
	int *piTab,          /* Write index of pSrc->a[] here д������*/
	int *piCol           /* Write index of pSrc->a[*piTab].pTab->aCol[] here д������*/
	){
	int i;               /* For looping over tables in pSrc ��pSrc������*/
	int iCol;            /* Index of column matching zCol   ƥ���ϵ��е��������ڼ���*/

	assert((piTab == 0) == (piCol == 0));  /* Both or neither are NULL  �жϱ����������������ǻ򶼲��ǿ�*/
	for (i = 0; i < N; i++){/*�������еı�*/
		iCol = columnIndex(pSrc->a[i].pTab, zCol); /*���ر���е���������iCol���������û���ڱ��У�iCol��ֵ��-1.*/
		if (iCol >= 0){/*�������������*/
			if (piTab){/*�������������*/
				*piTab = i;/*��i ����ָ��piTab ��Ŀ�����*/
				*piCol = iCol;/*��iCol ��ֵ��ָ��piCol ��Ŀ�����*/
			}
			return 1;/*���򷵻�1*/
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

** ��������������where�Ӿ���ͺ���JOIN�﷨��,�Ӷ�����select��䡣
** �����������ӵ�����where�Ӿ��еģ���ʽ����:
**
** (tab1.col1 = tab2.col2)
**
** tab1��SrcList pSrc��iSrc'th��tab2��(iSrc+1)'th����col1��tab1��iColLeft�У�col2��
** tab2��iColRight��
*/
static void addWhereTerm(
	Parse *pParse,                  /* Parsing context  �������*/
	SrcList *pSrc,                  /* List of tables in FROM clause   from�־��е��б� */
	int iLeft,                      /* Index of first table to join in pSrc  ��һ�����ӵı����� */
	int iColLeft,                   /* Index of column in first table  ��һ�����������*/
	int iRight,                     /* Index of second table in pSrc  �ڶ������ӵı�����*/
	int iColRight,                  /* Index of column in second table  �ڶ������������*/
	int isOuterJoin,                /* True if this is an OUTER join  ������������򷵻�true*/
	Expr **ppWhere                  /* IN/OUT: The WHERE clause to add to  where�Ӿ���ӵ�in/out*/
	){
	sqlite3 *db = pParse->db;/*����һ�����ݿ�����*/
	Expr *pE1; /*����ṹ��ָ��pE1*/
	Expr *pE2; /*����ṹ��ָ��pE2*/
	Expr *pEq; /*����ṹ��ָ��pEq*/

	assert(iLeft<iRight);/*�ж������һ��������ֵ�Ƿ�С�ڵڶ���������ֵ*/
	assert(pSrc->nSrc>iRight);/*�жϱ����еı����Ŀ�Ƿ�����ұ������ֵ*/
	assert(pSrc->a[iLeft].pTab);/*�жϱ����б�����������ı��Ƿ�Ϊ��*/
	assert(pSrc->a[iRight].pTab);/*�жϱ����б����ұ������ı��Ƿ�Ϊ��*/

	pE1 = sqlite3CreateColumnExpr(db, pSrc, iLeft, iColLeft);/*���䲢����һ�����ʽָ��ȥ���ر���������һ��������*/
	pE2 = sqlite3CreateColumnExpr(db, pSrc, iRight, iColRight);/*���䲢����һ�����ʽָ��ȥ���ر������ұ��һ��������*/

	pEq = sqlite3PExpr(pParse, TK_EQ, pE1, pE2, 0);/*����һ������ڵ������������������ʽ*/
	if (pEq && isOuterJoin){/*���pEq���ʽ��ȫ���ӱ��ʽ*/
		ExprSetProperty(pEq, EP_FromJoin);/*��ô��������ʹ��ON��USING�Ӿ�*/
		assert(!ExprHasAnyProperty(pEq, EP_TokenOnly | EP_Reduced));/*�ж�pEq���ʽ�Ƿ���EP_TokenOnly��EP_Reduced*/
		ExprSetIrreducible(pEq);/*����pEq,�����Ƿ����Լ��*/
		pEq->iRightJoinTable = (i16)pE2->iTable;/*ָ��Ҫ���ӵ��ұ��ǵڶ������ʽ�ı�*/
	}
	*ppWhere = sqlite3ExprAnd(db, *ppWhere, pEq);/*��ָ�����ݿ�ı��ʽ�ǽ�������*/
}

/*
** Set the EP_FromJoin property on all terms of the given expression.
** And set the Expr.iRightJoinTable to iTable for every term in the
** expression.
** �ڸ����ı��ʽ�е�������������EP_FromJoin���������á�����iTable��ÿһ��
** �����ʽ����Expr.iRightJoinTable���á�
**
** The EP_FromJoin property is used on terms of an expression to tell
** the LEFT OUTER JOIN processing logic that this term is part of the
** join restriction specified in the ON or USING clause and not a part
** of the more general WHERE clause.  These terms are moved over to the
** WHERE clause during join processing but we need to remember that they
** originated in the ON or USING clause.
** EP_FromJoin ���������������Ӵ����߼��ı����ʽ��������ʽ�Ǽ�������ָ��on����
** using�Ӿ��һ���֣�����һ��where�Ӿ��һ���֡���Щ������ֲ��where �Ӿ���
** ʹ�ã��������Ǳ����ס������Դ��on����useing�Ӿ䡣
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
** Expr.iRightJoinTable����where�Ӿ���ʽ������iRightJoinTable������ʹ����
** ���ʽ��û����ȷ�ᵽ����Щ��Ϣ��Ҫ���������:
** SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.b AND t1.x=5
** where �Ӿ���Ҫ�Ƴٴ���t1.x=5��ֱ������t2ѭ��֮�������ַ�ʽ��
** ÿ��t1.x!=5ʱ��һ��NULL t2�н������롣������ǲ��Ƴ� t1.x=5�Ĵ���
** ���ᱻ�����������t1ѭ������t1.x!=5��Զ������������ǲ���ȷ�ġ�
*/
static void setJoinExpr(Expr *p, int iTable){/*����setJoinExpr�Ĳ����б�Ϊ�ṹ��ָ��p������iTable*/
	while (p){/*��pΪ��ʱѭ��*/
		ExprSetProperty(p, EP_FromJoin);/*����join��ʹ��ON��USING�Ӿ�*/
		assert(!ExprHasAnyProperty(p, EP_TokenOnly | EP_Reduced));/*�жϱ��ʽ�����ԣ����ڱ��ʽ�ĳ��Ⱥ�ʣ�೤��*/
		ExprSetIrreducible(p);/*���Ա��ʽ���ж��Ƿ����*/
		p->iRightJoinTable = (i16)iTable;/*�����ұ��������д���ı�*/
		setJoinExpr(p->pLeft, iTable);/*�ݹ��������*/
		p = p->pRight;/*��ֵ���ʽ����p��ֵ��Ϊԭ��p�����ӽڵ�*/
	}
}

/*
** This routine processes the join information for a SELECT statement.
** ON and USING clauses are converted into extra terms of the WHERE clause.
** NATURAL joins also create extra WHERE clause terms.
** ���������һ��select����join��Ϣ��on����using�Ӿ�ת��Ϊ������ʽ��where�Ӿ䡣
** ��Ȼ����Ҳ���������where�Ӿ䡣
**
** The terms of a FROM clause are contained in the Select.pSrc structure.
** The left most table is the first entry in Select.pSrc.  The right-most
** table is the last entry.  The join operator is held in the entry to
** the left.  Thus entry 0 contains the join operator for the join between
** entries 0 and 1.  Any ON or USING clauses associated with the join are
** also attached to the left entry.
**from�Ӿ䱻Select.pSrc(Select�ṹ����FROM����)��������
**��ߵı�ͨ����Select.pSrc����ڡ��ұߵı�ͨ�������һ����ڣ�entry��hashmap����Ϊѭ���Ľڵ���ڣ��˴������ı����ӱ����ļ�¼��ڣ�
**join����������ڵ���ߡ�Ȼ����ڵ�0���������Ӳ����������0�����1֮�䡣�κ��漰join��ON��USING�Ӿ䣬Ҳ�����Ӳ������ŵ������ڡ�
**
** This routine returns the number of errors encountered.
** ������򷵻��������������
*/
static int sqliteProcessJoin(Parse *pParse, Select *p){/*���������pParse��Select�ṹ��p*/
	SrcList *pSrc;                  /* All tables in the FROM clause   from�Ӿ��е����б�*/
	int i, j;                       /* Loop counters  ѭ��������*/
	struct SrcList_item *pLeft;     /* Left table being joined   �������*/
	struct SrcList_item *pRight;    /* Right table being joined   �ұ�����*/

	pSrc = p->pSrc;/*��p->pSrc��ֵ���ṹ��ָ��pSrc*/
	pLeft = &pSrc->a[0];/*��&pSrc->a[0]�������*/
	pRight = &pLeft[1];/*��&pLeft[1]�����ұ�*/
	for (i = 0; i < pSrc->nSrc - 1; i++, pRight++, pLeft++){
		Table *pLeftTab = pLeft->pTab;/*��pLeft->pTab�����ṹ��ָ��pLeftTab*/
		Table *pRightTab = pRight->pTab;/*��pRight->pTab�����ṹ��ָ��pRightTab*/
		int isOuter;/*�����ж�*/

		if (NEVER(pLeftTab == 0 || pRightTab == 0)) continue;/*��������ұ���һ��Ϊ������������ѭ��*/
		isOuter = (pRight->jointype & JT_OUTER) != 0;/*�ұ���������ͽ����������Ͳ�Ϊ�գ��ٸ�ֵ��isOute����ֵ*/

		/* When the NATURAL keyword is present, add WHERE clause terms for
		** every column that the two tables have in common.
		** ��natural�ؼ��ִ��ڣ�����WHERE�Ӿ������Ϊ������������ͬ�С�
		*/
		if (pRight->jointype & JT_NATURAL){/*����ұ��������������Ȼ����*/
			if (pRight->pOn || pRight->pUsing){/*����ұ���ON��USING�Ӿ�*/
				sqlite3ErrorMsg(pParse, "a NATURAL join may not have "
					"an ON or USING clause", 0);/*��ô���������Ȼ�����в��ܺ���ON USING�Ӿ�*/
				return 1;
			}
			for (j = 0; j < pRightTab->nCol; j++){/*ѭ�������ұ��е���*/
				char *zName;   /* Name of column in the right table �ұ����е�����*/
				int iLeft;     /* Matching left table ƥ�����*/
				int iLeftCol;  /* Matching column in the left table �������ƥ����*/

				zName = pRightTab->aCol[j].zName;/*��������ֵ*/
				if (tableAndColumnIndex(pSrc, i + 1, zName, &iLeft, &iLeftCol)){/*������������е�����*/
					addWhereTerm(pParse, pSrc, iLeft, iLeftCol, i + 1, j,
						isOuter, &p->pWhere);/*���WHERE�Ӿ䣬�������ұ��к����ӷ�ʽ*/
				}
			}
		}

		/* Disallow both ON and USING clauses in the same join
		** ��������ͬһ��������ʹ��on��using�Ӿ�
		*/
		if (pRight->pOn && pRight->pUsing){/*����ṹ��ָ��pRight���õĳ�Ա�����pOn��pUsing�ǿ�*/
			sqlite3ErrorMsg(pParse, "cannot have both ON and USING "
				"clauses in the same join");/*���������Ϣ��cannot have both ON and USING clauses in the same join��*/
			return 1;
		}

		/* Add the ON clause to the end of the WHERE clause, connected by
		** an AND operator.
		** ��on�Ӿ���ӵ�where�Ӿ��ĩβ����and����������
		*/
		if (pRight->pOn){/*����ṹ��ָ��pRight���õĳ�Ա����pOn�ǿ�*/
			if (isOuter) setJoinExpr(pRight->pOn, pRight->iCursor);/*����������ӣ��������ӱ��ʽ��ON�Ӿ���α�*/
			p->pWhere = sqlite3ExprAnd(pParse->db, p->pWhere, pRight->pOn);/*���ý�WHERE�Ӿ���ON�Ӿ�����һ�𣬸�ֵ���ṹ���WHERE*/
			pRight->pOn = 0;/*���û�������ӣ������ò�ʹ��ON�Ӿ�*/
		}

		/* Create extra terms on the WHERE clause for each column named
		** in the USING clause.  Example: If the two tables to be joined are
		** A and B and the USING clause names X, Y, and Z, then add this
		** to the WHERE clause:    A.X=B.X AND A.Y=B.Y AND A.Z=B.Z
		** Report an error if any column mentioned in the USING clause is
		** not contained in both tables to be joined.
		** ��WHERE�Ӿ���Ϊÿһ�д���һ�������USING�Ӿ�������磺
		** ����������������A��B,using����ΪX,Y,Z,Ȼ���������ӵ�
		** where�Ӿ�:A.X=B.X AND A.Y=B.Y AND A.Z=B.Z
		** ���using�Ӿ����ᵽ���κ��в������ڱ�������У��ͻᱨ��
		** һ������
		*/
		if (pRight->pUsing){/*����ṹ��ָ��pRight���õĳ�Ա����pUsing�ǿ�*/
			IdList *pList = pRight->pUsing;/*��pRight->pUsing�����ṹ��ָ��pList*/
			for (j = 0; j < pList->nId; j++){/*������ʾ���б�*/
				char *zName;     /* Name of the term in the USING clause   using�Ӿ����������*/
				int iLeft;       /* Table on the left with matching column name   �����ƥ�������*/
				int iLeftCol;    /* Column number of matching column on the left  ���ƥ���е�����*/
				int iRightCol;   /* Column number of matching column on the right  �ұ�ƥ���е�����*/

				zName = pList->a[j].zName;/*��ʾ���б��еı�ʾ��*/
				iRightCol = columnIndex(pRightTab, zName);/*�����ұ�ͱ�ʾ���ұ�Ĵ�ƥ��������������к�*/
				if (iRightCol<0
					|| !tableAndColumnIndex(pSrc, i + 1, zName, &iLeft, &iLeftCol)/*����в�����*/
					){
					sqlite3ErrorMsg(pParse, "cannot join using column %s - column "
						"not present in both tables", zName);/*���������Ϣ*/
					return 1;
				}
				addWhereTerm(pParse, pSrc, iLeft, iLeftCol, i + 1, iRightCol,
					isOuter, &p->pWhere);/*������ڣ���ӵ�WHERE�Ӿ���*/
			}
		}
	}
	return 0;/*Ĭ�Ϸ���0���������ķ����ɵó�������һ��inner����*/
}

/*
** Insert code into "v" that will push the record on the top of the
** stack into the sorter.
** �������"v"���ڷ����������ƽ���¼��ջ�Ķ�����
*/
static void pushOntoSorter(
	Parse *pParse,         /* Parser context  �������*/
	ExprList *pOrderBy,    /* The ORDER BY clause   order by�Ӿ�*/
	Select *pSelect,       /* The whole SELECT statement  ����select���*/
	int regData            /* Register holding data to be sorted  ������������*/
	){
	Vdbe *v = pParse->pVdbe;/*����һ�������*/
	int nExpr = pOrderBy->nExpr;/*����һ��ORDERBY���ʽ*/
	int regBase = sqlite3GetTempRange(pParse, nExpr + 2); /*������ͷ�һ�������ļĴ���������һ������ֵ���Ѹ�ֵ����regBase��*/
	int regRecord = sqlite3GetTempReg(pParse); /*����һ���µļĴ������ڿ����м�����*/
	int op;
	sqlite3ExprCacheClear(pParse); /*��������еĻ�����Ŀ*/
	sqlite3ExprCodeExprList(pParse, pOrderBy, regBase, 0);/*���ɴ��룬�������ı��ʽ�б��ÿ��Ԫ�ص�ֵ�ŵ��Ĵ�����ʼ��Ŀ�����С�����Ԫ��������������*/
	sqlite3VdbeAddOp2(v, OP_Sequence, pOrderBy->iECursor, regBase + nExpr);/*�����ʽ�ŵ�VDBE�У��ٷ���һ���µ�ָ���ַ*/
	sqlite3ExprCodeMove(pParse, regData, regBase + nExpr + 1, 1);/*���ļĴ����е����ݣ��������ܼ�ʱ���¼Ĵ����е��л�������*/
	sqlite3VdbeAddOp3(v, OP_MakeRecord, regBase, nExpr + 2, regRecord);/*��nExpr�ŵ���ǰʹ�õ�VDBE�У��ٷ���һ���µ�ָ��ĵ�ַ*/
	if (pSelect->selFlags & SF_UseSorter){/*���select�ṹ����selFlags��ֵ��SF_UseSorter����һ�£�selFlags��ֵȫ����SF��ͷ�������ʾʹ���˷ּ����ˡ�*/
		op = OP_SorterInsert;/*��Ϊʹ�÷ּ��������Բ���������Ϊ����ּ���*/
	}
	else{
		op = OP_IdxInsert;/*����ʹ��������ʽ����*/
	}
	sqlite3VdbeAddOp2(v, op, pOrderBy->iECursor, regRecord);/*��Orderby���ʽ�ŵ���ǰʹ�õ�VDBE�У�Ȼ�󷵻�һ���µ�ָ���ַ*/
	sqlite3ReleaseTempReg(pParse, regRecord);/*�ͷ�regRecord�Ĵ���*/
	sqlite3ReleaseTempRange(pParse, regBase, nExpr + 2);/*�ͷ�regBase��������Ĵ����������Ǳ��ʽ�ĳ��ȼ�2*/
	if (pSelect->iLimit){/*���ʹ��Limit�Ӿ�*/
		int addr1, addr2;
		int iLimit;
		if (pSelect->iOffset){/*���ʹ����Offsetƫ����*/
			iLimit = pSelect->iOffset + 1;/*��ôLimit��ֵΪƫ������1*/
		}
		else{
			iLimit = pSelect->iLimit;/*�������Ĭ�ϵģ��ӵ�һ����ʼ����*/
		}
		addr1 = sqlite3VdbeAddOp1(v, OP_IfZero, iLimit);/*�����ַ�ǽ�������˷��ص������������µ�ָ���ַ*/
		sqlite3VdbeAddOp2(v, OP_AddImm, iLimit, -1);/*��ָ��ŵ���ǰʹ�õ�VDBE��Ȼ�󷵻�һ����ַ*/
		addr2 = sqlite3VdbeAddOp0(v, OP_Goto);/*�����ʹ��Goto���֮�󷵻صĵ�ַ*/
		sqlite3VdbeJumpHere(v, addr1);/*�ı�addr1�ĵ�ַ���Ա�VDBEָ����һ��ָ��ĵ�ַ*/
		sqlite3VdbeAddOp1(v, OP_Last, pOrderBy->iECursor);/*��ORDERBYָ��ŵ���ǰʹ�õ�������У�����Last�����ĵ�ַ*/
		sqlite3VdbeAddOp1(v, OP_Delete, pOrderBy->iECursor);/*��ORDERBYָ��ŵ���ǰʹ�õ�������У�����Delete�����ĵ�ַ*/
		sqlite3VdbeJumpHere(v, addr2);/*�ı�addr2�ĵ�ַ���Ա�VDBEָ����һ��ָ��ĵ�ַ*/
	}
}

/*
** Add code to implement the OFFSET
** ��Ӵ�����ʵ��offsetƫ��
*/
static void codeOffset(
	Vdbe *v,          /* Generate code into this VM  �������������ɴ���*/
	Select *p,        /* The SELECT statement being coded  select��䱻����*/
	int iContinue     /* Jump here to skip the current record  ����������������ǰ��¼*/
	){
	if (p->iOffset && iContinue != 0){/*����ṹ��ָ��p���õĳ�ԱiOffset�ǿ�������iContinue������0*/
		int addr;
		sqlite3VdbeAddOp2(v, OP_AddImm, p->iOffset, -1);/*��VDBE�������һ��ָ�����һ����ָ��ĵ�ַ*/
		addr = sqlite3VdbeAddOp1(v, OP_IfNeg, p->iOffset);/*ʵ���ϵ���sqlite3VdbeAddOp3�����޸�ָ��ĵ�ַ*/
		sqlite3VdbeAddOp2(v, OP_Goto, 0, iContinue);/*���������ĵ�ַ*/
		VdbeComment((v, "skip OFFSET records"));/*����ƫ������¼*/
		sqlite3VdbeJumpHere(v, addr);/*�ı�ָ����ַ�Ĳ�����ʹ��ָ����һ��ָ��ĵ�ַ����*/
	}
}

/*
** Add code that will check to make sure the N registers starting at iMem
** form a distinct entry.  iTab is a sorting index that holds previously
** seen combinations of the N values.  A new entry is made in iTab
** if the current N values are new.
** ��Ӵ��룬�����ȷ��N���Ĵ�����ʼiMem�γ�һ����������Ŀ��iTab��һ������������
** Ԥ�ȼ�����Nֵ����ϡ������ǰ��Nֵ���µģ�һ���µ���Ŀ����iTab�в�����
**
** A jump to addrRepeat is made and the N+1 values are popped from the
** stack if the top N elements are not distinct.
** ��������N��Ԫ�ز����ԣ�����ת��addrRepeat��N+1��ֵ��ջ�е�����
*/
static void codeDistinct(
	Parse *pParse,     /* Parsing and code generating context ����ʹ�������*/
	int iTab,          /* A sorting index used to test for distinctness һ��������������Ψһ�ԵĲ���*/
	int addrRepeat,    /* Jump to here if not distinct ���û�С�ȥ���ظ��������˴�*/
	int N,             /* Number of elements Ԫ����Ŀ*/
	int iMem           /* First element ��һ��Ԫ��*/
	){
	Vdbe *v;/*����ṹ��ָ��v*/
	int r1;

	v = pParse->pVdbe;/*�ѽṹ���ԱpParse->pVdbe�����ṹ��ָ��v*/
	r1 = sqlite3GetTempReg(pParse); /*����һ���µļĴ������ڿ����м��������ص���������r1.*/
	sqlite3VdbeAddOp4Int(v, OP_Found, iTab, addrRepeat, iMem, N);/*�Ѳ�����ֵ����������Ȼ�����������������������*/
	sqlite3VdbeAddOp3(v, OP_MakeRecord, iMem, N, r1);/*����sqlite3VdbeAddOp3�����޸�ָ��ĵ�ַ*/
	sqlite3VdbeAddOp2(v, OP_IdxInsert, iTab, r1);/*ʵ��Ҳ��ʹ��sqlite3VdbeAddOp3()ֻ�ǲ�����Ϊǰ4�����޸�ָ��ĵ�ַ*/
	sqlite3ReleaseTempReg(pParse, r1);/*���ɴ��룬�������ı��ʽ�б��ÿ��Ԫ�ص�ֵ�ŵ��Ĵ�����ʼ��Ŀ�����С�����Ԫ���������������ͷżĴ���*/
}

#ifndef SQLITE_OMIT_SUBQUERY/*����SQLITE_OMIT_SUBQUERY�Ƿ񱻺궨���*/
/*
** Generate an error message when a SELECT is used within a subexpression
** (example:  "a IN (SELECT * FROM table)") but it has more than 1 result
** column.  We do this in a subroutine because the error used to occur
** in multiple places.  (The error only occurs in one place now, but we
** retain the subroutine to minimize code disruption.)
** ��һ��select�����ʹ���ӱ��ʽ�Ͳ���һ���������Ϣ(����:a in(select * from table))��
** �������ж���1�Ľ���С��������ӳ���������������Ϊ����ͨ�������ڶ���ط���
** (���ڴ���ֻ������һ���ط����������Ǳ����жϵ��ӳ��򽫴��������ٵ���С��)
*/
static int checkForMultiColumnSelectError(
	Parse *pParse,       /* Parse context. ������� */
	SelectDest *pDest,   /* Destination of SELECT results   select����ļ���*/
	int nExpr            /* Number of result columns returned by SELECT  ����е���Ŀ��select����*/
	){
	int eDest = pDest->eDest;/*��������*/
	if (nExpr > 1 && (eDest == SRT_Mem || eDest == SRT_Set)){/*������������1����select�Ľ������SRT_Mem��SRT_Set*/
		sqlite3ErrorMsg(pParse, "only a single result allowed for "
			"a SELECT that is part of an expression");/*���������Ϣ*/
		return 1;
	}
	else{
		return 0;
	}
}
#endif/*��ֹif*/

/*
** This routine generates the code for the inside of the inner loop
** of a SELECT.
** ��������������Ϊ��select�����ӵ��ڲ����롣
**
** If srcTab and nColumn are both zero, then the pEList expressions
** are evaluated in order to get the data for this row.  If nColumn>0
** then data is pulled from srcTab and pEList is used only to get the
** datatypes for each column.
** ���srcTab��nColumn�����㣬��ôpEList���ʽΪ�˻�������ݽ��и�ֵ��
** ���nColumn>0 ��ô���ݴ�srcTab��������pEListֻ���ڴ�ÿһ�л���������͡�
*/
static void selectInnerLoop(
	Parse *pParse,          /* The parser context �������*/
	Select *p,              /* The complete select statement being coded ������select��䱻����*/
	ExprList *pEList,       /* List of values being extracted  �������е��﷨��*/
	int srcTab,             /* Pull data from this table �����������ȡ����*/
	int nColumn,            /* Number of columns in the source table  Դ�����е���Ŀ*/
	ExprList *pOrderBy,     /* If not NULL, sort results using this key �������NULL��ʹ�����key�Խ����������*/
	int distinct,           /* If >=0, make sure results are distinct ���>=0��ȷ������ǲ�ͬ��*/
	SelectDest *pDest,      /* How to dispose of the results ����������*/
	int iContinue,          /* Jump here to continue with next row �������������һ��*/
	int iBreak              /* Jump here to break out of the inner loop ���������ж��ڲ�ѭ��*/
	){
	Vdbe *v = pParse->pVdbe;/*����һ�������*/
	int i;
	int hasDistinct;        /* True if the DISTINCT keyword is present ���distinct�ؼ��ִ��ڷ���true*/
	int regResult;              /* Start of memory holding result set ��ʼ���ڴ���н����*/
	int eDest = pDest->eDest;   /* How to dispose of results ����������*/
	int iParm = pDest->iSDParm; /* First argument to disposal method ��һ�������Ĵ�����*/
	int nResultCol;             /* Number of result columns ����е���Ŀ*/

	assert(v);/*�ж������*/
	if (NEVER(v == 0)) return;/*�������������ڣ�ֱ�ӷ���*/
	assert(pEList != 0);/*�жϱ��ʽ�б��Ƿ�Ϊ��*/
	hasDistinct = distinct >= 0;/*��ֵ��ȥ���ظ���������*/
	if (pOrderBy == 0 && !hasDistinct){/*���ʹ����ORDERBY��hasDistinctȡ��ֵ*/
		codeOffset(v, p, iContinue);/*����ƫ������VDBE��selectȷ����ƫ�Ʋ�����IContinue*/
	}

	/* Pull the requested columns.
	** ��Ҫ�������ȡ������
	*/
	if (nColumn > 0){/*��������е���Ŀ����0*/
		nResultCol = nColumn;/*���е���Ŀ��������nResultCol*/
	}
	else{
		nResultCol = pEList->nExpr;/*���򣬸�ֵΪ����ȡֵ������*/
	}
	if (pDest->iSdst == 0){/*�����ѯ���ݼ���д�����Ļ�ַ�Ĵ�����ֵΪ0*/
		pDest->iSdst = pParse->nMem + 1;/*��ô��ַ�Ĵ�����ֵ��Ϊ�����﷨������һ����ַ*/
		pDest->nSdst = nResultCol;/*ע��Ĵ���������Ϊ����е�����*/
		pParse->nMem += nResultCol;/*�������ĵ�ַ��Ϊ������ټ��Ͻ���е�����*/
	}
	else{
		assert(pDest->nSdst == nResultCol);/*�жϽ�����мĴ����ĸ����Ƿ������е�������ͬ*/
	}
	regResult = pDest->iSdst;/*�ٰѴ��������ļĴ����ĵ�ַ��Ϊ���������ʼ��ַ*/
	if (nColumn > 0){/*�����������0*/
		for (i = 0; i < nColumn; i++){/*�����ÿһ��*/
			sqlite3VdbeAddOp3(v, OP_Column, srcTab, i, regResult + i);/*�����в������뵽VDBE�ٷ����µ�ָ���ַ*/
		}
	}
	else if (eDest != SRT_Exists){/*�������Ľ����������*/
		/* If the destination is an EXISTS(...) expression, the actual
		** values returned by the SELECT are not required.
		** ���Ŀ����һ��EXISTS(...)���ʽ����select���ص�ʵ��ֵ�ǲ���Ҫ�ġ�
		*/
		sqlite3ExprCacheClear(pParse);  /*����������е�����*/
		sqlite3ExprCodeExprList(pParse, pEList, regResult, eDest == SRT_Output);/*���ɴ��룬�������ı��ʽ�б��ÿ��Ԫ�ص�ֵ�ŵ��Ĵ�����ʼ��Ŀ�����С�����Ԫ��������������*/
	}
	nColumn = nResultCol;/*��������ֵ����е�����*/

	/* If the DISTINCT keyword was present on the SELECT statement
	** and this row has been seen before, then do not make this row
	** part of the result.
	** ���distinct�ؼ�����select����г��֣�����֮ǰ�Ѿ���������ô���в���Ϊ�����һ���֡�
	*/
	if (hasDistinct){/*���ʹ����distinct�ؼ���*/
		assert(pEList != 0);/*���ϵ㣬�жϱ���ȡ��ֵ�б��Ƿ�Ϊ��*/
		assert(pEList->nExpr == nColumn);/*����ȡ��ֵ�б�������Ƿ��������*/
		codeDistinct(pParse, distinct, iContinue, nColumn, regResult);/*���С�ȥ���ظ�������*/
		if (pOrderBy == 0){/*���û��ʹ��ORDERBY�־� */
			codeOffset(v, p, iContinue);/*ʹ��codeOffset�����������������в������*/
		}
	}

	switch (eDest){/*����switch���������ݲ���eDest�ж�����������*/
		/* In this mode, write each query result to the key of the temporary
		** table iParm.
		** ������ģʽ�£�����ʱ��iParmд��ÿ����ѯ�����
		*/
#ifndef SQLITE_OMIT_COMPOUND_SELECT/*����SQLITE_OMIT_COMPOUND_SELECT�Ƿ񱻺궨���*/
	case SRT_Union: {/*���eDestΪSRT_Union��������Ϊ�ؼ��ִ洢������*/
		int r1;
		r1 = sqlite3GetTempReg(pParse);/*����һ���µļĴ��������м���������ֵ����r1*/
		sqlite3VdbeAddOp3(v, OP_MakeRecord, regResult, nColumn, r1);/*��OP_MakeRecord������¼����������VDBE���ٷ���һ����ָ���ַ*/
		sqlite3VdbeAddOp2(v, OP_IdxInsert, iParm, r1);/*��OP_IdxInsert���������룩��������VDBE���ٷ���һ����ָ���ַ*/
		sqlite3ReleaseTempReg(pParse, r1);/*�ͷżĴ�����ʹ����Դ���������Ŀ�ġ����һ���Ĵ�����ǰ�������л��棬��dallocation���Ƴ٣�ֱ��ʹ�õ��мĴ�����ĳ¾�*/
		break;
	}

		/* Construct a record from the query result, but instead of
		** saving that record, use it as a key to delete elements from
		** the temporary table iParm.
		** ����һ����¼�Ĳ�ѯ����������Ǳ���ü�¼��������Ϊ����ʱ��iParmɾ��Ԫ�ص�һ������
		*/
	case SRT_Except: {/*���eDestΪSRT_Except�����union�������Ƴ����*/
		sqlite3VdbeAddOp3(v, OP_IdxDelete, iParm, regResult, nColumn); /*���һ���µ�ָ��VDBEָʾ��ǰ���б�������ָ��ĵ�ַ��*/
		break;
	}
#endif/*��ֹif*/

		/* 
		** Store the result as data using a unique key.
		** �洢����ʹ��Ψһ�ؼ��ֵĽ��
		*/
	case SRT_Table:/*���eDestΪSRT_Table�����������Զ���rowid�Զ�����*/
	case SRT_EphemTab: {/*���eDestΪSRT_EphemTab���򴴽���ʱ���洢Ϊ��SRT_Table�ı�*/
		int r1 = sqlite3GetTempReg(pParse); /*����һ���µļĴ������ڿ����м��������ѷ���ֵ����r1*/
		testcase(eDest == SRT_Table);/*���Դ���Ľ�����ı�����*/
		testcase(eDest == SRT_EphemTab);/*���Դ���Ľ�����ı�Ĵ�С*/
		sqlite3VdbeAddOp3(v, OP_MakeRecord, regResult, nColumn, r1);/*���һ���µ�ָ��VDBEָʾ��ǰ���б�������ָ��ĵ�ַ��*/
		if (pOrderBy){/*�����orderby�־�*/
			pushOntoSorter(pParse, pOrderBy, p, r1);/*�������"V"���ڷ�ѡ�������ƽ���¼��ջ�Ķ���*/
		}
		else{
			int r2 = sqlite3GetTempReg(pParse);/*����һ���µļĴ������ڿ����м��������ѷ���ֵ����r2*/
			sqlite3VdbeAddOp2(v, OP_NewRowid, iParm, r2);/*��OP_NewRowid���½���¼����������VDBE���ٷ���һ����ָ���ַ*/
			sqlite3VdbeAddOp3(v, OP_Insert, iParm, r1, r2);/*��OP_Insert�������¼����������VDBE���ٷ���һ����ָ���ַ*/
			sqlite3VdbeChangeP5(v, OPFLAG_APPEND);  /*����������ӵĲ������ı�p5��������ֵ��*/
			sqlite3ReleaseTempReg(pParse, r2);/*�ͷżĴ�����ʹ����Դ���������Ŀ�ġ����һ���Ĵ�����ǰ�������л��棬��dallocation���Ƴ٣�ֱ��ʹ�õ��мĴ�����ĳ¾�*/
		}
		sqlite3ReleaseTempReg(pParse, r1);/*�ͷżĴ���*/
		break;
	}

#ifndef SQLITE_OMIT_SUBQUERY/*����SQLITE_OMIT_SUBQUERY�Ƿ񱻺궨���*/
		/* 
		** If we are creating a set for an "expr IN (SELECT ...)" construct,
		** then there should be a single item on the stack.  Write this
		** item into the set table with bogus data.
		** ������Ǵ���һ��"expr IN (SELECT ...)"���ʽ ����ô�ڶ�ջ�Ͼ�Ӧ��
		** ��һ�������Ķ��󡣰��������д���������ݱ�
		*/
	case SRT_Set: {/*���eDestΪSRT_Set��������Ϊ�ؼ��ִ�������*/
		assert(nColumn == 1);/*��ϵ㣬��������1*/
		p->affinity = sqlite3CompareAffinity(pEList->a[0].pExpr, pDest->affSdst);/*���ݱ�ͽ�������洢�ṹ����׺��Խ����*/
		if (pOrderBy){
			/* 
			** At first glance you would think we could optimize out the
			** ORDER BY in this case since the order of entries in the set
			** does not matter.  But there might be a LIMIT clause, in which
			** case the order does matter
			** һ��ʼ����ʱ�������Ϊ������������������Ż���order by��
			** ���ǣ�ʹ����LIMIT�־��ʱ���������Ҫ��
			*/
			pushOntoSorter(pParse, pOrderBy, p, regResult);/*����ջ��*/
		}
		else{
			int r1 = sqlite3GetTempReg(pParse);/*����һ���Ĵ������洢�м������*/
			sqlite3VdbeAddOp4(v, OP_MakeRecord, regResult, 1, r1, &p->affinity, 1);/*��OP_MakeRecord��������VDBE���ٷ���һ����ָ���ַ*/
			sqlite3ExprCacheAffinityChange(pParse, regResult, 1);/*��¼�׺����͵����ݵĸı�ļ����Ĵ�������ʼ��ַ*/
			sqlite3VdbeAddOp2(v, OP_IdxInsert, iParm, r1);/*��OP_IdxInsert��������VDBE���ٷ���һ����ָ���ַ*/
			sqlite3ReleaseTempReg(pParse, r1);/*�ͷżĴ���*/
		}
		break;
	}

		/* 
		** If any row exist in the result set, record that fact and abort.
		** ����κ�һ���ڽ�����д��ڣ���¼��һ��ʵ����ֹ��
		*/
	case SRT_Exists: {/*���eDestΪSRT_Exists����������Ϊ�մ洢1*/
		sqlite3VdbeAddOp2(v, OP_Integer, 1, iParm);/*��OP_Integer��������VDBE���ٷ���һ����ָ���ַ*/
		/* 
		** The LIMIT clause will terminate the loop for us
		** limit�Ӿ佫��ֹ���ǵ�ѭ��
		*/
		break;
	}

		/* 
		** If this is a scalar select that is part of an expression, then
		** store the results in the appropriate memory cell and break out
		** of the scan loop.
		** �������ѡ���Ǳ��ʽ��һ���֣�Ȼ�󽫽���洢���ʵ��Ĵ洢��Ԫ����ֹɨ��ѭ����
		*/
	case SRT_Mem: {/*���eDestΪSRT_Mem���򽫽���洢�ڴ洢��Ԫ*/
		assert(nColumn == 1);/*���ϵ㣬�жϱ���ȡ��ֵ�б��Ƿ�Ϊ��*/
		if (pOrderBy){
			pushOntoSorter(pParse, pOrderBy, p, regResult);/*����ջ��*/
		}
		else{
			sqlite3ExprCodeMove(pParse, regResult, iParm, 1);/*�ͷżĴ����е����ݣ����ּĴ��������ݼ�ʱ����*/
			/* 
			** The LIMIT clause will jump out of the loop for us
			** limit�Ӿ��Ϊ��������ѭ��
			*/
		}
		break;
	}
#endif /* #ifndef SQLITE_OMIT_SUBQUERY */
		/* 
		** Send the data to the callback function or to a subroutine.  In the
		** case of a subroutine, the subroutine itself is responsible for
		** popping the data from the stack.
		** �����ݷ��͵��ص��������ӳ������ӳ��������£��ӳ�������Ӷ�ջ�е������ݡ�
		*/
		testcase(eDest == SRT_Coroutine);/*���Դ��������Ƿ���Эͬ����*/
		testcase(eDest == SRT_Output);   /*���Դ��������Ƿ�Ҫ���*/
		if (pOrderBy){/*���������OEDERBY�Ӿ�*/
			int r1 = sqlite3GetTempReg(pParse);/*����һ���Ĵ������洢�м������*/
			sqlite3VdbeAddOp3(v, OP_MakeRecord, regResult, nColumn, r1);/*��OP_MakeRecord��������VDBE���ٷ���һ����ָ���ַ*/
			pushOntoSorter(pParse, pOrderBy, p, r1);/*����ջ��*/
			sqlite3ReleaseTempReg(pParse, r1);/*�ͷżĴ���*/
		}
		else if (eDest == SRT_Coroutine){/*�������������Эͬ����*/
			sqlite3VdbeAddOp1(v, OP_Yield, pDest->iSDParm);/*��OP_Yield��������VDBE���ٷ���һ����ָ���ַ*/
		}
		else{
			sqlite3VdbeAddOp2(v, OP_ResultRow, regResult, nColumn);/*��OP_ResultRow��������VDBE���ٷ���һ����ָ���ַ*/
			sqlite3ExprCacheAffinityChange(pParse, regResult, nColumn);/*��¼�׺����͵����ݵĸı�ļ����Ĵ�������ʼ��ַ*/
		}
		break;
		}

#if !defined(SQLITE_OMIT_TRIGGER)/*��������*/
		/* 
		** Discard the results.  This is used for SELECT statements inside
		** the body of a TRIGGER.  The purpose of such selects is to call
		** user-defined functions that have side effects.  We do not care
		** about the actual results of the select.
		** ����������������ڴ�������select��䡣����ѡ���Ŀ����Ҫ�����û����庯����
		** ���ǲ��ع���ʵ�ʵ�ѡ������
		*/
	default: {/*Ĭ��������*/
		assert(eDest == SRT_Discard);/*�������������SRT_Discard��������*/
		break;
	}
#endif/*�����������*/
	}

	/* Jump to the end of the loop if the LIMIT is reached.  Except, if
	** there is a sorter, in which case the sorter has already limited
	** the output for us.
	** ������һ����ѡ��������������·�ѡ���Ѿ����������ǵ������
	** �������limit�Ӿ����������ת��ѭ��������
	*/
	if (pOrderBy == 0 && p->iLimit){/*���������ORDERBY���Һ���limit�Ӿ�*/
		sqlite3VdbeAddOp3(v, OP_IfZero, p->iLimit, iBreak, -1);/*��OP_IfZero��������VDBE���ٷ���һ����ָ���ַ*/
	}
}

/*
** Given an expression list, generate a KeyInfo structure that records
** the collating sequence for each expression in that expression list.
** ����һ�����ʽ�б�����һ��KeyInfo�ṹ����¼�ڸñ��ʽ�б��е�ÿ�����ʽ���������С�
**
** If the ExprList is an ORDER BY or GROUP BY clause then the resulting
** KeyInfo structure is appropriate for initializing a virtual index to
** implement that clause.  If the ExprList is the result set of a SELECT
** then the KeyInfo structure is appropriate for initializing a virtual
** index to implement a DISTINCT test.
** ���ExprList��һ��order by����group by�Ӿ䣬��ôKeyInfo�ṹ���ʺϳ�ʼ����������ȥʵ��
** ��Щ�Ӿ䡣���ExprList��select�Ľ��������ôKeyInfo�ṹ���ʺϳ�ʼ��һ����������ȥʵ
** ��DISTINCT���ԡ�
**
** Space to hold the KeyInfo structure is obtain from malloc.  The calling
** function is responsible for seeing that this structure is eventually
** freed.  Add the KeyInfo structure to the P4 field of an opcode using
** P4_KEYINFO_HANDOFF is the usual way of dealing with this.
** ����KeyInfo�ṹ��Ŀռ�����malloc��á����ú������𿴵�����ṹ�������ͷš�
** KeyInfo�ṹ��ӵ�ʹ��P4_KEYINFO_HANDOFF P4��һ����������ͨ���Ĵ���ʽ��
*/
static KeyInfo *keyInfoFromExprList(Parse *pParse, ExprList *pList){/*���徲̬�Ľṹ��ָ�뺯��keyInfoFromExprList*/
	sqlite3 *db = pParse->db;/*�ѽṹ��������pParse�ĳ�Ա����db�����ṹ��������sqlite3��ָ��db*/
	int nExpr;
	KeyInfo *pInfo;/*����ṹ��������KeyInfo��ָ��pInfo*/
	struct ExprList_item *pItem;/*����ṹ��������ExprList_item��ָ��pItem*/
	int i;

	nExpr = pList->nExpr;/*�������ʽ�б��б��ʽ�ĸ���*/
	pInfo = sqlite3DbMallocZero(db, sizeof(*pInfo) + nExpr*(sizeof(CollSeq*) + 1));/*���䲢����ڴ棬�����СΪ�ڶ����������ڴ�*/
	if (pInfo){/*������ڹؼ��ֽṹ��*/
		pInfo->aSortOrder = (u8*)&pInfo->aColl[nExpr];/*���ùؼ���Ϣ�ṹ�������Ϊ�ؼ���Ϣ�ṹ���б��ʽ�к��еĹؼ��֣�����aColl[1]��ʾΪÿһ���ؼ��ֽ�������*/
		pInfo->nField = (u16)nExpr;/*������nExpr����ǿ������ת����u16������ pInfo->nField*/
		pInfo->enc = ENC(db);/*�ؼ���Ϣ�ṹ���б��뷽ʽΪdb�ı��뷽ʽ*/
		pInfo->db = db;/*�ؼ���Ϣ�ṹ�������ݿ�Ϊ��ǰʹ�õ����ݿ�*/
		for (i = 0, pItem = pList->a; i < nExpr; i++, pItem++){/*������ǰ�ı��ʽ�б�*/
			CollSeq *pColl;/*����ṹ������ΪCollSeq��ָ��pColl*/
			pColl = sqlite3ExprCollSeq(pParse, pItem->pExpr); /*Ϊ���ʽpExpr����Ĭ������˳�����û��Ĭ���������ͣ�����0.*/
			if (!pColl){/*���û��ָ������ķ���*/
				pColl = db->pDfltColl;/*�����ݿ���Ĭ�ϵ����򷽷���ֵ��pColl*/
			}
			pInfo->aColl[i] = pColl;/*�ؼ���Ϣ�ṹ���жԹؼ�������������Ԫ�ض�Ӧ���ʽ�����������*/
			pInfo->aSortOrder[i] = pItem->sortOrder;/*�ؼ���Ϣ�ṹ���������˳��Ϊ�﷨���������﷨����ʽ�����򷽷�*/
			/*��ע������ǣ���û�п�����������ķ������������Ϊ��ָ��ʹ��ĳ������ķ�ʽ�����������û��ʹ��ϵͳĬ�ϵġ��ٰ��﷨���б��ʽ������������Ӧ����һ��������ֻ�Ǳ��ķ�ʽ��һ��*/
		}
	}
	return pInfo;/*��������ؼ���Ϣ�ṹ��*/
}

#ifndef SQLITE_OMIT_COMPOUND_SELECT/*����SQLITE_OMIT_COMPOUND_SELECT�Ƿ񱻺궨���*/
/*
** Name of the connection operator, used for error messages.
** ���ӷ������ƣ����ڱ�ʾ������Ϣ��
*/
static const char *selectOpName(int id){/*���徲̬����ֻ�����ַ���ָ��selectOpName*/
	char *z;/*�����ַ���ָ��z*/
	switch (id){/*switch�������ж�id��ֵ*/
	case TK_ALL:       z = "UNION ALL";   break;/*�������idΪTK_ALL�������ַ�"UNION ALL"*/
	case TK_INTERSECT: z = "INTERSECT";   break;/*�������idΪTK_INTERSECT�������ַ�"INTERSECT"*/
	case TK_EXCEPT:    z = "EXCEPT";      break;/*�������idΪTK_EXCEPT�������ַ�"EXCEPT"*/
	default:           z = "UNION";       break;/*Ĭ�������£������ַ�"UNION"*/
	}
	return z;  /*�������ӷ�������*/
}
#endif /* SQLITE_OMIT_COMPOUND_SELECT */

#ifndef SQLITE_OMIT_EXPLAIN/*����SQLITE_OMIT_EXPLAIN�Ƿ񱻺궨���*/
/*
** Unless an "EXPLAIN QUERY PLAN" command is being processed, this function
** is a no-op. Otherwise, it adds a single row of output to the EQP result,
** where the caption is of the form:
**
**   "USE TEMP B-TREE FOR xxx"
**
** where xxx is one of "DISTINCT", "ORDER BY" or "GROUP BY". Exactly which
** is determined by the zUsage argument.
** ����һ��"EXPLAIN QUERY PLAN"�������ڴ�������������ܾ���һ���ղ�����
** ����������һ������������е�EQP������������ʽΪ:
** "USE TEMP B-TREE FOR xxx"
** ����xxx��"distinct","order by",����"group by"�е�һ�����������ĸ���
** zUsage����������
*/
static void explainTempTable(Parse *pParse, const char *zUsage){
	if (pParse->explain == 2){/*����﷨�������е�explain�ǵڶ���*/
		Vdbe *v = pParse->pVdbe;/*����һ�������*/
		char *zMsg = sqlite3MPrintf(pParse->db, "USE TEMP B-TREE FOR %s", zUsage);/*������ĸ�ʽ�����ݴ��ݸ�zMsg������%S �Ǵ���Ĳ�����Usage*/
		sqlite3VdbeAddOp4(v, OP_Explain, pParse->iSelectId, 0, 0, zMsg, P4_DYNAMIC); /*���һ�������룬���а�����Ϊָ���p4ֵ��*/
	}
  }
}

/*
** Assign expression b to lvalue a. A second, no-op, version of this macro
** is provided when SQLITE_OMIT_EXPLAIN is defined. This allows the code
** in sqlite3Select() to assign values to structure member variables that
** only exist if SQLITE_OMIT_EXPLAIN is not defined without polluting the
** code with #ifndef directives.
** ��ֵ���ʽb����ֵa���ڶ����޲���������İ汾��SQLITE_OMIT_EXPLAIN �����ṩ��������
** sqlite3Select()�Ĵ�����ṹ��ĳ�Ա��������ֵ�����SQLITE_OMIT_EXPLAINû��
** ���壬û��#ifndef ָ��Ĵ�����Ż���ڡ�
*/
# define explainSetInteger(a, b) a = b/*�궨��*/

#else
/* No-op versions of the explainXXX() functions and macros. explainXXX() �����ͺ��޲������İ汾��*/
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
** ����һ��"EXPLAIN QUERY PLAN"�������ڴ���������ܾ���һ���ղ�����
** ����������һ������������е�EQP������������ʽΪ:
** "COMPOSITE SUBQUERIES iSub1 and iSub2 (op)"
** "COMPOSITE SUBQUERIES iSub1 and iSub2 USING TEMP B-TREE (op)"
** iSub1��iSub2������Ϊ��Ӧ�Ĵ��ݺ�����������������ͬ���ƵĲ���
** ���ı���ʾ������"op"����TK_UNION, TK_EXCEPT,TK_INTERSECT����TK_ALL֮һ��
** �������bUseTmp��false��ʹ�õ�һ��ʽ�����������true��ʹ�õڶ���ʽ��
*/
static void explainComposite(
	Parse *pParse,                  /* Parse context �������*/
	int op,                         /* One of TK_UNION, TK_EXCEPT etc.   TK_UNION, TK_EXCEPT��������е�һ��*/
	int iSub1,                      /* Subquery id 1 �Ӳ�ѯid 1*/
	int iSub2,                      /* Subquery id 2 �Ӳ�ѯid 2*/
	int bUseTmp                     /* True if a temp table was used �����ʱ��ʹ�þ���true*/
	){
	assert(op == TK_UNION || op == TK_EXCEPT || op == TK_INTERSECT || op == TK_ALL);/*����op�Ƿ���TK_UNION��TK_EXCEPT��TK_INTERSECT��TK_ALL*/
	if (pParse->explain == 2){/*���pParse->explain���ַ�z��ͬ*/
		Vdbe *v = pParse->pVdbe;/*����һ�������*/
		char *zMsg = sqlite3MPrintf(/*���ñ����Ϣ*/
			pParse->db, "COMPOUND SUBQUERIES %d AND %d %s(%s)", iSub1, iSub2,
			bUseTmp ? "USING TEMP B-TREE " : "", selectOpName(op)
			);/*���Ӳ�ѯ1���Ӳ�ѯ2���﷨���ݸ�ֵ��zMsg*/
		sqlite3VdbeAddOp4(v, OP_Explain, pParse->iSelectId, 0, 0, zMsg, P4_DYNAMIC);/*��OP_Explain���������������Ȼ�󷵻�һ����ַ����ַΪP4_DYNAMICָ���е�ֵ*/
	}
}
#else
/* No-op versions of the explainXXX() functions and macros. explainXXX()�����ͺ���޲����汾��*/
# define explainComposite(v,w,x,y,z)
#endif

/*
** If the inner loop was generated using a non-null pOrderBy argument,
** then the results were placed in a sorter.  After the loop is terminated
** we need to run the sorter and output the results.  The following
** routine generates the code needed to do that.
** ����ڲ�ѭ��ʹ��һ���ǿ�pOrderBy���ɲ���,Ȼ��ѽ��������һ����ѡ����
** ѭ����ֹ��������Ҫ���з�ѡ�����������������������������Ĵ��롣
*/
static void generateSortTail(
	Parse *pParse,    /* Parsing context �������*/
	Select *p,        /* The SELECT statement   select���*/
	Vdbe *v,          /* Generate code into this VDBE  ��VDBE�����ɴ���**/
	int nColumn,      /* Number of columns of data �������е���Ŀ*/
	SelectDest *pDest /* Write the sorted results here ������д��������*/
	){
	int addrBreak = sqlite3VdbeMakeLabel(v);     /* Jump here to exit loop ��ת�������˳�ѭ��*/
	int addrContinue = sqlite3VdbeMakeLabel(v);  /* Jump here for next cycle ��ת�����������һ��ѭ��*/
	int addr;
	int iTab;
	int pseudoTab = 0;
	ExprList *pOrderBy = p->pOrderBy;/*��Select�ṹ����ORDERBY��ֵ�����ʽ�б��е�ORDERBY���ʽ����*/

	int eDest = pDest->eDest;/*����ѯ������д���ʽ���ݸ�eDest*/
	int iParm = pDest->iSDParm;/*����ѯ������д���ʽ�еĲ������ݸ�iParm*/

	int regRow;
	int regRowid;

	iTab = pOrderBy->iECursor;/*��pOrderBy->iECursor��������iTab*/
	regRow = sqlite3GetTempReg(pParse);/*ΪpParse�﷨������һ���Ĵ���,�洢������м���*/
	if (eDest == SRT_Output || eDest == SRT_Coroutine){/*�������ʽ��SRT_Output���������SRT_Coroutine��Эͬ����*/
		pseudoTab = pParse->nTab++;/*��ν������﷨���б�������pseudoTab�����*/
		sqlite3VdbeAddOp3(v, OP_OpenPseudo, pseudoTab, regRow, nColumn);/*��OP_Explain�������������*/
		regRowid = 0;
	}
	else{
		regRowid = sqlite3GetTempReg(pParse);/*ΪpParse�﷨������һ���Ĵ���,�洢������м���*/
	}
	if (p->selFlags & SF_UseSorter){/*���Select�ṹ���е�selFlags����ֵΪSF_UseSorter��ʹ�÷ּ������������*/
		int regSortOut = ++pParse->nMem;/*����Ĵ����������Ƿ����﷨�����ڴ���+1*/
		int ptab2 = pParse->nTab++;/*�������﷨���б�ĸ�����ֵ��ptab2*/
		sqlite3VdbeAddOp3(v, OP_OpenPseudo, ptab2, regSortOut, pOrderBy->nExpr + 2);/*��OP_OpenPseudo�����������������VDBE�����ر��ʽ�б��б��ʽ������ֵ+2*/
		addr = 1 + sqlite3VdbeAddOp2(v, OP_SorterSort, iTab, addrBreak);/*��OP_SorterSort���ּ����������򣩽���VDBE�����صĵ�ַ+1��ֵ��addr*/
		codeOffset(v, p, addrContinue);/*����ƫ����������addrContinue����һ��ѭ��Ҫ�����ĵ�ַ*/
		sqlite3VdbeAddOp2(v, OP_SorterData, iTab, regSortOut);/*��OP_SorterData�������������*/
		sqlite3VdbeAddOp3(v, OP_Column, ptab2, pOrderBy->nExpr + 1, regRow);/*��OP_Column�������������*/
		sqlite3VdbeChangeP5(v, OPFLAG_CLEARCACHE);/*�ı�OPFLAG_CLEARCACHE��������棩�Ĳ���������Ϊ��ַ����sqlite3VdbeAddOp3��sqlite3VdbeAddOp2���������ı��˵�ַ*/
	}
	else{
		addr = 1 + sqlite3VdbeAddOp2(v, OP_Sort, iTab, addrBreak);/*��OP_Sort������������������صĵ�ַ+1*/
		codeOffset(v, p, addrContinue);/*����ƫ����������addrContinue����һ��ѭ��Ҫ�����ĵ�ַ*/
		sqlite3VdbeAddOp3(v, OP_Column, iTab, pOrderBy->nExpr + 1, regRow);/*��OP_Column��������VDBE���ٰ�OP_Column�ĵ�ַ����*/
	}
	switch (eDest){/*switch����������eDest��ѡ�������Ĵ�����*/
	case SRT_Table:/*���eDestΪSRT_Table�����������Զ���rowid�Զ�����*/
	case SRT_EphemTab: {/*���eDestΪSRT_EphemTab���򴴽���ʱ���洢Ϊ��SRT_Table�ı�*/
		testcase(eDest == SRT_Table);/*����ʽ���Ƿ�SRT_Table*/
		testcase(eDest == SRT_EphemTab);/*����ʽ���Ƿ�SRT_EphemTab*/
		sqlite3VdbeAddOp2(v, OP_NewRowid, iParm, regRowid);/*��OP_NewRowid��������VDBE���ٷ�����������ĵ�ַ*/
		sqlite3VdbeAddOp3(v, OP_Insert, iParm, regRow, regRowid);/*��OP_Insert��������VDBE���ٷ�����������ĵ�ַ*/
		sqlite3VdbeChangeP5(v, OPFLAG_APPEND);/*�ı�OPFLAG_APPEND������·��������Ϊ��ַ����sqlite3VdbeAddOp2������sqlite3VdbeAddOp3���������ı��˵�ַ*/
		break;
	}
#ifndef SQLITE_OMIT_SUBQUERY/*����SQLITE_OMIT_SUBQUERY�Ƿ񱻺궨���*/
	case SRT_Set: {/*���eDestΪSRT_Set��������Ϊ�ؼ��ִ�������*/
		assert(nColumn == 1);/*����ϵ㣬�ж������Ƿ����1*/
		sqlite3VdbeAddOp4(v, OP_MakeRecord, regRow, 1, regRowid, &p->affinity, 1);/*���һ��OP_MakeRecord��������������ֵ��Ϊһ��ָ��*/
		sqlite3ExprCacheAffinityChange(pParse, regRow, 1); /*��¼��iStart��ʼ��������iCount�Ĵ����еĸı����ʵ��*/
		sqlite3VdbeAddOp2(v, OP_IdxInsert, iParm, regRowid);/*��OP_IdxInsert���������룩��������VDBE���ٷ�����������ĵ�ַ*/
		break;
	}
	case SRT_Mem: {/*���eDestΪSRT_Mem���򽫽���洢�ڴ洢��Ԫ*/
		assert(nColumn == 1);/*����ϵ㣬�ж������Ƿ����1*/
		sqlite3ExprCodeMove(pParse, regRow, iParm, 1);/*�ͷżĴ����е����ݣ����ּĴ��������ݼ�ʱ����*/
		/* 
		** The LIMIT clause will terminate the loop for us
		** limit�Ӿ佫Ϊ������ֹѭ��
		*/
		break;
	}
#endif/*��ֹif*/
	default: {
		int i;
		assert(eDest == SRT_Output || eDest == SRT_Coroutine); /*����ϵ㣬�жϽ�������������Ƿ���SRT_Output���������SRT_Coroutine��Эͬ����*/
		testcase(eDest == SRT_Output);/*�����Ƿ����SRT_Output*/
		testcase(eDest == SRT_Coroutine);/*�����Ƿ����SRT_Coroutine*/
		for (i = 0; i<nColumn; i++){/*������*/
			assert(regRow != pDest->iSdst + i);/*����ϵ㣬�жϼĴ����ı��ֵ�����ڻ�ַ�Ĵ����ı��ֵ+i*/
			sqlite3VdbeAddOp3(v, OP_Column, pseudoTab, i, pDest->iSdst + i);/*��OP_Column��������VDBE���ٷ�����������ĵ�ַ*/
			if (i == 0){/*���û����*/
				sqlite3VdbeChangeP5(v, OPFLAG_CLEARCACHE);/*�ı�OPFLAG_CLEARCACHE��������棩�Ĳ���������Ϊ��ַ����sqlite3VdbeAddOp3���������ı��˵�ַ*/
			}
		}
		if (eDest == SRT_Output){/*���������Ĵ���ʽ��SRT_Output*/
			sqlite3VdbeAddOp2(v, OP_ResultRow, pDest->iSdst, nColumn);/*��OP_ResultRow��������VDBE���ٷ�����������ĵ�ַ*/
			sqlite3ExprCacheAffinityChange(pParse, pDest->iSdst, nColumn);/*�����﷨��pParse���Ĵ����е��׺�������*/
		}
		else{
			sqlite3VdbeAddOp1(v, OP_Yield, pDest->iSDParm);/*��OP_Yield��������VDBE���ٷ�����������ĵ�ַ*/
		}
		break;
	}
	}
	sqlite3ReleaseTempReg(pParse, regRow);/*�ͷżĴ���*/
	sqlite3ReleaseTempReg(pParse, regRowid);/*�ͷżĴ���*/

	/* The bottom of the loop
	** ѭ���ĵײ�
	*/
	sqlite3VdbeResolveLabel(v, addrContinue);/*addrContinue��Ϊ��һ������ָ��ĵ�ַ������addrContinue�����ȵ���sqlite3VdbeMakeLabel����*/
	if (p->selFlags & SF_UseSorter){/*selFlags��ֵ��SF_UseSorter*/
		sqlite3VdbeAddOp2(v, OP_SorterNext, iTab, addr);/*��OP_SorterNext��������VDBE���ٷ�����������ĵ�ַ*/
	}
	else{
		sqlite3VdbeAddOp2(v, OP_Next, iTab, addr);/*��OP_Next��������VDBE���ٷ�����������ĵ�ַ*/
	}
	sqlite3VdbeResolveLabel(v, addrBreak);/*addrBreak��Ϊ��һ������ָ��ĵ�ַ������addrBreak�����ȵ���sqlite3VdbeMakeLabel����*/
	if (eDest == SRT_Output || eDest == SRT_Coroutine){/*���������Ĵ���ʽSRT_Output��SRT_Coroutine*/
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
**
**
** ����һ��ָ����ʽpExpr ���� 'declaration type'���ַ�����
** ����ַ���������Ϊ��̬�����ߡ�
** ������ʽ��һ�У�����������ȷ�е��������Ͷ���������
** create table ����л�ȡ��ROWID�ֶε�������������������һ�����ʽ
** ����Ϊ��Ϊһ�����Ӳ�ѯ���Ǹ��ӵġ������������SELECT���
** �Ľ�����ı�ﱻ��Ϊ������������С�
**   SELECT col FROM tbl;
**   SELECT (SELECT col FROM tbl;
**   SELECT (SELECT col FROM tbl);
**   SELECT abc FROM (SELECT col AS abc FROM tbl);
** ��������������κα��ʽ���ǿյġ�
*/
static const char *columnType(/*���徲̬����ֻ�����ַ���ָ��columnType*/
	NameContext *pNC, /*����һ�����������Ľṹ�壨����������е����֣�*/
	Expr *pExpr,
	const char **pzOriginDb,/*����ֻ�����ַ��Ͷ���ָ��pzOriginDb*/
	const char **pzOriginTab,/*����ֻ�����ַ��Ͷ���ָ��pzOriginTab*/
	const char **pzOriginCol,/*����ֻ�����ַ��Ͷ���ָ��pzOriginCol*/
	){
	char const *zType = 0;
	char const *zOriginDb = 0;
	char const *zOriginTab = 0;
	char const *zOriginCol = 0;
	int j;
	if (NEVER(pExpr == 0) || pNC->pSrcList == 0) return 0;

	switch (pExpr->op){/*�������ʽ�еĲ���*/
	case TK_AGG_COLUMN:
	case TK_COLUMN: {
		/* 
		** The expression is a column. Locate the table the column is being
		** extracted from in NameContext.pSrcList. This table may be real
		** database table or a subquery.
		** ���ʽ��һ���С�����λ�ı���д�NameContext.pSrcList����ȡ��
		** ������������ʵ�����ݿ��������һ���Ӳ�ѯ��
		*/
		Table *pTab = 0;            /* Table structure column is extracted from ��ṹ�б���ȡ*/
		Select *pS = 0;             /* Select the column is extracted from ѡ���б���ȡ*/
		int iCol = pExpr->iColumn;  /* Index of column in pTab ��������pTab��*/
		testcase(pExpr->op == TK_AGG_COLUMN);/*������ʽ�Ĳ����Ƿ���TK_AGG_COLUMN��Ƕ���У�*/
		testcase(pExpr->op == TK_COLUMN);/*������ʽ�Ĳ����Ƿ���TK_COLUMN����������*/
		while (pNC && !pTab){/*���������Ľṹ����ڣ�����ȡ�ı�ṹ�У�����һ������ȡ������ɵı�������*/
			SrcList *pTabList = pNC->pSrcList;/*���������Ľṹ�����б�ֵ������FROM����Դ����Ӳ�ѯ������б�*/
			for (j = 0; j<pTabList->nSrc && pTabList->a[j].iCursor != pExpr->iTable; j++);/*������ѯ�б�*/
			if (j<pTabList->nSrc){/*���jС���б��б���ܸ���*/
				pTab = pTabList->a[j].pTab;/*��ֵ�б��еĵ�j-1���Table�ṹ���ʵ�����pTab*/
				pS = pTabList->a[j].pSelect;/*��ֵ�б��еĵ�j-1���select�ṹ���ps*/
			}
			else{
				pNC = pNC->pNext;/*���򣬽����������Ľṹ�����һ���ⲿ���������ĸ�ֵ��pNC����*/
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
			** ��ͬһʱ�䣬���紥������"SELECT new.x "���뽫��������״̬���С�����ʱ������
			** ���������鴥��������������ɵģ���˸��������ٿ��ܡ����ǣ�����Ȼ����������
			** ���������䣺
			** CREATE TABLE t1(col INTEGER);
			** SELECT (SELECT t1.col) FROM FROM t1;
			** ��columnType()��������ѡ���еı��ʽ"t1.col"������������£������е�����Ϊ�գ�
			** ��ʹ��ȷʵӦ��"INTEGER"��
			** �ⲻ��һ�����⣬��Ϊ" t1.col "���������Ǵ�δʹ�ù�����columnType ()�����õ�
			** ���ʽ"(SELECT t1.col)" ���򷵻���ȷ������(����������TK_SELECT��֧)��
			*/
			break;
		}

		assert(pTab && pExpr->pTab == pTab);
		if (pS){
			/* The "table" is actually a sub-select or a view in the FROM clause
			** of the SELECT statement. Return the declaration type and origin
			** data for the result-set column of the sub-select.
			** "��"ʵ������һ����ѡ�񣬻�����һ����select����from�Ӿ����ͼ��
			** �����������ͺ���Դ���ݵ���ѡ��Ľ�����С�
			*/
			if (iCol >= 0 && ALWAYS(iCol < pS->pEList->nExpr)){
				/* If iCol is less than zero, then the expression requests the
				** rowid of the sub-select or view. This expression is legal (see
				** test case misc2.2.2) - it always evaluates to NULL.
				** ���iColС���㣬����ʽ������ѡ�����ͼ��rowid��
				** ���ֱ��ʽ�Ϸ���(�����԰���misc2.2.2)-��ʼ�ռ���Ϊ�ա�
				*/
				NameContext sNC;
				Expr *p = pS->pEList->a[iCol].pExpr;/*����ȡ������ɵ�select�ṹ���б��ʽ�б��е�i�����ʽ��ֵ��p*/
				sNC.pSrcList = pS->pSrc;/*����ȡ������ɵ�select�ṹ����pSrc��FROM�Ӿ䣩��ֵ��pSrcList��һ���������������������ԣ�*/
				sNC.pNext = pNC;/*���������Ľṹ�帳ֵ����ǰ���������Ľṹ���nextָ��*/
				sNC.pParse = pNC->pParse;/*���������Ľṹ���е��﷨��������ֵ����ǰ���������Ľṹ����﷨������*/
				zType = columnType(&sNC, p, &zOriginDb, &zOriginTab, &zOriginCol); /*�����ɵ��������͸�ֵ��zType*/
			}
		}
		else if (ALWAYS(pTab->pSchema)){/*pTab���ģʽ����*/
			/* A real table *//*һ����ʵ�ı�*/
			assert(!pS);/*����ϵ㣬�ж�Select�ṹ���Ƿ�Ϊ��*/
			if (iCol<0) iCol = pTab->iPKey;/*����к�С��0�������еĹؼ����������Ԫ�ظ�ֵ��ICol*/
			assert(iCol == -1 || (iCol >= 0 && iCol<pTab->nCol));/*����ϵ㣬�ж�ICol��ȷ�����ĸ���Χ*/
			if (iCol<0){/*���ICol��С��0*/
				zType = "INTEGER";/*�����Ͷ���Ϊ����*/
				zOriginCol = "rowid";/*�ؼ���Ϊrowid*/
			}
			else{
				zType = pTab->aCol[iCol].zType;/*���򣬶�������Ϊ���ͱ��е�iCol������*/
				zOriginCol = pTab->aCol[iCol].zName;/*���͵�����Ϊ�ͱ��е�iCol������*/
			}
			zOriginTab = pTab->zName;/*ʹ��Ĭ�ϵ����֣���������*/
			if (pNC->pParse){/*������������Ľṹ���е��﷨����������*/
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
		assert(ExprHasProperty(pExpr, EP_xIsSelect));/*����ϵ㣬�����Ƿ����EP_xIsSelect���ʽ*/
		sNC.pSrcList = pS->pSrc;/*��SELECT�ṹ����FROM�Ӿ�����Ը�ֵ�����������Ľṹ����FROM�Ӿ��б�*/
		sNC.pNext = pNC;/*���������Ľṹ�帳ֵ����ǰ���������Ľṹ���nextָ��*/
		sNC.pParse = pNC->pParse;/*�����������Ľṹ���з����﷨����ֵ����ǰ�����ṹ��ķ����﷨������*/
		zType = columnType(&sNC, p, &zOriginDb, &zOriginTab, &zOriginCol); /*������������*/
		break;
	}
#endif
	}

	if (pzOriginDb){/*�������ԭʼ�����ݿ�*/
		assert(pzOriginTab && pzOriginCol);/*����ϵ㣬�жϱ�����Ƿ����*/
		*pzOriginDb = zOriginDb;/*�ļ������ݿ⸳ֵ�����ݿ����pzOriginDb*/
		*pzOriginTab = zOriginTab;/*�ļ��б�ֵ�������pzOriginTab*/
		*pzOriginCol = zOriginCol;/*�ļ����и�ֵ���б���pzOriginCol*/
	}
	return zType;/*����������*/
}


/*
** Generate code that will tell the VDBE the declaration types of columns
** in the result set.
** ���ɴ��룬����VDBE�ڽ�����е��е��������͵ġ�
*/
static void generateColumnTypes(
	Parse *pParse,      /* Parser context �������*/
	SrcList *pTabList,  /* List of tables �����ļ��ϵ��﷨��*/
	ExprList *pEList    /* Expressions defining the result set �������е��﷨��*/
	){
#ifndef SQLITE_OMIT_DECLTYPE/*����SQLITE_OMIT_DECLTYPE�Ƿ񱻺궨���*/
	Vdbe *v = pParse->pVdbe;
	int i;
	NameContext sNC;
	sNC.pSrcList = pTabList;
	sNC.pParse = pParse;
	for (i = 0; i < pEList->nExpr; i++){
		Expr *p = pEList->a[i].pExpr;
		const char *zType;
#ifdef SQLITE_ENABLE_COLUMN_METADATA/*����SQLITE_ENABLE_COLUMN_METADATA�Ƿ񱻺궨���*/
		const char *zOrigDb = 0;
		const char *zOrigTab = 0;
		const char *zOrigCol = 0;
		zType = columnType(&sNC, p, &zOrigDb, &zOrigTab, &zOrigCol);

		/* The vdbe must make its own copy of the column-type and other
		** column specific strings, in case the schema is reset before this
		** virtual machine is deleted.
		** Ϊ��ֹ���������ɾ��ǰ�ܹ����裬��VDBE���������Լ�����ʽ�������е��ض��ַ����ĸ�����
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
** ���ɴ��룬���� VDBE �ڽ�����е��е����ơ���Щ��Ϣ�������ṩ�ڻص���azCol[]��ֵ��
*/
static void generateColumnNames(
	Parse *pParse,      /* Parser context   ���������� */
	SrcList *pTabList,  /* List of tables   �б�*/
	ExprList *pEList    /* Expressions defining the result set   �������е��﷨��*/
	){
	Vdbe *v = pParse->pVdbe;
	int i, j;
	sqlite3 *db = pParse->db;
	int fullNames, shortNames;

#ifndef SQLITE_OMIT_EXPLAIN
	/* If this is an EXPLAIN, skip this step    �������һ��������, ������һ�� */
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
** ����һ�����ʽ�б�(�������γ�һ��SELECT��������ı��ʽ�б�)����Ϊӵ����Щ���ʽ�б�ı����һ���ʺϵ������ơ�
** All column names will be unique.
** ������������Ψһ��
** Only the column names are computed.  Column.zType, Column.zColl,
** and other fields of Column are zeroed.
** ֻ�������Ǽ�������ġ�zType�С�zColl�к������ж������㡣
** Return SQLITE_OK on success.  If a memory allocation error occurs,
** store NULL in *paCol and 0 in *pnCol and return SQLITE_NOMEM.
** �ɹ�ʱ����SQLITE_OK������ڴ���䷢������,��*paCol���NULL����*pnCol���0������SQLITE_NOMEM
*/
static int selectColumnsFromExprList(
	Parse *pParse,          /* Parsing context ���������� */
	ExprList *pEList,       /* Expr list from which to derive column names ��������Expr�б�*/
	int *pnCol,             /* Write the number of columns here ���е�����д������*/
	Column **paCol          /* Write the new column list here �������б�д������*/
	){
	sqlite3 *db = pParse->db;   /* Database connection �������ݿ�*/
	int i, j;                   /* Loop counters ѭ��������*/
	int cnt;                    /* Index added to make the name unique ���������ʹ����Ψһ*/
	Column *aCol, *pCol;        /* For looping over result columns �ڽ������ѭ��*/
	int nCol;                   /* Number of columns in the result set �ڽ�����е�����*/
	Expr *p;                    /* Expression for a single result column һ������еı��ʽ*/
	char *zName;                /* Column name ����*/
	int nName;                  /* Size of name in zName[]  ��zName[]�����ƵĴ�С*/

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
		** �õ�һ���ʵ����е�����
		*/
		p = pEList->a[i].pExpr;
		assert(p->pRight == 0 || ExprHasProperty(p->pRight, EP_IntValue)
			|| p->pRight->u.zToken == 0 || p->pRight->u.zToken[0] != 0);
		if ((zName = pEList->a[i].zName) != 0){
			/* If the column contains an "AS <name>" phrase, use <name> as the name
			**����а���һ����AS <name>������,ʹ��<name>��Ϊ����
			*/
			zName = sqlite3DbStrDup(db, zName);
		}
		else{
			Expr *pColExpr = p;  /* The expression that is the result column name ��������Ƶı��ʽ*/
			Table *pTab;         /* Table associated with this expression ����ʽ��صı�*/
			while (pColExpr->op == TK_DOT){
				pColExpr = pColExpr->pRight;
				assert(pColExpr != 0);
			}
			if (pColExpr->op == TK_COLUMN && ALWAYS(pColExpr->pTab != 0)){
				/* For columns use the column name name ����ʹ������������*/
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
				/* Use the original text of the column expression as its name ʹ�õ��б��ʽ��ԭʼ�ı���Ϊ��������*/
				zName = sqlite3MPrintf(db, "%s", pEList->a[i].zSpan);
			}
		}
		if (db->mallocFailed){
			sqlite3DbFree(db, zName);
			break;
		}

		/* Make sure the column name is unique.  If the name is not unique,
		** append a integer to the name so that it becomes unique.
		** ȷ��������Ψһ�ġ�������ֲ���Ψһ��,�����Ƹ���һ������,�����ͱ��Ψһ��
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
** �����ͺ����������Ϣ��ӵ�����һ�� SELECT �������б�
**
** The column list presumably came from selectColumnNamesFromExprList().
** The column list has only names, not types or collations.  This
** routine goes through and adds the types and collations.
** �б�������selectColumnNamesFromExprList()���б�ֻ������,û�����ͻ�����������������������ͺ�����
**
** This routine requires that all identifiers in the SELECT
** statement be resolved.
** �������Ҫ����SELECT����е����б�ʶ���õ������
*/
static void selectAddColumnTypeAndCollation(
	Parse *pParse,        /* Parsing contexts ����������*/
	int nCol,             /* Number of columns ����*/
	Column *aCol,         /* List of columns �б�*/
	Select *pSelect       /* SELECT used to determine types and collations      SELECT����ȷ�����ͺ�����*/
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
**����һ��SELECT���,����һ����ṹ,�������Ǹ�SELECT�Ľ����
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
	**sqlite3ResultSetOfSelect()��lookaside���õĵط�ֻʹ��n��������*/
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
** �Ӹ����Ľ��������һ��VDBE���ڱ�Ҫ������´���һ���µ�VDBE��
** ����������󣬷���NULL������pParse��������Ϣ��
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
** ����pLimit��pOffset���ʽ����SELECT�е�iLimit��iOffset�ֶΡ�
** nLimit��nOffset������Щ������ԭʼ��SQL�����LIMIT��OFFSET�ؼ���֮��ı��ʽ��
** �����Щ�ؼ��ʱ�ʡ�ԵĻ�,����ֵΪNULL��
** iLimit��iOffset�������������ƺ�ƫ�����������洢�Ĵ������ݼ�������
** ���û�����ƺ�/��ƫ��,��ôiLimit��iOffset�Ǹ��ġ�
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
** �������ֻ�������ƻ�ƫ����nLimit��nOffset�����ʱ��ı�iLimit�� iOffset��ֵ��
** �����������֮ǰiLimit��iOffsetӦ����Ԥ�赽�ʵ���Ĭ��ֵ(ͨ����������-1)��
** ֻ��nLimit>=0����nOffset>0���ƼĴ������¶��塣
** UNION ALL������ʹ���������ͨ�����SELECT�������ʹ��ͬ�����ƺ�ƫ���ݴ��������á�
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
	** "LIMIT -1" ������ʾ�����С�������ʲô����ȷ����Ϊ����һЩ���顣��ǰʵ�ֽ���"LIMIT 0"��ζ��û���С�
	*/
	sqlite3ExprCacheClear(pParse);
	assert(p->pOffset == 0 || p->pLimit != 0);
	if (p->pLimit){
		p->iLimit = iLimit = ++pParse->nMem;
		v = sqlite3GetVdbe(pParse);
		if (NEVER(v == 0)) return;  /* VDBE should have already been allocated VDBEӦ���Ѿ�����*/
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
			pParse->nMem++;   /* Allocate an extra register for limit+offset ����һ������ļĴ�����limit+offset*/
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
** ΪiCol-th������Ը��ϲ�ѯ���"p"�Ľ���������ʵ�����������.
** �����û��Ĭ�ϵ��������У�����NULL��
**
** The collating sequence for the compound select is taken from the
** left-most term of the select that has a collating sequence.
** ���ϲ�ѯ�������������Ծ����������е�����ߵĲ�ѯ��
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

/* Forward reference ��ǰ����*/
static int multiSelectOrderBy(
	Parse *pParse,        /* Parsing context ����������*/
	Select *p,            /* The right-most of SELECTs to be coded ���ұߵ���Ҫ����Ĳ�ѯ���*/
	SelectDest *pDest     /* What to do with query results ��δ����ѯ���*/
	);


#ifndef SQLITE_OMIT_COMPOUND_SELECT
/*
** This routine is called to process a compound query form from
** two or more separate queries using UNION, UNION ALL, EXCEPT, or
** INTERSECT
** ������򱻵���������ʹ����UNION, UNION ALL, EXCEPT,����INTERSECT�����ĸ������л������������ϵķ�ɢ����
**
** "p" points to the right-most of the two queries.  the query on the
** left is p->pPrior.  The left query could also be a compound query
** in which case this routine will be called recursively.
**��p��ָ�����ұߵ��������С���ߵ�������p->pPrior.�ڵݹ�ؽ�����������������£���ߵĲ�ѯҲ�����Ǹ��ϲ�ѯ��
**
** The results of the total query are to be written into a destination
** of type eDest with parameter iParm.
**�ܲ�ѯ�Ľ������д��һ�����в���iParm��eDest����Ŀ�ꡣ
**
** Example 1:  Consider a three-way compound SQL statement.  ��1:  ����һ�����򸴺�SQL��䡣
**
**     SELECT a FROM t1 UNION SELECT b FROM t2 UNION SELECT c FROM t3
**
** This statement is parsed up as follows:    ��仰�ǽ�������:
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
**����ͼ�еļ�ͷ����Select.pPriorָ�롣���������p����t3��ѯ�������������Ȼ��pPrior������t2 ��ѯ��
**��������£�p- >op������ TK_UNION
**
** Notice that because of the way SQLite parses compound SELECTs, the
** individual selects always group from left to right.
** ע��,��ΪSQLite�������ϲ�ѯ�����߼�����,������ѯ���Ǵ����ҡ�
*/
static int multiSelect(
	Parse *pParse,        /* Parsing context ������������ */
	Select *p,            /* The right-most of SELECTs to be coded ���ұߵ�SELECTs���ᱻ����*/
	SelectDest *pDest     /* What to do with query results ��δ����ѯ���*/
	){
	int rc = SQLITE_OK;   /* Success code from a subroutine ���ӳ��򴫵����ĳɹ����� */
	Select *pPrior;       /* Another SELECT immediately to our left ��һ��SELECTָ�����*/
	Vdbe *v;              /* Generate code to this VDBE Ϊ���VDBE���ɴ���*/
	SelectDest dest;      /* Alternative data destination �ɱ䶯������Ŀ�ĵ�*/
	Select *pDelete = 0;  /* Chain of simple selects to delete ����ɾ���ļ򵥲�ѯ��*/
	sqlite3 *db;          /* Database connection �������ݿ�*/
#ifndef SQLITE_OMIT_EXPLAIN
	int iSub1;            /* EQP id of left-hand query     EQP id���Ĳ�ѯ*/
	int iSub2;            /* EQP id of right-hand query    EQP id�Ҳ�Ĳ�ѯ*/
#endif

	/* Make sure there is no ORDER BY or LIMIT clause on prior SELECTs.  Only
	** the last (right-most) SELECT in the series may have an ORDER BY or LIMIT.
	**ȷ��֮ǰ�Ĳ�ѯ�����û���κ�ORDER BY ����LIMIT��ֻ���������е����һ��(������)��ѯ��������ORDER BY����LIMIT��
	*/
	assert(p && p->pPrior);  /* Calling function guarantees this much ��֤���㹻�ĵ��ú���*/
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
	assert(v != 0);  /* The VDBE already created by calling function    VDBE�Ѿ��ɵ��ú�������*/

	/* Create the destination temporary table if necessary   �ڱ�Ҫʱ����Ŀ����ʱ��
	*/
	if (dest.eDest == SRT_EphemTab){
		assert(p->pEList);
		sqlite3VdbeAddOp2(v, OP_OpenEphemeral, dest.iSDParm, p->pEList->nExpr);
		sqlite3VdbeChangeP5(v, BTREE_UNORDERED);
		dest.eDest = SRT_Table;
	}

	/* Make sure all SELECTs in the statement have the same number of elements
	** in their result sets.
	** ȷ�����������еĲ�ѯ��������ǵĽ�����о�����ͬ������Ԫ�ء�
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
	** ��ORDER BY�Ӿ�ĸ��ϲ�ѯ����������
	*/
	if (p->pOrderBy){
		return multiSelectOrderBy(pParse, p, pDest);
	}

	/* Generate code for the left and right SELECT statements.
	**Ϊ��ߺ��ұߵ�SELECT������ɴ��롣
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
		int unionTab;    /* Cursor number of the temporary table holding result   ��ʱ�������α���*/
		u8 op = 0;       /* One of the SRT_ operations to apply to self        ����һ��SRT_����Ӧ�õ�����*/
		int priorOp;     /* The SRT_ operation to apply to prior selects   ��SRT_����Ӧ�õ�֮ǰ�Ĳ�ѯ*/
		Expr *pLimit, *pOffset; /* Saved values of p->nLimit and p->nOffset    ����p->nLimit��p->nOffset��ֵ*/
		int addr;
		SelectDest uniondest;

		testcase(p->op == TK_EXCEPT);
		testcase(p->op == TK_UNION);
		priorOp = SRT_Union;
		if (dest.eDest == priorOp && ALWAYS(!p->pLimit &&!p->pOffset)){
			/* We can reuse a temporary table generated by a SELECT to our
			** right.
			**���ǿ�������һ�����Ҷ˲�ѯ������ɵ���ʱ��.
			*/
			assert(p->pRightmost != p);  /* Can only happen for leftward elements
										 ** of a 3-way or more compound    ֻ�������Ԫ����3�ֻ����ϸ��ϵ�ʱ�򴥷�*/
			assert(p->pLimit == 0);      /* Not allowed on leftward elements ������������Ԫ��*/
			assert(p->pOffset == 0);     /* Not allowed on leftward elements ������������Ԫ��*/
			unionTab = dest.iSDParm;
		}
		else{
			/* We will need to create our own temporary table to hold the
			** intermediate results.   ������Ҫ�����Լ�����ʱ���������м�����
			*/
			unionTab = pParse->nTab++;
			assert(p->pOrderBy == 0);
			addr = sqlite3VdbeAddOp2(v, OP_OpenEphemeral, unionTab, 0);
			assert(p->addrOpenEphm[0] == -1);
			p->addrOpenEphm[0] = addr;
			p->pRightmost->selFlags |= SF_UsesEphemeral;
			assert(p->pEList);
		}

		/* Code the SELECT statements to our left  ����ߵ�SELECT������
		*/
		assert(!pPrior->pOrderBy);
		sqlite3SelectDestInit(&uniondest, priorOp, unionTab);
		explainSetInteger(iSub1, pParse->iNextSelectId);
		rc = sqlite3Select(pParse, pPrior, &uniondest);
		if (rc){
			goto multi_select_end;
		}

		/* Code the current SELECT statement    ����ǰSELECT������
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
		**��sqlite3Select()�еı�ƽ��������ܻ��������p->pOrderBy.
		** ȷ��Ҫɾ��p - > pOrderBy,�������,���ܱ����ڴ�й©��
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
		**����ʱ���е��κ�����ת��������Ŀǰ���ݡ�
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
		**INTERSECT�������Ĳ�ͬ��Ϊ����Ҫ������ʱ��.��������Լ��İ�����
		**�ӷ���������Ҫ�ı�ʼ
		*/
		tab1 = pParse->nTab++;
		tab2 = pParse->nTab++;
		assert(p->pOrderBy == 0);

		addr = sqlite3VdbeAddOp2(v, OP_OpenEphemeral, tab1, 0);
		assert(p->addrOpenEphm[0] == -1);
		p->addrOpenEphm[0] = addr;
		p->pRightmost->selFlags |= SF_UsesEphemeral;
		assert(p->pEList);

		/* Code the SELECTs to our left into temporary table "tab1".   ��������ߵ�SELECTs ���뵽��ʱ��"tab1"��
		*/
		sqlite3SelectDestInit(&intersectdest, SRT_Union, tab1);
		explainSetInteger(iSub1, pParse->iNextSelectId);
		rc = sqlite3Select(pParse, pPrior, &intersectdest);
		if (rc){
			goto multi_select_end;
		}

		/* Code the current SELECT into temporary table "tab2"    �ѵ�ǰ��SELECT���뵽��ʱ��"tab2"��
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
		** tables.   ��������ʱ��Ľ������ɴ��롣
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
	**��ʱ����ʹ�õļ�������������Ҫʵ�ָ��ϲ�ѯ����KeyInfo�ṹ���ӵ�������ʱ��
	**
	** This section is run by the right-most SELECT statement only.
	** SELECT statements to the left always skip this part.  The right-most
	** SELECT might also skip this part if it has no ORDER BY clause and
	** no temp tables are required.
	**�ⲿ���ǽ���ͨ�����ұߵ�SELECT������еġ�
	** ��ߵ�SELECT�����������������֡�
	**���ұߵ�SELECT���û��ORDER BYҲ����Ҫ��ʱ��Ļ�Ҳ���������������
	*/
	if (p->selFlags & SF_UsesEphemeral){
		int i;                        /* Loop counter ѭ�������� */
		KeyInfo *pKeyInfo;            /* Collating sequence for the result set ��������������� */
		Select *pLoop;                /* For looping through SELECT statements ����SELECT��� */
		CollSeq **apColl;             /* For looping through pKeyInfo->aColl[] ����pKeyInfo - > aColl[]*/
		int nCol;                     /* Number of columns in result set �ڽ����������*/

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
					**���[0]û�б�ʹ�ã���ô[1]Ҳͬ��û�б�ʹ��. ���������ڷ����˵�һ��δʹ�õ�֮���ܹ���ȫ����ֹ��.
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
** ΪЭִͬ�в�ѯ����дһ������ӳ���.
**
** The data to be output is contained in pIn->iSdst.  There are
** pIn->nSdst columns to be output.  pDest is where the output should
** be sent.
** ����������ݰ�����pIn->iSdst��,����pIn->nSdst�еȴ����.��Щ���Ӧ����pDestΪĿ�ĵ�.
**
** regReturn is the number of the register holding the subroutine
** return address.
** regReturn�Ǵ洢���ӳ��򷵻ص�ַ�ļĴ�������.
**
** If regPrev>0 then it is the first register in a vector that
** records the previous output.  mem[regPrev] is a flag that is false
** if there has been no previous output.  If regPrev>0 then code is
** generated to suppress duplicates.  pKeyInfo is used for comparing
** keys.
** ���regPrev>0,��ô���������м�¼��֮ǰ����ĵ�һ���Ĵ���. 
** mem[regPrev]�����֮ǰû�������ʧ�ܱ��. ���regPrev>0��ô���ɵĴ������������ֹ�ظ�.
** pKeyInfo�������Ƚϼ�ֵ.
**
** If the LIMIT found in p->iLimit is reached, jump immediately to
** iBreak.
** ���p->iLimit�е�LIMIT�ﵽ��ֵ,��ô������ת��iBreak.
*/
static int generateOutputSubroutine(
	Parse *pParse,          /* Parsing context ����������*/
	Select *p,              /* The SELECT statement ��ѯ���*/
	SelectDest *pIn,        /* Coroutine supplying data Эͬ�����ṩ����*/
	SelectDest *pDest,      /* Where to send the data ���ݴ����Ŀ�ĵ�*/
	int regReturn,          /* The return address register ���ص�ַ�ļĴ��� */
	int regPrev,            /* Previous result register.  No uniqueness if 0. ֮ǰ����ļĴ���,�����0�Ļ���û��Ψһ�� */
	KeyInfo *pKeyInfo,      /* For comparing with previous entry. ��֮ǰ������Ƚ�*/
	int p4type,             /* The p4 type for pKeyInfo. ΪpKeyInfo����������*/
	int iBreak              /* Jump here if we hit the LIMIT. ���ﵽ��ֵ����ת������*/
	){
	Vdbe *v = pParse->pVdbe;
	int iContinue;
	int addr;

	addr = sqlite3VdbeCurrentAddr(v);
	iContinue = sqlite3VdbeMakeLabel(v);

	/* Suppress duplicates for UNION, EXCEPT, and INTERSECT
	** UNION, EXCEPT, and INTERSECT�Ľ�ֹ�ظ��ֶ�
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
	** ����OFFSETԪ�ص�ʱ���ֹ��һ��OFFSET����
	*/
	codeOffset(v, p, iContinue);

	switch (pDest->eDest){
		/* Store the result as data using a unique key.
		** ʹ��һ������ļ�ֵ�������ݵķ�ʽ�洢���
		*/
	case SRT_Table:
	case SRT_EphemTab: {
		int r1 = sqlite3GetTempReg(pParse);/*����һ���Ĵ������洢�м������*/*/
		int r2 = sqlite3GetTempReg(pParse);
		testcase(pDest->eDest == SRT_Table);/*���Դ���Ľ�����ı�����*/
		testcase(pDest->eDest == SRT_EphemTab);/*���Դ���Ľ�����ı�Ĵ�С*/
		sqlite3VdbeAddOp3(v, OP_MakeRecord, pIn->iSdst, pIn->nSdst, r1);/*��OP_MakeRecord������¼����������VDBE���ٷ���һ����ָ���ַ*/
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
		** �������Ϊ"expr IN (SELECT ...)"����һ���ṹ����,��ôջ����Ӧ��ֻ��һ��Ԫ��.
		** �����Ԫ��αװ��д�����.
		*/
	case SRT_Set: {
		int r1;
		assert(pIn->nSdst == 1);
		p->affinity =
			sqlite3CompareAffinity(p->pEList->a[0].pExpr, pDest->affSdst);
		r1 = sqlite3GetTempReg(pParse);
		sqlite3VdbeAddOp4(v, OP_MakeRecord, pIn->iSdst, 1, r1, &p->affinity, 1);
		sqlite3ExprCacheAffinityChange(pParse, pIn->iSdst, 1);
		sqlite3VdbeAddOp2(v, OP_IdxInsert, pDest->iSDParm, r1);/*��OP_IdxInsert��������VDBE���ٷ���һ����ָ���ַ*/
		sqlite3ReleaseTempReg(pParse, r1);/*�ͷ�����Ĵ���*/
		break;
	}

#if 0  /* Never occurs on an ORDER BY query */
		/* If any row exist in the result set, record that fact and abort.
		** ������κ�δ��������ݴ����ڽ������,��¼��Ȼ����ֹ
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
		** �������һ������ѡ����ʽ��һ����,��ô�ѽ���洢��һ�����ʵĴ洢��Ԫ��,Ȼ����������ѭ��
		*/
	case SRT_Mem: {
		assert(pIn->nSdst == 1);
		sqlite3ExprCodeMove(pParse, pIn->iSdst, pDest->iSDParm, 1);
		/* The LIMIT clause will jump out of the loop for us. LIMITԪ�ػ�����ѭ��*/
		break;
	}
#endif /* #ifndef SQLITE_OMIT_SUBQUERY */

		/* The results are stored in a sequence of registers
		** starting at pDest->iSdst.  Then the co-routine yields.
		** ����洢����pDest->iSdst��ʼ�ļĴ���������.����������Эͬ����ķ�����.
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

//������

#if !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW)//�궨��
/* Forward Declarations *//*Ԥ������*/
//������̬����
static void substExprList(sqlite3*, ExprList*, int, ExprList*);
static void substSelect(sqlite3*, Select *, int, ExprList *);

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
** ͨ�����ʽpExpr����ɨ�裬�ӱ��ʽ�б��и��Ƶ�iColumn-th����Ŀ���滻iTable��ÿ�����յ��С�
** ��ROWID�����κθı䣩
** ���������ƴ�Ϲ��̵�һ���֡��Ӳ�ѯ�Ľ���������ʽ���壬����Ϊһ����SELECT��FROM�Ӿ����Ŀ��
** ���VDBE�����FROM�Ӿ���α���һ��������������Ա��ʽ�����ı䣬����ֱ�������Ӳ�ѯ��Դ�������
** �Ӳ�ѯ�Ľ������
*/

static Expr *substExpr(
	sqlite3 *db,        /* Report malloc errors to this connection *//*�������ݿ�db,����malloc��������*/
	Expr *pExpr,        /* Expr in which substitution occurs *//*����pExpr���������滻����*/
	int iTable,         /* Table to be substituted *//*�滻�ı��*/
	ExprList *pEList    /* Substitute expressions *//*�滻�ı��ʽ�б�*/
	)
{
	///*�ж������������ʽΪ�գ�����*/
	if (pExpr == 0) return 0;

	if (pExpr->op == TK_COLUMN && pExpr->iTable == iTable){

		if (pExpr->iColumn<0){//���ʽ�������
			pExpr->op = TK_NULL;//��ֵΪ��
		}
		else{
			Expr *pNew;//����һ���µ�ָ����ʽ��ָ��
			assert(pEList != 0 && pExpr->iColumn<pEList->nExpr);//�쳣�������ʽ��Ϊ�գ���������С�ڱ��ʽ�б��б��ʽ�ĸ������������������׳�
			assert(pExpr->pLeft == 0 && pExpr->pRight == 0);//�쳣�������ʽ����������Ϊ�գ������������׳��ϵ�
			pNew = sqlite3ExprDup(db, pEList->a[pExpr->iColumn].pExpr, 0);//���
			if (pNew && pExpr->pColl){//��pNew��Ϊ���ұ��ʽ���������в�Ϊ�ճ�����ִ��
				pNew->pColl = pExpr->pColl;//ȫ�ֱ�����ֵ
			}
			sqlite3ExprDelete(db, pExpr);//�������ݿ��б��ʽɾ������
			pExpr = pNew;//ȫ�ֱ�����ֵ
		}
	}
	else{
		pExpr->pLeft = substExpr(db, pExpr->pLeft, iTable, pEList);//�ݹ����substExpr����
		pExpr->pRight = substExpr(db, pExpr->pRight, iTable, pEList);//�ݹ����substExpr����
		if (ExprHasProperty(pExpr, EP_xIsSelect)){//���ú���ExprHasProperty�жϱ��ʽpExpr�Ƿ����EP_xIsSelect��ѯ
			substSelect(db, pExpr->x.pSelect, iTable, pEList);//����substSelect��������pExpr->x.pSelect��Select�����д���
		}
		else{
			substExprList(db, pExpr->x.pList, iTable, pEList);//����substExprList��������pExpr->x.pList���ʽ�б���
		}
	}
	return pExpr;//������Ӧ�ı��ʽָ��
}
static void substExprList(
	sqlite3 *db,         /* Report malloc errors here *//*����sqlite�Ķ��󣬱�������ڴ����*/
	ExprList *pList,     /* List to scan and in which to make substitutes *//*����pExpr���������滻����ɨ���б�*/
	int iTable,          /* Table to be substituted *//*����Ҫ�滻�ı��*/
	ExprList *pEList     /* Substitute values *//*�滻�ı��ʽ�б�*/
	){
	int i;//����������i
	if (pList == 0) return;//���ʽ�б�Ϊ�գ�����
	//�������ʽ�б�
	for (i = 0; i<pList->nExpr; i++){
		pList->a[i].pExpr = substExpr(db, pList->a[i].pExpr, iTable, pEList);//�ݹ���ã����¸�ֵpList->a[i].pExpr
	}
}

static void substSelect(
	sqlite3 *db,         /* Report malloc errors here *//*����sqlite�Ķ��󣬱�������ڴ����*/
	Select *p,           /* SELECT statement in which to make substitutions *//*����һ��Select����*/
	int iTable,          /* Table to be replaced *//*����Ҫ�滻�ı��*/
	ExprList *pEList     /* Substitute values *//*�滻�ı��ʽ�б�*/
	){
	SrcList *pSrc;//��������SELECT�е�FROM�Ӿ�Ķ���
	struct SrcList_item *pItem;//����SrcList_item�ṹ���һ��FROM�Ӿ����
	int i;//����������i
	if (!p) return;//��p���գ�����

	substExprList(db, p->pEList, iTable, pEList);	//����substExprList�������� p->pEList���ʽ�б���
	substExprList(db, p->pGroupBy, iTable, pEList);	//����substExprList�������� p->pGroupBy���ʽ�б���
	substExprList(db, p->pOrderBy, iTable, pEList);	//����substExprList�������� p->pOrderBy���ʽ�б���
	p->pHaving = substExpr(db, p->pHaving, iTable, pEList);//����substExpr��������ֵ��SELECT��pHaving����
	p->pWhere = substExpr(db, p->pWhere, iTable, pEList);//����substExpr��������ֵ��SELECT��pWhere����
	substSelect(db, p->pPrior, iTable, pEList);//�ݹ����substSelect����
	pSrc = p->pSrc;//��ȡ��ǰfrom�Ӿ��е�p->pSrc
	assert(pSrc);  /* Even for (SELECT 1) we have: pSrc!=0 but pSrc->nSrc==0 *///��������������ϵ㣬�����쳣����
	if (ALWAYS(pSrc)){
		//����from�Ӿ���еĶ���
		for (i = pSrc->nSrc, pItem = pSrc->a; i>0; i--, pItem++){
			substSelect(db, pItem->pSelect, iTable, pEList);//����substSelect�������� pItem->x.pSelect��Select�����д���
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
/* �����̳����Ա�ƽ���Ӳ�ѯ��Ϊһ�������Ż�������������˸ı䣬���򷵻�1����û�б�ƽ������������򷵻�0.
**
** Ҫ����ƽ���ĸ��Ҫ�������µĲ�ѯ��
**   SELECT a FROM (SELECT x+y AS a FROM t1 WHERE z<100) WHERE a>5
** Ĭ����ִ���Ӳ�ѯ��䣬Ȼ�󽫽���洢����ʱ���У��ڸ���ʱ���������ⲿ��ѯ����Ҫ��������������
** ���⣬��Ϊ��ʱ��û�����������ⲿ��ѯ��where�־䲻�ܽ����Ż���
** �����������д��ѯ�����������SQL��䣬��Ϊ��������һ�������ı�ƽ����ѯ��
**   SELECT a FROM (SELECT x+y AS a FROM t1 WHERE z<100) WHERE a>5
** �������ͬ���Ľ����ֻ��Ҫɨ��һ�����ݡ���Ϊ���������ڱ�t1�ϴ��ڣ����ܱ���һ����ɵ�����ɨ�衣
**
** �����������¹��򣬾���ʵ�ֱ�ƽ����ѯ��
**��1���Ӳ�ѯ�����ѯ����ͬʱʹ�þۺϺ���
**��2���Ӳ�ѯ����һ���ۺϻ��ⲿ��ѯ����һ������
**��3���Ӳ�ѯ�����������ӵ��Ҳ�����
**��4���Ӳ�ѯ��ʹ�á�DISTINCT��
**��5������4���ͣ�5��������DISTINCT�Ӳ�ѯ���Ӽ����ų��������Ż����Ӳ�ѯ������4���Ѿ��ų������е�DISTINCT�Ӳ�ѯ
**��6���Ӳ�ѯ����ʹ�þۼ����ⲿ��ѯ����ʹ��DISTINCT
**��7���Ӳ�ѯ��һ��FROM�Ӿ䡣TODO���Ӳ�ѯû��һ��FROM�Ӿ䣬�������һ������ı�sqlite_once����ֻ������һ����һ�Ŀ�ֵ���С�
**��8���Ӳ�ѯ��ʹ�����ƻ��ⲿ��ѯ����һ������
**��9���Ӳ�ѯ��ʹ�����ƻ��ⲿ��ѯ��ʹ�þۼ�
**��10���Ӳ�ѯ��ʹ�þۺϻ��ⲿ��ѯ��ʹ������
**��11���Ӳ�ѯ���ⲿ��ѯ����ͬʱʹ��ORDER BY�Ӿ�
**��12��û��ʵ�֣����뵽����������3����
**��13���Ӳ�ѯ���ⲿ��ѯ����ͬʱʹ��LIMIT�Ӿ�
**��14���Ӳ�ѯ����ʹ��OFFSET�Ӿ�
**��15���ⲿ��ѯ������һ�����ϲ�ѯ��һ���ֻ����Ӳ�ѯ������LIMIT�Ӿ�
**��16���ⲿ��ѯ������һ���ۼ���ѯ���Ӳ�ѯ���ܰ���orderby�Ӿ䡣������group_concat������������ʵ��ƥ�䡣
**��17���Ӳ�ѯ����һ�����ϲ�ѯ������UNION ALL���ϲ�ѯ�зǾۼ���ѯ��ɵĲ�ѯ��䣬���������µĸ���ѯ��
**      *�����Ǹ��ϲ�ѯ
**      *����һ���ۼ���ѯ��DISTINCT��ѯ
**      *û�����Ӳ���
** ����ѯ���Ӳ�ѯ���ܰ���һ��WHERE�Ӿ䡣��Щ���ӹ���11������13���ͣ�14����������Ҳ���� ORDER BY, LIMIT ��OFFSET �Ӿ�.
** �Ӳ�ѯ����ʹ���κη��ϲ�������UNION ALL,��Ϊ���еĸ��ϲ����Ƕ���ʵ����DISTINCT������4��������ġ�
** ���⣬ÿһ���Ӳ�ѯ��������뷵�������Ľ���С����ʵ����Ҫ�󸴺ϲ�ѯ���Ӳ�ѯû�б�ƽ���ġ����ý���⵽�﷨����ͷ�����ϸ��Ϣ
**
**��18������Ӳ�ѯ�Ǹ��ϲ�ѯ������ѯ��ORDERBY�Ӿ����й����������������Ӳ�ѯ���С�
**��19���Ӳ�ѯ����ʹ��LIMIT�����ѯ����ʹ��WHERE�Ӿ�
**��20������Ӳ�ѯ�Ǹ��ϲ�ѯ������ʹ��ORDER BY �Ӿ䡣�ͷ���ЩԼ��������ORDER BY �Ӿ������ʾΪ�ⲿ��ѯ�е�δ�޸ĵĽ���С�
�ڴ�����δ�������������������Ż���
**��21���Ӳ�ѯ����ʹ��LIMIT�������ѯ����ʹ��DISTINCT��
**
**����������У�����p��һ��ָ�룬ָ�����ѯ���Ӳ�ѯp->pSrc->a[iFrom].������ѯʹ���˾ۼ�isAggΪTRUE�����
**�Ӳ�ѯʹ���˾ۼ���subqueryIsAggΪTRUE��
**
**������ܽ��б�ƽ������ô�������ִ�У�������0��
**���ʹ�ñ�ƽ���ˣ�����1.
**
*���ʽ�������뷢���ڳ�������֮ǰ���ⲿ��ѯ���Ӳ�ѯ�С�
*/
static int flattenSubquery(
	Parse *pParse,       /* Parsing context *//*�������������ĵı�������������*/
	Select *p,           /* The parent or outer SELECT statement *//*����һ������ѯ���ⲿ��ѯ�ı���*/
	int iFrom,           /* Index in p->pSrc->a[] of the inner subquery *//*�ڲ���ѯ��p->pSrc->a[]�е�����*/
	int isAgg,           /* True if outer SELECT uses aggregate functions *//*�ⲿ��ѯ������˾ۼ���������ΪTRUE*/
	int subqueryIsAgg    /* True if the subquery uses aggregate functions *//*�Ӳ�ѯ���ʹ���˾ۼ���������ΪTRUE*/
	){
	const char *zSavedAuthContext = pParse->zAuthContext;/*�������������﷨�������е������Ľ��и�ֵ*/
	Select *pParent;
	Select *pSub;       /* The inner query or "subquery" *//*�������� pSub���������ڲ�ѯ���Ӳ�ѯ����*/
	Select *pSub1;      /* Pointer to the rightmost select in sub-query *//*����pSub1������������ʾָ���Ӳ�ѯ�����ұߵĲ�ѯ*/
	SrcList *pSrc;      /* The FROM clause of the outer query *//*����Srclist�Ķ��󣬱����ⲿ��ѯ��FROM�Ӿ�*/
	SrcList *pSubSrc;   /* The FROM clause of the subquery *//*����Srclist�Ķ���������ʾ�Ӳ�ѯ��FROM�Ӿ�*/
	ExprList *pList;    /* The result set of the outer query *//*�������ʽExprlist�ı�������ʾ�ⲿ��ѯ�Ľ����*/
	int iParent;        /* VDBE cursor number of the pSub result set temp table *//*�����������͵ı����������ϱ�ʾVDBE�α�ţ�ָ���ڲ�ѯ����ʱ��*/
	int i;              /* Loop counter *//*ѭ������*/
	Expr *pWhere;                    /* The WHERE clause *//*������ʾWHERE�Ӿ�*/
	struct SrcList_item *pSubitem;   /* The subquery *//*����һ��SrcList_item���͵��Ӳ�ѯ�ṹ��*/
	sqlite3 *db = pParse->db;//����һ��sqlite3�Ķ��󣬱�ʾ���ݿ�����

	/* Check to see if flattening is permitted.  Return 0 if not.
	*//*����Ƿ������ƽ�����������򷵻�0*/
	assert(p != 0);//SELECT�ṹ��Ϊ�գ��׳��쳣�������������˶ϵ㣬���ڴ����쳣
	assert(p->pPrior == 0);  // Unable to flatten compound queries *//*���ܱ�ƽ���ĸ��ϲ�ѯ������ϵ㣬�쳣����
	if (db->flags & SQLITE_QueryFlattener) return 0;//�������������ֵΪSQLITE_QueryFlattener�ұ�Ǳ���db->flags������������ִ����һ�� 
	pSrc = p->pSrc;/*��ȡ��ѯ�ṹ����from�Ӿ�*/
	assert(pSrc && iFrom >= 0 && iFrom<pSrc->nSrc);//�쳣��������ϵ㲻�������������׳��쳣
	pSubitem = &pSrc->a[iFrom];//FROM�Ӿ�������������ַ��ֵ���Ӳ�ѯpSubitem
	iParent = pSubitem->iCursor;//��ȡ�Ӳ�ѯ���α��
	pSub = pSubitem->pSelect;//��ȡ��ǰ���Ӳ�ѯ���ڲ�ѯ
	assert(pSub != 0);//�쳣��������ϵ㣬����Ӳ�ѯ�ṹ��Ϊ�գ��׳�������Ϣ
	if (isAgg && subqueryIsAgg) return 0;                 /* Restriction (1)  *//*����1���Ӳ�ѯ���ⲿ��ѯʹ���˾ۼ�����������ֱ�ӷ���*/
	if (subqueryIsAgg && pSrc->nSrc>1) return 0;          /* Restriction (2)  *//*����2��*///ʹ�þۼ���������from���Ӿ����>1
	pSubSrc = pSub->pSrc;//��ȡ�Ӳ�ѯ��from���
	assert(pSubSrc);/*�쳣��������ϵ㣬���pSubSrcΪ�գ��׳�������Ϣ*/
	/* Prior to version 3.1.2, when LIMIT and OFFSET had to be simple constants,
	** not arbitrary expresssions, we allowed some combining of LIMIT and OFFSET
	** because they could be computed at compile-time.  But when LIMIT and OFFSET
	** became arbitrary expressions, we were forced to add restrictions (13)
	** and (14). */
	/*֮ǰ�汾 3.1.2�����ƺ�ƫ���������Ǽ򵥵ĳ�������������ı��ʽ��������������LIMIT �� OFFSET
	**��Ϊ���ǿ����ڱ���ʱ���㡣���ǵ�LIMIT �� OFFSETΪ����ı��ʽ�����Ǳ���ʹ�ù���13����
	**����14����
	*/
	if (pSub->pLimit && p->pLimit) return 0;              /* Restriction (13) *//*����13��*///�Ӳ�ѯ���ⲿ��ѯ����ʹ��limit
	if (pSub->pOffset) return 0;                          /* Restriction (14) *//*����14��*///�Ӳ�ѯ����ʹ��offset���
	if (p->pRightmost && pSub->pLimit){/**/
		return 0;                                            /* Restriction (15) *//*����15��*///�Ӳ�ѯ������limit�Ӿ�
	}
	if (pSubSrc->nSrc == 0) return 0;                       /* Restriction (7)  *//*����7��*///�ж��Ӳ�ѯ�Ƿ���from�Ӿ�
	if (pSub->selFlags & SF_Distinct) return 0;           /* Restriction (5)  *//*����5��*///����distinct�Ӿ䣬ֱ�ӷ���
	if (pSub->pLimit && (pSrc->nSrc>1 || isAgg)){
		return 0;         /* Restrictions (8)(9) *//*����8����9��*///�Ӳ�ѯ��ʹ�þۼ������Ӳ�������limit�Ӿ�
	}
	if ((p->selFlags & SF_Distinct) != 0 && subqueryIsAgg){
		return 0;         /* Restriction (6)  *//*����6��*///�Ӳ�ѯ��ʹ�þۼ��������ⲿ��ѯ��ʹ��distinct�Ӿ�
	}
	if (p->pOrderBy && pSub->pOrderBy){
		return 0;                                           /* Restriction (11) *//*����11��*///�Ӳ�ѯ���ⲿ��ѯ����ͬʱʹ��orderby�Ӿ�
	}
	if (isAgg && pSub->pOrderBy) return 0;                /* Restriction (16) *//*����16��*///�ⲿ��ѯ����ʹ�ۼ���ѯ���Ӳ�ѯ���ܺ���orderby�Ӿ�
	if (pSub->pLimit && p->pWhere) return 0;              /* Restriction (19) *//*����19��*///�Ӳ�ѯ����limit�Ӿ䣬�ⲿ��ѯ������where�Ӿ�
	if (pSub->pLimit && (p->selFlags & SF_Distinct) != 0){
		return 0;         /* Restriction (21) *//*����21��*///�Ӳ�ѯ���ܺ���limit���ⲿ��ѯ������distinct���
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
	  ** ����3������Ӳ�ѯ��һ�����ӣ���ȷ���Ӳ�ѯ�ǲ������ⲿ���ӵ��Ҳ�������
	  **     t1 LEFT OUTER JOIN (t2 JOIN t3)
	  ** ���Ա��ʽ���б�ƽ�������ǵõ�
	  **      (t1 LEFT OUTER JOIN t2) JOIN t3
	  ** ���������ʽ��ʾ�ǲ���ͬ�ˡ�
	  ** ��ʱ��ע�� 2��
	  ** ����12��������Ӳ�ѯ�����ⲿ���Ҳ���������ȷ���Ӳ�ѯ��û�� WHERE �Ӿ䡣
	  ** ��������Ӳ���ʹ�ã�
	  **  1 LEFT OUTER JOIN (SELECT * FROM t2 WHERE t2.x>0)
	  **�������������ƽ����
	  **  (t1 LEFT OUTER JOIN t2) WHERE t2.x>0
	  ** ���Խ�ʼ��ʧ���� t2����Ϊ��һ��NULL����t2���У���Ч�Ľ�OUTER JOINת��ΪINNER JOIN.
	  **
	  ** ��д��ʱ��ע��1��2����ƽ�������ӵ��������ǲ���ȫ�ġ���
	  ** ���Ӳ�ѯ�������ӵ����������������ƽ����
	  */
	  if( (pSubitem->jointype & JT_OUTER)!=0 ){/*if�ж������������ѯʹ��jointype*/
		return  0;/*ֱ�ӷ���*/
	  }

	  /* Restriction 17: If the sub-query is a compound SELECT, then it must
	  ** use only the UNION ALL operator. And none of the simple select queries
	  ** that make up the compound SELECT are allowed to be aggregate or distinct
	  ** queries.����17��


	  *//*����Ӳ�ѯ��һ�����ϵ�ѡ����ô������ʹ��ֻ�� UNION ALL �������û��һ���򵥵�ѡ���ѯ�����������ϲ�ѯ���Ӳ�ѯ�У�û��ʹ�þۼ�������ȥ���ظ�*/
	  if( pSub->pPrior ){//�ж��Ӳ�ѯ�Ƿ������Ȳ�ѯ
		if( pSub->pOrderBy ){/*�Ӳ�ѯ����OrderBy�Ӿ�*/
		  return 0;  /* ���� 20 ֱ�ӷ���*/
		}
		if( isAgg || (p->selFlags & SF_Distinct)!=0 || pSrc->nSrc!=1 ){/*����ⲿ��ѯʹ���˾ۼ�������û���ظ��������FROM������1*/
		  return 0;/*ֱ�ӷ���*/
		}
		for(pSub1=pSub; pSub1; pSub1=pSub1->pPrior){/*�����Ӳ�ѯ�����ұߵĲ�ѯ*/
		  testcase( (pSub1->selFlags & (SF_Distinct|SF_Aggregate))==SF_Distinct );/*����Distinct��ʹ��*/
		  testcase( (pSub1->selFlags & (SF_Distinct|SF_Aggregate))==SF_Aggregate );/*����Aggregate��ʹ��*/
		  assert( pSub->pSrc!=0 );/*�쳣����*/
		  if( (pSub1->selFlags & (SF_Distinct|SF_Aggregate))!=0//���Ӳ�ѯ����Distinct��Aggregate�ұ�Ǳ���=1
		   || (pSub1->pPrior && pSub1->op!=TK_ALL) //�Ӳ�ѯ�к����Ȳ�ѯSELECT���Ҳ���ΪTK_ALL
		   || pSub1->pSrc->nSrc<1//�Ӳ�ѯ��FROM�Ӿ��б��ʽ�Ҹ���<1*/
		   || pSub->pEList->nExpr!=pSub1->pEList->nExpr//�Ӳ�ѯ�б��ʽ�ĸ�����=�Ӳ�ѯ���ұ߲�ѯ�ı��ʽ����
		  ){
			return 0;
		  }
		  testcase( pSub1->pSrc->nSrc>1 );//����testcase���Ӳ�ѯ��FROM�Ӿ��б��ʽ����>1�Ĳ���
		}

		/* Restriction 18. *//*����18��*/
		if( p->pOrderBy ){//��ORDERBY�Ӿ�Ϊ��
		  int ii;//��������ii
		  for(ii=0; ii<p->pOrderBy->nExpr; ii++){//����
			if( p->pOrderBy->a[ii].iOrderByCol==0 ) return 0;//ֵΪ�գ����ء�
		  }
		}
	  }

	  /***** If we reach this point, flattening is permitted. *****/
      /*����ִ�е���һ���������ƽ��*/
	  /* Authorize the subquery *//*��Ȩ�����Ӳ�ѯ*/
	  pParse->zAuthContext = pSubitem->zName;//�Ӳ�ѯ�����ָ�ֵ���﷨���������Ѿ���Ȩ������������
	  TESTONLY(i =) sqlite3AuthCheck(pParse, SQLITE_SELECT, 0, 0, 0);/**/
	  testcase( i==SQLITE_DENY );//����testcase����i�Ƿ�ΪSQLITE_DENY
	  pParse->zAuthContext = zSavedAuthContext;//�﷨�������е������ĸ�ֵ���﷨����������Ȩ����������

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
	  /*����Ӳ�ѯ��һ������ SELECT ��䣬Ȼ����ݹ���17�͹���18������UNION ALL ���Ҹ���ѯ�ĸ�ʽ���£�
	  **    SELECT <expr-list> FROM (<sub-query>) <where-clause> 
	  **������κ� ORDER BY, LIMIT �� OFFSET �Ӿ䡣��һ�鴴����n-1�������Ĳ���ORDER BY, LIMIT �� OFFSET����ѯ���� 
	  **�������ӵ�ԭʼʹ�õ�UNION ALL��������N�Ǹ��ϲ�ѯ�м򵥲�ѯ�ı�š�
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
	  //ѭ�������Ӳ�ѯ
	  for(pSub=pSub->pPrior; pSub; pSub=pSub->pPrior){//�����Ӳ�ѯ�е����Ȳ�ѯ
		Select *pNew;//����һ��SELECT�ṹ��Ķ���
		ExprList *pOrderBy = p->pOrderBy;//S���ʽ�б�pOrderBy�ĸ�ֵ
		Expr *pLimit = p->pLimit;//�Ա��ʽ��pLimit���Եĸ�ֵ��select�ṹ�壩
		Select *pPrior = p->pPrior;//���Ȳ������¸�ֵ���µı���
	
		p->pOrderBy = 0;
		p->pSrc = 0;
		p->pPrior = 0;
		p->pLimit = 0;
		pNew = sqlite3SelectDup(db, p, 0);//���
		//������ֵ��select�ṹ���Ա
		p->pLimit = pLimit;
		p->pOrderBy = pOrderBy;
		p->pSrc = pSrc;
		p->op = TK_ALL;//��������������Ϊtk_all
		p->pRightmost = 0;//���ұ߲�ѯ�ĳ�ʼ������

		if( pNew==0 ){
		  pNew = pPrior;//SELECT�ṹ�������Ȳ�ѯ�ĸ�ֵ��pNew
		}
		
		else
		{
		  pNew->pPrior = pPrior;//select->pPrior��ֵ��pNew->pPrior
		  pNew->pRightmost = 0;//pNewָ������ұ���Ϊ0
		}
		p->pPrior = pNew;//������ո�ֵ�����Ȳ���
		if( db->mallocFailed ) return 1;//�ڴ����ʧ��
	  }

	  /* Begin flattening the iFrom-th entry of the FROM clause 
	  ** in the outer query.
	  *//*�����ѯ��ѹ��FROM�Ӿ�ĵ�iFrom����Ŀ*/
	  pSub = pSub1 = pSubitem->pSelect;//��ֵ��䣬�Ӳ�ѯ
	  /* Delete the transient table structure associated with the
	  ** subquery
	  *//*ɾ�����Ӳ�ѯ���������ʱ��ṹ*/
	  //�ڴ���ͷ�
	  sqlite3DbFree(db, pSubitem->zDatabase);//���ݿ�ģ����ڴ�
	  sqlite3DbFree(db, pSubitem->zName);//�Ӳ�ѯ��������
	  sqlite3DbFree(db, pSubitem->zAlias);//�Ӳ�ѯ������ϵ
	  //���¸�ֵ
	  pSubitem->zDatabase = 0;//���
	  pSubitem->zName = 0;
	  pSubitem->zAlias = 0;
	  pSubitem->pSelect = 0;

	  /* Defer deleting the Table object associated with the
	  ** subquery until code generation is
	  ** complete, since there may still exist Expr.pTab entries that
	  ** refer to the subquery even after flattening.  Ticket #3346.
	  **
	  ** pSubitem->pTab is always non-NULL by test restrictions and tests above.
	  *//*�ӳ�ɾ��������ı����ֱ���Ӳ�ѯ���ɴ�����ɣ���Ϊ���������Ȼ���� Expr.pTab ��ƽ������Ӳ�ѯ*/

	  if( ALWAYS(pSubitem->pTab!=0) ){//�Ӳ�ѯ���pTab����
		Table *pTabToDel = pSubitem->pTab;
		if( pTabToDel->nRef==1 ){
		  Parse *pToplevel = sqlite3ParseToplevel(pParse);//������
		  pTabToDel->pNextZombie = pToplevel->pZombieTab;//���������ֵ
		  pToplevel->pZombieTab = pTabToDel;//��������ṹ��ֵ
		}else{
		  pTabToDel->nRef--;//�Լ�
		}
		pSubitem->pTab = 0;//��0
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
	  /* �����ѭ��һ���ԵĽ����ϲ�ѯ��ÿ��������б�ƽ��������������������һ�ַ�ʽ�ı�ƽ������--
	  ** ���ֱ�ƽ���������Ǹ����Ӳ�ѯ�ı�ƽ������ѭ����������һ�Ρ�
	  **
	  ** ���ѭ�����Ӳ�ѯ�е�FROM�Ӿ������Ԫ�ض��ƶ����ⲿ��ѯ��FROM�Ӿ��С�����֮ǰ����ס����ѯ��ԭʼ�ⲿ��ѯ
	  ** FROM�Ӿ���α�š�����ѯ���α��û���ù�������Ĵ��� ��ɨ����� iParent ���õı��ʽ���滻����Щ���ý��������������ڸ��Ƶ��Ӳ�ѯ�ı��ʽԪ�ء�
	  */
	  //����
	  for(pParent=p; pParent; pParent=pParent->pPrior, pSub=pSub->pPrior){
		int nSubSrc;//�������ͱ���nsubsrc
		u8 jointype = 0; //�Զ������ͱ���������
		pSubSrc = pSub->pSrc;     /* FROM clause of subquery *//*from�Ӳ�ѯ*/
		nSubSrc = pSubSrc->nSrc;  /* Number of terms in subquery FROM clause *//*from�Ӿ����Ŀ*/
		pSrc = pParent->pSrc;     /* FROM clause of the outer query *//*�ⲿ��ѯ��FROM�Ӿ�*/

		if( pSrc ){//�ⲿ��ѯ��from�Ӿ�
		  assert( pParent==p );  // First time through the loop *//*��һ��ѭ��������ϵ㣬�����쳣��������ش���*/
		  jointype = pSubitem->jointype;//�Ӳ�ѯ����������
		}else{
		  assert( pParent!=p );  // 2nd and subsequent times through the loop *//*������������ͬ������ϵ�*/
		  pSrc = pParent->pSrc = sqlite3SrcListAppend(db, 0, 0, 0);//׷��from�Ӿ�
		  if( pSrc==0 ){//���û��׷��
			assert( db->mallocFailed );//�쳣��������ڴ�����
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
		** �Ӳ�ѯʹ�� FROM �Ӿ���ⲿ��ѯ�� ����Ӳ�ѯ���� FROM �Ӿ����ж��Ԫ�أ�
		չ�����ⲿ��ѯ����ʹ�����е�����Ԫ�صĿռ� ��**
		** ���ӣ�
		**      SELECT * FROM tabA, (SELECT * FROM sub1, sub2), tabB;
		** �ⲿ��3�������from�Ӿ��С��м��������Ӳ�ѯ����һ������齫���ѯ����Ϊ4����ڡ�
		** Ϊȷ���Ӳ�ѯ�Ŀռ䣬�м������չλ2����
		*/
		if( nSubSrc>1 ){//�Ӳ�ѯfrom�Ӿ䳬��һ��
		  pParent->pSrc = pSrc = sqlite3SrcListEnlarge(db, pSrc, nSubSrc-1,iFrom+1);//�����ⲿ��ѯ���
		  if( db->mallocFailed ){//�ڴ����ʧ��
			break;
		  }
		}

		/* Transfer the FROM clause terms from the subquery into the
		** outer query.
		*//*�Ӳ�ѯ�е�FROM�Ӿ�ת�����ѯ*/
		//�������е�from�Ӿ�
		for(i=0; i<nSubSrc; i++){
		  sqlite3IdListDelete(db, pSrc->a[i+iFrom].pUsing);//������ɾ���Ѿ��������from�Ӿ�
		  pSrc->a[i+iFrom] = pSubSrc->a[i];//ɾ������뵱ǰ��from�Ӿ�
		  memset(&pSubSrc->a[i], 0, sizeof(pSubSrc->a[i]));//��pSubSrc->a[i]��ָ�ĵ�ַ��ʼ����pSubSrc->a[i]��ǰsizeof(pSubSrc->a[i])���ֽ���0�滻
		}
		pSrc->a[iFrom].jointype = jointype;//��ǰfrom�Ӿ��������͵ĸ�ֵ
	  
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
		** ���磺
		** SELECT a+5, b*10 FROM (SELECT x*3 AS a, y+10 AS b FROM t1) WHERE a>b;
		**   \                     \_____________ subquery __________/          /
		**    \_____________________ outer query ______________________________/
		**���ǿ���ÿ���ⲿ��ѯ�еı��ʽ�����ǿ���ÿ���ط�  'a' ���� 'x * 3'��ÿ���ط������ǿ��� 'b' ���Ǵ���' y+10'
		*/
		pList = pParent->pEList;//�ⲿ��ѯ���ʽ�б�Ļ�ȡ
		//�������еı��ʽ�б�
		for(i=0; i<pList->nExpr; i++){
		  if( pList->a[i].zName==0 ){//�����ʽ������
			const char *zSpan = pList->a[i].zSpan;//��ֵ
			if( ALWAYS(zSpan) ){
			  pList->a[i].zName = sqlite3DbStrDup(db, zSpan);//���
			}
		  }
		}
		substExprList(db, pParent->pEList, iParent, pSub->pEList);//��ȡpParent->pEList��pSub->pEList
		if( isAgg ){//����ⲿ��ѯʹ���˾ۼ�����
		  substExprList(db, pParent->pGroupBy, iParent, pSub->pEList);//��ȡpParent->pGroupBy��pSub->pEList
		  pParent->pHaving = substExpr(db, pParent->pHaving, iParent, pSub->pEList);//��ȡpParent->pHaving���ʽ��pSub->pEList
		}
		if( pSub->pOrderBy ){//�Ӳ�ѯ�к�OrderBy�Ӿ�
		  assert( pParent->pOrderBy==0 );//����ϵ㣬�쳣����
		  pParent->pOrderBy = pSub->pOrderBy;//�Ӳ�ѯ��pOrderBy��ֵ������ѯ�е�pOrderBy�Ӿ���д���
		  pSub->pOrderBy = 0;//����������pSub->pOrderBy
		}else if( pParent->pOrderBy ){//������ѯ��pOrderBy�Ӿ�
		  substExprList(db, pParent->pOrderBy, iParent, pSub->pEList);//��ȡpParent->pOrderBy��pSub->pEList
		}
		if( pSub->pWhere ){//����where�Ӿ�
		  pWhere = sqlite3ExprDup(db, pSub->pWhere, 0);//���
		}else{
		  pWhere = 0;//û��where�Ӿ䣬��0
		}
		if( subqueryIsAgg ){//�����оۼ�����
			//�ۼ������Ĵ���
		  assert( pParent->pHaving==0 );//�쳣�ϵ㴦��
		  pParent->pHaving = pParent->pWhere;//where�Ӿ���having�д���
		  pParent->pWhere = pWhere;//��ǰ��where�Ӿ丳ֵ������ѯ��
		  pParent->pHaving = substExpr(db, pParent->pHaving, iParent, pSub->pEList);//��ȡpParent->pHaving��SELECT��pWhere
		  pParent->pHaving = sqlite3ExprAnd(db, pParent->pHaving,
									  sqlite3ExprDup(db, pSub->pHaving, 0));//����ѯpHaving�Ӿ����Ӳ�ѯpHaving�Ӿ䣬��ֵ������ѯ��pHaving
		  assert( pParent->pGroupBy==0 );//����ϵ㣬�������ѯ�к�GroupBy�׳��쳣
		  pParent->pGroupBy = sqlite3ExprListDup(db, pSub->pGroupBy, 0);//��ȿ���
		}else{
		  pParent->pWhere = substExpr(db, pParent->pWhere, iParent, pSub->pEList);//��ȡpParent->pWhere��pSub->pEList
		  pParent->pWhere = sqlite3ExprAnd(db, pParent->pWhere, pWhere);//where�Ӿ�������
		}
	  
		/* The flattened query is distinct if either the inner or the
		** outer query is distinct. 
		*//*����ڲ�ѯ�����ѯ����Ψһ�ģ���ƽ����ѯҲ��Ψһ�ġ�
		pParent->selFlags |= pSub->selFlags & SF_Distinct;
	  /*λ����*/
		/*
		** SELECT ... FROM (SELECT ... LIMIT a OFFSET b) LIMIT x OFFSET y;
		**
		** One is tempted to try to add a and b to combine the limits.  But this
		** does not work if either limit is negative.
		*//*���limit��ʹ��a��b����limit�Ǹ���ʱ����������
		if( pSub->pLimit ){//�Ӳ�ѯ�к���limit�Ӿ�
		  pParent->pLimit = pSub->pLimit;//�Ӳ�ѯ��������ѯ��limit
		  pSub->pLimit = 0;//��0
		}
	  }
	  
	  /* Finially, delete what is left of the subquery and return
	  ** success.
	  *//*���ɾ����ߵ��Ӳ�ѯ���ɹ�����*/
	  sqlite3SelectDelete(db, pSub1);/*�������ݿ�ɾ������*/

	  return 1;//��ʾִ��ͨ��
	}
	#endif /* !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW) */

/*
** Analyze the SELECT statement passed as an argument to see if it
** is a min() or max() query. Return WHERE_ORDERBY_MIN or WHERE_ORDERBY_MAX if 
** it is, or 0 otherwise. At present, a query is considered to be
** a min()/max() query if:
**�����������ݵ� SELECT����������Ƿ���һ����Сֵ�����ֵ��ѯ������ǣ��򷵻�WHERE_ORDERBY_MIN��WHERE_ORDERBY_MAX��
���򷵻�0.Ŀǰ��һ����ѯ�������������������Ϊ����һ����С�����ֵ��ѯ��
**   1. There is a single object in the FROM clause.
**   2. There is a single expression in the result set, and it is
**      either min(x) or max(x), where x is a column reference.
**   1.��From�Ӿ�����һ����һ����
**   2.�ڽ��������һ����һ���ʽ��������Ҫô��Сֵ��Ҫô���ֵ������xΪһ���вο���*/
static u8 minMaxQuery(Select *p){
  Expr *pExpr; //��ʼ��һ�����ʽ
  ExprList *pEList = p->pEList; //��ʼ�����ʽ�б�

  if( pEList->nExpr!=1 ) return WHERE_ORDERBY_NORMAL;  //�б��в���ֻ��һ�����ʽ����0������min()����Ҳ����max()����
  pExpr = pEList->a[0].pExpr; //�����ʽ�б��еĵ�һ�����ʽ����pExpr
  if( pExpr->op != TK_AGG_FUNCTION ) return 0; //������ʽ�Ĳ����벻����TK_AGG_FUNCTION���򷵻�0
/*
  if( NEVER((pExpr->flags & EP_xIsSelect) == EP_xIsSelect))
  pExpr->flags == EP_*
*/
  if( NEVER(ExprHasProperty(pExpr, EP_xIsSelect)) ) return 0; //�жϱ��ʽ��flags�Ƿ�����x.pSelect����Ч��
  pEList = pExpr->x.pList;
  if( pEList==0 || pEList->nExpr!=1 ) return 0;
  if( pEList->a[0].pExpr->op!=TK_AGG_COLUMN ) return WHERE_ORDERBY_NORMAL; //����µı��ʽ�Ĳ����벻����TK_AGG_COLUMN���򷵻�0
  assert( !ExprHasProperty(pExpr, EP_IntValue) ); //���ö��ԣ��ж�ExprHasProperty(pExpr, EP_IntValue) == false�Ƿ����
  if( sqlite3StrICmp(pExpr->u.zToken,"min")==0 ){ //������ʽ�ı��ֵ����min�򷵻�WHERE_ORDERBY_MIN
    return WHERE_ORDERBY_MIN;
  }else if( sqlite3StrICmp(pExpr->u.zToken,"max")==0 ){//������ʽ�ı��ֵ����max�򷵻�WHERE_ORDERBY_MAX
    return WHERE_ORDERBY_MAX;
  }
  return WHERE_ORDERBY_NORMAL; //���򷵻�0������min()����Ҳ����max()����
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
	/* ��Ϊ��һ���������ݵ� select ����Ǿۼ���ѯ��* * �ڶ���������������ľۼ���Ϣ����
	** 
	** SELECT count(*) FROM <tbl>
	** ����������ݿ��еı�������һ���Ӳ�ѯ�Ľ��Ҳ����һ����ͼ�������ѯ��ģʽƥ�䣬��ô���и�ָ��Table�������<tab>��ָ�뷵�ء�
	** ����ͷ���0
	*/
	static Table *isSimpleCount(Select *p, AggInfo *pAggInfo){//��������������p�ǲ�ѯ�﷨����pAggInfo�Ǿۼ������ṹ��
	  Table *pTab;//����һ����
	  Expr *pExpr;//����һ�����ʽ

	  assert( !p->pGroupBy );//�쳣��������ϵ㣬�����жϼ��

	  if( p->pWhere || p->pEList->nExpr!=1 //��WHERE�Ӿ���߱��ʽ�ĸ���������1
	   || p->pSrc->nSrc!=1 || p->pSrc->a[0].pSelect//����FROM�Ӿ䲻����1������Ƕ�ײ�ѯ
	  ){
		return 0;
	  }
	  pTab = p->pSrc->a[0].pTab;//�����еĵ�һ�����ֵ��ֵ��Table�ṹ���ԱpTab
	  pExpr = p->pEList->a[0].pExpr;//�����е�һ�����ʽ��ֵ��ֵ�����ʽpExpr
	  assert( pTab && !pTab->pSelect && pExpr );//�쳣��������ϵ㣬�����жϼ��

	  if( IsVirtual(pTab) ) return 0;//���ptab�Ǹ����
	  if( pExpr->op!=TK_AGG_FUNCTION ) return 0;//�Ǿۼ�����
	  if( NEVER(pAggInfo->nFunc==0) ) return 0;//û�оۼ�����
	  if( (pAggInfo->aFunc[0].pFunc->flags&SQLITE_FUNC_COUNT)==0 ) return 0;//�ۼ������ı�Ǳ�����SQLITE_FUNC_COUNTλ���㲻��
	  if( pExpr->flags&EP_Distinct ) return 0;//���ʽ��Ǳ�����EP_Distinct��ֵ���

	  return pTab;//����ָ��
	}

	/*
	** If the source-list item passed as an argument was augmented with an
	** INDEXED BY clause, then try to locate the specified index. If there
	** was such a clause and the named index cannot be found, return 
	** SQLITE_ERROR and leave an error in pParse. Otherwise, populate 
	** pFrom->pIndex and return SQLITE_OK.
	*/
	/*���Դ�б������Ϊһ��������������ģ���ô�ó��Զ�λ����������������һ���Ӿ���ұ������������Ҳ����ˣ�
	��ô�ͷ��ش������ڽ������б�ǳ�����
	������䵽 pFrom->pIndex���ҷ���һ�� SQLITE_OK
	*/
	//������Ĵ���
	int sqlite3IndexedByLookup(Parse *pParse, struct SrcList_item *pFrom){
	  if( pFrom->pTab && pFrom->zIndex ){//SrcList_itemΪFROM�Ľṹ�壬pTab�ǿ���pFrom->zIndex�ǿ�
		Table *pTab = pFrom->pTab;//����һ����
		char *zIndex = pFrom->zIndex;//from���������ʶ��
		Index *pIdx;//����һ������ָ��

		//�������е����������ָ�����ֵ�����
		for(pIdx=pTab->pIndex; 
			pIdx && sqlite3StrICmp(pIdx->zName, zIndex); 
			pIdx=pIdx->pNext
		);
		if( !pIdx ){//û���ҵ���Ӧ��������
		  sqlite3ErrorMsg(pParse, "no such index: %s", zIndex, 0);//���������Ϣ
		  pParse->checkSchema = 1;//�﷨������������Ϣ��ʶ
		  return SQLITE_ERROR;
		}
		pFrom->pIndex = pIdx;//�ҵ��������ӵ�FROM���ʽ��������ṹ����
	  }
	  return SQLITE_OK;//ִ����ȷ
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
	/* ���������Walker�ص���expanding����һ��SELECT��䣬��Expanding����ָ��������Щ��
	**   ��1��ȷ��VEBE�α���Ѿ��������FROM�Ӿ��ÿ��Ԫ��   
	**   ��2����SrcList�У����pTabList->a[].pTab�е����Ƕ�from�Ӿ�Ķ��塣��FROM�Ӿ��г�����ͼ�����һ��ʵ����ͼ��select�ĸ�����pTabList->a[].pSelect
	**        ��SELECT������ɵĸ��������ǿ������ɵ��޸Ļ���ɾ�������䣬�����õ���
	**        �Ὣ��ǰ����ͼ�޸ġ�
	**   ��3�������Ӳ��������Ӳ������漰��on�Լ�using�ϣ����һ��WHERE�Ӿ�����NATURAL�ؼ���
	**   ��4���ڽ������ɨ���б����ҡ�*����������ʵ����TABLE.*������������ҵ��ˣ���ÿ�����ÿһ����չ
	*/
	static int selectExpander(Walker *pWalker, Select *p){
	  Parse *pParse = pWalker->pParse;//�﷨������������
	  int i, j, k;//�Զ������
	  SrcList *pTabList;//from�Ӿ��б�ָ�������
	  ExprList *pEList;//���ʽ�б������
	  struct SrcList_item *pFrom;//ʵ����һ���ṹ��
	  sqlite3 *db = pParse->db;//���ݿ����ӵĶ���

	  if( db->mallocFailed  ){//����ڴ��������
		return WRC_Abort;//*����ڴ���������ֹ����
	  }
	  if( NEVER(p->pSrc==0) || (p->selFlags & SF_Expanded)!=0 ){//û��from�Ӿ���Ƿ���SF_Expandedλ�������
		return WRC_Prune;//ɾ��
	  }
	  p->selFlags |= SF_Expanded;//������λ����
	  pTabList = p->pSrc;//FROM�Ӿ���ָ�ĸ�ֵ��FROM�Ӿ��б�ָ��
	  pEList = p->pEList;

	  /* Make sure cursor numbers have been assigned to all entries in
	  ** the FROM clause of the SELECT statement.
	  *//*ȷ���α���Ѿ���������е���SELECT�������FROM�Ӿ��*/
	  sqlite3SrcListAssignCursors(pParse, pTabList);/*Ϊ���ʽ�б�pTabList�����б�����α��*/

	  /* Look up every table named in the FROM clause of the select.  If
	  ** an entry of the FROM clause is a subquery instead of a table or view,
	  ** then create a transient table structure to describe the subquery.
	  *//*����SELECT��FROM�Ӿ���ÿһ����������FROM�Ӿ��һ����Ŀ���Ӳ�ѯ������һ�������ͼ��
	  ��ô�ʹ���һ�������Ӳ�ѯ�������	  */
	  //������
	  for(i=0, pFrom=pTabList->a; i<pTabList->nSrc; i++, pFrom++){
		Table *pTab;//����һ��table���͵ı���
		if( pFrom->pTab!=0 ){//���from�������б�
		  /* This statement has already been prepared.  There is no need
		  ** to go further. */
		  ��������Ѿ�������ִ�У�û�б�Ҫ�������С�
		  assert( i==0 );//�쳣��������ϵ㡣
		  return WRC_Prune;//ɾ��
		}
		if( pFrom->zName==0 ){
		//������Ϊ��
	#ifndef SQLITE_OMIT_SUBQUERY
		  Select *pSel = pFrom->pSelect;//��pFrom��ֵ��pSel
		  /* A sub-query in the FROM clause of a SELECT *//*SELECT��FROM�Ӿ���Ӳ�ѯ*/
		  assert( pSel!=0 );//�쳣��������ϵ�
		  assert( pFrom->pTab==0 );//�쳣��������ϵ�
		  sqlite3WalkSelect(pWalker, pSel);//����sqlite3WalkSelect����
		  pFrom->pTab = pTab = sqlite3DbMallocZero(db, sizeof(Table));//����ͱ�Ĵ�С��ȵ��ڴ�
		  if( pTab==0 ) return WRC_Abort;//��������������ֹ����
		  pTab->nRef = 1;//pTab->nRef��ֵΪ1
		  pTab->zName = sqlite3MPrintf(db, "sqlite_subquery_%p_", (void*)pTab);//����ص���Ϣ��ӡ��������pTab->zName
		  //�������������ȵ�select
		  while( pSel->pPrior ){ pSel = pSel->pPrior; }
		  selectColumnsFromExprList(pParse, pSel->pEList, &pTab->nCol, &pTab->aCol);//�ڱ��ʽ�б��в�����
		  pTab->iPKey = -1;//��iPKey=-1��������������
		  pTab->nRowEst = 1000000;//���ñ������
		  pTab->tabFlags |= TF_Ephemeral;//λ���㣬���ñ�ı�Ǳ���
	#endif
		}else{
		  /* An ordinary table or view name in the FROM clause *//*FROM����г���ı����ͼ����*/
		  assert( pFrom->pTab==0 );//�쳣��������ϵ�
		  pFrom->pTab = pTab = 
			sqlite3LocateTable(pParse,0,pFrom->zName,pFrom->zDatabase);//���﷨���������ݿ����ӣ����ݿ����ֶ�λ��Ѱ�ҵı�
		  if( pTab==0 ) return WRC_Abort;//�����ڸñ�����ֹ
		  pTab->nRef++;//�Լ�
	#if !defined(SQLITE_OMIT_VIEW) || !defined (SQLITE_OMIT_VIRTUALTABLE)
		  if( pTab->pSelect || IsVirtual(pTab) ){//������SELECT�ǿջ�pTab�����
			/* We reach here if the named table is a really a view *//*������һ���������˵���������һ����������ͼ*/
			if( sqlite3ViewGetColumnNames(pParse, pTab) ) return WRC_Abort;//��ȡ��ͼ���������д�����ֹ
			assert( pFrom->pSelect==0 );//�쳣��������ϵ�
			pFrom->pSelect = sqlite3SelectDup(db, pTab->pSelect, 0);//���
			sqlite3WalkSelect(pWalker, pFrom->pSelect);/*����sqlite3WalkSelect��sqlite3WalkExpr
		  }
	#endif
		}

		/* Locate the index named by the INDEXED BY clause, if any. *//*�������Ӿ��ж�λ������������*/
		if( sqlite3IndexedByLookup(pParse, pFrom) ){//��������
		  return WRC_Abort;//�ٷ�����ִֹ�б��
		}
	  }

	  /* Process NATURAL keywords, and ON and USING clauses of joins.
	  *//*���������е�NATURAL�ؼ��� ON USING*/
	  if( db->mallocFailed || sqliteProcessJoin(pParse, p) ){//�ڴ����ʧ�ܻ��ߴ������Ӳ���
		return WRC_Abort;//��ֹ
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
	  ** ���б��г��ֵ�ÿһ����*���������еı�����е����в������֡�����ÿ��������������㣬������������е�����
	  ** �����κεġ�*�������﷨�������У�����һ���ۼ�������������������ʽ��
	  ** ʣ�µĴ�������ҵ��ۼ����ʽ������չ���б������е��б�
	  */

	  //�������ʽ�б�
	  for(k=0; k<pEList->nExpr; k++){
		Expr *pE = pEList->a[k].pExpr;/*���ʽ�б������е�ÿ��Ԫ�ظ�ֵ��pe*/
		if( pE->op==TK_ALL ) break;/*�������Ϊ�ۼ�����*/
		assert( pE->op!=TK_DOT || pE->pRight!=0 );//�쳣��������ϵ�
		assert( pE->op!=TK_DOT || (pE->pLeft!=0 && pE->pLeft->op==TK_ID) );//�쳣��������ϵ�
		if( pE->op==TK_DOT && pE->pRight->op==TK_ALL ) break;//��������������ѭ����
	  }
	  if( k<pEList->nExpr ){//��k��ֵС�ڱ��ʽ�б�ĸ���
		/*
		** If we get here it means the result set contains one or more "*"
		** operators that need to be expanded.  Loop through each expression
		** in the result set and expand them one by one.
		//������һ�����������������һ��������*��������Щ��������Ҫ��չ�ġ�ѭ������ÿһ��������еı��ʽ������չ����*/
	
		struct ExprList_item *a = pEList->a;//ExprList_item�ṹ�����ĸ�ֵ
		ExprList *pNew = 0;//*����һ�����ʽ�б�
		int flags = pParse->db->flags;//�������б�Ǳ���
		int longNames = (flags & SQLITE_FullColNames)!=0//flagsΪSQLITE_FullColNames���Ҳ�����SQLITE_ShortColNames
						  && (flags & SQLITE_ShortColNames)==0;

		//�������ʽ�б�
		for(k=0; k<pEList->nExpr; k++){
		  Expr *pE = a[k].pExpr;/*���ʽ�б������е�ÿ��Ԫ�ظ�ֵ��pe
		  assert( pE->op!=TK_DOT || pE->pRight!=0 );//�쳣��������ϵ�
		  if( pE->op!=TK_ALL && (pE->op!=TK_DOT || pE->pRight->op!=TK_ALL) ){
			/* This particular expression does not need to be expanded.
			//��������ı�ﲻ��Ҫ��չ*/
			pNew = sqlite3ExprListAppend(pParse, pNew, a[k].pExpr);//׷��
			if( pNew ){//�ǿ�
			  pNew->a[pNew->nExpr-1].zName = a[k].zName;//����Ԫ�ر������Ը�ֵ
			  pNew->a[pNew->nExpr-1].zSpan = a[k].zSpan;//����Ԫ�ص�zSpan���Ը�ֵ
			  a[k].zName = 0;//��0
			  a[k].zSpan = 0;//��0
			}
			a[k].pExpr = 0;//��0
		  }else{
			/* This expression is a "*" or a "TABLE.*" and needs to be
			** expanded.������ʽ��һ��*�����������ӣ���������Ҫ����չ��  */
			int tableSeen = 0;      /* Set to 1 when TABLE matches *///��ȷƥ���ʱ������Ϊ1
			char *zTName;            /* text of name of TABLE *///����ı���
			if( pE->op==TK_DOT ){
			  assert( pE->pLeft!=0 );//�쳣��������ϵ�
			  assert( !ExprHasProperty(pE->pLeft, EP_IntValue) );//�쳣��������ϵ�
			  zTName = pE->pLeft->u.zToken;//���ʽ��ߵ�u.zToken��ֵ���ı�
			}else{
			  zTName = 0;//�����ı����
			}
			//���ʽ�б�ı���
			for(i=0, pFrom=pTabList->a; i<pTabList->nSrc; i++, pFrom++){
			  Table *pTab = pFrom->pTab;//�����
			  char *zTabName = pFrom->zAlias;//from�Ӿ��������ϵ
			  if( zTabName==0 ){//Ϊ��
				zTabName = pTab->zName;//ֱ�Ӹ�����
			  }
			  if( db->mallocFailed ) break;//�ڴ����ʧ�ܣ�����ѭ����
			  if( zTName && sqlite3StrICmp(zTName, zTabName)!=0 ){//�����ǿգ�����һ��
				continue;//��������ѭ��
			  }
			  tableSeen = 1;//���ñ�Ǳ���
			  //�������е���
			  for(j=0; j<pTab->nCol; j++){
				Expr *pExpr, *pRight;//�������ʽ
				char *zName = pTab->aCol[j].zName;//��ȡÿһ�е�����
				char *zColname;  // The computed column name //���������
				char *zToFree;   /* Malloced string that needs to be freed *//*�ͷ��ѷ����ַ����Ŀռ�*/
				Token sColname;  /* Computed column name as a token *//*�������

				/* If a column is marked as 'hidden' (currently only possible
				** for virtual tables), do not include it in the expanded result-set list.
				** result-set list.
				*//*�����һ�б��Ϊ���أ���ǰֻ�ܶ��������չ�Ľ�����в���������*/
				if( IsHiddenColumn(&pTab->aCol[j]) ){//�ж����Ƿ�����
				  assert(IsVirtual(pTab));//�쳣��������ϵ�
				  continue;//��������ѭ��
				}

				if( i>0 && zTName==0 ){
				  if( (pFrom->jointype & JT_NATURAL)!=0/*��������ΪJT_NATURAL*/
					&& tableAndColumnIndex(pTabList, i, zName, 0, 0)/*���ұ�����������Ϊ��*/
				  ){
					/* In a NATURAL join, omit the join columns from the 
					** table to the right of the join *//*(ע��ʹ����Ȼ���ӣ����Ҳ�����ӱ���ʡ��������)*/
					continue;//��������ѭ��
				  }
				  if( sqlite3IdListIndex(pFrom->pUsing, zName)>=0 ){//������
					/* In a join with a USING clause, omit columns in the
					** using clause from the table on the right. *//*��USING�Ӿ�����ӣ����ұ���ʡ����using�Ӿ��е���*/
					continue;//��������ѭ��
				  }
				}
				pRight = sqlite3Expr(db, TK_ID, zName);//����sqlite3Expr������������zNameһ��TK_ID���
				zColname = zName;//�������ĸ�ֵ
				zToFree = 0;
				if( longNames || pTabList->nSrc>1 ){
				  Expr *pLeft;//�������ʽ
				  pLeft = sqlite3Expr(db, TK_ID, zTabName); //����sqlite3Expr������������zTabNameһ��TK_ID���
				  pExpr = sqlite3PExpr(pParse, TK_DOT, pLeft, pRight, 0);//����sqlite3PExpr��������pLeft��pRight
				  if( longNames ){//ȫ·��
					zColname = sqlite3MPrintf(db, "%s.%s", zTabName, zName);//�������
					zToFree = zColname;
				  }
				}else{
				  pExpr = pRight;//pright��ֵ�����ı��ʽ
				}
				pNew = sqlite3ExprListAppend(pParse, pNew, pExpr);//pExpr׷�ӵ�pNew
				sColname.z = zColname;//��ֵ
				sColname.n = sqlite3Strlen30(zColname);//�������ַ�������
				sqlite3ExprListSetName(pParse, pNew, &sColname, 0);//���ñ��ʽ�б������
				sqlite3DbFree(db, zToFree);//�ͷ��ڴ�ռ�
			  }
			}
			if( !tableSeen ){
			  if( zTName ){//������Ϊ��
				sqlite3ErrorMsg(pParse, "no such table: %s", zTName);//��ӡ������Ϣ
			  }else{
				sqlite3ErrorMsg(pParse, "no tables specified");//��ӡ������Ϣ
			  }
			}
		  }
		}
		sqlite3ExprListDelete(db, pEList);//ɾ�����ʽ�б�
		p->pEList = pNew;//��ֵ
	  }
	#if SQLITE_MAX_COLUMN
	  if( p->pEList && p->pEList->nExpr>db->aLimit[SQLITE_LIMIT_COLUMN] ){//�����
		sqlite3ErrorMsg(pParse, "too many columns in result set");//��ӡ������Ϣ
	  }
	#endif
	  return WRC_Continue;
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
	/* ���ڽ�����walker������
	**
	** ��������Walker.xExprCallback�ص����������ʽ����û���κβ��������ռ��ÿ���ڵ㡣���裬����������Walker.xExprCallback
	** Ȼ��Walker.xSelectCallback Ϊ�﷨�������е�ÿһ���Ӳ�ѯ�ṩ������
	*/
	static int exprWalkNoop(Walker *NotUsed, Expr *NotUsed2){
	  UNUSED_PARAMETER2(NotUsed, NotUsed2);
	  return WRC_Continue;//����ִ��
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
	/* ������̡�expands��һ��SELECT���������е��Ӳ�ѯ�����ĸ�����Ϣ��ָ����"expand" ����SELECT������
	**
	** SELECT�����еĵ�һ������չһ��SELECT��䡣�ڽ���ִ��ǰ�����select�����뱻��չ
	**
	** ���д�����֣�������������Ϣ��д���������С��ص�����������pParse->nErr �� pParse->db->mallocFailed���ҳ�������Ϣ
	*/
	static void sqlite3SelectExpand(Parse *pParse, Select *pSelect){
	  Walker w;//Walker�ṹ�������
	  w.xSelectCallback = selectExpander;
	  w.xExprCallback = exprWalkNoop;//�����ʽ����δʹ�õ���Ϣ�����ûص�����
	  w.pParse = pParse;//��ֵ
	  sqlite3WalkSelect(&w, pSelect);//�ص�����������sqlite3WalkSelect
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
	/* ����һ��Walker.xSelectCallback�ص�sqlite3SelectTypeInfo()�����Ľӿڡ�
	**
	** �����κ�һ��FROM�Ӳ�ѯ�����Column.zType�� Column.zColl��Ϣ����ṹ�У��������Ӳ�ѯ�Ľ������
	**
	** ��ṹ����������selectExpander��������������������������ͺ�������Ϣ�ᱻʡ�ԣ���Ϊû�н�����ʶ����
	** �������ʵ�ڽ�����ʶ֮����õ�
	*/
	static int selectAddSubqueryTypeInfo(Walker *pWalker, Select *p){
	  Parse *pParse;//�﷨������������
	  int i;//����һ�����ͱ���
	  SrcList *pTabList;//����from�Ӿ�ı��ʽ�б�
	  struct SrcList_item *pFrom;//����ṹ�����

	  assert( p->selFlags & SF_Resolved );//�쳣��������ϵ�
	  if( (p->selFlags & SF_HasTypeInfo)==0 ){//����Ǳ�����������Ϣ
		p->selFlags |= SF_HasTypeInfo;//λ����
		pParse = pWalker->pParse;//�﷨������
		pTabList = p->pSrc;
		//�����б�ı��ʽ
		for(i=0, pFrom=pTabList->a; i<pTabList->nSrc; i++, pFrom++){
		  Table *pTab = pFrom->pTab;//��ȡfrom�Ӿ��е��б�
		  if( ALWAYS(pTab!=0) && (pTab->tabFlags & TF_Ephemeral)!=0 ){//�ж�����
			/* A sub-query in the FROM clause of a SELECT ��from�Ӿ��е��Ӳ�ѯ*/
			Select *pSel = pFrom->pSelect;//��ȡselect���
			assert( pSel );//�쳣��������ϵ�
			while( pSel->pPrior ) pSel = pSel->pPrior;//ѭ����ȡ�����ȵĲ�ѯ
			selectAddColumnTypeAndCollation(pParse, pTab->nCol, pTab->aCol, pSel);//����е����ͺ���ص���Ϣ
		  }
		}
	  }
	  return WRC_Continue;//����ִ��
	}
	#endif


	/*
	** This routine adds datatype and collating sequence information to
	** the Table structures of all FROM-clause subqueries in a
	** SELECT statement.
	**
	** Use this routine after name resolution.
	*/
	/* ������from�Ӿ���Ӳ�ѯ�еı�ṹ�����datatype��������Ϣ��SELECT���Ӳ�ѯ������FROM�Ӿ��TABLE�ṹ�С�
	** �����ֽ�����ʹ����γ���*/
	static void sqlite3SelectAddTypeInfo(Parse *pParse, Select *pSelect){
	#ifndef SQLITE_OMIT_SUBQUERY
	  Walker w;//����һ��Walker�ṹ��
	  w.xSelectCallback = selectAddSubqueryTypeInfo;//�Ӳ�ѯ������Ϣ���ص�����
	  w.xExprCallback = exprWalkNoop;//���ʽ��Ϣ���ص�����
	  w.pParse = pParse;//������
	  sqlite3WalkSelect(&w, pSelect);//�ص�����������sqlite3WalkSelect
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
	/* ����������� SELECT �����д���
	**   * VDBE�α�ŷ�������е�FROM�Ӿ�
	**   * Ϊ���� FROM �Ӿ���Ӳ�ѯ������ʱ�����
	**   * ON �� USING �Ӿ��л���WHERE���
	**   * ���������չͨ���"*" �� "TABLE������"
	**   * �ڱ��ʽ�еı�ʶ����ƥ��ı�
	** ��������Եݹ�ķ�ʽ�������Ӳ�ѯ
	*/
	void sqlite3SelectPrep(
	  Parse *pParse,         /* The parser context *///���������
	  Select *p,             /* The SELECT statement being coded. *///����select���͵�ָ��
	  NameContext *pOuterNC  /* Name context for container *///Ϊ��������
	){
	  sqlite3 *db;//����һ��sqlite���͵����ݿ�����
	  if( NEVER(p==0) ) return;//��ָ�룬ֱ�ӷ���
	  db = pParse->db;//��ȡ�﷨�������е����ݿ�
	  if( p->selFlags & SF_HasTypeInfo ) return;//��ʶ������������Ϣ���ж�
	  sqlite3SelectExpand(pParse, p);//��չ
	  if( pParse->nErr || db->mallocFailed ) return;//����������߷����ڴ�ʧ��
	  sqlite3ResolveSelectNames(pParse, p, pOuterNC);//����������
	  if( pParse->nErr || db->mallocFailed ) return;//����������߷����ڴ�ʧ��
	  sqlite3SelectAddTypeInfo(pParse, p);//�����������﷨��Ϣ
	}

	/*
	** Reset the aggregate accumulator.
	**
	** The aggregate accumulator is a set of memory cells that hold
	** intermediate results while calculating an aggregate.  This
	** routine generates code that stores NULLs in all of those memory
	** cells.
	*/
	/* �ۼ���������������
	** �ۼ��ۼ�����һ����䵥Ԫ�������ڼ���һ���ۼ���ʱ�򱣴��м���������γ������ɵĴ��������еļ��䵥Ԫ�д洢�˿�ֵ
	*/
	static void resetAccumulator(Parse *pParse, AggInfo *pAggInfo){
	  Vdbe *v = pParse->pVdbe;//��ȡ�﷨�������е�pVdbe����
	  int i;//����һ������
	  struct AggInfo_func *pFunc;//����һ��AggInfo_func�ṹ�����
	  if( pAggInfo->nFunc+pAggInfo->nColumn==0 ){
		return;
	  }

	  //�������е���
	  for(i=0; i<pAggInfo->nColumn; i++){
		sqlite3VdbeAddOp2(v, OP_Null, 0, pAggInfo->aCol[i].iMem);//��������
	  }
	  //�ۼ������ı���
	  for(pFunc=pAggInfo->aFunc, i=0; i<pAggInfo->nFunc; i++, pFunc++){
		sqlite3VdbeAddOp2(v, OP_Null, 0, pFunc->iMem);//��������
		if( pFunc->iDistinct>=0 )
		  Expr *pE = pFunc->pExpr;//��ȡAggInfo_func�еı��ʽ
		  assert( !ExprHasProperty(pE, EP_xIsSelect) );//�쳣��������ϵ�
		  if( pE->x.pList==0 || pE->x.pList->nExpr!=1 ){
			sqlite3ErrorMsg(pParse, "DISTINCT aggregates must have exactly one "
			   "argument");//��ӡ������Ϣ
			pFunc->iDistinct = -1;//��ʱ����Ϊ-1����Ч��
		  }else{
			KeyInfo *pKeyInfo = keyInfoFromExprList(pParse, pE->x.pList);//�ӱ��ʽ�б��л�ȡ��Ϣ
			sqlite3VdbeAddOp4(v, OP_OpenEphemeral, pFunc->iDistinct, 0, 0,
							  (char*)pKeyInfo, P4_KEYINFO_HANDOFF);//�����һ��������OP_OpenEphemeral
		  }
		}
	  }
	}
	
	/*
	** Invoke the OP_AggFinalize opcode for every aggregate function 
	** in the AggInfo structure.
	*/
	/*
	** ��AggInfo�ṹ���У�Ϊÿһ���ۼ���������OP_AggFinalize������*/

	static void finalizeAggFunctions(Parse *pParse, AggInfo *pAggInfo){
	  Vdbe *v = pParse->pVdbe;//��ȡ�﷨�������е�pVdbe����
	  int i;//����һ������
	  struct AggInfo_func *pF;//����һ��AggInfo_func�ṹ�����
	  //�ۼ������ı���
	  for(i=0, pF=pAggInfo->aFunc; i<pAggInfo->nFunc; i++, pF++){
		ExprList *pList = pF->pExpr->x.pList;//��ȡ���ʽ�б�
		assert( !ExprHasProperty(pF->pExpr, EP_xIsSelect) );//�쳣��������ϵ�
		sqlite3VdbeAddOp4(v, OP_AggFinal, pF->iMem, pList ? pList->nExpr : 0, 0,
						  (void*)pF->pFunc, P4_FUNCDEF);//�����һ��������OP_OpenEphemeral
	  }
	}	

/*
** Update the accumulator memory cells for an aggregate based on
** the current cursor position.
**
**Ϊһ�����ڵ�ǰ������λ���ϵľۼ�����update�ۼ������ڴ浥Ԫ
*/
static void updateAccumulator(Parse *pParse, AggInfo *pAggInfo){
	Vdbe *v = pParse->pVdbe;
	int i;
	int regHit = 0;
	int addrHitTest = 0;
	struct AggInfo_func *pF;
	struct AggInfo_col *pC;/*���﷨�������е�pVdbe��ֵ��v��������һЩ��ʼ������*/

	pAggInfo->directMode = 1;/*����������Ϊ1*/
	sqlite3ExprCacheClear(pParse);/*���������*/
	for (i = 0, pF = pAggInfo->aFunc; i < pAggInfo->nFunc; i++, pF++){/*��ʼ���оۼ������ı���*/
		int nArg;
		int addrNext = 0;
		int regAgg;
		ExprList *pList = pF->pExpr->x.pList;
		assert(!ExprHasProperty(pF->pExpr, EP_xIsSelect));/*����һ���ϵ㣬���pE����EP_xIsSelect����ô���׳��������Ϣ*/
		if (pList){
			nArg = pList->nExpr;
			regAgg = sqlite3GetTempRange(pParse, nArg);
			sqlite3ExprCodeExprList(pParse, pList, regAgg, 1);/*��ʼΪ�ۼ���������Ĵ������ѱ��ʽ�е�ֵ�Ĵ��ڼĴ�����*/
		}
		else{
			nArg = 0;
			regAgg = 0;/*���ۼ������ĸ����ʹ洢�ۼ������Ĵ���������0����*/
		}
		if (pF->iDistinct >= 0){
			addrNext = sqlite3VdbeMakeLabel(v);
			assert(nArg == 1);
			codeDistinct(pParse, pF->iDistinct, addrNext, 1, regAgg);/*�����Դ����ȡֵ�ĸ���Ϊ0��ΪVDBE����һ����ǩ������ֵ��ֵ����һ�����е�ַ��
			                                                       ���Ҳ���ϵ㣬����ۼ�������Ϊ1���׳�������Ϣ*/
		}
		if (pF->pFunc->flags & SQLITE_FUNC_NEEDCOLL){
			CollSeq *pColl = 0;
			struct ExprList_item *pItem;
			int j;
			assert(pList != 0);  /* pList!=0 if pF->pFunc has NEEDCOLL */
			for (j = 0, pItem = pList->a; !pColl && j < nArg; j++, pItem++){
				pColl = sqlite3ExprCollSeq(pParse, pItem->pExpr);/*���pF->pFunc��NEEDCOLL����pList��Ϊ0,��ô�����ۼ�����������һ��Ĭ�ϵ���������*/
			}
			if (!pColl){
				pColl = pParse->db->pDfltColl;/*����������в�Ϊ0����ô�����ݿ������е�Ĭ�ϵ��������и�ֵ��pColl*/
			}
			if (regHit == 0 && pAggInfo->nAccumulator) regHit = ++pParse->nMem;
			sqlite3VdbeAddOp4(v, OP_CollSeq, regHit, 0, 0, (char *)pColl, P4_COLLSEQ);/*���regHitΪ0�����ۼ�����Ϊ0���ڴ浥Ԫ�����Լ�֮���ٸ�ֵ��regHit��
			                                                                           ���һ��OP_CollSeq��������������ֵ��Ϊһ��ָ��*/
		}
		sqlite3VdbeAddOp4(v, OP_AggStep, 0, regAgg, pF->iMem,
			(void*)pF->pFunc, P4_FUNCDEF);
		sqlite3VdbeChangeP5(v, (u8)nArg);
		sqlite3ExprCacheAffinityChange(pParse, regAgg, nArg);
		sqlite3ReleaseTempRange(pParse, regAgg, nArg);
		if (addrNext){
			sqlite3VdbeResolveLabel(v, addrNext);
			sqlite3ExprCacheClear(pParse);/*�������һ��ִ�е�ַ������addrNext������Ϊ��һ��������ĵ�ַ��������������е��﷨������*/
		}��
	}

	/* Before populating the accumulator registers, clear the column cache.
	** Otherwise, if any of the required column values are already present
	** in registers, sqlite3ExprCode() may use OP_SCopy to copy the value
	** to pC->iMem. But by the time the value is used, the original register
	** may have been used, invalidating the underlying buffer holding the
	** text or blob value. See ticket [883034dcb5].
	**
	**�ڻ�ȡ�ۼӼĴ������ڴ�֮ǰ,����л��档
	**����,����κ��������ֵ�Ѿ������ڼĴ���,
	**sqlite3ExprCode()����ʹ��OP_SCopyֵ���Ƶ�pC->iMem��
	**�����ͬʱ���ֵҲ��ʹ��,��ʼ�Ĵ������ܱ�ʹ��,
	**Ǳ�ڻ������б�����ı���ֵ����Ч�ġ�
	**
	** Another solution would be to change the OP_SCopy used to copy cached
	** values to an OP_Copy.
	**
	**��һ����������Ǹı�OP_SCopy�������ƻ���ֵ��OP_Copy��
	**
	*/
	if (regHit){
		addrHitTest = sqlite3VdbeAddOp1(v, OP_If, regHit);
	}
	sqlite3ExprCacheClear(pParse);/*��OP_Yield��������vdbe��Ȼ�󷵻���������ĵ�ַ����������е��﷨������*/
	for (i = 0, pC = pAggInfo->aCol; i < pAggInfo->nAccumulator; i++, pC++){/*�����ۼ���*/
		sqlite3ExprCode(pParse, pC->pExpr, pC->iMem);
	}
	pAggInfo->directMode = 0;
	sqlite3ExprCacheClear(pParse);
	if (addrHitTest){/*���addrHitTest��Ϊ��*/
		sqlite3VdbeJumpHere(v, addrHitTest);/*���addrHitTest��Ϊ�գ��������addrHitTest��Ϊ0���Ǹ����е�ַ��������ǰ��ַ*/
	}
}

/*
** Add a single OP_Explain instruction to the VDBE to explain a simple
** count(*) query ("SELECT count(*) FROM pTab").
**���һ����һ��OP_Explain �ṹ��VDBE ����������һ��������count(*)��ѯ.
*/
#ifndef SQLITE_OMIT_EXPLAIN
static void explainSimpleCount(
	Parse *pParse,                  /* ���������� */
	Table *pTab,                    /* ���ڲ�ѯ�ı�*/
	Index *pIdx                     /* �����Ż�ɨ������� */
	){
	if (pParse->explain == 2){/*����﷨��������explain���ʽΪ2*/
		char *zEqp = sqlite3MPrintf(pParse->db, "SCAN TABLE %s %s%s(~%d rows)",
			pTab->zName,
			pIdx ? "USING COVERING INDEX " : "",
			pIdx ? pIdx->zName : "",
			pTab->nRowEst
			);/*��ӡ���������ֵ��zEqp������%s%s%sΪ����ı�����*/
		sqlite3VdbeAddOp4(
			pParse->pVdbe, OP_Explain, pParse->iSelectId, 0, 0, zEqp, P4_DYNAMIC
			);/*�����ΪOP_Explain�Ĳ�������������ֵ��Ϊһ��ָ��*/
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
**Ϊ����p������дSELECT��䡣 
**����ֲ��ڲ�ͬ��SelectDest�ṹĿ¼ָ���У����£�
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
	Parse *pParse,         /* The parser context */
	Select *p,             /* The SELECT statement being coded. SELECT��䱻����*/
	SelectDest *pDest      /* What to do with the query results ��δ����ѯ���*/
	){
	int i, j;              /* Loop counters ѭ��������*/
	WhereInfo *pWInfo;     /* Return from sqlite3WhereBegin() ��sqlite3WhereBegin()����*/
	Vdbe *v;               /* The virtual machine under construction �����е������*/
	int isAgg;             /* True for select lists like "count(*)" ѡ���Ƿ��Ǿۼ�*/
	ExprList *pEList;      /* List of columns to extract. ��ȡ�����б�*/
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
	AggInfo sAggInfo;      /* Information used by aggregate queries �ۼ���Ϣ*/
	int iEnd;              /* Address of the end of the query ��ѯ������ַ*/
	sqlite3 *db;           /* The database connection ���ݿ�����*/

#ifndef SQLITE_OMIT_EXPLAIN
	int iRestoreSelectId = pParse->iSelectId;
	pParse->iSelectId = pParse->iNextSelectId++;/*���﷨�������в��ҵ�ID�洢��iRestoreSelectId�У�Ȼ���ٽ��﷨����������һ������ID�洢��iSelectId��*/
#endif

	db = pParse->db;
	if (p == 0 || db->mallocFailed || pParse->nErr){
	return 1;  
	}  /*����һ�����ݿ����ӣ����SELECTΪ�ջ�����ڴ�ʧ�ܻ��﷨���������д�����ôֱ�ӷ���1*/
   
	if (sqlite3AuthCheck(pParse, SQLITE_SELECT, 0, 0, 0)) return 1;/*��Ȩ���,�д���Ҳ����1*/
	memset(&sAggInfo, 0, sizeof(sAggInfo));/*��sAggInfo��ǰsizeof(sAggInfo)���ֽ���0�滻*/

	if (IgnorableOrderby(pDest)){
		assert(pDest->eDest == SRT_Exists || pDest->eDest == SRT_Union ||
			pDest->eDest == SRT_Except || pDest->eDest == SRT_Discard);
		/*�����ѯ����еľۼ�����û��oderby��������ô����ϵ㣬�����жϴ��������д���ʽΪSRT_Exists��SRT_Union���SRT_Except��SRT_Discar��
		�����û�У����״�*/
		sqlite3ExprListDelete(db, p->pOrderBy);/*ɾ�����ݿ��е�orderby�Ӿ���ʽ*/
		p->pOrderBy = 0;
		p->selFlags &= ~SF_Distinct;
	}
	sqlite3SelectPrep(pParse, p, 0);
	pOrderBy = p->pOrderBy;
	pTabList = p->pSrc;
	pEList = p->pEList;
	if (pParse->nErr || db->mallocFailed){
		goto select_end;/*����﷨������ʧ�ܻ��������ת��select_end*/
	}
	isAgg = (p->selFlags & SF_Aggregate) != 0;
	assert(pEList != 0);/*����selFlags�ж��Ƿ��Ǿۼ������������isAggΪtrue������ΪFALSE��Ȼ�����ϵ㣬������ʽ�б�Ϊ�գ����׳�������Ϣ*/

	/* Begin generating code.
	**��ʼ���ɴ���
	*/
	v = sqlite3GetVdbe(pParse);
	if (v == 0) goto select_end;/*�����﷨����������һ�������Database���棬���vdbe��ȡʧ�ܣ���������ѯ����*/

	/* If writing to memory or generating a set
	** only a single column may be output.
	**���д���ڴ��������һ�����ϣ�
	**��ô����һ���������п��ܱ����
	*/
#ifndef SQLITE_OMIT_SUBQUERY
	if (checkForMultiColumnSelectError(pParse, pDest, pEList->nExpr)){
		goto select_end;/*�����⵽��ά�в�ѯ���󣬾���ת����ѯ����*/
	}
#endif

	/* Generate code for all sub-queries in the FROM clause
	   *  Ϊfrom����е��Ӳ�ѯ���ɴ���
	   */
#if !defined(SQLITE_OMIT_SUBQUERY) || !defined(SQLITE_OMIT_VIEW)
	for (i = 0; !p->pPrior && i < pTabList->nSrc; i++){/*����FROM�Ӿ���ʽ�б�*/

		struct SrcList_item *pItem = &pTabList->a[i];
		SelectDest dest;
		Select *pSub = pItem->pSelect;
		int isAggSub;
		if (pSub == 0) continue;/*���SELECT�ṹ��pSubΪ0�������˴�ѭ�����´�ѭ��*/
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
		**���������ʽ���ĸ߶�Parse.nHeight����ѡ��
		**��ѡ����ܰ������ʽ�����
		**(SQLITE_MAX_EXPR_DEPTH-Parse.nHeight)�߶ȡ�
		**����ܱ���һЩ,����ǿ��
		**ִ��һ����ȷ�����Ƹ�����Щ
		*/
		pParse->nHeight += sqlite3SelectExprHeight(p);

		isAggSub = (pSub->selFlags & SF_Aggregate) != 0;/*���selFlagsΪSF_Aggregate�����ۼ�������Ϣ����isAggSub*/
		if (flattenSubquery(pParse, p, i, isAgg, isAggSub)){/*�����Ӳ�ѯ*/
			/*����Ӳ�ѯ���Բ����丸��ѯ�С�*/
			if (isAggSub){/*����Ӳ�ѯ���оۼ���ѯ��Ҳ������Ϣ����*/
				isAgg = 1;/*��Ϣλ��1*/
				p->selFlags |= SF_Aggregate;
			}
			i = -1;/*��i��Ϊ-1����һ��ѭ��i��0��ʼ*/
		}
		else{
			/* Generate a subroutine that will fill an ephemeral table with
			** the content of this subquery.  pItem->addrFillSub will point
			** to the address of the generated subroutine.  pItem->regReturn
			** is a register allocated to hold the subroutine return address
			**����һ���ӳ�������ӳ���
			**�һ����ʱ������Ӳ�ѯ�����ݡ�
			**pItem - > addrFillSub��ָ�����ɵ��ӳ����ַ��
			**pItem - > regReturn��һ���Ĵ���������ӳ��򷵻ص�ַ
			**
			*/
			int topAddr;
			int onceAddr = 0;
			int retAddr;
			assert(pItem->addrFillSub == 0);/*����ϵ�*/
			pItem->regReturn = ++pParse->nMem;/*��������ڴ�ռ��С��1��ֵ��regReturn*/
			topAddr = sqlite3VdbeAddOp2(v, OP_Integer, 0, pItem->regReturn);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ����ֵ��topAddr*/
			pItem->addrFillSub = topAddr + 1;/*��ʼ��ַ��1�ٸ�ֵ��addrFillSub����Ҳ��ָ���ӳ����ַ��*/
			VdbeNoopComment((v, "materialize %s", pItem->pTab->zName));//�����ص���ʾ������Ϣ
			if (pItem->isCorrelated == 0){
				/* If the subquery is no correlated and if we are not inside of
				** a trigger, then we only need to compute the value of the subquery
				** once.
				**����Ӳ�ѯû�й�����
				**һ��������,��ô����ֻ��Ҫ����
				**�Ӳ�ѯ��ֵһ�Ρ�*/
				onceAddr = sqlite3CodeOnce(pParse);/*����һ��һ�β���ָ�Ϊ�����ռ�*/
			}
			sqlite3SelectDestInit(&dest, SRT_EphemTab, pItem->iCursor);/*��ʼ��һ��SelectDest�ṹ�����ҰѴ��������ϴ洢��SRT_EphmTab*/
			explainSetInteger(pItem->iSelectId, (u8)pParse->iNextSelectId);
			sqlite3Select(pParse, pSub, &dest);/*Ϊ�Ӳ�ѯ������ɴ���,/*ʹ�������������ѯ*/*/
			pItem->pTab->nRowEst = (unsigned)pSub->nSelectRow;
			if (onceAddr) sqlite3VdbeJumpHere(v, onceAddr);
			retAddr = sqlite3VdbeAddOp1(v, OP_Return, pItem->regReturn);
			VdbeComment((v, "end %s", pItem->pTab->zName));
			sqlite3VdbeChangeP1(v, topAddr, retAddr);
			sqlite3ClearTempRegCache(pParse);/*����Ĵ������﷨������*/
		}
		if ( /*pParse->nErr ||*/ db->mallocFailed){
			goto select_end;/*��������ڴ����,����select_end����ѯ������*/
		}
		pParse->nHeight -= sqlite3SelectExprHeight(p);/*���ر��ʽ�������߶�*/
		pTabList = p->pSrc;/*���б�*/
		if (!IgnorableOrderby(pDest)){/*������������в�����Orderby,�ͽ�SELECT�ṹ����Orderby���Ը�ֵ��pOrderBy*/
			pOrderBy = p->pOrderBy;
		}
	}
	pEList = p->pEList;/*���ṹ����ʽ�б�ֵ��pEList*/
#endif
	pWhere = p->pWhere;/*SELECT�ṹ����WHERE�Ӿ丳ֵ��pWhere*/
	pGroupBy = p->pGroupBy;/*SELECT�ṹ����GROUP BY�Ӿ丳ֵ��pGroupBy*/
	pHaving = p->pHaving;/*SELECT�ṹ����Having�Ӿ丳ֵ��pHaving*/
	isDistinct = (p->selFlags & SF_Distinct) != 0;/*�������DISTINCT�ؼ�����Ϊtrue*/

#ifndef SQLITE_OMIT_COMPOUND_SELECT
	/* If there is are a sequence of queries, do the earlier ones first.
	  **�����һϵ�еĲ�ѯ,����ǰ��ġ�
	  */
	if (p->pPrior){
		if (p->pRightmost == 0){
			Select *pLoop, *pRight = 0;
			int cnt = 0;
			int mxSelect;
			for (pLoop = p; pLoop; pLoop = pLoop->pPrior, cnt++){/*����SELECT�����Ȳ���SELECT���ҵ������Ȳ���SELECT*/
				pLoop->pRightmost = p;/*��SELECT��ֵ��������*/
				pLoop->pNext = pRight;/*����������ֵ����һ���ڵ�*/
				pRight = pLoop;/*���м��ӽڵ㸳ֵ��������*/
			}
			mxSelect = db->aLimit[SQLITE_LIMIT_COMPOUND_SELECT];/*�����ϲ�ѯ�Ĳ�ѯ����ֵ���ظ�mxSelect*/
			if (mxSelect && cnt > mxSelect){
				sqlite3ErrorMsg(pParse, "too many terms in compound SELECT");
				goto select_end;/*���mxSelect���ڣ�������ȴ���SELECT�����Ļ�����ô���﷨�������д洢too many...Select,Ȼ��������ѯ����*/
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
	**�����GROUP BY �� ORDER BY�Ӿ䣬Ȼ�����������һ�µģ���ô��ִ��GROUP BYȻ����ִ��ORDER BY.
	** ����һ���Ż���ʽ�������Ľ��û���κ�Ӱ�졣ʹ�ô�SQLITE_TESTCTRL_OPTIMIZER��SQLITE_GroupByOrder���
	** ���ճ������в����Ż���
	*/
	if (sqlite3ExprListCompare(p->pGroupBy, pOrderBy) == 0/*����������ʽֵ��ͬ*/
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
	**�����ѯ��DISTINCT������һ���ۺϲ�ѯ,
	**���ѡ���б�select-list����ORDERBY�б���һ����
	**��ô���ò�ѯ������дΪһ��GROUP BY��
	**Ҳ����˵:
	**
	** SELECT DISTINCT xyz FROM ... ORDER BY xyz
	**
	**ת��Ϊ
	**
	**SELECT xyz FROM ... GROUP BY xyz
	**
	**�ڶ�����ʽ���ã�һ��������������ʱ�����������ܴ��� ORDER BY �� DISTINCT�����д��ѯ����ʹ����ʱ����
	** ���ORDER BY �� DISTINCT������һ��������һ��������ֿ���һ����ʱ�������һ����*/
	
	if ((p->selFlags & (SF_Distinct | SF_Aggregate)) == SF_Distinct
		&& sqlite3ExprListCompare(pOrderBy, p->pEList) == 0/*���SELECT��selFlagsΪSF_Distinct��SF_Aggregate�����ұ��ʽһֱ*/
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
	**�����ORDER BY�Ӿ�������ݱ���ǰ���������������ܲ��á�����������OP_OpenEphemeralָ���ı�
	**OP_Noop��һ����������Ҫ����������addrSortIndex�������ڰ����ı䡣*/
	
	if (pOrderBy){
		KeyInfo *pKeyInfo;
		pKeyInfo = keyInfoFromExprList(pParse, pOrderBy);
		pOrderBy->iECursor = pParse->nTab++;
		p->addrOpenEphm[2] = addrSortIndex =
			sqlite3VdbeAddOp4(v, OP_OpenEphemeral,
			pOrderBy->iECursor, pOrderBy->nExpr + 2, 0,
			(char*)pKeyInfo, P4_KEYINFO_HANDOFF);  /*�����˼���ǣ��������ORDERBY�Ӿ�,��ô����һ���ؼ���Ϣ�ṹ�壬�������ʽpOrderBy�ŵ��ؼ���Ϣ�ṹ����*/
	}
	else{
		addrSortIndex = -1;/*������ô�����������ı�־��Ϊ-1*/
	}

	/* If the output is destined for a temporary table, open that table.
	  **��������ָ����һ����ʱ��ʱ����ô��Ҫ�������
	  */
	if (pDest->eDest == SRT_EphemTab){
		sqlite3VdbeAddOp2(v, OP_OpenEphemeral, pDest->iSDParm, pEList->nExpr);
	}/*�������Ķ���ΪSRT_EphemTab����ô��OP_OpenEphemeral��������vdbe��Ȼ�󷵻���������ĵ�ַ*/

	/* Set the limiter.
	   **����������
	   */
	iEnd = sqlite3VdbeMakeLabel(v);/*����һ���±�ǩ������ֵ��ֵ��iEnd*/
	p->nSelectRow = (double)LARGEST_INT64;
	computeLimitRegisters(pParse, p, iEnd);/*����iLimit��iOffset�ֶ�*/
	if (p->iLimit == 0 && addrSortIndex >= 0){
		sqlite3VdbeGetOp(v, addrSortIndex)->opcode = OP_SorterOpen;/*������OP_SorterOpen���򿪷ּ���������������������VDBE*/
		p->selFlags |= SF_UseSorter;
	}

	/* Open a virtual index to use for the distinct set.
	  **Ϊdistinct���ϴ�һ����������
	  */
	if (p->selFlags & SF_Distinct){/*���selFlags��ֵΪSF_Distinct*/
		KeyInfo *pKeyInfo;/*����һ���ؼ���Ϣ�ṹ��*/
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
	  **�ۺϺͷǾۺϲ�ѯ����ͬ��ʽ����
	  */
	if (!isAgg && pGroupBy == 0){
		ExprList *pDist = (isDistinct ? p->pEList : 0);/*���isAggû��GroupBy���ж�isDistinct�Ƿ�Ϊtrue������p->pEList��ֵ��pDist*/

		/* Begin the database scan.
		**��ʼ���ݿ�ɨ��
		*/
		pWInfo = sqlite3WhereBegin(pParse, pTabList, pWhere, &pOrderBy, pDist, 0, 0);/*��������WHERE�Ӿ䴦���ѭ����䣬�����ؽ���ָ��*/
		if (pWInfo == 0) goto select_end;/*�������ָ��Ϊ0��������ѯ����*/
		if (pWInfo->nRowOut < p->nSelectRow) p->nSelectRow = pWInfo->nRowOut;/*�������*/

		/* If sorting index that was created by a prior OP_OpenEphemeral
		** instruction ended up not being needed, then change the OP_OpenEphemeral
		** into an OP_Noop.
		**
		**����������������� OP_OpenEphemeral ָ���
		**��������Ҫ������������ôOP_OpenEphemeral ��ΪOP_Noop
		*/
		if (addrSortIndex >= 0 && pOrderBy == 0){/*��������������ڲ���ORDERBYΪ0*/
			sqlite3VdbeChangeToNoop(v, addrSortIndex);/*��addrSortIndex�в�����ΪOP_Noop*/
			p->addrOpenEphm[2] = -1;/*��SELECT�ṹ�����ʱ����±�Ϊ2��Ԫ����Ϊ-1*/
		}

		if (pWInfo->eDistinct){/*������ص�WHERE��Ϣ�к�DISTINCT��ѯ*/
			VdbeOp *pOp;                /* ������ҪOpenEphemeral������ʱ��instr*/

			assert(addrDistinctIndex >= 0);/*����ϵ㣬���addrDistinctIndexС��0���׳�������Ϣ*/
			pOp = sqlite3VdbeGetOp(v, addrDistinctIndex);/*����addrDistinctIndex ָ��Ĳ�����*/

			assert(isDistinct);/*����ϵ㣬�ж��Ƿ���DISTINCT��ѯ����û���׳�������Ϣ*/
			assert(pWInfo->eDistinct == WHERE_DISTINCT_ORDERED/*����ϵ㣬���eDistinctΪWHERE_DISTINCT_ORDERED*/
				|| pWInfo->eDistinct == WHERE_DISTINCT_UNIQUE/*����ΪWHERE_DISTINCT_UNIQUE��������׳�������Ϣ*/
				);
			distinct = -1;/*��distinct��Ϊ-1��������ȡ���ظ�����*/
			if (pWInfo->eDistinct == WHERE_DISTINCT_ORDERED){/*���WHERE������Ϣ��eDistinctΪWHERE_DISTINCT_ORDERED*/
				int iJump;
				int iExpr;
				int iFlag = ++pParse->nMem;/*���﷨�������з����ڴ�ĸ����Ӽ��ٸ�ֵ��iFlag*/
				int iBase = pParse->nMem + 1;/*���﷨�������з����ڴ�ĸ�����1�ٸ�ֵ��iBase*/
				int iBase2 = iBase + pEList->nExpr;/*����ַiBase+���ʽ�����ٸ�ֵ��iBase2*/
				pParse->nMem += (pEList->nExpr * 2);/*�����ʽ��2���Ϸ�����ڴ����ٸ�ֵ��pParse->nMem*/

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

		/* Use the standard inner loop. ʹ�ñ�׼���ڲ�ѭ��*/
		selectInnerLoop(pParse, p, pEList, 0, 0, pOrderBy, distinct, pDest,
			pWInfo->iContinue, pWInfo->iBreak);

		/* End the database scan loop.�������ݿ�ɨ��ѭ����
		*/
		sqlite3WhereEnd(pWInfo);/*����һ��whereѭ���Ľ���*/
	}
	else{
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
		if (pGroupBy){/*groupby �Ӿ�ǿ�*/
			int k;                        /* Loop counter   ѭ��������*/
			struct ExprList_item *pItem;  /* For looping over expression in a list */

			for (k = p->pEList->nExpr, pItem = p->pEList->a; k > 0; k--, pItem++){
				pItem->iAlias = 0;
			}
			for (k = pGroupBy->nExpr, pItem = pGroupBy->a; k > 0; k--, pItem++){
				pItem->iAlias = 0;
			}
			if (p->nSelectRow > (double)100) p->nSelectRow = (double)100;/*���SELECT����д���100����ֵ��SELECT������Ϊ100���޶��˴�С������100��Ϊ100*/
		}
		else{
			p->nSelectRow = (double)1;
		}


		/* Create a label to jump to when we want to abort the query ��
		**��������ȡ����ѯʱ����һ����ǩ����ת
		*/
		addrEnd = sqlite3VdbeMakeLabel(v);

		/* Convert TK_COLUMN nodes into TK_AGG_COLUMN and make entries in
		** sAggInfo for all TK_AGG_FUNCTION nodes in expressions of the
		** SELECT statement.
		*//*��TK_COLUMN�ڵ�ת��ΪTK_AGG_COLUMN������ʹ����sAggInfo�е���Ŀת��ΪSELECT�еı��ʽ��TK_AGG_FUNCTION�ڵ�*/
		memset(&sNC, 0, sizeof(sNC));/*��sNC��ǰsizeof(*sNc)���ֽ���0�滻*/
		sNC.pParse = pParse;
		sNC.pSrcList = pTabList;/*��SELECT��Դ���ϸ�ֵ�����������ĵ�Դ���ϣ�FROM�Ӿ��б�*/
		sNC.pAggInfo = &sAggInfo;
		sAggInfo.nSortingColumn = pGroupBy ? pGroupBy->nExpr + 1 : 0;
		sAggInfo.pGroupBy = pGroupBy;
		sqlite3ExprAnalyzeAggList(&sNC, pEList);/*�������ʽ�ľۺϺ��������ش�����*/
		sqlite3ExprAnalyzeAggList(&sNC, pOrderBy);
		if (pHaving){
			sqlite3ExprAnalyzeAggregates(&sNC, pHaving);/*�������ʽ�ľۺϺ���*/
		}
		sAggInfo.nAccumulator = sAggInfo.nColumn;/*���ۺ���Ϣ�������������ۼ���*/
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
		**�������GROUP BY�ľۼ�������ͬ�ڲ����ģ����Ҹ�Ϊ���ӣ�
		**�����кܴ�Ĳ�ͬ����groupby��Ҫ���ӵöࡣ
		*/
		if (pGroupBy){
			KeyInfo *pKeyInfo;  /* Keying information for the group by clause �Ӿ�groupby �Ĺؼ���Ϣ*/
			int j1;             /* A-vs-B comparision jump */
			int addrOutputRow;  /* Start of subroutine that outputs a result row ��ʼһ�����������ӳ���*/
			int regOutputRow;   /* Return address register for output subroutine  Ϊ����ӳ��򷵻ص�ַ�Ĵ���*/
			int addrSetAbort;   /* Set the abort flag and return  ������ֹ��־������*/
			int addrTopOfLoop;  /* Top of the input loop  ����ѭ���Ķ���*/
			int addrSortingIdx; /* The OP_OpenEphemeral for the sorting index */
			int addrReset;      /* Subroutine for resetting the accumulator   �����ۼ������ӳ���*/
			int regReset;       /* Return address register for reset subroutine  Ϊ�����ӳ��򷵻ص�ַ�Ĵ���*/

			/* If there is a GROUP BY clause we might need a sorting index to
			** implement it.  Allocate that sorting index now.  If it turns out
			** that we do not need it after all, the OP_SorterOpen instruction
			** will be converted into a Noop.
			**������ע����ʵ�ַ��飬�Ƚ������������һ��GROUP BY�Ӿ䣬���ǿ�����Ҫһ����������ȥʵ������
		    ** ���������������������һ�������Ѿ�ʵ�ֵģ����OP_SorterOpenָ���תΪNoop*/
			*/
			sAggInfo.sortingIdx = pParse->nTab++;
			pKeyInfo = keyInfoFromExprList(pParse, pGroupBy);/*�����ʽ�б��е�pGroupBy��ȡ�ؼ���Ϣ��pKeyInfo*/
			addrSortingIdx = sqlite3VdbeAddOp4(v, OP_SorterOpen,
				sAggInfo.sortingIdx, sAggInfo.nSortingColumn,
				0, (char*)pKeyInfo, P4_KEYINFO_HANDOFF);

			/* Initialize memory locations used by GROUP BY aggregate processing
			**��ʼ����groupby �ۺϴ�����ڴ浥Ԫ
			*/
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
			VdbeComment((v, "clear abort flag"));;/*����clear abort flag�����뵽VDBE��*/
			sqlite3VdbeAddOp2(v, OP_Integer, 0, iUseFlag);/*��OP_Integer��������vdbe��Ȼ�󷵻���������ĵ�ַ*/
			VdbeComment((v, "indicate accumulator empty"));/*����indicate accumulator empty"���뵽VDBE��*/
			sqlite3VdbeAddOp3(v, OP_Null, 0, iAMem, iAMem + pGroupBy->nExpr - 1);/*��OP_Null��������vdbe��Ȼ�󷵻���������ĵ�ַ*/


			/* Begin a loop that will extract all source rows in GROUP BY order.
			** This might involve two separate loops with an OP_Sort in between, or
			** it might be a single loop that uses an index to extract information
			** in the right order to begin with.
			**����һ��ѭ������ȡGROUP BY��������е�ԭ�С�
			*/
			sqlite3VdbeAddOp2(v, OP_Gosub, regReset, addrReset);
			pWInfo = sqlite3WhereBegin(pParse, pTabList, pWhere, &pGroupBy, 0, 0, 0);
			if (pWInfo == 0) goto select_end;
			if (pGroupBy == 0){
				/* The optimizer is able to deliver rows in group by order so
				** we do not have to sort.  The OP_OpenEphemeral table will be
				** cancelled later because we still need to use the pKeyInfo
				**
				**�Ż����ܹ����ͳ�GROUP BY��������ٽ�������
				**�����ʱ��OP_OpenEphemeral���ᱻȡ������Ϊ����ʹ��pKeyInfo
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
				**��ȷ����˳������С�������Ҫ��ÿһ�����뵽����������
				**��ֹ��һ��ѭ����Ȼ�������������Ϊ�˵ó����������
				*/
				int regBase;/*��ַ�Ĵ���*/
				int regRecord;/*�Ĵ����м�¼*/
				int nCol;/*����*/
				int nGroupBy;/*GROUP BY�ĸ���*/

				explainTempTable(pParse,
					isDistinct && !(p->selFlags&SF_Distinct) ? "DISTINCT" : "GROUP BY");/*ִ�г���Ż�ʹ�øú��������������Ϣ���﷨��������*/


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
		  *//*���㵱ǰGROUP BY������Ҵ洢��b0,b1,b2����b0���ڴ��ַΪiBMem+0��b1���ڴ��ַΪiBMem+1���������ƣ�
		  **Ȼ�󽫵�ǰGROUP BY��������洢��a0,a1,a2����ǰ���е�the GROUP BY ������*/
			addrTopOfLoop = sqlite3VdbeCurrentAddr(v);/*������һ���������ָ��ĵ�ַ*/
			sqlite3ExprCacheClear(pParse);/*��������л�����Ŀ*/
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
		  ** ��һ���ӳ�����������group-by�ۼ���
		  ** ����������Ԫ�������ֹ��ѯ������ӳ������ڷ���֮ǰ������iAbortFlag�ڴ�Ϊ����ֹ�źŵ����ߡ�*/
			
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
			**���ڵ�ǰ���ݵĵ�ǰ�У����¾ۺ��ۼ���
			*/
			sqlite3VdbeJumpHere(v, j1);
			updateAccumulator(pParse, &sAggInfo);
			sqlite3VdbeAddOp2(v, OP_Integer, 1, iUseFlag);
			VdbeComment((v, "indicate data in accumulator"));

			/* End of the loop   ѭ����β
			*/
			if (groupBySort){
				sqlite3VdbeAddOp2(v, OP_SorterNext, sAggInfo.sortingIdx, addrTopOfLoop);
			}
			else{
				sqlite3WhereEnd(pWInfo);
				sqlite3VdbeChangeToNoop(v, addrSortingIdx);
			}

			/* Output the final row of result   �����������һ��
			*/
			sqlite3VdbeAddOp2(v, OP_Gosub, regOutputRow, addrOutputRow);
			VdbeComment((v, "output final row"));

			/* Jump over the subroutines   �����ӳ���
			*/
			sqlite3VdbeAddOp2(v, OP_Goto, 0, addrEnd);

			/* Generate a subroutine that outputs a single row of the result
			** set.  This subroutine first looks at the iUseFlag.  If iUseFlag
			** is less than or equal to zero, the subroutine is a no-op.  If
			** the processing calls for the query to abort, this subroutine
			** increments the iAbortFlag memory location before returning in
			** order to signal the caller to abort.
			**
			**����һ�����������ĵ�һ�е��ӳ���
			**����ӳ������ȼ��iUseFlag
			**���iUseFlag��0 С���ߵ���0 ����ô�ӳ���
			**���κβ���������Բ�ѯ�Ĵ��������ֹ��
			**��ô���ӳ����ڷ���֮ǰ����iAbortFlag ���ڴ浥Ԫ
			**��������Ϊ�˸��ߵ�������ֹ����
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
			**����һ���ӳ�������ӳ�������groupby �ۼ���
			*/
			sqlite3VdbeResolveLabel(v, addrReset);
			resetAccumulator(pParse, &sAggInfo);
			sqlite3VdbeAddOp1(v, OP_Return, regReset);

		} /* endif pGroupBy.  Begin aggregate queries without GROUP BY:
											   **��ʼһ����groupby�ľۺϲ�ѯ
											   */
		else {
			ExprList *pDel = 0;/*����һ�����ʽ�б�*/
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
				/* ���isSimpleCount() ����һ��ָ��TABLE�ṹ��ָ�룬Ȼ��SQL����ʽ��
			   **	SELECT count(*) FROM <tbl>
			   ** 
			   ** Table�ṹ���ص�table<tbl>���£�
			   ** ������ܳ������ʽ��������Ż��ˡ����OP_Countָ��ִ����intkey���������ݵ�table<tbl>������������
			   ** ���ȽϺõ�ִ��op��������������Ƶ�����ݱ����һ�£����������Ӧ�ı����ٵ�ҳ��
			   */
				const int iDb = sqlite3SchemaToIndex(pParse->db, pTab->pSchema);/*Ϊ��ģʽ��������������ֵ��iDb*/
				const int iCsr = pParse->nTab++;     /*  ɨ��b-tree ��ָ��*/
				Index *pIdx;                         /* Iterator variable ��������*/
				KeyInfo *pKeyInfo = 0;               /* Keyinfo for scanned index ��ɨ�������Ĺؼ���Ϣ*/
				Index *pBest = 0;                    /* Best index found so far ��ĿǰΪֹ�ҵ�����õ�����*/
				int iRoot = pTab->tnum;              /* Root page of scanned b-tree ɨ���b-tree �ĸ�ҳ*/

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
				**�������ٳ��ֵ��е���������������и����������бȽ��ٵ��У�Ȼ�����Ǽ����������˽�С�Ŀռ���Ӳ���ϲ���
			    ** ɨ���Ѿ�ȷ����ѯ��������Ƚϵ͡�����������£�����iRoot��b-tree�����ĸ�ҳ�Ų���pKeyInfo��KeyInfo�ṹ����Ҫ�ĵ�������*/
			
				for (pIdx = pTab->pIndex; pIdx; pIdx = pIdx->pNext){/*��������*/
					if (pIdx->bUnordered == 0 && (!pBest || pIdx->nColumn < pBest->nColumn)){/*�������û��������û����õ��������������е���С������������е���*/
						pBest = pIdx;
					}
				}
				if (pBest && pBest->nColumn < pTab->nCol){
					iRoot = pBest->tnum;/*����������и�������ֵ��iRoot*/
					pKeyInfo = sqlite3IndexKeyinfo(pParse, pBest);/*�������������Ϣ��ȡ���ؼ���Ϣ�ṹ��pKeyInfo��*/
				}

				/* Open a read-only cursor, execute the OP_Count, close the cursor.
					 **
					 **  ��ֻ���αִ꣬��OP_Count�������ر��α�
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
				if (flag){/*���flag����*/
					assert(!ExprHasProperty(p->pEList->a[0].pExpr, EP_xIsSelect));/*����ϵ㣬���p->pEList->a[0].pExpr�а���EP_xIsSelect���Բ�Ϊ�գ��׳�������Ϣ*/
					pMinMax = sqlite3ExprListDup(db, p->pEList->a[0].pExpr->x.pList, 0);/*����ϵ㣬���p->pEList->a[0].pExpr�а���EP_xIsSelect���Բ�Ϊ�գ�
					                                                                      �׳�������Ϣ*/
					pDel = pMinMax;/*����ѯ�����ֵ��pDel*/
					if (pMinMax && !db->mallocFailed){/*���pMinMax���ڲ��ҷ����ڴ�ɹ�*/
						pMinMax->a[0].sortOrder = flag != WHERE_ORDERBY_MIN ? 1 : 0;/*���flagΪWHERE_ORDERBY_MIN��1��ֵ�������ǣ�����ֵ0*/
						pMinMax->a[0].pExpr->op = TK_COLUMN;/*�����ʽ�в�������ֵΪTK_COLUMN*/
					}
				}

				/* This case runs if the aggregate has no GROUP BY clause.  The
				** processing is much simpler since there is only a single row
				** of output.
				**
				**����ۼ�������û�� GROUP BY���������������ܼ�
				**ֻ��һ���������������
				*/
				resetAccumulator(pParse, &sAggInfo);/*���þۺ��ۼ���*/
				pWInfo = sqlite3WhereBegin(pParse, pTabList, pWhere, &pMinMax, 0, flag, 0);/*���ɴ���where�Ӿ��ѭ���Ŀ�ʼ*/
				if (pWInfo == 0){/*��Ϊ�գ���ɾ��������select*/
					sqlite3ExprListDelete(db, pDel);/*ɾ��ִ����ֵ�ı��ʽ�б�*/
					goto select_end;/*������ѯ����*/
				}
				updateAccumulator(pParse, &sAggInfo);/*�����ۼ����ڴ浥Ԫ*//*Ϊ��ǰ�α�λ���ϵľۼ������������ۼ����ڴ浥Ԫ*/
				if (!pMinMax && flag){/*�����ֵ�ı��ʽΪ�ղ���flag��ǲ�Ϊ��*/
					sqlite3VdbeAddOp2(v, OP_Goto, 0, pWInfo->iBreak);/*��OP_Goto��������vdbe������ pWInfo->iBreakִ����䣬Ȼ�󷵻���������ĵ�ַ*/
					VdbeComment((v, "%s() by index",
						(flag == WHERE_ORDERBY_MIN ? "min" : "max")));/*��"%s() by index"���뵽VDBE�з��뵽VDBE��,����flag=WHERE_ORDERBY_MIN��%sΪmin������Ϊmax*/
				}
				sqlite3WhereEnd(pWInfo);/*����where ѭ��*/
				finalizeAggFunctions(pParse, &sAggInfo);/*�����ۺϺ���*/
			}

			pOrderBy = 0;
			sqlite3ExprIfFalse(pParse, pHaving, addrEnd, SQLITE_JUMPIFNULL);/*���Ϊtrue����ִ�У����ΪFALSE����addrEnd*/
			selectInnerLoop(pParse, p, p->pEList, 0, 0, 0, -1,
				pDest, addrEnd, addrEnd);/*���ݱ��ʽp->pEList���������ӣ���������addrEnd��pDest��������*/
			sqlite3ExprListDelete(db, pDel);/*ɾ��ִ����ֵ�ı��ʽ�б�*/
		}
		sqlite3VdbeResolveLabel(v, addrEnd);

	} /* endif aggregate query *//*����Ǿۼ���ѯ*/

	if (distinct >= 0){
		explainTempTable(pParse, "DISTINCT");/*ȡ���ظ����ʽ��ֵ���ڵ���0��ִ�г���Ż�ʹ�øú��������������Ϣ"DISTINCT"���﷨������*/
	}

	/* If there is an ORDER BY clause, then we need to sort the results
	** and send them to the callback one by one.
	**
	**�������һ��ORDERBY�Ӿ䣬
	**������Ҫ���������ҷ��ͽ��һ����һ���ĸ��ص�����
	*/
	if (pOrderBy){
		explainTempTable(pParse, "ORDER BY");
		generateSortTail(pParse, p, v, pEList->nExpr, pDest);/*���������������ORDER BY���*/
	}

	/* Jump here to skip this query
	  **Ϊ��������ѯ��ֱ����ת�����
	  */
	sqlite3VdbeResolveLabel(v, iEnd);/*����iEnd������Ϊ��һ��������ĵ�ַ*/

	/* The SELECT was successfully coded.   Set the return code to 0
	** to indicate no errors.
	**
	**  select ��䱻�ɹ����룬�����������Ϊ0������û�г���
	*/
	rc = 0;/*��ִ�н�����Ϊ0*/
	/* Control jumps to here if an error is encountered above, or upon
	** successful coding of the SELECT.
	**������Ծ���������������
	**  ��һ������,���select�ɹ����롣
	*/
select_end:
	explainSetInteger(pParse->iSelectId, iRestoreSelectId);

	/* Identify column names if results of the SELECT are to be output.
	**���select �����Ҫ���������������
	*/
	if (rc == SQLITE_OK && pDest->eDest == SRT_Output){
		generateColumnNames(pParse, pTabList, pEList);/*��������*/
	}

	sqlite3DbFree(db, sAggInfo.aCol);/*�ͷ����ݿ������д洢�ۼ�������Ϣ���е��ڴ�*/
	sqlite3DbFree(db, sAggInfo.aFunc);/*�ͷ����ݿ������д洢�ۼ�������Ϣ�ľۼ����������ڴ�*/
	return rc;/*����ִ�н������*/
}

#if defined(SQLITE_ENABLE_TREE_EXPLAIN)
/*
** Generate a human-readable description of a the Select object.
**����һ���ɶ���select ���������
*/
static void explainOneSelect(Vdbe *pVdbe, Select *p){
	sqlite3ExplainPrintf(pVdbe, "SELECT ");/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����"SELECT "*/
	if (p->selFlags & (SF_Distinct | SF_Aggregate)){/*��Distinct ����Aggregate*/
		if (p->selFlags & SF_Distinct){/*��Distinct *//*���selFlagsΪSF_Distinct*/
			sqlite3ExplainPrintf(pVdbe, "DISTINCT ");/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����"DISTINCT"*/
		}
		if (p->selFlags & SF_Aggregate){/*��Aggregate *//*���selFlagsΪSF_Aggregate*/
			sqlite3ExplainPrintf(pVdbe, "agg_flag ");/*ʵ���ϵ���sqlite3VXPrintf���������и�ʽ�����"agg_flag "*/
		}
		sqlite3ExplainNL(pVdbe);/*������'|n' *//*���һ�����з���'\n',ǰ���������βû�У�*/
		sqlite3ExplainPrintf(pVdbe, "   ");/*����FROM�Ӿ���ʽ�б�*/
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
		p = p->pNext;/*��p����������ֵ������ǰp*/
		if( p==0 ) break;/*�Ѿ�ѭ�������һ���ˣ�û�����ӽڵ�*/
		sqlite3ExplainNL(pVdbe);/*���һ�����з���'\n',ǰ���������βû�У�*/
		sqlite3ExplainPrintf(pVdbe, "%s\n", selectOpName(p->op));/*ʵ�����ǵ���sqlite3VXPrintf�����������и�ʽ�����Ϊ"%s\n"*/
	  }
	  sqlite3ExplainPrintf(pVdbe, "END");/*ʵ�����ǵ���sqlite3VXPrintf���������и�ʽ��������Ϊ"END"*/
	  sqlite3ExplainPop(pVdbe);/*�Ըղ�ѹ��ջ������������е���*/
	}
	/* End of the structure debug printing code ��������
	*****************************************************************************/
	#endif /* defined(SQLITE_ENABLE_TREE_EXPLAIN) */
