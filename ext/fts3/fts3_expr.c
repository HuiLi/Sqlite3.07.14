/*
** 2008 Nov 28
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
** This module contains code that implements a parser for fts3 query strings
** (the right-hand argument to the MATCH operator). Because the supported 
** syntax is relatively simple, the whole tokenizer/parser system is
** hand-coded. 
** 这个模块包含了一个使用FTS3解析器实现解析查询字符串的代码（MATCH操作的右侧操作）。
** 因为支持的语法比较简单，整个标记生成器/分析器系统都是手工编码
*/
#include "fts3Int.h"
#if !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_FTS3)

/*
** By default, this module parses the legacy syntax that has been 
** traditionally used by fts3. Or, if SQLITE_ENABLE_FTS3_PARENTHESIS
** is defined, then it uses the new syntax. The differences between
** the new and the old syntaxes are:
** 默认情况下，这个模块解析了历来被FTS3传统使用的语法，或者如果
** SQLITE_ENABLE_FTS3_PARENTHESIS有定义，则使用新的语法，新旧语法的区别是：
**
**  a) The new syntax supports parenthesis. The old does not.
**    新的句法支持括号，旧的不支持
**  b) The new syntax supports the AND and NOT operators. The old does not.
**		新的句法支持AND和NOT操作，旧的不支持
**  c) The old syntax supports the "-" token qualifier. This is not 
**     supported by the new syntax (it is replaced by the NOT operator).
**		旧的句法支持"-"是否是合格的验证，而新的句法不支持（以NOT操作代替）
**  d) When using the old syntax, the OR operator has a greater precedence
**     than an implicit AND. When using the new, both implicity and explicit
**     AND operators have a higher precedence than OR.
**  当使用旧的句法时，OR操作比隐含的AND有更大的优先级，用新句法时，不论是隐式
**  还是显式的，AND都比OR有更高的优先级
** If compiled with SQLITE_TEST defined, then this module exports the
** symbol "int sqlite3_fts3_enable_parentheses". Setting this variable
** to zero causes the module to use the old syntax. If it is set to 
** non-zero the new syntax is activated. This is so both syntaxes can
** be tested using a single build of testfixture.
**   如果已编译的SQLITE_TEST被定义，那这个模块输出"int sqlite3_fts
** 3_enable_parentheses"符号，将这个变量设置为零可以使这个模块使用旧句法
** 设置为非零的话新句法就会被激活，这就是为什么两种句法都可以通过使用单一构建
** testfixture来测试
** The following describes the syntax supported by the fts3 MATCH
** operator in a similar format to that used by the lemon parser
** generator. This module does not use actually lemon, it uses a
** custom parser.
**  下面描述了和LEMON语法分析生成器格式相似的FTS3MATCH操作所支持的语法，
**  该模块没有真正的使用LEMON语法分析生成器，采用了自定义语法分析生成器。

Lemon语法分析生成器:
此文件所描述的FTS3MATCH操作所支持的语法与Lemon语法分析生成器的语法相似。
但是没有完全引用，语法分析器是自定义的。Lemon操作原理：lemon的主要目标
是把一个特定语言的上下文无关文法（CFG）翻译成C语言实现的该语言的语法分
析器。相对其他分析器生成工具Lemon比yacc和bison更精致、更快，而且是可重
入的，也是线程安全的——这对于支持多线程的程序是非常重要的。

**   query ::= andexpr (OR andexpr)*.
**
**   andexpr ::= notexpr (AND? notexpr)*.
**
**   notexpr ::= nearexpr (NOT nearexpr|-TOKEN)*.
**   notexpr ::= LP query RP.
**
**   nearexpr ::= phrase (NEAR distance_opt nearexpr)*.
**
**   distance_opt ::= .
**   distance_opt ::= / INTEGER.
**
**   phrase ::= TOKEN.
**   phrase ::= COLUMN:TOKEN.
**   phrase ::= "TOKEN TOKEN TOKEN...".
*/

#ifdef SQLITE_TEST    /* #ifdef XX 是根据前面有没有定义过XX宏 如果有就编译 #ifdef 到 #endif之间的代码 */
int sqlite3_fts3_enable_parentheses = 0;
#else
# ifdef SQLITE_ENABLE_FTS3_PARENTHESIS 
#  define sqlite3_fts3_enable_parentheses 1
# else
#  define sqlite3_fts3_enable_parentheses 0
# endif
#endif

/*
** Default span for NEAR operators. NEAR运算符的默认宽度
*/
#define SQLITE_FTS3_DEFAULT_NEAR_PARAM 10
#include <string.h>
#include <assert.h>

/*
** isNot:
**   This variable is used by function getNextNode(). When getNextNode() is
**   called, it sets ParseContext.isNot to true if the 'next node' is a 
**   FTSQUERY_PHRASE with a unary "-" attached to it. i.e. "mysql" in the
**   FTS3 query "sqlite -mysql". Otherwise, ParseContext.isNot is set to
**   zero.
*/
/*
** isNot变量是被函数getNextNode()所使用的，当getNextNode()被调用时，如果‘next node’是
** 用一元的‘-’附属的FTSQUERY_PHRASE，它将设置ParseContext.isNot为true，“mysql“在FTS3查询”sqlite -mysql“
** 否则，ParseContext.isNot设为零
*/
typedef struct ParseContext ParseContext;
struct ParseContext {
  sqlite3_tokenizer *pTokenizer;      /* Tokenizer module  分词器模块*/
  int iLangid;                        /* Language id used with tokenizer 标记生成器使用的语言ID */
  const char **azCol;                 /* Array of column names for fts3 table 自定义名字的fts3表的数组*/
  int bFts4;                          /* True to allow FTS4-only syntax 允许只使用FTS4的语法 */
  int nCol;                           /* Number of entries in azCol[] azCol[]数组的大小 */
  int iDefaultCol;                    /* Default column to query 默认列查询 */
  int isNot;                          /* True if getNextNode() sees a unary - 设为true如果getNextNode()查到一元的‘-’*/
  sqlite3_context *pCtx;              /* Write error message here 书写错误信息*/
  int nNest;                          /* Number of nested brackets 嵌套括号的数量*/
};

