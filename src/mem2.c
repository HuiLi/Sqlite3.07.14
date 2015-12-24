/*
** 2007 August 15
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
** This file contains low-level memory allocation drivers for when
** SQLite will use the standard C-library malloc/realloc/free interface
** to obtain the memory it needs while adding lots of additional debugging
** information to each allocation in order to help detect and fix memory
** leaks and memory usage errors.
**
** This file contains implementations of the low-level memory allocation
** routines specified in the sqlite3_mem_methods object.
*/
/*����ļ��ǵײ���ڴ�����������򣬵�SQLite���ñ�׼��c-library malloc /realloc /free �ӿ�ȥ�������Ҫ���ڴ棬���һ���ÿһ�����丽���˶���
���Ŵ���Ϣ��Ϊ�˰��������ͼ���ڴ���©�Լ��ڴ����ô���
����ļ��еĵײ��ڴ�������ľ���ʵ������sqlite3_mem_methods�����С�*/
#include "sqliteInt.h"

/*
** This version of the memory allocator is used only if the
** SQLITE_MEMDEBUG macro is defined
*/
/*����汾���ڴ�����������ں�SQLITE_MEMDEBUG������ʱ*/
#ifdef SQLITE_MEMDEBUG

/*
** The backtrace functionality is only available with GLIBC
*/
/*���ݹ��ܽ�����GLIBC���п�*/
#ifdef __GLIBC__
  extern int backtrace(void**,int);
  extern void backtrace_symbols_fd(void*const*,int,int);
#else
# define backtrace(A,B) 1
# define backtrace_symbols_fd(A,B,C)
#endif
#include <stdio.h>

/*
** Each memory allocation looks like this:
**
**  ------------------------------------------------------------------------
**  | Title |  backtrace pointers |  MemBlockHdr |  allocation |  EndGuard |
**  ------------------------------------------------------------------------
**
** The application code sees only a pointer to the allocation.  We have
** to back up from the allocation pointer to find the MemBlockHdr.  The
** MemBlockHdr tells us the size of the allocation and the number of
** backtrace pointers.  There is also a guard word at the end of the
** MemBlockHdr.
*/
/*
ÿһ���ڴ����Ľṹ**
**  ------------------------------------------------------------------------
**  | ���⢘ |  ����·��ָ�� | �ڴ��HDR |  �ڴ����|  ������־
**  ------------------------------------------------------------------------
**
** ���Ӧ�ñ��뿴��ȥ����ָ���ڴ�����һ��ָ�롣���ԣ�����Ҫ���ڴ�ָ�뷵���ҵ�MemBlockHdr��
��MemBlockHdrָʾ������ڴ�Ĵ�С�ͻ���ָ��ı����Ŀ��ͬʱ������ṹ����һ��MemBlockHdr�����ؼ��֡�

Title������������ڴ棬�ڳ���ʱ���Դ�ӡ����
backtrace pointer���ڱ������ö�ջ
MemBlockHdr������Ƭ�ڴ�Ĺ����Լ�����δ�ͷŵ��ڴ��
allocation������ϲ�Ŀռ�
EndGuardβ�����ڱ������ڼ���ڴ汻�ȡ����и���HeadGaurd ����MemBlockHdr�С�  
  */
struct MemBlockHdr {
  i64 iSize;                          /* Size of this allocation     �ڴ����Ĵ�С*/
  struct MemBlockHdr *pNext, *pPrev;  /* Linked list of all unfreed memory    ָ����û�б��ͷŵ��ڴ�����ӱ�ṹ��*/
  char nBacktrace;                    /* Number of backtraces on this alloc     ���ڴ�����еķ���·���ı��*/
  char nBacktraceSlots;               /* Available backtrace slots     �����õ�·�����ز�*/
  u8 nTitle;                          /* Bytes of title; includes '\0'    �ո����ڵı�����ֽ�*/
  u8 eType;                           /* Allocation type code    �ڴ�������ͱ���*/
  int iForeGuard;                     /* Guard word for sanity     Ϊ�˽ṹ��ȷ���ı��*/
};

/*
** Guard words  �ؼ���
*/
#define FOREGUARD 0x80F5E153 /*ǰ�ؼ���*/
#define REARGUARD 0xE4676B53 /*��ؼ���*/
/*
** Number of malloc size increments to track.
*/
/*Ϊ��׷���ڴ�����С������������*/
#define NCSIZE  1000

