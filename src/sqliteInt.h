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
** Name of the master database table.  The master database table   主数据库表名。  
** is a special table that holds the names and attributes of all   主数据库表是一个特殊的表，拥有所有用户数据表和索引的名字和特征属性。
** user tables and indices.
*/
#define MASTER_NAME       "sqlite_master"
#define TEMP_MASTER_NAME  "sqlite_temp_master"

/*
** The root-page of the master database table.    主数据库表的根页。
*/
#define MASTER_ROOT       1

/*
** The name of the schema table.   模式表表名。
*/
#define SCHEMA_TABLE(x)  ((!OMIT_TEMPDB)&&(x==1)?TEMP_MASTER_NAME:MASTER_NAME)

/*
** A convenience macro that returns the number of elements in   一个很方便的宏，可以返回数组中元素的个数。
** an array.
*/
#define ArraySize(X)    ((int)(sizeof(X)/sizeof(X[0])))

/*
** The following value as a destructor means to use sqlite3DbFree().       以下的值作为一个析构函数意味着要使用sqlite3dbfree()
** The sqlite3DbFree() routine requires two parameters instead of the      常规的sqlite3dbfree()需要两个参数来代替析构函数通常所需要的一个参数
** one parameter that destructors normally want.  So we have to introduce  所以我们必须引入这个魔法值，魔法值的代码知道处理差异。
** this magic value that the code knows to handle differently.  Any 
** pointer will work here as long as it is distinct from SQLITE_STATIC     所有指针，只要与SQLITE_STATIC和SQLITE_TRANSIENT不同，都在这里起作用。
** and SQLITE_TRANSIENT.
*/
#define SQLITE_DYNAMIC   ((sqlite3_destructor_type)sqlite3MallocSize)

/*
** When SQLITE_OMIT_WSD is defined, it means that the target platform does     当SQLITE_OMIT_WSD被定义时，这就意味着目标平台不支持全局可写变量，例如全局变量和静态变量。
** not support Writable Static Data (WSD) such as global and static variables.   WSD:全局可写变量Writable Static Data (WSD)
** All variables must either be on the stack or dynamically allocated from     所有变量都必须是在堆栈上或从堆中动态分配的。
** the heap.  When WSD is unsupported, the variable declarations scattered     当WSD不支持时，遍布在SQLite代码中的变量声明必须变成常数代替。
** throughout the SQLite code must become constants instead.  The SQLITE_WSD   宏SQLITE_WSD就是用来达到这个目的的。
** macro is used for this purpose.  And instead of referencing the variable
** directly, we use its constant as a key to lookup the run-time allocated     代替直接引用变量，我们使用常量作为查找运行时分配存放实型变量的缓冲区的关键。
** buffer that holds real variable.  The constant is also the initializer      常量也是初始化运行时分配缓冲区的关键。
** for the run-time allocated buffer.
**
** In the usual case where WSD is supported, the SQLITE_WSD and GLOBAL         在通常的情况下WSD是被支持的,宏SQLITE_WSD 和 GLOBAL成了空操作，并且不影响执行。
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
** The following macros are used to suppress compiler warnings and to          以下的宏被用来来抑制编译器警告，
** make it clear to human readers when a function parameter is deliberately    而且当一个函数的参数是故意落在函数体内部未使用是，可以让人类读者清楚的知道这一点。
** left unused within the body of a function. This usually happens when
** a function is called via a function pointer. For example the                这通常发生在通过一个函数指针的调用函数时。
** implementation of an SQL aggregate step callback may not use the            例如，SQL聚合步骤的实现回调可能不会使用参数指出传递到总体的参数数量,
** parameter indicating the number of arguments passed to the aggregate,       如果它知道这是在其他地方强制执行的。
** if it knows that this is enforced elsewhere.
**
** When a function parameter is not used at all within the body of a function, 当一个函数的参数是根本不用在函数体内部，一般称之为 "NotUsed"或"NotUsed2"以使事情更加清晰。
** it is generally named "NotUsed" or "NotUsed2" to make things even clearer.
** However, these macros may also be used to suppress warnings related to      然而，这些宏也可以用来抑制，可能会或可能不会被用于根据编译选项参数相关的警告。
** parameters that may or may not be used depending on compilation options.
** For example those parameters only used in assert() statements. In these     例如，这些参数仅用于assert()语句。在这些情况下，参数的命名按惯例。
** cases the parameters are named as per the usual conventions.
*/
#define UNUSED_PARAMETER(x) (void)(x)
#define UNUSED_PARAMETER2(x,y) UNUSED_PARAMETER(x),UNUSED_PARAMETER(y)

/*
** Forward references to structures   结构体的前向引用
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
** Defer sourcing vdbe.h and btree.h until after the "u8" and               在"u8"和"BusyHandler"类型定义之后推迟 vdbe.h 和 btree.h的发起。
** "BusyHandler" typedefs. vdbe.h also requires a few of the opaque         vdbe.h也需要一些不透明指针型(即函数定义)定义上面的东西。
** pointer types (i.e. FuncDef) defined above.         不透明数据类型隐藏了它们内部格式或结构。在C语言中，它们就像黑盒一样。支持它们的语言不是很多。作为替代，开发者们利用typedef声明一个类型，把它叫做不透明类型，希望其他人别去把它重新转化回对应的那个标准C类型。
*/
#include "btree.h"
#include "vdbe.h"
#include "pager.h"
#include "pcache.h"

#include "os.h"
#include "mutex.h"


/*
** Each database file to be accessed by the system is an instance  每个数据库文件将被系统访问下列结构实例。
** of the following structure.  There are normally two of these structures  在数组sqlite.aDb[]里通常有两个这种结构。
** in the sqlite.aDb[] array.  aDb[0] is the main database file and         aDb[0]是主数据库文件，aDb[1]是用于存放临时数据表的数据库文件。
** aDb[1] is the database file used to hold temporary tables.  Additional   附加数据库可以被连接。
** databases may be attached.  
*/
struct Db {
<<<<<<< HEAD
  char *zName;         /* Name of this database 数据库名称*/
  Btree *pBt;          /* The B*Tree structure for this database file 数据库文件的B*Tree结构*/
  u8 inTrans;          /* 0: not writable.  1: Transaction.  2: Checkpoint */
  u8 safety_level;     /* How aggressive at syncing data to disk   0:不可写 1:事务处理  2:检查*/
  Schema *pSchema;     /* Pointer to database schema (possibly shared) 指向数据库模式的指针(可能是共享的)*/
=======
  char *zName;         /* Name of this database 数据库的名字*/
  Btree *pBt;          /* The B*Tree structure for this database file 此数据库文件的B树结构*/
  u8 inTrans;          /* 0: not writable.  1: Transaction.  2: Checkpoint 0：不可写 1：事务 2:检查点*/
  u8 safety_level;     /* How aggressive at syncing data to disk  将数据同步到磁盘的可靠程度 */
  Schema *pSchema;     /* Pointer to database schema (possibly shared)  指向数据库模式(可能被共享)*/
>>>>>>> ba548246c0eb8783d5eca71d784e77030f3fe838
};

/*
** An instance of the following structure stores a database schema.
**//以下结构体的一个实例存储的是数据库的模式。
** Most Schema objects are associated with a Btree.  The exception is  //大多数的模式对象都与树相关，只有TEMP数据库模式例外，它不需要其他结构的支撑。
** the Schema for the TEMP databaes (sqlite3.aDb[1]) which is free-standing.
** In shared cache mode, a single Schema object can be shared by multiple//在共享缓存模式下，一个单一的模式对象可以为多个B树所共享，指的是同一个底层BtShared对象。
** Btrees that refer to the same underlying BtShared object.
** 
** Schema objects are automatically deallocated when the last Btree that
** references them is destroyed.   The TEMP Schema is manually freed by
** sqlite3_close().//当没有B树引用该模式对象的时候，该模式对象将被系统自动释放，但是TEMP模式对象需要利用sqlite3_close()函数来手动释放。
*
** A thread must be holding a mutex on the corresponding Btree in order    //线程必须拥有相应的B树互斥才可以访问模式的内容。
** to access Schema content.  This implies that the thread must also be   //这意味着该线程必须同时拥有sqlite3连接指针的互斥。
** holding a mutex on the sqlite3 connection pointer that owns the Btree.
** For a TEMP Schema, only the connection mutex is required.//对于TEMP模式，只有连接互斥是必须的。
*/
struct Schema {
  int schema_cookie;   /* Database schema version number for this file该文件数据库模式的版本号 */
  int iGeneration;     /* Generation counter.  Incremented with each change 代计数器，随着模式的改变其值递增*/
  Hash tblHash;        /* All tables indexed by name 每个数据库表都是通过其名字来进行索引*/
  Hash idxHash;        /* All (named) indices indexed by name 用名字索引已命名的索引*/
  Hash trigHash;       /* All triggers indexed by name所有的触发器利用名字进行索引 */
  Hash fkeyHash;       /* All foreign keys by referenced table name 外键通过所参照表的名字进行索引*/
  Table *pSeqTab;      /* The sqlite_sequence table used by AUTOINCREMENT 被AUTOINCREMENThe使用的sqlite_sequence表*/
  u8 file_format;      /* Schema format version for this fil文件的模式格式版本 */
  u8 enc;              /* Text encoding used by this database数据库所使用的文字编码 */
  u16 flags;           /* Flags associated with this schema 与该模式相关联的标志*/
  int cache_size;      /* Number of pages to use in the cache 在缓存cache中使用的快数*/
};

/*
** These macros can be used to test, set, or clear bits in the 
** Db.pSchema->flags field.//这些宏可用于测试，设置或清除在Db.pSchema->flags字段中的位信息。
*/
#define DbHasProperty(D,I,P)     (((D)->aDb[I].pSchema->flags&(P))==(P))
#define DbHasAnyProperty(D,I,P)  (((D)->aDb[I].pSchema->flags&(P))!=0)
#define DbSetProperty(D,I,P)     (D)->aDb[I].pSchema->flags|=(P)
#define DbClearProperty(D,I,P)   (D)->aDb[I].pSchema->flags&=~(P)

/*
** Allowed values for the DB.pSchema->flags field.   //DB.pSchema->flags 字段所允许的值。
**
** The DB_SchemaLoaded flag is set after the database schema has been
** read into internal hash tables.  //当数据库模式被读入内部哈希表之后，DB_SchemaLoaded标志将被设置。
**
** DB_UnresetViews means that one or more views have column names that 
** have been filled out.  If the schema changes, these column names might
** changes and so the view will need to be reset.//DB_UnresetViews是指一个或者多个视图有列名，当模式改变的时候，列的名字或许也会变，所以视图也需要被复位。
*/
#define DB_SchemaLoaded    0x0001  /* The schema has been loaded 模式已被加载*/
#define DB_UnresetViews    0x0002  /* Some views have defined column names 一些定义列名字的视图*/
#define DB_Empty           0x0004  /* The file is empty (length 0 bytes) 空文件*/

/*
** The number of different kinds of things that can be limited
** using the sqlite3_limit() interface.//不同种类东西的数量是可以用sqlite3_limit()接口来进行限制的。
*/
#define SQLITE_N_LIMIT (SQLITE_LIMIT_TRIGGER_DEPTH+1)

/*
** Lookaside malloc is a set of fixed-size buffers that can be used    //malloc全称是memory allocation,即动态内存分配，无法知道内存具体位置的时候，想要绑定真正的内存空间，就需要用到动态的分配内存。
** to satisfy small transient memory allocation requests for objects
** associated with a particular database connection.  The use of       //后备动态内存分配是一组固定大小的缓冲区，可以用来满足一个特定数据库连接对象的小的即时内存分配请求。
** lookaside malloc provides a significant performance enhancement
** (approx 10%) by avoiding numerous malloc/free requests while parsing
** SQL statements.//通过避免SQL语句解析时的大量的动态内存分配或释放，后备动态内存缓冲区的使用使得SQLite的性能有显著的提高，大约有10%。
**
** The Lookaside structure holds configuration information about the
** lookaside malloc subsystem.  Each available memory allocation in   //Lookaside结构体拥有后备动态内存分配子系统的配置信息。
** the lookaside subsystem is stored on a linked list of LookasideSlot
** objects.  在后备内存分配子系统中的每一个可用的内存分配空间都是存储在结构体类型LookasideSlot类对象的链接列表中。//
**
** Lookaside allocations are only allowed for objects that are associated  //只允许对与特定的数据库连接相关联的对象进行后备动态内存分配。
** with a particular database connection.  Hence, schema information cannot
** be stored in lookaside because in shared cache mode the schema information//所以，模式信息不能存储在后备内存区里，因为在共享cache模块中模式信息是被多个数据库连接所共享的。
** is shared by multiple database connections.  Therefore, while parsing
** schema information, the Lookaside.bEnabled flag is cleared so that
** lookaside allocations are not used to construct the schema objects. //因此，当解析模式信息时，Lookaside.bEnabled标志将会被清除，来保证后备内存分配不会被用来构建模式对象。
*/
struct Lookaside {
  u16 sz;                 /* Size of each buffer in bytes , u16是sqlite内部自定义的一个类型，即是UINT16_TYPE，2-byte unsigned integer，两字节的无符号整数，sz代表每一个缓冲区的大小，即其所包含的字节数。*/
  u8 bEnabled;            /* False to disable new lookaside allocations , bEnabled是一个标志位，占用两个字节的无符号整数，表示可以进行新的后备内存区的分配。*/
  u8 bMalloced;           /* True if pStart obtained from sqlite3_malloc()  如果有足够的后备内存区，则bMalloced的值即为真。 */
  int nOut;               /* Number of buffers currently checked out 当前已知的缓冲区数量。 */
  int mxOut;              /* Highwater mark for nOut nOut的最大标记? */
  int anStat[3];          /* 0: hits.  1: size misses.  2: full misses   */
  LookasideSlot *pFree;   /* List of available buffers  可用缓冲区列表*/
  void *pStart;           /* First byte of available memory space  可用内存空间的第一个字节*/
  void *pEnd;             /* First byte past end of available space 可用空间后的首个字节*/
};
struct LookasideSlot {
  LookasideSlot *pNext;    /* Next buffer in the list of free buffers , pNext是一个LookasideSlot结构体类型的指针，指向的是空间缓冲区列表中的下一个缓冲区。*/
};

/*
** A hash table for function definitions.   //用于函数定义的哈希表
**
** Hash each FuncDef structure into one of the FuncDefHash.a[] slots.
** Collisions are on the FuncDef.pHash chain. //将每个FuncDef结构散列进FuncDefHash.a[]槽中，在FuncDef.pHash链上存在碰撞。
*/
struct FuncDefHash {
  FuncDef *a[23];       /* Hash table for functions 函数的哈希表*/
};

