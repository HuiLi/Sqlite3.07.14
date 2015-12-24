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
/*这个文件中包含的是一些无操作的内存分配驱动器，当宏SQLITE_ZERO_MALLOC被定义时驱动器才被运用。
这个内存分配驱动器这这个文件中的实现总是失败的。所以 ， SQLite 将不能够运作这些驱动程序。也就是说这些驱动器仅仅是一个占位符，
真正的驱动器是在SQLite操作之前，用sqlite3_config() 来替换的。*/


#include "sqliteInt.h"

/*
** This version of the memory allocator is the default.  It is
** used when no other memory allocator is specified using compile-time
** macros.
*/
/*此版本的内存分配器是默认的。仅用于内存分配器被用于具体的宏编译时。
*/
#ifdef SQLITE_ZERO_MALLOC

/*
** No-op versions of all memory allocation routines
*/
/*
所有内存分配程序的无操作版本。
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
**这个程序仅仅是用于连接外部程序的程序。
** Populate the low-level memory allocation function pointers in
** sqlite3GlobalConfig.m with pointers to the routines in this file.
*/
/*在这个文件中用指向这个程序的指针，用底层内存分配函数指针填充在sqlite3GlobalConfig.m中。
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
