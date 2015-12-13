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
//����ͷ�ļ�
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
//�в����ĺ궨��
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
static int enableTimer = 0;  // ��ʼ���˼���ʱ��  

/* ctype macros that work with signed characters */
#define IsSpace(X)  isspace((unsigned char)X)
#define IsDigit(X)  isdigit((unsigned char)X)
#define ToLower(X)  (char)tolower((unsigned char)X)

#if !defined(_WIN32) && !defined(WIN32) && !defined(_WRS_KERNEL)
#include <sys/time.h>
#include <sys/resource.h>

/* Saved resource information for the beginning of an operation */
static struct rusage sBegin;    //���忪ʼ

/*
** Begin timing an operation   
*/
static void beginTimer(void){  //��ʾ��ʼ��ʱ�亯��
  if( enableTimer ){
    getrusage(RUSAGE_SELF, &sBegin);
  }
}

/* Return the difference of two time_structs in seconds */
static double timeDiff(struct timeval *pStart, struct timeval *pEnd){  // �йط����û�ʱ���ϵͳʱ��֮���

����
  return (pEnd->tv_usec - pStart->tv_usec)*0.000001 + 
         (double)(pEnd->tv_sec - pStart->tv_sec);
}

/*
** Print the timing results. 
*/
static void endTimer(void){   //��ʾ��ӡ�����ʱ��
  if( enableTimer ){
    struct rusage sEnd;   //����
    getrusage(RUSAGE_SELF, &sEnd);
    printf("CPU Time: user %f sys %f\n",
       timeDiff(&sBegin.ru_utime, &sEnd.ru_utime),
       timeDiff(&sBegin.ru_stime, &sEnd.ru_stime));
  }
}

#define BEGIN_TIMER beginTimer()  //�궨���˿�ʼ�ͽ���ʱ��ĺ������������ʹ��
#define END_TIMER endTimer()
#define HAS_TIMER 1

#elif (defined(_WIN32) || defined(WIN32))

#include <windows.h>

/* Saved resource information for the beginning of an operation */
static HANDLE hProcess;
static FILETIME ftKernelBegin; //�ں˿�ʼʱ��
static FILETIME ftUserBegin;  //�û���ʼʱ��
typedef BOOL (WINAPI *GETPROCTIMES)(HANDLE, LPFILETIME, LPFILETIME, LPFILETIME, LPFILETIME);
static GETPROCTIMES getProcessTimesAddr = NULL;  //��ʾ�õ�����ʱ��

/*
** Check to see if we have timer support.  Return 1 if necessary
** support found (or found previously).
*/
static int hasTimer(void){    //��ʱ��
  if( getProcessTimesAddr ){   //���֧�ַ���1
    return 1;
  } else {
    /* GetProcessTimes() isn't supported in WIN95 and some other Windows versions.
    ** See if the version we are running on has it, and if it does, save off
    ** a pointer to it and the current process handle.
    */
    hProcess = GetCurrentProcess();
    if( hProcess ){
      HINSTANCE hinstLib = LoadLibrary(TEXT("Kernel32.dll"));  //���ض�̬���ӿ⡣֮����Է��ʿ��ڵ���Դ  
                                                               /*kernel32.dll��Windows 9x/Me�� �ǳ���Ҫ��32λ ��̬���ӿ��ļ�
�������ں˼��ļ�����������ϵͳ���ڴ�������ݵ���������������жϴ���
                                                                ** ��Windows����ʱ��kernel32.dll��פ�����ڴ����ض���д��������
��ʹ��ĳ����޷�ռ������ڴ�����*/  								

			 ** ��פ�����ڴ����ض���д��������ʹ��ĳ����޷�ռ������ڴ�����*/  
       if( NULL != hinstLib ){
        getProcessTimesAddr = (GETPROCTIMES) GetProcAddress(hinstLib, "GetProcessTimes");  //��ȡ��

̬���ӿ���Ĺ��ܺ�����ַ
        if( NULL != getProcessTimesAddr ){  //�����ȡ�ɹ�������1
          return 1;
        }
        FreeLibrary(hinstLib);  //�ͷŶ�̬���ӿ⡣
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
    FILETIME ftCreation, ftExit; //�ֱ����˱�ʾʱ����Ϣ�Ľ����ͽ����ı���
    getProcessTimesAddr(hProcess, &ftCreation, &ftExit, &ftKernelBegin, &ftUserBegin);
  }
}

/* Return the difference of two FILETIME structs in seconds */
static double timeDiff(FILETIME *pStart, FILETIME *pEnd){ // �йط��ؿ�ʼʱ���ϵͳʱ��֮��Ĳ���
  sqlite_int64 i64Start = *((sqlite_int64 *) pStart);  //����һ���µ����� sqlite_int64
  sqlite_int64 i64End = *((sqlite_int64 *) pEnd);
  return (double) ((i64End - i64Start) / 10000000.0);
}

/*
** Print the timing results.
*/
static void endTimer(void){  //��ʾ��ӡ�����ʱ��
  if( enableTimer && getProcessTimesAddr){  //����ɹ����ҵõ�������ַ
    FILETIME ftCreation, ftExit, ftKernelEnd, ftUserEnd;
    getProcessTimesAddr(hProcess, &ftCreation, &ftExit, &ftKernelEnd, &ftUserEnd);
    printf("CPU Time: user %f sys %f\n",
       timeDiff(&ftUserBegin, &ftUserEnd),
       timeDiff(&ftKernelBegin, &ftKernelEnd));
  }
}

#define BEGIN_TIMER beginTimer()
#define END_TIMER endTimer()
#define HAS_TIMER hasTimer()  //�����ϣʱ��

#else
#define BEGIN_TIMER 
#define END_TIMER
#define HAS_TIMER 0
#endif

/*
** Used to prevent warnings about unused parameters
*/
#define UNUSED_PARAMETER(x) (void)(x)  //Ϊ�˷�ֹ��ʹ�õĲ����ľ���

/*
** If the following flag is set, then command execution stops
** at an error if we are not interactive.
*/
static int bail_on_error = 0;//��������ı��,�������û�н�������ִ�оͻ���Ϊһ�������ֹͣ

/*
** Threat stdin as an interactive input if the following variable
** is true.  Otherwise, assume stdin is connected to a file or pipe.
*/
static int stdin_is_interactive = 1; //������������true�����н���ʽ���룬���򣬼��轻��ʽ���������ӵ��ļ�

���߹ܵ��ġ�

/*
** The following is the open SQLite database.  We make a pointer
** to this database a static variable so that it can be accessed
** by the SIGINT handler to interrupt database processing.
*/
static sqlite3 *db = 0; //��ʾ�򿪵����ݿ⣬����һ����̬��ָ����������Ǿ��ܹ�ͨ���ж��źſ������ж����ݿ�

����

/*
** True if an interrupt (Control-C) has been received.
*/
static volatile int seenInterrupt = 0;   //��������жϵı���������յ��ж��źţ��ͽ�������ֵΪ 1

/*
** This is the name of our program. It is set in main(), used
** in a number of other places, mostly for error messages.
*/
static char *Argv0;  //��ʹ����main���������ͺܶ��������ϣ���ʾ��������֣���������и��౻ʹ���ڴ�����

Ϣ��磺fprintf(stderr,"%s: Error: no database filename specified\n", Argv0);

/*
** Prompt strings. Initialized in main. Settable with
**   .prompt main continue 
*/
//��ʾ�ַ�������main�����г�ʼ������.prompt main continue �趨
static char mainPrompt[20];     /* First line prompt. default: "sqlite> "*/
static char continuePrompt[20]; /* Continuation prompt. default: "   ...> " */

/*
** Write I/O traces to the following stream.
*/
#ifdef SQLITE_ENABLE_IOTRACE
static FILE *iotrace = 0;  //��ʾ���������������
#endif

/*
** This routine works like printf in that its first argument is a
** format string and subsequent arguments are values to be substituted
** in place of % fields.  The result of formatting this string
** is written to iotrace.
*/ //���ʱ����һ��������һ����ʽ�ַ����������������%+�ֶεĸ�ʽ��������������ʾ�����������
#ifdef SQLITE_ENABLE_IOTRACE
static void iotracePrintf(const char *zFormat, ...){ //��һ������zFormat�̶�����,������Ĳ����ĸ���������

�ǿɱ�ģ��������㡰����������ռλ����
  va_list ap;  //��������Ǵ洢������ַ��ָ��.��Ϊ�õ������ĵ�ַ֮���ٽ�ϲ��������ͣ����ܵõ�������ֵ��
  char *z;
  if( iotrace==0 ) return;  //û�������������������
  va_start(ap, zFormat); //�Թ̶������ĵ�ַΪ���ȷ����ε��ڴ���ʼ��ַ
  z = sqlite3_vmprintf(zFormat, ap);//�������ص��ַ�����д��ͨ�� malloc() �õ����ڴ�ռ䣬��ˣ���Զ����

�����ڴ�й¶�����⡣���ص��ַ���Ҫ��sqlite3_free()�ͷſռ䡣
  va_end(ap); //����
  fprintf(iotrace, "%s", z); // ��ʽ����� fprintf(�ļ�ָ��,��ʽ�ַ���,�������)
  sqlite3_free(z);  //�ͷſռ�
}
#endif


/*
** Determines if a string is a number of not.  //����кܶ����������ֹ,zΪ�õ����ַ���
*/
static int isNumber(const char *z, int *realnum){
  if( *z=='-' || *z=='+' ) z++;  //�ж�����
  if( !IsDigit(*z) ){ //�ж��Ƿ������֣�������ǣ�����0
    return 0;
  }
  z++;       //ָ�����һλ
  if( realnum ) *realnum = 0; //  �ַ�����ʵ�ʳ���
  while( IsDigit(*z) ){ z++; } //����������֣�ָ�����һλ
  if( *z=='.' ){   //�ж��Ƿ���С��
    z++;
    if( !IsDigit(*z) ) return 0; //������������֣�����0
    while( IsDigit(*z) ){ z++; }
    if( realnum ) *realnum = 1; //
  }
  if( *z=='e' || *z=='E' ){ // �ж��Ƿ���ָ��
    z++;
    if( *z=='+' || *z=='-' ) z++; //ָ��������
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
*/    //һ��ȫ�ֵ�charָ�������һ��SQL������һ��SQL����з�������ǰ��ֵ���������֮ǰʹ��

sqlite_exec_printf() AP����һ���ַ�����ΪSQL��䣬sqlite3����ȷ�ķ�����ʹ��bind API,����shell�����ڻص�

ģʽ,��������ɴ����Ĺ���
static const char *zShellStatic = 0;
static void shellstaticFunc(   //
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  assert( 0==argc );
  assert( zShellStatic );
  UNUSED_PARAMETER(argc);  //��ʹ�õĲ���������ǰ��Ķ�������Ϊ��
  UNUSED_PARAMETER(argv);
  sqlite3_result_text(context, zShellStatic, -1, SQLITE_STATIC); //
}


/*
** This routine reads a line of text from FILE in, stores
** the text in memory obtained from malloc() and returns a pointer
** to the text.  NULL is returned at end of file, or if malloc()
** fails.
**
** The interface is like "readline" but no command-line editing  //readline����������һ��textstream�ļ�
��ȡһ���в����صõ����ַ���������ӿ�����readlineһ���������������б༭
** is done.
*/  //���ļ����ı��ж�ȡһ�У����ı��洢����malloc�����еõ����ڴ�ռ䣬���ҷ���һ��ָ�룬���ʧ�ܣ�����

����������NULL,�������malloc()ʧ�ܡ�
// ����Ҫһ�ζ���һ���кܳ�������ʱ����ʹ�ô˷���
static char *local_getline(char *zPrompt, FILE *in, int csvFlag){  //���ļ��ж�ȡ�еĺ�������:*zPrompt��ʾ

��ȡ���ַ���*in��ʾ���ļ���ָ�룬csvFlag��ȡ�ĳ���
  char *zLine;  //��ȡ��
  int nLine;  //ָ������
  int n; 
  int inQuote = 0;

  if( zPrompt && *zPrompt ){// ��ȡ�ɹ���������ַ���
    printf("%s",zPrompt);
    fflush(stdout);
  }
  nLine = 100;  //��ֵָ������
  zLine = malloc( nLine ); //�����СΪnLine���ڴ�ռ�
  if( zLine==0 ) return 0;  //����ַ���Ϊ�գ��򷵻�0
  n = 0;
  while( 1 ){  //�趨һ��һ���ַ����ĳ�������Ϊ�������Ĵ�С, ÿ�ζ�ȡ��, ���ж����Ƿ񵽴���ĩ, ���û�е���, 

����������ķ�����̬���仺����
    if( n+100>nLine ){  
      nLine = nLine*2 + 100;
      zLine = realloc(zLine, nLine); //��zLine����Ĵ洢�ռ��ΪnLine��С
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

struct previous_mode_data {  // �����˽ṹ�壬�ýṹ�����������.explain����ִ��֮ǰ��ģʽ��Ϣ
  int valid;        /* Is there legit data in here? */
  int mode;                       //���ģʽ
  int showHeader;                //��ʾ����
  int colWidth[100];             //�����п�
};

/*
** An pointer to an instance of this structure is passed from
** the main program to the callback.  This is used to communicate
** state and mode information.
*/
struct callback_data {  //����ṹ�壬�������и�����֮��Ĵ�ֵ�뵱ǰ״̬�Ļ�ȡ����ص�������������ģʽ��Ϣ

��
  sqlite3 *db;           //��ʾҪ�򿪵����ݿ�                                 /* The database */ 
  int echoOn;                                                                 /* True to echo input commands */
  int statsOn;          //��ÿ�ν���֮ǰ��ʾ�洢����ͳ������                  /* True to display memory stats 

before each finalize */
  int cnt;              //�Ѿ���ʾ�ļ�¼��                                    /* Number of records displayed so far */
  FILE *out;            //��ʾ����������ļ���                                /* Write results here */
  FILE *traceOut;                                                             /* Output for sqlite3_trace() */
  int nErr;               //��ʾ���صĴ���                                    /* Number of errors seen */
  int mode;               //���ģʽ                                           /* An output mode setting */
  int writableSchema;                                                         /* True if PRAGMA writable_schema=ON */
  int showHeader;          //���б������ģʽ����ʾ�е�����                    /* True to show column names in 

List or Column mode */
  char *zDestTable;       //��insert��ʾģʽ�£��洢������ƣ����㹹��sql���  /* Name of destination table 

when MODE_Insert */
  char separator[20];                                                         /* Separator character for MODE_List */
  int colWidth[100];      //����ģʽ�µ������п�                               /* Requested width of each column 

when in column mode*/
  int actualWidth[100];    //�е�ʵ�ʿ��                                      /* Actual width of each column */
  char nullvalue[20];     //��������ݿ��з��صļ�¼�пյ�ѡ��������ͨ��.nullvalue����������
                                        /* The text to print when a NULL comes back from
                                       ** the database */
  struct previous_mode_data explainPrev;    
                                       /* Holds the mode information just before
                                       ** .explain ON */
  char outfile[FILENAME_MAX];                                                  /* Filename for *out */
  const char *zDbFilename;    //������ݿ��ļ�������                           /* name of the database file */
  const char *zVfs;                                                            /* Name of VFS to use */
  sqlite3_stmt *pStmt;      //��ŵ�ǰ��statement���                          /* Current statement if any. */
  FILE *pLog;                 //��ʾ�����������־�ļ���                        /* Write log output here */
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

static const char *modeDescr[] = { //���������ģʽ�ַ����飻������ʾ��ʽ���кü�����ʾģʽ��Ĭ�ϵ��� 

list ��ʾģʽ��һ������ʹ�� column ��ʾģʽ
  "line",    //ÿ��һ��ֵ
  "column",   //�����������ʾÿһ������
  "list",    //�ָ����ָ����ַ�
  "semi",    //��listģʽ���ƣ�����ÿһ�л��ԡ���������
  "html",     //��html���뷽ʽ��ʾ
  "insert",  //��ʾinsert sql���
  "tcl",    //TCL�б�Ԫ��
  "csv",     //���ŷָ�ֵ
  "explain",  //��column���ƣ������ض�����
};

/*
** Number of elements in an array   //������Ԫ�ص�����
*/
#define ArraySize(X)  (int)(sizeof(X)/sizeof(X[0]))

/*
** Compute a string length that is limited to what can be stored in
** lower 30 bits of a 32-bit signed integer.
*/
static int strlen30(const char *z){     //�ܹ��洢�����bit��;�ַ������������޵�,���Դ洢�ڵ�30λ��32λ����

������
  const char *z2 = z;
  while( *z2 ){ z2++; }
  return 0x3fffffff & (int)(z2 - z);
}

/*
** A callback for the sqlite3_log() interface.    //sqlite3_log()�ӿڵĻص�
*/
static void shellLog(void *pArg, int iErrCode, const char *zMsg){  //����shell�����е���־
  struct callback_data *p = (struct callback_data*)pArg;  
  if( p->pLog==0 ) return;   //���û����־����
  fprintf(p->pLog, "(%d) %s\n", iErrCode, zMsg); //�����־��Ϣ
  fflush(p->pLog); //��ջ���
}

/*
** Output the given string as a hex-encoded blob (eg. X'1234' )
*/
static void output_hex_blob(FILE *out, const void *pBlob, int nBlob){//���ַ�����hex�����Ʊ���ķ�ʽ��

��
  int i;
  char *zBlob = (char *)pBlob;
  fprintf(out,"X'");
  for(i=0; i<nBlob; i++){ fprintf(out,"%02x",zBlob[i]&0xff); }
  fprintf(out,"'");
}

/*
** Output the given string as a quoted string using SQL quoting conventions.
*/
static void output_quoted_string(FILE *out, const char *z){//���ַ�������֤�ַ�������ʽ���
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
static void output_c_string(FILE *out, const char *z){  //����C��TCL���ù�������ַ���
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
static void output_html_string(FILE *out, const char *z){//�������HTML���뷽ʽ��ʾ�ַ���
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
** array, then the string must be quoted for CSV.  // ���һ��������κα���������Ķ�����ַ�������ַ�
�����뱻��֤ΪCSV
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
static void output_csv(struct callback_data *p, const char *z, int bSep){//��csv��ʽ����ַ���������p-

>separator��������ʾ�ָ�����p->nullvalue��ʾNUllֵ���ַ���ֻ���ڱ�Ҫ��ʱ������
  FILE *out = p->out;
  if( z==0 ){
    fprintf(out,"%s",p->nullvalue);  //��ʽ����� fprintf(�ļ�ָ��,��ʽ�ַ���,�������)
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
//seenInterrupt����������жϵı�����ǰ�涨���ֵΪ0������յ��ж��źţ��ͽ�������ֵΪ 1
static void interrupt_handler(int NotUsed){ //�жϿ��ƺ�����������ΪCtrl-C��ʱ�����
  UNUSED_PARAMETER(NotUsed);   //��ʾ��ʹ�õĲ���
  seenInterrupt = 1;   //ָʾ�ж��źŵı�������ʱ��ʾ�յ��ж��źš�
  if( db ) sqlite3_interrupt(db);  //������ݿⱻ�򿪣����ж���
}
#endif

/*
** This is the callback routine that the shell
** invokes for each row of a query result.
*/
static int shell_callback(void *pArg, int nArg, char **azArg, char **azCol, int *aiType){ //�������ص���ѯ��

����ÿһ��
  int i;
  struct callback_data *p = (struct callback_data*)pArg; //����һ��callback_data�Ķ���

  switch( p->mode ){  //�жϵ��õ�ģʽ�����ݵ��õ�ģʽ��ͬ��ѡ��ͬ�ķ�ʽ������
    case MODE_Line: {  //Lineģʽ
      int w = 5;
      if( azArg==0 ) break;
      for(i=0; i<nArg; i++){
        int len = strlen30(azCol[i] ? azCol[i] : "");
        if( len>w ) w = len;
      }
      if( p->cnt++>0 ) fprintf(p->out,"\n");
      for(i=0; i<nArg; i++){
        fprintf(p->out,"%*s = %s\n", w, azCol[i],
                azArg[i] ? azArg[i] : p->nullvalue);  //p->nullvalue��ʾNUllֵ
      }
      break;
    }
    case MODE_Explain:
    case MODE_Column: {  //Explain��Columnģʽ
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
    case MODE_List: { //Semi��Listģʽ
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
    case MODE_Html: {  //Htmlģʽ
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
    case MODE_Tcl: { // Tclģʽ
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
    case MODE_Csv: { //Csvģʽ
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
    case MODE_Insert: {  //Insertģʽ
      p->cnt++;
      if( azArg==0 ) break;
      fprintf(p->out,"INSERT INTO %s VALUES(",p->zDestTable);//ָĿ�ı�
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
static int callback(void *pArg, int nArg, char **azArg, char **azCol){   //����SQLite����ò�ѯ�����ÿһ��

�Ļص�����
  /* since we don't have type info, call the shell_callback with a NULL value */
  return shell_callback(pArg, nArg, azArg, azCol, NULL);  //��û��������Ϣ,ʹ��Nullֵ����shell_callback 
}

/*
** Set the destination table field of the callback_data structure to
** the name of the table given.  Escape any quote characters in the
** table name.
*/
static void set_table_name(struct callback_data *p, const char *zName){ //�趨��Ŀ����ֶ�callback_data

�ṹ�ı�����ơ��κ������ַ�ת��ı�����zName��ʾ����
  int i, n;
  int needQuote;
  char *z;

  if( p->zDestTable ){   //p->zDestTableָĿ�ı�
    free(p->zDestTable);  //�ͷſռ�
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
*/    //���zInt����null�����ͷſռ�
static char *appendText(char *zIn, char const *zAppend, char quote){//zInt����malloc()�л�õ��ڴ�����NUll�ַ�����β���ַ���ָ����ʾNUllָ�룻zAppendָ����ַ����Ǽӵ�zInt�ϵģ����صĽ������malloc()�������������������'\0',��ô����zAppend�����ַ�
  int len;//���峤��
  int i;
  int nAppend = strlen30(zAppend);
  int nIn = (zIn?strlen30(zIn):0);

  len = nAppend+nIn+1;
  if( quote ){//���quote����'\0',��ô����zAppend�����ַ�
    len += 2;
    for(i=0; i<nAppend; i++){
      if( zAppend[i]==quote ) len++;
    }
  }

  zIn = (char *)realloc(zIn, len);//���·����ڴ�
  if( !zIn ){
    return 0;
  }

  if( quote ){//���quote����'\0'
    char *zCsr = &zIn[nIn];
    *zCsr++ = quote;
    for(i=0; i<nAppend; i++){
      *zCsr++ = zAppend[i];
      if( zAppend[i]==quote ) *zCsr++ = quote; //���zAppendָ����ַ�����quote���
    }
    *zCsr++ = quote;
    *zCsr++ = '\0';
    assert( (zCsr-zIn)==len );
  }else{
    memcpy(&zIn[nIn], zAppend, nAppend);//�ַ�������
    zIn[len-1] = '\0';
  }

  return zIn;
}


/*
** Execute a query statement that will generate SQL output.  Print
** the result columns, comma-separated, on a line and then add a
** semicolon terminator to the end of that line.
**  //ִ��һ�����ɵ�SQL����Ĳ�ѯ��䡣��ӡ�����,���ŷָ���,��һ���ֺ��ս�����
** If the number of columns is 1 and that column contains text "--"
** then write the semicolon on a separate line.  That way, if a 
** "--" comment occurs at the end of the statement, the comment
** won't consume the semicolon terminator.
*/  //����е�������1�����а����ı�����������Ȼ������ֺ��ڵ��������С�����,���һ����������������������

���,��ʹ�÷ֺ�
static int run_table_dump_query(//ʹ��.dump������Խ����ݿ���󵼳���SQL��ʽ
  struct callback_data *p, //Ҫ��ѯ������    /* Query context */  
  const char *zSelect,     //��ȡѡ����������  /* SELECT statement to extract content */
  const char *zFirstRow    //�����ΪNUll�����ڵ�һ��֮ǰ��ӡ   /* Print before first row, if not NULL */
){
  sqlite3_stmt *pSelect;  //��һ��sql��������pSelect�У�����ŵ�ǰ��statement��� 
  int rc;     //���巵��ֵ
  int nResult;
  int i;
  const char *z;
  rc = sqlite3_prepare(p->db, zSelect, -1, &pSelect, 0); //������� sql ���Ľ�������һ��������ǰ��һ����

�Ǹ� sqlite3 * ���ͱ������ڶ���������һ�� sql ��䡣������������д����-1���������������ǰ�� sql ���ĳ���

�����С��0��sqlite���Զ��������ĳ��ȣ���sql��䵱����\0��β���ַ����������ĸ������� sqlite3_stmt ��ָ��

��ָ�롣�����Ժ��sql���ͷ�������ṹ�
  if( rc!=SQLITE_OK || !pSelect ){ //�������ֵ����SQLITE_OK����û�еõ���ǰ���
    fprintf(p->out, "/**** ERROR: (%d) %s *****/\n", rc, sqlite3_errmsg(p->db)); //�����������Ϣ
    p->nErr++;  //nErr��ʾ���صĴ�����Ϣ
    return rc;
  }
  rc = sqlite3_step(pSelect); //ͨ�������䣬pSelect ��ʾ��sql���ͱ�д�������ݿ�����Ҫ�� 

sqlite3_stmt �ṹ���ͷţ������ķ���ֵ���ڴ���sqlite3_stmt������ʹ�õĺ���
  nResult = sqlite3_column_count(pSelect); //����ռ�
  while( rc==SQLITE_ROW ){ //����ֵΪSQLITE_ROW
    if( zFirstRow ){ //�����ΪNUll�����ӡ��һ��
      fprintf(p->out, "%s", zFirstRow);
      zFirstRow = 0;
    }
    z = (const char*)sqlite3_column_text(pSelect, 0);//��ȡ����
    fprintf(p->out, "%s", z);
    for(i=1; i<nResult; i++){ 
      fprintf(p->out, ",%s", sqlite3_column_text(pSelect, i));//ѭ���������
    }
    if( z==0 ) z = "";
    while( z[0] && (z[0]!='-' || z[1]!='-') ) z++; //����е�������1�����а����ı�����������Ȼ������ֺ��ڵ�

��������
    if( z[0] ){
      fprintf(p->out, "\n;\n");
    }else{
      fprintf(p->out, ";\n");
    }    
    rc = sqlite3_step(pSelect); //��sql���д�����ݿ���
  }
  rc = sqlite3_finalize(pSelect);//�Ѹղŷ���������������������������ǰ�汻sqlite3_prepare������׼�����

��ÿ��׼����䶼����ʹ���������ȥ�����Է�ֹ�ڴ�й¶��
  if( rc!=SQLITE_OK ){ //�������ֵ����SQLITE_OK���򷵻ش�����Ϣ
    fprintf(p->out, "/**** ERROR: (%d) %s *****/\n", rc, sqlite3_errmsg(p->db));
    p->nErr++;
  }
  return rc;  //�����ķ���ֵ
}

/*
** Allocate space and save off current error string. //����ռ䣬����������ǰ������ַ���
*/
static char *save_err_msg(  //���������Ϣ
  sqlite3 *db               //Ҫ���ʵ����ݿ� /* Database to query */
){
  int nErrMsg = 1+strlen30(sqlite3_errmsg(db));
  char *zErrMsg = sqlite3_malloc(nErrMsg);//ͨ��sqlite3_malloc()�ӿڣ�SQLite��չ��Ӧ�ó���������ʹ

����ͬ��SQLite�ĵײ���亯����ʹ���ڴ�
  if( zErrMsg ){
    memcpy(zErrMsg, sqlite3_errmsg(db), nErrMsg);//���´�����Ϣ
  }
  return zErrMsg;
}

/*
** Display memory stats.   //��ʾ�ڴ�ͳ������
*/
static int display_stats(  //��ʾͳ������
  sqlite3 *db,                // Ҫ���ʵ����ݿ� /* Database to query */
  struct callback_data *pArg, //����һ���ص�������ָ��  /* Pointer to struct callback_data */
  int bReset                 //�����ò��������ж� /* True to reset the stats */
){
  int iCur;  //��������ָʾ�������洢��ǰ��ѡ��ֵ
  int iHiwtr; //�洢��ʷ���ֵ

  if( pArg && pArg->out ){  //pArg->outָ������������ļ���
    
    iHiwtr = iCur = -1;//��ֵΪ-1
    sqlite3_status(SQLITE_STATUS_MEMORY_USED, &iCur, &iHiwtr, 

bReset);//SQLITE_STATUS_MEMORY_USEDȷ�ϵ�ǰ���ʵ�ͳ����Ϣ����ǰѡ���ֵ��д�뵽iCur���Ͳ�������ʷ

���ֵ��д�뵽iHiwtr�����С����bResetΪtrue�����ڵ��÷���ʱiHiwtr��־������Ϊ��ǰѡ���ֵ��
    fprintf(pArg->out, "Memory Used:                         %d (max %d) bytes\n", iCur, iHiwtr);
    iHiwtr = iCur = -1;
    sqlite3_status(SQLITE_STATUS_MALLOC_COUNT, &iCur, &iHiwtr, bReset);//��ǰ���ڴ������Ϣ
    fprintf(pArg->out, "Number of Outstanding Allocations:   %d (max %d)\n", iCur, iHiwtr);
/*
** Not currently used by the CLI.  // û��ʹ�������н���
**    iHiwtr = iCur = -1;
**    sqlite3_status(SQLITE_STATUS_PAGECACHE_USED, &iCur, &iHiwtr, bReset);//ҳ�滺��ʹ����Ϣ
**    fprintf(pArg->out, "Number of Pcache Pages Used:         %d (max %d) pages\n", iCur, iHiwtr);//ʹ
�õļĴ���ҳ�������
*/
    iHiwtr = iCur = -1;
    sqlite3_status(SQLITE_STATUS_PAGECACHE_OVERFLOW, &iCur, &iHiwtr, bReset);//ҳ�滺�������Ϣ
    fprintf(pArg->out, "Number of Pcache Overflow Bytes:     %d (max %d) bytes\n", iCur, iHiwtr);
/*
** Not currently used by the CLI.
**    iHiwtr = iCur = -1;
**    sqlite3_status(SQLITE_STATUS_SCRATCH_USED, &iCur, &iHiwtr, bReset); //��¼��Ϣ
**    fprintf(pArg->out, "Number of Scratch Allocations Used:  %d (max %d)\n", iCur, iHiwtr);
*/
    iHiwtr = iCur = -1;
    sqlite3_status(SQLITE_STATUS_SCRATCH_OVERFLOW, &iCur, &iHiwtr, bReset);//��¼��Ϣ���
    fprintf(pArg->out, "Number of Scratch Overflow Byt es:    %d (max %d) bytes\n", iCur, iHiwtr);
    iHiwtr = iCur = -1;
    sqlite3_status(SQLITE_STATUS_MALLOC_SIZE, &iCur, &iHiwtr, bReset);//������ڴ��С
	.
    fprintf(pArg->out, "Largest Allocation:                  %d bytes\n", iHiwtr);
    iHiwtr = iCur = -1;
    sqlite3_status(SQLITE_STATUS_PAGECACHE_SIZE, &iCur, &iHiwtr, bReset);//ҳ�滺��Ĵ�С��Ϣ
    fprintf(pArg->out, "Largest Pcache Allocation:           %d bytes\n", iHiwtr);
    iHiwtr = iCur = -1;
    sqlite3_status(SQLITE_STATUS_SCRATCH_SIZE, &iCur, &iHiwtr, bReset);//��¼��Ϣ�Ĵ�С
    fprintf(pArg->out, "Largest Scratch Allocation:          %d bytes\n", iHiwtr);
#ifdef YYTRACKMAXSTACKDEPTH
    iHiwtr = iCur = -1;
    sqlite3_status(SQLITE_STATUS_PARSER_STACK, &iCur, &iHiwtr, bReset); //��������ջ
    fprintf(pArg->out, "Deepest Parser Stack:                %d (max %d)\n", iCur, iHiwtr);
#endif
  }
//���ڵ������ݿ����ӵ�ͳ��
  if( pArg && pArg->out && db ){//����õ�����ļ��������ݿ����ӳɹ�
    iHiwtr = iCur = -1;//��ֵ-1
    sqlite3_db_status(db, SQLITE_DBSTATUS_LOOKASIDE_USED, &iCur, &iHiwtr, 

bReset);//sqlite3_db_status()��һ�����ݿ����Ӳ��������ҷ��ص���������ӵ��ڴ�ͳ����Ϣ������������SQLite

��
    fprintf(pArg->out, "Lookaside Slots Used:                %d (max %d)\n", iCur, iHiwtr);
    sqlite3_db_status(db, SQLITE_DBSTATUS_LOOKASIDE_HIT, &iCur, &iHiwtr, bReset);//������
    fprintf(pArg->out, "Successful lookaside attempts:       %d\n", iHiwtr);
    sqlite3_db_status(db, SQLITE_DBSTATUS_LOOKASIDE_MISS_SIZE, &iCur, &iHiwtr, bReset);//��ȱʧ

��С
    fprintf(pArg->out, "Lookaside failures due to size:      %d\n", iHiwtr);
    sqlite3_db_status(db, SQLITE_DBSTATUS_LOOKASIDE_MISS_FULL, &iCur, &iHiwtr, bReset);//��ʧ��
    fprintf(pArg->out, "Lookaside failures due to OOM:       %d\n", iHiwtr);
    iHiwtr = iCur = -1;
    sqlite3_db_status(db, SQLITE_DBSTATUS_CACHE_USED, &iCur, &iHiwtr, bReset);//ҳ���ʹ��
    fprintf(pArg->out, "Pager Heap Usage:                    %d bytes\n", iCur);    iHiwtr = iCur = -1;
    sqlite3_db_status(db, SQLITE_DBSTATUS_CACHE_HIT, &iCur, &iHiwtr, 1);
    fprintf(pArg->out, "Page cache hits:                     %d\n", iCur);//ҳ��ĸ��ٻ�������
    iHiwtr = iCur = -1;
    sqlite3_db_status(db, SQLITE_DBSTATUS_CACHE_MISS, &iCur, &iHiwtr, 1);//ҳ��Ļ��涪ʧ
    fprintf(pArg->out, "Page cache misses:                   %d\n", iCur); 
    iHiwtr = iCur = -1;
    sqlite3_db_status(db, SQLITE_DBSTATUS_CACHE_WRITE, &iCur, &iHiwtr, 1);//ҳ����ٻ��涪ʧ
    fprintf(pArg->out, "Page cache writes:                   %d\n", iCur); 
    iHiwtr = iCur = -1;
    sqlite3_db_status(db, SQLITE_DBSTATUS_SCHEMA_USED, &iCur, &iHiwtr, bReset);//ģʽ�Ķ�ʹ��
    fprintf(pArg->out, "Schema Heap Usage:                   %d bytes\n", iCur); 
    iHiwtr = iCur = -1;
    sqlite3_db_status(db, SQLITE_DBSTATUS_STMT_USED, &iCur, &iHiwtr, bReset);//�����ĶѺͺ�ʹ��
    fprintf(pArg->out, "Statement Heap/Lookaside Usage:      %d bytes\n", iCur); 
  }

  if( pArg && pArg->out && db && pArg->pStmt ){//����õ������������͵�ǰ���������
    iCur = sqlite3_stmt_status(pArg->pStmt, SQLITE_STMTSTATUS_FULLSCAN_STEP, bReset);//����ɨ��
    fprintf(pArg->out, "Fullscan Steps:                      %d\n", iCur);
    iCur = sqlite3_stmt_status(pArg->pStmt, SQLITE_STMTSTATUS_SORT, bReset);//�������
    fprintf(pArg->out, "Sort Operations:                     %d\n", iCur);
    iCur = sqlite3_stmt_status(pArg->pStmt, SQLITE_STMTSTATUS_AUTOINDEX, bReset);//�Զ�����
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

/*ִ��һ����һ����䣬���ݵ�ǰģʽ����������sqlite3_exec()��������/*
static int shell_exec(
  sqlite3 *db,                                /* An open database */ /*һ���򿪵����ݿ�*/
  const char *zSql,                           /* SQL to be evaluated */ /*Ҫִ�е�SQL���*/
  int (*xCallback)(void*,int,char**,char**,int*),   /* Callback function */ /*�ص�����*/
                                              /* (not the same as sqlite3_exec) */
  struct callback_data *pArg,                 /* Pointer to struct callback_data */ /*��ṹ��ǰ�涨�������������������Ҫ��ֵ*/
  char **pzErrMsg                             /* Error msg written here */ /*���ڱ��������Ϣ*/
){
  sqlite3_stmt *pStmt = NULL;     /* Statement to execute. */ /*pStmt��ŵ�ǰ��SQL��䣬ĿǰΪ��*/
  int rc = SQLITE_OK;             /* Return Code */ /*������rc ��ֵΪSQLITE_OK��ʾ����*/
  int rc2;
  const char *zLeftover;          /* Tail of unprocessed SQL */ /*ָ��δ�����SQL���β��*/

  if( pzErrMsg ){
    *pzErrMsg = NULL; /*������Ϣ��ʼ��Ϊ��*/
  }

  while( zSql[0] && (SQLITE_OK == rc) ){/*��δִ����䣬�ҷ�������SQLITE_OK��ʾһ������*/
    rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, &zLeftover);/*�������*/
    if( SQLITE_OK != rc ){/*����д�*/
      if( pzErrMsg ){
        *pzErrMsg = save_err_msg(db);/*д�������Ϣ*/
      }
    }else{
      if( !pStmt ){
        /* this happens for a comment or white-space *//*����ע�ͻ��߿ո�ʱ��ִ�д˷�֧*/
        zSql = zLeftover;/*�����ָ���Ƶ�δ����������β��*/
        while( IsSpace(zSql[0]) ) zSql++;/*ѭ���������*/
        continue;
      }

      /* save off the prepared statment handle and reset row count */ /*����׼���õľ������������*/
      if( pArg ){
        pArg->pStmt = pStmt;
        pArg->cnt = 0;/*��������Ϊ0
      }
      /* echo the sql statement if echo on *//*�����Ҫ���������׼���õ�������*/
      if( pArg && pArg->echoOn ){
        const char *zStmtSql = sqlite3_sql(pStmt);
        fprintf(pArg->out, "%s\n", zStmtSql ? zStmtSql : zSql);
      }

      /* Output TESTCTRL_EXPLAIN text of requested *//*�����Ҫ��TESTCTRL_EXPLAIN�ĵ�*/
      if( pArg && pArg->mode==MODE_Explain ){
        const char *zExplain = 0;
        sqlite3_test_control(SQLITE_TESTCTRL_EXPLAIN_STMT, pStmt, &zExplain);/*����һ�����Sqlite���Ƿ���ȷ�ĺ���*/
        if( zExplain && zExplain[0] ){
          fprintf(pArg->out, "%s", zExplain);
        }
      }
            /*ִ�е�һ����Ȼ���֪���Ƿ���һ��������Լ����Ĵ�С*/
      /* perform the first step.  this will tell us if we
      ** have a result set or not and how wide it is.
      */
      rc = sqlite3_step(pStmt);/*ִ�����*/
      /* if we have a result set... *//*����Ѿ�������һ�����*/
      if( SQLITE_ROW == rc ){
        /* if we have a callback... *//*����лص�������ִ��*/
        if( xCallback ){
          /* allocate space for col name ptr, value ptr, and type *//*����ռ�*/
          int nCol = sqlite3_column_count(pStmt);/*ȡ�ֶ���*/
          void *pData = sqlite3_malloc(3*nCol*sizeof(const char*) + 1);/*�����ֶ�������ռ�*/
          if( !pData ){/*�����һ����������*/
            rc = SQLITE_NOMEM;/*�򷵻�SQLITE_NOMEM��ʾmalloc��������ʧ��*/
          }else{                                                                             
            char **azCols = (char **)pData;      /* Names of result columns *//*�����������*/
            char **azVals = &azCols[nCol];       /* Results *//*���*/
            int *aiTypes = (int *)&azVals[nCol]; /* Result types *//*�������*/
            int i;
            assert(sizeof(int) <= sizeof(char *)); 
            /* save off ptrs to column names *//* ȡ���ֶε�����*/
            for(i=0; i<nCol; i++){ 
              azCols[i] = (char *)sqlite3_column_name(pStmt, i);
            }
            do{
              /* extract the data and data types *//*��ȡ���ݺ���������*/
              for(i=0; i<nCol; i++){/*ȡ���ֶε�ֵ*/
                azVals[i] = (char *)sqlite3_column_text(pStmt, i);
                aiTypes[i] = sqlite3_column_type(pStmt, i);
                if( !azVals[i] && (aiTypes[i]!=SQLITE_NULL) ){
                  rc = SQLITE_NOMEM;
                  break; /* from for */
                }
              } /* end for */

              /* if data and types extracted successfully... *//*��������Լ�������ȡ�ɹ�*/
              if( SQLITE_ROW == rc ){ 
                /* call the supplied callback with the result row data *//*���ݵ�ǰ���ݵ��ûص������Է��صļ�¼���д���*/
                if( xCallback(pArg, nCol, azVals, azCols, aiTypes) ){
                  rc = SQLITE_ABORT;/*�ص����������ж�*/
                }else{
                  rc = sqlite3_step(pStmt);/*���û���ն˾�ִ�����*/
                }
              }
            } while( SQLITE_ROW == rc );/*�õ�������ͷſռ�*/
            sqlite3_free(pData);
          }
        }else{
          do{
            rc = sqlite3_step(pStmt);/*ִ�����*/
          } while( rc == SQLITE_ROW );
        }
      }

      /* print usage stats if stats on *//*���������ͳ�� �Ǿ���ʾͳ��*/
      if( pArg && pArg->statsOn ){
        display_stats(db, pArg, 0);
      }

      /* Finalize the statement just executed. If this fails, save a 
      ** copy of the error message. Otherwise, set zSql to point to the
      ** next statement to execute. */
      /*�������ִ�У����ʧ�ܱ��������Ϣ������ָ��ָ����һ����Ҫִ�е����*/
      rc2 = sqlite3_finalize(pStmt);
      if( rc!=SQLITE_NOMEM ) rc = rc2;
      if( rc==SQLITE_OK ){
        zSql = zLeftover;/*ָ����һ����Ҫִ�е����*/
        while( IsSpace(zSql[0]) ) zSql++;
      }else if( pzErrMsg ){
        *pzErrMsg = save_err_msg(db);/*���򱣴������Ϣ*/
      }

      /* clear saved stmt handle *//*����ѱ���ľ��*/
      if( pArg ){
        pArg->pStmt = NULL;
      }
    }
  } /* end while *//*����ѭ��*/

  return rc;
}


/*
** This is a different callback routine used for dumping the database.
** Each row received by this callback consists of a table name,
** the table type ("index" or "table") and SQL to create the table.
** This routine should print text sufficient to recreate the table.
*/

/*
**����һ������ת�����ݿ�Ļص����� �������յ��ɱ���������
**�ͣ��������Ǳ��ʹ�������SQL���У������Ӧ����㹻�Ŀ���
**�ؽ�����ĵ���
*/


static int dump_callback(void *pArg, int nArg, char **azArg, char **azCol){
  int rc; 
  const char *zTable;/*����*/
  const char *zType;/*������*/
  const char *zSql;/*SQL���*/
  const char *zPrepStmt = 0;
  struct callback_data *p = (struct callback_data *)pArg;
/*����һ��callback_data�ṹ�壬�������и��ֳ���֮��Ĵ�ֵ�Լ���ȡ��ǰ״̬*/
  UNUSED_PARAMETER(azCol);/*��ʾ��ʹ�����һ������*/
  if( nArg!=3 ) return 1;/* �������������ȫ���ʾ����*/
  zTable = azArg[0];
  zType = azArg[1];/*��azArg������Ԫ������Ϊ���� ������ SQL���*/
  zSql = azArg[2];
  /*���������Ϊ"sqlite_sequence"����SQLite���ݿ��а���������ʱ,���Զ�����һ����Ϊ sqlite_sequence �ı�*/
  if( strcmp(zTable, "sqlite_sequence")==0 ){ 
    zPrepStmt = "DELETE FROM sqlite_sequence;\n";/*�����б�������ж�����*/
     /*���zTable��ֵΪ"sqlite_stat1"�����е�ͳ����Ϣ������һ������sqlite_stat1 �ı���*/
  }else if( strcmp(zTable, "sqlite_stat1")==0 ){ 
    fprintf(p->out, "ANALYZE sqlite_master;\n");/*sqlite_master �����Ҳ���Զ����ɵģ����汣����sqLite�Ŀ��*/
  }else if( strncmp(zTable, "sqlite_", 7)==0 ){
    return 0;
  }else if( strncmp(zSql, "CREATE VIRTUAL TABLE", 20)==0 ){ /*���SQL����ʾ����һ�������*/
    char *zIns;
    if( !p->writableSchema ){
      fprintf(p->out, "PRAGMA writable_schema=ON;\n");/*������ǿ�дģʽ ��Ҫ�ȵ�������дģʽ*/
      p->writableSchema = 1;
    }
    zIns = sqlite3_mprintf(/*��ʽ��������� ������ SQL��䵽zIns*/
       "INSERT INTO sqlite_master(type,name,tbl_name,rootpage,sql)"/*������ ������ SQL�����뵽sqlite_maste��*/
       "VALUES('table','%q','%q',0,'%q');",
       zTable, zTable, zSql);
    fprintf(p->out, "%s\n", zIns);/*��ʽ�����zIns������*/
    sqlite3_free(zIns);/*�ͷ�zIns������*/
    return 0;
  }else{/*�ǿ�дģʽ��ֱ�Ӹ�ʽ�����zIns������*/
    fprintf(p->out, "%s;\n", zSql);
  }

  if( strcmp(zType, "table")==0 ){/*���������Ϊ��table��*/
    sqlite3_stmt *pTableInfo = 0;
    char *zSelect = 0;
    char *zTableInfo = 0;
    char *zTmp = 0;
    int nRow = 0;
   /*��appendText������zTableInfo��ֵ���⺯��֮ǰ�ж��壬��������ƴ��*/
    zTableInfo = appendText(zTableInfo, "PRAGMA table_info(", 0);
    zTableInfo = appendText(zTableInfo, zTable, '"');
    zTableInfo = appendText(zTableInfo, ");", 0);
 
    rc = sqlite3_prepare(p->db, zTableInfo, -1, &pTableInfo, 0);/*�������*/
    free(zTableInfo);/*�ͷ�zTableInfo*/
    if( rc!=SQLITE_OK || !pTableInfo ){/*������ִ��󣬷���1*/
      return 1;
    }
/*��appendText������zSelect��ֵ*/
    zSelect = appendText(zSelect, "SELECT 'INSERT INTO ' || ", 0);
    /* Always quote the table name, even if it appears to be pure ascii,
    ** in case it is a keyword. Ex:  INSERT INTO "table" ... */
    /*һ�����ñ��� */
    zTmp = appendText(zTmp, zTable, '"');
    if( zTmp ){
      zSelect = appendText(zSelect, zTmp, '\'');
      free(zTmp);
    }
    zSelect = appendText(zSelect, " || ' VALUES(' || ", 0);
    rc = sqlite3_step(pTableInfo);
    while( rc==SQLITE_ROW ){/*�Ѿ�����һ�����*/
      const char *zText = (const char *)sqlite3_column_text(pTableInfo, 1);
      zSelect = appendText(zSelect, "quote(", 0);
      zSelect = appendText(zSelect, zText, '"');
      rc = sqlite3_step(pTableInfo);/*pTableInfo��ʾ��sql��佫��д�����ݿ�*/
      if( rc==SQLITE_ROW ){
        zSelect = appendText(zSelect, "), ", 0);
      }else{
        zSelect = appendText(zSelect, ") ", 0);
      }
      nRow++;
    }
    rc = sqlite3_finalize(pTableInfo);/*����pTableInfoli����������*/
    if( rc!=SQLITE_OK || nRow==0 ){
      free(zSelect);
      return 1;
    }
    zSelect = appendText(zSelect, "|| ')' FROM  ", 0);
    zSelect = appendText(zSelect, zTable, '"');
 /*ʹ��run_table_dump_query������ʵ�ֲ�ѯ���������SQL������ */
    rc = run_table_dump_query(p, zSelect, zPrepStmt); 
    if( rc==SQLITE_CORRUPT ){/* ���ݿ����ӳ����ȷ*/
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
/*����zQuery����dump_callback()��Ϊһ���ص�������ô��ѯ�����ݾͻ���ΪSQL�������*/
static int run_schema_dump_query(
  struct callback_data *p, /*Ҫ��ѯ������*/
  const char *zQuery
){
  int rc;/*���巵��ֵ*/
  char *zErr = 0;/*��ʼ��������Ϣ*/
  rc = sqlite3_exec(p->db, zQuery, dump_callback, p, &zErr);/*ִ��*/
  if( rc==SQLITE_CORRUPT ){/* ���ݿ����ӳ����ȷ*/
    char *zQ2;
    int len = strlen30(zQuery);
    fprintf(p->out, "/****** CORRUPTION ERROR *******/\n"); 
    if( zErr ){
      fprintf(p->out, "/****** %s ******/\n", zErr);/*���������Ϣ*/
      sqlite3_free(zErr);/*�ͷſռ�*/
      zErr = 0;
    }
    zQ2 = malloc( len+100 );/*ΪZQ2����ռ�*/
    if( zQ2==0 ) return rc;
    sqlite3_snprintf(len+100, zQ2, "%s ORDER BY rowid DESC", zQuery);/*����sqlite3_snprintf����ʵ�����*/
    rc = sqlite3_exec(p->db, zQ2, dump_callback, p, &zErr);/*ִ��*/
    if( rc ){
      fprintf(p->out, "/****** ERROR: %s ******/\n", zErr);
    }else{
      rc = SQLITE_CORRUPT;/* ���ݿ����ӳ����ȷ*/
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
**������Ϣ���ĵ��������Ǹ��ֵ������˵��
*/
static char zHelp[] =
/*����ָ�������ݿ⵽ָ�����ļ���ȱʡΪ��ǰ���ӵ�main���ݿ�*/
  ".backup ?DB? FILE      Backup DB (default \"main\") to FILE\n" 
  /*���������������  Ĭ����OFF */
  ".bail ON|OFF           Stop after hitting an error.  Default OFF\n"
  /*�г����ݿ��ļ���*/
  ".databases             List names and files of attached databases\n"
  /*����ת�� �������γ����ݿ���SQL�ű�*/
  ".dump ?TABLE? ...      Dump the database in an SQL text format\n"

  "                         If TABLE specified, only dump tables matching\n"
  "                         LIKE pattern TABLE.\n"
  /*��ʾ���أ�����ΪON��������� */
  ".echo ON|OFF           Turn command echo on or off\n"
  ".exit                  Exit this program\n" /*�˳���ǰ����*/
  /*������ر��ʺ��� EXPLAIN �����ģʽ�����û�д����������� EXPLAIN��*/
  ".explain ?ON|OFF?      Turn output mode suitable for EXPLAIN on or off.\n"
  "                         With no args, it turns EXPLAIN on.\n"
 /*�򿪻��߹رձ�ͷ��ʾ*/
  ".header(s) ON|OFF      Turn display of headers on or off\n"/
  /*��ʾ���ĵ����г�������������*/
  ".help                  Show this message\n"
   /*����ָ���ļ������ݵ�ָ����*/
  ".import FILE TABLE     Import data from FILE into TABLE\n"
  /*��ʾ�������������֣����ָ���������������ʾƥ��ñ��������ݱ������������*/
  ".indices ?TABLE?       Show names of all indices\n"
  "                         If TABLE specified, only show indices for tables\n"  
  "                         matching LIKE pattern TABLE.\n"
#ifdef SQLITE_ENABLE_IOTRACE
  ".iotrace FILE          Enable I/O diagnostic logging to FILE\n"/*����I/O��ϼ�¼���ļ�*/
#endif
#ifndef SQLITE_OMIT_LOAD_EXTENSION
  ".load FILE ?ENTRY?     Load an extension library\n"  /*����һ����չ��*/
#endif/*�򿪻�ر���־���ܣ�FILE����Ϊ��׼���stdout�����׼�������stderr*/
  ".log FILE|off          Turn logging on or off.  FILE can be stderr/stdout\n"
  /*�������ģʽ��������Ϊ���õ�ģʽ��columnģʽ��ʹSELECT������������ʾ��*/
  ".mode MODE ?TABLE?     Set output mode where MODE is one of:\n" 
  "                         csv      Comma-separated values\n" /*�Զ��ŷָ�*/
  "                         column   Left-aligned columns.  (See .width)\n"/*�������*/
  "                         html     HTML <table> code\n" /*��ʾHTML����*/
  "                         insert   SQL insert statements for TABLE\n"/*sql�������*/
  "                         line     One value per line\n"/*һ��һ��ֵ*/
  "                         list     Values delimited by .separator string\n"/*ֵ��STRING�ָ�*/
  "                         tabs     Tab-separated values\n"/*��tab�ָ���ֵ*/
  "                         tcl      TCL list elements\n"/*TCL�б�Ԫ��*/
  ".nullvalue STRING      Print STRING in place of NULL values\n"/*��ָ���Ĵ����������NULL�� */
  ".output FILENAME       Send output to FILENAME\n"/*����ǰ�������������ض���ָ�����ļ���*/
  ".output stdout         Send output to the screen\n"  /*����ǰ�������������ض��򵽱�׼���(��Ļ)��*/
  ".prompt MAIN CONTINUE  Replace the standard prompts\n" /*�滻��׼��ʾ��*/
  ".quit                  Exit this program\n" /*�˳�*/
  ".read FILENAME         Execute SQL in FILENAME\n"/*ִ��ָ���ļ��ڵ�SQL��䡣*/
  /*��ָ�����ļ���ԭ���ݿ⣬ȱʡΪmain���ݿ⣬��ʱҲ����ָ���������ݿ���
  **��ָ�������ݿ��Ϊ��ǰ���ӵ�attached���ݿ⡣*/
  ".restore ?DB? FILE     Restore content of DB (default \"main\") from FILE\n" 
  ".schema ?TABLE?        Show the CREATE statements\n"/*��ʾ���ݱ�Ĵ�����䣬���ָ���������������ʾƥ��ñ���������*/
  "                         If TABLE specified, only show tables matching\n"
  "                         LIKE pattern TABLE.\n"
/*"�ı����ģʽ��.import���ֶμ�ָ����� */
  ".separator STRING      Change separator used by output mode and .import\n" 
  /*��ӡ����SQlite��������������*/
  ".show                  Show the current values for various settings\n"  
  ".stats ON|OFF          Turn stats on or off\n"/*������ر�ͳ��*/
  /*�г���ǰ������main���ݿ�����б��������ָ���������������ʾƥ��ñ��������ݱ�����
  **����TABLENAME֧��LIKE���ʽ֧�ֵ�ͨ�����*/
  ".tables ?TABLE?        List names of tables\n"
  "                         If TABLE specified, only list tables matching\n"
  "                         LIKE pattern TABLE.\n"
  ".timeout MS            Try opening locked tables for MS milliseconds\n"/*���Դ������ı� MS ΢��*/
  ".trace FILE|off        Output each SQL statement as it is run\n"/*���ÿһ���������е����*/
  ".vfsname ?AUX?         Print the name of the VFS stack\n"/*��������ջ������*/
   /*��MODEΪcolumnʱ�����ø����ֶεĿ�ȣ�ע�⣺������Ĳ���˳���ʾ�ֶ������˳��*/
  ".width NUM1 NUM2 ...   Set column widths for \"column\" mode\n" 
;

static char zTimerHelp[] =/*������ر� CPU ��ʱ������*/
  ".timer ON|OFF          Turn the CPU timer measurement on or off\n"
;

/* Forward reference *//*����*/
static int process_input(struct callback_data *p, FILE *in);

/*
** Make sure the database is open.  If it is not, then open it.  If
** the database fails to open, print an error message and exit.
*/
/*
���ܣ�ȷ�����ݿ��Ƿ��Ѿ��򿪡�����Ѵ򿪣���ʲô�����������
**û�У�������������ʧ�ܣ����һ��������Ϣ��
*/
static void open_db(struct callback_data *p){
  if( p->db==0 ){/*������ݿ�Ϊ��*/
    sqlite3_initialize();/*��ʼ��Sqlite���ݿ�*/
    sqlite3_open(p->zDbFilename, &p->db);/*zDbFilenamΪ������ݿ��ļ������� */
    db = p->db;
    if( db && sqlite3_errcode(db)==SQLITE_OK ){
      sqlite3_create_function(db, "shellstatic", 0, SQLITE_UTF8, 0,
          shellstaticFunc, 0, 0);
    }
    if( db==0 || SQLITE_OK!=sqlite3_errcode(db) ){/*�޷������ݿ�*/
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
/* C���Է�������*/
static void resolve_backslashes(char *z){
  int i, j;
  char c;
  for(i=j=0; (c = z[i])!=0; i++, j++){
    if( c=='\\' ){ /*�ַ�ֵΪ'\\'����ʾ���� */
      c = z[++i];
      if( c=='n' ){
        c = '\n';/*�ַ�ֵΪ'c'����ʾ����*/
      }else if( c=='t' ){
        c = '\t';/* �ַ�ֵΪ't'����ʾ��ǩ*/
      }else if( c=='r' ){
        c = '\r';/*�ַ�ֵΪ'r'����ʾ�س�*/
      }else if( c>='0' && c<='7' ){
        c -= '0';
        if( z[i+1]>='0' && z[i+1]<='7' ){/*����ǰ˽���*/
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
**��zArg����Ϊ����ֵ������1��0
*/

static int booleanValue(char *zArg){
  int val = atoi(zArg);
  int j;
  for(j=0; zArg[j]; j++){
    zArg[j] = ToLower(zArg[j]);/*ת��ΪСд*/
  }
  if( strcmp(zArg,"on")==0 ){/*��on��ת��Ϊ����ֵ1*/
    val = 1;
  }else if( strcmp(zArg,"yes")==0 ){/*��yes��ת��Ϊ����ֵ1*/
    val = 1;
  }
  return val;/*����ֵ*/
}

/*
** Close an output file, assuming it is not stderr or stdout
*/
/*
**�ر�һ���򿪵��ļ� ���費�Ǳ�׼������߱�׼���
*/
static void output_file_close(FILE *f){
  if( f && f!=stdout && f!=stderr ) fclose(f);
}

/*
** Try to open an output file.   The names "stdout" and "stderr" are
** recognized and do the right thing.  NULL is returned if the output 
** filename is "off".
*/
/*��һ������ļ�*/
static FILE *output_file_open(const char *zFile){
  FILE *f;/*f����Ҫ�򿪵��ļ�����*/
  if( strcmp(zFile,"stdout")==0 ){/*"�Ͽ�stdout"*/
    f = stdout;
  }else if( strcmp(zFile, "stderr")==0 ){/*�Ͽ�"stderr"*/
    f = stderr;
  }else if( strcmp(zFile, "off")==0 ){/*���������ļ�����OFF������NULL*/
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
**һ���ճ���,�ڶϵ���������
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

/*���һ���������ԡ�.����ʼ��CLP���
**��ô���ô˳������Ǹ���
**����1��ʾ���� 2��ʾ���� 0��ʾ����
*/   
/*����һ��**do_meta_command ��**������һ���������ַ�**�����ڶ��������ǻص�**������ָ��*/
static int do_meta_command(char *zLine, struct callback_data *p){
  int i = 1;
  int nArg = 0;
  int n, c;
  int rc = 0;
  char *azArg[50];

  /* Parse the input line into tokens.*//*���������� ���ַ���������azArg����*/
  */
  while( zLine[i] && nArg<ArraySize(azArg) ){ /*���з������*/
    while( IsSpace(zLine[i]) ){ i++; }
    if( zLine[i]==0 ) break;      /* û���������*/
    if( zLine[i]=='\'' || zLine[i]=='"' ){/*���Ϊ���з����߿ո� */
      int delim = zLine[i++];/*������*/
      azArg[nArg++] = &zLine[i];/*��������ַ������*/
      while( zLine[i] && zLine[i]!=delim ){ i++; }
      if( zLine[i]==delim ){
        zLine[i++] = 0;
      }
      if( delim=='"' ) resolve_backslashes(azArg[nArg-1]);/*�������Դ�ǿո�*/
    }else{
      azArg[nArg++] = &zLine[i];
      while( zLine[i] && !IsSpace(zLine[i]) ){ i++; }
      if( zLine[i] ) zLine[i++] = 0;
      resolve_backslashes(azArg[nArg-1]);
    }
  }

  /* Process the input line.*//*����������*/
  if( nArg==0 ) return 0; /* no tokens, no error */
  n = strlen30(azArg[0]);
  c = azArg[0][0];
  
  /*����һ��ָ�������ݿ⣨A����ָ�����ļ���B��ȱʡΪ��ǰ���ӵ�main���ݿ⣩*/
  if( c=='b' && n>=3 && strncmp(azArg[0], "backup", n)==0 && nArg>1 && nArg<4){
    const char *zDestFile;/*A������ */
    const char *zDb;/*B������ */
    sqlite3 *pDest;/*��Ҫ���ݵ����ݿ�A */
    sqlite3_backup *pBackup;/*Ŀ�����ݿ�B */
    if( nArg==2 ){
      zDestFile = azArg[1];
      zDb = "main"; /*ȱʡΪmain*/
    }else{
      zDestFile = azArg[2];
      zDb = azArg[1];
    }
    rc = sqlite3_open(zDestFile, &pDest);/*����Ҫ���ݵ����ݿ� */
    if( rc!=SQLITE_OK ){
      fprintf(stderr, "Error: cannot open \"%s\"\n", zDestFile);
      sqlite3_close(pDest);
      return 1;
    }
    open_db(p);
    /*Sqlite3_backup_init() ����һ��������Ŀ�����ݿ⣬������������Դ���ݿ�
    �涨���߲�����ͬ������ɹ�������ָ��Դ���ݿ��ָ��*/
    pBackup = sqlite3_backup_init(pDest, "main", p->db, zDb);
    if( pBackup==0 ){/*�������������������Ϣ */
      fprintf(stderr, "Error: %s\n", sqlite3_errmsg(pDest));
      sqlite3_close(pDest);
      return 1;
    }
    /*sqlite3_backup_step���ڱ������� */
    while(  (rc = sqlite3_backup_step(pBackup,100))==SQLITE_OK ){}
    sqlite3_backup_finish(pBackup);
    if( rc==SQLITE_DONE ){
      rc = 0;
    }else{
      fprintf(stderr, "Error: %s\n", sqlite3_errmsg(pDest));
      rc = 1;
    }/*��ɺ�رգ�����д�������Ϣ */
    sqlite3_close(pDest);/*�ر�pDestָ��Ŀռ�*/
  }else


/*��������ʱ���ټ���, Ĭ��ΪOFF*/
  if( c=='b' && n>=3 && strncmp(azArg[0], "bail", n)==0 && nArg>1 && nArg<3 ){
    bail_on_error = booleanValue(azArg[1]);/*��ת���Ĳ���ֵ��������*/
  }else

  /* The undocumented ".breakpoint" command causes a call to the no-op
  ** routine named test_breakpoint().
  */
  if( c=='b' && n>=3 && strncmp(azArg[0], "breakpoint", n)==0 ){
    test_breakpoint();
  }else
/*�г����ݿ��ļ���*/
  if( c=='d' && n>1 && strncmp(azArg[0], "databases", n)==0 && nArg==1 ){
    struct callback_data data;/*�������Բ���*/
    char *zErrMsg = 0;/*����һ����Ŵ�����Ϣ��ָ��*/
    open_db(p);/*��Pָ������ݿ�*/
    memcpy(&data, p, sizeof(data));/*��P��ָ���ڴ��ַ����ʼλ�� ����date���ȵ��ַ�
    ��date�ռ����ʼλ����*/
    data.showHeader = 1;/*�򿪱�ͷ��ʾ*/
    data.mode = MODE_Column;/*���õ�Columnģʽ*/
    data.colWidth[0] = 3;/*�����п�*/
    data.colWidth[1] = 15;
    data.colWidth[2] = 58;
    data.cnt = 0;/*��¼��Ϊ0*/
    sqlite3_exec(p->db, "PRAGMA database_list; ", callback, &data, &zErrMsg);
    /*ִ����ʾ���ݿ��б�*/
    if( zErrMsg ){/*���������Ϣ*/
      fprintf(stderr,"Error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      rc = 1;
    }
  }else

  if( c=='d' && strncmp(azArg[0], "dump", n)==0 && nArg<3 ){
    open_db(p);/*�����ݿ�*/
    /* When playing back a "dump", the content might appear in an order
    ** which causes immediate foreign key constraints to be violated.
    ** So disable foreign-key constraint enforcement to prevent problems. */
    fprintf(p->out, "PRAGMA foreign_keys=OFF;\n");
    fprintf(p->out, "BEGIN TRANSACTION;\n");
    p->writableSchema = 0;/*ת�����ݿ�ʱ Ҫ��ס������д����������ִ���*/
    sqlite3_exec(p->db, "SAVEPOINT dump; PRAGMA writable_schema=ON", 0, 0, 0);
    p->nErr = 0;
    /*���.dump�������û�в���������Ҫ�����ݿ�ģʽ�����б��¼������*/
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
      /*����в�������ֻ�Բ�������Ӧ�ı���б��ݡ�*/
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
/*�򿪻��߹ر������л���*/
  if( c=='e' && strncmp(azArg[0], "echo", n)==0 && nArg>1 && nArg<3 ){
    p->echoOn = booleanValue(azArg[1]);
  }else
/*�˳���ǰ����*/
  if( c=='e' && strncmp(azArg[0], "exit", n)==0  && nArg==1 ){
    rc = 2;
  }else

  if( c=='e' && strncmp(azArg[0], "explain", n)==0 && nArg<3 ){
    /*������������ϲ�����ȡ�ڶ�����������ֵ������ȡ1*/
    int val = nArg>=2 ? booleanValue(azArg[1]) : 1;
    if(val == 1) {
      if(!p->explainPrev.valid) {
        p->explainPrev.valid = 1;
        p->explainPrev.mode = p->mode;
        p->explainPrev.showHeader = p->showHeader; /*��ʾ��ͷ*/
        memcpy(p->explainPrev.colWidth,p->colWidth,sizeof(p->colWidth));
      }
      /*���������������ô����Ѿ���explainģʽ�¾Ͳ�������*/
      /* We could put this code under the !p->explainValid
      ** condition so that it does not execute if we are already in
      ** explain mode. However, always executing it allows us an easy
      ** was to reset to explain mode in case the user previously
      ** did an .explain followed by a .width, .mode or .header
      ** command.
      */
      p->mode = MODE_Explain;/*����ģʽ*/
      p->showHeader = 1;
      memset(p->colWidth,0,ArraySize(p->colWidth));/*��ʼ����������*/
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
/*�򿪻��߹رձ�ͷ��ʾ*/
  if( c=='h' && (strncmp(azArg[0], "header", n)==0 ||
                 strncmp(azArg[0], "headers", n)==0) && nArg>1 && nArg<3 ){
    p->showHeader = booleanValue(azArg[1]);
  }else
/*��ʾ�����ĵ�*/
  if( c=='h' && strncmp(azArg[0], "help", n)==0 ){
    fprintf(stderr,"%s",zHelp);
    if( HAS_TIMER ){
      fprintf(stderr,"%s",zTimerHelp);
    }
  }else
/*����ָ�����ļ���ָ���ı�*/
  if( c=='i' && strncmp(azArg[0], "import", n)==0 && nArg==3 ){
    char *zTable = azArg[2];    /* Insert data into this table *//*�����������ݵı�*/
    char *zFile = azArg[1];     /* The file from which to extract data *//*������ȡ���ݵ��ļ�*/
    sqlite3_stmt *pStmt = NULL; /* A statement *//*����һ���վ��*/
    int nCol;                   /* Number of columns in the table *//*����������*/
    int nByte;                  /* Number of bytes in an SQL string *//*һ��SQL���ı�����*/
    int i, j;                   /* Loop counters */
    int nSep;                   /* Number of bytes in p->separator[] */
    char *zSql;                 /* An SQL statement *//*һ��SQL�����*/
    char *zLine;                /* A single line of input from the file *//*�ļ�����*/
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
    /* sqlite3_step()����SQLITE_ROW�󣬸ú������ص�ǰ��¼������������Ҫ����������л�α�*/
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
      }/*sqlite3_bind_text�ĵڶ�������Ϊ��ţ���1��ʼ��������������Ϊ�ַ���ֵ�����ĸ�����Ϊ�ַ������ȡ�
      ���������Ϊһ������ָ�룬SQLITE3ִ���������ص��˺�����ͨ�������ͷ��ַ���ռ�õ��ڴ档*/
      sqlite3_step(pStmt);/* ִ�����*/
      rc = sqlite3_reset(pStmt);
      free(zLine);
      if( rc!=SQLITE_OK ){/* ���������Ϣ*/
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
    struct callback_data data;/*������Խṹ��*/
    char *zErrMsg = 0;/*���������Ϣ��ָ��*/
    open_db(p);/*�����ݿ�*/
    memcpy(&data, p, sizeof(data));
    data.showHeader = 0;/*�򿪱�ͷ��ʾ*/
    data.mode = MODE_List;
    if( nArg==1 ){/*û�в���ʱ */
      rc = sqlite3_exec(p->db,
        "SELECT name FROM sqlite_master "
        "WHERE type='index' AND name NOT LIKE 'sqlite_%' "
        "UNION ALL "
        "SELECT name FROM sqlite_temp_master "
        "WHERE type='index' "/*��ʾ��������*/
        "ORDER BY 1",
        callback, &data, &zErrMsg
      );
    }else{{/*�в���ʱ����ʾ��Ӧ�������*/
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
    if( zErrMsg ){/*������������������Ϣ*/
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
/*����һ����չ��*/
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
/*�򿪻�ر���־���ܣ�FILE����Ϊ��׼���stdout�����׼�������stderr*/
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
      p->mode = MODE_Line;/*���õ�Lineģʽ*/
    }else if( (n2==6 && strncmp(azArg[1],"column",n2)==0)
              ||
              (n2==7 && strncmp(azArg[1],"columns",n2)==0) ){
      p->mode = MODE_Column;/*���õ�Columnģʽ*/
    }else if( n2==4 && strncmp(azArg[1],"list",n2)==0 ){
      p->mode = MODE_List;/*���õ�Listģʽ*/
    }else if( n2==4 && strncmp(azArg[1],"html",n2)==0 ){
      p->mode = MODE_Html;/*���õ�Htmlģʽ*/
    }else if( n2==3 && strncmp(azArg[1],"tcl",n2)==0 ){
      p->mode = MODE_Tcl;/*���õ�Tclģʽ*/
    }else if( n2==3 && strncmp(azArg[1],"csv",n2)==0 ){
      p->mode = MODE_Csv;/*���õ�Csvģʽ*/
      sqlite3_snprintf(sizeof(p->separator), p->separator, ",");
    }else if( n2==4 && strncmp(azArg[1],"tabs",n2)==0 ){
      p->mode = MODE_List; /*���õ�Listģʽ*/
      sqlite3_snprintf(sizeof(p->separator), p->separator, "\t");
    }else if( n2==6 && strncmp(azArg[1],"insert",n2)==0 ){
      p->mode = MODE_Insert;/*���õ�Insertģʽ*/
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
/*��ָ���Ĵ����������NULL��*/
  if( c=='n' && strncmp(azArg[0], "nullvalue", n)==0 && nArg==2 ) {
    sqlite3_snprintf(sizeof(p->nullvalue), p->nullvalue,
                     "%.*s", (int)ArraySize(p->nullvalue)-1, azArg[1]);
  }else
/*����ǰ�������������ض��򵽱�׼���(��Ļ)��*/
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
/*�滻Ĭ�ϵı�׼��ʾ��*/
  if( c=='p' && strncmp(azArg[0], "prompt", n)==0 && (nArg==2 || nArg==3)){
    if( nArg >= 2) {
      strncpy(mainPrompt,azArg[1],(int)ArraySize(mainPrompt)-1);
    }
    if( nArg >= 3) {
      strncpy(continuePrompt,azArg[2],(int)ArraySize(continuePrompt)-1);
    }
  }else
/*ֹͣ��ǰ����*/
  if( c=='q' && strncmp(azArg[0], "quit", n)==0 && nArg==1 ){
    rc = 2;
  }else
/*ִ��ָ���ļ��ڵ�sql���*/
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
/*��ָ�����ļ���ԭ���ݿ⣬ȱʡΪmain���ݿ⣬��ʱҲ����ָ���������ݿ���
��ָ�������ݿ��Ϊ��ǰ���ӵ�attached���ݿ⡣*/
  if( c=='r' && n>=3 && strncmp(azArg[0], "restore", n)==0 && nArg>1 && nArg<4){
    const char *zSrcFile; /*��Ҫ����ԭ��Դ���ݿ�*/
    const char *zDb;/*ָ���ģ�Ŀ�����ݿ⣬ȱʡΪMAIN*/
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
    rc = sqlite3_open(zSrcFile, &pSrc);/*����Ҫ����ԭ��Դ���ݿ�*/
    if( rc!=SQLITE_OK ){
      fprintf(stderr, "Error: cannot open \"%s\"\n", zSrcFile);
      sqlite3_close(pSrc);
      return 1;
    }
    open_db(p);
    /* ��һ������ΪĿ�����ݿ⣬����������ΪԴ���ݿ⣬����ָ��Դ���ݿ��ָ��*/
    pBackup = sqlite3_backup_init(p->db, zDb, pSrc, "main");
    if( pBackup==0 ){
      fprintf(stderr, "Error: %s\n", sqlite3_errmsg(p->db));
      sqlite3_close(pSrc);
      return 1;
    }
    while( (rc = sqlite3_backup_step(pBackup,100))==SQLITE_OK/*��ʼ����*/
          || rc==SQLITE_BUSY  ){
      if( rc==SQLITE_BUSY ){
        if( nTimeout++ >= 3 ) break; /*��������ļ�������������֮���ն�*/
        sqlite3_sleep(100);  /*�߳̽���������ִͣ��100����*/
      }
    }/*������Ϻ��ͷſռ�*/
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
/*������Ϻ��ͷſռ�*/���ݳɹ����ͷſռ�ر����ݿ⣬ʧ�ܻᱣ�������Ϣ*/
  if( c=='s' && strncmp(azArg[0], "schema", n)==0 && nArg<3 ){
    struct callback_data data;
    char *zErrMsg = 0;
    /*ȷ�����ݿ��Ǵ򿪵ġ�������ǣ�����򿪡�
    ������ݿ��޷��򿪣����������Ϣ���˳�*/
    open_db(p);	
    /* ��ʼ������,���ڴ���sqlite3_backup����
    �ö�����Ϊ���ο��������ľ��������������������*/
    pBackup = sqlite3_backup_init(p->db, zDb, pSrc, "main");
    if( pBackup==0 ){/*��ʼ������ʧ��*/
      fprintf(stderr, "Error: %s\n", sqlite3_errmsg(p->db));/*�Ѵ�����Ϣ��Ҫ���ʽ�����stderr�ļ���*/
      sqlite3_close(pSrc);/*�ر�pSrcָ��Ŀռ�*/
      return 1;
    }
    while( (rc = sqlite3_backup_step(pBackup,100))==SQLITE_OK /*�ж�sqlite3_backup_step �Ƿ�ɹ�����
100��ҳ�档*/
          || rc==SQLITE_BUSY  ){
      if( rc==SQLITE_BUSY ) {  
        if( nTimeout++ >= 3 ) break;/*��������֮�����ݿ��ļ�һֱ��������������ǰ����*/ 
        sqlite3_sleep(100);  /*sqlite3_sleep ����ʹ��ǰ�߳���ִͣ��100���롣*/
      }
    }
    sqlite3_backup_finish(pBackup); /*�ͷ���pBackup �������������Դ��*/
    if( rc==SQLITE_DONE ){ /*�ж�sqlite3_backup_step �Ƿ�������б��ݲ�����*/
      rc = 0;
    }else if( rc==SQLITE_BUSY || rc==SQLITE_LOCKED ){/*��database connection ��д�뵽Դ���ݿ�
ʱ,sqlite3_backup_step �ͻ᷵��SQLITE_LOCKED */
      fprintf(stderr, "Error: source database is busy\n");/*�Ѵ�����Ϣ�����stderr��*/
      rc = 1;
    }else{
      fprintf(stderr, "Error: %s\n", sqlite3_errmsg(p->db));/*�Ѵ�����Ϣ����ʽҪ�������stderr��*/
      rc = 1;
    }
    sqlite3_close(pSrc);/*sqlite3�Ķ��󱻳ɹ����ٲ���������ص���Դ���ͷš�*/
  }else
  /*�ж��Ƿ�������.schema����
  ��������Եõ�һ�������ͼ�Ķ���(DDL)��䡣*/
  if( c=='s' && strncmp(azArg[0], "schema", n)==0 && nArg<3 ){
    struct callback_data data;/*���Բ���*/
    char *zErrMsg = 0;
    open_db(p);/*�����ݿ�*/
    memcpy(&data, p, sizeof(data));/* ��p��ָ���ڴ��ַ����ʼλ�ÿ�ʼ����sizeof(data)���ֽ�data���ڴ��
ַ����ʼλ���С�*/
    data.showHeader = 0;
    data.mode = MODE_Semi;/*���궨���MODE_Semi��ֵ �����ṹ�����*/
    if( nArg>1 ){
      int i;
      for(i=0; azArg[1][i]; i++) azArg[1][i] = ToLower(azArg[1][i]);/* ���ַ�ת����Сд��ĸ,����ĸ�ַ�������
���� */
      if( strcmp(azArg[1],"sqlite_master")==0 ){/*azArg[1]ָ���ַ�����Ҫ���ַ���ƥ�䣬�������Ӧ��*/
        char *new_argv[2], *new_colv[2];/*��������ָ������*/
        new_argv[0] = "CREATE TABLE sqlite_master (\n"/*SQL��䣬����sqlite_master��*/
                      "  type text,\n"
                      "  name text,\n"
                      "  tbl_name text,\n"
                      "  rootpage integer,\n"
                      "  sql text\n"
                      ")";
        new_argv[1] = 0;
        new_colv[0] = "sql";
        new_colv[1] = 0;
        callback(&data, 1, new_argv, new_colv);/*�ص�����������ʾ��ѯ�������ͬ*/
        rc = SQLITE_OK;
      }else if( strcmp(azArg[1],"sqlite_temp_master")==0 ){/*azArg[1]ָ���ַ�����Ҫ���ַ���ƥ�䣬�������
Ӧ��*/
        char *new_argv[2], *new_colv[2];/*��������ָ������*/
        new_argv[0] = "CREATE TEMP TABLE sqlite_temp_master (\n"/*��SQL��丳��new_argv[0]����*/
                      "  type text,\n"
                      "  name text,\n"
                      "  tbl_name text,\n"
                      "  rootpage integer,\n"
                      "  sql text\n"
                      ")";
        new_argv[1] = 0;
        new_colv[0] = "sql";
        new_colv[1] = 0;
        callback(&data, 1, new_argv, new_colv);/*�ص�����������ʾ��ѯ�������ͬ*/
        rc = SQLITE_OK;
      }else{
        zShellStatic = azArg[1];/*��ָ̬��zShellStaticָ����ָ��azArg[1]���ڴ�ռ�*/
        rc = sqlite3_exec(p->db,/*��pָ��ָ������ݿ�ִ������SQL���*/
          "SELECT sql FROM "
          "  (SELECT sql sql, type type, tbl_name tbl_name, name name, rowid x"
          "     FROM sqlite_master UNION ALL"
          "   SELECT sql, type, tbl_name, name, rowid FROM sqlite_temp_master) "
          "WHERE lower(tbl_name) LIKE shellstatic()"
          "  AND type!='meta' AND sql NOTNULL "
          "ORDER BY substr(type,2,1), "
                  " CASE type WHEN 'view' THEN rowid ELSE name END",
          callback(&data, &zErrMsg);/*��ʾ��ѯ���*/
        zShellStatic = 0;
      }
    }else{
      rc = sqlite3_exec(p->db,/*��db���ݿ�ִ������SQL���*/
         "SELECT sql FROM "
         "  (SELECT sql sql, type type, tbl_name tbl_name, name name, rowid x"
         "     FROM sqlite_master UNION ALL"
         "   SELECT sql, type, tbl_name, name, rowid FROM sqlite_temp_master) "
         "WHERE type!='meta' AND sql NOTNULL AND name NOT LIKE 'sqlite_%'"
         "ORDER BY substr(type,2,1),"
                  " CASE type WHEN 'view' THEN rowid ELSE name END",
         callback( &data, &zErrMsg);
    }
    if( zErrMsg ){/*����Ϊ�գ������zErrMsg�е����ݵ�stderr�ļ���*/
      fprintf(stderr,"Error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);/*�ͷ�zErrMsg���ڴ�ռ�*/
      rc = 1;
    }else if( rc != SQLITE_OK ){/*�Ƿ�ɹ����*/
      fprintf(stderr,"Error: querying schema information\n");
      rc = 1;
    }else{
      rc = 0;
    }
  }else
  if( c=='s' && strncmp(azArg[0], "separator", n)==0 && nArg==2 ){ /*�ж��Ƿ�������.separator ����*/
/*�� azArg[1]����format��ʽ�����ַ�����Ȼ����������ɹ��򷵻���д����ַ������ȣ��������򷵻ظ�ֵ��*/
    sqlite3_snprintf(sizeof(p->separator), p->separator,
                     "%.*s", (int)sizeof(p->separator)-1, azArg[1]);
  }else
  if( c=='s' && strncmp(azArg[0], "show", n)==0 && nArg==1 ){/*�ж��Ƿ�������.show ����*/
    int i;
    fprintf(p->out,"%9.9s: %s\n","echo", p->echoOn ? "on" : "off"); /*���Կ���*/
    fprintf(p->out,"%9.9s: %s\n","explain", p->explainPrev.valid ? "on" :"off");
    fprintf(p->out,"%9.9s: %s\n","headers", p->showHeader ? "on" : "off");/*�Ƿ�򿪱�ͷ*/
    fprintf(p->out,"%9.9s: %s\n","mode", modeDescr[p->mode]);/*mode����������ý�����ݵļ������
��ʽ,��Щ��ʽ�����modeDescr������*/
    fprintf(p->out,"%9.9s: ", "nullvalue");/*��ֵ��ʾ*/
      output_c_string(p->out, p->nullvalue);/*����C��TCL���ù���,����������ַ�����*/
      fprintf(p->out, "\n");
    fprintf(p->out,"%9.9s: %s\n","output",
            strlen30(p->outfile) ? p->outfile : "stdout");/*��׼���*/
    fprintf(p->out,"%9.9s: ", "separator");
      output_c_string(p->out, p->separator);/*����Ӧ�ָ�������ַ���*/
      fprintf(p->out, "\n");
    fprintf(p->out,"%9.9s: %s\n","stats", p->statsOn ? "on" : "off");
    fprintf(p->out,"%9.9s: ","width");
    for (i=0;i<(int)ArraySize(p->colWidth) && p->colWidth[i] != 0;i++) {/*������ÿ���п�Ϊ0*/
      fprintf(p->out,"%d ",p->colWidth[i]);/*����п�*/
    }
    fprintf(p->out,"\n");
  }else

  if( c=='s' && strncmp(azArg[0], "stats", n)==0 && nArg>1 && nArg<3 ){/*�ж��Ƿ�����stats����*/
    p->statsOn = booleanValue(azArg[1]);/*��azArg[1]��ֵת��Ϊ����ֵ*/
  }else

  if( c=='t' && n>1 && strncmp(azArg[0], "tables", n)==0 && nArg<3 ){ /*�ж��Ƿ�����tables����*/
    sqlite3_stmt *pStmt;/*����һ��ָ��*/
    char **azResult;/*��ά�����Ž��*/
    int nRow, nAlloc;
    char *zSql = 0;
    int ii;
    open_db(p);/*��pָ������ݿ�*/
/* 
**���nbyte����С���㣬��ZSQL����ȡ����һ������ֹ��
**���nByte�ǷǸ��ģ���ô���Ǵ�ZSQL��������ֽ���
**�ú��������б�������ʾ��
**int sqlite3_prepare_v2(
  **sqlite3 *db,            /* Database handle */
  **const char *zSql,       /* SQL statement, UTF-8 encoded */
  **int nByte,              /* Maximum length of zSql in bytes. */
  **sqlite3_stmt **ppStmt,  /* OUT: Statement handle */
  **const char **pzTail     /* OUT: Pointer to unused portion of zSql */
**);
*/
    rc = sqlite3_prepare_v2(p->db, "PRAGMA database_list", -1, &pStmt, 0);/*��������ֵ����rc*/
    if( rc ) return rc;
    zSql = sqlite3_mprintf(/*�����ѯ������ڴ�ռ���*/
        "SELECT name FROM sqlite_master"
        " WHERE type IN ('table','view')"
        "   AND name NOT LIKE 'sqlite_%%'"
        "   AND name LIKE ?1");
    while( sqlite3_step(pStmt)==SQLITE_ROW ){/*����sqlite_step ��ȡ������е�һ�У�������������α�λ
���ƶ������������һ��*/
      const char *zDbName = (const char*)sqlite3_column_text(pStmt, 1);/**zDbNameָ��ָ�򷵻�ֵΪ��
����ָ��ĺ����ռ�*/
      if( zDbName==0 || strcmp(zDbName,"main")==0 ) continue;
      if( strcmp(zDbName,"temp")==0 ){
        zSql = sqlite3_mprintf(/*����ѯ���д��zSqlָ����ڴ�ռ���*/
                 "%z UNION ALL "
                 "SELECT 'temp.' || name FROM sqlite_temp_master"
                 " WHERE type IN ('table','view')"/*ѡ������Ϊtype��'table'��'view'ֵ*/
                 "   AND name NOT LIKE 'sqlite_%%'"/*������sqlite_��ͷ��name*/
                 "   AND name LIKE ?1", zSql);
      }else{
        zSql = sqlite3_mprintf(/*�����ѯ������ڴ�ռ���*/
                 "%z UNION ALL "
                 "SELECT '%q.' || name FROM \"%w\".sqlite_master"
                 " WHERE type IN ('table','view')"
                 "   AND name NOT LIKE 'sqlite_%%'"
                 "   AND name LIKE ?1", zSql, zDbName, zDbName);
      }
    }
    sqlite3_finalize(pStmt);/*������pStmt������*/
    zSql = sqlite3_mprintf("%z ORDER BY 1", zSql);/*�����������*/
    rc = sqlite3_prepare_v2(p->db, zSql, -1, &pStmt, 0);
    sqlite3_free(zSql);/*�ͷ�zSqlָ��ĵ�ַ�ռ�*/
    if( rc ) return rc;/*rc��Ϊ�գ��򷵻���ֵ*/
    nRow = nAlloc = 0;
    azResult = 0;
    if( nArg>1 ){
      sqlite3_bind_text(pStmt, 1, azArg[1], -1, SQLITE_TRANSIENT);/*���ò���*/
    }else{
      sqlite3_bind_text(pStmt, 1, "%", -1, SQLITE_STATIC);/*���ò���*/
    }
    while( sqlite3_step(pStmt)==SQLITE_ROW ){/*��ǰ�����ķ���ֵΪSQLITE_ROW������ʾ���е�������׼��
�ô�����*/
      if( nRow>=nAlloc ){
        char **azNew;
        int n = nAlloc*2 + 10;
        azNew = sqlite3_realloc(azResult, sizeof(azResult[0])*n);/*���·���azResult���ڴ�ռ�,����sizeof
(azResult[0])*n���ֽ�*/
        if( azNew==0 ){/*����ʧ�ܣ�д�������Ϣ*/
          fprintf(stderr, "Error: out of memory\n");/*�ڴ治��*/
          break;
        }
        nAlloc = n;
        azResult = azNew;/*azResult ָ���µ��ڴ浥Ԫ*/
      }
      azResult[nRow] = sqlite3_mprintf("%s", sqlite3_column_text(pStmt, 0));
      if( azResult[nRow] ) nRow++;/*���δָ��գ��������һ���ַ�*/
    }
    sqlite3_finalize(pStmt);/*ɾ����pStmt������*/        
    if( nRow>0 ){
      int len, maxlen = 0;
      int i, j;
      int nPrintCol, nPrintRow;
      for(i=0; i<nRow; i++){
        len = strlen30(azResult[i]);/*����������ֵ����len����*/
        if( len>maxlen ) maxlen = len;
      }
      nPrintCol = 80/(maxlen+2);/*ͳ�ƴ�ӡ���п��*/
      if( nPrintCol<1 ) nPrintCol = 1;/*����һ�У���һ�д���*/
      nPrintRow = (nRow + nPrintCol - 1)/nPrintCol;/*ͳ�ƴ�ӡ���и�*/
      for(i=0; i<nPrintRow; i++){
        for(j=i; j<nRow; j+=nPrintRow){
          char *zSp = j<nPrintRow ? "" : "  ";/*�Ƿ��ӡ�ո�*/
          printf("%s%-*s", zSp, maxlen, azResult[j] ? azResult[j] : "");/*������*/
        }
        printf("\n");/*ÿ���һ�У����д���*/
      }
    }
    for(ii=0; ii<nRow; ii++) sqlite3_free(azResult[ii]);/*�ͷ��ڴ�ռ�*/
    sqlite3_free(azResult);/*�ͷŲ�ѯ������ڴ�ռ�*/
  }else

  if( c=='t' && n>=8 && strncmp(azArg[0], "testctrl", n)==0 && nArg>=2 ){ /*�ж��Ƿ�����.testctrl ����
*/
    static const struct {/*��̬���ṹ��*/
       const char *zCtrlName;   /* ָ������ָ�� */
       int ctrlCode;            /* ����һ�����ʹ����������Щ�ַ����Ѿ��궨��*/
    } aCtrl[] = {/*�ṹ�����鳣�������������ֽṹ*/
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
    open_db(p);/*�����ݿ�*/
    /* ��testctrl�ı�ѡ��ת��Ϊ��ֵ*/
    n = strlen30(azArg[1]); /*ͳ��azArg[1]���ַ�������*/
    for(i=0; i<(int)(sizeof(aCtrl)/sizeof(aCtrl[0])); i++){
      if( strncmp(azArg[1], aCtrl[i].zCtrlName, n)==0 ){/*�Ƚ������������Ƿ���ͬ*/
        if( testctrl<0 ){
          testctrl = aCtrl[i].ctrlCode;/*��aCtrl[i]�����ʹ���ֵ����testctrl*/
        }else{
          fprintf(stderr, "ambiguous option name: \"%s\"\n", azArg[1]);
          testctrl = -1;
          break;
        }
      }
    }
    if( testctrl<0 ) testctrl = atoi(azArg[1]);/*���ַ�ת���ɳ�����������testctrl������*/
    if( (testctrl<SQLITE_TESTCTRL_FIRST) || (testctrl>SQLITE_TESTCTRL_LAST) ){  /*���testctrlС��5 ����
����24��*/
      fprintf(stderr,"Error: invalid testctrl option: %s\n", azArg[1]);/*��Ч��testctrlѡ��*/
    }else{
      switch(testctrl){/*����testctrl��ֵ��ѡ��case��֧���ִ��*/
        /* sqlite3_test_control(int, db, int) *//*�ú����������������ֱ������͡����ݿ�ָ�롢����*/
        case SQLITE_TESTCTRL_OPTIMIZATIONS:/*#define SQLITE_TESTCTRL_OPTIMIZATIONS  15*/
        case SQLITE_TESTCTRL_RESERVE:          /* #define SQLITE_TESTCTRL_RESERVE  14*/
          if( nArg==3 ){
            int opt = (int)strtol(azArg[2], 0, 0); /*��azArg[2]�ַ����ݰ�ʮ����ת���ɳ���������ͬʱ������������
������ֹʱ�򷵻�0*/      
            rc = sqlite3_test_control(testctrl, p->db, opt);/*����ִ��״̬*/
            printf("%d (0x%08x)\n", rc, rc);
          } else {
            fprintf(stderr,"Error: testctrl %s takes a single int option\n",/*�õ�һ��������*/
                    azArg[1]);
          }
          break;
        /* sqlite3_test_control(int) *//*sqlite3_test_control����һ�����Ͳ���*/
        case SQLITE_TESTCTRL_PRNG_SAVE:/*#define SQLITE_TESTCTRL_PRNG_SAVE  5 */        
        case SQLITE_TESTCTRL_PRNG_RESTORE:  /*#define SQLITE_TESTCTRL_PRNG_RESTORE  6 */         
        case SQLITE_TESTCTRL_PRNG_RESET:/*#define SQLITE_TESTCTRL_PRNG_RESET  7 */   
          if( nArg==2 ){
            rc = sqlite3_test_control(testctrl);/*���ڷ���SQLite �ڲ�״̬*/
            printf("%d (0x%08x)\n", rc, rc);
          } else {
            fprintf(stderr,"Error: testctrl %s takes no options\n", azArg[1]);/*û�еõ�ѡ��*/
          }
          break;
        /* sqlite3_test_control(int, uint) *//*sqlite3_test_control����һ�����Ͳ�����һ���޷�������*/
        case SQLITE_TESTCTRL_PENDING_BYTE: /*#define SQLITE_TESTCTRL_PENDING_BYTE   11 */      
          if( nArg==3 ){
            unsigned int opt = (unsigned int)atoi(azArg[2]);/*���ַ���ת��������������ǿ��ת��Ϊ�޷������͸�
��opt����*/        
            rc = sqlite3_test_control(testctrl, opt);/*���ڷ���SQLite �ڲ�״̬*/
            printf("%d (0x%08x)\n", rc, rc);
          } else {
            fprintf(stderr,"Error: testctrl %s takes a single unsigned"/*�õ�һ���޷���������*/
                           " int option\n", azArg[1]);
          }
          break;
          
        /* sqlite3_test_control(int, int) *//*sqlite3_test_control�����������Ͳ���*/
        case SQLITE_TESTCTRL_ASSERT:/*#define SQLITE_TESTCTRL_ASSERT   12  */            
        case SQLITE_TESTCTRL_ALWAYS: /*#define SQLITE_TESTCTRL_ALWAYS  13  */           
          if( nArg==3 ){
            int opt = atoi(azArg[2]);  /*���ַ���ת��������������ֵ��opt ����*/    
            rc = sqlite3_test_control(testctrl, opt);
            printf("%d (0x%08x)\n", rc, rc);
          } else {
            fprintf(stderr,"Error: testctrl %s takes a single int option\n",/*�õ�һ��������*/
                            azArg[1]);
          }
          break;
/*����sqlite3_test_control�����ӿ����ڶ�����SQLite���ڲ�״̬����ֲ��SQLite�Ĵ�����Ϣ���ڲ���Ŀ�ġ�
��һ��������һ�������룬��ȷ�����еĺ��������ĸ���������Ͳ�����*/
        /* sqlite3_test_control(int, char *) *//*sqlite3_test_control����һ�����Ͳ�����һ��ָ���ַ��͵�ָ��*/
#ifdef SQLITE_N_KEYWORD/*����궨����SQLITE_N_KEYWORD����ִ�����²���*/
        case SQLITE_TESTCTRL_ISKEYWORD:           
          if( nArg==3 ){
            const char *opt = azArg[2];/*ָ��*optָ���ַ�������azArg[2]*/            
            rc = sqlite3_test_control(testctrl, opt);
            printf("%d (0x%08x)\n", rc, rc);
          } else {
            fprintf(stderr,"Error: testctrl %s takes a single char * option\n",/*�õ�һ��ָ���ַ�����ָ��*/
                            azArg[1]);
          }
          break;
#endif

        case SQLITE_TESTCTRL_BITVEC_TEST:         
        case SQLITE_TESTCTRL_FAULT_INSTALL:       
        case SQLITE_TESTCTRL_BENIGN_MALLOC_HOOKS: 
        case SQLITE_TESTCTRL_SCRATCHMALLOC:       
        default:
          fprintf(stderr,"Error: CLI support for testctrl %s not implemented\n",/*�����н�����δʵ�ֶ����֧��*/
                  azArg[1]);
          break;
      }
    }
  }else
  if( c=='t' && n>4 && strncmp(azArg[0], "timeout", n)==0 && nArg==2 ){/*�ж��Ƿ�����.timeout����
*/
    open_db(p);/*�����ݿ�*/
    sqlite3_busy_timeout(p->db, atoi(azArg[1]));/*�ó�������һ��æ����handler
��������ʱ������һ��ָ����ʱ����������С�ڻ��������ر�����ռ�ߴ������*/
  }else
    
  if( HAS_TIMER && c=='t' && n>=5 && strncmp(azArg[0], "timer", n)==0{/*�ж��Ƿ�����.time����*/
   && nArg==2
  ){
    enableTimer = booleanValue(azArg[1]);/*��azArg[1]ת��Ϊ����ֵ��ֵ��enableTimer*/
  }else
  
  if( c=='t' && strncmp(azArg[0], "trace", n)==0 && nArg>1 ){/*�ж��Ƿ�����.trace����*/
    open_db(p);
    output_file_close(p->traceOut);/*�ر��ļ�*/
    p->traceOut = output_file_open(azArg[1]);/*���ļ�*/
#if !defined(SQLITE_OMIT_TRACE) && !defined(SQLITE_OMIT_FLOATING_POINT)
    if( p->traceOut==0 ){
      sqlite3_trace(p->db, 0, 0);/*���ڸ��ٺͷ�����SQL����ִ�лص�������*/
    }else{
      sqlite3_trace(p->db, sql_trace_callback, p->traceOut);/*���ڸ��ٺͷ�����SQL����ִ�лص�������*/
    }
#endif
  }else

  if( c=='v' && strncmp(azArg[0], "version", n)==0 ){/*�ж��Ƿ�����.version ����*/
    printf("SQLite %s %s\n" /*extra-version-info*/,
        sqlite3_libversion(), sqlite3_sourceid());/*sqlite3_libversion��������һ��ָ��sqlite3_version[]�ַ���
������*/
  }else

  if( c=='v' && strncmp(azArg[0], "vfsname", n)==0 ){/*�ж��Ƿ�����.vfsname ����*/
    const char *zDbName = nArg==2 ? azArg[1] : "main";/*���nArg=2��ָ��ָ����azArg[1]������ָ��
�ַ���"main"*/
    char *zVfsName = 0;
    if( p->db ){
      sqlite3_file_control(p->db, zDbName, SQLITE_FCNTL_VFSNAME, &zVfsName);
      if( zVfsName ){
        printf("%s\n", zVfsName);
        sqlite3_free(zVfsName);/*�ͷ�zVfsName�ڴ�ռ�*/
      }
    }
  }else
  if( c=='w' && strncmp(azArg[0], "width", n)==0 && nArg>1 ){/*�ж��Ƿ�����.width����*/
    int j;
    assert( nArg<=ArraySize(azArg) );/*assert ����ֻ����SQLite ��SQLITE_DEBUG ����ʱ�Ż����á�*/
    for(j=1; j<nArg && j<ArraySize(p->colWidth); j++){
      p->colWidth[j-1] = atoi(azArg[j]);/*��azArg[j]ת��Ϊ����*/
    }
  }else
  {
    fprintf(stderr, "Error: unknown command or invalid arguments: "
      " \"%s\". Enter \".help\" for help\n", azArg[0]);/*����ָ�����Ч����*/
    rc = 1;
  }
  return rc;/*����rc��ֵ*/
}


static int _contains_semicolon(const char *z, int N){/*���ֺ� �������ַ���z�ĵ�N��λ���ϣ������򷵻�1��*/
  int i;
  for(i=0; i<N; i++){  if( z[i]==';' ) return 1; }
  return 0;
}


static int _all_whitespace(const char *z){/* �������Ƿ�Ϊ��*/
  for(; *z; z++){
    if( IsSpace(z[0]) ) continue;/*�ж�z[0]�����Ƿ�Ϊ��*/
    if( *z=='/' && z[1]=='*' ){/*zָ��/�����ҵڶ����ַ�Ϊ��*��*/
      z += 2;/*z=z+2;*/
      while( *z && (*z!='*' || z[1]!='/') ){ z++; }/*z��ָ��*�����ߵڶ����ַ���Ϊ��/��*/
      if( *z==0 ) return 0;
      z++;
      continue;
    }
    if( *z=='-' && z[1]=='-' ){/*zָ��-�����ҵڶ����ַ�Ϊ��-��*/
      z += 2;
      while( *z && *z!='\n' ){ z++; }/*ָ��z��ָ��ջ��ַ�����β*/
      if( *z==0 ) return 1;
      continue;
    }
    return 0;
  }
  return 1;
}

/*
����������һ��SQL�����β����������һ���ֺţ��򷵻�TRUE��
��SQL Server���ġ�go��������Ϊ��Oracle��/����
*/
static int _is_command_terminator(const char *zLine){
  while( IsSpace(zLine[0]) ){ zLine++; };
  if( zLine[0]=='/' && _all_whitespace(&zLine[1]) ){/*���鿪ʼΪ"/",֮��Ϊ��*/
    return 1;  /* Oracle */
  }
  if( ToLower(zLine[0])=='g' && ToLower(zLine[1])=='o' /*���鿪ʼΪ"go",֮��Ϊ��*/
         && _all_whitespace(&zLine[2]) ){
    return 1;  /* SQL Server */
  }
  return 0;
}

/*���zSql��һ��������SQL��䣬����true��
�������һ���ַ�����C���ע�͵��м����������false��*/
static int _is_complete(char *zSql, int nSql){
  int rc;
  if( zSql==0 ) return 1;
  zSql[nSql] = ';';
  zSql[nSql+1] = 0;
  rc = sqlite3_complete(zSql);
  zSql[nSql] = 0;
  return rc;
}

/*��* in �ж�ȡ���벢����
���* in==0����������- �û��������ݡ����� ��һ���ļ����豸���롣
ֻ�е������ǽ���ʽ�ģ���������ʾ����ʷ��¼�Żᱻ���档
һ���ж��źŽ����¸ó��������˳������������ǽ���ʽ�ġ�
���ش����������*/
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
    fflush(p->out);/*�����д����������Ҫ��������������������ݽ�������д��ʱ*/
    free(zLine);/*�ͷ�zLine�ڴ�ռ�*/
    zLine = one_input_line(zSql, in);
    if( zLine==0 ){
      /* End of input */
      if( stdin_is_interactive ) printf("\n");/*����ʽ��׼�����Ի��н���*/
      break;
    }
    if( seenInterrupt ){/*�ж���Ϣ���յ�������ֵΪtrue*/
      if( in!=0 ) break;
      seenInterrupt = 0;
    }
    lineno++;
    if( (zSql==0 || zSql[0]==0) && _all_whitespace(zLine) ) continue;
    if( zLine && zLine[0]=='.' && nSql==0 ){
      if( p->echoOn ) printf("%s\n", zLine);/*ִ�л��Բ���*/
      rc = do_meta_command(zLine, p);/*����ִ��״̬��rc*/
      if( rc==2 ){ /* exit requested */
        break;
      }else if( rc ){/*���rc��Ϊ0�������������1*/
        errCnt++;
      }
      continue;
    }
    if( _is_command_terminator(zLine) && _is_complete(zSql, nSql) ){
      memcpy(zLine,";",2);/*��zLine��ָ���ڴ��ַ����ʼλ�ÿ�ʼ����2���ֽڵ��ַ����С�*/
    }
    nSqlPrior = nSql;
    if( zSql==0 ){
      int i;
      for(i=0; zLine[i] && IsSpace(zLine[i]); i++){}
      if( zLine[i]!=0 ){
        nSql = strlen30(zLine);/*ͳ��zLine�ַ�������*/
        zSql = malloc( nSql+3 );/*ΪzSql��̬�����ڴ�ռ�*/
        if( zSql==0 ){
          fprintf(stderr, "Error: out of memory\n");
          exit(1);
        }
        memcpy(zSql, zLine, nSql+1);/*��zLine��ָ���ڴ��ַ����ʼλ�ÿ�ʼ����2���ֽڵ��ַ����С�*/
        startline = lineno;
      }
    }else{
      int len = strlen30(zLine);
      zSql = realloc( zSql, nSql + len + 4 );/*���·����ڴ�ռ䣬������·���ɹ��򷵻�ָ�򱻷����ڴ��ָ�룬���򷵻ؿ�ָ��NULL��*/
      if( zSql==0 ){
        fprintf(stderr,"Error: out of memory\n");
        exit(1);
      }
      zSql[nSql++] = '\n';
      memcpy(&zSql[nSql], zLine, len+1);/*��zLine��ָ���ڴ��ַ����ʼλ�ÿ�ʼ����2���ֽڵ��ַ����С�*/
      nSql += len;/* nSql�Լ�len��ֵ*/
    }
    if( zSql && _contains_semicolon(&zSql[nSqlPrior], nSql-nSqlPrior)
                && sqlite3_complete(zSql) ){
      p->cnt = 0;/*��ʼ������*/
      open_db(p);
      BEGIN_TIMER;/*������ʱ��*/
      rc = shell_exec(p->db, zSql, shell_callback, p, &zErrMsg);/*��sqlite3_exec()�����ǳ�����*/
      END_TIMER;/*�رն�ʱ��*/
      if( rc || zErrMsg ){
        char zPrefix[100];/*����һ��ǰ׺����*/
        if( in!=0 || !stdin_is_interactive ){
          sqlite3_snprintf(sizeof(zPrefix), zPrefix, 
                           "Error: near line %d:", startline);
        }else{
          sqlite3_snprintf(sizeof(zPrefix), zPrefix, "Error:");
        }
        if( zErrMsg!=0 ){
          fprintf(stderr, "%s %s\n", zPrefix, zErrMsg);
          sqlite3_free(zErrMsg);/*�ͷ�zErrMsg�ڴ�ռ�*/
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

/*����·�����û�����Ŀ¼������ 0 ʱ��ʾ����ĳ�����͵Ĵ���*/
static char *find_home_dir(void){
  static char *home_dir = NULL;
  if( home_dir ) return home_dir;

#if !defined(_WIN32) && !defined(WIN32) && !defined(_WIN32_WCE) && !defined(__RTP__) && !

defined(_WRS_KERNEL)/*��������ָ��������Ҫ��ı��뻷������ִ������ĳ����*/
  {
    struct passwd *pwent;
    uid_t uid = getuid();/* ����һ�����ó������ʵ�û�ID*/
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
    home_dir = getenv("USERPROFILE");/*��ȡUSERPROFILE����������ֵ*/
  }
#endif

  if (!home_dir) {
    home_dir = getenv("HOME");/*��ȡHOME����������ֵ*/
  }

#if defined(_WIN32) || defined(WIN32)
  if (!home_dir) {
    char *zDrive, *zPath;
    int n;
    zDrive = getenv("HOMEDRIVE");/*��ȡHOMEDRIVE����������ֵ*/
    zPath = getenv("HOMEPATH");/*��ȡHOMEPATH����������ֵ*/
    if( zDrive && zPath ){
      n = strlen30(zDrive) + strlen30(zPath) + 1;
      home_dir = malloc( n );/*home_dirָ��n���ֽڵ��ڴ�ռ�*/
      if( home_dir==0 ) return 0;
      sqlite3_snprintf(n, home_dir, "%s%s", zDrive, zPath);
      return home_dir;
    }
    home_dir = "c:\\";
  }
#endif

#endif /* !_WIN32_WCE */

  if( home_dir ){/*home_dirָ����ڴ�ռ䲻Ϊ��*/
    int n = strlen30(home_dir) + 1;
    char *z = malloc( n );/* zָ��n ���ֽڵ��ڴ�ռ�*/
    if( z ) memcpy(z, home_dir, n);
    home_dir = z;
  }

  return home_dir;
}

/*
��sqliterc_override�������ļ���ȡ���롣
��������ò���ΪNULL�����~/.sqliterc������
���ش����������
*/
static int process_sqliterc(/*����ֵΪ��̬����*/
  struct callback_data *p,        /* Configuration data */
  const char *sqliterc_override   /* Name of config file. NULL to use default */
){
  char *home_dir = NULL;
  const char *sqliterc = sqliterc_override;/*ָ���ַ���ָ��*/
  char *zBuf = 0;
  FILE *in = NULL;
  int rc = 0;

  if (sqliterc == NULL) {
    home_dir = find_home_dir();
    if( home_dir==0 ){
#if !defined(__RTP__) && !defined(_WRS_KERNEL)
      fprintf(stderr,"%s: Error: cannot locate your home directory\n", Argv0);/*��Argv0�еĴ�����Ϣ�����
stderr �ļ���*/
#endif
      return 1;
    }
    sqlite3_initialize();/*��ʼ��SQLite ���ݿ�*/
    zBuf = sqlite3_mprintf("%s/.sqliterc",home_dir);
    sqliterc = zBuf;
  }
  in = fopen(sqliterc,"rb");/*�ж��Ƿ�˳�����ļ�*/
  if( in ){
    if( stdin_is_interactive ){/*�жϱ�׼�����Ƿ�Ϊ����ʽ��*/
      fprintf(stderr,"-- Loading resources from %s\n",sqliterc);
    }
    rc = process_input(p,in);
    fclose(in);/*�ر�in ָ��ָ����ļ�*/
  }
  sqlite3_free(zBuf);
  return rc;
}

/*
** Show available command line options
*/
static const char zOptions[] = /*���徲̬���ַ�����*/
  "   -bail                stop after hitting an error\n"//��������ֹͣ
  "   -batch               force batch I/O\n"//������I/O
  "   -column              set output mode to 'column'\n"//���ģʽ����Ϊ���зֿ�
  "   -cmd command         run \"command\" before reading stdin\n"
  "   -csv                 set output mode to 'csv'\n"//�����ʽ����Ϊcsv
  "   -echo                print commands before execution\n"//��������
  "   -init filename       read/process named file\n"//��ʼ���ļ���
  "   -[no]header          turn headers on or off\n"//�Ƿ���ʾ��ͷ
  "   -help                show this message\n"//��ʾ������Ϣ
  "   -html                set output mode to HTML\n"//���ģʽ����ΪHTML
  "   -interactive         force interactive I/O\n"
  "   -line                set output mode to 'line'\n"
  "   -list                set output mode to 'list'\n"
#ifdef SQLITE_ENABLE_MULTIPLEX
  "   -multiplex           enable the multiplexor VFS\n"
#endif
  "   -nullvalue 'text'    set text string for NULL values\n"//���ı��ַ�������Ϊ��ֵ
  "   -separator 'x'       set output field separator (|)\n"//���÷ָ���
  "   -stats               print memory stats before each finalize\n"
  "   -version             show SQLite version\n"
  "   -vfs NAME            use NAME as the default VFS\n"//Ĭ��VFS����
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
    fprintf(stderr, "OPTIONS include:\n%s", zOptions);//��zOptions �����е������ʽҪ�������stderr��

����
  }else{
    fprintf(stderr, "Use the -help option for additional information\n");//ʹ��help����õ�������Ϣ
  }
  exit(1);
}

/*��ʼ�����ݵ�״̬��Ϣ*/
static void main_init(struct callback_data *data) {/*�����Ϊ�ṹ�����ָ��*/
  memset(data, 0, sizeof(*data));
  data->mode = MODE_List;
  memcpy(data->separator,"|", 2);
  data->showHeader = 0;
  sqlite3_config(SQLITE_CONFIG_URI, 1);
  /*sqlite3_config() ���ڸ���ȫ�ֱ�����SQLite ��ӦӦ�õľ�����Ҫ��
  ��֧��������Ӧ�ò�����������*/
  sqlite3_config(SQLITE_CONFIG_LOG, shellLog, data);
  sqlite3_snprintf(sizeof(mainPrompt), mainPrompt,"sqlite> ");
  sqlite3_snprintf(sizeof(continuePrompt), continuePrompt,"   ...> ");
  sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);
}

/*
**�����main()������shell.c��β����
**�򻯺��main()������ִ�й�����Ҫ��Ϊ5����
**1. ���û��Բ��� 
**2. ȡ���ݿ��ļ��� 
**3. �����ݿ�  
**4. ѭ������SQL���� 
**5. �ر����ݿ�
*/
int main(int argc, char **argv){
  char *zErrMsg = 0;/*����һ����Ŵ�����Ϣ��ָ��*/
  struct callback_data data;//�������Բ���
  const char *zInitFile = 0;
  char *zFirstCmd = 0;
  int i;
  int rc = 0;

  if( strcmp(sqlite3_sourceid(),SQLITE_SOURCE_ID)!=0 ){/*�Ƚ����ݿ�汾���Ƿ���ͬ*/
    fprintf(stderr, "SQLite header and source version mismatch\n%s\n%s\n",//���ݿ�汾��ƥ��
            sqlite3_sourceid(), SQLITE_SOURCE_ID);
    exit(1);
  }
  Argv0 = argv[0];
  main_init(&data);//����Ĭ�ϵĻ�����ʽ
  stdin_is_interactive = isatty(0);

  /* �����ǰ��ȷ�� ��һ����Ч���źŴ������ */
#ifdef SIGINT
  signal(SIGINT, interrupt_handler);//�û�����Ctrl-C��,�����ж��ź�
#endif

 /* 
**ͨ�������в�����λ���ݿ��ļ�������ʼ���ļ�����
**���е�malloc�ѵĴ�С��
**��ִ�е�һ�����
 */
  for(i=1; i<argc-1; i++){
    char *z;
    if( argv[i][0]!='-' ) break;// ���ĳ�еĵ�һ���ַ�����'-' ��������ǰѭ����
    z = argv[i];//ָ��Z����ָ��
    if( z[1]=='-' ) z++;
    if( strcmp(z,"-separator")==0
     || strcmp(z,"-nullvalue")==0
     || strcmp(z,"-cmd")==0
    ){//���������ַ����е�ĳ��ƥ�䣬��ִ�����³����
      i++;
    }else if( strcmp(z,"-init")==0 ){/*�Ƚ��ַ���*/
      i++;
      zInitFile = argv[i];
	  
  /* 
**�ڶ�����ʵ�δ����,
**��Ҫ���������ģʽ,
**�Ա������ܹ������ӡ��Ϣ����������sqliterc ���̣���
   */
    }else if( strcmp(z,"-batch")==0 ){/*�Ƚ��ַ���*/
      stdin_is_interactive = 0;
    }else if( strcmp(z,"-heap")==0 ){/*�Ƚ��ַ���*/
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
    /*sqlite3_config���ڸı�SQLite ��ȫ������������
** Ӧ�õľ�����Ҫ��*/
      sqlite3_config(SQLITE_CONFIG_HEAP, malloc((int)szHeap), (int)szHeap, 64);
	
#endif
#ifdef SQLITE_ENABLE_VFSTRACE
    }else if( strcmp(z,"-vfstrace")==0 ){/*�Ƚ��ַ���*/
      extern int vfstrace_register(//�����ⲿ����vfstrace_register
         const char *zTraceName,/*����һ��ָ���ַ��͵�ָ��*/
         const char *zOldVfsName,
         int (*xOut)(const char*,void*),
         void *pOutArg,
         int makeDefault
      );
      vfstrace_register("trace",0,(int(*)(const char*,void*))fputs,stderr,1);
#endif
#ifdef SQLITE_ENABLE_MULTIPLEX
    }else if( strcmp(z,"-multiplex")==0 ){/*�Ƚ��ַ���*/
      extern int sqlite3_multiple_initialize(const char*,int);//�����ⲿ����vfstrace_register
      sqlite3_multiplex_initialize(0, 1);//���س�ʼ������
#endif
    }else if( strcmp(z,"-vfs")==0 ){
      sqlite3_vfs *pVfs = sqlite3_vfs_find(argv[++i]);
      if( pVfs ){
        sqlite3_vfs_register(pVfs, 1);
      }else{
        fprintf(stderr, "no such VFS: \"%s\"\n", argv[i]);
        exit(1);
      }
    }
  }
  if( i<argc ){
    data.zDbFilename = argv[i++];//���ݿ��ļ���
  }else{
#ifndef SQLITE_OMIT_MEMORYDB
    data.zDbFilename = ":memory:";
#else
    data.zDbFilename = 0;
#endif
  }
  if( i<argc ){
    zFirstCmd = argv[i++];
  }
  if( i<argc ){
    fprintf(stderr,"%s: Error: too many options: \"%s\"\n", Argv0, argv[i]);
    fprintf(stderr,"Use -help for a list of options.\n");
    return 1;
  }
  data.out = stdout;

#ifdef SQLITE_OMIT_MEMORYDB
  if( data.zDbFilename==0 ){//data.zDbFilename��ֵΪ0����ִ�����²���
   /*��Argv0�е���Ϣ����ʽҪ��д��stderr*/
    fprintf(stderr,"%s: Error: no database filename specified\n", Argv0);
    return 1;
  }
#endif

  /* 
**������ݿ��ļ��Ѿ������������
**������ļ������ڣ��ӳٴ�����
**��ֹ�����ݿ��ļ����û������������ݿ����Ʋ�����ʱ�򱻴�����
  */
  if( access(data.zDbFilename, 0)==0 ){
    open_db(&data);
  }

  /*
**����ó�ʼ���ļ�����������ڡ�
**�����������û�и���-init ѡ�
**��Ѱ��һ����Ϊ~/.sqliterc ���ļ��������Խ��д���
  */
  rc = process_sqliterc(&data,zInitFile);
  if( rc>0 ){
    return rc;
  }

  /* 
**ͨ�������в���������ѡ����еڶ��β�����
**�ڶ� ���ӳ�ֱ����ʼ���ļ�������֮��
**�Ա������в������ǳ�ʼ���ļ����á�
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
      bail_on_error = 1;//���û�н����������ִ�н�ͣ��һ������״̬��
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
        rc = do_meta_command(z, &data);//�����������������z ָ�������
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
    /* ֻ���к����ݿ�����ƥ�������
    */
    if( zFirstCmd[0]=='.' ){
      rc = do_meta_command(zFirstCmd, &data);//�����������������zFirstCmdָ�������
    }else{
      open_db(&data);
    /*ͨ���ṩ�Ļص����������ݵ�ǰ��ģʽ��ӡ����Ӧ���*/
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
    /*���дӱ�׼������յ�������*/
    if( stdin_is_interactive ){//����ʽ����
      char *zHome;
      char *zHistory = 0;
      int nHistory;
      printf(
        "SQLite version %s %.19s\n" /*extra-version-info*/
        "Enter \".help\" for instructions\n"
        "Enter SQL statements terminated with a \";\"\n",
        sqlite3_libversion(), sqlite3_sourceid()
      );
      zHome = find_home_dir();//�����û���Ŀ¼
      if( zHome ){
        nHistory = strlen30(zHome) + 20;
        if( (zHistory = malloc(nHistory))!=0 ){
       /*��snprintf�������ƣ�������д�뻺������Ϊ�ڶ���������
      **��������С���ɵ�һ���������� ��*/
          sqlite3_snprintf(nHistory, zHistory,"%s/.sqlite_history", zHome);
        }
      }
#if defined(HAVE_READLINE) && HAVE_READLINE==1//�ж��������Ƿ��Ѿ�����
      if( zHistory ) read_history(zHistory);//�õ�zHistory ����ֵ
#endif
      rc = process_input(&data, 0);
      if( zHistory ){
        stifle_history(100);
        write_history(zHistory);
        free(zHistory);//�ͷ�zHistory�ռ�
      }
    }else{
      rc = process_input(&data, stdin);//�ѱ�׼����Ĵ����������ظ�rc
    }
  }
  set_table_name(&data, 0);//���ñ���
  if( data.db ){
    sqlite3_close(data.db);//�ر����ݿ�
  }
  return rc;
}