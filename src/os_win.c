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
** This file contains code that is specific to Windows.
** 此文件包含代码是特定于Windows.
*/
#include "sqliteInt.h"
#if SQLITE_OS_WIN               /* This file is used for Windows only 这个文件是用来仅适用于Windows */

#ifdef __CYGWIN__
# include <sys/cygwin.h>
#endif

/*
<<<<<<< HEAD
** Include code that is common to all os_*.c file
=======
** Include code that is common to all os_*.c files
** 包含的代码适用于所有os_*.c文件 
>>>>>>> 3d313061b3a68c29e8436c70be9f9507d13d8543
*/
#include "os_common.h"

/*
** Macro to find the minimum of two numeric values.
** 宏找到至少两个数值。
*/
#ifndef MIN
# define MIN(x,y) ((x)<(y)?(x):(y))
#endif

/*
** Some Microsoft compilers lack this definition.
** 一些微软的编译器缺乏这种定义。
*/
#ifndef INVALID_FILE_ATTRIBUTES
# define INVALID_FILE_ATTRIBUTES ((DWORD)-1) 
#endif

#ifndef FILE_FLAG_MASK
# define FILE_FLAG_MASK          (0xFF3C0000)
#endif

#ifndef FILE_ATTRIBUTE_MASK
# define FILE_ATTRIBUTE_MASK     (0x0003FFF7)
#endif

#ifndef SQLITE_OMIT_WAL
/* Forward references */
/*向前引用*/
typedef struct winShm winShm;           /* A connection to shared-memory 一个共享内存连接 */
typedef struct winShmNode winShmNode;   /* A region of shared-memory 一个共享内存区域 */
#endif

/*
** WinCE lacks native support for file locking so we have to fake it
** with some code of our own.
**WinCE缺乏原生支持文件锁定，所以我们必须用一些自己代码修改它
*/
#if SQLITE_OS_WINCE
typedef struct winceLock {
  int nReaders;       /* Number of reader locks obtained 一些阅读器锁定所获取内容*/
  BOOL bPending;      /* Indicates a pending lock has been obtained 指定一个已经获得未决锁*/
  BOOL bReserved;     /* Indicates a reserved lock has been obtained 指定一个已经获得的保留锁*/
  BOOL bExclusive;    /* Indicates an exclusive lock has been obtained 指定一个已经获得的排他锁*/
} winceLock;
#endif

/*
** The winFile structure is a subclass of sqlite3_file* specific to the win32
** portability layer.
**winfile结构是 sqlite3_file* 仅限于win32可移植层一个子类
*/
typedef struct winFile winFile;
struct winFile {
  const sqlite3_io_methods *pMethod; /*** Must be first 必须是首先的  ***/
  sqlite3_vfs *pVfs;      /* The VFS used to open this file VFS用来打开此文件*/
  HANDLE h;               /* Handle for accessing the file 访问文件的句柄 */
  u8 locktype;            /* Type of lock currently held on this file 目前持有此文件的锁类型*/
  short sharedLockByte;   /* Randomly chosen byte used as a shared lock 用作共享锁随机选择的字节*/
  u8 ctrlFlags;           /* Flags.  See WINFILE_* below 标识.  见下文WINFILE_* */
  DWORD lastErrno;        /* The Windows errno from the last I/O error Windows的错误号来源于最后一个I / O错误*/
#ifndef SQLITE_OMIT_WAL
  winShm *pShm;           /* Instance of shared memory on this file 此文件共享内存的实例*/
#endif
  const char *zPath;      /* Full pathname of this file 这个文件的全路径名*/
  int szChunk;            /* Chunk size configured by FCNTL_CHUNK_SIZE 通过FCNTL_CHUNK_SIZE配置的块大小 */
#if SQLITE_OS_WINCE
  LPWSTR zDeleteOnClose;  /* Name of file to delete when closing 当关闭时删除文件的名称*/
  HANDLE hMutex;          /* Mutex used to control access to shared lock 互斥用于控制访问共享锁*/  
  HANDLE hShared;         /* Shared memory segment used for locking 共享内存段用于锁定*/
  winceLock local;        /* Locks obtained by this instance of winFile 通过该WINFILE的实例获取锁*/
  winceLock *shared;      /* Global shared lock memory for the file  储存该文件的全局共享锁*/
#endif
};

/*
** Allowed values for winFile.ctrlFlags
**允许winFile.ctrlFlags值
*/
#define WINFILE_PERSIST_WAL     0x04   /* Persistent WAL mode 持续WAL模式*/
#define WINFILE_PSOW            0x10   /* SQLITE_IOCAP_POWERSAFE_OVERWRITE */

/*
 * The size of the buffer used by sqlite3_win32_write_debug().
 *sqlite3_win32_write_debug所使用到的缓冲区的大小。
 */
#ifndef SQLITE_WIN32_DBG_BUF_SIZE
#  define SQLITE_WIN32_DBG_BUF_SIZE   ((int)(4096-sizeof(DWORD)))
#endif

/*
 * The value used with sqlite3_win32_set_directory() to specify that
 * the data directory should be changed.
 *与sqlite3_win32_set_directory()使用的值指定
 *数据目录应该有所改变。
 */
#ifndef SQLITE_WIN32_DATA_DIRECTORY_TYPE
#  define SQLITE_WIN32_DATA_DIRECTORY_TYPE (1)
#endif

/*
 * The value used with sqlite3_win32_set_directory() to specify that
 * the temporary directory should be changed.
 *与sqlite3_win32_set_directory()中使用的值来指定
 *临时目录应该改变。
 */
#ifndef SQLITE_WIN32_TEMP_DIRECTORY_TYPE
#  define SQLITE_WIN32_TEMP_DIRECTORY_TYPE (2)
#endif

/*
 * If compiled with SQLITE_WIN32_MALLOC on Windows, we will use the
 * various Win32 API heap functions instead of our own.
 *如果有SQLITE_WIN32_MALLOC在Windows上编译，我们将使用
 *不同的Win32 API堆函数，而不是我们自己的。
 */
#ifdef SQLITE_WIN32_MALLOC

/*
 * If this is non-zero, an isolated heap will be created by the native Win32
 * allocator subsystem; otherwise, the default process heap will be used.  This
 * setting has no effect when compiling for WinRT.  By default, this is enabled
 * and an isolated heap will be created to store all allocated data.
 * 如果这是非零，独立的堆将由本地的Win32分配器子系统创建； 
 * 否则，默认的进程堆将被使用。当编译的WinRT的时候此设置不起作用。 
 * 默认情况下，启用该选项和独立堆将创建用以存储所有分配的数据。
 *
 ******************************************************************************
 * WARNING: It is important to note that when this setting is non-zero and the
 *          winMemShutdown function is called (e.g. by the sqlite3_shutdown
 *          function), all data that was allocated using the isolated heap will
 *          be freed immediately and any attempt to access any of that freed
 *          data will almost certainly result in an immediate access violation.
 * 警告: 需要注意的是，当该设置为非零并且winMemShutdown函数被调用
 * (例如sqlite3_shutdown函数), 所有使用独立堆的被分配数据将会立即被释放，并且任
 *何企图访问任何的释放数据几乎肯定会导致立即访问冲突 。
 ******************************************************************************
 */
#ifndef SQLITE_WIN32_HEAP_CREATE
#  define SQLITE_WIN32_HEAP_CREATE    (TRUE)
#endif

/*
 * The initial size of the Win32-specific heap.  This value may be zero.
 *特定Win32堆的初始大小。这个值可以是零
 */
#ifndef SQLITE_WIN32_HEAP_INIT_SIZE
#  define SQLITE_WIN32_HEAP_INIT_SIZE ((SQLITE_DEFAULT_CACHE_SIZE) * \
                                       (SQLITE_DEFAULT_PAGE_SIZE) + 4194304)
#endif

/*
 * The maximum size of the Win32-specific heap.  This value may be zero.
 *在特定Win32堆的最大大小。这个值可以是零。
 */
#ifndef SQLITE_WIN32_HEAP_MAX_SIZE
#  define SQLITE_WIN32_HEAP_MAX_SIZE  (0)
#endif

/*
 * The extra flags to use in calls to the Win32 heap APIs.  This value may be
 * zero for the default behavior.
 * 在调用Win32的堆API中使用额外的标志.  这个值可以默认设置为0；
 */
#ifndef SQLITE_WIN32_HEAP_FLAGS
#  define SQLITE_WIN32_HEAP_FLAGS     (0)
#endif

/*
** The winMemData structure stores information required by the Win32-specific
** sqlite3_mem_methods implementation.
**由特定Win32所要求的winMemData结构存储信息sqlite3_mem_methods实施。
*/
typedef struct winMemData winMemData;
struct winMemData {
#ifndef NDEBUG
  u32 magic;    /* Magic number to detect structure corruption. Magic数值来检测结构损坏 */
#endif
  HANDLE hHeap; /* The handle to our heap. 我们堆的句柄 */
  BOOL bOwned;  /* Do we own the heap (i.e. destroy it on shutdown)? 我们拥有这个堆吗(即关机销毁堆)? */
};

#ifndef NDEBUG
#define WINMEM_MAGIC     0x42b2830b
#endif

static struct winMemData win_mem_data = {
#ifndef NDEBUG
  WINMEM_MAGIC,
#endif
  NULL, FALSE
};

#ifndef NDEBUG
#define winMemAssertMagic() assert( win_mem_data.magic==WINMEM_MAGIC )
#else
#define winMemAssertMagic()
#endif

#define winMemGetHeap() win_mem_data.hHeap

static void *winMemMalloc(int nBytes);
static void winMemFree(void *pPrior);
static void *winMemRealloc(void *pPrior, int nBytes);
static int winMemSize(void *p);
static int winMemRoundup(int n);
static int winMemInit(void *pAppData);
static void winMemShutdown(void *pAppData);

const sqlite3_mem_methods *sqlite3MemGetWin32(void);
#endif /* SQLITE_WIN32_MALLOC */

/*
** The following variable is (normally) set once and never changes
** thereafter.  It records whether the operating system is Win9x
** or WinNT.
下面的变量（一般）设置一次，其后永远不会改变
** 它记录的操作系统是否为Win9x的或WinNT。
**
** 0:   Operating system unknown. 操作系统未知。
** 1:   Operating system is Win9x. 操作系统是Win9x。
** 2:   Operating system is WinNT. 操作系统是WinNT。
**
** In order to facilitate testing on a WinNT system, the test fixture
** can manually set this value to 1 to emulate Win98 behavior.
** 为了便于在WinNT系统上测试, 测试装置 
** 可以手动将该值设置为1来模拟Win98的行为。
*/
#ifdef SQLITE_TEST
int sqlite3_os_type = 0;
#else
static int sqlite3_os_type = 0;
#endif

#if !SQLITE_OS_WINCE && !SQLITE_OS_WINRT
#  define SQLITE_WIN32_HAS_ANSI
#endif

#if SQLITE_OS_WINCE || SQLITE_OS_WINNT || SQLITE_OS_WINRT
#  define SQLITE_WIN32_HAS_WIDE
#endif

#ifndef SYSCALL
#  define SYSCALL sqlite3_syscall_ptr
#endif

/*
** This function is not available on Windows CE or WinRT.
**此功能不适用于Windows CE或WinRT。
 */

#if SQLITE_OS_WINCE || SQLITE_OS_WINRT
#  define osAreFileApisANSI()       1
#endif

/*
** Many system calls are accessed through pointer-to-functions so that
** they may be overridden at runtime to facilitate fault injection during
** testing and sandboxing.  The following array holds the names and pointers
** to all overrideable system calls.
** 许多系统调用被访问都是通过指针到功能以便
** 在测试和沙盒时他们可能会在运行时重写来便于故障注入
** 下面的数组保存的姓名和指针为所有重载系统所调用。
*/
static struct win_syscall {
  const char *zName;            /* Name of the sytem call 该系统正调用的名称*/
  sqlite3_syscall_ptr pCurrent; /* Current value of the system call 系统调用的当前值*/
  sqlite3_syscall_ptr pDefault; /* Default value 默认值*/
} aSyscall[] = {
#if !SQLITE_OS_WINCE && !SQLITE_OS_WINRT
  { "AreFileApisANSI",         (SYSCALL)AreFileApisANSI,         0 },
#else
  { "AreFileApisANSI",         (SYSCALL)0,                       0 },
#endif

#ifndef osAreFileApisANSI
#define osAreFileApisANSI ((BOOL(WINAPI*)(VOID))aSyscall[0].pCurrent)
#endif

#if SQLITE_OS_WINCE && defined(SQLITE_WIN32_HAS_WIDE)
  { "CharLowerW",              (SYSCALL)CharLowerW,              0 },
#else
  { "CharLowerW",              (SYSCALL)0,                       0 },
#endif

#define osCharLowerW ((LPWSTR(WINAPI*)(LPWSTR))aSyscall[1].pCurrent)

#if SQLITE_OS_WINCE && defined(SQLITE_WIN32_HAS_WIDE)
  { "CharUpperW",              (SYSCALL)CharUpperW,              0 },
#else
  { "CharUpperW",              (SYSCALL)0,                       0 },
#endif

#define osCharUpperW ((LPWSTR(WINAPI*)(LPWSTR))aSyscall[2].pCurrent)

  { "CloseHandle",             (SYSCALL)CloseHandle,             0 },

#define osCloseHandle ((BOOL(WINAPI*)(HANDLE))aSyscall[3].pCurrent)

#if defined(SQLITE_WIN32_HAS_ANSI)
  { "CreateFileA",             (SYSCALL)CreateFileA,             0 },
#else
  { "CreateFileA",             (SYSCALL)0,                       0 },
#endif

#define osCreateFileA ((HANDLE(WINAPI*)(LPCSTR,DWORD,DWORD, \
        LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE))aSyscall[4].pCurrent)

#if !SQLITE_OS_WINRT && defined(SQLITE_WIN32_HAS_WIDE)
  { "CreateFileW",             (SYSCALL)CreateFileW,             0 },
#else
  { "CreateFileW",             (SYSCALL)0,                       0 },
#endif

#define osCreateFileW ((HANDLE(WINAPI*)(LPCWSTR,DWORD,DWORD, \
        LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE))aSyscall[5].pCurrent)

#if SQLITE_OS_WINCE || (!SQLITE_OS_WINRT && defined(SQLITE_WIN32_HAS_WIDE) && \
        !defined(SQLITE_OMIT_WAL))
  { "CreateFileMappingW",      (SYSCALL)CreateFileMappingW,      0 },
#else
  { "CreateFileMappingW",      (SYSCALL)0,                       0 },
#endif

#define osCreateFileMappingW ((HANDLE(WINAPI*)(HANDLE,LPSECURITY_ATTRIBUTES, \
        DWORD,DWORD,DWORD,LPCWSTR))aSyscall[6].pCurrent)

#if !SQLITE_OS_WINRT && defined(SQLITE_WIN32_HAS_WIDE)
  { "CreateMutexW",            (SYSCALL)CreateMutexW,            0 },
#else
  { "CreateMutexW",            (SYSCALL)0,                       0 },
#endif

#define osCreateMutexW ((HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES,BOOL, \
        LPCWSTR))aSyscall[7].pCurrent)

#if defined(SQLITE_WIN32_HAS_ANSI)
  { "DeleteFileA",             (SYSCALL)DeleteFileA,             0 },
#else
  { "DeleteFileA",             (SYSCALL)0,                       0 },
#endif

#define osDeleteFileA ((BOOL(WINAPI*)(LPCSTR))aSyscall[8].pCurrent)

#if defined(SQLITE_WIN32_HAS_WIDE)
  { "DeleteFileW",             (SYSCALL)DeleteFileW,             0 },
#else
  { "DeleteFileW",             (SYSCALL)0,                       0 },
#endif

#define osDeleteFileW ((BOOL(WINAPI*)(LPCWSTR))aSyscall[9].pCurrent)

#if SQLITE_OS_WINCE
  { "FileTimeToLocalFileTime", (SYSCALL)FileTimeToLocalFileTime, 0 },
#else
  { "FileTimeToLocalFileTime", (SYSCALL)0,                       0 },
#endif

#define osFileTimeToLocalFileTime ((BOOL(WINAPI*)(CONST FILETIME*, \
        LPFILETIME))aSyscall[10].pCurrent)

#if SQLITE_OS_WINCE
  { "FileTimeToSystemTime",    (SYSCALL)FileTimeToSystemTime,    0 },
#else
  { "FileTimeToSystemTime",    (SYSCALL)0,                       0 },
#endif

#define osFileTimeToSystemTime ((BOOL(WINAPI*)(CONST FILETIME*, \
        LPSYSTEMTIME))aSyscall[11].pCurrent)

  { "FlushFileBuffers",        (SYSCALL)FlushFileBuffers,        0 },

#define osFlushFileBuffers ((BOOL(WINAPI*)(HANDLE))aSyscall[12].pCurrent)

#if defined(SQLITE_WIN32_HAS_ANSI)
  { "FormatMessageA",          (SYSCALL)FormatMessageA,          0 },
#else
  { "FormatMessageA",          (SYSCALL)0,                       0 },
#endif

#define osFormatMessageA ((DWORD(WINAPI*)(DWORD,LPCVOID,DWORD,DWORD,LPSTR, \
        DWORD,va_list*))aSyscall[13].pCurrent)

#if defined(SQLITE_WIN32_HAS_WIDE)
  { "FormatMessageW",          (SYSCALL)FormatMessageW,          0 },
#else
  { "FormatMessageW",          (SYSCALL)0,                       0 },
#endif

#define osFormatMessageW ((DWORD(WINAPI*)(DWORD,LPCVOID,DWORD,DWORD,LPWSTR, \
        DWORD,va_list*))aSyscall[14].pCurrent)

  { "FreeLibrary",             (SYSCALL)FreeLibrary,             0 },

#define osFreeLibrary ((BOOL(WINAPI*)(HMODULE))aSyscall[15].pCurrent)

  { "GetCurrentProcessId",     (SYSCALL)GetCurrentProcessId,     0 },

#define osGetCurrentProcessId ((DWORD(WINAPI*)(VOID))aSyscall[16].pCurrent)

#if !SQLITE_OS_WINCE && defined(SQLITE_WIN32_HAS_ANSI)
  { "GetDiskFreeSpaceA",       (SYSCALL)GetDiskFreeSpaceA,       0 },
#else
  { "GetDiskFreeSpaceA",       (SYSCALL)0,                       0 },
#endif

#define osGetDiskFreeSpaceA ((BOOL(WINAPI*)(LPCSTR,LPDWORD,LPDWORD,LPDWORD, \
        LPDWORD))aSyscall[17].pCurrent)

#if !SQLITE_OS_WINCE && !SQLITE_OS_WINRT && defined(SQLITE_WIN32_HAS_WIDE)
  { "GetDiskFreeSpaceW",       (SYSCALL)GetDiskFreeSpaceW,       0 },
#else
  { "GetDiskFreeSpaceW",       (SYSCALL)0,                       0 },
#endif

#define osGetDiskFreeSpaceW ((BOOL(WINAPI*)(LPCWSTR,LPDWORD,LPDWORD,LPDWORD, \
        LPDWORD))aSyscall[18].pCurrent)

#if defined(SQLITE_WIN32_HAS_ANSI)
  { "GetFileAttributesA",      (SYSCALL)GetFileAttributesA,      0 },
#else
  { "GetFileAttributesA",      (SYSCALL)0,                       0 },
#endif

#define osGetFileAttributesA ((DWORD(WINAPI*)(LPCSTR))aSyscall[19].pCurrent)

#if !SQLITE_OS_WINRT && defined(SQLITE_WIN32_HAS_WIDE)
  { "GetFileAttributesW",      (SYSCALL)GetFileAttributesW,      0 },
#else
  { "GetFileAttributesW",      (SYSCALL)0,                       0 },
#endif

#define osGetFileAttributesW ((DWORD(WINAPI*)(LPCWSTR))aSyscall[20].pCurrent)

#if defined(SQLITE_WIN32_HAS_WIDE)
  { "GetFileAttributesExW",    (SYSCALL)GetFileAttributesExW,    0 },
#else
  { "GetFileAttributesExW",    (SYSCALL)0,                       0 },
#endif

#define osGetFileAttributesExW ((BOOL(WINAPI*)(LPCWSTR,GET_FILEEX_INFO_LEVELS, \
        LPVOID))aSyscall[21].pCurrent)

#if !SQLITE_OS_WINRT
  { "GetFileSize",             (SYSCALL)GetFileSize,             0 },
#else
  { "GetFileSize",             (SYSCALL)0,                       0 },
#endif

#define osGetFileSize ((DWORD(WINAPI*)(HANDLE,LPDWORD))aSyscall[22].pCurrent)

#if !SQLITE_OS_WINCE && defined(SQLITE_WIN32_HAS_ANSI)
  { "GetFullPathNameA",        (SYSCALL)GetFullPathNameA,        0 },
#else
  { "GetFullPathNameA",        (SYSCALL)0,                       0 },
#endif

#define osGetFullPathNameA ((DWORD(WINAPI*)(LPCSTR,DWORD,LPSTR, \
        LPSTR*))aSyscall[23].pCurrent)

#if !SQLITE_OS_WINCE && !SQLITE_OS_WINRT && defined(SQLITE_WIN32_HAS_WIDE)
  { "GetFullPathNameW",        (SYSCALL)GetFullPathNameW,        0 },
#else
  { "GetFullPathNameW",        (SYSCALL)0,                       0 },
#endif

#define osGetFullPathNameW ((DWORD(WINAPI*)(LPCWSTR,DWORD,LPWSTR, \
        LPWSTR*))aSyscall[24].pCurrent)

  { "GetLastError",            (SYSCALL)GetLastError,            0 },

