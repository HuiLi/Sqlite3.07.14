/*
 * 2001 September 15
**
** 解析：
** 2015 November 15
** Source resolve author： Qingkai Hu
** 源码解析作者：户庆凯(英文名字：hiekay)      
**
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
** 作者声明版权，这段源代码。在一个法律声明，这里是祝福：
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**        	 愿你做的好事，而不是坏事。
 	           愿你原谅自己，原谅别人。
	           你可以自由共享，一定多付出 少获取。
**
*************************************************************************
** This file contains code for implementations of the r-tree and r*-tree
** algorithms packaged as an SQLite virtual table module.
** 这个文件包含了用SQLite虚表实现rtree和r*tree算法的代码,
** 即将R树的结构放在虚表中,不仅便于存取,也便于查询.
*/

/*
** Database Format of R-Tree Tables
** R树表的数据库格式
** --------------------------------
**
** The data structure for a single virtual r-tree table is stored in three 
** native SQLite tables declared as follows. In each case, the '%' character
** in the table name is replaced with the user-supplied name of the r-tree
** table.
**  这种数据结构是一个虚拟的R树表被存储在3个本地的被声明的SQLite表如下：
**  在每一种情况下，在表名中的“％”字符被替换为R树表的用户提供的名称。
**
**   例如：
**   CREATE TABLE %_node(nodeno INTEGER PRIMARY KEY, data BLOB)
**   CREATE TABLE %_parent(nodeno INTEGER PRIMARY KEY, parentnode INTEGER)
**   CREATE TABLE %_rowid(rowid INTEGER PRIMARY KEY, nodeno INTEGER)
**
** The data for each node of the r-tree structure is stored in the %_node
** table. For each node that is not the root node of the r-tree, there is
** an entry in the %_parent table associating the node with its parent.
** And for each row of data in the table, there is an entry in the %_rowid
** table that maps from the entries rowid to the id of the node that it
** is stored on.
**
** 对于R树结构的每个节点的数据存储在％节点表。对于每个不是R-树的根节点的节点，则在％父表中有一个关联它与它父节点的条目（记录）。
** 并且针对表中的每行数据中，会有一个条目在%行ID表中，这个表是从条目ID到到它存储在节点的ID。
**
** The root node of an r-tree always exists, even if the r-tree table is
** empty. The nodeno  of the root node is always 1. All other nodes in the
** table must be the same size as the root node. The content of each node
** is formatted as follows:
**
** R树的根结点一直都存在，即使R树表是空的R树也有根节点。
** 根结点的id一直为1.
** 其他所有结点的大小必须与其根结点大小相同。
** 每个结点应该满足下面条件：
**
**   1. If the node is the root node (node 1), then the first 2 bytes
**      of the node contain the tree depth as a big-endian integer.
**      For non-root nodes, the first 2 bytes are left unused.
**
**   1.如果结点是根结点（节点1），则这个节点的前2个字节包含树的深度信息作为一个大端整数，
**     对于非根结点，前两个字节未被使用。
**
**   2. The next 2 bytes contain the number of entries currently 
**      stored in the node.
**
**   2.接下来的2字节包含了当前存在该结点条目（记录）的数量。
**
**   3. The remainder of the node contains the node entries. Each entry
**      consists of a single 8-byte integer followed by an even number
**      of 4-byte coordinates. For leaf nodes the integer is the rowid
**      of a record. For internal nodes it is the node number of a
**      child page.
**
**   3.结点剩下的部分包含了结点记录信息。每个记录包含一个8字节的整数，其后跟随的是偶数个4字节的坐标信息。
**     对于叶节点的整数是一个记录的行ID。对于内部结点整型信息则是子页的结点号。
*/

//此部分是预编译
#if !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_RTREE)
//如果 没有定义 SQLITE_CORE或定义了SQLITE_ENABLE_RTREE 就执行下面语句

/*
** This file contains an implementation of a couple of different variants
** of the r-tree algorithm. See the README file for further details. The 
** same data-structure is used for all, but the algorithms for insert and
	** delete operations vary. The variants used are selected at compile time
	** by defining the following symbols:
**
** 这个文件包含R树算法变体的两种不同实现.README文件中有更详细的说明.
** 两种算法的数据结构是一样的,但是算法的插入和删除操作过程不一样.
** 在编译的时候，通过定义下面两个标志来选择使用的的变量.
*/

/* Either, both or none of the following may be set to activate 
** r*tree variant algorithms.
** 或者，两个或没有以下的可以被设置为激活R *树变体的算法。
*/

//宏定义
#define VARIANT_RSTARTREE_CHOOSESUBTREE 0 //R*树的选择子树算法
#define VARIANT_RSTARTREE_REINSERT      1 //R*树的重插入算法

/* 
** Exactly one of the following must be set to 1.
** 正是以下情况之一必须设置为1。
*/
#define VARIANT_GUTTMAN_QUADRATIC_SPLIT 0  //平方分裂
#define VARIANT_GUTTMAN_LINEAR_SPLIT    0  //线性分裂
#define VARIANT_RSTARTREE_SPLIT         1  //R*树分裂算法
						// \ 相当于连接符号,表示下一行是上一行的继续
#define VARIANT_GUTTMAN_SPLIT \
        (VARIANT_GUTTMAN_LINEAR_SPLIT||VARIANT_GUTTMAN_QUADRATIC_SPLIT)

#if VARIANT_GUTTMAN_QUADRATIC_SPLIT    //平方分裂算法
  #define PickNext QuadraticPickNext   //用于为分裂后的左右结点选择下一条记录
  #define PickSeeds QuadraticPickSeeds //用于为分裂后的左右结点选择第一条记录，选出的是较小的种子:把如果放在一起会产生最多面积浪费的两个矩形选出来
  #define AssignCells splitNodeGuttman //指定单元格
#endif
#if VARIANT_GUTTMAN_LINEAR_SPLIT         //线性分裂算法
  #define PickNext LinearPickNext       //用于为分裂后的左右结点选择下一条插入记录
  #define PickSeeds LinearPickSeeds     //用于为分裂后的左右结点选择第一条记录
  #define AssignCells splitNodeGuttman  //指定单元格
#endif
#if VARIANT_RSTARTREE_SPLIT    //R*树分裂算法,  R*树分裂算法与R树分裂算法不同
  #define AssignCells splitNodeStartree  //指定单元格
#endif

//定义debug 调试方法
#if !defined(NDEBUG) && !defined(SQLITE_DEBUG)
# define NDEBUG 1
#endif

//SQLite 核心文件
#ifndef SQLITE_CORE
  #include "sqlite3ext.h"  //把sqlite3ext.h 文件加载进来   - 执行文件
  SQLITE_EXTENSION_INIT1
#else
  #include "sqlite3.h"	//把sqlite3.h 文件加载进来
#endif

//加载头文件
#include <string.h>
#include <assert.h>

/*
 * 如果没有定义 SQLITE_AMALGAMATION  则把sqlite3rtree.h文件加载进来
 *给sqlite3_int64取别名为 i64
 *给 无符号类char 取别名为  u8
 *给 无符号类int  取别名为 u32
 */
#ifndef SQLITE_AMALGAMATION
#include "sqlite3rtree.h"
typedef sqlite3_int64 i64;    //typedef 起别名
typedef unsigned char u8;     //unsigned 无符号类
typedef unsigned int u32;
#endif

/*  The following macro is used to suppress compiler warnings.
 * 下面的宏用来抑制编译器警告。
 * 这个方法就是定义了编译警告
*/
#ifndef UNUSED_PARAMETER                //只定义不使用 编译时就会有警告  这一句就是为了使用它 避免警告
# define UNUSED_PARAMETER(x) (void)(x)  // 这是宏定义 在编译的时候 把UNUSED_PARAMETER	(x)替换为((void)(x))编译
#endif

//  定义结构体 并起别名

typedef struct Rtree Rtree;     // 定义Rtree 结构体 ，并起别名 Rtree
typedef struct RtreeCursor RtreeCursor;
typedef struct RtreeNode RtreeNode;
typedef struct RtreeCell RtreeCell;
typedef struct RtreeConstraint RtreeConstraint;
typedef struct RtreeMatchArg RtreeMatchArg;
typedef struct RtreeGeomCallback RtreeGeomCallback;
typedef union RtreeCoord RtreeCoord;     //定义联合体 并起别名 RtreeCoord

/* The rtree may have between 1 and RTREE_MAX_DIMENSIONS dimensions. */
/* R树可能有  1 到 最大维数之间
 *
 */
// 定义宏变量 R树最大的维度为 5
#define RTREE_MAX_DIMENSIONS 5

/* Size of hash table Rtree.aHash. This hash table is not expected to
** ever contain very many entries, so a fixed number of buckets is 
** used.
**
** R树的hash表的大小 ，这个hash表预计不会包含很多的记录（条目），所以hash的桶有固定的大小.
*/

//定义 hash 大小为 128
#define HASHSIZE 128

/* 
** An rtree virtual-table object.
** 一个R 树 虚拟表 对象
** R 树结构
*/
struct Rtree {
  sqlite3_vtab base;          //虚拟表sqlite3_vtab 基本结构        声明一个 虚拟表对象 base
  sqlite3 *db;                /* Host database connection  主机数据库连接*/
  int iNodeSize;              /* Size in bytes of each node in the node table 节点表中每个节点的字节大小*/
  int nDim;                   /* Number of dimensions  R树的维数*/
  int nBytesPerCell;          /* Bytes consumed per cell 结点每个单元大小*/
  int iDepth;                 /* Current depth of the r-tree structure 当前R树的高度*/
  char *zDb;                  /* Name of database containing r-tree table 包含R树表的数据库名称*/
  char *zName;                /* Name of r-tree table  R树表的名称*/
  RtreeNode *aHash[HASHSIZE]; /* Hash table of in-memory nodes. 内存节点的hash表链*/
  int nBusy;                  /* Current number of users of this structure  当前使用该结构的用户数*/

  /* List of nodes removed during a CondenseTree operation. List is
  ** linked together via the pointer normally used for hash chains -
  ** RtreeNode.pNext. RtreeNode.iNode stores the depth of the sub-tree 
  ** headed by the node (leaf nodes have RtreeNode.iNode==0).
  **  当对树进行压缩操作时结点链表将移除 . 链表通过用hash链的RtreeNode.pNext指针将每个 结点连接起来，
  **  RtreeNode.iNode存放了已该节点为首的子树的高度信息。
  **（叶结点的RtreeNode.iNode=0）
  */
  RtreeNode *pDeleted;         // R树被删除的父节点的 声明
  int iReinsertHeight;        /* Height of sub-trees Reinsert() has run on 运行重插入函数的子树高度*/

  /* Statements to read/write/delete a record from xxx_node  */
  //从 XXX_node 读、写、删 一个记录的声明（语句）
  sqlite3_stmt *pReadNode;
  sqlite3_stmt *pWriteNode;
  sqlite3_stmt *pDeleteNode;

  /* Statements to read/write/delete a record from xxx_rowid */
  //从 XXX_rowid  读、写、删 一个记录的声明（语句）
  sqlite3_stmt *pReadRowid;
  sqlite3_stmt *pWriteRowid;
  sqlite3_stmt *pDeleteRowid;

  /* Statements to read/write/delete a record from xxx_parent */
  //从 XXX_parent  读、写、删 一个记录的声明（语句）
  sqlite3_stmt *pReadParent;
  sqlite3_stmt *pWriteParent;
  sqlite3_stmt *pDeleteParent;

  int eCoordType;             //坐标数据类型
};

/* Possible values for eCoordType: */
//定义坐标数据类型可能值
#define RTREE_COORD_REAL32 0
#define RTREE_COORD_INT32  1

/*
** If SQLITE_RTREE_INT_ONLY is defined, then this virtual table will
** only deal with integer coordinates.  No floating point operations
** will be done.
** 如果SQLITE_RTREE_INT_ONLY 被定义，则虚表将只支持整型坐标
** 浮点类操作不会被执行
*/
#ifdef SQLITE_RTREE_INT_ONLY
  typedef sqlite3_int64 RtreeDValue;       /* High accuracy coordinate  高精度的坐标*/
  typedef int RtreeValue;                  /* Low accuracy coordinate  低精度的坐标*/
#else
  typedef double RtreeDValue;              /* High accuracy coordinate */
  typedef float RtreeValue;                /* Low accuracy coordinate */
#endif

/*
** The minimum number of cells allowed for a node is a third of the 
** maximum. In Gutman's notation:
**
**     m = M/3
**
** If an R*-tree "Reinsert" operation is required, the same number of
** cells are removed from the overfull node and reinserted into the tree.
**
** 一个节点的包含的最小单元数是最大的1/3 。
**  在Gutman 的注释中是 m=M/3,这是Guttman论文实验测试的最佳参数值，
**  如果R*树允许进行重插入操作，在进行重插入的过程中, 如果该节点的单元数过多，则在该节点移除相同数目的单元，并重新插入到树中
** （结点单元数如果超过最大值,结点将重新插入到树中。）
*/
#define RTREE_MINCELLS(p) ((((p)->iNodeSize-4)/(p)->nBytesPerCell)/3) //获得结点最小单元数
#define RTREE_REINSERT(p) RTREE_MINCELLS(p)    //R树重插入的最小单元
#define RTREE_MAXCELLS 51                      //R树最大单元

/*
** The smallest possible node-size is (512-64)==448 bytes. And the largest
** supported cell size is 48 bytes (8 byte rowid + ten 4 byte coordinates).
** Therefore all non-root nodes must contain at least 3 entries. Since 
** 2^40 is greater than 2^64, an r-tree structure always has a depth of
** 40 or less.
** 最小结点的大小可能为512-64=448byte，最大的单元大小为48byte（8 byte行号+10 4坐标）
** 因此所有非根结点应该包含最少3个记录
** 由于2^40<2^64，所以一个rtree的高度一般为40或小于40
*/
//定义树的最大高度为 40
#define RTREE_MAX_DEPTH 40

/* 
** An rtree cursor object.
** 一个R树结点游标对象结构
*/
struct RtreeCursor {
  sqlite3_vtab_cursor base;          //虚拟表游标
  RtreeNode *pNode;                 /* Node cursor is currently pointing at 结点游标当前指向的节点*/
  int iCell;                        /* Index of current cell in pNode 结点当前单元的索引号*/
  int iStrategy;                    /* Copy of idxNum search parameter 搜索参数索引的复制*/
  int nConstraint;                  /* Number of entries in aConstraint 在aconstraint约束中的记录数*/
  RtreeConstraint *aConstraint;     /* Search constraints. 搜索约束*/
};

//R树坐标 联合体
union RtreeCoord {
  RtreeValue f;	//R树的值
  int i;        //坐标值
};

/*
** The argument is an RtreeCoord. Return the value stored within the RtreeCoord
** formatted as a RtreeDValue (double or int64). This macro assumes that local
** variable pRtree points to the Rtree structure associated with the
** RtreeCoord.
** 参数类型是RtreeCoord. 返回值存储在作为一个R树值（double类型或64位整数类型）的R树坐标中.
** 此宏假定局部变量pRtree指向Rtree结构与RtreeCoord相关联
**
*/
#ifdef SQLITE_RTREE_INT_ONLY   //R树是否只支持整型数据
# define DCOORD(coord) ((RtreeDValue)coord.i)   //定义游标
#else
# define DCOORD(coord) (                           \
    (pRtree->eCoordType==RTREE_COORD_REAL32) ?      \
      ((double)coord.f) :                           \
      ((double)coord.i)                             \
  )
#endif

/*
** A search constraint.
** 搜索约束
*/
struct RtreeConstraint {
  int iCoord;                     /* Index of constrained coordinate 约束坐标的索引*/
  int op;                         /* Constraining operation 约束操作*/
  RtreeDValue rValue;             /* Constraint value. 约束值*/
  int (*xGeom)(sqlite3_rtree_geometry*, int, RtreeDValue*, int*);
  sqlite3_rtree_geometry *pGeom;  /* Constraint callback argument for a MATCH 一个匹配的约束回调参数*/
};

/* Possible values for RtreeConstraint.op R树约束操作的可能值*/
#define RTREE_EQ    0x41
#define RTREE_LE    0x42
#define RTREE_LT    0x43
#define RTREE_GE    0x44
#define RTREE_GT    0x45
#define RTREE_MATCH 0x46

/* 
** An rtree structure node.
** 一个R树节点的数结构
*/
struct RtreeNode {
  RtreeNode *pParent;               /* Parent node  父节点 */
  i64 iNode;                        //结点子树高度
  int nRef;                         //nRef是该结点的引用次数,用来衡量是否需要将该结点从hash链中删除
  int isDirty;                      //isDirty是用来确认该结点是否已被修改.将该结点从hash链删除前如果isDirty=1,则需要将结点数据写入到数据库中
  u8 *zData;                       //zData保存了结点相关信息.如果该结点为根结点,则zData前2byte保存了树的高度信息,如果该结点非根节点,则前2byte未使用.zData[2-3]保存了该结点的单元数.
  RtreeNode *pNext;                 /* Next node in this hash chain  在该hash链的下一个节点*/
};
#define NCELL(pNode) readInt16(&(pNode)->zData[2])

/* 
** Structure to store a deserialized rtree record.
**存储反序列化R树记录的结构
*/
struct RtreeCell {
  i64 iRowid;
  RtreeCoord aCoord[RTREE_MAX_DIMENSIONS*2];
};

/*
** Value for the first field of every RtreeMatchArg object. The MATCH
** operator tests that the first field of a blob operand matches this
** value to avoid operating on invalid blobs (which could cause a segfault).
** 每个rtreematcharg对象第一个区域的值。
** 匹配运算符验证二进制大对象的第一区域是否匹配该值类型，以避免在无效的二进制大对象上进行运算（其能导致段错误）
*/
#define RTREE_GEOMETRY_MAGIC 0x891245AB

/*
** An instance of this structure must be supplied as a blob argument to
** the right-hand-side of an SQL MATCH operator used to constrain an
** r-tree query.
** 该结构的一个实例必须支持一个二进制大对象参数在SQL匹配操作符的右边来约束R树查询
*/
struct RtreeMatchArg {
  u32 magic;                      /* Always RTREE_GEOMETRY_MAGIC 总是 RTREE_GEOMETRY_MAGIC   */
  int (*xGeom)(sqlite3_rtree_geometry *, int, RtreeDValue*, int *);
  void *pContext;
  int nParam;
  RtreeDValue aParam[1];
};

