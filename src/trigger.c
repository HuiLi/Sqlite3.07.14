/*
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file contains the implementation for TRIGGERs
** 这个文件内容是实现触发器（TRIGGERs）
*/
#include "sqliteInt.h"

#ifndef SQLITE_OMIT_TRIGGER
/*
** Delete a linked list of TriggerStep structures.
** 删除TriggerStep结构体的链表
*/
void sqlite3DeleteTriggerStep(sqlite3 *db, TriggerStep *pTriggerStep){
	while (pTriggerStep){
		TriggerStep * pTmp = pTriggerStep;
		pTriggerStep = pTriggerStep->pNext;

		sqlite3ExprDelete(db, pTmp->pWhere);
		sqlite3ExprListDelete(db, pTmp->pExprList);
		sqlite3SelectDelete(db, pTmp->pSelect);
		sqlite3IdListDelete(db, pTmp->pIdList);

		sqlite3DbFree(db, pTmp);
	}
}

/*
** Given table pTab, return a list of all the triggers attached to
** the table. The list is connected by Trigger.pNext pointers.
**
** All of the triggers on pTab that are in the same database as pTab
** are already attached to pTab->pTrigger.  But there might be additional
** triggers on pTab in the TEMP schema.  This routine prepends all
** TEMP triggers on pTab to the beginning of the pTab->pTrigger list
** and returns the combined list.
**
** To state it another way:  This routine returns a list of all triggers
** that fire off of pTab.  The list will include any TEMP triggers on
** pTab as well as the triggers lised in pTab->pTrigger.
**
**给定pTab表，返回所有属于这个表的触发器的列表。这个列表与触发器的pNext指针链接。
**所有pTab上的触发器与已经附在pTab - > pTrigger的pTab在相同的数据库。但可能有额外的pTab触发器在临时模式。这个例程预先考虑所有临时在pTab - > pTrigger列表的开头的pTab的触发器,并返回组合的列表。
**为了规定其他方式：:这个例程返回一个所有pTab触发的pTab上的触发器列表。列表将包含任何临时pTab上的触发器，和pTab - > pTrigger已经列出来的触发器一样。
*/
Trigger *sqlite3TriggerList(Parse *pParse, Table *pTab){
	Schema * const pTmpSchema = pParse->db->aDb[1].pSchema;
	Trigger *pList = 0;                  /* List of triggers to return */

	if (pParse->disableTriggers){
		return 0;
	}

	if (pTmpSchema != pTab->pSchema){
		HashElem *p;
		assert(sqlite3SchemaMutexHeld(pParse->db, 0, pTmpSchema));
		for (p = sqliteHashFirst(&pTmpSchema->trigHash); p; p = sqliteHashNext(p)){
			Trigger *pTrig = (Trigger *)sqliteHashData(p);
			if (pTrig->pTabSchema == pTab->pSchema
				&& 0 == sqlite3StrICmp(pTrig->table, pTab->zName)
				){
				pTrig->pNext = (pList ? pList : pTab->pTrigger);
				pList = pTrig;
			}
		}
	}

	return (pList ? pList : pTab->pTrigger);
}