#define osGetLastError ((DWORD(WINAPI*)(VOID))aSyscall[25].pCurrent)

#if SQLITE_OS_WINCE
  /* The GetProcAddressA() routine is only available on Windows CE. */
  /*该GetProcAddressA（）函数是仅适用于Windows CE。 */
  { "GetProcAddressA",         (SYSCALL)GetProcAddressA,         0 },
#else
  /* All other Windows platforms expect GetProcAddress() to take
  ** an ANSI string regardless of the _UNICODE setting 
  **所有其他Windows平台预计GetProcAddress函数来使用
  **一个ANSI字符串不管_UNICODE设置*/
  { "GetProcAddressA",         (SYSCALL)GetProcAddress,          0 },
#endif

#define osGetProcAddressA ((FARPROC(WINAPI*)(HMODULE, \
        LPCSTR))aSyscall[26].pCurrent)

#if !SQLITE_OS_WINRT
  { "GetSystemInfo",           (SYSCALL)GetSystemInfo,           0 },
#else
  { "GetSystemInfo",           (SYSCALL)0,                       0 },
#endif

#define osGetSystemInfo ((VOID(WINAPI*)(LPSYSTEM_INFO))aSyscall[27].pCurrent)

  { "GetSystemTime",           (SYSCALL)GetSystemTime,           0 },

#define osGetSystemTime ((VOID(WINAPI*)(LPSYSTEMTIME))aSyscall[28].pCurrent)

#if !SQLITE_OS_WINCE
  { "GetSystemTimeAsFileTime", (SYSCALL)GetSystemTimeAsFileTime, 0 },
#else
  { "GetSystemTimeAsFileTime", (SYSCALL)0,                       0 },
#endif

#define osGetSystemTimeAsFileTime ((VOID(WINAPI*)( \
        LPFILETIME))aSyscall[29].pCurrent)

#if defined(SQLITE_WIN32_HAS_ANSI)
  { "GetTempPathA",            (SYSCALL)GetTempPathA,            0 },
#else
  { "GetTempPathA",            (SYSCALL)0,                       0 },
#endif

#define osGetTempPathA ((DWORD(WINAPI*)(DWORD,LPSTR))aSyscall[30].pCurrent)

#if !SQLITE_OS_WINRT && defined(SQLITE_WIN32_HAS_WIDE)
  { "GetTempPathW",            (SYSCALL)GetTempPathW,            0 },
#else
  { "GetTempPathW",            (SYSCALL)0,                       0 },
#endif

#define osGetTempPathW ((DWORD(WINAPI*)(DWORD,LPWSTR))aSyscall[31].pCurrent)

#if !SQLITE_OS_WINRT
  { "GetTickCount",            (SYSCALL)GetTickCount,            0 },
#else
  { "GetTickCount",            (SYSCALL)0,                       0 },
#endif

#define osGetTickCount ((DWORD(WINAPI*)(VOID))aSyscall[32].pCurrent)

#if defined(SQLITE_WIN32_HAS_ANSI)
  { "GetVersionExA",           (SYSCALL)GetVersionExA,           0 },
#else
  { "GetVersionExA",           (SYSCALL)0,                       0 },
#endif

#define osGetVersionExA ((BOOL(WINAPI*)( \
        LPOSVERSIONINFOA))aSyscall[33].pCurrent)

  { "HeapAlloc",               (SYSCALL)HeapAlloc,               0 },

#define osHeapAlloc ((LPVOID(WINAPI*)(HANDLE,DWORD, \
        SIZE_T))aSyscall[34].pCurrent)

#if !SQLITE_OS_WINRT
  { "HeapCreate",              (SYSCALL)HeapCreate,              0 },
#else
  { "HeapCreate",              (SYSCALL)0,                       0 },
#endif

#define osHeapCreate ((HANDLE(WINAPI*)(DWORD,SIZE_T, \
        SIZE_T))aSyscall[35].pCurrent)

#if !SQLITE_OS_WINRT
  { "HeapDestroy",             (SYSCALL)HeapDestroy,             0 },
#else
  { "HeapDestroy",             (SYSCALL)0,                       0 },
#endif

#define osHeapDestroy ((BOOL(WINAPI*)(HANDLE))aSyscall[36].pCurrent)

  { "HeapFree",                (SYSCALL)HeapFree,                0 },

#define osHeapFree ((BOOL(WINAPI*)(HANDLE,DWORD,LPVOID))aSyscall[37].pCurrent)

  { "HeapReAlloc",             (SYSCALL)HeapReAlloc,             0 },

#define osHeapReAlloc ((LPVOID(WINAPI*)(HANDLE,DWORD,LPVOID, \
        SIZE_T))aSyscall[38].pCurrent)

  { "HeapSize",                (SYSCALL)HeapSize,                0 },

#define osHeapSize ((SIZE_T(WINAPI*)(HANDLE,DWORD, \
        LPCVOID))aSyscall[39].pCurrent)

#if !SQLITE_OS_WINRT
  { "HeapValidate",            (SYSCALL)HeapValidate,            0 },
#else
  { "HeapValidate",            (SYSCALL)0,                       0 },
#endif

#define osHeapValidate ((BOOL(WINAPI*)(HANDLE,DWORD, \
        LPCVOID))aSyscall[40].pCurrent)

#if defined(SQLITE_WIN32_HAS_ANSI)
  { "LoadLibraryA",            (SYSCALL)LoadLibraryA,            0 },
#else
  { "LoadLibraryA",            (SYSCALL)0,                       0 },
#endif

#define osLoadLibraryA ((HMODULE(WINAPI*)(LPCSTR))aSyscall[41].pCurrent)

#if !SQLITE_OS_WINRT && defined(SQLITE_WIN32_HAS_WIDE)
  { "LoadLibraryW",            (SYSCALL)LoadLibraryW,            0 },
#else
  { "LoadLibraryW",            (SYSCALL)0,                       0 },
#endif

#define osLoadLibraryW ((HMODULE(WINAPI*)(LPCWSTR))aSyscall[42].pCurrent)

#if !SQLITE_OS_WINRT
  { "LocalFree",               (SYSCALL)LocalFree,               0 },
#else
  { "LocalFree",               (SYSCALL)0,                       0 },
#endif

#define osLocalFree ((HLOCAL(WINAPI*)(HLOCAL))aSyscall[43].pCurrent)

#if !SQLITE_OS_WINCE && !SQLITE_OS_WINRT
  { "LockFile",                (SYSCALL)LockFile,                0 },
#else
  { "LockFile",                (SYSCALL)0,                       0 },
#endif

#ifndef osLockFile
#define osLockFile ((BOOL(WINAPI*)(HANDLE,DWORD,DWORD,DWORD, \
        DWORD))aSyscall[44].pCurrent)
#endif

#if !SQLITE_OS_WINCE
  { "LockFileEx",              (SYSCALL)LockFileEx,              0 },
#else
  { "LockFileEx",              (SYSCALL)0,                       0 },
#endif

#ifndef osLockFileEx
#define osLockFileEx ((BOOL(WINAPI*)(HANDLE,DWORD,DWORD,DWORD,DWORD, \
        LPOVERLAPPED))aSyscall[45].pCurrent)
#endif

#if SQLITE_OS_WINCE || (!SQLITE_OS_WINRT && !defined(SQLITE_OMIT_WAL))
  { "MapViewOfFile",           (SYSCALL)MapViewOfFile,           0 },
#else
  { "MapViewOfFile",           (SYSCALL)0,                       0 },
#endif

#define osMapViewOfFile ((LPVOID(WINAPI*)(HANDLE,DWORD,DWORD,DWORD, \
        SIZE_T))aSyscall[46].pCurrent)

  { "MultiByteToWideChar",     (SYSCALL)MultiByteToWideChar,     0 },

#define osMultiByteToWideChar ((int(WINAPI*)(UINT,DWORD,LPCSTR,int,LPWSTR, \
        int))aSyscall[47].pCurrent)

  { "QueryPerformanceCounter", (SYSCALL)QueryPerformanceCounter, 0 },

#define osQueryPerformanceCounter ((BOOL(WINAPI*)( \
        LARGE_INTEGER*))aSyscall[48].pCurrent)

  { "ReadFile",                (SYSCALL)ReadFile,                0 },

#define osReadFile ((BOOL(WINAPI*)(HANDLE,LPVOID,DWORD,LPDWORD, \
        LPOVERLAPPED))aSyscall[49].pCurrent)

  { "SetEndOfFile",            (SYSCALL)SetEndOfFile,            0 },

#define osSetEndOfFile ((BOOL(WINAPI*)(HANDLE))aSyscall[50].pCurrent)

#if !SQLITE_OS_WINRT
  { "SetFilePointer",          (SYSCALL)SetFilePointer,          0 },
#else
  { "SetFilePointer",          (SYSCALL)0,                       0 },
#endif

#define osSetFilePointer ((DWORD(WINAPI*)(HANDLE,LONG,PLONG, \
        DWORD))aSyscall[51].pCurrent)

#if !SQLITE_OS_WINRT
  { "Sleep",                   (SYSCALL)Sleep,                   0 },
#else
  { "Sleep",                   (SYSCALL)0,                       0 },
#endif

#define osSleep ((VOID(WINAPI*)(DWORD))aSyscall[52].pCurrent)

  { "SystemTimeToFileTime",    (SYSCALL)SystemTimeToFileTime,    0 },

#define osSystemTimeToFileTime ((BOOL(WINAPI*)(CONST SYSTEMTIME*, \
        LPFILETIME))aSyscall[53].pCurrent)

#if !SQLITE_OS_WINCE && !SQLITE_OS_WINRT
  { "UnlockFile",              (SYSCALL)UnlockFile,              0 },
#else
  { "UnlockFile",              (SYSCALL)0,                       0 },
#endif

#ifndef osUnlockFile
#define osUnlockFile ((BOOL(WINAPI*)(HANDLE,DWORD,DWORD,DWORD, \
        DWORD))aSyscall[54].pCurrent)
#endif

#if !SQLITE_OS_WINCE
  { "UnlockFileEx",            (SYSCALL)UnlockFileEx,            0 },
#else
  { "UnlockFileEx",            (SYSCALL)0,                       0 },
#endif

#define osUnlockFileEx ((BOOL(WINAPI*)(HANDLE,DWORD,DWORD,DWORD, \
        LPOVERLAPPED))aSyscall[55].pCurrent)

#if SQLITE_OS_WINCE || !defined(SQLITE_OMIT_WAL)
  { "UnmapViewOfFile",         (SYSCALL)UnmapViewOfFile,         0 },
#else
  { "UnmapViewOfFile",         (SYSCALL)0,                       0 },
#endif

#define osUnmapViewOfFile ((BOOL(WINAPI*)(LPCVOID))aSyscall[56].pCurrent)

  { "WideCharToMultiByte",     (SYSCALL)WideCharToMultiByte,     0 },

#define osWideCharToMultiByte ((int(WINAPI*)(UINT,DWORD,LPCWSTR,int,LPSTR,int, \
        LPCSTR,LPBOOL))aSyscall[57].pCurrent)

  { "WriteFile",               (SYSCALL)WriteFile,               0 },

#define osWriteFile ((BOOL(WINAPI*)(HANDLE,LPCVOID,DWORD,LPDWORD, \
        LPOVERLAPPED))aSyscall[58].pCurrent)

#if SQLITE_OS_WINRT
  { "CreateEventExW",          (SYSCALL)CreateEventExW,          0 },
#else
  { "CreateEventExW",          (SYSCALL)0,                       0 },
#endif

#define osCreateEventExW ((HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES,LPCWSTR, \
        DWORD,DWORD))aSyscall[59].pCurrent)

#if !SQLITE_OS_WINRT
  { "WaitForSingleObject",     (SYSCALL)WaitForSingleObject,     0 },
#else
  { "WaitForSingleObject",     (SYSCALL)0,                       0 },
#endif

#define osWaitForSingleObject ((DWORD(WINAPI*)(HANDLE, \
        DWORD))aSyscall[60].pCurrent)

#if SQLITE_OS_WINRT
  { "WaitForSingleObjectEx",   (SYSCALL)WaitForSingleObjectEx,   0 },
#else
  { "WaitForSingleObjectEx",   (SYSCALL)0,                       0 },
#endif

#define osWaitForSingleObjectEx ((DWORD(WINAPI*)(HANDLE,DWORD, \
        BOOL))aSyscall[61].pCurrent)

#if SQLITE_OS_WINRT
  { "SetFilePointerEx",        (SYSCALL)SetFilePointerEx,        0 },
#else
  { "SetFilePointerEx",        (SYSCALL)0,                       0 },
#endif

#define osSetFilePointerEx ((BOOL(WINAPI*)(HANDLE,LARGE_INTEGER, \
        PLARGE_INTEGER,DWORD))aSyscall[62].pCurrent)

#if SQLITE_OS_WINRT
  { "GetFileInformationByHandleEx", (SYSCALL)GetFileInformationByHandleEx, 0 },
#else
  { "GetFileInformationByHandleEx", (SYSCALL)0,                  0 },
#endif

#define osGetFileInformationByHandleEx ((BOOL(WINAPI*)(HANDLE, \
        FILE_INFO_BY_HANDLE_CLASS,LPVOID,DWORD))aSyscall[63].pCurrent)

#if SQLITE_OS_WINRT && !defined(SQLITE_OMIT_WAL)
  { "MapViewOfFileFromApp",    (SYSCALL)MapViewOfFileFromApp,    0 },
#else
  { "MapViewOfFileFromApp",    (SYSCALL)0,                       0 },
#endif

#define osMapViewOfFileFromApp ((LPVOID(WINAPI*)(HANDLE,ULONG,ULONG64, \
        SIZE_T))aSyscall[64].pCurrent)

#if SQLITE_OS_WINRT
  { "CreateFile2",             (SYSCALL)CreateFile2,             0 },
#else
  { "CreateFile2",             (SYSCALL)0,                       0 },
#endif

#define osCreateFile2 ((HANDLE(WINAPI*)(LPCWSTR,DWORD,DWORD,DWORD, \
        LPCREATEFILE2_EXTENDED_PARAMETERS))aSyscall[65].pCurrent)

#if SQLITE_OS_WINRT
  { "LoadPackagedLibrary",     (SYSCALL)LoadPackagedLibrary,     0 },
#else
  { "LoadPackagedLibrary",     (SYSCALL)0,                       0 },
#endif

#define osLoadPackagedLibrary ((HMODULE(WINAPI*)(LPCWSTR, \
        DWORD))aSyscall[66].pCurrent)

#if SQLITE_OS_WINRT
  { "GetTickCount64",          (SYSCALL)GetTickCount64,          0 },
#else
  { "GetTickCount64",          (SYSCALL)0,                       0 },
#endif

#define osGetTickCount64 ((ULONGLONG(WINAPI*)(VOID))aSyscall[67].pCurrent)

#if SQLITE_OS_WINRT
  { "GetNativeSystemInfo",     (SYSCALL)GetNativeSystemInfo,     0 },
#else
  { "GetNativeSystemInfo",     (SYSCALL)0,                       0 },
#endif

#define osGetNativeSystemInfo ((VOID(WINAPI*)( \
        LPSYSTEM_INFO))aSyscall[68].pCurrent)

#if defined(SQLITE_WIN32_HAS_ANSI)
  { "OutputDebugStringA",      (SYSCALL)OutputDebugStringA,      0 },
#else
  { "OutputDebugStringA",      (SYSCALL)0,                       0 },
#endif

#define osOutputDebugStringA ((VOID(WINAPI*)(LPCSTR))aSyscall[69].pCurrent)

#if defined(SQLITE_WIN32_HAS_WIDE)
  { "OutputDebugStringW",      (SYSCALL)OutputDebugStringW,      0 },
#else
  { "OutputDebugStringW",      (SYSCALL)0,                       0 },
#endif

#define osOutputDebugStringW ((VOID(WINAPI*)(LPCWSTR))aSyscall[70].pCurrent)

  { "GetProcessHeap",          (SYSCALL)GetProcessHeap,          0 },

#define osGetProcessHeap ((HANDLE(WINAPI*)(VOID))aSyscall[71].pCurrent)

#if SQLITE_OS_WINRT && !defined(SQLITE_OMIT_WAL)
  { "CreateFileMappingFromApp", (SYSCALL)CreateFileMappingFromApp, 0 },
#else
  { "CreateFileMappingFromApp", (SYSCALL)0,                      0 },
#endif

#define osCreateFileMappingFromApp ((HANDLE(WINAPI*)(HANDLE, \
        LPSECURITY_ATTRIBUTES,ULONG,ULONG64,LPCWSTR))aSyscall[72].pCurrent)

}; /* End of the overrideable system calls 结束重载系统调用*/

