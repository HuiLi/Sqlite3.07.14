/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This is the implementation of the page cache subsystem or "pager".
    这是页面缓存子系统的实现或者叫"pager"。
** The pager is used to access a database disk file.  It implements
** atomic commit and rollback through the use of a journal file that
** is separate from the database file.  The pager also implements file
** locking to prevent two processes from writing the same database
** file simultaneously, or one process from reading the database while
** another is writing.
    Pager是用于访问数据库的磁盘文件。它通过使用独立于数据库文件的日志文件实现原子提交和恢复。
    Pager同样实现利用文件锁防止两个进程同时编写相同的数据库，或者防止一个进程读取正在被编写的数据库。
*/
#ifndef SQLITE_OMIT_DISKIO
#include "sqliteInt.h"
#include "wal.h"


/******************* NOTES ON THE DESIGN OF THE PAGER ************************
**
** This comment block describes invariants that hold when using a rollback
** journal.  These invariants do not apply for journal_mode=WAL,
** journal_mode=MEMORY, or journal_mode=OFF.
**
** Within this comment block, a page is deemed to have been synced
** automatically as soon as it is written when PRAGMA synchronous=OFF.
** Otherwise, the page is not synced until the xSync method of the VFS
** is called successfully on the file containing the page.
**
** Definition:  A page of the database file is said to be "overwriteable" if
** one or more of the following are true about the page:
** 
**     (a)  The original content of the page as it was at the beginning of
**          the transaction has been written into the rollback journal and
**          synced.
** 
**     (b)  The page was a freelist leaf page at the start of the transaction.
** 
**     (c)  The page number is greater than the largest page that existed in
**          the database file at the start of the transaction.
** 
** (1) A page of the database file is never overwritten unless one of the
**     following are true:
** 
**     (a) The page and all other pages on the same sector are overwriteable.
** 
**     (b) The atomic page write optimization is enabled, and the entire
**         transaction other than the update of the transaction sequence
**         number consists of a single page change.
** 
** (2) The content of a page written into the rollback journal exactly matches
**     both the content in the database when the rollback journal was written
**     and the content in the database at the beginning of the current
**     transaction.
** 
** (3) Writes to the database file are an integer multiple of the page size
**     in length and are aligned on a page boundary.
** 
** (4) Reads from the database file are either aligned on a page boundary and
**     an integer multiple of the page size in length or are taken from the
**     first 100 bytes of the database file.
** 
** (5) All writes to the database file are synced prior to the rollback journal
**     being deleted, truncated, or zeroed.
** 
** (6) If a master journal file is used, then all writes to the database file
**     are synced prior to the master journal being deleted.
** 
** Definition: Two databases (or the same database at two points it time)
** are said to be "logically equivalent" if they give the same answer to
** all queries.  Note in particular the content of freelist leaf
** pages can be changed arbitarily without effecting the logical equivalence
** of the database.
** 
** (7) At any time, if any subset, including the empty set and the total set,
**     of the unsynced changes to a rollback journal are removed and the 
**     journal is rolled back, the resulting database file will be logical
**     equivalent to the database file at the beginning of the transaction.
** 
** (8) When a transaction is rolled back, the xTruncate method of the VFS
**     is called to restore the database file to the same size it was at
**     the beginning of the transaction.  (In some VFSes, the xTruncate
**     method is a no-op, but that does not change the fact the SQLite will
**     invoke it.)
** 
** (9) Whenever the database file is modified, at least one bit in the range
**     of bytes from 24 through 39 inclusive will be changed prior to releasing
**     the EXCLUSIVE lock, thus signaling other connections on the same
**     database to flush their caches.
**
** (10) The pattern of bits in bytes 24 through 39 shall not repeat in less
**      than one billion transactions.
**
** (11) A database file is well-formed at the beginning and at the conclusion
**      of every transaction.
**
** (12) An EXCLUSIVE lock is held on the database file when writing to
**      the database file.
**
** (13) A SHARED lock is held on the database file while reading any
**      content out of the database file.
**
******************************************************************************/

/*
** Macros for troubleshooting.  Normally turned off
*/
#if 0
int sqlite3PagerTrace=1;  /* True to enable tracing */
#define sqlite3DebugPrintf printf
#define PAGERTRACE(X)     if( sqlite3PagerTrace ){ sqlite3DebugPrintf X; }
#else
#define PAGERTRACE(X)
#endif

/*
** The following two macros are used within the PAGERTRACE() macros above
** to print out file-descriptors. 
**
** PAGERID() takes a pointer to a Pager struct as its argument. The
** associated file-descriptor is returned. FILEHANDLEID() takes an sqlite3_file
** struct as its argument.
*/
#define PAGERID(p) ((int)(p->fd))
#define FILEHANDLEID(fd) ((int)fd)

/*
** The Pager.eState variable stores the current 'state' of a pager. A
** pager may be in any one of the seven states shown in the following
** state diagram.
**
**                            OPEN <------+------+
**                              |         |      |
**                              V         |      |
**               +---------> READER-------+      |
**               |              |                |
**               |              V                |
**               |<-------WRITER_LOCKED------> ERROR
**               |              |                ^  
**               |              V                |
**               |<------WRITER_CACHEMOD-------->|
**               |              |                |
**               |              V                |
**               |<-------WRITER_DBMOD---------->|
**               |              |                |
**               |              V                |
**               +<------WRITER_FINISHED-------->+
**
**
** List of state transitions and the C [function] that performs each:
** 
**   OPEN              -> READER              [sqlite3PagerSharedLock]
**   READER            -> OPEN                [pager_unlock]
**
**   READER            -> WRITER_LOCKED       [sqlite3PagerBegin]
**   WRITER_LOCKED     -> WRITER_CACHEMOD     [pager_open_journal]
**   WRITER_CACHEMOD   -> WRITER_DBMOD        [syncJournal]
**   WRITER_DBMOD      -> WRITER_FINISHED     [sqlite3PagerCommitPhaseOne]
**   WRITER_***        -> READER              [pager_end_transaction]
**
**   WRITER_***        -> ERROR               [pager_error]
**   ERROR             -> OPEN                [pager_unlock]
** 
**
**  OPEN:
**
**    The pager starts up in this state. Nothing is guaranteed in this
**    state - the file may or may not be locked and the database size is
**    unknown. The database may not be read or written.
**
**    * No read or write transaction is active.
**    * Any lock, or no lock at all, may be held on the database file.
**    * The dbSize, dbOrigSize and dbFileSize variables may not be trusted.
**
**  READER:
**
**    In this state all the requirements for reading the database in 
**    rollback (non-WAL) mode are met. Unless the pager is (or recently
**    was) in exclusive-locking mode, a user-level read transaction is 
**    open. The database size is known in this state.
**
**    A connection running with locking_mode=normal enters this state when
**    it opens a read-transaction on the database and returns to state
**    OPEN after the read-transaction is completed. However a connection
**    running in locking_mode=exclusive (including temp databases) remains in
**    this state even after the read-transaction is closed. The only way
**    a locking_mode=exclusive connection can transition from READER to OPEN
**    is via the ERROR state (see below).
** 
**    * A read transaction may be active (but a write-transaction cannot).
**    * A SHARED or greater lock is held on the database file.
**    * The dbSize variable may be trusted (even if a user-level read 
**      transaction is not active). The dbOrigSize and dbFileSize variables
**      may not be trusted at this point.
**    * If the database is a WAL database, then the WAL connection is open.
**    * Even if a read-transaction is not open, it is guaranteed that 
**      there is no hot-journal in the file-system.
**
**  WRITER_LOCKED:
**
**    The pager moves to this state from READER when a write-transaction
**    is first opened on the database. In WRITER_LOCKED state, all locks 
**    required to start a write-transaction are held, but no actual 
**    modifications to the cache or database have taken place.
**
**    In rollback mode, a RESERVED or (if the transaction was opened with 
**    BEGIN EXCLUSIVE) EXCLUSIVE lock is obtained on the database file when
**    moving to this state, but the journal file is not written to or opened 
**    to in this state. If the transaction is committed or rolled back while 
**    in WRITER_LOCKED state, all that is required is to unlock the database 
**    file.
**
**    IN WAL mode, WalBeginWriteTransaction() is called to lock the log file.
**    If the connection is running with locking_mode=exclusive, an attempt
**    is made to obtain an EXCLUSIVE lock on the database file.
**
**    * A write transaction is active.
**    * If the connection is open in rollback-mode, a RESERVED or greater 
**      lock is held on the database file.
**    * If the connection is open in WAL-mode, a WAL write transaction
**      is open (i.e. sqlite3WalBeginWriteTransaction() has been successfully
**      called).
**    * The dbSize, dbOrigSize and dbFileSize variables are all valid.
**    * The contents of the pager cache have not been modified.
**    * The journal file may or may not be open.
**    * Nothing (not even the first header) has been written to the journal.
**
**  WRITER_CACHEMOD:
**
**    A pager moves from WRITER_LOCKED state to this state when a page is
**    first modified by the upper layer. In rollback mode the journal file
**    is opened (if it is not already open) and a header written to the
**    start of it. The database file on disk has not been modified.
**
**    * A write transaction is active.
**    * A RESERVED or greater lock is held on the database file.
**    * The journal file is open and the first header has been written 
**      to it, but the header has not been synced to disk.
**    * The contents of the page cache have been modified.
**
**  WRITER_DBMOD:
**
**    The pager transitions from WRITER_CACHEMOD into WRITER_DBMOD state
**    when it modifies the contents of the database file. WAL connections
**    never enter this state (since they do not modify the database file,
**    just the log file).
**
**    * A write transaction is active.
**    * An EXCLUSIVE or greater lock is held on the database file.
**    * The journal file is open and the first header has been written 
**      and synced to disk.
**    * The contents of the page cache have been modified (and possibly
**      written to disk).
**
**  WRITER_FINISHED:
**
**    It is not possible for a WAL connection to enter this state.
**
**    A rollback-mode pager changes to WRITER_FINISHED state from WRITER_DBMOD
**    state after the entire transaction has been successfully written into the
**    database file. In this state the transaction may be committed simply
**    by finalizing the journal file. Once in WRITER_FINISHED state, it is 
**    not possible to modify the database further. At this point, the upper 
**    layer must either commit or rollback the transaction.
**
**    * A write transaction is active.
**    * An EXCLUSIVE or greater lock is held on the database file.
**    * All writing and syncing of journal and database data has finished.
**      If no error occured, all that remains is to finalize the journal to
**      commit the transaction. If an error did occur, the caller will need
**      to rollback the transaction. 
**
**  ERROR:
**
**    The ERROR state is entered when an IO or disk-full error (including
**    SQLITE_IOERR_NOMEM) occurs at a point in the code that makes it 
**    difficult to be sure that the in-memory pager state (cache contents, 
**    db size etc.) are consistent with the contents of the file-system.
**
**    Temporary pager files may enter the ERROR state, but in-memory pagers
**    cannot.
**
**    For example, if an IO error occurs while performing a rollback, 
**    the contents of the page-cache may be left in an inconsistent state.
**    At this point it would be dangerous to change back to READER state
**    (as usually happens after a rollback). Any subsequent readers might
**    report database corruption (due to the inconsistent cache), and if
**    they upgrade to writers, they may inadvertently corrupt the database
**    file. To avoid this hazard, the pager switches into the ERROR state
**    instead of READER following such an error.
**
**    Once it has entered the ERROR state, any attempt to use the pager
**    to read or write data returns an error. Eventually, once all 
**    outstanding transactions have been abandoned, the pager is able to
**    transition back to OPEN state, discarding the contents of the 
**    page-cache and any other in-memory state at the same time. Everything
**    is reloaded from disk (and, if necessary, hot-journal rollback peformed)
**    when a read-transaction is next opened on the pager (transitioning
**    the pager into READER state). At that point the system has recovered 
**    from the error.
**
**    Specifically, the pager jumps into the ERROR state if:
**
**      1. An error occurs while attempting a rollback. This happens in
**         function sqlite3PagerRollback().
**
**      2. An error occurs while attempting to finalize a journal file
**         following a commit in function sqlite3PagerCommitPhaseTwo().
**
**      3. An error occurs while attempting to write to the journal or
**         database file in function pagerStress() in order to free up
**         memory.
**
**    In other cases, the error is returned to the b-tree layer. The b-tree
**    layer then attempts a rollback operation. If the error condition 
**    persists, the pager enters the ERROR state via condition (1) above.
**
**    Condition (3) is necessary because it can be triggered by a read-only
**    statement executed within a transaction. In this case, if the error
**    code were simply returned to the user, the b-tree layer would not
**    automatically attempt a rollback, as it assumes that an error in a
**    read-only statement cannot leave the pager in an internally inconsistent 
**    state.
**
**    * The Pager.errCode variable is set to something other than SQLITE_OK.
**    * There are one or more outstanding references to pages (after the
**      last reference is dropped the pager should move back to OPEN state).
**    * The pager is not an in-memory pager.
**    
**
** Notes:
**
**   * A pager is never in WRITER_DBMOD or WRITER_FINISHED state if the
**     connection is open in WAL mode. A WAL connection is always in one
**     of the first four states.
**
**   * Normally, a connection open in exclusive mode is never in PAGER_OPEN
**     state. There are two exceptions: immediately after exclusive-mode has
**     been turned on (and before any read or write transactions are 
**     executed), and when the pager is leaving the "error state".
**
**   * See also: assert_pager_state().
*/
#define PAGER_OPEN                  0
#define PAGER_READER                1
#define PAGER_WRITER_LOCKED         2
#define PAGER_WRITER_CACHEMOD       3
#define PAGER_WRITER_DBMOD          4
#define PAGER_WRITER_FINISHED       5
#define PAGER_ERROR                 6

/*
** The Pager.eLock variable is almost always set to one of the 
** following locking-states, according to the lock currently held on
** the database file: NO_LOCK, SHARED_LOCK, RESERVED_LOCK or EXCLUSIVE_LOCK.
** This variable is kept up to date as locks are taken and released by
** the pagerLockDb() and pagerUnlockDb() wrappers.
**
** If the VFS xLock() or xUnlock() returns an error other than SQLITE_BUSY
** (i.e. one of the SQLITE_IOERR subtypes), it is not clear whether or not
** the operation was successful. In these circumstances pagerLockDb() and
** pagerUnlockDb() take a conservative approach - eLock is always updated
** when unlocking the file, and only updated when locking the file if the
** VFS call is successful. This way, the Pager.eLock variable may be set
** to a less exclusive (lower) value than the lock that is actually held
** at the system level, but it is never set to a more exclusive value.
**
** This is usually safe. If an xUnlock fails or appears to fail, there may 
** be a few redundant xLock() calls or a lock may be held for longer than
** required, but nothing really goes wrong.
**
** The exception is when the database file is unlocked as the pager moves
** from ERROR to OPEN state. At this point there may be a hot-journal file 
** in the file-system that needs to be rolled back (as part of a OPEN->SHARED
** transition, by the same pager or any other). If the call to xUnlock()
** fails at this point and the pager is left holding an EXCLUSIVE lock, this
** can confuse the call to xCheckReservedLock() call made later as part
** of hot-journal detection.
**
** xCheckReservedLock() is defined as returning true "if there is a RESERVED 
** lock held by this process or any others". So xCheckReservedLock may 
** return true because the caller itself is holding an EXCLUSIVE lock (but
** doesn't know it because of a previous error in xUnlock). If this happens
** a hot-journal may be mistaken for a journal being created by an active
** transaction in another process, causing SQLite to read from the database
** without rolling it back.
**
** To work around this, if a call to xUnlock() fails when unlocking the
** database in the ERROR state, Pager.eLock is set to UNKNOWN_LOCK. It
** is only changed back to a real locking state after a successful call
** to xLock(EXCLUSIVE). Also, the code to do the OPEN->SHARED state transition
** omits the check for a hot-journal if Pager.eLock is set to UNKNOWN_LOCK 
** lock. Instead, it assumes a hot-journal exists and obtains an EXCLUSIVE
** lock on the database file before attempting to roll it back. See function
** PagerSharedLock() for more detail.
**
** Pager.eLock may only be set to UNKNOWN_LOCK when the pager is in 
** PAGER_OPEN state.
*/
#define UNKNOWN_LOCK                (EXCLUSIVE_LOCK+1)

/*
** A macro used for invoking the codec if there is one
*/
#ifdef SQLITE_HAS_CODEC
# define CODEC1(P,D,N,X,E) \
    if( P->xCodec && P->xCodec(P->pCodec,D,N,X)==0 ){ E; }
# define CODEC2(P,D,N,X,E,O) \
    if( P->xCodec==0 ){ O=(char*)D; }else \
    if( (O=(char*)(P->xCodec(P->pCodec,D,N,X)))==0 ){ E; }
#else
# define CODEC1(P,D,N,X,E)   /* NO-OP */
# define CODEC2(P,D,N,X,E,O) O=(char*)D
#endif

/*
** The maximum allowed sector size. 64KiB. If the xSectorsize() method 
** returns a value larger than this, then MAX_SECTOR_SIZE is used instead.
** This could conceivably cause corruption following a power failure on
** such a system. This is currently an undocumented limit.
*/
#define MAX_SECTOR_SIZE 0x10000

/*
** An instance of the following structure is allocated for each active
** savepoint and statement transaction in the system. All such structures
** are stored in the Pager.aSavepoint[] array, which is allocated and
** resized using sqlite3Realloc().
**
** When a savepoint is created, the PagerSavepoint.iHdrOffset field is
** set to 0. If a journal-header is written into the main journal while
** the savepoint is active, then iHdrOffset is set to the byte offset 
** immediately following the last journal record written into the main
** journal before the journal-header. This is required during savepoint
** rollback (see pagerPlaybackSavepoint()).
*/
typedef struct PagerSavepoint PagerSavepoint;
struct PagerSavepoint {
  i64 iOffset;                 /* Starting offset in main journal */
  i64 iHdrOffset;              /* See above */
  Bitvec *pInSavepoint;        /* Set of pages in this savepoint */
  Pgno nOrig;                  /* Original number of pages in file */
  Pgno iSubRec;                /* Index of first record in sub-journal */
#ifndef SQLITE_OMIT_WAL
  u32 aWalData[WAL_SAVEPOINT_NDATA];        /* WAL savepoint context */
#endif
};

/*
** A open page cache is an instance of struct Pager. A description of
** some of the more important member variables follows:
**
** eState
**
**   The current 'state' of the pager object. See the comment and state
**   diagram above for a description of the pager state.
**
** eLock
**
**   For a real on-disk database, the current lock held on the database file -
**   NO_LOCK, SHARED_LOCK, RESERVED_LOCK or EXCLUSIVE_LOCK.
**
**   For a temporary or in-memory database (neither of which require any
**   locks), this variable is always set to EXCLUSIVE_LOCK. Since such
**   databases always have Pager.exclusiveMode==1, this tricks the pager
**   logic into thinking that it already has all the locks it will ever
**   need (and no reason to release them).
**
**   In some (obscure) circumstances, this variable may also be set to
**   UNKNOWN_LOCK. See the comment above the #define of UNKNOWN_LOCK for
**   details.
**
** changeCountDone
**
**   This boolean variable is used to make sure that the change-counter 
**   (the 4-byte header field at byte offset 24 of the database file) is 
**   not updated more often than necessary. //布尔变量去确认change-counter不是经常更新而是必要的更新
**
**   It is set to true when the change-counter field is updated, which 
**   can only happen if an exclusive lock is held on the database file.
**   It is cleared (set to false) whenever an exclusive lock is 
**   relinquished on the database file. Each time a transaction is committed,
**   The changeCountDone flag is inspected. If it is true, the work of
**   updating the change-counter is omitted for the current transaction.
//////change-counter文件更新了。则把changeCountDone设置为true，这只能发生在数据库文件是有独立锁的情况，
**当数据库文件的独立锁被消除时，设置为false,每当一个事务被提交，changeCountDone 会检查，如果它为true，再更新change-counter
//将会忽略当前的处理。
**   This mechanism means that when running in exclusive mode, a connection 
**   need only update the change-counter once, for the first transaction
**   committed.这个原理意思就是当在独占模式下运行时，一个连接仅仅需要更新change-counter一次，只是对第一次的提交。
**
** setMaster
**
**   When PagerCommitPhaseOne() is called to commit a transaction, it may
**   (or may not) specify a master-journal name to be written into the 
**   journal file before it is synced to disk.
**
**   Whether or not a journal file contains a master-journal pointer affects 
**   the way in which the journal file is finalized after the transaction is 
**   committed or rolled back when running in "journal_mode=PERSIST" mode.
**   If a journal file does not contain a master-journal pointer, it is
**   finalized by overwriting the first journal header with zeroes. If
**   it does contain a master-journal pointer the journal file is finalized 
**   by truncating it to zero bytes, just as if the connection were 
**   running in "journal_mode=truncate" mode.
**
**   Journal files that contain master journal pointers cannot be finalized
**   simply by overwriting the first journal-header with zeroes, as the
**   master journal pointer could interfere with hot-journal rollback of any
**   subsequently interrupted transaction that reuses the journal file.
**
**   The flag is cleared as soon as the journal file is finalized (either
**   by PagerCommitPhaseTwo or PagerRollback). If an IO error prevents the
**   journal file from being successfully finalized, the setMaster flag
**   is cleared anyway (and the pager will move to ERROR state).
**
** doNotSpill, doNotSyncSpill
**
**   These two boolean variables control the behaviour of cache-spills
**   (calls made by the pcache module to the pagerStress() routine to
**   write cached data to the file-system in order to free up memory).
**
**   When doNotSpill is non-zero, writing to the database from pagerStress()
**   is disabled altogether. This is done in a very obscure case that
**   comes up during savepoint rollback that requires the pcache module
**   to allocate a new page to prevent the journal file from being written
**   while it is being traversed by code in pager_playback().
** 
**   If doNotSyncSpill is non-zero, writing to the database from pagerStress()
**   is permitted, but syncing the journal file is not. This flag is set
**   by sqlite3PagerWrite() when the file-system sector-size is larger than
**   the database page-size in order to prevent a journal sync from happening 
**   in between the journalling of two pages on the same sector. 
**
** subjInMemory
**
**   This is a boolean variable. If true, then any required sub-journal
**   is opened as an in-memory journal file. If false, then in-memory
**   sub-journals are only used for in-memory pager files.
**
**   This variable is updated by the upper layer each time a new 
**   write-transaction is opened.
**
** dbSize, dbOrigSize, dbFileSize
**
**   Variable dbSize is set to the number of pages in the database file.
**   It is valid in PAGER_READER and higher states (all states except for
**   OPEN and ERROR). 
**
**   dbSize is set based on the size of the database file, which may be 
**   larger than the size of the database (the value stored at offset
**   28 of the database header by the btree). If the size of the file
**   is not an integer multiple of the page-size, the value stored in
**   dbSize is rounded down (i.e. a 5KB file with 2K page-size has dbSize==2).
**   Except, any file that is greater than 0 bytes in size is considered
**   to have at least one page. (i.e. a 1KB file with 2K page-size leads
**   to dbSize==1).
**
**   During a write-transaction, if pages with page-numbers greater than
**   dbSize are modified in the cache, dbSize is updated accordingly.
**   Similarly, if the database is truncated using PagerTruncateImage(), 
**   dbSize is updated.
**
**   Variables dbOrigSize and dbFileSize are valid in states 
**   PAGER_WRITER_LOCKED and higher. dbOrigSize is a copy of the dbSize
**   variable at the start of the transaction. It is used during rollback,
**   and to determine whether or not pages need to be journalled before
**   being modified.
**
**   Throughout a write-transaction, dbFileSize contains the size of
**   the file on disk in pages. It is set to a copy of dbSize when the
**   write-transaction is first opened, and updated when VFS calls are made
**   to write or truncate the database file on disk. 
**
**   The only reason the dbFileSize variable is required is to suppress 
**   unnecessary calls to xTruncate() after committing a transaction. If, 
**   when a transaction is committed, the dbFileSize variable indicates 
**   that the database file is larger than the database image (Pager.dbSize), 
**   pager_truncate() is called. The pager_truncate() call uses xFilesize()
**   to measure the database file on disk, and then truncates it if required.
**   dbFileSize is not used when rolling back a transaction. In this case
**   pager_truncate() is called unconditionally (which means there may be
**   a call to xFilesize() that is not strictly required). In either case,
**   pager_truncate() may cause the file to become smaller or larger.
**
** dbHintSize
**
**   The dbHintSize variable is used to limit the number of calls made to
**   the VFS xFileControl(FCNTL_SIZE_HINT) method. 
**
**   dbHintSize is set to a copy of the dbSize variable when a
**   write-transaction is opened (at the same time as dbFileSize and
**   dbOrigSize). If the xFileControl(FCNTL_SIZE_HINT) method is called,
**   dbHintSize is increased to the number of pages that correspond to the
**   size-hint passed to the method call. See pager_write_pagelist() for 
**   details.
**
** errCode
**
**   The Pager.errCode variable is only ever used in PAGER_ERROR state. It
**   is set to zero in all other states. In PAGER_ERROR state, Pager.errCode 
**   is always set to SQLITE_FULL, SQLITE_IOERR or one of the SQLITE_IOERR_XXX 
**   sub-codes.仅仅用在PAGER_ERROR状态。
*/
struct Pager {
  sqlite3_vfs *pVfs;          /* OS functions to use for IO */
  u8 exclusiveMode;           /* Boolean. True if locking_mode==EXCLUSIVE */
  u8 journalMode;             /* One of the PAGER_JOURNALMODE_* values */
  u8 useJournal;              /* Use a rollback journal on this file */
  u8 noSync;                  /* Do not sync the journal if true */
  u8 fullSync;                /* Do extra syncs of the journal for robustness */
  u8 ckptSyncFlags;           /* SYNC_NORMAL or SYNC_FULL for checkpoint */
  u8 walSyncFlags;            /* SYNC_NORMAL or SYNC_FULL for wal writes */
  u8 syncFlags;               /* SYNC_NORMAL or SYNC_FULL otherwise */
  u8 tempFile;                /* zFilename is a temporary file */
  u8 readOnly;                /* True for a read-only database */
  u8 memDb;                   /* True to inhibit all file I/O */

  /**************************************************************************
  ** The following block contains those class members that change during
  ** routine opertion.  Class members not in this block are either fixed
  ** when the pager is first created or else only change when there is a
  ** significant mode change (such as changing the page_size, locking_mode,
  ** or the journal_mode).  From another view, these class members describe
  ** the "state" of the pager, while other class members describe the
  ** "configuration" of the pager.
  */
  u8 eState;                  /* Pager state (OPEN, READER, WRITER_LOCKED..) */
  u8 eLock;                   /* Current lock held on database file */
  u8 changeCountDone;         /* Set after incrementing the change-counter */
  u8 setMaster;               /* True if a m-j name has been written to jrnl */
  u8 doNotSpill;              /* Do not spill the cache when non-zero */
  u8 doNotSyncSpill;          /* Do not do a spill that requires jrnl sync */
  u8 subjInMemory;            /* True to use in-memory sub-journals */
  Pgno dbSize;                /* Number of pages in the database */
  Pgno dbOrigSize;            /* dbSize before the current transaction */
  Pgno dbFileSize;            /* Number of pages in the database file */
  Pgno dbHintSize;            /* Value passed to FCNTL_SIZE_HINT call */
  int errCode;                /* One of several kinds of errors */
  int nRec;                   /* Pages journalled since last j-header written */
  u32 cksumInit;              /* Quasi-random value added to every checksum */
  u32 nSubRec;                /* Number of records written to sub-journal */
  Bitvec *pInJournal;         /* One bit for each page in the database file */ /*数据库文件的每一页中的一位*/
  sqlite3_file *fd;           /* File descriptor for database */
  sqlite3_file *jfd;          /* File descriptor for main journal */
  sqlite3_file *sjfd;         /* File descriptor for sub-journal */
  i64 journalOff;             /* Current write offset in the journal file */
  i64 journalHdr;             /* Byte offset to previous journal header */
  sqlite3_backup *pBackup;    /* Pointer to list of ongoing backup processes */
  PagerSavepoint *aSavepoint; /* Array of active savepoints */
  int nSavepoint;             /* Number of elements in aSavepoint[] */
  char dbFileVers[16];        /* Changes whenever database file changes */
  /*
  ** End of the routinely-changing class members
  ***************************************************************************/

  u16 nExtra;                 /* Add this many bytes to each in-memory page */
  i16 nReserve;               /* Number of unused bytes at end of each page */
  u32 vfsFlags;               /* Flags for sqlite3_vfs.xOpen() */
  u32 sectorSize;             /* Assumed sector size during rollback */
  int pageSize;               /* Number of bytes in a page */
  Pgno mxPgno;                /* Maximum allowed size of the database */
  i64 journalSizeLimit;       /* Size limit for persistent journal files */
  char *zFilename;            /* Name of the database file */
  char *zJournal;             /* Name of the journal file */
  int (*xBusyHandler)(void*); /* Function to call when busy */
  void *pBusyHandlerArg;      /* Context argument for xBusyHandler */
  int aStat[3];               /* Total cache hits, misses and writes */
#ifdef SQLITE_TEST
  int nRead;                  /* Database pages read */
#endif
  void (*xReiniter)(DbPage*); /* Call this routine when reloading pages */
#ifdef SQLITE_HAS_CODEC
  void *(*xCodec)(void*,void*,Pgno,int); /* Routine for en/decoding data */
  void (*xCodecSizeChng)(void*,int,int); /* Notify of page size changes */
  void (*xCodecFree)(void*);             /* Destructor for the codec */
  void *pCodec;               /* First argument to xCodec... methods */
#endif
  char *pTmpSpace;            /* Pager.pageSize bytes of space for tmp use */
  PCache *pPCache;            /* Pointer to page cache object */
#ifndef SQLITE_OMIT_WAL
  Wal *pWal;                  /* Write-ahead log used by "journal_mode=wal" */
  char *zWal;                 /* File name for write-ahead log */
#endif
};

/*
** Indexes for use with Pager.aStat[]. The Pager.aStat[] array contains
** the values accessed by passing SQLITE_DBSTATUS_CACHE_HIT, CACHE_MISS 
** or CACHE_WRITE to sqlite3_db_status().
*/
#define PAGER_STAT_HIT   0
#define PAGER_STAT_MISS  1
#define PAGER_STAT_WRITE 2

/*
** The following global variables hold counters used for
** testing purposes only.  These variables do not exist in
** a non-testing build.  These variables are not thread-safe.
*/
#ifdef SQLITE_TEST
int sqlite3_pager_readdb_count = 0;    /* Number of full pages read from DB */
int sqlite3_pager_writedb_count = 0;   /* Number of full pages written to DB */
int sqlite3_pager_writej_count = 0;    /* Number of pages written to journal */
# define PAGER_INCR(v)  v++
#else
# define PAGER_INCR(v)
#endif



/*
** Journal files begin with the following magic string.  The data
** was obtained from /dev/random.  It is used only as a sanity check.
**
** Since version 2.8.0, the journal format contains additional sanity
** checking information.  If the power fails while the journal is being
** written, semi-random garbage data might appear in the journal
** file after power is restored.  If an attempt is then made
** to roll the journal back, the database could be corrupted.  The additional
** sanity checking data is an attempt to discover the garbage in the
** journal and ignore it.
**
** The sanity checking information for the new journal format consists
** of a 32-bit checksum on each page of data.  The checksum covers both
** the page number and the pPager->pageSize bytes of data for the page.
** This cksum is initialized to a 32-bit random value that appears in the
** journal file right after the header.  The random initializer is important,
** because garbage data that appears at the end of a journal is likely
** data that was once in other files that have now been deleted.  If the
** garbage data came from an obsolete journal file, the checksums might
** be correct.  But by initializing the checksum to random value which
** is different for every journal, we minimize that risk.
*/
static const unsigned char aJournalMagic[] = {
  0xd9, 0xd5, 0x05, 0xf9, 0x20, 0xa1, 0x63, 0xd7,
};

/*
** The size of the of each page record in the journal is given by
** the following macro.
*/
#define JOURNAL_PG_SZ(pPager)  ((pPager->pageSize) + 8)

/*
** The journal header size for this pager. This is usually the same 
** size as a single disk sector. See also setSectorSize().
*/
#define JOURNAL_HDR_SZ(pPager) (pPager->sectorSize)

/*
** The macro MEMDB is true if we are dealing with an in-memory database.
** We do this as a macro so that if the SQLITE_OMIT_MEMORYDB macro is set,
** the value of MEMDB will be a constant and the compiler will optimize
** out code that would never execute.
*/
#ifdef SQLITE_OMIT_MEMORYDB
# define MEMDB 0
#else
# define MEMDB pPager->memDb
#endif