/*
** This is called by the parser when it sees a CREATE TRIGGER statement
** up to the point of the BEGIN before the trigger actions.  A Trigger
** structure is generated based on the information available and stored
** in pParse->pNewTrigger.  After the trigger actions have been parsed, the
** sqlite3FinishTrigger() function is called to complete the trigger
** construction process.
**这被语法分析器调用，在触发器有动作之前，当他遇到 CREATE TRIGGER 语句的BEGIN的点。基于有效信息一个触发器语法被生成并存储在pParse - > pNewTrigger里。
**在触发器的动作已经完成之后,sqlite3FinishTrigger()函数被调用来完成触发器的处理过程。
**
*/
void sqlite3BeginTrigger(
	Parse *pParse,      /* The parse context of the CREATE TRIGGER statement// CREATE TRIGGER语句的语法分析内容 */
	Token *pName1,      /* The name of the trigger 触发器的名字1*/
	Token *pName2,      /* The name of the trigger 触发器的名字2*/
	int tr_tm,          /* One of TK_BEFORE, TK_AFTER, TK_INSTEAD //TK_BEFORE, TK_AFTER, TK_INSTEAD中的一个 */
	int op,             /* One of TK_INSERT, TK_UPDATE, TK_DELETE //TK_INSERT, TK_UPDATE, TK_DELETE中的一个*/
	IdList *pColumns,   /* column list if this is an UPDATE OF trigger 更新触发器的行列表*/
	SrcList *pTableName,/* The name of the table/view the trigger applies to 触发器申请的表和视图的名字 */
	Expr *pWhen,        /* WHEN clause //WHEN子句 */
	int isTemp,         /* True if the TEMPORARY keyword is present //如果TEMPORARY的关键字存在则为true  */
	int noErr           /* Suppress errors if the trigger already exists 如果触发器已经存在则忽略错误  */
	){
	Trigger *pTrigger = 0;  /* The new trigger 新的触发器 */
	Table *pTab;            /* Table that the trigger fires off of 触发器触发的表 */
	char *zName = 0;        /* Name of the trigger 触发器名 */
	sqlite3 *db = pParse->db;  /* The database connection 数据库连接 */
	int iDb;                /* The database to store the trigger in 存储触发器的数据库 */
	Token *pName;           /* The unqualified db name 未经限定的数据库名字 */
	DbFixer sFix;           /* State vector for the DB fixer  数据库固定器的状态向量*/
	int iTabDb;             /* Index of the database holding pTab 数据库保存的pTab的索引 */

	assert(pName1 != 0);   /* pName1->z might be NULL, but not pName1 itself//pName1->z might 可能是空，但是不是pName1本身 */
	assert(pName2 != 0);
	assert(op == TK_INSERT || op == TK_UPDATE || op == TK_DELETE);
	assert(op > 0 && op<0xff);
	if (isTemp){
		/* If TEMP was specified, then the trigger name may not be qualified. 如果TEMP被指定,那么触发器名称可能不合格。 */
		if (pName2->n>0){
			sqlite3ErrorMsg(pParse, "temporary trigger may not have qualified name");
			goto trigger_cleanup;
		}
		iDb = 1;
		pName = pName1;
	}
	else{
		/* Figure out the db that the trigger will be created in 计算出触发器将会创建在之中的数据库 */
		iDb = sqlite3TwoPartName(pParse, pName1, pName2, &pName);
		if (iDb < 0){
			goto trigger_cleanup;
		}
	}
	if (!pTableName || db->mallocFailed){
		goto trigger_cleanup;
	}

	/* A long-standing parser bug is that this syntax was allowed: 一个长期存在的语法是允许的解析器错误:
	**
	**    CREATE TRIGGER attached.demo AFTER INSERT ON attached.tab ....
	**                                                 ^^^^^^^^
	**
	** To maintain backwards compatibility, ignore the database
	** name on pTableName if we are reparsing our of SQLITE_MASTER.向后保持兼容性,如果我们用SQLITE_MASTER 重新解析，则忽略pTableName的数据库名称。
	*/
	if (db->init.busy && iDb != 1){
		sqlite3DbFree(db, pTableName->a[0].zDatabase);
		pTableName->a[0].zDatabase = 0;
	}

	/* If the trigger name was unqualified, and the table is a temp table,
	** then set iDb to 1 to create the trigger in the temporary database.
	** If sqlite3SrcListLookup() returns 0, indicating the table does not
	** exist, the error is caught by the block below.如果触发器名不合格,而且表是一个临时表,那么设置iDb 的值为 1来创建临时数据库中触发器。如果sqlite3SrcListLookup()返回0,表明表不存在,错误将会被下块捕捉。
	*/
	pTab = sqlite3SrcListLookup(pParse, pTableName);
	if (db->init.busy == 0 && pName2->n == 0 && pTab
		&& pTab->pSchema == db->aDb[1].pSchema){
		iDb = 1;
	}

	/* Ensure the table name matches database name and that the table exists 确保表的表名匹配数据库名称并且表存在*/
	if (db->mallocFailed) goto trigger_cleanup;
	assert(pTableName->nSrc == 1);
	if (sqlite3FixInit(&sFix, pParse, iDb, "trigger", pName) &&
		sqlite3FixSrcList(&sFix, pTableName)){
		goto trigger_cleanup;
	}
	pTab = sqlite3SrcListLookup(pParse, pTableName);
	if (!pTab){
		/* The table does not exist. 表不存在 */
		if (db->init.iDb == 1){
			/* Ticket #3810.
			** Normally, whenever a table is dropped, all associated triggers are
			** dropped too.  But if a TEMP trigger is created on a non-TEMP table
			** and the table is dropped by a different database connection, the
			** trigger is not visible to the database connection that does the
			** drop so the trigger cannot be dropped.  This results in an
			** "orphaned trigger" - a trigger whose associated table is missing.
			**通常,当一个表被删除时,所有相关的触发器也终止了。但如果创建一个了临时的non-TEMP表触发器并且表被一个不同的数据库连接终止了，触发器不能发现数据库连接是不是被终止，所以触发器不能被删除。这将导致一个“orphaned trigger”一个与表相关的触发器不见了。
			*/
			db->init.orphanTrigger = 1;
		}
		goto trigger_cleanup;
	}

	if (IsVirtual(pTab)){
		sqlite3ErrorMsg(pParse, "cannot create triggers on virtual tables");
		goto trigger_cleanup;
	}

	/* Check that the trigger name is not reserved and that no trigger of the
	** specified name exists 检查触发器名是不是保留的,或者没有与其他触发器名称重复* /*/
	zName = sqlite3NameFromToken(db, pName);
	if (!zName || SQLITE_OK != sqlite3CheckObjectName(pParse, zName)){
		goto trigger_cleanup;
	}
	assert(sqlite3SchemaMutexHeld(db, iDb, 0));
	if (sqlite3HashFind(&(db->aDb[iDb].pSchema->trigHash),
		zName, sqlite3Strlen30(zName))){
		if (!noErr){
			sqlite3ErrorMsg(pParse, "trigger %T already exists", pName);
		}
		else{
			assert(!db->init.busy);
			sqlite3CodeVerifySchema(pParse, iDb);
		}
		goto trigger_cleanup;
	}

	/* Do not create a trigger on a system table 不能再系统表中创建触发器*/
	if (sqlite3StrNICmp(pTab->zName, "sqlite_", 7) == 0){
		sqlite3ErrorMsg(pParse, "cannot create trigger on system table");
		pParse->nErr++;
		goto trigger_cleanup;
	}

	/* INSTEAD of triggers are only for views and views only support INSTEAD
	** of triggers. 触发器的 INSTEAD语句只有对于视图，并且视图只支持触发器的 INSTEAD语句
	*/
	if (pTab->pSelect && tr_tm != TK_INSTEAD){
		sqlite3ErrorMsg(pParse, "cannot create %s trigger on view: %S",
			(tr_tm == TK_BEFORE) ? "BEFORE" : "AFTER", pTableName, 0);
		goto trigger_cleanup;
	}
	if (!pTab->pSelect && tr_tm == TK_INSTEAD){
		sqlite3ErrorMsg(pParse, "cannot create INSTEAD OF"
			" trigger on table: %S", pTableName, 0);
		goto trigger_cleanup;
	}
	iTabDb = sqlite3SchemaToIndex(db, pTab->pSchema);

#ifndef SQLITE_OMIT_AUTHORIZATION
	{
		int code = SQLITE_CREATE_TRIGGER;
		const char *zDb = db->aDb[iTabDb].zName;
		const char *zDbTrig = isTemp ? db->aDb[1].zName : zDb;
		if (iTabDb == 1 || isTemp) code = SQLITE_CREATE_TEMP_TRIGGER;
		if (sqlite3AuthCheck(pParse, code, zName, pTab->zName, zDbTrig)){
			goto trigger_cleanup;
		}
		if (sqlite3AuthCheck(pParse, SQLITE_INSERT, SCHEMA_TABLE(iTabDb), 0, zDb)){
			goto trigger_cleanup;
		}
	}
#endif

	/* INSTEAD OF triggers can only appear on views and BEFORE triggers
	** cannot appear on views.  So we might as well translate every
	** INSTEAD OF trigger into a BEFORE trigger.  It simplifies code
	** elsewhere.  触发器的 INSTEAD语句只能在视图出现，并且触发器的 BEFORE语句不能出现在视图。我们应该把每一个触发器的 INSTEAD语句翻译成一个触发器的 BEFORE语句。它简化了代码
	*/
	if (tr_tm == TK_INSTEAD){
		tr_tm = TK_BEFORE;
	}

	/* Build the Trigger object 构建触发器对象 */
	pTrigger = (Trigger*)sqlite3DbMallocZero(db, sizeof(Trigger));
	if (pTrigger == 0) goto trigger_cleanup;
	pTrigger->zName = zName;
	zName = 0;
	pTrigger->table = sqlite3DbStrDup(db, pTableName->a[0].zName);
	pTrigger->pSchema = db->aDb[iDb].pSchema;
	pTrigger->pTabSchema = pTab->pSchema;
	pTrigger->op = (u8)op;
	pTrigger->tr_tm = tr_tm == TK_BEFORE ? TRIGGER_BEFORE : TRIGGER_AFTER;
	pTrigger->pWhen = sqlite3ExprDup(db, pWhen, EXPRDUP_REDUCE);
	pTrigger->pColumns = sqlite3IdListDup(db, pColumns);
	assert(pParse->pNewTrigger == 0);
	pParse->pNewTrigger = pTrigger;
	//清除触发器
trigger_cleanup:
	sqlite3DbFree(db, zName);
	sqlite3SrcListDelete(db, pTableName);
	sqlite3IdListDelete(db, pColumns);
	sqlite3ExprDelete(db, pWhen);
	if (!pParse->pNewTrigger){
		sqlite3DeleteTrigger(db, pTrigger);
	}
	else{
		assert(pParse->pNewTrigger == pTrigger);
	}
}

