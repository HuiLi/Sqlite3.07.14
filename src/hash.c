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
** This is the implementation of generic hash-tables
** used in SQLite.使用SQLite
*/
/*SQLite中哈希表的实现*/

/**************************************************************
* *通用哈希表的实现
* *用于SQLite。
* *SQLite中哈希表的实现
***************************************************************/

#include "sqliteInt.h"
#include <assert.h>

/* Turn bulk memory into a hash table object by initializing the
** fields of the Hash structure.
**
** "pNew" is a pointer to the hash table that is to be initialized.
*/

/*
把内存变成哈希表的一个对象去初始化哈希表的结构体
pNew是指针初始化的哈希表。
*/

/**************************************************************
* *初始化哈希表
* *把内存变成哈希表的一个对象去初始化哈希表的结构体
* *pNew是哈希表的初始化指针。
***************************************************************/

void sqlite3HashInit(Hash *pNew){/*hash_table initliaze *///Elaine debug 
  assert( pNew!=0 );/*如果括号中的值为0，就跳出；如果为1，继续执行*/    /*检测初始指针*/
  pNew->first = 0;/*initialize the pointer of pNew *///Elaine debug     /*哈希元素指针，指向入口项双向链表的表头并赋值为0 */
  pNew->count = 0;                                                      /*初始化哈希表中入口项的个数（记录的个数）*/
  pNew->htsize = 0;                                                     /*初始化哈希表存储时，哈希表存储大小*/
  pNew->ht = 0;                                                         /*哈希表存储结构，当使用哈希表存储时，将表现为一个桶的数组*/
}

/* Remove all entries from a hash table.  Reclaim all memory.
** Call this routine to delete a hash table or to reset a hash table
** to the empty state.
*/

/*
删除哈希表中的所有条目，回收内存。使用这个方法删除哈希表或者重置哈希表为空状态。
*/

/**************************************************************
* *删除哈希表中的所有条目，回收内存。
* *使用这个方法删除哈希表或者重置哈希表为空状态。
* *删除哈希表中的所有条目，回收内存。
***************************************************************/

void sqlite3HashClear(Hash *pH){
  HashElem *elem;         /* For looping over all elements of the table *//*循环表中的所有元素*/        /*循环表中元素*/

  assert( pH!=0 );/*判读PH是否为0,如果为0,则说明HASH为空，程序会中止，否则说明HASH不为空继续执行*/      /*判读PH是否为0,如果为0,则说明hash为空,程序中止，否则说明hash不为空继续执行*/
  elem = pH->first;                                                                                     /*指向双向链表的表头*/
  pH->first = 0;                                                                                        /*初始化表头值为0*/
  sqlite3_free(pH->ht);/*释放HASH函数的ht*/                                                             /*释放hash函数的ht*/
  pH->ht = 0;//使哈希表的ht为0                                                                          /*给哈希表的赋值为0*/
  pH->htsize = 0;/*htsize is unsigned int,*///Elaine debug                                              /*给哈希表空间赋初值*/
  while( elem ){//判断elem是否还存在，如果存在继续做释放的动作                                          /*判断elem是否有元素*/
    HashElem *next_elem = elem->next;                                                                   /*逐个逐个元素的检查*/
    sqlite3_free(elem);//释放elem                                                                       /*释放elem*/
    elem = next_elem;//elam等于他的下一个elem，依次后移                                                 /*elam指向他的下一个elem，依次后移*/
  }
  pH->count = 0;//表中的count置为0                                                                      /*表中的记录置为0*/
}

/*
** The hashing function.
*/
/*散列函数*/

/**************************************************************
* *散列函数。
* *定义散列函数。
***************************************************************/

static unsigned int strHash(const char *z, int nKey){                           /*定义散列函数*/
  int h = 0;                                                                    /*要计算的散列值*/
  assert( nKey>=0 );//如果nKey不大于0，则程序终止，如果大于0，则程序继续执行.   /*如果nKey不大于0，则程序终止，如果大于0，则程序继续执行.*/
  while( nKey > 0  ){//循环nKey的值，当等于0时，跳出循环.                       /*循环nKey的值，当等于0时，跳出循环.*/
    h = (h<<3) ^ h ^ sqlite3UpperToLower[(unsigned char)*z++];                  /*对h值进行处理*/
    nKey--;//nKEY=nKey-1                                                        /*nKEY自减，直到小于0时结束*/
  }
  return h;//返回h的数值                                                        /*返回h的数值，结束函数*/
}