/*
** All of the static variables used by this module are collected
** into a single structure named "mem".  This is to keep the
** static variables organized and to reduce namespace pollution
** when this module is combined with other in the amalgamation.
*/
/*
���еı�������ʹ�õľ�̬���������ۼ���һ�����ֽС�mem���Ľṹ���С���������Ϊ������֯�Ľ�����̬������
�Լ�Ϊ�˼��ٵ��������������Ľ��ʱ��������Ⱦ*/
static struct {
  
  /*
  ** Mutex to control access to the memory allocation subsystem.
  */
  /*�ڴ������ϵͳ�Ŀ��ƴ�ȡ�Ļ���*/
  sqlite3_mutex *mutex;

  /*
  ** Head and tail of a linked list of all outstanding allocations
  */
 /*���е�δ��������ڴ�����ӱ��ͷ��β*/
  struct MemBlockHdr *pFirst;
  struct MemBlockHdr *pLast;
  
  /*
  ** The number of levels of backtrace to save in new allocations.
  */
  /*���µ��ڴ�����У����л���·���ȼ���ˮƽ���ĺ���*/
  int nBacktrace;
  void (*xBacktrace)(int, int, void **);

  /*
  ** Title text to insert in front of each block
  */
  /*��ÿһ���ڴ��ǰ��������ı�*/
  int nTitle;        /* Bytes of zTitle to save.  Includes '\0' and padding  ���ڴ洢����zTitle����'\ 0'��������ݣ��洢��С*/
  char zTitle[100];  /* The title text ������洢�����ı� */

  /* 
  ** sqlite3MallocDisallow() increments the following counter.
  ** sqlite3MallocAllow() decrements it.
  */
  /*
  sqlite3MallocDisallow()����������ļ�����
  sqlite3MallocAllow()�Ǽ�������ļ�����  */
  int disallow; /* Do not allow memory allocation  �������ڴ����*/

  /*
  ** Gather statistics on the sizes of memory allocations.
  ** nAlloc[i] is the number of allocation attempts of i*8
  ** bytes.  i==NCSIZE is the number of allocation attempts for
  ** sizes more than NCSIZE*8 bytes.
  */
  /*
  �ռ��ڴ�����ͳ�����ݡ�Alloc[i]����ͼ��i*8�ֽڵ���ʽ�Ļ���ڴ����ı�š�
  i==NCSIZE����ͼ�ڴ�Ĵ�С����NCSIZE*8 bytes������µ��ڴ����ı�š�
  */
  int nAlloc[NCSIZE];      /* Total number of allocations ���е��ڴ���������*/
  int nCurrent[NCSIZE];    /* Current number of allocationsĿǰ������ڴ������*/
  int mxCurrent[NCSIZE];   /* Highwater mark for nCurrent Ŀǰ�����ڴ����nCurrent�����ֵ�ı�־*/

} mem;


/*
** Adjust memory usage statistics
*/
/*�����ڴ�ʹ�õ�ͳ������*/
static void adjustStats(int iSize, int increment){
  int i = ROUND8(iSize)/8;/*iSize����ͼ��õ��ڴ�Ĵ�С��˵������8���ֽڵ���ʽ��ȡ�ڴ�*/
  if( i>NCSIZE-1 ){
    i = NCSIZE - 1;/*�������ֵ����Ϊ���ֵ*/
  }
  if( increment>0 ){
    mem.nAlloc[i]++;
    mem.nCurrent[i]++;
    if( mem.nCurrent[i]>mem.mxCurrent[i] ){
      mem.mxCurrent[i] = mem.nCurrent[i];
    }
  }else{
    mem.nCurrent[i]--;
    assert( mem.nCurrent[i]>=0 );
  }
}

/*
** Given an allocation, find the MemBlockHdr for that allocation.
**
** This routine checks the guards at either end of the allocation and
** if they are incorrect it asserts.
*/
/*
����һ���ڴ棬ҪΪ����ڴ�����ҵ�MemBlockHdr����������鿴�Ƿ�������ڴ�Ľ��������ж������Ƿ��д�*/
static struct MemBlockHdr *sqlite3MemsysGetHeader(void *pAllocation){
  struct MemBlockHdr *p;
  int *pInt;
  u8 *pU8;
  int nReserve;

