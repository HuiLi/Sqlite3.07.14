/*
** 2003 October 31 2003年10月31日
**
** The author disclaims copyright to this source code.  In place of 作者把这部分源代码进行了公开
** a legal notice, here is a blessing: 下面是作者的寄语
**
**    May you do good and not evil. 愿你行善莫行恶
**    May you find forgiveness for yourself and forgive others. 可能你要原谅自己或者别人。
**    May you share freely, never taking more than you give. 愿你宽心与人分享，索取不多于你所施舍。
**
*************************************************************************
** This file contains the C functions that implement date and time
** functions for SQLite. 此文件包含为SQLite数据库实现日期和时间函数的C函数。 
**
** There is only one exported symbol in this file - the function
** sqlite3RegisterDateTimeFunctions() found at the bottom of the file.
** All other code has file scope. 在这个文件中只有一个出口标志，sqlite3RegisterDateTimeFunctions()这个函数在文件的底部可以找到。
**
** SQLite processes all times and dates as Julian Day numbers.  The
** dates and times are stored as the number of days since noon
** in Greenwich on November 24, 4714 B.C. according to the Gregorian
** calendar system.  SQLite将所有时间和日期处理成朱利安日数。日期和时间保存为自公历公元前4714年格林尼治十一月二十四日中午的天数。
**
** 1970-01-01 00:00:00 is JD 2440587.5
** 2000-01-01 00:00:00 is JD 2451544.5
**
** This implemention requires years to be expressed as a 4-digit number
** which means that only dates between 0000-01-01 and 9999-12-31 can
** be represented, even though julian day numbers allow a much wider
** range of dates.  这种实现需要用4位数字表示的年份，也就是只有在0000-01-01和9999-12-31之间的日期可以表示，即使朱利安日数允许更宽范围的日期。
**
** The Gregorian calendar system is used for all dates and times,
** even those that predate the Gregorian calendar.  Historians usually
** use the Julian calendar for dates prior to 1582-10-15 and for some
** dates afterwards, depending on locale.  Beware of this difference.  公历系统适用于所有的日期和时间，即使是那些早于公历的部分。历史学家通常根据现实情况使用1582-10-15之前或者之后的一些朱利安日历作为日期。
**
** The conversion algorithms are implemented based on descriptions
** in the following text:  转换算法是基于以下描述来实现的
**
**      Jean Meeus
**      Astronomical Algorithms, 2nd Edition, 1998
**      ISBM 0-943396-61-1
**      Willmann-Bell, Inc
**      Richmond, Virginia (USA)
*/    《天文算法》，第二版，1998 ISBM 0-943396-61-1 Willmann钟，有限公司里士满，弗吉尼亚州（美国）
#include "sqliteInt.h"
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#ifndef SQLITE_OMIT_DATETIME_FUNCS   /*先测试SQLITE_OMIT_DATETIME_FUNCS是否被宏定义过*/ 


/*
** A structure for holding a single date and time.  保持单一的一个日期和时间的结构体
*/
typedef struct DateTime DateTime;
struct DateTime {
  sqlite3_int64 iJD; /* The julian day number times 86400000 */  定义朱利安日数
  int Y, M, D;       /* Year, month, and day */   定义年、月、日
  int h, m;          /* Hour and minutes */       定义时和分
  int tz;            /* Timezone offset in minutes */  定义在分钟的时区偏移
  double s;          /* Seconds */    定义秒
  char validYMD;     /* True (1) if Y,M,D are valid */ 如果Y,M,D有效，则真值为1
  char validHMS;     /* True (1) if h,m,s are valid */ 如果h,m,s有效，则真值为1
  char validJD;      /* True (1) if iJD is valid */    如果iJD有效，则真值为1
  char validTZ;      /* True (1) if tz is valid */     如果tz有效，则真值为1
};


/*
** Convert zDate into one or more integers.  Additional arguments
** come in groups of 5 as follows:  将zDate转换成一个或多个整数。额外的5个参数组如下：
**
**       N       number of digits in the integer  整数的位数
**       min     minimum allowed value of the integer  整数值允许的最小值
**       max     maximum allowed value of the integer  整数值允许的最大值
**       nextC   first character after the integer     整数后的第一个字符
**       pVal    where to write the integers value.    写入整数值的位置
**
** Conversions continue until one with nextC==0 is encountered.  当遇到nextC==0时，转换继续。
** The function returns the number of successful conversions.  该函数返回成功转换的次数。
*/
static int getDigits(const char *zDate, ...){
  va_list ap;
  int val;
  int N;
  int min;
  int max;
  int nextC;
  int *pVal;
  int cnt = 0;
  va_start(ap, zDate);
  do{
    N = va_arg(ap, int);
    min = va_arg(ap, int);
    max = va_arg(ap, int);
    nextC = va_arg(ap, int);
    pVal = va_arg(ap, int*);
    val = 0;
    while( N-- ){
      if( !sqlite3Isdigit(*zDate) ){
        goto end_getDigits;
      }
      val = val*10 + *zDate - '0';
      zDate++;
    }
    if( val<min || val>max || (nextC!=0 && nextC!=*zDate) ){
      goto end_getDigits;
    }
    *pVal = val;
    zDate++;
    cnt++;
  }while( nextC );
end_getDigits:
  va_end(ap);
  return cnt;
}

