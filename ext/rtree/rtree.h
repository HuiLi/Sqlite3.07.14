/*
** 2008 May 26
** 解析：
** 2015 November 15
** Source resolve author： Qingkai Hu
** 源码解析作者：户庆凯(英文名字：hiekay)      
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**作者声明版权，这段源代码。在一个法律声明，这里是祝福：
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
         	   愿你做的好事，而不是坏事。
 	           愿你原谅自己，原谅别人。
	           你可以自由共享，一定多付出 少获取。
**
******************************************************************************
**
** This header file is used by programs that want to link against the
** RTREE library.  All it does is declare the sqlite3RtreeInit() interface.
	这个头文件别用于连接R树库的程序
*/
#include "sqlite3.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

int sqlite3RtreeInit(sqlite3 *db);

#ifdef __cplusplus
}  /* extern "C" */
#endif  /* __cplusplus */
