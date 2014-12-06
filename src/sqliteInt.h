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
** Internal interface definitions for SQLite. SQLite内部界面的定义
** 
*/
#ifndef _SQLITEINT_H_
#define _SQLITEINT_H_

/*
** These #defines should enable >2GB file support on POSIX if the     如果潜在的操作系统能够支撑的话，_SQLITEINT_H_的定义应该使得可移植性操作系统接口能支撑2G以上的大文件
** underlying operating system supports it.  If the OS lacks
** large file support, or if the OS is windows, these should be no-ops.  如果操作系统缺乏大文件的支撑，或者如果操作系统是windows操作系统，这里就应该是空操作。
**
** Ticket #2739:  The _LARGEFILE_SOURCE macro must appear before any     标签#2739:宏_LARGEFILE_SOURCE必须在任一系统#includes前出现
** system #includes.  Hence, this block of code must be the very first   因此，代码块必须在所有源文件中首先编码。
** code in all source files.
**
** Large file support can be disabled using the -DSQLITE_DISABLE_LFS switch   大文件的支持可能会禁止 -DSQLITE_DISABLE_LFS在编译器命令行上的转换。
** on the compiler command line.  This is necessary if you are compiling      这是有必要的假如你要在在近期的机器上编译的话(Red Hat 7.2除外)，
** on a recent machine (ex: Red Hat 7.2) but you want your code to work       除非你想要你的代码在老式机器上运行(Red Hat 6.0除外)
** on an older machine (ex: Red Hat 6.0).  If you compile on Red Hat 7.2      如果你的编译在Red Hat 7.2上没有这个选项
** without this option, LFS is enable.  But LFS does not exist in the kernel  LFS逻辑文件系统（Logical File System）/逻辑文件结构（Logical File Structure）将使之成为可能。
** in Red Hat 6.0, so the code won't work.  Hence, for maximum binary         但如果 Red Hat 6.0的内核里面不存在LFS，那么代码不会被运行。
** portability you should omit LFS.  因此，对于最大的二进制可移植性，你应该忽略LFS。                                  
**
** Similar is true for Mac OS X.  LFS is only supported on Mac OS X 9 and later.  对于操作系统Mac OS X也是一样的。   LFC仅仅支持MAC OS X 9及之后的版本。
*/
#ifndef SQLITE_DISABLE_LFS
# define _LARGE_FILE       1
# ifndef _FILE_OFFSET_BITS
#   define _FILE_OFFSET_BITS 64
# endif
# define _LARGEFILE_SOURCE 1
#endif

/*
** Include the configuration header output by 'configure' if we're using the    假如我们使用基于atuoconf构建,则要incloude 'configure'配置头输出。
** autoconf-based build         Autoconf是一个用于生成可以自动地配置软件源代码包以适应多种Unix类系统的 shell脚本的工具。
*/
#ifdef _HAVE_SQLITE_CONFIG_H
#include "config.h"
#endif

#include "sqliteLimit.h"

/* Disable nuisance warnings on Borland compilers 禁止Borland编译器上的妨扰警告信号 */   
#if defined(__BORLANDC__)
#pragma warn -rch /* unreachable code 不可达代码 */
#pragma warn -ccc /* Condition is always true or false 条件要么是真要么是假*/
#pragma warn -aus /* Assigned value is never used 分配值从未用过*/
#pragma warn -csu /* Comparing signed and unsigned 比较有符号和无符号*/
#pragma warn -spa /* Suspicious pointer arithmetic 可疑的指针运算*/
#endif

/* Needed for various definitions... 需要为变量做定义*/
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

/*
** Include standard header files as necessary     Include标准的头文件是必要的
*/
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

/*
** The following macros are used to cast pointers to integers and     下面的宏定义是指针到整型，整型到指针的转换
** integers to pointers.  The way you do this varies from one compiler  用这样的方法，一个编译器到下一个编译器是不同的，
** to the next, so we have developed the following set of #if statements  因此，我们研制出了下面这样一组条件语句为许多不同的编译器生成一个适当的宏命令。
** to generate appropriate macros for a wide range of compilers.
**
** The correct "ANSI" way to do this is to use the intptr_t type    正确的方式是用intptr_t类型来做这些。  注:intptr_t在不同的平台是不一样的，始终与地址位数相同，因此用来存放地址，即地址。
** Unfortunately, that typedef is not available on all compilers, or  不幸的是，这种类型定义在所有的编译器上都是无效的，
** if it is available, it requires an #include of specific headers    即是是有效的，也需要#include特殊的从一个编译器到下一个编译器是不同的头文件
** that vary from one machine to the next.
**
** Ticket #3860:  The llvm-gcc-4.2 compiler from Apple chokes on    标签#3860: 来自苹果公司的llvm-gcc-4.2编译器停止了使用((void*)&((char*)0)[X])结构
** the ((void*)&((char*)0)[X]) construct.  But MSVC chokes on ((void*)(X)).   但是MSVC停止了使用((void*)(X))，因此我们不得不取决于编译器在不同的方式中定义宏指令
** So we have to define the macros in different ways depending on the
** compiler.
*/
#if defined(__PTRDIFF_TYPE__)  /* This case should work for GCC */
# define SQLITE_INT_TO_PTR(X)  ((void*)(__PTRDIFF_TYPE__)(X))
# define SQLITE_PTR_TO_INT(X)  ((int)(__PTRDIFF_TYPE__)(X))
#elif !defined(__GNUC__)       /* Works for compilers other than LLVM 在编译器上运行而不是在LLVM上运行   LLVM:低级虚拟机(Low Level Virtual Machine) */
# define SQLITE_INT_TO_PTR(X)  ((void*)&((char*)0)[X])
# define SQLITE_PTR_TO_INT(X)  ((int)(((char*)X)-(char*)0))
#elif defined(HAVE_STDINT_H)   /* Use this case if we have ANSI headers 在如果我没有ANSI标准的头文件，这种情况下使用    ANSI:美国国家标准学会（ American National Standards Institute） */
# define SQLITE_INT_TO_PTR(X)  ((void*)(intptr_t)(X))
# define SQLITE_PTR_TO_INT(X)  ((int)(intptr_t)(X))
#else                          /* Generates a warning - but it always works 形成一个警告，但是还是运行的*/
# define SQLITE_INT_TO_PTR(X)  ((void*)(X))
# define SQLITE_PTR_TO_INT(X)  ((int)(X))
#endif

/*
** The SQLITE_THREADSAFE macro must be defined as 0, 1, or 2.      宏SQLITE_THREADSAFE必须定义为'0','1'或者'2'
** 0 means mutexes are permanently disable and the library is never    '0'表示互斥锁是永久无效的，并且库是没有安全威胁的。
** threadsafe.  1 means the library is serialized which is the highest   '1'表示库是序列化的，线程安全的等级是最高级
** level of threadsafety.  2 means the libary is multithreaded - multiple   
** threads can use SQLite as long as no two threads try to use the same    '2'表示库可以多线程到多线程的使用，只要没有同时有2个线程使用相同的数据库连接。
** database connection at the same time.
**
** Older versions of SQLite used an optional THREADSAFE macro.   老版本的SQLite使用了一个可选择的 THREADSAFE宏
** We support that for legacy.   我们把它作为遗产来支持
*/
#if !defined(SQLITE_THREADSAFE)
#if defined(THREADSAFE)
# define SQLITE_THREADSAFE THREADSAFE
#else
# define SQLITE_THREADSAFE 1 /* IMP: R-07272-22309  接口信息处理器:R-07272-22309 */
#endif
#endif

/*
** Powersafe overwrite is on by default.  But can be turned off using    Powersafe默认情况下是覆盖。  
** the -DSQLITE_POWERSAFE_OVERWRITE=0 command-line option.      但是可以关掉 -DSQLITE_POWERSAFE_OVERWRITE=0命令行选项的使用。
*/
#ifndef SQLITE_POWERSAFE_OVERWRITE
# define SQLITE_POWERSAFE_OVERWRITE 1
#endif

/*
** The SQLITE_DEFAULT_MEMSTATUS macro must be defined as either 0 or 1.  宏SQLITE_DEFAULT_MEMSTATUS必须被定义为0或者1.
** It determines whether or not the features related to 
** SQLITE_CONFIG_MEMSTATUS are available by default or not. This value can    取决于这个特征是否与SQLITE_CONFIG_MEMSTATUS是不是默认可供使用的有关
** be overridden at runtime using the sqlite3_config() API. 在运行时使用sqlite3_config() API,这个值可以被覆盖        API:应用程序界面（Application Program Interface）
*/
#if !defined(SQLITE_DEFAULT_MEMSTATUS)
# define SQLITE_DEFAULT_MEMSTATUS 1
#endif

/*
** Exactly one of the following macros must be defined in order to     为了指定哪一个内存子系统被使用，下面的其中一个宏被定义是正确的。
** specify which memory allocation subsystem to use.
**
**     SQLITE_SYSTEM_MALLOC          // Use normal system malloc()    用标准系统的内存分配函数
**     SQLITE_WIN32_MALLOC           // Use Win32 native heap API     用win32自身的堆栈API
**     SQLITE_ZERO_MALLOC            // Use a stub allocator that always fails  用故障的根分配器
**     SQLITE_MEMDEBUG               // Debugging version of system malloc()  系统调试版的内存分配函数
**
** On Windows, if the SQLITE_WIN32_MALLOC_VALIDATE macro is defined and the   在windows操作系统上，如果宏SQLITE_WIN32_MALLOC_VALIDATE被定义并且宏assert()被启用。
** assert() macro is enabled, each call into the Win32 native heap subsystem                assert()是个定义在 <assert.h> 中的宏, 用来测试断言。一个断言本质上是写下程序员的假设, 如果假设被违反, 那表明有个严重的程序错误。
** will cause HeapValidate to be called.  If heap validation should fail, an  每一次在win32自身的堆栈子系统上的调用将引起HeapValidate的调用。
** assertion will be triggered. 如果堆栈建议失败了，将引起一个警告。
**
** (Historical note:  There used to be several other options, but we've  历史注释:过去有很多很多其他的选项，但我们消减得只剩三个了。
** pared it down to just these three.)
**
** If none of the above are defined, then set SQLITE_SYSTEM_MALLOC as   假如上面的都没有被定义，SQLITE_SYSTEM_MALLOC将被设置为默认值。
** the default.
*/
#if defined(SQLITE_SYSTEM_MALLOC) \
  + defined(SQLITE_WIN32_MALLOC) \
  + defined(SQLITE_ZERO_MALLOC) \
  + defined(SQLITE_MEMDEBUG)>1
# error "Two or more of the following compile-time configuration options\
 are defined but at most one is allowed:\
 SQLITE_SYSTEM_MALLOC, SQLITE_WIN32_MALLOC, SQLITE_MEMDEBUG,\
 SQLITE_ZERO_MALLOC"
#endif
#if defined(SQLITE_SYSTEM_MALLOC) \
  + defined(SQLITE_WIN32_MALLOC) \
  + defined(SQLITE_ZERO_MALLOC) \
  + defined(SQLITE_MEMDEBUG)==0
# define SQLITE_SYSTEM_MALLOC 1
#endif

/*
** If SQLITE_MALLOC_SOFT_LIMIT is not zero, then try to keep the   如果SQLITE_MALLOC_SOFT_LIMIT的值不是0，则保持可能的值下分配的内存大小。
** sizes of memory allocations below this value where possible.
*/
#if !defined(SQLITE_MALLOC_SOFT_LIMIT)
# define SQLITE_MALLOC_SOFT_LIMIT 1024
#endif

/*
** We need to define _XOPEN_SOURCE as follows in order to enable    为了启用在大多数UNIX操作系统上的递归互斥体， 我们需要将_XOPEN_SOURCE做如下定义
** recursive mutexes on most Unix systems.  But Mac OS X is different.  但 Mac OS X 操作系统是不同的。
** The _XOPEN_SOURCE define causes problems for Mac OS X we are told,    _XOPEN_SOURCE define的定义会导致我们说过的 Mac OS X操作系统的问题
** so it is omitted there.  See ticket #2673.  因此，这儿是忽略了的。见标签:#2673
**
** Later we learn that _XOPEN_SOURCE is poorly or incorrectly  稍后，我们将学到 _XOPEN_SOURCE在一些操作系统上的执行是不良的或者说是错误的。
** implemented on some systems.  So we avoid defining it at all
** if it is already defined or if it is unneeded because we are   因此，我们可以不定义它，假如它已经被定义了或如果它因为我们不做不安全的构建而不需要了。
** not doing a threadsafe build.  Ticket #2681.   标签:#2681
**
** See also ticket #2741.  也看标签:#2741
*/
#if !defined(_XOPEN_SOURCE) && !defined(__DARWIN__) && !defined(__APPLE__) && SQLITE_THREADSAFE
#  define _XOPEN_SOURCE 500  /* Needed to enable pthread recursive mutexes 需要启用多线程编程递归互斥体*/
#endif

/*
** The TCL headers are only needed when compiling the TCL bindings.   TCL头文件只有当编译TCL绑定时才需要。  TCL：事务控制语言
*/
#if defined(SQLITE_TCL) || defined(TCLSH)
# include <tcl.h>
#endif

/*
** NDEBUG and SQLITE_DEBUG are opposites.  It should always be true that      NDEBUG 和 SQLITE_DEBUG是相反的。
** defined(NDEBUG)==!defined(SQLITE_DEBUG).  If this is not currently true,   defined(NDEBUG)==!defined(SQLITE_DEBUG)这个定义是永远为真的。
** make it true by defining or undefining NDEBUG.     如果当前不是真的，则可以通过定义或者不定义NDEBUG来让它为真。
**
** Setting NDEBUG makes the code smaller and run faster by disabling the     设置NDEBUG，通过禁止在代码中assert()语句的数目，让代码更小一些，运行速度更快一些。
** number assert() statements in the code.  So we want the default action    因此，我们想要的默认操作是设置 NDEBUG并且 NDEBUG只有在SQLITE_DEBUG被设置的时候才不被定义。
** to be for NDEBUG to be set and NDEBUG to be undefined only if SQLITE_DEBUG
** is set.  Thus NDEBUG becomes an opt-in rather than an opt-out      因此，NDEBUG变成了选择性输入，而不是选择性输出
** feature.
*/
#if !defined(NDEBUG) && !defined(SQLITE_DEBUG) 
# define NDEBUG 1
#endif
#if defined(NDEBUG) && defined(SQLITE_DEBUG)
# undef NDEBUG
#endif

/*
** The testcase() macro is used to aid in coverage testing.  When  宏testcase()被用于帮助覆盖测试。
** doing coverage testing, the condition inside the argument to
** testcase() must be evaluated both true and false in order to           当做覆盖测试时内容概要中的条件是testcase() 必须在真和假之间估值 ，这样做为了得到完整的分支覆盖。 
** get full branch coverage.  The testcase() macro is inserted
** to help ensure adequate test coverage in places where simple          宏testcase()的插入式为了保证在某些地方充分的测试覆盖，这些地方简单的条件覆盖或分支覆盖是不足的。
** condition/decision coverage is inadequate.  For example, testcase()
** can be used to make sure boundary values are tested.  For    例如，宏testcase()可以用来确保分支值是被测试过的。
** bitmask tests, testcase() can be used to make sure each bit  对于位掩码测试，宏testcase()可以用来确保每一位都是有意义的并且至少被使用一次。
** is significant and used at least once.  On switch statements
** where multiple cases go to the same block of code, testcase()  在switch语句中，多重条件定位到相同的代码块，宏testcase()可以确保所有的条件是已经被估计的。
** can insure that all cases are evaluated.
**
*/
#ifdef SQLITE_COVERAGE_TEST
  void sqlite3Coverage(int);
# define testcase(X)  if( X ){ sqlite3Coverage(__LINE__); }
#else
# define testcase(X)
#endif

/*
** The TESTONLY macro is used to enclose variable declarations or    宏TESTONLY被用来装入变量声明或其它小块的代码，这需要宏testcase() 和 assert()中参数的支撑。
** other bits of code that are needed to support the arguments
** within testcase() and assert() macros.
*/
#if !defined(NDEBUG) || defined(SQLITE_COVERAGE_TEST)
# define TESTONLY(X)  X
#else
# define TESTONLY(X)
#endif

/*
** Sometimes we need a small amount of code such as a variable initialization   一些时候，我们需要一些少量的代码，如设置变量初始化的代码，来设置后面的assert()语句。
** to setup for a later assert() statement.  We do not want this code to    我们不想要这些代码在assert()无效时/被禁止时出现。
** appear when assert() is disabled.  The following macro is therefore   因此后面的宏被用来隐藏(contain)设置码。
** used to contain that setup code.  The "VVA" acronym stands for     首字母缩写'VVA'被用来替代"Verification, Validation, and Accreditation".
** "Verification, Validation, and Accreditation".  In other words, the
** code within VVA_ONLY() will only run during verification processes.  换句话说，VVA_ONLY()中的代码将仅仅在验证过程期间运行。
*/
#ifndef NDEBUG
# define VVA_ONLY(X)  X
#else
# define VVA_ONLY(X)
#endif

/*
** The ALWAYS and NEVER macros surround boolean expressions which      宏ALWAYS 和 NEVER围绕布尔表达式，其目的是它们分别总是真的或假的
** are intended to always be true or false, respectively.  Such
** expressions could be omitted from the code completely.  But they   这个表达可以完全从代码中删除。
** are included in a few cases in order to enhance the resilience     但这里包含了一些少数情形，其目的是为了提高SQLite意外行为的恢复力，
** of SQLite to unexpected behavior - to make the code "self-healing"   在首次意外行为暗示时，让代码自愈或可塑而不是易碎或彻底悔了
** or "ductile" rather than being "brittle" and crashing at the first
** hint of unplanned behavior.
**
** In other words, ALWAYS and NEVER are added for defensive code.   换句话说， 宏ALWAYS 和 NEVER是为了保护代码而引入的。
**
** When doing coverage testing ALWAYS and NEVER are hard-coded to   当做恢复测试时，宏ALWAYS 和 NEVER被硬编码为真和假，以至于当时被指定的不可达代码不被计算入未经检验的代码中。
** be true and false so that the unreachable code then specify will
** not be counted as untested code.
*/
#if defined(SQLITE_COVERAGE_TEST)
# define ALWAYS(X)      (1)
# define NEVER(X)       (0)
#elif !defined(NDEBUG)
# define ALWAYS(X)      ((X)?1:(assert(0),0))
# define NEVER(X)       ((X)?(assert(0),1):0)
#else
# define ALWAYS(X)      (X)
# define NEVER(X)       (X)
#endif

/*
** Return true (non-zero) if the input is a integer that is too large    如果输入的整型太大而不能放入32位，则返回真(非零)。
** to fit in 32-bits.  This macro is used inside of various testcase()   这个宏用于testcase()宏变量内，来验证我们已经测试的大文件支持的数据库。
** macros to verify that we have tested SQLite for large-file support.
*/
#define IS_BIG_INT(X)  (((X)&~(i64)0xffffffff)!=0)

/*
** The macro unlikely() is a hint that surrounds a boolean    宏unlikely()是一个环绕一个值通常为假的布尔表达式的提示。
** expression that is usually false.  Macro likely() surrounds  宏likely()是一个环绕一个值通常为真的布尔表达式的提示。
** a boolean expression that is usually true.  GCC is able to   有时，GCC能够用这种提示来生成更好的代码。   
** use these hints to generate better code, sometimes.      GCC（GNU Compiler Collection，GNU编译器套装），是一套由GNU开发的编程语言编译器。
*/
#if defined(__GNUC__) && 0
# define likely(X)    __builtin_expect((X),1)
# define unlikely(X)  __builtin_expect((X),0)
#else
# define likely(X)    !!(X)
# define unlikely(X)  !!(X)
#endif

#include "sqlite3.h"
#include "hash.h"
#include "parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>

/*
** If compiling for a processor that lacks floating point support,   假如处理机的编译缺乏浮点型的支撑，可以用整型取代浮点型。
** substitute integer for floating-point
*/
#ifdef SQLITE_OMIT_FLOATING_POINT
# define double sqlite_int64
# define float sqlite_int64
# define LONGDOUBLE_TYPE sqlite_int64
# ifndef SQLITE_BIG_DBL
#   define SQLITE_BIG_DBL (((sqlite3_int64)1)<<50)
# endif
# define SQLITE_OMIT_DATETIME_FUNCS 1
# define SQLITE_OMIT_TRACE 1
# undef SQLITE_MIXED_ENDIAN_64BIT_FLOAT
# undef SQLITE_HAVE_ISNAN
#endif
#ifndef SQLITE_BIG_DBL
# define SQLITE_BIG_DBL (1e99)
#endif