/*
** This function is equivalent to the standard isspace() function. 
** 这个函数和标准的isspace()函数等价
** The standard isspace() can be awkward to use safely, because although it
** is defined to accept an argument of type int, its behaviour when passed
** an integer that falls outside of the range of the unsigned char type
** is undefined (and sometimes, "undefined" means segfault). This wrapper
** is defined to accept an argument of type char, and always returns 0 for
** any values that fall outside of the range of the unsigned char type (i.e.
** negative values).
** 标准的isspace()函数想要安全的使用是令人尴尬的，因为虽然它被定义为可以接收整数参数，当它通过一个
**　超出无符号字符类型范围的整数时，它的行为未定义的（有时，”未定义“意味着段错误），这个封装
** 是定义来接收字符型参数的，并且总是返回0当有参数数值超出了无符号字符类型的范围时（负值）
*/
//以下函数为检查参数c是否为空格字符。
static int fts3isspace(char c){
  return c==' ' || c=='\t' || c=='\n' || c=='\r' || c=='\v' || c=='\f';
}

/*
** Allocate nByte bytes of memory using sqlite3_malloc(). If successful,
** zero the memory before returning a pointer to it. If unsuccessful, 
** return NULL.
** 使用sqlite3_malloc()函数给nByte分配内存字节，如果成功，在返回它的指针之前
** 清空内存，不成功则返回NULL
*/
static void *fts3MallocZero(int nByte){
  void *pRet = sqlite3_malloc(nByte);
  if( pRet ) memset(pRet, 0, nByte);
  return pRet;
}
//打开分词器
int sqlite3Fts3OpenTokenizer(
sqlite3_tokenizer *pTokenizer,
int iLangid,const char *z,
int n,
sqlite3_tokenizer_cursor **ppCsr
){
  sqlite3_tokenizer_module const *pModule = pTokenizer->pModule;
  sqlite3_tokenizer_cursor *pCsr = 0;
  int rc;

  rc = pModule->xOpen(pTokenizer, z, n, &pCsr);
  assert( rc==SQLITE_OK || pCsr==0 );
  if( rc==SQLITE_OK ){
    pCsr->pTokenizer = pTokenizer;
    if( pModule->iVersion>=1 ){
      rc = pModule->xLanguageid(pCsr, iLangid);
      if( rc!=SQLITE_OK ){
        pModule->xClose(pCsr);
        pCsr = 0;
      }
    }
  }
  *ppCsr = pCsr;
  return rc;
}


/*
** Extract the next token from buffer z (length n) using the tokenizer
** and other information (column names etc.) in pParse. Create an Fts3Expr
** structure of type FTSQUERY_PHRASE containing a phrase consisting of this
** single token and set *ppExpr to point to it. If the end of the buffer is
** reached before a token is found, set *ppExpr to zero. It is the
** responsibility of the caller to eventually deallocate the allocated 
** Fts3Expr structure (if any) by passing it to sqlite3_free().
**
** Return SQLITE_OK if successful, or SQLITE_NOMEM if a memory allocation
** fails.
** 从正在使用分词器的缓冲器z（长度n）中提取下一个标记和其他语法分析的信息（列名等），
** 创造一个FTSQUERY_PHRASE类型包含一个由单一标记组成的短语的Fts3Expr结构然后令*ppExpr指向它。如果
** 缓冲器在找到标记之前到达结尾的话，令*ppExpr为0。这是通过让其进入sqlite3_free()函数从而完
** 全释放被分配的Fts3Expr结构的caller的责任
** 
** 如果成功返回SQLITE_OK，或者返回SQLITE_NOMEN如果内存分配失败
*/
static int getNextToken(
  ParseContext *pParse,                   /* fts3 query parse context fts3查询分析*/
  int iCol,                               /* Value for Fts3Phrase.iColumn Fts3Phrase.iColumn的值*/
  const char *z, int n,                   /* Input string 输入字符串 */
  Fts3Expr **ppExpr,                      /* OUT: expression 输入符号*/
  int *pnConsumed                         /* OUT: Number of bytes consumed 字节消耗数*/
){
  sqlite3_tokenizer *pTokenizer = pParse->pTokenizer;
  sqlite3_tokenizer_module const *pModule = pTokenizer->pModule;
  int rc;
  sqlite3_tokenizer_cursor *pCursor;
  Fts3Expr *pRet = 0;
  int nConsumed = 0;

  rc = sqlite3Fts3OpenTokenizer(pTokenizer, pParse->iLangid, z, n, &pCursor);
  if( rc==SQLITE_OK ){
    const char *zToken;
    int nToken, iStart, iEnd, iPosition;
    int nByte;                               /* total space to allocate */

    rc = pModule->xNext(pCursor, &zToken, &nToken, &iStart, &iEnd, &iPosition);
    if( rc==SQLITE_OK ){
      nByte = sizeof(Fts3Expr) + sizeof(Fts3Phrase) + nToken;
      pRet = (Fts3Expr *)fts3MallocZero(nByte);
      if( !pRet ){
        rc = SQLITE_NOMEM;
      }else{
        pRet->eType = FTSQUERY_PHRASE;
        pRet->pPhrase = (Fts3Phrase *)&pRet[1];
        pRet->pPhrase->nToken = 1;
        pRet->pPhrase->iColumn = iCol;
        pRet->pPhrase->aToken[0].n = nToken;
        pRet->pPhrase->aToken[0].z = (char *)&pRet->pPhrase[1];
        memcpy(pRet->pPhrase->aToken[0].z, zToken, nToken);

        if( iEnd<n && z[iEnd]=='*' ){
          pRet->pPhrase->aToken[0].isPrefix = 1;
          iEnd++;
        }
        while( 1 ){
          if( !sqlite3_fts3_enable_parentheses 
           && iStart>0 && z[iStart-1]=='-' 
          ){
            pParse->isNot = 1;
            iStart--;
          }else if( pParse->bFts4 && iStart>0 && z[iStart-1]=='^' ){
            pRet->pPhrase->aToken[0].bFirst = 1;
            iStart--;
          }else{
            break;
          }
        }
      }
      nConsumed = iEnd;
    }

    pModule->xClose(pCursor);
  }
  
  *pnConsumed = nConsumed;
  *ppExpr = pRet;
  return rc;
}