/* Link pNew element into the hash table pH.  If pEntry!=0 then also
** insert pNew into the pEntry hash bucket.
*/

/*
 把pNew连接到哈希表pH上，如果pEntry不等于0 则插入语pNew到结构体
*/

/**************************************************************
* *把pNew连接到哈希表pH上。
* *如果pEntry不等于0 则插入语pNew到结构体。
* *向哈希表中插入元素
***************************************************************/

static void insertElement(                                                               /*定义插入函数，向哈希表中插入元素*/
  Hash *pH,              /* The complete hash table *///完整的哈希表                     /*完整的哈希表*/
  struct _ht *pEntry,    /* The entry into which pNew is inserted */                     /* pNew插入的进入地址*/
  HashElem *pNew         /* The element to be inserted *///被插入的元素                  /*被插入的元素 */
){
  HashElem *pHead;       /* First element already in pEntry */                           /* 第一个元素已经在pEntr中 */
  if( pEntry ){/*determine the pEntry is empty,*///Elaine debug                          /*如果地址存在*/
    pHead = pEntry->count ? pEntry->chain : 0;/*如果pEntry不空，则为pEntry结构赋值*/     /*判断要插入的空间有无记录，有则如果没有，则给头节点赋0*/
    pEntry->count++;                                                                     /*逐个访问*/
    pEntry->chain = pNew;                                                                /*循环链表*/
  }else{
    pHead = 0;//如果pEntry为0，则pHead为0                                                /*如果pEntry为0，则pHead为0*/
  }
  if( pHead ){//判断pHead的值                                                            /*判断pHead的值*/
    pNew->next = pHead;//pNew的next指向pHead                                             /*pNew的next指向pHead*/
    pNew->prev = pHead->prev;//pNew的prev指向pHead的prev                                 /*pNew的prev指向pHead的prev*/
    if( pHead->prev ){ pHead->prev->next = pNew; }//pHead的prev的next指向pNew            /*如果表头有前一个结点，pHead的prev的next指向pNew*/
    else             { pH->first = pNew; }                                               /*表头作为第一个结点*/
    pHead->prev = pNew;                                                                  /*并将表头赋值给它前一个结点*/
  }else{
    pNew->next = pH->first;//如果pHead为0，则pNew的下一个指向pH的first                   /*如果pHead为0，则pNew的下一个指向pH的first*/
    if( pH->first ){ pH->first->prev = pNew; }//pH的first的prev指向pNew                  /*指向表头结点，pH的first的prev指向pNew*/ 
    pNew->prev = 0;                                                                      /*把0赋值给prev*/
    pH->first = pNew;                                                                    /*给循环链表赋值*/
  }
}


/* Resize the hash table so that it cantains "new_size" buckets.
**
** The hash table might fail to resize if sqlite3_malloc() fails or
** if the new size is the same as the prior size.
** Return TRUE if the resize occurs and false if not.
*/

/*
调整哈希表的大小，使它可以包含new_size
如果哈希表申请空间失败或者新申请的大小和之前是一样的，哈希表重新改变大小是失败的。
如果重新改变大小发生或者失败没有发生，返回true
*/

/**************************************************************
* *调整哈希表的大小，使它可以包含new_size。
* *如果哈希表申请空间失败或者新申请的大小和之前是一样的。
* *哈希表重新改变大小是失败的
* *如果重新改变大小发生或者失败没有发生，返回true
* *定义一个静态的函数，用于调整hash表的大小
***************************************************************/