/*
** Parse a timezone extension on the end of a date-time. 解析在日期时间结束上的一个时区扩展。
** The extension is of the form: 扩展的形式：
**
**        (+/-)HH:MM
**
** Or the "zulu" notation:
**
**        Z
**
** If the parse is successful, write the number of minutes 
** of change in p->tz and return 0.  If a parser error occurs,
** return non-zero.  如果分析成功，写入在p->tz中改变的分钟次数并返回0.如果分析发生错误时，返回非0
**
** A missing specifier is not considered an error. 丢失的说明符不被认为是一个错误。
*/
static int parseTimezone(const char *zDate, DateTime *p){
  int sgn = 0;
  int nHr, nMn;
  int c;
  while( sqlite3Isspace(*zDate) ){ zDate++; }
  p->tz = 0;
  c = *zDate;
  if( c=='-' ){
    sgn = -1;
  }else if( c=='+' ){
    sgn = +1;
  }else if( c=='Z' || c=='z' ){
    zDate++;
    goto zulu_time;
  }else{
    return c!=0;
  }
  zDate++;
  if( getDigits(zDate, 2, 0, 14, ':', &nHr, 2, 0, 59, 0, &nMn)!=2 ){
    return 1;
  }
  zDate += 5;
  p->tz = sgn*(nMn + nHr*60);
zulu_time:
  while( sqlite3Isspace(*zDate) ){ zDate++; }
  return *zDate!=0;
}

/*
** Parse times of the form HH:MM or HH:MM:SS or HH:MM:SS.FFFF.  解析形式为 HH:MM 、 HH:MM:SS 或者 HH:MM:SS.FFFF的时间
** The HH, MM, and SS must each be exactly 2 digits.  The
** fractional seconds FFFF can be one or more digits.   HH, MM, 和SS这种形式的时间，每一个必须精确到2位。分数的秒FFFF可以是一位或更多位。

**
** Return 1 if there is a parsing error and 0 on success.  如果出现解析错误和0成功，则返回1.
*/
static int parseHhMmSs(const char *zDate, DateTime *p){
  int h, m, s;
  double ms = 0.0;
  if( getDigits(zDate, 2, 0, 24, ':', &h, 2, 0, 59, 0, &m)!=2 ){
    return 1;
  }
  zDate += 5;
  if( *zDate==':' ){
    zDate++;
    if( getDigits(zDate, 2, 0, 59, 0, &s)!=1 ){
      return 1;
    }
    zDate += 2;
    if( *zDate=='.' && sqlite3Isdigit(zDate[1]) ){
      double rScale = 1.0;
      zDate++;
      while( sqlite3Isdigit(*zDate) ){
        ms = ms*10.0 + *zDate - '0';
        rScale *= 10.0;
        zDate++;
      }
      ms /= rScale;
    }
  }else{
    s = 0;
  }
  p->validJD = 0;
  p->validHMS = 1;
  p->h = h;
  p->m = m;
  p->s = s + ms;
  if( parseTimezone(zDate, p) ) return 1;
  p->validTZ = (p->tz!=0)?1:0;
  return 0;
}

/*
** Convert from YYYY-MM-DD HH:MM:SS to julian day.  We always assume
** that the YYYY-MM-DD is according to the Gregorian calendar.  从YYYY-MM-DD HH:MM:SS的形式转化到朱利安日。我们通常假设YYYY-MM-DD是根据公历的时间。
**
** Reference:  Meeus page 61  参考书61页
*/
static void computeJD(DateTime *p){
  int Y, M, D, A, B, X1, X2;

  if( p->validJD ) return;
  if( p->validYMD ){
    Y = p->Y;
    M = p->M;
    D = p->D;
  }else{
    Y = 2000;  /* If no YMD specified, assume 2000-Jan-01 */  如果没有指定的年月日，假设为2000-Jan-01
    M = 1;
    D = 1;
  }
  if( M<=2 ){
    Y--;
    M += 12;
  }
  A = Y/100;
  B = 2 - A + (A/4);
  X1 = 36525*(Y+4716)/100;
  X2 = 306001*(M+1)/10000;
  p->iJD = (sqlite3_int64)((X1 + X2 + D + B - 1524.5 ) * 86400000);
  p->validJD = 1;
  if( p->validHMS ){
    p->iJD += p->h*3600000 + p->m*60000 + (sqlite3_int64)(p->s*1000);
    if( p->validTZ ){
      p->iJD -= p->tz*60000;
      p->validYMD = 0;
      p->validHMS = 0;
      p->validTZ = 0;
    }
  }
}

/*
** Parse dates of the form   解析日期的形式：
**
**     YYYY-MM-DD HH:MM:SS.FFF
**     YYYY-MM-DD HH:MM:SS
**     YYYY-MM-DD HH:MM
**     YYYY-MM-DD
**
** Write the result into the DateTime structure and return 0
** on success and 1 if the input string is not a well-formed
** date.  将结果写入DateTime结构体并返回0成功，如果输入的字符不是一个正确的日期形式则返回1.
*/
static int parseYyyyMmDd(const char *zDate, DateTime *p){
  int Y, M, D, neg;

  if( zDate[0]=='-' ){
    zDate++;
    neg = 1;
  }else{
    neg = 0;
  }
  if( getDigits(zDate,4,0,9999,'-',&Y,2,1,12,'-',&M,2,1,31,0,&D)!=3 ){
    return 1;
  }
  zDate += 10;
  while( sqlite3Isspace(*zDate) || 'T'==*(u8*)zDate ){ zDate++; }
  if( parseHhMmSs(zDate, p)==0 ){
    /* We got the time */   得到时间
  }else if( *zDate==0 ){
    p->validHMS = 0;
  }else{
    return 1;
  }
  p->validJD = 0;
  p->validYMD = 1;
  p->Y = neg ? -Y : Y;
  p->M = M;
  p->D = D;
  if( p->validTZ ){
    computeJD(p);
  }
  return 0;
}