/*
** Enlarge a memory allocation.  If an out-of-memory allocation occurs,
** then free the old allocation.
** 扩大内存分配，如果内存溢出，则释放之前所分配的
*/
static void *fts3ReallocOrFree(void *pOrig, int nNew){
  void *pRet = sqlite3_realloc(pOrig, nNew);
  if( !pRet ){
    sqlite3_free(pOrig);
  }
  return pRet;
}

/*
** Buffer zInput, length nInput, contains the contents of a quoted string
** that appeared as part of an fts3 query expression. Neither quote character
** is included in the buffer. This function attempts to tokenize the entire
** input buffer and create an Fts3Expr structure of type FTSQUERY_PHRASE 
** containing the results.
**  缓冲器zInput,长度nInput,包含了作为fts3查询表达式出现的引用的字符串的内容，两者都不包含
** 缓冲器所包含的引用字符，这个函数尝试了处理整个缓冲输入和创建包含结果的类型为FTSQUERY_PHRASE
** 的Fts3Expr结构体
** If successful, SQLITE_OK is returned and *ppExpr set to point at the
** allocated Fts3Expr structure. Otherwise, either SQLITE_NOMEM (out of memory
** error) or SQLITE_ERROR (tokenization error) is returned and *ppExpr set
** to 0.
** 如果成功，返回SQLITE_OK并且*ppExpr会指向分配好的Fts3Expr结构体，否则，不论是SQLITE_NOMEM（内存溢出错误）
** 还是SQLITE_ERROR（标记化错误）都会返回并且ppExpr指针设为空。
*/
static int getNextString(
  ParseContext *pParse,                   /* fts3 query parse context */
  const char *zInput, int nInput,         /* Input string */
  Fts3Expr **ppExpr                       /* OUT: expression */
){
  sqlite3_tokenizer *pTokenizer = pParse->pTokenizer;
  sqlite3_tokenizer_module const *pModule = pTokenizer->pModule;
  int rc;
  Fts3Expr *p = 0;
  sqlite3_tokenizer_cursor *pCursor = 0;
  char *zTemp = 0;
  int nTemp = 0;
“
  const int nSpace = sizeof(Fts3Expr) + sizeof(Fts3Phrase);
  int nToken = 0;

  /* The final Fts3Expr data structure, including the Fts3Phrase,
  ** Fts3PhraseToken structures token buffers are all stored as a single 
  ** allocation so that the expression can be freed with a single call to
  ** sqlite3_free(). Setting this up requires a two pass approach.
  **  最终的Fts3Expr数据结构体，包含了Fts3Phrase，Fts3PhraseToken结构体象征着缓冲区
  **  以单独分配的形式存储，所以表达式就可以以被sqlite3_free()单独访问的形式释放，
  ** 设置这个需要两种方法途径接近
  ** The first pass, in the block below, uses a tokenizer cursor to iterate
  ** through the tokens in the expression. This pass uses fts3ReallocOrFree()
  ** to assemble data in two dynamic buffers:
  ** 第一种，在块下面，使用分析器指针通过表达式的符号迭代，这种方法使用 fts3ReallocOrFree()
  ** 在2种动态缓冲区里收集数据。
  **   Buffer p: Points to the Fts3Expr structure, followed by the Fts3Phrase
  **             structure, followed by the array of Fts3PhraseToken 
  **             structures. This pass only populates the Fts3PhraseToken array.
  **  缓冲区P:指向Fts3Expr结构体，跟随Fts3Expr结构体，跟随Fts3PhraseToken结构体数组
  ** 这种途径只存在于Fts3PhraseToken数组中。
  **   Buffer zTemp: Contains copies of all tokens.
  **	缓冲区zTemp:包含所有符号的副本
  ** The second pass, in the block that begins "if( rc==SQLITE_DONE )" below,
  ** appends buffer zTemp to buffer p, and fills in the Fts3Expr and Fts3Phrase
  ** structures.
  ** 第二个途径，在块中开始"if( rc==SQLITE_DONE )"的下方，将缓冲区zTemp添加到p中，并填充到
  ** Fts3Expr and Fts3Phrase结构体中。
  */
  rc = sqlite3Fts3OpenTokenizer(
      pTokenizer, pParse->iLangid, zInput, nInput, &pCursor);
  if( rc==SQLITE_OK ){
    int ii;
    for(ii=0; rc==SQLITE_OK; ii++){
      const char *zByte;
      int nByte, iBegin, iEnd, iPos;
      rc = pModule->xNext(pCursor, &zByte, &nByte, &iBegin, &iEnd, &iPos);
      if( rc==SQLITE_OK ){
        Fts3PhraseToken *pToken;

        p = fts3ReallocOrFree(p, nSpace + ii*sizeof(Fts3PhraseToken));
        if( !p ) goto no_mem;

        zTemp = fts3ReallocOrFree(zTemp, nTemp + nByte);
        if( !zTemp ) goto no_mem;

        assert( nToken==ii );
        pToken = &((Fts3Phrase *)(&p[1]))->aToken[ii];
        memset(pToken, 0, sizeof(Fts3PhraseToken));

        memcpy(&zTemp[nTemp], zByte, nByte);
        nTemp += nByte;

        pToken->n = nByte;
        pToken->isPrefix = (iEnd<nInput && zInput[iEnd]=='*');
        pToken->bFirst = (iBegin>0 && zInput[iBegin-1]=='^');
        nToken = ii+1;
      }
    }

    pModule->xClose(pCursor);
    pCursor = 0;
  }

  if( rc==SQLITE_DONE ){
    int jj;
    char *zBuf = 0;

    p = fts3ReallocOrFree(p, nSpace + nToken*sizeof(Fts3PhraseToken) + nTemp);
    if( !p ) goto no_mem;
    memset(p, 0, (char *)&(((Fts3Phrase *)&p[1])->aToken[0])-(char *)p);
    p->eType = FTSQUERY_PHRASE;
    p->pPhrase = (Fts3Phrase *)&p[1];
    p->pPhrase->iColumn = pParse->iDefaultCol;
    p->pPhrase->nToken = nToken;

    zBuf = (char *)&p->pPhrase->aToken[nToken];
    if( zTemp ){
      memcpy(zBuf, zTemp, nTemp);
      sqlite3_free(zTemp);
    }else{
      assert( nTemp==0 );
    }

    for(jj=0; jj<p->pPhrase->nToken; jj++){
      p->pPhrase->aToken[jj].z = zBuf;
      zBuf += p->pPhrase->aToken[jj].n;
    }
    rc = SQLITE_OK;
  }

  *ppExpr = p;
  return rc;
no_mem:

  if( pCursor ){
    pModule->xClose(pCursor);
  }
  sqlite3_free(zTemp);
  sqlite3_free(p);
  *ppExpr = 0;
  return SQLITE_NOMEM;
}

