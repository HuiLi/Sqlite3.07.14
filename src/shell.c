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
** This file contains code to implement the "sqlite" command line
** utility for accessing SQLite databases.
*/
#if (defined(_WIN32) || defined(WIN32)) && !defined(_CRT_SECURE_NO_WARNINGS)
/* This needs to come before any includes for MSVC compiler */
#define _CRT_SECURE_NO_WARNINGS
#endif

/*
** Enable large-file support for fopen() and friends on unix.
*/
#ifndef SQLITE_DISABLE_LFS
# define _LARGE_FILE       1
# ifndef _FILE_OFFSET_BITS
# define _FILE_OFFSET_BITS 64
# endif
# define _LARGEFILE_SOURCE 1
#endif
//引入头文件
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "sqlite3.h"
#include <ctype.h>
#include <stdarg.h>

#if !defined(_WIN32) && !defined(WIN32)
# include <signal.h>
# if !defined(__RTP__) && !defined(_WRS_KERNEL)
#  include <pwd.h>
# endif
# include <unistd.h>
# include <sys/types.h>
#endif

#ifdef HAVE_EDITLINE
# include <editline/editline.h>
#endif
#if defined(HAVE_READLINE) && HAVE_READLINE==1
# include <readline/readline.h>
# include <readline/history.h>
#endif
#if !defined(HAVE_EDITLINE) && (!defined(HAVE_READLINE) || HAVE_READLINE!=1)
# define readline(p) local_getline(p,stdin,0)      
# define add_history(X)
# define read_history(X)
# define write_history(X)
# define stifle_history(X)
#endif
//有参数的宏定义
#if defined(_WIN32) || defined(WIN32)
# include <io.h>
#define isatty(h) _isatty(h)
#define access(f,m) _access((f),(m))
#undef popen
#define popen(a,b) _popen((a),(b))
#undef pclose
#define pclose(x) _pclose(x)
#else
/* Make sure isatty() has a prototype.
*/
extern int isatty(int);
#endif

#if defined(_WIN32_WCE)
/* Windows CE (arm-wince-mingw32ce-gcc) does not provide isatty()
 * thus we always assume that we have a console. That can be
 * overridden with the -batch command line option.
 */
#define isatty(x) 1
#endif

/* True if the timer is enabled */
static int enableTimer = 0;  // 初始化了激活时间  

/* ctype macros that work with signed characters */
#define IsSpace(X)  isspace((unsigned char)X)
#define IsDigit(X)  isdigit((unsigned char)X)
#define ToLower(X)  (char)tolower((unsigned char)X)

#if !defined(_WIN32) && !defined(WIN32) && !defined(_WRS_KERNEL)
#include <sys/time.h>
#include <sys/resource.h>

/* Saved resource information for the beginning of an operation */
static struct rusage sBegin;    //定义开始

/*
** Begin timing an operation   
*/
static void beginTimer(void){  //表示开始的时间函数
  if( enableTimer ){
    getrusage(RUSAGE_SELF, &sBegin);
  }
}

/* Return the difference of two time_structs in seconds */
static double timeDiff(struct timeval *pStart, struct timeval *pEnd){  // 有关返回用户时间和系统时间之间的

差异
  return (pEnd->tv_usec - pStart->tv_usec)*0.000001 + 
         (double)(pEnd->tv_sec - pStart->tv_sec);
}

/*
** Print the timing results. 
*/
static void endTimer(void){   //表示打印结果的时间
  if( enableTimer ){
    struct rusage sEnd;   //结束
    getrusage(RUSAGE_SELF, &sEnd);
    printf("CPU Time: user %f sys %f\n",
       timeDiff(&sBegin.ru_utime, &sEnd.ru_utime),
       timeDiff(&sBegin.ru_stime, &sEnd.ru_stime));
  }
}

#define BEGIN_TIMER beginTimer()  //宏定义了开始和结束时间的函数，方便后面使用
#define END_TIMER endTimer()
#define HAS_TIMER 1

#elif (defined(_WIN32) || defined(WIN32))

#include <windows.h>

/* Saved resource information for the beginning of an operation */
static HANDLE hProcess;
static FILETIME ftKernelBegin; //内核开始时间
static FILETIME ftUserBegin;  //用户开始时间
typedef BOOL (WINAPI *GETPROCTIMES)(HANDLE, LPFILETIME, LPFILETIME, LPFILETIME, LPFILETIME);
static GETPROCTIMES getProcessTimesAddr = NULL;  //表示得到进程时间

/*
** Check to see if we have timer support.  Return 1 if necessary
** support found (or found previously).
*/
static int hasTimer(void){    //计时器
  if( getProcessTimesAddr ){   //如果支持返回1
    return 1;
  } else {
    /* GetProcessTimes() isn't supported in WIN95 and some other Windows versions.
    ** See if the version we are running on has it, and if it does, save off
    ** a pointer to it and the current process handle.
    */
    hProcess = GetCurrentProcess();
    if( hProcess ){
      HINSTANCE hinstLib = LoadLibrary(TEXT("Kernel32.dll"));  //加载动态链接库。之后可以访问库内的资源  
                                                               /*kernel32.dll是Windows 9x/Me中 非常重要的32位 动态链接库文件

，属于内核级文件。它控制着系统的内存管理、数据的输入输出操作和中断处理
                                                                ** 当Windows启动时，kernel32.dll就驻留在内存中特定的写保护区域

，使别的程序无法占用这个内存区域。*/  								

			 ** 就驻留在内存中特定的写保护区域，使别的程序无法占用这个内存区域。*/  
       if( NULL != hinstLib ){
        getProcessTimesAddr = (GETPROCTIMES) GetProcAddress(hinstLib, "GetProcessTimes");  //获取动

态连接库里的功能函数地址
        if( NULL != getProcessTimesAddr ){  //如果获取成功，返回1
          return 1;
        }
        FreeLibrary(hinstLib);  //释放动态连接库。
      }
    }
  }
  return 0;
}

/*
** Begin timing an operation
*/
static void beginTimer(void){
  if( enableTimer && getProcessTimesAddr ){
    FILETIME ftCreation, ftExit; //分别定义了表示时间信息的建立和结束的变量
    getProcessTimesAddr(hProcess, &ftCreation, &ftExit, &ftKernelBegin, &ftUserBegin);
  }
}

/* Return the difference of two FILETIME structs in seconds */
static double timeDiff(FILETIME *pStart, FILETIME *pEnd){ // 有关返回开始时间和系统时间之间的差异
  sqlite_int64 i64Start = *((sqlite_int64 *) pStart);  //定义一个新的类型 sqlite_int64
  sqlite_int64 i64End = *((sqlite_int64 *) pEnd);
  return (double) ((i64End - i64Start) / 10000000.0);
}

/*
** Print the timing results.
*/
static void endTimer(void){  //表示打印结果的时间
  if( enableTimer && getProcessTimesAddr){  //激活成功并且得到函数地址
    FILETIME ftCreation, ftExit, ftKernelEnd, ftUserEnd;
    getProcessTimesAddr(hProcess, &ftCreation, &ftExit, &ftKernelEnd, &ftUserEnd);
    printf("CPU Time: user %f sys %f\n",
       timeDiff(&ftUserBegin, &ftUserEnd),
       timeDiff(&ftKernelBegin, &ftKernelEnd));
  }
}

#define BEGIN_TIMER beginTimer()
#define END_TIMER endTimer()
#define HAS_TIMER hasTimer()  //定义哈希时间

#else
#define BEGIN_TIMER 
#define END_TIMER
#define HAS_TIMER 0
#endif

/*
** Used to prevent warnings about unused parameters
*/
#define UNUSED_PARAMETER(x) (void)(x)  //为了防止不使用的参数的警告

/*
** If the following flag is set, then command execution stops
** at an error if we are not interactive.
*/
static int bail_on_error = 0;//设置下面的标记,如果我们没有交互命令执行就会因为一个错误而停止

/*
** Threat stdin as an interactive input if the following variable
** is true.  Otherwise, assume stdin is connected to a file or pipe.
*/
static int stdin_is_interactive = 1; //如果这个变量是true，进行交互式输入，否则，假设交互式输入是连接到文件

或者管道的。

/*
** The following is the open SQLite database.  We make a pointer
** to this database a static variable so that it can be accessed
** by the SIGINT handler to interrupt database processing.
*/
static sqlite3 *db = 0; //表示打开的数据库，定义一个静态的指针变量，我们就能够通过中断信号控制来中断数据库

操作

/*
** True if an interrupt (Control-C) has been received.
*/
static volatile int seenInterrupt = 0;   //用来检测中断的变量，如果收到中断信号，就将变量赋值为 1

/*
** This is the name of our program. It is set in main(), used
** in a number of other places, mostly for error messages.
*/
static char *Argv0;  //被使用在main（）函数和很多其他场合，表示程序的名字，下面程序中更多被使用在错误信

息里。如：fprintf(stderr,"%s: Error: no database filename specified\n", Argv0);

/*
** Prompt strings. Initialized in main. Settable with
**   .prompt main continue 
*/
//提示字符串，在main函数中初始化，用.prompt main continue 设定
static char mainPrompt[20];     /* First line prompt. default: "sqlite> "*/
static char continuePrompt[20]; /* Continuation prompt. default: "   ...> " */

/*
** Write I/O traces to the following stream.
*/
#ifdef SQLITE_ENABLE_IOTRACE
static FILE *iotrace = 0;  //表示用于输入输出的流
#endif

/*
** This routine works like printf in that its first argument is a
** format string and subsequent arguments are values to be substituted
** in place of % fields.  The result of formatting this string
** is written to iotrace.
*/ //输出时，第一个内容是一个格式字符串，后面的内容是%+字段的格式。这个结果是来表示输入输出流的
#ifdef SQLITE_ENABLE_IOTRACE
static void iotracePrintf(const char *zFormat, ...){ //有一个参数zFormat固定以外,后面跟的参数的个数和类型

是可变的（用三个点“…”做参数占位符）
  va_list ap;  //这个变量是存储参数地址的指针.因为得到参数的地址之后，再结合参数的类型，才能得到参数的值。
  char *z;
  if( iotrace==0 ) return;  //没有输入输出操作，返回
  va_start(ap, zFormat); //以固定参数的地址为起点确定变参的内存起始地址
  z = sqlite3_vmprintf(zFormat, ap);//函数返回的字符串被写入通过 malloc() 得到的内存空间，因此，永远不会

存在内存泄露的问题。返回的字符串要用sqlite3_free()释放空间。
  va_end(ap); //结束
  fprintf(iotrace, "%s", z); // 格式化输出 fprintf(文件指针,格式字符串,输出表列)
  sqlite3_free(z);  //释放空间
}
#endif


/*
** Determines if a string is a number of not.  //如果有很多非数字则终止,z为得到的字符串
*/
static int isNumber(const char *z, int *realnum){
  if( *z=='-' || *z=='+' ) z++;  //判断正负
  if( !IsDigit(*z) ){ //判断是否是数字，如果不是，返回0
    return 0;
  }
  z++;       //指针后移一位
  if( realnum ) *realnum = 0; //  字符串的实际长度
  while( IsDigit(*z) ){ z++; } //如果遇到数字，指针后移一位
  if( *z=='.' ){   //判断是否是小数
    z++;
    if( !IsDigit(*z) ) return 0; //如果遇到非数字，返回0
    while( IsDigit(*z) ){ z++; }
    if( realnum ) *realnum = 1; //
  }
  if( *z=='e' || *z=='E' ){ // 判断是否有指数
    z++;
    if( *z=='+' || *z=='-' ) z++; //指数的正负
    if( !IsDigit(*z) ) return 0;
    while( IsDigit(*z) ){ z++; }
    if( realnum ) *realnum = 1;
  }
  return *z==0;
}

/*
** A global char* and an SQL function to access its current value 
** from within an SQL statement. This program used to use the 
** sqlite_exec_printf() API to substitue a string into an SQL statement.
** The correct way to do this with sqlite3 is to use the bind API, but
** since the shell is built around the callback paradigm it would be a lot
** of work. Instead just use this hack, which is quite harmless.
*/    //一个全局的char指针变量和一个SQL函数从一个SQL语句中访问它当前的值。这个程序之前使用

sqlite_exec_printf() AP代替一个字符串成为SQL语句，sqlite3的正确的方法是使用bind API,但当shell建立在回调

模式,将可以完成大量的工作
static const char *zShellStatic = 0;
static void shellstaticFunc(   //
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  assert( 0==argc );
  assert( zShellStatic );
  UNUSED_PARAMETER(argc);  //不使用的参数，根据前面的定义设置为空
  UNUSED_PARAMETER(argv);
  sqlite3_result_text(context, zShellStatic, -1, SQLITE_STATIC); //
}


/*
** This routine reads a line of text from FILE in, stores
** the text in memory obtained from malloc() and returns a pointer
** to the text.  NULL is returned at end of file, or if malloc()
** fails.
**
** The interface is like "readline" but no command-line editing  //readline方法描述从一个textstream文件

读取一整行并返回得到的字符串，这个接口是像readline一样，而不是命令行编辑
** is done.
*/  //从文件的文本中读取一行，将文本存储到从malloc（）中得到的内存空间，并且返回一个指针，如果失败，在文

件结束返回NULL,或者如果malloc()失败。
// 在需要一次读入一整行很长的内容时可以使用此方法
static char *local_getline(char *zPrompt, FILE *in, int csvFlag){  //从文件中读取行的函数定义:*zPrompt表示

读取的字符串*in表示打开文件的指针，csvFlag读取的长度
  char *zLine;  //读取行
  int nLine;  //指定长度
  int n; 
  int inQuote = 0;

  if( zPrompt && *zPrompt ){// 读取成功，则输出字符串
    printf("%s",zPrompt);
    fflush(stdout);
  }
  nLine = 100;  //赋值指定长度
  zLine = malloc( nLine ); //分配大小为nLine的内存空间
  if( zLine==0 ) return 0;  //如果字符串为空，则返回0
  n = 0;
  while( 1 ){  //设定一个一般字符串的长度限制为缓冲区的大小, 每次读取后, 再判断下是否到达行末, 如果没有到达, 

再利用上面的方法动态分配缓冲区
    if( n+100>nLine ){  
      nLine = nLine*2 + 100;
      zLine = realloc(zLine, nLine); //将zLine对象的存储空间改为nLine大小
      if( zLine==0 ) return 0;
    }
    if( fgets(&zLine[n], nLine - n, in)==0 ){
      if( n==0 ){
        free(zLine);
        return 0;
      }
      zLine[n] = 0;
      break;
    }
    while( zLine[n] ){
      if( zLine[n]=='"' ) inQuote = !inQuote;
      n++;
    }
    if( n>0 && zLine[n-1]=='\n' && (!inQuote || !csvFlag) ){
      n--;
      if( n>0 && zLine[n-1]=='\r' ) n--;
      zLine[n] = 0;
      break;
    }
  }
  zLine = realloc( zLine, n+1 );
  return zLine;
}

/*
** Retrieve a single line of input text.
**
** zPrior is a string of prior text retrieved.  If not the empty
** string, then issue a continuation prompt.
*/
static char *one_input_line(const char *zPrior, FILE *in){
  char *zPrompt;
  char *zResult;
  if( in!=0 ){
    return local_getline(0, in, 0);
  }
  if( zPrior && zPrior[0] ){
    zPrompt = continuePrompt;
  }else{
    zPrompt = mainPrompt;
  }
  zResult = readline(zPrompt);
#if defined(HAVE_READLINE) && HAVE_READLINE==1
  if( zResult && *zResult ) add_history(zResult);
#endif
  return zResult;
}

struct previous_mode_data {  // 定义了结构体，该结构体的作用是在.explain命令执行之前的模式信息
  int valid;        /* Is there legit data in here? */
  int mode;                       //输出模式
  int showHeader;                //显示列名
  int colWidth[100];             //所需列宽
};

/*
** An pointer to an instance of this structure is passed from
** the main program to the callback.  This is used to communicate
** state and mode information.
*/
struct callback_data {  //定义结构体，用来进行各方法之间的传值与当前状态的获取；如回调，传达声明和模式信息

。
  sqlite3 *db;           //表示要打开的数据库                                 /* The database */ 
  int echoOn;                                                                 /* True to echo input commands */
  int statsOn;          //在每次结束之前显示存储器的统计数据                  /* True to display memory stats 

before each finalize */
  int cnt;              //已经显示的记录数                                    /* Number of records displayed so far */
  FILE *out;            //表示用于输出的文件流                                /* Write results here */
  FILE *traceOut;                                                             /* Output for sqlite3_trace() */
  int nErr;               //表示返回的错误                                    /* Number of errors seen */
  int mode;               //输出模式                                           /* An output mode setting */
  int writableSchema;                                                         /* True if PRAGMA writable_schema=ON */
  int showHeader;          //在列表或者列模式下显示列的名字                    /* True to show column names in 

List or Column mode */
  char *zDestTable;       //在insert显示模式下，存储表的名称，方便构建sql语句  /* Name of destination table 

when MODE_Insert */
  char separator[20];                                                         /* Separator character for MODE_List */
  int colWidth[100];      //在列模式下的所需列宽                               /* Requested width of each column 

when in column mode*/
  int actualWidth[100];    //列的实际宽度                                      /* Actual width of each column */
  char nullvalue[20];     //代替从数据库中返回的记录中空的选项，这个可以通过.nullvalue命令来设置
                                        /* The text to print when a NULL comes back from
                                       ** the database */
  struct previous_mode_data explainPrev;    
                                       /* Holds the mode information just before
                                       ** .explain ON */
  char outfile[FILENAME_MAX];                                                  /* Filename for *out */
  const char *zDbFilename;    //存放数据库文件的名字                           /* name of the database file */
  const char *zVfs;                                                            /* Name of VFS to use */
  sqlite3_stmt *pStmt;      //存放当前的statement句柄                          /* Current statement if any. */
  FILE *pLog;                 //表示用于输出的日志文件流                        /* Write log output here */
};

/*
** These are the allowed modes.
*/
#define MODE_Line     0  /* One column per line.  Blank line between records */
#define MODE_Column   1  /* One record per line in neat columns */
#define MODE_List     2  /* One record per line with a separator */
#define MODE_Semi     3  /* Same as MODE_List but append ";" to each line */
#define MODE_Html     4  /* Generate an XHTML table */
#define MODE_Insert   5  /* Generate SQL "insert" statements */
#define MODE_Tcl      6  /* Generate ANSI-C or TCL quoted elements */
#define MODE_Csv      7  /* Quote strings, numbers are plain */
#define MODE_Explain  8  /* Like MODE_Column, but do not truncate data */