/*
** Each database connection is an instance of the following structure. //每一个数据库链接都是如下结构体的一个实例
*/
struct sqlite3 {
  sqlite3_vfs *pVfs;            /* OS Interface 操作系统接口 */
  struct Vdbe *pVdbe;           /* List of active virtual machines 活动虚拟机列表*/
  CollSeq *pDfltColl;           /* The default collating sequence (BINARY) 默认排序顺序*/
  sqlite3_mutex *mutex;         /* Connection mutex 链接互斥*/
  Db *aDb;                      /* All backends 所有后端*/
  int nDb;                      /* Number of backends currently in use 目前所使用的后端数*/
  int flags;                    /* Miscellaneous flags. See below 杂项标志*/
  i64 lastRowid;                /* ROWID of most recent insert (see above) 最近插入的行ID*/
  unsigned int openFlags;       /* Flags passed to sqlite3_vfs.xOpen() 传递给sqlite3_vfs.xOpen()函数的标志*/
  int errCode;                  /* Most recent error code (SQLITE_*) 最近的错误代码*/
  int errMask;                  /* & result codes with this before returning 所出现错误的提示码*/
  u8 autoCommit;                /* The auto-commit flag. 自动提交标志*/
  u8 temp_store;                /* 1: file 2: memory 0: default 1:文件  2:内存  0:默认*/
  u8 mallocFailed;              /* True if we have seen a malloc failure 若动态内存分配失败即为真*/
  u8 dfltLockMode;              /* Default locking-mode for attached dbs 附加数据库系统的默认锁定模式*/
  signed char nextAutovac;      /* Autovac setting after VACUUM if >=0 */
  u8 suppressErr;               /* Do not issue error messages if true 若为真则不提示错误信息*/
  u8 vtabOnConflict;            /* Value to return for s3_vtab_on_conflict() , 返回给s3_vtab_on_conflict()函数的值*/
  u8 isTransactionSavepoint;    /* True if the outermost savepoint is a TS 若外层保存点是一个事务保存点，则为真*/
  int nextPagesize;             /* Pagesize after VACUUM if >0 */
  u32 magic;                    /* Magic number for detect library misuse 幻数检测库滥用*/
  int nChange;                  /* Value returned by sqlite3_changes() , sqlite3_changes()函数所返回的值*/
  int nTotalChange;             /* Value returned by sqlite3_total_changes() , sqlite3_total_changes()函数所返回的值*/
  int aLimit[SQLITE_N_LIMIT];   /* Limits 限制信息*/
  struct sqlite3InitInfo {      /* Information used during initialization 初始化时候所使用到的信息*/
    int newTnum;                /* Rootpage of table being initialized 表的Rootpage被初始化*/
    u8 iDb;                     /* Which db file is being initialized 哪一个数据库文件被初始化*/
    u8 busy;                    /* TRUE if currently initializing 若当前正在被初始化，即为真*/
    u8 orphanTrigger;           /* Last statement is orphaned TEMP trigger 最后一条语句是一个孤立的TEMP触发器*/
  } init;
  int activeVdbeCnt;            /* Number of VDBEs currently executing 正在执行的虚拟数据库引擎的数目*/
  int writeVdbeCnt;             /* Number of active VDBEs that are writing 活跃的写虚拟数据库引擎的数目*/
  int vdbeExecCnt;              /* Number of nested calls to VdbeExec() 嵌套调用VdbeExec()的次数*/
  int nExtension;               /* Number of loaded extensions 加载扩展数*/
  void **aExtension;            /* Array of shared library handles 共享库句柄数组*/
  void (*xTrace)(void*,const char*);        /* Trace function 跟踪功能*/
  void *pTraceArg;                          /* Argument to the trace function 跟踪功能的参数*/
  void (*xProfile)(void*,const char*,u64);  /* Profiling function 分析功能*/
  void *pProfileArg;                        /* Argument to profile function 分析功能的参数*/
  void *pCommitArg;                 /* Argument to xCommitCallback() 函数xCommitCallback()的参数*/   
  int (*xCommitCallback)(void*);    /* Invoked at every commit. 每次提交时都会被调用*/
  void *pRollbackArg;               /* Argument to xRollbackCallback() */   
  void (*xRollbackCallback)(void*); /* Invoked at every commit. 函数xRollbackCallback()的参数*/
  void *pUpdateArg;
  void (*xUpdateCallback)(void*,int, const char*,const char*,sqlite_int64);
#ifndef SQLITE_OMIT_WAL
  int (*xWalCallback)(void *, sqlite3 *, const char *, int);
  void *pWalArg;
#endif
  void(*xCollNeeded)(void*,sqlite3*,int eTextRep,const char*);
  void(*xCollNeeded16)(void*,sqlite3*,int eTextRep,const void*);
  void *pCollNeededArg;
  sqlite3_value *pErr;          /* Most recent error message 最近的错误信息*/
  char *zErrMsg;                /* Most recent error message (UTF-8 encoded) 单字节编码的最近错误信息*/
  char *zErrMsg16;              /* Most recent error message (UTF-16 encoded) 双字节编码的最近错误信息*/
  union {
    volatile int isInterrupted; /* True if sqlite3_interrupt has been called 若sqlite3_interrupt被调用即为真*/
    double notUsed1;            /* Spacer */
  } u1;
  Lookaside lookaside;          /* Lookaside malloc configuration 后备动态内存分配配置*/
#ifndef SQLITE_OMIT_AUTHORIZATION
  int (*xAuth)(void*,int,const char*,const char*,const char*,const char*);
                                /* Access authorization function 访问授权功能*/
  void *pAuthArg;               /* 1st argument to the access auth function 访问身份验证功能的首个参数*/
#endif
#ifndef SQLITE_OMIT_PROGRESS_CALLBACK
  int (*xProgress)(void *);     /* The progress callback 进程回滚*/
  void *pProgressArg;           /* Argument to the progress callback 进程回滚的参数*/
  int nProgressOps;             /* Number of opcodes for progress callback 进程回滚所需要的操作码的数目*/
#endif
#ifndef SQLITE_OMIT_VIRTUALTABLE
  int nVTrans;                  /* Allocated size of aVTrans 为aVTrans(开放交易的虚拟表)所分配的大小*/
  Hash aModule;                 /* populated by sqlite3_create_module() 通过sqlite3_create_module()函数填充*/
  VtabCtx *pVtabCtx;            /* Context for active vtab connect/create 活跃的未分类连接/创建*/
  VTable **aVTrans;             /* Virtual tables with open transactions 开放交易的虚拟表*/
  VTable *pDisconnect;    /* Disconnect these in next sqlite3_prepare() 在接下来的sqlite3_prepare()函数中断开这些链接*/
#endif
  FuncDefHash aFunc;            /* Hash table of connection functions 连接功能哈希表*/
  Hash aCollSeq;                /* All collating sequences 所有排序序列*/
  BusyHandler busyHandler;      /* Busy callback 回滚繁忙*/
  Db aDbStatic[2];              /* Static space for the 2 default backends 2默认后端的静态空间*/
  Savepoint *pSavepoint;        /* List of active savepoints 活动保存点列表*/
  int busyTimeout;              /* Busy handler timeout, in msec 忙处理超时，以毫秒为单位*/
  int nSavepoint;               /* Number of non-transaction savepoints 非交易保存点的数?*/
  int nStatement;               /* Number of nested statement-transactions  嵌套事务语句的数量*/
  i64 nDeferredCons;            /* Net deferred constraints this transaction. 网络延迟约束这个交易*/
  int *pnBytesFreed;            /* If not NULL, increment this in DbFree() 若不为空，将其加入函数DbFree()中*/

#ifdef SQLITE_ENABLE_UNLOCK_NOTIFY
  /* The following variables are all protected by the STATIC_MASTER //以下变量是由STATIC_MASTER互斥保护的，而不是sqlite3.mutex。
  ** mutex, not by sqlite3.mutex. They are used by code in notify.c. //它们被notify.c中的代码所使用
  **
  ** When X.pUnlockConnection==Y, that means that X is waiting for Y to
  ** unlock so that it can proceed.//若X.pUnlockConnection==Y,意味着X正在等待Y解锁，以便它可以继续执行
  **
  ** When X.pBlockingConnection==Y, that means that something that X tried
  ** tried to do recently failed with an SQLITE_LOCKED error due to locks
  ** held by Y.//当X.pBlockingConnection==Y,这说明X最近试图产生的动作因为Y所持有的所未被释放而引发的SQLITE_LOCKED错误而失败
  */
  sqlite3 *pBlockingConnection; /* Connection that caused SQLITE_LOCKED 引发SQLITE_LOCKED链接*/
  sqlite3 *pUnlockConnection;           /* Connection to watch for unlock 等待解锁的链接*/
  void *pUnlockArg;                     /* Argument to xUnlockNotify ，xUnlockNotify的参数*/
  void (*xUnlockNotify)(void **, int);  /* Unlock notify callback 解锁通知回调*/
  sqlite3 *pNextBlocked;        /* Next in list of all blocked connections 加锁链接列表中的下一个*/
#endif
};

/*
** A macro to discover the encoding of a database. 检测数据库编码的宏
*/
#define ENC(db) ((db)->aDb[0].pSchema->enc)

/*
** Possible values for the sqlite3.flags.    sqlite3.flags的可能的值
*/
#define SQLITE_VdbeTrace      0x00000100  /* True to trace VDBE execution 跟踪VDBE的执行*/
#define SQLITE_InternChanges  0x00000200  /* Uncommitted Hash table changes 未提交的哈希表的更改*/
#define SQLITE_FullColNames   0x00000400  /* Show full column names on SELECT 在select操作时显示列全名*/
#define SQLITE_ShortColNames  0x00000800  /* Show short columns names 显示列短名*/
#define SQLITE_CountRows      0x00001000  /* Count rows changed by INSERT, */
                                          /*   DELETE, or UPDATE and return */
                                          /*   the count using a callback. 计算因为增删改而变化的行数，并通过回调返回数值*/
#define SQLITE_NullCallback   0x00002000  /* Invoke the callback once if the */
                                          /*   result set is empty 若结果集合为空，调用一次回滚*/
#define SQLITE_SqlTrace       0x00004000  /* Debug print SQL as it executes 当SQL执行时将其调试打印*/
#define SQLITE_VdbeListing    0x00008000  /* Debug listings of VDBE programs，VDBE程序的调试列表 */
#define SQLITE_WriteSchema    0x00010000  /* OK to update SQLITE_MASTER 可以更新SQLITE_MASTER*/
                         /*   0x00020000  Unused */
#define SQLITE_IgnoreChecks   0x00040000  /* Do not enforce check constraints 忽略强制检查约束*/
#define SQLITE_ReadUncommitted 0x0080000  /* For shared-cache mode 对于共享缓存模式*/
#define SQLITE_LegacyFileFmt  0x00100000  /* Create new databases in format 1 创建格式1的新数据库*/
#define SQLITE_FullFSync      0x00200000  /* Use full fsync on the backend 在后端使用全fsync(fsync函数同步内存中所有已修改的文件数据到储存设备).*/
#define SQLITE_CkptFullFSync  0x00400000  /* Use full fsync for checkpoint 对检查点使用全fsync*/
#define SQLITE_RecoveryMode   0x00800000  /* Ignore schema errors 忽略模式错误*/
#define SQLITE_ReverseOrder   0x01000000  /* Reverse unordered SELECTs 逆转无序查询*/
#define SQLITE_RecTriggers    0x02000000  /* Enable recursive triggers 启用递归触发器*/
#define SQLITE_ForeignKeys    0x04000000  /* Enforce foreign key constraints  执行外键约束*/
#define SQLITE_AutoIndex      0x08000000  /* Enable automatic indexes 启用自动索引*/
#define SQLITE_PreferBuiltin  0x10000000  /* Preference to built-in funcs 内置函数优先*/
#define SQLITE_LoadExtension  0x20000000  /* Enable load_extension 启用load_extension*/
#define SQLITE_EnableTrigger  0x40000000  /* True to enable triggers 为真以便于启用触发器*/

/*
** Bits of the sqlite3.flags field that are used by the
** sqlite3_test_control(SQLITE_TESTCTRL_OPTIMIZATIONS,...) interface.
** These must be the low-order bits of the flags field.
** sqlite3_test_control(SQLITE_TESTCTRL_OPTIMIZATIONS,...)接口所使用的sqlite3.flags字段，必须是这些标志字段的低位信息
*/
#define SQLITE_QueryFlattener 0x01        /* Disable query flattening 关闭查询扁平化*/
#define SQLITE_ColumnCache    0x02        /* Disable the column cache 禁用列缓存*/
#define SQLITE_IndexSort      0x04        /* Disable indexes for sorting 禁用索引排序*/
#define SQLITE_IndexSearch    0x08        /* Disable indexes for searching 禁用索引搜索*/
#define SQLITE_IndexCover     0x10        /* Disable index covering table 禁用索引覆盖表*/
#define SQLITE_GroupByOrder   0x20        /* Disable GROUPBY cover of ORDERBY  禁止ORDERBY覆盖GROUPB*/
#define SQLITE_FactorOutConst 0x40        /* Disable factoring out constants 禁止分解出常数*/
#define SQLITE_IdxRealAsInt   0x80        /* Store REAL as INT in indices 将REAL存储为下标中的INT*/
#define SQLITE_DistinctOpt    0x80        /* DISTINCT using indexes 下标不允许重复*/
#define SQLITE_OptMask        0xff        /* Mask of all disablable opts 屏蔽所有禁止项*/

/*
** Possible values for the sqlite.magic field.
** The numbers are obtained at random and have no special meaning, other
** than being distinct from one another.
**sqlite.magic 字段的值是随机获得的，除了它们彼此各不相同之外，没有其他特别的含义
*/
#define SQLITE_MAGIC_OPEN     0xa029a697  /* Database is open 数据库是打开的*/
#define SQLITE_MAGIC_CLOSED   0x9f3c2d33  /* Database is closed 数据库是关闭的*/
#define SQLITE_MAGIC_SICK     0x4b771290  /* Error and awaiting close 错误并等待关闭*/
#define SQLITE_MAGIC_BUSY     0xf03b7906  /* Database currently in use 当前正在使用的数据库*/
#define SQLITE_MAGIC_ERROR    0xb5357930  /* An SQLITE_MISUSE error occurred 出现一个数据库滥用错误*/
#define SQLITE_MAGIC_ZOMBIE   0x64cffc7f  /* Close with last statement close 最后一条语句结束即关闭*/