/*
** Set the time to the current time reported by the VFS. 将时间设置成VFS报道的当前时间。
**
** Return the number of errors.   返回错误次数。
*/
static int setDateTimeToCurrent(sqlite3_context *context, DateTime *p){
  sqlite3 *db = sqlite3_context_db_handle(context);
  if( sqlite3OsCurrentTimeInt64(db->pVfs, &p->iJD)==SQLITE_OK ){
    p->validJD = 1;
    return 0;
  }else{
    return 1;
  }
}

/*
** Attempt to parse the given string into a Julian Day Number.  Return
** the number of errors.  试着把给出的字符串解析为朱利安日数。返回错误次数。
**
** The following are acceptable forms for the input string:  以下是正确的字符串形式：
**
**      YYYY-MM-DD HH:MM:SS.FFF  +/-HH:MM
**      DDDD.DD 
**      now
**
** In the first form, the +/-HH:MM is always optional.  The fractional
** seconds extension (the ".FFF") is optional.  The seconds portion
** (":SS.FFF") is option.  The year and date can be omitted as long
** as there is a time string.  The time string can be omitted as long
** as there is a year and date.  第一种形式中，+/-HH:MM是始终可选的。小数秒的扩展（.FFF）是可选的。分数部分(":SS.FFF")是可选的。只要有时间字符串，年和日期可以省略。有年和日期，时间字符串也可以省略。
*/
static int parseDateOrTime(
  sqlite3_context *context, 
  const char *zDate, 
  DateTime *p
){
  double r;
  if( parseYyyyMmDd(zDate,p)==0 ){
    return 0;
  }else if( parseHhMmSs(zDate, p)==0 ){
    return 0;
  }else if( sqlite3StrICmp(zDate,"now")==0){
    return setDateTimeToCurrent(context, p);
  }else if( sqlite3AtoF(zDate, &r, sqlite3Strlen30(zDate), SQLITE_UTF8) ){
    p->iJD = (sqlite3_int64)(r*86400000.0 + 0.5);
    p->validJD = 1;
    return 0;
  }
  return 1;
}

/*
** Compute the Year, Month, and Day from the julian day number.  从朱利安日数中计算年、月、日。
*/
static void computeYMD(DateTime *p){
  int Z, A, B, C, D, E, X1;
  if( p->validYMD ) return;
  if( !p->validJD ){
    p->Y = 2000;
    p->M = 1;
    p->D = 1;
  }else{
    Z = (int)((p->iJD + 43200000)/86400000);
    A = (int)((Z - 1867216.25)/36524.25);
    A = Z + 1 + A - (A/4);
    B = A + 1524;
    C = (int)((B - 122.1)/365.25);
    D = (36525*C)/100;
    E = (int)((B-D)/30.6001);
    X1 = (int)(30.6001*E);
    p->D = B - D - X1;
    p->M = E<14 ? E-1 : E-13;
    p->Y = p->M>2 ? C - 4716 : C - 4715;
  }
  p->validYMD = 1;
}

/*
** Compute the Hour, Minute, and Seconds from the julian day number.  从朱利安日数中计算时、分、秒。
*/
static void computeHMS(DateTime *p){
  int s;
  if( p->validHMS ) return;
  computeJD(p);
  s = (int)((p->iJD + 43200000) % 86400000);
  p->s = s/1000.0;
  s = (int)p->s;
  p->s -= s;
  p->h = s/3600;
  s -= p->h*3600;
  p->m = s/60;
  p->s += s - p->m*60;
  p->validHMS = 1;
}

/*
** Compute both YMD and HMS   计算年月日和时分秒
*/
static void computeYMD_HMS(DateTime *p){
  computeYMD(p);
  computeHMS(p);
}

/*
** Clear the YMD and HMS and the TZ  清除年月日、时分秒和TZ
*/
static void clearYMD_HMS_TZ(DateTime *p){
  p->validYMD = 0;
  p->validHMS = 0;
  p->validTZ = 0;
}

/*
** On recent Windows platforms, the localtime_s() function is available
** as part of the "Secure CRT". It is essentially equivalent to 
** localtime_r() available under most POSIX platforms, except that the 
** order of the parameters is reversed.   在最近的Windows平台上，作为"Secure CRT"的一部分，函数localtime_s()是可用的。它本质上等价于在大多数POSIX平台下可用的 localtime_r()函数，但是参数的顺序是相反的。
**
** See http://msdn.microsoft.com/en-us/library/a442x3ye(VS.80).aspx.
**
** If the user has not indicated to use localtime_r() or localtime_s()
** already, check for an MSVC build environment that provides 
** localtime_s().   如果用户没有明确指出要使用或者已经使用函数localtime_r()，检查提供localtime_r()函数的MSVC建设环境。
*/
#if !defined(HAVE_LOCALTIME_R) && !defined(HAVE_LOCALTIME_S) && \
     defined(_MSC_VER) && defined(_CRT_INSECURE_DEPRECATE)
