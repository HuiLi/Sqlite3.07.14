/*
** 2008 October 7
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
** This file contains code use to implement an in-memory rollback journal.
** The in-memory rollback journal is used to journal transactions for
** ":memory:" databases and when the journal_mode=MEMORY pragma is used.
*/
/*此文件中的代码用于实现内存DB回滚日志。在内存中的回滚事务用于存储事务汇报，
当journal_mode=MEMORY，说明内存编译使用中。*/
#include "sqliteInt.h"

/* Forward references to internal structures */
/*对于内部结构的引用*/
typedef struct MemJournal MemJournal;
typedef struct FilePoint FilePoint;
typedef struct FileChunk FileChunk;

/* Space to hold the rollback journal is allocated in increments of
** this many bytes.
**存放回滚日志的空间被分配在这么多字节的内存增量中。
** The size chosen is a little less than a power of two.  That way,
** the FileChunk object will have a size that almost exactly fills
** a power-of-two allocation.  This mimimizes wasted space in power-of-two
** memory allocators.
*/
/*输入的大小小于2的幂。这样，FileChunk对象会填充内存分配大小至2的幂数。
**这样就能将空间浪费降至最小。
*/
#define JOURNAL_CHUNKSIZE ((int)(1024-sizeof(FileChunk*)))

/* Macro to find the minimum of two numeric values.
*/
/*宏能找到两个数中的最小值*/
#ifndef MIN
# define MIN(x,y) ((x)<(y)?(x):(y))
#endif
 
/*
** The rollback journal is composed of a linked list of these structures.
*/
/*回滚日志是由这些结构的链表组成的。*/
struct FileChunk {
  FileChunk *pNext;               /* Next chunk in the journal *//*指向下一个节点*/
  u8 zChunk[JOURNAL_CHUNKSIZE];   /* Content of this chunk *//*这个节点的内容*/
};

/*
** An instance of this object serves as a cursor into the rollback journal.
** The cursor can be either for reading or writing.
*/
/*这个对象的一个实例作为一个游标到回滚日志。这个游标可以进行读和写。*/
/*游标（cursor）是系统为用户开设的一个数据缓冲区，存放SQL语句的执行结果。
**每个游标区都有一个名字,用户可以用SQL语句逐一从游标中获取记录，并赋给主变量，
**交由主语言进一步处理。
*/
struct FilePoint {
  sqlite3_int64 iOffset;          /* Offset from the beginning of the file *//*文件开头的偏移量*/
  FileChunk *pChunk;              /* Specific chunk into which cursor points *//*游标指到的内存块*/
};

/*
** This subclass is a subclass of sqlite3_file.  Each open memory-journal
** is an instance of this class.
*/
/*这是sqlite3_file的一个子类。每一个打开的内存日志是这个类的一个实例。*/
struct MemJournal {
  sqlite3_io_methods *pMethod;    /* Parent class. MUST BE FIRST 。*//*指向父类的指针*/
  FileChunk *pFirst;              /* Head of in-memory chunk-list *//*指向内存块列表的头*/
  FilePoint endpoint;             /* Pointer to the end of the file *//*指向文件的末尾*/
  FilePoint readpoint;            /* Pointer to the end of the last xRead() *//*到最后xread()末端指针*/
};

/*
** Read data from the in-memory journal file.  This is the implementation
** of the sqlite3_vfs.xRead method.
*/
/*从内存中读取数据的日志文件。 这是一个实现sqlite3_vfs.xRead方法。*/
static int memjrnlRead(
  sqlite3_file *pJfd,    /* The journal file from which to read *//*日志文件阅读*/
  void *zBuf,            /* Put the results here *//*存放结果*/
  int iAmt,              /* Number of bytes to read *//*读取的字节数*/
  sqlite_int64 iOfst     /* Begin reading at this offset *//*从这个偏移量开始阅读*/
){
  MemJournal *p = (MemJournal *)pJfd;
  u8 *zOut = zBuf;
  int nRead = iAmt;
  int iChunkOffset;
  FileChunk *pChunk;

  /* SQLite never tries to read past the end of a rollback journal file */
  /*SQLite不会试图读取之前回滚日志文件的最后事务。*/
  assert( iOfst+iAmt<=p->endpoint.iOffset );

  if( p->readpoint.iOffset!=iOfst || iOfst==0 ){
    sqlite3_int64 iOff = 0;
    for(pChunk=p->pFirst; 
        ALWAYS(pChunk) && (iOff+JOURNAL_CHUNKSIZE)<=iOfst;
        pChunk=pChunk->pNext
    ){
      iOff += JOURNAL_CHUNKSIZE;
    }
  }else{
    pChunk = p->readpoint.pChunk;
  }

  iChunkOffset = (int)(iOfst%JOURNAL_CHUNKSIZE);
  do {
    int iSpace = JOURNAL_CHUNKSIZE - iChunkOffset;
    int nCopy = MIN(nRead, (JOURNAL_CHUNKSIZE - iChunkOffset));
    memcpy(zOut, &pChunk->zChunk[iChunkOffset], nCopy);
    zOut += nCopy;
    nRead -= iSpace;
    iChunkOffset = 0;
  } while( nRead>=0 && (pChunk=pChunk->pNext)!=0 && nRead>0 );
  p->readpoint.iOffset = iOfst+iAmt;
  p->readpoint.pChunk = pChunk;

  return SQLITE_OK;
}

