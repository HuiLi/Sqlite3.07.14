/*
** 2011 July 9
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file contains code for the VdbeSorter object(vdbesorter的实例), used in concert with(和..相呼应)
** a VdbeCursor to sort large numbers of keys （对大量的key值排序）(as may be required, for
** example, by CREATE INDEX statements on tables too large to fit in main
** memory  例如在为一个太大以致于不能完全放到内存中的表建立索引时，就需要key).
*/

#include "sqliteInt.h"//定义了SQLite的内部接口和数据结构
#include "vdbeInt.h"//vdbeInt.h 定义了虚拟机私有的数据结构

#ifndef SQLITE_OMIT_MERGE_SORT//宏定义

typedef struct VdbeSorterIter VdbeSorterIter;//an iterator for a PMA
typedef struct SorterRecord SorterRecord;//sorter记录
typedef struct FileWriter FileWriter;//用来往文件中写的结构体

/*
** NOTES ON DATA STRUCTURE USED FOR N-WAY MERGES:——N路归并算法及数据结构说明
**
** As keys are added to the sorter, they are written to disk in a series
** of sorted packed-memory-arrays (PMAs). 
   当key值加到sorter后，sorter就会被写到磁盘上的一系列排好序的PMA中
** The size of each PMA is roughly the same as the cache-size allowed for temporary databases. 
   每个PMA的大小大约和暂时数据库的Cache大小差不多
** In order to allow the caller to extract keys from the sorter in sorted order,
** all PMAs currently stored on disk must be merged together. 
   This comment describes the data structure used to do so. 
   为了能使调用者按照排好的顺序从sorter中提取key值，所有磁盘上的PMA需要合并到一起。
   这段说明描述了做这件事所需要的数据结构
** The structure supports merging any number of arrays in a single pass with no redundant comparison operations.
** 所描述的数据结构支持通过一趟算法就把任意数量的数组合并起来，并且没有冗余比较
** 
** 
** The aIter[] array contains an iterator for each of the PMAs being merged.
   数组aIter[]包含为需要合并的所有PMA准备的迭代器（应该是有多少需要合并的ＰＭＡ，就有多少迭代器，数组aIter[]中的元素就是一个个的迭代器）。
** An aIter[] iterator either points to a valid key or else is at EOF. 
   一个aIter[]中的迭代器，要么指向一个有效的key值，要么就位于EOF
** For the purposes of the paragraphs below, we assume that the array is actually 
** N elements in size, where N is the smallest power of 2 greater to or equal 
** to the number of iterators being merged. The extra aIter[] elements are 
** treated as if they are empty (always at EOF).
   为方便下面的举例说明，我们假设数组aIter[]有N个元素，N是2的幂，N>=需要被合并的迭代器。多余的aIter[]元素被认为是空的，假设它们位于ＥＯＦ
**
** The aTree[] array is also N elements in size. The value of N is stored in　the VdbeSorter.nTree variable.
** 数组aTree[]也是有Ｎ个元素，Ｎ的值存储在变量VdbeSorter.nTree中。
**
** The final (N/2) elements of aTree[] contain the results of comparing
** pairs of iterator keys together. 
　　数组aTree[]的最后(N/2)个元素包含对迭代器Ｋｅｙ值的比较结果
　　 
** Element i contains the result of comparing aIter[2*i-N] and aIter[2*i-N+1]. Whichever key is smaller, the
** aTree element is set to the index of it. 
**　数组aTree[]的第ｉ个元素保存的是aIter[2*i-N]和 aIter[2*i-N+1]大小比较结果（这句话说明了数组aTree[]和数组aIter[]中元素的对应关系），
　　aIter[2*i-N]和 aIter[2*i-N+1]二者中，谁更小，就把谁的下标存在数组aTree[]的第ｉ个元素里。
** For the purposes of this comparison, EOF is considered greater than any
** other key value. If the keys are equal (only possible with two EOF
** values), it doesn't matter which index is stored.
**　因为上面的比较方法，我们认为ＥＯＦ大于任何ＫＥＹ值。如果两个相比较的ｋｅｙ值一样大，则存二者中任意一个所对应的迭代器的下标。
** The (N/4) elements of aTree[] that preceed the final (N/2) described 
** above contains the index of the smallest of each block of 4 iterators.
** And so on. So that aTree[1] contains the index of the iterator that 
** currently points to the smallest key value. aTree[0] is unused.
**　数组aTree[]中有(N/4)个元素存的是每四个迭代器所组成的一组中ｋｅｙ值最小的那个迭代器的下标。
	所以aTree[1]中存有指向最小的ｋｅｙ值的迭代器的下标。aTree[0]没有被使用。
** Example:举例：
**
**     aIter[0] -> Banana
**     aIter[1] -> Feijoa
**     aIter[2] -> Elderberry
**     aIter[3] -> Currant
**     aIter[4] -> Grapefruit
**     aIter[5] -> Apple
**     aIter[6] -> Durian
**     aIter[7] -> EOF
**
**     aTree[] = { X, 5   0, 5    0, 3, 5, 6 }
**
** The current element is "Apple" (the value of the key indicated by iterator 5).
** 当前的元素（最小的ｋｅｙ值）是Ａｐｐｌｅ，它是迭代器５所指的ｋｅｙ的值。 
	
	When the Next() operation is invoked, iterator 5 will
** be advanced to the next key in its segment. Say the next key is
** "Eggplant":　aIter[5] -> Eggplant
**当函数Next()被调用时，迭代器５将会前进到它的段中的下一个ｋｅｙ值，假设下一个ｋｅｙ是"Eggplant"，则有：aIter[5] -> Eggplant
**     
**　
** The contents of aTree[] are updated first by comparing the new iterator
** 5 key to the current key of iterator 4 (still "Grapefruit"). The iterator
** 5 value is still smaller, so aTree[6] is set to 5. And so on up the tree.
** The value of iterator 6 - "Durian" - is now smaller than that of iterator
** 5, so aTree[3] is set to 6. Key 0 is smaller than key 6 (Banana<Durian),
** so the value written into element 1 of the array is 0. As follows:
**
**     aTree[] = { X, 0   0, 6    0, 3, 5, 6 }
**　数组aTree[]的内容按如下方式更新：首先，比较迭代器５所指向的ｋｅｙ的值和迭代器４的当前所指向的ｋｅｙ值（其ｋｅｙ值仍然是"Grapefruit"）
　　这时，迭代器５所指向的ｋｅｙ的值仍然是更小的，所以aTree[6]的值被设置为５.
　　迭代器６所指向的ｋｅｙ的值——"Durian"——比迭代器５所指向的ｋｅｙ值（Eggplant）小，则aTree[3]被设置为6.
　　而迭代器０所指的ｋｅｙ的值（Banana）又比迭代器６所指的ｋｅｙ的值小，即Banana<Durian，所以aTree[１]被设置成０.结果如下：
		aTree[] = { X, 0   0, 6    0, 3, 5, 6 }
** In other words, each time we advance to the next sorter element, log2(N)
** key comparison operations are required, where N is the number of segments
** being merged (rounded up to the next power of 2)nnn.
　　也就是说，我们每次前进到ｓｏｒｔｅｒ的下一个元素，需要做log2(N)次的ｋｅｙ值比较，这里Ｎ是要被合并的段的数量
　　
*/
struct VdbeSorter {
  i64 iWriteOff;                  /* Current write offset within file pTemp1 ——文件ptemp1中，当前的写偏移量*/
  i64 iReadOff;                   /* Current read offset within file pTemp1 ——文件ptemp1中，当前的读偏移量*/
  int nInMemory;                  /* Current size of pRecord list as PMA ——pRecord list的当前大小*/
  int nTree;                      /* Used size of aTree/aIter (power of 2) ——aTree/aIter的已用大小（2的幂）*/
  int nPMA;                       /* Number of PMAs stored in pTemp1 ——文件pTemp1中PMA的数量*/
  int mnPmaSize;                  /* Minimum PMA size, in bytes */
  int mxPmaSize;                  /* Maximum PMA size, in bytes.  0==no limit */
  VdbeSorterIter *aIter;          /* Array of iterators to merge ——存储要合并到一起的iterator的数组*/
  int *aTree;                     /* Current state of incremental merge ——增量合并的当前状态*/
  sqlite3_file *pTemp1;           /* PMA file 1 ——的指针*/
  SorterRecord *pRecord;          /* Head of in-memory record list ——内存中record list的头*/
  UnpackedRecord *pUnpacked;      /* Used to unpack keys ——用来解包keys*/
};

