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
** This file contains C code routines that are called by the SQLite parser
** when syntax rules are reduced.  The routines in this file handle the
** following kinds of SQL syntax:
** 当语法规则被简化，这个文件中的C语言代码程序将会被SQLite的语法解析器去调用，在这个文件中的程序执行了下面的这些SQL语法。
**     CREATE TABLE   //创建表
**     DROP TABLE     //删除表
**     CREATE INDEX   //创建索引
**     DROP INDEX     //删除索引
**     creating ID lists
**     BEGIN TRANSACTION   //数据库中所有的操作都是以事务为单位进行的，这个语法是开始一个事务
**     COMMIT             //提交事务，如果成功的话
**     ROLLBACK      //当操作不能完成时，应该能够根据日志去回滚操作
** build.c文件的作用就是解析上面提到的那些操作
*/

#include "sqliteInt.h"
/*
** An SQL parser（语法分析程序） context. A copy of this structure is passed（传递） through
** the parser and down into all the parser action routine in order to
** carry around information that is global to the entire parse.
** 一个SQL语法分析器的上下文。这个结构通过分析器被传递，并且这个结构会向下进入到分析器的动作程序中，这个结构做上面两件事情的目的是为了携带语法分析程序的全局信息
** The structure is divided into two parts.  When the parser and code
** generate call themselves recursively, the first part of the structure
** is constant but the second part is reset at the beginning and end of
** each recursion（递归）.
** 这个结构被分成两个部分，当语法分析程序和代码生成程序递归调用它们自身的时候，这个结构的第一个部分是常量，但是第二个部分在每一次递归的开始和结束的时候都会被重置
** The nTableLock and aTableLock variables are only used if the shared-cache
** feature is enabled (if sqlite3Tsd()->useSharedData is true).
** 如果共享缓存结构有效的时候，整型的锁表变量和数组型的锁表变量就会被使用。
** They are used to store the set of table-locks required by the statement being
** compiled. Function sqlite3TableLock() is used to add entries to the
** list.
** 当锁表被要求设置的时候，上面提到的两个变量通常被存储。函数sqlite3TableLock()就是被用来在缓冲区有效时为表增加一个相应的锁。
*/
//struct Parse {
//	sqlite3 *db;         /* The main database structure   主数据库*/
//	char *zErrMsg;       /* An error message   错误信息 */
//	Vdbe *pVdbe;         /* An engine for executing database bytecode   执行数据库字节码的引擎*/
//	int rc;              /* Return code from execution   返回 */
//	//typedef UINT8_TYPE u8;             /* 1-byte unsigned integer  一个字节的无符号整型*/
//	u8 colNamesSet;      /* TRUE after OP_ColumnName has been issued(发送) to pVdbe  OP_ColumnName操作被发送给字节码执行引擎之后为真，也就是说VDBE引擎将要执行列名设置的操作 */
//	u8 checkSchema;      /* Causes schema cookie check after an error   检查模式错误*/
//	u8 nested;           /* Number of nested calls to the parser/code generator  嵌套的调用解析器或者代码生成器的层次*/
//	u8 nTempReg;         /* Number of temporary registers in aTempReg[] 在临时寄存器数组中临时寄存器的数目*/
//	u8 nTempInUse;       /* Number of aTempReg[] currently checked out  在临时寄存器数组中当前被检出的数目 */
//	u8 nColCache;        /* Number of entries in aColCache[]  列缓存数组中实体的数目*/
//	u8 iColCache;        /* Next entry in aColCache[] to replace 列缓存数组中下一个实体 */
//	u8 isMultiWrite;     /* True if statement may modify/insert multiple rows   如果语句有修改或者插入多行的操作时为真*/
//	u8 mayAbort;         /* True if statement may throw an ABORT exception   抛出一个终止的异常时为真 */
//	int aTempReg[8];     /* Holding area for temporary registers    保持临时寄存器区域 */
//	int nRangeReg;       /* Size of the temporary register block  临时寄存器块的大小 */
//	int iRangeReg;       /* First register in temporary register block  临时寄存器块中第一个寄存器*/
//	int nErr;            /* Number of errors seen   错误数*/
//	int nTab;            /* Number of previously allocated VDBE cursors  早前关联VDBE游标的数目*/
//	int nMem;            /* Number of memory cells used so far   到目前为止被使用的内存数目*/
//	int nSet;            /* Number of sets used so far */
//	int nOnce;           /* Number of OP_Once instructions so far */
//	int ckBase;          /* Base register of data during check constraints */
//	int iCacheLevel;     /* ColCache valid when aColCache[].iLevel<=iCacheLevel */
//	int iCacheCnt;       /* Counter used to generate aColCache[].lru values */
//	struct yColCache {
//		int iTable;           /* Table cursor number  表的游标数目*/
//		int iColumn;          /* Table column number   表的列数目*/
//		u8 tempReg;           /* iReg is a temp register that needs to be freed   一个需要被释放的临时寄存器*/
//		int iLevel;           /* Nesting level  嵌套的深度*/
//		int iReg;             /* Reg with value of this column. 0 means none. */
//		int lru;              /* Least recently used entry has the smallest value   最近最少使用记录（项）有最小的值*/
//	} aColCache[SQLITE_N_COLCACHE];  /* One for each column cache entry  对于每一个列缓存项*/
//	yDbMask writeMask;   /* Start a write transaction on these databases   在数据库中开始一个写事务*/
//	yDbMask cookieMask;  /* Bitmask of schema verified databases 已经验证的数据库的模式位掩码 */
//	int cookieGoto;      /* Address of OP_Goto to cookie verifier subroutine  跳转到子程序的地址*/
//	int cookieValue[SQLITE_MAX_ATTACHED + 2];  /* Values of cookies to verify  被验证的cookies的值*/
//	int regRowid;        /* Register holding rowid of CREATE TABLE entry  */
//	int regRoot;         /* Register holding root page number for new objects */
//	int nMaxArg;         /* Max args passed to user function by sub-program  用户函数的最大个数*/
//	Token constraintName;/* Name of the constraint currently being parsed 当前正在被解析的约束的名字*/
//#ifndef SQLITE_OMIT_SHARED_CACHE
//	int nTableLock;        /* Number of locks in aTableLock   在锁表数组中锁的数目*/
//	TableLock *aTableLock; /* Required table locks for shared-cache mode  共享缓存模式时锁表被要求*/
//#endif
//	AutoincInfo *pAinc;  /* Information about AUTOINCREMENT counters  自动增量计数器的信息 */
//
//	/* Information used while coding trigger（触发器） programs.  下面的信息是当用户编写触发器程序时用到的*/
//	Parse *pToplevel;    /* Parse structure for main program (or NULL)  主程序的语法分析结构*/
//	Table *pTriggerTab;  /* Table triggers are being coded for  */
//	double nQueryLoop;   /* Estimated number of iterations of a query  估计估算一个查询的迭代次数*/
//	u32 oldmask;         /* Mask of old.* columns referenced   */
//	u32 newmask;         /* Mask of new.* columns referenced */
//	u8 eTriggerOp;       /* TK_UPDATE, TK_INSERT or TK_DELETE  定义触发器的操作*/
//	u8 eOrconf;          /* Default ON CONFLICT policy for trigger steps  在冲突时发生时触发器默认的操作*/
//	u8 disableTriggers;  /* True to disable triggers 当触发器无效时为真*/
//
//	/* Above is constant between recursions.  Below is reset before and after
//	** each recursion   当在递归发生的时候，上面的操作是一个常量。 当在每一个递归开始和结束的时候下面的结构将要被重置*/
//
//	int nVar;                 /* Number of '?' variables seen in the SQL so far  到目前为止我们所看到的SQL变量的数目*/
//	int nzVar;                /* Number of available slots in azVar[]  */
//	u8 explain;               /* True if the EXPLAIN flag is found on the query  如果在查询中解释标志被找到（发现）时explain被设置为真*/
//#ifndef SQLITE_OMIT_VIRTUALTABLE
//	u8 declareVtab;           /* True if inside sqlite3_declare_vtab() 在sqlite3_declare_vtab()内部的时候被设置为真*/
//	int nVtabLock;            /* Number of virtual tables to lock  虚表锁的数目*/
//#endif
//	int nAlias;               /* Number of aliased result set columns  设置列别名*/
//	int nHeight;              /* Expression tree height of current sub-select  当前子选择结构树的高度 */
//#ifndef SQLITE_OMIT_EXPLAIN
//	int iSelectId;            /* ID of current select for EXPLAIN output */
//	int iNextSelectId;        /* Next available select ID for EXPLAIN output */
//#endif
//	char **azVar;             /* Pointers to names of parameters  参数名指针 */
//	Vdbe *pReprepare;         /* VM being reprepared (sqlite3Reprepare()) */
//	int *aAlias;              /* Register used to hold aliased result 存放别名结果寄存器*/
//	const char *zTail;        /* All SQL text past the last semicolon parsed  SQL语句最后的技术；号*/
//	Table *pNewTable;         /* A table being constructed by CREATE TABLE   一个通过CREATE TABLE语句被创建的表*/
//	Trigger *pNewTrigger;     /* Trigger under construct by a CREATE TRIGGER  以个通过CREATE TRIGGER语句被创建的触发器 */
//	const char *zAuthContext; /* The 6th parameter to db->xAuth callbacks  */
//	Token sNameToken;         /* Token with unqualified schema object name 不合法的模式名符号 */
//	Token sLastToken;         /* The last token parsed  最后一个被解析的符号 */
//#ifndef SQLITE_OMIT_VIRTUALTABLE
//	Token sArg;               /* Complete text of a module argument  完整的模式参数 */
//	Table **apVtabLock;       /* Pointer to virtual tables needing locking 需要被加锁的虚表的指针*/
//#endif
//	Table *pZombieTab;        /* List of Table objects to delete after code gen 在生成之后再被删除的表 */
//	TriggerPrg *pTriggerPrg;  /* Linked list of coded triggers   */
//};

/*
** This routine is called when a new SQL statement is beginning to
** be parsed.  Initialize the pParse structure as needed.
**当一个SQL语句将要开始被解析的时候，下面的这个程序将要被调用，当我们在需要的时候有必要去初始化这个数据结构
*/
static void sqlite3BeginParse(Parse *pParse, int explainFlag){
	pParse->explain = (u8)explainFlag;  //解释标记位（无符号的一个byte）赋值给explain，并且进行了强制的类型转换，u8指的是无符号的一个byte
	                                    //解释标记位被置位意味着将要开始执行SQL语句
	pParse->nVar = 0;   //SQL语句中到目前为止被发现的"?"变量的数目
}

#ifndef SQLITE_OMIT_SHARED_CACHE
/*
** The TableLock structure is only used by the sqlite3TableLock() and
** codeTableLocks() functions.
**锁表结构仅仅被使用在 sqlite3TableLock() 和codeTableLocks()这两个函数中
*/
struct TableLock {
	int iDb;             /* The database containing the table to be locked */   //包含被锁定的表的数据库
	int iTab;            /* The root page of the table to be locked */          //表的根页面被锁定
	u8 isWriteLock;      /* True for write lock.  False for a read lock */      //读写锁，如果为真则是写锁，否则就是读锁
	const char *zName;   /* Name of the table */                                //将要锁定的表的名字
};

/*
** Record the fact that we want to lock a table at run-time.
**记录我们在运行过程中想要去锁定的表
** The table to be locked has root page iTab and is found in database iDb./被锁定的表的根页面，并且这个表在数据库iDb中被发现/
** A read or a write lock can be taken depending on isWritelock./读或写锁依赖于isWriteLock/
**
** This routine just records the fact that the lock is desired.  The
** code to make the lock occur is generated by a later call to
** codeTableLocks() which occurs during sqlite3FinishCoding().
**这个程序仅仅记录被期望的锁。
**这段代码在调用codeTableLocks()之后使锁发生，而codeTableLocks()发生于sqlite3FinishCoding()之中。
*/
void sqlite3TableLock(
	Parse *pParse,     /*Parsing context 语法分析上下文  函数的第一个参数*/
	int iDb,           /* Index of the database containing the table to lock  数据库中被锁定的表的索引，实际上就是表的索引，只不过这个表已经被锁定。所谓的被锁定就是指相应的标记为值1*/
	int iTab,          /* Root page number of the table to be locked   被锁定的表的根页面 */
	u8 isWriteLock,    /* True for a write lock 如果加了一个写操作锁则将该位置位为true*/
	const char *zName  /* Name of the table to be locked  被加锁的表的名字  函数的最后一个参数*/
	){
	Parse *pToplevel = sqlite3ParseToplevel(pParse);   //指向语法树顶层的指针
	int i;
	int nBytes;
	TableLock *p;    //锁表指针
	assert(iDb >= 0);    //断言数据库中包含这个被锁定的表的索引，也就说已经给这个被锁定的表建立了相应的索引，并且这个表能在数据库中被找到

	//下面的for循环是在检索锁表，并且决定是置成读锁还是写锁
	for (i = 0; i < pToplevel->nTableLock; i++){
		p = &pToplevel->aTableLock[i];   //取锁表数组的第i个单元的地址，将地址赋值给p（p指针实际上是锁表指针TableLock *p）指针
		if (p->iDb == iDb && p->iTab == iTab){
			p->isWriteLock = (p->isWriteLock || isWriteLock);
			return;
		}
	}

	nBytes = sizeof(TableLock) * (pToplevel->nTableLock + 1);
	pToplevel->aTableLock =
		sqlite3DbReallocOrFree(pToplevel->db, pToplevel->aTableLock, nBytes);
	if (pToplevel->aTableLock){
		p = &pToplevel->aTableLock[pToplevel->nTableLock++];
		p->iDb = iDb;
		p->iTab = iTab;
		p->isWriteLock = isWriteLock;
		p->zName = zName;
	}
	else{
		pToplevel->nTableLock = 0;
		pToplevel->db->mallocFailed = 1;
	}
}

/*
** Code an OP_TableLock instruction for each table locked by the
** statement (configured by calls to sqlite3TableLock()).
**通过调用sqlite3TableLock()，为每一个已经加锁的表编写一条OP_TableLock指令
*/
static void codeTableLocks(Parse *pParse){
	int i;
	Vdbe *pVdbe;

	pVdbe = sqlite3GetVdbe(pParse);
	assert(pVdbe != 0); /* sqlite3GetVdbe cannot fail: VDBE already allocated【sqlite3GetVdbe不能为失败：虚拟数据库引擎已经被占用】 */

	for (i = 0; i<pParse->nTableLock; i++){
		TableLock *p = &pParse->aTableLock[i];
		int p1 = p->iDb;
		sqlite3VdbeAddOp4(pVdbe, OP_TableLock, p1, p->iTab, p->isWriteLock,
			p->zName, P4_STATIC);
	}
}
#else
#define codeTableLocks(x)
#endif

/*
** This routine is called after a single SQL statement has been
** parsed and a VDBE program to execute that statement has been
** prepared.  当一个SQL语句被解析之后，sqlite3FinishCoding(Parse *pParse)将被调用，接着VDBE程序将执行这个已经准备好的语句
** This routine puts the finishing touches on the
** VDBE program and resets the pParse structure for the next
** parse.
**sqlite3FinishCoding(Parse *pParse)程序重置了pParse结构，为下一个parse做好准备，并且使VDBE程序结束。
** Note that if an error occurred, it might be the case that
** no VDBE code was generated.请注意，如果有错误发生，可能是这样的情况：VDBE代码没有生成。
*/
void sqlite3FinishCoding(Parse *pParse){
	sqlite3 *db;
	Vdbe *v;      //VDBE指针v，后面的程序中多次用到这个指针，用于指向VDBE，看到这里请去熟悉在sqlite数据库中为什么药使用VDBE以及VDBE的功能是什么？否则不能正确的理解下面的代码

	db = pParse->db;               //将pParse中的db指针指向sqlite3的db指针
	if (db->mallocFailed) return;  //如果内存申请失败，则退出
	if (pParse->nested) return;    //如果发生嵌套，则退出
	if (pParse->nErr) return;      //如果发生错误，则退出

	/* Begin by generating some termination code at the end of the
	** vdbe program  
	**在VDBE程序的结束阶段生成一些终端代码
	*/
	v = sqlite3GetVdbe(pParse);
	assert(!pParse->isMultiWrite
		|| sqlite3VdbeAssertMayAbort(v, pParse->mayAbort));
	if (v){
		sqlite3VdbeAddOp0(v, OP_Halt);    //增加指令OP_Halt

		/* The cookie mask contains one bit for each database file open.
		** (Bit 0 is for main, bit 1 is for temp, and so forth.)  Bits are
		** set for each database that is used.  Generate code to start a
		** transaction on each used database and to verify the schema cookie
		** on each used database.【这个cookie隐含一个bit值为每一个已经打开的数据库文件。
		（bit==0时意思是：主数据库文件，bit==1时意思是：临时数据库文件，等等。）
		bit值是为每一个被使用数据库设置。每个使用过的数据库产生代码来开启一个事务，并验证模式cookie。】
		*/
		if (pParse->cookieGoto>0){
			yDbMask mask;
			int iDb;
			sqlite3VdbeJumpHere(v, pParse->cookieGoto - 1);
			for (iDb = 0, mask = 1; iDb<db->nDb; mask <<= 1, iDb++){
				if ((mask & pParse->cookieMask) == 0) continue;
				sqlite3VdbeUsesBtree(v, iDb);      //使用Btree
				sqlite3VdbeAddOp2(v, OP_Transaction, iDb, (mask & pParse->writeMask) != 0);  //增加指令OP_Transaction
				if (db->init.busy == 0){
					assert(sqlite3SchemaMutexHeld(db, iDb, 0));    //已经占用临界区，因为数据库模式互斥量已经被得到
					sqlite3VdbeAddOp3(v, OP_VerifyCookie,   
						iDb, pParse->cookieValue[iDb],
						db->aDb[iDb].pSchema->iGeneration);
				}   //增加了第三个操作OP_VerifyCookie，并且模式已经生成
			}
#ifndef SQLITE_OMIT_VIRTUALTABLE
	  {
		  int i;
		  for (i = 0; i<pParse->nVtabLock; i++){
			  char *vtab = (char *)sqlite3GetVTable(db, pParse->apVtabLock[i]);
			  sqlite3VdbeAddOp4(v, OP_VBegin, 0, 0, 0, vtab, P4_VTAB);     //增加操作OP_VBegin
		  }
		  pParse->nVtabLock = 0;
	  }
#endif

	  /* Once all the cookies have been verified and transactions opened,
	  ** obtain the required table-locks. This is a no-op unless the
	  ** shared-cache feature is enabled.
	  **一旦所有的cookies被验证完，所有的事务被打开，并且获得所需的锁表。这是一个空操作，除非启用共享缓存特性。
	  */
	  codeTableLocks(pParse);

	  /* Initialize any AUTOINCREMENT data structures required.
	  **初始化一些被需要的自增的数据结构 
	  *
	  sqlite3AutoincrementBegin(pParse);    //自动开始下一个SQL语句的解析

	  /* Finally, jump back to the beginning of the executable code. 
	  **最后，跳回到可执行代码的开端。
	  */
	  sqlite3VdbeAddOp2(v, OP_Goto, 0, pParse->cookieGoto);   //给VDBE增加指令OP_Goto
		}   
	}


	/* Get the VDBE program ready for execution
	**获取VDBE项目为运行做准备。
	*/

	//在正确的获得VDBE指针，并且pParse没有错误，且内存申请成功的情况下，进行下面的操作
	if (v && ALWAYS(pParse->nErr == 0) && !db->mallocFailed){
#ifdef SQLITE_DEBUG
		FILE *trace = (db->flags & SQLITE_VdbeTrace) != 0 ? stdout : 0;
		sqlite3VdbeTrace(v, trace);
#endif
		assert(pParse->iCacheLevel == 0);  
		/* Disables and re-enables match  无效，重新启用匹配 */
		/* A minimum of one cursor is required if autoincrement is used   
		**如果自动增量被用过，至少需要一个光标。
		*  See ticket [a696379c1f08866] */
		if (pParse->pAinc != 0 && pParse->nTab == 0) pParse->nTab = 1;
		sqlite3VdbeMakeReady(v, pParse);
		pParse->rc = SQLITE_DONE;
		pParse->colNamesSet = 0;
	}
	else{
		pParse->rc = SQLITE_ERROR;
	}
	pParse->nTab = 0;
	pParse->nMem = 0;
	pParse->nSet = 0;
	pParse->nVar = 0;
	pParse->cookieMask = 0;
	pParse->cookieGoto = 0;
}

/*
** Run the parser and code generator recursively in order to generate
** code for the SQL statement given onto the end of the pParse context
** currently under construction.  When the parser is run recursively
** this way, the final OP_Halt is not appended and other initialization
** and finalization steps are omitted because those are handling by the
** outermost parser.
**解析器和生成器递归运行为了生成代码到给定的SQL语句去结束pParse目前正在建设的上下文。当parser以这种方式递归运行时，最后OP_Halt是没有被附加的，还有其他的初始值和
**终止步调时被忽略的，因为它们正在被parser的最外层处理。
**
** Not everything is nestable.  This facility is designed to permit
** INSERT, UPDATE, and DELETE operations against SQLITE_MASTER.  Use
** care if you decide to try to use this routine for some other purposes.
**不是每个操作都是嵌套的，这个设备的目的是允许插入，更新和删除操作不适用于SQLITE_MASTER。如果你试图把这个例程用于一些其他目的，那么你用的时候要小心了。
*/

//嵌套解析函数，用于对插入、删除、更新操作进行嵌套的解析
void sqlite3NestedParse(Parse *pParse, const char *zFormat, ...){
	va_list ap;    //可变参数列表的使用
	char *zSql;
	char *zErrMsg = 0;  //错误信息指针
	sqlite3 *db = pParse->db;
# define SAVE_SZ  (sizeof(Parse) - offsetof(Parse,nVar))
	char saveBuf[SAVE_SZ];   //保存信息的缓冲区

	if (pParse->nErr) return;  //如果解析有错误，则直接停止这个嵌套的程序
	assert(pParse->nested<10);  /* Nesting should only be of limited depth  断言改嵌套解析函数的深度小于10，如果大于等于10，则断言失败，程序立刻停止 */
	va_start(ap, zFormat);    //开始读取可变参数列表中的参数
	zSql = sqlite3VMPrintf(db, zFormat, ap);  
	va_end(ap);   //可变参数列表使用结束
	if (zSql == 0){
		return;   /* A malloc must have failed 分配内存失败，结束整个程序*/
	}
	pParse->nested++;    //嵌套的深度+1
	memcpy(saveBuf, &pParse->nVar, SAVE_SZ);   //把SAVE_SZ大小的内容从&pParse->nVar拷贝到saveBuf
	memset(&pParse->nVar, 0, SAVE_SZ);    
	sqlite3RunParser(pParse, zSql, &zErrMsg);       //运行parser
	sqlite3DbFree(db, zErrMsg);         //数据库释放
	sqlite3DbFree(db, zSql);
	memcpy(&pParse->nVar, saveBuf, SAVE_SZ);      //把SAVE_SZ大小的内容从saveBuf拷贝到&pParse->nVar
	pParse->nested--;
}

/*
** Locate the in-memory structure that describes a particular database
** table given the name of that table and (optionally) the name of the
** database containing the table.  Return NULL if not found.
**定位描述一个特定的数据库表的内存结构，给出这个特殊的表的名字和（可选）数据库的名称，这个数据库包含这个表格。如果没有找到返回0.
** If zDatabase is 0, all databases are searched for the table and the
** first matching table is returned.  (No checking for duplicate table
** names is done.)  The search order is TEMP first, then MAIN, then any
** auxiliary databases added using the ATTACH command.
**如果zDatabase是0，在所有的数据库中查找这个表，返回第一个匹配的表。（对重复的表名不做检查）查找的顺序是：首先是临时文件，
**然后是主文件，然后是一些辅助数据库，添加辅助数据库是用于ATTACH命令。
**
** See also sqlite3LocateTable().
*/

/*  junpeng zhu created 
**寻找数据库表函数：sqlite3FindTable，并且返回这个表的指针，参数zDatabase去指定数据库，避免去查找所有的数据库；ZName要查找的表的名字，z代表的是zero，也就是是否为空,
**如果查找的表的名字也为空的话是没有必要查找的。这两个参数都是const类型的，即不允许程序显式的去修改这两个参数
*/
Table *sqlite3FindTable(sqlite3 *db, const char *zName, const char *zDatabase){
	Table *p = 0;     //建立这个表的指针，保存表的首地址
	int i;
	int nName;
	assert(zName != 0);    //断言这个表名不是0，也就是说表名不为空，如果为空的话查找是没有效果的，也是没有必要的操作
	nName = sqlite3Strlen30(zName);    
	/* All mutexes are required for schema access.  Make sure we hold them. 所有需要互斥访问模式。确保我们拥有他们。加锁的操作*/
	assert(zDatabase != 0 || sqlite3BtreeHoldsAllMutexes(db));   //断言确实指定了数据库，否则将要去扫描所有的数据库去寻找指定的表，这简直是浪费时间
	for (i = OMIT_TEMPDB; i<db->nDb; i++){
		int j = (i<2) ? i ^ 1 : i;   /* Search TEMP before MAIN    界定临时文件与主文件的查找顺序*/
		if (zDatabase != 0 && sqlite3StrICmp(zDatabase, db->aDb[j].zName)) continue;
		assert(sqlite3SchemaMutexHeld(db, j, 0));   //断言这个表模式已经完全获得了临界区 ，临界区是互斥访问的      
		p = sqlite3HashFind(&db->aDb[j].pSchema->tblHash, zName, nName);    //进行的是hash查找算法
		if (p) break;   //如果找到了，则结束查找工作
	}
	return p;   //如果找到的话，返回这个表的地址
}

/*
** Locate the in-memory structure that describes a particular database
** table given the name of that table and (optionally) the name of the
** database containing the table.  Return NULL if not found.  Also leave an
** error message in pParse->zErrMsg.
**定位描述一个特定的数据库表的内存结构，给出这个特殊的表的名字和（可选）数据库的名称，这个数据库包含这个表格。如果没有找到返回0.在pParse->zErrMsg中留下一个错误信息。
** The difference between this routine and sqlite3FindTable() is that this
** routine leaves an error message in pParse->zErrMsg where
** sqlite3FindTable() does not.
**这个例程和sqlite3FindTable()的区别是：这个例程留下一个错误信息在pParse->zErrMsg中，而sqlite3FindTable()没有。
*/
Table *sqlite3LocateTable(
	Parse *pParse,         /* context in which to report errors 【上下文来报告错误】*/
	int isView,            /* True if looking for a VIEW rather than a TABLE  如果找到的是视图而不是表，则值是true */
	const char *zName,     /* Name of the table we are looking for 【查找的表名】*/
	const char *zDbase     /* Name of the database.  Might be NULL【数据库的名字，可能是空】 */
	){
	Table *p;    //表指针，返回这个表的地址

	/* Read the database schema. If an error occurs, leave an error message
	** and code in pParse and return NULL.   读取数据库模式，如果有错误发生，留下一个错误信息和代码在pParse，然后返回NULL。 */
	if (SQLITE_OK != sqlite3ReadSchema(pParse)){
		return 0;
	}

	p = sqlite3FindTable(pParse->db, zName, zDbase);    //调用寻找表的函数
	if (p == 0){
		const char *zMsg = isView ? "no such view" : "no such table";   //判断得到的视图还是表
		if (zDbase){   //如果数据库不为空
			sqlite3ErrorMsg(pParse, "%s: %s.%s", zMsg, zDbase, zName);    //输出相应的信息
		}
		else{
			sqlite3ErrorMsg(pParse, "%s: %s", zMsg, zName);    //如果数据库为空，并且没有找到相应的表，则输出相应的信息
		}
		pParse->checkSchema = 1;
	}
	return p;     //返回指针
}

