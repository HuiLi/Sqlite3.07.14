/*
** 2006 Oct 10   
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
** Implementation of the "simple" full-text-search tokenizer.
*/
/*
** 2006年 10月10日
** 作者放弃对源代码的版权，而改为一个一个法律声明。
** 可能你用在好的地方而不是坏的地方。
**可能你宽恕他人和自己。
**愿你宽心与人分享,索取不多于你所施予。
**
******************************************************************************
**
**简单的全文搜索分词器的实现
*/
/*
** The code in this file is only compiled if:
**
**     * The FTS2 module is being built as an extension
**       (in which case SQLITE_CORE is not defined), or
**
**     * The FTS2 module is being built into the core of
**       SQLite (in which case SQLITE_ENABLE_FTS2 is defined).
*/
/*
**本文件中的代码只在如下情况下被编译:
**   *FTS2模块被创建为一个扩展
**    （在这种情况下SQLITE_CORE没有定义），或者
**   *FTS2模块被创建在 SQLite的核心
**   （在这种情况下定义了SQLITE_ENABLE_FTS2）。
*/
#if !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_FTS2)
/* SQLITE_CORE没有定义而定义了SQLITE_ENABLE_FTS2 */


#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fts2_tokenizer.h"

typedef struct simple_tokenizer {    /*定义结构体“simple_tokenizer”*/
  sqlite3_tokenizer base;                  /**/
  char delim[128];             /* flag ASCII delimiters  */  
  /* 标记ASCII分隔符 */
} simple_tokenizer;

typedef struct simple_tokenizer_cursor {
  sqlite3_tokenizer_cursor base;
  const char *pInput;          /* input we are tokenizing */
  /* 放入我们分词的字符 */
  int nBytes;                  /* size of the input  */
  /* 放入字符的大小 */
  int iOffset;                 /* current position in pInput */
  /* 在pInput中的当前位置 */
  int iToken;                  /* index of next token to be returned */
  /* 下一个标记符返回的索引 */
  char *pToken;                /* storage for current token */
  /* 存储当前标记符 */
  int nTokenAllocated;         /* space allocated to zToken buffer */
  /* 分配给zToken缓冲空间 */
} simple_tokenizer_cursor;


/* Forward declaration */
/*  声明  */
static const sqlite3_tokenizer_module simpleTokenizerModule;  /*定义静态变量*/

static int simpleDelim(simple_tokenizer *t, unsigned char c){
  return c<0x80 && t->delim[c];
}

/*
** Create a new tokenizer instance.
** 创建一个新的分词器实例
*/
static int simpleCreate(
  int argc, const char * const *argv,
  sqlite3_tokenizer **ppTokenizer
){
  simple_tokenizer *t;

  t = (simple_tokenizer *) sqlite3_malloc(sizeof(*t));
  if( t==NULL ) return SQLITE_NOMEM;
  memset(t, 0, sizeof(*t));

  /* TODO(shess) Delimiters need to remain the same from run to run,
  ** else we need to reindex.  One solution would be a meta-table to
  ** track such information in the database, then we'd only want this
  ** information on the initial create.
  **在运行时分隔符需要保持不变,否则我们需要重建索引。
  **一个解决方案：用一个元表在数据库中来跟踪这些信息,
  **那么我们就会只需要这最初创建的信息。
  */
  if( argc>1 ){
    int i, n = strlen(argv[1]);
    for(i=0; i<n; i++){
      unsigned char ch = argv[1][i];
      /* We explicitly don't support UTF-8 delimiters for now. */
	  /*我们现在明确不支持utf - 8的分隔符。*/
      if( ch>=0x80 ){
        sqlite3_free(t);
        return SQLITE_ERROR;
      }
      t->delim[ch] = 1;
    }
  } else {
    /* Mark non-alphanumeric ASCII characters as delimiters */
	/*非字母数字的ASCII字符标记为分隔符*/
    int i;
    for(i=1; i<0x80; i++){
      t->delim[i] = !((i>='0' && i<='9') || (i>='A' && i<='Z') ||
                      (i>='a' && i<='z'));
    }
  }

  *ppTokenizer = &t->base;
  return SQLITE_OK;
}

/*
** Destroy a tokenizer
** 删除一个分词器
*/
static int simpleDestroy(sqlite3_tokenizer *pTokenizer){
  sqlite3_free(pTokenizer);
  return SQLITE_OK;
}