/*
** Function getNextNode(), which is called by fts3ExprParse(), may itself
** call fts3ExprParse(). So this forward declaration is required.
** 函数getNextNode()，被函数fts3ExprParse()所引用，可能它自己引用fts3ExprParse()，所以需要提前声明
*/
static int fts3ExprParse(ParseContext *, const char *, int, Fts3Expr **, int *);

/*
** The output variable *ppExpr is populated with an allocated Fts3Expr 
** structure, or set to 0 if the end of the input buffer is reached.
** 输出变量*ppExpr在一个被分配的结构体Fts3Expr中。如果输入缓冲到达的化就设为0
** Returns an SQLite error code. SQLITE_OK if everything works, SQLITE_NOMEM
** if a malloc failure occurs, or SQLITE_ERROR if a parse error is encountered.
** If SQLITE_ERROR is returned, pContext is populated with an error message.
** 返回SQLite错误代码，一切正常就返回SQLITE_OK,内存分配失败返回SQLITE_NOMEN,或者遇到
** 分析错误则返回SQLITE_ERROR,
*/
static int getNextNode(
  ParseContext *pParse,                   /* fts3 query parse context */
  const char *z, int n,                   /* Input string */
  Fts3Expr **ppExpr,                      /* OUT: expression */
  int *pnConsumed                         /* OUT: Number of bytes consumed */
){
  static const struct Fts3Keyword {
    char *z;                              /* Keyword text */
    unsigned char n;                      /* Length of the keyword */
    unsigned char parenOnly;              /* Only valid in paren mode 仅在括弧模式有效*/
    unsigned char eType;                  /* Keyword code */
  } aKeyword[] = {
    { "OR" ,  2, 0, FTSQUERY_OR   },
    { "AND",  3, 1, FTSQUERY_AND  },
    { "NOT",  3, 1, FTSQUERY_NOT  },
    { "NEAR", 4, 0, FTSQUERY_NEAR }
  };
  int ii;
  int iCol;
  int iColLen;
  int rc;
  Fts3Expr *pRet = 0;

  const char *zInput = z;
  int nInput = n;

  pParse->isNot = 0;

  /* Skip over any whitespace before checking for a keyword, an open or
  ** close bracket, or a quoted string. 
  ** 在检查关键词时跳过任何分隔符，或者一个打开或者关闭的括号，或者一个引用的字符串
  */
  while( nInput>0 && fts3isspace(*zInput) ){
    nInput--;
    zInput++;
  }
  if( nInput==0 ){
    return SQLITE_DONE;
  }

  /* See if we are dealing with a keyword. 当我们处理一个关键词时 */
  for(ii=0; ii<(int)(sizeof(aKeyword)/sizeof(struct Fts3Keyword)); ii++){
    const struct Fts3Keyword *pKey = &aKeyword[ii];

    if( (pKey->parenOnly & ~sqlite3_fts3_enable_parentheses)!=0 ){
      continue;
    }

    if( nInput>=pKey->n && 0==memcmp(zInput, pKey->z, pKey->n) ){
      int nNear = SQLITE_FTS3_DEFAULT_NEAR_PARAM;
      int nKey = pKey->n;
      char cNext;

      /* If this is a "NEAR" keyword, check for an explicit nearness.
	    如果这是一个近似的词，则找出最接近的
	  */
      if( pKey->eType==FTSQUERY_NEAR ){
        assert( nKey==4 );
        if( zInput[4]=='/' && zInput[5]>='0' && zInput[5]<='9' ){
          nNear = 0;
          for(nKey=5; zInput[nKey]>='0' && zInput[nKey]<='9'; nKey++){
            nNear = nNear * 10 + (zInput[nKey] - '0');
          }
        }
      }

      /* At this point this is probably a keyword. But for that to be true,
      ** the next byte must contain either whitespace, an open or close
      ** parenthesis, a quote character, or EOF. 
	  ** 在这个点上很可能是一个关键词，但是要确定的话，下一个字节必须包含另一个分隔符，一个
	  ** 开或关的括号，一个引用字符，或者文件结束符号
      */
      cNext = zInput[nKey];
      if( fts3isspace(cNext) 
       || cNext=='"' || cNext=='(' || cNext==')' || cNext==0
      ){
        pRet = (Fts3Expr *)fts3MallocZero(sizeof(Fts3Expr));
        if( !pRet ){
          return SQLITE_NOMEM;
        }
        pRet->eType = pKey->eType;
        pRet->nNear = nNear;
        *ppExpr = pRet;
        *pnConsumed = (int)((zInput - z) + nKey);
        return SQLITE_OK;
      }

      /* Turns out that wasn't a keyword after all. This happens if the
      ** user has supplied a token such as "ORacle". Continue.
	  ** 最终证明它不是一个关键词，这种情况会在使用者提供了一个相当于甲骨文的字符时发生，继续
      */
    }
  }

  /* Check for an open bracket. 检查一个打开的括号*/
  if( sqlite3_fts3_enable_parentheses ){
    if( *zInput=='(' ){
      int nConsumed;
      pParse->nNest++;
      rc = fts3ExprParse(pParse, &zInput[1], nInput-1, ppExpr, &nConsumed);
      if( rc==SQLITE_OK && !*ppExpr ){
        rc = SQLITE_DONE;
      }
      *pnConsumed = (int)((zInput - z) + 1 + nConsumed);
      return rc;
    }
  
    /* Check for a close bracket. 检查一个关闭的括号*/
    if( *zInput==')' ){
      pParse->nNest--;
      *pnConsumed = (int)((zInput - z) + 1);
      return SQLITE_DONE;
    }
  }

  /* See if we are dealing with a quoted phrase. If this is the case, then
  ** search for the closing quote and pass the whole string to getNextString()
  ** for processing. This is easy to do, as fts3 has no syntax for escaping
  ** a quote character embedded in a string.
     当我们在解决一个引用词组时，如果是一个实例，那么寻找关闭的括号并且把整个字符串传送到getNextString()
	 处理，这很简单，因为fts3没有溢出一个嵌入字符串中引用字符的语法。
  */
  if( *zInput=='"' ){
    for(ii=1; ii<nInput && zInput[ii]!='"'; ii++);
    *pnConsumed = (int)((zInput - z) + ii + 1);
    if( ii==nInput ){
      return SQLITE_ERROR;
    }
    return getNextString(pParse, &zInput[1], ii-1, ppExpr);
  }


  /* If control flows to this point, this must be a regular token, or 
  ** the end of the input. Read a regular token using the sqlite3_tokenizer
  ** interface. Before doing so, figure out if there is an explicit
  ** column specifier for the token. 
  ** 如果控制流向这个点，那一定是一个规则的记号，或者是在输入的末端，使用sqlite3_tokenizer
  ** 读取规则的记号，在这之前，要先算出此记号是否存在一个显式的列说明符
  ** TODO: Strangely, it is not possible to associate a column specifier
  ** with a quoted phrase, only with a single token. Not sure if this was
  ** an implementation artifact or an intentional decision when fts3 was
  ** first implemented. Whichever it was, this module duplicates the 
  ** limitation.
  ** 备忘：奇怪的是，不可能只用一个标记来连接一个列说明符和一个引用词组，当fts
  ** 3第一次执行时，它是安装时就启用或者是刻意的决定都是不一定的，无论是哪一个，
  ** 这个模块到拷贝了这个限制
  */
  iCol = pParse->iDefaultCol;
  iColLen = 0;
  for(ii=0; ii<pParse->nCol; ii++){
    const char *zStr = pParse->azCol[ii];
    int nStr = (int)strlen(zStr);
    if( nInput>nStr && zInput[nStr]==':' 
     && sqlite3_strnicmp(zStr, zInput, nStr)==0 
    ){
      iCol = ii;
      iColLen = (int)((zInput - z) + nStr + 1);
      break;
    }
  }
  rc = getNextToken(pParse, iCol, &z[iColLen], n-iColLen, ppExpr, pnConsumed);
  *pnConsumed += iColLen;
  return rc;
}

