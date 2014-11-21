/*
** 2002 February 23
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file contains the C functions that implement various SQL这个文件包含实现各种SQL函数的SQLite的C函数。
** functions of SQLite.  
**
** There is only one exported symbol in this file - the function
** sqliteRegisterBuildinFunctions() found at the bottom of the file.这个文件只有一个出口标志——函数sqliteRegisterBuildinFunctions()发现在文件的底部。
** All other code has file scope.
*/
#include "sqliteInt.h"
#include <stdlib.h>
#include <assert.h>//断言就是用于在代码中捕捉这些假设，可以将断言看作是异常处理的一种高级形式。
//断言表示为一些布尔表达式，程序员相信在程序中的某个特定点该表达式值为真。
//assert是宏，而不是函数。在C的assert.h头文件中。
//assert宏的原型定义在<assert.h>中，其作用是如果它的条件返回错误，则终止程序执行
#include "vdbeInt.h"

/*
** Return the collating function associated with a function.返回排序函数与函数有关。
*/
static CollSeq *sqlite3GetFuncCollSeq(sqlite3_context *context){
  return context->pColl;//返回排序的序列
}

/*
** Indicate that the accumulator load should be skipped on this
** iteration of the aggregate loop.表明累加器负载应该跳过这个迭代的总循环。。
*/
static void sqlite3SkipAccumulatorLoad(sqlite3_context *context){
  context->skipFlag = 1;//跳转标志为1
}

/*
** Implementation of the non-aggregate min() and max() functions
实现非聚合min()和max()函数
*/
static void minmaxFunc(
  sqlite3_context *context,?													//函数上下文
  int argc,
  sqlite3_value **argv
){
  int i;
  int mask;    /* 0 for min() or 0xffffffff for max();0为最小,-1为最大
  因为int 为带符号类型,带符号类型最高为是符号位
  ,又因为0xFFFFFFFF,也就是四个字节32 bits全是1, 符号位是1,所以这个数是负数 */
  int iBest;
  CollSeq *pColl;

  assert( argc>1 );																	                                //如果 argc>1，则继续执行，否则终止程序执行
  mask = sqlite3_user_data(context)==0 ? 0 : -1;								                  	//判断最小最大值
  pColl = sqlite3GetFuncCollSeq(context);
  assert( pColl );
  assert( mask==-1 || mask==0 );													                          //断言最大值是-1或者最小值是0
  iBest = 0;
  if( sqlite3_value_type(argv[0])==SQLITE_NULL ) return;							              //自定义接口函数
  for(i=1; i<argc; i++){
    if( sqlite3_value_type(argv[i])==SQLITE_NULL ) return;
    if( (sqlite3MemCompare(argv[iBest], argv[i], pColl)^mask)>=0 ){					        //比较大小
      testcase( mask==0 );
      iBest = i;
    }
  }
  sqlite3_result_value(context, argv[iBest]);									                    	//返回最值
}

/*
** Return the type of the argument.返回的类型参数。
*/
static void typeofFunc(
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **argv
){
  const char *z = 0;
  UNUSED_PARAMETER(NotUsed);//从未用过的参数
  switch( sqlite3_value_type(argv[0]) ){
    case SQLITE_INTEGER: z = "integer"; break;									                  	//带符号整型
    case SQLITE_TEXT:    z = "text";    break;									                  	//字符串文本
    case SQLITE_FLOAT:   z = "real";    break;									                   	//浮点数
    case SQLITE_BLOB:    z = "blob";    break;									                  	//二进制大对象
    default:             z = "null";    break;									                   	//空值
  }
  sqlite3_result_text(context, z, -1, SQLITE_STATIC);							                	//返回结果
}


/*
** Implementation of the length() function长度函数的实现
*/
static void lengthFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  int len;

  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  switch( sqlite3_value_type(argv[0]) ){
    case SQLITE_BLOB:														                                    	//二进制
    case SQLITE_INTEGER:				                                                     	//整型
    case SQLITE_FLOAT: {														                                	//浮点数
      sqlite3_result_int(context, sqlite3_value_bytes(argv[0]));				            	//返回标量值
      break;
    }
    case SQLITE_TEXT: {															                              	  //文本
      const unsigned char *z = sqlite3_value_text(argv[0]);
      if( z==0 ) return;														                              	  //长度为0，返回0
      len = 0;
      while( *z ){
        len++;
        SQLITE_SKIP_UTF8(z);
      }
      sqlite3_result_int(context, len);											                  	   //返回长度
      break;
    }
    default: {
      sqlite3_result_null(context);												                      	//返回空值
      break;
    }
  }
}

/*
** Implementation of the abs() function.abs函数的实现。
**
** IMP: R-23979-26855 The abs(X) function returns the absolute value of
** the numeric argument X. abs(X)函数返回数值的绝对值参数X
*/  
static void absFunc(sqlite3_context *context, int argc, sqlite3_value **argv){	  //绝对值函数
  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  switch( sqlite3_value_type(argv[0]) ){
    case SQLITE_INTEGER: {
      i64 iVal = sqlite3_value_int64(argv[0]);
      if( iVal<0 ){                                                              	//判断如果ival小于0
        if( (iVal<<1)==0 ){
          /* IMP: R-35460-15084 If X is the integer -9223372036854775807 then
          ** abs(X) throws an integer overflow error since there is no
          ** equivalent positive 64-bit two complement value. 如果X是整数-9223372036854775807
          那么abs(X)抛出一个整数溢出错误,  因为没有积极的64位两个补充值。*/
          sqlite3_result_error(context, "integer overflow", -1);                	//返回错误，整型溢出
          return;
        }
        iVal = -iVal;                                                          		//取ival的负赋值给ival
      } 
      sqlite3_result_int64(context, iVal);
      break;
    }
    case SQLITE_NULL: {
      /* IMP: R-37434-19929 Abs(X) returns NULL if X is NULL. 如果x是空，则返回绝对值为空*/
      sqlite3_result_null(context);
      break;
    }
    default: {
      /* Because sqlite3_value_double() returns 0.0 if the argument is not
      ** something that can be converted into a number, we have:
      ** IMP: R-57326-31541 Abs(X) return 0.0 if X is a string or blob that
      ** cannot be converted to a numeric value.
      因为sqlite3_value_double()返回0.如果这个内容不可以被转换成一个数字值，
      Abs(X)返回0，如果X是一个字符串或blob,不能被转换成数字值。
      */
      double rVal = sqlite3_value_double(argv[0]);
      if( rVal<0 ) rVal = -rVal;													                         //如果rval小于0，取rval的负赋值给rval
      sqlite3_result_double(context, rVal);										                  	//返回绝对值
      break;
    }
  }
}