#define HAVE_LOCALTIME_S 1
#endif

#ifndef SQLITE_OMIT_LOCALTIME
/*
** The following routine implements the rough equivalent of localtime_r()
** using whatever operating-system specific localtime facility that
** is available.  This routine returns 0 on success and
** non-zero on any kind of error.  以下程序实现了与任何操作系统具体可用的地方时间工具大致等价的localtime_r()函数的应用。这个程序成功执行返回0，任何形式的错误返回非0.
**
** If the sqlite3GlobalConfig.bLocaltimeFault variable is true then this
** routine will always fail.   如果sqlite3GlobalConfig.bLocaltimeFault的变量是真值，那么这个程序将永远运行失败。
*/
static int osLocaltime(time_t *t, struct tm *pTm){
  int rc;
#if (!defined(HAVE_LOCALTIME_R) || !HAVE_LOCALTIME_R) \
      && (!defined(HAVE_LOCALTIME_S) || !HAVE_LOCALTIME_S)
  struct tm *pX;
#if SQLITE_THREADSAFE>0
  sqlite3_mutex *mutex = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER);
#endif
  sqlite3_mutex_enter(mutex);
  pX = localtime(t);
#ifndef SQLITE_OMIT_BUILTIN_TEST
  if( sqlite3GlobalConfig.bLocaltimeFault ) pX = 0;
#endif
  if( pX ) *pTm = *pX;
  sqlite3_mutex_leave(mutex);
  rc = pX==0;
#else
#ifndef SQLITE_OMIT_BUILTIN_TEST
  if( sqlite3GlobalConfig.bLocaltimeFault ) return 1;
#endif
#if defined(HAVE_LOCALTIME_R) && HAVE_LOCALTIME_R
  rc = localtime_r(t, pTm)==0;
#else
  rc = localtime_s(pTm, t);
#endif /* HAVE_LOCALTIME_R */
#endif /* HAVE_LOCALTIME_R || HAVE_LOCALTIME_S */
  return rc;
}
#endif /* SQLITE_OMIT_LOCALTIME */


#ifndef SQLITE_OMIT_LOCALTIME
/*
** Compute the difference (in milliseconds) between localtime and UTC
** (a.k.a. GMT) for the time value p where p is in UTC. If no error occurs,
** return this value and set *pRc to SQLITE_OK.  当p实在UTC时，计算当地时间与UTC（即格林威治标准时间）时间值p之间的差异（毫秒）。如果没有错误，返回这个值并设定SQLITE_OK为*pRc。
**
** Or, if an error does occur, set *pRc to SQLITE_ERROR. The returned value
** is undefined in this case.  或者，如果出现错误，设置SQLITE_ERROR为*pRc。在这种情况下，返回值是不确定的。
*/
static sqlite3_int64 localtimeOffset(
  DateTime *p,                    /* Date at which to calculate offset */ 计算偏移量的日期
  sqlite3_context *pCtx,          /* Write error here if one occurs */    如果错误出现写入错误
  int *pRc                        /* OUT: Error code. SQLITE_OK or ERROR */ 错误代码。SQLITE_OK或者错误。
){
  DateTime x, y;
  time_t t;
  struct tm sLocal;

  /* Initialize the contents of sLocal to avoid a compiler warning. */ 初始化sLocal内容以避免编译器警告。
  memset(&sLocal, 0, sizeof(sLocal));

  x = *p;
  computeYMD_HMS(&x);
  if( x.Y<1971 || x.Y>=2038 ){
    x.Y = 2000;
    x.M = 1;
    x.D = 1;
    x.h = 0;
    x.m = 0;
    x.s = 0.0;
  } else {
    int s = (int)(x.s + 0.5);
    x.s = s;
  }
  x.tz = 0;
  x.validJD = 0;
  computeJD(&x);
  t = (time_t)(x.iJD/1000 - 21086676*(i64)10000);
  if( osLocaltime(&t, &sLocal) ){
    sqlite3_result_error(pCtx, "local time unavailable", -1);
    *pRc = SQLITE_ERROR;
    return 0;
  }
  y.Y = sLocal.tm_year + 1900;
  y.M = sLocal.tm_mon + 1;
  y.D = sLocal.tm_mday;
  y.h = sLocal.tm_hour;
  y.m = sLocal.tm_min;
  y.s = sLocal.tm_sec;
  y.validYMD = 1;
  y.validHMS = 1;
  y.validJD = 0;
  y.validTZ = 0;
  computeJD(&y);
  *pRc = SQLITE_OK;
  return y.iJD - x.iJD;
}
#endif /* SQLITE_OMIT_LOCALTIME */

