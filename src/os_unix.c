/*
** 2004 May 22
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
** This file contains the VFS implementation for unix-like operating systems
** include Linux, MacOSX, *BSD, QNX, VxWorks, AIX, HPUX, and others.
**这个文件包含包括Linux,MacOSX,* BSD,QNX,VxWorks,AIX,HPUX等类unix操作系统的VFS实现。
**
** There are actually several different VFS implementations in this file.
** The differences are in the way that file locking is done.  The default
** implementation uses Posix Advisory Locks.  Alternative implementations
** use flock(), dot-files, various proprietary locking schemas, or simply
** skip locking all together.
**实际上在这个文件中有几种不同的VFS实现(VFS的作用就是采用标准的Unix系统调
**用读写位于不同物理介质上的不同文件系统。VFS是一个可以让open()、read()、write()
**等系统调用不用关心底层的存储介质和文件系统类型就可以工作的粘合层。)
**差异在于文件加锁的方式。默认的实现是使用Posix咨询锁。替代实现使用flock(),dot文件,
**各种专有锁定模式,或者只是跳过锁定在一起.
**
** This source file is organized into divisions where the logic for various
** subfunctions is contained within the appropriate division.  PLEASE
** KEEP THE STRUCTURE OF THIS FILE INTACT.  New code should be placed
** in the correct division and should be clearly labeled.
**这些源文件组成了包含了各种逻辑子函数的分区.请保持这个文件结构的完整
**新代码应放置在正确的分区,并且都应有明确的标示。

**
** The layout of divisions is as follows:
**
**   *  General-purpose declarations and utility functions.  通用的声明和实用功能
**   *  Unique file ID logic used by VxWorks.   VxWorks 所使用的唯一的文件 ID 逻辑
**   *  Various locking primitive implementations (all except proxy locking):  锁定原语的实现（除了代理锁定）
**      + for Posix Advisory Locks   Posix咨询锁 
**      + for no-op locks		无操作锁
**      + for dot-file locks		点文件锁
**      + for flock() locking	聚集锁
**      + for named semaphore locks (VxWorks only)  命名信号锁（只存在于VxWorks）
**      + for AFP filesystem locks (MacOSX only)		AFP系统文件锁（只存在于MacOSX）
**   *  sqlite3_file methods not associated with locking. sqlite3_file方法和加锁没有关系
**   *  Definitions of sqlite3_io_methods objects for all locking
**      methods plus "finder" functions for each locking method.
**      sqlite3_io_methods对象的定义为所有锁定方法增加了"finder"功能
**   *  sqlite3_vfs method implementations. 
**      sqlite3_vfs方法实现
**   *  Locking primitives for the proxy uber-locking-method. (MacOSX only)
**       proxy uber-locking-method的锁定原语（只在MacOSX上有）
**
**   *  Definitions of sqlite3_vfs objects for all locking methods
**      plus implementations of sqlite3_os_init() and sqlite3_os_end().
**      为所有加锁方法定义了sqlite3_vfs对象，增加了sqlite3_os_init() 和 sqlite3_os_end()的实现.
*/
#include "sqliteInt.h"
#if SQLITE_OS_UNIX              /* This file is used on unix only */  //这个文件只用于unix

/*
** There are various methods for file locking used for concurrency   多种用于并发控制的文件加锁的方法
** control:
**
**   1. POSIX locking (the default),
**   2. No locking,
**   3. Dot-file locking,
**   4. flock() locking,
**   5. AFP locking (OSX only),
**   6. Named POSIX semaphores (VXWorks only),
**   7. proxy locking. (OSX only)
**
** Styles 4, 5, and 7 are only available of SQLITE_ENABLE_LOCKING_STYLE
** is defined to 1.  The SQLITE_ENABLE_LOCKING_STYLE also enables automatic
** selection of the appropriate locking style based on the filesystem
** where the database is located.  
** 4、5、7只可用定义为1的SQLITE_ENABLE_LOCKING_STYLE。SQLITE_ENABLE_LOCKING_STYLE
** 还支持基于数据库所在的文件系统自动选择适当的加锁风格。
*/
#if !defined(SQLITE_ENABLE_LOCKING_STYLE)
#  if defined(__APPLE__)
#    define SQLITE_ENABLE_LOCKING_STYLE 1
#  else
#    define SQLITE_ENABLE_LOCKING_STYLE 0
#  endif
#endif

/*
** Define the OS_VXWORKS pre-processor macro to 1 if building on 
** vxworks, or 0 otherwise.
如果基于vxworks，定义OS_VXWORKS预处理器宏为1，否则为0
*/
#ifndef OS_VXWORKS
#  if defined(__RTP__) || defined(_WRS_KERNEL)
#    define OS_VXWORKS 1
#  else
#    define OS_VXWORKS 0
#  endif
#endif

/*
** These #defines should enable >2GB file support on Posix if the
** underlying operating system supports it.  If the OS lacks
** large file support, these should be no-ops.
** 这些#define应该使>2GB的文件支持Posix，如果底层操作系统支持的话。如果操作系统不支持大文件,就应该无操作
**
** Large file support can be disabled using the -DSQLITE_DISABLE_LFS switch
** on the compiler command line.  This is necessary if you are compiling
** on a recent machine (ex: RedHat 7.2) but you want your code to work
** on an older machine (ex: RedHat 6.0).  If you compile on RedHat 7.2
** without this option, LFS is enable.  But LFS does not exist in the kernel
** in RedHat 6.0, so the code won't work.  Hence, for maximum binary
** portability you should omit LFS.
** 支持大文件可以使用编译器命令行上的-DSQLITE_DISABLE_LFS 开关禁用。这是必要的，
** 如果你正在一个最近的机器上进行编译（如:RedHat 7.2）但是又希望代码在较旧的机器上工作（如:RedHat 6.0）。
** 如果你在RedHat 7.2上编译，但没有这个选项,LFS启用（Linux from Scratch，就是一种从网上直接下载源码，
** 从头编译LINUX的安装方式）。但LFS并不存在于RedHat6.0的内核,所以代码不能工作。因此,为了达到最大的二进制可移植性，
** 应该省略LFS。
**
** The previous paragraph was written in 2005.  (This paragraph is written
** on 2008-11-28.) These days, all Linux kernels support large files, so
** you should probably leave LFS enabled.  But some embedded platforms might
** lack LFS in which case the SQLITE_DISABLE_LFS macro might still be useful.
** 前一段是在2005年写的。(这一段是在2008-11-28写的)这些天来,所有的Linux内核都支持大文件,所以你应该放弃启用LFS。
** 但是一些嵌入式平台可能缺乏LFS，在这种情况下,SQLITE_DISABLE_LFS宏可能仍然是有用的。
*/
#ifndef SQLITE_DISABLE_LFS
# define _LARGE_FILE       1
# ifndef _FILE_OFFSET_BITS
#   define _FILE_OFFSET_BITS 64
# endif
# define _LARGEFILE_SOURCE 1
#endif

/*
** standard include files.标准包含文件
*/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#ifndef SQLITE_OMIT_WAL
#include <sys/mman.h>
#endif


#if SQLITE_ENABLE_LOCKING_STYLE
# include <sys/ioctl.h>
# if OS_VXWORKS
#  include <semaphore.h>
#  include <limits.h>
# else
#  include <sys/file.h>
#  include <sys/param.h>
# endif
#endif /* SQLITE_ENABLE_LOCKING_STYLE */

#if defined(__APPLE__) || (SQLITE_ENABLE_LOCKING_STYLE && !OS_VXWORKS)
# include <sys/mount.h>
#endif

#ifdef HAVE_UTIME
# include <utime.h>
#endif

/*
** Allowed values of unixFile.fsFlags         unixFile.fsFlags允许的值
*/
#define SQLITE_FSFLAGS_IS_MSDOS     0x1

/*
** If we are to be thread-safe, include the pthreads header and define
** the SQLITE_UNIX_THREADS macro.
** 如果希望线程安全，包括线程头部并且定义SQLITE_UNIX_THREADS宏
*/
#if SQLITE_THREADSAFE
# include <pthread.h>
# define SQLITE_UNIX_THREADS 1
#endif

/*
** Default permissions when creating a new file  创建一个新文件时的默认权限
*/
#ifndef SQLITE_DEFAULT_FILE_PERMISSIONS
# define SQLITE_DEFAULT_FILE_PERMISSIONS 0644
#endif

/*
** Default permissions when creating auto proxy dir  创建自动代理路径时的默认权限
*/
#ifndef SQLITE_DEFAULT_PROXYDIR_PERMISSIONS
# define SQLITE_DEFAULT_PROXYDIR_PERMISSIONS 0755
#endif

/*
** Maximum supported path-length.     路径长度支持的最大值
*/
#define MAX_PATHNAME 512

/*
** Only set the lastErrno if the error code is a real error and not 
** a normal expected return code of SQLITE_BUSY or SQLITE_OK
** 只能设置lastErrno，如果错误代码是一个真正的错误，而不是一个正常的预期的SQLITE_BUSY或SQLITE_OK返回代码
*/
#define IS_LOCK_ERROR(x)  ((x != SQLITE_OK) && (x != SQLITE_BUSY))

/* Forward references */    //前向引用
typedef struct unixShm unixShm;               /* Connection shared memory */ //连接共享内存
typedef struct unixShmNode unixShmNode;       /* Shared memory instance */  //共享内存实例
typedef struct unixInodeInfo unixInodeInfo;   /* An i-node */ //一个i节点
typedef struct UnixUnusedFd UnixUnusedFd;     /* An unused file descriptor */ //一个未使用的文件描述符

/*
** Sometimes, after a file handle is closed by SQLite, the file descriptor
** cannot be closed immediately. In these cases, instances of the following
** structure are used to store the file descriptor while waiting for an
** opportunity to either close or reuse it.
** 有时，SQLite 关闭文件句柄后，不能立即关闭文件描述符。在这些情况下，下面的结构的实例用于存储的文件描述符，
** 同时等待机会，要么关闭或重用它。
*/
struct UnixUnusedFd {
  int fd;                   /* File descriptor to close */  //要关闭的文件描述符
  int flags;                /* Flags this file descriptor was opened with */  //打开此文件描述符的标志
  UnixUnusedFd *pNext;      /* Next unused file descriptor on same file */  //同一文件的下一个未使用的文件描述符
};

/*
** The unixFile structure is subclass of sqlite3_file specific to the unix
** VFS implementations.
** unixFile 结构体是 sqlite3_file 特定于 unix VFS 的子类实现
*/
typedef struct unixFile unixFile;
struct unixFile {
  sqlite3_io_methods const *pMethod;  /* Always the first entry */  //总是第一个进入
  sqlite3_vfs *pVfs;                  /* The VFS that created this unixFile */  //创建这个unixFile的VFS
  unixInodeInfo *pInode;              /* Info about locks on this inode */  //关于这个i节点上的锁的消息
  int h;                              /* The file descriptor */ //文件描述符
  unsigned char eFileLock;            /* The type of lock held on this fd */  //fd（上面定义的文件描述符）上加锁的类型
  unsigned short int ctrlFlags;       /* Behavioral bits.  UNIXFILE_* flags */  //行为位
  int lastErrno;                      /* The unix errno from last I/O error */  //最后一个输入/输出错误的unix错误
  void *lockingContext;               /* Locking style specific state */  //加锁类型特定的状态
  UnixUnusedFd *pUnused;              /* Pre-allocated UnixUnusedFd */  //预分配的UnixUnusedFd
  const char *zPath;                  /* Name of the file */  //文件名
  unixShm *pShm;                      /* Shared memory segment information */ //共享内存段的信息
  int szChunk;                        /* Configured by FCNTL_CHUNK_SIZE */  //由 FCNTL_CHUNK_SIZE 配置
#if SQLITE_ENABLE_LOCKING_STYLE
  int openFlags;                      /* The flags specified at open() */ //指定的open()标志
#endif
#if SQLITE_ENABLE_LOCKING_STYLE || defined(__APPLE__)
  unsigned fsFlags;                   /* cached details from statfs() */  //statfs() 的缓存的详细信息
#endif
#if OS_VXWORKS
  struct vxworksFileId *pId;          /* Unique file ID */  //唯一的文件ID
#endif
#ifdef SQLITE_DEBUG
  /* The next group of variables are used to track whether or not the
  ** transaction counter in bytes 24-27 of database files are updated
  ** whenever any part of the database changes.  An assertion fault will
  ** occur if a file is updated without also updating the transaction
  ** counter.  This test is made to avoid new problems similar to the
  ** one described by ticket #3584. 
  **下一组变量用来跟踪数据库文件的24-27字节的事务计数器在数据库其他部分发生变化是否更新。如果一个文件
  **更新了但没有更新事务计数器，断言故障就会发生。进行此测试，是为了避免出现新的类似于标签#3584描述的问题。
  */
  unsigned char transCntrChng;   /* True if the transaction counter changed */  //如果事物计数器更改则为True 
  unsigned char dbUpdate;        /* True if any part of database file changed */  //如果数据库文件的任何部分发生了变化则为True 
  unsigned char inNormalWrite;   /* True if in a normal write operation */  //如果在一个正常的写操作内则为True 
#endif
#ifdef SQLITE_TEST
  /* In test mode, increase the size of this structure a bit so that 
  ** it is larger than the struct CrashFile defined in test6.c.
  ** 在测试模式下，增加一点此结构的大小，让它大于 CrashFile 在 test6.c 中定义的结构。
  */
  char aPadding[32];
#endif
};

/*
** Allowed values for the unixFile.ctrlFlags bitmask:
** unixFile.ctrlFlags位掩码允许的值 :
*/
#define UNIXFILE_EXCL        0x01     /* Connections from one process only */ //仅来自一个进程的连接
#define UNIXFILE_RDONLY      0x02     /* Connection is read only */ //仅读连接
#define UNIXFILE_PERSIST_WAL 0x04     /* Persistent WAL mode */ //持续的WAL模式
#ifndef SQLITE_DISABLE_DIRSYNC
# define UNIXFILE_DIRSYNC    0x08     /* Directory sync needed */ //所需的目录同步
#else
# define UNIXFILE_DIRSYNC    0x00
#endif
#define UNIXFILE_PSOW        0x10     /* SQLITE_IOCAP_POWERSAFE_OVERWRITE */
#define UNIXFILE_DELETE      0x20     /* Delete on close */   //关闭后删除
#define UNIXFILE_URI         0x40     /* Filename might have query parameters */  //文件名可能有查询参数
#define UNIXFILE_NOLOCK      0x80     /* Do no file locking */  //没有文件锁定
#define UNIXFILE_WARNED    0x0100     /* verifyDbFile() warnings have been issued */  //已发出的verifyDbFile() 警告

/*
** Include code that is common to all os_*.c files  包括了所有os_*.c文件通用的代码
*/
#include "os_common.h"

/*
** Define various macros that are missing from some systems. 定义了许多系统都缺少的各种宏
*/
#ifndef O_LARGEFILE
# define O_LARGEFILE 0
#endif
#ifdef SQLITE_DISABLE_LFS
# undef O_LARGEFILE
# define O_LARGEFILE 0
#endif
#ifndef O_NOFOLLOW
# define O_NOFOLLOW 0
#endif
#ifndef O_BINARY
# define O_BINARY 0
#endif

/*
** The threadid macro resolves to the thread-id or to 0.  Used for
** testing and debugging only.
** threadid 宏解析到线程 id 或 0.只用于测试和调试.
*/
#if SQLITE_THREADSAFE
#define threadid pthread_self()
#else
#define threadid 0
#endif

/*
** Different Unix systems declare open() in different ways.  Same use
** open(const char*,int,mode_t).  Others use open(const char*,int,...).
** The difference is important when using a pointer to the function.
**不同的 Unix 系统以不同的方式声明 open ()。同样使用open（ const char *，int mode_t）。
**其他的使用open (const char *，int，......)。当使用指向函数的指针时这些区别是很重要的。
**
** The safest way to deal with the problem is to always use this wrapper
** which always has the same well-defined interface.
**处理这个问题最安全的方式是始终使用这个总是具有相同的定义良好的接口的封装。
*/
static int posixOpen(const char *zFile, int flags, int mode){
  return open(zFile, flags, mode);
}

/*
** On some systems, calls to fchown() will trigger a message in a security
** log if they come from non-root processes.  So avoid calling fchown() if
** we are not running as root.
**在某些系统上，对 fchown() 的调用将触发在安全日志中的消息，如果他们来自非根进程。
**所以，如果我们不以 root 身份运行，要避免调用 fchown()。
*/
static int posixFchown(int fd, uid_t uid, gid_t gid){
  return geteuid() ? 0 : fchown(fd,uid,gid);
}

/* Forward reference */ //前向引用
static int openDirectory(const char*, int*);

/*
** Many system calls are accessed through pointer-to-functions so that
** they may be overridden at runtime to facilitate fault injection during
** testing and sandboxing.  The following array holds the names and pointers
** to all overrideable system calls.
**可以通过指针函数访问很多的系统调用，因为，他们可能会在运行时被重写，以便在测试时和沙盒中故障注入。
**下面的数组保存有可重写的系统调用的名称和指针到所。
*/
static struct unix_syscall {
  const char *zName;            /* Name of the system call */	//系统调用名称
  sqlite3_syscall_ptr pCurrent; /* Current value of the system call */	//系统调用的当前值
  sqlite3_syscall_ptr pDefault; /* Default value */ 	//默认值
} aSyscall[] = {
  { "open",         (sqlite3_syscall_ptr)posixOpen,  0  },
#define osOpen      ((int(*)(const char*,int,int))aSyscall[0].pCurrent)

  { "close",        (sqlite3_syscall_ptr)close,      0  },
#define osClose     ((int(*)(int))aSyscall[1].pCurrent)

  { "access",       (sqlite3_syscall_ptr)access,     0  },
#define osAccess    ((int(*)(const char*,int))aSyscall[2].pCurrent)

  { "getcwd",       (sqlite3_syscall_ptr)getcwd,     0  },
#define osGetcwd    ((char*(*)(char*,size_t))aSyscall[3].pCurrent)

  { "stat",         (sqlite3_syscall_ptr)stat,       0  },
#define osStat      ((int(*)(const char*,struct stat*))aSyscall[4].pCurrent)

/*
** The DJGPP compiler environment looks mostly like Unix, but it
** lacks the fcntl() system call.  So redefine fcntl() to be something
** that always succeeds.  This means that locking does not occur under
** DJGPP.  But it is DOS - what did you expect?
**DJGPP 编译器环境看起来很像Unix，但它缺少 fcntl() 系统调用。所以重新定义 fcntl() 的时候总是会成功。
**这意味着锁定不会出现在 DJGPP 之下。但它可以是DOS-
*/
#ifdef __DJGPP__
  { "fstat",        0,                 0  },
#define osFstat(a,b,c)    0
#else     
  { "fstat",        (sqlite3_syscall_ptr)fstat,      0  },
#define osFstat     ((int(*)(int,struct stat*))aSyscall[5].pCurrent)
#endif

  { "ftruncate",    (sqlite3_syscall_ptr)ftruncate,  0  },
#define osFtruncate ((int(*)(int,off_t))aSyscall[6].pCurrent)

  { "fcntl",        (sqlite3_syscall_ptr)fcntl,      0  },
#define osFcntl     ((int(*)(int,int,...))aSyscall[7].pCurrent)

  { "read",         (sqlite3_syscall_ptr)read,       0  },
#define osRead      ((ssize_t(*)(int,void*,size_t))aSyscall[8].pCurrent)

#if defined(USE_PREAD) || SQLITE_ENABLE_LOCKING_STYLE
  { "pread",        (sqlite3_syscall_ptr)pread,      0  },
#else
  { "pread",        (sqlite3_syscall_ptr)0,          0  },
#endif
#define osPread     ((ssize_t(*)(int,void*,size_t,off_t))aSyscall[9].pCurrent)

#if defined(USE_PREAD64)
  { "pread64",      (sqlite3_syscall_ptr)pread64,    0  },
#else
  { "pread64",      (sqlite3_syscall_ptr)0,          0  },
#endif
#define osPread64   ((ssize_t(*)(int,void*,size_t,off_t))aSyscall[10].pCurrent)

  { "write",        (sqlite3_syscall_ptr)write,      0  },
#define osWrite     ((ssize_t(*)(int,const void*,size_t))aSyscall[11].pCurrent)

#if defined(USE_PREAD) || SQLITE_ENABLE_LOCKING_STYLE
  { "pwrite",       (sqlite3_syscall_ptr)pwrite,     0  },
#else
  { "pwrite",       (sqlite3_syscall_ptr)0,          0  },
#endif
#define osPwrite    ((ssize_t(*)(int,const void*,size_t,off_t))\
                    aSyscall[12].pCurrent)

#if defined(USE_PREAD64)
  { "pwrite64",     (sqlite3_syscall_ptr)pwrite64,   0  },
#else
  { "pwrite64",     (sqlite3_syscall_ptr)0,          0  },
#endif
#define osPwrite64  ((ssize_t(*)(int,const void*,size_t,off_t))\
                    aSyscall[13].pCurrent)

#if SQLITE_ENABLE_LOCKING_STYLE
  { "fchmod",       (sqlite3_syscall_ptr)fchmod,     0  },
#else
  { "fchmod",       (sqlite3_syscall_ptr)0,          0  },
#endif
#define osFchmod    ((int(*)(int,mode_t))aSyscall[14].pCurrent)

#if defined(HAVE_POSIX_FALLOCATE) && HAVE_POSIX_FALLOCATE
  { "fallocate",    (sqlite3_syscall_ptr)posix_fallocate,  0 },
#else
  { "fallocate",    (sqlite3_syscall_ptr)0,                0 },
#endif
#define osFallocate ((int(*)(int,off_t,off_t))aSyscall[15].pCurrent)

  { "unlink",       (sqlite3_syscall_ptr)unlink,           0 },
#define osUnlink    ((int(*)(const char*))aSyscall[16].pCurrent)

  { "openDirectory",    (sqlite3_syscall_ptr)openDirectory,      0 },
#define osOpenDirectory ((int(*)(const char*,int*))aSyscall[17].pCurrent)

  { "mkdir",        (sqlite3_syscall_ptr)mkdir,           0 },
#define osMkdir     ((int(*)(const char*,mode_t))aSyscall[18].pCurrent)

  { "rmdir",        (sqlite3_syscall_ptr)rmdir,           0 },
#define osRmdir     ((int(*)(const char*))aSyscall[19].pCurrent)

  { "fchown",       (sqlite3_syscall_ptr)posixFchown,     0 },
#define osFchown    ((int(*)(int,uid_t,gid_t))aSyscall[20].pCurrent)

  { "umask",        (sqlite3_syscall_ptr)umask,           0 },
#define osUmask     ((mode_t(*)(mode_t))aSyscall[21].pCurrent)

}; /* End of the overrideable system calls */ 	//可重写系统调用结束

/*
** This is the xSetSystemCall() method of sqlite3_vfs for all of the
** "unix" VFSes.  Return SQLITE_OK opon successfully updating the
** system call pointer, or SQLITE_NOTFOUND if there is no configurable
** system call named zName.
**这是sqlite3_vfs中所有"unix" VFSes的xSetSystemCall()方法。成功地更新系统调用指针并返回SQLITE_OK，
**如果有是没有名为zName的可配置的系统调用就返回SQLITE_NOTFOUND。
*/
static int unixSetSystemCall(
  sqlite3_vfs *pNotUsed,        /* The VFS pointer.  Not used */  //VFS 的指针。不使用
  const char *zName,          /* Name of system call to override */ //系统调用重写的名称
  sqlite3_syscall_ptr pNewFunc  /* Pointer to new system call value */  //指向新的系统调用值的指针
){
  unsigned int i;
  int rc = SQLITE_NOTFOUND;

  UNUSED_PARAMETER(pNotUsed);
  if( zName==0 ){
    /* If no zName is given, restore all system calls to their default
    ** settings and return NULL.
    ** 如果给出没有zName（系统调用的名字），所有系统调用还原为其默认设置并且返回 NULL
    */
    rc = SQLITE_OK;
    for(i=0; i<sizeof(aSyscall)/sizeof(aSyscall[0]); i++){
      if( aSyscall[i].pDefault ){
        aSyscall[i].pCurrent = aSyscall[i].pDefault;
      }
    }
  }else{
    /* If zName is specified, operate on only the one system call
    ** specified.
    ** 如果指定了zName，只能运行在一个指定的系统调用上
    */
    for(i=0; i<sizeof(aSyscall)/sizeof(aSyscall[0]); i++){
      if( strcmp(zName, aSyscall[i].zName)==0 ){
        if( aSyscall[i].pDefault==0 ){
          aSyscall[i].pDefault = aSyscall[i].pCurrent;
        }
        rc = SQLITE_OK;
        if( pNewFunc==0 ) pNewFunc = aSyscall[i].pDefault;
        aSyscall[i].pCurrent = pNewFunc;
        break;
      }
    }
  }
  return rc;
}

/*
** Return the value of a system call.  Return NULL if zName is not a
** recognized system call name.  NULL is also returned if the system call
** is currently undefined.
**返回系统调用的值。如果zName不是一个被认可的系统调用名称，返回NULL。如果系统调用目前未定义也返回NULL。
*/
static sqlite3_syscall_ptr unixGetSystemCall(
  sqlite3_vfs *pNotUsed,
  const char *zName
){
  unsigned int i;

  UNUSED_PARAMETER(pNotUsed);
  for(i=0; i<sizeof(aSyscall)/sizeof(aSyscall[0]); i++){
    if( strcmp(zName, aSyscall[i].zName)==0 ) return aSyscall[i].pCurrent;
  }
  return 0;
}

/*
** Return the name of the first system call after zName.  If zName==NULL
** then return the name of the first system call. Return NULL if zNameis the
** last system call or if zName is not the name of a valid system call.
**在zName后返回的第一次系统调用的名称。如果zName==NULL，则返回第一次系统调用的名称。如果zName是最后一次的系统调用，
**或者zName不是一个有效的系统调用的名称，请返回NULL。
*/
static const char *unixNextSystemCall(sqlite3_vfs *p, const char *zName){
  int i = -1;

  UNUSED_PARAMETER(p);
  if( zName ){
    for(i=0; i<ArraySize(aSyscall)-1; i++){
      if( strcmp(zName, aSyscall[i].zName)==0 ) break;
    }
  }
  for(i++; i<ArraySize(aSyscall); i++){
    if( aSyscall[i].pCurrent!=0 ) return aSyscall[i].zName;
  }
  return 0;
}

/*
** Invoke open().  Do so multiple times, until it either succeeds or
** fails for some reason other than EINTR.
**调用 open ()。重复做多次，直到它不是成功就是因为EINTR以外的某种原因失败(EINTR:linux中函数的返回状态，在不同的函数中意义不同)
**
** If the file creation mode "m" is 0 then set it to the default for
** SQLite.  The default is SQLITE_DEFAULT_FILE_PERMISSIONS (normally
** 0644) as modified by the system umask.  If m is not 0, then
** make the file creation mode be exactly m ignoring the umask.
**如果文件的创建模式"m"为0，那么将它设置为 SQLite默认的。按系统 umask（umask:设置了用户创建文件的默认权限）
**将默认值修改为SQLITE_DEFAULT_FILE_PERMISSIONS（通常 0644）。如果 m 不为0，然后将文件的创建模式设为完全忽略umask 的 m。
**
** The m parameter will be non-zero only when creating -wal, -journal,
** and -shm files.  We want those files to have *exactly* the same
** permissions as their original database, unadulterated by the umask.
** In that way, if a database file is -rw-rw-rw or -rw-rw-r-, and a
** transaction crashes and leaves behind hot journals, then any
** process that is able to write to the database will also be able to
** recover the hot journals.
**只有在创建后缀名为-wal、-journal和-shm 文件时，m 参数将为非零。我们希望这些文件有和他们原始的数据库“完全”相同的权限，纯粹由 umask 设置。
**在这种方式中，如果数据库文件权限是-rw-rw-rw 或-rw-rw-r-和交易崩溃留下的热日志，则任何能写入到数据库的程序也能恢复热日志.
*/
static int robust_open(const char *z, int f, mode_t m){
  int fd;
  mode_t m2;
  mode_t origM = 0;
  if( m==0 ){
    m2 = SQLITE_DEFAULT_FILE_PERMISSIONS;
  }else{
    m2 = m;
    origM = osUmask(0);
  }
  do{
#if defined(O_CLOEXEC)
    fd = osOpen(z,f|O_CLOEXEC,m2);
#else
    fd = osOpen(z,f,m2);
#endif
  }while( fd<0 && errno==EINTR );
  if( m ){
    osUmask(origM);
  }
#if defined(FD_CLOEXEC) && (!defined(O_CLOEXEC) || O_CLOEXEC==0)
  if( fd>=0 ) osFcntl(fd, F_SETFD, osFcntl(fd, F_GETFD, 0) | FD_CLOEXEC);
#endif
  return fd;
}

/*
** Helper functions to obtain and relinquish the global mutex. The
** global mutex is used to protect the unixInodeInfo and
** vxworksFileId objects used by this file, all of which may be 
** shared by multiple threads.
**获得和放弃全局互斥锁的 helper 函数。全局互斥锁用于保护使用此文件中，所有的一**切都可以由多个线程共享的 unixInodeInfo 和 vxworksFileId 的对象。
**
** Function unixMutexHeld() is used to assert() that the global mutex 
** is held when required. This function is only used as part of assert() 
** statements. e.g.
**当有需要时，函数 unixMutexHeld() 用于使用全局互斥锁时所需的assert() 函数。**此函数仅用作 assert() 语句的一部分。例如：
**
**   unixEnterMutex()
**     assert( unixMutexHeld() );
**   unixEnterLeave()
*/
static void unixEnterMutex(void){
  sqlite3_mutex_enter(sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER));
}
static void unixLeaveMutex(void){
  sqlite3_mutex_leave(sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER));
}
#ifdef SQLITE_DEBUG
static int unixMutexHeld(void) {
  return sqlite3_mutex_held(sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER));
}
#endif


#if defined(SQLITE_TEST) && defined(SQLITE_DEBUG)
/*
** Helper function for printing out trace information from debugging
** binaries. This returns the string represetation of the supplied
** integer lock-type.
**Helper函数打印来自调试二进制文件的追踪信息。这里的返回值是提供的整型的加锁类型的字符串表示形式
*/
static const char *azFileLock(int eFileLock){
  switch( eFileLock ){
    case NO_LOCK: return "NONE";
    case SHARED_LOCK: return "SHARED";
    case RESERVED_LOCK: return "RESERVED";
    case PENDING_LOCK: return "PENDING";
    case EXCLUSIVE_LOCK: return "EXCLUSIVE";
  }
  return "ERROR";
}
#endif

#ifdef SQLITE_LOCK_TRACE
/*
** Print out information about all locking operations.
**打印出所有加锁操作的信息
**
** This routine is used for troubleshooting locks on multithreaded
** platforms.  Enable by compiling with the -DSQLITE_LOCK_TRACE
** command-line option on the compiler.  This code is normally
** turned off.
**这个程序用于在多线程的平台上的疑难解答锁。通过编译编译器上的-DSQLITE_LOCK_TRACE 命令行选项启用。此代码通常是关闭的。
*/
static int lockTrace(int fd, int op, struct flock *p){
  char *zOpName, *zType;
  int s;
  int savedErrno;
  if( op==F_GETLK ){
    zOpName = "GETLK";
  }else if( op==F_SETLK ){
    zOpName = "SETLK";
  }else{
    s = osFcntl(fd, op, p);
    sqlite3DebugPrintf("fcntl unknown %d %d %d\n", fd, op, s);
    return s;
  }
  if( p->l_type==F_RDLCK ){
    zType = "RDLCK";
  }else if( p->l_type==F_WRLCK ){
    zType = "WRLCK";
  }else if( p->l_type==F_UNLCK ){
    zType = "UNLCK";
  }else{
    assert( 0 );
  }
  assert( p->l_whence==SEEK_SET );
  s = osFcntl(fd, op, p);
  savedErrno = errno;
  sqlite3DebugPrintf("fcntl %d %d %s %s %d %d %d %d\n",
     threadid, fd, zOpName, zType, (int)p->l_start, (int)p->l_len,
     (int)p->l_pid, s);
  if( s==(-1) && op==F_SETLK && (p->l_type==F_RDLCK || p->l_type==F_WRLCK) ){
    struct flock l2;
    l2 = *p;
    osFcntl(fd, F_GETLK, &l2);
    if( l2.l_type==F_RDLCK ){
      zType = "RDLCK";
    }else if( l2.l_type==F_WRLCK ){
      zType = "WRLCK";
    }else if( l2.l_type==F_UNLCK ){
      zType = "UNLCK";
    }else{
      assert( 0 );
    }
    sqlite3DebugPrintf("fcntl-failure-reason: %s %d %d %d\n",
       zType, (int)l2.l_start, (int)l2.l_len, (int)l2.l_pid);
  }
  errno = savedErrno;
  return s;
}
#undef osFcntl
#define osFcntl lockTrace
#endif /* SQLITE_LOCK_TRACE */

/*
** Retry ftruncate() calls that fail due to EINTR    重试由于EINTR失败的ftruncate()调用
*/
static int robust_ftruncate(int h, sqlite3_int64 sz){
  int rc;
  do{ rc = osFtruncate(h,sz); }while( rc<0 && errno==EINTR );
  return rc;
}

/*
** This routine translates a standard POSIX errno code into something
** useful to the clients of the sqlite3 functions.  Specifically, it is
** intended to translate a variety of "try again" errors into SQLITE_BUSY
** and a variety of "please close the file descriptor NOW" errors into 
** SQLITE_IOERR
** 这个程序将标准的POSIX错误代码转化成 sqlite3 功能的客户端有用的东西。具体来说，
** 它将被翻译成各种"重试"错误为SQLITE_BUSY 和各种"请立即关闭文件描述符"错误为SQLITE_IOERR
** Errors during initialization of locks, or file system support for locks,
** should handle ENOLCK, ENOTSUP, EOPNOTSUPP separately.
** 锁或文件系统支持的锁的初始化期间发生的错误应分开成ENOLCK，ENOTSUP，EOPNOTSUPP来处理。
*/ 
static int sqliteErrorFromPosixError(int posixError, int sqliteIOErr) {
  switch (posixError) {
#if 0
  /* At one point this code was not commented out. In theory, this branch
  ** should never be hit, as this function should only be called after
  ** a locking-related function (i.e. fcntl()) has returned non-zero with
  ** the value of errno as the first argument. Since a system call has failed,
  ** errno should be non-zero.
  **在一个点此代码没有注释掉。理论上讲，这一部分应该永远不会命中，因为这个函数只能在一个锁定关系函数
  **（如 fcnt1()）返回errno的非零值作为第一个参数后被调用。由于系统调用失败，errno 应为非零值。
  **
  ** Despite this, if errno really is zero, we still don't want to return
  ** SQLITE_OK. The system call failed, and *some* SQLite error should be
  ** propagated back to the caller. Commenting this branch out means errno==0
  ** will be handled by the "default:" case below.
  **尽管如此，如果errno真的是零,我们仍然不想返回SQLITE_OK。系统调用失败,“一些”SQLite错误应该被传回给调用者。
  **评价这个分支意味着errno = = 0时将通过下面“默认的”情况处理。
  */
  case 0: 
    return SQLITE_OK;
#endif

  case EAGAIN:
  case ETIMEDOUT:
  case EBUSY:
  case EINTR:
  case ENOLCK:  
    /* random NFS retry error, unless during file system support 
     * introspection, in which it actually means what it says */
     /*随机NFS重试错误，除非在文件系统支持自省期间,否则这实际上就意味着错误所说的内容。*/
    return SQLITE_BUSY;
    
  case EACCES: 
    /* EACCES is like EAGAIN during locking operations, but not any other time*/
    /* 在锁定操作期间EACCES就像EAGAIN，但没有任何其他时间*/
    if( (sqliteIOErr == SQLITE_IOERR_LOCK) || 
        (sqliteIOErr == SQLITE_IOERR_UNLOCK) || 
        (sqliteIOErr == SQLITE_IOERR_RDLOCK) ||
        (sqliteIOErr == SQLITE_IOERR_CHECKRESERVEDLOCK) ){
      return SQLITE_BUSY;
    }
    /* else fall through */ //其他的的失败
  case EPERM: 
    return SQLITE_PERM;
    
  /* EDEADLK is only possible if a call to fcntl(F_SETLKW) is made. And
  ** this module never makes such a call. And the code in SQLite itself 
  ** asserts that SQLITE_IOERR_BLOCKED is never returned. For these reasons
  ** this case is also commented out. If the system does set errno to EDEADLK,
  ** the default SQLITE_IOERR_XXX code will be returned. */
  /* 如果调用fcntl(F_SETLKW)，EDEADLK 是唯一的可能。且此模块永远不会再做这个调用。
  并且自己的SQLite代码维护SQLITE_IOERR_BLOCKED永远不会返回。基于这些原因，这个case也被注释掉。
  如果该系统将 errno 设置为 EDEADLK，将返回默认的 SQLITE_IOERR_XXX 代码。*/
#if 0
  case EDEADLK:
    return SQLITE_IOERR_BLOCKED;
#endif
    
#if EOPNOTSUPP!=ENOTSUP
  case EOPNOTSUPP: 
    /* something went terribly awry, unless during file system support 
     * introspection, in which it actually means what it says */
    /*出现严重失误，除非在文件系统支持自省期间，否则这实际上意味着错误所说的内容*/
#endif
#ifdef ENOTSUP
  case ENOTSUP: 
    /* invalid fd, unless during file system support introspection, in which 
     * it actually means what it says */
     /*无效的fd，除非在文件系统支持自省期间，否则这实际上意味着错误所说的内容*/
#endif
  case EIO:
  case EBADF:
  case EINVAL:
  case ENOTCONN:
  case ENODEV:
  case ENXIO:
  case ENOENT:
#ifdef ESTALE                     /* ESTALE is not defined on Interix systems */ //ESTALE不是在Interix系统上定义的
  case ESTALE:
#endif
  case ENOSYS:
    /* these should force the client to close the file and reconnect */   //应该强制客户端关闭该文件，然后重新连接
    
  default: 
    return sqliteIOErr;
  }
}



