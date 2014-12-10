/*
** 2008 February 16
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
** 作者本人放弃此代码的版权，在任何有法律的地方，这里给使用SQLite的人以下的祝福：
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**    愿你行善莫行恶。
      愿你原谅自己宽恕他人。
      愿你宽心与人分享，不要索取多于给予。

*************************************************************************
** This file implements an object that represents a fixed-length
** bitmap.  Bits are numbered starting with 1.
** 这个文件实现了一个对象代表一个固定长度的位图。比特数从1开始。

** A bitmap is used to record which pages of a database file have been
** journalled during a transaction, or which pages have the "dont-write"
** property.  Usually only a few pages are meet either condition.
** So the bitmap is usually sparse and has low cardinality.
** But sometimes (for example when during a DROP of a large table) most
** or all of the pages in a database can get journalled.  In those cases, 
** the bitmap becomes dense with high cardinality.  The algorithm needs 
** to handle both cases well.
** 一个位图用来记录在一个事务处理过程中数据库的哪几个页被日志记录，或者哪几个页有
   "dont-write"的性质。通常只有很少的页满足条件。因此位图通常很稀少，并且基数小。
   但是有时(比如当丢弃一个大的表时)数据库中绝大多数或所有的页会被日志记录。在这种
   情况下，位图是稠密的，并且基数大。算法应该能够很好的处理这两种情况。
  
** The size of the bitmap is fixed when the object is created.
** 当对象被创建时，位图的大小就被固定了。

** All bits are clear when the bitmap is created.  Individual bits
** may be set or cleared one at a time.
**当位图被创建时所有的位都被清空。单独的位可能会被设置或清除一次。

** Test operations are about 100 times more common that set operations.
** Clear operations are exceedingly rare.  There are usually between
** 5 and 500 set operations per Bitvec object, though the number of sets can
** sometimes grow into tens of thousands or larger.  The size of the
** Bitvec object is the number of pages in the database file at the
** start of a transaction, and is thus usually less than a few thousand,
** but can be as large as 2 billion for a really big database.
   测试操作比集合操作要通用100倍，也就是说测试操作更加常用。清除操作是极其少见
   的。尽管集合的数量有时会增长到成千上万或者更多，但是每个Bitvec对象通常有5到
   500个集合操作。Bitvec对象的大小是在事务开始时数据库文件中的页的数量，因此通
   常不到几千，但是可以大到像20亿那么大的数据库。
*/

#include "sqliteInt.h"      /*sqliteInt头文件（它定义了提供给应用使用的API和数据结构）*/

/* Size of the Bitvec structure in bytes. */
   /*Bitvec结构的字节大小 */
#define BITVEC_SZ        512               /*定义BITVEC_SZ大小为512个字节*/

/* Round the union size down to the nearest pointer boundary, since that's how 
** it will be aligned within the Bitvec struct. 
   使Bitvec结构总的大小，降到最近的指针的边界，来看看这样它是如何排列在bitvec结构之中的。
*/
#define BITVEC_USIZE     (((BITVEC_SZ-(3*sizeof(u32)))/sizeof(Bitvec*))*sizeof(Bitvec*))

/* Type of the array "element" for the bitmap representation. 
** Should be a power of 2, and ideally, evenly divide into BITVEC_USIZE. 
** Setting this to the "natural word" size of your CPU may improve
** performance. 
   数组“元素”，为位图表示的类型。
   应该是2的幂，并且理想地，均匀地分成BITVEC_USIZE。
   将其设置为你的CPU的“自然词”大小可以提高性能。
*/
#define BITVEC_TELEM     u8

/* Size, in bits, of the bitmap element. */
/*以位为单位的位图元素的大小。 */
#define BITVEC_SZELEM    8

/* Number of elements in a bitmap array. */
/*在一个位图数组元素的编号。 */
#define BITVEC_NELEM     (BITVEC_USIZE/sizeof(BITVEC_TELEM))

/* Number of bits in the bitmap array. */
/*位图数组中的位的数目。 */
#define BITVEC_NBIT      (BITVEC_NELEM*BITVEC_SZELEM)

/* Number of u32 values in hash table. */
/*在哈希表中U32值的数量。 */
#define BITVEC_NINT      (BITVEC_USIZE/sizeof(u32))

/* Maximum number of entries in hash table before 
** sub-dividing and re-hashing. */
/*在细分和二度哈希之前在哈希表中条目的最大数量。 */
#define BITVEC_MXHASH    (BITVEC_NINT/2)