/*
** The following type is an iterator for a PMA. It caches the current key in 
** variables nKey/aKey. If the iterator is at EOF, pFile==0.
*/
struct VdbeSorterIter {           /*这个结构体定义了PMA的iterator，它把当前的key储存在变量nKey/aKey中，若iterator在EOF处，pFile==0*/
  i64 iReadOff;                   /* Current read offset ——当前读偏移量*/
  i64 iEof;                       /* 1 byte past EOF for this iterator 此变量的值比１个字节大１*/
  int nAlloc;                     /* Bytes of space at aAlloc ——aAlloc处的字节数*/
  int nKey;                       /* Number of bytes in key ——变量Ｋｅｙ中的字节数*/
  sqlite3_file *pFile;            /* File iterator is reading from ——此指针所指的地方是ｉｔｅｒａｔｏｒ开始的地方*/
  u8 *aAlloc;                     /* Allocated space ——已经分配出去的空间*/
  u8 *aKey;                       /* Pointer to current key ——指向当前ｋｅｙ的指针*/
  u8 *aBuffer;                    /* Current read buffer ——当前的读缓存*/
  int nBuffer;                    /* Size of read buffer in bytes 读缓存的字节数*/
};

/*
** An instance of this structure is used to organize the stream of records
** being written to files by the merge-sort code into aligned, page-sized
** blocks.  Doing all I/O in aligned page-sized blocks helps I/O to go
** faster on many operating systems.
   下面这个结构体的实例用来组织记录流，这些记录流将按照mergecod的算法写入到文件中的对齐的、页面大小的块中。
*/
struct FileWriter {
  int eFWErr;                     /* Non-zero if in an error state 当处于错误状态时是个非零值*/
  u8 *aBuffer;                    /* Pointer to write buffer 指向写缓存的指针*/
  int nBuffer;                    /* Size of write buffer in bytes 写缓存的字节数*/
  int iBufStart;                  /* First byte of buffer to write 缓存中要写入的第一个字节*/
  int iBufEnd;                    /* Last byte of buffer to write 缓存中要写入的最后一个缓存*/
  i64 iWriteOff;                  /* Offset of start of buffer in file 缓存开始出在文件中的偏移量*/
  sqlite3_file *pFile;            /* File to write to 指向将被写入的文件的指针*/
};

/*
** A structure to store a single record. All in-memory records are connected
** together into a linked list headed at VdbeSorter.pRecord using the 
** SorterRecord.pNext pointer.
   下面这个结构体用来存储一个单独的记录。所有内存中的记录被连接成一个链表，链表的头由指针SorterRecord *pNext指向SorterRecord *pRecord
*/
struct SorterRecord {
  void *pVal;
  int nVal;
  SorterRecord *pNext;
};

/* Minimum allowable value for the VdbeSorter.nWorking variable 变量VdbeSorter.nWorking所允许的最小值*/
#define SORTER_MIN_WORKING 10

/* Maximum number of segments to merge in a single pass. 一趟算法里所允许归并的最大段数*/
#define SORTER_MAX_MERGE_COUNT 16

/*
** Free all memory belonging to the VdbeSorterIter object passed as the second
** argument. All structure fields are set to zero before returning.
   释放由第二个参数VdbeSorterIter *pIter确定的所有属于VdbeSorterIter对象的内存空间
*/
static void vdbeSorterIterZero(sqlite3 *db, VdbeSorterIter *pIter){
  sqlite3DbFree(db, pIter->aAlloc);
  sqlite3DbFree(db, pIter->aBuffer);
  memset(pIter, 0, sizeof(VdbeSorterIter));
}