/*
** This routine is called after all of the trigger actions have been parsed
** in order to complete the process of building the trigger. 触发器的操作被全部语法分析之后这个例程被调用,去完成触发器构建过程。
*/
void sqlite3FinishTrigger(
	Parse *pParse,          /* Parser context 语法解析环境 */
	TriggerStep *pStepList, /* The triggered program  触发进程*/
	Token *pAll             /* Token that describes the complete CREATE TRIGGER 描述CREATE TRIGGER完成的标记 */
	){
	Trigger *pTrig = pParse->pNewTrigger;   /* Trigger being finished 触发器被创建  */
	char *zName;                            /* Name of trigger 触发器名字 */
	sqlite3 *db = pParse->db;               /* The database 数据库 */
	DbFixer sFix;                           /* Fixer object 固定对象 */
	int iDb;                                /* Database containing the trigger 数据库包含的触发器 */
	Token nameToken;                        /* Trigger name for error reporting 错误报告的触发器名字*/

	pParse->pNewTrigger = 0;
	if (NEVER(pParse->nErr) || !pTrig) goto triggerfinish_cleanup;
	zName = pTrig->zName;
	iDb = sqlite3SchemaToIndex(pParse->db, pTrig->pSchema);
	pTrig->step_list = pStepList;
	while (pStepList){
		pStepList->pTrig = pTrig;
		pStepList = pStepList->pNext;
	}
	nameToken.z = pTrig->zName;
	nameToken.n = sqlite3Strlen30(nameToken.z);
	if (sqlite3FixInit(&sFix, pParse, iDb, "trigger", &nameToken)
		&& sqlite3FixTriggerStep(&sFix, pTrig->step_list)){
		goto triggerfinish_cleanup;
	}

	/* if we are not initializing,
	** build the sqlite_master entry   如果我们没有初始化，则创建sqlite_master入口
	*/
	if (!db->init.busy){
		Vdbe *v;
		char *z;

		/* Make an entry in the sqlite_master table 在sqlite_master表中创建一个入口*/
		v = sqlite3GetVdbe(pParse);
		if (v == 0) goto triggerfinish_cleanup;
		sqlite3BeginWriteOperation(pParse, 0, iDb);
		z = sqlite3DbStrNDup(db, (char*)pAll->z, pAll->n);
		sqlite3NestedParse(pParse,
			"INSERT INTO %Q.%s VALUES('trigger',%Q,%Q,0,'CREATE TRIGGER %q')",
			db->aDb[iDb].zName, SCHEMA_TABLE(iDb), zName,
			pTrig->table, z);
		sqlite3DbFree(db, z);
		sqlite3ChangeCookie(pParse, iDb);
		sqlite3VdbeAddParseSchemaOp(v, iDb,
			sqlite3MPrintf(db, "type='trigger' AND name='%q'", zName));
	}

	if (db->init.busy){
		Trigger *pLink = pTrig;
		Hash *pHash = &db->aDb[iDb].pSchema->trigHash;
		assert(sqlite3SchemaMutexHeld(db, iDb, 0));
		pTrig = sqlite3HashInsert(pHash, zName, sqlite3Strlen30(zName), pTrig);
		if (pTrig){
			db->mallocFailed = 1;
		}
		else if (pLink->pSchema == pLink->pTabSchema){
			Table *pTab;
			int n = sqlite3Strlen30(pLink->table);
			pTab = sqlite3HashFind(&pLink->pTabSchema->tblHash, pLink->table, n);
			assert(pTab != 0);
			pLink->pNext = pTab->pTrigger;
			pTab->pTrigger = pLink;
		}
	}

triggerfinish_cleanup:
	sqlite3DeleteTrigger(db, pTrig);
	assert(!pParse->pNewTrigger);
	sqlite3DeleteTriggerStep(db, pStepList);
}

/*
** Turn a SELECT statement (that the pSelect parameter points to) into
** a trigger step.  Return a pointer to a TriggerStep structure.
**把一个SELECT语句(pSelect参数指向的)装入触发器的执行步骤。返回一个TriggerStep结构指针。
** The parser calls this routine when it finds a SELECT statement in
** body of a TRIGGER.  解析调用这个程序，当发现在TRIGGER语句中有SELECT语句
*/
TriggerStep *sqlite3TriggerSelectStep(sqlite3 *db, Select *pSelect){
	TriggerStep *pTriggerStep = sqlite3DbMallocZero(db, sizeof(TriggerStep));
	if (pTriggerStep == 0) {
		sqlite3SelectDelete(db, pSelect);
		return 0;
	}
	pTriggerStep->op = TK_SELECT;
	pTriggerStep->pSelect = pSelect;
	pTriggerStep->orconf = OE_Default;
	return pTriggerStep;
}

/*
** Allocate space to hold a new trigger step.  The allocated space
** holds both the TriggerStep object and the TriggerStep.target.z string.
**分配空间来保存一个新的触发器步骤。分配的空间拥有TriggerStep对象和TriggerStep.target.z字符串。
** If an OOM error occurs, NULL is returned and db->mallocFailed is set. 如果内存溢出，返回null，并且db - > mallocFailed被设置。
*/
static TriggerStep *triggerStepAllocate(
	sqlite3 *db,                /* Database connection */
	u8 op,                      /* Trigger opcode 触发器操作码 */
	Token *pName                /* The target name */
	){
	TriggerStep *pTriggerStep;

	pTriggerStep = sqlite3DbMallocZero(db, sizeof(TriggerStep)+pName->n);
	if (pTriggerStep){
		char *z = (char*)&pTriggerStep[1];
		memcpy(z, pName->z, pName->n);
		pTriggerStep->target.z = z;
		pTriggerStep->target.n = pName->n;
		pTriggerStep->op = op;
	}
	return pTriggerStep;
}

/*
** Build a trigger step out of an INSERT statement.  Return a pointer
** to the new trigger step. 创建一个INSERT语句的触发器步骤。返回一个新触发器步骤的指正。
**
** The parser calls this routine when it sees an INSERT inside the
** body of a trigger. 解析调用这个程序，当发现在触发器中有INSERT语句
*/
TriggerStep *sqlite3TriggerInsertStep(
	sqlite3 *db,        /* The database connection */
	Token *pTableName,  /* Name of the table into which we insert 插入的表的名字 */
	IdList *pColumn,    /* List of columns in pTableName to insert into 出入的pTableName表的行列表 */
	ExprList *pEList,   /* The VALUE clause: a list of values to be inserted ///VALUE子句：被插入列表的值 */
	Select *pSelect,    /* A SELECT statement that supplies values 提供值的SELECT语句 */
	u8 orconf           /* The conflict algorithm (OE_Abort, OE_Replace, etc.) 冲突的算法 */
	){
	TriggerStep *pTriggerStep;

	assert(pEList == 0 || pSelect == 0);
	assert(pEList != 0 || pSelect != 0 || db->mallocFailed);

	pTriggerStep = triggerStepAllocate(db, TK_INSERT, pTableName);
	if (pTriggerStep){
		pTriggerStep->pSelect = sqlite3SelectDup(db, pSelect, EXPRDUP_REDUCE);
		pTriggerStep->pIdList = pColumn;
		pTriggerStep->pExprList = sqlite3ExprListDup(db, pEList, EXPRDUP_REDUCE);
		pTriggerStep->orconf = orconf;
	}
	else{
		sqlite3IdListDelete(db, pColumn);
	}
	sqlite3ExprListDelete(db, pEList);
	sqlite3SelectDelete(db, pSelect);

	return pTriggerStep;
}