/*
** OMIT_TEMPDB is set to 1 if SQLITE_OMIT_TEMPDB is defined, or 0       如果SQLITE_OMIT_TEMPDB被定义了，OMIT_TEMPDB被设置为1，否则，设为0
** afterward. Having this macro allows us to cause the C compiler     这个宏允许我们触发C编译器忽略没有凌乱的#ifndef语句的TEMP表的代码的使用。 
** to omit code used by TEMP tables without messy #ifndef statements.      
*/
#ifdef SQLITE_OMIT_TEMPDB
#define OMIT_TEMPDB 1
#else
#define OMIT_TEMPDB 0
#endif

/*
** The "file format" number is an integer that is incremented whenever   文件格式号是一个整数，每当VDBE级的文件格式改变时这个值是递增的。    VDBE:虚拟数据库引擎Virtual Database Engine
** the VDBE-level file format changes.  The following macros define the     下面的宏定义了新数据库的缺省文件格式和库可以读的最大文件格式
** the default file format for new databases and the maximum file format
** that the library can read.
*/
#define SQLITE_MAX_FILE_FORMAT 4
#ifndef SQLITE_DEFAULT_FILE_FORMAT
# define SQLITE_DEFAULT_FILE_FORMAT 4
#endif

/*
** Determine whether triggers are recursive by default.  This can be   取决于触发器是否是默认递归的。这是可以被改变的在运行时使用一个编译指示。
** changed at run-time using a pragma.
*/
#ifndef SQLITE_DEFAULT_RECURSIVE_TRIGGERS
# define SQLITE_DEFAULT_RECURSIVE_TRIGGERS 0
#endif

/*
** Provide a default value for SQLITE_TEMP_STORE in case it is not specified  为 SQLITE_TEMP_STORE提供一个缺省值，如果它在命令行上没被规定的话。
** on the command-line
*/
#ifndef SQLITE_TEMP_STORE
# define SQLITE_TEMP_STORE 1
#endif

/*
** GCC does not define the offsetof() macro so we'll have to do it    GCC并没有定义宏offsetof()，因此我们不得不自己定义。
** ourselves.                                                         GCC（GNU Compiler Collection，GNU编译器套装），是一套由GNU开发的编程语言编译器。
*/
#ifndef offsetof
#define offsetof(STRUCTURE,FIELD) ((int)((char*)&((STRUCTURE*)0)->FIELD))
#endif

/*
** Check to see if this machine uses EBCDIC.  (Yes, believe it or       检查机器是否使用了EBCDIC。 (是，相信或者不相信，都会有机器在那里使用EBCDIC)
** not, there are still machines out there that use EBCDIC.)              EBCDIC:扩充的二进制编码的十进制交换码（Extended Binary Coded Decimal Interchange Code）
*/
#if 'A' == '\301'
# define SQLITE_EBCDIC 1
#else
# define SQLITE_ASCII 1
#endif

/*
** Integers of known sizes.  These typedefs might change for architectures   已知尺寸的整型。  这些类型可能会改变结构的大小。
** where the sizes very.  Preprocessor macros are available so that the      预处理宏是可用的，所以在编译类型上可以方便地重新定义的类型。
** types can be conveniently redefined at compile-type.  Like this:          例如:把'-DUINTPTR_TYPE定义为long long int型
**
**         cc '-DUINTPTR_TYPE=long long int' ...                         int 在内存占两个字节 ，范围是-32768~32767；而long long int在内存占八个字节， 范围是-922337203685775808~922337203685775807
*/
#ifndef UINT32_TYPE
# ifdef HAVE_UINT32_T
#  define UINT32_TYPE uint32_t
# else
#  define UINT32_TYPE unsigned int
# endif
#endif
#ifndef UINT16_TYPE
# ifdef HAVE_UINT16_T
#  define UINT16_TYPE uint16_t
# else
#  define UINT16_TYPE unsigned short int
# endif
#endif
#ifndef INT16_TYPE
# ifdef HAVE_INT16_T
#  define INT16_TYPE int16_t
# else
#  define INT16_TYPE short int
# endif
#endif
#ifndef UINT8_TYPE
# ifdef HAVE_UINT8_T
#  define UINT8_TYPE uint8_t
# else
#  define UINT8_TYPE unsigned char
# endif
#endif
#ifndef INT8_TYPE
# ifdef HAVE_INT8_T
#  define INT8_TYPE int8_t
# else
#  define INT8_TYPE signed char
# endif
#endif
#ifndef LONGDOUBLE_TYPE
# define LONGDOUBLE_TYPE long double
#endif
typedef sqlite_int64 i64;          /* 8-byte signed integer 8位有符号整型*/
typedef sqlite_uint64 u64;         /* 8-byte unsigned integer 8位无符号整型*/
typedef UINT32_TYPE u32;           /* 4-byte unsigned integer 4位无符号整型*/
typedef UINT16_TYPE u16;           /* 2-byte unsigned integer 2位无符号整型*/
typedef INT16_TYPE i16;            /* 2-byte signed integer 2位有符号整型*/
typedef UINT8_TYPE u8;             /* 1-byte unsigned integer 1位无符号整型*/
typedef INT8_TYPE i8;              /* 1-byte signed integer 1位有符号整型*/

/*
** SQLITE_MAX_U32 is a u64 constant that is the maximum u64 value        
SQLITE_MAX_U32是一个u64类型(上面定义的8位无符号整型)的常量，就是说，u64的最大值可以被存储在u32(4位无符号整型)中而且不丢失数据。
** that can be stored in a u32 without loss of data.  The value     这个值是0x00000000ffffffff。
** is 0x00000000ffffffff.  But because of quirks of some compilers, we     但是由于一些编译器的怪异模式，我们不得不指定这个值在不直观的方式显示。
** have to specify the value in the less intuitive manner shown:
*/
#define SQLITE_MAX_U32  ((((u64)1)<<32)-1)

/*
** The datatype used to store estimates of the number of rows in a    这个数据类型被用来存储一个表或者索引中所估计的行数。
** table or index.  This is an unsigned integer type.  For 99.9% of   这是一个无符号整型。
** the world, a 32-bit integer is sufficient.  But a 64-bit integer   世界上99.9%的32位整型是足够的。 但64位整型如有必要的话将在编译阶段被使用。
** can be used at compile-time if desired.
*/
#ifdef SQLITE_64BIT_STATS
 typedef u64 tRowcnt;    /* 64-bit only if requested at compile-time 64位只在编译阶段有使用请求*/
#else
 typedef u32 tRowcnt;    /* 32-bit is the default 32位是默认的*/
#endif

/*
** Macros to determine whether the machine is big or little endian,    宏决定机器在运行期间的估值是低位优先还是高位优先
** evaluated at runtime.
*/
#ifdef SQLITE_AMALGAMATION
const int sqlite3one = 1;
#else
extern const int sqlite3one;
#endif
#if defined(i386) || defined(__i386__) || defined(_M_IX86)\
                             || defined(__x86_64) || defined(__x86_64__)
# define SQLITE_BIGENDIAN    0
# define SQLITE_LITTLEENDIAN 1
# define SQLITE_UTF16NATIVE  SQLITE_UTF16LE
#else
# define SQLITE_BIGENDIAN    (*(char *)(&sqlite3one)==0)
# define SQLITE_LITTLEENDIAN (*(char *)(&sqlite3one)==1)
# define SQLITE_UTF16NATIVE (SQLITE_BIGENDIAN?SQLITE_UTF16BE:SQLITE_UTF16LE)
#endif

/*
** Constants for the largest and smallest possible 64-bit signed integers.  64位有符号整型可能的最大常量和最小常量。
** These macros are designed to work correctly on both 32-bit and 64-bit    这些宏被定义正确地在32位和64位编译器上工作。 
** compilers.
*/
#define LARGEST_INT64  (0xffffffff|(((i64)0x7fffffff)<<32))
#define SMALLEST_INT64 (((i64)-1) - LARGEST_INT64)

/* 
** Round up a number to the next larger multiple of 8.  This is used      向上舍入一个数，使之接近8的倍数。
** to force 8-byte alignment on 64-bit architectures.     这是用来强制8位对齐64位的体系结构。
*/
#define ROUND8(x)     (((x)+7)&~7)

/*
** Round down to the nearest multiple of 8   最接近8的倍数的四舍五入。
*/
#define ROUNDDOWN8(x) ((x)&~7)

/*
** Assert that the pointer X is aligned to an 8-byte boundary.  This   声明指针X是对齐到8字节边界的。
** macro is used only within assert() to verify that the code gets     这个宏只在assert()中用来验证代码是否得到了正确的对齐限制。
** all alignment restrictions correct.
**
** Except, if SQLITE_4_BYTE_ALIGNED_MALLOC is defined, then the      有例外，如果 SQLITE_4_BYTE_ALIGNED_MALLOC被定义了， 潜在的malloc()实现可能会返回我们4字节对齐的指针
** underlying malloc() implemention might return us 4-byte aligned
** pointers.  In that case, only verify 4-byte alignment.            在这种情况下，只验证4字节的对齐。
*/
#ifdef SQLITE_4_BYTE_ALIGNED_MALLOC
# define EIGHT_BYTE_ALIGNMENT(X)   ((((char*)(X) - (char*)0)&3)==0)
#else
# define EIGHT_BYTE_ALIGNMENT(X)   ((((char*)(X) - (char*)0)&7)==0)
#endif


/*
** An instance of the following structure is used to store the busy-handler   以下的结构的一个实例是用于存储繁忙的处理器回调给SQLite的一个处理。
** callback for a given sqlite handle. 
**
** The sqlite.busyHandler member of the sqlite struct contains the busy   结构体busyHandler的成员包括数据库句柄的频繁回调
** callback for the database handle. Each pager opened via the sqlite     每一页通过SQLite句柄传递一个指针到sqlite.busyhandler打开
** handle is passed a pointer to sqlite.busyHandler. The busy-handler     繁忙的处理程序的回调目前仅仅是pager.c中的调用。
** callback is currently invoked only from within pager.c.
*/
typedef struct BusyHandler BusyHandler;
struct BusyHandler {
  int (*xFunc)(void *,int);  /* The busy callback 频繁回调*/
  void *pArg;                /* First arg to busy callback 频繁回调的第一个自变量*/
  int nBusy;                 /* Incremented with each busy call 每一个频繁调用的增加*/
};

/*
** Name of the master database table.  The master database table
** is a special table that holds the names and attributes of all
** user tables and indices.
*/
#define MASTER_NAME       "sqlite_master"
#define TEMP_MASTER_NAME  "sqlite_temp_master"

/*
** The root-page of the master database table.
*/
#define MASTER_ROOT       1

/*
** The name of the schema table.
*/
#define SCHEMA_TABLE(x)  ((!OMIT_TEMPDB)&&(x==1)?TEMP_MASTER_NAME:MASTER_NAME)

/*
** A convenience macro that returns the number of elements in
** an array.
*/
#define ArraySize(X)    ((int)(sizeof(X)/sizeof(X[0])))

/*
** The following value as a destructor means to use sqlite3DbFree().
** The sqlite3DbFree() routine requires two parameters instead of the 
** one parameter that destructors normally want.  So we have to introduce 
** this magic value that the code knows to handle differently.  Any 
** pointer will work here as long as it is distinct from SQLITE_STATIC
** and SQLITE_TRANSIENT.
*/
#define SQLITE_DYNAMIC   ((sqlite3_destructor_type)sqlite3MallocSize)

/*
** When SQLITE_OMIT_WSD is defined, it means that the target platform does
** not support Writable Static Data (WSD) such as global and static variables.
** All variables must either be on the stack or dynamically allocated from
** the heap.  When WSD is unsupported, the variable declarations scattered
** throughout the SQLite code must become constants instead.  The SQLITE_WSD
** macro is used for this purpose.  And instead of referencing the variable
** directly, we use its constant as a key to lookup the run-time allocated
** buffer that holds real variable.  The constant is also the initializer
** for the run-time allocated buffer.
**
** In the usual case where WSD is supported, the SQLITE_WSD and GLOBAL
** macros become no-ops and have zero performance impact.
*/
#ifdef SQLITE_OMIT_WSD
  #define SQLITE_WSD const
  #define GLOBAL(t,v) (*(t*)sqlite3_wsd_find((void*)&(v), sizeof(v)))
  #define sqlite3GlobalConfig GLOBAL(struct Sqlite3Config, sqlite3Config)
  int sqlite3_wsd_init(int N, int J);
  void *sqlite3_wsd_find(void *K, int L);
#else
  #define SQLITE_WSD 
  #define GLOBAL(t,v) v
  #define sqlite3GlobalConfig sqlite3Config
#endif

/*
** The following macros are used to suppress compiler warnings and to
** make it clear to human readers when a function parameter is deliberately 
** left unused within the body of a function. This usually happens when
** a function is called via a function pointer. For example the 
** implementation of an SQL aggregate step callback may not use the
** parameter indicating the number of arguments passed to the aggregate,
** if it knows that this is enforced elsewhere.
**
** When a function parameter is not used at all within the body of a function,
** it is generally named "NotUsed" or "NotUsed2" to make things even clearer.
** However, these macros may also be used to suppress warnings related to
** parameters that may or may not be used depending on compilation options.
** For example those parameters only used in assert() statements. In these
** cases the parameters are named as per the usual conventions.
*/
#define UNUSED_PARAMETER(x) (void)(x)
#define UNUSED_PARAMETER2(x,y) UNUSED_PARAMETER(x),UNUSED_PARAMETER(y)

/*
** Forward references to structures
*/
typedef struct AggInfo AggInfo;
typedef struct AuthContext AuthContext;
typedef struct AutoincInfo AutoincInfo;
typedef struct Bitvec Bitvec;
typedef struct CollSeq CollSeq;
typedef struct Column Column;
typedef struct Db Db;
typedef struct Schema Schema;
typedef struct Expr Expr;
typedef struct ExprList ExprList;
typedef struct ExprSpan ExprSpan;
typedef struct FKey FKey;
typedef struct FuncDestructor FuncDestructor;
typedef struct FuncDef FuncDef;
typedef struct FuncDefHash FuncDefHash;
typedef struct IdList IdList;
typedef struct Index Index;
typedef struct IndexSample IndexSample;
typedef struct KeyClass KeyClass;
typedef struct KeyInfo KeyInfo;
typedef struct Lookaside Lookaside;
typedef struct LookasideSlot LookasideSlot;
typedef struct Module Module;
typedef struct NameContext NameContext;
typedef struct Parse Parse;
typedef struct RowSet RowSet;
typedef struct Savepoint Savepoint;
typedef struct Select Select;
typedef struct SrcList SrcList;
typedef struct StrAccum StrAccum;
typedef struct Table Table;
typedef struct TableLock TableLock;
typedef struct Token Token;
typedef struct Trigger Trigger;
typedef struct TriggerPrg TriggerPrg;
typedef struct TriggerStep TriggerStep;
typedef struct UnpackedRecord UnpackedRecord;
typedef struct VTable VTable;
typedef struct VtabCtx VtabCtx;
typedef struct Walker Walker;
typedef struct WherePlan WherePlan;
typedef struct WhereInfo WhereInfo;
typedef struct WhereLevel WhereLevel;

/*
** Defer sourcing vdbe.h and btree.h until after the "u8" and 
** "BusyHandler" typedefs. vdbe.h also requires a few of the opaque
** pointer types (i.e. FuncDef) defined above.
*/
#include "btree.h"
#include "vdbe.h"
#include "pager.h"
#include "pcache.h"

#include "os.h"
#include "mutex.h"


/*
** Each database file to be accessed by the system is an instance
** of the following structure.  There are normally two of these structures
** in the sqlite.aDb[] array.  aDb[0] is the main database file and
** aDb[1] is the database file used to hold temporary tables.  Additional
** databases may be attached.
*/
struct Db {
  char *zName;         /* Name of this database */
  Btree *pBt;          /* The B*Tree structure for this database file */
  u8 inTrans;          /* 0: not writable.  1: Transaction.  2: Checkpoint */
  u8 safety_level;     /* How aggressive at syncing data to disk */
  Schema *pSchema;     /* Pointer to database schema (possibly shared) */
};

/*
** An instance of the following structure stores a database schema.
**
** Most Schema objects are associated with a Btree.  The exception is
** the Schema for the TEMP databaes (sqlite3.aDb[1]) which is free-standing.
** In shared cache mode, a single Schema object can be shared by multiple
** Btrees that refer to the same underlying BtShared object.
** 
** Schema objects are automatically deallocated when the last Btree that
** references them is destroyed.   The TEMP Schema is manually freed by
** sqlite3_close().
*
** A thread must be holding a mutex on the corresponding Btree in order
** to access Schema content.  This implies that the thread must also be
** holding a mutex on the sqlite3 connection pointer that owns the Btree.
** For a TEMP Schema, only the connection mutex is required.
*/
struct Schema {
  int schema_cookie;   /* Database schema version number for this file */
  int iGeneration;     /* Generation counter.  Incremented with each change */
  Hash tblHash;        /* All tables indexed by name */
  Hash idxHash;        /* All (named) indices indexed by name */
  Hash trigHash;       /* All triggers indexed by name */
  Hash fkeyHash;       /* All foreign keys by referenced table name */
  Table *pSeqTab;      /* The sqlite_sequence table used by AUTOINCREMENT */
  u8 file_format;      /* Schema format version for this file */
  u8 enc;              /* Text encoding used by this database */
  u16 flags;           /* Flags associated with this schema */
  int cache_size;      /* Number of pages to use in the cache */
};

/*
** These macros can be used to test, set, or clear bits in the 
** Db.pSchema->flags field.
*/
#define DbHasProperty(D,I,P)     (((D)->aDb[I].pSchema->flags&(P))==(P))
#define DbHasAnyProperty(D,I,P)  (((D)->aDb[I].pSchema->flags&(P))!=0)
#define DbSetProperty(D,I,P)     (D)->aDb[I].pSchema->flags|=(P)
#define DbClearProperty(D,I,P)   (D)->aDb[I].pSchema->flags&=~(P)

/*
** Allowed values for the DB.pSchema->flags field.
**
** The DB_SchemaLoaded flag is set after the database schema has been
** read into internal hash tables.
**
** DB_UnresetViews means that one or more views have column names that
** have been filled out.  If the schema changes, these column names might
** changes and so the view will need to be reset.
*/
#define DB_SchemaLoaded    0x0001  /* The schema has been loaded */
#define DB_UnresetViews    0x0002  /* Some views have defined column names */
#define DB_Empty           0x0004  /* The file is empty (length 0 bytes) */

/*
** The number of different kinds of things that can be limited
** using the sqlite3_limit() interface.
*/
#define SQLITE_N_LIMIT (SQLITE_LIMIT_TRIGGER_DEPTH+1)

/*
** Lookaside malloc is a set of fixed-size buffers that can be used
** to satisfy small transient memory allocation requests for objects
** associated with a particular database connection.  The use of
** lookaside malloc provides a significant performance enhancement
** (approx 10%) by avoiding numerous malloc/free requests while parsing
** SQL statements.
**
** The Lookaside structure holds configuration information about the
** lookaside malloc subsystem.  Each available memory allocation in
** the lookaside subsystem is stored on a linked list of LookasideSlot
** objects.
**
** Lookaside allocations are only allowed for objects that are associated
** with a particular database connection.  Hence, schema information cannot
** be stored in lookaside because in shared cache mode the schema information
** is shared by multiple database connections.  Therefore, while parsing
** schema information, the Lookaside.bEnabled flag is cleared so that
** lookaside allocations are not used to construct the schema objects.
*/
struct Lookaside {
  u16 sz;                 /* Size of each buffer in bytes */
  u8 bEnabled;            /* False to disable new lookaside allocations */
  u8 bMalloced;           /* True if pStart obtained from sqlite3_malloc() */
  int nOut;               /* Number of buffers currently checked out */
  int mxOut;              /* Highwater mark for nOut */
  int anStat[3];          /* 0: hits.  1: size misses.  2: full misses */
  LookasideSlot *pFree;   /* List of available buffers */
  void *pStart;           /* First byte of available memory space */
  void *pEnd;             /* First byte past end of available space */
};
struct LookasideSlot {
  LookasideSlot *pNext;    /* Next buffer in the list of free buffers */
};

/*
** A hash table for function definitions.
**
** Hash each FuncDef structure into one of the FuncDefHash.a[] slots.
** Collisions are on the FuncDef.pHash chain.
*/
struct FuncDefHash {
  FuncDef *a[23];       /* Hash table for functions */
};