/*
** When a geometry callback is created (see sqlite3_rtree_geometry_callback),
** a single instance of the following structure is allocated. It is used
** as the context for the user-function created by by s_r_g_c(). The object
** is eventually deleted by the destructor mechanism provided by
** sqlite3_create_function_v2() (which is called by s_r_g_c() to create
** the geometry callback function).
** 当一个几何回调被创建(见sqlite3_rtree_geometry_callback)时，后面结构的单个实例将被分配。
** 它将作为 通过s_r_g_c()创建用户函数的上下文，这个对象最终通过sqlite3_create_function_v2()提供的析构函数机制删除
** （调用s_r_g_c()去创建几何回调函数）
*/
struct RtreeGeomCallback {  // R 树几何回调结构
  int (*xGeom)(sqlite3_rtree_geometry*, int, RtreeDValue*, int*);
  void *pContext;
};
/*
** xGeom是一种回调形式,sqlite3_rtree_geometry是一个结构体
** 它提供了SQL函数如果回调的信息,其中包括如何执行和析构
*/

#ifndef MAX
# define MAX(x,y) ((x) < (y) ? (y) : (x))   //求最大值
#endif
#ifndef MIN
# define MIN(x,y) ((x) > (y) ? (y) : (x))    //求最小值
#endif

/*
** Functions to deserialize a 16 bit integer, 32 bit real number and
** 64 bit integer. The deserialized value is returned.
** 该函数是反序列化一个16位的整型，32位真值和64位整型数，并返回反序列化的值
*/
static int readInt16(u8 *p){
  return (p[0]<<8) + p[1];   // 左移符号 <<
}
static void readCoord(u8 *p, RtreeCoord *pCoord){
  u32 i = (
    (((u32)p[0]) << 24) + 
    (((u32)p[1]) << 16) + 
    (((u32)p[2]) <<  8) + 
    (((u32)p[3]) <<  0)
  );
  *(u32 *)pCoord = i;
}
static i64 readInt64(u8 *p){
  return (
    (((i64)p[0]) << 56) + 
    (((i64)p[1]) << 48) + 
    (((i64)p[2]) << 40) + 
    (((i64)p[3]) << 32) + 
    (((i64)p[4]) << 24) + 
    (((i64)p[5]) << 16) + 
    (((i64)p[6]) <<  8) + 
    (((i64)p[7]) <<  0)
  );
}

/*
** Functions to serialize a 16 bit integer, 32 bit real number and
** 64 bit integer. The value returned is the number of bytes written
** to the argument buffer (always 2, 4 and 8 respectively).
** 该函数是序列化16位整型,32位真值和64位整型数.返回值是写在参数流中的字节数（分别始终为2，4和8）.
*/
static int writeInt16(u8 *p, int i){
  p[0] = (i>> 8)&0xFF;
  p[1] = (i>> 0)&0xFF;
  return 2;
}
static int writeCoord(u8 *p, RtreeCoord *pCoord){
  u32 i;
  assert( sizeof(RtreeCoord)==4 );
  assert( sizeof(u32)==4 );
  i = *(u32 *)pCoord;
  p[0] = (i>>24)&0xFF;
  p[1] = (i>>16)&0xFF;
  p[2] = (i>> 8)&0xFF;
  p[3] = (i>> 0)&0xFF;
  return 4;
}
static int writeInt64(u8 *p, i64 i){
  p[0] = (i>>56)&0xFF;
  p[1] = (i>>48)&0xFF;
  p[2] = (i>>40)&0xFF;
  p[3] = (i>>32)&0xFF;
  p[4] = (i>>24)&0xFF;
  p[5] = (i>>16)&0xFF;
  p[6] = (i>> 8)&0xFF;
  p[7] = (i>> 0)&0xFF;
  return 8;
}

/*
** Increment the reference count of node p.
** 递增P节点的引用计数 每次加 1
*/
static void nodeReference(RtreeNode *p){
  if( p ){
    p->nRef++;
  }
}

/*
** Clear the content of node p (set all bytes to 0x00).
** 清除结点p的内容
*/
static void nodeZero(Rtree *pRtree, RtreeNode *p){
  memset(&p->zData[2], 0, pRtree->iNodeSize-2);
  p->isDirty = 1;
}

/*
** Given a node number iNode, return the corresponding key to use
** in the Rtree.aHash table.
** 输入一个结点数字类型的iNode，返回其在相应的R树hash表中的关键值
*/
static int nodeHash(i64 iNode){
  return (
    (iNode>>56) ^ (iNode>>48) ^ (iNode>>40) ^ (iNode>>32) ^  //^异或 符号
    (iNode>>24) ^ (iNode>>16) ^ (iNode>> 8) ^ (iNode>> 0)
  ) % HASHSIZE;
}

/*
** Search the node hash table for node iNode. If found, return a pointer
** to it. Otherwise, return 0.
** 在节点hash表中搜索节点iNode的位置，如果找到，就返回一个指向它的指针，否则返回0
*/
static RtreeNode *nodeHashLookup(Rtree *pRtree, i64 iNode){ //查找结点iNode的hash值
  RtreeNode *p;
  for(p=pRtree->aHash[nodeHash(iNode)]; p && p->iNode!=iNode; p=p->pNext);
  return p;
}

/*
** Add node pNode to the node hash table.
** 将结点pNode插入到pRtree的hash表中
*/
static void nodeHashInsert(Rtree *pRtree, RtreeNode *pNode){
  int iHash;
  assert( pNode->pNext==0 );
  iHash = nodeHash(pNode->iNode);
  pNode->pNext = pRtree->aHash[iHash];
  pRtree->aHash[iHash] = pNode;
}

/*
** Remove node pNode from the node hash table.
** 从hash表中删除结点pNode
*/
static void nodeHashDelete(Rtree *pRtree, RtreeNode *pNode){
  RtreeNode **pp;        // 二级指针 
  if( pNode->iNode!=0 ){
    pp = &pRtree->aHash[nodeHash(pNode->iNode)];
    for( ; (*pp)!=pNode; pp = &(*pp)->pNext){ assert(*pp); }
    *pp = pNode->pNext;
    pNode->pNext = 0;
  }
}

/*
** Allocate and return new r-tree node. Initially, (RtreeNode.iNode==0),
** indicating that node has not yet been assigned a node number. It is
** assigned a node number when nodeWrite() is called to write the
** node contents out to the database.
** 分配并返回一个新的R树结点。
** 初始时(RtreeNode.iNode==0)指明该节点没有被分配一个节点，
** 该节点会被分配一个节点当节点写方法被调用用于写节点内容到数据库的时候
**
**引用函数void *memset(void *s, int ch, unsigned n);
**　将s所指向的某一块内存中的每个字节的内容全部设置为ch指定的ASCII值,
**　　块的大小由第三个参数指定,这个函数通常为新申请的内存做初始化工作,
**　　其返回值为指向S的指针。
**
**
**
*/
static RtreeNode *nodeNew(Rtree *pRtree, RtreeNode *pParent){
  RtreeNode *pNode;
  pNode = (RtreeNode *)sqlite3_malloc(sizeof(RtreeNode) + pRtree->iNodeSize);  //sqlite3_malloc 申请内存空间
  if( pNode ){
    memset(pNode, 0, sizeof(RtreeNode) + pRtree->iNodeSize); //void *memset(void *s, int ch, unsigned n)
    pNode->zData = (u8 *)&pNode[1];
    pNode->nRef = 1;
    pNode->pParent = pParent;
    pNode->isDirty = 1;
    nodeReference(pParent);
  }
  return pNode;
}

/*
** Obtain a reference to an r-tree node.
** 获得一个指向R树结点的指针
*/
static int
nodeAcquire(
  Rtree *pRtree,             /* R-tree structure  R树结构*/
  i64 iNode,                 /* Node number to load 节点数量加载 */
  RtreeNode *pParent,        /* Either the parent node or NULL 父节点或没有节点 */
  RtreeNode **ppNode         /* OUT: Acquired node  输出：获得的节点*/
){
  int rc;
  int rc2 = SQLITE_OK;
  RtreeNode *pNode;

  /* Check if the requested node is already in the hash table. If so,
  ** increase its reference count and return it.
  ** 检查所请求的结点是否已在hash表中，
  ** 如果在，则增加指向它的指针数量并返回
  **引用的方法：
  **void assert( int expression )
  ** assert的作用是现计算表达式 expression ，如果其值为假（即为0），那么它先向stderr打印一条出错信息，然后通过调用 abort 来终止程序运行。
  */
  if( (pNode = nodeHashLookup(pRtree, iNode)) ){
    assert( !pParent || !pNode->pParent || pNode->pParent==pParent );  //void assert( int expression )
    if( pParent && !pNode->pParent ){
      nodeReference(pParent);
      pNode->pParent = pParent;
    }
    pNode->nRef++;
    *ppNode = pNode;
    return SQLITE_OK; 
  }

  sqlite3_bind_int64(pRtree->pReadNode, 1, iNode); //邦定64位参数 方法
  rc = sqlite3_step(pRtree->pReadNode);            //执行sql语句
  if( rc==SQLITE_ROW ){
    const u8 *zBlob = sqlite3_column_blob(pRtree->pReadNode, 0);  //sqlite3_step返回SQLITE_ROW，需要用sqlite3_column_blob取blob类型的数据 
    if( pRtree->iNodeSize==sqlite3_column_bytes(pRtree->pReadNode, 0) ){       //用sqlite3_column_bytes取bytes类型的数据
      pNode = (RtreeNode *)sqlite3_malloc(sizeof(RtreeNode)+pRtree->iNodeSize);
      if( !pNode ){
        rc2 = SQLITE_NOMEM;
      }else{
        pNode->pParent = pParent;
        pNode->zData = (u8 *)&pNode[1];
        pNode->nRef = 1;
        pNode->iNode = iNode;
        pNode->isDirty = 0;
        pNode->pNext = 0;
        memcpy(pNode->zData, zBlob, pRtree->iNodeSize);  //void *memcpy(void *dest, const void *src, size_t n);
                                                          //memcpy函数的功能是从源src所指的内存地址的起始位置开始拷贝n个字节到目标dest所指的内存地址的起始位置中。
        nodeReference(pParent);                         
      }
    }
  }
  rc = sqlite3_reset(pRtree->pReadNode);     //重置过程的执行 ,过程将回到没有执行之前的状态，绑定的参数不会变化
  if( rc==SQLITE_OK ) rc = rc2;

  /* If the root node was just loaded, set pRtree->iDepth to the height
  ** of the r-tree structure. A height of zero means all data is stored on
  ** the root node. A height of one means the children of the root node
  ** are the leaves, and so on. If the depth as specified on the root node
  ** is greater than RTREE_MAX_DEPTH, the r-tree structure must be corrupt.
  ** 如果根结点只是加载进去，设置pRtree->iDepth 为R树的高度。
  ** 高度为0表示所有的数据都存放在根结点，高度为1表示根结点的孩子节点都为叶结点，以此类推。
  ** 如果指定根结点 的高度超过了最大值(RTREE_MAX_DEPTH)，R树肯定会崩溃。
  */
  if( pNode && iNode==1 ){
    pRtree->iDepth = readInt16(pNode->zData); //反序列化
    if( pRtree->iDepth>RTREE_MAX_DEPTH ){
      rc = SQLITE_CORRUPT_VTAB;
    }
  }

  /* If no error has occurred so far, check if the "number of entries"
  ** field on the node is too large. If so, set the return code to 
  ** SQLITE_CORRUPT_VTAB.
  ** 如果到目前还没错误发生，检查结点的记录数是否太大，
  ** 如果是这样，设置返回代码为SQLITE_CORRUPT_VTAB
  */
  if( pNode && rc==SQLITE_OK ){
    if( NCELL(pNode)>((pRtree->iNodeSize-4)/pRtree->nBytesPerCell) ){   //NCELL ： readIn16 
      rc = SQLITE_CORRUPT_VTAB;
    }
  }

  if( rc==SQLITE_OK ){
    if( pNode!=0 ){
      nodeHashInsert(pRtree, pNode);    //pnode 插入pRtree 
    }else{
      rc = SQLITE_CORRUPT_VTAB;
    }
    *ppNode = pNode;
  }else{
    sqlite3_free(pNode);    //释放pNode
    *ppNode = 0;
  }

  return rc;
}

/*
** Overwrite cell iCell of node pNode with the contents of pCell.
** 用pCell中的内容重写pNode的iCell单元的内容
*/
static void nodeOverwriteCell(
  Rtree *pRtree, 
  RtreeNode *pNode,  
  RtreeCell *pCell, 
  int iCell
){
  int ii;
  u8 *p = &pNode->zData[4 + pRtree->nBytesPerCell*iCell];
  p += writeInt64(p, pCell->iRowid);         //序列化64位
  for(ii=0; ii<(pRtree->nDim*2); ii++){
    p += writeCoord(p, &pCell->aCoord[ii]);    //序列化32位
  }
  pNode->isDirty = 1;
}

/*
** Remove cell the cell with index iCell from node pNode.
** 移除结点pNode中索引为iCell的单元
*/
static void nodeDeleteCell(Rtree *pRtree, RtreeNode *pNode, int iCell){ 
  u8 *pDst = &pNode->zData[4 + pRtree->nBytesPerCell*iCell];
  u8 *pSrc = &pDst[pRtree->nBytesPerCell];
  int nByte = (NCELL(pNode) - iCell - 1) * pRtree->nBytesPerCell;  //NCELL ： readIn16 
  memmove(pDst, pSrc, nByte);                           //void *memmove( void* dest, const void* src, size_t count );
                                                        //头文件：<string.h> 功能：由src所指内存区域复制count个字节到dest所指内存区域。
  writeInt16(&pNode->zData[2], NCELL(pNode)-1);
  pNode->isDirty = 1;
}

/*
** Insert the contents of cell pCell into node pNode. If the insert
** is successful, return SQLITE_OK.
** 插入pCell中的内容到结点pNode，如果插入成功，返回SQLITE_OK
** If there is not enough free space in pNode, return SQLITE_FULL.
** 如果结点pNode没有足够的空间，已满，返回SQLITE_FULL
*/
static int
nodeInsertCell(
  Rtree *pRtree, 
  RtreeNode *pNode, 
  RtreeCell *pCell 
){
  int nCell;                    /* Current number of cells in pNode  pNode节点当前的单元数*/
  int nMaxCell;                 /* Maximum number of cells for pNode pNode节点最大的单元数*/

  nMaxCell = (pRtree->iNodeSize-4)/pRtree->nBytesPerCell;
  nCell = NCELL(pNode);        //NCELL ： readIn16 
 
  assert( nCell<=nMaxCell );       //条件为错，就终止程序
  if( nCell<nMaxCell ){
    nodeOverwriteCell(pRtree, pNode, pCell, nCell);
    writeInt16(&pNode->zData[2], nCell+1);
    pNode->isDirty = 1;
  }

  return (nCell==nMaxCell);
}

/*
** If the node is dirty, write it out to the database.
** 如果结点是脏的，将结点写入到数据库
*/
static int
nodeWrite(Rtree *pRtree, RtreeNode *pNode){
  int rc = SQLITE_OK;
  if( pNode->isDirty ){
    sqlite3_stmt *p = pRtree->pWriteNode;
    if( pNode->iNode ){
      sqlite3_bind_int64(p, 1, pNode->iNode);
    }else{
      sqlite3_bind_null(p, 1);
    }
    sqlite3_bind_blob(p, 2, pNode->zData, pRtree->iNodeSize, SQLITE_STATIC);
    sqlite3_step(p);
    pNode->isDirty = 0;
    rc = sqlite3_reset(p);
    if( pNode->iNode==0 && rc==SQLITE_OK ){
      pNode->iNode = sqlite3_last_insert_rowid(pRtree->db);  //返回最近插入的记录的id 
      nodeHashInsert(pRtree, pNode);
    }
  }
  return rc;
}

/*
** Release a reference to a node. If the node is dirty and the reference
** count drops to zero, the node data is written to the database.
** 释放指向一个节点的指针.如果该节点是脏的，且指向它的指针数为0，则该节点数据已经写入到数据库
*/
static int
nodeRelease(Rtree *pRtree, RtreeNode *pNode){
  int rc = SQLITE_OK;
  if( pNode ){
    assert( pNode->nRef>0 );
    pNode->nRef--;
    if( pNode->nRef==0 ){
      if( pNode->iNode==1 ){
        pRtree->iDepth = -1;
      }
      if( pNode->pParent ){
        rc = nodeRelease(pRtree, pNode->pParent);
      }
      if( rc==SQLITE_OK ){
        rc = nodeWrite(pRtree, pNode);
      }
      nodeHashDelete(pRtree, pNode);
      sqlite3_free(pNode);
    }
  }
  return rc;
}

/*
** Return the 64-bit integer value associated with cell iCell of
** node pNode. If pNode is a leaf node, this is a rowid. If it is
** an internal node, then the 64-bit integer is a child page number.
** 返回关联了节点pNode的iCell单元的64位的整型rowid的值。
** 如果pNode结点是叶结点，则该值为行号rowid，如果pNode为内部结点，那么该值为其子结点的页号
*/
static i64 nodeGetRowid(
  Rtree *pRtree, 
  RtreeNode *pNode,
  int iCell
){
  assert( iCell<NCELL(pNode) );
  return readInt64(&pNode->zData[4 + pRtree->nBytesPerCell*iCell]);
}

/*
** Return coordinate iCoord from cell iCell in node pNode.
** 返回pNode结点中iCell单元的iCoord坐标
*/
static void nodeGetCoord(
  Rtree *pRtree, 
  RtreeNode *pNode, 
  int iCell,
  int iCoord,
  RtreeCoord *pCoord           /* Space to write result to 保存单元的坐标*/
){
  readCoord(&pNode->zData[12 + pRtree->nBytesPerCell*iCell + 4*iCoord], pCoord);
}

/*
** Deserialize cell iCell of node pNode. Populate the structure pointed
** to by pCell with the results.
** 反序列化pNode节点的iCell单元.
** 把结果填写到pNode指向的结构
*/
static void nodeGetCell(
  Rtree *pRtree, 
  RtreeNode *pNode, 
  int iCell,
  RtreeCell *pCell
){
  int ii;
  pCell->iRowid = nodeGetRowid(pRtree, pNode, iCell);
  for(ii=0; ii<pRtree->nDim*2; ii++){       //nDim  维度
    nodeGetCoord(pRtree, pNode, iCell, ii, &pCell->aCoord[ii]);
  }
}


