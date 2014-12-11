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
** to handle INSERT statements in SQLite.
*/
#include "sqliteInt.h"

/*
** Generate code that will open a table for reading.
*/
void sqlite3OpenTable(
  Parse *p,       /* Generate code into this VDBE */
  int iCur,       /* The cursor number of the table */
  int iDb,        /* The database index in sqlite3.aDb[] */
  Table *pTab,    /* The table to be opened */
  int opcode      /* OP_OpenRead or OP_OpenWrite */
){
  Vdbe *v;
  if( IsVirtual(pTab) ) return;
  v = sqlite3GetVdbe(p);
  assert( opcode==OP_OpenWrite || opcode==OP_OpenRead );
  sqlite3TableLock(p, iDb, pTab->tnum, (opcode==OP_OpenWrite)?1:0, pTab->zName);
  sqlite3VdbeAddOp3(v, opcode, iCur, pTab->tnum, iDb);
  sqlite3VdbeChangeP4(v, -1, SQLITE_INT_TO_PTR(pTab->nCol), P4_INT32);
  VdbeComment((v, "%s", pTab->zName));
}

/*
** Return a pointer to the column affinity string associated with index
** pIdx. A column affinity string has one character for each column in 
** the table, according to the affinity of the column:
**
**  Character      Column affinity
**  ------------------------------
**  'a'            TEXT
**  'b'            NONE
**  'c'            NUMERIC
**  'd'            INTEGER
**  'e'            REAL
**
** An extra 'd' is appended to the end of the string to cover the
** rowid that appears as the last column in every index.
**
** Memory for the buffer containing the column index affinity string
** is managed along with the rest of the Index structure. It will be
** released when sqlite3DeleteIndex() is called.
*/
const char *sqlite3IndexAffinityStr(Vdbe *v, Index *pIdx){
  if( !pIdx->zColAff ){
    /* The first time a column affinity string for a particular index is
    ** required, it is allocated and populated here. It is then stored as
    ** a member of the Index structure for subsequent use.
    **
    ** The column affinity string will eventually be deleted by
    ** sqliteDeleteIndex() when the Index structure itself is cleaned
    ** up.
    */
    int n;
    Table *pTab = pIdx->pTable;
    sqlite3 *db = sqlite3VdbeDb(v);
    pIdx->zColAff = (char *)sqlite3DbMallocRaw(0, pIdx->nColumn+2);
    if( !pIdx->zColAff ){
      db->mallocFailed = 1;
      return 0;
    }
    for(n=0; n<pIdx->nColumn; n++){
      pIdx->zColAff[n] = pTab->aCol[pIdx->aiColumn[n]].affinity;
    }
    pIdx->zColAff[n++] = SQLITE_AFF_INTEGER;
    pIdx->zColAff[n] = 0;
  }
 
  return pIdx->zColAff;
}

/*
** Set P4 of the most recently inserted opcode to a column affinity
** string for table pTab. A column affinity string has one character
** for each column indexed by the index, according to the affinity of the
** column:
**
**  Character      Column affinity
**  ------------------------------
**  'a'            TEXT
**  'b'            NONE
**  'c'            NUMERIC
**  'd'            INTEGER
**  'e'            REAL
*/
void sqlite3TableAffinityStr(Vdbe *v, Table *pTab){
  /* The first time a column affinity string for a particular table
  ** is required, it is allocated and populated here. It is then 
  ** stored as a member of the Table structure for subsequent use.
  **
  ** The column affinity string will eventually be deleted by
  ** sqlite3DeleteTable() when the Table structure itself is cleaned up.
  */
  if( !pTab->zColAff ){
    char *zColAff;
    int i;
    sqlite3 *db = sqlite3VdbeDb(v);

    zColAff = (char *)sqlite3DbMallocRaw(0, pTab->nCol+1);
    if( !zColAff ){
      db->mallocFailed = 1;
      return;
    }

    for(i=0; i<pTab->nCol; i++){
      zColAff[i] = pTab->aCol[i].affinity;
    }
    zColAff[pTab->nCol] = '\0';

    pTab->zColAff = zColAff;
  }

  sqlite3VdbeChangeP4(v, -1, pTab->zColAff, P4_TRANSIENT);
}

/*
** Return non-zero if the table pTab in database iDb or any of its indices
** have been opened at any point in the VDBE program beginning at location
** iStartAddr throught the end of the program.  This is used to see if 
** a statement of the form  "INSERT INTO <iDb, pTab> SELECT ..." can 
** run without using temporary table for the results of the SELECT. 
*/
static int readsTable(Parse *p, int iStartAddr, int iDb, Table *pTab){
  Vdbe *v = sqlite3GetVdbe(p);
  int i;
  int iEnd = sqlite3VdbeCurrentAddr(v);
#ifndef SQLITE_OMIT_VIRTUALTABLE
  VTable *pVTab = IsVirtual(pTab) ? sqlite3GetVTable(p->db, pTab) : 0;
#endif

  for(i=iStartAddr; i<iEnd; i++){
    VdbeOp *pOp = sqlite3VdbeGetOp(v, i);
    assert( pOp!=0 );
    if( pOp->opcode==OP_OpenRead && pOp->p3==iDb ){
      Index *pIndex;
      int tnum = pOp->p2;
      if( tnum==pTab->tnum ){
        return 1;
      }
      for(pIndex=pTab->pIndex; pIndex; pIndex=pIndex->pNext){
        if( tnum==pIndex->tnum ){
          return 1;
        }
      }
    }
#ifndef SQLITE_OMIT_VIRTUALTABLE
    if( pOp->opcode==OP_VOpen && pOp->p4.pVtab==pVTab ){
      assert( pOp->p4.pVtab!=0 );
      assert( pOp->p4type==P4_VTAB );
      return 1;
    }
#endif
  }
  return 0;
}

#ifndef SQLITE_OMIT_AUTOINCREMENT
/*
** Locate or create an AutoincInfo structure associated with table pTab
** which is in database iDb.  Return the register number for the register
** that holds the maximum rowid.
**
** There is at most one AutoincInfo structure per table even if the
** same table is autoincremented multiple times due to inserts within
** triggers.  A new AutoincInfo structure is created if this is the
** first use of table pTab.  On 2nd and subsequent uses, the original
** AutoincInfo structure is used.
**
** Three memory locations are allocated:
**
**   (1)  Register to hold the name of the pTab table.
**   (2)  Register to hold the maximum ROWID of pTab.
**   (3)  Register to hold the rowid in sqlite_sequence of pTab
**
** The 2nd register is the one that is returned.  That is all the
** insert routine needs to know about.
*/
static int autoIncBegin(
  Parse *pParse,      /* Parsing context */
  int iDb,            /* Index of the database holding pTab */
  Table *pTab         /* The table we are writing to */
){
  int memId = 0;      /* Register holding maximum rowid */
  if( pTab->tabFlags & TF_Autoincrement ){
    Parse *pToplevel = sqlite3ParseToplevel(pParse);
    AutoincInfo *pInfo;

    pInfo = pToplevel->pAinc;
    while( pInfo && pInfo->pTab!=pTab ){ pInfo = pInfo->pNext; }
    if( pInfo==0 ){
      pInfo = sqlite3DbMallocRaw(pParse->db, sizeof(*pInfo));
      if( pInfo==0 ) return 0;
      pInfo->pNext = pToplevel->pAinc;
      pToplevel->pAinc = pInfo;
      pInfo->pTab = pTab;
      pInfo->iDb = iDb;
      pToplevel->nMem++;                  /* Register to hold name of table */
      pInfo->regCtr = ++pToplevel->nMem;  /* Max rowid register */
      pToplevel->nMem++;                  /* Rowid in sqlite_sequence */
    }
    memId = pInfo->regCtr;
  }
  return memId;
}

/*
** This routine generates code that will initialize all of the
** register used by the autoincrement tracker.  
*/
void sqlite3AutoincrementBegin(Parse *pParse){
  AutoincInfo *p;            /* Information about an AUTOINCREMENT */
  sqlite3 *db = pParse->db;  /* The database connection */
  Db *pDb;                   /* Database only autoinc table */
  int memId;                 /* Register holding max rowid */
  int addr;                  /* A VDBE address */
  Vdbe *v = pParse->pVdbe;   /* VDBE under construction */

  /* This routine is never called during trigger-generation.  It is
  ** only called from the top-level */
  assert( pParse->pTriggerTab==0 );
  assert( pParse==sqlite3ParseToplevel(pParse) );

  assert( v );   /* We failed long ago if this is not so */
  for(p = pParse->pAinc; p; p = p->pNext){
    pDb = &db->aDb[p->iDb];
    memId = p->regCtr;
    assert( sqlite3SchemaMutexHeld(db, 0, pDb->pSchema) );
    sqlite3OpenTable(pParse, 0, p->iDb, pDb->pSchema->pSeqTab, OP_OpenRead);
    sqlite3VdbeAddOp3(v, OP_Null, 0, memId, memId+1);
    addr = sqlite3VdbeCurrentAddr(v);
    sqlite3VdbeAddOp4(v, OP_String8, 0, memId-1, 0, p->pTab->zName, 0);
    sqlite3VdbeAddOp2(v, OP_Rewind, 0, addr+9);
    sqlite3VdbeAddOp3(v, OP_Column, 0, 0, memId);
    sqlite3VdbeAddOp3(v, OP_Ne, memId-1, addr+7, memId);
    sqlite3VdbeChangeP5(v, SQLITE_JUMPIFNULL);
    sqlite3VdbeAddOp2(v, OP_Rowid, 0, memId+1);
    sqlite3VdbeAddOp3(v, OP_Column, 0, 1, memId);
    sqlite3VdbeAddOp2(v, OP_Goto, 0, addr+9);
    sqlite3VdbeAddOp2(v, OP_Next, 0, addr+2);
    sqlite3VdbeAddOp2(v, OP_Integer, 0, memId);
    sqlite3VdbeAddOp0(v, OP_Close);
  }
}

/*
** Update the maximum rowid for an autoincrement calculation.
**
** This routine should be called when the top of the stack holds a
** new rowid that is about to be inserted.  If that new rowid is
** larger than the maximum rowid in the memId memory cell, then the
** memory cell is updated.  The stack is unchanged.
*/
static void autoIncStep(Parse *pParse, int memId, int regRowid){
  if( memId>0 ){
    sqlite3VdbeAddOp2(pParse->pVdbe, OP_MemMax, memId, regRowid);
  }
}