/*
** Implementation of the substr() function.substr()函数的实现substr 取部份字符串string substr(string string, int start, int [length])
**
** substr(x,p1,p2)  returns p2 characters of x[] beginning with p1.
** p1 is 1-indexed.  So substr(x,1,1) returns the first character
** of x.  If x is text, then we actually count UTF-8 characters.
** If x is a blob, then we count bytes.
**
** If p1 is negative, then we begin abs(p1) from the end of x[].
**
** If p2 is negative, return the p2 characters preceeding p1.
substr(x,p1,p2)是返回从p1开始查找x[]中第p2的字符
。所以substr(x,1,1)返回第一个字符x,
如果x是文本,那么我们实际上计算utf - 8字符数。
如果x是一个blob,那么我们计数字节。
如果p1是负的,那么我们开始从abs(p1)中x[]尾端算起。
如果p2是负的,返回倒数第p2个字符。
*/
static void substrFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  const unsigned char *z;
  const unsigned char *z2;
  int len;
  int p0type;
  i64 p1, p2;
  int negP2 = 0;

  assert( argc==3 || argc==2 );
  if( sqlite3_value_type(argv[1])==SQLITE_NULL
   || (argc==3 && sqlite3_value_type(argv[2])==SQLITE_NULL)							//判断为空值的情况
  ){
    return;																		                        	//返回空
  }
  p0type = sqlite3_value_type(argv[0]);
  p1 = sqlite3_value_int(argv[1]);
  if( p0type==SQLITE_BLOB ){													                	//判断取值是否为二进制
    len = sqlite3_value_bytes(argv[0]);
    z = sqlite3_value_blob(argv[0]);
    if( z==0 ) return;
    assert( len==sqlite3_value_bytes(argv[0]) );
  }else{																		                          	//若z是文本			
    z = sqlite3_value_text(argv[0]);
    if( z==0 ) return;
    len = 0;
    if( p1<0 ){																		                      //若p1为负
      for(z2=z; *z2; len++){
        SQLITE_SKIP_UTF8(z2);														
      }
    }
  }
  if( argc==3 ){
    p2 = sqlite3_value_int(argv[2]);												
    if( p2<0 ){																		
      p2 = -p2;
      negP2 = 1;
    }
  }else{
    p2 = sqlite3_context_db_handle(context)->aLimit[SQLITE_LIMIT_LENGTH];
  }
  if( p1<0 ){
    p1 += len;
    if( p1<0 ){
      p2 += p1;
      if( p2<0 ) p2 = 0;
      p1 = 0;
    }
  }else if( p1>0 ){
    p1--;
  }else if( p2>0 ){
    p2--;
  }
  if( negP2 ){
    p1 -= p2;
    if( p1<0 ){
      p2 += p1;
      p1 = 0;
    }
  }
  assert( p1>=0 && p2>=0 );
  if( p0type!=SQLITE_BLOB ){
    while( *z && p1 ){
      SQLITE_SKIP_UTF8(z);
      p1--;
    }
    for(z2=z; *z2 && p2; p2--){
      SQLITE_SKIP_UTF8(z2);
    }
    sqlite3_result_text(context, (char*)z, (int)(z2-z), SQLITE_TRANSIENT);
  }else{
    if( p1+p2>len ){
      p2 = len-p1;
      if( p2<0 ) p2 = 0;
    }
    sqlite3_result_blob(context, (char*)&z[p1], (int)p2, SQLITE_TRANSIENT);
  }
}

/*
** Implementation of the round() function
返回数字表达式并四舍五入为指定的长度或精度。
*/
#ifndef SQLITE_OMIT_FLOATING_POINT
static void roundFunc(sqlite3_context *context, int argc, sqlite3_value **argv){		
  int n = 0;
  double r;
  char *zBuf;
  assert( argc==1 || argc==2 );
  if( argc==2 ){
    if( SQLITE_NULL==sqlite3_value_type(argv[1]) ) return;								//先判断为空值的情况
    n = sqlite3_value_int(argv[1]);
    if( n>30 ) n = 30;
    if( n<0 ) n = 0;
  }
  if( sqlite3_value_type(argv[0])==SQLITE_NULL ) return;
  r = sqlite3_value_double(argv[0]);
  /* If Y==0 and X will fit in a 64-bit int
  ** handle the rounding directly,
  ** otherwise use printf.
  如果Y = X = 0,适合在一个64位整数,
直接处理输出
否则使用printf。
  */
  if( n==0 && r>=0 && r<LARGEST_INT64-1 ){
    r = (double)((sqlite_int64)(r+0.5));
  }else if( n==0 && r<0 && (-r)<LARGEST_INT64-1 ){
    r = -(double)((sqlite_int64)((-r)+0.5));
  }else{
    zBuf = sqlite3_mprintf("%.*f",n,r);
    if( zBuf==0 ){
      sqlite3_result_error_nomem(context);
      return;
    }
    sqlite3AtoF(zBuf, &r, sqlite3Strlen30(zBuf), SQLITE_UTF8);
    sqlite3_free(zBuf);
  }
  sqlite3_result_double(context, r);
}
#endif

/*
** Allocate nByte bytes of space using sqlite3_malloc(). If the
** allocation fails, call sqlite3_result_error_nomem() to notify
** the database handle that malloc() has failed and return NULL.
** If nByte is larger than the maximum string or blob length, then
** raise an SQLITE_TOOBIG exception and return NULL.
分配nByte字节的空间使用sqlite3_malloc()。
如果分配失败,调用sqlite3_result_error_nomem()通知数据库句柄,malloc()失败,返回NULL。
如果nByte大于最大字符串或blob长度,然后引发一个SQLITE_TOOBIG异常,返回NULL。
*/
static void *contextMalloc(sqlite3_context *context, i64 nByte){					//该函数分配了NBytes个字节,并返回了指向这块内存的指针
																					                                //如果分配失败，则返回一个空指针null
  char *z;
  sqlite3 *db = sqlite3_context_db_handle(context);
  assert( nByte>0 );
  testcase( nByte==db->aLimit[SQLITE_LIMIT_LENGTH] );
  testcase( nByte==db->aLimit[SQLITE_LIMIT_LENGTH]+1 );
  if( nByte>db->aLimit[SQLITE_LIMIT_LENGTH] ){									        	//如果nByte大于最大字符串或blob长度,然后引发一个SQLITE_TOOBIG异常,返回NULL。
    sqlite3_result_error_toobig(context);
    z = 0;
  }else{
    z = sqlite3Malloc((int)nByte);
    if( !z ){
      sqlite3_result_error_nomem(context);											//如果分配失败,调用sqlite3_result_error_nomem()通知数据库句柄,malloc()失败,返回NULL?
    }
  }
  return z;
}

/*
** Implementation of the upper() and lower() SQL functions.
*/
static void upperFunc(sqlite3_context *context, int argc, sqlite3_value **argv){
  char *z1;
  const char *z2;
  int i, n;
  UNUSED_PARAMETER(argc);
  z2 = (char*)sqlite3_value_text(argv[0]);
  n = sqlite3_value_bytes(argv[0]);
  /* Verify that the call to _bytes() does not invalidate the _text() pointer */
  assert( z2==(char*)sqlite3_value_text(argv[0]) );
  if( z2 ){
    z1 = contextMalloc(context, ((i64)n)+1);
    if( z1 ){
      for(i=0; i<n; i++){
        z1[i] = (char)sqlite3Toupper(z2[i]);
      }
      sqlite3_result_text(context, z1, n, sqlite3_free);
    }
  }
}
static void lowerFunc(sqlite3_context *context, int argc, sqlite3_value **argv){
  char *z1;
  const char *z2;
  int i, n;
  UNUSED_PARAMETER(argc);
  z2 = (char*)sqlite3_value_text(argv[0]);
  n = sqlite3_value_bytes(argv[0]);
  /* Verify that the call to _bytes() does not invalidate the _text() pointer */
  assert( z2==(char*)sqlite3_value_text(argv[0]) );
  if( z2 ){
    z1 = contextMalloc(context, ((i64)n)+1);
    if( z1 ){
      for(i=0; i<n; i++){
        z1[i] = sqlite3Tolower(z2[i]);
      }
      sqlite3_result_text(context, z1, n, sqlite3_free);
    }
  }
}