/*
** The argument is an Fts3Expr structure for a binary operator (any type
** except an FTSQUERY_PHRASE). Return an integer value representing the
** precedence of the operator. Lower values have a higher precedence (i.e.
** group more tightly). For example, in the C language, the == operator
** groups more tightly than ||, and would therefore have a higher precedence.
**  这里阐述了一个用于二元操作的Fts3Expr结构体（除了FTSQUERY_PHRASE之外的所有类型）
** 返回一个代表操作优先级的整数。较小的数有较高的优先级，比如，C语言中，"=="
** 操作组比||更小，因此有较高的优先级。
** When using the new fts3 query syntax (when SQLITE_ENABLE_FTS3_PARENTHESIS
** is defined), the order of the operators in precedence from highest to
** lowest is:
** 当使用新的fts3查询语法时（SQLITE_ENABLE_FTS3_PARENTHESIS已定义，）优先级顺序
** 从高往低则是：
**   NEAR
**   NOT
**   AND (including implicit ANDs)
**   OR
**
** Note that when using the old query syntax, the OR operator has a higher
** precedence than the AND operator.
** 注意当使用旧的查询语法时，OR操作有比AND操作更高的优先级
*/
static int opPrecedence(Fts3Expr *p){
  assert( p->eType!=FTSQUERY_PHRASE );
  if( sqlite3_fts3_enable_parentheses ){
    return p->eType;
  }else if( p->eType==FTSQUERY_NEAR ){
    return 1;
  }else if( p->eType==FTSQUERY_OR ){
    return 2;
  }
  assert( p->eType==FTSQUERY_AND );
  return 3;
}

/*
** Argument ppHead contains a pointer to the current head of a query 
** expression tree being parsed. pPrev is the expression node most recently
** inserted into the tree. This function adds pNew, which is always a binary
** operator node, into the expression tree based on the relative precedence
** of pNew and the existing nodes of the tree. This may result in the head
** of the tree changing, in which case *ppHead is set to the new root node.
** ppHead包含了一个指针指向了一个正在被解析的查询表达树的顶部，pPrev则是一个
** 最近插入树中的结点，这个函数增加了pNew，它总是一个二元操作结点，基于和pNew
** 相对的优先级以及现存于树中结点的优先级插入树中，这可能会导致树头结点的改变，
** ppHead会指向新的根节点
*/
static void insertBinaryOperator(
  Fts3Expr **ppHead,       /* Pointer to the root node of a tree */
  Fts3Expr *pPrev,         /* Node most recently inserted into the tree */
  Fts3Expr *pNew           /* New binary node to insert into expression tree */
){
  Fts3Expr *pSplit = pPrev;
  while( pSplit->pParent && opPrecedence(pSplit->pParent)<=opPrecedence(pNew) ){
    pSplit = pSplit->pParent;
  }

  if( pSplit->pParent ){
    assert( pSplit->pParent->pRight==pSplit );
    pSplit->pParent->pRight = pNew;
    pNew->pParent = pSplit->pParent;
  }else{
    *ppHead = pNew;
  }
  pNew->pLeft = pSplit;
  pSplit->pParent = pNew;
}