/******************************************************************************
****************** Begin Unique File ID Utility Used By VxWorks ***************
**
** On most versions of unix, we can get a unique ID for a file by concatenating
** the device number and the inode number.  But this does not work on VxWorks.
** On VxWorks, a unique file id must be based on the canonical filename.
**对大多数版本的 unix 中，我们可以得到一个唯一的文件ID，通过串联设备数量和i节点数量。
**但这不能用在VxWorks.在VxWorks上，一个唯一的文件 id 必须基于规范的文件名。
**
** A pointer to an instance of the following structure can be used as a
** unique file ID in VxWorks.  Each instance of this structure contains
** a copy of the canonical filename.  There is also a reference count.  
** The structure is reclaimed when the number of pointers to it drops to
** zero.
**一个指向下列结构体的一个实例指针可以用作 VxWorks 中的一个唯一的文件 ID。这种结构体的每个实例包含一份规范的文件名。
**还有一个引用计数。当指针指向它的数目降到零时回收结构体。
**
** There are never very many files open at one time and lookups are not
** a performance-critical path, so it is sufficient to put these
** structures on a linked list.
**有很多文件从来不会打开一次，查找不是性能关键的路径，所以它足以将这些结构放在一个链表上。
*/
struct vxworksFileId {
  struct vxworksFileId *pNext;  /* Next in a list of them all */		//指向链表中的下一个
  int nRef;                     /* Number of references to this one */	//对这一引用的次数
  int nName;               /* Length of the zCanonicalName[] string */	//zCanonicalName[]字符串的长度
  char *zCanonicalName;         /* Canonical filename */		//规范的文件名
};

#if OS_VXWORKS
/* 
** All unique filenames are held on a linked list headed by this
** variable:
*/
static struct vxworksFileId *vxworksFileList = 0;

/*
** Simplify a filename into its canonical form
** by making the following changes:
** 通过进行以下更改可以简化一个文件名为其规范的形式
**
**  *  removing any trailing and duplicate /
**  *  convert /./ into just /
**  *  convert /A/../ where A is any simple name into just /
**  * 删除任何尾部的和重复的 /
    * 将/./ 转换为 /
    * 将/A /../ (A 只是任何简单的名称)转换成 /
**
** Changes are made in-place.  Return the new name length.
** 就地进行更改。返回新的名称长度。
** The original filename is in z[0..n-1].  Return the number of
** characters in the simplified name.
** 原来的文件名在z[0..n-1]中。返回简化的名称的字符的数目。
*/
static int vxworksSimplifyName(char *z, int n){
  int i, j;
  while( n>1 && z[n-1]=='/' ){ n--; }
  for(i=j=0; i<n; i++){
    if( z[i]=='/' ){
      if( z[i+1]=='/' ) continue;
      if( z[i+1]=='.' && i+2<n && z[i+2]=='/' ){
        i += 1;
        continue;
      }
      if( z[i+1]=='.' && i+3<n && z[i+2]=='.' && z[i+3]=='/' ){
        while( j>0 && z[j-1]!='/' ){ j--; }
        if( j>0 ){ j--; }
        i += 2;
        continue;
      }
    }
    z[j++] = z[i];
  }
  z[j] = 0;
  return j;
}

/*
** Find a unique file ID for the given absolute pathname.  Return
** a pointer to the vxworksFileId object.  This pointer is the unique
** file ID.
**查找唯一的一个给定绝对路径名的文件ID。返回一个vxworksFileId 对象的指针。这个指针是唯一的文件ID。
**
** The nRef field of the vxworksFileId object is incremented before
** the object is returned.  A new vxworksFileId object is created
** and added to the global list if necessary.
**在返回对象之前VxworksFileId 对象的 nRef 字段是递增的。如有必要，创建一个新的 vxworksFileId 对象并将其添加到全局列表。
** If a memory allocation error occurs, return NULL.
**如果出现内存分配错误，返回 NULL
*/
static struct vxworksFileId *vxworksFindFileId(const char *zAbsoluteName){
  struct vxworksFileId *pNew;         /* search key and new file ID */	//搜索键和新的文件 ID
  struct vxworksFileId *pCandidate;   /* For looping over existing file ID*/	//用于遍历现有的文件ID
  int n;                              /* Length of zAbsoluteName string */	// ZAbsoluteName 字符串的长度

  assert( zAbsoluteName[0]=='/' );
  n = (int)strlen(zAbsoluteName);
  pNew = sqlite3_malloc( sizeof(*pNew) + (n+1) );
  if( pNew==0 ) return 0;
  pNew->zCanonicalName = (char*)&pNew[1];
  memcpy(pNew->zCanonicalName, zAbsoluteName, n+1);
  n = vxworksSimplifyName(pNew->zCanonicalName, n);

  /* Search for an existing entry that matching the canonical name.
  ** If found, increment the reference count and return a pointer to
  ** the existing file ID.
  ** 搜索现有的匹配规范名称的条目。如果发现,增加引用计数并返回一个指向现有文件ID的指针。
  */
  unixEnterMutex();
  for(pCandidate=vxworksFileList; pCandidate; pCandidate=pCandidate->pNext){
    if( pCandidate->nName==n 
     && memcmp(pCandidate->zCanonicalName, pNew->zCanonicalName, n)==0
    ){
       sqlite3_free(pNew);
       pCandidate->nRef++;
       unixLeaveMutex();
       return pCandidate;
    }
  }

  /* No match was found.  We will make a new file ID */ //找不到匹配的。我们将建立新的文件ID
  pNew->nRef = 1;
  pNew->nName = n;
  pNew->pNext = vxworksFileList;
  vxworksFileList = pNew;
  unixLeaveMutex();
  return pNew;
}

/*
** Decrement the reference count on a vxworksFileId object.  Free
** the object when the reference count reaches zero.
** 减小对vxworksFileId对象的引用计数。当引用计数减至0时释放对象
*/
static void vxworksReleaseFileId(struct vxworksFileId *pId){
  unixEnterMutex();
  assert( pId->nRef>0 );
  pId->nRef--;
  if( pId->nRef==0 ){
    struct vxworksFileId **pp;
    for(pp=&vxworksFileList; *pp && *pp!=pId; pp = &((*pp)->pNext)){}
    assert( *pp==pId );
    *pp = pId->pNext;
    sqlite3_free(pId);
  }
  unixLeaveMutex();
}
#endif /* OS_VXWORKS */
/*************** End of Unique File ID Utility Used By VxWorks ****************
******************************************************************************/


/******************************************************************************
*************************** Posix Advisory Locking ****************************
**
** POSIX advisory locks are broken by design.  ANSI STD 1003.1 (1996)
** section 6.5.2.2 lines 483 through 490 specify that when a process
** sets or clears a lock, that operation overrides any prior locks set
** by the same process.  It does not explicitly say so, but this implies
** that it overrides locks set by the same process using a different
** file descriptor.  Consider this test case:
**POSIX咨询锁是靠设计打破的。ANSI标准1003.1(1996)部分6.5.2.2 483到490行规定，
**当一个进程设置或者清除一个锁的时候，这个操作将会覆盖任何之前由相同进程设置的锁。
**它并没有明确地说明，但是这意味着它将覆盖通过同一进程使用一个不同的文件描述符设置的锁。请考虑此测试用例
**
**       int fd1 = open("./file1", O_RDWR|O_CREAT, 0644);
**       int fd2 = open("./file2", O_RDWR|O_CREAT, 0644);
**
** Suppose ./file1 and ./file2 are really the same file (because
** one is a hard or symbolic link to the other) then if you set
** an exclusive lock on fd1, then try to get an exclusive lock
** on fd2, it works.  I would have expected the second lock to
** fail since there was already a lock on the file due to fd1.
** But not so.  Since both locks came from the same process, the
** second overrides the first, even though they were on different
** file descriptors opened on different file names.
**假设./file1和./file2实际上是同一个文件（因为一个是硬链接或符号链接到另一个），
**然后如果你在fd1设置排它锁,然后试图在fd2获得排它锁,它的工作原理。我预期第二锁定失败,因为由于fd1文件已经锁定。
**但不是这样的。因为锁来自相同的进程,第二个覆盖第一个,尽管他们在不同的文件名打开的不同的文件描述符上。
**
** This means that we cannot use POSIX locks to synchronize file access
** among competing threads of the same process.  POSIX locks will work fine
** to synchronize access for threads in separate processes, but not
** threads within the same process.
**这意味着我们不能使用 POSIX 锁来同步竞争同一进程的线程之间的文件访问。POSIX锁可以同步不同进程的线程的访问，
**但不能同步同一进程的线程访问。
**
** To work around the problem, SQLite has to manage file locks internally
** on its own.  Whenever a new database is opened, we have to find the
** specific inode of the database file (the inode is determined by the
** st_dev and st_ino fields of the stat structure that fstat() fills in)
** and check for locks already existing on that inode.  When locks are
** created or removed, we have to look at our own internal record of the
** locks to see if another thread has previously set a lock on that same
** inode.
**若想解决这一问题，SQLite必须在其内部管理文件锁定。每当打开一个新的数据库,我们必须找到特定的数据库文件的
**i节点（这个i节点由st_dev和st_ino字段的统计结构的fstat()函数填写），并且检查这个锁已经存在在这个i节点上了。
**当创建或删除锁，我们要看看我们自己内部的锁记录，看是否有另一个线程先在相同的i节点上设置了锁。
**
** (Aside: The use of inode numbers as unique IDs does not work on VxWorks.
** For VxWorks, we have to use the alternative unique ID system based on
** canonical filename and implemented in the previous division.)
**顺便说句：不能在VxWorks上使用i节点编号作为唯一的ID。对于VxWorks，我们必须使用基于规范的文件名，并且在原先的部分实施.
**
** The sqlite3_file structure for POSIX is no longer just an integer file
** descriptor.  It is now a structure that holds the integer file
** descriptor and a pointer to a structure that describes the internal
** locks on the corresponding inode.  There is one locking structure
** per inode, so if the same inode is opened twice, both unixFile structures
** point to the same locking structure.  The locking structure keeps
** a reference count (so we will know when to delete it) and a "cnt"
** field that tells us its internal lock status.  cnt==0 means the
** file is unlocked.  cnt==-1 means the file has an exclusive lock.
** cnt>0 means there are cnt shared locks on the file.
** POSIX的sqlite3_file结构不再仅仅是一个整型的文件描述符。它现在是一个包含整型的文件描述符和指向描述内部相应的
** i节点上的锁的结构的指针。每个i节点有一个锁结构，所以如果同一个inode打开两次, 两个unixFile结构指向同一个锁结构。
** 锁定结构保持引用计数(因此,我们将知道什么时候删除它)并且“cnt”字段告诉我们其内部锁状态。
** cnt==0意味着文件没有加锁。cnt=-1意味着文件上有一个排它锁。cnt>0意味着该文件上有cnt共享锁。
**
** Any attempt to lock or unlock a file first checks the locking
** structure.  The fcntl() system call is only invoked to set a 
** POSIX lock if the internal lock structure transitions between
** a locked and an unlocked state.
**任何企图要锁定或解锁文件首先检查锁结构。如果内部锁结构在锁定和解除锁定的状态之间转换，
**才会调用 fcntl() 系统调用设置 POSIX 锁。

** But wait:  there are yet more problems with POSIX advisory locks.
** POSIX 咨询锁还是存在一些问题
**
** If you close a file descriptor that points to a file that has locks,
** all locks on that file that are owned by the current process are
** released.  To work around this problem, each unixInodeInfo object
** maintains a count of the number of pending locks on tha inode.
** When an attempt is made to close an unixFile, if there are
** other unixFile open on the same inode that are holding locks, the call
** to close() the file descriptor is deferred until all of the locks clear.
** The unixInodeInfo structure keeps a list of file descriptors that need to
** be closed and that list is walked (and cleared) when the last lock
** clears.
** 如果你关闭一个指向已锁定的文件的文件描述符，由当前进程对该文件的设置的所有锁都会释放。
** 为了解决这个问题，每个 unixInodeInfo 对象维护在该i节点上挂起的锁的数目的计数。
** 当试图关闭一个unixFile,如果在在该持有锁的i节点上有其他unixFile打开，close()关闭文件描述符的调用将推迟，
** 直到所有的锁都清除。unixInodeInfo结构体保存一个需要关闭的文件描述符列表,并且最后一个锁清除时该列表也将移除（清除）。
**
** Yet another problem:  LinuxThreads do not play well with posix locks.
**另外一个问题：LinuxThreads不能与posix locks很好地工作
**
** Many older versions of linux use the LinuxThreads library which is
** not posix compliant.  Under LinuxThreads, a lock created by thread
** A cannot be modified or overridden by a different thread B.
** Only thread A can modify the lock.  Locking behavior is correct
** if the appliation uses the newer Native Posix Thread Library (NPTL)
** on linux - with NPTL a lock created by thread A can override locks
** in thread B.  But there is no way to know at compile-time which
** threading library is being used.  So there is no way to know at
** compile-time whether or not thread A can override locks on thread B.
** One has to do a run-time check to discover the behavior of the
** current process.
** 许多旧版本的linux使用LinuxThreads库是不兼容posix的。在LinuxThreads下，线程A创建的锁不能由线程B修改或者重写。
** 只有线程A可以修改这个锁。如果应用程序在Linux上使用新的本机Posix线程库（NPTL），锁定行为是正确的，
** 使用NPTL线程A创建的锁可以重写线程B中的锁。但是没有办法知道在编译时使用的是哪一种线程库。
** 所以没有办法知道在编译时线程A是否可以重写线程B上的锁。必需在运行时检查发现当前进程的行为。
**
** SQLite used to support LinuxThreads.  But support for LinuxThreads
** was dropped beginning with version 3.7.0.  SQLite will still work with
** LinuxThreads provided that (1) there is no more than one connection 
** per database file in the same process and (2) database connections
** do not move across threads.
** SQLite过去支持Linux线程。但从3.7.0版本开始就删除了对Linux线程的支持,SQLite 只规定
** (1)在同一进程中每个数据库文件没有超过一个以上的连接,并且(2)数据库连接不会跨线程移动的情况下，仍然会与以支持。 
*/

/*
** An instance of the following structure serves as the key used
** to locate a particular unixInodeInfo object.
** 以下结构体的一个实例用于查找一个特定的 unixInodeInfo 对象的键。
*/
struct unixFileId {
  dev_t dev;                  /* Device number */		//设备号
#if OS_VXWORKS
  struct vxworksFileId *pId;  /* Unique file ID for vxworks. */	//用于 vxworks 的唯一的文件 ID
#else
  ino_t ino;                  /* Inode number */		//i节点数目
#endif
};

/*
** An instance of the following structure is allocated for each open
** inode.  Or, on LinuxThreads, there is one of these structures for
** each inode opened by each thread.
** 下面结构体的一个实例分配给每个打开的i节点。或者，在Linux线程中，这些结构体中有一个是给由线程打开的i节点的。
**
** A single inode can have multiple file descriptors, so each unixFile
** structure contains a pointer to an instance of this object and this
** object keeps a count of the number of unixFile pointing to it.
** 单一的i节点可以有多个文件描述符，因此每个 unixFile 结构包含一个指针，指向该对象的一个实例，
** 此对象保持 unixFile 指向它的数目的计数。
*/
struct unixInodeInfo {
  struct unixFileId fileId;       /* The lookup key */	//查找键
  int nShared;                    /* Number of SHARED locks held */	//持有共享锁的数目
  unsigned char eFileLock;        /* One of SHARED_LOCK, RESERVED_LOCK etc. */
  unsigned char bProcessLock;     /* An exclusive process lock is held */	//独占进程锁
  int nRef;                       /* Number of pointers to this structure */	//指向这个结构体的指针数目
  unixShmNode *pShmNode;          /* Shared memory associated with this inode */	//与这个i节点有关的共享内存
  int nLock;                      /* Number of outstanding file locks */  //未完成的文件锁定的数目
  UnixUnusedFd  *pUnused;         /* Unused file descriptors to close */	//关闭未使用的文件描述符
  unixInodeInfo *pNext;           /* List of all unixInodeInfo objects */	//所有的 unixInodeInfo 对象的列表
  unixInodeInfo *pPrev;           /*    .... doubly linked */	//双重链接
#if SQLITE_ENABLE_LOCKING_STYLE
  unsigned long long sharedByte;  /* for AFP simulated shared lock */	//用于模拟APF共享锁
#endif
#if OS_VXWORKS
  sem_t *pSem;                    /* Named POSIX semaphore */	//命名的 POSIX 信号量
  char aSemName[MAX_PATHNAME+2];  /* Name of that semaphore */	//该信号量的名称
#endif
};

/*
** A lists of all unixInodeInfo objects.  UnixInodeInfo 的所有对象的列表
*/
static unixInodeInfo *inodeList = 0;

/*
**
** This function - unixLogError_x(), is only ever called via the macro
** unixLogError().
**这个unixLogError_x()函数永远只能通过宏定义的unixLogError()函数调用
**
** It is invoked after an error occurs in an OS function and errno has been
** set. It logs a message using sqlite3_log() containing the current value of
** errno and, if possible, the human-readable equivalent from strerror() or
** strerror_r().
**在操作系统函数出现错误并且错误被设定之后调用它。它使用sqlite3_log()记录一条消息，消息包含当前errno的值，
**如果可能，认可读的等效来自于strerror() or strerror_r()
**
** The first argument passed to the macro should be the error code that
** will be returned to SQLite (e.g. SQLITE_IOERR_DELETE, SQLITE_CANTOPEN). 
** The two subsequent arguments should be the name of the OS function that
** failed (e.g. "unlink", "open") and the associated file-system path,
** if any.
**传递给宏的第一个参数应是将被返回到SQLite的（例如SQLITE_IOERR_DELETE，SQLITE_CANTOPEN）错误代码。
**随后的两个参数应该是失败的操作系统函数名称（例如"取消链接"，"打开"），以及相关的系统文件路径。
*/
#define unixLogError(a,b,c)     unixLogErrorAtLine(a,b,c,__LINE__)
static int unixLogErrorAtLine(
  int errcode,                    /* SQLite error code */   //SQLite错误代码
  const char *zFunc,              /* Name of OS function that failed */   //失败的操作系统函数名称
  const char *zPath,              /* File path associated with error */ //错误的文件路径关联
  int iLine                       /* Source line number where error occurred */ //发生错误的位置的源代码行号
){
  char *zErr;                     /* Message from strerror() or equivalent */ ///strerror()的或与之等效的消息
  int iErrno = errno;             /* Saved syscall error number */    //保存系统调用错误号

  /* If this is not a threadsafe build (SQLITE_THREADSAFE==0), then use
  ** the strerror() function to obtain the human-readable error message
  ** equivalent to errno. Otherwise, use strerror_r().
  **如果这不是一个线程安全构建(SQLITE_THREADSAFE == 0),则使用strerror()函数来获得相当于errno的人可读的错误消息。
  **否则,使用strerror_r()。
  */ 
#if SQLITE_THREADSAFE && defined(HAVE_STRERROR_R)
  char aErr[80];
  memset(aErr, 0, sizeof(aErr));
  zErr = aErr;

  /* If STRERROR_R_CHAR_P (set by autoconf scripts) or __USE_GNU is defined,
  ** assume that the system provides the GNU version of strerror_r() that
  ** returns a pointer to a buffer containing the error message. That pointer 
  ** may point to aErr[], or it may point to some static storage somewhere. 
  ** Otherwise, assume that the system provides the POSIX version of 
  ** strerror_r(), which always writes an error message into aErr[].
  **如果定义了STRERROR_R_CHAR_P（由 autoconf 脚本设置）或 __USE_GNU，
  **假定该系统提供的 GNU 版本的 strerror_r()它返回一个指向包含错误消息的缓冲区。这个指针可以指向aErr[]，
  **或者指向一些静态存储区域。否则，假设该系统提供的 POSIX 版本的 strerror_r()，总是将错误消息写入到 aErr []。
  **
  ** If the code incorrectly assumes that it is the POSIX version that is
  ** available, the error message will often be an empty string. Not a
  ** huge problem. Incorrectly concluding that the GNU version is available 
  ** could lead to a segfault though.
  **如果代码错误地假定它是可用的 POSIX 版本，错误消息通常会是一个空字符串。这不是一个大问题。
  **虽然不正确地结束 GNU 版本可用可能导致分段错误。
  */
#if defined(STRERROR_R_CHAR_P) || defined(__USE_GNU)
  zErr = 
# endif
  strerror_r(iErrno, aErr, sizeof(aErr)-1);

#elif SQLITE_THREADSAFE
  /* This is a threadsafe build, but strerror_r() is not available. *///这是一个线程安全的构建，但是strerror_r()是不可用的
  zErr = "";
#else
  /* Non-threadsafe build, use strerror(). */ //非线程安全的构建，使用strerror()
  zErr = strerror(iErrno);
#endif

  assert( errcode!=SQLITE_OK );
  if( zPath==0 ) zPath = "";
  sqlite3_log(errcode,
      "os_unix.c:%d: (%d) %s(%s) - %s",
      iLine, iErrno, zFunc, zPath, zErr
  );

  return errcode;
}

/*
** Close a file descriptor.
**关闭一个文件描述符
**
** We assume that close() almost always works, since it is only in a
** very sick application or on a very sick platform that it might fail.
** If it does fail, simply leak the file descriptor, but do log the
** error.
**因为只在一个非常恶心的应用程序或在一个非常恶心的平台,它可能会失败，**我们假设close()几乎总是工作的。如果它失败了,除了记录错误之外，仅仅**泄漏文件描述符。
**
** Note that it is not safe to retry close() after EINTR since the
** file descriptor might have already been reused by another thread.
** So we don't even try to recover from an EINTR.  Just log the error
** and move on.
**
**需要注意的是，由于文件描述符可能已经被另一个线程重新使用，在EINTR
**后重试close（）是不安全的。所以，我们甚至不尝试从EINTR恢复。只要记**录错误然后继续执行
*/
static void robust_close(unixFile *pFile, int h, int lineno){
  if( osClose(h) ){
    unixLogErrorAtLine(SQLITE_IOERR_CLOSE, "close",
                       pFile ? pFile->zPath : 0, lineno);
  }
}

/*
** Close all file descriptors accumuated in the unixInodeInfo->pUnused list.
**关闭所有存放在unixInodeInfo->pUnused列表中的文件描述符
*/ 
static void closePendingFds(unixFile *pFile){
  unixInodeInfo *pInode = pFile->pInode;
  UnixUnusedFd *p;
  UnixUnusedFd *pNext;
  for(p=pInode->pUnused; p; p=pNext){
    pNext = p->pNext;
    robust_close(pFile, p->fd, __LINE__);
    sqlite3_free(p);
  }
  pInode->pUnused = 0;
}

/*
** Release a unixInodeInfo structure previously allocated by findInodeInfo().
**释放之前findInodeInfo()分配的一个unixInodeInfo结构
**
** The mutex entered using the unixEnterMutex() function must be held
** when this function is called.
**当这个函数被调用时，必须使用unixEnterMutex（）函数输入的互斥量
*/
static void releaseInodeInfo(unixFile *pFile){
  unixInodeInfo *pInode = pFile->pInode;
  assert( unixMutexHeld() );
  if( ALWAYS(pInode) ){
    pInode->nRef--;
    if( pInode->nRef==0 ){
      assert( pInode->pShmNode==0 );
      closePendingFds(pFile);
      if( pInode->pPrev ){
        assert( pInode->pPrev->pNext==pInode );
        pInode->pPrev->pNext = pInode->pNext;
      }else{
        assert( inodeList==pInode );
        inodeList = pInode->pNext;
      }
      if( pInode->pNext ){
        assert( pInode->pNext->pPrev==pInode );
        pInode->pNext->pPrev = pInode->pPrev;
      }
      sqlite3_free(pInode);
    }
  }
}

/*
** Given a file descriptor, locate the unixInodeInfo object that
** describes that file descriptor.  Create a new one if necessary.  The
** return value might be uninitialized if an error occurs.
** 给定一个文件描述符,定位unixInodeInfo对象描述文件描述符。在必要时创建一个新的。如果出现错误,返回值可能是未初始化的。
**
** The mutex entered using the unixEnterMutex() function must be held
** when this function is called.
**当这个函数被调用时，必须使用unixEnterMutex（）函数输入的互斥量
**
** Return an appropriate error code.
**返回相应的错误代码
*/
static int findInodeInfo(
  unixFile *pFile,               /* Unix file with file desc used in the key */ //降序排列的Unix文件使用的键
  unixInodeInfo **ppInode        /* Return the unixInodeInfo object here */ //返回unixInodeInfo对象
){
  int rc;                        /* System call return code */    //系统调用返回代码
  int fd;                        /* The file descriptor for pFile */    //pFile的文件描述符
  struct unixFileId fileId;      /* Lookup key for the unixInodeInfo */ //unixInodeInfo的查找键
  struct stat statbuf;           /* Low-level file information */ //底层文件信息
  unixInodeInfo *pInode = 0;     /* Candidate unixInodeInfo object */ //候选的unixInodeInfo对象

  assert( unixMutexHeld() );

  /* Get low-level information about the file that we can used to
  ** create a unique name for the file.
  **获得底层文件信息,我们可以用来为该文件创建一个唯一的名称。
  */
  fd = pFile->h;
  rc = osFstat(fd, &statbuf);
  if( rc!=0 ){
    pFile->lastErrno = errno;
#ifdef EOVERFLOW
    if( pFile->lastErrno==EOVERFLOW ) return SQLITE_NOLFS;
#endif
    return SQLITE_IOERR;
  }

#ifdef __APPLE__
  /* On OS X on an msdos filesystem, the inode number is reported
  ** incorrectly for zero-size files.  See ticket #3260.  To work
  ** around this problem (we consider it a bug in OS X, not SQLite)
  ** we always increase the file size to 1 by writing a single byte
  ** prior to accessing the inode number.  The one byte written is
  ** an ASCII 'S' character which also happens to be the first byte
  ** in the header of every SQLite database.  In this way, if there
  ** is a race condition such that another thread has already populated
  ** the first page of the database, no damage is done.
  **在os x系统的msdos文件系统中,对于大小为0的文件,i节点会报错.见标签#3260.
  **为了解决这个问题(这是OS X系统的一个bug,而不是SQLite的问题),我们将写入一个字节到i节点中使得文件大小增加1.
  **这个字节写入一个ASCII 字符S，在每个SQLite数据库中文件头的第一个字节也会这样处理。通过这种方式，
  **如果出现多个进程进行竞争,如果其中一个进程已经占用了数据库的第一个页面,其他进程就会识别而不会对数据库造成破坏.
  */
  if( statbuf.st_size==0 && (pFile->fsFlags & SQLITE_FSFLAGS_IS_MSDOS)!=0 ){
    do{ rc = osWrite(fd, "S", 1); }while( rc<0 && errno==EINTR );
    if( rc!=1 ){
      pFile->lastErrno = errno;
      return SQLITE_IOERR;
    }
    rc = osFstat(fd, &statbuf);
    if( rc!=0 ){
      pFile->lastErrno = errno;
      return SQLITE_IOERR;
    }
  }
#endif

  memset(&fileId, 0, sizeof(fileId));
  fileId.dev = statbuf.st_dev;
#if OS_VXWORKS
  fileId.pId = pFile->pId;
#else
  fileId.ino = statbuf.st_ino;
#endif
  pInode = inodeList;
  while( pInode && memcmp(&fileId, &pInode->fileId, sizeof(fileId)) ){
    pInode = pInode->pNext;
  }
  if( pInode==0 ){
    pInode = sqlite3_malloc( sizeof(*pInode) );
    if( pInode==0 ){
      return SQLITE_NOMEM;
    }
    memset(pInode, 0, sizeof(*pInode));
    memcpy(&pInode->fileId, &fileId, sizeof(fileId));
    pInode->nRef = 1;
    pInode->pNext = inodeList;
    pInode->pPrev = 0;
    if( inodeList ) inodeList->pPrev = pInode;
    inodeList = pInode;
  }else{
    pInode->nRef++;
  }
  *ppInode = pInode;
  return SQLITE_OK;
}


/*
** This routine checks if there is a RESERVED lock held on the specified
** file by this or any other process. If such a lock is held, set *pResOut
** to a non-zero value otherwise *pResOut is set to zero.  The return value
** is set to SQLITE_OK unless an I/O error occurs during lock checking.
**这个例程检查这个进程或其他进程指定的文件是否持有保留锁。如果持有这样的锁，
**设置*pResOut为非零值，否则设置为0.返回值设置为SQLITE_OK，除非在锁定检查期间出现I/O错误。
*/
static int unixCheckReservedLock(sqlite3_file *id, int *pResOut){
  int rc = SQLITE_OK;
  int reserved = 0;
  unixFile *pFile = (unixFile*)id;

  SimulateIOError( return SQLITE_IOERR_CHECKRESERVEDLOCK; );

  assert( pFile );
  unixEnterMutex(); /* Because pFile->pInode is shared across threads */  //因为pFile->pInode在线程间是共享的

  /* Check if a thread in this process holds such a lock */	//检查是否这一进程中的线程持有此类锁
  if( pFile->pInode->eFileLock>SHARED_LOCK ){
    reserved = 1;
  }

  /* Otherwise see if some other process holds it.
  ** 否则，看看是否一些其他进程持有它。
  */
#ifndef __DJGPP__
  if( !reserved && !pFile->pInode->bProcessLock ){
    struct flock lock;
    lock.l_whence = SEEK_SET;
    lock.l_start = RESERVED_BYTE;
    lock.l_len = 1;
    lock.l_type = F_WRLCK;
    if( osFcntl(pFile->h, F_GETLK, &lock) ){
      rc = SQLITE_IOERR_CHECKRESERVEDLOCK;
      pFile->lastErrno = errno;
    } else if( lock.l_type!=F_UNLCK ){
      reserved = 1;
    }
  }
#endif
  
  unixLeaveMutex();
  OSTRACE(("TEST WR-LOCK %d %d %d (unix)\n", pFile->h, rc, reserved));

  *pResOut = reserved;
  return rc;
}

/*
** Attempt to set a system-lock on the file pFile.  The lock is 
** described by pLock.
**试图在文件pFile设置一个系统锁，这个锁由pLock描述。
**
** If the pFile was opened read/write from unix-excl, then the only lock
** ever obtained is an exclusive lock, and it is obtained exactly once
** the first time any lock is attempted.  All subsequent system locking
** operations become no-ops.  Locking operations still happen internally,
** in order to coordinate access between separate database connections
** within this process, but all of that is handled in memory and the
** operating system does not participate.
**如果pFile是从unix-excl打开读/写的,那么只有曾经获得的排它锁,这是第一次获得试图得到的任何的锁。
**所有后续系统锁定操作成为空操作。锁定操作仍然发生在内部,以便协调在这一进程内的单独的数据库连接之间的访问，
**但这一切都是在内存中处理，操作系统并不参与。
**
** This function is a pass-through to fcntl(F_SETLK) if pFile is using
** any VFS other than "unix-excl" or if pFile is opened on "unix-excl"
** and is read-only.
**这个函数传递到fcntl(F_SETLK)如果pFile使用了除了"unix-excl"之外的任何VFS，或者pFile 在"unix-excl"上是打开的并且是只读的。
**
** Zero is returned if the call completes successfully, or -1 if a call
** to fcntl() fails. In this case, errno is set appropriately (by fcntl()).
**如果调用成功完成，返回零，或-1，如果调用fcntl()失败。在这种情况下，errno是设置正确(通过fcntl())。
*/
static int unixFileLock(unixFile *pFile, struct flock *pLock){
  int rc;
  unixInodeInfo *pInode = pFile->pInode;
  assert( unixMutexHeld() );
  assert( pInode!=0 );
  if( ((pFile->ctrlFlags & UNIXFILE_EXCL)!=0 || pInode->bProcessLock)
   && ((pFile->ctrlFlags & UNIXFILE_RDONLY)==0)
  ){
    if( pInode->bProcessLock==0 ){
      struct flock lock;
      assert( pInode->nLock==0 );
      lock.l_whence = SEEK_SET;
      lock.l_start = SHARED_FIRST;
      lock.l_len = SHARED_SIZE;
      lock.l_type = F_WRLCK;
      rc = osFcntl(pFile->h, F_SETLK, &lock);
      if( rc<0 ) return rc;
      pInode->bProcessLock = 1;
      pInode->nLock++;
    }else{
      rc = 0;
    }
  }else{
    rc = osFcntl(pFile->h, F_SETLK, pLock);
  }
  return rc;
}