  p = (struct MemBlockHdr*)pAllocation;
  p--;
  assert( p->iForeGuard==(int)FOREGUARD );
  nReserve = ROUND8(p->iSize);
  pInt = (int*)pAllocation;
  pU8 = (u8*)pAllocation;
  assert( pInt[nReserve/sizeof(int)]==(int)REARGUARD );/*pInt[nReserve/sizeof(int)]��ָ�����飬ָ����pInt[]ָ�����һ���Ƿ���β�ؼ���*/
  /* This checks any of the "extra" bytes allocated due
  ** to rounding up to an 8 byte boundary to ensure 
  ** they haven't been overwritten.
  */
  /*���ڴﵽ8�ֽڵĽ��޵��κεġ����⡱�ı�������ֽڣ����������Ϊ�˱������Ǳ���д*/
  while( nReserve-- > p->iSize ) assert( pU8[nReserve]==0x65 );
  return p;
}

/*
** Return the number of bytes currently allocated at address p.
*/
/*����Ŀǰ�ڵ�ַp�з�����ֽڵ���Ŀ*/
static int sqlite3MemSize(void *p){
  struct MemBlockHdr *pHdr;
  if( !p ){
    return 0;/*���p==0���򷵻�0*/
  }
  pHdr = sqlite3MemsysGetHeader(p);/*����0,���ú���sqlite3MemsysGetHeader()��ȡp�Ĵ�С*/
  return pHdr->iSize;
}

/*
** Initialize the memory allocation subsystem.
*/
/*��ʼ���ڴ������ϵͳ*/
static int sqlite3MemInit(void *NotUsed){
  UNUSED_PARAMETER(NotUsed);
  assert( (sizeof(struct MemBlockHdr)&7) == 0 );
  if( !sqlite3GlobalConfig.bMemstat ){
    /* If memory status is enabled, then the malloc.c wrapper will already
    ** hold the STATIC_MEM mutex when the routines here are invoked. */
    /*����ڴ��ǲ������õ�״̬����ôSTATIC_MEM mutex ���������ﱻ���ã�malloc.c��װ���������ڴ澲̬����״̬*/
    mem.mutex = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MEM);
  }
  return SQLITE_OK;/*ָ����һ��׼�����������*/
}

/*
** Deinitialize the memory allocation subsystem.
*/
/*ȡ����ʼ���ڴ������ϵͳ*/
static void sqlite3MemShutdown(void *NotUsed){
  UNUSED_PARAMETER(NotUsed);
  mem.mutex = 0;
}

/*
** Round up a request size to the next valid allocation size.
*/
/*�ռ�������һ�����ô�С���ڴ����*/
static int sqlite3MemRoundup(int n){
  return ROUND8(n);
}

/*
** Fill a buffer with pseudo-random bytes.  This is used to preset
** the content of a new memory allocation to unpredictable values and
** to clear the content of a freed allocation to unpredictable values.
*/
/*��α����ֽ����һ������������������Ϊ�˶�һ�����ܹ�����ֵ���·�����ڴ�����ݽ���Ԥ���ã�
�Լ����޷�����ֵ���Ѿ��ͷŵ��ڴ�����ݽ��������
*/
static void randomFill(char *pBuf, int nByte){
  unsigned int x, y, r;
  x = SQLITE_PTR_TO_INT(pBuf);
  y = nByte | 1;
  while( nByte >= 4 ){
    x = (x>>1) ^ (-(x&1) & 0xd0000001);
    y = y*1103515245 + 12345;
    r = x ^ y;
    *(int*)pBuf = r;
    pBuf += 4;
    nByte -= 4;
  }
  while( nByte-- > 0 ){
    x = (x>>1) ^ (-(x&1) & 0xd0000001);
    y = y*1103515245 + 12345;
    r = x ^ y;
    *(pBuf++) = r & 0xff;
  }
}

