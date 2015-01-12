1、打开页面：
pvfs 代表所应用的虚拟文件系统， 
**ppPager代表这里返回页面结构，
zFilename代表所打开的数据库文件名字，
nExtra代表在每一个内存页面上附加的额外的字节，
flags代表控制文件的标志，
vfsFlag 表示通过sqlite3-vfs.XOpen（）的标志，
函数void(*xReinit)(DbPage*)表示重新启动页面功能：
*pPtr
pPager代表页面对象的分配和返回
rc=SQLITE_OK表示返回代码
tempFile表示临时文件为真
memDB表示如果是一个内存文件则为真
readonly表示如果这是一个只读文件则为真
journalFileSize每一个日志文件分配的字节数
zPathname表示数据库文件的全部路径
nPathname表示zPathnamed 比特数
useJournal表示忽略日志失败
pcacheSize表示PCach分配的内存
szPageSize表示默认页面大小
zUri表示拷贝的URI自变量
nUri表示在*zUri的URI自变量的比特数

int sqlite3PagerClose(Pager *pPager);
int sqlite3PagerReadFileheader(Pager*, int, unsigned char*);

/* Functions used to configure a Pager object. */
void sqlite3PagerSetBusyhandler(Pager*, int(*)(void *), void *);
int sqlite3PagerSetPagesize(Pager*, u32*, int);
int sqlite3PagerMaxPageCount(Pager*, int);
void sqlite3PagerSetCachesize(Pager*, int);
void sqlite3PagerShrink(Pager*);
void sqlite3PagerSetSafetyLevel(Pager*,int,int,int);
int sqlite3PagerLockingMode(Pager *, int);
int sqlite3PagerSetJournalMode(Pager *, int);
int sqlite3PagerGetJournalMode(Pager*);
int sqlite3PagerOkToChangeJournalMode(Pager*);
i64 sqlite3PagerJournalSizeLimit(Pager *, i64);
sqlite3_backup **sqlite3PagerBackupPtr(Pager*);

/* Functions used to obtain and release page references. */ 
int sqlite3PagerAcquire(Pager *pPager, Pgno pgno, DbPage **ppPage, int clrFlag);
#define sqlite3PagerGet(A,B,C) sqlite3PagerAcquire(A,B,C,0)
DbPage *sqlite3PagerLookup(Pager *pPager, Pgno pgno);
void sqlite3PagerRef(DbPage*);
void sqlite3PagerUnref(DbPage*);

/* Operations on page references. */
int sqlite3PagerWrite(DbPage*);
void sqlite3PagerDontWrite(DbPage*);
int sqlite3PagerMovepage(Pager*,DbPage*,Pgno,int);
int sqlite3PagerPageRefcount(DbPage*);
void *sqlite3PagerGetData(DbPage *); 
void *sqlite3PagerGetExtra(DbPage *); 

/* Functions used to manage pager transactions and savepoints. */
void sqlite3PagerPagecount(Pager*, int*);
int sqlite3PagerBegin(Pager*, int exFlag, int);
int sqlite3PagerCommitPhaseOne(Pager*,const char *zMaster, int);
int sqlite3PagerExclusiveLock(Pager*);
int sqlite3PagerSync(Pager *pPager);
int sqlite3PagerCommitPhaseTwo(Pager*);
int sqlite3PagerRollback(Pager*);
int sqlite3PagerOpenSavepoint(Pager *pPager, int n);
int sqlite3PagerSavepoint(Pager *pPager, int op, int iSavepoint);
int sqlite3PagerSharedLock(Pager *pPager);

int sqlite3PagerCheckpoint(Pager *pPager, int, int*, int*);
int sqlite3PagerWalSupported(Pager *pPager);
int sqlite3PagerWalCallback(Pager *pPager);
int sqlite3PagerOpenWal(Pager *pPager, int *pisOpen);
int sqlite3PagerCloseWal(Pager *pPager);
#ifdef SQLITE_ENABLE_ZIPVFS
  int sqlite3PagerWalFramesize(Pager *pPager);
#endif

/* Functions used to query pager state and configuration. */
u8 sqlite3PagerIsreadonly(Pager*);
int sqlite3PagerRefcount(Pager*);
int sqlite3PagerMemUsed(Pager*);
const char *sqlite3PagerFilename(Pager*, int);
const sqlite3_vfs *sqlite3PagerVfs(Pager*);
sqlite3_file *sqlite3PagerFile(Pager*);
const char *sqlite3PagerJournalname(Pager*);
int sqlite3PagerNosync(Pager*);
void *sqlite3PagerTempSpace(Pager*);
int sqlite3PagerIsMemdb(Pager*);
void sqlite3PagerCacheStat(Pager *, int, int, int *);
void sqlite3PagerClearCache(Pager *);

/* Functions used to truncate the database file. */
void sqlite3PagerTruncateImage(Pager*,Pgno);

#if defined(SQLITE_HAS_CODEC) && !defined(SQLITE_OMIT_WAL)
void *sqlite3PagerCodec(DbPage *);
#endif

/* Functions to support testing and debugging. */
#if !defined(NDEBUG) || defined(SQLITE_TEST)
  Pgno sqlite3PagerPagenumber(DbPage*);
  int sqlite3PagerIswriteable(DbPage*);
#endif
#ifdef SQLITE_TEST
  int *sqlite3PagerStats(Pager*);
  void sqlite3PagerRefdump(Pager*);
  void disable_simulated_io_errors(void);
  void enable_simulated_io_errors(void);
#else
# define disable_simulated_io_errors()
# define enable_simulated_io_errors()
#endif

#endif /* _PAGER_H_ */

/*
** Allocate and initialize a new Pager object and put a pointer to it
** in *ppPager. The pager should eventually be freed by passing it
** to sqlite3PagerClose().
**
** The zFilename argument is the path to the database file to open.
** If zFilename is NULL then a randomly-named temporary file is created
** and used as the file to be cached. Temporary files are be deleted
** automatically when they are closed. If zFilename is ":memory:" then 
** all information is held in cache. It is never written to disk. 
** This can be used to implement an in-memory database.
**
** The nExtra parameter specifies the number of bytes of space allocated
** along with each page reference. This space is available to the user
** via the sqlite3PagerGetExtra() API.
**
** The flags argument is used to specify properties that affect the
** operation of the pager. It should be passed some bitwise combination
** of the PAGER_* flags.
**
** The vfsFlags parameter is a bitmask to pass to the flags parameter
** of the xOpen() method of the supplied VFS when opening files. 
**
** If the pager object is allocated and the specified file opened 
** successfully, SQLITE_OK is returned and *ppPager set to point to
** the new pager object. If an error occurs, *ppPager is set to NULL
** and error code returned. This function may return SQLITE_NOMEM
** (sqlite3Malloc() is used to allocate memory), SQLITE_CANTOPEN or 
** various SQLITE_IO_XXX errors.
*/
//分配并且初始化一个新页面对象，然后又放置一个指向它的指针在*ppPager中。这个页面应该最终被sqlite3PagerClose（）释放。
//文件名参数是打开数据库文件的路径。如果文件名为空，那就会建立一个随机名字的临时文件并且用作缓存的文件。临时文件将在关闭的时候自动的删除。如果文件名是“：memory”，那么所有信息都被存储在缓存中。并且永远不写进存储。它将用来实施一个内存数据库。
//nExtra参数详细说明了每个引用到的页面分配的指定空间字节数。这个空间通过sqlite3PagerGetExtra（）API是对用户可用的。
//标志参数是被用来详细说明了能影响页面的操作的性能。这应该通过一些PAGER_*flag的按位组合。
//vsfFlags参数是按位来打开文件时通过提供VFS的xOpen（）方式的标志参数。
//如果页面对象是已分配并且详细说明过的成功打开的文件，SQLITE_OK将返回*ppPager指向新页面对象的设置。如果一个错误发生，*ppPager将设为空并且错误代码会返回。这个函数可能返回SQLITE_NOMEN（sqlite3Malloc（）是用来分配存储的），SQLITE_CANTOPEN或者多种SQLITE_IO_XXX之类的错误。
int sqlite3PagerOpen(
  sqlite3_vfs *pVfs,       /* The virtual file system to use */
  Pager **ppPager,         /* OUT: Return the Pager structure here */
  const char *zFilename,   /* Name of the database file to open */
  int nExtra,              /* Extra bytes append to each in-memory page */
  int flags,               /* flags controlling this file */
  int vfsFlags,            /* flags passed through to sqlite3_vfs.xOpen() */
  sqlite3_vfs*,//pvfs  所应用的虚拟文件系统             
  Pager **ppPager,//out：这里返回页面结构
  const char*,// zFilename //所打开的数据库文件名字
  int,//nExtra 在每一个内存页面上附加的额外的字节
  int,//flags 控制文件的标志
  int,//vfsFlag 通过sqlite3-vfs.XOpen（）的标志

  void (*xReinit)(DbPage*) /* Function to reinitialize pages */
){//重启页面的函数
  u8 *pPtr;
  Pager *pPager = 0;       /* Pager object to allocate and return */
  int rc = SQLITE_OK;      /* Return code */
  int tempFile = 0;        /* True for temp files (incl. in-memory files) */
  int memDb = 0;           /* True if this is an in-memory file */
  int readOnly = 0;        /* True if this is a read-only file */
  int journalFileSize;     /* Bytes to allocate for each journal fd */
  char *zPathname = 0;     /* Full path to database file */
  int nPathname = 0;       /* Number of bytes in zPathname */
  int useJournal = (flags & PAGER_OMIT_JOURNAL)==0; /* False to omit journal */
  int pcacheSize = sqlite3PcacheSize();       /* Bytes to allocate for PCache */
  u32 szPageDflt = SQLITE_DEFAULT_PAGE_SIZE;  /* Default page size */
  const char *zUri = 0;    /* URI args to copy */
  int nUri = 0;            /* Number of bytes of URI args at *zUri */
页面对象的分配和返回
返回代码
临时文件为真
如果是一个内存文件则为真
如果这是一个只读文件则为真
每一个日志文件分配的字节数
zPathname：数据库文件的完整路径
zPathnamed的比特数
忽略日志失败
PCach分配的比特数大小
默认页面大小
拷贝的URI自变量
在*zUri的URI自变量的比特数

  /* Figure out how much space is required for each journal file-handle
  ** (there are two of them, the main journal and the sub-journal). This
  ** is the maximum space required for an in-memory journal file handle 
  ** and a regular journal file-handle. Note that a "regular journal-handle"
  ** may be a wrapper capable of caching the first portion of the journal
  ** file in memory to implement the atomic-write optimization (see 
  ** source file journal.c).
  */
//算出对于每个日志文件操作需要多少空间（这里有两个，是主要日志和子日志）。这是对于一个内存日志文件操作和一个规则的日志文件操作所需要的最大空间和常规空间。注意一个“规则日志文件操作”可能是一个缓冲在存储中的第一部分日志文件的包装能力，用来实施自动写最优化（看源文件journal.c）

  if( sqlite3JournalSize(pVfs)>sqlite3MemJournalSize() ){
    journalFileSize = ROUND8(sqlite3JournalSize(pVfs));
  }else{
    journalFileSize = ROUND8(sqlite3MemJournalSize());
  }
如果日志大小（虚拟文件系统pVfs）>存储日志大小，那么日志文件大小=round8（日志大小）；反之，则等于参数换为存储日志大小。
（总之，日志文件大小=ROUND8（日志大小和存储日志大小比较大的一个））

/* Set the output variable to NULL in case an error occurs. */
  *ppPager = 0;
//万一错误发生的话，设置输出变量为0
所返回的页面结构（*ppPager）为0

#ifndef SQLITE_OMIT_MEMORYDB
  if( flags & PAGER_MEMORY ){
    memDb = 1;
    if( zFilename && zFilename[0] ){
      zPathname = sqlite3DbStrDup(0, zFilename);
      if( zPathname==0  ) return SQLITE_NOMEM;
      nPathname = sqlite3Strlen30(zPathname);
      zFilename = 0;
    }
  }
#endif
如果没有定义SQLITE-OMIT-MEMORYDB（忽略存储数据库）
    如果（标志和页面内存均为真），则memDb=1（是内存文件）
        如果数据库文件路径存在并且起码有一个，则赋值zPathname=sqlite3DbStrDup（0，zFilename）；
            如果zPathname为0的时候，返回SQLITE_NOMEM；
            nPathname（路径数）=sqlite3Strlen30（zPathname）（统计路径数函数）；
            路径名=0；

  /* Compute and store the full pathname in an allocated buffer pointed
  ** to by zPathname, length nPathname. Or, if this is a temporary file,
  ** leave both nPathname and zPathname set to 0.
  */
//计算和存储完整路径名在分配在 通过路径名,路径名长度 指向的缓冲区。或者,如果这是一个临时文件,把路径长度和路径设置为0。

  if( zFilename && zFilename[0] ){
    const char *z;
    nPathname = pVfs->mxPathname+1;
    zPathname = sqlite3DbMallocRaw(0, nPathname*2);
    if( zPathname==0 ){
      return SQLITE_NOMEM;
    }
    zPathname[0] = 0; /* Make sure initialized even if FullPathname() fails */
    rc = sqlite3OsFullPathname(pVfs, zFilename, nPathname, zPathname);
    nPathname = sqlite3Strlen30(zPathname);
    z = zUri = &zFilename[sqlite3Strlen30(zFilename)+1];
    while( *z ){
      z += sqlite3Strlen30(z)+1;
      z += sqlite3Strlen30(z)+1;
    }
    nUri = (int)(&z[1] - zUri);
    assert( nUri>=0 );
    if( rc==SQLITE_OK && nPathname+8>pVfs->mxPathname ){
      /* This branch is taken when the journal path required by
      ** the database being opened will be more than pVfs->mxPathname
      ** bytes in length. This means the database cannot be opened,
      ** as it will not be possible to open the journal file or even
      ** check for a hot-journal before reading.
      */
      rc = SQLITE_CANTOPEN_BKPT;
    }
    if( rc!=SQLITE_OK ){
      sqlite3DbFree(0, zPathname);
      return rc;
    }
  }
如果（文件名&文件名[0]）
    路径长度=虚拟系统的最大路径长度+1
    路径名=分配长度为（文件名*2）的名字
    如果（路径名==0） 返回SQLITE_NOMEM
    路径名[0]=0；//确保初始化，防止下句语句失效
    返回代码判定rc=sqlite3OsFullPathname（正在用的虚拟文件系统，正打开的文件名，路径名长度，全部数据库文件路径）；
    文件名比特数=sqlite3Strlen30（文件名）；
    z（不变的char）=zUri（复制的URI自变量）=打开数据库文件的名字（sqlite3Strlen30（打开的数据库文件的名字）+1）的地址
    当（恒定的z有指向时）{
        z=z+sqlite3Strlen30（z）+1；
        z=z+sqlite3Strlen30（z）+1；（为什么会有两个增加啊？直接加2不行吗？）
    }
    在*zUri（复制的URI自变量）中URI的比特数nUri=（强制转换类型到整形）（z的地址的第一个字节-复制的URI自变量）；
    assert（URI的自变量数>=0）；（检测括号里的布尔表达式是否为真）
     if( rc==SQLITE_OK && nPathname+8>pVfs->mxPathname )（如果返回代码为真并且路径数+8>现在正在运行的虚拟系统所指向的最长路径名）{
        //当日志路径被正在打开的数据库需要，这个分支是被用的，这将超过现在所运行的虚拟数据库所指的 - > mxPathname的字节长度。这意味着数据库不能打开,因为它将不可能打开日志文件,甚至不能在阅读之前检查一个热门日志。
        返回代码rc=SQLITE_CANTOPEN_BKPT（能打开）
    }
    如果（rc！=SQLITE_OK）{
    sqlite3DbFree（0，数据库文件的全部路径名）；
    返回rc；
    }
}
（不需要endif吗？）

  /* Allocate memory for the Pager structure, PCache object, the
  ** three file descriptors, the database file name and the journal 
  ** file name. The layout in memory is as follows:
  **
  **     Pager object                    (sizeof(Pager) bytes)
  **     PCache object                   (sqlite3PcacheSize() bytes)
  **     Database file handle            (pVfs->szOsFile bytes)
  **     Sub-journal file handle         (journalFileSize bytes)
  **     Main journal file handle        (journalFileSize bytes)
  **     Database file name              (nPathname+1 bytes)
  **     Journal file name               (nPathname+8+1 bytes)
  */
//对于页面结构分配存储，页面存储对象，三个文件描述符，数据库文件名和日志文件名。在存储中的设计如下所示：
页面对象                    Pager大小
页面缓存对象             sqlite3PcacheSize（）分配的缓存空间
数据库文件操作         正在运行的虚拟数据库所指向的szOsFile（操作系统文件是不是）
子日志文件操作         日志文件大小
主日志文件操作         日志文件大小
数据库文件名             路径名数目+1
日志文件名                路径名数目+8+1（为啥加8？8位的意思吗？）

  pPtr = (u8 *)sqlite3MallocZero(
    ROUND8(sizeof(*pPager)) +      /* Pager structure */
    ROUND8(pcacheSize) +           /* PCache object */
    ROUND8(pVfs->szOsFile) +       /* The main db file */
    journalFileSize * 2 +          /* The two journal files */ 
    nPathname + 1 + nUri +         /* zFilename */
    nPathname + 8 + 2              /* zJournal */
#ifndef SQLITE_OMIT_WAL
    + nPathname + 4 + 2            /* zWal */
#endif
  );
  assert( EIGHT_BYTE_ALIGNMENT(SQLITE_INT_TO_PTR(journalFileSize)) );
  if( !pPtr ){
    sqlite3DbFree(0, zPathname);
    return SQLITE_NOMEM;
  }
pPtr=（u8的强制类型转换）sqlite3MallocZero分配大小（页面结构+缓存页面对象+主要的db文件+两个日志文件+文件名+路径名（定义忽略与写日志就加8，没有定义就加4））
判定（八字节队列（int类型转换到ptr类型（日志文件大小）））；
如果（pPtr不存在）{
sqlite3DbFree（0，路径名）；（释放路径名）
返回SQLITE_NOMEM
}

  pPager =              (Pager*)(pPtr);
  pPager->pPCache =    (PCache*)(pPtr += ROUND8(sizeof(*pPager)));
  pPager->fd =   (sqlite3_file*)(pPtr += ROUND8(pcacheSize));
  pPager->sjfd = (sqlite3_file*)(pPtr += ROUND8(pVfs->szOsFile));
  pPager->jfd =  (sqlite3_file*)(pPtr += journalFileSize);
  pPager->zFilename =    (char*)(pPtr += journalFileSize);
  assert( EIGHT_BYTE_ALIGNMENT(pPager->jfd) );
pPager是Pager类型的指针；
他指向的pPCache缓存页面；
fd缓存页面大小文件；
sjfd现行虚拟数据库的日志文件大小的文件
zFilename日志文件的大小字节数
判定（八字节队列（他指向的日志文件大小））；

  /* Fill in the Pager.zFilename and Pager.zJournal buffers, if required. */
  if( zPathname ){
    assert( nPathname>0 );
    pPager->zJournal =   (char*)(pPtr += nPathname + 1 + nUri);
    memcpy(pPager->zFilename, zPathname, nPathname);
    if( nUri ) memcpy(&pPager->zFilename[nPathname+1], zUri, nUri);
    memcpy(pPager->zJournal, zPathname, nPathname);
    memcpy(&pPager->zJournal[nPathname], "-journal\000", 8+1);
    sqlite3FileSuffix3(pPager->zFilename, pPager->zJournal);
#ifndef SQLITE_OMIT_WAL
    pPager->zWal = &pPager->zJournal[nPathname+8+1];
    memcpy(pPager->zWal, zPathname, nPathname);
    memcpy(&pPager->zWal[nPathname], "-wal\000", 4+1);
    sqlite3FileSuffix3(pPager->zFilename, pPager->zWal);
#endif
    sqlite3DbFree(0, zPathname);
  }
  pPager->pVfs = pVfs;
  pPager->vfsFlags = vfsFlags;
