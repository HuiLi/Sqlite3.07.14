/*
** 2010 February 23 2010年2月23日
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**作者本人放弃此代码的版权，在任何有法律的地方，这里给使用SQLite的人以下的祝福：
**    May you do good and not evil. 愿你行善莫行恶。 
**    May you find forgiveness for yourself and forgive others.愿你原谅自己宽恕他人。 
**    May you share freely, never taking more than you give.愿你宽心与人分享，索取不多于你所施予。 
**
*************************************************************************
**
** This file implements routines used to report what compile-time options
** SQLite was built with.
**本文件实现例程用于报告哪一个SQLite编译时间选择被建立。
*/

#ifndef SQLITE_OMIT_COMPILEOPTION_DIAGS /*如果没有定义SQLITE_OMIT_COMPILEOPTION_DIAGS ，则执行下面语句知道文件结尾。如果SQLITE_OMIT_COMPILEOPTION_DIAGS被定义则什么都不编译*/

#include "sqliteInt.h"

/*
** An array of names of all compile-time options.  This array should 
** be sorted A-Z.
**编译时间选择的数组名称，该数组按照A-Z的顺序排序。
** This array looks large, but in a typical installation actually uses
** only a handful of compile-time options, so most times this array is usually
** rather short and uses little memory space.
**这个数组看起来很大,但在一个典型的安装实际使用只有少数编译时的选项,
**所以大多数时候这个数组通常是相当短并且使用小的内存空间。
*/
/*定义编译时间选择数组*/
static const char * const azCompileOpt[] = {

/* These macros are provided to "stringify" the value of the define
** for those options in which the value is meaningful. 
**这些宏提供给函数“stringify”定义的值对这些选项的值是有意义的。*/
#define CTIMEOPT_VAL_(opt) #opt
#define CTIMEOPT_VAL(opt) CTIMEOPT_VAL_(opt)

#ifdef SQLITE_32BIT_ROWID  /*如果  SQLITE_32BIT_ROWID 已经被定义过，则执行下面语句，否则什么都不执行*/
  "32BIT_ROWID",/*32位行编号*/
#endif
#ifdef SQLITE_4_BYTE_ALIGNED_MALLOC /*如果  SQLITE_4_BYTE_ALIGNED_MALLOC 已经被#define定义过，则编译下面语句，否则什么都不执行*/
  "4_BYTE_ALIGNED_MALLOC",/* 4字节内存申请  */
#endif
#ifdef SQLITE_CASE_SENSITIVE_LIKE /*如果  SQLITE_CASE_SENSITIVE_LIKE 已经被#define定义过，则编译下面语句，否则什么都不执行*/
  "CASE_SENSITIVE_LIKE",
#endif
#ifdef SQLITE_CHECK_PAGES /*如果  SQLITE_CHECK_PAGES 已经被#define定义过，则编译下面语句，否则什么都不执行*/
  "CHECK_PAGES",
#endif
#ifdef SQLITE_COVERAGE_TEST /*如果  SQLITE_COVERAGE_TEST 已经被#define定义过，则编译下面语句，否则什么都不执行*/
  "COVERAGE_TEST",
#endif
#ifdef SQLITE_CURDIR /*如果  SQLITE_CURDIR 已经被定义过，则执行下面语句，否则什么都不执行*/
  "CURDIR",
#endif
#ifdef SQLITE_DEBUG /*如果  SQLITE_DEBUG 已经被定义过，则执行下面语句，否则什么都不执行*/
  "DEBUG",
#endif
#ifdef SQLITE_DEFAULT_LOCKING_MODE /*如果  SQLITE_DEFAULT_LOCKING_MODE 已经被定义过，则执行下面语句，否则什么都不执行*/
  "DEFAULT_LOCKING_MODE=" CTIMEOPT_VAL(SQLITE_DEFAULT_LOCKING_MODE),
#endif
#ifdef SQLITE_DISABLE_DIRSYNC /*如果  SQLITE_DISABLE_DIRSYNC 已经被定义过，则执行下面语句，否则什么都不执行*/
  "DISABLE_DIRSYNC",
#endif
#ifdef SQLITE_DISABLE_LFS /*如果  SQLITE_DISABLE_LFS 已经被定义过，则执行下面语句，否则什么都不执行*/
  "DISABLE_LFS",
#endif
#ifdef SQLITE_ENABLE_ATOMIC_WRITE /*如果  SQLITE_ENABLE_ATOMIC_WRITE 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_ATOMIC_WRITE",
#endif
#ifdef SQLITE_ENABLE_CEROD /*如果  SQLITE_ENABLE_CEROD 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_CEROD",
#endif
#ifdef SQLITE_ENABLE_COLUMN_METADATA /*如果  SQLITE_ENABLE_COLUMN_METADATA 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_COLUMN_METADATA",
#endif
#ifdef SQLITE_ENABLE_EXPENSIVE_ASSERT /*如果  SQLITE_ENABLE_EXPENSIVE_ASSERT 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_EXPENSIVE_ASSERT",
#endif
#ifdef SQLITE_ENABLE_FTS1 /*如果  SQLITE_ENABLE_FTS1 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_FTS1",
#endif
#ifdef SQLITE_ENABLE_FTS2 /*如果  SQLITE_ENABLE_FTS2 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_FTS2",
#endif
#ifdef SQLITE_ENABLE_FTS3 /*如果  SQLITE_ENABLE_FTS3 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_FTS3",
#endif
#ifdef SQLITE_ENABLE_FTS3_PARENTHESIS /*如果  SQLITE_ENABLE_FTS3_PARENTHESIS 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_FTS3_PARENTHESIS",
#endif
#ifdef SQLITE_ENABLE_FTS4 /*如果  SQLITE_ENABLE_FTS4 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_FTS4",
#endif
#ifdef SQLITE_ENABLE_ICU /*如果  SQLITE_ENABLE_ICU 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_ICU",
#endif
#ifdef SQLITE_ENABLE_IOTRACE /*如果  SQLITE_ENABLE_IOTRACE 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_IOTRACE",
#endif
#ifdef SQLITE_ENABLE_LOAD_EXTENSION /*如果  SQLITE_ENABLE_LOAD_EXTENSION 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_LOAD_EXTENSION",
#endif
#ifdef SQLITE_ENABLE_LOCKING_STYLE /*如果  SQLITE_ENABLE_LOCKING_STYLE 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_LOCKING_STYLE=" CTIMEOPT_VAL(SQLITE_ENABLE_LOCKING_STYLE),
#endif
#ifdef SQLITE_ENABLE_MEMORY_MANAGEMENT /*如果  SQLITE_ENABLE_MEMORY_MANAGEMENT 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_MEMORY_MANAGEMENT",
#endif
#ifdef SQLITE_ENABLE_MEMSYS3 /*如果  SQLITE_ENABLE_MEMSYS3 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_MEMSYS3",
#endif
#ifdef SQLITE_ENABLE_MEMSYS5 /*如果  SQLITE_ENABLE_MEMSYS5 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_MEMSYS5",
#endif
#ifdef SQLITE_ENABLE_OVERSIZE_CELL_CHECK /*如果  SQLITE_ENABLE_OVERSIZE_CELL_CHECK 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_OVERSIZE_CELL_CHECK",
#endif
#ifdef SQLITE_ENABLE_RTREE /*如果  SQLITE_ENABLE_RTREE 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_RTREE",
#endif
#ifdef SQLITE_ENABLE_STAT3 /*如果  SQLITE_ENABLE_STAT3 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_STAT3",
#endif
#ifdef SQLITE_ENABLE_UNLOCK_NOTIFY /*如果  SQLITE_ENABLE_UNLOCK_NOTIFY 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_UNLOCK_NOTIFY",
#endif
#ifdef SQLITE_ENABLE_UPDATE_DELETE_LIMIT /*如果  SQLITE_ENABLE_UPDATE_DELETE_LIMIT 已经被定义过，则执行下面语句，否则什么都不执行*/
  "ENABLE_UPDATE_DELETE_LIMIT",
#endif
#ifdef SQLITE_HAS_CODEC /*如果  SQLITE_HAS_CODEC 已经被定义过，则执行下面语句，否则什么都不执行*/
  "HAS_CODEC",
#endif
#ifdef SQLITE_HAVE_ISNAN /*如果  SQLITE_HAVE_ISNAN 已经被定义过，则执行下面语句，否则什么都不执行*/
  "HAVE_ISNAN",
#endif
#ifdef SQLITE_HOMEGROWN_RECURSIVE_MUTEX /*如果  SQLITE_HOMEGROWN_RECURSIVE_MUTEX 已经被定义过，则执行下面语句，否则什么都不执行*/
  "HOMEGROWN_RECURSIVE_MUTEX",
#endif
#ifdef SQLITE_IGNORE_AFP_LOCK_ERRORS /*如果  SQLITE_IGNORE_AFP_LOCK_ERRORS 已经被定义过，则执行下面语句，否则什么都不执行*/
  "IGNORE_AFP_LOCK_ERRORS",
#endif
#ifdef SQLITE_IGNORE_FLOCK_LOCK_ERRORS /*如果  SQLITE_IGNORE_FLOCK_LOCK_ERRORS 已经被定义过，则执行下面语句，否则什么都不执行*/
  "IGNORE_FLOCK_LOCK_ERRORS",
#endif
#ifdef SQLITE_INT64_TYPE /*如果  SQLITE_INT64_TYPE 已经被定义过，则执行下面语句，否则什么都不执行*/
  "INT64_TYPE",
#endif
#ifdef SQLITE_LOCK_TRACE /*如果  SQLITE_LOCK_TRACE 已经被定义过，则执行下面语句，否则什么都不执行*/
  "LOCK_TRACE",
#endif
#ifdef SQLITE_MAX_SCHEMA_RETRY /*如果  SQLITE_MAX_SCHEMA_RETRY 已经被定义过，则执行下面语句，否则什么都不执行*/
  "MAX_SCHEMA_RETRY=" CTIMEOPT_VAL(SQLITE_MAX_SCHEMA_RETRY),
#endif
#ifdef SQLITE_MEMDEBUG /*如果  SQLITE_MEMDEBUG 已经被定义过，则执行下面语句，否则什么都不执行*/
  "MEMDEBUG",
#endif
#ifdef SQLITE_MIXED_ENDIAN_64BIT_FLOAT /*如果  SQLITE_MIXED_ENDIAN_64BIT_FLOAT 已经被定义过，则执行下面语句，否则什么都不执行*/
  "MIXED_ENDIAN_64BIT_FLOAT",/*混合字节顺序的64位浮点型*/
#endif
#ifdef SQLITE_NO_SYNC /*如果  SQLITE_NO_SYNC 已经被定义过，则执行下面语句，否则什么都不执行*/
  "NO_SYNC",
#endif
#ifdef SQLITE_OMIT_ALTERTABLE /*如果  SQLITE_OMIT_ALTERTABLE 已经被定义过，则执行下面语句，否则什么都不执行*/
  "OMIT_ALTERTABLE",
#endif
#ifdef SQLITE_OMIT_ANALYZE /*如果  SQLITE_OMIT_ANALYZE 已经被定义过，则执行下面语句，否则什么都不执行*/
  "OMIT_ANALYZE",
#endif
#ifdef SQLITE_OMIT_ATTACH /*如果  SQLITE_OMIT_ATTACH 已经被定义过，则执行下面语句，否则什么都不执行*/
  "OMIT_ATTACH",
#endif
#ifdef SQLITE_OMIT_AUTHORIZATION /*如果  SQLITE_OMIT_AUTHORIZATION 已经被定义过，则执行下面语句，否则什么都不执行*/
  "OMIT_AUTHORIZATION",
#endif
#ifdef SQLITE_OMIT_AUTOINCREMENT /*如果  SQLITE_OMIT_AUTOINCREMENT 已经被定义过，则执行下面语句，否则什么都不执行*/
  "OMIT_AUTOINCREMENT",
#endif
#ifdef SQLITE_OMIT_AUTOINIT /*如果  SQLITE_OMIT_AUTOINIT 已经被定义过，则执行下面语句，否则什么都不执行*/
  "OMIT_AUTOINIT",
#endif
#ifdef SQLITE_OMIT_AUTOMATIC_INDEX /*如果  SQLITE_OMIT_AUTOMATIC_INDEX 已经被定义过，则执行下面语句，否则什么都不执行*/
  "OMIT_AUTOMATIC_INDEX",
#endif
#ifdef SQLITE_OMIT_AUTORESET /*如果  SQLITE_OMIT_AUTORESET 已经被定义过，则执行下面语句，否则什么都不执行*/
  "OMIT_AUTORESET",
#endif
#ifdef SQLITE_OMIT_AUTOVACUUM /*如果  SQLITE_OMIT_AUTOVACUUM 已经被定义过，则执行下面语句，否则什么都不执行*/
  "OMIT_AUTOVACUUM",
#endif
#ifdef SQLITE_OMIT_BETWEEN_OPTIMIZATION /*如果  SQLITE_OMIT_BETWEEN_OPTIMIZATION 已经被定义过，则执行下面语句，否则什么都不执行*/
  "OMIT_BETWEEN_OPTIMIZATION",
#endif
#ifdef SQLITE_OMIT_BLOB_LITERAL /*如果  SQLITE_OMIT_BLOB_LITERAL 已经被定义过，则执行下面语句，否则什么都不执行*/
  "OMIT_BLOB_LITERAL",
#endif
#ifdef SQLITE_OMIT_BTREECOUNT /*如果  SQLITE_OMIT_BTREECOUNT 已经被定义过，则执行下面语句，否则什么都不执行*/
  "OMIT_BTREECOUNT",
#endif
#ifdef SQLITE_OMIT_BUILTIN_TEST /*如果  SQLITE_OMIT_BUILTIN_TEST 已经被定义过，则执行下面语句，否则什么都不执行*/
  "OMIT_BUILTIN_TEST",
#endif
#ifdef SQLITE_OMIT_CAST /*如果  SQLITE_OMIT_CAST 已经被定义过，则执行下面语句，否则什么都不执行*/
  "OMIT_CAST",
#endif
#ifdef SQLITE_OMIT_CHECK /*如果  SQLITE_OMIT_CHECK 已经被定义过，则执行下面语句，否则什么都不执行*/
  "OMIT_CHECK", /*省略核对*/
#endif
/* // redundant
** #ifdef SQLITE_OMIT_COMPILEOPTION_DIAGS
**   "OMIT_COMPILEOPTION_DIAGS",
** #endif
*/
#ifdef SQLITE_OMIT_COMPLETE
  "OMIT_COMPLETE",
#endif
#ifdef SQLITE_OMIT_COMPOUND_SELECT
  "OMIT_COMPOUND_SELECT",
#endif
#ifdef SQLITE_OMIT_DATETIME_FUNCS
  "OMIT_DATETIME_FUNCS",
#endif
#ifdef SQLITE_OMIT_DECLTYPE
  "OMIT_DECLTYPE",
#endif
#ifdef SQLITE_OMIT_DEPRECATED
  "OMIT_DEPRECATED",
#endif
#ifdef SQLITE_OMIT_DISKIO
  "OMIT_DISKIO",
#endif
#ifdef SQLITE_OMIT_EXPLAIN
  "OMIT_EXPLAIN",
#endif
#ifdef SQLITE_OMIT_FLAG_PRAGMAS
  "OMIT_FLAG_PRAGMAS",
#endif
#ifdef SQLITE_OMIT_FLOATING_POINT
  "OMIT_FLOATING_POINT",
#endif
#ifdef SQLITE_OMIT_FOREIGN_KEY
  "OMIT_FOREIGN_KEY",
#endif
#ifdef SQLITE_OMIT_GET_TABLE
  "OMIT_GET_TABLE",
#endif
#ifdef SQLITE_OMIT_INCRBLOB
  "OMIT_INCRBLOB",
#endif
#ifdef SQLITE_OMIT_INTEGRITY_CHECK
  "OMIT_INTEGRITY_CHECK",
#endif
#ifdef SQLITE_OMIT_LIKE_OPTIMIZATION
  "OMIT_LIKE_OPTIMIZATION",
#endif
#ifdef SQLITE_OMIT_LOAD_EXTENSION
  "OMIT_LOAD_EXTENSION",
#endif
#ifdef SQLITE_OMIT_LOCALTIME
  "OMIT_LOCALTIME",
#endif
#ifdef SQLITE_OMIT_LOOKASIDE
  "OMIT_LOOKASIDE",
#endif
#ifdef SQLITE_OMIT_MEMORYDB
  "OMIT_MEMORYDB",
#endif
#ifdef SQLITE_OMIT_MERGE_SORT
  "OMIT_MERGE_SORT",
#endif
#ifdef SQLITE_OMIT_OR_OPTIMIZATION
  "OMIT_OR_OPTIMIZATION",
#endif
#ifdef SQLITE_OMIT_PAGER_PRAGMAS
  "OMIT_PAGER_PRAGMAS",
#endif
#ifdef SQLITE_OMIT_PRAGMA
  "OMIT_PRAGMA",
#endif
#ifdef SQLITE_OMIT_PROGRESS_CALLBACK
  "OMIT_PROGRESS_CALLBACK",
#endif
#ifdef SQLITE_OMIT_QUICKBALANCE
  "OMIT_QUICKBALANCE",
#endif
#ifdef SQLITE_OMIT_REINDEX
  "OMIT_REINDEX",
#endif
#ifdef SQLITE_OMIT_SCHEMA_PRAGMAS
  "OMIT_SCHEMA_PRAGMAS",
#endif
#ifdef SQLITE_OMIT_SCHEMA_VERSION_PRAGMAS
  "OMIT_SCHEMA_VERSION_PRAGMAS",
#endif
#ifdef SQLITE_OMIT_SHARED_CACHE
  "OMIT_SHARED_CACHE",
#endif
#ifdef SQLITE_OMIT_SUBQUERY
  "OMIT_SUBQUERY",
#endif
#ifdef SQLITE_OMIT_TCL_VARIABLE
  "OMIT_TCL_VARIABLE",
#endif
#ifdef SQLITE_OMIT_TEMPDB
  "OMIT_TEMPDB",
#endif
#ifdef SQLITE_OMIT_TRACE
  "OMIT_TRACE",
#endif
#ifdef SQLITE_OMIT_TRIGGER
  "OMIT_TRIGGER",
#endif
#ifdef SQLITE_OMIT_TRUNCATE_OPTIMIZATION
  "OMIT_TRUNCATE_OPTIMIZATION",
#endif
#ifdef SQLITE_OMIT_UTF16
  "OMIT_UTF16",
#endif
#ifdef SQLITE_OMIT_VACUUM
  "OMIT_VACUUM",
#endif
#ifdef SQLITE_OMIT_VIEW
  "OMIT_VIEW",
#endif
#ifdef SQLITE_OMIT_VIRTUALTABLE
  "OMIT_VIRTUALTABLE",
#endif
#ifdef SQLITE_OMIT_WAL
  "OMIT_WAL",
#endif
#ifdef SQLITE_OMIT_WSD
  "OMIT_WSD",
#endif
#ifdef SQLITE_OMIT_XFER_OPT
  "OMIT_XFER_OPT",
#endif
#ifdef SQLITE_PERFORMANCE_TRACE
  "PERFORMANCE_TRACE",
#endif
#ifdef SQLITE_PROXY_DEBUG
  "PROXY_DEBUG",
#endif
#ifdef SQLITE_SECURE_DELETE
  "SECURE_DELETE",
#endif
#ifdef SQLITE_SMALL_STACK
  "SMALL_STACK",
#endif
#ifdef SQLITE_SOUNDEX
  "SOUNDEX",
#endif
#ifdef SQLITE_TCL
  "TCL",
#endif
#ifdef SQLITE_TEMP_STORE
  "TEMP_STORE=" CTIMEOPT_VAL(SQLITE_TEMP_STORE),
#endif
#ifdef SQLITE_TEST
  "TEST",
#endif
#ifdef SQLITE_THREADSAFE
  "THREADSAFE=" CTIMEOPT_VAL(SQLITE_THREADSAFE),
#endif
#ifdef SQLITE_USE_ALLOCA
  "USE_ALLOCA",
#endif
#ifdef SQLITE_ZERO_MALLOC
  "ZERO_MALLOC"
#endif
};

/*
** Given the name of a compile-time option, return true if that option
** was used and false if not.
**给出编译时间选择名称，如果该编译时间选择名称被使用返回true，否者返回false。
**
** The name can optionally begin with "SQLITE_" but the "SQLITE_" prefix
** is not required for a match.
**这个名称可以任意的以"SQLITE_"开头，但是"SQLITE_"前缀不是一个匹配要求。
*/
int sqlite3_compileoption_used(const char *zOptName){
  int i, n;
  /*sqlite3StrNICmp()是比较函数，如果zOptName的前缀是SQLITE_返回0.
  **如果zOptName是以"SQLITE_"开头，则zOptName指向zOptName+7，即忽略"SQLITE_"开头。
  */
  if( sqlite3StrNICmp(zOptName, "SQLITE_", 7)==0 ) zOptName += 7;
  n = sqlite3Strlen30(zOptName);//sqlite3Strlen30（）函数计算字符串zOptName的长度

  /* Since ArraySize(azCompileOpt) is normally in single digits, a
  ** linear search is adequate.  No need for a binary search. 
  **因为ArraySize(azCompileOpt)的大小通常是个位数字的，线性搜索就能满足，不需要二分查找。
  */
  
  for(i=0; i<ArraySize(azCompileOpt); i++){ /*ArraySize返回azCompileOpt数组元素的数量*/
  /*判断zOptName是否等于azCompileOpt[i]。相等即该编译时间选择名称被使用则返回1.否者返回0*/
    if(   (sqlite3StrNICmp(zOptName, azCompileOpt[i], n)==0)
       && ( (azCompileOpt[i][n]==0) || (azCompileOpt[i][n]=='=') ) ) return 1;
  }
  return 0;
}

/*
** Return the N-th compile-time option string.  If N is out of range,
** return a NULL pointer.
**返回第N个编译时间选择字符串。如果N超出范围，返回一个NULL指针。
*/
const char *sqlite3_compileoption_get(int N){
  if( N>=0 && N<ArraySize(azCompileOpt) ){//N大于0并且N小于数组ArraySize长度时，返回第N个编译时间选择字符串
    return azCompileOpt[N];
  }
  return 0;//返回NULL指针
}

#endif /* SQLITE_OMIT_COMPILEOPTION_DIAGS */