/*
** Construct a trigger step that implements an UPDATE statement and return
** a pointer to that trigger step.  The parser calls this routine when it
** sees an UPDATE statement inside the body of a CREATE TRIGGER.构造一个触发器步骤去实现一个UPDATE语句，并返回一个触发步骤指针。解析调用这个程序，当发现在CREATE TRIGGER语句中有UPDATE语句
*/
TriggerStep *sqlite3TriggerUpdateStep(
	sqlite3 *db,         /* The database connection */
	Token *pTableName,   /* Name of the table to be updated 更行表的名字 */
	ExprList *pEList,    /* The SET clause: list of column and new values //SET子句：行列表和新值 */
	Expr *pWhere,        /* The WHERE clause */
	u8 orconf            /* The conflict algorithm. (OE_Abort, OE_Ignore, etc) */
	){
	TriggerStep *pTriggerStep;

	pTriggerStep = triggerStepAllocate(db, TK_UPDATE, pTableName);
	if (pTriggerStep){
		pTriggerStep->pExprList = sqlite3ExprListDup(db, pEList, EXPRDUP_REDUCE);
		pTriggerStep->pWhere = sqlite3ExprDup(db, pWhere, EXPRDUP_REDUCE);
		pTriggerStep->orconf = orconf;
	}
	sqlite3ExprListDelete(db, pEList);
	sqlite3ExprDelete(db, pWhere);
	return pTriggerStep;
}

/*
** Construct a trigger step that implements a DELETE statement and return
** a pointer to that trigger step.  The parser calls this routine when it
** sees a DELETE statement inside the body of a CREATE TRIGGER. 构造一个触发器步骤去实现DELETE语句并返回一个触发步骤指针。解析器调用这个例程时,当发现在CREATE TRIGGER语句中有DELETE语句。
*/
TriggerStep *sqlite3TriggerDeleteStep(
	sqlite3 *db,            /* Database connection */
	Token *pTableName,      /* The table from which rows are deleted 表中被删除的行 */
	Expr *pWhere            /* The WHERE clause */
	){
	TriggerStep *pTriggerStep;

	pTriggerStep = triggerStepAllocate(db, TK_DELETE, pTableName);
	if (pTriggerStep){
		pTriggerStep->pWhere = sqlite3ExprDup(db, pWhere, EXPRDUP_REDUCE);
		pTriggerStep->orconf = OE_Default;
	}
	sqlite3ExprDelete(db, pWhere);
	return pTriggerStep;
}

/*
** Recursively delete a Trigger structure 递归删除一个触发器结构
*/
void sqlite3DeleteTrigger(sqlite3 *db, Trigger *pTrigger){
	if (pTrigger == 0) return;
	sqlite3DeleteTriggerStep(db, pTrigger->step_list);
	sqlite3DbFree(db, pTrigger->zName);
	sqlite3DbFree(db, pTrigger->table);
	sqlite3ExprDelete(db, pTrigger->pWhen);
	sqlite3IdListDelete(db, pTrigger->pColumns);
	sqlite3DbFree(db, pTrigger);
}

/*
** This function is called to drop a trigger from the database schema. 这个函数被调用当从数据库模式中终止一个触发器
**
** This may be called directly from the parser and therefore identifies
** the trigger by name.  The sqlite3DropTriggerPtr() routine does the
** same job as this routine except it takes a pointer to the trigger
** instead of the trigger name.这可能被解析器直接调用,因此需要确定触发器的名字。sqlite3DropTriggerPtr()例程与这个例程同样的工作,除了需要改变指针到这个触发器代替触发器的名字。
**/
void sqlite3DropTrigger(Parse *pParse, SrcList *pName, int noErr){
	Trigger *pTrigger = 0;
	int i;
	const char *zDb;
	const char *zName;
	int nName;
	sqlite3 *db = pParse->db;

	if (db->mallocFailed) goto drop_trigger_cleanup;
	if (SQLITE_OK != sqlite3ReadSchema(pParse)){
		goto drop_trigger_cleanup;
	}

	assert(pName->nSrc == 1);
	zDb = pName->a[0].zDatabase;
	zName = pName->a[0].zName;
	nName = sqlite3Strlen30(zName);
	assert(zDb != 0 || sqlite3BtreeHoldsAllMutexes(db));
	for (i = OMIT_TEMPDB; i < db->nDb; i++){
		int j = (i < 2) ? i ^ 1 : i;  /* Search TEMP before MAIN  搜索TEMP在MAIN之后 */
		if (zDb && sqlite3StrICmp(db->aDb[j].zName, zDb)) continue;
		assert(sqlite3SchemaMutexHeld(db, j, 0));
		pTrigger = sqlite3HashFind(&(db->aDb[j].pSchema->trigHash), zName, nName);
		if (pTrigger) break;
	}
	if (!pTrigger){
		if (!noErr){
			sqlite3ErrorMsg(pParse, "no such trigger: %S", pName, 0);
		}
		else{
			sqlite3CodeVerifyNamedSchema(pParse, zDb);
		}
		pParse->checkSchema = 1;
		goto drop_trigger_cleanup;
	}
	sqlite3DropTriggerPtr(pParse, pTrigger);

drop_trigger_cleanup:
	sqlite3SrcListDelete(db, pName);
}

/*
** Return a pointer to the Table structure for the table that a trigger
** is set on. 返回一个指针到触发器被打开表的表结构。
*/
static Table *tableOfTrigger(Trigger *pTrigger){
	int n = sqlite3Strlen30(pTrigger->table);
	return sqlite3HashFind(&pTrigger->pTabSchema->tblHash, pTrigger->table, n);
}


/*
** Drop a trigger given a pointer to that trigger.  终止一个触发器，并指向这个触发器
*/
void sqlite3DropTriggerPtr(Parse *pParse, Trigger *pTrigger){
	Table   *pTable;
	Vdbe *v;
	sqlite3 *db = pParse->db;
	int iDb;

	iDb = sqlite3SchemaToIndex(pParse->db, pTrigger->pSchema);
	assert(iDb >= 0 && iDb < db->nDb);
	pTable = tableOfTrigger(pTrigger);
	assert(pTable);
	assert(pTable->pSchema == pTrigger->pSchema || iDb == 1);
#ifndef SQLITE_OMIT_AUTHORIZATION
	{
		int code = SQLITE_DROP_TRIGGER;
		const char *zDb = db->aDb[iDb].zName;
		const char *zTab = SCHEMA_TABLE(iDb);
		if (iDb == 1) code = SQLITE_DROP_TEMP_TRIGGER;
		if (sqlite3AuthCheck(pParse, code, pTrigger->zName, pTable->zName, zDb) ||
			sqlite3AuthCheck(pParse, SQLITE_DELETE, zTab, 0, zDb)){
			return;
		}
	}
#endif

	/* Generate code to destroy the database record of the trigger. 生成代码破坏数据库触发器的记录。
	*/
	assert(pTable != 0);
	if ((v = sqlite3GetVdbe(pParse)) != 0){
		int base;
		static const VdbeOpList dropTrigger[] = {
			{ OP_Rewind, 0, ADDR(9), 0 },
			{ OP_String8, 0, 1, 0 }, /* 1 */
			{ OP_Column, 0, 1, 2 },
			{ OP_Ne, 2, ADDR(8), 1 },
			{ OP_String8, 0, 1, 0 }, /* 4: "trigger" */
			{ OP_Column, 0, 0, 2 },
			{ OP_Ne, 2, ADDR(8), 1 },
			{ OP_Delete, 0, 0, 0 },
			{ OP_Next, 0, ADDR(1), 0 }, /* 8 */
		};

		sqlite3BeginWriteOperation(pParse, 0, iDb);
		sqlite3OpenMasterTable(pParse, iDb);
		base = sqlite3VdbeAddOpList(v, ArraySize(dropTrigger), dropTrigger);
		sqlite3VdbeChangeP4(v, base + 1, pTrigger->zName, P4_TRANSIENT);
		sqlite3VdbeChangeP4(v, base + 4, "trigger", P4_STATIC);
		sqlite3ChangeCookie(pParse, iDb);
		sqlite3VdbeAddOp2(v, OP_Close, 0, 0);
		sqlite3VdbeAddOp4(v, OP_DropTrigger, iDb, 0, 0, pTrigger->zName, 0);
		if (pParse->nMem < 3){
			pParse->nMem = 3;
		}
	}
}

