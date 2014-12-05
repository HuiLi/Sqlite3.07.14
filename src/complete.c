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
** An tokenizer for SQL一个SQL的定制 
**
** This file contains C code that implements the sqlite3_complete() API.此文件包含C代码实现sqlite3_complete()的API。
** This code used to be part of the tokenizer.c source file.  But by
** separating it out, the code will be automatically omitted from
** static links that do not use it.本代码用的是tokenizer.c源文件的一部分。但是分离出来了，该代码会自动从省略静态链接而是不使用它。
*/
#include "sqliteInt.h"
#ifndef SQLITE_OMIT_COMPLETE

/*
** This is defined in tokenize.c.  We just have to import the definition.这个在tokenize.c中定义，我们只是需要引入定义 
*/
#ifndef SQLITE_AMALGAMATION
#ifdef SQLITE_ASCII
#define IdChar(C)  ((sqlite3CtypeMap[(unsigned char)C]&0x46)!=0)
#endif
#ifdef SQLITE_EBCDIC
extern const char sqlite3IsEbcdicIdChar[];
#define IdChar(C)  (((c=C)>=0x42 && sqlite3IsEbcdicIdChar[c-0x40]))
#endif
#endif /* SQLITE_AMALGAMATION */


/*
** Token types used by the sqlite3_complete() routine.  See the header
** comments on that procedure for additional information.查看更多的信息，程序集的信息。
*/
#define tkSEMI    0
#define tkWS      1
#define tkOTHER   2
#ifndef SQLITE_OMIT_TRIGGER
#define tkEXPLAIN 3
#define tkCREATE  4
#define tkTEMP    5
#define tkTRIGGER 6
#define tkEND     7
#endif

