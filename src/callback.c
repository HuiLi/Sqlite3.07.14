/*
** 2005 May 23 
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:    作者声明这个源代码版权。代替一个法律声明，这里是一个祝福：
**
**    May you do good and not evil.愿你做的是好事，而不是邪恶。也许你会发现宽恕自己和宽容别人。你可以自由分享，不要获取多余给予。
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
**
** This file contains functions used to access the internal hash tables
** of user defined functions and collation sequences.该文件包含用于访问用户定义的函数和排序队列的内部哈希表的功能。
*/

#include "sqliteInt.h"

/*
** Invoke the 'collation needed' callback to request a collation sequence
** in the encoding enc of name zName, length nName.引用“collation needed'回调至zName，长度nNAME，编码ENC要求归类队列。
*/
static void callCollNeeded(sqlite3 *db, int enc, const char *zName){
  assert( !db->xCollNeeded || !db->xCollNeeded16 );
  if( db->xCollNeeded ){
    char *zExternal = sqlite3DbStrDup(db, zName);
    if( !zExternal ) return;
    db->xCollNeeded(db->pCollNeededArg, db, enc, zExternal);
    sqlite3DbFree(db, zExternal);
  }
#ifndef SQLITE_OMIT_UTF16
  if( db->xCollNeeded16 ){
    char const *zExternal;
    sqlite3_value *pTmp = sqlite3ValueNew(db);
    sqlite3ValueSetStr(pTmp, -1, zName, SQLITE_UTF8, SQLITE_STATIC);
    zExternal = sqlite3ValueText(pTmp, SQLITE_UTF16NATIVE);
    if( zExternal ){
      db->xCollNeeded16(db->pCollNeededArg, db, (int)ENC(db), zExternal);
    }
    sqlite3ValueFree(pTmp);
  }
#endif
}

/*
** This routine is called if the collation factory fails to deliver a
** collation function in the best encoding but there may be other versions
** of this collation function (for other text encodings) available. Use one
** of these instead if they exist. Avoid a UTF-8 <-> UTF-16 conversion if
** possible.如果回收未能提供一个排序函数中最好的编码，但也有可能提供其他版本的这个排序函数的（其他文本编码）供这个程序被调用。如果它们存在使用其中的一个来代替。如果可能的话避免UTF-8<- > UTF-16转换。
*/
static int synthCollSeq(sqlite3 *db, CollSeq *pColl){
  CollSeq *pColl2;
  char *z = pColl->zName;
  int i;
  static const u8 aEnc[] = { SQLITE_UTF16BE, SQLITE_UTF16LE, SQLITE_UTF8 };
  for(i=0; i<3; i++){
    pColl2 = sqlite3FindCollSeq(db, aEnc[i], z, 0);
    if( pColl2->xCmp!=0 ){
      memcpy(pColl, pColl2, sizeof(CollSeq));
      pColl->xDel = 0;         /* Do not copy the destructor */
      return SQLITE_OK;
    }
  }
  return SQLITE_ERROR;
}

/*
** This function is responsible for invoking the collation factory callback
** or substituting a collation sequence of a different encoding when the
** requested collation sequence is not available in the desired encoding.这个功能是负责调用回收工厂回调或代以一个不同的编码的排列顺序时所需的排序顺序并不在所需的编码时可用。
** 
** If it is not NULL, then pColl must point to the database native encoding 
** collation sequence with name zName, length nName.如果不是NULL，则pColl必须指向本地数据库，通过对zName，长度nNAME归类序列。
**
** The return value is either the collation sequence to be used in database
** db for collation type name zName, length nName, or NULL, if no collation
** sequence can be found.返回值是被用来在数据库DB整理类型名称zName，长度nName，或NULL，如果没有排序顺序，可以发现归类序列。
**
** See also: sqlite3LocateCollSeq(), sqlite3FindCollSeq()
*/
CollSeq *sqlite3GetCollSeq(
  sqlite3* db,          /* The database connection 数据库连接*/
  u8 enc,               /* The desired encoding for the collating sequence 为整理顺序所需的编码*/
  CollSeq *pColl,       /* Collating sequence with native encoding, or NULL 与本地编码，或NULL排序序列*/
  const char *zName     /* Collating sequence name 排序序列名称*/
){
  CollSeq *p;

  p = pColl;
  if( !p ){
    p = sqlite3FindCollSeq(db, enc, zName, 0);
  }
  if( !p || !p->xCmp ){
    /* No collation sequence of this type for this encoding is registered.任何归类于这种类型这种编码序列被注册。
    ** Call the collation factory to see if it can supply us with one.调用回收工厂，看它是否能够提供给我们。
    */
    callCollNeeded(db, enc, zName);
    p = sqlite3FindCollSeq(db, enc, zName, 0);
  }
  if( p && !p->xCmp && synthCollSeq(db, p) ){
    p = 0;
  }
  assert( !p || p->xCmp );
  return p;
}