/*
** Lock the file with the lock specified by parameter eFileLock - one	
** of the following:
**对文件进行由参数eFileLock指定的下列加锁
**
**     (1) SHARED_LOCK 共享锁
**     (2) RESERVED_LOCK保留锁
**     (3) PENDING_LOCK未决锁
**     (4) EXCLUSIVE_LOCK排它锁
**
** Sometimes when requesting one lock state, additional lock states
** are inserted in between.  The locking might fail on one of the later
** transitions leaving the lock state different from what it started but
** still short of its goal.  The following chart shows the allowed
** transitions and the inserted intermediate states:
**有时，请求一个锁定状态时，会插入额外的锁定状态。之后的转换让锁的状态不同于它开始的状态，锁定可能失败，达不到预期的目标。
**下面的图表显示了允许的转换和插入的中间状态
**
**    UNLOCKED -> SHARED 未加锁->共享
**    SHARED -> RESERVED  共享->保留
**    SHARED -> (PENDING) -> EXCLUSIVE 共享->（未决）->排它
**    RESERVED -> (PENDING) -> EXCLUSIVE 保留->（未决）->排它
**    PENDING -> EXCLUSIVE 未决->排它
**
** This routine will only increase a lock.  Use the sqlite3OsUnlock()
** routine to lower a locking level.
** 这个程序只会增加锁。使用sqlite3OsUnlock()程序以降低锁定级别。
*/
static int unixLock(sqlite3_file *id, int eFileLock){
  /* The following describes the implementation of the various locks and
  ** lock transitions in terms of the POSIX advisory shared and exclusive
  ** lock primitives (called read-locks and write-locks below, to avoid
  ** confusion with SQLite lock names). The algorithms are complicated
  ** slightly in order to be compatible with windows systems simultaneously
  ** accessing the same database file, in case that is ever required.
  **下面描述了各种锁的执行和锁在POSIX咨询共享锁和排它锁之间转换的原语(以下称为读锁和写锁，以避免混淆SQLite的锁名称)。
  **算法略显复杂，以便兼容与windows系统同时访问同一数据库文件，如果有需要的话。
  **
  ** Symbols defined in os.h indentify the 'pending byte' and the 'reserved
  ** byte', each single bytes at well known offsets, and the 'shared byte
  ** range', a range of 510 bytes at a well known offset.
  **在os.h中定义的符号识别 'pending byte' and the 'reserved byte'，每个字节在总所周知的偏移处，
  **'shared byte range'在总所周知的偏移处的510个字节范围内。
  **
  ** To obtain a SHARED lock, a read-lock is obtained on the 'pending
  ** byte'.  If this is successful, a random byte from the 'shared byte
  ** range' is read-locked and the lock on the 'pending byte' released.
  **为了获得共享锁，要在'pending byte'获得读锁。如果成功了，从'shared byte range' 得到的一个随机字节读锁定，
  **并且'pending byte' 上的锁移除。
  **
  ** A process may only obtain a RESERVED lock after it has a SHARED lock.
  ** A RESERVED lock is implemented by grabbing a write-lock on the
  ** 'reserved byte'. 
  **在有一个共享锁后，一个进程可能只能获得保留的锁。通过获取'reserved byte'上的写锁执行这个保留锁。
  **
  ** A process may only obtain a PENDING lock after it has obtained a
  ** SHARED lock. A PENDING lock is implemented by obtaining a write-lock
  ** on the 'pending byte'. This ensures that no new SHARED locks can be
  ** obtained, but existing SHARED locks are allowed to persist. A process
  ** does not have to obtain a RESERVED lock on the way to a PENDING lock.
  ** This property is used by the algorithm for rolling back a journal file
  ** after a crash.
  **在已经获得了共享锁后，一个进程可能只获得一个未决锁。通过获得'pending byte'的写锁执行这个未决锁。
  **这确保了不能获得新的共享锁，但现有的共享锁还是允许继续下去。一个进程在获取未决锁的过程中不必获取共享锁。
  **该算法使用此属性在崩溃后回滚日志文件。
  **
  ** An EXCLUSIVE lock, obtained after a PENDING lock is held, is
  ** implemented by obtaining a write-lock on the entire 'shared byte
  ** range'. Since all other locks require a read-lock on one of the bytes
  ** within this range, this ensures that no other locks are held on the
  ** database. 
  **在持有未决锁后获得的排它锁通过获得整个'shared byte range'上的写锁实现。
  **从之所有其他锁后需要在这个范围内的字节上有读锁，这可以确保数据库上没有其他锁。
  **
  ** The reason a single byte cannot be used instead of the 'shared byte
  ** range' is that some versions of windows do not support read-locks. By
  ** locking a random byte from a range, concurrent SHARED locks may exist
  ** even if the locking primitive used is always a write-lock.
  **单个的字节不能代替'shared byte range'的原因是很多Windows版本不支持读锁。
  **通过锁定一个范围内的一个随机字节，可能会存在并发共享，即使锁定原语使用的一直是一个写锁。
  */
  int rc = SQLITE_OK;
  unixFile *pFile = (unixFile*)id;
  unixInodeInfo *pInode;
  struct flock lock;
  int tErrno = 0;

  assert( pFile );
  OSTRACE(("LOCK    %d %s was %s(%s,%d) pid=%d (unix)\n", pFile->h,
      azFileLock(eFileLock), azFileLock(pFile->eFileLock),
      azFileLock(pFile->pInode->eFileLock), pFile->pInode->nShared , getpid()));

  /* If there is already a lock of this type or more restrictive on the
  ** unixFile, do nothing. Don't use the end_lock: exit path, as
  ** unixEnterMutex() hasn't been called yet.
如果已经存在此类型的锁或者unixfile上有更强的限制性，什么都不用做。不要使用end_lock:退出路径，也不调用 unixEnterMutex()。
  */
  if( pFile->eFileLock>=eFileLock ){
    OSTRACE(("LOCK    %d %s ok (already held) (unix)\n", pFile->h,
            azFileLock(eFileLock)));
    return SQLITE_OK;
  }

  /* Make sure the locking sequence is correct.确保锁定序列是正确的。
  **  (1) We never move from unlocked to anything higher than shared lock.
  **  (2) SQLite never explicitly requests a pendig lock.
  **  (3) A shared lock is always held when a reserve lock is requested.
  **  (1) 不要从未加锁移动到高于共享锁的状态。
  **  (2)SQLite从未明确要求pendig锁。
  **  (3)当请求一个保留锁时，共享锁总是持有的。
  */
  assert( pFile->eFileLock!=NO_LOCK || eFileLock==SHARED_LOCK );
  assert( eFileLock!=PENDING_LOCK );
  assert( eFileLock!=RESERVED_LOCK || pFile->eFileLock==SHARED_LOCK );

  /* This mutex is needed because pFile->pInode is shared across threads
  **需要这个互斥信号量，因为 pFile-> pInode 在线程之间是共享的
  */
  unixEnterMutex();
  pInode = pFile->pInode;

  /* If some thread using this PID has a lock via a different unixFile*
  ** handle that precludes the requested lock, return BUSY.
  **如果某个使用此PID的线程通过一个不同的unixFile* handle，即排除请求的锁，获得了一个锁，返回BUSY。
  */
  if( (pFile->eFileLock!=pInode->eFileLock && 
          (pInode->eFileLock>=PENDING_LOCK || eFileLock>SHARED_LOCK))
  ){
    rc = SQLITE_BUSY;
    goto end_lock;
  }

  /* If a SHARED lock is requested, and some thread using this PID already
  ** has a SHARED or RESERVED lock, then increment reference counts and
  ** return SQLITE_OK.
  ** 如果请求共享锁时，使用此PID的一些线程已经有一个共享锁或保留锁，则增加引用计数并返回SQLITE_OK。
  */
  if( eFileLock==SHARED_LOCK && 
      (pInode->eFileLock==SHARED_LOCK || pInode->eFileLock==RESERVED_LOCK) ){
    assert( eFileLock==SHARED_LOCK );
    assert( pFile->eFileLock==0 );
    assert( pInode->nShared>0 );
    pFile->eFileLock = SHARED_LOCK;
    pInode->nShared++;
    pInode->nLock++;
    goto end_lock;
  }


  /* A PENDING lock is needed before acquiring a SHARED lock and before
  ** acquiring an EXCLUSIVE lock.  For the SHARED lock, the PENDING will
  ** be released.
  在获取共享锁和排它锁之前必须有一个未决锁。获得了共享锁，该未决锁将被移除。
  */
  lock.l_len = 1L;
  lock.l_whence = SEEK_SET;
  if( eFileLock==SHARED_LOCK 
      || (eFileLock==EXCLUSIVE_LOCK && pFile->eFileLock<PENDING_LOCK)
  ){
    lock.l_type = (eFileLock==SHARED_LOCK?F_RDLCK:F_WRLCK);
    lock.l_start = PENDING_BYTE;
    if( unixFileLock(pFile, &lock) ){
      tErrno = errno;
      rc = sqliteErrorFromPosixError(tErrno, SQLITE_IOERR_LOCK);
      if( rc!=SQLITE_BUSY ){
        pFile->lastErrno = tErrno;
      }
      goto end_lock;
    }
  }


  /* If control gets to this point, then actually go ahead and make
  ** operating system calls for the specified lock.
  **如果控制达到这一点,那么实际上继续让操作系统调用指定的锁。
  */
  if( eFileLock==SHARED_LOCK ){
    assert( pInode->nShared==0 );
    assert( pInode->eFileLock==0 );
    assert( rc==SQLITE_OK );

    /* Now get the read-lock */ //获取读锁
    lock.l_start = SHARED_FIRST;
    lock.l_len = SHARED_SIZE;
    if( unixFileLock(pFile, &lock) ){
      tErrno = errno;
      rc = sqliteErrorFromPosixError(tErrno, SQLITE_IOERR_LOCK);
    }

    /* Drop the temporary PENDING lock */ //删除临时的未决锁
    lock.l_start = PENDING_BYTE;
    lock.l_len = 1L;
    lock.l_type = F_UNLCK;
    if( unixFileLock(pFile, &lock) && rc==SQLITE_OK ){
      /* This could happen with a network mount */    //这可能在网络加载时发生
      tErrno = errno;
      rc = SQLITE_IOERR_UNLOCK; 
    }

    if( rc ){
      if( rc!=SQLITE_BUSY ){
        pFile->lastErrno = tErrno;
      }
      goto end_lock;
    }else{
      pFile->eFileLock = SHARED_LOCK;
      pInode->nLock++;
      pInode->nShared = 1;
    }
  }else if( eFileLock==EXCLUSIVE_LOCK && pInode->nShared>1 ){
    /* We are trying for an exclusive lock but another thread in this
    ** same process is still holding a shared lock. 
    **我们尝试获取排它锁，但是在这一过程中另一个线程一直持有共享锁。
    */
    rc = SQLITE_BUSY;
  }else{
    /* The request was for a RESERVED or EXCLUSIVE lock.  It is
    ** assumed that there is a SHARED or greater lock on the file
    ** already.
    ** 请求保留锁或独占锁。假设已经有一个共享锁或文件有更高级的了
    */
    assert( 0!=pFile->eFileLock );
    lock.l_type = F_WRLCK;

    assert( eFileLock==RESERVED_LOCK || eFileLock==EXCLUSIVE_LOCK );
    if( eFileLock==RESERVED_LOCK ){
      lock.l_start = RESERVED_BYTE;
      lock.l_len = 1L;
    }else{
      lock.l_start = SHARED_FIRST;
      lock.l_len = SHARED_SIZE;
    }

    if( unixFileLock(pFile, &lock) ){
      tErrno = errno;
      rc = sqliteErrorFromPosixError(tErrno, SQLITE_IOERR_LOCK);
      if( rc!=SQLITE_BUSY ){
        pFile->lastErrno = tErrno;
      }
    }
  }
  

#ifdef SQLITE_DEBUG
  /* Set up the transaction-counter change checking flags when
  ** transitioning from a SHARED to a RESERVED lock.  The change
  ** from SHARED to RESERVED marks the beginning of a normal
  ** write operation (not a hot journal rollback).
  **设置事务计数器改变检查标记，当从共享锁转换到保留锁时。
  **从共享锁转换到保留锁标志着一个正常的写操作（不是热日志回滚）开始了。
  */
  if( rc==SQLITE_OK
   && pFile->eFileLock<=SHARED_LOCK
   && eFileLock==RESERVED_LOCK
  ){
    pFile->transCntrChng = 0;
    pFile->dbUpdate = 0;
    pFile->inNormalWrite = 1;
  }
#endif


  if( rc==SQLITE_OK ){
    pFile->eFileLock = eFileLock;
    pInode->eFileLock = eFileLock;
  }else if( eFileLock==EXCLUSIVE_LOCK ){
    pFile->eFileLock = PENDING_LOCK;
    pInode->eFileLock = PENDING_LOCK;
  }

end_lock:
  unixLeaveMutex();
  OSTRACE(("LOCK    %d %s %s (unix)\n", pFile->h, azFileLock(eFileLock), 
      rc==SQLITE_OK ? "ok" : "failed"));
  return rc;
}

/*
** Add the file descriptor used by file handle pFile to the corresponding
** pUnused list.
**添加文件句柄pFile使用的文件描述符到相应的pUnused列表。
*/
static void setPendingFd(unixFile *pFile){
  unixInodeInfo *pInode = pFile->pInode;
  UnixUnusedFd *p = pFile->pUnused;
  p->pNext = pInode->pUnused;
  pInode->pUnused = p;
  pFile->h = -1;
  pFile->pUnused = 0;
}

/*
** Lower the locking level on file descriptor pFile to eFileLock.  eFileLock
** must be either NO_LOCK or SHARED_LOCK.
**降低文件描述符pFile上的锁定级别为eFileLock。eFileLock必须是NO_LOCK或SHARED_LOCK。
**
** If the locking level of the file descriptor is already at or below
** the requested locking level, this routine is a no-op.
** 如果文件描述符的锁定级别已经达到或低于所请求的锁定级别，此例程是一个空操作。
**
** If handleNFSUnlock is true, then on downgrading an EXCLUSIVE_LOCK to SHARED
** the byte range is divided into 2 parts and the first part is unlocked then
** set to a read lock, then the other part is simply unlocked.  This works 
** around a bug in BSD NFS lockd (also seen on MacOSX 10.3+) that fails to 
** remove the write lock on a region when a read lock is set.
**如果handleNFSUnlock为真，则降低一个EXCLUSIVE_LOCK为共享锁，其字节范围分为两部分，
**一部分是为加锁然后设置为读锁，另一部分则是简单的未加锁。这一错误存在DSB NFS 加锁（在MacOSX 10.3+中也见过），
**即当设置了一个读锁就无法在该区域删除写锁。
*/
static int posixUnlock(sqlite3_file *id, int eFileLock, int handleNFSUnlock){
  unixFile *pFile = (unixFile*)id;
  unixInodeInfo *pInode;
  struct flock lock;
  int rc = SQLITE_OK;

  assert( pFile );
  OSTRACE(("UNLOCK  %d %d was %d(%d,%d) pid=%d (unix)\n", pFile->h, eFileLock,
      pFile->eFileLock, pFile->pInode->eFileLock, pFile->pInode->nShared,
      getpid()));

  assert( eFileLock<=SHARED_LOCK );
  if( pFile->eFileLock<=eFileLock ){
    return SQLITE_OK;
  }
  unixEnterMutex();
  pInode = pFile->pInode;
  assert( pInode->nShared!=0 );
  if( pFile->eFileLock>SHARED_LOCK ){
    assert( pInode->eFileLock==pFile->eFileLock );

#ifdef SQLITE_DEBUG
        /* When reducing a lock such that other processes can start
    ** reading the database file again, make sure that the
    ** transaction counter was updated if any part of the database
    ** file changed.  If the transaction counter is not updated,
    ** other connections to the same file might not realize that
    ** the file has changed and hence might not know to flush their
    ** cache.  The use of a stale cache can lead to database corruption.
    **当减少锁以便其他进程可以再次读取数据库文件的时候，要确保事务计数器已经更新，如果数据库文件文件的任何部分发生了改变。
    **如果事务计数器没有更新，对同一文件的其他连接可能没有意识到该文件已经更改，因此可能不知道要刷新其缓存。
    **使用旧的缓存可能会导致数据库的损坏。
*/
    pFile->inNormalWrite = 0;
#endif

    /* downgrading to a shared lock on NFS involves clearing the write lock
    ** before establishing the readlock - to avoid a race condition we downgrade
    ** the lock in 2 blocks, so that part of the range will be covered by a 
    ** write lock until the rest is covered by a read lock:
    **在NFS上降级到共享锁，包括在建立读锁之前清除写锁—以免在降低2块锁时有竞争的情况，使一个写锁覆盖部分范围，
    **直到读锁覆盖剩下的部分：
    **  1:   [WWWWW]
    **  2:   [....W]
    **  3:   [RRRRW]
    **  4:   [RRRR.]
    */
    if( eFileLock==SHARED_LOCK ){

#if !defined(__APPLE__) || !SQLITE_ENABLE_LOCKING_STYLE
      (void)handleNFSUnlock;
      assert( handleNFSUnlock==0 );
#endif
#if defined(__APPLE__) && SQLITE_ENABLE_LOCKING_STYLE
      if( handleNFSUnlock ){
        int tErrno;               /* Error code from system call errors */  //系统调用错误的错误代码
        off_t divSize = SHARED_SIZE - 1;
        
        lock.l_type = F_UNLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = SHARED_FIRST;
        lock.l_len = divSize;
        if( unixFileLock(pFile, &lock)==(-1) ){
          tErrno = errno;
          rc = SQLITE_IOERR_UNLOCK;
          if( IS_LOCK_ERROR(rc) ){
            pFile->lastErrno = tErrno;
          }
          goto end_unlock;
        }
        lock.l_type = F_RDLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = SHARED_FIRST;
        lock.l_len = divSize;
        if( unixFileLock(pFile, &lock)==(-1) ){
          tErrno = errno;
          rc = sqliteErrorFromPosixError(tErrno, SQLITE_IOERR_RDLOCK);
          if( IS_LOCK_ERROR(rc) ){
            pFile->lastErrno = tErrno;
          }
          goto end_unlock;
        }
        lock.l_type = F_UNLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = SHARED_FIRST+divSize;
        lock.l_len = SHARED_SIZE-divSize;
        if( unixFileLock(pFile, &lock)==(-1) ){
          tErrno = errno;
          rc = SQLITE_IOERR_UNLOCK;
          if( IS_LOCK_ERROR(rc) ){
            pFile->lastErrno = tErrno;
          }
          goto end_unlock;
        }
      }else
#endif /* defined(__APPLE__) && SQLITE_ENABLE_LOCKING_STYLE */
      {
        lock.l_type = F_RDLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = SHARED_FIRST;
        lock.l_len = SHARED_SIZE;
        if( unixFileLock(pFile, &lock) ){
          /* In theory, the call to unixFileLock() cannot fail because another
          ** process is holding an incompatible lock. If it does, this 
          ** indicates that the other process is not following the locking
          ** protocol. If this happens, return SQLITE_IOERR_RDLOCK. Returning
          ** SQLITE_BUSY would confuse the upper layer (in practice it causes 
          ** an assert to fail).
          **理论上，调用unixFileLock()不能失败，因为另一程序正持有一个不兼容锁。
          **如果失败了，这表明其他进程没有遵循加锁协议。如果发生这种情况，返回SQLITE_IOERR_RDLOCK。
          **返回SQLITE_BUSY会让上层混淆(实际上它导致一个失败的生效)。
          */ 
          rc = SQLITE_IOERR_RDLOCK;
          pFile->lastErrno = errno;
          goto end_unlock;
        }
      }
    }
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = PENDING_BYTE;
    lock.l_len = 2L;  assert( PENDING_BYTE+1==RESERVED_BYTE );
    if( unixFileLock(pFile, &lock)==0 ){
      pInode->eFileLock = SHARED_LOCK;
    }else{
      rc = SQLITE_IOERR_UNLOCK;
      pFile->lastErrno = errno;
      goto end_unlock;
    }
  }
  if( eFileLock==NO_LOCK ){
    /* Decrement the shared lock counter.  Release the lock using an
    ** OS call only when all threads in this same process have released
    ** the lock.
    **共享锁计数器进行递减。当这一进程中的所有线程释放锁时，释放使用OS调用的锁。
    */
    pInode->nShared--;
    if( pInode->nShared==0 ){
      lock.l_type = F_UNLCK;
      lock.l_whence = SEEK_SET;
      lock.l_start = lock.l_len = 0L;
      if( unixFileLock(pFile, &lock)==0 ){
        pInode->eFileLock = NO_LOCK;
      }else{
        rc = SQLITE_IOERR_UNLOCK;
        pFile->lastErrno = errno;
        pInode->eFileLock = NO_LOCK;
        pFile->eFileLock = NO_LOCK;
      }
    }

    /* Decrement the count of locks against this same file.  When the
    ** count reaches zero, close any other file descriptors whose close
    ** was deferred because of outstanding locks.
    **对相同的文件的锁的计数进行递减。当计数达到0 时，关闭所有其他由于outstanding锁而推迟关闭的文件描述符
    */
    pInode->nLock--;
    assert( pInode->nLock>=0 );
    if( pInode->nLock==0 ){
      closePendingFds(pFile);
    }
  }

end_unlock:
  unixLeaveMutex();
  if( rc==SQLITE_OK ) pFile->eFileLock = eFileLock;
  return rc;
}

/*
** Lower the locking level on file descriptor pFile to eFileLock.  eFileLock
** must be either NO_LOCK or SHARED_LOCK.
**降低文件描述符pFile上的锁定级别为eFileLock。eFileLock必须是NO_LOCK或SHARED_LOCK。
**
** If the locking level of the file descriptor is already at or below
** the requested locking level, this routine is a no-op.
**如果文件描述符的锁定级别已经达到或低于所请求的锁定级别，此例程是一个空操作。
*/
static int unixUnlock(sqlite3_file *id, int eFileLock){
  return posixUnlock(id, eFileLock, 0);
}

/*
** This function performs the parts of the "close file" operation 
** common to all locking schemes. It closes the directory and file
** handles, if they are valid, and sets all fields of the unixFile
** structure to 0.
**这个函数执行所有文件锁定计划共同的“关闭文件”操作部分。关闭目录和文件句柄,如果他们是有效的,并设置unixFile结构的所有字段为0。
** It is *not* necessary to hold the mutex when this routine is called,
** even on VxWorks.  A mutex will be acquired on VxWorks by the
** vxworksReleaseFileId() routine.
**在调用这个例程时它并不需要互斥量，即使是在VxWorks。在VxWorks上，互斥量通过vxworksReleaseFileId()例程获得。
*/
static int closeUnixFile(sqlite3_file *id){
  unixFile *pFile = (unixFile*)id;
  if( pFile->h>=0 ){
    robust_close(pFile, pFile->h, __LINE__);
    pFile->h = -1;
  }
#if OS_VXWORKS
  if( pFile->pId ){
    if( pFile->ctrlFlags & UNIXFILE_DELETE ){
      osUnlink(pFile->pId->zCanonicalName);
    }
    vxworksReleaseFileId(pFile->pId);
    pFile->pId = 0;
  }
#endif
  OSTRACE(("CLOSE   %-3d\n", pFile->h));
  OpenCounter(-1);
  sqlite3_free(pFile->pUnused);
  memset(pFile, 0, sizeof(unixFile));
  return SQLITE_OK;
}

/*
** Close a file.  关闭一个文件
*/
static int unixClose(sqlite3_file *id){
  int rc = SQLITE_OK;
  unixFile *pFile = (unixFile *)id;
  unixUnlock(id, NO_LOCK);
  unixEnterMutex();

  /* unixFile.pInode is always valid here. Otherwise, a different close
  ** routine (e.g. nolockClose()) would be called instead.
**在这里unixFile.pInode始终是有效的。否则，将被调用一个不同的但相似的程序（如nolockClose（））来代替。
  */
  assert( pFile->pInode->nLock>0 || pFile->pInode->bProcessLock==0 );
  if( ALWAYS(pFile->pInode) && pFile->pInode->nLock ){
    /* If there are outstanding locks, do not actually close the file just
    ** yet because that would clear those locks.  Instead, add the file
    ** descriptor to pInode->pUnused list.  It will be automatically closed 
    ** when the last lock is cleared.
    **如果有未完成的锁，事实还不能关闭这个文件，因为那样会清除这些锁。
    **相反，将该文件描述符添加到pInode->pUnused列表。当清除最后的锁定时，它会自动关闭。
    */
    setPendingFd(pFile);
  }
  releaseInodeInfo(pFile);
  rc = closeUnixFile(id);
  unixLeaveMutex();
  return rc;
}

/************** End of the posix advisory lock implementation *****************
******************************************************************************/

/******************************************************************************
****************************** No-op Locking **********************************
// 无操作锁
** Of the various locking implementations available, this is by far the
** simplest:  locking is ignored.  No attempt is made to lock the database
** file for reading or writing.
**
// 相对其他一系列锁的实现，这种无操作锁是迄今为止最简单的，它没有试图锁住数据库文件进行读或写。
** This locking mode is appropriate for use on read-only databases
** (ex: databases that are burned into CD-ROM, for example.)  It can
** also be used if the application employs some external mechanism to
** prevent simultaneous access of the same database by two or more
** database connections.  But there is a serious risk of database
** corruption if this locking mode is used in situations where multiple
** database connections are accessing the same database file at the same
** time and one or more of those connections are writing.
*/
// 这种模式的锁适用于只读的数据库，例如刻在CD-ROM上的数据库。当一个应用程序使用外部机制去阻止两个或两个以上的
// 数据库连接对同一个数据库同时进行访问时也可以使用它。但是，当这种锁用于大量的数据库连接同时访问一个数据库，并
// 且这些连接当中有一个或者更多的是进行写操作的情况的时候，就会一个严重的数据库损坏风险。

static int nolockCheckReservedLock(sqlite3_file *NotUsed, int *pResOut){
  UNUSED_PARAMETER(NotUsed);
  *pResOut = 0;
  return SQLITE_OK;
}
static int nolockLock(sqlite3_file *NotUsed, int NotUsed2){
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  return SQLITE_OK;
}
static int nolockUnlock(sqlite3_file *NotUsed, int NotUsed2){
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  return SQLITE_OK;
}

/*
** Close the file.   // 关闭文件
*/
static int nolockClose(sqlite3_file *id) {
  return closeUnixFile(id);
}

/******************* End of the no-op lock implementation *********************
******************************************************************************/

/******************************************************************************
************************* Begin dot-file Locking ******************************
// 点文件锁
** The dotfile locking implementation uses the existance of separate lock
** files (really a directory) to control access to the database.  This works
** on just about every filesystem imaginable.  But there are serious downsides:
**
**    (1)  There is zero concurrency.  A single reader blocks all other
**         connections from reading or writing the database.
**
**    (2)  An application crash or power loss can leave stale lock files
**         sitting around that need to be cleared manually.
**
** Nevertheless, a dotlock is an appropriate locking mode for use if no
** other locking strategy is available.
**
// 点文件锁的实现是使用存在单独的锁文件，就是一种目录来控制对数据库的访问。这种机制用于任意一种文件系统，但是它又存在严重的缺陷：
（1）它没有并发性，一个读者就阻碍了其他所有要对数据库进行读写的连接；
（2）应用程序的崩溃或功率的耗损都会使旧的锁文件无法自动清理，而需要手动清除。
不过，点文件锁是一种适用于没有其他锁策略可用的情况的锁机制。
** Dotfile locking works by creating a subdirectory in the same directory as
** the database and with the same name but with a ".lock" extension added.
** The existance of a lock directory implies an EXCLUSIVE lock.  All other
** lock types (SHARED, RESERVED, PENDING) are mapped into EXCLUSIVE.
*/
// 点文件锁的工作原理是在同一个目录里面创建次目录存入数据库，并且有相同的扩展名“.lock”。一个锁目录的存在意
// 味着它是排他锁。其它所有类型的锁（共享锁，保留锁，未决锁）都映射到拍他锁。
/*
** The file suffix added to the data base filename in order to create the
** lock directory.
*/
// 文件后缀添加到数据库文件里面以创建锁目录。
#define DOTLOCK_SUFFIX ".lock"    //预定义锁文件后缀为.lock

/*
** This routine checks if there is a RESERVED lock held on the specified
** file by this or any other process. If such a lock is held, set *pResOut
** to a non-zero value otherwise *pResOut is set to zero.  The return value
** is set to SQLITE_OK unless an I/O error occurs during lock checking.
**
// 这个程序会检查指定文件或者是其他任何进程里是否持有保留锁，如果持有保留锁，将指针*pResOout的值设为非零，
否则设为0.返回值设为SQLITE_OK除非在锁检查期间发生I/O错误。
** In dotfile locking, either a lock exists or it does not.  So in this
** variation of CheckReservedLock(), *pResOut is set to true if any lock
** is held on the file and false if the file is unlocked.
*/
// 在点文件锁机制中，锁要么存在要么不存在。所以函数CheckReservedLock()的变化就是如果文件被加锁，*pResOut 
//的值设为true，文件没被加锁则设为false。
static int dotlockCheckReservedLock(sqlite3_file *id, int *pResOut) {
  int rc = SQLITE_OK;
  int reserved = 0;
  unixFile *pFile = (unixFile*)id;

  SimulateIOError( return SQLITE_IOERR_CHECKRESERVEDLOCK; );
  
  assert( pFile );

  /* Check if a thread in this process holds such a lock */    // 检查进程中是否有一个线程持有锁
  if( pFile->eFileLock>SHARED_LOCK ){
    /* Either this connection or some other connection in the same process
** holds a lock on the file.  No need to check further. */ 
// 这个连接或者是同一进程中其它一些连接中有锁定文件，则停止检查
    reserved = 1;
  }else{
    /* The lock is held if and only if the lockfile exists */    // 持有锁，当且仅当锁文件存在
    const char *zLockFile = (const char*)pFile->lockingContext;
    reserved = osAccess(zLockFile, 0)==0;
  }
  OSTRACE(("TEST WR-LOCK %d %d %d (dotlock)\n", pFile->h, rc, reserved));
  *pResOut = reserved;
  return rc;
}

/*
** Lock the file with the lock specified by parameter eFileLock - one
** of the following:  
**
// 使用参数eFileLock以下值之一指定的锁对文件加锁
**     (1) SHARED_LOCK   // 共享锁
**     (2) RESERVED_LOCK   // 保留锁
**     (3) PENDING_LOCK    // 未决锁
**     (4) EXCLUSIVE_LOCK   // 排他锁
**
** Sometimes when requesting one lock state, additional lock states
** are inserted in between.  The locking might fail on one of the later
** transitions leaving the lock state different from what it started but
** still short of its goal.  The following chart shows the allowed
** transitions and the inserted intermediate states:
// 当请求一个锁状态的时候，额外的锁状态会插入到其中。锁定失败后会后来的转换会使它不同于开始时候的锁状态，
但是仍能达到预期目标。以下的记录是允许的转换和插入的状态。
**
**    UNLOCKED -> SHARED   // 未加锁-->共享锁
**    SHARED -> RESERVED    // 共享锁转为-->保留锁
**    SHARED -> (PENDING) -> EXCLUSIVE   // 共享锁-->未决锁（插入）-->排他锁
**    RESERVED -> (PENDING) -> EXCLUSIVE  // 保留锁-->未决锁（插入）-->排他锁
**    PENDING -> EXCLUSIVE  // 未决锁-->排他锁
**
** This routine will only increase a lock.  Use the sqlite3OsUnlock()
** routine to lower a locking level.
** 
// 这个程序只是增加一个锁。使用sqlite3的OsUnlock()程序降低锁的级别。
** With dotfile locking, we really only support state (4): EXCLUSIVE.
** But we track the other locking levels internally.
*/
// 使用点文件锁，实际上只支持第四种状态：排他锁，但是可以在内部追踪其他级别的锁。
static int dotlockLock(sqlite3_file *id, int eFileLock) {
  unixFile *pFile = (unixFile*)id;
  char *zLockFile = (char *)pFile->lockingContext;
  int rc = SQLITE_OK;


  /* If we have any lock, then the lock file already exists.  All we have
  ** to do is adjust our internal record of the lock level.
  */
// 如果有其中任意一种锁，那么锁文件其实已经存在了，我们要做的只是修改内部记录的锁级别。
  if( pFile->eFileLock > NO_LOCK ){
    pFile->eFileLock = eFileLock;
    /* Always update the timestamp on the old file */   // 更新旧文件的时间戳
#ifdef HAVE_UTIME
    utime(zLockFile, NULL);
#else
    utimes(zLockFile, NULL);
#endif
    return SQLITE_OK;
  }
  
  /* grab an exclusive lock */    // 攫取一个排他锁
  rc = osMkdir(zLockFile, 0777);
  if( rc<0 ){
    /* failed to open/create the lock directory */   // 创建锁目录失败
    int tErrno = errno;
    if( EEXIST == tErrno ){
      rc = SQLITE_BUSY;
    } else {
      rc = sqliteErrorFromPosixError(tErrno, SQLITE_IOERR_LOCK);
      if( IS_LOCK_ERROR(rc) ){
        pFile->lastErrno = tErrno;
      }
    }
    return rc;
  } 
  
  /* got it, set the type and return ok */    // 获取成功，设置类型值并返回ok
  pFile->eFileLock = eFileLock;
  return rc;
}

/*
** Lower the locking level on file descriptor pFile to eFileLock.  eFileLock
** must be either NO_LOCK or SHARED_LOCK.
**
// 在文件描述符pFile 到eFileLok降低锁的级别 ，eFileLock必须是NO_LOCK or和SHARED_LOCK两者之一。
** If the locking level of the file descriptor is already at or below
** the requested locking level, this routine is a no-op.
**
// 如果文件描述符的锁定级别已经在正在请求的锁级别或以下，那么这个程序是一个空操作。
** When the locking level reaches NO_LOCK, delete the lock file.
*/
// 当锁定级别达到了NO_LOCK时删除锁文件。
static int dotlockUnlock(sqlite3_file *id, int eFileLock) {
  unixFile *pFile = (unixFile*)id;
  char *zLockFile = (char *)pFile->lockingContext;
  int rc;

  assert( pFile );
  OSTRACE(("UNLOCK  %d %d was %d pid=%d (dotlock)\n", pFile->h, eFileLock,
           pFile->eFileLock, getpid()));
  assert( eFileLock<=SHARED_LOCK );
  
  /* no-op if possible */   // 空操作如果有可能
  if( pFile->eFileLock==eFileLock ){
    return SQLITE_OK;
  }

  /* To downgrade to shared, simply update our internal notion of the
  ** lock state.  No need to mess with the file on disk.
  */  
// 降级到共享时，只需简单地更新锁状态的内部概念，而不需要毁坏磁盘文件。
  if( eFileLock==SHARED_LOCK ){
    pFile->eFileLock = SHARED_LOCK;
    return SQLITE_OK;
  }
  
  /* To fully unlock the database, delete the lock file */   // 完全打开数据库时删除锁文件
  assert( eFileLock==NO_LOCK );
  rc = osRmdir(zLockFile);
  if( rc<0 && errno==ENOTDIR ) rc = osUnlink(zLockFile);
  if( rc<0 ){
    int tErrno = errno;
    rc = 0;
    if( ENOENT != tErrno ){
      rc = SQLITE_IOERR_UNLOCK;
    }
    if( IS_LOCK_ERROR(rc) ){
      pFile->lastErrno = tErrno;
    }
    return rc; 
  }
  pFile->eFileLock = NO_LOCK;
  return SQLITE_OK;
}

/*
** Close a file.  Make sure the lock has been released before closing.
*/ 
// 关闭一个文件，确保锁在关闭之前已经被释放。
static int dotlockClose(sqlite3_file *id) {
  int rc;
  if( id ){
    unixFile *pFile = (unixFile*)id;
    dotlockUnlock(id, NO_LOCK);
    sqlite3_free(pFile->lockingContext);
  }
  rc = closeUnixFile(id);
  return rc;
}
/****************** End of the dot-file lock implementation *******************
******************************************************************************/

/******************************************************************************
************************** Begin flock Locking ********************************
// 聚集锁
** Use the flock() system call to do file locking. 使用系统调用函数flock() 执行文件锁定。
**
** flock() locking is like dot-file locking in that the various
** fine-grain locking levels supported by SQLite are collapsed into
** a single exclusive lock.  In other words, SHARED, RESERVED, and
** PENDING locks are the same thing as an EXCLUSIVE lock.  SQLite
** still works when you do this, but concurrency is reduced since
** only a single process can be reading the database at a time.
**
// 聚集锁在SQLite支持各种细粒锁级别紧缩成一个单独的排他锁这一点上和点文件锁是一样的。换句话说，共享锁，
保留锁和未决锁对于排他锁来说是一样的。当你这样做时，SQLite仍然工作,但并发性降低,因为同一时间只有一个进程可以读取数据库。
** Omit this section if SQLITE_ENABLE_LOCKING_STYLE is turned off or if
** compiling for VXWORKS.
*/  
// 如果SQLITE_ENABLE_LOCKING_STYLE被关闭或者VXWORKS被编译，则忽略这部分。
#if SQLITE_ENABLE_LOCKING_STYLE && !OS_VXWORKS

/*
** Retry flock() calls that fail with EINTR  // 系统调用中断则重试flock()函数调用
*/
#ifdef EINTR
static int robust_flock(int fd, int op){
  int rc;
  do{ rc = flock(fd,op); }while( rc<0 && errno==EINTR );
  return rc;
}
#else
# define robust_flock(a,b) flock(a,b)
#endif
     

/*
** This routine checks if there is a RESERVED lock held on the specified
** file by this or any other process. If such a lock is held, set *pResOut
** to a non-zero value otherwise *pResOut is set to zero.  The return value
** is set to SQLITE_OK unless an I/O error occurs during lock checking.
*/
// 这个程序会检查指定文件或者是其他任何进程里是否持有保留锁，如果持有保留锁，将指针*pResOout的值设为非零，
// 否则设为0.返回值设为SQLITE_OK除非在锁检查期间发生I/O错误。
static int flockCheckReservedLock(sqlite3_file *id, int *pResOut){
  int rc = SQLITE_OK;
  int reserved = 0;
  unixFile *pFile = (unixFile*)id;
  
  SimulateIOError( return SQLITE_IOERR_CHECKRESERVEDLOCK; );
  
  assert( pFile );
  
  /* Check if a thread in this process holds such a lock */  // 检查进程中是否有线程持有保留锁
  if( pFile->eFileLock>SHARED_LOCK ){
    reserved = 1;
  }
  
  /* Otherwise see if some other process holds it. */   // 如果没有则查看其它进程
  if( !reserved ){
    /* attempt to get the lock */     // 获取锁
    int lrc = robust_flock(pFile->h, LOCK_EX | LOCK_NB);
    if( !lrc ){
      /* got the lock, unlock it */    // 获取锁，并解锁
      lrc = robust_flock(pFile->h, LOCK_UN);
      if ( lrc ) {
        int tErrno = errno;
        /* unlock failed with an error */     // 解锁发生错误并且失败
        lrc = SQLITE_IOERR_UNLOCK; 
        if( IS_LOCK_ERROR(lrc) ){
          pFile->lastErrno = tErrno;
          rc = lrc;
        }
      }
    } else {
      int tErrno = errno;
      reserved = 1;
      /* someone else might have it reserved */     // 其它进程可能保留了锁
      lrc = sqliteErrorFromPosixError(tErrno, SQLITE_IOERR_LOCK); 
      if( IS_LOCK_ERROR(lrc) ){
        pFile->lastErrno = tErrno;
        rc = lrc;
      }
    }
  }
  OSTRACE(("TEST WR-LOCK %d %d %d (flock)\n", pFile->h, rc, reserved));

#ifdef SQLITE_IGNORE_FLOCK_LOCK_ERRORS
  if( (rc & SQLITE_IOERR) == SQLITE_IOERR ){
    rc = SQLITE_OK;
    reserved=1;
  }
#endif /* SQLITE_IGNORE_FLOCK_LOCK_ERRORS */
  *pResOut = reserved;
  return rc;
}