//如果要求的话，完成页面文件名和日志缓存。
如果（路径名）{
    判定（路径名长度>0）；
    页面的日志= (char*)(pPtr += nPathname + 1 + nUri);
    字符串拷贝（页面的文件名，路径名，路径名大小）；
    如果（nUri存在），那么：字符串拷贝（页面的文件名的地址，zUri，nUri）；
    字符串拷贝（页面的日志文件，路径名，路径名大小）；
    字符串拷贝（页面的日志文件【路径名】，"-journal\000", 8+1）；
    sqlite3FileSuffix3（页面指向的文件名，页面指向的日志名）
    如果没有定义SQLITE_OMIT_WAL
        pPager指向的zWal=……日志文件……
        字符串拷贝（zWal+路径名+路经长度）；
        字符串拷贝（zWal地址+地址+4,1）；
    释放路径名
}
指向虚拟数据库系统
指向虚拟数据库标志

  /* Open the pager file.
  */
  if( zFilename && zFilename[0] ){
    int fout = 0;                    /* VFS flags returned by xOpen() */
    rc = sqlite3OsOpen(pVfs, pPager->zFilename, pPager->fd, vfsFlags, &fout);
    assert( !memDb );
    readOnly = (fout&SQLITE_OPEN_READONLY);
//打开一个页面文件
如果（文件名和文件名【0】均存在）{
   int fout = 0; 虚拟文件系统通过xOpen() 返回
   rc = sqlite3OsOpen(pVfs, pPager->zFilename, pPager->fd, vfsFlags, &fout);返回代码=操作系统打开虚拟操作系统中的，文件中的，页面中的标志
检验（memDb不存在）；
只读文件（标志和打开只读文件均为真）；
}

    /* If the file was successfully opened for read/write access,
    ** choose a default page size in case we have to create the
    ** database file. The default page size is the maximum of:
    **
    **    + SQLITE_DEFAULT_PAGE_SIZE,
    **    + The value returned by sqlite3OsSectorSize()
    **    + The largest page size that can be written atomically.
    */
//如果文件是成功的用于打开后的读和写进程，如果我们创建一个数据库文件就选择一个默认页面大小。默认页面大小是以下值的最大值：默认页面大小+操作部分大小的返回值+可以被自动写的最大页面大小。
    if( rc==SQLITE_OK && !readOnly ){
      setSectorSize(pPager);
 assert(SQLITE_DEFAULT_PAGE_SIZE<=SQLITE_MAX_DEFAULT_PAGE_SIZE);
      if( szPageDflt<pPager->sectorSize ){
        if( pPager->sectorSize>SQLITE_MAX_DEFAULT_PAGE_SIZE ){
          szPageDflt = SQLITE_MAX_DEFAULT_PAGE_SIZE;
        }else{
          szPageDflt = (u32)pPager->sectorSize;
        }
      }
#ifdef SQLITE_ENABLE_ATOMIC_WRITE
      {
        int iDc = sqlite3OsDeviceCharacteristics(pPager->fd);
        int ii;
        assert(SQLITE_IOCAP_ATOMIC512==(512>>8));
        assert(SQLITE_IOCAP_ATOMIC64K==(65536>>8));
        assert(SQLITE_MAX_DEFAULT_PAGE_SIZE<=65536);
        for(ii=szPageDflt; ii<=SQLITE_MAX_DEFAULT_PAGE_SIZE; ii=ii*2){
          if( iDc&(SQLITE_IOCAP_ATOMIC|(ii>>8)) ){
            szPageDflt = ii;
          }
        }
      }
#endif
    }
  }
如果返回代码判定==SQLITE_OK && !readOnly{
    设置区域大小（页面）；
    判定（默认页面大小<=最大默认页面大小）；
    如果（szPageDflt<pPager->sectorSize ）{
        如果（页面区域大小>默认最大页面大小）{
            szPageDflt = SQLITE_MAX_DEFAULT_PAGE_SIZE
        }否则，等于另一个（哪个大等于哪个）
    }
    如果定义了SQLITE_ENABLE_ATOMIC_WRITE{
         int iDc = sqlite3OsDeviceCharacteristics(pPager->fd);
        int ii;
        assert(SQLITE_IOCAP_ATOMIC512==(512>>8));
        assert(SQLITE_IOCAP_ATOMIC64K==(65536>>8));
        assert(SQLITE_MAX_DEFAULT_PAGE_SIZE<=65536);
        for(ii=szPageDflt; ii<=SQLITE_MAX_DEFAULT_PAGE_SIZE; ii=ii*2){
          if( iDc&(SQLITE_IOCAP_ATOMIC|(ii>>8)) ){
            szPageDflt = ii;
            }
        }
    }
}
else{
    /* If a temporary file is requested, it is not opened immediately.
    ** In this case we accept the default page size and delay actually
    ** opening the file until the first call to OsWrite().
    **
    ** This branch is also run for an in-memory database. An in-memory
    ** database is the same as a temp-file that is never written out to
    ** disk and uses an in-memory rollback journal.
    */ 
//如果一个临时文件被请求，这不是被立即打开的。在这个例子中，我们接受默认页面大小并且拖延直到第一个调用OsWrite()才实际打开文件。
//这个分支也对于一个内存数据库运行。一个内存数据库是和临时文件一样的绝不会写出磁盘并且运用一个内存回滚日志。
    tempFile = 1;
    pPager->eState = PAGER_READER;
    pPager->eLock = EXCLUSIVE_LOCK;
    readOnly = (vfsFlags&SQLITE_OPEN_READONLY);
  }
临时文件赋值1；
状态赋值已读；
锁状态赋值锁；
只读赋值标志&&打开只读

  /* The following call to PagerSetPagesize() serves to set the value of 
  ** Pager.pageSize and to allocate the Pager.pTmpSpace buffer.
  */
//下面调用PagerSetPagesize()服务来设置Pager.pageSize值并且分配Pager.pTmpSpace缓存

  if( rc==SQLITE_OK ){
    assert( pPager->memDb==0 );
    rc = sqlite3PagerSetPagesize(pPager, &szPageDflt, -1);
    testcase( rc!=SQLITE_OK );
  }
如果（返回代码==SQLITE_OK）{
判定（页面指向memDb==0）；
返回代码=设置页面大小（页面，页面大小地址，-1）；
测试例子（返回代码！=SQLITE_OK）；
}

  /* If an error occurred in either of the blocks above, free the 
  ** Pager structure and close the file.
  */
//如果一个错误发生在以上任何一个模块，释放页面结构并且关闭文件。
  if( rc!=SQLITE_OK ){
    assert( !pPager->pTmpSpace );
    sqlite3OsClose(pPager->fd);
    sqlite3_free(pPager);
    return rc;
  }
如果（rc不等于SQLITE_OK）{
那么判定（页面不指向页面临时空间）；
关闭指向的文件
释放页面
返回rc
}

  /* Initialize the PCache object. */
//初始化页面缓存对象
  assert( nExtra<1000 );
  nExtra = ROUND8(nExtra);
  sqlite3PcacheOpen(szPageDflt, nExtra, !memDb,
                    !memDb?pagerStress:0, (void *)pPager, pPager->pPCache);

  PAGERTRACE(("OPEN %d %s\n", FILEHANDLEID(pPager->fd), pPager->zFilename));
  IOTRACE(("OPEN %p %s\n", pPager, pPager->zFilename))

  pPager->useJournal = (u8)useJournal;
  /* pPager->stmtOpen = 0; */
  /* pPager->stmtInUse = 0; */
  /* pPager->nRef = 0; */
  /* pPager->stmtSize = 0; */
  /* pPager->stmtJSize = 0; */
  /* pPager->nPage = 0; */
  pPager->mxPgno = SQLITE_MAX_PAGE_COUNT;
  /* pPager->state = PAGER_UNLOCK; */
#if 0
  assert( pPager->state == (tempFile ? PAGER_EXCLUSIVE : PAGER_UNLOCK) );
#endif
  /* pPager->errMask = 0; */
  pPager->tempFile = (u8)tempFile;
  assert( tempFile==PAGER_LOCKINGMODE_NORMAL 
          || tempFile==PAGER_LOCKINGMODE_EXCLUSIVE );
  assert( PAGER_LOCKINGMODE_EXCLUSIVE==1 );
  pPager->exclusiveMode = (u8)tempFile; 
  pPager->changeCountDone = pPager->tempFile;
  pPager->memDb = (u8)memDb;
  pPager->readOnly = (u8)readOnly;
  assert( useJournal || pPager->tempFile );
  pPager->noSync = pPager->tempFile;
  if( pPager->noSync ){
    assert( pPager->fullSync==0 );
    assert( pPager->syncFlags==0 );
    assert( pPager->walSyncFlags==0 );
    assert( pPager->ckptSyncFlags==0 );
  }else{
    pPager->fullSync = 1;
    pPager->syncFlags = SQLITE_SYNC_NORMAL;
    pPager->walSyncFlags = SQLITE_SYNC_NORMAL | WAL_SYNC_TRANSACTIONS;
    pPager->ckptSyncFlags = SQLITE_SYNC_NORMAL;
  }
  /* pPager->pFirst = 0; */
  /* pPager->pFirstSynced = 0; */
  /* pPager->pLast = 0; */
  pPager->nExtra = (u16)nExtra;
  pPager->journalSizeLimit = SQLITE_DEFAULT_JOURNAL_SIZE_LIMIT;
  assert( isOpen(pPager->fd) || tempFile );
  setSectorSize(pPager);
  if( !useJournal ){
    pPager->journalMode = PAGER_JOURNALMODE_OFF;
  }else if( memDb ){
    pPager->journalMode = PAGER_JOURNALMODE_MEMORY;
  }
  /* pPager->xBusyHandler = 0; */
  /* pPager->pBusyHandlerArg = 0; */
  pPager->xReiniter = xReinit;
  /* memset(pPager->aHash, 0, sizeof(pPager->aHash)); */

  *ppPager = pPager;
  return SQLITE_OK;
}


/*
** Shutdown the page cache.  Free all memory and close all files.
**
** If a transaction was in progress when this routine is called, that
** transaction is rolled back.  All outstanding pages are invalidated
** and their memory is freed.  Any attempt to use a page associated
** with this page cache after this function returns will likely
** result in a coredump.
**
** This function always succeeds. If a transaction is active an attempt
** is made to roll it back. If an error occurs during the rollback 
** a hot journal may be left in the filesystem but no error is returned
** to the caller.
*/
//关闭页面缓存。释放所有内存和关闭所有文件。如果当调用这个例程时候，事务是在运行,那么事务回滚。所有未处理的页面是无效的并且他的内存被释放。任何试图用这个页面与缓存页面关联的页面，在这个函数返回后可能会导致coredump（貌似是垃圾堆的意思）。
//这个函数总是成功。如果一个事务激活了一个临时来回滚该事务。如果一个错误发生在一个热门日志的回滚期间，可能留在一个文件系统中而没有错误返回到调用中。

int sqlite3PagerClose(Pager *pPager){
  u8 *pTmp = (u8 *)pPager->pTmpSpace;

  assert( assert_pager_state(pPager) );
  disable_simulated_io_errors();
  sqlite3BeginBenignMalloc();
  /* pPager->errCode = 0; */
  pPager->exclusiveMode = 0;
#ifndef SQLITE_OMIT_WAL
  sqlite3WalClose(pPager->pWal, pPager->ckptSyncFlags, pPager->pageSize, pTmp);
  pPager->pWal = 0;
#endif
  pager_reset(pPager);
  if( MEMDB ){
    pager_unlock(pPager);
  }else{
    /* If it is open, sync the journal file before calling UnlockAndRollback.
    ** If this is not done, then an unsynced portion of the open journal 
    ** file may be played back into the database. If a power failure occurs 
    ** while this is happening, the database could become corrupt.
    **
    ** If an error occurs while trying to sync the journal, shift the pager
    ** into the ERROR state. This causes UnlockAndRollback to unlock the
    ** database and close the journal file without attempting to roll it
    ** back or finalize it. The next database user will have to do hot-journal
    ** rollback before accessing the database file.
    */
//如果它是打开状态，日志文件在调用 UnlockAndRollback（非锁并且回滚） 之前是同步的。如果这没有被完成，那么一个开放的日志文件的不同步的部分可能被运行返回到数据库中。如果在运行当中发生电力故障，数据库可能成为corrupt（腐败，可能是不好的意思）。
//如果当尝试同步日志的时候一个错误发生了，转换页面进入错误状态。着导致了 UnlockAndRollback（非锁并且回滚） 来解锁这个数据库并且不尝试回滚或者完成它来直接关闭日志文件。
    if( isOpen(pPager->jfd) ){
      pager_error(pPager, pagerSyncHotJournal(pPager));
    }
    pagerUnlockAndRollback(pPager);
  }
  sqlite3EndBenignMalloc();
  enable_simulated_io_errors();
  PAGERTRACE(("CLOSE %d\n", PAGERID(pPager)));
  IOTRACE(("CLOSE %p\n", pPager))
  sqlite3OsClose(pPager->jfd);
  sqlite3OsClose(pPager->fd);
  sqlite3PageFree(pTmp);
  sqlite3PcacheClose(pPager->pPCache);

#ifdef SQLITE_HAS_CODEC
  if( pPager->xCodecFree ) pPager->xCodecFree(pPager->pCodec);
#endif

  assert( !pPager->aSavepoint && !pPager->pInJournal );
  assert( !isOpen(pPager->jfd) && !isOpen(pPager->sjfd) );

  sqlite3_free(pPager);
  return SQLITE_OK;
}
/*
** Read the first N bytes from the beginning of the file into memory
** that pDest points to. 
**
** If the pager was opened on a transient file (zFilename==""), or
** opened on a file less than N bytes in size, the output buffer is
** zeroed and SQLITE_OK returned. The rationale for this is that this 
** function is used to read database headers, and a new transient or
** zero sized database has a header than consists entirely of zeroes.
**
** If any IO error apart from SQLITE_IOERR_SHORT_READ is encountered,
** the error code is returned to the caller and the contents of the
** output buffer undefined.
*/
//从文件的最开始读取前N个字节放入pDest所指向的存储中。
如果页面打开一个临时文件(zFilename = = " "),或打开一个文件小于N个字节大小,输出缓冲区赋值为零并且返回SQLITE_OK。这个基本原理是函数是用于读取数据库标题,并且一个新的事务或零大小的数据库有标题而不是完全由零构成。
//如果遇到除了SQLITE_IOERR_SHORT_READ任何IO错误,错误代码返回给调用者并且输出缓冲区的内容未定义。
int sqlite3PagerReadFileheader(Pager *pPager, int N, unsigned char *pDest){
  int rc = SQLITE_OK;
  memset(pDest, 0, N);
  assert( isOpen(pPager->fd) || pPager->tempFile );

  /* This routine is only called by btree immediately after creating
  ** the Pager object.  There has not been an opportunity to transition
  ** to WAL mode yet.
  */
这个例程只在创建一个页面对象之后，被btree立即调用。一直没有机会转换WAL模式。（与delete模式相比，WAL模式在大部分情况下更快，并发性更好，读和写之间互不阻塞）
  assert( !pagerUseWal(pPager) );

  if( isOpen(pPager->fd) ){
    IOTRACE(("DBHDR %p 0 %d\n", pPager, N))
    rc = sqlite3OsRead(pPager->fd, pDest, N, 0);
    if( rc==SQLITE_IOERR_SHORT_READ ){
      rc = SQLITE_OK;
    }
  }
  return rc;
}
/*
** Set the busy handler function.
**
** The pager invokes the busy-handler if sqlite3OsLock() returns 
** SQLITE_BUSY when trying to upgrade from no-lock to a SHARED lock,
** or when trying to upgrade from a RESERVED lock to an EXCLUSIVE 
** lock. It does *not* invoke the busy handler when upgrading from
** SHARED to RESERVED, or when upgrading from SHARED to EXCLUSIVE
** (which occurs during hot-journal rollback). Summary:
**
**   Transition                        | Invokes xBusyHandler
**   --------------------------------------------------------
**   NO_LOCK       -> SHARED_LOCK      | Yes
**   SHARED_LOCK   -> RESERVED_LOCK    | No
**   SHARED_LOCK   -> EXCLUSIVE_LOCK   | No
**   RESERVED_LOCK -> EXCLUSIVE_LOCK   | Yes
**
** If the busy-handler callback returns non-zero, the lock is 
** retried. If it returns zero, then the SQLITE_BUSY error is
** returned to the caller of the pager API function.
*/
//设置忙时操作函数，如果当试图从一个保留锁升级成一个共享锁时，sqlite3OsLock()返回SQLITE_BUSY。当从共享锁升级成保留锁的时候，或者当从共享锁升级成排他锁的时候，不调用忙操作。（发生在热门日志回滚时候）总结：

//**   过渡                         | 调用 xBusyHandler
**   --------------------------------------------------------
**   不锁       ->共享锁      | Yes
**   共享锁   -> 保留锁    | No
**   共享锁   -> 排他锁   | No
**   保留锁 -> 排他锁   | Yes

//如果忙操作回调返回非零,锁重试。如果它返回0,然后SQLITE_BUSY错误返回给页面API功能的调用者

void sqlite3PagerSetBusyhandler(
  Pager *pPager,                       /* Pager object */
  int (*xBusyHandler)(void *),         /* Pointer to busy-handler function */
  void *pBusyHandlerArg                /* Argument to pass to xBusyHandler */
){  
  pPager->xBusyHandler = xBusyHandler;
  pPager->pBusyHandlerArg = pBusyHandlerArg;
}

/*
** Change the page size used by the Pager object. The new page size 
** is passed in *pPageSize.
**
** If the pager is in the error state when this function is called, it
** is a no-op. The value returned is the error state error code (i.e. 
** one of SQLITE_IOERR, an SQLITE_IOERR_xxx sub-code or SQLITE_FULL).
**
** Otherwise, if all of the following are true:
**
**   * the new page size (value of *pPageSize) is valid (a power 
**     of two between 512 and SQLITE_MAX_PAGE_SIZE, inclusive), and
**
**   * there are no outstanding page references, and
**
**   * the database is either not an in-memory database or it is
**     an in-memory database that currently consists of zero pages.
**
** then the pager object page size is set to *pPageSize.
**
** If the page size is changed, then this function uses sqlite3PagerMalloc() 
** to obtain a new Pager.pTmpSpace buffer. If this allocation attempt 
** fails, SQLITE_NOMEM is returned and the page size remains unchanged. 
** In all other cases, SQLITE_OK is returned.
**
** If the page size is not changed, either because one of the enumerated
** conditions above is not true, the pager was in error state when this
** function was called, or because the memory allocation attempt failed, 
** then *pPageSize is set to the old, retained page size before returning.
*/
//改变页面对象所用的页面大小。新页面大小在*pPageSize中传递。

//如果页面在被函数调用时候处于错误状态，这是一个无操作的。返回值是错误状态的错误代码（也就是SQLITE_IOERR,SQLITE_IOERR_xxx 子代码或者SQLITE_FULL中的一个）

//另外的话，如果如下所有都是真的：
*新页面大小（*pPageSize的值）是可用的（介于512和SQLITE_MAX_PAGE_SIZE中的一个数字），并且
*这里没有未处理的页面引用，并且
*数据库是既没有内存数据库，或者他是一个内存数据库而当前由零页面构成。

//接下来页面对象页面大小是在*pPageSize中设置的