/*
** Locate the in-memory structure that describes
** a particular index given the name of that index
** and the name of the database that contains the index.
** Return NULL if not found.
**定位描述一个特定的索引的内存结构，给出这个特殊的索引的名字和数据库的名称，这个数据库包含这个索引。如果没有找到返回0.
**If zDatabase is 0, all databases are searched for the
** table and the first matching index is returned.  (No checking
** for duplicate index names is done.)  The search order is
** TEMP（临时文件夹） first, then MAIN（主要文件夹）, then any auxiliary databases added
** using the ATTACH command.（使用附加命令添加到任何辅助数据库。）

**找到的内存结构，它描述给该索引的名称，并包含index.Return NULL，如果没有找到该数据库的名称特定的索引。如果zDatabase为0，
**所有的数据库搜索的表和第一个匹配的索引返回。
**（没有检查是否有重复的索引名称就完成了。）搜索顺序是TEMP文件，再主文件，然后加入使用attach命令任何辅助数据库。
*/

/*
**寻找索引，提高查询的速度，其中zDb参数是表明数据库是否为空的参数，采用的是const参数，不允许在程序中去修改这个参数；
**zName是索引名字，也采用的是const参数，表明我们在程序中是不能随便取更改这个参数的。
*/
Index *sqlite3FindIndex(sqlite3 *db, const char *zName, const char *zDb){
	Index *p = 0;  //索引指针，返回这个索引的地址
	int i;
	int nName = sqlite3Strlen30(zName);     //Sqlite索引的名字
	/* All mutexes are required for schema access.  Make sure we hold them.   互斥的访问临界区，独立的拥有临界区的访问权 */
	assert(zDb != 0 || sqlite3BtreeHoldsAllMutexes(db));   
	for (i = OMIT_TEMPDB; i<db->nDb; i++){
		int j = (i<2) ? i ^ 1 : i;  /* Search TEMP before MAIN   确定查找的顺序，先查找临时文件后查找主文件*/
		Schema *pSchema = db->aDb[j].pSchema;   
		assert(pSchema);
		if (zDb && sqlite3StrICmp(zDb, db->aDb[j].zName)) continue;
		assert(sqlite3SchemaMutexHeld(db, j, 0));   //断言该模式已经完全的拥有了临界区
		p = sqlite3HashFind(&pSchema->idxHash, zName, nName);   //采用的是hash查找
		if (p) break;   //如果查找到了相应的索引，则终止这个循环的查找
	}
	return p;
}

/*
** Reclaim the memory used by an index 回收被所以使用的内存
*/
static void freeIndex(sqlite3 *db, Index *p){
#ifndef SQLITE_OMIT_ANALYZE
	sqlite3DeleteIndexSamples(db, p);//删除参数范例
#endif
	sqlite3DbFree(db, p->zColAff);
	sqlite3DbFree(db, p);//释放数据库连接
}

/*
** For the index called zIdxName which is found in the database iDb,
** unlike that index from its Table then remove the index from
** the index hash table and free all memory structures associated
** with the index.
**zIdexName这个索引被在数据库iDb中找到，不同于其表的索引，它（zIdexName）需要从哈希索引文件中移除并释放所有的与这个索引相关的内存数据结构。
*/

/* junpeng zhu created
**删除与参数zIdexName相关的所有的内存数据结构,参数db是当前数据库的指针，指向现在正在操作的数据库；iDb是这个索引所在的数据库；zIdxName是要删除的索引，与其相关的内存数据结构也要全部删除。
*/
void sqlite3UnlinkAndDeleteIndex(sqlite3 *db, int iDb, const char *zIdxName){
	Index *pIndex;  
	int len;
	Hash *pHash;

	assert(sqlite3SchemaMutexHeld(db, iDb, 0));   //断言数据库iDb拥有了完全的临界区控制权
	pHash = &db->aDb[iDb].pSchema->idxHash;
	len = sqlite3Strlen30(zIdxName);  //Len获取zIdxName前30个字符。
	pIndex = sqlite3HashInsert(pHash, zIdxName, len, 0);
	if (ALWAYS(pIndex)){
		if (pIndex->pTable->pIndex == pIndex){
			pIndex->pTable->pIndex = pIndex->pNext;
		}
		else{
			Index *p;
			/* Justification of ALWAYS();  The index must be on the list of
			** indices. 【论证ALWAYS()函数】*/
			p = pIndex->pTable->pIndex;
			while (ALWAYS(p) && p->pNext != pIndex){ p = p->pNext; }
			if (ALWAYS(p && p->pNext == pIndex)){
				p->pNext = pIndex->pNext;
			}
		}
		freeIndex(db, pIndex);  //回收被索引使用的内存
	}
	db->flags |= SQLITE_InternChanges;
}

/*
** Look through the list of open database files in db->aDb[] and if
** any have been closed, remove them from the list.  Reallocate the
** db->aDb[] structure to a smaller size, if possible.
**扫描打开的数据库的文件，如果有任何一个被关闭的，从这个表中移除他们。如果可能的话重新分配这个数据结构的内存空间，这样可以降低内存的无效使用
** Entry 0 (the "main" database) and entry 1 (the "temp" database)
** are never candidates for being collapsed.
**main数据库和temp临时数据库将不再上面移除的数据库考虑范围之内
*/
/*junpeng zhu created
收缩数据库，将已经关闭的数据库移除内存数据结构，节省内存的无效使用,其中参数db指向当前的数据库
*/
void sqlite3CollapseDatabaseArray(sqlite3 *db){
	int i, j;   //参数i指向当前正在查找的数据库的内存编号，是以数组存储的；j一直是指向当前数据库的内存编号
	for (i = j = 2; i<db->nDb; i++){   //参数nDb指示数据库的个数
		struct Db *pDb = &db->aDb[i];//打开数据库文件
		if (pDb->pBt == 0){//如果pDb->pBt是0则说明数据库被关闭
			sqlite3DbFree(db, pDb->zName);//释放内存
			pDb->zName = 0;
			continue;
		}
		if (j<i){   
			db->aDb[j] = db->aDb[i]; //如果查找到了已经关闭的数据库，从内存数据结构中移除之后，后面的数据库的指针应该一次向前进行调整
		}
		j++;
	}
	memset(&db->aDb[j], 0, (db->nDb - j)*sizeof(db->aDb[j]));//将&db->aDb[j]中前(db->nDb-j)*sizeof(db->aDb[j])个字节置0。
	db->nDb = j;//改变数据库数目
	if (db->nDb <= 2 && db->aDb != db->aDbStatic){//只有一个数据库而且不是aDbStatic
		memcpy(db->aDbStatic, db->aDb, 2 * sizeof(db->aDb[0]));
		sqlite3DbFree(db, db->aDb);
		db->aDb = db->aDbStatic;
	}
}

/*
** Reset the schema for the database at index iDb.  Also reset the
** TEMP schema.【重置数据库索引iDb的模式。也重置临时模式。】
*/
void sqlite3ResetOneSchema(sqlite3 *db, int iDb){
	Db *pDb;
	assert(iDb<db->nDb);

	/* Case 1:  Reset the single schema identified by iDb 【重置iDB鉴定的单一模式】*/
	pDb = &db->aDb[iDb];
	assert(sqlite3SchemaMutexHeld(db, iDb, 0));
	assert(pDb->pSchema != 0);//0为main，1为临时
	sqlite3SchemaClear(pDb->pSchema);

	/* If any database other than TEMP is reset, then also reset TEMP
	** since TEMP might be holding triggers that reference tables in the
	** other database.
	【如果重置TEMP以外的任何数据库，那么TEMP也重置，因为TEMP可能引用其他数据库中的参照表的触发器】
	*/
	if (iDb != 1){
		pDb = &db->aDb[1];
		assert(pDb->pSchema != 0);
		sqlite3SchemaClear(pDb->pSchema);
	}
	return;
}

/*
** Erase all schema information from all attached databases (including
** "main" and "temp") for a single database connection.
【删除所有附加模式信息数据库（包括“main”和“temp”）为一个数据库连接】
*/
void sqlite3ResetAllSchemasOfConnection(sqlite3 *db){
	int i;
	sqlite3BtreeEnterAll(db);//通过一个数据库加载所有的Btree
	for (i = 0; i<db->nDb; i++){//循环查找出所有的数据库并对数据库模式进行操作
		Db *pDb = &db->aDb[i];
		if (pDb->pSchema){
			sqlite3SchemaClear(pDb->pSchema);
		}
	}
	db->flags &= ~SQLITE_InternChanges;
	sqlite3VtabUnlockList(db);//
	sqlite3BtreeLeaveAll(db);//去掉加载的数据库
	sqlite3CollapseDatabaseArray(db);//解除数据库数组
}

/*
** This routine is called when a commit occurs.【当提交时调用这段程序。】
*/
void sqlite3CommitInternalChanges(sqlite3 *db){
	db->flags &= ~SQLITE_InternChanges;
}

/*
** Delete memory allocated for the column names of a table or view (the
** Table.aCol[] array).【删除分配给一个表的列名或试图的内存。】
*/
static void sqliteDeleteColumnNames(sqlite3 *db, Table *pTable){
	int i;
	Column *pCol;
	assert(pTable != 0);//如果表不为空继续执行，如果为空结束执行
	if ((pCol = pTable->aCol) != 0){//列不为空
		for (i = 0; i<pTable->nCol; i++, pCol++){//删除分配给一个表的列名或试图里面的信息。
			sqlite3DbFree(db, pCol->zName);
			sqlite3ExprDelete(db, pCol->pDflt);
			sqlite3DbFree(db, pCol->zDflt);
			sqlite3DbFree(db, pCol->zType);
			sqlite3DbFree(db, pCol->zColl);
		}
		sqlite3DbFree(db, pTable->aCol);//释放表的列
	}
}

/*
** Remove the memory data structures associated with the given
** Table.  No changes are made to disk by this routine.
**删除与表相关联的内存数据结构，但是这个程序不改变磁盘
**
** This routine just deletes the data structure.  It does not unlink
** the table data structure from the hash table.  But it does destroy
** memory structures of the indices and foreign keys associated with
** the table.
**这段程序仅仅删除数据结构。此段程序不拆开来自哈希表的表数据结构（哈希表的数据结构在删除的时候要删除与之相关联的所有内存数据结构）。但是它破坏了索引内存结构和与这个表相关联的外键。
** The db parameter is optional.  It is needed if the Table object
** contains lookaside memory.  (Table objects in the schema do not use
** lookaside memory, but some ephemeral Table objects do.)  Or the
** db parameter can be used with db->pnBytesFreed to measure the memory
** used by the Table object.
**这个db是可选的。如果表对象包含后备存储器db就是需要的。（模式下的表对象不用后备存储器，但是一些短暂的表对象是需要用后备存储器的。）
**或db参数可以使用db - > pnBytesFreed衡量表对象使用的内存。】
*/
void sqlite3DeleteTable(sqlite3 *db, Table *pTable){//不拆开给定的哈希表的表结构，还有删除这个表结构以及与该表相关联的所有索引和外键。
	Index *pIndex, *pNext;
	TESTONLY(int nLookaside;) /* Used to verify lookaside not used for schema  用于验证后备不用于模式*/

		assert(!pTable || pTable->nRef>0);

	/* Do not delete the table until the reference count reaches zero. 如果正在使用这个表则不会删除，如果没有引用那么就删除这个表，是否引用这个表用count参数指定 */
	if (!pTable) return;
	if (((!db || db->pnBytesFreed == 0) && (--pTable->nRef)>0)) return;

	/* Record the number of outstanding lookaside allocations in schema Tables
	** prior to doing any free() operations.  Since schema Tables do not use
	** lookaside, this number should not change. 
	**记录突出的后备分配模式表的数量之前做一些free()操作。
	由于不使用后备模式表,这个数字不应该改变。*/
	TESTONLY(nLookaside = (db && (pTable->tabFlags & TF_Ephemeral) == 0) ?
		db->lookaside.nOut : 0);

	/* Delete all indices associated with this table. 删除和这个表有关联的所有索引。 */
	for (pIndex = pTable->pIndex; pIndex; pIndex = pNext){
		pNext = pIndex->pNext;
		assert(pIndex->pSchema == pTable->pSchema);   //断言这个表和索引确实是在同一个模式中，防止错误的删除表
		if (!db || db->pnBytesFreed == 0){   //数据库费空，并且趋势已经释放了这个表
			char *zName = pIndex->zName;
			TESTONLY(Index *pOld = ) sqlite3HashInsert(
				&pIndex->pSchema->idxHash, zName, sqlite3Strlen30(zName), 0
				);
			assert(db == 0 || sqlite3SchemaMutexHeld(db, 0, pIndex->pSchema));   //临界区的互斥访问
			assert(pOld == pIndex || pOld == 0);
		}
		freeIndex(db, pIndex);    //释放索引
	}

	/* Delete any foreign keys attached to this table. 删除依赖于这个表的任何外键。 */
	sqlite3FkDelete(db, pTable);

	/* 
	Delete the Table structure itself. 删除这个表结构。
	*/
	sqliteDeleteColumnNames(db, pTable);   //删除列名
	sqlite3DbFree(db, pTable->zName);
	sqlite3DbFree(db, pTable->zColAff);
	sqlite3SelectDelete(db, pTable->pSelect);
#ifndef SQLITE_OMIT_CHECK
	sqlite3ExprListDelete(db, pTable->pCheck);
#endif
#ifndef SQLITE_OMIT_VIRTUALTABLE
	sqlite3VtabClear(db, pTable);
#endif
	sqlite3DbFree(db, pTable);

	/* Verify that no lookaside memory was used by schema tables 验证没有后备存储器用于模式表格。  */
	assert(nLookaside == 0 || nLookaside == db->lookaside.nOut);
}

/*
** Unlink the given table from the hash tables and the delete the
** table structure with all its indices and foreign keys.【拆开给定的哈希表的表结构，还有删除这个表结构以及与该表相关联的所有索引和外键。】
*/
void sqlite3UnlinkAndDeleteTable(sqlite3 *db, int iDb, const char *zTabName){//拆开给定的哈希表的表结构，还有删除这个表结构以及与该表相关联的所有索引和外键。
	Table *p;
	Db *pDb;

	assert(db != 0);
	assert(iDb >= 0 && iDb<db->nDb);
	assert(zTabName);
	assert(sqlite3SchemaMutexHeld(db, iDb, 0));
	testcase(zTabName[0] == 0);  /* Zero-length table names are allowed */
	pDb = &db->aDb[iDb];
	p = sqlite3HashInsert(&pDb->pSchema->tblHash, zTabName,
		sqlite3Strlen30(zTabName), 0);//通过在数据库中存放表名的Hash找到表
	sqlite3DeleteTable(db, p);//删除数据库中的表
	db->flags |= SQLITE_InternChanges;
}

/*
** Given a token, return a string that consists of the text of that
** token.  Space to hold the returned string
** is obtained from sqliteMalloc() and must be freed by the calling
** function.
**给定一个符号,返回一个字符串,该字符串包含文本的符号。
**空间将返回的字符串从sqliteMalloc获得()，这个空间必须被调用函数释放。
**
** Any quotation marks (ex:  "name", 'name', [name], or `name`) that
** surround the body of the token are removed.
**这个符号周围的引号标志被清除
**
** Tokens are often just pointers into the original SQL text and so
** are not \000 terminated and are not persistent.  The returned string
** is \000 terminated and is persistent.
**符号一般只指向原始的SQL文本，因此这些符号并不是\000终止也不是持久的。
这个返回的字符串是\000终止而且是持久的。
*/
char *sqlite3NameFromToken(sqlite3 *db, Token *pName){//输入一个Token的数据返回一个字符串
	char *zName;
	if (pName){
		zName = sqlite3DbStrNDup(db, (char*)pName->z, pName->n);
		sqlite3Dequote(zName);
	}
	else{
		zName = 0;
	}
	return zName;
}

/*
** Open the sqlite_master table stored in database number iDb for
** writing. The table is opened using cursor 0.打开存储在数据库iDb中的sqlite_master表，为写操作做准备，这个被打开的表使用的游标是0.
*/
void sqlite3OpenMasterTable(Parse *p, int iDb){
	Vdbe *v = sqlite3GetVdbe(p);
	sqlite3TableLock(p, iDb, MASTER_ROOT, 1, SCHEMA_TABLE(iDb));//打开锁
	sqlite3VdbeAddOp3(v, OP_OpenWrite, 0, MASTER_ROOT, iDb);//可以进行写的操作
	sqlite3VdbeChangeP4(v, -1, (char *)5, P4_INT32);  /* 5 column table */
	if (p->nTab == 0){
		p->nTab = 1;
	}
}

/*
** Parameter zName points to a nul-terminated buffer containing the name
** of a database ("main", "temp" or the name of an attached db). This
** function returns the index of the named database in db->aDb[], or
** -1 if the named db cannot be found.
** 参数zName指针指向一个空终止缓冲区（也就是说这个缓冲区是以\000结束的，并且使字符串类型的），这个缓冲区包括的数据库有：main数据库、临时数据库、和一个attached 数据库
** The token *pName contains the name of a database (either "main" or
** "temp" or the name of an attached db). This routine returns the
** index of the named database in db->aDb[], or -1 if the named db
** does not exist
**这个*pName 符号包含一个数据的名称。这个函数用db->aDb[]返回被指定数据库的索引，如果这个被指定的数据库db不存在，则返回-1。
*/
int sqlite3FindDb(sqlite3 *db, Token *pName){//通过数据的名字返回数据库的索引
	int i;                               /* 数据库的索引 */
	char *zName;                         /*数据库名字 */
	zName = sqlite3NameFromToken(db, pName);//转换字符串
	i = sqlite3FindDbName(db, zName);//查找索引
	sqlite3DbFree(db, zName);
	return i;
}

/* The table or view or trigger name is passed to this routine via tokens
** pName1 and pName2. If the table name was fully qualified, for example:
**
** CREATE TABLE xxx.yyy (...).Then pName1 is set to "xxx" and pName2 "yyy". 
**表名，视图名，或是触发器名被传给这个例程，通道符号为pName1和pName2。如果表名完全被限制了，比如说：CREATE TABLE xxx.yyy (...)，那么pName1被设置为xxx，pName2被设置为yyy
**On the other hand if the table name is not fully qualified, i.e.:
**CREATE TABLE yyy(...);Then pName1 is set to "yyy" and pName2 is "".
**另一方面如果表名没有被完全限制，例如：CREATE TABLE yyy(...);那么pName1被设置为yyy，而此时的pName2被设置为“”。

**This routine sets the **ppUnqual pointer to point at the token (pName1 or
** pName2) that stores the unqualified table name.  The index of the
** database "xxx" is returned.   
**这个例程设定*ppUnqual指针指向（pName1 or pName2），这两个指针储存的是没有被完全限制的表名。数据库"xxx"的索引被返回。
**
*/
int sqlite3TwoPartName(
	Parse *pParse,      /* Parsing and code generating context 【解析和代码生成上下文】*/
	Token *pName1,      /* The "xxx" in the name "xxx.yyy" or "xxx" */
	Token *pName2,      /* The "yyy" in the name "xxx.yyy" */
	Token **pUnqual     /* Write the unqualified object name here 【写没有完全被限制的目标名称】*/
	){
	int iDb;                    /* Database holding the object【数据库的对象】 */
	sqlite3 *db = pParse->db;

	if (ALWAYS(pName2 != 0) && pName2->n>0){
		if (db->init.busy) {//数据库初始化正在占用
			sqlite3ErrorMsg(pParse, "corrupt database");   //发送错误信息
			pParse->nErr++;  
			return -1;
		}
		**pUnqual = pName2;
		iDb = sqlite3FindDb(db, pName1);//通过名字查找数据库的索引
		if (iDb<0){//数据库不存在
			sqlite3ErrorMsg(pParse, "unknown database %T", pName1);
			pParse->nErr++;
			return -1;
		}
	}
	else{
		assert(db->init.iDb == 0 || db->init.busy);
		iDb = db->init.iDb;
		*pUnqual = pName1;
	}
	return iDb;//返回数据库索引
}
/*
** This routine is used to check if the UTF-8 string zName is a legal
** unqualified name for a new schema object (table, index, view or
** trigger). All names are legal except those that begin with the string
** "sqlite_" (in upper, lower or mixed case). This portion of the namespace
** is reserved for internal use.  
**这个例程是用于检查UTF-8字符串zName是否是完全合法的名字对于一个模式对象（表，索引，视图或是触发器）。
**所有的名字都是合法的除了那些以sqlite_开头的字符串。命名空间的部分被留给内部使用。】
*/

/*junpeng zhu created
**检查数据库是否是合法的，包括其中包含的表、索引、视图、触发器。
*/
int sqlite3CheckObjectName(Parse *pParse, const char *zName){//检测对象的名字是否合法
	if (!pParse->db->init.busy && pParse->nested == 0    //如果数据库不忙，并且没有嵌套的解析，并且没有处于数据库的写模式，且数据库的前7位不是sqlite_
		&& (pParse->db->flags & SQLITE_WriteSchema) == 0
		&& 0 == sqlite3StrNICmp(zName, "sqlite_", 7)){//比较字符串的前七为是不是“sqlite_”
		sqlite3ErrorMsg(pParse, "object name reserved for internal use: %s", zName);  //部分命名空间被保留给数据库的内部使用
		return SQLITE_ERROR;
	}
	return SQLITE_OK; 

}