/*
** This routine is called on a collation sequence before it is used to
** check that it is defined. An undefined collation sequence exists when
** a database is loaded that contains references to collation sequences
** that have not been defined by sqlite3_create_collation() etc.这个程序被调用的排列顺序前先用它来检查它的定义。当数据库被加载，其中包含对那些尚未被sqlite3_create_collation（）定义等，整理序列未定义的排序顺序。
**
** If required, this routine calls the 'collation needed' callback to如果需要的话，这个程序将调用“collation needed'，回调要求整理顺序的定义。
** request a definition of the collating sequence. If this doesn't work, 
** an equivalent collating sequence that uses a text encoding different
** from the main database is substituted, if one is available.如果这不行，则使用文本编码，如果可用的话从主数据库不同的等效排序序列替换。
*/
int sqlite3CheckCollSeq(Parse *pParse, CollSeq *pColl){
  if( pColl ){
    const char *zName = pColl->zName;
    sqlite3 *db = pParse->db;
    CollSeq *p = sqlite3GetCollSeq(db, ENC(db), pColl, zName);
    if( !p ){
      sqlite3ErrorMsg(pParse, "no such collation sequence: %s", zName);
      pParse->nErr++;
      return SQLITE_ERROR;
    }
    assert( p==pColl );
  }
  return SQLITE_OK;
}



/*
** Locate and return an entry from the db.aCollSeq hash table. If the entry
** specified by zName and nName is not found and parameter 'create' is
** true, then create a new entry. Otherwise return NULL.找到并返回从db.aCollSeq哈希表中的条目。如果该条目没有发现zName和nNAME规定和参数“创造”是真的，然后创建一个新的条目。否则返回NULL。
**
** Each pointer stored in the sqlite3.aCollSeq hash table contains an
** array of three CollSeq structures. The first is the collation sequence
** prefferred for UTF-8, the second UTF-16le, and the third UTF-16be.存储在sqlite3.aCollSeq哈希表中的每个指针包含一个排列三CollSeq结构。首先是排序顺序为UTF-8，第二UTF-16LE，第三UTF-16BE。
**
** Stored immediately after the three collation sequences is a copy of
** the collation sequence name. A pointer to this string is stored in
** each collation sequence structure.三个归类序列后，立即存储是归类序列名称的副本。一个指向该字符串存储在每个排列顺序结构。
*/
static CollSeq *findCollSeqEntry(
  sqlite3 *db,          /* Database connection 数据库连接*/
  const char *zName,    /* Name of the collating sequence 整理顺序名称*/
  int create            /* Create a new entry if true 建立一个入口*/
){
  CollSeq *pColl;
  int nName = sqlite3Strlen30(zName);
  pColl = sqlite3HashFind(&db->aCollSeq, zName, nName);

  if( 0==pColl && create ){
    pColl = sqlite3DbMallocZero(db, 3*sizeof(*pColl) + nName + 1 );
    if( pColl ){
      CollSeq *pDel = 0;
      pColl[0].zName = (char*)&pColl[3];
      pColl[0].enc = SQLITE_UTF8;
      pColl[1].zName = (char*)&pColl[3];
      pColl[1].enc = SQLITE_UTF16LE;
      pColl[2].zName = (char*)&pColl[3];
      pColl[2].enc = SQLITE_UTF16BE;
      memcpy(pColl[0].zName, zName, nName);
      pColl[0].zName[nName] = 0;
      pDel = sqlite3HashInsert(&db->aCollSeq, pColl[0].zName, nName, pColl);

      /* If a malloc() failure occurred in sqlite3HashInsert(), it will 
      ** return the pColl pointer to be deleted (because it wasn't added
      ** to the hash table).如果一个malloc（）故障发生在sqlite3HashInsert（），它将返回pColl指针被删除（因为它不添加到哈希表）。
      */
      assert( pDel==0 || pDel==pColl );
      if( pDel!=0 ){
        db->mallocFailed = 1;
        sqlite3DbFree(db, pDel);
        pColl = 0;
      }
    }
  }
  return pColl;
}