//如果页面大小改变了，那么这个功能用sqlite3PagerMalloc() 来保持一个新Pager.pTmpSpace缓存。如果这个分配尝试失败了，SQLITE_NOMEM被返回并且页面大小保持不变。在所有其他情况中，SQLITE_OK被返回。

//如果页面大小没有被改变，无论是因为上面的任意一个枚举类型条件是不为真的，当这个函数被调用的时候，这个页面是在错误的状态；还是因为存储分配尝试失败，然后*pPageSize设置为旧的，在返回前保持页面大小。
int sqlite3PagerSetPagesize(Pager *pPager, u32 *pPageSize, int nReserve){
  int rc = SQLITE_OK;

  /* It is not possible to do a full assert_pager_state() here, as this
  ** function may be called from within PagerOpen(), before the state
  ** of the Pager object is internally consistent.
  **
  ** At one point this function returned an error if the pager was in 
  ** PAGER_ERROR state. But since PAGER_ERROR state guarantees that
  ** there is at least one outstanding page reference, this function
  ** is a no-op for that case anyhow.
  */
//不可能来做一个完全的assert_pager_state()在这里，因为这个函数可能是从PagerOpen()内部调用出来的，在页面对象的状态内部的始终如一之前。

//在一点上来说，函数返回一个错误，如果页面在PAGER_ERROR的状态上。但是因为PAGER_ERROR状态保证这里至少有一个未处理页面引用，这个函数对于这个事件无论如何是一个无操作的。
  u32 pageSize = *pPageSize;
  assert( pageSize==0 || (pageSize>=512 && pageSize<=SQLITE_MAX_PAGE_SIZE) );
  if( (pPager->memDb==0 || pPager->dbSize==0)
   && sqlite3PcacheRefCount(pPager->pPCache)==0 
   && pageSize && pageSize!=(u32)pPager->pageSize 
  ){
    char *pNew = NULL;             /* New temp space */
    i64 nByte = 0;

    if( pPager->eState>PAGER_OPEN && isOpen(pPager->fd) ){
      rc = sqlite3OsFileSize(pPager->fd, &nByte);
    }
    if( rc==SQLITE_OK ){
      pNew = (char *)sqlite3PageMalloc(pageSize);
      if( !pNew ) rc = SQLITE_NOMEM;
    }

    if( rc==SQLITE_OK ){
      pager_reset(pPager);
      pPager->dbSize = (Pgno)((nByte+pageSize-1)/pageSize);
      pPager->pageSize = pageSize;
      sqlite3PageFree(pPager->pTmpSpace);
      pPager->pTmpSpace = pNew;
      sqlite3PcacheSetPageSize(pPager->pPCache, pageSize);
    }
  }

  *pPageSize = pPager->pageSize;
  if( rc==SQLITE_OK ){
    if( nReserve<0 ) nReserve = pPager->nReserve;
    assert( nReserve>=0 && nReserve<1000 );
    pPager->nReserve = (i16)nReserve;
    pagerReportSize(pPager);
  }
  return rc;
}

/*
** Attempt to set the maximum database page count if mxPage is positive. 
** Make no changes if mxPage is zero or negative.  And never reduce the
** maximum page count below the current size of the database.
**
** Regardless of mxPage, return the current maximum page count.
*/
//尝试设置数据库页面计算最大值，如果mxPage是正的。
如果mxPage为零或者为负，那就毫无改变。并且永远不要减少页面累计数的最大值，让他低于当前数据库大小。
//无视mxPage，返回当前页面计数的最大值
int sqlite3PagerMaxPageCount(Pager *pPager, int mxPage){
  if( mxPage>0 ){
    pPager->mxPgno = mxPage;
  }
  assert( pPager->eState!=PAGER_OPEN );      /* Called only by OP_MaxPgcnt */
  assert( pPager->mxPgno>=pPager->dbSize );  /* OP_MaxPgcnt enforces this */
  return pPager->mxPgno;
}

/*
** Change the maximum number of in-memory pages that are allowed.
*/
//改变所允许的内存页面的最大页数
void sqlite3PagerSetCachesize(Pager *pPager, int mxPage){
  sqlite3PcacheSetCachesize(pPager->pPCache, mxPage);
}

/*
** Free as much memory as possible from the pager.
*/
//从页面中尽可能的释放存储
void sqlite3PagerShrink(Pager *pPager){
  sqlite3PcacheShrink(pPager->pPCache);
}
void sqlite3PagerSetSafetyLevel(
//对页面设置安全等级
  Pager *pPager,        /* The pager to set safety level for */
  int level,            /* PRAGMA synchronous.  1=OFF, 2=NORMAL, 3=FULL */  
  int bFullFsync,       /* PRAGMA fullfsync */
  int bCkptFullFsync    /* PRAGMA checkpoint_fullfsync */
){
  assert( level>=1 && level<=3 );
  pPager->noSync =  (level==1 || pPager->tempFile) ?1:0;
  pPager->fullSync = (level==3 && !pPager->tempFile) ?1:0;
  if( pPager->noSync ){
    pPager->syncFlags = 0;
    pPager->ckptSyncFlags = 0;
  }else if( bFullFsync ){
    pPager->syncFlags = SQLITE_SYNC_FULL;
    pPager->ckptSyncFlags = SQLITE_SYNC_FULL;
  }else if( bCkptFullFsync ){
    pPager->syncFlags = SQLITE_SYNC_NORMAL;
    pPager->ckptSyncFlags = SQLITE_SYNC_FULL;
  }else{
    pPager->syncFlags = SQLITE_SYNC_NORMAL;
    pPager->ckptSyncFlags = SQLITE_SYNC_NORMAL;
  }
  pPager->walSyncFlags = pPager->syncFlags;
  if( pPager->fullSync ){
    pPager->walSyncFlags |= WAL_SYNC_TRANSACTIONS;
  }
}
/*
** Get/set the locking-mode for this pager. Parameter eMode must be one
** of PAGER_LOCKINGMODE_QUERY, PAGER_LOCKINGMODE_NORMAL or 
** PAGER_LOCKINGMODE_EXCLUSIVE. If the parameter is not _QUERY, then
** the locking-mode is set to the value specified.
**
** The returned value is either PAGER_LOCKINGMODE_NORMAL or
** PAGER_LOCKINGMODE_EXCLUSIVE, indicating the current (possibly updated)
** locking-mode.
*/
//得到/设定这个页面的锁模式。参数eMode一定是PAGER_LOCKINGMODE_QUERY，PAGER_LOCKINGMODE_NORMAL或者PAGER_LOCKINGMODE_EXCLUSIVE中的一个。如果参数是not _QUERY，那么锁模式被赋值为指定的值
//返回值是PAGER_LOCKINGMODE_NORMAL或者PAGER_LOCKINGMODE_EXCLUSIVE中的一个，表明当前（可能会更新）的锁模式。
int sqlite3PagerLockingMode(Pager *pPager, int eMode){
  assert( eMode==PAGER_LOCKINGMODE_QUERY
            || eMode==PAGER_LOCKINGMODE_NORMAL
            || eMode==PAGER_LOCKINGMODE_EXCLUSIVE );
  assert( PAGER_LOCKINGMODE_QUERY<0 );
  assert( PAGER_LOCKINGMODE_NORMAL>=0 && PAGER_LOCKINGMODE_EXCLUSIVE>=0 );
  assert( pPager->exclusiveMode || 0==sqlite3WalHeapMemory(pPager->pWal) );
  if( eMode>=0 && !pPager->tempFile && !sqlite3WalHeapMemory(pPager->pWal) ){
    pPager->exclusiveMode = (u8)eMode;
  }
  return (int)pPager->exclusiveMode;
}

/*
** Set the journal-mode for this pager. Parameter eMode must be one of:
**
**    PAGER_JOURNALMODE_DELETE
**    PAGER_JOURNALMODE_TRUNCATE
**    PAGER_JOURNALMODE_PERSIST
**    PAGER_JOURNALMODE_OFF
**    PAGER_JOURNALMODE_MEMORY
**    PAGER_JOURNALMODE_WAL
**
** The journalmode is set to the value specified if the change is allowed.
** The change may be disallowed for the following reasons:
**
**   *  An in-memory database can only have its journal_mode set to _OFF
**      or _MEMORY.
**
**   *  Temporary databases cannot have _WAL journalmode.
**
** The returned indicate the current (possibly updated) journal-mode.
*/
//对这个页面设置日志模式。参数eMode一定是下列中的一个：
页面_日志模式_删除（delete）
页面_日志模式_截断（truncate）
页面_日志模式_存留（persist）
页面_日志模式_关闭（off）
页面_日志模式_存储（memory）
页面_日志模式_预写式日志（wal）

//如果改变是允许的，那么日志模式被设置成特定值。这个改变可能是不被下面的几个原因允许的：
*一个内存数据库可以直邮他的日志模式设置_off或者_memory。
*临时数据库不能有_wal日志模式。
返回指出当前（可能更新的）的日志模式。
int sqlite3PagerSetJournalMode(Pager *pPager, int eMode){
  u8 eOld = pPager->journalMode;    /* Prior journalmode */

#ifdef SQLITE_DEBUG
  /* The print_pager_state() routine is intended to be used by the debugger
  ** only.  We invoke it once here to suppress a compiler warning. */
//print_pager_state()程序是打算只被用于调试器的。一旦抑制一个编译器警告，我们调用他。
  print_pager_state(pPager);
#endif


  /* The eMode parameter is always valid */
//eMode参数经常有效的
  assert(      eMode==PAGER_JOURNALMODE_DELETE
            || eMode==PAGER_JOURNALMODE_TRUNCATE
            || eMode==PAGER_JOURNALMODE_PERSIST
            || eMode==PAGER_JOURNALMODE_OFF 
            || eMode==PAGER_JOURNALMODE_WAL 
            || eMode==PAGER_JOURNALMODE_MEMORY );

  /* This routine is only called from the OP_JournalMode opcode, and
  ** the logic there will never allow a temporary file to be changed
  ** to WAL mode.
  */
//这个程序只从OP_JournalMode操作码中调用，并且对于预写日志这里有“永远不允许一个临时文件改变”的逻辑。
  assert( pPager->tempFile==0 || eMode!=PAGER_JOURNALMODE_WAL );

  /* Do allow the journalmode of an in-memory database to be set to
  ** anything other than MEMORY or OFF
  */
//确实允许一个内存数据库的日志模式被设定成不是memory的一些东西或者off
  if( MEMDB ){
    assert( eOld==PAGER_JOURNALMODE_MEMORY || eOld==PAGER_JOURNALMODE_OFF );
    if( eMode!=PAGER_JOURNALMODE_MEMORY && eMode!=PAGER_JOURNALMODE_OFF ){
      eMode = eOld;
    }
  }

  if( eMode!=eOld ){

    /* Change the journal mode. */
//改变日志模式
    assert( pPager->eState!=PAGER_ERROR );
    pPager->journalMode = (u8)eMode;

    /* When transistioning from TRUNCATE or PERSIST to any other journal
    ** mode except WAL, unless the pager is in locking_mode=exclusive mode,
    ** delete the journal file.
    */
//当从truncate或者persist转换成任何除wal之外的其他日志模式，除非页面是在锁模式=排他模式中，删除日志文件。
    assert( (PAGER_JOURNALMODE_TRUNCATE & 5)==1 );
    assert( (PAGER_JOURNALMODE_PERSIST & 5)==1 );
    assert( (PAGER_JOURNALMODE_DELETE & 5)==0 );
    assert( (PAGER_JOURNALMODE_MEMORY & 5)==4 );
    assert( (PAGER_JOURNALMODE_OFF & 5)==0 );
    assert( (PAGER_JOURNALMODE_WAL & 5)==5 );

    assert( isOpen(pPager->fd) || pPager->exclusiveMode );
    if( !pPager->exclusiveMode && (eOld & 5)==1 && (eMode & 1)==0 ){

      /* In this case we would like to delete the journal file. If it is
      ** not possible, then that is not a problem. Deleting the journal file
      ** here is an optimization only.
      **
      ** Before deleting the journal file, obtain a RESERVED lock on the
      ** database file. This ensures that the journal file is not deleted
      ** while it is in use by some other client.
      */
//在这个例子中，我们可能删除日志文件。如果这是不可能的，那么这不是一个问题。删除这里的日志文件是唯一的最优化。
//再删除日志文件之前，保证reserver （保留）锁在一个数据库文件上。这保证了日志文件在他被用的时候不会被一些其他用户删除。
      sqlite3OsClose(pPager->jfd);
      if( pPager->eLock>=RESERVED_LOCK ){
        sqlite3OsDelete(pPager->pVfs, pPager->zJournal, 0);
      }else{
        int rc = SQLITE_OK;
        int state = pPager->eState;
        assert( state==PAGER_OPEN || state==PAGER_READER );
        if( state==PAGER_OPEN ){
          rc = sqlite3PagerSharedLock(pPager);
        }
        if( pPager->eState==PAGER_READER ){
          assert( rc==SQLITE_OK );
          rc = pagerLockDb(pPager, RESERVED_LOCK);
        }
        if( rc==SQLITE_OK ){
          sqlite3OsDelete(pPager->pVfs, pPager->zJournal, 0);
        }
        if( rc==SQLITE_OK && state==PAGER_READER ){
          pagerUnlockDb(pPager, SHARED_LOCK);
        }else if( state==PAGER_OPEN ){
          pager_unlock(pPager);
        }
        assert( state==pPager->eState );
      }
    }
  }

  /* Return the new journal mode */
//返回新日志模式
  return (int)pPager->journalMode;
}

/*
** Return the current journal mode.
*/
//返回当前日志模式
int sqlite3PagerGetJournalMode(Pager *pPager){
  return (int)pPager->journalMode;
}

/*
** Return TRUE if the pager is in a state where it is OK to change the
** journalmode.  Journalmode changes can only happen when the database
** is unmodified.
*/
如果页面在一个（改变日志模式是OK的）状态时候返回真。日志模式改变可以只在数据库没有模式化的时候发生。
int sqlite3PagerOkToChangeJournalMode(Pager *pPager){
  assert( assert_pager_state(pPager) );
  if( pPager->eState>=PAGER_WRITER_CACHEMOD ) return 0;
  if( NEVER(isOpen(pPager->jfd) && pPager->journalOff>0) ) return 0;
  return 1;
}

/*
** Get/set the size-limit used for persistent journal files.
**
** Setting the size limit to -1 means no limit is enforced.
** An attempt to set a limit smaller than -1 is a no-op.
*/
//得到/设置不变的日志文件所用到的大小限制。
设置大小限制为-1意味着没有限制被执行。
一个设置一个小于-1限制的尝试是空操作
i64 sqlite3PagerJournalSizeLimit(Pager *pPager, i64 iLimit){
  if( iLimit>=-1 ){
    pPager->journalSizeLimit = iLimit;
    sqlite3WalLimit(pPager->pWal, iLimit);
  }
  return pPager->journalSizeLimit;
}

/*
** Return a pointer to the pPager->pBackup variable. The backup module
** in backup.c maintains the content of this variable. This module
** uses it opaquely as an argument to sqlite3BackupRestart() and
** sqlite3BackupUpdate() only.
*/
返回一个对于pPager指向pBackup可用的的指针变量。备份模块在backup.c中维护了这个变量的内容。这个模块不透明地使用它作为sqlite3BackupRestart()的参数和sqlite3BackupUpdate()的参数。
sqlite3_backup **sqlite3PagerBackupPtr(Pager *pPager){
  return &pPager->pBackup;
}

/*
** Acquire a reference to page number pgno in pager pPager (a page
** reference has type DbPage*). If the requested reference is 
** successfully obtained, it is copied to *ppPage and SQLITE_OK returned.
**
** If the requested page is already in the cache, it is returned. 
** Otherwise, a new page object is allocated and populated with data
** read from the database file. In some cases, the pcache module may
** choose not to allocate a new page object and may reuse an existing
** object with no outstanding references.
**
** The extra data appended to a page is always initialized to zeros the 
** first time a page is loaded into memory. If the page requested is 
** already in the cache when this function is called, then the extra
** data is left as it was when the page object was last used.
**
** If the database image is smaller than the requested page or if a 
** non-zero value is passed as the noContent parameter and the 
** requested page is not already stored in the cache, then no 
** actual disk read occurs. In this case the memory image of the 
** page is initialized to all zeros. 
**
** If noContent is true, it means that we do not care about the contents
** of the page. This occurs in two seperate scenarios:
**
**   a) When reading a free-list leaf page from the database, and
**
**   b) When a savepoint is being rolled back and we need to load
**      a new page into the cache to be filled with the data read
**      from the savepoint journal.
**
** If noContent is true, then the data returned is zeroed instead of
** being read from the database. Additionally, the bits corresponding
** to pgno in Pager.pInJournal (bitvec of pages already written to the
** journal file) and the PagerSavepoint.pInSavepoint bitvecs of any open
** savepoints are set. This means if the page is made writable at any
** point in the future, using a call to sqlite3PagerWrite(), its contents
** will not be journaled. This saves IO.
**
** The acquisition might fail for several reasons.  In all cases,
** an appropriate error code is returned and *ppPage is set to NULL.
**
** See also sqlite3PagerLookup().  Both this routine and Lookup() attempt
** to find a page in the in-memory cache first.  If the page is not already
** in memory, this routine goes to disk to read it in whereas Lookup()
** just returns 0.  This routine acquires a read-lock the first time it
** has to go to disk, and could also playback an old journal if necessary.
** Since Lookup() never goes to disk, it never has to deal with locks
** or journal files.
*/
//获取一个页码pgno的引用在Pager类型的pager中（一个页面引用有一个典型的DbPage*）。如果所请求的引用是成功获得的，这是复制到*ppPage并且SQLITE_OK被返回。
//如果所请求的页面时已经在缓存中，那么返回他。
否则，一个新的页面对象被分配并且填充从数据库文件中读取的数据。在某些情况下，pcache模块可能选择一个（没有被分配一个新页面对象并且可能又使用了已经存在的未完成的引用的）对象。
//额外的数据附加到页面上经常初始化为0在第一次一个页面加载进内存中时候。如果当函数被调用时，页面请求已经在缓存中存在，那么额外的数据是像当页面对象最后一次使用时候的样子被留下。
//如果数据库图像是小于请求的页面或者一个非0值是作为noContent参数传递，并且请求页面不是早就存储在缓存中的，没有实际硬盘读取发生。这种情况页面的存储图像是所有都被初始化为0。
//如果noContent为真，这就意味着我们不关心页面内容。这在两个独立的场景中发生：
a）当从数据库中读取空闲列表叶页面时候，并且
b）当一个保存节点被回滚并且我们需要加载一个新页面进入一个被保存点日志中读取的数据充满的缓存中。
//如果noContent是真的，那么数据返回时0而不是从数据库中读取的。此外，比特数对应中Pager.pInJournal的页码（页面的bitvec已经写进日志文件）并且PagerSavepoint.pInSavepoint的任何开放的bitvecs设置保存点。这意味着如果页面是被制作成可写的在未来的任何点上，用一个对sqlite3PagerWrite()的调用，他的内容将不是日志的，这节省了IO。
//这次要求的失败可能有多个原因。在所有的例子中，一个适当的错误代码被返回并且*ppPager被设置为NULL。
//参见sqlite3PagerLookup()。这个例程和Lookup（）尝试先找到一个在内存缓存中的页面。如果页面不是已经在存储中的，这个例程进入磁盘去读它（在Lookup（）正好返回0时）。这个例程在第一次存入磁盘时要求一个读锁，并且如果需要的话也能重放一个旧日志。因为Lookup()永远不会进入磁盘，他永远不会去解决锁文件或者日志文件。
int sqlite3PagerAcquire(
  Pager *pPager,      /* The pager open on the database file */
  Pgno pgno,          /* Page number to fetch */
  DbPage **ppPage,    /* Write a pointer to the page here */
  int noContent       /* Do not bother reading content from disk if true */
){
  int rc;
  PgHdr *pPg;

  assert( pPager->eState>=PAGER_READER );
  assert( assert_pager_state(pPager) );

  if( pgno==0 ){
    return SQLITE_CORRUPT_BKPT;
  }

  /* If the pager is in the error state, return an error immediately. 
  ** Otherwise, request the page from the PCache layer. */
  if( pPager->errCode!=SQLITE_OK ){
    rc = pPager->errCode;
  }else{
    rc = sqlite3PcacheFetch(pPager->pPCache, pgno, 1, ppPage);
  }

  if( rc!=SQLITE_OK ){
    /* Either the call to sqlite3PcacheFetch() returned an error or the
    ** pager was already in the error-state when this function was called.
    ** Set pPg to 0 and jump to the exception handler.  */
    pPg = 0;
    goto pager_acquire_err;
  }
  assert( (*ppPage)->pgno==pgno );
  assert( (*ppPage)->pPager==pPager || (*ppPage)->pPager==0 );

  if( (*ppPage)->pPager && !noContent ){
    /* In this case the pcache already contains an initialized copy of
    ** the page. Return without further ado.  */
    assert( pgno<=PAGER_MAX_PGNO && pgno!=PAGER_MJ_PGNO(pPager) );
    pPager->aStat[PAGER_STAT_HIT]++;
    return SQLITE_OK;

  }else{
    /* The pager cache has created a new page. Its content needs to 
    ** be initialized.  */

    pPg = *ppPage;
    pPg->pPager = pPager;

    /* The maximum page number is 2^31. Return SQLITE_CORRUPT if a page
    ** number greater than this, or the unused locking-page, is requested. */
    if( pgno>PAGER_MAX_PGNO || pgno==PAGER_MJ_PGNO(pPager) ){
      rc = SQLITE_CORRUPT_BKPT;
      goto pager_acquire_err;
    }

    if( MEMDB || pPager->dbSize<pgno || noContent || !isOpen(pPager->fd) ){
      if( pgno>pPager->mxPgno ){
        rc = SQLITE_FULL;
        goto pager_acquire_err;
      }
      if( noContent ){
        /* Failure to set the bits in the InJournal bit-vectors is benign.
        ** It merely means that we might do some extra work to journal a 
        ** page that does not need to be journaled.  Nevertheless, be sure 
        ** to test the case where a malloc error occurs while trying to set 
        ** a bit in a bit vector.
        */
        sqlite3BeginBenignMalloc();
        if( pgno<=pPager->dbOrigSize ){
          TESTONLY( rc = ) sqlite3BitvecSet(pPager->pInJournal, pgno);
          testcase( rc==SQLITE_NOMEM );
        }
        TESTONLY( rc = ) addToSavepointBitvecs(pPager, pgno);
        testcase( rc==SQLITE_NOMEM );
        sqlite3EndBenignMalloc();
      }
      memset(pPg->pData, 0, pPager->pageSize);
      IOTRACE(("ZERO %p %d\n", pPager, pgno));
    }else{
      assert( pPg->pPager==pPager );
      pPager->aStat[PAGER_STAT_MISS]++;
      rc = readDbPage(pPg);
      if( rc!=SQLITE_OK ){
        goto pager_acquire_err;
      }
    }
    pager_set_pagehash(pPg);
  }

  return SQLITE_OK;

pager_acquire_err:
  assert( rc!=SQLITE_OK );
  if( pPg ){
    sqlite3PcacheDrop(pPg);
  }
  pagerUnlockIfUnused(pPager);

  *ppPage = 0;
  return rc;
}