static int rehash(Hash *pH, unsigned int new_size){                                                      /* 定义一个静态函数，用于调整hash表的大小 */
  struct _ht *new_ht;            /* The new hash table */                                                /* 定义一个新的哈希表 */
  HashElem *elem, *next_elem;    /* For looping over existing elements */                                /* 定义哈希数组指针 */

#if SQLITE_MALLOC_SOFT_LIMIT>0 //如果定义的SQLITE_MALLOC_SOFT_LIMIT大于0，则执行下面的if语句，否则不执行 /*判断定义的SQLITE_MALLOC_SOFT_LIMIT是否大于0*/
  if( new_size*sizeof(struct _ht)>SQLITE_MALLOC_SOFT_LIMIT ){                                            /*如果新申请的大小大于之前的，执行if的语句*/
    new_size = SQLITE_MALLOC_SOFT_LIMIT/sizeof(struct _ht);                                              /*把原来的空间大小赋值给变量new_size(新的空间大小）*/
  }
  if( new_size==pH->htsize ) return 0;                                                                   /*如果哈希表新申请的大小和之前一样，则返回值为0，表示改变大小失败*/
#endif

  /* The inability to allocates space for a larger hash table is
  ** a performance hit but it is not a fatal error.  So mark the
  ** allocation as a benign. Use sqlite3Malloc()/memset(0) instead of 
  ** sqlite3MallocZero() to make the allocation, as sqlite3MallocZero()
  ** only zeroes the requested number of bytes whereas this module will
  ** use the actual amount of space allocated for the hash table (which
  ** may be larger than the requested amount).
  */
  
   /*
  无法分配一个较大的哈希表空间是一个性能损失，但这不是一个致命的错误。所以标记分配作为一种有利的。
  使用sqlite3Malloc()/memset(0)而不是sqlite3malloczero()进行分配，sqlite3malloczero()是零请
  求的字节数，这个模块将实际使用量的空间分配哈希表（可大于所要求的数量）。   
  */
  
/**************************************************************
* *无法分配一个较大的哈希表空间是一个性能损失，但这不是一个
* *致命的错误，所以标记分配作为一种有利的。
* *使用sqlite3Malloc()/memset(0)而不是sqlite3malloczero()进行分配
* *sqlite3malloczero()是零请
* *求的字节数，这个模块将实际使用量的空间分配哈希表。
***************************************************************/

  sqlite3BeginBenignMalloc();//在src/fault中有这个函数的函数体，必须与sqlite3EndBenignMalloc()配合使用                        /*在src/fault中有这个函数的函数体，必须与sqlite3EndBenignMalloc()配合使用*/
  new_ht = (struct _ht *)sqlite3Malloc( new_size*sizeof(struct _ht) );//申请新的大小                                          /*申请新的大小*/
  sqlite3EndBenignMalloc();//分配实际使用空间结束                                                                             /*分配实际使用空间结束*/

  if( new_ht==0 ) return 0;//如果新申请的大小等于0，返回0                                                                     /* 如果新申请的大小等于0，返回0*/
  sqlite3_free(pH->ht);//分配一个免费的空间                                                                                   /*分配一个免费的空间*/
  pH->ht = new_ht;//把new_ht的值赋值给pH->ht
  pH->htsize = new_size = sqlite3MallocSize(new_ht)/sizeof(struct _ht);/*htsize的大小等于申请空间大小/ht结构体所占字节大小*/  /*htsize的大小等于申请空间大小/ht结构体所占字节大小*/
  memset(new_ht, 0, new_size*sizeof(struct _ht));/*将新开辟空间new_ht设置为0*/                                                /*将新开辟空间new_ht设置为0*/
  for(elem=pH->first, pH->first=0; elem; elem = next_elem)//若数组的头指针指的值为0，则循环直到指针指向数组的最后一个数       /*若数组的头指针指的值为0，则循环直到指针指向数组的最后一个数*/
  {
    unsigned int h = strHash(elem->pKey, elem->nKey) % new_size;//定义一个无字符的整型变量h表示数组长度                       /*定义一个无字符的整型变量h表示数组长度*/
    next_elem = elem->next;//把elem的指针赋值给变量next_elem                                                                  /*把elem的指针赋值给变量next_elem*/
    insertElement(pH, &new_ht[h], elem);/*把elem连接到PH的哈希表的后面*/                                                      /*把elem连接到PH的哈希表的后面*/
  }
  return 1;//重新改变大小发生或者失败没有发生，返回true                                                                       /* 重新改变大小发生或者失败没有发生，返回true*/
}

/* This function (for internal use only) locates an element in an
** hash table that matches the given key.  The hash for this key has
** already been computed and is passed as the 4th parameter.
*/

/*这个函数（仅供内部使用）位于一个元素在一个哈希表相匹配的给定的键值。
这个键值已被计算并作为第四个参数传递。*/

**************************************************************
* *这个函数（仅供内部使用）位于一个元素
* *在一个哈希表相匹配的给定的键值。
* *这个键值已被计算并作为第四个参数传递。
* *PH被搜索的哈希表
* *PKEY被查找的键值
* *NKEY定义一个整型的键值(not counting zero terminator)
* *H定义一个无符号的整型变量h
* *定义一个静态的函数给哈希表中的匹配的元素定一个键值
***************************************************************/