/*
** This is the xSetSystemCall() method of sqlite3_vfs for all of the
** "win32" VFSes.  Return SQLITE_OK opon successfully updating the
** system call pointer, or SQLITE_NOTFOUND if there is no configurable
** system call named zName.
**这是对于所有"win32" VFSes的sqlite3_vfs的xSetSystemCall()方法
**返回SQLITE_OK opon成功更新系统调用指针
**或者这里SQLITE_NOTFOUND没有被叫做zName系统调用配置 
*/
static int winSetSystemCall(
  sqlite3_vfs *pNotUsed,        /* The VFS pointer.  Not used 该VFS指针。未使用*/
  const char *zName,            /* Name of system call to override 系统调用的名称覆盖*/
  sqlite3_syscall_ptr pNewFunc  /* Pointer to new system call value 指向新的系统调用值*/
){
  unsigned int i;
  int rc = SQLITE_NOTFOUND;

  UNUSED_PARAMETER(pNotUsed);
  if( zName==0 ){
    /* If no zName is given, restore all system calls to their default
    ** settings and return NULL
    *如果没有zName给出，恢复所有的系统调用为其默认设置和返回NULL
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
    * 如果zName是指定, 只在指定一个系统调用运行
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
**返回系统调用的数值。返回NULL如果zName不是
**系统可认识的调用名称。如果系统调用是当前
**没有定义的也返回NULL。 
*/
static sqlite3_syscall_ptr winGetSystemCall(
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
** then return the name of the first system call.  Return NULL if zName
** is the last system call or if zName is not the name of a valid
** system call.
** 返回zName后的第一个系统调用的名称。 如果zName==NULL
** 就返回第一个系统调用的名称。如果zName是最后一个的系统调用
**或者如果zName不是一个有效的系统调用名称也返回NULL。
*/
static const char *winNextSystemCall(sqlite3_vfs *p, const char *zName){
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
** This function outputs the specified (ANSI) string to the Win32 debugger
** (if available).
**此功能输出指定（ANSI）字符串到Win32的调试器
**（如果可用）。
*/

void sqlite3_win32_write_debug(char *zBuf, int nBuf){
  char zDbgBuf[SQLITE_WIN32_DBG_BUF_SIZE];
  int nMin = MIN(nBuf, (SQLITE_WIN32_DBG_BUF_SIZE - 1)); /* may be negative. 可能是负的。*/
  if( nMin<-1 ) nMin = -1; /* all negative values become -1. 所有的负值变为-1。*/
  assert( nMin==-1 || nMin==0 || nMin<SQLITE_WIN32_DBG_BUF_SIZE );
#if defined(SQLITE_WIN32_HAS_ANSI)
  if( nMin>0 ){
    memset(zDbgBuf, 0, SQLITE_WIN32_DBG_BUF_SIZE);
    memcpy(zDbgBuf, zBuf, nMin);
    osOutputDebugStringA(zDbgBuf);
  }else{
    osOutputDebugStringA(zBuf);
  }
#elif defined(SQLITE_WIN32_HAS_WIDE)
  memset(zDbgBuf, 0, SQLITE_WIN32_DBG_BUF_SIZE);
  if ( osMultiByteToWideChar(
          osAreFileApisANSI() ? CP_ACP : CP_OEMCP, 0, zBuf,
          nMin, (LPWSTR)zDbgBuf, SQLITE_WIN32_DBG_BUF_SIZE/sizeof(WCHAR))<=0 ){
    return;
  }
  osOutputDebugStringW((LPCWSTR)zDbgBuf);
#else
  if( nMin>0 ){
    memset(zDbgBuf, 0, SQLITE_WIN32_DBG_BUF_SIZE);
    memcpy(zDbgBuf, zBuf, nMin);
    fprintf(stderr, "%s", zDbgBuf);
  }else{
    fprintf(stderr, "%s", zBuf);
  }
#endif
}

/*
** The following routine suspends the current thread for at least ms
** milliseconds.  This is equivalent to the Win32 Sleep() interface.
**以下常规暂停当前线程，至少毫秒。 
**这相当于Win32的Sleep()接口
*/
#if SQLITE_OS_WINRT
static HANDLE sleepObj = NULL;
#endif

void sqlite3_win32_sleep(DWORD milliseconds){
#if SQLITE_OS_WINRT
  if ( sleepObj==NULL ){
    sleepObj = osCreateEventExW(NULL, NULL, CREATE_EVENT_MANUAL_RESET,
                                SYNCHRONIZE);
  }
  assert( sleepObj!=NULL );
  osWaitForSingleObjectEx(sleepObj, milliseconds, FALSE);
#else
  osSleep(milliseconds);
#endif
}

/*
** Return true (non-zero) if we are running under WinNT, Win2K, WinXP,
** or WinCE.  Return false (zero) for Win95, Win98, or WinME.
**
** Here is an interesting observation:  Win95, Win98, and WinME lack
** the LockFileEx() API.  But we can still statically link against that
** API as long as we don't call it when running Win95/98/ME.  A call to
** this routine is used to determine if the host is Win95/98/ME or
** WinNT/2K/XP so that we will know whether or not we can safely call
** the LockFileEx() API.
**返回true（不为零），如果我们在WinNT，Win2K，WinXP或者WinCE下运行的.
**返回false（0）在Win95，Win98下，WinME下运行。
**
**这里有一个有趣的现象：Win95，Win98和WinME的平台下缺乏LockFileEx()的API。
**但是，我们仍然可以使用静态链接替代 API，只要我们运行WIN95 /98/ME的时候
**不调用它。一个常规调用则被用来判断是否主机是Win95/ 98 / ME或WinNT/2K/ XP
**以便于我们能知道我们是否可以安全地调用LockFileEx()的API。
*/
#if SQLITE_OS_WINCE || SQLITE_OS_WINRT
# define isNT()  (1)
#else
  static int isNT(void){
    if( sqlite3_os_type==0 ){
      OSVERSIONINFOA sInfo;
      sInfo.dwOSVersionInfoSize = sizeof(sInfo);
      osGetVersionExA(&sInfo);
      sqlite3_os_type = sInfo.dwPlatformId==VER_PLATFORM_WIN32_NT ? 2 : 1;
    }
    return sqlite3_os_type==2;
  }
#endif /* SQLITE_OS_WINCE */

#ifdef SQLITE_WIN32_MALLOC
/*
** Allocate nBytes of memory.分配内存中的nBytes 
*/
static void *winMemMalloc(int nBytes){
  HANDLE hHeap;
  void *p;

  winMemAssertMagic();
  hHeap = winMemGetHeap();
  assert( hHeap!=0 );
  assert( hHeap!=INVALID_HANDLE_VALUE );
#if !SQLITE_OS_WINRT && defined(SQLITE_WIN32_MALLOC_VALIDATE)
  assert ( osHeapValidate(hHeap, SQLITE_WIN32_HEAP_FLAGS, NULL) );
#endif
  assert( nBytes>=0 );
  p = osHeapAlloc(hHeap, SQLITE_WIN32_HEAP_FLAGS, (SIZE_T)nBytes);
  if( !p ){
    sqlite3_log(SQLITE_NOMEM, "failed to HeapAlloc %u bytes (%d), heap=%p",
                nBytes, osGetLastError(), (void*)hHeap);
  }
  return p;
}

/*
** Free memory.释放内存。
*/
static void winMemFree(void *pPrior){
  HANDLE hHeap;

  winMemAssertMagic();
  hHeap = winMemGetHeap();
  assert( hHeap!=0 );
  assert( hHeap!=INVALID_HANDLE_VALUE );
#if !SQLITE_OS_WINRT && defined(SQLITE_WIN32_MALLOC_VALIDATE)
  assert ( osHeapValidate(hHeap, SQLITE_WIN32_HEAP_FLAGS, pPrior) );
#endif
  if( !pPrior ) return; /* Passing NULL to HeapFree is undefined. 传递NULL到HeapFree是未定义的。*/
  if( !osHeapFree(hHeap, SQLITE_WIN32_HEAP_FLAGS, pPrior) ){
    sqlite3_log(SQLITE_NOMEM, "failed to HeapFree block %p (%d), heap=%p",
                pPrior, osGetLastError(), (void*)hHeap);
  }
}

/*
** Change the size of an existing memory allocation 更改现有的内存分配的大小
*/
static void *winMemRealloc(void *pPrior, int nBytes){
  HANDLE hHeap;
  void *p;

  winMemAssertMagic();
  hHeap = winMemGetHeap();
  assert( hHeap!=0 );
  assert( hHeap!=INVALID_HANDLE_VALUE );
#if !SQLITE_OS_WINRT && defined(SQLITE_WIN32_MALLOC_VALIDATE)
  assert ( osHeapValidate(hHeap, SQLITE_WIN32_HEAP_FLAGS, pPrior) );
#endif
  assert( nBytes>=0 );
  if( !pPrior ){
    p = osHeapAlloc(hHeap, SQLITE_WIN32_HEAP_FLAGS, (SIZE_T)nBytes);
  }else{
    p = osHeapReAlloc(hHeap, SQLITE_WIN32_HEAP_FLAGS, pPrior, (SIZE_T)nBytes);
  }
  if( !p ){
    sqlite3_log(SQLITE_NOMEM, "failed to %s %u bytes (%d), heap=%p",
                pPrior ? "HeapReAlloc" : "HeapAlloc", nBytes, osGetLastError(),
                (void*)hHeap);
  }
  return p;
}

/*
** Return the size of an outstanding allocation, in bytes.
**返回一个未完成的分配大小，以字节为单位
*/
static int winMemSize(void *p){
  HANDLE hHeap;
  SIZE_T n;

  winMemAssertMagic();
  hHeap = winMemGetHeap();
  assert( hHeap!=0 );
  assert( hHeap!=INVALID_HANDLE_VALUE );
#if !SQLITE_OS_WINRT && defined(SQLITE_WIN32_MALLOC_VALIDATE)
  assert ( osHeapValidate(hHeap, SQLITE_WIN32_HEAP_FLAGS, NULL) );
#endif
  if( !p ) return 0;
  n = osHeapSize(hHeap, SQLITE_WIN32_HEAP_FLAGS, p);
  if( n==(SIZE_T)-1 ){
    sqlite3_log(SQLITE_NOMEM, "failed to HeapSize block %p (%d), heap=%p",
                p, osGetLastError(), (void*)hHeap);
    return 0;
  }
  return (int)n;
}

/*
** Round up a request size to the next valid allocation size.
**集中一个请求大小到下一个有效的分配大小。
*/
static int winMemRoundup(int n){
  return n;
}

/*
** Initialize this module./*模块的初始*/ 
*/
static int winMemInit(void *pAppData){/*定义一个静态整型的内存初始化函数，变量为指针型，没有返回值*/
  winMemData *pWinMemData = (winMemData *)pAppData;/*定义一个winMemInit型的指针变量pWinMemDat*/

  if( !pWinMemData ) return SQLITE_ERROR;/*如果pWinMemData的值不存在，就返回SQLITE_ERROR这样的提示/
  assert( pWinMemData->magic==WINMEM_MAGIC );/*把这个指针变量指向magic，用WINMEM_MAGIC进行赋值
*/

#if !SQLITE_OS_WINRT && SQLITE_WIN32_HEAP_CREATE/*如果有定义SQlite中操作系统不是winrt种跨平台应用程序架构，并且没有win32堆被创建时，就执行以下代码*/
  if( !pWinMemData->hHeap ){
    pWinMemData->hHeap = osHeapCreate(SQLITE_WIN32_HEAP_FLAGS,/*堆标志*/
                                      SQLITE_WIN32_HEAP_INIT_SIZE,/*堆初始化大小*/ 
                                      SQLITE_WIN32_HEAP_MAX_SIZE);/*堆最大的容量*/ 
    if( !pWinMemData->hHeap ){
      sqlite3_log(SQLITE_NOMEM,
          "failed to HeapCreate (%d), flags=%u, initSize=%u, maxSize=%u",
          osGetLastError(), SQLITE_WIN32_HEAP_FLAGS,
          SQLITE_WIN32_HEAP_INIT_SIZE, SQLITE_WIN32_HEAP_MAX_SIZE);
      return SQLITE_NOMEM;
    }/*对对应的堆标志，大小，和最大容量进行赋值，返回SQLITE_NOME*/
    pWinMemData->bOwned = TRUE;/*否则这个指针就指向bOwned，并赋值为真*/
    assert( pWinMemData->bOwned );/*把这个指针变量指向bOwne*/
  }
#else
  pWinMemData->hHeap = osGetProcessHeap();/*否则如果pWinMemData指向的hHeap是系统获取的进程中的堆值时，就执行以下代码*/
  if( !pWinMemData->hHeap ){
    sqlite3_log(SQLITE_NOMEM,
        "failed to GetProcessHeap (%d)", osGetLastError());
    return SQLITE_NOMEM;
  }
  pWinMemData->bOwned = FALSE;
  assert( !pWinMemData->bOwned );
#endif
  assert( pWinMemData->hHeap!=0 );
  assert( pWinMemData->hHeap!=INVALID_HANDLE_VALUE );/*将一个无效的句柄值赋值给hHeap*/
#if !SQLITE_OS_WINRT && defined(SQLITE_WIN32_MALLOC_VALIDATE)/*如果有定义SQlite中操作系统不是winrt种跨平台应用程序架构,
或者定义win32 使用合法化*/
  assert( osHeapValidate(pWinMemData->hHeap, SQLITE_WIN32_HEAP_FLAGS, NULL) );/*设置操作系统堆合法化的一系列变量*/ 
#endif
  return SQLITE_OK;
}

/*
** Deinitialize this module/*取消初始设置代码块*/ 
*/
static void winMemShutdown(void *pAppData)/*定义一个内存关闭的静态空函数*/{
  winMemData *pWinMemData = (winMemData *)pAppData;

  if( !pWinMemData ) return;
  if( pWinMemData->hHeap ){
    assert( pWinMemData->hHeap!=INVALID_HANDLE_VALUE );
#if !SQLITE_OS_WINRT && defined(SQLITE_WIN32_MALLOC_VALIDATE)/*如果有定义SQlite中操作系统不是winrt种跨平台应用程序架构,
或者定义win32 使用合法化*/
    assert( osHeapValidate(pWinMemData->hHeap, SQLITE_WIN32_HEAP_FLAGS, NULL) );
#endif
    if( pWinMemData->bOwned ){
      if( !osHeapDestroy(pWinMemData->hHeap) ){
        sqlite3_log(SQLITE_NOMEM, "failed to HeapDestroy (%d), heap=%p",
                    osGetLastError(), (void*)pWinMemData->hHeap);
      }
      pWinMemData->bOwned = FALSE;
    }
    pWinMemData->hHeap = NULL;
  }
}

/*
** Populate the low-level memory allocation function pointers in
** sqlite3GlobalConfig.m with pointers to the routines in this file. The
** arguments specify the block of memory to manage.
***//*在低级别的内存中分配函数指针，.m指针的程序文件指向这个文件的例行程序，参数指定内存管理*/
** This routine is only called by sqlite3_config(), and therefore
** is not required to be threadsafe (it is not).
*/
/*这个例行程序仅被sqlite3_config()这个函数使用，因此不需要线程安全*/ 
const sqlite3_mem_methods *sqlite3MemGetWin32(void){
  static const sqlite3_mem_methods winMemMethods = {
    winMemMalloc,
    winMemFree,
    winMemRealloc,
    winMemSize,
    winMemRoundup,
    winMemInit,
    winMemShutdown,
    &win_mem_data
  };
  return &winMemMethods;
}

void sqlite3MemSetDefault(void){
  sqlite3_config(SQLITE_CONFIG_MALLOC, sqlite3MemGetWin32());
}
#endif /* SQLITE_WIN32_MALLOC *//*结束win32地址分配*/ 

/*
** Convert a UTF-8 string to Microsoft Unicode (UTF-16?). 
**//*转换到微软Unicode（UTF-8字符串微软UTF-16)*/
** Space to hold the returned string is obtained from malloc.
*//*空间来保存返回的字符串是由malloc来接收*/
*/ 
static LPWSTR utf8ToUnicode(const char *zFilename){
  int nChar;
  LPWSTR zWideFilename;

  nChar = osMultiByteToWideChar(CP_UTF8, 0, zFilename, -1, NULL, 0);
  if( nChar==0 ){
    return 0;
  }
  zWideFilename = sqlite3_malloc( nChar*sizeof(zWideFilename[0]) );
  if( zWideFilename==0 ){
    return 0;
  }
  nChar = osMultiByteToWideChar(CP_UTF8, 0, zFilename, -1, zWideFilename,
                                nChar);
  if( nChar==0 ){
    sqlite3_free(zWideFilename);
    zWideFilename = 0;
  }
  return zWideFilename;
}

/*
** Convert Microsoft Unicode to UTF-8.  Space to hold the returned string is
** obtained from sqlite3_malloc().
*//*转换到微软Unicode UTF-8。保存返回的字符串是来自从sqlite3_malloc()*/
static char *unicodeToUtf8(LPCWSTR zWideFilename){
  int nByte;
  char *zFilename;

  nByte = osWideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, 0, 0, 0, 0);
  if( nByte == 0 ){
    return 0;
  }
  zFilename = sqlite3_malloc( nByte );
  if( zFilename==0 ){
    return 0;
  }
  nByte = osWideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, zFilename, nByte,
                                0, 0);
  if( nByte == 0 ){
    sqlite3_free(zFilename);
    zFilename = 0;
  }
  return zFilename;
}

/*
** Convert an ANSI string to Microsoft Unicode, based on the
** current codepage settings for file apis.
** /*将ANSI字符串转换到微软的Unicode，基于当前代码页设置文件的API*/
** Space to hold the returned string is obtained
** from sqlite3_malloc.
*//*得到的空间来容纳返回的字符串sqlite3_malloc*/
static LPWSTR mbcsToUnicode(const char *zFilename){
  int nByte;
  LPWSTR zMbcsFilename;
  int codepage = osAreFileApisANSI() ? CP_ACP : CP_OEMCP;

  nByte = osMultiByteToWideChar(codepage, 0, zFilename, -1, NULL,
                                0)*sizeof(WCHAR);
  if( nByte==0 ){
    return 0;
  }
  zMbcsFilename = sqlite3_malloc( nByte*sizeof(zMbcsFilename[0]) );
  if( zMbcsFilename==0 ){
    return 0;
  }
  nByte = osMultiByteToWideChar(codepage, 0, zFilename, -1, zMbcsFilename,
                                nByte);
  if( nByte==0 ){
    sqlite3_free(zMbcsFilename);
    zMbcsFilename = 0;
  }
  return zMbcsFilename;
}

/*
** Convert Microsoft Unicode to multi-byte character string, based on the
** user's ANSI codepage.
**//*微软Unicode转换成多字节字符的字符串，基于用户的codepage ANSi*/ 
** Space to hold the returned string is obtained from
** sqlite3_malloc().
*//*所开辟的空间来保存返回的字符串，该字符串源自于fromsqlite3_malloc()*/
static char *unicodeToMbcs(LPCWSTR zWideFilename){
  int nByte;
  char *zFilename;
  int codepage = osAreFileApisANSI() ? CP_ACP : CP_OEMCP;

  nByte = osWideCharToMultiByte(codepage, 0, zWideFilename, -1, 0, 0, 0, 0);
  if( nByte == 0 ){
    return 0;
  }
  zFilename = sqlite3_malloc( nByte );
  if( zFilename==0 ){
    return 0;
  }
  nByte = osWideCharToMultiByte(codepage, 0, zWideFilename, -1, zFilename,
                                nByte, 0, 0);
  if( nByte == 0 ){
    sqlite3_free(zFilename);
    zFilename = 0;
  }
  return zFilename;
}

/*
** Convert multibyte character string to UTF-8.  Space to hold the
** returned string is obtained from sqlite3_malloc().
*//*将多字节字符的字符串为UTF-8,空间来保存返回的字符串是源自于fromsqlite3_malloc()*/ 
char *sqlite3_win32_mbcs_to_utf8(const char *zFilename){
  char *zFilenameUtf8;
  LPWSTR zTmpWide;

  zTmpWide = mbcsToUnicode(zFilename);
  if( zTmpWide==0 ){
    return 0;
  }
  zFilenameUtf8 = unicodeToUtf8(zTmpWide);
  sqlite3_free(zTmpWide);
  return zFilenameUtf8;
}

/*
** Convert UTF-8 to multibyte character string.  Space to hold the 
** returned string is obtained from sqlite3_malloc().
*//*转换UTF-8多字节字符,空间来保存返回的字符串是源自于fromsqlite3_malloc()*/
char *sqlite3_win32_utf8_to_mbcs(const char *zFilename){
  char *zFilenameMbcs;
  LPWSTR zTmpWide;

  zTmpWide = utf8ToUnicode(zFilename);
  if( zTmpWide==0 ){
    return 0;
  }
  zFilenameMbcs = unicodeToMbcs(zTmpWide);
  sqlite3_free(zTmpWide);
  return zFilenameMbcs;
}

/*
** This function sets the data directory or the temporary directory based on
** the provided arguments.  The type argument must be 1 in order to set the
** data directory or 2 in order to set the temporary directory.  The zValue
** argument is the name of the directory to use.  The return value will be
** SQLITE_OK if successful.
*//*这个函数设置数据目录，或根据所提供的参数设置临时目录，类型参数必须以1组数据目录或2为了设置临时目录
该zvalueargument是使用目录的名称。如果成功,返回值将是sqlite_ok*/
int sqlite3_win32_set_directory(DWORD type, LPCWSTR zValue){
  char **ppDirectory = 0;
#ifndef SQLITE_OMIT_AUTOINIT
  int rc = sqlite3_initialize();
  if( rc ) return rc;
#endif
  if( type==SQLITE_WIN32_DATA_DIRECTORY_TYPE ){
    ppDirectory = &sqlite3_data_directory;
  }else if( type==SQLITE_WIN32_TEMP_DIRECTORY_TYPE ){
    ppDirectory = &sqlite3_temp_directory;
  }
  assert( !ppDirectory || type==SQLITE_WIN32_DATA_DIRECTORY_TYPE
          || type==SQLITE_WIN32_TEMP_DIRECTORY_TYPE
  );
  assert( !ppDirectory || sqlite3MemdebugHasType(*ppDirectory, MEMTYPE_HEAP) );
  if( ppDirectory ){
    char *zValueUtf8 = 0;
    if( zValue && zValue[0] ){
      zValueUtf8 = unicodeToUtf8(zValue);
      if ( zValueUtf8==0 ){
        return SQLITE_NOMEM;
      }
    }
    sqlite3_free(*ppDirectory);
    *ppDirectory = zValueUtf8;
    return SQLITE_OK;
  }
  return SQLITE_ERROR;
}

/*
** The return value of getLastErrorMsg
** is zero if the error message fits in the buffer, or non-zero
** otherwise (if the message was truncated).
*/ /*getlasterrormsg的返回值为零如果错误消息符合缓冲区或非零，否则（如果消息被截断）*/
static int getLastErrorMsg(DWORD lastErrno, int nBuf, char *zBuf){
  /* FormatMessage returns 0 on failure.  Otherwise it
  ** returns the number of TCHARs written to the output
  ** buffer, excluding the terminating null char.
  *//*FormatMessage返回0，失败。否则，它返回写入输出缓冲集数，不包括空结束字符串。*/
  DWORD dwLen = 0;
  char *zOut = 0;

  if( isNT() ){
#if SQLITE_OS_WINRT
    WCHAR zTempWide[MAX_PATH+1]; /* NOTE: Somewhat arbitrary.注意：稍微有点武断 */
    dwLen = osFormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM |
                             FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL,
                             lastErrno,
                             0,
                             zTempWide,
                             MAX_PATH,
                             0);
#else
    LPWSTR zTempWide = NULL;
    dwLen = osFormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                             FORMAT_MESSAGE_FROM_SYSTEM |
                             FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL,
                             lastErrno,
                             0,
                             (LPWSTR) &zTempWide,
                             0,
                             0);
#endif
    if( dwLen > 0 ){
      /* allocate a buffer and convert to UTF8 *//*分配一个缓冲区并转换为UTF8*/
      sqlite3BeginBenignMalloc();
      zOut = unicodeToUtf8(zTempWide);
      sqlite3EndBenignMalloc();
#if !SQLITE_OS_WINRT
      /* free the system buffer allocated by FormatMessage *//*免费分配的格式化信息系统的缓冲区*/
      osLocalFree(zTempWide);
#endif
    }
  }
#ifdef SQLITE_WIN32_HAS_ANSI
  else{
    char *zTemp = NULL;
    dwLen = osFormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                             FORMAT_MESSAGE_FROM_SYSTEM |
                             FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL,
                             lastErrno,
                             0,
                             (LPSTR) &zTemp,
                             0,
                             0);
    if( dwLen > 0 ){
      /* allocate a buffer and convert to UTF8 *//*分配一个缓冲区并转换为UTF8*/
      sqlite3BeginBenignMalloc();
      zOut = sqlite3_win32_mbcs_to_utf8(zTemp);
      sqlite3EndBenignMalloc();
      /* free the system buffer allocated by FormatMessage *//*免费分配的格式化信息系统的缓冲区*/
      osLocalFree(zTemp);
    }
  }
#endif
  if( 0 == dwLen ){
    sqlite3_snprintf(nBuf, zBuf, "OsError 0x%x (%u)", lastErrno, lastErrno);
  }else{
    /* copy a maximum of nBuf chars to output buffer *//*复制一个最大的nbuf字符输出缓冲区*/
    sqlite3_snprintf(nBuf, zBuf, "%s", zOut);
    /* free the UTF8 buffer *//*the UTF8无缓冲*/
    sqlite3_free(zOut);
  }
  return 0;
}