/* Hashing function for the aHash representation.
** Empirical testing showed that the *37 multiplier 
** (an arbitrary prime)in the hash function provided 
** no fewer collisions than the no-op *1. */
/*哈希函数的aHash表示.实证检验表明，×37乘法器（任意素数）
  在哈希函数中没有提供比空指令×1更少的冲突*/
#define BITVEC_HASH(X)   (((X)*1)%BITVEC_NINT)

#define BITVEC_NPTR      (BITVEC_USIZE/sizeof(Bitvec *))


/*
** A bitmap is an instance of the following structure.
** 下面结构体的一个实例是一个位图。

** This bitmap records the existance of zero or more bits
** with values between 1 and iSize, inclusive.
** 这个位图记录0或更多位的存在，用在1和iSize之间的值记录，并且包含1和iSize。

** There are three possible representations of the bitmap.
** If iSize<=BITVEC_NBIT, then Bitvec.u.aBitmap[] is a straight
** bitmap.  The least significant bit is bit 1.
** 如果iSize<=BITVEC_NBIT，则Bitvec.u.aBitmap[]是一个直接的位图。最低有效单位是1位。

** If iSize>BITVEC_NBIT and iDivisor==0 then Bitvec.u.aHash[] is
** a hash table that will hold up to BITVEC_MXHASH distinct values.
** 如果iSize>BITVEC_NBIT并且iDivisor==0，那么Bitvec.u.aHash[]是一个能够
   容纳BITVEC_MXHASH的不同值的hash表。

** Otherwise, the value i is redirected into one of BITVEC_NPTR
** sub-bitmaps pointed to by Bitvec.u.apSub[].  Each subbitmap
** handles up to iDivisor separate values of i.  apSub[0] holds
** values between 1 and iDivisor.  apSub[1] holds values between
** iDivisor+1 and 2*iDivisor.  apSub[N] holds values between
** N*iDivisor+1 and (N+1)*iDivisor.  Each subbitmap is normalized
** to hold deal with values between 1 and iDivisor.
  否则，值i会被重新传递给由Bitvec.u.apSub[]所指的BITVEC_NPTR子位图。
  每一个子位图句柄多不同于值i。apSub[0]的值介于1和iDivisor之间。
  apSub[1]的值介于iDivisor+1和2*iDivisor之间。
  apSub[N]的值介于N*iDivisor+1和(N+1)*iDivisor之间。
  每一个子位图都很规范的处理介于1到iDivisor之间的值。
*/
struct Bitvec {
  u32 iSize;      /* Maximum bit index.  Max iSize is 4,294,967,296.
                    最大位指数，最大的iSize是4,294,967,296.*/
  
  u32 nSet;       /* Number of bits that are set - only valid for aHash
                  ** element.  Max is BITVEC_NINT.  For BITVEC_SZ of 512,
                  ** this would be 125.
                    被设置的位的数量仅适用于aHash元素。最大是BITVEC_NINT。
                    因为BITVEC_SZ 的大小是512，所以这里是125.*/
                    
  u32 iDivisor;   /* Number of bits handled by each apSub[] entry.
                     位的数量由每个apSub[]的记录处理。*/
                  /* Should >=0 for apSub element.
                     apSub元素应该>=0*/
                  /* Max iDivisor is max(u32) / BITVEC_NPTR + 1.  
                  最大的iDivisor是max(u32) / BITVEC_NPTR + 1.*/
                  /* For a BITVEC_SZ of 512, this would be 34,359,739.
                  因为BITVEC_SZ 的大小是512，所以这里是34,359,739.*/
  union {
    BITVEC_TELEM aBitmap[BITVEC_NELEM];    /* Bitmap representation  位图的表示*/
    u32 aHash[BITVEC_NINT];      /* Hash table representation   哈希表的表示 */
    Bitvec *apSub[BITVEC_NPTR];  /* Recursive representation   递归的表示*/
  } u;      /*联合变量*/
};

/*
** Create a new bitmap object able to handle bits between 0 and iSize,
** inclusive.  Return a pointer to the new object.  Return NULL if 
** malloc fails.
   创建一个能过处理0到iSize个位(包含0和iSize)的新位图对象。返回新对象的指针。
  如果分配失败返回空。
*/
Bitvec *sqlite3BitvecCreate(u32 iSize){
  Bitvec *p;
  assert( sizeof(*p)==BITVEC_SZ );
  p = sqlite3MallocZero( sizeof(*p) );
  if( p ){
    p->iSize = iSize;
  }
  return p;
}