/*
** Lock the file with the lock specified by parameter eFileLock - one
** of the following:
**
// 使用参数eFileLock以下值之一指定的锁对文件加锁
**     (1) SHARED_LOCK   // 共享锁
**     (2) RESERVED_LOCK   // 保留锁
**     (3) PENDING_LOCK    // 未决锁
**     (4) EXCLUSIVE_LOCK   // 排他锁
** Sometimes when requesting one lock state, additional lock states
** are inserted in between.  The locking might fail on one of the later
** transitions leaving the lock state different from what it started but
** still short of its goal.  The following chart shows the allowed
** transitions and the inserted intermediate states:
**
// 当请求一个锁状态的时候，额外的锁状态会插入到其中。锁定失败后会后来的转换会使它不同于开始时候的锁状态，
但是仍能达到预期目标。以下的记录是允许的转换和插入的状态。
**
**    UNLOCKED -> SHARED   //未加锁-->共享锁
**    SHARED -> RESERVED    //共享锁转为-->保留锁
**    SHARED -> (PENDING) -> EXCLUSIVE   //共享锁-->未决锁（插入）-->排他锁
**    RESERVED -> (PENDING) -> EXCLUSIVE  //保留锁-->未决锁（插入）-->排他锁
**    PENDING -> EXCLUSIVE  //未决锁-->排他锁
**
** flock() only really support EXCLUSIVE locks.  We track intermediate
** lock states in the sqlite3_file structure, but all locks SHARED or
** above are really EXCLUSIVE locks and exclude all other processes from
** access the file.
**
// 聚集锁实际上只支持排他锁。我们在sqlite3文件结构中追踪中间级别的锁状态，但是所有的共享锁或者以上级别的锁事实上
都是排他锁，并且拒绝其它进程访问这个文件。
** This routine will only increase a lock.  Use the sqlite3OsUnlock()
** routine to lower a locking level.
*/
// 这个程序只是增加一个锁。使用sqlite3的OsUnlock()程序降低锁的级别。
static int flockLock(sqlite3_file *id, int eFileLock) {
  int rc = SQLITE_OK;
  unixFile *pFile = (unixFile*)id;

  assert( pFile );

  /* if we already have a lock, it is exclusive.  
  ** Just adjust level and punt on outta here. */  // 如果已经有一个排他锁，只需调整锁的级别
  if (pFile->eFileLock > NO_LOCK) {
    pFile->eFileLock = eFileLock;
    return SQLITE_OK;
  }
  
  /* grab an exclusive lock */
  
  if (robust_flock(pFile->h, LOCK_EX | LOCK_NB)) {
    int tErrno = errno;
    /* didn't get, must be busy */     // 系统繁忙，请求失败
    rc = sqliteErrorFromPosixError(tErrno, SQLITE_IOERR_LOCK);
    if( IS_LOCK_ERROR(rc) ){
      pFile->lastErrno = tErrno;
    }
  } else {
    /* got it, set the type and return ok */    // 获取成功，设置类型值并返回ok
    pFile->eFileLock = eFileLock;
  }
  OSTRACE(("LOCK    %d %s %s (flock)\n", pFile->h, azFileLock(eFileLock), 
           rc==SQLITE_OK ? "ok" : "failed"));
#ifdef SQLITE_IGNORE_FLOCK_LOCK_ERRORS
  if( (rc & SQLITE_IOERR) == SQLITE_IOERR ){
    rc = SQLITE_BUSY;
  }
#endif /* SQLITE_IGNORE_FLOCK_LOCK_ERRORS */
  return rc;
}


/*
** Lower the locking level on file descriptor pFile to eFileLock.  eFileLock
** must be either NO_LOCK or SHARED_LOCK.
**
// 在文件描述符pFile 到eFileLok降低锁的级别 ，eFileLock必须是NO_LOCK or和SHARED_LOCK两者之一。
** If the locking level of the file descriptor is already at or below
** the requested locking level, this routine is a no-op.
*/
// 如果文件描述符的锁定级别已经在正在请求的锁级别或以下，那么这个程序是一个空操作。
static int flockUnlock(sqlite3_file *id, int eFileLock) {
  unixFile *pFile = (unixFile*)id;
  
  assert( pFile );
  OSTRACE(("UNLOCK  %d %d was %d pid=%d (flock)\n", pFile->h, eFileLock,
           pFile->eFileLock, getpid()));
  assert( eFileLock<=SHARED_LOCK );
  
  /* no-op if possible */     // 无操作是可能的
  if( pFile->eFileLock==eFileLock ){
    return SQLITE_OK;
  }
  
  /* shared can just be set because we always have an exclusive */    // 共享锁只能被旁置因为总有一个排他锁
  if (eFileLock==SHARED_LOCK) {
    pFile->eFileLock = eFileLock;
    return SQLITE_OK;
  }
  
  /* no, really, unlock. */     // 否则就解锁
  if( robust_flock(pFile->h, LOCK_UN) ){
#ifdef SQLITE_IGNORE_FLOCK_LOCK_ERRORS
    return SQLITE_OK;
#endif /* SQLITE_IGNORE_FLOCK_LOCK_ERRORS */
    return SQLITE_IOERR_UNLOCK;
  }else{
    pFile->eFileLock = NO_LOCK;
    return SQLITE_OK;
  }
}

/*
** Close a file.     // 关闭文件
*/
static int flockClose(sqlite3_file *id) {
  if( id ){
    flockUnlock(id, NO_LOCK);
  }
  return closeUnixFile(id);
}

#endif /* SQLITE_ENABLE_LOCKING_STYLE && !OS_VXWORK */    
// 如果是SQLITE_ENABLE_LOCKING_STYLE并且操作系统不是VXWORK就编译，否则跳过

/******************* End of the flock lock implementation *********************
******************************************************************************/
/******************************************************************************
************************ Begin Named Semaphore Locking ************************
// 命名信号锁
** Named semaphore locking is only supported on VxWorks.  
// 只有VxWorks操作系统支持命名信号锁
**
** Semaphore locking is like dot-lock and flock in that it really only
** supports EXCLUSIVE locking.  Only a single process can read or write
** the database file at a time.  This reduces potential concurrency, but
** makes the lock implementation much easier.
*/
// 命名信号锁就像点文件锁和聚集锁一样，实际上只支持排他锁。只有单个进程可以在同一时间对数据库文件进行读或写。
// 这虽然减少了潜在的并发性，却使得这种锁的实现更加容易。
#if OS_VXWORKS

/*
** This routine checks if there is a RESERVED lock held on the specified
** file by this or any other process. If such a lock is held, set *pResOut
** to a non-zero value otherwise *pResOut is set to zero.  The return value
** is set to SQLITE_OK unless an I/O error occurs during lock checking.
*/
// 这个程序会检查指定文件或者是其他任何进程里是否持有保留锁，如果持有保留锁，将指针*pResOout的值设为非零，否则设为0.
// 返回值设为SQLITE_OK除非在锁检查期间发生I/O错误。
static int semCheckReservedLock(sqlite3_file *id, int *pResOut) {
  int rc = SQLITE_OK;
  int reserved = 0;
  unixFile *pFile = (unixFile*)id;

  SimulateIOError( return SQLITE_IOERR_CHECKRESERVEDLOCK; );
  
  assert( pFile );

  /* Check if a thread in this process holds such a lock */     // 检查这个进程的其它线程是否持有这个锁
  if( pFile->eFileLock>SHARED_LOCK ){
    reserved = 1;
  }
  
  /* Otherwise see if some other process holds it. */    // 否则查看别的进程
  if( !reserved ){
    sem_t *pSem = pFile->pInode->pSem;
    struct stat statBuf;

    if( sem_trywait(pSem)==-1 ){
      int tErrno = errno;
      if( EAGAIN != tErrno ){
        rc = sqliteErrorFromPosixError(tErrno, SQLITE_IOERR_CHECKRESERVEDLOCK);
        pFile->lastErrno = tErrno;
      } else {
        /* someone else has the lock when we are in NO_LOCK */  
// 当我们处于未加锁状态时别的进程持有命名信号锁
        reserved = (pFile->eFileLock < SHARED_LOCK);
      }
    }else{
      /* we could have it if we want it */    // 如果需要就可以获取
      sem_post(pSem);
    }
  }
  OSTRACE(("TEST WR-LOCK %d %d %d (sem)\n", pFile->h, rc, reserved));

  *pResOut = reserved;
  return rc;
}

/*
** Lock the file with the lock specified by parameter eFileLock - one
** of the following:
**
// 使用参数eFileLock以下值之一指定的锁对文件加锁
**     (1) SHARED_LOCK   // 共享锁
**     (2) RESERVED_LOCK   // 保留锁
**     (3) PENDING_LOCK    // 未决锁
**     (4) EXCLUSIVE_LOCK   // 排他锁
**
** Sometimes when requesting one lock state, additional lock states
** are inserted in between.  The locking might fail on one of the later
** transitions leaving the lock state different from what it started but
** still short of its goal.  The following chart shows the allowed
** transitions and the inserted intermediate states:
**
// 当请求一个锁状态的时候，额外的锁状态会插入到其中。锁定失败后会后来的转换会使它不同于开始时候的锁状态，
但是仍能达到预期目标。以下的记录是允许的转换和插入的状态。
**
**    UNLOCKED -> SHARED   // 未加锁-->共享锁
**    SHARED -> RESERVED    // 共享锁转为-->保留锁
**    SHARED -> (PENDING) -> EXCLUSIVE   // 共享锁-->未决锁（插入）-->排他锁
**    RESERVED -> (PENDING) -> EXCLUSIVE  // 保留锁-->未决锁（插入）-->排他锁
**    PENDING -> EXCLUSIVE  // 未决锁-->排他锁
**
** Semaphore locks only really support EXCLUSIVE locks.  We track intermediate
** lock states in the sqlite3_file structure, but all locks SHARED or
** above are really EXCLUSIVE locks and exclude all other processes from
** access the file.
**
// 命名信号锁实际上只支持排他锁。我们在sqlite3文件结构中追踪中间级别的锁状态，但是所有的共享锁或者以上级别的
锁事实上都是排他锁，并且拒绝其它进程访问这个文件。
** This routine will only increase a lock.  Use the sqlite3OsUnlock()
** routine to lower a locking level.
*/
// 这个程序只是增加一个锁。使用sqlite3的OsUnlock()程序降低锁的级别。
static int semLock(sqlite3_file *id, int eFileLock) {
  unixFile *pFile = (unixFile*)id;
  int fd;
  sem_t *pSem = pFile->pInode->pSem;
  int rc = SQLITE_OK;

  /* if we already have a lock, it is exclusive.  
  ** Just adjust level and punt on outta here. */    // 调整级别后结束程序
  if (pFile->eFileLock > NO_LOCK) {
    pFile->eFileLock = eFileLock;
    rc = SQLITE_OK;
    goto sem_end_lock;
  }
  
  /* lock semaphore now but bail out when already locked. */     // 锁定信号，但是信号被锁定时将其释放
  if( sem_trywait(pSem)==-1 ){
    rc = SQLITE_BUSY;
    goto sem_end_lock;
  }

  /* got it, set the type and return ok */      // 获取成功，设置类型值并返回ok
  pFile->eFileLock = eFileLock;

 sem_end_lock:
  return rc;
}

/*
** Lower the locking level on file descriptor pFile to eFileLock.  eFileLock
** must be either NO_LOCK or SHARED_LOCK.
**
** If the locking level of the file descriptor is already at or below
** the requested locking level, this routine is a no-op.
*/
// 在文件描述符pFile 到eFileLok降低锁的级别 ，eFileLock必须是NO_LOCK or和SHARED_LOCK两者之一。
// 如果文件描述符的锁定级别已经在正在请求的锁级别或以下，那么这个程序是一个空操作。

static int semUnlock(sqlite3_file *id, int eFileLock) {
  unixFile *pFile = (unixFile*)id;
  sem_t *pSem = pFile->pInode->pSem;

  assert( pFile );
  assert( pSem );
  OSTRACE(("UNLOCK  %d %d was %d pid=%d (sem)\n", pFile->h, eFileLock,
           pFile->eFileLock, getpid()));
  assert( eFileLock<=SHARED_LOCK );
  
  /* no-op if possible */      // 无操作是可能的
  if( pFile->eFileLock==eFileLock ){
    return SQLITE_OK;
  }
  
  /* shared can just be set because we always have an exclusive */     // 共享锁只能被旁置因为总有一个排他锁

  if (eFileLock==SHARED_LOCK) {
    pFile->eFileLock = eFileLock;
    return SQLITE_OK;
  }
  
  /* no, really unlock. */     // 否则就解锁   
  if ( sem_post(pSem)==-1 ) {
    int rc, tErrno = errno;
    rc = sqliteErrorFromPosixError(tErrno, SQLITE_IOERR_UNLOCK);
    if( IS_LOCK_ERROR(rc) ){
      pFile->lastErrno = tErrno;
    }
    return rc; 
  }
  pFile->eFileLock = NO_LOCK;
  return SQLITE_OK;
}

/*
 ** Close a file.     // 关闭文件
 */
static int semClose(sqlite3_file *id) {
  if( id ){
    unixFile *pFile = (unixFile*)id;
    semUnlock(id, NO_LOCK);
    assert( pFile );
    unixEnterMutex();
    releaseInodeInfo(pFile);
    unixLeaveMutex();
    closeUnixFile(id);
  }
  return SQLITE_OK;
}

#endif /* OS_VXWORKS */
/*
** Named semaphore locking is only available on VxWorks.
**
// 命名信号锁只适用于VxWorks，在VxWorks下才编译
*************** End of the named semaphore lock implementation ****************
******************************************************************************/


/******************************************************************************
*************************** Begin AFP Locking *********************************
// 苹果文件协议锁
** AFP is the Apple Filing Protocol.  AFP is a network filesystem found
** on Apple Macintosh computers - both OS9 and OSX.
**
// AFP是苹果文件协议，AFP是建立在苹果麦金塔什电脑，包括OS9 和OSX操作系统之上的网络文件系统。
** Third-party implementations of AFP are available.  But this code here
** only works on OSX.  
// AFP的第三方实现是有效的，但是这段代码仅仅作用于OSX系统上。
*/

#if defined(__APPLE__) && SQLITE_ENABLE_LOCKING_STYLE
/*
** The afpLockingContext structure contains all afp lock specific state
*/
// afpLockingContext 结构体包含所有苹果文件协议锁特定的状态。
typedef struct afpLockingContext afpLockingContext;
struct afpLockingContext {
  int reserved;
  const char *dbPath;             /* Name of the open file */     // 打开的文件名
};

struct ByteRangeLockPB2
{
  unsigned long long offset;        /* offset to first byte to lock */   // 锁定第一个字节偏移量
  unsigned long long length;        /* nbr of bytes to lock */      // 锁定的字节数
  unsigned long long retRangeStart; /* nbr of 1st byte locked if successful */      // 成功锁定nbr的第一个字节
  unsigned char unLockFlag;         /* 1 = unlock, 0 = lock */      // 未加锁置为1，锁定置为0
  unsigned char startEndFlag;       /* 1=rel to end of fork, 0=rel to start */    // 端交叉结束置为1，开始置为0
  int fd;                           /* file desc to assoc this lock with */    // 文件用关联文件名扩展描述这个锁
};

#define afpfsByteRangeLock2FSCTL        _IOWR('z', 23, struct ByteRangeLockPB2)

/*
** This is a utility for setting or clearing a bit-range lock on an
** AFP filesystem.
// 这是一个用于在AFP文件系统中设置或清除比特范围内的锁的实用程序
** 
** Return SQLITE_OK on success, SQLITE_BUSY on failure. 
*/     成功返回SQLITE_OK，失败返回SQLITE_BUSY
static int afpSetLock(
  const char *path,         /* Name of the file to be locked or unlocked */   // 锁定或未锁定文件名称
  unixFile *pFile,           /* Open file descriptor on path */     // 在路径中打开文件描述符
  unsigned long long offset,     /* First byte to be locked */     // 锁定第一个字节
  unsigned long long length,     /* Number of bytes to lock */      // 锁定的字节数
  int setLockFlag                /* True to set lock.  False to clear lock */    // 成功就设置锁，失败则清除锁
){
  struct ByteRangeLockPB2 pb;
  int err;
  
  pb.unLockFlag = setLockFlag ? 0 : 1;
  pb.startEndFlag = 0;
  pb.offset = offset;
  pb.length = length; 
  pb.fd = pFile->h;
  
  OSTRACE(("AFPSETLOCK [%s] for %d%s in range %llx:%llx\n", 
    (setLockFlag?"ON":"OFF"), pFile->h, (pb.fd==-1?"[testval-1]":""),
    offset, length));
  err = fsctl(path, afpfsByteRangeLock2FSCTL, &pb, 0);
  if ( err==-1 ) {
    int rc;
    int tErrno = errno;
    OSTRACE(("AFPSETLOCK failed to fsctl() '%s' %d %s\n",
             path, tErrno, strerror(tErrno)));
#ifdef SQLITE_IGNORE_AFP_LOCK_ERRORS
    rc = SQLITE_BUSY;
#else
    rc = sqliteErrorFromPosixError(tErrno,
                    setLockFlag ? SQLITE_IOERR_LOCK : SQLITE_IOERR_UNLOCK);
#endif /* SQLITE_IGNORE_AFP_LOCK_ERRORS */   
// 如果是SQLITE_IGNORE_AFP_LOCK_ERRORS就编译，否则跳过
    if( IS_LOCK_ERROR(rc) ){
      pFile->lastErrno = tErrno;
    }
    return rc;
  } else {
    return SQLITE_OK;
  }
}

/*
** This routine checks if there is a RESERVED lock held on the specified
** file by this or any other process. If such a lock is held, set *pResOut
** to a non-zero value otherwise *pResOut is set to zero.  The return value
** is set to SQLITE_OK unless an I/O error occurs during lock checking.
*/
// 这个程序会检查指定文件或者是其他任何进程里是否持有保留锁，如果持有保留锁，将指针*pResOout的值设为非零，
否则设为0.返回值设为SQLITE_OK除非在锁检查期间发生I/O错误。
static int afpCheckReservedLock(sqlite3_file *id, int *pResOut){
  int rc = SQLITE_OK;
  int reserved = 0;
  unixFile *pFile = (unixFile*)id;
  afpLockingContext *context;
  
  SimulateIOError( return SQLITE_IOERR_CHECKRESERVEDLOCK; );
  
  assert( pFile );
  context = (afpLockingContext *) pFile->lockingContext;
  if( context->reserved ){
    *pResOut = 1;
    return SQLITE_OK;
  }
  unixEnterMutex(); /* Because pFile->pInode is shared across threads */  
// pFile->pInode在线程之间共享
  
  /* Check if a thread in this process holds such a lock */
  if( pFile->pInode->eFileLock>SHARED_LOCK ){
    reserved = 1;
  }
  
  /* Otherwise see if some other process holds it.
   */
  if( !reserved ){
    /* lock the RESERVED byte */     // 锁定保留状态字节
    int lrc = afpSetLock(context->dbPath, pFile, RESERVED_BYTE, 1,1);  
    if( SQLITE_OK==lrc ){
      /* if we succeeded in taking the reserved lock, unlock it to restore
      ** the original state */  
// 如果成功获得保留锁，则解锁并让它恢复到最初状态
      lrc = afpSetLock(context->dbPath, pFile, RESERVED_BYTE, 1, 0);
    } else {
      /* if we failed to get the lock then someone else must have it */   
// 如果获取保留锁失败则表明别的进程持有它
      reserved = 1;
    }
    if( IS_LOCK_ERROR(lrc) ){
      rc=lrc;
    }
  }
  
  unixLeaveMutex();
  OSTRACE(("TEST WR-LOCK %d %d %d (afp)\n", pFile->h, rc, reserved));
  
  *pResOut = reserved;
  return rc;
}

/*
** Lock the file with the lock specified by parameter eFileLock - one
** of the following:
**
// 使用参数eFileLock以下值之一指定的锁对文件加锁
**     (1) SHARED_LOCK   //共享锁
**     (2) RESERVED_LOCK   //保留锁
**     (3) PENDING_LOCK    //未决锁
**     (4) EXCLUSIVE_LOCK   //排他锁
**
** Sometimes when requesting one lock state, additional lock states
** are inserted in between.  The locking might fail on one of the later
** transitions leaving the lock state different from what it started but
** still short of its goal.  The following chart shows the allowed
** transitions and the inserted intermediate states:
**
// 当请求一个锁状态的时候，额外的锁状态会插入到其中。锁定失败后会后来的转换会使它不同于开始时候的锁状态，
但是仍能达到预期目标。以下的记录是允许的转换和插入的状态。
**
**    UNLOCKED -> SHARED   //未加锁-->共享锁
**    SHARED -> RESERVED    //共享锁转为-->保留锁
**    SHARED -> (PENDING) -> EXCLUSIVE   //共享锁-->未决锁（插入）-->排他锁
**    RESERVED -> (PENDING) -> EXCLUSIVE  //保留锁-->未决锁（插入）-->排他锁
**    PENDING -> EXCLUSIVE  //未决锁-->排他锁
**
** This routine will only increase a lock.  Use the sqlite3OsUnlock()
** routine to lower a locking level.
*/
// 这个程序只是增加一个锁。使用sqlite3的OsUnlock()程序降低锁的级别。
static int afpLock(sqlite3_file *id, int eFileLock){
  int rc = SQLITE_OK;
  unixFile *pFile = (unixFile*)id;
  unixInodeInfo *pInode = pFile->pInode;
  afpLockingContext *context = (afpLockingContext *) pFile->lockingContext;
  
  assert( pFile );
  OSTRACE(("LOCK    %d %s was %s(%s,%d) pid=%d (afp)\n", pFile->h,
           azFileLock(eFileLock), azFileLock(pFile->eFileLock),
           azFileLock(pInode->eFileLock), pInode->nShared , getpid()));

  /* If there is already a lock of this type or more restrictive on the
  ** unixFile, do nothing. Don't use the afp_end_lock: exit path, as
  ** unixEnterMutex() hasn't been called yet.
  */
// 如果已经有一个这种类型的锁或unixFile有更多限制性，不做任何操作。不要使用afp_end_lock:退出路径,因为unixEnterMutex()尚未被调用。
  if( pFile->eFileLock>=eFileLock ){
    OSTRACE(("LOCK    %d %s ok (already held) (afp)\n", pFile->h,
           azFileLock(eFileLock)));
    return SQLITE_OK;
  }

  /* Make sure the locking sequence is correct 
  **  (1) We never move from unlocked to anything higher than shared lock.
  **  (2) SQLite never explicitly requests a pendig lock.
  **  (3) A shared lock is always held when a reserve lock is requested.
  */
// 确保锁定顺序是正确的
（1）从未加锁状态只能转移到共享锁
（2）SQLite不会明确请求一个未决锁
（3）当获取到保留锁时，共享锁也一直不能释放
  assert( pFile->eFileLock!=NO_LOCK || eFileLock==SHARED_LOCK );
  assert( eFileLock!=PENDING_LOCK );
  assert( eFileLock!=RESERVED_LOCK || pFile->eFileLock==SHARED_LOCK );
  
  /* This mutex is needed because pFile->pInode is shared across threads
  */    
// 这种互斥是必要的，因为pFile->pInode在线程之间是共享的
  unixEnterMutex();
  pInode = pFile->pInode;

  /* If some thread using this PID has a lock via a different unixFile*
  ** handle that precludes the requested lock, return BUSY.
  */    
// 如果有线程使用拥有一个不同的unixFile*操作的进程ID去排除请求的锁，则返回BUSY。
  if( (pFile->eFileLock!=pInode->eFileLock && 
       (pInode->eFileLock>=PENDING_LOCK || eFileLock>SHARED_LOCK))
     ){
    rc = SQLITE_BUSY;
    goto afp_end_lock;
  }
  
  /* If a SHARED lock is requested, and some thread using this PID already
  ** has a SHARED or RESERVED lock, then increment reference counts and
  ** return SQLITE_OK.
  */   
// 如果要请求一个共享锁，并且一些使用这个进程ID的线程已经持有一个共享锁或者保留锁，那么只需增加引用计数并且返回SQLITE_OK。
  if( eFileLock==SHARED_LOCK && 
     (pInode->eFileLock==SHARED_LOCK || pInode->eFileLock==RESERVED_LOCK) ){
    assert( eFileLock==SHARED_LOCK );
    assert( pFile->eFileLock==0 );
    assert( pInode->nShared>0 );
    pFile->eFileLock = SHARED_LOCK;
    pInode->nShared++;
    pInode->nLock++;
    goto afp_end_lock;
  }
    
  /* A PENDING lock is needed before acquiring a SHARED lock and before
  ** acquiring an EXCLUSIVE lock.  For the SHARED lock, the PENDING will
  ** be released.
  */  
// 在获得共享锁和排他锁之前，需要保持未决锁。获得共享锁之后，未决锁就会被释放掉
  if( eFileLock==SHARED_LOCK 
      || (eFileLock==EXCLUSIVE_LOCK && pFile->eFileLock<PENDING_LOCK)
  ){
    int failed;
    failed = afpSetLock(context->dbPath, pFile, PENDING_BYTE, 1, 1);
    if (failed) {
      rc = failed;
      goto afp_end_lock;
    }
  }
  
  /* If control gets to this point, then actually go ahead and make
  ** operating system calls for the specified lock.
  */
// 如果控制到这个点上，那么继续让操作系统调用指定的锁。
  if( eFileLock==SHARED_LOCK ){
    int lrc1, lrc2, lrc1Errno = 0;
    long lk, mask;
    
    assert( pInode->nShared==0 );
    assert( pInode->eFileLock==0 );
        
    mask = (sizeof(long)==8) ? LARGEST_INT64 : 0x7fffffff;
    /* Now get the read-lock SHARED_LOCK */      // 获取共享锁的读锁
    /* note that the quality of the randomness doesn't matter that much */      // 需要注意的是随机性的质量是无关紧要的
    lk = random(); 
    pInode->sharedByte = (lk & mask)%(SHARED_SIZE - 1);
    lrc1 = afpSetLock(context->dbPath, pFile, 
          SHARED_FIRST+pInode->sharedByte, 1, 1);
    if( IS_LOCK_ERROR(lrc1) ){
      lrc1Errno = pFile->lastErrno;
    }
    /* Drop the temporary PENDING lock */     // 释放临时的未决锁
    lrc2 = afpSetLock(context->dbPath, pFile, PENDING_BYTE, 1, 0);
    
    if( IS_LOCK_ERROR(lrc1) ) {
      pFile->lastErrno = lrc1Errno;
      rc = lrc1;
      goto afp_end_lock;
    } else if( IS_LOCK_ERROR(lrc2) ){
      rc = lrc2;
      goto afp_end_lock;
    } else if( lrc1 != SQLITE_OK ) {
      rc = lrc1;
    } else {
      pFile->eFileLock = SHARED_LOCK;
      pInode->nLock++;
      pInode->nShared = 1;
    }
  }else if( eFileLock==EXCLUSIVE_LOCK && pInode->nShared>1 ){
    /* We are trying for an exclusive lock but another thread in this  
     ** same process is still holding a shared lock. */
// 请求一个排他锁，而这时候同一个进程的其他线程仍然持有共享锁
    rc = SQLITE_BUSY;
  }else{
    /* The request was for a RESERVED or EXCLUSIVE lock.  It is
    ** assumed that there is a SHARED or greater lock on the file
    ** already.
*/   
// 请求保留锁或者排他锁。假设这个文件已经存在一个共享锁或者更高级别的锁。
    int failed = 0;
    assert( 0!=pFile->eFileLock );
    if (eFileLock >= RESERVED_LOCK && pFile->eFileLock < RESERVED_LOCK) {
        /* Acquire a RESERVED lock */     // 获取保留锁
        failed = afpSetLock(context->dbPath, pFile, RESERVED_BYTE, 1,1);
      if( !failed ){
        context->reserved = 1;
      }
    }
    if (!failed && eFileLock == EXCLUSIVE_LOCK) {
      /* Acquire an EXCLUSIVE lock */    // 获取排他锁
        
      /* Remove the shared lock before trying the range.  we'll need to 
      ** reestablish the shared lock if we can't get the  afpUnlock
      */   
// 获取之前要删除共享锁，如果没有得到AFP锁，就需要重新请求共享锁。
      if( !(failed = afpSetLock(context->dbPath, pFile, SHARED_FIRST +
                         pInode->sharedByte, 1, 0)) ){
        int failed2 = SQLITE_OK;
        /* now attemmpt to get the exclusive lock range */    // 再尝试请求排他锁
        failed = afpSetLock(context->dbPath, pFile, SHARED_FIRST, 
                               SHARED_SIZE, 1);
        if( failed && (failed2 = afpSetLock(context->dbPath, pFile, 
                       SHARED_FIRST + pInode->sharedByte, 1, 1)) ){
          /* Can't reestablish the shared lock.  Sqlite can't deal, this is
          ** a critical I/O error  
          */   
// 不能重建共享锁，这是一个Sqlite不能处理的I/O错误
          rc = ((failed & SQLITE_IOERR) == SQLITE_IOERR) ? failed2 : 
               SQLITE_IOERR_LOCK;
          goto afp_end_lock;
        } 
      }else{
        rc = failed; 
      }
    }
    if( failed ){
      rc = failed;
    }
  }
  
  if( rc==SQLITE_OK ){
    pFile->eFileLock = eFileLock;
    pInode->eFileLock = eFileLock;
  }else if( eFileLock==EXCLUSIVE_LOCK ){
    pFile->eFileLock = PENDING_LOCK;
    pInode->eFileLock = PENDING_LOCK;
  }
  
afp_end_lock:
  unixLeaveMutex();
  OSTRACE(("LOCK    %d %s %s (afp)\n", pFile->h, azFileLock(eFileLock), 
         rc==SQLITE_OK ? "ok" : "failed"));
  return rc;
}

/*
** Lower the locking level on file descriptor pFile to eFileLock.  eFileLock
** must be either NO_LOCK or SHARED_LOCK.
**
** If the locking level of the file descriptor is already at or below
** the requested locking level, this routine is a no-op.
*/
// 在文件描述符pFile 到eFileLok降低锁的级别 ，eFileLock必须是NO_LOCK or和SHARED_LOCK两者之一。
如果文件描述符的锁定级别已经在正在请求的锁级别或以下，那么这个程序是一个空操作。

static int afpUnlock(sqlite3_file *id, int eFileLock) {
  int rc = SQLITE_OK;
  unixFile *pFile = (unixFile*)id;
  unixInodeInfo *pInode;
  afpLockingContext *context = (afpLockingContext *) pFile->lockingContext;
  int skipShared = 0;
#ifdef SQLITE_TEST
  int h = pFile->h;
#endif

  assert( pFile );
  OSTRACE(("UNLOCK  %d %d was %d(%d,%d) pid=%d (afp)\n", pFile->h, eFileLock,
           pFile->eFileLock, pFile->pInode->eFileLock, pFile->pInode->nShared,
           getpid()));

  assert( eFileLock<=SHARED_LOCK );
  if( pFile->eFileLock<=eFileLock ){
    return SQLITE_OK;
  }
  unixEnterMutex();
  pInode = pFile->pInode;
  assert( pInode->nShared!=0 );
  if( pFile->eFileLock>SHARED_LOCK ){
    assert( pInode->eFileLock==pFile->eFileLock );
    SimulateIOErrorBenign(1);
    SimulateIOError( h=(-1) )
    SimulateIOErrorBenign(0);
    
#ifdef SQLITE_DEBUG
    /* When reducing a lock such that other processes can start
    ** reading the database file again, make sure that the
    ** transaction counter was updated if any part of the database
    ** file changed.  If the transaction counter is not updated,
    ** other connections to the same file might not realize that
    ** the file has changed and hence might not know to flush their
    ** cache.  The use of a stale cache can lead to database corruption.
*/
// 当锁减少至其它进程可以再读数据库文件时，确保数据库文件有任何变动都要更新  事务计数器。如果事务计数器
// 没有更新，那么访问这个文件的其它连接就不能识别文件的改变，因此可能不会刷新它们的缓存。使用旧的缓存会导
// 致数据库损坏。
    assert( pFile->inNormalWrite==0
           || pFile->dbUpdate==0
           || pFile->transCntrChng==1 );
    pFile->inNormalWrite = 0;
#endif
    
    if( pFile->eFileLock==EXCLUSIVE_LOCK ){
      rc = afpSetLock(context->dbPath, pFile, SHARED_FIRST, SHARED_SIZE, 0);
      if( rc==SQLITE_OK && (eFileLock==SHARED_LOCK || pInode->nShared>1) ){
        /* only re-establish the shared lock if necessary */   如果有必要就只能重建共享锁
        int sharedLockByte = SHARED_FIRST+pInode->sharedByte;
        rc = afpSetLock(context->dbPath, pFile, sharedLockByte, 1, 1);
      } else {
        skipShared = 1;
      }
    }
    if( rc==SQLITE_OK && pFile->eFileLock>=PENDING_LOCK ){
      rc = afpSetLock(context->dbPath, pFile, PENDING_BYTE, 1, 0);
    } 
    if( rc==SQLITE_OK && pFile->eFileLock>=RESERVED_LOCK && context->reserved ){
      rc = afpSetLock(context->dbPath, pFile, RESERVED_BYTE, 1, 0);
      if( !rc ){ 
        context->reserved = 0; 
      }
    }
    if( rc==SQLITE_OK && (eFileLock==SHARED_LOCK || pInode->nShared>1)){
      pInode->eFileLock = SHARED_LOCK;
    }
  }
  if( rc==SQLITE_OK && eFileLock==NO_LOCK ){

    /* Decrement the shared lock counter.  Release the lock using an
    ** OS call only when all threads in this same process have released
    ** the lock.
*/       
// 共享锁计数器进行递减操作。当且仅当同一进程中的所有线程都被释放掉
      // 共享锁，使用一个操作系统调用释放该锁。
    unsigned long long sharedLockByte = SHARED_FIRST+pInode->sharedByte;
    pInode->nShared--;
    if( pInode->nShared==0 ){
      SimulateIOErrorBenign(1);
      SimulateIOError( h=(-1) )
      SimulateIOErrorBenign(0);
      if( !skipShared ){
        rc = afpSetLock(context->dbPath, pFile, sharedLockByte, 1, 0);
      }
      if( !rc ){
        pInode->eFileLock = NO_LOCK;
        pFile->eFileLock = NO_LOCK;
      }
    }
    if( rc==SQLITE_OK ){
      pInode->nLock--;
      assert( pInode->nLock>=0 );
      if( pInode->nLock==0 ){
        closePendingFds(pFile);
      }
    }
  }
  
  unixLeaveMutex();
  if( rc==SQLITE_OK ) pFile->eFileLock = eFileLock;
  return rc;
}

/*
** Close a file & cleanup AFP specific locking context     // 关闭一个文件，并清理AFP特定的锁记录
*/
static int afpClose(sqlite3_file *id) {
  int rc = SQLITE_OK;
  if( id ){
    unixFile *pFile = (unixFile*)id;
    afpUnlock(id, NO_LOCK);
    unixEnterMutex();
    if( pFile->pInode && pFile->pInode->nLock ){
      /* If there are outstanding locks, do not actually close the file just
      ** yet because that would clear those locks.  Instead, add the file
      ** descriptor to pInode->aPending.  It will be automatically closed when
      ** the last lock is cleared.
      */  
// 如果还有未完成的锁，清理完这些锁事前不要关闭文件。相反,添加文件
// 描述符pInode - >a Pending。它将在最后一个锁被清理掉之后自动关闭。
      setPendingFd(pFile);
    }
    releaseInodeInfo(pFile);
    sqlite3_free(pFile->lockingContext);
    rc = closeUnixFile(id);
    unixLeaveMutex();
  }
  return rc;
}

#endif /* defined(__APPLE__) && SQLITE_ENABLE_LOCKING_STYLE */
// 如果是SQLITE_ENABLE_LOCKING_STYLE并且操作系统是APPLE就编译，否则跳过
/*
** The code above is the AFP lock implementation.  The code is specific
** to MacOSX and does not work on other unix platforms.  No alternative
** is available.  If you don't compile for a mac, then the "unix-afp"
** VFS is not available.
**   
// 以上代码是AFP锁的实现，它是MacOSX操作系统特有的，不适合其它的unix平台。没有别的选择，如果不用mac编译，
那么unix-afp的虚拟文件系统就不可用。
********************* End of the AFP lock implementation **********************
******************************************************************************/

/******************************************************************************
*************************** Begin NFS Locking ********************************/
// 网络系统文件锁
#if defined(__APPLE__) && SQLITE_ENABLE_LOCKING_STYLE
/*
 ** Lower the locking level on file descriptor pFile to eFileLock.  eFileLock
 ** must be either NO_LOCK or SHARED_LOCK.
 **
 ** If the locking level of the file descriptor is already at or below
 ** the requested locking level, this routine is a no-op.
 */
static int nfsUnlock(sqlite3_file *id, int eFileLock){
  return posixUnlock(id, eFileLock, 1);
}

#endif /* defined(__APPLE__) && SQLITE_ENABLE_LOCKING_STYLE */
/*
** The code above is the NFS lock implementation.  The code is specific
** to MacOSX and does not work on other unix platforms.  No alternative
** is available.  
// 以上代码是AFP锁的实现，它是MacOSX操作系统特有的，不适合其它的unix平台。没有别的选择，如果不用mac编译，
那么unix-afp的虚拟文件系统就不可用。
**
********************* End of the NFS lock implementation **********************
******************************************************************************/
/******************************************************************************
**************** Non-locking sqlite3_file methods *****************************
**
** The next division contains implementations for all methods of the 
** sqlite3_file object other than the locking methods.  The locking
** methods were defined in divisions above (one locking method per
** division).  Those methods that are common to all locking modes
** are gather together into this division.
*/