/*
** The maximum legal page number is (2^31 - 1).
*/
#define PAGER_MAX_PGNO 2147483647

/*
** The argument to this macro is a file descriptor (type sqlite3_file*).
** Return 0 if it is not open, or non-zero (but not 1) if it is.
**
** This is so that expressions can be written as:
**
**   if( isOpen(pPager->jfd) ){ ...
**
** instead of
**
**   if( pPager->jfd->pMethods ){ ...
*/
#define isOpen(pFd) ((pFd)->pMethods)

/*
** Return true if this pager uses a write-ahead log instead of the usual
** rollback journal. Otherwise false.
*/
#ifndef SQLITE_OMIT_WAL
static int pagerUseWal(Pager *pPager){
  return (pPager->pWal!=0);
}
#else
# define pagerUseWal(x) 0
# define pagerRollbackWal(x) 0
# define pagerWalFrames(v,w,x,y) 0
# define pagerOpenWalIfPresent(z) SQLITE_OK
# define pagerBeginReadTransaction(z) SQLITE_OK
#endif

#ifndef NDEBUG 
/*
** Usage:
**
**   assert( assert_pager_state(pPager) );
**
** This function runs many asserts to try to find inconsistencies in
** the internal state of the Pager object.
*/
static int assert_pager_state(Pager *p){
  Pager *pPager = p;

  /* State must be valid. */
  assert( p->eState==PAGER_OPEN
       || p->eState==PAGER_READER
       || p->eState==PAGER_WRITER_LOCKED
       || p->eState==PAGER_WRITER_CACHEMOD
       || p->eState==PAGER_WRITER_DBMOD
       || p->eState==PAGER_WRITER_FINISHED
       || p->eState==PAGER_ERROR
  );

  /* Regardless of the current state, a temp-file connection always behaves
  ** as if it has an exclusive lock on the database file. It never updates
  ** the change-counter field, so the changeCountDone flag is always set.
  */
  assert( p->tempFile==0 || p->eLock==EXCLUSIVE_LOCK );
  assert( p->tempFile==0 || pPager->changeCountDone );

  /* If the useJournal flag is clear, the journal-mode must be "OFF". 
  ** And if the journal-mode is "OFF", the journal file must not be open.
  */
  assert( p->journalMode==PAGER_JOURNALMODE_OFF || p->useJournal );
  assert( p->journalMode!=PAGER_JOURNALMODE_OFF || !isOpen(p->jfd) );

  /* Check that MEMDB implies noSync. And an in-memory journal. Since 
  ** this means an in-memory pager performs no IO at all, it cannot encounter 
  ** either SQLITE_IOERR or SQLITE_FULL during rollback or while finalizing 
  ** a journal file. (although the in-memory journal implementation may 
  ** return SQLITE_IOERR_NOMEM while the journal file is being written). It 
  ** is therefore not possible for an in-memory pager to enter the ERROR 
  ** state.
  */
  if( MEMDB ){
    assert( p->noSync );
    assert( p->journalMode==PAGER_JOURNALMODE_OFF 
         || p->journalMode==PAGER_JOURNALMODE_MEMORY 
    );
    assert( p->eState!=PAGER_ERROR && p->eState!=PAGER_OPEN );
    assert( pagerUseWal(p)==0 );
  }

  /* If changeCountDone is set, a RESERVED lock or greater must be held
  ** on the file.
  */
  assert( pPager->changeCountDone==0 || pPager->eLock>=RESERVED_LOCK );
  assert( p->eLock!=PENDING_LOCK );

  switch( p->eState ){
    case PAGER_OPEN:
      assert( !MEMDB );
      assert( pPager->errCode==SQLITE_OK );
      assert( sqlite3PcacheRefCount(pPager->pPCache)==0 || pPager->tempFile );
      break;

    case PAGER_READER:
      assert( pPager->errCode==SQLITE_OK );
      assert( p->eLock!=UNKNOWN_LOCK );
      assert( p->eLock>=SHARED_LOCK );
      break;

    case PAGER_WRITER_LOCKED:
      assert( p->eLock!=UNKNOWN_LOCK );
      assert( pPager->errCode==SQLITE_OK );
      if( !pagerUseWal(pPager) ){
        assert( p->eLock>=RESERVED_LOCK );
      }
      assert( pPager->dbSize==pPager->dbOrigSize );
      assert( pPager->dbOrigSize==pPager->dbFileSize );
      assert( pPager->dbOrigSize==pPager->dbHintSize );
      assert( pPager->setMaster==0 );
      break;

    case PAGER_WRITER_CACHEMOD:
      assert( p->eLock!=UNKNOWN_LOCK );
      assert( pPager->errCode==SQLITE_OK );
      if( !pagerUseWal(pPager) ){
        /* It is possible that if journal_mode=wal here that neither the
        ** journal file nor the WAL file are open. This happens during
        ** a rollback transaction that switches from journal_mode=off
        ** to journal_mode=wal.
        */
        assert( p->eLock>=RESERVED_LOCK );
        assert( isOpen(p->jfd) 
             || p->journalMode==PAGER_JOURNALMODE_OFF 
             || p->journalMode==PAGER_JOURNALMODE_WAL 
        );
      }
      assert( pPager->dbOrigSize==pPager->dbFileSize );
      assert( pPager->dbOrigSize==pPager->dbHintSize );
      break;

    case PAGER_WRITER_DBMOD:
      assert( p->eLock==EXCLUSIVE_LOCK );
      assert( pPager->errCode==SQLITE_OK );
      assert( !pagerUseWal(pPager) );
      assert( p->eLock>=EXCLUSIVE_LOCK );
      assert( isOpen(p->jfd) 
           || p->journalMode==PAGER_JOURNALMODE_OFF 
           || p->journalMode==PAGER_JOURNALMODE_WAL 
      );
      assert( pPager->dbOrigSize<=pPager->dbHintSize );
      break;

    case PAGER_WRITER_FINISHED:
      assert( p->eLock==EXCLUSIVE_LOCK );
      assert( pPager->errCode==SQLITE_OK );
      assert( !pagerUseWal(pPager) );
      assert( isOpen(p->jfd) 
           || p->journalMode==PAGER_JOURNALMODE_OFF 
           || p->journalMode==PAGER_JOURNALMODE_WAL 
      );
      break;

    case PAGER_ERROR:
      /* There must be at least one outstanding reference to the pager if
      ** in ERROR state. Otherwise the pager should have already dropped
      ** back to OPEN state.
      */
      assert( pPager->errCode!=SQLITE_OK );
      assert( sqlite3PcacheRefCount(pPager->pPCache)>0 );
      break;
  }

  return 1;
}
#endif /* ifndef NDEBUG */

#ifdef SQLITE_DEBUG 
/*
** Return a pointer to a human readable string in a static buffer
** containing the state of the Pager object passed as an argument. This
** is intended to be used within debuggers. For example, as an alternative
** to "print *pPager" in gdb:
**
** (gdb) printf "%s", print_pager_state(pPager)
*/
static char *print_pager_state(Pager *p){
  static char zRet[1024];

  sqlite3_snprintf(1024, zRet,
      "Filename:      %s\n"
      "State:         %s errCode=%d\n"
      "Lock:          %s\n"
      "Locking mode:  locking_mode=%s\n"
      "Journal mode:  journal_mode=%s\n"
      "Backing store: tempFile=%d memDb=%d useJournal=%d\n"
      "Journal:       journalOff=%lld journalHdr=%lld\n"
      "Size:          dbsize=%d dbOrigSize=%d dbFileSize=%d\n"
      , p->zFilename
      , p->eState==PAGER_OPEN            ? "OPEN" :
        p->eState==PAGER_READER          ? "READER" :
        p->eState==PAGER_WRITER_LOCKED   ? "WRITER_LOCKED" :
        p->eState==PAGER_WRITER_CACHEMOD ? "WRITER_CACHEMOD" :
        p->eState==PAGER_WRITER_DBMOD    ? "WRITER_DBMOD" :
        p->eState==PAGER_WRITER_FINISHED ? "WRITER_FINISHED" :
        p->eState==PAGER_ERROR           ? "ERROR" : "?error?"
      , (int)p->errCode
      , p->eLock==NO_LOCK         ? "NO_LOCK" :
        p->eLock==RESERVED_LOCK   ? "RESERVED" :
        p->eLock==EXCLUSIVE_LOCK  ? "EXCLUSIVE" :
        p->eLock==SHARED_LOCK     ? "SHARED" :
        p->eLock==UNKNOWN_LOCK    ? "UNKNOWN" : "?error?"
      , p->exclusiveMode ? "exclusive" : "normal"
      , p->journalMode==PAGER_JOURNALMODE_MEMORY   ? "memory" :
        p->journalMode==PAGER_JOURNALMODE_OFF      ? "off" :
        p->journalMode==PAGER_JOURNALMODE_DELETE   ? "delete" :
        p->journalMode==PAGER_JOURNALMODE_PERSIST  ? "persist" :
        p->journalMode==PAGER_JOURNALMODE_TRUNCATE ? "truncate" :
        p->journalMode==PAGER_JOURNALMODE_WAL      ? "wal" : "?error?"
      , (int)p->tempFile, (int)p->memDb, (int)p->useJournal
      , p->journalOff, p->journalHdr
      , (int)p->dbSize, (int)p->dbOrigSize, (int)p->dbFileSize
  );

  return zRet;
}
#endif

/*
** Return true if it is necessary to write page *pPg into the sub-journal.
** A page needs to be written into the sub-journal if there exists one
** or more open savepoints for which:
**
**   * The page-number is less than or equal to PagerSavepoint.nOrig, and
**   * The bit corresponding to the page-number is not set in
**     PagerSavepoint.pInSavepoint.
*/
static int subjRequiresPage(PgHdr *pPg){
  Pgno pgno = pPg->pgno;
  Pager *pPager = pPg->pPager;
  int i;
  for(i=0; i<pPager->nSavepoint; i++){
    PagerSavepoint *p = &pPager->aSavepoint[i];
    if( p->nOrig>=pgno && 0==sqlite3BitvecTest(p->pInSavepoint, pgno) ){
      return 1;
    }
  }
  return 0;
}

/*
** Return true if the page is already in the journal file.
*/
static int pageInJournal(PgHdr *pPg){
  return sqlite3BitvecTest(pPg->pPager->pInJournal, pPg->pgno);
}

/*
** Read a 32-bit integer from the given file descriptor.  Store the integer
** that is read in *pRes.  Return SQLITE_OK if everything worked, or an
** error code is something goes wrong.
**
** All values are stored on disk as big-endian.
*/
static int read32bits(sqlite3_file *fd, i64 offset, u32 *pRes){
  unsigned char ac[4];
  int rc = sqlite3OsRead(fd, ac, sizeof(ac), offset);
  if( rc==SQLITE_OK ){
    *pRes = sqlite3Get4byte(ac);
  }
  return rc;
}

/*
** Write a 32-bit integer into a string buffer in big-endian byte order.
*/
#define put32bits(A,B)  sqlite3Put4byte((u8*)A,B)


/*
** Write a 32-bit integer into the given file descriptor.  Return SQLITE_OK
** on success or an error code is something goes wrong.
*/
static int write32bits(sqlite3_file *fd, i64 offset, u32 val){
  char ac[4];
  put32bits(ac, val);
  return sqlite3OsWrite(fd, ac, 4, offset);
}

/*
** Unlock the database file to level eLock, which must be either NO_LOCK
** or SHARED_LOCK. Regardless of whether or not the call to xUnlock()
** succeeds, set the Pager.eLock variable to match the (attempted) new lock.
**
** Except, if Pager.eLock is set to UNKNOWN_LOCK when this function is
** called, do not modify it. See the comment above the #define of 
** UNKNOWN_LOCK for an explanation of this.
*/
static int pagerUnlockDb(Pager *pPager, int eLock){
  int rc = SQLITE_OK;

  assert( !pPager->exclusiveMode || pPager->eLock==eLock );
  assert( eLock==NO_LOCK || eLock==SHARED_LOCK );
  assert( eLock!=NO_LOCK || pagerUseWal(pPager)==0 );
  if( isOpen(pPager->fd) ){
    assert( pPager->eLock>=eLock );
    rc = sqlite3OsUnlock(pPager->fd, eLock);
    if( pPager->eLock!=UNKNOWN_LOCK ){
      pPager->eLock = (u8)eLock;
    }
    IOTRACE(("UNLOCK %p %d\n", pPager, eLock))
  }
  return rc;
}

/*
** Lock the database file to level eLock, which must be either SHARED_LOCK,
** RESERVED_LOCK or EXCLUSIVE_LOCK. If the caller is successful, set the
** Pager.eLock variable to the new locking state. 
**
** Except, if Pager.eLock is set to UNKNOWN_LOCK when this function is 
** called, do not modify it unless the new locking state is EXCLUSIVE_LOCK. 
** See the comment above the #define of UNKNOWN_LOCK for an explanation 
** of this.
*/
static int pagerLockDb(Pager *pPager, int eLock){
  int rc = SQLITE_OK;

  assert( eLock==SHARED_LOCK || eLock==RESERVED_LOCK || eLock==EXCLUSIVE_LOCK );
  if( pPager->eLock<eLock || pPager->eLock==UNKNOWN_LOCK ){
    rc = sqlite3OsLock(pPager->fd, eLock);
    if( rc==SQLITE_OK && (pPager->eLock!=UNKNOWN_LOCK||eLock==EXCLUSIVE_LOCK) ){
      pPager->eLock = (u8)eLock;
      IOTRACE(("LOCK %p %d\n", pPager, eLock))
    }
  }
  return rc;
}

/*
** This function determines whether or not the atomic-write optimization
** can be used with this pager. The optimization can be used if:
**
**  (a) the value returned by OsDeviceCharacteristics() indicates that
**      a database page may be written atomically, and
**  (b) the value returned by OsSectorSize() is less than or equal
**      to the page size.
**
** The optimization is also always enabled for temporary files. It is
** an error to call this function if pPager is opened on an in-memory
** database.
**
** If the optimization cannot be used, 0 is returned. If it can be used,
** then the value returned is the size of the journal file when it
** contains rollback data for exactly one page.
*/
#ifdef SQLITE_ENABLE_ATOMIC_WRITE
static int jrnlBufferSize(Pager *pPager){
  assert( !MEMDB );
  if( !pPager->tempFile ){
    int dc;                           /* Device characteristics */
    int nSector;                      /* Sector size */
    int szPage;                       /* Page size */

    assert( isOpen(pPager->fd) );
    dc = sqlite3OsDeviceCharacteristics(pPager->fd);
    nSector = pPager->sectorSize;
    szPage = pPager->pageSize;

    assert(SQLITE_IOCAP_ATOMIC512==(512>>8));
    assert(SQLITE_IOCAP_ATOMIC64K==(65536>>8));
    if( 0==(dc&(SQLITE_IOCAP_ATOMIC|(szPage>>8)) || nSector>szPage) ){
      return 0;
    }
  }

  return JOURNAL_HDR_SZ(pPager) + JOURNAL_PG_SZ(pPager);
}
#endif

/*
** If SQLITE_CHECK_PAGES is defined then we do some sanity checking
** on the cache using a hash function.  This is used for testing
** and debugging only.
*/
#ifdef SQLITE_CHECK_PAGES
/*
** Return a 32-bit hash of the page data for pPage.
*/
static u32 pager_datahash(int nByte, unsigned char *pData){
  u32 hash = 0;
  int i;
  for(i=0; i<nByte; i++){
    hash = (hash*1039) + pData[i];
  }
  return hash;
}
static u32 pager_pagehash(PgHdr *pPage){
  return pager_datahash(pPage->pPager->pageSize, (unsigned char *)pPage->pData);
}
static void pager_set_pagehash(PgHdr *pPage){
  pPage->pageHash = pager_pagehash(pPage);
}

/*
** The CHECK_PAGE macro takes a PgHdr* as an argument. If SQLITE_CHECK_PAGES
** is defined, and NDEBUG is not defined, an assert() statement checks
** that the page is either dirty or still matches the calculated page-hash.
*/
#define CHECK_PAGE(x) checkPage(x)
static void checkPage(PgHdr *pPg){
  Pager *pPager = pPg->pPager;
  assert( pPager->eState!=PAGER_ERROR );
  assert( (pPg->flags&PGHDR_DIRTY) || pPg->pageHash==pager_pagehash(pPg) );
}

#else
#define pager_datahash(X,Y)  0
#define pager_pagehash(X)  0
#define pager_set_pagehash(X)
#define CHECK_PAGE(x)
#endif  /* SQLITE_CHECK_PAGES */

/*
** When this is called the journal file for pager pPager must be open.
** This function attempts to read a master journal file name from the 
** end of the file and, if successful, copies it into memory supplied 
** by the caller. See comments above writeMasterJournal() for the format
** used to store a master journal file name at the end of a journal file.
**
** zMaster must point to a buffer of at least nMaster bytes allocated by
** the caller. This should be sqlite3_vfs.mxPathname+1 (to ensure there is
** enough space to write the master journal name). If the master journal
** name in the journal is longer than nMaster bytes (including a
** nul-terminator), then this is handled as if no master journal name
** were present in the journal.
**
** If a master journal file name is present at the end of the journal
** file, then it is copied into the buffer pointed to by zMaster. A
** nul-terminator byte is appended to the buffer following the master
** journal file name.
**
** If it is determined that no master journal file name is present 
** zMaster[0] is set to 0 and SQLITE_OK returned.
**
** If an error occurs while reading from the journal file, an SQLite
** error code is returned.
*/
static int readMasterJournal(sqlite3_file *pJrnl, char *zMaster, u32 nMaster){
  int rc;                    /* Return code */
  u32 len;                   /* Length in bytes of master journal name */
  i64 szJ;                   /* Total size in bytes of journal file pJrnl */
  u32 cksum;                 /* MJ checksum value read from journal */
  u32 u;                     /* Unsigned loop counter */
  unsigned char aMagic[8];   /* A buffer to hold the magic header */
  zMaster[0] = '\0';

  if( SQLITE_OK!=(rc = sqlite3OsFileSize(pJrnl, &szJ))
   || szJ<16
   || SQLITE_OK!=(rc = read32bits(pJrnl, szJ-16, &len))
   || len>=nMaster 
   || SQLITE_OK!=(rc = read32bits(pJrnl, szJ-12, &cksum))
   || SQLITE_OK!=(rc = sqlite3OsRead(pJrnl, aMagic, 8, szJ-8))
   || memcmp(aMagic, aJournalMagic, 8)
   || SQLITE_OK!=(rc = sqlite3OsRead(pJrnl, zMaster, len, szJ-16-len))
  ){
    return rc;
  }

  /* See if the checksum matches the master journal name */
  for(u=0; u<len; u++){
    cksum -= zMaster[u];
  }
  if( cksum ){
    /* If the checksum doesn't add up, then one or more of the disk sectors
    ** containing the master journal filename is corrupted. This means
    ** definitely roll back, so just return SQLITE_OK and report a (nul)
    ** master-journal filename.
    */
    len = 0;
  }
  zMaster[len] = '\0';
   
  return SQLITE_OK;
}

/*
** Return the offset of the sector boundary at or immediately 
** following the value in pPager->journalOff, assuming a sector 
** size of pPager->sectorSize bytes.
**
** i.e for a sector size of 512:
**
**   Pager.journalOff          Return value
**   ---------------------------------------
**   0                         0
**   512                       512
**   100                       512
**   2000                      2048
** 
*/
static i64 journalHdrOffset(Pager *pPager){
  i64 offset = 0;
  i64 c = pPager->journalOff;
  if( c ){
    offset = ((c-1)/JOURNAL_HDR_SZ(pPager) + 1) * JOURNAL_HDR_SZ(pPager);
  }
  assert( offset%JOURNAL_HDR_SZ(pPager)==0 );
  assert( offset>=c );
  assert( (offset-c)<JOURNAL_HDR_SZ(pPager) );
  return offset;
}

/*
** The journal file must be open when this function is called.
**
** This function is a no-op if the journal file has not been written to
** within the current transaction (i.e. if Pager.journalOff==0).
**
** If doTruncate is non-zero or the Pager.journalSizeLimit variable is
** set to 0, then truncate the journal file to zero bytes in size. Otherwise,
** zero the 28-byte header at the start of the journal file. In either case, 
** if the pager is not in no-sync mode, sync the journal file immediately 
** after writing or truncating it.
**
** If Pager.journalSizeLimit is set to a positive, non-zero value, and
** following the truncation or zeroing described above the size of the 
** journal file in bytes is larger than this value, then truncate the
** journal file to Pager.journalSizeLimit bytes. The journal file does
** not need to be synced following this operation.
**
** If an IO error occurs, abandon processing and return the IO error code.
** Otherwise, return SQLITE_OK.
*/
static int zeroJournalHdr(Pager *pPager, int doTruncate){
  int rc = SQLITE_OK;                               /* Return code */
  assert( isOpen(pPager->jfd) );
  if( pPager->journalOff ){
    const i64 iLimit = pPager->journalSizeLimit;    /* Local cache of jsl */

    IOTRACE(("JZEROHDR %p\n", pPager))
    if( doTruncate || iLimit==0 ){
      rc = sqlite3OsTruncate(pPager->jfd, 0);
    }else{
      static const char zeroHdr[28] = {0};
      rc = sqlite3OsWrite(pPager->jfd, zeroHdr, sizeof(zeroHdr), 0);
    }
    if( rc==SQLITE_OK && !pPager->noSync ){
      rc = sqlite3OsSync(pPager->jfd, SQLITE_SYNC_DATAONLY|pPager->syncFlags);
    }

    /* At this point the transaction is committed but the write lock 
    ** is still held on the file. If there is a size limit configured for 
    ** the persistent journal and the journal file currently consumes more
    ** space than that limit allows for, truncate it now. There is no need
    ** to sync the file following this operation.
    */
    if( rc==SQLITE_OK && iLimit>0 ){
      i64 sz;
      rc = sqlite3OsFileSize(pPager->jfd, &sz);
      if( rc==SQLITE_OK && sz>iLimit ){
        rc = sqlite3OsTruncate(pPager->jfd, iLimit);
      }
    }
  }
  return rc;
}

/*
** The journal file must be open when this routine is called. A journal
** header (JOURNAL_HDR_SZ bytes) is written into the journal file at the
** current location.
**
** The format for the journal header is as follows:
** - 8 bytes: Magic identifying journal format.
** - 4 bytes: Number of records in journal, or -1 no-sync mode is on.
** - 4 bytes: Random number used for page hash.
** - 4 bytes: Initial database page count.
** - 4 bytes: Sector size used by the process that wrote this journal.
** - 4 bytes: Database page size.
** 
** Followed by (JOURNAL_HDR_SZ - 28) bytes of unused space.
*/
static int writeJournalHdr(Pager *pPager){
  int rc = SQLITE_OK;                 /* Return code */
  char *zHeader = pPager->pTmpSpace;  /* Temporary space used to build header */
  u32 nHeader = (u32)pPager->pageSize;/* Size of buffer pointed to by zHeader */
  u32 nWrite;                         /* Bytes of header sector written */
  int ii;                             /* Loop counter */

  assert( isOpen(pPager->jfd) );      /* Journal file must be open. */

  if( nHeader>JOURNAL_HDR_SZ(pPager) ){
    nHeader = JOURNAL_HDR_SZ(pPager);
  }

  /* If there are active savepoints and any of them were created 
  ** since the most recent journal header was written, update the 
  ** PagerSavepoint.iHdrOffset fields now.
  */
  for(ii=0; ii<pPager->nSavepoint; ii++){
    if( pPager->aSavepoint[ii].iHdrOffset==0 ){
      pPager->aSavepoint[ii].iHdrOffset = pPager->journalOff;
    }
  }

  pPager->journalHdr = pPager->journalOff = journalHdrOffset(pPager);

  /* 
  ** Write the nRec Field - the number of page records that follow this
  ** journal header. Normally, zero is written to this value at this time.
  ** After the records are added to the journal (and the journal synced, 
  ** if in full-sync mode), the zero is overwritten with the true number
  ** of records (see syncJournal()).
  **
  ** A faster alternative is to write 0xFFFFFFFF to the nRec field. When
  ** reading the journal this value tells SQLite to assume that the
  ** rest of the journal file contains valid page records. This assumption
  ** is dangerous, as if a failure occurred whilst writing to the journal
  ** file it may contain some garbage data. There are two scenarios
  ** where this risk can be ignored:
  **
  **   * When the pager is in no-sync mode. Corruption can follow a
  **     power failure in this case anyway.
  **
  **   * When the SQLITE_IOCAP_SAFE_APPEND flag is set. This guarantees
  **     that garbage data is never appended to the journal file.
  */
  assert( isOpen(pPager->fd) || pPager->noSync );
  if( pPager->noSync || (pPager->journalMode==PAGER_JOURNALMODE_MEMORY)
   || (sqlite3OsDeviceCharacteristics(pPager->fd)&SQLITE_IOCAP_SAFE_APPEND) 
  ){
    memcpy(zHeader, aJournalMagic, sizeof(aJournalMagic));
    put32bits(&zHeader[sizeof(aJournalMagic)], 0xffffffff);
  }else{
    memset(zHeader, 0, sizeof(aJournalMagic)+4);
  }

  /* The random check-hash initialiser */ 
  sqlite3_randomness(sizeof(pPager->cksumInit), &pPager->cksumInit);
  put32bits(&zHeader[sizeof(aJournalMagic)+4], pPager->cksumInit);
  /* The initial database size */
  put32bits(&zHeader[sizeof(aJournalMagic)+8], pPager->dbOrigSize);
  /* The assumed sector size for this process */
  put32bits(&zHeader[sizeof(aJournalMagic)+12], pPager->sectorSize);

  /* The page size */
  put32bits(&zHeader[sizeof(aJournalMagic)+16], pPager->pageSize);

  /* Initializing the tail of the buffer is not necessary.  Everything
  ** works find if the following memset() is omitted.  But initializing
  ** the memory prevents valgrind from complaining, so we are willing to
  ** take the performance hit.
  */
  memset(&zHeader[sizeof(aJournalMagic)+20], 0,
         nHeader-(sizeof(aJournalMagic)+20));

  /* In theory, it is only necessary to write the 28 bytes that the 
  ** journal header consumes to the journal file here. Then increment the 
  ** Pager.journalOff variable by JOURNAL_HDR_SZ so that the next 
  ** record is written to the following sector (leaving a gap in the file
  ** that will be implicitly filled in by the OS).
  **
  ** However it has been discovered that on some systems this pattern can 
  ** be significantly slower than contiguously writing data to the file,
  ** even if that means explicitly writing data to the block of 
  ** (JOURNAL_HDR_SZ - 28) bytes that will not be used. So that is what
  ** is done. 
  **
  ** The loop is required here in case the sector-size is larger than the 
  ** database page size. Since the zHeader buffer is only Pager.pageSize
  ** bytes in size, more than one call to sqlite3OsWrite() may be required
  ** to populate the entire journal header sector.
  */ 
  for(nWrite=0; rc==SQLITE_OK&&nWrite<JOURNAL_HDR_SZ(pPager); nWrite+=nHeader){
    IOTRACE(("JHDR %p %lld %d\n", pPager, pPager->journalHdr, nHeader))
    rc = sqlite3OsWrite(pPager->jfd, zHeader, nHeader, pPager->journalOff);
    assert( pPager->journalHdr <= pPager->journalOff );
    pPager->journalOff += nHeader;
  }

  return rc;
}

/*
** The journal file must be open when this is called. A journal header file
** (JOURNAL_HDR_SZ bytes) is read from the current location in the journal
** file. The current location in the journal file is given by
** pPager->journalOff. See comments above function writeJournalHdr() for
** a description of the journal header format.
**
** If the header is read successfully, *pNRec is set to the number of
** page records following this header and *pDbSize is set to the size of the
** database before the transaction began, in pages. Also, pPager->cksumInit
** is set to the value read from the journal header. SQLITE_OK is returned
** in this case.
**
** If the journal header file appears to be corrupted, SQLITE_DONE is
** returned and *pNRec and *PDbSize are undefined.  If JOURNAL_HDR_SZ bytes
** cannot be read from the journal file an error code is returned.
*/
static int readJournalHdr(
  Pager *pPager,               /* Pager object */
  int isHot,
  i64 journalSize,             /* Size of the open journal file in bytes */
  u32 *pNRec,                  /* OUT: Value read from the nRec field */
  u32 *pDbSize                 /* OUT: Value of original database size field */
){
  int rc;                      /* Return code */
  unsigned char aMagic[8];     /* A buffer to hold the magic header */
  i64 iHdrOff;                 /* Offset of journal header being read */

  assert( isOpen(pPager->jfd) );      /* Journal file must be open. */

  /* Advance Pager.journalOff to the start of the next sector. If the
  ** journal file is too small for there to be a header stored at this
  ** point, return SQLITE_DONE.
  */
  pPager->journalOff = journalHdrOffset(pPager);
  if( pPager->journalOff+JOURNAL_HDR_SZ(pPager) > journalSize ){
    return SQLITE_DONE;
  }
  iHdrOff = pPager->journalOff;

  /* Read in the first 8 bytes of the journal header. If they do not match
  ** the  magic string found at the start of each journal header, return
  ** SQLITE_DONE. If an IO error occurs, return an error code. Otherwise,
  ** proceed.
  */
  if( isHot || iHdrOff!=pPager->journalHdr ){
    rc = sqlite3OsRead(pPager->jfd, aMagic, sizeof(aMagic), iHdrOff);
    if( rc ){
      return rc;
    }
    if( memcmp(aMagic, aJournalMagic, sizeof(aMagic))!=0 ){
      return SQLITE_DONE;
    }
  }

  /* Read the first three 32-bit fields of the journal header: The nRec
  ** field, the checksum-initializer and the database size at the start
  ** of the transaction. Return an error code if anything goes wrong.
  */
  if( SQLITE_OK!=(rc = read32bits(pPager->jfd, iHdrOff+8, pNRec))
   || SQLITE_OK!=(rc = read32bits(pPager->jfd, iHdrOff+12, &pPager->cksumInit))
   || SQLITE_OK!=(rc = read32bits(pPager->jfd, iHdrOff+16, pDbSize))
  ){
    return rc;
  }

  if( pPager->journalOff==0 ){
    u32 iPageSize;               /* Page-size field of journal header */
    u32 iSectorSize;             /* Sector-size field of journal header */

    /* Read the page-size and sector-size journal header fields. */
    if( SQLITE_OK!=(rc = read32bits(pPager->jfd, iHdrOff+20, &iSectorSize))
     || SQLITE_OK!=(rc = read32bits(pPager->jfd, iHdrOff+24, &iPageSize))
    ){
      return rc;
    }

    /* Versions of SQLite prior to 3.5.8 set the page-size field of the
    ** journal header to zero. In this case, assume that the Pager.pageSize
    ** variable is already set to the correct page size.
    */
    if( iPageSize==0 ){
      iPageSize = pPager->pageSize;
    }

    /* Check that the values read from the page-size and sector-size fields
    ** are within range. To be 'in range', both values need to be a power
    ** of two greater than or equal to 512 or 32, and not greater than their 
    ** respective compile time maximum limits.
    */
    if( iPageSize<512                  || iSectorSize<32
     || iPageSize>SQLITE_MAX_PAGE_SIZE || iSectorSize>MAX_SECTOR_SIZE
     || ((iPageSize-1)&iPageSize)!=0   || ((iSectorSize-1)&iSectorSize)!=0 
    ){
      /* If the either the page-size or sector-size in the journal-header is 
      ** invalid, then the process that wrote the journal-header must have 
      ** crashed before the header was synced. In this case stop reading 
      ** the journal file here.
      */
      return SQLITE_DONE;
    }

    /* Update the page-size to match the value read from the journal. 
    ** Use a testcase() macro to make sure that malloc failure within 
    ** PagerSetPagesize() is tested.
    */
    rc = sqlite3PagerSetPagesize(pPager, &iPageSize, -1);
    testcase( rc!=SQLITE_OK );

    /* Update the assumed sector-size to match the value used by 
    ** the process that created this journal. If this journal was
    ** created by a process other than this one, then this routine
    ** is being called from within pager_playback(). The local value
    ** of Pager.sectorSize is restored at the end of that routine.
    */
    pPager->sectorSize = iSectorSize;
  }

  pPager->journalOff += JOURNAL_HDR_SZ(pPager);
  return rc;
}