/*
** Read nByte bytes of data from the stream of data iterated by object p.
** If successful, set *ppOut to point to a buffer containing the data
** and return SQLITE_OK. Otherwise, if an error occurs, return an SQLite
** error code.
** 从由对象p迭代的数据流中读出nByte字节的数据
   如果读取成功的话，令指针ppout指向包含读出数据的缓存并返回SQLITE_OK。否则，如果出现错误，返回一个SQLite error代号
** The buffer indicated by *ppOut may only be considered valid until the
** next call to this function.
   由ppOut指向的缓存只在下次调用该函数之前是有效地
*/
//下面是函数vdbeSorterIterRead()
static int vdbeSorterIterRead(
  sqlite3 *db,                    /* Database handle (for malloc)针对malloc的数据库句柄 */
  VdbeSorterIter *p,              /* Iterator 迭代器的指针*/
  int nByte,                      /* Bytes of data to read 要读的数据的字节数*/
  u8 **ppOut                      /* OUT: Pointer to buffer containing data 指向包含数据的缓存的指针 */
)
{
  int iBuf;                       /* Offset within buffer to read from 缓存内部，读开始处的偏移量*/
  int nAvail;                     /* Bytes of data available in buffer 缓存中可用的数据的字节数*/
  assert( p->aBuffer );

  /* If there is no more data to be read from the buffer, read the next 
  ** p->nBuffer bytes of data from the file into it. Or, if there are less
  ** than p->nBuffer bytes remaining in the PMA, read all remaining data.  
     如果缓存中没有数据可读了，就从文件中读出接下来的大小等于nBuffer字节的数据存入缓存中，
	 如果PMA中的字节数小于nBuffer，就把剩下的所有数据读出来。
  */
  iBuf = p->iReadOff % p->nBuffer;//当前读偏移量 模上 读缓存的字节数
  if( iBuf==0 ){
    int nRead;                    /* Bytes to read from disk ——要从硬盘上读的字节数*/
    int rc;                       /* sqlite3OsRead() return code */

    /* Determine how many bytes of data to read. 决定要读的字节数*/
    nRead = (int)(p->iEof - p->iReadOff);
    if( nRead>p->nBuffer ) nRead = p->nBuffer;
    assert( nRead>0 );

    /* Read data from the file. Return early if an error occurs. 从文件中读数据，如果发生错误就提前返回*/
    rc = sqlite3OsRead(p->pFile, p->aBuffer, nRead, p->iReadOff);
    assert( rc!=SQLITE_IOERR_SHORT_READ );
    if( rc!=SQLITE_OK ) return rc;
  }
  nAvail = p->nBuffer - iBuf; 

  if( nByte<=nAvail ){
    /* The requested data is available in the in-memory buffer. In this
    ** case there is no need to make a copy of the data, just return a 
    ** pointer into the buffer to the caller.  
	** 需要的数据全都在内存的缓存中，这种情况下，就没必要在对数据进行备份，只需把指向缓存的一个指针返回给调用者即可
	*/
    *ppOut = &p->aBuffer[iBuf];
    p->iReadOff += nByte;
  }else{
    /* The requested data is not all available in the in-memory buffer.
    ** In this case, allocate space at p->aAlloc[] to copy the requested
    ** range into. Then return a copy of pointer p->aAlloc to the caller.  
	** 需要的数据不全在内存的缓存中，这种情况下，在p->aAlloc[]中分配空间，用来把需要的数据拷贝进来
	** 最后，返回指针p->aAlloc的一个副本给调用者
	*/
    int nRem;                     /* Bytes remaining to copy 余下的、要复制的字节数目*/

    /* Extend the p->aAlloc[] allocation if required. 若有必要，扩展p->aAlloc[]的大小*/
    if( p->nAlloc<nByte ){
      int nNew = p->nAlloc*2;
      while( nByte>nNew ) nNew = nNew*2;
      p->aAlloc = sqlite3DbReallocOrFree(db, p->aAlloc, nNew);
      if( !p->aAlloc ) return SQLITE_NOMEM;
      p->nAlloc = nNew;
    }

    /* Copy as much data as is available in the buffer into the start of
    ** p->aAlloc[].  尽量多的把buffer中的可用数据复制到p->aAlloc[]开始处*/
    memcpy(p->aAlloc, &p->aBuffer[iBuf], nAvail);
    p->iReadOff += nAvail;
    nRem = nByte - nAvail;

    /* The following loop copies up to p->nBuffer bytes per iteration into
    ** the p->aAlloc[] buffer.  
	下面这个循环，在每次迭代过程中，都把至多p->nBuffer（写缓存字节数）个字节拷贝到p->aAlloc[]缓存中*/
    while( nRem>0 ){//只要余下的、要复制的字节数目大于零就循环
      int rc;                     /* vdbeSorterIterRead() return code */
      int nCopy;                  /* Number of bytes to copy 要拷贝的字节的个数*/
      u8 *aNext;                  /* Pointer to buffer to copy data from 指向缓存的指针，要从相应缓存中复制数据*/

      nCopy = nRem;
      if( nRem>p->nBuffer ) nCopy = p->nBuffer;
      rc = vdbeSorterIterRead(db, p, nCopy, &aNext);
      if( rc!=SQLITE_OK ) return rc;
      assert( aNext!=p->aAlloc );
      memcpy(&p->aAlloc[nByte - nRem], aNext, nCopy);
      nRem -= nCopy;
    }

    *ppOut = p->aAlloc;
  }

  return SQLITE_OK;
}
//函数vdbeSorterIterRead()结束了


/*
** Read a varint from the stream of data accessed by p. Set *pnOut to
** the value read.
   下面这个函数的功能是：从和参变量VdbeSorterIter *p相对应的数据流中读一个varint（可变长整数），
   并使指针pnOut指向读出来的这个数
*/
static int vdbeSorterIterVarint(sqlite3 *db, VdbeSorterIter *p, u64 *pnOut){
  int iBuf;

  iBuf = p->iReadOff % p->nBuffer;
  if( iBuf && (p->nBuffer-iBuf)>=9 ){
    p->iReadOff += sqlite3GetVarint(&p->aBuffer[iBuf], pnOut);
  }else{
    u8 aVarint[16], *a;
    int i = 0, rc;
    do{
      rc = vdbeSorterIterRead(db, p, 1, &a);
      if( rc ) return rc;
      aVarint[(i++)&0xf] = a[0];
    }while( (a[0]&0x80)!=0 );
    sqlite3GetVarint(aVarint, pnOut);
  }

  return SQLITE_OK;
}


/*
** Advance iterator pIter to the next key in its PMA. Return SQLITE_OK if
** no error occurs, or an SQLite error code if one does.
** 使迭代器pIter前进到对应PMA的下一个key，
** 如果没有错误发生就返回SQLITE_OK，如果有错误发生，就返回SQLite error。
*/
static int vdbeSorterIterNext(
  sqlite3 *db,                    /* Database handle (for sqlite3DbMalloc() ) 针对sqlite3DbMalloc()的数据库句柄*/
  VdbeSorterIter *pIter           /* Iterator to advance 要前进的迭代器*/
)
{
  int rc;                         /* Return Code */
  u64 nRec = 0;                   /* Size of record in bytes 记录的字节数大小*/

  if( pIter->iReadOff>=pIter->iEof ){
    /* This is an EOF condition 这是一个EOF条件*/
    vdbeSorterIterZero(db, pIter);
    return SQLITE_OK;
  }

  rc = vdbeSorterIterVarint(db, pIter, &nRec);
  if( rc==SQLITE_OK ){
    pIter->nKey = (int)nRec;
    rc = vdbeSorterIterRead(db, pIter, (int)nRec, &pIter->aKey);
  }

  return rc;
}