/* Forward declaration for the function that does the work of
** the virtual table module xCreate() and xConnect() methods.
** 对虚表模型xCreate() 和xConnect()方法如何工作的前置声明
*/
static int rtreeInit(
  sqlite3 *, void *, int, const char *const*, sqlite3_vtab **, char **, int
);
//虚表模块Create()和Connect()方法均调用rtreeInit()来实现其过程
/* 
** Rtree virtual table module xCreate method.
** R树虚表模的xCreate 方法
*/
static int rtreeCreate(
  sqlite3 *db,
  void *pAux,         //void *则为“无类型指针”，void *可以指向任何类型的数据
  int argc, const char *const*argv,       //const  常类型
  sqlite3_vtab **ppVtab,                 // 虚拟表
  char **pzErr
){
  return rtreeInit(db, pAux, argc, argv, ppVtab, pzErr, 1);
}

/* 
** Rtree virtual table module xConnect method.
** R树虚表模的xConnect 方法
*/
static int rtreeConnect(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVtab,   
  char **pzErr
){
  return rtreeInit(db, pAux, argc, argv, ppVtab, pzErr, 0);
}

/*
** Increment the r-tree reference count.
** R树引用数量+1
*/
static void rtreeReference(Rtree *pRtree){
  pRtree->nBusy++;      
}

/*
** Decrement the r-tree reference count. When the reference count reaches
** zero the structure is deleted.
** R树引用数量减1，当引用为0时，则删除该结构
*/
static void rtreeRelease(Rtree *pRtree){
  pRtree->nBusy--;
  if( pRtree->nBusy==0 ){
    sqlite3_finalize(pRtree->pReadNode);      // 销毁资源 
    sqlite3_finalize(pRtree->pWriteNode);
    sqlite3_finalize(pRtree->pDeleteNode);
    sqlite3_finalize(pRtree->pReadRowid);
    sqlite3_finalize(pRtree->pWriteRowid);
    sqlite3_finalize(pRtree->pDeleteRowid);
    sqlite3_finalize(pRtree->pReadParent);
    sqlite3_finalize(pRtree->pWriteParent);
    sqlite3_finalize(pRtree->pDeleteParent);
    sqlite3_free(pRtree);
  }
}

/* 
** Rtree virtual table module xDisconnect method.
** R树虚表模块释放连接xDisconnect方法
*/
static int rtreeDisconnect(sqlite3_vtab *pVtab){
  rtreeRelease((Rtree *)pVtab);
  return SQLITE_OK;
}

/* 
** Rtree virtual table module xDestroy method.
** R树虚表模块销毁xDestory方法
*/
static int rtreeDestroy(sqlite3_vtab *pVtab){
  Rtree *pRtree = (Rtree *)pVtab;
  int rc;
  char *zCreate = sqlite3_mprintf(       //格式化
    "DROP TABLE '%q'.'%q_node';"
    "DROP TABLE '%q'.'%q_rowid';"
    "DROP TABLE '%q'.'%q_parent';",
    pRtree->zDb, pRtree->zName, 
    pRtree->zDb, pRtree->zName,
    pRtree->zDb, pRtree->zName
  );
  if( !zCreate ){
    rc = SQLITE_NOMEM;
  }else{
    rc = sqlite3_exec(pRtree->db, zCreate, 0, 0, 0);  //执行sql 语句 
    sqlite3_free(zCreate);     //  释放对象防止内存泄露
  }
  if( rc==SQLITE_OK ){
    rtreeRelease(pRtree);
  }

  return rc;
}

/* 
** Rtree virtual table module xOpen method.
** R树虚表模块打开xOpen方法
*/
static int rtreeOpen(sqlite3_vtab *pVTab, sqlite3_vtab_cursor **ppCursor){
  int rc = SQLITE_NOMEM;
  RtreeCursor *pCsr;

  pCsr = (RtreeCursor *)sqlite3_malloc(sizeof(RtreeCursor));
  if( pCsr ){
    memset(pCsr, 0, sizeof(RtreeCursor));
    pCsr->base.pVtab = pVTab;
    rc = SQLITE_OK;
  }
  *ppCursor = (sqlite3_vtab_cursor *)pCsr;

  return rc;
}


/*
** Free the RtreeCursor.aConstraint[] array and its contents.
** 释放RtreeCursor约束数组和其内容
*/
static void freeCursorConstraints(RtreeCursor *pCsr){
  if( pCsr->aConstraint ){
    int i;                        /* Used to iterate through constraint array 被用于通过约束数组的迭代计数器*/
    for(i=0; i<pCsr->nConstraint; i++){
      sqlite3_rtree_geometry *pGeom = pCsr->aConstraint[i].pGeom;    
      if( pGeom ){  
        if( pGeom->xDelUser ) pGeom->xDelUser(pGeom->pUser);
        sqlite3_free(pGeom);
      }
    }
    sqlite3_free(pCsr->aConstraint);
    pCsr->aConstraint = 0;
  }
}

/* 
** Rtree virtual table module xClose method.
** R树虚表模块关闭xClose方法
*/
static int rtreeClose(sqlite3_vtab_cursor *cur){
  Rtree *pRtree = (Rtree *)(cur->pVtab);
  int rc;
  RtreeCursor *pCsr = (RtreeCursor *)cur;
  freeCursorConstraints(pCsr);
  rc = nodeRelease(pRtree, pCsr->pNode);
  sqlite3_free(pCsr);
  return rc;
}

/*
** Rtree virtual table module xEof method.
** R树虚表模块结束xEOF方法
** Return non-zero if the cursor does not currently point to a valid 
** record (i.e if the scan has finished), or zero otherwise.
** 如果当前指针没有指向一个有效的记录(已完成扫描)则返回非空值,否则返回0
*/
static int rtreeEof(sqlite3_vtab_cursor *cur){
  RtreeCursor *pCsr = (RtreeCursor *)cur;
  return (pCsr->pNode==0);
}

/*
** The r-tree constraint passed as the second argument to this function is
** guaranteed to be a MATCH constraint.
** R树约束通过该函数的第二个参数保证其成为一个匹配约束
*/
static int testRtreeGeom(
  Rtree *pRtree,                  /* R-Tree object R树对象*/
  RtreeConstraint *pConstraint,   /* MATCH constraint to test 测试约束的匹配 */
  RtreeCell *pCell,               /* Cell to test 测试的单元*/
  int *pbRes                      /* OUT: Test result  输出：测试结果*/
){
  int i;
  RtreeDValue aCoord[RTREE_MAX_DIMENSIONS*2];
  int nCoord = pRtree->nDim*2;

  assert( pConstraint->op==RTREE_MATCH );
  assert( pConstraint->pGeom );

  for(i=0; i<nCoord; i++){
    aCoord[i] = DCOORD(pCell->aCoord[i]);
  }
  return pConstraint->xGeom(pConstraint->pGeom, nCoord, aCoord, pbRes);
}

/* 
** Cursor pCursor currently points to a cell in a non-leaf page.
** Set *pbEof to true if the sub-tree headed by the cell is filtered
** (excluded) by the constraints in the pCursor->aConstraint[] 
** array, or false otherwise.
** 游标pCursor当前指向一个非叶子结点页中的单元，
** 如果子树中单元是被约束集
** pCursor->aConstraint[]滤过的，则设置pbEof为true，否则设为false

** Return SQLITE_OK if successful or an SQLite error code if an error
** occurs within a geometry callback.
** 如果成功返回SQLITE_OK，如果是一个几何回调内的错误返回SQLite error
*/
static int testRtreeCell(Rtree *pRtree, RtreeCursor *pCursor, int *pbEof){
  RtreeCell cell;
  int ii;
  int bRes = 0;
  int rc = SQLITE_OK;

  nodeGetCell(pRtree, pCursor->pNode, pCursor->iCell, &cell);
  for(ii=0; bRes==0 && ii<pCursor->nConstraint; ii++){
    RtreeConstraint *p = &pCursor->aConstraint[ii];
    RtreeDValue cell_min = DCOORD(cell.aCoord[(p->iCoord>>1)*2]);
    RtreeDValue cell_max = DCOORD(cell.aCoord[(p->iCoord>>1)*2+1]);

    assert(p->op==RTREE_LE || p->op==RTREE_LT || p->op==RTREE_GE 
        || p->op==RTREE_GT || p->op==RTREE_EQ || p->op==RTREE_MATCH
    );

    switch( p->op ){
      case RTREE_LE: case RTREE_LT: 
        bRes = p->rValue<cell_min; 
        break;

      case RTREE_GE: case RTREE_GT: 
        bRes = p->rValue>cell_max; 
        break;

      case RTREE_EQ:
        bRes = (p->rValue>cell_max || p->rValue<cell_min);
        break;

      default: {
        assert( p->op==RTREE_MATCH );
        rc = testRtreeGeom(pRtree, p, &cell, &bRes);
        bRes = !bRes;
        break;
      }
    }
  }

  *pbEof = bRes;
  return rc;
}

/* 
** Test if the cell that cursor pCursor currently points to
** would be filtered (excluded) by the constraints in the 
** pCursor->aConstraint[] array. If so, set *pbEof to true before
** returning. If the cell is not filtered (excluded) by the constraints,
** set pbEof to zero.
** 验证pCursor指针所指向的单元是否满足约束集pCursor->aConstraint[] array约束条件，（通过pCursor->aConstraint[]约束集来过滤当前游标所指向的单元是否通过测试）
** 如果满足返回之前设置*pbEof为真，如果这个单元通过约束没有被排除设为0
** Return SQLITE_OK if successful or an SQLite error code if an error
** occurs within a geometry callback.
** 如果成功返回SQLITE_OK,如果是一个几何回调内的错误返回SQLite error
** This function assumes that the cell is part of a leaf node.
** 这个函数假设单元是叶结点的一部分
*/
static int testRtreeEntry(Rtree *pRtree, RtreeCursor *pCursor, int *pbEof){
  RtreeCell cell;
  int ii;
  *pbEof = 0;

  nodeGetCell(pRtree, pCursor->pNode, pCursor->iCell, &cell);
  for(ii=0; ii<pCursor->nConstraint; ii++){
    RtreeConstraint *p = &pCursor->aConstraint[ii];
    RtreeDValue coord = DCOORD(cell.aCoord[p->iCoord]);
    int res;
    assert(p->op==RTREE_LE || p->op==RTREE_LT || p->op==RTREE_GE 
        || p->op==RTREE_GT || p->op==RTREE_EQ || p->op==RTREE_MATCH
    );
    switch( p->op ){
      case RTREE_LE: res = (coord<=p->rValue); break;
      case RTREE_LT: res = (coord<p->rValue);  break;
      case RTREE_GE: res = (coord>=p->rValue); break;
      case RTREE_GT: res = (coord>p->rValue);  break;
      case RTREE_EQ: res = (coord==p->rValue); break;
      default: {
        int rc;
        assert( p->op==RTREE_MATCH );
        rc = testRtreeGeom(pRtree, p, &cell, &res);
        if( rc!=SQLITE_OK ){
          return rc;
        }
        break;
      }
    }

    if( !res ){
      *pbEof = 1;
      return SQLITE_OK;
    }
  }

  return SQLITE_OK;
}

/*
** Cursor pCursor currently points at a node that heads a sub-tree of
** height iHeight (if iHeight==0, then the node is a leaf). Descend
** to point to the left-most cell of the sub-tree that matches the 
** configured constraints.
** 游标PCursor当前指向一个高度为IHeight的节点（如果IHeight=0 则这个节点就是一个叶子节点），
** 游标向下指向匹配这个配置约束的子树的最左单元
*/
static int descendToCell(
  Rtree *pRtree, 
  RtreeCursor *pCursor, 
  int iHeight,
  int *pEof                 /* OUT: Set to true if cannot descend */
){
  int isEof;
  int rc;
  int ii;
  RtreeNode *pChild;
  sqlite3_int64 iRowid;

  RtreeNode *pSavedNode = pCursor->pNode;
  int iSavedCell = pCursor->iCell;

  assert( iHeight>=0 );

  if( iHeight==0 ){
    rc = testRtreeEntry(pRtree, pCursor, &isEof);
  }else{
    rc = testRtreeCell(pRtree, pCursor, &isEof);
  }
  if( rc!=SQLITE_OK || isEof || iHeight==0 ){
    goto descend_to_cell_out;
  }

  iRowid = nodeGetRowid(pRtree, pCursor->pNode, pCursor->iCell);
  rc = nodeAcquire(pRtree, iRowid, pCursor->pNode, &pChild);
  if( rc!=SQLITE_OK ){
    goto descend_to_cell_out;
  }

  nodeRelease(pRtree, pCursor->pNode);
  pCursor->pNode = pChild;
  isEof = 1;
  for(ii=0; isEof && ii<NCELL(pChild); ii++){
    pCursor->iCell = ii;
    rc = descendToCell(pRtree, pCursor, iHeight-1, &isEof);
    if( rc!=SQLITE_OK ){
      goto descend_to_cell_out;
    }
  }

  if( isEof ){
    assert( pCursor->pNode==pChild );
    nodeReference(pSavedNode);
    nodeRelease(pRtree, pChild);
    pCursor->pNode = pSavedNode;
    pCursor->iCell = iSavedCell;
  }

descend_to_cell_out:
  *pEof = isEof;
  return rc;
}

/*
** One of the cells in node pNode is guaranteed to have a 64-bit 
** integer value equal to iRowid. Return the index of this cell.
**  pNode节点的其中一个单元被保证有一个64位的整数值等于iRowid，并返回该单元的索引
*/
static int nodeRowidIndex(
  Rtree *pRtree, 
  RtreeNode *pNode, 
  i64 iRowid,
  int *piIndex
){
  int ii;
  int nCell = NCELL(pNode);
  for(ii=0; ii<nCell; ii++){
    if( nodeGetRowid(pRtree, pNode, ii)==iRowid ){
      *piIndex = ii;
      return SQLITE_OK;
    }
  }
  return SQLITE_CORRUPT_VTAB;
}

/*
** Return the index of the cell containing a pointer to node pNode
** in its parent. If pNode is the root node, return -1.
** 返回一个包含指向其父节点中pNode指针单元的索引，如果pNode为根结点则返回-1
*/
static int nodeParentIndex(Rtree *pRtree, RtreeNode *pNode, int *piIndex){
  RtreeNode *pParent = pNode->pParent;
  if( pParent ){
    return nodeRowidIndex(pRtree, pParent, pNode->iNode, piIndex);
  }
  *piIndex = -1;
  return SQLITE_OK;
}

/* 
** Rtree virtual table module xNext method.
** R树虚拟表模块的xNext()函数
*/
static int rtreeNext(sqlite3_vtab_cursor *pVtabCursor){
  Rtree *pRtree = (Rtree *)(pVtabCursor->pVtab);
  RtreeCursor *pCsr = (RtreeCursor *)pVtabCursor;
  int rc = SQLITE_OK;

  /* RtreeCursor.pNode must not be NULL. If is is NULL, then this cursor is
  ** already at EOF. It is against the rules to call the xNext() method of
  ** a cursor that has already reached EOF.
  ** RtreeCursor.pNode必须不为空,如果为空,表明指针已经到达结尾,该方法已结束.
  ** 它违反了如果这个游标已经到达结尾的xNext（）方法的执行规则
  */
  assert( pCsr->pNode );

  if( pCsr->iStrategy==1 ){
    /* This "scan" is a direct lookup by rowid. There is no next entry. 
    ** 这个扫描是通过rowid的直接扫描,不存在下一条记录.
    */
    nodeRelease(pRtree, pCsr->pNode);
    pCsr->pNode = 0;
  }else{
    /* Move to the next entry that matches the configured constraints. 
    ** 移动到下一条满足配置约束的记录.
    */
    int iHeight = 0;
    while( pCsr->pNode ){
      RtreeNode *pNode = pCsr->pNode;
      int nCell = NCELL(pNode);
      for(pCsr->iCell++; pCsr->iCell<nCell; pCsr->iCell++){
        int isEof;
        rc = descendToCell(pRtree, pCsr, iHeight, &isEof);
        if( rc!=SQLITE_OK || !isEof ){
          return rc;
        }
      }
      pCsr->pNode = pNode->pParent;
      rc = nodeParentIndex(pRtree, pNode, &pCsr->iCell);
      if( rc!=SQLITE_OK ){
        return rc;
      }
      nodeReference(pCsr->pNode);
      nodeRelease(pRtree, pNode);
      iHeight++;
    }
  }

  return rc;
}

/* 
** Rtree virtual table module xRowid method.
** R树虚拟表模块的Rowid()方法
*/
static int rtreeRowid(sqlite3_vtab_cursor *pVtabCursor, sqlite_int64 *pRowid){
  Rtree *pRtree = (Rtree *)pVtabCursor->pVtab;
  RtreeCursor *pCsr = (RtreeCursor *)pVtabCursor;

  assert(pCsr->pNode);
  *pRowid = nodeGetRowid(pRtree, pCsr->pNode, pCsr->iCell);

  return SQLITE_OK;
}

/* 
** Rtree virtual table module xColumn method.
** R树虚表模块方法xColumn 方法
*/
static int rtreeColumn(sqlite3_vtab_cursor *cur, sqlite3_context *ctx, int i){
  Rtree *pRtree = (Rtree *)cur->pVtab;
  RtreeCursor *pCsr = (RtreeCursor *)cur;

  if( i==0 ){
    i64 iRowid = nodeGetRowid(pRtree, pCsr->pNode, pCsr->iCell);
    sqlite3_result_int64(ctx, iRowid);
  }else{
    RtreeCoord c;
    nodeGetCoord(pRtree, pCsr->pNode, pCsr->iCell, i-1, &c);
#ifndef SQLITE_RTREE_INT_ONLY
    if( pRtree->eCoordType==RTREE_COORD_REAL32 ){
      sqlite3_result_double(ctx, c.f);
    }else
#endif
    {
      assert( pRtree->eCoordType==RTREE_COORD_INT32 );
      sqlite3_result_int(ctx, c.i);
    }
  }

  return SQLITE_OK;
}

/* 
** Use nodeAcquire() to obtain the leaf node containing the record with 
** rowid iRowid. If successful, set *ppLeaf to point to the node and
** return SQLITE_OK. If there is no such record in the table, set
** *ppLeaf to 0 and return SQLITE_OK. If an error occurs, set *ppLeaf
** to zero and return an SQLite error code.
** 用iRowid调用nodeAcquire()函数得到包含记录的叶结点，
** 如果成功，设置*ppLeaf指向该结点并返回SQLITE_OK，
** 如果表中没有该记录，设置*ppLeaf指向0并返回SQLITE_OK，如果有错误产生，则设置*ppLeaf为zero，并返回SQLite error
*/
static int findLeafNode(Rtree *pRtree, i64 iRowid, RtreeNode **ppLeaf){
  int rc;
  *ppLeaf = 0;
  sqlite3_bind_int64(pRtree->pReadRowid, 1, iRowid);
  if( sqlite3_step(pRtree->pReadRowid)==SQLITE_ROW ){
    i64 iNode = sqlite3_column_int64(pRtree->pReadRowid, 0);
    rc = nodeAcquire(pRtree, iNode, 0, ppLeaf);
    sqlite3_reset(pRtree->pReadRowid);
  }else{
    rc = sqlite3_reset(pRtree->pReadRowid);
  }
  return rc;
}