/*
** Parse the fts3 query expression found in buffer z, length n. This function
** returns either when the end of the buffer is reached or an unmatched 
** closing bracket - ')' - is encountered.
** 解析建立在缓冲器z上长度为n的fts3查询表达式，这个函数会在缓冲区达到末尾时或
** 遇到没匹配的括号“)”时返回。
** If successful, SQLITE_OK is returned, *ppExpr is set to point to the
** parsed form of the expression and *pnConsumed is set to the number of
** bytes read from buffer z. Otherwise, *ppExpr is set to 0 and SQLITE_NOMEM
** (out of memory error) or SQLITE_ERROR (parse error) is returned.
** 如果成功，则返回SQLITE_OK,*ppExpr指向被解析过的form，*pnConsumed 指向，从缓冲区
** z读来的字节数，否则，*ppExpr设为空指针，并且返回SQLITE_NOMEM（当内存溢出时）
**  或SQLITE_ERROR（解析错误）
*/
static int fts3ExprParse(
  ParseContext *pParse,                   /* fts3 query parse context */
  const char *z, int n,                   /* Text of MATCH query */
  Fts3Expr **ppExpr,                      /* OUT: Parsed query structure */
  int *pnConsumed                         /* OUT: Number of bytes consumed */
){
  Fts3Expr *pRet = 0;
  Fts3Expr *pPrev = 0;
  Fts3Expr *pNotBranch = 0;               /* Only used in legacy parse mode */
  int nIn = n;
  const char *zIn = z;
  int rc = SQLITE_OK;
  int isRequirePhrase = 1;

  while( rc==SQLITE_OK ){
    Fts3Expr *p = 0;
    int nByte = 0;
    rc = getNextNode(pParse, zIn, nIn, &p, &nByte);
    if( rc==SQLITE_OK ){
      int isPhrase;

      if( !sqlite3_fts3_enable_parentheses 
       && p->eType==FTSQUERY_PHRASE && pParse->isNot 
      ){
        /* Create an implicit NOT operator. 创建一个隐含的not操作*/
        Fts3Expr *pNot = fts3MallocZero(sizeof(Fts3Expr));
        if( !pNot ){
          sqlite3Fts3ExprFree(p);
          rc = SQLITE_NOMEM;
          goto exprparse_out;
        }
        pNot->eType = FTSQUERY_NOT;
        pNot->pRight = p;
        if( pNotBranch ){
          pNot->pLeft = pNotBranch;
        }
        pNotBranch = pNot;
        p = pPrev;
      }else{
        int eType = p->eType;
        isPhrase = (eType==FTSQUERY_PHRASE || p->pLeft);

        /* The isRequirePhrase variable is set to true if a phrase or
        ** an expression contained in parenthesis is required. If a
        ** binary operator (AND, OR, NOT or NEAR) is encounted when
        ** isRequirePhrase is set, this is a syntax error.
		** isRequirePhrase变量会在当一个包含查询或者表达式的圆括号返回时设为
		** true。如果在isRequirePhrase设置时遇到一个二元操作(AND, OR, NOT or NEAR)
		** 则是一个语法错误
        */
        if( !isPhrase && isRequirePhrase ){
          sqlite3Fts3ExprFree(p);
          rc = SQLITE_ERROR;
          goto exprparse_out;
        }
  
        if( isPhrase && !isRequirePhrase ){
          /* Insert an implicit AND operator. */
          Fts3Expr *pAnd;
          assert( pRet && pPrev );
          pAnd = fts3MallocZero(sizeof(Fts3Expr));
          if( !pAnd ){
            sqlite3Fts3ExprFree(p);
            rc = SQLITE_NOMEM;
            goto exprparse_out;
          }
          pAnd->eType = FTSQUERY_AND;
          insertBinaryOperator(&pRet, pPrev, pAnd);
          pPrev = pAnd;
        }

        /* This test catches attempts to make either operand of a NEAR
        ** operator something other than a phrase. For example, either of
        ** the following:
        ** 这个测试引发一个让NEAR操作除了数组之外的东西的尝试，例如，以下任何一个：
        **    (bracketed expression 一个有括号的表达式) NEAR phrase
        **    phrase NEAR (bracketed expression)
        **
        ** Return an error in either case 以上任一例子都返回错误.
        */
        if( pPrev && (
            (eType==FTSQUERY_NEAR && !isPhrase && pPrev->eType!=FTSQUERY_PHRASE)
         || (eType!=FTSQUERY_PHRASE && isPhrase && pPrev->eType==FTSQUERY_NEAR)
        )){
          sqlite3Fts3ExprFree(p);
          rc = SQLITE_ERROR;
          goto exprparse_out;
        }
  
        if( isPhrase ){
          if( pRet ){
            assert( pPrev && pPrev->pLeft && pPrev->pRight==0 );
            pPrev->pRight = p;
            p->pParent = pPrev;
          }else{
            pRet = p;
          }
        }else{
          insertBinaryOperator(&pRet, pPrev, p);
        }
        isRequirePhrase = !isPhrase;
      }
      assert( nByte>0 );
    }
    assert( rc!=SQLITE_OK || (nByte>0 && nByte<=nIn) );
    nIn -= nByte;
    zIn += nByte;
    pPrev = p;
  }

  if( rc==SQLITE_DONE && pRet && isRequirePhrase ){
    rc = SQLITE_ERROR;
  }

  if( rc==SQLITE_DONE ){
    rc = SQLITE_OK;
    if( !sqlite3_fts3_enable_parentheses && pNotBranch ){
      if( !pRet ){
        rc = SQLITE_ERROR;
      }else{
        Fts3Expr *pIter = pNotBranch;
        while( pIter->pLeft ){
          pIter = pIter->pLeft;
        }
        pIter->pLeft = pRet;
        pRet = pNotBranch;
      }
    }
  }
  *pnConsumed = n - nIn;

exprparse_out:
  if( rc!=SQLITE_OK ){
    sqlite3Fts3ExprFree(pRet);
    sqlite3Fts3ExprFree(pNotBranch);
    pRet = 0;
  }
  *ppExpr = pRet;
  return rc;
}