/*
** Allocate nByte bytes of memory.
*/
/*����nByte���ֽ���Ϊ�ڴ�Ĵ�С*/
static void *sqlite3MemMalloc(int nByte){
  struct MemBlockHdr *pHdr;
  void **pBt;
  char *z;
  int *pInt;
  void *p = 0;
  int totalSize;
  int nReserve;
  sqlite3_mutex_enter(mem.mutex);
  assert( mem.disallow==0 );/*���disallow==0�����ܽ����ڴ����*/
  nReserve = ROUND8(nByte);
  totalSize = nReserve + sizeof(*pHdr) + sizeof(int) +
               mem.nBacktrace*sizeof(void*) + mem.nTitle;/*����һ���ڴ���ռ�õ��ܵĿռ��С*/
  /*�������ڴ����׼���׶�*/
  p = malloc(totalSize);
  if( p ){
    z = p;
    pBt = (void**)&z[mem.nTitle];/*����СΪnTitle������z���׵�ַ��pBt*/
    pHdr = (struct MemBlockHdr*)&pBt[mem.nBacktrace];/*�û���·������Ŀ��ָ��pBt����ʾ�ڴ��MemBlockHdr*/
    pHdr->pNext = 0;
    pHdr->pPrev = mem.pLast;
    if( mem.pLast ){
      mem.pLast->pNext = pHdr;
    }else{
      mem.pFirst = pHdr;
    }
    mem.pLast = pHdr;
    pHdr->iForeGuard = FOREGUARD;
    pHdr->eType = MEMTYPE_HEAP;
    pHdr->nBacktraceSlots = mem.nBacktrace;
    pHdr->nTitle = mem.nTitle;
    if( mem.nBacktrace ){
      /*���mem.nBacktrace��ֵΪ��*/
      void *aAddr[40];
      pHdr->nBacktrace = backtrace(aAddr, mem.nBacktrace+1)-1;
      memcpy(pBt, &aAddr[1], pHdr->nBacktrace*sizeof(void*));
      assert(pBt[0]);
      if( mem.xBacktrace ){
      /*如果有回调函数，那么执行回调函数*/
        mem.xBacktrace(nByte, pHdr->nBacktrace-1, &aAddr[1]);
      }
    }else{
      pHdr->nBacktrace = 0;
    }
    if( mem.nTitle ){
      memcpy(z, mem.zTitle, mem.nTitle);
    }
    pHdr->iSize = nByte;
    adjustStats(nByte, +1);
    pInt = (int*)&pHdr[1];
    pInt[nReserve/sizeof(int)] = REARGUARD;
    randomFill((char*)pInt, nByte);
    memset(((char*)pInt)+nByte, 0x65, nReserve-nByte);
    p = (void*)pInt;
  }
  sqlite3_mutex_leave(mem.mutex);
  return p; 
}

/*
** Free memory.
*/
/*�ͷ��ڴ�*/
static void sqlite3MemFree(void *pPrior){
  struct MemBlockHdr *pHdr;
  void **pBt;
  char *z;
  assert( sqlite3GlobalConfig.bMemstat || sqlite3GlobalConfig.bCoreMutex==0 
       || mem.mutex!=0 );
  pHdr = sqlite3MemsysGetHeader(pPrior);
  pBt = (void**)pHdr;
  pBt -= pHdr->nBacktraceSlots;
  sqlite3_mutex_enter(mem.mutex);
  if( pHdr->pPrev ){
    assert( pHdr->pPrev->pNext==pHdr );
    pHdr->pPrev->pNext = pHdr->pNext;
  }else{
    assert( mem.pFirst==pHdr );
    mem.pFirst = pHdr->pNext;
  }
  if( pHdr->pNext ){
    assert( pHdr->pNext->pPrev==pHdr );
    pHdr->pNext->pPrev = pHdr->pPrev;
  }else{
    assert( mem.pLast==pHdr );
    mem.pLast = pHdr->pPrev;
  }
  z = (char*)pBt;
  z -= pHdr->nTitle;
  adjustStats(pHdr->iSize, -1);
  randomFill(z, sizeof(void*)*pHdr->nBacktraceSlots + sizeof(*pHdr) +
                pHdr->iSize + sizeof(int) + pHdr->nTitle);
  free(z);
  sqlite3_mutex_leave(mem.mutex);  
}