/*
** This function is called to configure the RtreeConstraint object passed
** as the second argument for a MATCH constraint. The value passed as the
** first argument to this function is the right-hand operand to the MATCH
** operator.
** 这个函数被调用配置这个R树约束对象通过作为第二个参数为了一个匹配约束（R树约束对象作为这个函数的第二个参数被调用去匹配该约束）
** 通过该函数的第一个参数的值去匹配右边运算对象的操作
*/
static int deserializeGeometry(sqlite3_value *pValue, RtreeConstraint *pCons){
  RtreeMatchArg *p;
  sqlite3_rtree_geometry *pGeom;
  int nBlob;

  /* Check that value is actually a blob. */
    // 检查值实际上是一个二进制大对象
  if( sqlite3_value_type(pValue)!=SQLITE_BLOB ) return SQLITE_ERROR;

  /* Check that the blob is roughly the right size. */
  //检查这个二进制大对象大致是右子树的大小
  nBlob = sqlite3_value_bytes(pValue);
  if( nBlob<(int)sizeof(RtreeMatchArg) 
   || ((nBlob-sizeof(RtreeMatchArg))%sizeof(RtreeDValue))!=0
  ){
    return SQLITE_ERROR;
  }

  pGeom = (sqlite3_rtree_geometry *)sqlite3_malloc(
      sizeof(sqlite3_rtree_geometry) + nBlob
  );
  if( !pGeom ) return SQLITE_NOMEM;
  memset(pGeom, 0, sizeof(sqlite3_rtree_geometry));
  p = (RtreeMatchArg *)&pGeom[1];

  memcpy(p, sqlite3_value_blob(pValue), nBlob);
  if( p->magic!=RTREE_GEOMETRY_MAGIC 
   || nBlob!=(int)(sizeof(RtreeMatchArg) + (p->nParam-1)*sizeof(RtreeDValue))
  ){
    sqlite3_free(pGeom);
    return SQLITE_ERROR;
  }

  pGeom->pContext = p->pContext;
  pGeom->nParam = p->nParam;
  pGeom->aParam = p->aParam;

  pCons->xGeom = p->xGeom;
  pCons->pGeom = pGeom;
  return SQLITE_OK;
}

/* 
** Rtree virtual table module xFilter method.
** R树虚表模块过滤器（xFilter）方法
*/
static int rtreeFilter(
  sqlite3_vtab_cursor *pVtabCursor, 
  int idxNum, const char *idxStr,
  int argc, sqlite3_value **argv
){
  Rtree *pRtree = (Rtree *)pVtabCursor->pVtab;
  RtreeCursor *pCsr = (RtreeCursor *)pVtabCursor;

  RtreeNode *pRoot = 0;
  int ii;
  int rc = SQLITE_OK;

  rtreeReference(pRtree);

  freeCursorConstraints(pCsr);
  pCsr->iStrategy = idxNum;

  if( idxNum==1 ){
    /* Special case - lookup by rowid. */
	  //特例：通过rowid 查找
    RtreeNode *pLeaf;        /* Leaf on which the required cell resides 叶子存在哪个必须的单元*/
    i64 iRowid = sqlite3_value_int64(argv[0]);
    rc = findLeafNode(pRtree, iRowid, &pLeaf);
    pCsr->pNode = pLeaf; 
    if( pLeaf ){
      assert( rc==SQLITE_OK );
      rc = nodeRowidIndex(pRtree, pLeaf, iRowid, &pCsr->iCell);
    }
  }else{
    /* Normal case - r-tree scan. Set up the RtreeCursor.aConstraint array 
    ** with the configured constraints. 
    **正常情况--R树扫描   通过匹配约束设定R树游标约束数组
    */
    if( argc>0 ){
      pCsr->aConstraint = sqlite3_malloc(sizeof(RtreeConstraint)*argc);
      pCsr->nConstraint = argc;
      if( !pCsr->aConstraint ){
        rc = SQLITE_NOMEM;
      }else{
        memset(pCsr->aConstraint, 0, sizeof(RtreeConstraint)*argc);
        assert( (idxStr==0 && argc==0)
                || (idxStr && (int)strlen(idxStr)==argc*2) );
        for(ii=0; ii<argc; ii++){
          RtreeConstraint *p = &pCsr->aConstraint[ii];
          p->op = idxStr[ii*2];
          p->iCoord = idxStr[ii*2+1]-'a';
          if( p->op==RTREE_MATCH ){
            /* A MATCH operator. The right-hand-side must be a blob that
            ** can be cast into an RtreeMatchArg object. One created using
            ** an sqlite3_rtree_geometry_callback() SQL user function.
            ** 一个匹配操作。最右边必须是一个二进制大对象才能转换成一个RtreeMatchArg对象。
            **用sqlite3_rtree_geometry_callback()用户方法创建一个RtreeMatchArg对象。
            */
            rc = deserializeGeometry(argv[ii], p);
            if( rc!=SQLITE_OK ){
              break;
            }
          }else{
#ifdef SQLITE_RTREE_INT_ONLY
            p->rValue = sqlite3_value_int64(argv[ii]);
#else
            p->rValue = sqlite3_value_double(argv[ii]);
#endif
          }
        }
      }
    }
  
    if( rc==SQLITE_OK ){
      pCsr->pNode = 0;
      rc = nodeAcquire(pRtree, 1, 0, &pRoot);
    }
    if( rc==SQLITE_OK ){
      int isEof = 1;
      int nCell = NCELL(pRoot);
      pCsr->pNode = pRoot;
      for(pCsr->iCell=0; rc==SQLITE_OK && pCsr->iCell<nCell; pCsr->iCell++){
        assert( pCsr->pNode==pRoot );
        rc = descendToCell(pRtree, pCsr, pRtree->iDepth, &isEof);
        if( !isEof ){
          break;
        }
      }
      if( rc==SQLITE_OK && isEof ){
        assert( pCsr->pNode==pRoot );
        nodeRelease(pRtree, pRoot);
        pCsr->pNode = 0;
      }
      assert( rc!=SQLITE_OK || !pCsr->pNode || pCsr->iCell<NCELL(pCsr->pNode) );
    }
  }

  rtreeRelease(pRtree);
  return rc;
}

/*
** Rtree virtual table module xBestIndex method. There are three
** table scan strategies to choose from (in order from most to 
** least desirable):
** R树虚拟表模块xBestIdex方法
** 有三种表扫描策略可选择（为了从多到少可取）
**   idxNum     idxStr        Strategy
**   ------------------------------------------------
**     1        Unused        Direct lookup by rowid.
**     2        See below     R-tree query or full-table scan.
**   ------------------------------------------------
**   索引                        id字符串                          策略
**   --------------------------------------------------
**   1          没有                                 通过rowid直接查找
**   2          查看下面                         R树查找或全表扫描
**
** If strategy 1 is used, then idxStr is not meaningful. If strategy
** 2 is used, idxStr is formatted to contain 2 bytes for each 
** constraint used. The first two bytes of idxStr correspond to 
** the constraint in sqlite3_index_info.aConstraintUsage[] with
** (argvIndex==1) etc.
** 如果策略1被使用，那么idxstr没有意义的，如果策略2被使用，idxstr被格式化成2个字节供约束使用
** idxStr前2个字节通过（参数argvIndex==1 等等）对应于sqlite3_index_info.aConstrainUsage数组中的约束
** The first of each pair of bytes in idxStr identifies the constraint
** operator as follows:
** idxStr每对字节的第一个2字节 识别这个约束操作     如下：
**   Operator    Byte Value
**   ----------------------
**      =        0x41 ('A')
**     <=        0x42 ('B')
**      <        0x43 ('C')
**     >=        0x44 ('D')
**      >        0x45 ('E')
**   MATCH       0x46 ('F')
**   ----------------------
**
** The second of each pair of bytes identifies the coordinate column
** to which the constraint applies. The leftmost coordinate column
** is 'a', the second from the left 'b' etc.
** idxStr每对字节的第二个2字节识别哪种约束被使用在坐标列上，最左边的列为a，接下来的为b，等等
*/
static int rtreeBestIndex(sqlite3_vtab *tab, sqlite3_index_info *pIdxInfo){
  int rc = SQLITE_OK;
  int ii;

  int iIdx = 0;
  char zIdxStr[RTREE_MAX_DIMENSIONS*8+1];
  memset(zIdxStr, 0, sizeof(zIdxStr));
  UNUSED_PARAMETER(tab);

  assert( pIdxInfo->idxStr==0 );
  for(ii=0; ii<pIdxInfo->nConstraint && iIdx<(int)(sizeof(zIdxStr)-1); ii++){
    struct sqlite3_index_constraint *p = &pIdxInfo->aConstraint[ii];

    if( p->usable && p->iColumn==0 && p->op==SQLITE_INDEX_CONSTRAINT_EQ ){
      /* We have an equality constraint on the rowid. Use strategy 1. */
      //如果在rowid上有等价约束,使用策略1
      int jj;
      for(jj=0; jj<ii; jj++){
        pIdxInfo->aConstraintUsage[jj].argvIndex = 0;
        pIdxInfo->aConstraintUsage[jj].omit = 0;
      }
      pIdxInfo->idxNum = 1;
      pIdxInfo->aConstraintUsage[ii].argvIndex = 1;
      pIdxInfo->aConstraintUsage[jj].omit = 1;

      /* This strategy involves a two rowid lookups on an B-Tree structures
      ** and then a linear search of an R-Tree node. This should be 
      ** considered almost as quick as a direct rowid lookup (for which 
      ** sqlite uses an internal cost of 0.0).
      ** 这个策略包含了一种在B树结构中两个rowid查找方法和在R树结点的线性搜索方法，这种直接在rowid搜索是非常迅速的。
      */
      pIdxInfo->estimatedCost = 10.0;
      return SQLITE_OK;
    }

    if( p->usable && (p->iColumn>0 || p->op==SQLITE_INDEX_CONSTRAINT_MATCH) ){
      u8 op;
      switch( p->op ){
        case SQLITE_INDEX_CONSTRAINT_EQ: op = RTREE_EQ; break;
        case SQLITE_INDEX_CONSTRAINT_GT: op = RTREE_GT; break;
        case SQLITE_INDEX_CONSTRAINT_LE: op = RTREE_LE; break;
        case SQLITE_INDEX_CONSTRAINT_LT: op = RTREE_LT; break;
        case SQLITE_INDEX_CONSTRAINT_GE: op = RTREE_GE; break;
        default:
          assert( p->op==SQLITE_INDEX_CONSTRAINT_MATCH );
          op = RTREE_MATCH; 
          break;
      }
      zIdxStr[iIdx++] = op;
      zIdxStr[iIdx++] = p->iColumn - 1 + 'a';
      pIdxInfo->aConstraintUsage[ii].argvIndex = (iIdx/2);
      pIdxInfo->aConstraintUsage[ii].omit = 1;
    }
  }

  pIdxInfo->idxNum = 2;
  pIdxInfo->needToFreeIdxStr = 1;
  if( iIdx>0 && 0==(pIdxInfo->idxStr = sqlite3_mprintf("%s", zIdxStr)) ){
    return SQLITE_NOMEM;
  }
  assert( iIdx>=0 );
  pIdxInfo->estimatedCost = (2000000.0 / (double)(iIdx + 1));
  return rc;
}

/*
** Return the N-dimensional volumn of the cell stored in *p.
** 返回存储在p中的单元的的N维列数据
*/
static RtreeDValue cellArea(Rtree *pRtree, RtreeCell *p){
  RtreeDValue area = (RtreeDValue)1;
  int ii;
  for(ii=0; ii<(pRtree->nDim*2); ii+=2){
    area = (area * (DCOORD(p->aCoord[ii+1]) - DCOORD(p->aCoord[ii])));
  }
  return area;
}

/*
** Return the margin length of cell p. The margin length is the sum
** of the objects size in each dimension.
** 返回p单元的边界长度，该边界长度是所有对象在每个空间尺寸大小的总和。
*/
static RtreeDValue cellMargin(Rtree *pRtree, RtreeCell *p){
  RtreeDValue margin = (RtreeDValue)0;
  int ii;
  for(ii=0; ii<(pRtree->nDim*2); ii+=2){
    margin += (DCOORD(p->aCoord[ii+1]) - DCOORD(p->aCoord[ii]));
  }
  return margin;
}

/*
** Store the union of cells p1 and p2 in p1.
** 将p1和p2单元的联合存储到p1(将单元p1和p2合并到单元p1,需要计算维度坐标值)
**
*/
static void cellUnion(Rtree *pRtree, RtreeCell *p1, RtreeCell *p2){
  int ii;
  if( pRtree->eCoordType==RTREE_COORD_REAL32 ){
    for(ii=0; ii<(pRtree->nDim*2); ii+=2){
      p1->aCoord[ii].f = MIN(p1->aCoord[ii].f, p2->aCoord[ii].f);
      p1->aCoord[ii+1].f = MAX(p1->aCoord[ii+1].f, p2->aCoord[ii+1].f);
    }
  }else{
    for(ii=0; ii<(pRtree->nDim*2); ii+=2){
      p1->aCoord[ii].i = MIN(p1->aCoord[ii].i, p2->aCoord[ii].i);
      p1->aCoord[ii+1].i = MAX(p1->aCoord[ii+1].i, p2->aCoord[ii+1].i);
    }
  }
}

/*
** Return true if the area covered by p2 is a subset of the area covered
** by p1. False otherwise.
** 如果p2区域是区域p1的子集，则返回true,否则返回false
*/
static int cellContains(Rtree *pRtree, RtreeCell *p1, RtreeCell *p2){
  int ii;
  int isInt = (pRtree->eCoordType==RTREE_COORD_INT32);
  for(ii=0; ii<(pRtree->nDim*2); ii+=2){
    RtreeCoord *a1 = &p1->aCoord[ii];
    RtreeCoord *a2 = &p2->aCoord[ii];
    if( (!isInt && (a2[0].f<a1[0].f || a2[1].f>a1[1].f)) 
     || ( isInt && (a2[0].i<a1[0].i || a2[1].i>a1[1].i)) 
    ){
      return 0;
    }
  }
  return 1;
}

/*
** Return the amount cell p would grow by if it were unioned with pCell.
** 返回单元p如果把pCell包含进来合并后的区域增长值,用于计算合并代价
*/
static RtreeDValue cellGrowth(Rtree *pRtree, RtreeCell *p, RtreeCell *pCell){
  RtreeDValue area;
  RtreeCell cell;
  memcpy(&cell, p, sizeof(RtreeCell));
  area = cellArea(pRtree, &cell);
  cellUnion(pRtree, &cell, pCell);
  return (cellArea(pRtree, &cell)-area);
}

#if VARIANT_RSTARTREE_CHOOSESUBTREE || VARIANT_RSTARTREE_SPLIT
static RtreeDValue cellOverlap(
  Rtree *pRtree, 
  RtreeCell *p, 
  RtreeCell *aCell, 
  int nCell, 
  int iExclude
){
  int ii;
  RtreeDValue overlap = 0.0;
  for(ii=0; ii<nCell; ii++){
#if VARIANT_RSTARTREE_CHOOSESUBTREE
    if( ii!=iExclude )
#else
    assert( iExclude==-1 );
    UNUSED_PARAMETER(iExclude);
#endif
    {
      int jj;
      RtreeDValue o = (RtreeDValue)1;
      for(jj=0; jj<(pRtree->nDim*2); jj+=2){
        RtreeDValue x1, x2;

        x1 = MAX(DCOORD(p->aCoord[jj]), DCOORD(aCell[ii].aCoord[jj]));
        x2 = MIN(DCOORD(p->aCoord[jj+1]), DCOORD(aCell[ii].aCoord[jj+1]));

        if( x2<x1 ){
          o = 0.0;
          break;
        }else{
          o = o * (x2-x1);
        }
      }
      overlap += o;
    }
  }
  return overlap;
}
#endif

#if VARIANT_RSTARTREE_CHOOSESUBTREE  //R*树选择子树算法
static RtreeDValue cellOverlapEnlargement(
        //返回插入单元pInsert后单元p和单元aCell区域的重叠值增量
  Rtree *pRtree, 
  RtreeCell *p, 
  RtreeCell *pInsert, 
  RtreeCell *aCell, 
  int nCell, 
  int iExclude
){
  RtreeDValue before, after;
  before = cellOverlap(pRtree, p, aCell, nCell, iExclude);
  cellUnion(pRtree, p, pInsert);
  after = cellOverlap(pRtree, p, aCell, nCell, iExclude);
  return (after-before);
}
#endif


/*
** This function implements the ChooseLeaf algorithm from Gutman[84].
** ChooseSubTree in r*tree terminology.
** 这个函数实现了Gutman[84]中的选择叶结点算法
** ChooseSubTree 是R*树 中的术语
*/
static int ChooseLeaf(
  Rtree *pRtree,               /* Rtree table */
  RtreeCell *pCell,            /* Cell to insert into rtree */
  int iHeight,                 /* Height of sub-tree rooted at pCell */
  RtreeNode **ppLeaf           /* OUT: Selected leaf page */
){
  int rc;
  int ii;
  RtreeNode *pNode;
  rc = nodeAcquire(pRtree, 1, 0, &pNode);

  for(ii=0; rc==SQLITE_OK && ii<(pRtree->iDepth-iHeight); ii++){
    int iCell;
    sqlite3_int64 iBest = 0;

    RtreeDValue fMinGrowth = 0.0;
    RtreeDValue fMinArea = 0.0;
#if VARIANT_RSTARTREE_CHOOSESUBTREE
    RtreeDValue fMinOverlap = 0.0;
    RtreeDValue overlap;
#endif

    int nCell = NCELL(pNode);
    RtreeCell cell;
    RtreeNode *pChild;

    RtreeCell *aCell = 0;

#if VARIANT_RSTARTREE_CHOOSESUBTREE
    if( ii==(pRtree->iDepth-1) ){
      int jj;
      aCell = sqlite3_malloc(sizeof(RtreeCell)*nCell);
      if( !aCell ){
        rc = SQLITE_NOMEM;
        nodeRelease(pRtree, pNode);
        pNode = 0;
        continue;
      }
      for(jj=0; jj<nCell; jj++){
        nodeGetCell(pRtree, pNode, jj, &aCell[jj]);
      }
    }
#endif

    /* Select the child node which will be enlarged the least if pCell
    ** is inserted into it. Resolve ties by choosing the entry with
    ** the smallest area.
    ** 选择如果插入pCell单元后扩大最小的子结点，选择增量最小区域的情况来分裂结点
    */
    for(iCell=0; iCell<nCell; iCell++){
      int bBest = 0;
      RtreeDValue growth;
      RtreeDValue area;
      nodeGetCell(pRtree, pNode, iCell, &cell);
      growth = cellGrowth(pRtree, &cell, pCell);
      area = cellArea(pRtree, &cell);

#if VARIANT_RSTARTREE_CHOOSESUBTREE
      if( ii==(pRtree->iDepth-1) ){
        overlap = cellOverlapEnlargement(pRtree,&cell,pCell,aCell,nCell,iCell);
      }else{
        overlap = 0.0;
      }
      if( (iCell==0) 
       || (overlap<fMinOverlap) 
       || (overlap==fMinOverlap && growth<fMinGrowth)
       || (overlap==fMinOverlap && growth==fMinGrowth && area<fMinArea)
      ){
        bBest = 1;
        fMinOverlap = overlap;
      }
#else
      if( iCell==0||growth<fMinGrowth||(growth==fMinGrowth && area<fMinArea) ){
        bBest = 1;
      }
#endif
      if( bBest ){
        fMinGrowth = growth;
        fMinArea = area;
        iBest = cell.iRowid;
      }
    }

    sqlite3_free(aCell);
    rc = nodeAcquire(pRtree, iBest, pNode, &pChild);
    nodeRelease(pRtree, pNode);
    pNode = pChild;
  }

  *ppLeaf = pNode;
  return rc;
}