/*
** Begin constructing a new table representation in memory.  This is
** the first of several action routines that get called in response
** to a CREATE TABLE statement.  
** 开始构造一个新的表在内存中的表示。这是第一次去调用多个程序去响应CREATE TABLE语句。
** In particular, this routine is called
** after seeing tokens "CREATE" and "TABLE" and the table name. 
** 尤其是，这个程序在看到“CREATE” 和“TABLE”还有一个表名的时候，表示这个程序将会被调用

** The isTemp flag is true if the table should be stored in the auxiliary database
** file instead of in the main database file.  
** 如果这个表被存储在辅助数据库中而不是在主数据库文件中，那么此时的这个标志isTemp应该被置为真（True）
** This is normally the case when the "TEMP" or "TEMPORARY" keyword occurs in between CREATE and TABLE。
**"TEMP" or "TEMPORARY"关键字在CREATE 和 TABLE之间出现这种情况是正常的。

** The new table record is initialized and put in pParse->pNewTable.
** 初始化新表记录,放入pParse - > pNewTable。
** As more of the CREATE TABLE statement is parsed, additional action  如果更多的CREATE TABLE 语句被解析，
** routines will be called to add more information to this record.   额外的行为例程将会被调用去增加更多的信息为这个记录。 
** At the end of the CREATE TABLE statement, the sqlite3EndTable() routine
** is called to complete the construction of the new table record. CREATE TABLE语句最后，调用sqlite3EndTable()去完成新表记录的构建。
*/
void sqlite3StartTable(
	Parse *pParse,   /* Parser context  语法分析程序的上下文 */
	Token *pName1,   /* First part of the name of the table or view  表或视图名字的第一部分*/
	Token *pName2,   /* Second part of the name of the table or view 表或视图名字的第二部分*/
	int isTemp,      /* True if this is a TEMP table如果是临时表为true */
	int isView,      /* True if this is a VIEW 如果是视图时为true*/
	int isVirtual,   /* True if this is a VIRTUAL table  如果是虚拟表格为true */
	int noErr        /* Do nothing if table already exists  如果表已经存在什么也不做*/
	){
	Table *pTable;
	char *zName = 0; /* The name of the new table  新建表的表名*/
	sqlite3 *db = pParse->db;
	Vdbe *v;
	int iDb;         /* Database number to create the table in 记录数据库中创建表的数量  */
	Token *pName;    /* Unqualified name of the table to create   创建不规范的表名 */

	/* The table or view name to create is passed to this routine via tokens
	** pName1 and pName2. If the table name was fully qualified, for example:
	**
	** CREATE TABLE xxx.yyy (...);
	**
	** Then pName1 is set to "xxx" and pName2 "yyy". On the other hand if
	** the table name is not fully qualified, i.e.:
	**
	** CREATE TABLE yyy(...);
	** 表名，视图名，被传给这个例程通道符号pName1和pName2。如果表名完全被限制了，
	**比如说：CREATE TABLE xxx.yyy (...);
	**Then pName1 is set to "xxx" and pName2 "yyy".另一方面如果表名没有被完全限制，i.e.;
	**CREATE TABLE yyy(...);】
	** Then pName1 is set to "yyy" and pName2 is "".
	**
	** The call below sets the pName pointer to point at the token (pName1 or
	** pName2) that stores the unqualified table name. The variable iDb is
	** set to the index of the database that the table or view is to be
	** created in. 访问接下来设定pName指针指向pName1或pName2,这两个符号存储的是没有被限制的表名。这个变量iDb是对数据库的索引进行设置，表或试图是在数据库中创建。
	*/
	iDb = sqlite3TwoPartName(pParse, pName1, pName2, &pName);//把合格的xxx.yyy拆分，pName1 =xxx，pName2=yyy，返回数据库索引
	if (iDb<0) return;//iDb表示创建的数据库的数目，小于0表示没有相应的数据库被创建
	if (!OMIT_TEMPDB && isTemp && pName2->n>0 && iDb != 1){
		/* 
		**If creating a temp table, the name may not be qualified. Unless
		** the database name is "temp" anyway.
		**如果创建的是一个临时表，这个名字也许没有被限制使用。除非这个数据库名也是“临时的”。
		*/
		sqlite3ErrorMsg(pParse, "temporary table name must be unqualified");
		return;
	}
	if (!OMIT_TEMPDB && isTemp) iDb = 1;

	pParse->sNameToken = *pName;
	zName = sqlite3NameFromToken(db, pName);//把pName转换成字符串
	if (zName == 0) return;//zName代表新表是否被创建，如果没有则结束程序
	if (SQLITE_OK != sqlite3CheckObjectName(pParse, zName)){//如果新建的表名不合法（或者是以sqlist_开头），直接跳转
		goto begin_table_error;
	}
	if (db->init.iDb == 1) isTemp = 1;//如果数据库是临时的，所建的表也是临时的
#ifndef SQLITE_OMIT_AUTHORIZATION
	assert((isTemp & 1) == isTemp);
	{
		int code;
		char *zDb = db->aDb[iDb].zName;
		if (sqlite3AuthCheck(pParse, SQLITE_INSERT, SCHEMA_TABLE(isTemp), 0, zDb)){//检测角色是否有读写权限
			goto begin_table_error;
		}
		if (isView){//如果创建的是视图
			if (!OMIT_TEMPDB && isTemp){   //如果没有忽略临时数据库，并且视图是临时的
				code = SQLITE_CREATE_TEMP_VIEW;//创建临时视图
			}
			else{
				code = SQLITE_CREATE_VIEW;//创建非临时视图
			}
		}
		else{//否则创建的是表
			if (!OMIT_TEMPDB && isTemp){
				code = SQLITE_CREATE_TEMP_TABLE;//创建临时表
			}
			else{
				code = SQLITE_CREATE_TABLE;//创建非临时表
			}
		}
		if (!isVirtual && sqlite3AuthCheck(pParse, code, zName, 0, zDb)){//检测角色是否有创建视图、临时视图、表、临时表的权限
			goto begin_table_error;
		}
	}
#endif

	/* Make sure the new table name does not collide with an existing
	** index or table name in the same database.  Issue an error message if
	** it does. The exception is if the statement being parsed was passed
	** to an sqlite3_declare_vtab() call. In that case only the column names
	** and types will be used, so there is no need to test for namespace
	** collisions.
	**在同一个数据库中确保新的表名与已经存在的视图或表名不冲突。如果发生冲突则发出一个错误信息。
	**有个例外是如果被解析的语句传递给一个sqlite3_declare_vtab()调用。在这种情况下只有列名称和类型将被使用,所以没有必要测试名称空间冲突。
	*/
	if (!IN_DECLARE_VTAB){
		char *zDb = db->aDb[iDb].zName;
		if (SQLITE_OK != sqlite3ReadSchema(pParse)){
			goto begin_table_error;
		}
		pTable = sqlite3FindTable(db, zName, zDb);//通过数据库名和表名找到表
		if (pTable){//如果在数据库中通过表明找到了表
			if (!noErr){
				sqlite3ErrorMsg(pParse, "table %T already exists", pName);
			}
			else{
				assert(!db->init.busy);
				sqlite3CodeVerifySchema(pParse, iDb);
			}
			goto begin_table_error;
		}
		if (sqlite3FindIndex(db, zName, zDb) != 0){//查找在数据库中是否有索引的名字为zName
			sqlite3ErrorMsg(pParse, "there is already an index named %s", zName);
			goto begin_table_error;
		}
	}

	pTable = sqlite3DbMallocZero(db, sizeof(Table));//数据库为新建的表分配内存
	if (pTable == 0){//内存分配失败
		db->mallocFailed = 1;
		pParse->rc = SQLITE_NOMEM;
		pParse->nErr++;
		goto begin_table_error;
	}
	pTable->zName = zName;//表名
	pTable->iPKey = -1;//没有主键
	pTable->pSchema = db->aDb[iDb].pSchema;//和数据库的模式一样
	pTable->nRef = 1;
	pTable->nRowEst = 1000000;
	assert(pParse->pNewTable == 0);
	pParse->pNewTable = pTable;

	/* If this is the magic sqlite_sequence table used by autoincrement,
	** then record a pointer to this table in the main database structure
	** so that INSERT can find the table easily.【
    **如果这是由sqlite_sequence表所使用的自动增量,然后记录一个指针，这个指针指向这个在主数据库结构中的表,以便插入可以很容易地找到表。
	*/
#ifndef SQLITE_OMIT_AUTOINCREMENT
	if (!pParse->nested && strcmp(zName, "sqlite_sequence") == 0){
		assert(sqlite3SchemaMutexHeld(db, iDb, 0));
		pTable->pSchema->pSeqTab = pTable;
	}
#endif

	/* Begin generating the code that will insert the table record into
	** the SQLITE_MASTER table.  Note in particular that we must go ahead
	** and allocate the record number for the table entry now.  Before any
	** PRIMARY KEY or UNIQUE keywords are parsed.  Those keywords will cause
	** indices to be created and the table record must come before the
	** indices.  Hence, the record number for the table must be allocated
	** now.
	**开始时生成的这些代码将会把表记录插入到SQLITE_MASTER 表中。尤其需注意的是，我们必须继续，现在为表项目指定记录编号。在任何PRIMARY KEY或UNIQUE关键字被解析之前，
	**这些关键字将导致创建索引和表记录必须先于指数。因此，对于表的记录编号必须现在指定。】
	*/
	//接下来的操作是分配表的记录编号
	if (!db->init.busy && (v = sqlite3GetVdbe(pParse)) != 0){
		int j1;
		int fileFormat;
		int reg1, reg2, reg3;
		sqlite3BeginWriteOperation(pParse, 0, iDb);   //执行写操作

#ifndef SQLITE_OMIT_VIRTUALTABLE
		if (isVirtual){
			sqlite3VdbeAddOp0(v, OP_VBegin);    //vdbe增加一个OP_VBegin
		}
#endif

		/* If the file format and encoding in the database have not been set,
		** set them now.
		**如果文件格式和代字符编码在数据库中还没有被设定，则现在设定他们。
		*/
		reg1 = pParse->regRowid = ++pParse->nMem;
		reg2 = pParse->regRoot = ++pParse->nMem;
		reg3 = ++pParse->nMem;
		sqlite3VdbeAddOp3(v, OP_ReadCookie, iDb, reg3, BTREE_FILE_FORMAT);
		sqlite3VdbeUsesBtree(v, iDb);
		j1 = sqlite3VdbeAddOp1(v, OP_If, reg3);
		fileFormat = (db->flags & SQLITE_LegacyFileFmt) != 0 ?
			1 : SQLITE_MAX_FILE_FORMAT;
		sqlite3VdbeAddOp2(v, OP_Integer, fileFormat, reg3);
		sqlite3VdbeAddOp3(v, OP_SetCookie, iDb, BTREE_FILE_FORMAT, reg3);
		sqlite3VdbeAddOp2(v, OP_Integer, ENC(db), reg3);
		sqlite3VdbeAddOp3(v, OP_SetCookie, iDb, BTREE_TEXT_ENCODING, reg3);
		sqlite3VdbeJumpHere(v, j1);

		/* This just creates a place-holder record in the sqlite_master table.
		** The record created does not contain anything yet.  It will be replaced
		** by the real entry in code generated at sqlite3EndTable().【这仅仅是创建了一个占位记录在sqlite_master表中。不包含任何创建的记录。取代它的是真正的进入代码在sqlite3EndTable()生成的。】
		**
		** The rowid for the new entry is left in register pParse->regRowid.
		** The root page number of the new table is left in reg pParse->regRoot.
		** The rowid and root page number values are needed by the code that
		** sqlite3EndTable will generate.【新条目的rowid留在注册pParse - > regRowid。
		新表的根页码是留在reg pParse - > regRoot。
		这个rowid和根页码属性所需的代码由sqlite3EndTable生成。】
		*/
#if !defined(SQLITE_OMIT_VIEW) || !defined(SQLITE_OMIT_VIRTUALTABLE)
		if (isView || isVirtual){
			sqlite3VdbeAddOp2(v, OP_Integer, 0, reg2);
		}
		else
#endif
		{
			sqlite3VdbeAddOp2(v, OP_CreateTable, iDb, reg2);
		}
		sqlite3OpenMasterTable(pParse, iDb);
		sqlite3VdbeAddOp2(v, OP_NewRowid, 0, reg1);
		sqlite3VdbeAddOp2(v, OP_Null, 0, reg3);
		sqlite3VdbeAddOp3(v, OP_Insert, 0, reg3, reg1);
		sqlite3VdbeChangeP5(v, OPFLAG_APPEND);
		sqlite3VdbeAddOp0(v, OP_Close);
	}

	/* Normal (non-error) return. */
	return;

	/* If an error occurs, we jump here */
begin_table_error:
	sqlite3DbFree(db, zName);
	return;
}

/*
** This macro is used to compare two strings in a case-insensitive manner.
**这个宏是用来比较两个字符串用不区分大小写的方式。
** It is slightly faster than calling sqlite3StrICmp() directly, but
** produces larger code.
**他是比直接调用sqlite3StrICmp()稍微快一点，但是会产生大量代码。
**
** WARNING: This macro is not compatible with the strcmp() family. It
** returns true if the two strings are equal, otherwise false.
**这个宏不兼容strcmp()家族的函数。如果两个字符串相等，则返回true,否则返回false
*/
#define STRICMP(x, y) (\
sqlite3UpperToLower[*(unsigned char *)(x)]==   \
sqlite3UpperToLower[*(unsigned char *)(y)]     \
&& sqlite3StrICmp((x)+1,(y)+1)==0 )

/*
** Add a new column to the table currently being constructed.
**向当前正在构建的表中添加新的一列。
**
** The parser calls this routine once for each column declaration
** in a CREATE TABLE statement.  sqlite3StartTable() gets called
** first to get things going.  Then this routine is called for each
** column.
**一旦每一列在一个CREATE TABLE语句中声明，解析器就会调用这个例程。首先被调用做一些事情，然后为每一列调用这个例程。
*/
void sqlite3AddColumn(Parse *pParse, Token *pName){
	Table *p;      //指针指向当前要创建的表
	int i;
	char *z;
	Column *pCol;    //列指针
	sqlite3 *db = pParse->db;     //db指向的是语法分析程序当前上下文正在处理的数据库，也就是db始终指向正在处理的数据库
	if ((p = pParse->pNewTable) == 0) return;    //pNewTable参数指明当前是否正在创建表，如果参数为0，则表明没有创建表，直接退出函数
#if SQLITE_MAX_COLUMN//表的列总数已经达到最大值
	if (p->nCol + 1>db->aLimit[SQLITE_LIMIT_COLUMN]){  //如果当前表的列数目+1得到的值比SQLITE所指定的列的峰值更大的话
		sqlite3ErrorMsg(pParse, "too many columns on %s", p->zName);   //发送错误信息“列太多”，并且退出函数
		return;
	}
#endif
	z = sqlite3NameFromToken(db, pName);   //否则的话就需要创建列，从分词程序中读出列名
	if (z == 0) return;     //如果列名为空的话就退出程序，表名当前就没有新的列创建
	for (i = 0; i<p->nCol; i++){//循环的检测新插入的一列的名字，是不是已经存在
		if (STRICMP(z, p->aCol[i].zName)){//比较两个字符串用不区分大小写的方式
			sqlite3ErrorMsg(pParse, "duplicate column name: %s", z);   //发出列名重复的错误
			sqlite3DbFree(db, z);   //释放当前的数据库
			return;
		}
	}
	//否则的话就创建新的列
	if ((p->nCol & 0x7) == 0){
		Column *aNew;
		aNew = sqlite3DbRealloc(db, p->aCol, (p->nCol + 8)*sizeof(p->aCol[0]));//为新的一列分配内存
		if (aNew == 0){//没有分配成功
			sqlite3DbFree(db, z);   //如果内存分配失败，释放数据库
			return;
		}
		p->aCol = aNew;//添加一列
	}
	pCol = &p->aCol[p->nCol];//改变原来的列数组的地址
	memset(pCol, 0, sizeof(p->aCol[0]));
	pCol->zName = z;

	/* If there is no type specified, columns have the default affinity
	** 'NONE'. If there is a type specified, then sqlite3AddColumnType() will
	** be called next to set pCol->affinity correctly.
	**如果没有类型指定，列默认关联“NONE”。如果有类型指定，那么sqlite3AddColumnType()就会被调用，接着正确地设定pCol->affinity的值。
	*/
	pCol->affinity = SQLITE_AFF_NONE;//设置列队数据类型，默认为NONE
	p->nCol++;//对列名总数进行修改，这个参数用于判断列名的个数是否得到阈值
}

/*
** This routine is called by the parser while in the middle of
** parsing a CREATE TABLE statement.  A "NOT NULL" constraint has
** been seen on a column.  This routine sets the notNull flag on
** the column currently under construction.【调用这个例程的解析器当解析CREATE TABLE语句的时候。会看到列不能为空的约束。这个例程为当前构建的列设定不能为空的标志。】
*/
void sqlite3AddNotNull(Parse *pParse, int onError){
	Table *p;
	p = pParse->pNewTable;     //指针指向当前正在创建的新表
	if (p == 0 || NEVER(p->nCol<1)) return;//如果新表指针为0或者其列数目是小于1的，表示根本就没有创建新表，结束这个函数
	p->aCol[p->nCol - 1].notNull = (u8)onError;//对当前添加的列进行“NOT NULL”设置  
}

/*
** Scan the column type name zType (length nType) and return the
** associated affinity type.
** 扫描列类型名称zType(长度nType)并返回相关的关联类型。
**
** This routine does a case-independent search of zType for the
** substrings in the following table. If one of the substrings is
** found, the corresponding affinity is returned. If zType contains
** more than one of the substrings, entries toward the top of
** the table take priority. For example, if zType is 'BLOBINT',
** SQLITE_AFF_INTEGER is returned.
** 这个例程做一次与案例无关的zType搜索对表下的子字符串。如果找到一个子字符串，返回相应的关系。如果zType包含不止一个子字符串，表头的条目优先。
**
** Substring     | Affinity
** --------------------------------
** 'INT'         | SQLITE_AFF_INTEGER
** 'CHAR'        | SQLITE_AFF_TEXT
** 'CLOB'        | SQLITE_AFF_TEXT
** 'TEXT'        | SQLITE_AFF_TEXT
** 'BLOB'        | SQLITE_AFF_NONE
** 'REAL'        | SQLITE_AFF_REAL
** 'FLOA'        | SQLITE_AFF_REAL
** 'DOUB'        | SQLITE_AFF_REAL
**
** If none of the substrings in the above table are found,
** SQLITE_AFF_NUMERIC is returned.
** 如果在以上所述的表中没有找到子字符串，则返回SQLITE_AFF_NUMERIC。
*/

/* created by junpeng zhu
**函数sqlite3AffinityType的功能是：为用户在创建表时指定的列类型返回系统统一的处理方式 
**其中参数:zIn是用户在创建表时为列名指定的类型，这个参数是一个const类型字符串，也就是在程序中不能修改这个指定的类型
*/
char sqlite3AffinityType(const char *zIn){
	u32 h = 0;   //无符号32位整型，u是指unsigned
	char aff = SQLITE_AFF_NUMERIC;   

	if (zIn) while (zIn[0]){//取字符串的第一个单元
		h = (h << 8) + sqlite3UpperToLower[(*zIn) & 0xff];//把大写转换成小写字母
		zIn++;  //接着取字符串的下一个单元
		if (h == (('c' << 24) + ('h' << 16) + ('a' << 8) + 'r')){             /*拼接CHAR */
			aff = SQLITE_AFF_TEXT;
		}
		else if (h == (('c' << 24) + ('l' << 16) + ('o' << 8) + 'b')){       /* CLOB */
			aff = SQLITE_AFF_TEXT;
		}
		else if (h == (('t' << 24) + ('e' << 16) + ('x' << 8) + 't')){       /* TEXT */
			aff = SQLITE_AFF_TEXT;
		}
		else if (h == (('b' << 24) + ('l' << 16) + ('o' << 8) + 'b')          /* BLOB */
			&& (aff == SQLITE_AFF_NUMERIC || aff == SQLITE_AFF_REAL)){
			aff = SQLITE_AFF_NONE;
#ifndef SQLITE_OMIT_FLOATING_POINT
		}
		else if (h == (('r' << 24) + ('e' << 16) + ('a' << 8) + 'l')          /* REAL */
			&& aff == SQLITE_AFF_NUMERIC){
			aff = SQLITE_AFF_REAL;
		}
		else if (h == (('f' << 24) + ('l' << 16) + ('o' << 8) + 'a')          /* FLOA */
			&& aff == SQLITE_AFF_NUMERIC){
			aff = SQLITE_AFF_REAL;
		}
		else if (h == (('d' << 24) + ('o' << 16) + ('u' << 8) + 'b')          /* DOUB */
			&& aff == SQLITE_AFF_NUMERIC){
			aff = SQLITE_AFF_REAL;
#endif
		}
		else if ((h & 0x00FFFFFF) == (('i' << 16) + ('n' << 8) + 't')){    /* INT */
			aff = SQLITE_AFF_INTEGER;
			break;
		}
	}

	return aff;
}

/*
** This routine is called by the parser while in the middle of
** parsing a CREATE TABLE statement.  The pFirst token is the first
** token in the sequence of tokens that describe the type of the
** column currently under construction.   pLast is the last token
** in the sequence.  Use this information to construct a string
** that contains the typename of the column and store that string
** in zType.
** 当解析CREATE TABLE语句的时候就会调用这个例程的解析器。会看到列不能为空的约束。这个例程为当前构建的列设定不能为空的标志。这个pFirst代表了符号序列的第一个符号，
** 这序列符号描述的是当前正在创建的列。pLast代表的是符号序列的最后一个符号。用此信息构造一个字符串，这个字符串包含列的类名和用zType存储的字符串。
*/
void sqlite3AddColumnType(Parse *pParse, Token *pType){
	Table *p;    //指向当前正在新建的表
	Column *pCol;  //列指针
	 
	p = pParse->pNewTable;    //指向当前正在新建的表
	if (p == 0 || NEVER(p->nCol<1)) return;    //如果当前新建表的指针为0或者是其列数目是小于1的，说明并没有新建表，退出函数
	pCol = &p->aCol[p->nCol - 1];//，否则找到当前创建的列
	assert(pCol->zType == 0);   
	pCol->zType = sqlite3NameFromToken(pParse->db, pType);//列的类型
	pCol->affinity = sqlite3AffinityType(pCol->zType);//，sqlite系统中相关联的类型
}

/*
** The expression is the default value for the most recently added column
** of the table currently under construction.
** 这个表达式是一个默认值对于当前正在创建的表的最近增添的列。
**
** Default value expressions must be constant.  Raise an exception if this
** is not the case.【默认值表达式必须是常量。如果不是这种情况引发一个异常。】
**
** This routine is called by the parser while in the middle of
** parsing a CREATE TABLE statement.【当解析CREATE TABLE语句的时候就会调用这个例程的解析器。】
*/
void sqlite3AddDefaultValue(Parse *pParse, ExprSpan *pSpan){
	Table *p;
	Column *pCol;
	sqlite3 *db = pParse->db;    //语法分析上下文指向的数据库，保证正在处理的是当前的数据库
	p = pParse->pNewTable;     //指向当前正在创建的表 
	if (p != 0){  
		pCol = &(p->aCol[p->nCol - 1]);
		if (!sqlite3ExprIsConstantOrFunction(pSpan->pExpr)){//默认值表达式必须是常量。如果不是这种情况引发一个异常
			sqlite3ErrorMsg(pParse, "default value of column [%s] is not constant",
				pCol->zName);
		}
		else{   //当前没有创建表
			/* A copy of pExpr is used instead of the original, as pExpr contains
			** tokens that point to volatile memory. The 'span' of the expression
			** is required by pragma table_info.
			**pExpr用于代替原来的副本，当pExpr包含指向易失存储器的符号时。
			**'span'表达式需要编译table_info。
			*/
			sqlite3ExprDelete(db, pCol->pDflt);//先删除，默认值
			pCol->pDflt = sqlite3ExprDup(db, pSpan->pExpr, EXPRDUP_REDUCE);//在复制
			sqlite3DbFree(db, pCol->zDflt);//释放原始默认值
			pCol->zDflt = sqlite3DbStrNDup(db, (char*)pSpan->zStart,
				(int)(pSpan->zEnd - pSpan->zStart));//把(char*)pSpan->zStart中的值复制给pCol->zDflt 
		}
	}
	sqlite3ExprDelete(db, pSpan->pExpr);
}

/*
** Designate the PRIMARY KEY for the table.  pList is a list of names
** of columns that form the primary key.  If pList is NULL, then the
** most recently added column of the table is the primary key.
** 给表指派外键。pList代表的是一系列构成主键列的名字。如果pList为空，则表的最近增加的列为主键。
**
** A table can have at most one primary key.  If the table already has
** a primary key (and this is the second primary key) then create an
** error.
** 一个表最多有一个主键。如果表已经存在一个主键（并且这是第二个主键）那么就创建一个错误。
**
** If the PRIMARY KEY is on a single column whose datatype is INTEGER,
** then we will try to use that column as the rowid.  Set the Table.iPKey
** field of the table under construction to be the index of the
** INTEGER PRIMARY KEY column.  Table.iPKey is set to -1 if there is
** no INTEGER PRIMARY KEY.
** 如果这个主键是建立在单一列上，这一列的数据类型是INTEGER，那么我们将会试图用这一列作为rowid（rowid意思是记录的唯一标识）.
** 为正在创建的表设置Table.iPKey字段，作为INTEGER PRIMARY KEY列的索引。如果没有INTEGER PRIMARY KEY，则Table.iPKey设定为-1.
**
** If the key is not an INTEGER PRIMARY KEY, then create a unique
** index for the key.  No index is created for INTEGER PRIMARY KEYs.
** 如果这个键不是INTEGER PRIMARY KEY，那么为这个键创建一个唯一的索引。 INTEGER PRIMARY KEYs是不创建索引的。
*/
void sqlite3AddPrimaryKey(
	Parse *pParse,    /* Parsing context   语法分析上下文*/
	ExprList *pList,  /* List of field names to be indexed 被索引的域名 */
	int onError,      /* What to do with a uniqueness conflict 如何处理唯一型冲突，也就是上面注释中提到的主键的唯一性 */
	int autoInc,      /* True if the AUTOINCREMENT keyword is present  如果关键字AUTOINCREMENT存在，则为true，换句话说就是设置自增的关键字*/
	int sortOrder     /* SQLITE_SO_ASC or SQLITE_SO_DESC   升序或者是降序关键字的处理*/
	){
	Table *pTab = pParse->pNewTable;     //指向当前正在创建的表
	char *zType = 0;
	int iCol = -1, i;
	if (pTab == 0 || IN_DECLARE_VTAB) goto primary_key_exit;   //如果根本没有创建表，则不必要设置主键，直接退出函数
	if (pTab->tabFlags & TF_HasPrimaryKey){   //当前表已经存在主键
		sqlite3ErrorMsg(pParse,
			"table \"%s\" has more than one primary key", pTab->zName);   //发送已经存在主键的错误信息
		goto primary_key_exit;     //退出程序
	}
	pTab->tabFlags |= TF_HasPrimaryKey;  //将tabFlags标志位置位没有主键的情况
	if (pList == 0){  //如果主键为NULL，则指定最近一个增加的列为主键
		iCol = pTab->nCol - 1;
		pTab->aCol[iCol].isPrimKey = 1;   //设定最近增加的一个列为主键
	}
	else{
		for (i = 0; i<pList->nExpr; i++){
			for (iCol = 0; iCol<pTab->nCol; iCol++){  //索引当前创建表的所有的列
				if (sqlite3StrICmp(pList->a[i].zName, pTab->aCol[iCol].zName) == 0){   
					break;
				}
			}
			if (iCol<pTab->nCol){
				pTab->aCol[iCol].isPrimKey = 1;   //索引所有的列设置主键
			}
		}
		if (pList->nExpr>1) iCol = -1;
	}
	if (iCol >= 0 && iCol<pTab->nCol){
		zType = pTab->aCol[iCol].zType;
	}
	if (zType && sqlite3StrICmp(zType, "INTEGER") == 0  //主键是单一的列并且是INTEGER，则创建iPKey，并设置为索引
		&& sortOrder == SQLITE_SO_ASC){   //默认的排序方式是增
		pTab->iPKey = iCol;
		pTab->keyConf = (u8)onError;  
		assert(autoInc == 0 || autoInc == 1);  //当指定类型为INTERGE时，断言不是自增的就是自减的
		pTab->tabFlags |= autoInc*TF_Autoincrement;  //设定标志位为自增
	}
	else if (autoInc){
#ifndef SQLITE_OMIT_AUTOINCREMENT
		sqlite3ErrorMsg(pParse, "AUTOINCREMENT is only allowed on an "
			"INTEGER PRIMARY KEY");     //类型不是整型是不允许设置自增关键字的，如果设置了则发送错误
#endif
	}
	else{
		Index *p;
		p = sqlite3CreateIndex(pParse, 0, 0, 0, pList, onError, 0, 0, sortOrder, 0);   //在主键上创建索引
		if (p){
			p->autoIndex = 2;
		}
		pList = 0;  
	}

primary_key_exit:   //标签
	sqlite3ExprListDelete(pParse->db, pList);   //当执行goto语句之后会跳到此处，删除主键或者根本不设置主键
	return;
}

/*
** Add a new CHECK constraint to the table currently under construction. 为当前正在创建的表添加一个新的CHECK约束。
*/
void sqlite3AddCheckConstraint(
	Parse *pParse,    /* Parsing context   语法分析程序的上下文*/
	Expr *pCheckExpr  /* The check expression   约束表达式*/
	){
#ifndef SQLITE_OMIT_CHECK
	Table *pTab = pParse->pNewTable;   //指向当前正在创建的新表
	if (pTab && !IN_DECLARE_VTAB){   //当前表存在
		pTab->pCheck = sqlite3ExprListAppend(pParse, pTab->pCheck, pCheckExpr);    //追加约束
		if (pParse->constraintName.n){
			sqlite3ExprListSetName(pParse, pTab->pCheck, &pParse->constraintName, 1);
		}
	}
	else
#endif
	{
		sqlite3ExprDelete(pParse->db, pCheckExpr);
	}
}

/*
** Set the collation function of the most recently parsed table column
** to the CollSeq given.
** 设置最近的排序函数解析CollSeq给定的表列。
*/
void sqlite3AddCollateType(Parse *pParse, Token *pToken){
	Table *p;
	int i;
	char *zColl;              /* Dequoted name of collation sequence */
	sqlite3 *db;

	if ((p = pParse->pNewTable) == 0) return;   //如果上下文数据库所指向的当前表为空，直接退出函数，因为病没有创建表
	i = p->nCol - 1;
	db = pParse->db;
	zColl = sqlite3NameFromToken(db, pToken);
	if (!zColl) return;

	if (sqlite3LocateCollSeq(pParse, zColl)){
		Index *pIdx;
		p->aCol[i].zColl = zColl;

		/* If the column is declared as "<name> PRIMARY KEY COLLATE <type>",
		** then an index may have been created on this column before the
		** collation type was added. Correct this if it is the case.【如果这一列被声明称为"<name> PRIMARY KEY COLLATE <type>"这样的列，
		那么在排序模式增加之前，在这个列上一个索引也许就要被创建。】
		*/
		for (pIdx = p->pIndex; pIdx; pIdx = pIdx->pNext){
			assert(pIdx->nColumn == 1);
			if (pIdx->aiColumn[0] == i){
				pIdx->azColl[0] = p->aCol[i].zColl;
			}
		}
	}
	else{
		sqlite3DbFree(db, zColl);
	}
}