/*
** Acquire a page if it is already in the in-memory cache.  Do
** not read the page from disk.  Return a pointer to the page,
** or 0 if the page is not in cache. 
**
** See also sqlite3PagerGet().  The difference between this routine
** and sqlite3PagerGet() is that _get() will go to the disk and read
** in the page if the page is not already in cache.  This routine
** returns NULL if the page is not in cache or if a disk I/O error 
** has ever happened.
*/

//如果一个页面已经在内存缓存中，那么获取一个。不从磁盘读取的页面。如果页面不在缓存中，就返回一个指向该页面的指针或0。
//参见sqlite3PagerGet()。这个例程和sqlite3PagerGet()之间的区别是_get()将进入磁盘并且读取页面（如果页面已经不在缓存中）。这个例程返回NULL，如果页面不在缓存或者磁盘I / O错误曾经发生过。
DbPage *sqlite3PagerLookup(Pager *pPager, Pgno pgno){
  PgHdr *pPg = 0;
  assert( pPager!=0 );
  assert( pgno!=0 );
  assert( pPager->pPCache!=0 );
  assert( pPager->eState>=PAGER_READER && pPager->eState!=PAGER_ERROR );
  sqlite3PcacheFetch(pPager->pPCache, pgno, 0, &pPg);
  return pPg;
}

/*
** Increment the reference count for page pPg.
*/
//页面pPg的引用计数的增量。
void sqlite3PagerRef(DbPage *pPg){
  sqlite3PcacheRef(pPg);
}

/*
** Release a page reference.
**
** If the number of references to the page drop to zero, then the
** page is added to the LRU list.  When all references to all pages
** are released, a rollback occurs and the lock on the database is
** removed.
*/
//释放一个页面引用。
//如果引用页面的数量降至零,然后页面被添加到LRU列表。当所有对所有页面的所有引用被释放,一个回滚发生并且数据库上的锁被删除。
void sqlite3PagerUnref(DbPage *pPg){
  if( pPg ){
    Pager *pPager = pPg->pPager;
    sqlite3PcacheRelease(pPg);
    pagerUnlockIfUnused(pPager);
  }
}

/*
** Mark a data page as writeable. This routine must be called before 
** making changes to a page. The caller must check the return value 
** of this function and be careful not to change any page data unless 
** this routine returns SQLITE_OK.
**
** The difference between this function and pager_write() is that this
** function also deals with the special case where 2 or more pages
** fit on a single disk sector. In this case all co-resident pages
** must have been written to the journal file before returning.
**
** If an error occurs, SQLITE_NOMEM or an IO error code is returned
** as appropriate. Otherwise, SQLITE_OK.
*/
//数据页面标记为可写。在修改一个页面之前必须调用这个例程。调用者必须检查这个函数的返回值并且注意不要改变任何页面数据,除非这个例程返回SQLITE_OK。
//这个函数和pager_write()之间的区别是这个函数也处理特殊情况（既两个或更多的页面适合单个磁盘扇区）。在这种情况下共驻主存的所有页面一定是在返回前写入日志文件。
//如果出现错误,SQLITE_NOMEM或IO错误代码会适当的返回。否则,返回SQLITE_OK。
int sqlite3PagerWrite(DbPage *pDbPage){
  int rc = SQLITE_OK;

  PgHdr *pPg = pDbPage;
  Pager *pPager = pPg->pPager;
  Pgno nPagePerSector = (pPager->sectorSize/pPager->pageSize);

  assert( pPager->eState>=PAGER_WRITER_LOCKED );
  assert( pPager->eState!=PAGER_ERROR );
  assert( assert_pager_state(pPager) );

  if( nPagePerSector>1 ){
    Pgno nPageCount;          /* Total number of pages in database file */
    Pgno pg1;                 /* First page of the sector pPg is located on. */
    int nPage = 0;            /* Number of pages starting at pg1 to journal */
    int ii;                   /* Loop counter */
    int needSync = 0;         /* True if any page has PGHDR_NEED_SYNC */

    /* Set the doNotSyncSpill flag to 1. This is because we cannot allow
    ** a journal header to be written between the pages journaled by
    ** this function.
    */
    assert( !MEMDB );
    assert( pPager->doNotSyncSpill==0 );
    pPager->doNotSyncSpill++;

    /* This trick assumes that both the page-size and sector-size are
    ** an integer power of 2. It sets variable pg1 to the identifier
    ** of the first page of the sector pPg is located on.
    */
    pg1 = ((pPg->pgno-1) & ~(nPagePerSector-1)) + 1;

    nPageCount = pPager->dbSize;
    if( pPg->pgno>nPageCount ){
      nPage = (pPg->pgno - pg1)+1;
    }else if( (pg1+nPagePerSector-1)>nPageCount ){
      nPage = nPageCount+1-pg1;
    }else{
      nPage = nPagePerSector;
    }
    assert(nPage>0);
    assert(pg1<=pPg->pgno);
    assert((pg1+nPage)>pPg->pgno);

    for(ii=0; ii<nPage && rc==SQLITE_OK; ii++){
      Pgno pg = pg1+ii;
      PgHdr *pPage;
      if( pg==pPg->pgno || !sqlite3BitvecTest(pPager->pInJournal, pg) ){
        if( pg!=PAGER_MJ_PGNO(pPager) ){
          rc = sqlite3PagerGet(pPager, pg, &pPage);
          if( rc==SQLITE_OK ){
            rc = pager_write(pPage);
            if( pPage->flags&PGHDR_NEED_SYNC ){
              needSync = 1;
            }
            sqlite3PagerUnref(pPage);
          }
        }
      }else if( (pPage = pager_lookup(pPager, pg))!=0 ){
        if( pPage->flags&PGHDR_NEED_SYNC ){
          needSync = 1;
        }
        sqlite3PagerUnref(pPage);
      }
    }

    /* If the PGHDR_NEED_SYNC flag is set for any of the nPage pages 
    ** starting at pg1, then it needs to be set for all of them. Because
    ** writing to any of these nPage pages may damage the others, the
    ** journal file must contain sync()ed copies of all of them
    ** before any of them can be written out to the database file.
    */
    if( rc==SQLITE_OK && needSync ){
      assert( !MEMDB );
      for(ii=0; ii<nPage; ii++){
        PgHdr *pPage = pager_lookup(pPager, pg1+ii);
        if( pPage ){
          pPage->flags |= PGHDR_NEED_SYNC;
          sqlite3PagerUnref(pPage);
        }
      }
    }

    assert( pPager->doNotSyncSpill==1 );
    pPager->doNotSyncSpill--;
  }else{
    rc = pager_write(pDbPage);
  }
  return rc;
}


/*
** A call to this routine tells the pager that it is not necessary to
** write the information on page pPg back to the disk, even though
** that page might be marked as dirty.  This happens, for example, when
** the page has been added as a leaf of the freelist and so its
** content no longer matters.
**
** The overlying software layer calls this routine when all of the data
** on the given page is unused. The pager marks the page as clean so
** that it does not get written to disk.
**
** Tests show that this optimization can quadruple the speed of large 
** DELETE operations.
*/
//这个例程的调用告诉页面,没有必要将页面pPg的信息写回到磁盘,尽管这个页面可能标记为脏页面。这一切发生的时候,例如,当页面被添加作为释放列表的叶节点，并且这样它的内容不再重要。

//当在给定的页面所有数据时是未使用的，整个覆盖的软件层的调用这个例程，。页面标志页面时作为干净的，这样它不会被写入磁盘。

//测试表明,这种优化可以四倍的大型删除操作速度。
void sqlite3PagerDontWrite(PgHdr *pPg){
  Pager *pPager = pPg->pPager;
  if( (pPg->flags&PGHDR_DIRTY) && pPager->nSavepoint==0 ){
    PAGERTRACE(("DONT_WRITE page %d of %d\n", pPg->pgno, PAGERID(pPager)));
    IOTRACE(("CLEAN %p %d\n", pPager, pPg->pgno))
    pPg->flags |= PGHDR_DONT_WRITE;
    pager_set_pagehash(pPg);
  }
}
/*
** Move the page pPg to location pgno in the file.
**
** There must be no references to the page previously located at
** pgno (which we call pPgOld) though that page is allowed to be
** in cache.  If the page previously located at pgno is not already
** in the rollback journal, it is not put there by by this routine.
**
** References to the page pPg remain valid. Updating any
** meta-data associated with pPg (i.e. data stored in the nExtra bytes
** allocated along with the page) is the responsibility of the caller.
**
** A transaction must be active when this routine is called. It used to be
** required that a statement transaction was not active, but this restriction
** has been removed (CREATE INDEX needs to move a page when a statement
** transaction is active).
**
** If the fourth argument, isCommit, is non-zero, then this page is being
** moved as part of a database reorganization just before the transaction 
** is being committed. In this case, it is guaranteed that the database page 
** pPg refers to will not be written to again within this transaction.
**
** This function may return SQLITE_NOMEM or an IO error code if an error
** occurs. Otherwise, it returns SQLITE_OK.
*/
//移动页面pPg来定位文件中的pgno。
//这里之前一定没有定位于pgno（我们称为pPgOld）对页面的引用，尽管页面是允许在缓存中的。如果页面之前定位在pgno，那么它不是已经在回滚日志中，它不是被这个线程放在这里的。
//页面pPg的引用仍然有效。更新任何与pPg联系的元数据（例如，存储在nExtra字节的数据随着页面分配）是调用者的责任。
//一个事务在这个例程被调用时一定很活跃。这是被用来要求一个声明事物不活跃，但是这一限制已经被删除。（CREATE INDEX需要在声明事务是活跃的时候移动一个页面）
//如果第四个参数isCommit是非0的，那么在事务提交前，这个页面是作为数据库重组的一部分来移动。在这种情况下，他是保证数据库页面pPg引用将不在这个事务中被重写。
//这个函数可能返回 SQLITE_NOMEM或者一个IO错误代码，如果一个错误发生的时候。否则他就返回 SQLITE_OK.
int sqlite3PagerMovepage(Pager *pPager, DbPage *pPg, Pgno pgno, int isCommit){
  PgHdr *pPgOld;               /* The page being overwritten. */
  Pgno needSyncPgno = 0;       /* Old value of pPg->pgno, if sync is required */
  int rc;                      /* Return code */
  Pgno origPgno;               /* The original page number */

  assert( pPg->nRef>0 );
  assert( pPager->eState==PAGER_WRITER_CACHEMOD
       || pPager->eState==PAGER_WRITER_DBMOD
  );
  assert( assert_pager_state(pPager) );

  /* In order to be able to rollback, an in-memory database must journal
  ** the page we are moving from.
  */
  if( MEMDB ){
    rc = sqlite3PagerWrite(pPg);
    if( rc ) return rc;
  }

  /* If the page being moved is dirty and has not been saved by the latest
  ** savepoint, then save the current contents of the page into the 
  ** sub-journal now. This is required to handle the following scenario:
  **
  **   BEGIN;
  **     <journal page X, then modify it in memory>
  **     SAVEPOINT one;
  **       <Move page X to location Y>
  **     ROLLBACK TO one;
  **
  ** If page X were not written to the sub-journal here, it would not
  ** be possible to restore its contents when the "ROLLBACK TO one"
  ** statement were is processed.
  **
  ** subjournalPage() may need to allocate space to store pPg->pgno into
  ** one or more savepoint bitvecs. This is the reason this function
  ** may return SQLITE_NOMEM.
  */
  if( pPg->flags&PGHDR_DIRTY
   && subjRequiresPage(pPg)
   && SQLITE_OK!=(rc = subjournalPage(pPg))
  ){
    return rc;
  }

  PAGERTRACE(("MOVE %d page %d (needSync=%d) moves to %d\n", 
      PAGERID(pPager), pPg->pgno, (pPg->flags&PGHDR_NEED_SYNC)?1:0, pgno));
  IOTRACE(("MOVE %p %d %d\n", pPager, pPg->pgno, pgno))

  /* If the journal needs to be sync()ed before page pPg->pgno can
  ** be written to, store pPg->pgno in local variable needSyncPgno.
  **
  ** If the isCommit flag is set, there is no need to remember that
  ** the journal needs to be sync()ed before database page pPg->pgno 
  ** can be written to. The caller has already promised not to write to it.
  */
  if( (pPg->flags&PGHDR_NEED_SYNC) && !isCommit ){
    needSyncPgno = pPg->pgno;
    assert( pageInJournal(pPg) || pPg->pgno>pPager->dbOrigSize );
    assert( pPg->flags&PGHDR_DIRTY );
  }

  /* If the cache contains a page with page-number pgno, remove it
  ** from its hash chain. Also, if the PGHDR_NEED_SYNC flag was set for 
  ** page pgno before the 'move' operation, it needs to be retained 
  ** for the page moved there.
  */
  pPg->flags &= ~PGHDR_NEED_SYNC;
  pPgOld = pager_lookup(pPager, pgno);
  assert( !pPgOld || pPgOld->nRef==1 );
  if( pPgOld ){
    pPg->flags |= (pPgOld->flags&PGHDR_NEED_SYNC);
    if( MEMDB ){
      /* Do not discard pages from an in-memory database since we might
      ** need to rollback later.  Just move the page out of the way. */
      sqlite3PcacheMove(pPgOld, pPager->dbSize+1);
    }else{
      sqlite3PcacheDrop(pPgOld);
    }
  }

  origPgno = pPg->pgno;
  sqlite3PcacheMove(pPg, pgno);
  sqlite3PcacheMakeDirty(pPg);

  /* For an in-memory database, make sure the original page continues
  ** to exist, in case the transaction needs to roll back.  Use pPgOld
  ** as the original page since it has already been allocated.
  */
  if( MEMDB ){
    assert( pPgOld );
    sqlite3PcacheMove(pPgOld, origPgno);
    sqlite3PagerUnref(pPgOld);
  }

  if( needSyncPgno ){
    /* If needSyncPgno is non-zero, then the journal file needs to be 
    ** sync()ed before any data is written to database file page needSyncPgno.
    ** Currently, no such page exists in the page-cache and the 
    ** "is journaled" bitvec flag has been set. This needs to be remedied by
    ** loading the page into the pager-cache and setting the PGHDR_NEED_SYNC
    ** flag.
    **
    ** If the attempt to load the page into the page-cache fails, (due
    ** to a malloc() or IO failure), clear the bit in the pInJournal[]
    ** array. Otherwise, if the page is loaded and written again in
    ** this transaction, it may be written to the database file before
    ** it is synced into the journal file. This way, it may end up in
    ** the journal file twice, but that is not a problem.
    */
    PgHdr *pPgHdr;
    rc = sqlite3PagerGet(pPager, needSyncPgno, &pPgHdr);
    if( rc!=SQLITE_OK ){
      if( needSyncPgno<=pPager->dbOrigSize ){
        assert( pPager->pTmpSpace!=0 );
        sqlite3BitvecClear(pPager->pInJournal, needSyncPgno, pPager->pTmpSpace);
      }
      return rc;
    }
    pPgHdr->flags |= PGHDR_NEED_SYNC;
    sqlite3PcacheMakeDirty(pPgHdr);
    sqlite3PagerUnref(pPgHdr);
  }

  return SQLITE_OK;
}

/*
** Return the number of references to the specified page.
*/
//*返回引用指定的页面的数量。
int sqlite3PagerPageRefcount(DbPage *pPage){
  return sqlite3PcachePageRefcount(pPage);
}

/*
** Return a pointer to the data for the specified page.
*/
//返回一个对于指定页面数据的指针。
void *sqlite3PagerGetData(DbPage *pPg){
  assert( pPg->nRef>0 || pPg->pPager->memDb );
  return pPg->pData;
}

/*
** Return a pointer to the Pager.nExtra bytes of "extra" space 
** allocated along with the specified page.
*/
//返回一个对于页面的nExtra“额外”空间字节的指针，空间是随着指定的页面分配。
void *sqlite3PagerGetExtra(DbPage *pPg){
  return pPg->pExtra;
}