#if 0  /* This function is never used. */
/*
** The COALESCE() and IFNULL() functions used to be implemented as shown
** here.  But now they are implemented as VDBE code so that unused arguments
** do not have to be computed.  This legacy implementation is retained as
** comment.
*/
/*
** Implementation of the IFNULL(), NVL(), and COALESCE() functions.  
** All three do the same thing.  They return the first non-NULL
** argument.
*/
static void ifnullFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  int i;
  for(i=0; i<argc; i++){
    if( SQLITE_NULL!=sqlite3_value_type(argv[i]) ){
      sqlite3_result_value(context, argv[i]);
      break;
    }
  }
}
#endif /* NOT USED */
#define ifnullFunc versionFunc   /* Substitute function - never called */

/*
** Implementation of random().  Return a random integer.  
*/
static void randomFunc(
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **NotUsed2
){
  sqlite_int64 r;
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  sqlite3_randomness(sizeof(r), &r);
  if( r<0 ){
    /* We need to prevent a random number of 0x8000000000000000 
    ** (or -9223372036854775808) since when you do abs() of that
    ** number of you get the same value back again.  To do this
    ** in a way that is testable, mask the sign bit off of negative
    ** values, resulting in a positive value.  Then take the 
    ** 2s complement of that positive value.  The end result can
    ** therefore be no less than -9223372036854775807.
    */
    r = -(r & LARGEST_INT64);
  }
  sqlite3_result_int64(context, r);
}

/*
** Implementation of randomblob(N).  Return a random blob
** that is N bytes long.
*/
static void randomBlob(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  int n;
  unsigned char *p;
  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  n = sqlite3_value_int(argv[0]);
  if( n<1 ){
    n = 1;
  }
  p = contextMalloc(context, n);
  if( p ){
    sqlite3_randomness(n, p);
    sqlite3_result_blob(context, (char*)p, n, sqlite3_free);
  }
}

/*
** Implementation of the last_insert_rowid() SQL function.  The return
** value is the same as the sqlite3_last_insert_rowid() API function.
*/
static void last_insert_rowid(
  sqlite3_context *context, 
  int NotUsed, 
  sqlite3_value **NotUsed2
){
  sqlite3 *db = sqlite3_context_db_handle(context);
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  /* IMP: R-51513-12026 The last_insert_rowid() SQL function is a
  ** wrapper around the sqlite3_last_insert_rowid() C/C++ interface
  ** function. */
  sqlite3_result_int64(context, sqlite3_last_insert_rowid(db));
}

/*
** Implementation of the changes() SQL function.
**
** IMP: R-62073-11209 The changes() SQL function is a wrapper
** around the sqlite3_changes() C/C++ function and hence follows the same
** rules for counting changes.
*/
static void changes(
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **NotUsed2
){
  sqlite3 *db = sqlite3_context_db_handle(context);
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  sqlite3_result_int(context, sqlite3_changes(db));
}

/*
** Implementation of the total_changes() SQL function.  The return value is
** the same as the sqlite3_total_changes() API function.
*/
static void total_changes(
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **NotUsed2
){
  sqlite3 *db = sqlite3_context_db_handle(context);
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  /* IMP: R-52756-41993 This function is a wrapper around the
  ** sqlite3_total_changes() C/C++ interface. */
  sqlite3_result_int(context, sqlite3_total_changes(db));
}

/*
** A structure defining how to do GLOB-style comparisons.
*/
struct compareInfo {
  u8 matchAll;
  u8 matchOne;
  u8 matchSet;
  u8 noCase;
};

/*
** For LIKE and GLOB matching on EBCDIC machines, assume that every
** character is exactly one byte in size.  Also, all characters are
** able to participate in upper-case-to-lower-case mappings in EBCDIC
** whereas only characters less than 0x80 do in ASCII.
*/
#if defined(SQLITE_EBCDIC)
# define sqlite3Utf8Read(A,C)  (*(A++))
# define GlogUpperToLower(A)   A = sqlite3UpperToLower[A]
#else
# define GlogUpperToLower(A)   if( !((A)&~0x7f) ){ A = sqlite3UpperToLower[A]; }
#endif

static const struct compareInfo globInfo = { '*', '?', '[', 0 };
/* The correct SQL-92 behavior is for the LIKE operator to ignore
** case.  Thus  'a' LIKE 'A' would be true. */
static const struct compareInfo likeInfoNorm = { '%', '_',   0, 1 };
/* If SQLITE_CASE_SENSITIVE_LIKE is defined, then the LIKE operator
** is case sensitive causing 'a' LIKE 'A' to be false */
static const struct compareInfo likeInfoAlt = { '%', '_',   0, 0 };

/*
** Compare two UTF-8 strings for equality where the first string can
** potentially be a "glob" expression.  Return true (1) if they
** are the same and false (0) if they are different.
**
** Globbing rules:
**
**      '*'       Matches any sequence of zero or more characters.
**
**      '?'       Matches exactly one character.
**
**     [...]      Matches one character from the enclosed list of
**                characters.
**
**     [^...]     Matches one character not in the enclosed list.
**
** With the [...] and [^...] matching, a ']' character can be included
** in the list by making it the first character after '[' or '^'.  A
** range of characters can be specified using '-'.  Example:
** "[a-z]" matches any single lower-case letter.  To match a '-', make
** it the last character in the list.
**
** This routine is usually quick, but can be N**2 in the worst case.
**
** Hints: to match '*' or '?', put them in "[]".  Like this:
**
**         abc[*]xyz        Matches "abc*xyz" only
*/
static int patternCompare(
  const u8 *zPattern,              /* The glob pattern */
  const u8 *zString,               /* The string to compare against the glob */
  const struct compareInfo *pInfo, /* Information about how to do the compare */
  u32 esc                          /* The escape character */
){
  u32 c, c2;
  int invert;
  int seen;
  u8 matchOne = pInfo->matchOne;
  u8 matchAll = pInfo->matchAll;
  u8 matchSet = pInfo->matchSet;
  u8 noCase = pInfo->noCase; 
  int prevEscape = 0;     /* True if the previous character was 'escape' */

  while( (c = sqlite3Utf8Read(zPattern,&zPattern))!=0 ){
    if( !prevEscape && c==matchAll ){
      while( (c=sqlite3Utf8Read(zPattern,&zPattern)) == matchAll
               || c == matchOne ){
        if( c==matchOne && sqlite3Utf8Read(zString, &zString)==0 ){
          return 0;
        }
      }
      if( c==0 ){
        return 1;
      }else if( c==esc ){
        c = sqlite3Utf8Read(zPattern, &zPattern);
        if( c==0 ){
          return 0;
        }
      }else if( c==matchSet ){
        assert( esc==0 );         /* This is GLOB, not LIKE */
        assert( matchSet<0x80 );  /* '[' is a single-byte character */
        while( *zString && patternCompare(&zPattern[-1],zString,pInfo,esc)==0 ){
          SQLITE_SKIP_UTF8(zString);
        }
        return *zString!=0;
      }
      while( (c2 = sqlite3Utf8Read(zString,&zString))!=0 ){
        if( noCase ){
          GlogUpperToLower(c2);
          GlogUpperToLower(c);
          while( c2 != 0 && c2 != c ){
            c2 = sqlite3Utf8Read(zString, &zString);
            GlogUpperToLower(c2);
          }
        }else{
          while( c2 != 0 && c2 != c ){
            c2 = sqlite3Utf8Read(zString, &zString);
          }
        }
        if( c2==0 ) return 0;
        if( patternCompare(zPattern,zString,pInfo,esc) ) return 1;
      }
      return 0;
    }else if( !prevEscape && c==matchOne ){
      if( sqlite3Utf8Read(zString, &zString)==0 ){
        return 0;
      }
    }else if( c==matchSet ){
      u32 prior_c = 0;
      assert( esc==0 );    /* This only occurs for GLOB, not LIKE */
      seen = 0;
      invert = 0;
      c = sqlite3Utf8Read(zString, &zString);
      if( c==0 ) return 0;
      c2 = sqlite3Utf8Read(zPattern, &zPattern);
      if( c2=='^' ){
        invert = 1;
        c2 = sqlite3Utf8Read(zPattern, &zPattern);
      }
      if( c2==']' ){
        if( c==']' ) seen = 1;
        c2 = sqlite3Utf8Read(zPattern, &zPattern);
      }
      while( c2 && c2!=']' ){
        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){
          c2 = sqlite3Utf8Read(zPattern, &zPattern);
          if( c>=prior_c && c<=c2 ) seen = 1;
          prior_c = 0;
        }else{
          if( c==c2 ){
            seen = 1;
          }
          prior_c = c2;
        }
        c2 = sqlite3Utf8Read(zPattern, &zPattern);
      }
      if( c2==0 || (seen ^ invert)==0 ){
        return 0;
      }
    }else if( esc==c && !prevEscape ){
      prevEscape = 1;
    }else{
      c2 = sqlite3Utf8Read(zString, &zString);
      if( noCase ){
        GlogUpperToLower(c);
        GlogUpperToLower(c2);
      }
      if( c!=c2 ){
        return 0;
      }
      prevEscape = 0;
    }
  }
  return *zString==0;
}