/*
** Write the supplied master journal name into the journal file for pager
** pPager at the current location. The master journal name must be the last
** thing written to a journal file. If the pager is in full-sync mode, the
** journal file descriptor is advanced to the next sector boundary before
** anything is written. The format is:
**
**   + 4 bytes: PAGER_MJ_PGNO.
**   + N bytes: Master journal filename in utf-8.
**   + 4 bytes: N (length of master journal name in bytes, no nul-terminator).
**   + 4 bytes: Master journal name checksum.
**   + 8 bytes: aJournalMagic[].
**
** The master journal page checksum is the sum of the bytes in the master
** journal name, where each byte is interpreted as a signed 8-bit integer.
**
** If zMaster is a NULL pointer (occurs for a single database transaction), 
** this call is a no-op.
*/
static int writeMasterJournal(Pager *pPager, const char *zMaster){
  int rc;                          /* Return code */
  int nMaster;                     /* Length of string zMaster */
  i64 iHdrOff;                     /* Offset of header in journal file */
  i64 jrnlSize;                    /* Size of journal file on disk */
  u32 cksum = 0;                   /* Checksum of string zMaster */

  assert( pPager->setMaster==0 );
  assert( !pagerUseWal(pPager) );

  if( !zMaster 
   || pPager->journalMode==PAGER_JOURNALMODE_MEMORY 
   || pPager->journalMode==PAGER_JOURNALMODE_OFF 
  ){
    return SQLITE_OK;
  }
  pPager->setMaster = 1;
  assert( isOpen(pPager->jfd) );
  assert( pPager->journalHdr <= pPager->journalOff );

  /* Calculate the length in bytes and the checksum of zMaster */
  for(nMaster=0; zMaster[nMaster]; nMaster++){
    cksum += zMaster[nMaster];
  }

  /* If in full-sync mode, advance to the next disk sector before writing
  ** the master journal name. This is in case the previous page written to
  ** the journal has already been synced.
  */
  if( pPager->fullSync ){
    pPager->journalOff = journalHdrOffset(pPager);
  }
  iHdrOff = pPager->journalOff;

  /* Write the master journal data to the end of the journal file. If
  ** an error occurs, return the error code to the caller.
  */
  if( (0 != (rc = write32bits(pPager->jfd, iHdrOff, PAGER_MJ_PGNO(pPager))))
   || (0 != (rc = sqlite3OsWrite(pPager->jfd, zMaster, nMaster, iHdrOff+4)))
   || (0 != (rc = write32bits(pPager->jfd, iHdrOff+4+nMaster, nMaster)))
   || (0 != (rc = write32bits(pPager->jfd, iHdrOff+4+nMaster+4, cksum)))
   || (0 != (rc = sqlite3OsWrite(pPager->jfd, aJournalMagic, 8, iHdrOff+4+nMaster+8)))
  ){
    return rc;
  }
  pPager->journalOff += (nMaster+20);

  /* If the pager is in peristent-journal mode, then the physical 
  ** journal-file may extend past the end of the master-journal name
  ** and 8 bytes of magic data just written to the file. This is 
  ** dangerous because the code to rollback a hot-journal file
  ** will not be able to find the master-journal name to determine 
  ** whether or not the journal is hot. 
  **
  ** Easiest thing to do in this scenario is to truncate the journal 
  ** file to the required size.
  */ 
  if( SQLITE_OK==(rc = sqlite3OsFileSize(pPager->jfd, &jrnlSize))
   && jrnlSize>pPager->journalOff
  ){
    rc = sqlite3OsTruncate(pPager->jfd, pPager->journalOff);
  }
  return rc;
}

/*
** Find a page in the hash table given its page number. Return
** a pointer to the page or NULL if the requested page is not 
** already in memory.如果请求页面没有在内存中返回一个指针或者空，找出在哈希表给出的页码输的页面。
*/
static PgHdr *pager_lookup(Pager *pPager, Pgno pgno){
  PgHdr *p;                         /* Return value */

  /* It is not possible for a call to PcacheFetch() with createFlag==0 to
  ** fail, since no attempt to allocate dynamic memory will be made.
  */
  (void)sqlite3PcacheFetch(pPager->pPCache, pgno, 0, &p);
  return p;
}

/*
** Discard the entire contents of the in-memory page-cache.
*/
static void pager_reset(Pager *pPager){
  sqlite3BackupRestart(pPager->pBackup);
  sqlite3PcacheClear(pPager->pPCache);
}

/*
** Free all structures in the Pager.aSavepoint[] array and set both
** Pager.aSavepoint and Pager.nSavepoint to zero. Close the sub-journal
** if it is open and the pager is not in exclusive mode.
*/
static void releaseAllSavepoints(Pager *pPager){
  int ii;               /* Iterator for looping through Pager.aSavepoint */
  for(ii=0; ii<pPager->nSavepoint; ii++){
    sqlite3BitvecDestroy(pPager->aSavepoint[ii].pInSavepoint);
  }
  if( !pPager->exclusiveMode || sqlite3IsMemJournal(pPager->sjfd) ){
    sqlite3OsClose(pPager->sjfd);
  }
  sqlite3_free(pPager->aSavepoint);
  pPager->aSavepoint = 0;
  pPager->nSavepoint = 0;
  pPager->nSubRec = 0;
}

/*
** Set the bit number pgno in the PagerSavepoint.pInSavepoint 
** bitvecs of all open savepoints. Return SQLITE_OK if successful
** or SQLITE_NOMEM if a malloc failure occurs.
*/
static int addToSavepointBitvecs(Pager *pPager, Pgno pgno){
  int ii;                   /* Loop counter */
  int rc = SQLITE_OK;       /* Result code */

  for(ii=0; ii<pPager->nSavepoint; ii++){
    PagerSavepoint *p = &pPager->aSavepoint[ii];
    if( pgno<=p->nOrig ){
      rc |= sqlite3BitvecSet(p->pInSavepoint, pgno);
      testcase( rc==SQLITE_NOMEM );
      assert( rc==SQLITE_OK || rc==SQLITE_NOMEM );
    }
  }
  return rc;
}

/*
** This function is a no-op if the pager is in exclusive mode and not
** in the ERROR state. Otherwise, it switches the pager to PAGER_OPEN
** state.如果pager是在独占模式而且不是在异常状态，这个函数是个空操作。否则的话，它将使pager转换到打开状态。
**
** If the pager is not in exclusive-access mode, the database file is completely unlocked. 
**If the file is unlocked and the file-system does
** not exhibit the UNDELETABLE_WHEN_OPEN property, the journal file is
** closed (if it is open).如果pager不是在独占访问，数据库文件将会完全解锁。如果文件是开放的，文件系统不会在打开时不能删除的特点，这个日志文件将会被关闭。
**
** If the pager is in ERROR state when this function is called, the 
** contents of the pager cache are discarded before switching back to 
** the OPEN state.若这个函数被调用时pager是在异常状态，它缓存中的文件在转回打开状态之前将会被丢弃。
 Regardless of whether the pager is in exclusive-mode
** or not, any journal file left in the file-system will be treated
** as a hot-journal and rolled back the next time a read-transaction
** is opened (by this or by any other connection).不管pager是否处于独占模式
与否，留在该文件系统的任何日志文件将被处理
作为热日志，下一次回滚一个读事务
 被打开（通过这个或通过任何其它连接）。
*/
static void pager_unlock(Pager *pPager){

  assert( pPager->eState==PAGER_READER 
       || pPager->eState==PAGER_OPEN 
       || pPager->eState==PAGER_ERROR 
  ); /* assert() 是宏，而不是函数，assert(int expression)的作用是判断expression的值是否为假 若为假，
  则他先向标准错误流stderr打印一条出错信息，然后通过调用abort来终止程序运行，否则无任何作用。其作用就是用于确认程序的正常操作，
  即若pPager是在读，打开，异常的状态下就执行下面的代码*/

  sqlite3BitvecDestroy(pPager->pInJournal);/* 摧毁这个位图，回收所有的内存使用*/
  pPager->pInJournal = 0;
  releaseAllSavepoints(pPager);/*释放Pager.aSavepoint[]数组中所有的结构，
把Pager.aSavepoint and Pager.nSavepoint都设置为0,若pager不是在独占模式而且是打开的，则要关闭这个日志。*/

  if( pagerUseWal(pPager) ){     
    assert( !isOpen(pPager->jfd) );
    sqlite3WalEndReadTransaction(pPager->pWal);
    pPager->eState = PAGER_OPEN;/*如果pPager用的是写日志，pPager的主要指日文件没有打开。则完成一个读操作，释放读锁，pPager设置为打开状态*/
  }else if( !pPager->exclusiveMode ){           /*如果pPager用的不是写日志，而且不是在独占模式，则执行*/
                  
    int rc;                       /* Error code returned by pagerUnlockDb() */
    int iDc = isOpen(pPager->fd)?sqlite3OsDeviceCharacteristics(pPager->fd):0;/*若pPager数据库文件打开了，则返回打开文件的位图给变量iDc，否则返回0给变量iDc*/

    /* If the operating system support deletion of open files, then
    ** close the journal file when dropping the database lock.  Otherwise
    ** another connection with journal_mode=delete might delete the file
    ** out from under us.  如果操作系统支持打开文件的删除，当删除数据库锁时将关闭日志文件。否则另一个journal_mode=delete的连接将会从我们下面删除文件。
    */
    assert( (PAGER_JOURNALMODE_MEMORY   & 5)!=1 );/*若PAGER_JOURNALMODE_MEMORY与5进行&计算的值不为1，则程序继续执行，否则停止*/
    assert( (PAGER_JOURNALMODE_OFF      & 5)!=1 );/*若PAGER_JOURNALMODE_OFF与5进行&计算的值不为1，则程序继续执行，否则停止*/
    assert( (PAGER_JOURNALMODE_WAL      & 5)!=1 );/*若PAGER_JOURNALMODE_WAL进行&计算的值不为1，则程序继续执行，否则停止*/
    assert( (PAGER_JOURNALMODE_DELETE   & 5)!=1 );/*若PAGER_JOURNALMODE_DELETE进行&计算的值为1，则程序继续执行，否则停止*/
    assert( (PAGER_JOURNALMODE_TRUNCATE & 5)==1 );/*若PAGER_JOURNALMODE_TRUNCATE 进行&计算的值为1，则程序继续执行，否则停止*/
    assert( (PAGER_JOURNALMODE_PERSIST  & 5)==1 );/*若PAGER_JOURNALMODE_PERSIST 进行&计算的值为1，则程序继续执行，否则停止*/
    if( 0==(iDc & SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN)/*若SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN与iDc进行&运算结果为0或者pPager->journalMode & 5的值不为1
                                                则关闭打开的主要日志文件*/
     || 1!=(pPager->journalMode & 5)
    ){
      sqlite3OsClose(pPager->jfd);
    }

    /* If the pager is in the ERROR state and the call to unlock the database
    ** file fails, set the current lock to UNKNOWN_LOCK. See the comment
    ** above the #define for UNKNOWN_LOCK for an explanation of why this
    ** is necessary.如果寻呼机是在异常状态 并且调用打开的的数据库文件失败，则要设置当前锁为UNKNOWN_LOCK。
    看前面关于UNKNOWN_LOCK的预定义可以知道为什么这是必要的。
    */
    rc = pagerUnlockDb(pPager, NO_LOCK);/*打开数据库文件，把值赋给rc,若数据库文件打开失败而且pPager状态异常，设置当前锁为UNKNOWN_LOCK*/
    if( rc!=SQLITE_OK && pPager->eState==PAGER_ERROR ){
      pPager->eLock = UNKNOWN_LOCK;
    }

    /* The pager state may be changed from PAGER_ERROR to PAGER_OPEN here
    ** without clearing the error code. This is intentional - the error
    ** code is cleared and the cache reset in the block below.
    */      /*pager状态可能从异常转变到打开状态没有清除错误的代码，异常代码的清除和缓存在下面块的重置是人为的。*/
     assert( pPager->errCode || pPager->eState!=PAGER_ERROR );
    pPager->changeCountDone = 0;
    pPager->eState = PAGER_OPEN;//有错误的代码或者是pPager是在异常状态，则不做什么改变，把pPager设置为打开状态
  }

  /* If Pager.errCode is set, the contents of the pager cache cannot be
  ** trusted. Now that there are no outstanding references to the pager,
  ** it can safely move back to PAGER_OPEN state. This happens in both
  ** normal and exclusive-locking mode.
  */  
  //如果Pager出现了错误，Pager的缓存内容则不被信任。现在没有引用pager，他可以安全的返回到打开状态，这种情况发生在正常和独立所模式下。
  if( pPager->errCode ){//若存在错误
    assert( !MEMDB );//若没有禁止所有的I/O文件
    pager_reset(pPager);//丢弃整个缓存页面的要求
    pPager->changeCountDone = pPager->tempFile;//看临时文件是否被更新
    pPager->eState = PAGER_OPEN;//把pPager设置为打开状态
    pPager->errCode = SQLITE_OK;//pPager状态设置为正常状态。
  }

  pPager->journalOff = 0;
  pPager->journalHdr = 0;
  pPager->setMaster = 0;
}

/*
** This function is called whenever an IOERR or FULL error that requires
** the pager to transition into the ERROR state may ahve occurred.
** The first argument is a pointer to the pager structure, the second 
** the error-code about to be returned by a pager API function. The 
** value returned is a copy of the second argument to this function. 
** If the second argument is SQLITE_FULL, SQLITE_IOERR or one of the
** IOERR sub-codes, the pager enters the ERROR state and the error code
** is stored in Pager.errCode. While the pager remains in the ERROR state,
** all major API calls on the Pager will immediately return Pager.errCode.
**The ERROR state indicates that the contents of the pager-cache 
** cannot be trusted. This state can be cleared by completely discarding 
** the contents of the pager-cache. If a transaction was active when
** the persistent error occurred, then the rollback journal may need
** to be replayed to restore the contents of the database file (as if
** it were a hot-journal).每当IOERR或FULL错误，需要pager调用此函数过渡到异常状态，其中可能会发生ahve。
 第一个参数是一个指向pager的指针，第二错误代码即将被pager的API函数返回。这个函数返回的值是第二个参数的副本。
 如果第二个参数是SQLITE_FULL，SQLITE_IOERR或之一IOERR子代码，pager进入ERROR状态和错误代码
 存储在Pager.errCode。而pager仍处于错误状态，在pager所有主要的API调用将立即返回Pager.errCode。
 异常状态下，pager-cache的内容不能被信任。这种状态可以完全丢弃pager-cache的内容。
若如果一个事务积极持久的错误发生时，然后回滚日志可能需要被重播恢复数据库文件的内容。
*/
static int pager_error(Pager *pPager, int rc){
  int rc2 = rc & 0xff;
  assert( rc==SQLITE_OK || !MEMDB );
  assert(
       pPager->errCode==SQLITE_FULL ||
       pPager->errCode==SQLITE_OK ||
       (pPager->errCode & 0xff)==SQLITE_IOERR
  );
  if( rc2==SQLITE_FULL || rc2==SQLITE_IOERR ){
    pPager->errCode = rc;
    pPager->eState = PAGER_ERROR;
  }
  return rc;
}

/*
** This routine ends a transaction. A transaction is usually ended by 
** either a COMMIT or a ROLLBACK operation. This routine may be called 
** after rollback of a hot-journal, or if an error occurs while opening
** the journal file or writing the very first journal-header of a
** database transaction.这个程序结束一个事务。这个事务通常是被COMMIT或者ROLLBACK操作结束。
** 这个程序可能在回滚一个hot-journal之后，或者如果当打开日志文件或者写一个数据库事务的journal-header时被调用。
** This routine is never called in PAGER_ERROR state. If it is called
** in PAGER_NONE or PAGER_SHARED state and the lock held is less
** exclusive than a RESERVED lock, it is a no-op.
** Otherwise, any active savepoints are released.
这段程序从不在异常状态下被调用。如果它在PAGER_NONE 或 PAGER_SHARED状态被调用，
而且数据库锁是RESERVED lock而不是排他锁，那么他是一个空程序。否则，任何活动的保存点被释放。
** If the journal file is open, then it is "finalized". Once a journal 
** file has been finalized it is not possible to use it to roll back a 
** transaction. Nor will it be considered to be a hot-journal by this
** or any other database connection. Exactly how a journal is finalized
** depends on whether or not the pager is running in exclusive mode and
** the current journal-mode (Pager.journalMode value), as follows:
**如果日志文件时空的，它就会结束。一旦一个日志文件已经结束，那么再用它去回滚一个事务是不可能的。
既不会认为是一个hot-journal也不是是任何的数据库连接。准确的说一个日志是怎样结束的依赖于是不是
pager运行在独占模式和当前的Pager.journalMode的值。比如下面的

**   journalMode==MEMORY
**     Journal file descriptor is simply closed. This destroys an 
**     in-memory journal.//日志文件标识符是关闭的，这毁坏了内存中的日志。
**
**   journalMode==TRUNCATE
**     Journal file is truncated to zero bytes in size.//日志文件被截成0字节。
**
**   journalMode==PERSIST
**     The first 28 bytes of the journal file are zeroed. This invalidates
**     the first journal header in the file, and hence the entire journal
**     file. An invalid journal file cannot be rolled back.//日志文件的前28位置零。这使得文件中的日志前面部分没有意义。
**     因此整个日志文件无效，一个无效的日志文件不能被回滚。
**   journalMode==DELETE
**     The journal file is closed and deleted using sqlite3OsDelete().
**
**     If the pager is running in exclusive mode, this method of finalizing
**     the journal file is never used. Instead, if the journalMode is
**     DELETE and the pager is in exclusive mode, the method described under
**     journalMode==PERSIST is used instead.//这个日志文件被关闭，并且被sqlite3OsDelete()函数删除。
    如果pager运行在独占模式下，日志文件的结束方法将从不被用。如果journalMode为DELETE而且pager是在独占模式下。
    journalMode==PERSIST 下的方法会被替代。
**
** After the journal is finalized, the pager moves to PAGER_READER state.
** If running in non-exclusive rollback mode, the lock on the file is 
** downgraded to a SHARED_LOCK.日志被关闭之后，pager转变到PAGER_READER状态。
如果是运行在非独占回滚模式，文件的锁会变为SHARED_LOCK锁。
**
** SQLITE_OK is returned if no error occurs. If an error occurs during
** any of the IO operations to finalize the journal file or unlock the
** database then the IO error code is returned to the user. If the 
** operation to finalize the journal file fails, then the code still
** tries to unlock the database file if not in exclusive mode. If the
** unlock operation fails as well, then the first error code related
** to the first error encountered (the journal finalization one) is
** returned.如果没有错误发生，将返回SQLITE_OK，如果任何的IO操作去结束日志文件或者解锁数据库过程中发生错误，
然后IO错误代码返回给用户。如果结束日志文件的操作失败，那么代码仍然试图打开数据库文件如果不是在独占模式。
如果打开操作也失败，那么跟第一个错误有关的第一个错误代码将会被返回。
*/
static int pager_end_transaction(Pager *pPager, int hasMaster){
  int rc = SQLITE_OK;      /* Error code from journal finalization operation */ //结束日志操作的错误代码
  int rc2 = SQLITE_OK;     /* Error code from db file unlock operation */ //数据库文件解锁操作的错误代码

  /* Do nothing if the pager does not have an open write transaction
  ** or at least a RESERVED lock. This function may be called when there
  ** is no write-transaction active but a RESERVED or greater lock is
  ** held under two circumstances:
  **如果pager没有打开的写事务或者至少一个RESERVED锁，什么都不做。
  当没有写操作除了在这两种情况下RESERVED或更大的锁被执行，这个函数可能被调用。
  **   1. After a successful hot-journal rollback, it is called with
  **      eState==PAGER_NONE and eLock==EXCLUSIVE_LOCK.
  **在hot-journal成功回滚后，在eState==PAGER_NONE and eLock==EXCLUSIVE_LOCK时被调用。
  **   2. If a connection with locking_mode=exclusive holding an EXCLUSIVE 
  **      lock switches back to locking_mode=normal and then executes a
  **      read-transaction, this function is called with eState==PAGER_READER 
  **      and eLock==EXCLUSIVE_LOCK when the read-transaction is closed.
  如果一个locking_mode=exclusive，持有独占锁的连接转换成locking_mode=normal，而且执行一个读操作，
  那么读事务关闭时eState==PAGER_READER and eLock==EXCLUSIVE_LOCK 这个函数被调用。
  */
  assert( assert_pager_state(pPager) );//判断是否有错误发生，没有正常执行，否则停止。
  assert( pPager->eState!=PAGER_ERROR );//pPager状态没有出现异常时程序正常执行，否则停止。
  if( pPager->eState<PAGER_WRITER_LOCKED && pPager->eLock<RESERVED_LOCK ){
    return SQLITE_OK;
  }//如果pPager状态是在PAGER_OPEN或者是PAGER_READER状态，并且pPager->eLock为NO_LOCK 或SHARED_LOCK时，返回SQLITE_OK。

  releaseAllSavepoints(pPager);/*释放Pager.aSavepoint[]数组中所有的结构，
把Pager.aSavepoint and Pager.nSavepoint都设置为0,若pager不是在独占模式而且是打开的，则要关闭这个日志。*/
  assert( isOpen(pPager->jfd) || pPager->pInJournal==0 );//若打开了主日志文件或者数据库文件的每一页中的一位为0
  if( isOpen(pPager->jfd) ){
    assert( !pagerUseWal(pPager) );//如果pagerUseWal(pPager)返回的值不为0，程序正常执行，否则停止。

    /* Finalize the journal file. */  //结束日志文件
    if( sqlite3IsMemJournal(pPager->jfd) ){
      assert( pPager->journalMode==PAGER_JOURNALMODE_MEMORY );
      sqlite3OsClose(pPager->jfd);       //若主日志文件是in-memory 日志，pPager->journalMode==PAGER_JOURNALMODE_MEMORY ，则关闭主日志文件。
    }else if( pPager->journalMode==PAGER_JOURNALMODE_TRUNCATE ){
      if( pPager->journalOff==0 ){
        rc = SQLITE_OK;
      }else{
        rc = sqlite3OsTruncate(pPager->jfd, 0);
      }
      pPager->journalOff = 0;
    }else if( pPager->journalMode==PAGER_JOURNALMODE_PERSIST
      || (pPager->exclusiveMode && pPager->journalMode!=PAGER_JOURNALMODE_WAL)
    ){
      rc = zeroJournalHdr(pPager, hasMaster);
      pPager->journalOff = 0;
    }else{
      /* This branch may be executed with Pager.journalMode==MEMORY if
      ** a hot-journal was just rolled back. In this case the journal
      ** file should be closed and deleted. If this connection writes to
      ** the database file, it will do so using an in-memory journal. 
      */
      //这个分支可能执行。如果一个热日志回滚journalMode==MEMORY。这种情况下日志文件应该被关闭或者删除。
      //如果这个连接写的是数据库文件，它将被写进内存中的日志。
      assert( pPager->journalMode==PAGER_JOURNALMODE_DELETE 
           || pPager->journalMode==PAGER_JOURNALMODE_MEMORY 
           || pPager->journalMode==PAGER_JOURNALMODE_WAL 
      );
      sqlite3OsClose(pPager->jfd);//关闭主日志文件
      if( !pPager->tempFile ){  //如果不是一个临时文件
        rc = sqlite3OsDelete(pPager->pVfs, pPager->zJournal, 0);//删除日志文件。
      }
    }
  }

#ifdef SQLITE_CHECK_PAGES
  sqlite3PcacheIterateDirty(pPager->pPCache, pager_set_pagehash);
  if( pPager->dbSize==0 && sqlite3PcacheRefCount(pPager->pPCache)>0 ){
    PgHdr *p = pager_lookup(pPager, 1);
    if( p ){
      p->pageHash = 0;
      sqlite3PagerUnref(p);
    }
  } //如果定义SQLITE_CHECK_PAGES，执行sqlite3PcacheIterateDirty（）函数，如果数据库页面的数目为零而且返回的页面缓存数目大于0，
//找出在哈希表给出的页码输的页面，如果请求页面没有在内存中返回一个指针或者空给p，若p不为0，令p->pageHash = 0，释放对这个页面的引用。

#endif

  sqlite3BitvecDestroy(pPager->pInJournal);
  pPager->pInJournal = 0;//数据库文件中的每一页中的一位为0
  pPager->nRec = 0;
  sqlite3PcacheCleanAll(pPager->pPCache);
  sqlite3PcacheTruncate(pPager->pPCache, pPager->dbSize);

  if( pagerUseWal(pPager) ){
    /* Drop the WAL write-lock, if any. Also, if the connection was in 
    ** locking_mode=exclusive mode but is no longer, drop the EXCLUSIVE 
    ** lock held on the database file.
    */
    //终止WAL写锁，如果连接是在独占模式下，在数据库文件中不再终止EXCLUSIVE锁
    rc2 = sqlite3WalEndWriteTransaction(pPager->pWal);
    assert( rc2==SQLITE_OK );
  }
  if( !pPager->exclusiveMode 
   && (!pagerUseWal(pPager) || sqlite3WalExclusiveMode(pPager->pWal, 0))
  ){
    rc2 = pagerUnlockDb(pPager, SHARED_LOCK);
    pPager->changeCountDone = 0;
  }
  pPager->eState = PAGER_READER;
  pPager->setMaster = 0;

  return (rc==SQLITE_OK?rc2:rc);
}

/*
** Execute a rollback if a transaction is active and unlock the 
** database file. //如果一个事务是活跃的而且数据库文件是打开的，执行一个回滚。
**
** If the pager has already entered the ERROR state, do not attempt 
** the rollback at this time. Instead, pager_unlock() is called. The
** call to pager_unlock() will discard all in-memory pages, unlock
** the database file and move the pager back to OPEN state. If this 
** means that there is a hot-journal left in the file-system, the next 
** connection to obtain a shared lock on the pager (which may be this one) 
** will roll it back.
**如果pager已经是异常状态，不要在这个时候企图回滚，而是调用pager_unlock()。调用pager_unlock()将会丢弃所有在内存中的页面，
解锁数据库文件，pager转换回打开状态。若果这意味着有一个hot-journal被留在文件系统，下一个获得一个共享锁的链接将会回滚。
** If the pager has not already entered the ERROR state, but an IO or
** malloc error occurs during a rollback, then this will itself cause 
** the pager to enter the ERROR state. Which will be cleared by the
** call to pager_unlock(), as described above.
*/
//如果pager还没有进入异常状态，但是在回滚时发生malloc错误或IO错误，它自己就会进入异常状态，像上面描述的一样，在调用pager_unlock()
//它将会被清除。
static void pagerUnlockAndRollback(Pager *pPager){
  if( pPager->eState!=PAGER_ERROR && pPager->eState!=PAGER_OPEN ){
    assert( assert_pager_state(pPager) );
    if( pPager->eState>=PAGER_WRITER_LOCKED ){
      sqlite3BeginBenignMalloc();
      sqlite3PagerRollback(pPager);
      sqlite3EndBenignMalloc();
    }else if( !pPager->exclusiveMode ){
      assert( pPager->eState==PAGER_READER );
      pager_end_transaction(pPager, 0);
    }
  }
  pager_unlock(pPager);
}

/*
** Parameter aData must point to a buffer of pPager->pageSize bytes
** of data. Compute and return a checksum based ont the contents of the 
** page of data and the current value of pPager->cksumInit.
**参数aData必须指向页面中的字节数数据的缓冲区，计算并返回一个基于页面数据内容和Pager->cksumInit当前值的校验和。
** This is not a real checksum. It is really just the sum of the 
** random initial value (pPager->cksumInit) and every 200th byte
** of the page data, starting with byte offset (pPager->pageSize%200).
** Each byte is interpreted as an 8-bit unsigned integer.
**这不是真的校验和，它只是pPager->cksumInit随机初始值的和，以pPager->pageSize%200的之开始，每个字节都是8位的无符号整形。
** Changing the formula used to compute this checksum results in an
** incompatible journal file format.
**在一个不兼容的日志文件格式改变通常用于计算校验和结果的公式。
** If journal corruption occurs due to a power failure, the most likely 
** scenario is that one end or the other of the record will be changed. 
** It is much less likely that the two ends of the journal record will be
** correct and the middle be corrupt.  Thus, this "checksum" scheme,
** though fast and simple, catches the mostly likely kind of corruption.
*/  //如果由于电源故障日志发生错误，一种最可能的情况就是一端或一个记录将会被改变。
//日志两端的记录正确，中间部分将会发生错误是不太可能的。这样，这个校验和机制，虽然快而且简单，但会捕获最有可能的错误。
static u32 pager_cksum(Pager *pPager, const u8 *aData){
  u32 cksum = pPager->cksumInit;         /* Checksum value to return */ //返回校验和
  int i = pPager->pageSize-200;          /* Loop counter */ //循环计数器
  while( i>0 ){
    cksum += aData[i];
    i -= 200;
  }
  return cksum;
}

/*
** Report the current page size and number of reserved bytes back
** to the codec.
*/ //报告当前页面大小和保留字节的数量给编解码器。，
#ifdef SQLITE_HAS_CODEC
static void pagerReportSize(Pager *pPager){
  if( pPager->xCodecSizeChng ){
    pPager->xCodecSizeChng(pPager->pCodec, pPager->pageSize,
                           (int)pPager->nReserve);
  }
}
#else
# define pagerReportSize(X)     /* No-op if we do not support a codec */  //如果我们不支持编解码器，这将是个空操作。
#endif