/*
** This function may only be called when a read-transaction is open on
** the pager. It returns the total number of pages in the database.
**
** However, if the file is between 1 and <page-size> bytes in size, then 
** this is considered a 1 page file.
*/
//这个函数可能只能在页面上打开一个读事务时被调用。它返回在数据库中的页面总数。
//然而,如果文件是介于1和<页面大小>字节大小之间的话，那么这是一个1大小的页文件。
void sqlite3PagerPagecount(Pager *pPager, int *pnPage){
  assert( pPager->eState>=PAGER_READER );
  assert( pPager->eState!=PAGER_WRITER_FINISHED );
  *pnPage = (int)pPager->dbSize;
}

/*
** Begin a write-transaction on the specified pager object. If a 
** write-transaction has already been opened, this function is a no-op.
**
** If the exFlag argument is false, then acquire at least a RESERVED
** lock on the database file. If exFlag is true, then acquire at least
** an EXCLUSIVE lock. If such a lock is already held, no locking 
** functions need be called.
**
** If the subjInMemory argument is non-zero, then any sub-journal opened
** within this transaction will be opened as an in-memory file. This
** has no effect if the sub-journal is already opened (as it may be when
** running in exclusive mode) or if the transaction does not require a
** sub-journal. If the subjInMemory argument is zero, then any required
** sub-journal is implemented in-memory if pPager is an in-memory database, 
** or using a temporary file otherwise.
*/
//开始一个写事务在特定的页面对象上。如果一个写事务已经被打开，这个函数是没有操作的。
//如果exFlag参数是错误的，那么至少获得一个保留锁在数据库文件中。如果exFlag是正确的，那么获得至少一个排他锁。如果这样一个锁已经存在，没有需要被调用的锁函数。
//如果subjInMemory参数是非0的，那么任何在事务中打开的子日志将作为一个内存文件打开。这是没有影响的，如果子日志已经打开了（就像它可能在互斥方式中运行的时候），或者如果事务不要求一个子日志。如果subjInMemory参数是0，那么任何所要求的子日志是实现在内存中的，如果pPager是一个内存数据库或者另外用一个临时文件。
int sqlite3PagerBegin(Pager *pPager, int exFlag, int subjInMemory){
  int rc = SQLITE_OK;

  if( pPager->errCode ) return pPager->errCode;
  assert( pPager->eState>=PAGER_READER && pPager->eState<PAGER_ERROR );
  pPager->subjInMemory = (u8)subjInMemory;

  if( ALWAYS(pPager->eState==PAGER_READER) ){
    assert( pPager->pInJournal==0 );

    if( pagerUseWal(pPager) ){
      /* If the pager is configured to use locking_mode=exclusive, and an
      ** exclusive lock on the database is not already held, obtain it now.
      */
      if( pPager->exclusiveMode && sqlite3WalExclusiveMode(pPager->pWal, -1) ){
        rc = pagerLockDb(pPager, EXCLUSIVE_LOCK);
        if( rc!=SQLITE_OK ){
          return rc;
        }
        sqlite3WalExclusiveMode(pPager->pWal, 1);
      }

      /* Grab the write lock on the log file. If successful, upgrade to
      ** PAGER_RESERVED state. Otherwise, return an error code to the caller.
      ** The busy-handler is not invoked if another connection already
      ** holds the write-lock. If possible, the upper layer will call it.
      */
      rc = sqlite3WalBeginWriteTransaction(pPager->pWal);
    }else{
      /* Obtain a RESERVED lock on the database file. If the exFlag parameter
      ** is true, then immediately upgrade this to an EXCLUSIVE lock. The
      ** busy-handler callback can be used when upgrading to the EXCLUSIVE
      ** lock, but not when obtaining the RESERVED lock.
      */
      rc = pagerLockDb(pPager, RESERVED_LOCK);
      if( rc==SQLITE_OK && exFlag ){
        rc = pager_wait_on_lock(pPager, EXCLUSIVE_LOCK);
      }
    }

    if( rc==SQLITE_OK ){
      /* Change to WRITER_LOCKED state.
      **
      ** WAL mode sets Pager.eState to PAGER_WRITER_LOCKED or CACHEMOD
      ** when it has an open transaction, but never to DBMOD or FINISHED.
      ** This is because in those states the code to roll back savepoint 
      ** transactions may copy data from the sub-journal into the database 
      ** file as well as into the page cache. Which would be incorrect in 
      ** WAL mode.
      */
      pPager->eState = PAGER_WRITER_LOCKED;
      pPager->dbHintSize = pPager->dbSize;
      pPager->dbFileSize = pPager->dbSize;
      pPager->dbOrigSize = pPager->dbSize;
      pPager->journalOff = 0;
    }

    assert( rc==SQLITE_OK || pPager->eState==PAGER_READER );
    assert( rc!=SQLITE_OK || pPager->eState==PAGER_WRITER_LOCKED );
    assert( assert_pager_state(pPager) );
  }

  PAGERTRACE(("TRANSACTION %d\n", PAGERID(pPager)));
  return rc;
}

/*
** Sync the database file for the pager pPager. zMaster points to the name
** of a master journal file that should be written into the individual
** journal file. zMaster may be NULL, which is interpreted as no master
** journal (a single database transaction).
**
** This routine ensures that:
**
**   * The database file change-counter is updated,
**   * the journal is synced (unless the atomic-write optimization is used),
**   * all dirty pages are written to the database file, 
**   * the database file is truncated (if required), and
**   * the database file synced. 
**
** The only thing that remains to commit the transaction is to finalize 
** (delete, truncate or zero the first part of) the journal file (or 
** delete the master journal file if specified).
**
** Note that if zMaster==NULL, this does not overwrite a previous value
** passed to an sqlite3PagerCommitPhaseOne() call.
**
** If the final parameter - noSync - is true, then the database file itself
** is not synced. The caller must call sqlite3PagerSync() directly to
** sync the database file before calling CommitPhaseTwo() to delete the
** journal file in this case.
*/
//对于页面pPager同步数据库文件。zMaster指向一个应该写入单独的日志文件的主日志文件的名字。zMaster可能为空，这个就可以解释为什么没有主日志（一个单独的数据库事务）。
这个例程确保了：
*change-counter被更新。
*日志是同步的（除非运用自动写优化）
*所有脏页面时被写入数据库文件中
*数据库文件被截断（如果要求的话），并且
*同步数据库文件
//保持提交事务的唯一的事情是完成（删除、截断或者对第一部分赋值0）日志文件（或者如果特别指定的话，删除主日志文件）。
//注意到如果zMaster==NULL，这不会重写一个传递给sqlite3PagerCommitPhaseOne()调用的前值
//如果最后一个参数，noSynced为真，那么数据库文件他自己不是同步的。调用者一定调用sqlite3PagerSync()，在这个例子中，在调用 CommitPhaseTwo()去删除日志文件之前直接同步数据库文件。

int sqlite3PagerCommitPhaseOne(
  Pager *pPager,                  /* Pager object */
  const char *zMaster,            /* If not NULL, the master journal name */
  int noSync                      /* True to omit the xSync on the db file */
){
  int rc = SQLITE_OK;             /* Return code */

  assert( pPager->eState==PAGER_WRITER_LOCKED
       || pPager->eState==PAGER_WRITER_CACHEMOD
       || pPager->eState==PAGER_WRITER_DBMOD
       || pPager->eState==PAGER_ERROR
  );
  assert( assert_pager_state(pPager) );

  /* If a prior error occurred, report that error again. */
  if( NEVER(pPager->errCode) ) return pPager->errCode;

  PAGERTRACE(("DATABASE SYNC: File=%s zMaster=%s nSize=%d\n", 
      pPager->zFilename, zMaster, pPager->dbSize));

  /* If no database changes have been made, return early. */
  if( pPager->eState<PAGER_WRITER_CACHEMOD ) return SQLITE_OK;

  if( MEMDB ){
    /* If this is an in-memory db, or no pages have been written to, or this
    ** function has already been called, it is mostly a no-op.  However, any
    ** backup in progress needs to be restarted.
    */
    sqlite3BackupRestart(pPager->pBackup);
  }else{
    if( pagerUseWal(pPager) ){
      PgHdr *pList = sqlite3PcacheDirtyList(pPager->pPCache);
      PgHdr *pPageOne = 0;
      if( pList==0 ){
        /* Must have at least one page for the WAL commit flag.
        ** Ticket [2d1a5c67dfc2363e44f29d9bbd57f] 2011-05-18 */
        rc = sqlite3PagerGet(pPager, 1, &pPageOne);
        pList = pPageOne;
        pList->pDirty = 0;
      }
      assert( rc==SQLITE_OK );
      if( ALWAYS(pList) ){
        rc = pagerWalFrames(pPager, pList, pPager->dbSize, 1);
      }
      sqlite3PagerUnref(pPageOne);
      if( rc==SQLITE_OK ){
        sqlite3PcacheCleanAll(pPager->pPCache);
      }
    }else{
      /* The following block updates the change-counter. Exactly how it
      ** does this depends on whether or not the atomic-update optimization
      ** was enabled at compile time, and if this transaction meets the 
      ** runtime criteria to use the operation: 
      **
      **    * The file-system supports the atomic-write property for
      **      blocks of size page-size, and 
      **    * This commit is not part of a multi-file transaction, and
      **    * Exactly one page has been modified and store in the journal file.
      **
      ** If the optimization was not enabled at compile time, then the
      ** pager_incr_changecounter() function is called to update the change
      ** counter in 'indirect-mode'. If the optimization is compiled in but
      ** is not applicable to this transaction, call sqlite3JournalCreate()
      ** to make sure the journal file has actually been created, then call
      ** pager_incr_changecounter() to update the change-counter in indirect
      ** mode. 
      **
      ** Otherwise, if the optimization is both enabled and applicable,
      ** then call pager_incr_changecounter() to update the change-counter
      ** in 'direct' mode. In this case the journal file will never be
      ** created for this transaction.
      */
  #ifdef SQLITE_ENABLE_ATOMIC_WRITE
      PgHdr *pPg;
      assert( isOpen(pPager->jfd) 
           || pPager->journalMode==PAGER_JOURNALMODE_OFF 
           || pPager->journalMode==PAGER_JOURNALMODE_WAL 
      );
      if( !zMaster && isOpen(pPager->jfd) 
       && pPager->journalOff==jrnlBufferSize(pPager) 
       && pPager->dbSize>=pPager->dbOrigSize
       && (0==(pPg = sqlite3PcacheDirtyList(pPager->pPCache)) || 0==pPg->pDirty)
      ){
        /* Update the db file change counter via the direct-write method. The 
        ** following call will modify the in-memory representation of page 1 
        ** to include the updated change counter and then write page 1 
        ** directly to the database file. Because of the atomic-write 
        ** property of the host file-system, this is safe.
        */
        rc = pager_incr_changecounter(pPager, 1);
      }else{
        rc = sqlite3JournalCreate(pPager->jfd);
        if( rc==SQLITE_OK ){
          rc = pager_incr_changecounter(pPager, 0);
        }
      }
  #else
      rc = pager_incr_changecounter(pPager, 0);
  #endif
      if( rc!=SQLITE_OK ) goto commit_phase_one_exit;
  
      /* If this transaction has made the database smaller, then all pages
      ** being discarded by the truncation must be written to the journal
      ** file. This can only happen in auto-vacuum mode.
      **
      ** Before reading the pages with page numbers larger than the 
      ** current value of Pager.dbSize, set dbSize back to the value
      ** that it took at the start of the transaction. Otherwise, the
      ** calls to sqlite3PagerGet() return zeroed pages instead of 
      ** reading data from the database file.
      */
  #ifndef SQLITE_OMIT_AUTOVACUUM
      if( pPager->dbSize<pPager->dbOrigSize 
       && pPager->journalMode!=PAGER_JOURNALMODE_OFF
      ){
        Pgno i;                                   /* Iterator variable */
        const Pgno iSkip = PAGER_MJ_PGNO(pPager); /* Pending lock page */
        const Pgno dbSize = pPager->dbSize;       /* Database image size */ 
        pPager->dbSize = pPager->dbOrigSize;
        for( i=dbSize+1; i<=pPager->dbOrigSize; i++ ){
          if( !sqlite3BitvecTest(pPager->pInJournal, i) && i!=iSkip ){
            PgHdr *pPage;             /* Page to journal */
            rc = sqlite3PagerGet(pPager, i, &pPage);
            if( rc!=SQLITE_OK ) goto commit_phase_one_exit;
            rc = sqlite3PagerWrite(pPage);
            sqlite3PagerUnref(pPage);
            if( rc!=SQLITE_OK ) goto commit_phase_one_exit;
          }
        }
        pPager->dbSize = dbSize;
      } 
  #endif
  
      /* Write the master journal name into the journal file. If a master 
      ** journal file name has already been written to the journal file, 
      ** or if zMaster is NULL (no master journal), then this call is a no-op.
      */
      rc = writeMasterJournal(pPager, zMaster);
      if( rc!=SQLITE_OK ) goto commit_phase_one_exit;
  
      /* Sync the journal file and write all dirty pages to the database.
      ** If the atomic-update optimization is being used, this sync will not 
      ** create the journal file or perform any real IO.
      **
      ** Because the change-counter page was just modified, unless the
      ** atomic-update optimization is used it is almost certain that the
      ** journal requires a sync here. However, in locking_mode=exclusive
      ** on a system under memory pressure it is just possible that this is 
      ** not the case. In this case it is likely enough that the redundant
      ** xSync() call will be changed to a no-op by the OS anyhow. 
      */
      rc = syncJournal(pPager, 0);
      if( rc!=SQLITE_OK ) goto commit_phase_one_exit;
  
      rc = pager_write_pagelist(pPager,sqlite3PcacheDirtyList(pPager->pPCache));
      if( rc!=SQLITE_OK ){
        assert( rc!=SQLITE_IOERR_BLOCKED );
        goto commit_phase_one_exit;
      }
      sqlite3PcacheCleanAll(pPager->pPCache);
  
      /* If the file on disk is not the same size as the database image,
      ** then use pager_truncate to grow or shrink the file here.
      */
      if( pPager->dbSize!=pPager->dbFileSize ){
        Pgno nNew = pPager->dbSize - (pPager->dbSize==PAGER_MJ_PGNO(pPager));
        assert( pPager->eState==PAGER_WRITER_DBMOD );
        rc = pager_truncate(pPager, nNew);
        if( rc!=SQLITE_OK ) goto commit_phase_one_exit;
      }
  
      /* Finally, sync the database file. */
      if( !noSync ){
        rc = sqlite3PagerSync(pPager);
      }
      IOTRACE(("DBSYNC %p\n", pPager))
    }
  }

commit_phase_one_exit:
  if( rc==SQLITE_OK && !pagerUseWal(pPager) ){
    pPager->eState = PAGER_WRITER_FINISHED;
  }
  return rc;
}

/*
** This function may only be called while a write-transaction is active in
** rollback. If the connection is in WAL mode, this call is a no-op. 
** Otherwise, if the connection does not already have an EXCLUSIVE lock on 
** the database file, an attempt is made to obtain one.
**
** If the EXCLUSIVE lock is already held or the attempt to obtain it is
** successful, or the connection is in WAL mode, SQLITE_OK is returned.
** Otherwise, either SQLITE_BUSY or an SQLITE_IOERR_XXX error code is 
** returned.
*/
//这个函数只能被调用，当一个写事务活跃在回滚时。如果连接是在WAL模式,这个调用是一个空操作。否则,如果连接不是已经有一个独占锁在数据库文件上,一个尝试来保证他是成功的。
//如果互斥型锁已经存在或试图保证他是成功的,或者连接是在wal模式,返回SQLITE_OK。否则,SQLITE_BUSY或一个SQLITE_IOERR_XXX错误代码被返回
int sqlite3PagerExclusiveLock(Pager *pPager){
  int rc = SQLITE_OK;
  assert( pPager->eState==PAGER_WRITER_CACHEMOD 
       || pPager->eState==PAGER_WRITER_DBMOD 
       || pPager->eState==PAGER_WRITER_LOCKED 
  );
  assert( assert_pager_state(pPager) );
  if( 0==pagerUseWal(pPager) ){
    rc = pager_wait_on_lock(pPager, EXCLUSIVE_LOCK);
  }
  return rc;
}

/*
** Sync the database file to disk. This is a no-op for in-memory databases
** or pages with the Pager.noSync flag set.
**
** If successful, or if called on a pager for which it is a no-op, this
** function returns SQLITE_OK. Otherwise, an IO error code is returned.
*/
//同步数据库文件到磁盘上。这对于内存数据库或者是有Pager.noSync标志设置的页面是无操作的。
//如果成功，或者如果在一个页面上对于一个无操作的调用，这个函数返回 SQLITE_OK，否则，返回一个IO错误代码。
int sqlite3PagerSync(Pager *pPager){
  int rc = SQLITE_OK;
  if( !pPager->noSync ){
    assert( !MEMDB );
    rc = sqlite3OsSync(pPager->fd, pPager->syncFlags);
  }else if( isOpen(pPager->fd) ){
    assert( !MEMDB );
    rc = sqlite3OsFileControl(pPager->fd, SQLITE_FCNTL_SYNC_OMITTED, 0);
    if( rc==SQLITE_NOTFOUND ){
      rc = SQLITE_OK;
    }
  }
  return rc;
}


/*
** When this function is called, the database file has been completely
** updated to reflect the changes made by the current transaction and
** synced to disk. The journal file still exists in the file-system 
** though, and if a failure occurs at this point it will eventually
** be used as a hot-journal and the current transaction rolled back.
**
** This function finalizes the journal file, either by deleting, 
** truncating or partially zeroing it, so that it cannot be used 
** for hot-journal rollback. Once this is done the transaction is
** irrevocably committed.
**
** If an error occurs, an IO error code is returned and the pager
** moves into the error state. Otherwise, SQLITE_OK is returned.
*/
//当函数被调用时，数据库文件被完全的更新来反映当前事务和同步到磁盘时产生的变化。日志文件仍然存在于文件系统中，如果发生在这一点上它最终将作为热门日志并且当前事务回滚。

//这个函数通过删除、截断或部分归零来结束日志文件,所以它不能被用作热门日志回滚。一旦完成，事务是不可逆转地提交。

//如果出现错误,返回一个IO错误代码并且页面进入错误状态。否则,返回SQLITE_OK。

int sqlite3PagerCommitPhaseTwo(Pager *pPager){
  int rc = SQLITE_OK;                  /* Return code */

  /* This routine should not be called if a prior error has occurred.
  ** But if (due to a coding error elsewhere in the system) it does get
  ** called, just return the same error code without doing anything. */
  if( NEVER(pPager->errCode) ) return pPager->errCode;

  assert( pPager->eState==PAGER_WRITER_LOCKED
       || pPager->eState==PAGER_WRITER_FINISHED
       || (pagerUseWal(pPager) && pPager->eState==PAGER_WRITER_CACHEMOD)
  );
  assert( assert_pager_state(pPager) );

  /* An optimization. If the database was not actually modified during
  ** this transaction, the pager is running in exclusive-mode and is
  ** using persistent journals, then this function is a no-op.
  **
  ** The start of the journal file currently contains a single journal 
  ** header with the nRec field set to 0. If such a journal is used as
  ** a hot-journal during hot-journal rollback, 0 changes will be made
  ** to the database file. So there is no need to zero the journal 
  ** header. Since the pager is in exclusive mode, there is no need
  ** to drop any locks either.
  */
  if( pPager->eState==PAGER_WRITER_LOCKED 
   && pPager->exclusiveMode 
   && pPager->journalMode==PAGER_JOURNALMODE_PERSIST
  ){
    assert( pPager->journalOff==JOURNAL_HDR_SZ(pPager) || !pPager->journalOff );
    pPager->eState = PAGER_READER;
    return SQLITE_OK;
  }

  PAGERTRACE(("COMMIT %d\n", PAGERID(pPager)));
  rc = pager_end_transaction(pPager, pPager->setMaster);
  return pager_error(pPager, rc);
}
 