/*
** Seek to the offset passed as the second argument, then read cnt 
** bytes into pBuf. Return the number of bytes actually read.
**
** NB:  If you define USE_PREAD or USE_PREAD64, then it might also
** be necessary to define _XOPEN_SOURCE to be 500.  This varies from
** one system to another.  Since SQLite does not define USE_PREAD
** any any form by default, we will not attempt to define _XOPEN_SOURCE.
** See tickets #2741 and #2681.
**
** To avoid stomping the errno value on a failed read the lastErrno value
** is set before returning.
*/
static int seekAndRead(unixFile *id, sqlite3_int64 offset, void *pBuf, int cnt){
  int got;
  int prior = 0;
#if (!defined(USE_PREAD) && !defined(USE_PREAD64))
  i64 newOffset;
#endif
  TIMER_START;
  do{
#if defined(USE_PREAD)
    got = osPread(id->h, pBuf, cnt, offset);
    SimulateIOError( got = -1 );
#elif defined(USE_PREAD64)
    got = osPread64(id->h, pBuf, cnt, offset);
    SimulateIOError( got = -1 );
#else
    newOffset = lseek(id->h, offset, SEEK_SET);
    SimulateIOError( newOffset-- );
    if( newOffset!=offset ){
      if( newOffset == -1 ){
        ((unixFile*)id)->lastErrno = errno;
      }else{
        ((unixFile*)id)->lastErrno = 0;
      }
      return -1;
    }
    got = osRead(id->h, pBuf, cnt);
#endif
    if( got==cnt ) break;
    if( got<0 ){
      if( errno==EINTR ){ got = 1; continue; }
      prior = 0;
      ((unixFile*)id)->lastErrno = errno;
      break;
    }else if( got>0 ){
      cnt -= got;
      offset += got;
      prior += got;
      pBuf = (void*)(got + (char*)pBuf);
    }
  }while( got>0 );
  TIMER_END;
  OSTRACE(("READ    %-3d %5d %7lld %llu\n",
            id->h, got+prior, offset-prior, TIMER_ELAPSED));
  return got+prior;
}

/*
** Read data from a file into a buffer.  Return SQLITE_OK if all
** bytes were read successfully and SQLITE_IOERR if anything goes
** wrong.
*/
static int unixRead(
  sqlite3_file *id, 
  void *pBuf, 
  int amt,
  sqlite3_int64 offset
){
  unixFile *pFile = (unixFile *)id;
  int got;
  assert( id );

  /* If this is a database file (not a journal, master-journal or temp
  ** file), the bytes in the locking range should never be read or written. */
#if 0
  assert( pFile->pUnused==0
       || offset>=PENDING_BYTE+512
       || offset+amt<=PENDING_BYTE 
  );
#endif

  got = seekAndRead(pFile, offset, pBuf, amt);
  if( got==amt ){
    return SQLITE_OK;
  }else if( got<0 ){
    /* lastErrno set by seekAndRead */
    return SQLITE_IOERR_READ;
  }else{
    pFile->lastErrno = 0; /* not a system error */
    /* Unread parts of the buffer must be zero-filled */
    memset(&((char*)pBuf)[got], 0, amt-got);
    return SQLITE_IOERR_SHORT_READ;
  }
}

/*
** Seek to the offset in id->offset then read cnt bytes into pBuf.
** Return the number of bytes actually read.  Update the offset.
**
** To avoid stomping the errno value on a failed write the lastErrno value
** is set before returning.
*/
static int seekAndWrite(unixFile *id, i64 offset, const void *pBuf, int cnt){
  int got;
#if (!defined(USE_PREAD) && !defined(USE_PREAD64))
  i64 newOffset;
#endif
  TIMER_START;
#if defined(USE_PREAD)
  do{ got = osPwrite(id->h, pBuf, cnt, offset); }while( got<0 && errno==EINTR );
#elif defined(USE_PREAD64)
  do{ got = osPwrite64(id->h, pBuf, cnt, offset);}while( got<0 && errno==EINTR);
#else
  do{
    newOffset = lseek(id->h, offset, SEEK_SET);
    SimulateIOError( newOffset-- );
    if( newOffset!=offset ){
      if( newOffset == -1 ){
        ((unixFile*)id)->lastErrno = errno;
      }else{
        ((unixFile*)id)->lastErrno = 0;
      }
      return -1;
    }
    got = osWrite(id->h, pBuf, cnt);
  }while( got<0 && errno==EINTR );
#endif
  TIMER_END;
  if( got<0 ){
    ((unixFile*)id)->lastErrno = errno;
  }

  OSTRACE(("WRITE   %-3d %5d %7lld %llu\n", id->h, got, offset, TIMER_ELAPSED));
  return got;
}


/*
** Write data from a buffer into a file.  Return SQLITE_OK on success
** or some other error code on failure.
*/
static int unixWrite(
  sqlite3_file *id, 
  const void *pBuf, 
  int amt,
  sqlite3_int64 offset 
){
  unixFile *pFile = (unixFile*)id;
  int wrote = 0;
  assert( id );
  assert( amt>0 );

  /* If this is a database file (not a journal, master-journal or temp
  ** file), the bytes in the locking range should never be read or written. */
#if 0
  assert( pFile->pUnused==0
       || offset>=PENDING_BYTE+512
       || offset+amt<=PENDING_BYTE 
  );
#endif

#ifdef SQLITE_DEBUG
  /* If we are doing a normal write to a database file (as opposed to
  ** doing a hot-journal rollback or a write to some file other than a
  ** normal database file) then record the fact that the database
  ** has changed.  If the transaction counter is modified, record that
  ** fact too.
  */
  if( pFile->inNormalWrite ){
    pFile->dbUpdate = 1;  /* The database has been modified */
    if( offset<=24 && offset+amt>=27 ){
      int rc;
      char oldCntr[4];
      SimulateIOErrorBenign(1);
      rc = seekAndRead(pFile, 24, oldCntr, 4);
      SimulateIOErrorBenign(0);
      if( rc!=4 || memcmp(oldCntr, &((char*)pBuf)[24-offset], 4)!=0 ){
        pFile->transCntrChng = 1;  /* The transaction counter has changed */
      }
    }
  }
#endif

  while( amt>0 && (wrote = seekAndWrite(pFile, offset, pBuf, amt))>0 ){
    amt -= wrote;
    offset += wrote;
    pBuf = &((char*)pBuf)[wrote];
  }
  SimulateIOError(( wrote=(-1), amt=1 ));
  SimulateDiskfullError(( wrote=0, amt=1 ));

  if( amt>0 ){
    if( wrote<0 && pFile->lastErrno!=ENOSPC ){
      /* lastErrno set by seekAndWrite */
      return SQLITE_IOERR_WRITE;
    }else{
      pFile->lastErrno = 0; /* not a system error */
      return SQLITE_FULL;
    }
  }

  return SQLITE_OK;
}

#ifdef SQLITE_TEST
/*
** Count the number of fullsyncs and normal syncs.  This is used to test
** that syncs and fullsyncs are occurring at the right times.
*/
int sqlite3_sync_count = 0;
int sqlite3_fullsync_count = 0;
#endif

/*
** We do not trust systems to provide a working fdatasync().  Some do.
** Others do no.  To be safe, we will stick with the (slightly slower)
** fsync(). If you know that your system does support fdatasync() correctly,
** then simply compile with -Dfdatasync=fdatasync
*/
#if !defined(fdatasync)
# define fdatasync fsync
#endif

/*
** Define HAVE_FULLFSYNC to 0 or 1 depending on whether or not
** the F_FULLFSYNC macro is defined.  F_FULLFSYNC is currently
** only available on Mac OS X.  But that could change.
*/
#ifdef F_FULLFSYNC
# define HAVE_FULLFSYNC 1
#else
# define HAVE_FULLFSYNC 0
#endif


/*
** The fsync() system call does not work as advertised on many
** unix systems.  The following procedure is an attempt to make
** it work better.
**
** The SQLITE_NO_SYNC macro disables all fsync()s.  This is useful
** for testing when we want to run through the test suite quickly.
** You are strongly advised *not* to deploy with SQLITE_NO_SYNC
** enabled, however, since with SQLITE_NO_SYNC enabled, an OS crash
** or power failure will likely corrupt the database file.
**
** SQLite sets the dataOnly flag if the size of the file is unchanged.
** The idea behind dataOnly is that it should only write the file content
** to disk, not the inode.  We only set dataOnly if the file size is 
** unchanged since the file size is part of the inode.  However, 
** Ted Ts'o tells us that fdatasync() will also write the inode if the
** file size has changed.  The only real difference between fdatasync()
** and fsync(), Ted tells us, is that fdatasync() will not flush the
** inode if the mtime or owner or other inode attributes have changed.
** We only care about the file size, not the other file attributes, so
** as far as SQLite is concerned, an fdatasync() is always adequate.
** So, we always use fdatasync() if it is available, regardless of
** the value of the dataOnly flag.
*/
static int full_fsync(int fd, int fullSync, int dataOnly){
  int rc;

  /* The following "ifdef/elif/else/" block has the same structure as
  ** the one below. It is replicated here solely to avoid cluttering 
  ** up the real code with the UNUSED_PARAMETER() macros.
  */
#ifdef SQLITE_NO_SYNC
  UNUSED_PARAMETER(fd);
  UNUSED_PARAMETER(fullSync);
  UNUSED_PARAMETER(dataOnly);
#elif HAVE_FULLFSYNC
  UNUSED_PARAMETER(dataOnly);
#else
  UNUSED_PARAMETER(fullSync);
  UNUSED_PARAMETER(dataOnly);
#endif

  /* Record the number of times that we do a normal fsync() and 
  ** FULLSYNC.  This is used during testing to verify that this procedure
  ** gets called with the correct arguments.
  */
#ifdef SQLITE_TEST
  if( fullSync ) sqlite3_fullsync_count++;
  sqlite3_sync_count++;
#endif

  /* If we compiled with the SQLITE_NO_SYNC flag, then syncing is a
  ** no-op
  */
#ifdef SQLITE_NO_SYNC
  rc = SQLITE_OK;
#elif HAVE_FULLFSYNC
  if( fullSync ){
    rc = osFcntl(fd, F_FULLFSYNC, 0);
  }else{
    rc = 1;
  }
  /* If the FULLFSYNC failed, fall back to attempting an fsync().
  ** It shouldn't be possible for fullfsync to fail on the local 
  ** file system (on OSX), so failure indicates that FULLFSYNC
  ** isn't supported for this file system. So, attempt an fsync 
  ** and (for now) ignore the overhead of a superfluous fcntl call.  
  ** It'd be better to detect fullfsync support once and avoid 
  ** the fcntl call every time sync is called.
  */
  if( rc ) rc = fsync(fd);

#elif defined(__APPLE__)
  /* fdatasync() on HFS+ doesn't yet flush the file size if it changed correctly
  ** so currently we default to the macro that redefines fdatasync to fsync
  */
  rc = fsync(fd);
#else 
  rc = fdatasync(fd);
#if OS_VXWORKS
  if( rc==-1 && errno==ENOTSUP ){
    rc = fsync(fd);
  }
#endif /* OS_VXWORKS */
#endif /* ifdef SQLITE_NO_SYNC elif HAVE_FULLFSYNC */

  if( OS_VXWORKS && rc!= -1 ){
    rc = 0;
  }
  return rc;
}

/*
** Open a file descriptor to the directory containing file zFilename.
** If successful, *pFd is set to the opened file descriptor and
** SQLITE_OK is returned. If an error occurs, either SQLITE_NOMEM
** or SQLITE_CANTOPEN is returned and *pFd is set to an undefined
** value.
**
** The directory file descriptor is used for only one thing - to
** fsync() a directory to make sure file creation and deletion events
** are flushed to disk.  Such fsyncs are not needed on newer
** journaling filesystems, but are required on older filesystems.
**
** This routine can be overridden using the xSetSysCall interface.
** The ability to override this routine was added in support of the
** chromium sandbox.  Opening a directory is a security risk (we are
** told) so making it overrideable allows the chromium sandbox to
** replace this routine with a harmless no-op.  To make this routine
** a no-op, replace it with a stub that returns SQLITE_OK but leaves
** *pFd set to a negative number.
**
** If SQLITE_OK is returned, the caller is responsible for closing
** the file descriptor *pFd using close().
*/
static int openDirectory(const char *zFilename, int *pFd){
  int ii;
  int fd = -1;
  char zDirname[MAX_PATHNAME+1];

  sqlite3_snprintf(MAX_PATHNAME, zDirname, "%s", zFilename);
  for(ii=(int)strlen(zDirname); ii>1 && zDirname[ii]!='/'; ii--);
  if( ii>0 ){
    zDirname[ii] = '\0';
    fd = robust_open(zDirname, O_RDONLY|O_BINARY, 0);
    if( fd>=0 ){
      OSTRACE(("OPENDIR %-3d %s\n", fd, zDirname));
    }
  }
  *pFd = fd;
  return (fd>=0?SQLITE_OK:unixLogError(SQLITE_CANTOPEN_BKPT, "open", zDirname));
}

/*
** Make sure all writes to a particular file are committed to disk.
**
** If dataOnly==0 then both the file itself and its metadata (file
** size, access time, etc) are synced.  If dataOnly!=0 then only the
** file data is synced.
**
** Under Unix, also make sure that the directory entry for the file
** has been created by fsync-ing the directory that contains the file.
** If we do not do this and we encounter a power failure, the directory
** entry for the journal might not exist after we reboot.  The next
** SQLite to access the file will not know that the journal exists (because
** the directory entry for the journal was never created) and the transaction
** will not roll back - possibly leading to database corruption.
*/
static int unixSync(sqlite3_file *id, int flags){
  int rc;
  unixFile *pFile = (unixFile*)id;

  int isDataOnly = (flags&SQLITE_SYNC_DATAONLY);
  int isFullsync = (flags&0x0F)==SQLITE_SYNC_FULL;

  /* Check that one of SQLITE_SYNC_NORMAL or FULL was passed */
  assert((flags&0x0F)==SQLITE_SYNC_NORMAL
      || (flags&0x0F)==SQLITE_SYNC_FULL
  );

  /* Unix cannot, but some systems may return SQLITE_FULL from here. This
  ** line is to test that doing so does not cause any problems.
  */
  SimulateDiskfullError( return SQLITE_FULL );

  assert( pFile );
  OSTRACE(("SYNC    %-3d\n", pFile->h));
  rc = full_fsync(pFile->h, isFullsync, isDataOnly);
  SimulateIOError( rc=1 );
  if( rc ){
    pFile->lastErrno = errno;
    return unixLogError(SQLITE_IOERR_FSYNC, "full_fsync", pFile->zPath);
  }

  /* Also fsync the directory containing the file if the DIRSYNC flag
  ** is set.  This is a one-time occurrance.  Many systems (examples: AIX)
  ** are unable to fsync a directory, so ignore errors on the fsync.
  */
  if( pFile->ctrlFlags & UNIXFILE_DIRSYNC ){
    int dirfd;
    OSTRACE(("DIRSYNC %s (have_fullfsync=%d fullsync=%d)\n", pFile->zPath,
            HAVE_FULLFSYNC, isFullsync));
    rc = osOpenDirectory(pFile->zPath, &dirfd);
    if( rc==SQLITE_OK && dirfd>=0 ){
      full_fsync(dirfd, 0, 0);
      robust_close(pFile, dirfd, __LINE__);
    }else if( rc==SQLITE_CANTOPEN ){
      rc = SQLITE_OK;
    }
    pFile->ctrlFlags &= ~UNIXFILE_DIRSYNC;
  }
  return rc;
}

/*
** Truncate an open file to a specified size
*/
static int unixTruncate(sqlite3_file *id, i64 nByte){
  unixFile *pFile = (unixFile *)id;
  int rc;
  assert( pFile );
  SimulateIOError( return SQLITE_IOERR_TRUNCATE );

  /* If the user has configured a chunk-size for this file, truncate the
  ** file so that it consists of an integer number of chunks (i.e. the
  ** actual file size after the operation may be larger than the requested
  ** size).
  */
  if( pFile->szChunk>0 ){
    nByte = ((nByte + pFile->szChunk - 1)/pFile->szChunk) * pFile->szChunk;
  }

  rc = robust_ftruncate(pFile->h, (off_t)nByte);
  if( rc ){
    pFile->lastErrno = errno;
    return unixLogError(SQLITE_IOERR_TRUNCATE, "ftruncate", pFile->zPath);
  }else{
#ifdef SQLITE_DEBUG
    /* If we are doing a normal write to a database file (as opposed to
    ** doing a hot-journal rollback or a write to some file other than a
    ** normal database file) and we truncate the file to zero length,
    ** that effectively updates the change counter.  This might happen
    ** when restoring a database using the backup API from a zero-length
    ** source.
    */
    if( pFile->inNormalWrite && nByte==0 ){
      pFile->transCntrChng = 1;
    }
#endif

    return SQLITE_OK;
  }
}

/*
** Determine the current size of a file in bytes
*/
static int unixFileSize(sqlite3_file *id, i64 *pSize){
  int rc;
  struct stat buf;
  assert( id );
  rc = osFstat(((unixFile*)id)->h, &buf);
  SimulateIOError( rc=1 );
  if( rc!=0 ){
    ((unixFile*)id)->lastErrno = errno;
    return SQLITE_IOERR_FSTAT;
  }
  *pSize = buf.st_size;

  /* When opening a zero-size database, the findInodeInfo() procedure
  ** writes a single byte into that file in order to work around a bug
  ** in the OS-X msdos filesystem.  In order to avoid problems with upper
  ** layers, we need to report this file size as zero even though it is
  ** really 1.   Ticket #3260.
  */
  if( *pSize==1 ) *pSize = 0;


  return SQLITE_OK;
}

#if SQLITE_ENABLE_LOCKING_STYLE && defined(__APPLE__)
/*
** Handler for proxy-locking file-control verbs.  Defined below in the
** proxying locking division.
*/
static int proxyFileControl(sqlite3_file*,int,void*);
#endif

/* 
** This function is called to handle the SQLITE_FCNTL_SIZE_HINT 
** file-control operation.  Enlarge the database to nBytes in size
** (rounded up to the next chunk-size).  If the database is already
** nBytes or larger, this routine is a no-op.
*/
static int fcntlSizeHint(unixFile *pFile, i64 nByte){
  if( pFile->szChunk>0 ){
    i64 nSize;                    /* Required file size */
    struct stat buf;              /* Used to hold return values of fstat() */
   
    if( osFstat(pFile->h, &buf) ) return SQLITE_IOERR_FSTAT;

    nSize = ((nByte+pFile->szChunk-1) / pFile->szChunk) * pFile->szChunk;
    if( nSize>(i64)buf.st_size ){

#if defined(HAVE_POSIX_FALLOCATE) && HAVE_POSIX_FALLOCATE
      /* The code below is handling the return value of osFallocate() 
      ** correctly. posix_fallocate() is defined to "returns zero on success, 
      ** or an error number on  failure". See the manpage for details. */
      int err;
      do{
        err = osFallocate(pFile->h, buf.st_size, nSize-buf.st_size);
      }while( err==EINTR );
      if( err ) return SQLITE_IOERR_WRITE;
#else
      /* If the OS does not have posix_fallocate(), fake it. First use
      ** ftruncate() to set the file size, then write a single byte to
      ** the last byte in each block within the extended region. This
      ** is the same technique used by glibc to implement posix_fallocate()
      ** on systems that do not have a real fallocate() system call.
      */
      int nBlk = buf.st_blksize;  /* File-system block size */
      i64 iWrite;                 /* Next offset to write to */

      if( robust_ftruncate(pFile->h, nSize) ){
        pFile->lastErrno = errno;
        return unixLogError(SQLITE_IOERR_TRUNCATE, "ftruncate", pFile->zPath);
      }
      iWrite = ((buf.st_size + 2*nBlk - 1)/nBlk)*nBlk-1;
      while( iWrite<nSize ){
        int nWrite = seekAndWrite(pFile, iWrite, "", 1);
        if( nWrite!=1 ) return SQLITE_IOERR_WRITE;
        iWrite += nBlk;
      }
#endif
    }
  }

  return SQLITE_OK;
}

/*
** If *pArg is inititially negative then this is a query.  Set *pArg to
** 1 or 0 depending on whether or not bit mask of pFile->ctrlFlags is set.
**
** If *pArg is 0 or 1, then clear or set the mask bit of pFile->ctrlFlags.
*/
static void unixModeBit(unixFile *pFile, unsigned char mask, int *pArg){
  if( *pArg<0 ){
    *pArg = (pFile->ctrlFlags & mask)!=0;
  }else if( (*pArg)==0 ){
    pFile->ctrlFlags &= ~mask;
  }else{
    pFile->ctrlFlags |= mask;
  }
}

/*
** Information and control of an open file handle.
*/
static int unixFileControl(sqlite3_file *id, int op, void *pArg){
  unixFile *pFile = (unixFile*)id;
  switch( op ){
    case SQLITE_FCNTL_LOCKSTATE: {
      *(int*)pArg = pFile->eFileLock;
      return SQLITE_OK;
    }
    case SQLITE_LAST_ERRNO: {
      *(int*)pArg = pFile->lastErrno;
      return SQLITE_OK;
    }
    case SQLITE_FCNTL_CHUNK_SIZE: {
      pFile->szChunk = *(int *)pArg;
      return SQLITE_OK;
    }
    case SQLITE_FCNTL_SIZE_HINT: {
      int rc;
      SimulateIOErrorBenign(1);
      rc = fcntlSizeHint(pFile, *(i64 *)pArg);
      SimulateIOErrorBenign(0);
      return rc;
    }
    case SQLITE_FCNTL_PERSIST_WAL: {
      unixModeBit(pFile, UNIXFILE_PERSIST_WAL, (int*)pArg);
      return SQLITE_OK;
    }
    case SQLITE_FCNTL_POWERSAFE_OVERWRITE: {
      unixModeBit(pFile, UNIXFILE_PSOW, (int*)pArg);
      return SQLITE_OK;
    }
    case SQLITE_FCNTL_VFSNAME: {
      *(char**)pArg = sqlite3_mprintf("%s", pFile->pVfs->zName);
      return SQLITE_OK;
    }
#ifdef SQLITE_DEBUG
    /* The pager calls this method to signal that it has done
    ** a rollback and that the database is therefore unchanged and
    ** it hence it is OK for the transaction change counter to be
    ** unchanged.
    */
    case SQLITE_FCNTL_DB_UNCHANGED: {
      ((unixFile*)id)->dbUpdate = 0;
      return SQLITE_OK;
    }
#endif
#if SQLITE_ENABLE_LOCKING_STYLE && defined(__APPLE__)
    case SQLITE_SET_LOCKPROXYFILE:
    case SQLITE_GET_LOCKPROXYFILE: {
      return proxyFileControl(id,op,pArg);
    }
#endif /* SQLITE_ENABLE_LOCKING_STYLE && defined(__APPLE__) */
  }
  return SQLITE_NOTFOUND;
}

/*
** Return the sector size in bytes of the underlying block device for
** the specified file. This is almost always 512 bytes, but may be
** larger for some devices.
**
** SQLite code assumes this function cannot fail. It also assumes that
** if two files are created in the same file-system directory (i.e.
** a database and its journal file) that the sector size will be the
** same for both.
*/
static int unixSectorSize(sqlite3_file *pFile){
  (void)pFile;
  return SQLITE_DEFAULT_SECTOR_SIZE;
}

/*
** Return the device characteristics for the file.
**
** This VFS is set up to return SQLITE_IOCAP_POWERSAFE_OVERWRITE by default.
** However, that choice is contraversial since technically the underlying
** file system does not always provide powersafe overwrites.  (In other
** words, after a power-loss event, parts of the file that were never
** written might end up being altered.)  However, non-PSOW behavior is very,
** very rare.  And asserting PSOW makes a large reduction in the amount
** of required I/O for journaling, since a lot of padding is eliminated.
**  Hence, while POWERSAFE_OVERWRITE is on by default, there is a file-control
** available to turn it off and URI query parameter available to turn it off.
*/
static int unixDeviceCharacteristics(sqlite3_file *id){
  unixFile *p = (unixFile*)id;
  if( p->ctrlFlags & UNIXFILE_PSOW ){
    return SQLITE_IOCAP_POWERSAFE_OVERWRITE;
  }else{
    return 0;
  }
}

#ifndef SQLITE_OMIT_WAL


/*
** Object used to represent an shared memory buffer.  
**
** When multiple threads all reference the same wal-index, each thread
** has its own unixShm object, but they all point to a single instance
** of this unixShmNode object.  In other words, each wal-index is opened
** only once per process.
**
** Each unixShmNode object is connected to a single unixInodeInfo object.
** We could coalesce this object into unixInodeInfo, but that would mean
** every open file that does not use shared memory (in other words, most
** open files) would have to carry around this extra information.  So
** the unixInodeInfo object contains a pointer to this unixShmNode object
** and the unixShmNode object is created only when needed.
**
** unixMutexHeld() must be true when creating or destroying
** this object or while reading or writing the following fields:
**
**      nRef
**
** The following fields are read-only after the object is created:
** 
**      fid
**      zFilename
**
** Either unixShmNode.mutex must be held or unixShmNode.nRef==0 and
** unixMutexHeld() is true when reading or writing any other field
** in this structure.
*/
struct unixShmNode {
  unixInodeInfo *pInode;     /* unixInodeInfo that owns this SHM node */
  sqlite3_mutex *mutex;      /* Mutex to access this object */
  char *zFilename;           /* Name of the mmapped file */
  int h;                     /* Open file descriptor */
  int szRegion;              /* Size of shared-memory regions */
  u16 nRegion;               /* Size of array apRegion */
  u8 isReadonly;             /* True if read-only */
  char **apRegion;           /* Array of mapped shared-memory regions */
  int nRef;                  /* Number of unixShm objects pointing to this */
  unixShm *pFirst;           /* All unixShm objects pointing to this */
#ifdef SQLITE_DEBUG
  u8 exclMask;               /* Mask of exclusive locks held */
  u8 sharedMask;             /* Mask of shared locks held */
  u8 nextShmId;              /* Next available unixShm.id value */
#endif
};

/*
** Structure used internally by this VFS to record the state of an
** open shared memory connection.
**
** The following fields are initialized when this object is created and
** are read-only thereafter:
**
**    unixShm.pFile
**    unixShm.id
**
** All other fields are read/write.  The unixShm.pFile->mutex must be held
** while accessing any read/write fields.
*/
struct unixShm {
  unixShmNode *pShmNode;     /* The underlying unixShmNode object */
  unixShm *pNext;            /* Next unixShm with the same unixShmNode */
  u8 hasMutex;               /* True if holding the unixShmNode mutex */
  u8 id;                     /* Id of this connection within its unixShmNode */
  u16 sharedMask;            /* Mask of shared locks held */
  u16 exclMask;              /* Mask of exclusive locks held */
};

/*
** Constants used for locking
*/
#define UNIX_SHM_BASE   ((22+SQLITE_SHM_NLOCK)*4)         /* first lock byte */
#define UNIX_SHM_DMS    (UNIX_SHM_BASE+SQLITE_SHM_NLOCK)  /* deadman switch */

/*
** Apply posix advisory locks for all bytes from ofst through ofst+n-1.
**
** Locks block if the mask is exactly UNIX_SHM_C and are non-blocking
** otherwise.
*/
static int unixShmSystemLock(
  unixShmNode *pShmNode, /* Apply locks to this open shared-memory segment */
  int lockType,          /* F_UNLCK, F_RDLCK, or F_WRLCK */
  int ofst,              /* First byte of the locking range */
  int n                  /* Number of bytes to lock */
){
  struct flock f;       /* The posix advisory locking structure */
  int rc = SQLITE_OK;   /* Result code form fcntl() */

  /* Access to the unixShmNode object is serialized by the caller */
  assert( sqlite3_mutex_held(pShmNode->mutex) || pShmNode->nRef==0 );

  /* Shared locks never span more than one byte */
  assert( n==1 || lockType!=F_RDLCK );

  /* Locks are within range */
  assert( n>=1 && n<SQLITE_SHM_NLOCK );

  if( pShmNode->h>=0 ){
    /* Initialize the locking parameters */
    memset(&f, 0, sizeof(f));
    f.l_type = lockType;
    f.l_whence = SEEK_SET;
    f.l_start = ofst;
    f.l_len = n;

    rc = osFcntl(pShmNode->h, F_SETLK, &f);
    rc = (rc!=(-1)) ? SQLITE_OK : SQLITE_BUSY;
  }

  /* Update the global lock state and do debug tracing */
#ifdef SQLITE_DEBUG
  { u16 mask;
  OSTRACE(("SHM-LOCK "));
  mask = (1<<(ofst+n)) - (1<<ofst);
  if( rc==SQLITE_OK ){
    if( lockType==F_UNLCK ){
      OSTRACE(("unlock %d ok", ofst));
      pShmNode->exclMask &= ~mask;
      pShmNode->sharedMask &= ~mask;
    }else if( lockType==F_RDLCK ){
      OSTRACE(("read-lock %d ok", ofst));
      pShmNode->exclMask &= ~mask;
      pShmNode->sharedMask |= mask;
    }else{
      assert( lockType==F_WRLCK );
      OSTRACE(("write-lock %d ok", ofst));
      pShmNode->exclMask |= mask;
      pShmNode->sharedMask &= ~mask;
    }
  }else{
    if( lockType==F_UNLCK ){
      OSTRACE(("unlock %d failed", ofst));
    }else if( lockType==F_RDLCK ){
      OSTRACE(("read-lock failed"));
    }else{
      assert( lockType==F_WRLCK );
      OSTRACE(("write-lock %d failed", ofst));
    }
  }
  OSTRACE((" - afterwards %03x,%03x\n",
           pShmNode->sharedMask, pShmNode->exclMask));
  }
#endif

  return rc;        
}


/*
** Purge the unixShmNodeList list of all entries with unixShmNode.nRef==0.
**
** This is not a VFS shared-memory method; it is a utility function called
** by VFS shared-memory methods.
*/
static void unixShmPurge(unixFile *pFd){
  unixShmNode *p = pFd->pInode->pShmNode;
  assert( unixMutexHeld() );
  if( p && p->nRef==0 ){
    int i;
    assert( p->pInode==pFd->pInode );
    sqlite3_mutex_free(p->mutex);
    for(i=0; i<p->nRegion; i++){
      if( p->h>=0 ){
        munmap(p->apRegion[i], p->szRegion);
      }else{
        sqlite3_free(p->apRegion[i]);
      }
    }
    sqlite3_free(p->apRegion);
    if( p->h>=0 ){
      robust_close(pFd, p->h, __LINE__);
      p->h = -1;
    }
    p->pInode->pShmNode = 0;
    sqlite3_free(p);
  }
}

/*
** Open a shared-memory area associated with open database file pDbFd.  
** This particular implementation uses mmapped files.
**
** The file used to implement shared-memory is in the same directory
** as the open database file and has the same name as the open database
** file with the "-shm" suffix added.  For example, if the database file
** is "/home/user1/config.db" then the file that is created and mmapped
** for shared memory will be called "/home/user1/config.db-shm".  
**
** Another approach to is to use files in /dev/shm or /dev/tmp or an
** some other tmpfs mount. But if a file in a different directory
** from the database file is used, then differing access permissions
** or a chroot() might cause two different processes on the same
** database to end up using different files for shared memory - 
** meaning that their memory would not really be shared - resulting
** in database corruption.  Nevertheless, this tmpfs file usage
** can be enabled at compile-time using -DSQLITE_SHM_DIRECTORY="/dev/shm"
** or the equivalent.  The use of the SQLITE_SHM_DIRECTORY compile-time
** option results in an incompatible build of SQLite;  builds of SQLite
** that with differing SQLITE_SHM_DIRECTORY settings attempt to use the
** same database file at the same time, database corruption will likely
** result. The SQLITE_SHM_DIRECTORY compile-time option is considered
** "unsupported" and may go away in a future SQLite release.
**
** When opening a new shared-memory file, if no other instances of that
** file are currently open, in this process or in other processes, then
** the file must be truncated to zero length or have its header cleared.
**
** If the original database file (pDbFd) is using the "unix-excl" VFS
** that means that an exclusive lock is held on the database file and
** that no other processes are able to read or write the database.  In
** that case, we do not really need shared memory.  No shared memory
** file is created.  The shared memory will be simulated with heap memory.
*/
static int unixOpenSharedMemory(unixFile *pDbFd){
  struct unixShm *p = 0;          /* The connection to be opened */
  struct unixShmNode *pShmNode;   /* The underlying mmapped file */
  int rc;                         /* Result code */
  unixInodeInfo *pInode;          /* The inode of fd */
  char *zShmFilename;             /* Name of the file used for SHM */
  int nShmFilename;               /* Size of the SHM filename in bytes */

  /* Allocate space for the new unixShm object. */
  p = sqlite3_malloc( sizeof(*p) );
  if( p==0 ) return SQLITE_NOMEM;
  memset(p, 0, sizeof(*p));
  assert( pDbFd->pShm==0 );

  /* Check to see if a unixShmNode object already exists. Reuse an existing
  ** one if present. Create a new one if necessary.
  */
  unixEnterMutex();
  pInode = pDbFd->pInode;
  pShmNode = pInode->pShmNode;
  if( pShmNode==0 ){
    struct stat sStat;                 /* fstat() info for database file */

    /* Call fstat() to figure out the permissions on the database file. If
    ** a new *-shm file is created, an attempt will be made to create it
    ** with the same permissions.
    */
    if( osFstat(pDbFd->h, &sStat) && pInode->bProcessLock==0 ){
      rc = SQLITE_IOERR_FSTAT;
      goto shm_open_err;
    }

#ifdef SQLITE_SHM_DIRECTORY
    nShmFilename = sizeof(SQLITE_SHM_DIRECTORY) + 31;
#else
    nShmFilename = 6 + (int)strlen(pDbFd->zPath);
#endif
    pShmNode = sqlite3_malloc( sizeof(*pShmNode) + nShmFilename );
    if( pShmNode==0 ){
      rc = SQLITE_NOMEM;
      goto shm_open_err;
    }
    memset(pShmNode, 0, sizeof(*pShmNode)+nShmFilename);
    zShmFilename = pShmNode->zFilename = (char*)&pShmNode[1];
#ifdef SQLITE_SHM_DIRECTORY
    sqlite3_snprintf(nShmFilename, zShmFilename, 
                     SQLITE_SHM_DIRECTORY "/sqlite-shm-%x-%x",
                     (u32)sStat.st_ino, (u32)sStat.st_dev);
#else
    sqlite3_snprintf(nShmFilename, zShmFilename, "%s-shm", pDbFd->zPath);
    sqlite3FileSuffix3(pDbFd->zPath, zShmFilename);
#endif
    pShmNode->h = -1;
    pDbFd->pInode->pShmNode = pShmNode;
    pShmNode->pInode = pDbFd->pInode;
    pShmNode->mutex = sqlite3_mutex_alloc(SQLITE_MUTEX_FAST);
    if( pShmNode->mutex==0 ){
      rc = SQLITE_NOMEM;
      goto shm_open_err;
    }

    if( pInode->bProcessLock==0 ){
      int openFlags = O_RDWR | O_CREAT;
      if( sqlite3_uri_boolean(pDbFd->zPath, "readonly_shm", 0) ){
        openFlags = O_RDONLY;
        pShmNode->isReadonly = 1;
      }
      pShmNode->h = robust_open(zShmFilename, openFlags, (sStat.st_mode&0777));
      if( pShmNode->h<0 ){
        rc = unixLogError(SQLITE_CANTOPEN_BKPT, "open", zShmFilename);
        goto shm_open_err;
      }

      /* If this process is running as root, make sure that the SHM file
      ** is owned by the same user that owns the original database.  Otherwise,
      ** the original owner will not be able to connect.
      */
      osFchown(pShmNode->h, sStat.st_uid, sStat.st_gid);
  
      /* Check to see if another process is holding the dead-man switch.
      ** If not, truncate the file to zero length. 
      */
      rc = SQLITE_OK;
      if( unixShmSystemLock(pShmNode, F_WRLCK, UNIX_SHM_DMS, 1)==SQLITE_OK ){
        if( robust_ftruncate(pShmNode->h, 0) ){
          rc = unixLogError(SQLITE_IOERR_SHMOPEN, "ftruncate", zShmFilename);
        }
      }
      if( rc==SQLITE_OK ){
        rc = unixShmSystemLock(pShmNode, F_RDLCK, UNIX_SHM_DMS, 1);
      }
      if( rc ) goto shm_open_err;
    }
  }

  /* Make the new connection a child of the unixShmNode */
  p->pShmNode = pShmNode;
#ifdef SQLITE_DEBUG
  p->id = pShmNode->nextShmId++;
#endif
  pShmNode->nRef++;
  pDbFd->pShm = p;
  unixLeaveMutex();

  /* The reference count on pShmNode has already been incremented under
  ** the cover of the unixEnterMutex() mutex and the pointer from the
  ** new (struct unixShm) object to the pShmNode has been set. All that is
  ** left to do is to link the new object into the linked list starting
  ** at pShmNode->pFirst. This must be done while holding the pShmNode->mutex 
  ** mutex.
  */
  sqlite3_mutex_enter(pShmNode->mutex);
  p->pNext = pShmNode->pFirst;
  pShmNode->pFirst = p;
  sqlite3_mutex_leave(pShmNode->mutex);
  return SQLITE_OK;

  /* Jump here on any error */
shm_open_err:
  unixShmPurge(pDbFd);       /* This call frees pShmNode if required */
  sqlite3_free(p);
  unixLeaveMutex();
  return rc;
}