/*
** This routine generates the code needed to write autoincrement
** maximum rowid values back into the sqlite_sequence register.
** Every statement that might do an INSERT into an autoincrement
** table (either directly or through triggers) needs to call this
** routine just before the "exit" code.
*/
void sqlite3AutoincrementEnd(Parse *pParse){
  AutoincInfo *p;
  Vdbe *v = pParse->pVdbe;
  sqlite3 *db = pParse->db;

  assert( v );
  for(p = pParse->pAinc; p; p = p->pNext){
    Db *pDb = &db->aDb[p->iDb];
    int j1, j2, j3, j4, j5;
    int iRec;
    int memId = p->regCtr;

    iRec = sqlite3GetTempReg(pParse);
    assert( sqlite3SchemaMutexHeld(db, 0, pDb->pSchema) );
    sqlite3OpenTable(pParse, 0, p->iDb, pDb->pSchema->pSeqTab, OP_OpenWrite);
    j1 = sqlite3VdbeAddOp1(v, OP_NotNull, memId+1);
    j2 = sqlite3VdbeAddOp0(v, OP_Rewind);
    j3 = sqlite3VdbeAddOp3(v, OP_Column, 0, 0, iRec);
    j4 = sqlite3VdbeAddOp3(v, OP_Eq, memId-1, 0, iRec);
    sqlite3VdbeAddOp2(v, OP_Next, 0, j3);
    sqlite3VdbeJumpHere(v, j2);
    sqlite3VdbeAddOp2(v, OP_NewRowid, 0, memId+1);
    j5 = sqlite3VdbeAddOp0(v, OP_Goto);
    sqlite3VdbeJumpHere(v, j4);
    sqlite3VdbeAddOp2(v, OP_Rowid, 0, memId+1);
    sqlite3VdbeJumpHere(v, j1);
    sqlite3VdbeJumpHere(v, j5);
    sqlite3VdbeAddOp3(v, OP_MakeRecord, memId-1, 2, iRec);
    sqlite3VdbeAddOp3(v, OP_Insert, 0, iRec, memId+1);
    sqlite3VdbeChangeP5(v, OPFLAG_APPEND);
    sqlite3VdbeAddOp0(v, OP_Close);
    sqlite3ReleaseTempReg(pParse, iRec);
  }
}
#else
/*
** If SQLITE_OMIT_AUTOINCREMENT is defined, then the three routines
** above are all no-ops
*/
# define autoIncBegin(A,B,C) (0)
# define autoIncStep(A,B,C)
#endif /* SQLITE_OMIT_AUTOINCREMENT */


/* Forward declaration */
static int xferOptimization(
  Parse *pParse,        /* Parser context */
  Table *pDest,         /* The table we are inserting into */
  Select *pSelect,      /* A SELECT statement to use as the data source */
  int onError,          /* How to handle constraint errors */
  int iDbDest           /* The database of pDest */
);