/*
** If a write transaction is open, then all changes made within the
** transaction are reverted and the current write-transaction is closed.
** The pager falls back to PAGER_READER state if successful, or PAGER_ERROR
** state if an error occurs.
**
** If the pager is already in PAGER_ERROR state when this function is called,
** it returns Pager.errCode immediately. No work is performed in this case.
**
** Otherwise, in rollback mode, this function performs two functions:
**
**   1) It rolls back the journal file, restoring all database file and
**      in-memory cache pages to the state they were in when the transaction
**      was opened, and
**
**   2) It finalizes the journal file, so that it is not used for hot
**      rollback at any point in the future.
**
** Finalization of the journal file (task 2) is only performed if the
** rollback is successful.
**
** In WAL mode, all cache-entries containing data modified within the
** current transaction are either expelled from the cache or reverted to
** their pre-transaction state by re-reading data from the database or
** WAL files. The WAL transaction is then closed.
*/

//如果一个写进程是打开的，那么所有的改变都是在事务被恢复之内的，并且当前写-事务是关闭的。如果成功的话，页面落回PAGER_READER状态；否则如果错误发生，页面落回PAGER_ERROR状态。

//另外，在回滚模式，这个函数表现出两个功能：
1)它回滚日志文件，当事务打开的时候，恢复所有数据库文件和内存缓存页面所在的状态
2)它结束日志文件，所以在未来任何点上的热回滚是没有用的。

//如果回滚成功，日志文件的结束（task 2）是唯一表现出来的。

//在WAL模式，所有的缓存条目包括在当前事务之内的数据修改 是从缓存中除去或者通过重读数据库中或WAL文件中的数据 恢复到他们前事务状态。WAL事务接下来就关闭了。

int sqlite3PagerRollback(Pager *pPager){
  int rc = SQLITE_OK;                  /* Return code */
  PAGERTRACE(("ROLLBACK %d\n", PAGERID(pPager)));
 
  /* PagerRollback() is a no-op if called in READER or OPEN state. If
  ** the pager is already in the ERROR state, the rollback is not
  ** attempted here. Instead, the error code is returned to the caller.
  */
  assert( assert_pager_state(pPager) );
  if( pPager->eState==PAGER_ERROR ) return pPager->errCode;
  if( pPager->eState<=PAGER_READER ) return SQLITE_OK;
 
  if( pagerUseWal(pPager) ){
    int rc2;
    rc = sqlite3PagerSavepoint(pPager, SAVEPOINT_ROLLBACK, -1);
    rc2 = pager_end_transaction(pPager, pPager->setMaster);
    if( rc==SQLITE_OK ) rc = rc2;
  }else if( !isOpen(pPager->jfd) || pPager->eState==PAGER_WRITER_LOCKED ){
    int eState = pPager->eState;
    rc = pager_end_transaction(pPager, 0);
    if( !MEMDB && eState>PAGER_WRITER_LOCKED ){
      /* This can happen using journal_mode=off. Move the pager to the error
      ** state to indicate that the contents of the cache may not be trusted.
      ** Any active readers will get SQLITE_ABORT.
      */
      pPager->errCode = SQLITE_ABORT;
      pPager->eState = PAGER_ERROR;
      return rc;
    }
  }else{
    rc = pager_playback(pPager, 0);
  }
 
  assert( pPager->eState==PAGER_READER || rc!=SQLITE_OK );
  assert( rc==SQLITE_OK || rc==SQLITE_FULL
          || rc==SQLITE_NOMEM || (rc&0xFF)==SQLITE_IOERR );
 
  /* If an error occurs during a ROLLBACK, we can no longer trust the pager
  ** cache. So call pager_error() on the way out to make any error persistent.
  */
  return pager_error(pPager, rc);
}
一、概述：
 
    在SQLite中，锁和并发控制机制都是由pager_module模块负责处理的，如ACID(Atomic, Consistent, Isolated, and Durable)。在含有数据修改的事务中，该模块将确保或者所有的数据修改全部提交，或者全部回滚。与此同时，该模块还提供了一些磁盘文件的内存Cache功能。
    事实上，pager_module模块并不关心数据库存储的细节，如B-Tree、编码方式、索引等，它只是将其视为由统一大小(通常为1024字节)的数据块构成的单一文件，其中每个块被称为一个页(page)。在该模块中页的起始编号为1，即第一个页的索引值是1，其后的页编号以此类推。
 
二、文件锁：
 
    在SQLite的当前版本中，主要提供了以下五种方式的文件锁状态。
    1). UNLOCKED：
    文件没有持有任何锁，即当前数据库不存在任何读或写的操作。其它的进程可以在该数据库上执行任意的读写操作。此状态为缺省状态。
    2). SHARED：
    在此状态下，该数据库可以被读取但是不能被写入。在同一时刻可以有任意数量的进程在同一个数据库上持有共享锁，因此读操作是并发的。换句话说，只要有一个或多个共享锁处于活动状态，就不再允许有数据库文件写入的操作存在。
    3). RESERVED：
    假如某个进程在将来的某一时刻打算在当前的数据库中执行写操作，然而此时只是从数据库中读取数据，那么我们就可以简单的理解为数据库文件此时已经拥有了保留锁。当保留锁处于活动状态时，该数据库只能有一个或多个共享锁存在，即同一数据库的同一时刻只能存在一个保留锁和多个共享锁。在Oracle中此类锁被称之为预写锁，不同的是Oracle中锁的粒度可以细化到表甚至到行，因此该种锁在Oracle中对并发的影响程序不像SQLite中这样大。
    4). PENDING：
    PENDING锁的意思是说，某个进程正打算在该数据库上执行写操作，然而此时该数据库中却存在很多共享锁(读操作)，那么该写操作就必须处于等待状态，即等待所有共享锁消失为止，与此同时，新的读操作将不再被允许，以防止写锁饥饿的现象发生。在此等待期间，该数据库文件的锁状态为PENDING，在等到所有共享锁消失以后，PENDING锁状态的数据库文件将在获取排他锁之后进入EXCLUSIVE状态。
    5). EXCLUSIVE：
    在执行写操作之前，该进程必须先获取该数据库的排他锁。然而一旦拥有了排他锁，任何其它锁类型都不能与之共存。因此，为了最大化并发效率，SQLite将会最小化排他锁被持有的时间总量。
 
    最后需要说明的是，和其它关系型数据库相比，如MySQL、Oracle等，SQLite数据库中所有的数据都存储在同一文件中，与此同时，它却仅仅提供了粗粒度的文件锁，因此，SQLite在并发性和伸缩性等方面和其它关系型数据库还是无法比拟的。由此可见，SQLite有其自身的适用场景，就如在本系列开篇中所说，它和其它关系型数据库之间的互换性还是非常有限的。
 
三、回滚日志：
 
    当一个进程要改变数据库文件的时候，它首先将未改变之前的内容记录到回滚日志文件中。如果SQLite中的某一事务正在试图修改多个数据库中的数据，那么此时每一个数据库都将生成一个属于自己的回滚日志文件，用于分别记录属于自己的数据改变，与此同时还要生成一个用于协调多个数据库操作的主数据库日志文件，在主数据库日志文件中将包含各个数据库回滚日志文件的文件名，在每个回滚日志文件中也同样包含了主数据库日志文件的文件名信息。然而对于无需主数据库日志文件的回滚日志文件，其中也会保留主数据库日志文件的信息，只是此时该信息的值为空。
    我们可以将回滚日志视为"HOT"日志文件，因为它的存在就是为了恢复数据库的一致性状态。当某一进程正在更新数据库时，应用程序或OS突然崩溃，这样更新操作就不能顺利完成。因此我们可以说"HOT"日志只有在异常条件下才会生成，如果一切都非常顺利的话，该文件将永远不会存在。
 
四、数据写入：
 
    如果某一进程要想在数据库上执行写操作，那么必须先获取共享锁，在共享锁获取之后再获取保留锁。因为保留锁则预示着在将来某一时刻该进程将会执行写操作，所以在同一时刻只有一个进程可以持有一把保留锁，但是其它进程可以继续持有共享锁以完成数据读取的操作。如果要执行写操作的进程不能获取保留锁，那么这将说明另一进程已经获取了保留锁。在此种情况下，写操作将失败，并立即返回SQLITE_BUSY错误。在成功获取保留锁之后，该写进程将创建回滚日志。
    在对任何数据作出改变之前，写进程会将待修改页中的原有内容先行写入回滚日志文件中，然而，这些数据发生变化的页起初并不会直接写入磁盘文件，而是保留在内存中，这样其它进程就可以继续读取该数据库中的数据了。
    或者是因为内存中的cache已满，或者是应用程序已经提交了事务，最终，写进程将数据更新到数据库文件中。然而在此之前，写进程必须确保没有其它的进程正在读取数据库，同时回滚日志中的数据确实被物理的写入到磁盘文件中，其步骤如下：
    1). 确保所有的回滚日志数据被物理的写入磁盘文件，以便在出现系统崩溃时可以将数据库恢复到一致的状态。
    2). 获取PENDING锁，再获取排他锁，如果此时其它的进程仍然持有共享锁，写入线程将不得不被挂起并等待直到那些共享锁消失之后，才能进而得到排他锁。
    3). 将内存中持有的修改页写出到原有的磁盘文件中。
    如果写入到数据库文件的原因是因为cache已满，那么写入进程将不会立刻提交，而是继续对其它页进行修改。但是在接下来的修改被写入到数据库文件之前，回滚日志必须被再一次写到磁盘中。还要注意的是，写入进程获取到的排他锁必须被一直持有，直到所有的改变被提交时为止。这也意味着，从数据第一次被刷新到磁盘文件开始，直到事务被提交之前，其它的进程不能访问该数据库。
    当写入进程准备提交时，将遵循以下步骤：
    4). 获取排他锁，同时确保所有内存中的变化数据都被写入到磁盘文件中。
    5). 将所有数据库文件的变化数据物理的写入到磁盘中。
    6). 删除日志文件。如果在删除之前出现系统故障，进程在下一次打开该数据库时仍将基于该HOT日志进行恢复操作。因此只有在成功删除日志文件之后，我们才可以认为该事务成功完成。
    7). 从数据库文件中删除所有的排他锁和PENDING锁。
    一旦PENDING锁被释放，其它的进程就可以开始再次读取数据库了。
    如果一个事务中包含多个数据库的修改，那么它的提交逻辑将更为复杂，见如下步骤：
    4). 确保每个数据库文件都已经持有了排他锁和一个有效的日志文件。
    5). 创建主数据库日志文件，同时将每个数据库的回滚日志文件的文件名写入到该主数据库日志文件中。
    6). 再将主数据库日志文件的文件名分别写入到每个数据库回滚日志文件的指定位置中。
    7). 将所有的数据库变化持久化到数据库磁盘文件中。
    8). 删除主日志文件，如果在删除之前出现系统故障，进程在下一次打开该数据库时仍将基于该HOT日志进行恢复操作。因此只有在成功删除主日志文件之后，我们才可以认为该事务成功完成。
    9). 删除每个数据库各自的日志文件。
    10).从所有数据库中删除掉排他锁和PENDING锁。
 
    最后需要说明的是，在SQLite2中，如果多个进程正在从数据库中读取数据，也就是说该数据库始终都有读操作发生，即在每一时刻该数据库都持有至少一把共享锁，这样将会导致没有任何进程可以执行写操作，因为在数据库持有读锁的时候是无法获取写锁的，我们将这种情形称为"写饥饿"。在SQLite3中，通过使用PENDING锁则有效的避免了"写饥饿"情形的发生。当某一进程持有PENDING锁时，已经存在的读操作可以继续进行，直到其正常结束，但是新的读操作将不会再被SQLite接受，所以在已有的读操作全部结束后，持有PENDING锁的进程就可以被激活并试图进一步获取排他锁以完成数据的修改操作。
 
五、SQL级别的事务控制：
 
    SQLite3在实现上确实针对锁和并发控制做出了一些精巧的变化，特别是对于事务这一SQL语言级别的特征。在缺省情况下，SQLite3会将所有的SQL操作置于antocommit模式下，这样所有针对数据库的修改操作都会在SQL命令执行结束后被自动提交。在SQLite中，SQL命令"BEGIN TRANSACTION"用于显式的声明一个事务，即其后的SQL语句在执行后都不会自动提交，而是需要等到SQL命令"COMMIT"或"ROLLBACK"被执行时，才考虑提交还是回滚。由此可以推断出，在BEGIN命令被执行后并没有立即获得任何类型的锁，而是在执行第一个SELECT语句时才得到一个共享锁，或者是在执行第一个DML语句时才获得一个保留锁。至于排它锁，只有在数据从内存写入磁盘时开始，直到事务提交或回滚之前才能持有排它锁。
    如果多个SQL命令在同一个时刻同一个数据库连接中被执行，autocommit将会被延迟执行，直到最后一个命令完成。比如，如果一个SELECT语句正在被执行，在这个命令执行期间，需要返回所有检索出来的行记录，如果此时处理结果集的线程因为业务逻辑的需要被暂时挂起并处于等待状态，而其它的线程此时或许正在该连接上对该数据库执行INSERT、UPDATE或DELETE命令，那么所有这些命令作出的数据修改都必须等到SELECT检索结束后才能被提交。
     
/*
** Check that there are at least nSavepoint savepoints open. If there are
** currently less than nSavepoints open, then open one or more savepoints
** to make up the difference. If the number of savepoints is already
** equal to nSavepoint, then this function is a no-op.
**
** If a memory allocation fails, SQLITE_NOMEM is returned. If an error
** occurs while opening the sub-journal file, then an IO error code is
** returned. Otherwise, SQLITE_OK.
*/

//检查这里至少有nSavepoint保存点开放。如果这里目前不到nSavepoints打开,那么打开一个或多个保存点来弥补差额。如果保存点的数量已经等于nSavepoint,那么这个函数是一个空操作。

//如果内存分配失败,返回SQLITE_NOMEM。如果在打开子日志文件时候一个错误出现,那么返回一个IO错误代码。否则,返回SQLITE_OK。

int sqlite3PagerOpenSavepoint(Pager *pPager, int nSavepoint){
  int rc = SQLITE_OK;                       /* Return code */
  int nCurrent = pPager->nSavepoint;        /* Current number of savepoints */
 
  assert( pPager->eState>=PAGER_WRITER_LOCKED );
  assert( assert_pager_state(pPager) );
 
  if( nSavepoint>nCurrent && pPager->useJournal ){
    int ii;                                 /* Iterator variable */
    PagerSavepoint *aNew;                   /* New Pager.aSavepoint array */
 
    /* Grow the Pager.aSavepoint array using realloc(). Return SQLITE_NOMEM
    ** if the allocation fails. Otherwise, zero the new portion in case a
    ** malloc failure occurs while populating it in the for(...) loop below.
    */
    aNew = (PagerSavepoint *)sqlite3Realloc(
        pPager->aSavepoint, sizeof(PagerSavepoint)*nSavepoint
    );
    if( !aNew ){
      return SQLITE_NOMEM;
    }
    memset(&aNew[nCurrent], 0, (nSavepoint-nCurrent) * sizeof(PagerSavepoint));
    pPager->aSavepoint = aNew;
 
    /* Populate the PagerSavepoint structures just allocated. */
    for(ii=nCurrent; ii<nSavepoint; ii++){
      aNew[ii].nOrig = pPager->dbSize;
      if( isOpen(pPager->jfd) && pPager->journalOff>0 ){
        aNew[ii].iOffset = pPager->journalOff;
      }else{
        aNew[ii].iOffset = JOURNAL_HDR_SZ(pPager);
      }
      aNew[ii].iSubRec = pPager->nSubRec;
      aNew[ii].pInSavepoint = sqlite3BitvecCreate(pPager->dbSize);
      if( !aNew[ii].pInSavepoint ){
        return SQLITE_NOMEM;
      }
      if( pagerUseWal(pPager) ){
        sqlite3WalSavepoint(pPager->pWal, aNew[ii].aWalData);
      }
      pPager->nSavepoint = ii+1;
    }
    assert( pPager->nSavepoint==nSavepoint );
    assertTruncateConstraint(pPager);
  }
 
  return rc;
}
 
/*
** This function is called to rollback or release (commit) a savepoint.
** The savepoint to release or rollback need not be the most recently
** created savepoint.
**
** Parameter op is always either SAVEPOINT_ROLLBACK or SAVEPOINT_RELEASE.
** If it is SAVEPOINT_RELEASE, then release and destroy the savepoint with
** index iSavepoint. If it is SAVEPOINT_ROLLBACK, then rollback all changes
** that have occurred since the specified savepoint was created.
**
** The savepoint to rollback or release is identified by parameter
** iSavepoint. A value of 0 means to operate on the outermost savepoint
** (the first created). A value of (Pager.nSavepoint-1) means operate
** on the most recently created savepoint. If iSavepoint is greater than
** (Pager.nSavepoint-1), then this function is a no-op.
**
** If a negative value is passed to this function, then the current
** transaction is rolled back. This is different to calling
** sqlite3PagerRollback() because this function does not terminate
** the transaction or unlock the database, it just restores the
** contents of the database to its original state.
**
** In any case, all savepoints with an index greater than iSavepoint
** are destroyed. If this is a release operation (op==SAVEPOINT_RELEASE),
** then savepoint iSavepoint is also destroyed.
**
** This function may return SQLITE_NOMEM if a memory allocation fails,
** or an IO error code if an IO error occurs while rolling back a
** savepoint. If no errors occur, SQLITE_OK is returned.
*/

//这个函数被调用来回滚或释放(提交)保存点。释放或回滚的savepoint需要不是最近创建的保存点。

//参数op是SAVEPOINT_ROLLBACK或SAVEPOINT_RELEASE。如果是SAVEPOINT_RELEASE,那么用索引iSavepoint释放并销毁保存点；如果是SAVEPOINT_ROLLBACK,然后回滚所有在创建指定的保存点以来发生的更改

//回滚或者释放的保存点是被参数iSavepoint识别的。一个0值意味着在最外面（首先被创建）的savepoint上操作。一个Pager.nSavepoint -1的值意味着在大多数最近创建的savepoint上操作。如果iSavepoint是比（Pager.nSavepoint -1）更大，那么这个函数是无操作的。

//如果一个负值被传递到这个函数中，那么当前的事务被回滚。这与调用sqlite3PagerRollback()是不同的，因为这个函数不会终结事务或者解锁数据库，他只是恢复数据库内容到他的原始状态。

//在任何情况下，所有的有比iSavepoint大的索引的保存点都被销毁了。如果这里是一个释放操作（操作==SAVEPOINT_RELEASE），那么保存点iSavepoint也被销毁。

//如果一个内存分配失败，那么这个函数可能返回SQLITE_NOMEM；或者如果在回滚一个savepoint时发生一个IO错误，那么函数可能返回一个IO错误代码；如果没有错误发生，返回SQLITE_OK