/*
** Initialize iterator pIter to scan through the PMA stored in file pFile
** starting at offset iStart and ending at offset iEof-1. 
** 初始化一个用来扫描对应PMA的迭代器pIter，PMA存在文件中的位置是：开始于偏移量位iStart的位置，结束于偏移量为iEof-1的位置    
** This function leaves the iterator pointing to the first key in the PMA (or EOF if the PMA is empty).
** 这个函数最后使迭代器指向对应PMA的第一个位置（或EOF位置，如果ＰＭＡ＼是空的话）。
*/
static int vdbeSorterIterInit(
  sqlite3 *db,                    /* Database handle 数据库句柄*/
  const VdbeSorter *pSorter,      /* Sorter object ——VdbeSorter的一个实例*/
  i64 iStart,                     /* Start offset in pFile ——ｐＦｉｌｅ中的初始偏移量*/
  VdbeSorterIter *pIter,          /* Iterator to populate 要增添的迭代器*/
  i64 *pnByte                     /* IN/OUT: Increment this value by PMA size 以ＰＭＡ的大小为单位增加变量pnByte的值*/
){
  int rc = SQLITE_OK;
  int nBuf;

  nBuf = sqlite3BtreeGetPageSize(db->aDb[0].pBt);

  assert( pSorter->iWriteOff>iStart );
  assert( pIter->aAlloc==0 );
  assert( pIter->aBuffer==0 );
  pIter->pFile = pSorter->pTemp1;
  pIter->iReadOff = iStart;
  pIter->nAlloc = 128;
  pIter->aAlloc = (u8 *)sqlite3DbMallocRaw(db, pIter->nAlloc);
  pIter->nBuffer = nBuf;
  pIter->aBuffer = (u8 *)sqlite3DbMallocRaw(db, nBuf);

  if( !pIter->aBuffer ){
    rc = SQLITE_NOMEM;
  }else{
    int iBuf;

    iBuf = iStart % nBuf;
    if( iBuf ){
      int nRead = nBuf - iBuf;
      if( (iStart + nRead) > pSorter->iWriteOff ){
        nRead = (int)(pSorter->iWriteOff - iStart);
      }
      rc = sqlite3OsRead(
          pSorter->pTemp1, &pIter->aBuffer[iBuf], nRead, iStart
      );
      assert( rc!=SQLITE_IOERR_SHORT_READ );
    }

    if( rc==SQLITE_OK ){
      u64 nByte;                       /* Size of PMA in bytes ——PMA的字节数大小*/
      pIter->iEof = pSorter->iWriteOff;
      rc = vdbeSorterIterVarint(db, pIter, &nByte);
      pIter->iEof = pIter->iReadOff + nByte;
      *pnByte += nByte;
    }
  }

  if( rc==SQLITE_OK ){
    rc = vdbeSorterIterNext(db, pIter);
  }
  return rc;
}


/*
** Compare key1 (buffer pKey1, size nKey1 bytes) with key2 (buffer pKey2, 
** size nKey2 bytes).  Argument pKeyInfo supplies the collation functions
** used by the comparison. If an error occurs, return an SQLite error code.
** Otherwise, return SQLITE_OK and set *pRes to a negative, zero or positive
** value, depending on whether key1 is smaller, equal to or larger than key2.
**　下面的函数用来比较key1和key2。参数pKeyInfo提供比较时要使用的校对功能。如果有错误发生就返回一个SQLite错误码，
　　否则就返回SQLITE_OK，并给*pRes赋值，如果ｋｅｙ１小，就赋负值，如果二者相等就赋０，若ｋｅｙ１大，就赋正值。
** If the bOmitRowid argument is non-zero, assume both keys end in a rowid　field. 
　　如果函数的参数bOmitRowid是非零的，就假设两个ｋｅｙｓ在ｒｏｗｉｄ域结尾。
** For the purposes of the comparison, ignore it. 
　　基于比较的目的，忽略这种情况。
**　Also, if bOmitRowid　is true and key1 contains even a single NULL value,
**  it is considered to　be less than key2. Even if key2 also contains NULL values.
** 如果bOmitRowid是真值，ｋｅｙ１仅含有一个单独的ＮＵＬＬ值，那么ｋｅｙ１小于ｋｅｙ２，甚至在ｋｅｙ２也包含一个ＮＵＬＬ值得情况下。
** If pKey2 is passed a NULL pointer, then it is assumed that the pCsr->aSpace
** has been allocated and contains an unpacked record that is used as key2.
**　如果ｐＫｅｙ２由一个空指针代表，则假设pCsr->aSpace已经被分配并包含一个未被解包的记录，被当做ｋｅｙ２使用。
*/
static void vdbeSorterCompare(
  const VdbeCursor *pCsr,         /* Cursor object (for pKeyInfo) 针对pKeyInfo的游标实例*/
  int bOmitRowid,                 /* Ignore rowid field at end of keys 忽略ｋｅｙｓ结尾处的ｒｏｗｉｄ域*/
  const void *pKey1, int nKey1,   /* Left side of comparison 要比较的一方*/
  const void *pKey2, int nKey2,   /* Right side of comparison 要比较的另一方*/
  int *pRes                       /* OUT: Result of comparison 比较后所得结果*/
){
  KeyInfo *pKeyInfo = pCsr->pKeyInfo;
  VdbeSorter *pSorter = pCsr->pSorter;
  UnpackedRecord *r2 = pSorter->pUnpacked;
  int i;

  if( pKey2 ){　
    sqlite3VdbeRecordUnpack(pKeyInfo, nKey2, pKey2, r2);
  }

  if( bOmitRowid ){
    r2->nField = pKeyInfo->nField;
    assert( r2->nField>0 );
    for(i=0; i<r2->nField; i++){
      if( r2->aMem[i].flags & MEM_Null ){
        *pRes = -1;
        return;
      }
    }
    r2->flags |= UNPACKED_PREFIX_MATCH;
  }

  *pRes = sqlite3VdbeRecordCompare(nKey1, pKey1, r2);
}

/*
** This function is called to compare two iterator keys when merging 
** multiple b-tree segments. Parameter iOut is the index of the aTree[] 
** value to recalculate.
　　这个函数在ｍｅｒｇｅ多元ｂ树段时被调用。参数iOut是ａＴｒｅｅ中要被重新计算值得元素的下标值。
*/
static int vdbeSorterDoCompare(const VdbeCursor *pCsr, int iOut){
  VdbeSorter *pSorter = pCsr->pSorter;
  int i1;
  int i2;
  int iRes;
  VdbeSorterIter *p1;
  VdbeSorterIter *p2;

  assert( iOut<pSorter->nTree && iOut>0 );

  if( iOut>=(pSorter->nTree/2) ){
    i1 = (iOut - pSorter->nTree/2) * 2;
    i2 = i1 + 1;
  }else{
    i1 = pSorter->aTree[iOut*2];
    i2 = pSorter->aTree[iOut*2+1];
  }

  p1 = &pSorter->aIter[i1];
  p2 = &pSorter->aIter[i2];

  if( p1->pFile==0 ){
    iRes = i2;
  }else if( p2->pFile==0 ){
    iRes = i1;
  }else{
    int res;
    assert( pCsr->pSorter->pUnpacked!=0 );  /* allocated in vdbeSorterMerge()部署在函数vdbeSorterMerge()中 */
    vdbeSorterCompare(
        pCsr, 0, p1->aKey, p1->nKey, p2->aKey, p2->nKey, &res
    );
    if( res<=0 ){
      iRes = i1;
    }else{
      iRes = i2;
    }
  }

  pSorter->aTree[iOut] = iRes;
  return SQLITE_OK;
}