/*
** This function returns the collation sequence for database native text
** encoding identified by the string zName, length nName.
** 这个函数返回的是数据库本地文件编码的排序序列，用字符串zName识别，长度由nName识别。
**
** If the requested collation sequence is not available, or not available
** in the database native encoding, the collation factory is invoked to
** request it. If the collation factory does not supply such a sequence,
** and the sequence is available in another text encoding, then that is
** returned instead.
** 如果这个所需的对照序列是无效的，或者是在数据库本地编码中无效，那么这个序列工厂就会被唤醒为了请求这个序列。
** 如果这个序列工厂没有提供这样的一个序列和这个序列是无效的在另一个文本编码中，那么这将被代替返回。
**
** If no versions of the requested collations sequence are available, or
** another error occurs, NULL is returned and an error message written into
** pParse.
** 如果没有所需对照序列的版本可用，或是出现另一个错误，返回NULL，并且把错误信息写入pParse中。
**
** This routine is a wrapper around sqlite3FindCollSeq().  This routine
** invokes the collation factory if the named collation cannot be found
** and generates an error message.
**
** See also: sqlite3FindCollSeq(), sqlite3GetCollSeq()
*/
CollSeq *sqlite3LocateCollSeq(Parse *pParse, const char *zName){
	sqlite3 *db = pParse->db;
	u8 enc = ENC(db);
	u8 initbusy = db->init.busy;
	CollSeq *pColl;

	pColl = sqlite3FindCollSeq(db, enc, zName, initbusy);
	if (!initbusy && (!pColl || !pColl->xCmp)){
		pColl = sqlite3GetCollSeq(db, enc, pColl, zName);
		if (!pColl){
			sqlite3ErrorMsg(pParse, "no such collation sequence: %s", zName);
		}
	}

	return pColl;
}



/*
** Generate code that will increment the schema cookie.【产生代码将增加模式cookie.】
**
** The schema cookie is used to determine when the schema for the
** database changes.  After each schema change, the cookie value
** changes.  When a process first reads the schema it records the
** cookie.  Thereafter, whenever it goes to access the database,
** it checks the cookie to make sure the schema has not changed
** since it was last read.【这个模式cookie是用于确定数据库模式改变时间。没有模式改变以后，这个cookie属性就会改变。当一个进程首先读取这个cookie时，
** 这个进程就会记录这个cookie值。因此，无论这个进程什么时候访问数据库，它都会检查这个cookie值确定这个模式没有被改变，这个过程直到进程读取到最后。】
**
** This plan is not completely bullet-proof.  It is possible for
** the schema to change multiple times and for the cookie to be
** set back to prior value.  But schema changes are infrequent
** and the probability of hitting the same cookie value is only
** 1 chance in 2^32.  So we're safe enough.【这个计划是不完全防护的。对于这个模式可能改变了多次和也可能这个cookie设定会原来的属性值。但是模式改变时很少的，
并且一样的cookie属性值碰撞的可能性仅仅一次在2^32下。因此我们是比较安全的。】
*/
void sqlite3ChangeCookie(Parse *pParse, int iDb){
	int r1 = sqlite3GetTempReg(pParse);
	sqlite3 *db = pParse->db;
	Vdbe *v = pParse->pVdbe;
	assert(sqlite3SchemaMutexHeld(db, iDb, 0));
	sqlite3VdbeAddOp2(v, OP_Integer, db->aDb[iDb].pSchema->schema_cookie + 1, r1);
	sqlite3VdbeAddOp3(v, OP_SetCookie, iDb, BTREE_SCHEMA_VERSION, r1);
	sqlite3ReleaseTempReg(pParse, r1);
}

/*
** Measure the number of characters needed to output the given
** identifier.  The number returned includes any quotes used
** but does not include the null terminator.【衡量一下输出所需的字符数、给出的标识符。包括使用的任何引用返回的数量，但是包含的空终结器不返回。】
**
** The estimate is conservative.  It might be larger that what is
** really needed.【这个估量是保守的。也许比真实所需的更大。】
*/
static int identLength(const char *z){
	int n;
	for (n = 0; *z; n++, z++){
		if (*z == '"'){ n++; }
	}
	return n + 2;
}

/*
** The first parameter is a pointer to an output buffer. The second
** parameter is a pointer to an integer that contains the offset at
** which to write into the output buffer. This function copies the
** nul-terminated string pointed to by the third parameter, zSignedIdent,
** to the specified offset in the buffer and updates *pIdx to refer
** to the first byte after the last byte written before returning.【第一个参数指向一个输出缓冲区指针。
** 第二个参数是一个指向一个整数的指针,这个整数包含的抵消编写到输出缓冲区。这个函数复制第三个参数（zSignedIdent）指向的空终止字符串，
** 这个指定的zSignedIdent在缓冲器总抵消和更新*pIdx参照第一个字符在最后一个字符写入之后，而且在返回之前。】
**
** If the string zSignedIdent consists entirely of alpha-numeric
** characters, does not begin with a digit and is not an SQL keyword,
** then it is copied to the output buffer exactly as it is. Otherwise,
** it is quoted using double-quotes.【如果这个字符串完全由文字数字组成，不是以一个数字符号开头，也不是以一个SQL关键字开头，
那么这个字符串就会被完全拷贝到输出缓冲区。否则，这个字符串就用双引号引住。】
*/
static void identPut(char *z, int *pIdx, char *zSignedIdent){
	unsigned char *zIdent = (unsigned char*)zSignedIdent;
	int i, j, needQuote;
	i = *pIdx;

	for (j = 0; zIdent[j]; j++){
		if (!sqlite3Isalnum(zIdent[j]) && zIdent[j] != '_') break;
	}
	needQuote = sqlite3Isdigit(zIdent[0]) || sqlite3KeywordCode(zIdent, j) != TK_ID;
	if (!needQuote){
		needQuote = zIdent[j];
	}

	if (needQuote) z[i++] = '"';
	for (j = 0; zIdent[j]; j++){
		z[i++] = zIdent[j];
		if (zIdent[j] == '"') z[i++] = '"';
	}
	if (needQuote) z[i++] = '"';
	z[i] = 0;
	*pIdx = i;
}

/*
** Generate a CREATE TABLE statement appropriate for the given
** table.  Memory to hold the text of the statement is obtained
** from sqliteMalloc() and must be freed by the calling function.
** 生成一个CREATE TABLE语句适合给定的表。
** 内存保存从sqliteMalloc()获得的文本声明，并且内存必须是通过调用函数释放。
*/
static char *createTableStmt(sqlite3 *db, Table *p){
	int i, k, n;
	char *zStmt;
	char *zSep, *zSep2, *zEnd;
	Column *pCol;
	n = 0;
	for (pCol = p->aCol, i = 0; i<p->nCol; i++, pCol++){
		n += identLength(pCol->zName) + 5;
	}
	n += identLength(p->zName);
	if (n<50){
		zSep = "";
		zSep2 = ",";
		zEnd = ")";
	}
	else{
		zSep = "\n  ";
		zSep2 = ",\n  ";
		zEnd = "\n)";
	}
	n += 35 + 6 * p->nCol;
	zStmt = sqlite3DbMallocRaw(0, n);
	if (zStmt == 0){
		db->mallocFailed = 1;
		return 0;
	}
	sqlite3_snprintf(n, zStmt, "CREATE TABLE ");
	k = sqlite3Strlen30(zStmt);
	identPut(zStmt, &k, p->zName);
	zStmt[k++] = '(';
	for (pCol = p->aCol, i = 0; i<p->nCol; i++, pCol++){
		static const char * const azType[] = {
			/* SQLITE_AFF_TEXT    */ " TEXT",
			/* SQLITE_AFF_NONE    */ "",
			/* SQLITE_AFF_NUMERIC */ " NUM",
			/* SQLITE_AFF_INTEGER */ " INT",
			/* SQLITE_AFF_REAL    */ " REAL"
		};
		int len;
		const char *zType;

		sqlite3_snprintf(n - k, &zStmt[k], zSep);
		k += sqlite3Strlen30(&zStmt[k]);
		zSep = zSep2;
		identPut(zStmt, &k, pCol->zName);
		assert(pCol->affinity - SQLITE_AFF_TEXT >= 0);
		assert(pCol->affinity - SQLITE_AFF_TEXT < ArraySize(azType));
		testcase(pCol->affinity == SQLITE_AFF_TEXT);
		testcase(pCol->affinity == SQLITE_AFF_NONE);
		testcase(pCol->affinity == SQLITE_AFF_NUMERIC);
		testcase(pCol->affinity == SQLITE_AFF_INTEGER);
		testcase(pCol->affinity == SQLITE_AFF_REAL);

		zType = azType[pCol->affinity - SQLITE_AFF_TEXT];
		len = sqlite3Strlen30(zType);
		assert(pCol->affinity == SQLITE_AFF_NONE
			|| pCol->affinity == sqlite3AffinityType(zType));
		memcpy(&zStmt[k], zType, len);
		k += len;
		assert(k <= n);
	}
	sqlite3_snprintf(n - k, &zStmt[k], "%s", zEnd);
	return zStmt;
}

/*
** This routine is called to report the final ")" that terminates
** a CREATE TABLE statement.
** 这段代码被调用去报告最终一个CREATE TABLE语句“）”。
**
** The table structure that other action routines have been building
** is added to the internal hash tables, assuming no errors have
** occurred.
** 其他操作例程的表结构建立被添加到内部哈希表，这些操作例程假设没有发生错误。
**
** An entry for the table is made in the master table on disk, unless
** this is a temporary table or db->init.busy==1.  When db->init.busy==1
** it means we are reading the sqlite_master table because we just
** connected to the database or because the sqlite_master table has
** recently changed, so the entry for this table already exists in
** the sqlite_master table.  We do not want to create it again.
** 一个表的条目是被建立在主表磁盘上，除非这是临时表或db->init.busy==1情况除外。
** 当db->init.busy==1时意思是我们正在读取这个sqlite主表，因为我们只是连接到数据库或是因为这个sqlite主表最近发生了改变，
** 因此这个表的条目在sqlite主表中已经存在。我们没必要再次重建这个表的条目。
**
** If the pSelect argument is not NULL, it means that this routine
** was called to create a table generated from a
** "CREATE TABLE ... AS SELECT ..." statement.  The column names of
** the new table will match the result set of the SELECT.
** 如果pSelect的内容是NULL，意思是这个例程被调用去创建一个产生于 "CREATE TABLE ... AS SELECT ..." 语句的表。
** 新建表的列名将与这个SELECT设定的结果相匹配。
*/
void sqlite3EndTable(
	Parse *pParse,          /* Parse context   语法解析的上下文*/
	Token *pCons,           /* The ',' token after the last column defn.  在最后的一个列之后的, */
	Token *pEnd,            /* The final ')' token in the CREATE TABLE  CREATE TABLE语句的结束符号*/
	Select *pSelect         /* Select from a "CREATE ... AS SELECT"  生成一个带有AS SELECT 字句的CREATE TABLE语句 */
	){
	Table *p;
	sqlite3 *db = pParse->db;   //指向语法解析上下文正在操作的数据库
	int iDb; 

	if ((pEnd == 0 && pSelect == 0) || db->mallocFailed){   //如果没有遇到CREATE TABLE语句的结束右括号，并且确实是没有AS SELECT字句，这两个条件同时满足，或者内存申请失败
		return;   //直接退出函数，因为这样的创建表是不符合语法的 
	} 
	p = pParse->pNewTable;   //p指针指向当前正在创建的表的地址
	if (p == 0) return;   //如果p为0意味着根本没有创建表

	assert(!db->init.busy || !pSelect);        //断言已经没有在读取SQL语句，并且并没有AS SELECT语句需要进行处理，后面的处理针对这种假设

	iDb = sqlite3SchemaToIndex(db, p->pSchema);   //为当前的模式创建索引    

#ifndef SQLITE_OMIT_CHECK
	/* Resolve names in all CHECK constraint expressions. 用所有的CHECK约束表达式确定名字。
	*/
	if (p->pCheck){
		SrcList sSrc;                   /* Fake SrcList for pParse->pNewTable 为 pParse->pNewTable假设SrcList */
		NameContext sNC;                /* Name context for pParse->pNewTable  为pParse->pNewTable记录名称上下文*/
		ExprList *pList;                /* List of all CHECK constraints 所有CHECK约束的列表  */
		int i;                          /* Loop counter  循环计数器 */

		memset(&sNC, 0, sizeof(sNC));   //将sNC的所有单元用0代替，处理完成之后返回sNC
		memset(&sSrc, 0, sizeof(sSrc));  //将sSrc的所有单元用0代替，处理完成之后返回sSrc
		sSrc.nSrc = 1;
		sSrc.a[0].zName = p->zName;
		sSrc.a[0].pTab = p;
		sSrc.a[0].iCursor = -1;
		sNC.pParse = pParse;
		sNC.pSrcList = &sSrc;
		sNC.ncFlags = NC_IsCheck;
		pList = p->pCheck;
		for (i = 0; i<pList->nExpr; i++){
			if (sqlite3ResolveExprNames(&sNC, pList->a[i].pExpr)){
				return;
			}
		}
	}
#endif 
	/* !defined(SQLITE_OMIT_CHECK) */

	/* If the db->init.busy is 1 it means we are reading the SQL off the
	** "sqlite_master" or "sqlite_temp_master" table on the disk.
	** So do not write to the disk again.  Extract the root page number
	** for the table from the db->init.newTnum field.  (The page number
	** should have been put there by the sqliteOpenCb routine.)
	** 如果db->init.busy是1意思是我们正在读取SQL语句"sqlite_master"或磁盘上的"sqlite_temp_master"表。
	** 从 db->init.newTnum 中提取出这个表根页码数。（这个页码数应该是通过sqliteOpenCb例程放在db->init.newTnum那里的。）
	*/
	if (db->init.busy){   //如果正在读取SQL语句
		p->tnum = db->init.newTnum;
	}

	/* If not initializing, then create a record for the new table
	** in the SQLITE_MASTER table of the database.
	** 如果没有初始化，那么在数据库的SQLITE_MASTER表中创建一个新表记录。
	**
	** If this is a TEMPORARY table, write the entry into the auxiliary
	** file instead of into the main database file.
	** 如果是一个临时表，则把这个表条目写入辅助文件而不是写入主数据库文件
	*/
	if (!db->init.busy){   //没有读取SQL语句
		int n;
		Vdbe *v;
		char *zType;    /* "view" or "table"   表示视图或者表*/
		char *zType2;   /* "VIEW" or "TABLE"    视图或者表，第1814行涉及到了这个参数的初始化 */
		char *zStmt;    /* Text of the CREATE TABLE or CREATE VIEW statement   创建表或者创建视图语句的上下文*/

		v = sqlite3GetVdbe(pParse);  //SQLite中修改数据库时必须要获取VDBE
		if (NEVER(v == 0)) return;  //如果v为0代表根本没有在修改数据库，直接退出操作
		 
		sqlite3VdbeAddOp1(v, OP_Close, 0);  //给VDBE增加操作OP_close（关闭操作）

		/*
		** Initialize zType for the new view or table.
		** 为新视图或表初始化zType
		*/
		if (p->pSelect == 0){   //如果没有AS SELECT语句
			/* A regular table  一个常规表 */
			zType = "table";   //进行赋值的初始化
			zType2 = "TABLE";
#ifndef SQLITE_OMIT_VIEW
		}
		else{
			/* A view */
			zType = "view";   //否则初始化为视图
			zType2 = "VIEW";
#endif
		}

		/* If this is a CREATE TABLE xx AS SELECT ..., execute the SELECT
		** statement to populate the new table. The root-page number for the
		** new table is in register pParse->regRoot.
		** 如果这是一个CREATE TABLE xx AS SELECT ...，执行SELECT语句填充新表。新表的根页码数保存在寄存器pParse->regRoot中。
		** 
		** Once the SELECT has been coded by sqlite3Select(), it is in a
		** suitable state to query for the column names and types to be used
		** by the new table.
		** 一旦这个SELECT被编译通过sqlite3Select()，它就会在合适的状态下去查询用于这个新表的列名和属性。
		**
		** A shared-cache write-lock is not required to write to the new table,
		** as a schema-lock must have already been obtained to create it. Since
		** a schema-lock excludes all other database users, the write-lock would
		** be redundant.
		** 共享缓存的写锁不需要写到新表里面,因为一个模式锁已经获得，并且已经创建了新表（英文注释中it指代新创建的表）。
		** 一旦一个schema-lock包含所有其他的数据库用户，这个写入锁将是多余的。
		*/
		if (pSelect){    //如果有AS SELECT语句，下面处理这种情况
			SelectDest dest;  
			Table *pSelTab;

			assert(pParse->nTab == 1);
			sqlite3VdbeAddOp3(v, OP_OpenWrite, 1, pParse->regRoot, iDb);
			sqlite3VdbeChangeP5(v, OPFLAG_P2ISREG);
			pParse->nTab = 2;
			sqlite3SelectDestInit(&dest, SRT_Table, 1);
			sqlite3Select(pParse, pSelect, &dest);
			sqlite3VdbeAddOp1(v, OP_Close, 1);
			if (pParse->nErr == 0){
				pSelTab = sqlite3ResultSetOfSelect(pParse, pSelect);
				if (pSelTab == 0) return;
				assert(p->aCol == 0);
				p->nCol = pSelTab->nCol;
				p->aCol = pSelTab->aCol;
				pSelTab->nCol = 0;
				pSelTab->aCol = 0;
				sqlite3DeleteTable(db, pSelTab);
			}
		}

		/* Compute the complete text of the CREATE statement
		** 计算这个CREATE语句的全文 */
		if (pSelect){
			zStmt = createTableStmt(db, p);
		}
		else{
			n = (int)(pEnd->z - pParse->sNameToken.z) + 1;
			zStmt = sqlite3MPrintf(db,
				"CREATE %s %.*s", zType2, n, pParse->sNameToken.z
				);
		}

		/* A slot for the record has already been allocated in the
		** SQLITE_MASTER table.  We just need to update that slot with all
		** the information we've collected.
		** 记录的位置已经被分派到SQLITE_MASTER表中。我们仅仅需要用我们收集的所有信息去更新位置。
		*/
		sqlite3NestedParse(pParse,   //调用嵌套解析函数
			"UPDATE %Q.%s "
			"SET type='%s', name=%Q, tbl_name=%Q, rootpage=#%d, sql=%Q "
			"WHERE rowid=#%d",
			db->aDb[iDb].zName, SCHEMA_TABLE(iDb),
			zType,
			p->zName,
			p->zName,
			pParse->regRoot,
			zStmt,
			pParse->regRowid
			);
		sqlite3DbFree(db, zStmt);
		sqlite3ChangeCookie(pParse, iDb);

#ifndef SQLITE_OMIT_AUTOINCREMENT
		/* Check to see if we need to create an sqlite_sequence table for
		** keeping track of autoincrement keys.
		** 检查看看我们是否需要创建一个sqlite_sequence table表为了跟踪自动增量键。
		*/
		if (p->tabFlags & TF_Autoincrement){
			Db *pDb = &db->aDb[iDb];
			assert(sqlite3SchemaMutexHeld(db, iDb, 0));
			if (pDb->pSchema->pSeqTab == 0){
				sqlite3NestedParse(pParse,
					"CREATE TABLE %Q.sqlite_sequence(name,seq)",
					pDb->zName
					);
			}
		}
#endif

		/* Reparse everything to update our internal data structures
		** 重新解析一切更新我们的内部数据结构 */
		sqlite3VdbeAddParseSchemaOp(v, iDb,
			sqlite3MPrintf(db, "tbl_name='%q'", p->zName));
	}


	/* Add the table to the in-memory representation of the database.
	** 将表添加到数据库的内存中表示
	*/
	if (db->init.busy){   //如果数据库现在正在读SQL语句
		Table *pOld;
		Schema *pSchema = p->pSchema;
		assert(sqlite3SchemaMutexHeld(db, iDb, 0));  //互斥访问
		pOld = sqlite3HashInsert(&pSchema->tblHash, p->zName,
			sqlite3Strlen30(p->zName), p);
		if (pOld){
			assert(p == pOld);  /* Malloc must have failed inside HashInsert()
								** 内存分配一定发生了错误在HashInsert()内 */
			db->mallocFailed = 1;    //内存申请失败
			return;
		}
		pParse->pNewTable = 0;
		db->flags |= SQLITE_InternChanges;

#ifndef SQLITE_OMIT_ALTERTABLE
		if (!p->pSelect){
			const char *zName = (const char *)pParse->sNameToken.z;
			int nName;
			assert(!pSelect && pCons && pEnd);
			if (pCons->z == 0){
				pCons = pEnd;
			}
			nName = (int)((const char *)pCons->z - zName);
			p->addColOffset = 13 + sqlite3Utf8CharLen(zName, nName);
		}
#endif
	}
}

#ifndef SQLITE_OMIT_VIEW
/*
** The parser calls this routine in order to create a new VIEW
** 解析器调用这个例程为了创建一个新视图。
*/
void sqlite3CreateView(
	Parse *pParse,     /* The parsing context   语法分析程序上下文*/
	Token *pBegin,     /* The CREATE token that begins the statement    开始这个创建视图语句的CREATE符号*/
	Token *pName1,     /* The token that holds the name of the view   包含这个视图名字的符号*/
	Token *pName2,     /* The token that holds the name of the view  包含这个视图名字的符号*/
	Select *pSelect,   /* A SELECT statement that will become the new view  SELECT语句将创建一个新的视图 */
	int isTemp,        /* TRUE for a TEMPORARY view   如果是临时视图的话这个变量的值就为真*/
	int noErr          /* Suppress error messages if VIEW already exists  如果视图已经存在的话，发出错误信息*/
	){
	Table *p;
	int n;    //分析字符的个数
	const char *z;   //分词数组指针，一个常量的字符数组，这个数组由词法分析程序提供
	Token sEnd;   //指向创建视图语句的结束部分，会有一个函数对这个变量进行赋值，搜索整个词法符号，找到结束符号\0或者；
	DbFixer sFix;
	Token *pName = 0;
	int iDb;
	sqlite3 *db = pParse->db;    //指向语法分析上下文正在操作的数据库

	if (pParse->nVar>0){
		sqlite3ErrorMsg(pParse, "parameters are not allowed in views");   //发出错误，在视图中不允许有参数存在
		sqlite3SelectDelete(db, pSelect);     
		return;
	}
	sqlite3StartTable(pParse, pName1, pName2, isTemp, 1, 0, noErr);
	p = pParse->pNewTable;   //指向当前正在创建的表
	if (p == 0 || pParse->nErr){   //如果当前没有创建表或者当前的上下文中出现了错误，直接退出函数
		sqlite3SelectDelete(db, pSelect);
		return;
	}
	sqlite3TwoPartName(pParse, pName1, pName2, &pName);  //进行两部分名称的格式修改，用于内存中表示，和创建表中的函数是一个定义
	iDb = sqlite3SchemaToIndex(db, p->pSchema);
	if (sqlite3FixInit(&sFix, pParse, iDb, "view", pName)
		&& sqlite3FixSelect(&sFix, pSelect)
		){
		sqlite3SelectDelete(db, pSelect);
		return;
	}

	/* Make a copy of the entire SELECT statement that defines the view.
	** This will force all the Expr.token.z values to be dynamically
	** allocated rather than point to the input string - which means that
	** they will persist after the current sqlite3_exec() call returns.
	** 复制全部的SELECT语句,定义视图。
	** 这将使所有的Expr.token.z值被动态的分配，而不是单单指向输入的字符串。意思是当前的sqlite3_exec()调用返回值之后这些Expr.token.z值将保持不变。
	*/
	p->pSelect = sqlite3SelectDup(db, pSelect, EXPRDUP_REDUCE);   //对SELECT语句进行备份
	sqlite3SelectDelete(db, pSelect);   //删除SELECT语句
	if (db->mallocFailed){   //如果内存分配失败，直接退出函数
		return;
	}
	if (!db->init.busy){   //读取SQL语句的操作已经结束
		sqlite3ViewGetColumnNames(pParse, p);    //获得要创建视图的列名
	}

	/* Locate the end of the CREATE VIEW statement.  Make sEnd point to
	** the end.
	** 定位CREATE VIEW语句最后，确保sEnd指向最后。
	*/
	sEnd = pParse->sLastToken;   //定位到创建视图语句的结尾部分，也就是最后一个符号
	if (ALWAYS(sEnd.z[0] != 0) && sEnd.z[0] != ';'){   //如果最后一个结束符号不是\或者不是；那么继续读下一个符号，直到扫描得到创建视图语句的最后一个符号
		sEnd.z += sEnd.n;    
	}
	sEnd.n = 0;   
	n = (int)(sEnd.z - pBegin->z);   //计算整个创建视图语句的符号个数，这些符号由词法分析程序给出
	z = pBegin->z;   //指向创建视图语句的开始符号
	while (ALWAYS(n>0) && sqlite3Isspace(z[n - 1])){ n--; }  //扫面这个符号数组，将其中的空格去掉
	sEnd.z = &z[n - 1];  //得到去掉空格之后的符号数组的最后一个单元
	sEnd.n = 1;    //将个数重新赋值为1

	/* Use sqlite3EndTable() to add the view to the SQLITE_MASTER table */
	sqlite3EndTable(pParse, 0, &sEnd, 0);   //调用创建表的结束函数
	return;  //处理完成
}
#endif /* SQLITE_OMIT_VIEW */