/*
** Change the size of an existing memory allocation.
**
** For this debugging implementation, we *always* make a copy of the
** allocation into a new place in memory.  In this way, if the 
** higher level code is using pointer to the old allocation, it is 
** much more likely to break and we are much more liking to find
** the error.
*/
/*�ı����еķ�����ڴ�Ĵ�С��

Ϊ���ܹ����Դ����������ڴ��У�����ĳ���ڴ���䵽һ���µĵط���
�����ַ�ʽ������߲�ı�����������ָ��ָ��ɵ��ڴ���䣬�����п���ȥ�ж��Լ����ܹ��ҵ�����

*/
static void *sqlite3MemRealloc(void *pPrior, int nByte){
  struct MemBlockHdr *pOldHdr;
  void *pNew;
  assert( mem.disallow==0 );
  assert( (nByte & 7)==0 );     /* EV: R-46199-30249 */
  pOldHdr = sqlite3MemsysGetHeader(pPrior);
  pNew = sqlite3MemMalloc(nByte);/*��һ���ڴ���临�Ƶ��µĵĵط�*/
  if( pNew ){
    memcpy(pNew, pPrior, nByte<pOldHdr->iSize ? nByte : pOldHdr->iSize);
    if( nByte>pOldHdr->iSize ){
      randomFill(&((char*)pNew)[pOldHdr->iSize], nByte - pOldHdr->iSize);
    }
    sqlite3MemFree(pPrior);
  }
  return pNew;
}

/*
** Populate the low-level memory allocation function pointers in
** sqlite3GlobalConfig.m with pointers to the routines in this file.
*/
/*
���ײ���ڴ���亯��ָ���ַ��ָ������ļ��еĳ����ָ����䵽qlite3GlobalConfig.m�У��Լ��İ汾��*/
/*��sqlite3GlobalConfig.m������ļ�ָ������������ײ��ڴ���亯��ָ�루��ǰ�İ汾��*/
void sqlite3MemSetDefault(void){
  static const sqlite3_mem_methods defaultMethods = {
     sqlite3MemMalloc,
     sqlite3MemFree,
     sqlite3MemRealloc,
     sqlite3MemSize,
     sqlite3MemRoundup,
     sqlite3MemInit,
     sqlite3MemShutdown,
     0
  };
  sqlite3_config(SQLITE_CONFIG_MALLOC, &defaultMethods);
}

/*
** Set the "type" of an allocation.
*/
/*����һ���ڴ���������*/
void sqlite3MemdebugSetType(void *p, u8 eType){
  if( p && sqlite3GlobalConfig.m.xMalloc==sqlite3MemMalloc ){
    struct MemBlockHdr *pHdr;
    pHdr = sqlite3MemsysGetHeader(p);
    assert( pHdr->iForeGuard==FOREGUARD );
    pHdr->eType = eType;
  }
}

/*
** Return TRUE if the mask of type in eType matches the type of the
** allocation p.  Also return true if p==NULL.
**
** This routine is designed for use within an assert() statement, to
** verify the type of an allocation.  For example:
**
**     assert( sqlite3MemdebugHasType(p, MEMTYPE_DB) );
*/
/*����eType�е��ڴ�����͵�����ֵ���ڴ����p��������ͬʱ�������档���ң���p�ǿ�ֵʱҲ������ֵ��

 ������������Ǳ�����assert�������������еģ���Ϊ�˼������ڴ�����͵ġ��ٸ�������˵��assert( sqlite3MemdebugHasType(p, MEMTYPE_DB) );
*/
int sqlite3MemdebugHasType(void *p, u8 eType){
  int rc = 1;
  if( p && sqlite3GlobalConfig.m.xMalloc==sqlite3MemMalloc ){
    struct MemBlockHdr *pHdr;
    pHdr = sqlite3MemsysGetHeader(p);
    assert( pHdr->iForeGuard==FOREGUARD );         /* Allocation is valid 分配是有效的*/
    if( (pHdr->eType&eType)==0 ){
      rc = 0;
    }
  }
  return rc;
}

/*
** Return TRUE if the mask of type in eType matches no bits of the type of the
** allocation p.  Also return true if p==NULL.
**
** This routine is designed for use within an assert() statement, to
** verify the type of an allocation.  For example:
**
**     assert( sqlite3MemdebugNoType(p, MEMTYPE_DB) );
*/
/*
�����eType�е����͵�����루�ڴ����p�����Ͳ�ƥ�䣬��ô�ͷ�����ֵ��ͬʱ����pΪ��ֵʱҲ������ֵ��
�������������assert() �����еģ�Ϊ�˼���ڴ��������͵ġ����磬ssert( sqlite3MemdebugNoType(p, MEMTYPE_DB) );
*/
int sqlite3MemdebugNoType(void *p, u8 eType){
  int rc = 1;
  if( p && sqlite3GlobalConfig.m.xMalloc==sqlite3MemMalloc ){
    struct MemBlockHdr *pHdr;
    pHdr = sqlite3MemsysGetHeader(p);
    assert( pHdr->iForeGuard==FOREGUARD );         /* Allocation is valid �ڴ�������ܹ�ʹ�õ�*/
    if( (pHdr->eType&eType)!=0 ){
      rc = 0;
    }
  }
  return rc;
}