/*
** Remove a trigger from the hash tables of the sqlite* pointer. 在sqlite*指向的哈希表中移出触发器
*/
void sqlite3UnlinkAndDeleteTrigger(sqlite3 *db, int iDb, const char *zName){
	Trigger *pTrigger;
	Hash *pHash;

	assert(sqlite3SchemaMutexHeld(db, iDb, 0));
	pHash = &(db->aDb[iDb].pSchema->trigHash);
	pTrigger = sqlite3HashInsert(pHash, zName, sqlite3Strlen30(zName), 0);
	if (ALWAYS(pTrigger)){
		if (pTrigger->pSchema == pTrigger->pTabSchema){
			Table *pTab = tableOfTrigger(pTrigger);
			Trigger **pp;
			for (pp = &pTab->pTrigger; *pp != pTrigger; pp = &((*pp)->pNext));
			*pp = (*pp)->pNext;
		}
		sqlite3DeleteTrigger(db, pTrigger);
		db->flags |= SQLITE_InternChanges;
	}
}

/*
** pEList is the SET clause of an UPDATE statement.  Each entry
** in pEList is of the format <id>=<expr>.  If any of the entries
** in pEList have an <id> which matches an identifier in pIdList,
** then return TRUE.  If pIdList==NULL, then it is considered a
** wildcard that matches anything.  Likewise if pEList==NULL then
** it matches anything so always return true.  Return false only
** if there is no match.
**	pEList是 一个UPDATE语句的SET子句。每一个pEList的入口都是<id>=<expr>格式。如果任何pEList的入口有一个<id>在pIdList匹配一个标识符，就会返回TRUE。如果pIdList为空，那么它被认为是一个通配符匹配。
**同样如果pEList为空,那么它匹配任何值，所以总是返回true。只有没有匹配才会返回false。
*/
static int checkColumnOverlap(IdList *pIdList, ExprList *pEList){
	int e;
	if (pIdList == 0 || NEVER(pEList == 0)) return 1;
	for (e = 0; e < pEList->nExpr; e++){
		if (sqlite3IdListIndex(pIdList, pEList->a[e].zName) >= 0) return 1;
	}
	return 0;
}

/*
** Return a list of all triggers on table pTab if there exists at least
** one trigger that must be fired when an operation of type 'op' is
** performed on the table, and, if that operation is an UPDATE, if at
** least one of the columns in pChanges is being modified.
**如果存在至少一个触发器,必须被引发当'op'类型在表中执行,返回一个表pTab的所有触发器的列表，并且如果操作是UPDATE,如果至少一个pChanges列正在被修改，
*/
Trigger *sqlite3TriggersExist(
	Parse *pParse,          /* Parse context  */
	Table *pTab,            /* The table the contains the triggers */
	int op,                 /* one of TK_DELETE, TK_INSERT, TK_UPDATE */
	ExprList *pChanges,     /* Columns that change in an UPDATE statement */
	int *pMask              /* OUT: Mask of TRIGGER_BEFORE|TRIGGER_AFTER */
	){
	int mask = 0;
	Trigger *pList = 0;
	Trigger *p;

	if ((pParse->db->flags & SQLITE_EnableTrigger) != 0){
		pList = sqlite3TriggerList(pParse, pTab);
	}
	assert(pList == 0 || IsVirtual(pTab) == 0);
	for (p = pList; p; p = p->pNext){
		if (p->op == op && checkColumnOverlap(p->pColumns, pChanges)){
			mask |= p->tr_tm;
		}
	}
	if (pMask){
		*pMask = mask;
	}
	return (mask ? pList : 0);
}

/*
** Convert the pStep->target token into a SrcList and return a pointer
** to that SrcList.转换pStep->target并放进一个SrcList，并且返回一个指向SrcList的指针
**
** This routine adds a specific database name, if needed, to the target when
** forming the SrcList.  This prevents a trigger in one database from
** referring to a target in another database.  An exception is when the
** trigger is in TEMP in which case it can refer to any other database it
** wants.
**这个例程添加一个特定的数据库名称,如果需要,作为SrcList形成的目标。这可以防止在一个数据库触发器提交的目标在另一个数据库。例外是当触发器在TEMP状态,它可以指任何其他数据库。
**
*/
static SrcList *targetSrcList(
	Parse *pParse,       /* The parsing context */
	TriggerStep *pStep   /* The trigger containing the target token */
	){
	int iDb;             /* Index of the database to use */
	SrcList *pSrc;       /* SrcList to be returned */

	pSrc = sqlite3SrcListAppend(pParse->db, 0, &pStep->target, 0);
	if (pSrc){
		assert(pSrc->nSrc > 0);
		assert(pSrc->a != 0);
		iDb = sqlite3SchemaToIndex(pParse->db, pStep->pTrig->pSchema);
		if (iDb == 0 || iDb >= 2){
			sqlite3 *db = pParse->db;
			assert(iDb < pParse->db->nDb);
			pSrc->a[pSrc->nSrc - 1].zDatabase = sqlite3DbStrDup(db, db->aDb[iDb].zName);
		}
	}
	return pSrc;
}

/*
** Generate VDBE code for the statements inside the body of a single
** trigger.生成VDBE代码给一个单一的触发器语句体内。
*/
static int codeTriggerProgram(
	Parse *pParse,            /* The parser context */
	TriggerStep *pStepList,   /* List of statements inside the trigger body */
	int orconf                /* Conflict algorithm. (OE_Abort, etc) */
	){
	TriggerStep *pStep;
	Vdbe *v = pParse->pVdbe;
	sqlite3 *db = pParse->db;

	assert(pParse->pTriggerTab && pParse->pToplevel);
	assert(pStepList);
	assert(v != 0);
	for (pStep = pStepList; pStep; pStep = pStep->pNext){
		/* Figure out the ON CONFLICT policy that will be used for this step
		** of the trigger program. If the statement that caused this trigger
		** to fire had an explicit ON CONFLICT, then use it. Otherwise, use
		** the ON CONFLICT policy that was specified as part of the trigger
		** step statement. Example:
		**找出冲突并将会被用在触发器程序的步骤中的规则。如果这引发触发器错误的语句描述ON CONFLICT,则使用它。否则,使用指定的ON CONFLICT规则作为触发器步骤声明的一部分。例子:
		**
		**   CREATE TRIGGER AFTER INSERT ON t1 BEGIN;
		**     INSERT OR REPLACE INTO t2 VALUES(new.a, new.b);
		**   END;
		**
		**   INSERT INTO t1 ... ;            -- insert into t2 uses REPLACE policy
		**   INSERT OR IGNORE INTO t1 ... ;  -- insert into t2 uses IGNORE policy
		*/
		pParse->eOrconf = (orconf == OE_Default) ? pStep->orconf : (u8)orconf;

		switch (pStep->op){
		case TK_UPDATE: {
							sqlite3Update(pParse,
								targetSrcList(pParse, pStep),
								sqlite3ExprListDup(db, pStep->pExprList, 0),
								sqlite3ExprDup(db, pStep->pWhere, 0),
								pParse->eOrconf
								);
							break;
		}
		case TK_INSERT: {
							sqlite3Insert(pParse,
								targetSrcList(pParse, pStep),
								sqlite3ExprListDup(db, pStep->pExprList, 0),
								sqlite3SelectDup(db, pStep->pSelect, 0),
								sqlite3IdListDup(db, pStep->pIdList),
								pParse->eOrconf
								);
							break;
		}
		case TK_DELETE: {
							sqlite3DeleteFrom(pParse,
								targetSrcList(pParse, pStep),
								sqlite3ExprDup(db, pStep->pWhere, 0)
								);
							break;
		}
		default: assert(pStep->op == TK_SELECT); {
					 SelectDest sDest;
					 Select *pSelect = sqlite3SelectDup(db, pStep->pSelect, 0);
					 sqlite3SelectDestInit(&sDest, SRT_Discard, 0);
					 sqlite3Select(pParse, pSelect, &sDest);
					 sqlite3SelectDelete(db, pSelect);
					 break;
		}
		}
		if (pStep->op != TK_SELECT){
			sqlite3VdbeAddOp0(v, OP_ResetCount);
		}
	}

	return 0;
}