#if !defined(SQLITE_OMIT_VIEW) || !defined(SQLITE_OMIT_VIRTUALTABLE)
/*
** The Table structure pTable is really a VIEW.  Fill in the names of
** the columns of the view in the pTable structure.  Return the number
** of errors.  If an error is seen leave an error message in pParse->zErrMsg.
** 参数pTable是一个已经创建的视图，在这个视图结构中填充列名
** 错误的信息被保存在pParse->zErrMsg
*/
int sqlite3ViewGetColumnNames(Parse *pParse, Table *pTable){
	Table *pSelTab;   /* A fake table from which we get the result set */
	Select *pSel;     /* Copy of the SELECT that implements the view */
	int nErr = 0;     /* Number of errors encountered  遇到的错误数量 */
	int n;            /* Temporarily holds the number of cursors assigned 暂时持有游标的数量分配 */
	sqlite3 *db = pParse->db;  /* Database connection for malloc errors  内存分配中数据库连接错误*/
	int(*xAuth)(void*, int, const char*, const char*, const char*, const char*);  //通用函数指针模板

	assert(pTable);

#ifndef SQLITE_OMIT_VIRTUALTABLE
	if (sqlite3VtabCallConnect(pParse, pTable)){
		return SQLITE_ERROR;
	}
	if (IsVirtual(pTable)) return 0;    //如果是虚拟视图，退出函数
#endif

#ifndef SQLITE_OMIT_VIEW
	/* A positive nCol means the columns names for this view are
	** already known.
	** 正数nCol参数的意思是列名已经存在了（已经获取到了，这个函数的功能就是获取视图的列名）
	*/ 
	if (pTable->nCol>0) return 0;     //如果已经获取列名，则直接退出

	/* A negative nCol is a special marker meaning that we are currently
	** trying to compute the column names.  If we enter this routine with
	** a negative nCol, it means two or more views form a loop, like this:
	**
	**     CREATE VIEW one AS SELECT * FROM two;
	**     CREATE VIEW two AS SELECT * FROM one;
	**
	** Actually, the error above is now caught prior to reaching this point.
	** But the following test is still important as it does come up
	** in the following:
	**
	**     CREATE TABLE main.ex1(a);
	**     CREATE TEMP VIEW ex1 AS SELECT a FROM ex1;
	**     SELECT * FROM temp.ex1;
	** 负数nCol是一个特殊的标记，意思是我们目前试着去计算列名。如果我们带着这个负数进入这个例程，它的意思是来自于一个循环的两个或更多的视图，就像下面这样：
	** CREATE VIEW one AS SELECT * FROM two;
	** CREATE VIEW two AS SELECT * FROM one;
	** 实际上，上面这个错误先在是被捕获了在达到这个点之前。但是接下来的测试任然很重要，因为接下来这个情况那个错误会发生的。
	** CREATE TABLE main.ex1(a);
	** CREATE TEMP VIEW ex1 AS SELECT a FROM ex1;
	** SELECT * FROM temp.ex1;

	*/
	if (pTable->nCol<0){   
		sqlite3ErrorMsg(pParse, "view %s is circularly defined", pTable->zName); //视图被循环的定义
		return 1;
	}
	assert(pTable->nCol >= 0);

	/* If we get this far, it means we need to compute the table names.
	** Note that the call to sqlite3ResultSetOfSelect() will expand any
	** "*" elements in the results set of the view and will assign cursors
	** to the elements of the FROM clause.  But we do not want these changes
	** to be permanent（永久的）.  So the computation is done on a copy of the SELECT
	** statement that defines the view.
	** 如果我们打到了这个最大极限，意味着我们需要计算这个表名。
	** 要注意的是，在视图结果集中，调用sqlite3ResultSetOfSelect()将展开任何一个'*'元素
	** ，并且将为来自FROM子句的元素设置游标。
	** 但是我们不想这些改变时永久的。因此计算要被做在这个定义视图的SELECT语句的副本上（也就是在副本上做下面的这个函数的操作）。
	*/
	assert(pTable->pSelect);   //断言这个定义视图的语句带有SELECT子句
	pSel = sqlite3SelectDup(db, pTable->pSelect, 0);   //获取这个SELECT子句的副本，其中变量pSel代表的就是这个子句的副本，其中Dup（Duplicate 复制）的简写
	if (pSel){   //如果副本确实是存在的，也就是说确实是有SELECT子句
		u8 enableLookaside = db->lookaside.bEnabled;   
		n = pParse->nTab;
		sqlite3SrcListAssignCursors(pParse, pSel->pSrc);   //为来自FROM子句的元素设置游标
		pTable->nCol = -1;
		db->lookaside.bEnabled = 0;
#ifndef SQLITE_OMIT_AUTHORIZATION     
		xAuth = db->xAuth;
		db->xAuth = 0;
		pSelTab = sqlite3ResultSetOfSelect(pParse, pSel);
		db->xAuth = xAuth;
#else
		pSelTab = sqlite3ResultSetOfSelect(pParse, pSel);
#endif
		db->lookaside.bEnabled = enableLookaside;
		pParse->nTab = n;
		if (pSelTab){
			assert(pTable->aCol == 0);
			pTable->nCol = pSelTab->nCol;
			pTable->aCol = pSelTab->aCol;
			pSelTab->nCol = 0;
			pSelTab->aCol = 0;
			sqlite3DeleteTable(db, pSelTab);
			assert(sqlite3SchemaMutexHeld(db, 0, pTable->pSchema));
			pTable->pSchema->flags |= DB_UnresetViews;
		}
		else{
			pTable->nCol = 0;
			nErr++;
		}
		sqlite3SelectDelete(db, pSel);
	}
	else {
		nErr++;
	}
#endif /* SQLITE_OMIT_VIEW */
	return nErr;
}
#endif /* !defined(SQLITE_OMIT_VIEW) || !defined(SQLITE_OMIT_VIRTUALTABLE) */

#ifndef SQLITE_OMIT_VIEW
/*
** Clear the column names from every VIEW in database idx.【清除数据库idx每个视图的列名称。】
*/
static void sqliteViewResetAll(sqlite3 *db, int idx){
	HashElem *i;
	assert(sqlite3SchemaMutexHeld(db, idx, 0));
	if (!DbHasProperty(db, idx, DB_UnresetViews)) return;
	for (i = sqliteHashFirst(&db->aDb[idx].pSchema->tblHash); i; i = sqliteHashNext(i)){
		Table *pTab = sqliteHashData(i);
		if (pTab->pSelect){
			sqliteDeleteColumnNames(db, pTab);
			pTab->aCol = 0;
			pTab->nCol = 0;
		}
	}
	DbClearProperty(db, idx, DB_UnresetViews);
}
#else
# define sqliteViewResetAll(A,B)
#endif /* SQLITE_OMIT_VIEW */

/*
** This function is called by the VDBE to adjust the internal schema
** used by SQLite when the btree layer moves a table root page. The
** root-page of a table or index in database iDb has changed from iFrom
** to iTo.
** 当btree的层调整移动了一个表的根页面时，这个函数被VDBE调用去调整SQLITE的内部模式。一个表的root-page或数据库中的索引由iFrom改变到iTo。
**
** Ticket #1728:  The symbol table might still contain information
** on tables and/or indices that are the process of being deleted.
** If you are unlucky, one of those deleted indices or tables might
** have the same rootpage number as the real table or index that is
** being moved.  So we cannot stop searching after the first match
** because the first match might be for one of the deleted indices
** or tables and not the table/index that is actually being moved.
** We must continue looping until all tables and indices with
** rootpage==iFrom have been converted to have a rootpage of iTo
** in order to be certain that we got the right one.
** 标签#1728：符号表仍可能包含信息表和/或索引，但是他们确实是被删除了。
** 如果你是不幸的，那些中的一个删除索引或表，它们也许有相同的根页码数，正如这个真实的被迁移的表或索引。
** 因此我们不能停止搜索第一个相匹配后，因为第一个相匹配项可能是被删除的索引或表中的其中一个和不是事实上正在移动的表/索引。
** 我们必须继续循环直到所有的满足rootpage==iFrom条件的表和索引被转换为有一个iTo的页码，这样做是为了确保我们得到正确的一个结果。
*/
#ifndef SQLITE_OMIT_AUTOVACUUM
void sqlite3RootPageMoved(sqlite3 *db, int iDb, int iFrom, int iTo){
	HashElem *pElem;
	Hash *pHash;
	Db *pDb;

	assert(sqlite3SchemaMutexHeld(db, iDb, 0));
	pDb = &db->aDb[iDb];
	pHash = &pDb->pSchema->tblHash;
	for (pElem = sqliteHashFirst(pHash); pElem; pElem = sqliteHashNext(pElem)){
		Table *pTab = sqliteHashData(pElem);
		if (pTab->tnum == iFrom){
			pTab->tnum = iTo;
		}
	}
	pHash = &pDb->pSchema->idxHash;
	for (pElem = sqliteHashFirst(pHash); pElem; pElem = sqliteHashNext(pElem)){
		Index *pIdx = sqliteHashData(pElem);
		if (pIdx->tnum == iFrom){
			pIdx->tnum = iTo;
		}
	}
}
#endif

/*
** Write code to erase the table with root-page iTable from database iDb.
** Also write code to modify the sqlite_master table and internal schema
** if a root-page of another table is moved by the btree-layer whilst
** erasing iTable (this can happen with an auto-vacuum database).
** 编写代码来删除这个表与来自于数据库iDb的根页iTable。
** 并且编写代码来更新sqlite_master表和内部模式，如果另一个表的一个跟页被btree-layer迁移，则将清除页码iTable（这可能在一个auto-vacuum数据库中发生）
*/
static void destroyRootPage(Parse *pParse, int iTable, int iDb){
	Vdbe *v = sqlite3GetVdbe(pParse);
	int r1 = sqlite3GetTempReg(pParse);
	sqlite3VdbeAddOp3(v, OP_Destroy, iTable, r1, iDb);
	sqlite3MayAbort(pParse);
#ifndef SQLITE_OMIT_AUTOVACUUM
	/* OP_Destroy stores an in integer r1. If this integer
	** is non-zero, then it is the root page number of a table moved to
	** location iTable. The following code modifies the sqlite_master table to
	** reflect this.【OP_Destroy存储一个整数r1.如果这个整数不为0，那么它是一个表的根页码号移动到位置iTable。接下来的代码更新sqlite_master表来反映这种情况。】
	**
	** The "#NNN" in the SQL is a special constant that means whatever value
	** is in register NNN.  See grammar rules associated with the TK_REGISTER
	** token for additional information.【SQL中的“#NNN”是一种特殊的常数，它的意思是无论什么属性值都是在寄存器NNN中。看到与TK_REGISTER符号相关的语法规则额外的信息。】
	*/
  int iTab = pTab->tnum;
  int iDestroyed = 0;

  while( 1 ){
    Index *pIdx;
    int iLargest = 0;

    if( iDestroyed==0 || iTab<iDestroyed ){
      iLargest = iTab;
    }
    for(pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext){
      int iIdx = pIdx->tnum;
      assert( pIdx->pSchema==pTab->pSchema );
      if( (iDestroyed==0 || (iIdx<iDestroyed)) && iIdx>iLargest ){
        iLargest = iIdx;
      }
    }
    if( iLargest==0 ){
      return;
    }else{
      int iDb = sqlite3SchemaToIndex(pParse->db, pTab->pSchema);
      destroyRootPage(pParse, iLargest, iDb);
      iDestroyed = iLargest;
    }
  }
#endif
}

/*
** Remove entries from the sqlite_statN tables (for N in (1,2,3))//运行删除索引或删除表的指令后从sqlite_statN表删除条目(N(1、2、3))
** after a DROP INDEX or DROP TABLE command.
*/
static void sqlite3ClearStatTables(
  Parse *pParse,         /* The parsing context */            //解析上下文
  int iDb,               /* The database number */            //数据库的个数
  const char *zType,     /* "idx" or "tbl" */                 //指向索引或表
  const char *zName      /* Name of index or table */         //表或者是索引的名字
){
  int i;
  const char *zDbName = pParse->db->aDb[iDb].zName;  //语法解析上下文所指向的表或者索引的名字
  for(i=1; i<=3; i++){
    char zTab[24];
    sqlite3_snprintf(sizeof(zTab),zTab,"sqlite_stat%d",i);     //  打印出现在的表是1或者2或者3
    if( sqlite3FindTable(pParse->db, zTab, zDbName) ){        //寻找相应的表
      sqlite3NestedParse(pParse,           //嵌套的删除表
        "DELETE FROM %Q.%s WHERE %s=%Q",
        zDbName, zTab, zType, zName
      );
    }
  }
}

/*
** Generate code to drop a table.         //执行删除DROP TABLE执行
*/
void sqlite3CodeDropTable(Parse *pParse, Table *pTab, int iDb, int isView){
  Vdbe *v;
  sqlite3 *db = pParse->db;   //指向当前语法解析的上下文
  Trigger *pTrigger;   //定义一个触发器
  Db *pDb = &db->aDb[iDb];

  v = sqlite3GetVdbe(pParse);  
  assert( v!=0 );    //断言成功获取了VDBE
  sqlite3BeginWriteOperation(pParse, 1, iDb);   //开始写操作

#ifndef SQLITE_OMIT_VIRTUALTABLE
  if( IsVirtual(pTab) ){  //判断表是否是虚表
    sqlite3VdbeAddOp0(v, OP_VBegin);    //给VDBE增加操作OP_VBegin
  }
#endif

  /* Drop all triggers associated with the table being dropped. Code //删除所有与已经的删除的表相关联的触发器
  ** is generated to remove entries from sqlite_master and/or        //在有需求的情况下，移除来自Sqlite_master或sqlite_temp_mater条目的那部分代码
  ** sqlite_temp_master if required.
  */
  pTrigger = sqlite3TriggerList(pParse, pTab);   //指向触发器列表的指针
  while( pTrigger ){  //当触发器列表确实是存在的时候
    assert( pTrigger->pSchema==pTab->pSchema ||      //断言触发器确实是与表相关联的，这种检查是通过检查两者的模式是否相同
        pTrigger->pSchema==db->aDb[1].pSchema );   
    sqlite3DropTriggerPtr(pParse, pTrigger);   //循环的删除触发器列表的触发器
    pTrigger = pTrigger->pNext;   //指向触发器列表的下一个位置，也就是指向下一个相关联的触发器
  }

#ifndef SQLITE_OMIT_AUTOINCREMENT
  /* Remove any entries of the sqlite_sequence table associated with    //移除sqlite_sequence表中与已删除的表相关联的那部分记录。
  ** the table being dropped. This is done before the table is dropped   //在btree表被删除前这样做是防止sqlite_sequence表中需要移除删除的结果，因为这可能发生在auto-vacuum。
  ** at the btree level, in case the sqlite_sequence table needs to
  ** move as a result of the drop (can happen in auto-vacuum mode).
  */
  if( pTab->tabFlags & TF_Autoincrement ){
    sqlite3NestedParse(pParse,
      "DELETE FROM %Q.sqlite_sequence WHERE name=%Q",
      pDb->zName, pTab->zName
    );
  }
#endif

  /* Drop all SQLITE_MASTER table and index entries that refer to the     //删除所有SQLITE_MASTER表和索引条目参考表。
  ** table. The program name loops through the master table and deletes   //这个程序在与删除的表具有相同名称的主表之间循环，并删除表中的每一行。
  ** every row that refers to a table of the same name as the one being
  ** dropped. Triggers are handled seperately because a trigger can be     //触发器要分开处理，因为临时数据库可以创建一个触发器，而这个表在另一个数据库中
  ** created in the temp database that refers to a table in another
  ** database.
  */
  sqlite3NestedParse(pParse, 
      "DELETE FROM %Q.%s WHERE tbl_name=%Q and type!='trigger'",
      pDb->zName, SCHEMA_TABLE(iDb), pTab->zName);
  if( !isView && !IsVirtual(pTab) ){
    destroyTable(pParse, pTab);
  }

  /* Remove the table entry from SQLite's internal schema and modify
  ** the schema cookie.         //从SQLite的内部模式和修改模式的访问日志中删除表项目
  */
  if( IsVirtual(pTab) ){
    sqlite3VdbeAddOp4(v, OP_VDestroy, iDb, 0, 0, pTab->zName, 0);
  }
  sqlite3VdbeAddOp4(v, OP_DropTable, iDb, 0, 0, pTab->zName, 0);
  sqlite3ChangeCookie(pParse, iDb);
  sqliteViewResetAll(db, iDb);
}

/*
** This routine is called to do the work of a DROP TABLE statement.          //调用DROP TABLE进程执行删除表的任务。
** pName is the name of the table to be dropped.                             //pName是要删除的表的名称。
*/
void sqlite3DropTable(Parse *pParse, SrcList *pName, int isView, int noErr){
  Table *pTab;   //指向当前要删除的表的指针，而pName是当前要删除的表的名字
  Vdbe *v;   //需要VDBE引擎
  sqlite3 *db = pParse->db;   //指向当前语法分析上下文中的数据库
  int iDb;

  if( db->mallocFailed ){    //如果数据库的内存申请失败，直接跳转到另外的一个处理入口
    goto exit_drop_table;   
  }
  assert( pParse->nErr==0 );       //断言上下文的处理过程中没有任何的错误
  assert( pName->nSrc==1 );    //断言表名所指向的资源是唯一的
  if( noErr ) db->suppressErr++;    
  pTab = sqlite3LocateTable(pParse, isView, 
                            pName->a[0].zName, pName->a[0].zDatabase);    //定位到这个表
  if( noErr ) db->suppressErr--;

  if( pTab==0 ){  //如果当前根本没有表
    if( noErr ) sqlite3CodeVerifyNamedSchema(pParse, pName->a[0].zDatabase);    //进行名字核验之后确实是没有这个表
    goto exit_drop_table;  //直接跳转到另一个处理入口
  }  
  iDb = sqlite3SchemaToIndex(db, pTab->pSchema);  
  assert( iDb>=0 && iDb<db->nDb ); 

  /* If pTab is a virtual table, call ViewGetColumnNames() to ensure   //如果获取的当前表的指针指向的是一个虚表，则调用相关的视图处理函数去确定它已经被初始化了。
  ** it is initialized.
  */
  if( IsVirtual(pTab) && sqlite3ViewGetColumnNames(pParse, pTab) ){  //如果确实是虚表，并且相关的视图处理函数已经对它进行了初始化
    goto exit_drop_table;  //跳转到另一个处理入口
  }
#ifndef SQLITE_OMIT_AUTHORIZATION   //进行授权的检查，例如确定是有足够的权限删除表等
  {
    int code;
    const char *zTab = SCHEMA_TABLE(iDb);  
    const char *zDb = db->aDb[iDb].zName;  
    const char *zArg2 = 0;
    if( sqlite3AuthCheck(pParse, SQLITE_DELETE, zTab, 0, zDb)){     
      goto exit_drop_table;
    }
    if( isView ){
      if( !OMIT_TEMPDB && iDb==1 ){
        code = SQLITE_DROP_TEMP_VIEW;
      }else{
        code = SQLITE_DROP_VIEW;
      }
#ifndef SQLITE_OMIT_VIRTUALTABLE
    }else if( IsVirtual(pTab) ){
      code = SQLITE_DROP_VTABLE;
      zArg2 = sqlite3GetVTable(db, pTab)->pMod->zName;
#endif
    }else{
      if( !OMIT_TEMPDB && iDb==1 ){
        code = SQLITE_DROP_TEMP_TABLE;
      }else{
        code = SQLITE_DROP_TABLE;
      }
    }
    if( sqlite3AuthCheck(pParse, code, pTab->zName, zArg2, zDb) ){
      goto exit_drop_table;
    }
    if( sqlite3AuthCheck(pParse, SQLITE_DELETE, pTab->zName, 0, zDb) ){
      goto exit_drop_table;
    }
  }
#endif
  if( sqlite3StrNICmp(pTab->zName, "sqlite_", 7)==0 
    && sqlite3StrNICmp(pTab->zName, "sqlite_stat", 11)!=0 ){
    sqlite3ErrorMsg(pParse, "table %s may not be dropped", pTab->zName);
    goto exit_drop_table;
  }

#ifndef SQLITE_OMIT_VIEW
  /* Ensure DROP TABLE is not used on a view, and DROP VIEW is not used      //确保不使用DROP TABLE去删除视图，并且DROP VIEW也没有用于去删除表
  ** on a table.
  */
  if( isView && pTab->pSelect==0 ){   //如果是视图，并且当前的表确实没有SELECT子句
    sqlite3ErrorMsg(pParse, "use DROP TABLE to delete table %s", pTab->zName);   //发出错误信息:“使用DROP TABLE删除表”,不能去删除视图
    goto exit_drop_table;
  }
  if( !isView && pTab->pSelect ){  //如果不是视图，并且没有SELECT子句
    sqlite3ErrorMsg(pParse, "use DROP VIEW to delete view %s", pTab->zName);  //发出错误信息："使用DROP VIEW删除视图"，不能去删除表
    goto exit_drop_table;
  }
#endif

  /* Generate code to remove the table from the master table             //彻底从磁盘上删除表
  ** on disk.
  */
  v = sqlite3GetVdbe(pParse);
  if( v ){
    sqlite3BeginWriteOperation(pParse, 1, iDb);  //具备写操作权限
    sqlite3ClearStatTables(pParse, iDb, "tbl", pTab->zName);  //清除保存的所有的表状态
    sqlite3FkDropTable(pParse, pName, pTab);   //删除表
    sqlite3CodeDropTable(pParse, pTab, iDb, isView);   
  }

exit_drop_table:
  sqlite3SrcListDelete(db, pName);  
}

/*
** This routine is called to create a new foreign key on the table     //调用进程从当前正在建的表中创建一个新的外键约束。
** currently under construction.  pFromCol determines which columns    //pFromCol决定哪一列指向当前表的外键（也就是说哪一列将是外键）
** in the current table point to the foreign key.  If pFromCol==0 then   //如果pFromCol==0，那么最后插入的一列将为外键。
** connect the key to the last column inserted.  pTo is the name of      //将前面提及的表命名为pTo。
** the table referred to.  pToCol is a list of tables in the other    //pToCol作为pTo表的外键指向的表。
** pTo table that the foreign key points to.  flags contains all      
** information about the conflict resolution algorithms specified    //在DELETE、UPDATE、INSERT语句中标记所有关于冲突消除算法的信息内容。
** in the ON DELETE, ON UPDATE and ON INSERT clauses.
**
** An FKey structure is created and added to the table currently    //在当前pParse->pNewTable 字段创建并添加一个FKey结构表。
** under construction in the pParse->pNewTable field.
**
** The foreign key is set for IMMEDIATE processing.  A subsequent call     //对IMMEDIA进程设置主键约束。然后调用sqlite3DeferForeignKey()方法改变DEFERRED.
** to sqlite3DeferForeignKey() might change this to DEFERRED.   
*/
void sqlite3CreateForeignKey(
  Parse *pParse,       /* Parsing context */              //解析上下文
  ExprList *pFromCol,  /* Columns in this table that point to other table */    //创建这个表中指向另一表的列（也就是所谓的外键）。
  Token *pTo,          /* Name of the other table */                      //创建新表的名字
  ExprList *pToCol,    /* Columns in the other table */                 //创建另一表的列指向
  int flags            /* Conflict resolution algorithms. */             //添加冲突解决算法的数目。
){
  sqlite3 *db = pParse->db;   //语法分析上下文所指向的数据库
#ifndef SQLITE_OMIT_FOREIGN_KEY
  FKey *pFKey = 0;     
  FKey *pNextTo;
  Table *p = pParse->pNewTable;   //上下文创建的新表
  int nByte;
  int i;
  int nCol;     //标记列的数目
  char *z;

  assert( pTo!=0 );    //断言这个即将要添加外键的表确实是存在，如果不存在，这个函数式没有任何意义的
  if( p==0 || IN_DECLARE_VTAB ) goto fk_end;    //如果上下文指向的当前表不存在，调出这个添加外键处理函数
  if( pFromCol==0 ){    //外键为0，意味着要找到这个表最后追加的一列作为外键
    int iCol = p->nCol-1;
    if( NEVER(iCol<0) ) goto fk_end;    //列小于0说明根本没有创建表，退出函数
    if( pToCol && pToCol->nExpr!=1 ){    //如果确实存在外键指向，并且所指向的不是另一个表的一列时
      sqlite3ErrorMsg(pParse, "foreign key on %s"
         " should reference only one column of table %T",   //输出错误信息，外键应该指向另一个表的一列，这显然是理所当然的
         p->aCol[iCol].zName, pTo);
      goto fk_end;   //如果发生上面的情况，退出函数
    }
    nCol = 1;
  }else if( pToCol && pToCol->nExpr!=pFromCol->nExpr ){ 
    sqlite3ErrorMsg(pParse,
        "number of columns in foreign key does not match the number of "
        "columns in the referenced table");   //如果确实已经指定了外键，并且锁指向表的列也已经存在，但是二者的表达式数目是不同的，则输出错误的信息
    goto fk_end;
  }else{
    nCol = pFromCol->nExpr;
  }
  nByte = sizeof(*pFKey) + (nCol-1)*sizeof(pFKey->aCol[0]) + pTo->n + 1;
  if( pToCol ){
    for(i=0; i<pToCol->nExpr; i++){
      nByte += sqlite3Strlen30(pToCol->a[i].zName) + 1;
    }
  }
  pFKey = sqlite3DbMallocZero(db, nByte );
  if( pFKey==0 ){
    goto fk_end;
  }
  pFKey->pFrom = p;
  pFKey->pNextFrom = p->pFKey;
  z = (char*)&pFKey->aCol[nCol];
  pFKey->zTo = z;
  memcpy(z, pTo->z, pTo->n);
  z[pTo->n] = 0;
  sqlite3Dequote(z);
  z += pTo->n+1;
  pFKey->nCol = nCol;
  if( pFromCol==0 ){
    pFKey->aCol[0].iFrom = p->nCol-1;
  }else{
    for(i=0; i<nCol; i++){
      int j;
      for(j=0; j<p->nCol; j++){
        if( sqlite3StrICmp(p->aCol[j].zName, pFromCol->a[i].zName)==0 ){
          pFKey->aCol[i].iFrom = j;
          break;
        }
      }
      if( j>=p->nCol ){
        sqlite3ErrorMsg(pParse, 
          "unknown column \"%s\" in foreign key definition", 
          pFromCol->a[i].zName);
        goto fk_end;
      }
    }
  }
  if( pToCol ){
    for(i=0; i<nCol; i++){
      int n = sqlite3Strlen30(pToCol->a[i].zName);
      pFKey->aCol[i].zCol = z;
      memcpy(z, pToCol->a[i].zName, n);
      z[n] = 0;
      z += n+1;
    }
  }
  pFKey->isDeferred = 0;
  pFKey->aAction[0] = (u8)(flags & 0xff);            /* ON DELETE action */    //在删除操作中执行这一动作指令
  pFKey->aAction[1] = (u8)((flags >> 8 ) & 0xff);    /* ON UPDATE action */    //在更新操作中执行这一动作指令。

  assert( sqlite3SchemaMutexHeld(db, 0, p->pSchema) );
  pNextTo = (FKey *)sqlite3HashInsert(&p->pSchema->fkeyHash, 
      pFKey->zTo, sqlite3Strlen30(pFKey->zTo), (void *)pFKey
  );
  if( pNextTo==pFKey ){
    db->mallocFailed = 1;
    goto fk_end;
  }
  if( pNextTo ){
    assert( pNextTo->pPrevTo==0 );
    pFKey->pNextTo = pNextTo;
    pNextTo->pPrevTo = pFKey;
  }

  /* Link the foreign key to the table as the last step.     //链接表的外键作为最后一步要执行的指令。
  */
  p->pFKey = pFKey;
  pFKey = 0;

fk_end:
  sqlite3DbFree(db, pFKey);
#endif /* !defined(SQLITE_OMIT_FOREIGN_KEY) */    //不定义（SQLITE_OMIT_FOREIGN_KEY）
  sqlite3ExprListDelete(db, pFromCol);
  sqlite3ExprListDelete(db, pToCol);
}