/*
** Write data to the file.将数据写入文件
*/
static int memjrnlWrite(
  sqlite3_file *pJfd,    /* The journal file into which to write *//*写入日志文件*/
  const void *zBuf,      /* Take data to be written from here *//*将zBuf中的数据写入*/
  int iAmt,              /* Number of bytes to write *//*写入数据的大小*/
  sqlite_int64 iOfst     /* Begin writing at this offset into the file *//*从这个偏移量开始写入*/
){
  MemJournal *p = (MemJournal *)pJfd;
  int nWrite = iAmt;
  u8 *zWrite = (u8 *)zBuf;

  /* An in-memory journal file should only ever be appended to. Random
  ** access writes are not required by sqlite.
  */
  /*内存中的日志文件只能被追加，sqlite不需要写入数据。*/
  assert( iOfst==p->endpoint.iOffset );
  UNUSED_PARAMETER(iOfst);

  while( nWrite>0 ){
    FileChunk *pChunk = p->endpoint.pChunk;
    int iChunkOffset = (int)(p->endpoint.iOffset%JOURNAL_CHUNKSIZE);
    int iSpace = MIN(nWrite, JOURNAL_CHUNKSIZE - iChunkOffset);

    if( iChunkOffset==0 ){
      /* New chunk is required to extend the file. 需要新的块来扩展这个文件*/
      FileChunk *pNew = sqlite3_malloc(sizeof(FileChunk));
      if( !pNew ){
        return SQLITE_IOERR_NOMEM;
      }
      pNew->pNext = 0;
      if( pChunk ){
        assert( p->pFirst );
        pChunk->pNext = pNew;
      }else{
        assert( !p->pFirst );
        p->pFirst = pNew;
      }
      p->endpoint.pChunk = pNew;
    }

    memcpy(&p->endpoint.pChunk->zChunk[iChunkOffset], zWrite, iSpace);
    zWrite += iSpace;
    nWrite -= iSpace;
    p->endpoint.iOffset += iSpace;
  }

  return SQLITE_OK;
}

/*
** Truncate the file.截断该文件。
*/
static int memjrnlTruncate(sqlite3_file *pJfd, sqlite_int64 size){
  MemJournal *p = (MemJournal *)pJfd;
  FileChunk *pChunk;
  assert(size==0);
  UNUSED_PARAMETER(size);
  pChunk = p->pFirst;
  while( pChunk ){
    FileChunk *pTmp = pChunk;
    pChunk = pChunk->pNext;
    sqlite3_free(pTmp);
  }
  sqlite3MemJournalOpen(pJfd);
  return SQLITE_OK;
}

/*
** Close the file.关闭文件
*/
static int memjrnlClose(sqlite3_file *pJfd){
  memjrnlTruncate(pJfd, 0);
  return SQLITE_OK;
}


/*
** Sync the file.同步文件
**
** Syncing an in-memory journal is a no-op.  And, in fact, this routine
** is never called in a working implementation.  This implementation
** exists purely as a contingency, in case some malfunction in some other
** part of SQLite causes Sync to be called by mistake.
*/
/*同步内存中的日志是一个空操作。事实上，在一个工作执行中是不会调用这个函数文件的。
**这个操作作为应急而存在，以防一些SQLite其他部分的同步故障。
*/
static int memjrnlSync(sqlite3_file *NotUsed, int NotUsed2){
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  return SQLITE_OK;
}

/*
** Query the size of the file in bytes.按字节显示查询文件的大小。
*/
static int memjrnlFileSize(sqlite3_file *pJfd, sqlite_int64 *pSize){
  MemJournal *p = (MemJournal *)pJfd;
  *pSize = (sqlite_int64) p->endpoint.iOffset;
  return SQLITE_OK;
}

/*
** Table of methods for MemJournal sqlite3_file object.
** MemJournal sqlite3_file对象的方法表。
*/
static const struct sqlite3_io_methods MemJournalMethods = {
  1,                /* iVersion */
  memjrnlClose,     /* xClose */
  memjrnlRead,      /* xRead */
  memjrnlWrite,     /* xWrite */
  memjrnlTruncate,  /* xTruncate */
  memjrnlSync,      /* xSync */
  memjrnlFileSize,  /* xFileSize */
  0,                /* xLock */
  0,                /* xUnlock */
  0,                /* xCheckReservedLock */
  0,                /* xFileControl */
  0,                /* xSectorSize */
  0,                /* xDeviceCharacteristics */
  0,                /* xShmMap */
  0,                /* xShmLock */
  0,                /* xShmBarrier */
  0                 /* xShmUnlock */
};

/* 
** Open a journal file.
**打开日志文件。
*/
void sqlite3MemJournalOpen(sqlite3_file *pJfd){
  MemJournal *p = (MemJournal *)pJfd;
  assert( EIGHT_BYTE_ALIGNMENT(p) );
  memset(p, 0, sqlite3MemJournalSize());
  p->pMethod = (sqlite3_io_methods*)&MemJournalMethods;
}

/*
** Return true if the file-handle passed as an argument is 
** an in-memory journal.
**如果文件句柄作为参数传递的是内存中的日志，返回true
*/
int sqlite3IsMemJournal(sqlite3_file *pJfd){
  return pJfd->pMethods==&MemJournalMethods;
}

/* 
** Return the number of bytes required to store a MemJournal file descriptor.
**返回的是存储MemJournal文件描述符所需的字节数。
*/
int sqlite3MemJournalSize(void){
  return sizeof(MemJournal);
}