/*
** Each database connection is an instance of the following structure.
*/
struct sqlite3 {
  sqlite3_vfs *pVfs;            /* OS Interface */
  struct Vdbe *pVdbe;           /* List of active virtual machines */
  CollSeq *pDfltColl;           /* The default collating sequence (BINARY) */
  sqlite3_mutex *mutex;         /* Connection mutex */
  Db *aDb;                      /* All backends */
  int nDb;                      /* Number of backends currently in use */
  int flags;                    /* Miscellaneous flags. See below */
  i64 lastRowid;                /* ROWID of most recent insert (see above) */
  unsigned int openFlags;       /* Flags passed to sqlite3_vfs.xOpen() */
  int errCode;                  /* Most recent error code (SQLITE_*) */
  int errMask;                  /* & result codes with this before returning */
  u8 autoCommit;                /* The auto-commit flag. */
  u8 temp_store;                /* 1: file 2: memory 0: default */
  u8 mallocFailed;              /* True if we have seen a malloc failure */
  u8 dfltLockMode;              /* Default locking-mode for attached dbs */
  signed char nextAutovac;      /* Autovac setting after VACUUM if >=0 */
  u8 suppressErr;               /* Do not issue error messages if true */
  u8 vtabOnConflict;            /* Value to return for s3_vtab_on_conflict() */
  u8 isTransactionSavepoint;    /* True if the outermost savepoint is a TS */
  int nextPagesize;             /* Pagesize after VACUUM if >0 */
  u32 magic;                    /* Magic number for detect library misuse */
  int nChange;                  /* Value returned by sqlite3_changes() */
  int nTotalChange;             /* Value returned by sqlite3_total_changes() */
  int aLimit[SQLITE_N_LIMIT];   /* Limits */
  struct sqlite3InitInfo {      /* Information used during initialization */
    int newTnum;                /* Rootpage of table being initialized */
    u8 iDb;                     /* Which db file is being initialized */
    u8 busy;                    /* TRUE if currently initializing */
    u8 orphanTrigger;           /* Last statement is orphaned TEMP trigger */
  } init;
  int activeVdbeCnt;            /* Number of VDBEs currently executing */
  int writeVdbeCnt;             /* Number of active VDBEs that are writing */
  int vdbeExecCnt;              /* Number of nested calls to VdbeExec() */
  int nExtension;               /* Number of loaded extensions */
  void **aExtension;            /* Array of shared library handles */
  void (*xTrace)(void*,const char*);        /* Trace function */
  void *pTraceArg;                          /* Argument to the trace function */
  void (*xProfile)(void*,const char*,u64);  /* Profiling function */
  void *pProfileArg;                        /* Argument to profile function */
  void *pCommitArg;                 /* Argument to xCommitCallback() */   
  int (*xCommitCallback)(void*);    /* Invoked at every commit. */
  void *pRollbackArg;               /* Argument to xRollbackCallback() */   
  void (*xRollbackCallback)(void*); /* Invoked at every commit. */
  void *pUpdateArg;
  void (*xUpdateCallback)(void*,int, const char*,const char*,sqlite_int64);
#ifndef SQLITE_OMIT_WAL
  int (*xWalCallback)(void *, sqlite3 *, const char *, int);
  void *pWalArg;
#endif
  void(*xCollNeeded)(void*,sqlite3*,int eTextRep,const char*);
  void(*xCollNeeded16)(void*,sqlite3*,int eTextRep,const void*);
  void *pCollNeededArg;
  sqlite3_value *pErr;          /* Most recent error message */
  char *zErrMsg;                /* Most recent error message (UTF-8 encoded) */
  char *zErrMsg16;              /* Most recent error message (UTF-16 encoded) */
  union {
    volatile int isInterrupted; /* True if sqlite3_interrupt has been called */
    double notUsed1;            /* Spacer */
  } u1;
  Lookaside lookaside;          /* Lookaside malloc configuration */
#ifndef SQLITE_OMIT_AUTHORIZATION
  int (*xAuth)(void*,int,const char*,const char*,const char*,const char*);
                                /* Access authorization function */
  void *pAuthArg;               /* 1st argument to the access auth function */
#endif
#ifndef SQLITE_OMIT_PROGRESS_CALLBACK
  int (*xProgress)(void *);     /* The progress callback */
  void *pProgressArg;           /* Argument to the progress callback */
  int nProgressOps;             /* Number of opcodes for progress callback */
#endif
#ifndef SQLITE_OMIT_VIRTUALTABLE
  int nVTrans;                  /* Allocated size of aVTrans */
  Hash aModule;                 /* populated by sqlite3_create_module() */
  VtabCtx *pVtabCtx;            /* Context for active vtab connect/create */
  VTable **aVTrans;             /* Virtual tables with open transactions */
  VTable *pDisconnect;    /* Disconnect these in next sqlite3_prepare() */
#endif
  FuncDefHash aFunc;            /* Hash table of connection functions */
  Hash aCollSeq;                /* All collating sequences */
  BusyHandler busyHandler;      /* Busy callback */
  Db aDbStatic[2];              /* Static space for the 2 default backends */
  Savepoint *pSavepoint;        /* List of active savepoints */
  int busyTimeout;              /* Busy handler timeout, in msec */
  int nSavepoint;               /* Number of non-transaction savepoints */
  int nStatement;               /* Number of nested statement-transactions  */
  i64 nDeferredCons;            /* Net deferred constraints this transaction. */
  int *pnBytesFreed;            /* If not NULL, increment this in DbFree() */

#ifdef SQLITE_ENABLE_UNLOCK_NOTIFY
  /* The following variables are all protected by the STATIC_MASTER 
  ** mutex, not by sqlite3.mutex. They are used by code in notify.c. 
  **
  ** When X.pUnlockConnection==Y, that means that X is waiting for Y to
  ** unlock so that it can proceed.
  **
  ** When X.pBlockingConnection==Y, that means that something that X tried
  ** tried to do recently failed with an SQLITE_LOCKED error due to locks
  ** held by Y.
  */
  sqlite3 *pBlockingConnection; /* Connection that caused SQLITE_LOCKED */
  sqlite3 *pUnlockConnection;           /* Connection to watch for unlock */
  void *pUnlockArg;                     /* Argument to xUnlockNotify */
  void (*xUnlockNotify)(void **, int);  /* Unlock notify callback */
  sqlite3 *pNextBlocked;        /* Next in list of all blocked connections */
#endif
};

/*
** A macro to discover the encoding of a database.
*/
#define ENC(db) ((db)->aDb[0].pSchema->enc)

/*
** Possible values for the sqlite3.flags.
*/
#define SQLITE_VdbeTrace      0x00000100  /* True to trace VDBE execution */
#define SQLITE_InternChanges  0x00000200  /* Uncommitted Hash table changes */
#define SQLITE_FullColNames   0x00000400  /* Show full column names on SELECT */
#define SQLITE_ShortColNames  0x00000800  /* Show short columns names */
#define SQLITE_CountRows      0x00001000  /* Count rows changed by INSERT, */
                                          /*   DELETE, or UPDATE and return */
                                          /*   the count using a callback. */
#define SQLITE_NullCallback   0x00002000  /* Invoke the callback once if the */
                                          /*   result set is empty */
#define SQLITE_SqlTrace       0x00004000  /* Debug print SQL as it executes */
#define SQLITE_VdbeListing    0x00008000  /* Debug listings of VDBE programs */
#define SQLITE_WriteSchema    0x00010000  /* OK to update SQLITE_MASTER */
                         /*   0x00020000  Unused */
#define SQLITE_IgnoreChecks   0x00040000  /* Do not enforce check constraints */
#define SQLITE_ReadUncommitted 0x0080000  /* For shared-cache mode */
#define SQLITE_LegacyFileFmt  0x00100000  /* Create new databases in format 1 */
#define SQLITE_FullFSync      0x00200000  /* Use full fsync on the backend */
#define SQLITE_CkptFullFSync  0x00400000  /* Use full fsync for checkpoint */
#define SQLITE_RecoveryMode   0x00800000  /* Ignore schema errors */
#define SQLITE_ReverseOrder   0x01000000  /* Reverse unordered SELECTs */
#define SQLITE_RecTriggers    0x02000000  /* Enable recursive triggers */
#define SQLITE_ForeignKeys    0x04000000  /* Enforce foreign key constraints  */
#define SQLITE_AutoIndex      0x08000000  /* Enable automatic indexes */
#define SQLITE_PreferBuiltin  0x10000000  /* Preference to built-in funcs */
#define SQLITE_LoadExtension  0x20000000  /* Enable load_extension */
#define SQLITE_EnableTrigger  0x40000000  /* True to enable triggers */

/*
** Bits of the sqlite3.flags field that are used by the
** sqlite3_test_control(SQLITE_TESTCTRL_OPTIMIZATIONS,...) interface.
** These must be the low-order bits of the flags field.
*/
#define SQLITE_QueryFlattener 0x01        /* Disable query flattening */
#define SQLITE_ColumnCache    0x02        /* Disable the column cache */
#define SQLITE_IndexSort      0x04        /* Disable indexes for sorting */
#define SQLITE_IndexSearch    0x08        /* Disable indexes for searching */
#define SQLITE_IndexCover     0x10        /* Disable index covering table */
#define SQLITE_GroupByOrder   0x20        /* Disable GROUPBY cover of ORDERBY */
#define SQLITE_FactorOutConst 0x40        /* Disable factoring out constants */
#define SQLITE_IdxRealAsInt   0x80        /* Store REAL as INT in indices */
#define SQLITE_DistinctOpt    0x80        /* DISTINCT using indexes */
#define SQLITE_OptMask        0xff        /* Mask of all disablable opts */

/*
** Possible values for the sqlite.magic field.
** The numbers are obtained at random and have no special meaning, other
** than being distinct from one another.
*/
#define SQLITE_MAGIC_OPEN     0xa029a697  /* Database is open */
#define SQLITE_MAGIC_CLOSED   0x9f3c2d33  /* Database is closed */
#define SQLITE_MAGIC_SICK     0x4b771290  /* Error and awaiting close */
#define SQLITE_MAGIC_BUSY     0xf03b7906  /* Database currently in use */
#define SQLITE_MAGIC_ERROR    0xb5357930  /* An SQLITE_MISUSE error occurred */
#define SQLITE_MAGIC_ZOMBIE   0x64cffc7f  /* Close with last statement close */

/*
** Each SQL function is defined by an instance of the following
** structure.  A pointer to this structure is stored in the sqlite.aFunc
** hash table.  When multiple functions have the same name, the hash table
** points to a linked list of these structures.
*/
struct FuncDef {
  i16 nArg;            /* Number of arguments.  -1 means unlimited */
  u8 iPrefEnc;         /* Preferred text encoding (SQLITE_UTF8, 16LE, 16BE) */
  u8 flags;            /* Some combination of SQLITE_FUNC_* */
  void *pUserData;     /* User data parameter */
  FuncDef *pNext;      /* Next function with same name */
  void (*xFunc)(sqlite3_context*,int,sqlite3_value**); /* Regular function */
  void (*xStep)(sqlite3_context*,int,sqlite3_value**); /* Aggregate step */
  void (*xFinalize)(sqlite3_context*);                /* Aggregate finalizer */
  char *zName;         /* SQL name of the function. */
  FuncDef *pHash;      /* Next with a different name but the same hash */
  FuncDestructor *pDestructor;   /* Reference counted destructor function */
};

/*
** This structure encapsulates a user-function destructor callback (as
** configured using create_function_v2()) and a reference counter. When
** create_function_v2() is called to create a function with a destructor,
** a single object of this type is allocated. FuncDestructor.nRef is set to 
** the number of FuncDef objects created (either 1 or 3, depending on whether
** or not the specified encoding is SQLITE_ANY). The FuncDef.pDestructor
** member of each of the new FuncDef objects is set to point to the allocated
** FuncDestructor.
**
** Thereafter, when one of the FuncDef objects is deleted, the reference
** count on this object is decremented. When it reaches 0, the destructor
** is invoked and the FuncDestructor structure freed.
*/
struct FuncDestructor {
  int nRef;
  void (*xDestroy)(void *);
  void *pUserData;
};

/*
** Possible values for FuncDef.flags.  Note that the _LENGTH and _TYPEOF
** values must correspond to OPFLAG_LENGTHARG and OPFLAG_TYPEOFARG.  There
** are assert() statements in the code to verify this.
*/
#define SQLITE_FUNC_LIKE     0x01 /* Candidate for the LIKE optimization */
#define SQLITE_FUNC_CASE     0x02 /* Case-sensitive LIKE-type function */
#define SQLITE_FUNC_EPHEM    0x04 /* Ephemeral.  Delete with VDBE */
#define SQLITE_FUNC_NEEDCOLL 0x08 /* sqlite3GetFuncCollSeq() might be called */
#define SQLITE_FUNC_COUNT    0x10 /* Built-in count(*) aggregate */
#define SQLITE_FUNC_COALESCE 0x20 /* Built-in coalesce() or ifnull() function */
#define SQLITE_FUNC_LENGTH   0x40 /* Built-in length() function */
#define SQLITE_FUNC_TYPEOF   0x80 /* Built-in typeof() function */