static const char *modeDescr[] = { //定义允许的模式字符数组；数据显示格式；有好几种显示模式，默认的是 

list 显示模式，一般我们使用 column 显示模式
  "line",    //每行一个值
  "column",   //以整齐的列显示每一行数据
  "list",    //分隔符分隔的字符
  "semi",    //和list模式类似，但是每一行会以“；”结束
  "html",     //以html代码方式显示
  "insert",  //显示insert sql语句
  "tcl",    //TCL列表元素
  "csv",     //逗号分隔值
  "explain",  //和column类似，但不截断数据
};

/*
** Number of elements in an array   //数组中元素的数量
*/
#define ArraySize(X)  (int)(sizeof(X)/sizeof(X[0]))

/*
** Compute a string length that is limited to what can be stored in
** lower 30 bits of a 32-bit signed integer.
*/
static int strlen30(const char *z){     //能够存储的最大bit数;字符串长度是有限的,可以存储在低30位的32位带符

号整数
  const char *z2 = z;
  while( *z2 ){ z2++; }
  return 0x3fffffff & (int)(z2 - z);
}

/*
** A callback for the sqlite3_log() interface.    //sqlite3_log()接口的回调
*/
static void shellLog(void *pArg, int iErrCode, const char *zMsg){  //生产shell下运行的日志
  struct callback_data *p = (struct callback_data*)pArg;  
  if( p->pLog==0 ) return;   //如果没有日志返回
  fprintf(p->pLog, "(%d) %s\n", iErrCode, zMsg); //输出日志信息
  fflush(p->pLog); //清空缓存
}

/*
** Output the given string as a hex-encoded blob (eg. X'1234' )
*/
static void output_hex_blob(FILE *out, const void *pBlob, int nBlob){//将字符串以hex二进制编码的方式输

出
  int i;
  char *zBlob = (char *)pBlob;
  fprintf(out,"X'");
  for(i=0; i<nBlob; i++){ fprintf(out,"%02x",zBlob[i]&0xff); }
  fprintf(out,"'");
}

/*
** Output the given string as a quoted string using SQL quoting conventions.
*/
static void output_quoted_string(FILE *out, const char *z){//将字符串以引证字符串的形式输出
  int i;
  int nSingle = 0;
  for(i=0; z[i]; i++){
    if( z[i]=='\'' ) nSingle++;
  }
  if( nSingle==0 ){ //
    fprintf(out,"'%s'",z);
  }else{
    fprintf(out,"'");
    while( *z ){
      for(i=0; z[i] && z[i]!='\''; i++){}
      if( i==0 ){
        fprintf(out,"''");
        z++;
      }else if( z[i]=='\'' ){
        fprintf(out,"%.*s''",i,z);
        z += i+1;
      }else{
        fprintf(out,"%s",z);
        break;
      }
    }
    fprintf(out,"'");
  }
}

/*
** Output the given string as a quoted according to C or TCL quoting rules.
*/
static void output_c_string(FILE *out, const char *z){  //根据C或TCL引用规则输出字符串
  unsigned int c;
  fputc('"', out);
  while( (c = *(z++))!=0 ){
    if( c=='\\' ){
      fputc(c, out);
      fputc(c, out);
    }else if( c=='\t' ){
      fputc('\\', out);
      fputc('t', out);
    }else if( c=='\n' ){
      fputc('\\', out);
      fputc('n', out);
    }else if( c=='\r' ){
      fputc('\\', out);
      fputc('r', out);
    }else if( !isprint(c) ){
      fprintf(out, "\\%03o", c&0xff);
    }else{
      fputc(c, out);
    }
  }
  fputc('"', out);
}

/*
** Output the given string with characters that are special to
** HTML escaped. 
*/
static void output_html_string(FILE *out, const char *z){//以特殊的HTML代码方式显示字符串
  int i;
  while( *z ){
    for(i=0;   z[i] 
            && z[i]!='<' 
            && z[i]!='&' 
            && z[i]!='>' 
            && z[i]!='\"' 
            && z[i]!='\'';
        i++){}
    if( i>0 ){
      fprintf(out,"%.*s",i,z);
    }
    if( z[i]=='<' ){
      fprintf(out,"&lt;");
    }else if( z[i]=='&' ){
      fprintf(out,"&amp;");
    }else if( z[i]=='>' ){
      fprintf(out,"&gt;");
    }else if( z[i]=='\"' ){
      fprintf(out,"&quot;");
    }else if( z[i]=='\'' ){
      fprintf(out,"&#39;");
    }else{
      break;
    }
    z += i + 1;
  }
}

/*
** If a field contains any character identified by a 1 in the following
** array, then the string must be quoted for CSV.  // 如果一个域包含任何被下面数组的定义的字符，这个字符

串必须被引证为CSV
*/
static const char needCsvQuote[] = {
  1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,   
  1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,   
  1, 0, 1, 0, 0, 0, 0, 1,   0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 1, 
  1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,   
  1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,   
  1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,   
  1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,   
  1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,   
  1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,   
  1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,   
  1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,   
};

/*
** Output a single term of CSV.  Actually, p->separator is used for
** the separator, which may or may not be a comma.  p->nullvalue is
** the null value.  Strings are quoted if necessary.
*/  //
static void output_csv(struct callback_data *p, const char *z, int bSep){//以csv格式输出字符串，其中p-

>separator被用作表示分隔符，p->nullvalue表示NUll值，字符串只有在必要的时候被引用
  FILE *out = p->out;
  if( z==0 ){
    fprintf(out,"%s",p->nullvalue);  //格式化输出 fprintf(文件指针,格式字符串,输出表列)
  }else{
    int i;
    int nSep = strlen30(p->separator);
    for(i=0; z[i]; i++){
      if( needCsvQuote[((unsigned char*)z)[i]] 
         || (z[i]==p->separator[0] && 
             (nSep==1 || memcmp(z, p->separator, nSep)==0)) ){
        i = 0;
        break;
      }
    }
    if( i==0 ){
      putc('"', out);
      for(i=0; z[i]; i++){
        if( z[i]=='"' ) putc('"', out);
        putc(z[i], out);
      }
      putc('"', out);
    }else{
      fprintf(out, "%s", z);
    }
  }
  if( bSep ){
    fprintf(p->out, "%s", p->separator);
  }
}

#ifdef SIGINT
/*
** This routine runs when the user presses Ctrl-C 
*/
//seenInterrupt是用来检测中断的变量，前面定义初值为0，如果收到中断信号，就将变量赋值为 1
static void interrupt_handler(int NotUsed){ //中断控制函数，当操作为Ctrl-C的时候调用
  UNUSED_PARAMETER(NotUsed);   //表示不使用的参数
  seenInterrupt = 1;   //指示中断信号的变量，此时表示收到中断信号。
  if( db ) sqlite3_interrupt(db);  //如果数据库被打开，则中断它
}
#endif

/*
** This is the callback routine that the shell
** invokes for each row of a query result.
*/
static int shell_callback(void *pArg, int nArg, char **azArg, char **azCol, int *aiType){ //解释器回调查询结

果的每一行
  int i;
  struct callback_data *p = (struct callback_data*)pArg; //定义一个callback_data的对象

  switch( p->mode ){  //判断调用的模式，根据调用的模式不同，选择不同的方式输出结果
    case MODE_Line: {  //Line模式
      int w = 5;
      if( azArg==0 ) break;
      for(i=0; i<nArg; i++){
        int len = strlen30(azCol[i] ? azCol[i] : "");
        if( len>w ) w = len;
      }
      if( p->cnt++>0 ) fprintf(p->out,"\n");
      for(i=0; i<nArg; i++){
        fprintf(p->out,"%*s = %s\n", w, azCol[i],
                azArg[i] ? azArg[i] : p->nullvalue);  //p->nullvalue表示NUll值
      }
      break;
    }
    case MODE_Explain:
    case MODE_Column: {  //Explain和Column模式
      if( p->cnt++==0 ){
        for(i=0; i<nArg; i++){
          int w, n;
          if( i<ArraySize(p->colWidth) ){
            w = p->colWidth[i];
          }else{
            w = 0;
          }
          if( w<=0 ){
            w = strlen30(azCol[i] ? azCol[i] : "");
            if( w<10 ) w = 10;
            n = strlen30(azArg && azArg[i] ? azArg[i] : p->nullvalue);
            if( w<n ) w = n;
          }
          if( i<ArraySize(p->actualWidth) ){
            p->actualWidth[i] = w;
          }
          if( p->showHeader ){
            fprintf(p->out,"%-*.*s%s",w,w,azCol[i], i==nArg-1 ? "\n": "  ");
          }
        }
        if( p->showHeader ){
          for(i=0; i<nArg; i++){
            int w;
            if( i<ArraySize(p->actualWidth) ){
               w = p->actualWidth[i];
            }else{
               w = 10;
            }
            fprintf(p->out,"%-*.*s%s",w,w,"-----------------------------------"
                   "----------------------------------------------------------",
                    i==nArg-1 ? "\n": "  ");
          }
        }
      }
      if( azArg==0 ) break;
      for(i=0; i<nArg; i++){
        int w;
        if( i<ArraySize(p->actualWidth) ){
           w = p->actualWidth[i];
        }else{
           w = 10;
        }
        if( p->mode==MODE_Explain && azArg[i] && 
           strlen30(azArg[i])>w ){
          w = strlen30(azArg[i]);
        }
        fprintf(p->out,"%-*.*s%s",w,w,
            azArg[i] ? azArg[i] : p->nullvalue, i==nArg-1 ? "\n": "  ");
      }
      break;
    }
    case MODE_Semi:
    case MODE_List: { //Semi和List模式
      if( p->cnt++==0 && p->showHeader ){
        for(i=0; i<nArg; i++){
          fprintf(p->out,"%s%s",azCol[i], i==nArg-1 ? "\n" : p->separator);
        }
      }
      if( azArg==0 ) break;
      for(i=0; i<nArg; i++){
        char *z = azArg[i];
        if( z==0 ) z = p->nullvalue;
        fprintf(p->out, "%s", z);
        if( i<nArg-1 ){
          fprintf(p->out, "%s", p->separator);
        }else if( p->mode==MODE_Semi ){
          fprintf(p->out, ";\n");
        }else{
          fprintf(p->out, "\n");
        }
      }
      break;
    }
    case MODE_Html: {  //Html模式
      if( p->cnt++==0 && p->showHeader ){
        fprintf(p->out,"<TR>");
        for(i=0; i<nArg; i++){
          fprintf(p->out,"<TH>");
          output_html_string(p->out, azCol[i]);
          fprintf(p->out,"</TH>\n");
        }
        fprintf(p->out,"</TR>\n");
      }
      if( azArg==0 ) break;
      fprintf(p->out,"<TR>");
      for(i=0; i<nArg; i++){
        fprintf(p->out,"<TD>");
        output_html_string(p->out, azArg[i] ? azArg[i] : p->nullvalue);
        fprintf(p->out,"</TD>\n");
      }
      fprintf(p->out,"</TR>\n");
      break;
    }
    case MODE_Tcl: { // Tcl模式
      if( p->cnt++==0 && p->showHeader ){
        for(i=0; i<nArg; i++){
          output_c_string(p->out,azCol[i] ? azCol[i] : "");
          fprintf(p->out, "%s", p->separator);
        }
        fprintf(p->out,"\n");
      }
      if( azArg==0 ) break;
      for(i=0; i<nArg; i++){
        output_c_string(p->out, azArg[i] ? azArg[i] : p->nullvalue);
        fprintf(p->out, "%s", p->separator);
      }
      fprintf(p->out,"\n");
      break;
    }
    case MODE_Csv: { //Csv模式
      if( p->cnt++==0 && p->showHeader ){
        for(i=0; i<nArg; i++){
          output_csv(p, azCol[i] ? azCol[i] : "", i<nArg-1);
        }
        fprintf(p->out,"\n");
      }
      if( azArg==0 ) break;
      for(i=0; i<nArg; i++){
        output_csv(p, azArg[i], i<nArg-1);
      }
      fprintf(p->out,"\n");
      break;
    }
    case MODE_Insert: {  //Insert模式
      p->cnt++;
      if( azArg==0 ) break;
      fprintf(p->out,"INSERT INTO %s VALUES(",p->zDestTable);//指目的表
      for(i=0; i<nArg; i++){
        char *zSep = i>0 ? ",": "";
        if( (azArg[i]==0) || (aiType && aiType[i]==SQLITE_NULL) ){
          fprintf(p->out,"%sNULL",zSep);
        }else if( aiType && aiType[i]==SQLITE_TEXT ){
          if( zSep[0] ) fprintf(p->out,"%s",zSep);
          output_quoted_string(p->out, azArg[i]);
        }else if( aiType && (aiType[i]==SQLITE_INTEGER || aiType[i]==SQLITE_FLOAT) ){
          fprintf(p->out,"%s%s",zSep, azArg[i]);
        }else if( aiType && aiType[i]==SQLITE_BLOB && p->pStmt ){
          const void *pBlob = sqlite3_column_blob(p->pStmt, i);
          int nBlob = sqlite3_column_bytes(p->pStmt, i);
          if( zSep[0] ) fprintf(p->out,"%s",zSep);
          output_hex_blob(p->out, pBlob, nBlob);
        }else if( isNumber(azArg[i], 0) ){
          fprintf(p->out,"%s%s",zSep, azArg[i]);
        }else{
          if( zSep[0] ) fprintf(p->out,"%s",zSep);
          output_quoted_string(p->out, azArg[i]);
        }
      }
      fprintf(p->out,");\n");
      break;
    }
  }
  return 0;
}

/*
** This is the callback routine that the SQLite library
** invokes for each row of a query result.
*/  
static int callback(void *pArg, int nArg, char **azArg, char **azCol){   //定义SQLite库调用查询结果的每一行

的回调程序
  /* since we don't have type info, call the shell_callback with a NULL value */
  return shell_callback(pArg, nArg, azArg, azCol, NULL);  //当没有类型信息,使用Null值调用shell_callback 
}

/*
** Set the destination table field of the callback_data structure to
** the name of the table given.  Escape any quote characters in the
** table name.
*/
static void set_table_name(struct callback_data *p, const char *zName){ //设定的目标表字段callback_data

结构的表的名称。任何引用字符转义的表名。zName表示表名
  int i, n;
  int needQuote;
  char *z;

  if( p->zDestTable ){   //p->zDestTable指目的表
    free(p->zDestTable);  //释放空间
    p->zDestTable = 0;
  }
  if( zName==0 ) return;
  needQuote = !isalpha((unsigned char)*zName) && *zName!='_';
  for(i=n=0; zName[i]; i++, n++){
    if( !isalnum((unsigned char)zName[i]) && zName[i]!='_' ){
      needQuote = 1;
      if( zName[i]=='\'' ) n++;
    }
  }
  if( needQuote ) n += 2;
  z = p->zDestTable = malloc( n+1 );
  if( z==0 ){
    fprintf(stderr,"Error: out of memory\n");
    exit(1);
  }
  n = 0;
  if( needQuote ) z[n++] = '\'';
  for(i=0; zName[i]; i++){
    z[n++] = zName[i];
    if( zName[i]=='\'' ) z[n++] = '\'';
  }
  if( needQuote ) z[n++] = '\'';
  z[n] = 0;
}

/* zIn is either a pointer to a NULL-terminated string in memory obtained
** from malloc(), or a NULL pointer. The string pointed to by zAppend is
** added to zIn, and the result returned in memory obtained from malloc().
** zIn, if it was not NULL, is freed.
**
** If the third argument, quote, is not '\0', then it is used as a 
** quote character for zAppend.
*/    //如果zInt不是null，则释放空间
static char *appendText(char *zIn, char const *zAppend, char quote){//zInt是在malloc()中获得的内存中以NUll字符串结尾的字符串指针或表示NUll指针；zAppend指向的字符串是加到zInt上的，返回的结果来自malloc()；如果第三个参数不是'\0',那么用作zAppend引用字符
  int len;//定义长度
  int i;
  int nAppend = strlen30(zAppend);
  int nIn = (zIn?strlen30(zIn):0);

  len = nAppend+nIn+1;
  if( quote ){//如果quote不是'\0',那么用作zAppend引用字符
    len += 2;
    for(i=0; i<nAppend; i++){
      if( zAppend[i]==quote ) len++;
    }
  }

  zIn = (char *)realloc(zIn, len);//重新分配内存
  if( !zIn ){
    return 0;
  }

  if( quote ){//如果quote不是'\0'
    char *zCsr = &zIn[nIn];
    *zCsr++ = quote;
    for(i=0; i<nAppend; i++){
      *zCsr++ = zAppend[i];
      if( zAppend[i]==quote ) *zCsr++ = quote; //如果zAppend指向的字符串和quote相等
    }
    *zCsr++ = quote;
    *zCsr++ = '\0';
    assert( (zCsr-zIn)==len );
  }else{
    memcpy(&zIn[nIn], zAppend, nAppend);//字符串拷贝
    zIn[len-1] = '\0';
  }

  return zIn;
}


/*
** Execute a query statement that will generate SQL output.  Print
** the result columns, comma-separated, on a line and then add a
** semicolon terminator to the end of that line.
**  //执行一个生成的SQL输出的查询语句。打印结果列,逗号分隔线,以一个分号终结这行
** If the number of columns is 1 and that column contains text "--"
** then write the semicolon on a separate line.  That way, if a 
** "--" comment occurs at the end of the statement, the comment
** won't consume the semicolon terminator.
*/  //如果列的数量是1并且列包含文本”——“，然后输出分号在单独的行中。这样,如果一个”——“出现在声明的