#ifdef SQLITE_DEBUG
/*
** This function is used to add VdbeComment() annotations to a VDBE
** program. It is not used in production code, only for debugging.这个函数被用来添加VdbeComment()注释VDBE程序。它不是用于生产代码,只有进行调试。
*/
static const char *onErrorText(int onError){
	switch (onError){
	case OE_Abort:    return "abort";
	case OE_Rollback: return "rollback";
	case OE_Fail:     return "fail";
	case OE_Replace:  return "replace";
	case OE_Ignore:   return "ignore";
	case OE_Default:  return "default";
	}
	return "n/a";
}
#endif

/*
** Parse context structure pFrom has just been used to create a sub-vdbe
** (trigger program). If an error has occurred, transfer error information
** from pFrom to pTo. 解析环境结构pFrom只能被用了该创建sub-vdbe(触发程序)。如果发生了错误,从pFrom输出错误信息到pTo。
*/
static void transferParseError(Parse *pTo, Parse *pFrom){
	assert(pFrom->zErrMsg == 0 || pFrom->nErr);
	assert(pTo->zErrMsg == 0 || pTo->nErr);
	if (pTo->nErr == 0){
		pTo->zErrMsg = pFrom->zErrMsg;
		pTo->nErr = pFrom->nErr;
	}
	else{
		sqlite3DbFree(pFrom->db, pFrom->zErrMsg);
	}
}

/*
** Create and populate a new TriggerPrg object with a sub-program
** implementing trigger pTrigger with ON CONFLICT policy orconf.创建并填充一个新的TriggerPrg对象同子程序，触发pTrigger orconf冲突策略。
*/
static TriggerPrg *codeRowTrigger(
	Parse *pParse,       /* Current parse context */
	Trigger *pTrigger,   /* Trigger to code */
	Table *pTab,         /* The table pTrigger is attached to */
	int orconf           /* ON CONFLICT policy to code trigger program with */
	){
	Parse *pTop = sqlite3ParseToplevel(pParse);
	sqlite3 *db = pParse->db;   /* Database handle 数据库句柄  */
	TriggerPrg *pPrg;           /* Value to return 返回值 */
	Expr *pWhen = 0;            /* Duplicate of trigger WHEN expression 触发器WHEN语句的副本 */
	Vdbe *v;                    /* Temporary VM 临时虚拟存贮器*/
	NameContext sNC;            /* Name context for sub-vdbe ///sub-vdbe的命名环境*/
	SubProgram *pProgram = 0;   /* Sub-vdbe for trigger program */
	Parse *pSubParse;           /* Parse context for sub-vdbe///sub-vdbe的解析环境 */
	int iEndTrigger = 0;        /* Label to jump to if WHEN is false//WHEN值false的跳转标签  */

	assert(pTrigger->zName == 0 || pTab == tableOfTrigger(pTrigger));
	assert(pTop->pVdbe);

	/* Allocate the TriggerPrg and SubProgram objects. To ensure that they
	** are freed if an error occurs, link them into the Parse.pTriggerPrg
	** list of the top-level Parse object sooner rather than later. 分配TriggerPrg和子程序对象。为了确保如果出现错误他们被释放,联系到语法分析。顶级解析对象的pTriggerPrg列表越早越好。 */
	pPrg = sqlite3DbMallocZero(db, sizeof(TriggerPrg));
	if (!pPrg) return 0;
	pPrg->pNext = pTop->pTriggerPrg;
	pTop->pTriggerPrg = pPrg;
	pPrg->pProgram = pProgram = sqlite3DbMallocZero(db, sizeof(SubProgram));
	if (!pProgram) return 0;
	sqlite3VdbeLinkSubProgram(pTop->pVdbe, pProgram);
	pPrg->pTrigger = pTrigger;
	pPrg->orconf = orconf;
	pPrg->aColmask[0] = 0xffffffff;
	pPrg->aColmask[1] = 0xffffffff;

	/* Allocate and populate a new Parse context to use for coding the
	** trigger sub-program. 分配和填充一个语法分析器，用于编码触发器子程序。 */
	pSubParse = sqlite3StackAllocZero(db, sizeof(Parse));
	if (!pSubParse) return 0;
	memset(&sNC, 0, sizeof(sNC));
	sNC.pParse = pSubParse;
	pSubParse->db = db;
	pSubParse->pTriggerTab = pTab;
	pSubParse->pToplevel = pTop;
	pSubParse->zAuthContext = pTrigger->zName;
	pSubParse->eTriggerOp = pTrigger->op;
	pSubParse->nQueryLoop = pParse->nQueryLoop;

	v = sqlite3GetVdbe(pSubParse);
	if (v){
		VdbeComment((v, "Start: %s.%s (%s %s%s%s ON %s)",
			pTrigger->zName, onErrorText(orconf),
			(pTrigger->tr_tm == TRIGGER_BEFORE ? "BEFORE" : "AFTER"),
			(pTrigger->op == TK_UPDATE ? "UPDATE" : ""),
			(pTrigger->op == TK_INSERT ? "INSERT" : ""),
			(pTrigger->op == TK_DELETE ? "DELETE" : ""),
			pTab->zName
			));
#ifndef SQLITE_OMIT_TRACE
		sqlite3VdbeChangeP4(v, -1,
			sqlite3MPrintf(db, "-- TRIGGER %s", pTrigger->zName), P4_DYNAMIC
			);
#endif

		/* If one was specified, code the WHEN clause. If it evaluates to false
		** (or NULL) the sub-vdbe is immediately halted by jumping to the
		** OP_Halt inserted at the end of the program. 如果代码WHEN子句被指定。如果它的求值结果为false(或NULL)sub-vdbe立即停止，通过转到插入在项目最后的OP_Halt。  */
		if (pTrigger->pWhen){
			pWhen = sqlite3ExprDup(db, pTrigger->pWhen, 0);
			if (SQLITE_OK == sqlite3ResolveExprNames(&sNC, pWhen)
				&& db->mallocFailed == 0
				){
				iEndTrigger = sqlite3VdbeMakeLabel(v);
				sqlite3ExprIfFalse(pSubParse, pWhen, iEndTrigger, SQLITE_JUMPIFNULL);
			}
			sqlite3ExprDelete(db, pWhen);
		}

		/* Code the trigger program into the sub-vdbe. 触发器程序编码进入sub-vdbe */
		codeTriggerProgram(pSubParse, pTrigger->step_list, orconf);

		/* Insert an OP_Halt at the end of the sub-program. 插入一个OP_Halt在子程序最后 */
		if (iEndTrigger){
			sqlite3VdbeResolveLabel(v, iEndTrigger);
		}
		sqlite3VdbeAddOp0(v, OP_Halt);
		VdbeComment((v, "End: %s.%s", pTrigger->zName, onErrorText(orconf)));

		transferParseError(pParse, pSubParse);
		if (db->mallocFailed == 0){
			pProgram->aOp = sqlite3VdbeTakeOpArray(v, &pProgram->nOp, &pTop->nMaxArg);
		}
		pProgram->nMem = pSubParse->nMem;
		pProgram->nCsr = pSubParse->nTab;
		pProgram->nOnce = pSubParse->nOnce;
		pProgram->token = (void *)pTrigger;
		pPrg->aColmask[0] = pSubParse->oldmask;
		pPrg->aColmask[1] = pSubParse->newmask;
		sqlite3VdbeDelete(v);
	}

	assert(!pSubParse->pAinc       && !pSubParse->pZombieTab);
	assert(!pSubParse->pTriggerPrg && !pSubParse->nMaxArg);
	sqlite3StackFree(db, pSubParse);

	return pPrg;
}