/*
** Parameters z and n contain a pointer to and length of a buffer containing
** an fts3 query expression, respectively. This function attempts to parse the
** query expression and create a tree of Fts3Expr structures representing the
** parsed expression. If successful, *ppExpr is set to point to the head
** of the parsed expression tree and SQLITE_OK is returned. If an error
** occurs, either SQLITE_NOMEM (out-of-memory error) or SQLITE_ERROR (parse
** error) is returned and *ppExpr is set to 0.
** 参数z和n分别包含一个指针和一个包含一个fts3查询表达式的缓冲区的长度，这个函数
** 尝试解析一个查询表达式和创建一个表示已被查询的表达式的Fts3Expr结构体树，如果成功
** *ppExpr指向查询表达树的头结点并返回SQLITE_OK，如果有错误发生，返回SQLITE_NOMEN或
** SQLITE_ERROR，*ppExpr设为空指针。
** If parameter n is a negative number, then z is assumed to point to a
** nul-terminated string and the length is determined using strlen().
** 如果参数n为负数，则假设z指向一个空终止字符串并且长度使用strlen()
** The first parameter, pTokenizer, is passed the fts3 tokenizer module to
** use to normalize query tokens while parsing the expression. The azCol[]
** array, which is assumed to contain nCol entries, should contain the names
** of each column in the target fts3 table, in order from left to right. 
** Column names must be nul-terminated strings.
** 第一个参数pTokenizer传递了当解析表达式时使用正规查询符号的fts3编译器模块，
** azCol[]数组，它是被假定为包含nCol入口，并应该包含目标fts3表格每一列的名字，
** 为了能从左到右表示，列名必须为没有终止符的字符串。
** The iDefaultCol parameter should be passed the index of the table column
** that appears on the left-hand-side of the MATCH operator (the default
** column to match against for tokens for which a column name is not explicitly
** specified as part of the query string), or -1 if tokens may by default
** match any table column.
**  iDefaultCol参数应该传递一个出现在MATCH操作的左部表中的列索引(默认match列反对一个表示列的名字没有明确表示
** 它是查询字符串的一部分的符号)，或者如果符号可能是由默认match的表的列则iDefaultCol为-1
*/
int sqlite3Fts3ExprParse(
  sqlite3_tokenizer *pTokenizer,      /* Tokenizer module */
  int iLangid,                        /* Language id for tokenizer */
  char **azCol,                       /* Array of column names for fts3 table */
  int bFts4,                          /* True to allow FTS4-only syntax */
  int nCol,                           /* Number of entries in azCol[] */
  int iDefaultCol,                    /* Default column to query */
  const char *z, int n,               /* Text of MATCH query */
  Fts3Expr **ppExpr                   /* OUT: Parsed query structure */
){
  int nParsed;
  int rc;
  ParseContext sParse;

  memset(&sParse, 0, sizeof(ParseContext));
  sParse.pTokenizer = pTokenizer;
  sParse.iLangid = iLangid;
  sParse.azCol = (const char **)azCol;
  sParse.nCol = nCol;
  sParse.iDefaultCol = iDefaultCol;
  sParse.bFts4 = bFts4;
  if( z==0 ){
    *ppExpr = 0;
    return SQLITE_OK;
  }
  if( n<0 ){
    n = (int)strlen(z);
  }
  rc = fts3ExprParse(&sParse, z, n, ppExpr, &nParsed);

  /* Check for mismatched parenthesis */
  if( rc==SQLITE_OK && sParse.nNest ){
    rc = SQLITE_ERROR;
    sqlite3Fts3ExprFree(*ppExpr);
    *ppExpr = 0;
  }

  return rc;
}

/*
** Free a parsed fts3 query expression allocated by sqlite3Fts3ExprParse().
释放一个被sqlite3Fts3ExprParse()分配的已分析的fts3查询表达式。
*/
void sqlite3Fts3ExprFree(Fts3Expr *p){
  if( p ){
    assert( p->eType==FTSQUERY_PHRASE || p->pPhrase==0 );
    sqlite3Fts3ExprFree(p->pLeft);
    sqlite3Fts3ExprFree(p->pRight);
    sqlite3Fts3EvalPhraseCleanup(p->pPhrase);
    sqlite3_free(p->aMI);
    sqlite3_free(p);
  }
}

/****************************************************************************
*****************************************************************************
** Everything after this point is just test code. 在这之后的都是测试代码
*/

#ifdef SQLITE_TEST

#include <stdio.h>