int sqlite3PagerSavepoint(Pager *pPager, int op, int iSavepoint){
  int rc = pPager->errCode;       /* Return code */
 
  assert( op==SAVEPOINT_RELEASE || op==SAVEPOINT_ROLLBACK );
  assert( iSavepoint>=0 || op==SAVEPOINT_ROLLBACK );
 
  if( rc==SQLITE_OK && iSavepoint<pPager->nSavepoint ){
    int ii;            /* Iterator variable */
    int nNew;          /* Number of remaining savepoints after this op. */
 
    /* Figure out how many savepoints will still be active after this
    ** operation. Store this value in nNew. Then free resources associated
    ** with any savepoints that are destroyed by this operation.
    */
    nNew = iSavepoint + (( op==SAVEPOINT_RELEASE ) ? 0 : 1);
    for(ii=nNew; ii<pPager->nSavepoint; ii++){
      sqlite3BitvecDestroy(pPager->aSavepoint[ii].pInSavepoint);
    }
    pPager->nSavepoint = nNew;
 
    /* If this is a release of the outermost savepoint, truncate
    ** the sub-journal to zero bytes in size. */
    if( op==SAVEPOINT_RELEASE ){
      if( nNew==0 && isOpen(pPager->sjfd) ){
        /* Only truncate if it is an in-memory sub-journal. */
        if( sqlite3IsMemJournal(pPager->sjfd) ){
          rc = sqlite3OsTruncate(pPager->sjfd, 0);
          assert( rc==SQLITE_OK );
        }
        pPager->nSubRec = 0;
      }
    }
    /* Else this is a rollback operation, playback the specified savepoint.
    ** If this is a temp-file, it is possible that the journal file has
    ** not yet been opened. In this case there have been no changes to
    ** the database file, so the playback operation can be skipped.
    */
    else if( pagerUseWal(pPager) || isOpen(pPager->jfd) ){
      PagerSavepoint *pSavepoint = (nNew==0)?0:&pPager->aSavepoint[nNew-1];
      rc = pagerPlaybackSavepoint(pPager, pSavepoint);
      assert(rc!=SQLITE_DONE);
    }
  }
 
  return rc;
}
 
/*
** This function is called to obtain a shared lock on the database file.
** It is illegal to call sqlite3PagerAcquire() until after this function
** has been successfully called. If a shared-lock is already held when
** this function is called, it is a no-op.
**
** The following operations are also performed by this function.
**
**   1) If the pager is currently in PAGER_OPEN state (no lock held
**      on the database file), then an attempt is made to obtain a
**      SHARED lock on the database file. Immediately after obtaining
**      the SHARED lock, the file-system is checked for a hot-journal,
**      which is played back if present. Following any hot-journal
**      rollback, the contents of the cache are validated by checking
**      the 'change-counter' field of the database file header and
**      discarded if they are found to be invalid.
**
**   2) If the pager is running in exclusive-mode, and there are currently
**      no outstanding references to any pages, and is in the error state,
**      then an attempt is made to clear the error state by discarding
**      the contents of the page cache and rolling back any open journal
**      file.
**
** If everything is successful, SQLITE_OK is returned. If an IO error
** occurs while locking the database, checking for a hot-journal file or
** rolling back a journal file, the IO error code is returned.
*/

//这个函数被调用时,获得一个在数据库文件上的共享锁。调用sqlite3PagerAcquire()是非法的，直到这个函数被成功调用之后。如果当这个函数被调用时一个共享锁已经被加上了，函数无操作的。

//下面的操作也由这个函数执行：
1)如果页面当前在PAGER_OPEN状态(在数据库文件上无锁),然后试图在数据库文件上获得一个共享锁定。立即获得共享锁之后,文件系统检查热门日志,如果存在的话那就回放。以下任何热门日志回滚,缓存的内容被检查数据库文件头的‘change-counter’领域所验证，并且如果发现时无效的话，丢弃缓存内容。
2)如果页面在独占（排他）模式运行,并且目前对于任何页面来说，没有未使用的引用,并且处于错误状态下,那么通过丢弃页面缓存内容和回滚任何打开的日志文件来清除错误状态。

//如果一切成功,返回SQLITE_OK。如果当锁定数据库的时候一个IO错误发生，检查热门日志文件或者回滚一个日志文件,返回IO错误代码。

int sqlite3PagerSharedLock(Pager *pPager){
  int rc = SQLITE_OK;                /* Return code */
 
  /* This routine is only called from b-tree and only when there are no
  ** outstanding pages. This implies that the pager state should either
  ** be OPEN or READER. READER is only possible if the pager is or was in
  ** exclusive access mode.
  */
  assert( sqlite3PcacheRefCount(pPager->pPCache)==0 );
  assert( assert_pager_state(pPager) );
  assert( pPager->eState==PAGER_OPEN || pPager->eState==PAGER_READER );
  if( NEVER(MEMDB && pPager->errCode) ){ return pPager->errCode; }
 
  if( !pagerUseWal(pPager) && pPager->eState==PAGER_OPEN ){
    int bHotJournal = 1;          /* True if there exists a hot journal-file */
 
    assert( !MEMDB );
 
    rc = pager_wait_on_lock(pPager, SHARED_LOCK);
    if( rc!=SQLITE_OK ){
      assert( pPager->eLock==NO_LOCK || pPager->eLock==UNKNOWN_LOCK );
      goto failed;
    }
 
    /* If a journal file exists, and there is no RESERVED lock on the
    ** database file, then it either needs to be played back or deleted.
    */
    if( pPager->eLock<=SHARED_LOCK ){
      rc = hasHotJournal(pPager, &bHotJournal);
    }
    if( rc!=SQLITE_OK ){
      goto failed;
    }
    if( bHotJournal ){
      /* Get an EXCLUSIVE lock on the database file. At this point it is
      ** important that a RESERVED lock is not obtained on the way to the
      ** EXCLUSIVE lock. If it were, another process might open the
      ** database file, detect the RESERVED lock, and conclude that the
      ** database is safe to read while this process is still rolling the
      ** hot-journal back.
      **
      ** Because the intermediate RESERVED lock is not requested, any
      ** other process attempting to access the database file will get to
      ** this point in the code and fail to obtain its own EXCLUSIVE lock
      ** on the database file.
      **
      ** Unless the pager is in locking_mode=exclusive mode, the lock is
      ** downgraded to SHARED_LOCK before this function returns.
      */
      rc = pagerLockDb(pPager, EXCLUSIVE_LOCK);
      if( rc!=SQLITE_OK ){
        goto failed;
      }
 
      /* If it is not already open and the file exists on disk, open the
      ** journal for read/write access. Write access is required because
      ** in exclusive-access mode the file descriptor will be kept open
      ** and possibly used for a transaction later on. Also, write-access
      ** is usually required to finalize the journal in journal_mode=persist
      ** mode (and also for journal_mode=truncate on some systems).
      **
      ** If the journal does not exist, it usually means that some
      ** other connection managed to get in and roll it back before
      ** this connection obtained the exclusive lock above. Or, it
      ** may mean that the pager was in the error-state when this
      ** function was called and the journal file does not exist.
      */
      if( !isOpen(pPager->jfd) ){
        sqlite3_vfs * const pVfs = pPager->pVfs;
        int bExists;              /* True if journal file exists */
        rc = sqlite3OsAccess(
            pVfs, pPager->zJournal, SQLITE_ACCESS_EXISTS, &bExists);
        if( rc==SQLITE_OK && bExists ){
          int fout = 0;
          int f = SQLITE_OPEN_READWRITE|SQLITE_OPEN_MAIN_JOURNAL;
          assert( !pPager->tempFile );
          rc = sqlite3OsOpen(pVfs, pPager->zJournal, pPager->jfd, f, &fout);
          assert( rc!=SQLITE_OK || isOpen(pPager->jfd) );
          if( rc==SQLITE_OK && fout&SQLITE_OPEN_READONLY ){
            rc = SQLITE_CANTOPEN_BKPT;
            sqlite3OsClose(pPager->jfd);
          }
        }
      }
 
      /* Playback and delete the journal.  Drop the database write
      ** lock and reacquire the read lock. Purge the cache before
      ** playing back the hot-journal so that we don't end up with
      ** an inconsistent cache.  Sync the hot journal before playing
      ** it back since the process that crashed and left the hot journal
      ** probably did not sync it and we are required to always sync
      ** the journal before playing it back.
      */
      if( isOpen(pPager->jfd) ){
        assert( rc==SQLITE_OK );
        rc = pagerSyncHotJournal(pPager);
        if( rc==SQLITE_OK ){
          rc = pager_playback(pPager, 1);
          pPager->eState = PAGER_OPEN;
        }
      }else if( !pPager->exclusiveMode ){
        pagerUnlockDb(pPager, SHARED_LOCK);
      }
 
      if( rc!=SQLITE_OK ){
        /* This branch is taken if an error occurs while trying to open
        ** or roll back a hot-journal while holding an EXCLUSIVE lock. The
        ** pager_unlock() routine will be called before returning to unlock
        ** the file. If the unlock attempt fails, then Pager.eLock must be
        ** set to UNKNOWN_LOCK (see the comment above the #define for
        ** UNKNOWN_LOCK above for an explanation).
        **
        ** In order to get pager_unlock() to do this, set Pager.eState to
        ** PAGER_ERROR now. This is not actually counted as a transition
        ** to ERROR state in the state diagram at the top of this file,
        ** since we know that the same call to pager_unlock() will very
        ** shortly transition the pager object to the OPEN state. Calling
        ** assert_pager_state() would fail now, as it should not be possible
        ** to be in ERROR state when there are zero outstanding page
        ** references.
        */
        pager_error(pPager, rc);
        goto failed;
      }
 
      assert( pPager->eState==PAGER_OPEN );
      assert( (pPager->eLock==SHARED_LOCK)
           || (pPager->exclusiveMode && pPager->eLock>SHARED_LOCK)
      );
    }
 
    if( !pPager->tempFile
     && (pPager->pBackup || sqlite3PcachePagecount(pPager->pPCache)>0)
    ){
      /* The shared-lock has just been acquired on the database file
      ** and there are already pages in the cache (from a previous
      ** read or write transaction).  Check to see if the database
      ** has been modified.  If the database has changed, flush the
      ** cache.
      **
      ** Database changes is detected by looking at 15 bytes beginning
      ** at offset 24 into the file.  The first 4 of these 16 bytes are
      ** a 32-bit counter that is incremented with each change.  The
      ** other bytes change randomly with each file change when
      ** a codec is in use.
      **
      ** There is a vanishingly small chance that a change will not be
      ** detected.  The chance of an undetected change is so small that
      ** it can be neglected.
      */
      Pgno nPage = 0;
      char dbFileVers[sizeof(pPager->dbFileVers)];
 
      rc = pagerPagecount(pPager, &nPage);
      if( rc ) goto failed;
 
      if( nPage>0 ){
        IOTRACE(("CKVERS %p %d\n", pPager, sizeof(dbFileVers)));
        rc = sqlite3OsRead(pPager->fd, &dbFileVers, sizeof(dbFileVers), 24);
        if( rc!=SQLITE_OK ){
          goto failed;
        }
      }else{
        memset(dbFileVers, 0, sizeof(dbFileVers));
      }
 
      if( memcmp(pPager->dbFileVers, dbFileVers, sizeof(dbFileVers))!=0 ){
        pager_reset(pPager);
      }
    }
 
    /* If there is a WAL file in the file-system, open this database in WAL
    ** mode. Otherwise, the following function call is a no-op.
    */
    rc = pagerOpenWalIfPresent(pPager);
#ifndef SQLITE_OMIT_WAL
    assert( pPager->pWal==0 || rc==SQLITE_OK );
#endif
  }
 
  if( pagerUseWal(pPager) ){
    assert( rc==SQLITE_OK );
    rc = pagerBeginReadTransaction(pPager);
  }
 
  if( pPager->eState==PAGER_OPEN && rc==SQLITE_OK ){
    rc = pagerPagecount(pPager, &pPager->dbSize);
  }
 
failed:
  if( rc!=SQLITE_OK ){
    assert( !MEMDB );
    pager_unlock(pPager);
    assert( pPager->eState==PAGER_OPEN );
  }else{
    pPager->eState = PAGER_READER;
  }
  return rc;
}
 
/*
** This function is called to obtain a shared lock on the database file.
** It is illegal to call sqlite3PagerAcquire() until after this function
** has been successfully called. If a shared-lock is already held when
** this function is called, it is a no-op.
**
** The following operations are also performed by this function.
**
**   1) If the pager is currently in PAGER_OPEN state (no lock held
**      on the database file), then an attempt is made to obtain a
**      SHARED lock on the database file. Immediately after obtaining
**      the SHARED lock, the file-system is checked for a hot-journal,
**      which is played back if present. Following any hot-journal
**      rollback, the contents of the cache are validated by checking
**      the 'change-counter' field of the database file header and
**      discarded if they are found to be invalid.
**
**   2) If the pager is running in exclusive-mode, and there are currently
**      no outstanding references to any pages, and is in the error state,
**      then an attempt is made to clear the error state by discarding
**      the contents of the page cache and rolling back any open journal
**      file.
**
** If everything is successful, SQLITE_OK is returned. If an IO error
** occurs while locking the database, checking for a hot-journal file or
** rolling back a journal file, the IO error code is returned.
*/

//这个函数被调用时,获得一个在数据库文件上的共享锁。调用sqlite3PagerAcquire()是非法的，直到这个函数被成功调用之后。如果当这个函数被调用时一个共享锁已经被加上了，函数无操作的。

//下面的操作也由这个函数执行：
1)如果页面当前在PAGER_OPEN状态(在数据库文件上无锁),然后试图在数据库文件上获得一个共享锁定。立即获得共享锁之后,文件系统检查热门日志,如果存在的话那就回放。以下任何热门日志回滚,缓存的内容被检查数据库文件头的‘change-counter’领域所验证，并且如果发现时无效的话，丢弃缓存内容。
2)如果页面在独占（排他）模式运行,并且目前对于任何页面来说，没有未使用的引用,并且处于错误状态下,那么通过丢弃页面缓存内容和回滚任何打开的日志文件来清除错误状态。

//如果一切成功,返回SQLITE_OK。如果当锁定数据库的时候一个IO错误发生，检查热门日志文件或者回滚一个日志文件,返回IO错误代码。

int sqlite3PagerSharedLock(Pager *pPager){
  int rc = SQLITE_OK;                /* Return code */
 
  /* This routine is only called from b-tree and only when there are no
  ** outstanding pages. This implies that the pager state should either
  ** be OPEN or READER. READER is only possible if the pager is or was in
  ** exclusive access mode.
  */
  assert( sqlite3PcacheRefCount(pPager->pPCache)==0 );
  assert( assert_pager_state(pPager) );
  assert( pPager->eState==PAGER_OPEN || pPager->eState==PAGER_READER );
  if( NEVER(MEMDB && pPager->errCode) ){ return pPager->errCode; }
 
  if( !pagerUseWal(pPager) && pPager->eState==PAGER_OPEN ){
    int bHotJournal = 1;          /* True if there exists a hot journal-file */
 
    assert( !MEMDB );
 
    rc = pager_wait_on_lock(pPager, SHARED_LOCK);
    if( rc!=SQLITE_OK ){
      assert( pPager->eLock==NO_LOCK || pPager->eLock==UNKNOWN_LOCK );
      goto failed;
    }
 
    /* If a journal file exists, and there is no RESERVED lock on the
    ** database file, then it either needs to be played back or deleted.
    */
    if( pPager->eLock<=SHARED_LOCK ){
      rc = hasHotJournal(pPager, &bHotJournal);
    }
    if( rc!=SQLITE_OK ){
      goto failed;
    }
    if( bHotJournal ){
      /* Get an EXCLUSIVE lock on the database file. At this point it is
      ** important that a RESERVED lock is not obtained on the way to the
      ** EXCLUSIVE lock. If it were, another process might open the
      ** database file, detect the RESERVED lock, and conclude that the
      ** database is safe to read while this process is still rolling the
      ** hot-journal back.
      **
      ** Because the intermediate RESERVED lock is not requested, any
      ** other process attempting to access the database file will get to
      ** this point in the code and fail to obtain its own EXCLUSIVE lock
      ** on the database file.
      **
      ** Unless the pager is in locking_mode=exclusive mode, the lock is
      ** downgraded to SHARED_LOCK before this function returns.
      */
      rc = pagerLockDb(pPager, EXCLUSIVE_LOCK);
      if( rc!=SQLITE_OK ){
        goto failed;
      }
 
      /* If it is not already open and the file exists on disk, open the
      ** journal for read/write access. Write access is required because
      ** in exclusive-access mode the file descriptor will be kept open
      ** and possibly used for a transaction later on. Also, write-access
      ** is usually required to finalize the journal in journal_mode=persist
      ** mode (and also for journal_mode=truncate on some systems).
      **
      ** If the journal does not exist, it usually means that some
      ** other connection managed to get in and roll it back before
      ** this connection obtained the exclusive lock above. Or, it
      ** may mean that the pager was in the error-state when this
      ** function was called and the journal file does not exist.
      */
      if( !isOpen(pPager->jfd) ){
        sqlite3_vfs * const pVfs = pPager->pVfs;
        int bExists;              /* True if journal file exists */
        rc = sqlite3OsAccess(
            pVfs, pPager->zJournal, SQLITE_ACCESS_EXISTS, &bExists);
        if( rc==SQLITE_OK && bExists ){
          int fout = 0;
          int f = SQLITE_OPEN_READWRITE|SQLITE_OPEN_MAIN_JOURNAL;
          assert( !pPager->tempFile );
          rc = sqlite3OsOpen(pVfs, pPager->zJournal, pPager->jfd, f, &fout);
          assert( rc!=SQLITE_OK || isOpen(pPager->jfd) );
          if( rc==SQLITE_OK && fout&SQLITE_OPEN_READONLY ){
            rc = SQLITE_CANTOPEN_BKPT;
            sqlite3OsClose(pPager->jfd);
          }
        }
      }
 
      /* Playback and delete the journal.  Drop the database write
      ** lock and reacquire the read lock. Purge the cache before
      ** playing back the hot-journal so that we don't end up with
      ** an inconsistent cache.  Sync the hot journal before playing
      ** it back since the process that crashed and left the hot journal
      ** probably did not sync it and we are required to always sync
      ** the journal before playing it back.
      */
      if( isOpen(pPager->jfd) ){
        assert( rc==SQLITE_OK );
        rc = pagerSyncHotJournal(pPager);
        if( rc==SQLITE_OK ){
          rc = pager_playback(pPager, 1);
          pPager->eState = PAGER_OPEN;
        }
      }else if( !pPager->exclusiveMode ){
        pagerUnlockDb(pPager, SHARED_LOCK);
      }
 
      if( rc!=SQLITE_OK ){
        /* This branch is taken if an error occurs while trying to open
        ** or roll back a hot-journal while holding an EXCLUSIVE lock. The
        ** pager_unlock() routine will be called before returning to unlock
        ** the file. If the unlock attempt fails, then Pager.eLock must be
        ** set to UNKNOWN_LOCK (see the comment above the #define for
        ** UNKNOWN_LOCK above for an explanation).
        **
        ** In order to get pager_unlock() to do this, set Pager.eState to
        ** PAGER_ERROR now. This is not actually counted as a transition
        ** to ERROR state in the state diagram at the top of this file,
        ** since we know that the same call to pager_unlock() will very
        ** shortly transition the pager object to the OPEN state. Calling
        ** assert_pager_state() would fail now, as it should not be possible
        ** to be in ERROR state when there are zero outstanding page
        ** references.
        */
        pager_error(pPager, rc);
        goto failed;
      }
 
      assert( pPager->eState==PAGER_OPEN );
      assert( (pPager->eLock==SHARED_LOCK)
           || (pPager->exclusiveMode && pPager->eLock>SHARED_LOCK)
      );
    }
 
    if( !pPager->tempFile
     && (pPager->pBackup || sqlite3PcachePagecount(pPager->pPCache)>0)
    ){
      /* The shared-lock has just been acquired on the database file
      ** and there are already pages in the cache (from a previous
      ** read or write transaction).  Check to see if the database
      ** has been modified.  If the database has changed, flush the
      ** cache.
      **
      ** Database changes is detected by looking at 15 bytes beginning
      ** at offset 24 into the file.  The first 4 of these 16 bytes are
      ** a 32-bit counter that is incremented with each change.  The
      ** other bytes change randomly with each file change when
      ** a codec is in use.
      **
      ** There is a vanishingly small chance that a change will not be
      ** detected.  The chance of an undetected change is so small that
      ** it can be neglected.
      */
      Pgno nPage = 0;
      char dbFileVers[sizeof(pPager->dbFileVers)];
 
      rc = pagerPagecount(pPager, &nPage);
      if( rc ) goto failed;
 
      if( nPage>0 ){
        IOTRACE(("CKVERS %p %d\n", pPager, sizeof(dbFileVers)));
        rc = sqlite3OsRead(pPager->fd, &dbFileVers, sizeof(dbFileVers), 24);
        if( rc!=SQLITE_OK ){
          goto failed;
        }
      }else{
        memset(dbFileVers, 0, sizeof(dbFileVers));
      }
 
      if( memcmp(pPager->dbFileVers, dbFileVers, sizeof(dbFileVers))!=0 ){
        pager_reset(pPager);
      }
    }
 
    /* If there is a WAL file in the file-system, open this database in WAL
    ** mode. Otherwise, the following function call is a no-op.
    */
    rc = pagerOpenWalIfPresent(pPager);
#ifndef SQLITE_OMIT_WAL
    assert( pPager->pWal==0 || rc==SQLITE_OK );
#endif
  }
 
  if( pagerUseWal(pPager) ){
    assert( rc==SQLITE_OK );
    rc = pagerBeginReadTransaction(pPager);
  }
 
  if( pPager->eState==PAGER_OPEN && rc==SQLITE_OK ){
    rc = pagerPagecount(pPager, &pPager->dbSize);
  }
 
failed:
  if( rc!=SQLITE_OK ){
    assert( !MEMDB );
    pager_unlock(pPager);
    assert( pPager->eState==PAGER_OPEN );
  }else{
    pPager->eState = PAGER_READER;
  }
  return rc;
}
 