/*
** This routine is called when an INITIALLY IMMEDIATE or INITIALLY DEFERRED          //程序执行包含有INITIALLY IMMEDIA或INITIALLY DEFERRED 内容的外键定义。
** clause is seen as part of a foreign key definition.  The isDeferred    
** parameter is 1 for INITIALLY DEFERRED and 0 for INITIALLY IMMEDIATE.    //INITIALLY DEFERRED的isDeferred参数为1,INITIALLY IMMEDIATE的isDeferred参数为0。
** The behavior of the most recently created foreign key is adjusted        //对刚创建的外键做相应的调整。
** accordingly.
*/
void sqlite3DeferForeignKey(Parse *pParse, int isDeferred){
#ifndef SQLITE_OMIT_FOREIGN_KEY
  Table *pTab;
  FKey *pFKey;
  if( (pTab = pParse->pNewTable)==0 || (pFKey = pTab->pFKey)==0 ) return;    //如果pTab = pParse->pNewTable)==0或者pFKey = pTab->pFKey)==0，语句执行返回指令。
  assert( isDeferred==0 || isDeferred==1 ); /* EV: R-30323-21917 */    //声明isDeferred==0或者isDeferred==1
  pFKey->isDeferred = (u8)isDeferred;
#endif
}
//
/*
** Generate code that will erase and refill index *pIdx.  This is    //生成删除和补充*pIdx索引的代码。
** used to initialize a newly created index or to recompute the      //初始化新创建的索引或验算关于REINDEX指令相应的索引
** content of an index in response to a REINDEX command.
**
** if memRootPage is not negative, it means that the index is newly   //如果memRootPage不是负数,这意味着该指数是新创建的。
** created.  The register specified by memRootPage contains the      //memRootPage制定的寄存器包含索引的根页码。
** root page number of the index.  If memRootPage is negative, then
** the index already exists and must be cleared before being refilled and
** the root page number of the index is taken from pIndex->tnum.   //如果memRootPage是负数，那么该索引已经存在，并且必须在填充取自pIndex->tnum索引的根页码前删除该索引。
*/
static void sqlite3RefillIndex(Parse *pParse, Index *pIndex, int memRootPage){
  Table *pTab = pIndex->pTable;  /* The table that is indexed */            //创建索引的表
  int iTab = pParse->nTab++;     /* Btree cursor used for pTab */          //用于pTab Btree
  int iIdx = pParse->nTab++;     /* Btree cursor used for pIndex */        //用于pIndex Btree的游标
  int iSorter;                   /* Cursor opened by OpenSorter (if in use) */       //当使用的时候使用OpenSorter打开游标
  int addr1;                     /* Address of top of loop */                       //地址的循环次数
  int addr2;                     /* Address to jump to for next iteration */       //地址跳转到下一个迭代的次数
  int tnum;                      /* Root page of index */                         //索引的根页
  Vdbe *v;                       /* Generate code into this virtual machine */       //生成代码到这个虚拟机
  KeyInfo *pKey;                 /* KeyInfo for index */                       //KeyInfo为索引
#ifdef SQLITE_OMIT_MERGE_SORT
  int regIdxKey;                 /* Registers containing the index key */           //寄存器包含索引键
#endif
  int regRecord;                 /* Register holding assemblied index record */       //登记所得到的索引的记录
  sqlite3 *db = pParse->db;      /* The database connection */                        //连接数据库
  int iDb = sqlite3SchemaToIndex(db, pIndex->pSchema);

#ifndef SQLITE_OMIT_AUTHORIZATION
  if( sqlite3AuthCheck(pParse, SQLITE_REINDEX, pIndex->zName, 0,
      db->aDb[iDb].zName ) ){
    return;
  }
#endif

  /* Require a write-lock on the table to perform this operation */     //请求一个写锁在这个表上执行这些操作
  sqlite3TableLock(pParse, iDb, pTab->tnum, 1, pTab->zName);

  v = sqlite3GetVdbe(pParse);
  if( v==0 ) return;
  if( memRootPage>=0 ){
    tnum = memRootPage;
  }else{
    tnum = pIndex->tnum;
    sqlite3VdbeAddOp2(v, OP_Clear, tnum, iDb);
  }
  pKey = sqlite3IndexKeyinfo(pParse, pIndex);
  sqlite3VdbeAddOp4(v, OP_OpenWrite, iIdx, tnum, iDb, 
                    (char *)pKey, P4_KEYINFO_HANDOFF);
  sqlite3VdbeChangeP5(v, OPFLAG_BULKCSR|((memRootPage>=0)?OPFLAG_P2ISREG:0));

#ifndef SQLITE_OMIT_MERGE_SORT
  /* Open the sorter cursor if we are to use one. */            //如果要使用游标打开游标分选机
  iSorter = pParse->nTab++;
  sqlite3VdbeAddOp4(v, OP_SorterOpen, iSorter, 0, 0, (char*)pKey, P4_KEYINFO);
#else
  iSorter = iTab;
#endif

  /* Open the table. Loop through all rows of the table, inserting index    //打开表，遍历表的所有行，将索引记录插入到分选机
  ** records into the sorter. */
  sqlite3OpenTable(pParse, iTab, iDb, pTab, OP_OpenRead);
  addr1 = sqlite3VdbeAddOp2(v, OP_Rewind, iTab, 0);
  regRecord = sqlite3GetTempReg(pParse);

#ifndef SQLITE_OMIT_MERGE_SORT
  sqlite3GenerateIndexKey(pParse, pIndex, iTab, regRecord, 1);
  sqlite3VdbeAddOp2(v, OP_SorterInsert, iSorter, regRecord);
  sqlite3VdbeAddOp2(v, OP_Next, iTab, addr1+1);
  sqlite3VdbeJumpHere(v, addr1);
  addr1 = sqlite3VdbeAddOp2(v, OP_SorterSort, iSorter, 0);
  if( pIndex->onError!=OE_None ){
    int j2 = sqlite3VdbeCurrentAddr(v) + 3;
    sqlite3VdbeAddOp2(v, OP_Goto, 0, j2);
    addr2 = sqlite3VdbeCurrentAddr(v);
    sqlite3VdbeAddOp3(v, OP_SorterCompare, iSorter, j2, regRecord);
    sqlite3HaltConstraint(
        pParse, OE_Abort, "indexed columns are not unique", P4_STATIC
    );
  }else{
    addr2 = sqlite3VdbeCurrentAddr(v);
  }
  sqlite3VdbeAddOp2(v, OP_SorterData, iSorter, regRecord);
  sqlite3VdbeAddOp3(v, OP_IdxInsert, iIdx, regRecord, 1);
  sqlite3VdbeChangeP5(v, OPFLAG_USESEEKRESULT);
#else
  regIdxKey = sqlite3GenerateIndexKey(pParse, pIndex, iTab, regRecord, 1);
  addr2 = addr1 + 1;
  if( pIndex->onError!=OE_None ){
    const int regRowid = regIdxKey + pIndex->nColumn;
    const int j2 = sqlite3VdbeCurrentAddr(v) + 2;
    void * const pRegKey = SQLITE_INT_TO_PTR(regIdxKey);

    /* The registers accessed by the OP_IsUnique opcode were allocated  //通过OP_IsUnique用Sqlite3GetTempRange()分配的操作码的内部方法Sqlite3GenerateIndexKey()方法的调用访问寄存器。
    ** using sqlite3GetTempRange() inside of the sqlite3GenerateIndexKey()
    ** call above. Just before that function was freed they were released   //在启用上面的操作之前，当这些进程是空闲的时候用sqliteReleaseTempRange()方法释放进程，用合适的编译器重新编译。
    ** (made available to the compiler for reuse) using 
    ** sqlite3ReleaseTempRange(). So in some ways having the OP_IsUnique    //因此在某些方面拥有OP_IsUnique操作码的属性值存储值时有时可能存在某些危险。
    ** opcode use the values stored within seems dangerous. However, since
    ** we can be sure that no other temp registers have been allocated
    ** since sqlite3ReleaseTempRange() was called, it is safe to do so.    //然而，我们可以确定没有其它临时寄存器使用sqlite3ReleaseTempRange()方法事，这样的操作是安全的。 
    */
    sqlite3VdbeAddOp4(v, OP_IsUnique, iIdx, j2, regRowid, pRegKey, P4_INT32);
    sqlite3HaltConstraint(
        pParse, OE_Abort, "indexed columns are not unique", P4_STATIC);
  }
  sqlite3VdbeAddOp3(v, OP_IdxInsert, iIdx, regRecord, 0);
  sqlite3VdbeChangeP5(v, OPFLAG_USESEEKRESULT);
#endif
  sqlite3ReleaseTempReg(pParse, regRecord);
  sqlite3VdbeAddOp2(v, OP_SorterNext, iSorter, addr2);
  sqlite3VdbeJumpHere(v, addr1);

  sqlite3VdbeAddOp1(v, OP_Close, iTab);
  sqlite3VdbeAddOp1(v, OP_Close, iIdx);
  sqlite3VdbeAddOp1(v, OP_Close, iSorter);
}

/*
** Create a new index for an SQL table.  pName1.pName2 is the name of the index //为SQL表创建新的索引
** and pTblList is the name of the table that is to be indexed.  Both will     //pName1.pName2是这个索引的名字，pTblList是被索引的表的名字。
** be NULL for a primary key or an index that is created to satisfy a          //创建的用于满足UNIQUE约束的主键或索引都是空的。
** UNIQUE constraint.  If pTable and pIndex are NULL, use pParse->pNewTable   //如果pTable和pIndex是空的，用pParse->pNewTable作为被索引的表（换句话说就是当pTable和pIndex均为空的时候，采用现在正在创建的表作为要建立索引的表）。
** as the table to be indexed.  pParse->pNewTable is a table that is          //pParse->pNewTable是一个正在使用CREATE TABLE语句去构建的表。
** currently being constructed by a CREATE TABLE statement.
**
** pList is a list of columns to be indexed.  pList will be NULL if this     //pList要被索引的列。
** is a primary key or unique-constraint on the most recent column added     //如果这个主键或唯一约束最近添加到正在建设的表中，pList将是NULL。
** to the table currently under construction.  
**
** If the index is created successfully, return a pointer to the new Index    //如果索引创建成功，将返回一个新的索引结构。
** structure. This is used by sqlite3AddPrimaryKey() to mark the index        //当Index.autoIndex==2时，使用sqlite3AddPrimaryKey()方法标记这个索引，作为这个表的主键。
** as the tables primary key (Index.autoIndex==2).
*/
Index *sqlite3CreateIndex(
  Parse *pParse,     /* All information about this parse */                 //关于这个语法解析器的所有信息
  Token *pName1,     /* First part of index name. May be NULL */            //索引名称的第一部分，这部分可能是空的。
  Token *pName2,     /* Second part of index name. May be NULL */           //索引名称的第二部分，可能是空的。
  SrcList *pTblName, /* Table to index. Use pParse->pNewTable if 0 */       //被索引的表的名字，当为0时用pParse->pNewTable表示被索引的表
  ExprList *pList,   /* A list of columns to be indexed */                  //被索引的列
  int onError,       /* OE_Abort, OE_Ignore, OE_Replace, or OE_None */       //关于OE_Abort、OE_Ignore OE_Replace或OE_None的参数
  Token *pStart,     /* The CREATE token that begins this statement */       //CREATE指令开始这样的声明
  Token *pEnd,       /* The ")" that closes the CREATE INDEX statement */     //当出现）时关闭CREATE INDEX语句
  int sortOrder,     /* Sort order of primary key when pList==NULL */       //当被索引的列为空的时候，那么会去排序主键
  int ifNotExist     /* Omit error if index already exists */               //如果索引已经存在忽略错误
){
  Index *pRet = 0;     /* Pointer to return */                               //指针返回
  Table *pTab = 0;     /* Table to be indexed */                           //被索引的表（也就是将要创建索引的表）
  Index *pIndex = 0;   /* The index to be created */                       //要创建的索引
  char *zName = 0;     /* Name of the index */                             //索引的名称
  int nName;           /* Number of characters in zName */                //zName（索引名字）的字符数
  int i, j;
  Token nullId;        /* Fake token for an empty ID list */             //假令牌空ID列表
  DbFixer sFix;        /* For assigning database names to pTable */      //为pTable指定数据库的名称
  int sortOrderMask;   /* 1 to honor DESC in index.  0 to ignore. */     //在索引中出现1时降序排列，0的时候升序排列
  sqlite3 *db = pParse->db;                                                    //语法分析上下文所指向的数据库
  Db *pDb;             /* The specific table containing the indexed database */     //包含索引数据库的具体的表
  int iDb;             /* Index of the database that is being written */            //记录数据库的索引
  Token *pName = 0;    /* Unqualified name of the index to create */                //创建索引的不合格的名称
  struct ExprList_item *pListItem; /* For looping over pList */                   //指向 struct ExprList_item类型数据的指针变量pListItem
  int nCol;
  int nExtra = 0;
  char *zExtra;

  assert( pStart==0 || pEnd!=0 ); /* pEnd must be non-NULL if pStart is */     //pStart为0或pEnd不为0时
  assert( pParse->nErr==0 );      /* Never called with prior errors */         //当变量 pParse->nErr为0时，声明这一变量。
  if( db->mallocFailed || IN_DECLARE_VTAB ){
    goto exit_create_index;    //如果数据库内存分配失败，跳转到另一个处理入口
  }
  if( SQLITE_OK!=sqlite3ReadSchema(pParse) ){
    goto exit_create_index;   //没有切换到读表模式时候，退出函数的执行
  }

  /*
  ** Find the table that is to be indexed.  Return early if not found.   //找到索引的表。如果没有发现提前返回。
  */
  if( pTblName!=0 ){   //索引的表确实是存在的

    /* Use the two-part index name to determine the database           //使用两部分的索引名称来确定数据库搜索表。
    ** to search for the table. 'Fix' the table name to this db        //修复这个这个数据库查找表的表名。
    ** before looking up the table.
    */
    assert( pName1 && pName2 );   //断言索引的两段表名是存在的，这样就保证了当索引不存在的时候提前退出这个函数
    iDb = sqlite3TwoPartName(pParse, pName1, pName2, &pName);    //根据两段表名函数返回找到的数据库    
    if( iDb<0 ) goto exit_create_index;   //说明不存在这个数据库，直接退出函数
    assert( pName && pName->z );   //断言这个被创建的索引名是合法的，如果不合法，直接退出

#ifndef SQLITE_OMIT_TEMPDB
    /* If the index name was unqualified, check if the table    //如果索引名称不合格,检查表是否是是一个临时表。
    ** is a temp table. If so, set the database to 1. Do not do this      //如果是这样,将数据库设置为1。
    ** if initialising a database schema.                                 //如果不是这样，初始化数据库模式。
    */
    if( !db->init.busy ){   //如果数据库的初始化工作已经完成
      pTab = sqlite3SrcListLookup(pParse, pTblName);    //查找索引表
      if( pName2->n==0 && pTab && pTab->pSchema==db->aDb[1].pSchema ){    //确定其模式
        iDb = 1;    //被索引的表是一个临时表，将其数据库设置为1
      }
    }
#endif

    if( sqlite3FixInit(&sFix, pParse, iDb, "index", pName) &&
        sqlite3FixSrcList(&sFix, pTblName)
    ){
      /* Because the parser constructs pTblName from a single identifier,
      ** sqlite3FixSrcList can never fail. */             //因为解析器构造的pTblName是单一标示符，从而sqlite3FixSrcList永远不会失效。
      assert(0);
    }
    pTab = sqlite3LocateTable(pParse, 0, pTblName->a[0].zName,     //定位这个被索引的表
        pTblName->a[0].zDatabase);
    if( !pTab || db->mallocFailed ) goto exit_create_index;   //如果不存在或者数据库内存分配失败，停止执行当前创建索引函数
    assert( db->aDb[iDb].pSchema==pTab->pSchema );    //断言数据库要创建表的模式和索引表的模式是相同的
  }else{
    assert( pName==0 );  
    assert( pStart==0 );  
    pTab = pParse->pNewTable;
    if( !pTab ) goto exit_create_index;
    iDb = sqlite3SchemaToIndex(db, pTab->pSchema);
  }
  pDb = &db->aDb[iDb];

  assert( pTab!=0 );
  assert( pParse->nErr==0 );
  if( sqlite3StrNICmp(pTab->zName, "sqlite_", 7)==0 
       && memcmp(&pTab->zName[7],"altertab_",9)!=0 ){
    sqlite3ErrorMsg(pParse, "table %s may not be indexed", pTab->zName);
    goto exit_create_index;
  }
#ifndef SQLITE_OMIT_VIEW
  if( pTab->pSelect ){
    sqlite3ErrorMsg(pParse, "views may not be indexed");
    goto exit_create_index;
  }
#endif
#ifndef SQLITE_OMIT_VIRTUALTABLE
  if( IsVirtual(pTab) ){
    sqlite3ErrorMsg(pParse, "virtual tables may not be indexed");
    goto exit_create_index;
  }
#endif

  /*
  ** Find the name of the index.  Make sure there is not already another    //找出索引的名称，确保这里没有其他的索引或同样的索引名称。
  ** index or table with the same name.  
  **
  ** Exception:  If we are reading the names of permanent indices from the         //异常：如果我们读取的来自sqlite_master表的名称的永久指数和索引的名称与临时表或索引的名称发生冲突，我们将处理这个索引。
  ** sqlite_master table (because some other process changed the schema) and
  ** one of the index names collides with the name of a temporary table or
  ** index, then we will continue to process this index.
  **
  ** If pName==0 it means that we are
  ** dealing with a primary key or UNIQUE constraint.  We have to invent our     //如果pName为0意味着我们正在处理一个主键或唯一约束。我们必须创建自己的名称。
  ** own name.
  */
  if( pName ){    //索引的名字是合法的
    zName = sqlite3NameFromToken(db, pName);   //将索引的名字转化为符号表中的符号
    if( zName==0 ) goto exit_create_index;    //如果符号为空，则意味着索引名是没有的或者不合法的，直接退出函数
    assert( pName->z!=0 );     //断言索引名是合法的
    if( SQLITE_OK!=sqlite3CheckObjectName(pParse, zName) ){   
      goto exit_create_index;
    }
    if( !db->init.busy ){    //数据库已经初始化完成
      if( sqlite3FindTable(db, zName, 0)!=0 ){    //寻找表
        sqlite3ErrorMsg(pParse, "there is already a table named %s", zName);    //如果索引名和表名冲突发出错误消息：已经有表被命名为当前的名字
        goto exit_create_index;   //退出函数，跳转到其他的处理入口
      }  
    }
    if( sqlite3FindIndex(db, zName, pDb->zName)!=0 ){    //寻找索引
      if( !ifNotExist ){
        sqlite3ErrorMsg(pParse, "index %s already exists", zName);   //发现索引已经存在，发送错误消息
      }else{
        assert( !db->init.busy );   //断言数据库的初始化工作已经完成
        sqlite3CodeVerifySchema(pParse, iDb);   
      }
      goto exit_create_index;
    }
  }else{    //我们现在处理的谁一个主键或者是唯一性约束
    int n;
    Index *pLoop;
    for(pLoop=pTab->pIndex, n=1; pLoop; pLoop=pLoop->pNext, n++){}
    zName = sqlite3MPrintf(db, "sqlite_autoindex_%s_%d", pTab->zName, n);   //输出被索引的表的索引名，此时的索引的名字是我们自己创建的
    if( zName==0 ){
      goto exit_create_index;   //索引名为空说明索引根本是不存在的
    }
  }

  /* Check for authorization to create an index.              //检查创建索引的授权
  */
#ifndef SQLITE_OMIT_AUTHORIZATION
  {
    const char *zDb = pDb->zName;
    if( sqlite3AuthCheck(pParse, SQLITE_INSERT, SCHEMA_TABLE(iDb), 0, zDb) ){
      goto exit_create_index;
    }
    i = SQLITE_CREATE_INDEX;
    if( !OMIT_TEMPDB && iDb==1 ) i = SQLITE_CREATE_TEMP_INDEX;
    if( sqlite3AuthCheck(pParse, i, zName, pTab->zName, zDb) ){
      goto exit_create_index;
    }
  }
#endif

  /* If pList==0, it means this routine was called to make a primary
  ** key out of the last column added to the table under construction.    //如果pList = = 0，意味着具有一个主键的表的程序的最后一列添加到正在构造的表中。
  ** So create a fake list to simulate this.                             //因此创建一个假的列表来进行模拟这一操作。
  */
  if( pList==0 ){
    nullId.z = pTab->aCol[pTab->nCol-1].zName;
    nullId.n = sqlite3Strlen30((char*)nullId.z);
    pList = sqlite3ExprListAppend(pParse, 0, 0);
    if( pList==0 ) goto exit_create_index;
    sqlite3ExprListSetName(pParse, pList, &nullId, 0);
    pList->a[0].sortOrder = (u8)sortOrder;
  }

  /* Figure out how many bytes of space are required to store explicitly   //找出需要存储多少字节的空间，显示指定序列的名称。
  ** specified collation sequence names.
  */
  for(i=0; i<pList->nExpr; i++){
    Expr *pExpr = pList->a[i].pExpr;
    if( pExpr ){
      CollSeq *pColl = pExpr->pColl;
      /* Either pColl!=0 or there was an OOM failure.  But if an OOM   //pColl != 0或者有一个OOM失效。
      ** failure we have quit before reaching this point. */           //如果OOM失效失效，在达到这一点时撤销这一操作。
      if( ALWAYS(pColl) ){
        nExtra += (1 + sqlite3Strlen30(pColl->zName));
      }
    }
  }

  /* 
  ** Allocate the index structure.                                 //分配索引结构
  */
  nName = sqlite3Strlen30(zName);
  nCol = pList->nExpr;
  pIndex = sqlite3DbMallocZero(db, 
      ROUND8(sizeof(Index)) +              /* Index structure  */            //索引结构
	  ROUND8(sizeof(tRowcnt)*(nCol + 1)) +   /* Index.aiRowEst   */
      sizeof(char *)*nCol +                /* Index.azColl     */
      sizeof(int)*nCol +                   /* Index.aiColumn   */
      sizeof(u8)*nCol +                    /* Index.aSortOrder */
      nName + 1 +                          /* Index.zName      */              //索引的名字
      nExtra                               /* Collation sequence names */         //校对序列的名字
  );
  if( db->mallocFailed ){    //如果数据库的内存分配失败，则立即结束整个程序的执行
    goto exit_create_index;
  }
  zExtra = (char*)pIndex;
  pIndex->aiRowEst = (tRowcnt*)&zExtra[ROUND8(sizeof(Index))];
  pIndex->azColl = (char**)
     ((char*)pIndex->aiRowEst + ROUND8(sizeof(tRowcnt)*nCol+1));
  assert( EIGHT_BYTE_ALIGNMENT(pIndex->aiRowEst) );
  assert( EIGHT_BYTE_ALIGNMENT(pIndex->azColl) );
  pIndex->aiColumn = (int *)(&pIndex->azColl[nCol]);
  pIndex->aSortOrder = (u8 *)(&pIndex->aiColumn[nCol]);
  pIndex->zName = (char *)(&pIndex->aSortOrder[nCol]);
  zExtra = (char *)(&pIndex->zName[nName+1]);
  memcpy(pIndex->zName, zName, nName+1);
  pIndex->pTable = pTab;
  pIndex->nColumn = pList->nExpr;
  pIndex->onError = (u8)onError;
  pIndex->autoIndex = (u8)(pName==0);
  pIndex->pSchema = db->aDb[iDb].pSchema;
  assert( sqlite3SchemaMutexHeld(db, iDb, 0) );

  /* Check to see if we should honor DESC requests on index columns          //检查一下看看我们是否应该将请求的索引列进行将序排列。
  */
  if( pDb->pSchema->file_format>=4 ){
    sortOrderMask = -1;   /* Honor DESC */                                  //将序排列
  }else{
    sortOrderMask = 0;    /* Ignore DESC */                                 //升序排列
  }

  /* Scan the names of the columns of the table to be indexed and          //扫描被索引表的列名，并且加载这些列目录到索引结构中。
  ** load the column indices into the Index structure.  Report an error
  ** if any column is not found.                                          //如果没有发现某些列，报告这些错误。
  **
  ** TODO:  Add a test to make sure that the same column is not named     //待办事项：添加一个测试，以确保相同的列不会被命名在相同的索引上。
  ** more than once within the same index.  Only the first instance of     //只有这列的第一个实例对象将会被优化器使用。
  ** the column will ever be used by the optimizer.  Note that using the   //注意，使用同一列超过一次不应该是一个错误，因为这只影响向后的兼容性，这应该是一个警告。
  ** same column more than once cannot be an error because that would 
  ** break backwards compatibility - it needs to be a warning.
  */
  for(i=0, pListItem=pList->a; i<pList->nExpr; i++, pListItem++){
    const char *zColName = pListItem->zName;
    Column *pTabCol;
    int requestedSortOrder;
    char *zColl;                   /* Collation sequence name */                //定义排序序列的名称

    for(j=0, pTabCol=pTab->aCol; j<pTab->nCol; j++, pTabCol++){
      if( sqlite3StrICmp(zColName, pTabCol->zName)==0 ) break;   
    }
    if( j>=pTab->nCol ){
      sqlite3ErrorMsg(pParse, "table %s has no column named %s",
        pTab->zName, zColName);
      pParse->checkSchema = 1;
      goto exit_create_index;
    }
    pIndex->aiColumn[i] = j;
    /* Justification of the ALWAYS(pListItem->pExpr->pColl):  Because of   //关于pListItem - > pExpr - > pColl常见的原因：因为“idxlist”非终结符是解析器，
    ** the way the "idxlist" non-terminal is constructed by the parser,    //当pListItem - > pExpr也不是空的时候，那么pListItem - > pExpr - > pColl必须存在,
    ** must exist or else there must have been an OOM error.  But if there  //否则一定那将是一个OOM错误，我们将达不到这一点。
    ** if pListItem->pExpr is not null then either pListItem->pExpr->pColl  
    ** was an OOM error, we would never reach this point. */
    if( pListItem->pExpr && ALWAYS(pListItem->pExpr->pColl) ){
      int nColl;
      zColl = pListItem->pExpr->pColl->zName;
      nColl = sqlite3Strlen30(zColl) + 1;
      assert( nExtra>=nColl );
      memcpy(zExtra, zColl, nColl);
      zColl = zExtra;
      zExtra += nColl;
      nExtra -= nColl;
    }else{
      zColl = pTab->aCol[j].zColl;
      if( !zColl ){
        zColl = "BINARY";
      }
    }
    if( !db->init.busy && !sqlite3LocateCollSeq(pParse, zColl) ){
      goto exit_create_index;
    }
    pIndex->azColl[i] = zColl;
    requestedSortOrder = pListItem->sortOrder & sortOrderMask;
    pIndex->aSortOrder[i] = (u8)requestedSortOrder;
  }
  sqlite3DefaultRowEst(pIndex);

  if( pTab==pParse->pNewTable ){
    /* This routine has been called to create an automatic index as a      //这个程序被用于创建一个自动索引，这个自动索引作为主键或
    ** result of a PRIMARY KEY or UNIQUE clause on a column definition, or  //唯一子句的列定义，或者是后来的主键或唯一子句的列定义。
    ** a PRIMARY KEY or UNIQUE clause following the column definitions.
    ** i.e. one of:
    **
    ** CREATE TABLE t(x PRIMARY KEY, y);   //创建表t(x PRIMARY KEY, y)
    ** CREATE TABLE t(x, y, UNIQUE(x, y));  //创建表t(x, y, UNIQUE(x, y))
    **
    ** Either way, check to see if the table already has such an index. If   //不管怎么样，查看表中是否有这样的索引。
    ** so, don't bother creating this one. This only applies to              //如果有，不要在创建这样的。
    ** automatically created indices. Users can do as they wish with         //这只适用于自动创建索引。
    ** explicit indices.                                                     //当用户当要显示这些明确的索引时，可以执行上述操作。
    **
    ** Two UNIQUE or PRIMARY KEY constraints are considered equivalent    //唯一约束或主键约束被认为是等价的（这也是为了防止第二个的产生），
    ** (and thus suppressing the second one) even if they have different  //即使他们有不同的排列顺序。
    ** sort orders.
    **
    ** If there are different collating sequences or if the columns of   //如果有不同的排序序列或者猎德约束发生在不同的指令中，
    ** the constraint occur in different orders, then the constraints are  //那么将会被认为这一约束是和单独的约束是有区别的。
    ** considered distinct and both result in separate indices.
    */
    Index *pIdx;
    for(pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext){
      int k;
      assert( pIdx->onError!=OE_None );   //断言没有发生错误
      assert( pIdx->autoIndex );   //断言存在自动的索引
      assert( pIndex->onError!=OE_None );

      if( pIdx->nColumn!=pIndex->nColumn ) continue;
      for(k=0; k<pIdx->nColumn; k++){
        const char *z1;
        const char *z2;
        if( pIdx->aiColumn[k]!=pIndex->aiColumn[k] ) break;
        z1 = pIdx->azColl[k];
        z2 = pIndex->azColl[k];
        if( z1!=z2 && sqlite3StrICmp(z1, z2) ) break;
      }
      if( k==pIdx->nColumn ){
        if( pIdx->onError!=pIndex->onError ){
          /* This constraint creates the same index as a previous
          ** constraint specified somewhere in the CREATE TABLE statement.  //在CREATE TABLE语句声明的早前的约束机制中约束创建相同的约束。
          ** However the ON CONFLICT clauses are different. If both this   //然而，ON CONFLICT从句和上述不同。
          ** constraint and the previous equivalent constraint have explicit   //如果这个约束和前面等效的约束有明确的CONFLICT子句，那么就会发生错误。
          ** ON CONFLICT clauses this is an error. Otherwise, use the      //否则的话，使用显示的指定行为的索引。
          ** explicitly specified behaviour for the index.
          */
          if( !(pIdx->onError==OE_Default || pIndex->onError==OE_Default) ){
            sqlite3ErrorMsg(pParse, 
                "conflicting ON CONFLICT clauses specified", 0);
          }
          if( pIdx->onError==OE_Default ){
            pIdx->onError = pIndex->onError;
          }
        }
        goto exit_create_index;
      }
    }
  }

  /* Link the new Index structure to its table and to the other   //链接新的索引结构和其它的内存数据库结构
  ** in-memory database structures. 
  */
  if( db->init.busy ){   //数据库正在进行初始化的操作
    Index *p;   //即将要建立的索引名
    assert( sqlite3SchemaMutexHeld(db, 0, pIndex->pSchema) );
    p = sqlite3HashInsert(&pIndex->pSchema->idxHash, 
                          pIndex->zName, sqlite3Strlen30(pIndex->zName),
                          pIndex);    //调用函数进行hash插入操作
    if( p ){   //如果索引确实是中存在的
      assert( p==pIndex );  /* Malloc must have failed */          //当p==pIndex时，Malloc已经出错。
      db->mallocFailed = 1;
      goto exit_create_index;
    }
    db->flags |= SQLITE_InternChanges;
    if( pTblName!=0 ){
      pIndex->tnum = db->init.newTnum;
    }
  }

  /* If the db->init.busy is 0 then create the index on disk.  This   //如果db->init.busy为0，那么在硬盘上创建索引。
  ** involves writing the index into the master table and filling in the  //这涉及到在主表上编写索引，并在当前表中填写索引的内容。
  ** index with the current table contents.
  **
  ** The db->init.busy is 0 when the user first enters a CREATE INDEX //当第一次用CREATE INDEX指令时db->init.busy为0。
  ** command.  db->init.busy is 1 when a database is opened and       //当打开数据库时db->init.busy为1,然后从主表上读取CREATE INDEX声明。
  ** CREATE INDEX statements are read out of the master table.  In
  ** the latter case the index already exists on disk, which is why  //对于后一种情况，该索引已经存在在磁盘上，这就是为什么我们不用再重新创建它。
  ** we don't want to recreate it.                                    
  **
  ** If pTblName==0 it means this index is generated as a primary key  //当 pTblName==0那意味着索引已经生成了主键或CREATE TABLE声明已经生成了 UNIQUE约束。
  ** or UNIQUE constraint of a CREATE TABLE statement.  Since the table  //由于表刚刚创建，它不包含数据和索引，因为可以跳过初始化这一步骤。
  ** has just been created, it contains no data and the index initialization
  ** step can be skipped.
  */
  else{ /* if( db->init.busy==0 ) */     //如果db->init.busy==0
    Vdbe *v;
    char *zStmt;
    int iMem = ++pParse->nMem;

    v = sqlite3GetVdbe(pParse);
    if( v==0 ) goto exit_create_index;


    /* Create the rootpage for the index   //创建的rootpage索引
    */
    sqlite3BeginWriteOperation(pParse, 1, iDb);
    sqlite3VdbeAddOp2(v, OP_CreateIndex, iDb, iMem);

    /* Gather the complete text of the CREATE INDEX statement into  //收集CREATE INDEX语句的完整文本到zStmt变量中
    ** the zStmt variable
    */
    if( pStart ){
      assert( pEnd!=0 );
      /* A named index with an explicit CREATE INDEX statement */  //一个命名索引和一个显式创建索引语句
      zStmt = sqlite3MPrintf(db, "CREATE%s INDEX %.*s",
        onError==OE_None ? "" : " UNIQUE",
        (int)(pEnd->z - pName->z) + 1,
        pName->z);
    }else{
      /* An automatic index created by a PRIMARY KEY or UNIQUE constraint */  //当zStmt = sqlite3MPrintf(" ")时，通过主键或唯一约束创建一个自动索引。
      /* zStmt = sqlite3MPrintf(""); */
      zStmt = 0;
    }

    /* Add an entry in sqlite_master for this index   //在sqlite_master索引中添加一个入口
    */
    sqlite3NestedParse(pParse, 
        "INSERT INTO %Q.%s VALUES('index',%Q,%Q,#%d,%Q);",
        db->aDb[iDb].zName, SCHEMA_TABLE(iDb),
        pIndex->zName,
        pTab->zName,
        iMem,
        zStmt
    );
    sqlite3DbFree(db, zStmt);

    /* Fill the index with data and reparse the schema. Code an OP_Expire     //填充这一索引的数据，然后重新解析模式。
    ** to invalidate all pre-compiled statements.                            //代码OP_Expire中所有无效的预编译语句。
    */
    if( pTblName ){
      sqlite3RefillIndex(pParse, pIndex, iMem);
      sqlite3ChangeCookie(pParse, iDb);
      sqlite3VdbeAddParseSchemaOp(v, iDb,
         sqlite3MPrintf(db, "name='%q' AND type='index'", pIndex->zName));
      sqlite3VdbeAddOp1(v, OP_Expire, 0);
    }
  }

  /* When adding an index to the list of indices for a table, make   //当对一个索引列表添加索引，
  ** sure all indices labeled OE_Replace come after all those labeled  //确保所有的OE_Replace标记中，哪些标有OE_Ignore。
  ** OE_Ignore.  This is necessary for the correct constraint check  //在sqlite3GenerateConstraintChecks()方法为了正确的约束检查，对于UPDATE或者INSERT声明这些是有必要的。
  ** processing (in sqlite3GenerateConstraintChecks()) as part of
  ** UPDATE and INSERT statements.  
  */
  if( db->init.busy || pTblName==0 ){
    if( onError!=OE_Replace || pTab->pIndex==0
         || pTab->pIndex->onError==OE_Replace){
      pIndex->pNext = pTab->pIndex;
      pTab->pIndex = pIndex;
    }else{
      Index *pOther = pTab->pIndex;
      while( pOther->pNext && pOther->pNext->onError!=OE_Replace ){
        pOther = pOther->pNext;
      }
      pIndex->pNext = pOther->pNext;
      pOther->pNext = pIndex;
    }
    pRet = pIndex;
    pIndex = 0;
  }

  /* Clean up before exiting */              //退出前清理
exit_create_index:
  if( pIndex ){
    sqlite3DbFree(db, pIndex->zColAff);
    sqlite3DbFree(db, pIndex);
  }
  sqlite3ExprListDelete(db, pList);
  sqlite3SrcListDelete(db, pTblName);
  sqlite3DbFree(db, zName);
  return pRet;
}