static HashElem *findElementGivenHash(                                                                       /* 定义一个静态函数，给哈希表中的元素匹配一个键值。 */
  const Hash *pH,     /* The pH to be searched *//*被搜索的哈希表*/                                          /* 定义被搜索的哈希表 */
  const char *pKey,   /* The key we are searching for *//*被查找的键值*/                                     /* 定义被查找的键值 */
  int nKey,           /* Bytes in key (not counting zero terminator)定义一个整型的键值 */                    /* 定义哈希表的一个整形的键值 */
  unsigned int h      /* The hash for this key. 定义一个无符号的整型变量h*/                                  /* 定义一个无符号的整形变量 */
)//定义一个静态的函数给哈希表中的匹配的元素定一个键值
{
  HashElem *elem;                /* Used to loop thru the element list ， 定义一个哈希表指针 */              /* 定义一个哈希表指针 */
  int count;                     /* Number of elements left to test，定义一个整形变量计算表中元素的个数 */   /* 定义一个整形变量计算表中元素的个数 */

  if( pH->ht )//如果表的大小不为0，则执行if语句，否则执行else语句                                            /*如果表的大小不为0，则执行if语句，否则执行else语句*/
  {
    struct _ht *pEntry = &pH->ht[h];//定义结构体变量ht同时定义一个指针并等于表的首地址                       /*定义结构体变量ht同时定义一个指针并等于表的首地址*/
    elem = pEntry->chain;//变量指针pEntry->chain赋值于elem                                                   /*变量指针pEntry->chain赋值于elem*/
    count = pEntry->count;//变量指针pEntry->count赋值于count                                                 /*变量指针pEntry->count赋值于count*/
  }
  else{
    elem = pH->first;//表头指针赋值给变量elem                                                                /* 表头指针赋值给变量elem*/
    count = pH->count;//指针ph->count赋值给变量count                                                         /*指针ph->count赋值给变量count*/
  }
  while( count-- && ALWAYS(elem) )//表中元素不为空时循环到count值为0                                         /*表中元素不为空时循环到count值为0*/
  {
    if( elem->nKey==nKey && sqlite3StrNICmp(elem->pKey,pKey,nKey)==0 )//如果表中的值等于给定的键值同         /*如果表中的值等于给定的键值同时分配的空间为0，直接返回elem,否则指针继续指向下一个元素*/
时分配的空间为0，直接返回elem,否则指针继续指向下一个元素*/
    { 
      return elem;                                                                                          /*如果表中的值等于给定的键值，则直接返回elem*/
    }
    elem = elem->next;                                                                                      /*继续访问elem的值*/
  }
  return 0;//程序结束函数返回值为0                                                                          /*程序结束函数返回值为0*/
}

/* Remove a single entry from the hash table given a pointer to that
** element and a hash on the element's key.
*/

/*按照给定的指针和哈希表的元素值删除一个条目*/

/**************************************************************
* *按照给定的指针和哈希表的元素值删除一个条目
* *PH表有要被删除的元素
* *elem被删除的元素
* *h元素的Hash值
* *静态定义一个返回值为空的函数，用于删除一条目
***************************************************************/