/*
** Read a single page from either the journal file (if isMainJrnl==1) or
** from the sub-journal (if isMainJrnl==0) and playback that page.
** The page begins at offset *pOffset into the file. The *pOffset
** value is increased to the start of the next page in the journal.
**从主日志文件中或者从子日志文件中读一个页面，并且回放这个页面。
**页面开始抵消*pOffset到文件。*pOffset的值增加到下一个页面的开始。
** The main rollback journal uses checksums - the statement journal does 
** not.
**主要的回滚日志用校验和，日志的声明不用。
** If the page number of the page record read from the (sub-)journal file
** is greater than the current value of Pager.dbSize, then playback is
** skipped and SQLITE_OK is returned.
**如果从（子）的日志文件读出的页的记录的页面数大于Pager.dbSize的当前值，然后跳过回放，返回SQLITE_OK。
** If pDone is not NULL, then it is a record of pages that have already
** been played back.  If the page at *pOffset has already been played back
** (if the corresponding pDone bit is set) then skip the playback.
** Make sure the pDone bit corresponding to the *pOffset page is set
** prior to returning.
**若pDone不为空，页面的记录已经被回放。若*pOffset页面已经被回放，则跳过回放。确保pDone位对应的*pOffset页面设置优先级返回。
** If the page record is successfully read from the (sub-)journal file
** and played back, then SQLITE_OK is returned. If an IO error occurs
** while reading the record from the (sub-)journal file or while writing
** to the database file, then the IO error code is returned. If data
** is successfully read from the (sub-)journal file but appears to be
** corrupted, SQLITE_DONE is returned. Data is considered corrupted in
** two circumstances:
** 若页面记录从（子）日志文件中成功的被读或者回放，则将返回SQLITE_OK。
**如果当从（子）日志文件中读或写进数据库文件时发生IO错误，则错误的代码将会返回。
**如果从（子）日志文件中的数据成功的读但是出现了破坏，将返回SQLITE_DONE。数据被破坏在两种情况下：
**   * If the record page-number is illegal (0 or PAGER_MJ_PGNO), or
**   * If the record is being rolled back from the main journal file
**     and the checksum field does not match the record content.
**如果这个页面记录是不合法的，或者这个记录从主日志文件回滚，校验和文件没有匹配日志内容。
** Neither of these two scenarios are possible during a savepoint rollback.
**在保存点回滚，这两种情况都是不可能的。
** If this is a savepoint rollback, then memory may have to be dynamically
** allocated by this function. If this is the case and an allocation fails,
** SQLITE_NOMEM is returned.
*/ //如果这是保存点回滚，内存则会不得不被这个函数动态的分配。如果遇到这种情况而且分配失败，将会返回SQLITE_NOMEM。
static int pager_playback_one_page( 
  Pager *pPager,                /* The pager being played back */  //pager的回放
  i64 *pOffset,                 /* Offset of record to playback */  //抵消回放的记录
  Bitvec *pDone,                /* Bitvec of pages already played back */ //Bitvec页面已经回放
  int isMainJrnl,               /* 1 -> main journal. 0 -> sub-journal. */
  int isSavepnt                 /* True for a savepoint rollback */ //保存点回滚是为true
){
  int rc;
  PgHdr *pPg;                   /* An existing page in the cache */
  Pgno pgno;                    /* The page number of a page in journal */
  u32 cksum;                    /* Checksum used for sanity checking */
  char *aData;                  /* Temporary storage for the page */
  sqlite3_file *jfd;            /* The file descriptor for the journal file */
  int isSynced;                 /* True if journal page is synced */ //日志页面同步时为true

  assert( (isMainJrnl&~1)==0 );      /* isMainJrnl is 0 or 1 */
  assert( (isSavepnt&~1)==0 );       /* isSavepnt is 0 or 1 */
  assert( isMainJrnl || pDone );     /* pDone always used on sub-journals */
  assert( isSavepnt || pDone==0 );   /* pDone never used on non-savepoint */

  aData = pPager->pTmpSpace;
  assert( aData );         /* Temp storage must have already been allocated */ //临时存储必须已经被分配，否则程序不执行。
  assert( pagerUseWal(pPager)==0 || (!isMainJrnl && isSavepnt) );

  /* Either the state is greater than PAGER_WRITER_CACHEMOD (a transaction 
  ** or savepoint rollback done at the request of the caller) or this is
  ** a hot-journal rollback. If it is a hot-journal rollback, the pager
  ** is in state OPEN and holds an EXCLUSIVE lock. Hot-journal rollback
  ** only reads from the main journal, not the sub-journal.
  */ //要么大于PAGER_WRITER_CACHEMOD状态（事务或保存点回滚完成请求的调用者）要么这是热日志的回滚。如果hot-journal回滚
  //pager是在打开状态，并拥有一个独占锁。热日志回滚仅仅在从主日志中读的时候，子日志中不会。
  assert( pPager->eState>=PAGER_WRITER_CACHEMOD
       || (pPager->eState==PAGER_OPEN && pPager->eLock==EXCLUSIVE_LOCK)
  );
  assert( pPager->eState>=PAGER_WRITER_CACHEMOD || isMainJrnl );

  /* Read the page number and page data from the journal or sub-journal
  ** file. Return an error code to the caller if an IO error occurs.
  */ //从日志或子日志文件读页码和页面的数据。若有IO错误发生，返回错误代码给调用者。
  jfd = isMainJrnl ? pPager->jfd : pPager->sjfd;
  rc = read32bits(jfd, *pOffset, &pgno);
  if( rc!=SQLITE_OK ) return rc;
  rc = sqlite3OsRead(jfd, (u8*)aData, pPager->pageSize, (*pOffset)+4);
  if( rc!=SQLITE_OK ) return rc;
  *pOffset += pPager->pageSize + 4 + isMainJrnl*4;

  /* Sanity checking on the page.  This is more important that I originally
  ** thought.  If a power failure occurs while the journal is being written,
  ** it could cause invalid data to be written into the journal.  We need to
  ** detect this invalid data (with high probability) and ignore it.
  */ //页面上的完备性检查。我原本想这是更重要的。若当文件被写时，发生电源故障时，他将会造成无效的数据写进日志。
  //我们需要发现无效的数据并忽视它。
  if( pgno==0 || pgno==PAGER_MJ_PGNO(pPager) ){
    assert( !isSavepnt );
    return SQLITE_DONE;
  }
  if( pgno>(Pgno)pPager->dbSize || sqlite3BitvecTest(pDone, pgno) ){
    return SQLITE_OK;
  }
  if( isMainJrnl ){
    rc = read32bits(jfd, (*pOffset)-4, &cksum);
    if( rc ) return rc;
    if( !isSavepnt && pager_cksum(pPager, (u8*)aData)!=cksum ){
      return SQLITE_DONE;
    }
  }

  /* If this page has already been played by before during the current
  ** rollback, then don't bother to play it back again.
  */ //如果这个页面已经回放被当前回滚，不要再去回放。
  if( pDone && (rc = sqlite3BitvecSet(pDone, pgno))!=SQLITE_OK ){
    return rc;
  }

  /* When playing back page 1, restore the nReserve setting
  */  //当回放页面1，恢复nReserve设置
  if( pgno==1 && pPager->nReserve!=((u8*)aData)[20] ){
    pPager->nReserve = ((u8*)aData)[20];
    pagerReportSize(pPager);
  }

  /* If the pager is in CACHEMOD state, then there must be a copy of this
  ** page in the pager cache. In this case just update the pager cache,
  ** not the database file. The page is left marked dirty in this case.
  **如果pager是在CACHEMOD状态，那么在pager缓存中必须有这个页面的副本。
  ** 在这种情况下仅仅更新pager缓存，而不是数据库文件。这种情况下剩下的页面记为脏数据。
  ** An exception to the above rule: If the database is in no-sync mode
  ** and a page is moved during an incremental vacuum then the page may
  ** not be in the pager cache. Later: if a malloc() or IO error occurs
  ** during a Movepage() call, then the page may not be in the cache
  ** either. So the condition described in the above paragraph is not
  ** assert()able.
  **一个例外对于上述规则，如果数据库是在没有同步的模式，页面被移动在一个在增值的真空，则页面将无法在pager缓存中。
  随后，若在调用Movepage()时，发生malloc()或IO错误，页面也将无法在pager缓存中。
  因此上个段落描述的条件不是assert()函数可以实现的。
  ** If in WRITER_DBMOD, WRITER_FINISHED or OPEN state, then we update the
  ** pager cache if it exists and the main file. The page is then marked 
  ** not dirty. Since this code is only executed in PAGER_OPEN state for
  ** a hot-journal rollback, it is guaranteed that the page-cache is empty
  ** if the pager is in OPEN state.
  **如果在WRITER_DBMOD, WRITER_FINISHED or OPEN状态，那我们要更新pager 缓存（若存在），和主文件。
  页面不标记为脏。由于hot-journal回滚 ，这个代码仅仅执行在PAGER_OPEN状态，如果pager是在打开状态，要保证页面缓存为空。
  ** Ticket #1171:  The statement journal might contain page content that is
  ** different from the page content at the start of the transaction.
  标签#1171：日志的状态可能包含页面内容，不同于事务开始时的页面内容。
  ** This occurs when a page is changed prior to the start of a statement
  ** then changed again within the statement.  When rolling back such a
  ** statement we must not write to the original database unless we know
  ** for certain that original page contents are synced into the main rollback
  ** journal. 这发生在一个页面改变开始之前，然后在声明中 再次改变。当这样的声明回滚，我们不必写进原始数据库，
  除非我们知道对于某些原始页面的内容同步到主回滚日志上。
  **Otherwise, a power loss might leave modified data in the
  ** database file without an entry in the rollback journal that can
  ** restore the database to its original form. 
  否则，功率损耗可能会把修改后的数据留在数据库文件没有一个条目在回滚日志上，将数据库恢复到原来的形式。
  Two conditions must be met before writing to the database files. (1) the database must be
  ** locked.  (2) we know that the original page content is fully synced
  ** in the main journal either because the page is not in cache or else
  ** the page is marked as needSync==0.
  **在写进数据库文件之前两种情况必须有，（1）数据库必须有锁（2）我们知道在主日志中原始页面内容完全被同步，
  因为页面没有在缓存中或者是页面被标记为needSync==0。
  ** 2008-04-14:  When attempting to vacuum a corrupt database file, it
  ** is possible to fail a statement on a database that does not yet exist.
  ** Do not attempt to write if database file has never been opened.
  *///2008-04-14： 当试图去消除一个腐败的数据库文件，失败的声明在一个上不存在的数据库中是可能的，
  // 如果数据库文件从没有被打开，不要试着去写。
  if( pagerUseWal(pPager) ){
    pPg = 0;
  }else{
    pPg = pager_lookup(pPager, pgno);
  }
  assert( pPg || !MEMDB );
  assert( pPager->eState!=PAGER_OPEN || pPg==0 );
  PAGERTRACE(("PLAYBACK %d page %d hash(%08x) %s\n",
           PAGERID(pPager), pgno, pager_datahash(pPager->pageSize, (u8*)aData),
           (isMainJrnl?"main-journal":"sub-journal")
  ));
  if( isMainJrnl ){
    isSynced = pPager->noSync || (*pOffset <= pPager->journalHdr);
  }else{
    isSynced = (pPg==0 || 0==(pPg->flags & PGHDR_NEED_SYNC));
  }
  if( isOpen(pPager->fd)
   && (pPager->eState>=PAGER_WRITER_DBMOD || pPager->eState==PAGER_OPEN)
   && isSynced
  ){
    i64 ofst = (pgno-1)*(i64)pPager->pageSize;
    testcase( !isSavepnt && pPg!=0 && (pPg->flags&PGHDR_NEED_SYNC)!=0 );
    assert( !pagerUseWal(pPager) );
    rc = sqlite3OsWrite(pPager->fd, (u8*)aData, pPager->pageSize, ofst);
    if( pgno>pPager->dbFileSize ){
      pPager->dbFileSize = pgno;
    }
    if( pPager->pBackup ){
      CODEC1(pPager, aData, pgno, 3, rc=SQLITE_NOMEM);
      sqlite3BackupUpdate(pPager->pBackup, pgno, (u8*)aData);
      CODEC2(pPager, aData, pgno, 7, rc=SQLITE_NOMEM, aData);
    }
  }else if( !isMainJrnl && pPg==0 ){
    /* If this is a rollback of a savepoint and data was not written to
    ** the database and the page is not in-memory, there is a potential
    ** problem. When the page is next fetched by the b-tree layer, it 
    ** will be read from the database file, which may or may not be 
    ** current.如果这是一个保存点回滚,数据不写入数据库和页面不是内存, 这会有一个潜在的问题。
    ** There are a couple of different ways this can happen. All are quite
    ** obscure. When running in synchronous mode, this can only happen 
    ** if the page is on the free-list at the start of the transaction, then
    ** populated, then moved using sqlite3PagerMovepage().
    **
    ** The solution is to add an in-memory page to the cache containing
    ** the data just read from the sub-journal. Mark the page as dirty 
    ** and if the pager requires a journal-sync, then mark the page as 
    ** requiring a journal-sync before it is written.
    */
    assert( isSavepnt );
    assert( pPager->doNotSpill==0 );
    pPager->doNotSpill++;
    rc = sqlite3PagerAcquire(pPager, pgno, &pPg, 1);
    assert( pPager->doNotSpill==1 );
    pPager->doNotSpill--;
    if( rc!=SQLITE_OK ) return rc;
    pPg->flags &= ~PGHDR_NEED_READ;
    sqlite3PcacheMakeDirty(pPg);
  }
  if( pPg ){
    /* No page should ever be explicitly rolled back that is in use, except
    ** for page 1 which is held in use in order to keep the lock on the
    ** database active. However such a page may be rolled back as a result
    ** of an internal error resulting in an automatic call to
    ** sqlite3PagerRollback().
    */
    void *pData;
    pData = pPg->pData;
    memcpy(pData, (u8*)aData, pPager->pageSize);
    pPager->xReiniter(pPg);
    if( isMainJrnl && (!isSavepnt || *pOffset<=pPager->journalHdr) ){
      /* If the contents of this page were just restored from the main 
      ** journal file, then its content must be as they were when the 
      ** transaction was first opened. In this case we can mark the page
      ** as clean, since there will be no need to write it out to the
      ** database.
      **
      ** There is one exception to this rule. If the page is being rolled
      ** back as part of a savepoint (or statement) rollback from an 
      ** unsynced portion of the main journal file, then it is not safe
      ** to mark the page as clean. This is because marking the page as
      ** clean will clear the PGHDR_NEED_SYNC flag. Since the page is
      ** already in the journal file (recorded in Pager.pInJournal) and
      ** the PGHDR_NEED_SYNC flag is cleared, if the page is written to
      ** again within this transaction, it will be marked as dirty but
      ** the PGHDR_NEED_SYNC flag will not be set. It could then potentially
      ** be written out into the database file before its journal file
      ** segment is synced. If a crash occurs during or following this,
      ** database corruption may ensue.
      */
      assert( !pagerUseWal(pPager) );
      sqlite3PcacheMakeClean(pPg);
    }
    pager_set_pagehash(pPg);

    /* If this was page 1, then restore the value of Pager.dbFileVers.
    ** Do this before any decoding. */
    if( pgno==1 ){
      memcpy(&pPager->dbFileVers, &((u8*)pData)[24],sizeof(pPager->dbFileVers));
    }

    /* Decode the page just read from disk */
    CODEC1(pPager, pData, pPg->pgno, 3, rc=SQLITE_NOMEM);
    sqlite3PcacheRelease(pPg);
  }
  return rc;
}

/*
** Parameter zMaster is the name of a master journal file. A single journal
** file that referred to the master journal file has just been rolled back.
** This routine checks if it is possible to delete the master journal file,
** and does so if it is.
**
** Argument zMaster may point to Pager.pTmpSpace. So that buffer is not 
** available for use within this function.
**
** When a master journal file is created, it is populated with the names 
** of all of its child journals, one after another, formatted as utf-8 
** encoded text. The end of each child journal file is marked with a 
** nul-terminator byte (0x00). i.e. the entire contents of a master journal
** file for a transaction involving two databases might be:
**
**   "/home/bill/a.db-journal\x00/home/bill/b.db-journal\x00"
**
** A master journal file may only be deleted once all of its child 
** journals have been rolled back.
**
** This function reads the contents of the master-journal file into 
** memory and loops through each of the child journal names. For
** each child journal, it checks if:
**
**   * if the child journal exists, and if so
**   * if the child journal contains a reference to master journal 
**     file zMaster
**
** If a child journal can be found that matches both of the criteria
** above, this function returns without doing anything. Otherwise, if
** no such child journal can be found, file zMaster is deleted from
** the file-system using sqlite3OsDelete().
**
** If an IO error within this function, an error code is returned. This
** function allocates memory by calling sqlite3Malloc(). If an allocation
** fails, SQLITE_NOMEM is returned. Otherwise, if no IO or malloc errors 
** occur, SQLITE_OK is returned.
**
** TODO: This function allocates a single block of memory to load
** the entire contents of the master journal file. This could be
** a couple of kilobytes or so - potentially larger than the page 
** size.
*/
static int pager_delmaster(Pager *pPager, const char *zMaster){
  sqlite3_vfs *pVfs = pPager->pVfs;
  int rc;                   /* Return code */
  sqlite3_file *pMaster;    /* Malloc'd master-journal file descriptor */
  sqlite3_file *pJournal;   /* Malloc'd child-journal file descriptor */
  char *zMasterJournal = 0; /* Contents of master journal file */
  i64 nMasterJournal;       /* Size of master journal file */
  char *zJournal;           /* Pointer to one journal within MJ file */
  char *zMasterPtr;         /* Space to hold MJ filename from a journal file */
  int nMasterPtr;           /* Amount of space allocated to zMasterPtr[] */

  /* Allocate space for both the pJournal and pMaster file descriptors.
  ** If successful, open the master journal file for reading.
  */
  pMaster = (sqlite3_file *)sqlite3MallocZero(pVfs->szOsFile * 2);
  pJournal = (sqlite3_file *)(((u8 *)pMaster) + pVfs->szOsFile);
  if( !pMaster ){
    rc = SQLITE_NOMEM;
  }else{
    const int flags = (SQLITE_OPEN_READONLY|SQLITE_OPEN_MASTER_JOURNAL);
    rc = sqlite3OsOpen(pVfs, zMaster, pMaster, flags, 0);
  }
  if( rc!=SQLITE_OK ) goto delmaster_out;

  /* Load the entire master journal file into space obtained from
  ** sqlite3_malloc() and pointed to by zMasterJournal.   Also obtain
  ** sufficient space (in zMasterPtr) to hold the names of master
  ** journal files extracted from regular rollback-journals.
  */
  rc = sqlite3OsFileSize(pMaster, &nMasterJournal);
  if( rc!=SQLITE_OK ) goto delmaster_out;
  nMasterPtr = pVfs->mxPathname+1;
  zMasterJournal = sqlite3Malloc((int)nMasterJournal + nMasterPtr + 1);
  if( !zMasterJournal ){
    rc = SQLITE_NOMEM;
    goto delmaster_out;
  }
  zMasterPtr = &zMasterJournal[nMasterJournal+1];
  rc = sqlite3OsRead(pMaster, zMasterJournal, (int)nMasterJournal, 0);
  if( rc!=SQLITE_OK ) goto delmaster_out;
  zMasterJournal[nMasterJournal] = 0;

  zJournal = zMasterJournal;
  while( (zJournal-zMasterJournal)<nMasterJournal ){
    int exists;
    rc = sqlite3OsAccess(pVfs, zJournal, SQLITE_ACCESS_EXISTS, &exists);
    if( rc!=SQLITE_OK ){
      goto delmaster_out;
    }
    if( exists ){
      /* One of the journals pointed to by the master journal exists.
      ** Open it and check if it points at the master journal. If
      ** so, return without deleting the master journal file.
      */
      int c;
      int flags = (SQLITE_OPEN_READONLY|SQLITE_OPEN_MAIN_JOURNAL);
      rc = sqlite3OsOpen(pVfs, zJournal, pJournal, flags, 0);
      if( rc!=SQLITE_OK ){
        goto delmaster_out;
      }

      rc = readMasterJournal(pJournal, zMasterPtr, nMasterPtr);
      sqlite3OsClose(pJournal);
      if( rc!=SQLITE_OK ){
        goto delmaster_out;
      }

      c = zMasterPtr[0]!=0 && strcmp(zMasterPtr, zMaster)==0;
      if( c ){
        /* We have a match. Do not delete the master journal file. */
        goto delmaster_out;
      }
    }
    zJournal += (sqlite3Strlen30(zJournal)+1);
  }
 
  sqlite3OsClose(pMaster);
  rc = sqlite3OsDelete(pVfs, zMaster, 0);

delmaster_out:
  sqlite3_free(zMasterJournal);
  if( pMaster ){
    sqlite3OsClose(pMaster);
    assert( !isOpen(pJournal) );
    sqlite3_free(pMaster);
  }
  return rc;
}


/*
** This function is used to change the actual size of the database 
** file in the file-system. This only happens when committing a transaction,
** or rolling back a transaction (including rolling back a hot-journal).
**
** If the main database file is not open, or the pager is not in either
** DBMOD or OPEN state, this function is a no-op. Otherwise, the size 
** of the file is changed to nPage pages (nPage*pPager->pageSize bytes). 
** If the file on disk is currently larger than nPage pages, then use the VFS
** xTruncate() method to truncate it.
**
** Or, it might might be the case that the file on disk is smaller than 
** nPage pages. Some operating system implementations can get confused if 
** you try to truncate a file to some size that is larger than it 
** currently is, so detect this case and write a single zero byte to 
** the end of the new file instead.
**
** If successful, return SQLITE_OK. If an IO error occurs while modifying
** the database file, return the error code to the caller.
*/
static int pager_truncate(Pager *pPager, Pgno nPage){
  int rc = SQLITE_OK;
  assert( pPager->eState!=PAGER_ERROR );
  assert( pPager->eState!=PAGER_READER );
  
  if( isOpen(pPager->fd) 
   && (pPager->eState>=PAGER_WRITER_DBMOD || pPager->eState==PAGER_OPEN) 
  ){
    i64 currentSize, newSize;
    int szPage = pPager->pageSize;
    assert( pPager->eLock==EXCLUSIVE_LOCK );
    /* TODO: Is it safe to use Pager.dbFileSize here? */
    rc = sqlite3OsFileSize(pPager->fd, &currentSize);
    newSize = szPage*(i64)nPage;
    if( rc==SQLITE_OK && currentSize!=newSize ){
      if( currentSize>newSize ){
        rc = sqlite3OsTruncate(pPager->fd, newSize);
      }else if( (currentSize+szPage)<=newSize ){
        char *pTmp = pPager->pTmpSpace;
        memset(pTmp, 0, szPage);
        testcase( (newSize-szPage) == currentSize );
        testcase( (newSize-szPage) >  currentSize );
        rc = sqlite3OsWrite(pPager->fd, pTmp, szPage, newSize-szPage);
      }
      if( rc==SQLITE_OK ){
        pPager->dbFileSize = nPage;
      }
    }
  }
  return rc;
}

/*
** Set the value of the Pager.sectorSize variable for the given
** pager based on the value returned by the xSectorSize method
** of the open database file. The sector size will be used used 
** to determine the size and alignment of journal header and 
** master journal pointers within created journal files.
**
** For temporary files the effective sector size is always 512 bytes.
**
** Otherwise, for non-temporary files, the effective sector size is
** the value returned by the xSectorSize() method rounded up to 32 if
** it is less than 32, or rounded down to MAX_SECTOR_SIZE if it
** is greater than MAX_SECTOR_SIZE.
**
** If the file has the SQLITE_IOCAP_POWERSAFE_OVERWRITE property, then set
** the effective sector size to its minimum value (512).  The purpose of
** pPager->sectorSize is to define the "blast radius" of bytes that
** might change if a crash occurs while writing to a single byte in
** that range.  But with POWERSAFE_OVERWRITE, the blast radius is zero
** (that is what POWERSAFE_OVERWRITE means), so we minimize the sector
** size.  For backwards compatibility of the rollback journal file format,
** we cannot reduce the effective sector size below 512.
*/
static void setSectorSize(Pager *pPager){
  assert( isOpen(pPager->fd) || pPager->tempFile );

  if( pPager->tempFile
   || (sqlite3OsDeviceCharacteristics(pPager->fd) & 
              SQLITE_IOCAP_POWERSAFE_OVERWRITE)!=0
  ){
    /* Sector size doesn't matter for temporary files. Also, the file
    ** may not have been opened yet, in which case the OsSectorSize()
    ** call will segfault. */
    pPager->sectorSize = 512;
  }else{
    pPager->sectorSize = sqlite3OsSectorSize(pPager->fd);
    if( pPager->sectorSize<32 ){
      pPager->sectorSize = 512;
    }
    if( pPager->sectorSize>MAX_SECTOR_SIZE ){
      assert( MAX_SECTOR_SIZE>=512 );
      pPager->sectorSize = MAX_SECTOR_SIZE;
    }
  }
}

/*
** Playback the journal and thus restore the database file to
** the state it was in before we started making changes.  
**
** The journal file format is as follows: 
**
**  (1)  8 byte prefix.  A copy of aJournalMagic[].
**  (2)  4 byte big-endian integer which is the number of valid page records
**       in the journal.  If this value is 0xffffffff, then compute the
**       number of page records from the journal size.
**  (3)  4 byte big-endian integer which is the initial value for the 
**       sanity checksum.
**  (4)  4 byte integer which is the number of pages to truncate the
**       database to during a rollback.
**  (5)  4 byte big-endian integer which is the sector size.  The header
**       is this many bytes in size.
**  (6)  4 byte big-endian integer which is the page size.
**  (7)  zero padding out to the next sector size.
**  (8)  Zero or more pages instances, each as follows:
**        +  4 byte page number.
**        +  pPager->pageSize bytes of data.
**        +  4 byte checksum
**
** When we speak of the journal header, we mean the first 7 items above.
** Each entry in the journal is an instance of the 8th item.
**
** Call the value from the second bullet "nRec".  nRec is the number of
** valid page entries in the journal.  In most cases, you can compute the
** value of nRec from the size of the journal file.  But if a power
** failure occurred while the journal was being written, it could be the
** case that the size of the journal file had already been increased but
** the extra entries had not yet made it safely to disk.  In such a case,
** the value of nRec computed from the file size would be too large.  For
** that reason, we always use the nRec value in the header.
**
** If the nRec value is 0xffffffff it means that nRec should be computed
** from the file size.  This value is used when the user selects the
** no-sync option for the journal.  A power failure could lead to corruption
** in this case.  But for things like temporary table (which will be
** deleted when the power is restored) we don't care.  
**
** If the file opened as the journal file is not a well-formed
** journal file then all pages up to the first corrupted page are rolled
** back (or no pages if the journal header is corrupted). The journal file
** is then deleted and SQLITE_OK returned, just as if no corruption had
** been encountered.
**
** If an I/O or malloc() error occurs, the journal-file is not deleted
** and an error code is returned.
**
** The isHot parameter indicates that we are trying to rollback a journal
** that might be a hot journal.  Or, it could be that the journal is 
** preserved because of JOURNALMODE_PERSIST or JOURNALMODE_TRUNCATE.
** If the journal really is hot, reset the pager cache prior rolling
** back any content.  If the journal is merely persistent, no reset is
** needed.
*/
static int pager_playback(Pager *pPager, int isHot){
  sqlite3_vfs *pVfs = pPager->pVfs;
  i64 szJ;                 /* Size of the journal file in bytes */
  u32 nRec;                /* Number of Records in the journal */
  u32 u;                   /* Unsigned loop counter */
  Pgno mxPg = 0;           /* Size of the original file in pages */
  int rc;                  /* Result code of a subroutine */
  int res = 1;             /* Value returned by sqlite3OsAccess() */
  char *zMaster = 0;       /* Name of master journal file if any */
  int needPagerReset;      /* True to reset page prior to first page rollback */

  /* Figure out how many records are in the journal.  Abort early if
  ** the journal is empty.
  */
  assert( isOpen(pPager->jfd) );
  rc = sqlite3OsFileSize(pPager->jfd, &szJ);
  if( rc!=SQLITE_OK ){
    goto end_playback;
  }

  /* Read the master journal name from the journal, if it is present.
  ** If a master journal file name is specified, but the file is not
  ** present on disk, then the journal is not hot and does not need to be
  ** played back.
  **
  ** TODO: Technically the following is an error because it assumes that
  ** buffer Pager.pTmpSpace is (mxPathname+1) bytes or larger. i.e. that
  ** (pPager->pageSize >= pPager->pVfs->mxPathname+1). Using os_unix.c,
  **  mxPathname is 512, which is the same as the minimum allowable value
  ** for pageSize.
  */
  zMaster = pPager->pTmpSpace;
  rc = readMasterJournal(pPager->jfd, zMaster, pPager->pVfs->mxPathname+1);
  if( rc==SQLITE_OK && zMaster[0] ){
    rc = sqlite3OsAccess(pVfs, zMaster, SQLITE_ACCESS_EXISTS, &res);
  }
  zMaster = 0;
  if( rc!=SQLITE_OK || !res ){
    goto end_playback;
  }
  pPager->journalOff = 0;
  needPagerReset = isHot;

  /* This loop terminates either when a readJournalHdr() or 
  ** pager_playback_one_page() call returns SQLITE_DONE or an IO error 
  ** occurs. 
  */
  while( 1 ){
    /* Read the next journal header from the journal file.  If there are
    ** not enough bytes left in the journal file for a complete header, or
    ** it is corrupted, then a process must have failed while writing it.
    ** This indicates nothing more needs to be rolled back.
    */
    rc = readJournalHdr(pPager, isHot, szJ, &nRec, &mxPg);
    if( rc!=SQLITE_OK ){ 
      if( rc==SQLITE_DONE ){
        rc = SQLITE_OK;
      }
      goto end_playback;
    }

    /* If nRec is 0xffffffff, then this journal was created by a process
    ** working in no-sync mode. This means that the rest of the journal
    ** file consists of pages, there are no more journal headers. Compute
    ** the value of nRec based on this assumption.
    */
    if( nRec==0xffffffff ){
      assert( pPager->journalOff==JOURNAL_HDR_SZ(pPager) );
      nRec = (int)((szJ - JOURNAL_HDR_SZ(pPager))/JOURNAL_PG_SZ(pPager));
    }

    /* If nRec is 0 and this rollback is of a transaction created by this
    ** process and if this is the final header in the journal, then it means
    ** that this part of the journal was being filled but has not yet been
    ** synced to disk.  Compute the number of pages based on the remaining
    ** size of the file.
    **
    ** The third term of the test was added to fix ticket #2565.
    ** When rolling back a hot journal, nRec==0 always means that the next
    ** chunk of the journal contains zero pages to be rolled back.  But
    ** when doing a ROLLBACK and the nRec==0 chunk is the last chunk in
    ** the journal, it means that the journal might contain additional
    ** pages that need to be rolled back and that the number of pages 
    ** should be computed based on the journal file size.
    */
    if( nRec==0 && !isHot &&
        pPager->journalHdr+JOURNAL_HDR_SZ(pPager)==pPager->journalOff ){
      nRec = (int)((szJ - pPager->journalOff) / JOURNAL_PG_SZ(pPager));
    }

    /* If this is the first header read from the journal, truncate the
    ** database file back to its original size.
    */
    if( pPager->journalOff==JOURNAL_HDR_SZ(pPager) ){
      rc = pager_truncate(pPager, mxPg);
      if( rc!=SQLITE_OK ){
        goto end_playback;
      }
      pPager->dbSize = mxPg;
    }

    /* Copy original pages out of the journal and back into the 
    ** database file and/or page cache.
    */
    for(u=0; u<nRec; u++){
      if( needPagerReset ){
        pager_reset(pPager);
        needPagerReset = 0;
      }
      rc = pager_playback_one_page(pPager,&pPager->journalOff,0,1,0);
      if( rc!=SQLITE_OK ){
        if( rc==SQLITE_DONE ){
          pPager->journalOff = szJ;
          break;
        }else if( rc==SQLITE_IOERR_SHORT_READ ){
          /* If the journal has been truncated, simply stop reading and
          ** processing the journal. This might happen if the journal was
          ** not completely written and synced prior to a crash.  In that
          ** case, the database should have never been written in the
          ** first place so it is OK to simply abandon the rollback. */
          rc = SQLITE_OK;
          goto end_playback;
        }else{
          /* If we are unable to rollback, quit and return the error
          ** code.  This will cause the pager to enter the error state
          ** so that no further harm will be done.  Perhaps the next
          ** process to come along will be able to rollback the database.
          */
          goto end_playback;
        }
      }
    }
  }
  /*NOTREACHED*/
  assert( 0 );

end_playback:
  /* Following a rollback, the database file should be back in its original
  ** state prior to the start of the transaction, so invoke the
  ** SQLITE_FCNTL_DB_UNCHANGED file-control method to disable the
  ** assertion that the transaction counter was modified.
  */
#ifdef SQLITE_DEBUG
  if( pPager->fd->pMethods ){
    sqlite3OsFileControlHint(pPager->fd,SQLITE_FCNTL_DB_UNCHANGED,0);
  }
#endif

  /* If this playback is happening automatically as a result of an IO or 
  ** malloc error that occurred after the change-counter was updated but 
  ** before the transaction was committed, then the change-counter 
  ** modification may just have been reverted. If this happens in exclusive 
  ** mode, then subsequent transactions performed by the connection will not
  ** update the change-counter at all. This may lead to cache inconsistency
  ** problems for other processes at some point in the future. So, just
  ** in case this has happened, clear the changeCountDone flag now.
  */
  pPager->changeCountDone = pPager->tempFile;

  if( rc==SQLITE_OK ){
    zMaster = pPager->pTmpSpace;
    rc = readMasterJournal(pPager->jfd, zMaster, pPager->pVfs->mxPathname+1);
    testcase( rc!=SQLITE_OK );
  }
  if( rc==SQLITE_OK
   && (pPager->eState>=PAGER_WRITER_DBMOD || pPager->eState==PAGER_OPEN)
  ){
    rc = sqlite3PagerSync(pPager);
  }
  if( rc==SQLITE_OK ){
    rc = pager_end_transaction(pPager, zMaster[0]!='\0');
    testcase( rc!=SQLITE_OK );
  }
  if( rc==SQLITE_OK && zMaster[0] && res ){
    /* If there was a master journal and this routine will return success,
    ** see if it is possible to delete the master journal.
    */
    rc = pager_delmaster(pPager, zMaster);
    testcase( rc!=SQLITE_OK );
  }

  /* The Pager.sectorSize variable may have been updated while rolling
  ** back a journal created by a process with a different sector size
  ** value. Reset it to the correct value for this process.
  */
  setSectorSize(pPager);
  return rc;
}


