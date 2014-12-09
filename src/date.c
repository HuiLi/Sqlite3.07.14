/*
** 2003 October 31      
** 2003年10月31日
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing: 
** 作者本人放弃此代码的版权，在任何有法律的地方，这里给使用SQLite的人以下的祝福： 
**    May you do good and not evil. 
**    愿你行善莫行恶。 
**    May you find forgiveness for yourself and forgive others.
**    愿你原谅自己宽恕他人。 
**    May you share freely, never taking more than you give.
**    愿你宽心与人分享，索取不多于你所施予。 
*************************************************************************
** This file contains the C functions that implement date and time
** functions for SQLite.  
** SQLite中的这个文件包含C函数实现的日期和时间函数。 
** There is only one exported symbol in this file - the function
** sqlite3RegisterDateTimeFunctions() found at the bottom of the file.
** 在sqlite3RegisterDateTimeFunctions()文件中只有一个出口标志，在文件的底部发现。
** All other code has file scope.
** 所有其他代码文件代码 
** SQLite processes all times and dates as Julian Day numbers.  The
** dates and times are stored as the number of days since noon
** in Greenwich on November 24, 4714 B.C. according to the Gregorian
** calendar system. 
** SQLite处理所有时间和日期作为儒略日数。这日期和时间被保存从格林威治时间公 
** 元前4714年11月24号中午起，根据公历体系。 
** 1970-01-01 00:00:00 is JD 2440587.5
** 1970-01-01 00:00:00用儒略日表示是2440587.5 
** 2000-01-01 00:00:00 is JD 2451544.5
** 2000-01-01 00:00:00用儒略日表示是2451544.5
** This implemention requires years to be expressed as a 4-digit number
** which means that only dates between 0000-01-01 and 9999-12-31 can
** be represented, even though julian day numbers allow a much wider
** range of dates.
** 这种实现需要年被4个数字表示，意味着日期只能被表示在0000-01-01和9999-12-31之
** 间，即使儒略日数允许表示更大范围内的日期。 
** The Gregorian calendar system is used for all dates and times,
** even those that predate the Gregorian calendar.  Historians usually
** use the Julian calendar for dates prior to 1582-10-15 and for some
** dates afterwards, depending on locale.  Beware of this difference.
** 公历系统是用于所有的日期和时间，即使那些早于公历。历史学家通常使用儒略日日历
** 就日期而言在1582-10-15之前 和一些日期之后，根据现场。注意这一差异。 
** The conversion algorithms are implemented based on descriptions
** in the following text:
** 转换算法是根据下面的文本描述来实现的： 
**      Jean Meeus
**      Astronomical Algorithms, 2nd Edition, 1998
**      ISBM 0-943396-61-1
**      Willmann-Bell, Inc
**      Richmond, Virginia (USA)
** Jean Meeus《天文算法》，第二版，1998 ISBM 0-943396-61-1 Willmann钟，
** 有限公司里士满，弗吉尼亚州（美国） 
*/
#include "sqliteInt.h"                /*sqliteInt头文件*/ 
#include <stdlib.h>                  /*stdlib头文件*/
#include <assert.h>                 /*assert头文件*/
#include <time.h>                  /*time头文件*/

#ifndef SQLITE_OMIT_DATETIME_FUNCS  
 /*先测试SQLITE_OMIT_DATETIME_FUNCS是否被宏定义过*/  

/*
** A structure for holding a single date and time.
** 用于保持一个单一的日期和时间结构。 
*/
typedef struct DateTime DateTime; /*struct DateTime结构体重命名为DateTime*/
struct DateTime {
  sqlite3_int64 iJD; /* The julian day number times 86400000 儒略日表示一天的毫秒数*/
  int Y, M, D;       /* Year, month, and day 定义变量年，月，日*/
  int h, m;          /* Hour and minutes 定义变量小时和分钟*/
  int tz;            /* Timezone offset in minutes 在分钟的时区偏移*/
  double s;          /* Seconds 定义变量秒*/
  char validYMD;     /* True (1) if Y,M,D are valid 判断年月日是否有效*/
  char validHMS;     /* True (1) if h,m,s are valid 判断小时、分钟和秒是否有效*/
  char validJD;      /* True (1) if iJD is valid 判断毫秒数iJD是否有效*/
  char validTZ;      /* True (1) if tz is valid 判断偏移量tz是否有效*/
};