static void removeElementGivenHash(                                                                 /* 定义一个返回值为空的函数*/
  Hash *pH,         /* The pH containing "elem" */                                                  /* PH表有要被删除的元素*/
  HashElem* elem,   /* The element to be removed from the pH */                                     /* 定义elem被删除元素*/
  unsigned int h    /* Hash value for the element */                                                /* 定义无符号整形用于标记一个条目*/
)//静态定义一个返回值为空的函数，用于删除一条目
{
  struct _ht *pEntry;//定义一个结构体变量ht及一个指针变量                                           /* 定义一个结构体变量ht及一个指针变量*/
  if( elem->prev )//如果表中元素的前驱不为空，则执行下if语句，否则执行else的语句                    /*如果表中元素的前驱不为空，则执行下if语句，否则执行else的语句*/
  {
    elem->prev->next = elem->next; //把元素的后继赋值于元素的前驱的后继                             /* 把元素的后继赋值于元素的前驱的后继*/
  }else{
    pH->first = elem->next;//把元素的后继赋值于表的头指针                                           /*把元素的后继赋值于表的头指针*/
  }
  if( elem->next )//如果元素的后继不为空，执行if语句                                                /*如果元素的后继不为空，执行if语句*/
  {
    elem->next->prev = elem->prev;//把元素的前驱赋值于元素的后继的前驱                              /*把元素的前驱赋值于元素的后继的前驱*/
  }
  if( pH->ht )//如果指针pH->ht 不为空，执行if语句                                                   /*如果指针pH->ht 不为空，执行if语句*/
  {
    pEntry = &pH->ht[h];//把表的首地址赋值于变量pEntry                                              /*把表的首地址赋值于变量pEntry*/
    if( pEntry->chain==elem )//如果pEntry->chain==elem，执行if语句                                  /*如果pEntry->chain==elem，执行if语句*/
    {
      pEntry->chain = elem->next;//把元素的后继赋值于变量 pEntry->chain                             /*把元素的后继赋值于变量 pEntry->chain*/
    }
    pEntry->count--;//自减                                                                          /*逐个访问元素*/
    assert( pEntry->count>=0 );//执行assert函数，若pEntry->count>=0为真，则继续执行，否则终止程序   /*执行assert函数，若pEntry->count>=0为真，则继续执行，否则终止程序*/
  }
  sqlite3_free( elem );//申请一个空的空间                                                           /*释放elem元素*/
  pH->count--;//自减                                                                                /*逐个访问内存中的元素*/
  if( pH->count<=0 )//如果pH->count<=0 ，执行if语句                                                 /*如果pH->count<=0 ，执行if语句*/
  {
    assert( pH->first==0 );/*pH->first为真，继续执行，否则推出执行*/                                /*pH->first为真，继续执行，否则推出执行*/ 
    assert( pH->count==0 );/*pH->count为真，继续执行，否则推出执行*/                                /*pH->count为真，继续执行，否则推出执行*/
    sqlite3HashClear(pH);/*清空PH哈希表中的值*/                                                     /*清空PH哈希表中的值*/
  }
}

/* Attempt to locate an element of the hash table pH with a key
** that matches pKey,nKey.  Return the data for this element if it is
** found, or NULL if there is no match.
*/

/*
  在哈希表PH中找到与pkey，nkey匹配的key。
  如果找到返回元素的数据，如果没有找到返回NULL
*/

/**************************************************************
* *在哈希表PH中找到与pkey，nkey匹配的key。
* *如果找到返回元素的数据，如果没有找到返回NULL。
* **elem匹配的元素
* *h一个hash散列值
***************************************************************/

void *sqlite3HashFind(const Hash *pH, const char *pKey, int nKey){     /*定义一个哈希表查找函数*/
  HashElem *elem;    /* The element that matches key *//*匹配的元素*/  /*匹配需要查找的元素*/
  unsigned int h;    /* A hash on key */                               /*定义无符号整形*/

  assert( pH!=0 );/*pH!=0为真，继续执行，否则推出执行*/                /*pH!=0为真，继续执行，否则推出执行*/
  assert( pKey!=0 );/*pKey!=0为真，继续执行，否则推出执行*/            /*pKey!=0为真，继续执行，否则推出执行*/
  assert( nKey>=0 );/*nKey!=0为真，继续执行，否则推出执行*/            /*nKey!=0为真，继续执行，否则推出执行*/
  if( pH->ht ){                                                        /*若hash表不空*/
    h = strHash(pKey, nKey) % pH->htsize;                              /*使用 strHash 函数计算桶号h*/
  }else{
    h = 0;                                                             /*若为空，散列值就为0*/
  }
  elem = findElementGivenHash(pH, pKey, nKey, h);                      /*在表中查找给定的元素*/
  return elem ? elem->data : 0;                                        /*找到则返回数据，没找到就返回0*/
}

/* Insert an element into the hash table pH.  The key is pKey,nKey
** and the data is "data".
**
** If no element exists with a matching key, then a new
** element is created and NULL is returned.
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
插入元素到哈希表PH中，pkey是键值，nkey和data是数据。
如果哈希表中不存在相同键值的入口项，则插入，然后返回 NULL。
如果存在相同的入口项，然后替换原来的数据并且返回原数据。
键值不在这个实例中。如果申请空间失败，则返回新数据并且哈希表不变。
如果data的参数对于这个函数来说是空的，那么和key对应的元素就要从哈希表中移除

*/