/*
** Read the content for page pPg out of the database file and into 
** pPg->pData. A shared lock or greater must be held on the database
** file before this function is called.
**
** If page 1 is read, then the value of Pager.dbFileVers[] is set to
** the value read from the database file.
**
** If an IO error occurs, then the IO error is returned to the caller.
** Otherwise, SQLITE_OK is returned.
*/
static int readDbPage(PgHdr *pPg){
  Pager *pPager = pPg->pPager; /* Pager object associated with page pPg */
  Pgno pgno = pPg->pgno;       /* Page number to read */
  int rc = SQLITE_OK;          /* Return code */
  int isInWal = 0;             /* True if page is in log file */
  int pgsz = pPager->pageSize; /* Number of bytes to read */

  assert( pPager->eState>=PAGER_READER && !MEMDB );
  assert( isOpen(pPager->fd) );

  if( NEVER(!isOpen(pPager->fd)) ){
    assert( pPager->tempFile );
    memset(pPg->pData, 0, pPager->pageSize);
    return SQLITE_OK;
  }

  if( pagerUseWal(pPager) ){
    /* Try to pull the page from the write-ahead log. */
    rc = sqlite3WalRead(pPager->pWal, pgno, &isInWal, pgsz, pPg->pData);
  }
  if( rc==SQLITE_OK && !isInWal ){
    i64 iOffset = (pgno-1)*(i64)pPager->pageSize;
    rc = sqlite3OsRead(pPager->fd, pPg->pData, pgsz, iOffset);
    if( rc==SQLITE_IOERR_SHORT_READ ){
      rc = SQLITE_OK;
    }
  }

  if( pgno==1 ){
    if( rc ){
      /* If the read is unsuccessful, set the dbFileVers[] to something
      ** that will never be a valid file version.  dbFileVers[] is a copy
      ** of bytes 24..39 of the database.  Bytes 28..31 should always be
      ** zero or the size of the database in page. Bytes 32..35 and 35..39
      ** should be page numbers which are never 0xffffffff.  So filling
      ** pPager->dbFileVers[] with all 0xff bytes should suffice.
      **
      ** For an encrypted database, the situation is more complex:  bytes
      ** 24..39 of the database are white noise.  But the probability of
      ** white noising equaling 16 bytes of 0xff is vanishingly small so
      ** we should still be ok.
      */
      memset(pPager->dbFileVers, 0xff, sizeof(pPager->dbFileVers));
    }else{
      u8 *dbFileVers = &((u8*)pPg->pData)[24];
      memcpy(&pPager->dbFileVers, dbFileVers, sizeof(pPager->dbFileVers));
    }
  }
  CODEC1(pPager, pPg->pData, pgno, 3, rc = SQLITE_NOMEM);

  PAGER_INCR(sqlite3_pager_readdb_count);
  PAGER_INCR(pPager->nRead);
  IOTRACE(("PGIN %p %d\n", pPager, pgno));
  PAGERTRACE(("FETCH %d page %d hash(%08x)\n",
               PAGERID(pPager), pgno, pager_pagehash(pPg)));

  return rc;
}

/*
** Update the value of the change-counter at offsets 24 and 92 in
** the header and the sqlite version number at offset 96.
**
** This is an unconditional update.  See also the pager_incr_changecounter()
** routine which only updates the change-counter if the update is actually
** needed, as determined by the pPager->changeCountDone state variable.
*/
static void pager_write_changecounter(PgHdr *pPg){
  u32 change_counter;

  /* Increment the value just read and write it back to byte 24. */
  change_counter = sqlite3Get4byte((u8*)pPg->pPager->dbFileVers)+1;
  put32bits(((char*)pPg->pData)+24, change_counter);

  /* Also store the SQLite version number in bytes 96..99 and in
  ** bytes 92..95 store the change counter for which the version number
  ** is valid. */
  put32bits(((char*)pPg->pData)+92, change_counter);
  put32bits(((char*)pPg->pData)+96, SQLITE_VERSION_NUMBER);
}

#ifndef SQLITE_OMIT_WAL
/*
** This function is invoked once for each page that has already been 
** written into the log file when a WAL transaction is rolled back.
** Parameter iPg is the page number of said page. The pCtx argument 
** is actually a pointer to the Pager structure.
**
** If page iPg is present in the cache, and has no outstanding references,
** it is discarded. Otherwise, if there are one or more outstanding
** references, the page content is reloaded from the database. If the
** attempt to reload content from the database is required and fails, 
** return an SQLite error code. Otherwise, SQLITE_OK.
*/
static int pagerUndoCallback(void *pCtx, Pgno iPg){
  int rc = SQLITE_OK;
  Pager *pPager = (Pager *)pCtx;
  PgHdr *pPg;

  pPg = sqlite3PagerLookup(pPager, iPg);
  if( pPg ){
    if( sqlite3PcachePageRefcount(pPg)==1 ){
      sqlite3PcacheDrop(pPg);
    }else{
      rc = readDbPage(pPg);
      if( rc==SQLITE_OK ){
        pPager->xReiniter(pPg);
      }
      sqlite3PagerUnref(pPg);
    }
  }

  /* Normally, if a transaction is rolled back, any backup processes are
  ** updated as data is copied out of the rollback journal and into the
  ** database. This is not generally possible with a WAL database, as
  ** rollback involves simply truncating the log file. Therefore, if one
  ** or more frames have already been written to the log (and therefore 
  ** also copied into the backup databases) as part of this transaction,
  ** the backups must be restarted.
  */
  sqlite3BackupRestart(pPager->pBackup);

  return rc;
}

/*
** This function is called to rollback a transaction on a WAL database.
*/
static int pagerRollbackWal(Pager *pPager){
  int rc;                         /* Return Code */
  PgHdr *pList;                   /* List of dirty pages to revert */

  /* For all pages in the cache that are currently dirty or have already
  ** been written (but not committed) to the log file, do one of the 
  ** following:
  **
  **   + Discard the cached page (if refcount==0), or
  **   + Reload page content from the database (if refcount>0).
  */
  pPager->dbSize = pPager->dbOrigSize;
  rc = sqlite3WalUndo(pPager->pWal, pagerUndoCallback, (void *)pPager);
  pList = sqlite3PcacheDirtyList(pPager->pPCache);
  while( pList && rc==SQLITE_OK ){
    PgHdr *pNext = pList->pDirty;
    rc = pagerUndoCallback((void *)pPager, pList->pgno);
    pList = pNext;
  }

  return rc;
}

/*
** This function is a wrapper around sqlite3WalFrames(). As well as logging
** the contents of the list of pages headed by pList (connected by pDirty),
** this function notifies any active backup processes that the pages have
** changed. 
**
** The list of pages passed into this routine is always sorted by page number.
** Hence, if page 1 appears anywhere on the list, it will be the first page.
*/ 
static int pagerWalFrames(
  Pager *pPager,                  /* Pager object */
  PgHdr *pList,                   /* List of frames to log */
  Pgno nTruncate,                 /* Database size after this commit */
  int isCommit                    /* True if this is a commit */
){
  int rc;                         /* Return code */
  int nList;                      /* Number of pages in pList */
#if defined(SQLITE_DEBUG) || defined(SQLITE_CHECK_PAGES)
  PgHdr *p;                       /* For looping over pages */
#endif

  assert( pPager->pWal );
  assert( pList );
#ifdef SQLITE_DEBUG
  /* Verify that the page list is in accending order */
  for(p=pList; p && p->pDirty; p=p->pDirty){
    assert( p->pgno < p->pDirty->pgno );
  }
#endif

  assert( pList->pDirty==0 || isCommit );
  if( isCommit ){
    /* If a WAL transaction is being committed, there is no point in writing
    ** any pages with page numbers greater than nTruncate into the WAL file.
    ** They will never be read by any client. So remove them from the pDirty
    ** list here. */
    PgHdr *p;
    PgHdr **ppNext = &pList;
    nList = 0;
    for(p=pList; (*ppNext = p)!=0; p=p->pDirty){
      if( p->pgno<=nTruncate ){
        ppNext = &p->pDirty;
        nList++;
      }
    }
    assert( pList );
  }else{
    nList = 1;
  }
  pPager->aStat[PAGER_STAT_WRITE] += nList;

  if( pList->pgno==1 ) pager_write_changecounter(pList);
  rc = sqlite3WalFrames(pPager->pWal, 
      pPager->pageSize, pList, nTruncate, isCommit, pPager->walSyncFlags
  );
  if( rc==SQLITE_OK && pPager->pBackup ){
    PgHdr *p;
    for(p=pList; p; p=p->pDirty){
      sqlite3BackupUpdate(pPager->pBackup, p->pgno, (u8 *)p->pData);
    }
  }

#ifdef SQLITE_CHECK_PAGES
  pList = sqlite3PcacheDirtyList(pPager->pPCache);
  for(p=pList; p; p=p->pDirty){
    pager_set_pagehash(p);
  }
#endif

  return rc;
}

/*
** Begin a read transaction on the WAL.
**
** This routine used to be called "pagerOpenSnapshot()" because it essentially
** makes a snapshot of the database at the current point in time and preserves
** that snapshot for use by the reader in spite of concurrently changes by
** other writers or checkpointers.
*/
static int pagerBeginReadTransaction(Pager *pPager){
  int rc;                         /* Return code */
  int changed = 0;                /* True if cache must be reset */

  assert( pagerUseWal(pPager) );
  assert( pPager->eState==PAGER_OPEN || pPager->eState==PAGER_READER );

  /* sqlite3WalEndReadTransaction() was not called for the previous
  ** transaction in locking_mode=EXCLUSIVE.  So call it now.  If we
  ** are in locking_mode=NORMAL and EndRead() was previously called,
  ** the duplicate call is harmless.
  */
  sqlite3WalEndReadTransaction(pPager->pWal);

  rc = sqlite3WalBeginReadTransaction(pPager->pWal, &changed);
  if( rc!=SQLITE_OK || changed ){
    pager_reset(pPager);
  }

  return rc;
}
#endif

/*
** This function is called as part of the transition from PAGER_OPEN
** to PAGER_READER state to determine the size of the database file
** in pages (assuming the page size currently stored in Pager.pageSize).
**
** If no error occurs, SQLITE_OK is returned and the size of the database
** in pages is stored in *pnPage. Otherwise, an error code (perhaps
** SQLITE_IOERR_FSTAT) is returned and *pnPage is left unmodified.
*/
static int pagerPagecount(Pager *pPager, Pgno *pnPage){
  Pgno nPage;                     /* Value to return via *pnPage */

  /* Query the WAL sub-system for the database size. The WalDbsize()
  ** function returns zero if the WAL is not open (i.e. Pager.pWal==0), or
  ** if the database size is not available. The database size is not
  ** available from the WAL sub-system if the log file is empty or
  ** contains no valid committed transactions.
  */
  assert( pPager->eState==PAGER_OPEN );
  assert( pPager->eLock>=SHARED_LOCK );
  nPage = sqlite3WalDbsize(pPager->pWal);

  /* If the database size was not available from the WAL sub-system,
  ** determine it based on the size of the database file. If the size
  ** of the database file is not an integer multiple of the page-size,
  ** round down to the nearest page. Except, any file larger than 0
  ** bytes in size is considered to contain at least one page.
  */
  if( nPage==0 ){
    i64 n = 0;                    /* Size of db file in bytes */
    assert( isOpen(pPager->fd) || pPager->tempFile );
    if( isOpen(pPager->fd) ){
      int rc = sqlite3OsFileSize(pPager->fd, &n);
      if( rc!=SQLITE_OK ){
        return rc;
      }
    }
    nPage = (Pgno)((n+pPager->pageSize-1) / pPager->pageSize);
  }

  /* If the current number of pages in the file is greater than the
  ** configured maximum pager number, increase the allowed limit so
  ** that the file can be read.
  */
  if( nPage>pPager->mxPgno ){
    pPager->mxPgno = (Pgno)nPage;
  }

  *pnPage = nPage;
  return SQLITE_OK;
}

#ifndef SQLITE_OMIT_WAL
/*
** Check if the *-wal file that corresponds to the database opened by pPager
** exists if the database is not empy, or verify that the *-wal file does
** not exist (by deleting it) if the database file is empty.
**
** If the database is not empty and the *-wal file exists, open the pager
** in WAL mode.  If the database is empty or if no *-wal file exists and
** if no error occurs, make sure Pager.journalMode is not set to
** PAGER_JOURNALMODE_WAL.
**
** Return SQLITE_OK or an error code.
**
** The caller must hold a SHARED lock on the database file to call this
** function. Because an EXCLUSIVE lock on the db file is required to delete 
** a WAL on a none-empty database, this ensures there is no race condition 
** between the xAccess() below and an xDelete() being executed by some 
** other connection.
*/
static int pagerOpenWalIfPresent(Pager *pPager){
  int rc = SQLITE_OK;
  assert( pPager->eState==PAGER_OPEN );
  assert( pPager->eLock>=SHARED_LOCK );

  if( !pPager->tempFile ){
    int isWal;                    /* True if WAL file exists */
    Pgno nPage;                   /* Size of the database file */

    rc = pagerPagecount(pPager, &nPage);
    if( rc ) return rc;
    if( nPage==0 ){
      rc = sqlite3OsDelete(pPager->pVfs, pPager->zWal, 0);
      isWal = 0;
    }else{
      rc = sqlite3OsAccess(
          pPager->pVfs, pPager->zWal, SQLITE_ACCESS_EXISTS, &isWal
      );
    }
    if( rc==SQLITE_OK ){
      if( isWal ){
        testcase( sqlite3PcachePagecount(pPager->pPCache)==0 );
        rc = sqlite3PagerOpenWal(pPager, 0);
      }else if( pPager->journalMode==PAGER_JOURNALMODE_WAL ){
        pPager->journalMode = PAGER_JOURNALMODE_DELETE;
      }
    }
  }
  return rc;
}
#endif

/*
** Playback savepoint pSavepoint. Or, if pSavepoint==NULL, then playback
** the entire master journal file. The case pSavepoint==NULL occurs when 
** a ROLLBACK TO command is invoked on a SAVEPOINT that is a transaction 
** savepoint.
**
** When pSavepoint is not NULL (meaning a non-transaction savepoint is 
** being rolled back), then the rollback consists of up to three stages,
** performed in the order specified:
**
**   * Pages are played back from the main journal starting at byte
**     offset PagerSavepoint.iOffset and continuing to 
**     PagerSavepoint.iHdrOffset, or to the end of the main journal
**     file if PagerSavepoint.iHdrOffset is zero.
**
**   * If PagerSavepoint.iHdrOffset is not zero, then pages are played
**     back starting from the journal header immediately following 
**     PagerSavepoint.iHdrOffset to the end of the main journal file.
**
**   * Pages are then played back from the sub-journal file, starting
**     with the PagerSavepoint.iSubRec and continuing to the end of
**     the journal file.
**
** Throughout the rollback process, each time a page is rolled back, the
** corresponding bit is set in a bitvec structure (variable pDone in the
** implementation below). This is used to ensure that a page is only
** rolled back the first time it is encountered in either journal.
**
** If pSavepoint is NULL, then pages are only played back from the main
** journal file. There is no need for a bitvec in this case.
**
** In either case, before playback commences the Pager.dbSize variable
** is reset to the value that it held at the start of the savepoint 
** (or transaction). No page with a page-number greater than this value
** is played back. If one is encountered it is simply skipped.
*/
static int pagerPlaybackSavepoint(Pager *pPager, PagerSavepoint *pSavepoint){
  i64 szJ;                 /* Effective size of the main journal */
  i64 iHdrOff;             /* End of first segment of main-journal records */
  int rc = SQLITE_OK;      /* Return code */
  Bitvec *pDone = 0;       /* Bitvec to ensure pages played back only once */

  assert( pPager->eState!=PAGER_ERROR );
  assert( pPager->eState>=PAGER_WRITER_LOCKED );

  /* Allocate a bitvec to use to store the set of pages rolled back */
  if( pSavepoint ){
    pDone = sqlite3BitvecCreate(pSavepoint->nOrig);
    if( !pDone ){
      return SQLITE_NOMEM;
    }
  }

  /* Set the database size back to the value it was before the savepoint 
  ** being reverted was opened.
  */
  pPager->dbSize = pSavepoint ? pSavepoint->nOrig : pPager->dbOrigSize;
  pPager->changeCountDone = pPager->tempFile;

  if( !pSavepoint && pagerUseWal(pPager) ){
    return pagerRollbackWal(pPager);
  }

  /* Use pPager->journalOff as the effective size of the main rollback
  ** journal.  The actual file might be larger than this in
  ** PAGER_JOURNALMODE_TRUNCATE or PAGER_JOURNALMODE_PERSIST.  But anything
  ** past pPager->journalOff is off-limits to us.
  */
  szJ = pPager->journalOff;
  assert( pagerUseWal(pPager)==0 || szJ==0 );

  /* Begin by rolling back records from the main journal starting at
  ** PagerSavepoint.iOffset and continuing to the next journal header.
  ** There might be records in the main journal that have a page number
  ** greater than the current database size (pPager->dbSize) but those
  ** will be skipped automatically.  Pages are added to pDone as they
  ** are played back.
  */
  if( pSavepoint && !pagerUseWal(pPager) ){
    iHdrOff = pSavepoint->iHdrOffset ? pSavepoint->iHdrOffset : szJ;
    pPager->journalOff = pSavepoint->iOffset;
    while( rc==SQLITE_OK && pPager->journalOff<iHdrOff ){
      rc = pager_playback_one_page(pPager, &pPager->journalOff, pDone, 1, 1);
    }
    assert( rc!=SQLITE_DONE );
  }else{
    pPager->journalOff = 0;
  }

  /* Continue rolling back records out of the main journal starting at
  ** the first journal header seen and continuing until the effective end
  ** of the main journal file.  Continue to skip out-of-range pages and
  ** continue adding pages rolled back to pDone.
  */
  while( rc==SQLITE_OK && pPager->journalOff<szJ ){
    u32 ii;            /* Loop counter */
    u32 nJRec = 0;     /* Number of Journal Records */
    u32 dummy;
    rc = readJournalHdr(pPager, 0, szJ, &nJRec, &dummy);
    assert( rc!=SQLITE_DONE );

    /*
    ** The "pPager->journalHdr+JOURNAL_HDR_SZ(pPager)==pPager->journalOff"
    ** test is related to ticket #2565.  See the discussion in the
    ** pager_playback() function for additional information.
    */
    if( nJRec==0 
     && pPager->journalHdr+JOURNAL_HDR_SZ(pPager)==pPager->journalOff
    ){
      nJRec = (u32)((szJ - pPager->journalOff)/JOURNAL_PG_SZ(pPager));
    }
    for(ii=0; rc==SQLITE_OK && ii<nJRec && pPager->journalOff<szJ; ii++){
      rc = pager_playback_one_page(pPager, &pPager->journalOff, pDone, 1, 1);
    }
    assert( rc!=SQLITE_DONE );
  }
  assert( rc!=SQLITE_OK || pPager->journalOff>=szJ );

  /* Finally,  rollback pages from the sub-journal.  Page that were
  ** previously rolled back out of the main journal (and are hence in pDone)
  ** will be skipped.  Out-of-range pages are also skipped.
  */
  if( pSavepoint ){
    u32 ii;            /* Loop counter */
    i64 offset = (i64)pSavepoint->iSubRec*(4+pPager->pageSize);

    if( pagerUseWal(pPager) ){
      rc = sqlite3WalSavepointUndo(pPager->pWal, pSavepoint->aWalData);
    }
    for(ii=pSavepoint->iSubRec; rc==SQLITE_OK && ii<pPager->nSubRec; ii++){
      assert( offset==(i64)ii*(4+pPager->pageSize) );
      rc = pager_playback_one_page(pPager, &offset, pDone, 0, 1);
    }
    assert( rc!=SQLITE_DONE );
  }

  sqlite3BitvecDestroy(pDone);
  if( rc==SQLITE_OK ){
    pPager->journalOff = szJ;
  }

  return rc;
}

/*
** Change the maximum number of in-memory pages that are allowed.
*/
void sqlite3PagerSetCachesize(Pager *pPager, int mxPage){
  sqlite3PcacheSetCachesize(pPager->pPCache, mxPage);
}

/*
** Free as much memory as possible from the pager.
*/
void sqlite3PagerShrink(Pager *pPager){
  sqlite3PcacheShrink(pPager->pPCache);
}