/*
** Parameter zName points to a UTF-8 encoded string nName bytes long.
** Return the CollSeq* pointer for the collation sequence named zName
** for the encoding 'enc' from the database 'db'.参数zName指向一个UTF-8编码字符串n名称字节长。返回CollSeq*指针命名zName从数据库“DB”编码“ENC”的排列顺序。
**
** If the entry specified is not found and 'create' is true, then create a
** new entry.  Otherwise return NULL.如果没有找到指定的条目和'CREAT'是真的，然后创建一个新的条目。否则返回NULL。
**
** A separate function sqlite3LocateCollSeq() is a wrapper around
** this routine.  sqlite3LocateCollSeq() invokes the collation factory
** if necessary and generates an error message if the collating sequence
** cannot be found.一个单独的功能sqlite3LocateCollSeq（）是围绕这个例程的。如果排序序列无法找到，sqlite3LocateCollSeq（）调用整理工厂并在必要时产生的错误信息。
**
** See also: sqlite3LocateCollSeq(), sqlite3GetCollSeq()另请参见：sqlite3LocateCollSeq（），sqlite3GetCollSeq（）
*/
CollSeq *sqlite3FindCollSeq(
  sqlite3 *db,
  u8 enc,
  const char *zName,
  int create
){
  CollSeq *pColl;
  if( zName ){
    pColl = findCollSeqEntry(db, zName, create);
  }else{
    pColl = db->pDfltColl;
  }
  assert( SQLITE_UTF8==1 && SQLITE_UTF16LE==2 && SQLITE_UTF16BE==3 );
  assert( enc>=SQLITE_UTF8 && enc<=SQLITE_UTF16BE );
  if( pColl ) pColl += enc-1;
  return pColl;
}