/*
**
** This function - winLogErrorAtLine() - is only ever called via the macro
** winLogError()这个函数winlogerroratline() -曾经是称为通过宏观winlogerror()。
**
** This routine is invoked after an error occurs in an OS function.
** It logs a message using sqlite3_log() containing the current value of
** error code and, if possible, the human-readable equivalent from 
** FormatMessage.这个程序调用操作系统函数发生错误后。这一消息日志使用含有错误代码的当前值sqlite3_log()
如果可能的话，从FormatMessage人类可读的等效。
**
** The first argument passed to the macro should be the error code that
** will be returned to SQLite (e.g. SQLITE_IOERR_DELETE, SQLITE_CANTOPEN). 
** The two subsequent arguments should be the name of the OS function that
** failed and the associated file-system path, if any.第一个参数传递给宏应该是错误代码，
将返回到SQLite（例如sqlite_ioerr_delete，sqlite_cantopen）。
随后的两个参数应该是操作系统功能的失败和相关的文件系统路径的名称，如。
*/
#define winLogError(a,b,c,d)   winLogErrorAtLine(a,b,c,d,__LINE__)
static int winLogErrorAtLine(
  int errcode,                    /* SQLite error code *//*SQLite的错误代码*/
  DWORD lastErrno,                /* Win32 last error *//*Win32最后错误*/
  const char *zFunc,              /* Name of OS function that failed *//*名称操作系统功能的失败*/
  const char *zPath,              /* File path associated with error *//*文件路径相关的错误*/ 
  int iLine                       /* Source line number where error occurred *//*发生错误的代码行数*/
){
  char zMsg[500];                 /* Human readable error text *//*readable错误文本*/
  int i;                          /* Loop counter *//*循环计数器*/

  zMsg[0] = 0;
  getLastErrorMsg(lastErrno, sizeof(zMsg), zMsg);
  assert( errcode!=SQLITE_OK );
  if( zPath==0 ) zPath = "";
  for(i=0; zMsg[i] && zMsg[i]!='\r' && zMsg[i]!='\n'; i++){}
  zMsg[i] = 0;
  sqlite3_log(errcode,
      "os_win.c:%d: (%d) %s(%s) - %s",
      iLine, lastErrno, zFunc, zPath, zMsg
  );

  return errcode;
}

/*
** The number of times that a ReadFile(), WriteFile(), and DeleteFile()
** will be retried following a locking error - probably caused by 
** antivirus software.  Also the initial delay before the first retry.
** The delay increases linearly with each retry.
*//*readfile()，writefile()，和deletefile()将重试后锁定错误可能造成的杀毒软件次数。
同时初始延迟在第一次重试延迟后随着每次重试的增加而线性增加。
*/
#ifndef SQLITE_WIN32_IOERR_RETRY
# define SQLITE_WIN32_IOERR_RETRY 10
#endif
#ifndef SQLITE_WIN32_IOERR_RETRY_DELAY
# define SQLITE_WIN32_IOERR_RETRY_DELAY 25
#endif
static int win32IoerrRetry = SQLITE_WIN32_IOERR_RETRY;
static int win32IoerrRetryDelay = SQLITE_WIN32_IOERR_RETRY_DELAY;

/*
** If a ReadFile() or WriteFile() error occurs, invoke this routine
** to see if it should be retried.  Return TRUE to retry.  Return FALSE
** to give up with an error.
*//*如果一个readfile()或writefile()错误发生时，调用这个例程看是否应该重试。
重试,返回true，错误,返回false*/ 
static int retryIoerr(int *pnRetry, DWORD *pError){
  DWORD e = osGetLastError();
  if( *pnRetry>=win32IoerrRetry ){
    if( pError ){
      *pError = e;
    }
    return 0;
  }
  if( e==ERROR_ACCESS_DENIED ||
      e==ERROR_LOCK_VIOLATION ||
      e==ERROR_SHARING_VIOLATION ){
    sqlite3_win32_sleep(win32IoerrRetryDelay*(1+*pnRetry));
    ++*pnRetry;
    return 1;
  }
  if( pError ){
    *pError = e;
  }
  return 0;
}

/*
** Log a I/O error retry episode.
*//*日志I/O错误重试集*/
static void logIoerr(int nRetry){
  if( nRetry ){
    sqlite3_log(SQLITE_IOERR, 
      "delayed %dms for lock/sharing conflict",
      win32IoerrRetryDelay*nRetry*(nRetry+1)/2
    );
  }
}

#if SQLITE_OS_WINCE
/*************************************************************************
** This section contains code for WinCE only.
*//*本节包含的代码只用于WinCE*/
/*
** Windows CE does not have a localtime() function.  So create a
** substitute.
*//*Windows CE没有localtime()功能。所以建立一个替代的*/
#include <time.h>
struct tm *__cdecl localtime(const time_t *t)
{
  static struct tm y;
  FILETIME uTm, lTm;
  SYSTEMTIME pTm;
  sqlite3_int64 t64;
  t64 = *t;
  t64 = (t64 + 11644473600)*10000000;
  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);
  uTm.dwHighDateTime= (DWORD)(t64 >> 32);
  osFileTimeToLocalFileTime(&uTm,&lTm);
  osFileTimeToSystemTime(&lTm,&pTm);
  y.tm_year = pTm.wYear - 1900;
  y.tm_mon = pTm.wMonth - 1;
  y.tm_wday = pTm.wDayOfWeek;
  y.tm_mday = pTm.wDay;
  y.tm_hour = pTm.wHour;
  y.tm_min = pTm.wMinute;
  y.tm_sec = pTm.wSecond;
  return &y;
}

#define HANDLE_TO_WINFILE(a) (winFile*)&((char*)a)[-(int)offsetof(winFile,h)]

/*
** Acquire a lock on the handle h
*//*获取对处理H锁*/
static void winceMutexAcquire(HANDLE h){
   DWORD dwErr;
   do {
     dwErr = osWaitForSingleObject(h, INFINITE);
   } while (dwErr != WAIT_OBJECT_0 && dwErr != WAIT_ABANDONED);
}
/*
** Release a lock acquired by winceMutexAcquire()
*//*通过wincemutexacquire()释放锁*/
#define winceMutexRelease(h) ReleaseMutex(h)

/*
** Create the mutex and shared memory used for locking in the file
** descriptor pFile
*//*创建互斥体和用于锁定的文件描述符pfile共享内存*/
static BOOL winceCreateLock(const char *zFilename, winFile *pFile){
  LPWSTR zTok;
  LPWSTR zName;
  BOOL bInit = TRUE;

  zName = utf8ToUnicode(zFilename);
  if( zName==0 ){
    /* out of memory *//*内存不足*/
    return FALSE;
  }

  /* Initialize the local lockdata *//*初始化的局部lockdata*/
  memset(&pFile->local, 0, sizeof(pFile->local));

  /* Replace the backslashes from the filename and lowercase it
  ** to derive a mutex name. *//*从文件名替换反斜线和小写得出一个互斥体的名称。*/
  zTok = osCharLowerW(zName);
  for (;*zTok;zTok++){
    if (*zTok == '\\') *zTok = '_';
  }

  /* Create/open the named mutex *//*创建/打开命名互斥*/
  pFile->hMutex = osCreateMutexW(NULL, FALSE, zName);
  if (!pFile->hMutex){
    pFile->lastErrno = osGetLastError();
    winLogError(SQLITE_ERROR, pFile->lastErrno, "winceCreateLock1", zFilename);
    sqlite3_free(zName);
    return FALSE;
  }

  /* Acquire the mutex before continuing *//*在继续之前获得互斥体*/
  winceMutexAcquire(pFile->hMutex);
  
  /* Since the names of named mutexes, semaphores, file mappings etc are 
  ** case-sensitive, take advantage of that by uppercasing the mutex name
  ** and using that as the shared filemapping name.
  *//*因为命名为互斥信号量，文件映射等的名称是区分大小写的，
  利用这一点通过uppercasing互斥名称和使用作为共享文件映射名称*/
  osCharUpperW(zName);
  pFile->hShared = osCreateFileMappingW(INVALID_HANDLE_VALUE, NULL,
                                        PAGE_READWRITE, 0, sizeof(winceLock),
                                        zName);  

  /* Set a /*flag that indicates we're the first to create the memory so it 
  ** must be zero-initialized *//*设置一个标志，表明我们第一个创建内存必须初始化为零的*/
  if (osGetLastError() == ERROR_ALREADY_EXISTS){
    bInit = FALSE;
  }

  sqlite3_free(zName);

  /* If we /*succeeded in making the shared memory handle, map it. *//*如果我们成功地使共享内存句柄，来映射它*/ 
  if (pFile->hShared){
    pFile->shared = (winceLock*)osMapViewOfFile(pFile->hShared, 
             FILE_MAP_READ|FILE_MAP_WRITE, 0, 0, sizeof(winceLock));
    /* If /*mapping failed, close the shared memory handle and erase it *//*如果映射失败，关闭共享内存句柄和清除它*/
    if (!pFile->shared){
      pFile->lastErrno = osGetLastError();
      winLogError(SQLITE_ERROR, pFile->lastErrno,
               "winceCreateLock2", zFilename);
      osCloseHandle(pFile->hShared);
      pFile->hShared = NULL;
    }
  }

  /* If shared memory could not be created, then close the mutex and fail *//*如果无法创建共享内存，然后关闭互斥，就失败*/
  if (pFile->hShared == NULL){
    winceMutexRelease(pFile->hMutex);
    osCloseHandle(pFile->hMutex);
    pFile->hMutex = NULL;
    return FALSE;
  }
  
  /* Initialize the shared memory if we're supposed to *//*如果我们假设可以共享内存*/ 
  if (bInit) {
    memset(pFile->shared, 0, sizeof(winceLock));
  }

  winceMutexRelease(pFile->hMutex);
  return TRUE;
}

/*
** Destroy the part of winFile that deals with wince locks/*破坏winfile，WinCE锁交易的一部分*/
*/
static void winceDestroyLock(winFile *pFile){
  if (pFile->hMutex){
    /* Acquire the mutex *//*获取互斥体*/
    winceMutexAcquire(pFile->hMutex);

    /* The following blocks should probably assert in debug mode, but they
       are to cleanup in case any locks remained open *//*下面的模块应该嵌入到调试模式，但他们
会被清除在任何情况下锁打开*/
    if (pFile->local.nReaders){
      pFile->shared->nReaders --;
    }
    if (pFile->local.bReserved){
      pFile->shared->bReserved = FALSE;
    }
    if (pFile->local.bPending){
      pFile->shared->bPending = FALSE;
    }
    if (pFile->local.bExclusive){
      pFile->shared->bExclusive = FALSE;
    }

    /* De-reference and close our copy of the shared memory handle *//*取消引用关闭了我们的共享内存的句柄复制*/ 
    osUnmapViewOfFile(pFile->shared);
    osCloseHandle(pFile->hShared);

    /* Done with the mutex *//*用互斥*/
    winceMutexRelease(pFile->hMutex);    
    osCloseHandle(pFile->hMutex);
    pFile->hMutex = NULL;
  }
}

/* 
** An implementation of the LockFile() API of Windows for CE
*//*Windows CE API的lockfile()实现*/
static BOOL winceLockFile(
  LPHANDLE phFile,
  DWORD dwFileOffsetLow,
  DWORD dwFileOffsetHigh,
  DWORD nNumberOfBytesToLockLow,
  DWORD nNumberOfBytesToLockHigh
){
  winFile *pFile = HANDLE_TO_WINFILE(phFile);
  BOOL bReturn = FALSE;

  UNUSED_PARAMETER(dwFileOffsetHigh);
  UNUSED_PARAMETER(nNumberOfBytesToLockHigh);

  if (!pFile->hMutex) return TRUE;
  winceMutexAcquire(pFile->hMutex);

  /* Wanting an exclusive lock? *//*想独占锁*/
  if (dwFileOffsetLow == (DWORD)SHARED_FIRST
       && nNumberOfBytesToLockLow == (DWORD)SHARED_SIZE){
    if (pFile->shared->nReaders == 0 && pFile->shared->bExclusive == 0){
       pFile->shared->bExclusive = TRUE;
       pFile->local.bExclusive = TRUE;
       bReturn = TRUE;
    }
  }

  /* Want a read-only lock? *//*想要一个只读的锁?/
  else if (dwFileOffsetLow == (DWORD)SHARED_FIRST &&
           nNumberOfBytesToLockLow == 1){
    if (pFile->shared->bExclusive == 0){
      pFile->local.nReaders ++;
      if (pFile->local.nReaders == 1){
        pFile->shared->nReaders ++;
      }
      bReturn = TRUE;
    }
  }

  /* Want a pending lock? *//*要挂起锁?/
  else if (dwFileOffsetLow == (DWORD)PENDING_BYTE && nNumberOfBytesToLockLow == 1){
    /* If no pending lock has been acquired, then acquire it *//*如果没有待定锁已被收购，然后得到它*/
    if (pFile->shared->bPending == 0) {
      pFile->shared->bPending = TRUE;
      pFile->local.bPending = TRUE;
      bReturn = TRUE;
    }
  }

  /* Want a reserved lock? *//*要保留的锁?/
  else if (dwFileOffsetLow == (DWORD)RESERVED_BYTE && nNumberOfBytesToLockLow == 1){
    if (pFile->shared->bReserved == 0) {
      pFile->shared->bReserved = TRUE;
      pFile->local.bReserved = TRUE;
      bReturn = TRUE;
    }
  }

  winceMutexRelease(pFile->hMutex);
  return bReturn;
}

/*
** An implementation of the UnlockFile API of Windows for CE
*//*Windows?CE的unlockfile?API的实现*/
static BOOL winceUnlockFile(
  LPHANDLE phFile,
  DWORD dwFileOffsetLow,
  DWORD dwFileOffsetHigh,
  DWORD nNumberOfBytesToUnlockLow,
  DWORD nNumberOfBytesToUnlockHigh
){
  winFile *pFile = HANDLE_TO_WINFILE(phFile);
  BOOL bReturn = FALSE;

  UNUSED_PARAMETER(dwFileOffsetHigh);
  UNUSED_PARAMETER(nNumberOfBytesToUnlockHigh);

  if (!pFile->hMutex) return TRUE;
  winceMutexAcquire(pFile->hMutex);

  /* Releasing a reader lock or an exclusive lock *//*释放读锁或排它的锁*/ 
  if (dwFileOffsetLow == (DWORD)SHARED_FIRST){
    /* Did we have an exclusive lock? *//*我们有一个专属的锁？*/
    if (pFile->local.bExclusive){
      assert(nNumberOfBytesToUnlockLow == (DWORD)SHARED_SIZE);
      pFile->local.bExclusive = FALSE;
      pFile->shared->bExclusive = FALSE;
      bReturn = TRUE;
    }

    /* Did we just have a reader lock? *//*我们是不是有可读的锁*/
    else if (pFile->local.nReaders){
      assert(nNumberOfBytesToUnlockLow == (DWORD)SHARED_SIZE || nNumberOfBytesToUnlockLow == 1);
      pFile->local.nReaders --;
      if (pFile->local.nReaders == 0)
      {
        pFile->shared->nReaders --;
      }
      bReturn = TRUE;
    }
  }

  /* Releasing a pending lock *//*释放一个等待的锁*/
  else if (dwFileOffsetLow == (DWORD)PENDING_BYTE && nNumberOfBytesToUnlockLow == 1){
    if (pFile->local.bPending){
      pFile->local.bPending = FALSE;
      pFile->shared->bPending = FALSE;
      bReturn = TRUE;
    }
  }
  /* Releasing a reserved lock *//*释放保留的锁*/ 
  else if (dwFileOffsetLow == (DWORD)RESERVED_BYTE && nNumberOfBytesToUnlockLow == 1){
    if (pFile->local.bReserved) {
      pFile->local.bReserved = FALSE;
      pFile->shared->bReserved = FALSE;
      bReturn = TRUE;
    }
  }

  winceMutexRelease(pFile->hMutex);
  return bReturn;
}
/*
** End of the special code for wince/* wincewince的特殊码结束*/ 
*****************************************************************************/
#endif /* SQLITE_OS_WINCE */

/*
** Lock a file region.
*//*锁定文件区*/
static BOOL winLockFile(
  LPHANDLE phFile,
  DWORD flags,
  DWORD offsetLow,
  DWORD offsetHigh,
  DWORD numBytesLow,
  DWORD numBytesHigh
){
#if SQLITE_OS_WINCE
  /*
  ** NOTE: Windows CE is handled differently here due its lack of the Win32
  **       API LockFile.
  *//*Windows CE是不同，这里的Win32API的lockfile由于缺乏处理*/ 
  return winceLockFile(phFile, offsetLow, offsetHigh,
                       numBytesLow, numBytesHigh);
#else
  if( isNT() ){
    OVERLAPPED ovlp;
    memset(&ovlp, 0, sizeof(OVERLAPPED));
    ovlp.Offset = offsetLow;
    ovlp.OffsetHigh = offsetHigh;
    return osLockFileEx(*phFile, flags, 0, numBytesLow, numBytesHigh, &ovlp);
  }else{
    return osLockFile(*phFile, offsetLow, offsetHigh, numBytesLow,
                      numBytesHigh);
  }
#endif
}

/*
** Unlock a file region.
 *//*unlock的文件区*/ 
static BOOL winUnlockFile(
  LPHANDLE phFile,
  DWORD offsetLow,
  DWORD offsetHigh,
  DWORD numBytesLow,
  DWORD numBytesHigh
){
#if SQLITE_OS_WINCE
  /*
  ** NOTE: Windows CE is handled differently here due its lack of the Win32
  **       API UnlockFile.
  *//*注：Windows CE是不同，这里的Win32?API?unlockfile由于缺乏处理*/ 
  return winceUnlockFile(phFile, offsetLow, offsetHigh,
                         numBytesLow, numBytesHigh);
#else
  if( isNT() ){
    OVERLAPPED ovlp;
    memset(&ovlp, 0, sizeof(OVERLAPPED));
    ovlp.Offset = offsetLow;
    ovlp.OffsetHigh = offsetHigh;
    return osUnlockFileEx(*phFile, 0, numBytesLow, numBytesHigh, &ovlp);
  }else{
    return osUnlockFile(*phFile, offsetLow, offsetHigh, numBytesLow,
                        numBytesHigh);
  }
#endif
}

/*****************************************************************************
** The next group of routines implement the I/O methods specified
** by the sqlite3_io_methods object.
******************************************************************************/
/*一组例程执行的I / O方法所指定的sqlite3_io_methods对象。*/
/*
** Some Microsoft compilers lack this definition.
*//*一些微软的编译器缺乏这种定义*/
#ifndef INVALID_SET_FILE_POINTER
# define INVALID_SET_FILE_POINTER ((DWORD)-1)
#endif

/*
** Move the current position of the file handle passed as the first 
** argument to offset iOffset within the file. If successful, return 0. 
** Otherwise, set pFile->lastErrno and return non-zero.
*//*移动文件的当前位置的手柄来抵消IOFFSET文件中的第一个参数传递。如果成功，返回0。
否则，将返回非零lasterrno pfile ->。*/
static int seekWinFile(winFile *pFile, sqlite3_int64 iOffset){
#if !SQLITE_OS_WINRT
  LONG upperBits;                 /* Most sig. 32 bits of new offset *//*最重要的。新32位的偏移*/
  LONG lowerBits;                 /* Least sig. 32 bits of new offset *//*最小信号。新32位的偏移*/
  DWORD dwRet;                    /* Value returned by SetFilePointer() *//*通过setfilepointer()返回值*/
  DWORD lastErrno;                /* Value returned by GetLastError() *//*通过getlasterror()返回值*/

  upperBits = (LONG)((iOffset>>32) & 0x7fffffff);
  lowerBits = (LONG)(iOffset & 0xffffffff);

  /* API oddity: If successful, SetFilePointer() returns a dword 
  ** containing the lower 32-bits of the new file-offset. Or, if it fails,
  ** it returns INVALID_SET_FILE_POINTER. However according to MSDN, 
  ** INVALID_SET_FILE_POINTER may also be a valid new offset. So to determine 
  ** whether an error has actually occured, it is also necessary to call 
  ** GetLastError().
  *//*API的怪事：如果成功，返回一个DWORD含有setfilepointer()的低32位新的文件偏移。
  或者，如果失败，则返回invalid_set_file_pointer。
  然而据MSDN，invalid_set_file_pointer也可能是一个有效的新的偏移量。
  以确定是否有错误确实发生，它也需要调用getlasterror()。*/
  dwRet = osSetFilePointer(pFile->h, lowerBits, &upperBits, FILE_BEGIN);

  if( (dwRet==INVALID_SET_FILE_POINTER
      && ((lastErrno = osGetLastError())!=NO_ERROR)) ){
    pFile->lastErrno = lastErrno;
    winLogError(SQLITE_IOERR_SEEK, pFile->lastErrno,
             "seekWinFile", pFile->zPath);
    return 1;
  }

  return 0;
#else
  /*
  ** Same as above, except that this implementation works for WinRT.
  *//*同上面一样，只不过这实现工作WinRT*/

  LARGE_INTEGER x;                /* The new offset *//*新的偏移*/
  BOOL bRet;                      /* Value returned by SetFilePointerEx() *//*通过setfilepointerex()返回值*/

  x.QuadPart = iOffset;
  bRet = osSetFilePointerEx(pFile->h, x, 0, FILE_BEGIN);

  if(!bRet){
    pFile->lastErrno = osGetLastError();
    winLogError(SQLITE_IOERR_SEEK, pFile->lastErrno,
             "seekWinFile", pFile->zPath);
    return 1;
  }

  return 0;
#endif
}

