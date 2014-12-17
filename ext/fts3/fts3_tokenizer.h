/*
** 2007 June 22
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
** This is part of an SQLite module implementing full-text search.
** This particular file implements the generic tokenizer interface.
*/
/*这是SQLite模块的一部分执行全文搜索。
这个特殊的文件实现了通用的分词器接口。*/

/*
** The code in this file is only compiled if:
**
**     * The FTS3 module is being built as an extension
**       (in which case SQLITE_CORE is not defined), or
**
**     * The FTS3 module is being built into the core of
**       SQLite (in which case SQLITE_ENABLE_FTS3 is defined).
*/
/*    该文件中的代码在下列条件下才编译:

    * FTS3模块正在建设成为一个扩展
     (在这种情况下SQLITE_CORE没有定义),或

    * FTS3模块正在建设成SQLite的核心
      (在这种情况下SQLITE_ENABLE_FTS3定义)。
*/
#include "fts3Int.h"
#if !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_FTS3)

#include <assert.h>
#include <string.h>

/*
** Implementation of the SQL scalar function for accessing the underlying 
** hash table. This function may be called as follows:
**
**   SELECT <function-name>(<key-name>);
**   SELECT <function-name>(<key-name>, <pointer>);
**
** where <function-name> is the name passed as the second argument
** to the sqlite3Fts3InitHashTable() function (e.g. 'fts3_tokenizer').
**
** If the <pointer> argument is specified, it must be a blob value
** containing a pointer to be stored as the hash data corresponding
** to the string <key-name>. If <pointer> is not specified, then
** the string <key-name> must already exist in the has table. Otherwise,
** an error is returned.
**
** Whether or not the <pointer> argument is specified, the value returned
** is a blob containing the pointer stored as the hash data corresponding
** to string <key-name> (after the hash-table is updated, if applicable).
*/

/* 执行SQL标量函数来访问底层的哈希表。这个函数可以表示为如下形式:
   
     SELECT <function-name>(<key-name>);
     SELECT <function-name>(<key-name>, <pointer>);

   把<function-name>的名称作为第二个参数传递给sqlite3Fts3InitHashTable()函数(如。“fts3_tokenizer”)。
   如果指定<pointer>参数,它必须是一个blob值，并且包含一个被存储为散列数据对应字符串<key-name>的指针。
   如果没有指定<pointer>,那么字符串<key-name>必须存在于已有表中。否则,返回一个错误。

   是否指定<指针>参数,返回值是一个blob值，其包含被存储为散列数据对应字符串<key-name>的指针(如果适用的话,在哈希表被更新后)。
   
*/

static void scalarFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  Fts3Hash *pHash;
  void *pPtr = 0;
  const unsigned char *zName;
  int nName;

  assert( argc==1 || argc==2 );

  pHash = (Fts3Hash *)sqlite3_user_data(context);

  zName = sqlite3_value_text(argv[0]);
  nName = sqlite3_value_bytes(argv[0])+1;

  if( argc==2 ){
    void *pOld;
    int n = sqlite3_value_bytes(argv[1]);
    if( n!=sizeof(pPtr) ){
      sqlite3_result_error(context, "argument type mismatch", -1);
      return;
    }
    pPtr = *(void **)sqlite3_value_blob(argv[1]);
    pOld = sqlite3Fts3HashInsert(pHash, (void *)zName, nName, pPtr);
    if( pOld==pPtr ){
      sqlite3_result_error(context, "out of memory", -1);
      return;
    }
  }else{
    pPtr = sqlite3Fts3HashFind(pHash, zName, nName);
    if( !pPtr ){
      char *zErr = sqlite3_mprintf("unknown tokenizer: %s", zName);
      sqlite3_result_error(context, zErr, -1);
      sqlite3_free(zErr);
      return;
    }
  }

  sqlite3_result_blob(context, (void *)&pPtr, sizeof(pPtr), SQLITE_TRANSIENT);
}