/*
** Initialize the temporary index cursor just opened as a sorter cursor.——初始化临时索引游标，使之作为ｓｏｒｔｅｒ游标
*/
int sqlite3VdbeSorterInit(sqlite3 *db, VdbeCursor *pCsr){
  int pgsz;                       /* Page size of main database 主数据库的页大小*/
  int mxCache;                    /* Cache size ——Ｃａｃｈｅ缓存大小*/
  VdbeSorter *pSorter;            /* The new sorter 指向新ｓｏｒｔｅｒ的指针*/
  char *d;                        /* Dummy */

  assert( pCsr->pKeyInfo && pCsr->pBt==0 );
  pCsr->pSorter = pSorter = sqlite3DbMallocZero(db, sizeof(VdbeSorter));
  if( pSorter==0 ){
    return SQLITE_NOMEM;
  }
  
  pSorter->pUnpacked = sqlite3VdbeAllocUnpackedRecord(pCsr->pKeyInfo, 0, 0, &d);
  if( pSorter->pUnpacked==0 ) return SQLITE_NOMEM;
  assert( pSorter->pUnpacked==(UnpackedRecord *)d );

  if( !sqlite3TempInMemory(db) ){
    pgsz = sqlite3BtreeGetPageSize(db->aDb[0].pBt);
    pSorter->mnPmaSize = SORTER_MIN_WORKING * pgsz;
    mxCache = db->aDb[0].pSchema->cache_size;
    if( mxCache<SORTER_MIN_WORKING ) mxCache = SORTER_MIN_WORKING;
    pSorter->mxPmaSize = mxCache * pgsz;
  }

  return SQLITE_OK;
}

/*
** Free the list of sorted records starting at pRecord.——下面的函数的功能：从ｐＲｅｃｏｒｄ所指的地方开始释放已排序的记录列表
*/
static void vdbeSorterRecordFree(sqlite3 *db, SorterRecord *pRecord){
  SorterRecord *p;
  SorterRecord *pNext;
  for(p=pRecord; p; p=pNext){
    pNext = p->pNext;
    sqlite3DbFree(db, p);
  }
}

/*
** Free any cursor components allocated by sqlite3VdbeSorterXXX routines.
	释放任何一个由sqlite3VdbeSorterXXX routine部署的游标元素
*/
void sqlite3VdbeSorterClose(sqlite3 *db, VdbeCursor *pCsr){
  VdbeSorter *pSorter = pCsr->pSorter;
  if( pSorter ){
    if( pSorter->aIter ){
      int i;
      for(i=0; i<pSorter->nTree; i++){
        vdbeSorterIterZero(db, &pSorter->aIter[i]);
      }
      sqlite3DbFree(db, pSorter->aIter);
    }
    if( pSorter->pTemp1 ){
      sqlite3OsCloseFree(pSorter->pTemp1);
    }
    vdbeSorterRecordFree(db, pSorter->pRecord);
    sqlite3DbFree(db, pSorter->pUnpacked);
    sqlite3DbFree(db, pSorter);
    pCsr->pSorter = 0;
  }
}

/*
** Allocate space for a file-handle and open a temporary file. If successful,
** set *ppFile to point to the malloc'd file-handle and return SQLITE_OK.
** Otherwise, set *ppFile to 0 and return an SQLite error code.
	下面这个函数用来为文件句柄分配空间，并开辟一个临时文件。如果成功了，设置指针ppFile 指向 malloc的文件句柄并返回SQLITE_OK
*/
static int vdbeSorterOpenTempFile(sqlite3 *db, sqlite3_file **ppFile){
  int dummy;
  return sqlite3OsOpenMalloc(db->pVfs, 0, ppFile,
      SQLITE_OPEN_TEMP_JOURNAL |
      SQLITE_OPEN_READWRITE    | SQLITE_OPEN_CREATE |
      SQLITE_OPEN_EXCLUSIVE    | SQLITE_OPEN_DELETEONCLOSE, &dummy
  );
}

/*
** Merge the two sorted lists p1 and p2 into a single list.
** Set *ppOut to the head of the new list.
　　下面这个函数把两个已经排好序的列表合并成一个，并把ｐｐＯｕｔ指向新列表的表头
*/
static void vdbeSorterMerge(
  const VdbeCursor *pCsr,         /* For pKeyInfo */
  SorterRecord *p1,               /* First list to merge 参与合并的第一个列表*/
  SorterRecord *p2,               /* Second list to merge 参与合并的第二个列表*/
  SorterRecord **ppOut            /* OUT: Head of merged list 返回的：指向合并后的列表的头的指针*/
){
  SorterRecord *pFinal = 0;
  SorterRecord **pp = &pFinal;
  void *pVal2 = p2 ? p2->pVal : 0;

  while( p1 && p2 ){
    int res;
    vdbeSorterCompare(pCsr, 0, p1->pVal, p1->nVal, pVal2, p2->nVal, &res);
    if( res<=0 ){
      *pp = p1;
      pp = &p1->pNext;
      p1 = p1->pNext;
      pVal2 = 0;
    }else{
      *pp = p2;
       pp = &p2->pNext;
      p2 = p2->pNext;
      if( p2==0 ) break;
      pVal2 = p2->pVal;
    }
  }
  *pp = p1 ? p1 : p2;
  *ppOut = pFinal;
}

/*
** Sort the linked list of records headed at pCsr->pRecord. Return SQLITE_OK
** if successful, or an SQLite error code (i.e. SQLITE_NOMEM) if an error
** occurs.
　　对头在pCsr->pRecord处的记录链表排序。成功就返回SQLITE_OK；否则，返回SQLite错误码
*/
static int vdbeSorterSort(const VdbeCursor *pCsr){
  int i;
  SorterRecord **aSlot;
  SorterRecord *p;
  VdbeSorter *pSorter = pCsr->pSorter;

  aSlot = (SorterRecord **)sqlite3MallocZero(64 * sizeof(SorterRecord *));
  if( !aSlot ){
    return SQLITE_NOMEM;
  }

  p = pSorter->pRecord;
  while( p ){
    SorterRecord *pNext = p->pNext;
    p->pNext = 0;
    for(i=0; aSlot[i]; i++){
      vdbeSorterMerge(pCsr, p, aSlot[i], &p);
      aSlot[i] = 0;
    }
    aSlot[i] = p;
    p = pNext;
  }

  p = 0;
  for(i=0; i<64; i++){
    vdbeSorterMerge(pCsr, p, aSlot[i], &p);
  }
  pSorter->pRecord = p;

  sqlite3_free(aSlot);
  return SQLITE_OK;
}