/*
** Fill the Index.aiRowEst[] array with default information - information  //用默认的信息填充Index.aiRowEst[]数组，
** to be used when we have not run the ANALYZE command.                    //当我们不运行ANALYZE指令时，就使用这些信息。
**
** aiRowEst[0] is suppose to contain the number of elements in the index.  //aiRowEst[0]是索引中的基本内容。
** Since we do not know, guess 1 million.  aiRowEst[1] is an estimate of the  //当我们在不知道的时候，可以猜测为100万。
** number of rows in the table that match any particular value of the        //aiRowEst[1]是表行中的一个估计数字，它和表中第一列的特殊数值相匹配。
** first column of the index.  aiRowEst[2] is an estimate of the number     //aiRowEst[2]是表中其它行的估计数值，它和表中从表中第2列开始的特殊数值想匹配。
** of rows that match any particular combiniation of the first 2 columns
** of the index.  And so forth.  It must always be the case that           //就这样，不断地进行相应的匹配。
*
**           aiRowEst[N]<=aiRowEst[N-1]
**           aiRowEst[N]>=1
**
** Apart from that, we have little to go on besides intuition as to   //除此之外，我们消除了如何对aiRowEst[]进行初始化的判断。
** how aiRowEst[] should be initialized.  The numbers generated here  //这里面生成的数值是基于在实际索引中发现的典型值。
** are based on typical values found in actual indices.
*/
void sqlite3DefaultRowEst(Index *pIdx){
  tRowcnt *a = pIdx->aiRowEst;
  int i;
  tRowcnt n;
  assert( a!=0 );
  a[0] = pIdx->pTable->nRowEst;
  if( a[0]<10 ) a[0] = 10;
  n = 10;
  for(i=1; i<=pIdx->nColumn; i++){
    a[i] = n;
    if( n>5 ) n--;
  }
  if( pIdx->onError!=OE_None ){
    a[pIdx->nColumn] = 1;
  }
}

/*
** This routine will drop an existing named index.  This routine
** implements the DROP INDEX statement.
** 这个程序被用来调用去删除一个已经存在的索引，实现了DROP INDEX语句。       
*/

void sqlite3DropIndex(Parse *pParse, SrcList *pName, int ifExists){
  Index *pIndex;    //定义了索引指针
  Vdbe *v;
  sqlite3 *db = pParse->db;//指向语法解析上下文处理的数据库
  int iDb;
  assert( pParse->nErr==0 );   /* Never called with prior errors */   //断言语法解析的上下文没有出现错误
  if( db->mallocFailed ){    //如果数据库内存分配失败的话，退出删除索引函数
    goto exit_drop_index;
  }
  assert( pName->nSrc==1 );//断言索引表的数目为1
  if( SQLITE_OK!=sqlite3ReadSchema(pParse) ){  
    goto exit_drop_index;//转到另外一个处理的入口
  }
  pIndex = sqlite3FindIndex(db, pName->a[0].zName, pName->a[0].zDatabase);   //找到相应的索引
  if( pIndex==0 ){//如果索引为空，也就是不存在索引
    if( !ifExists ){//如果索引不存在
      sqlite3ErrorMsg(pParse, "no such index: %S", pName, 0);   //发出错误信息：确实没有这个索引
    }else{//如果索引不为空且确实是存在的
      sqlite3CodeVerifyNamedSchema(pParse, pName->a[0].zDatabase);  //确定索引的模式，也就是确定这个索引属于哪个表的索引
    }
    pParse->checkSchema = 1;
    goto exit_drop_index;
  }
  if( pIndex->autoIndex ){//如果该索引为自动添加的索引，也就是有PRIMARY KEY和UNIQUE子句的自动添加索引
    sqlite3ErrorMsg(pParse, "index associated with UNIQUE "
      "or PRIMARY KEY constraint cannot be dropped", 0);   //发出错误消息：索引被UNIQUE或者PRIMARY KEY限制，这样的索引是不能被删除的
    goto exit_drop_index;  //跳转到另一个处理入口，结束删除索引的动作
  }
  iDb = sqlite3SchemaToIndex(db, pIndex->pSchema);
#ifndef SQLITE_OMIT_AUTHORIZATION
  {
    int code = SQLITE_DROP_INDEX;
    Table *pTab = pIndex->pTable;//定义一个表指针pTab 
    const char *zDb = db->aDb[iDb].zName;//常变量
    const char *zTab = SCHEMA_TABLE(iDb);//常变量
    if( sqlite3AuthCheck(pParse, SQLITE_DELETE, zTab, 0, zDb) ){   //检查是否具有删除索引的授权，如果不具有，立马退出删除索引函数
      goto exit_drop_index;
    }
    if( !OMIT_TEMPDB && iDb ) code = SQLITE_DROP_TEMP_INDEX;
    if( sqlite3AuthCheck(pParse, code, pIndex->zName, pTab->zName, zDb) ){
      goto exit_drop_index;
    }
  }
#endif

  /* Generate code to remove the index and from the master table *///生成代码来从主表中删除索引
  v = sqlite3GetVdbe(pParse);  //需要VDBE引擎的支持
  if( v ){
    sqlite3BeginWriteOperation(pParse, 1, iDb);   //开始写操作
    sqlite3NestedParse(pParse,
       "DELETE FROM %Q.%s WHERE name=%Q AND type='index'",
       db->aDb[iDb].zName, SCHEMA_TABLE(iDb), pIndex->zName
    ); //嵌套的删除
    sqlite3ClearStatTables(pParse, iDb, "idx", pIndex->zName);  //清理一些内存结构
    sqlite3ChangeCookie(pParse, iDb);
    destroyRootPage(pParse, pIndex->tnum, iDb);
    sqlite3VdbeAddOp4(v, OP_DropIndex, iDb, 0, 0, pIndex->zName, 0);
  }

exit_drop_index:
  sqlite3SrcListDelete(db, pName);
}

/*
** pArray is a pointer to an array of objects. Each object in the
** pArray是指向一个对象数组的指针，而且数组中的每个对象szEntry字节的大小。
** array is szEntry bytes in size. This routine uses sqlite3DbRealloc()
** 这个程序使用了 sqlite3DbRealloc()去扩展数组的大小从而使得在最后对于新的对象还有新的空间。
** to extend the array so that there is space for a new object at the end.
**
** When this function is called, *pnEntry contains the current size of
** 当调用这个函数时，*pnEntry包含当前数组的大小（*pnEntry进行分配） 
** the array (in entries - so the allocation is ((*pnEntry) * szEntry) bytes
** in total).
**
** If the realloc() is successful (i.e. if no OOM condition occurs), the//如果realloc函数调用成功（没有OOM条件发生的话），
** space allocated for the new object is zeroed, *pnEntry updated to//分配新对象的空间大小为零，*pnEntry就指向数组的新空间，返回一个指针，指向新的分配空间首地址
** reflect the new size of the array and a pointer to the new allocation
** returned. *pIdx is set to the index of the new array entry in this case.//*pIdx 设置为新数组的索引条目。
**
** Otherwise, if the realloc() fails, *pIdx is set to -1, *pnEntry remains另外，如果调用realloc()函数失败，*pIdx返回-1，保持 指针*pnEntry不变，复制 pArray并且返回指向的指针。
** unchanged and a copy of pArray returned.
*/
void *sqlite3ArrayAllocate(
  sqlite3 *db,      /* Connection to notify of malloc failures   联系到malloc函数错误的通知*/
  void *pArray,     /* Array of objects.  Might be reallocated   对象数组。可能被重新分配*/
  int szEntry,      /* Size of each object in the array  数组中的每个对象的大小*/
  int *pnEntry,     /* Number of objects currently in use  代表目前使用的对象数*/
  int *pIdx         /* Write the index of a new slot here  写索引*/
){
  char *z;
  int n = *pnEntry;   //获取当前正在使用的对象的个数
  if( (n & (n-1))==0 ){   //当前没有对象正在被使用或者当前正在被使用的对象数据是1
    int sz = (n==0) ? 1 : 2*n; //条件表达式，如果n为0的话则sz的值为1，否则sz的值为2*n
    void *pNew = sqlite3DbRealloc(db, pArray, sz*szEntry);  //重新申请两倍的对象数组的空间，当n为0时，申请的空间大小就是现在对象数组的大小
    if( pNew==0 ){
      *pIdx = -1;   //如果空间没有申请成功，那么索引指针为空，返回当前数组的指针
      return pArray;
    }
    pArray = pNew;//如果空间申请成功，返回的是新数组的指针
  }
  z = (char*)pArray;  //将新数组的指针指向z
  memset(&z[n * szEntry], 0, szEntry);
  *pIdx = n;   //指向新的空闲的位置
  ++*pnEntry;  //当前正在使用的对象数目加1 
  return pArray;  //返回新申请的数组
}

/*
** Append a new element to the given IdList.  Create a new IdList if//在给定的IdList添加一个新元素。如果需要的话建立一个新的IdList。
** need be.
**
** A new IdList is returned, or NULL if malloc() fails.//要么一个新的idlist被返回，当malloc()函数失败之后返回空。
*/
IdList *sqlite3IdListAppend(sqlite3 *db, IdList *pList, Token *pToken){
  int i;
  if( pList==0 ){  //说明没有创建Idlists
    pList = sqlite3DbMallocZero(db, sizeof(IdList) ); 
    if( pList==0 ) return 0;  //直接退出函数
  }
  pList->a = sqlite3ArrayAllocate(   //否则重新申请空间
      db,
      pList->a,
      sizeof(pList->a[0]),
      &pList->nId,
      &i
  );
  if( i<0 ){
    sqlite3IdListDelete(db, pList);
    return 0;
  }
  pList->a[i].zName = sqlite3NameFromToken(db, pToken);//从符号表中读取符号，复制给变量
  return pList;
}

/*
** Delete an IdList.//删除一个IdList。
*/
void sqlite3IdListDelete(sqlite3 *db, IdList *pList){
  int i;
  if( pList==0 ) return;  //说明没有Idlists被创建
  for(i=0; i<pList->nId; i++){
    sqlite3DbFree(db, pList->a[i].zName);
  }
  sqlite3DbFree(db, pList->a);
  sqlite3DbFree(db, pList);
}

/*
** Return the index in pList of the identifier named zId.  Return -1//返回 名为zId的标示符中的pList的索引。返回-1值。
** if not found.//如果没有建立。
*/
int sqlite3IdListIndex(IdList *pList, const char *zName){
  int i;
  if( pList==0 ) return -1;
  for(i=0; i<pList->nId; i++){
    if( sqlite3StrICmp(pList->a[i].zName, zName)==0 ) return i;
  }
  return -1;
}

/*
** Expand the space allocated for the given SrcList object by//通过在iStart创建nExtra 开始，扩大所给SrcList object空间。
** creating nExtra new slots beginning at iStart.  iStart is zero based.//iStart零基础。
** New slots are zeroed.//新的slots全置零。
**
** For example, suppose a SrcList initially contains two entries: A,B.//例如,假设一个SrcList最初包含两个条目:A，B。
** To append 3 new entries onto the end, do this://在结束时，添加3个新条目。
**
**    sqlite3SrcListEnlarge(db, pSrclist, 3, 2);//声明sqlite3SrcListEnlarge(db, pSrclist, 3, 2)。
**
** After the call above it would contain:  A, B, nil, nil, nil.//在上面的调用将包含:A、B,零,零,零。
** If the iStart argument had been 1 instead of 2, then the result//如果参数一直1而不是2 ，那么结果将是A，0,0,0，B.
** would have been:  A, nil, nil, nil, B.  To prepend the new slots,//要在前面加上了新的slots，iStart值是0。
** the iStart value would be 0.  The result then would//然后这个结果将为0,0,0，A，B。
** be: nil, nil, nil, A, B.
**
** If a memory allocation fails the SrcList is unchanged.  The//如果内存分配失败的SrcList不变。
** db->mallocFailed flag will be set to true.//将DB- > mallocFailed标志设置为true。
*/
SrcList *sqlite3SrcListEnlarge(
  sqlite3 *db,       /* Database connection to notify of OOM errors *///数据库连接通知OOM错误。
  SrcList *pSrc,     /* The SrcList to be enlarged *///该SrcList待放大。
  int nExtra,        /* Number of new slots to add to pSrc->a[] *///新的slots数量添加到pSrc- > a[ ]中。
  int iStart         /* Index in pSrc->a[] of first new slot *///在第一个slot中的pSrc->a[]设置索引
){
  int i;

  /* Sanity checking on calling parameters *///检查在调用参数时。
  assert( iStart>=0 );
  assert( nExtra>=1 );
  assert( pSrc!=0 );
  assert( iStart<=pSrc->nSrc );

  /* Allocate additional space if needed *///如果需要，分配额外的空间。
  if( pSrc->nSrc+nExtra>pSrc->nAlloc ){
    SrcList *pNew;
    int nAlloc = pSrc->nSrc+nExtra;
    int nGot;
    pNew = sqlite3DbRealloc(db, pSrc,
               sizeof(*pSrc) + (nAlloc-1)*sizeof(pSrc->a[0]) );
    if( pNew==0 ){
      assert( db->mallocFailed );
      return pSrc;
    }
    pSrc = pNew;
    nGot = (sqlite3DbMallocSize(db, pNew) - sizeof(*pSrc))/sizeof(pSrc->a[0])+1;
    pSrc->nAlloc = (u16)nGot;
  }

  /* Move existing slots that come after the newly inserted slots//将新插入slots代替现有的slots。

  ** out of the way */
  for(i=pSrc->nSrc-1; i>=iStart; i--){
    pSrc->a[i+nExtra] = pSrc->a[i];
  }
  pSrc->nSrc += (i16)nExtra;

  /* Zero the newly allocated slots *///将新分配的slots置为0。
  memset(&pSrc->a[iStart], 0, sizeof(pSrc->a[0])*nExtra);
  for(i=iStart; i<iStart+nExtra; i++){
    pSrc->a[i].iCursor = -1;
  }

  /* Return a pointer to the enlarged SrcList *///返回一个指向放大SrcList的指针。
  return pSrc;
}


/*
** Append a new table name to the given SrcList.  Create a new SrcList if//追加新的表名到给定SrcList。如果需要的话创建一个新的SrcList。
** need be.  A new entry is created in the SrcList even if pTable is NULL.//即使PTABLE是NULL，在SrcList创建新条目。
**
** A SrcList is returned, or NULL if there is an OOM error.  The returned//如果有错误OOM，返回一个SrcList，或NULL。
** SrcList might be the same as the SrcList that was input or it might be//SrcList可能是相同的，这是输入SrcList或者它可能是一个新的。
** a new one.  If an OOM error does occurs, then the prior value of pList//如果确实发生了OOM错误， 则自动释放plist中的被输入到本程序前一个值。
** that is input to this routine is automatically freed.
**
** If pDatabase is not null, it means that the table has an optional//如果pDatabase不为空，则意味着该表有一个可选的数据库名称前缀。
** database name prefix.  Like this:  "database.table".  The pDatabase//像这样："database.table"。
** points to the table name and the pTable points to the database name.//所述pDatabase指向表名，Ptable指向数据库名称。
** The SrcList.a[].zName field is filled with the table name which might//所述SrcList.a []。 zName字段填充有表名可能来自PTABLE （如果pDatabase为NULL ）或者从pDatabase（如果pDatabase 是空的话）。
** come from pTable (if pDatabase is NULL) or from pDatabase.  
** SrcList.a[].zDatabase is filled with the database name from pTable,// 从PTABLE 中填充zDatabase数据库名，如果没有指定数据库用NULL。
** or with NULL if no database is specified.
**
** In other words, if call like this://换句话说，如果像这样。
**
**         sqlite3SrcListAppend(D,A,B,0);
**
** Then B is a table name and the database name is unspecified.  If called//那么B是一个表名和数据库名是不确定的。

** like this://如果像这样：
**
**         sqlite3SrcListAppend(D,A,B,C);
**
** Then C is the table name and B is the database name.  If C is defined//那么C是表名和B是数据库名称。
** then so is B.  In other words, we never have a case where://如果C被定义，那么这样是B。换句话说，我们从来没有的情况下：
**
**         sqlite3SrcListAppend(D,A,0,C);
**
** Both pTable and pDatabase are assumed to be quoted.  They are dequoted//无论PTABLE和pDatabase假设被引用。
** before being added to the SrcList.//它们被加入到SrcList之前未用引号引起。
*/
SrcList *sqlite3SrcListAppend(
  sqlite3 *db,        /* Connection to notify of malloc failures *///连接通知失败的malloc。
  SrcList *pList,     /* Append to this SrcList. NULL creates a new SrcList *///追加到此SrcList 。 NULL创建一个新的SrcList。
  Token *pTable,      /* Table to append *///追加表
  Token *pDatabase    /* Database of the table *///表中的数据库
){
  struct SrcList_item *pItem;
  assert( pDatabase==0 || pTable!=0 );  /* Cannot have C without B *///没有哦B时不能有C
  if( pList==0 ){
    pList = sqlite3DbMallocZero(db, sizeof(SrcList) );
    if( pList==0 ) return 0;
    pList->nAlloc = 1;
  }
  pList = sqlite3SrcListEnlarge(db, pList, 1, pList->nSrc);
  if( db->mallocFailed ){
    sqlite3SrcListDelete(db, pList);
    return 0;
  }
  pItem = &pList->a[pList->nSrc-1];
  if( pDatabase && pDatabase->z==0 ){
    pDatabase = 0;
  }
  if( pDatabase ){
    Token *pTemp = pDatabase;
    pDatabase = pTable;
    pTable = pTemp;
  }
  pItem->zName = sqlite3NameFromToken(db, pTable);
  pItem->zDatabase = sqlite3NameFromToken(db, pDatabase);
  return pList;
}