最后,则不使用分号
static int run_table_dump_query(//使用.dump命令可以将数据库对象导出成SQL格式
  struct callback_data *p, //要查询的内容    /* Query context */  
  const char *zSelect,     //抽取选择语句的内容  /* SELECT statement to extract content */
  const char *zFirstRow    //如果不为NUll，则在第一行之前打印   /* Print before first row, if not NULL */
){
  sqlite3_stmt *pSelect;  //把一个sql语句解析到pSelect中，即存放当前的statement句柄 
  int rc;     //定义返回值
  int nResult;
  int i;
  const char *z;
  rc = sqlite3_prepare(p->db, zSelect, -1, &pSelect, 0); //函数完成 sql 语句的解析。第一个参数跟前面一样，

是个 sqlite3 * 类型变量，第二个参数是一个 sql 语句。第三个参数我写的是-1，这个参数含义是前面 sql 语句的长度

。如果小于0，sqlite会自动计算它的长度（把sql语句当成以\0结尾的字符串）。第四个参数是 sqlite3_stmt 的指针

的指针。解析以后的sql语句就放在这个结构里。
  if( rc!=SQLITE_OK || !pSelect ){ //如果返回值不是SQLITE_OK或者没有得到当前语句
    fprintf(p->out, "/**** ERROR: (%d) %s *****/\n", rc, sqlite3_errmsg(p->db)); //则输出错误信息
    p->nErr++;  //nErr表示返回的错误信息
    return rc;
  }
  rc = sqlite3_step(pSelect); //通过这个语句，pSelect 表示的sql语句就被写到了数据库里。最后，要把 

sqlite3_stmt 结构给释放，函数的返回值基于创建sqlite3_stmt参数所使用的函数
  nResult = sqlite3_column_count(pSelect); //分配空间
  while( rc==SQLITE_ROW ){ //返回值为SQLITE_ROW
    if( zFirstRow ){ //如果不为NUll，则打印第一行
      fprintf(p->out, "%s", zFirstRow);
      zFirstRow = 0;
    }
    z = (const char*)sqlite3_column_text(pSelect, 0);//提取数据
    fprintf(p->out, "%s", z);
    for(i=1; i<nResult; i++){ 
      fprintf(p->out, ",%s", sqlite3_column_text(pSelect, i));//循环输出数据
    }
    if( z==0 ) z = "";
    while( z[0] && (z[0]!='-' || z[1]!='-') ) z++; //如果列的数量是1并且列包含文本”——“，然后输出分号在单

独的行中
    if( z[0] ){
      fprintf(p->out, "\n;\n");
    }else{
      fprintf(p->out, ";\n");
    }    
    rc = sqlite3_step(pSelect); //把sql语句写到数据库里
  }
  rc = sqlite3_finalize(pSelect);//把刚才分配的内容析构掉，这个过程销毁前面被sqlite3_prepare创建的准备语句

，每个准备语句都必须使用这个函数去销毁以防止内存泄露。
  if( rc!=SQLITE_OK ){ //如果返回值不是SQLITE_OK，则返回错误信息
    fprintf(p->out, "/**** ERROR: (%d) %s *****/\n", rc, sqlite3_errmsg(p->db));
    p->nErr++;
  }
  return rc;  //函数的返回值
}

/*
** Allocate space and save off current error string. //分配空间，保存消除当前错误的字符串
*/
static char *save_err_msg(  //保存错误信息
  sqlite3 *db               //要访问的数据库 /* Database to query */
){
  int nErrMsg = 1+strlen30(sqlite3_errmsg(db));
  char *zErrMsg = sqlite3_malloc(nErrMsg);//通过sqlite3_malloc()接口，SQLite扩展或应用程序本身都可以使

用相同的SQLite的底层分配函数来使用内存
  if( zErrMsg ){
    memcpy(zErrMsg, sqlite3_errmsg(db), nErrMsg);//更新错误信息
  }
  return zErrMsg;
}

/*
** Display memory stats.   //显示内存统计数据
*/
static int display_stats(  //显示统计数据
  sqlite3 *db,                // 要访问的数据库 /* Database to query */
  struct callback_data *pArg, //定义一个回调函数的指针  /* Pointer to struct callback_data */
  int bReset                 //对重置操作进行判断 /* True to reset the stats */
){
  int iCur;  //定义两个指示变量，存储当前的选择值
  int iHiwtr; //存储历史最高值

  if( pArg && pArg->out ){  //pArg->out指向用于输出的文件流
    
    iHiwtr = iCur = -1;//赋值为-1
    sqlite3_status(SQLITE_STATUS_MEMORY_USED, &iCur, &iHiwtr, 

bReset);//SQLITE_STATUS_MEMORY_USED确认当前访问的统计信息，当前选择的值会写入到iCur整型参数，历史

最高值会写入到iHiwtr参数中。如果bReset为true，则在调用返回时iHiwtr标志会重置为当前选择的值。
    fprintf(pArg->out, "Memory Used:                         %d (max %d) bytes\n", iCur, iHiwtr);
    iHiwtr = iCur = -1;
    sqlite3_status(SQLITE_STATUS_MALLOC_COUNT, &iCur, &iHiwtr, bReset);//当前的内存分配信息
    fprintf(pArg->out, "Number of Outstanding Allocations:   %d (max %d)\n", iCur, iHiwtr);
/*
** Not currently used by the CLI.  // 没有使用命令行界面
**    iHiwtr = iCur = -1;
**    sqlite3_status(SQLITE_STATUS_PAGECACHE_USED, &iCur, &iHiwtr, bReset);//页面缓存使用信息
**    fprintf(pArg->out, "Number of Pcache Pages Used:         %d (max %d) pages\n", iCur, iHiwtr);//使

用的寄存器页面的数量
*/
    iHiwtr = iCur = -1;
    sqlite3_status(SQLITE_STATUS_PAGECACHE_OVERFLOW, &iCur, &iHiwtr, bReset);//页面缓存溢出信息
    fprintf(pArg->out, "Number of Pcache Overflow Bytes:     %d (max %d) bytes\n", iCur, iHiwtr);
/*
** Not currently used by the CLI.
**    iHiwtr = iCur = -1;
**    sqlite3_status(SQLITE_STATUS_SCRATCH_USED, &iCur, &iHiwtr, bReset); //记录信息
**    fprintf(pArg->out, "Number of Scratch Allocations Used:  %d (max %d)\n", iCur, iHiwtr);
*/
    iHiwtr = iCur = -1;
    sqlite3_status(SQLITE_STATUS_SCRATCH_OVERFLOW, &iCur, &iHiwtr, bReset);//记录信息溢出
    fprintf(pArg->out, "Number of Scratch Overflow Byt es:    %d (max %d) bytes\n", iCur, iHiwtr);
    iHiwtr = iCur = -1;
    sqlite3_status(SQLITE_STATUS_MALLOC_SIZE, &iCur, &iHiwtr, bReset);//分配的内存大小
	.
    fprintf(pArg->out, "Largest Allocation:                  %d bytes\n", iHiwtr);
    iHiwtr = iCur = -1;
    sqlite3_status(SQLITE_STATUS_PAGECACHE_SIZE, &iCur, &iHiwtr, bReset);//页面缓存的大小信息
    fprintf(pArg->out, "Largest Pcache Allocation:           %d bytes\n", iHiwtr);
    iHiwtr = iCur = -1;
    sqlite3_status(SQLITE_STATUS_SCRATCH_SIZE, &iCur, &iHiwtr, bReset);//记录信息的大小
    fprintf(pArg->out, "Largest Scratch Allocation:          %d bytes\n", iHiwtr);
#ifdef YYTRACKMAXSTACKDEPTH
    iHiwtr = iCur = -1;
    sqlite3_status(SQLITE_STATUS_PARSER_STACK, &iCur, &iHiwtr, bReset); //解析器堆栈
    fprintf(pArg->out, "Deepest Parser Stack:                %d (max %d)\n", iCur, iHiwtr);
#endif
  }
//对于单个数据库连接的统计
  if( pArg && pArg->out && db ){//如果得到输出文件流和数据库连接成功
    iHiwtr = iCur = -1;//赋值-1
    sqlite3_db_status(db, SQLITE_DBSTATUS_LOOKASIDE_USED, &iCur, &iHiwtr, 

bReset);//sqlite3_db_status()多一个数据库连接参数，并且返回的是这个连接的内存统计信息，而不是整个SQLite

库
    fprintf(pArg->out, "Lookaside Slots Used:                %d (max %d)\n", iCur, iHiwtr);
    sqlite3_db_status(db, SQLITE_DBSTATUS_LOOKASIDE_HIT, &iCur, &iHiwtr, bReset);//后备命中
    fprintf(pArg->out, "Successful lookaside attempts:       %d\n", iHiwtr);
    sqlite3_db_status(db, SQLITE_DBSTATUS_LOOKASIDE_MISS_SIZE, &iCur, &iHiwtr, bReset);//后备缺失

大小
    fprintf(pArg->out, "Lookaside failures due to size:      %d\n", iHiwtr);
    sqlite3_db_status(db, SQLITE_DBSTATUS_LOOKASIDE_MISS_FULL, &iCur, &iHiwtr, bReset);//后备失败
    fprintf(pArg->out, "Lookaside failures due to OOM:       %d\n", iHiwtr);
    iHiwtr = iCur = -1;
    sqlite3_db_status(db, SQLITE_DBSTATUS_CACHE_USED, &iCur, &iHiwtr, bReset);//页面堆使用
    fprintf(pArg->out, "Pager Heap Usage:                    %d bytes\n", iCur);    iHiwtr = iCur = -1;
    sqlite3_db_status(db, SQLITE_DBSTATUS_CACHE_HIT, &iCur, &iHiwtr, 1);
    fprintf(pArg->out, "Page cache hits:                     %d\n", iCur);//页面的高速缓存命中
    iHiwtr = iCur = -1;
    sqlite3_db_status(db, SQLITE_DBSTATUS_CACHE_MISS, &iCur, &iHiwtr, 1);//页面的缓存丢失
    fprintf(pArg->out, "Page cache misses:                   %d\n", iCur); 
    iHiwtr = iCur = -1;
    sqlite3_db_status(db, SQLITE_DBSTATUS_CACHE_WRITE, &iCur, &iHiwtr, 1);//页面高速缓存丢失
    fprintf(pArg->out, "Page cache writes:                   %d\n", iCur); 
    iHiwtr = iCur = -1;
    sqlite3_db_status(db, SQLITE_DBSTATUS_SCHEMA_USED, &iCur, &iHiwtr, bReset);//模式的堆使用
    fprintf(pArg->out, "Schema Heap Usage:                   %d bytes\n", iCur); 
    iHiwtr = iCur = -1;
    sqlite3_db_status(db, SQLITE_DBSTATUS_STMT_USED, &iCur, &iHiwtr, bReset);//声明的堆和后备使用
    fprintf(pArg->out, "Statement Heap/Lookaside Usage:      %d bytes\n", iCur); 
  }

  if( pArg && pArg->out && db && pArg->pStmt ){//如果得到输入数据流和当前的声明句柄
    iCur = sqlite3_stmt_status(pArg->pStmt, SQLITE_STMTSTATUS_FULLSCAN_STEP, bReset);//按步扫描
    fprintf(pArg->out, "Fullscan Steps:                      %d\n", iCur);
    iCur = sqlite3_stmt_status(pArg->pStmt, SQLITE_STMTSTATUS_SORT, bReset);//排序操作
    fprintf(pArg->out, "Sort Operations:                     %d\n", iCur);
    iCur = sqlite3_stmt_status(pArg->pStmt, SQLITE_STMTSTATUS_AUTOINDEX, bReset);//自动索引
    fprintf(pArg->out, "Autoindex Inserts:                   %d\n", iCur);
  }
  
  return 0;
}

/*
** Execute a statement or set of statements.  Print 
** any result rows/columns depending on the current mode 
** set via the supplied callback.
**
** This is very similar to SQLite's built-in sqlite3_exec() 
** function except it takes a slightly different callback 
** and callback data argument.
*/

/*执行一个或一组语句，根据当前模式输出结果，和sqlite3_exec()函数相似/*

static int shell_exec(
  sqlite3 *db,                                /* An open database */ /*一个打开的数据库*/
  const char *zSql,                           /* SQL to be evaluated */ /*要执行的SQL语句*/
  int (*xCallback)(void*,int,char**,char**,int*),   /* Callback function */ /*回调函数*/
                                              /* (not the same as sqlite3_exec) */
  struct callback_data *pArg,                 /* Pointer to struct callback_data */ /*这结构体前面定义过，用来回显我们需要的值*/
  char **pzErrMsg                             /* Error msg written here */ /*用于保存错误信息*/
){
  sqlite3_stmt *pStmt = NULL;     /* Statement to execute. */ /*pStmt存放当前的SQL语句，目前为空*/
  int rc = SQLITE_OK;             /* Return Code */ /*返回码rc 赋值为SQLITE_OK表示正常*/
  int rc2;
  const char *zLeftover;          /* Tail of unprocessed SQL */ /*指向未处理的SQL语句尾部*/

  if( pzErrMsg ){
    *pzErrMsg = NULL; /*错误信息初始化为空*/
  }

  while( zSql[0] && (SQLITE_OK == rc) ){/*还未执行语句，且返回码是SQLITE_OK表示一切正常*/
    rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, &zLeftover);/*编译语句*/
    if( SQLITE_OK != rc ){/*如果有错*/
      if( pzErrMsg ){
        *pzErrMsg = save_err_msg(db);/*写入错误信息*/
      }
    }else{
      if( !pStmt ){
        /* this happens for a comment or white-space *//*遇到注释或者空格时，执行此分支*/
        zSql = zLeftover;/*将语句指针移到未被处理的语句尾部*/
        while( IsSpace(zSql[0]) ) zSql++;/*循环处理语句*/
        continue;
      }

      /* save off the prepared statment handle and reset row count */ /*保存准备好的句柄，重置行数*/
      if( pArg ){
        pArg->pStmt = pStmt;
        pArg->cnt = 0;/*重置行数为0
      }

      /* echo the sql statement if echo on *//*如果需要回显则回显准备好的命令行*/
      if( pArg && pArg->echoOn ){
        const char *zStmtSql = sqlite3_sql(pStmt);
        fprintf(pArg->out, "%s\n", zStmtSql ? zStmtSql : zSql);
      }

      /* Output TESTCTRL_EXPLAIN text of requested *//*输出需要的TESTCTRL_EXPLAIN文档*/
      if( pArg && pArg->mode==MODE_Explain ){
        const char *zExplain = 0;
        sqlite3_test_control(SQLITE_TESTCTRL_EXPLAIN_STMT, pStmt, &zExplain);/*这是一个检测Sqlite库是否正确的函数*/
        if( zExplain && zExplain[0] ){
          fprintf(pArg->out, "%s", zExplain);
        }
      }
            /*执行第一步，然后会知道是否有一个结果，以及它的大小*/
      /* perform the first step.  this will tell us if we
      ** have a result set or not and how wide it is.
      */
      rc = sqlite3_step(pStmt);/*执行语句*/
      /* if we have a result set... *//*如果已经产生了一个结果*/
      if( SQLITE_ROW == rc ){
        /* if we have a callback... *//*如果有回调函数，执行*/
        if( xCallback ){
          /* allocate space for col name ptr, value ptr, and type *//*分配空间*/
          int nCol = sqlite3_column_count(pStmt);/*取字段数*/
          void *pData = sqlite3_malloc(3*nCol*sizeof(const char*) + 1);/*根据字段数分配空间*/
          if( !pData ){/*如果这一步发生错误*/
            rc = SQLITE_NOMEM;/*则返回SQLITE_NOMEM表示malloc函数调用失败*/
          }else{                                                                             
            char **azCols = (char **)pData;      /* Names of result columns *//*结果集的名字*/
            char **azVals = &azCols[nCol];       /* Results *//*结果*/
            int *aiTypes = (int *)&azVals[nCol]; /* Result types *//*结果类型*/
            int i;
            assert(sizeof(int) <= sizeof(char *)); 
            /* save off ptrs to column names *//* 取各字段的名称*/
            for(i=0; i<nCol; i++){ 
              azCols[i] = (char *)sqlite3_column_name(pStmt, i);
            }
            do{
              /* extract the data and data types *//*提取数据和数据类型*/
              for(i=0; i<nCol; i++){/*取各字段的值*/
                azVals[i] = (char *)sqlite3_column_text(pStmt, i);
                aiTypes[i] = sqlite3_column_type(pStmt, i);
                if( !azVals[i] && (aiTypes[i]!=SQLITE_NULL) ){
                  rc = SQLITE_NOMEM;
                  break; /* from for */
                }
              } /* end for */

              /* if data and types extracted successfully... *//*如果数据以及类型提取成功*/
              if( SQLITE_ROW == rc ){ 
                /* call the supplied callback with the result row data *//*根据当前数据调用回调函数对返回的记录进行处理*/
                if( xCallback(pArg, nCol, azVals, azCols, aiTypes) ){
                  rc = SQLITE_ABORT;/*回调函数请求中断*/
                }else{
                  rc = sqlite3_step(pStmt);/*如果没有终端就执行语句*/
                }
              }
            } while( SQLITE_ROW == rc );/*得到结果后释放空间*/
            sqlite3_free(pData);
          }
        }else{
          do{
            rc = sqlite3_step(pStmt);/*执行语句*/
          } while( rc == SQLITE_ROW );
        }
      }

      /* print usage stats if stats on *//*如果开启了统计 那就显示统计*/
      if( pArg && pArg->statsOn ){
        display_stats(db, pArg, 0);
      }

      /* Finalize the statement just executed. If this fails, save a 
      ** copy of the error message. Otherwise, set zSql to point to the
      ** next statement to execute. */
      /*完成语句的执行，如果失败保存错误信息，否则将指针指向下一个需要执行的语句*/
      rc2 = sqlite3_finalize(pStmt);
      if( rc!=SQLITE_NOMEM ) rc = rc2;
      if( rc==SQLITE_OK ){
        zSql = zLeftover;/*指向下一个需要执行的语句*/
        while( IsSpace(zSql[0]) ) zSql++;
      }else if( pzErrMsg ){
        *pzErrMsg = save_err_msg(db);/*否则保存错误信息*/
      }

      /* clear saved stmt handle *//*清除已保存的句柄*/
      if( pArg ){
        pArg->pStmt = NULL;
      }
    }
  } /* end while *//*结束循环*/

  return rc;
}


/*
** This is a different callback routine used for dumping the database.
** Each row received by this callback consists of a table name,
** the table type ("index" or "table") and SQL to create the table.
** This routine should print text sufficient to recreate the table.
*/

/*
**这是一个用于转储数据库的回调函数 ，它会收到由表名、表类
**型（索引还是表）和创建这表的SQL的行，这程序应输出足够的可以
**重建表的文档。
*/