int sqlite3Fts3IsIdChar(char c){
  static const char isFtsIdChar[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x */
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 1x */
      0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 2x */
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,  /* 3x */
      0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 4x */
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,  /* 5x */
      0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 6x */
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,  /* 7x */
  };
  return (c&0x80 || isFtsIdChar[(int)(c)]);
}

const char *sqlite3Fts3NextToken(const char *zStr, int *pn){
  const char *z1;
  const char *z2 = 0;

  /* Find the start of the next token. */       /* 寻找下一个符号的开端 */
  z1 = zStr;
  while( z2==0 ){
    char c = *z1;
    switch( c ){
      case '\0': return 0;        /* No more tokens here */              /* 这里没有更多的符号 */
      case '\'':
      case '"':
      case '`': {
        z2 = z1;
        while( *++z2 && (*z2!=c || *++z2==c) );
        break;
      }
      case '[':
        z2 = &z1[1];
        while( *z2 && z2[0]!=']' ) z2++;
        if( *z2 ) z2++;
        break;

      default:
        if( sqlite3Fts3IsIdChar(*z1) ){
          z2 = &z1[1];
          while( sqlite3Fts3IsIdChar(*z2) ) z2++;
        }else{
          z1++;
        }
    }
  }

  *pn = (int)(z2-z1);
  return z1;
}

int sqlite3Fts3InitTokenizer(
  Fts3Hash *pHash,                /* Tokenizer hash table */                       /* 分词器哈希表 */
  const char *zArg,               /* Tokenizer name */                             /* 分词器名 */
  sqlite3_tokenizer **ppTok,      /* OUT: Tokenizer (if applicable) */             /* 输出：分词器（如果可以） */
  char **pzErr                    /* OUT: Set to malloced error message */         /* 输出：设置为malloc错误消息 */
){
  int rc;
  char *z = (char *)zArg;
  int n = 0;
  char *zCopy;
  char *zEnd;                     /* Pointer to nul-term of zCopy */               /* 指向zCopy中的空项指针 */
  sqlite3_tokenizer_module *m;

  zCopy = sqlite3_mprintf("%s", zArg);
  if( !zCopy ) return SQLITE_NOMEM;
  zEnd = &zCopy[strlen(zCopy)];

  z = (char *)sqlite3Fts3NextToken(zCopy, &n);
  z[n] = '\0';
  sqlite3Fts3Dequote(z);

  m = (sqlite3_tokenizer_module *)sqlite3Fts3HashFind(pHash,z,(int)strlen(z)+1);
  if( !m ){
    *pzErr = sqlite3_mprintf("unknown tokenizer: %s", z);
    rc = SQLITE_ERROR;
  }else{
    char const **aArg = 0;
    int iArg = 0;
    z = &z[n+1];
    while( z<zEnd && (NULL!=(z = (char *)sqlite3Fts3NextToken(z, &n))) ){
      int nNew = sizeof(char *)*(iArg+1);
      char const **aNew = (const char **)sqlite3_realloc((void *)aArg, nNew);
      if( !aNew ){
        sqlite3_free(zCopy);
        sqlite3_free((void *)aArg);
        return SQLITE_NOMEM;
      }
      aArg = aNew;
      aArg[iArg++] = z;
      z[n] = '\0';
      sqlite3Fts3Dequote(z);
      z = &z[n+1];
    }
    rc = m->xCreate(iArg, aArg, ppTok);
    assert( rc!=SQLITE_OK || *ppTok );
    if( rc!=SQLITE_OK ){
      *pzErr = sqlite3_mprintf("unknown tokenizer");
    }else{
      (*ppTok)->pModule = m; 
    }
    sqlite3_free((void *)aArg);
  }

  sqlite3_free(zCopy);
  return rc;
}


#ifdef SQLITE_TEST

#include <tcl.h>
#include <string.h>

