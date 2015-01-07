/*
** 2008 October 28
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
**
** This file contains a no-op memory allocation drivers for use when
** SQLITE_ZERO_MALLOC is defined.  The allocation drivers implemented
** here always fail.  SQLite will not operate with these drivers.  These
** are merely placeholders.  Real drivers must be substituted using
** sqlite3_config() before SQLite will operate.
*/
/*
文件包含一个使用空操作内存分配驱动程序，当SQLITE_ZERO_MALLOC 被宏定义。在这里配置驱动程序总是失败。SQLite将不会使用这些驱动程序。这些都只是占位符。在SQLite操作之前，使用sqlite3_config()会取代真正的驱动程序。
*/
#include "sqliteInt.h"

/*
** This version of the memory allocator is the default.  It is
** used when no other memory allocator is specified using compile-time
** macros.
*/
/*
此版本的内存分配器是默认的。当没有其他内存分配器指定使用时，如果使用SQLITE_ZERO_MALLOC宏定义时，它就会被使用。
*/
#ifdef SQLITE_ZERO_MALLOC

/*
** No-op versions of all memory allocation routines
*/
/*
空操作版本的所有内存分配例程
*/
static void *sqlite3MemMalloc(int nByte){ return 0; }
static void sqlite3MemFree(void *pPrior){ return; }
static void *sqlite3MemRealloc(void *pPrior, int nByte){ return 0; }
static int sqlite3MemSize(void *pPrior){ return 0; }
static int sqlite3MemRoundup(int n){ return n; }
static int sqlite3MemInit(void *NotUsed){ return SQLITE_OK; }
static void sqlite3MemShutdown(void *NotUsed){ return; }

/*
** This routine is the only routine in this file with external linkage.
**
** Populate the low-level memory allocation function pointers in
** sqlite3GlobalConfig.m with pointers to the routines in this file.
*/
/*
该例程是外部链接这个文件的唯一的例程。
在sqlite3GlobalConfig.m与这个文件指针的例程中填充底层内存分配函数指针。
*/
void sqlite3MemSetDefault(void){
  static const sqlite3_mem_methods defaultMethods = {
     sqlite3MemMalloc,
     sqlite3MemFree,
     sqlite3MemRealloc,
     sqlite3MemSize,
     sqlite3MemRoundup,
     sqlite3MemInit,
     sqlite3MemShutdown,
     0
  };
  sqlite3_config(SQLITE_CONFIG_MALLOC, &defaultMethods);
}

#endif /* SQLITE_ZERO_MALLOC */