/*
** Return a pointer to a TriggerPrg object containing the sub-program for
** trigger pTrigger with default ON CONFLICT algorithm orconf. If no such
** TriggerPrg object exists, a new object is allocated and populated before
** being returned. 返回一个指针,指向TriggerPrg对象包含触发器pTrigger子程序，用默认ON CONFLICT算法规则，。如果没有这样的TriggerPrg对象存在,分配一个新对象并在返回之前填充。
*/
static TriggerPrg *getRowTrigger(
	Parse *pParse,       /* Current parse context */
	Trigger *pTrigger,   /* Trigger to code */
	Table *pTab,         /* The table trigger pTrigger is attached to */
	int orconf           /* ON CONFLICT algorithm. */
	){
	Parse *pRoot = sqlite3ParseToplevel(pParse);
	TriggerPrg *pPrg;

	assert(pTrigger->zName == 0 || pTab == tableOfTrigger(pTrigger));

	/* It may be that this trigger has already been coded (or is in the
	** process of being coded). If this is the case, then an entry with
	** a matching TriggerPrg.pTrigger field will be present somewhere
	** in the Parse.pTriggerPrg list. Search for such an entry. 也许这个触发器已经被编码(或在被编码过程中)。如果是这样的话,那么一个匹配TriggerPrg.pTrigger字段的入口将出现在解析。pTriggerPrg列表。寻找这样的一个条目。* / */
	for (pPrg = pRoot->pTriggerPrg;
		pPrg && (pPrg->pTrigger != pTrigger || pPrg->orconf != orconf);
		pPrg = pPrg->pNext
		);

	/* If an existing TriggerPrg could not be located, create a new one. 如果现有的TriggerPrg不能被定位,则创建一个新的。*/
	if (!pPrg){
		pPrg = codeRowTrigger(pParse, pTrigger, pTab, orconf);
	}

	return pPrg;
}

/*
** Generate code for the trigger program associated with trigger p on
** table pTab. The reg, orconf and ignoreJump parameters passed to this
** function are the same as those described in the header function for
** sqlite3CodeRowTrigger() 生成触发程序代码与触发器表pTab中的p关联.这个注册,orconf和ignoreJump参数传递给该函数，与sqlite3CodeRowTrigger()头函数描述的一样
*/
void sqlite3CodeRowTriggerDirect(
	Parse *pParse,       /* Parse context */
	Trigger *p,          /* Trigger to code */
	Table *pTab,         /* The table to code triggers from */
	int reg,             /* Reg array containing OLD.* and NEW.* values */
	int orconf,          /* ON CONFLICT policy */
	int ignoreJump       /* Instruction to jump to for RAISE(IGNORE)  //RAISE(IGNORE)的跳转指令 */
	){
	Vdbe *v = sqlite3GetVdbe(pParse); /* Main VM 主虚拟存贮器 */
	TriggerPrg *pPrg;
	pPrg = getRowTrigger(pParse, p, pTab, orconf);
	assert(pPrg || pParse->nErr || pParse->db->mallocFailed);

	/* Code the OP_Program opcode in the parent VDBE. P4 of the OP_Program
	** is a pointer to the sub-vdbe containing the trigger program.   在父VDBE中对OP_Program的操作码编码。OP_Program的P4是一个指向包含触发器程序的sub-vdbe 指针。*/
	if (pPrg){
		int bRecursive = (p->zName && 0 == (pParse->db->flags&SQLITE_RecTriggers));

		sqlite3VdbeAddOp3(v, OP_Program, reg, ignoreJump, ++pParse->nMem);
		sqlite3VdbeChangeP4(v, -1, (const char *)pPrg->pProgram, P4_SUBPROGRAM);
		VdbeComment(
			(v, "Call: %s.%s", (p->zName ? p->zName : "fkey"), onErrorText(orconf)));

		/* Set the P5 operand of the OP_Program instruction to non-zero if
		** recursive invocation of this trigger program is disallowed. Recursive
		** invocation is disallowed if (a) the sub-program is really a trigger,
		** not a foreign key action, and (b) the flag to enable recursive triggers
		** is clear.
		**设置OP_Program 的P5操作数的指令设置为非零,如果这个触发器程序的递归调用无效。递归调用是无效的如果(a)子程序实际上是一个触发器,不是一个外键操作,(b)递归调用触发器有效的标记将被清除。
		*/

		sqlite3VdbeChangeP5(v, (u8)bRecursive);
	}
}