/*
** A cell with the same content as pCell has just been inserted into
** the node pNode. This function updates the bounding box cells in
** all ancestor elements.
** 与pCell有相同内容的单元插入到结点pNode中，这个函数在原来所有的元素上更新了所有的边界框单元。
*/
static int AdjustTree(
  Rtree *pRtree,                    /* Rtree table R树表*/
  RtreeNode *pNode,                 /* Adjust ancestry of this node.  适合匹配这个节点的父节点 */
  RtreeCell *pCell                  /* This cell was just inserted 这个单元仅仅是被插入的*/
){
  RtreeNode *p = pNode;
  while( p->pParent ){
    RtreeNode *pParent = p->pParent;
    RtreeCell cell;
    int iCell;

    if( nodeParentIndex(pRtree, p, &iCell) ){
      return SQLITE_CORRUPT_VTAB;
    }

    nodeGetCell(pRtree, pParent, iCell, &cell);
    if( !cellContains(pRtree, &cell, pCell) ){
      cellUnion(pRtree, &cell, pCell);
      nodeOverwriteCell(pRtree, pParent, &cell, iCell);
    }
    p = pParent;
  }
  return SQLITE_OK;
}

/*
** Write mapping (iRowid->iNode) to the <rtree>_rowid table.
** 把映射（iRowid->iNode）写到<rtree>_rowid 表中。
*/
static int rowidWrite(Rtree *pRtree, sqlite3_int64 iRowid, sqlite3_int64 iNode){
  sqlite3_bind_int64(pRtree->pWriteRowid, 1, iRowid);
  sqlite3_bind_int64(pRtree->pWriteRowid, 2, iNode);
  sqlite3_step(pRtree->pWriteRowid);
  return sqlite3_reset(pRtree->pWriteRowid);
}

/*
** Write mapping (iNode->iPar) to the <rtree>_parent table.
** 把(iRowid->iNode) 的映射到写入 <rtree>_rowid 表,将R树结构存放到虚表中
*/
static int parentWrite(Rtree *pRtree, sqlite3_int64 iNode, sqlite3_int64 iPar){
  sqlite3_bind_int64(pRtree->pWriteParent, 1, iNode);
  sqlite3_bind_int64(pRtree->pWriteParent, 2, iPar);
  sqlite3_step(pRtree->pWriteParent);
  return sqlite3_reset(pRtree->pWriteParent);
}

static int rtreeInsertCell(Rtree *, RtreeNode *, RtreeCell *, int);
// 将单元pCell插入到结点pNode中，结点pNode是高度为iHeight子树的首结点

#if VARIANT_GUTTMAN_LINEAR_SPLIT
/*
** Implementation of the linear variant of the PickNext() function from
** Guttman[84].
** 实现Guttman[84]中方法PickNext()的变形 ----线性PickNext()方法
*/
static RtreeCell *LinearPickNext(
  Rtree *pRtree,
  RtreeCell *aCell, 
  int nCell, 
  RtreeCell *pLeftBox, 
  RtreeCell *pRightBox,
  int *aiUsed
){
  int ii;
  for(ii=0; aiUsed[ii]; ii++);
  aiUsed[ii] = 1;
  return &aCell[ii];
}

/*
** Implementation of the linear variant of the PickSeeds() function from
** Guttman[84].
** 实现Guttman[84]中方法PickSeeds()的变形 ----线性PickSeeds()方法
*/
static void LinearPickSeeds(
  Rtree *pRtree,
  RtreeCell *aCell, 
  int nCell, 
  int *piLeftSeed, 
  int *piRightSeed
){
  int i;
  int iLeftSeed = 0;
  int iRightSeed = 1;
  RtreeDValue maxNormalInnerWidth = (RtreeDValue)0;

  /* Pick two "seed" cells from the array of cells. The algorithm used
  ** here is the LinearPickSeeds algorithm from Gutman[1984]. The 
  ** indices of the two seed cells in the array are stored in local
  ** variables iLeftSeek and iRightSeed.
  ** 从单元数组中选择两个种子单元，该算法被用到这是Gutman[1984]中线性PickSeeds算法中的算法。
  **这个数组中的两个子单元的指数存储在本地变量iLeftSeek和iRightSeed中
  */
  for(i=0; i<pRtree->nDim; i++){
    RtreeDValue x1 = DCOORD(aCell[0].aCoord[i*2]);
    RtreeDValue x2 = DCOORD(aCell[0].aCoord[i*2+1]);
    RtreeDValue x3 = x1;
    RtreeDValue x4 = x2;
    int jj;

    int iCellLeft = 0;
    int iCellRight = 0;

    for(jj=1; jj<nCell; jj++){
      RtreeDValue left = DCOORD(aCell[jj].aCoord[i*2]);
      RtreeDValue right = DCOORD(aCell[jj].aCoord[i*2+1]);

      if( left<x1 ) x1 = left;
      if( right>x4 ) x4 = right;
      if( left>x3 ){
        x3 = left;
        iCellRight = jj;
      }
      if( right<x2 ){
        x2 = right;
        iCellLeft = jj;
      }
    }

    if( x4!=x1 ){
      RtreeDValue normalwidth = (x3 - x2) / (x4 - x1);
      if( normalwidth>maxNormalInnerWidth ){
        iLeftSeed = iCellLeft;
        iRightSeed = iCellRight;
      }
    }
  }

  *piLeftSeed = iLeftSeed;
  *piRightSeed = iRightSeed;
}
#endif /* VARIANT_GUTTMAN_LINEAR_SPLIT 变形guttman线性分裂方法*/

#if VARIANT_GUTTMAN_QUADRATIC_SPLIT
/*
** Implementation of the quadratic variant of the PickNext() function from
** Guttman[84].
** 实现Guttman[84]PickNext（）方法的变形方法-----平方分裂算法
*/
static RtreeCell *QuadraticPickNext(
  Rtree *pRtree,
  RtreeCell *aCell, 
  int nCell, 
  RtreeCell *pLeftBox, 
  RtreeCell *pRightBox,
  int *aiUsed
){
  #define FABS(a) ((a)<0.0?-1.0*(a):(a))

  int iSelect = -1;
  RtreeDValue fDiff;
  int ii;
  for(ii=0; ii<nCell; ii++){
    if( aiUsed[ii]==0 ){
      RtreeDValue left = cellGrowth(pRtree, pLeftBox, &aCell[ii]);
      RtreeDValue right = cellGrowth(pRtree, pLeftBox, &aCell[ii]);
      RtreeDValue diff = FABS(right-left);
      if( iSelect<0 || diff>fDiff ){
        fDiff = diff;
        iSelect = ii;
      }
    }
  }
  aiUsed[iSelect] = 1;
  return &aCell[iSelect];
}

/*
** Implementation of the quadratic variant of the PickSeeds() function from
** Guttman[84].
** 实现Guttman[84]PickSeeds（）方法的变形方法-----平方算法  QuadraticPickSeeds（）
*/
static void QuadraticPickSeeds(
  Rtree *pRtree,
  RtreeCell *aCell, 
  int nCell, 
  int *piLeftSeed, 
  int *piRightSeed
){
  int ii;
  int jj;

  int iLeftSeed = 0;
  int iRightSeed = 1;
  RtreeDValue fWaste = 0.0;

  for(ii=0; ii<nCell; ii++){
    for(jj=ii+1; jj<nCell; jj++){
      RtreeDValue right = cellArea(pRtree, &aCell[jj]);
      RtreeDValue growth = cellGrowth(pRtree, &aCell[ii], &aCell[jj]);
      RtreeDValue waste = growth - right;

      if( waste>fWaste ){
        iLeftSeed = ii;
        iRightSeed = jj;
        fWaste = waste;
      }
    }
  }

  *piLeftSeed = iLeftSeed;
  *piRightSeed = iRightSeed;
}
#endif /* VARIANT_GUTTMAN_QUADRATIC_SPLIT 变形Guttman平方分裂算法*/

/*
** Arguments aIdx, aDistance and aSpare all point to arrays of size
** nIdx. The aIdx array contains the set of integers from 0 to 
** (nIdx-1) in no particular order. This function sorts the values
** in aIdx according to the indexed values in aDistance. For
** example, assuming the inputs:
** 参数 aIdx、aDistace 和aSpare 都指向长度为nIdx的数组
** 这个aIdx数组包含没有特定顺序的从0到nIdx-1整数的集合
** 该方法根据在aDistance中的索引值排序在aIdx中的值
** 例如：假设输入是这些:
**
**   aIdx      = { 0,   1,   2,   3 }
**   aDistance = { 5.0, 2.0, 7.0, 6.0 }
**
** this function sets the aIdx array to contain:
** 方法设置aIdx数组包括：
**
**   aIdx      = { 0,   1,   2,   3 }
**
** The aSpare array is used as temporary working space by the
** sorting algorithm.
** aSpare数组作为临时空间来实现排序算法。
*/
static void SortByDistance(
  int *aIdx, 
  int nIdx, 
  RtreeDValue *aDistance, 
  int *aSpare
){
  if( nIdx>1 ){
    int iLeft = 0;
    int iRight = 0;

    int nLeft = nIdx/2;
    int nRight = nIdx-nLeft;
    int *aLeft = aIdx;
    int *aRight = &aIdx[nLeft];

    SortByDistance(aLeft, nLeft, aDistance, aSpare);
    SortByDistance(aRight, nRight, aDistance, aSpare);

    memcpy(aSpare, aLeft, sizeof(int)*nLeft);
    aLeft = aSpare;

    while( iLeft<nLeft || iRight<nRight ){
      if( iLeft==nLeft ){
        aIdx[iLeft+iRight] = aRight[iRight];
        iRight++;
      }else if( iRight==nRight ){
        aIdx[iLeft+iRight] = aLeft[iLeft];
        iLeft++;
      }else{
        RtreeDValue fLeft = aDistance[aLeft[iLeft]];
        RtreeDValue fRight = aDistance[aRight[iRight]];
        if( fLeft<fRight ){
          aIdx[iLeft+iRight] = aLeft[iLeft];
          iLeft++;
        }else{
          aIdx[iLeft+iRight] = aRight[iRight];
          iRight++;
        }
      }
    }

#if 0
    /* Check that the sort worked 检查排序工作*/
    {
      int jj;
      for(jj=1; jj<nIdx; jj++){
        RtreeDValue left = aDistance[aIdx[jj-1]];
        RtreeDValue right = aDistance[aIdx[jj]];
        assert( left<=right );
      }
    }
#endif
  }
}

/*
** Arguments aIdx, aCell and aSpare all point to arrays of size
** nIdx. The aIdx array contains the set of integers from 0 to 
** (nIdx-1) in no particular order. This function sorts the values
** in aIdx according to dimension iDim of the cells in aCell. The
** minimum value of dimension iDim is considered first, the
** maximum used to break ties.
** 参数 aIdx、aDistace 和aSpare 都指向长度为nIdx的数组
** 这个aIdx数组包含没有特定顺序的从0到nIdx-1整数的集合
** 该方法根据在aCell中尺寸值IDim的值排序在aIdx中的值
** 这尺寸iDim中的最小值首先考虑，这个最大的被用来分裂节点
**
** The aSpare array is used as temporary working space by the
** sorting algorithm.
** aSpare数组作为临时空间来实现排序算法。
*/
static void SortByDimension(
  Rtree *pRtree,
  int *aIdx, 
  int nIdx, 
  int iDim, 
  RtreeCell *aCell, 
  int *aSpare
){
  if( nIdx>1 ){

    int iLeft = 0;
    int iRight = 0;

    int nLeft = nIdx/2;
    int nRight = nIdx-nLeft;
    int *aLeft = aIdx;
    int *aRight = &aIdx[nLeft];

    SortByDimension(pRtree, aLeft, nLeft, iDim, aCell, aSpare);
    SortByDimension(pRtree, aRight, nRight, iDim, aCell, aSpare);

    memcpy(aSpare, aLeft, sizeof(int)*nLeft);
    aLeft = aSpare;
    while( iLeft<nLeft || iRight<nRight ){
      RtreeDValue xleft1 = DCOORD(aCell[aLeft[iLeft]].aCoord[iDim*2]);
      RtreeDValue xleft2 = DCOORD(aCell[aLeft[iLeft]].aCoord[iDim*2+1]);
      RtreeDValue xright1 = DCOORD(aCell[aRight[iRight]].aCoord[iDim*2]);
      RtreeDValue xright2 = DCOORD(aCell[aRight[iRight]].aCoord[iDim*2+1]);
      if( (iLeft!=nLeft) && ((iRight==nRight)
       || (xleft1<xright1)
       || (xleft1==xright1 && xleft2<xright2)
      )){
        aIdx[iLeft+iRight] = aLeft[iLeft];
        iLeft++;
      }else{
        aIdx[iLeft+iRight] = aRight[iRight];
        iRight++;
      }
    }

#if 0
    /* Check that the sort worked 排序程序的检查*/
    {
      int jj;
      for(jj=1; jj<nIdx; jj++){
        RtreeDValue xleft1 = aCell[aIdx[jj-1]].aCoord[iDim*2];
        RtreeDValue xleft2 = aCell[aIdx[jj-1]].aCoord[iDim*2+1];
        RtreeDValue xright1 = aCell[aIdx[jj]].aCoord[iDim*2];
        RtreeDValue xright2 = aCell[aIdx[jj]].aCoord[iDim*2+1];
        assert( xleft1<=xright1 && (xleft1<xright1 || xleft2<=xright2) );
      }
    }
#endif
  }
}

#if VARIANT_RSTARTREE_SPLIT　//R*树结点分裂算法
/*
** Implementation of the R*-tree variant of SplitNode from Beckman[1990].
** 实现Beckman[1990]中的SplitNode方法的变形-------R*树变形方法splitNodeStartree（）
*/
static int splitNodeStartree(
  Rtree *pRtree,
  RtreeCell *aCell,
  int nCell,
  RtreeNode *pLeft,
  RtreeNode *pRight,
  RtreeCell *pBboxLeft,
  RtreeCell *pBboxRight
){
  int **aaSorted;
  int *aSpare;
  int ii;

  int iBestDim = 0;
  int iBestSplit = 0;
  RtreeDValue fBestMargin = 0.0;

  int nByte = (pRtree->nDim+1)*(sizeof(int*)+nCell*sizeof(int));

  aaSorted = (int **)sqlite3_malloc(nByte);
  if( !aaSorted ){
    return SQLITE_NOMEM;
  }

  aSpare = &((int *)&aaSorted[pRtree->nDim])[pRtree->nDim*nCell];
  memset(aaSorted, 0, nByte);
  for(ii=0; ii<pRtree->nDim; ii++){
    int jj;
    aaSorted[ii] = &((int *)&aaSorted[pRtree->nDim])[ii*nCell];
    for(jj=0; jj<nCell; jj++){
      aaSorted[ii][jj] = jj;
    }
    SortByDimension(pRtree, aaSorted[ii], nCell, ii, aCell, aSpare);
  }

  for(ii=0; ii<pRtree->nDim; ii++){
    RtreeDValue margin = 0.0;
    RtreeDValue fBestOverlap = 0.0;
    RtreeDValue fBestArea = 0.0;
    int iBestLeft = 0;
    int nLeft;

    for(
      nLeft=RTREE_MINCELLS(pRtree); 
      nLeft<=(nCell-RTREE_MINCELLS(pRtree)); 
      nLeft++
    ){
      RtreeCell left;
      RtreeCell right;
      int kk;
      RtreeDValue overlap;
      RtreeDValue area;

      memcpy(&left, &aCell[aaSorted[ii][0]], sizeof(RtreeCell));
      memcpy(&right, &aCell[aaSorted[ii][nCell-1]], sizeof(RtreeCell));
      for(kk=1; kk<(nCell-1); kk++){
        if( kk<nLeft ){
          cellUnion(pRtree, &left, &aCell[aaSorted[ii][kk]]);
        }else{
          cellUnion(pRtree, &right, &aCell[aaSorted[ii][kk]]);
        }
      }
      margin += cellMargin(pRtree, &left);
      margin += cellMargin(pRtree, &right);
      overlap = cellOverlap(pRtree, &left, &right, 1, -1);
      area = cellArea(pRtree, &left) + cellArea(pRtree, &right);
      if( (nLeft==RTREE_MINCELLS(pRtree))
       || (overlap<fBestOverlap)
       || (overlap==fBestOverlap && area<fBestArea)
      ){
        iBestLeft = nLeft;
        fBestOverlap = overlap;
        fBestArea = area;
      }
    }

    if( ii==0 || margin<fBestMargin ){
      iBestDim = ii;
      fBestMargin = margin;
      iBestSplit = iBestLeft;
    }
  }

  memcpy(pBboxLeft, &aCell[aaSorted[iBestDim][0]], sizeof(RtreeCell));
  memcpy(pBboxRight, &aCell[aaSorted[iBestDim][iBestSplit]], sizeof(RtreeCell));
  for(ii=0; ii<nCell; ii++){
    RtreeNode *pTarget = (ii<iBestSplit)?pLeft:pRight;
    RtreeCell *pBbox = (ii<iBestSplit)?pBboxLeft:pBboxRight;
    RtreeCell *pCell = &aCell[aaSorted[iBestDim][ii]];
    nodeInsertCell(pRtree, pTarget, pCell);
    cellUnion(pRtree, pBbox, pCell);
  }

  sqlite3_free(aaSorted);
  return SQLITE_OK;
}
#endif