/*
** The following three macros, FUNCTION(), LIKEFUNC() and AGGREGATE() are
** used to create the initializers for the FuncDef structures.
**
**   FUNCTION(zName, nArg, iArg, bNC, xFunc)
**     Used to create a scalar function definition of a function zName 
**     implemented by C function xFunc that accepts nArg arguments. The
**     value passed as iArg is cast to a (void*) and made available
**     as the user-data (sqlite3_user_data()) for the function. If 
**     argument bNC is true, then the SQLITE_FUNC_NEEDCOLL flag is set.
**
**   AGGREGATE(zName, nArg, iArg, bNC, xStep, xFinal)
**     Used to create an aggregate function definition implemented by
**     the C functions xStep and xFinal. The first four parameters
**     are interpreted in the same way as the first 4 parameters to
**     FUNCTION().
**
**   LIKEFUNC(zName, nArg, pArg, flags)
**     Used to create a scalar function definition of a function zName 
**     that accepts nArg arguments and is implemented by a call to C 
**     function likeFunc. Argument pArg is cast to a (void *) and made
**     available as the function user-data (sqlite3_user_data()). The
**     FuncDef.flags variable is set to the value passed as the flags
**     parameter.
*/
#define FUNCTION(zName, nArg, iArg, bNC, xFunc) \
  {nArg, SQLITE_UTF8, (bNC*SQLITE_FUNC_NEEDCOLL), \
   SQLITE_INT_TO_PTR(iArg), 0, xFunc, 0, 0, #zName, 0, 0}
#define FUNCTION2(zName, nArg, iArg, bNC, xFunc, extraFlags) \
  {nArg, SQLITE_UTF8, (bNC*SQLITE_FUNC_NEEDCOLL)|extraFlags, \
   SQLITE_INT_TO_PTR(iArg), 0, xFunc, 0, 0, #zName, 0, 0}
#define STR_FUNCTION(zName, nArg, pArg, bNC, xFunc) \
  {nArg, SQLITE_UTF8, bNC*SQLITE_FUNC_NEEDCOLL, \
   pArg, 0, xFunc, 0, 0, #zName, 0, 0}
#define LIKEFUNC(zName, nArg, arg, flags) \
  {nArg, SQLITE_UTF8, flags, (void *)arg, 0, likeFunc, 0, 0, #zName, 0, 0}
#define AGGREGATE(zName, nArg, arg, nc, xStep, xFinal) \
  {nArg, SQLITE_UTF8, nc*SQLITE_FUNC_NEEDCOLL, \
   SQLITE_INT_TO_PTR(arg), 0, 0, xStep,xFinal,#zName,0,0}

/*
** All current savepoints are stored in a linked list starting at
** sqlite3.pSavepoint. The first element in the list is the most recently
** opened savepoint. Savepoints are added to the list by the vdbe
** OP_Savepoint instruction.
*/
struct Savepoint {
  char *zName;                        /* Savepoint name (nul-terminated) */
  i64 nDeferredCons;                  /* Number of deferred fk violations */
  Savepoint *pNext;                   /* Parent savepoint (if any) */
};

/*
** The following are used as the second parameter to sqlite3Savepoint(),
** and as the P1 argument to the OP_Savepoint instruction.
** 下面的宏定义用作函数sqlite3Savepoint()的第2个参数，同时在操作码Savepoint中，P1要与它们作比较判断。
*/
#define SAVEPOINT_BEGIN      0
#define SAVEPOINT_RELEASE    1
#define SAVEPOINT_ROLLBACK   2


/*
** Each SQLite module (virtual table definition) is defined by an
** instance of the following structure, stored in the sqlite3.aModule
** hash table.
*/
struct Module {
  const sqlite3_module *pModule;       /* Callback pointers */
  const char *zName;                   /* Name passed to create_module() */
  void *pAux;                          /* pAux passed to create_module() */
  void (*xDestroy)(void *);            /* Module destructor function */
};

/*
** information about each column of an SQL table is held in an instance
** of this structure.
*/
struct Column {
  char *zName;     /* Name of this column */
  Expr *pDflt;     /* Default value of this column */
  char *zDflt;     /* Original text of the default value */
  char *zType;     /* Data type for this column */
  char *zColl;     /* Collating sequence.  If NULL, use the default */
  u8 notNull;      /* True if there is a NOT NULL constraint */
  u8 isPrimKey;    /* True if this column is part of the PRIMARY KEY */
  char affinity;   /* One of the SQLITE_AFF_... values */
#ifndef SQLITE_OMIT_VIRTUALTABLE
  u8 isHidden;     /* True if this column is 'hidden' */
#endif
};

/*
** A "Collating Sequence" is defined by an instance of the following
** structure. Conceptually, a collating sequence consists of a name and
** a comparison routine that defines the order of that sequence.
** 一个“排序序列”就是下面这个结构体的一个实例。从概念上讲，一个排序序列由一个名称，以及
** 一个用于对序列进行比较、排序的程序(不知道怎么翻译routine)构成。
**
** There may two separate implementations of the collation function, one
** that processes text in UTF-8 encoding (CollSeq.xCmp) and another that
** processes text encoded in UTF-16 (CollSeq.xCmp16), using the machine
** native byte order. When a collation sequence is invoked, SQLite selects
** the version that will require the least expensive encoding
** translations, if any.
** 排序函数有两个不同的的实现版本，一个处理utf-8编码的文本(CollSeq.xCmp)，另一个处理utf - 16
** 编码的文本(CollSeq.xCmp16)，两种都使用计算机原生的字节顺序。在调用排序序列时，如果条件允许，
** SQLite会选择编码转换代价最小的版本。
**
** The CollSeq.pUser member variable is an extra parameter that passed in
** as the first argument to the UTF-8 comparison function, xCmp.
** CollSeq.pUser16 is the equivalent for the UTF-16 comparison function,
** xCmp16.
** 成员变量CollSeq.pUser是一个额外的参数，它作为第一个参数传递到utf-8版本的比较函数xCmp()。
** CollSeq.pUser16对于编码为utf-16的比较函数xCmp16有同等意义。
**
** If both CollSeq.xCmp and CollSeq.xCmp16 are NULL, it means that the
** collating sequence is undefined.  Indices built on an undefined
** collating sequence may not be read or written.
** 如果两个CollSeq.xCmp和CollSeq.xCmp16都是NULL，这意味着排序序列没有定义。
** 索引建立在一个未定义的排序序列可能不会被读或写。
*/
struct CollSeq {
  char *zName;          /* Name of the collating sequence, UTF-8 encoded */
  u8 enc;               /* Text encoding handled by xCmp() */
  void *pUser;          /* First argument to xCmp() */
  int (*xCmp)(void*,int, const void*, int, const void*);
  void (*xDel)(void*);  /* Destructor for pUser */
};

/*
** A sort order can be either ASC or DESC.
*/
#define SQLITE_SO_ASC       0  /* Sort in ascending order */
#define SQLITE_SO_DESC      1  /* Sort in ascending order */

/*
** Column affinity types.
** 列关联类型。
**
** These used to have mnemonic name like 'i' for SQLITE_AFF_INTEGER and
** 't' for SQLITE_AFF_TEXT.  But we can save a little space and improve
** the speed a little by numbering the values consecutively.
** 这些宏定义的作用是让我们有一个便于记忆的名字，比如‘i’对应SQLITE_AFF_INTEGER，‘t’对应SQLITE_AFF_TEXT。
** 但是我们可以通过给变量连续编号来节省一些内存空间，同时提升一点运行速度
**
** But rather than start with 0 or 1, we begin with 'a'.  That way,
** when multiple affinity types are concatenated into a string and
** used as the P4 operand, they will be more readable.
** 不过，这里我们从“a”开始编号，而不是从0或1开始。这样,当多个关联类型连接成一个字符串,
** 并且被用作P4操作数时，他们将更具可读性。
**
** Note also that the numeric types are grouped together so that testing
** for a numeric type is a single comparison.
** 还要注意，这些数值类型组合在一起是为了方便对每一个数值类型进行测试，这种测试仅仅是一种比较。
*/
#define SQLITE_AFF_TEXT     'a'
#define SQLITE_AFF_NONE     'b'
#define SQLITE_AFF_NUMERIC  'c'
#define SQLITE_AFF_INTEGER  'd'
#define SQLITE_AFF_REAL     'e'

#define sqlite3IsNumericAffinity(X)  ((X)>=SQLITE_AFF_NUMERIC)

/*
** The SQLITE_AFF_MASK values masks off the significant bits of an
** affinity value.
** SQLITE_AFF_MASK相当于一个掩码，与一个代表关联性的二进制位相对应。
*/
#define SQLITE_AFF_MASK     0x67

/*
** Additional bit values that can be ORed with an affinity without
** changing the affinity.
** 另外一些二进制值。
*/
#define SQLITE_JUMPIFNULL   0x08  /* jumps if either operand is NULL 
								  ** 如果任何一个操作数是空，就跳转
								  */
#define SQLITE_STOREP2      0x10  /* Store result in reg[P2] rather than jump */
								  ** 结果存储在寄存器P2中，不进行跳转
								  */
#define SQLITE_NULLEQ       0x80  /* NULL=NULL */

/*
** An object of this type is created for each virtual table present in
** the database schema.
**
** If the database schema is shared, then there is one instance of this
** structure for each database connection (sqlite3*) that uses the shared
** schema. This is because each database connection requires its own unique
** instance of the sqlite3_vtab* handle used to access the virtual table
** implementation. sqlite3_vtab* handles can not be shared between
** database connections, even when the rest of the in-memory database
** schema is shared, as the implementation often stores the database
** connection handle passed to it via the xConnect() or xCreate() method
** during initialization internally. This database connection handle may
** then be used by the virtual table implementation to access real tables
** within the database. So that they appear as part of the callers
** transaction, these accesses need to be made via the same database
** connection as that used to execute SQL operations on the virtual table.
**
** All VTable objects that correspond to a single table in a shared
** database schema are initially stored in a linked-list pointed to by
** the Table.pVTable member variable of the corresponding Table object.
** When an sqlite3_prepare() operation is required to access the virtual
** table, it searches the list for the VTable that corresponds to the
** database connection doing the preparing so as to use the correct
** sqlite3_vtab* handle in the compiled query.
**
** When an in-memory Table object is deleted (for example when the
** schema is being reloaded for some reason), the VTable objects are not 
** deleted and the sqlite3_vtab* handles are not xDisconnect()ed 
** immediately. Instead, they are moved from the Table.pVTable list to
** another linked list headed by the sqlite3.pDisconnect member of the
** corresponding sqlite3 structure. They are then deleted/xDisconnected 
** next time a statement is prepared using said sqlite3*. This is done
** to avoid deadlock issues involving multiple sqlite3.mutex mutexes.
** Refer to comments above function sqlite3VtabUnlockList() for an
** explanation as to why it is safe to add an entry to an sqlite3.pDisconnect
** list without holding the corresponding sqlite3.mutex mutex.
**
** The memory for objects of this type is always allocated by 
** sqlite3DbMalloc(), using the connection handle stored in VTable.db as 
** the first argument.
*/
struct VTable {
  sqlite3 *db;              /* Database connection associated with this table */
  Module *pMod;             /* Pointer to module implementation */
  sqlite3_vtab *pVtab;      /* Pointer to vtab instance */
  int nRef;                 /* Number of pointers to this structure */
  u8 bConstraint;           /* True if constraints are supported */
  int iSavepoint;           /* Depth of the SAVEPOINT stack */
  VTable *pNext;            /* Next in linked list (see above) */
};

/*
** Each SQL table is represented in memory by an instance of the
** following structure.
**
** Table.zName is the name of the table.  The case of the original
** CREATE TABLE statement is stored, but case is not significant for
** comparisons.
**
** Table.nCol is the number of columns in this table.  Table.aCol is a
** pointer to an array of Column structures, one for each column.
**
** If the table has an INTEGER PRIMARY KEY, then Table.iPKey is the index of
** the column that is that key.   Otherwise Table.iPKey is negative.  Note
** that the datatype of the PRIMARY KEY must be INTEGER for this field to
** be set.  An INTEGER PRIMARY KEY is used as the rowid for each row of
** the table.  If a table has no INTEGER PRIMARY KEY, then a random rowid
** is generated for each row of the table.  TF_HasPrimaryKey is set if
** the table has any PRIMARY KEY, INTEGER or otherwise.
**
** Table.tnum is the page number for the root BTree page of the table in the
** database file.  If Table.iDb is the index of the database table backend
** in sqlite.aDb[].  0 is for the main database and 1 is for the file that
** holds temporary tables and indices.  If TF_Ephemeral is set
** then the table is stored in a file that is automatically deleted
** when the VDBE cursor to the table is closed.  In this case Table.tnum 
** refers VDBE cursor number that holds the table open, not to the root
** page number.  Transient tables are used to hold the results of a
** sub-query that appears instead of a real table name in the FROM clause 
** of a SELECT statement.
*/
struct Table {
  char *zName;         /* Name of the table or view */
  int iPKey;           /* If not negative, use aCol[iPKey] as the primary key */
  int nCol;            /* Number of columns in this table */
  Column *aCol;        /* Information about each column */
  Index *pIndex;       /* List of SQL indexes on this table. */
  int tnum;            /* Root BTree node for this table (see note above) */
  tRowcnt nRowEst;     /* Estimated rows in table - from sqlite_stat1 table */
  Select *pSelect;     /* NULL for tables.  Points to definition if a view. */
  u16 nRef;            /* Number of pointers to this Table */
  u8 tabFlags;         /* Mask of TF_* values */
  u8 keyConf;          /* What to do in case of uniqueness conflict on iPKey */
  FKey *pFKey;         /* Linked list of all foreign keys in this table */
  char *zColAff;       /* String defining the affinity of each column */
#ifndef SQLITE_OMIT_CHECK
  ExprList *pCheck;    /* All CHECK constraints */
#endif
#ifndef SQLITE_OMIT_ALTERTABLE
  int addColOffset;    /* Offset in CREATE TABLE stmt to add a new column */
#endif
#ifndef SQLITE_OMIT_VIRTUALTABLE
  VTable *pVTable;     /* List of VTable objects. */
  int nModuleArg;      /* Number of arguments to the module */
  char **azModuleArg;  /* Text of all module args. [0] is module name */
#endif
  Trigger *pTrigger;   /* List of triggers stored in pSchema */
  Schema *pSchema;     /* Schema that contains this table */
  Table *pNextZombie;  /* Next on the Parse.pZombieTab list */
};

/*
** Allowed values for Tabe.tabFlags.
*/
#define TF_Readonly        0x01    /* Read-only system table */
#define TF_Ephemeral       0x02    /* An ephemeral table */
#define TF_HasPrimaryKey   0x04    /* Table has a primary key */
#define TF_Autoincrement   0x08    /* Integer primary key is autoincrement */
#define TF_Virtual         0x10    /* Is a virtual table */


/*
** Test to see whether or not a table is a virtual table.  This is
** done as a macro so that it will be optimized out when virtual
** table support is omitted from the build.
*/
#ifndef SQLITE_OMIT_VIRTUALTABLE
#  define IsVirtual(X)      (((X)->tabFlags & TF_Virtual)!=0)
#  define IsHiddenColumn(X) ((X)->isHidden)
#else
#  define IsVirtual(X)      0
#  define IsHiddenColumn(X) 0
#endif

/*
** Each foreign key constraint is an instance of the following structure.
**
** A foreign key is associated with two tables.  The "from" table is
** the table that contains the REFERENCES clause that creates the foreign
** key.  The "to" table is the table that is named in the REFERENCES clause.
** Consider this example:
**
**     CREATE TABLE ex1(
**       a INTEGER PRIMARY KEY,
**       b INTEGER CONSTRAINT fk1 REFERENCES ex2(x)
**     );
**
** For foreign key "fk1", the from-table is "ex1" and the to-table is "ex2".
**
** Each REFERENCES clause generates an instance of the following structure
** which is attached to the from-table.  The to-table need not exist when
** the from-table is created.  The existence of the to-table is not checked.
*/
struct FKey {
  Table *pFrom;     /* Table containing the REFERENCES clause (aka: Child) */
  FKey *pNextFrom;  /* Next foreign key in pFrom */
  char *zTo;        /* Name of table that the key points to (aka: Parent) */
  FKey *pNextTo;    /* Next foreign key on table named zTo */
  FKey *pPrevTo;    /* Previous foreign key on table named zTo */
  int nCol;         /* Number of columns in this key */
  /* EV: R-30323-21917 */
  u8 isDeferred;    /* True if constraint checking is deferred till COMMIT */
  u8 aAction[2];          /* ON DELETE and ON UPDATE actions, respectively */
  Trigger *apTrigger[2];  /* Triggers for aAction[] actions */
  struct sColMap {  /* Mapping of columns in pFrom to columns in zTo */
    int iFrom;         /* Index of column in pFrom */
    char *zCol;        /* Name of column in zTo.  If 0 use PRIMARY KEY */
  } aCol[1];        /* One entry for each of nCol column s */
};

/*
** SQLite supports many different ways to resolve a constraint
** error.  ROLLBACK processing means that a constraint violation
** causes the operation in process to fail and for the current transaction
** to be rolled back.  ABORT processing means the operation in process
** fails and any prior changes from that one operation are backed out,
** but the transaction is not rolled back.  FAIL processing means that
** the operation in progress stops and returns an error code.  But prior
** changes due to the same operation are not backed out and no rollback
** occurs.  IGNORE means that the particular row that caused the constraint
** error is not inserted or updated.  Processing continues and no error
** is returned.  REPLACE means that preexisting database rows that caused
** a UNIQUE constraint violation are removed so that the new insert or
** update can proceed.  Processing continues and no error is reported.
**
** RESTRICT, SETNULL, and CASCADE actions apply only to foreign keys.
** RESTRICT is the same as ABORT for IMMEDIATE foreign keys and the
** same as ROLLBACK for DEFERRED keys.  SETNULL means that the foreign
** key is set to NULL.  CASCADE means that a DELETE or UPDATE of the
** referenced table row is propagated into the row that holds the
** foreign key.
**
** The following symbolic values are used to record which type
** of action to take.
*/
#define OE_None     0   /* There is no constraint to check */
#define OE_Rollback 1   /* Fail the operation and rollback the transaction */
#define OE_Abort    2   /* Back out changes but do no rollback transaction */
#define OE_Fail     3   /* Stop the operation but leave all prior changes */
#define OE_Ignore   4   /* Ignore the error. Do not do the INSERT or UPDATE */
#define OE_Replace  5   /* Delete existing record, then do INSERT or UPDATE */

#define OE_Restrict 6   /* OE_Abort for IMMEDIATE, OE_Rollback for DEFERRED */
#define OE_SetNull  7   /* Set the foreign key value to NULL */
#define OE_SetDflt  8   /* Set the foreign key value to its default */
#define OE_Cascade  9   /* Cascade the changes */

#define OE_Default  99  /* Do whatever the default action is */


/*
** An instance of the following structure is passed as the first
** argument to sqlite3VdbeKeyCompare and is used to control the
** comparison of the two index keys.
** 下面这个结构体的实例会作为第一个参数传递给sqlite3VdbeKeyCompare方法，还会被用于控制两个索引键的比较。
*/
struct KeyInfo {
  sqlite3 *db;        /* The database connection */
  u8 enc;             /* Text encoding - one of the SQLITE_UTF* values */
  u16 nField;         /* Number of entries in aColl[] */
  u8 *aSortOrder;     /* Sort order for each column.  May be NULL */
  CollSeq *aColl[1];  /* Collating sequence for each term of the key */
};

/*
** An instance of the following structure holds information about a
** single index record that has already been parsed out into individual
** values.
**
** A record is an object that contains one or more fields of data.
** Records are used to store the content of a table row and to store
** the key of an index.  A blob encoding of a record is created by
** the OP_MakeRecord opcode of the VDBE and is disassembled by the
** OP_Column opcode.
**
** This structure holds a record that has already been disassembled
** into its constituent fields.
*/
struct UnpackedRecord {
  KeyInfo *pKeyInfo;  /* Collation and sort-order information */
  u16 nField;         /* Number of entries in apMem[] */
  u8 flags;           /* Boolean settings.  UNPACKED_... below */
  i64 rowid;          /* Used by UNPACKED_PREFIX_SEARCH */
  Mem *aMem;          /* Values */
};

/*
** Allowed values of UnpackedRecord.flags
*/
#define UNPACKED_INCRKEY       0x01  /* Make this key an epsilon larger */
#define UNPACKED_PREFIX_MATCH  0x02  /* A prefix match is considered OK */
#define UNPACKED_PREFIX_SEARCH 0x04  /* Ignore final (rowid) field */

/*
** Each SQL index is represented in memory by an
** instance of the following structure.
**
** The columns of the table that are to be indexed are described
** by the aiColumn[] field of this structure.  For example, suppose
** we have the following table and index:
**
**     CREATE TABLE Ex1(c1 int, c2 int, c3 text);
**     CREATE INDEX Ex2 ON Ex1(c3,c1);
**
** In the Table structure describing Ex1, nCol==3 because there are
** three columns in the table.  In the Index structure describing
** Ex2, nColumn==2 since 2 of the 3 columns of Ex1 are indexed.
** The value of aiColumn is {2, 0}.  aiColumn[0]==2 because the 
** first column to be indexed (c3) has an index of 2 in Ex1.aCol[].
** The second column to be indexed (c1) has an index of 0 in
** Ex1.aCol[], hence Ex2.aiColumn[1]==0.
**
** The Index.onError field determines whether or not the indexed columns
** must be unique and what to do if they are not.  When Index.onError=OE_None,
** it means this is not a unique index.  Otherwise it is a unique index
** and the value of Index.onError indicate the which conflict resolution 
** algorithm to employ whenever an attempt is made to insert a non-unique
** element.
*/
struct Index {
  char *zName;     /* Name of this index */
  int *aiColumn;   /* Which columns are used by this index.  1st is 0 */
  tRowcnt *aiRowEst; /* Result of ANALYZE: Est. rows selected by each column */
  Table *pTable;   /* The SQL table being indexed */
  char *zColAff;   /* String defining the affinity of each column */
  Index *pNext;    /* The next index associated with the same table */
  Schema *pSchema; /* Schema containing this index */
  u8 *aSortOrder;  /* Array of size Index.nColumn. True==DESC, False==ASC */
  char **azColl;   /* Array of collation sequence names for index */
  int nColumn;     /* Number of columns in the table used by this index */
  int tnum;        /* Page containing root of this index in database file */
  u8 onError;      /* OE_Abort, OE_Ignore, OE_Replace, or OE_None */
  u8 autoIndex;    /* True if is automatically created (ex: by UNIQUE) */
  u8 bUnordered;   /* Use this index for == or IN queries only */
#ifdef SQLITE_ENABLE_STAT3
  int nSample;             /* Number of elements in aSample[] */
  tRowcnt avgEq;           /* Average nEq value for key values not in aSample */
  IndexSample *aSample;    /* Samples of the left-most key */
#endif
};

/*
** Each sample stored in the sqlite_stat3 table is represented in memory 
** using a structure of this type.  See documentation at the top of the
** analyze.c source file for additional information.
*/
struct IndexSample {
  union {
    char *z;        /* Value if eType is SQLITE_TEXT or SQLITE_BLOB */
    double r;       /* Value if eType is SQLITE_FLOAT */
    i64 i;          /* Value if eType is SQLITE_INTEGER */
  } u;
  u8 eType;         /* SQLITE_NULL, SQLITE_INTEGER ... etc. */
  int nByte;        /* Size in byte of text or blob. */
  tRowcnt nEq;      /* Est. number of rows where the key equals this sample */
  tRowcnt nLt;      /* Est. number of rows where key is less than this sample */
  tRowcnt nDLt;     /* Est. number of distinct keys less than this sample */
};

/*
** Each token coming out of the lexer is an instance of
** this structure.  Tokens are also used as part of an expression.
**
** Note if Token.z==0 then Token.dyn and Token.n are undefined and
** may contain random values.  Do not make any assumptions about Token.dyn
** and Token.n when Token.z==0.
*/
struct Token {
  const char *z;     /* Text of the token.  Not NULL-terminated! */
  unsigned int n;    /* Number of characters in this token */
};

/*
** An instance of this structure contains information needed to generate
** code for a SELECT that contains aggregate functions.
**
** If Expr.op==TK_AGG_COLUMN or TK_AGG_FUNCTION then Expr.pAggInfo is a
** pointer to this structure.  The Expr.iColumn field is the index in
** AggInfo.aCol[] or AggInfo.aFunc[] of information needed to generate
** code for that node.
**
** AggInfo.pGroupBy and AggInfo.aFunc.pExpr point to fields within the
** original Select structure that describes the SELECT statement.  These
** fields do not need to be freed when deallocating the AggInfo structure.
*/
struct AggInfo {
  u8 directMode;          /* Direct rendering mode means take data directly
                          ** from source tables rather than from accumulators */
  u8 useSortingIdx;       /* In direct mode, reference the sorting index rather
                          ** than the source table */
  int sortingIdx;         /* Cursor number of the sorting index */
  int sortingIdxPTab;     /* Cursor number of pseudo-table */
  int nSortingColumn;     /* Number of columns in the sorting index */
  ExprList *pGroupBy;     /* The group by clause */
  struct AggInfo_col {    /* For each column used in source tables */
    Table *pTab;             /* Source table */
    int iTable;              /* Cursor number of the source table */
    int iColumn;             /* Column number within the source table */
    int iSorterColumn;       /* Column number in the sorting index */
    int iMem;                /* Memory location that acts as accumulator */
    Expr *pExpr;             /* The original expression */
  } *aCol;
  int nColumn;            /* Number of used entries in aCol[] */
  int nAccumulator;       /* Number of columns that show through to the output.
                          ** Additional columns are used only as parameters to
                          ** aggregate functions */
  struct AggInfo_func {   /* For each aggregate function */
    Expr *pExpr;             /* Expression encoding the function */
    FuncDef *pFunc;          /* The aggregate function implementation */
    int iMem;                /* Memory location that acts as accumulator */
    int iDistinct;           /* Ephemeral table used to enforce DISTINCT */
  } *aFunc;
  int nFunc;              /* Number of entries in aFunc[] */
};

/*
** The datatype ynVar is a signed integer, either 16-bit or 32-bit.
** Usually it is 16-bits.  But if SQLITE_MAX_VARIABLE_NUMBER is greater
** than 32767 we have to make it 32-bit.  16-bit is preferred because
** it uses less memory in the Expr object, which is a big memory user
** in systems with lots of prepared statements.  And few applications
** need more than about 10 or 20 variables.  But some extreme users want
** to have prepared statements with over 32767 variables, and for them
** the option is available (at compile-time).
*/
#if SQLITE_MAX_VARIABLE_NUMBER<=32767
typedef i16 ynVar;
#else
typedef int ynVar;
#endif

/*
** Each node of an expression in the parse tree is an instance
** of this structure.
**
** Expr.op is the opcode. The integer parser token codes are reused
** as opcodes here. For example, the parser defines TK_GE to be an integer
** code representing the ">=" operator. This same integer code is reused
** to represent the greater-than-or-equal-to operator in the expression
** tree.
**
** If the expression is an SQL literal (TK_INTEGER, TK_FLOAT, TK_BLOB, 
** or TK_STRING), then Expr.token contains the text of the SQL literal. If
** the expression is a variable (TK_VARIABLE), then Expr.token contains the 
** variable name. Finally, if the expression is an SQL function (TK_FUNCTION),
** then Expr.token contains the name of the function.
**
** Expr.pRight and Expr.pLeft are the left and right subexpressions of a
** binary operator. Either or both may be NULL.
**
** Expr.x.pList is a list of arguments if the expression is an SQL function,
** a CASE expression or an IN expression of the form "<lhs> IN (<y>, <z>...)".
** Expr.x.pSelect is used if the expression is a sub-select or an expression of
** the form "<lhs> IN (SELECT ...)". If the EP_xIsSelect bit is set in the
** Expr.flags mask, then Expr.x.pSelect is valid. Otherwise, Expr.x.pList is 
** valid.
**
** An expression of the form ID or ID.ID refers to a column in a table.
** For such expressions, Expr.op is set to TK_COLUMN and Expr.iTable is
** the integer cursor number of a VDBE cursor pointing to that table and
** Expr.iColumn is the column number for the specific column.  If the
** expression is used as a result in an aggregate SELECT, then the
** value is also stored in the Expr.iAgg column in the aggregate so that
** it can be accessed after all aggregates are computed.
**
** If the expression is an unbound variable marker (a question mark 
** character '?' in the original SQL) then the Expr.iTable holds the index 
** number for that variable.
**
** If the expression is a subquery then Expr.iColumn holds an integer
** register number containing the result of the subquery.  If the
** subquery gives a constant result, then iTable is -1.  If the subquery
** gives a different answer at different times during statement processing
** then iTable is the address of a subroutine that computes the subquery.
**
** If the Expr is of type OP_Column, and the table it is selecting from
** is a disk table or the "old.*" pseudo-table, then pTab points to the
** corresponding table definition.
**
** ALLOCATION NOTES:
**
** Expr objects can use a lot of memory space in database schema.  To
** help reduce memory requirements, sometimes an Expr object will be
** truncated.  And to reduce the number of memory allocations, sometimes
** two or more Expr objects will be stored in a single memory allocation,
** together with Expr.zToken strings.
**
** If the EP_Reduced and EP_TokenOnly flags are set when
** an Expr object is truncated.  When EP_Reduced is set, then all
** the child Expr objects in the Expr.pLeft and Expr.pRight subtrees
** are contained within the same memory allocation.  Note, however, that
** the subtrees in Expr.x.pList or Expr.x.pSelect are always separately
** allocated, regardless of whether or not EP_Reduced is set.
*/
struct Expr {
  u8 op;                 /* Operation performed by this node */
  char affinity;         /* The affinity of the column or 0 if not a column */
  u16 flags;             /* Various flags.  EP_* See below */
  union {
    char *zToken;          /* Token value. Zero terminated and dequoted */
    int iValue;            /* Non-negative integer value if EP_IntValue */
  } u;

  /* If the EP_TokenOnly flag is set in the Expr.flags mask, then no
  ** space is allocated for the fields below this point. An attempt to
  ** access them will result in a segfault or malfunction. 
  *********************************************************************/

  Expr *pLeft;           /* Left subnode */
  Expr *pRight;          /* Right subnode */
  union {
    ExprList *pList;     /* Function arguments or in "<expr> IN (<expr-list)" */
    Select *pSelect;     /* Used for sub-selects and "<expr> IN (<select>)" */
  } x;
  CollSeq *pColl;        /* The collation type of the column or 0 */

  /* If the EP_Reduced flag is set in the Expr.flags mask, then no
  ** space is allocated for the fields below this point. An attempt to
  ** access them will result in a segfault or malfunction.
  *********************************************************************/

  int iTable;            /* TK_COLUMN: cursor number of table holding column
                         ** TK_REGISTER: register number
                         ** TK_TRIGGER: 1 -> new, 0 -> old */
  ynVar iColumn;         /* TK_COLUMN: column index.  -1 for rowid.
                         ** TK_VARIABLE: variable number (always >= 1). */
  i16 iAgg;              /* Which entry in pAggInfo->aCol[] or ->aFunc[] */
  i16 iRightJoinTable;   /* If EP_FromJoin, the right table of the join */
  u8 flags2;             /* Second set of flags.  EP2_... */
  u8 op2;                /* TK_REGISTER: original value of Expr.op
                         ** TK_COLUMN: the value of p5 for OP_Column
                         ** TK_AGG_FUNCTION: nesting depth */
  AggInfo *pAggInfo;     /* Used by TK_AGG_COLUMN and TK_AGG_FUNCTION */
  Table *pTab;           /* Table for TK_COLUMN expressions. */
#if SQLITE_MAX_EXPR_DEPTH>0
  int nHeight;           /* Height of the tree headed by this node */
#endif
};

/*
** The following are the meanings of bits in the Expr.flags field.
*/
#define EP_FromJoin   0x0001  /* Originated in ON or USING clause of a join */
#define EP_Agg        0x0002  /* Contains one or more aggregate functions */
#define EP_Resolved   0x0004  /* IDs have been resolved to COLUMNs */
#define EP_Error      0x0008  /* Expression contains one or more errors */
#define EP_Distinct   0x0010  /* Aggregate function with DISTINCT keyword */
#define EP_VarSelect  0x0020  /* pSelect is correlated, not constant */
#define EP_DblQuoted  0x0040  /* token.z was originally in "..." */
#define EP_InfixFunc  0x0080  /* True for an infix function: LIKE, GLOB, etc */
#define EP_ExpCollate 0x0100  /* Collating sequence specified explicitly */
#define EP_FixedDest  0x0200  /* Result needed in a specific register */
#define EP_IntValue   0x0400  /* Integer value contained in u.iValue */
#define EP_xIsSelect  0x0800  /* x.pSelect is valid (otherwise x.pList is) */
#define EP_Hint       0x1000  /* Not used */
#define EP_Reduced    0x2000  /* Expr struct is EXPR_REDUCEDSIZE bytes only */
#define EP_TokenOnly  0x4000  /* Expr struct is EXPR_TOKENONLYSIZE bytes only */
#define EP_Static     0x8000  /* Held in memory not obtained from malloc() */

/*
** The following are the meanings of bits in the Expr.flags2 field.
*/
#define EP2_MallocedToken  0x0001  /* Need to sqlite3DbFree() Expr.zToken */
#define EP2_Irreducible    0x0002  /* Cannot EXPRDUP_REDUCE this Expr */

/*
** The pseudo-routine sqlite3ExprSetIrreducible sets the EP2_Irreducible
** flag on an expression structure.  This flag is used for VV&A only.  The
** routine is implemented as a macro that only works when in debugging mode,
** so as not to burden production code.
*/
#ifdef SQLITE_DEBUG
# define ExprSetIrreducible(X)  (X)->flags2 |= EP2_Irreducible
#else
# define ExprSetIrreducible(X)
#endif

/*
** These macros can be used to test, set, or clear bits in the 
** Expr.flags field.
*/
#define ExprHasProperty(E,P)     (((E)->flags&(P))==(P))
#define ExprHasAnyProperty(E,P)  (((E)->flags&(P))!=0)
#define ExprSetProperty(E,P)     (E)->flags|=(P)
#define ExprClearProperty(E,P)   (E)->flags&=~(P)

/*
** Macros to determine the number of bytes required by a normal Expr 
** struct, an Expr struct with the EP_Reduced flag set in Expr.flags 
** and an Expr struct with the EP_TokenOnly flag set.
*/
#define EXPR_FULLSIZE           sizeof(Expr)           /* Full size */
#define EXPR_REDUCEDSIZE        offsetof(Expr,iTable)  /* Common features */
#define EXPR_TOKENONLYSIZE      offsetof(Expr,pLeft)   /* Fewer features */

/*
** Flags passed to the sqlite3ExprDup() function. See the header comment 
** above sqlite3ExprDup() for details.
*/
#define EXPRDUP_REDUCE         0x0001  /* Used reduced-size Expr nodes */

/*
** A list of expressions.  Each expression may optionally have a
** name.  An expr/name combination can be used in several ways, such
** as the list of "expr AS ID" fields following a "SELECT" or in the
** list of "ID = expr" items in an UPDATE.  A list of expressions can
** also be used as the argument to a function, in which case the a.zName
** field is not used.
*/
struct ExprList {
  int nExpr;             /* Number of expressions on the list */
  int iECursor;          /* VDBE Cursor associated with this ExprList */
  struct ExprList_item { /* For each expression in the list */
    Expr *pExpr;           /* The list of expressions */
    char *zName;           /* Token associated with this expression */
    char *zSpan;           /* Original text of the expression */
    u8 sortOrder;          /* 1 for DESC or 0 for ASC */
    u8 done;               /* A flag to indicate when processing is finished */
    u16 iOrderByCol;       /* For ORDER BY, column number in result set */
    u16 iAlias;            /* Index into Parse.aAlias[] for zName */
  } *a;                  /* Alloc a power of two greater or equal to nExpr */
};

/*
** An instance of this structure is used by the parser to record both
** the parse tree for an expression and the span of input text for an
** expression.
*/
struct ExprSpan {
  Expr *pExpr;          /* The expression parse tree */
  const char *zStart;   /* First character of input text */
  const char *zEnd;     /* One character past the end of input text */
};

/*
** An instance of this structure can hold a simple list of identifiers,
** such as the list "a,b,c" in the following statements:
**
**      INSERT INTO t(a,b,c) VALUES ...;
**      CREATE INDEX idx ON t(a,b,c);
**      CREATE TRIGGER trig BEFORE UPDATE ON t(a,b,c) ...;
**
** The IdList.a.idx field is used when the IdList represents the list of
** column names after a table name in an INSERT statement.  In the statement
**
**     INSERT INTO t(a,b,c) ...
**
** If "a" is the k-th column of table "t", then IdList.a[0].idx==k.
*/
struct IdList {
  struct IdList_item {
    char *zName;      /* Name of the identifier */
    int idx;          /* Index in some Table.aCol[] of a column named zName */
  } *a;
  int nId;         /* Number of identifiers on the list */
};

/*
** The bitmask datatype defined below is used for various optimizations.
**
** Changing this from a 64-bit to a 32-bit type limits the number of
** tables in a join to 32 instead of 64.  But it also reduces the size
** of the library by 738 bytes on ix86.
*/
typedef u64 Bitmask;

/*
** The number of bits in a Bitmask.  "BMS" means "BitMask Size".
*/
#define BMS  ((int)(sizeof(Bitmask)*8))

/*
** The following structure describes the FROM clause of a SELECT statement.
** Each table or subquery in the FROM clause is a separate element of
** the SrcList.a[] array.
**
** With the addition of multiple database support, the following structure
** can also be used to describe a particular table such as the table that
** is modified by an INSERT, DELETE, or UPDATE statement.  In standard SQL,
** such a table must be a simple name: ID.  But in SQLite, the table can
** now be identified by a database name, a dot, then the table name: ID.ID.
**
** The jointype starts out showing the join type between the current table
** and the next table on the list.  The parser builds the list this way.
** But sqlite3SrcListShiftJoinType() later shifts the jointypes so that each
** jointype expresses the join between the table and the previous table.
**
** In the colUsed field, the high-order bit (bit 63) is set if the table
** contains more than 63 columns and the 64-th or later column is used.
*/
struct SrcList {
  i16 nSrc;        /* Number of tables or subqueries in the FROM clause */
  i16 nAlloc;      /* Number of entries allocated in a[] below */
  struct SrcList_item {
    char *zDatabase;  /* Name of database holding this table */
    char *zName;      /* Name of the table */
    char *zAlias;     /* The "B" part of a "A AS B" phrase.  zName is the "A" */
    Table *pTab;      /* An SQL table corresponding to zName */
    Select *pSelect;  /* A SELECT statement used in place of a table name */
    int addrFillSub;  /* Address of subroutine to manifest a subquery */
    int regReturn;    /* Register holding return address of addrFillSub */
    u8 jointype;      /* Type of join between this able and the previous */
    u8 notIndexed;    /* True if there is a NOT INDEXED clause */
    u8 isCorrelated;  /* True if sub-query is correlated */
#ifndef SQLITE_OMIT_EXPLAIN
    u8 iSelectId;     /* If pSelect!=0, the id of the sub-select in EQP */
#endif
    int iCursor;      /* The VDBE cursor number used to access this table */
    Expr *pOn;        /* The ON clause of a join */
    IdList *pUsing;   /* The USING clause of a join */
    Bitmask colUsed;  /* Bit N (1<<N) set if column N of pTab is used */
    char *zIndex;     /* Identifier from "INDEXED BY <zIndex>" clause */
    Index *pIndex;    /* Index structure corresponding to zIndex, if any */
  } a[1];             /* One entry for each identifier on the list */
};

/*
** Permitted values of the SrcList.a.jointype field
*/
#define JT_INNER     0x0001    /* Any kind of inner or cross join */
#define JT_CROSS     0x0002    /* Explicit use of the CROSS keyword */
#define JT_NATURAL   0x0004    /* True for a "natural" join */
#define JT_LEFT      0x0008    /* Left outer join */
#define JT_RIGHT     0x0010    /* Right outer join */
#define JT_OUTER     0x0020    /* The "OUTER" keyword is present */
#define JT_ERROR     0x0040    /* unknown or unsupported join type */


/*
** A WherePlan object holds information that describes a lookup
** strategy.
**
** This object is intended to be opaque outside of the where.c module.
** It is included here only so that that compiler will know how big it
** is.  None of the fields in this object should be used outside of
** the where.c module.
**
** Within the union, pIdx is only used when wsFlags&WHERE_INDEXED is true.
** pTerm is only used when wsFlags&WHERE_MULTI_OR is true.  And pVtabIdx
** is only used when wsFlags&WHERE_VIRTUALTABLE is true.  It is never the
** case that more than one of these conditions is true.
*/
struct WherePlan {
  u32 wsFlags;                   /* WHERE_* flags that describe the strategy */
  u32 nEq;                       /* Number of == constraints */
  double nRow;                   /* Estimated number of rows (for EQP) */
  union {
    Index *pIdx;                   /* Index when WHERE_INDEXED is true */
    struct WhereTerm *pTerm;       /* WHERE clause term for OR-search */
    sqlite3_index_info *pVtabIdx;  /* Virtual table index to use */
  } u;
};

/*
** For each nested loop in a WHERE clause implementation, the WhereInfo
** structure contains a single instance of this structure.  This structure
** is intended to be private to the where.c module and should not be
** access or modified by other modules.
**
** The pIdxInfo field is used to help pick the best index on a
** virtual table.  The pIdxInfo pointer contains indexing
** information for the i-th table in the FROM clause before reordering.
** All the pIdxInfo pointers are freed by whereInfoFree() in where.c.
** All other information in the i-th WhereLevel object for the i-th table
** after FROM clause ordering.
*/
struct WhereLevel {
  WherePlan plan;       /* query plan for this element of the FROM clause */
  int iLeftJoin;        /* Memory cell used to implement LEFT OUTER JOIN */
  int iTabCur;          /* The VDBE cursor used to access the table */
  int iIdxCur;          /* The VDBE cursor used to access pIdx */
  int addrBrk;          /* Jump here to break out of the loop */
  int addrNxt;          /* Jump here to start the next IN combination */
  int addrCont;         /* Jump here to continue with the next loop cycle */
  int addrFirst;        /* First instruction of interior of the loop */
  u8 iFrom;             /* Which entry in the FROM clause */
  u8 op, p5;            /* Opcode and P5 of the opcode that ends the loop */
  int p1, p2;           /* Operands of the opcode used to ends the loop */
  union {               /* Information that depends on plan.wsFlags */
    struct {
      int nIn;              /* Number of entries in aInLoop[] */
      struct InLoop {
        int iCur;              /* The VDBE cursor used by this IN operator */
        int addrInTop;         /* Top of the IN loop */
      } *aInLoop;           /* Information about each nested IN operator */
    } in;                 /* Used when plan.wsFlags&WHERE_IN_ABLE */
    Index *pCovidx;       /* Possible covering index for WHERE_MULTI_OR */
  } u;

  /* The following field is really not part of the current level.  But
  ** we need a place to cache virtual table index information for each
  ** virtual table in the FROM clause and the WhereLevel structure is
  ** a convenient place since there is one WhereLevel for each FROM clause
  ** element.
  */
  sqlite3_index_info *pIdxInfo;  /* Index info for n-th source table */
};

/*
** Flags appropriate for the wctrlFlags parameter of sqlite3WhereBegin()
** and the WhereInfo.wctrlFlags member.
*/
#define WHERE_ORDERBY_NORMAL   0x0000 /* No-op */
#define WHERE_ORDERBY_MIN      0x0001 /* ORDER BY processing for min() func */
#define WHERE_ORDERBY_MAX      0x0002 /* ORDER BY processing for max() func */
#define WHERE_ONEPASS_DESIRED  0x0004 /* Want to do one-pass UPDATE/DELETE */
#define WHERE_DUPLICATES_OK    0x0008 /* Ok to return a row more than once */
#define WHERE_OMIT_OPEN_CLOSE  0x0010 /* Table cursors are already open */
#define WHERE_FORCE_TABLE      0x0020 /* Do not use an index-only search */
#define WHERE_ONETABLE_ONLY    0x0040 /* Only code the 1st table in pTabList */
#define WHERE_AND_ONLY         0x0080 /* Don't use indices for OR terms */

/*
** The WHERE clause processing routine has two halves.  The
** first part does the start of the WHERE loop and the second
** half does the tail of the WHERE loop.  An instance of
** this structure is returned by the first half and passed
** into the second half to give some continuity.
*/
struct WhereInfo {
  Parse *pParse;       /* Parsing and code generating context */
  u16 wctrlFlags;      /* Flags originally passed to sqlite3WhereBegin() */
  u8 okOnePass;        /* Ok to use one-pass algorithm for UPDATE or DELETE */
  u8 untestedTerms;    /* Not all WHERE terms resolved by outer loop */
  u8 eDistinct;
  SrcList *pTabList;             /* List of tables in the join */
  int iTop;                      /* The very beginning of the WHERE loop */
  int iContinue;                 /* Jump here to continue with next record */
  int iBreak;                    /* Jump here to break out of the loop */
  int nLevel;                    /* Number of nested loop */
  struct WhereClause *pWC;       /* Decomposition of the WHERE clause */
  double savedNQueryLoop;        /* pParse->nQueryLoop outside the WHERE loop */
  double nRowOut;                /* Estimated number of output rows */
  WhereLevel a[1];               /* Information about each nest loop in WHERE */
};

#define WHERE_DISTINCT_UNIQUE 1
#define WHERE_DISTINCT_ORDERED 2

/*
** A NameContext defines a context in which to resolve table and column
** names.  The context consists of a list of tables (the pSrcList) field and
** a list of named expression (pEList).  The named expression list may
** be NULL.  The pSrc corresponds to the FROM clause of a SELECT or
** to the table being operated on by INSERT, UPDATE, or DELETE.  The
** pEList corresponds to the result set of a SELECT and is NULL for
** other statements.
**
** NameContexts can be nested.  When resolving names, the inner-most 
** context is searched first.  If no match is found, the next outer
** context is checked.  If there is still no match, the next context
** is checked.  This process continues until either a match is found
** or all contexts are check.  When a match is found, the nRef member of
** the context containing the match is incremented. 
**
** Each subquery gets a new NameContext.  The pNext field points to the
** NameContext in the parent query.  Thus the process of scanning the
** NameContext list corresponds to searching through successively outer
** subqueries looking for a match.
*/
struct NameContext {
  Parse *pParse;       /* The parser */
  SrcList *pSrcList;   /* One or more tables used to resolve names */
  ExprList *pEList;    /* Optional list of named expressions */
  AggInfo *pAggInfo;   /* Information about aggregates at this level */
  NameContext *pNext;  /* Next outer name context.  NULL for outermost */
  int nRef;            /* Number of names resolved by this context */
  int nErr;            /* Number of errors encountered while resolving names */
  u8 ncFlags;          /* Zero or more NC_* flags defined below */
};

/*
** Allowed values for the NameContext, ncFlags field.
*/
#define NC_AllowAgg  0x01    /* Aggregate functions are allowed here */
#define NC_HasAgg    0x02    /* One or more aggregate functions seen */
#define NC_IsCheck   0x04    /* True if resolving names in a CHECK constraint */
#define NC_InAggFunc 0x08    /* True if analyzing arguments to an agg func */

/*
** An instance of the following structure contains all information
** needed to generate code for a single SELECT statement.
**
** nLimit is set to -1 if there is no LIMIT clause.  nOffset is set to 0.
** If there is a LIMIT clause, the parser sets nLimit to the value of the
** limit and nOffset to the value of the offset (or 0 if there is not
** offset).  But later on, nLimit and nOffset become the memory locations
** in the VDBE that record the limit and offset counters.
**
** addrOpenEphm[] entries contain the address of OP_OpenEphemeral opcodes.
** These addresses must be stored so that we can go back and fill in
** the P4_KEYINFO and P2 parameters later.  Neither the KeyInfo nor
** the number of columns in P2 can be computed at the same time
** as the OP_OpenEphm instruction is coded because not
** enough information about the compound query is known at that point.
** The KeyInfo for addrOpenTran[0] and [1] contains collating sequences
** for the result set.  The KeyInfo for addrOpenTran[2] contains collating
** sequences for the ORDER BY clause.
*/
struct Select {
  ExprList *pEList;      /* The fields of the result */
  u8 op;                 /* One of: TK_UNION TK_ALL TK_INTERSECT TK_EXCEPT */
  char affinity;         /* MakeRecord with this affinity for SRT_Set */
  u16 selFlags;          /* Various SF_* values */
  int iLimit, iOffset;   /* Memory registers holding LIMIT & OFFSET counters */
  int addrOpenEphm[3];   /* OP_OpenEphem opcodes related to this select */
  double nSelectRow;     /* Estimated number of result rows */
  SrcList *pSrc;         /* The FROM clause */
  Expr *pWhere;          /* The WHERE clause */
  ExprList *pGroupBy;    /* The GROUP BY clause */
  Expr *pHaving;         /* The HAVING clause */
  ExprList *pOrderBy;    /* The ORDER BY clause */
  Select *pPrior;        /* Prior select in a compound select statement */
  Select *pNext;         /* Next select to the left in a compound */
  Select *pRightmost;    /* Right-most select in a compound select statement */
  Expr *pLimit;          /* LIMIT expression. NULL means not used. */
  Expr *pOffset;         /* OFFSET expression. NULL means not used. */
};

/*
** Allowed values for Select.selFlags.  The "SF" prefix stands for
** "Select Flag".
*/
#define SF_Distinct        0x01  /* Output should be DISTINCT */
#define SF_Resolved        0x02  /* Identifiers have been resolved */
#define SF_Aggregate       0x04  /* Contains aggregate functions */
#define SF_UsesEphemeral   0x08  /* Uses the OpenEphemeral opcode */
#define SF_Expanded        0x10  /* sqlite3SelectExpand() called on this */
#define SF_HasTypeInfo     0x20  /* FROM subqueries have Table metadata */
#define SF_UseSorter       0x40  /* Sort using a sorter */
#define SF_Values          0x80  /* Synthesized from VALUES clause */


/*
** The results of a select can be distributed in several ways.  The
** "SRT" prefix means "SELECT Result Type".
*/
#define SRT_Union        1  /* Store result as keys in an index */
#define SRT_Except       2  /* Remove result from a UNION index */
#define SRT_Exists       3  /* Store 1 if the result is not empty */
#define SRT_Discard      4  /* Do not save the results anywhere */

/* The ORDER BY clause is ignored for all of the above */
#define IgnorableOrderby(X) ((X->eDest)<=SRT_Discard)

#define SRT_Output       5  /* Output each row of result */
#define SRT_Mem          6  /* Store result in a memory cell */
#define SRT_Set          7  /* Store results as keys in an index */
#define SRT_Table        8  /* Store result as data with an automatic rowid */
#define SRT_EphemTab     9  /* Create transient tab and store like SRT_Table */
#define SRT_Coroutine   10  /* Generate a single row of result */

/*
** A structure used to customize the behavior of sqlite3Select(). See
** comments above sqlite3Select() for details.
*/
typedef struct SelectDest SelectDest;
struct SelectDest {
  u8 eDest;         /* How to dispose of the results */
  u8 affSdst;       /* Affinity used when eDest==SRT_Set */
  int iSDParm;      /* A parameter used by the eDest disposal method */
  int iSdst;        /* Base register where results are written */
  int nSdst;        /* Number of registers allocated */
};

/*
** During code generation of statements that do inserts into AUTOINCREMENT 
** tables, the following information is attached to the Table.u.autoInc.p
** pointer of each autoincrement table to record some side information that
** the code generator needs.  We have to keep per-table autoincrement
** information in case inserts are down within triggers.  Triggers do not
** normally coordinate their activities, but we do need to coordinate the
** loading and saving of autoincrement information.
*/
struct AutoincInfo {
  AutoincInfo *pNext;   /* Next info block in a list of them all */
  Table *pTab;          /* Table this info block refers to */
  int iDb;              /* Index in sqlite3.aDb[] of database holding pTab */
  int regCtr;           /* Memory register holding the rowid counter */
};

/*
** Size of the column cache
*/
#ifndef SQLITE_N_COLCACHE
# define SQLITE_N_COLCACHE 10
#endif

/*
** At least one instance of the following structure is created for each 
** trigger that may be fired while parsing an INSERT, UPDATE or DELETE
** statement. All such objects are stored in the linked list headed at
** Parse.pTriggerPrg and deleted once statement compilation has been
** completed.
**
** A Vdbe sub-program that implements the body and WHEN clause of trigger
** TriggerPrg.pTrigger, assuming a default ON CONFLICT clause of
** TriggerPrg.orconf, is stored in the TriggerPrg.pProgram variable.
** The Parse.pTriggerPrg list never contains two entries with the same
** values for both pTrigger and orconf.
**
** The TriggerPrg.aColmask[0] variable is set to a mask of old.* columns
** accessed (or set to 0 for triggers fired as a result of INSERT 
** statements). Similarly, the TriggerPrg.aColmask[1] variable is set to
** a mask of new.* columns used by the program.
*/
struct TriggerPrg {
  Trigger *pTrigger;      /* Trigger this program was coded from */
  TriggerPrg *pNext;      /* Next entry in Parse.pTriggerPrg list */
  SubProgram *pProgram;   /* Program implementing pTrigger/orconf */
  int orconf;             /* Default ON CONFLICT policy */
  u32 aColmask[2];        /* Masks of old.*, new.* columns accessed */
};

/*
** The yDbMask datatype for the bitmask of all attached databases.
*/
#if SQLITE_MAX_ATTACHED>30
  typedef sqlite3_uint64 yDbMask;
#else
  typedef unsigned int yDbMask;
#endif

/*
** An SQL parser context.  A copy of this structure is passed through
** the parser and down into all the parser action routine in order to
** carry around information that is global to the entire parse.
**
** The structure is divided into two parts.  When the parser and code
** generate call themselves recursively, the first part of the structure
** is constant but the second part is reset at the beginning and end of
** each recursion.
**
** The nTableLock and aTableLock variables are only used if the shared-cache 
** feature is enabled (if sqlite3Tsd()->useSharedData is true). They are
** used to store the set of table-locks required by the statement being
** compiled. Function sqlite3TableLock() is used to add entries to the
** list.
*/
struct Parse {
  sqlite3 *db;         /* The main database structure */
  char *zErrMsg;       /* An error message */
  Vdbe *pVdbe;         /* An engine for executing database bytecode */
  int rc;              /* Return code from execution */
  u8 colNamesSet;      /* TRUE after OP_ColumnName has been issued to pVdbe */
  u8 checkSchema;      /* Causes schema cookie check after an error */
  u8 nested;           /* Number of nested calls to the parser/code generator */
  u8 nTempReg;         /* Number of temporary registers in aTempReg[] */
  u8 nTempInUse;       /* Number of aTempReg[] currently checked out */
  u8 nColCache;        /* Number of entries in aColCache[] */
  u8 iColCache;        /* Next entry in aColCache[] to replace */
  u8 isMultiWrite;     /* True if statement may modify/insert multiple rows */
  u8 mayAbort;         /* True if statement may throw an ABORT exception */
  int aTempReg[8];     /* Holding area for temporary registers */
  int nRangeReg;       /* Size of the temporary register block */
  int iRangeReg;       /* First register in temporary register block */
  int nErr;            /* Number of errors seen */
  int nTab;            /* Number of previously allocated VDBE cursors */
  int nMem;            /* Number of memory cells used so far */
  int nSet;            /* Number of sets used so far */
  int nOnce;           /* Number of OP_Once instructions so far */
  int ckBase;          /* Base register of data during check constraints */
  int iCacheLevel;     /* ColCache valid when aColCache[].iLevel<=iCacheLevel */
  int iCacheCnt;       /* Counter used to generate aColCache[].lru values */
  struct yColCache {
    int iTable;           /* Table cursor number */
    int iColumn;          /* Table column number */
    u8 tempReg;           /* iReg is a temp register that needs to be freed */
    int iLevel;           /* Nesting level */
    int iReg;             /* Reg with value of this column. 0 means none. */
    int lru;              /* Least recently used entry has the smallest value */
  } aColCache[SQLITE_N_COLCACHE];  /* One for each column cache entry */
  yDbMask writeMask;   /* Start a write transaction on these databases */
  yDbMask cookieMask;  /* Bitmask of schema verified databases */
  int cookieGoto;      /* Address of OP_Goto to cookie verifier subroutine */
  int cookieValue[SQLITE_MAX_ATTACHED+2];  /* Values of cookies to verify */
  int regRowid;        /* Register holding rowid of CREATE TABLE entry */
  int regRoot;         /* Register holding root page number for new objects */
  int nMaxArg;         /* Max args passed to user function by sub-program */
  Token constraintName;/* Name of the constraint currently being parsed */
#ifndef SQLITE_OMIT_SHARED_CACHE
  int nTableLock;        /* Number of locks in aTableLock */
  TableLock *aTableLock; /* Required table locks for shared-cache mode */
#endif
  AutoincInfo *pAinc;  /* Information about AUTOINCREMENT counters */

  /* Information used while coding trigger programs. */
  Parse *pToplevel;    /* Parse structure for main program (or NULL) */
  Table *pTriggerTab;  /* Table triggers are being coded for */
  double nQueryLoop;   /* Estimated number of iterations of a query */
  u32 oldmask;         /* Mask of old.* columns referenced */
  u32 newmask;         /* Mask of new.* columns referenced */
  u8 eTriggerOp;       /* TK_UPDATE, TK_INSERT or TK_DELETE */
  u8 eOrconf;          /* Default ON CONFLICT policy for trigger steps */
  u8 disableTriggers;  /* True to disable triggers */

  /* Above is constant between recursions.  Below is reset before and after
  ** each recursion */

  int nVar;                 /* Number of '?' variables seen in the SQL so far */
  int nzVar;                /* Number of available slots in azVar[] */
  u8 explain;               /* True if the EXPLAIN flag is found on the query */
#ifndef SQLITE_OMIT_VIRTUALTABLE
  u8 declareVtab;           /* True if inside sqlite3_declare_vtab() */
  int nVtabLock;            /* Number of virtual tables to lock */
#endif
  int nAlias;               /* Number of aliased result set columns */
  int nHeight;              /* Expression tree height of current sub-select */
#ifndef SQLITE_OMIT_EXPLAIN
  int iSelectId;            /* ID of current select for EXPLAIN output */
  int iNextSelectId;        /* Next available select ID for EXPLAIN output */
#endif
  char **azVar;             /* Pointers to names of parameters */
  Vdbe *pReprepare;         /* VM being reprepared (sqlite3Reprepare()) */
  int *aAlias;              /* Register used to hold aliased result */
  const char *zTail;        /* All SQL text past the last semicolon parsed */
  Table *pNewTable;         /* A table being constructed by CREATE TABLE */
  Trigger *pNewTrigger;     /* Trigger under construct by a CREATE TRIGGER */
  const char *zAuthContext; /* The 6th parameter to db->xAuth callbacks */
  Token sNameToken;         /* Token with unqualified schema object name */
  Token sLastToken;         /* The last token parsed */
#ifndef SQLITE_OMIT_VIRTUALTABLE
  Token sArg;               /* Complete text of a module argument */
  Table **apVtabLock;       /* Pointer to virtual tables needing locking */
#endif
  Table *pZombieTab;        /* List of Table objects to delete after code gen */
  TriggerPrg *pTriggerPrg;  /* Linked list of coded triggers */
};

/*
** Return true if currently inside an sqlite3_declare_vtab() call.
*/
#ifdef SQLITE_OMIT_VIRTUALTABLE
  #define IN_DECLARE_VTAB 0
#else
  #define IN_DECLARE_VTAB (pParse->declareVtab)
#endif

/*
** An instance of the following structure can be declared on a stack and used
** to save the Parse.zAuthContext value so that it can be restored later.
*/
struct AuthContext {
  const char *zAuthContext;   /* Put saved Parse.zAuthContext here */
  Parse *pParse;              /* The Parse structure */
};

/*
** Bitfield flags for P5 value in various opcodes.
*/
#define OPFLAG_NCHANGE       0x01    /* Set to update db->nChange */
#define OPFLAG_LASTROWID     0x02    /* Set to update db->lastRowid */
#define OPFLAG_ISUPDATE      0x04    /* This OP_Insert is an sql UPDATE */
#define OPFLAG_APPEND        0x08    /* This is likely to be an append */
#define OPFLAG_USESEEKRESULT 0x10    /* Try to avoid a seek in BtreeInsert() */
#define OPFLAG_CLEARCACHE    0x20    /* Clear pseudo-table cache in OP_Column */
#define OPFLAG_LENGTHARG     0x40    /* OP_Column only used for length() */
#define OPFLAG_TYPEOFARG     0x80    /* OP_Column only used for typeof() */
#define OPFLAG_BULKCSR       0x01    /* OP_Open** used to open bulk cursor */
#define OPFLAG_P2ISREG       0x02    /* P2 to OP_Open** is a register number */

/*
 * Each trigger present in the database schema is stored as an instance of
 * struct Trigger. 
 *
 * Pointers to instances of struct Trigger are stored in two ways.
 * 1. In the "trigHash" hash table (part of the sqlite3* that represents the 
 *    database). This allows Trigger structures to be retrieved by name.
 * 2. All triggers associated with a single table form a linked list, using the
 *    pNext member of struct Trigger. A pointer to the first element of the
 *    linked list is stored as the "pTrigger" member of the associated
 *    struct Table.
 *
 * The "step_list" member points to the first element of a linked list
 * containing the SQL statements specified as the trigger program.
 */
struct Trigger {
  char *zName;            /* The name of the trigger                        */
  char *table;            /* The table or view to which the trigger applies */
  u8 op;                  /* One of TK_DELETE, TK_UPDATE, TK_INSERT         */
  u8 tr_tm;               /* One of TRIGGER_BEFORE, TRIGGER_AFTER */
  Expr *pWhen;            /* The WHEN clause of the expression (may be NULL) */
  IdList *pColumns;       /* If this is an UPDATE OF <column-list> trigger,
                             the <column-list> is stored here */
  Schema *pSchema;        /* Schema containing the trigger */
  Schema *pTabSchema;     /* Schema containing the table */
  TriggerStep *step_list; /* Link list of trigger program steps             */
  Trigger *pNext;         /* Next trigger associated with the table */
};

/*
** A trigger is either a BEFORE or an AFTER trigger.  The following constants
** determine which. 
**
** If there are multiple triggers, you might of some BEFORE and some AFTER.
** In that cases, the constants below can be ORed together.
*/
#define TRIGGER_BEFORE  1
#define TRIGGER_AFTER   2

/*
 * An instance of struct TriggerStep is used to store a single SQL statement
 * that is a part of a trigger-program. 
 *
 * Instances of struct TriggerStep are stored in a singly linked list (linked
 * using the "pNext" member) referenced by the "step_list" member of the 
 * associated struct Trigger instance. The first element of the linked list is
 * the first step of the trigger-program.
 * 
 * The "op" member indicates whether this is a "DELETE", "INSERT", "UPDATE" or
 * "SELECT" statement. The meanings of the other members is determined by the 
 * value of "op" as follows:
 *
 * (op == TK_INSERT)
 * orconf    -> stores the ON CONFLICT algorithm
 * pSelect   -> If this is an INSERT INTO ... SELECT ... statement, then
 *              this stores a pointer to the SELECT statement. Otherwise NULL.
 * target    -> A token holding the quoted name of the table to insert into.
 * pExprList -> If this is an INSERT INTO ... VALUES ... statement, then
 *              this stores values to be inserted. Otherwise NULL.
 * pIdList   -> If this is an INSERT INTO ... (<column-names>) VALUES ... 
 *              statement, then this stores the column-names to be
 *              inserted into.
 *
 * (op == TK_DELETE)
 * target    -> A token holding the quoted name of the table to delete from.
 * pWhere    -> The WHERE clause of the DELETE statement if one is specified.
 *              Otherwise NULL.
 * 
 * (op == TK_UPDATE)
 * target    -> A token holding the quoted name of the table to update rows of.
 * pWhere    -> The WHERE clause of the UPDATE statement if one is specified.
 *              Otherwise NULL.
 * pExprList -> A list of the columns to update and the expressions to update
 *              them to. See sqlite3Update() documentation of "pChanges"
 *              argument.
 * 
 */
struct TriggerStep {
  u8 op;               /* One of TK_DELETE, TK_UPDATE, TK_INSERT, TK_SELECT */
  u8 orconf;           /* OE_Rollback etc. */
  Trigger *pTrig;      /* The trigger that this step is a part of */
  Select *pSelect;     /* SELECT statment or RHS of INSERT INTO .. SELECT ... */
  Token target;        /* Target table for DELETE, UPDATE, INSERT */
  Expr *pWhere;        /* The WHERE clause for DELETE or UPDATE steps */
  ExprList *pExprList; /* SET clause for UPDATE.  VALUES clause for INSERT */
  IdList *pIdList;     /* Column names for INSERT */
  TriggerStep *pNext;  /* Next in the link-list */
  TriggerStep *pLast;  /* Last element in link-list. Valid for 1st elem only */
};

/*
** The following structure contains information used by the sqliteFix...
** routines as they walk the parse tree to make database references
** explicit.  
*/
typedef struct DbFixer DbFixer;
struct DbFixer {
  Parse *pParse;      /* The parsing context.  Error messages written here */
  const char *zDb;    /* Make sure all objects are contained in this database */
  const char *zType;  /* Type of the container - used for error messages */
  const Token *pName; /* Name of the container - used for error messages */
};

/*
** An objected used to accumulate the text of a string where we
** do not necessarily know how big the string will be in the end.
*/
struct StrAccum {
  sqlite3 *db;         /* Optional database for lookaside.  Can be NULL */
  char *zBase;         /* A base allocation.  Not from malloc. */
  char *zText;         /* The string collected so far */
  int  nChar;          /* Length of the string so far */
  int  nAlloc;         /* Amount of space allocated in zText */
  int  mxAlloc;        /* Maximum allowed string length */
  u8   mallocFailed;   /* Becomes true if any memory allocation fails */
  u8   useMalloc;      /* 0: none,  1: sqlite3DbMalloc,  2: sqlite3_malloc */
  u8   tooBig;         /* Becomes true if string size exceeds limits */
};

/*
** A pointer to this structure is used to communicate information
** from sqlite3Init and OP_ParseSchema into the sqlite3InitCallback.
*/
typedef struct {
  sqlite3 *db;        /* The database being initialized */
  char **pzErrMsg;    /* Error message stored here */
  int iDb;            /* 0 for main database.  1 for TEMP, 2.. for ATTACHed */
  int rc;             /* Result code stored here */
} InitData;

/*
** Structure containing global configuration data for the SQLite library.
**
** This structure also contains some state information.
*/
struct Sqlite3Config {
  int bMemstat;                     /* True to enable memory status */
  int bCoreMutex;                   /* True to enable core mutexing */
  int bFullMutex;                   /* True to enable full mutexing */
  int bOpenUri;                     /* True to interpret filenames as URIs */
  int mxStrlen;                     /* Maximum string length */
  int szLookaside;                  /* Default lookaside buffer size */
  int nLookaside;                   /* Default lookaside buffer count */
  sqlite3_mem_methods m;            /* Low-level memory allocation interface */
  sqlite3_mutex_methods mutex;      /* Low-level mutex interface */
  sqlite3_pcache_methods2 pcache2;  /* Low-level page-cache interface */
  void *pHeap;                      /* Heap storage space */
  int nHeap;                        /* Size of pHeap[] */
  int mnReq, mxReq;                 /* Min and max heap requests sizes */
  void *pScratch;                   /* Scratch memory */
  int szScratch;                    /* Size of each scratch buffer */
  int nScratch;                     /* Number of scratch buffers */
  void *pPage;                      /* Page cache memory */
  int szPage;                       /* Size of each page in pPage[] */
  int nPage;                        /* Number of pages in pPage[] */
  int mxParserStack;                /* maximum depth of the parser stack */
  int sharedCacheEnabled;           /* true if shared-cache mode enabled */
  /* The above might be initialized to non-zero.  The following need to always
  ** initially be zero, however. */
  int isInit;                       /* True after initialization has finished */
  int inProgress;                   /* True while initialization in progress */
  int isMutexInit;                  /* True after mutexes are initialized */
  int isMallocInit;                 /* True after malloc is initialized */
  int isPCacheInit;                 /* True after malloc is initialized */
  sqlite3_mutex *pInitMutex;        /* Mutex used by sqlite3_initialize() */
  int nRefInitMutex;                /* Number of users of pInitMutex */
  void (*xLog)(void*,int,const char*); /* Function for logging */
  void *pLogArg;                       /* First argument to xLog() */
  int bLocaltimeFault;              /* True to fail localtime() calls */
};

/*
** Context pointer passed down through the tree-walk.
*/
struct Walker {
  int (*xExprCallback)(Walker*, Expr*);     /* Callback for expressions */
  int (*xSelectCallback)(Walker*,Select*);  /* Callback for SELECTs */
  Parse *pParse;                            /* Parser context.  */
  int walkerDepth;                          /* Number of subqueries */
  union {                                   /* Extra data for callback */
    NameContext *pNC;                          /* Naming context */
    int i;                                     /* Integer value */
    SrcList *pSrcList;                         /* FROM clause */
    struct SrcCount *pSrcCount;                /* Counting column references */
  } u;
};

/* Forward declarations */
int sqlite3WalkExpr(Walker*, Expr*);
int sqlite3WalkExprList(Walker*, ExprList*);
int sqlite3WalkSelect(Walker*, Select*);
int sqlite3WalkSelectExpr(Walker*, Select*);
int sqlite3WalkSelectFrom(Walker*, Select*);

/*
** Return code from the parse-tree walking primitives and their
** callbacks.
*/
#define WRC_Continue    0   /* Continue down into children */
#define WRC_Prune       1   /* Omit children but continue walking siblings */
#define WRC_Abort       2   /* Abandon the tree walk */

/*
** Assuming zIn points to the first byte of a UTF-8 character,
** advance zIn to point to the first byte of the next UTF-8 character.
*/
#define SQLITE_SKIP_UTF8(zIn) {                        \
  if( (*(zIn++))>=0xc0 ){                              \
    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \
  }                                                    \
}

/*
** The SQLITE_*_BKPT macros are substitutes for the error codes with
** the same name but without the _BKPT suffix.  These macros invoke
** routines that report the line-number on which the error originated
** using sqlite3_log().  The routines also provide a convenient place
** to set a debugger breakpoint.
** 这些命名为SQLITE_*_BKPT的宏定义用来代替与它们名字相同的错误代码，不包括_BKPT后缀。
** 这些宏调用sqlite3_log()函数提示代码在哪儿一行发生了错误。这些用于调试程序的例程
** 还提供了一个方便的地方设置断点调试器。
*/
int sqlite3CorruptError(int);
int sqlite3MisuseError(int);
int sqlite3CantopenError(int);
#define SQLITE_CORRUPT_BKPT sqlite3CorruptError(__LINE__)
#define SQLITE_MISUSE_BKPT sqlite3MisuseError(__LINE__)
#define SQLITE_CANTOPEN_BKPT sqlite3CantopenError(__LINE__)


/*
** FTS4 is really an extension for FTS3.  It is enabled using the
** SQLITE_ENABLE_FTS3 macro.  But to avoid confusion we also all
** the SQLITE_ENABLE_FTS4 macro to serve as an alisse for SQLITE_ENABLE_FTS3.
*/
#if defined(SQLITE_ENABLE_FTS4) && !defined(SQLITE_ENABLE_FTS3)
# define SQLITE_ENABLE_FTS3
#endif

/*
** The ctype.h header is needed for non-ASCII systems.  It is also
** needed by FTS3 when FTS3 is included in the amalgamation.
*/
#if !defined(SQLITE_ASCII) || \
    (defined(SQLITE_ENABLE_FTS3) && defined(SQLITE_AMALGAMATION))
# include <ctype.h>
#endif

/*
** The following macros mimic the standard library functions toupper(),
** isspace(), isalnum(), isdigit() and isxdigit(), respectively. The
** sqlite versions only work for ASCII characters, regardless of locale.
*/
#ifdef SQLITE_ASCII
# define sqlite3Toupper(x)  ((x)&~(sqlite3CtypeMap[(unsigned char)(x)]&0x20))
# define sqlite3Isspace(x)   (sqlite3CtypeMap[(unsigned char)(x)]&0x01)
# define sqlite3Isalnum(x)   (sqlite3CtypeMap[(unsigned char)(x)]&0x06)
# define sqlite3Isalpha(x)   (sqlite3CtypeMap[(unsigned char)(x)]&0x02)
# define sqlite3Isdigit(x)   (sqlite3CtypeMap[(unsigned char)(x)]&0x04)
# define sqlite3Isxdigit(x)  (sqlite3CtypeMap[(unsigned char)(x)]&0x08)
# define sqlite3Tolower(x)   (sqlite3UpperToLower[(unsigned char)(x)])
#else
# define sqlite3Toupper(x)   toupper((unsigned char)(x))
# define sqlite3Isspace(x)   isspace((unsigned char)(x))
# define sqlite3Isalnum(x)   isalnum((unsigned char)(x))
# define sqlite3Isalpha(x)   isalpha((unsigned char)(x))
# define sqlite3Isdigit(x)   isdigit((unsigned char)(x))
# define sqlite3Isxdigit(x)  isxdigit((unsigned char)(x))
# define sqlite3Tolower(x)   tolower((unsigned char)(x))
#endif

/*
** Internal function prototypes
*/
#define sqlite3StrICmp sqlite3_stricmp
int sqlite3Strlen30(const char*);
#define sqlite3StrNICmp sqlite3_strnicmp

int sqlite3MallocInit(void);
void sqlite3MallocEnd(void);
void *sqlite3Malloc(int);
void *sqlite3MallocZero(int);
void *sqlite3DbMallocZero(sqlite3*, int);
void *sqlite3DbMallocRaw(sqlite3*, int);
char *sqlite3DbStrDup(sqlite3*,const char*);
char *sqlite3DbStrNDup(sqlite3*,const char*, int);
void *sqlite3Realloc(void*, int);
void *sqlite3DbReallocOrFree(sqlite3 *, void *, int);
void *sqlite3DbRealloc(sqlite3 *, void *, int);
void sqlite3DbFree(sqlite3*, void*);
int sqlite3MallocSize(void*);
int sqlite3DbMallocSize(sqlite3*, void*);
void *sqlite3ScratchMalloc(int);
void sqlite3ScratchFree(void*);
void *sqlite3PageMalloc(int);
void sqlite3PageFree(void*);
void sqlite3MemSetDefault(void);
void sqlite3BenignMallocHooks(void (*)(void), void (*)(void));
int sqlite3HeapNearlyFull(void);

/*
** On systems with ample stack space and that support alloca(), make
** use of alloca() to obtain space for large automatic objects.  By default,
** obtain space from malloc().
**
** The alloca() routine never returns NULL.  This will cause code paths
** that deal with sqlite3StackAlloc() failures to be unreachable.
*/
#ifdef SQLITE_USE_ALLOCA
# define sqlite3StackAllocRaw(D,N)   alloca(N)
# define sqlite3StackAllocZero(D,N)  memset(alloca(N), 0, N)
# define sqlite3StackFree(D,P)       
#else
# define sqlite3StackAllocRaw(D,N)   sqlite3DbMallocRaw(D,N)
# define sqlite3StackAllocZero(D,N)  sqlite3DbMallocZero(D,N)
# define sqlite3StackFree(D,P)       sqlite3DbFree(D,P)
#endif

#ifdef SQLITE_ENABLE_MEMSYS3
const sqlite3_mem_methods *sqlite3MemGetMemsys3(void);
#endif
#ifdef SQLITE_ENABLE_MEMSYS5
const sqlite3_mem_methods *sqlite3MemGetMemsys5(void);
#endif


#ifndef SQLITE_MUTEX_OMIT
  sqlite3_mutex_methods const *sqlite3DefaultMutex(void);
  sqlite3_mutex_methods const *sqlite3NoopMutex(void);
  sqlite3_mutex *sqlite3MutexAlloc(int);
  int sqlite3MutexInit(void);
  int sqlite3MutexEnd(void);
#endif

int sqlite3StatusValue(int);
void sqlite3StatusAdd(int, int);
void sqlite3StatusSet(int, int);

#ifndef SQLITE_OMIT_FLOATING_POINT
  int sqlite3IsNaN(double);
#else
# define sqlite3IsNaN(X)  0
#endif

void sqlite3VXPrintf(StrAccum*, int, const char*, va_list);
#ifndef SQLITE_OMIT_TRACE
void sqlite3XPrintf(StrAccum*, const char*, ...);
#endif
char *sqlite3MPrintf(sqlite3*,const char*, ...);
char *sqlite3VMPrintf(sqlite3*,const char*, va_list);
char *sqlite3MAppendf(sqlite3*,char*,const char*,...);
#if defined(SQLITE_TEST) || defined(SQLITE_DEBUG)
  void sqlite3DebugPrintf(const char*, ...);
#endif
#if defined(SQLITE_TEST)
  void *sqlite3TestTextToPtr(const char*);
#endif

/* Output formatting for SQLITE_TESTCTRL_EXPLAIN */
#if defined(SQLITE_ENABLE_TREE_EXPLAIN)
  void sqlite3ExplainBegin(Vdbe*);
  void sqlite3ExplainPrintf(Vdbe*, const char*, ...);
  void sqlite3ExplainNL(Vdbe*);
  void sqlite3ExplainPush(Vdbe*);
  void sqlite3ExplainPop(Vdbe*);
  void sqlite3ExplainFinish(Vdbe*);
  void sqlite3ExplainSelect(Vdbe*, Select*);
  void sqlite3ExplainExpr(Vdbe*, Expr*);
  void sqlite3ExplainExprList(Vdbe*, ExprList*);
  const char *sqlite3VdbeExplanation(Vdbe*);
#else
# define sqlite3ExplainBegin(X)
# define sqlite3ExplainSelect(A,B)
# define sqlite3ExplainExpr(A,B)
# define sqlite3ExplainExprList(A,B)
# define sqlite3ExplainFinish(X)
# define sqlite3VdbeExplanation(X) 0
#endif


void sqlite3SetString(char **, sqlite3*, const char*, ...);
void sqlite3ErrorMsg(Parse*, const char*, ...);
int sqlite3Dequote(char*);
int sqlite3KeywordCode(const unsigned char*, int);
int sqlite3RunParser(Parse*, const char*, char **);
void sqlite3FinishCoding(Parse*);
int sqlite3GetTempReg(Parse*);
void sqlite3ReleaseTempReg(Parse*,int);
int sqlite3GetTempRange(Parse*,int);
void sqlite3ReleaseTempRange(Parse*,int,int);
void sqlite3ClearTempRegCache(Parse*);
Expr *sqlite3ExprAlloc(sqlite3*,int,const Token*,int);
Expr *sqlite3Expr(sqlite3*,int,const char*);
void sqlite3ExprAttachSubtrees(sqlite3*,Expr*,Expr*,Expr*);
Expr *sqlite3PExpr(Parse*, int, Expr*, Expr*, const Token*);
Expr *sqlite3ExprAnd(sqlite3*,Expr*, Expr*);
Expr *sqlite3ExprFunction(Parse*,ExprList*, Token*);
void sqlite3ExprAssignVarNumber(Parse*, Expr*);
void sqlite3ExprDelete(sqlite3*, Expr*);
ExprList *sqlite3ExprListAppend(Parse*,ExprList*,Expr*);
void sqlite3ExprListSetName(Parse*,ExprList*,Token*,int);
void sqlite3ExprListSetSpan(Parse*,ExprList*,ExprSpan*);
void sqlite3ExprListDelete(sqlite3*, ExprList*);
int sqlite3Init(sqlite3*, char**);
int sqlite3InitCallback(void*, int, char**, char**);
void sqlite3Pragma(Parse*,Token*,Token*,Token*,int);
void sqlite3ResetAllSchemasOfConnection(sqlite3*);
void sqlite3ResetOneSchema(sqlite3*,int);
void sqlite3CollapseDatabaseArray(sqlite3*);
void sqlite3BeginParse(Parse*,int);
void sqlite3CommitInternalChanges(sqlite3*);
Table *sqlite3ResultSetOfSelect(Parse*,Select*);
void sqlite3OpenMasterTable(Parse *, int);
void sqlite3StartTable(Parse*,Token*,Token*,int,int,int,int);
void sqlite3AddColumn(Parse*,Token*);
void sqlite3AddNotNull(Parse*, int);
void sqlite3AddPrimaryKey(Parse*, ExprList*, int, int, int);
void sqlite3AddCheckConstraint(Parse*, Expr*);
void sqlite3AddColumnType(Parse*,Token*);
void sqlite3AddDefaultValue(Parse*,ExprSpan*);
void sqlite3AddCollateType(Parse*, Token*);
void sqlite3EndTable(Parse*,Token*,Token*,Select*);
int sqlite3ParseUri(const char*,const char*,unsigned int*,
                    sqlite3_vfs**,char**,char **);
Btree *sqlite3DbNameToBtree(sqlite3*,const char*);
int sqlite3CodeOnce(Parse *);

Bitvec *sqlite3BitvecCreate(u32);
int sqlite3BitvecTest(Bitvec*, u32);
int sqlite3BitvecSet(Bitvec*, u32);
void sqlite3BitvecClear(Bitvec*, u32, void*);
void sqlite3BitvecDestroy(Bitvec*);
u32 sqlite3BitvecSize(Bitvec*);
int sqlite3BitvecBuiltinTest(int,int*);

RowSet *sqlite3RowSetInit(sqlite3*, void*, unsigned int);
void sqlite3RowSetClear(RowSet*);
void sqlite3RowSetInsert(RowSet*, i64);
int sqlite3RowSetTest(RowSet*, u8 iBatch, i64);
int sqlite3RowSetNext(RowSet*, i64*);

void sqlite3CreateView(Parse*,Token*,Token*,Token*,Select*,int,int);

#if !defined(SQLITE_OMIT_VIEW) || !defined(SQLITE_OMIT_VIRTUALTABLE)
  int sqlite3ViewGetColumnNames(Parse*,Table*);
#else
# define sqlite3ViewGetColumnNames(A,B) 0
#endif

void sqlite3DropTable(Parse*, SrcList*, int, int);
void sqlite3CodeDropTable(Parse*, Table*, int, int);
void sqlite3DeleteTable(sqlite3*, Table*);
#ifndef SQLITE_OMIT_AUTOINCREMENT
  void sqlite3AutoincrementBegin(Parse *pParse);
  void sqlite3AutoincrementEnd(Parse *pParse);
#else
# define sqlite3AutoincrementBegin(X)
# define sqlite3AutoincrementEnd(X)
#endif
void sqlite3Insert(Parse*, SrcList*, ExprList*, Select*, IdList*, int);
void *sqlite3ArrayAllocate(sqlite3*,void*,int,int*,int*);
IdList *sqlite3IdListAppend(sqlite3*, IdList*, Token*);
int sqlite3IdListIndex(IdList*,const char*);
SrcList *sqlite3SrcListEnlarge(sqlite3*, SrcList*, int, int);
SrcList *sqlite3SrcListAppend(sqlite3*, SrcList*, Token*, Token*);
SrcList *sqlite3SrcListAppendFromTerm(Parse*, SrcList*, Token*, Token*,
                                      Token*, Select*, Expr*, IdList*);
void sqlite3SrcListIndexedBy(Parse *, SrcList *, Token *);
int sqlite3IndexedByLookup(Parse *, struct SrcList_item *);
void sqlite3SrcListShiftJoinType(SrcList*);
void sqlite3SrcListAssignCursors(Parse*, SrcList*);
void sqlite3IdListDelete(sqlite3*, IdList*);
void sqlite3SrcListDelete(sqlite3*, SrcList*);
Index *sqlite3CreateIndex(Parse*,Token*,Token*,SrcList*,ExprList*,int,Token*,
                        Token*, int, int);
void sqlite3DropIndex(Parse*, SrcList*, int);
int sqlite3Select(Parse*, Select*, SelectDest*);
Select *sqlite3SelectNew(Parse*,ExprList*,SrcList*,Expr*,ExprList*,
                         Expr*,ExprList*,int,Expr*,Expr*);
void sqlite3SelectDelete(sqlite3*, Select*);
Table *sqlite3SrcListLookup(Parse*, SrcList*);
int sqlite3IsReadOnly(Parse*, Table*, int);
void sqlite3OpenTable(Parse*, int iCur, int iDb, Table*, int);
#if defined(SQLITE_ENABLE_UPDATE_DELETE_LIMIT) && !defined(SQLITE_OMIT_SUBQUERY)
Expr *sqlite3LimitWhere(Parse *, SrcList *, Expr *, ExprList *, Expr *, Expr *, char *);
#endif
void sqlite3DeleteFrom(Parse*, SrcList*, Expr*);
void sqlite3Update(Parse*, SrcList*, ExprList*, Expr*, int);
WhereInfo *sqlite3WhereBegin(
    Parse*,SrcList*,Expr*,ExprList**,ExprList*,u16,int);
void sqlite3WhereEnd(WhereInfo*);
int sqlite3ExprCodeGetColumn(Parse*, Table*, int, int, int, u8);
void sqlite3ExprCodeGetColumnOfTable(Vdbe*, Table*, int, int, int);
void sqlite3ExprCodeMove(Parse*, int, int, int);
void sqlite3ExprCodeCopy(Parse*, int, int, int);
void sqlite3ExprCacheStore(Parse*, int, int, int);
void sqlite3ExprCachePush(Parse*);
void sqlite3ExprCachePop(Parse*, int);
void sqlite3ExprCacheRemove(Parse*, int, int);
void sqlite3ExprCacheClear(Parse*);
void sqlite3ExprCacheAffinityChange(Parse*, int, int);
int sqlite3ExprCode(Parse*, Expr*, int);
int sqlite3ExprCodeTemp(Parse*, Expr*, int*);
int sqlite3ExprCodeTarget(Parse*, Expr*, int);
int sqlite3ExprCodeAndCache(Parse*, Expr*, int);
void sqlite3ExprCodeConstants(Parse*, Expr*);
int sqlite3ExprCodeExprList(Parse*, ExprList*, int, int);
void sqlite3ExprIfTrue(Parse*, Expr*, int, int);
void sqlite3ExprIfFalse(Parse*, Expr*, int, int);
Table *sqlite3FindTable(sqlite3*,const char*, const char*);
Table *sqlite3LocateTable(Parse*,int isView,const char*, const char*);
Index *sqlite3FindIndex(sqlite3*,const char*, const char*);
void sqlite3UnlinkAndDeleteTable(sqlite3*,int,const char*);
void sqlite3UnlinkAndDeleteIndex(sqlite3*,int,const char*);
void sqlite3Vacuum(Parse*);
int sqlite3RunVacuum(char**, sqlite3*);
char *sqlite3NameFromToken(sqlite3*, Token*);
int sqlite3ExprCompare(Expr*, Expr*);
int sqlite3ExprListCompare(ExprList*, ExprList*);
void sqlite3ExprAnalyzeAggregates(NameContext*, Expr*);
void sqlite3ExprAnalyzeAggList(NameContext*,ExprList*);
int sqlite3FunctionUsesThisSrc(Expr*, SrcList*);
Vdbe *sqlite3GetVdbe(Parse*);
void sqlite3PrngSaveState(void);
void sqlite3PrngRestoreState(void);
void sqlite3PrngResetState(void);
void sqlite3RollbackAll(sqlite3*,int);
void sqlite3CodeVerifySchema(Parse*, int);
void sqlite3CodeVerifyNamedSchema(Parse*, const char *zDb);
void sqlite3BeginTransaction(Parse*, int);
void sqlite3CommitTransaction(Parse*);
void sqlite3RollbackTransaction(Parse*);
void sqlite3Savepoint(Parse*, int, Token*);
void sqlite3CloseSavepoints(sqlite3 *);
void sqlite3LeaveMutexAndCloseZombie(sqlite3*);
int sqlite3ExprIsConstant(Expr*);
int sqlite3ExprIsConstantNotJoin(Expr*);
int sqlite3ExprIsConstantOrFunction(Expr*);
int sqlite3ExprIsInteger(Expr*, int*);
int sqlite3ExprCanBeNull(const Expr*);
void sqlite3ExprCodeIsNullJump(Vdbe*, const Expr*, int, int);
int sqlite3ExprNeedsNoAffinityChange(const Expr*, char);
int sqlite3IsRowid(const char*);
void sqlite3GenerateRowDelete(Parse*, Table*, int, int, int, Trigger *, int);
void sqlite3GenerateRowIndexDelete(Parse*, Table*, int, int*);
int sqlite3GenerateIndexKey(Parse*, Index*, int, int, int);
void sqlite3GenerateConstraintChecks(Parse*,Table*,int,int,
                                     int*,int,int,int,int,int*);
void sqlite3CompleteInsertion(Parse*, Table*, int, int, int*, int, int, int);
int sqlite3OpenTableAndIndices(Parse*, Table*, int, int);
void sqlite3BeginWriteOperation(Parse*, int, int);
void sqlite3MultiWrite(Parse*);
void sqlite3MayAbort(Parse*);
void sqlite3HaltConstraint(Parse*, int, char*, int);
Expr *sqlite3ExprDup(sqlite3*,Expr*,int);
ExprList *sqlite3ExprListDup(sqlite3*,ExprList*,int);
SrcList *sqlite3SrcListDup(sqlite3*,SrcList*,int);
IdList *sqlite3IdListDup(sqlite3*,IdList*);
Select *sqlite3SelectDup(sqlite3*,Select*,int);
void sqlite3FuncDefInsert(FuncDefHash*, FuncDef*);
FuncDef *sqlite3FindFunction(sqlite3*,const char*,int,int,u8,u8);
void sqlite3RegisterBuiltinFunctions(sqlite3*);
void sqlite3RegisterDateTimeFunctions(void);
void sqlite3RegisterGlobalFunctions(void);
int sqlite3SafetyCheckOk(sqlite3*);
int sqlite3SafetyCheckSickOrOk(sqlite3*);
void sqlite3ChangeCookie(Parse*, int);

#if !defined(SQLITE_OMIT_VIEW) && !defined(SQLITE_OMIT_TRIGGER)
void sqlite3MaterializeView(Parse*, Table*, Expr*, int);
#endif

#ifndef SQLITE_OMIT_TRIGGER
  void sqlite3BeginTrigger(Parse*, Token*,Token*,int,int,IdList*,SrcList*,
                           Expr*,int, int);
  void sqlite3FinishTrigger(Parse*, TriggerStep*, Token*);
  void sqlite3DropTrigger(Parse*, SrcList*, int);
  void sqlite3DropTriggerPtr(Parse*, Trigger*);
  Trigger *sqlite3TriggersExist(Parse *, Table*, int, ExprList*, int *pMask);
  Trigger *sqlite3TriggerList(Parse *, Table *);
  void sqlite3CodeRowTrigger(Parse*, Trigger *, int, ExprList*, int, Table *,
                            int, int, int);
  void sqlite3CodeRowTriggerDirect(Parse *, Trigger *, Table *, int, int, int);
  void sqliteViewTriggers(Parse*, Table*, Expr*, int, ExprList*);
  void sqlite3DeleteTriggerStep(sqlite3*, TriggerStep*);
  TriggerStep *sqlite3TriggerSelectStep(sqlite3*,Select*);
  TriggerStep *sqlite3TriggerInsertStep(sqlite3*,Token*, IdList*,
                                        ExprList*,Select*,u8);
  TriggerStep *sqlite3TriggerUpdateStep(sqlite3*,Token*,ExprList*, Expr*, u8);
  TriggerStep *sqlite3TriggerDeleteStep(sqlite3*,Token*, Expr*);
  void sqlite3DeleteTrigger(sqlite3*, Trigger*);
  void sqlite3UnlinkAndDeleteTrigger(sqlite3*,int,const char*);
  u32 sqlite3TriggerColmask(Parse*,Trigger*,ExprList*,int,int,Table*,int);
# define sqlite3ParseToplevel(p) ((p)->pToplevel ? (p)->pToplevel : (p))
#else
# define sqlite3TriggersExist(B,C,D,E,F) 0
# define sqlite3DeleteTrigger(A,B)
# define sqlite3DropTriggerPtr(A,B)
# define sqlite3UnlinkAndDeleteTrigger(A,B,C)
# define sqlite3CodeRowTrigger(A,B,C,D,E,F,G,H,I)
# define sqlite3CodeRowTriggerDirect(A,B,C,D,E,F)
# define sqlite3TriggerList(X, Y) 0
# define sqlite3ParseToplevel(p) p
# define sqlite3TriggerColmask(A,B,C,D,E,F,G) 0
#endif

int sqlite3JoinType(Parse*, Token*, Token*, Token*);
void sqlite3CreateForeignKey(Parse*, ExprList*, Token*, ExprList*, int);
void sqlite3DeferForeignKey(Parse*, int);
#ifndef SQLITE_OMIT_AUTHORIZATION
  void sqlite3AuthRead(Parse*,Expr*,Schema*,SrcList*);
  int sqlite3AuthCheck(Parse*,int, const char*, const char*, const char*);
  void sqlite3AuthContextPush(Parse*, AuthContext*, const char*);
  void sqlite3AuthContextPop(AuthContext*);
  int sqlite3AuthReadCol(Parse*, const char *, const char *, int);
#else
# define sqlite3AuthRead(a,b,c,d)
# define sqlite3AuthCheck(a,b,c,d,e)    SQLITE_OK
# define sqlite3AuthContextPush(a,b,c)
# define sqlite3AuthContextPop(a)  ((void)(a))
#endif
void sqlite3Attach(Parse*, Expr*, Expr*, Expr*);
void sqlite3Detach(Parse*, Expr*);
int sqlite3FixInit(DbFixer*, Parse*, int, const char*, const Token*);
int sqlite3FixSrcList(DbFixer*, SrcList*);
int sqlite3FixSelect(DbFixer*, Select*);
int sqlite3FixExpr(DbFixer*, Expr*);
int sqlite3FixExprList(DbFixer*, ExprList*);
int sqlite3FixTriggerStep(DbFixer*, TriggerStep*);
int sqlite3AtoF(const char *z, double*, int, u8);
int sqlite3GetInt32(const char *, int*);
int sqlite3Atoi(const char*);
int sqlite3Utf16ByteLen(const void *pData, int nChar);
int sqlite3Utf8CharLen(const char *pData, int nByte);
u32 sqlite3Utf8Read(const u8*, const u8**);

/*
** Routines to read and write variable-length integers.  These used to
** be defined locally, but now we use the varint routines in the util.c
** file.  Code should use the MACRO forms below, as the Varint32 versions
** are coded to assume the single byte case is already handled (which 
** the MACRO form does).
*/
int sqlite3PutVarint(unsigned char*, u64);
int sqlite3PutVarint32(unsigned char*, u32);
u8 sqlite3GetVarint(const unsigned char *, u64 *);
u8 sqlite3GetVarint32(const unsigned char *, u32 *);
int sqlite3VarintLen(u64 v);

/*
** The header of a record consists of a sequence variable-length integers.
** These integers are almost always small and are encoded as a single byte.
** The following macros take advantage this fact to provide a fast encode
** and decode of the integers in a record header.  It is faster for the common
** case where the integer is a single byte.  It is a little slower when the
** integer is two or more bytes.  But overall it is faster.
**
** The following expressions are equivalent:
**
**     x = sqlite3GetVarint32( A, &B );
**     x = sqlite3PutVarint32( A, B );
**
**     x = getVarint32( A, B );
**     x = putVarint32( A, B );
**
*/
#define getVarint32(A,B)  (u8)((*(A)<(u8)0x80) ? ((B) = (u32)*(A)),1 : sqlite3GetVarint32((A), (u32 *)&(B)))
#define putVarint32(A,B)  (u8)(((u32)(B)<(u32)0x80) ? (*(A) = (unsigned char)(B)),1 : sqlite3PutVarint32((A), (B)))
#define getVarint    sqlite3GetVarint
#define putVarint    sqlite3PutVarint


const char *sqlite3IndexAffinityStr(Vdbe *, Index *);
void sqlite3TableAffinityStr(Vdbe *, Table *);
char sqlite3CompareAffinity(Expr *pExpr, char aff2);
int sqlite3IndexAffinityOk(Expr *pExpr, char idx_affinity);
char sqlite3ExprAffinity(Expr *pExpr);
int sqlite3Atoi64(const char*, i64*, int, u8);
void sqlite3Error(sqlite3*, int, const char*,...);
void *sqlite3HexToBlob(sqlite3*, const char *z, int n);
u8 sqlite3HexToInt(int h);
int sqlite3TwoPartName(Parse *, Token *, Token *, Token **);
const char *sqlite3ErrStr(int);
int sqlite3ReadSchema(Parse *pParse);
CollSeq *sqlite3FindCollSeq(sqlite3*,u8 enc, const char*,int);
CollSeq *sqlite3LocateCollSeq(Parse *pParse, const char*zName);
CollSeq *sqlite3ExprCollSeq(Parse *pParse, Expr *pExpr);
Expr *sqlite3ExprSetColl(Expr*, CollSeq*);
Expr *sqlite3ExprSetCollByToken(Parse *pParse, Expr*, Token*);
int sqlite3CheckCollSeq(Parse *, CollSeq *);
int sqlite3CheckObjectName(Parse *, const char *);
void sqlite3VdbeSetChanges(sqlite3 *, int);
int sqlite3AddInt64(i64*,i64);
int sqlite3SubInt64(i64*,i64);
int sqlite3MulInt64(i64*,i64);
int sqlite3AbsInt32(int);
#ifdef SQLITE_ENABLE_8_3_NAMES
void sqlite3FileSuffix3(const char*, char*);
#else
# define sqlite3FileSuffix3(X,Y)
#endif
u8 sqlite3GetBoolean(const char *z,int);

const void *sqlite3ValueText(sqlite3_value*, u8);
int sqlite3ValueBytes(sqlite3_value*, u8);
void sqlite3ValueSetStr(sqlite3_value*, int, const void *,u8, 
                        void(*)(void*));
void sqlite3ValueFree(sqlite3_value*);
sqlite3_value *sqlite3ValueNew(sqlite3 *);
char *sqlite3Utf16to8(sqlite3 *, const void*, int, u8);
#ifdef SQLITE_ENABLE_STAT3
char *sqlite3Utf8to16(sqlite3 *, u8, char *, int, int *);
#endif
int sqlite3ValueFromExpr(sqlite3 *, Expr *, u8, u8, sqlite3_value **);
void sqlite3ValueApplyAffinity(sqlite3_value *, u8, u8);
#ifndef SQLITE_AMALGAMATION
extern const unsigned char sqlite3OpcodeProperty[];
extern const unsigned char sqlite3UpperToLower[];
extern const unsigned char sqlite3CtypeMap[];
extern const Token sqlite3IntTokens[];
extern SQLITE_WSD struct Sqlite3Config sqlite3Config;
extern SQLITE_WSD FuncDefHash sqlite3GlobalFunctions;
#ifndef SQLITE_OMIT_WSD
extern int sqlite3PendingByte;
#endif
#endif
void sqlite3RootPageMoved(sqlite3*, int, int, int);
void sqlite3Reindex(Parse*, Token*, Token*);
void sqlite3AlterFunctions(void);
void sqlite3AlterRenameTable(Parse*, SrcList*, Token*);
int sqlite3GetToken(const unsigned char *, int *);
void sqlite3NestedParse(Parse*, const char*, ...);
void sqlite3ExpirePreparedStatements(sqlite3*);
int sqlite3CodeSubselect(Parse *, Expr *, int, int);
void sqlite3SelectPrep(Parse*, Select*, NameContext*);
int sqlite3ResolveExprNames(NameContext*, Expr*);
void sqlite3ResolveSelectNames(Parse*, Select*, NameContext*);
int sqlite3ResolveOrderGroupBy(Parse*, Select*, ExprList*, const char*);
void sqlite3ColumnDefault(Vdbe *, Table *, int, int);
void sqlite3AlterFinishAddColumn(Parse *, Token *);
void sqlite3AlterBeginAddColumn(Parse *, SrcList *);
CollSeq *sqlite3GetCollSeq(sqlite3*, u8, CollSeq *, const char*);
char sqlite3AffinityType(const char*);
void sqlite3Analyze(Parse*, Token*, Token*);
int sqlite3InvokeBusyHandler(BusyHandler*);
int sqlite3FindDb(sqlite3*, Token*);
int sqlite3FindDbName(sqlite3 *, const char *);
int sqlite3AnalysisLoad(sqlite3*,int iDB);
void sqlite3DeleteIndexSamples(sqlite3*,Index*);
void sqlite3DefaultRowEst(Index*);
void sqlite3RegisterLikeFunctions(sqlite3*, int);
int sqlite3IsLikeFunction(sqlite3*,Expr*,int*,char*);
void sqlite3MinimumFileFormat(Parse*, int, int);
void sqlite3SchemaClear(void *);
Schema *sqlite3SchemaGet(sqlite3 *, Btree *);
int sqlite3SchemaToIndex(sqlite3 *db, Schema *);
KeyInfo *sqlite3IndexKeyinfo(Parse *, Index *);
int sqlite3CreateFunc(sqlite3 *, const char *, int, int, void *, 
  void (*)(sqlite3_context*,int,sqlite3_value **),
  void (*)(sqlite3_context*,int,sqlite3_value **), void (*)(sqlite3_context*),
  FuncDestructor *pDestructor
);
int sqlite3ApiExit(sqlite3 *db, int);
int sqlite3OpenTempDatabase(Parse *);

void sqlite3StrAccumInit(StrAccum*, char*, int, int);
void sqlite3StrAccumAppend(StrAccum*,const char*,int);
void sqlite3AppendSpace(StrAccum*,int);
char *sqlite3StrAccumFinish(StrAccum*);
void sqlite3StrAccumReset(StrAccum*);
void sqlite3SelectDestInit(SelectDest*,int,int);
Expr *sqlite3CreateColumnExpr(sqlite3 *, SrcList *, int, int);

void sqlite3BackupRestart(sqlite3_backup *);
void sqlite3BackupUpdate(sqlite3_backup *, Pgno, const u8 *);

/*
** The interface to the LEMON-generated parser
*/
void *sqlite3ParserAlloc(void*(*)(size_t));
void sqlite3ParserFree(void*, void(*)(void*));
void sqlite3Parser(void*, int, Token, Parse*);
#ifdef YYTRACKMAXSTACKDEPTH
  int sqlite3ParserStackPeak(void*);
#endif

void sqlite3AutoLoadExtensions(sqlite3*);
#ifndef SQLITE_OMIT_LOAD_EXTENSION
  void sqlite3CloseExtensions(sqlite3*);
#else
# define sqlite3CloseExtensions(X)
#endif

#ifndef SQLITE_OMIT_SHARED_CACHE
  void sqlite3TableLock(Parse *, int, int, u8, const char *);
#else
  #define sqlite3TableLock(v,w,x,y,z)
#endif

#ifdef SQLITE_TEST
  int sqlite3Utf8To8(unsigned char*);
#endif

#ifdef SQLITE_OMIT_VIRTUALTABLE
#  define sqlite3VtabClear(Y)
#  define sqlite3VtabSync(X,Y) SQLITE_OK
#  define sqlite3VtabRollback(X)
#  define sqlite3VtabCommit(X)
#  define sqlite3VtabInSync(db) 0
#  define sqlite3VtabLock(X) 
#  define sqlite3VtabUnlock(X)
#  define sqlite3VtabUnlockList(X)
#  define sqlite3VtabSavepoint(X, Y, Z) SQLITE_OK
#  define sqlite3GetVTable(X,Y)  ((VTable*)0)
#else
   void sqlite3VtabClear(sqlite3 *db, Table*);
   void sqlite3VtabDisconnect(sqlite3 *db, Table *p);
   int sqlite3VtabSync(sqlite3 *db, char **);
   int sqlite3VtabRollback(sqlite3 *db);
   int sqlite3VtabCommit(sqlite3 *db);
   void sqlite3VtabLock(VTable *);
   void sqlite3VtabUnlock(VTable *);
   void sqlite3VtabUnlockList(sqlite3*);
   int sqlite3VtabSavepoint(sqlite3 *, int, int);
   VTable *sqlite3GetVTable(sqlite3*, Table*);
#  define sqlite3VtabInSync(db) ((db)->nVTrans>0 && (db)->aVTrans==0)
#endif
void sqlite3VtabMakeWritable(Parse*,Table*);
void sqlite3VtabBeginParse(Parse*, Token*, Token*, Token*, int);
void sqlite3VtabFinishParse(Parse*, Token*);
void sqlite3VtabArgInit(Parse*);
void sqlite3VtabArgExtend(Parse*, Token*);
int sqlite3VtabCallCreate(sqlite3*, int, const char *, char **);
int sqlite3VtabCallConnect(Parse*, Table*);
int sqlite3VtabCallDestroy(sqlite3*, int, const char *);
int sqlite3VtabBegin(sqlite3 *, VTable *);
FuncDef *sqlite3VtabOverloadFunction(sqlite3 *,FuncDef*, int nArg, Expr*);
void sqlite3InvalidFunction(sqlite3_context*,int,sqlite3_value**);
int sqlite3VdbeParameterIndex(Vdbe*, const char*, int);
int sqlite3TransferBindings(sqlite3_stmt *, sqlite3_stmt *);
int sqlite3Reprepare(Vdbe*);
void sqlite3ExprListCheckLength(Parse*, ExprList*, const char*);
CollSeq *sqlite3BinaryCompareCollSeq(Parse *, Expr *, Expr *);
int sqlite3TempInMemory(const sqlite3*);
const char *sqlite3JournalModename(int);
int sqlite3Checkpoint(sqlite3*, int, int, int*, int*);
int sqlite3WalDefaultHook(void*,sqlite3*,const char*,int);

/* Declarations for functions in fkey.c. All of these are replaced by
** no-op macros if OMIT_FOREIGN_KEY is defined. In this case no foreign
** key functionality is available. If OMIT_TRIGGER is defined but
** OMIT_FOREIGN_KEY is not, only some of the functions are no-oped. In
** this case foreign keys are parsed, but no other functionality is 
** provided (enforcement of FK constraints requires the triggers sub-system).
*/
#if !defined(SQLITE_OMIT_FOREIGN_KEY) && !defined(SQLITE_OMIT_TRIGGER)
  void sqlite3FkCheck(Parse*, Table*, int, int);
  void sqlite3FkDropTable(Parse*, SrcList *, Table*);
  void sqlite3FkActions(Parse*, Table*, ExprList*, int);
  int sqlite3FkRequired(Parse*, Table*, int*, int);
  u32 sqlite3FkOldmask(Parse*, Table*);
  FKey *sqlite3FkReferences(Table *);
#else
  #define sqlite3FkActions(a,b,c,d)
  #define sqlite3FkCheck(a,b,c,d)
  #define sqlite3FkDropTable(a,b,c)
  #define sqlite3FkOldmask(a,b)      0
  #define sqlite3FkRequired(a,b,c,d) 0
#endif
#ifndef SQLITE_OMIT_FOREIGN_KEY
  void sqlite3FkDelete(sqlite3 *, Table*);
#else
  #define sqlite3FkDelete(a,b)
#endif


/*
** Available fault injectors.  Should be numbered beginning with 0.
*/
#define SQLITE_FAULTINJECTOR_MALLOC     0
#define SQLITE_FAULTINJECTOR_COUNT      1

/*
** The interface to the code in fault.c used for identifying "benign"
** malloc failures. This is only present if SQLITE_OMIT_BUILTIN_TEST
** is not defined.
*/
#ifndef SQLITE_OMIT_BUILTIN_TEST
  void sqlite3BeginBenignMalloc(void);
  void sqlite3EndBenignMalloc(void);
#else
  #define sqlite3BeginBenignMalloc()
  #define sqlite3EndBenignMalloc()
#endif

#define IN_INDEX_ROWID           1
#define IN_INDEX_EPH             2
#define IN_INDEX_INDEX           3
int sqlite3FindInIndex(Parse *, Expr *, int*);

#ifdef SQLITE_ENABLE_ATOMIC_WRITE
  int sqlite3JournalOpen(sqlite3_vfs *, const char *, sqlite3_file *, int, int);
  int sqlite3JournalSize(sqlite3_vfs *);
  int sqlite3JournalCreate(sqlite3_file *);
#else
  #define sqlite3JournalSize(pVfs) ((pVfs)->szOsFile)
#endif

void sqlite3MemJournalOpen(sqlite3_file *);
int sqlite3MemJournalSize(void);
int sqlite3IsMemJournal(sqlite3_file *);

#if SQLITE_MAX_EXPR_DEPTH>0
  void sqlite3ExprSetHeight(Parse *pParse, Expr *p);
  int sqlite3SelectExprHeight(Select *);
  int sqlite3ExprCheckHeight(Parse*, int);
#else
  #define sqlite3ExprSetHeight(x,y)
  #define sqlite3SelectExprHeight(x) 0
  #define sqlite3ExprCheckHeight(x,y)
#endif

u32 sqlite3Get4byte(const u8*);
void sqlite3Put4byte(u8*, u32);

#ifdef SQLITE_ENABLE_UNLOCK_NOTIFY
  void sqlite3ConnectionBlocked(sqlite3 *, sqlite3 *);
  void sqlite3ConnectionUnlocked(sqlite3 *db);
  void sqlite3ConnectionClosed(sqlite3 *db);
#else
  #define sqlite3ConnectionBlocked(x,y)
  #define sqlite3ConnectionUnlocked(x)
  #define sqlite3ConnectionClosed(x)
#endif

#ifdef SQLITE_DEBUG
  void sqlite3ParserTrace(FILE*, char *);
#endif

/*
** If the SQLITE_ENABLE IOTRACE exists then the global variable
** sqlite3IoTrace is a pointer to a printf-like routine used to
** print I/O tracing messages. 
*/
#ifdef SQLITE_ENABLE_IOTRACE
# define IOTRACE(A)  if( sqlite3IoTrace ){ sqlite3IoTrace A; }
  void sqlite3VdbeIOTraceSql(Vdbe*);
SQLITE_EXTERN void (*sqlite3IoTrace)(const char*,...);
#else
# define IOTRACE(A)
# define sqlite3VdbeIOTraceSql(X)
#endif

/*
** These routines are available for the mem2.c debugging memory allocator
** only.  They are used to verify that different "types" of memory
** allocations are properly tracked by the system.
**
** sqlite3MemdebugSetType() sets the "type" of an allocation to one of
** the MEMTYPE_* macros defined below.  The type must be a bitmask with
** a single bit set.
**
** sqlite3MemdebugHasType() returns true if any of the bits in its second
** argument match the type set by the previous sqlite3MemdebugSetType().
** sqlite3MemdebugHasType() is intended for use inside assert() statements.
**
** sqlite3MemdebugNoType() returns true if none of the bits in its second
** argument match the type set by the previous sqlite3MemdebugSetType().
**
** Perhaps the most important point is the difference between MEMTYPE_HEAP
** and MEMTYPE_LOOKASIDE.  If an allocation is MEMTYPE_LOOKASIDE, that means
** it might have been allocated by lookaside, except the allocation was
** too large or lookaside was already full.  It is important to verify
** that allocations that might have been satisfied by lookaside are not
** passed back to non-lookaside free() routines.  Asserts such as the
** example above are placed on the non-lookaside free() routines to verify
** this constraint. 
**
** All of this is no-op for a production build.  It only comes into
** play when the SQLITE_MEMDEBUG compile-time option is used.
*/
#ifdef SQLITE_MEMDEBUG
  void sqlite3MemdebugSetType(void*,u8);
  int sqlite3MemdebugHasType(void*,u8);
  int sqlite3MemdebugNoType(void*,u8);
#else
# define sqlite3MemdebugSetType(X,Y)  /* no-op */
# define sqlite3MemdebugHasType(X,Y)  1
# define sqlite3MemdebugNoType(X,Y)   1
#endif
#define MEMTYPE_HEAP       0x01  /* General heap allocations */
#define MEMTYPE_LOOKASIDE  0x02  /* Might have been lookaside memory */
#define MEMTYPE_SCRATCH    0x04  /* Scratch allocations */
#define MEMTYPE_PCACHE     0x08  /* Page cache allocations */
#define MEMTYPE_DB         0x10  /* Uses sqlite3DbMalloc, not sqlite_malloc */

#endif /* _SQLITEINT_H_ */