/*
** Count the number of times that the LIKE operator (or GLOB which is
** just a variation of LIKE) gets called.  This is used for testing
** only.
*/
#ifdef SQLITE_TEST
int sqlite3_like_count = 0;
#endif


/*
** Implementation of the like() SQL function.  This function implements
** the build-in LIKE operator.  The first argument to the function is the
** pattern and the second argument is the string.  So, the SQL statements:
**
**       A LIKE B
**
** is implemented as like(B,A).
**
** This same function (with a different compareInfo structure) computes
** the GLOB operator.
*/
static void likeFunc(
  sqlite3_context *context, 
  int argc, 
  sqlite3_value **argv
){
  const unsigned char *zA, *zB;
  u32 escape = 0;
  int nPat;
  sqlite3 *db = sqlite3_context_db_handle(context);

  zB = sqlite3_value_text(argv[0]);
  zA = sqlite3_value_text(argv[1]);

  /* Limit the length of the LIKE or GLOB pattern to avoid problems
  ** of deep recursion and N*N behavior in patternCompare().
  */
  nPat = sqlite3_value_bytes(argv[0]);
  testcase( nPat==db->aLimit[SQLITE_LIMIT_LIKE_PATTERN_LENGTH] );
  testcase( nPat==db->aLimit[SQLITE_LIMIT_LIKE_PATTERN_LENGTH]+1 );
  if( nPat > db->aLimit[SQLITE_LIMIT_LIKE_PATTERN_LENGTH] ){
    sqlite3_result_error(context, "LIKE or GLOB pattern too complex", -1);
    return;
  }
  assert( zB==sqlite3_value_text(argv[0]) );  /* Encoding did not change */

  if( argc==3 ){
    /* The escape character string must consist of a single UTF-8 character.
    ** Otherwise, return an error.
    */
    const unsigned char *zEsc = sqlite3_value_text(argv[2]);
    if( zEsc==0 ) return;
    if( sqlite3Utf8CharLen((char*)zEsc, -1)!=1 ){
      sqlite3_result_error(context, 
          "ESCAPE expression must be a single character", -1);
      return;
    }
    escape = sqlite3Utf8Read(zEsc, &zEsc);
  }
  if( zA && zB ){
    struct compareInfo *pInfo = sqlite3_user_data(context);
#ifdef SQLITE_TEST
    sqlite3_like_count++;
#endif
    
    sqlite3_result_int(context, patternCompare(zB, zA, pInfo, escape));
  }
}

/*
** Implementation of the NULLIF(x,y) function.  The result is the first
** argument if the arguments are different.  The result is NULL if the
** arguments are equal to each other.
*/
static void nullifFunc(
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **argv
){
  CollSeq *pColl = sqlite3GetFuncCollSeq(context);
  UNUSED_PARAMETER(NotUsed);
  if( sqlite3MemCompare(argv[0], argv[1], pColl)!=0 ){
    sqlite3_result_value(context, argv[0]);
  }
}

/*
** Implementation of the sqlite_version() function.  The result is the version
** of the SQLite library that is running.
*/
static void versionFunc(
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **NotUsed2
){
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  /* IMP: R-48699-48617 This function is an SQL wrapper around the
  ** sqlite3_libversion() C-interface. */
  sqlite3_result_text(context, sqlite3_libversion(), -1, SQLITE_STATIC);
}

/*
** Implementation of the sqlite_source_id() function. The result is a string
** that identifies the particular version of the source code used to build
** SQLite.
*/
static void sourceidFunc(
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **NotUsed2
){
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  /* IMP: R-24470-31136 This function is an SQL wrapper around the
  ** sqlite3_sourceid() C interface. */
  sqlite3_result_text(context, sqlite3_sourceid(), -1, SQLITE_STATIC);
}

/*
** Implementation of the sqlite_log() function.  This is a wrapper around
** sqlite3_log().  The return value is NULL.  The function exists purely for
** its side-effects.
*/
static void errlogFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  UNUSED_PARAMETER(argc);
  UNUSED_PARAMETER(context);
  sqlite3_log(sqlite3_value_int(argv[0]), "%s", sqlite3_value_text(argv[1]));
}

/*
** Implementation of the sqlite_compileoption_used() function.
** The result is an integer that identifies if the compiler option
** was used to build SQLite.
*/
#ifndef SQLITE_OMIT_COMPILEOPTION_DIAGS
static void compileoptionusedFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  const char *zOptName;
  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  /* IMP: R-39564-36305 The sqlite_compileoption_used() SQL
  ** function is a wrapper around the sqlite3_compileoption_used() C/C++
  ** function.
  */
  if( (zOptName = (const char*)sqlite3_value_text(argv[0]))!=0 ){
    sqlite3_result_int(context, sqlite3_compileoption_used(zOptName));
  }
}
#endif /* SQLITE_OMIT_COMPILEOPTION_DIAGS */

/*
** Implementation of the sqlite_compileoption_get() function. 
** The result is a string that identifies the compiler options 
** used to build SQLite.
*/
#ifndef SQLITE_OMIT_COMPILEOPTION_DIAGS
static void compileoptiongetFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  int n;
  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  /* IMP: R-04922-24076 The sqlite_compileoption_get() SQL function
  ** is a wrapper around the sqlite3_compileoption_get() C/C++ function.
  */
  n = sqlite3_value_int(argv[0]);
  sqlite3_result_text(context, sqlite3_compileoption_get(n), -1, SQLITE_STATIC);
}
#endif /* SQLITE_OMIT_COMPILEOPTION_DIAGS */

/* Array for converting from half-bytes (nybbles) into ASCII hex
** digits. */
static const char hexdigits[] = {
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' 
};