/*
** Assign VdbeCursor index numbers to all tables in a SrcList//在SrcList中所有表分配VdbeCursor索引号。
*/
void sqlite3SrcListAssignCursors(Parse *pParse, SrcList *pList){
  int i;
  struct SrcList_item *pItem;
  assert(pList || pParse->db->mallocFailed );
  if( pList ){
    for(i=0, pItem=pList->a; i<pList->nSrc; i++, pItem++){
      if( pItem->iCursor>=0 ) break;
      pItem->iCursor = pParse->nTab++;
      if( pItem->pSelect ){
        sqlite3SrcListAssignCursors(pParse, pItem->pSelect->pSrc);
      }
    }
  }
}

/*
** Delete an entire SrcList including all its substructure.//删除整个SrcList包括其所有子结构。
*/
void sqlite3SrcListDelete(sqlite3 *db, SrcList *pList){
  int i;
  struct SrcList_item *pItem;
  if( pList==0 ) return;
  for(pItem=pList->a, i=0; i<pList->nSrc; i++, pItem++){
    sqlite3DbFree(db, pItem->zDatabase);
    sqlite3DbFree(db, pItem->zName);
    sqlite3DbFree(db, pItem->zAlias);
    sqlite3DbFree(db, pItem->zIndex);
    sqlite3DeleteTable(db, pItem->pTab);
    sqlite3SelectDelete(db, pItem->pSelect);
    sqlite3ExprDelete(db, pItem->pOn);
    sqlite3IdListDelete(db, pItem->pUsing);
  }
  sqlite3DbFree(db, pList);
}

/*
** This routine is called by the parser to add a new term to the//这个程序被解析器调用，调用的 目的是增加一个新的术语到FROM子句的末尾。
** end of a growing FROM clause.  The "p" parameter is the part of//在“P”参数是FROM子句的一部分并且这个语句已经被成功的构建。
** the FROM clause that has already been constructed.  "p" is NULL//如果这是在FROM子句中的第一项“P”是NULL 。
** if this is the first term of the FROM clause.  pTable and pDatabase//从FROM子句中，PTable和pDatabase是表和数据库的命名。
** are the name of the table and database named in the FROM clause term.
** pDatabase is NULL if the database name qualifier is missing - the//通常情况下，如果数据库名称限定丢失，pDatabase是NULL 。
** usual case.  If the term has a alias, then pAlias points to the//如果术语有一个别名，然后pAlias​​指向别名符号。
** alias token.  If the term is a subquery, then pSubquery is the//如果术语是一个子查询，那么pSubquery是SELECT语句的子查询编码。
** SELECT statement that the subquery encodes.  The pTable and//对于该子查询，PTABLE和pDatabase参数是NULL。
** pDatabase parameters are NULL for subqueries.  The pOn and pUsing//在PON和参数Pusing参数是ON子句和USING子句的上下文。
** parameters are the content of the ON and USING clauses.
** Return a new SrcList which encodes is the FROM with the new/从与新一届中增加返回一个新的SrcList编码。
** term added.
*/
SrcList *sqlite3SrcListAppendFromTerm(
	Parse *pParse,          /* Parsing context *///语法分析的上下文
  SrcList *p,             /* The left part of the FROM clause already seen *///FROM子句的左侧部分已经看过
  Token *pTable,          /* Name of the table to add to the FROM clause *///增加到FROM子句中的表的名字
  Token *pDatabase,       /* Name of the database containing pTable *///含PTable表的数据库名字
  Token *pAlias,          /* The right-hand side of the AS subexpression *///在AS子表达式的右侧
  Select *pSubquery,      /* A subquery used in place of a table name *///代替表名称中使用子查询
  Expr *pOn,              /* The ON clause of a join *///join中的ON子句
  IdList *pUsing          /* The USING clause of a join *///join中的USING子句
){
  struct SrcList_item *pItem;
  sqlite3 *db = pParse->db;
  if( !p && (pOn || pUsing) ){
    sqlite3ErrorMsg(pParse, "a JOIN clause is required before %s", 
      (pOn ? "ON" : "USING")
    );
    goto append_from_error;
  }
  p = sqlite3SrcListAppend(db, p, pTable, pDatabase);
  if( p==0 || NEVER(p->nSrc==0) ){
    goto append_from_error;
  }
  pItem = &p->a[p->nSrc-1];
  assert( pAlias!=0 );
  if( pAlias->n ){
    pItem->zAlias = sqlite3NameFromToken(db, pAlias);
  }
  pItem->pSelect = pSubquery;
  pItem->pOn = pOn;
  pItem->pUsing = pUsing;
  return p;

 append_from_error:
  assert( p==0 );
  sqlite3ExprDelete(db, pOn);
  sqlite3IdListDelete(db, pUsing);
  sqlite3SelectDelete(db, pSubquery);
  return 0;
}

/*
** Add an INDEXED BY or NOT INDEXED clause to the most recently added 
** element of the source-list passed as the second argument.
** 增加INDEXED BY子句或者是NOT INDEXED子句到最近增加的source-list中，作为第二个参数
*/
void sqlite3SrcListIndexedBy(Parse *pParse, SrcList *p, Token *pIndexedBy){
  assert( pIndexedBy!=0 );
  if( p && ALWAYS(p->nSrc>0) ){
    struct SrcList_item *pItem = &p->a[p->nSrc-1];
    assert( pItem->notIndexed==0 && pItem->zIndex==0 );
    if( pIndexedBy->n==1 && !pIndexedBy->z ){
      /* A "NOT INDEXED" clause was supplied. See parse.y //NOT INDEXED子句被提供。在语法解析器结构中查看详细的细节。
      ** construct "indexed_opt" for details. */
      pItem->notIndexed = 1;
    }else{
      pItem->zIndex = sqlite3NameFromToken(pParse->db, pIndexedBy);
    }
  }
}

/*
** When building up a FROM clause in the parser, the join operator//当在分析器中建立FROM子句，联接（join）运算符被初始化附着到左操作数。
** is initially attached to the left operand.  But the code generator//但代码生成期望join操作符在初始化时被附着到右操作数中。
** expects the join operator to be on the right operand.  This routine//为整个FROMclause这个程序从左至右切换所有join操作符。
** Shifts all join operators from left to right for an entire FROM
** clause.
**
** Example: Suppose the join is like this://例如：假设连接是这样的
**
**           A natural cross join B//自然交叉连接B
**
** The operator is "natural cross join".  The A and B operands are stored//操作符是“自然交叉联接”。
** in p->a[0] and p->a[1], respectively.  The parser initially stores the//A和B的操作数被各自的存储在p->一个[0]和对 - >一个[1]中 。解析器用A初始化这个操作符
** operator with A.  This routine shifts that operator over to B.//这个程序切换的操作交给B。
*/
void sqlite3SrcListShiftJoinType(SrcList *p){
  if( p ){
    int i;
    assert( p->a || p->nSrc==0 );
    for(i=p->nSrc-1; i>0; i--){
      p->a[i].jointype = p->a[i-1].jointype;
    }
    p->a[0].jointype = 0;
  }
}

/*
** Begin a transaction//开始事务
*/
void sqlite3BeginTransaction(Parse *pParse, int type){
  sqlite3 *db;
  Vdbe *v; 
  int i;

  assert( pParse!=0 );   //断言语法解析上下文正在运行
  db = pParse->db;   //指向语法分析上下文正在操作的数据库
  assert( db!=0 );   //断言数据库不为空，也就是说语法解析上下文正在操作这个数据库
/*  if( db->aDb[0].pBt==0 ) return; *///如果（ DB- > ADB [ 0 ] .pBt == 0 ） 返回
  if( sqlite3AuthCheck(pParse, SQLITE_TRANSACTION, "BEGIN", 0, 0) ){   //检查事务授权是否为开始（BEGIN）
    return;
  }
  v = sqlite3GetVdbe(pParse);
  if( !v ) return;   //获得VDBE引擎的支持
  if( type!=TK_DEFERRED ){
    for(i=0; i<db->nDb; i++){
      sqlite3VdbeAddOp2(v, OP_Transaction, i, (type==TK_EXCLUSIVE)+1);   //给VDBE引擎增加处理事务的操作OP_Transaction
      sqlite3VdbeUsesBtree(v, i);  //VDBE操作时需要使用BTree
    }
  }
  sqlite3VdbeAddOp2(v, OP_AutoCommit, 0, 0);  //给VDBE增加自动提交事务的操作
}

/*
** Commit a transaction//提交事务
*/
void sqlite3CommitTransaction(Parse *pParse){
  Vdbe *v;

  assert( pParse!=0 );  
  assert( pParse->db!=0 );//断言语法解析上下文正在对一个数据库进行操作
  if( sqlite3AuthCheck(pParse, SQLITE_TRANSACTION, "COMMIT", 0, 0) ){   //检查事务处理是否具有COMMIT授权
    return;
  }
  v = sqlite3GetVdbe(pParse);   //获得VDBE引擎的支持
  if( v ){
    sqlite3VdbeAddOp2(v, OP_AutoCommit, 1, 0);  //为VDBE引擎增加自动提交的操作
  }
}

/*
** Rollback a transaction//回滚事务
*/
void sqlite3RollbackTransaction(Parse *pParse){
  Vdbe *v;

  assert( pParse!=0 );
  assert( pParse->db!=0 );  //断言语法解析上下文正在处理数据库
  if( sqlite3AuthCheck(pParse, SQLITE_TRANSACTION, "ROLLBACK", 0, 0) ){  //检查是否具有回滚事务的权限
    return;
  }
  v = sqlite3GetVdbe(pParse);   //获得VEBE引擎的支持
  if( v ){
    sqlite3VdbeAddOp2(v, OP_AutoCommit, 1, 1);   //增加一个自动提交的操作OP_AutoCommit
  }
}

/*
** This function is called by the parser when it parses a command to create,//这个函数被调用时，由它解析一个命令来创建解析器，释放或回滚SQL保存点。
** release or rollback an SQL savepoint. 
*/
void sqlite3Savepoint(Parse *pParse, int op, Token *pName){
  char *zName = sqlite3NameFromToken(pParse->db, pName);   //从符号表中取出语法解析上下文处理的数据库
  if( zName ){
    Vdbe *v = sqlite3GetVdbe(pParse);  //如果这个数据库是存在的，则首先应该获得VDBE引擎的支持
#ifndef SQLITE_OMIT_AUTHORIZATION
    static const char * const az[] = { "BEGIN", "RELEASE", "ROLLBACK" };  //断言已经具有开始，释放或者回滚的权限
    assert( !SAVEPOINT_BEGIN && SAVEPOINT_RELEASE==1 && SAVEPOINT_ROLLBACK==2 );
#endif
    if( !v || sqlite3AuthCheck(pParse, SQLITE_SAVEPOINT, az[op], zName, 0) ){   //授权上面提到的三个权限
      sqlite3DbFree(pParse->db, zName);  //释放相应的数据库
      return;
    }
    sqlite3VdbeAddOp4(v, OP_Savepoint, op, 0, 0, zName, P4_DYNAMIC);  //增加一个保存点操作
  }
}

/*
** Make sure the TEMP database is open and available for use.  Return//确保TEMP数据库是开放的，可以使用。
** the number of errors.  Leave any error messages in the pParse structure.//返回错误的数量。丢掉留在pParse结构中的任何错误信息。
*/
int sqlite3OpenTempDatabase(Parse *pParse){
  sqlite3 *db = pParse->db;
  if( db->aDb[1].pBt==0 && !pParse->explain ){
    int rc;
    Btree *pBt;
    static const int flags = 
          SQLITE_OPEN_READWRITE |
          SQLITE_OPEN_CREATE |
          SQLITE_OPEN_EXCLUSIVE |
          SQLITE_OPEN_DELETEONCLOSE |
          SQLITE_OPEN_TEMP_DB;

    rc = sqlite3BtreeOpen(db->pVfs, 0, db, &pBt, 0, flags);
    if( rc!=SQLITE_OK ){
      sqlite3ErrorMsg(pParse, "unable to open a temporary database "
        "file for storing temporary tables");
      pParse->rc = rc;
      return 1;
    }
    db->aDb[1].pBt = pBt;
    assert( db->aDb[1].pSchema );
    if( SQLITE_NOMEM==sqlite3BtreeSetPageSize(pBt, db->nextPagesize, -1, 0) ){
      db->mallocFailed = 1;
      return 1;
    }
  }
  return 0;
}

/*
** Generate VDBE code that will verify the schema cookie and start//生成VDBE代码，这些代码能验证模式的cookie并且对所有命名数据库文件开始一个读事务。
** a read-transaction for all named database files.
**
** It is important that all schema cookies be verified and all//所有的模式cookies被验证，所有的读事务应该开始，这两件事情应该发生在所有其他的VDBE程序之前。
** read transactions be started before anything else happens in
** the VDBE program.  But this routine can be called after much other
** code has been generated.  So here is what we do://但是这个能够被许多其他的代码生成之后再去调用.这是我们所做的 :
**
** The first time this routine is called, we code an OP_Goto that//第一次调用这个程序,我们编码一个操作OP_Goto在程序结束时将跳转到一个子程序。
** will jump to a subroutine at the end of the program.  Then we
** record every database that needs its schema verified in the//然后我们记录每个数据库，这个数据库在pParse - > cookieMask字段中需要它的模式验证。
** pParse->cookieMask field.  Later, after all other code has been//然后，所有其他代码被生成,  做了cookie验证并启动事务的子程序将被编码并且这个OP_Goto P2值将指向子程序。
** generated, the subroutine that does the cookie verifications and
** starts the transactions will be coded and the OP_Goto P2 value
** will be made to point to that subroutine.  The generation of the//cookie验证子程序代码的生成发生在sqlite3FinishCoding()中。
** cookie verification subroutine code happens in sqlite3FinishCoding().
**
** If iDb<0 then code the OP_Goto only - don't set flag to verify the//如果iDb<0 ，然后仅仅给OP_Goto编码 - 不设置标志去验证任何数据库的模式 这可以早点在我们知道之前，如果任何数据库的表都将被用的话，则在编码中用作OP_Goto位置
** schema on any databases.  This can be used to position the OP_Goto
** early in the code, before we know if any database tables will be used.
*/
void sqlite3CodeVerifySchema(Parse *pParse, int iDb){
  Parse *pToplevel = sqlite3ParseToplevel(pParse);

  if( pToplevel->cookieGoto==0 ){
    Vdbe *v = sqlite3GetVdbe(pToplevel);
    if( v==0 ) return;  /* This only happens if there was a prior error *///这仅仅发生在如果之前有一个错误的时候
    pToplevel->cookieGoto = sqlite3VdbeAddOp2(v, OP_Goto, 0, 0)+1;   //跳转到这个程序末尾的子程序中
  }
  if( iDb>=0 ){
    sqlite3 *db = pToplevel->db;
    yDbMask mask;

    assert( iDb<db->nDb );
    assert( db->aDb[iDb].pBt!=0 || iDb==1 );
    assert( iDb<SQLITE_MAX_ATTACHED+2 );
    assert( sqlite3SchemaMutexHeld(db, iDb, 0) );
    mask = ((yDbMask)1)<<iDb;
    if( (pToplevel->cookieMask & mask)==0 ){
      pToplevel->cookieMask |= mask;
      pToplevel->cookieValue[iDb] = db->aDb[iDb].pSchema->schema_cookie;
      if( !OMIT_TEMPDB && iDb==1 ){
        sqlite3OpenTempDatabase(pToplevel);
      }
    }
  }
}

/*
** If argument zDb is NULL, then call sqlite3CodeVerifySchema() for each //如果参数zDb为空,然后为每个连接数据库调用sqlite3CodeVerifySchema()。否则,只让名叫zDb的数据库调用它。
** attached database. Otherwise, invoke it for the database named zDb only.
*/
void sqlite3CodeVerifyNamedSchema(Parse *pParse, const char *zDb){
  sqlite3 *db = pParse->db;
  int i;
  for(i=0; i<db->nDb; i++){
    Db *pDb = &db->aDb[i];
    if( pDb->pBt && (!zDb || 0==sqlite3StrICmp(zDb, pDb->zName)) ){
      sqlite3CodeVerifySchema(pParse, i);
    }
  }
}

void sqlite3BeginWriteOperation(Parse *pParse, int setStatement, int iDb){
  Parse *pToplevel = sqlite3ParseToplevel(pParse);
  sqlite3CodeVerifySchema(pParse, iDb);
  pToplevel->writeMask |= ((yDbMask)1)<<iDb;
  pToplevel->isMultiWrite |= setStatement;
}

/*
** Indicate that the statement currently under construction might write
** more than one entry (example: deleting one row then inserting another,
** inserting multiple rows in a table, or inserting a row and index entries.)
** 表明,目前正在构建的语句可以写多个条目(例如:删除一行然后插入另一个表中插入多行,或插入一行和索引条目)。
** If an abort occurs after some of these writes have completed, then it will
** be necessary to undo the completed writes.
** 如果在这些写完成后发生中止,那么它将需要撤消已经完成的写。
*/
void sqlite3MultiWrite(Parse *pParse){
  Parse *pToplevel = sqlite3ParseToplevel(pParse);
  pToplevel->isMultiWrite = 1;
}

/*
** The code generator calls this routine if is discovers that it is
** possible to abort a statement prior to completion.  In order to
** perform this abort without corrupting the database, we need to make
** sure that the statement is protected by a statement transaction.
** 当这个程序被发现可能在完成一个语句之前被终止，那么代码生成器调用这个程序。为了去执行这个终止程序但是不弄脏数据库，
** 因此我们需要去确保这个语句被事务处理语句保护。
** Technically, we only need to set the mayAbort flag if the
** isMultiWrite flag was previously set.  
** 从技术上讲，如果在之前多次写操作标志被设置，我们仅仅需要去重置一个mayAbort标志。
** There is a time dependency such that the abort must occur after the multiwrite. 
** 有一些依赖关系是存在的以至于终止操作必须发生在多次写操作之后。
** This makes some statements involving the REPLACE conflict resolution algorithm
** go a little faster. 
** 这使得一些语句调用REPLACE冲突解决算法。
** But taking advantage of this time dependency makes it more difficult to prove that the code is correct (in
** particular, it prevents us from writing an effective
** implementation of sqlite3AssertMayAbort()) and so we have chosen
** to take the safe route and skip the optimization.
** 但是利用这种依赖使得它更加的困难去证明这个代码的正确性（特别地，它阻止我们去写一个sqlite3AssertMayAbort()有效的实现），并且，我们选择安全的程序然后跳过一些优化
*/

void sqlite3MayAbort(Parse *pParse){
  Parse *pToplevel = sqlite3ParseToplevel(pParse);
  pToplevel->mayAbort = 1;
}

/*
** Code an OP_Halt that causes the vdbe to return an SQLITE_CONSTRAINT error.
** 编码OP_Halt导致的VDBE返回一个SQLITE_CONSTRAINT错误。
** The onError parameter determines which (if any) of the statement
** and/or current transaction is rolled back.
** 该onError的参数决定的声明和/或当前事务的哪个（如果有的话）被回滚。
*/
void sqlite3HaltConstraint(Parse *pParse, int onError, char *p4, int p4type){
  Vdbe *v = sqlite3GetVdbe(pParse);
  if( onError==OE_Abort ){
    sqlite3MayAbort(pParse);
  }
  sqlite3VdbeAddOp4(v, OP_Halt, SQLITE_CONSTRAINT, onError, 0, p4, p4type);
}

/*
** Check to see if pIndex uses the collating sequence pColl.  Return
** 检查是否pIndex使用的排序序列pColl 。
** true if it does and false if it does not.
** 如果它没有，返回如果它的真假。
*/
#ifndef SQLITE_OMIT_REINDEX
static int collationMatch(const char *zColl, Index *pIndex){
  int i;
  assert( zColl!=0 );
  for(i=0; i<pIndex->nColumn; i++){
    const char *z = pIndex->azColl[i];
    assert( z!=0 );
    if( 0==sqlite3StrICmp(z, zColl) ){
      return 1;
    }
  }
  return 0;
}
#endif

/*
** Recompute all indices of pTab that use the collating sequence pColl.
** 重新计算pTab中使用的排序序列的所有指标pColl。
** If pColl==0 then recompute all indices of pTab.
** 如果pColl == 0，则重新计算pTab的各项指标。
*/
#ifndef SQLITE_OMIT_REINDEX
static void reindexTable(Parse *pParse, Table *pTab, char const *zColl){
  Index *pIndex;              /* An index associated with pTab  与pTab所关联的索引*/

  for(pIndex=pTab->pIndex; pIndex; pIndex=pIndex->pNext){
    if( zColl==0 || collationMatch(zColl, pIndex) ){
      int iDb = sqlite3SchemaToIndex(pParse->db, pTab->pSchema);
      sqlite3BeginWriteOperation(pParse, 0, iDb);
      sqlite3RefillIndex(pParse, pIndex, -1);
    }
  }
}
#endif

/*
** Recompute all indices of all tables in all databases where the
** 重新计算所有在那里的指数使用的排序序列pColl数据库中的所有表的所有指标。
** indices use the collating sequence pColl.  If pColl==0 then recompute
** 如果pColl == 0，则重新计算所有指数。
** all indices everywhere.
*/
#ifndef SQLITE_OMIT_REINDEX
static void reindexDatabases(Parse *pParse, char const *zColl){
  Db *pDb;                    /* A single database *///单个数据库
  int iDb;                    /* The database index number *///该数据库索引号
  sqlite3 *db = pParse->db;   /* The database connection *///数据库连接
  HashElem *k;                /* For looping over tables in pDb *///对于PDB遍历表
  Table *pTab;                /* A table in the database *///在数据库中的表

  assert( sqlite3BtreeHoldsAllMutexes(db) );  /* Needed for schema access *///需要接入模式
  for(iDb=0, pDb=db->aDb; iDb<db->nDb; iDb++, pDb++){
    assert( pDb!=0 );
    for(k=sqliteHashFirst(&pDb->pSchema->tblHash);  k; k=sqliteHashNext(k)){
      pTab = (Table*)sqliteHashData(k);
      reindexTable(pParse, pTab, zColl);
    }
  }
}
#endif

/*
** Generate code for the REINDEX command.//生成的REINDEX命令代码。
**
**        REINDEX                            -- 1//REINDEX - 1
**        REINDEX  <collation>               -- 2//REINDEX <整理>--2
**        REINDEX  ?<database>.?<tablename>  -- 3//REINDEX <数据库> <表名> - 3
**        REINDEX  ?<database>.?<indexname>  -- 4//REINDEX <数据库> < INDEXNAME > - 4
**
** Form 1 causes all indices in all attached databases to be rebuilt.//表1导致所有附加的数据库中的所有指标进行重建。
** Form 2 rebuilds all indices in all databases that use the named//表格2重建在所有使用命名整理功能，数据库中的所有指标。
** collating function.  Forms 3 and 4 rebuild the named index or all//表格3和4重建命名的索引或与指定表相关联的所有索引。
** indices associated with the named table.
*/
#ifndef SQLITE_OMIT_REINDEX
void sqlite3Reindex(Parse *pParse, Token *pName1, Token *pName2){
  CollSeq *pColl;             /* Collating sequence to be reindexed, or NULL *///整理序列重建索引，或NULL。
  char *z;                    /* Name of a table or index *///表或索引的名称
  const char *zDb;            /* Name of the database *///数据库的名称
  Table *pTab;                /* A table in the database *///在数据库中的表
  Index *pIndex;              /* An index associated with pTab *///与PTAB相关联的索引
  int iDb;                    /* The database index number *///该数据库索引号
  sqlite3 *db = pParse->db;   /* The database connection *///数据库连接
  Token *pObjName;            /* Name of the table or index to be reindexed *///要重建索引的表或索引的名称

  /* Read the database schema. If an error occurs, leave an error message//读取数据库架构。如果出现错误，留下一个错误消息和代码在pParse并返回NULL。
  ** and code in pParse and return NULL. */
  if( SQLITE_OK!=sqlite3ReadSchema(pParse) ){
    return;
  }

  if( pName1==0 ){
    reindexDatabases(pParse, 0);
    return;
  }else if( NEVER(pName2==0) || pName2->z==0 ){
    char *zColl;
    assert( pName1->z );
    zColl = sqlite3NameFromToken(pParse->db, pName1);
    if( !zColl ) return;
    pColl = sqlite3FindCollSeq(db, ENC(db), zColl, 0);
    if( pColl ){
      reindexDatabases(pParse, zColl);
      sqlite3DbFree(db, zColl);
      return;
    }
    sqlite3DbFree(db, zColl);
  }
  iDb = sqlite3TwoPartName(pParse, pName1, pName2, &pObjName);
  if( iDb<0 ) return;
  z = sqlite3NameFromToken(db, pObjName);
  if( z==0 ) return;
  zDb = db->aDb[iDb].zName;
  pTab = sqlite3FindTable(db, z, zDb);
  if( pTab ){
    reindexTable(pParse, pTab, 0);
    sqlite3DbFree(db, z);
    return;
  }
  pIndex = sqlite3FindIndex(db, z, zDb);
  sqlite3DbFree(db, z);
  if( pIndex ){
    sqlite3BeginWriteOperation(pParse, 0, iDb);
    sqlite3RefillIndex(pParse, pIndex, -1);
    return;
  }
  sqlite3ErrorMsg(pParse, "unable to identify the object to be reindexed");
}
#endif

/*
** Return a dynamicly allocated KeyInfo structure that can be used//返回一个动态地分配的密钥信息的结构，可以使用与OP_OpenRead或OP_OpenWrite访问数据库索引PIDX 。
** with OP_OpenRead or OP_OpenWrite to access database index pIdx.
**
** If successful, a pointer to the new structure is returned. In this case//如果成功，则返回一个指向新的结构。
** the caller is responsible for calling sqlite3DbFree(db, ) on the returned //在这种情况下，调用者负责调用sqlite3DbFree 上返回的指针。
** pointer. If an error occurs (out of memory or missing collation //如果出现错误（内存不足或缺失的排列顺序） ，则返回NULL和pParse的状态更新，以反映错误。
** sequence), NULL is returned and the state of pParse updated to reflect
** the error.
*/
KeyInfo *sqlite3IndexKeyinfo(Parse *pParse, Index *pIdx){
  int i;
  int nCol = pIdx->nColumn;
  int nBytes = sizeof(KeyInfo) + (nCol-1)*sizeof(CollSeq*) + nCol;
  sqlite3 *db = pParse->db;
  KeyInfo *pKey = (KeyInfo *)sqlite3DbMallocZero(db, nBytes);

  if( pKey ){
    pKey->db = pParse->db;
    pKey->aSortOrder = (u8 *)&(pKey->aColl[nCol]);
    assert( &pKey->aSortOrder[nCol]==&(((u8 *)pKey)[nBytes]) );
    for(i=0; i<nCol; i++){
      char *zColl = pIdx->azColl[i];
      assert( zColl );
      pKey->aColl[i] = sqlite3LocateCollSeq(pParse, zColl);
      pKey->aSortOrder[i] = pIdx->aSortOrder[i];
    }
    pKey->nField = (u16)nCol;
  }

  if( pParse->nErr ){
    sqlite3DbFree(db, pKey);
    pKey = 0;
  }
  return pKey；
}