/*
** Process a modifier to a date-time stamp.  The modifiers are
** as follows:  对日期时间戳进行修改。修改如下：
**
**     NNN days             天数
**     NNN hours            小时数
**     NNN minutes          分钟数
**     NNN.NNNN seconds     秒数
**     NNN months           月数
**     NNN years            年数
**     start of month       开始一个月     
**     start of year        开始的一年
**     start of week        开始的一周
**     start of day         开始的一天
**     weekday N            工作日
**     unixepoch            时间上一点
**     localtime            当地时间
**     utc                  世界时间代码
**
** Return 0 on success and 1 if there is any kind of error. If the error
** is in a system call (i.e. localtime()), then an error message is written
** to context pCtx. If the error is an unrecognized modifier, no error is
** written to pCtx.   执行成功返回0，有错误就返回1.如果错误在一个系统调用中（即localtime()），那么一个错误信息就会写入到pCtx的文本里。如果错误是一个无法识别的修正，就不会有错误写入pCtx。
*/
static int parseModifier(sqlite3_context *pCtx, const char *zMod, DateTime *p){
  int rc = 1;
  int n;
  double r;
  char *z, zBuf[30];
  z = zBuf;
  for(n=0; n<ArraySize(zBuf)-1 && zMod[n]; n++){
    z[n] = (char)sqlite3UpperToLower[(u8)zMod[n]];
  }
  z[n] = 0;
  switch( z[0] ){
#ifndef SQLITE_OMIT_LOCALTIME
    case 'l': {
      /*    localtime         当地时间
      **
      ** Assuming the current time value is UTC (a.k.a. GMT), shift it to
      ** show local time.     假设当前的时间值为格林尼治时间，转移到显示本地时间。
      */
      if( strcmp(z, "localtime")==0 ){
        computeJD(p);
        p->iJD += localtimeOffset(p, pCtx, &rc);
        clearYMD_HMS_TZ(p);
      }
      break;
    }
#endif
    case 'u': {
      /*
      **    unixepoch
      **
      ** Treat the current value of p->iJD as the number of
      ** seconds since 1970.  Convert to a real julian day number.  把p->iJD的当前时间值看做自1970以来的秒数。转换到一个真正的朱利安日数。
      */
      if( strcmp(z, "unixepoch")==0 && p->validJD ){
        p->iJD = (p->iJD + 43200)/86400 + 21086676*(i64)10000000;
        clearYMD_HMS_TZ(p);
        rc = 0;
      }
#ifndef SQLITE_OMIT_LOCALTIME
      else if( strcmp(z, "utc")==0 ){
        sqlite3_int64 c1;
        computeJD(p);
        c1 = localtimeOffset(p, pCtx, &rc);
        if( rc==SQLITE_OK ){
          p->iJD -= c1;
          clearYMD_HMS_TZ(p);
          p->iJD += c1 - localtimeOffset(p, pCtx, &rc);
        }
      }
#endif
      break;
    }
    case 'w': {
      /*
      **    weekday N
      **
      ** Move the date to the same time on the next occurrence of
      ** weekday N where 0==Sunday, 1==Monday, and so forth.  If the
      ** date is already on the appropriate weekday, this is a no-op.  将时间移动到与下一个工作日出现时相同的时间，此时，0==Sunday, 1==Monday，等等。如果日期已经在适当的工作日，这是一个空操作。
      */
      if( strncmp(z, "weekday ", 8)==0
               && sqlite3AtoF(&z[8], &r, sqlite3Strlen30(&z[8]), SQLITE_UTF8)
               && (n=(int)r)==r && n>=0 && r<7 ){
        sqlite3_int64 Z;
        computeYMD_HMS(p);
        p->validTZ = 0;
        p->validJD = 0;
        computeJD(p);
        Z = ((p->iJD + 129600000)/86400000) % 7;
        if( Z>n ) Z -= 7;
        p->iJD += (n - Z)*86400000;
        clearYMD_HMS_TZ(p);
        rc = 0;
      }
      break;
    }
    case 's': {
      /*
      **    start of TTTTT
      **
      ** Move the date backwards to the beginning of the current day,
      ** or month or year.   把日期往回移动到当前一天、一个月或者一年的开始。
      */
      if( strncmp(z, "start of ", 9)!=0 ) break;
      z += 9;
      computeYMD(p);
      p->validHMS = 1;
      p->h = p->m = 0;
      p->s = 0.0;
      p->validTZ = 0;
      p->validJD = 0;
      if( strcmp(z,"month")==0 ){
        p->D = 1;
        rc = 0;
      }else if( strcmp(z,"year")==0 ){
        computeYMD(p);
        p->M = 1;
        p->D = 1;
        rc = 0;
      }else if( strcmp(z,"day")==0 ){
        rc = 0;
      }
      break;
    }
    case '+':
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
      double rRounder;
      for(n=1; z[n] && z[n]!=':' && !sqlite3Isspace(z[n]); n++){}
      if( !sqlite3AtoF(z, &r, n, SQLITE_UTF8) ){
        rc = 1;
        break;
      }
      if( z[n]==':' ){
        /* A modifier of the form (+|-)HH:MM:SS.FFF adds (or subtracts) the
        ** specified number of hours, minutes, seconds, and fractional seconds
        ** to the time.  The ".FFF" may be omitted.  The ":SS.FFF" may be
        ** omitted.   形式为(+|-)HH:MM:SS.FFF的修改在时间上加上（或者减去）指定的小时、分钟、秒和小数秒。 ".FFF"可以省略。":SS.FFF"也可以省略。
        */
        const char *z2 = z;
        DateTime tx;
        sqlite3_int64 day;
        if( !sqlite3Isdigit(*z2) ) z2++;
        memset(&tx, 0, sizeof(tx));
        if( parseHhMmSs(z2, &tx) ) break;
        computeJD(&tx);
        tx.iJD -= 43200000;
        day = tx.iJD/86400000;
        tx.iJD -= day*86400000;
        if( z[0]=='-' ) tx.iJD = -tx.iJD;
        computeJD(p);
        clearYMD_HMS_TZ(p);
        p->iJD += tx.iJD;
        rc = 0;
        break;
      }
      z += n;
      while( sqlite3Isspace(*z) ) z++;
      n = sqlite3Strlen30(z);
      if( n>10 || n<3 ) break;
      if( z[n-1]=='s' ){ z[n-1] = 0; n--; }
      computeJD(p);
      rc = 0;
      rRounder = r<0 ? -0.5 : +0.5;
      if( n==3 && strcmp(z,"day")==0 ){
        p->iJD += (sqlite3_int64)(r*86400000.0 + rRounder);
      }else if( n==4 && strcmp(z,"hour")==0 ){
        p->iJD += (sqlite3_int64)(r*(86400000.0/24.0) + rRounder);
      }else if( n==6 && strcmp(z,"minute")==0 ){
        p->iJD += (sqlite3_int64)(r*(86400000.0/(24.0*60.0)) + rRounder);
      }else if( n==6 && strcmp(z,"second")==0 ){
        p->iJD += (sqlite3_int64)(r*(86400000.0/(24.0*60.0*60.0)) + rRounder);
      }else if( n==5 && strcmp(z,"month")==0 ){
        int x, y;
        computeYMD_HMS(p);
        p->M += (int)r;
        x = p->M>0 ? (p->M-1)/12 : (p->M-12)/12;
        p->Y += x;
        p->M -= x*12;
        p->validJD = 0;
        computeJD(p);
        y = (int)r;
        if( y!=r ){
          p->iJD += (sqlite3_int64)((r - y)*30.0*86400000.0 + rRounder);
        }
      }else if( n==4 && strcmp(z,"year")==0 ){
        int y = (int)r;
        computeYMD_HMS(p);
        p->Y += y;
        p->validJD = 0;
        computeJD(p);
        if( y!=r ){
          p->iJD += (sqlite3_int64)((r - y)*365.0*86400000.0 + rRounder);
        }
      }else{
        rc = 1;
      }
      clearYMD_HMS_TZ(p);
      break;
    }
    default: {
      break;
    }
  }
  return rc;
}