/*
** This function is called to obtain a pointer to region iRegion of the 
** shared-memory associated with the database file fd. Shared-memory regions 
** are numbered starting from zero. Each shared-memory region is szRegion 
** bytes in size.
**
** If an error occurs, an error code is returned and *pp is set to NULL.
**
** Otherwise, if the bExtend parameter is 0 and the requested shared-memory
** region has not been allocated (by any client, including one running in a
** separate process), then *pp is set to NULL and SQLITE_OK returned. If 
** bExtend is non-zero and the requested shared-memory region has not yet 
** been allocated, it is allocated by this function.
**
** If the shared-memory region has already been allocated or is allocated by
** this call as described above, then it is mapped into this processes 
** address space (if it is not already), *pp is set to point to the mapped 
** memory and SQLITE_OK returned.
*/
static int unixShmMap(
  sqlite3_file *fd,               /* Handle open on database file */
  int iRegion,                    /* Region to retrieve */
  int szRegion,                   /* Size of regions */
  int bExtend,                    /* True to extend file if necessary */
  void volatile **pp              /* OUT: Mapped memory */
){
  unixFile *pDbFd = (unixFile*)fd;
  unixShm *p;
  unixShmNode *pShmNode;
  int rc = SQLITE_OK;

  /* If the shared-memory file has not yet been opened, open it now. */
  if( pDbFd->pShm==0 ){
    rc = unixOpenSharedMemory(pDbFd);
    if( rc!=SQLITE_OK ) return rc;
  }

  p = pDbFd->pShm;
  pShmNode = p->pShmNode;
  sqlite3_mutex_enter(pShmNode->mutex);
  assert( szRegion==pShmNode->szRegion || pShmNode->nRegion==0 );
  assert( pShmNode->pInode==pDbFd->pInode );
  assert( pShmNode->h>=0 || pDbFd->pInode->bProcessLock==1 );
  assert( pShmNode->h<0 || pDbFd->pInode->bProcessLock==0 );

  if( pShmNode->nRegion<=iRegion ){
    char **apNew;                      /* New apRegion[] array */
    int nByte = (iRegion+1)*szRegion;  /* Minimum required file size */
    struct stat sStat;                 /* Used by fstat() */

    pShmNode->szRegion = szRegion;

    if( pShmNode->h>=0 ){
      /* The requested region is not mapped into this processes address space.
      ** Check to see if it has been allocated (i.e. if the wal-index file is
      ** large enough to contain the requested region).
      */
      if( osFstat(pShmNode->h, &sStat) ){
        rc = SQLITE_IOERR_SHMSIZE;
        goto shmpage_out;
      }
  
      if( sStat.st_size<nByte ){
        /* The requested memory region does not exist. If bExtend is set to
        ** false, exit early. *pp will be set to NULL and SQLITE_OK returned.
        **
        ** Alternatively, if bExtend is true, use ftruncate() to allocate
        ** the requested memory region.
        */
        if( !bExtend ) goto shmpage_out;
        if( robust_ftruncate(pShmNode->h, nByte) ){
          rc = unixLogError(SQLITE_IOERR_SHMSIZE, "ftruncate",
                            pShmNode->zFilename);
          goto shmpage_out;
        }
      }
    }

    /* Map the requested memory region into this processes address space. */
    apNew = (char **)sqlite3_realloc(
        pShmNode->apRegion, (iRegion+1)*sizeof(char *)
    );
    if( !apNew ){
      rc = SQLITE_IOERR_NOMEM;
      goto shmpage_out;
    }
    pShmNode->apRegion = apNew;
    while(pShmNode->nRegion<=iRegion){
      void *pMem;
      if( pShmNode->h>=0 ){
        pMem = mmap(0, szRegion,
            pShmNode->isReadonly ? PROT_READ : PROT_READ|PROT_WRITE, 
            MAP_SHARED, pShmNode->h, pShmNode->nRegion*szRegion
        );
        if( pMem==MAP_FAILED ){
          rc = unixLogError(SQLITE_IOERR_SHMMAP, "mmap", pShmNode->zFilename);
          goto shmpage_out;
        }
      }else{
        pMem = sqlite3_malloc(szRegion);
        if( pMem==0 ){
          rc = SQLITE_NOMEM;
          goto shmpage_out;
        }
        memset(pMem, 0, szRegion);
      }
      pShmNode->apRegion[pShmNode->nRegion] = pMem;
      pShmNode->nRegion++;
    }
  }

shmpage_out:
  if( pShmNode->nRegion>iRegion ){
    *pp = pShmNode->apRegion[iRegion];
  }else{
    *pp = 0;
  }
  if( pShmNode->isReadonly && rc==SQLITE_OK ) rc = SQLITE_READONLY;
  sqlite3_mutex_leave(pShmNode->mutex);
  return rc;
}

/*
** Change the lock state for a shared-memory segment.
**
** Note that the relationship between SHAREd and EXCLUSIVE locks is a little
** different here than in posix.  In xShmLock(), one can go from unlocked
** to shared and back or from unlocked to exclusive and back.  But one may
** not go from shared to exclusive or from exclusive to shared.
*/
static int unixShmLock(
  sqlite3_file *fd,          /* Database file holding the shared memory */
  int ofst,                  /* First lock to acquire or release */
  int n,                     /* Number of locks to acquire or release */
  int flags                  /* What to do with the lock */
){
  unixFile *pDbFd = (unixFile*)fd;      /* Connection holding shared memory */
  unixShm *p = pDbFd->pShm;             /* The shared memory being locked */
  unixShm *pX;                          /* For looping over all siblings */
  unixShmNode *pShmNode = p->pShmNode;  /* The underlying file iNode */
  int rc = SQLITE_OK;                   /* Result code */
  u16 mask;                             /* Mask of locks to take or release */

  assert( pShmNode==pDbFd->pInode->pShmNode );
  assert( pShmNode->pInode==pDbFd->pInode );
  assert( ofst>=0 && ofst+n<=SQLITE_SHM_NLOCK );
  assert( n>=1 );
  assert( flags==(SQLITE_SHM_LOCK | SQLITE_SHM_SHARED)
       || flags==(SQLITE_SHM_LOCK | SQLITE_SHM_EXCLUSIVE)
       || flags==(SQLITE_SHM_UNLOCK | SQLITE_SHM_SHARED)
       || flags==(SQLITE_SHM_UNLOCK | SQLITE_SHM_EXCLUSIVE) );
  assert( n==1 || (flags & SQLITE_SHM_EXCLUSIVE)!=0 );
  assert( pShmNode->h>=0 || pDbFd->pInode->bProcessLock==1 );
  assert( pShmNode->h<0 || pDbFd->pInode->bProcessLock==0 );

  mask = (1<<(ofst+n)) - (1<<ofst);
  assert( n>1 || mask==(1<<ofst) );
  sqlite3_mutex_enter(pShmNode->mutex);
  if( flags & SQLITE_SHM_UNLOCK ){
    u16 allMask = 0; /* Mask of locks held by siblings */

    /* See if any siblings hold this same lock */
    for(pX=pShmNode->pFirst; pX; pX=pX->pNext){
      if( pX==p ) continue;
      assert( (pX->exclMask & (p->exclMask|p->sharedMask))==0 );
      allMask |= pX->sharedMask;
    }

    /* Unlock the system-level locks */
    if( (mask & allMask)==0 ){
      rc = unixShmSystemLock(pShmNode, F_UNLCK, ofst+UNIX_SHM_BASE, n);
    }else{
      rc = SQLITE_OK;
    }

    /* Undo the local locks */
    if( rc==SQLITE_OK ){
      p->exclMask &= ~mask;
      p->sharedMask &= ~mask;
    } 
  }else if( flags & SQLITE_SHM_SHARED ){
    u16 allShared = 0;  /* Union of locks held by connections other than "p" */

    /* Find out which shared locks are already held by sibling connections.
    ** If any sibling already holds an exclusive lock, go ahead and return
    ** SQLITE_BUSY.
    */
    for(pX=pShmNode->pFirst; pX; pX=pX->pNext){
      if( (pX->exclMask & mask)!=0 ){
        rc = SQLITE_BUSY;
        break;
      }
      allShared |= pX->sharedMask;
    }

    /* Get shared locks at the system level, if necessary */
    if( rc==SQLITE_OK ){
      if( (allShared & mask)==0 ){
        rc = unixShmSystemLock(pShmNode, F_RDLCK, ofst+UNIX_SHM_BASE, n);
      }else{
        rc = SQLITE_OK;
      }
    }

    /* Get the local shared locks */
    if( rc==SQLITE_OK ){
      p->sharedMask |= mask;
    }
  }else{
    /* Make sure no sibling connections hold locks that will block this
    ** lock.  If any do, return SQLITE_BUSY right away.
    */
    for(pX=pShmNode->pFirst; pX; pX=pX->pNext){
      if( (pX->exclMask & mask)!=0 || (pX->sharedMask & mask)!=0 ){
        rc = SQLITE_BUSY;
        break;
      }
    }
  
    /* Get the exclusive locks at the system level.  Then if successful
    ** also mark the local connection as being locked.
    */
    if( rc==SQLITE_OK ){
      rc = unixShmSystemLock(pShmNode, F_WRLCK, ofst+UNIX_SHM_BASE, n);
      if( rc==SQLITE_OK ){
        assert( (p->sharedMask & mask)==0 );
        p->exclMask |= mask;
      }
    }
  }
  sqlite3_mutex_leave(pShmNode->mutex);
  OSTRACE(("SHM-LOCK shmid-%d, pid-%d got %03x,%03x\n",
           p->id, getpid(), p->sharedMask, p->exclMask));
  return rc;
}

/*
** Implement a memory barrier or memory fence on shared memory.  
**
** All loads and stores begun before the barrier must complete before
** any load or store begun after the barrier.
*/
static void unixShmBarrier(
  sqlite3_file *fd                /* Database file holding the shared memory */
){
  UNUSED_PARAMETER(fd);
  unixEnterMutex();
  unixLeaveMutex();
}

/*
** Close a connection to shared-memory.  Delete the underlying 
** storage if deleteFlag is true.
**
** If there is no shared memory associated with the connection then this
** routine is a harmless no-op.
*/
static int unixShmUnmap(
  sqlite3_file *fd,               /* The underlying database file */
  int deleteFlag                  /* Delete shared-memory if true */
){
  unixShm *p;                     /* The connection to be closed */
  unixShmNode *pShmNode;          /* The underlying shared-memory file */
  unixShm **pp;                   /* For looping over sibling connections */
  unixFile *pDbFd;                /* The underlying database file */

  pDbFd = (unixFile*)fd;
  p = pDbFd->pShm;
  if( p==0 ) return SQLITE_OK;
  pShmNode = p->pShmNode;

  assert( pShmNode==pDbFd->pInode->pShmNode );
  assert( pShmNode->pInode==pDbFd->pInode );

  /* Remove connection p from the set of connections associated
  ** with pShmNode */
  sqlite3_mutex_enter(pShmNode->mutex);
  for(pp=&pShmNode->pFirst; (*pp)!=p; pp = &(*pp)->pNext){}
  *pp = p->pNext;

  /* Free the connection p */
  sqlite3_free(p);
  pDbFd->pShm = 0;
  sqlite3_mutex_leave(pShmNode->mutex);

  /* If pShmNode->nRef has reached 0, then close the underlying
  ** shared-memory file, too */
  unixEnterMutex();
  assert( pShmNode->nRef>0 );
  pShmNode->nRef--;
  if( pShmNode->nRef==0 ){
    if( deleteFlag && pShmNode->h>=0 ) osUnlink(pShmNode->zFilename);
    unixShmPurge(pDbFd);
  }
  unixLeaveMutex();

  return SQLITE_OK;
}


#else
# define unixShmMap     0
# define unixShmLock    0
# define unixShmBarrier 0
# define unixShmUnmap   0
#endif /* #ifndef SQLITE_OMIT_WAL */

/*
** Here ends the implementation of all sqlite3_file methods.
**
********************** End sqlite3_file Methods *******************************
******************************************************************************/

/*
** This division contains definitions of sqlite3_io_methods objects that
** implement various file locking strategies.  It also contains definitions
** of "finder" functions.  A finder-function is used to locate the appropriate
** sqlite3_io_methods object for a particular database file.  The pAppData
** field of the sqlite3_vfs VFS objects are initialized to be pointers to
** the correct finder-function for that VFS.
**
** Most finder functions return a pointer to a fixed sqlite3_io_methods
** object.  The only interesting finder-function is autolockIoFinder, which
** looks at the filesystem type and tries to guess the best locking
** strategy from that.
**
** For finder-funtion F, two objects are created:
**
**    (1) The real finder-function named "FImpt()".
**
**    (2) A constant pointer to this function named just "F".
**
**
** A pointer to the F pointer is used as the pAppData value for VFS
** objects.  We have to do this instead of letting pAppData point
** directly at the finder-function since C90 rules prevent a void*
** from be cast into a function pointer.
**
**
** Each instance of this macro generates two objects:
**
**   *  A constant sqlite3_io_methods object call METHOD that has locking
**      methods CLOSE, LOCK, UNLOCK, CKRESLOCK.
**
**   *  An I/O method finder function called FINDER that returns a pointer
**      to the METHOD object in the previous bullet.
*/
#define IOMETHODS(FINDER, METHOD, VERSION, CLOSE, LOCK, UNLOCK, CKLOCK)      \
static const sqlite3_io_methods METHOD = {                                   \
   VERSION,                    /* iVersion */                                \
   CLOSE,                      /* xClose */                                  \
   unixRead,                   /* xRead */                                   \
   unixWrite,                  /* xWrite */                                  \
   unixTruncate,               /* xTruncate */                               \
   unixSync,                   /* xSync */                                   \
   unixFileSize,               /* xFileSize */                               \
   LOCK,                       /* xLock */                                   \
   UNLOCK,                     /* xUnlock */                                 \
   CKLOCK,                     /* xCheckReservedLock */                      \
   unixFileControl,            /* xFileControl */                            \
   unixSectorSize,             /* xSectorSize */                             \
   unixDeviceCharacteristics,  /* xDeviceCapabilities */                     \
   unixShmMap,                 /* xShmMap */                                 \
   unixShmLock,                /* xShmLock */                                \
   unixShmBarrier,             /* xShmBarrier */                             \
   unixShmUnmap                /* xShmUnmap */                               \
};                                                                           \
static const sqlite3_io_methods *FINDER##Impl(const char *z, unixFile *p){   \
  UNUSED_PARAMETER(z); UNUSED_PARAMETER(p);                                  \
  return &METHOD;                                                            \
}                                                                            \
static const sqlite3_io_methods *(*const FINDER)(const char*,unixFile *p)    \
    = FINDER##Impl;

/*
** Here are all of the sqlite3_io_methods objects for each of the
** locking strategies.  Functions that return pointers to these methods
** are also created.
*/
IOMETHODS(
  posixIoFinder,            /* Finder function name */
  posixIoMethods,           /* sqlite3_io_methods object name */
  2,                        /* shared memory is enabled */
  unixClose,                /* xClose method */
  unixLock,                 /* xLock method */
  unixUnlock,               /* xUnlock method */
  unixCheckReservedLock     /* xCheckReservedLock method */
)
IOMETHODS(
  nolockIoFinder,           /* Finder function name */
  nolockIoMethods,          /* sqlite3_io_methods object name */
  1,                        /* shared memory is disabled */
  nolockClose,              /* xClose method */
  nolockLock,               /* xLock method */
  nolockUnlock,             /* xUnlock method */
  nolockCheckReservedLock   /* xCheckReservedLock method */
)
IOMETHODS(
  dotlockIoFinder,          /* Finder function name */
  dotlockIoMethods,         /* sqlite3_io_methods object name */
  1,                        /* shared memory is disabled */
  dotlockClose,             /* xClose method */
  dotlockLock,              /* xLock method */
  dotlockUnlock,            /* xUnlock method */
  dotlockCheckReservedLock  /* xCheckReservedLock method */
)

#if SQLITE_ENABLE_LOCKING_STYLE && !OS_VXWORKS
IOMETHODS(
  flockIoFinder,            /* Finder function name */
  flockIoMethods,           /* sqlite3_io_methods object name */
  1,                        /* shared memory is disabled */
  flockClose,               /* xClose method */
  flockLock,                /* xLock method */
  flockUnlock,              /* xUnlock method */
  flockCheckReservedLock    /* xCheckReservedLock method */
)
#endif

#if OS_VXWORKS
IOMETHODS(
  semIoFinder,              /* Finder function name */
  semIoMethods,             /* sqlite3_io_methods object name */
  1,                        /* shared memory is disabled */
  semClose,                 /* xClose method */
  semLock,                  /* xLock method */
  semUnlock,                /* xUnlock method */
  semCheckReservedLock      /* xCheckReservedLock method */
)
#endif

#if defined(__APPLE__) && SQLITE_ENABLE_LOCKING_STYLE
IOMETHODS(
  afpIoFinder,              /* Finder function name */
  afpIoMethods,             /* sqlite3_io_methods object name */
  1,                        /* shared memory is disabled */
  afpClose,                 /* xClose method */
  afpLock,                  /* xLock method */
  afpUnlock,                /* xUnlock method */
  afpCheckReservedLock      /* xCheckReservedLock method */
)
#endif

/*
** The proxy locking method is a "super-method" in the sense that it
** opens secondary file descriptors for the conch and lock files and
** it uses proxy, dot-file, AFP, and flock() locking methods on those
** secondary files.  For this reason, the division that implements
** proxy locking is located much further down in the file.  But we need
** to go ahead and define the sqlite3_io_methods and finder function
** for proxy locking here.  So we forward declare the I/O methods.
*/
#if defined(__APPLE__) && SQLITE_ENABLE_LOCKING_STYLE
static int proxyClose(sqlite3_file*);
static int proxyLock(sqlite3_file*, int);
static int proxyUnlock(sqlite3_file*, int);
static int proxyCheckReservedLock(sqlite3_file*, int*);
IOMETHODS(
  proxyIoFinder,            /* Finder function name */
  proxyIoMethods,           /* sqlite3_io_methods object name */
  1,                        /* shared memory is disabled */
  proxyClose,               /* xClose method */
  proxyLock,                /* xLock method */
  proxyUnlock,              /* xUnlock method */
  proxyCheckReservedLock    /* xCheckReservedLock method */
)
#endif

/* nfs lockd on OSX 10.3+ doesn't clear write locks when a read lock is set */
#if defined(__APPLE__) && SQLITE_ENABLE_LOCKING_STYLE
IOMETHODS(
  nfsIoFinder,               /* Finder function name */
  nfsIoMethods,              /* sqlite3_io_methods object name */
  1,                         /* shared memory is disabled */
  unixClose,                 /* xClose method */
  unixLock,                  /* xLock method */
  nfsUnlock,                 /* xUnlock method */
  unixCheckReservedLock      /* xCheckReservedLock method */
)
#endif

#if defined(__APPLE__) && SQLITE_ENABLE_LOCKING_STYLE
/* 
** This "finder" function attempts to determine the best locking strategy 
** for the database file "filePath".  It then returns the sqlite3_io_methods
** object that implements that strategy.
**
** This is for MacOSX only.
*/
static const sqlite3_io_methods *autolockIoFinderImpl(
  const char *filePath,    /* name of the database file */
  unixFile *pNew           /* open file object for the database file */
){
  static const struct Mapping {
    const char *zFilesystem;              /* Filesystem type name */
    const sqlite3_io_methods *pMethods;   /* Appropriate locking method */
  } aMap[] = {
    { "hfs",    &posixIoMethods },
    { "ufs",    &posixIoMethods },
    { "afpfs",  &afpIoMethods },
    { "smbfs",  &afpIoMethods },
    { "webdav", &nolockIoMethods },
    { 0, 0 }
  };
  int i;
  struct statfs fsInfo;
  struct flock lockInfo;

  if( !filePath ){
    /* If filePath==NULL that means we are dealing with a transient file
    ** that does not need to be locked. */
    return &nolockIoMethods;
  }
  if( statfs(filePath, &fsInfo) != -1 ){
    if( fsInfo.f_flags & MNT_RDONLY ){
      return &nolockIoMethods;
    }
    for(i=0; aMap[i].zFilesystem; i++){
      if( strcmp(fsInfo.f_fstypename, aMap[i].zFilesystem)==0 ){
        return aMap[i].pMethods;
      }
    }
  }

  /* Default case. Handles, amongst others, "nfs".
  ** Test byte-range lock using fcntl(). If the call succeeds, 
  ** assume that the file-system supports POSIX style locks. 
  */
  lockInfo.l_len = 1;
  lockInfo.l_start = 0;
  lockInfo.l_whence = SEEK_SET;
  lockInfo.l_type = F_RDLCK;
  if( osFcntl(pNew->h, F_GETLK, &lockInfo)!=-1 ) {
    if( strcmp(fsInfo.f_fstypename, "nfs")==0 ){
      return &nfsIoMethods;
    } else {
      return &posixIoMethods;
    }
  }else{
    return &dotlockIoMethods;
  }
}
static const sqlite3_io_methods 
  *(*const autolockIoFinder)(const char*,unixFile*) = autolockIoFinderImpl;

#endif /* defined(__APPLE__) && SQLITE_ENABLE_LOCKING_STYLE */

#if OS_VXWORKS && SQLITE_ENABLE_LOCKING_STYLE
/* 
** This "finder" function attempts to determine the best locking strategy 
** for the database file "filePath".  It then returns the sqlite3_io_methods
** object that implements that strategy.
**
** This is for VXWorks only.
*/
static const sqlite3_io_methods *autolockIoFinderImpl(
  const char *filePath,    /* name of the database file */
  unixFile *pNew           /* the open file object */
){
  struct flock lockInfo;

  if( !filePath ){
    /* If filePath==NULL that means we are dealing with a transient file
    ** that does not need to be locked. */
    return &nolockIoMethods;
  }

  /* Test if fcntl() is supported and use POSIX style locks.
  ** Otherwise fall back to the named semaphore method.
  */
  lockInfo.l_len = 1;
  lockInfo.l_start = 0;
  lockInfo.l_whence = SEEK_SET;
  lockInfo.l_type = F_RDLCK;
  if( osFcntl(pNew->h, F_GETLK, &lockInfo)!=-1 ) {
    return &posixIoMethods;
  }else{
    return &semIoMethods;
  }
}
static const sqlite3_io_methods 
  *(*const autolockIoFinder)(const char*,unixFile*) = autolockIoFinderImpl;

#endif /* OS_VXWORKS && SQLITE_ENABLE_LOCKING_STYLE */

/*
** An abstract type for a pointer to a IO method finder function:
*/
typedef const sqlite3_io_methods *(*finder_type)(const char*,unixFile*);


/****************************************************************************
**************************** sqlite3_vfs methods ****************************
**
** This division contains the implementation of methods on the
** sqlite3_vfs object.
*/

/*
** Initialize the contents of the unixFile structure pointed to by pId.
*/
static int fillInUnixFile(
  sqlite3_vfs *pVfs,      /* Pointer to vfs object */
  int h,                  /* Open file descriptor of file being opened */
  sqlite3_file *pId,      /* Write to the unixFile structure here */
  const char *zFilename,  /* Name of the file being opened */
  int ctrlFlags           /* Zero or more UNIXFILE_* values */
){
  const sqlite3_io_methods *pLockingStyle;
  unixFile *pNew = (unixFile *)pId;
  int rc = SQLITE_OK;

  assert( pNew->pInode==NULL );

  /* Usually the path zFilename should not be a relative pathname. The
  ** exception is when opening the proxy "conch" file in builds that
  ** include the special Apple locking styles.
  */
#if defined(__APPLE__) && SQLITE_ENABLE_LOCKING_STYLE
  assert( zFilename==0 || zFilename[0]=='/' 
    || pVfs->pAppData==(void*)&autolockIoFinder );
#else
  assert( zFilename==0 || zFilename[0]=='/' );
#endif

  /* No locking occurs in temporary files */
  assert( zFilename!=0 || (ctrlFlags & UNIXFILE_NOLOCK)!=0 );

  OSTRACE(("OPEN    %-3d %s\n", h, zFilename));
  pNew->h = h;
  pNew->pVfs = pVfs;
  pNew->zPath = zFilename;
  pNew->ctrlFlags = (u8)ctrlFlags;
  if( sqlite3_uri_boolean(((ctrlFlags & UNIXFILE_URI) ? zFilename : 0),
                           "psow", SQLITE_POWERSAFE_OVERWRITE) ){
    pNew->ctrlFlags |= UNIXFILE_PSOW;
  }
  if( memcmp(pVfs->zName,"unix-excl",10)==0 ){
    pNew->ctrlFlags |= UNIXFILE_EXCL;
  }

#if OS_VXWORKS
  pNew->pId = vxworksFindFileId(zFilename);
  if( pNew->pId==0 ){
    ctrlFlags |= UNIXFILE_NOLOCK;
    rc = SQLITE_NOMEM;
  }
#endif

  if( ctrlFlags & UNIXFILE_NOLOCK ){
    pLockingStyle = &nolockIoMethods;
  }else{
    pLockingStyle = (**(finder_type*)pVfs->pAppData)(zFilename, pNew);
#if SQLITE_ENABLE_LOCKING_STYLE
    /* Cache zFilename in the locking context (AFP and dotlock override) for
    ** proxyLock activation is possible (remote proxy is based on db name)
    ** zFilename remains valid until file is closed, to support */
    pNew->lockingContext = (void*)zFilename;
#endif
  }

  if( pLockingStyle == &posixIoMethods
#if defined(__APPLE__) && SQLITE_ENABLE_LOCKING_STYLE
    || pLockingStyle == &nfsIoMethods
#endif
  ){
    unixEnterMutex();
    rc = findInodeInfo(pNew, &pNew->pInode);
    if( rc!=SQLITE_OK ){
      /* If an error occured in findInodeInfo(), close the file descriptor
      ** immediately, before releasing the mutex. findInodeInfo() may fail
      ** in two scenarios:
      **
      **   (a) A call to fstat() failed.
      **   (b) A malloc failed.
      **
      ** Scenario (b) may only occur if the process is holding no other
      ** file descriptors open on the same file. If there were other file
      ** descriptors on this file, then no malloc would be required by
      ** findInodeInfo(). If this is the case, it is quite safe to close
      ** handle h - as it is guaranteed that no posix locks will be released
      ** by doing so.
      **
      ** If scenario (a) caused the error then things are not so safe. The
      ** implicit assumption here is that if fstat() fails, things are in
      ** such bad shape that dropping a lock or two doesn't matter much.
      */
      robust_close(pNew, h, __LINE__);
      h = -1;
    }
    unixLeaveMutex();
  }

#if SQLITE_ENABLE_LOCKING_STYLE && defined(__APPLE__)
  else if( pLockingStyle == &afpIoMethods ){
    /* AFP locking uses the file path so it needs to be included in
    ** the afpLockingContext.
    */
    afpLockingContext *pCtx;
    pNew->lockingContext = pCtx = sqlite3_malloc( sizeof(*pCtx) );
    if( pCtx==0 ){
      rc = SQLITE_NOMEM;
    }else{
      /* NB: zFilename exists and remains valid until the file is closed
      ** according to requirement F11141.  So we do not need to make a
      ** copy of the filename. */
      pCtx->dbPath = zFilename;
      pCtx->reserved = 0;
      srandomdev();
      unixEnterMutex();
      rc = findInodeInfo(pNew, &pNew->pInode);
      if( rc!=SQLITE_OK ){
        sqlite3_free(pNew->lockingContext);
        robust_close(pNew, h, __LINE__);
        h = -1;
      }
      unixLeaveMutex();        
    }
  }
#endif

  else if( pLockingStyle == &dotlockIoMethods ){
    /* Dotfile locking uses the file path so it needs to be included in
    ** the dotlockLockingContext 
    */
    char *zLockFile;
    int nFilename;
    assert( zFilename!=0 );
    nFilename = (int)strlen(zFilename) + 6;
    zLockFile = (char *)sqlite3_malloc(nFilename);
    if( zLockFile==0 ){
      rc = SQLITE_NOMEM;
    }else{
      sqlite3_snprintf(nFilename, zLockFile, "%s" DOTLOCK_SUFFIX, zFilename);
    }
    pNew->lockingContext = zLockFile;
  }

#if OS_VXWORKS
  else if( pLockingStyle == &semIoMethods ){
    /* Named semaphore locking uses the file path so it needs to be
    ** included in the semLockingContext
    */
    unixEnterMutex();
    rc = findInodeInfo(pNew, &pNew->pInode);
    if( (rc==SQLITE_OK) && (pNew->pInode->pSem==NULL) ){
      char *zSemName = pNew->pInode->aSemName;
      int n;
      sqlite3_snprintf(MAX_PATHNAME, zSemName, "/%s.sem",
                       pNew->pId->zCanonicalName);
      for( n=1; zSemName[n]; n++ )
        if( zSemName[n]=='/' ) zSemName[n] = '_';
      pNew->pInode->pSem = sem_open(zSemName, O_CREAT, 0666, 1);
      if( pNew->pInode->pSem == SEM_FAILED ){
        rc = SQLITE_NOMEM;
        pNew->pInode->aSemName[0] = '\0';
      }
    }
    unixLeaveMutex();
  }
#endif
  
  pNew->lastErrno = 0;
#if OS_VXWORKS
  if( rc!=SQLITE_OK ){
    if( h>=0 ) robust_close(pNew, h, __LINE__);
    h = -1;
    osUnlink(zFilename);
    isDelete = 0;
  }
  if( isDelete ) pNew->ctrlFlags |= UNIXFILE_DELETE;
#endif
  if( rc!=SQLITE_OK ){
    if( h>=0 ) robust_close(pNew, h, __LINE__);
  }else{
    pNew->pMethod = pLockingStyle;
    OpenCounter(+1);
  }
  return rc;
}

/*
** Return the name of a directory in which to put temporary files.
** If no suitable temporary file directory can be found, return NULL.
*/
static const char *unixTempFileDir(void){
  static const char *azDirs[] = {
     0,
     0,
     "/var/tmp",
     "/usr/tmp",
     "/tmp",
     0        /* List terminator */
  };
  unsigned int i;
  struct stat buf;
  const char *zDir = 0;

  azDirs[0] = sqlite3_temp_directory;
  if( !azDirs[1] ) azDirs[1] = getenv("TMPDIR");
  for(i=0; i<sizeof(azDirs)/sizeof(azDirs[0]); zDir=azDirs[i++]){
    if( zDir==0 ) continue;
    if( osStat(zDir, &buf) ) continue;
    if( !S_ISDIR(buf.st_mode) ) continue;
    if( osAccess(zDir, 07) ) continue;
    break;
  }
  return zDir;
}