/*
** Return true if the underlying VFS for the given pager supports the
** primitives necessary for write-ahead logging.
*/

//如果对于给定页面的底层VFS支持对于写前日志很必要的原语，返回真
int sqlite3PagerWalSupported(Pager *pPager){
  const sqlite3_io_methods *pMethods = pPager->fd->pMethods;
  return pPager->exclusiveMode || (pMethods->iVersion>=2 && pMethods->xShmMap);
}
 
int sqlite3PagerWalCallback(Pager *pPager){
  return sqlite3WalCallback(pPager->pWal);
}

/*
** Call sqlite3WalOpen() to open the WAL handle. If the pager is in 
** exclusive-locking mode when this function is called, take an EXCLUSIVE
** lock on the database file and use heap-memory to store the wal-index
** in. Otherwise, use the normal shared-memory.
*/

//调用sqlite3WalOpen()打开WAL操作。如果调用这个函数时页面在exclusive-locking模式,在数据库文件上采取排他锁，并且用堆内存来存储wal-index。否则,使用正常的共享内存（shared-memory）。

static int pagerOpenWal(Pager *pPager){
  int rc = SQLITE_OK;

  assert( pPager->pWal==0 && pPager->tempFile==0 );
  assert( pPager->eLock==SHARED_LOCK || pPager->eLock==EXCLUSIVE_LOCK );

  /* If the pager is already in exclusive-mode, the WAL module will use 
  ** heap-memory for the wal-index instead of the VFS shared-memory 
  ** implementation. Take the exclusive lock now, before opening the WAL
  ** file, to make sure this is safe.
  */
  if( pPager->exclusiveMode ){
    rc = pagerExclusiveLock(pPager);
  }

  /* Open the connection to the log file. If this operation fails, 
  ** (e.g. due to malloc() failure), return an error code.
  */
  if( rc==SQLITE_OK ){
    rc = sqlite3WalOpen(pPager->pVfs, 
        pPager->fd, pPager->zWal, pPager->exclusiveMode,
        pPager->journalSizeLimit, &pPager->pWal
    );
  }

  return rc;
}
 
/*
** This function is called to close the connection to the log file prior
** to switching from WAL to rollback mode.
**
** Before closing the log file, this function attempts to take an
** EXCLUSIVE lock on the database file. If this cannot be obtained, an
** error (SQLITE_BUSY) is returned and the log connection is not closed.
** If successful, the EXCLUSIVE lock is not released before returning.
*/

//这个函数被调用来关闭对于日志文件的连接，在从WAL转换到回滚模式之前。
//在关闭日志文件之前，这个函数试图在数据库文件上采取一个排他锁。如果不能获取，一个错误（SQLITE_BUSY）被返回并且日志连接没有关闭。如果成功，排他锁在返回之前不释放。

int sqlite3PagerCloseWal(Pager *pPager){
  int rc = SQLITE_OK;
 
  assert( pPager->journalMode==PAGER_JOURNALMODE_WAL );
 
  /* If the log file is not already open, but does exist in the file-system,
  ** it may need to be checkpointed before the connection can switch to
  ** rollback mode. Open it now so this can happen.
  */
  if( !pPager->pWal ){
    int logexists = 0;
    rc = pagerLockDb(pPager, SHARED_LOCK);
    if( rc==SQLITE_OK ){
      rc = sqlite3OsAccess(
          pPager->pVfs, pPager->zWal, SQLITE_ACCESS_EXISTS, &logexists
      );
    }
    if( rc==SQLITE_OK && logexists ){
      rc = pagerOpenWal(pPager);
    }
  }
 
  /* Checkpoint and close the log. Because an EXCLUSIVE lock is held on
  ** the database file, the log and log-summary files will be deleted.
  */
  if( rc==SQLITE_OK && pPager->pWal ){
    rc = pagerExclusiveLock(pPager);
    if( rc==SQLITE_OK ){
      rc = sqlite3WalClose(pPager->pWal, pPager->ckptSyncFlags,
                           pPager->pageSize, (u8*)pPager->pTmpSpace);
      pPager->pWal = 0;
    }
  }
  return rc;
}
/*
** A read-lock must be held on the pager when this function is called. If
** the pager is in WAL mode and the WAL file currently contains one or more
** frames, return the size in bytes of the page images stored within the
** WAL frames. Otherwise, if this is not a WAL database or the WAL file
** is empty, return 0.
*/

//当这个函数被调用的时候，一个读锁一定在页面上。如果页面在WAL模式并且WAL文件当前包含一个或者多个框架，返回存储在WAL框架中的页面图像大小的比特数。另外，如果这不是一个WAL数据库或者WAL文件为空的话，返回0.
int sqlite3PagerWalFramesize(Pager *pPager){
  assert( pPager->eState==PAGER_READER );
  return sqlite3WalFramesize(pPager->pWal);
}
 
/*
** Return TRUE if the database file is opened read-only.  Return FALSE
** if the database is (in theory) writable.
*/

//如果数据库文件打开是只读的就返回真。如果数据库是理论上可写的，返回假。
u8 sqlite3PagerIsreadonly(Pager *pPager){
  return pPager->readOnly;
}

/*
** Return the number of references to the pager.
*/
//返回页面引用数
int sqlite3PagerRefcount(Pager *pPager){
  return sqlite3PcacheRefCount(pPager->pPCache);
}
 
/*
** Return the approximate number of bytes of memory currently
** used by the pager and its associated cache.
*/
//返回当前被页面使用的内存近似比特数和他的关联缓存
int sqlite3PagerMemUsed(Pager *pPager){
  int perPageSize = pPager->pageSize + pPager->nExtra + sizeof(PgHdr)
                                     + 5*sizeof(void*);
  return perPageSize*sqlite3PcachePagecount(pPager->pPCache)
           + sqlite3MallocSize(pPager)
           + pPager->pageSize;
}

/*
** Return the full pathname of the database file.
**
** Except, if the pager is in-memory only, then return an empty string if
** nullIfMemDb is true.  This routine is called with nullIfMemDb==1 when
** used to report the filename to the user, for compatibility with legacy
** behavior.  But when the Btree needs to know the filename for matching to
** shared cache, it uses nullIfMemDb==0 so that in-memory databases can
** participate in shared-cache.
*/
//返回数据库文件的完整路径名。

//除外,如果页面只在内存中,那么返回一个空字符串如果nullIfMemDb为真。当被用来向用户报告文件名时，例程被用nullIfMemDb = = 1调用，对于遗产行为的兼容。但是当Btree需要知道文件名来匹配共享缓存，它用nullIfMemDb==0，所以内存数据库可以参与到shared-cache中。

const char *sqlite3PagerFilename(Pager *pPager, int nullIfMemDb){
  return (nullIfMemDb && pPager->memDb) ? "" : pPager->zFilename;
}


/*
** Return the VFS structure for the pager.
*/
//对于页面返回VFS结构

const sqlite3_vfs *sqlite3PagerVfs(Pager *pPager){
  return pPager->pVfs;
}


/*
** Return the file handle for the database file associated
** with the pager.  This might return NULL if the file has
** not yet been opened.
*/

//返回与页面相关的数据库文件文件操作。如果文件至今未被打开，这可能返回NULL。
sqlite3_file *sqlite3PagerFile(Pager *pPager){
  return pPager->fd;
}

/*
** Return the full pathname of the journal file.
*/
//返回日志文件的完整文件名
const char *sqlite3PagerJournalname(Pager *pPager){
  return pPager->zJournal;
}


/*
** Return true if fsync() calls are disabled for this pager.  Return FALSE
** if fsync()s are executed normally.
*/

//如果fsync()调用对于这个页面是不可用的，返回真。如果fsync()正常运行则返回假。
int sqlite3PagerNosync(Pager *pPager){
  return pPager->noSync;
}

/*
** Return a pointer to the "temporary page" buffer held internally
** by the pager.  This is a buffer that is big enough to hold the
** entire content of a database page.  This buffer is used internally
** during rollback and will be overwritten whenever a rollback
** occurs.  But other modules are free to use it too, as long as
** no rollbacks are happening.
*/
//返回一个指向页面内部举行的“临时页面”缓冲的指针。这个缓冲区足够大来容纳一个数据库页面的整个内容。这个缓冲区在整个回滚期间是内部的运用的，并且无论何时一个回滚发生的时候都将被重写。但是其他模块也可以自由的运用，只要没有回滚发生。

void *sqlite3PagerTempSpace(Pager *pPager){
  return pPager->pTmpSpace;
}

/*
** Return true if this is an in-memory pager.
*/
//如果这是一个内存页面的话返回真
int sqlite3PagerIsMemdb(Pager *pPager){
  return MEMDB;
}


/*
** Parameter eStat must be either SQLITE_DBSTATUS_CACHE_HIT or
** SQLITE_DBSTATUS_CACHE_MISS. Before returning, *pnVal is incremented by the
** current cache hit or miss count, according to the value of eStat. If the 
** reset parameter is non-zero, the cache hit or miss count is zeroed before 
** returning.
*/
//参数eStat一定是SQLITE_DBSTATUS_CACHE_HIT或者SQLITE_DBSTATUS_CACHE_MISS。在返回之前，*pnVal是通过当前缓存命中或者未命中计数来增加，通过eStat的值。如果重置参数是非零，缓存命中或者未命中在返回前都是0.

void sqlite3PagerCacheStat(Pager *pPager, int eStat, int reset, int *pnVal){

  assert( eStat==SQLITE_DBSTATUS_CACHE_HIT
       || eStat==SQLITE_DBSTATUS_CACHE_MISS
       || eStat==SQLITE_DBSTATUS_CACHE_WRITE
  );

  assert( SQLITE_DBSTATUS_CACHE_HIT+1==SQLITE_DBSTATUS_CACHE_MISS );
  assert( SQLITE_DBSTATUS_CACHE_HIT+2==SQLITE_DBSTATUS_CACHE_WRITE );
  assert( PAGER_STAT_HIT==0 && PAGER_STAT_MISS==1 && PAGER_STAT_WRITE==2 );

  *pnVal += pPager->aStat[eStat - SQLITE_DBSTATUS_CACHE_HIT];
  if( reset ){
    pPager->aStat[eStat - SQLITE_DBSTATUS_CACHE_HIT] = 0;
  }
}

/*
** Unless this is an in-memory or temporary database, clear the pager cache.
*/
//除非这是一个内存或者临时数据库，清除页面缓存
void sqlite3PagerClearCache(Pager *pPager){
  if( !MEMDB && pPager->tempFile==0 ) pager_reset(pPager);
}


/*
** Truncate the in-memory database file image to nPage pages. This 
** function does not actually modify the database file on disk. It 
** just sets the internal state of the pager object so that the 
** truncation will be done when the current transaction is committed.
*/
//截断内存数据库文件图像到nPage页面上。这个功能不是真的修改磁盘上的数据库文件。这只是设置页面对象的内部状态，这样当当前事务被提交的时候，截断将被完成。
void sqlite3PagerTruncateImage(Pager *pPager, Pgno nPage){
  assert( pPager->dbSize>=nPage );
  assert( pPager->eState>=PAGER_WRITER_CACHEMOD );
  pPager->dbSize = nPage;
  assertTruncateConstraint(pPager);
}

/*
** This function is called by the wal module when writing page content
** into the log file.
**
** This function returns a pointer to a buffer containing the encrypted
** page content. If a malloc fails, this function may return NULL.
*/

//这个功能是被wal模块调用的，当把页面内容写到日志文件中
void *sqlite3PagerCodec(PgHdr *pPg){
  void *aData = 0;
  CODEC2(pPg->pPager, pPg->pData, pPg->pgno, 6, return 0, aData);
  return aData;
}
/*
** Return the page number for page pPg.
*/

//返回页面pPg的页码
Pgno sqlite3PagerPagenumber(DbPage *pPg){
  return pPg->pgno;
}
int sqlite3PagerIswriteable(DbPage *pPg){
  return pPg->flags&PGHDR_DIRTY;
}
/*
** This routine is used for testing and analysis only.
*/
//这个例程只被用作测试和分析
int *sqlite3PagerStats(Pager *pPager){
  static int a[11];
  a[0] = sqlite3PcacheRefCount(pPager->pPCache);
  a[1] = sqlite3PcachePagecount(pPager->pPCache);
  a[2] = sqlite3PcacheGetCachesize(pPager->pPCache);
  a[3] = pPager->eState==PAGER_OPEN ? -1 : (int) pPager->dbSize;
  a[4] = pPager->eState;
  a[5] = pPager->errCode;
  a[6] = pPager->aStat[PAGER_STAT_HIT];
  a[7] = pPager->aStat[PAGER_STAT_MISS];
  a[8] = 0;  /* Used to be pPager->nOvfl */
  a[9] = pPager->nRead;
  a[10] = pPager->aStat[PAGER_STAT_WRITE];
  return a;
}
int sqlite3PagerOpen()
1/* 算出对于每个日志文件操作需要多少空间（这里有两个，是主要日志和子日志）。这是对于一个内存日志文件操作和一个规则的日志文件操作所需要的最大空间和常规空间。注意一个“规则日志文件操作”可能是一个缓冲在存储中的第一部分日志文件的包装能力，用来实施自动写最优化
2/* 万一错误发生的话，设置输出变量为0
3/* 计算和存储完整路径名在分配缓冲区的指向的 （通过zPathname,nPathname长度）。或者,如果这是一个临时文件,把nPathname和zPathname设置为0。
4 /*对于页面结构分配存储，页面存储对象，三个文件描述符，数据库文件名和日志文件名。在存储中的设计如下所示
5/* 打开一个页面文件
6.1/* 打开一个页面文件
   6.1.1/* 如果文件是成功的用于打开后的读和写进程，如果我们创建一个数据库文件就选择一个默认页面大小。默认页面大小是以下值的最大值：默认页面大小+操作部分大小的返回值+可以被自动写的最大页面大小。
6.2/* 如果一个临时文件被要求，这不是被立即打开的。在这个例子中，我们接受默认页面大小并且拖延直到第一个调用OsWrite()才实际打开文件。这个分支也对于一个内存数据库运行。一个内存数据库是和临时文件一样的绝不会写出磁盘并且运用一个内存回滚日志。
7 /* 下面调用PagerSetPagesize()服务来设置页面大小值并且分配页面临时空间缓存。
8 /* 如果一个错误发生在以上任何一个模块，释放页面结构并且关闭文件。
9/* 初始化页面缓存对象
1//关闭页面缓存。释放所有内存和关闭所有文件。如果当调用这个例程时候，事务是在运行,那么事务回滚。所有未处理的页面是无效的并且他的内存被释放。任何试图用这个页面与缓存页面关联的页面，在这个函数返回后可能会导致coredump（貌似是垃圾堆的意思）。
//这个函数总是成功。如果一个事务激活了一个临时来回滚该事务。如果一个错误发生在一个热门日志的回滚期间，可能留在一个文件系统中而没有错误返回到调用中。
2//如果它是打开状态，日志文件在调用 UnlockAndRollback（非锁并且回滚） 之前是同步的。如果这没有被完成，那么一个开放的日志文件的不同步的部分可能被运行返回到数据库中。如果在运行当中发生电力故障，数据库可能成为corrupt（腐败，可能是不好的意思）。
//如果当尝试同步日志的时候一个错误发生了，转换页面进入错误状态。着导致了 UnlockAndRollback（非锁并且回滚） 来解锁这个数据库并且不尝试回滚或者完成它来直接关闭日志文件。
1//关闭页面缓存。释放所有内存和关闭所有文件。如果当调用这个例程时候，事务是在运行,那么事务回滚。所有未处理的页面是无效的并且他的内存被释放。任何试图用这个页面与缓存页面关联的页面，在这个函数返回后可能会导致coredump（貌似是垃圾堆的意思）。
//这个函数总是成功。如果一个事务激活了一个临时来回滚该事务。如果一个错误发生在一个热门日志的回滚期间，可能留在一个文件系统中而没有错误返回到调用中。
2//如果它是打开状态，日志文件在调用 UnlockAndRollback（非锁并且回滚） 之前是同步的。如果这没有被完成，那么一个开放的日志文件的不同步的部分可能被运行返回到数据库中。如果在运行当中发生电力故障，数据库可能成为corrupt（腐败，可能是不好的意思）。
//如果当尝试同步日志的时候一个错误发生了，转换页面进入错误状态。着导致了 UnlockAndRollback（非锁并且回滚） 来解锁这个数据库并且不尝试回滚或者完成它来直接关闭日志文件。
•//设置忙时操作函数，如果当试图从一个保留锁升级成一个共享锁时，sqlite3OsLock()返回SQLITE_BUSY。当从共享锁升级成保留锁的时候，或者当从共享锁升级成排他锁的时候，不调用忙操作。（发生在热门日志回滚时候）总结：
•
•//**   过渡                         | 调用 xBusyHandler
•**   --------------------------------------------------------
•**   不锁       ->共享锁      | Yes
•**   共享锁   -> 保留锁    | No
•**   共享锁   -> 排他锁   | No
•**   保留锁 -> 排他锁   | Yes
•
•//如果忙操作回调返回非零,锁重试。如果它返回0,然后SQLITE_BUSY错误返回给页面API功能的调用者
•//设置忙时操作函数，如果当试图从一个保留锁升级成一个共享锁时，sqlite3OsLock()返回SQLITE_BUSY。当从共享锁升级成保留锁的时候，或者当从共享锁升级成排他锁的时候，不调用忙操作。（发生在热门日志回滚时候）总结：
•
•//**   过渡                         | 调用 xBusyHandler
•**   --------------------------------------------------------
•**   不锁       ->共享锁      | Yes
•**   共享锁   -> 保留锁    | No
•**   共享锁   -> 排他锁   | No
•**   保留锁 -> 排他锁   | Yes
•
•//如果忙操作回调返回非零,锁重试。如果它返回0,然后SQLITE_BUSY错误返回给页面API功能的调用者
尝试设置数据库页面计算最大值，如果mxPage是正的。
如果mxPage为零或者为负，那就毫无改变。并且永远不要减少页面累计数的最大值，让他低于当前数据库大小。
