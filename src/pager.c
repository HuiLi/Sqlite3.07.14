/*
pvfs ������Ӧ�õ������ļ�ϵͳ��
*pPager�������ﷵ��ҳ���ṹ��
zFilename�������򿪵����ݿ��ļ����֣�
nExtra������ÿһ���ڴ�ҳ���ϸ��ӵĶ������ֽڣ�
flags���������ļ��ı�־��
vfsFlag ��ʾͨ��sqlite3-vfs.XOpen�����ı�־��
����void(*xReinit)(DbPage*)��ʾ��������ҳ�湦�ܣ�
*pPtr
pPager����ҳ�������ķ����ͷ���
rc=SQLITE_OK��ʾ���ش���
tempFile��ʾ��ʱ�ļ�Ϊ��
memDB��ʾ������һ���ڴ��ļ���Ϊ��
readonly��ʾ��������һ��ֻ���ļ���Ϊ��
journalFileSizeÿһ����־�ļ��������ֽ���
zPathname��ʾ���ݿ��ļ���ȫ��·��
nPathname��ʾzPathnamed ������
useJournal��ʾ������־ʧ��
pcacheSize��ʾPCach�������ڴ�
szPageSize��ʾĬ��ҳ����С
zUri��ʾ������URI�Ա���
nUri��ʾ��*zUri��URI�Ա����ı�����*/

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
/*���䲢�ҳ�ʼ��һ����ҳ��������Ȼ���ַ���һ��ָ������ָ����*ppPager�С�����ҳ��Ӧ�����ձ�sqlite3PagerClose�����ͷš�
�ļ��������Ǵ������ݿ��ļ���·���������ļ���Ϊ�գ��ǾͻὨ��һ���������ֵ���ʱ�ļ��������󻺴����ļ���
��ʱ�ļ����ڹرյ�ʱ���Զ���ɾ����
�����ļ����ǡ���memory������ô������Ϣ�����洢�ڻ����С�
������Զ��д���洢����������ʵʩһ���ڴ����ݿ⡣
nExtra������ϸ˵����ÿ�����õ���ҳ��������ָ���ռ��ֽ����������ռ�ͨ��sqlite3PagerGetExtra����API�Ƕ��û����õġ�
��־�����Ǳ�������ϸ˵������Ӱ��ҳ���Ĳ��������ܡ���Ӧ��ͨ��һЩPAGER_*flag�İ�λ���ϡ�
vsfFlags������λ����,���ᴫ�ݸ�xOpen()�ı�־�������������ļ�ʱxOpen�����ṩVFS��
����ҳ���������ѷ��䲢����ϸ˵�����ĳɹ��򿪵��ļ���SQLITE_OK������*ppPagerָ����ҳ�����������á�
����һ������������*ppPager����Ϊ�ղ��Ҵ��������᷵�ء�
�����������ܷ���SQLITE_NOMEN��sqlite3Malloc���������������洢�ģ���SQLITE_CANTOPEN���߶���SQLITE_IO_XXX֮���Ĵ�����
*/
int sqlite3PagerOpen(
  sqlite3_vfs *pVfs,       /* The virtual file system to use     ��Ӧ�õ������ļ�ϵͳ*/
  Pager **ppPager,         /* OUT: Return the Pager structure here      ���ﷵ��ҳ���ṹ*/
  const char *zFilename,   /* Name of the database file to open       ���򿪵����ݿ��ļ�����*/
  int nExtra,              /* Extra bytes append to each in-memory pagenExtra      ��ÿһ���ڴ�ҳ���ϸ��ӵĶ������ֽ� */
  int flags,               /* flags controlling this file           �����ļ��ı�־*/
  int vfsFlags,            /* flags passed through to sqlite3_vfs.xOpen()      ͨ��sqlite3-vfs.XOpen�����ı�־*/
  void (*xReinit)(DbPage*) /* Function to reinitialize pages//����ҳ���ĺ��� */
){
  u8 *pPtr;     //unsigned char
  Pager *pPager = 0;       /* Pager object to allocate and return   ���ڷ����ͷ��ص�ҳ������*/
  int rc = SQLITE_OK;      /* Return code */
  int tempFile = 0;        /* True for temp files (incl. in-memory files)   ��ʱ�ļ�Ϊ��*/
  int memDb = 0;           /* True if this is an in-memory file ������һ���ڴ��ļ���Ϊ��*/
  int readOnly = 0;        /* True if this is a read-only file ��������һ��ֻ���ļ���Ϊ��*/
  int journalFileSize;     /* Bytes to allocate for each journal fd ÿһ����־�ļ��������ֽ���*/
  char *zPathname = 0;     /* Full path to database file       zPathname�����ݿ��ļ�������·��*/
  int nPathname = 0;       /* Number of bytes in zPathname   zPathnamed�ı�����*/
  int useJournal = (flags & PAGER_OMIT_JOURNAL)==0;    /* False to omit journal ������־ʧ��*/
  int pcacheSize = sqlite3PcacheSize();       /* Bytes to allocate for PCache   PCach�����ı�������С*/
  u32 szPageDflt = SQLITE_DEFAULT_PAGE_SIZE;  /* Default page size Ĭ��ҳ����С*/
  const char *zUri = 0;    /* URI args to copy ������URI�Ա���*/
  int nUri = 0;            /* Number of bytes of URI args at *zUri ��*zUri��URI�Ա����ı�����*/

  /* Figure out how much space is required for each journal file-handle
  ** (there are two of them, the main journal and the sub-journal). This
  ** is the maximum space required for an in-memory journal file handle 
  ** and a regular journal file-handle. Note that a "regular journal-handle"
  ** may be a wrapper capable of caching the first portion of the journal
  ** file in memory to implement the atomic-write optimization (see 
  ** source file journal.c).
  */
/*��������ÿ����־�ļ�������Ҫ���ٿռ䣨����������������Ҫ��־������־����
���Ƕ���һ���ڴ���־�ļ�������һ����������־�ļ���������Ҫ�������ռ��ͳ����ռ䡣
  ע��һ�򡰹�����־�ļ����󡱿�����һ�򻺳��ڴ洢�еĵ�һ������־�ļ��İ�װ������
  ����ʵʩ�Զ�д���Ż�����Դ�ļ�journal.c��*/

  if( sqlite3JournalSize(pVfs)>sqlite3MemJournalSize() ){
    journalFileSize = ROUND8(sqlite3JournalSize(pVfs));         //ROUND8(x)     (((x)+7)&~7)     7ȡ������������
  }else{
    journalFileSize = ROUND8(sqlite3MemJournalSize());
  }
/*������־��С�������ļ�ϵͳpVfs��>�洢��־��С����ô��־�ļ���С=round8����־��С������֮�������ڲ�����Ϊ�洢��־��С��
����֮����־�ļ���С=ROUND8����־��С�ʹ洢��־��С�Ƚϴ���һ�򣩣�*/

/* Set the output variable to NULL in case an error occurs.��һ���������Ļ���������������Ϊ0 */
  *ppPager = 0;
//�����ص�ҳ���ṹ��*ppPager��Ϊ0

/*����û�ж���SQLITE-OMIT-MEMORYDB�����Դ洢���ݿ⣩
    ��������־��ҳ���ڴ���Ϊ�棩����memDb=1�����ڴ��ļ���
        �������ݿ��ļ�·�����ڲ���������һ��������ֵzPathname=sqlite3DbStrDup��0��zFilename����
            ����zPathnameΪ0��ʱ�򣬷���SQLITE_NOMEM��
            nPathname��·������)=sqlite3Strlen30��zPathname����ͳ��·������������
            ·����=0��*/
#ifndef SQLITE_OMIT_MEMORYDB
  if( flags & PAGER_MEMORY ){
    memDb = 1;
    if( zFilename && zFilename[0] ){
      zPathname = sqlite3DbStrDup(0, zFilename);//malloc.c
      if( zPathname==0  ) return SQLITE_NOMEM;
      nPathname = sqlite3Strlen30(zPathname);
      zFilename = 0;
    }
  }
#endif

  /* Compute and store the full pathname in an allocated buffer pointed
  ** to by zPathname, length nPathname. Or, if this is a temporary file,
  ** leave both nPathname and zPathname set to 0.
  */
//��·������·�����ȷ����Ļ����У������ʹ洢����·����������,��������һ����ʱ�ļ�,��·�����Ⱥ�·������Ϊ0��

  if( zFilename && zFilename[0] ){//�������ļ���&�ļ���[0]��
    const char *z;
    nPathname = pVfs->mxPathname+1;//·������=����ϵͳ������·������+1
    zPathname = sqlite3DbMallocRaw(0, nPathname*2);//·����=���䳤��Ϊ���ļ���*2��������
    if( zPathname==0 ){//������·����==0�� ����SQLITE_NOMEM
      return SQLITE_NOMEM;
    }
    zPathname[0] = 0; /* Make sure initialized even if FullPathname() fails ȷ����ʼ������ֹ�¾�����ʧЧ*/
    rc = sqlite3OsFullPathname(pVfs, zFilename, nPathname, zPathname);//���ش����ж�rc=sqlite3OsFullPathname�������õ������ļ�ϵͳ�����򿪵��ļ�����·�������ȣ�ȫ�����ݿ��ļ�·������
    nPathname = sqlite3Strlen30(zPathname);//�ļ���������=sqlite3Strlen30���ļ�������
    z = zUri = &zFilename[sqlite3Strlen30(zFilename)+1];//z��������char��=zUri�����Ƶ�URI�Ա�����=�������ݿ��ļ������֣�sqlite3Strlen30���򿪵����ݿ��ļ������֣�+1���ĵ�ַ
    while( *z ){        //�����㶨��z��ָ��ʱ����Ϊʲô�����������Ӱ���ֱ�Ӽ�2�����𣿣� ***************************************************************************************************
      z += sqlite3Strlen30(z)+1;
      z += sqlite3Strlen30(z)+1;
    }
    nUri = (int)(&z[1] - zUri);// ��*zUri�����Ƶ�URI�Ա�������URI�ı�����nUri=��ǿ��ת�����͵����Σ���z�ĵ�ַ�ĵ�һ���ֽ�-���Ƶ�URI�Ա�������
    assert( nUri>=0 );//assert��URI���Ա�����>=0�����������������Ĳ�������ʽ�Ƿ�Ϊ�棩
    if( rc==SQLITE_OK && nPathname+8>pVfs->mxPathname ){//���������ش���Ϊ�沢��·����+8>�����������е�����ϵͳ��ָ�����·������
      /* This branch is taken when the journal path required by
      ** the database being opened will be more than pVfs->mxPathname
      ** bytes in length. This means the database cannot be opened,
      ** as it will not be possible to open the journal file or even
      ** check for a hot-journal before reading.
      ** ����־·�������ڴ򿪵����ݿ���Ҫ��������֧�Ǳ��õģ��⽫�������������е��������ݿ���ָ�� - > mxPathname���ֽڳ��ȡ�
      ** ����ζ�����ݿⲻ�ܴ���,��Ϊ���������ܴ�����־�ļ�,�����������Ķ�֮ǰ����һ��������־��
      */
      rc = SQLITE_CANTOPEN_BKPT;//���ش���rc=SQLITE_CANTOPEN_BKPT���ܴ򿪣�
    }
    if( rc!=SQLITE_OK ){
      sqlite3DbFree(0, zPathname);/*Free memory that might be associated with a particular database connection.
                                                                                                          �ͷŷ������ض����ݿ����ӵ��ڴ�*/
      return rc;
    }
  }
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
 /*
����ҳ���ṹ�����洢��ҳ���洢�����������ļ������������ݿ��ļ�������־�ļ������ڴ洢�е�����������ʾ��
ҳ������                    Pager��С
ҳ�滺������             sqlite3PcacheSize���������Ļ����ռ�
���ݿ��ļ�����         �������е��������ݿ���ָ����szOsFile������ϵͳ�ļ��ǲ��ǣ�
����־�ļ�����         ��־�ļ���С
����־�ļ�����         ��־�ļ���С
���ݿ��ļ���           ·������Ŀ+1
��־�ļ���             ·������Ŀ+8+1��Ϊɶ��8��8λ����˼�𣿣�*/

  pPtr = (u8 *)sqlite3MallocZero(//Allocate and zero memory  /* pPtr=��u8��ǿ������ת����sqlite3MallocZero������С��
    ROUND8(sizeof(*pPager)) +      /* Pager structure */   //ҳ���ṹ+
    ROUND8(pcacheSize) +           /* PCache object */    //����ҳ������+
    ROUND8(pVfs->szOsFile) +       /* The main db file */  //��Ҫ��db�ļ�+
    journalFileSize * 2 +          /* The two journal files */   //������־�ļ�+
    nPathname + 1 + nUri +         /* zFilename */    //�ļ���+
    nPathname + 8 + 2              /* zJournal */     //·����������"������д��־"�ͼ�8��û�ж����ͼ�4����
#ifndef SQLITE_OMIT_WAL
    + nPathname + 4 + 2            /* zWal */
#endif
  );
  assert( EIGHT_BYTE_ALIGNMENT(SQLITE_INT_TO_PTR(journalFileSize)) );//�ж������ֽڶ��У�int����ת����ptr���ͣ���־�ļ���С��������
  if( !pPtr ){
    sqlite3DbFree(0, zPathname);//�ͷ�·����
    return SQLITE_NOMEM;
  }

}

  pPager =(Pager*)(pPtr);
  pPager->pPCache =(PCache*)(pPtr += ROUND8(sizeof(*pPager)));//pPCache����ҳ��
  pPager->fd =   (sqlite3_file*)(pPtr += ROUND8(pcacheSize));//fd����ҳ����С�ļ���
  pPager->sjfd = (sqlite3_file*)(pPtr += ROUND8(pVfs->szOsFile));//sjfd�����������ݿ�����־�ļ���С���ļ�
  pPager->jfd =  (sqlite3_file*)(pPtr += journalFileSize);
  pPager->zFilename =    (char*)(pPtr += journalFileSize);//zFilename��־�ļ��Ĵ�С�ֽ���
  assert( EIGHT_BYTE_ALIGNMENT(pPager->jfd) );//�ж������ֽڶ��У���ָ������־�ļ���С������

  /* Fill in the Pager.zFilename and Pager.zJournal buffers, if required.  //����Ҫ���Ļ�������ҳ���ļ�������־���档 */ 
  if( zPathname ){                                                   //����·����Ϊ��
    assert( nPathname>0 );                                            ///�ж�·���������Ƿ�����0�������ж�����Ϊ��������ֱ����ֹ����
    pPager->zJournal =   (char*)(pPtr += nPathname + 1 + nUri);      //ҳ������־= (char*)(pPtr += nPathname + 1 + nUri);
    memcpy(pPager->zFilename, zPathname, nPathname);               //�ַ���������ҳ�����ļ�����·������·������С����
    if( nUri ) memcpy(&pPager->zFilename[nPathname+1], zUri, nUri);  // ������nUri���ڣ�����ô���ַ���������ҳ�����ļ����ĵ�ַ��zUri��nUri����
    memcpy(pPager->zJournal, zPathname, nPathname);           //�ַ���������ҳ������־�ļ���·������·������С����
    memcpy(&pPager->zJournal[nPathname], "-journal\000", 8+1);  //�ַ���������ҳ������־�ļ���·��������"-journal\000", 8+1����
    sqlite3FileSuffix3(pPager->zFilename, pPager->zJournal);     //sqlite3FileSuffix3��ҳ��ָ�����ļ�����ҳ��ָ������־����
#ifndef SQLITE_OMIT_WAL                                             //����û�ж���SQLITE_OMIT_WAL
    pPager->zWal = &pPager->zJournal[nPathname+8+1];        //pPagerָ����zWal=������־�ļ�����
    memcpy(pPager->zWal, zPathname, nPathname);           //�ַ���������zWal+·����+·�����ȣ���
    memcpy(&pPager->zWal[nPathname], "-wal\000", 4+1);    //�ַ���������zWal��ַ+��ַ+4,1����
    sqlite3FileSuffix3(pPager->zFilename, pPager->zWal);   //�ͷ�·����
#endif
    sqlite3DbFree(0, zPathname);
  }
  pPager->pVfs = pVfs;               //ָ���������ݿ�ϵͳ
  pPager->vfsFlags = vfsFlags;      //ָ���������ݿ���־

  /* Open the pager file.����һ��ҳ���ļ�
  */
  if( zFilename && zFilename[0] ){
    int fout = 0;                    /* VFS flags returned by xOpen() */
    rc = sqlite3OsOpen(pVfs, pPager->zFilename, pPager->fd, vfsFlags, &fout);
    assert( !memDb );
    readOnly = (fout&SQLITE_OPEN_READONLY);
	/*
�������ļ������ļ�����0�������ڣ�{
   ͨ��xOpen() ���������ļ�ϵͳ
 ���ش���=����ϵͳ������������ϵͳ�еģ��ļ��еģ�ҳ���еı�־
���飨memDb�����ڣ���
ֻ���ļ�����־�ʹ���ֻ���ļ���Ϊ�棩��*/
}

    /* If the file was successfully opened for read/write access,
    ** choose a default page size in case we have to create the
    ** database file. The default page size is the maximum of:
    **
    **    + SQLITE_DEFAULT_PAGE_SIZE,
    **    + The value returned by sqlite3OsSectorSize()
    **    + The largest page size that can be written atomically.
       �����ļ��ɹ��򿪺󣬿������ڶ�д���������ڴ������ݿ��ļ�ʱ��ѡ��Ĭ��ҳ����С
//Ĭ��ҳ����С������ֵ������ֵ��Ĭ��ҳ����С+���󲿷ִ�С�ķ���ֵ+���Ա��Զ�д������ҳ����С��*/
	
    if( rc==SQLITE_OK && !readOnly ){  
      setSectorSize(pPager);           //����������С��ҳ�棩��
 assert(SQLITE_DEFAULT_PAGE_SIZE<=SQLITE_MAX_DEFAULT_PAGE_SIZE);   //�ж���Ĭ��ҳ����С<=����Ĭ��ҳ����С����
      if( szPageDflt<pPager->sectorSize){         //������szPageDflt<pPager->sectorSize ��{
        if( pPager->sectorSize>SQLITE_MAX_DEFAULT_PAGE_SIZE ){   //������ҳ��������С>Ĭ������ҳ����С��{
          szPageDflt = SQLITE_MAX_DEFAULT_PAGE_SIZE;
        }else{
          szPageDflt = (u32)pPager->sectorSize;
        }
      }    //���ĸ��������ĸ���
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
        
else{
    /* If a temporary file is requested, it is not opened immediately.
    ** In this case we accept the default page size and delay actually
    ** opening the file until the first call to OsWrite().
    **
    ** This branch is also run for an in-memory database. An in-memory
    ** database is the same as a temp-file that is never written out to
    ** disk and uses an in-memory rollback journal.
     ����һ����ʱ�ļ����������ⲻ�Ǳ������򿪵ġ�
     �����������У����ǽ���Ĭ��ҳ����С��������ֱ����һ������OsWrite()��ʵ�ʴ����ļ���
     ������֧ҲΪһ���ڴ����ݿ�����
     һ���ڴ����ݿ��Ǻ���ʱ�ļ�һ���ľ�����д�����̲�������һ���ڴ��ع���־�� */
     
    tempFile = 1;              //��ʱ�ļ���ֵ1��
    pPager->eState = PAGER_READER;      //״̬��ֵ�Ѷ���
    pPager->eLock = EXCLUSIVE_LOCK;     //��״̬��ֵ����
    readOnly = (vfsFlags&SQLITE_OPEN_READONLY);   //ֻ����ֵ��־&&����ֻ��
  }


  /* The following call to PagerSetPagesize() serves to set the value of 
  ** Pager.pageSize and to allocate the Pager.pTmpSpace buffer.
  */
//��������PagerSetPagesize()����������Pager.pageSizeֵ���ҷ���Pager.pTmpSpace����

  if( rc==SQLITE_OK ){    
    assert( pPager->memDb==0 );         //�ж���ҳ��ָ��memDb==0����
    rc = sqlite3PagerSetPagesize(pPager, &szPageDflt, -1);  //���ش���=����ҳ����С��ҳ�棬ҳ����С��ַ��-1����
    testcase( rc!=SQLITE_OK );   //�������ӣ����ش��룡=SQLITE_OK����
  }

  /* If an error occurred in either of the blocks above, free the 
  ** Pager structure and close the file.
  */
//����һ�����������������κ�һ��ģ�飬�ͷ�ҳ���ṹ���ҹر��ļ���

  if( rc!=SQLITE_OK ){
    assert( !pPager->pTmpSpace );     //��ô�ж���ҳ�治ָ��ҳ����ʱ�ռ䣩��
    sqlite3OsClose(pPager->fd);     //�ر�ָ�����ļ�
    sqlite3_free(pPager);     //�ͷ�ҳ��
    return rc;
  }

  /* Initialize the PCache object. */
//��ʼ��ҳ�滺������
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
  pPager->mxPgno = SQLITE_MAX_PAGE_COUNT;    //ҳ���ṹ������ҳ��
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

//�ر�ҳ�滺�档�ͷ������ڴ沢�ҹر������ļ���������������������ʱ����������������,��ô�����ع���
����δ������ҳ������Ч�Ĳ��������ڴ汻�ͷš�
�κ���ͼ������ҳ���뻺��ҳ��������ҳ�棬�������������غ����ܻᵼ��coredump��ò���������ѵ���˼����
//�����������ǳɹ�������һ�����񼤻�ͻ��ع�������
����һ������������һ��������־�Ļع��ڼ䣬��������һ���ļ�ϵͳ�ж�û�д��󷵻ص������С�
*/

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
    
//�������Ǵ���״̬����־�ļ��ڵ��� UnlockAndRollback���������һع��� ֮ǰ��ͬ���ġ�
������û�б����ɣ���ôһ�򿪷ŵ���־�ļ��Ĳ�ͬ���Ĳ��ֿ��ܱ����з��ص����ݿ��С�
���������е��з����������ϣ����ݿ����ܳ�Ϊcorrupt�����ܣ������ǲ��õ���˼����
//����������ͬ����־��ʱ��һ�����������ˣ�ת��ҳ����������״̬��
�⵼���� UnlockAndRollback���������һع��� �������������ݿⲢ�Ҳ����Իع�������������ֱ�ӹر���־�ļ���
����һ�����ݿ��û��ڷ������ݿ��ļ�֮ǰ������hot-journal�ع���
*/

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

//���ļ����ʼ��ȡǰN���ֽڷ���pDest��ָ���Ĵ洢�С�
//����ҳ������һ�������ļ�(zFilename = = " "),������һ���ļ�С��N���ֽڴ�С,���򻺳�����ֵΪ�㲢�ҷ���SQLITE_OK��
//��������ԭ���Ǻ��������ڶ�ȡ���ݿ�����,����һ���µ�����������С�����ݿ��б�����������ȫ���㹹�ɡ�
//�����򵽳���SQLITE_IOERR_SHORT_READ�κ�IO����,�������뷵�ظ������߲������򻺳���δ���������ݡ�
*/

int sqlite3PagerReadFileheader(Pager *pPager, int N, unsigned char *pDest){
  int rc = SQLITE_OK;
  memset(pDest, 0, N);
  assert( isOpen(pPager->fd) || pPager->tempFile );

  /* This routine is only called by btree immediately after creating
  ** the Pager object.  There has not been an opportunity to transition
  ** to WAL mode yet.
//��������ֻ�ڴ���һ��ҳ������֮�󣬱�btree�������á�һֱû�л���ת��WALģʽ��
����deleteģʽ���ȣ�WALģʽ�ڴ󲿷������¸��죬�����Ը��ã�����д֮�以��������
  */

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

//����æʱ������������������ͼ��һ�����������ɹ����������ߴӱ�����������������ʱ��
sqlite3OsLock()����SQLITE_BUSY,��ʱ��ҳ���ỽ��æ����
���ӹ����������ɱ�������ʱ�򣬻��ߵ��ӹ�������������������ʱ�򣬲�����æ���󡣣�������������־�ع�ʱ�����ܽ᣺

//**   ����                         | ���� xBusyHandler
**   --------------------------------------------------------
//**   ����       ->������          | Yes
//**   ������   -> ������           | No
//**   ������   -> ������           | No
//**   ������   -> ������             | Yes
//����æ�����ص󷵻ط���,�����ԡ�����������0,Ȼ��SQLITE_BUSY���󷵻ظ�ҳ��API���ܵĵ�����
*/

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

//�ı�ҳ���������õ�ҳ����С����ҳ����С��*pPageSize�д��ݡ�
//����ҳ���ڱ���������ʱ�����ڴ���״̬������һ�޲����ġ�
����ֵ�Ǵ���״̬�Ĵ������루Ҳ����SQLITE_IOERR,SQLITE_IOERR_xxx �Ӵ�������SQLITE_FULL�е�һ����
//�����Ļ��������������ж������ģ�
*��ҳ����С��*pPageSize��ֵ���ǿ��õģ�����512��SQLITE_MAX_PAGE_SIZE�е�һ�����֣�������
*����û��δ������ҳ�����ã�����
*���ݿⲻ���ڴ����ݿ⣬��������һ���ڴ����ݿ�����ǰ����ҳ�湹�ɡ�
//������ҳ������ҳ����С����*pPageSize�����õ�
//����ҳ����С�ı��ˣ���ô����������sqlite3PagerMalloc() ������һ����Pager.pTmpSpace���档
�����������䳢��ʧ���ˣ�SQLITE_NOMEM�����ز���ҳ����С���ֲ��䡣���������������У�SQLITE_OK�����ء�
//����ҳ����Сû�б��ı䣬��������Ϊ����������һ��ö�����������ǲ�Ϊ���ģ����������������õ�ʱ����
����ҳ�����ڴ�����״̬��������Ϊ�洢���䳢��ʧ�ܣ�Ȼ��*pPageSize����Ϊ�ɵģ��ڷ���ǰ����ҳ����С��
*/

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
  
//����������������һ����ȫ��assert_pager_state()����Ϊ��ҳ��������״̬�ڲ�һ��֮ǰ��
�������������Ǵ�PagerOpen()�ڲ����ó����ġ�

//��ĳ����������˵������ҳ����PAGER_ERROR��״̬�ϣ������᷵��һ��������
������ΪPAGER_ERROR״̬��֤����������һ��δ����ҳ�����ã������������������¼�����������һ���޲����ġ�
  */

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

//����mxPage�����ģ������������ݿ�ҳ����������ֵ��
����mxPageΪ������Ϊ�����Ǿͺ��޸ı䡣������Զ��Ҫ����ҳ���ۼ���������ֵ���������ڵ�ǰ���ݿ���С��
//����mxPage�����ص�ǰҳ������������ֵ
*/

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
//�ı����������ڴ�ҳ��������ҳ��
void sqlite3PagerSetCachesize(Pager *pPager, int mxPage){
  sqlite3PcacheSetCachesize(pPager->pPCache, mxPage);
}

/*
** Free as much memory as possible from the pager.
*/
//��ҳ���о����ܵ��ͷŴ洢
void sqlite3PagerShrink(Pager *pPager){
  sqlite3PcacheShrink(pPager->pPCache);
}
void sqlite3PagerSetSafetyLevel(
//��ҳ�����ð�ȫ�ȼ�
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
** The returned value is either PAGER_LOCKINGMODE_NORMAL org
** PAGER_LOCKINGMODE_EXCLUSIVE, indicating the current (possibly updated)
** locking-mode.

//�õ�/�趨����ҳ������ģʽ������eModeһ����PAGER_LOCKINGMODE_QUERY��
PAGER_LOCKINGMODE_NORMAL����PAGER_LOCKINGMODE_EXCLUSIVE�е�һ��������������not _QUERY��
��ô��ģʽ����ֵΪָ����ֵ��
//����ֵ��PAGER_LOCKINGMODE_NORMAL����PAGER_LOCKINGMODE_EXCLUSIVE�е�һ�򣬱�����ǰ�����ܻ����£�����ģʽ��
*/

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

//������ҳ��������־ģʽ������eModeһ���������е�һ����
ҳ��_��־ģʽ_ɾ����delete��
ҳ��_��־ģʽ_�ضϣ�truncate��
ҳ��_��־ģʽ_������persist��
ҳ��_��־ģʽ_�رգ�off��
ҳ��_��־ģʽ_�洢��memory��
ҳ��_��־ģʽ_Ԥдʽ��־��wal��

//�����ı��������ģ���ô��־ģʽ�����ó��ض�ֵ�������ı������ǲ��������ļ���ԭ�������ģ�
*һ���ڴ����ݿ�����ֻ��������־ģʽ���ó�_off����_memory��
*��ʱ���ݿⲻ����_wal��־ģʽ��
����ָ����ǰ�����ܸ��µģ�����־ģʽ��
*/

int sqlite3PagerSetJournalMode(Pager *pPager, int eMode){
  u8 eOld = pPager->journalMode;    /* Prior journalmode */

#ifdef SQLITE_DEBUG
  /* The print_pager_state() routine is intended to be used by the debugger
  ** only.  We invoke it once here to suppress a compiler warning. */
//print_pager_state()�����Ǵ���ֻ�����ڵ������ġ�һ������һ�������󾯸棬���ǵ�������
  print_pager_state(pPager);
#endif


  /* The eMode parameter is always valid */
//eMode����������Ч��
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
//��������ֻ��OP_JournalMode�������е��ã����Ҷ���Ԥд��־�����С���Զ������һ����ʱ�ļ��ı䡱���߼���
  assert( pPager->tempFile==0 || eMode!=PAGER_JOURNALMODE_WAL );

  /* Do allow the journalmode of an in-memory database to be set to
  ** anything other than MEMORY or OFF
  */
//ȷʵ����һ���ڴ����ݿ�����־ģʽ���趨�ɲ���memory��һЩ��������off
  if( MEMDB ){
    assert( eOld==PAGER_JOURNALMODE_MEMORY || eOld==PAGER_JOURNALMODE_OFF );
    if( eMode!=PAGER_JOURNALMODE_MEMORY && eMode!=PAGER_JOURNALMODE_OFF ){
      eMode = eOld;
    }
  }

  if( eMode!=eOld ){

    /* Change the journal mode. */
//�ı���־ģʽ
    assert( pPager->eState!=PAGER_ERROR );
    pPager->journalMode = (u8)eMode;

    /* When transistioning from TRUNCATE or PERSIST to any other journal
    ** mode except WAL, unless the pager is in locking_mode=exclusive mode,
    ** delete the journal file.
    */
//����truncate����persistת�����κγ�wal֮����������־ģʽ������ҳ��������ģʽ=����ģʽ�У�ɾ����־�ļ���
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
//�����������У����ǿ���ɾ����־�ļ����������ǲ����ܵģ���ô�ⲻ��һ�����⡣ɾ����������־�ļ���Ψһ�����Ż���
//��ɾ����־�ļ�֮ǰ����֤reserver ������������һ�����ݿ��ļ��ϡ��Ᵽ֤����־�ļ��������õ�ʱ�򲻻ᱻһЩ�����û�ɾ����
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
//��������־ģʽ
  return (int)pPager->journalMode;
}

/*
** Return the current journal mode.
*/
//���ص�ǰ��־ģʽ
int sqlite3PagerGetJournalMode(Pager *pPager){
  return (int)pPager->journalMode;
}

/*
** Return TRUE if the pager is in a state where it is OK to change the
** journalmode.  Journalmode changes can only happen when the database
** is unmodified.

��ҳ�洦�ڿ��Ըı���־ģʽ��״̬��ʱ�򷵻��棬��־ģʽ�ĸı�����ֻ�����ݿ�û�иı���ʱ��������

*/

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

//�õ�/���ò�������־�ļ����õ��Ĵ�С���ơ�
���ô�С����Ϊ-1��ζ��û�����Ʊ�ִ�С�
һ��С��-1���Ƶĳ����ǿղ���
*/

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

����һ������pPagerָ��pBackup���õĵ�ָ������������ģ����backup.c��ά�����������������ݡ�
����ģ�鲻͸����ʹ������Ϊsqlite3BackupRestart()�Ĳ�����sqlite3BackupUpdate()�Ĳ�����
*/

sqlite3_backup **sqlite3PagerBackupPtr(Pager *pPager){
  return &pPager->pBackup;
}

/*
** Acquire a reference to page number pgno in pager pPager (a page
** reference has type DbPage*). If the requested reference is 
** successfully obtained, it is copied to *ppPage and SQLITE_OK returned.
** //If the requested page is already in the cache, it is returned. 
** Otherwise, a new page object is allocated and populated with data
** read from the database file. In some cases, the pcache module may
** choose not to allocate a new page object and may reuse an existing
** object with no outstanding references.
** //The extra data appended to a page is always initialized to zeros the 
** first time a page is loaded into memory. If the page requested is 
** already in the cache when this function is called, then the extra
** data is left as it was when the page object was last used.
** //If the database image is smaller than the requested page or if a 
** non-zero value is passed as the noContent parameter and the 
** requested page is not already stored in the cache, then no 
** actual disk read occurs. In this case the memory image of the 
** page is initialized to all zeros. 
** //If noContent is true, it means that we do not care about the contents
** of the page. This occurs in two seperate scenarios:
**   a) When reading a free-list leaf page from the database, and
**
**   b) When a savepoint is being rolled back and we need to load
**      a new page into the cache to be filled with the data read
**      from the savepoint journal.
** //If noContent is true, then the data returned is zeroed instead of
** being read from the database. Additionally, the bits corresponding
** to pgno in Pager.pInJournal (bitvec of pages already written to the
** journal file) and the PagerSavepoint.pInSavepoint bitvecs of any open
** savepoints are set. This means if the page is made writable at any
** point in the future, using a call to sqlite3PagerWrite(), its contents
** will not be journaled. This saves IO.
** //The acquisition might fail for several reasons.  In all cases,
** an appropriate error code is returned and *ppPage is set to NULL.
** //See also sqlite3PagerLookup().  Both this routine and Lookup() attempt
** to find a page in the in-memory cache first.  If the page is not already
** in memory, this routine goes to disk to read it in whereas Lookup()
** just returns 0.  This routine acquires a read-lock the first time it
** has to go to disk, and could also playback an old journal if necessary.
** Since Lookup() never goes to disk, it never has to deal with locks
** or journal files.
//��Pager���͵�pager��,��ȡһ��ҳ��pgno�����ã�һ��ҳ��������һ�����͵�DbPage*����
���������������ÿ��Գɹ����ã��ͻᱻ���Ƶ�*ppPage���ҷ���SQLITE_OK��
//������������ҳ���Ѿ��ڻ����У���ô��������
������һ���µ�ҳ�����󱻷��䲢�����������ݿ��ļ��ж�ȡ�����ݡ�
��ĳЩ�����£�pcacheģ������ѡ��һ����û�б�����һ����ҳ���������ҿ�����ʹ�����Ѿ����ڵ�δ���ɵ����õģ�������
//���������ݸ��ӵ�ҳ���Ͼ�����ʼ��Ϊ0�ڵ�һ��һ��ҳ�����ؽ��ڴ���ʱ����
����������������ʱ��ҳ�������Ѿ��ڻ����д��ڣ���ô����������������ҳ����������һ��ʹ��ʱ�������ӱ����¡�
//�������ݿ�ͼ����С��������ҳ������һ����0ֵ����ΪnoContent�������ݣ���������ҳ�治�����ʹ洢�ڻ����еģ�
��ô��û��ʵ��Ӳ�̶�ȡ��������������ҳ���Ĵ洢ͼ�񶼱���ʼ��Ϊ0��
//����noContentΪ�棬������ζ�����ǲ�����ҳ�����ݡ��������������ĳ����з�����
a���������ݿ��ж�ȡ�����б�Ҷҳ��ʱ�򣬲���
b����һ�򱣴��ڵ㱻�ع�����������Ҫ����һ����ҳ������һ�򱻱�������־�ж�ȡ�����ݳ����Ļ����С�
//����noContent�����ģ���ô���ݷ�����0�����Ǵ����ݿ��ж�ȡ�ġ�
���⣬��������ӦPager.pInJournal��ҳ�루ҳ����bitvec�Ѿ�д����־�ļ���
����PagerSavepoint.pInSavepoint���κο��ŵ�bitvecs���ñ����㡣
����ζ������ҳ���Ǳ���������δ�����κε����ǿ�д�ģ���һ����sqlite3PagerWrite()�ĵ��ã�
�������ݽ�������־�ģ�����ʡ��IO��
//����Ҫ����ʧ�ܿ����ж���ԭ���������е������У�һ���ʵ��Ĵ������뱻���ز���*ppPager������ΪNULL��
//�μ�sqlite3PagerLookup()���������̺�Lookup�����������ҵ�һ�����ڴ滺���е�ҳ�档
����ҳ�治���Ѿ��ڴ洢�еģ��������̽�������ȥ��������Lookup�������÷���0ʱ����
���������ڵ�һ�δ�������ʱҪ��һ������������������Ҫ�Ļ�Ҳ���ط�һ������־��
��ΪLookup()��Զ�����������̣�����Զ����ȥ�������ļ�������־�ļ���
*/

int sqlite3PagerAcquire(
  Pager *pPager,      /* The pager open on the database file ��ҳ���������ݿ��ļ�����*/
  Pgno pgno,          /* Page number to fetch   ��ȡҳ��*/
  DbPage **ppPage,    /* Write a pointer to the page here дһ��ָ��ָ��������ҳ��*/
  int noContent       /* Do not bother reading content from disk if true  ����Ϊ�治Ҫ�жϴӴ��̶�ȡ����*/
){
  int rc;
  PgHdr *pPg;

  assert( pPager->eState>=PAGER_READER );
  assert( assert_pager_state(pPager) );

  if( pgno==0 ){
    return SQLITE_CORRUPT_BKPT;
  }

  /* If the pager is in the error state, return an error immediately. 
  ** Otherwise, request the page from the PCache layer. 
  ������ҳ�洦�ڴ���״̬����ô�������ش��󡣷�����pcache������ҳ��*/
  if( pPager->errCode!=SQLITE_OK ){
    rc = pPager->errCode;
  }else{
    rc = sqlite3PcacheFetch(pPager->pPCache, pgno, 1, ppPage);
  }

  if( rc!=SQLITE_OK ){
    /* Either the call to sqlite3PcacheFetch() returned an error or the
    ** pager was already in the error-state when this function was called.
    ** Set pPg to 0 and jump to the exception handler.  
    ���ú��������õ�ʱ����Ҫô��sqlite3PcacheFetch()�ĵ����Ѿ����ش�����Ҫô��ҳ���Ѿ����ڴ���״̬��
    ��pPg(PgHdr)����Ϊ0�����ҵ����쳣����*/
    pPg = 0;
    goto pager_acquire_err;
  }
  assert( (*ppPage)->pgno==pgno );
  assert( (*ppPage)->pPager==pPager || (*ppPage)->pPager==0 );

  if( (*ppPage)->pPager && !noContent ){
    /* In this case the pcache already contains an initialized copy of
    ** the page. Return without further ado.  �����������£�pcache�Ѿ��������ѳ�ʼ����ҳ���ĸ���*/
    assert( pgno<=PAGER_MAX_PGNO && pgno!=PAGER_MJ_PGNO(pPager) );
    pPager->aStat[PAGER_STAT_HIT]++;
    return SQLITE_OK;

  }else{
    /* The pager cache has created a new page. Its content needs to 
    ** be initialized.  ��ҳ�滺�棬�Ѿ�������һ���µ�ҳ�档����������Ҫ����ʼ����*/

    pPg = *ppPage;
    pPg->pPager = pPager;

    /* The maximum page number is 2^31. Return SQLITE_CORRUPT if a page
    ** number greater than this, or the unused locking-page, is requested.
    ������ҳ����2^31������һ��ҳ��������ֵ��������ô����SQLITE_CORRUPT����������δʹ�õļ���ҳ��*/
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
        ��InJournal bit-vectors�����ñ�����ʧ�ܣ����ᵼ�´����⡣��������ζ��������Ҫ��һЩ�����Ĺ���ȥ��¼
        ������Ҫ��¼��־��ҳ�档���ǣ�����λ����������a bitʱ��һ��Ҫ�Գ��ַ��������ĵط����в���*/
        
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
//����һ��ҳ���Ѿ����ڴ滺���У���ô��ȡ�������Ӵ��̶�ȡҳ�档
����ҳ�治�ڻ����У��ͷ���һ��ָ����ҳ����ָ����0��
//�μ�sqlite3PagerGet()������ҳ���Ѿ����ڻ����У�
�������̺�sqlite3PagerGet()֮����������_get()���������̲��Ҷ�ȡҳ�档
����ҳ�治�ڻ������ߴ���I / O�����������������������̷���NULL��
*/

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
//ҳ��pPg�����ü�����������
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

//�ͷ�һ��ҳ�����á�
//��������ҳ��������������,Ȼ��ҳ�汻���ӵ�LRU�б�
�����ж�����ҳ�����������ñ��ͷ�,һ���ع������������ݿ��ϵ�����ɾ����
*/

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

//����ҳ������Ϊ��д�����޸�һ��ҳ��֮ǰ���������������̡�
�����߱����������������ķ���ֵ����ע�ⲻҪ�ı��κ�ҳ������,�����������̷���SQLITE_OK��
//����������pager_write()֮������������������Ҳ�����������򣨼�������������ҳ���ʺϵ���������������
�����������¹�פ����������ҳ��һ�����ڷ���ǰд����־�ļ���
//�������ִ���,SQLITE_NOMEM��IO�����������ʵ��ķ��ء�����,����SQLITE_OK��
*/

int sqlite3PagerWrite(DbPage *pDbPage){
  int rc = SQLITE_OK;

  PgHdr *pPg = pDbPage;
  Pager *pPager = pPg->pPager;
  Pgno nPagePerSector = (pPager->sectorSize/pPager->pageSize);

  assert( pPager->eState>=PAGER_WRITER_LOCKED );
  assert( pPager->eState!=PAGER_ERROR );
  assert( assert_pager_state(pPager) );

  if( nPagePerSector>1 ){
    Pgno nPageCount;          /* Total number of pages in database file ���ݿ��ļ����ܵ�ҳ����Ŀ*/
    Pgno pg1;                 /* First page of the sector pPg is located on. ����pPg�еĵ�һ��ҳ�汻����*/
    int nPage = 0;            /* Number of pages starting at pg1 to journal ��pg1��ʼ��ҳ����¼��־*/
    int ii;                   /* Loop counter ѭ��������*/
    int needSync = 0;         /* True if any page has PGHDR_NEED_SYNC �����κ�һ��ҳ����PGHDR_NEED_SYNC��Ϊ��*/

    /* Set the doNotSyncSpill flag to 1. This is because we cannot allow
    ** a journal header to be written between the pages journaled by
    ** this function.
    ��doNotSyncSpill��־����Ϊ1��������Ϊ���ǲ�������ҳ�汻�ú���������־��¼��ʱ��д��־ͷ*/
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
    ����Ϊ�κ�һ����pg1��ʼ��nPageҳ��������PGHDR_NEED_SYNC��־����ô����Ҫ�����е�ҳ���������á�
    ��Ϊ�����κ�һ�򣬾Ϳ��ܻ���������ҳ�棬���κ�ҳ�汻д�����ݿ��ļ�֮ǰ��
    ��־�ļ���������������ҳ��ͬ���ĸ���*/
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

//�������̵ĵ��ø���ҳ��,û�б�Ҫ��ҳ��pPg����Ϣд�ص�����,��������ҳ�����ܱ���Ϊ��ҳ�档
��һ�з�����ʱ��,����,��ҳ�汻������Ϊ�ͷ��б���Ҷ�ڵ㣬���������������ݲ�����Ҫ��
//���ڸ�����ҳ������������δʹ�õģ����򸲸ǵ��������������������̡�
ҳ����־ҳ��Ϊ�ɾ��ģ����������ᱻд�����̡�
//���Ա���,�����Ż�����ʹɾ�������ٶ������ı���
*/

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

//�ƶ�ҳ��pPg����λ�ļ��е�pgno��
//����֮ǰһ��û�ж�λ��pgno�����ǳ�ΪpPgOld���Ķ�ҳ�������ã�����ҳ���������ڻ����еġ�
����ҳ��֮ǰ��λ��pgno�����������Ѿ��ڻع���־�У���ô�����Ǳ������̷߳��������ġ�
//ҳ��pPg��������Ȼ��Ч�������κ���pPg��ϵ��Ԫ���ݣ����磬�洢��nExtra�ֽڵ���������ҳ�����䣩�ǵ����ߵ����Ρ�
//һ���������������̱�����ʱһ���ܻ�Ծ�����Ǳ�����Ҫ��һ���������ﲻ��Ծ��������һ�����Ѿ���ɾ����
��CREATE INDEX��Ҫ�����������ǻ�Ծ��ʱ���ƶ�һ��ҳ�棩
//�������ĸ�����isCommit�Ƿ�0�ģ���ô�������ύǰ������ҳ������Ϊ���ݿ�������һ�������ƶ���
�����������£����Ǳ�֤���ݿ�ҳ��pPg���ý��������������б���д��
//�����������ܷ��� SQLITE_NOMEM����һ��IO�������룬����һ������������ʱ�򡣷������ͷ��� SQLITE_OK.
*/

int sqlite3PagerMovepage(Pager *pPager, DbPage *pPg, Pgno pgno, int isCommit){
  PgHdr *pPgOld;               /* The page being overwritten. ҳ����д*/
  Pgno needSyncPgno = 0;  /* Old value of pPg->pgno, if sync is required ����Ҫ��ͬ������¼pPg->pgno�ľ�ֵ*/
  int rc;                      /* Return code */
  Pgno origPgno;               /* The original page number ��ʼҳ��*/

  assert( pPg->nRef>0 );
  assert( pPager->eState==PAGER_WRITER_CACHEMOD
       || pPager->eState==PAGER_WRITER_DBMOD
  );
  assert( assert_pager_state(pPager) );

  /* In order to be able to rollback, an in-memory database must journal
  ** the page we are moving from.
  Ϊ�˿��Իع����ڴ����ݿ�������¼�����ƶ���ҳ��*/
  if( MEMDB ){
    rc = sqlite3PagerWrite(pPg);
    if( rc ) return rc;
  }

  /* If the page being moved is dirty and has not been saved by the latest
  ** savepoint, then save the current contents of the page into the 
  ** sub-journal now. This is required to handle the following scenario:
  **�������ƶ���ҳ������ҳ������û�б�����һ�������㱣�棬��ô�Ͱѵ�ǰ��ҳ�����ݱ��浽����־�У�
  �����ڴ��������龰�Ǳ�����:
  **   BEGIN;
  **     <journal page X, then modify it in memory>
  **     SAVEPOINT one;
  **       <Move page X to location Y>
  **     ROLLBACK TO one;
  **
  ** If page X were not written to the sub-journal here, it would not
  ** be possible to restore its contents when the "ROLLBACK TO one"
  ** statement were is processed.
  ** ����ҳ��xû��д������־����ִ�е�"ROLLBACK TO one"����ʱ���������ݽ������ָ���
  ** subjournalPage() may need to allocate space to store pPg->pgno into
  ** one or more savepoint bitvecs. This is the reason this function
  ** may return SQLITE_NOMEM.
      ubjournalPage()Ҳ����Ҫ�����ռ�ȥ��pPg->pgno�洢��һ�����߶��򱣴���bitvecs�С������Ǹú����᷵��
      SQLITE_NOMEM��ԭ��
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
  **������ҳ��pPg->pgno���Ա�д֮ǰ����־Ҳ��Ҫ����ͬ������ô��pPg->pgno���浽���ر���needSyncPgno��
  ** If the isCommit flag is set, there is no need to remember that
  ** the journal needs to be sync()ed before database page pPg->pgno 
  ** can be written to. The caller has already promised not to write to it.
     ����isCommit�������ˣ������ݿ�ҳ��pPg->pgno ��д֮ǰ�����ǲ���Ҫ������־ͬ����������Ҳ����ȥд*/
  if( (pPg->flags&PGHDR_NEED_SYNC) && !isCommit ){
    needSyncPgno = pPg->pgno;
    assert( pageInJournal(pPg) || pPg->pgno>pPager->dbOrigSize );
    assert( pPg->flags&PGHDR_DIRTY );
  }

  /* If the cache contains a page with page-number pgno, remove it
  ** from its hash chain. Also, if the PGHDR_NEED_SYNC flag was set for 
  ** page pgno before the 'move' operation, it needs to be retained 
  ** for the page moved there.
     ��������������ҳ����ҳ�棬�����ӹ�ϣ�����Ƴ������Ƴ�����֮ǰ��ҳ���Ѿ���������PGHDR_NEED_SYNC��־
     ����ô��ҳ����Ҫ����������*/
  pPg->flags &= ~PGHDR_NEED_SYNC;
  pPgOld = pager_lookup(pPager, pgno);
  assert( !pPgOld || pPgOld->nRef==1 );
  if( pPgOld ){
    pPg->flags |= (pPgOld->flags&PGHDR_NEED_SYNC);
    if( MEMDB ){
      /* Do not discard pages from an in-memory database since we might
      ** need to rollback later.  Just move the page out of the way.
         ��Ҫ�����ڴ����ݿ���ҳ�棬��Ϊ����֮��������Ҫ�ع򡣰�ҳ���ӵ�ǰ���߾���*/
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
     ����һ���ڴ����ݿ���˵��ȷ����ʼ��ҳ�滹���ڣ��Է�������Ҫ�ع���
     ��pPgOld��Ϊ��ʼҳ����Ϊ���Ѿ���������*/
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
    **����needSyncPgno���㣬��ô��־�ļ���Ҫ��������д�����ݿ��ļ�֮ǰͬ����Ŀǰ��û��ҳ�滺���в�����������ҳ��
    ����"is journaled" bitvec��־Ҳ�Ѿ��������ˡ�����Ҫͨ������ҳ�浽ҳ�滺�棬��������PGHDR_NEED_SYNC��־�����ȡ�
    ** If the attempt to load the page into the page-cache fails, (due
    ** to a malloc() or IO failure), clear the bit in the pInJournal[]
    ** array. Otherwise, if the page is loaded and written again in
    ** this transaction, it may be written to the database file before
    ** it is synced into the journal file. This way, it may end up in
    ** the journal file twice, but that is not a problem.
        ������������ҳ�浽ҳ�滺��ʧ����(�����ڴ�����ʧ�ܻ���IOʧ��)������pInJournal[]���顣����
        ����ҳ���Ѿ������أ��������������ٴα�д������Ҫ�ڱ�ͬ������־֮ǰд�����ݿ��ļ�������������־�ļ���
        �ж����Σ����ⲻ�����⡣*/
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
//*��������ָ����ҳ����������
int sqlite3PagerPageRefcount(DbPage *pPage){
  return sqlite3PcachePageRefcount(pPage);
}

/*
** Return a pointer to the data for the specified page.
*/
//����һ������ָ��ҳ�����ݵ�ָ�롣
void *sqlite3PagerGetData(DbPage *pPg){
  assert( pPg->nRef>0 || pPg->pPager->memDb );
  return pPg->pData;
}

/*
** Return a pointer to the Pager.nExtra bytes of "extra" space 
** allocated along with the specified page.
*/
//����һ������ҳ����nExtra�����⡱�ռ��ֽڵ�ָ�룬�ռ�������ָ����ҳ�����䡣
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
//������������ֻ����ҳ���ϴ���һ��������ʱ�����á������������ݿ��е�ҳ��������
//Ȼ��,�����ļ��ǽ���1��<ҳ����С>�ֽڴ�С֮���Ļ�����ô����һ��1��С��ҳ�ļ���
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

//���ض���ҳ�������Ͽ�ʼһ��д����������һ��д�����Ѿ����򿪣�����������û�в����ġ�
//����exFlag�����Ǵ����ģ���ô�����ݿ��ļ������ٻ���һ����������
����exFlag����ȷ�ģ���ô��������һ������������������һ�����Ѿ����ڣ�û����Ҫ�����õ���������
//����subjInMemory�����Ƿ�0�ģ���ô�κ��������д򿪵�����־����Ϊһ���ڴ��ļ��򿪡�
����û��Ӱ���ģ���������־�Ѿ������ˣ������������ڻ��ⷽʽ�����е�ʱ�򣩣�
//��������������Ҫ��һ������־������subjInMemory������0��
����pPager��һ���ڴ����ݿ�����������һ����ʱ�ļ�����ô�κ���Ҫ��������־��ʵ�����ڴ��еġ�

*/

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
        ����ҳ�汻���ó�locking_mode=exclusive���������ݿ��л�û�п����ų�������ô���ھͼ���*/
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
          ����־�ļ���ȡд���������ɹ������µ�PAGER_RESERVED״̬�����򣬸������߷���һ����������
          ������һ�������Ѿ�����д������ôæ���󲢲��ᱻ���ѡ��������ܣ���һ����������*/
          
      rc = sqlite3WalBeginWriteTransaction(pPager->pWal);
    }else{
      /* Obtain a RESERVED lock on the database file. If the exFlag parameter
      ** is true, then immediately upgrade this to an EXCLUSIVE lock. The
      ** busy-handler callback can be used when upgrading to the EXCLUSIVE
      ** lock, but not when obtaining the RESERVED lock.
         �����ݿ��ļ�������һ��������������exFlag����Ϊ�棬��ô�����������ĳ�����������ʱ������Ӧ��æ����
         ���أ����ǵ��������Ǳ�������ʱ���Ͳ�������������*/
         
      rc = pagerLockDb(pPager, RESERVED_LOCK);
      if( rc==SQLITE_OK && exFlag ){
        rc = pager_wait_on_lock(pPager, EXCLUSIVE_LOCK);
      }
    }

    if( rc==SQLITE_OK ){
      /* Change to WRITER_LOCKED state.
      **�ĳ�д��״̬
      ** WAL mode sets Pager.eState to PAGER_WRITER_LOCKED or CACHEMOD
      ** when it has an open transaction, but never to DBMOD or FINISHED.
      ** This is because in those states the code to roll back savepoint 
      ** transactions may copy data from the sub-journal into the database 
      ** file as well as into the page cache. Which would be incorrect in 
      ** WAL mode.
         WALģʽ��Pager.eState���ó�PAGER_WRITER_LOCKED����CACHEMOD
         ������һ���Ѵ򿪵����񣬵���Զ������DBMOD����FINISHED.������Ϊ����Щ�����£��ع������������Ĵ���
         ��������־�ĵ����ݸ��Ƶ����ݿ��ļ���ҳ�滺���С�����WALģʽ���ǲ���ȷ�ġ�
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
/*����ҳ��pPagerͬ�����ݿ��ļ���zMasterָ��һ��Ӧ��д�뵥������־�ļ�������־�ļ������֡�
zMaster����Ϊ�գ������Ϳ��Խ���Ϊû������־��һ�򵥶������ݿ����񣩡�
��������ȷ���ˣ�
*change-counter�����¡�
*��־��ͬ���ģ����������Զ�д�Ż���
*������ҳ�汻д�����ݿ��ļ���
*���ݿ��ļ����ضϣ�����Ҫ���Ļ���������
*ͬ�����ݿ��ļ�
//�����ύ������Ψһ�����������ɣ�ɾ�����ضϻ��߶Ե�һ���ָ�ֵ0����־�ļ������������ر�ָ���Ļ���ɾ������־�ļ�����
//ע�⵽����zMaster==NULL���ⲻ����дһ�򴫵ݸ�sqlite3PagerCommitPhaseOne()���õ�ǰֵ
//��������һ��������noSyncedΪ�棬��ô���ݿ��ļ����Լ�����ͬ���ġ�������һ������sqlite3PagerSync()��
�����������У��ڵ��� CommitPhaseTwo()ȥɾ����־�ļ�֮ǰֱ��ͬ�����ݿ��ļ���*/

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

  /* If a prior error occurred, report that error again.����ǰ�����ִ������ٴα����Ǹ����� */
  if( NEVER(pPager->errCode) ) return pPager->errCode;

  PAGERTRACE(("DATABASE SYNC: File=%s zMaster=%s nSize=%d\n", 
      pPager->zFilename, zMaster, pPager->dbSize));

  /* If no database changes have been made, return early. �������ݿ�û�з����仯�����㷵��*/
  if( pPager->eState<PAGER_WRITER_CACHEMOD ) return SQLITE_OK;

  if( MEMDB ){
    /* If this is an in-memory db, or no pages have been written to, or this
    ** function has already been called, it is mostly a no-op.  However, any
    ** backup in progress needs to be restarted.
        ��������һ���ڴ����ݿ⣬����û��ҳ�汻д���������������Ѿ������ã����ܿ����޲�����
        Ȼ�������������κ�����������Ҫ������*/
    sqlite3BackupRestart(pPager->pBackup);
  }else{
    if( pagerUseWal(pPager) ){
      PgHdr *pList = sqlite3PcacheDirtyList(pPager->pPCache);
      PgHdr *pPageOne = 0;
      if( pList==0 ){
        /* Must have at least one page for the WAL commit flag.
        ** Ticket [2d1a5c67dfc2363e44f29d9bbd57f] 2011-05-18 
            ����WAL�ݽ���������������һ������*/
            
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
      ** ����ģ�����¼�����������������ô����������ԭ�Ӹ����Ż��ڱ���ʱ���Ƿ����У��Լ�������������
      �������²����ı�׼:
      **    * The file-system supports the atomic-write property for
      **      blocks of size page-size, and
          �ļ�ϵͳ��ҳ����Сģ��֧��ԭ��д���ܣ�
      **    * This commit is not part of a multi-file transaction, and
          ���ִ��ݲ����Ƕ��ļ�ϵͳ��һ����
      **    * Exactly one page has been modified and store in the journal file.
      **     һ��ҳ���Ѿ������Ĳ��Ҵ洢����־�ļ���
      ** If the optimization was not enabled at compile time, then the
      ** pager_incr_changecounter() function is called to update the change
      ** counter in 'indirect-mode'. If the optimization is compiled in but
      ** is not applicable to this transaction, call sqlite3JournalCreate()
      ** to make sure the journal file has actually been created, then call
      ** pager_incr_changecounter() to update the change-counter in indirect
      ** mode. 
      **�����Ż������ڱ���ʱ�䲢�����У���ôpager_incr_changecounter()�ᱻ����
      ȥ����indirect-mode�еļ����������������Ż��󱻱��룬���ǲ���������������������ô����sqlite3JournalCreate()
      ȷ����־�ļ��Ѿ���������Ȼ������pager_incr_changecounter()����indirect mode�еļ�������
      
      ** Otherwise, if the optimization is both enabled and applicable,
      ** then call pager_incr_changecounter() to update the change-counter
      ** in 'direct' mode. In this case the journal file will never be
      ** created for this transaction.
         �����������Ż����ǿ��еģ���ô����pager_incr_changecounter()����direct mode�еļ������������������£�
         ��־�ļ������������������в���*/
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
           ͨ��ֱ��д�����������ݿ��ļ��ı������������µ��û��ı�page1���ڴ���ʾ���Ӷ����¼�������Ȼ��ֱ�Ӱ�
           page1д�����ݿ��ļ�����Ϊ���ļ�ϵͳ��ԭ��д���ܣ����������ܰ�ȫ��*/
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
      **����������ʹ�����ݿ�����С����ô���б�����������ҳ�涼Ҫ��д����־�ļ���
      ������������auto-vacuumģʽ��
      ** Before reading the pages with page numbers larger than the 
      ** current value of Pager.dbSize, set dbSize back to the value
      ** that it took at the start of the transaction. Otherwise, the
      ** calls to sqlite3PagerGet() return zeroed pages instead of 
      ** reading data from the database file.
          ��ͨ��ҳ����ҳ����Pager.dbSize�ĵ�ǰֵ��֮ǰ����dbSize���ó�������ʼʱ��ֵ��������sqlite3PagerGet()
          �ĵ��û᷵����ҳ�棬�����Ǵ����ݿ��ļ���ȡ���ݡ�*/
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
         ������־����д����־�ļ�������һ������־�ļ��������Ѿ���д����־�ļ�������
         ����zMaster�ǿ�(û������־)����ô���������޲���*/
      rc = writeMasterJournal(pPager, zMaster);
      if( rc!=SQLITE_OK ) goto commit_phase_one_exit;
  
      /* Sync the journal file and write all dirty pages to the database.
      ** If the atomic-update optimization is being used, this sync will not 
      ** create the journal file or perform any real IO.
      ** ͬ����־�ļ������Ұ����е���ҳд�����ݿ⡣�����õ�ԭ���Ż�����������ͬ������������־�ļ�IO��
      ** Because the change-counter page was just modified, unless the
      ** atomic-update optimization is used it is almost certain that the
      ** journal requires a sync here. However, in locking_mode=exclusive
      ** on a system under memory pressure it is just possible that this is 
      ** not the case. In this case it is likely enough that the redundant
      ** xSync() call will be changed to a no-op by the OS anyhow. 
         ��Ϊ������ҳ�ձ��޸ģ������õ�ԭ�Ӹ����Ż�����������ȷ����־��������Ҫͬ����Ȼ����
         ��ϵͳ�ڴ�ѹ����locking_mode=exclusive�����������������������������£��ܿ���������xSync()����
         �ᱻ����ϵͳ��ĳ�̶ֳ��ϸĳ��޲���*/
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
         ���������ϵ��ļ������ݿ�ͼ����С��һ������ô������ͨ��ʹ��ҳ���ض�ȥ����������С�ļ�*/
      if( pPager->dbSize!=pPager->dbFileSize ){
        Pgno nNew = pPager->dbSize - (pPager->dbSize==PAGER_MJ_PGNO(pPager));
        assert( pPager->eState==PAGER_WRITER_DBMOD );
        rc = pager_truncate(pPager, nNew);
        if( rc!=SQLITE_OK ) goto commit_phase_one_exit;
      }
  
      /* Finally, sync the database file. ͬ�����ݿ��ļ� */
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

//��������ֻ����һ��д������Ծ�ڻع�ʱ�����á�������������WALģʽ,����������һ���ղ�����
����,�������Ӳ����Ѿ���һ�������������ݿ��ļ���,��ô�ͻ᳢��ȥ����һ����
//�������������Ѿ����ڻ���ͼ��֤���ǳɹ���,������������walģʽ,����SQLITE_OK��
����,�ͻ᷵��SQLITE_BUSY��һ��SQLITE_IOERR_XXX��������
*/

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

//ͬ�����ݿ��ļ��������ϡ��������ڴ����ݿ���������Pager.noSync��־���õ�ҳ�����޲����ġ�
//�����ɹ�������������һ��ҳ���϶���һ���޲����ĵ��ã������������� SQLITE_OK�����򣬷���һ��IO�������롣
*/

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

//������������ʱ�����ݿ��ļ�����ȫ�ĸ�������ӳ��ǰ������ͬ��������ʱ�����ı仯��
��־�ļ���Ȼ�������ļ�ϵͳ�У�������������һ���������ս���Ϊ������־���ҵ�ǰ�����ع���

//��������ͨ��ɾ�����ضϻ򲿷ֹ�����������־�ļ�,���������ܱ�����������־�ع���
һ�����ɣ������ǲ�����ת���ύ��

//�������ִ���,����һ��IO�������벢��ҳ����������״̬������,����SQLITE_OK��
*/

int sqlite3PagerCommitPhaseTwo(Pager *pPager){
  int rc = SQLITE_OK;                  /* Return code */

  /* This routine should not be called if a prior error has occurred.
  ** But if (due to a coding error elsewhere in the system) it does get
  ** called, just return the same error code without doing anything. 
     ��������һ��ǰ����������ô�������̲�Ӧ�ñ����á����ǣ�����(����ϵͳ�����ط��Ĵ�������)��û�б�����
     ��ô�������κ��£�ֻ�践��ͬ���Ĵ������롣*/
  if( NEVER(pPager->errCode) ) return pPager->errCode;

  assert( pPager->eState==PAGER_WRITER_LOCKED
       || pPager->eState==PAGER_WRITER_FINISHED
       || (pagerUseWal(pPager) && pPager->eState==PAGER_WRITER_CACHEMOD)
  );
  assert( assert_pager_state(pPager) );

  /* An optimization. If the database was not actually modified during
  ** this transaction, the pager is running in exclusive-mode and is
  ** using persistent journals, then this function is a no-op.
  ** �Ż����������������У����ݿ�û�б����ģ�ҳ����������ģʽ�����в���ʹ�ó־���־����ô���������ǿղ�����
  ** The start of the journal file currently contains a single journal 
  ** header with the nRec field set to 0. If such a journal is used as
  ** a hot-journal during hot-journal rollback, 0 changes will be made
  ** to the database file. So there is no need to zero the journal 
  ** header. Since the pager is in exclusive mode, there is no need
  ** to drop any locks either.
     ��־�ļ��Ŀ�ͷĿǰ����һ��nRec����Ϊ0�ĵ���־ͷ��������hot-journal�ع��ڼ䣬������־����Ϊhot-journal
     ��ô���ݿ��ļ��������κα仯������û�б�Ҫȥ������־ͷ����Ϊҳ�洦������ģʽ����ôҲû�б�Ҫȥ��ֹ�κ���*/
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


//����һ��д�����Ǵ򿪵ģ���ô���еĸı䶼�������񱻻ָ�֮�ڵģ����ҵ�ǰд-�����ǹرյġ�
�����ɹ��Ļ���ҳ������PAGER_READER״̬��������������������ҳ������PAGER_ERROR״̬��
���������������õ�ʱ����ҳ���Ѿ�����PAGER_ERROR״̬��������������Pager.errCode���������򲻻����κβ�����
//���⣬�ڻع�ģʽ�������������ֳ��������ܣ�
/*1)���ع���־�ļ����������򿪵�ʱ�򣬻ָ��������ݿ��ļ����ڴ滺��ҳ�����ڵ�״̬
2)��������־�ļ���������δ���κε��ϵ��Ȼع���û���õġ�

//�����ع��ɹ�����־�ļ��Ľ�����task 2����Ψһ���ֳ����ġ�

//��WALģʽ�����еĻ�����Ŀ�����ڵ�ǰ����֮�ڵ������޸� 
�Ǵӻ����г�ȥ����ͨ���ض����ݿ��л�WAL�ļ��е����� �ָ�������ǰ����״̬��WAL�����������͹ر��ˡ�
*/

int sqlite3PagerRollback(Pager *pPager){
  int rc = SQLITE_OK;                  /* Return code */
  PAGERTRACE(("ROLLBACK %d\n", PAGERID(pPager)));
 
  /* PagerRollback() is a no-op if called in READER or OPEN state. If
  ** the pager is already in the ERROR state, the rollback is not
  ** attempted here. Instead, the error code is returned to the caller.
      �������ô���READER����OPEN״̬��PagerRollback()�޲�����
      ����ҳ���Ѿ����ڴ���״̬����ô�Ͳ��������ﳢ�Իع�
      �෴����������Ҳ�᷵�ظ�������*/
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
      ʹ��journal_mode=off��ִ�иþ䡣��ҳ���Ƶ�����״̬ȥ��ʾ����������Ҳ�����š�
      �κλ�Ծ�Ķ��߽�����ȡSQLITE_ABORT*/
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
     �����ڻع��ڼ䣬�������������ǽ���������ҳ�滺�棬���Դ�ʱ����pager_error()�ᵼ�³־��ԵĴ���*/
  return pager_error(pPager, rc);
}
/*һ��������

    ��SQLite�У����Ͳ������ƻ��ƶ�����pager_moduleģ�鸺�������ģ���ACID(Atomic, Consistent, Isolated, and Durable)���ں��������޸ĵ������У���ģ�齫ȷ���������е������޸�ȫ���ύ������ȫ���ع�������ͬʱ����ģ�黹�ṩ��һЩ�����ļ����ڴ�Cache���ܡ�
    ��ʵ�ϣ�pager_moduleģ�鲢���������ݿ��洢��ϸ�ڣ���B-Tree�����뷽ʽ�������ȣ���ֻ�ǽ�����Ϊ��ͳһ��С(ͨ��Ϊ1024�ֽ�)�����ݿ鹹�ɵĵ�һ�ļ�������ÿ���鱻��Ϊһ��ҳ(page)���ڸ�ģ����ҳ����ʼ����Ϊ1������һ��ҳ������ֵ��1��������ҳ�����Դ����ơ�

�����ļ�����

    ��SQLite�ĵ�ǰ�汾�У���Ҫ�ṩ���������ַ�ʽ���ļ���״̬��
    1). UNLOCKED��
    �ļ�û�г����κ���������ǰ���ݿⲻ�����κζ���д�Ĳ����������Ľ��̿����ڸ����ݿ���ִ�������Ķ�д���󡣴�״̬Ϊȱʡ״̬��
    2). SHARED��
    �ڴ�״̬�£������ݿ����Ա���ȡ���ǲ��ܱ�д�롣��ͬһʱ�̿��������������Ľ�����ͬһ�����ݿ��ϳ��й����������˶������ǲ����ġ����仰˵��ֻҪ��һ�����������������ڻ״̬���Ͳ������������ݿ��ļ�д���Ĳ������ڡ�
    3). RESERVED��
    ����ĳ�������ڽ�����ĳһʱ�̴����ڵ�ǰ�����ݿ���ִ��д������Ȼ����ʱֻ�Ǵ����ݿ��ж�ȡ���ݣ���ô���ǾͿ��Լ򵥵�����Ϊ���ݿ��ļ���ʱ�Ѿ�ӵ���˱������������������ڻ״̬ʱ�������ݿ�ֻ����һ�����������������ڣ���ͬһ���ݿ���ͬһʱ��ֻ�ܴ���һ���������Ͷ�������������Oracle�д���������֮ΪԤд������ͬ����Oracle���������ȿ���ϸ�������������У����˸�������Oracle�жԲ�����Ӱ����������SQLite����������
    4). PENDING��
    PENDING������˼��˵��ĳ�������������ڸ����ݿ���ִ��д������Ȼ����ʱ�����ݿ���ȴ���ںܶ๲����(������)����ô��д�����ͱ��봦�ڵȴ�״̬�����ȴ����й�������ʧΪֹ������ͬʱ���µĶ����󽫲��ٱ������Է�ֹд�������������������ڴ˵ȴ��ڼ䣬�����ݿ��ļ�����״̬ΪPENDING���ڵȵ����й�������ʧ�Ժ���PENDING��״̬�����ݿ��ļ����ڻ�ȡ������֮������EXCLUSIVE״̬��
    5). EXCLUSIVE��
    ��ִ��д����֮ǰ���ý��̱����Ȼ�ȡ�����ݿ�����������Ȼ��һ��ӵ�������������κ����������Ͷ�������֮���档���ˣ�Ϊ�����󻯲���Ч�ʣ�SQLite������С�������������е�ʱ��������

    ������Ҫ˵�����ǣ���������ϵ�����ݿ����ȣ���MySQL��Oracle�ȣ�SQLite���ݿ������е����ݶ��洢��ͬһ�ļ��У�����ͬʱ����ȴ�����ṩ�˴����ȵ��ļ��������ˣ�SQLite�ڲ����Ժ������Եȷ�����������ϵ�����ݿ⻹���޷������ġ��ɴ˿ɼ���SQLite�������������ó����������ڱ�ϵ�п�ƪ����˵������������ϵ�����ݿ�֮���Ļ����Ի��Ƿǳ����޵ġ�

�����ع���־��

    ��һ������Ҫ�ı����ݿ��ļ���ʱ���������Ƚ�δ�ı�֮ǰ�����ݼ�¼���ع���־�ļ��С�����SQLite�е�ĳһ����������ͼ�޸Ķ������ݿ��е����ݣ���ô��ʱÿһ�����ݿⶼ������һ�������Լ��Ļع���־�ļ������ڷֱ���¼�����Լ������ݸı䣬����ͬʱ��Ҫ����һ������Э���������ݿ������������ݿ���־�ļ����������ݿ���־�ļ��н������������ݿ��ع���־�ļ����ļ�������ÿ���ع���־�ļ���Ҳͬ�������������ݿ���־�ļ����ļ�����Ϣ��Ȼ���������������ݿ���־�ļ��Ļع���־�ļ�������Ҳ�ᱣ�������ݿ���־�ļ�����Ϣ��ֻ�Ǵ�ʱ����Ϣ��ֵΪ�ա�
    ���ǿ��Խ��ع���־��Ϊ"HOT"��־�ļ�����Ϊ���Ĵ��ھ���Ϊ�˻ָ����ݿ���һ����״̬����ĳһ�������ڸ������ݿ�ʱ��Ӧ�ó�����OSͻȻ�������������²����Ͳ���˳�����ɡ��������ǿ���˵"HOT"��־ֻ�����쳣�����²Ż����ɣ�����һ�ж��ǳ�˳���Ļ������ļ�����Զ�������ڡ�

�ġ�����д�룺

    ����ĳһ����Ҫ�������ݿ���ִ��д��������ô�����Ȼ�ȡ���������ڹ�������ȡ֮���ٻ�ȡ����������Ϊ��������Ԥʾ���ڽ���ĳһʱ�̸ý��̽���ִ��д������������ͬһʱ��ֻ��һ�����̿��Գ���һ�ѱ������������������̿��Լ������й��������������ݶ�ȡ�Ĳ���������Ҫִ��д�����Ľ��̲��ܻ�ȡ����������ô�⽫˵����һ�����Ѿ���ȡ�˱��������ڴ��������£�д������ʧ�ܣ�����������SQLITE_BUSY�������ڳɹ���ȡ������֮�󣬸�д���̽������ع���־��
    �ڶ��κ����������ı�֮ǰ��д���̻Ὣ���޸�ҳ�е�ԭ����������д���ع���־�ļ��У�Ȼ������Щ���ݷ����仯��ҳ���񲢲���ֱ��д�������ļ������Ǳ������ڴ��У������������̾Ϳ��Լ�����ȡ�����ݿ��е������ˡ�
    ��������Ϊ�ڴ��е�cache������������Ӧ�ó����Ѿ��ύ�����������գ�д���̽����ݸ��µ����ݿ��ļ��С�Ȼ���ڴ�֮ǰ��д���̱���ȷ��û�������Ľ������ڶ�ȡ���ݿ⣬ͬʱ�ع���־�е�����ȷʵ��������д�뵽�����ļ��У��䲽�����£�
    1). ȷ�����еĻع���־���ݱ�������д�������ļ����Ա��ڳ���ϵͳ����ʱ���Խ����ݿ��ָ���һ�µ�״̬��
    2). ��ȡPENDING�����ٻ�ȡ��������������ʱ�����Ľ�����Ȼ���й�������д���߳̽����ò������𲢵ȴ�ֱ����Щ��������ʧ֮�󣬲��ܽ����õ���������
    3). ���ڴ��г��е��޸�ҳд����ԭ�еĴ����ļ��С�
    ����д�뵽���ݿ��ļ���ԭ������Ϊcache��������ôд�����̽����������ύ�����Ǽ���������ҳ�����޸ġ������ڽ��������޸ı�д�뵽���ݿ��ļ�֮ǰ���ع���־���뱻��һ��д�������С���Ҫע�����ǣ�д�����̻�ȡ�������������뱻һֱ���У�ֱ�����еĸı䱻�ύʱΪֹ����Ҳ��ζ�ţ������ݵ�һ�α�ˢ�µ������ļ���ʼ��ֱ���������ύ֮ǰ�������Ľ��̲��ܷ��ʸ����ݿ⡣
    ��д������׼���ύʱ������ѭ���²��裺
    4). ��ȡ��������ͬʱȷ�������ڴ��еı仯���ݶ���д�뵽�����ļ��С�
    5). ���������ݿ��ļ��ı仯����������д�뵽�����С�
    6). ɾ����־�ļ���������ɾ��֮ǰ����ϵͳ���ϣ���������һ�δ򿪸����ݿ�ʱ�Խ����ڸ�HOT��־���лָ�����������ֻ���ڳɹ�ɾ����־�ļ�֮�������ǲſ�����Ϊ�������ɹ����ɡ�
    7). �����ݿ��ļ���ɾ�����е���������PENDING����
    һ��PENDING�����ͷţ������Ľ��̾Ϳ��Կ�ʼ�ٴζ�ȡ���ݿ��ˡ�
    ����һ�������а����������ݿ����޸ģ���ô�����ύ�߼�����Ϊ���ӣ������²��裺
    4). ȷ��ÿ�����ݿ��ļ����Ѿ���������������һ����Ч����־�ļ���
    5). ���������ݿ���־�ļ���ͬʱ��ÿ�����ݿ��Ļع���־�ļ����ļ���д�뵽�������ݿ���־�ļ��С�
    6). �ٽ������ݿ���־�ļ����ļ����ֱ�д�뵽ÿ�����ݿ��ع���־�ļ���ָ��λ���С�
    7). �����е����ݿ��仯�־û������ݿ������ļ��С�
    8). ɾ������־�ļ���������ɾ��֮ǰ����ϵͳ���ϣ���������һ�δ򿪸����ݿ�ʱ�Խ����ڸ�HOT��־���лָ�����������ֻ���ڳɹ�ɾ������־�ļ�֮�������ǲſ�����Ϊ�������ɹ����ɡ�
    9). ɾ��ÿ�����ݿ����Ե���־�ļ���
    10).���������ݿ���ɾ������������PENDING����
 
    ������Ҫ˵�����ǣ���SQLite2�У����������������ڴ����ݿ��ж�ȡ���ݣ�Ҳ����˵�����ݿ�ʼ�ն��ж���������������ÿһʱ�̸����ݿⶼ��������һ�ѹ��������������ᵼ��û���κν��̿���ִ��д��������Ϊ�����ݿ����ж�����ʱ�����޷���ȡд���ģ����ǽ��������γ�Ϊ"д����"����SQLite3�У�ͨ��ʹ��PENDING������Ч�ı�����"д����"���εķ�������ĳһ���̳���PENDING��ʱ���Ѿ����ڵĶ��������Լ������У�ֱ�������������������µĶ����󽫲����ٱ�SQLite���ܣ����������еĶ�����ȫ�������󣬳���PENDING���Ľ��̾Ϳ��Ա������ͼ��һ����ȡ���������������ݵ��޸Ĳ�����
 
�塢SQL�������������ƣ�
 
    SQLite3��ʵ����ȷʵ�������Ͳ�������������һЩ���ɵı仯���ر��Ƕ���������һSQL���Լ�������������ȱʡ�����£�SQLite3�Ὣ���е�SQL��������antocommitģʽ�£����������������ݿ����޸Ĳ��󶼻���SQL����ִ�н��������Զ��ύ����SQLite�У�SQL����"BEGIN TRANSACTION"������ʽ������һ�����񣬼�������SQL������ִ�к󶼲����Զ��ύ��������Ҫ�ȵ�SQL����"COMMIT"��"ROLLBACK"��ִ��ʱ���ſ����ύ���ǻع����ɴ˿����ƶϳ�����BEGIN���ִ�к���û�����������κ����͵�����������ִ�е�һ��SELECT����ʱ�ŵõ�һ������������������ִ�е�һ��DML����ʱ�Ż���һ����������������������ֻ�������ݴ��ڴ�д������ʱ��ʼ��ֱ�������ύ���ع�֮ǰ���ܳ�����������
    ��������SQL������ͬһ��ʱ��ͬһ�����ݿ������б�ִ�У�autocommit���ᱻ�ӳ�ִ�У�ֱ������һ���������ɡ����磬����һ��SELECT�������ڱ�ִ�У�����������ִ���ڼ䣬��Ҫ�������м����������м�¼��������ʱ�������������߳���Ϊҵ���߼�����Ҫ����ʱ���𲢴��ڵȴ�״̬�����������̴߳�ʱ�������ڸ������϶Ը����ݿ�ִ��INSERT��UPDATE��DELETE�����ô������Щ���������������޸Ķ������ȵ�SELECT�������������ܱ��ύ��

*//*
** Check that there are at least nSavepoint savepoints open. If there are
** currently less than nSavepoints open, then open one or more savepoints
** to make up the difference. If the number of savepoints is already
** equal to nSavepoint, then this function is a no-op.
**
** If a memory allocation fails, SQLITE_NOMEM is returned. If an error
** occurs while opening the sub-journal file, then an IO error code is
** returned. Otherwise, SQLITE_OK.


//��������������nSavepoint�����㿪�š���������Ŀǰ����nSavepoints����,
��ô����һ�������򱣴������ֲ���������������������Ѿ�����nSavepoint,��ô����������һ���ղ�����

//�����ڴ�����ʧ��,����SQLITE_NOMEM�������ڴ�������־�ļ�ʱ��һ����������,��ô����һ��IO�������롣
����,����SQLITE_OK��
*/

int sqlite3PagerOpenSavepoint(Pager *pPager, int nSavepoint){
  int rc = SQLITE_OK;                       /* Return code */
  int nCurrent = pPager->nSavepoint;        /* Current number of savepoints ��ǰ����������Ŀ*/
 
  assert( pPager->eState>=PAGER_WRITER_LOCKED );
  assert( assert_pager_state(pPager) );
 
  if( nSavepoint>nCurrent && pPager->useJournal ){
    int ii;                                 /* Iterator variable ����������*/
    PagerSavepoint *aNew;                   /* New Pager.aSavepoint array */
 
    /* Grow the Pager.aSavepoint array using realloc(). Return SQLITE_NOMEM
    ** if the allocation fails. Otherwise, zero the new portion in case a
    ** malloc failure occurs while populating it in the for(...) loop below.
       Ӧ��realloc()����Pager.aSavepoint���顣��������ʧ�ܣ�����SQLITE_NOMEM�������������ò��֡��Է�
       ��������forѭ�������ڼ䣬���ַ���ʧ��*/
    aNew = (PagerSavepoint *)sqlite3Realloc(
        pPager->aSavepoint, sizeof(PagerSavepoint)*nSavepoint
    );
    if( !aNew ){
      return SQLITE_NOMEM;
    }
    memset(&aNew[nCurrent], 0, (nSavepoint-nCurrent) * sizeof(PagerSavepoint));
    pPager->aSavepoint = aNew;
 
    /* Populate the PagerSavepoint structures just allocated. �����ոշ�����ҳ�汣����*/
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

//�����������������ع����ͷ�(�ύ)�����㡣�ͷŻ��ع���savepoint��Ҫ�������������ı����㡣

//����op��SAVEPOINT_ROLLBACK��SAVEPOINT_RELEASE��������SAVEPOINT_RELEASE,
��ô������iSavepoint�ͷŲ����ٱ����㣻������SAVEPOINT_ROLLBACK,
Ȼ���ع������ڴ���ָ���ı��������������ĸ���

//�ع������ͷŵı������Ǳ�����iSavepointʶ���ġ�һ��0ֵ��ζ���������棨���ȱ���������savepoint�ϲ�����
һ��Pager.nSavepoint -1��ֵ��ζ���ڴ���������������savepoint�ϲ�����
����iSavepoint�Ǳȣ�Pager.nSavepoint -1����������ô�����������޲����ġ�

//����һ����ֵ�����ݵ����������У���ô��ǰ�����񱻻ع���
��������sqlite3PagerRollback()�ǲ�ͬ�ģ���Ϊ�������������ս��������߽������ݿ⣬
��ֻ�ǻָ����ݿ����ݵ�����ԭʼ״̬��

//���κ������£����е��б�iSavepoint���������ı����㶼�������ˡ�
����������һ���ͷŲ��󣨲���==SAVEPOINT_RELEASE������ô������iSavepointҲ�����١�

//����һ���ڴ�����ʧ�ܣ���ô�����������ܷ���SQLITE_NOMEM��
���������ڻع�һ��savepointʱ����һ��IO��������ô�������ܷ���һ��IO�������룻
����û�д�������������SQLITE_OK
*/

int sqlite3PagerSavepoint(Pager *pPager, int op, int iSavepoint){
  int rc = pPager->errCode;       /* Return code */
 
  assert( op==SAVEPOINT_RELEASE || op==SAVEPOINT_ROLLBACK );
  assert( iSavepoint>=0 || op==SAVEPOINT_ROLLBACK );
 
  if( rc==SQLITE_OK && iSavepoint<pPager->nSavepoint ){
    int ii;            /* Iterator variable */
    int nNew;          /* Number of remaining savepoints after this op. �˲���֮����ʣ���ı���������Ŀ*/
 
    /* Figure out how many savepoints will still be active after this
    ** operation. Store this value in nNew. Then free resources associated
    ** with any savepoints that are destroyed by this operation.
       ��������֮�����������ж��ٱ������ǻ�Ծ�ġ��ѽ����洢��nNew�С�Ȼ���ͷŷ�������Щ�����ٵı���������Դ*/
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
        ����һ���ع����󣬻طŷ���Щȷ���ı����㡣��������һ����ʱ�ļ�����ô������־�ļ���û�б��򿪡�
        �����������£����ݿ��ļ���û�б仯�����ԻطŲ���Ҳ���Ա�������*/
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


/*��������������ʱ,����һ�������ݿ��ļ��ϵĹ�����������sqlite3PagerAcquire()�ǷǷ��ģ�
ֱ�������������ɹ�����֮������������������������ʱһ���������Ѿ��������ˣ������޲����ġ�

//�����Ĳ���Ҳ����������ִ�У�
1)����ҳ�浱ǰ��PAGER_OPEN״̬(�����ݿ��ļ�������),Ȼ����ͼ�����ݿ��ļ��ϻ���һ������������
�������ù�����֮��,�ļ�ϵͳ����������־,�������ڵĻ��Ǿͻطš������κ�������־�ع�,
���������ݱ��������ݿ��ļ�ͷ�ġ�change-counter����������֤��������������ʱ��Ч�Ļ��������������ݡ�
2)����ҳ���ڶ�ռ��������ģʽ����,����Ŀǰ�����κ�ҳ����˵��û��δʹ�õ�����,���Ҵ��ڴ���״̬��,
��ôͨ������ҳ�滺�����ݺͻع��κδ򿪵���־�ļ�����������״̬��

//����һ�гɹ�,����SQLITE_OK���������������ݿ���ʱ��һ��IO����������
����������־�ļ����߻ع�һ����־�ļ�,����IO�������롣
*/

int sqlite3PagerSharedLock(Pager *pPager){
  int rc = SQLITE_OK;                /* Return code */
 
  /* This routine is only called from b-tree and only when there are no
  ** outstanding pages. This implies that the pager state should either
  ** be OPEN or READER. READER is only possible if the pager is or was in
  ** exclusive access mode.
     ������δ������ҳ����ʱ�����������̲Ż���b-tree�����á�����ζ��ҳ��״̬Ҫô��OPEN��Ҫô��READER��
     ����ҳ�洦����������ģʽ��READER�Ż�����*/
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
       ����һ����־�ļ����ڣ����������ݿ��ļ���û�б���������ô��ô��Ҫô��Ҫ���طţ�Ҫô��Ҫ��ɾ��*/
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
          �����ݿ��ļ���ȡ�ų�������ʱ�����������������������������Ǻ���Ҫ�ġ�������Ҫ��
          ��ô��һ������Ҳ�����������ݿ��ļ���̽�Ᵽ���������ҵó����۵�������Ȼ�ڻع�hot-journal
          �����ݿ��ǰ�ȫ�ĵġ�
          ��Ϊ�м��ı������������������κ������Ľ�����ͼ�������ݿ��ļ������ڴ����������������⣬����
          �����ݿ��ļ���ȡ�ų���ʧ�ܡ�
          ����ҳ�洦��locking_mode=exclusiveģʽ����ô�ں�������֮ǰ�����ή��������*/
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
         �����ļ�û�б��򿪲����������ڴ����У�������־����ȡ��/д��������Ϊ������ģʽ�У��ļ�������
         ���ᱻһֱ�򿪣����ҿ���֮����������������ô��Ҫ����д���������ң�д������������journal_mode=persist
         ģʽ����ֹ��־.
         ������־�����ڣ���ͨ��˵��һЩ�������ӳɹ�������������Ӱ����ų���֮ǰ�ع򡣻��ߣ�����Ҳ����ζ��
         ���������������ò�����־�ļ������ڣ���ôҳ�洦�ڴ���״̬��*/
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
         �طŲ���ɾ����־��ɾ�����ݿ�д�������һ�ȡ�������ڻط�hot-journal֮ǰ�����棬����������ֹ
         ��һ�»��档�ڻط�����־֮ǰͬ��������Ϊ��ͻ�Ľ��̺�ʣ��������־����û��ͬ��������������Ҫ�ڻط���־֮ǰ
         ͬ����־*/
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
           ���򿪻��ع���������������־��ʱ�����������ִ�����������֧�ͻᱻִ�С��ڷ���δ����
           �ļ�֮ǰ��pager_unlock()���̽��ᱻ���á�����δ������ʧ�ܣ���ôPager.eLock���뱻���ó�
           UNKNOWN_LOCK��
           Ϊ�˻�ȡpager_unlock()�����������ڰ�Pager.eState����ΪPAGER_ERROR���Ⲣ���ᱻ�����Ƕ����ļ�
           ״̬ͼ���������ڴ���״̬�����ڵ���assert_pager_state()����ʧ�ܣ���Ϊ������û�ж�����ҳ�棬Ӧ�ò���
           ���ڴ���״̬��*/
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
         �Ѿ������ݿ��ļ��л��ù����������һ������Ѿ���ҳ�档����һ�����ݿ��Ƿ񱻸��ġ�
         �������ݿ��Ѿ��ı䣬ˢ�»��档
         ͨ���鿴�ļ�ƫ��24λ��ʼ��15�������������⵽���ݿ��仯����16���ص�ǰ�ĸ���32λ������������
         ������ÿһ�εı仯�������ӣ��������ı������仯�����ڴ���Ӧ�õ�ʱ�������ļ��仯�����仯��
         Ҳ�������Ծ����ı仯�����������鷢���ļ���С�����Ժ��ԡ�*/
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
       �����ļ�ϵͳ��û��WAL�ļ�����WALģʽ�´������ݿ⡣���������µĺ����������޲�����*/
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
/*��������������ʱ,����һ�������ݿ��ļ��ϵĹ�������
����sqlite3PagerAcquire()�ǷǷ��ģ�ֱ�������������ɹ�����֮����
��������������������ʱһ���������Ѿ��������ˣ������޲����ġ�

//�����Ĳ���Ҳ����������ִ�У�
1)����ҳ�浱ǰ��PAGER_OPEN״̬(�����ݿ��ļ�������),Ȼ����ͼ�����ݿ��ļ��ϻ���һ������������
�������ù�����֮��,�ļ�ϵͳ����������־,�������ڵĻ��Ǿͻطš�
�����κ�������־�ع�,���������ݱ��������ݿ��ļ�ͷ�ġ�change-counter����������֤��
������������ʱ��Ч�Ļ��������������ݡ�
2)����ҳ���ڶ�ռ��������ģʽ����,����Ŀǰ�����κ�ҳ����˵��
û��δʹ�õ�����,���Ҵ��ڴ���״̬��,��ôͨ������ҳ�滺�����ݺͻع��κδ򿪵���־�ļ�����������״̬��

//����һ�гɹ�,����SQLITE_OK���������������ݿ���ʱ��һ��IO����������
����������־�ļ����߻ع�һ����־�ļ�,����IO�������롣*/
*/

int sqlite3PagerSharedLock(Pager *pPager){
  int rc = SQLITE_OK;                /* Return code */
 
  /* This routine is only called from b-tree and only when there are no
  ** outstanding pages. This implies that the pager state should either
  ** be OPEN or READER. READER is only possible if the pager is or was in
  ** exclusive access mode.
      ������û������ҳ����ʱ�򣬸����̲Żᱻb-tree���á�����ζ��ҳ��״̬��OPEN����READER��
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
       ������־�ļ����ڣ����������ݿ��ļ�û�б���������ô����Ҫ���طŻ���ɾ��*/
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
         �����ݿ��ļ���������������ʱû�а����������Ǻ���Ҫ�ġ������У���ô��һ������Ҳ�����������ݿ��ļ���
         ̽�Ᵽ���������ҵó����ۣ��ڽ�����Ȼ�ع�����־�ڼ䣬ȥ�����ݿ��ܰ�ȫ��
         ����ҳ�洦��locking_mode=exclusiveģʽ���ں�������ǰ�����ή����������*/
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
         ������û���Ѿ��򿪲����ļ������ڴ��̣�Ϊ������д����������־��*/
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


//�������ڸ���ҳ���ĵײ�VFS֧�ֶ���дǰ��־�ܱ�Ҫ��ԭ�������
*/

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


//����sqlite3WalOpen()����WAL����������������������ʱҳ����exclusive-lockingģʽ,
�����ݿ��ļ��ϲ�ȡ�������������ö��ڴ����洢wal-index��
����,ʹ�������Ĺ����ڴ棨shared-memory����
*/
static int pagerOpenWal(Pager *pPager){
  int rc = SQLITE_OK;

  assert( pPager->pWal==0 && pPager->tempFile==0 );
  assert( pPager->eLock==SHARED_LOCK || pPager->eLock==EXCLUSIVE_LOCK );

  /* If the pager is already in exclusive-mode, the WAL module will use 
  ** heap-memory for the wal-index instead of the VFS shared-memory 
  ** implementation. Take the exclusive lock now, before opening the WAL
  ** file, to make sure this is safe.
      ����ҳ���Ѿ���������ģʽ����ôWALģʽΪwal-indexʹ�ö�ģʽ������ʵ��VFS����ģʽ��
      ����ʹ�������������ٴ���WAL�ļ�֮ǰ��ȷ�����ǰ�ȫ�ġ�*/
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

//�����������������رն�����־�ļ������ӣ��ڴ�WALת�����ع�ģʽ֮ǰ��
//�ڹر���־�ļ�֮ǰ������������ͼ�����ݿ��ļ��ϲ�ȡһ����������
�������ܻ�ȡ��һ��������SQLITE_BUSY�������ز�����־����û�йرա������ɹ����������ڷ���֮ǰ���ͷš�
*/

int sqlite3PagerCloseWal(Pager *pPager){
  int rc = SQLITE_OK;
 
  assert( pPager->journalMode==PAGER_JOURNALMODE_WAL );
 
  /* If the log file is not already open, but does exist in the file-system,
  ** it may need to be checkpointed before the connection can switch to
  ** rollback mode. Open it now so this can happen.
     ������־�ļ���û���Ѿ��򿪣����Ǵ������ļ�ϵͳ�У���ô�����ӿ���ת���ɻع�ģʽ֮ǰ����Ҫ���顣
     ���ڴ����������Żᷢ����*/
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
      �����㲢�ҹر���־����Ϊ�����������ӵ����ݿ��ļ��ϣ���־����־�ܽ��ļ����ᱻɾ��*/
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

//���������������õ�ʱ����һ������һ����ҳ���ϡ�
����ҳ����WALģʽ����WAL�ļ���ǰ����һ�����߶������ܣ����ش洢��WAL�����е�ҳ��ͼ����С�ı�������
���⣬�����ⲻ��һ��WAL���ݿ�����WAL�ļ�Ϊ�յĻ�������0.*/

int sqlite3PagerWalFramesize(Pager *pPager){
  assert( pPager->eState==PAGER_READER );
  return sqlite3WalFramesize(pPager->pWal);
}
 
/*
** Return TRUE if the database file is opened read-only.  Return FALSE
** if the database is (in theory) writable.
*/

//�������ݿ��ļ�������ֻ���ľͷ����档�������ݿ��������Ͽ�д�ģ����ؼ١�
u8 sqlite3PagerIsreadonly(Pager *pPager){
  return pPager->readOnly;
}

/*
** Return the number of references to the pager.
*/
//����ҳ��������
int sqlite3PagerRefcount(Pager *pPager){
  return sqlite3PcacheRefCount(pPager->pPCache);
}
 
/*
** Return the approximate number of bytes of memory currently
** used by the pager and its associated cache.
*/
//���ص�ǰ��ҳ��ʹ�õ��ڴ����Ʊ����������Ĺ�������
int sqlite3PagerMemUsed(Pager *pPager){
  int perPageSize = pPager->pageSize + pPager->nExtra + sizeof(PgHdr)
                                     + 5*sizeof(void*);
  return perPageSize*sqlite3PcachePagecount(pPager->pPCache)
           + sqlite3MallocSize(pPager)
           + pPager->pageSize;
}

/*
** Return the full pathname of the database file.
** Except, if the pager is in-memory only, then return an empty string if
** nullIfMemDb is true.  This routine is called with nullIfMemDb==1 when
** used to report the filename to the user, for compatibility with legacy
** behavior.  But when the Btree needs to know the filename for matching to
** shared cache, it uses nullIfMemDb==0 so that in-memory databases can
** participate in shared-cache.
//�������ݿ��ļ�������·������
//����,����ҳ��ֻ���ڴ�����nullIfMemDbΪ��,��ô����һ�����ַ�����
�����������û������ļ���ʱ�����̱���nullIfMemDb = = 1���ã������Ų���Ϊ�ļ��ݡ�
���ǵ�Btree��Ҫ֪���ļ�����ƥ�乲���棬����nullIfMemDb==0�������ڴ����ݿ����Բ��뵽shared-cache�С�
*/

const char *sqlite3PagerFilename(Pager *pPager, int nullIfMemDb){
  return (nullIfMemDb && pPager->memDb) ? "" : pPager->zFilename;
}


/*
** Return the VFS structure for the pager.
*/
//����ҳ�淵��VFS�ṹ

const sqlite3_vfs *sqlite3PagerVfs(Pager *pPager){
  return pPager->pVfs;
}


/*
** Return the file handle for the database file associated
** with the pager.  This might return NULL if the file has
** not yet been opened.
*/

//������ҳ�����ص����ݿ��ļ������������ļ�����δ���򿪣������ܷ���NULL��
sqlite3_file *sqlite3PagerFile(Pager *pPager){
  return pPager->fd;
}

/*
** Return the full pathname of the journal file.
*/
//������־�ļ��������ļ���
const char *sqlite3PagerJournalname(Pager *pPager){
  return pPager->zJournal;
}


/*
** Return true if fsync() calls are disabled for this pager.  Return FALSE
** if fsync()s are executed normally.
*/

//����fsync()���ö�������ҳ���ǲ����õģ������档����fsync()���������򷵻ؼ١�
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

//����һ��ָ��ҳ���ڲ����еġ���ʱҳ�桱������ָ�롣���򻺳����㹻��������һ�����ݿ�ҳ�����������ݡ�
���򻺳����������ع��ڼ����ڲ������õģ��������ۺ�ʱһ���ع�������ʱ�򶼽�����д��
��������ģ��Ҳ�������ɵ����ã�ֻҪû�лع�������
*/

void *sqlite3PagerTempSpace(Pager *pPager){
  return pPager->pTmpSpace;
}

/*
** Return true if this is an in-memory pager.
*/
//��������һ���ڴ�ҳ���Ļ�������
int sqlite3PagerIsMemdb(Pager *pPager){
  return MEMDB;
}


/*
** Parameter eStat must be either SQLITE_DBSTATUS_CACHE_HIT or
** SQLITE_DBSTATUS_CACHE_MISS. Before returning, *pnVal is incremented by the
** current cache hit or miss count, according to the value of eStat. If the 
** reset parameter is non-zero, the cache hit or miss count is zeroed before 
** returning.

//����eStatһ����SQLITE_DBSTATUS_CACHE_HIT����SQLITE_DBSTATUS_CACHE_MISS��
�ڷ���֮ǰ��*pnVal��ͨ����ǰ�������л���δ���м��������ӣ�ͨ��eStat��ֵ��
�������ò����Ƿ��㣬�������л���δ�����ڷ���ǰ����0.
*/
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
//��������һ���ڴ�������ʱ���ݿ⣬����ҳ�滺��
void sqlite3PagerClearCache(Pager *pPager){
  if( !MEMDB && pPager->tempFile==0 ) pager_reset(pPager);
}


/*
** Truncate the in-memory database file image to nPage pages. This 
** function does not actually modify the database file on disk. It 
** just sets the internal state of the pager object so that the 
** truncation will be done when the current transaction is committed.

//�ض��ڴ����ݿ��ļ�ͼ����nPageҳ���ϡ��������ܲ��������޸Ĵ����ϵ����ݿ��ļ���
��ֻ������ҳ���������ڲ�״̬����������ǰ�������ύ��ʱ�򣬽ضϽ������ɡ�
*/

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


//����ҳ������д����־�ļ��У����������Ǳ�walģ�����õġ�
�ú�������������ҳ�����ݵĻ�����������һ��ָ�롣��������ʧ�ܣ����������᷵�ؿա�
*/

void *sqlite3PagerCodec(PgHdr *pPg){
  void *aData = 0;
  CODEC2(pPg->pPager, pPg->pData, pPg->pgno, 6, return 0, aData);
  return aData;
}
/*
** Return the page number for page pPg.
*/

//����ҳ��pPg��ҳ��
Pgno sqlite3PagerPagenumber(DbPage *pPg){
  return pPg->pgno;
}
int sqlite3PagerIswriteable(DbPage *pPg){
  return pPg->flags&PGHDR_DIRTY;
}
/*
** This routine is used for testing and analysis only.
*/
//��������ֻ���������Ժͷ���
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
1/* ��������ÿ����־�ļ�������Ҫ���ٿռ䣨����������������Ҫ��־������־�������Ƕ���һ���ڴ���־�ļ�������һ����������־�ļ���������Ҫ�������ռ��ͳ����ռ䡣
ע��һ�򡰹�����־�ļ����󡱿�����һ�򻺳��ڴ洢�еĵ�һ������־�ļ��İ�װ����������ʵʩ�Զ�д���Ż�
2/* ��һ���������Ļ���������������Ϊ0
3/* �����ʹ洢����·�����ڷ��仺������ָ���� ��ͨ��zPathname,nPathname���ȣ�������,��������һ����ʱ�ļ�,��nPathname��zPathname����Ϊ0��
4 /*����ҳ���ṹ�����洢��ҳ���洢�����������ļ������������ݿ��ļ�������־�ļ������ڴ洢�е�����������ʾ
5/* ����һ��ҳ���ļ�
6.1/* ����һ��ҳ���ļ�
   6.1.1/* �����ļ��ǳɹ������ڴ򿪺��Ķ���д���̣��������Ǵ���һ�����ݿ��ļ���ѡ��һ��Ĭ��ҳ����С��
   Ĭ��ҳ����С������ֵ������ֵ��Ĭ��ҳ����С+���󲿷ִ�С�ķ���ֵ+���Ա��Զ�д������ҳ����С��
6.2/* ����һ����ʱ�ļ���Ҫ�����ⲻ�Ǳ������򿪵ġ������������У����ǽ���Ĭ��ҳ����С��������ֱ����һ������OsWrite()��ʵ�ʴ����ļ���������֧Ҳ����һ���ڴ����ݿ����С�
һ���ڴ����ݿ��Ǻ���ʱ�ļ�һ���ľ�����д�����̲�������һ���ڴ��ع���־��
7 /* ��������PagerSetPagesize()����������ҳ����Сֵ���ҷ���ҳ����ʱ�ռ仺�档
8 /* ����һ�����������������κ�һ��ģ�飬�ͷ�ҳ���ṹ���ҹر��ļ���
9/* ��ʼ��ҳ�滺������
1//�ر�ҳ�滺�档�ͷ������ڴ��͹ر������ļ���������������������ʱ����������������,��ô�����ع�������δ������ҳ������Ч�Ĳ��������ڴ汻�ͷš�
�κ���ͼ������ҳ���뻺��ҳ��������ҳ�棬�������������غ����ܻᵼ��coredump��ò���������ѵ���˼����
//�����������ǳɹ�������һ�����񼤻���һ����ʱ���ع�������������һ������������һ��������־�Ļع��ڼ䣬��������һ���ļ�ϵͳ�ж�û�д��󷵻ص������С�
2//�������Ǵ���״̬����־�ļ��ڵ��� UnlockAndRollback���������һع��� ֮ǰ��ͬ���ġ�������û�б����ɣ���ôһ�򿪷ŵ���־�ļ��Ĳ�ͬ���Ĳ��ֿ��ܱ����з��ص����ݿ��С�
���������е��з����������ϣ����ݿ����ܳ�Ϊcorrupt�����ܣ������ǲ��õ���˼����
//����������ͬ����־��ʱ��һ�����������ˣ�ת��ҳ����������״̬���ŵ����� UnlockAndRollback���������һع��� �������������ݿⲢ�Ҳ����Իع�������������ֱ�ӹر���־�ļ���
1//�ر�ҳ�滺�档�ͷ������ڴ��͹ر������ļ���������������������ʱ����������������,��ô�����ع�������δ������ҳ������Ч�Ĳ��������ڴ汻�ͷš�
�κ���ͼ������ҳ���뻺��ҳ��������ҳ�棬�������������غ����ܻᵼ��coredump��ò���������ѵ���˼����
//�����������ǳɹ�������һ�����񼤻���һ����ʱ���ع�������������һ������������һ��������־�Ļع��ڼ䣬��������һ���ļ�ϵͳ�ж�û�д��󷵻ص������С�
2//�������Ǵ���״̬����־�ļ��ڵ��� UnlockAndRollback���������һع��� ֮ǰ��ͬ���ġ�������û�б����ɣ���ôһ�򿪷ŵ���־�ļ��Ĳ�ͬ���Ĳ��ֿ��ܱ����з��ص����ݿ��С�
���������е��з����������ϣ����ݿ����ܳ�Ϊcorrupt�����ܣ������ǲ��õ���˼����
//����������ͬ����־��ʱ��һ�����������ˣ�ת��ҳ����������״̬���ŵ����� UnlockAndRollback���������һع��� �������������ݿⲢ�Ҳ����Իع�������������ֱ�ӹر���־�ļ���
?//����æʱ������������������ͼ��һ��������������һ��������ʱ��sqlite3OsLock()����SQLITE_BUSY�����ӹ����������ɱ�������ʱ�򣬻��ߵ��ӹ�������������������ʱ�򣬲�����æ������
��������������־�ع�ʱ�����ܽ᣺
?
?//**   ����                         | ���� xBusyHandler
?**   --------------------------------------------------------
?**   ����       ->������      | Yes
?**   ������   -> ������    | No
?**   ������   -> ������   | No
?**   ������ -> ������   | Yes
?
?//����æ�����ص󷵻ط���,�����ԡ�����������0,Ȼ��SQLITE_BUSY���󷵻ظ�ҳ��API���ܵĵ�����
?//����æʱ������������������ͼ��һ��������������һ��������ʱ��sqlite3OsLock()����SQLITE_BUSY��
���ӹ����������ɱ�������ʱ�򣬻��ߵ��ӹ�������������������ʱ�򣬲�����æ���󡣣�������������־�ع�ʱ�����ܽ᣺
?
?//**   ����                         | ���� xBusyHandler
?**   --------------------------------------------------------
?**   ����       ->������      | Yes
?**   ������   -> ������    | No
?**   ������   -> ������   | No
?**   ������ -> ������   | Yes
?
?//����æ�����ص󷵻ط���,�����ԡ�����������0,Ȼ��SQLITE_BUSY���󷵻ظ�ҳ��API���ܵĵ�����
�����������ݿ�ҳ����������ֵ������mxPage�����ġ�
����mxPageΪ������Ϊ�����Ǿͺ��޸ı䡣������Զ��Ҫ����ҳ���ۼ���������ֵ���������ڵ�ǰ���ݿ���С��
