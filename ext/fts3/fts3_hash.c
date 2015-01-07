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
** This is the implementation of generic hash-tables used in SQLite.
** We've modified it slightly to serve as a standalone hash table
** implementation for the full-text indexing module.
*/
//这是在SQLite中FTS使用的一般哈希表的实现，我们为了实现全文本索引模块做了一定的修改add by guancan 20141209
/*
** The code in this file is only compiled if:
**
**     * The FTS3 module is being built as an extension
**       (in which case SQLITE_CORE is not defined), or
**
**     * The FTS3 module is being built into the core of
**       SQLite (in which case SQLITE_ENABLE_FTS3 is defined).
*/
//在该文件中的代码仅仅当下列情况下才会被编译
//（1）当SQLITE_CORE没有被定义时如果FTS3模块作为一个扩展被创建
//（2）当SQLITE_ENABLE_FTS3被定义的时候如果FTS3模块作为一个扩展被创建
#include "fts3Int.h"
#if !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_FTS3)

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "fts3_hash.h"

/*
** Malloc and Free functions
*/
//申请内存空间并且初始化为0，add by guancan 20141209
static void *fts3HashMalloc(int n){
  void *p = sqlite3_malloc(n);
  if( p ){
    memset(p, 0, n);
  }
  return p;
}
//释放内存空间 add by guancan 20141209
static void fts3HashFree(void *p){
  sqlite3_free(p);
}

/* Turn bulk memory into a hash table object by initializing the
** fields of the Hash structure.
**
** "pNew" is a pointer to the hash table that is to be initialized.
** keyClass is one of the constants 
** FTS3_HASH_BINARY or FTS3_HASH_STRING.  The value of keyClass 
** determines what kind of key the hash table will use.  "copyKey" is
** true if the hash table should make its own private copy of keys and
** false if it should just use the supplied pointer.
*/
//通过初始化哈希结构的作用于来打开
//"pNew"是一个被初始化了的哈希表的指针。
//其中keyClass是FTS3_HASH_BINARY或者FTS3_HASH_STRING中的一个.其中FTS3_HASH_BINARY的值为2，FTS3_HASH_STRING的值为1
//keyClass定义了将会使用哪一种哈希表。add by guancan 20141209
//如果哈希表将使用自己的私有拷贝则"copyKey"的值为true，如果仅仅只作为一个指针使用则值为false。
//add by guancan 20141206
void sqlite3Fts3HashInit(Fts3Hash *pNew, char keyClass, char copyKey){
  assert( pNew!=0 );
  assert( keyClass>=FTS3_HASH_STRING && keyClass<=FTS3_HASH_BINARY );
  pNew->keyClass = keyClass;
  pNew->copyKey = copyKey;
  pNew->first = 0;
  pNew->count = 0;
  pNew->htsize = 0;
  pNew->ht = 0;
}

/* Remove all entries from a hash table.  Reclaim all memory.
** Call this routine to delete a hash table or to reset a hash table
** to the empty state.
*/
//从哈希表中清除所有的实体，回收所有的内存，调用该函数主要用来删除一个哈希表或者
//将一个哈希表重置到空表状态 add by guancan 20141209
void sqlite3Fts3HashClear(Fts3Hash *pH){
  Fts3HashElem *elem;         /* For looping over all elements of the table */
// Fts3HashElem该结构体中存在一个data指针和一个key指针，另外还有两个指针分别指向前一个和下一个Fts3HashElem，可以理解为
//一个双向链表 add by guancan 20141209
  assert( pH!=0 );
  elem = pH->first;
  pH->first = 0;
  fts3HashFree(pH->ht);
  pH->ht = 0;
  pH->htsize = 0;
  //循环释放双向链表中每一个node占用的内存 add by guancan 20141210
  while( elem ){
    Fts3HashElem *next_elem = elem->next;
    if( pH->copyKey && elem->pKey ){
      fts3HashFree(elem->pKey);
    }
    fts3HashFree(elem);
    elem = next_elem;
  }
  pH->count = 0;
}

/*
** Hash and comparison functions when the mode is FTS3_HASH_STRING
*/
//当模式是FTS3_HASH_STRING时的哈希函数 add by guancan 20141210
static int fts3StrHash(const void *pKey, int nKey){
  const char *z = (const char *)pKey;
  int h = 0;
  //个人认为代码这样写比较ugly，容易引发误解 add by guancan 20141210
  if( nKey<=0 ) nKey = (int) strlen(z);
  while( nKey > 0  ){
    h = (h<<3) ^ h ^ *z++;
    nKey--;
  }
  return h & 0x7fffffff;
}
//当模式是FTS3_HASH_STRING时的比较函数 add by guancan 20141210
static int fts3StrCompare(const void *pKey1, int n1, const void *pKey2, int n2){
  if( n1!=n2 ) return 1;
  return strncmp((const char*)pKey1,(const char*)pKey2,n1);
}

/*
** Hash and comparison functions when the mode is FTS3_HASH_BINARY
*/
//当模式是FTS3_HASH_BINARY时的哈希函数 add by guancan 20141210
static int fts3BinHash(const void *pKey, int nKey){
  int h = 0;
  const char *z = (const char *)pKey;
  while( nKey-- > 0 ){
    h = (h<<3) ^ h ^ *(z++);
  }
  return h & 0x7fffffff;
}
//当模式是FTS3_HASH_BINARY时的比较函数 add by guancan 20141210
static int fts3BinCompare(const void *pKey1, int n1, const void *pKey2, int n2){
  if( n1!=n2 ) return 1;
  return memcmp(pKey1,pKey2,n1);
}

/*
** Return a pointer to the appropriate hash function given the key class.
**
** The C syntax in this function definition may be unfamilar to some 
** programmers, so we provide the following additional explanation:
**
** The name of the function is "ftsHashFunction".  The function takes a
** single parameter "keyClass".  The return value of ftsHashFunction()
** is a pointer to another function.  Specifically, the return value
** of ftsHashFunction() is a pointer to a function that takes two parameters
** with types "const void*" and "int" and returns an "int".
*/
/* 
**在该函数定义中的C语法可能对于很多程序员来说比较不常见，因此我们提供以下解释：
**该函数的函数名是ftsHashFunction。该函数只有一个形参"keyClass".该函数
**ftsHashFunction()的返回值是是一个指向另一个函数的指针。特殊地，函数ftsHashFunction()
**的返回值是一个指针，该指针指向的函数是一个有着两个形参，类型分别为"const void*"和"int"
**而且返回值是int。
**这个用法真心好诡异 add by guancan 20141215
*/
static int (*ftsHashFunction(int keyClass))(const void*,int){
  if( keyClass==FTS3_HASH_STRING ){
    return &fts3StrHash;
  }else{
    assert( keyClass==FTS3_HASH_BINARY );
    return &fts3BinHash;
  }
}

/*
** Return a pointer to the appropriate hash function given the key class.
**
** For help in interpreted the obscure C code in the function definition,
** see the header comment on the previous function.
*/
/*传入一个keyClass，返回一个指针指向一个哈希函数,该函数的写法和
***ftsHashFunction(int keyClass)相同
**add by guancan 20141215
*/
static int (*ftsCompareFunction(int keyClass))(const void*,int,const void*,int){
  if( keyClass==FTS3_HASH_STRING ){
    return &fts3StrCompare;
  }else{
    assert( keyClass==FTS3_HASH_BINARY );
    return &fts3BinCompare;
  }
}

/* Link an element into the hash table
*/
/*向哈希表中插入一个元素
**add by guancan 20141215
*/
static void fts3HashInsertElement(
  Fts3Hash *pH,            /* The complete hash table */
  struct _fts3ht *pEntry,  /* The entry into which pNew is inserted */
  Fts3HashElem *pNew       /* The element to be inserted */
){
  Fts3HashElem *pHead;     /* First element already in pEntry */
  pHead = pEntry->chain;
  if( pHead ){
    pNew->next = pHead;
    pNew->prev = pHead->prev;
    if( pHead->prev ){ pHead->prev->next = pNew; }
    else             { pH->first = pNew; }
    pHead->prev = pNew;
  }else{
    pNew->next = pH->first;
    if( pH->first ){ pH->first->prev = pNew; }
    pNew->prev = 0;
    pH->first = pNew;
  }
  pEntry->count++;
  pEntry->chain = pNew;
}


/* Resize the hash table so that it cantains "new_size" buckets.
** "new_size" must be a power of 2.  The hash table might fail 
** to resize if sqliteMalloc() fails.
**
** Return non-zero if a memory allocation error occurs.
*/
/*重置哈希表的大小以便于他包含"new_size"的桶。
**"new_size"必须是原来的2倍。如果分配内存空间失败，哈希表会重新分配内存大小失败。
**如果内存重分配失败，那么返货一个non-zero.
**add by guancan 20141215
*/
static int fts3Rehash(Fts3Hash *pH, int new_size){
  struct _fts3ht *new_ht;          /* The new hash table */
  Fts3HashElem *elem, *next_elem;  /* For looping over existing elements */
  int (*xHash)(const void*,int);   /* The hash function */

  assert( (new_size & (new_size-1))==0 );
  new_ht = (struct _fts3ht *)fts3HashMalloc( new_size*sizeof(struct _fts3ht) );
  if( new_ht==0 ) return 1;
  fts3HashFree(pH->ht);
  pH->ht = new_ht;
  pH->htsize = new_size;
  xHash = ftsHashFunction(pH->keyClass);
  for(elem=pH->first, pH->first=0; elem; elem = next_elem){
    int h = (*xHash)(elem->pKey, elem->nKey) & (new_size-1);
    next_elem = elem->next;
    fts3HashInsertElement(pH, &new_ht[h], elem);
  }
  return 0;
}

/* This function (for internal use only) locates an element in an
** hash table that matches the given key.  The hash for this key has
** already been computed and is passed as the 4th parameter.
*/
/*
** 该函数（仅供内部使用）在哈希表中通过给定的key值定位一个元素。 
** add by guancan 20141210
*/
static Fts3HashElem *fts3FindElementByHash(
  //需要被搜索的哈希表add by guancan 20141210 
  const Fts3Hash *pH, /* The pH to be searched */
  //外部传入的用于搜索元素的keyadd by guancan 20141210
  const void *pKey,   /* The key we are searching for */
  int nKey,
  //对应于该key的哈希值add by guancan 20141210
  int h               /* The hash for this key. */
){
  Fts3HashElem *elem;            /* Used to loop thru the element list */
  int count;                     /* Number of elements left to test */
  //字符串比较函数，如果两个整形参数不相等，那么返回1，如果相等则比较两个指针指向的内容，
  //具体比较结果参见C语言函数库中的 int strncmp (const char *s1, const char *s2, size_t size)
  //函数，该实现见Fts1_hash.c文件 add by guancan 20141210
  int (*xCompare)(const void*,int,const void*,int);  /* comparison function */

  if( pH->ht ){
    struct _fts3ht *pEntry = &pH->ht[h];
    elem = pEntry->chain;
    count = pEntry->count;
     //比较函数，该函数的函数体见Fts1_hash.c add by guancan 20141210
    xCompare = ftsCompareFunction(pH->keyClass);
    //循环遍历链表 add by guancan 20141210
    while( count-- && elem ){
      //检测到要查询的内容，返回该元素 add by guancan 20141210
      if( (*xCompare)(elem->pKey,elem->nKey,pKey,nKey)==0 ){ 
        return elem;
      }
      elem = elem->next;
    }
  }
  return 0;
}

/* Remove a single entry from the hash table given a pointer to that
** element and a hash on the element's key.
*/
/*移除哈希表中的一个实体
**add by guancan 20141215
**
*/
static void fts3RemoveElementByHash(
  Fts3Hash *pH,         /* The pH containing "elem" */
  Fts3HashElem* elem,   /* The element to be removed from the pH */
  int h                 /* Hash value for the element */
){
  struct _fts3ht *pEntry;
  if( elem->prev ){
    elem->prev->next = elem->next; 
  }else{
    pH->first = elem->next;
  }
  if( elem->next ){
    elem->next->prev = elem->prev;
  }
  pEntry = &pH->ht[h];
  if( pEntry->chain==elem ){
    pEntry->chain = elem->next;
  }
  pEntry->count--;
  if( pEntry->count<=0 ){
    pEntry->chain = 0;
  }
  if( pH->copyKey && elem->pKey ){
    fts3HashFree(elem->pKey);
  }
  fts3HashFree( elem );
  pH->count--;
  if( pH->count<=0 ){
    assert( pH->first==0 );
    assert( pH->count==0 );
    fts3HashClear(pH);
  }
}
//查找哈希表中的元素 add by guancan 20141215
Fts3HashElem *sqlite3Fts3HashFindElem(
  const Fts3Hash *pH, 
  const void *pKey, 
  int nKey
){
  int h;                          /* A hash on key */
  int (*xHash)(const void*,int);  /* The hash function */

  if( pH==0 || pH->ht==0 ) return 0;
  xHash = ftsHashFunction(pH->keyClass);
  assert( xHash!=0 );
  h = (*xHash)(pKey,nKey);
  assert( (pH->htsize & (pH->htsize-1))==0 );
  return fts3FindElementByHash(pH,pKey,nKey, h & (pH->htsize-1));
}

/* 
** Attempt to locate an element of the hash table pH with a key
** that matches pKey,nKey.  Return the data for this element if it is
** found, or NULL if there is no match.
*/
/*试图通过一个key定位一个哈希表的元素pH，使之适合pKey,nKey.如果查找定位成功，那么返回元素
**所包含的data，否则返回NULL
**add by guancan 20141215
*/
void *sqlite3Fts3HashFind(const Fts3Hash *pH, const void *pKey, int nKey){
  Fts3HashElem *pElem;            /* The element that matches key (if any) */

  pElem = sqlite3Fts3HashFindElem(pH, pKey, nKey);
  return pElem ? pElem->data : 0;
}

/* Insert an element into the hash table pH.  The key is pKey,nKey
** and the data is "data".
**
** If no element exists with a matching key, then a new
** element is created.  A copy of the key is made if the copyKey
** flag is set.  NULL is returned.
**
** If another element already exists with the same key, then the
** new data replaces the old data and the old data is returned.
** The key is not copied in this instance.  If a malloc fails, then
** the new data is returned and the hash table is unchanged.
**
** If the "data" parameter to this function is NULL, then the
** element corresponding to "key" is removed from the hash table.
*/
/*
** 向哈希表pH中插入一个元素。其中元素的key是pKey，nKey和data是元素中的data。
** 如何在哈希表中没有和要插入的key相同的key，那么一个新的元素将会被创建。如果copyKey
** 标志为真则该key将会被拷贝。返回值是NULL。
** 如果在哈希表中已经存在和要插入的key相同的key，那么新的数据将会取代旧的数据并且旧的
** 数据将会被返回。在这个实例中key将不会被拷贝。如果内存分配失败，那么新的数据将会被返
** 回而且哈希表不会被改变。
** 如果参数data是空值，那么这个data所对应的key也会相应的从哈希表中移除。
** add by guancan 20141210
*/
void *sqlite3Fts3HashInsert(
  //要插入的哈希表 add by guancan 20141210
  Fts3Hash *pH,        /* The hash table to insert into */
  //key add by guancan 20141210
  const void *pKey,    /* The key */
  //key所占的字节数 add by guancan 20141210
  int nKey,            /* Number of bytes in the key */
  //哈希表中的data add by guancan 20141210
  void *data           /* The data */
){
  int hraw;                 /* Raw hash value of the key */
  int h;                    /* the hash of the key modulo hash table size */
  Fts3HashElem *elem;       /* Used to loop thru the element list */
  Fts3HashElem *new_elem;   /* New element added to the pH */
  int (*xHash)(const void*,int);  /* The hash function */

  assert( pH!=0 );
  xHash = ftsHashFunction(pH->keyClass);
  assert( xHash!=0 );
  hraw = (*xHash)(pKey, nKey);
  assert( (pH->htsize & (pH->htsize-1))==0 );
  h = hraw & (pH->htsize-1);
  elem = fts3FindElementByHash(pH,pKey,nKey,h);
  if( elem ){
    void *old_data = elem->data;
    if( data==0 ){
      fts3RemoveElementByHash(pH,elem,h);
    }else{
      elem->data = data;
    }
    return old_data;
  }
  if( data==0 ) return 0;
  if( (pH->htsize==0 && fts3Rehash(pH,8))
   || (pH->count>=pH->htsize && fts3Rehash(pH, pH->htsize*2))
  ){
    pH->count = 0;
    return data;
  }
  assert( pH->htsize>0 );
  new_elem = (Fts3HashElem*)fts3HashMalloc( sizeof(Fts3HashElem) );
  if( new_elem==0 ) return data;
  if( pH->copyKey && pKey!=0 ){
    new_elem->pKey = fts3HashMalloc( nKey );
    if( new_elem->pKey==0 ){
      fts3HashFree(new_elem);
      return data;
    }
    memcpy((void*)new_elem->pKey, pKey, nKey);
  }else{
    new_elem->pKey = (void*)pKey;
  }
  new_elem->nKey = nKey;
  pH->count++;
  assert( pH->htsize>0 );
  assert( (pH->htsize & (pH->htsize-1))==0 );
  h = hraw & (pH->htsize-1);
  fts3HashInsertElement(pH, &pH->ht[h], new_elem);
  new_elem->data = data;
  return 0;
}

#endif /* !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_FTS3) */