/*
** This routine is call to handle SQL of the following forms:
**
**    insert into TABLE (IDLIST) values(EXPRLIST)
**    insert into TABLE (IDLIST) select
**
** The IDLIST following the table name is always optional.  If omitted,
** then a list of all columns for the table is substituted.  The IDLIST
** appears in the pColumn parameter.  pColumn is NULL if IDLIST is omitted.
**
** The pList parameter holds EXPRLIST in the first form of the INSERT
** statement above, and pSelect is NULL.  For the second form, pList is
** NULL and pSelect is a pointer to the select statement used to generate
** data for the insert.
**
** The code generated follows one of four templates.  For a simple
** select with data coming from a VALUES clause, the code executes
** once straight down through.  Pseudo-code follows (we call this
** the "1st template"):
**
**         open write cursor to <table> and its indices
**         puts VALUES clause expressions onto the stack
**         write the resulting record into <table>
**         cleanup
**
** The three remaining templates assume the statement is of the form
**
**   INSERT INTO <table> SELECT ...
**
** If the SELECT clause is of the restricted form "SELECT * FROM <table2>" -
** in other words if the SELECT pulls all columns from a single table
** and there is no WHERE or LIMIT or GROUP BY or ORDER BY clauses, and
** if <table2> and <table1> are distinct tables but have identical
** schemas, including all the same indices, then a special optimization
** is invoked that copies raw records from <table2> over to <table1>.
** See the xferOptimization() function for the implementation of this
** template.  This is the 2nd template.
**
**         open a write cursor to <table>
**         open read cursor on <table2>
**         transfer all records in <table2> over to <table>
**         close cursors
**         foreach index on <table>
**           open a write cursor on the <table> index
**           open a read cursor on the corresponding <table2> index
**           transfer all records from the read to the write cursors
**           close cursors
**         end foreach
**
** The 3rd template is for when the second template does not apply
** and the SELECT clause does not read from <table> at any time.
** The generated code follows this template:
**
**         EOF <- 0
**         X <- A
**         goto B
**      A: setup for the SELECT
**         loop over the rows in the SELECT
**           load values into registers R..R+n
**           yield X
**         end loop
**         cleanup after the SELECT
**         EOF <- 1
**         yield X
**         goto A
**      B: open write cursor to <table> and its indices
**      C: yield X
**         if EOF goto D
**         insert the select result into <table> from R..R+n
**         goto C
**      D: cleanup
**
** The 4th template is used if the insert statement takes its
** values from a SELECT but the data is being inserted into a table
** that is also read as part of the SELECT.  In the third form,
** we have to use a intermediate table to store the results of
** the select.  The template is like this:
**
**         EOF <- 0
**         X <- A
**         goto B
**      A: setup for the SELECT
**         loop over the tables in the SELECT
**           load value into register R..R+n
**           yield X
**         end loop
**         cleanup after the SELECT
**         EOF <- 1
**         yield X
**         halt-error
**      B: open temp table
**      L: yield X
**         if EOF goto M
**         insert row from R..R+n into temp table
**         goto L
**      M: open write cursor to <table> and its indices
**         rewind temp table
**      C: loop over rows of intermediate table
**           transfer values form intermediate table into <table>
**         end loop
**      D: cleanup
*/
void sqlite3Insert(
  Parse *pParse,        /* Parser context */
  SrcList *pTabList,    /* Name of table into which we are inserting */
  ExprList *pList,      /* List of values to be inserted */
  Select *pSelect,      /* A SELECT statement to use as the data source */
  IdList *pColumn,      /* Column names corresponding to IDLIST. */
  int onError           /* How to handle constraint errors */
){
  sqlite3 *db;          /* The main database structure */
  Table *pTab;          /* The table to insert into.  aka TABLE */
  char *zTab;           /* Name of the table into which we are inserting */
  const char *zDb;      /* Name of the database holding this table */
  int i, j, idx;        /* Loop counters */
  Vdbe *v;              /* Generate code into this virtual machine */
  Index *pIdx;          /* For looping over indices of the table */
  int nColumn;          /* Number of columns in the data */
  int nHidden = 0;      /* Number of hidden columns if TABLE is virtual */
  int baseCur = 0;      /* VDBE Cursor number for pTab */
  int keyColumn = -1;   /* Column that is the INTEGER PRIMARY KEY */
  int endOfLoop;        /* Label for the end of the insertion loop */
  int useTempTable = 0; /* Store SELECT results in intermediate table */
  int srcTab = 0;       /* Data comes from this temporary cursor if >=0 */
  int addrInsTop = 0;   /* Jump to label "D" */
  int addrCont = 0;     /* Top of insert loop. Label "C" in templates 3 and 4 */
  int addrSelect = 0;   /* Address of coroutine that implements the SELECT */
  SelectDest dest;      /* Destination for SELECT on rhs of INSERT */
  int iDb;              /* Index of database holding TABLE */
  Db *pDb;              /* The database containing table being inserted into */
  int appendFlag = 0;   /* True if the insert is likely to be an append */

  /* Register allocations */
  int regFromSelect = 0;/* Base register for data coming from SELECT */
  int regAutoinc = 0;   /* Register holding the AUTOINCREMENT counter */
  int regRowCount = 0;  /* Memory cell used for the row counter */
  int regIns;           /* Block of regs holding rowid+data being inserted */
  int regRowid;         /* registers holding insert rowid */
  int regData;          /* register holding first column to insert */
  int regEof = 0;       /* Register recording end of SELECT data */
  int *aRegIdx = 0;     /* One register allocated to each index */

#ifndef SQLITE_OMIT_TRIGGER
  int isView;                 /* True if attempting to insert into a view */
  Trigger *pTrigger;          /* List of triggers on pTab, if required */
  int tmask;                  /* Mask of trigger times */
#endif

  db = pParse->db;
  memset(&dest, 0, sizeof(dest));
  if( pParse->nErr || db->mallocFailed ){
    goto insert_cleanup;
  }

  /* Locate the table into which we will be inserting new information.
  */
  assert( pTabList->nSrc==1 );
  zTab = pTabList->a[0].zName;
  if( NEVER(zTab==0) ) goto insert_cleanup;
  pTab = sqlite3SrcListLookup(pParse, pTabList);
  if( pTab==0 ){
    goto insert_cleanup;
  }
  iDb = sqlite3SchemaToIndex(db, pTab->pSchema);
  assert( iDb<db->nDb );
  pDb = &db->aDb[iDb];
  zDb = pDb->zName;
  if( sqlite3AuthCheck(pParse, SQLITE_INSERT, pTab->zName, 0, zDb) ){
    goto insert_cleanup;
  }

  /* Figure out if we have any triggers and if the table being
  ** inserted into is a view
  */
#ifndef SQLITE_OMIT_TRIGGER
  pTrigger = sqlite3TriggersExist(pParse, pTab, TK_INSERT, 0, &tmask);
  isView = pTab->pSelect!=0;
#else
# define pTrigger 0
# define tmask 0
# define isView 0
#endif
#ifdef SQLITE_OMIT_VIEW
# undef isView
# define isView 0
#endif
  assert( (pTrigger && tmask) || (pTrigger==0 && tmask==0) );

  /* If pTab is really a view, make sure it has been initialized.
  ** ViewGetColumnNames() is a no-op if pTab is not a view (or virtual 
  ** module table).
  */
  if( sqlite3ViewGetColumnNames(pParse, pTab) ){
    goto insert_cleanup;
  }

  /* Ensure that:
  *  (a) the table is not read-only, 
  *  (b) that if it is a view then ON INSERT triggers exist
  */
  if( sqlite3IsReadOnly(pParse, pTab, tmask) ){
    goto insert_cleanup;
  }

  /* Allocate a VDBE
  */
  v = sqlite3GetVdbe(pParse);
  if( v==0 ) goto insert_cleanup;
  if( pParse->nested==0 ) sqlite3VdbeCountChanges(v);
  sqlite3BeginWriteOperation(pParse, pSelect || pTrigger, iDb);

#ifndef SQLITE_OMIT_XFER_OPT
  /* If the statement is of the form
  **
  **       INSERT INTO <table1> SELECT * FROM <table2>;
  **
  ** Then special optimizations can be applied that make the transfer
  ** very fast and which reduce fragmentation of indices.
  **
  ** This is the 2nd template.
  */
  if( pColumn==0 && xferOptimization(pParse, pTab, pSelect, onError, iDb) ){
    assert( !pTrigger );
    assert( pList==0 );
    goto insert_end;
  }
#endif /* SQLITE_OMIT_XFER_OPT */

  /* If this is an AUTOINCREMENT table, look up the sequence number in the
  ** sqlite_sequence table and store it in memory cell regAutoinc.
  */
  regAutoinc = autoIncBegin(pParse, iDb, pTab);

  /* Figure out how many columns of data are supplied.  If the data
  ** is coming from a SELECT statement, then generate a co-routine that
  ** produces a single row of the SELECT on each invocation.  The
  ** co-routine is the common header to the 3rd and 4th templates.
  */
  if( pSelect ){
    /* Data is coming from a SELECT.  Generate code to implement that SELECT
    ** as a co-routine.  The code is common to both the 3rd and 4th
    ** templates:
    **
    **         EOF <- 0
    **         X <- A
    **         goto B
    **      A: setup for the SELECT
    **         loop over the tables in the SELECT
    **           load value into register R..R+n
    **           yield X
    **         end loop
    **         cleanup after the SELECT
    **         EOF <- 1
    **         yield X
    **         halt-error
    **
    ** On each invocation of the co-routine, it puts a single row of the
    ** SELECT result into registers dest.iMem...dest.iMem+dest.nMem-1.
    ** (These output registers are allocated by sqlite3Select().)  When
    ** the SELECT completes, it sets the EOF flag stored in regEof.
    */
    int rc, j1;

    regEof = ++pParse->nMem;
    sqlite3VdbeAddOp2(v, OP_Integer, 0, regEof);      /* EOF <- 0 */
    VdbeComment((v, "SELECT eof flag"));
    sqlite3SelectDestInit(&dest, SRT_Coroutine, ++pParse->nMem);
    addrSelect = sqlite3VdbeCurrentAddr(v)+2;
    sqlite3VdbeAddOp2(v, OP_Integer, addrSelect-1, dest.iSDParm);
    j1 = sqlite3VdbeAddOp2(v, OP_Goto, 0, 0);
    VdbeComment((v, "Jump over SELECT coroutine"));

    /* Resolve the expressions in the SELECT statement and execute it. */
    rc = sqlite3Select(pParse, pSelect, &dest);
    assert( pParse->nErr==0 || rc );
    if( rc || NEVER(pParse->nErr) || db->mallocFailed ){
      goto insert_cleanup;
    }
    sqlite3VdbeAddOp2(v, OP_Integer, 1, regEof);         /* EOF <- 1 */
    sqlite3VdbeAddOp1(v, OP_Yield, dest.iSDParm);   /* yield X */
    sqlite3VdbeAddOp2(v, OP_Halt, SQLITE_INTERNAL, OE_Abort);
    VdbeComment((v, "End of SELECT coroutine"));
    sqlite3VdbeJumpHere(v, j1);                          /* label B: */

    regFromSelect = dest.iSdst;
    assert( pSelect->pEList );
    nColumn = pSelect->pEList->nExpr;
    assert( dest.nSdst==nColumn );

    /* Set useTempTable to TRUE if the result of the SELECT statement
    ** should be written into a temporary table (template 4).  Set to
    ** FALSE if each* row of the SELECT can be written directly into
    ** the destination table (template 3).
    **
    ** A temp table must be used if the table being updated is also one
    ** of the tables being read by the SELECT statement.  Also use a 
    ** temp table in the case of row triggers.
    */
    if( pTrigger || readsTable(pParse, addrSelect, iDb, pTab) ){
      useTempTable = 1;
    }

    if( useTempTable ){
      /* Invoke the coroutine to extract information from the SELECT
      ** and add it to a transient table srcTab.  The code generated
      ** here is from the 4th template:
      **
      **      B: open temp table
      **      L: yield X
      **         if EOF goto M
      **         insert row from R..R+n into temp table
      **         goto L
      **      M: ...
      */
      int regRec;          /* Register to hold packed record */
      int regTempRowid;    /* Register to hold temp table ROWID */
      int addrTop;         /* Label "L" */
      int addrIf;          /* Address of jump to M */

      srcTab = pParse->nTab++;
      regRec = sqlite3GetTempReg(pParse);
      regTempRowid = sqlite3GetTempReg(pParse);
      sqlite3VdbeAddOp2(v, OP_OpenEphemeral, srcTab, nColumn);
      addrTop = sqlite3VdbeAddOp1(v, OP_Yield, dest.iSDParm);
      addrIf = sqlite3VdbeAddOp1(v, OP_If, regEof);
      sqlite3VdbeAddOp3(v, OP_MakeRecord, regFromSelect, nColumn, regRec);
      sqlite3VdbeAddOp2(v, OP_NewRowid, srcTab, regTempRowid);
      sqlite3VdbeAddOp3(v, OP_Insert, srcTab, regRec, regTempRowid);
      sqlite3VdbeAddOp2(v, OP_Goto, 0, addrTop);
      sqlite3VdbeJumpHere(v, addrIf);
      sqlite3ReleaseTempReg(pParse, regRec);
      sqlite3ReleaseTempReg(pParse, regTempRowid);
    }
  }else{
    /* This is the case if the data for the INSERT is coming from a VALUES
    ** clause
    */
    NameContext sNC;
    memset(&sNC, 0, sizeof(sNC));
    sNC.pParse = pParse;
    srcTab = -1;
    assert( useTempTable==0 );
    nColumn = pList ? pList->nExpr : 0;
    for(i=0; i<nColumn; i++){
      if( sqlite3ResolveExprNames(&sNC, pList->a[i].pExpr) ){
        goto insert_cleanup;
      }
    }
  }

  /* Make sure the number of columns in the source data matches the number
  ** of columns to be inserted into the table.
  */
  if( IsVirtual(pTab) ){
    for(i=0; i<pTab->nCol; i++){
      nHidden += (IsHiddenColumn(&pTab->aCol[i]) ? 1 : 0);
    }
  }
  if( pColumn==0 && nColumn && nColumn!=(pTab->nCol-nHidden) ){
    sqlite3ErrorMsg(pParse, 
       "table %S has %d columns but %d values were supplied",
       pTabList, 0, pTab->nCol-nHidden, nColumn);
    goto insert_cleanup;
  }
  if( pColumn!=0 && nColumn!=pColumn->nId ){
    sqlite3ErrorMsg(pParse, "%d values for %d columns", nColumn, pColumn->nId);
    goto insert_cleanup;
  }

  /* If the INSERT statement included an IDLIST term, then make sure
  ** all elements of the IDLIST really are columns of the table and 
  ** remember the column indices.
  **
  ** If the table has an INTEGER PRIMARY KEY column and that column
  ** is named in the IDLIST, then record in the keyColumn variable
  ** the index into IDLIST of the primary key column.  keyColumn is
  ** the index of the primary key as it appears in IDLIST, not as
  ** is appears in the original table.  (The index of the primary
  ** key in the original table is pTab->iPKey.)
  */
  if( pColumn ){
    for(i=0; i<pColumn->nId; i++){
      pColumn->a[i].idx = -1;
    }
    for(i=0; i<pColumn->nId; i++){
      for(j=0; j<pTab->nCol; j++){
        if( sqlite3StrICmp(pColumn->a[i].zName, pTab->aCol[j].zName)==0 ){
          pColumn->a[i].idx = j;
          if( j==pTab->iPKey ){
            keyColumn = i;
          }
          break;
        }
      }
      if( j>=pTab->nCol ){
        if( sqlite3IsRowid(pColumn->a[i].zName) ){
          keyColumn = i;
        }else{
          sqlite3ErrorMsg(pParse, "table %S has no column named %s",
              pTabList, 0, pColumn->a[i].zName);
          pParse->checkSchema = 1;
          goto insert_cleanup;
        }
      }
    }
  }

  /* If there is no IDLIST term but the table has an integer primary
  ** key, the set the keyColumn variable to the primary key column index
  ** in the original table definition.
  */
  if( pColumn==0 && nColumn>0 ){
    keyColumn = pTab->iPKey;
  }
    
  /* Initialize the count of rows to be inserted
  */
  if( db->flags & SQLITE_CountRows ){
    regRowCount = ++pParse->nMem;
    sqlite3VdbeAddOp2(v, OP_Integer, 0, regRowCount);
  }

  /* If this is not a view, open the table and and all indices */
  if( !isView ){
    int nIdx;

    baseCur = pParse->nTab;
    nIdx = sqlite3OpenTableAndIndices(pParse, pTab, baseCur, OP_OpenWrite);
    aRegIdx = sqlite3DbMallocRaw(db, sizeof(int)*(nIdx+1));
    if( aRegIdx==0 ){
      goto insert_cleanup;
    }
    for(i=0; i<nIdx; i++){
      aRegIdx[i] = ++pParse->nMem;
    }
  }

  /* This is the top of the main insertion loop */
  if( useTempTable ){
    /* This block codes the top of loop only.  The complete loop is the
    ** following pseudocode (template 4):
    **
    **         rewind temp table
    **      C: loop over rows of intermediate table
    **           transfer values form intermediate table into <table>
    **         end loop
    **      D: ...
    */
    addrInsTop = sqlite3VdbeAddOp1(v, OP_Rewind, srcTab);
    addrCont = sqlite3VdbeCurrentAddr(v);
  }else if( pSelect ){
    /* This block codes the top of loop only.  The complete loop is the
    ** following pseudocode (template 3):
    **
    **      C: yield X
    **         if EOF goto D
    **         insert the select result into <table> from R..R+n
    **         goto C
    **      D: ...
    */
    addrCont = sqlite3VdbeAddOp1(v, OP_Yield, dest.iSDParm);
    addrInsTop = sqlite3VdbeAddOp1(v, OP_If, regEof);
  }

  /* Allocate registers for holding the rowid of the new row,
  ** the content of the new row, and the assemblied row record.
  */
  regRowid = regIns = pParse->nMem+1;
  pParse->nMem += pTab->nCol + 1;
  if( IsVirtual(pTab) ){
    regRowid++;
    pParse->nMem++;
  }
  regData = regRowid+1;

  /* Run the BEFORE and INSTEAD OF triggers, if there are any
  */
  endOfLoop = sqlite3VdbeMakeLabel(v);
  if( tmask & TRIGGER_BEFORE ){
    int regCols = sqlite3GetTempRange(pParse, pTab->nCol+1);

    /* build the NEW.* reference row.  Note that if there is an INTEGER
    ** PRIMARY KEY into which a NULL is being inserted, that NULL will be
    ** translated into a unique ID for the row.  But on a BEFORE trigger,
    ** we do not know what the unique ID will be (because the insert has
    ** not happened yet) so we substitute a rowid of -1
    */
    if( keyColumn<0 ){
      sqlite3VdbeAddOp2(v, OP_Integer, -1, regCols);
    }else{
      int j1;
      if( useTempTable ){
        sqlite3VdbeAddOp3(v, OP_Column, srcTab, keyColumn, regCols);
      }else{
        assert( pSelect==0 );  /* Otherwise useTempTable is true */
        sqlite3ExprCode(pParse, pList->a[keyColumn].pExpr, regCols);
      }
      j1 = sqlite3VdbeAddOp1(v, OP_NotNull, regCols);
      sqlite3VdbeAddOp2(v, OP_Integer, -1, regCols);
      sqlite3VdbeJumpHere(v, j1);
      sqlite3VdbeAddOp1(v, OP_MustBeInt, regCols);
    }

    /* Cannot have triggers on a virtual table. If it were possible,
    ** this block would have to account for hidden column.
    */
    assert( !IsVirtual(pTab) );

    /* Create the new column data
    */
    for(i=0; i<pTab->nCol; i++){
      if( pColumn==0 ){
        j = i;
      }else{
        for(j=0; j<pColumn->nId; j++){
          if( pColumn->a[j].idx==i ) break;
        }
      }
      if( (!useTempTable && !pList) || (pColumn && j>=pColumn->nId) ){
        sqlite3ExprCode(pParse, pTab->aCol[i].pDflt, regCols+i+1);
      }else if( useTempTable ){
        sqlite3VdbeAddOp3(v, OP_Column, srcTab, j, regCols+i+1); 
      }else{
        assert( pSelect==0 ); /* Otherwise useTempTable is true */
        sqlite3ExprCodeAndCache(pParse, pList->a[j].pExpr, regCols+i+1);
      }
    }

    /* If this is an INSERT on a view with an INSTEAD OF INSERT trigger,
    ** do not attempt any conversions before assembling the record.
    ** If this is a real table, attempt conversions as required by the
    ** table column affinities.
    */
    if( !isView ){
      sqlite3VdbeAddOp2(v, OP_Affinity, regCols+1, pTab->nCol);
      sqlite3TableAffinityStr(v, pTab);
    }

    /* Fire BEFORE or INSTEAD OF triggers */
    sqlite3CodeRowTrigger(pParse, pTrigger, TK_INSERT, 0, TRIGGER_BEFORE, 
        pTab, regCols-pTab->nCol-1, onError, endOfLoop);

    sqlite3ReleaseTempRange(pParse, regCols, pTab->nCol+1);
  }

  /* Push the record number for the new entry onto the stack.  The
  ** record number is a randomly generate integer created by NewRowid
  ** except when the table has an INTEGER PRIMARY KEY column, in which
  ** case the record number is the same as that column. 
  */
  if( !isView ){
    if( IsVirtual(pTab) ){
      /* The row that the VUpdate opcode will delete: none */
      sqlite3VdbeAddOp2(v, OP_Null, 0, regIns);
    }
    if( keyColumn>=0 ){
      if( useTempTable ){
        sqlite3VdbeAddOp3(v, OP_Column, srcTab, keyColumn, regRowid);
      }else if( pSelect ){
        sqlite3VdbeAddOp2(v, OP_SCopy, regFromSelect+keyColumn, regRowid);
      }else{
        VdbeOp *pOp;
        sqlite3ExprCode(pParse, pList->a[keyColumn].pExpr, regRowid);
        pOp = sqlite3VdbeGetOp(v, -1);
        if( ALWAYS(pOp) && pOp->opcode==OP_Null && !IsVirtual(pTab) ){
          appendFlag = 1;
          pOp->opcode = OP_NewRowid;
          pOp->p1 = baseCur;
          pOp->p2 = regRowid;
          pOp->p3 = regAutoinc;
        }
      }
      /* If the PRIMARY KEY expression is NULL, then use OP_NewRowid                      //如果主键表达式为空，则使用OP_NewRowid
      ** to generate a unique primary key value.                                          //产生一个唯一的主键值。
      */
      if( !appendFlag ){
        int j1;
        if( !IsVirtual(pTab) ){
          j1 = sqlite3VdbeAddOp1(v, OP_NotNull, regRowid);
          sqlite3VdbeAddOp3(v, OP_NewRowid, baseCur, regRowid, regAutoinc);
          sqlite3VdbeJumpHere(v, j1);
        }else{
          j1 = sqlite3VdbeCurrentAddr(v);
          sqlite3VdbeAddOp2(v, OP_IsNull, regRowid, j1+2);
        }
        sqlite3VdbeAddOp1(v, OP_MustBeInt, regRowid);
      }
    }else if( IsVirtual(pTab) ){
      sqlite3VdbeAddOp2(v, OP_Null, 0, regRowid);
    }else{
      sqlite3VdbeAddOp3(v, OP_NewRowid, baseCur, regRowid, regAutoinc);
      appendFlag = 1;
    }
    autoIncStep(pParse, regAutoinc, regRowid);

    /* Push onto the stack, data for all columns of the new entry, beginning            //压入栈中，新实体的所有列的数据，
    ** with the first column.                                                           //从第一列开始
    */
    nHidden = 0;
    for(i=0; i<pTab->nCol; i++){
      int iRegStore = regRowid+1+i;
      if( i==pTab->iPKey ){
        /* The value of the INTEGER PRIMARY KEY column is always a NULL.                //INTEGER主键列的之总是为空
        ** Whenever this column is read, the record number will be substituted          //无论何时列值是可读的，记录的编号将被取代在这。
        ** in its place.  So will fill this column with a NULL to avoid                 //所以将填补这一列空来避免
        ** taking up data space with information that will never be used. */			//占用数据空间的信息永远不会被使用。
        sqlite3VdbeAddOp2(v, OP_Null, 0, iRegStore);
        continue;
      }
      if( pColumn==0 ){
        if( IsHiddenColumn(&pTab->aCol[i]) ){
          assert( IsVirtual(pTab) );
          j = -1;
          nHidden++;
        }else{
          j = i - nHidden;
        }
      }else{
        for(j=0; j<pColumn->nId; j++){
          if( pColumn->a[j].idx==i ) break;
        }
      }
      if( j<0 || nColumn==0 || (pColumn && j>=pColumn->nId) ){
        sqlite3ExprCode(pParse, pTab->aCol[i].pDflt, iRegStore);
      }else if( useTempTable ){
        sqlite3VdbeAddOp3(v, OP_Column, srcTab, j, iRegStore); 
      }else if( pSelect ){
        sqlite3VdbeAddOp2(v, OP_SCopy, regFromSelect+j, iRegStore);
      }else{
        sqlite3ExprCode(pParse, pList->a[j].pExpr, iRegStore);
      }
    }

    /* Generate code to check constraints and generate index keys and               //代码生成检查约束并产生索引并进行插入。
    ** do the insertion.
    */
#ifndef SQLITE_OMIT_VIRTUALTABLE
    if( IsVirtual(pTab) ){
      const char *pVTab = (const char *)sqlite3GetVTable(db, pTab);
      sqlite3VtabMakeWritable(pParse, pTab);
      sqlite3VdbeAddOp4(v, OP_VUpdate, 1, pTab->nCol+2, regIns, pVTab, P4_VTAB);
      sqlite3VdbeChangeP5(v, onError==OE_Default ? OE_Abort : onError);
      sqlite3MayAbort(pParse);
    }else
#endif
    {
      int isReplace;    /* Set to true if constraints may cause a replace */        //如果约束可能导致替换设置为true
      sqlite3GenerateConstraintChecks(pParse, pTab, baseCur, regIns, aRegIdx,
          keyColumn>=0, 0, onError, endOfLoop, &isReplace
      );
      sqlite3FkCheck(pParse, pTab, 0, regIns);
      sqlite3CompleteInsertion(
          pParse, pTab, baseCur, regIns, aRegIdx, 0, appendFlag, isReplace==0
      );
    }
  }

  /* Update the count of rows that are inserted										//插入更新的行数
  */
  if( (db->flags & SQLITE_CountRows)!=0 ){
    sqlite3VdbeAddOp2(v, OP_AddImm, regRowCount, 1);
  }

  if( pTrigger ){
    /* Code AFTER triggers */														//代码在触发器之后
    sqlite3CodeRowTrigger(pParse, pTrigger, TK_INSERT, 0, TRIGGER_AFTER, 
        pTab, regData-2-pTab->nCol, onError, endOfLoop);
  }

  /* The bottom of the main insertion loop, if the data source          			//主要的底部循环插入,是否数据源是一个
  ** is a SELECT statement.                                                         //select语句。
  */
  sqlite3VdbeResolveLabel(v, endOfLoop);
  if( useTempTable ){
    sqlite3VdbeAddOp2(v, OP_Next, srcTab, addrCont);
    sqlite3VdbeJumpHere(v, addrInsTop);
    sqlite3VdbeAddOp1(v, OP_Close, srcTab);
  }else if( pSelect ){
    sqlite3VdbeAddOp2(v, OP_Goto, 0, addrCont);
    sqlite3VdbeJumpHere(v, addrInsTop);
  }

  if( !IsVirtual(pTab) && !isView ){
    /* Close all tables opened */                                                //关闭所有打开的表
    sqlite3VdbeAddOp1(v, OP_Close, baseCur);
    for(idx=1, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, idx++){
      sqlite3VdbeAddOp1(v, OP_Close, idx+baseCur);
    }
  }

insert_end:
  /* Update the sqlite_sequence table by storing the content of the              //更新sqlite_sequence表通过存储最大rowid值的记录的值的内容
  ** maximum rowid counter values recorded while inserting into                  //然而插入到自动增长的表中。
  ** autoincrement tables.
  */
  if( pParse->nested==0 && pParse->pTriggerTab==0 ){
    sqlite3AutoincrementEnd(pParse);
  }

  /*
  ** Return the number of rows inserted. If this routine is                     //返回插入的行数。如果这个程序产生代码由于调用了
  ** generating code because of a call to sqlite3NestedParse(), do not          //sqlite3NestedParse()，不会唤醒回调函数。
  ** invoke the callback function.
  */
  if( (db->flags&SQLITE_CountRows) && !pParse->nested && !pParse->pTriggerTab ){
    sqlite3VdbeAddOp2(v, OP_ResultRow, regRowCount, 1);
    sqlite3VdbeSetNumCols(v, 1);
    sqlite3VdbeSetColName(v, 0, COLNAME_NAME, "rows inserted", SQLITE_STATIC);
  }

insert_cleanup:
  sqlite3SrcListDelete(db, pTabList);
  sqlite3ExprListDelete(db, pList);
  sqlite3SelectDelete(db, pSelect);
  sqlite3IdListDelete(db, pColumn);
  sqlite3DbFree(db, aRegIdx);
}