/*
** Return TRUE if the given SQL string ends in a semicolon.如果给定的SQL字符串以分号结束返回true。
**
** Special handling is require for CREATE TRIGGER statements.特殊处理需要创建触发器语句。
** Whenever the CREATE TRIGGER keywords are seen, the statement
** must end with ";END;".每当创建被触发关键词，声明必须以“；END；”。
**
** This implementation uses a state machine with 8 states:该实现使用一个具有8个状态的状态机：
**
**   (0) INVALID   We have not yet seen a non-whitespace character.我们还没有看到一个非空格字符。
**
**   (1) START     At the beginning or end of an SQL statement.  This routine
**                 returns 1 if it ends in the START state and 0 if it ends
**                 in any other state.在一个SQL语句的开始或结束。本例程返回1，如果它结束在开始的状态或0，如果它结束在任何其他状态。
**
**   (2) NORMAL    We are in the middle of statement which ends with a single
**                 semicolon.我们在语句结束与一个单一的中间用分号。
**
**   (3) EXPLAIN   The keyword EXPLAIN has been seen at the beginning of 
**                 a statement. 关键词EXPLAIN在开始的时候就已经声明。
**
**   (4) CREATE    The keyword CREATE has been seen at the beginning of a
**                 statement, possibly preceeded by EXPLAIN and/or followed by
**                 TEMP or TEMPORARY   关键词CREATE已经在一开始声明被看到，可能的解释之前和/或之后临时的或临时的
**
**   (5) TRIGGER   We are in the middle of a trigger definition that must be
**                 ended by a semicolon, the keyword END, and another semicolon.我们是在一个触发器定义中，必须是以分号结束，关键字END，另一个分号。
**
**   (6) SEMI      We've seen the first semicolon in the ";END;" that occurs at
**                 the end of a trigger definition.我们已经看到分号在第一个分号结尾";END;" 一个触发器定义结束。
**
**   (7) END       We've seen the ";END" of the ";END;" that occurs at the end
**                 of a trigger difinition.我们已经看到了";END" ；“END；“末尾触发器的定义。
**
** Transitions between states above are determined by tokens extracted
** from the input.  The following tokens are significant:状态之间的转换是由标记提取确定以上从输入。下面的标记是重要的：
**
**   (0) tkSEMI      A semicolon.一个分号
**   (1) tkWS        Whitespace.空格。
**   (2) tkOTHER     Any other SQL token.任何其他SQL令牌  
**   (3) tkEXPLAIN   The "explain" keyword.关键字"explain"
**   (4) tkCREATE    The "create" keyword.关键字 "create"
**   (5) tkTEMP      The "temp" or "temporary" keyword.关键字 "temp"   "temporary" 
**   (6) tkTRIGGER   The "trigger" keyword.关键字"trigger" 
**   (7) tkEND       The "end" keyword.关键字"end"
**
** Whitespace never causes a state transition and is always ignored.空格不会导致状态转换，它常被忽略。
** This means that a SQL string of all whitespace is invalid.这意味着一个空白的SQL字符串无效。
**
** If we compile with SQLITE_OMIT_TRIGGER, all of the computation needed如果我们用sqlite_omit_trigger编译的，所有的计算需要，一个触发端可以省略。
** to recognize the end of a trigger can be omitted.  All we have to do
** is look for a semicolon that is not part of an string or comment.我们要做的就是找一个分号，不是一个字符串或注释的一部分。
*/
int sqlite3_complete(const char *zSql){
  u8 state = 0;   /* Current state, using numbers defined in header comment 用成员定义注释*/
  u8 token;       /* Value of the next token 下一个标记的价值*/

#ifndef SQLITE_OMIT_TRIGGER
  /* A complex statement machine used to detect the end of a CREATE TRIGGER复杂的语句机用于检测CREATE TRIGGER结束
  ** statement.  This is the normal case. 这是正常的情况下 
  */
  static const u8 trans[8][8] = {
                     /* Token:                                                */
     /* State:       **  SEMI  WS  OTHER  EXPLAIN  CREATE  TEMP  TRIGGER  END */
     /* 0 INVALID: */ {    1,  0,     2,       3,      4,    2,       2,   2, },
     /* 1   START: */ {    1,  1,     2,       3,      4,    2,       2,   2, },
     /* 2  NORMAL: */ {    1,  2,     2,       2,      2,    2,       2,   2, },
     /* 3 EXPLAIN: */ {    1,  3,     3,       2,      4,    2,       2,   2, },
     /* 4  CREATE: */ {    1,  4,     2,       2,      2,    4,       5,   2, },
     /* 5 TRIGGER: */ {    6,  5,     5,       5,      5,    5,       5,   5, },
     /* 6    SEMI: */ {    6,  6,     5,       5,      5,    5,       5,   7, },
     /* 7     END: */ {    1,  7,     5,       5,      5,    5,       5,   5, },
  };
#else
  /* If triggers are not supported by this compile then the statement machine
  ** used to detect the end of a statement is much simplier  如果触发器不支持这种编译，然后用于检测语句的结束声明的机器是非常简单的 
  */
  static const u8 trans[3][3] = {
                     /* Token:           */
     /* State:       **  SEMI  WS  OTHER */
     /* 0 INVALID: */ {    1,  0,     2, },
     /* 1   START: */ {    1,  1,     2, },
     /* 2  NORMAL: */ {    1,  2,     2, },
  };
#endif /* SQLITE_OMIT_TRIGGER */

  while( *zSql ){
    switch( *zSql ){
      case ';': {  /* A semicolon 分好*/
        token = tkSEMI;
        break;
      }
      case ' ':
      case '\r':
      case '\t':
      case '\n':
      case '\f': {  /* White space is ignored 空格被忽略*/
        token = tkWS;
        break;
      }
      case '/': {   /* C-style comments */
        if( zSql[1]!='*' ){
          token = tkOTHER;
          break;
        }
        zSql += 2;
        while( zSql[0] && (zSql[0]!='*' || zSql[1]!='/') ){ zSql++; }
        if( zSql[0]==0 ) return 0;
        zSql++;
        token = tkWS;
        break;
      }
      case '-': {   /* SQL-style comments from "--" to end of line */
        if( zSql[1]!='-' ){
          token = tkOTHER;
          break;
        }
        while( *zSql && *zSql!='\n' ){ zSql++; }
        if( *zSql==0 ) return state==1;
        token = tkWS;
        break;
      }
      case '[': {   /* Microsoft-style identifiers in [...] */
        zSql++;
        while( *zSql && *zSql!=']' ){ zSql++; }
        if( *zSql==0 ) return 0;
        token = tkOTHER;
        break;
      }
      case '`':     /* Grave-accent quoted symbols used by MySQL */
      case '"':     /* single- and double-quoted strings 单，双引号字符串*/
      case '\'': {
        int c = *zSql;
        zSql++;
        while( *zSql && *zSql!=c ){ zSql++; }
        if( *zSql==0 ) return 0;
        token = tkOTHER;
        break;
      }
      default: {
#ifdef SQLITE_EBCDIC
        unsigned char c;
#endif
        if( IdChar((u8)*zSql) ){
          /* Keywords and unquoted identifiers */
          int nId;
          for(nId=1; IdChar(zSql[nId]); nId++){}
#ifdef SQLITE_OMIT_TRIGGER
          token = tkOTHER;
#else
          switch( *zSql ){
            case 'c': case 'C': {
              if( nId==6 && sqlite3StrNICmp(zSql, "create", 6)==0 ){
                token = tkCREATE;
              }else{
                token = tkOTHER;
              }
              break;
            }
            case 't': case 'T': {
              if( nId==7 && sqlite3StrNICmp(zSql, "trigger", 7)==0 ){
                token = tkTRIGGER;
              }else if( nId==4 && sqlite3StrNICmp(zSql, "temp", 4)==0 ){
                token = tkTEMP;
              }else if( nId==9 && sqlite3StrNICmp(zSql, "temporary", 9)==0 ){
                token = tkTEMP;
              }else{
                token = tkOTHER;
              }
              break;
            }
            case 'e':  case 'E': {
              if( nId==3 && sqlite3StrNICmp(zSql, "end", 3)==0 ){
                token = tkEND;
              }else
#ifndef SQLITE_OMIT_EXPLAIN
              if( nId==7 && sqlite3StrNICmp(zSql, "explain", 7)==0 ){
                token = tkEXPLAIN;
              }else
#endif
              {
                token = tkOTHER;
              }
              break;
            }
            default: {
              token = tkOTHER;
              break;
            }
          }
#endif /* SQLITE_OMIT_TRIGGER */
          zSql += nId-1;
        }else{
          /* Operators and special symbols */
          token = tkOTHER;
        }
        break;
      }
    }
    state = trans[state][token];
    zSql++;
  }
  return state==1;
}

#ifndef SQLITE_OMIT_UTF16
/*
** This routine is the same as the sqlite3_complete() routine described
** above, except that the parameter is required to be UTF-16 encoded, not
** UTF-8.这个程序是相同描述的sqlite3_complete（）例程，所不同的是该参数需要是UTF-16进行编码，而不是UTF-8。
*/
int sqlite3_complete16(const void *zSql){
  sqlite3_value *pVal;
  char const *zSql8;
  int rc = SQLITE_NOMEM;

#ifndef SQLITE_OMIT_AUTOINIT
  rc = sqlite3_initialize();
  if( rc ) return rc;
#endif
  pVal = sqlite3ValueNew(0);
  sqlite3ValueSetStr(pVal, -1, zSql, SQLITE_UTF16NATIVE, SQLITE_STATIC);
  zSql8 = sqlite3ValueText(pVal, SQLITE_UTF8);
  if( zSql8 ){
    rc = sqlite3_complete(zSql8);
  }else{
    rc = SQLITE_NOMEM;
  }
  sqlite3ValueFree(pVal);
  return sqlite3ApiExit(0, rc);
}
#endif /* SQLITE_OMIT_UTF16 */
#endif /* SQLITE_OMIT_COMPLETE */