/* During the search for the best function definition, this procedure
** is called to test how well the function passed as the first argument
** matches the request for a function with nArg arguments in a system
** that uses encoding enc. The value returned indicates how well the
** request is matched. A higher value indicates a better match.在寻找最佳函数定义，这个过程被称为测试，以及如何传递的第一个参数的功能，在使用编码ENC系统nArg参数的函数时要求相匹配。返回的值表示良好的请求相匹配。较高的值表示更好的匹配。
**
** If nArg is -1 that means to only return a match (non-zero) if p->nArg
** is also -1.  In other words, we are searching for a function that
** takes a variable number of arguments.如果nArg是-1，这意味着只返回匹配（非零），如对 - > nArg也是-1。换句话说，我们正在寻找一个函数，采用可变数量的参数。
**
** If nArg is -2 that means that we are searching for any function 
** regardless of the number of arguments it uses, so return a positive
** match score for any如果nArg是-2意味着我们正在寻找任何功能无关的参数，它使用的数量，以便返回一个正匹配的记录
**
** The returned value is always between 0 and 6, as follows:返回的值总是在0和6，如下：
**
** 0: Not a match.不匹配 
** 1: UTF8/16 conversion required and function takes any number of arguments.
** 2: UTF16 byte order change required and function takes any number of args.
** 3: encoding matches and function takes any number of arguments
** 4: UTF8/16 conversion required - argument count matches exactly
** 5: UTF16 byte order conversion required - argument count matches exactly
** 6: Perfect match:  encoding and argument count match exactly.1：UTF8/16的转换要求和功能需要的任何数量的参数。2：需要UTF16字节顺序的变化和功能的需要任意数量的args。3：编码匹配和函数接受任何数目的参数4：UTF8/16的转换需要- 参数数完全匹配5：UTF16字节顺序转换所需- 参数数完全匹配6：完美匹配：编码和参数个数完全匹配。
**
** If nArg==(-2) then any function with a non-null xStep or xFunc is
** a perfect match and any function with both xStep and xFunc NULL is
** a non-match.f nArg==（- 2），然后用任何一个非空XSTEP功能或xFunc是一个很好的匹配，并与两个XSTEP和xFunc NULL任何函数是一个不匹配。
*/
*/
#define FUNC_PERFECT_MATCH 6  /* The score for a perfect match 记录很匹配*/
static int matchQuality(
  FuncDef *p,     /* The function we are evaluating for match quality 我们正在评估匹配质量的功能*/
  int nArg,       /* Desired number of arguments.  (-1)==any */
  u8 enc          /* Desired text encoding 期望中的文本编码*/
){
  int match;

  /* nArg of -2 is a special case */
  if( nArg==(-2) ) return (p->xFunc==0 && p->xStep==0) ? 0 : FUNC_PERFECT_MATCH;

  /* Wrong number of arguments means "no match" 参数错误的意思是“不匹配”*/
  if( p->nArg!=nArg && p->nArg>=0 ) return 0;

  /* Give a better score to a function with a specific number of arguments
  ** than to function that accepts any number of arguments. 举一个更好的记录，功能与函数参数的具体数量，接受任意记录的参数。如果文本编码匹配*/
  if( p->nArg==nArg ){
    match = 4;
  }else{
    match = 1;
  }

  /* Bonus points if the text encoding matches 如果文本编码匹配*/
  if( enc==p->iPrefEnc ){
    match += 2;  /* Exact encoding match 确切的编码匹配*/
  }else if( (enc & p->iPrefEnc & 2)!=0 ){
    match += 1;  /* Both are UTF16, but with different byte orders 两者都是UTF16，但不同的字节顺序*/
  }

  return match;
}

/*
** Search a FuncDefHash for a function with the given name.  Return
** a pointer to the matching FuncDef if found, or 0 if there is no match.搜索FuncDefHash与给定名称的功能。如果不存在匹配，返回指针到匹配FuncDef如果找到，或0。 
*/
static FuncDef *functionSearch(
  FuncDefHash *pHash,  /* Hash table to search 哈希表搜索*/
  int h,               /* Hash of the name 名称的哈西表*/
  const char *zFunc,   /* Name of function 功能的名字*/
  int nFunc            /* Number of bytes in zFunc zFunc字节数*/
){
  FuncDef *p;
  for(p=pHash->a[h]; p; p=p->pHash){
    if( sqlite3StrNICmp(p->zName, zFunc, nFunc)==0 && p->zName[nFunc]==0 ){
      return p;
    }
  }
  return 0;
}

/*
** Insert a new FuncDef into a FuncDefHash hash table.插入一个新的FuncDef成FuncDefHash哈希表
*/
void sqlite3FuncDefInsert(
  FuncDefHash *pHash,  /* The hash table into which to insert 哈希表插入*/
  FuncDef *pDef        /* The function definition to insert 该函数定义插入*/
){
  FuncDef *pOther;
  int nName = sqlite3Strlen30(pDef->zName);
  u8 c1 = (u8)pDef->zName[0];
  int h = (sqlite3UpperToLower[c1] + nName) % ArraySize(pHash->a);
  pOther = functionSearch(pHash, h, pDef->zName, nName);
  if( pOther ){
    assert( pOther!=pDef && pOther->pNext!=pDef );
    pDef->pNext = pOther->pNext;
    pOther->pNext = pDef;
  }else{
    pDef->pNext = 0;
    pDef->pHash = pHash->a[h];
    pHash->a[h] = pDef;
  }
}
  
  