/*
** Function to query the hash-table of tokenizers (see README.tokenizers). 一个查询tokenizers的哈希表的函数
*/
static int queryTestTokenizer(
  sqlite3 *db, 
  const char *zName,  
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

/*
** Return a pointer to a buffer containing a text representation of the
** expression passed as the first argument. The buffer is obtained from
** sqlite3_malloc(). It is the responsibility of the caller to use 
** sqlite3_free() to release the memory. If an OOM condition is encountered,
** NULL is returned.
** 返回一个指向一个包含一个代表表达式被作为第一个部分传递的文本的缓冲区，这个
** 缓冲区从sqlite3_malloc()取得。引用者需使用sqlite3_free()来释放内存。如果遇到内存不足
** 的情况，返回NULL
** If the second argument is not NULL, then its contents are prepended to 
** the returned expression text and then freed using sqlite3_free().
** 如果第二部分不为空，那它的目录会预设为已返回的表达式文本然后释放正在使用的sqlite3_free()
*/
static char *exprToString(Fts3Expr *pExpr, char *zBuf){
  switch( pExpr->eType ){
    case FTSQUERY_PHRASE: {
      Fts3Phrase *pPhrase = pExpr->pPhrase;
      int i;
      zBuf = sqlite3_mprintf(
          "%zPHRASE %d 0", zBuf, pPhrase->iColumn);
      for(i=0; zBuf && i<pPhrase->nToken; i++){
        zBuf = sqlite3_mprintf("%z %.*s%s", zBuf, 
            pPhrase->aToken[i].n, pPhrase->aToken[i].z,
            (pPhrase->aToken[i].isPrefix?"+":"")
        );
      }
      return zBuf;
    }

    case FTSQUERY_NEAR:
      zBuf = sqlite3_mprintf("%zNEAR/%d ", zBuf, pExpr->nNear);
      break;
    case FTSQUERY_NOT:
      zBuf = sqlite3_mprintf("%zNOT ", zBuf);
      break;
    case FTSQUERY_AND:
      zBuf = sqlite3_mprintf("%zAND ", zBuf);
      break;
    case FTSQUERY_OR:
      zBuf = sqlite3_mprintf("%zOR ", zBuf);
      break;
  }

  if( zBuf ) zBuf = sqlite3_mprintf("%z{", zBuf);
  if( zBuf ) zBuf = exprToString(pExpr->pLeft, zBuf);
  if( zBuf ) zBuf = sqlite3_mprintf("%z} {", zBuf);

  if( zBuf ) zBuf = exprToString(pExpr->pRight, zBuf);
  if( zBuf ) zBuf = sqlite3_mprintf("%z}", zBuf);

  return zBuf;
}

/*
** This is the implementation of a scalar SQL function used to test the 
** expression parser. It should be called as follows:
**  下面实现了一个使用来测试表达式分析器的标量SQL函数，它被定义为如下格式：
**   fts3_exprtest(<tokenizer>, <expr>, <column 1>, ...);
**
** The first argument, <tokenizer>, is the name of the fts3 tokenizer used
** to parse the query expression (see README.tokenizers). The second argument
** is the query expression to parse. Each subsequent argument is the name
** of a column of the fts3 table that the query expression may refer to.
** For example:
** 第一个部分<tokenizer>是用来解析查询表达式的fts3符号的名字，第二部分是要分析的查询表达式，
** 随后的每一个部分都是查询表达式可能涉及到的fts3标的列的名字，
** 例如
**   SELECT fts3_exprtest('simple', 'Bill col2:Bloggs', 'col1', 'col2');
*/
static void fts3ExprTest(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  sqlite3_tokenizer_module const *pModule = 0;
  sqlite3_tokenizer *pTokenizer = 0;
  int rc;
  char **azCol = 0;
  const char *zExpr;
  int nExpr;
  int nCol;
  int ii;
  Fts3Expr *pExpr;
  char *zBuf = 0;
  sqlite3 *db = sqlite3_context_db_handle(context);

  if( argc<3 ){
    sqlite3_result_error(context, 
        "Usage: fts3_exprtest(tokenizer, expr, col1, ...", -1
    );
    return;
  }

  rc = queryTestTokenizer(db,
                          (const char *)sqlite3_value_text(argv[0]), &pModule);
  if( rc==SQLITE_NOMEM ){
    sqlite3_result_error_nomem(context);
    goto exprtest_out;
  }else if( !pModule ){
    sqlite3_result_error(context, "No such tokenizer module", -1);
    goto exprtest_out;
  }

  rc = pModule->xCreate(0, 0, &pTokenizer);
  assert( rc==SQLITE_NOMEM || rc==SQLITE_OK );
  if( rc==SQLITE_NOMEM ){
    sqlite3_result_error_nomem(context);
    goto exprtest_out;
  }
  pTokenizer->pModule = pModule;

  zExpr = (const char *)sqlite3_value_text(argv[1]);
  nExpr = sqlite3_value_bytes(argv[1]);
  nCol = argc-2;
  azCol = (char **)sqlite3_malloc(nCol*sizeof(char *));
  if( !azCol ){
    sqlite3_result_error_nomem(context);
    goto exprtest_out;
  }
  for(ii=0; ii<nCol; ii++){
    azCol[ii] = (char *)sqlite3_value_text(argv[ii+2]);
  }

  rc = sqlite3Fts3ExprParse(
      pTokenizer, 0, azCol, 0, nCol, nCol, zExpr, nExpr, &pExpr
  );
  if( rc!=SQLITE_OK && rc!=SQLITE_NOMEM ){
    sqlite3_result_error(context, "Error parsing expression", -1);
  }else if( rc==SQLITE_NOMEM || !(zBuf = exprToString(pExpr, 0)) ){
    sqlite3_result_error_nomem(context);
  }else{
    sqlite3_result_text(context, zBuf, -1, SQLITE_TRANSIENT);
    sqlite3_free(zBuf);
  }

  sqlite3Fts3ExprFree(pExpr);

exprtest_out:
  if( pModule && pTokenizer ){
    rc = pModule->xDestroy(pTokenizer);
  }
  sqlite3_free(azCol);
}

/*
** Register the query expression parser test function fts3_exprtest() 
** with database connection db. 
登记使用了数据库连接db的查询表达式分析器测试函数fts3_exprtest()
*/
int sqlite3Fts3ExprInitTestInterface(sqlite3* db){
  return sqlite3_create_function(
      db, "fts3_exprtest", -1, SQLITE_UTF8, 0, fts3ExprTest, 0, 0
  );
}

#endif
#endif /* !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_FTS3) */