/*
** Check to see if the i-th bit is set.  Return true or false.
** If p is NULL (if the bitmap has not been created) or if
** i is out of range, then return false.
   检查第i个位是否被分配。返回真或假。
   如果p为空(如果位图没有被创建)或i溢出返回假。
*/
int sqlite3BitvecTest(Bitvec *p, u32 i){
  if( p==0 ) return 0;
  if( i>p->iSize || i==0 ) return 0;
  i--;
  while( p->iDivisor ){
    u32 bin = i/p->iDivisor;
    i = i%p->iDivisor;
    p = p->u.apSub[bin];
    if (!p) {
      return 0;
    }
  }
  if( p->iSize<=BITVEC_NBIT ){
    return (p->u.aBitmap[i/BITVEC_SZELEM] & (1<<(i&(BITVEC_SZELEM-1))))!=0;
  } else{
    u32 h = BITVEC_HASH(i++);
    while( p->u.aHash[h] ){
      if( p->u.aHash[h]==i ) return 1;
      h = (h+1) % BITVEC_NINT;
    }
    return 0;
  }
}

/*
** Set the i-th bit.  Return 0 on success and an error code if
** anything goes wrong.
** 分配第i个位。成功时返回0，如果出错则返回错误信息。

** This routine might cause sub-bitmaps to be allocated.  Failing
** to get the memory needed to hold the sub-bitmap is the only
** that can go wrong with an insert, assuming p and i are valid.
** 这个程序可能会分配子位图。假设p和i是有效的，那么对于插入来说需要
   存放子位图却没有获得内存是唯一可能出错的地方。

** The calling function must ensure that p is a valid Bitvec object
** and that the value for "i" is within range of the Bitvec object.
** Otherwise the behavior is undefined.
   调用函数必须确保p是有效的Bitvec对象，并且i的值没有溢出Bitvec对象的范围。
   否则行为是无效的。
*/
int sqlite3BitvecSet(Bitvec *p, u32 i){
  u32 h;
  if( p==0 ) return SQLITE_OK;
  assert( i>0 );
  assert( i<=p->iSize );
  i--;
  while((p->iSize > BITVEC_NBIT) && p->iDivisor) {
    u32 bin = i/p->iDivisor;
    i = i%p->iDivisor;
    if( p->u.apSub[bin]==0 ){
      p->u.apSub[bin] = sqlite3BitvecCreate( p->iDivisor );
      if( p->u.apSub[bin]==0 ) return SQLITE_NOMEM;
    }
    p = p->u.apSub[bin];
  }
  if( p->iSize<=BITVEC_NBIT ){
    p->u.aBitmap[i/BITVEC_SZELEM] |= 1 << (i&(BITVEC_SZELEM-1));
    return SQLITE_OK;
  }
  h = BITVEC_HASH(i++);
  
  /* if there wasn't a hash collision, and this doesn't */
  /* completely fill the hash, then just add it without */
  /* worring about sub-dividing and re-hashing. 
     如果一个哈希冲突都没有，并且也没有完全填充哈希，这样只要增加
     它就可以了，而不用担心子划分和二次哈希了。*/
  if( !p->u.aHash[h] ){
    if (p->nSet<(BITVEC_NINT-1)) {
      goto bitvec_set_end;
    } else {
      goto bitvec_set_rehash;
    }
  }
  
  /* there was a collision, check to see if it's already */
  /* in hash, if not, try to find a spot for it。
     有一个冲突，检查看看是不是已经在哈希，如果没有，试着找到它的位置。 */
  do {
    if( p->u.aHash[h]==i ) return SQLITE_OK;
    h++;
    if( h>=BITVEC_NINT ) h = 0;
  } while( p->u.aHash[h] );
  
  /* we didn't find it in the hash.  h points to the first */
  /* available free spot. check to see if this is going to */
  /* make our hash too "full". 
     我们没有在哈希找到它。h点到第一个可用的自由点。检查看看是否这会
     让我们的哈希太“满”。*/
bitvec_set_rehash:
  if( p->nSet>=BITVEC_MXHASH ){
    unsigned int j;
    int rc;
    u32 *aiValues = sqlite3StackAllocRaw(0, sizeof(p->u.aHash));
    if( aiValues==0 ){
      return SQLITE_NOMEM;
    }else{
      memcpy(aiValues, p->u.aHash, sizeof(p->u.aHash));
      memset(p->u.apSub, 0, sizeof(p->u.apSub));
      p->iDivisor = (p->iSize + BITVEC_NPTR - 1)/BITVEC_NPTR;
      rc = sqlite3BitvecSet(p, i);
      for(j=0; j<BITVEC_NINT; j++){
        if( aiValues[j] ) rc |= sqlite3BitvecSet(p, aiValues[j]);
      }
      sqlite3StackFree(0, aiValues);
      return rc;
    }
  }
bitvec_set_end:
  p->nSet++;
  p->u.aHash[h] = i;
  return SQLITE_OK;
}