/*
** Implementation of a special SQL scalar function for testing tokenizers 
** designed to be used in concert with the Tcl testing framework. This
** function must be called with two or more arguments:
**
**   SELECT <function-name>(<key-name>, ..., <input-string>);
**
** where <function-name> is the name passed as the second argument
** to the sqlite3Fts3InitHashTable() function (e.g. 'fts3_tokenizer')
** concatenated with the string '_test' (e.g. 'fts3_tokenizer_test').
**
** The return value is a string that may be interpreted as a Tcl
** list. For each token in the <input-string>, three elements are
** added to the returned list. The first is the token position, the 
** second is the token text (folded, stemmed, etc.) and the third is the
** substring of <input-string> associated with the token. For example, 
** using the built-in "simple" tokenizer:
**
**   SELECT fts_tokenizer_test('simple', 'I don't see how');
**
** will return the string:
**
**   "{0 i I 1 dont don't 2 see see 3 how how}"
**   
*/
/*  实现特殊的SQL标量函数的测试分词器设计使用和Tcl测试框架一致。调用这
    个函数必须使用两个或两个以上的参数:
	    SELECT <function-name>(<key-name>, ..., <input-string>);
    在<函数名>的名称作为第二个参数传递给sqlite3Fts3InitHashTable()函数
	(如。“fts3_tokenizer”)连接的字符串“_test”(如。“fts3_tokenizer_test”)。

    返回值是一个可以被看作为Tcl列表的字符串。对于<input-string>为中的每一个词汇,三个
	元素被添加到返回的列表中。第一个是词汇的位置,第二个是象征性的文本(折叠、遏制等。),
	第三是和词汇有关系的<input-string>子串。例如,使用内置的“简单”分词器:
	  SELECT fts_tokenizer_test('simple', 'I don't see how');
	将返回如下字符串:
	    "{0 i I 1 dont don't 2 see see 3 how how}"
*/
static void testFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  Fts3Hash *pHash;
  sqlite3_tokenizer_module *p;
  sqlite3_tokenizer *pTokenizer = 0;
  sqlite3_tokenizer_cursor *pCsr = 0;

  const char *zErr = 0;

  const char *zName;
  int nName;
  const char *zInput;
  int nInput;

  const char *azArg[64];

  const char *zToken;
  int nToken;
  int iStart;
  int iEnd;
  int iPos;
  int i;

  Tcl_Obj *pRet;

  if( argc<2 ){
    sqlite3_result_error(context, "insufficient arguments", -1);
    return;
  }

  nName = sqlite3_value_bytes(argv[0]);
  zName = (const char *)sqlite3_value_text(argv[0]);
  nInput = sqlite3_value_bytes(argv[argc-1]);
  zInput = (const char *)sqlite3_value_text(argv[argc-1]);

  pHash = (Fts3Hash *)sqlite3_user_data(context);
  p = (sqlite3_tokenizer_module *)sqlite3Fts3HashFind(pHash, zName, nName+1);

  if( !p ){
    char *zErr = sqlite3_mprintf("unknown tokenizer: %s", zName);
    sqlite3_result_error(context, zErr, -1);
    sqlite3_free(zErr);
    return;
  }

  pRet = Tcl_NewObj();
  Tcl_IncrRefCount(pRet);

  for(i=1; i<argc-1; i++){
    azArg[i-1] = (const char *)sqlite3_value_text(argv[i]);
  }

  if( SQLITE_OK!=p->xCreate(argc-2, azArg, &pTokenizer) ){
    zErr = "error in xCreate()";
    goto finish;
  }
  pTokenizer->pModule = p;
  if( sqlite3Fts3OpenTokenizer(pTokenizer, 0, zInput, nInput, &pCsr) ){
    zErr = "error in xOpen()";
    goto finish;
  }

  while( SQLITE_OK==p->xNext(pCsr, &zToken, &nToken, &iStart, &iEnd, &iPos) ){
    Tcl_ListObjAppendElement(0, pRet, Tcl_NewIntObj(iPos));
    Tcl_ListObjAppendElement(0, pRet, Tcl_NewStringObj(zToken, nToken));
    zToken = &zInput[iStart];
    nToken = iEnd-iStart;
    Tcl_ListObjAppendElement(0, pRet, Tcl_NewStringObj(zToken, nToken));
  }

  if( SQLITE_OK!=p->xClose(pCsr) ){
    zErr = "error in xClose()";
    goto finish;
  }
  if( SQLITE_OK!=p->xDestroy(pTokenizer) ){
    zErr = "error in xDestroy()";
    goto finish;
  }