/*
** Convert zDate into one or more integers.  Additional arguments
** come in groups of 5 as follows:
** zDate中包含一个或是多个整数。其中的5个参数的作用如下： 
**       N       number of digits in the integer
**       N表示整数的位数 
**       min     minimum allowed value of the integer
**       min表示整数中允许的最小值 
**       max     maximum allowed value of the integer
**       max表示整数中允许的最大值 
**       nextC   first character after the integer
**       nextC表示指向后面整数的第一个字符 
**       pVal    where to write the integers value.
**       pVal表示存储整数值的地方 
** Conversions continue until one with nextC==0 is encountered.
** 循环一直持续到nextC==0才停止。 
** The function returns the number of successful conversions.
** 返回函数成功循环的次数。 
*/
static int getDigits(const char *zDate, ...){
  va_list ap;      /*定义一具va_list型的变量ap，这个变量ap是指向参数的指针*/
  int val;
  int N;
  int min;
  int max;
  int nextC;
  int *pVal;
  int cnt = 0;                        /*记录函数循环的次数*/
  va_start(ap, zDate);    /*用va_start宏初始化变量刚定义的va_list变量ap*/
  do{
    N = va_arg(ap, int); /*用va_arg返回可变的参数，va_arg的第二个参数是返回的参数的类型*/
    min = va_arg(ap, int);
    max = va_arg(ap, int);
    nextC = va_arg(ap, int);
    pVal = va_arg(ap, int*);
    val = 0;
    while( N-- ){
      if( !sqlite3Isdigit(*zDate) ){ /*判断是否是数字，不是数字执行该if语句*/
        goto end_getDigits;          /*执行goto指定标记end_getDigits后的语句*/
      }
      val = val*10 + *zDate - '0';
      zDate++;
    }
    if( val<min || val>max || (nextC!=0 && nextC!=*zDate) ){/*判断val是否合理，val不在给定的范围就执行该if语句*/
      goto end_getDigits;
    }
    *pVal = val;
    zDate++;
    cnt++;
  }while( nextC );
end_getDigits:
  va_end(ap);      /*用va_end宏结束可变参数的获取*/
  return cnt;      /*返回函数成功循环的次数cnt*/
}

/*
** Parse a timezone extension on the end of a date-time.解析日期时间结束一个时区的扩展。
** The extension is of the form:  扩展的形式如下：
**
**        (+/-)HH:MM
**
** Or the "zulu" notation:    或“zulu”的符号如下：
**
**        Z
**
** If the parse is successful, write the number of minutes
** of change in p->tz and return 0.  If a parser error occurs,
** return non-zero.如果分析是成功的，把改变后的分钟数写到偏移量P - > TZ中和返回0。
** 如果发生一个分析器错误时，返回非零。 
** A missing specifier is not considered an error.丢失的说明符不认为是一个错误。
*/
static int parseTimezone(const char *zDate, DateTime *p){
  int sgn = 0;
  int nHr, nMn;
  int c;
  while( sqlite3Isspace(*zDate) ){ zDate++; } /*检测是否为空格字符，若是空格字符就执行该语句，否则不执行。*/
  p->tz = 0;   
  c = *zDate;
  if( c=='-' ){ /*判断是否是+/-和Z，是就执行下面语句，也是时间开始的标志，否则直接返回非零*/
    sgn = -1;
  }else if( c=='+' ){
    sgn = +1;
  }else if( c=='Z' || c=='z' ){
    zDate++; 
    goto zulu_time;        /*执行goto指定标记end_getDigits后的语句*/
  }else{
    return c!=0;
  }
  zDate++;    /*向前移动一位正式开始*/
  if( getDigits(zDate, 2, 0, 14, ':', &nHr, 2, 0, 59, 0, &nMn)!=2 ){ /*对getDigits函数的调用并判断*/ 
    return 1;
  }
  zDate += 5;     /*HH:MM读取了五位*/
  p->tz = sgn*(nMn + nHr*60);   /*把改变后的分钟数写到偏移量P - > TZ中*/
zulu_time:
  while( sqlite3Isspace(*zDate) ){ zDate++; }  /*检测是否为空格字符，若是空格字符就执行该语句，否则不执行。*/
  return *zDate!=0;
}