/*
** Locate a user function given a name, a number of arguments and a flag
** indicating whether the function prefers UTF-16 over UTF-8.  Return a
** pointer to the FuncDef structure that defines that function, or return
** NULL if the function does not exist.找到一个名字用户的功能，一些参数和标志，指示功能是否倾向UTF-16在UTF-8。如果功能不存在，返回一个指针FuncDef结构定义功能，或返回NULL。
**
** If the createFlag argument is true, then a new (blank) FuncDef
** structure is created and liked into the "db" structure if a
** no matching function previously existed. 如果createFlag是真的，那么一个新的（空白）FuncDef结构创建，像到“DB”的结构，如果以前就存在一个不匹配功能。
**
** If nArg is -2, then the first valid function found is returned.  A
** function is valid if either xFunc or xStep is non-zero.  The nArg==(-2)
** case is used to see if zName is a valid function name for some number
** of arguments.  If nArg is -2, then createFlag must be 0.如果nArg是-2，那么找到的第一个有效的函数返回。函数是有效的，如果任一xFunc或XSTEP是非零。nArg==（- 2）的情况下被使用，看是否zName是参数一定数量有效的函数名称。如果nArg为-2，则createFlag必须为0。
**
** If createFlag is false, then a function with the required name and
** number of arguments may be returned even if the eTextRep flag does not
** match that requested.    如果createFlag是假，用参数的所需名称和编号的函数可以返回，即使eTextRep标志不匹配请求。
*/
FuncDef *sqlite3FindFunction(
  sqlite3 *db,       /* An open database 一个开放数据库*/
  const char *zName, /* Name of the function.  Not null-terminated Not null-terminated 功能名，不是空终止*/
  int nName,         /* Number of characters in the name 名称中的字符数*/
  int nArg,          /* Number of arguments.  -1 means any number 数量的参数。 -1意味着任何数字*/
  u8 enc,            /* Preferred text encoding 偏好文本编码*/
  u8 createFlag      /* Create new entry if true and does not otherwise exist */
){
  FuncDef *p;         /* Iterator variable 迭代变量*/
  FuncDef *pBest = 0; /* Best match found so far 迄今发现最佳匹配*/
  int bestScore = 0;  /* Score of best match 最佳匹配*/
  int h;              /* Hash value 哈希值*/

  assert( nArg>=(-2) );
  assert( nArg>=(-1) || createFlag==0 );
  assert( enc==SQLITE_UTF8 || enc==SQLITE_UTF16LE || enc==SQLITE_UTF16BE );
  h = (sqlite3UpperToLower[(u8)zName[0]] + nName) % ArraySize(db->aFunc.a);

  /* First search for a match amongst the application-defined functions.首先搜索匹配之的应用程序定义的功能。
  */
  p = functionSearch(&db->aFunc, h, zName, nName);
  while( p ){
    int score = matchQuality(p, nArg, enc);
    if( score>bestScore ){
      pBest = p;
      bestScore = score;
    }
    p = p->pNext;
  }

  /* If no match is found, search the built-in functions.如果没有找到匹配，搜索内置函数。
  **
  ** If the SQLITE_PreferBuiltin flag is set, then search the built-in
  ** functions even if a prior app-defined function was found.  And give
  ** priority to built-in functions.如果SQLITE_PreferBuiltin标志被设置，则搜索所述内置函数,即使现有的应用程序定义的函数被发现。并内置函数优先。
  **
  ** Except, if createFlag is true, that means that we are trying to但是，如果createFlag是真实的，这意味着我们正在试图安装一个新的功能。返回FuncDef结构将有覆盖新的信息，适用于新的功能领域。但是FuncDefs为内置函数是只读。因此，创建一个新的功能时，我们必须去寻找内置插件。
  ** install a new function.  Whatever FuncDef structure is returned it will
  ** have fields overwritten with new information appropriate for the
  ** new function.  But the FuncDefs for built-in functions are read-only.
  ** So we must not search for built-ins when creating a new function.
  */ 
  if( !createFlag && (pBest==0 || (db->flags & SQLITE_PreferBuiltin)!=0) ){
    FuncDefHash *pHash = &GLOBAL(FuncDefHash, sqlite3GlobalFunctions);
    bestScore = 0;
    p = functionSearch(pHash, h, zName, nName);
    while( p ){
      int score = matchQuality(p, nArg, enc);
      if( score>bestScore ){
        pBest = p;
        bestScore = score;
      }
      p = p->pNext;
    }
  }

  /* If the createFlag parameter is true and the search did not reveal an
  ** exact match for the name, number of arguments and encoding, then add a
  ** new entry to the hash table and return it.如果createFlag参数为true，搜索没有透露姓名的精确匹配，参数和编码号，那么添加一个新条目哈希表并将其返回。
  */
  if( createFlag && bestScore<FUNC_PERFECT_MATCH && 
      (pBest = sqlite3DbMallocZero(db, sizeof(*pBest)+nName+1))!=0 ){
    pBest->zName = (char *)&pBest[1];
    pBest->nArg = (u16)nArg;
    pBest->iPrefEnc = enc;
    memcpy(pBest->zName, zName, nName);
    pBest->zName[nName] = 0;
    sqlite3FuncDefInsert(&db->aFunc, pBest);
  }

  if( pBest && (pBest->xStep || pBest->xFunc || createFlag) ){
    return pBest;
  }
  return 0;
}