#if VARIANT_GUTTMAN_SPLIT //R树结点分裂算法
/*
** Implementation of the regular R-tree SplitNode from Guttman[1984].
** 实现Guttman[1984]中的常规R树SplitNode方法splitNodeGuttman （）
*/
static int splitNodeGuttman(
  Rtree *pRtree,
  RtreeCell *aCell,
  int nCell,
  RtreeNode *pLeft,
  RtreeNode *pRight,
  RtreeCell *pBboxLeft,
  RtreeCell *pBboxRight
){
  int iLeftSeed = 0;
  int iRightSeed = 1;
  int *aiUsed;
  int i;

  aiUsed = sqlite3_malloc(sizeof(int)*nCell);
  if( !aiUsed ){
    return SQLITE_NOMEM;
  }
  memset(aiUsed, 0, sizeof(int)*nCell);

  PickSeeds(pRtree, aCell, nCell, &iLeftSeed, &iRightSeed);

  memcpy(pBboxLeft, &aCell[iLeftSeed], sizeof(RtreeCell));
  memcpy(pBboxRight, &aCell[iRightSeed], sizeof(RtreeCell));
  nodeInsertCell(pRtree, pLeft, &aCell[iLeftSeed]);
  nodeInsertCell(pRtree, pRight, &aCell[iRightSeed]);
  aiUsed[iLeftSeed] = 1;
  aiUsed[iRightSeed] = 1;

  for(i=nCell-2; i>0; i--){
    RtreeCell *pNext;
    pNext = PickNext(pRtree, aCell, nCell, pBboxLeft, pBboxRight, aiUsed);
    RtreeDValue diff =  
      cellGrowth(pRtree, pBboxLeft, pNext) - 
      cellGrowth(pRtree, pBboxRight, pNext)
    ;
    if( (RTREE_MINCELLS(pRtree)-NCELL(pRight)==i)
     || (diff>0.0 && (RTREE_MINCELLS(pRtree)-NCELL(pLeft)!=i))
    ){
      nodeInsertCell(pRtree, pRight, pNext);
      cellUnion(pRtree, pBboxRight, pNext);
    }else{
      nodeInsertCell(pRtree, pLeft, pNext);
      cellUnion(pRtree, pBboxLeft, pNext);
    }
  }

  sqlite3_free(aiUsed);
  return SQLITE_OK;
}
#endif

static int updateMapping(  //更新pNode结点与iRowid映射关系
  Rtree *pRtree, 
  i64 iRowid, 
  RtreeNode *pNode, 
  int iHeight
){
  int (*xSetMapping)(Rtree *, sqlite3_int64, sqlite3_int64);
  xSetMapping = ((iHeight==0)?rowidWrite:parentWrite);
  if( iHeight>0 ){
    RtreeNode *pChild = nodeHashLookup(pRtree, iRowid);
    if( pChild ){
      nodeRelease(pRtree, pChild->pParent);
      nodeReference(pNode);
      pChild->pParent = pNode;
    }
  }
  return xSetMapping(pRtree, iRowid, pNode->iNode);
}

static int SplitNode(
  Rtree *pRtree,
  RtreeNode *pNode,
  RtreeCell *pCell,
  int iHeight
){
  int i;
  int newCellIsRight = 0;

  int rc = SQLITE_OK;
  int nCell = NCELL(pNode);
  RtreeCell *aCell;
  int *aiUsed;

  RtreeNode *pLeft = 0;
  RtreeNode *pRight = 0;

  RtreeCell leftbbox;
  RtreeCell rightbbox;

  /* Allocate an array and populate it with a copy of pCell and 
  ** all cells from node pLeft. Then zero the original node.
  ** 分配一个数组，并将结点pLeft所有单元和pCell放到数组中，然后将原结点置0,即删除原结点
  */
  aCell = sqlite3_malloc((sizeof(RtreeCell)+sizeof(int))*(nCell+1));
  if( !aCell ){
    rc = SQLITE_NOMEM;
    goto splitnode_out;
  }
  aiUsed = (int *)&aCell[nCell+1];
  memset(aiUsed, 0, sizeof(int)*(nCell+1));
  for(i=0; i<nCell; i++){
    nodeGetCell(pRtree, pNode, i, &aCell[i]);
  }
  nodeZero(pRtree, pNode);
  memcpy(&aCell[nCell], pCell, sizeof(RtreeCell));
  nCell++;

  if( pNode->iNode==1 ){
    pRight = nodeNew(pRtree, pNode);
    pLeft = nodeNew(pRtree, pNode);
    pRtree->iDepth++;
    pNode->isDirty = 1;
    writeInt16(pNode->zData, pRtree->iDepth);
  }else{
    pLeft = pNode;
    pRight = nodeNew(pRtree, pLeft->pParent);
    nodeReference(pLeft);
  }

  if( !pLeft || !pRight ){
    rc = SQLITE_NOMEM;
    goto splitnode_out;
  }

  memset(pLeft->zData, 0, pRtree->iNodeSize);
  memset(pRight->zData, 0, pRtree->iNodeSize);

  rc = AssignCells(pRtree, aCell, nCell, pLeft, pRight, &leftbbox, &rightbbox);
  if( rc!=SQLITE_OK ){
    goto splitnode_out;
  }

  /* Ensure both child nodes have node numbers assigned to them by calling
  ** nodeWrite(). Node pRight always needs a node number, as it was created
  ** by nodeNew() above. But node pLeft sometimes already has a node number.
  ** In this case avoid the all to nodeWrite().
  **
  ** 通过调用nodeWrite()方法确保左右孩子结点有结点号分配给它们，
  ** 由于结点pRight一般通过 nodeNew()被创建，所以它总是需要一个结点号，但是结点pLeft有时候已有结点号。
  ** 这种情况就避免了所有的结点都需要调用nodeWrite().
  */
  if( SQLITE_OK!=(rc = nodeWrite(pRtree, pRight))
   || (0==pLeft->iNode && SQLITE_OK!=(rc = nodeWrite(pRtree, pLeft)))
  ){
    goto splitnode_out;
  }

  rightbbox.iRowid = pRight->iNode;
  leftbbox.iRowid = pLeft->iNode;

  if( pNode->iNode==1 ){
    rc = rtreeInsertCell(pRtree, pLeft->pParent, &leftbbox, iHeight+1);
    if( rc!=SQLITE_OK ){
      goto splitnode_out;
    }
  }else{
    RtreeNode *pParent = pLeft->pParent;
    int iCell;
    rc = nodeParentIndex(pRtree, pLeft, &iCell);
    if( rc==SQLITE_OK ){
      nodeOverwriteCell(pRtree, pParent, &leftbbox, iCell);
      rc = AdjustTree(pRtree, pParent, &leftbbox);
    }
    if( rc!=SQLITE_OK ){
      goto splitnode_out;
    }
  }
  if( (rc = rtreeInsertCell(pRtree, pRight->pParent, &rightbbox, iHeight+1)) ){
    goto splitnode_out;
  }

  for(i=0; i<NCELL(pRight); i++){
    i64 iRowid = nodeGetRowid(pRtree, pRight, i);
    rc = updateMapping(pRtree, iRowid, pRight, iHeight);
    if( iRowid==pCell->iRowid ){
      newCellIsRight = 1;
    }
    if( rc!=SQLITE_OK ){
      goto splitnode_out;
    }
  }
  if( pNode->iNode==1 ){
    for(i=0; i<NCELL(pLeft); i++){
      i64 iRowid = nodeGetRowid(pRtree, pLeft, i);
      rc = updateMapping(pRtree, iRowid, pLeft, iHeight);
      if( rc!=SQLITE_OK ){
        goto splitnode_out;
      }
    }
  }else if( newCellIsRight==0 ){
    rc = updateMapping(pRtree, pCell->iRowid, pLeft, iHeight);
  }

  if( rc==SQLITE_OK ){
    rc = nodeRelease(pRtree, pRight);
    pRight = 0;
  }
  if( rc==SQLITE_OK ){
    rc = nodeRelease(pRtree, pLeft);
    pLeft = 0;
  }

splitnode_out:
  nodeRelease(pRtree, pRight);
  nodeRelease(pRtree, pLeft);
  sqlite3_free(aCell);
  return rc;
}

/*
** If node pLeaf is not the root of the r-tree and its pParent pointer is 
** still NULL, load all ancestor nodes of pLeaf into memory and populate
** the pLeaf->pParent chain all the way up to the root node.
** 如果pLeaf 节点不是R树的根节点及它的pParent指针为空，加载所有pLeaf的原来节点到内存
** 并用这些节点沿着pLeaf->pParent链上升根节点逐渐加到进去
**
** This operation is required when a row is deleted (or updated - an update
** is implemented as a delete followed by an insert). SQLite provides the
** rowid of the row to delete, which can be used to find the leaf on which
** the entry resides (argument pLeaf). Once the leaf is located, this 
** function is called to determine its ancestry.
** 当一行记录被删除的时候该方法被调用（或是更新：更新就是在插入后进行删除来实现的）。
** SQLite提供行的行id来删除，通过行id能找到叶子节点在哪个存储的记录中（参数为pLeaf）。
** 当一个叶结点被找到时，这个函数将被调用来确定其父结点。
*/
static int fixLeafParent(Rtree *pRtree, RtreeNode *pLeaf){
  int rc = SQLITE_OK;
  RtreeNode *pChild = pLeaf;
  while( rc==SQLITE_OK && pChild->iNode!=1 && pChild->pParent==0 ){
    int rc2 = SQLITE_OK;          /* sqlite3_reset() return code */
    sqlite3_bind_int64(pRtree->pReadParent, 1, pChild->iNode);
    rc = sqlite3_step(pRtree->pReadParent);
    if( rc==SQLITE_ROW ){
      RtreeNode *pTest;           /* Used to test for reference loops 测试引用循环*/
      i64 iNode;                  /* Node number of parent node 父节点的节点号*/

      /* Before setting pChild->pParent, test that we are not creating a
      ** loop of references (as we would if, say, pChild==pParent). We don't
      ** want to do this as it leads to a memory leak when trying to delete
      ** the referenced counted node structures.
      ** 设置pChild的pParent之前，测试我们是否创建成了一个引用环（如果是，则设置pChild==pParent）
      ** 我们不想这样做，因为当试着去删除统计节点结构的引用时会发生内存泄露
      */
      iNode = sqlite3_column_int64(pRtree->pReadParent, 0);
      for(pTest=pLeaf; pTest && pTest->iNode!=iNode; pTest=pTest->pParent);
      if( !pTest ){
        rc2 = nodeAcquire(pRtree, iNode, 0, &pChild->pParent);
      }
    }
    rc = sqlite3_reset(pRtree->pReadParent);
    if( rc==SQLITE_OK ) rc = rc2;
    if( rc==SQLITE_OK && !pChild->pParent ) rc = SQLITE_CORRUPT_VTAB;
    pChild = pChild->pParent;
  }
  return rc;
}

static int deleteCell(Rtree *, RtreeNode *, int, int);

static int removeNode(Rtree *pRtree, RtreeNode *pNode, int iHeight){
  int rc;
  int rc2;
  RtreeNode *pParent = 0;
  int iCell;

  assert( pNode->nRef==1 );

  /* Remove the entry in the parent cell. 从父单元中删除一个记录*/
  rc = nodeParentIndex(pRtree, pNode, &iCell);
  if( rc==SQLITE_OK ){
    pParent = pNode->pParent;
    pNode->pParent = 0;
    rc = deleteCell(pRtree, pParent, iCell, iHeight+1);
  }
  rc2 = nodeRelease(pRtree, pParent);
  if( rc==SQLITE_OK ){
    rc = rc2;
  }
  if( rc!=SQLITE_OK ){
    return rc;
  }

  /* Remove the xxx_node entry. 删除xxx_node 记录*/
  sqlite3_bind_int64(pRtree->pDeleteNode, 1, pNode->iNode);
  sqlite3_step(pRtree->pDeleteNode);
  if( SQLITE_OK!=(rc = sqlite3_reset(pRtree->pDeleteNode)) ){
    return rc;
  }

  /* Remove the xxx_parent entry. 删除xxx_parent记录*/
  sqlite3_bind_int64(pRtree->pDeleteParent, 1, pNode->iNode);
  sqlite3_step(pRtree->pDeleteParent);
  if( SQLITE_OK!=(rc = sqlite3_reset(pRtree->pDeleteParent)) ){
    return rc;
  }
  
  /* Remove the node from the in-memory hash table and link it into
  ** the Rtree.pDeleted list. Its contents will be re-inserted later on.
  ** 从内存中的hash表中移除结点并将其链接到R树的pDeleted列表中,它的内容之后会在重插入进去
  */
  nodeHashDelete(pRtree, pNode);
  pNode->iNode = iHeight;
  pNode->pNext = pRtree->pDeleted;
  pNode->nRef++;
  pRtree->pDeleted = pNode;

  return SQLITE_OK;
}

static int fixBoundingBox(Rtree *pRtree, RtreeNode *pNode){
  RtreeNode *pParent = pNode->pParent;
  int rc = SQLITE_OK; 
  if( pParent ){
    int ii; 
    int nCell = NCELL(pNode);
    RtreeCell box;                            /* Bounding box for pNode pNode的边界框*/
    nodeGetCell(pRtree, pNode, 0, &box);
    for(ii=1; ii<nCell; ii++){
      RtreeCell cell;
      nodeGetCell(pRtree, pNode, ii, &cell);
      cellUnion(pRtree, &box, &cell);
    }
    box.iRowid = pNode->iNode;
    rc = nodeParentIndex(pRtree, pNode, &ii);
    if( rc==SQLITE_OK ){
      nodeOverwriteCell(pRtree, pParent, &box, ii);
      rc = fixBoundingBox(pRtree, pParent);
    }
  }
  return rc;
}

/*
** Delete the cell at index iCell of node pNode. After removing the
** cell, adjust the r-tree data structure if required.
** 删除结点pNode的索引为iCell的单元，删除后如果有需要重新调整R树结构
*/
static int deleteCell(Rtree *pRtree, RtreeNode *pNode, int iCell, int iHeight){
  RtreeNode *pParent;
  int rc;

  if( SQLITE_OK!=(rc = fixLeafParent(pRtree, pNode)) ){
    return rc;
  }

  /* Remove the cell from the node. This call just moves bytes around
  ** the in-memory node image, so it cannot fail.
  ** 移除节点中的单元，这个操作只是移动内存中节点镜像，所以不会失败
  */
  nodeDeleteCell(pRtree, pNode, iCell);

  /* If the node is not the tree root and now has less than the minimum
  ** number of cells, remove it from the tree. Otherwise, update the
  ** cell in the parent node so that it tightly contains the updated
  ** node.
  ** 如果结点不是根结点并且现在它单元数小于最小单元数，将它从Rtree中移出。
  ** 因此将节点更新到到父结点中以便结点更加紧凑。
  */
  pParent = pNode->pParent;
  assert( pParent || pNode->iNode==1 );
  if( pParent ){
    if( NCELL(pNode)<RTREE_MINCELLS(pRtree) ){
      rc = removeNode(pRtree, pNode, iHeight);
    }else{
      rc = fixBoundingBox(pRtree, pNode);
    }
  }

  return rc;
}

static int Reinsert(  // 进行重插入操作
  Rtree *pRtree,
  RtreeNode *pNode, 
  RtreeCell *pCell, 
  int iHeight
){
  int *aOrder;
  int *aSpare;
  RtreeCell *aCell;
  RtreeDValue *aDistance;
  int nCell;
  RtreeDValue aCenterCoord[RTREE_MAX_DIMENSIONS];
  int iDim;
  int ii;
  int rc = SQLITE_OK;
  int n;

  memset(aCenterCoord, 0, sizeof(RtreeDValue)*RTREE_MAX_DIMENSIONS);

  nCell = NCELL(pNode)+1;
  n = (nCell+1)&(~1);

  /* Allocate the buffers used by this operation. The allocation is
  ** relinquished before this function returns.
  ** 通过这个操作分配缓冲区，在函数返回前分配的缓冲区将会被废弃。
  */
  aCell = (RtreeCell *)sqlite3_malloc(n * (
    sizeof(RtreeCell)     +         /* aCell array aCell数组*/
    sizeof(int)           +         /* aOrder array aOrder数组*/
    sizeof(int)           +         /* aSpare array aSpare数组*/
    sizeof(RtreeDValue)             /* aDistance array aDistance数组*/
  ));
  if( !aCell ){
    return SQLITE_NOMEM;
  }
  aOrder    = (int *)&aCell[n];
  aSpare    = (int *)&aOrder[n];
  aDistance = (RtreeDValue *)&aSpare[n];

  for(ii=0; ii<nCell; ii++){
    if( ii==(nCell-1) ){
      memcpy(&aCell[ii], pCell, sizeof(RtreeCell));
    }else{
      nodeGetCell(pRtree, pNode, ii, &aCell[ii]);
    }
    aOrder[ii] = ii;
    for(iDim=0; iDim<pRtree->nDim; iDim++){
      aCenterCoord[iDim] += DCOORD(aCell[ii].aCoord[iDim*2]);
      aCenterCoord[iDim] += DCOORD(aCell[ii].aCoord[iDim*2+1]);
    }
  }
  for(iDim=0; iDim<pRtree->nDim; iDim++){
    aCenterCoord[iDim] = (aCenterCoord[iDim]/(nCell*(RtreeDValue)2));
  }

  for(ii=0; ii<nCell; ii++){
    aDistance[ii] = 0.0;
    for(iDim=0; iDim<pRtree->nDim; iDim++){
      RtreeDValue coord = (DCOORD(aCell[ii].aCoord[iDim*2+1]) - 
                               DCOORD(aCell[ii].aCoord[iDim*2]));
      aDistance[ii] += (coord-aCenterCoord[iDim])*(coord-aCenterCoord[iDim]);
    }
  }

  SortByDistance(aOrder, nCell, aDistance, aSpare);
  nodeZero(pRtree, pNode);

  for(ii=0; rc==SQLITE_OK && ii<(nCell-(RTREE_MINCELLS(pRtree)+1)); ii++){
    RtreeCell *p = &aCell[aOrder[ii]];
    nodeInsertCell(pRtree, pNode, p);
    if( p->iRowid==pCell->iRowid ){
      if( iHeight==0 ){
        rc = rowidWrite(pRtree, p->iRowid, pNode->iNode);
      }else{
        rc = parentWrite(pRtree, p->iRowid, pNode->iNode);
      }
    }
  }
  if( rc==SQLITE_OK ){
    rc = fixBoundingBox(pRtree, pNode);
  }
  for(; rc==SQLITE_OK && ii<nCell; ii++){
    /* Find a node to store this cell in. pNode->iNode currently contains
    ** the height of the sub-tree headed by the cell.
    ** 找到一个结点来存储这个单元。pNode->iNode包含了以此单元为首的子树的高度
    */
    RtreeNode *pInsert;
    RtreeCell *p = &aCell[aOrder[ii]];
    rc = ChooseLeaf(pRtree, p, iHeight, &pInsert);
    if( rc==SQLITE_OK ){
      int rc2;
      rc = rtreeInsertCell(pRtree, pInsert, p, iHeight);
      rc2 = nodeRelease(pRtree, pInsert);
      if( rc==SQLITE_OK ){
        rc = rc2;
      }
    }
  }

  sqlite3_free(aCell);
  return rc;
}