/*
** Close a file.
**
** It is reported that an attempt to close a handle might sometimes
** fail.  This is a very unreasonable result, but Windows is notorious
** for being unreasonable so I do not doubt that it might happen.  If
** the close fails, we pause for 100 milliseconds and try again.  As
** many as MX_CLOSE_ATTEMPT attempts to close the handle are made before
** giving up and returning an error.
*//*关闭文件。据悉，试图关闭一个句柄可能有时会失败。
这是一个非常不合理的结果，但Windows是不合理的，
我不相信这会发生的。如果关闭失败，我们暂停100毫秒后再试一次。
作为mx_close_attempt试图关闭该句柄是在放弃之前，导致一个错误。*/
#define MX_CLOSE_ATTEMPT 3
static int winClose(sqlite3_file *id){
  int rc, cnt = 0;
  winFile *pFile = (winFile*)id;

  assert( id!=0 );
#ifndef SQLITE_OMIT_WAL
  assert( pFile->pShm==0 );
#endif
  OSTRACE(("CLOSE %d\n", pFile->h));
  do{
    rc = osCloseHandle(pFile->h);
    /* SimulateIOError( rc=0; cnt=MX_CLOSE_ATTEMPT; ); *//*模拟错误的发生，最大的关闭尝试次数*/ 
  }while( rc==0 && ++cnt < MX_CLOSE_ATTEMPT && (sqlite3_win32_sleep(100), 1) );
#if SQLITE_OS_WINCE
#define WINCE_DELETION_ATTEMPTS 3
  winceDestroyLock(pFile);
  if( pFile->zDeleteOnClose ){
    int cnt = 0;
    while(
           osDeleteFileW(pFile->zDeleteOnClose)==0
        && osGetFileAttributesW(pFile->zDeleteOnClose)!=0xffffffff 
        && cnt++ < WINCE_DELETION_ATTEMPTS
    ){
       sqlite3_win32_sleep(100);  /* Wait a little before trying again *//*再次尝试之前等待一100秒*/
    }
    sqlite3_free(pFile->zDeleteOnClose);
  }
#endif
  OSTRACE(("CLOSE %d %s\n", pFile->h, rc ? "ok" : "failed"));
  if( rc ){
    pFile->h = NULL;
  }
  OpenCounter(-1);
  return rc ? SQLITE_OK
            : winLogError(SQLITE_IOERR_CLOSE, osGetLastError(),
                          "winClose", pFile->zPath);
}