/* Make sure "isView" and other macros defined above are undefined. Otherwise    //确保“isView”和其他上面宏定义是未定义的
** thely may interfere with compilation of other functions in this file          //否则会产生干扰在编辑其他功能在这个文件中
** (or in another file, if this file becomes part of the amalgamation).  */      //（或在另一个文件,如果这个文件成为融合的一部分）
#ifdef isView
 #undef isView
#endif
#ifdef pTrigger
 #undef pTrigger
#endif
#ifdef tmask
 #undef tmask
#endif


/*
** Generate code to do constraint checks prior to an INSERT or an UPDATE.                             生成代码做约束检查当一个插入或更新操作。
**
** The input is a range of consecutive registers as follows:                                          输入的范围的连贯性表现为以下：
**
**    1.  The rowid of the row after the update.                                                      行ID更新在行更新之后
**
**    2.  The data in the first column of the entry after the update.                                 数据的第一列在整个实体更新之后更新
**
**    i.  Data from middle columns...                                                                 数据来自中间的列的...
**
**    N.  The data in the last column of the entry after the update.                                  最后一列的数据在整个实体更新之后更新
**
** The regRowid parameter is the index of the register containing (1).                                那个regRowid参数是寄存器包含的索引
**
** If isUpdate is true and rowidChng is non-zero, then rowidChng contains                             如果参数isUpdate是TRUE并且参数rowidChng不为零
** the address of a register containing the rowid before the update takes                             则rowidChng包含一个寄存器地址包含rowid 在更新之前发生
** place. isUpdate is true for UPDATEs and false for INSERTs. If isUpdate                             isUpdate参数是true表示更新操作，false表示插入操作，
** is false, indicating an INSERT statement, then a non-zero rowidChng                                如果isUpdate是false，表明一个insert语句，然而非零的rowidChng
** indicates that the rowid was explicitly specified as part of the                                   参数表明rowid被显示的为insert语句的一部分，如果rowidChng是false
** INSERT statement. If rowidChng is false, it means that  the rowid is                               意味着rowid是自动计算的在一个插入或是rowid不需要修改的更新操作。
** computed automatically in an insert or that the rowid value is not 
** modified by an update.
**
** The code generated by this routine store new index entries into                                    这些代码产生由程序，存储新的索引实体到寄存器中通过
** registers identified by aRegIdx[].  No index entry is created for                                  aRegIdx[]数组来识别。没有实体索引被创建在aRegIdx[i]==0
** indices where aRegIdx[i]==0.  The order of indices in aRegIdx[] is                                 时。指示的顺序在aRegIdx[]与链表（依附于表）的顺序一致，
** the same as the order of indices on the linked list of indices
** attached to the table.
**
** This routine also generates code to check constraints.  NOT NULL,                                  这个程序也会产生代码去检查约束。非空，
** CHECK, and UNIQUE constraints are all checked.  If a constraint fails,                             检查，唯一约束都会被检查。如果约束失效，
** then the appropriate action is performed.  There are five possible                                 然后适当的动作被执行。有五种可能的动作
** actions: ROLLBACK, ABORT, FAIL, REPLACE, and IGNORE.                                               ROLLBACK（回滚）, ABORT（终止）, FAIL（失败）, REPLACE（代替）, and IGNORE（忽略）.
**
**  Constraint type  Action       What Happens                                              约束类型      动作       说明
**  ---------------  ----------   ----------------------------------------               ---------------------------------------------------------  
**  any              ROLLBACK     The current transaction is rolled back and               任何           回滚     当前的事务被回滚并且 sqlite3_exec()
**                                sqlite3_exec() returns immediately with a                                        返回立即QLITE_CONSTRAINT的代码
**                                return code of SQLITE_CONSTRAINT.
**
**  any              ABORT        Back out changes from the current command                任何           终止     撤销更改从当前的命令中，只（对于已完成的不进行回滚）
**                                only (do not do a complete rollback) then                                        然而 由于sqlite3_exec()会立即返回SQLITE_CONSTRAINT代码
**                                cause sqlite3_exec() to return immediately
**                                with SQLITE_CONSTRAINT.
**
**  any              FAIL         Sqlite3_exec() returns immediately with a                任何          失败      Sqlite3_exec()立即返回一个SQLITE_CONSTRAINT
**                                return code of SQLITE_CONSTRAINT.  The                                           代码，事务不会回滚，并且任何之前的改变都会保留 
**                                transaction is not rolled back and any
**                                prior changes are retained.
**
**  any              IGNORE       The record number and data is popped from                任何           忽略      记录的数量和数据出现在堆中，并且
**                                the stack and there is an immediate jump                                          立即跳到标签 忽略其他。 
**                                to label ignoreDest.
**
**  NOT NULL         REPLACE      The NULL value is replace by the default                 非空          代替       空值被默认值代替在那列值中，如果默认值为空   
**                                value for that column.  If the default value                                      这动作与终止相同。     
**                                is NULL, the action is the same as ABORT.
**
**  UNIQUE           REPLACE      The other row that conflicts with the row                唯一           代替       其他行的冲突是被查入行被删掉
**                                being inserted is removed.
**
**  CHECK            REPLACE      Illegal.  The results in an exception.                   检查          代替        非法的，结果集是一个异常。 
**
** Which action to take is determined by the overrideError parameter.                 哪一个动作被执行取决于被重写的错误的参数。
** Or if overrideError==OE_Default, then the pParse->onError parameter                或者如果overrideError==OE_Default，则pParse->onError 参数被使用
** is used.  Or if pParse->onError==OE_Default then the onError value                 或者如果pParse->onError==OE_Default则onError的值将被约束使用
** for the constraint is used.
**
** The calling routine must open a read/write cursor for pTab with                    调用程序必须打开读/写游标，pTab随着游标的数目计入到baseCur中。
** cursor number "baseCur".  All indices of pTab must also have open                  所有的pTab指标必须打开读/写游标随着游标数目baseCur+i对第i个游标
** read/write cursors with cursor number baseCur+i for the i-th cursor.               
** Except, if there is no possibility of a REPLACE action then                        此外，如果不可能有一个代替动作，则游标不需要打开，在aRegIdx[i]==0。
** cursors do not need to be open for indices where aRegIdx[i]==0.
*/
void sqlite3GenerateConstraintChecks(
  Parse *pParse,      /* The parser context */                                         //分析上下文环境
  Table *pTab,        /* the table into which we are inserting */                      //要进行插入的表
  int baseCur,        /* Index of a read/write cursor pointing at pTab */               //读/写游标的索引在pTab
  int regRowid,       /* Index of the range of input registers */                       //输入寄存器的下标的范围
  int *aRegIdx,       /* Register used by each index.  0 for unused indices */          //寄存器使用的下标，0是未被使用的指标
  int rowidChng,      /* True if the rowid might collide with existing entry */         //true如果rowid和存在实体冲突时
  int isUpdate,       /* True for UPDATE, False for INSERT */                           //true是更新操作，false是插入操作
  int overrideError,  /* Override onError to this if not OE_Default */                  //重写onError到这，如果不是OE_Default
  int ignoreDest,     /* Jump to this label on an OE_Ignore resolution */               //跳到这个标签在一个OE_Ignore解决
  int *pbMayReplace   /* OUT: Set to true if constraint may cause a replace */          //输出，设置真如果限制可能引起一个代替动作
){
  int i;              /* loop counter */                                               //循环计数器i
  Vdbe *v;            /* VDBE under constrution */                                      //虚拟数据库引擎的构建
  int nCol;           /* Number of columns */                                          //列的数量 
  int onError;        /* Conflict resolution strategy */                                //冲突的解决策略
  int j1;             /* Addresss of jump instruction */                                //地址跳跃指令
  int j2 = 0, j3;     /* Addresses of jump instructions */                               //地址跳跃指令  
  int regData;        /* Register containing first data column */                       //寄存器包含第一个数据列
  int iCur;           /* Table cursor number */                                          //表游标数量
  Index *pIdx;         /* Pointer to one of the indices */                              //指针的一个指示   
  sqlite3 *db;         /* Database connection */                                         //数据库连接
  int seenReplace = 0; /* True if REPLACE is used to resolve INT PK conflict */          //真值如果代替被使用解决INT主键冲突
  int regOldRowid = (rowidChng && isUpdate) ? rowidChng : regRowid;

  db = pParse->db;
  v = sqlite3GetVdbe(pParse);
  assert( v!=0 );
  assert( pTab->pSelect==0 );  /* This table is not a VIEW */                           //这个表不是视图
  nCol = pTab->nCol;
  regData = regRowid + 1;

  /* Test all NOT NULL constraints.                                                   //测试所有非空约束    
  */
  for(i=0; i<nCol; i++){
    if( i==pTab->iPKey ){
      continue;
    }
    onError = pTab->aCol[i].notNull;
    if( onError==OE_None ) continue;
    if( overrideError!=OE_Default ){
      onError = overrideError;
    }else if( onError==OE_Default ){
      onError = OE_Abort;
    }
    if( onError==OE_Replace && pTab->aCol[i].pDflt==0 ){
      onError = OE_Abort;
    }
    assert( onError==OE_Rollback || onError==OE_Abort || onError==OE_Fail
        || onError==OE_Ignore || onError==OE_Replace );
    switch( onError ){
      case OE_Abort:
        sqlite3MayAbort(pParse);
      case OE_Rollback:
      case OE_Fail: {
        char *zMsg;
        sqlite3VdbeAddOp3(v, OP_HaltIfNull,
                                  SQLITE_CONSTRAINT, onError, regData+i);
        zMsg = sqlite3MPrintf(db, "%s.%s may not be NULL",
                              pTab->zName, pTab->aCol[i].zName);
        sqlite3VdbeChangeP4(v, -1, zMsg, P4_DYNAMIC);
        break;
      }
      case OE_Ignore: {
        sqlite3VdbeAddOp2(v, OP_IsNull, regData+i, ignoreDest);
        break;
      }
      default: {
        assert( onError==OE_Replace );
        j1 = sqlite3VdbeAddOp1(v, OP_NotNull, regData+i);
        sqlite3ExprCode(pParse, pTab->aCol[i].pDflt, regData+i);
        sqlite3VdbeJumpHere(v, j1);
        break;
      }
    }
  }

  /* Test all CHECK constraints                                                    //测试所有检查约束
  */
#ifndef SQLITE_OMIT_CHECK
  if( pTab->pCheck && (db->flags & SQLITE_IgnoreChecks)==0 ){
    ExprList *pCheck = pTab->pCheck;
    pParse->ckBase = regData;
    onError = overrideError!=OE_Default ? overrideError : OE_Abort;
    for(i=0; i<pCheck->nExpr; i++){
      int allOk = sqlite3VdbeMakeLabel(v);
      sqlite3ExprIfTrue(pParse, pCheck->a[i].pExpr, allOk, SQLITE_JUMPIFNULL);
      if( onError==OE_Ignore ){
        sqlite3VdbeAddOp2(v, OP_Goto, 0, ignoreDest);
      }else{
        char *zConsName = pCheck->a[i].zName;
        if( onError==OE_Replace ) onError = OE_Abort; /* IMP: R-15569-63625 */        //
        if( zConsName ){
          zConsName = sqlite3MPrintf(db, "constraint %s failed", zConsName);
        }else{
          zConsName = 0;
        }
        sqlite3HaltConstraint(pParse, onError, zConsName, P4_DYNAMIC);
      }
      sqlite3VdbeResolveLabel(v, allOk);
    }
  }
#endif /* !defined(SQLITE_OMIT_CHECK) */

  /* If we have an INTEGER PRIMARY KEY, make sure the primary key                  //如果我们有INTEGER主键，确定新记录的主键之前不存在
  ** of the new record does not previously exist.  Except, if this                  //除了，如果这是一个更新并且主键不改变这是OK的
  ** is an UPDATE and the primary key is not changing, that is OK.
  */
  if( rowidChng ){
    onError = pTab->keyConf;
    if( overrideError!=OE_Default ){
      onError = overrideError;
    }else if( onError==OE_Default ){
      onError = OE_Abort;
    }
    
    if( isUpdate ){
      j2 = sqlite3VdbeAddOp3(v, OP_Eq, regRowid, 0, rowidChng);
    }
    j3 = sqlite3VdbeAddOp3(v, OP_NotExists, baseCur, 0, regRowid);
    switch( onError ){
      default: {
        onError = OE_Abort;
        /* Fall thru into the next case */                              //下降到下一个案例
      }
      case OE_Rollback:
      case OE_Abort:
      case OE_Fail: {
        sqlite3HaltConstraint(
          pParse, onError, "PRIMARY KEY must be unique", P4_STATIC);
        break;
      }
      case OE_Replace: {
        /* If there are DELETE triggers on this table and the                           //如果有删除触发器在这个表上并且
        ** recursive-triggers flag is set, call GenerateRowDelete() to                  //循环触发器标志设置了，调用GenerateRowDelete()
        ** remove the conflicting row from the table. This will fire                    //从表中删除冲突的行这将启动触发器并且
        ** the triggers and remove both the table and index b-tree entries.             //删除表和b树索引的实体
        **
        ** Otherwise, if there are no triggers or the recursive-triggers                //另外，如果没有触发器或是循环触发器标志没有设置
        ** flag is not set, but the table has one or more indexes, call                 //但是表有一个或多个索引，调用GenerateRowIndexDelete()
        ** GenerateRowIndexDelete(). This removes the index b-tree entries              //这将只删除b树索引实体，
        ** only. The table b-tree entry will be replaced by the new entry               //表的b树实体将被新实体替换当实体被插入时
        ** when it is inserted.  
        **
        ** If either GenerateRowDelete() or GenerateRowIndexDelete() is called,         //如果GenerateRowDelete()或者GenerateRowIndexDelete()被调用
        ** also invoke MultiWrite() to indicate that this VDBE may require              //并且调用MultiWrite()去指出哪一个虚拟数据库引擎要求声明
        ** statement rollback (if the statement is aborted after the delete             //回滚（如果那个声明在删除发生后被终止）
        ** takes place). Earlier versions called sqlite3MultiWrite() regardless,        //早期版本调用sqlite3MultiWrite()无论什么情况，
        ** but being more selective here allows statements like:                        //但是存在更多的可选择在这，允许一下声明： 
        **
        **   REPLACE INTO t(rowid) VALUES($newrowid)                                    // REPLACE INTO t(rowid) VALUES($newrowid)
        **
        ** to run without a statement journal if there are no indexes on the            //运行没有声明日志，如果没有索引在表上。
        ** table.
        */
        Trigger *pTrigger = 0;
        if( db->flags&SQLITE_RecTriggers ){
          pTrigger = sqlite3TriggersExist(pParse, pTab, TK_DELETE, 0, 0);
        }
        if( pTrigger || sqlite3FkRequired(pParse, pTab, 0, 0) ){
          sqlite3MultiWrite(pParse);
          sqlite3GenerateRowDelete(
              pParse, pTab, baseCur, regRowid, 0, pTrigger, OE_Replace
          );
        }else if( pTab->pIndex ){
          sqlite3MultiWrite(pParse);
          sqlite3GenerateRowIndexDelete(pParse, pTab, baseCur, 0);
        }
        seenReplace = 1;
        break;
      }
      case OE_Ignore: {
        assert( seenReplace==0 );
        sqlite3VdbeAddOp2(v, OP_Goto, 0, ignoreDest);
        break;
      }
    }
    sqlite3VdbeJumpHere(v, j3);
    if( isUpdate ){
      sqlite3VdbeJumpHere(v, j2);
    }
  }

  /* Test all UNIQUE constraints by creating entries for each UNIQUE                   //测试所有唯一约束通过创建实体为每一个唯一的
  ** index and making sure that duplicate entries do not already exist.                //索引并且确定 重复的实体不会存在  
  ** Add the new records to the indices as we go.                                      //添加新的记录到指标
  */
  for(iCur=0, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, iCur++){
    int regIdx;
    int regR;

    if( aRegIdx[iCur]==0 ) continue;  /* Skip unused indices */                       //跳过不用的指标

    /* Create a key for accessing the index entry */                                  //创建一个键来访问索引实体
    regIdx = sqlite3GetTempRange(pParse, pIdx->nColumn+1);
    for(i=0; i<pIdx->nColumn; i++){
      int idx = pIdx->aiColumn[i];
      if( idx==pTab->iPKey ){
        sqlite3VdbeAddOp2(v, OP_SCopy, regRowid, regIdx+i);
      }else{
        sqlite3VdbeAddOp2(v, OP_SCopy, regData+idx, regIdx+i);
      }
    }
    sqlite3VdbeAddOp2(v, OP_SCopy, regRowid, regIdx+i);
    sqlite3VdbeAddOp3(v, OP_MakeRecord, regIdx, pIdx->nColumn+1, aRegIdx[iCur]);
    sqlite3VdbeChangeP4(v, -1, sqlite3IndexAffinityStr(v, pIdx), P4_TRANSIENT);
    sqlite3ExprCacheAffinityChange(pParse, regIdx, pIdx->nColumn+1);

    /* Find out what action to take in case there is an indexing conflict */         //找出什么动作会引起索引冲突
    onError = pIdx->onError;
    if( onError==OE_None ){ 
      sqlite3ReleaseTempRange(pParse, regIdx, pIdx->nColumn+1);
      continue;  /* pIdx is not a UNIQUE index */
    }
    if( overrideError!=OE_Default ){
      onError = overrideError;
    }else if( onError==OE_Default ){
      onError = OE_Abort;
    }
    if( seenReplace ){
      if( onError==OE_Ignore ) onError = OE_Replace;
      else if( onError==OE_Fail ) onError = OE_Abort;
    }
    
    /* Check to see if the new index entry will be unique */                        //查看是否有新的索引实体是唯一的
    regR = sqlite3GetTempReg(pParse);
    sqlite3VdbeAddOp2(v, OP_SCopy, regOldRowid, regR);
    j3 = sqlite3VdbeAddOp4(v, OP_IsUnique, baseCur+iCur+1, 0,
                           regR, SQLITE_INT_TO_PTR(regIdx),
                           P4_INT32);
    sqlite3ReleaseTempRange(pParse, regIdx, pIdx->nColumn+1);

    /* Generate code that executes if the new index entry is not unique */          //生成代码将执行如果新索引实体不是唯一的
    assert( onError==OE_Rollback || onError==OE_Abort || onError==OE_Fail
        || onError==OE_Ignore || onError==OE_Replace );
    switch( onError ){
      case OE_Rollback:
      case OE_Abort:
      case OE_Fail: {
        int j;
        StrAccum errMsg;
        const char *zSep;
        char *zErr;

        sqlite3StrAccumInit(&errMsg, 0, 0, 200);
        errMsg.db = db;
        zSep = pIdx->nColumn>1 ? "columns " : "column ";
        for(j=0; j<pIdx->nColumn; j++){
          char *zCol = pTab->aCol[pIdx->aiColumn[j]].zName;
          sqlite3StrAccumAppend(&errMsg, zSep, -1);
          zSep = ", ";
          sqlite3StrAccumAppend(&errMsg, zCol, -1);
        }
        sqlite3StrAccumAppend(&errMsg,
            pIdx->nColumn>1 ? " are not unique" : " is not unique", -1);
        zErr = sqlite3StrAccumFinish(&errMsg);
        sqlite3HaltConstraint(pParse, onError, zErr, 0);
        sqlite3DbFree(errMsg.db, zErr);
        break;
      }
      case OE_Ignore: {
        assert( seenReplace==0 );
        sqlite3VdbeAddOp2(v, OP_Goto, 0, ignoreDest);
        break;
      }
      default: {
        Trigger *pTrigger = 0;
        assert( onError==OE_Replace );
        sqlite3MultiWrite(pParse);
        if( db->flags&SQLITE_RecTriggers ){
          pTrigger = sqlite3TriggersExist(pParse, pTab, TK_DELETE, 0, 0);
        }
        sqlite3GenerateRowDelete(
            pParse, pTab, baseCur, regR, 0, pTrigger, OE_Replace
        );
        seenReplace = 1;
        break;
      }
    }
    sqlite3VdbeJumpHere(v, j3);
    sqlite3ReleaseTempReg(pParse, regR);
  }
  
  if( pbMayReplace ){
    *pbMayReplace = seenReplace;
  }
}