/*
** Initialize a file-writer object.初始化一个file-writer的实例
*/
static void fileWriterInit(
  sqlite3 *db,                    /* Database (for malloc) 针对malloc的数据库*/
  sqlite3_file *pFile,            /* File to write to 指向写入文件的指针*/
  FileWriter *p,                  /* Object to populate 要增添的对象*/
  i64 iStart                      /* Offset of pFile to begin writing at 文件中，开始写的位置的偏移量*/
){
  int nBuf = sqlite3BtreeGetPageSize(db->aDb[0].pBt);

  memset(p, 0, sizeof(FileWriter));
  p->aBuffer = (u8 *)sqlite3DbMallocRaw(db, nBuf);
  if( !p->aBuffer ){
    p->eFWErr = SQLITE_NOMEM;
  }else{
    p->iBufEnd = p->iBufStart = (iStart % nBuf);
    p->iWriteOff = iStart - p->iBufStart;
    p->nBuffer = nBuf;
    p->pFile = pFile;
  }
}

/*
** Write nData bytes of data to the file-write object. Return SQLITE_OK
** if successful, or an SQLite error code if an error occurs.
	下面的函数往file-write实例中写nData字节的数据，成功就返回SQLITE_OK
	否则返回SQLite错误码
*/
static void fileWriterWrite(FileWriter *p, u8 *pData, int nData){
  int nRem = nData;
  while( nRem>0 && p->eFWErr==0 ){
    int nCopy = nRem;
    if( nCopy>(p->nBuffer - p->iBufEnd) ){
      nCopy = p->nBuffer - p->iBufEnd;
    }

    memcpy(&p->aBuffer[p->iBufEnd], &pData[nData-nRem], nCopy);
    p->iBufEnd += nCopy;
    if( p->iBufEnd==p->nBuffer ){
      p->eFWErr = sqlite3OsWrite(p->pFile, 
          &p->aBuffer[p->iBufStart], p->iBufEnd - p->iBufStart, 
          p->iWriteOff + p->iBufStart
      );
      p->iBufStart = p->iBufEnd = 0;
      p->iWriteOff += p->nBuffer;
    }
    assert( p->iBufEnd<p->nBuffer );

    nRem -= nCopy;
  }
}

/*
** Flush any buffered data to disk and clean up the file-writer object.把所有缓存的数据都存到磁盘上，并清空file-writer实例
** The results of using the file-writer after this call are undefined.调用完下面的函数后再使用file-writer的结果还没有定义
** Return SQLITE_OK if flushing the buffered data succeeds or is not 成功就返回SQLITE_OK
** required. Otherwise, return an SQLite error code.失败就返回错误码
**
** Before returning, set *piEof to the offset immediately following the
** last byte written to the file.
　　在ｒｅｔｕｒｎ之前，把最后写入的一个字节后面所对应的偏移量赋给*piEof

*/
static int fileWriterFinish(sqlite3 *db, FileWriter *p, i64 *piEof){
  int rc;
  if( p->eFWErr==0 && ALWAYS(p->aBuffer) && p->iBufEnd>p->iBufStart ){
    p->eFWErr = sqlite3OsWrite(p->pFile, 
        &p->aBuffer[p->iBufStart], p->iBufEnd - p->iBufStart, 
        p->iWriteOff + p->iBufStart
    );
  }
  *piEof = (p->iWriteOff + p->iBufEnd);
  sqlite3DbFree(db, p->aBuffer);
  rc = p->eFWErr;
  memset(p, 0, sizeof(FileWriter));
  return rc;
}

/*
** Write value iVal encoded as a varint to the file-write object. Return 
** SQLITE_OK if successful, or an SQLite error code if an error occurs.
	下面这个函数把形参iVal（编码成一个可变长整数变量）的值传到一个file-write实例中
	成功返回SQLITE_OK，失败则返回错误码
*/
static void fileWriterWriteVarint(FileWriter *p, u64 iVal){
  int nByte; 
  u8 aByte[10];
  nByte = sqlite3PutVarint(aByte, iVal);
  fileWriterWrite(p, aByte, nByte);
}

/*
** Write the current contents of the in-memory linked-list to a PMA. Return
** SQLITE_OK if successful, or an SQLite error code otherwise.
**　把内存中的链表的当前内容写到一个PMA中。成功就返回SQLITE_OK，否则就返回一个错误码
** The format of a PMA is:
**　ＰＭＡ的格式如下：
**     * A varint. This varint contains the total number of bytes of content
**       in the PMA (not including the varint itself).
**　　　一个可变长的整数变量，这个变量包含ＰＭＡ中所有内容的字节数
**     * One or more records packed end-to-end in order of ascending keys. 
**       Each record consists of a varint followed by a blob of data (the 
**       key). The varint is the number of bytes in the blob of data.
		　一个或多个记录以尾对尾的方式、按照ｋｅｙｓ的递增顺序排序。每条记录都由一个可变长整数和其后的一系列数据组成。
		　可变长变量的值等于其后一系列的数据占用的字节的数目
*/
static int vdbeSorterListToPMA(sqlite3 *db, const VdbeCursor *pCsr){
  int rc = SQLITE_OK;             /* Return code 返回代码*/
  VdbeSorter *pSorter = pCsr->pSorter;
  FileWriter writer;

  memset(&writer, 0, sizeof(FileWriter));

  if( pSorter->nInMemory==0 ){
    assert( pSorter->pRecord==0 );
    return rc;
  }

  rc = vdbeSorterSort(pCsr);

  /* If the first temporary PMA file has not been opened, open it now. 如果第一个临时ＰＭＡ文件没有打开，现在就打开*/
  if( rc==SQLITE_OK && pSorter->pTemp1==0 ){
    rc = vdbeSorterOpenTempFile(db, &pSorter->pTemp1);
    assert( rc!=SQLITE_OK || pSorter->pTemp1 );
    assert( pSorter->iWriteOff==0 );
    assert( pSorter->nPMA==0 );
  }

  if( rc==SQLITE_OK ){
    SorterRecord *p;
    SorterRecord *pNext = 0;

    fileWriterInit(db, pSorter->pTemp1, &writer, pSorter->iWriteOff);
    pSorter->nPMA++;
    fileWriterWriteVarint(&writer, pSorter->nInMemory);
    for(p=pSorter->pRecord; p; p=pNext){
      pNext = p->pNext;
      fileWriterWriteVarint(&writer, p->nVal);
      fileWriterWrite(&writer, p->pVal, p->nVal);
      sqlite3DbFree(db, p);
    }
    pSorter->pRecord = p;
    rc = fileWriterFinish(db, &writer, &pSorter->iWriteOff);
  }

  return rc;
}