/*
** Each SQL function is defined by an instance of the following
** structure.  A pointer to this structure is stored in the sqlite.aFunc
** hash table.  When multiple functions have the same name, the hash table
** points to a linked list of these structures.
**每个SQL函数都是由以下结构体的一个实例来定义。指向以下结构的指针被存储在sqlite.aFunc哈希表中。当有多个函数重名的时候，哈希表指向的是这些结构体的一个链接列表。
*/
struct FuncDef {
  i16 nArg;            /* Number of arguments.  -1 means unlimited 参数的数量，1表示无限制*/
  u8 iPrefEnc;         /* Preferred text encoding (SQLITE_UTF8, 16LE, 16BE) 所选择的文本编码方式*/
  u8 flags;            /* Some combination of SQLITE_FUNC_* ， SQLITE_FUNC_*的某种组合*/
  void *pUserData;     /* User data parameter 用户数据参数*/
  FuncDef *pNext;      /* Next function with same name 重名的下一个函数*/
  void (*xFunc)(sqlite3_context*,int,sqlite3_value**); /* Regular function 常规功能*/
  void (*xStep)(sqlite3_context*,int,sqlite3_value**); /* Aggregate step 总步*/
  void (*xFinalize)(sqlite3_context*);                /* Aggregate finalizer 总终结*/
  char *zName;         /* SQL name of the function. 函数的SQL名称*/
  FuncDef *pHash;      /* Next with a different name but the same hash 下一个不重名但是有相同哈希的函数*/
  FuncDestructor *pDestructor;   /* Reference counted destructor function 引用计数析构函数*/
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
** 这个结构体封装了一个用户功能的析构函数回滚和一个参照计数器.
**当create_function_v2（）被调用来利用一个析构函数创建一个函数，这个结构体类型的一个对象就会被分配.
**FuncDestructor.nRef的值设置为FuncDef对象所被创建的数量。每个新FuncDef对象的FuncDef.pDestructor构件被设置为指向已分配的FuncDestructor。
**
** Thereafter, when one of the FuncDef objects is deleted, the reference
** count on this object is decremented. When it reaches 0, the destructor
** is invoked and the FuncDestructor structure freed.
此后，当FuncDef的某一对象被删除时，该对象上的引用计数递减。当它到达0，析构函数被调用，FuncDestructor结构体被释放。
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
FuncDef.flags的可能的值。需要注意的是_length和_TYPEOF值必须与OPFLAG_LENGTHARG和OPFLAG_TYPEOFARG相对应。
在代码中有assert()语句来验证这一点。
*/
#define SQLITE_FUNC_LIKE     0x01 /* Candidate for the LIKE optimization LIKE优化的候选结果*/
#define SQLITE_FUNC_CASE     0x02 /* Case-sensitive LIKE-type function LIKE型功能区分大小写*/
#define SQLITE_FUNC_EPHEM    0x04 /* Ephemeral.  Delete with VDBE 暂时的，和VDBE一起删除*/
#define SQLITE_FUNC_NEEDCOLL 0x08 /* sqlite3GetFuncCollSeq() might be called ， sqlite3GetFuncCollSeq()函数很可能被调用*/
#define SQLITE_FUNC_COUNT    0x10 /* Built-in count(*) aggregate 内置计数聚合函数*/
#define SQLITE_FUNC_COALESCE 0x20 /* Built-in coalesce() or ifnull() function 内置coalesce()或ifnull()函数*/
#define SQLITE_FUNC_LENGTH   0x40 /* Built-in length() function 内置length()函数*/
#define SQLITE_FUNC_TYPEOF   0x80 /* Built-in typeof() function 内置typeof()函数*/

/*
** The following three macros, FUNCTION(), LIKEFUNC() and AGGREGATE() are
** used to create the initializers for the FuncDef structures.
  以下三个宏定义，FUNCTION(),LIKEFUNC()和AGGREGATE()被用于FuncDef结构体的初始化
**
**   FUNCTION(zName, nArg, iArg, bNC, xFunc)
**     Used to create a scalar function definition of a function zName 
**     implemented by C function xFunc that accepts nArg arguments. The
**     value passed as iArg is cast to a (void*) and made available
**     as the user-data (sqlite3_user_data()) for the function. If 
**     argument bNC is true, then the SQLITE_FUNC_NEEDCOLL flag is set.
**FUNCTION(zName, nArg, iArg, bNC, xFunc)创建了功能zName的标量函数定义，zName函数由接受nArg参数的C函数xFunc来实现.
**作为iArg参数被传递的值被转换成无类型，并且通过函数sqlite3_user_data()成为函数可以使用的用户数据.
**若参数bNC 为真，那么将会设置SQLITE_FUNC_NEEDCOLL标志的值.
**     AGGREGATE(zName, nArg, iArg, bNC, xStep, xFinal)
**     Used to create an aggregate function definition implemented by
**     the C functions xStep and xFinal. The first four parameters
**     are interpreted in the same way as the first 4 parameters to
**     FUNCTION().
**AGGREGATE(zName, nArg, iArg, bNC, xStep, xFinal)用于创建聚合函数定义，这个是由函数xStep和xFinal实现的.
**前四个参数将以相同的方式被解释成为FUNCTION()函数的前四个参数.
**
**
**   LIKEFUNC(zName, nArg, pArg, flags)
**     Used to create a scalar function definition of a function zName 
**     that accepts nArg arguments and is implemented by a call to C 
**     function likeFunc. Argument pArg is cast to a (void *) and made
**     available as the function user-data (sqlite3_user_data()). The
**     FuncDef.flags variable is set to the value passed as the flags
**     parameter.
**LIKEFUNC(zName, nArg, pArg, flags)用于创建函数zName的标量函数定义，zName接受参数nArg并且通过调用函数likeFunc来实现.
**参数pArg被转换成一个无类型数据，后通过函数sqlite3_user_data()转换成用户可利用的数据.
**FuncDef.flags变量被设置为传递的标志参数的值.
**
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
目前所有的保存点都存储在一个以sqlite3.pSavepoint开始的链接列表中.
在列表中的第一个元素是最近打开的保存点.保存点通过VDBE OP_Savepoint指令被添加到该列表.
*/
struct Savepoint {
  char *zName;                        /* Savepoint name (nul-terminated) 保存点名称（空终止）*/
  i64 nDeferredCons;                  /* Number of deferred fk violations 延迟的外键违规的数量*/
  Savepoint *pNext;                   /* Parent savepoint (if any) 父保存点（如果有的话）*/
};

/*
** The following are used as the second parameter to sqlite3Savepoint(),
** and as the P1 argument to the OP_Savepoint instruction.
<<<<<<< HEAD
** 下面的宏定义用作函数sqlite3Savepoint()的第2个参数，同时在操作码Savepoint中，P1要与它们作比较判断。
=======
**   以下三个变量将作为sqlite3Savepoint()函数的第二参数，并作为P1参数传递给OP_Savepoint指令。
>>>>>>> ba548246c0eb8783d5eca71d784e77030f3fe838
*/
#define SAVEPOINT_BEGIN      0
#define SAVEPOINT_RELEASE    1
#define SAVEPOINT_ROLLBACK   2


/*
** Each SQLite module (virtual table definition) is defined by an
** instance of the following structure, stored in the sqlite3.aModule
** hash table.
每个SQLite的模块（虚拟表定义）由下面的结构体的一个实例来定义，并存储在sqlite3.aModule哈希表中
*/
struct Module {
  const sqlite3_module *pModule;       /* Callback pointers 回滚指针*/
  const char *zName;                   /* Name passed to create_module() 传递给create_module()函数的名字*/
  void *pAux;                          /* pAux passed to create_module() 将pAux传递给create_module()函数*/
  void (*xDestroy)(void *);            /* Module destructor function 模块析构函数*/
};

/*
** information about each column of an SQL table is held in an instance
** of this structure.
一个SQL表的任何一个列信息都被保存在如下结构体类型的一个实例中
*/
struct Column {
  char *zName;     /* Name of this column 列名字*/
  Expr *pDflt;     /* Default value of this column 此列的默认值*/
  char *zDflt;     /* Original text of the default value 默认值的原始类型*/
  char *zType;     /* Data type for this column 此列的数据类型*/
  char *zColl;     /* Collating sequence.  If NULL, use the default 整理序列，如果为NULL，则使用缺省*/
  u8 notNull;      /* True if there is a NOT NULL constraint 若有一个非空约束的话，则为真*/
  u8 isPrimKey;    /* True if this column is part of the PRIMARY KEY 如果此列是主键的一部分，则为真*/
  char affinity;   /* One of the SQLITE_AFF_... values , SQLITE_AFF_...的其中一个值*/
#ifndef SQLITE_OMIT_VIRTUALTABLE
  u8 isHidden;     /* True if this column is 'hidden'  若此列被隐藏则为真*/
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
  char *zName;          /* Name of the collating sequence, UTF-8 encoded 排序序列名称，UTF-8编码*/
  u8 enc;               /* Text encoding handled by xCmp() 通过xCmp()函数处理文本编码*/
  void *pUser;          /* First argument to xCmp() xCmp()函数的首参*/
  int (*xCmp)(void*,int, const void*, int, const void*);
  void (*xDel)(void*);  /* Destructor for pUser , pUser的析构函数*/
};

/*
** A sort order can be either ASC or DESC.
 按照升序或者降序排列
*/
#define SQLITE_SO_ASC       0  /* Sort in ascending order 升序排列*/
#define SQLITE_SO_DESC      1  /* Sort in ascending order 升序排列*/

/*
** Column affinity types.
** 列关联类型。
**
** These used to have mnemonic name like 'i' for SQLITE_AFF_INTEGER and
** 't' for SQLITE_AFF_TEXT.  But we can save a little space and improve
** the speed a little by numbering the values consecutively.
** 这些定义曾也有助记的名字，就像'i'对于SQLITE_AFF_INTEGER，'t'对于SQLITE_AFF_TEXT.
** 但是，我们可以通过将这些值进行连续地编号节省一点空间，提高一点速度.
**
** But rather than start with 0 or 1, we begin with 'a'.  That way,
** when multiple affinity types are concatenated into a string and
** used as the P4 operand, they will be more readable.
** 但是,我们以'a'开始编号的，而不是先从0或1。通过这种方式，当多个关联类型连接成一个字符串，并作为P4操作数，他们会更易读
** 还需要注意的是数字类型被分组在一起，所以对于一个数字类型的测试只是一个单一的比较.
**
** Note also that the numeric types are grouped together so that testing
** for a numeric type is a single comparison.
**还需要注意的是数字类型被分组在一起，所以对于一个数字类型的测试只是一个单一的比较.
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
** SQLITE_AFF_MASK的值屏蔽了一个近似值的有效位
*/
#define SQLITE_AFF_MASK     0x67

/*
** Additional bit values that can be ORed with an affinity without
** changing the affinity.
** 可以与近似值进行或运算，而不改变近似值
*/
#define SQLITE_JUMPIFNULL   0x08  /* jumps if either operand is NULL 
								  ** 若操作数为空则跳转
								  */
#define SQLITE_STOREP2      0x10  /* Store result in reg[P2] rather than jump */
								  ** 将结果保存到reg[P2]而不是跳转
								  */
#define SQLITE_NULLEQ       0x80  /* NULL=NULL */

/*
** An object of this type is created for each virtual table present in
** the database schema.
**为数据库模式中存在的每一个虚拟表创建一个该类型的对象
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
**若数据库模式是共享的，那么使用这个共享模式的每一个数据库链接都会有一个这个结构体类型的实例.
**这是因为，每个数据库连接需要其自己的唯一sqlite3_vtab*手柄的实例来访问虚拟表的实现.
**sqlite3_vtab*手柄不能被数据库连接之间共享，即使当存储器内的数据库模式的其余部分是共享的.因为在内部初始化的时候，通过xConnect() 或 xCreate()将数据库链接句柄传递给其实现
**这个数据库链接句柄或许将会被虚拟表的实现所用，用于访问在数据库中的实表.
**所以他们将会作为调用者事务处理的一部分，这些访问需要通过相同的数据库连接来实现，就像在虚拟表上执行SQL操作一样
**
**
** All VTable objects that correspond to a single table in a shared
** database schema are initially stored in a linked-list pointed to by
** the Table.pVTable member variable of the corresponding Table object.
** When an sqlite3_prepare() operation is required to access the virtual
** table, it searches the list for the VTable that corresponds to the
** database connection doing the preparing so as to use the correct
** sqlite3_vtab* handle in the compiled query.
**在一个共享数据库模式中对应于单个表的所有VTable对象开始都被储存在由相应表对象的成员变量 Table.pVTable所指向的一个链接表中.
**当一个sqlite3_prepare()操作需要访问虚拟表，它会查找与数据库链接相对应的的VTable的列表，为在编译查询时使用正确的sqlite3_vtab*句柄做准备
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
**当在内存中的表对象被删除（例如，当模式由于某种原因被重新加载），则虚函数表的对象不会被立刻删除，sqlite3_vtab*句柄也不会立即断开链接.
**相反，它们会从Table.pVTable列表移动到另一个由相应的sqlite3结构体的sqlite3.pDisconnect成员开头的链接列表中.
**然后它们会被sqlite3*执行的一条语句在下一次删除或者是断开链接,这样做是为了避免涉及多个sqlite3.mutex互斥死锁问题.
**请参考上面的函数sqlite3VtabUnlockList（）评论作出解释，为什么它是安全的,不持有相应sqlite3.mutex互斥条目添加到列表sqlite3.pDisconnec.
**
** The memory for objects of this type is always allocated by 
** sqlite3DbMalloc(), using the connection handle stored in VTable.db as 
** the first argument.
**这个类型的对象的内存总是通过存储在VTable.db中作为首参的链接句柄通过sqlite3DbMalloc()函数进行分配.
**
*/
struct VTable {
  sqlite3 *db;              /* Database connection associated with this table 与此表相关的数据库链接*/
  Module *pMod;             /* Pointer to module implementation 指向模块实现的指针*/
  sqlite3_vtab *pVtab;      /* Pointer to vtab instance 指向vtab实例的指针*/
  int nRef;                 /* Number of pointers to this structure 指向这个结构体的指针的数量*/
  u8 bConstraint;           /* True if constraints are supported 如果约束支持的话即为真*/
  int iSavepoint;           /* Depth of the SAVEPOINT stack 保存点栈的深度*/
  VTable *pNext;            /* Next in linked list (see above)  链接列表中的下一个*/
};

/*
** Each SQL table is represented in memory by an instance of the
** following structure.
**每个SQL表由以下结构的一个实例表示在存储器中.
** Table.zName is the name of the table.  The case of the original
** CREATE TABLE statement is stored, but case is not significant for
** comparisons.
**Table.zName是这个表的名字.开始的创建表语句的实例被存储，但是比较而言这个实例并不是那样重要.
** Table.nCol is the number of columns in this table.  Table.aCol is a
** pointer to an array of Column structures, one for each column.
**Table.nCol是这个表里面列的数量.Table.aCol是一个指向为每一列所有的列结构体数组的指针.
** If the table has an INTEGER PRIMARY KEY, then Table.iPKey is the index of
** the column that is that key.   Otherwise Table.iPKey is negative.  Note
** that the datatype of the PRIMARY KEY must be INTEGER for this field to
** be set.  An INTEGER PRIMARY KEY is used as the rowid for each row of
** the table.  If a table has no INTEGER PRIMARY KEY, then a random rowid
** is generated for each row of the table.  TF_HasPrimaryKey is set if
** the table has any PRIMARY KEY, INTEGER or otherwise.
**如果表中有一个整型主键，那么Table.iPKey是该键所在列的索引。否则Table.iPKey为负.
**需要注意的是为了这个字段的设置，主键的数据类型必须为整型.
**主键被用来作为表的每一行的标志.
**若表没有整型的主键，则为表的每一行随机产生一个随机的行标.
**若表没有整型或者其它类型的任何主键，则TF_HasPrimaryKey将会被设置.
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
**Table.tnum表的数据库文件中的B树的根页面的页码.
**如果Table.iD是sqlite.aDb[]中数据库表后端的索引.0是对于主数据库,1是用于保存临时表和索引文件.
**如果TF_Ephemeral被设置，那么该表将被存储在当VDBE光标移动到表被关闭时，将会被系统自动删除的文件中.
**在这种情况下，Table.tnum指的是持有打开表权限的VDBE的光标号，而不是根页号.
**暂存表存储的是出现的子查询的结果，而不是在 select...from语句中所出现的真实的表名.
**
**
*/
struct Table {
  char *zName;         /* Name of the table or view 表或者视图的名字*/
  int iPKey;           /* If not negative, use aCol[iPKey] as the primary key 若不为负，用aCol[iPKey]作为主键*/
  int nCol;            /* Number of columns in this table 表的列号*/
  Column *aCol;        /* Information about each column 每一列的信息*/
  Index *pIndex;       /* List of SQL indexes on this table. 表中的SQL下标列表*/
  int tnum;            /* Root BTree node for this table (see note above) 表的B树的根节点*/
  tRowcnt nRowEst;     /* Estimated rows in table - from sqlite_stat1 table 估计在表中的行数- 从sqlite_stat1表*/
  Select *pSelect;     /* NULL for tables.  Points to definition if a view. 若是表则为空，若是一个视图则指向定义*/
  u16 nRef;            /* Number of pointers to this Table 指向这个表的指针数目*/
  u8 tabFlags;         /* Mask of TF_* values 屏蔽TF_*的值*/
  u8 keyConf;          /* What to do in case of uniqueness conflict on iPKey 若在iPKey上存在唯一性冲突的情况将做什么*/
  FKey *pFKey;         /* Linked list of all foreign keys in this table 此表的所有外键的链接列表*/
  char *zColAff;       /* String defining the affinity of each column 每一列近似值的字符串定义*/
#ifndef SQLITE_OMIT_CHECK
  ExprList *pCheck;    /* All CHECK constraints 所有的检查约束*/
#endif
#ifndef SQLITE_OMIT_ALTERTABLE
  int addColOffset;    /* Offset in CREATE TABLE stmt to add a new column 在创建表语句中添加一列的偏移量*/
#endif
#ifndef SQLITE_OMIT_VIRTUALTABLE
  VTable *pVTable;     /* List of VTable objects. VTable对象列表*/
  int nModuleArg;      /* Number of arguments to the module  模块参数的数目*/
  char **azModuleArg;  /* Text of all module args. [0] is module name 所有模块参数的文本，[0]是模块的名字/
#endif
  Trigger *pTrigger;   /* List of triggers stored in pSchema 存储在pSchema中的触发器列表*/
  Schema *pSchema;     /* Schema that contains this table 包含此表的模式*/
  Table *pNextZombie;  /* Next on the Parse.pZombieTab list 在Parse.pZombieTab列表的下一个*/
};

/*
** Allowed values for Tabe.tabFlags.
**Tabe.tabFlags的可能值
*/
#define TF_Readonly        0x01    /* Read-only system table 只读系统表*/
#define TF_Ephemeral       0x02    /* An ephemeral table 一个短暂的表*/
#define TF_HasPrimaryKey   0x04    /* Table has a primary key 有主键的表*/
#define TF_Autoincrement   0x08    /* Integer primary key is autoincrement 自增的整型主键*/
#define TF_Virtual         0x10    /* Is a virtual table 是一个虚表*/


/*
** Test to see whether or not a table is a virtual table.  This is
** done as a macro so that it will be optimized out when virtual
** table support is omitted from the build.
**测试该表是否是一个虚表
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
**每一个外键约束都是如下结构体的一个实例.
** A foreign key is associated with two tables.  The "from" table is
** the table that contains the REFERENCES clause that creates the foreign
** key.  The "to" table is the table that is named in the REFERENCES clause.
**外键与两个表关联.“从”表是创建外键并且包含REFERENCES子句的表。在“到”表是在references子句中命名的表.
**
** Consider this example:
**
**     CREATE TABLE ex1(
**       a INTEGER PRIMARY KEY,
**       b INTEGER CONSTRAINT fk1 REFERENCES ex2(x)
**     );
**
** For foreign key "fk1", the from-table is "ex1" and the to-table is "ex2".
**对于外键"fk1"，"ex1"是从表，"ex2"是到表.
** Each REFERENCES clause generates an instance of the following structure
** which is attached to the from-table.  The to-table need not exist when
** the from-table is created.  The existence of the to-table is not checked.
**每一个REFERENCES字句都会产生一个附属于从表的如下结构体的一个实例.
**当从表被创建的时候到表是不需要存在的，不检查到表的存在性.
**
*/
struct FKey {
  Table *pFrom;     /* Table containing the REFERENCES clause (aka: Child) 包含REFERENCES子句的表*/
  FKey *pNextFrom;  /* Next foreign key in pFrom 在pFrom中的下一个外键*/
  char *zTo;        /* Name of table that the key points to (aka: Parent) 键所指向的表名*/
  FKey *pNextTo;    /* Next foreign key on table named zTo 在表zTo上的下一个外键*/
  FKey *pPrevTo;    /* Previous foreign key on table named zTo 表zTo上的先前的外键*/
  int nCol;         /* Number of columns in this key 在此键上的列的数目*/
  /* EV: R-30323-21917 */
  u8 isDeferred;    /* True if constraint checking is deferred till COMMIT 如果约束检查被推迟到COMMIT，则为真*/
  u8 aAction[2];          /* ON DELETE and ON UPDATE actions, respectively 分别为ON DELETE，ON UPDATE操作*/
  Trigger *apTrigger[2];  /* Triggers for aAction[] actions , aAction[]操作的触发器*/
  struct sColMap {  /* Mapping of columns in pFrom to columns in zTo 从表pFrom到表zTo的列映射*/
    int iFrom;         /* Index of column in pFrom 表pFrom中的列索引*/
    char *zCol;        /* Name of column in zTo.  If 0 use PRIMARY KEY 表zTo中列的名字，若为0则使用主键*/
  } aCol[1];        /* One entry for each of nCol column s 对每个nCol中列加一*/
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
**SQLite支持许多不同的方法来处理约束错误.
**回滚处理意味着违反约束导致在进程失败的操作和当前事务被回滚.
**ABORT操作是指正在处理中的操作失败，并且该操作之前的所有更改均回滚，但是事务不回滚.
**FAIL处理是指正在进行的操作停止，并返回错误代码.
**但由于同样的操作而造成的之前的更改都不会改变，没有发生回滚.
**IGNORE是指导致约束错误的特定行不被插入或更新。处理继续，并且不返回错误.
**REPLACE是指违背唯一性约束的在数据库中先前存在的行被删除，从而新的插入或者更新操作可以进行.
**操作继续，不报告错误.
**
** RESTRICT, SETNULL, and CASCADE actions apply only to foreign keys.
** RESTRICT is the same as ABORT for IMMEDIATE foreign keys and the
** same as ROLLBACK for DEFERRED keys.  SETNULL means that the foreign
** key is set to NULL.  CASCADE means that a DELETE or UPDATE of the
** referenced table row is propagated into the row that holds the
** foreign key.
**RESTRICT, SETNULL, 以及CASCADE操作是适用于外键.
**RESTRICT就像是ABORT对于IMMEDIATE外键，以及ROLLBACK对于DEFERRED键.
**SETNULL是指外键被置为空.级联意味着所涉及到的表行的删除或更新操同时影响到包含外键的行.
**
** The following symbolic values are used to record which type
** of action to take.
**下面的符号值是用来记录所采取的行动的类型
*/
#define OE_None     0   /* There is no constraint to check 没有约束需要检查*/
#define OE_Rollback 1   /* Fail the operation and rollback the transaction 操作失败，事务回滚*/
#define OE_Abort    2   /* Back out changes but do no rollback transaction 撤销更改，但是不回滚事务*/
#define OE_Fail     3   /* Stop the operation but leave all prior changes 停止操作，保持先前更改*/
#define OE_Ignore   4   /* Ignore the error. Do not do the INSERT or UPDATE 忽略错误，不做插入或者更新*/
#define OE_Replace  5   /* Delete existing record, then do INSERT or UPDATE 删除存在的记录，然后执行插入或者更新操作*/

#define OE_Restrict 6   /* OE_Abort for IMMEDIATE, OE_Rollback for DEFERRED , OE_Abort针对IMMEDIAT型，OE_Rollback针对DEFERRED型*/
#define OE_SetNull  7   /* Set the foreign key value to NULL 将外键的值置为空*/
#define OE_SetDflt  8   /* Set the foreign key value to its default 将外键设置为默认值*/
#define OE_Cascade  9   /* Cascade the changes 级联更改*/

#define OE_Default  99  /* Do whatever the default action is 执行任何默认操作*/


/*
** An instance of the following structure is passed as the first
** argument to sqlite3VdbeKeyCompare and is used to control the
** comparison of the two index keys.
** 以下结构体的一个实例是通过作为首参传递给sqlite3VdbeKeyCompare函数，用于控制这两个索引关键字的比较。
*/
struct KeyInfo {
  sqlite3 *db;        /* The database connection  数据库链接*/
  u8 enc;             /* Text encoding - one of the SQLITE_UTF* values 文本编码-SQLITE_UTF*的其中一个值*/
  u16 nField;         /* Number of entries in aColl[] 加入aColl[]中的数目*/
  u8 *aSortOrder;     /* Sort order for each column.  May be NULL 每列的排序顺序，可能为空*/
  CollSeq *aColl[1];  /* Collating sequence for each term of the key 每一个键的术语的整体序列*/
};

/*
** An instance of the following structure holds information about a
** single index record that has already been parsed out into individual
** values.
**以下结构的一个实例包含有关已被解析出到单个值的单个索引记录的信息.
**
** A record is an object that contains one or more fields of data.
** Records are used to store the content of a table row and to store
** the key of an index.  A blob encoding of a record is created by
** the OP_MakeRecord opcode of the VDBE and is disassembled by the
** OP_Column opcode.
**一个记录是一个包含数据的一个或多个字段的对象.
**记录用于存储一个表行的内容，并存储索引的键
**对于记录的二进制大对象的编码是由VDBE的OP_MakeRecord opcode创建的，并且由OP_Column opcode拆解.
**
** This structure holds a record that has already been disassembled
** into its constituent fields.
**此结构用于保存已被分解成其组成的字段的记录.
*/
struct UnpackedRecord {
  KeyInfo *pKeyInfo;  /* Collation and sort-order information 整理顺序和排序顺序信息*/
  u16 nField;         /* Number of entries in apMem[] , apMem[]中存在的数目*/
  u8 flags;           /* Boolean settings.  UNPACKED_... below 布尔类型的设置信息*/
  i64 rowid;          /* Used by UNPACKED_PREFIX_SEARCH , 为UNPACKED_PREFIX_SEARCH所使用*/
  Mem *aMem;          /* Values  值*/
};

/*
** Allowed values of UnpackedRecord.flags
**UnpackedRecord.flags 所允许的值.
*/
#define UNPACKED_INCRKEY       0x01  /* Make this key an epsilon larger 使这个关键字是一个更大的小量*/
#define UNPACKED_PREFIX_MATCH  0x02  /* A prefix match is considered OK 前缀匹配合法*/
#define UNPACKED_PREFIX_SEARCH 0x04  /* Ignore final (rowid) field 忽略最终的行字段*/

/*
** Each SQL index is represented in memory by an
** instance of the following structure.
**每个SQL索引由下面结构体的一个实例表示在存储器中.
** The columns of the table that are to be indexed are described
** by the aiColumn[] field of this structure.  For example, suppose
** we have the following table and index:
**表中的要被索引的列由这种结构的aiColumn[]字段表示.
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
**在所描述的表结构Ex1中，nCol的值为3，因为在该表中存在三列.
**在所描述的索引结构Ex2中,当Ex1中三列中由两列被索引了之后，nColumn的值为2.
**aiColumn数组的值为{2, 0}. aiColumn[0]的值为2，因为被索引的第一列c3在Ex1.aCol[]中的下标为2.
**被索引的第二列c1在Ex1.aCol[]中的下标为0，所以Ex2.aiColumn[1]的值为0.
**
**
** The Index.onError field determines whether or not the indexed columns
** must be unique and what to do if they are not.  When Index.onError=OE_None,
** it means this is not a unique index.  Otherwise it is a unique index
** and the value of Index.onError indicate the which conflict resolution 
** algorithm to employ whenever an attempt is made to insert a non-unique
** element.
**该Index.onError字段确定索引列是否必须是唯一的，若不是，应该做什么处理.
**当Index.onError=OE_None,意味着不是一个唯一性索引.否则它就是一个唯一性索引
**Index.onError的值表明了当试图插入一个非唯一性元素的时候应该采取哪种冲突处理算法.
**
**
*/
struct Index {
  char *zName;     /* Name of this index 索引的名字*/
  int *aiColumn;   /* Which columns are used by this index.  1st is 0 通过该索引而被使用的列，0是第一个*/
  tRowcnt *aiRowEst; /* Result of ANALYZE: Est. rows selected by each column , ANALYZE的结果:Est. rows由每一列选择*/
  Table *pTable;   /* The SQL table being indexed 被索引的SQL表*/
  char *zColAff;   /* String defining the affinity of each column 为每一列的近似值做字符串定义*/
  Index *pNext;    /* The next index associated with the same table 用相同的表相关联的下一个索引*/
  Schema *pSchema; /* Schema containing this index 含有这种索引的模式*/
  u8 *aSortOrder;  /* Array of size Index.nColumn. True==DESC, False==ASC , Index.nColumn长度的数组，True==DESC, False==ASC*/
  char **azColl;   /* Array of collation sequence names for index 为索引名字进行排序的排序序列数组*/
  int nColumn;     /* Number of columns in the table used by this index 通过该索引使用的表的列数*/
  int tnum;        /* Page containing root of this index in database file 在数据库文件中包含该索引的根的页*/
  u8 onError;      /* OE_Abort, OE_Ignore, OE_Replace, or OE_None */
  u8 autoIndex;    /* True if is automatically created (ex: by UNIQUE) 若是系统自动创建则为真*/
  u8 bUnordered;   /* Use this index for == or IN queries only 对于==使用该索引，或者是仅仅对IN 查询使用*/
#ifdef SQLITE_ENABLE_STAT3
  int nSample;             /* Number of elements in aSample[] 数组aSample中的元素数目*/
  tRowcnt avgEq;           /* Average nEq value for key values not in aSample 不在aSample数组中的键值的平均nEq值*/
  IndexSample *aSample;    /* Samples of the left-most key 最左边键的值*/
#endif
};

/*
** Each sample stored in the sqlite_stat3 table is represented in memory 
** using a structure of this type.  See documentation at the top of the
** analyze.c source file for additional information.
**存储在sqlite_stat3表中的每个样本值均使用这种类型的结构表示在存储器中.
**更多内容参见analyze.c源文件中顶端的文件信息.
*/
struct IndexSample {
  union {
    char *z;        /* Value if eType is SQLITE_TEXT or SQLITE_BLOB */
    double r;       /* Value if eType is SQLITE_FLOAT */
    i64 i;          /* Value if eType is SQLITE_INTEGER */
  } u;
  u8 eType;         /* SQLITE_NULL, SQLITE_INTEGER ... etc. */
  int nByte;        /* Size in byte of text or blob. 文本或者是二进制大对象的字节长度*/
  tRowcnt nEq;      /* Est. number of rows where the key equals this sample 键与该样本相同的行的Est. number*/
  tRowcnt nLt;      /* Est. number of rows where key is less than this sample 键少于该样本的行的Est. number*/
  tRowcnt nDLt;     /* Est. number of distinct keys less than this sample 少于该样本的不重复的键的Est. number*/
};

/*
** Each token coming out of the lexer is an instance of
** this structure.  Tokens are also used as part of an expression.
**来源于词法分析器的每个记号都是这个结构体的一个实例。记号也作为表达式的一部分使用.
** Note if Token.z==0 then Token.dyn and Token.n are undefined and
** may contain random values.  Do not make any assumptions about Token.dyn
** and Token.n when Token.z==0.
**注意,若Token.z==0，那么Token.dyn和Token.n是未被定义的，并且很可能包含随机值.
**当 Token.z==0时，不要对Token.dyn和Token.n做任何假设
*/
struct Token {
  const char *z;     /* Text of the token.  Not NULL-terminated! 记号的文本，不是空终结*/
  unsigned int n;    /* Number of characters in this token 在该记号中所包含的字符的数目*/
};

/*
** An instance of this structure contains information needed to generate
** code for a SELECT that contains aggregate functions.
**这种结构体的一个实例包含了需要为选择语句生成代码的信息，这个选择中包含聚合函数.
** If Expr.op==TK_AGG_COLUMN or TK_AGG_FUNCTION then Expr.pAggInfo is a
** pointer to this structure.  The Expr.iColumn field is the index in
** AggInfo.aCol[] or AggInfo.aFunc[] of information needed to generate
** code for that node.
**若Expr.op==TK_AGG_COLUMN，或Expr.op==TK_AGG_FUNCTION，那么Expr.pAggInfo是指向该结构体的指针.
** Expr.iColumn字段是AggInfo.aCol[]或者AggInfo.aFunc[]中需要为该结点生成代码的信息的索引.
** AggInfo.pGroupBy and AggInfo.aFunc.pExpr point to fields within the
** original Select structure that describes the SELECT statement.  These
** fields do not need to be freed when deallocating the AggInfo structure.
** AggInfo.pGroupBy以及AggInfo.aFunc.pExpr指向描述选择语句，并包含在原始选择结构中的字段.
** 当AggInfo结构体被释放的时候，这些字段不需要被释放.
*/
struct AggInfo {
  u8 directMode;          /* Direct rendering mode means take data directly
                          ** from source tables rather than from accumulators */
  u8 useSortingIdx;       /* In direct mode, reference the sorting index rather
                          ** than the source table 在直接模式中，参考排序索引而不是源表*/
  int sortingIdx;         /* Cursor number of the sorting index 排序索引的游标号*/
  int sortingIdxPTab;     /* Cursor number of pseudo-table , pseudo-table的游标号*/
  int nSortingColumn;     /* Number of columns in the sorting index 排序索引中的列的数量*/
  ExprList *pGroupBy;     /* The group by clause , group by 子句*/
  struct AggInfo_col {    /* For each column used in source tables 对于在源表中使用的每一列*/
    Table *pTab;             /* Source table  源表*/
    int iTable;              /* Cursor number of the source table 源表的游标号*/
    int iColumn;             /* Column number within the source table 源表中所包含的游标数量*/
    int iSorterColumn;       /* Column number in the sorting index 在排序索引中的列的数量*/
    int iMem;                /* Memory location that acts as accumulator 充当累加器的存储器位置*/
    Expr *pExpr;             /* The original expression 原始表达式*/
  } *aCol;
  int nColumn;            /* Number of used entries in aCol[] , 数组aCol中被使用的数量*/
  int nAccumulator;       /* Number of columns that show through to the output.
                          ** Additional columns are used only as parameters to
                          ** aggregate functions 通过输出显示的列的数量，其他的列仅作为参数传递给聚合函数*/
  struct AggInfo_func {   /* For each aggregate function 对于每一个聚合函数*/
    Expr *pExpr;             /* Expression encoding the function 编码功能的表达式*/
    FuncDef *pFunc;          /* The aggregate function implementation 聚合函数的实现*/
    int iMem;                /* Memory location that acts as accumulator 充当累加器的存储器位置*/
    int iDistinct;           /* Ephemeral table used to enforce DISTINCT 用于执行DISTINCT操作的临时表*/
  } *aFunc;
  int nFunc;              /* Number of entries in aFunc[] 数组aFunc中的数量*/
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
**数据类型ynVar是16位或32位的有符号整数，通常是16位.
**但是如果SQLITE_MAX_VARIABLE_NUMBER大于32767，必须使用32位.
**16位是首选，因为在Expr对象中这种方式占用更少的内存，包含很多预处理语句的Expr对象是系统内存使用的大用户.
**没有应用程序需要10个或者20个以上的变量，但是有些极端用户需要超过32767个变量的预处理语句，并且在编译阶段它们的请求是可被允许的.
**
**
*/
#if SQLITE_MAX_VARIABLE_NUMBER<=32767
typedef i16 ynVar;
#else
typedef int ynVar;
#endif

/*
** Each node of an expression in the parse tree is an instance			在分析数中表达式的每个节点是该结构的一个实例
** of this structure.
**
** Expr.op is the opcode. The integer parser token codes are reused		Expr.op是操作码。整数解析器表示在这里代码重用为操作码
** as opcodes here. For example, the parser defines TK_GE to be an integer	例如，解析器定义整数代码TK_GE代表>=操作符
** code representing the ">=" operator. This same integer code is reused	相同的整数代码被重用来表示在表达式树中大于或等于操作数
** to represent the greater-than-or-equal-to operator in the expression
** tree.
**
** If the expression is an SQL literal (TK_INTEGER, TK_FLOAT, TK_BLOB, 		如果表达式是一个SQL文字(TK_INTEGER,TK_FLOAT,TK_BLOB或TK_STRING)
** or TK_STRING), then Expr.token contains the text of the SQL literal. If	那么Expr.token包含SQL文字的文本
** the expression is a variable (TK_VARIABLE), then Expr.token contains the 	如果表达式是一个变量(TK_VARIABLE)，那么，Expr.token包含了变量的名字
** variable name. Finally, if the expression is an SQL function (TK_FUNCTION),	最后，如果该表达式是一个SQL函数(TK_FUNCTION)，那么Expr.token包含了函数的名称
** then Expr.token contains the name of the function.
**
** Expr.pRight and Expr.pLeft are the left and right subexpressions of a	Expr.pRight和Expr.pLeft是一个二元运算符左边和右边的子表达式
** binary operator. Either or both may be NULL.					一个或者两个都可以为空
**
** Expr.x.pList is a list of arguments if the expression is an SQL function,	如果 表达式是一个SQL函数，Expr.x.pList是参数列表
** a CASE expression or an IN expression of the form "<lhs> IN (<y>, <z>...)".	一个CASE表达式或者"<lhs>IN(<y>,<z>...)"形式的IN表达式
** Expr.x.pSelect is used if the expression is a sub-select or an expression of	如果表达式是一个子选择或者"<lhs>IN(<y>,<z>...)"形式的表达式，Expr.x.pSelect会被使用
** the form "<lhs> IN (SELECT ...)". If the EP_xIsSelect bit is set in the	如果Expr.x.pSelect位在Expr.flags被隐秘设置，那么Expr.x.pSelect是有效的
** Expr.flags mask, then Expr.x.pSelect is valid. Otherwise, Expr.x.pList is 	否则，Expr.x.pList是有效的
** valid.
**
** An expression of the form ID or ID.ID refers to a column in a table.		一个表达式表单的ID或ID。ID指表中的一列。
** For such expressions, Expr.op is set to TK_COLUMN and Expr.iTable is		对这样的表达式，Expr.op设置TK_COLUMN,Expr.iTable是VDBE光标指向该表的整数光标号，
** the integer cursor number of a VDBE cursor pointing to that table and	Expr.iTable是VDBE光标指向该表的整数光标号，
** Expr.iColumn is the column number for the specific column.  If the		Expr.iCloumn是特定列的列数
** expression is used as a result in an aggregate SELECT, then the		如果表达式用作一个聚合的SELECT的结果，
** value is also stored in the Expr.iAgg column in the aggregate so that	那么该值也会被存储在聚合的Expr.iAgg中，
** it can be accessed after all aggregates are computed.			以便它可以在所有聚合体被计算之后访问
**
** If the expression is an unbound variable marker (a question mark 		如果表达式是一个未绑定变量的标记(在原始的SQL中一个问号字符'?')
** character '?' in the original SQL) then the Expr.iTable holds the index 	那么Expr.iTable持有该变量的索引。
** number for that variable.
**
** If the expression is a subquery then Expr.iColumn holds an integer		如果表达式是一个子查询，
** register number containing the result of the subquery.  If the		Expr.iColumn记录包含子查询结果的整数寄存器号
** subquery gives a constant result, then iTable is -1.  If the subquery	如果子查询提供了一个恒定的结果，那么iTable为-1
** gives a different answer at different times during statement processing	如果子查询在语句处理过程中不同时间的结果不一样，
** then iTable is the address of a subroutine that computes the subquery.	那么iTable是计算子查询的子程序的地址
**
** If the Expr is of type OP_Column, and the table it is selecting from		如果Expr是OP_Column类型的，
** is a disk table or the "old.*" pseudo-table, then pTab points to the		表是从一个磁盘表或"old.*"伪表中选择出来的
** corresponding table definition.						那么pTab是指向相应表的定义
**
** ALLOCATION NOTES:								分配注意
**
** Expr objects can use a lot of memory space in database schema.  To		Expr对象可以在数据库架构时使用大量的内存空间
** help reduce memory requirements, sometimes an Expr object will be		为了帮助减少内存需求，有时一个Expr对象会被截断
** truncated.  And to reduce the number of memory allocations, sometimes	为了减少存储器分配的数量，
** two or more Expr objects will be stored in a single memory allocation,	有时两个或更多的Expr对象将被存储在一个内存单元
** together with Expr.zToken strings.						连同Expr.zToken字符串
**
** If the EP_Reduced and EP_TokenOnly flags are set when			当Expr对象被截断EP_Reduced和EP_ToKenOnly标志被设置
** an Expr object is truncated.  When EP_Reduced is set, then all		当EP_Reduced设置时，
** the child Expr objects in the Expr.pLeft and Expr.pRight subtrees		Expr.pLeft和Expr.pRight子树的所有子Expr对象在相同的内存单元中
** are contained within the same memory allocation.  Note, however, that	注意，无论EP_Reduced是否设置，Expr.x.pList和Expr.x.PSelect子树总是单独分配
** the subtrees in Expr.x.pList or Expr.x.pSelect are always separately
** allocated, regardless of whether or not EP_Reduced is set.
*/
struct Expr {
  u8 op;                 /* Operation performed by this node 			操作由该节点进行*/
  char affinity;         /* The affinity of the column or 0 if not a colum	 */
  u16 flags;             /* Various flags.  EP_* See below 			 各种标志	EP_*参阅下文*/
  union {
    char *zToken;          /* Token value. Zero terminated and dequoted 	 标记值。零终止，未引用*/
    int iValue;            /* Non-negative integer value if EP_IntValu		 EP_IntValue非负整数值*/
  } u;