/*
** Read data from a file into a buffer.  Return SQLITE_OK if all
** bytes were read successfully and SQLITE_IOERR if anything goes
** wrong.
*//*从一个文件到一个缓冲区读取数据。
如果所有字节成功和SQLITE_IOERR能阅读的话，返回SQLITE_OK，如果有错误就不返回k*/
static int winRead(
  sqlite3_file *id,          /* File to read from *//*文件读取*/
  void *pBuf,                /* Write content into this buffer *//*该缓冲区的内容写入*/
  int amt,                   /* Number of bytes to read *//*读取的字节数*/
  sqlite3_int64 offset       /* Begin reading at this offset *//*在这个偏移量开始阅读*/
){
#if !SQLITE_OS_WINCE
  OVERLAPPED overlapped;          /* The offset for ReadFile. *//*对于ReadFile偏移*/
#endif
  winFile *pFile = (winFile*)id;  /* file handle *//*文件句柄*/
  DWORD nRead;                    /* Number of bytes actually read from file *//*从文件中读取的字节数*/
  int nRetry = 0;                 /* Number of retrys *//*retrys数*/

  assert( id!=0 );
  SimulateIOError(return SQLITE_IOERR_READ);
  OSTRACE(("READ %d lock=%d\n", pFile->h, pFile->locktype));

#if SQLITE_OS_WINCE
  if( seekWinFile(pFile, offset) ){
    return SQLITE_FULL;
  }
  while( !osReadFile(pFile->h, pBuf, amt, &nRead, 0) ){
#else
  memset(&overlapped, 0, sizeof(OVERLAPPED));
  overlapped.Offset = (LONG)(offset & 0xffffffff);
  overlapped.OffsetHigh = (LONG)((offset>>32) & 0x7fffffff);
  while( !osReadFile(pFile->h, pBuf, amt, &nRead, &overlapped) &&
         osGetLastError()!=ERROR_HANDLE_EOF ){
#endif
    DWORD lastErrno;
    if( retryIoerr(&nRetry, &lastErrno) ) continue;
    pFile->lastErrno = lastErrno;
    return winLogError(SQLITE_IOERR_READ, pFile->lastErrno,
             "winRead", pFile->zPath);
  }
  logIoerr(nRetry);
  if( nRead<(DWORD)amt ){
    /* Unread parts of the buffer must be zero-filled *//*缓冲区的未读的部分必须是零填充*/
    memset(&((char*)pBuf)[nRead], 0, amt-nRead);
    return SQLITE_IOERR_SHORT_READ;
  }

  return SQLITE_OK;
}

/*
** Write data from a buffer into a file.  Return SQLITE_OK on success
** or some other error code on failure.
*//*从缓冲区写入数据到文件。成功返回sqlite_ok否则有一些错误代码就失败*/
static int winWrite(
  sqlite3_file *id,               /* File to write into *//*文件写入*/
  const void *pBuf,               /* The bytes to be written *//*要写入的字节*/
  int amt,                        /* Number of bytes to write *//*写的字节数*/
  sqlite3_int64 offset            /* Offset into the file to begin writing at *//*文件的偏移开始写入部分*/
){
  int rc = 0;                     /* True if error has occured, else false *//*如果发生错误，则为真，否则为假/
  winFile *pFile = (winFile*)id;  /* File handle *//*文件句柄*/
  int nRetry = 0;                 /* Number of retries *//*重试次数*/

  assert( amt>0 );
  assert( pFile );
  SimulateIOError(return SQLITE_IOERR_WRITE);
  SimulateDiskfullError(return SQLITE_FULL);

  OSTRACE(("WRITE %d lock=%d\n", pFile->h, pFile->locktype));

#if SQLITE_OS_WINCE
  rc = seekWinFile(pFile, offset);
  if( rc==0 ){
#else
  {
#endif
#if !SQLITE_OS_WINCE
    OVERLAPPED overlapped;        /* The offset for WriteFile. */
#endif
    u8 *aRem = (u8 *)pBuf;        /* Data yet to be written */
    int nRem = amt;               /* Number of bytes yet to be written */
    DWORD nWrite;                 /* Bytes written by each WriteFile() call */
    DWORD lastErrno = NO_ERROR;   /* Value returned by GetLastError() */

#if !SQLITE_OS_WINCE
    memset(&overlapped, 0, sizeof(OVERLAPPED));
    overlapped.Offset = (LONG)(offset & 0xffffffff);
    overlapped.OffsetHigh = (LONG)((offset>>32) & 0x7fffffff);
#endif

    while( nRem>0 ){
#if SQLITE_OS_WINCE
      if( !osWriteFile(pFile->h, aRem, nRem, &nWrite, 0) ){
#else
      if( !osWriteFile(pFile->h, aRem, nRem, &nWrite, &overlapped) ){
#endif
        if( retryIoerr(&nRetry, &lastErrno) ) continue;
        break;
      }
      if( nWrite<=0 ){
        lastErrno = osGetLastError();
        break;
      }
#if !SQLITE_OS_WINCE
      offset += nWrite;
      overlapped.Offset = (LONG)(offset & 0xffffffff);
      overlapped.OffsetHigh = (LONG)((offset>>32) & 0x7fffffff);
#endif
      aRem += nWrite;
      nRem -= nWrite;
    }
    if( nRem>0 ){
      pFile->lastErrno = lastErrno;
      rc = 1;
    }
  }

  if( rc ){
    if(   ( pFile->lastErrno==ERROR_HANDLE_DISK_FULL )
       || ( pFile->lastErrno==ERROR_DISK_FULL )){
      return SQLITE_FULL;
    }
    return winLogError(SQLITE_IOERR_WRITE, pFile->lastErrno,
             "winWrite", pFile->zPath);
  }else{
    logIoerr(nRetry);
  }
  return SQLITE_OK;
}

/*
** Truncate an open file to a specified size
*/
static int winTruncate(sqlite3_file *id, sqlite3_int64 nByte){
  winFile *pFile = (winFile*)id;  /* File handle object */
  int rc = SQLITE_OK;             /* Return code for this function */

  assert( pFile );

  OSTRACE(("TRUNCATE %d %lld\n", pFile->h, nByte));
  SimulateIOError(return SQLITE_IOERR_TRUNCATE);

  /* If the user has configured a chunk-size for this file, truncate the
  ** file so that it consists of an integer number of chunks (i.e. the
  ** actual file size after the operation may be larger than the requested
  ** size).
  */
  if( pFile->szChunk>0 ){
    nByte = ((nByte + pFile->szChunk - 1)/pFile->szChunk) * pFile->szChunk;
  }

  /* SetEndOfFile() returns non-zero when successful, or zero when it fails. */
  if( seekWinFile(pFile, nByte) ){
    rc = winLogError(SQLITE_IOERR_TRUNCATE, pFile->lastErrno,
             "winTruncate1", pFile->zPath);
  }else if( 0==osSetEndOfFile(pFile->h) ){
    pFile->lastErrno = osGetLastError();
    rc = winLogError(SQLITE_IOERR_TRUNCATE, pFile->lastErrno,
             "winTruncate2", pFile->zPath);
  }

  OSTRACE(("TRUNCATE %d %lld %s\n", pFile->h, nByte, rc ? "failed" : "ok"));
  return rc;
}

#ifdef SQLITE_TEST
/*
** Count the number of fullsyncs and normal syncs.  This is used to test
** that syncs and fullsyncs are occuring at the right times.
*/
int sqlite3_sync_count = 0;
int sqlite3_fullsync_count = 0;
#endif

/*
** Make sure all writes to a particular file are committed to disk.
*/
static int winSync(sqlite3_file *id, int flags){
#ifndef SQLITE_NO_SYNC
  /*
  ** Used only when SQLITE_NO_SYNC is not defined.
   */
  BOOL rc;
#endif
#if !defined(NDEBUG) || !defined(SQLITE_NO_SYNC) || \
    (defined(SQLITE_TEST) && defined(SQLITE_DEBUG))
  /*
  ** Used when SQLITE_NO_SYNC is not defined and by the assert() and/or
  ** OSTRACE() macros.
   */
  winFile *pFile = (winFile*)id;
#else
  UNUSED_PARAMETER(id);
#endif

  assert( pFile );
  /* Check that one of SQLITE_SYNC_NORMAL or FULL was passed */
  assert((flags&0x0F)==SQLITE_SYNC_NORMAL
      || (flags&0x0F)==SQLITE_SYNC_FULL
  );

  OSTRACE(("SYNC %d lock=%d\n", pFile->h, pFile->locktype));

  /* Unix cannot, but some systems may return SQLITE_FULL from here. This
  ** line is to test that doing so does not cause any problems.
  */
  SimulateDiskfullError( return SQLITE_FULL );

#ifndef SQLITE_TEST
  UNUSED_PARAMETER(flags);
#else
  if( (flags&0x0F)==SQLITE_SYNC_FULL ){
    sqlite3_fullsync_count++;
  }
  sqlite3_sync_count++;
#endif

  /* If we compiled with the SQLITE_NO_SYNC flag, then syncing is a
  ** no-op
  */
#ifdef SQLITE_NO_SYNC
  return SQLITE_OK;
#else
  rc = osFlushFileBuffers(pFile->h);
  SimulateIOError( rc=FALSE );
  if( rc ){
    return SQLITE_OK;
  }else{
    pFile->lastErrno = osGetLastError();
    return winLogError(SQLITE_IOERR_FSYNC, pFile->lastErrno,
             "winSync", pFile->zPath);
  }
#endif
}

/*
** Determine the current size of a file in bytes
*/
static int winFileSize(sqlite3_file *id, sqlite3_int64 *pSize){
  winFile *pFile = (winFile*)id;
  int rc = SQLITE_OK;

  assert( id!=0 );
  SimulateIOError(return SQLITE_IOERR_FSTAT);
#if SQLITE_OS_WINRT
  {
    FILE_STANDARD_INFO info;
    if( osGetFileInformationByHandleEx(pFile->h, FileStandardInfo,
                                     &info, sizeof(info)) ){
      *pSize = info.EndOfFile.QuadPart;
    }else{
      pFile->lastErrno = osGetLastError();
      rc = winLogError(SQLITE_IOERR_FSTAT, pFile->lastErrno,
                       "winFileSize", pFile->zPath);
    }
  }
#else
  {
    DWORD upperBits;
    DWORD lowerBits;
    DWORD lastErrno;

    lowerBits = osGetFileSize(pFile->h, &upperBits);
    *pSize = (((sqlite3_int64)upperBits)<<32) + lowerBits;
    if(   (lowerBits == INVALID_FILE_SIZE)
       && ((lastErrno = osGetLastError())!=NO_ERROR) ){
      pFile->lastErrno = lastErrno;
      rc = winLogError(SQLITE_IOERR_FSTAT, pFile->lastErrno,
             "winFileSize", pFile->zPath);
    }
  }
#endif
  return rc;
}

/*
** LOCKFILE_FAIL_IMMEDIATELY is undefined on some Windows systems.
*/
#ifndef LOCKFILE_FAIL_IMMEDIATELY
# define LOCKFILE_FAIL_IMMEDIATELY 1
#endif

#ifndef LOCKFILE_EXCLUSIVE_LOCK
# define LOCKFILE_EXCLUSIVE_LOCK 2
#endif

/*
** Historically, SQLite has used both the LockFile and LockFileEx functions.
** When the LockFile function was used, it was always expected to fail
** immediately if the lock could not be obtained.  Also, it always expected to
** obtain an exclusive lock.  These flags are used with the LockFileEx function
** and reflect those expectations; therefore, they should not be changed.
*/
#ifndef SQLITE_LOCKFILE_FLAGS
# define SQLITE_LOCKFILE_FLAGS   (LOCKFILE_FAIL_IMMEDIATELY | \
                                  LOCKFILE_EXCLUSIVE_LOCK)
#endif

/*
** Currently, SQLite never calls the LockFileEx function without wanting the
** call to fail immediately if the lock cannot be obtained.
*/
#ifndef SQLITE_LOCKFILEEX_FLAGS
# define SQLITE_LOCKFILEEX_FLAGS (LOCKFILE_FAIL_IMMEDIATELY)
#endif

/*
** Acquire a reader lock.
** Different API routines are called depending on whether or not this
** is Win9x or WinNT.
*/
static int getReadLock(winFile *pFile){
  int res;
  if( isNT() ){
#if SQLITE_OS_WINCE
    /*
    ** NOTE: Windows CE is handled differently here due its lack of the Win32
    **       API LockFileEx.
    */
    res = winceLockFile(&pFile->h, SHARED_FIRST, 0, 1, 0);
#else
    res = winLockFile(&pFile->h, SQLITE_LOCKFILEEX_FLAGS, SHARED_FIRST, 0,
                      SHARED_SIZE, 0);
#endif
  }
#ifdef SQLITE_WIN32_HAS_ANSI
  else{
    int lk;
    sqlite3_randomness(sizeof(lk), &lk);
    pFile->sharedLockByte = (short)((lk & 0x7fffffff)%(SHARED_SIZE - 1));
    res = winLockFile(&pFile->h, SQLITE_LOCKFILE_FLAGS,
                      SHARED_FIRST+pFile->sharedLockByte, 0, 1, 0);
  }
#endif
  if( res == 0 ){
    pFile->lastErrno = osGetLastError();
    /* No need to log a failure to lock */
  }
  return res;
}

/*
** Undo a readlock
*/
static int unlockReadLock(winFile *pFile){
  int res;
  DWORD lastErrno;
  if( isNT() ){
    res = winUnlockFile(&pFile->h, SHARED_FIRST, 0, SHARED_SIZE, 0);
  }
#ifdef SQLITE_WIN32_HAS_ANSI
  else{
    res = winUnlockFile(&pFile->h, SHARED_FIRST+pFile->sharedLockByte, 0, 1, 0);
  }
#endif
  if( res==0 && ((lastErrno = osGetLastError())!=ERROR_NOT_LOCKED) ){
    pFile->lastErrno = lastErrno;
    winLogError(SQLITE_IOERR_UNLOCK, pFile->lastErrno,
             "unlockReadLock", pFile->zPath);
  }
  return res;
}

/*
** Lock the file with the lock specified by parameter locktype - one
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
** This routine will only increase a lock.  The winUnlock() routine
** erases all locks at once and returns us immediately to locking level 0.
** It is not possible to lower the locking level one step at a time.  You
** must go straight to locking level 0.
*/
static int winLock(sqlite3_file *id, int locktype){
  int rc = SQLITE_OK;    /* Return code from subroutines */
  int res = 1;           /* Result of a Windows lock call */
  int newLocktype;       /* Set pFile->locktype to this value before exiting */
  int gotPendingLock = 0;/* True if we acquired a PENDING lock this time */
  winFile *pFile = (winFile*)id;
  DWORD lastErrno = NO_ERROR;

  assert( id!=0 );
  OSTRACE(("LOCK %d %d was %d(%d)\n",
           pFile->h, locktype, pFile->locktype, pFile->sharedLockByte));

  /* If there is already a lock of this type or more restrictive on the
  ** OsFile, do nothing. Don't use the end_lock: exit path, as
  ** sqlite3OsEnterMutex() hasn't been called yet.
  */
  if( pFile->locktype>=locktype ){
    return SQLITE_OK;
  }

  /* Make sure the locking sequence is correct
  */
  assert( pFile->locktype!=NO_LOCK || locktype==SHARED_LOCK );
  assert( locktype!=PENDING_LOCK );
  assert( locktype!=RESERVED_LOCK || pFile->locktype==SHARED_LOCK );

  /* Lock the PENDING_LOCK byte if we need to acquire a PENDING lock or
  ** a SHARED lock.  If we are acquiring a SHARED lock, the acquisition of
  ** the PENDING_LOCK byte is temporary.
  */
  newLocktype = pFile->locktype;
  if(   (pFile->locktype==NO_LOCK)
     || (   (locktype==EXCLUSIVE_LOCK)
         && (pFile->locktype==RESERVED_LOCK))
  ){
    int cnt = 3;
    while( cnt-->0 && (res = winLockFile(&pFile->h, SQLITE_LOCKFILE_FLAGS,
                                         PENDING_BYTE, 0, 1, 0))==0 ){
      /* Try 3 times to get the pending lock.  This is needed to work
      ** around problems caused by indexing and/or anti-virus software on
      ** Windows systems.
      ** If you are using this code as a model for alternative VFSes, do not
      ** copy this retry logic.  It is a hack intended for Windows only.
      */
      OSTRACE(("could not get a PENDING lock. cnt=%d\n", cnt));
      if( cnt ) sqlite3_win32_sleep(1);
    }
    gotPendingLock = res;
    if( !res ){
      lastErrno = osGetLastError();
    }
  }

  /* Acquire a shared lock
  */
  if( locktype==SHARED_LOCK && res ){
    assert( pFile->locktype==NO_LOCK );
    res = getReadLock(pFile);
    if( res ){
      newLocktype = SHARED_LOCK;
    }else{
      lastErrno = osGetLastError();
    }
  }

  /* Acquire a RESERVED lock
  */
  if( locktype==RESERVED_LOCK && res ){
    assert( pFile->locktype==SHARED_LOCK );
    res = winLockFile(&pFile->h, SQLITE_LOCKFILE_FLAGS, RESERVED_BYTE, 0, 1, 0);
    if( res ){
      newLocktype = RESERVED_LOCK;
    }else{
      lastErrno = osGetLastError();
    }
  }

  /* Acquire a PENDING lock
  */
  if( locktype==EXCLUSIVE_LOCK && res ){
    newLocktype = PENDING_LOCK;
    gotPendingLock = 0;
  }

  /* Acquire an EXCLUSIVE lock
  */
  if( locktype==EXCLUSIVE_LOCK && res ){
    assert( pFile->locktype>=SHARED_LOCK );
    res = unlockReadLock(pFile);
    OSTRACE(("unreadlock = %d\n", res));
    res = winLockFile(&pFile->h, SQLITE_LOCKFILE_FLAGS, SHARED_FIRST, 0,
                      SHARED_SIZE, 0);
    if( res ){
      newLocktype = EXCLUSIVE_LOCK;
    }else{
      lastErrno = osGetLastError();
      OSTRACE(("error-code = %d\n", lastErrno));
      getReadLock(pFile);
    }
  }

  /* If we are holding a PENDING lock that ought to be released, then
  ** release it now.
  */
  if( gotPendingLock && locktype==SHARED_LOCK ){
    winUnlockFile(&pFile->h, PENDING_BYTE, 0, 1, 0);
  }

  /* Update the state of the lock has held in the file descriptor then
  ** return the appropriate result code.
  */
  if( res ){
    rc = SQLITE_OK;
  }else{
    OSTRACE(("LOCK FAILED %d trying for %d but got %d\n", pFile->h,
           locktype, newLocktype));
    pFile->lastErrno = lastErrno;
    rc = SQLITE_BUSY;
  }
  pFile->locktype = (u8)newLocktype;
  return rc;
}

/*
** This routine checks if there is a RESERVED lock held on the specified
** file by this or any other process. If such a lock is held, return
** non-zero, otherwise zero.
*/
static int winCheckReservedLock(sqlite3_file *id, int *pResOut){
  int rc;
  winFile *pFile = (winFile*)id;

  SimulateIOError( return SQLITE_IOERR_CHECKRESERVEDLOCK; );

  assert( id!=0 );
  if( pFile->locktype>=RESERVED_LOCK ){
    rc = 1;
    OSTRACE(("TEST WR-LOCK %d %d (local)\n", pFile->h, rc));
  }else{
    rc = winLockFile(&pFile->h, SQLITE_LOCKFILE_FLAGS, RESERVED_BYTE, 0, 1, 0);
    if( rc ){
      winUnlockFile(&pFile->h, RESERVED_BYTE, 0, 1, 0);
    }
    rc = !rc;
    OSTRACE(("TEST WR-LOCK %d %d (remote)\n", pFile->h, rc));
  }
  *pResOut = rc;
  return SQLITE_OK;
}

/*
** Lower the locking level on file descriptor id to locktype.  locktype
** must be either NO_LOCK or SHARED_LOCK.
**
** If the locking level of the file descriptor is already at or below
** the requested locking level, this routine is a no-op.
**
** It is not possible for this routine to fail if the second argument
** is NO_LOCK.  If the second argument is SHARED_LOCK then this routine
** might return SQLITE_IOERR;
*/
static int winUnlock(sqlite3_file *id, int locktype){
  int type;
  winFile *pFile = (winFile*)id;
  int rc = SQLITE_OK;
  assert( pFile!=0 );
  assert( locktype<=SHARED_LOCK );
  OSTRACE(("UNLOCK %d to %d was %d(%d)\n", pFile->h, locktype,
          pFile->locktype, pFile->sharedLockByte));
  type = pFile->locktype;
  if( type>=EXCLUSIVE_LOCK ){
    winUnlockFile(&pFile->h, SHARED_FIRST, 0, SHARED_SIZE, 0);
    if( locktype==SHARED_LOCK && !getReadLock(pFile) ){
      /* This should never happen.  We should always be able to
      ** reacquire the read lock */
      rc = winLogError(SQLITE_IOERR_UNLOCK, osGetLastError(),
               "winUnlock", pFile->zPath);
    }
  }
  if( type>=RESERVED_LOCK ){
    winUnlockFile(&pFile->h, RESERVED_BYTE, 0, 1, 0);
  }
  if( locktype==NO_LOCK && type>=SHARED_LOCK ){
    unlockReadLock(pFile);
  }
  if( type>=PENDING_LOCK ){
    winUnlockFile(&pFile->h, PENDING_BYTE, 0, 1, 0);
  }
  pFile->locktype = (u8)locktype;
  return rc;
}

/*
** If *pArg is inititially negative then this is a query.  Set *pArg to
** 1 or 0 depending on whether or not bit mask of pFile->ctrlFlags is set.
**
** If *pArg is 0 or 1, then clear or set the mask bit of pFile->ctrlFlags.
*/
static void winModeBit(winFile *pFile, unsigned char mask, int *pArg){
  if( *pArg<0 ){
    *pArg = (pFile->ctrlFlags & mask)!=0;
  }else if( (*pArg)==0 ){
    pFile->ctrlFlags &= ~mask;
  }else{
    pFile->ctrlFlags |= mask;
  }
}

/*
** Control and query of the open file handle.
*/
static int winFileControl(sqlite3_file *id, int op, void *pArg){
  winFile *pFile = (winFile*)id;
  switch( op ){
    case SQLITE_FCNTL_LOCKSTATE: {
      *(int*)pArg = pFile->locktype;
      return SQLITE_OK;
    }
    case SQLITE_LAST_ERRNO: {
      *(int*)pArg = (int)pFile->lastErrno;
      return SQLITE_OK;
    }
    case SQLITE_FCNTL_CHUNK_SIZE: {
      pFile->szChunk = *(int *)pArg;
      return SQLITE_OK;
    }
    case SQLITE_FCNTL_SIZE_HINT: {
      if( pFile->szChunk>0 ){
        sqlite3_int64 oldSz;
        int rc = winFileSize(id, &oldSz);
        if( rc==SQLITE_OK ){
          sqlite3_int64 newSz = *(sqlite3_int64*)pArg;
          if( newSz>oldSz ){
            SimulateIOErrorBenign(1);
            rc = winTruncate(id, newSz);
            SimulateIOErrorBenign(0);
          }
        }
        return rc;
      }
      return SQLITE_OK;
    }
    case SQLITE_FCNTL_PERSIST_WAL: {
      winModeBit(pFile, WINFILE_PERSIST_WAL, (int*)pArg);
      return SQLITE_OK;
    }
    case SQLITE_FCNTL_POWERSAFE_OVERWRITE: {
      winModeBit(pFile, WINFILE_PSOW, (int*)pArg);
      return SQLITE_OK;
    }
    case SQLITE_FCNTL_VFSNAME: {
      *(char**)pArg = sqlite3_mprintf("win32");
      return SQLITE_OK;
    }
    case SQLITE_FCNTL_WIN32_AV_RETRY: {
      int *a = (int*)pArg;
      if( a[0]>0 ){
        win32IoerrRetry = a[0];
      }else{
        a[0] = win32IoerrRetry;
      }
      if( a[1]>0 ){
        win32IoerrRetryDelay = a[1];
      }else{
        a[1] = win32IoerrRetryDelay;
      }
      return SQLITE_OK;
    }
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
static int winSectorSize(sqlite3_file *id){
  (void)id;
  return SQLITE_DEFAULT_SECTOR_SIZE;
}

/*
** Return a vector of device characteristics.
*/
static int winDeviceCharacteristics(sqlite3_file *id){
  winFile *p = (winFile*)id;
  return SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN |
         ((p->ctrlFlags & WINFILE_PSOW)?SQLITE_IOCAP_POWERSAFE_OVERWRITE:0);
}

#ifndef SQLITE_OMIT_WAL

/* 
** Windows will only let you create file view mappings
** on allocation size granularity boundaries.
** During sqlite3_os_init() we do a GetSystemInfo()
** to get the granularity size.
*/
SYSTEM_INFO winSysInfo;

/*
** Helper functions to obtain and relinquish the global mutex. The
** global mutex is used to protect the winLockInfo objects used by 
** this file, all of which may be shared by multiple threads.
**
** Function winShmMutexHeld() is used to assert() that the global mutex 
** is held when required. This function is only used as part of assert() 
** statements. e.g.
**
**   winShmEnterMutex()
**     assert( winShmMutexHeld() );
**   winShmLeaveMutex()
*/
static void winShmEnterMutex(void){
  sqlite3_mutex_enter(sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER));
}
static void winShmLeaveMutex(void){
  sqlite3_mutex_leave(sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER));
}
#ifdef SQLITE_DEBUG
static int winShmMutexHeld(void) {
  return sqlite3_mutex_held(sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER));
}
#endif

/*
** Object used to represent a single file opened and mmapped to provide
** shared memory.  When multiple threads all reference the same
** log-summary, each thread has its own winFile object, but they all
** point to a single instance of this object.  In other words, each
** log-summary is opened only once per process.
**
** winShmMutexHeld() must be true when creating or destroying
** this object or while reading or writing the following fields:
**
**      nRef
**      pNext 
**
** The following fields are read-only after the object is created:
** 
**      fid
**      zFilename
**
** Either winShmNode.mutex must be held or winShmNode.nRef==0 and
** winShmMutexHeld() is true when reading or writing any other field
** in this structure.
**
*/
struct winShmNode {
  sqlite3_mutex *mutex;      /* Mutex to access this object */
  char *zFilename;           /* Name of the file */
  winFile hFile;             /* File handle from winOpen */

  int szRegion;              /* Size of shared-memory regions */
  int nRegion;               /* Size of array apRegion */
  struct ShmRegion {
    HANDLE hMap;             /* File handle from CreateFileMapping */
    void *pMap;
  } *aRegion;
  DWORD lastErrno;           /* The Windows errno from the last I/O error */

  int nRef;                  /* Number of winShm objects pointing to this */
  winShm *pFirst;            /* All winShm objects pointing to this */
  winShmNode *pNext;         /* Next in list of all winShmNode objects */
#ifdef SQLITE_DEBUG
  u8 nextShmId;              /* Next available winShm.id value */
#endif
};

/*
** A global array of all winShmNode objects.
**
** The winShmMutexHeld() must be true while reading or writing this list.
*/
static winShmNode *winShmNodeList = 0;

/*
** Structure used internally by this VFS to record the state of an
** open shared memory connection.
**
** The following fields are initialized when this object is created and
** are read-only thereafter:
**
**    winShm.pShmNode
**    winShm.id
**
** All other fields are read/write.  The winShm.pShmNode->mutex must be held
** while accessing any read/write fields.
*/
struct winShm {
  winShmNode *pShmNode;      /* The underlying winShmNode object */
  winShm *pNext;             /* Next winShm with the same winShmNode */
  u8 hasMutex;               /* True if holding the winShmNode mutex */
  u16 sharedMask;            /* Mask of shared locks held */
  u16 exclMask;              /* Mask of exclusive locks held */
#ifdef SQLITE_DEBUG
  u8 id;                     /* Id of this connection with its winShmNode */
#endif
};

/*
** Constants used for locking
*/
#define WIN_SHM_BASE   ((22+SQLITE_SHM_NLOCK)*4)        /* first lock byte */
#define WIN_SHM_DMS    (WIN_SHM_BASE+SQLITE_SHM_NLOCK)  /* deadman switch */

/*
** Apply advisory locks for all n bytes beginning at ofst.
*/
#define _SHM_UNLCK  1
#define _SHM_RDLCK  2
#define _SHM_WRLCK  3
static int winShmSystemLock(
  winShmNode *pFile,    /* Apply locks to this open shared-memory segment */
  int lockType,         /* _SHM_UNLCK, _SHM_RDLCK, or _SHM_WRLCK */
  int ofst,             /* Offset to first byte to be locked/unlocked */
  int nByte             /* Number of bytes to lock or unlock */
){
  int rc = 0;           /* Result code form Lock/UnlockFileEx() */

  /* Access to the winShmNode object is serialized by the caller */
  assert( sqlite3_mutex_held(pFile->mutex) || pFile->nRef==0 );

  /* Release/Acquire the system-level lock */
  if( lockType==_SHM_UNLCK ){
    rc = winUnlockFile(&pFile->hFile.h, ofst, 0, nByte, 0);
  }else{
    /* Initialize the locking parameters */
    DWORD dwFlags = LOCKFILE_FAIL_IMMEDIATELY;
    if( lockType == _SHM_WRLCK ) dwFlags |= LOCKFILE_EXCLUSIVE_LOCK;
    rc = winLockFile(&pFile->hFile.h, dwFlags, ofst, 0, nByte, 0);
  }
  
  if( rc!= 0 ){
    rc = SQLITE_OK;
  }else{
    pFile->lastErrno =  osGetLastError();
    rc = SQLITE_BUSY;
  }

  OSTRACE(("SHM-LOCK %d %s %s 0x%08lx\n", 
           pFile->hFile.h,
           rc==SQLITE_OK ? "ok" : "failed",
           lockType==_SHM_UNLCK ? "UnlockFileEx" : "LockFileEx",
           pFile->lastErrno));

  return rc;
}

/* Forward references to VFS methods */
static int winOpen(sqlite3_vfs*,const char*,sqlite3_file*,int,int*);
static int winDelete(sqlite3_vfs *,const char*,int);

/*
** Purge the winShmNodeList list of all entries with winShmNode.nRef==0.
**
** This is not a VFS shared-memory method; it is a utility function called
** by VFS shared-memory methods.
*/
static void winShmPurge(sqlite3_vfs *pVfs, int deleteFlag){
  winShmNode **pp;
  winShmNode *p;
  BOOL bRc;
  assert( winShmMutexHeld() );
  pp = &winShmNodeList;
  while( (p = *pp)!=0 ){
    if( p->nRef==0 ){
      int i;
      if( p->mutex ) sqlite3_mutex_free(p->mutex);
      for(i=0; i<p->nRegion; i++){
        bRc = osUnmapViewOfFile(p->aRegion[i].pMap);
        OSTRACE(("SHM-PURGE pid-%d unmap region=%d %s\n",
                 (int)osGetCurrentProcessId(), i,
                 bRc ? "ok" : "failed"));
        bRc = osCloseHandle(p->aRegion[i].hMap);
        OSTRACE(("SHM-PURGE pid-%d close region=%d %s\n",
                 (int)osGetCurrentProcessId(), i,
                 bRc ? "ok" : "failed"));
      }
      if( p->hFile.h != INVALID_HANDLE_VALUE ){
        SimulateIOErrorBenign(1);
        winClose((sqlite3_file *)&p->hFile);
        SimulateIOErrorBenign(0);
      }
      if( deleteFlag ){
        SimulateIOErrorBenign(1);
        sqlite3BeginBenignMalloc();
        winDelete(pVfs, p->zFilename, 0);
        sqlite3EndBenignMalloc();
        SimulateIOErrorBenign(0);
      }
      *pp = p->pNext;
      sqlite3_free(p->aRegion);
      sqlite3_free(p);
    }else{
      pp = &p->pNext;
    }
  }
}

/*
** Open the shared-memory area associated with database file pDbFd.
**
** When opening a new shared-memory file, if no other instances of that
** file are currently open, in this process or in other processes, then
** the file must be truncated to zero length or have its header cleared.
*/
static int winOpenSharedMemory(winFile *pDbFd){
  struct winShm *p;                  /* The connection to be opened */
  struct winShmNode *pShmNode = 0;   /* The underlying mmapped file */
  int rc;                            /* Result code */
  struct winShmNode *pNew;           /* Newly allocated winShmNode */
  int nName;                         /* Size of zName in bytes */

  assert( pDbFd->pShm==0 );    /* Not previously opened */

  /* Allocate space for the new sqlite3_shm object.  Also speculatively
  ** allocate space for a new winShmNode and filename.
  */
  p = sqlite3_malloc( sizeof(*p) );
  if( p==0 ) return SQLITE_IOERR_NOMEM;
  memset(p, 0, sizeof(*p));
  nName = sqlite3Strlen30(pDbFd->zPath);
  pNew = sqlite3_malloc( sizeof(*pShmNode) + nName + 17 );
  if( pNew==0 ){
    sqlite3_free(p);
    return SQLITE_IOERR_NOMEM;
  }
  memset(pNew, 0, sizeof(*pNew) + nName + 17);
  pNew->zFilename = (char*)&pNew[1];
  sqlite3_snprintf(nName+15, pNew->zFilename, "%s-shm", pDbFd->zPath);
  sqlite3FileSuffix3(pDbFd->zPath, pNew->zFilename); 

  /* Look to see if there is an existing winShmNode that can be used.
  ** If no matching winShmNode currently exists, create a new one.
  */
  winShmEnterMutex();
  for(pShmNode = winShmNodeList; pShmNode; pShmNode=pShmNode->pNext){
    /* TBD need to come up with better match here.  Perhaps
    ** use FILE_ID_BOTH_DIR_INFO Structure.
    */
    if( sqlite3StrICmp(pShmNode->zFilename, pNew->zFilename)==0 ) break;
  }
  if( pShmNode ){
    sqlite3_free(pNew);
  }else{
    pShmNode = pNew;
    pNew = 0;
    ((winFile*)(&pShmNode->hFile))->h = INVALID_HANDLE_VALUE;
    pShmNode->pNext = winShmNodeList;
    winShmNodeList = pShmNode;

    pShmNode->mutex = sqlite3_mutex_alloc(SQLITE_MUTEX_FAST);
    if( pShmNode->mutex==0 ){
      rc = SQLITE_IOERR_NOMEM;
      goto shm_open_err;
    }

    rc = winOpen(pDbFd->pVfs,
                 pShmNode->zFilename,             /* Name of the file (UTF-8) */
                 (sqlite3_file*)&pShmNode->hFile,  /* File handle here */
                 SQLITE_OPEN_WAL | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, /* Mode flags */
                 0);
    if( SQLITE_OK!=rc ){
      goto shm_open_err;
    }

    /* Check to see if another process is holding the dead-man switch.
    ** If not, truncate the file to zero length. 
    */
    if( winShmSystemLock(pShmNode, _SHM_WRLCK, WIN_SHM_DMS, 1)==SQLITE_OK ){
      rc = winTruncate((sqlite3_file *)&pShmNode->hFile, 0);
      if( rc!=SQLITE_OK ){
        rc = winLogError(SQLITE_IOERR_SHMOPEN, osGetLastError(),
                 "winOpenShm", pDbFd->zPath);
      }
    }
    if( rc==SQLITE_OK ){
      winShmSystemLock(pShmNode, _SHM_UNLCK, WIN_SHM_DMS, 1);
      rc = winShmSystemLock(pShmNode, _SHM_RDLCK, WIN_SHM_DMS, 1);
    }
    if( rc ) goto shm_open_err;
  }

  /* Make the new connection a child of the winShmNode */
  p->pShmNode = pShmNode;
#ifdef SQLITE_DEBUG
  p->id = pShmNode->nextShmId++;
#endif
  pShmNode->nRef++;
  pDbFd->pShm = p;
  winShmLeaveMutex();

  /* The reference count on pShmNode has already been incremented under
  ** the cover of the winShmEnterMutex() mutex and the pointer from the
  ** new (struct winShm) object to the pShmNode has been set. All that is
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
  winShmSystemLock(pShmNode, _SHM_UNLCK, WIN_SHM_DMS, 1);
  winShmPurge(pDbFd->pVfs, 0);      /* This call frees pShmNode if required */
  sqlite3_free(p);
  sqlite3_free(pNew);
  winShmLeaveMutex();
  return rc;
}

/*
** Close a connection to shared-memory.  Delete the underlying 
** storage if deleteFlag is true.
*/
static int winShmUnmap(
  sqlite3_file *fd,          /* Database holding shared memory */
  int deleteFlag             /* Delete after closing if true */
){
  winFile *pDbFd;       /* Database holding shared-memory */
  winShm *p;            /* The connection to be closed */
  winShmNode *pShmNode; /* The underlying shared-memory file */
  winShm **pp;          /* For looping over sibling connections */

  pDbFd = (winFile*)fd;
  p = pDbFd->pShm;
  if( p==0 ) return SQLITE_OK;
  pShmNode = p->pShmNode;

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
  winShmEnterMutex();
  assert( pShmNode->nRef>0 );
  pShmNode->nRef--;
  if( pShmNode->nRef==0 ){
    winShmPurge(pDbFd->pVfs, deleteFlag);
  }
  winShmLeaveMutex();

  return SQLITE_OK;
}

/*
** Change the lock state for a shared-memory segment.
*/
static int winShmLock(
  sqlite3_file *fd,          /* Database file holding the shared memory */
  int ofst,                  /* First lock to acquire or release */
  int n,                     /* Number of locks to acquire or release */
  int flags                  /* What to do with the lock */
){
  winFile *pDbFd = (winFile*)fd;        /* Connection holding shared memory */
  winShm *p = pDbFd->pShm;              /* The shared memory being locked */
  winShm *pX;                           /* For looping over all siblings */
  winShmNode *pShmNode = p->pShmNode;
  int rc = SQLITE_OK;                   /* Result code */
  u16 mask;                             /* Mask of locks to take or release */

  assert( ofst>=0 && ofst+n<=SQLITE_SHM_NLOCK );
  assert( n>=1 );
  assert( flags==(SQLITE_SHM_LOCK | SQLITE_SHM_SHARED)
       || flags==(SQLITE_SHM_LOCK | SQLITE_SHM_EXCLUSIVE)
       || flags==(SQLITE_SHM_UNLOCK | SQLITE_SHM_SHARED)
       || flags==(SQLITE_SHM_UNLOCK | SQLITE_SHM_EXCLUSIVE) );
  assert( n==1 || (flags & SQLITE_SHM_EXCLUSIVE)!=0 );

  mask = (u16)((1U<<(ofst+n)) - (1U<<ofst));
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

    
    /* 解锁系统级别的锁 */
    if( (mask & allMask)==0 ){
      rc = winShmSystemLock(pShmNode, _SHM_UNLCK, ofst+WIN_SHM_BASE, n);
    }else{
      rc = SQLITE_OK;
    }

    /* 撤消本地锁 */
    if( rc==SQLITE_OK ){
      p->exclMask &= ~mask;
      p->sharedMask &= ~mask;
    } 
  }else if( flags & SQLITE_SHM_SHARED ){
    u16 allShared = 0;  /*  通过连接不是“P”等持有的锁联盟*/

    /* 找出已经由同级连接的共享锁。
    ** 如果有任何兄弟已经持有一个排它锁，继续前进，返回
    ** SQLITE_BUSY.
    */
    for(pX=pShmNode->pFirst; pX; pX=pX->pNext){
      if( (pX->exclMask & mask)!=0 ){
        rc = SQLITE_BUSY;
        break;
      }
      allShared |= pX->sharedMask;
    }

    /* 如果有必要的话得到在系统级的共享锁*/
    if( rc==SQLITE_OK ){
      if( (allShared & mask)==0 ){
        rc = winShmSystemLock(pShmNode, _SHM_RDLCK, ofst+WIN_SHM_BASE, n);
      }else{
        rc = SQLITE_OK;
      }
    }

    /*获取本地共享锁*/
    if( rc==SQLITE_OK ){
      p->sharedMask |= mask;
    }
  }else{
    /* 
    ** .确保没有兄弟姐妹的连接保持锁定，这将阻止此
锁定。如果有任何事，返回SQLITE_BUSY。
    */
    for(pX=pShmNode->pFirst; pX; pX=pX->pNext){
      if( (pX->exclMask & mask)!=0 || (pX->sharedMask & mask)!=0 ){
        rc = SQLITE_BUSY;
        break;
      }
    }
  
    /* 
    ** 获取了系统级的排它锁。那么，如果成功也标记为被锁定的本地连接。
    */
    if( rc==SQLITE_OK ){
      rc = winShmSystemLock(pShmNode, _SHM_WRLCK, ofst+WIN_SHM_BASE, n);
      if( rc==SQLITE_OK ){
        assert( (p->sharedMask & mask)==0 );
        p->exclMask |= mask;
      }
    }
  }
  sqlite3_mutex_leave(pShmNode->mutex);
  OSTRACE(("SHM-LOCK shmid-%d, pid-%d got %03x,%03x %s\n",
           p->id, (int)osGetCurrentProcessId(), p->sharedMask, p->exclMask,
           rc ? "failed" : "ok"));
  return rc;
}

/*
** 实现共享内存的内存屏障或内存栅栏。
**
** 所有加载和存储开始前的屏障前必须完成
屏障之后的任何加载或存储开始。
*/
static void winShmBarrier(
  sqlite3_file *fd          /*数据库保持的共享存储器 */
){
  UNUSED_PARAMETER(fd);
  /* MemoryBarrier(); // does not work -- do not know why not */
  winShmEnterMutex();
  winShmLeaveMutex();
}

