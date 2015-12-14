/*
** 2010 August 30
**
   解析：
** 2015 November 15
** Source resolve author： Qingkai Hu
** 源码解析作者：户庆凯(英文名字：hiekay)      
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
   作者声明版权，这段源代码。在一个法律声明，这里是祝福：
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
             愿你做的好事，而不是坏事。
             愿你原谅自己，原谅别人。
             你可以自由共享，一定多付出 少获取。
**
*************************************************************************
*/

#ifndef _SQLITE3RTREE_H_
#define _SQLITE3RTREE_H_

#include <sqlite3.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sqlite3_rtree_geometry sqlite3_rtree_geometry;

/*
** Register a geometry callback named zGeom that can be used as part of an
** R-Tree geometry query as follows:
   注册名为zGeom几何回调，可以用来作为R树几何查询的一部分
   如下：
**
**   SELECT ... FROM <rtree> WHERE <rtree col> MATCH $zGeom(... params ...)
*/
int sqlite3_rtree_geometry_callback(
  sqlite3 *db,
  const char *zGeom,
#ifdef SQLITE_RTREE_INT_ONLY
  int (*xGeom)(sqlite3_rtree_geometry*, int n, sqlite3_int64 *a, int *pRes),
#else
  int (*xGeom)(sqlite3_rtree_geometry*, int n, double *a, int *pRes),
#endif
  void *pContext
);


/*
** A pointer to a structure of the following type is passed as the first
** argument to callbacks registered using rtree_geometry_callback().
   一个指向下面类型的通过第一个参数来回调注册用于rtree_geometry_callback的指针
*/
struct sqlite3_rtree_geometry {
  void *pContext;                 /* Copy of pContext passed to s_r_g_c() pCountext通过s_r_g_c方法的复制*/
  int nParam;                     /* Size of array aParam[] aParam 数组的大小*/
  double *aParam;                 /* Parameters passed to SQL geom function 通过SQL geom 方法的参数*/
  void *pUser;                    /* Callback implementation user data 实现用户数据的回调*/
  void (*xDelUser)(void *);       /* Called by SQLite to clean up pUser 通过SQLite的调用来清空用户*/
};


#ifdef __cplusplus
}  /* end of the 'extern "C"' block 扩展C块的结束*/
#endif

#endif  /* ifndef _SQLITE3RTREE_H_ _SQLITE3RTREE_H_结束*/