/*
** Free all resources held by the schema structure. The void* argument points
** at a Schema struct. This function does not call sqlite3DbFree(db, ) on the 
** pointer itself, it just cleans up subsidiary resources (i.e. the contents
** of the schema hash tables).免费的模式结构有的所有资源。void*参数指向在模式结构。此功能不会调用sqlite3DbFree（db，）上的指针本身，它只是清理子的资源（即架构哈希表中的内容）。
**
** The Schema.cache_size variable is not cleared.该Schema.cache_size变量没有被清除。
*/
void sqlite3SchemaClear(void *p){
  Hash temp1;
  Hash temp2;
  HashElem *pElem;
  Schema *pSchema = (Schema *)p;

  temp1 = pSchema->tblHash;
  temp2 = pSchema->trigHash;
  sqlite3HashInit(&pSchema->trigHash);
  sqlite3HashClear(&pSchema->idxHash);
  for(pElem=sqliteHashFirst(&temp2); pElem; pElem=sqliteHashNext(pElem)){
    sqlite3DeleteTrigger(0, (Trigger*)sqliteHashData(pElem));
  }
  sqlite3HashClear(&temp2);
  sqlite3HashInit(&pSchema->tblHash);
  for(pElem=sqliteHashFirst(&temp1); pElem; pElem=sqliteHashNext(pElem)){
    Table *pTab = sqliteHashData(pElem);
    sqlite3DeleteTable(0, pTab);
  }
  sqlite3HashClear(&temp1);
  sqlite3HashClear(&pSchema->fkeyHash);
  pSchema->pSeqTab = 0;
  if( pSchema->flags & DB_SchemaLoaded ){
    pSchema->iGeneration++;
    pSchema->flags &= ~DB_SchemaLoaded;
  }
}

/*
** Find and return the schema associated with a BTree.  Create
** a new one if necessary.查找并返回一个B树关联的模式。如果必要的，创建一个新的
*/
Schema *sqlite3SchemaGet(sqlite3 *db, Btree *pBt){
  Schema * p;
  if( pBt ){
    p = (Schema *)sqlite3BtreeSchema(pBt, sizeof(Schema), sqlite3SchemaClear);
  }else{
    p = (Schema *)sqlite3DbMallocZero(0, sizeof(Schema));
  }
  if( !p ){
    db->mallocFailed = 1;
  }else if ( 0==p->file_format ){
    sqlite3HashInit(&p->tblHash);
    sqlite3HashInit(&p->idxHash);
    sqlite3HashInit(&p->trigHash);
    sqlite3HashInit(&p->fkeyHash);
    p->enc = SQLITE_UTF8;
  }
  return p;
}