finish:
  if( zErr ){
    sqlite3_result_error(context, zErr, -1);
  }else{
    sqlite3_result_text(context, Tcl_GetString(pRet), -1, SQLITE_TRANSIENT);
  }
  Tcl_DecrRefCount(pRet);
}

static
int registerTokenizer(
  sqlite3 *db, 
  char *zName, 
  const sqlite3_tokenizer_module *p
){
  int rc;
  sqlite3_stmt *pStmt;
  const char zSql[] = "SELECT fts3_tokenizer(?, ?)";

  rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
  if( rc!=SQLITE_OK ){
    return rc;
  }

  sqlite3_bind_text(pStmt, 1, zName, -1, SQLITE_STATIC);
  sqlite3_bind_blob(pStmt, 2, &p, sizeof(p), SQLITE_STATIC);
  sqlite3_step(pStmt);

  return sqlite3_finalize(pStmt);
}

static
int queryTokenizer(
  sqlite3 *db, 
  char *zName,  
  const sqlite3_tokenizer_module **pp
){
  int rc;
  sqlite3_stmt *pStmt;
  const char zSql[] = "SELECT fts3_tokenizer(?)";

  *pp = 0;
  rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
  if( rc!=SQLITE_OK ){
    return rc;
  }

  sqlite3_bind_text(pStmt, 1, zName, -1, SQLITE_STATIC);
  if( SQLITE_ROW==sqlite3_step(pStmt) ){
    if( sqlite3_column_type(pStmt, 0)==SQLITE_BLOB ){
      memcpy((void *)pp, sqlite3_column_blob(pStmt, 0), sizeof(*pp));
    }
  }

  return sqlite3_finalize(pStmt);
}

void sqlite3Fts3SimpleTokenizerModule(sqlite3_tokenizer_module const**ppModule);

/*
** Implementation of the scalar function fts3_tokenizer_internal_test().
** This function is used for testing only, it is not included in the
** build unless SQLITE_TEST is defined.
**
** The purpose of this is to test that the fts3_tokenizer() function
** can be used as designed by the C-code in the queryTokenizer and
** registerTokenizer() functions above. These two functions are repeated
** in the README.tokenizer file as an example, so it is important to
** test them.
**
** To run the tests, evaluate the fts3_tokenizer_internal_test() scalar
** function with no arguments. An assert() will fail if a problem is
** detected. i.e.:
**
**     SELECT fts3_tokenizer_internal_test();
**
*/

