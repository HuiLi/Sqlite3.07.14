/**
**注释人：付惠茹
**学号：2015021597
*/


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
static int rehash(Hash *pH, unsigned int new_size)/*定义一个静态的函数，用于调整hash表的大小*/
{
  struct _ht *new_ht;            /* 定义一个新的哈希表 */
  HashElem *elem, *next_elem;    /* 定义哈希数组指针 */

#if SQLITE_MALLOC_SOFT_LIMIT>0 //如果定义的SQLITE_MALLOC_SOFT_LIMIT大于0，则执行下面的if语句，否则不执行
  if( new_size*sizeof(struct _ht)>SQLITE_MALLOC_SOFT_LIMIT )//如果新申请的大小大于之前的，执行if的语句
  {
    new_size = SQLITE_MALLOC_SOFT_LIMIT/sizeof(struct _ht);//把原来的空间大小赋值给变量new_size(新的空间大小）
  }
  if( new_size==pH->htsize ) return 0;//如果哈希表新申请的大小和之前一样，则返回值为0，表示改变大小失败
#endif//结束

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
  
  sqlite3BeginBenignMalloc();//在src/fault中有这个函数的函数体，必须与sqlite3EndBenignMalloc()配合使用
  new_ht = (struct _ht *)sqlite3Malloc( new_size*sizeof(struct _ht) );//申请新的大小
  sqlite3EndBenignMalloc();//分配实际使用空间结束

  if( new_ht==0 ) return 0;//如果新申请的大小等于0，返回0
  sqlite3_free(pH->ht);//分配一个免费的空间
  pH->ht = new_ht;//把new_ht的值赋值给pH->ht
  pH->htsize = new_size = sqlite3MallocSize(new_ht)/sizeof(struct _ht);/*htsize的大小等于申请空间大小/ht结构体所占字节大小*/
  memset(new_ht, 0, new_size*sizeof(struct _ht));/*将新开辟空间new_ht设置为0*/
  for(elem=pH->first, pH->first=0; elem; elem = next_elem)//若数组的头指针指的值为0，则循环直到指针指向数组的最后一个数
  {
    unsigned int h = strHash(elem->pKey, elem->nKey) % new_size;//定义一个无字符的整型变量h表示数组长度
    next_elem = elem->next;//把elem的指针赋值给变量next_elem
    insertElement(pH, &new_ht[h], elem);/*把elem连接到PH的哈希表的后面*/
  }
  return 1;//重新改变大小发生或者失败没有发生，返回true
}

/* This function (for internal use only) locates an element in an
** hash table that matches the given key.  The hash for this key has
** already been computed and is passed as the 4th parameter.
*/

/*这个函数（仅供内部使用）位于一个元素在一个哈希表相匹配的给定的键值。
这个键值已被计算并作为第四个参数传递。*/

static HashElem *findElementGivenHash(
  const Hash *pH,     /*被搜索的哈希表*/
  const char *pKey,   /*被查找的键值*/
  int nKey,           /*定义一个整型的键值(not counting zero terminator) */
  unsigned int h      /* 定义一个无符号的整型变量h */
)//定义一个静态的函数给哈希表中的匹配的元素定一个键值
{
  HashElem *elem;                /* 定义一个哈希表指针 */
  int count;                     /* 定义一个整形变量计算表中元素的个数 */
  if( pH->ht )//如果表的大小不为0，则执行if语句，否则执行else语句
  {
    struct _ht *pEntry = &pH->ht[h];//定义结构体变量ht同时定义一个指针并等于表的首地址
    elem = pEntry->chain;//变量指针pEntry->chain赋值于elem
    count = pEntry->count;//变量指针pEntry->count赋值于count
  }
  else{
    elem = pH->first;//表头指针赋值给变量elem
    count = pH->count;//指针ph->count赋值给变量count
  }
  while( count-- && ALWAYS(elem) )//表中元素不为空时循环到count值为0
  {
    if( elem->nKey==nKey && sqlite3StrNICmp(elem->pKey,pKey,nKey)==0 )//如果表中的值等于给定的键值同时分配的空间为0，直接返回elem,否则指针继续指向下一个元素
	{ 
      return elem;
    }
    elem = elem->next;
  }
  return 0;//程序结束函数返回值为0
}

/* Remove a single entry from the hash table given a pointer to that
** element and a hash on the element's key.
*/

/*按照给定的指针和哈希表的元素值删除一个条目*/
static void removeElementGivenHash(
  Hash *pH,         /* The pH containing "elem" */
  HashElem* elem,   /* The element to be removed from the pH */
  unsigned int h    /* Hash value for the element */
)//静态定义一个返回值为空的函数，用于删除一条目
{
  struct _ht *pEntry;//定义一个结构体变量ht及一个指针变量
  if( elem->prev )//如果表中元素的前驱不为空，则执行下if语句，否则执行else的语句
  {
    elem->prev->next = elem->next; //把元素的后继赋值于元素的前驱的后继
  }
  else
  {
    pH->first = elem->next;//把元素的后继赋值于表的头指针
  }
  if( elem->next )//如果元素的后继不为空，执行if语句
  {
    elem->next->prev = elem->prev;//把元素的前驱赋值于元素的后继的前驱
  }
  if( pH->ht )//如果指针pH->ht 不为空，执行if语句
  {
    pEntry = &pH->ht[h];//把表的首地址赋值于变量pEntry
    if( pEntry->chain==elem )//如果pEntry->chain==elem，执行if语句
	{
      pEntry->chain = elem->next;//把元素的后继赋值于变量 pEntry->chain
    }
    pEntry->count--;//自减
    assert( pEntry->count>=0 );//执行assert函数，若pEntry->count>=0为真，则继续执行，否则终止程序
  }
  sqlite3_free( elem );//申请一个免费的空间
  pH->count--;//自减
  if( pH->count<=0 )//如果pH->count<=0 ，执行if语句
  {
    assert( pH->first==0 );/*pH->first为真，继续执行，否则推出执行*/ 
    assert( pH->count==0 );/*pH->count为真，继续执行，否则推出执行*/
    sqlite3HashClear(pH);/*清空PH哈希表中的值*/
  }
}