static int dump_callback(void *pArg, int nArg, char **azArg, char **azCol){
  int rc; 
  const char *zTable;/*表名*/
  const char *zType;/*表类型*/
  const char *zSql;/*SQL语句*/
  const char *zPrepStmt = 0;
  struct callback_data *p = (struct callback_data *)pArg;
/*这是一个callback_data结构体，用来进行各种程序之间的传值以及获取当前状态*/
  UNUSED_PARAMETER(azCol);/*表示不使用最后一个参数*/
  if( nArg!=3 ) return 1;/* 如果三个参数不全则表示错误*/
  zTable = azArg[0];
  zType = azArg[1];/*将azArg的三个元素设置为表名 表类型 SQL语句*/
  zSql = azArg[2];
  /*如果表名字为"sqlite_sequence"，当SQLite数据库中包含自增列时,会自动建立一个名为 sqlite_sequence 的表*/
  if( strcmp(zTable, "sqlite_sequence")==0 ){ 
    zPrepStmt = "DELETE FROM sqlite_sequence;\n";/*将所有表的自增列都归零*/
     /*如果zTable的值为"sqlite_stat1"，所有的统计信息储存在一个名叫sqlite_stat1 的表中*/
  }else if( strcmp(zTable, "sqlite_stat1")==0 ){ 
    fprintf(p->out, "ANALYZE sqlite_master;\n");/*sqlite_master 这个表也是自动生成的，里面保存了sqLite的框架*/
  }else if( strncmp(zTable, "sqlite_", 7)==0 ){
    return 0;
  }else if( strncmp(zSql, "CREATE VIRTUAL TABLE", 20)==0 ){ /*如果SQL语句表示创建一个虚拟表*/
    char *zIns;
    if( !p->writableSchema ){
      fprintf(p->out, "PRAGMA writable_schema=ON;\n");/*如果不是可写模式 需要先调整到可写模式*/
      p->writableSchema = 1;
    }
    zIns = sqlite3_mprintf(/*格式化输入表名 表类型 SQL语句到zIns*/
       "INSERT INTO sqlite_master(type,name,tbl_name,rootpage,sql)"/*将表名 表类型 SQL语句插入到sqlite_maste表*/
       "VALUES('table','%q','%q',0,'%q');",
       zTable, zTable, zSql);
    fprintf(p->out, "%s\n", zIns);/*格式化输出zIns的内容*/
    sqlite3_free(zIns);/*释放zIns的内容*/
    return 0;
  }else{/*是可写模式就直接格式化输出zIns的内容*/
    fprintf(p->out, "%s;\n", zSql);
  }

  if( strcmp(zType, "table")==0 ){/*如果表类型为“table”*/
    sqlite3_stmt *pTableInfo = 0;
    char *zSelect = 0;
    char *zTableInfo = 0;
    char *zTmp = 0;
    int nRow = 0;
   /*用appendText函数给zTableInfo赋值，这函数之前有定义，用于语句的拼接*/
    zTableInfo = appendText(zTableInfo, "PRAGMA table_info(", 0);
    zTableInfo = appendText(zTableInfo, zTable, '"');
    zTableInfo = appendText(zTableInfo, ");", 0);
 
    rc = sqlite3_prepare(p->db, zTableInfo, -1, &pTableInfo, 0);/*解析语句*/
    free(zTableInfo);/*释放zTableInfo*/
    if( rc!=SQLITE_OK || !pTableInfo ){/*如果出现错误，返回1*/
      return 1;
    }
/*用appendText函数给zSelect赋值*/
    zSelect = appendText(zSelect, "SELECT 'INSERT INTO ' || ", 0);
    /* Always quote the table name, even if it appears to be pure ascii,
    ** in case it is a keyword. Ex:  INSERT INTO "table" ... */
    /*一般引用表名 */
    zTmp = appendText(zTmp, zTable, '"');
    if( zTmp ){
      zSelect = appendText(zSelect, zTmp, '\'');
      free(zTmp);
    }
    zSelect = appendText(zSelect, " || ' VALUES(' || ", 0);
    rc = sqlite3_step(pTableInfo);
    while( rc==SQLITE_ROW ){/*已经产生一个结果*/
      const char *zText = (const char *)sqlite3_column_text(pTableInfo, 1);
      zSelect = appendText(zSelect, "quote(", 0);
      zSelect = appendText(zSelect, zText, '"');
      rc = sqlite3_step(pTableInfo);/*pTableInfo表示的sql语句将被写入数据库*/
      if( rc==SQLITE_ROW ){
        zSelect = appendText(zSelect, "), ", 0);
      }else{
        zSelect = appendText(zSelect, ") ", 0);
      }
      nRow++;
    }
    rc = sqlite3_finalize(pTableInfo);/*销毁pTableInfoli里分配的内容*/
    if( rc!=SQLITE_OK || nRow==0 ){
      free(zSelect);
      return 1;
    }
    zSelect = appendText(zSelect, "|| ')' FROM  ", 0);
    zSelect = appendText(zSelect, zTable, '"');
 /*使用run_table_dump_query函数可实现查询，结果将以SQL语句输出 */
    rc = run_table_dump_query(p, zSelect, zPrepStmt); 
    if( rc==SQLITE_CORRUPT ){/* 数据库磁盘映像不正确*/
      zSelect = appendText(zSelect, " ORDER BY rowid DESC", 0);
      run_table_dump_query(p, zSelect, 0);
    }
    free(zSelect);
  }
  return 0;
}

/*
** Run zQuery.  Use dump_callback() as the callback routine so that
** the contents of the query are output as SQL statements.
**
** If we get a SQLITE_CORRUPT error, rerun the query after appending
** "ORDER BY rowid DESC" to the end.
*/
/*运行zQuery，用dump_callback()作为一个回调程序那么查询的内容就会作为SQL语言输出*/
static int run_schema_dump_query(
  struct callback_data *p, /*要查询的内容*/
  const char *zQuery
){
  int rc;/*定义返回值*/
  char *zErr = 0;/*初始化错误信息*/
  rc = sqlite3_exec(p->db, zQuery, dump_callback, p, &zErr);/*执行*/
  if( rc==SQLITE_CORRUPT ){/* 数据库磁盘映像不正确*/
    char *zQ2;
    int len = strlen30(zQuery);
    fprintf(p->out, "/****** CORRUPTION ERROR *******/\n"); 
    if( zErr ){
      fprintf(p->out, "/****** %s ******/\n", zErr);/*输出错误信息*/
      sqlite3_free(zErr);/*释放空间*/
      zErr = 0;
    }
    zQ2 = malloc( len+100 );/*为ZQ2分配空间*/
    if( zQ2==0 ) return rc;
    sqlite3_snprintf(len+100, zQ2, "%s ORDER BY rowid DESC", zQuery);/*调用sqlite3_snprintf函数实现输出*/
    rc = sqlite3_exec(p->db, zQ2, dump_callback, p, &zErr);/*执行*/
    if( rc ){
      fprintf(p->out, "/****** ERROR: %s ******/\n", zErr);
    }else{
      rc = SQLITE_CORRUPT;/* 数据库磁盘映像不正确*/
    }
    sqlite3_free(zErr);
    free(zQ2);
  }
  return rc;
}

/*
** Text of a help message
*/
/*
**帮助信息的文档，里面是各种点命令的说明
*/
static char zHelp[] =
/*备份指定的数据库到指定的文件，缺省为当前连接的main数据库*/
  ".backup ?DB? FILE      Backup DB (default \"main\") to FILE\n" 
  /*遇到错误后不再运行  默认是OFF */
  ".bail ON|OFF           Stop after hitting an error.  Default OFF\n"
  /*列出数据库文件名*/
  ".databases             List names and files of attached databases\n"
  /*用于转储 可生成形成数据库表的SQL脚本*/
  ".dump ?TABLE? ...      Dump the database in an SQL text format\n"

  "                         If TABLE specified, only dump tables matching\n"
  "                         LIKE pattern TABLE.\n"
  /*显示开关，设置为ON后，命令回显 */
  ".echo ON|OFF           Turn command echo on or off\n"
  ".exit                  Exit this program\n" /*退出当前程序*/
  /*开启或关闭适合于 EXPLAIN 的输出模式。如果没有带参数，则开启 EXPLAIN。*/
  ".explain ?ON|OFF?      Turn output mode suitable for EXPLAIN on or off.\n"
  "                         With no args, it turns EXPLAIN on.\n"
 /*打开或者关闭表头显示*/
  ".header(s) ON|OFF      Turn display of headers on or off\n"/
  /*显示本文档，列出所有内置命令*/
  ".help                  Show this message\n"
   /*导入指定文件的数据到指定表*/
  ".import FILE TABLE     Import data from FILE into TABLE\n"
  /*显示所有索引的名字，如果指定表名，则仅仅显示匹配该表名的数据表的索引，参数*/
  ".indices ?TABLE?       Show names of all indices\n"
  "                         If TABLE specified, only show indices for tables\n"  
  "                         matching LIKE pattern TABLE.\n"
#ifdef SQLITE_ENABLE_IOTRACE
  ".iotrace FILE          Enable I/O diagnostic logging to FILE\n"/*启用I/O诊断记录到文件*/
#endif
#ifndef SQLITE_OMIT_LOAD_EXTENSION
  ".load FILE ?ENTRY?     Load an extension library\n"  /*加载一个扩展库*/
#endif/*打开或关闭日志功能，FILE可以为标准输出stdout，或标准错误输出stderr*/
  ".log FILE|off          Turn logging on or off.  FILE can be stderr/stdout\n"
  /*设置输出模式，这里最为常用的模式是column模式，使SELECT输出列左对齐显示。*/
  ".mode MODE ?TABLE?     Set output mode where MODE is one of:\n" 
  "                         csv      Comma-separated values\n" /*以逗号分隔*/
  "                         column   Left-aligned columns.  (See .width)\n"/*列左对齐*/
  "                         html     HTML <table> code\n" /*显示HTML代码*/
  "                         insert   SQL insert statements for TABLE\n"/*sql插入语句*/
  "                         line     One value per line\n"/*一行一个值*/
  "                         list     Values delimited by .separator string\n"/*值用STRING分隔*/
  "                         tabs     Tab-separated values\n"/*以tab分隔的值*/
  "                         tcl      TCL list elements\n"/*TCL列表元素*/
  ".nullvalue STRING      Print STRING in place of NULL values\n"/*用指定的串代替输出的NULL串 */
  ".output FILENAME       Send output to FILENAME\n"/*将当前命令的所有输出重定向到指定的文件。*/
  ".output stdout         Send output to the screen\n"  /*将当前命令的所有输出重定向到标准输出(屏幕)。*/
  ".prompt MAIN CONTINUE  Replace the standard prompts\n" /*替换标准提示符*/
  ".quit                  Exit this program\n" /*退出*/
  ".read FILENAME         Execute SQL in FILENAME\n"/*执行指定文件内的SQL语句。*/
  /*从指定的文件还原数据库，缺省为main数据库，此时也可以指定其它数据库名
  **被指定的数据库成为当前连接的attached数据库。*/
  ".restore ?DB? FILE     Restore content of DB (default \"main\") from FILE\n" 
  ".schema ?TABLE?        Show the CREATE statements\n"/*显示数据表的创建语句，如果指定表名，则仅仅显示匹配该表名的数据*/
  "                         If TABLE specified, only show tables matching\n"
  "                         LIKE pattern TABLE.\n"
/*"改变输出模式和.import的字段间分隔符。 */
  ".separator STRING      Change separator used by output mode and .import\n" 
  /*打印所有SQlite环境变量的设置*/
  ".show                  Show the current values for various settings\n"  
  ".stats ON|OFF          Turn stats on or off\n"/*开启或关闭统计*/
  /*列出当前连接中main数据库的所有表名，如果指定表名，则仅仅显示匹配该表名的数据表名称
  **参数TABLENAME支持LIKE表达式支持的通配符。*/
  ".tables ?TABLE?        List names of tables\n"
  "                         If TABLE specified, only list tables matching\n"
  "                         LIKE pattern TABLE.\n"
  ".timeout MS            Try opening locked tables for MS milliseconds\n"/*尝试打开锁定的表 MS 微秒*/
  ".trace FILE|off        Output each SQL statement as it is run\n"/*输出每一个正在运行的语句*/
  ".vfsname ?AUX?         Print the name of the VFS stack\n"/*输出虚拟堆栈的名字*/
   /*在MODE为column时，设置各个字段的宽度，注意：该命令的参数顺序表示字段输出的顺序*/
  ".width NUM1 NUM2 ...   Set column widths for \"column\" mode\n" 
;

static char zTimerHelp[] =/*开启或关闭 CPU 定时器测量*/
  ".timer ON|OFF          Turn the CPU timer measurement on or off\n"
;

/* Forward reference *//*引用*/
static int process_input(struct callback_data *p, FILE *in);

/*
** Make sure the database is open.  If it is not, then open it.  If
** the database fails to open, print an error message and exit.
*/
/*
功能：确认数据库是否已经打开。如果已打开，则什么都不做。如果
**没有，则打开它。如果打开失败，输出一个错误信息。
*/
static void open_db(struct callback_data *p){
  if( p->db==0 ){/*如果数据库为空*/
    sqlite3_initialize();/*初始化Sqlite数据库*/
    sqlite3_open(p->zDbFilename, &p->db);/*zDbFilenam为存放数据库文件的名字 */
    db = p->db;
    if( db && sqlite3_errcode(db)==SQLITE_OK ){
      sqlite3_create_function(db, "shellstatic", 0, SQLITE_UTF8, 0,
          shellstaticFunc, 0, 0);
    }
    if( db==0 || SQLITE_OK!=sqlite3_errcode(db) ){/*无法打开数据库*/
      fprintf(stderr,"Error: unable to open database \"%s\": %s\n", 
          p->zDbFilename, sqlite3_errmsg(db));
      exit(1);
    }
#ifndef SQLITE_OMIT_LOAD_EXTENSION
    sqlite3_enable_load_extension(p->db, 1);
#endif
  }
}

/*
** Do C-language style dequoting.
**
**    \t    -> tab
**    \n    -> newline
**    \r    -> carriage return
**    \NNN  -> ascii character NNN in octal
**    \\    -> backslash
*/
/* C语言风格的引用*/
static void resolve_backslashes(char *z){
  int i, j;
  char c;
  for(i=j=0; (c = z[i])!=0; i++, j++){
    if( c=='\\' ){ /*字符值为'\\'，表示自增 */
      c = z[++i];
      if( c=='n' ){
        c = '\n';/*字符值为'c'，表示换行*/
      }else if( c=='t' ){
        c = '\t';/* 字符值为't'，表示标签*/
      }else if( c=='r' ){
        c = '\r';/*字符值为'r'，表示回车*/
      }else if( c>='0' && c<='7' ){
        c -= '0';
        if( z[i+1]>='0' && z[i+1]<='7' ){/*如果是八进制*/
          i++;
          c = (c<<3) + z[i] - '0';
          if( z[i+1]>='0' && z[i+1]<='7' ){
            i++;
            c = (c<<3) + z[i] - '0';
          }
        }
      }
    }
    z[j] = c;
  }
  z[j] = 0;
}

/*
** Interpret zArg as a boolean value.  Return either 0 or 1.
*/
/*
**将zArg翻译为布尔值，返回1或0
*/

static int booleanValue(char *zArg){
  int val = atoi(zArg);
  int j;
  for(j=0; zArg[j]; j++){
    zArg[j] = ToLower(zArg[j]);/*转换为小写*/
  }
  if( strcmp(zArg,"on")==0 ){/*“on”转化为布尔值1*/
    val = 1;
  }else if( strcmp(zArg,"yes")==0 ){/*“yes”转化为布尔值1*/
    val = 1;
  }
  return val;/*返回值*/
}

/*
** Close an output file, assuming it is not stderr or stdout
*/
/*
**关闭一个打开的文件 假设不是标准错误或者标准输出
*/
static void output_file_close(FILE *f){
  if( f && f!=stdout && f!=stderr ) fclose(f);
}

/*
** Try to open an output file.   The names "stdout" and "stderr" are
** recognized and do the right thing.  NULL is returned if the output 
** filename is "off".
*/
/*打开一个输出文件*/
static FILE *output_file_open(const char *zFile){
  FILE *f;/*f是需要打开的文件名字*/
  if( strcmp(zFile,"stdout")==0 ){/*"认可stdout"*/
    f = stdout;
  }else if( strcmp(zFile, "stderr")==0 ){/*认可"stderr"*/
    f = stderr;
  }else if( strcmp(zFile, "off")==0 ){/*如果输出的文件名是OFF，返回NULL*/
    f = 0;
  }else{
    f = fopen(zFile, "wb");
    if( f==0 ){
      fprintf(stderr, "Error: cannot open \"%s\"\n", zFile);
    }
  }
  return f;
}

/*
** A routine for handling output from sqlite3_trace().
*/

static void sql_trace_callback(void *pArg, const char *z){
  FILE *f = (FILE*)pArg;
  if( f ) fprintf(f, "%s\n", z);
}

/*
** A no-op routine that runs with the ".breakpoint" doc-command.  This is
** a useful spot to set a debugger breakpoint.
*/
/*
**一个空程序,在断点运行命令
*/

static void test_breakpoint(void){
  static int nCall = 0;
  nCall++;
}

/*
** If an input line begins with "." then invoke this routine to
** process that line.
**
** Return 1 on error, 2 to exit, and 0 otherwise.
*/