/*
** This routine generates code to finish the INSERT or UPDATE operation              //程序产生代码来完成插入或者更新操作
** that was started by a prior call to sqlite3GenerateConstraintChecks.              //开始通过之前称为sqlite3GenerateConstraintChecks
** A consecutive range of registers starting at regRowid contains the                //寄存器连续的范围开始在regRowid包含rowid和要被插入的数据
** rowid and the content to be inserted.
**
** The arguments to this routine should be the same as the first six                 //程序的参数应该和sqlite3GenerateConstraintChecks的前六个参数一样
** arguments to sqlite3GenerateConstraintChecks.
*/
void sqlite3CompleteInsertion(
  Parse *pParse,      /* The parser context */                                       //分析上下文环境
  Table *pTab,        /* the table into which we are inserting */                    //我们要插入的表
  int baseCur,        /* Index of a read/write cursor pointing at pTab */            //读/写指针指向pTab指数
  int regRowid,       /* Range of content */                                         //内容的范围
  int *aRegIdx,       /* Register used by each index.  0 for unused indices */       //寄存器使用的每一个索引，0表示未被使用索引 
  int isUpdate,       /* True for UPDATE, False for INSERT */                        //TRUE是更新，FALSE是插入  
  int appendBias,     /* True if this is likely to be an append */                   // TRUE，如果这可能是一个附加
  int useSeekResult   /* True to set the USESEEKRESULT flag on OP_[Idx]Insert */     //TRUE设置USESEEKRESULT标记，在OP_[Idx]插入
){
  int i;
  Vdbe *v;
  int nIdx;
  Index *pIdx;
  u8 pik_flags;
  int regData;
  int regRec;

  v = sqlite3GetVdbe(pParse);
  assert( v!=0 );
  assert( pTab->pSelect==0 );  /* This table is not a VIEW */
  for(nIdx=0, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, nIdx++){}
  for(i=nIdx-1; i>=0; i--){
    if( aRegIdx[i]==0 ) continue;
    sqlite3VdbeAddOp2(v, OP_IdxInsert, baseCur+i+1, aRegIdx[i]);
    if( useSeekResult ){
      sqlite3VdbeChangeP5(v, OPFLAG_USESEEKRESULT);
    }
  }
  regData = regRowid + 1;
  regRec = sqlite3GetTempReg(pParse);
  sqlite3VdbeAddOp3(v, OP_MakeRecord, regData, pTab->nCol, regRec);
  sqlite3TableAffinityStr(v, pTab);
  sqlite3ExprCacheAffinityChange(pParse, regData, pTab->nCol);
  if( pParse->nested ){
    pik_flags = 0;
  }else{
    pik_flags = OPFLAG_NCHANGE;
    pik_flags |= (isUpdate?OPFLAG_ISUPDATE:OPFLAG_LASTROWID);
  }
  if( appendBias ){
    pik_flags |= OPFLAG_APPEND;
  }
  if( useSeekResult ){
    pik_flags |= OPFLAG_USESEEKRESULT;
  }
  sqlite3VdbeAddOp3(v, OP_Insert, baseCur, regRec, regRowid);
  if( !pParse->nested ){
    sqlite3VdbeChangeP4(v, -1, pTab->zName, P4_TRANSIENT);
  }
  sqlite3VdbeChangeP5(v, pik_flags);
}