/*
** EXPERIMENTAL - This is not an official function.  The interface may
** change.  This function may disappear.  Do not write code that depends
** on this function.
**
** Implementation of the QUOTE() function.  This function takes a single
** argument.  If the argument is numeric, the return value is the same as
** the argument.  If the argument is NULL, the return value is the string
** "NULL".  Otherwise, the argument is enclosed in single quotes with
** single-quote escapes.
*/
static void quoteFunc(sqlite3_context *context, int argc, sqlite3_value **argv){
  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  switch( sqlite3_value_type(argv[0]) ){
    case SQLITE_FLOAT: {
      double r1, r2;
      char zBuf[50];
      r1 = sqlite3_value_double(argv[0]);
      sqlite3_snprintf(sizeof(zBuf), zBuf, "%!.15g", r1);
      sqlite3AtoF(zBuf, &r2, 20, SQLITE_UTF8);
      if( r1!=r2 ){
        sqlite3_snprintf(sizeof(zBuf), zBuf, "%!.20e", r1);
      }
      sqlite3_result_text(context, zBuf, -1, SQLITE_TRANSIENT);
      break;
    }
    case SQLITE_INTEGER: {
      sqlite3_result_value(context, argv[0]);
      break;
    }
    case SQLITE_BLOB: {
      char *zText = 0;
      char const *zBlob = sqlite3_value_blob(argv[0]);
      int nBlob = sqlite3_value_bytes(argv[0]);
      assert( zBlob==sqlite3_value_blob(argv[0]) ); /* No encoding change */
      zText = (char *)contextMalloc(context, (2*(i64)nBlob)+4); 
      if( zText ){
        int i;
        for(i=0; i<nBlob; i++){
          zText[(i*2)+2] = hexdigits[(zBlob[i]>>4)&0x0F];
          zText[(i*2)+3] = hexdigits[(zBlob[i])&0x0F];
        }
        zText[(nBlob*2)+2] = '\'';
        zText[(nBlob*2)+3] = '\0';
        zText[0] = 'X';
        zText[1] = '\'';
        sqlite3_result_text(context, zText, -1, SQLITE_TRANSIENT);
        sqlite3_free(zText);
      }
      break;
    }
    case SQLITE_TEXT: {
      int i,j;
      u64 n;
      const unsigned char *zArg = sqlite3_value_text(argv[0]);
      char *z;

      if( zArg==0 ) return;
      for(i=0, n=0; zArg[i]; i++){ if( zArg[i]=='\'' ) n++; }
      z = contextMalloc(context, ((i64)i)+((i64)n)+3);
      if( z ){
        z[0] = '\'';
        for(i=0, j=1; zArg[i]; i++){
          z[j++] = zArg[i];
          if( zArg[i]=='\'' ){
            z[j++] = '\'';
          }
        }
        z[j++] = '\'';
        z[j] = 0;
        sqlite3_result_text(context, z, j, sqlite3_free);
      }
      break;
    }
    default: {
      assert( sqlite3_value_type(argv[0])==SQLITE_NULL );
      sqlite3_result_text(context, "NULL", 4, SQLITE_STATIC);
      break;
    }
  }
}

/*
** The hex() function.  Interpret the argument as a blob.  Return
** a hexadecimal rendering as text.
*/
static void hexFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  int i, n;
  const unsigned char *pBlob;
  char *zHex, *z;
  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  pBlob = sqlite3_value_blob(argv[0]);
  n = sqlite3_value_bytes(argv[0]);
  assert( pBlob==sqlite3_value_blob(argv[0]) );  /* No encoding change */
  z = zHex = contextMalloc(context, ((i64)n)*2 + 1);
  if( zHex ){
    for(i=0; i<n; i++, pBlob++){
      unsigned char c = *pBlob;
      *(z++) = hexdigits[(c>>4)&0xf];
      *(z++) = hexdigits[c&0xf];
    }
    *z = 0;
    sqlite3_result_text(context, zHex, n*2, sqlite3_free);
  }
}

/*
** The zeroblob(N) function returns a zero-filled blob of size N bytes.
*/
static void zeroblobFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  i64 n;
  sqlite3 *db = sqlite3_context_db_handle(context);
  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  n = sqlite3_value_int64(argv[0]);
  testcase( n==db->aLimit[SQLITE_LIMIT_LENGTH] );
  testcase( n==db->aLimit[SQLITE_LIMIT_LENGTH]+1 );
  if( n>db->aLimit[SQLITE_LIMIT_LENGTH] ){
    sqlite3_result_error_toobig(context);
  }else{
    sqlite3_result_zeroblob(context, (int)n); /* IMP: R-00293-64994 */
  }
}

/*
** The replace() function.  Three arguments are all strings: call
** them A, B, and C. The result is also a string which is derived
** from A by replacing every occurance of B with C.  The match
** must be exact.  Collating sequences are not used.
*/
static void replaceFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  const unsigned char *zStr;        /* The input string A */
  const unsigned char *zPattern;    /* The pattern string B */
  const unsigned char *zRep;        /* The replacement string C */
  unsigned char *zOut;              /* The output */
  int nStr;                /* Size of zStr */
  int nPattern;            /* Size of zPattern */
  int nRep;                /* Size of zRep */
  i64 nOut;                /* Maximum size of zOut */
  int loopLimit;           /* Last zStr[] that might match zPattern[] */
  int i, j;                /* Loop counters */

  assert( argc==3 );
  UNUSED_PARAMETER(argc);
  zStr = sqlite3_value_text(argv[0]);
  if( zStr==0 ) return;
  nStr = sqlite3_value_bytes(argv[0]);
  assert( zStr==sqlite3_value_text(argv[0]) );  /* No encoding change */
  zPattern = sqlite3_value_text(argv[1]);
  if( zPattern==0 ){
    assert( sqlite3_value_type(argv[1])==SQLITE_NULL
            || sqlite3_context_db_handle(context)->mallocFailed );
    return;
  }
  if( zPattern[0]==0 ){
    assert( sqlite3_value_type(argv[1])!=SQLITE_NULL );
    sqlite3_result_value(context, argv[0]);
    return;
  }
  nPattern = sqlite3_value_bytes(argv[1]);
  assert( zPattern==sqlite3_value_text(argv[1]) );  /* No encoding change */
  zRep = sqlite3_value_text(argv[2]);
  if( zRep==0 ) return;
  nRep = sqlite3_value_bytes(argv[2]);
  assert( zRep==sqlite3_value_text(argv[2]) );
  nOut = nStr + 1;
  assert( nOut<SQLITE_MAX_LENGTH );
  zOut = contextMalloc(context, (i64)nOut);
  if( zOut==0 ){
    return;
  }
  loopLimit = nStr - nPattern;  
  for(i=j=0; i<=loopLimit; i++){
    if( zStr[i]!=zPattern[0] || memcmp(&zStr[i], zPattern, nPattern) ){
      zOut[j++] = zStr[i];
    }else{
      u8 *zOld;
      sqlite3 *db = sqlite3_context_db_handle(context);
      nOut += nRep - nPattern;
      testcase( nOut-1==db->aLimit[SQLITE_LIMIT_LENGTH] );
      testcase( nOut-2==db->aLimit[SQLITE_LIMIT_LENGTH] );
      if( nOut-1>db->aLimit[SQLITE_LIMIT_LENGTH] ){
        sqlite3_result_error_toobig(context);
        sqlite3_free(zOut);
        return;
      }
      zOld = zOut;
      zOut = sqlite3_realloc(zOut, (int)nOut);
      if( zOut==0 ){
        sqlite3_result_error_nomem(context);
        sqlite3_free(zOld);
        return;
      }
      memcpy(&zOut[j], zRep, nRep);
      j += nRep;
      i += nPattern-1;
    }
  }
  assert( j+nStr-i+1==nOut );
  memcpy(&zOut[j], &zStr[i], nStr-i);
  j += nStr - i;
  assert( j<=nOut );
  zOut[j] = 0;
  sqlite3_result_text(context, (char*)zOut, j, sqlite3_free);
}