/*
** Add a record to the sorter.把一个记录添加到sorter
*/
int sqlite3VdbeSorterWrite(
  sqlite3 *db,                    /* Database handle 数据库句柄*/
  const VdbeCursor *pCsr,               /* Sorter cursor 游标*/
  Mem *pVal                       /* Memory cell containing record 含有记录的内存单元*/
){
  VdbeSorter *pSorter = pCsr->pSorter;
  int rc = SQLITE_OK;             /* Return Code 返回码*/
  SorterRecord *pNew;             /* New list element 新列表元素*/

  assert( pSorter );
  pSorter->nInMemory += sqlite3VarintLen(pVal->n) + pVal->n;

  pNew = (SorterRecord *)sqlite3DbMallocRaw(db, pVal->n + sizeof(SorterRecord));
  if( pNew==0 ){
    rc = SQLITE_NOMEM;
  }else{
    pNew->pVal = (void *)&pNew[1];
    memcpy(pNew->pVal, pVal->z, pVal->n);
    pNew->nVal = pVal->n;
    pNew->pNext = pSorter->pRecord;
    pSorter->pRecord = pNew;
  }

  /* See if the contents of the sorter should now be written out. They
  ** are written out when either of the following are true:
  ** 判断sorter的内容是不是现在就写出去，只要满足下面的条件之一就可写出
  **   * The total memory allocated for the in-memory list is greater 
  **     than (page-size * cache-size), or
  **		已经为内存中的列表分配的内存大于page-size * cache-size时
  **   * The total memory allocated for the in-memory list is greater 
  **     than (page-size * 10) and sqlite3HeapNearlyFull() returns true.
  */			//已经为内存中的列表分配的内存大于page-size * 10并且函数sqlite3HeapNearlyFull()返回真时
  if( rc==SQLITE_OK && pSorter->mxPmaSize>0 && (
        (pSorter->nInMemory>pSorter->mxPmaSize)
     || (pSorter->nInMemory>pSorter->mnPmaSize && sqlite3HeapNearlyFull())
  )){
#ifdef SQLITE_DEBUG
    i64 nExpect = pSorter->iWriteOff
                + sqlite3VarintLen(pSorter->nInMemory)
                + pSorter->nInMemory;
#endif
    rc = vdbeSorterListToPMA(db, pCsr);
    pSorter->nInMemory = 0;
    assert( rc!=SQLITE_OK || (nExpect==pSorter->iWriteOff) );
  }

  return rc;
}

/*
** Helper function for sqlite3VdbeSorterRewind().
   下面的函数是函数sqlite3VdbeSorterRewind()的辅助函数
*/
static int vdbeSorterInitMerge(
  sqlite3 *db,                    /* Database handle 数据库句柄*/
  const VdbeCursor *pCsr,         /* Cursor handle for this sorter 此sorter的游标句柄*/
  i64 *pnByte                     /* Sum of bytes in all opened PMAs 所有PMA中的字节总和*/
){
  VdbeSorter *pSorter = pCsr->pSorter;
  int rc = SQLITE_OK;             /* Return code 返回码*/
  int i;                          /* Used to iterator through aIter[] 循环变量*/
  i64 nByte = 0;                  /* Total bytes in all opened PMAs 所有PMA中的字节总和*/

  /* Initialize the iterators. 初始化iterators*/
  for(i=0; i<SORTER_MAX_MERGE_COUNT; i++){
    VdbeSorterIter *pIter = &pSorter->aIter[i];
    rc = vdbeSorterIterInit(db, pSorter, pSorter->iReadOff, pIter, &nByte);
    pSorter->iReadOff = pIter->iEof;
    assert( rc!=SQLITE_OK || pSorter->iReadOff<=pSorter->iWriteOff );
    if( rc!=SQLITE_OK || pSorter->iReadOff>=pSorter->iWriteOff ) break;
  }

  /* Initialize the aTree[] array. 初始化aTree[]数组*/
  for(i=pSorter->nTree-1; rc==SQLITE_OK && i>0; i--){
    rc = vdbeSorterDoCompare(pCsr, i);
  }

  *pnByte = nByte;
  return rc;
}

/*
** Once the sorter has been populated, this function is called to prepare
** for iterating through its contents in sorted order.
   一旦这个sorter被增添，下面这个函数就被调用以排好的顺序遍历sorter的内容
*/
int sqlite3VdbeSorterRewind(sqlite3 *db, const VdbeCursor *pCsr, int *pbEof){
  VdbeSorter *pSorter = pCsr->pSorter;
  int rc;                         /* Return code 返回码*/
  sqlite3_file *pTemp2 = 0;       /* Second temp file to use 要使用的第二个临时文件*/
  i64 iWrite2 = 0;                /* Write offset for pTemp2 为pTemp2定义偏移量*/
  int nIter;                      /* Number of iterators used 被使用的迭代器的个数*/
  int nByte;                      /* Bytes of space required for aIter/aTree ——aIter/aTree需要的空间的字节数*/
  int N = 2;                      /* Power of 2 >= nIter ；2的幂>= nIter*/

  assert( pSorter );

  /* If no data has been written to disk, then do not do so now. Instead,
  ** sort the VdbeSorter.pRecord list. The vdbe layer will read data directly
  ** from the in-memory list.  
     如果还没有数据被写到磁盘，现在就先暂时不做。而是对VdbeSorter.pRecord list进行排序，vdbe层将直接从内存列表里读数据
  */
  if( pSorter->nPMA==0 ){
    *pbEof = !pSorter->pRecord;
    assert( pSorter->aTree==0 );
    return vdbeSorterSort(pCsr);
  }

  /* Write the current in-memory list to a PMA. 把当前内存中的列表写到PMA中去*/
  rc = vdbeSorterListToPMA(db, pCsr);
  if( rc!=SQLITE_OK ) return rc;

  /* Allocate space for aIter[] and aTree[]. 为aIter[] 和 aTree[]分配空间*/
  nIter = pSorter->nPMA;
  if( nIter>SORTER_MAX_MERGE_COUNT ) nIter = SORTER_MAX_MERGE_COUNT;
  assert( nIter>0 );
  while( N<nIter ) N += N;
  nByte = N * (sizeof(int) + sizeof(VdbeSorterIter));
  pSorter->aIter = (VdbeSorterIter *)sqlite3DbMallocZero(db, nByte);
  if( !pSorter->aIter ) return SQLITE_NOMEM;
  pSorter->aTree = (int *)&pSorter->aIter[N];
  pSorter->nTree = N;

  do {
    int iNew;                     /* Index of new, merged, PMA 新合并进来的PMA的下标*/

    for(iNew=0; 
        rc==SQLITE_OK && iNew*SORTER_MAX_MERGE_COUNT<pSorter->nPMA; 
        iNew++
    ){
      int rc2;                    /* Return code from fileWriterFinish() ；从函数fileWriterFinish()返回的返回码*/
      FileWriter writer;          /* Object used to write to disk 用来往磁盘里写数据的实例*/
      i64 nWrite;                 /* Number of bytes in new PMA 新PMA中的字节数目*/

      memset(&writer, 0, sizeof(FileWriter));

      /* If there are SORTER_MAX_MERGE_COUNT or less PMAs in file pTemp1,
      ** initialize an iterator for each of them and break out of the loop.
      ** These iterators will be incrementally merged as the VDBE layer calls
      ** sqlite3VdbeSorterNext().
      ** 如果文件pTemp1中有SORTER_MAX_MERGE_COUNT个或者更少的PMA，就为他们各初始化一个迭代器，
		 然后跳出循环。
      ** Otherwise, if pTemp1 contains more than SORTER_MAX_MERGE_COUNT PMAs,
      ** initialize interators for SORTER_MAX_MERGE_COUNT of them. These PMAs
      ** are merged into a single PMA that is written to file pTemp2.
		 否则，如果文件pTemp1中有多于SORTER_MAX_MERGE_COUNT个的PMA，就为其中的SORTER_MAX_MERGE_COUNT个PMA各初始化一个迭代器，
		 把这些PMA合并成一个PMA，并把它写入文件pTemp2中
      */
      rc = vdbeSorterInitMerge(db, pCsr, &nWrite);
      assert( rc!=SQLITE_OK || pSorter->aIter[ pSorter->aTree[1] ].pFile );
      if( rc!=SQLITE_OK || pSorter->nPMA<=SORTER_MAX_MERGE_COUNT ){
        break;
      }

      /* Open the second temp file, if it is not already open. 如果第二个临时文件还没打开的话，则现在就打开*/
      if( pTemp2==0 ){
        assert( iWrite2==0 );
        rc = vdbeSorterOpenTempFile(db, &pTemp2);
      }

      if( rc==SQLITE_OK ){
        int bEof = 0;
        fileWriterInit(db, pTemp2, &writer, iWrite2);
        fileWriterWriteVarint(&writer, nWrite);
        while( rc==SQLITE_OK && bEof==0 ){
          VdbeSorterIter *pIter = &pSorter->aIter[ pSorter->aTree[1] ];
          assert( pIter->pFile );

          fileWriterWriteVarint(&writer, pIter->nKey);
          fileWriterWrite(&writer, pIter->aKey, pIter->nKey);
          rc = sqlite3VdbeSorterNext(db, pCsr, &bEof);
        }
        rc2 = fileWriterFinish(db, &writer, &iWrite2);
        if( rc==SQLITE_OK ) rc = rc2;
      }
    }

    if( pSorter->nPMA<=SORTER_MAX_MERGE_COUNT ){
      break;
    }else{
      sqlite3_file *pTmp = pSorter->pTemp1;
      pSorter->nPMA = iNew;
      pSorter->pTemp1 = pTemp2;
      pTemp2 = pTmp;
      pSorter->iWriteOff = iWrite2;
      pSorter->iReadOff = 0;
      iWrite2 = 0;
    }
  }while( rc==SQLITE_OK );

  if( pTemp2 ){
    sqlite3OsCloseFree(pTemp2);
  }
  *pbEof = (pSorter->aIter[pSorter->aTree[1]].pFile==0);
  return rc;
}