/**************************************************************
* *插入元素到哈希表PH中，pkey是键值，nkey和data是数据。
* *如果哈希表中不存在相同键值的入口项，则插入，然后返回 NULL。
* *如果存在相同的入口项，然后替换原来的数据并且返回原数据。
* *键值不在这个实例中。如果申请空间失败，
* *则返回新数据并且哈希表不变。
* *如果data的参数对于这个函数来说是空的，
* *那么和key对应的元素就要从哈希表中移除。
***************************************************************/

void *sqlite3HashInsert(Hash *pH, const char *pKey, int nKey, void *data){  /*定义插入元素到哈希表PH中的函数*/
  unsigned int h;       /* the hash of the key modulo hash table size */    /* hash表的散列模型的大小*/
  HashElem *elem;       /* Used to loop thru the element list */            /* 用来通过循环列表的元素*/	    
  HashElem *new_elem;   /* New element added to the pH */                   /*添加到hash表的新元素 */

  assert( pH!=0 );/*pH!=0为真，继续执行，否则推出执行*/                     /*若表存在，继续执行*/
  assert( pKey!=0 );/*pKey!=0为真，继续执行，否则推出执行*/                 /*若表存在且有键值，继续执行*/
  assert( nKey>=0 );/*nKey!=0为真，继续执行，否则推出执行*/                 /*若表存在，有键值且有数据，继续执行*/
  if( pH->htsize ){                                                         /*如果pH的htsize成员变量不为0,即使用哈希表存放数据项*/
    h = strHash(pKey, nKey) % pH->htsize;/*如果pH的htsize成员变量不为0,     /*使用 strHash 函数计算桶号h*/
即使用哈希表存放数据项那么使用 strHash 函数计算桶号h*/
  }else{
    h = 0;/*如果pH的htsize成员变量为0,那么h=0*/                             /*如果pH的htsize成员变量为0,那么h=0*/
  }
  elem = findElementGivenHash(pH,pKey,nKey,h);                              /*查找给定的hash元素*/
  if( elem ){                                                               /*若存在，就更新数据*/
    void *old_data = elem->data;                                            /*给指定的一个空类型的指针赋值*/
    if( data==0 ){                                                          /*如果数据为0，按照给定的指针和哈希表的元素值删除*/
      removeElementGivenHash(pH,elem,h);                                    /*调用移除哈希表中元素的函数*/
    }else{                                                                  /*否则就将数据、键值写入表，并验证数据*/
      elem->data = data;                                                    /*逐个访问elem中的数据元素*/
      elem->pKey = pKey;                                                    /*逐个访问pKey中的数据元素*/
      assert(nKey==elem->nKey);                                             /*判断键值是否相等*/
    }
    return old_data;                                                        /*更新完数据后将老数据返回*/
  }
  if( data==0 ) return 0;                                                   /*如果数据为0，就返回0*/
  new_elem = (HashElem*)sqlite3Malloc( sizeof(HashElem) );                  /*给新元素分配空间，大小为sizeof(HashElem)*/
  if( new_elem==0 ) return data;                                            /*若新元素空间为0，则返回老数据，并将键值、数据写到新空间*/                                        
  new_elem->pKey = pKey;                                                    /*把pKey的值赋给新节点*/
  new_elem->nKey = nKey;                                                    /*把nKey的值赋给新节点*/
  new_elem->data = data;                                                    /*把data的值赋给新节点*/
  pH->count++;                                                              /*hash表记录自加*/
  if( pH->count>=10 && pH->count > 2*pH->htsize ){                          /*若hsah表记录数大于等于10条且大于表空间的2倍*/
    if( rehash(pH, pH->count*2) ){                                          /*把hash表大小调整为记录数的2倍并检测大小是否大于0*/
      assert( pH->htsize>0 );                                               /*调用函数判断取值*/
      h = strHash(pKey, nKey) % pH->htsize;                                 /*大于0，则计算散列值*/
    }
  }
  if( pH->ht ){                                                             /*若hash表存在结构指针，则按查找的散列值在表中插入新元素*/
    insertElement(pH, &pH->ht[h], new_elem);                                /*调用插入函数进行插入操作*/
  }else{
    insertElement(pH, 0, new_elem);                                         /*再次判断插入函数要插入什么元素*/
  }
  return 0;                                                                 /*函数结束返回0*/
}