/*
** This is called to code the required FOR EACH ROW triggers for an operation
** on table pTab. The operation to code triggers for (INSERT, UPDATE or DELETE)
** is given by the op paramater. The tr_tm parameter determines whether the
** BEFORE or AFTER triggers are coded. If the operation is an UPDATE, then
** parameter pChanges is passed the list of columns being modified.
** 这里被调用去编码FOR EACH ROW触发器的请求，对于pTab表的操作。这个操作区编码(INSERT, UPDATE or DELETE)被给予的参数。tr_tm参数确定是BEFORE 或 AFTER触发器被编码。如果操作是UPDATE,那么参数pChanges被传递到正在被修改的列表行。
**
** If there are no triggers that fire at the specified time for the specified
** operation on pTab, this function is a no-op.如果没有触发器,可以在指定的时间指定的操作pTab,这个函数是一个空操作。
**
** The reg argument is the address of the first in an array of registers
** that contain the values substituted for the new.* and old.* references
** in the trigger program. If N is the number of columns in table pTab
** (a copy of pTab->nCol), then registers are populated as follows:
**注册参数是第一个寄存器数组的地址包含的值代替 new.* 和old.* 在触发器程序的引用。如果N是pTab表的列数量(pTab - > nCol的副本),然后寄存器填充如下:
**
**   Register       Contains
**   ------------------------------------------------------
**   reg+0          OLD.rowid
**   reg+1          OLD.* value of left-most column of pTab pTab最左列的值
**   ...            ...
**   reg+N          OLD.* value of right-most column of pTab
**   reg+N+1        NEW.rowid
**   reg+N+2        OLD.* value of left-most column of pTab
**   ...            ...
**   reg+N+N+1      NEW.* value of right-most column of pTab
**
** For ON DELETE triggers, the registers containing the NEW.* values will
** never be accessed by the trigger program, so they are not allocated or
** populated by the caller (there is no data to populate them with anyway).
** Similarly, for ON INSERT triggers the values stored in the OLD.* registers
** are never accessed, and so are not allocated by the caller. So, for an
** ON INSERT trigger, the value passed to this function as parameter reg
** is not a readable register, although registers (reg+N) through
** (reg+N+N+1) are.
**在ON DELETE触发器,包含NEW.*值永远不会触发程序的访问,所以他们不被分配或由调用者(没有数据来填充他们)。
**同样,在ON INSERT触发器存储在 OLD.*值注册无法访问,所以不分配调用者。所以对一个ON INSERT触发器,传递给这个函数的作为注册参数的值不是可读寄存器,尽管暂存器(reg+ N)到(reg + N + N + 1)是。
**
** Parameter orconf is the default conflict resolution algorithm for the
** trigger program to use (REPLACE, IGNORE etc.). Parameter ignoreJump
** is the instruction that control should jump to if a trigger program
** raises an IGNORE exception.
**参数orconf是默认的触发程序使用(REPLACE、IGNORE等)的冲突解决算法。参数ignoreJump控制应该跳转到一个是否增加一个IGNORE异常的触发器程序的指令。
*/
void sqlite3CodeRowTrigger(
	Parse *pParse,       /* Parse context */
	Trigger *pTrigger,   /* List of triggers on table pTab */
	int op,              /* One of TK_UPDATE, TK_INSERT, TK_DELETE */
	ExprList *pChanges,  /* Changes list for any UPDATE OF triggers */
	int tr_tm,           /* One of TRIGGER_BEFORE, TRIGGER_AFTER */
	Table *pTab,         /* The table to code triggers from */
	int reg,             /* The first in an array of registers (see above) */
	int orconf,          /* ON CONFLICT policy */
	int ignoreJump       /* Instruction to jump to for RAISE(IGNORE) */
	){
	Trigger *p;          /* Used to iterate through pTrigger list 用于遍历pTrigger列表 */

	assert(op == TK_UPDATE || op == TK_INSERT || op == TK_DELETE);
	assert(tr_tm == TRIGGER_BEFORE || tr_tm == TRIGGER_AFTER);
	assert((op == TK_UPDATE) == (pChanges != 0));

	for (p = pTrigger; p; p = p->pNext){

		/* Sanity checking:  The schema for the trigger and for the table are
		** always defined.  The trigger must be in the same schema as the table
		** or else it must be a TEMP trigger. 完整性检查:触发器和表的模式总是被定义的。触发器必须和表在相同的模式,否则必须是临时触发器。 */
		assert(p->pSchema != 0);
		assert(p->pTabSchema != 0);
		assert(p->pSchema == p->pTabSchema
			|| p->pSchema == pParse->db->aDb[1].pSchema);

		/* Determine whether we should code this trigger 确定我们是否应该编码这个触发器*/
		if (p->op == op
			&& p->tr_tm == tr_tm
			&& checkColumnOverlap(p->pColumns, pChanges)
			){
			sqlite3CodeRowTriggerDirect(pParse, p, pTab, reg, orconf, ignoreJump);
		}
	}
}

/*
** Triggers may access values stored in the old.* or new.* pseudo-table.
** This function returns a 32-bit bitmask indicating which columns of the
** old.* or new.* tables actually are used by triggers. This information
** may be used by the caller, for example, to avoid having to load the entire
** old.* record into memory when executing an UPDATE or DELETE command.
**触发器可以存取的值存储在old.* 或 new.* 伪表。这个函数返回一个32位的位掩码来显示old.* 或 new.*表哪些列实际上是使用触发器的。这些信息可能由调用者使用,例如,为了避免执行UPDATE或DELETE命令时加载old.*记录到内存中。
**
** Bit 0 of the returned mask is set if the left-most column of the
** table may be accessed using an [old|new].<col> reference. Bit 1 is set if
** the second leftmost column value is required, and so on. If there
** are more than 32 columns in the table, and at least one of the columns
** with an index greater than 32 may be accessed, 0xffffffff is returned.
**返回掩码的位0设置是否是最表左侧列可以访问，使用一个[old|new].<col>参考。位1设置是否是第二个最左边的列的值是必需的,等等。如果有超过32列在表中,或者至少一个列的索引大于可能访问的32,返回0 xffffffff。
**
** It is not possible to determine if the old.rowid or new.rowid column is
** accessed by triggers. The caller must always assume that it is. //old.记录或者 new.记录 列访问触发器是不可能确定的。调用者必须假定它是。
**
** Parameter isNew must be either 1 or 0. If it is 0, then the mask returned
** applies to the old.* table. If 1, the new.* table. 参数isNew必须是1或0。如果是0,那么返回的掩码适用于old.*表。如果是1,者是new.* 表
**
** Parameter tr_tm must be a mask with one or both of the TRIGGER_BEFORE
** and TRIGGER_AFTER bits set. Values accessed by BEFORE triggers are only
** included in the returned mask if the TRIGGER_BEFORE bit is set in the
** tr_tm parameter. Similarly, values accessed by AFTER triggers are only
** included in the returned mask if the TRIGGER_AFTER bit is set in tr_tm.
**参数tr_tm必须是一个一个或者多个TRIGGER_BEFORE和TRIGGER_AFTER位设置的掩码。BEFORE触发器的值存取只包含在返回掩码是否是TRIGGER_BEFORE位被在tr_tm参数设置。同样,AFTER触发器的值存取只包含在返回掩码是否是TRIGGER_AFTER位被在tr_tm参数设置
*/
u32 sqlite3TriggerColmask(
	Parse *pParse,       /* Parse context */
	Trigger *pTrigger,   /* List of triggers on table pTab */
	ExprList *pChanges,  /* Changes list for any UPDATE OF triggers */
	int isNew,           /* 1 for new.* ref mask, 0 for old.* ref mask */
	int tr_tm,           /* Mask of TRIGGER_BEFORE|TRIGGER_AFTER// TRIGGER_BEFORE|TRIGGER_AFTER的掩码 */
	Table *pTab,         /* The table to code triggers from */
	int orconf           /* Default ON CONFLICT policy for trigger steps */
	){
	const int op = pChanges ? TK_UPDATE : TK_DELETE;
	u32 mask = 0;
	Trigger *p;

	assert(isNew == 1 || isNew == 0);
	for (p = pTrigger; p; p = p->pNext){
		if (p->op == op && (tr_tm&p->tr_tm)
			&& checkColumnOverlap(p->pColumns, pChanges)
			){
			TriggerPrg *pPrg;
			pPrg = getRowTrigger(pParse, p, pTab, orconf);
			if (pPrg){
				mask |= pPrg->aColmask[isNew];
			}
		}
	}

	return mask;
}

#endif /* !defined(SQLITE_OMIT_TRIGGER) */