/*
** Advance to the next element in the sorter.——前进到sorter中的下一个元素
*/
int sqlite3VdbeSorterNext(sqlite3 *db, const VdbeCursor *pCsr, int *pbEof){
  VdbeSorter *pSorter = pCsr->pSorter;
  int rc;                         /* 返回码 Return code */

  if( pSorter->aTree ){
    int iPrev = pSorter->aTree[1];/* Index of iterator to advance 要前进的迭代器的下标*/
    int i;                        /* Index of aTree[] to recalculate 要重算的数组aTree[]中元素的下标*/

    rc = vdbeSorterIterNext(db, &pSorter->aIter[iPrev]);
    for(i=(pSorter->nTree+iPrev)/2; rc==SQLITE_OK && i>0; i=i/2){
      rc = vdbeSorterDoCompare(pCsr, i);
    }

    *pbEof = (pSorter->aIter[pSorter->aTree[1]].pFile==0);
  }else{
    SorterRecord *pFree = pSorter->pRecord;
    pSorter->pRecord = pFree->pNext;
    pFree->pNext = 0;
    vdbeSorterRecordFree(db, pFree);
    *pbEof = !pSorter->pRecord;
    rc = SQLITE_OK;
  }
  return rc;
}

/*
** Return a pointer to a buffer owned by the sorter that contains the current key.
   返回一个指针给buffer，这个buffer是包含当前key的sorter的
** 
*/
static void *vdbeSorterRowkey(
  const VdbeSorter *pSorter,      /* Sorter object ——sorter实例*/
  int *pnKey                      /* OUT: Size of current key in bytes 输出：当前值得字节数大小*/
){
  void *pKey;
  if( pSorter->aTree ){
    VdbeSorterIter *pIter;
    pIter = &pSorter->aIter[ pSorter->aTree[1] ];
    *pnKey = pIter->nKey;
    pKey = pIter->aKey;
  }else{
    *pnKey = pSorter->pRecord->nVal;
    pKey = pSorter->pRecord->pVal;
  }
  return pKey;
}

/*
** Copy the current sorter key into the memory cell pOut.把当前sorter key复制到内存单元pOut中
*/
int sqlite3VdbeSorterRowkey(const VdbeCursor *pCsr, Mem *pOut){
  VdbeSorter *pSorter = pCsr->pSorter;
  void *pKey; int nKey;           /* Sorter key to copy into pOut */

  pKey = vdbeSorterRowkey(pSorter, &nKey);
  if( sqlite3VdbeMemGrow(pOut, nKey, 0) ){
    return SQLITE_NOMEM;
  }
  pOut->n = nKey;
  MemSetTypeFlag(pOut, MEM_Blob);
  memcpy(pOut->z, pKey, nKey);

  return SQLITE_OK;
}

/*
** Compare the key in memory cell pVal with the key that the sorter cursor
** passed as the first argument currently points to. For the purposes of
** the comparison, ignore the rowid field at the end of each record.
** 把内存单元pVal中的key和由sorter cursor当做第一个参数传出来的key相比较；忽略每条记录末尾处的rowid域
** If an error occurs, return an SQLite error code (i.e. SQLITE_NOMEM).
** Otherwise, set *pRes to a negative, zero or positive value if the
** key in pVal is smaller than, equal to or larger than the current sorter
** key.
	如有错误发生，就返回一个SQLite错误码；否则把*pRes设置成一个负数，0，或正数，分别对应pVal中的key比当前sorter key小、相等或大。	
*/
int sqlite3VdbeSorterCompare(
  const VdbeCursor *pCsr,         /* Sorter cursor ——sorter游标*/
  Mem *pVal,                      /* Value to compare to current sorter key 要和当前sorter key相比较的值*/
  int *pRes                       /* OUT: Result of comparison 输出：比较的结果*/
){
  VdbeSorter *pSorter = pCsr->pSorter;
  void *pKey; int nKey;           /* Sorter key to compare pVal with 要和pVal相比较的sorter key*/

  pKey = vdbeSorterRowkey(pSorter, &nKey);
  vdbeSorterCompare(pCsr, 1, pVal->z, pVal->n, pKey, nKey, pRes);
  return SQLITE_OK;
}

#endif /* #ifndef SQLITE_OMIT_MERGE_SORT 结束宏定义*/