/*
** Set the number of backtrace levels kept for each allocation.
** A value of zero turns off backtracing.  The number is always rounded
** up to a multiple of 2.
*/
/*Ϊÿһ���ڴ���䣬���û��ݵȼ�����ȡ���ֵ�ǹرջ��ݣ��������ֵ����2�ı�����
*/
void sqlite3MemdebugBacktrace(int depth){
  if( depth<0 ){ depth = 0; }
  if( depth>20 ){ depth = 20; }
  depth = (depth+1)&0xfe;
  mem.nBacktrace = depth;
}

void sqlite3MemdebugBacktraceCallback(void (*xBacktrace)(int, int, void **)){
  mem.xBacktrace = xBacktrace;
}

/*
** Set the title string for subsequent allocations.
*/
/*Ϊ�������ڴ�������ñ����ַ���*/
void sqlite3MemdebugSettitle(const char *zTitle){
  unsigned int n = sqlite3Strlen30(zTitle) + 1;
  sqlite3_mutex_enter(mem.mutex);
  if( n>=sizeof(mem.zTitle) ) n = sizeof(mem.zTitle)-1;
  memcpy(mem.zTitle, zTitle, n);
  mem.zTitle[n] = 0;
  mem.nTitle = ROUND8(n);
  sqlite3_mutex_leave(mem.mutex);
}

void sqlite3MemdebugSync(){
  struct MemBlockHdr *pHdr;
  for(pHdr=mem.pFirst; pHdr; pHdr=pHdr->pNext){
    void **pBt = (void**)pHdr;
    pBt -= pHdr->nBacktraceSlots;
    mem.xBacktrace(pHdr->iSize, pHdr->nBacktrace-1, &pBt[1]);
  }
}

/*
** Open the file indicated and write a log of all unfreed memory 
** allocations into that log.
*/
/*�򿪱�ָ�����ļ���Ȼ�������е��ͷ��˵��ڴ���д����־��*/
void sqlite3MemdebugDump(const char *zFilename){
  FILE *out;
  struct MemBlockHdr *pHdr;
  void **pBt;
  int i;
  out = fopen(zFilename, "w");
  if( out==0 ){
    fprintf(stderr, "** Unable to output memory debug output log: %s **\n",
                    zFilename);
    return;
  }
  for(pHdr=mem.pFirst; pHdr; pHdr=pHdr->pNext){
    char *z = (char*)pHdr;
    z -= pHdr->nBacktraceSlots*sizeof(void*) + pHdr->nTitle;
    fprintf(out, "**** %lld bytes at %p from %s ****\n", 
            pHdr->iSize, &pHdr[1], pHdr->nTitle ? z : "???");
    if( pHdr->nBacktrace ){
      fflush(out);
      pBt = (void**)pHdr;
      pBt -= pHdr->nBacktraceSlots;
      backtrace_symbols_fd(pBt, pHdr->nBacktrace, fileno(out));
      fprintf(out, "\n");
    }
  }
  fprintf(out, "COUNTS:\n");
  for(i=0; i<NCSIZE-1; i++){
    if( mem.nAlloc[i] ){
      fprintf(out, "   %5d: %10d %10d %10d\n", 
            i*8, mem.nAlloc[i], mem.nCurrent[i], mem.mxCurrent[i]);
    }
  }
  if( mem.nAlloc[NCSIZE-1] ){
    fprintf(out, "   %5d: %10d %10d %10d\n",
             NCSIZE*8-8, mem.nAlloc[NCSIZE-1],
             mem.nCurrent[NCSIZE-1], mem.mxCurrent[NCSIZE-1]);
  }
  fclose(out);
}

/*
** Return the number of times sqlite3MemMalloc() has been called.
*/
/*����sqlite3MemMalloc()�����ѱ����õĴ�������Ŀ */
int sqlite3MemdebugMallocCount(){
  int i;
  int nTotal = 0;
  for(i=0; i<NCSIZE; i++){
    nTotal += mem.nAlloc[i];
  }
  return nTotal;
}


#endif /* SQLITE_MEMDEBUG */