/*
** Insert cell pCell into node pNode. Node pNode is the head of a 
** subtree iHeight high (leaf nodes have iHeight==0).
** 把单元pCell插入到结点pNode中.结点pNode是高度为iHeight节点的头（叶子节点的IHeight为0）
*/
static int rtreeInsertCell(
  Rtree *pRtree,
  RtreeNode *pNode,
  RtreeCell *pCell,
  int iHeight
){
  int rc = SQLITE_OK;
  if( iHeight>0 ){
    RtreeNode *pChild = nodeHashLookup(pRtree, pCell->iRowid);
    if( pChild ){
      nodeRelease(pRtree, pChild->pParent);
      nodeReference(pNode);
      pChild->pParent = pNode;
    }
  }
  if( nodeInsertCell(pRtree, pNode, pCell) ){
#if VARIANT_RSTARTREE_REINSERT
    if( iHeight<=pRtree->iReinsertHeight || pNode->iNode==1){
      rc = SplitNode(pRtree, pNode, pCell, iHeight);
    }else{
      pRtree->iReinsertHeight = iHeight;
      rc = Reinsert(pRtree, pNode, pCell, iHeight);
    }
#else
    rc = SplitNode(pRtree, pNode, pCell, iHeight);
#endif
  }else{
    rc = AdjustTree(pRtree, pNode, pCell);
    if( rc==SQLITE_OK ){
      if( iHeight==0 ){
        rc = rowidWrite(pRtree, pCell->iRowid, pNode->iNode);
      }else{
        rc = parentWrite(pRtree, pCell->iRowid, pNode->iNode);
      }
    }
  }
  return rc;
}

static int reinsertNodeContent(Rtree *pRtree, RtreeNode *pNode){
  int ii;
  int rc = SQLITE_OK;
  int nCell = NCELL(pNode);

  for(ii=0; rc==SQLITE_OK && ii<nCell; ii++){
    RtreeNode *pInsert;
    RtreeCell cell;
    nodeGetCell(pRtree, pNode, ii, &cell);

    /* Find a node to store this cell in. pNode->iNode currently contains
    ** the height of the sub-tree headed by the cell.
    ** 寻找一个节点去存储这个单元。pNode->iNode包含了以此单元为首的子树的高度
    */
    rc = ChooseLeaf(pRtree, &cell, (int)pNode->iNode, &pInsert);
    if( rc==SQLITE_OK ){
      int rc2;
      rc = rtreeInsertCell(pRtree, pInsert, &cell, (int)pNode->iNode);
      rc2 = nodeRelease(pRtree, pInsert);
      if( rc==SQLITE_OK ){
        rc = rc2;
      }
    }
  }
  return rc;
}

/*
** Select a currently unused rowid for a new r-tree record.
** 为新的R树记录选择一个当前未使用的rowid
*/
static int newRowid(Rtree *pRtree, i64 *piRowid){
  int rc;
  sqlite3_bind_null(pRtree->pWriteRowid, 1);
  sqlite3_bind_null(pRtree->pWriteRowid, 2);
  sqlite3_step(pRtree->pWriteRowid);
  rc = sqlite3_reset(pRtree->pWriteRowid);
  *piRowid = sqlite3_last_insert_rowid(pRtree->db);
  return rc;
}

/*
** Remove the entry with rowid=iDelete from the r-tree structure.
** 从Rtree中删除rowid=iDelete的记录
*/
static int rtreeDeleteRowid(Rtree *pRtree, sqlite3_int64 iDelete){
  int rc;                         /* Return code 返回的字符串*/
  RtreeNode *pLeaf;               /* Leaf node containing record iDelete 叶子节点包含iDelete记录*/
  int iCell;                      /* Index of iDelete cell in pLeaf 在pLeaf中IDelete单元的索引*/
  RtreeNode *pRoot;               /* Root node of rtree structure R树结构的根节点*/


  /* Obtain a reference to the root node to initialise Rtree.iDepth 
  ** 得到指向根结点的索引去初始化R树的高度
  */
    rc = nodeAcquire(pRtree, 1, 0, &pRoot);

  /* Obtain a reference to the leaf node that contains the entry 
  ** about to be deleted. 
  ** 得到指向包含要被删除的叶结点的索引
  */
  if( rc==SQLITE_OK ){
    rc = findLeafNode(pRtree, iDelete, &pLeaf);
  }

  /* Delete the cell in question from the leaf node. 
  ** 从叶结点中删除被请求的单元
  */
  if( rc==SQLITE_OK ){
    int rc2;
    rc = nodeRowidIndex(pRtree, pLeaf, iDelete, &iCell);
    if( rc==SQLITE_OK ){
      rc = deleteCell(pRtree, pLeaf, iCell, 0);
    }
    rc2 = nodeRelease(pRtree, pLeaf);
    if( rc==SQLITE_OK ){
      rc = rc2;
    }
  }

  /* Delete the corresponding entry in the <rtree>_rowid table. 
  ** 从Rtree_rowid表中删除对应的记录
  */
  if( rc==SQLITE_OK ){
    sqlite3_bind_int64(pRtree->pDeleteRowid, 1, iDelete);
    sqlite3_step(pRtree->pDeleteRowid);
    rc = sqlite3_reset(pRtree->pDeleteRowid);
  }

  /* Check if the root node now has exactly one child. If so, remove
  ** it, schedule the contents of the child for reinsertion and 
  ** reduce the tree height by one.
  ** 检查这个根节点是否正好有一个子节点。如果是，删除它，计划将子节点内容重新插入来减少树的高度
  **
  ** This is equivalent to copying the contents of the child into
  ** the root node (the operation that Gutman's paper says to perform 
  ** in this scenario).
  ** 这相当于复制子结点的内容到根结点,即用子结点代替根结点（这古特曼的论文说，操作在这种情况下进行）
  */
  if( rc==SQLITE_OK && pRtree->iDepth>0 && NCELL(pRoot)==1 ){
    int rc2;
    RtreeNode *pChild;
    i64 iChild = nodeGetRowid(pRtree, pRoot, 0);
    rc = nodeAcquire(pRtree, iChild, pRoot, &pChild);
    if( rc==SQLITE_OK ){
      rc = removeNode(pRtree, pChild, pRtree->iDepth-1);
    }
    rc2 = nodeRelease(pRtree, pChild);
    if( rc==SQLITE_OK ) rc = rc2;
    if( rc==SQLITE_OK ){
      pRtree->iDepth--;
      writeInt16(pRoot->zData, pRtree->iDepth);
      pRoot->isDirty = 1;
    }
  }

  /* Re-insert the contents of any underfull nodes removed from the tree. */
  /* 重插入从R树中移除的任何不满的结点的内容 */
  for(pLeaf=pRtree->pDeleted; pLeaf; pLeaf=pRtree->pDeleted){
    if( rc==SQLITE_OK ){
      rc = reinsertNodeContent(pRtree, pLeaf);
    }
    pRtree->pDeleted = pLeaf->pNext;
    sqlite3_free(pLeaf);
  }

  /* Release the reference to the root node. */
  /* 释放指向根结点的指针 */
  if( rc==SQLITE_OK ){
    rc = nodeRelease(pRtree, pRoot);
  }else{
    nodeRelease(pRtree, pRoot);
  }

  return rc;
}

/*
** Rounding constants for float->double conversion.
** 对float类型转换double类型的常数进行四舍五入
*/
#define RNDTOWARDS  (1.0 - 1.0/8388608.0)  /* Round towards zero 四舍五入趋近与0*/
#define RNDAWAY     (1.0 + 1.0/8388608.0)  /* Round away from zero 四舍五入不趋近与0*/

#if !defined(SQLITE_RTREE_INT_ONLY)
/*
** Convert an sqlite3_value into an RtreeValue (presumably a float)
** while taking care to round toward negative or positive, respectively.
** 将sqlite3_value转换为RtreeValue（大概是一个float类型）分别四舍五入为正数、负数
**
*/
static RtreeValue rtreeValueDown(sqlite3_value *v){
  double d = sqlite3_value_double(v);
  float f = (float)d;
  if( f>d ){
    f = (float)(d*(d<0 ? RNDAWAY : RNDTOWARDS));
  }
  return f;
}
static RtreeValue rtreeValueUp(sqlite3_value *v){
  double d = sqlite3_value_double(v);
  float f = (float)d;
  if( f<d ){
    f = (float)(d*(d<0 ? RNDTOWARDS : RNDAWAY));
  }
  return f;
}
#endif /* !defined(SQLITE_RTREE_INT_ONLY) 定义sqlite_Rtree_int_only 定义sqliteR树仅整数*/


/*
** The xUpdate method for rtree module virtual tables.
** R树模块虚拟表的方法xUpdate
*/
static int rtreeUpdate(
  sqlite3_vtab *pVtab, 
  int nData, 
  sqlite3_value **azData, 
  sqlite_int64 *pRowid
){
  Rtree *pRtree = (Rtree *)pVtab;
  int rc = SQLITE_OK;
  RtreeCell cell;                 /* New cell to insert if nData>1 如果nDate>1 则插入新的单元*/
  int bHaveRowid = 0;             /* Set to 1 after new rowid is determined 当新的rowid被确定后设置为1*/

  rtreeReference(pRtree);
  assert(nData>=1);

  /* Constraint handling. A write operation on an r-tree table may return
  ** SQLITE_CONSTRAINT for two reasons:
  ** 约束处理，R树上的一个写入操作可能返回SQLITE_CONSTRAINT 两种原因：
  **   1. A duplicate rowid value, or
  **   1. 一个重复的rowid值，或
  **   2. The supplied data violates the "x2>=x1" constraint.
  **   2. 提供的数据违反了x2>=x1的约束
  ** In the first case, if the conflict-handling mode is REPLACE, then
  ** the conflicting row can be removed before proceeding. In the second
  ** case, SQLITE_CONSTRAINT must be returned regardless of the
  ** conflict-handling mode specified by the user.
  ** 在第一个情况中，如果冲突处理模式设置为REPLACE，在程序运行前冲突的行将被移除。
  ** 在第二种情况中，不管用户指定的冲突模式，SQLITE_CONSTRAINT都必须返回
  */
  if( nData>1 ){
    int ii;

    /* Populate the cell.aCoord[] array. The first coordinate is azData[3]. 
    ** 填充这个单元.aCoord数组。这个第一个坐标欧式azData[3]  azData[0]用来作为删除标记
    */
    assert( nData==(pRtree->nDim*2 + 3) );
#ifndef SQLITE_RTREE_INT_ONLY
    if( pRtree->eCoordType==RTREE_COORD_REAL32 ){
      for(ii=0; ii<(pRtree->nDim*2); ii+=2){ //一个维度有两个值
        cell.aCoord[ii].f = rtreeValueDown(azData[ii+3]);
        cell.aCoord[ii+1].f = rtreeValueUp(azData[ii+4]);
        if( cell.aCoord[ii].f>cell.aCoord[ii+1].f ){
          rc = SQLITE_CONSTRAINT;
          goto constraint;
        }
      }
    }else
#endif
    {
      for(ii=0; ii<(pRtree->nDim*2); ii+=2){
        cell.aCoord[ii].i = sqlite3_value_int(azData[ii+3]);
        cell.aCoord[ii+1].i = sqlite3_value_int(azData[ii+4]);
        if( cell.aCoord[ii].i>cell.aCoord[ii+1].i ){
          rc = SQLITE_CONSTRAINT;
          goto constraint;
        }
      }
    }

    /* If a rowid value was supplied, check if it is already present in 
    ** the table. If so, the constraint has failed. 
    ** 如果提供了rowid值，验证它在当前表中是否存在，如果存在，约束将失效
    */
    if( sqlite3_value_type(azData[2])!=SQLITE_NULL ){
      cell.iRowid = sqlite3_value_int64(azData[2]);
      if( sqlite3_value_type(azData[0])==SQLITE_NULL
       || sqlite3_value_int64(azData[0])!=cell.iRowid
      ){
        int steprc;
        sqlite3_bind_int64(pRtree->pReadRowid, 1, cell.iRowid);
        steprc = sqlite3_step(pRtree->pReadRowid);
        rc = sqlite3_reset(pRtree->pReadRowid);
        if( SQLITE_ROW==steprc ){
          if( sqlite3_vtab_on_conflict(pRtree->db)==SQLITE_REPLACE ){
            rc = rtreeDeleteRowid(pRtree, cell.iRowid);
          }else{
            rc = SQLITE_CONSTRAINT;
            goto constraint;
          }
        }
      }
      bHaveRowid = 1;
    }
  }

  /* If azData[0] is not an SQL NULL value, it is the rowid of a
  ** record to delete from the r-tree table. The following block does
  ** just that.
  ** 如果azData[0]不是SQL空值，它是从R树表中删除的记录的rowid
  ** 下面的数据块也是同样操作
  */
  if( sqlite3_value_type(azData[0])!=SQLITE_NULL ){
    rc = rtreeDeleteRowid(pRtree, sqlite3_value_int64(azData[0]));
  }

  /* If the azData[] array contains more than one element, elements
  ** (azData[2]..azData[argc-1]) contain a new record to insert into
  ** the r-tree structure.
  ** 如果azData[]数组包含了多于一个元素，（azData[2]..azData[argc-1]）
  ** 元素包含了需要插入到R树结构的一个新记录
  */
  if( rc==SQLITE_OK && nData>1 ){
    /* Insert the new record into the r-tree 向R树中插入想新的记录*/
    RtreeNode *pLeaf;

    /* Figure out the rowid of the new row. 算出新的行的rowid*/
    if( bHaveRowid==0 ){
      rc = newRowid(pRtree, &cell.iRowid);
    }
    *pRowid = cell.iRowid;

    if( rc==SQLITE_OK ){
      rc = ChooseLeaf(pRtree, &cell, 0, &pLeaf);
    }
    if( rc==SQLITE_OK ){
      int rc2;
      pRtree->iReinsertHeight = -1;
      rc = rtreeInsertCell(pRtree, pLeaf, &cell, 0);
      rc2 = nodeRelease(pRtree, pLeaf);
      if( rc==SQLITE_OK ){
        rc = rc2;
      }
    }
  }

constraint:
  rtreeRelease(pRtree);
  return rc;
}

/*
** The xRename method for rtree module virtual tables.
** R树模式虚拟表的重命名方法xRename
*/
static int rtreeRename(sqlite3_vtab *pVtab, const char *zNewName){
  Rtree *pRtree = (Rtree *)pVtab;
  int rc = SQLITE_NOMEM;
  char *zSql = sqlite3_mprintf(
    "ALTER TABLE %Q.'%q_node'   RENAME TO \"%w_node\";"
    "ALTER TABLE %Q.'%q_parent' RENAME TO \"%w_parent\";"
    "ALTER TABLE %Q.'%q_rowid'  RENAME TO \"%w_rowid\";"
    , pRtree->zDb, pRtree->zName, zNewName 
    , pRtree->zDb, pRtree->zName, zNewName 
    , pRtree->zDb, pRtree->zName, zNewName
  );
  if( zSql ){
    rc = sqlite3_exec(pRtree->db, zSql, 0, 0, 0);
    sqlite3_free(zSql);
  }
  return rc;
}

static sqlite3_module rtreeModule = {
  0,                          /* iVersion 版本号 */
  rtreeCreate,                /* xCreate - create a table 创建一个表 */
  rtreeConnect,               /* xConnect - connect to an existing table 连接一个已存在的表 */
  rtreeBestIndex,             /* xBestIndex - Determine search strategy 确定搜索策略*/
  rtreeDisconnect,            /* xDisconnect - Disconnect from a table 断开表的连接*/
  rtreeDestroy,               /* xDestroy - Drop a table 删除一个表*/
  rtreeOpen,                  /* xOpen - open a cursor 打开一个游标*/
  rtreeClose,                 /* xClose - close a cursor 关闭一个游标*/
  rtreeFilter,                /* xFilter - configure scan constraints 配置扫描约束条件*/
  rtreeNext,                  /* xNext - advance a cursor 指针前进一格*/
  rtreeEof,                   /* xEof 结束标志*/
  rtreeColumn,                /* xColumn - read data 读取列数据*/
  rtreeRowid,                 /* xRowid - read data 读取行数据*/
  rtreeUpdate,                /* xUpdate - write data 写入数据*/
  0,                          /* xBegin - begin transaction 事务开始*/
  0,                          /* xSync - sync transaction 事务同步*/
  0,                          /* xCommit - commit transaction 提交事务*/
  0,                          /* xRollback - rollback transaction 回滚事务*/
  0,                          /* xFindFunction - function overloading 函数重载*/
  rtreeRename,                /* xRename - rename the table 重命名表名称*/
  0,                          /* xSavepoint 设置保存点*/
  0,                          /* xRelease 释放*/
  0                           /* xRollbackTo 回滚到*/
};