/*  
    标量函数fts3_tokenizer_inte rnal_test()的实现。这个函数只用于测试,它不包括在构建过程中，
    除非SQLITE_TEST被定义了。
    
	这个函数的目的是测试在上述queryTokenizer和registerTokenizer()函数中被用来用c设计的fts3_tokenizer()函数。
	这两个函数在README中重复。分词器文件作为一个例子,所以重要的是要测试它们。

    为了运行测试,在没有参数的情况下，评估fts3_tokenizer_internal_test()标量函数。
	如果检测到一个问题assert()将会调用失败。例如:
	   SELECT fts3_tokenizer_internal_test();
*/
static void intTestFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  int rc;
  const sqlite3_tokenizer_module *p1;
  const sqlite3_tokenizer_module *p2;
  sqlite3 *db = (sqlite3 *)sqlite3_user_data(context);

  UNUSED_PARAMETER(argc);
  UNUSED_PARAMETER(argv);

  /* Test the query function */           /* 测试查询函数 */
  sqlite3Fts3SimpleTokenizerModule(&p1);
  rc = queryTokenizer(db, "simple", &p2);
  assert( rc==SQLITE_OK );
  assert( p1==p2 );
  rc = queryTokenizer(db, "nosuchtokenizer", &p2);
  assert( rc==SQLITE_ERROR );
  assert( p2==0 );
  assert( 0==strcmp(sqlite3_errmsg(db), "unknown tokenizer: nosuchtokenizer") );

  /* Test the storage function */         /* 测试存储函数 */
  rc = registerTokenizer(db, "nosuchtokenizer", p1);
  assert( rc==SQLITE_OK );
  rc = queryTokenizer(db, "nosuchtokenizer", &p2);
  assert( rc==SQLITE_OK );
  assert( p2==p1 );

  sqlite3_result_text(context, "ok", -1, SQLITE_STATIC);
}

#endif

/*
** Set up SQL objects in database db used to access the contents of
** the hash table pointed to by argument pHash. The hash table must
** been initialised to use string keys, and to take a private copy 
** of the key when a value is inserted. i.e. by a call similar to:
**
**    sqlite3Fts3HashInit(pHash, FTS3_HASH_STRING, 1);
**
** This function adds a scalar function (see header comment above
** scalarFunc() in this file for details) and, if ENABLE_TABLE is
** defined at compilation time, a temporary virtual table (see header 
** comment above struct HashTableVtab) to the database schema. Both 
** provide read/write access to the contents of *pHash.
**
** The third argument to this function, zName, is used as the name
** of both the scalar and, if created, the virtual table.
*/

/* 
   在数据库db中设置SQL对象用于访问由pHash论点指出哈希表的内容。哈希表必须
   被初始化为用字符串键表示,并在插入值时采取码的私有副本。即由一个类似于:
     
       sqlite3Fts3HashInit(pHash, FTS3_HASH_STRING, 1);

   这个函数添加了一个标量函数(参见上文scalarFunc（）函数在该文件中的详细头注释),
   如果ENABLE_TABLE定义在编译时间内,在数据库模式上的临时虚拟表(参见上面的头注释
   struct HashTableVtab)。可以读写* pHash的内容。

   如果创建虚拟表,这个函数的第三个参数zName作为标量和的名字。
*/
int sqlite3Fts3InitHashTable(
  sqlite3 *db, 
  Fts3Hash *pHash, 
  const char *zName  
){
  int rc = SQLITE_OK;
  void *p = (void *)pHash;
  const int any = SQLITE_ANY;

#ifdef SQLITE_TEST
  char *zTest = 0;
  char *zTest2 = 0;
  void *pdb = (void *)db;
  zTest = sqlite3_mprintf("%s_test", zName);
  zTest2 = sqlite3_mprintf("%s_internal_test", zName);
  if( !zTest || !zTest2 ){
    rc = SQLITE_NOMEM;
  }
#endif

  if( SQLITE_OK==rc ){
    rc = sqlite3_create_function(db, zName, 1, any, p, scalarFunc, 0, 0);
  }
  if( SQLITE_OK==rc ){
    rc = sqlite3_create_function(db, zName, 2, any, p, scalarFunc, 0, 0);
  }
#ifdef SQLITE_TEST
  if( SQLITE_OK==rc ){
    rc = sqlite3_create_function(db, zTest, -1, any, p, testFunc, 0, 0);
  }
  if( SQLITE_OK==rc ){
    rc = sqlite3_create_function(db, zTest2, 0, any, pdb, intTestFunc, 0, 0);
  }
#endif

#ifdef SQLITE_TEST
  sqlite3_free(zTest);
  sqlite3_free(zTest2);
#endif

  return rc;
}

#endif /* !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_FTS3) */