/*
** Implementation of the TRIM(), LTRIM(), and RTRIM() functions.
** The userdata is 0x1 for left trim, 0x2 for right trim, 0x3 for both.
*/
static void trimFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  const unsigned char *zIn;         /* Input string */
  const unsigned char *zCharSet;    /* Set of characters to trim */
  int nIn;                          /* Number of bytes in input */
  int flags;                        /* 1: trimleft  2: trimright  3: trim */
  int i;                            /* Loop counter */
  unsigned char *aLen = 0;          /* Length of each character in zCharSet */
  unsigned char **azChar = 0;       /* Individual characters in zCharSet */
  int nChar;                        /* Number of characters in zCharSet */

  if( sqlite3_value_type(argv[0])==SQLITE_NULL ){
    return;
  }
  zIn = sqlite3_value_text(argv[0]);
  if( zIn==0 ) return;
  nIn = sqlite3_value_bytes(argv[0]);
  assert( zIn==sqlite3_value_text(argv[0]) );
  if( argc==1 ){
    static const unsigned char lenOne[] = { 1 };
    static unsigned char * const azOne[] = { (u8*)" " };
    nChar = 1;
    aLen = (u8*)lenOne;
    azChar = (unsigned char **)azOne;
    zCharSet = 0;
  }else if( (zCharSet = sqlite3_value_text(argv[1]))==0 ){
    return;
  }else{
    const unsigned char *z;
    for(z=zCharSet, nChar=0; *z; nChar++){
      SQLITE_SKIP_UTF8(z);
    }
    if( nChar>0 ){
      azChar = contextMalloc(context, ((i64)nChar)*(sizeof(char*)+1));
      if( azChar==0 ){
        return;
      }
      aLen = (unsigned char*)&azChar[nChar];
      for(z=zCharSet, nChar=0; *z; nChar++){
        azChar[nChar] = (unsigned char *)z;
        SQLITE_SKIP_UTF8(z);
        aLen[nChar] = (u8)(z - azChar[nChar]);
      }
    }
  }
  if( nChar>0 ){
    flags = SQLITE_PTR_TO_INT(sqlite3_user_data(context));
    if( flags & 1 ){
      while( nIn>0 ){
        int len = 0;
        for(i=0; i<nChar; i++){
          len = aLen[i];
          if( len<=nIn && memcmp(zIn, azChar[i], len)==0 ) break;
        }
        if( i>=nChar ) break;
        zIn += len;
        nIn -= len;
      }
    }
    if( flags & 2 ){
      while( nIn>0 ){
        int len = 0;
        for(i=0; i<nChar; i++){
          len = aLen[i];
          if( len<=nIn && memcmp(&zIn[nIn-len],azChar[i],len)==0 ) break;
        }
        if( i>=nChar ) break;
        nIn -= len;
      }
    }
    if( zCharSet ){
      sqlite3_free(azChar);
    }
  }
  sqlite3_result_text(context, (char*)zIn, nIn, SQLITE_TRANSIENT);
}


/* IMP: R-25361-16150 This function is omitted from SQLite by default. It
** is only available if the SQLITE_SOUNDEX compile-time option is used
** when SQLite is built.
*/
#ifdef SQLITE_SOUNDEX
/*
** Compute the soundex encoding of a word.
**
** IMP: R-59782-00072 The soundex(X) function returns a string that is the
** soundex encoding of the string X. 
*/
static void soundexFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  char zResult[8];
  const u8 *zIn;
  int i, j;
  static const unsigned char iCode[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,
    1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,
    0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,
    1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,
  };
  assert( argc==1 );
  zIn = (u8*)sqlite3_value_text(argv[0]);
  if( zIn==0 ) zIn = (u8*)"";
  for(i=0; zIn[i] && !sqlite3Isalpha(zIn[i]); i++){}
  if( zIn[i] ){
    u8 prevcode = iCode[zIn[i]&0x7f];
    zResult[0] = sqlite3Toupper(zIn[i]);
    for(j=1; j<4 && zIn[i]; i++){
      int code = iCode[zIn[i]&0x7f];
      if( code>0 ){
        if( code!=prevcode ){
          prevcode = code;
          zResult[j++] = code + '0';
        }
      }else{
        prevcode = 0;
      }
    }
    while( j<4 ){
      zResult[j++] = '0';
    }
    zResult[j] = 0;
    sqlite3_result_text(context, zResult, 4, SQLITE_TRANSIENT);
  }else{
    /* IMP: R-64894-50321 The string "?000" is returned if the argument
    ** is NULL or contains no ASCII alphabetic characters. */
    sqlite3_result_text(context, "?000", 4, SQLITE_STATIC);
  }
}
#endif /* SQLITE_SOUNDEX */

#ifndef SQLITE_OMIT_LOAD_EXTENSION
/*
** A function that loads a shared-library extension then returns NULL.
*/
static void loadExt(sqlite3_context *context, int argc, sqlite3_value **argv){
  const char *zFile = (const char *)sqlite3_value_text(argv[0]);
  const char *zProc;
  sqlite3 *db = sqlite3_context_db_handle(context);
  char *zErrMsg = 0;

  if( argc==2 ){
    zProc = (const char *)sqlite3_value_text(argv[1]);
  }else{
    zProc = 0;
  }
  if( zFile && sqlite3_load_extension(db, zFile, zProc, &zErrMsg) ){
    sqlite3_result_error(context, zErrMsg, -1);
    sqlite3_free(zErrMsg);
  }
}
#endif


/*
** An instance of the following structure holds the context of a
** sum() or avg() aggregate computation.
*/
typedef struct SumCtx SumCtx;
struct SumCtx {
  double rSum;      /* Floating point sum */
  i64 iSum;         /* Integer sum */   
  i64 cnt;          /* Number of elements summed */
  u8 overflow;      /* True if integer overflow seen */
  u8 approx;        /* True if non-integer value was input to the sum */
};

/*
** Routines used to compute the sum, average, and total.
**
** The SUM() function follows the (broken) SQL standard which means
** that it returns NULL if it sums over no inputs.  TOTAL returns
** 0.0 in that case.  In addition, TOTAL always returns a float where
** SUM might return an integer if it never encounters a floating point
** value.  TOTAL never fails, but SUM might through an exception if
** it overflows an integer.
*/
static void sumStep(sqlite3_context *context, int argc, sqlite3_value **argv){
  SumCtx *p;
  int type;
  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  p = sqlite3_aggregate_context(context, sizeof(*p));
  type = sqlite3_value_numeric_type(argv[0]);
  if( p && type!=SQLITE_NULL ){
    p->cnt++;
    if( type==SQLITE_INTEGER ){
      i64 v = sqlite3_value_int64(argv[0]);
      p->rSum += v;
      if( (p->approx|p->overflow)==0 && sqlite3AddInt64(&p->iSum, v) ){
        p->overflow = 1;
      }
    }else{
      p->rSum += sqlite3_value_double(argv[0]);
      p->approx = 1;
    }
  }
}
static void sumFinalize(sqlite3_context *context){
  SumCtx *p;
  p = sqlite3_aggregate_context(context, 0);
  if( p && p->cnt>0 ){
    if( p->overflow ){
      sqlite3_result_error(context,"integer overflow",-1);
    }else if( p->approx ){
      sqlite3_result_double(context, p->rSum);
    }else{
      sqlite3_result_int64(context, p->iSum);
    }
  }
}
static void avgFinalize(sqlite3_context *context){
  SumCtx *p;
  p = sqlite3_aggregate_context(context, 0);
  if( p && p->cnt>0 ){
    sqlite3_result_double(context, p->rSum/(double)p->cnt);
  }
}
static void totalFinalize(sqlite3_context *context){
  SumCtx *p;
  p = sqlite3_aggregate_context(context, 0);
  /* (double)0 In case of SQLITE_OMIT_FLOATING_POINT... */
  sqlite3_result_double(context, p ? p->rSum : (double)0);
}