/*
** Process time function arguments.  argv[0] is a date-time stamp.
** argv[1] and following are modifiers.  Parse them all and write
** the resulting time into the DateTime structure p.  Return 0
** on success and 1 if there are any errors.  处理时间函数的参数。参数[0]是一个日期时间戳。参数[1]和下面的是修改。分析全部并在日期时间结构体p中写入结果时间。成功返回0，有错误返回1.
**
** If there are zero parameters (if even argv[0] is undefined)
** then assume a default value of "now" for argv[0].  如果有零参数（即使是未定义的参数[0]），就为参数[0]假设一个暂时的默认值。
*/
static int isDate(
  sqlite3_context *context, 
  int argc, 
  sqlite3_value **argv, 
  DateTime *p
){
  int i;
  const unsigned char *z;
  int eType;
  memset(p, 0, sizeof(*p));
  if( argc==0 ){
    return setDateTimeToCurrent(context, p);
  }
  if( (eType = sqlite3_value_type(argv[0]))==SQLITE_FLOAT
                   || eType==SQLITE_INTEGER ){
    p->iJD = (sqlite3_int64)(sqlite3_value_double(argv[0])*86400000.0 + 0.5);
    p->validJD = 1;
  }else{
    z = sqlite3_value_text(argv[0]);
    if( !z || parseDateOrTime(context, (char*)z, p) ){
      return 1;
    }
  }
  for(i=1; i<argc; i++){
    z = sqlite3_value_text(argv[i]);
    if( z==0 || parseModifier(context, (char*)z, p) ) return 1;
  }
  return 0;
}


/*
** The following routines implement the various date and time functions
** of SQLite.   下面的程序实现SQLite的不同的日期和时间函数。
*/

/*
**    julianday( TIMESTRING, MOD, MOD, ...)  朱利安日数（时间字符串，模数，模数，...）
**
** Return the julian day number of the date specified in the arguments  返回参数中所指定的日期朱利安日数
*/
static void juliandayFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  DateTime x;
  if( isDate(context, argc, argv, &x)==0 ){
    computeJD(&x);
    sqlite3_result_double(context, x.iJD/86400000.0);
  }
}

/*
**    datetime( TIMESTRING, MOD, MOD, ...)  日期时间（时间字符串，模数，模数，...）
**
** Return YYYY-MM-DD HH:MM:SS  返回YYYY-MM-DD HH:MM:SS
*/
static void datetimeFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  DateTime x;
  if( isDate(context, argc, argv, &x)==0 ){
    char zBuf[100];
    computeYMD_HMS(&x);
    sqlite3_snprintf(sizeof(zBuf), zBuf, "%04d-%02d-%02d %02d:%02d:%02d",
                     x.Y, x.M, x.D, x.h, x.m, (int)(x.s));
    sqlite3_result_text(context, zBuf, -1, SQLITE_TRANSIENT);
  }
}

/*
**    time( TIMESTRING, MOD, MOD, ...)   时间（时间字符串，模数，模数，...）
**
** Return HH:MM:SS  返回HH:MM:SS
*/
static void timeFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  DateTime x;
  if( isDate(context, argc, argv, &x)==0 ){
    char zBuf[100];
    computeHMS(&x);
    sqlite3_snprintf(sizeof(zBuf), zBuf, "%02d:%02d:%02d", x.h, x.m, (int)x.s);
    sqlite3_result_text(context, zBuf, -1, SQLITE_TRANSIENT);
  }
}