/*
** Create a temporary file name in zBuf.  zBuf must be allocated
** by the calling process and must be big enough to hold at least
** pVfs->mxPathname bytes.
*/
static int unixGetTempname(int nBuf, char *zBuf){
  static const unsigned char zChars[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789";
  unsigned int i, j;
  const char *zDir;

  /* It's odd to simulate an io-error here, but really this is just
  ** using the io-error infrastructure to test that SQLite handles this
  ** function failing. 
  */
  SimulateIOError( return SQLITE_IOERR );

  zDir = unixTempFileDir();
  if( zDir==0 ) zDir = ".";

  /* Check that the output buffer is large enough for the temporary file 
  ** name. If it is not, return SQLITE_ERROR.
  */
  if( (strlen(zDir) + strlen(SQLITE_TEMP_FILE_PREFIX) + 18) >= (size_t)nBuf ){
    return SQLITE_ERROR;
  }

  do{
    sqlite3_snprintf(nBuf-18, zBuf, "%s/"SQLITE_TEMP_FILE_PREFIX, zDir);
    j = (int)strlen(zBuf);
    sqlite3_randomness(15, &zBuf[j]);
    for(i=0; i<15; i++, j++){
      zBuf[j] = (char)zChars[ ((unsigned char)zBuf[j])%(sizeof(zChars)-1) ];
    }
    zBuf[j] = 0;
    zBuf[j+1] = 0;
  }while( osAccess(zBuf,0)==0 );
  return SQLITE_OK;
}

#if SQLITE_ENABLE_LOCKING_STYLE && defined(__APPLE__)
/*
** Routine to transform a unixFile into a proxy-locking unixFile.
** Implementation in the proxy-lock division, but used by unixOpen()
** if SQLITE_PREFER_PROXY_LOCKING is defined.
*/
static int proxyTransformUnixFile(unixFile*, const char*);
#endif

/*
** Search for an unused file descriptor that was opened on the database 
** file (not a journal or master-journal file) identified by pathname
** zPath with SQLITE_OPEN_XXX flags matching those passed as the second
** argument to this function.
**
** Such a file descriptor may exist if a database connection was closed
** but the associated file descriptor could not be closed because some
** other file descriptor open on the same file is holding a file-lock.
** Refer to comments in the unixClose() function and the lengthy comment
** describing "Posix Advisory Locking" at the start of this file for 
** further details. Also, ticket #4018.
**
** If a suitable file descriptor is found, then it is returned. If no
** such file descriptor is located, -1 is returned.
*/
static UnixUnusedFd *findReusableFd(const char *zPath, int flags){
  UnixUnusedFd *pUnused = 0;

  /* Do not search for an unused file descriptor on vxworks. Not because
  ** vxworks would not benefit from the change (it might, we're not sure),
  ** but because no way to test it is currently available. It is better 
  ** not to risk breaking vxworks support for the sake of such an obscure 
  ** feature.  */
#if !OS_VXWORKS
  struct stat sStat;                   /* Results of stat() call */

  /* A stat() call may fail for various reasons. If this happens, it is
  ** almost certain that an open() call on the same path will also fail.
  ** For this reason, if an error occurs in the stat() call here, it is
  ** ignored and -1 is returned. The caller will try to open a new file
  ** descriptor on the same path, fail, and return an error to SQLite.
  **
  ** Even if a subsequent open() call does succeed, the consequences of
  ** not searching for a resusable file descriptor are not dire.  */
  if( 0==osStat(zPath, &sStat) ){
    unixInodeInfo *pInode;

    unixEnterMutex();
    pInode = inodeList;
    while( pInode && (pInode->fileId.dev!=sStat.st_dev
                     || pInode->fileId.ino!=sStat.st_ino) ){
       pInode = pInode->pNext;
    }
    if( pInode ){
      UnixUnusedFd **pp;
      for(pp=&pInode->pUnused; *pp && (*pp)->flags!=flags; pp=&((*pp)->pNext));
      pUnused = *pp;
      if( pUnused ){
        *pp = pUnused->pNext;
      }
    }
    unixLeaveMutex();
  }
#endif    /* if !OS_VXWORKS */
  return pUnused;
}

/*
** This function is called by unixOpen() to determine the unix permissions
** to create new files with. If no error occurs, then SQLITE_OK is returned
** and a value suitable for passing as the third argument to open(2) is
** written to *pMode. If an IO error occurs, an SQLite error code is 
** returned and the value of *pMode is not modified.
**
** In most cases cases, this routine sets *pMode to 0, which will become
** an indication to robust_open() to create the file using
** SQLITE_DEFAULT_FILE_PERMISSIONS adjusted by the umask.
** But if the file being opened is a WAL or regular journal file, then 
** this function queries the file-system for the permissions on the 
** corresponding database file and sets *pMode to this value. Whenever 
** possible, WAL and journal files are created using the same permissions 
** as the associated database file.
**
** If the SQLITE_ENABLE_8_3_NAMES option is enabled, then the
** original filename is unavailable.  But 8_3_NAMES is only used for
** FAT filesystems and permissions do not matter there, so just use
** the default permissions.
*/
static int findCreateFileMode(
  const char *zPath,              /* Path of file (possibly) being created */
  int flags,                      /* Flags passed as 4th argument to xOpen() */
  mode_t *pMode,                  /* OUT: Permissions to open file with */
  uid_t *pUid,                    /* OUT: uid to set on the file */
  gid_t *pGid                     /* OUT: gid to set on the file */
){
  int rc = SQLITE_OK;             /* Return Code */
  *pMode = 0;
  *pUid = 0;
  *pGid = 0;
  if( flags & (SQLITE_OPEN_WAL|SQLITE_OPEN_MAIN_JOURNAL) ){
    char zDb[MAX_PATHNAME+1];     /* Database file path */
    int nDb;                      /* Number of valid bytes in zDb */
    struct stat sStat;            /* Output of stat() on database file */

    /* zPath is a path to a WAL or journal file. The following block derives
    ** the path to the associated database file from zPath. This block handles
    ** the following naming conventions:
    **
    **   "<path to db>-journal"
    **   "<path to db>-wal"
    **   "<path to db>-journalNN"
    **   "<path to db>-walNN"
    **
    ** where NN is a decimal number. The NN naming schemes are 
    ** used by the test_multiplex.c module.
    */
    nDb = sqlite3Strlen30(zPath) - 1; 
#ifdef SQLITE_ENABLE_8_3_NAMES
    while( nDb>0 && sqlite3Isalnum(zPath[nDb]) ) nDb--;
    if( nDb==0 || zPath[nDb]!='-' ) return SQLITE_OK;
#else
    while( zPath[nDb]!='-' ){
      assert( nDb>0 );
      assert( zPath[nDb]!='\n' );
      nDb--;
    }
#endif
    memcpy(zDb, zPath, nDb);
    zDb[nDb] = '\0';

    if( 0==osStat(zDb, &sStat) ){
      *pMode = sStat.st_mode & 0777;
      *pUid = sStat.st_uid;
      *pGid = sStat.st_gid;
    }else{
      rc = SQLITE_IOERR_FSTAT;
    }
  }else if( flags & SQLITE_OPEN_DELETEONCLOSE ){
    *pMode = 0600;
  }
  return rc;
}

/*
** Open the file zPath.
** 
** Previously, the SQLite OS layer used three functions in place of this
** one:
**
**     sqlite3OsOpenReadWrite();
**     sqlite3OsOpenReadOnly();
**     sqlite3OsOpenExclusive();
**
** These calls correspond to the following combinations of flags:
**
**     ReadWrite() ->     (READWRITE | CREATE)
**     ReadOnly()  ->     (READONLY) 
**     OpenExclusive() -> (READWRITE | CREATE | EXCLUSIVE)
**
** The old OpenExclusive() accepted a boolean argument - "delFlag". If
** true, the file was configured to be automatically deleted when the
** file handle closed. To achieve the same effect using this new 
** interface, add the DELETEONCLOSE flag to those specified above for 
** OpenExclusive().
*/
static int unixOpen(
  sqlite3_vfs *pVfs,           /* The VFS for which this is the xOpen method */
  const char *zPath,           /* Pathname of file to be opened */
  sqlite3_file *pFile,         /* The file descriptor to be filled in */
  int flags,                   /* Input flags to control the opening */
  int *pOutFlags               /* Output flags returned to SQLite core */
){
  unixFile *p = (unixFile *)pFile;
  int fd = -1;                   /* File descriptor returned by open() */
  int openFlags = 0;             /* Flags to pass to open() */
  int eType = flags&0xFFFFFF00;  /* Type of file to open */
  int noLock;                    /* True to omit locking primitives */
  int rc = SQLITE_OK;            /* Function Return Code */
  int ctrlFlags = 0;             /* UNIXFILE_* flags */

  int isExclusive  = (flags & SQLITE_OPEN_EXCLUSIVE);
  int isDelete     = (flags & SQLITE_OPEN_DELETEONCLOSE);
  int isCreate     = (flags & SQLITE_OPEN_CREATE);
  int isReadonly   = (flags & SQLITE_OPEN_READONLY);
  int isReadWrite  = (flags & SQLITE_OPEN_READWRITE);
#if SQLITE_ENABLE_LOCKING_STYLE
  int isAutoProxy  = (flags & SQLITE_OPEN_AUTOPROXY);
#endif
#if defined(__APPLE__) || SQLITE_ENABLE_LOCKING_STYLE
  struct statfs fsInfo;
#endif

  /* If creating a master or main-file journal, this function will open
  ** a file-descriptor on the directory too. The first time unixSync()
  ** is called the directory file descriptor will be fsync()ed and close()d.
  */
  int syncDir = (isCreate && (
        eType==SQLITE_OPEN_MASTER_JOURNAL 
     || eType==SQLITE_OPEN_MAIN_JOURNAL 
     || eType==SQLITE_OPEN_WAL
  ));

  /* If argument zPath is a NULL pointer, this function is required to open
  ** a temporary file. Use this buffer to store the file name in.
  */
  char zTmpname[MAX_PATHNAME+2];
  const char *zName = zPath;

  /* Check the following statements are true: 
  **
  **   (a) Exactly one of the READWRITE and READONLY flags must be set, and 
  **   (b) if CREATE is set, then READWRITE must also be set, and
  **   (c) if EXCLUSIVE is set, then CREATE must also be set.
  **   (d) if DELETEONCLOSE is set, then CREATE must also be set.
  */
  assert((isReadonly==0 || isReadWrite==0) && (isReadWrite || isReadonly));
  assert(isCreate==0 || isReadWrite);
  assert(isExclusive==0 || isCreate);
  assert(isDelete==0 || isCreate);

  /* The main DB, main journal, WAL file and master journal are never 
  ** automatically deleted. Nor are they ever temporary files.  */
  assert( (!isDelete && zName) || eType!=SQLITE_OPEN_MAIN_DB );
  assert( (!isDelete && zName) || eType!=SQLITE_OPEN_MAIN_JOURNAL );
  assert( (!isDelete && zName) || eType!=SQLITE_OPEN_MASTER_JOURNAL );
  assert( (!isDelete && zName) || eType!=SQLITE_OPEN_WAL );

  /* Assert that the upper layer has set one of the "file-type" flags. */
  assert( eType==SQLITE_OPEN_MAIN_DB      || eType==SQLITE_OPEN_TEMP_DB 
       || eType==SQLITE_OPEN_MAIN_JOURNAL || eType==SQLITE_OPEN_TEMP_JOURNAL 
       || eType==SQLITE_OPEN_SUBJOURNAL   || eType==SQLITE_OPEN_MASTER_JOURNAL 
       || eType==SQLITE_OPEN_TRANSIENT_DB || eType==SQLITE_OPEN_WAL
  );

  memset(p, 0, sizeof(unixFile));

  if( eType==SQLITE_OPEN_MAIN_DB ){
    UnixUnusedFd *pUnused;
    pUnused = findReusableFd(zName, flags);
    if( pUnused ){
      fd = pUnused->fd;
    }else{
      pUnused = sqlite3_malloc(sizeof(*pUnused));
      if( !pUnused ){
        return SQLITE_NOMEM;
      }
    }
    p->pUnused = pUnused;

    /* Database filenames are double-zero terminated if they are not
    ** URIs with parameters.  Hence, they can always be passed into
    ** sqlite3_uri_parameter(). */
    assert( (flags & SQLITE_OPEN_URI) || zName[strlen(zName)+1]==0 );

  }else if( !zName ){
    /* If zName is NULL, the upper layer is requesting a temp file. */
    assert(isDelete && !syncDir);
    rc = unixGetTempname(MAX_PATHNAME+2, zTmpname);
    if( rc!=SQLITE_OK ){
      return rc;
    }
    zName = zTmpname;

    /* Generated temporary filenames are always double-zero terminated
    ** for use by sqlite3_uri_parameter(). */
    assert( zName[strlen(zName)+1]==0 );
  }

  /* Determine the value of the flags parameter passed to POSIX function
  ** open(). These must be calculated even if open() is not called, as
  ** they may be stored as part of the file handle and used by the 
  ** 'conch file' locking functions later on.  */
  if( isReadonly )  openFlags |= O_RDONLY;
  if( isReadWrite ) openFlags |= O_RDWR;
  if( isCreate )    openFlags |= O_CREAT;
  if( isExclusive ) openFlags |= (O_EXCL|O_NOFOLLOW);
  openFlags |= (O_LARGEFILE|O_BINARY);

  if( fd<0 ){
    mode_t openMode;              /* Permissions to create file with */
    uid_t uid;                    /* Userid for the file */
    gid_t gid;                    /* Groupid for the file */
    rc = findCreateFileMode(zName, flags, &openMode, &uid, &gid);
    if( rc!=SQLITE_OK ){
      assert( !p->pUnused );
      assert( eType==SQLITE_OPEN_WAL || eType==SQLITE_OPEN_MAIN_JOURNAL );
      return rc;
    }
    fd = robust_open(zName, openFlags, openMode);
    OSTRACE(("OPENX   %-3d %s 0%o\n", fd, zName, openFlags));
    if( fd<0 && errno!=EISDIR && isReadWrite && !isExclusive ){
      /* Failed to open the file for read/write access. Try read-only. */
      flags &= ~(SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE);
      openFlags &= ~(O_RDWR|O_CREAT);
      flags |= SQLITE_OPEN_READONLY;
      openFlags |= O_RDONLY;
      isReadonly = 1;
      fd = robust_open(zName, openFlags, openMode);
    }
    if( fd<0 ){
      rc = unixLogError(SQLITE_CANTOPEN_BKPT, "open", zName);
      goto open_finished;
    }

    /* If this process is running as root and if creating a new rollback
    ** journal or WAL file, set the ownership of the journal or WAL to be
    ** the same as the original database.
    */
    if( flags & (SQLITE_OPEN_WAL|SQLITE_OPEN_MAIN_JOURNAL) ){
      osFchown(fd, uid, gid);
    }
  }
  assert( fd>=0 );
  if( pOutFlags ){
    *pOutFlags = flags;
  }

  if( p->pUnused ){
    p->pUnused->fd = fd;
    p->pUnused->flags = flags;
  }

  if( isDelete ){
#if OS_VXWORKS
    zPath = zName;
#else
    osUnlink(zName);
#endif
  }
#if SQLITE_ENABLE_LOCKING_STYLE
  else{
    p->openFlags = openFlags;
  }
#endif

  noLock = eType!=SQLITE_OPEN_MAIN_DB;

  
#if defined(__APPLE__) || SQLITE_ENABLE_LOCKING_STYLE
  if( fstatfs(fd, &fsInfo) == -1 ){
    ((unixFile*)pFile)->lastErrno = errno;
    robust_close(p, fd, __LINE__);
    return SQLITE_IOERR_ACCESS;
  }
  if (0 == strncmp("msdos", fsInfo.f_fstypename, 5)) {
    ((unixFile*)pFile)->fsFlags |= SQLITE_FSFLAGS_IS_MSDOS;
  }
#endif

  /* Set up appropriate ctrlFlags */
  if( isDelete )                ctrlFlags |= UNIXFILE_DELETE;
  if( isReadonly )              ctrlFlags |= UNIXFILE_RDONLY;
  if( noLock )                  ctrlFlags |= UNIXFILE_NOLOCK;
  if( syncDir )                 ctrlFlags |= UNIXFILE_DIRSYNC;
  if( flags & SQLITE_OPEN_URI ) ctrlFlags |= UNIXFILE_URI;

#if SQLITE_ENABLE_LOCKING_STYLE
#if SQLITE_PREFER_PROXY_LOCKING
  isAutoProxy = 1;
#endif
  if( isAutoProxy && (zPath!=NULL) && (!noLock) && pVfs->xOpen ){
    char *envforce = getenv("SQLITE_FORCE_PROXY_LOCKING");
    int useProxy = 0;

    /* SQLITE_FORCE_PROXY_LOCKING==1 means force always use proxy, 0 means 
    ** never use proxy, NULL means use proxy for non-local files only.  */
    if( envforce!=NULL ){
      useProxy = atoi(envforce)>0;
    }else{
      if( statfs(zPath, &fsInfo) == -1 ){
        /* In theory, the close(fd) call is sub-optimal. If the file opened
        ** with fd is a database file, and there are other connections open
        ** on that file that are currently holding advisory locks on it,
        ** then the call to close() will cancel those locks. In practice,
        ** we're assuming that statfs() doesn't fail very often. At least
        ** not while other file descriptors opened by the same process on
        ** the same file are working.  */
        p->lastErrno = errno;
        robust_close(p, fd, __LINE__);
        rc = SQLITE_IOERR_ACCESS;
        goto open_finished;
      }
      useProxy = !(fsInfo.f_flags&MNT_LOCAL);
    }
    if( useProxy ){
      rc = fillInUnixFile(pVfs, fd, pFile, zPath, ctrlFlags);
      if( rc==SQLITE_OK ){
        rc = proxyTransformUnixFile((unixFile*)pFile, ":auto:");
        if( rc!=SQLITE_OK ){
          /* Use unixClose to clean up the resources added in fillInUnixFile 
          ** and clear all the structure's references.  Specifically, 
          ** pFile->pMethods will be NULL so sqlite3OsClose will be a no-op 
          */
          unixClose(pFile);
          return rc;
        }
      }
      goto open_finished;
    }
  }
#endif
  
  rc = fillInUnixFile(pVfs, fd, pFile, zPath, ctrlFlags);

open_finished:
  if( rc!=SQLITE_OK ){
    sqlite3_free(p->pUnused);
  }
  return rc;
}


/*
** Delete the file at zPath. If the dirSync argument is true, fsync()
** the directory after deleting the file.
*/
static int unixDelete(
  sqlite3_vfs *NotUsed,     /* VFS containing this as the xDelete method */
  const char *zPath,        /* Name of file to be deleted */
  int dirSync               /* If true, fsync() directory after deleting file */
){
  int rc = SQLITE_OK;
  UNUSED_PARAMETER(NotUsed);
  SimulateIOError(return SQLITE_IOERR_DELETE);
  if( osUnlink(zPath)==(-1) && errno!=ENOENT ){
    return unixLogError(SQLITE_IOERR_DELETE, "unlink", zPath);
  }
#ifndef SQLITE_DISABLE_DIRSYNC
  if( (dirSync & 1)!=0 ){
    int fd;
    rc = osOpenDirectory(zPath, &fd);
    if( rc==SQLITE_OK ){
#if OS_VXWORKS
      if( fsync(fd)==-1 )
#else
      if( fsync(fd) )
#endif
      {
        rc = unixLogError(SQLITE_IOERR_DIR_FSYNC, "fsync", zPath);
      }
      robust_close(0, fd, __LINE__);
    }else if( rc==SQLITE_CANTOPEN ){
      rc = SQLITE_OK;
    }
  }
#endif
  return rc;
}

/*
** Test the existance of or access permissions of file zPath. The
** test performed depends on the value of flags:
**
**     SQLITE_ACCESS_EXISTS: Return 1 if the file exists
**     SQLITE_ACCESS_READWRITE: Return 1 if the file is read and writable.
**     SQLITE_ACCESS_READONLY: Return 1 if the file is readable.
**
** Otherwise return 0.
*/
static int unixAccess(
  sqlite3_vfs *NotUsed,   /* The VFS containing this xAccess method */
  const char *zPath,      /* Path of the file to examine */
  int flags,              /* What do we want to learn about the zPath file? */
  int *pResOut            /* Write result boolean here */
){
  int amode = 0;
  UNUSED_PARAMETER(NotUsed);
  SimulateIOError( return SQLITE_IOERR_ACCESS; );
  switch( flags ){
    case SQLITE_ACCESS_EXISTS:
      amode = F_OK;
      break;
    case SQLITE_ACCESS_READWRITE:
      amode = W_OK|R_OK;
      break;
    case SQLITE_ACCESS_READ:
      amode = R_OK;
      break;

    default:
      assert(!"Invalid flags argument");
  }
  *pResOut = (osAccess(zPath, amode)==0);
  if( flags==SQLITE_ACCESS_EXISTS && *pResOut ){
    struct stat buf;
    if( 0==osStat(zPath, &buf) && buf.st_size==0 ){
      *pResOut = 0;
    }
  }
  return SQLITE_OK;
}


/*
** Turn a relative pathname into a full pathname. The relative path
** is stored as a nul-terminated string in the buffer pointed to by
** zPath. 
**
** zOut points to a buffer of at least sqlite3_vfs.mxPathname bytes 
** (in this case, MAX_PATHNAME bytes). The full-path is written to
** this buffer before returning.
*/
static int unixFullPathname(
  sqlite3_vfs *pVfs,            /* Pointer to vfs object */
  const char *zPath,            /* Possibly relative input path */
  int nOut,                     /* Size of output buffer in bytes */
  char *zOut                    /* Output buffer */
){

  /* It's odd to simulate an io-error here, but really this is just
  ** using the io-error infrastructure to test that SQLite handles this
  ** function failing. This function could fail if, for example, the
  ** current working directory has been unlinked.
  */
  SimulateIOError( return SQLITE_ERROR );

  assert( pVfs->mxPathname==MAX_PATHNAME );
  UNUSED_PARAMETER(pVfs);

  zOut[nOut-1] = '\0';
  if( zPath[0]=='/' ){
    sqlite3_snprintf(nOut, zOut, "%s", zPath);
  }else{
    int nCwd;
    if( osGetcwd(zOut, nOut-1)==0 ){
      return unixLogError(SQLITE_CANTOPEN_BKPT, "getcwd", zPath);
    }
    nCwd = (int)strlen(zOut);
    sqlite3_snprintf(nOut-nCwd, &zOut[nCwd], "/%s", zPath);
  }
  return SQLITE_OK;
}


#ifndef SQLITE_OMIT_LOAD_EXTENSION
/*
** Interfaces for opening a shared library, finding entry points
** within the shared library, and closing the shared library.
*/
#include <dlfcn.h>
static void *unixDlOpen(sqlite3_vfs *NotUsed, const char *zFilename){
  UNUSED_PARAMETER(NotUsed);
  return dlopen(zFilename, RTLD_NOW | RTLD_GLOBAL);
}

/*
** SQLite calls this function immediately after a call to unixDlSym() or
** unixDlOpen() fails (returns a null pointer). If a more detailed error
** message is available, it is written to zBufOut. If no error message
** is available, zBufOut is left unmodified and SQLite uses a default
** error message.
*/
static void unixDlError(sqlite3_vfs *NotUsed, int nBuf, char *zBufOut){
  const char *zErr;
  UNUSED_PARAMETER(NotUsed);
  unixEnterMutex();
  zErr = dlerror();
  if( zErr ){
    sqlite3_snprintf(nBuf, zBufOut, "%s", zErr);
  }
  unixLeaveMutex();
}
static void (*unixDlSym(sqlite3_vfs *NotUsed, void *p, const char*zSym))(void){
  /* 
  ** GCC with -pedantic-errors says that C90 does not allow a void* to be
  ** cast into a pointer to a function.  And yet the library dlsym() routine
  ** returns a void* which is really a pointer to a function.  So how do we
  ** use dlsym() with -pedantic-errors?
  **
  ** Variable x below is defined to be a pointer to a function taking
  ** parameters void* and const char* and returning a pointer to a function.
  ** We initialize x by assigning it a pointer to the dlsym() function.
  ** (That assignment requires a cast.)  Then we call the function that
  ** x points to.  
  **
  ** This work-around is unlikely to work correctly on any system where
  ** you really cannot cast a function pointer into void*.  But then, on the
  ** other hand, dlsym() will not work on such a system either, so we have
  ** not really lost anything.
  */
  void (*(*x)(void*,const char*))(void);
  UNUSED_PARAMETER(NotUsed);
  x = (void(*(*)(void*,const char*))(void))dlsym;
  return (*x)(p, zSym);
}
static void unixDlClose(sqlite3_vfs *NotUsed, void *pHandle){
  UNUSED_PARAMETER(NotUsed);
  dlclose(pHandle);
}
#else /* if SQLITE_OMIT_LOAD_EXTENSION is defined: */
  #define unixDlOpen  0
  #define unixDlError 0
  #define unixDlSym   0
  #define unixDlClose 0
#endif

/*
** Write nBuf bytes of random data to the supplied buffer zBuf.
*/
static int unixRandomness(sqlite3_vfs *NotUsed, int nBuf, char *zBuf){
  UNUSED_PARAMETER(NotUsed);
  assert((size_t)nBuf>=(sizeof(time_t)+sizeof(int)));

  /* We have to initialize zBuf to prevent valgrind from reporting
  ** errors.  The reports issued by valgrind are incorrect - we would
  ** prefer that the randomness be increased by making use of the
  ** uninitialized space in zBuf - but valgrind errors tend to worry
  ** some users.  Rather than argue, it seems easier just to initialize
  ** the whole array and silence valgrind, even if that means less randomness
  ** in the random seed.
  **
  ** When testing, initializing zBuf[] to zero is all we do.  That means
  ** that we always use the same random number sequence.  This makes the
  ** tests repeatable.
  */
  memset(zBuf, 0, nBuf);
#if !defined(SQLITE_TEST)
  {
    int pid, fd, got;
    fd = robust_open("/dev/urandom", O_RDONLY, 0);
    if( fd<0 ){
      time_t t;
      time(&t);
      memcpy(zBuf, &t, sizeof(t));
      pid = getpid();
      memcpy(&zBuf[sizeof(t)], &pid, sizeof(pid));
      assert( sizeof(t)+sizeof(pid)<=(size_t)nBuf );
      nBuf = sizeof(t) + sizeof(pid);
    }else{
      do{ got = osRead(fd, zBuf, nBuf); }while( got<0 && errno==EINTR );
      robust_close(0, fd, __LINE__);
    }
  }
#endif
  return nBuf;
}


/*
** Sleep for a little while.  Return the amount of time slept.
** The argument is the number of microseconds we want to sleep.
** The return value is the number of microseconds of sleep actually
** requested from the underlying operating system, a number which
** might be greater than or equal to the argument, but not less
** than the argument.
*/
static int unixSleep(sqlite3_vfs *NotUsed, int microseconds){
#if OS_VXWORKS
  struct timespec sp;

  sp.tv_sec = microseconds / 1000000;
  sp.tv_nsec = (microseconds % 1000000) * 1000;
  nanosleep(&sp, NULL);
  UNUSED_PARAMETER(NotUsed);
  return microseconds;
#elif defined(HAVE_USLEEP) && HAVE_USLEEP
  usleep(microseconds);
  UNUSED_PARAMETER(NotUsed);
  return microseconds;
#else
  int seconds = (microseconds+999999)/1000000;
  sleep(seconds);
  UNUSED_PARAMETER(NotUsed);
  return seconds*1000000;
#endif
}

/*
** The following variable, if set to a non-zero value, is interpreted as
** the number of seconds since 1970 and is used to set the result of
** sqlite3OsCurrentTime() during testing.
*/
#ifdef SQLITE_TEST
int sqlite3_current_time = 0;  /* Fake system time in seconds since 1970. */
#endif

/*
** Find the current time (in Universal Coordinated Time).  Write into *piNow
** the current time and date as a Julian Day number times 86_400_000.  In
** other words, write into *piNow the number of milliseconds since the Julian
** epoch of noon in Greenwich on November 24, 4714 B.C according to the
** proleptic Gregorian calendar.
**
** On success, return SQLITE_OK.  Return SQLITE_ERROR if the time and date 
** cannot be found.
*/
static int unixCurrentTimeInt64(sqlite3_vfs *NotUsed, sqlite3_int64 *piNow){
  static const sqlite3_int64 unixEpoch = 24405875*(sqlite3_int64)8640000;
  int rc = SQLITE_OK;
#if defined(NO_GETTOD)
  time_t t;
  time(&t);
  *piNow = ((sqlite3_int64)t)*1000 + unixEpoch;
#elif OS_VXWORKS
  struct timespec sNow;
  clock_gettime(CLOCK_REALTIME, &sNow);
  *piNow = unixEpoch + 1000*(sqlite3_int64)sNow.tv_sec + sNow.tv_nsec/1000000;
#else
  struct timeval sNow;
  if( gettimeofday(&sNow, 0)==0 ){
    *piNow = unixEpoch + 1000*(sqlite3_int64)sNow.tv_sec + sNow.tv_usec/1000;
  }else{
    rc = SQLITE_ERROR;
  }
#endif

#ifdef SQLITE_TEST
  if( sqlite3_current_time ){
    *piNow = 1000*(sqlite3_int64)sqlite3_current_time + unixEpoch;
  }
#endif
  UNUSED_PARAMETER(NotUsed);
  return rc;
}

/*
** Find the current time (in Universal Coordinated Time).  Write the
** current time and date as a Julian Day number into *prNow and
** return 0.  Return 1 if the time and date cannot be found.
*/
static int unixCurrentTime(sqlite3_vfs *NotUsed, double *prNow){
  sqlite3_int64 i = 0;
  int rc;
  UNUSED_PARAMETER(NotUsed);
  rc = unixCurrentTimeInt64(0, &i);
  *prNow = i/86400000.0;
  return rc;
}

/*
** We added the xGetLastError() method with the intention of providing
** better low-level error messages when operating-system problems come up
** during SQLite operation.  But so far, none of that has been implemented
** in the core.  So this routine is never called.  For now, it is merely
** a place-holder.
*/
static int unixGetLastError(sqlite3_vfs *NotUsed, int NotUsed2, char *NotUsed3){
  UNUSED_PARAMETER(NotUsed);
  UNUSED_PARAMETER(NotUsed2);
  UNUSED_PARAMETER(NotUsed3);
  return 0;
}


/*
************************ End of sqlite3_vfs methods ***************************
******************************************************************************/

/******************************************************************************
************************** Begin Proxy Locking ********************************
**
** Proxy locking is a "uber-locking-method" in this sense:  It uses the
** other locking methods on secondary lock files.  Proxy locking is a
** meta-layer over top of the primitive locking implemented above.  For
** this reason, the division that implements of proxy locking is deferred
** until late in the file (here) after all of the other I/O methods have
** been defined - so that the primitive locking methods are available
** as services to help with the implementation of proxy locking.
**
****
**
** The default locking schemes in SQLite use byte-range locks on the
** database file to coordinate safe, concurrent access by multiple readers
** and writers [http://sqlite.org/lockingv3.html].  The five file locking
** states (UNLOCKED, PENDING, SHARED, RESERVED, EXCLUSIVE) are implemented
** as POSIX read & write locks over fixed set of locations (via fsctl),
** on AFP and SMB only exclusive byte-range locks are available via fsctl
** with _IOWR('z', 23, struct ByteRangeLockPB2) to track the same 5 states.
** To simulate a F_RDLCK on the shared range, on AFP a randomly selected
** address in the shared range is taken for a SHARED lock, the entire
** shared range is taken for an EXCLUSIVE lock):
**
**      PENDING_BYTE        0x40000000
**      RESERVED_BYTE       0x40000001
**      SHARED_RANGE        0x40000002 -> 0x40000200
**
** This works well on the local file system, but shows a nearly 100x
** slowdown in read performance on AFP because the AFP client disables
** the read cache when byte-range locks are present.  Enabling the read
** cache exposes a cache coherency problem that is present on all OS X
** supported network file systems.  NFS and AFP both observe the
** close-to-open semantics for ensuring cache coherency
** [http://nfs.sourceforge.net/#faq_a8], which does not effectively
** address the requirements for concurrent database access by multiple
** readers and writers
** [http://www.nabble.com/SQLite-on-NFS-cache-coherency-td15655701.html].
**
** To address the performance and cache coherency issues, proxy file locking
** changes the way database access is controlled by limiting access to a
** single host at a time and moving file locks off of the database file
** and onto a proxy file on the local file system.  
**
**
** Using proxy locks
** -----------------
**
** C APIs
**
**  sqlite3_file_control(db, dbname, SQLITE_SET_LOCKPROXYFILE,
**                       <proxy_path> | ":auto:");
**  sqlite3_file_control(db, dbname, SQLITE_GET_LOCKPROXYFILE, &<proxy_path>);
**
**
** SQL pragmas
**
**  PRAGMA [database.]lock_proxy_file=<proxy_path> | :auto:
**  PRAGMA [database.]lock_proxy_file
**
** Specifying ":auto:" means that if there is a conch file with a matching
** host ID in it, the proxy path in the conch file will be used, otherwise
** a proxy path based on the user's temp dir
** (via confstr(_CS_DARWIN_USER_TEMP_DIR,...)) will be used and the
** actual proxy file name is generated from the name and path of the
** database file.  For example:
**
**       For database path "/Users/me/foo.db" 
**       The lock path will be "<tmpdir>/sqliteplocks/_Users_me_foo.db:auto:")
**
** Once a lock proxy is configured for a database connection, it can not
** be removed, however it may be switched to a different proxy path via
** the above APIs (assuming the conch file is not being held by another
** connection or process). 
**
**
** How proxy locking works
** -----------------------
**
** Proxy file locking relies primarily on two new supporting files: 
**
**   *  conch file to limit access to the database file to a single host
**      at a time
**
**   *  proxy file to act as a proxy for the advisory locks normally
**      taken on the database
**
** The conch file - to use a proxy file, sqlite must first "hold the conch"
** by taking an sqlite-style shared lock on the conch file, reading the
** contents and comparing the host's unique host ID (see below) and lock
** proxy path against the values stored in the conch.  The conch file is
** stored in the same directory as the database file and the file name
** is patterned after the database file name as ".<databasename>-conch".
** If the conch file does not exist, or it's contents do not match the
** host ID and/or proxy path, then the lock is escalated to an exclusive
** lock and the conch file contents is updated with the host ID and proxy
** path and the lock is downgraded to a shared lock again.  If the conch
** is held by another process (with a shared lock), the exclusive lock
** will fail and SQLITE_BUSY is returned.
**
** The proxy file - a single-byte file used for all advisory file locks
** normally taken on the database file.   This allows for safe sharing
** of the database file for multiple readers and writers on the same
** host (the conch ensures that they all use the same local lock file).
**
** Requesting the lock proxy does not immediately take the conch, it is
** only taken when the first request to lock database file is made.  
** This matches the semantics of the traditional locking behavior, where
** opening a connection to a database file does not take a lock on it.
** The shared lock and an open file descriptor are maintained until 
** the connection to the database is closed. 
**
** The proxy file and the lock file are never deleted so they only need
** to be created the first time they are used.
**
** Configuration options
** ---------------------
**
**  SQLITE_PREFER_PROXY_LOCKING
**
**       Database files accessed on non-local file systems are
**       automatically configured for proxy locking, lock files are
**       named automatically using the same logic as
**       PRAGMA lock_proxy_file=":auto:"
**    
**  SQLITE_PROXY_DEBUG
**
**       Enables the logging of error messages during host id file
**       retrieval and creation
**
**  LOCKPROXYDIR
**
**       Overrides the default directory used for lock proxy files that
**       are named automatically via the ":auto:" setting
**
**  SQLITE_DEFAULT_PROXYDIR_PERMISSIONS
**
**       Permissions to use when creating a directory for storing the
**       lock proxy files, only used when LOCKPROXYDIR is not set.
**    
**    
** As mentioned above, when compiled with SQLITE_PREFER_PROXY_LOCKING,
** setting the environment variable SQLITE_FORCE_PROXY_LOCKING to 1 will
** force proxy locking to be used for every database file opened, and 0
** will force automatic proxy locking to be disabled for all database
** files (explicity calling the SQLITE_SET_LOCKPROXYFILE pragma or
** sqlite_file_control API is not affected by SQLITE_FORCE_PROXY_LOCKING).
*/

/*
** Proxy locking is only available on MacOSX 
*/
#if defined(__APPLE__) && SQLITE_ENABLE_LOCKING_STYLE

/*
** The proxyLockingContext has the path and file structures for the remote 
** and local proxy files in it
*/
typedef struct proxyLockingContext proxyLockingContext;
struct proxyLockingContext {
  unixFile *conchFile;         /* Open conch file */
  char *conchFilePath;         /* Name of the conch file */
  unixFile *lockProxy;         /* Open proxy lock file */
  char *lockProxyPath;         /* Name of the proxy lock file */
  char *dbPath;                /* Name of the open file */
  int conchHeld;               /* 1 if the conch is held, -1 if lockless */
  void *oldLockingContext;     /* Original lockingcontext to restore on close */
  sqlite3_io_methods const *pOldMethod;     /* Original I/O methods for close */
};

/* 
** The proxy lock file path for the database at dbPath is written into lPath, 
** which must point to valid, writable memory large enough for a maxLen length
** file path. 
*/
static int proxyGetLockPath(const char *dbPath, char *lPath, size_t maxLen){
  int len;
  int dbLen;
  int i;

#ifdef LOCKPROXYDIR
  len = strlcpy(lPath, LOCKPROXYDIR, maxLen);
#else
# ifdef _CS_DARWIN_USER_TEMP_DIR
  {
    if( !confstr(_CS_DARWIN_USER_TEMP_DIR, lPath, maxLen) ){
      OSTRACE(("GETLOCKPATH  failed %s errno=%d pid=%d\n",
               lPath, errno, getpid()));
      return SQLITE_IOERR_LOCK;
    }
    len = strlcat(lPath, "sqliteplocks", maxLen);    
  }
# else
  len = strlcpy(lPath, "/tmp/", maxLen);
# endif
#endif

  if( lPath[len-1]!='/' ){
    len = strlcat(lPath, "/", maxLen);
  }
  
  /* transform the db path to a unique cache name */
  dbLen = (int)strlen(dbPath);
  for( i=0; i<dbLen && (i+len+7)<(int)maxLen; i++){
    char c = dbPath[i];
    lPath[i+len] = (c=='/')?'_':c;
  }
  lPath[i+len]='\0';
  strlcat(lPath, ":auto:", maxLen);
  OSTRACE(("GETLOCKPATH  proxy lock path=%s pid=%d\n", lPath, getpid()));
  return SQLITE_OK;
}

/* 
 ** Creates the lock file and any missing directories in lockPath
 */
static int proxyCreateLockPath(const char *lockPath){
  int i, len;
  char buf[MAXPATHLEN];
  int start = 0;
  
  assert(lockPath!=NULL);
  /* try to create all the intermediate directories */
  len = (int)strlen(lockPath);
  buf[0] = lockPath[0];
  for( i=1; i<len; i++ ){
    if( lockPath[i] == '/' && (i - start > 0) ){
      /* only mkdir if leaf dir != "." or "/" or ".." */
      if( i-start>2 || (i-start==1 && buf[start] != '.' && buf[start] != '/') 
         || (i-start==2 && buf[start] != '.' && buf[start+1] != '.') ){
        buf[i]='\0';
        if( osMkdir(buf, SQLITE_DEFAULT_PROXYDIR_PERMISSIONS) ){
          int err=errno;
          if( err!=EEXIST ) {
            OSTRACE(("CREATELOCKPATH  FAILED creating %s, "
                     "'%s' proxy lock path=%s pid=%d\n",
                     buf, strerror(err), lockPath, getpid()));
            return err;
          }
        }
      }
      start=i+1;
    }
    buf[i] = lockPath[i];
  }
  OSTRACE(("CREATELOCKPATH  proxy lock path=%s pid=%d\n", lockPath, getpid()));
  return 0;
}

/*
** Create a new VFS file descriptor (stored in memory obtained from
** sqlite3_malloc) and open the file named "path" in the file descriptor.
**
** The caller is responsible not only for closing the file descriptor
** but also for freeing the memory associated with the file descriptor.
*/
static int proxyCreateUnixFile(
    const char *path,        /* path for the new unixFile */
    unixFile **ppFile,       /* unixFile created and returned by ref */
    int islockfile           /* if non zero missing dirs will be created */
) {
  int fd = -1;
  unixFile *pNew;
  int rc = SQLITE_OK;
  int openFlags = O_RDWR | O_CREAT;
  sqlite3_vfs dummyVfs;
  int terrno = 0;
  UnixUnusedFd *pUnused = NULL;

  /* 1. first try to open/create the file
  ** 2. if that fails, and this is a lock file (not-conch), try creating
  ** the parent directories and then try again.
  ** 3. if that fails, try to open the file read-only
  ** otherwise return BUSY (if lock file) or CANTOPEN for the conch file
  */
  pUnused = findReusableFd(path, openFlags);
  if( pUnused ){
    fd = pUnused->fd;
  }else{
    pUnused = sqlite3_malloc(sizeof(*pUnused));
    if( !pUnused ){
      return SQLITE_NOMEM;
    }
  }
  if( fd<0 ){
    fd = robust_open(path, openFlags, 0);
    terrno = errno;
    if( fd<0 && errno==ENOENT && islockfile ){
      if( proxyCreateLockPath(path) == SQLITE_OK ){
        fd = robust_open(path, openFlags, 0);
      }
    }
  }
  if( fd<0 ){
    openFlags = O_RDONLY;
    fd = robust_open(path, openFlags, 0);
    terrno = errno;
  }
  if( fd<0 ){
    if( islockfile ){
      return SQLITE_BUSY;
    }
    switch (terrno) {
      case EACCES:
        return SQLITE_PERM;
      case EIO: 
        return SQLITE_IOERR_LOCK; /* even though it is the conch */
      default:
        return SQLITE_CANTOPEN_BKPT;
    }
  }
  
  pNew = (unixFile *)sqlite3_malloc(sizeof(*pNew));
  if( pNew==NULL ){
    rc = SQLITE_NOMEM;
    goto end_create_proxy;
  }
  memset(pNew, 0, sizeof(unixFile));
  pNew->openFlags = openFlags;
  memset(&dummyVfs, 0, sizeof(dummyVfs));
  dummyVfs.pAppData = (void*)&autolockIoFinder;
  dummyVfs.zName = "dummy";
  pUnused->fd = fd;
  pUnused->flags = openFlags;
  pNew->pUnused = pUnused;
  
  rc = fillInUnixFile(&dummyVfs, fd, (sqlite3_file*)pNew, path, 0);
  if( rc==SQLITE_OK ){
    *ppFile = pNew;
    return SQLITE_OK;
  }
end_create_proxy:    
  robust_close(pNew, fd, __LINE__);
  sqlite3_free(pNew);
  sqlite3_free(pUnused);
  return rc;
}

#ifdef SQLITE_TEST
/* simulate multiple hosts by creating unique hostid file paths */
int sqlite3_hostid_num = 0;
#endif

#define PROXY_HOSTIDLEN    16  /* conch file host id length */

/* Not always defined in the headers as it ought to be */
extern int gethostuuid(uuid_t id, const struct timespec *wait);

/* get the host ID via gethostuuid(), pHostID must point to PROXY_HOSTIDLEN 
** bytes of writable memory.
*/
static int proxyGetHostID(unsigned char *pHostID, int *pError){
  assert(PROXY_HOSTIDLEN == sizeof(uuid_t));
  memset(pHostID, 0, PROXY_HOSTIDLEN);
#if defined(__MAX_OS_X_VERSION_MIN_REQUIRED)\
               && __MAC_OS_X_VERSION_MIN_REQUIRED<1050
  {
    static const struct timespec timeout = {1, 0}; /* 1 sec timeout */
    if( gethostuuid(pHostID, &timeout) ){
      int err = errno;
      if( pError ){
        *pError = err;
      }
      return SQLITE_IOERR;
    }
  }
#else
  UNUSED_PARAMETER(pError);
#endif
#ifdef SQLITE_TEST
  /* simulate multiple hosts by creating unique hostid file paths */
  if( sqlite3_hostid_num != 0){
    pHostID[0] = (char)(pHostID[0] + (char)(sqlite3_hostid_num & 0xFF));
  }
#endif
  
  return SQLITE_OK;
}

/* The conch file contains the header, host id and lock file path
 */
#define PROXY_CONCHVERSION 2   /* 1-byte header, 16-byte host id, path */
#define PROXY_HEADERLEN    1   /* conch file header length */
#define PROXY_PATHINDEX    (PROXY_HEADERLEN+PROXY_HOSTIDLEN)
#define PROXY_MAXCONCHLEN  (PROXY_HEADERLEN+PROXY_HOSTIDLEN+MAXPATHLEN)

/* 
** Takes an open conch file, copies the contents to a new path and then moves 
** it back.  The newly created file's file descriptor is assigned to the
** conch file structure and finally the original conch file descriptor is 
** closed.  Returns zero if successful.
*/
static int proxyBreakConchLock(unixFile *pFile, uuid_t myHostID){
  proxyLockingContext *pCtx = (proxyLockingContext *)pFile->lockingContext; 
  unixFile *conchFile = pCtx->conchFile;
  char tPath[MAXPATHLEN];
  char buf[PROXY_MAXCONCHLEN];
  char *cPath = pCtx->conchFilePath;
  size_t readLen = 0;
  size_t pathLen = 0;
  char errmsg[64] = "";
  int fd = -1;
  int rc = -1;
  UNUSED_PARAMETER(myHostID);

  /* create a new path by replace the trailing '-conch' with '-break' */
  pathLen = strlcpy(tPath, cPath, MAXPATHLEN);
  if( pathLen>MAXPATHLEN || pathLen<6 || 
     (strlcpy(&tPath[pathLen-5], "break", 6) != 5) ){
    sqlite3_snprintf(sizeof(errmsg),errmsg,"path error (len %d)",(int)pathLen);
    goto end_breaklock;
  }
  /* read the conch content */
  readLen = osPread(conchFile->h, buf, PROXY_MAXCONCHLEN, 0);
  if( readLen<PROXY_PATHINDEX ){
    sqlite3_snprintf(sizeof(errmsg),errmsg,"read error (len %d)",(int)readLen);
    goto end_breaklock;
  }
  /* write it out to the temporary break file */
  fd = robust_open(tPath, (O_RDWR|O_CREAT|O_EXCL), 0);
  if( fd<0 ){
    sqlite3_snprintf(sizeof(errmsg), errmsg, "create failed (%d)", errno);
    goto end_breaklock;
  }
  if( osPwrite(fd, buf, readLen, 0) != (ssize_t)readLen ){
    sqlite3_snprintf(sizeof(errmsg), errmsg, "write failed (%d)", errno);
    goto end_breaklock;
  }
  if( rename(tPath, cPath) ){
    sqlite3_snprintf(sizeof(errmsg), errmsg, "rename failed (%d)", errno);
    goto end_breaklock;
  }
  rc = 0;
  fprintf(stderr, "broke stale lock on %s\n", cPath);
  robust_close(pFile, conchFile->h, __LINE__);
  conchFile->h = fd;
  conchFile->openFlags = O_RDWR | O_CREAT;

end_breaklock:
  if( rc ){
    if( fd>=0 ){
      osUnlink(tPath);
      robust_close(pFile, fd, __LINE__);
    }
    fprintf(stderr, "failed to break stale lock on %s, %s\n", cPath, errmsg);
  }
  return rc;
}

/* Take the requested lock on the conch file and break a stale lock if the 
** host id matches.
*/
static int proxyConchLock(unixFile *pFile, uuid_t myHostID, int lockType){
  proxyLockingContext *pCtx = (proxyLockingContext *)pFile->lockingContext; 
  unixFile *conchFile = pCtx->conchFile;
  int rc = SQLITE_OK;
  int nTries = 0;
  struct timespec conchModTime;
  
  memset(&conchModTime, 0, sizeof(conchModTime));
  do {
    rc = conchFile->pMethod->xLock((sqlite3_file*)conchFile, lockType);
    nTries ++;
    if( rc==SQLITE_BUSY ){
      /* If the lock failed (busy):
       * 1st try: get the mod time of the conch, wait 0.5s and try again. 
       * 2nd try: fail if the mod time changed or host id is different, wait 
       *           10 sec and try again
       * 3rd try: break the lock unless the mod time has changed.
       */
      struct stat buf;
      if( osFstat(conchFile->h, &buf) ){
        pFile->lastErrno = errno;
        return SQLITE_IOERR_LOCK;
      }
      
      if( nTries==1 ){
        conchModTime = buf.st_mtimespec;
        usleep(500000); /* wait 0.5 sec and try the lock again*/
        continue;  
      }

      assert( nTries>1 );
      if( conchModTime.tv_sec != buf.st_mtimespec.tv_sec || 
         conchModTime.tv_nsec != buf.st_mtimespec.tv_nsec ){
        return SQLITE_BUSY;
      }
      
      if( nTries==2 ){  
        char tBuf[PROXY_MAXCONCHLEN];
        int len = osPread(conchFile->h, tBuf, PROXY_MAXCONCHLEN, 0);
        if( len<0 ){
          pFile->lastErrno = errno;
          return SQLITE_IOERR_LOCK;
        }
        if( len>PROXY_PATHINDEX && tBuf[0]==(char)PROXY_CONCHVERSION){
          /* don't break the lock if the host id doesn't match */
          if( 0!=memcmp(&tBuf[PROXY_HEADERLEN], myHostID, PROXY_HOSTIDLEN) ){
            return SQLITE_BUSY;
          }
        }else{
          /* don't break the lock on short read or a version mismatch */
          return SQLITE_BUSY;
        }
        usleep(10000000); /* wait 10 sec and try the lock again */
        continue; 
      }
      
      assert( nTries==3 );
      if( 0==proxyBreakConchLock(pFile, myHostID) ){
        rc = SQLITE_OK;
        if( lockType==EXCLUSIVE_LOCK ){
          rc = conchFile->pMethod->xLock((sqlite3_file*)conchFile, SHARED_LOCK);          
        }
        if( !rc ){
          rc = conchFile->pMethod->xLock((sqlite3_file*)conchFile, lockType);
        }
      }
    }
  } while( rc==SQLITE_BUSY && nTries<3 );
  
  return rc;
}

/* Takes the conch by taking a shared lock and read the contents conch, if 
** lockPath is non-NULL, the host ID and lock file path must match.  A NULL 
** lockPath means that the lockPath in the conch file will be used if the 
** host IDs match, or a new lock path will be generated automatically 
** and written to the conch file.
*/
static int proxyTakeConch(unixFile *pFile){
  proxyLockingContext *pCtx = (proxyLockingContext *)pFile->lockingContext; 
  
  if( pCtx->conchHeld!=0 ){
    return SQLITE_OK;
  }else{
    unixFile *conchFile = pCtx->conchFile;
    uuid_t myHostID;
    int pError = 0;
    char readBuf[PROXY_MAXCONCHLEN];
    char lockPath[MAXPATHLEN];
    char *tempLockPath = NULL;
    int rc = SQLITE_OK;
    int createConch = 0;
    int hostIdMatch = 0;
    int readLen = 0;
    int tryOldLockPath = 0;
    int forceNewLockPath = 0;
    
    OSTRACE(("TAKECONCH  %d for %s pid=%d\n", conchFile->h,
             (pCtx->lockProxyPath ? pCtx->lockProxyPath : ":auto:"), getpid()));

    rc = proxyGetHostID(myHostID, &pError);
    if( (rc&0xff)==SQLITE_IOERR ){
      pFile->lastErrno = pError;
      goto end_takeconch;
    }
    rc = proxyConchLock(pFile, myHostID, SHARED_LOCK);
    if( rc!=SQLITE_OK ){
      goto end_takeconch;
    }
    /* read the existing conch file */
    readLen = seekAndRead((unixFile*)conchFile, 0, readBuf, PROXY_MAXCONCHLEN);
    if( readLen<0 ){
      /* I/O error: lastErrno set by seekAndRead */
      pFile->lastErrno = conchFile->lastErrno;
      rc = SQLITE_IOERR_READ;
      goto end_takeconch;
    }else if( readLen<=(PROXY_HEADERLEN+PROXY_HOSTIDLEN) || 
             readBuf[0]!=(char)PROXY_CONCHVERSION ){
      /* a short read or version format mismatch means we need to create a new 
      ** conch file. 
      */
      createConch = 1;
    }
    /* if the host id matches and the lock path already exists in the conch
    ** we'll try to use the path there, if we can't open that path, we'll 
    ** retry with a new auto-generated path 
    */
    do { /* in case we need to try again for an :auto: named lock file */

      if( !createConch && !forceNewLockPath ){
        hostIdMatch = !memcmp(&readBuf[PROXY_HEADERLEN], myHostID, 
                                  PROXY_HOSTIDLEN);
        /* if the conch has data compare the contents */
        if( !pCtx->lockProxyPath ){
          /* for auto-named local lock file, just check the host ID and we'll
           ** use the local lock file path that's already in there
           */
          if( hostIdMatch ){
            size_t pathLen = (readLen - PROXY_PATHINDEX);
            
            if( pathLen>=MAXPATHLEN ){
              pathLen=MAXPATHLEN-1;
            }
            memcpy(lockPath, &readBuf[PROXY_PATHINDEX], pathLen);
            lockPath[pathLen] = 0;
            tempLockPath = lockPath;
            tryOldLockPath = 1;
            /* create a copy of the lock path if the conch is taken */
            goto end_takeconch;
          }
        }else if( hostIdMatch
               && !strncmp(pCtx->lockProxyPath, &readBuf[PROXY_PATHINDEX],
                           readLen-PROXY_PATHINDEX)
        ){
          /* conch host and lock path match */
          goto end_takeconch; 
        }
      }
      
      /* if the conch isn't writable and doesn't match, we can't take it */
      if( (conchFile->openFlags&O_RDWR) == 0 ){
        rc = SQLITE_BUSY;
        goto end_takeconch;
      }
      
      /* either the conch didn't match or we need to create a new one */
      if( !pCtx->lockProxyPath ){
        proxyGetLockPath(pCtx->dbPath, lockPath, MAXPATHLEN);
        tempLockPath = lockPath;
        /* create a copy of the lock path _only_ if the conch is taken */
      }
      
      /* update conch with host and path (this will fail if other process
      ** has a shared lock already), if the host id matches, use the big
      ** stick.
      */
      futimes(conchFile->h, NULL);
      if( hostIdMatch && !createConch ){
        if( conchFile->pInode && conchFile->pInode->nShared>1 ){
          /* We are trying for an exclusive lock but another thread in this
           ** same process is still holding a shared lock. */
          rc = SQLITE_BUSY;
        } else {          
          rc = proxyConchLock(pFile, myHostID, EXCLUSIVE_LOCK);
        }
      }else{
        rc = conchFile->pMethod->xLock((sqlite3_file*)conchFile, EXCLUSIVE_LOCK);
      }
      if( rc==SQLITE_OK ){
        char writeBuffer[PROXY_MAXCONCHLEN];
        int writeSize = 0;
        
        writeBuffer[0] = (char)PROXY_CONCHVERSION;
        memcpy(&writeBuffer[PROXY_HEADERLEN], myHostID, PROXY_HOSTIDLEN);
        if( pCtx->lockProxyPath!=NULL ){
          strlcpy(&writeBuffer[PROXY_PATHINDEX], pCtx->lockProxyPath, MAXPATHLEN);
        }else{
          strlcpy(&writeBuffer[PROXY_PATHINDEX], tempLockPath, MAXPATHLEN);
        }
        writeSize = PROXY_PATHINDEX + strlen(&writeBuffer[PROXY_PATHINDEX]);
        robust_ftruncate(conchFile->h, writeSize);
        rc = unixWrite((sqlite3_file *)conchFile, writeBuffer, writeSize, 0);
        fsync(conchFile->h);
        /* If we created a new conch file (not just updated the contents of a 
         ** valid conch file), try to match the permissions of the database 
         */
        if( rc==SQLITE_OK && createConch ){
          struct stat buf;
          int err = osFstat(pFile->h, &buf);
          if( err==0 ){
            mode_t cmode = buf.st_mode&(S_IRUSR|S_IWUSR | S_IRGRP|S_IWGRP |
                                        S_IROTH|S_IWOTH);
            /* try to match the database file R/W permissions, ignore failure */
#ifndef SQLITE_PROXY_DEBUG
            osFchmod(conchFile->h, cmode);
#else
            do{
              rc = osFchmod(conchFile->h, cmode);
            }while( rc==(-1) && errno==EINTR );
            if( rc!=0 ){
              int code = errno;
              fprintf(stderr, "fchmod %o FAILED with %d %s\n",
                      cmode, code, strerror(code));
            } else {
              fprintf(stderr, "fchmod %o SUCCEDED\n",cmode);
            }
          }else{
            int code = errno;
            fprintf(stderr, "STAT FAILED[%d] with %d %s\n", 
                    err, code, strerror(code));
#endif
          }
        }
      }
      conchFile->pMethod->xUnlock((sqlite3_file*)conchFile, SHARED_LOCK);
      
    end_takeconch:
      OSTRACE(("TRANSPROXY: CLOSE  %d\n", pFile->h));
      if( rc==SQLITE_OK && pFile->openFlags ){
        int fd;
        if( pFile->h>=0 ){
          robust_close(pFile, pFile->h, __LINE__);
        }
        pFile->h = -1;
        fd = robust_open(pCtx->dbPath, pFile->openFlags, 0);
        OSTRACE(("TRANSPROXY: OPEN  %d\n", fd));
        if( fd>=0 ){
          pFile->h = fd;
        }else{
          rc=SQLITE_CANTOPEN_BKPT; /* SQLITE_BUSY? proxyTakeConch called
           during locking */
        }
      }
      if( rc==SQLITE_OK && !pCtx->lockProxy ){
        char *path = tempLockPath ? tempLockPath : pCtx->lockProxyPath;
        rc = proxyCreateUnixFile(path, &pCtx->lockProxy, 1);
        if( rc!=SQLITE_OK && rc!=SQLITE_NOMEM && tryOldLockPath ){
          /* we couldn't create the proxy lock file with the old lock file path
           ** so try again via auto-naming 
           */
          forceNewLockPath = 1;
          tryOldLockPath = 0;
          continue; /* go back to the do {} while start point, try again */
        }
      }
      if( rc==SQLITE_OK ){
        /* Need to make a copy of path if we extracted the value
         ** from the conch file or the path was allocated on the stack
         */
        if( tempLockPath ){
          pCtx->lockProxyPath = sqlite3DbStrDup(0, tempLockPath);
          if( !pCtx->lockProxyPath ){
            rc = SQLITE_NOMEM;
          }
        }
      }
      if( rc==SQLITE_OK ){
        pCtx->conchHeld = 1;
        
        if( pCtx->lockProxy->pMethod == &afpIoMethods ){
          afpLockingContext *afpCtx;
          afpCtx = (afpLockingContext *)pCtx->lockProxy->lockingContext;
          afpCtx->dbPath = pCtx->lockProxyPath;
        }
      } else {
        conchFile->pMethod->xUnlock((sqlite3_file*)conchFile, NO_LOCK);
      }
      OSTRACE(("TAKECONCH  %d %s\n", conchFile->h,
               rc==SQLITE_OK?"ok":"failed"));
      return rc;
    } while (1); /* in case we need to retry the :auto: lock file - 
                 ** we should never get here except via the 'continue' call. */
  }
}

/*
** If pFile holds a lock on a conch file, then release that lock.
*/
static int proxyReleaseConch(unixFile *pFile){
  int rc = SQLITE_OK;         /* Subroutine return code */
  proxyLockingContext *pCtx;  /* The locking context for the proxy lock */
  unixFile *conchFile;        /* Name of the conch file */

  pCtx = (proxyLockingContext *)pFile->lockingContext;
  conchFile = pCtx->conchFile;
  OSTRACE(("RELEASECONCH  %d for %s pid=%d\n", conchFile->h,
           (pCtx->lockProxyPath ? pCtx->lockProxyPath : ":auto:"), 
           getpid()));
  if( pCtx->conchHeld>0 ){
    rc = conchFile->pMethod->xUnlock((sqlite3_file*)conchFile, NO_LOCK);
  }
  pCtx->conchHeld = 0;
  OSTRACE(("RELEASECONCH  %d %s\n", conchFile->h,
           (rc==SQLITE_OK ? "ok" : "failed")));
  return rc;
}

/*
** Given the name of a database file, compute the name of its conch file.
** Store the conch filename in memory obtained from sqlite3_malloc().
** Make *pConchPath point to the new name.  Return SQLITE_OK on success
** or SQLITE_NOMEM if unable to obtain memory.
**
** The caller is responsible for ensuring that the allocated memory
** space is eventually freed.
**
** *pConchPath is set to NULL if a memory allocation error occurs.
*/
static int proxyCreateConchPathname(char *dbPath, char **pConchPath){
  int i;                        /* Loop counter */
  int len = (int)strlen(dbPath); /* Length of database filename - dbPath */
  char *conchPath;              /* buffer in which to construct conch name */

  /* Allocate space for the conch filename and initialize the name to
  ** the name of the original database file. */  
  *pConchPath = conchPath = (char *)sqlite3_malloc(len + 8);
  if( conchPath==0 ){
    return SQLITE_NOMEM;
  }
  memcpy(conchPath, dbPath, len+1);
  
  /* now insert a "." before the last / character */
  for( i=(len-1); i>=0; i-- ){
    if( conchPath[i]=='/' ){
      i++;
      break;
    }
  }
  conchPath[i]='.';
  while ( i<len ){
    conchPath[i+1]=dbPath[i];
    i++;
  }

  /* append the "-conch" suffix to the file */
  memcpy(&conchPath[i+1], "-conch", 7);
  assert( (int)strlen(conchPath) == len+7 );

  return SQLITE_OK;
}


/* Takes a fully configured proxy locking-style unix file and switches
** the local lock file path 
*/
static int switchLockProxyPath(unixFile *pFile, const char *path) {
  proxyLockingContext *pCtx = (proxyLockingContext*)pFile->lockingContext;
  char *oldPath = pCtx->lockProxyPath;
  int rc = SQLITE_OK;

  if( pFile->eFileLock!=NO_LOCK ){
    return SQLITE_BUSY;
  }  

  /* nothing to do if the path is NULL, :auto: or matches the existing path */
  if( !path || path[0]=='\0' || !strcmp(path, ":auto:") ||
    (oldPath && !strncmp(oldPath, path, MAXPATHLEN)) ){
    return SQLITE_OK;
  }else{
    unixFile *lockProxy = pCtx->lockProxy;
    pCtx->lockProxy=NULL;
    pCtx->conchHeld = 0;
    if( lockProxy!=NULL ){
      rc=lockProxy->pMethod->xClose((sqlite3_file *)lockProxy);
      if( rc ) return rc;
      sqlite3_free(lockProxy);
    }
    sqlite3_free(oldPath);
    pCtx->lockProxyPath = sqlite3DbStrDup(0, path);
  }
  
  return rc;
}

/*
** pFile is a file that has been opened by a prior xOpen call.  dbPath
** is a string buffer at least MAXPATHLEN+1 characters in size.
**
** This routine find the filename associated with pFile and writes it
** int dbPath.
*/
static int proxyGetDbPathForUnixFile(unixFile *pFile, char *dbPath){
#if defined(__APPLE__)
  if( pFile->pMethod == &afpIoMethods ){
    /* afp style keeps a reference to the db path in the filePath field 
    ** of the struct */
    assert( (int)strlen((char*)pFile->lockingContext)<=MAXPATHLEN );
    strlcpy(dbPath, ((afpLockingContext *)pFile->lockingContext)->dbPath, MAXPATHLEN);
  } else
#endif
  if( pFile->pMethod == &dotlockIoMethods ){
    /* dot lock style uses the locking context to store the dot lock
    ** file path */
    int len = strlen((char *)pFile->lockingContext) - strlen(DOTLOCK_SUFFIX);
    memcpy(dbPath, (char *)pFile->lockingContext, len + 1);
  }else{
    /* all other styles use the locking context to store the db file path */
    assert( strlen((char*)pFile->lockingContext)<=MAXPATHLEN );
    strlcpy(dbPath, (char *)pFile->lockingContext, MAXPATHLEN);
  }
  return SQLITE_OK;
}

/*
** Takes an already filled in unix file and alters it so all file locking 
** will be performed on the local proxy lock file.  The following fields
** are preserved in the locking context so that they can be restored and 
** the unix structure properly cleaned up at close time:
**  ->lockingContext
**  ->pMethod
*/
static int proxyTransformUnixFile(unixFile *pFile, const char *path) {
  proxyLockingContext *pCtx;
  char dbPath[MAXPATHLEN+1];       /* Name of the database file */
  char *lockPath=NULL;
  int rc = SQLITE_OK;
  
  if( pFile->eFileLock!=NO_LOCK ){
    return SQLITE_BUSY;
  }
  proxyGetDbPathForUnixFile(pFile, dbPath);
  if( !path || path[0]=='\0' || !strcmp(path, ":auto:") ){
    lockPath=NULL;
  }else{
    lockPath=(char *)path;
  }
  
  OSTRACE(("TRANSPROXY  %d for %s pid=%d\n", pFile->h,
           (lockPath ? lockPath : ":auto:"), getpid()));

  pCtx = sqlite3_malloc( sizeof(*pCtx) );
  if( pCtx==0 ){
    return SQLITE_NOMEM;
  }
  memset(pCtx, 0, sizeof(*pCtx));

  rc = proxyCreateConchPathname(dbPath, &pCtx->conchFilePath);
  if( rc==SQLITE_OK ){
    rc = proxyCreateUnixFile(pCtx->conchFilePath, &pCtx->conchFile, 0);
    if( rc==SQLITE_CANTOPEN && ((pFile->openFlags&O_RDWR) == 0) ){
      /* if (a) the open flags are not O_RDWR, (b) the conch isn't there, and
      ** (c) the file system is read-only, then enable no-locking access.
      ** Ugh, since O_RDONLY==0x0000 we test for !O_RDWR since unixOpen asserts
      ** that openFlags will have only one of O_RDONLY or O_RDWR.
      */
      struct statfs fsInfo;
      struct stat conchInfo;
      int goLockless = 0;

      if( osStat(pCtx->conchFilePath, &conchInfo) == -1 ) {
        int err = errno;
        if( (err==ENOENT) && (statfs(dbPath, &fsInfo) != -1) ){
          goLockless = (fsInfo.f_flags&MNT_RDONLY) == MNT_RDONLY;
        }
      }
      if( goLockless ){
        pCtx->conchHeld = -1; /* read only FS/ lockless */
        rc = SQLITE_OK;
      }
    }
  }  
  if( rc==SQLITE_OK && lockPath ){
    pCtx->lockProxyPath = sqlite3DbStrDup(0, lockPath);
  }

  if( rc==SQLITE_OK ){
    pCtx->dbPath = sqlite3DbStrDup(0, dbPath);
    if( pCtx->dbPath==NULL ){
      rc = SQLITE_NOMEM;
    }
  }
  if( rc==SQLITE_OK ){
    /* all memory is allocated, proxys are created and assigned, 
    ** switch the locking context and pMethod then return.
    */
    pCtx->oldLockingContext = pFile->lockingContext;
    pFile->lockingContext = pCtx;
    pCtx->pOldMethod = pFile->pMethod;
    pFile->pMethod = &proxyIoMethods;
  }else{
    if( pCtx->conchFile ){ 
      pCtx->conchFile->pMethod->xClose((sqlite3_file *)pCtx->conchFile);
      sqlite3_free(pCtx->conchFile);
    }
    sqlite3DbFree(0, pCtx->lockProxyPath);
    sqlite3_free(pCtx->conchFilePath); 
    sqlite3_free(pCtx);
  }
  OSTRACE(("TRANSPROXY  %d %s\n", pFile->h,
           (rc==SQLITE_OK ? "ok" : "failed")));
  return rc;
}


/*
** This routine handles sqlite3_file_control() calls that are specific
** to proxy locking.
*/
static int proxyFileControl(sqlite3_file *id, int op, void *pArg){
  switch( op ){
    case SQLITE_GET_LOCKPROXYFILE: {
      unixFile *pFile = (unixFile*)id;
      if( pFile->pMethod == &proxyIoMethods ){
        proxyLockingContext *pCtx = (proxyLockingContext*)pFile->lockingContext;
        proxyTakeConch(pFile);
        if( pCtx->lockProxyPath ){
          *(const char **)pArg = pCtx->lockProxyPath;
        }else{
          *(const char **)pArg = ":auto: (not held)";
        }
      } else {
        *(const char **)pArg = NULL;
      }
      return SQLITE_OK;
    }
    case SQLITE_SET_LOCKPROXYFILE: {
      unixFile *pFile = (unixFile*)id;
      int rc = SQLITE_OK;
      int isProxyStyle = (pFile->pMethod == &proxyIoMethods);
      if( pArg==NULL || (const char *)pArg==0 ){
        if( isProxyStyle ){
          /* turn off proxy locking - not supported */
          rc = SQLITE_ERROR /*SQLITE_PROTOCOL? SQLITE_MISUSE?*/;
        }else{
          /* turn off proxy locking - already off - NOOP */
          rc = SQLITE_OK;
        }
      }else{
        const char *proxyPath = (const char *)pArg;
        if( isProxyStyle ){
          proxyLockingContext *pCtx = 
            (proxyLockingContext*)pFile->lockingContext;
          if( !strcmp(pArg, ":auto:") 
           || (pCtx->lockProxyPath &&
               !strncmp(pCtx->lockProxyPath, proxyPath, MAXPATHLEN))
          ){
            rc = SQLITE_OK;
          }else{
            rc = switchLockProxyPath(pFile, proxyPath);
          }
        }else{
          /* turn on proxy file locking */
          rc = proxyTransformUnixFile(pFile, proxyPath);
        }
      }
      return rc;
    }
    default: {
      assert( 0 );  /* The call assures that only valid opcodes are sent */
    }
  }
  /*NOTREACHED*/
  return SQLITE_ERROR;
}

/*
** Within this division (the proxying locking implementation) the procedures
** above this point are all utilities.  The lock-related methods of the
** proxy-locking sqlite3_io_method object follow.
*/


/*
** This routine checks if there is a RESERVED lock held on the specified
** file by this or any other process. If such a lock is held, set *pResOut
** to a non-zero value otherwise *pResOut is set to zero.  The return value
** is set to SQLITE_OK unless an I/O error occurs during lock checking.
*/
static int proxyCheckReservedLock(sqlite3_file *id, int *pResOut) {
  unixFile *pFile = (unixFile*)id;
  int rc = proxyTakeConch(pFile);
  if( rc==SQLITE_OK ){
    proxyLockingContext *pCtx = (proxyLockingContext *)pFile->lockingContext;
    if( pCtx->conchHeld>0 ){
      unixFile *proxy = pCtx->lockProxy;
      return proxy->pMethod->xCheckReservedLock((sqlite3_file*)proxy, pResOut);
    }else{ /* conchHeld < 0 is lockless */
      pResOut=0;
    }
  }
  return rc;
}

/*
** Lock the file with the lock specified by parameter eFileLock - one
** of the following:
**
**     (1) SHARED_LOCK
**     (2) RESERVED_LOCK
**     (3) PENDING_LOCK
**     (4) EXCLUSIVE_LOCK
**
** Sometimes when requesting one lock state, additional lock states
** are inserted in between.  The locking might fail on one of the later
** transitions leaving the lock state different from what it started but
** still short of its goal.  The following chart shows the allowed
** transitions and the inserted intermediate states:
**
**    UNLOCKED -> SHARED
**    SHARED -> RESERVED
**    SHARED -> (PENDING) -> EXCLUSIVE
**    RESERVED -> (PENDING) -> EXCLUSIVE
**    PENDING -> EXCLUSIVE
**
** This routine will only increase a lock.  Use the sqlite3OsUnlock()
** routine to lower a locking level.
*/
static int proxyLock(sqlite3_file *id, int eFileLock) {
  unixFile *pFile = (unixFile*)id;
  int rc = proxyTakeConch(pFile);
  if( rc==SQLITE_OK ){
    proxyLockingContext *pCtx = (proxyLockingContext *)pFile->lockingContext;
    if( pCtx->conchHeld>0 ){
      unixFile *proxy = pCtx->lockProxy;
      rc = proxy->pMethod->xLock((sqlite3_file*)proxy, eFileLock);
      pFile->eFileLock = proxy->eFileLock;
    }else{
      /* conchHeld < 0 is lockless */
    }
  }
  return rc;
}


/*
** Lower the locking level on file descriptor pFile to eFileLock.  eFileLock
** must be either NO_LOCK or SHARED_LOCK.
**
** If the locking level of the file descriptor is already at or below
** the requested locking level, this routine is a no-op.
*/
static int proxyUnlock(sqlite3_file *id, int eFileLock) {
  unixFile *pFile = (unixFile*)id;
  int rc = proxyTakeConch(pFile);
  if( rc==SQLITE_OK ){
    proxyLockingContext *pCtx = (proxyLockingContext *)pFile->lockingContext;
    if( pCtx->conchHeld>0 ){
      unixFile *proxy = pCtx->lockProxy;
      rc = proxy->pMethod->xUnlock((sqlite3_file*)proxy, eFileLock);
      pFile->eFileLock = proxy->eFileLock;
    }else{
      /* conchHeld < 0 is lockless */
    }
  }
  return rc;
}

/*
** Close a file that uses proxy locks.
*/
static int proxyClose(sqlite3_file *id) {
  if( id ){
    unixFile *pFile = (unixFile*)id;
    proxyLockingContext *pCtx = (proxyLockingContext *)pFile->lockingContext;
    unixFile *lockProxy = pCtx->lockProxy;
    unixFile *conchFile = pCtx->conchFile;
    int rc = SQLITE_OK;
    
    if( lockProxy ){
      rc = lockProxy->pMethod->xUnlock((sqlite3_file*)lockProxy, NO_LOCK);
      if( rc ) return rc;
      rc = lockProxy->pMethod->xClose((sqlite3_file*)lockProxy);
      if( rc ) return rc;
      sqlite3_free(lockProxy);
      pCtx->lockProxy = 0;
    }
    if( conchFile ){
      if( pCtx->conchHeld ){
        rc = proxyReleaseConch(pFile);
        if( rc ) return rc;
      }
      rc = conchFile->pMethod->xClose((sqlite3_file*)conchFile);
      if( rc ) return rc;
      sqlite3_free(conchFile);
    }
    sqlite3DbFree(0, pCtx->lockProxyPath);
    sqlite3_free(pCtx->conchFilePath);
    sqlite3DbFree(0, pCtx->dbPath);
    /* restore the original locking context and pMethod then close it */
    pFile->lockingContext = pCtx->oldLockingContext;
    pFile->pMethod = pCtx->pOldMethod;
    sqlite3_free(pCtx);
    return pFile->pMethod->xClose(id);
  }
  return SQLITE_OK;
}



#endif /* defined(__APPLE__) && SQLITE_ENABLE_LOCKING_STYLE */
/*
** The proxy locking style is intended for use with AFP filesystems.
** And since AFP is only supported on MacOSX, the proxy locking is also
** restricted to MacOSX.
** 
**
******************* End of the proxy lock implementation **********************
******************************************************************************/

/*
** Initialize the operating system interface.
**
** This routine registers all VFS implementations for unix-like operating
** systems.  This routine, and the sqlite3_os_end() routine that follows,
** should be the only routines in this file that are visible from other
** files.
**
** This routine is called once during SQLite initialization and by a
** single thread.  The memory allocation and mutex subsystems have not
** necessarily been initialized when this routine is called, and so they
** should not be used.
*/
int sqlite3_os_init(void){ 
  /* 
  ** The following macro defines an initializer for an sqlite3_vfs object.
  ** The name of the VFS is NAME.  The pAppData is a pointer to a pointer
  ** to the "finder" function.  (pAppData is a pointer to a pointer because
  ** silly C90 rules prohibit a void* from being cast to a function pointer
  ** and so we have to go through the intermediate pointer to avoid problems
  ** when compiling with -pedantic-errors on GCC.)
  **
  ** The FINDER parameter to this macro is the name of the pointer to the
  ** finder-function.  The finder-function returns a pointer to the
  ** sqlite_io_methods object that implements the desired locking
  ** behaviors.  See the division above that contains the IOMETHODS
  ** macro for addition information on finder-functions.
  **
  ** Most finders simply return a pointer to a fixed sqlite3_io_methods
  ** object.  But the "autolockIoFinder" available on MacOSX does a little
  ** more than that; it looks at the filesystem type that hosts the 
  ** database file and tries to choose an locking method appropriate for
  ** that filesystem time.
  */
  #define UNIXVFS(VFSNAME, FINDER) {                        \
    3,                    /* iVersion */                    \
    sizeof(unixFile),     /* szOsFile */                    \
    MAX_PATHNAME,         /* mxPathname */                  \
    0,                    /* pNext */                       \
    VFSNAME,              /* zName */                       \
    (void*)&FINDER,       /* pAppData */                    \
    unixOpen,             /* xOpen */                       \
    unixDelete,           /* xDelete */                     \
    unixAccess,           /* xAccess */                     \
    unixFullPathname,     /* xFullPathname */               \
    unixDlOpen,           /* xDlOpen */                     \
    unixDlError,          /* xDlError */                    \
    unixDlSym,            /* xDlSym */                      \
    unixDlClose,          /* xDlClose */                    \
    unixRandomness,       /* xRandomness */                 \
    unixSleep,            /* xSleep */                      \
    unixCurrentTime,      /* xCurrentTime */                \
    unixGetLastError,     /* xGetLastError */               \
    unixCurrentTimeInt64, /* xCurrentTimeInt64 */           \
    unixSetSystemCall,    /* xSetSystemCall */              \
    unixGetSystemCall,    /* xGetSystemCall */              \
    unixNextSystemCall,   /* xNextSystemCall */             \
  }

  /*
  ** All default VFSes for unix are contained in the following array.
  **
  ** Note that the sqlite3_vfs.pNext field of the VFS object is modified
  ** by the SQLite core when the VFS is registered.  So the following
  ** array cannot be const.
  */
  static sqlite3_vfs aVfs[] = {
#if SQLITE_ENABLE_LOCKING_STYLE && (OS_VXWORKS || defined(__APPLE__))
    UNIXVFS("unix",          autolockIoFinder ),
#else
    UNIXVFS("unix",          posixIoFinder ),
#endif
    UNIXVFS("unix-none",     nolockIoFinder ),
    UNIXVFS("unix-dotfile",  dotlockIoFinder ),
    UNIXVFS("unix-excl",     posixIoFinder ),
#if OS_VXWORKS
    UNIXVFS("unix-namedsem", semIoFinder ),
#endif
#if SQLITE_ENABLE_LOCKING_STYLE
    UNIXVFS("unix-posix",    posixIoFinder ),
#if !OS_VXWORKS
    UNIXVFS("unix-flock",    flockIoFinder ),
#endif
#endif
#if SQLITE_ENABLE_LOCKING_STYLE && defined(__APPLE__)
    UNIXVFS("unix-afp",      afpIoFinder ),
    UNIXVFS("unix-nfs",      nfsIoFinder ),
    UNIXVFS("unix-proxy",    proxyIoFinder ),
#endif
  };
  unsigned int i;          /* Loop counter */

  /* Double-check that the aSyscall[] array has been constructed
  ** correctly.  See ticket [bb3a86e890c8e96ab] */
  assert( ArraySize(aSyscall)==22 );

  /* Register all VFSes defined in the aVfs[] array */
  for(i=0; i<(sizeof(aVfs)/sizeof(sqlite3_vfs)); i++){
    sqlite3_vfs_register(&aVfs[i], i==0);
  }
  return SQLITE_OK; 
}

/*
** Shutdown the operating system interface.
**
** Some operating systems might need to do some cleanup in this routine,
** to release dynamically allocated objects.  But not on unix.
** This routine is a no-op for unix.
*/
int sqlite3_os_end(void){ 
  return SQLITE_OK; 
}
 
#endif /* SQLITE_OS_UNIX */