/*
** Generate code that will open cursors for a table and for all                         //生成代码将打开一个表的游标并且表的所有指标
** indices of that table.  The "baseCur" parameter is the cursor number used            //baseCur参数是表所使用的游标的数量
** for the table.  Indices are opened on subsequent cursors.                            //在随后的游标索引被打开。
**
** Return the number of indices on the table.                                          //返回表的指标的数量
*/
int sqlite3OpenTableAndIndices(
  Parse *pParse,   /* Parsing context */                                               //分析上下文
  Table *pTab,     /* Table to be opened */                                            //表被打开
  int baseCur,     /* Cursor number assigned to the table */                           //游标数量分配到表
  int op           /* OP_OpenRead or OP_OpenWrite */                                   //OP_OpenRead 或者 OP_OpenWrite
){
  int i;
  int iDb;
  Index *pIdx;
  Vdbe *v;

  if( IsVirtual(pTab) ) return 0;
  iDb = sqlite3SchemaToIndex(pParse->db, pTab->pSchema);
  v = sqlite3GetVdbe(pParse);
  assert( v!=0 );
  sqlite3OpenTable(pParse, baseCur, iDb, pTab, op);
  for(i=1, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, i++){
    KeyInfo *pKey = sqlite3IndexKeyinfo(pParse, pIdx);
    assert( pIdx->pSchema==pTab->pSchema );
    sqlite3VdbeAddOp4(v, op, i+baseCur, pIdx->tnum, iDb,
                      (char*)pKey, P4_KEYINFO_HANDOFF);
    VdbeComment((v, "%s", pIdx->zName));
  }
  if( pParse->nTab<baseCur+i ){
    pParse->nTab = baseCur+i;
  }
  return i-1;
}


