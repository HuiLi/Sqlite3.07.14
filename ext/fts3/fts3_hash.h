/*
** 2001 September 22
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This is the header file for the generic hash-table implemenation
** used in SQLite.  We've modified it slightly to serve as a standalone
** hash table implementation for the full-text indexing module.
**
*/
//这是在SQLite中使用的一般哈希表的实现，我们为了实现全文本索引模块做了一定的修改add by guancan 20141209
#ifndef _FTS3_HASH_H_
#define _FTS3_HASH_H_

/* Forward declarations of structures. */
typedef struct Fts3Hash Fts3Hash;
typedef struct Fts3HashElem Fts3HashElem;

/* A complete hash table is an instance of the following structure.
** The internals of this structure are intended to be opaque -- client
** code should not attempt to access or modify the fields of this structure
** directly.  Change this structure only by using the routines below.
** However, many of the "procedures" and "functions" for modifying and
** accessing this structure are really macros, so we can't really make
** this structure opaque.
*/
/* 一个完整的哈希表是下列结构的一个实例。
  该结构体的内部趋向于隐藏--客户端代码不应该企图去直接获取或者修改该结构体的作用域。
  只能通过使用下面的途径去修改该结构体，然而，许多程序或者函数通过宏去修改或者获取该结构体，
  所以我们不能真正将该结构体隐藏. 
  add by guancan 20141209
*/
struct Fts3Hash {
  //keyClass只能有四种类型即HASH_INT, _POINTER, _STRING, _BINARY，其中
  //HASH_INT表示1，_POINTER表示2，_STRING表示3, _BINARY表示4。在这其中
  //HASH_INT和_POINTER已经不再使用。add by guancan 20141209
  char keyClass;          /* HASH_INT, _POINTER, _STRING, _BINARY */
  //在插入操作中copykey的值为真 add by guancan 20141209
  char copyKey;           /* True if copy of key made on insert */
  //在表中的实体数目 add by guancan 20141209
  int count;              /* Number of entries in this table */
  //数组中的第一个元素
  Fts3HashElem *first;    /* The first element of the array */
  //在哈希表中的桶的数目add by guancan 20141209
  int htsize;             /* Number of buckets in the hash table */
  //结构体中的子结构体，该结构体表示一个哈希表add by guancan 20141209
  struct _fts3ht {        /* the hash table */
    //哈希表中的实体数目add by guancan 20141209
    int count;               /* Number of entries with this hash */
    //链表的结构体add by guancan 20141209
    Fts3HashElem *chain;     /* Pointer to first entry with this hash */
  } *ht;
};

/* Each element in the hash table is an instance of the following 
** structure.  All elements are stored on a single doubly-linked list.
**
** Again, this structure is intended to be opaque, but it can't really
** be opaque because it is used by macros.
*/
/*
  在哈希表中的每一个元素都是以下结构体的一个实例。所有的元素都被以单一的
  双向链表的形式进行存储。再次重申，该结构体设计意图是让他成为被封装好的，
  但是事实上因为他被以宏的方式来使用所以不能做到完全隐藏实现细节
  add by guancan 20141209

*/
  //Fts3HashElem可以理解为一个双向链表的node的定义add by guancan 20141209
struct Fts3HashElem {
  Fts3HashElem *next, *prev; /* Next and previous elements in the table */
  void *data;                /* Data associated with this element */
  void *pKey; int nKey;      /* Key associated with this element */
};

/*
** There are 2 different modes of operation for a hash table:
**
**   FTS3_HASH_STRING        pKey points to a string that is nKey bytes long
**                           (including the null-terminator, if any).  Case
**                           is respected in comparisons.
**
**   FTS3_HASH_BINARY        pKey points to binary data nKey bytes long. 
**                           memcmp() is used to compare keys.
**
** A copy of the key is made if the copyKey parameter to fts3HashInit is 1.  
*/
/*
  在一个哈希表中分别有两种不同形式的操作：
**   FTS3_HASH_STRING       pKey指向一个长度是nKey字节的字符串（如果可以，包括空串）
**                          在比较的案例中也可以被接受
**
**   FTS3_HASH_BINARY        pKey指向长度为nKey字节的二进制数据，memcmp（）被用来进行
**                           比较key的操作。 
**
** 当fts3HashInit函数中的参数copyKey的值是1的时候进行key的拷贝操作。
** add by guancan 20141209  
*/
//哈希表两种不同操作的宏定义，String操作和binary操作，值分别为1和2 add by guancan 20141209
#define FTS3_HASH_STRING    1
#define FTS3_HASH_BINARY    2