/*
** Clear the i-th bit.
** 清除第i位。
** pBuf must be a pointer to at least BITVEC_SZ bytes of temporary storage
** that BitvecClear can use to rebuilt its hash table.
   pBuf必须是一个至少为BITVEC_SZ个字节的指针指向临时存储器，通过临时存储器
   BitvecClear可以重建hash表。
*/
void sqlite3BitvecClear(Bitvec *p, u32 i, void *pBuf){
  if( p==0 ) return;
  assert( i>0 );
  i--;
  while( p->iDivisor ){
    u32 bin = i/p->iDivisor;
    i = i%p->iDivisor;
    p = p->u.apSub[bin];
    if (!p) {
      return;
    }
  }
  if( p->iSize<=BITVEC_NBIT ){
    p->u.aBitmap[i/BITVEC_SZELEM] &= ~(1 << (i&(BITVEC_SZELEM-1)));
  }else{
    unsigned int j;
    u32 *aiValues = pBuf;
    memcpy(aiValues, p->u.aHash, sizeof(p->u.aHash));
    memset(p->u.aHash, 0, sizeof(p->u.aHash));
    p->nSet = 0;
    for(j=0; j<BITVEC_NINT; j++){
      if( aiValues[j] && aiValues[j]!=(i+1) ){
        u32 h = BITVEC_HASH(aiValues[j]-1);
        p->nSet++;
        while( p->u.aHash[h] ){
          h++;
          if( h>=BITVEC_NINT ) h = 0;
        }
        p->u.aHash[h] = aiValues[j];
      }
    }
  }
}

/*
** Destroy a bitmap object.  Reclaim all memory used.
   销毁一个位图对象。回收所有可用内存。
*/
void sqlite3BitvecDestroy(Bitvec *p){
  if( p==0 ) return;
  if( p->iDivisor ){
    unsigned int i;
    for(i=0; i<BITVEC_NPTR; i++){
      sqlite3BitvecDestroy(p->u.apSub[i]);
    }
  }
  sqlite3_free(p);
}

/*
** Return the value of the iSize parameter specified when Bitvec *p
** was created.
   当Bitvec *p被创建时返回参数iSize指定的值。
*/
u32 sqlite3BitvecSize(Bitvec *p){
  return p->iSize;
}

#ifndef SQLITE_OMIT_BUILTIN_TEST
/*
** Let V[] be an array of unsigned characters sufficient to hold
** up to N bits.  Let I be an integer between 0 and N.  0<=I<N.
** Then the following macros can be used to set, clear, or test
** individual bits within V.
   令V[]是一个无符号字符数组并有足够的空间容纳N位。
   令I是一个介于0和N之间的整数，0<=I<N。
   然后剩下的模块就可以在V中用来分配，清除或测试单独的位。
*/
#define SETBIT(V,I)      V[I>>3] |= (1<<(I&7))
#define CLEARBIT(V,I)    V[I>>3] &= ~(1<<(I&7))
#define TESTBIT(V,I)     (V[I>>3]&(1<<(I&7)))!=0