/*如果一个输入行以“.”开始（CLP命令）
**那么调用此程序处理那个行
**返回1表示错误 2表示结束 0表示其他
*/   
/*定义一个**do_meta_command 函**数，第一个参数是字符**串，第二个参数是回调**函数的指针*/
static int do_meta_command(char *zLine, struct callback_data *p){
  int i = 1;
  int nArg = 0;
  int n, c;
  int rc = 0;
  char *azArg[50];

  /* Parse the input line into tokens.*//*解析输入行 将字符串保存在azArg数组*/
  */
  while( zLine[i] && nArg<ArraySize(azArg) ){ /*逐行分析语句*/
    while( IsSpace(zLine[i]) ){ i++; }
    if( zLine[i]==0 ) break;      /* 没有语句后结束*/
    if( zLine[i]=='\'' || zLine[i]=='"' ){/*语句为换行符或者空格 */
      int delim = zLine[i++];/*则跳过*/
      azArg[nArg++] = &zLine[i];/*输入语句地址到数组*/
      while( zLine[i] && zLine[i]!=delim ){ i++; }
      if( zLine[i]==delim ){
        zLine[i++] = 0;
      }
      if( delim=='"' ) resolve_backslashes(azArg[nArg-1]);/*如果数据源是空格*/
    }else{
      azArg[nArg++] = &zLine[i];
      while( zLine[i] && !IsSpace(zLine[i]) ){ i++; }
      if( zLine[i] ) zLine[i++] = 0;
      resolve_backslashes(azArg[nArg-1]);
    }
  }

  /* Process the input line.*//*处理输入行*/
  if( nArg==0 ) return 0; /* no tokens, no error */
  n = strlen30(azArg[0]);
  c = azArg[0][0];
  
  /*备份一个指定的数据库（A）到指定的文件（B，缺省为当前连接的main数据库）*/
  if( c=='b' && n>=3 && strncmp(azArg[0], "backup", n)==0 && nArg>1 && nArg<4){
    const char *zDestFile;/*A的名字 */
    const char *zDb;/*B的名字 */
    sqlite3 *pDest;/*需要备份的数据库A */
    sqlite3_backup *pBackup;/*目标数据库B */
    if( nArg==2 ){
      zDestFile = azArg[1];
      zDb = "main"; /*缺省为main*/
    }else{
      zDestFile = azArg[2];
      zDb = azArg[1];
    }
    rc = sqlite3_open(zDestFile, &pDest);/*打开需要备份的数据库 */
    if( rc!=SQLITE_OK ){
      fprintf(stderr, "Error: cannot open \"%s\"\n", zDestFile);
      sqlite3_close(pDest);
      return 1;
    }
    open_db(p);
    /*Sqlite3_backup_init() ：第一个参数是目标数据库，第三个参数是源数据库
    规定两者不能相同，如果成功将返回指向源数据库的指针*/
    pBackup = sqlite3_backup_init(pDest, "main", p->db, zDb);
    if( pBackup==0 ){/*发生错误则输出错误信息 */
      fprintf(stderr, "Error: %s\n", sqlite3_errmsg(pDest));
      sqlite3_close(pDest);
      return 1;
    }
    /*sqlite3_backup_step用于备份数据 */
    while(  (rc = sqlite3_backup_step(pBackup,100))==SQLITE_OK ){}
    sqlite3_backup_finish(pBackup);
    if( rc==SQLITE_DONE ){
      rc = 0;
    }else{
      fprintf(stderr, "Error: %s\n", sqlite3_errmsg(pDest));
      rc = 1;
    }/*完成后关闭，否则写入错误信息 */
    sqlite3_close(pDest);/*关闭pDest指向的空间*/
  }else


/*遇到错误时不再继续, 默认为OFF*/
  if( c=='b' && n>=3 && strncmp(azArg[0], "bail", n)==0 && nArg>1 && nArg<3 ){
    bail_on_error = booleanValue(azArg[1]);/*由转化的布尔值决定开关*/
  }else

  /* The undocumented ".breakpoint" command causes a call to the no-op
  ** routine named test_breakpoint().
  */
  if( c=='b' && n>=3 && strncmp(azArg[0], "breakpoint", n)==0 ){
    test_breakpoint();
  }else
/*列出数据库文件名*/
  if( c=='d' && n>1 && strncmp(azArg[0], "databases", n)==0 && nArg==1 ){
    struct callback_data data;/*声明回显参数*/
    char *zErrMsg = 0;/*声明一个存放错误信息的指针*/
    open_db(p);/*打开P指向的数据库*/
    memcpy(&data, p, sizeof(data));/*从P所指的内存地址的起始位置 拷贝date长度的字符
    到date空间的起始位置中*/
    data.showHeader = 1;/*打开表头显示*/
    data.mode = MODE_Column;/*设置到Column模式*/
    data.colWidth[0] = 3;/*定义列宽*/
    data.colWidth[1] = 15;
    data.colWidth[2] = 58;
    data.cnt = 0;/*记录数为0*/
    sqlite3_exec(p->db, "PRAGMA database_list; ", callback, &data, &zErrMsg);
    /*执行显示数据库列表*/
    if( zErrMsg ){/*保存错误信息*/
      fprintf(stderr,"Error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      rc = 1;
    }
  }else

  if( c=='d' && strncmp(azArg[0], "dump", n)==0 && nArg<3 ){
    open_db(p);/*打开数据库*/
    /* When playing back a "dump", the content might appear in an order
    ** which causes immediate foreign key constraints to be violated.
    ** So disable foreign-key constraint enforcement to prevent problems. */
    fprintf(p->out, "PRAGMA foreign_keys=OFF;\n");
    fprintf(p->out, "BEGIN TRANSACTION;\n");
    p->writableSchema = 0;/*转储数据库时 要锁住（不可写），以免出现错误*/
    sqlite3_exec(p->db, "SAVEPOINT dump; PRAGMA writable_schema=ON", 0, 0, 0);
    p->nErr = 0;
    /*如果.dump命令后面没有参数，则需要对数据库模式和所有表记录做备份*/
    if( nArg==1 ){
      run_schema_dump_query(p, 
        "SELECT name, type, sql FROM sqlite_master "
        "WHERE sql NOT NULL AND type=='table' AND name!='sqlite_sequence'"
      );
      run_schema_dump_query(p, 
        "SELECT name, type, sql FROM sqlite_master "
        "WHERE name=='sqlite_sequence'"
      );
      run_table_dump_query(p,
        "SELECT sql FROM sqlite_master "
        "WHERE sql NOT NULL AND type IN ('index','trigger','view')", 0
      );
      /*如果有参数，则只对参数所对应的表进行备份。*/
    }else{
      int i;
      for(i=1; i<nArg; i++){
        zShellStatic = azArg[i];
        run_schema_dump_query(p,
          "SELECT name, type, sql FROM sqlite_master "
          "WHERE tbl_name LIKE shellstatic() AND type=='table'"
          "  AND sql NOT NULL");
        run_table_dump_query(p,
          "SELECT sql FROM sqlite_master "
          "WHERE sql NOT NULL"
          "  AND type IN ('index','trigger','view')"
          "  AND tbl_name LIKE shellstatic()", 0
        );
        zShellStatic = 0;
      }
    }
    if( p->writableSchema ){
      fprintf(p->out, "PRAGMA writable_schema=OFF;\n");
      p->writableSchema = 0;
    }
    sqlite3_exec(p->db, "PRAGMA writable_schema=OFF;", 0, 0, 0);
    sqlite3_exec(p->db, "RELEASE dump;", 0, 0, 0);
    fprintf(p->out, p->nErr ? "ROLLBACK; -- due to errors\n" : "COMMIT;\n");
  }else
/*打开或者关闭命令行回显*/
  if( c=='e' && strncmp(azArg[0], "echo", n)==0 && nArg>1 && nArg<3 ){
    p->echoOn = booleanValue(azArg[1]);
  }else
/*退出当前程序*/
  if( c=='e' && strncmp(azArg[0], "exit", n)==0  && nArg==1 ){
    rc = 2;
  }else

  if( c=='e' && strncmp(azArg[0], "explain", n)==0 && nArg<3 ){
    /*如果有两个以上参数，取第二个参数布尔值，否则取1*/
    int val = nArg>=2 ? booleanValue(azArg[1]) : 1;
    if(val == 1) {
      if(!p->explainPrev.valid) {
        p->explainPrev.valid = 1;
        p->explainPrev.mode = p->mode;
        p->explainPrev.showHeader = p->showHeader; /*显示表头*/
        memcpy(p->explainPrev.colWidth,p->colWidth,sizeof(p->colWidth));
      }
      /*设置这个条件，那么如果已经在explain模式下就不会运行*/
      /* We could put this code under the !p->explainValid
      ** condition so that it does not execute if we are already in
      ** explain mode. However, always executing it allows us an easy
      ** was to reset to explain mode in case the user previously
      ** did an .explain followed by a .width, .mode or .header
      ** command.
      */
      p->mode = MODE_Explain;/*设置模式*/
      p->showHeader = 1;
      memset(p->colWidth,0,ArraySize(p->colWidth));/*初始化，即清零*/
      p->colWidth[0] = 4;                  /* addr */
      p->colWidth[1] = 13;                 /* opcode */
      p->colWidth[2] = 4;                  /* P1 */
      p->colWidth[3] = 4;                  /* P2 */
      p->colWidth[4] = 4;                  /* P3 */
      p->colWidth[5] = 13;                 /* P4 */
      p->colWidth[6] = 2;                  /* P5 */
      p->colWidth[7] = 13;                  /* Comment */
    }else if (p->explainPrev.valid) {
      p->explainPrev.valid = 0;
      p->mode = p->explainPrev.mode;
      p->showHeader = p->explainPrev.showHeader;
      memcpy(p->colWidth,p->explainPrev.colWidth,sizeof(p->colWidth));
    }
  }else
/*打开或者关闭表头显示*/
  if( c=='h' && (strncmp(azArg[0], "header", n)==0 ||
                 strncmp(azArg[0], "headers", n)==0) && nArg>1 && nArg<3 ){
    p->showHeader = booleanValue(azArg[1]);
  }else
/*显示帮助文档*/
  if( c=='h' && strncmp(azArg[0], "help", n)==0 ){
    fprintf(stderr,"%s",zHelp);
    if( HAS_TIMER ){
      fprintf(stderr,"%s",zTimerHelp);
    }
  }else
/*导入指定的文件到指定的表*/
  if( c=='i' && strncmp(azArg[0], "import", n)==0 && nArg==3 ){
    char *zTable = azArg[2];    /* Insert data into this table *//*将被导入数据的表*/
    char *zFile = azArg[1];     /* The file from which to extract data *//*将被提取数据的文件*/
    sqlite3_stmt *pStmt = NULL; /* A statement *//*定义一个空句柄*/
    int nCol;                   /* Number of columns in the table *//*定义表的列数*/
    int nByte;                  /* Number of bytes in an SQL string *//*一个SQL串的比特数*/
    int i, j;                   /* Loop counters */
    int nSep;                   /* Number of bytes in p->separator[] */
    char *zSql;                 /* An SQL statement *//*一个SQL语句句柄*/
    char *zLine;                /* A single line of input from the file *//*文件的行*/
    char **azCol;               /* zLine[] broken up into columns */
    char *zCommit;              /* How to commit changes */   
    FILE *in;                   /* The input file */
    int lineno = 0;             /* Line number of input file */

    open_db(p);
    nSep = strlen30(p->separator);
    if( nSep==0 ){
      fprintf(stderr, "Error: non-null separator required for import\n");
      return 1;
    }
    zSql = sqlite3_mprintf("SELECT * FROM %s", zTable);
    if( zSql==0 ){
      fprintf(stderr, "Error: out of memory\n");
      return 1;
    }
    nByte = strlen30(zSql);
    rc = sqlite3_prepare(p->db, zSql, -1, &pStmt, 0);
    sqlite3_free(zSql);
    if( rc ){
      if (pStmt) sqlite3_finalize(pStmt);
      fprintf(stderr,"Error: %s\n", sqlite3_errmsg(db));
      return 1;
    }
    /* sqlite3_step()返回SQLITE_ROW后，该函数返回当前记录的列数，但是要求语句句柄上有活动游标*/
    nCol = sqlite3_column_count(pStmt);
    sqlite3_finalize(pStmt);
    pStmt = 0;
    if( nCol==0 ) return 0; /* no columns, no error */
    zSql = malloc( nByte + 20 + nCol*2 );
    if( zSql==0 ){
      fprintf(stderr, "Error: out of memory\n");
      return 1;
    }
    sqlite3_snprintf(nByte+20, zSql, "INSERT INTO %s VALUES(?", zTable);
    j = strlen30(zSql);
    for(i=1; i<nCol; i++){
      zSql[j++] = ',';
      zSql[j++] = '?';
    }
    zSql[j++] = ')';
    zSql[j] = 0;
    rc = sqlite3_prepare(p->db, zSql, -1, &pStmt, 0);
    free(zSql);
    if( rc ){
      fprintf(stderr, "Error: %s\n", sqlite3_errmsg(db));
      if (pStmt) sqlite3_finalize(pStmt);
      return 1;
    }
    in = fopen(zFile, "rb");
    if( in==0 ){
      fprintf(stderr, "Error: cannot open \"%s\"\n", zFile);
      sqlite3_finalize(pStmt);
      return 1;
    }
    azCol = malloc( sizeof(azCol[0])*(nCol+1) );
    if( azCol==0 ){
      fprintf(stderr, "Error: out of memory\n");
      fclose(in);
      sqlite3_finalize(pStmt);
      return 1;
    }
    sqlite3_exec(p->db, "BEGIN", 0, 0, 0);
    zCommit = "COMMIT";
    while( (zLine = local_getline(0, in, 1))!=0 ){
      char *z, c;
      int inQuote = 0;
      lineno++;
      azCol[0] = zLine;
      for(i=0, z=zLine; (c = *z)!=0; z++){
        if( c=='"' ) inQuote = !inQuote;
        if( c=='\n' ) lineno++;
        if( !inQuote && c==p->separator[0] && strncmp(z,p->separator,nSep)==0 ){
          *z = 0;
          i++;
          if( i<nCol ){
            azCol[i] = &z[nSep];
            z += nSep-1;
          }
        }
      } /* end for */
      *z = 0;
      if( i+1!=nCol ){
        fprintf(stderr,
                "Error: %s line %d: expected %d columns of data but found %d\n",
                zFile, lineno, nCol, i+1);
        zCommit = "ROLLBACK";
        free(zLine);
        rc = 1;
        break; /* from while */
      }
      for(i=0; i<nCol; i++){
        if( azCol[i][0]=='"' ){
          int k;
          for(z=azCol[i], j=1, k=0; z[j]; j++){
            if( z[j]=='"' ){ j++; if( z[j]==0 ) break; }
            z[k++] = z[j];
          }
          z[k] = 0;
        }
        sqlite3_bind_text(pStmt, i+1, azCol[i], -1, SQLITE_STATIC);
      }/*sqlite3_bind_text的第二个参数为序号（从1开始），第三个参数为字符串值，第四个参数为字符串长度。
      第五个参数为一个函数指针，SQLITE3执行完操作后回调此函数，通常用于释放字符串占用的内存。*/
      sqlite3_step(pStmt);/* 执行语句*/
      rc = sqlite3_reset(pStmt);
      free(zLine);
      if( rc!=SQLITE_OK ){/* 输出错误信息*/
        fprintf(stderr,"Error: %s\n", sqlite3_errmsg(db));
        zCommit = "ROLLBACK";
        rc = 1;
        break; /* from while */
      }
    } /* end while */
    free(azCol);
    fclose(in);
    sqlite3_finalize(pStmt);
    sqlite3_exec(p->db, zCommit, 0, 0, 0);
  }else

  if( c=='i' && strncmp(azArg[0], "indices", n)==0 && nArg<3 ){
    struct callback_data data;/*定义回显结构体*/
    char *zErrMsg = 0;/*保存错误信息的指针*/
    open_db(p);/*打开数据库*/
    memcpy(&data, p, sizeof(data));
    data.showHeader = 0;/*打开表头显示*/
    data.mode = MODE_List;
    if( nArg==1 ){/*没有参数时 */
      rc = sqlite3_exec(p->db,
        "SELECT name FROM sqlite_master "
        "WHERE type='index' AND name NOT LIKE 'sqlite_%' "
        "UNION ALL "
        "SELECT name FROM sqlite_temp_master "
        "WHERE type='index' "/*显示所有索引*/
        "ORDER BY 1",
        callback, &data, &zErrMsg
      );
    }else{{/*有参数时，显示对应表的索引*/
      zShellStatic = azArg[1];
      rc = sqlite3_exec(p->db,
        "SELECT name FROM sqlite_master "
        "WHERE type='index' AND tbl_name LIKE shellstatic() "
        "UNION ALL "
        "SELECT name FROM sqlite_temp_master "
        "WHERE type='index' AND tbl_name LIKE shellstatic() "
        "ORDER BY 1",
        callback, &data, &zErrMsg
      );
      zShellStatic = 0;
    }
    if( zErrMsg ){/*如果错误则输出错误信息*/
      fprintf(stderr,"Error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      rc = 1;
    }else if( rc != SQLITE_OK ){
      fprintf(stderr,"Error: querying sqlite_master and sqlite_temp_master\n");
      rc = 1;
    }
  }else

#ifdef SQLITE_ENABLE_IOTRACE
  if( c=='i' && strncmp(azArg[0], "iotrace", n)==0 ){
    extern void (*sqlite3IoTrace)(const char*, ...);
    if( iotrace && iotrace!=stdout ) fclose(iotrace);
    iotrace = 0;
    if( nArg<2 ){
      sqlite3IoTrace = 0;
    }else if( strcmp(azArg[1], "-")==0 ){
      sqlite3IoTrace = iotracePrintf;
      iotrace = stdout;
    }else{
      iotrace = fopen(azArg[1], "w");
      if( iotrace==0 ){
        fprintf(stderr, "Error: cannot open \"%s\"\n", azArg[1]);
        sqlite3IoTrace = 0;
        rc = 1;
      }else{
        sqlite3IoTrace = iotracePrintf;
      }
    }
  }else
#endif
/*加载一个扩展库*/
#ifndef SQLITE_OMIT_LOAD_EXTENSION
  if( c=='l' && strncmp(azArg[0], "load", n)==0 && nArg>=2 ){
    const char *zFile, *zProc;
    char *zErrMsg = 0;
    zFile = azArg[1];
    zProc = nArg>=3 ? azArg[2] : 0;
    open_db(p);
    rc = sqlite3_load_extension(p->db, zFile, zProc, &zErrMsg);
    if( rc!=SQLITE_OK ){
      fprintf(stderr, "Error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      rc = 1;
    }
  }else
#endif
/*打开或关闭日志功能，FILE可以为标准输出stdout，或标准错误输出stderr*/
  if( c=='l' && strncmp(azArg[0], "log", n)==0 && nArg>=2 ){
    const char *zFile = azArg[1];
    output_file_close(p->pLog);
    p->pLog = output_file_open(zFile);
  }else
*/
  if( c=='m' && strncmp(azArg[0], "mode", n)==0 && nArg==2 ){
    int n2 = strlen30(azArg[1]);
    if( (n2==4 && strncmp(azArg[1],"line",n2)==0)
        ||
        (n2==5 && strncmp(azArg[1],"lines",n2)==0) ){
      p->mode = MODE_Line;/*设置到Line模式*/
    }else if( (n2==6 && strncmp(azArg[1],"column",n2)==0)
              ||
              (n2==7 && strncmp(azArg[1],"columns",n2)==0) ){
      p->mode = MODE_Column;/*设置到Column模式*/
    }else if( n2==4 && strncmp(azArg[1],"list",n2)==0 ){
      p->mode = MODE_List;/*设置到List模式*/
    }else if( n2==4 && strncmp(azArg[1],"html",n2)==0 ){
      p->mode = MODE_Html;/*设置到Html模式*/
    }else if( n2==3 && strncmp(azArg[1],"tcl",n2)==0 ){
      p->mode = MODE_Tcl;/*设置到Tcl模式*/
    }else if( n2==3 && strncmp(azArg[1],"csv",n2)==0 ){
      p->mode = MODE_Csv;/*设置到Csv模式*/
      sqlite3_snprintf(sizeof(p->separator), p->separator, ",");
    }else if( n2==4 && strncmp(azArg[1],"tabs",n2)==0 ){
      p->mode = MODE_List; /*设置到List模式*/
      sqlite3_snprintf(sizeof(p->separator), p->separator, "\t");
    }else if( n2==6 && strncmp(azArg[1],"insert",n2)==0 ){
      p->mode = MODE_Insert;/*设置到Insert模式*/
      set_table_name(p, "table");
    }else {
      fprintf(stderr,"Error: mode should be one of: "
         "column csv html insert line list tabs tcl\n");
      rc = 1;
    }
  }else

  if( c=='m' && strncmp(azArg[0], "mode", n)==0 && nArg==3 ){
    int n2 = strlen30(azArg[1]);
    if( n2==6 && strncmp(azArg[1],"insert",n2)==0 ){
      p->mode = MODE_Insert;
      set_table_name(p, azArg[2]);
    }else {
      fprintf(stderr, "Error: invalid arguments: "
        " \"%s\". Enter \".help\" for help\n", azArg[2]);
      rc = 1;
    }
  }else
/*用指定的串代替输出的NULL串*/
  if( c=='n' && strncmp(azArg[0], "nullvalue", n)==0 && nArg==2 ) {
    sqlite3_snprintf(sizeof(p->nullvalue), p->nullvalue,
                     "%.*s", (int)ArraySize(p->nullvalue)-1, azArg[1]);
  }else
/*将当前命令的所有输出重定向到标准输出(屏幕)。*/
  if( c=='o' && strncmp(azArg[0], "output", n)==0 && nArg==2 ){
    if( p->outfile[0]=='|' ){
      pclose(p->out);
    }else{
      output_file_close(p->out);
    }
    p->outfile[0] = 0;
    if( azArg[1][0]=='|' ){
      p->out = popen(&azArg[1][1], "w");
      if( p->out==0 ){
        fprintf(stderr,"Error: cannot open pipe \"%s\"\n", &azArg[1][1]);
        p->out = stdout;
        rc = 1;
      }else{
        sqlite3_snprintf(sizeof(p->outfile), p->outfile, "%s", azArg[1]);
      }
    }else{
      p->out = output_file_open(azArg[1]);
      if( p->out==0 ){
        if( strcmp(azArg[1],"off")!=0 ){
          fprintf(stderr,"Error: cannot write to \"%s\"\n", azArg[1]);
        }
        p->out = stdout;
        rc = 1;
      } else {
        sqlite3_snprintf(sizeof(p->outfile), p->outfile, "%s", azArg[1]);
      }
    }
  }else
/*替换默认的标准提示符*/
  if( c=='p' && strncmp(azArg[0], "prompt", n)==0 && (nArg==2 || nArg==3)){
    if( nArg >= 2) {
      strncpy(mainPrompt,azArg[1],(int)ArraySize(mainPrompt)-1);
    }
    if( nArg >= 3) {
      strncpy(continuePrompt,azArg[2],(int)ArraySize(continuePrompt)-1);
    }
  }else
/*停止当前程序*/
  if( c=='q' && strncmp(azArg[0], "quit", n)==0 && nArg==1 ){
    rc = 2;
  }else
/*执行指定文件内的sql语句*/
  if( c=='r' && n>=3 && strncmp(azArg[0], "read", n)==0 && nArg==2 ){
    FILE *alt = fopen(azArg[1], "rb");
    if( alt==0 ){
      fprintf(stderr,"Error: cannot open \"%s\"\n", azArg[1]);
      rc = 1;
    }else{
      rc = process_input(p, alt);
      fclose(alt);
    }
  }else
/*从指定的文件还原数据库，缺省为main数据库，此时也可以指定其它数据库名
被指定的数据库成为当前连接的attached数据库。*/
  if( c=='r' && n>=3 && strncmp(azArg[0], "restore", n)==0 && nArg>1 && nArg<4){
    const char *zSrcFile; /*需要被还原的源数据库*/
    const char *zDb;/*指定的，目标数据库，缺省为MAIN*/
    sqlite3 *pSrc;
    sqlite3_backup *pBackup;
    int nTimeout = 0;

    if( nArg==2 ){
      zSrcFile = azArg[1];
      zDb = "main";
    }else{
      zSrcFile = azArg[2];
      zDb = azArg[1];
    }
    rc = sqlite3_open(zSrcFile, &pSrc);/*打开需要被还原的源数据库*/
    if( rc!=SQLITE_OK ){
      fprintf(stderr, "Error: cannot open \"%s\"\n", zSrcFile);
      sqlite3_close(pSrc);
      return 1;
    }
    open_db(p);
    /* 第一个参数为目标数据库，第三个参数为源数据库，返回指向源数据库的指针*/
    pBackup = sqlite3_backup_init(p->db, zDb, pSrc, "main");
    if( pBackup==0 ){
      fprintf(stderr, "Error: %s\n", sqlite3_errmsg(p->db));
      sqlite3_close(pSrc);
      return 1;
    }
    while( (rc = sqlite3_backup_step(pBackup,100))==SQLITE_OK/*开始备份*/
          || rc==SQLITE_BUSY  ){
      if( rc==SQLITE_BUSY ){
        if( nTimeout++ >= 3 ) break; /*如果数据文件被锁定，三次之后终断*/
        sqlite3_sleep(100);  /*线程将被挂起暂停执行100毫秒*/
      }
    }/*备份完毕后释放空间*/
    sqlite3_backup_finish(pBackup);
    if( rc==SQLITE_DONE ){
      rc = 0;
    }else if( rc==SQLITE_BUSY || rc==SQLITE_LOCKED ){
      fprintf(stderr, "Error: source database is busy\n");
      rc = 1;
    }else{
      fprintf(stderr, "Error: %s\n", sqlite3_errmsg(p->db));
      rc = 1;
    }
    sqlite3_close(pSrc);
  }else
/*备份完毕后释放空间*/备份成功后将释放空间关闭数据库，失败会保存错误信息*/
  if( c=='s' && strncmp(azArg[0], "schema", n)==0 && nArg<3 ){
    struct callback_data data;
    char *zErrMsg = 0;
    /*确保数据库是打开的。如果不是，则将其打开。
    如果数据库无法打开，输出错误消息并退出*/
    open_db(p);	
    /* 初始化备份,用于创建sqlite3_backup对象，
    该对象将作为本次拷贝操作的句柄传给其余两个函数。*/
    pBackup = sqlite3_backup_init(p->db, zDb, pSrc, "main");
    if( pBackup==0 ){/*初始化备份失败*/
      fprintf(stderr, "Error: %s\n", sqlite3_errmsg(p->db));/*把错误信息按要求格式输出到stderr文件中*/
      sqlite3_close(pSrc);/*关闭pSrc指向的空间*/
      return 1;
    }
    while( (rc = sqlite3_backup_step(pBackup,100))==SQLITE_OK /*判断sqlite3_backup_step 是否成功复制

100个页面。*/
          || rc==SQLITE_BUSY  ){
      if( rc==SQLITE_BUSY ) {  
        if( nTimeout++ >= 3 ) break;/*三次请求之后，数据库文件一直锁定，则跳出当前操作*/ 
        sqlite3_sleep(100);  /*sqlite3_sleep 函数使当前线程暂停执行100毫秒。*/
      }
    }
    sqlite3_backup_finish(pBackup); /*释放与pBackup 相关联的所有资源。*/
    if( rc==SQLITE_DONE ){ /*判断sqlite3_backup_step 是否完成所有备份操作。*/
      rc = 0;
    }else if( rc==SQLITE_BUSY || rc==SQLITE_LOCKED ){/*当database connection 被写入到源数据库

时,sqlite3_backup_step 就会返回SQLITE_LOCKED */
      fprintf(stderr, "Error: source database is busy\n");/*把错误信息输出到stderr中*/
      rc = 1;
    }else{
      fprintf(stderr, "Error: %s\n", sqlite3_errmsg(p->db));/*把错误信息按格式要求输出到stderr中*/
      rc = 1;
    }
    sqlite3_close(pSrc);/*sqlite3的对象被成功销毁并且所有相关的资源被释放。*/
  }else
  /*判断是否输入了.schema命令
  该命令可以得到一个表或视图的定义(DDL)语句。*/
  if( c=='s' && strncmp(azArg[0], "schema", n)==0 && nArg<3 ){
    struct callback_data data;/*回显参数*/
    char *zErrMsg = 0;
    open_db(p);/*打开数据库*/
    memcpy(&data, p, sizeof(data));/* 从p所指的内存地址的起始位置开始拷贝sizeof(data)个字节data的内存地

址的起始位置中。*/
    data.showHeader = 0;
    data.mode = MODE_Semi;/*将宏定义的MODE_Semi的值 赋给结构体变量*/
    if( nArg>1 ){
      int i;
      for(i=0; azArg[1][i]; i++) azArg[1][i] = ToLower(azArg[1][i]);/* 把字符转换成小写字母,非字母字符不做出

处理 */
      if( strcmp(azArg[1],"sqlite_master")==0 ){/*azArg[1]指向字符串与要求字符串匹配，则输出对应表*/
        char *new_argv[2], *new_colv[2];/*定义两个指针数组*/
        new_argv[0] = "CREATE TABLE sqlite_master (\n"/*SQL语句，创建sqlite_master表*/
                      "  type text,\n"
                      "  name text,\n"
                      "  tbl_name text,\n"
                      "  rootpage integer,\n"
                      "  sql text\n"
                      ")";
        new_argv[1] = 0;
        new_colv[0] = "sql";
        new_colv[1] = 0;
        callback(&data, 1, new_argv, new_colv);/*回调函数用以显示查询结果，下同*/
        rc = SQLITE_OK;
      }else if( strcmp(azArg[1],"sqlite_temp_master")==0 ){/*azArg[1]指向字符串与要求字符串匹配，则输出对

应表*/
        char *new_argv[2], *new_colv[2];/*创建两个指针数组*/
        new_argv[0] = "CREATE TEMP TABLE sqlite_temp_master (\n"/*将SQL语句赋给new_argv[0]数组*/
                      "  type text,\n"
                      "  name text,\n"
                      "  tbl_name text,\n"
                      "  rootpage integer,\n"
                      "  sql text\n"
                      ")";
        new_argv[1] = 0;
        new_colv[0] = "sql";
        new_colv[1] = 0;
        callback(&data, 1, new_argv, new_colv);/*回调函数用以显示查询结果，下同*/
        rc = SQLITE_OK;
      }else{
        zShellStatic = azArg[1];/*静态指针zShellStatic指向常量指针azArg[1]的内存空间*/
        rc = sqlite3_exec(p->db,/*对p指针指向的数据库执行下列SQL语句*/
          "SELECT sql FROM "
          "  (SELECT sql sql, type type, tbl_name tbl_name, name name, rowid x"
          "     FROM sqlite_master UNION ALL"
          "   SELECT sql, type, tbl_name, name, rowid FROM sqlite_temp_master) "
          "WHERE lower(tbl_name) LIKE shellstatic()"
          "  AND type!='meta' AND sql NOTNULL "
          "ORDER BY substr(type,2,1), "
                  " CASE type WHEN 'view' THEN rowid ELSE name END",
          callback(&data, &zErrMsg);/*显示查询结果*/
        zShellStatic = 0;
      }
    }else{
      rc = sqlite3_exec(p->db,/*对db数据库执行下列SQL语句*/
         "SELECT sql FROM "
         "  (SELECT sql sql, type type, tbl_name tbl_name, name name, rowid x"
         "     FROM sqlite_master UNION ALL"
         "   SELECT sql, type, tbl_name, name, rowid FROM sqlite_temp_master) "
         "WHERE type!='meta' AND sql NOTNULL AND name NOT LIKE 'sqlite_%'"
         "ORDER BY substr(type,2,1),"
                  " CASE type WHEN 'view' THEN rowid ELSE name END",
         callback( &data, &zErrMsg);
    }
    if( zErrMsg ){/*若不为空，则输出zErrMsg中的内容到stderr文件中*/
      fprintf(stderr,"Error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);/*释放zErrMsg的内存空间*/
      rc = 1;
    }else if( rc != SQLITE_OK ){/*是否成功完成*/
      fprintf(stderr,"Error: querying schema information\n");
      rc = 1;
    }else{
      rc = 0;
    }
  }else
  if( c=='s' && strncmp(azArg[0], "separator", n)==0 && nArg==2 ){ /*判断是否输入了.separator 命令*/
/*将 azArg[1]按照format格式化成字符串，然后输出。若成功则返回欲写入的字符串长度，若出错则返回负值。*/
    sqlite3_snprintf(sizeof(p->separator), p->separator,
                     "%.*s", (int)sizeof(p->separator)-1, azArg[1]);
  }else
  if( c=='s' && strncmp(azArg[0], "show", n)==0 && nArg==1 ){/*判断是否输入了.show 命令*/
    int i;
    fprintf(p->out,"%9.9s: %s\n","echo", p->echoOn ? "on" : "off"); /*回显开关*/
    fprintf(p->out,"%9.9s: %s\n","explain", p->explainPrev.valid ? "on" :"off");
    fprintf(p->out,"%9.9s: %s\n","headers", p->showHeader ? "on" : "off");/*是否打开表头*/
    fprintf(p->out,"%9.9s: %s\n","mode", modeDescr[p->mode]);/*mode命令可以设置结果数据的几种输出

格式,这些格式存放在modeDescr数组中*/
    fprintf(p->out,"%9.9s: ", "nullvalue");/*空值显示*/
      output_c_string(p->out, p->nullvalue);/*根据C或TCL引用规则,输出给定的字符串。*/
      fprintf(p->out, "\n");
    fprintf(p->out,"%9.9s: %s\n","output",
            strlen30(p->outfile) ? p->outfile : "stdout");/*标准输出*/
    fprintf(p->out,"%9.9s: ", "separator");
      output_c_string(p->out, p->separator);/*用相应分隔符输出字符串*/
      fprintf(p->out, "\n");
    fprintf(p->out,"%9.9s: %s\n","stats", p->statsOn ? "on" : "off");
    fprintf(p->out,"%9.9s: ","width");
    for (i=0;i<(int)ArraySize(p->colWidth) && p->colWidth[i] != 0;i++) {/*列数和每列列宽不为0*/
      fprintf(p->out,"%d ",p->colWidth[i]);/*输出列宽*/
    }
    fprintf(p->out,"\n");
  }else

  if( c=='s' && strncmp(azArg[0], "stats", n)==0 && nArg>1 && nArg<3 ){/*判断是否输入stats命令*/
    p->statsOn = booleanValue(azArg[1]);/*把azArg[1]的值转化为布尔值*/
  }else

  if( c=='t' && n>1 && strncmp(azArg[0], "tables", n)==0 && nArg<3 ){ /*判断是否输入tables命令*/
    sqlite3_stmt *pStmt;/*声明一个指针*/
    char **azResult;/*二维数组存放结果*/
    int nRow, nAlloc;
    char *zSql = 0;
    int ii;
    open_db(p);/*打开p指向的数据库*/
/* 
**如果nbyte参数小于零，则ZSQL被读取到第一个零终止。
**如果nByte是非负的，那么它是从ZSQL读的最大字节数
**该函数参数列表如下所示：
**int sqlite3_prepare_v2(
  **sqlite3 *db,            /* Database handle */
  **const char *zSql,       /* SQL statement, UTF-8 encoded */
  **int nByte,              /* Maximum length of zSql in bytes. */
  **sqlite3_stmt **ppStmt,  /* OUT: Statement handle */
  **const char **pzTail     /* OUT: Pointer to unused portion of zSql */
**);
*/
    rc = sqlite3_prepare_v2(p->db, "PRAGMA database_list", -1, &pStmt, 0);/*函数返回值赋给rc*/
    if( rc ) return rc;
    zSql = sqlite3_mprintf(/*输出查询结果到内存空间中*/
        "SELECT name FROM sqlite_master"
        " WHERE type IN ('table','view')"
        "   AND name NOT LIKE 'sqlite_%%'"
        "   AND name LIKE ?1");
    while( sqlite3_step(pStmt)==SQLITE_ROW ){/*调用sqlite_step 获取结果集中的一行，并将语句句柄的游标位

置移动到结果集的下一行*/
      const char *zDbName = (const char*)sqlite3_column_text(pStmt, 1);/**zDbName指针指向返回值为字

符型指针的函数空间*/
      if( zDbName==0 || strcmp(zDbName,"main")==0 ) continue;
      if( strcmp(zDbName,"temp")==0 ){
        zSql = sqlite3_mprintf(/*将查询结果写入zSql指向的内存空间中*/
                 "%z UNION ALL "
                 "SELECT 'temp.' || name FROM sqlite_temp_master"
                 " WHERE type IN ('table','view')"/*选择条件为type是'table'或'view'值*/
                 "   AND name NOT LIKE 'sqlite_%%'"/*不是以sqlite_开头的name*/
                 "   AND name LIKE ?1", zSql);
      }else{
        zSql = sqlite3_mprintf(/*输出查询结果到内存空间中*/
                 "%z UNION ALL "
                 "SELECT '%q.' || name FROM \"%w\".sqlite_master"
                 " WHERE type IN ('table','view')"
                 "   AND name NOT LIKE 'sqlite_%%'"
                 "   AND name LIKE ?1", zSql, zDbName, zDbName);
      }
    }
    sqlite3_finalize(pStmt);/*撤销对pStmt的声明*/
    zSql = sqlite3_mprintf("%z ORDER BY 1", zSql);/*排序后输出结果*/
    rc = sqlite3_prepare_v2(p->db, zSql, -1, &pStmt, 0);
    sqlite3_free(zSql);/*释放zSql指向的地址空间*/
    if( rc ) return rc;/*rc不为空，则返回其值*/
    nRow = nAlloc = 0;
    azResult = 0;
    if( nArg>1 ){
      sqlite3_bind_text(pStmt, 1, azArg[1], -1, SQLITE_TRANSIENT);/*作用不明*/
    }else{
      sqlite3_bind_text(pStmt, 1, "%", -1, SQLITE_STATIC);/*作用不明*/
    }
    while( sqlite3_step(pStmt)==SQLITE_ROW ){/*当前操作的返回值为SQLITE_ROW，即表示新行的数据已准备

好待处理*/
      if( nRow>=nAlloc ){
        char **azNew;
        int n = nAlloc*2 + 10;
        azNew = sqlite3_realloc(azResult, sizeof(azResult[0])*n);/*重新分配azResult的内存空间,至少sizeof

(azResult[0])*n个字节*/
        if( azNew==0 ){/*分配失败，写入错误信息*/
          fprintf(stderr, "Error: out of memory\n");/*内存不足*/
          break;
        }
        nAlloc = n;
        azResult = azNew;/*azResult 指向新的内存单元*/
      }
      azResult[nRow] = sqlite3_mprintf("%s", sqlite3_column_text(pStmt, 0));
      if( azResult[nRow] ) nRow++;/*如果未指向空，则遍历下一个字符*/
    }
    sqlite3_finalize(pStmt);/*删除对pStmt的声明*/        
    if( nRow>0 ){
      int len, maxlen = 0;
      int i, j;
      int nPrintCol, nPrintRow;
      for(i=0; i<nRow; i++){
        len = strlen30(azResult[i]);/*将函数返回值赋给len变量*/
        if( len>maxlen ) maxlen = len;
      }
      nPrintCol = 80/(maxlen+2);/*统计打印的列宽度*/
      if( nPrintCol<1 ) nPrintCol = 1;/*不足一行，作一行处理*/
      nPrintRow = (nRow + nPrintCol - 1)/nPrintCol;/*统计打印的行高*/
      for(i=0; i<nPrintRow; i++){
        for(j=i; j<nRow; j+=nPrintRow){
          char *zSp = j<nPrintRow ? "" : "  ";/*是否打印空格*/
          printf("%s%-*s", zSp, maxlen, azResult[j] ? azResult[j] : "");/*输出表格*/
        }
        printf("\n");/*每完成一行，换行处理*/
      }
    }
    for(ii=0; ii<nRow; ii++) sqlite3_free(azResult[ii]);/*释放内存空间*/
    sqlite3_free(azResult);/*释放查询结果的内存空间*/
  }else

  if( c=='t' && n>=8 && strncmp(azArg[0], "testctrl", n)==0 && nArg>=2 ){ /*判断是否输入.testctrl 命令

*/
    static const struct {/*静态常结构体*/
       const char *zCtrlName;   /* 指向常量的指针 */
       int ctrlCode;            /* 声明一个整型代码变量，这些字符串已经宏定义*/
    } aCtrl[] = {/*结构体数组常量，包含两部分结构*/
      { "prng_save",             SQLITE_TESTCTRL_PRNG_SAVE              },
      { "prng_restore",          SQLITE_TESTCTRL_PRNG_RESTORE           },
      { "prng_reset",            SQLITE_TESTCTRL_PRNG_RESET             },
      { "bitvec_test",           SQLITE_TESTCTRL_BITVEC_TEST            },
      { "fault_install",         SQLITE_TESTCTRL_FAULT_INSTALL          },
      { "benign_malloc_hooks",   SQLITE_TESTCTRL_BENIGN_MALLOC_HOOKS    },
      { "pending_byte",          SQLITE_TESTCTRL_PENDING_BYTE           },
      { "assert",                SQLITE_TESTCTRL_ASSERT                 },
      { "always",                SQLITE_TESTCTRL_ALWAYS                 },
      { "reserve",               SQLITE_TESTCTRL_RESERVE                },
      { "optimizations",         SQLITE_TESTCTRL_OPTIMIZATIONS          },
      { "iskeyword",             SQLITE_TESTCTRL_ISKEYWORD              },
      { "scratchmalloc",         SQLITE_TESTCTRL_SCRATCHMALLOC          },
    };
    int testctrl = -1;
    int rc = 0;
    int i, n;
    open_db(p);/*打开数据库*/
    /* 把testctrl文本选项转化为数值*/
    n = strlen30(azArg[1]); /*统计azArg[1]的字符串长度*/
    for(i=0; i<(int)(sizeof(aCtrl)/sizeof(aCtrl[0])); i++){
      if( strncmp(azArg[1], aCtrl[i].zCtrlName, n)==0 ){/*比较两数组内容是否相同*/
        if( testctrl<0 ){
          testctrl = aCtrl[i].ctrlCode;/*把aCtrl[i]的整型代码值赋给testctrl*/
        }else{
          fprintf(stderr, "ambiguous option name: \"%s\"\n", azArg[1]);
          testctrl = -1;
          break;
        }
      }
    }
    if( testctrl<0 ) testctrl = atoi(azArg[1]);/*把字符转换成长整型数赋给testctrl变量。*/
    if( (testctrl<SQLITE_TESTCTRL_FIRST) || (testctrl>SQLITE_TESTCTRL_LAST) ){  /*如果testctrl小于5 或者

大于24。*/
      fprintf(stderr,"Error: invalid testctrl option: %s\n", azArg[1]);/*无效的testctrl选项*/
    }else{
      switch(testctrl){/*依据testctrl的值，选择case分支语句执行*/
        /* sqlite3_test_control(int, db, int) *//*该函数有三个参数，分别是整型、数据库指针、整型*/
        case SQLITE_TESTCTRL_OPTIMIZATIONS:/*#define SQLITE_TESTCTRL_OPTIMIZATIONS  15*/
        case SQLITE_TESTCTRL_RESERVE:          /* #define SQLITE_TESTCTRL_RESERVE  14*/
          if( nArg==3 ){
            int opt = (int)strtol(azArg[2], 0, 0); /*将azArg[2]字符根据按十进制转换成长整型数。同时当遇到不合条

件而终止时则返回0*/      
            rc = sqlite3_test_control(testctrl, p->db, opt);/*返回执行状态*/
            printf("%d (0x%08x)\n", rc, rc);
          } else {
            fprintf(stderr,"Error: testctrl %s takes a single int option\n",/*得到一个整型项*/
                    azArg[1]);
          }
          break;
        /* sqlite3_test_control(int) *//*sqlite3_test_control包含一个整型参数*/
        case SQLITE_TESTCTRL_PRNG_SAVE:/*#define SQLITE_TESTCTRL_PRNG_SAVE  5 */        
        case SQLITE_TESTCTRL_PRNG_RESTORE:  /*#define SQLITE_TESTCTRL_PRNG_RESTORE  6 */         
        case SQLITE_TESTCTRL_PRNG_RESET:/*#define SQLITE_TESTCTRL_PRNG_RESET  7 */   
          if( nArg==2 ){
            rc = sqlite3_test_control(testctrl);/*用于返回SQLite 内部状态*/
            printf("%d (0x%08x)\n", rc, rc);
          } else {
            fprintf(stderr,"Error: testctrl %s takes no options\n", azArg[1]);/*没有得到选项*/
          }
          break;
        /* sqlite3_test_control(int, uint) *//*sqlite3_test_control包含一个整型参数，一个无符号整型*/
        case SQLITE_TESTCTRL_PENDING_BYTE: /*#define SQLITE_TESTCTRL_PENDING_BYTE   11 */      
          if( nArg==3 ){
            unsigned int opt = (unsigned int)atoi(azArg[2]);/*把字符串转换成整型数，再强制转换为无符号整型赋

给opt变量*/        
            rc = sqlite3_test_control(testctrl, opt);/*用于返回SQLite 内部状态*/
            printf("%d (0x%08x)\n", rc, rc);
          } else {
            fprintf(stderr,"Error: testctrl %s takes a single unsigned"/*得到一个无符号整型项*/
                           " int option\n", azArg[1]);
          }
          break;
          
        /* sqlite3_test_control(int, int) *//*sqlite3_test_control包含两个整型参数*/
        case SQLITE_TESTCTRL_ASSERT:/*#define SQLITE_TESTCTRL_ASSERT   12  */            
        case SQLITE_TESTCTRL_ALWAYS: /*#define SQLITE_TESTCTRL_ALWAYS  13  */           
          if( nArg==3 ){
            int opt = atoi(azArg[2]);  /*把字符串转换成整型数，赋值给opt 变量*/    
            rc = sqlite3_test_control(testctrl, opt);
            printf("%d (0x%08x)\n", rc, rc);
          } else {
            fprintf(stderr,"Error: testctrl %s takes a single int option\n",/*得到一个整型项*/
                            azArg[1]);
          }
          break;
/*上述sqlite3_test_control（）接口用于读出的SQLite的内部状态，并植入SQLite的错误信息用于测试目的。
第一个参数是一个操作码，它确定所有的后续参数的个数，意义和操作。*/
        /* sqlite3_test_control(int, char *) *//*sqlite3_test_control包含一个整型参数，一个指向字符型的指针*/
#ifdef SQLITE_N_KEYWORD/*如果宏定义了SQLITE_N_KEYWORD，则执行以下操作*/
        case SQLITE_TESTCTRL_ISKEYWORD:           
          if( nArg==3 ){
            const char *opt = azArg[2];/*指针*opt指向字符串常量azArg[2]*/            
            rc = sqlite3_test_control(testctrl, opt);
            printf("%d (0x%08x)\n", rc, rc);
          } else {
            fprintf(stderr,"Error: testctrl %s takes a single char * option\n",/*得到一个指向字符串的指针*/
                            azArg[1]);
          }
          break;
#endif

        case SQLITE_TESTCTRL_BITVEC_TEST:         
        case SQLITE_TESTCTRL_FAULT_INSTALL:       
        case SQLITE_TESTCTRL_BENIGN_MALLOC_HOOKS: 
        case SQLITE_TESTCTRL_SCRATCHMALLOC:       
        default:
          fprintf(stderr,"Error: CLI support for testctrl %s not implemented\n",/*命令行界面尚未实现对其的支持*/
                  azArg[1]);
          break;
      }
    }
  }else
  if( c=='t' && n>4 && strncmp(azArg[0], "timeout", n)==0 && nArg==2 ){/*判断是否输入.timeout命令

*/
    open_db(p);/*打开数据库*/
    sqlite3_busy_timeout(p->db, atoi(azArg[1]));/*该程序设置一个忙处理handler
当表被锁定时，休眠一个指定的时间量。参数小于或等于零则关闭所有占线处理程序。*/
  }else
    
  if( HAS_TIMER && c=='t' && n>=5 && strncmp(azArg[0], "timer", n)==0{/*判断是否输入.time命令*/
   && nArg==2
  ){
    enableTimer = booleanValue(azArg[1]);/*将azArg[1]转换为布尔值赋值给enableTimer*/
  }else
  
  if( c=='t' && strncmp(azArg[0], "trace", n)==0 && nArg>1 ){/*判断是否输入.trace命令*/
    open_db(p);
    output_file_close(p->traceOut);/*关闭文件*/
    p->traceOut = output_file_open(azArg[1]);/*打开文件*/
#if !defined(SQLITE_OMIT_TRACE) && !defined(SQLITE_OMIT_FLOATING_POINT)
    if( p->traceOut==0 ){
      sqlite3_trace(p->db, 0, 0);/*用于跟踪和分析的SQL语句的执行回调函数。*/
    }else{
      sqlite3_trace(p->db, sql_trace_callback, p->traceOut);/*用于跟踪和分析的SQL语句的执行回调函数。*/
    }
#endif
  }else

  if( c=='v' && strncmp(azArg[0], "version", n)==0 ){/*判断是否输入.version 命令*/
    printf("SQLite %s %s\n" /*extra-version-info*/,
        sqlite3_libversion(), sqlite3_sourceid());/*sqlite3_libversion函数返回一个指向sqlite3_version[]字符串

常量。*/
  }else

  if( c=='v' && strncmp(azArg[0], "vfsname", n)==0 ){/*判断是否输入.vfsname 命令*/
    const char *zDbName = nArg==2 ? azArg[1] : "main";/*如果nArg=2，指针指向常量azArg[1]，否则指向
字符串"main"*/
    char *zVfsName = 0;
    if( p->db ){
      sqlite3_file_control(p->db, zDbName, SQLITE_FCNTL_VFSNAME, &zVfsName);
      if( zVfsName ){
        printf("%s\n", zVfsName);
        sqlite3_free(zVfsName);/*释放zVfsName内存空间*/
      }
    }
  }else
  if( c=='w' && strncmp(azArg[0], "width", n)==0 && nArg>1 ){/*判断是否输入.width命令*/
    int j;
    assert( nArg<=ArraySize(azArg) );/*assert 函数只有在SQLite 被SQLITE_DEBUG 编译时才会启用。*/
    for(j=1; j<nArg && j<ArraySize(p->colWidth); j++){
      p->colWidth[j-1] = atoi(azArg[j]);/*把azArg[j]转化为整型*/
    }
  }else
  {
    fprintf(stderr, "Error: unknown command or invalid arguments: "
      " \"%s\". Enter \".help\" for help\n", azArg[0]);/*不明指令或无效参数*/
    rc = 1;
  }
  return rc;/*返回rc的值*/
}


static int _contains_semicolon(const char *z, int N){/*当分号 出现在字符串z的第N个位置上，函数则返回1。*/
  int i;
  for(i=0; i<N; i++){  if( z[i]==';' ) return 1; }
  return 0;
}


static int _all_whitespace(const char *z){/* 测试行是否为空*/
  for(; *z; z++){
    if( IsSpace(z[0]) ) continue;/*判断z[0]数组是否为空*/
    if( *z=='/' && z[1]=='*' ){/*z指向‘/’并且第二个字符为‘*’*/
      z += 2;/*z=z+2;*/
      while( *z && (*z!='*' || z[1]!='/') ){ z++; }/*z不指向‘*’或者第二个字符不为‘/’*/
      if( *z==0 ) return 0;
      z++;
      continue;
    }
    if( *z=='-' && z[1]=='-' ){/*z指向‘-’并且第二个字符为‘-’*/
      z += 2;
      while( *z && *z!='\n' ){ z++; }/*指针z不指向空或字符串结尾*/
      if( *z==0 ) return 1;
      continue;
    }
    return 0;
  }
  return 1;
}

/*
如果键入的是一个SQL命令结尾，其他不是一个分号，则返回TRUE。
在SQL Server风格的“go”命令被理解为是Oracle“/”。
*/
static int _is_command_terminator(const char *zLine){
  while( IsSpace(zLine[0]) ){ zLine++; };
  if( zLine[0]=='/' && _all_whitespace(&zLine[1]) ){/*数组开始为"/",之后为空*/
    return 1;  /* Oracle */
  }
  if( ToLower(zLine[0])=='g' && ToLower(zLine[1])=='o' /*数组开始为"go",之后为空*/
         && _all_whitespace(&zLine[2]) ){
    return 1;  /* SQL Server */
  }
  return 0;
}

/*如果zSql是一个完整的SQL语句，返回true；
如果它在一个字符串或C风格注释的中间结束，返回false。*/
static int _is_complete(char *zSql, int nSql){
  int rc;
  if( zSql==0 ) return 1;
  zSql[nSql] = ';';
  zSql[nSql+1] = 0;
  rc = sqlite3_complete(zSql);
  zSql[nSql] = 0;
  return rc;
}

/*从* in 中读取输入并处理。
如果* in==0，则发生交互- 用户键入内容。否则 从一个文件或设备输入。
只有当输入是交互式的，发出的提示和历史记录才会被保存。
一个中断信号将导致该程序立即退出，除非输入是交互式的。
返回错误的数量。*/
static int process_input(struct callback_data *p, FILE *in){
  char *zLine = 0;
  char *zSql = 0;
  int nSql = 0;
  int nSqlPrior = 0;
  char *zErrMsg;
  int rc;
  int errCnt = 0;
  int lineno = 0;
  int startline = 0;

  while( errCnt==0 || !bail_on_error || (in==0 && stdin_is_interactive) ){
    fflush(p->out);/*清除读写缓冲区，需要立即把输出缓冲区的数据进行物理写入时*/
    free(zLine);/*释放zLine内存空间*/
    zLine = one_input_line(zSql, in);
    if( zLine==0 ){
      /* End of input */
      if( stdin_is_interactive ) printf("\n");/*交互式标准输入以换行结束*/
      break;
    }
    if( seenInterrupt ){/*中断信息被收到，则其值为true*/
      if( in!=0 ) break;
      seenInterrupt = 0;
    }
    lineno++;
    if( (zSql==0 || zSql[0]==0) && _all_whitespace(zLine) ) continue;
    if( zLine && zLine[0]=='.' && nSql==0 ){
      if( p->echoOn ) printf("%s\n", zLine);/*执行回显操作*/
      rc = do_meta_command(zLine, p);/*返回执行状态给rc*/
      if( rc==2 ){ /* exit requested */
        break;
      }else if( rc ){/*如果rc不为0，则错误数量加1*/
        errCnt++;
      }
      continue;
    }
    if( _is_command_terminator(zLine) && _is_complete(zSql, nSql) ){
      memcpy(zLine,";",2);/*从zLine所指的内存地址的起始位置开始拷贝2个字节到字符串中。*/
    }
    nSqlPrior = nSql;
    if( zSql==0 ){
      int i;
      for(i=0; zLine[i] && IsSpace(zLine[i]); i++){}
      if( zLine[i]!=0 ){
        nSql = strlen30(zLine);/*统计zLine字符串长度*/
        zSql = malloc( nSql+3 );/*为zSql动态分配内存空间*/
        if( zSql==0 ){
          fprintf(stderr, "Error: out of memory\n");
          exit(1);
        }
        memcpy(zSql, zLine, nSql+1);/*从zLine所指的内存地址的起始位置开始拷贝2个字节到字符串中。*/
        startline = lineno;
      }
    }else{
      int len = strlen30(zLine);
      zSql = realloc( zSql, nSql + len + 4 );/*重新分配内存空间，如果重新分配成功则返回指向被分配内存的指针，否则返回空指针NULL。*/
      if( zSql==0 ){
        fprintf(stderr,"Error: out of memory\n");
        exit(1);
      }
      zSql[nSql++] = '\n';
      memcpy(&zSql[nSql], zLine, len+1);/*从zLine所指的内存地址的起始位置开始拷贝2个字节到字符串中。*/
      nSql += len;/* nSql自加len个值*/
    }
    if( zSql && _contains_semicolon(&zSql[nSqlPrior], nSql-nSqlPrior)
                && sqlite3_complete(zSql) ){
      p->cnt = 0;/*初始化数据*/
      open_db(p);
      BEGIN_TIMER;/*开启定时器*/
      rc = shell_exec(p->db, zSql, shell_callback, p, &zErrMsg);/*与sqlite3_exec()函数非常相似*/
      END_TIMER;/*关闭定时器*/
      if( rc || zErrMsg ){
        char zPrefix[100];/*声明一个前缀数组*/
        if( in!=0 || !stdin_is_interactive ){
          sqlite3_snprintf(sizeof(zPrefix), zPrefix, 
                           "Error: near line %d:", startline);
        }else{
          sqlite3_snprintf(sizeof(zPrefix), zPrefix, "Error:");
        }
        if( zErrMsg!=0 ){
          fprintf(stderr, "%s %s\n", zPrefix, zErrMsg);
          sqlite3_free(zErrMsg);/*释放zErrMsg内存空间*/
          zErrMsg = 0;
        }else{
          fprintf(stderr, "%s %s\n", zPrefix, sqlite3_errmsg(p->db));
        }
        errCnt++;
      }
      free(zSql);
      zSql = 0;
      nSql = 0;
    }
  }
  if( zSql ){
    if( !_all_whitespace(zSql) ){
      fprintf(stderr, "Error: incomplete SQL: %s\n", zSql);
    }
    free(zSql);
  }
  free(zLine);
  return errCnt;
}

/*返回路径是用户的主目录。返回 0 时表示存在某种类型的错误。*/
static char *find_home_dir(void){
  static char *home_dir = NULL;
  if( home_dir ) return home_dir;

#if !defined(_WIN32) && !defined(WIN32) && !defined(_WIN32_WCE) && !defined(__RTP__) && !

defined(_WRS_KERNEL)/*条件编译指令，如果满足要求的编译环境，则执行下面的程序段*/
  {
    struct passwd *pwent;
    uid_t uid = getuid();/* 返回一个调用程序的真实用户ID*/
    if( (pwent=getpwuid(uid)) != NULL) {
      home_dir = pwent->pw_dir;
    }
  }
#endif

#if defined(_WIN32_WCE)
  /* Windows CE (arm-wince-mingw32ce-gcc) does not provide getenv()
   */
  home_dir = "/";
#else

#if defined(_WIN32) || defined(WIN32)
  if (!home_dir) {
    home_dir = getenv("USERPROFILE");/*获取USERPROFILE环境变量的值*/
  }
#endif

  if (!home_dir) {
    home_dir = getenv("HOME");/*获取HOME环境变量的值*/
  }

#if defined(_WIN32) || defined(WIN32)
  if (!home_dir) {
    char *zDrive, *zPath;
    int n;
    zDrive = getenv("HOMEDRIVE");/*获取HOMEDRIVE环境变量的值*/
    zPath = getenv("HOMEPATH");/*获取HOMEPATH环境变量的值*/
    if( zDrive && zPath ){
      n = strlen30(zDrive) + strlen30(zPath) + 1;
      home_dir = malloc( n );/*home_dir指向n个字节的内存空间*/
      if( home_dir==0 ) return 0;
      sqlite3_snprintf(n, home_dir, "%s%s", zDrive, zPath);
      return home_dir;
    }
    home_dir = "c:\\";
  }
#endif

#endif /* !_WIN32_WCE */

  if( home_dir ){/*home_dir指向的内存空间不为空*/
    int n = strlen30(home_dir) + 1;
    char *z = malloc( n );/* z指向n 个字节的内存空间*/
    if( z ) memcpy(z, home_dir, n);
    home_dir = z;
  }

  return home_dir;
}

/*
从sqliterc_override给出的文件读取输入。
或者如果该参数为NULL，则从~/.sqliterc中输入
返回错误的数量。
*/
static int process_sqliterc(/*返回值为静态整型*/
  struct callback_data *p,        /* Configuration data */
  const char *sqliterc_override   /* Name of config file. NULL to use default */
){
  char *home_dir = NULL;
  const char *sqliterc = sqliterc_override;/*指向常字符的指针*/
  char *zBuf = 0;
  FILE *in = NULL;
  int rc = 0;

  if (sqliterc == NULL) {
    home_dir = find_home_dir();
    if( home_dir==0 ){
#if !defined(__RTP__) && !defined(_WRS_KERNEL)
      fprintf(stderr,"%s: Error: cannot locate your home directory\n", Argv0);/*把Argv0中的错误信息输出到

stderr 文件中*/
#endif
      return 1;
    }
    sqlite3_initialize();/*初始化SQLite 数据库*/
    zBuf = sqlite3_mprintf("%s/.sqliterc",home_dir);
    sqliterc = zBuf;
  }
  in = fopen(sqliterc,"rb");/*判断是否顺利打开文件*/
  if( in ){
    if( stdin_is_interactive ){/*判断标准输入是否为交互式的*/
      fprintf(stderr,"-- Loading resources from %s\n",sqliterc);
    }
    rc = process_input(p,in);
    fclose(in);/*关闭in 指针指向的文件*/
  }
  sqlite3_free(zBuf);
  return rc;
}

/*
** Show available command line options
*/
static const char zOptions[] = /*定义静态常字符数组*/
  "   -bail                stop after hitting an error\n"//遇到错误即停止
  "   -batch               force batch I/O\n"//批处理I/O
  "   -column              set output mode to 'column'\n"//输出模式设置为按列分开
  "   -cmd command         run \"command\" before reading stdin\n"
  "   -csv                 set output mode to 'csv'\n"//输出格式设置为csv
  "   -echo                print commands before execution\n"//回显设置
  "   -init filename       read/process named file\n"//初始化文件名
  "   -[no]header          turn headers on or off\n"//是否显示表头
  "   -help                show this message\n"//显示帮助信息
  "   -html                set output mode to HTML\n"//输出模式设置为HTML
  "   -interactive         force interactive I/O\n"
  "   -line                set output mode to 'line'\n"
  "   -list                set output mode to 'list'\n"
#ifdef SQLITE_ENABLE_MULTIPLEX
  "   -multiplex           enable the multiplexor VFS\n"
#endif
  "   -nullvalue 'text'    set text string for NULL values\n"//将文本字符串设置为空值
  "   -separator 'x'       set output field separator (|)\n"//设置分隔符
  "   -stats               print memory stats before each finalize\n"
  "   -version             show SQLite version\n"
  "   -vfs NAME            use NAME as the default VFS\n"//默认VFS名称
#ifdef SQLITE_ENABLE_VFSTRACE
  "   -vfstrace            enable tracing of all VFS calls\n"
#endif
;
static void usage(int showDetail){
  fprintf(stderr,
      "Usage: %s [OPTIONS] FILENAME [SQL]\n"  
      "FILENAME is the name of an SQLite database. A new database is created\n"
      "if the file does not previously exist.\n", Argv0);
  if( showDetail ){
    fprintf(stderr, "OPTIONS include:\n%s", zOptions);//把zOptions 数组中的命令按格式要求输出到stderr文

件中
  }else{
    fprintf(stderr, "Use the -help option for additional information\n");//使用help命令得到更多信息
  }
  exit(1);
}

/*初始化数据的状态信息*/
static void main_init(struct callback_data *data) {/*其参数为结构体回显指针*/
  memset(data, 0, sizeof(*data));//清零 sizeof(*data),指针data所占内存的字节数 4
  data->mode = MODE_List;//设置数据库的输出模式为list
  memcpy(data->separator,"|", 2);//从源"|"所指的内存地址的起始位置开始拷贝2个字节到目标data->separator所指的内存地址的起始位置中
  data->showHeader = 0;//不显示列名
  sqlite3_config(SQLITE_CONFIG_URI, 1);//非零启用，所有文件名传递给sqlite3_open(),sqlite3_open_v2(),sqlite3_open16()
  /*sqlite3_config() 用于更改全局变量让SQLite 适应应用的具体需要。
  它支持少数的应用不常见的需求。*/
  sqlite3_config(SQLITE_CONFIG_LOG, shellLog, data);
  sqlite3_snprintf(sizeof(mainPrompt), mainPrompt,"sqlite> ");//char mainPrompt[20],mainPrompt的初始值为sqlite>.最多从源串中拷贝sizeof(mainPrompt)－1个字符到目标串中，然后再在后面加一个0。
  sqlite3_snprintf(sizeof(continuePrompt), continuePrompt,"   ...> ");//延续提示continuePrompt[20]，continuePrompt的初始值为...>
  sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);//这个选项设置单线程的线程模式。换句话说,它禁用所有互斥锁并将SQLite数据放入一个模式,它只能由一个线程使用。
}

/*
**程序的main()函数在shell.c的尾部，
**简化后的main()函数的执行过程主要分为5步：
**1. 设置回显参数 
**2. 取数据库文件名 
**3. 打开数据库  
**4. 循环处理SQL命令 
**5. 关闭数据库
*/
int main(int argc, char **argv){
  char *zErrMsg = 0;/*声明一个存放错误信息的指针*/
  struct callback_data data;//声明回显参数
  const char *zInitFile = 0;//文件初始化
  char *zFirstCmd = 0;//接收命令
  int i;
  int rc = 0; //一个标志位

  if( strcmp(sqlite3_sourceid(),SQLITE_SOURCE_ID)!=0 ){/*比较数据库版本号是否相同*/
    fprintf(stderr, "SQLite header and source version mismatch\n%s\n%s\n",//数据库版本不匹配
            sqlite3_sourceid(), SQLITE_SOURCE_ID);
    exit(1);
  }
  Argv0 = argv[0];// argv[]是argc个参数，其中第0个参数是程序的全名，以后的参数 
  main_init(&data);//设置默认的回显形式
  stdin_is_interactive = isatty(0);//如果返回值为1则可以进行交互式输入，否则交换输入是管道或者文件，isatty函数判断其是不是设备

  /* 完成以前，确保 有一个有效的信号处理程序 */
#ifdef SIGINT
  signal(SIGINT, interrupt_handler);//用户按下Ctrl-C键,发出中断信号
  //signal函数的原型void (*signal(int signo, void (*handler)(int)))(int);
  //当随后出现信号当随后出现信号SIGINT时，就中断正在执行的操作，转而执行信号处理函数interrupt_handler(SIGINT)。如果从信号处理程序中返回，则从中断的位置继续执行。
#endif

 /* 
**通过命令行参数定位数据库文件名，初始化文件名，
**空闲的malloc堆的大小，
**和执行第一条命令。
 */
  for(i=1; i<argc-1; i++){
    char *z;
    if( argv[i][0]!='-' ) break;// 如果某行的第一个字符不是'-' 则跳出当前循环。
    z = argv[i];//指针Z是行指针
    if( z[1]=='-' ) z++;
    if( strcmp(z,"-separator")==0//判断输入的命令中是否有-separator||-nullvalue||-cmd
     || strcmp(z,"-nullvalue")==0
     || strcmp(z,"-cmd")==0
    ){//若与上述字符串中的某个匹配，则执行以下程序段
      i++;
    }else if( strcmp(z,"-init")==0 ){/*比较字符串*/
      i++;
      zInitFile = argv[i];
	  
  /* 
**第二次做实参处理后,
**需要检查批处理模式,
**以便我们能够避免打印信息（就像来自sqliterc 进程）。
   */
    }else if( strcmp(z,"-batch")==0 ){/*比较字符串*/
      stdin_is_interactive = 0;//文件或者管道进行交互
    }else if( strcmp(z,"-heap")==0 ){/*比较字符串*/
#if defined(SQLITE_ENABLE_MEMSYS3) || defined(SQLITE_ENABLE_MEMSYS5)
      int j, c;
      const char *zSize;
      sqlite3_int64 szHeap;

      zSize = argv[++i];
      szHeap = atoi(zSize);
      for(j=0; (c = zSize[j])!=0; j++){
        if( c=='M' ){ szHeap *= 1000000; break; }
        if( c=='K' ){ szHeap *= 1000; break; }
        if( c=='G' ){ szHeap *= 1000000000; break; }
      }
      if( szHeap>0x7fff0000 ) szHeap = 0x7fff0000;
    /*sqlite3_config用于改变SQLite 的全局配置以满足
** 应用的具体需要。*/
      sqlite3_config(SQLITE_CONFIG_HEAP, malloc((int)szHeap), (int)szHeap, 64);
	
#endif
#ifdef SQLITE_ENABLE_VFSTRACE
    }else if( strcmp(z,"-vfstrace")==0 ){/*比较字符串*/
      extern int vfstrace_register(//声明外部函数vfstrace_register
         const char *zTraceName,/*声明一个指向常字符型的指针*/
         const char *zOldVfsName,
         int (*xOut)(const char*,void*),
         void *pOutArg,
         int makeDefault
      );
      vfstrace_register("trace",0,(int(*)(const char*,void*))fputs,stderr,1);
#endif
#ifdef SQLITE_ENABLE_MULTIPLEX
    }else if( strcmp(z,"-multiplex")==0 ){/*比较字符串*/
      extern int sqlite3_multiple_initialize(const char*,int);//声明外部函数vfstrace_register
      sqlite3_multiplex_initialize(0, 1);//多重初始化操作
#endif
    }else if( strcmp(z,"-vfs")==0 ){
      sqlite3_vfs *pVfs = sqlite3_vfs_find(argv[++i]);
      if( pVfs ){
		  sqlite3_vfs_register(pVfs, 1);	//使用sqlite3_vfs_register()接口,通过注册或更改默认VFS re - registering VFS 
      }else{
        fprintf(stderr, "no such VFS: \"%s\"\n", argv[i]);
        exit(1);
      }
    }
  }
  if( i<argc ){
    data.zDbFilename = argv[i++];//数据库文件名
  }else{
#ifndef SQLITE_OMIT_MEMORYDB
    data.zDbFilename = ":memory:";//如果没有给出数据库名则选用默认的数据库:memory:
#else
    data.zDbFilename = 0;
#endif
  }
  if( i<argc ){
    zFirstCmd = argv[i++];//将命令行的命令赋值给zFirstCmd 
  }
  if( i<argc ){
    fprintf(stderr,"%s: Error: too many options: \"%s\"\n", Argv0, argv[i]);//输出错误信息
    fprintf(stderr,"Use -help for a list of options.\n");
    return 1;
  }
  data.out = stdout;	//它就是一个文件，而这个文件和标准输出设备(屏幕)建立了某种关联，当数据写到这个文件里面的时候，屏幕就会通过既定的方式把你写进去的东西显示出来

#ifdef SQLITE_OMIT_MEMORYDB
  if( data.zDbFilename==0 ){//data.zDbFilename的值为0，则执行如下操作
   /*把Argv0中的信息按格式要求写入stderr*/
    fprintf(stderr,"%s: Error: no database filename specified\n", Argv0);
    return 1;
  }
#endif

  /* 
**如果数据库文件已经存在则打开它。
**如果该文件不存在，延迟打开它。
**防止空数据库文件在用户错误输入数据库名称参数的时候被创建。
  */
  if( access(data.zDbFilename, 0)==0 ){//access函数确定文件或文件夹的访问权限。即，检查某个文件的存取方式，比如说是只读方式、只写方式等。
			                           //如果指定的存取方式有效，则函数返回0，否则函数返回-1。
    open_db(&data);//打开数据库
  }

  /*
**处理该初始化文件，如果它存在。
**如果命令行上没有给出-init 选项，
**则寻找一个名为~/.sqliterc 的文件，并尝试进行处理。
  */
  rc = process_sqliterc(&data,zInitFile);
  if( rc>0 ){
    return rc;
  }

  /* 
**通过命令行参数和设置选项进行第二次操作。
**第二 次延迟直到初始化文件被处理之后，
**以便命令行参数覆盖初始化文件设置。
  */
  for(i=1; i<argc && argv[i][0]=='-'; i++){
    char *z = argv[i];
    if( z[1]=='-' ){ z++; }
    if( strcmp(z,"-init")==0 ){
      i++;
    }else if( strcmp(z,"-html")==0 ){
      data.mode = MODE_Html;
    }else if( strcmp(z,"-list")==0 ){
      data.mode = MODE_List;
    }else if( strcmp(z,"-line")==0 ){
      data.mode = MODE_Line;
    }else if( strcmp(z,"-column")==0 ){
      data.mode = MODE_Column;
    }else if( strcmp(z,"-csv")==0 ){
      data.mode = MODE_Csv;
      memcpy(data.separator,",",2);
    }else if( strcmp(z,"-separator")==0 ){
      i++;
      if(i>=argc){
        fprintf(stderr,"%s: Error: missing argument for option: %s\n",
                        Argv0, z);
        fprintf(stderr,"Use -help for a list of options.\n");
        return 1;
      }
      sqlite3_snprintf(sizeof(data.separator), data.separator,
                       "%.*s",(int)sizeof(data.separator)-1,argv[i]);
    }else if( strcmp(z,"-nullvalue")==0 ){
      i++;
      if(i>=argc){
        fprintf(stderr,"%s: Error: missing argument for option: %s\n",
                        Argv0, z);
        fprintf(stderr,"Use -help for a list of options.\n");
        return 1;
      }
      sqlite3_snprintf(sizeof(data.nullvalue), data.nullvalue,
                       "%.*s",(int)sizeof(data.nullvalue)-1,argv[i]);
    }else if( strcmp(z,"-header")==0 ){
      data.showHeader = 1;
    }else if( strcmp(z,"-noheader")==0 ){
      data.showHeader = 0;
    }else if( strcmp(z,"-echo")==0 ){
      data.echoOn = 1;
    }else if( strcmp(z,"-stats")==0 ){
      data.statsOn = 1;
    }else if( strcmp(z,"-bail")==0 ){
      bail_on_error = 1;//如果没有交互，命令的执行将停在一个错误状态，
    }else if( strcmp(z,"-version")==0 ){
      printf("%s %s\n", sqlite3_libversion(), sqlite3_sourceid());
      return 0;
    }else if( strcmp(z,"-interactive")==0 ){
      stdin_is_interactive = 1;
    }else if( strcmp(z,"-batch")==0 ){
      stdin_is_interactive = 0;
    }else if( strcmp(z,"-heap")==0 ){
      i++;
    }else if( strcmp(z,"-vfs")==0 ){
      i++;
#ifdef SQLITE_ENABLE_VFSTRACE
    }else if( strcmp(z,"-vfstrace")==0 ){
      i++;
#endif
#ifdef SQLITE_ENABLE_MULTIPLEX
    }else if( strcmp(z,"-multiplex")==0 ){
      i++;
#endif
    }else if( strcmp(z,"-help")==0 ){
      usage(1);
    }else if( strcmp(z,"-cmd")==0 ){
      if( i==argc-1 ) break;
      i++;
      z = argv[i];
      if( z[0]=='.' ){
        rc = do_meta_command(z, &data);//调用这个程序来处理z 指定的命令。
        if( rc && bail_on_error ) return rc;
      }else{
        open_db(&data);
        rc = shell_exec(data.db, z, shell_callback, &data, &zErrMsg);
        if( zErrMsg!=0 ){
          fprintf(stderr,"Error: %s\n", zErrMsg);
          if( bail_on_error ) return rc!=0 ? rc : 1;
        }else if( rc!=0 ){
          fprintf(stderr,"Error: unable to process SQL \"%s\"\n", z);
          if( bail_on_error ) return rc;
        }
      }
    }else{
      fprintf(stderr,"%s: Error: unknown option: %s\n", Argv0, z);
      fprintf(stderr,"Use -help for a list of options.\n");
      return 1;
    }
  }

  if( zFirstCmd ){
    /* 只运行和数据库名称匹配的命令
    */
    if( zFirstCmd[0]=='.' ){
      rc = do_meta_command(zFirstCmd, &data);//调用这个程序来处理zFirstCmd指定的命令。
    }else{
      open_db(&data);
    /*通过提供的回调函数，根据当前的模式打印出相应结果*/
      rc = shell_exec(data.db, zFirstCmd, shell_callback, &data, &zErrMsg);

      if( zErrMsg!=0 ){
        fprintf(stderr,"Error: %s\n", zErrMsg);
        return rc!=0 ? rc : 1;
      }else if( rc!=0 ){
        fprintf(stderr,"Error: unable to process SQL \"%s\"\n", zFirstCmd);
        return rc;
      }
    }
  }else{
    /*运行从标准输入接收到的命令*/
    if( stdin_is_interactive ){//交互式输入
      char *zHome;
      char *zHistory = 0;
      int nHistory;
      printf(
        "SQLite version %s %.19s\n" /*extra-version-info*/
        "Enter \".help\" for instructions\n"
        "Enter SQL statements terminated with a \";\"\n",
        sqlite3_libversion(), sqlite3_sourceid()
      );
      zHome = find_home_dir();//返回用户主目录
      if( zHome ){
        nHistory = strlen30(zHome) + 20;
        if( (zHistory = malloc(nHistory))!=0 ){
       /*与snprintf函数类似，其结果被写入缓冲区作为第二个参数，
      **缓冲区大小则由第一个参数给出 。*/
          sqlite3_snprintf(nHistory, zHistory,"%s/.sqlite_history", zHome);
        }
      }
#if defined(HAVE_READLINE) && HAVE_READLINE==1//判断两个宏是否已经定义
      if( zHistory ) read_history(zHistory);//得到zHistory 参数值
#endif
      rc = process_input(&data, 0);
      if( zHistory ){
        stifle_history(100);
        write_history(zHistory);
        free(zHistory);//释放zHistory空间
      }
    }else{
      rc = process_input(&data, stdin);//把标准输入的错误数量返回给rc
    }
  }
  set_table_name(&data, 0);//设置表名
  if( data.db ){
    sqlite3_close(data.db);//关闭数据库
  }
  return rc;