#ifdef SQLITE_TEST
/*
** The following global variable is incremented whenever the                          //下一个全局变量被增加当转移优化被使用时
** transfer optimization is used.  This is used for testing                           //这被用来测试目标
** purposes only - to make sure the transfer optimization really                      //确保转移最优真正存在这假设的时候出现
** is happening when it is suppose to.
*/
int sqlite3_xferopt_count;
#endif /* SQLITE_TEST */


#ifndef SQLITE_OMIT_XFER_OPT
/*
** Check to collation names to see if they are compatible.                           //检查校对名字看他们是否可兼容的
*/
static int xferCompatibleCollation(const char *z1, const char *z2){
  if( z1==0 ){
    return z2==0;
  }
  if( z2==0 ){
    return 0;
  }
  return sqlite3StrICmp(z1, z2)==0;
}


/*
** Check to see if index pSrc is compatible as a source of data                            //查看是否索引pSrc是可兼容的作为数据源
** for index pDest in an insert transfer optimization.  The rules                          //为了索引pDest在一个插入转移最优中。 
** for a compatible index:                                                                 //可兼容的索引规则： 
**
**    *   The index is over the same set of columns                                        //该索引是在相同的列集合中
**    *   The same DESC and ASC markings occurs on all columns                             //DESC 和 ASC 相同标记在所有列上
**    *   The same onError processing (OE_Abort, OE_Ignore, etc)                           //相同的onError加工过程  
**    *   The same collating sequence on each column                                       //相同的校准顺序在每一列上
*/
static int xferCompatibleIndex(Index *pDest, Index *pSrc){
  int i;
  assert( pDest && pSrc );
  assert( pDest->pTable!=pSrc->pTable );
  if( pDest->nColumn!=pSrc->nColumn ){
    return 0;   /* Different number of columns */                                          //不同数量的列
  }
  if( pDest->onError!=pSrc->onError ){
    return 0;   /* Different conflict resolution strategies */                             //不同的碰撞解决策略
  }
  for(i=0; i<pSrc->nColumn; i++){
    if( pSrc->aiColumn[i]!=pDest->aiColumn[i] ){
      return 0;   /* Different columns indexed */                                         //不同的列索引
    }
    if( pSrc->aSortOrder[i]!=pDest->aSortOrder[i] ){
      return 0;   /* Different sort orders */                                             //不同的排序
    }
    if( !xferCompatibleCollation(pSrc->azColl[i],pDest->azColl[i]) ){
      return 0;   /* Different collating sequences */                                      //不同的核对序列 
    }
  }

  /* If no test above fails then the indices must be compatible */                        //如果以上的测试不出错则 索引是可兼容的  
  return 1;
}