/*
** 此函数被调用，以获得一个指针的区域IREGION
与数据库文件FD关联的共享内存。共享内存区域
编号从零开始。每个共享存储器区域是szRegion
字节大小。
**
** 如果发生错误，则返回错误代码和*页设置为NULL。
**
** 否则，如果isWrite参数是0，所请求的共享存储器
区域尚未分配（由任何客户端，其中包括一个运行
独立的进程），那么*页设置为NULL，并返回SQLITE_OK。如果
isWrite是非零并且请求的共享存储器区域具有尚未被分配，它是由该功能分配。
**
**如果共享存储器区域已经被分配或者被分配
此呼叫，如上所述，则它被映射到该过程
地址空间（如果它尚未），*页被设定为指向映射
内存和SQLITE_OK返回。
*/
static int winShmMap(
  sqlite3_file *fd,               /*处理数据库文件打开 */
  int iRegion,                    /*地区检索*/
  int szRegion,                   /*地区规模 */
  int isWrite,                    /* 如此，如果需要延长的文件*/
  void volatile **pp              /* OUT: Mapped memory */
){
  winFile *pDbFd = (winFile*)fd;
  winShm *p = pDbFd->pShm;
  winShmNode *pShmNode;
  int rc = SQLITE_OK;

  if( !p ){
    rc = winOpenSharedMemory(pDbFd);
    if( rc!=SQLITE_OK ) return rc;
    p = pDbFd->pShm;
  }
  pShmNode = p->pShmNode;

  sqlite3_mutex_enter(pShmNode->mutex);
  assert( szRegion==pShmNode->szRegion || pShmNode->nRegion==0 );

  if( pShmNode->nRegion<=iRegion ){
    struct ShmRegion *apNew;           /* 新aRegion[]数组 */
    int nByte = (iRegion+1)*szRegion;  /* 最小所需文件大小 */
    sqlite3_int64 sz;                  /* 沃尔玛索引文件的电流大小 */

    pShmNode->szRegion = szRegion;

    /* 所请求的区域没有被映射到这个进程的地址空间。
检查，看它是否已被分配（即如果沃尔玛索引文件足够大，以包含所请求的区域）。
    */
    rc = winFileSize((sqlite3_file *)&pShmNode->hFile, &sz);
    if( rc!=SQLITE_OK ){
      rc = winLogError(SQLITE_IOERR_SHMSIZE, osGetLastError(),
               "winShmMap1", pDbFd->zPath);
      goto shmpage_out;
    }

    if( sz<nByte ){
      /*
 所请求的存储区域不存在。如果isWrite设置为为零，退出早。 *页将被设置为NULL，并返回SQLITE_OK。或者，如果isWrite非零，使用ftruncate（）分配所请求的存储区域。
      */
      if( !isWrite ) goto shmpage_out;
      rc = winTruncate((sqlite3_file *)&pShmNode->hFile, nByte);
      if( rc!=SQLITE_OK ){
        rc = winLogError(SQLITE_IOERR_SHMSIZE, osGetLastError(),
                 "winShmMap2", pDbFd->zPath);
        goto shmpage_out;
      }
    }

    /* 地图请求的内存区域到这个进程的地址空间。 */
    apNew = (struct ShmRegion *)sqlite3_realloc(
        pShmNode->aRegion, (iRegion+1)*sizeof(apNew[0])
    );
    if( !apNew ){
      rc = SQLITE_IOERR_NOMEM;
      goto shmpage_out;
    }
    pShmNode->aRegion = apNew;

    while( pShmNode->nRegion<=iRegion ){
      HANDLE hMap;                /* 文件映射手柄 */
      void *pMap = 0;             /* 映射内存区域 */
     
#if SQLITE_OS_WINRT
      hMap = osCreateFileMappingFromApp(pShmNode->hFile.h,
          NULL, PAGE_READWRITE, nByte, NULL
      );
#else
      hMap = osCreateFileMappingW(pShmNode->hFile.h, 
          NULL, PAGE_READWRITE, 0, nByte, NULL
      );
#endif
      OSTRACE(("SHM-MAP pid-%d create region=%d nbyte=%d %s\n",
               (int)osGetCurrentProcessId(), pShmNode->nRegion, nByte,
               hMap ? "ok" : "failed"));
      if( hMap ){
        int iOffset = pShmNode->nRegion*szRegion;
        int iOffsetShift = iOffset % winSysInfo.dwAllocationGranularity;
#if SQLITE_OS_WINRT
        pMap = osMapViewOfFileFromApp(hMap, FILE_MAP_WRITE | FILE_MAP_READ,
            iOffset - iOffsetShift, szRegion + iOffsetShift
        );
#else
        pMap = osMapViewOfFile(hMap, FILE_MAP_WRITE | FILE_MAP_READ,
            0, iOffset - iOffsetShift, szRegion + iOffsetShift
        );
#endif
        OSTRACE(("SHM-MAP pid-%d map region=%d offset=%d size=%d %s\n",
                 (int)osGetCurrentProcessId(), pShmNode->nRegion, iOffset,
                 szRegion, pMap ? "ok" : "failed"));
      }
      if( !pMap ){
        pShmNode->lastErrno = osGetLastError();
        rc = winLogError(SQLITE_IOERR_SHMMAP, pShmNode->lastErrno,
                 "winShmMap3", pDbFd->zPath);
        if( hMap ) osCloseHandle(hMap);
        goto shmpage_out;
      }

      pShmNode->aRegion[pShmNode->nRegion].pMap = pMap;
      pShmNode->aRegion[pShmNode->nRegion].hMap = hMap;
      pShmNode->nRegion++;
    }
  }

shmpage_out:
  if( pShmNode->nRegion>iRegion ){
    int iOffset = iRegion*szRegion;
    int iOffsetShift = iOffset % winSysInfo.dwAllocationGranularity;
    char *p = (char *)pShmNode->aRegion[iRegion].pMap;
    *pp = (void *)&p[iOffsetShift];
  }else{
    *pp = 0;
  }
  sqlite3_mutex_leave(pShmNode->mutex);
  return rc;
}

#else
# define winShmMap     0
# define winShmLock    0
# define winShmBarrier 0
# define winShmUnmap   0
#endif /* #ifndef SQLITE_OMIT_WAL */

/*
** 在这里，结束所有sqlite3_file方法的实现。
**
********************** End sqlite3_file Methods *******************************
******************************************************************************/

/*
** 此向量定义了所有能上的sqlite3_file为Win32操作方法。
*/
static const sqlite3_io_methods winIoMethod = {
  2,                              /* iVersion */
  winClose,                       /* xClose */
  winRead,                        /* xRead */
  winWrite,                       /* xWrite */
  winTruncate,                    /* xTruncate */
  winSync,                        /* xSync */
  winFileSize,                    /* xFileSize */
  winLock,                        /* xLock */
  winUnlock,                      /* xUnlock */
  winCheckReservedLock,           /* xCheckReservedLock */
  winFileControl,                 /* xFileControl */
  winSectorSize,                  /* xSectorSize */
  winDeviceCharacteristics,       /* xDeviceCharacteristics */
  winShmMap,                      /* xShmMap */
  winShmLock,                     /* xShmLock */
  winShmBarrier,                  /* xShmBarrier */
  winShmUnmap                     /* xShmUnmap */
};

/****************************************************************************
**************************** sqlite3_vfs methods ****************************
**
** 这种划分包含的sqlite3_vfs对象方法的实现
*/

/*
将一个UTF-8名成任何形式的基本操作系统都想在太空保存结果的文件名从malloc的获得，必须通过调用函数被释放。
*/
static void *convertUtf8Filename(const char *zFilename){
  void *zConverted = 0;
  if( isNT() ){
    zConverted = utf8ToUnicode(zFilename);
  }
#ifdef SQLITE_WIN32_HAS_ANSI
  else{
    zConverted = sqlite3_win32_utf8_to_mbcs(zFilename);
  }
#endif
  /* 主叫方会处理好了内存 */
  return zConverted;
}