/*
** Adjust the robustness of the database to damage due to OS crashes
** or power failures by changing the number of syncs()s when writing
** the rollback journal.  There are three levels:
**
**    OFF       sqlite3OsSync() is never called.  This is the default
**              for temporary and transient files.
**
**    NORMAL    The journal is synced once before writes begin on the
**              database.  This is normally adequate protection, but
**              it is theoretically possible, though very unlikely,
**              that an inopertune power failure could leave the journal
**              in a state which would cause damage to the database
**              when it is rolled back.
**
**    FULL      The journal is synced twice before writes begin on the
**              database (with some additional information - the nRec field
**              of the journal header - being written in between the two
**              syncs).  If we assume that writing a
**              single disk sector is atomic, then this mode provides
**              assurance that the journal will not be corrupted to the
**              point of causing damage to the database during rollback.
**
** The above is for a rollback-journal mode.  For WAL mode, OFF continues
** to mean that no syncs ever occur.  NORMAL means that the WAL is synced
** prior to the start of checkpoint and that the database file is synced
** at the conclusion of the checkpoint if the entire content of the WAL
** was written back into the database.  But no sync operations occur for
** an ordinary commit in NORMAL mode with WAL.  FULL means that the WAL
** file is synced following each commit operation, in addition to the
** syncs associated with NORMAL.
**
** Do not confuse synchronous=FULL with SQLITE_SYNC_FULL.  The
** SQLITE_SYNC_FULL macro means to use the MacOSX-style full-fsync
** using fcntl(F_FULLFSYNC).  SQLITE_SYNC_NORMAL means to do an
** ordinary fsync() call.  There is no difference between SQLITE_SYNC_FULL
** and SQLITE_SYNC_NORMAL on platforms other than MacOSX.  But the
** synchronous=FULL versus synchronous=NORMAL setting determines when
** the xSync primitive is called and is relevant to all platforms.
**
** Numeric values associated with these states are OFF==1, NORMAL=2,
** and FULL=3.
*/
#ifndef SQLITE_OMIT_PAGER_PRAGMAS
void sqlite3PagerSetSafetyLevel(
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
#endif

/*
** The following global variable is incremented whenever the library
** attempts to open a temporary file.  This information is used for
** testing and analysis only.  
*/
#ifdef SQLITE_TEST
int sqlite3_opentemp_count = 0;
#endif

/*
** Open a temporary file.
**
** Write the file descriptor into *pFile. Return SQLITE_OK on success 
** or some other error code if we fail. The OS will automatically 
** delete the temporary file when it is closed.
**
** The flags passed to the VFS layer xOpen() call are those specified
** by parameter vfsFlags ORed with the following:
**
**     SQLITE_OPEN_READWRITE
**     SQLITE_OPEN_CREATE
**     SQLITE_OPEN_EXCLUSIVE
**     SQLITE_OPEN_DELETEONCLOSE
*/
static int pagerOpentemp(
  Pager *pPager,        /* The pager object */
  sqlite3_file *pFile,  /* Write the file descriptor here */
  int vfsFlags          /* Flags passed through to the VFS */
){
  int rc;               /* Return code */

#ifdef SQLITE_TEST
  sqlite3_opentemp_count++;  /* Used for testing and analysis only */
#endif

  vfsFlags |=  SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
            SQLITE_OPEN_EXCLUSIVE | SQLITE_OPEN_DELETEONCLOSE;
  rc = sqlite3OsOpen(pPager->pVfs, 0, pFile, vfsFlags, 0);
  assert( rc!=SQLITE_OK || isOpen(pFile) );
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
void sqlite3PagerSetBusyhandler(
  Pager *pPager,                       /* Pager object */
  int (*xBusyHandler)(void *),         /* Pointer to busy-handler function */
  void *pBusyHandlerArg                /* Argument to pass to xBusyHandler */
){  
  pPager->xBusyHandler = xBusyHandler;
  pPager->pBusyHandlerArg = pBusyHandlerArg;
}
//我的到此结束。
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
** Return a pointer to the "temporary page" buffer held internally
** by the pager.  This is a buffer that is big enough to hold the
** entire content of a database page.  This buffer is used internally
** during rollback and will be overwritten whenever a rollback
** occurs.  But other modules are free to use it too, as long as
** no rollbacks are happening.
*/
void *sqlite3PagerTempSpace(Pager *pPager){
  return pPager->pTmpSpace;
}

/*
** Attempt to set the maximum database page count if mxPage is positive. 
** Make no changes if mxPage is zero or negative.  And never reduce the
** maximum page count below the current size of the database.
**
** Regardless of mxPage, return the current maximum page count.
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
** The following set of routines are used to disable the simulated
** I/O error mechanism.  These routines are used to avoid simulated
** errors in places where we do not care about errors.
**
** Unless -DSQLITE_TEST=1 is used, these routines are all no-ops
** and generate no code.
*/
#ifdef SQLITE_TEST
extern int sqlite3_io_error_pending;
extern int sqlite3_io_error_hit;
static int saved_cnt;
void disable_simulated_io_errors(void){
  saved_cnt = sqlite3_io_error_pending;
  sqlite3_io_error_pending = -1;
}
void enable_simulated_io_errors(void){
  sqlite3_io_error_pending = saved_cnt;
}
#else
# define disable_simulated_io_errors()
# define enable_simulated_io_errors()
#endif

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
int sqlite3PagerReadFileheader(Pager *pPager, int N, unsigned char *pDest){
  int rc = SQLITE_OK;
  memset(pDest, 0, N);
  assert( isOpen(pPager->fd) || pPager->tempFile );

  /* This routine is only called by btree immediately after creating
  ** the Pager object.  There has not been an opportunity to transition
  ** to WAL mode yet.
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
** This function may only be called when a read-transaction is open on
** the pager. It returns the total number of pages in the database.
**
** However, if the file is between 1 and <page-size> bytes in size, then 
** this is considered a 1 page file.
*/
void sqlite3PagerPagecount(Pager *pPager, int *pnPage){
  assert( pPager->eState>=PAGER_READER );
  assert( pPager->eState!=PAGER_WRITER_FINISHED );
  *pnPage = (int)pPager->dbSize;
}


/*
** Try to obtain a lock of type locktype on the database file. If
** a similar or greater lock is already held, this function is a no-op
** (returning SQLITE_OK immediately).
**
** Otherwise, attempt to obtain the lock using sqlite3OsLock(). Invoke 
** the busy callback if the lock is currently not available. Repeat 
** until the busy callback returns false or until the attempt to 
** obtain the lock succeeds.
**
** Return SQLITE_OK on success and an error code if we cannot obtain
** the lock. If the lock is obtained successfully, set the Pager.state 
** variable to locktype before returning.
*/
static int pager_wait_on_lock(Pager *pPager, int locktype){
  int rc;                              /* Return code */

  /* Check that this is either a no-op (because the requested lock is 
  ** already held, or one of the transistions that the busy-handler
  ** may be invoked during, according to the comment above
  ** sqlite3PagerSetBusyhandler().
  */
  assert( (pPager->eLock>=locktype)
       || (pPager->eLock==NO_LOCK && locktype==SHARED_LOCK)
       || (pPager->eLock==RESERVED_LOCK && locktype==EXCLUSIVE_LOCK)
  );

  do {
    rc = pagerLockDb(pPager, locktype);
  }while( rc==SQLITE_BUSY && pPager->xBusyHandler(pPager->pBusyHandlerArg) );
  return rc;
}

/*
** Function assertTruncateConstraint(pPager) checks that one of the 
** following is true for all dirty pages currently in the page-cache:
**
**   a) The page number is less than or equal to the size of the 
**      current database image, in pages, OR
**
**   b) if the page content were written at this time, it would not
**      be necessary to write the current content out to the sub-journal
**      (as determined by function subjRequiresPage()).
**
** If the condition asserted by this function were not true, and the
** dirty page were to be discarded from the cache via the pagerStress()
** routine, pagerStress() would not write the current page content to
** the database file. If a savepoint transaction were rolled back after
** this happened, the correct behaviour would be to restore the current
** content of the page. However, since this content is not present in either
** the database file or the portion of the rollback journal and 
** sub-journal rolled back the content could not be restored and the
** database image would become corrupt. It is therefore fortunate that 
** this circumstance cannot arise.
*/
#if defined(SQLITE_DEBUG)
static void assertTruncateConstraintCb(PgHdr *pPg){
  assert( pPg->flags&PGHDR_DIRTY );
  assert( !subjRequiresPage(pPg) || pPg->pgno<=pPg->pPager->dbSize );
}
static void assertTruncateConstraint(Pager *pPager){
  sqlite3PcacheIterateDirty(pPager->pPCache, assertTruncateConstraintCb);
}
#else
# define assertTruncateConstraint(pPager)
#endif

/*
** Truncate the in-memory database file image to nPage pages. This 
** function does not actually modify the database file on disk. It 
** just sets the internal state of the pager object so that the 
** truncation will be done when the current transaction is committed.
*/
void sqlite3PagerTruncateImage(Pager *pPager, Pgno nPage){
  assert( pPager->dbSize>=nPage );
  assert( pPager->eState>=PAGER_WRITER_CACHEMOD );
  pPager->dbSize = nPage;
  assertTruncateConstraint(pPager);
}


/*
** This function is called before attempting a hot-journal rollback. It
** syncs the journal file to disk, then sets pPager->journalHdr to the
** size of the journal file so that the pager_playback() routine knows
** that the entire journal file has been synced.
**
** Syncing a hot-journal to disk before attempting to roll it back ensures 
** that if a power-failure occurs during the rollback, the process that
** attempts rollback following system recovery sees the same journal
** content as this process.
**
** If everything goes as planned, SQLITE_OK is returned. Otherwise, 
** an SQLite error code.
*/
static int pagerSyncHotJournal(Pager *pPager){
  int rc = SQLITE_OK;
  if( !pPager->noSync ){
    rc = sqlite3OsSync(pPager->jfd, SQLITE_SYNC_NORMAL);
  }
  if( rc==SQLITE_OK ){
    rc = sqlite3OsFileSize(pPager->jfd, &pPager->journalHdr);
  }
  return rc;
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

#if !defined(NDEBUG) || defined(SQLITE_TEST)
/*
** Return the page number for page pPg.
*/
Pgno sqlite3PagerPagenumber(DbPage *pPg){
  return pPg->pgno;
}
#endif

/*
** Increment the reference count for page pPg.
*/
void sqlite3PagerRef(DbPage *pPg){
  sqlite3PcacheRef(pPg);
}

/*
** Sync the journal. In other words, make sure all the pages that have
** been written to the journal have actually reached the surface of the
** disk and can be restored in the event of a hot-journal rollback.
**
** If the Pager.noSync flag is set, then this function is a no-op.
** Otherwise, the actions required depend on the journal-mode and the 
** device characteristics of the file-system, as follows:
**
**   * If the journal file is an in-memory journal file, no action need
**     be taken.
**
**   * Otherwise, if the device does not support the SAFE_APPEND property,
**     then the nRec field of the most recently written journal header
**     is updated to contain the number of journal records that have
**     been written following it. If the pager is operating in full-sync
**     mode, then the journal file is synced before this field is updated.
**
**   * If the device does not support the SEQUENTIAL property, then 
**     journal file is synced.
**
** Or, in pseudo-code:
**
**   if( NOT <in-memory journal> ){
**     if( NOT SAFE_APPEND ){
**       if( <full-sync mode> ) xSync(<journal file>);
**       <update nRec field>
**     } 
**     if( NOT SEQUENTIAL ) xSync(<journal file>);
**   }
**
** If successful, this routine clears the PGHDR_NEED_SYNC flag of every 
** page currently held in memory before returning SQLITE_OK. If an IO
** error is encountered, then the IO error code is returned to the caller.
*/
static int syncJournal(Pager *pPager, int newHdr){
  int rc;                         /* Return code */

  assert( pPager->eState==PAGER_WRITER_CACHEMOD
       || pPager->eState==PAGER_WRITER_DBMOD
  );
  assert( assert_pager_state(pPager) );
  assert( !pagerUseWal(pPager) );

  rc = sqlite3PagerExclusiveLock(pPager);
  if( rc!=SQLITE_OK ) return rc;

  if( !pPager->noSync ){
    assert( !pPager->tempFile );
    if( isOpen(pPager->jfd) && pPager->journalMode!=PAGER_JOURNALMODE_MEMORY ){
      const int iDc = sqlite3OsDeviceCharacteristics(pPager->fd);
      assert( isOpen(pPager->jfd) );

      if( 0==(iDc&SQLITE_IOCAP_SAFE_APPEND) ){
        /* This block deals with an obscure problem. If the last connection
        ** that wrote to this database was operating in persistent-journal
        ** mode, then the journal file may at this point actually be larger
        ** than Pager.journalOff bytes. If the next thing in the journal
        ** file happens to be a journal-header (written as part of the
        ** previous connection's transaction), and a crash or power-failure 
        ** occurs after nRec is updated but before this connection writes 
        ** anything else to the journal file (or commits/rolls back its 
        ** transaction), then SQLite may become confused when doing the 
        ** hot-journal rollback following recovery. It may roll back all
        ** of this connections data, then proceed to rolling back the old,
        ** out-of-date data that follows it. Database corruption.
        **
        ** To work around this, if the journal file does appear to contain
        ** a valid header following Pager.journalOff, then write a 0x00
        ** byte to the start of it to prevent it from being recognized.
        **
        ** Variable iNextHdrOffset is set to the offset at which this
        ** problematic header will occur, if it exists. aMagic is used 
        ** as a temporary buffer to inspect the first couple of bytes of
        ** the potential journal header.
        */
        i64 iNextHdrOffset;
        u8 aMagic[8];
        u8 zHeader[sizeof(aJournalMagic)+4];

        memcpy(zHeader, aJournalMagic, sizeof(aJournalMagic));
        put32bits(&zHeader[sizeof(aJournalMagic)], pPager->nRec);

        iNextHdrOffset = journalHdrOffset(pPager);
        rc = sqlite3OsRead(pPager->jfd, aMagic, 8, iNextHdrOffset);
        if( rc==SQLITE_OK && 0==memcmp(aMagic, aJournalMagic, 8) ){
          static const u8 zerobyte = 0;
          rc = sqlite3OsWrite(pPager->jfd, &zerobyte, 1, iNextHdrOffset);
        }
        if( rc!=SQLITE_OK && rc!=SQLITE_IOERR_SHORT_READ ){
          return rc;
        }

        /* Write the nRec value into the journal file header. If in
        ** full-synchronous mode, sync the journal first. This ensures that
        ** all data has really hit the disk before nRec is updated to mark
        ** it as a candidate for rollback.
        **
        ** This is not required if the persistent media supports the
        ** SAFE_APPEND property. Because in this case it is not possible 
        ** for garbage data to be appended to the file, the nRec field
        ** is populated with 0xFFFFFFFF when the journal header is written
        ** and never needs to be updated.
        */
        if( pPager->fullSync && 0==(iDc&SQLITE_IOCAP_SEQUENTIAL) ){
          PAGERTRACE(("SYNC journal of %d\n", PAGERID(pPager)));
          IOTRACE(("JSYNC %p\n", pPager))
          rc = sqlite3OsSync(pPager->jfd, pPager->syncFlags);
          if( rc!=SQLITE_OK ) return rc;
        }
        IOTRACE(("JHDR %p %lld\n", pPager, pPager->journalHdr));
        rc = sqlite3OsWrite(
            pPager->jfd, zHeader, sizeof(zHeader), pPager->journalHdr
        );
        if( rc!=SQLITE_OK ) return rc;
      }
      if( 0==(iDc&SQLITE_IOCAP_SEQUENTIAL) ){
        PAGERTRACE(("SYNC journal of %d\n", PAGERID(pPager)));
        IOTRACE(("JSYNC %p\n", pPager))
        rc = sqlite3OsSync(pPager->jfd, pPager->syncFlags| 
          (pPager->syncFlags==SQLITE_SYNC_FULL?SQLITE_SYNC_DATAONLY:0)
        );
        if( rc!=SQLITE_OK ) return rc;
      }

      pPager->journalHdr = pPager->journalOff;
      if( newHdr && 0==(iDc&SQLITE_IOCAP_SAFE_APPEND) ){
        pPager->nRec = 0;
        rc = writeJournalHdr(pPager);
        if( rc!=SQLITE_OK ) return rc;
      }
    }else{
      pPager->journalHdr = pPager->journalOff;
    }
  }

  /* Unless the pager is in noSync mode, the journal file was just 
  ** successfully synced. Either way, clear the PGHDR_NEED_SYNC flag on 
  ** all pages.
  */
  sqlite3PcacheClearSyncFlags(pPager->pPCache);
  pPager->eState = PAGER_WRITER_DBMOD;
  assert( assert_pager_state(pPager) );
  return SQLITE_OK;
}

/*
** The argument is the first in a linked list of dirty pages connected
** by the PgHdr.pDirty pointer. This function writes each one of the
** in-memory pages in the list to the database file. The argument may
** be NULL, representing an empty list. In this case this function is
** a no-op.
**
** The pager must hold at least a RESERVED lock when this function
** is called. Before writing anything to the database file, this lock
** is upgraded to an EXCLUSIVE lock. If the lock cannot be obtained,
** SQLITE_BUSY is returned and no data is written to the database file.
** 
** If the pager is a temp-file pager and the actual file-system file
** is not yet open, it is created and opened before any data is 
** written out.
**
** Once the lock has been upgraded and, if necessary, the file opened,
** the pages are written out to the database file in list order. Writing
** a page is skipped if it meets either of the following criteria:
**
**   * The page number is greater than Pager.dbSize, or
**   * The PGHDR_DONT_WRITE flag is set on the page.
**
** If writing out a page causes the database file to grow, Pager.dbFileSize
** is updated accordingly. If page 1 is written out, then the value cached
** in Pager.dbFileVers[] is updated to match the new value stored in
** the database file.
**
** If everything is successful, SQLITE_OK is returned. If an IO error 
** occurs, an IO error code is returned. Or, if the EXCLUSIVE lock cannot
** be obtained, SQLITE_BUSY is returned.
*/
static int pager_write_pagelist(Pager *pPager, PgHdr *pList){
  int rc = SQLITE_OK;                  /* Return code */

  /* This function is only called for rollback pagers in WRITER_DBMOD state. */
  assert( !pagerUseWal(pPager) );
  assert( pPager->eState==PAGER_WRITER_DBMOD );
  assert( pPager->eLock==EXCLUSIVE_LOCK );

  /* If the file is a temp-file has not yet been opened, open it now. It
  ** is not possible for rc to be other than SQLITE_OK if this branch
  ** is taken, as pager_wait_on_lock() is a no-op for temp-files.
  */
  if( !isOpen(pPager->fd) ){
    assert( pPager->tempFile && rc==SQLITE_OK );
    rc = pagerOpentemp(pPager, pPager->fd, pPager->vfsFlags);
  }

  /* Before the first write, give the VFS a hint of what the final
  ** file size will be.
  */
  assert( rc!=SQLITE_OK || isOpen(pPager->fd) );
  if( rc==SQLITE_OK && pPager->dbSize>pPager->dbHintSize ){
    sqlite3_int64 szFile = pPager->pageSize * (sqlite3_int64)pPager->dbSize;
    sqlite3OsFileControlHint(pPager->fd, SQLITE_FCNTL_SIZE_HINT, &szFile);
    pPager->dbHintSize = pPager->dbSize;
  }

  while( rc==SQLITE_OK && pList ){
    Pgno pgno = pList->pgno;

    /* If there are dirty pages in the page cache with page numbers greater
    ** than Pager.dbSize, this means sqlite3PagerTruncateImage() was called to
    ** make the file smaller (presumably by auto-vacuum code). Do not write
    ** any such pages to the file.
    **
    ** Also, do not write out any page that has the PGHDR_DONT_WRITE flag
    ** set (set by sqlite3PagerDontWrite()).
    */
    if( pgno<=pPager->dbSize && 0==(pList->flags&PGHDR_DONT_WRITE) ){
      i64 offset = (pgno-1)*(i64)pPager->pageSize;   /* Offset to write */
      char *pData;                                   /* Data to write */    

      assert( (pList->flags&PGHDR_NEED_SYNC)==0 );
      if( pList->pgno==1 ) pager_write_changecounter(pList);

      /* Encode the database */
      CODEC2(pPager, pList->pData, pgno, 6, return SQLITE_NOMEM, pData);

      /* Write out the page data. */
      rc = sqlite3OsWrite(pPager->fd, pData, pPager->pageSize, offset);

      /* If page 1 was just written, update Pager.dbFileVers to match
      ** the value now stored in the database file. If writing this 
      ** page caused the database file to grow, update dbFileSize. 
      */
      if( pgno==1 ){
        memcpy(&pPager->dbFileVers, &pData[24], sizeof(pPager->dbFileVers));
      }
      if( pgno>pPager->dbFileSize ){
        pPager->dbFileSize = pgno;
      }
      pPager->aStat[PAGER_STAT_WRITE]++;

      /* Update any backup objects copying the contents of this pager. */
      sqlite3BackupUpdate(pPager->pBackup, pgno, (u8*)pList->pData);

      PAGERTRACE(("STORE %d page %d hash(%08x)\n",
                   PAGERID(pPager), pgno, pager_pagehash(pList)));
      IOTRACE(("PGOUT %p %d\n", pPager, pgno));
      PAGER_INCR(sqlite3_pager_writedb_count);
    }else{
      PAGERTRACE(("NOSTORE %d page %d\n", PAGERID(pPager), pgno));
    }
    pager_set_pagehash(pList);
    pList = pList->pDirty;
  }

  return rc;
}

/*
** Ensure that the sub-journal file is open. If it is already open, this 
** function is a no-op.
**
** SQLITE_OK is returned if everything goes according to plan. An 
** SQLITE_IOERR_XXX error code is returned if a call to sqlite3OsOpen() 
** fails.
*/
static int openSubJournal(Pager *pPager){
  int rc = SQLITE_OK;
  if( !isOpen(pPager->sjfd) ){
    if( pPager->journalMode==PAGER_JOURNALMODE_MEMORY || pPager->subjInMemory ){
      sqlite3MemJournalOpen(pPager->sjfd);
    }else{
      rc = pagerOpentemp(pPager, pPager->sjfd, SQLITE_OPEN_SUBJOURNAL);
    }
  }
  return rc;
}

/*
** Append a record of the current state of page pPg to the sub-journal. 
** It is the callers responsibility to use subjRequiresPage() to check 
** that it is really required before calling this function.
**
** If successful, set the bit corresponding to pPg->pgno in the bitvecs
** for all open savepoints before returning.
**
** This function returns SQLITE_OK if everything is successful, an IO
** error code if the attempt to write to the sub-journal fails, or 
** SQLITE_NOMEM if a malloc fails while setting a bit in a savepoint
** bitvec.
*/
static int subjournalPage(PgHdr *pPg){
  int rc = SQLITE_OK;
  Pager *pPager = pPg->pPager;
  if( pPager->journalMode!=PAGER_JOURNALMODE_OFF ){

    /* Open the sub-journal, if it has not already been opened */
    assert( pPager->useJournal );
    assert( isOpen(pPager->jfd) || pagerUseWal(pPager) );
    assert( isOpen(pPager->sjfd) || pPager->nSubRec==0 );
    assert( pagerUseWal(pPager) 
         || pageInJournal(pPg) 
         || pPg->pgno>pPager->dbOrigSize 
    );
    rc = openSubJournal(pPager);

    /* If the sub-journal was opened successfully (or was already open),
    ** write the journal record into the file.  */
    if( rc==SQLITE_OK ){
      void *pData = pPg->pData;
      i64 offset = (i64)pPager->nSubRec*(4+pPager->pageSize);
      char *pData2;
  
      CODEC2(pPager, pData, pPg->pgno, 7, return SQLITE_NOMEM, pData2);
      PAGERTRACE(("STMT-JOURNAL %d page %d\n", PAGERID(pPager), pPg->pgno));
      rc = write32bits(pPager->sjfd, offset, pPg->pgno);
      if( rc==SQLITE_OK ){
        rc = sqlite3OsWrite(pPager->sjfd, pData2, pPager->pageSize, offset+4);
      }
    }
  }
  if( rc==SQLITE_OK ){
    pPager->nSubRec++;
    assert( pPager->nSavepoint>0 );
    rc = addToSavepointBitvecs(pPager, pPg->pgno);
  }
  return rc;
}

/*
** This function is called by the pcache layer when it has reached some
** soft memory limit. The first argument is a pointer to a Pager object
** (cast as a void*). The pager is always 'purgeable' (not an in-memory
** database). The second argument is a reference to a page that is 
** currently dirty but has no outstanding references. The page
** is always associated with the Pager object passed as the first 
** argument.
**
** The job of this function is to make pPg clean by writing its contents
** out to the database file, if possible. This may involve syncing the
** journal file. 
**
** If successful, sqlite3PcacheMakeClean() is called on the page and
** SQLITE_OK returned. If an IO error occurs while trying to make the
** page clean, the IO error code is returned. If the page cannot be
** made clean for some other reason, but no error occurs, then SQLITE_OK
** is returned by sqlite3PcacheMakeClean() is not called.
*/
static int pagerStress(void *p, PgHdr *pPg){
  Pager *pPager = (Pager *)p;
  int rc = SQLITE_OK;

  assert( pPg->pPager==pPager );
  assert( pPg->flags&PGHDR_DIRTY );

  /* The doNotSyncSpill flag is set during times when doing a sync of
  ** journal (and adding a new header) is not allowed.  This occurs
  ** during calls to sqlite3PagerWrite() while trying to journal multiple
  ** pages belonging to the same sector.
  **
  ** The doNotSpill flag inhibits all cache spilling regardless of whether
  ** or not a sync is required.  This is set during a rollback.
  **
  ** Spilling is also prohibited when in an error state since that could
  ** lead to database corruption.   In the current implementaton it 
  ** is impossible for sqlite3PcacheFetch() to be called with createFlag==1
  ** while in the error state, hence it is impossible for this routine to
  ** be called in the error state.  Nevertheless, we include a NEVER()
  ** test for the error state as a safeguard against future changes.
  */
  if( NEVER(pPager->errCode) ) return SQLITE_OK;
  if( pPager->doNotSpill ) return SQLITE_OK;
  if( pPager->doNotSyncSpill && (pPg->flags & PGHDR_NEED_SYNC)!=0 ){
    return SQLITE_OK;
  }

  pPg->pDirty = 0;
  if( pagerUseWal(pPager) ){
    /* Write a single frame for this page to the log. */
    if( subjRequiresPage(pPg) ){ 
      rc = subjournalPage(pPg); 
    }
    if( rc==SQLITE_OK ){
      rc = pagerWalFrames(pPager, pPg, 0, 0);
    }
  }else{
  
    /* Sync the journal file if required. */
    if( pPg->flags&PGHDR_NEED_SYNC 
     || pPager->eState==PAGER_WRITER_CACHEMOD
    ){
      rc = syncJournal(pPager, 1);
    }
  
    /* If the page number of this page is larger than the current size of
    ** the database image, it may need to be written to the sub-journal.
    ** This is because the call to pager_write_pagelist() below will not
    ** actually write data to the file in this case.
    **
    ** Consider the following sequence of events:
    **
    **   BEGIN;
    **     <journal page X>
    **     <modify page X>
    **     SAVEPOINT sp;
    **       <shrink database file to Y pages>
    **       pagerStress(page X)
    **     ROLLBACK TO sp;
    **
    ** If (X>Y), then when pagerStress is called page X will not be written
    ** out to the database file, but will be dropped from the cache. Then,
    ** following the "ROLLBACK TO sp" statement, reading page X will read
    ** data from the database file. This will be the copy of page X as it
    ** was when the transaction started, not as it was when "SAVEPOINT sp"
    ** was executed.
    **
    ** The solution is to write the current data for page X into the 
    ** sub-journal file now (if it is not already there), so that it will
    ** be restored to its current value when the "ROLLBACK TO sp" is 
    ** executed.
    */
    if( NEVER(
        rc==SQLITE_OK && pPg->pgno>pPager->dbSize && subjRequiresPage(pPg)
    ) ){
      rc = subjournalPage(pPg);
    }
  
    /* Write the contents of the page out to the database file. */
    if( rc==SQLITE_OK ){
      assert( (pPg->flags&PGHDR_NEED_SYNC)==0 );
      rc = pager_write_pagelist(pPager, pPg);
    }
  }

  /* Mark the page as clean. */
  if( rc==SQLITE_OK ){
    PAGERTRACE(("STRESS %d page %d\n", PAGERID(pPager), pPg->pgno));
    sqlite3PcacheMakeClean(pPg);
  }

  return pager_error(pPager, rc); 
}


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
int sqlite3PagerOpen(
  sqlite3_vfs *pVfs,       /* The virtual file system to use */
  Pager **ppPager,         /* OUT: Return the Pager structure here */
  const char *zFilename,   /* Name of the database file to open */
  int nExtra,              /* Extra bytes append to each in-memory page */
  int flags,               /* flags controlling this file */
  int vfsFlags,            /* flags passed through to sqlite3_vfs.xOpen() */
  void (*xReinit)(DbPage*) /* Function to reinitialize pages */
){
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

  /* Figure out how much space is required for each journal file-handle
  ** (there are two of them, the main journal and the sub-journal). This
  ** is the maximum space required for an in-memory journal file handle 
  ** and a regular journal file-handle. Note that a "regular journal-handle"
  ** may be a wrapper capable of caching the first portion of the journal
  ** file in memory to implement the atomic-write optimization (see 
  ** source file journal.c).
  */
  if( sqlite3JournalSize(pVfs)>sqlite3MemJournalSize() ){
    journalFileSize = ROUND8(sqlite3JournalSize(pVfs));
  }else{
    journalFileSize = ROUND8(sqlite3MemJournalSize());
  }

  /* Set the output variable to NULL in case an error occurs. */
  *ppPager = 0;

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

  /* Compute and store the full pathname in an allocated buffer pointed
  ** to by zPathname, length nPathname. Or, if this is a temporary file,
  ** leave both nPathname and zPathname set to 0.
  */
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
  pPager =              (Pager*)(pPtr);
  pPager->pPCache =    (PCache*)(pPtr += ROUND8(sizeof(*pPager)));
  pPager->fd =   (sqlite3_file*)(pPtr += ROUND8(pcacheSize));
  pPager->sjfd = (sqlite3_file*)(pPtr += ROUND8(pVfs->szOsFile));
  pPager->jfd =  (sqlite3_file*)(pPtr += journalFileSize);
  pPager->zFilename =    (char*)(pPtr += journalFileSize);
  assert( EIGHT_BYTE_ALIGNMENT(pPager->jfd) );

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

  /* Open the pager file.
  */
  if( zFilename && zFilename[0] ){
    int fout = 0;                    /* VFS flags returned by xOpen() */
    rc = sqlite3OsOpen(pVfs, pPager->zFilename, pPager->fd, vfsFlags, &fout);
    assert( !memDb );
    readOnly = (fout&SQLITE_OPEN_READONLY);

    /* If the file was successfully opened for read/write access,
    ** choose a default page size in case we have to create the
    ** database file. The default page size is the maximum of:
    **
    **    + SQLITE_DEFAULT_PAGE_SIZE,
    **    + The value returned by sqlite3OsSectorSize()
    **    + The largest page size that can be written atomically.
    */
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
  }else{
    /* If a temporary file is requested, it is not opened immediately.
    ** In this case we accept the default page size and delay actually
    ** opening the file until the first call to OsWrite().
    **
    ** This branch is also run for an in-memory database. An in-memory
    ** database is the same as a temp-file that is never written out to
    ** disk and uses an in-memory rollback journal.
    */ 
    tempFile = 1;
    pPager->eState = PAGER_READER;
    pPager->eLock = EXCLUSIVE_LOCK;
    readOnly = (vfsFlags&SQLITE_OPEN_READONLY);
  }

  /* The following call to PagerSetPagesize() serves to set the value of 
  ** Pager.pageSize and to allocate the Pager.pTmpSpace buffer.
  */
  if( rc==SQLITE_OK ){
    assert( pPager->memDb==0 );
    rc = sqlite3PagerSetPagesize(pPager, &szPageDflt, -1);
    testcase( rc!=SQLITE_OK );
  }

  /* If an error occurred in either of the blocks above, free the 
  ** Pager structure and close the file.
  */
  if( rc!=SQLITE_OK ){
    assert( !pPager->pTmpSpace );
    sqlite3OsClose(pPager->fd);
    sqlite3_free(pPager);
    return rc;
  }

  /* Initialize the PCache object. */
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
** This function is called after transitioning from PAGER_UNLOCK to
** PAGER_SHARED state. It tests if there is a hot journal present in
** the file-system for the given pager. A hot journal is one that 
** needs to be played back. According to this function, a hot-journal
** file exists if the following criteria are met:
**
**   * The journal file exists in the file system, and
**   * No process holds a RESERVED or greater lock on the database file, and
**   * The database file itself is greater than 0 bytes in size, and
**   * The first byte of the journal file exists and is not 0x00.
**
** If the current size of the database file is 0 but a journal file
** exists, that is probably an old journal left over from a prior
** database with the same name. In this case the journal file is
** just deleted using OsDelete, *pExists is set to 0 and SQLITE_OK
** is returned.
**
** This routine does not check if there is a master journal filename
** at the end of the file. If there is, and that master journal file
** does not exist, then the journal file is not really hot. In this
** case this routine will return a false-positive. The pager_playback()
** routine will discover that the journal file is not really hot and 
** will not roll it back. 
**
** If a hot-journal file is found to exist, *pExists is set to 1 and 
** SQLITE_OK returned. If no hot-journal file is present, *pExists is
** set to 0 and SQLITE_OK returned. If an IO error occurs while trying
** to determine whether or not a hot-journal file exists, the IO error
** code is returned and the value of *pExists is undefined.
*/
static int hasHotJournal(Pager *pPager, int *pExists){
  sqlite3_vfs * const pVfs = pPager->pVfs;
  int rc = SQLITE_OK;           /* Return code */
  int exists = 1;               /* True if a journal file is present */
  int jrnlOpen = !!isOpen(pPager->jfd);

  assert( pPager->useJournal );
  assert( isOpen(pPager->fd) );
  assert( pPager->eState==PAGER_OPEN );

  assert( jrnlOpen==0 || ( sqlite3OsDeviceCharacteristics(pPager->jfd) &
    SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN
  ));

  *pExists = 0;
  if( !jrnlOpen ){
    rc = sqlite3OsAccess(pVfs, pPager->zJournal, SQLITE_ACCESS_EXISTS, &exists);
  }
  if( rc==SQLITE_OK && exists ){
    int locked = 0;             /* True if some process holds a RESERVED lock */

    /* Race condition here:  Another process might have been holding the
    ** the RESERVED lock and have a journal open at the sqlite3OsAccess() 
    ** call above, but then delete the journal and drop the lock before
    ** we get to the following sqlite3OsCheckReservedLock() call.  If that
    ** is the case, this routine might think there is a hot journal when
    ** in fact there is none.  This results in a false-positive which will
    ** be dealt with by the playback routine.  Ticket #3883.
    */
    rc = sqlite3OsCheckReservedLock(pPager->fd, &locked);
    if( rc==SQLITE_OK && !locked ){
      Pgno nPage;                 /* Number of pages in database file */

      /* Check the size of the database file. If it consists of 0 pages,
      ** then delete the journal file. See the header comment above for 
      ** the reasoning here.  Delete the obsolete journal file under
      ** a RESERVED lock to avoid race conditions and to avoid violating
      ** [H33020].
      */
      rc = pagerPagecount(pPager, &nPage);
      if( rc==SQLITE_OK ){
        if( nPage==0 ){
          sqlite3BeginBenignMalloc();
          if( pagerLockDb(pPager, RESERVED_LOCK)==SQLITE_OK ){
            sqlite3OsDelete(pVfs, pPager->zJournal, 0);
            if( !pPager->exclusiveMode ) pagerUnlockDb(pPager, SHARED_LOCK);
          }
          sqlite3EndBenignMalloc();
        }else{
          /* The journal file exists and no other connection has a reserved
          ** or greater lock on the database file. Now check that there is
          ** at least one non-zero bytes at the start of the journal file.
          ** If there is, then we consider this journal to be hot. If not, 
          ** it can be ignored.
          */
          if( !jrnlOpen ){
            int f = SQLITE_OPEN_READONLY|SQLITE_OPEN_MAIN_JOURNAL;
            rc = sqlite3OsOpen(pVfs, pPager->zJournal, pPager->jfd, f, &f);
          }
          if( rc==SQLITE_OK ){
            u8 first = 0;
            rc = sqlite3OsRead(pPager->jfd, (void *)&first, 1, 0);
            if( rc==SQLITE_IOERR_SHORT_READ ){
              rc = SQLITE_OK;
            }
            if( !jrnlOpen ){
              sqlite3OsClose(pPager->jfd);
            }
            *pExists = (first!=0);
          }else if( rc==SQLITE_CANTOPEN ){
            /* If we cannot open the rollback journal file in order to see if
            ** its has a zero header, that might be due to an I/O error, or
            ** it might be due to the race condition described above and in
            ** ticket #3883.  Either way, assume that the journal is hot.
            ** This might be a false positive.  But if it is, then the
            ** automatic journal playback and recovery mechanism will deal
            ** with it under an EXCLUSIVE lock where we do not need to
            ** worry so much with race conditions.
            */
            *pExists = 1;
            rc = SQLITE_OK;
          }
        }
      }
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
** If the reference count has reached zero, rollback any active
** transaction and unlock the pager.
**
** Except, in locking_mode=EXCLUSIVE when there is nothing to in
** the rollback journal, the unlock is not performed and there is
** nothing to rollback, so this routine is a no-op.
*/ 
static void pagerUnlockIfUnused(Pager *pPager){
  if( (sqlite3PcacheRefCount(pPager->pPCache)==0) ){
    pagerUnlockAndRollback(pPager);
  }
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
** Release a page reference.
**
** If the number of references to the page drop to zero, then the
** page is added to the LRU list.  When all references to all pages
** are released, a rollback occurs and the lock on the database is
** removed.
*/
void sqlite3PagerUnref(DbPage *pPg){
  if( pPg ){
    Pager *pPager = pPg->pPager;
    sqlite3PcacheRelease(pPg);
    pagerUnlockIfUnused(pPager);
  }
}

/*
** This function is called at the start of every write transaction.
** There must already be a RESERVED or EXCLUSIVE lock on the database 
** file when this routine is called.
**
** Open the journal file for pager pPager and write a journal header
** to the start of it. If there are active savepoints, open the sub-journal
** as well. This function is only used when the journal file is being 
** opened to write a rollback log for a transaction. It is not used 
** when opening a hot journal file to roll it back.
**
** If the journal file is already open (as it may be in exclusive mode),
** then this function just writes a journal header to the start of the
** already open file. 
**
** Whether or not the journal file is opened by this function, the
** Pager.pInJournal bitvec structure is allocated.
**
** Return SQLITE_OK if everything is successful. Otherwise, return 
** SQLITE_NOMEM if the attempt to allocate Pager.pInJournal fails, or 
** an IO error code if opening or writing the journal file fails.
*/
static int pager_open_journal(Pager *pPager){
  int rc = SQLITE_OK;                        /* Return code */
  sqlite3_vfs * const pVfs = pPager->pVfs;   /* Local cache of vfs pointer */

  assert( pPager->eState==PAGER_WRITER_LOCKED );
  assert( assert_pager_state(pPager) );
  assert( pPager->pInJournal==0 );
  
  /* If already in the error state, this function is a no-op.  But on
  ** the other hand, this routine is never called if we are already in
  ** an error state. */
  if( NEVER(pPager->errCode) ) return pPager->errCode;

  if( !pagerUseWal(pPager) && pPager->journalMode!=PAGER_JOURNALMODE_OFF ){
    pPager->pInJournal = sqlite3BitvecCreate(pPager->dbSize);
    if( pPager->pInJournal==0 ){
      return SQLITE_NOMEM;
    }
  
    /* Open the journal file if it is not already open. */
    if( !isOpen(pPager->jfd) ){
      if( pPager->journalMode==PAGER_JOURNALMODE_MEMORY ){
        sqlite3MemJournalOpen(pPager->jfd);
      }else{
        const int flags =                   /* VFS flags to open journal file */
          SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|
          (pPager->tempFile ? 
            (SQLITE_OPEN_DELETEONCLOSE|SQLITE_OPEN_TEMP_JOURNAL):
            (SQLITE_OPEN_MAIN_JOURNAL)
          );
  #ifdef SQLITE_ENABLE_ATOMIC_WRITE
        rc = sqlite3JournalOpen(
            pVfs, pPager->zJournal, pPager->jfd, flags, jrnlBufferSize(pPager)
        );
  #else
        rc = sqlite3OsOpen(pVfs, pPager->zJournal, pPager->jfd, flags, 0);
  #endif
      }
      assert( rc!=SQLITE_OK || isOpen(pPager->jfd) );
    }
  
  
    /* Write the first journal header to the journal file and open 
    ** the sub-journal if necessary.
    */
    if( rc==SQLITE_OK ){
      /* TODO: Check if all of these are really required. */
      pPager->nRec = 0;
      pPager->journalOff = 0;
      pPager->setMaster = 0;
      pPager->journalHdr = 0;
      rc = writeJournalHdr(pPager);
    }
  }

  if( rc!=SQLITE_OK ){
    sqlite3BitvecDestroy(pPager->pInJournal);
    pPager->pInJournal = 0;
  }else{
    assert( pPager->eState==PAGER_WRITER_LOCKED );
    pPager->eState = PAGER_WRITER_CACHEMOD;
  }

  return rc;
}

/*
** Begin a write-transaction on the specified pager object. If a 
** write-transaction has already been opened, this function is a no-op.
** 在指定pager对象上开始一个写事务。如果写事务已经被打开，那么这个功能就是个空操作。
** If the exFlag argument is false, then acquire at least a RESERVED
** lock on the database file. If exFlag is true, then acquire at least
** an EXCLUSIVE lock. If such a lock is already held, no locking 
** functions need be called.
   如果参数exFlag是错误的，那么数据库文件中至少需要一个保留锁。如果参数exFlag是正确的，那么它至少需要一个排他锁。
   如果这样的锁已经被保留，那么不需要调用锁函数。
** If the subjInMemory argument is non-zero, then any sub-journal opened
** within this transaction will be opened as an in-memory file. This
** has no effect if the sub-journal is already opened (as it may be when
** running in exclusive mode) or if the transaction does not require a
** sub-journal. If the subjInMemory argument is zero, then any required
** sub-journal is implemented in-memory if pPager is an in-memory database, 
** or using a temporary file otherwise.
   如果参数subInMemory不为零，那么任何子日志在此事务中会以内存文件打开。如果子日志已经被打开（因为它可能在独占模式下运行）
   或者此事务不需要子日志，那就没有影响了。如果参数subjInMemory为零，pPager是一个内存数据库或者另外使用一个临时文件，
   那么在内存中实现所有必要的子日志文件。
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
        如果pager配置为使用排它锁模式，并且排它锁在数据库中没有被保留，那么立即获取它。
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
         抓取日志文件写锁。如果抓取成功，升级到PAGER_RESERVED状态。否则，给调用者返回一个错误代码。
         如果另外一个连接已经保留写锁，那么busy-handler不被传唤。如果可能的话，上层将调用它。
      */
      rc = sqlite3WalBeginWriteTransaction(pPager->pWal);
    }else{
      /* Obtain a RESERVED lock on the database file. If the exFlag parameter
      ** is true, then immediately upgrade this to an EXCLUSIVE lock. The
      ** busy-handler callback can be used when upgrading to the EXCLUSIVE
      ** lock, but not when obtaining the RESERVED lock.
         在数据库文件中获取保留锁。如果参数exFlag为真值，那么立即升级为排它锁。
         当升级到排它锁而不是获得保留锁时，它会使用busy-handler回调。
      */
      rc = pagerLockDb(pPager, RESERVED_LOCK);
      if( rc==SQLITE_OK && exFlag ){
        rc = pager_wait_on_lock(pPager, EXCLUSIVE_LOCK);
      }
    }

    if( rc==SQLITE_OK ){
      /* Change to WRITER_LOCKED state.
      ** 更改为WRITER_LOCKED状态。
      ** WAL mode sets Pager.eState to PAGER_WRITER_LOCKED or CACHEMOD
      ** when it has an open transaction, but never to DBMOD or FINISHED.
      ** This is because in those states the code to roll back savepoint 
      ** transactions may copy data from the sub-journal into the database 
      ** file as well as into the page cache. Which would be incorrect in 
      ** WAL mode.
         当打开一个事务，通过预写式日志方式把Pager.eState设置成PAGER_WRITER_LOCKED or CACHEMOD，
         但是不要设置成DBMOD或者FINISHED。这是因为在这些状态中，回滚保存点事务代码会将子日志数据复制到数据库文件以及页面缓存。
         在WAL模式下，这是不正确的。
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
** Mark a single data page as writeable. The page is written into the 
** main journal or sub-journal as required. If the page is written into
** one of the journals, the corresponding bit is set in the 
** Pager.pInJournal bitvec and the PagerSavepoint.pInSavepoint bitvecs
** of any open savepoints as appropriate.
   把一个数据页标记为可写。按照要求，把这页写入主日志或者子日志。如果页面被写入其中一个日志，在Pager设置相应位。pInJournal bitvec 和PagerSavepoint。
   在所有打开保存点中，pInSavepoint bitvecs是适当保存点。
*/
static int pager_write(PgHdr *pPg){
  void *pData = pPg->pData;
  Pager *pPager = pPg->pPager;
  int rc = SQLITE_OK;

  /* This routine is not called unless a write-transaction has already 
  ** been started. The journal file may or may not be open at this point.
  ** It is never called in the ERROR state.
     这个程序不会被调用除非写事务已经开始。此时，这个日志文件或许会被打开也或许不会被打开。
     在异常状态下，它不会被调用。
  */
  assert( pPager->eState==PAGER_WRITER_LOCKED
       || pPager->eState==PAGER_WRITER_CACHEMOD
       || pPager->eState==PAGER_WRITER_DBMOD
  );
  assert( assert_pager_state(pPager) );

  /* If an error has been previously detected, report the same error
  ** again. This should not happen, but the check provides robustness.
     如果一开始检查出错误，那么再次报告相同错误。这不应该发生，但是检查提供了健壮性。
   */
  if( NEVER(pPager->errCode) )  return pPager->errCode;

  /* Higher-level routines never call this function if database is not
  ** writable.  But check anyway, just for robustness. 
     如果数据库不可写，那么上层程序不会调用这个函数。但无论如何，检查只是为了健壮性。
  */
  if( NEVER(pPager->readOnly) ) return SQLITE_PERM;

  CHECK_PAGE(pPg);

  /* The journal file needs to be opened. Higher level routines have already
  ** obtained the necessary locks to begin the write-transaction, but the
  ** rollback journal might not yet be open. Open it now if this is the case.
  ** 需要打开日志文件。上层程序已经获取必要锁开始这个写事务，但是回滚日志可能还没有打开。
     如果是这种情况，现在打开它。
  ** This is done before calling sqlite3PcacheMakeDirty() on the page. 
  ** Otherwise, if it were done after calling sqlite3PcacheMakeDirty(), then
  ** an error might occur and the pager would end up in WRITER_LOCKED state
  ** with pages marked as dirty in the cache.
     在调用 sqlite3PcacheMakeDirty()函数之前，这些已经完成。否则，如果在调用 sqlite3PcacheMakeDirty()函数之后完成
     会出现错误，pager会在 WRITER_LOCKED状态下结束，在缓存中pages被标记为dirty。
  */
  if( pPager->eState==PAGER_WRITER_LOCKED ){
    rc = pager_open_journal(pPager);
    if( rc!=SQLITE_OK ) return rc;
  }
  assert( pPager->eState>=PAGER_WRITER_CACHEMOD );
  assert( assert_pager_state(pPager) );

  /* Mark the page as dirty.  If the page has already been written
  ** to the journal then we can return right away.
     页面标记为dirty。如果这个页面已经被写入日志，那么我们可以立即返回。
  */
  sqlite3PcacheMakeDirty(pPg);
  if( pageInJournal(pPg) && !subjRequiresPage(pPg) ){
    assert( !pagerUseWal(pPager) );
  }else{
  
    /* The transaction journal now exists and we have a RESERVED or an
    ** EXCLUSIVE lock on the main database file.  Write the current page to
    ** the transaction journal if it is not there already.
       事务日志已经存在，并且在主数据库文件中有保留锁或排它锁。
       如果它已经不存在，把当前页写入事务日志。
    */
    if( !pageInJournal(pPg) && !pagerUseWal(pPager) ){
      assert( pagerUseWal(pPager)==0 );
      if( pPg->pgno<=pPager->dbOrigSize && isOpen(pPager->jfd) ){
        u32 cksum;
        char *pData2;
        i64 iOff = pPager->journalOff;

        /* We should never write to the journal file the page that
        ** contains the database locks.  The following assert verifies
        ** that we do not.
           我们不应该把包含数据库锁的页面写入事务日志。下面的声明证实我们不可以这样操作。
        */
        assert( pPg->pgno!=PAGER_MJ_PGNO(pPager) );

        assert( pPager->journalHdr<=pPager->journalOff );
        CODEC2(pPager, pData, pPg->pgno, 7, return SQLITE_NOMEM, pData2);
        cksum = pager_cksum(pPager, (u8*)pData2);

        /* Even if an IO or diskfull error occurs while journalling the
        ** page in the block above, set the need-sync flag for the page.
        ** Otherwise, when the transaction is rolled back, the logic in
        ** playback_one_page() will think that the page needs to be restored
        ** in the database file. And if an IO error occurs while doing so,
        ** then corruption may follow.
           即使在上面块的日志文件中IO或者DISKFULL发生错误，也要为页面设置need-sync标记。
           否则，当日志恢复， playback_one_page() 逻辑认为这个页面需要在数据库文件中被恢复。
           如果同时发生IO错误，那么程序会中断。
        */
        pPg->flags |= PGHDR_NEED_SYNC;

        rc = write32bits(pPager->jfd, iOff, pPg->pgno);
        if( rc!=SQLITE_OK ) return rc;
        rc = sqlite3OsWrite(pPager->jfd, pData2, pPager->pageSize, iOff+4);
        if( rc!=SQLITE_OK ) return rc;
        rc = write32bits(pPager->jfd, iOff+pPager->pageSize+4, cksum);
        if( rc!=SQLITE_OK ) return rc;

        IOTRACE(("JOUT %p %d %lld %d\n", pPager, pPg->pgno, 
                 pPager->journalOff, pPager->pageSize));
        PAGER_INCR(sqlite3_pager_writej_count);
        PAGERTRACE(("JOURNAL %d page %d needSync=%d hash(%08x)\n",
             PAGERID(pPager), pPg->pgno, 
             ((pPg->flags&PGHDR_NEED_SYNC)?1:0), pager_pagehash(pPg)));

        pPager->journalOff += 8 + pPager->pageSize;
        pPager->nRec++;
        assert( pPager->pInJournal!=0 );
        rc = sqlite3BitvecSet(pPager->pInJournal, pPg->pgno);
        testcase( rc==SQLITE_NOMEM );
        assert( rc==SQLITE_OK || rc==SQLITE_NOMEM );
        rc |= addToSavepointBitvecs(pPager, pPg->pgno);
        if( rc!=SQLITE_OK ){
          assert( rc==SQLITE_NOMEM );
          return rc;
        }
      }else{
        if( pPager->eState!=PAGER_WRITER_DBMOD ){
          pPg->flags |= PGHDR_NEED_SYNC;
        }
        PAGERTRACE(("APPEND %d page %d needSync=%d\n",
                PAGERID(pPager), pPg->pgno,
               ((pPg->flags&PGHDR_NEED_SYNC)?1:0)));
      }
    }
  
    /* If the statement journal is open and the page is not in it,
    ** then write the current page to the statement journal.  Note that
    ** the statement journal format differs from the standard journal format
    ** in that it omits the checksums and the header.
       如果打开日志，里面没有页面。那么把当前页面写入这个日志。
       注意这个日志格式不同于标准日志格式，它省去了校验和标题。
    */
    if( subjRequiresPage(pPg) ){
      rc = subjournalPage(pPg);
    }
  }

  /* Update the database size and return.
    更新数据库大小并且返回。
  */
  if( pPager->dbSize<pPg->pgno ){
    pPager->dbSize = pPg->pgno;
  }
  return rc;
}

/*
** Mark a data page as writeable. This routine must be called before 
** making changes to a page. The caller must check the return value 
** of this function and be careful not to change any page data unless 
** this routine returns SQLITE_OK.
   标记一个可写数据页面。在修改页面之前，这个程序必须被调用。调用者必须检查这个功能的返回值并且不能修改任何页面数据，
   除非这个程序返回SQLITE_OK。
** The difference between this function and pager_write() is that this
** function also deals with the special case where 2 or more pages
** fit on a single disk sector. In this case all co-resident pages
** must have been written to the journal file before returning.
** 这个函数和pager_write()之间的区别是这个函数同时处理两个或更多页面适合单个磁盘扇区这种特殊情况。
   在这种特殊情况下，必须在返回前把所有共驻主存页面写入日志文件。
** If an error occurs, SQLITE_NOMEM or an IO error code is returned
** as appropriate. Otherwise, SQLITE_OK.
   如果发生错误，适当返回SQLITE_NOMEM 和IO错误代码。否则，返回SQLITE_OK。
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
    Pgno nPageCount;          /* Total number of pages in database file 数据库文件中的总页数*/
    Pgno pg1;                 /* First page of the sector pPg is located on.pPg扇区中第一个页面的位置 */
    int nPage = 0;            /* Number of pages starting at pg1 to journal 从pg1到日志的总页数*/
    int ii;                   /* Loop counter 循环计数器*/
    int needSync = 0;         /* True if any page has PGHDR_NEED_SYNC  所有PGHDR_NEED_SYNC的页面是对的*/

    /* Set the doNotSyncSpill flag to 1. This is because we cannot allow
    ** a journal header to be written between the pages journaled by
    ** this function.
       设置doNotSyncSpill标记为1.这是因为我们不允许用这个函数在页面编写日志标题。
    */
    assert( !MEMDB );
    assert( pPager->doNotSyncSpill==0 );
    pPager->doNotSyncSpill++;

    /* This trick assumes that both the page-size and sector-size are
    ** an integer power of 2. It sets variable pg1 to the identifier
    ** of the first page of the sector pPg is located on.
       这个方法假定页面大小和扇区大小都是2的整数幂。
       它设置变量pg1到扇区pPg第一页面标示符的位置。
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
       如果PGHDR_NEED_SYNC标记设置为pg1启动下的nPage所有页面，那么它需要设置为全部。
       因为写这些nPage页面会影响到其他，在他们被写出数据库文件之前，日志文件必须包含sync()ed的全部副本。
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
** Return TRUE if the page given in the argument was previously passed
** to sqlite3PagerWrite().  In other words, return TRUE if it is ok
** to change the content of the page.
   如果页面给出的参数被提前传递给sqlite3PagerWrite()，返回TRUE。
   换句话说，如果它成功改变了页面的内容则返回TRUE。
*/
#ifndef NDEBUG
int sqlite3PagerIswriteable(DbPage *pPg){
  return pPg->flags&PGHDR_DIRTY;
}
#endif

/*
** A call to this routine tells the pager that it is not necessary to
** write the information on page pPg back to the disk, even though
** that page might be marked as dirty.  This happens, for example, when
** the page has been added as a leaf of the freelist and so its
** content no longer matters.
** 调用这个程序告诉pager没有必要编写分回磁盘页的信息，即使页面会被标记为dirty。
   当这一切发生,例如,当页面添加了自由表中的叶,它的内容不再重要。
** The overlying software layer calls this routine when all of the data
** on the given page is unused. The pager marks the page as clean so
** that it does not get written to disk.
** 当所提供页面上的所有数据未使用，整个软件层就调用这个程序。page把它标记干净页面以至于它不会被写入磁盘。
** Tests show that this optimization can quadruple the speed of large 
** DELETE operations.测试表明，这种优化可以将删除操作的速度提高四倍。
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
** This routine is called to increment the value of the database file 
** change-counter, stored as a 4-byte big-endian integer starting at 
** byte offset 24 of the pager file.  The secondary change counter at
** 92 is also updated, as is the SQLite version number at offset 96.
*  调用这个程序增加数据库文件中change-counter的值，存储为一个4字节的高位优先整数，它起始于pager文件中24个字节偏移量。
** But this only happens if the pPager->changeCountDone flag is false.
** To avoid excess churning of page 1, the update only happens once.
** See also the pager_write_changecounter() routine that does an 
** unconditional update of the change counters.
** 但是这只发生在pPager->changeCountDone标记错误的情况下。为了避免page1发生抖动，更新只发生一次。
   另请参阅 pager_write_changecounter()程序，这是一个无条件更新变化计数器。  
** If the isDirectMode flag is zero, then this is done by calling 
** sqlite3PagerWrite() on page 1, then modifying the contents of the
** page data. In this case the file will be updated when the current
** transaction is committed.
** 如果DirectMode标记为零，那么它在page1上通过调用sqlite3PagerWrite()函数完成，
   然后修改页面数据的内容。在这种情况下,当前事务被提交的时候直接更新文件。
** The isDirectMode flag may only be non-zero if the library was compiled
** with the SQLITE_ENABLE_ATOMIC_WRITE macro defined. In this case,
** if isDirect is non-zero, then the database file is updated directly
** by writing an updated version of page 1 using a call to the 
** sqlite3OsWrite() function.
   当宏SQLITE_ENABLE_ATOMIC_WRITE定义时，那么在编译后的库中DirectMode只能非零。
   在这种情况下，如果Direct不为零，那么通过写升级版调用page1的sqlite3OsWrite()函数直接更新数据库文件。
*/
static int pager_incr_changecounter(Pager *pPager, int isDirectMode){
  int rc = SQLITE_OK;

  assert( pPager->eState==PAGER_WRITER_CACHEMOD
       || pPager->eState==PAGER_WRITER_DBMOD
  );
  assert( assert_pager_state(pPager) );

  /* Declare and initialize constant integer 'isDirect'. If the
  ** atomic-write optimization is enabled in this build, then isDirect
  ** is initialized to the value passed as the isDirectMode parameter
  ** to this function. Otherwise, it is always set to zero.
  ** 声明和初始化整型常量isDirect。如果atomic-write优化在构建中被启用，那么
     isDirect初始化值通过isDirectMode参数传递给这个函数。否则，它总是设置为0。
  ** The idea is that if the atomic-write optimization is not
  ** enabled at compile time, the compiler can omit the tests of
  ** 'isDirect' below, as well as the block enclosed in the
  ** "if( isDirect )" condition.
      这个思想是指如果在编译阶段没有启用atomic-write优化，
      那么这个编译器可以省略'isDirect'下面的测验，以及"if( isDirect )"条件下块的封闭。
  */
#ifndef SQLITE_ENABLE_ATOMIC_WRITE
# define DIRECT_MODE 0
  assert( isDirectMode==0 );
  UNUSED_PARAMETER(isDirectMode);
#else
# define DIRECT_MODE isDirectMode
#endif

  if( !pPager->changeCountDone && pPager->dbSize>0 ){
    PgHdr *pPgHdr;                /* Reference to page 1 参考第一页*/

    assert( !pPager->tempFile && isOpen(pPager->fd) );

    /* Open page 1 of the file for writing. 打开第一页并进行写操作*/
    rc = sqlite3PagerGet(pPager, 1, &pPgHdr);
    assert( pPgHdr==0 || rc==SQLITE_OK );

    /* If page one was fetched successfully, and this function is not
    ** operating in direct-mode, make page 1 writable.  When not in 
    ** direct mode, page 1 is always held in cache and hence the PagerGet()
    ** above is always successful - hence the ALWAYS on rc==SQLITE_OK.
       如果页面获取成功，并且在direct-mode下没有执行这个函数，那么使得page1可写。
       不在direct-mode的时候，第1页一直在缓存中,所以PagerGet()总是成功，因此总是处于rc = = SQLITE_OK。
    */
    if( !DIRECT_MODE && ALWAYS(rc==SQLITE_OK) ){
      rc = sqlite3PagerWrite(pPgHdr);
    }

    if( rc==SQLITE_OK ){
      /* Actually do the update of the change counter 事实上更新改变计数器*/
      pager_write_changecounter(pPgHdr);

      /* If running in direct mode, write the contents of page 1 to the file. 
         如果在direct mode运行，把第一页的内容写入文件。 */
      if( DIRECT_MODE ){
        const void *zBuf;
        assert( pPager->dbFileSize>0 );
        CODEC2(pPager, pPgHdr->pData, 1, 6, rc=SQLITE_NOMEM, zBuf);
        if( rc==SQLITE_OK ){
          rc = sqlite3OsWrite(pPager->fd, zBuf, pPager->pageSize, 0);
          pPager->aStat[PAGER_STAT_WRITE]++;
        }
        if( rc==SQLITE_OK ){
          pPager->changeCountDone = 1;
        }
      }else{
        pPager->changeCountDone = 1;
      }
    }

    /* Release the page reference. 释放页面索引*/
    sqlite3PagerUnref(pPgHdr);
  }
  return rc;
}

/*
** Sync the database file to disk. This is a no-op for in-memory databases
** or pages with the Pager.noSync flag set.
** 同步数据库文件到磁盘。这是内存数据库的空操作或者设置为Pager.noSync的页面。
** If successful, or if called on a pager for which it is a no-op, this
** function returns SQLITE_OK. Otherwise, an IO error code is returned.
   如果执行成功，或者在pager上调用一个空操作，那么这个函数返回 SQLITE_OK。否则，返回IO错误代码。
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
** This function may only be called while a write-transaction is active in
** rollback. If the connection is in WAL mode, this call is a no-op. 
** Otherwise, if the connection does not already have an EXCLUSIVE lock on 
** the database file, an attempt is made to obtain one.
** 只能在写事务活跃于回滚的时候调用这个函数。如果这个连接在WAL模式下，那么这个调用是个空操作。
   否则，如果数据库文件中这个连接没有排它锁，试图获取一个。
** If the EXCLUSIVE lock is already held or the attempt to obtain it is
** successful, or the connection is in WAL mode, SQLITE_OK is returned.
** Otherwise, either SQLITE_BUSY or an SQLITE_IOERR_XXX error code is 
** returned.
   如果排它锁已经存在或者试图获取成功，那么在WAL模式下，这个连接返回SQLITE_OK 。
   否则，返回SQLITE_BUSY 或者 SQLITE_IOERR_XXX 错误代码。
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
** Sync the database file for the pager pPager. zMaster points to the name
** of a master journal file that should be written into the individual
** journal file. zMaster may be NULL, which is interpreted as no master
** journal (a single database transaction).
** 同步数据库文件pager pPager。zMaster指向一个主日志文件的名字，使得它被写入个人日志文件。
   zMaster可能为空，它只是单一的数据库事务，没有主日志翻译它。
** This routine ensures that:
** 这个程序确保：
**   * The database file change-counter is updated,
       数据库文件change-counter的更新，
**   * the journal is synced (unless the atomic-write optimization is used),
       同步日志（除非使用atomic-write优化）
**   * all dirty pages are written to the database file, 
       把所有dirty页面写入数据库文件
**   * the database file is truncated (if required), and
**   * the database file synced. 截断数据库文件（如果需要），同步数据库文件。
**
** The only thing that remains to commit the transaction is to finalize 
** (delete, truncate or zero the first part of) the journal file (or 
** delete the master journal file if specified).
** 唯一有待完成的是提交事务（删除，截断，使第一部分为零）来完成日志文件的操作（如果需要，删除主日志文件）。
** Note that if zMaster==NULL, this does not overwrite a previous value
** passed to an sqlite3PagerCommitPhaseOne() call.
** 注意，如果zMaster为空，并不覆盖传递给sqlite3PagerCommitPhaseOne()函数调用之前的值。
** If the final parameter - noSync - is true, then the database file itself
** is not synced. The caller must call sqlite3PagerSync() directly to
** sync the database file before calling CommitPhaseTwo() to delete the
** journal file in this case.
   如果最后一个参数noSync是真值的，那么数据库文件自身不同步。
   在调用CommitPhaseTwo() 函数删除日志文件的情况下，使用者必须直接调用sqlite3PagerSync() 函数同步数据库文件。
*/
int sqlite3PagerCommitPhaseOne(
  Pager *pPager,                  /* Pager object  Pager对象 */
  const char *zMaster,            /* If not NULL, the master journal name  如果不为空，主日志名称 */
  int noSync                      /* True to omit the xSync on the db file  在db文件真正忽略xSync*/
){
  int rc = SQLITE_OK;             /* Return code 返回代码*/

  assert( pPager->eState==PAGER_WRITER_LOCKED
       || pPager->eState==PAGER_WRITER_CACHEMOD
       || pPager->eState==PAGER_WRITER_DBMOD
       || pPager->eState==PAGER_ERROR
  );
  assert( assert_pager_state(pPager) );

  /* If a prior error occurred, report that error again.
     如果之前发生错误，再次报告错误*/
  if( NEVER(pPager->errCode) ) return pPager->errCode;

  PAGERTRACE(("DATABASE SYNC: File=%s zMaster=%s nSize=%d\n", 
      pPager->zFilename, zMaster, pPager->dbSize));

  /* If no database changes have been made, return early. 
     如果没有数据库更新，提前返回。*/
  if( pPager->eState<PAGER_WRITER_CACHEMOD ) return SQLITE_OK;

  if( MEMDB ){
    /* If this is an in-memory db, or no pages have been written to, or this
    ** function has already been called, it is mostly a no-op.  However, any
    ** backup in progress needs to be restarted.
       如果这是个内存db，或者没有可写页面，或者这个函数已经被调用，那么。
       然而，在进程中需要重新启动所有备份。
    */
    sqlite3BackupRestart(pPager->pBackup);
  }else{
    if( pagerUseWal(pPager) ){
      PgHdr *pList = sqlite3PcacheDirtyList(pPager->pPCache);
      PgHdr *pPageOne = 0;
      if( pList==0 ){
        /* Must have at least one page for the WAL commit flag.
        ** Ticket [2d1a5c67dfc2363e44f29d9bbd57f] 2011-05-18 
           必须至少有一个WAL提交标记页面。证明书[2d1a5c67dfc2363e44f29d9bbd57f] 2011-05-18 */
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
      ** 接下来的块更新change-counter。事实上它如何操作是根据在编译阶段是否启用atomic-update 优化，
         和这个事务是否运行时使用操作标准：
      **    * The file-system supports the atomic-write property for
      **      blocks of size page-size, and 
              这文件系统支持atomic-write属性为size page-size中的块，
      **    * This commit is not part of a multi-file transaction,这提交的不是多文件事务里的一部分 and
      **    * Exactly one page has been modified and store in the journal file.
      **      事实上页面已经修改并储存在日志文件里。
      ** If the optimization was not enabled at compile time, then the
      ** pager_incr_changecounter() function is called to update the change
      ** counter in 'indirect-mode'. If the optimization is compiled in but
      ** is not applicable to this transaction, call sqlite3JournalCreate()
      ** to make sure the journal file has actually been created, then call
      ** pager_incr_changecounter() to update the change-counter in indirect
      ** mode. 如果在编译阶段没有启用这个优化，那么在indirect模式下，调用pager_incr_changecounter()函数更新改变计数器。
         如果优化被编译但不适合这个事务，并且调用sqlite3JournalCreate()函数确保日志文件事实上已经被创建，那么在indirect模式下调用pager_incr_changecounter() 
         函数更新change-counter。
      ** Otherwise, if the optimization is both enabled and applicable,
      ** then call pager_incr_changecounter() to update the change-counter
      ** in 'direct' mode. In this case the journal file will never be
      ** created for this transaction.
         否则，如果适当的启用这个优化，那么在'direct'模式下调用pager_incr_changecounter()函数更新change-counter。
         在这种情况下，这个事务不会创建日志文件。
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
           通过直写方法更新db文件change counter。接下来调用函数修改page1中的内存表示方法，包括更新改变计数器，
           然后再把Page1写入数据库文件。由于主机文件系统的atomic-write 属性，这操作是安全的。
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
      ** 如果这个事务使数据库变小，那么截断被丢弃的所有页面，然后把他们写入日志文件。
         这只在auto-vacuum模式下发生。
      ** Before reading the pages with page numbers larger than the 
      ** current value of Pager.dbSize, set dbSize back to the value
      ** that it took at the start of the transaction. Otherwise, the
      ** calls to sqlite3PagerGet() return zeroed pages instead of 
      ** reading data from the database file.
         在阅读有页码的页面大于Pager.dbsize当前值之前，设置dbSize返回值，这样才能开始事务。
         否则，调用sqlite3PagerGet()函数返回零页面，而不是读取数据库文件中的数据。
      */
  #ifndef SQLITE_OMIT_AUTOVACUUM
      if( pPager->dbSize<pPager->dbOrigSize 
       && pPager->journalMode!=PAGER_JOURNALMODE_OFF
      ){
        Pgno i;                                   /* Iterator variable 迭代器变量*/
        const Pgno iSkip = PAGER_MJ_PGNO(pPager); /* Pending lock page 等待锁定页面*/
        const Pgno dbSize = pPager->dbSize;       /* Database image size 数据库图像大小*/ 
        pPager->dbSize = pPager->dbOrigSize;
        for( i=dbSize+1; i<=pPager->dbOrigSize; i++ ){
          if( !sqlite3BitvecTest(pPager->pInJournal, i) && i!=iSkip ){
            PgHdr *pPage;             /* Page to journal 页面杂志*/
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
         把主日志名字写入日志文件。如果一个主日志文件名已经写入日志文件，或者如果zMaster为空，那么这个调用是空操作。      */
      rc = writeMasterJournal(pPager, zMaster);
      if( rc!=SQLITE_OK ) goto commit_phase_one_exit;
  
      /* Sync the journal file and write all dirty pages to the database.
      ** If the atomic-update optimization is being used, this sync will not 
      ** create the journal file or perform any real IO.
      ** 同步日志文件并且把所有dirty页面写入数据库。如果使用atomic-update优化，此同步不会创建日志文件或者执行任何真正的IO操作。
      ** Because the change-counter page was just modified, unless the
      ** atomic-update optimization is used it is almost certain that the
      ** journal requires a sync here. However, in locking_mode=exclusive
      ** on a system under memory pressure it is just possible that this is 
      ** not the case. In this case it is likely enough that the redundant
      ** xSync() call will be changed to a no-op by the OS anyhow.
         因为只是在修改change-counter页面，除非使用atomic-update优化，这说明此时需要同步日志。
         然而，在内存压力下的locking_mode=exclusive系统并非如此。在这种情况下，通过任意OS操作调用xSync()函数冗余把它改成空。
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
         如果磁盘文件和数据库图像大小不一样，那么使用pager_truncate扩大或者缩小文件。
      */
      if( pPager->dbSize!=pPager->dbFileSize ){
        Pgno nNew = pPager->dbSize - (pPager->dbSize==PAGER_MJ_PGNO(pPager));
        assert( pPager->eState==PAGER_WRITER_DBMOD );
        rc = pager_truncate(pPager, nNew);
        if( rc!=SQLITE_OK ) goto commit_phase_one_exit;
      }
  
      /* Finally, sync the database file.最后，同步数据库文件。 */
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
** When this function is called, the database file has been completely
** updated to reflect the changes made by the current transaction and
** synced to disk. The journal file still exists in the file-system 
** though, and if a failure occurs at this point it will eventually
** be used as a hot-journal and the current transaction rolled back.
** 调用这个函数的时候，数据库文件已经完成更新，这体现出当前事务和磁盘同步操作的变化。
   虽然日志文件仍然存在于文件系统，如果此时发生故障，它将最终被当做hot-journal使用或者恢复当前事务。
** This function finalizes the journal file, either by deleting, 
** truncating or partially zeroing it, so that it cannot be used 
** for hot-journal rollback. Once this is done the transaction is
** irrevocably committed.
** 通过删除，截断或者部分归零，这个函数完成日志文件的操作，以便它不被用来恢复 hot-journal。
   此操作一旦完成，事务不可取消提交。
** If an error occurs, an IO error code is returned and the pager
** moves into the error state. Otherwise,SQLITE_OK is returned.
   如果发生错误，返回IO错误代码并将且pager移入错误状态。否则，返回SQLITE_OK。
*/
int sqlite3PagerCommitPhaseTwo(Pager *pPager){
  int rc = SQLITE_OK;                  /* Return code返回代码 */

  /* This routine should not be called if a prior error has occurred.
  ** But if (due to a coding error elsewhere in the system) it does get
  ** called, just return the same error code without doing anything.
     如果之前已经发生错误，那么不会调用这个程序。但是如果它被调用，只返回相同的错误代码，不进行任何操作。*/
  if( NEVER(pPager->errCode) ) return pPager->errCode;

  assert( pPager->eState==PAGER_WRITER_LOCKED
       || pPager->eState==PAGER_WRITER_FINISHED
       || (pagerUseWal(pPager) && pPager->eState==PAGER_WRITER_CACHEMOD)
  );
  assert( assert_pager_state(pPager) );

  /* An optimization. If the database was not actually modified during
  ** this transaction, the pager is running in exclusive-mode and is
  ** using persistent journals, then this function is a no-op.
  **一个最优化事务。在这个事务中，如果事实上没有修改数据库，pager在独占模式下运行并且使用永久对象日志，那么这是空操作。
  ** The start of the journal file currently contains a single journal 
  ** header with the nRec field set to 0. If such a journal is used as
  ** a hot-journal during hot-journal rollback, 0 changes will be made
  ** to the database file. So there is no need to zero the journal 
  ** header. Since the pager is in exclusive mode, there is no need
  ** to drop any locks either.
     当前启动的日志文件包含单一日志头，设置nRec字段为0。如果在恢复hot-journal时日志被当做hot-journal使用，数据库文件变成0。
     所以日志头不需要为零。由于pager在独占模式下，那就不需要放弃任何锁。
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
** 如果打开一个写事务，那么恢复事务做出的所有改变并且关闭写事务。如果操作成功，pager落回到PAGER_READER状态，
   或者如果出现错误，就进入PAGER_ERROR状态。
** If the pager is already in PAGER_ERROR state when this function is called,
** it returns Pager.errCode immediately. No work is performed in this case.
** 如果调用这个函数的时候pager已经处于PAGER_ERROR状态，那么马上返回Pager错误代码。
   在这种情况下不执行任何操作。
** Otherwise, in rollback mode, this function performs two functions:
** 另外，在回滚模式下，这个函数体现两个功能：
**   1) It rolls back the journal file, restoring all database file and 
**      in-memory cache pages to the state they were in when the transaction
**      was opened, and
**      打开事务时，它将恢复日志文件，储存所有数据库文件并且把内存缓存页面写入它所在的状态。
**   2) It finalizes the journal file, so that it is not used for hot
**      rollback at any point in the future.
**      结束日志文件时，它确保在将来任何时候不会用于热回滚。
** Finalization of the journal file (task 2) is only performed if the 
** rollback is successful.
** 如果恢复成功，只实现日志文件的结束操作。
** In WAL mode, all cache-entries containing data modified within the
** current transaction are either expelled from the cache or reverted to
** their pre-transaction state by re-reading data from the database or
** WAL files. The WAL transaction is then closed.
   在WAL模式下，当前事务中，所有含修改数据的缓存条目通过在数据库或者WAL文件中重读数据，
   从缓存中驱逐或恢复它们之前的事务状态，然后关闭wal事务。
*/
int sqlite3PagerRollback(Pager *pPager){
  int rc = SQLITE_OK;                  /* Return code 返回代码*/
  PAGERTRACE(("ROLLBACK %d\n", PAGERID(pPager)));

  /* PagerRollback() is a no-op if called in READER or OPEN state. If
  ** the pager is already in the ERROR state, the rollback is not 
  ** attempted here. Instead, the error code is returned to the caller.
     如果在READER 或者 OPEN状态下调用PagerRollback(）函数，这是一个空操作。
     如果pager已经在ERROR状态，没有实现恢复功能。相反地，返回错误代码。
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
         使用关闭日志模式，可能会发生。把pager移动到错误状态表明不信任缓存内容。
         所有活跃读者将得到SQLITE_ABORT。
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
     如果回滚时发生错误，我们不再相信pager缓存。所以调用pager_error()方法持续错误。
  */
  return pager_error(pPager, rc);
}

/*
** Return TRUE if the database file is opened read-only.  Return FALSE
** if the database is (in theory) writable.
   如果打开数据库文件只读则返回true。如果数据库可写则返回false。
*/
u8 sqlite3PagerIsreadonly(Pager *pPager){
  return pPager->readOnly;
}

/*
** Return the number of references to the pager.返回pager引用的数量。
*/
int sqlite3PagerRefcount(Pager *pPager){
  return sqlite3PcacheRefCount(pPager->pPCache);
}

/*
** Return the approximate number of bytes of memory currently
** used bythe pager and its associated cache.
   返回pager当前使用的字节的内存近似数和相关的缓存。
*/
int sqlite3PagerMemUsed(Pager *pPager){
  int perPageSize = pPager->pageSize + pPager->nExtra + sizeof(PgHdr)
                                     + 5*sizeof(void*);
  return perPageSize*sqlite3PcachePagecount(pPager->pPCache)
           + sqlite3MallocSize(pPager)
           + pPager->pageSize;
}

/*
** Return the number of references to the specified page.返回引用指定页面的数量。
*/
int sqlite3PagerPageRefcount(DbPage *pPage){
  return sqlite3PcachePageRefcount(pPage);
}

#ifdef SQLITE_TEST
/*
** This routine is used for testing and analysis only.这个程序只是用来测试和分析。
*/
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
  a[8] = 0;  /* Used to be pPager->nOvfl 曾经是pPager->nOvfl */
  a[9] = pPager->nRead;
  a[10] = pPager->aStat[PAGER_STAT_WRITE];
  return a;
}
#endif

/*
** Parameter eStat must be either SQLITE_DBSTATUS_CACHE_HIT or
** SQLITE_DBSTATUS_CACHE_MISS. Before returning, *pnVal is incremented by the
** current cache hit or miss count, according to the value of eStat. If the 
** reset parameter is non-zero, the cache hit or miss count is zeroed before 
** returning.
   参数eStat必须是SQLITE_DBSTATUS_CACHE_HIT或者SQLITE_DBSTATUS_CACHE_MISS。
   在返回之前，根据eStat参数的值，通过命中当前缓存或者错过计数，*pnVal的值增加。
   如果复位参数不为零，在返回之前，命中缓存或错过计数为零。
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
** Return true if this is an in-memory pager.如果这是个内存pager返回TRUE。
*/
int sqlite3PagerIsMemdb(Pager *pPager){
  return MEMDB;
}

/*
** Check that there are at least nSavepoint savepoints open. If there are
** currently less than nSavepoints open, then open one or more savepoints
** to make up the difference. If the number of savepoints is already
** equal to nSavepoint, then this function is a no-op.
** 检查至少打开nsavepoint保存点。如果当前还不到nSavepoints打开，那么打开一个或更多保存点来弥补不同。
** If a memory allocation fails, SQLITE_NOMEM is returned. If an error 
** occurs while opening the sub-journal file, then an IO error code is
** returned. Otherwise, SQLITE_OK.
   如果一个内存分配失败，则返回SQLITE_NOMEM。如果在打开sub-journal文件时出现错误，那么返回IO错误代码。
   否则，返回SQLITE_OK。
*/
int sqlite3PagerOpenSavepoint(Pager *pPager, int nSavepoint){
  int rc = SQLITE_OK;                       /* Return code 返回代码 */
  int nCurrent = pPager->nSavepoint;        /* Current number of savepoints 当前保存点数量*/

  assert( pPager->eState>=PAGER_WRITER_LOCKED );
  assert( assert_pager_state(pPager) );

  if( nSavepoint>nCurrent && pPager->useJournal ){
    int ii;                                 /* Iterator variable 迭代器变量 */
    PagerSavepoint *aNew;                   /* New Pager.aSavepoint array 新Pager.aSavepoint数组 */

    /* Grow the Pager.aSavepoint array using realloc(). Return SQLITE_NOMEM
    ** if the allocation fails. Otherwise, zero the new portion in case a 
    ** malloc failure occurs while populating it in the for(...) loop below.
       使用realloc()函数扩大Pager.aSavepoint 数组。如果分配失败，返回SQLITE_NOMEM。
       否则，在填充下面的for（…）循环时把程序中发生失败新的部分分配内存清零。
    */
    aNew = (PagerSavepoint *)sqlite3Realloc(
        pPager->aSavepoint, sizeof(PagerSavepoint)*nSavepoint
    );
    if( !aNew ){
      return SQLITE_NOMEM;
    }
    memset(&aNew[nCurrent], 0, (nSavepoint-nCurrent) * sizeof(PagerSavepoint));
    pPager->aSavepoint = aNew;

    /* Populate the PagerSavepoint structures just allocated. 填充刚分配的PagerSavepoint结构。*/
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
** 调用这个函数是用来恢复或者释放一个保存点。这个保存点不需要是最近新创建的保存点。
** Parameter op is always either SAVEPOINT_ROLLBACK or SAVEPOINT_RELEASE.
** If it is SAVEPOINT_RELEASE, then release and destroy the savepoint with
** index iSavepoint. If it is SAVEPOINT_ROLLBACK, then rollback all changes
** that have occurred since the specified savepoint was created.
** 参数运算始终是SAVEPOINT_ROLLBACK或者SAVEPOINT_RELEASE。如果是SAVEPOINT_RELEASE，那么用iSavepoint指针释放和破坏保存点。
   如果是 SAVEPOINT_ROLLBACK，那么恢复由于创建指定保存点发生的所有改变。
** The savepoint to rollback or release is identified by parameter 
** iSavepoint. A value of 0 means to operate on the outermost savepoint
** (the first created). A value of (Pager.nSavepoint-1) means operate
** on the most recently created savepoint. If iSavepoint is greater than
** (Pager.nSavepoint-1), then this function is a no-op.
** 通过参数iSavepoint确认恢复或者释放保存点。0表示运行在最外层的保存点。
   Pager.nSavepoint-1表示在最近创建的保存点操作。如果 iSavepoint大于Pager.nSavepoint-1，那么这是个空操作。
** If a negative value is passed to this function, then the current
** transaction is rolled back. This is different to calling 
** sqlite3PagerRollback() because this function does not terminate
** the transaction or unlock the database, it just restores the 
** contents of the database to its original state. 
** 如果一个负数传递给这个函数，那么恢复当前事务。这是不同的调用sqlite3PagerRollback()函数 ，因为这个函数没有终止事务或者打开数据库，
   它只是将数据库的内容储存到原始状态。
** In any case, all savepoints with an index greater than iSavepoint 
** are destroyed. If this is a release operation (op==SAVEPOINT_RELEASE),
** then savepoint iSavepoint is also destroyed.
** 在任何情况下，所有大于iSavepoint含索引的保存点被破坏。如果它被释放，那么iSavepoint保存点同时被破坏。
** This function may return SQLITE_NOMEM if a memory allocation fails,
** or an IO error code if an IO error occurs while rolling back a 
** savepoint. If no errors occur, SQLITE_OK is returned.
   如果内存分配失败或者回滚一个保存点时IO发生错误，这个函数会返回SQLITE_NOMEM 。
   如果没有错误，返回SQLITE_OK。
*/ 
int sqlite3PagerSavepoint(Pager *pPager, int op, int iSavepoint){
  int rc = pPager->errCode;       /* Return code 返回代码*/

  assert( op==SAVEPOINT_RELEASE || op==SAVEPOINT_ROLLBACK );
  assert( iSavepoint>=0 || op==SAVEPOINT_ROLLBACK );

  if( rc==SQLITE_OK && iSavepoint<pPager->nSavepoint ){
    int ii;            /* Iterator variable 迭代器变量 */
    int nNew;          /* Number of remaining savepoints after this op.操作后的保留的保存点数量。 */

    /* Figure out how many savepoints will still be active after this
    ** operation. Store this value in nNew. Then free resources associated 
    ** with any savepoints that are destroyed by this operation.
       计算出此操作之后仍然是活跃保存点的数量。把它储存在nNew。那么所有和保存点相联系的免费资源通过此操作被破坏。
    */
    nNew = iSavepoint + (( op==SAVEPOINT_RELEASE ) ? 0 : 1);
    for(ii=nNew; ii<pPager->nSavepoint; ii++){
      sqlite3BitvecDestroy(pPager->aSavepoint[ii].pInSavepoint);
    }
    pPager->nSavepoint = nNew;

    /* If this is a release of the outermost savepoint, truncate 
    ** the sub-journal to zero bytes in size. 
       如果释放最外层保存点，把sub-journal大小截断成0字节。*/
    if( op==SAVEPOINT_RELEASE ){
      if( nNew==0 && isOpen(pPager->sjfd) ){
        /* Only truncate if it is an in-memory sub-journal.只在它是内存sub-journal时截断。 */
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
       回放指定保存点，除非这是个回滚操作。如果这是个空文件，可能是还没有打开日志文件。
       在这种情况下，如果数据库文件没有变化，所以可以跳过回放操作。
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
** Return the full pathname of the database file.
** 返回数据库文件的全路径名。
** Except, if the pager is in-memory only, then return an empty string if
** nullIfMemDb is true.  This routine is called with nullIfMemDb==1 when
** used to report the filename to the user, for compatibility with legacy
** behavior.  But when the Btree needs to know the filename for matching to
** shared cache, it uses nullIfMemDb==0 so that in-memory databases can
** participate in shared-cache.
   另外，如果pager只是内存，nullIfMemDb是真值，那么返回一个空的字符串。
   调用程序nullIfMemDb==1用于向用户报告文件名用来兼容和遗留。
   但是，当Btree需要知道匹配共享缓存的文件名时，调用nullIfMemDb==0 以至于内存数据库可以参与共享缓存。
*/
const char *sqlite3PagerFilename(Pager *pPager, int nullIfMemDb){
  return (nullIfMemDb && pPager->memDb) ? "" : pPager->zFilename;
}

/*
** Return the VFS structure for the pager.返回pager VFS结构。
*/
const sqlite3_vfs *sqlite3PagerVfs(Pager *pPager){
  return pPager->pVfs;
}

/*
** Return the file handle for the database file associated
** with the pager.  This might return NULL if the file has
** not yet been opened.
   返回与pager相关的数据库文件文件句柄。如果没有打开文件，它可能会返回null。
*/
sqlite3_file *sqlite3PagerFile(Pager *pPager){
  return pPager->fd;
}

/*
** Return the full pathname of the journal file.返回日志文件的全路径名。
*/
const char *sqlite3PagerJournalname(Pager *pPager){
  return pPager->zJournal;
}

/*
** Return true if fsync() calls are disabled for this pager.  Return FALSE
** if fsync()s are executed normally.
   如果pager中fsync()函数调用被禁用则返回true。如果fsync()s函数正常执行则返回FALSE。
*/
int sqlite3PagerNosync(Pager *pPager){
  return pPager->noSync;
}

#ifdef SQLITE_HAS_CODEC
/*
** Set or retrieve the codec for this pager设置或检索pager的解码器。
*/
void sqlite3PagerSetCodec(
  Pager *pPager,
  void *(*xCodec)(void*,void*,Pgno,int),
  void (*xCodecSizeChng)(void*,int,int),
  void (*xCodecFree)(void*),
  void *pCodec
){
  if( pPager->xCodecFree ) pPager->xCodecFree(pPager->pCodec);
  pPager->xCodec = pPager->memDb ? 0 : xCodec;
  pPager->xCodecSizeChng = xCodecSizeChng;
  pPager->xCodecFree = xCodecFree;
  pPager->pCodec = pCodec;
  pagerReportSize(pPager);
}
void *sqlite3PagerGetCodec(Pager *pPager){
  return pPager->pCodec;
}
#endif

#ifndef SQLITE_OMIT_AUTOVACUUM
/*
** Move the page pPg to location pgno in the file.
** 移动页面pPg到文件中pgno的位置。
** There must be no references to the page previously located at
** pgno (which we call pPgOld) though that page is allowed to be
** in cache.  If the page previously located at pgno is not already
** in the rollback journal, it is not put there by by this routine.
** 尽管那页面允许在缓存中，那么之前一定没有引用页面位于pgno(我们称之为pPgOld)。
   如果之前位于pgno的页面没有在回滚日志中，那么说明它没有使用这个程序。
** References to the page pPg remain valid. Updating any
** meta-data associated with pPg (i.e. data stored in the nExtra bytes
** allocated along with the page) is the responsibility of the caller.
** 引用pPg页面仍然有效。更新与pPg关联的任何元数据(即数据存储在nExtra字节分配的页)是调用者的责任。 
** A transaction must be active when this routine is called. It used to be
** required that a statement transaction was not active, but this restriction
** has been removed (CREATE INDEX needs to move a page when a statement
** transaction is active).
** 当调用程序时，事务必须是活跃的。以前要求事务并不一定活跃,但这一限制已被删除(创建索引时需要移动一个页面声明事务活动)。
** If the fourth argument, isCommit, is non-zero, then this page is being
** moved as part of a database reorganization just before the transaction 
** is being committed. In this case, it is guaranteed that the database page 
** pPg refers to will not be written to again within this transaction.
** 如果第四个参数已提交并且不为0，那么就在事务被提交的时候，这个页面被当做数据库重做的一部分被移动。
   在这种情况下，它保证数据库page页面不会再对这个事务进行写操作。
** This function may return SQLITE_NOMEM or an IO error code if an error
** occurs. Otherwise, it returns SQLITE_OK.
   如果发生错误，这个函数会返回SQLITE_NOMEM或者IO错误代码。否则，返回SQLITE_OK。
*/
int sqlite3PagerMovepage(Pager *pPager, DbPage *pPg, Pgno pgno, int isCommit){
  PgHdr *pPgOld;               /* The page being overwritten.页面被覆盖。 */
  Pgno needSyncPgno = 0;       /* Old value of pPg->pgno, if sync is required 如果需要同步，pPg->pgno的旧值*/
  int rc;                      /* Return code返回代码 */
  Pgno origPgno;               /* The original page number 最初的页码*/

  assert( pPg->nRef>0 );
  assert( pPager->eState==PAGER_WRITER_CACHEMOD
       || pPager->eState==PAGER_WRITER_DBMOD
  );
  assert( assert_pager_state(pPager) );

  /* In order to be able to rollback, an in-memory database must journal
  ** the page we are moving from.为了回滚操作，内存数据库必须把我们正在移动的页面写入日志。，
  */
  if( MEMDB ){
    rc = sqlite3PagerWrite(pPg);
    if( rc ) return rc;
  }

  /* If the page being moved is dirty and has not been saved by the latest
  ** savepoint, then save the current contents of the page into the 
  ** sub-journal now. This is required to handle the following scenario:
  ** 如果正在移动的页面是dirty，并且没有被最后一个保存点保存，那么就把页面的当前内容保存到sub-journal。
     这需要处理下面的情境：
  **   BEGIN;开始
  **     <journal page X, then modify it in memory> 在内存中修改它杂志页面X
  **     SAVEPOINT one;一个保存点
  **       <Move page X to location Y>把页面x移动到Y
  **     ROLLBACK TO one;回滚到保存点
  **
  ** If page X were not written to the sub-journal here, it would not
  ** be possible to restore its contents when the "ROLLBACK TO one"
  ** statement were is processed.
  ** 如果页面X没有写入sub-journal，当处理"ROLLBACK TO one"时，它不可能恢复它的内容。
  ** subjournalPage() may need to allocate space to store pPg->pgno into
  ** one or more savepoint bitvecs. This is the reason this function
  ** may return SQLITE_NOMEM. 
     subjournalPage()可能需要分配空间来存储pPg->pgno到一个或多个保存点bitvecs。
     这就是这个函数返回SQLITE_NOMEM的原因。
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
  ** 如果日志在页面 pPg->pgno可写之前需要同步，那么存储pPg->pgno在本地变量needSyncPgno。
  ** If the isCommit flag is set, there is no need to remember that
  ** the journal needs to be sync()ed before database page pPg->pgno 
  ** can be written to. The caller has already promised not to write to it.
     如果设置isCommit标记，在数据库页面pPg->pgno可写之前不需要记得这个日志需要同步。调用者已经承诺不写它。
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
     如果有包含page-number pgno页面的缓存，从其哈希链移动它。同时，在移动操作之前，如果PGHDR_NEED_SYNC标记设置为页面 pgno，
     那么它需要被保留为页面移动。
  */
  pPg->flags &= ~PGHDR_NEED_SYNC;
  pPgOld = pager_lookup(pPager, pgno);
  assert( !pPgOld || pPgOld->nRef==1 );
  if( pPgOld ){
    pPg->flags |= (pPgOld->flags&PGHDR_NEED_SYNC);
    if( MEMDB ){
      /* Do not discard pages from an in-memory database since we might
      ** need to rollback later.  Just move the page out of the way. 
         不要从内存数据库丢弃页面，因为我们或许一会需要回滚操作。这只需要移动页面。
      */
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
     对于一个内存数据库，万一事务需要回滚，所以确保原始页面继续存在。使用pPgOld作为原始页面,因为它已经被分配。
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
    ** 如果needSyncPgno不为0，那么在任何数据被写入数据库文件页面needSyncPgno之前，日志文件需要被同步。
       当前，没有这样的页面存在于页面缓存，并且"is journaled" bitvec标记已经设置。这就需要通过加载页面到页面缓存和设置PGHDR_NEED_SYNC标记
       当方式来加以弥补。
    ** If the attempt to load the page into the page-cache fails, (due
    ** to a malloc() or IO failure), clear the bit in the pInJournal[]
    ** array. Otherwise, if the page is loaded and written again in
    ** this transaction, it may be written to the database file before
    ** it is synced into the journal file. This way, it may end up in
    ** the journal file twice, but that is not a problem.
       如果试图把页面加载到页面缓存失败，（由于malloc（）或者IO失败）那么清除pInJournal[]数组中的比特。
       另外，如果在这个事务中加载页面，并且对此再次进行书写，那么在它同步到日志文件之前它会被写入数据库文件。
       通过这种方式，它可能在日志文件中结束两次，但这不是问题。
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
#endif

/*
** Return a pointer to the data for the specified page.返回指定页面的指向数据的指针。
*/
void *sqlite3PagerGetData(DbPage *pPg){
  assert( pPg->nRef>0 || pPg->pPager->memDb );
  return pPg->pData;
}

/*
** Return a pointer to the Pager.nExtra bytes of "extra" space 
** allocated along with the specified page.返回指定页面额外分配空间的Pager.nExtra字节的一个指针。
*/
void *sqlite3PagerGetExtra(DbPage *pPg){
  return pPg->pExtra;
}

/*
** Get/set the locking-mode for this pager. Parameter eMode must be one
** of PAGER_LOCKINGMODE_QUERY, PAGER_LOCKINGMODE_NORMAL or 
** PAGER_LOCKINGMODE_EXCLUSIVE. If the parameter is not _QUERY, then
** the locking-mode is set to the value specified.
** 设置pager的锁模式。参数eMode必须为PAGER_LOCKINGMODE_QUERY之一,PAGER_LOCKINGMODE_NORMAL或PAGER_LOCKINGMODE_EXCLUSIVE。
   如果没有参数_QUERY,那么locking-mode设置为指定的值。
** The returned value is either PAGER_LOCKINGMODE_NORMAL or
** PAGER_LOCKINGMODE_EXCLUSIVE, indicating the current (possibly updated)
** locking-mode.
   这个返回值是PAGER_LOCKINGMODE_NORMAL或者PAGER_LOCKINGMODE_EXCLUSIVE，表明当前的锁模式。
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
** 设置pager的日志模式。参数 eMode必须为其中之一：
**    PAGER_JOURNALMODE_DELETE            PAGER_JOURNALMODE的删除
**    PAGER_JOURNALMODE_TRUNCATE          PAGER_JOURNALMODE的截断            
**    PAGER_JOURNALMODE_PERSIST           PAGER_JOURNALMODE的坚持
**    PAGER_JOURNALMODE_OFF               PAGER_JOURNALMODE的关闭
**    PAGER_JOURNALMODE_MEMORY            PAGER_JOURNALMODE的内存
**    PAGER_JOURNALMODE_WAL               PAGER_JOURNALMODE的预写式日志
**
** The journalmode is set to the value specified if the change is allowed.
** The change may be disallowed for the following reasons:
** 如果允许改变，这个日志模式设置为指定值。不可以改变的原因如下：
**   *  An in-memory database can only have its journal_mode set to _OFF
**      or _MEMORY.
**      一个内存数据库只能把其日志模式设置为关闭或者存储。
**   *  Temporary databases cannot have _WAL journalmode.
**      临时数据库不能有预写式日志模式。
** The returned indicate the current (possibly updated) journal-mode.
   返回值表示当前日志模式。
*/
int sqlite3PagerSetJournalMode(Pager *pPager, int eMode){
  u8 eOld = pPager->journalMode;    /* Prior journalmode 原先日志模式*/

#ifdef SQLITE_DEBUG
  /* The print_pager_state() routine is intended to be used by the debugger
  ** only.  We invoke it once here to suppress a compiler warning.
     print_pager_state()程序目的只是用来调试的。我们调用它来抑制编译器警告。*/
  print_pager_state(pPager);
#endif


  /* The eMode parameter is always valid   eMode参数总是有效的。*/
  assert(      eMode==PAGER_JOURNALMODE_DELETE
            || eMode==PAGER_JOURNALMODE_TRUNCATE
            || eMode==PAGER_JOURNALMODE_PERSIST
            || eMode==PAGER_JOURNALMODE_OFF 
            || eMode==PAGER_JOURNALMODE_WAL 
            || eMode==PAGER_JOURNALMODE_MEMORY );

  /* This routine is only called from the OP_JournalMode opcode, and
  ** the logic there will never allow a temporary file to be changed
  ** to WAL mode.
     这个程序只从OP_JournalMode获取操作码,逻辑永远不会允许一个临时文件改为WAL模式。
  */
  assert( pPager->tempFile==0 || eMode!=PAGER_JOURNALMODE_WAL );

  /* Do allow the journalmode of an in-memory database to be set to
  ** anything other than MEMORY or OFF
     允许内存数据库的日志模式设置为任意状态除了存储或者关闭。
  */
  if( MEMDB ){
    assert( eOld==PAGER_JOURNALMODE_MEMORY || eOld==PAGER_JOURNALMODE_OFF );
    if( eMode!=PAGER_JOURNALMODE_MEMORY && eMode!=PAGER_JOURNALMODE_OFF ){
      eMode = eOld;
    }
  }

  if( eMode!=eOld ){

    /* Change the journal mode. 改变日志模式。 */
    assert( pPager->eState!=PAGER_ERROR );
    pPager->journalMode = (u8)eMode;

    /* When transistioning from TRUNCATE or PERSIST to any other journal
    ** mode except WAL, unless the pager is in locking_mode=exclusive mode,
    ** delete the journal file.
       当transistioning截断或存在其他日志模式除了WAL,除非pager在排它锁模式,那么删除日志文件。
    */
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
      ** 在这种情况下，我们要删除日志文件。如果不可能，那不是问题。删除日志文件只是个优化。
      ** Before deleting the journal file, obtain a RESERVED lock on the
      ** database file. This ensures that the journal file is not deleted
      ** while it is in use by some other client.
         在删除日志文件之前，在数据库文件中获取一个保留锁。这将确保在其他客户端使用的时候，日志文件不会被删除。
      */
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

  /* Return the new journal mode 返回新的日志模式*/
  return (int)pPager->journalMode;
}

/*
** Return the current journal mode.返回当前日志模式。
*/
int sqlite3PagerGetJournalMode(Pager *pPager){
  return (int)pPager->journalMode;
}

/*
** Return TRUE if the pager is in a state where it is OK to change the
** journalmode.  Journalmode changes can only happen when the database
** is unmodified.
   如果pager在可以改变日志模式的状态，那么返回TRUE。当数据库不被修改，日志模式只发生变化。
*/
int sqlite3PagerOkToChangeJournalMode(Pager *pPager){
  assert( assert_pager_state(pPager) );
  if( pPager->eState>=PAGER_WRITER_CACHEMOD ) return 0;
  if( NEVER(isOpen(pPager->jfd) && pPager->journalOff>0) ) return 0;
  return 1;
}

/*
** Get/set the size-limit used for persistent journal files.
** 获取/设置用于持久化日志文件的大小限制。
** Setting the size limit to -1 means no limit is enforced.
** An attempt to set a limit smaller than -1 is a no-op.
   设置大小限制为1意味着执行没有限制。试图设置一个限制小于1是个空操作。
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
   返回pPager - > pBackup的一个指针变量。backup.c中的backup模块维持这个变量的内容。
   这个模块只是使用它不透明地作为sqlite3BackupRestart()和sqlite3BackupUpdate()函数的参数。
*/
sqlite3_backup **sqlite3PagerBackupPtr(Pager *pPager){
  return &pPager->pBackup;
}

#ifndef SQLITE_OMIT_VACUUM
/*
** Unless this is an in-memory or temporary database, clear the pager cache.
   清除pager缓存，除非这是个内存或者临时数据库。
*/
void sqlite3PagerClearCache(Pager *pPager){
  if( !MEMDB && pPager->tempFile==0 ) pager_reset(pPager);
}
#endif

#ifndef SQLITE_OMIT_WAL
/*
** This function is called when the user invokes "PRAGMA wal_checkpoint",
** "PRAGMA wal_blocking_checkpoint" or calls the sqlite3_wal_checkpoint()
** or  wal_blocking_checkpoint() API functions.
** 当用户传唤"PRAGMA wal_checkpoint、"PRAGMA wal_blocking_checkpoint"或者
调用 sqlite3_wal_checkpoint()、wal_blocking_checkpoint() API 函数时，这个函数被调用。
** Parameter eMode is one of SQLITE_CHECKPOINT_PASSIVE, FULL or RESTART.
   eMode是SQLITE_CHECKPOINT_PASSIVE、FULL或者RESTART其中的一个参数。
*/
int sqlite3PagerCheckpoint(Pager *pPager, int eMode, int *pnLog, int *pnCkpt){
  int rc = SQLITE_OK;
  if( pPager->pWal ){
    rc = sqlite3WalCheckpoint(pPager->pWal, eMode,
        pPager->xBusyHandler, pPager->pBusyHandlerArg,
        pPager->ckptSyncFlags, pPager->pageSize, (u8 *)pPager->pTmpSpace,
        pnLog, pnCkpt
    );
  }
  return rc;
}

int sqlite3PagerWalCallback(Pager *pPager){
  return sqlite3WalCallback(pPager->pWal);
}

/*
** Return true if the underlying VFS for the given pager supports the
** primitives necessary for write-ahead logging.
   如果给定pager的底层VFS支持预写式日志记录的必要基元，则返回TRUE。
*/
int sqlite3PagerWalSupported(Pager *pPager){
  const sqlite3_io_methods *pMethods = pPager->fd->pMethods;
  return pPager->exclusiveMode || (pMethods->iVersion>=2 && pMethods->xShmMap);
}

/*
** Attempt to take an exclusive lock on the database file. If a PENDING lock
** is obtained instead, immediately release it.
   尝试在数据库文件中获取一个排它锁。如果获取的是一个等待锁，立马释放它。
*/
static int pagerExclusiveLock(Pager *pPager){
  int rc;                         /* Return code 返回代码 */

  assert( pPager->eLock==SHARED_LOCK || pPager->eLock==EXCLUSIVE_LOCK );
  rc = pagerLockDb(pPager, EXCLUSIVE_LOCK);
  if( rc!=SQLITE_OK ){
    /* If the attempt to grab the exclusive lock failed, release the 
    ** pending lock that may have been obtained instead. 
       如果尝试抓取排它锁失败，释放已经获取的等待锁。*/
    pagerUnlockDb(pPager, SHARED_LOCK);
  }

  return rc;
}

/*
** Call sqlite3WalOpen() to open the WAL handle. If the pager is in 
** exclusive-locking mode when this function is called, take an EXCLUSIVE
** lock on the database file and use heap-memory to store the wal-index
** in. Otherwise, use the normal shared-memory.
   调用sqlite3WalOpen()函数打开WAL句柄。如果在这个函数被调用的时候pager在排它锁模式下，
   那么使用堆内存存储wal-index，在数据库文件获取一个排它锁。否则，使用正常的共享内存。
*/
static int pagerOpenWal(Pager *pPager){
  int rc = SQLITE_OK;

  assert( pPager->pWal==0 && pPager->tempFile==0 );
  assert( pPager->eLock==SHARED_LOCK || pPager->eLock==EXCLUSIVE_LOCK );

  /* If the pager is already in exclusive-mode, the WAL module will use 
  ** heap-memory for the wal-index instead of the VFS shared-memory 
  ** implementation. Take the exclusive lock now, before opening the WAL
  ** file, to make sure this is safe.
    如果这个页面已经在排它模式，WAL模块将堆内存的wal-index代替VFS共享内存实现。在打开WAL文件之前，使用排它锁确保安全。
  */
  if( pPager->exclusiveMode ){
    rc = pagerExclusiveLock(pPager);
  }

  /* Open the connection to the log file. If this operation fails, 
  ** (e.g. due to malloc() failure), return an error code.
      打开连接到日志文件。如果这个操作失败，返回错误代码。
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
** The caller must be holding a SHARED lock on the database file to call
** this function.
** 使用者必须在数据库文件保留一个共享锁来调用这个函数。
** If the pager passed as the first argument is open on a real database
** file (not a temp file or an in-memory database), and the WAL file
** is not already open, make an attempt to open it now. If successful,
** return SQLITE_OK. If an error occurs or the VFS used by the pager does 
** not support the xShmXXX() methods, return an error code. *pbOpen is
** not modified in either case.
** 如果在打开的真正的数据库文件中(不是临时文件或内存中的数据库），
   pager作为第一个参数传递,WAL文件没有打开,现在试图打开它。如果成功，则返回SQLITE_O。
   如果发生错误或者pager使用的VFS不支持xShmXXX()方法，返回错误代码。在任何情况下，*pbOpen不被修改。
** If the pager is open on a temp-file (or in-memory database), or if
** the WAL file is already open, set *pbOpen to 1 and return SQLITE_OK
** without doing anything.
   如果在内存数据库打开pager，或者是否打开WAL文件，设置 *pbOpen为1并且返回SQLITE_OK不进行任何操作。
*/
int sqlite3PagerOpenWal(
  Pager *pPager,                  /* Pager object  Pager对象 */
  int *pbOpen                     /* OUT: Set to true if call is a no-op 如果调用时隔空操作，设置为TRUE*/
){
  int rc = SQLITE_OK;             /* Return code 返回代码 */

  assert( assert_pager_state(pPager) );
  assert( pPager->eState==PAGER_OPEN   || pbOpen );
  assert( pPager->eState==PAGER_READER || !pbOpen );
  assert( pbOpen==0 || *pbOpen==0 );
  assert( pbOpen!=0 || (!pPager->tempFile && !pPager->pWal) );

  if( !pPager->tempFile && !pPager->pWal ){
    if( !sqlite3PagerWalSupported(pPager) ) return SQLITE_CANTOPEN;

    /* Close any rollback journal previously open 开放之前关闭任何日志恢复*/
    sqlite3OsClose(pPager->jfd);

    rc = pagerOpenWal(pPager);
    if( rc==SQLITE_OK ){
      pPager->journalMode = PAGER_JOURNALMODE_WAL;
      pPager->eState = PAGER_OPEN;
    }
  }else{
    *pbOpen = 1;
  }

  return rc;
}

/*
** This function is called to close the connection to the log file prior
** to switching from WAL to rollback mode.
** 调用这个函数之前关闭到日志文件的连接，从WAL切换到恢复模式。
** Before closing the log file, this function attempts to take an 
** EXCLUSIVE lock on the database file. If this cannot be obtained, an
** error (SQLITE_BUSY) is returned and the log connection is not closed.
** If successful, the EXCLUSIVE lock is not released before returning.
   在关闭记录文件之前，这个函数试图从数据库文件获取排它锁。如果没有获取，返回错误，记录连接不会关闭。
   如果成功，排它锁在返回之前不被释放。
*/
int sqlite3PagerCloseWal(Pager *pPager){
  int rc = SQLITE_OK;

  assert( pPager->journalMode==PAGER_JOURNALMODE_WAL );

  /* If the log file is not already open, but does exist in the file-system,
  ** it may need to be checkpointed before the connection can switch to
  ** rollback mode. Open it now so this can happen.
     如果没有打开记录文件，但是它存在于文件系统中，在连接被切换到恢复模式之前它需要检查。
     打开它就可以执行。
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
     检查并且关闭记录。因为数据库文件中有排它锁，这个记录和记录概要文件就将被删除。
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

#ifdef SQLITE_ENABLE_ZIPVFS
/*
** A read-lock must be held on the pager when this function is called. If
** the pager is in WAL mode and the WAL file currently contains one or more
** frames, return the size in bytes of the page images stored within the
** WAL frames. Otherwise, if this is not a WAL database or the WAL file
** is empty, return 0.
   当调用这个函数的时候，读锁必须被保留在pager中。如果pager在WAL模式下，WAL文件当前包含一个或者更多的框架，
   返回储存在WAL框架上页面图像字节的大小。否则，如果这不是一个WAL数据库或者WAL文件为空，则返回0。
*/
int sqlite3PagerWalFramesize(Pager *pPager){
  assert( pPager->eState==PAGER_READER );
  return sqlite3WalFramesize(pPager->pWal);
}
#endif

#ifdef SQLITE_HAS_CODEC
/*
** This function is called by the wal module when writing page content
** into the log file.
** 当写页面满足于日志文件，这个函数被WAL模块调用。
** This function returns a pointer to a buffer containing the encrypted
** page content. If a malloc fails, this function may return NULL.
   这个函数返回一个包含加密页面内容的缓冲器指针。如果分配内存失败，这个函数会返回空。
*/
void *sqlite3PagerCodec(PgHdr *pPg){
  void *aData = 0;
  CODEC2(pPg->pPager, pPg->pData, pPg->pgno, 6, return 0, aData);
  return aData;
}
#endif /* SQLITE_HAS_CODEC */

#endif /* !SQLITE_OMIT_WAL */

#endif /* SQLITE_OMIT_DISKIO */