  /* If the EP_TokenOnly flag is set in the Expr.flags mask, then no		如果Expr.flags隐秘设置EP_TokenOnly标志，
  ** space is allocated for the fields below this point. An attempt to		则没有空间分配该这点下面的字段
  ** access them will result in a segfault or malfunction. 			试图访问它们将导致一个段错误或故障
  *********************************************************************/

  Expr *pLeft;           /* Left subnode 	左子节点*/
  Expr *pRight;          /* Right subnode 	右子节点*/
  union {
    ExprList *pList;     /* Function arguments or in "<expr> IN (<expr-list)" 	函数参数或者"<表达式>IN(<表达式列表>)*/
    Select *pSelect;     /* Used for sub-selects and "<expr> IN (<select>)" 	用于子选择和"<表达式>IN(<选择>)"*/
  } x;
  CollSeq *pColl;        /* The collation type of the column or 0 		列的整理类型或0*/

  /* If the EP_Reduced flag is set in the Expr.flags mask, then no		如果Expr.flags隐秘设置EP_Reduced
  ** space is allocated for the fields below this point. An attempt to		则没有空间被分配给这点下面的字段
  ** access them will result in a segfault or malfunction.			试图访问它们将导致一个段错误或故障
  *********************************************************************/

  int iTable;            /* TK_COLUMN: cursor number of table holding column	TK_COLUMN:表列持有光标数
                         ** TK_REGISTER: register number			TK_REGISTER:寄存器号码
                         ** TK_TRIGGER: 1 -> new, 0 -> old 			TK_TRIGGER:1->新,0->旧*/
  ynVar iColumn;         /* TK_COLUMN: column index.  -1 for rowid.		TK_COLUMN:列索引。-1为ROWID
                         ** TK_VARIABLE: variable number (always >= 1). 	TK_VARIABLE: 变量数(总是大于等于1)*/
  i16 iAgg;              /* Which entry in pAggInfo->aCol[] or ->aFunc*/
  i16 iRightJoinTable;   /* If EP_FromJoin, the right table of the join 	右表中的连接*/
  u8 flags2;             /* Second set of flags.  EP2_... 			第二组的标志*/
  u8 op2;                /* TK_REGISTER: original value of Expr.op		TK_REGISTER:Expr.op的原始值
                         ** TK_COLUMN: the value of p5 for OP_Column		TK_COLUMN: 对于OP_Column，P5的值
                         ** TK_AGG_FUNCTION: nesting depth 			TK_AGG_FUNCTION: 嵌套深度*/
  AggInfo *pAggInfo;     /* Used by TK_AGG_COLUMN and TK_AGG_FUNCTION 		TK_AGG_COLUMN和TK_AGG_FUNCTION使用*/
  Table *pTab;           /* Table for TK_COLUMN expressions. 			表达式TK_COLUMN的表*/
#if SQLITE_MAX_EXPR_DEPTH>0
  int nHeight;           /* Height of the tree headed by this node 		以此节点为根节点的数的高度*/
#endif
};

/*
** The following are the meanings of bits in the Expr.flags field.
*/
#define EP_FromJoin   0x0001  /* Originated in ON or USING clause of a join 	起源于连接的ON或USING语句*/
#define EP_Agg        0x0002  /* Contains one or more aggregate functions 	包含了一个或多个聚合函数*/
#define EP_Resolved   0x0004  /* IDs have been resolved to COLUMNs 		标识已经被解析到列*/
#define EP_Error      0x0008  /* Expression contains one or more errors 	表达式包含一个或多个错误*/
#define EP_Distinct   0x0010  /* Aggregate function with DISTINCT keyword 	有DISTINCT关键字的聚合函数*/
#define EP_VarSelect  0x0020  /* pSelect is correlated, not constant 		pSelect是相关的，不是持续的*/
#define EP_DblQuoted  0x0040  /* token.z was originally in "..." 		token.z源自于"..."*/
#define EP_InfixFunc  0x0080  /* True for an infix function: LIKE, GLOB, etc 	适合于中缀功能:LINKE,GLOB等*/
#define EP_ExpCollate 0x0100  /* Collating sequence specified explicitly 	排序序列明确规定*/
#define EP_FixedDest  0x0200  /* Result needed in a specific register 		结果需要保存在一个特定的寄存器*/
#define EP_IntValue   0x0400  /* Integer value contained in u.iValue 		包含在u.iValue的整数值*/
#define EP_xIsSelect  0x0800  /* x.pSelect is valid (otherwise x.pList is) 	x.pSelect是有效的(否则x.pList是)*/
#define EP_Hint       0x1000  /* Not used 					未使用*/
#define EP_Reduced    0x2000  /* Expr struct is EXPR_REDUCEDSIZE bytes only 	Expr结构体是唯一的EXPR_REDUCEDSIZE字节*/
#define EP_TokenOnly  0x4000  /* Expr struct is EXPR_TOKENONLYSIZE bytes only 	Expr结构体是唯一EXPR_TOKENONLYSIZE字节*/
#define EP_Static     0x8000  /* Held in memory not obtained from malloc() 	保存在内存中没有用malloc()获得*/

/*
** The following are the meanings of bits in the Expr.flags2 field.
*/
#define EP2_MallocedToken  0x0001  /* Need to sqlite3DbFree() Expr.zToken */
#define EP2_Irreducible    0x0002  /* Cannot EXPRDUP_REDUCE this Expr */

/*
** The pseudo-routine sqlite3ExprSetIrreducible sets the EP2_Irreducible	伪例程sqlite3ExprSetlrreducible通过表达式结构来设置EP2_Irreducible标志
** flag on an expression structure.  This flag is used for VV&A only.  The	该标志只用于VV&A
** routine is implemented as a macro that only works when in debugging mode,	例程被实现为一个只在调试模式下工作下工作的宏
** so as not to burden production code.						以免生产代码产生负担
*/
#ifdef SQLITE_DEBUG
# define ExprSetIrreducible(X)  (X)->flags2 |= EP2_Irreducible
#else
# define ExprSetIrreducible(X)
#endif

/*
** These macros can be used to test, set, or clear bits in the 			这些宏在Expr.flags字段可用于测试，置位或清零
** Expr.flags field.
*/
#define ExprHasProperty(E,P)     (((E)->flags&(P))==(P))
#define ExprHasAnyProperty(E,P)  (((E)->flags&(P))!=0)
#define ExprSetProperty(E,P)     (E)->flags|=(P)
#define ExprClearProperty(E,P)   (E)->flags&=~(P)

/*
** Macros to determine the number of bytes required by a normal Expr 		宏来确定一个普通Expr结构体的字节数
** struct, an Expr struct with the EP_Reduced flag set in Expr.flags 		有EP_Reduced标记设置Expr.flags的Expr结构体
** and an Expr struct with the EP_TokenOnly flag set.				EP_TokenOnly设置的结构体
*/
#define EXPR_FULLSIZE           sizeof(Expr)           /* Full size 		全部大小*/
#define EXPR_REDUCEDSIZE        offsetof(Expr,iTable)  /* Common features 	共同的特征*/
#define EXPR_TOKENONLYSIZE      offsetof(Expr,pLeft)   /* Fewer features 	较少的特征*/

/*
** Flags passed to the sqlite3ExprDup() function. See the header comment 	标志传递给sqlite3ExprDup()函数
** above sqlite3ExprDup() for details.						在sqlite3ExprDup()头部进行了详细的注释
*/
#define EXPRDUP_REDUCE         0x0001  /* Used reduced-size Expr nodes */

/*
** A list of expressions.  Each expression may optionally have a		表达式列表。每个表达式可以有一个名称
** name.  An expr/name combination can be used in several ways, such		一个表达式/名称组合可以用于几个方面
** as the list of "expr AS ID" fields following a "SELECT" or in the		比如在一个"SELECT"之后的"expr AS ID"列表
** list of "ID = expr" items in an UPDATE.  A list of expressions can		或在UPDATE中"ID=expr"列表
** also be used as the argument to a function, in which case the a.zName	表达式列表也可以被用作a.zName字段不被使用的函数的参数
** field is not used.
*/
struct ExprList {
  int nExpr;             /* Number of expressions on the list 			列表中的表达式数目*/
  int iECursor;          /* VDBE Cursor associated with this ExprList 		与ExprList相关的VDBE游标*/
  struct ExprList_item { /* For each expression in the list 			对于列表中的每个表达式*/
    Expr *pExpr;           /* The list of expressions 				表达式列表*/
    char *zName;           /* Token associated with this expression 		与此表达式相关的符号*/
    char *zSpan;           /* Original text of the expression 			原文的表达*/
    u8 sortOrder;          /* 1 for DESC or 0 for ASC 				1为降序或0为升序*/
    u8 done;               /* A flag to indicate when processing is finished 	一个指示何时处理完毕的标志*/
    u16 iOrderByCol;       /* For ORDER BY, column number in result set 	对于ORDER BY，结果集中的列数*/
    u16 iAlias;            /* Index into Parse.aAlias[] for zName 		zName在Parse.aAlias[]中的索引 */
  } *a;                  /* Alloc a power of two greater or equal to nExpr 	分配两个大于或等于nExpr空间*/
};

/*
** An instance of this structure is used by the parser to record both		解析器用这种结构的一个实例来记录
** the parse tree for an expression and the span of input text for an		表达式的解析树和表达式输入文本的跨度
** expression.
*/
struct ExprSpan {
  Expr *pExpr;          /* The expression parse tree 				表达式解析树*/
  const char *zStart;   /* First character of input text 			输入文本的第一个字符*/
  const char *zEnd;     /* One character past the end of input text 		输入文本的最后一个字符*/
};

/*
** An instance of this structure can hold a simple list of identifiers,		这种结构的一个实例可以容纳一个简单的标识符列表
** such as the list "a,b,c" in the following statements:			如在下面这些语句中的"a,b,c"列表:
**
**      INSERT INTO t(a,b,c) VALUES ...;
**      CREATE INDEX idx ON t(a,b,c);
**      CREATE TRIGGER trig BEFORE UPDATE ON t(a,b,c) ...;
**
** The IdList.a.idx field is used when the IdList represents the list of	表名在INSERT语句中IdList代表列名的列表时，IdList.a.idx被使用
** column names after a table name in an INSERT statement.  In the statement
**
**     INSERT INTO t(a,b,c) ...
**
** If "a" is the k-th column of table "t", then IdList.a[0].idx==k.		如果"a"是表"t"的第k列，那么IdList.a[0].idx==k
*/
struct IdList {
  struct IdList_item {
    char *zName;      /* Name of the identifier 				标识符的名称*/
    int idx;          /* Index in some Table.aCol[] of a column named zName 	在一些Table.aCol[]中列称为zName*/
  } *a;
  int nId;         /* Number of identifiers on the list 			列表中标识符的数目*/
};

/*
** The bitmask datatype defined below is used for various optimizations.	下面定义的位掩码数据类型被用于各种优化
**
** Changing this from a 64-bit to a 32-bit type limits the number of		从64位到32位的类型改变限制了连接到的表的个数是32不是64
** tables in a join to 32 instead of 64.  But it also reduces the size		但是也减少了库的大小到738字节在ix86上
** of the library by 738 bytes on ix86.
*/
typedef u64 Bitmask;

/*
** The number of bits in a Bitmask.  "BMS" means "BitMask Size".		位掩码的比特数，"BMS"表示"掩码大小"
*/
#define BMS  ((int)(sizeof(Bitmask)*8))

/*
** The following structure describes the FROM clause of a SELECT statement.	下面的结构描述了一个SELECT语句中的FORM子句
** Each table or subquery in the FROM clause is a separate element of		在FORM子句中的每个表或子查询是SrcList.a[].array的一个独立元素
** the SrcList.a[] array.
**
** With the addition of multiple database support, the following structure	在多个数据库的支持下，下面的结构也可以用来描述一个特定的表
** can also be used to describe a particular table such as the table that	如由一个INSERT,DELETE,UPDATE子句修改的表
** is modified by an INSERT, DELETE, or UPDATE statement.  In standard SQL,	在标准的SQL中，这样的表必须有一个简单的名字:ID
** such a table must be a simple name: ID.  But in SQLite, the table can	但是在SQLite中，表现在可以通过一个数据库名，一个点甚至表名来确认
** now be identified by a database name, a dot, then the table name: ID.ID.
**
** The jointype starts out showing the join type between the current table	jointype开始表明在列表中这个表和下个表的连接类型
** and the next table on the list.  The parser builds the list this way.	解析器用这种方式构建列表
** But sqlite3SrcListShiftJoinType() later shifts the jointypes so that each	但是sqlite3SrcListShiftJoinType()以后改变jointypes使得
** jointype expresses the join between the table and the previous table.	每个jointype表示这个表和上个表之间的联系
**
** In the colUsed field, the high-order bit (bit 63) is set if the table	在colUsed字段，如果表包含多于63列和64列或更高列被使用，高位(63位)被设置
** contains more than 63 columns and the 64-th or later column is used.
*/
struct SrcList {
  i16 nSrc;        /* Number of tables or subqueries in the FROM clause 	表或子查询的FORM子句数*/
  i16 nAlloc;      /* Number of entries allocated in a[] below 			分配在a[]下面的输入的数目*/
  struct SrcList_item {
    char *zDatabase;  /* Name of database holding this table 			持有这个表的数据库名称*/
    char *zName;      /* Name of the table 					表的名称*/
    char *zAlias;     /* The "B" part of a "A AS B" phrase.  zName is the "A" 	"A AS B"子句中"B"部分，zName是"A"*/
    Table *pTab;      /* An SQL table corresponding to zName 			对应zName的SQL表*/
    Select *pSelect;  /* A SELECT statement used in place of a table name 	代替表名使用SELECT语句*/
    int addrFillSub;  /* Address of subroutine to manifest a subquery 		实现子查询的子程序地址*/
    int regReturn;    /* Register holding return address of addrFillSub 	寄存器保存addrFillSub返回地址*/
    u8 jointype;      /* Type of join between this able and the previous 	这个表和之前表的的连接类型*/
    u8 notIndexed;    /* True if there is a NOT INDEXED clause 			是否有一个NOT INDEXED子句*/
    u8 isCorrelated;  /* True if sub-query is correlated 			是否相关子查询*/
#ifndef SQLITE_OMIT_EXPLAIN
    u8 iSelectId;     /* If pSelect!=0, the id of the sub-select in EQP 	如果pSelect!=0,EQP中子选择的id*/
#endif
    int iCursor;      /* The VDBE cursor number used to access this table 	用来访问此表的VDBE光标号*/
    Expr *pOn;        /* The ON clause of a join 				ON子句的加入*/
    IdList *pUsing;   /* The USING clause of a join 				USING子句的加入*/
    Bitmask colUsed;  /* Bit N (1<<N) set if column N of pTab is used 		设置位N(1<<N)如果表p的列N被使用*/
    char *zIndex;     /* Identifier from "INDEXED BY <zIndex>" clause 		"INDEXED BY<zIndex>"子句的识别码*/
    Index *pIndex;    /* Index structure corresponding to zIndex, if any 	对应于zIndex的索引结构*/
  } a[1];             /* One entry for each identifier on the list 		列表上每个标识符的一个条目*/
};

/*
** Permitted values of the SrcList.a.jointype field				SrcList.a.jointype字段的允许值
*/
#define JT_INNER     0x0001    /* Any kind of inner or cross join 		任何内部或交叉连接*/
#define JT_CROSS     0x0002    /* Explicit use of the CROSS keyword 		明确使用了CROSS关键字*/
#define JT_NATURAL   0x0004    /* True for a "natural" join 			真正的"自然"连接*/
#define JT_LEFT      0x0008    /* Left outer join 				左外连接*/
#define JT_RIGHT     0x0010    /* Right outer join 				右外连接*/
#define JT_OUTER     0x0020    /* The "OUTER" keyword is present 		存在"OUTER"关键字*/
#define JT_ERROR     0x0040    /* unknown or unsupported join type 		未知或不支持的连接类型*/


/*
** A WherePlan object holds information that describes a lookup			WherePlan对象包含如下信息，描述一个查找策略
** strategy.
**
** This object is intended to be opaque outside of the where.c module.		这个对象是在不透明的where.c模块外的。
** It is included here only so that that compiler will know how big it		它只包括这里所以编译器会知道它的大小
** is.  None of the fields in this object should be used outside of		没有在此对象中的字段应在where.c模块外使用
** the where.c module.
**
** Within the union, pIdx is only used when wsFlags&WHERE_INDEXED is true.	在连接中，pIdx仅用于当wsFlag和WHERE_INDEXED是真
** pTerm is only used when wsFlags&WHERE_MULTI_OR is true.  And pVtabIdx	pTerm仅用于当wsFlags和WHERE_MULTI_OR是真
** is only used when wsFlags&WHERE_VIRTUALTABLE is true.  It is never the	pVtabIdx仅用于当wsFlags和WHERE_VIRTUALTABLE是真
** case that more than one of these conditions is true.				永远不会多于一个条件为真
*/
struct WherePlan {
  u32 wsFlags;                   /* WHERE_* flags that describe the strategy 	描述战略的WHERE_*标志*/
  u32 nEq;                       /* Number of == constraints 			==的数量限制*/
  double nRow;                   /* Estimated number of rows (for EQP) 		估计的行数*/
  union {
    Index *pIdx;                   /* Index when WHERE_INDEXED is true 		当WHERE_INDEXED是真时的索引*/
    struct WhereTerm *pTerm;       /* WHERE clause term for OR-search 		OR搜索中WHERE子句术语*/
    sqlite3_index_info *pVtabIdx;  /* Virtual table index to use 		使用虚表索引*/
  } u;
};

/*
** For each nested loop in a WHERE clause implementation, the WhereInfo		对于在WHERE子句中实现的每个嵌套循环中，
** structure contains a single instance of this structure.  This structure	Whereinfo结构包含一个单一实例的结构
** is intended to be private to the where.c module and should not be		这个结构对where.c模块是私有的，
** access or modified by other modules.						不应该访问或通过其他模块进行修改
**
** The pIdxInfo field is used to help pick the best index on a			pIdxInfo字段用于帮助挑选最好的虚拟表索引
** virtual table.  The pIdxInfo pointer contains indexing			pIdxInfo指针包含FORM子句第i个表重新排序前的索引信息
** information for the i-th table in the FROM clause before reordering.	
** All the pIdxInfo pointers are freed by whereInfoFree() in where.c.		whereInfoFree()在where.c中时释放所有pIdxInfo指针	
** All other information in the i-th WhereLevel object for the i-th table	在FORM子句排序后第i个表的第i个WhereLevel对象的所有其他信息
** after FROM clause ordering.
*/
struct WhereLevel {
  WherePlan plan;       /* query plan for this element of the FROM clause 	FORM子句这个元素的查询计划*/
  int iLeftJoin;        /* Memory cell used to implement LEFT OUTER JOIN 	存储单元用于实现LEFT OUTER JOIN*/
  int iTabCur;          /* The VDBE cursor used to access the table 		访问表的VDBE光标*/
  int iIdxCur;          /* The VDBE cursor used to access pIdx 			访问pIdx的VDBE光标*/
  int addrBrk;          /* Jump here to break out of the loop 			跳转到这里跳出循环*/
  int addrNxt;          /* Jump here to start the next IN combination 		跳转到这里开始下一个IN连接*/
  int addrCont;         /* Jump here to continue with the next loop cycle 	跳转到这里继续下一个循环周期*/
  int addrFirst;        /* First instruction of interior of the loop 		循环内部的第一条指令*/
  u8 iFrom;             /* Which entry in the FROM clause 			FORM子句中的条目*/
  u8 op, p5;            /* Opcode and P5 of the opcode that ends the loop 	操作码和循环结束的操作码P5*/
  int p1, p2;           /* Operands of the opcode used to ends the loop 	用于结束循环的操作码的操作数*/
  union {               /* Information that depends on plan.wsFlags 		取决于plan.wsFlags的信息*/
    struct {
      int nIn;              /* Number of entries in aInLoop[] 			在alnLoop[]条目数*/
      struct InLoop {
        int iCur;              /* The VDBE cursor used by this IN operator 	IN操作符使用的VDBE游标*/
        int addrInTop;         /* Top of the IN loop 				IN循环顶部*/
      } *aInLoop;           /* Information about each nested IN operator 	每个嵌套IN操作符的信息*/
    } in;                 /* Used when plan.wsFlags&WHERE_IN_ABLE 		plan.wsFlags和WHERE_IN_ABLE是使用*/
    Index *pCovidx;       /* Possible covering index for WHERE_MULTI_OR 	对于WHERE_MULTI_OR可能覆盖索引*/
  } u;

  /* The following field is really not part of the current level.  But		以下字段真的不是当前级别的一部分
  ** we need a place to cache virtual table index information for each		但是我们需要一个地方来缓存在FORM子句的每个虚拟表的虚拟表索引信息
  ** virtual table in the FROM clause and the WhereLevel structure is
  ** a convenient place since there is one WhereLevel for each FROM clause	WhereLevel结构是一个方便的地方因为每个FORM子句元素都有一个WhereLevel
  ** element.
  */
  sqlite3_index_info *pIdxInfo;  /* Index info for n-th source table 		第n个源表的索引信息*/
};

/*
** Flags appropriate for the wctrlFlags parameter of sqlite3WhereBegin()	标志适合sqlite3WhereBegin()的wctrlFlags参数和WhereInfo.wctrFlags成员
** and the WhereInfo.wctrlFlags member.
*/
#define WHERE_ORDERBY_NORMAL   0x0000 /* No-op 					无操作*/
#define WHERE_ORDERBY_MIN      0x0001 /* ORDER BY processing for min() func 	ORDER BY 处理min()函数*/
#define WHERE_ORDERBY_MAX      0x0002 /* ORDER BY processing for max() func 	ORDER BY处理max()函数*/
#define WHERE_ONEPASS_DESIRED  0x0004 /* Want to do one-pass UPDATE/DELETE 	想一次通过的UPDATE/DELETE*/
#define WHERE_DUPLICATES_OK    0x0008 /* Ok to return a row more than once 	不止一次确认返回的行*/
#define WHERE_OMIT_OPEN_CLOSE  0x0010 /* Table cursors are already open 	表指针已经打开*/
#define WHERE_FORCE_TABLE      0x0020 /* Do not use an index-only search 	不要仅使用索引搜索*/
#define WHERE_ONETABLE_ONLY    0x0040 /* Only code the 1st table in pTabList 	仅对pTable的第一个表进行编码*/
#define WHERE_AND_ONLY         0x0080 /* Don't use indices for OR terms 	不要对OR使用指数*/

/*
** The WHERE clause processing routine has two halves.  The			WHERE子句处理程序有两个部分
** first part does the start of the WHERE loop and the second			第一部分确实在WHERE循环的开始
** half does the tail of the WHERE loop.  An instance of			第二部分确实在WHERE循环的尾部
** this structure is returned by the first half and passed			这种结构的实例是由上半部分返回
** into the second half to give some continuity.				进入下半部分给一些连续性
*/
struct WhereInfo {
  Parse *pParse;       /* Parsing and code generating context 			解析和编码生成环境*/
  u16 wctrlFlags;      /* Flags originally passed to sqlite3WhereBegin() 	最初的标志传递给sqlite3WhereBegin()*/
  u8 okOnePass;        /* Ok to use one-pass algorithm for UPDATE or DELETE 	可以使用一次通过的算法更新或删除*/
  u8 untestedTerms;    /* Not all WHERE terms resolved by outer loop 		并不是所有的WHERE条件解决外部循环*/
  u8 eDistinct;
  SrcList *pTabList;             /* List of tables in the join 			连接表的列表*/
  int iTop;                      /* The very beginning of the WHERE loop 	WHERE循环的最开始处*/
  int iContinue;                 /* Jump here to continue with next record 	跳转到这里继续下一个记录*/
  int iBreak;                    /* Jump here to break out of the loop 		跳转到这里跳出循环*/
  int nLevel;                    /* Number of nested loop 			嵌套循环数*/
  struct WhereClause *pWC;       /* Decomposition of the WHERE clause 		分解WHERE子句*/
  double savedNQueryLoop;        /* pParse->nQueryLoop outside the WHERE loop 	外循环之外pParse->nQueryLoop*/
  double nRowOut;                /* Estimated number of output rows 		输出行的估计数*/
  WhereLevel a[1];               /* Information about each nest loop in WHERE 	WHERE中每个嵌套循环的信息*/
};

#define WHERE_DISTINCT_UNIQUE 1
#define WHERE_DISTINCT_ORDERED 2

/*
** A NameContext defines a context in which to resolve table and column		一个NameContext定义了解决表和列名的上下文
** names.  The context consists of a list of tables (the pSrcList) field and	上下文包含了表(pSrcList)的列表字段和命名表达式列表(pEList)
** a list of named expression (pEList).  The named expression list may		命名表达式列表有可能是空
** be NULL.  The pSrc corresponds to the FROM clause of a SELECT or		pSrc对应于一个SELECT中的FROM子句
** to the table being operated on by INSERT, UPDATE, or DELETE.  The		或者表正由INSERT,UPDATE,DELETE操作
** pEList corresponds to the result set of a SELECT and is NULL for		pEList对应于SELECT的结果集，对其他声明来说是空
** other statements.
**
** NameContexts can be nested.  When resolving names, the inner-most 		NameContexts可以嵌套。当解析名称时，在最里面的上下文将被第一搜索
** context is searched first.  If no match is found, the next outer		如果没有找到匹配，下一个外部环境进行检查
** context is checked.  If there is still no match, the next context		如果仍然没有匹配，检查下一个上下文
** is checked.  This process continues until either a match is found		这一过程继续直到找到一个匹配或者所有上下文都检查
** or all contexts are check.  When a match is found, the nRef member of	当找到一个匹配，包含该匹配上下文的nRef成员增加
** the context containing the match is incremented. 
**
** Each subquery gets a new NameContext.  The pNext field points to the		每个子查询得到一个新的NameContext.在父查询中pNext字段指向NameContext
** NameContext in the parent query.  Thus the process of scanning the		因此扫描NameContext列表的过程对应于通过依次外子查询查找匹配的搜索
** NameContext list corresponds to searching through successively outer
** subqueries looking for a match.
*/
struct NameContext {
  Parse *pParse;       /* The parser 						解析器*/
  SrcList *pSrcList;   /* One or more tables used to resolve names 		用于解析名称的一个或多个表*/
  ExprList *pEList;    /* Optional list of named expressions 			命名表达式的可选列表*/
  AggInfo *pAggInfo;   /* Information about aggregates at this level 		在这个级别有关聚集的信息*/
  NameContext *pNext;  /* Next outer name context.  NULL for outermost 		下一个外部名字的上下文，最外层为空*/
  int nRef;            /* Number of names resolved by this context 		上下文解决的名字的数量*/
  int nErr;            /* Number of errors encountered while resolving names 	解析名称时遇到的错误数*/
  u8 ncFlags;          /* Zero or more NC_* flags defined below 		零个或多个NC_*标志定义如下*/
};

/*
** Allowed values for the NameContext, ncFlags field.				NameContext,ncFlags字段的允许值
*/
#define NC_AllowAgg  0x01    /* Aggregate functions are allowed here 		这里允许聚合函数*/
#define NC_HasAgg    0x02    /* One or more aggregate functions seen 		一个或多个聚合函数可见*/
#define NC_IsCheck   0x04    /* True if resolving names in a CHECK constraint 	在CHECK约束中解析名称为真*/
#define NC_InAggFunc 0x08    /* True if analyzing arguments to an agg func 	如果分析参数*/

/*
** An instance of the following structure contains all information		以下结构的一个实例包含需要生成代码的一个SELECT语句的所有信息
** needed to generate code for a single SELECT statement.
**
** nLimit is set to -1 if there is no LIMIT clause.  nOffset is set to 0.	如果没有LIMIT子句，nLimit设置为-1.nOffset设置为0
** If there is a LIMIT clause, the parser sets nLimit to the value of the	如果有一个LIMIT子句，分析器设置nLimit为限制的值
** limit and nOffset to the value of the offset (or 0 if there is not		nOffset为偏移值(如果没有偏移为0)
** offset).  But later on, nLimit and nOffset become the memory locations	但后来，nLimit和nOffset成为在VDBE中记录限制和偏移计数器的内存位置
** in the VDBE that record the limit and offset counters.
**
** addrOpenEphm[] entries contain the address of OP_OpenEphemeral opcodes.	addrOpenEphm[]包含OP_OpenEphemeral操作码的地址
** These addresses must be stored so that we can go back and fill in		这些地址必须存储，
** the P4_KEYINFO and P2 parameters later.  Neither the KeyInfo nor		这样我们就可以回去填写P4_KEYINFO和P2参数
** the number of columns in P2 can be computed at the same time			因为没有足够的关于符合查询的信息是已知的，
** as the OP_OpenEphm instruction is coded because not				所以不仅KeyInfo而且P2中列的数目可以计算出来
** enough information about the compound query is known at that point.		同时OP_OpenEphm被编码
** The KeyInfo for addrOpenTran[0] and [1] contains collating sequences		addrOpenTran[0]和[1]的KeyInfo包含结果集的排序序列
** for the result set.  The KeyInfo for addrOpenTran[2] contains collating	addrOpenTran[2]的KeyInfo包含ORDER BY子句的排序序列
** sequences for the ORDER BY clause.
*/
struct Select {
  ExprList *pEList;      /* The fields of the result 				结果的字段*/
  u8 op;                 /* One of: TK_UNION TK_ALL TK_INTERSECT TK_EXCEPT 	TK_UNION TK_ALL TK_INTERSECT TK_EXCEPT 之一*/
  char affinity;         /* MakeRecord with this affinity for SRT_Set 		*/
  u16 selFlags;          /* Various SF_* values 				各种SF_*值*/
  int iLimit, iOffset;   /* Memory registers holding LIMIT & OFFSET counters 	内存寄存器持有LIMIT和OFFSET计数器*/
  int addrOpenEphm[3];   /* OP_OpenEphem opcodes related to this select 	与此选择相关的OP_OpenEphem操作码*/
  double nSelectRow;     /* Estimated number of result rows 			结果行的估计数*/
  SrcList *pSrc;         /* The FROM clause 					FORM子句*/
  Expr *pWhere;          /* The WHERE clause 					WHERE子句*/
  ExprList *pGroupBy;    /* The GROUP BY clause 				GROUP BY子句*/
  Expr *pHaving;         /* The HAVING clause 					HAVING子句*/
  ExprList *pOrderBy;    /* The ORDER BY clause 				ORDER BY子句*/
  Select *pPrior;        /* Prior select in a compound select statement 	在复合SELECT语句之前选择*/
  Select *pNext;         /* Next select to the left in a compound 		接下来在复合语句中选择左侧*/
  Select *pRightmost;    /* Right-most select in a compound select statement 	最右边一个SELECT语句的select*/
  Expr *pLimit;          /* LIMIT expression. NULL means not used. 		LIMIT表达式，NULL表示未使用*/
  Expr *pOffset;         /* OFFSET expression. NULL means not used. 		OFFSET表达式，NULL表示未使用*/
};

/*
** Allowed values for Select.selFlags.  The "SF" prefix stands for		Select.setFlags的允许值。"SF"前缀代表"Select Flag"
** "Select Flag".
*/
#define SF_Distinct        0x01  /* Output should be DISTINCT 			输出应该不同*/
#define SF_Resolved        0x02  /* Identifiers have been resolved 		标识符已经解析*/
#define SF_Aggregate       0x04  /* Contains aggregate functions 		包含聚合函数*/
#define SF_UsesEphemeral   0x08  /* Uses the OpenEphemeral opcode 		使用OpenEphemeral操作码*/
#define SF_Expanded        0x10  /* sqlite3SelectExpand() called on this 	sqlite3SelectExpand()调用这个*/
#define SF_HasTypeInfo     0x20  /* FROM subqueries have Table metadata 	FORM子查询有Table元数据*/
#define SF_UseSorter       0x40  /* Sort using a sorter 			排序使用分拣机**/
#define SF_Values          0x80  /* Synthesized from VALUES clause 		从VALUES子句合成*/


/*
** The results of a select can be distributed in several ways.  The		结果的选择可以分布在几个方面
** "SRT" prefix means "SELECT Result Type".					"SRT"前缀表示"SELECT Result Type"
*/
#define SRT_Union        1  /* Store result as keys in an index 		结果以索引键存储*/
#define SRT_Except       2  /* Remove result from a UNION index 		从UNION索引中删除的结果*/
#define SRT_Exists       3  /* Store 1 if the result is not empty 		如果结果不为空存储1*/
#define SRT_Discard      4  /* Do not save the results anywhere 		不在任何地方保存结果*/

/* The ORDER BY clause is ignored for all of the above 				在ORDER BY 子句忽略上述所有*/
#define IgnorableOrderby(X) ((X->eDest)<=SRT_Discard)

#define SRT_Output       5  /* Output each row of result 			输出结果的每一行*/
#define SRT_Mem          6  /* Store result in a memory cell 			将结果存储在存储单元*/
#define SRT_Set          7  /* Store results as keys in an index 		结果以索引键存储*/
#define SRT_Table        8  /* Store result as data with an automatic rowid 	存储结果为自动rowid数据*/
#define SRT_EphemTab     9  /* Create transient tab and store like SRT_Table 	创建瞬时标签，存储像SRT_Table*/
#define SRT_Coroutine   10  /* Generate a single row of result 			生成结果的一个单列*/

/*
** A structure used to customize the behavior of sqlite3Select(). See		用来制定sqlite3Select()行为的结构
** comments above sqlite3Select() for details.					见上面sqlite3Select()的注释连接详细信息
*/
typedef struct SelectDest SelectDest;
struct SelectDest {
  u8 eDest;         /* How to dispose of the results 				如何处理结果*/
  u8 affSdst;       /* Affinity used when eDest==SRT_Set */
  int iSDParm;      /* A parameter used by the eDest disposal method 		eDest方法所用的参数*/
  int iSdst;        /* Base register where results are written 			结果被写入基址寄存器**/
  int nSdst;        /* Number of registers allocated 				分配寄存器的数量*/
};

/*
** During code generation of statements that do inserts into AUTOINCREMENT 	在代码生成声明中插入到AUTOINCREMENT 表
** tables, the following information is attached to the Table.u.autoInc.p	以下信息被附加到每个自动增量表的Table.u.authoInc.p指针记录代码生成需要的一些侧面信息
** pointer of each autoincrement table to record some side information that
** the code generator needs.  We have to keep per-table autoincrement		我们必须有每个表的自动增量信息	，以防插入数在触发器内下降
** information in case inserts are down within triggers.  Triggers do not	以防插入数在触发器内下降
** normally coordinate their activities, but we do need to coordinate the	触发器通常不会协调它们的活动
** loading and saving of autoincrement information.				但是我们确实需要加载和保存自动增量信息
*/
struct AutoincInfo {
  AutoincInfo *pNext;   /* Next info block in a list of them all 		在他们所有列表的下一个信息块*/
  Table *pTab;          /* Table this info block refers to 			表信息块的指向*/
  int iDb;              /* Index in sqlite3.aDb[] of database holding pTab 	数据库中sqlite3.aDb[]的索引保存pTab*/
  int regCtr;           /* Memory register holding the rowid counter 		用于保存ROWID计算器的内存寄存器*/
};

/*
** Size of the column cache							列缓存大小
*/
#ifndef SQLITE_N_COLCACHE
# define SQLITE_N_COLCACHE 10
#endif

/*
** At least one instance of the following structure is created for each 	至少一个以下结构体的实例为每个触发器创建
** trigger that may be fired while parsing an INSERT, UPDATE or DELETE		当执行INSERT,UPDATE,DELETE是有可能触发触发器。
** statement. All such objects are stored in the linked list headed at		所有这些对象存储在Parse.pTriggerPrg为首的链表，
** Parse.pTriggerPrg and deleted once statement compilation has been		一旦语句编译完成就删除这些对象。
** completed.
**
** A Vdbe sub-program that implements the body and WHEN clause of trigger	一个实现了体和WHEN子句触发器TriggerPrg.pTrigger的VDBE分程序
** TriggerPrg.pTrigger, assuming a default ON CONFLICT clause of		假设一个TriggerPrg.orconf默认ON CONFLICT子句
** TriggerPrg.orconf, is stored in the TriggerPrg.pProgram variable.		存储在TriggerPrg.pProgram变量中
** The Parse.pTriggerPrg list never contains two entries with the same		Parse.pTriggerPrg列表从未包含pTrigger和orconf值相同的两个条目
** values for both pTrigger and orconf.
**
** The TriggerPrg.aColmask[0] variable is set to a mask of old.* columns	TriggerPrg.aColmask[0]变量被设置为一个隐藏的old.*列存储
** accessed (or set to 0 for triggers fired as a result of INSERT 		(或设置为0当INSERT结果触发了触发器)
** statements). Similarly, the TriggerPrg.aColmask[1] variable is set to	同样，TriggerPrg.aColmask[1]变量设置为一个隐藏的由程序使用new.*列
** a mask of new.* columns used by the program.
*/
struct TriggerPrg {
  Trigger *pTrigger;      /* Trigger this program was coded from 		触发这个程序的代码*/
  TriggerPrg *pNext;      /* Next entry in Parse.pTriggerPrg list 		Parse.pTriggerPrg列表的下一个条目*/
  SubProgram *pProgram;   /* Program implementing pTrigger/orconf 		项目实施pTrigger/orconf*/
  int orconf;             /* Default ON CONFLICT policy 			默认ON CONFLICT政策*/
  u32 aColmask[2];        /* Masks of old.*, new.* columns accessed 		old.*,new.*列的隐藏访问*/
};

/*
** The yDbMask datatype for the bitmask of all attached databases.		所有附加数据库的位掩码的yDbMask数据类型
*/
#if SQLITE_MAX_ATTACHED>30
  typedef sqlite3_uint64 yDbMask;
#else
  typedef unsigned int yDbMask;
#endif

/*
** An SQL parser context.  A copy of this structure is passed through		一个SQL解析器上下文。
** the parser and down into all the parser action routine in order to		这种结构的副本通过解析器和分解为所有解析器操作例程
** carry around information that is global to the entire parse.			以携带整个全局解析信息
**
** The structure is divided into two parts.  When the parser and code		结构分为两部分。当解析器和代码生成递归调用它们自身，
** generate call themselves recursively, the first part of the structure	该结构的第一部分是不变的
** is constant but the second part is reset at the beginning and end of		但是第二部分在每个递归开始和结束时被复位
** each recursion.
**
** The nTableLock and aTableLock variables are only used if the shared-cache 	nTableLock和TableLock变量仅当启用共享缓存特性(当sq	lite3Ted()->useShareDate为真)时使用
** feature is enabled (if sqlite3Tsd()->useSharedData is true). They are	(当sq	lite3Ted()->useShareDate为真)时使用
** used to store the set of table-locks required by the statement being		它们用来存储一组表锁需要的被编译语句
** compiled. Function sqlite3TableLock() is used to add entries to the		sqlite3TableLock()函数用于将条目添加到列表中
** list.
*/
struct Parse {
  sqlite3 *db;         /* The main database structure 				主数据结构*/
  char *zErrMsg;       /* An error message 					错误消息*/
  Vdbe *pVdbe;         /* An engine for executing database bytecode 		执行数据库字节码的引擎*/
  int rc;              /* Return code from execution 				返回代码的执行*/
  u8 colNamesSet;      /* TRUE after OP_ColumnName has been issued to pVdbe 	OP_ColumnName已处理给pVdbe后*/
  u8 checkSchema;      /* Causes schema cookie check after an error 		错误后引起的模式cookie检查*/
  u8 nested;           /* Number of nested calls to the parser/code generator 	嵌套调用解析器/代码生成器的数量*/
  u8 nTempReg;         /* Number of temporary registers in aTempReg[] 		aTempReg[]的临时寄存器数量*/
  u8 nTempInUse;       /* Number of aTempReg[] currently checked out 		当前被检查的TempReg[]数量*/
  u8 nColCache;        /* Number of entries in aColCache[] 			在aColCache[]的条目数*/
  u8 iColCache;        /* Next entry in aColCache[] to replace 			在aColCache[]中被取代的下一个条目*/
  u8 isMultiWrite;     /* True if statement may modify/insert multiple rows 	如果语句可以修改/插入多行*/
  u8 mayAbort;         /* True if statement may throw an ABORT exception 	如果语句可能会抛出一个ABORT异常*/
  int aTempReg[8];     /* Holding area for temporary registers 			为临时寄存器保留地方*/
  int nRangeReg;       /* Size of the temporary register block 			临时寄存器块的大小*/
  int iRangeReg;       /* First register in temporary register block 		在临时寄存器块的第一个寄存器*/
  int nErr;            /* Number of errors seen 				看到错误数*/
  int nTab;            /* Number of previously allocated VDBE cursors 		以前分配VDBE光标的数量*/
  int nMem;            /* Number of memory cells used so far 			到目前为止使用的内存单元的数量*/
  int nSet;            /* Number of sets used so far 				到目前为止使用的设置数量*/
  int nOnce;           /* Number of OP_Once instructions so far 		到目前为止OP_Once指令的数量*/
  int ckBase;          /* Base register of data during check constraints 	在检查约束时基址寄存器的数据*/
  int iCacheLevel;     /* ColCache valid when aColCache[].iLevel<=iCacheLevel 	当aColCache[].iLevel<=iCacheLevel时，ColCache有效*/
  int iCacheCnt;       /* Counter used to generate aColCache[].lru values 	用于生成aColCache[].lru值的计数器*/
  struct yColCache {
    int iTable;           /* Table cursor number 				表指针数*/
    int iColumn;          /* Table column number 				表列号*/
    u8 tempReg;           /* iReg is a temp register that needs to be freed 	iReg是需要被释放的临时寄存器*/
    int iLevel;           /* Nesting level 					嵌套层次*/
    int iReg;             /* Reg with value of this column. 0 means none. 	保存这个column.0的寄存器表示空*/
    int lru;              /* Least recently used entry has the smallest value 	最近最少使用条目的最小值*/
  } aColCache[SQLITE_N_COLCACHE];  /* One for each column cache entry 		一个用于每列缓存条目*/
  yDbMask writeMask;   /* Start a write transaction on these databases 		开始对这些数据库进行写事务*/
  yDbMask cookieMask;  /* Bitmask of schema verified databases 			位掩码模式验证数据库*/
  int cookieGoto;      /* Address of OP_Goto to cookie verifier subroutine 	追踪验证子程序的OP_Goto地址*/
  int cookieValue[SQLITE_MAX_ATTACHED+2];  /* Values of cookies to verify 	追踪的值来验证*/
  int regRowid;        /* Register holding rowid of CREATE TABLE entry 		有CREATE TABLE条目的伪列的寄存器*/
  int regRoot;         /* Register holding root page number for new objects 	有新对象的跟页码的寄存器*/
  int nMaxArg;         /* Max args passed to user function by sub-program 	Max参数通过子程序传递给用户函数*/
  Token constraintName;/* Name of the constraint currently being parsed 	目前正在解析的约束的名字*/
#ifndef SQLITE_OMIT_SHARED_CACHE
  int nTableLock;        /* Number of locks in aTableLock 			在aTableLock中锁的数量*/
  TableLock *aTableLock; /* Required table locks for shared-cache mode 		要求表的共享缓存模式的锁*/
#endif
  AutoincInfo *pAinc;  /* Information about AUTOINCREMENT counters 		有关AUTOINCREMENT计数器的信息*/