/*
** 创建Zbuf成立临时文件名。Zbuf成立必须足够大，以容纳在pVfs-> mxPathname字符。
*/
static int getTempname(int nBuf, char *zBuf){
  static char zChars[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789";
  size_t i, j;
  int nTempPath;
  char zTempPath[MAX_PATH+2];

  /*
 这是奇怪，这里模拟一个IO错误，但实际上这只是使用IO-误差的基础设施，以测试SQLite的处理这功能失败。
  */
  SimulateIOError( return SQLITE_IOERR );

  memset(zTempPath, 0, MAX_PATH+2);

  if( sqlite3_temp_directory ){
    sqlite3_snprintf(MAX_PATH-30, zTempPath, "%s", sqlite3_temp_directory);
  }
#if !SQLITE_OS_WINRT
  else if( isNT() ){
    char *zMulti;
    WCHAR zWidePath[MAX_PATH];
    osGetTempPathW(MAX_PATH-30, zWidePath);
    zMulti = unicodeToUtf8(zWidePath);
    if( zMulti ){
      sqlite3_snprintf(MAX_PATH-30, zTempPath, "%s", zMulti);
      sqlite3_free(zMulti);
    }else{
      return SQLITE_IOERR_NOMEM;
    }
  }
#ifdef SQLITE_WIN32_HAS_ANSI
  else{
    char *zUtf8;
    char zMbcsPath[MAX_PATH];
    osGetTempPathA(MAX_PATH-30, zMbcsPath);
    zUtf8 = sqlite3_win32_mbcs_to_utf8(zMbcsPath);
    if( zUtf8 ){
      sqlite3_snprintf(MAX_PATH-30, zTempPath, "%s", zUtf8);
      sqlite3_free(zUtf8);
    }else{
      return SQLITE_IOERR_NOMEM;
    }
  }
#endif
#endif

  /* 
检查输出缓冲区的临时文件名足够大。如果不是，则返回SQLITE_ERROR。
  */
  nTempPath = sqlite3Strlen30(zTempPath);

  if( (nTempPath + sqlite3Strlen30(SQLITE_TEMP_FILE_PREFIX) + 18) >= nBuf ){
    return SQLITE_ERROR;
  }

  for(i=nTempPath; i>0 && zTempPath[i-1]=='\\'; i--){}
  zTempPath[i] = 0;

  sqlite3_snprintf(nBuf-18, zBuf, (nTempPath > 0) ?
                       "%s\\"SQLITE_TEMP_FILE_PREFIX : SQLITE_TEMP_FILE_PREFIX,
                   zTempPath);
  j = sqlite3Strlen30(zBuf);
  sqlite3_randomness(15, &zBuf[j]);
  for(i=0; i<15; i++, j++){
    zBuf[j] = (char)zChars[ ((unsigned char)zBuf[j])%(sizeof(zChars)-1) ];
  }
  zBuf[j] = 0;
  zBuf[j+1] = 0;

  OSTRACE(("TEMP FILENAME: %s\n", zBuf));
  return SQLITE_OK; 
}

/*
** 返回true，如果指定的文件实际上是目录。返回false
它比一个目录的东西之外，或者如果有任何种类的存储器分配失败。
*/
static int winIsDir(const void *zConverted){
  DWORD attr;
  int rc = 0;
  DWORD lastErrno;

  if( isNT() ){
    int cnt = 0;
    WIN32_FILE_ATTRIBUTE_DATA sAttrData;
    memset(&sAttrData, 0, sizeof(sAttrData));
    while( !(rc = osGetFileAttributesExW((LPCWSTR)zConverted,
                             GetFileExInfoStandard,
                             &sAttrData)) && retryIoerr(&cnt, &lastErrno) ){}
    if( !rc ){
      return 0; /* Invalid name? */
    }
    attr = sAttrData.dwFileAttributes;
#if SQLITE_OS_WINCE==0
  }else{
    attr = osGetFileAttributesA((char*)zConverted);
#endif
  }
  return (attr!=INVALID_FILE_ATTRIBUTES) && (attr&FILE_ATTRIBUTE_DIRECTORY);
}

/*
** 打开一个文件。
*/
static int winOpen(
  sqlite3_vfs *pVfs,        /* Not used */
  const char *zName,        /* Name of the file (UTF-8) */
  sqlite3_file *id,         /* Write the SQLite file handle here */
  int flags,                /* Open mode flags */
  int *pOutFlags            /* Status return flags */
){
  HANDLE h;
  DWORD lastErrno;
  DWORD dwDesiredAccess;
  DWORD dwShareMode;
  DWORD dwCreationDisposition;
  DWORD dwFlagsAndAttributes = 0;
#if SQLITE_OS_WINCE
  int isTemp = 0;
#endif
  winFile *pFile = (winFile*)id;
  void *zConverted;              /* Filename in OS encoding */
  const char *zUtf8Name = zName; /* Filename in UTF-8 encoding */
  int cnt = 0;

  /* 
如果参数zPath是一个NULL指针，该功能需要打开一个临时文件。使用该缓冲器来存储在文件名中。
  */
  char zTmpname[MAX_PATH+2];     /* 缓冲区用于创建临时文件名 */

  int rc = SQLITE_OK;            /* 函数返回代码 */
#if !defined(NDEBUG) || SQLITE_OS_WINCE
  int eType = flags&0xFFFFFF00;  /* 文件类型打开*/
#endif

  int isExclusive  = (flags & SQLITE_OPEN_EXCLUSIVE);
  int isDelete     = (flags & SQLITE_OPEN_DELETEONCLOSE);
  int isCreate     = (flags & SQLITE_OPEN_CREATE);
#ifndef NDEBUG
  int isReadonly   = (flags & SQLITE_OPEN_READONLY);
#endif
  int isReadWrite  = (flags & SQLITE_OPEN_READWRITE);

#ifndef NDEBUG
  int isOpenJournal = (isCreate && (
        eType==SQLITE_OPEN_MASTER_JOURNAL 
     || eType==SQLITE_OPEN_MAIN_JOURNAL 
     || eType==SQLITE_OPEN_WAL
  ));
#endif

  /*请检查下面的语句是正确的：
??（一）必须设置READWRITE和READONLY标志究竟之一，
??（二）如果CREATE设置，然后READWRITE也必须设置，并
??（三）如果EXCLUSIVE设置，然后CREATE也必须设置。
??（四）如果DELETEONCLOSE设置，然后CREATE也必须设置。
  */
  assert((isReadonly==0 || isReadWrite==0) && (isReadWrite || isReadonly));
  assert(isCreate==0 || isReadWrite);
  assert(isExclusive==0 || isCreate);
  assert(isDelete==0 || isCreate);

  /* 主要的DB，主轴颈，WAL文件和主杂志从不
???自动删除。也不是他们曾经的临时文件。  */
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

  assert( id!=0 );
  UNUSED_PARAMETER(pVfs);

#if SQLITE_OS_WINRT
  if( !sqlite3_temp_directory ){
    sqlite3_log(SQLITE_ERROR,
        "sqlite3_temp_directory variable should be set for WinRT");
  }
#endif

  pFile->h = INVALID_HANDLE_VALUE;

  /* 
如果第二个参数给这个函数是NULL，生成一个临时文件名使用
  */
  if( !zUtf8Name ){
    assert(isDelete && !isOpenJournal);
    rc = getTempname(MAX_PATH+2, zTmpname);
    if( rc!=SQLITE_OK ){
      return rc;
    }
    zUtf8Name = zTmpname;
  }

  /*数据库文件名是双零终止的，如果他们不
?用的URI参数。因此，他们总是可以传递到
??sqlite3_uri_parameter（）。
  */
  assert( (eType!=SQLITE_OPEN_MAIN_DB) || (flags & SQLITE_OPEN_URI) ||
        zUtf8Name[strlen(zUtf8Name)+1]==0 );

  /* Convert the filename to the system encoding. */
  zConverted = convertUtf8Filename(zUtf8Name);
  if( zConverted==0 ){
    return SQLITE_IOERR_NOMEM;
  }

  if( winIsDir(zConverted) ){
    sqlite3_free(zConverted);
    return SQLITE_CANTOPEN_ISDIR;
  }

  if( isReadWrite ){
    dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
  }else{
    dwDesiredAccess = GENERIC_READ;
  }

  /* 
SQLITE_OPEN_EXCLUSIVE用于确保新的文件是
??创建。 SQLite不使用它来表示“独家访问”
??因为它通常被理解。
  */
  if( isExclusive ){
    /* 创建一个新文件，只有当它不存在。 */
    /* 如果该文件存在，它失败。 */
    dwCreationDisposition = CREATE_NEW;
  }else if( isCreate ){
    /* 打开现有文件或创建如果它不存在 */
    dwCreationDisposition = OPEN_ALWAYS;
  }else{
    /* 打开一个文件，只有当它的存在。 */
    dwCreationDisposition = OPEN_EXISTING;
  }

  dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;

  if( isDelete ){
#if SQLITE_OS_WINCE
    dwFlagsAndAttributes = FILE_ATTRIBUTE_HIDDEN;
    isTemp = 1;
#else
    dwFlagsAndAttributes = FILE_ATTRIBUTE_TEMPORARY
                               | FILE_ATTRIBUTE_HIDDEN
                               | FILE_FLAG_DELETE_ON_CLOSE;
#endif
  }else{
    dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
  }
  /*从互联网的报道是性能，如果FILE_FLAG_RANDOM_ACCESS用于总是更好。票务＃2699. */
#if SQLITE_OS_WINCE
  dwFlagsAndAttributes |= FILE_FLAG_RANDOM_ACCESS;
#endif

  if( isNT() ){
#if SQLITE_OS_WINRT
    CREATEFILE2_EXTENDED_PARAMETERS extendedParameters;
    extendedParameters.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
    extendedParameters.dwFileAttributes =
            dwFlagsAndAttributes & FILE_ATTRIBUTE_MASK;
    extendedParameters.dwFileFlags = dwFlagsAndAttributes & FILE_FLAG_MASK;
    extendedParameters.dwSecurityQosFlags = SECURITY_ANONYMOUS;
    extendedParameters.lpSecurityAttributes = NULL;
    extendedParameters.hTemplateFile = NULL;
    while( (h = osCreateFile2((LPCWSTR)zConverted,
                              dwDesiredAccess,
                              dwShareMode,
                              dwCreationDisposition,
                              &extendedParameters))==INVALID_HANDLE_VALUE &&
                              retryIoerr(&cnt, &lastErrno) ){
               /* Noop */
    }
#else
    while( (h = osCreateFileW((LPCWSTR)zConverted,
                              dwDesiredAccess,
                              dwShareMode, NULL,
                              dwCreationDisposition,
                              dwFlagsAndAttributes,
                              NULL))==INVALID_HANDLE_VALUE &&
                              retryIoerr(&cnt, &lastErrno) ){
               /* Noop */
    }
#endif
  }
#ifdef SQLITE_WIN32_HAS_ANSI
  else{
    while( (h = osCreateFileA((LPCSTR)zConverted,
                              dwDesiredAccess,
                              dwShareMode, NULL,
                              dwCreationDisposition,
                              dwFlagsAndAttributes,
                              NULL))==INVALID_HANDLE_VALUE &&
                              retryIoerr(&cnt, &lastErrno) ){
               /* 空操作 */
    }
  }
#endif
  logIoerr(cnt);

  OSTRACE(("OPEN %d %s 0x%lx %s\n", 
           h, zName, dwDesiredAccess, 
           h==INVALID_HANDLE_VALUE ? "failed" : "ok"));

  if( h==INVALID_HANDLE_VALUE ){
    pFile->lastErrno = lastErrno;
    winLogError(SQLITE_CANTOPEN, pFile->lastErrno, "winOpen", zUtf8Name);
    sqlite3_free(zConverted);
    if( isReadWrite && !isExclusive ){
      return winOpen(pVfs, zName, id, 
             ((flags|SQLITE_OPEN_READONLY)&~(SQLITE_OPEN_CREATE|SQLITE_OPEN_READWRITE)), pOutFlags);
    }else{
      return SQLITE_CANTOPEN_BKPT;
    }
  }

  if( pOutFlags ){
    if( isReadWrite ){
      *pOutFlags = SQLITE_OPEN_READWRITE;
    }else{
      *pOutFlags = SQLITE_OPEN_READONLY;
    }
  }

  memset(pFile, 0, sizeof(*pFile));
  pFile->pMethod = &winIoMethod;
  pFile->h = h;
  pFile->lastErrno = NO_ERROR;
  pFile->pVfs = pVfs;
#ifndef SQLITE_OMIT_WAL
  pFile->pShm = 0;
#endif
  pFile->zPath = zName;
  if( sqlite3_uri_boolean(zName, "psow", SQLITE_POWERSAFE_OVERWRITE) ){
    pFile->ctrlFlags |= WINFILE_PSOW;
  }

#if SQLITE_OS_WINCE
  if( isReadWrite && eType==SQLITE_OPEN_MAIN_DB
       && !winceCreateLock(zName, pFile)
  ){
    osCloseHandle(h);
    sqlite3_free(zConverted);
    return SQLITE_CANTOPEN_BKPT;
  }
  if( isTemp ){
    pFile->zDeleteOnClose = zConverted;
  }else
#endif
  {
    sqlite3_free(zConverted);
  }

  OpenCounter(+1);
  return rc;
}

/*
** 删除指定的文件。
**
** 需要注意的是Windows不允许一个文件，如果其他一些被删除
进程把它打开。有时，病毒扫描程序或索引程序
?将打开后不久它是为了做创建一个日志文件
不管它。虽然这个其他进程持有
文件打开，我们将无法将其删除。要解决此
?问题，我们推迟100毫秒，再次尝试删除。向上
?并返回一个错误。
*/
static int winDelete(
  sqlite3_vfs *pVfs,          /* Not used on win32 */
  const char *zFilename,      /* Name of file to delete */
  int syncDir                 /* Not used on win32 */
){
  int cnt = 0;
  int rc;
  DWORD attr;
  DWORD lastErrno;
  void *zConverted;
  UNUSED_PARAMETER(pVfs);
  UNUSED_PARAMETER(syncDir);

  SimulateIOError(return SQLITE_IOERR_DELETE);
  zConverted = convertUtf8Filename(zFilename);
  if( zConverted==0 ){
    return SQLITE_IOERR_NOMEM;
  }
  if( isNT() ){
    do {
#if SQLITE_OS_WINRT
      WIN32_FILE_ATTRIBUTE_DATA sAttrData;
      memset(&sAttrData, 0, sizeof(sAttrData));
      if ( osGetFileAttributesExW(zConverted, GetFileExInfoStandard,
                                  &sAttrData) ){
        attr = sAttrData.dwFileAttributes;
      }else{
        rc = SQLITE_OK; /* Already gone? */
        break;
      }
#else
      attr = osGetFileAttributesW(zConverted);
#endif
      if ( attr==INVALID_FILE_ATTRIBUTES ){
        rc = SQLITE_OK; /* Already gone? */
        break;
      }
      if ( attr&FILE_ATTRIBUTE_DIRECTORY ){
        rc = SQLITE_ERROR; /* Files only. */
        break;
      }
      if ( osDeleteFileW(zConverted) ){
        rc = SQLITE_OK; /* Deleted OK. */
        break;
      }
      if ( !retryIoerr(&cnt, &lastErrno) ){
        rc = SQLITE_ERROR; /* No more retries. */
        break;
      }
    } while(1);
  }
#ifdef SQLITE_WIN32_HAS_ANSI
  else{
    do {
      attr = osGetFileAttributesA(zConverted);
      if ( attr==INVALID_FILE_ATTRIBUTES ){
        rc = SQLITE_OK; /* Already gone? */
        break;
      }
      if ( attr&FILE_ATTRIBUTE_DIRECTORY ){
        rc = SQLITE_ERROR; /* Files only. */
        break;
      }
      if ( osDeleteFileA(zConverted) ){
        rc = SQLITE_OK; /* Deleted OK. */
        break;
      }
      if ( !retryIoerr(&cnt, &lastErrno) ){
        rc = SQLITE_ERROR; /* No more retries. */
        break;
      }
    } while(1);
  }
#endif
  if( rc ){
    rc = winLogError(SQLITE_IOERR_DELETE, lastErrno,
             "winDelete", zFilename);
  }else{
    logIoerr(cnt);
  }
  sqlite3_free(zConverted);
  OSTRACE(("DELETE \"%s\" %s\n", zFilename, (rc ? "failed" : "ok" )));
  return rc;
}

/*
** 检查文件的存在性和状态.
*/
static int winAccess(
  sqlite3_vfs *pVfs,         /* Not used on win32 */
  const char *zFilename,     /* Name of file to check */
  int flags,                 /* Type of test to make on this file */
  int *pResOut               /* OUT: Result */
){
  DWORD attr;
  int rc = 0;
  DWORD lastErrno;
  void *zConverted;
  UNUSED_PARAMETER(pVfs);

  SimulateIOError( return SQLITE_IOERR_ACCESS; );
  zConverted = convertUtf8Filename(zFilename);
  if( zConverted==0 ){
    return SQLITE_IOERR_NOMEM;
  }
  if( isNT() ){
    int cnt = 0;
    WIN32_FILE_ATTRIBUTE_DATA sAttrData;
    memset(&sAttrData, 0, sizeof(sAttrData));
    while( !(rc = osGetFileAttributesExW((LPCWSTR)zConverted,
                             GetFileExInfoStandard, 
                             &sAttrData)) && retryIoerr(&cnt, &lastErrno) ){}
    if( rc ){
      /* 对于一个SQLITE_ACCESS_EXISTS查询，处理一个零长度文件，就好像它不存在.
      */
      if(    flags==SQLITE_ACCESS_EXISTS
          && sAttrData.nFileSizeHigh==0 
          && sAttrData.nFileSizeLow==0 ){
        attr = INVALID_FILE_ATTRIBUTES;
      }else{
        attr = sAttrData.dwFileAttributes;
      }
    }else{
      logIoerr(cnt);
      if( lastErrno!=ERROR_FILE_NOT_FOUND && lastErrno!=ERROR_PATH_NOT_FOUND ){
        winLogError(SQLITE_IOERR_ACCESS, lastErrno, "winAccess", zFilename);
        sqlite3_free(zConverted);
        return SQLITE_IOERR_ACCESS;
      }else{
        attr = INVALID_FILE_ATTRIBUTES;
      }
    }
  }
#ifdef SQLITE_WIN32_HAS_ANSI
  else{
    attr = osGetFileAttributesA((char*)zConverted);
  }
#endif
  sqlite3_free(zConverted);
  switch( flags ){
    case SQLITE_ACCESS_READ:
    case SQLITE_ACCESS_EXISTS:
      rc = attr!=INVALID_FILE_ATTRIBUTES;
      break;
    case SQLITE_ACCESS_READWRITE:
      rc = attr!=INVALID_FILE_ATTRIBUTES &&
             (attr & FILE_ATTRIBUTE_READONLY)==0;
      break;
    default:
      assert(!"Invalid flags argument");
  }
  *pResOut = rc;
  return SQLITE_OK;
}


/*
** 返回非零如果指定的路径名应逐字使用。如果
?非零从该函数返回时，调用函数必须简单
?逐字使用所提供的路径名 - 或 - 使用GetFullPathName的Win32 API函数（如果可用）解析成一个完整的路径名。
*/
static BOOL winIsVerbatimPathname(
  const char *zPathname
){
  /*
  ** 如果路径名称以正斜杠或反斜杠，它可以是
一个合法的UNC名称，一个体积相对路径，或在绝对路径名
?在Windows上的“Unix”的格式。有没有简单的方法来区分
?最后两例;因此，我们返回TRUE的安全返回值
所以这个函数的调用者就干脆利用它一字不差。
  */
  if ( zPathname[0]=='/' || zPathname[0]=='\\' ){
    return TRUE;
  }

  /*
  ** 如果路径名以字母开头和一个冒号它要么是卷
??**相对路径或绝对路径。这个函数的调用者不得
??**试图把它当作一个相对路径名（即他们只需要使用
??**它逐字）。
  */
  if ( sqlite3Isalpha(zPathname[0]) && zPathname[1]==':' ){
    return TRUE;
  }

  /*
  ** 如果我们到了这一点，路径名应几乎可以肯定是一个纯粹的
??**相对的（即不是一个UNC名称，而不是绝对的，相对不卷）。
  */
  return FALSE;
}

/*
** 把一个相对路径名成为一个完整的路径名。写全
**路径进入ZOUT[]。 ZOUT[]将至少pVfs-> mxPathname
**字节大小。
*/
static int winFullPathname(
  sqlite3_vfs *pVfs,            /* 指针VFS对象 */
  const char *zRelative,        /* 可能是相对输入路径 */
  int nFull,                    /* 在字节的输出缓冲区大小*/
  char *zFull                   /* 输出缓冲 */
){
  
#if defined(__CYGWIN__)
  SimulateIOError( return SQLITE_ERROR );
  UNUSED_PARAMETER(nFull);
  assert( pVfs->mxPathname>=MAX_PATH );
  assert( nFull>=pVfs->mxPathname );
  if ( sqlite3_data_directory && !winIsVerbatimPathname(zRelative) ){
    /*
    ** 注：我们正在处理的相对路径名和数据
??????目录已被设定。因此，用它作为基础
????????为相对路径名转换为绝对
????1通过预先的数据目录和斜线。
    */
    char zOut[MAX_PATH+1];
    memset(zOut, 0, MAX_PATH+1);
    cygwin_conv_to_win32_path(zRelative, zOut); /* POSIX to Win32 */
    sqlite3_snprintf(MIN(nFull, pVfs->mxPathname), zFull, "%s\\%s",
                     sqlite3_data_directory, zOut);
  }else{
    /*
    ** 注：最大长度所需的Cygwin的文档状态
????**为通过缓冲区cygwin_conv_to_full_win32_path
????**是MAX_PATH。
    */
    cygwin_conv_to_full_win32_path(zRelative, zFull);
  }
  return SQLITE_OK;
#endif

#if (SQLITE_OS_WINCE || SQLITE_OS_WINRT) && !defined(__CYGWIN__)
  SimulateIOError( return SQLITE_ERROR );
  /* WinCE has no concept of a relative pathname, or so I am told. */
  /* WinRT has no way to convert a relative path to an absolute one. */
  if ( sqlite3_data_directory && !winIsVerbatimPathname(zRelative) ){
    /*
    ** 注：我们正在处理的相对路径名和数据
????**目录已设置。因此，用它作为基础
????**为相对路径名转换为绝对
????**1通过预先的数据目录和反斜杠。
    */
    sqlite3_snprintf(MIN(nFull, pVfs->mxPathname), zFull, "%s\\%s",
                     sqlite3_data_directory, zRelative);
  }else{
    sqlite3_snprintf(MIN(nFull, pVfs->mxPathname), zFull, "%s", zRelative);
  }
  return SQLITE_OK;
#endif

#if !SQLITE_OS_WINCE && !SQLITE_OS_WINRT && !defined(__CYGWIN__)
  int nByte;
  void *zConverted;
  char *zOut;

  /*如果此路径名称以“/ X”，其中“X”是任何字母
??**性格，丢弃的路径最初的“/”。
  */
  if( zRelative[0]=='/' && sqlite3Isalpha(zRelative[1]) && zRelative[2]==':' ){
    zRelative++;
  }

  /* 这是奇怪，这里模拟一个IO错误，但实际上这只是
??**使用IO-误差的基础设施，以测试SQLite的处理这
??**功能失败。此功能可能会失败，如果，例如，该
??**当前工作目录已经取消链接。
  */
  SimulateIOError( return SQLITE_ERROR );
  if ( sqlite3_data_directory && !winIsVerbatimPathname(zRelative) ){
    /*
    ** 注：我们正在处理的相对路径名和数据
????**目录已设置。因此，用它作为基础
????**为相对路径名转换为绝对
????**1通过预先的数据目录和反斜杠。

    */
    sqlite3_snprintf(MIN(nFull, pVfs->mxPathname), zFull, "%s\\%s",
                     sqlite3_data_directory, zRelative);
    return SQLITE_OK;
  }
  zConverted = convertUtf8Filename(zRelative);
  if( zConverted==0 ){
    return SQLITE_IOERR_NOMEM;
  }
  if( isNT() ){
    LPWSTR zTemp;
    nByte = osGetFullPathNameW((LPCWSTR)zConverted, 0, 0, 0) + 3;
    zTemp = sqlite3_malloc( nByte*sizeof(zTemp[0]) );
    if( zTemp==0 ){
      sqlite3_free(zConverted);
      return SQLITE_IOERR_NOMEM;
    }
    osGetFullPathNameW((LPCWSTR)zConverted, nByte, zTemp, 0);
    sqlite3_free(zConverted);
    zOut = unicodeToUtf8(zTemp);
    sqlite3_free(zTemp);
  }
#ifdef SQLITE_WIN32_HAS_ANSI
  else{
    char *zTemp;
    nByte = osGetFullPathNameA((char*)zConverted, 0, 0, 0) + 3;
    zTemp = sqlite3_malloc( nByte*sizeof(zTemp[0]) );
    if( zTemp==0 ){
      sqlite3_free(zConverted);
      return SQLITE_IOERR_NOMEM;
    }
    osGetFullPathNameA((char*)zConverted, nByte, zTemp, 0);
    sqlite3_free(zConverted);
    zOut = sqlite3_win32_mbcs_to_utf8(zTemp);
    sqlite3_free(zTemp);
  }
#endif
  if( zOut ){
    sqlite3_snprintf(MIN(nFull, pVfs->mxPathname), zFull, "%s", zOut);
    sqlite3_free(zOut);
    return SQLITE_OK;
  }else{
    return SQLITE_IOERR_NOMEM;
  }
#endif
}

#ifndef SQLITE_OMIT_LOAD_EXTENSION
/*
** 接口打开共享库，找到切入点
**共享库中，并关闭共享库。
*/
/*
**接口打开共享库，找到切入点
**共享库中，并关闭共享库。
*/
static void *winDlOpen(sqlite3_vfs *pVfs, const char *zFilename){
  HANDLE h;
  void *zConverted = convertUtf8Filename(zFilename);
  UNUSED_PARAMETER(pVfs);
  if( zConverted==0 ){
    return 0;
  }
  if( isNT() ){
#if SQLITE_OS_WINRT
    h = osLoadPackagedLibrary((LPCWSTR)zConverted, 0);
#else
    h = osLoadLibraryW((LPCWSTR)zConverted);
#endif
  }
#ifdef SQLITE_WIN32_HAS_ANSI
  else{
    h = osLoadLibraryA((char*)zConverted);
  }
#endif
  sqlite3_free(zConverted);
  return (void*)h;
}
static void winDlError(sqlite3_vfs *pVfs, int nBuf, char *zBufOut){
  UNUSED_PARAMETER(pVfs);
  getLastErrorMsg(osGetLastError(), nBuf, zBufOut);
}
static void (*winDlSym(sqlite3_vfs *pVfs, void *pHandle, const char *zSymbol))(void){
  UNUSED_PARAMETER(pVfs);
  return (void(*)(void))osGetProcAddressA((HANDLE)pHandle, zSymbol);
}
static void winDlClose(sqlite3_vfs *pVfs, void *pHandle){
  UNUSED_PARAMETER(pVfs);
  osFreeLibrary((HANDLE)pHandle);
}
#else /* 如果SQLITE_OMIT_LOAD_EXTENSION定义： */
  #define winDlOpen  0
  #define winDlError 0
  #define winDlSym   0
  #define winDlClose 0
#endif


/*
** 写至NBUF字节随机进入Zbuf成立。
*/
static int winRandomness(sqlite3_vfs *pVfs, int nBuf, char *zBuf){
  int n = 0;
  UNUSED_PARAMETER(pVfs);
#if defined(SQLITE_TEST)
  n = nBuf;
  memset(zBuf, 0, nBuf);
#else
  if( sizeof(SYSTEMTIME)<=nBuf-n ){
    SYSTEMTIME x;
    osGetSystemTime(&x);
    memcpy(&zBuf[n], &x, sizeof(x));
    n += sizeof(x);
  }
  if( sizeof(DWORD)<=nBuf-n ){
    DWORD pid = osGetCurrentProcessId();
    memcpy(&zBuf[n], &pid, sizeof(pid));
    n += sizeof(pid);
  }
#if SQLITE_OS_WINRT
  if( sizeof(ULONGLONG)<=nBuf-n ){
    ULONGLONG cnt = osGetTickCount64();
    memcpy(&zBuf[n], &cnt, sizeof(cnt));
    n += sizeof(cnt);
  }
#else
  if( sizeof(DWORD)<=nBuf-n ){
    DWORD cnt = osGetTickCount();
    memcpy(&zBuf[n], &cnt, sizeof(cnt));
    n += sizeof(cnt);
  }
#endif
  if( sizeof(LARGE_INTEGER)<=nBuf-n ){
    LARGE_INTEGER i;
    osQueryPerformanceCounter(&i);
    memcpy(&zBuf[n], &i, sizeof(i));
    n += sizeof(i);
  }
#endif
  return n;
}


/*
**睡了一小会儿。返回的时间量睡。
*/
static int winSleep(sqlite3_vfs *pVfs, int microsec){
  sqlite3_win32_sleep((microsec+999)/1000);
  UNUSED_PARAMETER(pVfs);
  return ((microsec+999)/1000)*1000;
}

/*
** 以下变量，如果设置为非零值时，将被解释为
**秒自1970年以来的数目和用于设置的结果
**在测试过程中sqlite3OsCurrentTime（）。
*/
#ifdef SQLITE_TEST
int sqlite3_current_time = 0;  /* Fake system time in seconds since 1970. */
#endif

/*
** 查找当前时间（世界标准时间）。写入* piNow
**当前时间和日期的儒略日数乘以86_400_000。在
**换句话说，写入* piNow的毫秒数，因为朱利安
**中午在格林威治11月24日时代，根据BC4714
** proleptic公历。

**
** 如果成功，返回SQLITE_OK。如果返回SQLITE_ERROR时间和日期
**无法找到。
*/
static int winCurrentTimeInt64(sqlite3_vfs *pVfs, sqlite3_int64 *piNow){
  /* FILETIME structure is a 64-bit value representing the number of 
     100-nanosecond intervals since January 1, 1601 (= JD 2305813.5). 
  */
  FILETIME ft;
  static const sqlite3_int64 winFiletimeEpoch = 23058135*(sqlite3_int64)8640000;
#ifdef SQLITE_TEST
  static const sqlite3_int64 unixEpoch = 24405875*(sqlite3_int64)8640000;
#endif
  /* 2^32 - 避免使用LL和警告，在gcc */
  static const sqlite3_int64 max32BitValue = 
      (sqlite3_int64)2000000000 + (sqlite3_int64)2000000000 + (sqlite3_int64)294967296;

#if SQLITE_OS_WINCE
  SYSTEMTIME time;
  osGetSystemTime(&time);
  /* if SystemTimeToFileTime() fails, it returns zero. */
  if (!osSystemTimeToFileTime(&time,&ft)){
    return SQLITE_ERROR;
  }
#else
  osGetSystemTimeAsFileTime( &ft );
#endif

  *piNow = winFiletimeEpoch +
            ((((sqlite3_int64)ft.dwHighDateTime)*max32BitValue) + 
               (sqlite3_int64)ft.dwLowDateTime)/(sqlite3_int64)10000;

#ifdef SQLITE_TEST
  if( sqlite3_current_time ){
    *piNow = 1000*(sqlite3_int64)sqlite3_current_time + unixEpoch;
  }
#endif
  UNUSED_PARAMETER(pVfs);
  return SQLITE_OK;
}

/*
** 查找当前时间（世界标准时间）。写
**当前时间和日期的儒略日数为* prNow和
**返回0返回1，如果时间和日期不能被发现。
*/
static int winCurrentTime(sqlite3_vfs *pVfs, double *prNow){
  int rc;
  sqlite3_int64 i;
  rc = winCurrentTimeInt64(pVfs, &i);
  if( !rc ){
    *prNow = i/86400000.0;
  }
  return rc;
}

/*
** 这个想法是，此功能的工作等的组合
** GetLastError函数（）和的FormatMessage（）在Windows（或errno和
** strerror_r（）在Unix）。之后由OS返回一个错误
**功能，SQLite的调用这个函数Zbuf成立指向
** NBUF的缓冲区的字节。 OS层应该填充
**缓冲带空终止的UTF-8编码的错误信息
**描述过去的IO错误已调用内发生
**线程。

**
** 如果错误消息是太大，提供的缓冲区，
**它应该被截断。 xGetLastError的返回值
**是零，如果错误消息在缓冲器适合，或非零
**否则（如果消息被截断）。如果返回为非零，
**那么就没有必要包括NUL终止子字
**在输出缓冲器。
**
** 不供给的错误消息将没有任何不利的影响
**在SQLite的。这是罚款有一个实现永远
**返回一个错误信息：
**
** INT xGetLastError（sqlite3_vfs* PVFS，INT NBUF，字符* Zbuf成立）{
**断言（Zbuf成立[0]=='\0'）;
**返回0;
**}
**
**然而，如果一个错误消息被提供时，它会被并入
**由源码到错误消息提供给用户使用
** sqlite3_errmsg（），可能使IO错误容易调试.
*/
static int winGetLastError(sqlite3_vfs *pVfs, int nBuf, char *zBuf){
  UNUSED_PARAMETER(pVfs);
  return getLastErrorMsg(osGetLastError(), nBuf, zBuf);
}

/*
** 初始化和取消初始化操作系统接口.
*/
int sqlite3_os_init(void){
  static sqlite3_vfs winVfs = {
    3,                   /* iVersion */
    sizeof(winFile),     /* szOsFile */
    MAX_PATH,            /* mxPathname */
    0,                   /* pNext */
    "win32",             /* zName */
    0,                   /* pAppData */
    winOpen,             /* xOpen */
    winDelete,           /* xDelete */
    winAccess,           /* xAccess */
    winFullPathname,     /* xFullPathname */
    winDlOpen,           /* xDlOpen */
    winDlError,          /* xDlError */
    winDlSym,            /* xDlSym */
    winDlClose,          /* xDlClose */
    winRandomness,       /* xRandomness */
    winSleep,            /* xSleep */
    winCurrentTime,      /* xCurrentTime */
    winGetLastError,     /* xGetLastError */
    winCurrentTimeInt64, /* xCurrentTimeInt64 */
    winSetSystemCall,    /* xSetSystemCall */
    winGetSystemCall,    /* xGetSystemCall */
    winNextSystemCall,   /* xNextSystemCall */
  };

  /* 仔细检查了aSyscall[]数组已建成
??**正确。见票[bb3a86e890c8e96ab] */
  assert( ArraySize(aSyscall)==73 );

#ifndef SQLITE_OMIT_WAL
  /* 获取内存映射分配粒度 */
  memset(&winSysInfo, 0, sizeof(SYSTEM_INFO));
#if SQLITE_OS_WINRT
  osGetNativeSystemInfo(&winSysInfo);
#else
  osGetSystemInfo(&winSysInfo);
#endif
  assert(winSysInfo.dwAllocationGranularity > 0);
#endif

  sqlite3_vfs_register(&winVfs, 1);
  return SQLITE_OK; 
}

int sqlite3_os_end(void){ 
#if SQLITE_OS_WINRT
  if( sleepObj != NULL ){
    osCloseHandle(sleepObj);
    sleepObj = NULL;
  }
#endif
  return SQLITE_OK;
}

#endif /* SQLITE_OS_WIN */