/*
** This routine runs an extensive test of the Bitvec code.
** 这个函数可以广泛的测试Bitvec代码。

** The input is an array of integers that acts as a program
** to test the Bitvec.  The integers are opcodes followed
** by 0, 1, or 3 operands, depending on the opcode.  Another
** opcode follows immediately after the last operand.
** 输入是一个可以作为程序检测Bitvec的整型数组。
   整数是跟有0,1或3个操作数的操作码，这取决于操作码。
   另一个操作码紧跟在最后一个操作数后面。
   
** There are 6 opcodes numbered from 0 through 5.  0 is the
** "halt" opcode and causes the test to end.
**  有从0到5一共6个操作码。0是”停止”操作码并表示测试结束。

**    0          Halt and return the number of errors
**    1 N S X    Set N bits beginning with S and incrementing by X
**    2 N S X    Clear N bits beginning with S and incrementing by X
**    3 N        Set N randomly chosen bits
**    4 N        Clear N randomly chosen bits
**    5 N S X    Set N bits from S increment X in array only, not in bitvec
**    0          停止并返回错误个数
      1 N S X    分配N位从S开始递增的X
      2 N S X    清除N位从S开始递增到X
      3 N        分配N个随机的位
      4 N        清除N个随机的位
      5 N S X    只在数组中分配N位从S递增到X，而不是在bitvec中
      
** The opcodes 1 through 4 perform set and clear operations are performed
** on both a Bitvec object and on a linear array of bits obtained from malloc.
** Opcode 5 works on the linear array only, not on the Bitvec.
** Opcode 5 is used to deliberately induce a fault in order to
** confirm that error detection works.
** 操作码1到4执行分配，清除操作在Bitvec对象和从malloc中得到的位线性数组上都执行。
   操作码5只在线性数组上起作用，而在Bitvec上不行。
   操作码5特意用来引导错误，为了确认错误的检测起作用。
   
** At the conclusion of the test the linear array is compared
** against the Bitvec object.  If there are any differences,
** an error is returned.  If they are the same, zero is returned.
** 检测线性数组的结论是和Bitvec对象比较。如果有不同，就返回错误。如果相同，则返回0。
** If a memory allocation error occurs, return -1.
   如果内存分配出现错误，返回-1.
*/
int sqlite3BitvecBuiltinTest(int sz, int *aOp){
  Bitvec *pBitvec = 0;
  unsigned char *pV = 0;
  int rc = -1;
  int i, nx, pc, op;
  void *pTmpSpace;

  /* Allocate the Bitvec to be tested and a linear array of
  ** bits to act as the reference.
     分配bitvec进行测试和一个以位为单位的线性数组作为参考。
  */
  pBitvec = sqlite3BitvecCreate( sz );
  pV = sqlite3MallocZero( (sz+7)/8 + 1 );
  pTmpSpace = sqlite3_malloc(BITVEC_SZ);
  if( pBitvec==0 || pV==0 || pTmpSpace==0  ) goto bitvec_end;

  /* NULL pBitvec tests 
     pBitvec空测试*/
  sqlite3BitvecSet(0, 1);
  sqlite3BitvecClear(0, 1, pTmpSpace);

  /* Run the program 
     运行程序*/
  pc = 0;
  while( (op = aOp[pc])!=0 ){
    switch( op ){
      case 1:
      case 2:
      case 5: {
        nx = 4;
        i = aOp[pc+2] - 1;
        aOp[pc+2] += aOp[pc+3];
        break;
      }
      case 3:
      case 4: 
      default: {
        nx = 2;
        sqlite3_randomness(sizeof(i), &i);
        break;
      }
    }
    if( (--aOp[pc+1]) > 0 ) nx = 0;
    pc += nx;
    i = (i & 0x7fffffff)%sz;
    if( (op & 1)!=0 ){
      SETBIT(pV, (i+1));
      if( op!=5 ){
        if( sqlite3BitvecSet(pBitvec, i+1) ) goto bitvec_end;
      }
    }else{
      CLEARBIT(pV, (i+1));
      sqlite3BitvecClear(pBitvec, i+1, pTmpSpace);
    }
  }

  /* Test to make sure the linear array exactly matches the
  ** Bitvec object.  Start with the assumption that they do
  ** match (rc==0).  Change rc to non-zero if a discrepancy
  ** is found.
     测试，以确保线性数组完全与Bitvec对象相匹配。开始时假设它们匹配（rc== 0）。
     如果发现有差异则改变rc为非零值。
  */
  rc = sqlite3BitvecTest(0,0) + sqlite3BitvecTest(pBitvec, sz+1)
          + sqlite3BitvecTest(pBitvec, 0)
          + (sqlite3BitvecSize(pBitvec) - sz);
  for(i=1; i<=sz; i++){
    if(  (TESTBIT(pV,i))!=sqlite3BitvecTest(pBitvec,i) ){
      rc = i;
      break;
    }
  }

  /* Free allocated structure 
     释放所分配的结构 */
bitvec_end:
  sqlite3_free(pTmpSpace);
  sqlite3_free(pV);
  sqlite3BitvecDestroy(pBitvec);
  return rc;
}
#endif /* SQLITE_OMIT_BUILTIN_TEST  */
        /* 省略了SQLITE的内测 */