/*
** Parse times of the form HH:MM or HH:MM:SS or HH:MM:SS.FFFF.
** 解析时间形式为HH:MM或HH：MM：SS或HH：MM：ss.ffff。
** The HH, MM, and SS must each be exactly 2 digits.  The
** fractional seconds FFFF can be one or more digits.
** HH，MM,SS必须都是2位数。秒的小数部分FFFF可以是一个或多个数字。
** Return 1 if there is a parsing error and 0 on success.如果有语法错误就返回1，正确的就返回0。
*/
static int parseHhMmSs(const char *zDate, DateTime *p){
  int h, m, s;
  double ms = 0.0;
  if( getDigits(zDate, 2, 0, 24, ':', &h, 2, 0, 59, 0, &m)!=2 ){ /*对getDigits函数的调用并判断*/ 
    return 1;
  }
  zDate += 5;    /*HH:MM读取了五位*/
  if( *zDate==':' ){/*判断是否是：，是就执行下面语句，否则不执行*/
    zDate++;
    if( getDigits(zDate, 2, 0, 59, 0, &s)!=1 ){
      return 1;
    }
    zDate += 2;
    if( *zDate=='.' && sqlite3Isdigit(zDate[1]) ){/*判断是否是.，是就执行下面语句获取毫秒的值，否则执行else下的语句并给秒s赋值为0*/
      double rScale = 1.0;
      zDate++;    /*向前移动一位*/
      while( sqlite3Isdigit(*zDate) ){  /*判断是否是数字，直至不是数字就跳出该while循环*/
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
  if( parseTimezone(zDate, p) ) return 1;   /*对parseTimezone函数的调用并判断*/
  p->validTZ = (p->tz!=0)?1:0;   /*偏移量*/ 
  return 0;
}

/*
** Convert from YYYY-MM-DD HH:MM:SS to julian day.  We always assume
** that the YYYY-MM-DD is according to the Gregorian calendar.
** 把YYYY-MM-DD HH:MM:SS转换为儒略日。我们总是假定YYYY-MM-DD是根据公历的。 
** Reference:  Meeus page 61  参考文献：书61页
*/
static void computeJD(DateTime *p){
  int Y, M, D, A, B, X1, X2;

  if( p->validJD ) return;
  if( p->validYMD ){  /*指定有效的年月日*/
    Y = p->Y;
    M = p->M;
    D = p->D;
  }else{
    Y = 2000;  /* If no YMD specified, assume 2000-Jan-01 如果年月日没有指定，我们就假定为2000年1月1日*/
    M = 1;
    D = 1;
  }
  if( M<=2 ){ /*如果月份不大于2，年份自减呀，月份加12*/ 
    Y--;
    M += 12;
  }
  A = Y/100;
  B = 2 - A + (A/4);
  X1 = 36525*(Y+4716)/100;
  X2 = 306001*(M+1)/10000;
  p->iJD = (sqlite3_int64)((X1 + X2 + D + B - 1524.5 ) * 86400000);/*把时间换算为儒略日的毫秒时间*/
  p->validJD = 1;
  if( p->validHMS ){  /*如果validHMS值有效，就把小时、分钟和秒换算成毫秒并累加*/
    p->iJD += p->h*3600000 + p->m*60000 + (sqlite3_int64)(p->s*1000);
    if( p->validTZ ){  /*如果validTZ值有效，把正常时间转换为儒略日时间，最后把validYMD、validHMS和validTZ都赋值为0*/
      p->iJD -= p->tz*60000;
      p->validYMD = 0;
      p->validHMS = 0;
      p->validTZ = 0;
    }
  }
}

/*
** Parse dates of the form 解析日期的形式
**
**     YYYY-MM-DD HH:MM:SS.FFF
**     YYYY-MM-DD HH:MM:SS
**     YYYY-MM-DD HH:MM
**     YYYY-MM-DD
**
** Write the result into the DateTime structure and return 0
** on success and 1 if the input string is not a well-formed
** date.结果写入DateTime结构体中并成功就返回0，如果输入字符串不是一个符合语法规则的就返回1。
*/
static int parseYyyyMmDd(const char *zDate, DateTime *p){
  int Y, M, D, neg;

  if( zDate[0]=='-' ){
    zDate++;
    neg = 1;
  }else{
    neg = 0;
  }
  if( getDigits(zDate,4,0,9999,'-',&Y,2,1,12,'-',&M,2,1,31,0,&D)!=3 ){/*对getDigits函数的调用并判断*/
    return 1;
  }
  zDate += 10; /*YYYY-MM-DD读取了十位*/
  while( sqlite3Isspace(*zDate) || 'T'==*(u8*)zDate ){ zDate++; }
  if( parseHhMmSs(zDate, p)==0 ){
    /* We got the time 我们得到了时间*/
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
  if( p->validTZ ){ /*如果validTZ值有效，就调用computeJD函数，否则不执行*/
    computeJD(p);
  }
  return 0;
}

/*
** Set the time to the current time reported by the VFS.
** 根据VFS设置当前的时间。 
** Return the number of errors. 返回错误的数。 
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
** the number of errors.
** 试图把字符串解析成儒略日数。返回错误的数。 
** The following are acceptable forms for the input string:
** 以下是可以被接受的输入字符串的形式： 
**      YYYY-MM-DD HH:MM:SS.FFF  +/-HH:MM
**      DDDD.DD 
**      now
**
** In the first form, the +/-HH:MM is always optional.  The fractional
** seconds extension (the ".FFF") is optional.  The seconds portion
** (":SS.FFF") is option.  The year and date can be omitted as long
** as there is a time string.  The time string can be omitted as long
** as there is a year and date.
** 在第一种形式中，+/-HH:MM始终是可选的。秒的小数部分扩展（".FFF"）是可选的。 
** 秒的小数部分(":SS.FFF")是可选的。年和日期是可以省略的，只要有时间字符串。或是
** 时间字符串是可以省略的，只要有时间和日期。 
*/
static int parseDateOrTime(
  sqlite3_context *context, 
  const char *zDate, 
  DateTime *p
){
  double r;
  if( parseYyyyMmDd(zDate,p)==0 ){ /*调用parseYyyyMmDd函数并判断*/
    return 0;
  }else if( parseHhMmSs(zDate, p)==0 ){ /*调用parseHhMmSs函数并判断*/
    return 0;
  }else if( sqlite3StrICmp(zDate,"now")==0){ /*判断zDate是否与当前值相同，相同就返回当前时间，否则执行下面的else语句计算毫秒值*/
    return setDateTimeToCurrent(context, p);
  }else if( sqlite3AtoF(zDate, &r, sqlite3Strlen30(zDate), SQLITE_UTF8) ){
    p->iJD = (sqlite3_int64)(r*86400000.0 + 0.5);
    p->validJD = 1;
    return 0;
  }
  return 1;
}

/*
** Compute the Year, Month, and Day from the julian day number.把儒略日数换算为正常时间的年月日。 
*/
static void computeYMD(DateTime *p){
  int Z, A, B, C, D, E, X1;
  if( p->validYMD ) return; /* 如果validYMD有效，就直接返回值，否则就执行下面语句把儒略日数换算为正常时间的年月日*/
  if( !p->validJD ){ /* 如果validJD无效，我们就假定为2000年1月1日，否则就执行下面的else语句*/
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
** Compute the Hour, Minute, and Seconds from the julian day number.把儒略日数转换为正常时间的小时，分钟和秒。
*/
static void computeHMS(DateTime *p){
  int s;
  if( p->validHMS ) return; /* 如果validHMS有效，就直接返回值，否则就执行下面语句把儒略日数换算为正常时间的小时，分钟和秒*/
  computeJD(p); /*调用computeJD函数*/
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
** Compute both YMD and HMS 计算年月日和小时、分钟和秒。 
*/
static void computeYMD_HMS(DateTime *p){
  computeYMD(p);           /*调用computeYMD()函数*/
  computeHMS(p);           /*调用computeHMS()函数*/
}

/*
** Clear the YMD and HMS and the TZ 清除年月日和小时、分钟、秒和TZ
*/
static void clearYMD_HMS_TZ(DateTime *p){
  p->validYMD = 0;               /*validYMD清零*/
  p->validHMS = 0;              /*validHMS清零*/
  p->validTZ = 0;               /*validTZ清零*/
}

/*
** On recent Windows platforms, the localtime_s() function is available
** as part of the "Secure CRT". It is essentially equivalent to 
** localtime_r() available under most POSIX platforms, except that the 
** order of the parameters is reversed.
** 在最近的Windows平台，localtime_s()功能可作为“Secure CRT”部分是有效的。
** 它本质上和localtime_r()在大部分POSIX平台下可用是等价的，但参数的顺序是相反的。 
** See http://msdn.microsoft.com/en-us/library/a442x3ye(VS.80).aspx.
** 查看网址 http://msdn.microsoft.com/en-us/library/a442x3ye(VS.80).aspx.
** If the user has not indicated to use localtime_r() or localtime_s()
** already, check for an MSVC build environment that provides 
** localtime_s().  如果用户没有表明已经使用localtime_r()或localtime_s()，
** 检查MSVC（就是vc++）环境建立，环境提供localtime_s()。
*/
#if !defined(HAVE_LOCALTIME_R) && !defined(HAVE_LOCALTIME_S) && \
     defined(_MSC_VER) && defined(_CRT_INSECURE_DEPRECATE)
#define HAVE_LOCALTIME_S 1
#endif   
/*判断如果HAVE_LOCALTIME_R、HAVE_LOCALTIME_S没有被定义，_MSC_VER、
       _CRT_INSECURE_DEPRECATE被定义，就用HAVE_LOCALTIME_S代替1*/

#ifndef SQLITE_OMIT_LOCALTIME  /*先测试SQLITE_OMIT_LOCALTIME是否被宏定义过，没有定义过就执行下面的程序段直到结束*/
/*
** The following routine implements the rough equivalent of localtime_r()
** using whatever operating-system specific localtime facility that
** is available.  This routine returns 0 on success and
** non-zero on any kind of error.
** 下面的程序大致实现localtime_r()的用法，无论在任何操作系统下的具体地方时间设施
** 是可用的。这个程序成功就返回0，只要发生任何一种错误就返回非零。 
** If the sqlite3GlobalConfig.bLocaltimeFault variable is true then this
** routine will always fail. 如果sqlite3GlobalConfig.bLocaltimeFault变量时真的话，这个程序将一直是失败的。 
*/
static int osLocaltime(time_t *t, struct tm *pTm){
  int rc;
#if (!defined(HAVE_LOCALTIME_R) || !HAVE_LOCALTIME_R) \
      && (!defined(HAVE_LOCALTIME_S) || !HAVE_LOCALTIME_S)  /*判断如果HAVE_LOCALTIME_R、HAVE_LOCALTIME_S没有被定义，HAVE_LOCALTIME_R、HAVE_LOCALTIME_S值为假，就执行后面离它最近else前的语句，否则就执行else后的语句。*/
  struct tm *pX;
#if SQLITE_THREADSAFE>0 /*如果SQLITE_THREADSAFE>0成立，执行后面离它最近endif前的语句，否则不执行。*/
  sqlite3_mutex *mutex = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER);
#endif
  sqlite3_mutex_enter(mutex);
  pX = localtime(t);
#ifndef SQLITE_OMIT_BUILTIN_TEST /*先测试SQLITE_OMIT_BUILTIN_TEST是否被宏定义过，没有定义过就执行后面离它最近endif前的语句，否则不执行*/
  if( sqlite3GlobalConfig.bLocaltimeFault ) pX = 0;
#endif
  if( pX ) *pTm = *pX; 
  sqlite3_mutex_leave(mutex);
  rc = pX==0;
#else
#ifndef SQLITE_OMIT_BUILTIN_TEST /*先测试SQLITE_OMIT_BUILTIN_TEST是否被宏定义过，没有定义过就执行后面离它最近endif前的语句，否则不执行*/
  if( sqlite3GlobalConfig.bLocaltimeFault ) return 1;
#endif
#if defined(HAVE_LOCALTIME_R) && HAVE_LOCALTIME_R
  rc = localtime_r(t, pTm)==0;
#else
  rc = localtime_s(pTm, t);
#endif /* HAVE_LOCALTIME_R 判断执行停止的地方。*/
#endif /* HAVE_LOCALTIME_R || HAVE_LOCALTIME_S 判断执行停止的地方。*/
  return rc;
}
#endif /* SQLITE_OMIT_LOCALTIME 判断执行停止的地方。*/


#ifndef SQLITE_OMIT_LOCALTIME  /*先测试SQLITE_OMIT_LOCALTIME是否被宏定义过，没有定义过就执行下面的程序段直到结束*/
/*
** Compute the difference (in milliseconds) between localtime and UTC
** (a.k.a. GMT) for the time value p where p is in UTC. If no error occurs,
** return this value and set *pRc to SQLITE_OK. 
** 计算当地时间与UTC（即格林尼治标准时间）时间值间的差异（在毫秒上）。如果没有发生错误，把QLITE_OK赋值给*pRc。 
** Or, if an error does occur, set *pRc to SQLITE_ERROR. The returned value
** is undefined in this case. 或者，如果发生错误，把SQLITE_ERROR赋值给*pRc。在这种情况下返回的值是不确定的 
*/
static sqlite3_int64 localtimeOffset(
  DateTime *p,                    /* Date at which to calculate offset 计算日期的偏移*/
  sqlite3_context *pCtx,          /* Write error here if one occurs 如果发生错误，就把错误写入该指针*/
  int *pRc                        /* OUT: Error code. SQLITE_OK or ERROR 存放该程序段结果错误与正确的地方*/
){
  DateTime x, y;
  time_t t;
  struct tm sLocal;

  /* Initialize the contents of sLocal to avoid a compiler warning. 初始化sLocal内容以避免编译器警告。*/
  memset(&sLocal, 0, sizeof(sLocal));

  x = *p;
  computeYMD_HMS(&x);    /*调用computeYMD_HMS函数*/
  if( x.Y<1971 || x.Y>=2038 ){     /*如果时间不在1971和2038年间，就直接赋值时间为2000/1/1 0:0:0.0,否则执行下面else后面的语句*/
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
  computeJD(&x);   /*调用computeJD函数*/
  t = (time_t)(x.iJD/1000 - 21086676*(i64)10000);
  if( osLocaltime(&t, &sLocal) ){  /*判断osLocaltime，为真执行if中的语句并返回值，否则执行if后面的语句并获取系统当前时间*/
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
#endif /* SQLITE_OMIT_LOCALTIME 判断执行停止的地方。*/

/*
** Process a modifier to a date-time stamp.  The modifiers are
** as follows: 对日期时间戳的改良方法。编辑器有一下几种： 
**
**     NNN days
**     NNN hours
**     NNN minutes
**     NNN.NNNN seconds
**     NNN months
**     NNN years
**     start of month
**     start of year
**     start of week
**     start of day
**     weekday N
**     unixepoch
**     localtime
**     utc
**
** Return 0 on success and 1 if there is any kind of error. If the error
** is in a system call (i.e. localtime()), then an error message is written
** to context pCtx. If the error is an unrecognized modifier, no error is
** written to pCtx. 成功就返回0，如果发生任何一个错误就返回1.如果错误发生在系统调用上（即localtime()，然后就把该错误信息写到pCtx中。如果错误无法识别修正 ，该错误就不写入pCtx中。） 
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
#ifndef SQLITE_OMIT_LOCALTIME /*先测试SQLITE_OMIT_LOCALTIME是否被宏定义过，没有定义过就执行后面离它最近endif前的语句，否则不执行*/
    case 'l': {
      /*    localtime  当地时间
      **
      ** Assuming the current time value is UTC (a.k.a. GMT), shift it to
      ** show local time. 假如当前时间值是UTC（即格林尼治标准时间），就把它转换成显示本地时间。
      */
      if( strcmp(z, "localtime")==0 ){   /*比较z和当前时间localtime是否一致，一致就执行if内的语句，否则不执行*/
        computeJD(p);  /*调用computeJD函数*/
        p->iJD += localtimeOffset(p, pCtx, &rc); /*p->iJD加上localtimeOffset函数计算出的时间偏差*/
        clearYMD_HMS_TZ(p);  /*调用clearYMD_HMS_TZ函数清零*/
      }
      break;
    }
#endif
    case 'u': {
      /*
      **    unixepoch
      **
      ** Treat the current value of p->iJD as the number of 自1970年来p->iJD的当前值被当作秒数。 
      ** seconds since 1970.  Convert to a real julian day number.把它转换成一个真正的儒略日数。 
      */
      if( strcmp(z, "unixepoch")==0 && p->validJD ){  /*z和unixepoch一致并且validJD有效，就执行if内的语句，否则不执行*/
        p->iJD = (p->iJD + 43200)/86400 + 21086676*(i64)10000000;
        clearYMD_HMS_TZ(p);  /*调用clearYMD_HMS_TZ函数清零*/
        rc = 0;
      }
#ifndef SQLITE_OMIT_LOCALTIME  /*先测试SQLITE_OMIT_LOCALTIME是否被宏定义过，没有定义过就执行后面离它最近endif前的语句，否则不执行*/
      else if( strcmp(z, "utc")==0 ){  /*比较z和utc是否一致，一致就执行if内的语句，否则不执行*/ 
        sqlite3_int64 c1;
        computeJD(p); /*调用computeJD函数*/
        c1 = localtimeOffset(p, pCtx, &rc);  /*用localtimeOffset函数计算出的时间偏差并赋值给c1*/
        if( rc==SQLITE_OK ){
          p->iJD -= c1;
          clearYMD_HMS_TZ(p); /*调用clearYMD_HMS_TZ函数清零*/
          p->iJD += c1 - localtimeOffset(p, pCtx, &rc);
        }
      }
#endif
      break;
    }
    case 'w': {
      /*
      **    weekday N  工作日 
      ** 在移动日期的同时接下来发生发生0代表Sunday，1代表Monday等等。如果日期已经是一个合适的工作日，就表示这是一个空操作。 
      ** Move the date to the same time on the next occurrence of
      ** weekday N where 0==Sunday, 1==Monday, and so forth.  If the
      ** date is already on the appropriate weekday, this is a no-op.
      */
      if( strncmp(z, "weekday ", 8)==0
               && sqlite3AtoF(&z[8], &r, sqlite3Strlen30(&z[8]), SQLITE_UTF8)
               && (n=(int)r)==r && n>=0 && r<7 ){ /*比较z和weekday的前8字符一致并且sqlite3AtoF为真以及(n=(int)r)==r、 n>=0 、r<7都成立，就执行if内的语句，否则不执行*/
        sqlite3_int64 Z;
        computeYMD_HMS(p);   /*调用computeYMD_HMS函数*/
        p->validTZ = 0;
        p->validJD = 0;
        computeJD(p);       /*调用computeJD函数*/
        Z = ((p->iJD + 129600000)/86400000) % 7;
        if( Z>n ) Z -= 7;
        p->iJD += (n - Z)*86400000;
        clearYMD_HMS_TZ(p);  /*调用clearYMD_HMS_TZ函数清零*/
        rc = 0;
      }
      break;
    }
    case 's': {
      /*
      **    start of TTTTT
      **
      ** Move the date backwards to the beginning of the current day,
      ** or month or year. 移动日期到开始的当前日期之后，或是月或是年。 
      */
      if( strncmp(z, "start of ", 9)!=0 ) break;   /* /*比较z和start of的前9字符不一致就跳出switch循环，否则执行下面语句*/
      z += 9;
      computeYMD(p);  /*调用computeYMD函数*/
      p->validHMS = 1;
      p->h = p->m = 0;
      p->s = 0.0;
      p->validTZ = 0;
      p->validJD = 0;
      if( strcmp(z,"month")==0 ){   /*比较z和month是否一致，一致就执行if内的语句，否则执行下面else中的语句*/
        p->D = 1;
        rc = 0;
      }else if( strcmp(z,"year")==0 ){   /*比较z和year是否一致，一致就执行if内的语句，否则执行下面else中的语句*/
        computeYMD(p);  /*调用computeYMD函数*/
        p->M = 1;
        p->D = 1;
        rc = 0;
      }else if( strcmp(z,"day")==0 ){ /*比较z和day是否一致，一致就执行if内的语句，否则不执行*/
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
        ** omitted.修改(+|-)HH:MM:SS.FFF的形式，增加（或减少）指定小时数，分钟数，秒数和秒的小数部分。".FFF"可以省略。":SS.FFF" 可以省略。
        */
        const char *z2 = z;
        DateTime tx;
        sqlite3_int64 day;
        if( !sqlite3Isdigit(*z2) ) z2++;
        memset(&tx, 0, sizeof(tx));   /*把DateTime结构体对象tx进行清零*/
        if( parseHhMmSs(z2, &tx) ) break;   /*调用parseHhMmSs函数并判断，为真跳出该switch循环，否则执行下面的语句*/
        computeJD(&tx);        /*调用computeJD函数*/
        tx.iJD -= 43200000;
        day = tx.iJD/86400000;
        tx.iJD -= day*86400000;
        if( z[0]=='-' ) tx.iJD = -tx.iJD;
        computeJD(p);          /*调用computeJD函数*/
        clearYMD_HMS_TZ(p);     /*调用clearYMD_HMS_TZ函数清零*/
        p->iJD += tx.iJD;
        rc = 0;
        break;
      }
      z += n;
      while( sqlite3Isspace(*z) ) z++; /*检测是否为空格字符，若是空格字符就执行该语句，否则不执行。*/
      n = sqlite3Strlen30(z);          /*获取字符串z的长度*/
      if( n>10 || n<3 ) break;
      if( z[n-1]=='s' ){ z[n-1] = 0; n--; }
      computeJD(p);     /*调用computeJD函数*/  
      rc = 0;
      rRounder = r<0 ? -0.5 : +0.5;
      if( n==3 && strcmp(z,"day")==0 ){ /*z和day是一致并且n==3，就执行if内的语句，否则执行下面else中的语句*/
        p->iJD += (sqlite3_int64)(r*86400000.0 + rRounder);
      }else if( n==4 && strcmp(z,"hour")==0 ){  /*z和hour是一致并且n==4，就执行if内的语句，否则执行下面else中的语句*/
        p->iJD += (sqlite3_int64)(r*(86400000.0/24.0) + rRounder);
      }else if( n==6 && strcmp(z,"minute")==0 ){  /*z和minute是一致并且n==6，就执行if内的语句，否则执行下面else中的语句*/
        p->iJD += (sqlite3_int64)(r*(86400000.0/(24.0*60.0)) + rRounder);
      }else if( n==6 && strcmp(z,"second")==0 ){  /*z和second是一致并且n==6，就执行if内的语句，否则执行下面else中的语句*/
        p->iJD += (sqlite3_int64)(r*(86400000.0/(24.0*60.0*60.0)) + rRounder);
      }else if( n==5 && strcmp(z,"month")==0 ){ /*z和month是一致并且n==5，就执行if内的语句，否则不执行*/
        int x, y;
        computeYMD_HMS(p);  /*调用computeYMD_HMS函数*/
        p->M += (int)r;
        x = p->M>0 ? (p->M-1)/12 : (p->M-12)/12;
        p->Y += x;
        p->M -= x*12;
        p->validJD = 0;
        computeJD(p);   /*调用computeJD函数*/   
        y = (int)r;
        if( y!=r ){
          p->iJD += (sqlite3_int64)((r - y)*30.0*86400000.0 + rRounder);
        }
      }else if( n==4 && strcmp(z,"year")==0 ){/*z和year是一致并且n==4，就执行if内的语句，否则执行下面else中的语句*/
        int y = (int)r;
        computeYMD_HMS(p);  /*调用computeYMD_HMS函数*/
        p->Y += y;
        p->validJD = 0;
        computeJD(p);    /*调用computeJD函数*/  
        if( y!=r ){     /*判断y与r是否相等，不相等就执行if中的语句，否则执行else中的语句*/ 
          p->iJD += (sqlite3_int64)((r - y)*365.0*86400000.0 + rRounder);
        }
      }else{
        rc = 1;
      }
      clearYMD_HMS_TZ(p);   /*调用clearYMD_HMS_TZ函数清零*/
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
** on success and 1 if there are any errors.处理时间函数参数。argv[0]是一个日期时间戳。argv[1]和接下来的argv数组中的元素是可以修改的。 
** 分析DateTime结构体p中的所有和被写入的时间结果。成功就返回0，如果有任何一个错误就返回1. 
** If there are zero parameters (if even argv[0] is undefined)
** then assume a default value of "now" for argv[0].如果argv[0]有零个参数（甚至argv[0]是未定义的）那么给argv[0]赋一个默认值"now"。
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
  memset(p, 0, sizeof(*p));   /*把DateTime结构体对象p进行清零*/
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
** of SQLite.下面的程序实现SQLite对不同的日期和时间处理的功能。
*/

/*
**    julianday( TIMESTRING, MOD, MOD, ...)儒略日 
**
** Return the julian day number of the date specified in the arguments时间函数以指定的儒略日格式返回 
*/
static void juliandayFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  DateTime x;
  if( isDate(context, argc, argv, &x)==0 ){
    computeJD(&x);      /*调用computeJD函数*/  
    sqlite3_result_double(context, x.iJD/86400000.0);
  }
}

/*
**    datetime( TIMESTRING, MOD, MOD, ...)
**
** Return YYYY-MM-DD HH:MM:SS 返回YYYY-MM-DD HH:MM:SS 
*/
static void datetimeFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  DateTime x;
  if( isDate(context, argc, argv, &x)==0 ){
    char zBuf[100];
    computeYMD_HMS(&x);    /*调用computeYMD_HMS函数*/ 
    sqlite3_snprintf(sizeof(zBuf), zBuf, "%04d-%02d-%02d %02d:%02d:%02d",
                     x.Y, x.M, x.D, x.h, x.m, (int)(x.s));
    sqlite3_result_text(context, zBuf, -1, SQLITE_TRANSIENT);
  }
}

/*
**    time( TIMESTRING, MOD, MOD, ...)
**
** Return HH:MM:SS  以HH:MM:SS形式返回 
*/
static void timeFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  DateTime x;
  if( isDate(context, argc, argv, &x)==0 ){
    char zBuf[100];
    computeHMS(&x);       /*调用computeHMS函数*/ 
    sqlite3_snprintf(sizeof(zBuf), zBuf, "%02d:%02d:%02d", x.h, x.m, (int)x.s);
    sqlite3_result_text(context, zBuf, -1, SQLITE_TRANSIENT);
  }
}

/*
**    date( TIMESTRING, MOD, MOD, ...)
**
** Return YYYY-MM-DD  以YYYY-MM-DD形式返回 
*/
static void dateFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  DateTime x;
  if( isDate(context, argc, argv, &x)==0 ){
    char zBuf[100];
    computeYMD(&x);   /*调用computeYMD函数*/ 
    sqlite3_snprintf(sizeof(zBuf), zBuf, "%04d-%02d-%02d", x.Y, x.M, x.D);
    sqlite3_result_text(context, zBuf, -1, SQLITE_TRANSIENT);
  }
}

/*
**    strftime( FORMAT, TIMESTRING, MOD, MOD, ...)
**
** Return a string described by FORMAT.  Conversions as follows:返回一个字符串的格式描述。转换如下：
**
**   %d  day of month                    %d是月中的某一天的格式 
**   %f  ** fractional seconds  SS.SSS   %f是秒的小数部分SS.SSS的格式 
**   %H  hour 00-24                      %H是用00到24表示小时的格式 
**   %j  day of year 000-366             %j是用000到366表示一年当中的某一天的格式 
**   %J  ** Julian day number            %J是儒略日数的格式 
**   %m  month 01-12                     %m是用01到12表示月的格式 
**   %M  minute 00-59                    %M是用00到59表示分钟的格式 
**   %s  seconds since 1970-01-01        %s是从1970-01-01来秒的格式 
**   %S  seconds 00-59                   %S是用00到59表示秒的格式 
**   %w  day of week 0-6  sunday==0      %w是用0到6表示一周的格式，其中0表示sunday（星期天） 
**   %W  week of year 00-53              %W是用00到53表示一年当中的周数的格式 
**   %Y  year 0000-9999                  %Y是用0000到9999表示年的格式 
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
          return;  /* ERROR.  return a NULL 如果发生错误就返回一个空值*/
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
  computeJD(&x);       /*调用computeJD函数*/
  computeYMD_HMS(&x);  /*调用computeYMD_HMS函数*/
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
          int nDay;             /* Number of days since 1st day of year 从第一年第一天开始的天数*/
          DateTime y = x;
          y.validJD = 0;
          y.M = 1;
          y.D = 1;
          computeJD(&y);   /*调用computeJD函数*/
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
**这个函数返回的值作为（当前的）时间。 
** This function returns the same value as time('now').
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
** 这个函数返回的值作为（当前的）日期。
** This function returns the same value as date('now').
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
** 这个函数返回的值作为（当前的）日期时间。
** This function returns the same value as datetime('now').
*/
static void ctimestampFunc(
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **NotUsed2
){
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  datetimeFunc(context, 0, 0);
}
#endif /* !defined(SQLITE_OMIT_DATETIME_FUNCS) 判断执行停止的地方。*/

#ifdef SQLITE_OMIT_DATETIME_FUNCS  /*先测试SQLITE_OMIT_DATETIME_FUNCS是否被宏定义过*/
/*
** If the library is compiled to omit the full-scale date and time
** handling (to get a smaller binary), the following minimal version
** of the functions current_time(), current_date() and current_timestamp()
** are included instead. This is to support column declarations that
** include "DEFAULT CURRENT_TIME" etc.如果library的编译是省略全部的日期和时间处理（得到一个较小的二进制），
** 下面的最小版本的current_time(), current_date() 和 current_timestamp()函数就会被替代。这是为了支持列的声明，其中包括"DEFAULT CURRENT_TIME"等。
** This function uses the C-library functions time(), gmtime()
** and strftime(). The format string to pass to strftime() is supplied
** as the user-data for the function.这个函数使用C-library中time()，gmtime()和strftime()函数。
** 这个格式字符串被作为用户数据提供给strftime()函数。
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
#ifdef HAVE_GMTIME_R  /*先测试HAVE_GMTIME_R是否被宏定义过，如果被定义了就执行下面的语句，否则就执行else下面的语句。*/
  pTm = gmtime_r(&t, &sNow);
#else
  sqlite3_mutex_enter(sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER));
  pTm = gmtime(&t);
  if( pTm ) memcpy(&sNow, pTm, sizeof(sNow));
  sqlite3_mutex_leave(sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER));
#endif  /* HAVE_GMTIME_R 判断执行停止的地方。*/
  if( pTm ){
    strftime(zBuf, 20, zFormat, &sNow);
    sqlite3_result_text(context, zBuf, -1, SQLITE_TRANSIENT);
  }
}
#endif  /* SQLITE_OMIT_DATETIME_FUNCS 判断执行停止的地方。*/

/*
** This function registered all of the above C functions as SQL
** functions.  This should be the only routine in this file with
** external linkage.这个函数注册所有上述C函数作为SQL的功能。这应该是唯一的函数在这个与外部联系有关的文件。
*/
void sqlite3RegisterDateTimeFunctions(void){
  static SQLITE_WSD FuncDef aDateTimeFuncs[] = {
#ifndef SQLITE_OMIT_DATETIME_FUNCS   /*先测试SQLITE_OMIT_DATETIME_FUNCS是否被宏定义过，如果没有被定义了就执行下面的语句，否则就执行else下面的语句。*/
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
#endif  /* SQLITE_OMIT_DATETIME_FUNCS 判断执行停止的地方。*/
  };
  int i;
  FuncDefHash *pHash = &GLOBAL(FuncDefHash, sqlite3GlobalFunctions);
  FuncDef *aFunc = (FuncDef*)&GLOBAL(FuncDef, aDateTimeFuncs);

  for(i=0; i<ArraySize(aDateTimeFuncs); i++){
    sqlite3FuncDefInsert(pHash, &aFunc[i]);
  }
}