/*
** Access routines.  To delete, insert a NULL pointer.
*/
//下面的函数分别表示哈希表的操作 add by guancan 20141209
//哈希表的初始化操作add by guancan 20141209
void sqlite3Fts3HashInit(Fts3Hash *pNew, char keyClass, char copyKey);
//哈希表的插入操作 add by guancan 20141209
void *sqlite3Fts3HashInsert(Fts3Hash*, const void *pKey, int nKey, void *pData);
//哈希表的查找操作add by guancan 20141209
void *sqlite3Fts3HashFind(const Fts3Hash*, const void *pKey, int nKey);
//哈希表的清除操作add by guancan 20141209
void sqlite3Fts3HashClear(Fts3Hash*);
//哈希表的查找元素的操作add by guancan 20141209
Fts3HashElem *sqlite3Fts3HashFindElem(const Fts3Hash *, const void *, int);

/*
** Shorthand for the functions above
*/
/*上面函数的简写的宏定义add by guancan 20141209
在C语言中这种写法很常见，特别是在linux内核中存在很多结构体的元素是一个函数的情形
个人认为这也是C语言的精华之处
add by guancan 20141209*/
//哈希表的初始化操作简写的宏定义add by guancan 20141209
#define fts3HashInit     sqlite3Fts3HashInit
//哈希表的插入操作简写的宏定义 add by guancan 20141209
#define fts3HashInsert   sqlite3Fts3HashInsert
//哈希表的查找操作简写的宏定义add by guancan 20141209
#define fts3HashFind     sqlite3Fts3HashFind
//哈希表的清除操作简写的宏定义add by guancan 20141209
#define fts3HashClear    sqlite3Fts3HashClear
//哈希表的查找元素的操作简写的宏定义add by guancan 20141209
#define fts3HashFindElem sqlite3Fts3HashFindElem

/*
** Macros for looping over all elements of a hash table.  The idiom is
** like this:
**
**   Fts3Hash h;
**   Fts3HashElem *p;
**   ...
**   for(p=fts3HashFirst(&h); p; p=fts3HashNext(p)){
**     SomeStructure *pData = fts3HashData(p);
**     // do something with pData
**   }
*/
/*
**   循环遍历哈希表的所有元素的宏。习惯性这样写：
**   Fts3Hash h;
**   Fts3HashElem *p;
**   ...
**   for(p=fts3HashFirst(&h); p; p=fts3HashNext(p)){
**     SomeStructure *pData = fts3HashData(p);
**     // do something with pData
**   }
**   add by guancan 20141209
*/
/*下面的写法也是C语言宏定义中常见的用法之一，
  使用者为了隐藏细节，往往会用这种方式来写代码，
  我们可以把它理解为java中的get函数
  add by guancan 20141209
*/
//获取双向链表中node的前一个元素 add by guancan 20141209
#define fts3HashFirst(H)  ((H)->first)
//获取双向链表中node的后一个元素 add by guancan 20141209
#define fts3HashNext(E)   ((E)->next)
//获取双向链表中当前node的data add by guancan 20141209
#define fts3HashData(E)   ((E)->data)
//获取双向链表中当前node的hash key add by guancan 20141209
#define fts3HashKey(E)    ((E)->pKey)
//获取双向链表中当前node的hashkey的大小 add by guancan 20141209
#define fts3HashKeysize(E) ((E)->nKey)

/*
** Number of entries in a hash table
*/
//获取哈希表中实体元素的个数 add by guancan 20141209
#define fts3HashCount(H)  ((H)->count)

#endif /* _FTS3_HASH_H_ */