/*
** Prepare to begin tokenizing a particular string.  The input
** string to be tokenized is pInput[0..nBytes-1].  A cursor
** used to incrementally tokenize this string is returned in 
** *ppCursor.
*/
/*
** 准备开始标记一个特殊的字符串。
**被标记的输入字符串是pInput。
**增量标记此字符串的光标被返回到ppCursor。
*/
static int simpleOpen(
  sqlite3_tokenizer *pTokenizer,         /* The tokenizer */
  /*分词器 */
  const char *pInput, int nBytes,        /* String to be tokenized */
  /*将要被标记的字符串*/
  sqlite3_tokenizer_cursor **ppCursor    /* OUT: Tokenization cursor */
  /*指向标记光标*/
){
  simple_tokenizer_cursor *c;

  c = (simple_tokenizer_cursor *) sqlite3_malloc(sizeof(*c));
  if( c==NULL ) return SQLITE_NOMEM;

  c->pInput = pInput;
  if( pInput==0 ){
    c->nBytes = 0;
  }else if( nBytes<0 ){
    c->nBytes = (int)strlen(pInput);
  }else{
    c->nBytes = nBytes;
  }
  c->iOffset = 0;                 /* start tokenizing at the beginning */
  /*最先开始标记*/
  c->iToken = 0;
  c->pToken = NULL;               /* no space allocated, yet. */
  /*没有分配空间*/
  c->nTokenAllocated = 0;

  *ppCursor = &c->base;
  return SQLITE_OK;
}

/*
** Close a tokenization cursor previously opened by a call to
** simpleOpen() above.
*/
/*
**关闭标记光标之前打开调用simpleOpen()以上。
*/
static int simpleClose(sqlite3_tokenizer_cursor *pCursor){
  simple_tokenizer_cursor *c = (simple_tokenizer_cursor *) pCursor;
  sqlite3_free(c->pToken);
  sqlite3_free(c);
  return SQLITE_OK;
}

/*
** Extract the next token from a tokenization cursor.  The cursor must
** have been opened by a prior call to simpleOpen().
*/
/*
**从标记光标提取下一个标记。
**光标被打开之前必须调用simpleOpen()。
*/
static int simpleNext(
  sqlite3_tokenizer_cursor *pCursor,  /* Cursor returned by simpleOpen */
  /*光标由simpleOpen返回*/
  const char **ppToken,                    /* OUT: *ppToken is the token text */
  /*   *ppToken就是标记文本*/
  int *pnBytes,                                   /* OUT: Number of bytes in token */
  /*标记的字节数*/
  int *piStartOffset,                           /* OUT: Starting offset of token */
  /*标记的起始偏移量*/
  int *piEndOffset,                            /* OUT: Ending offset of token */
  /*标记的结束偏移量*/
  int *piPosition                               /* OUT: Position integer of token */
  /*标记的位置是整数*/
){
  simple_tokenizer_cursor *c = (simple_tokenizer_cursor *) pCursor;
  simple_tokenizer *t = (simple_tokenizer *) pCursor->pTokenizer;
  unsigned char *p = (unsigned char *)c->pInput;

  while( c->iOffset<c->nBytes ){
    int iStartOffset;

    /* Scan past delimiter characters */
	/*扫描过去分隔符字符*/
    while( c->iOffset<c->nBytes && simpleDelim(t, p[c->iOffset]) ){
      c->iOffset++;
    }

    /* Count non-delimiter characters. */
	/*对non-delimiter字符计数*/
    iStartOffset = c->iOffset;
    while( c->iOffset<c->nBytes && !simpleDelim(t, p[c->iOffset]) ){
      c->iOffset++;
    }

    if( c->iOffset>iStartOffset ){
      int i, n = c->iOffset-iStartOffset;
      if( n>c->nTokenAllocated ){
        c->nTokenAllocated = n+20;
        c->pToken = sqlite3_realloc(c->pToken, c->nTokenAllocated);
        if( c->pToken==NULL ) return SQLITE_NOMEM;
      }
      for(i=0; i<n; i++){
        /* TODO(shess) This needs expansion to handle UTF-8
        ** case-insensitivity.
        */
		/*
		**这里需要扩展对UTF-8大小写区分的处理
		*/
        unsigned char ch = p[iStartOffset+i];
        c->pToken[i] = (ch>='A' && ch<='Z') ? (ch - 'A' + 'a') : ch;
      }
      *ppToken = c->pToken;
      *pnBytes = n;
      *piStartOffset = iStartOffset;
      *piEndOffset = c->iOffset;
      *piPosition = c->iToken++;

      return SQLITE_OK;
    }
  }
  return SQLITE_DONE;
}

/*
** The set of routines that implement the simple tokenizer
*/
/*
一个实现简单编译器的声明集
*/
static const sqlite3_tokenizer_module simpleTokenizerModule = {
  0,
  simpleCreate,
  simpleDestroy,
  simpleOpen,
  simpleClose,
  simpleNext,
};

/*
** Allocate a new simple tokenizer.  Return a pointer to the new
** tokenizer in *ppModule
*/
/*
**分配一个新的简单分词器。
**在*ppModule里给新的分词器返回指针
*/
void sqlite3Fts2SimpleTokenizerModule(
  sqlite3_tokenizer_module const**ppModule
){
  *ppModule = &simpleTokenizerModule;
}

#endif /* !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_FTS2) */
/*SQLITE_CORE没有定义而定义了SQLITE_ENABLE_FTS2*/