  /* Information used while coding trigger programs. 				当编码触发程序时使用的信息*/
  Parse *pToplevel;    /* Parse structure for main program (or NULL) 		解析主程序的结构(或空)*/
  Table *pTriggerTab;  /* Table triggers are being coded for 			表触发器被编码*/
  double nQueryLoop;   /* Estimated number of iterations of a query 		一个迭代查询的估计数*/
  u32 oldmask;         /* Mask of old.* columns referenced 			old.*列的隐藏引用*/
  u32 newmask;         /* Mask of new.* columns referenced 			new.*列的隐藏引用*/
  u8 eTriggerOp;       /* TK_UPDATE, TK_INSERT or TK_DELETE */
  u8 eOrconf;          /* Default ON CONFLICT policy for trigger steps 		默认ON CONFICT规则的触发步骤*/
  u8 disableTriggers;  /* True to disable triggers 				禁用触发器*/

  /* Above is constant between recursions.  Below is reset before and after	以上在每次递归时不变，以下在每次递归前和递归后复位
  ** each recursion */

  int nVar;                 /* Number of '?' variables seen in the SQL so far 	目前为止在SQL中可见的"?"变量数量*/
  int nzVar;                /* Number of available slots in azVar[] 		在azVar[]中可用测试器数量*/
  u8 explain;               /* True if the EXPLAIN flag is found on the query 	如果查询时发现EXPLAIN标志*/
#ifndef SQLITE_OMIT_VIRTUALTABLE
  u8 declareVtab;           /* True if inside sqlite3_declare_vtab() 		如果在sqlite3_declare_vtab()中*/
  int nVtabLock;            /* Number of virtual tables to lock 		锁住的虚表数量*/
#endif
  int nAlias;               /* Number of aliased result set columns 		设置列的别名结果集数量*/
  int nHeight;              /* Expression tree height of current sub-select 	表达式数当前子选择的高度*/
#ifndef SQLITE_OMIT_EXPLAIN
  int iSelectId;            /* ID of current select for EXPLAIN output 		EXPLAIN输出的当前选择的ID*/
  int iNextSelectId;        /* Next available select ID for EXPLAIN output 	EXPLAIN输出的下一个可用的选择ID*/
#endif
  char **azVar;             /* Pointers to names of parameters 			参数名称的指针*/
  Vdbe *pReprepare;         /* VM being reprepared (sqlite3Reprepare()) 	VM被重修(sqlite3Reprepare())*/
  int *aAlias;              /* Register used to hold aliased result 		用来存储别名结果的寄存器*/
  const char *zTail;        /* All SQL text past the last semicolon parsed 	所有SQL文本通过最后一个分号解析*/
  Table *pNewTable;         /* A table being constructed by CREATE TABLE 	一个表由CREATE TABLE构建*/
  Trigger *pNewTrigger;     /* Trigger under construct by a CREATE TRIGGER 	根据CREATE TRIGGER结构触发**/
  const char *zAuthContext; /* The 6th parameter to db->xAuth callbacks 	第六个DB-> XAUTH回调参数*/
  Token sNameToken;         /* Token with unqualified schema object name 	标记不合格架构对象名称*/
  Token sLastToken;         /* The last token parsed 				最后一个符号解析*/
#ifndef SQLITE_OMIT_VIRTUALTABLE
  Token sArg;               /* Complete text of a module argument 		一个模块参数的完整文本*/
  Table **apVtabLock;       /* Pointer to virtual tables needing locking 	需要锁定的虚表指针*/
#endif
  Table *pZombieTab;        /* List of Table objects to delete after code gen 	代码生成后删除表对象列表*/
  TriggerPrg *pTriggerPrg;  /* Linked list of coded triggers 			编码触发器链表*/
};

/*
** Return true if currently inside an sqlite3_declare_vtab() call.		如果当前的sqlite3_declare_vtab（）调用里面返回真
*/
#ifdef SQLITE_OMIT_VIRTUALTABLE
  #define IN_DECLARE_VTAB 0
#else
  #define IN_DECLARE_VTAB (pParse->declareVtab)
#endif

/*
** An instance of the following structure can be declared on a stack and used	以下结构的实例可以在堆栈中声明
** to save the Parse.zAuthContext value so that it can be restored later.	和用来保存保存Parse.zAuthContext值，以便在以后恢复	
*/
struct AuthContext {
  const char *zAuthContext;   /* Put saved Parse.zAuthContext here 		把Parse.zAuthContext保存在这里*/
  Parse *pParse;              /* The Parse structure 				解析结构*/
};

/*
** Bitfield flags for P5 value in various opcodes.				用于各种操作码P5值的位域标志
*/
#define OPFLAG_NCHANGE       0x01    /* Set to update db->nChange 		设置更新db->nChange*/
#define OPFLAG_LASTROWID     0x02    /* Set to update db->lastRowid 		设置更新db->lastRowid*/
#define OPFLAG_ISUPDATE      0x04    /* This OP_Insert is an sql UPDATE 	这OP_Insert是一个SQL UPDATE*/
#define OPFLAG_APPEND        0x08    /* This is likely to be an append 		这很可能是一个附加*/
#define OPFLAG_USESEEKRESULT 0x10    /* Try to avoid a seek in BtreeInsert() 	尽量避免寻求BtreeInsert（）*/
#define OPFLAG_CLEARCACHE    0x20    /* Clear pseudo-table cache in OP_Column 	在OP_Column清除伪表缓存*/
#define OPFLAG_LENGTHARG     0x40    /* OP_Column only used for length() 	OP_Column仅用于length()*/
#define OPFLAG_TYPEOFARG     0x80    /* OP_Column only used for typeof() 	OP_Column仅用于typeof()*/
#define OPFLAG_BULKCSR       0x01    /* OP_Open** used to open bulk cursor 	OP_Open**用于打开大批游标*/
#define OPFLAG_P2ISREG       0x02    /* P2 to OP_Open** is a register number 	OP_P2到OP_Open**是一个寄存器号*/

/*
 * Each trigger present in the database schema is stored as an instance of	在数据库模式中每个现存的触发器存储为一个触发结构的实例
 * struct Trigger. 
 *
 * Pointers to instances of struct Trigger are stored in two ways.		这种触发器实例的指针有两种存储方式
 * 1. In the "trigHash" hash table (part of the sqlite3* that represents the 	1.在“trigHash”哈希表（代表数据库的sqlite3的*一部分）。
 *    database). This allows Trigger structures to be retrieved by name.	这允许通过名称检索触发结构。
 * 2. All triggers associated with a single table form a linked list, using the	2.与一个表相关联的所有触发器形成一个链表,使用触发结构的pNext成员
 *    pNext member of struct Trigger. A pointer to the first element of the	第一个元素的链表指针存储在与Table相关的"pTrigger"成员中				
 *    linked list is stored as the "pTrigger" member of the associated
 *    struct Table.
 *
 * The "step_list" member points to the first element of a linked list		该“step_list”成员指向含有指定作为触发程序的SQL语句的链表的第一个元素..
 * containing the SQL statements specified as the trigger program.
 */
struct Trigger {
  char *zName;            /* The name of the trigger                        	触发器名称*/
  char *table;            /* The table or view to which the trigger applies 	触发器适用的表或视图*/
  u8 op;                  /* One of TK_DELETE, TK_UPDATE, TK_INSERT         	TK_DELETE, TK_UPDATE, TK_INSERT其中之一*/
  u8 tr_tm;               /* One of TRIGGER_BEFORE, TRIGGER_AFTER 		TRIGGER_BEFORE, TRIGGER_AFTER其中之一*/
  Expr *pWhen;            /* The WHEN clause of the expression (may be NULL) 	WHEN子句的表达式(可能为空)*/
  IdList *pColumns;       /* If this is an UPDATE OF <column-list> trigger,	如果是一个UPDATE OF<列列表>触发器，
                             the <column-list> is stored here 			<列列表>触发器存储在这里*/
  Schema *pSchema;        /* Schema containing the trigger 			包含触发器的模式*/
  Schema *pTabSchema;     /* Schema containing the table 			包含表的模式*/
  TriggerStep *step_list; /* Link list of trigger program steps             	触发程序步骤的链表*/
  Trigger *pNext;         /* Next trigger associated with the table 		与表关联的下一个触发器*/
};

/*
** A trigger is either a BEFORE or an AFTER trigger.  The following constants	触发器是无论是BEFORE或AFTER触发器。以下常量确定。
** determine which. 
**
** If there are multiple triggers, you might of some BEFORE and some AFTER.	如果有多个触发器，可能有BEGORE触发器和AFTER触发器
** In that cases, the constants below can be ORed together.			在这种情况下，下面的常量可以进行或运算
*/
#define TRIGGER_BEFORE  1
#define TRIGGER_AFTER   2

/*
 * An instance of struct TriggerStep is used to store a single SQL statement	TriggerStep结构的一个实例用于存储触发器程序一部分的单个SQL语句
 * that is a part of a trigger-program. 
 *
 * Instances of struct TriggerStep are stored in a singly linked list (linked	TriggerStep结构的一个实例存储在一个通过引用与此相关的Trigger结构的
 * using the "pNext" member) referenced by the "step_list" member of the 	实例的"step_list"成员的单链表(使用"pNext"成员连接) 
 * associated struct Trigger instance. The first element of the linked list is	该链表的第一个元素是触发程序的第一步。
 * the first step of the trigger-program.
 * 
 * The "op" member indicates whether this is a "DELETE", "INSERT", "UPDATE" or	"op"成员表示这是否是一个"DELETE", "INSERT", "UPDATE" 或"SELECT"	语句	
 * "SELECT" statement. The meanings of the other members is determined by the 	其他成员的含义是由如下的“OP”的值确定：
 * value of "op" as follows:
 *
 * (op == TK_INSERT)
 * orconf    -> stores the ON CONFLICT algorithm				orconf->存储ON CONFLICT算法
 * pSelect   -> If this is an INSERT INTO ... SELECT ... statement, then	pSelect->如果这是一个INSERT INTO ... SELECT...语句，
 *              this stores a pointer to the SELECT statement. Otherwise NULL.	那么这个存储的指针SELECT语句。否则NULL。
 * target    -> A token holding the quoted name of the table to insert into.	target    -> 标记持有插入表的引用名
 * pExprList -> If this is an INSERT INTO ... VALUES ... statement, then	pExprList ->如果这是一个 INSERT INTO ... VALUES ... 语句，
 *              this stores values to be inserted. Otherwise NULL.		那么这个存储要插入的值。否则NULL
 * pIdList   -> If this is an INSERT INTO ... (<column-names>) VALUES ... 	pIdList   ->如果这是一个INSERT INTO ... (<column-names>) VALUES ... 语句，
 *              statement, then this stores the column-names to be		那么这个存储要被插入的列名
 *              inserted into.
 *
 * (op == TK_DELETE)
 * target    -> A token holding the quoted name of the table to delete from.	target->标记持有进行删除的表的引用名称
 * pWhere    -> The WHERE clause of the DELETE statement if one is specified.	pWhere ->指定DELETE语句的WHERE子句
 *              Otherwise NULL.
 * 
 * (op == TK_UPDATE)
 * target    -> A token holding the quoted name of the table to update rows of.	target->标记持有更新行的表的引用名
 * pWhere    -> The WHERE clause of the UPDATE statement if one is specified.	pWhere->指定UPDATE语句的WHERE子句
 *              Otherwise NULL.
 * pExprList -> A list of the columns to update and the expressions to update	pExprList -> 更新的列的列表和更新它们的表达式
 *              them to. See sqlite3Update() documentation of "pChanges"	看sqlite3Update()文档的"pChanges"说法
 *              argument.
 * 
 */
struct TriggerStep {
  u8 op;               /* One of TK_DELETE, TK_UPDATE, TK_INSERT, TK_SELECT 	TK_DELETE, TK_UPDATE, TK_INSERT, TK_SELECT其中之一*/
  u8 orconf;           /* OE_Rollback etc. */
  Trigger *pTrig;      /* The trigger that this step is a part of 		这个触发是这一步的一部分*/
  Select *pSelect;     /* SELECT statment or RHS of INSERT INTO .. SELECT ... */
  Token target;        /* Target table for DELETE, UPDATE, INSERT 		对于DELETE，UPDATE，INSERT目标表*/
  Expr *pWhere;        /* The WHERE clause for DELETE or UPDATE steps 		WHERE子句中DELETE 或UPDATE步骤*/
  ExprList *pExprList; /* SET clause for UPDATE.  VALUES clause for INSERT 	UPDATE的SET子句，INSERT的VALUES子句*/
  IdList *pIdList;     /* Column names for INSERT 				对于INSERT的列名*/
  TriggerStep *pNext;  /* Next in the link-list 				在链表中后继*/
  TriggerStep *pLast;  /* Last element in link-list. Valid for 1st elem only 	链表中最后一个元素。只有第一个元素有效*/
};

/*
** The following structure contains information used by the sqliteFix...	以下结构包含用于例程sqliteFix…的信息
** routines as they walk the parse tree to make database references		例程解析式行走时数据库引用明确
** explicit.  
*/
typedef struct DbFixer DbFixer;
struct DbFixer {
  Parse *pParse;      /* The parsing context.  Error messages written here 	解析上下文。错误信息写在这里*/
  const char *zDb;    /* Make sure all objects are contained in this database 	确保所有对象均包含在这个数据库*/
  const char *zType;  /* Type of the container - used for error messages 	容器的类型-用于错误信息*/
  const Token *pName; /* Name of the container - used for error messages 	容器的名称-用于错误信息*/
};

/*
** An objected used to accumulate the text of a string where we			一个用来积累字符串文本的对象
** do not necessarily know how big the string will be in the end.		我们不一定知道字符串最终多大
*/
struct StrAccum {
  sqlite3 *db;         /* Optional database for lookaside.  Can be NULL 	可选数据库的后备。可以为空*/
  char *zBase;         /* A base allocation.  Not from malloc. 			一个基本的分配。不是从malloc分配的*/
  char *zText;         /* The string collected so far 				至今收集的字符串*/
  int  nChar;          /* Length of the string so far 				到目前字符串的长度*/
  int  nAlloc;         /* Amount of space allocated in zText 			分配给zText的空间大小*/
  int  mxAlloc;        /* Maximum allowed string length 			允许字符串的最大长度*/
  u8   mallocFailed;   /* Becomes true if any memory allocation fails 		如果有内存分配失败则变为真*/
  u8   useMalloc;      /* 0: none,  1: sqlite3DbMalloc,  2: sqlite3_malloc */
  u8   tooBig;         /* Becomes true if string size exceeds limits 		如果字符串大小超过限定值变为真*/
};

/*
** A pointer to this structure is used to communicate information		该结构的指针用来从sqlite3Init和OP_ParseSchema到sqlite3InitCallback交流信息
** from sqlite3Init and OP_ParseSchema into the sqlite3InitCallback.
*/
typedef struct {
  sqlite3 *db;        /* The database being initialized 			初始化数据库*/
  char **pzErrMsg;    /* Error message stored here 				在此存储错误信息*/
  int iDb;            /* 0 for main database.  1 for TEMP, 2.. for ATTACHed 	0为主数据库，1为临时，2为附加*/
  int rc;             /* Result code stored here 				存储在此的结果代码*/
} InitData;

/*
** Structure containing global configuration data for the SQLite library.	包含SQLite数据库全局配置数据的结构
**
** This structure also contains some state information.				此结构还包含一些状态信息
*/
struct Sqlite3Config {
  int bMemstat;                     /* True to enable memory status 		启用内存状态为真*/
  int bCoreMutex;                   /* True to enable core mutexing 		启用核心互斥为真*/
  int bFullMutex;                   /* True to enable full mutexing 		启用完整的互斥锁为真*/
  int bOpenUri;                     /* True to interpret filenames as URIs 	解释文件名作为URL*/
  int mxStrlen;                     /* Maximum string length 			字符串的最大长度*/
  int szLookaside;                  /* Default lookaside buffer size 		默认的后被缓冲区大小*/
  int nLookaside;                   /* Default lookaside buffer count 		默认后备缓冲计算器*/
  sqlite3_mem_methods m;            /* Low-level memory allocation interface 	低级别的内存分配借口*/
  sqlite3_mutex_methods mutex;      /* Low-level mutex interface 		低层次的互斥借口*/
  sqlite3_pcache_methods2 pcache2;  /* Low-level page-cache interface 		低级别的页面缓存接口*/
  void *pHeap;                      /* Heap storage space 			堆存储空间*/
  int nHeap;                        /* Size of pHeap[] 				pHead[]的大小*/
  int mnReq, mxReq;                 /* Min and max heap requests sizes 		最小堆和最大堆的要求大小*/
  void *pScratch;                   /* Scratch memory 				临时内存*/
  int szScratch;                    /* Size of each scratch buffer 		每个暂用缓存的大小*/
  int nScratch;                     /* Number of scratch buffers 		暂用缓存的数量*/
  void *pPage;                      /* Page cache memory 			页面高速缓存存储器*/
  int szPage;                       /* Size of each page in pPage[] 		pPage[]中每个页面的大小*/
  int nPage;                        /* Number of pages in pPage[] 		pPage[]中页面的数量*/
  int mxParserStack;                /* maximum depth of the parser stack 	解析器堆栈的最大深度*/
  int sharedCacheEnabled;           /* true if shared-cache mode enabled 	如果共享缓存模式为真*/
  /* The above might be initialized to non-zero.  The following need to always	上面可能会初始化为非零。但是下面始终初始化为零
  ** initially be zero, however. */
  int isInit;                       /* True after initialization has finished 	初始化完成后为真*/
  int inProgress;                   /* True while initialization in progress 	正在进行初始化为真*/
  int isMutexInit;                  /* True after mutexes are initialized 	互斥体被初始化后为真*/
  int isMallocInit;                 /* True after malloc is initialized 	malloc被初始化后为真*/
  int isPCacheInit;                 /* True after malloc is initialized 	malloc被初始化后为真*/
  sqlite3_mutex *pInitMutex;        /* Mutex used by sqlite3_initialize() 	sqlite3_initialize()使用的互斥*/
  int nRefInitMutex;                /* Number of users of pInitMutex 		pInitMutex的用户数*/
  void (*xLog)(void*,int,const char*); /* Function for logging 			功能记录*/
  void *pLogArg;                       /* First argument to xLog() 		xLog()的第一个参数*/
  int bLocaltimeFault;              /* True to fail localtime() calls 		调用localtime()失败后为真*/
};

/*
** Context pointer passed down through the tree-walk.				通过数的路径传递下来的上下文指针
*/
struct Walker {
  int (*xExprCallback)(Walker*, Expr*);     /* Callback for expressions *回调函数表达式/
  int (*xSelectCallback)(Walker*,Select*);  /* Callback for SELECTs 回调函数SELECTs*/
  Parse *pParse;                            /* Parser context. 分析器上下文 */
  int walkerDepth;                          /* Number of subqueries 子查询数*/
  union {                                   /* Extra data for callback 额外的回调数据*/
    NameContext *pNC;                          /* Naming context 一个namecontext结构体的指针命名上下文*/
    int i;                                     /* Integer value 定义一个整形*/
    SrcList *pSrcList;                         /* FROM clause FROM子句*/
    struct SrcCount *pSrcCount;                /* Counting column references计算列引用 */
  } u;
};

/* Forward declarations 前置声明*/
int sqlite3WalkExpr(Walker*, Expr*);
int sqlite3WalkExprList(Walker*, ExprList*);
int sqlite3WalkSelect(Walker*, Select*);
int sqlite3WalkSelectExpr(Walker*, Select*);
int sqlite3WalkSelectFrom(Walker*, Select*);

/*
** Return code from the parse-tree walking primitives and their
** callbacks.
从解析树遍历的结点和他们的回调函数返回代码*/
#define WRC_Continue    0   /* Continue down into children 继续向下访问孩子结点*/
#define WRC_Prune       1   /* Omit children but continue walking siblings 忽略孩子结点但继续访问兄弟结点*/
#define WRC_Abort       2   /* Abandon the tree walk放弃树的遍历 */

/*
** Assuming zIn points to the first byte of a UTF-8 character,
** advance zIn to point to the first byte of the next UTF-8 character.
假设指针ZIN指向UTF-8字符的第一个字节，就将ZIN前进以指向下一个UTF-8字符的第一个字节。*/
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
FTS4是FTS3真正的延伸。它能够使用宏SQLITE_ENABLE_FTS3。但为了避免混淆，我们仍然将所有的SQLITE_ENABLE_FTS4作为alisse来服务于SQLITE_ENABLE_FTS3。*/
#if defined(SQLITE_ENABLE_FTS4) && !defined(SQLITE_ENABLE_FTS3)
# define SQLITE_ENABLE_FTS3
#endif

/*
** The ctype.h header is needed for non-ASCII systems.  It is also
   头文件 ctype.h是非ASCII系统所必须的。当FTS3被包括于amalgamation中时，这个头文件对FTS3来说也是必需的。
** needed by FTS3 when FTS3 is included in the amalgamation.
*/
#if !defined(SQLITE_ASCII) || \
    (defined(SQLITE_ENABLE_FTS3) && defined(SQLITE_AMALGAMATION))
# include <ctype.h>
#endif

/*
** The following macros mimic the standard library functions toupper(),
** isspace(), isalnum(), isdigit() and isxdigit(), respectively. The
以下的宏分别模仿了标准库函数 toupper(),isspace(), isalnum(), isdigit() and isxdigit()。
** sqlite versions only work for ASCII characters, regardless of locale.
该sqlite版本只对ASCII字符生效，不论区域。*/
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
** Internal function prototypes内部函数原型
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
在具有充足的堆栈空间和支持alloca（）的系统中，使用alloca（）来为大型自动对象获取空间。在缺省的情况下，用 malloc()来获取空间。
** The alloca() routine never returns NULL.  This will cause code paths
** that deal with sqlite3StackAlloc() failures to be unreachable.
alloca()永远不会返回空值，这将导致处理sqlite3StackAlloc()错误的代码路径不可达。
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

/* Output formatting for SQLITE_TESTCTRL_EXPLAIN*/ /*SQLITE_TESTCTRL_EXPLAIN的格式化输出*/
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
**记录的头部由一系列可变长度的整数组成。这些整数几乎都很小并且被编码为一个单字节。下面的宏利用了这个优点
提供了一种对记录头里的整数快速编解码的方式。当整数是单字节时这种方式就比较快，当整数是双字节或者多字节它
就会慢一些，但是不管怎样，它都更快了。
** The following expressions are equivalent:
      下面的式子是相等的
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
 LEMON-generated分析器的接口（Lemon的主要功能就是根据上下文无关文法(CFG)，生成支持该文法的分析器。）
*/
void *sqlite3ParserAlloc(void*(*)(size_t));   /*ParseAlloc为分析器分配空间，然后初始化它，返回一个指向分析器的指针*/
void sqlite3ParserFree(void*, void(*)(void*));  /*当程序不再使用分析器时，应该回收为其分配的内存*/
void sqlite3Parser(void*, int, Token, Parse*); /*Parse是Lemon生成的分析器的核心例程。在分析器调用ParseAlloc后，分词器就可以将切分的词传递给Parse，进行语法分析,该函数由sqlite3RunParser调用*/
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
fkey.c文件中的函数声明。如果 OMIT_FOREIGN_KEY没有被定义那所有的函数声明就会被无操作宏所取代。
在这种情况下，外键的功能不可用。如果OMIT_TRIGGER被定义但是OMIT_FOREIGN_KEY没有被定义,那就会有部分的功能不被启用。
在这种情况下，外键会被解析，但是不会提供别的功能。（实施FK约束要求触发器子系统）。
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
可用的错误注射器，应该从0开始编号
*/
#define SQLITE_FAULTINJECTOR_MALLOC     0
#define SQLITE_FAULTINJECTOR_COUNT      1

/*
** The interface to the code in fault.c used for identifying "benign"
** malloc failures. This is only present if SQLITE_OMIT_BUILTIN_TEST
** is not defined.
fault.c中的代码的接口用于识别“良性的”malloc的故障，只有当SQLITE_OMIT_BUILTIN_TEST没有定义时才会出现这种情况。
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
如果SQLITE_ENABLE IOTRACE存在，那么那么全局变量sqlite3的IoTrace是一个指向用于打印I / O跟踪消息的类printf程序。
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
**这些程序只对mem2.c调试内存分配器可用。他们被用来验证不同类型的内存分配是否被系统正确地追踪。
** sqlite3MemdebugSetType() sets the "type" of an allocation to one of
** the MEMTYPE_* macros defined below.  The type must be a bitmask with
** a single bit set.
**sqlite3MemdebugSetType()设置的“类型”分配给下面定义的 MEMTYPE_* 宏之一。这个类型必须是一个单比特集的掩码。
** sqlite3MemdebugHasType() returns true if any of the bits in its second
** argument match the type set by the previous sqlite3MemdebugSetType().
** sqlite3MemdebugHasType() is intended for use inside assert() statements.
** 如果在sqlite3MemdebugHasType()第二个参数的位匹配之前 sqlite3MemdebugSetType()设置的类型，那么sqlite3MemdebugHasType() 返回真
   sqlite3MemdebugHasType()是为了使用assert()内部的语句。
** sqlite3MemdebugNoType() returns true if none of the bits in its second
** argument match the type set by the previous sqlite3MemdebugSetType().
**如果sqlite3MemdebugHasType()第二个参数的位没有一个匹配之前 sqlite3MemdebugSetType()设置的类型，那么sqlite3MemdebugNoType() 返回真
** Perhaps the most important point is the difference between MEMTYPE_HEAP
** and MEMTYPE_LOOKASIDE.  If an allocation is MEMTYPE_LOOKASIDE, that means
** it might have been allocated by lookaside, except the allocation was
** too large or lookaside was already full.  It is important to verify
** that allocations that might have been satisfied by lookaside are not
** passed back to non-lookaside free() routines.  Asserts such as the
** example above are placed on the non-lookaside free() routines to verify
** this constraint. 
**  最重要的一点就是MEMTYPE_HEAP与MEMTYPE_LOOKASIDE的区别。如果一个分配是MEMTYPE_LOOKASIDE，那意味着这可能是由后备分配的，除非分配了太大或后备已经全满了。
确认后备已经被分配充足不会回传给非后备free()程序是非常重要的。声称如上面的示例中放入非后备 free() 程序来验证此约束。
** All of this is no-op for a production build.  It only comes into
** play when the SQLITE_MEMDEBUG compile-time option is used.
*/
#ifdef SQLITE_MEMDEBUG
  void sqlite3MemdebugSetType(void*,u8);
  int sqlite3MemdebugHasType(void*,u8);
  int sqlite3MemdebugNoType(void*,u8);
#else
# define sqlite3MemdebugSetType(X,Y)  /* no-op 空操作*/
# define sqlite3MemdebugHasType(X,Y)  1
# define sqlite3MemdebugNoType(X,Y)   1
#endif
#define MEMTYPE_HEAP       0x01  /* General heap allocations 总的堆式分配*/
#define MEMTYPE_LOOKASIDE  0x02  /* Might have been lookaside memory 可能是后备存储器*/
#define MEMTYPE_SCRATCH    0x04  /* Scratch allocations 从头开始分配*/
#define MEMTYPE_PCACHE     0x08  /* Page cache allocations页面缓存分配 */
#define MEMTYPE_DB         0x10  /* Uses sqlite3DbMalloc, not sqlite_malloc 使用sqlite3DbMalloc，而不是sqlite_malloc*/

#endif /* _SQLITEINT_H_ */