/*
** The following structure keeps track of state information for the
** count() aggregate function.
*/
typedef struct CountCtx CountCtx;
struct CountCtx {
  i64 n;
};

/*
** Routines to implement the count() aggregate function.
*/
static void countStep(sqlite3_context *context, int argc, sqlite3_value **argv){
  CountCtx *p;
  p = sqlite3_aggregate_context(context, sizeof(*p));
  if( (argc==0 || SQLITE_NULL!=sqlite3_value_type(argv[0])) && p ){
    p->n++;
  }

#ifndef SQLITE_OMIT_DEPRECATED
  /* The sqlite3_aggregate_count() function is deprecated.  But just to make
  ** sure it still operates correctly, verify that its count agrees with our 
  ** internal count when using count(*) and when the total count can be
  ** expressed as a 32-bit integer. */
  assert( argc==1 || p==0 || p->n>0x7fffffff
          || p->n==sqlite3_aggregate_count(context) );
#endif
}   
static void countFinalize(sqlite3_context *context){
  CountCtx *p;
  p = sqlite3_aggregate_context(context, 0);
  sqlite3_result_int64(context, p ? p->n : 0);
}

/*
** Routines to implement min() and max() aggregate functions.
*/
static void minmaxStep(
  sqlite3_context *context, 
  int NotUsed, 
  sqlite3_value **argv
){
  Mem *pArg  = (Mem *)argv[0];
  Mem *pBest;
  UNUSED_PARAMETER(NotUsed);

  pBest = (Mem *)sqlite3_aggregate_context(context, sizeof(*pBest));
  if( !pBest ) return;

  if( sqlite3_value_type(argv[0])==SQLITE_NULL ){
    if( pBest->flags ) sqlite3SkipAccumulatorLoad(context);
  }else if( pBest->flags ){
    int max;
    int cmp;
    CollSeq *pColl = sqlite3GetFuncCollSeq(context);
    /* This step function is used for both the min() and max() aggregates,
    ** the only difference between the two being that the sense of the
    ** comparison is inverted. For the max() aggregate, the
    ** sqlite3_user_data() function returns (void *)-1. For min() it
    ** returns (void *)db, where db is the sqlite3* database pointer.
    ** Therefore the next statement sets variable 'max' to 1 for the max()
    ** aggregate, or 0 for min().
    */
    max = sqlite3_user_data(context)!=0;
    cmp = sqlite3MemCompare(pBest, pArg, pColl);
    if( (max && cmp<0) || (!max && cmp>0) ){
      sqlite3VdbeMemCopy(pBest, pArg);
    }else{
      sqlite3SkipAccumulatorLoad(context);
    }
  }else{
    sqlite3VdbeMemCopy(pBest, pArg);
  }
}
static void minMaxFinalize(sqlite3_context *context){
  sqlite3_value *pRes;
  pRes = (sqlite3_value *)sqlite3_aggregate_context(context, 0);
  if( pRes ){
    if( pRes->flags ){
      sqlite3_result_value(context, pRes);
    }
    sqlite3VdbeMemRelease(pRes);
  }
}

/*
** group_concat(EXPR, ?SEPARATOR?)
*/
static void groupConcatStep(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  const char *zVal;
  StrAccum *pAccum;
  const char *zSep;
  int nVal, nSep;
  assert( argc==1 || argc==2 );
  if( sqlite3_value_type(argv[0])==SQLITE_NULL ) return;
  pAccum = (StrAccum*)sqlite3_aggregate_context(context, sizeof(*pAccum));

  if( pAccum ){
    sqlite3 *db = sqlite3_context_db_handle(context);
    int firstTerm = pAccum->useMalloc==0;
    pAccum->useMalloc = 2;
    pAccum->mxAlloc = db->aLimit[SQLITE_LIMIT_LENGTH];
    if( !firstTerm ){
      if( argc==2 ){
        zSep = (char*)sqlite3_value_text(argv[1]);
        nSep = sqlite3_value_bytes(argv[1]);
      }else{
        zSep = ",";
        nSep = 1;
      }
      sqlite3StrAccumAppend(pAccum, zSep, nSep);
    }
    zVal = (char*)sqlite3_value_text(argv[0]);
    nVal = sqlite3_value_bytes(argv[0]);
    sqlite3StrAccumAppend(pAccum, zVal, nVal);
  }
}
static void groupConcatFinalize(sqlite3_context *context){
  StrAccum *pAccum;
  pAccum = sqlite3_aggregate_context(context, 0);
  if( pAccum ){
    if( pAccum->tooBig ){
      sqlite3_result_error_toobig(context);
    }else if( pAccum->mallocFailed ){
      sqlite3_result_error_nomem(context);
    }else{    
      sqlite3_result_text(context, sqlite3StrAccumFinish(pAccum), -1, 
                          sqlite3_free);
    }
  }
}

/*
** This routine does per-connection function registration.  Most
** of the built-in functions above are part of the global function set.
** This routine only deals with those that are not global.
*/
void sqlite3RegisterBuiltinFunctions(sqlite3 *db){
  int rc = sqlite3_overload_function(db, "MATCH", 2);
  assert( rc==SQLITE_NOMEM || rc==SQLITE_OK );
  if( rc==SQLITE_NOMEM ){
    db->mallocFailed = 1;
  }
}

/*
** Set the LIKEOPT flag on the 2-argument function with the given name.
*/
static void setLikeOptFlag(sqlite3 *db, const char *zName, u8 flagVal){
  FuncDef *pDef;
  pDef = sqlite3FindFunction(db, zName, sqlite3Strlen30(zName),
                             2, SQLITE_UTF8, 0);
  if( ALWAYS(pDef) ){
    pDef->flags = flagVal;
  }
}

/*
** Register the built-in LIKE and GLOB functions.  The caseSensitive
** parameter determines whether or not the LIKE operator is case
** sensitive.  GLOB is always case sensitive.
*/
void sqlite3RegisterLikeFunctions(sqlite3 *db, int caseSensitive){
  struct compareInfo *pInfo;
  if( caseSensitive ){
    pInfo = (struct compareInfo*)&likeInfoAlt;
  }else{
    pInfo = (struct compareInfo*)&likeInfoNorm;
  }
  sqlite3CreateFunc(db, "like", 2, SQLITE_UTF8, pInfo, likeFunc, 0, 0, 0);
  sqlite3CreateFunc(db, "like", 3, SQLITE_UTF8, pInfo, likeFunc, 0, 0, 0);
  sqlite3CreateFunc(db, "glob", 2, SQLITE_UTF8, 
      (struct compareInfo*)&globInfo, likeFunc, 0, 0, 0);
  setLikeOptFlag(db, "glob", SQLITE_FUNC_LIKE | SQLITE_FUNC_CASE);
  setLikeOptFlag(db, "like", 
      caseSensitive ? (SQLITE_FUNC_LIKE | SQLITE_FUNC_CASE) : SQLITE_FUNC_LIKE);
}