/*
**    date( TIMESTRING, MOD, MOD, ...)   日期（时间字符串，模数，模数，...）
**
** Return YYYY-MM-DD  返回YYYY-MM-DD
*/
static void dateFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  DateTime x;
  if( isDate(context, argc, argv, &x)==0 ){
    char zBuf[100];
    computeYMD(&x);
    sqlite3_snprintf(sizeof(zBuf), zBuf, "%04d-%02d-%02d", x.Y, x.M, x.D);
    sqlite3_result_text(context, zBuf, -1, SQLITE_TRANSIENT);
  }
}

/*
**    strftime( FORMAT, TIMESTRING, MOD, MOD, ...)  格式化时间字符串（格式，时间字符串，模数，模数，...）
**
** Return a string described by FORMAT.  Conversions as follows:  返回一个格式描述的字符串。
**
**   %d  day of month                      月的一天
**   %f  ** fractional seconds  SS.SSS     小数秒  SS.SSS
**   %H  hour 00-24                        小时   00-24 
**   %j  day of year 000-366               年的一天   000-366
**   %J  ** Julian day number              朱利安日数
**   %m  month 01-12                       月份   01-12
**   %M  minute 00-59                      分钟   00-59
**   %s  seconds since 1970-01-01          自1970-01-01以来的秒
**   %S  seconds 00-59                     秒   00-59
**   %w  day of week 0-6  sunday==0        周的一天  0-6  sunday==0
**   %W  week of year 00-53                年的一周  00-53
**   %Y  year 0000-9999                    年   0000-9999
**   %%  %
*/
static void strftimeFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  DateTime x;
  u64 n;
  size_t i,j;
  char *z;
  sqlite3 *db;
  const char *zFmt = (const char*)sqlite3_value_text(argv[0]);
  char zBuf[100];
  if( zFmt==0 || isDate(context, argc-1, argv+1, &x) ) return;
  db = sqlite3_context_db_handle(context);
  for(i=0, n=1; zFmt[i]; i++, n++){
    if( zFmt[i]=='%' ){
      switch( zFmt[i+1] ){
        case 'd':
        case 'H':
        case 'm':
        case 'M':
        case 'S':
        case 'W':
          n++;
          /* fall thru */
        case 'w':
        case '%':
          break;
        case 'f':
          n += 8;
          break;
        case 'j':
          n += 3;
          break;
        case 'Y':
          n += 8;
          break;
        case 's':
        case 'J':
          n += 50;
          break;
        default:
          return;  /* ERROR.  return a NULL */    出现错误。返回一个null
      }
      i++;
    }
  }
  testcase( n==sizeof(zBuf)-1 );
  testcase( n==sizeof(zBuf) );
  testcase( n==(u64)db->aLimit[SQLITE_LIMIT_LENGTH]+1 );
  testcase( n==(u64)db->aLimit[SQLITE_LIMIT_LENGTH] );
  if( n<sizeof(zBuf) ){
    z = zBuf;
  }else if( n>(u64)db->aLimit[SQLITE_LIMIT_LENGTH] ){
    sqlite3_result_error_toobig(context);
    return;
  }else{
    z = sqlite3DbMallocRaw(db, (int)n);
    if( z==0 ){
      sqlite3_result_error_nomem(context);
      return;
    }
  }
  computeJD(&x);
  computeYMD_HMS(&x);
  for(i=j=0; zFmt[i]; i++){
    if( zFmt[i]!='%' ){
      z[j++] = zFmt[i];
    }else{
      i++;
      switch( zFmt[i] ){
        case 'd':  sqlite3_snprintf(3, &z[j],"%02d",x.D); j+=2; break;
        case 'f': {
          double s = x.s;
          if( s>59.999 ) s = 59.999;
          sqlite3_snprintf(7, &z[j],"%06.3f", s);
          j += sqlite3Strlen30(&z[j]);
          break;
        }
        case 'H':  sqlite3_snprintf(3, &z[j],"%02d",x.h); j+=2; break;
        case 'W': /* Fall thru */
        case 'j': {
          int nDay;             /* Number of days since 1st day of year */ 从年的第一天起的天数
          DateTime y = x;
          y.validJD = 0;
          y.M = 1;
          y.D = 1;
          computeJD(&y);
          nDay = (int)((x.iJD-y.iJD+43200000)/86400000);
          if( zFmt[i]=='W' ){
            int wd;   /* 0=Monday, 1=Tuesday, ... 6=Sunday */
            wd = (int)(((x.iJD+43200000)/86400000)%7);
            sqlite3_snprintf(3, &z[j],"%02d",(nDay+7-wd)/7);
            j += 2;
          }else{
            sqlite3_snprintf(4, &z[j],"%03d",nDay+1);
            j += 3;
          }
          break;
        }
        case 'J': {
          sqlite3_snprintf(20, &z[j],"%.16g",x.iJD/86400000.0);
          j+=sqlite3Strlen30(&z[j]);
          break;
        }
        case 'm':  sqlite3_snprintf(3, &z[j],"%02d",x.M); j+=2; break;
        case 'M':  sqlite3_snprintf(3, &z[j],"%02d",x.m); j+=2; break;
        case 's': {
          sqlite3_snprintf(30,&z[j],"%lld",
                           (i64)(x.iJD/1000 - 21086676*(i64)10000));
          j += sqlite3Strlen30(&z[j]);
          break;
        }
        case 'S':  sqlite3_snprintf(3,&z[j],"%02d",(int)x.s); j+=2; break;
        case 'w': {
          z[j++] = (char)(((x.iJD+129600000)/86400000) % 7) + '0';
          break;
        }
        case 'Y': {
          sqlite3_snprintf(5,&z[j],"%04d",x.Y); j+=sqlite3Strlen30(&z[j]);
          break;
        }
        default:   z[j++] = '%'; break;
      }
    }
  }
  z[j] = 0;
  sqlite3_result_text(context, z, -1,
                      z==zBuf ? SQLITE_TRANSIENT : SQLITE_DYNAMIC);
}