/*
** Attempt the transfer optimization on INSERTs of the form                         试图以最优化的方法插入表单中
**
**     INSERT INTO tab1 SELECT * FROM tab2;                                          例如语句
**
** The xfer optimization transfers raw records from tab2 over to tab1.              从tab2中传行记录较优
** Columns are not decoded and reassemblied, which greatly improves                 列不用解析和重新聚集，这样有很大的性能提升  
** performance.  Raw index records are transferred in the same way.                 未加索引的也是这样进行记录插入
**
** The xfer optimization is only attempted if tab1 and tab2 are compatible.         这种最优化只是试图tab1与tab2可兼容的
** There are lots of rules for determining compatibility - see comments             有许多规则决定兼容性（详细请看代码详解）
** embedded in the code for details.
**
** This routine returns TRUE if the optimization is guaranteed to be used.           这套程序返回true如果最优化被使用
** Sometimes the xfer optimization will only work if the destination table           有时转移（xfer）最优化只会在目标table为空时才工作，
** is empty - a factor that can only be determined at run-time.  In that             一个因素是只有在运行时才能被检测到。由于这个原因，本程序
** case, this routine generates code for the xfer optimization but also              编码为了转移（xfer）最优,但是做一个测试来看是否目标table
** does a test to see if the destination table is empty and jumps over the           为空，并且跳过xfer最优的代码，表明测试失败。在这种情，程序返回
** xfer optimization code if the test fails.  In that case, this routine             FALSE，所以调用者将知道往下执行，调用一个不优化的转移。这个程序返回  
** returns FALSE so that the caller will know to go ahead and generate               FALSE，是没有机会让xfer最优被使用。
** an unoptimized transfer.  This routine also returns FALSE if there
** is no chance that the xfer optimization can be applied.
**
** This optimization is particularly useful at making VACUUM run faster.             这种最优在making VACUUM执行快速尤其有用
*/
static int xferOptimization(
  Parse *pParse,        /* Parser context */                                        // 分析程序上下文
  Table *pDest,         /* The table we are inserting into */                       //插入的table
  Select *pSelect,      /* A SELECT statement to use as the data source */          //一个select语句用来作为数据源
  int onError,          /* How to handle constraint errors */                       //怎样控制约束错误
  int iDbDest           /* The database of pDest */                                 //pdest数据库，（即插入表的数据库）  
){
  ExprList *pEList;                /* The result set of the SELECT */               //select的结果集
  Table *pSrc;                     /* The table in the FROM clause of SELECT */     //from子句的select table 
  Index *pSrcIdx, *pDestIdx;       /* Source and destination indices */             //源和目标目录
  struct SrcList_item *pItem;      /* An element of pSelect->pSrc */                //元素，pSelect->pSrc
  int i;                           /* Loop counter */                               //遍历变量计数i
  int iDbSrc;                      /* The database of pSrc */                       //psrc的数据库
  int iSrc, iDest;                 /* Cursors from source and destination */        //游标从源到目标
  int addr1, addr2;                /* Loop addresses */                             //遍历地址  
  int emptyDestTest;               /* Address of test for empty pDest */            //空pDest的测试地址
  int emptySrcTest;                /* Address of test for empty pSrc */             //空pSrc的测试地址
  Vdbe *v;                         /* The VDBE we are building */                   //虚拟数据库引擎建立
  KeyInfo *pKey;                   /* Key information for an index */               //索引的关键信息
  int regAutoinc;                  /* Memory register used by AUTOINC */            //存储寄存器使用AUTOINC
  int destHasUniqueIdx = 0;        /* True if pDest has a UNIQUE index */           //true 如果pDest有唯一索引
  int regData, regRowid;           /* Registers holding data and rowid */           //寄存器持有数据和行id（data和rowid）

  if( pSelect==0 ){
    return 0;   /* Must be of the form  INSERT INTO ... SELECT ... */               //必须是INSERT INTO ... SELECT ...的形式
  }
  if( sqlite3TriggerList(pParse, pDest) ){
    return 0;   /* tab1 must not have triggers */                                   //tab1不可以有触发器
  }
#ifndef SQLITE_OMIT_VIRTUALTABLE
  if( pDest->tabFlags & TF_Virtual ){
    return 0;   /* tab1 must not be a virtual table */                              //tab1不可以是虚表
  }
#endif
  if( onError==OE_Default ){
    if( pDest->iPKey>=0 ) onError = pDest->keyConf;
    if( onError==OE_Default ) onError = OE_Abort;
  }
  assert(pSelect->pSrc);   /* allocated even if there is no FROM clause */         //分配即使没有from子句 
  if( pSelect->pSrc->nSrc!=1 ){
    return 0;   /* FROM clause must have exactly one term */                       //from子句必须有有一项
  }
  if( pSelect->pSrc->a[0].pSelect ){
    return 0;   /* FROM clause cannot contain a subquery */                        //from子句不含有子查询
  }
  if( pSelect->pWhere ){
    return 0;   /* SELECT may not have a WHERE clause */                           //select 没有一个where子句
  }
  if( pSelect->pOrderBy ){
    return 0;   /* SELECT may not have an ORDER BY clause */                       //select 没有一个order by子句
  }
  /* Do not need to test for a HAVING clause.  If HAVING is present but            //不需要测试having子句，如果having子句出现，但是没有orderby
  ** there is no ORDER BY, we will get an error. */                                //我们将得到一个错误。
  if( pSelect->pGroupBy ){
    return 0;   /* SELECT may not have a GROUP BY clause */                        //select 没有一个group by子句
  }
  if( pSelect->pLimit ){
    return 0;   /* SELECT may not have a LIMIT clause */                           //select 没有一个limit子句
  }
  assert( pSelect->pOffset==0 );  /* Must be so if pLimit==0 */                    //一定如此如果pLimit==0
  if( pSelect->pPrior ){
    return 0;   /* SELECT may not be a compound query */                           //select 不可能有复合查询
  }
  if( pSelect->selFlags & SF_Distinct ){
    return 0;   /* SELECT may not be DISTINCT */                                   //select 不可能有distinct
  }
  pEList = pSelect->pEList;
  assert( pEList!=0 );
  if( pEList->nExpr!=1 ){
    return 0;   /* The result set must have exactly one column */                 //结果集必须恰好只有一列
  }
  assert( pEList->a[0].pExpr );
  if( pEList->a[0].pExpr->op!=TK_ALL ){
    return 0;   /* The result set must be the special operator "*" */             //结果集一定有特别的操作符“*”
  }

  /* At this point we have established that the statement is of the               //此时我们建立的声明正确的语法形式
  ** correct syntactic form to participate in this optimization.  Now             //添加到这种最优，现在我们必须检查语义。
  ** we have to check the semantics.
  */
  pItem = pSelect->pSrc->a;
  pSrc = sqlite3LocateTable(pParse, 0, pItem->zName, pItem->zDatabase);
  if( pSrc==0 ){
    return 0;   /* FROM clause does not contain a real table */                  //from子句不含有真实的表
  }
  if( pSrc==pDest ){
    return 0;   /* tab1 and tab2 may not be the same table */                    //tab1和tab2可能不是相同的表
  }
#ifndef SQLITE_OMIT_VIRTUALTABLE
  if( pSrc->tabFlags & TF_Virtual ){
    return 0;   /* tab2 must not be a virtual table */                           //tab2一定不能是虚拟表
  }
#endif
  if( pSrc->pSelect ){
    return 0;   /* tab2 may not be a view */                                    //tab2可能不是视图
  }
  if( pDest->nCol!=pSrc->nCol ){
    return 0;   /* Number of columns must be the same in tab1 and tab2 */       //tab1和tab2的列的数目一定是相同的
  }
  if( pDest->iPKey!=pSrc->iPKey ){
    return 0;   /* Both tables must have the same INTEGER PRIMARY KEY */       //两张表一定有相同的INTEGER PRIMARY KEY（INTEGER主键）
  }
  for(i=0; i<pDest->nCol; i++){
    if( pDest->aCol[i].affinity!=pSrc->aCol[i].affinity ){
      return 0;    /* Affinity must be the same on all columns */              //类型，一定是相同在所有的列上
    }
    if( !xferCompatibleCollation(pDest->aCol[i].zColl, pSrc->aCol[i].zColl) ){
      return 0;    /* Collating sequence must be the same on all columns */   //核对列的序列一定是在所有列上是一致的
    }
    if( pDest->aCol[i].notNull && !pSrc->aCol[i].notNull ){
      return 0;    /* tab2 must be NOT NULL if tab1 is */                     //tab2一定是非空如果tab1是    
    }
  }
  for(pDestIdx=pDest->pIndex; pDestIdx; pDestIdx=pDestIdx->pNext){
    if( pDestIdx->onError!=OE_None ){
      destHasUniqueIdx = 1;
    }
    for(pSrcIdx=pSrc->pIndex; pSrcIdx; pSrcIdx=pSrcIdx->pNext){
      if( xferCompatibleIndex(pDestIdx, pSrcIdx) ) break;
    }
    if( pSrcIdx==0 ){
      return 0;    /* pDestIdx has no corresponding index in pSrc */        //目标目录没有相应的索引在源处
    }
  }
#ifndef SQLITE_OMIT_CHECK
  if( pDest->pCheck && sqlite3ExprListCompare(pSrc->pCheck, pDest->pCheck) ){
    return 0;   /* Tables have different CHECK constraints.  Ticket #2252 */  //表有不同的的检查约束，Ticket #2252
  }
#endif
#ifndef SQLITE_OMIT_FOREIGN_KEY
  /* Disallow the transfer optimization if the destination table constains        //不允许传递优化，如果目标表中含有外键约束。
  ** any foreign key constraints.  This is more restrictive than necessary.        //这是必要的限制
  ** But the main beneficiary of the transfer optimization is the VACUUM           //但是主要的转移优化的受益者是VACUUM指令，
  ** command, and the VACUUM command disables foreign key constraints.  So         //但是VACUUM指令不能够有外键约束。 
  ** the extra complication to make this rule less restrictive is probably        //所以额外的复杂使得这个规则少的限制可能不值得这样的花费
  ** not worth the effort.  Ticket [6284df89debdfa61db8073e062908af0c9b6118e]    //Ticket [6284df89debdfa61db8073e062908af0c9b6118e]
  */
  if( (pParse->db->flags & SQLITE_ForeignKeys)!=0 && pDest->pFKey!=0 ){
    return 0;
  }
#endif
  if( (pParse->db->flags & SQLITE_CountRows)!=0 ){
    return 0;  /* xfer opt does not play well with PRAGMA count_changes */       //xfer最优化不会表现好的 随着 PRAGMA count_changes
  }

  /* If we get this far, it means that the xfer optimization is at               //如果我们达到这个，这意味着xfer最优化至少是有可能的，
  ** least a possibility, though it might only work if the destination           //尽管可能生效如果目标表（tab1）最初是空。
  ** table (tab1) is initially empty.
  */
#ifdef SQLITE_TEST
  sqlite3_xferopt_count++;
#endif
  iDbSrc = sqlite3SchemaToIndex(pParse->db, pSrc->pSchema);
  v = sqlite3GetVdbe(pParse);
  sqlite3CodeVerifySchema(pParse, iDbSrc);
  iSrc = pParse->nTab++;
  iDest = pParse->nTab++;
  regAutoinc = autoIncBegin(pParse, iDbDest, pDest);
  sqlite3OpenTable(pParse, iDest, iDbDest, pDest, OP_OpenWrite);
  if( (pDest->iPKey<0 && pDest->pIndex!=0)          /* (1) */
   || destHasUniqueIdx                              /* (2) */
   || (onError!=OE_Abort && onError!=OE_Rollback)   /* (3) */
  ){
    /* In some circumstances, we are able to run the xfer optimization                     在某些环境，我们能够运行xfer最优化，只要目标表初始化为空即可，
    ** only if the destination table is initially empty.  This code makes                  这些编码作出决定.在条件是目标必须是空。      
    ** that determination.  Conditions under which the destination must
    ** be empty:
    **
    ** (1) There is no INTEGER PRIMARY KEY but there are indices.                          没有INTEGER主键但是有其他索引
    **     (If the destination is not initially empty, the rowid fields                    （如果目标初始化不是为空，行的ID字段的索引可能需要改变）
    **     of index entries might need to change.)
    **
    ** (2) The destination has a unique index.  (The xfer optimization                     目标有唯一索引（xfer最优化不能检测唯一性）
    **     is unable to test uniqueness.)
    **
    ** (3) onError is something other than OE_Abort and OE_Rollback.                       出错是有一些东西除了OE_Abort 和 OE_Rollback
    */
    addr1 = sqlite3VdbeAddOp2(v, OP_Rewind, iDest, 0);
    emptyDestTest = sqlite3VdbeAddOp2(v, OP_Goto, 0, 0);
    sqlite3VdbeJumpHere(v, addr1);
  }else{
    emptyDestTest = 0;
  }
  sqlite3OpenTable(pParse, iSrc, iDbSrc, pSrc, OP_OpenRead);
  emptySrcTest = sqlite3VdbeAddOp2(v, OP_Rewind, iSrc, 0);
  regData = sqlite3GetTempReg(pParse);
  regRowid = sqlite3GetTempReg(pParse);
  if( pDest->iPKey>=0 ){
    addr1 = sqlite3VdbeAddOp2(v, OP_Rowid, iSrc, regRowid);
    addr2 = sqlite3VdbeAddOp3(v, OP_NotExists, iDest, 0, regRowid);
    sqlite3HaltConstraint(
        pParse, onError, "PRIMARY KEY must be unique", P4_STATIC);
    sqlite3VdbeJumpHere(v, addr2);
    autoIncStep(pParse, regAutoinc, regRowid);
  }else if( pDest->pIndex==0 ){
    addr1 = sqlite3VdbeAddOp2(v, OP_NewRowid, iDest, regRowid);
  }else{
    addr1 = sqlite3VdbeAddOp2(v, OP_Rowid, iSrc, regRowid);
    assert( (pDest->tabFlags & TF_Autoincrement)==0 );
  }
  sqlite3VdbeAddOp2(v, OP_RowData, iSrc, regData);
  sqlite3VdbeAddOp3(v, OP_Insert, iDest, regData, regRowid);
  sqlite3VdbeChangeP5(v, OPFLAG_NCHANGE|OPFLAG_LASTROWID|OPFLAG_APPEND);
  sqlite3VdbeChangeP4(v, -1, pDest->zName, 0);
  sqlite3VdbeAddOp2(v, OP_Next, iSrc, addr1);
  for(pDestIdx=pDest->pIndex; pDestIdx; pDestIdx=pDestIdx->pNext){
    for(pSrcIdx=pSrc->pIndex; ALWAYS(pSrcIdx); pSrcIdx=pSrcIdx->pNext){
      if( xferCompatibleIndex(pDestIdx, pSrcIdx) ) break;
    }
    assert( pSrcIdx );
    sqlite3VdbeAddOp2(v, OP_Close, iSrc, 0);
    sqlite3VdbeAddOp2(v, OP_Close, iDest, 0);
    pKey = sqlite3IndexKeyinfo(pParse, pSrcIdx);
    sqlite3VdbeAddOp4(v, OP_OpenRead, iSrc, pSrcIdx->tnum, iDbSrc,
                      (char*)pKey, P4_KEYINFO_HANDOFF);
    VdbeComment((v, "%s", pSrcIdx->zName));
    pKey = sqlite3IndexKeyinfo(pParse, pDestIdx);
    sqlite3VdbeAddOp4(v, OP_OpenWrite, iDest, pDestIdx->tnum, iDbDest,
                      (char*)pKey, P4_KEYINFO_HANDOFF);
    VdbeComment((v, "%s", pDestIdx->zName));
    addr1 = sqlite3VdbeAddOp2(v, OP_Rewind, iSrc, 0);
    sqlite3VdbeAddOp2(v, OP_RowKey, iSrc, regData);
    sqlite3VdbeAddOp3(v, OP_IdxInsert, iDest, regData, 1);
    sqlite3VdbeAddOp2(v, OP_Next, iSrc, addr1+1);
    sqlite3VdbeJumpHere(v, addr1);
  }
  sqlite3VdbeJumpHere(v, emptySrcTest);
  sqlite3ReleaseTempReg(pParse, regRowid);
  sqlite3ReleaseTempReg(pParse, regData);
  sqlite3VdbeAddOp2(v, OP_Close, iSrc, 0);
  sqlite3VdbeAddOp2(v, OP_Close, iDest, 0);
  if( emptyDestTest ){
    sqlite3VdbeAddOp2(v, OP_Halt, SQLITE_OK, 0);
    sqlite3VdbeJumpHere(v, emptyDestTest);
    sqlite3VdbeAddOp2(v, OP_Close, iDest, 0);
    return 0;
  }else{
    return 1;
  }
}
#endif /* SQLITE_OMIT_XFER_OPT */