/*
** pExpr points to an expression which implements a function.  If
** it is appropriate to apply the LIKE optimization to that function
** then set aWc[0] through aWc[2] to the wildcard characters and
** return TRUE.  If the function is not a LIKE-style function then
** return FALSE.
*/
int sqlite3IsLikeFunction(sqlite3 *db, Expr *pExpr, int *pIsNocase, char *aWc){
  FuncDef *pDef;
  if( pExpr->op!=TK_FUNCTION 
   || !pExpr->x.pList 
   || pExpr->x.pList->nExpr!=2
  ){
    return 0;
  }
  assert( !ExprHasProperty(pExpr, EP_xIsSelect) );
  pDef = sqlite3FindFunction(db, pExpr->u.zToken, 
                             sqlite3Strlen30(pExpr->u.zToken),
                             2, SQLITE_UTF8, 0);
  if( NEVER(pDef==0) || (pDef->flags & SQLITE_FUNC_LIKE)==0 ){
    return 0;
  }

  /* The memcpy() statement assumes that the wildcard characters are
  ** the first three statements in the compareInfo structure.  The
  ** asserts() that follow verify that assumption
  */
  memcpy(aWc, pDef->pUserData, 3);
  assert( (char*)&likeInfoAlt == (char*)&likeInfoAlt.matchAll );
  assert( &((char*)&likeInfoAlt)[1] == (char*)&likeInfoAlt.matchOne );
  assert( &((char*)&likeInfoAlt)[2] == (char*)&likeInfoAlt.matchSet );
  *pIsNocase = (pDef->flags & SQLITE_FUNC_CASE)==0;
  return 1;
}

/*
** All all of the FuncDef structures in the aBuiltinFunc[] array above
** to the global function hash table.  This occurs at start-time (as
** a consequence of calling sqlite3_initialize()).
**
** After this routine runs
*/
void sqlite3RegisterGlobalFunctions(void){
  /*
  ** The following array holds FuncDef structures for all of the functions
  ** defined in this file.
  **
  ** The array cannot be constant since changes are made to the
  ** FuncDef.pHash elements at start-time.  The elements of this array
  ** are read-only after initialization is complete.
  */
  static SQLITE_WSD FuncDef aBuiltinFunc[] = {
    FUNCTION(ltrim,              1, 1, 0, trimFunc         ),
    FUNCTION(ltrim,              2, 1, 0, trimFunc         ),
    FUNCTION(rtrim,              1, 2, 0, trimFunc         ),
    FUNCTION(rtrim,              2, 2, 0, trimFunc         ),
    FUNCTION(trim,               1, 3, 0, trimFunc         ),
    FUNCTION(trim,               2, 3, 0, trimFunc         ),
    FUNCTION(min,               -1, 0, 1, minmaxFunc       ),
    FUNCTION(min,                0, 0, 1, 0                ),
    AGGREGATE(min,               1, 0, 1, minmaxStep,      minMaxFinalize ),
    FUNCTION(max,               -1, 1, 1, minmaxFunc       ),
    FUNCTION(max,                0, 1, 1, 0                ),
    AGGREGATE(max,               1, 1, 1, minmaxStep,      minMaxFinalize ),
    FUNCTION2(typeof,            1, 0, 0, typeofFunc,  SQLITE_FUNC_TYPEOF),
    FUNCTION2(length,            1, 0, 0, lengthFunc,  SQLITE_FUNC_LENGTH),
    FUNCTION(substr,             2, 0, 0, substrFunc       ),
    FUNCTION(substr,             3, 0, 0, substrFunc       ),
    FUNCTION(abs,                1, 0, 0, absFunc          ),
#ifndef SQLITE_OMIT_FLOATING_POINT
    FUNCTION(round,              1, 0, 0, roundFunc        ),
    FUNCTION(round,              2, 0, 0, roundFunc        ),
#endif
    FUNCTION(upper,              1, 0, 0, upperFunc        ),
    FUNCTION(lower,              1, 0, 0, lowerFunc        ),
    FUNCTION(coalesce,           1, 0, 0, 0                ),
    FUNCTION(coalesce,           0, 0, 0, 0                ),
    FUNCTION2(coalesce,         -1, 0, 0, ifnullFunc,  SQLITE_FUNC_COALESCE),
    FUNCTION(hex,                1, 0, 0, hexFunc          ),
    FUNCTION2(ifnull,            2, 0, 0, ifnullFunc,  SQLITE_FUNC_COALESCE),
    FUNCTION(random,             0, 0, 0, randomFunc       ),
    FUNCTION(randomblob,         1, 0, 0, randomBlob       ),
    FUNCTION(nullif,             2, 0, 1, nullifFunc       ),
    FUNCTION(sqlite_version,     0, 0, 0, versionFunc      ),
    FUNCTION(sqlite_source_id,   0, 0, 0, sourceidFunc     ),
    FUNCTION(sqlite_log,         2, 0, 0, errlogFunc       ),
#ifndef SQLITE_OMIT_COMPILEOPTION_DIAGS
    FUNCTION(sqlite_compileoption_used,1, 0, 0, compileoptionusedFunc  ),
    FUNCTION(sqlite_compileoption_get, 1, 0, 0, compileoptiongetFunc  ),
#endif /* SQLITE_OMIT_COMPILEOPTION_DIAGS */
    FUNCTION(quote,              1, 0, 0, quoteFunc        ),
    FUNCTION(last_insert_rowid,  0, 0, 0, last_insert_rowid),
    FUNCTION(changes,            0, 0, 0, changes          ),
    FUNCTION(total_changes,      0, 0, 0, total_changes    ),
    FUNCTION(replace,            3, 0, 0, replaceFunc      ),
    FUNCTION(zeroblob,           1, 0, 0, zeroblobFunc     ),
  #ifdef SQLITE_SOUNDEX
    FUNCTION(soundex,            1, 0, 0, soundexFunc      ),
  #endif
  #ifndef SQLITE_OMIT_LOAD_EXTENSION
    FUNCTION(load_extension,     1, 0, 0, loadExt          ),
    FUNCTION(load_extension,     2, 0, 0, loadExt          ),
  #endif
    AGGREGATE(sum,               1, 0, 0, sumStep,         sumFinalize    ),
    AGGREGATE(total,             1, 0, 0, sumStep,         totalFinalize    ),
    AGGREGATE(avg,               1, 0, 0, sumStep,         avgFinalize    ),
 /* AGGREGATE(count,             0, 0, 0, countStep,       countFinalize  ), */
    {0,SQLITE_UTF8,SQLITE_FUNC_COUNT,0,0,0,countStep,countFinalize,"count",0,0},
    AGGREGATE(count,             1, 0, 0, countStep,       countFinalize  ),
    AGGREGATE(group_concat,      1, 0, 0, groupConcatStep, groupConcatFinalize),
    AGGREGATE(group_concat,      2, 0, 0, groupConcatStep, groupConcatFinalize),
  
    LIKEFUNC(glob, 2, &globInfo, SQLITE_FUNC_LIKE|SQLITE_FUNC_CASE),
  #ifdef SQLITE_CASE_SENSITIVE_LIKE
    LIKEFUNC(like, 2, &likeInfoAlt, SQLITE_FUNC_LIKE|SQLITE_FUNC_CASE),
    LIKEFUNC(like, 3, &likeInfoAlt, SQLITE_FUNC_LIKE|SQLITE_FUNC_CASE),
  #else
    LIKEFUNC(like, 2, &likeInfoNorm, SQLITE_FUNC_LIKE),
    LIKEFUNC(like, 3, &likeInfoNorm, SQLITE_FUNC_LIKE),
  #endif
  };

  int i;
  FuncDefHash *pHash = &GLOBAL(FuncDefHash, sqlite3GlobalFunctions);
  FuncDef *aFunc = (FuncDef*)&GLOBAL(FuncDef, aBuiltinFunc);

  for(i=0; i<ArraySize(aBuiltinFunc); i++){
    sqlite3FuncDefInsert(pHash, &aFunc[i]);
  }
  sqlite3RegisterDateTimeFunctions();
#ifndef SQLITE_OMIT_ALTERTABLE
  sqlite3AlterFunctions();
#endif
}