static int rtreeSqlInit(   //RtreeSQL语句初始化
  Rtree *pRtree,
  sqlite3 *db, 
  const char *zDb, 
  const char *zPrefix, 
  int isCreate
){
  int rc = SQLITE_OK;

  #define N_STATEMENT 9
  static const char *azSql[N_STATEMENT] = {
    /* Read and write the xxx_node table 读和写xxx_node 表*/
    "SELECT data FROM '%q'.'%q_node' WHERE nodeno = :1",
    "INSERT OR REPLACE INTO '%q'.'%q_node' VALUES(:1, :2)",
    "DELETE FROM '%q'.'%q_node' WHERE nodeno = :1",

    /* Read and write the xxx_rowid table 读和写xxx_rowid 表*/
    "SELECT nodeno FROM '%q'.'%q_rowid' WHERE rowid = :1",
    "INSERT OR REPLACE INTO '%q'.'%q_rowid' VALUES(:1, :2)",
    "DELETE FROM '%q'.'%q_rowid' WHERE rowid = :1",

    /* Read and write the xxx_parent table 读和写xxx_parent 表*/
    "SELECT parentnode FROM '%q'.'%q_parent' WHERE nodeno = :1",
    "INSERT OR REPLACE INTO '%q'.'%q_parent' VALUES(:1, :2)",
    "DELETE FROM '%q'.'%q_parent' WHERE nodeno = :1"
  };
  sqlite3_stmt **appStmt[N_STATEMENT];
  int i;

  pRtree->db = db;

  if( isCreate ){
    char *zCreate = sqlite3_mprintf(
"CREATE TABLE \"%w\".\"%w_node\"(nodeno INTEGER PRIMARY KEY, data BLOB);"
"CREATE TABLE \"%w\".\"%w_rowid\"(rowid INTEGER PRIMARY KEY, nodeno INTEGER);"
"CREATE TABLE \"%w\".\"%w_parent\"(nodeno INTEGER PRIMARY KEY, parentnode INTEGER);"
"INSERT INTO '%q'.'%q_node' VALUES(1, zeroblob(%d))",
      zDb, zPrefix, zDb, zPrefix, zDb, zPrefix, zDb, zPrefix, pRtree->iNodeSize
    );
    if( !zCreate ){
      return SQLITE_NOMEM;
    }
    rc = sqlite3_exec(db, zCreate, 0, 0, 0);
    sqlite3_free(zCreate);
    if( rc!=SQLITE_OK ){
      return rc;
    }
  }

  appStmt[0] = &pRtree->pReadNode;
  appStmt[1] = &pRtree->pWriteNode;
  appStmt[2] = &pRtree->pDeleteNode;
  appStmt[3] = &pRtree->pReadRowid;
  appStmt[4] = &pRtree->pWriteRowid;
  appStmt[5] = &pRtree->pDeleteRowid;
  appStmt[6] = &pRtree->pReadParent;
  appStmt[7] = &pRtree->pWriteParent;
  appStmt[8] = &pRtree->pDeleteParent;

  for(i=0; i<N_STATEMENT && rc==SQLITE_OK; i++){
    char *zSql = sqlite3_mprintf(azSql[i], zDb, zPrefix);
    if( zSql ){
      rc = sqlite3_prepare_v2(db, zSql, -1, appStmt[i], 0); 
    }else{
      rc = SQLITE_NOMEM;
    }
    sqlite3_free(zSql);
  }

  return rc;
}

/*
** The second argument to this function contains the text of an SQL statement
** that returns a single integer value. The statement is compiled and executed
** using database connection db. If successful, the integer value returned
** is written to *piVal and SQLITE_OK returned. Otherwise, an SQLite error
** code is returned and the value of *piVal after returning is not defined.
** 这个函数的第二个参数包含了一个SQL语句,方法返回单独的一个整型值。
** 这个语句被编译和执行是用数据库链接db时
** 如果成功这个整型值将返回并将值写入到pival，返回SQLITE_OK
** 否则返回 一个SQLite error，返回后piVal的值是未定义的。
*/
static int getIntFromStmt(sqlite3 *db, const char *zSql, int *piVal){
  int rc = SQLITE_NOMEM;
  if( zSql ){
    sqlite3_stmt *pStmt = 0;
    rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
    if( rc==SQLITE_OK ){
      if( SQLITE_ROW==sqlite3_step(pStmt) ){
        *piVal = sqlite3_column_int(pStmt, 0);
      }
      rc = sqlite3_finalize(pStmt);
    }
  }
  return rc;
}

/*
** This function is called from within the xConnect() or xCreate() method to
** determine the node-size used by the rtree table being created or connected
** to. If successful, pRtree->iNodeSize is populated and SQLITE_OK returned.
** Otherwise, an SQLite error code is returned.
** 当R树表被创建或链接的时候回用这个函数被调用通过xConnet()或xCreate()方法去决定节点的大小
** 如果成功pRtree->iNodeSize被填充并返回SQLITE_OK
**
** If this function is being called as part of an xConnect(), then the rtree
** table already exists. In this case the node-size is determined by inspecting
** the root node of the tree.
** 如果这个函数作为xConnet()方法的一部分调用，则R树表已经存在。
** 在这种情况是这个节点的大小通过检查该树的根节点来决定
**
** Otherwise, for an xCreate(), use 64 bytes less than the database page-size. 
** This ensures that each node is stored on a single database page. If the 
** database page-size is so large that more than RTREE_MAXCELLS entries 
** would fit in a single node, use a smaller node-size.
** 另外，对于xCreate()函数，用少于数据库页大小64字节做为节点大小，
** 这可以保证每个结点都可以存储在单张数据页中，
** 如果数据库页远大于单个结点的最大R树最大单元数记录为了适应一个单独节点，则使用较小的结点大小
*/
static int getNodeSize(
  sqlite3 *db,                    /* Database handle */
  Rtree *pRtree,                  /* Rtree handle */
  int isCreate                    /* True for xCreate, false for xConnect */
){
  int rc;
  char *zSql;
  if( isCreate ){
    int iPageSize = 0;
    zSql = sqlite3_mprintf("PRAGMA %Q.page_size", pRtree->zDb);
    rc = getIntFromStmt(db, zSql, &iPageSize);
    if( rc==SQLITE_OK ){
      pRtree->iNodeSize = iPageSize-64;
      if( (4+pRtree->nBytesPerCell*RTREE_MAXCELLS)<pRtree->iNodeSize ){
        pRtree->iNodeSize = 4+pRtree->nBytesPerCell*RTREE_MAXCELLS;
      }
    }
  }else{
    zSql = sqlite3_mprintf(
        "SELECT length(data) FROM '%q'.'%q_node' WHERE nodeno = 1",
        pRtree->zDb, pRtree->zName
    );
    rc = getIntFromStmt(db, zSql, &pRtree->iNodeSize);
  }

  sqlite3_free(zSql);
  return rc;
}

/* 
** This function is the implementation of both the xConnect and xCreate
** methods of the r-tree virtual table.
** 这个方法实现了R树虚拟表的链接和创建两个方法
**
**   argv[0]   -> module name    //模块名称
**   argv[1]   -> database name  //数据库名称
**   argv[2]   -> table name     // 表名
**   argv[...] -> column names...  ///列名
*/
static int rtreeInit(
  sqlite3 *db,                        /* Database connection 数据库链接*/
  void *pAux,                         /* One of the RTREE_COORD_* constants RTREE_COORD_*约束的其中一个*/
  int argc, const char *const*argv,   /* Parameters to CREATE TABLE statement 创建表预计的参数*/
  sqlite3_vtab **ppVtab,              /* OUT: New virtual table 输出：新的虚拟表*/
  char **pzErr,                       /* OUT: Error message, if any 输出：任何错误信息*/
  int isCreate                        /* True for xCreate, false for xConnect 创建成果，链接错误*/
){
  int rc = SQLITE_OK;
  Rtree *pRtree;
  int nDb;              /* Length of string argv[1] 数据库字符串数组的长度*/
  int nName;            /* Length of string argv[2] 名称字符串数组的长度*/
  int eCoordType = (pAux ? RTREE_COORD_INT32 : RTREE_COORD_REAL32);

  const char *aErrMsg[] = {
    0,                                                    /* 0 */
    "Wrong number of columns for an rtree table",         /* 1 */
    "Too few columns for an rtree table",                 /* 2 */
    "Too many columns for an rtree table"                 /* 3 */
  };

  int iErr = (argc<6) ? 2 : argc>(RTREE_MAX_DIMENSIONS*2+4) ? 3 : argc%2;
  if( aErrMsg[iErr] ){
    *pzErr = sqlite3_mprintf("%s", aErrMsg[iErr]);
    return SQLITE_ERROR;
  }

  sqlite3_vtab_config(db, SQLITE_VTAB_CONSTRAINT_SUPPORT, 1);

  /* Allocate the sqlite3_vtab structure 分配sqlite虚拟表结构*/
  nDb = (int)strlen(argv[1]);
  nName = (int)strlen(argv[2]);
  pRtree = (Rtree *)sqlite3_malloc(sizeof(Rtree)+nDb+nName+2);
  if( !pRtree ){
    return SQLITE_NOMEM;
  }
  memset(pRtree, 0, sizeof(Rtree)+nDb+nName+2);
  pRtree->nBusy = 1;
  pRtree->base.pModule = &rtreeModule;
  pRtree->zDb = (char *)&pRtree[1];
  pRtree->zName = &pRtree->zDb[nDb+1];
  pRtree->nDim = (argc-4)/2;
  pRtree->nBytesPerCell = 8 + pRtree->nDim*4*2;
  pRtree->eCoordType = eCoordType;
  memcpy(pRtree->zDb, argv[1], nDb);
  memcpy(pRtree->zName, argv[2], nName);

  /* Figure out the node size to use. 算出节点大小来使用*/
  rc = getNodeSize(db, pRtree, isCreate);

  /* Create/Connect to the underlying relational database schema. If
  ** that is successful, call sqlite3_declare_vtab() to configure
  ** the r-tree table schema.
  ** 创建或链接底层关系型数据库架构
  ** 如果成功则调用sqlite3_declare_vtab()方法去配置该R树表架构
  */
  if( rc==SQLITE_OK ){
    if( (rc = rtreeSqlInit(pRtree, db, argv[1], argv[2], isCreate)) ){
      *pzErr = sqlite3_mprintf("%s", sqlite3_errmsg(db));
    }else{
      char *zSql = sqlite3_mprintf("CREATE TABLE x(%s", argv[3]);
      char *zTmp;
      int ii;
      for(ii=4; zSql && ii<argc; ii++){
        zTmp = zSql;
        zSql = sqlite3_mprintf("%s, %s", zTmp, argv[ii]);
        sqlite3_free(zTmp);
      }
      if( zSql ){
        zTmp = zSql;
        zSql = sqlite3_mprintf("%s);", zTmp);
        sqlite3_free(zTmp);
      }
      if( !zSql ){
        rc = SQLITE_NOMEM;
      }else if( SQLITE_OK!=(rc = sqlite3_declare_vtab(db, zSql)) ){
        *pzErr = sqlite3_mprintf("%s", sqlite3_errmsg(db));
      }
      sqlite3_free(zSql);
    }
  }

  if( rc==SQLITE_OK ){
    *ppVtab = (sqlite3_vtab *)pRtree;
  }else{
    rtreeRelease(pRtree);
  }
  return rc;
}


/*
** Implementation of a scalar function that decodes r-tree nodes to
** human readable strings. This can be used for debugging and analysis.
** 实现一个解码R树节点为人可读字符串的标量函数。
** 它能用于调试和分析
** The scalar function takes two arguments, a blob of data containing
** an r-tree node, and the number of dimensions the r-tree indexes.
** For a two-dimensional r-tree structure called "rt", to deserialize
** all nodes, a statement like:
** 这个解码函数有两个参数，一个是包含了R树节点数据的二进制大对象，一个是R树索引维度的数量
** 对于一个二维R树结构被成“rt” 为了描述所有节点  一个语句像：
**   SELECT rtreenode(2, data) FROM rt_node;
**
** The human readable string takes the form of a Tcl list with one
** entry for each cell in the r-tree node. Each entry is itself a
** list, containing the 8-byte rowid/pageno followed by the 
** <num-dimension>*2 coordinates.
** 该可读字符串采用了一种Tcl列表格式，这个格式是在R树节点的每个单元都有一个记录
** 每个记录本身就是列表，包含一个在N维*2坐标之后的8字节的行id或页码
** SELECT rtreenode(2, data) FROM rt_node;
*/
static void rtreenode(sqlite3_context *ctx, int nArg, sqlite3_value **apArg){
  char *zText = 0;
  RtreeNode node;
  Rtree tree;
  int ii;

  UNUSED_PARAMETER(nArg);
  memset(&node, 0, sizeof(RtreeNode));
  memset(&tree, 0, sizeof(Rtree));
  tree.nDim = sqlite3_value_int(apArg[0]);
  tree.nBytesPerCell = 8 + 8 * tree.nDim;
  node.zData = (u8 *)sqlite3_value_blob(apArg[1]);

  for(ii=0; ii<NCELL(&node); ii++){
    char zCell[512];
    int nCell = 0;
    RtreeCell cell;
    int jj;

    nodeGetCell(&tree, &node, ii, &cell);
    sqlite3_snprintf(512-nCell,&zCell[nCell],"%lld", cell.iRowid);
    nCell = (int)strlen(zCell);
    for(jj=0; jj<tree.nDim*2; jj++){
#ifndef SQLITE_RTREE_INT_ONLY
      sqlite3_snprintf(512-nCell,&zCell[nCell], " %f",
                       (double)cell.aCoord[jj].f);
#else
      sqlite3_snprintf(512-nCell,&zCell[nCell], " %d",
                       cell.aCoord[jj].i);
#endif
      nCell = (int)strlen(zCell);
    }

    if( zText ){
      char *zTextNew = sqlite3_mprintf("%s {%s}", zText, zCell);
      sqlite3_free(zText);
      zText = zTextNew;
    }else{
      zText = sqlite3_mprintf("{%s}", zCell);
    }
  }
  
  sqlite3_result_text(ctx, zText, -1, sqlite3_free);
}

static void rtreedepth(sqlite3_context *ctx, int nArg, sqlite3_value **apArg){
  UNUSED_PARAMETER(nArg);
  if( sqlite3_value_type(apArg[0])!=SQLITE_BLOB 
   || sqlite3_value_bytes(apArg[0])<2
  ){
    sqlite3_result_error(ctx, "Invalid argument to rtreedepth()", -1); 
  }else{
    u8 *zBlob = (u8 *)sqlite3_value_blob(apArg[0]);
    sqlite3_result_int(ctx, readInt16(zBlob));
  }
}

/*
** Register the r-tree module with database handle db. This creates the
** virtual table module "rtree" and the debugging/analysis scalar 
** function "rtreenode".
** 用数据库柄db来注册一个R数模块，这创建了虚拟表模块R树和调试、分析标量函数rtreenode
*/
int sqlite3RtreeInit(sqlite3 *db){
  const int utf8 = SQLITE_UTF8;
  int rc;

  rc = sqlite3_create_function(db, "rtreenode", 2, utf8, 0, rtreenode, 0, 0);
  if( rc==SQLITE_OK ){
    rc = sqlite3_create_function(db, "rtreedepth", 1, utf8, 0,rtreedepth, 0, 0);
  }
  if( rc==SQLITE_OK ){
#ifdef SQLITE_RTREE_INT_ONLY
    void *c = (void *)RTREE_COORD_INT32;
#else
    void *c = (void *)RTREE_COORD_REAL32;
#endif
    rc = sqlite3_create_module_v2(db, "rtree", &rtreeModule, c, 0);
  }
  if( rc==SQLITE_OK ){
    void *c = (void *)RTREE_COORD_INT32;
    rc = sqlite3_create_module_v2(db, "rtree_i32", &rtreeModule, c, 0);
  }

  return rc;
}

/*
** A version of sqlite3_free() that can be used as a callback. This is used
** in two places - as the destructor for the blob value returned by the
** invocation of a geometry function, and as the destructor for the geometry
** functions themselves.
** sqlite3_free()方法的这一个版本作为一个回调函数。
** 它被用在两个地方：
** 一个是作为通过调用一个几何函数返回的二进制大对象值的析构函数
** 一个是作为几何函数本身的析构函数
*/
static void doSqlite3Free(void *p){
  sqlite3_free(p);
}

/*
** Each call to sqlite3_rtree_geometry_callback() creates an ordinary SQLite
** scalar user function. This C function is the callback used for all such
** registered SQL functions.
**
** 每次调用sqlite3_rtree_geometry_callback()将创建一个普通的标量用户函数，
** 这个函数回调用于注册的所有SQL功能

** The scalar user functions return a blob that is interpreted by r-tree
** table MATCH operators.
** 这个标量用户函数返回一个被R树表的匹配操作解析过的二进制大对象数据
*/
static void geomCallback(sqlite3_context *ctx, int nArg, sqlite3_value **aArg){
  RtreeGeomCallback *pGeomCtx = (RtreeGeomCallback *)sqlite3_user_data(ctx);
  RtreeMatchArg *pBlob;
  int nBlob;

  nBlob = sizeof(RtreeMatchArg) + (nArg-1)*sizeof(RtreeDValue);
  pBlob = (RtreeMatchArg *)sqlite3_malloc(nBlob);
  if( !pBlob ){
    sqlite3_result_error_nomem(ctx);
  }else{
    int i;
    pBlob->magic = RTREE_GEOMETRY_MAGIC;
    pBlob->xGeom = pGeomCtx->xGeom;
    pBlob->pContext = pGeomCtx->pContext;
    pBlob->nParam = nArg;
    for(i=0; i<nArg; i++){
#ifdef SQLITE_RTREE_INT_ONLY
      pBlob->aParam[i] = sqlite3_value_int64(aArg[i]);
#else
      pBlob->aParam[i] = sqlite3_value_double(aArg[i]);
#endif
    }
    sqlite3_result_blob(ctx, pBlob, nBlob, doSqlite3Free);
  }
}

/*
** Register a new geometry function for use with the r-tree MATCH operator.
** 为R树匹配操作的使用注册一个新的函数
*/
int sqlite3_rtree_geometry_callback(
  sqlite3 *db,
  const char *zGeom,
  int (*xGeom)(sqlite3_rtree_geometry *, int, RtreeDValue *, int *),
  void *pContext
){
  RtreeGeomCallback *pGeomCtx;      /* Context object for new user-function 用户函数的上下文对象*/

  /* Allocate and populate the context object.  分配并设置上下文对象*/
  pGeomCtx = (RtreeGeomCallback *)sqlite3_malloc(sizeof(RtreeGeomCallback));
  if( !pGeomCtx ) return SQLITE_NOMEM;
  pGeomCtx->xGeom = xGeom;
  pGeomCtx->pContext = pContext;

  /* Create the new user-function. Register a destructor function to delete
  ** the context object when it is no longer required. 
  ** 创建新的用户函数,注册一个析构函数来删除不再需要的上下文对象.
  */
  return sqlite3_create_function_v2(db, zGeom, -1, SQLITE_ANY, 
      (void *)pGeomCtx, geomCallback, 0, 0, doSqlite3Free
  );
}

#if !SQLITE_CORE
int sqlite3_extension_init(
  sqlite3 *db,
  char **pzErrMsg,
  const sqlite3_api_routines *pApi
){
  SQLITE_EXTENSION_INIT2(pApi)
  return sqlite3RtreeInit(db);
}
#endif

#endif