/*
** current_time()
**
** This function returns the same value as time('now').  这个函数返回与现在时间相同的值。
*/
static void ctimeFunc(
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **NotUsed2
){
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  timeFunc(context, 0, 0);
}

/*
** current_date()
**
** This function returns the same value as date('now').  这个函数返回与现在日期相同的值。
*/
static void cdateFunc(
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **NotUsed2
){
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  dateFunc(context, 0, 0);
}

/*
** current_timestamp()
**
** This function returns the same value as datetime('now').  这个函数返回与现在日期时间相同的值。
*/
static void ctimestampFunc(
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **NotUsed2
){
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  datetimeFunc(context, 0, 0);
}
#endif /* !defined(SQLITE_OMIT_DATETIME_FUNCS) */  定义(SQLITE_OMIT_DATETIME_FUNCS)

#ifdef SQLITE_OMIT_DATETIME_FUNCS
/*
** If the library is compiled to omit the full-scale date and time
** handling (to get a smaller binary), the following minimal version
** of the functions current_time(), current_date() and current_timestamp()
** are included instead. This is to support column declarations that
** include "DEFAULT CURRENT_TIME" etc.  如果文库被编译的省略了全面的日期和时间处理（得到一个更小的二进制），以下current_time(), current_date() 和 current_timestamp()这些函数的最小版本被包含代替。这是为了支持包含函数"DEFAULT CURRENT_TIME"等的列声明。
**
** This function uses the C-library functions time(), gmtime()
** and strftime(). The format string to pass to strftime() is supplied
** as the user-data for the function.   这个函数使用了C文库的time()函数、gmtime()函数和strftime()函数。格式字符串传递给strftime()作为函数的用户数据。
*/
static void currentTimeFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  time_t t;
  char *zFormat = (char *)sqlite3_user_data(context);
  sqlite3 *db;
  sqlite3_int64 iT;
  struct tm *pTm;
  struct tm sNow;
  char zBuf[20];

  UNUSED_PARAMETER(argc);
  UNUSED_PARAMETER(argv);

  db = sqlite3_context_db_handle(context);
  if( sqlite3OsCurrentTimeInt64(db->pVfs, &iT) ) return;
  t = iT/1000 - 10000*(sqlite3_int64)21086676;
#ifdef HAVE_GMTIME_R
  pTm = gmtime_r(&t, &sNow);
#else
  sqlite3_mutex_enter(sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER));
  pTm = gmtime(&t);
  if( pTm ) memcpy(&sNow, pTm, sizeof(sNow));
  sqlite3_mutex_leave(sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER));
#endif
  if( pTm ){
    strftime(zBuf, 20, zFormat, &sNow);
    sqlite3_result_text(context, zBuf, -1, SQLITE_TRANSIENT);
  }
}
#endif

/*
** This function registered all of the above C functions as SQL
** functions.  This should be the only routine in this file with
** external linkage.   这个函数注册以上所有的C函数为SQL函数。这应该是与外部联系的唯一程序。
*/
void sqlite3RegisterDateTimeFunctions(void){
  static SQLITE_WSD FuncDef aDateTimeFuncs[] = {
#ifndef SQLITE_OMIT_DATETIME_FUNCS
    FUNCTION(julianday,        -1, 0, 0, juliandayFunc ),
    FUNCTION(date,             -1, 0, 0, dateFunc      ),
    FUNCTION(time,             -1, 0, 0, timeFunc      ),
    FUNCTION(datetime,         -1, 0, 0, datetimeFunc  ),
    FUNCTION(strftime,         -1, 0, 0, strftimeFunc  ),
    FUNCTION(current_time,      0, 0, 0, ctimeFunc     ),
    FUNCTION(current_timestamp, 0, 0, 0, ctimestampFunc),
    FUNCTION(current_date,      0, 0, 0, cdateFunc     ),
#else
    STR_FUNCTION(current_time,      0, "%H:%M:%S",          0, currentTimeFunc),
    STR_FUNCTION(current_date,      0, "%Y-%m-%d",          0, currentTimeFunc),
    STR_FUNCTION(current_timestamp, 0, "%Y-%m-%d %H:%M:%S", 0, currentTimeFunc),
#endif
  };
  int i;
  FuncDefHash *pHash = &GLOBAL(FuncDefHash, sqlite3GlobalFunctions);
  FuncDef *aFunc = (FuncDef*)&GLOBAL(FuncDef, aDateTimeFuncs);

  for(i=0; i<ArraySize(aDateTimeFuncs); i++){
    sqlite3FuncDefInsert(pHash, &aFunc[i]);
  }
}
