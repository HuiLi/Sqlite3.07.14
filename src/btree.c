/*
** 2004 April 6
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file implements a external (disk-based) database using BTrees.
** See the header comment on "btreeInt.h" for additional information.
** Including a description of file format and an overview of operation.
** ����������ļ�ʹ����ʵ���ⲿ(���ڴ���)���ݿ⡣�ڡ�btreeInt.h���ļ��в鿴�����ĸ�����Ϣ��
** �����ļ���ʽ�����Ͳ���������
*/
/*
���˹��䡿
����ļ�ʹ����ʵ���ⲿ(���ڴ���)���ݿ���"btreeInt.h"�ļ��︽��һЩ��Ҫ���õķ����� 
�����ļ���ʽ�������Ͳ����ĸ�����
*/
#include "btreeInt.h"

/*
** The header string that appears at the beginning of every
** SQLite database.
** ������"btreeInt.h"���ͷ�ļ������е�SQLite���ݿ�Ŀ�ͷ�ж�����֡�
*/
/*
���˹��䡿����һ�����ַ�����Ȼ��"btreeInt.h"��Ķ����һ��SQLiteͷ�ļ���ֵ����������ַ���

*/
static const char zMagicHeader[] = SQLITE_FILE_HEADER;

/*
** Set this global variable to 1 to enable tracing using the TRACE
** macro.
** ����������ȫ�ֱ�����ֵΪ1�����ú�TRACE����
*/
/*
���˹��䡿����һ����ָ�����ȫ�ֱ�������Ϊ1�������0���Ͷ���һ�����ε�׷������
����ֵΪ1��Ȼ����һ��׷�ٺ��������sqlite3BtreeTraceΪ�棬�ͽ���׷�١�����һ��ʼ��Ϊ��Ļ����ͽ���׷�١�
*/
#if 0
int sqlite3BtreeTrace=1;  /* True to enable tracing *//* �߼�ֵΪ���ʾ����׷�� *///���˹��䡿�����true����׷��
# define TRACE(X)  if(sqlite3BtreeTrace){printf X;fflush(stdout);}
#else
# define TRACE(X)
#endif

/*
** Extract a 2-byte big-endian integer from an array of unsigned bytes.
** But if the value is zero, make it 65536.
**
** This routine is used to extract the "offset to cell content area" value
** from the header of a btree page.  If the page size is 65536 and the page
** is empty, the offset should be 65536, but the 2-byte value stores zero.
** This routine makes the necessary adjustment to 65536.
*/
/*
**���޷����ֽ�������ȡ��һ��2�ֽڵĴ�����������ǣ������ֵΪ�㣬ʹ������65536��
�˳���������B��ҳ��ı�������ȡ��ƫ�Ƶ�Ԫ�������������ֵ��
���ҳ���С��65536��ҳ�ǿյģ�ƫ��Ӧ����65536����2���ֽڵ�ֵ�洢Ϊ�㡣
���������б�Ҫ�ĵ�����������65536��
*/
/*
���˹��䡿**��һ���޷����ֽ���������ȡ����cell�ĵ�ַ�Ĵ����������������ֵΪ0���ͽ�����ֵΪ65536��
�������ͨ��������һ��btreeҳ��ͷ��(�ײ�)����ȡ��ƫ�Ƹ�cell�������򡱡����ҳ�Ĵ�СΪ65536��ҳ
�Ĵ�СΪ�գ�ƫ�ƵĴ�СӦ��Ϊ65536�����Ǳ���cell�ĵ�ַ��ֵ�洢Ϊ0�����������б�Ҫ������65536��
*/

#define get2byteNotZero(X)  (((((int)get2byte(X))-1)&0xffff)+1)

#ifndef SQLITE_OMIT_SHARED_CACHE
/*
** A list of BtShared objects that are eligible for participation
** in shared cache.  This variable has file scope during normal builds,
** but the test harness needs to access it so we make it global for 
** test builds.
**
** Access to this variable is protected by SQLITE_MUTEX_STATIC_MASTER.
*/
/*
һϵ��BtShared������Ȩ�޷��ʹ����档��������ڴ���ʱ��һ���ļ������򣬵����Թ�����Ҫ��������
����Ϊ�˲������ǰ�����Ϊȫ�ֱ��������������SQLITE_MUTEX_STATIC_MASTER�����ı�����
*/
/*
���˹��䡿** btree�ṹ������Ҫ����һ��BtShared�ṹ���ýṹ��Ȩ�޷��ʹ����棬�����������ʱ��һ���ļ�������
���ǲ��Թ�����Ҫ��������������������Ϊ�˲��ԾͰ�����������ȫ�ֱ����������������ʱ������SQLITE_MUTEX_STATIC_MASTER����
*/
#ifdef SQLITE_TEST
BtShared *SQLITE_WSD sqlite3SharedCacheList = 0;
#else
static BtShared *SQLITE_WSD sqlite3SharedCacheList = 0;
#endif
#endif /* SQLITE_OMIT_SHARED_CACHE */

#ifndef SQLITE_OMIT_SHARED_CACHE
/*
** Enable or disable the shared pager and schema features.
**
** This routine has no effect on existing database connections.
** The shared cache setting effects only future calls to
** sqlite3_open(), sqlite3_open16(), or sqlite3_open_v2().
*/
/*
���û���ù����ҳ��ģʽ���ص㡣�����������е����ݿ�����û��Ӱ�졣
���������ý�Ӱ�콫������sqlite3_open������sqlite3_open16��������sqlite3_open_v2������
*/
/*
���˹��䡿**�������ֹ����ҳ��ģʽ�������������û��Ӱ�쵽�ִ�����ݿ�����ӣ�
����������ֻӰ��δ���ĵ���sqlite3_open(), sqlite3_open16(), or sqlite3_open_v2()

*/
int sqlite3_enable_shared_cache(int enable){
  sqlite3GlobalConfig.sharedCacheEnabled = enable;
  return SQLITE_OK;
}
#endif



#ifdef SQLITE_OMIT_SHARED_CACHE
  /*
  ** The functions querySharedCacheTableLock(), setSharedCacheTableLock(),
  ** and clearAllSharedCacheTableLocks()
  ** manipulate entries in the BtShared.pLock linked list used to store
  ** shared-cache table level locks. If the library is compiled with the
  ** shared-cache feature disabled, then there is only ever one user
  ** of each BtShared structure and so this locking is not necessary. 
  ** So define the lock related functions as no-ops.
  */
  /*
����querySharedCacheTableLock������setSharedCacheTableLock������clearAllSharedCacheTableLocks����
��������BtShared.pLock�еļ�¼���������洢�����������������ڹ����湦�ܽ��õ�����±��룬
��ôÿ��BtShared�ṹ����Զֻ����һ���û�����˸�������û�б�Ҫ�ġ�
���Զ�������صĹ���Ϊ�ղ�����
*/
  /*
���˹��䡿�ֱ����˺���querySharedCacheTableLock����������洢�����������
setSharedCacheTableLock���������ù����������
clearAllSharedCacheTableLocks������������й����������
ͨ����������BtShared.pLock�еļ�¼���������洢�����������������ڹ����湦�ܽ��õ�����±��룬
��ôÿ��BtShared�ṹ����Զֻ����һ���û�����˸�������û�б�Ҫ�ġ����Զ�������صĹ���Ϊ�ղ�����
*/
  #define querySharedCacheTableLock(a,b,c) SQLITE_OK  //��������ѯ���������
  #define setSharedCacheTableLock(a,b,c) SQLITE_OK    //���������ù��������
  #define clearAllSharedCacheTableLocks(a)            //������ɾ�����й��������
  #define downgradeAllSharedCacheTableLocks(a)        //���������͹�������������ȼ�
  #define hasSharedCacheTableLock(a,b,c,d) 1          //�������ж��Ƿ��й����������1��������
  #define hasReadConflicts(a, b) 0                    //�������ж���ͻ
#endif

#ifndef SQLITE_OMIT_SHARED_CACHE

#ifdef SQLITE_DEBUG
/*
**** This function is only used as part of an assert() statement. ***
**
** Check to see if pBtree holds the required locks to read or write to the 
** table with root page iRoot.   Return 1 if it does and 0 if not.
**
** For example, when writing to a table with root-page iRoot via 
** Btree connection pBtree:
**
**    assert( hasSharedCacheTableLock(pBtree, iRoot, 0, WRITE_LOCK) );
**
** When writing to an index that resides in a sharable database, the 
** caller should have first obtained a lock specifying the root page of
** the corresponding table. This makes things a bit more complicated,
** as this module treats each table as a separate structure. To determine
** the table corresponding to the index being written, this
** function has to search through the database schema.
**
** Instead of a lock on the table/index rooted at page iRoot, the caller may
** hold a write-lock on the schema table (root page 1). This is also
** acceptable.
*/
/*
�˹��ܽ���Ϊassert��������һ���֡� ���pBtree(Btree�������ľ��)�Ƿ���������������ȡ��д��
��ĸ�ҳiRoot��������򷵻�1�����򷵻�0�����磬ͨ��B������pBtree��д����ҳiRoot��
assert��hasSharedCacheTableLock��pBtree��iRoot��0��WRITE_LOCK����;
дפ���ڹ������ݿ�����ʱ��������Ӧ���Ȼ��һ����ָ���ĸ�ҳ��Ӧ�ı���Ϊ��ģ�齫ÿ������
Ϊһ�������Ľṹ����ʹ�������е㸴�ӡ�Ϊ��ȷ����Ӧ�ڸ��������ĸ���д�룬������������ѱ�
���ݿ�ܹ����������ܳ��мܹ����е�һ��д���������Ǹ�ֲ��ҳ��iRoot�ϵı���������ϵ�����
��Ҳ�ǿ��Խ��ܵġ�
*/
/*
���˹��䡿���������������Ϊһ��assert()����һ���֡����pBtreeӵ�������������дiRoot�����ҳ�档
���������򷵻�1�����򷵻�0��
����,��д����ҳiRootͨ��Btree����pBtree:
assert(hasSharedCacheTableLock(WRITE_LOCK pBtree iRoot 0));
����дһ������,פ���ڹ������ݿ�,������Ӧ�����Ȼ��һ����ָ����Ӧ�ĸ�ҳ��
��ʹ��������Ӹ���,��Ϊ���ģ���ÿ������Ϊһ�������Ľṹ��
ȷ��д��������,��������������ݿ�ģʽ�������Ǹ�ֲ��ҳ��iRoot�ϵı���������ϵ���,������
�ڽ���һ��д��ģʽ��(��1ҳ)����Ҳ�ǿ��Խ��ܵġ�
*/
static int hasSharedCacheTableLock(
  Btree *pBtree,         /* Handle that must hold lock *������Ҫ������*/ /*���˹��䡿b��ҳ���������*/
  Pgno iRoot,            /* Root page of b-tree  *B�����ĸ�ҳ*/   /*���˹��䡿��Btree�ĸ�ҳҳ��*/
  int isIndex,           /* True if iRoot is the root of an index b-tree *���iRoot��Brtee�����ĸ�ҳ��Ϊtrue*/ /*���˹��䡿����B���ĸ�ҳ*/
  int eLockType          /* Required lock type (READ_LOCK or WRITE_LOCK) ��Ҫ������*/  /*���˹��䡿��Ҫ�����ͣ���������д����*/
){
  Schema *pSchema = (Schema *)pBtree->pBt->pSchema;
  Pgno iTab = 0;
  BtLock *pLock;

  /* If this database is not shareable, or if the client is reading
  ** and has the read-uncommitted flag set, then no lock is required. 
  ** Return true immediately.
  */
  /*
  ��������ݿ��Ƿǹ���ģ���������ͻ������ڶ����Ҿ��ж�δ�ύ�ı�־���ã�����Ҫ����
  ��������true��
  */
   /*
  ���������ݿⲻ�ǿɹ����,���߿ͻ����ڶ�,��δ�ύ�ı������,Ȼ����Ҫ������������true��
  */
  
  if( (pBtree->sharable==0)
   || (eLockType==READ_LOCK && (pBtree->db->flags & SQLITE_ReadUncommitted))
  ){
    return 1;
  }

  /* If the client is reading  or writing an index and the schema is
  ** not loaded, then it is too difficult to actually check to see if
  ** the correct locks are held.  So do not bother - just return true.
  ** This case does not come up very often anyhow.
  */
  /*����û����ڶ�ȡ��д��������ʱ��(isIndex)��ģʽû�м���(!pSchema)��
  ��ʱȥ�ж�pBtree�Ƿ������ȷ�����ǳ�����((pSchema->flags&DB_SchemaLoaded)==0)��
  ���ԣ���Ҫ���󣬽�������true��  �Һã���������ֵĺ��١�*/
  /*
���˹��䡿����ͻ����Ƕ���дһ������ģʽ�����Ǽ���,��ô��ʵ�����Ǻ��Ѽ����ȷ��������˲�Ҫ����,����true���С�
������������������֡�
  */
  if( isIndex && (!pSchema || (pSchema->flags&DB_SchemaLoaded)==0) ){
    return 1; //������
  }

  /* Figure out the root-page that the lock should be held on. For table
  ** b-trees, this is just the root page of the b-tree being read or
  ** written. For index b-trees, it is the root page of the associated
  ** table.  */
  /*�ҳ���ҳӦ�ó��е��������ڱ�B���� B���ĸ�ҳ����ȡ��д��(else iRoot)������B��������������ĸ�ҳ��*/
  /*������
  ** �����Ӧ�ó������ĸ�ҳ���Ա�B��������B+-tree������ֻ�����ڱ�������д��B���ĸ�ҳ��
  ** ��������B�����������Ӧ��ĸ�ҳ��
  */
  /*
   ���˹��䡿�ҳ���ҳӦ�ó��е��������ڱ�b��,��ֻ��b���ĸ�ҳ������д������b��,���ĸ�ҳ��ر�
  */
  if( isIndex ){
    HashElem *p;
    for(p=sqliteHashFirst(&pSchema->idxHash); p; p=sqliteHashNext(p)){
      Index *pIdx = (Index *)sqliteHashData(p);
      if( pIdx->tnum==(int)iRoot ){
        iTab = pIdx->pTable->tnum;
      }
    }
  }else{
    iTab = iRoot;
  }

  /* Search for the required lock. Either a write-lock on root-page iTab, a 
  ** write-lock on the schema table, or (if the client is reading) a
  ** read-lock on iTab will suffice. Return 1 if any of these are found.  */
  /*�����������(pLock)���ڸ�ҳiTAB�ϵ�д��(pLock->eLock==WRITE_LOCK) ���ڼܹ����ϵ�д��( pLock->iTable==1)��������ͻ����ڶ���ITAB�ϵĶ���
  ���㹻��(pLock->eLock>=eLockType,eLockTypeΪ����Ҫ����)���������������־ͷ���1��
  */
  /*
  ���˹��䡿Ѱ�����������pLock�����ڸ�ҳiTab�ϵ�д��(pLock->eLock==WRITE_LOCK) ��
  �ڼܹ����ϵ�д��( pLock->iTable==1)��������ͻ����ڶ���iTab�ϵĶ���
  ���㹻��(pLock->eLock>=eLockType,eLockTypeΪ����Ҫ����)�������Щ���ַ���1��
  */
  for(pLock=pBtree->pBt->pLock; pLock; pLock=pLock->pNext){
    if( pLock->pBtree==pBtree 
     && (pLock->iTable==iTab || (pLock->eLock==WRITE_LOCK && pLock->iTable==1))
     && pLock->eLock>=eLockType 
    ){
      return 1;
    }
  }

  /* Failed to find the required lock. δ��ѯ����Ӧ�����򷵻�0 *//*���˹��䡿û���ҵ��������,�򷵻�0*/
  return 0;
}
#endif /* SQLITE_DEBUG */  //���Գ���SQLITE_DEBUG 

#ifdef SQLITE_DEBUG
/*
**** This function may be used as part of assert() statements only. ****
**
** Return true if it would be illegal for pBtree to write into the
** table or index rooted at iRoot because other shared connections are
** simultaneously reading that same table or index.
**
** It is illegal for pBtree to write if some other Btree object that
** shares the same BtShared object is currently reading or writing
** the iRoot table.  Except, if the other Btree object has the
** read-uncommitted flag set, then it is OK for the other object to
** have a read cursor.
**
** For example, before writing to any part of the table or index
** rooted at page iRoot, one should call:
**
**    assert( !hasReadConflicts(pBtree, iRoot) );
*/
/*
** �����Ϊ�����Ĺ�������ͬʱ��ȡ��ͬ�ı��������������pBtreeд��ȥ��
** ����iRoot�ϵ������ǷǷ��ģ��򷵻�true.
���һЩ������B����������ͬ��BtShared����BtShared�������ڶ�ȡ��д���iRoot��
(p->pgnoRoot==iRoot )����ʱpBtree��д���ǷǷ��ġ����⣬��������B��������ж�δ�ύ��־����
(SQLITE_ReadUncommitted)������ȷ������������һ����ָ�롣
���磬��д��ҳ�ϵı������֮ǰ��Ӧ�õ��ã�
	assert����hasReadConflicts��pBtree��iRoot����;
*/
/*
���˹��䡿����ʹ���������ֻassert()����һ���֡��������Ϊ������������ͬʱ��ȡͬһ�����������
���·Ƿ���pBtreeд��ȥ�ı���iRoot�ϵ����������һЩ������B����������ͬ��BtShared����
BtShared�������ڶ�ȡ��д���iRoot��(p->pgnoRoot==iRoot )����ʱpBtree��д���ǷǷ��ġ�
���⣬���������Btree�����δ�ύ��Ǽ�,��ô������Ϊ����������һ����ָ�룬����true��
����,��д��ҳ�ϵı��������һ���ָ�ҳ��iRoot֮ǰ��
Ӧ�õ��ã�assert( !hasReadConflicts(pBtree, iRoot) );
*/
static int hasReadConflicts(Btree *pBtree, Pgno iRoot){
  BtCursor *p;
  for(p=pBtree->pBt->pCursor; p; p=p->pNext){
    if( p->pgnoRoot==iRoot 
     && p->pBtree!=pBtree
     && 0==(p->pBtree->db->flags & SQLITE_ReadUncommitted)
    ){
      return 1;
    }
  }
  return 0;
}
#endif    /* #ifdef SQLITE_DEBUG */

/*
** Query to see if Btree handle p may obtain a lock of type eLock 
** (READ_LOCK or WRITE_LOCK) on the table with root-page iTab. Return
** SQLITE_OK if the lock may be obtained (by calling
** setSharedCacheTableLock()), or SQLITE_LOCKED if not.
*/
/*
��ѯ���ж�B�����p�Ƿ�����iTab��ҳ�ı��ϻ�ȡeLock���͵�����READ_LOCK��WRITE_LOCK��(eLock==READ_LOCK || eLock==WRITE_LOCK )��
���ͨ������ setSharedCacheTableLock���������Ի����������SQLITE_OK�����򷵻�SQLITE_LOCKED��
**�������鿴Btree���p�Ƿ��ھ��и�ҳiTab�ı��ϻ����eLock���ͣ�������д����������
** ���ͨ������setSharedCacheTableLock()�������������SQLITE_OK,���򷵻�SQLITE_LOCKED.
*/
/*
���˹��䡿��ѯ���ж�B�����p�Ƿ�����iTab��ҳ�ı��ϻ�ȡeLock���͵���������������д����
���ͨ������ setSharedCacheTableLock���������Ի����������SQLITE_OK�����򷵻�SQLITE_LOCKED��
*/
static int querySharedCacheTableLock(Btree *p, Pgno iTab, u8 eLock){//����һ����ѯ����������ĺ���
  BtShared *pBt = p->pBt;
  BtLock *pIter;    //������pIterB���ϵ���ָ�����

  assert( sqlite3BtreeHoldsMutex(p) );  
  assert( eLock==READ_LOCK || eLock==WRITE_LOCK );
  assert( p->db!=0 );
  assert( !(p->db->flags&SQLITE_ReadUncommitted)||eLock==WRITE_LOCK||iTab==1 );
  
  /* If requesting a write-lock, then the Btree must have an open write
  ** transaction on this file. And, obviously, for this to be so there 
  ** must be an open write transaction on the file itself.
  ** �����Ҫһ��д������ôB��������һ�����ŵ�д������Ȼ��Ϊ�˴ﵽ����Ч��
  ** �ļ����������һ�����ŵ�д����
  */
  /*
  ���˹��䡿�������һ��д��,��ôBtree������һ��������ļ���д����
  ������,���Ǳ�����һ�����ŵ�д�����ļ�����  */
  assert( eLock==READ_LOCK || (p==pBt->pWriter && p->inTrans==TRANS_WRITE) );
  assert( eLock==READ_LOCK || pBt->inTransaction==TRANS_WRITE );
  
  /* This routine is a no-op if the shared-cache is not enabled */
  /*���δ���ù����棬�����������һ���ղ���*/ /*���˹��䡿���û�����ù����棬���������һ���ղ���*/
  if( !p->sharable ){    //���д������SQLITE_OK
    return SQLITE_OK;
  }

  /* If some other connection is holding an exclusive lock, the
  ** requested lock may not be obtained.
  ** ���������һЩ�������������ڳ��л�����(pBt->btsFlags & BTS_EXCLUSIVE)!=0,��ô�޷���������������
  */
  /*
���˹��䡿����������ӳ���������,���ܲ����������������  */
  if( pBt->pWriter!=p && (pBt->btsFlags & BTS_EXCLUSIVE)!=0 ){
    sqlite3ConnectionBlocked(p->db, pBt->pWriter->db);
    return SQLITE_LOCKED_SHAREDCACHE;
  }

  for(pIter=pBt->pLock; pIter; pIter=pIter->pNext){
    /* The condition (pIter->eLock!=eLock) in the following if(...) 
    ** statement is a simplification of:
    **
    **   (eLock==WRITE_LOCK || pIter->eLock==WRITE_LOCK)
    **
    ** since we know that if eLock==WRITE_LOCK, then no other connection
    ** may hold a WRITE_LOCK on any table in this file (since there can
    ** only be a single writer).
    */
    /*
	** ����������(pIter->eLock!=eLock)��if�����(eLock==WRITE_LOCK || pIter->eLock==WRITE_LOCK)�ļ򻯡�
	** ��Ϊ����֪�������eLock== WRITE_LOCK����û���������ӿ��ܳ�������ļ����κα��WRITE_LOCK
	**����Ϊֻ��һ��д���̣���
	*/
	  /*
	���˹��䡿�������������pIter-> eLock��= eLock�������...��   
    �����һ���򻯣�        
    ��eLock== WRITE_LOCK|| pIter-> eLock== WRITE_LOCK����
    ��Ϊ����֪�������eLock== WRITE_LOCK��Ȼ��û����������
	���ܳ��ж�����ļ��е��κα��WRITE_LOCK����Ϊֻ��һ��д���̣���
    	*/
    assert( pIter->eLock==READ_LOCK || pIter->eLock==WRITE_LOCK );
    assert( eLock==READ_LOCK || pIter->pBtree==p || pIter->eLock==READ_LOCK);
    if( pIter->pBtree!=p && pIter->iTable==iTab && pIter->eLock!=eLock ){
      sqlite3ConnectionBlocked(p->db, pIter->pBtree->db);
      if( eLock==WRITE_LOCK ){
        assert( p==pBt->pWriter );
        pBt->btsFlags |= BTS_PENDING;
      }
      return SQLITE_LOCKED_SHAREDCACHE; //���ع�������
    }
  }
  return SQLITE_OK;
}
#endif /* !SQLITE_OMIT_SHARED_CACHE */

#ifndef SQLITE_OMIT_SHARED_CACHE
/*
** Add a lock on the table with root-page iTable to the shared-btree used
** by Btree handle p. Parameter eLock must be either READ_LOCK or 
** WRITE_LOCK.
**
** This function assumes the following:
**
**   (a) The specified Btree object p is connected to a sharable
**       database (one with the BtShared.sharable flag set), and
**
**   (b) No other Btree objects hold a lock that conflicts
**       with the requested lock (i.e. querySharedCacheTableLock() has
**       already been called and returned SQLITE_OK).
**
** SQLITE_OK is returned if the lock is added successfully. SQLITE_NOMEM 
** is returned if a malloc attempt fails.
**������ ͨ��B�����p�ڸ�ҳiTable�ı��������������B���ϡ�����eLock������READ_LOCK�� WRITE_LOCK��
** �˹��ܼٶ����£�
**  (a)ָ����B������p�����ӵ�һ���ɹ������ݿ⣨һ����BtShare�ɹ���ı�־���ã����Լ�
**  (b)û������B�����������������������ͻ����������querySharedCacheTableLock����
**     �Ѿ������ò��ҷ���SQLITE_OK��
** ����ɹ�������������򷵻�SQLITE_OK������ڴ����ʧ�ܣ��򷵻�SQLITE_NOMEM.
*/
/*
ͨ��B�����p�ڸ�ҳiTable�ı��������������B���ϡ� ����eLock������READ_LOCK�� WRITE_LOCK��
�˹��ܼٶ���
��һ��ָ����B������p�����ӵ�һ���ɹ������ݿ⣨һ����BtShare�ɹ���ı�־���ã����Լ�
������û������B�����������������������ͻ����������querySharedCacheTableLock����
�Ѿ������ò��ҷ���SQLITE_OK����������ɹ���ӷ���SQLITE_OK�����mallocʧ���򷵻�SQLITE_NOMEM��
*/
/*
���˹��䡿ͨ��B�����p�ڸ�ҳiTable�ı��������������B���ϡ� 
����eLock������READ_LOCK�� WRITE_LOCK��
���������������:
��a������ָ��B������p�����ӵ�һ���������ݿ�(һ��BtShared�����ù����־)
��b��û������B��������е��������������ͻ(��querySharedCacheTableLock()
�Ѿ������ò�����SQLITE_OK)��
�������ӳɹ��򷵻�SQLITE_OK�����malloc����ʧ���򷵻�SQLITE_NOMEM��
*/
static int setSharedCacheTableLock(Btree *p, Pgno iTable, u8 eLock){//���ù���������ĺ���
  BtShared *pBt = p->pBt;
  BtLock *pLock = 0;
  BtLock *pIter;

  assert( sqlite3BtreeHoldsMutex(p) );
  assert( eLock==READ_LOCK || eLock==WRITE_LOCK );
  assert( p->db!=0 );

  /* A connection with the read-uncommitted flag set will never try to
  ** obtain a read-lock using this function. The only read-lock obtained
  ** by a connection in read-uncommitted mode is on the sqlite_master 
  ** table, and that lock is obtained in BtreeBeginTrans(). 
  **���������ж�δ�ύ������õ����Ӳ��ܳ��������������ȡ��������SQLITE_MASTER���ж�δ�ύģʽ��
  ** ���ܻ��ֻ������������BtreeBeginTrans�����л������*/
  /*
  �ж�δ�ύ��־�����Ӳ���ͨ���ù��ܻ�ö�������SQLITE_MASTER���ж�δ�ύģʽ�²��ܻ��ֻ������
  ������BtreeBeginTrans�����л������
  */
  /*
  ���˹��䡿�ж�δ�ύ��־����Զ���᳢��ʹ�����������ö�����Ψһͨ�����Ӽ�sqlite_master���δ�ύģʽ,
  �Ż������BtreeBeginTrans()��
  */
  assert( 0==(p->db->flags&SQLITE_ReadUncommitted) || eLock==WRITE_LOCK );

  /* This function should only be called on a sharable b-tree after it 
  ** has been determined that no other b-tree holds a conflicting lock.  
  ** ��û��������B������һ����ͻ����֮�󣬲�����һ�������B���ϵ������������
  */
  /*
 ���˹��䡿��ȷ��û������B�����г�ͻ�������������ֻ�ܵ���һ���ɷ����B����
	  */
  assert( p->sharable );//ָ�����b��
  assert( SQLITE_OK==querySharedCacheTableLock(p, iTable, eLock) );/*����*/

  /* First search the list for an existing lock on this table. */
  /*������ ���������ڱ����Ѵ��ڵ����б�   */
   /*
  ���˹��䡿�����������е������������б��ϡ�
    */
  for(pIter=pBt->pLock; pIter; pIter=pIter->pNext){
    if( pIter->iTable==iTable && pIter->pBtree==p ){
      pLock = pIter;
      break;
    }
  }

  /* If the above search did not find a BtLock struct associating Btree p
  ** with table iTable, allocate one and link it into the list.
  */
  /*
  ** ������������û���ҵ�( !pLock)BtLock�ṹ�������ڱ�iTable�ϵ�B��p����ô�ͷ���
  ** һ��( pLock = (BtLock *)sqlite3MallocZero(sizeof(BtLock)); )���������ӵ��б��С�
  */
  
  if( !pLock ){ //���˹��䡿����ڱ�iTable�ϵ�B��pû���ҵ���������
    pLock = (BtLock *)sqlite3MallocZero(sizeof(BtLock));//���˹��䡿���������������ӵ��б���
    if( !pLock ){
      return SQLITE_NOMEM;
    }
    pLock->iTable = iTable;
    pLock->pBtree = p;
    pLock->pNext = pBt->pLock;
    pBt->pLock = pLock;
  }

  /* Set the BtLock.eLock variable to the maximum of the current lock
  ** and the requested lock. This means if a write-lock was already held
  ** and a read-lock requested, we don't incorrectly downgrade the lock.
  ** ��BtLock.eLock��������Ϊ��ǰ��������������������ֵ����˼������Ѿ�����һ��д��
  ** ��������һ�����������ǽ�����������ļ���
  */
  /*��BtLock.eLock��������Ϊ��ǰ�������������������� ֵ������ζ�ţ�
  ����Ѿ�����һ��д��  ������һ��������������ȷ�ؽ�������*/
   /*
���˹��䡿����BtLock.eLock����Ϊ��ǰ��������������������ֵ
����ζ������Ѿ�����һ��д����һ����������������ʵ��ؽ�������
  */
  assert( WRITE_LOCK>READ_LOCK );
  if( eLock>pLock->eLock ){
    pLock->eLock = eLock;
  }

  return SQLITE_OK;
}
#endif /* !SQLITE_OMIT_SHARED_CACHE */

#ifndef SQLITE_OMIT_SHARED_CACHE
/*
** Release all the table locks (locks obtained via calls to
** the setSharedCacheTableLock() procedure) held by Btree object p.
**
** This function assumes that Btree p has an open read or write 
** transaction. If it does not, then the BTS_PENDING flag
** may be incorrectly cleared.
*/
/*
�ͷ�����B������P�����еı�����ͨ������setSharedCacheTableLock����
������õ��������˺����ٶ�B��P��һ�����ŵĶ���д��������
���û�У���BTS_PENDING��־���ܱ�����������pBt->btsFlags &= ~(BTS_EXCLUSIVE|BTS_PENDING);
*/
/*
���˹��䡿�ͷ����еı���(���Ļ����ͨ������setSharedCacheTableLock()�Ĺ���)Btree������е�p��
�˺����ٶ�B��P��һ�����ŵĶ���д�����������û�У���ôBTS_PENDING��־���ܱ�����ȷ�������
*/
static void clearAllSharedCacheTableLocks(Btree *p){
  BtShared *pBt = p->pBt;
  BtLock **ppIter = &pBt->pLock;

  assert( sqlite3BtreeHoldsMutex(p) );
  assert( p->sharable || 0==*ppIter );
  assert( p->inTrans>0 );

  while( *ppIter ){
    BtLock *pLock = *ppIter;
    assert( (pBt->btsFlags & BTS_EXCLUSIVE)==0 || pBt->pWriter==pLock->pBtree );
    assert( pLock->pBtree->inTrans>=pLock->eLock );
    if( pLock->pBtree==p ){
      *ppIter = pLock->pNext;
      assert( pLock->iTable!=1 || pLock==&p->lock );
      if( pLock->iTable!=1 ){
        sqlite3_free(pLock);
      }
    }else{
      ppIter = &pLock->pNext;
    }
  }

  assert( (pBt->btsFlags & BTS_PENDING)==0 || pBt->pWriter );
  if( pBt->pWriter==p ){
    pBt->pWriter = 0;
    pBt->btsFlags &= ~(BTS_EXCLUSIVE|BTS_PENDING);
  }else if( pBt->nTransaction==2 ){
    /* This function is called when Btree p is concluding its 
    ** transaction. If there currently exists a writer, and p is not
    ** that writer, then the number of locks held by connections other
    ** than the writer must be about to drop to zero. In this case
    ** set the BTS_PENDING flag to 0.
    **
    ** If there is not currently a writer, then BTS_PENDING must
    ** be zero already. So this next line is harmless in that case.
	** ���������B��p���ڽ�������ʱ�����á������ǰ����һ��д������
	** p�����Ǹ�д������ô���ӽ��̶�����д���̳��е�����������Լ�����㡣
	** ���������������BTS_PENDING��ǩΪ0.
    */
    /*��B��p��������ʱ���ú��������á�����е�ǰ����һ��д����p�����Ǹ�д����
    ��ô���ӽ��̶�����д���̳��е�����������Լ�����㡣����������£�����BTS_PENDING��־Ϊ0��
    ���Ŀǰ��û��һ��д������ôBTS_PENDINGΪ�㡣��ˣ���һ��(pBt->btsFlags &= ~BTS_PENDING;)�������������û��Ӱ��ġ�*/
	   /*
	���˹��䡿��B��p��������ʱ�������ô˺��������Ŀǰ����һ��д���񣬲���p�����Ǹ�д������ô���ӽ��̶�����
	д���̳��е�����������Լ�����㡣����������£�����BTS_PENDING��־Ϊ0��
	���Ŀǰ��û��һ��д������ôBTS_PENDING����Ϊ�㡣��ˣ���һ��(pBt->btsFlags &= ~BTS_PENDING;)�������������û��Ӱ��ġ�
	*/
    pBt->btsFlags &= ~BTS_PENDING;
  }
}

/*
** This function changes all write-locks held by Btree p into read-locks.
*/
/*���������B��p���е�����д���ı�Ϊ������*/
/*
���˹��䡿�ú�����B��P���е�����д����Ϊ����
*/
static void downgradeAllSharedCacheTableLocks(Btree *p){
  BtShared *pBt = p->pBt;
  if( pBt->pWriter==p ){
    BtLock *pLock;
    pBt->pWriter = 0;
    pBt->btsFlags &= ~(BTS_EXCLUSIVE|BTS_PENDING);
    for(pLock=pBt->pLock; pLock; pLock=pLock->pNext){
      assert( pLock->eLock==READ_LOCK || pLock->pBtree==p );
      pLock->eLock = READ_LOCK;
    }
  }
}

#endif /* SQLITE_OMIT_SHARED_CACHE */

static void releasePage(MemPage *pPage);  /* Forward reference ��ǰ����*/ //���˹��䡿�ͷ�pPage

/*
***** This routine is used inside of assert() only ****
** ����������������������assert()�ڲ�
** Verify that the cursor holds the mutex on its BtShared
*/
/*�����������ֻ��assert������ȷ���α����BtShared�ϵĻ�������*/
/*
���˹��䡿������������ڲ���assert()����ȷ���α�BtShared���л�����
*/
#ifdef SQLITE_DEBUG
static int cursorHoldsMutex(BtCursor *p){
  return sqlite3_mutex_held(p->pBt->mutex);
}
#endif


#ifndef SQLITE_OMIT_INCRBLOB
/*
** Invalidate the overflow page-list cache for cursor pCur, if any.
*/
/*
�α�pCur���е����ҳ�棨���У������б���Ч( pCur->aOverflow = 0)��
*/
static void invalidateOverflowCache(BtCursor *pCur){
  assert( cursorHoldsMutex(pCur) );
  sqlite3_free(pCur->aOverflow);
  pCur->aOverflow = 0;  //���˹��䡿�α�pCur���е����ҳ��Ϊ0
}

/*
** Invalidate the overflow page-list cache for all cursors opened
** on the shared btree structure pBt.
*/
/*�ڹ���B���ṹpBt�ϣ������д򿪵��α�ʹ���ҳ�б���Ч��invalidateOverflowCache(p)*/
static void invalidateAllOverflowCache(BtShared *pBt){
  BtCursor *p;
  assert( sqlite3_mutex_held(pBt->mutex));
  for(p=pBt->pCursor; p; p=p->pNext){
    invalidateOverflowCache(p); //���˹��䡿�����α�pCur���е����ҳ�棨���У������б���Ч�ķ���
  }
}

/*
** This function is called before modifying the contents of a table
** to invalidate any incrblob cursors that are open on the
** row or one of the rows being modified.
**
** If argument isClearTable is true, then the entire contents of the
** table is about to be deleted. In this case invalidate all incrblob
** cursors open on any row within the table with root-page pgnoRoot.
**
** Otherwise, if argument isClearTable is false, then the row with
** rowid iRow is being replaced or deleted. In this case invalidate
** only those incrblob cursors open on that specific row.
** ��������ڱ�����ݱ��޸�֮ǰ�����ã�ʹ���ŵ��л����е�һ��
** ���޸ĵ�һ��incrblob�α���Ч��
** �������isClearTableΪ�棬����ȫ�����ݶ�����ɾ����������������£�
** ʹ�ڸ�ҳpgnoRoot�Ͽ��ŵ��л����е�һ�����޸ĵ�incrblob�α���Ч��
** ���⣬�������isClearTableΪ�٣���ô��rowid iRow���н��������ɾ����
** ����������£����ض����ϵĿ��ŵ���Щincrblob�α���Ч��
*/
/*
���˹��䡿�������������֮ǰ�޸�һ�����������Ч���κ�incrblob�α���л��б��޸ġ�
����۵�isClearTable�����,��ô��ȫ�����ݱ���ɾ�������������������incrblobʧЧ
�α�򿪱��е������и�ҳpgnoRoot������,����۵�isClearTable�Ǽٵ�,��ô��
rowid iRow���滻��ɾ�����������������Чֻ����Щincrblob�α���ض��С�
*/
static void invalidateIncrblobCursors(        //ʹ���ŵ��л����е�һ�����޸ĵ�һ��incrblob�α���Ч
  Btree *pBtree,          /* The database file to check */         //������ݿ��ļ� 
  i64 iRow,               /* The rowid that might be changing */   //rowid���ܷ����ı�
  int isClearTable        /* True if all rows are being deleted */ //������е��ж���ɾ��������
){
  BtCursor *p;
  BtShared *pBt = pBtree->pBt;
  assert( sqlite3BtreeHoldsMutex(pBtree) );
  for(p=pBt->pCursor; p; p=p->pNext){
    if( p->isIncrblobHandle && (isClearTable || p->info.nKey==iRow) ){
      p->eState = CURSOR_INVALID;
    }
  }
}

#else
  /* Stub functions when INCRBLOB is omitted ��INCRBLOB������ʱ���������*/ /*���˹��䡿��INCRBLOB��ʡ��ʱ���������*/
  #define invalidateOverflowCache(x)
  #define invalidateAllOverflowCache(x)
  #define invalidateIncrblobCursors(x,y,z)
#endif /* SQLITE_OMIT_INCRBLOB */

/*
** Set bit pgno of the BtShared.pHasContent bitvec. This is called 
** when a page that previously contained data becomes a free-list leaf 
** page.
**
** The BtShared.pHasContent bitvec exists to work around an obscure
** bug caused by the interaction of two useful IO optimizations surrounding
** free-list leaf pages:
**
**   1) When all data is deleted from a page and the page becomes
**      a free-list leaf page, the page is not written to the database
**      (as free-list leaf pages contain no meaningful data). Sometimes
**      such a page is not even journalled (as it will not be modified,
**      why bother journalling it?).
**
**   2) When a free-list leaf page is reused, its content is not read
**      from the database or written to the journal file (why should it
**      be, if it is not at all meaningful?).
**
** By themselves, these optimizations work fine and provide a handy(�����)
** performance boost to bulk delete or insert operations. However, if
** a page is moved to the free-list and then reused within the same
** transaction, a problem comes up. If the page is not journalled when
** it is moved to the free-list and it is also not journalled when it
** is extracted from the free-list and reused, then the original data
** may be lost. In the event of a rollback, it may not be possible
** to restore the database to its original configuration.
**
** The solution is the BtShared.pHasContent bitvec. Whenever a page is 
** moved to become a free-list leaf page, the corresponding bit is
** set in the bitvec. Whenever a leaf page is extracted from the free-list,
** optimization 2 above is omitted if the corresponding bit is already
** set in BtShared.pHasContent. The contents of the bitvec are cleared
** at the end of every transaction.
**������ �趨λ����BtShared.pHasContent��pgno��λ���������������ǰ�������ݵ�ҳ
** ��Ϊһ������Ҷ�ڵ�ҳ��ʱ�򱻵��á�
**
** BtShared.pHasContentλ����������Ϊ�˽��δ֪�Ĵ�����������Ƿ����ڿ���Ҷ�ڵ�ҳ��
** �ٽ��ڵ��������õ�IO�Ż����໥���ò����ģ�
** 1����ҳ���������ݶ���ɾ������ҳ��ɿ��б�Ҷ�ڵ�ҳ��ʱ�����ҳ�����ܱ�д�����ݿ���
**   �����б�Ҷ�ڵ��ҳ�а���������Ч�����ݣ�����ʱ������ҳ�������ᱻ��¼����־�С�
**	 ����Ϊ����û�б��޸�����û�б�Ҫ��¼����־��
** 2�������б�Ҷ�ڵ��ҳ������ʹ�õ�ʱ�����Ĳ�������ݿ��л��Ѿ���д����־�ļ��ж�ȡ��
**    ��Ϊ���ǲ��Ƕ�������ġ�
**
** ͨ��������Щ�Ż��𵽺ܺõ����ã����ṩһ���������������ﵽ����ɾ������������
** Ȼ�������һ��ҳ���Ƴ�����Ϊ�����б�Ȼ������ͬ�������������ã��ͻ�������⡣
** ��ҳ���Ƴ�����Ϊ�����б�ʱ�����ҳ��û�м��뵽��־����Ҳ������뵽��־���������ɱ���
** ��ȡ������ʹ�ã���ԭʼ���ݿ��ܻᶪʧ���ڻع�������£��������޷������ݿ�ָ���ԭ�������á�
**
** �ý����������BtShared.pHasContent λ������ÿ��һ��ҳ���Ƴ���Ϊ���б�Ҷ�ӽڵ�ҳʱ��
** ��Ӧ��λ����λ���������á�ÿ��һ��Ҷ�ڵ�ҳ�ӿ��б�����ȡ�������Ӧ��λ�Ѿ���
** BtShared.pHasContent�����ã������������Ż��������ԡ���ÿһ���������ʱ��λ���������ݽ��������
*/
/*
���˹��䡿����һЩʹ��pgno BtShared��pHasContent bitvec��
��һ��ҳ��֮ǰ�������ݳ�Ϊ�����б��Ҷ��ҳ��,��ʱ�����á�
BtShared��pHasContent bitvec������һ�������۵Ĺ���
�����������Χ�������õ�IO�Ż�֮��Ľ���
Ҷ�����б�ҳ:
1)���������ݴ�һ��ҳ���ҳ��ɾ��Ҷ�����б�ҳ��,ҳ�治д�����ݿ�(��Ҷ�����б�ҳ�治���������������)��
��ʱ����һ��ҳ������������־(���ᱻ�޸�,Ϊʲô��������־?)��
2)��һ�������б�Ҷҳ������,�����ݲ����Ķ������ݿ��д����־�ļ�(Ϊʲô����,���������������?)��

������Щ�Ż�����,�ṩһ�����������ɾ����������������������Ȼ��,����ᵽһ��ҳ���ڵĿ����б�Ȼ��������ͬ����,�������⡣���ҳ��û����־ʱ
�ᵽ�����б�,��Ҳ������־ʱ�ӿ����б�����ȡ������,��ôԭʼ���ݿ��ܻᶪʧ���ڷ����ع�,�������ǲ����ܵĻָ����ݿ⵽ԭ�������á�

���������BtShared��pHasContent bitvec����һ��ҳ��ᵽ��Ϊһ�������б�ҳ,��Ӧ��λ��bitvec�����á�
ÿ���ӿ����б�����ȡҶ��ҳ��ʱ,�Ż�2�����Ӧ��һЩ�Ѿ�������BtShared.pHasContent��bitvec�����ݱ������ÿ�ʽ��׵Ľ�����

*/
static int btreeSetHasContent(BtShared *pBt, Pgno pgno){
  int rc = SQLITE_OK;
  if( !pBt->pHasContent ){  /*����ҳ*/
    assert( pgno<=pBt->nPage );
    pBt->pHasContent = sqlite3BitvecCreate(pBt->nPage);
    if( !pBt->pHasContent ){
      rc = SQLITE_NOMEM;   /*�����ڴ�ʧ��*/
    }
  }
  if( rc==SQLITE_OK && pgno<=sqlite3BitvecSize(pBt->pHasContent) ){
    rc = sqlite3BitvecSet(pBt->pHasContent, pgno);
  }
  return rc;
}

/*
** Query the BtShared.pHasContent vector.
**
** This function is called when a free-list leaf page is removed from the
** free-list for reuse. It returns false if it is safe to retrieve the
** page from the pager layer with the 'no-content' flag set. True otherwise.
** ��ѯBtShared.pHasContent������
** ��һ���ձ��Ҷ�ڵ��ҳ������õĿձ��б��Ƴ�ʱ����������������á������
** ����no-content��ǩ��ҳ���������ҳ���ǰ�ȫ���򷵻�false�����򷵻�true
*/
/*
���˹��䡿**��ѯBtShared.pHasContent�������������������ʱҶ�����б�ҳ����
�����б��Ա����á�����������false,������ǰ�ȫ�Ĵ�ҳ����ȳ����ҳ������û�����ݵı�־������ΪTrue��
*/
static int btreeGetHasContent(BtShared *pBt, Pgno pgno){
  Bitvec *p = pBt->pHasContent; /*�Ƿ�Ϊ����ҳ*/
  return (p && (pgno>sqlite3BitvecSize(p) || sqlite3BitvecTest(p, pgno)));
}

/*
** Clear (destroy) the BtShared.pHasContent bitvec. This should be
** invoked at the conclusion of each write-transaction.
** ���BtShared.pHasContentλ�������������Ӧ���ڵõ�ÿ��д��������ǵ��á�
*/
/*��ÿ��д����Ľ�β������*/
static void btreeClearHasContent(BtShared *pBt){
  sqlite3BitvecDestroy(pBt->pHasContent);/*����λͼ���󣬻����ù����ڴ�*/
  pBt->pHasContent = 0;
}

/*
** Save the current cursor position in the variables BtCursor.nKey 
** and BtCursor.pKey. The cursor's state is set to CURSOR_REQUIRESEEK.
**
** The caller must ensure that the cursor is valid (has eState==CURSOR_VALID)
** prior to calling this routine.  
** ���浱ǰ�α��ڱ���BtCursor.nKey��BtCursor.pKey�ϵ�λ�á��α��״̬������ΪCURSOR_REQUIRESEEK.
** ��������߱���ȷ��֮ǰ�������������α�����Ч(��eState==CURSOR_VALID)��
*/
/*���α��λ�ñ����ڱ���BtCursor.nKey��BtCursor.pKey�У�ȡpKey
��nKey���ȵ��ֶξͿ����ҵ��α�����λ�á������α��0��ʼ��pKey
ָ���α�� last knownλ�á�
*/
static int saveCursorPosition(BtCursor *pCur){//���˹��䡿�����α����ڵ�λ�õķ���
  int rc;

  assert( CURSOR_VALID==pCur->eState );/*ǰ��:�α���Ч*/
  assert( 0==pCur->pKey );
  assert( cursorHoldsMutex(pCur) );

  rc = sqlite3BtreeKeySize(pCur, &pCur->nKey);
  assert( rc==SQLITE_OK );  /* KeySize() cannot fail����Զ����SQLITE_OK*/

  /* If this is an intKey table, then the above call to BtreeKeySize()
  ** stores the integer key in pCur->nKey. In this case this value is
  ** all that is required. Otherwise, if pCur is not open on an intKey
  ** table, then malloc space for and store the pCur->nKey bytes of key 
  ** data.
  ** �����һ��intKey��Ȼ���ϱߵ���BtreeKeySize()���洢���������pCur->nKey�
  ** ����ʱ�����ֵ��������Ҫ��ֵ�����������intKey����pCur���ǿ��ŵģ���ô
  ** ��̬����ռ䲢�Ҵ洢�ؼ������ݵ� pCur->nKey�ֽڡ�
  */
  /*
  ���˹��䡿�������һ��intKey��,Ȼ������ĵ���BtreeKeySize()
  ���������洢��pCur->nKey�������������,���ֵ����Ҫ�ġ�
  ����,����α���intKey����û�д�,Ȼ�����malloc�ʹ洢�ռ� pCur->nKey�ֽڵĹؼ����ݡ�
  */
  if( 0==pCur->apPage[0]->intKey ){/*�α���intKey����û�д򿪣�����pCur->nKey��С�Ŀռ�*/
    void *pKey = sqlite3Malloc( (int)pCur->nKey );
    if( pKey ){
      rc = sqlite3BtreeKey(pCur, 0, (int)pCur->nKey, pKey);
      if( rc==SQLITE_OK ){
        pCur->pKey = pKey;
      }else{
        sqlite3_free(pKey);
      }
    }else{
      rc = SQLITE_NOMEM;
    }
  }
  assert( !pCur->apPage[0]->intKey || !pCur->pKey );

  if( rc==SQLITE_OK ){
    int i;
    for(i=0; i<=pCur->iPage; i++){
      releasePage(pCur->apPage[i]);
      pCur->apPage[i] = 0;
    }
    pCur->iPage = -1;
    pCur->eState = CURSOR_REQUIRESEEK;/*�α�ָ��ı��޸��ˣ���Ҫ���¶�λ�α��λ��*/
  }

  invalidateOverflowCache(pCur);//���˹��䡿�����α�pCur���е����ҳ�棨���У������б���Ч�ķ���
  return rc;
}

/*
** Save the positions of all cursors (except pExcept) that are open on
** the table  with root-page iRoot. Usually, this is called just before cursor
** pExcept is used to modify the table (BtreeDelete() or BtreeInsert()).
** �����������и�ҳiRoot�ı��Ͽ��ŵ��α��λ�á������б��δ֪����˼����B���ϵ�λ��
** ����¼����������B�����޸ĺ��ƻص���ͬ�ĵ㡣����������α�pExcept�������޸ı�֮ǰ������
** ������BtreeDelete()�� BtreeInsert()�С�
** �������ͬ��Btree�����������߸�����α꣬�������������α궼Ӧ����BTCF_Multiple��ǡ�
** btreeCursor()��ִ���������������򱻵����ڲ�����������£���pExpect�Ѿ�������BTCF_Multiple���ʱ��
** ���pExpect!=NULL�����������ͬ�ĸ�ҳ��û���α꣬��ô��pExpect�ϵ�BTCF_Multiple��ǽ������
** ������һ��������ĵ����������
** ʵ��ʱע�⣺����������ȥ�˶��Ƿ����α���Ҫ���档������saveCursorsOnList()(�쳣)�¼�,�α�����Ҫ�����档
*/
/*
���˹��䡿���������α��λ��(pExcept����)��򿪱��ϸ�ҳ��iRoot��
ͨ��,�����ǰ�α�pExcept�����޸ı�(BtreeDelete()��BtreeInsert())��

*/
static int saveAllCursors(BtShared *pBt, Pgno iRoot, BtCursor *pExcept){
  BtCursor *p;
  assert( sqlite3_mutex_held(pBt->mutex) );
  assert( pExcept==0 || pExcept->pBt==pBt );
  for(p=pBt->pCursor; p; p=p->pNext){
    if( p!=pExcept && (0==iRoot || p->pgnoRoot==iRoot) && 
        p->eState==CURSOR_VALID ){/*ָ���ҳ���α겻��Ҫ����*/
      int rc = saveCursorPosition(p);
      if( SQLITE_OK!=rc ){
        return rc;
      }
    }
  }
  return SQLITE_OK;
}

/* Clear the current cursor position.   �����ǰ�α�λ��*//*���˹��䡿ɾ����ǰ�α�����λ��*/
void sqlite3BtreeClearCursor(BtCursor *pCur){
  assert( cursorHoldsMutex(pCur) );
  sqlite3_free(pCur->pKey);
  pCur->pKey = 0;
  pCur->eState = CURSOR_INVALID;
}

/*
** In this version of BtreeMoveto, pKey is a packed index record
** such as is generated by the OP_MakeRecord opcode.  Unpack the
** record and then call BtreeMovetoUnpacked() to do the work.
** ��BtreeMoveto������汾�У�pKey��һ����������¼����OP_MakeRecord���ɵĲ����롣
** �򿪼�¼Ȼ�����BtreeMovetoUnpacked()������������
*/
/*���˹��䡿������汾��BtreeMoveto,pKeyӵ��ָ����OP_MakeRecord���������ɵȼ�¼��
�򿪼�¼,Ȼ�����BtreeMovetoUnpacked()�������������*/
static int btreeMoveto(
  BtCursor *pCur,     /* Cursor open on the btree to be searched  ��B���Ͽ����α�ʹ֮�ܱ�������*//*���˹��䡿�α��btree����*/
  const void *pKey,   /* Packed key if the btree is an index ���B����һ�����������ؼ���*/ /*���˹��䡿���btree�����ǰ�װ�Ĺؼ�*/
  i64 nKey,           /* Integer key for tables.  Size of pKey for indices ��������ؼ��֣�pKey�Ĵ�С*/  /*���˹��䡿���е����μ�*/
  int bias,           /* Bias search to the high end �������ո߶�*/ /*���˹��䡿���������ֵ*/
  int *pRes           /* Write search results here д�������*/    /*���˹��䡿�������д������*/
){
  int rc;                    /* Status code ״̬��*/
  UnpackedRecord *pIdxKey;   /* Unpacked index key �������� */
  char aSpace[150];          /* Temp space for pIdxKey - to avoid a malloc **pIdxKey����ʱ�ռ䣬���⶯̬����*/
  char *pFree = 0;

  if( pKey ){
    assert( nKey==(i64)(int)nKey );
    pIdxKey = sqlite3VdbeAllocUnpackedRecord(
        pCur->pKeyInfo, aSpace, sizeof(aSpace), &pFree
    );
    if( pIdxKey==0 ) return SQLITE_NOMEM;
    sqlite3VdbeRecordUnpack(pCur->pKeyInfo, (int)nKey, pKey, pIdxKey);
  }else{
    pIdxKey = 0;
  }
  rc = sqlite3BtreeMovetoUnpacked(pCur, pIdxKey, nKey, bias, pRes);
  if( pFree ){
    sqlite3DbFree(pCur->pKeyInfo->db, pFree);
  }
  return rc;
}

/*
** Restore the cursor to the position it was in (or as close to as possible)
** when saveCursorPosition() was called. Note that this call deletes the 
** saved position info stored by saveCursorPosition(), so there can be
** at most one effective restoreCursorPosition() call after each 
** saveCursorPosition().
** ��saveCursorPosition()�����õ�ʱ�����±����б��λ�á�ע��������û�
** ɾ��saveCursorPosition()֮ǰ�����λ����Ϣ�������ÿһ��saveCursorPosition()��
** ��һ����Ч��restoreCursorPosition()����
*/
/*����saveCursorPosition()֮��saveCursorPosition()�б����λ����Ϣ��ɾ�������Ҫ�ָ�
�α�λ�á�*/
static int btreeRestoreCursorPosition(BtCursor *pCur){
  int rc;
  assert( cursorHoldsMutex(pCur) );
  assert( pCur->eState>=CURSOR_REQUIRESEEK );/*�α괦��CURSOR_FAULT|CURSOR_REQUIRESEEK״̬ */
  if( pCur->eState==CURSOR_FAULT ){
    return pCur->skipNext;  /* Prev() is noop if (skipNext) negative. Next() is noop if positive  ���skipNext�Ǹ����� Prev()�޲��������Ϊ����Next()�޲���*/
  }
  pCur->eState = CURSOR_INVALID;
  rc = btreeMoveto(pCur, pCur->pKey, pCur->nKey, 0, &pCur->skipNext);
  if( rc==SQLITE_OK ){
    sqlite3_free(pCur->pKey);
    pCur->pKey = 0;
    assert( pCur->eState==CURSOR_VALID || pCur->eState==CURSOR_INVALID );
  }
  return rc;
}

#define restoreCursorPosition(p) \
  (p->eState>=CURSOR_REQUIRESEEK ? \
         btreeRestoreCursorPosition(p) : \
         SQLITE_OK)

/*
** Determine whether or not a cursor has moved from the position it
** was last placed at.  Cursors can move when the row they are pointing
** at is deleted out from under them.
**
** This routine returns an error code if something goes wrong.  The
** integer *pHasMoved is set to one if the cursor has moved and 0 if not.
** ȷ���α��Ƿ��Ѿ�����һ�ε�λ�÷������ƶ�������������ԭ�����Ч��
** ���磬���α���ָ��ɾ������ʱ�α�����ƶ������B����ƽ�⣬�α�Ҳ��ᷢ���ƶ���
** ���ô��п��α�ĳ���ʱ����false��
** ʹ�õ�����sqlite3BtreeCursorRestore()����ָ�һ���α굽��Ӧ���ڵ�λ�ã����������򷵻�true�Ļ���
*/
/*�α��Ƿ��ƶ��������ش�����롣*/
/*
���˹��䡿ȷ���Ƿ�һ���α��Ѿ�������λ����󱻷��á��α�����ƶ�,����ָ��
�ڱ�ɾ�������ǡ�������ִ���������򷵻�һ��������롣����α��Ѿ��ƶ��ˣ�
�� pHasMoved�������ָ�뱻����Ϊ1������Ϊ0��
*/
int sqlite3BtreeCursorHasMoved(BtCursor *pCur, int *pHasMoved){
  int rc;  //״̬��

  rc = restoreCursorPosition(pCur);
  if( rc ){
    *pHasMoved = 1;
    return rc;
  }
  if( pCur->eState!=CURSOR_VALID || pCur->skipNext!=0 ){
    *pHasMoved = 1;
  }else{
    *pHasMoved = 0;
  }
  return SQLITE_OK;
}

#ifndef SQLITE_OMIT_AUTOVACUUM
/*
** Given a page number of a regular database page, return the page
** number for the pointer-map page that contains the entry for the
** input page number.
**
** Return 0 (not a valid page) for pgno==1 since there is
** no pointer map associated with page 1.  The integrity_check logic
** requires that ptrmapPageno(*,1)!=1.
** ���ڳ������ݿ�ҳ��ҳ�ţ�����ҳ��Ϊ�������ڽ�����ҳ����Ŀ��ָ��λͼҳ��
** ����pgno==1������0������һ����Ч��ҳ������Ϊû����ҳ1��ص�ָ��λͼ��
** �����Լ����߼�Ҫ����ptrmapPageno(*,1)!=1
*/
/*
���˹��䡿���ڳ������ݿ�ҳ���ҳ��,���ص�ҳ������pointer-mapҳ��,���а�������Ŀ
����ҳ�롣����0(����һ����Ч��ҳ��)����pgno = = 1û��ָ��ӳ�����1ҳ��integrity_check�߼�
Ҫ��ptrmapPageno(* 1)! = 1��
*/
static Pgno ptrmapPageno(BtShared *pBt, Pgno pgno){
  int nPagesPerMapPage;
  Pgno iPtrMap, ret;
  assert( sqlite3_mutex_held(pBt->mutex) );
  if( pgno<2 ) return 0;     /*��Чҳ����Ϊû��ָ��ָ��ҳ��1*/
  nPagesPerMapPage = (pBt->usableSize/5)+1;
  iPtrMap = (pgno-2)/nPagesPerMapPage;
  ret = (iPtrMap*nPagesPerMapPage) + 2; 
  if( ret==PENDING_BYTE_PAGE(pBt) ){
    ret++;
  }
  return ret;
}

/*
** Write an entry into the pointer map.
**
** This routine updates the pointer map entry for page number 'key'
** so that it maps to type 'eType' and parent page number 'pgno'.
**
** If *pRC is initially non-zero (non-SQLITE_OK) then this routine is
** a no-op.  If an error occurs, the appropriate error code is written
** into *pRC.
** дһ����Ŀ����ָ��λͼ������������ҳ�롰key����ָ��λͼ��Ŀ��
** �Ա��ڣ���ӳ�䵽����'eType'�븸ҳ��'pgno'.
** ���*pRC�ĳ�ʼ���Ƿ����(non-SQLITE_OK)����ô��������޲����ġ�
** ���һ��������������Ӧ�Ĵ�����뱻д��*pRC.
*/
/*
���˹��䡿
*дһ�������ָ��ӳ�
**����������ҳ��'key'��ָ��ӳ�����Ա���ӳ�䵽����'eType'�͸�ҳ��'pgno'���*pRC�������(non-SQLITE_OK)��
�������������κβ�����������������ʵ��Ĵ�������д��*pRC*/
static void ptrmapPut(BtShared *pBt, Pgno key, u8 eType, Pgno parent, int *pRC){
  DbPage *pDbPage;  /* The pointer map page ָ��λͼҳ*/ /*���˹��䡿Pager��ҳ���*/
  u8 *pPtrmap;      /* The pointer map data ָ��λͼ��������*/ 
  Pgno iPtrmap;     /* The pointer map page number ָ��λͼ��ҳ��*/
  int offset;       /* Offset in pointer map page ָ��λͼҳ��ƫ����*/
  int rc;           /* Return code from subfunctions(�Ӻ���) ���Ӻ������ص�����*//* ���˹��䡿�����Ӻ�������*/

  if( *pRC ) return;

  assert( sqlite3_mutex_held(pBt->mutex) );
  /* The master-journal page number must never be used as a pointer map page 
  ** ����־ҳ��һ����������ָ��λͼҳ
  */
  /*���˹��䡿master-journalҳ�����������ܱ�����һ��ָ��λͼҳ��*/
  assert( 0==PTRMAP_ISPAGE(pBt, PENDING_BYTE_PAGE(pBt)) );

  assert( pBt->autoVacuum );
  if( key==0 ){
    *pRC = SQLITE_CORRUPT_BKPT;
    return;
  }
  iPtrmap = PTRMAP_PAGENO(pBt, key);
  rc = sqlite3PagerGet(pBt->pPager, iPtrmap, &pDbPage);
  if( rc!=SQLITE_OK ){
    *pRC = rc;
    return;
  }
  offset = PTRMAP_PTROFFSET(iPtrmap, key);
  if( offset<0 ){
    *pRC = SQLITE_CORRUPT_BKPT;
    goto ptrmap_exit;
  }
  assert( offset <= (int)pBt->usableSize-5 );
  pPtrmap = (u8 *)sqlite3PagerGetData(pDbPage);

  if( eType!=pPtrmap[offset] || get4byte(&pPtrmap[offset+1])!=parent ){
    TRACE(("PTRMAP_UPDATE: %d->(%d,%d)\n", key, eType, parent));
    *pRC= rc = sqlite3PagerWrite(pDbPage);
    if( rc==SQLITE_OK ){
      pPtrmap[offset] = eType;
      put4byte(&pPtrmap[offset+1], parent);
    }
  }

ptrmap_exit:
  sqlite3PagerUnref(pDbPage);
}

/*
** Read an entry from the pointer map.
** 
** This routine retrieves the pointer map entry for page 'key', writing
** the type and parent page number to *pEType and *pPgno respectively.
** An error code is returned if something goes wrong, otherwise SQLITE_OK.
** ��ָ��λͼ��ȡ��Ŀ��
** �����������ҳ�� 'key'��ָ��λͼ��Ŀ���ֱ����ͺ͸�ҳ��д�뵽*pEType �� *pPgno�С�
** ������г����򷵻ش�����룬��������SQLITE_OK.
*/
/*��ȡpointer map����Ŀ��д��pEType��pPgno��*/
/*
���˹��䡿*��һ������ӳ���ָ�롣
**��������ȡָ��ӳ����Ŀҳ��'key',д�����ͺ͸�ҳ��Ŀ��*pEType �� *pPgno�໥�ֿ�
**������ִ���,����һ������Ĵ��룬���򷵻�SQLITE_OK��
*/
static int ptrmapGet(BtShared *pBt, Pgno key, u8 *pEType, Pgno *pPgno){
  DbPage *pDbPage;   /* The pointer map page ָ��λͼҳ*/  /*���˹��䡿Pager��ҳ���*/
  int iPtrmap;       /* Pointer map page index ָ��λͼҳ����*//*���˹��䡿ָ��ӳ��ҳ������*/
  u8 *pPtrmap;       /* Pointer map page data ָ��λͼҳ����*/ /*���˹��䡿ָ��ӳ��ҳ����*/
  int offset;        /* Offset of entry in pointer map ָ��λͼҳ��ƫ����*//*���˹��䡿ָ��ӳ�������ƫ��*/
  int rc;

  assert( sqlite3_mutex_held(pBt->mutex) );

  iPtrmap = PTRMAP_PAGENO(pBt, key);
  rc = sqlite3PagerGet(pBt->pPager, iPtrmap, &pDbPage);
  if( rc!=0 ){
    return rc;
  }
  pPtrmap = (u8 *)sqlite3PagerGetData(pDbPage);

  offset = PTRMAP_PTROFFSET(iPtrmap, key);
  if( offset<0 ){
    sqlite3PagerUnref(pDbPage);
    return SQLITE_CORRUPT_BKPT;
  }
  assert( offset <= (int)pBt->usableSize-5 );
  assert( pEType!=0 );
  *pEType = pPtrmap[offset];
  if( pPgno ) *pPgno = get4byte(&pPtrmap[offset+1]);

  sqlite3PagerUnref(pDbPage);
  if( *pEType<1 || *pEType>5 ) return SQLITE_CORRUPT_BKPT;
  return SQLITE_OK;
}

#else /* if defined SQLITE_OMIT_AUTOVACUUM */
  #define ptrmapPut(w,x,y,z,rc)
  #define ptrmapGet(w,x,y,z) SQLITE_OK
  #define ptrmapPutOvflPtr(x, y, rc)
#endif

/*
** Given a btree page and a cell index (0 means the first cell on
** the page, 1 means the second cell, and so forth) return a pointer
** to the cell content.
**
** This routine works only for pages that do not contain overflow cells.
** ������B��ҳ�͵�Ԫ������0��ζ��ҳ�ϵĵ�һ����Ԫ��1�ǵڶ�����Ԫ���ȵȣ�����һ��ָ��Ԫ���ݵ�ָ��
** �������ֻ�Բ����������Ԫ��ҳ������
*/
/*������B��ҳ�͵�Ԫ������0��ζ��ҳ�ϵĵ�һ����Ԫ��1�ǵڶ�����Ԫ���ȵȣ�����һ��ָ��Ԫ���ݵ�ָ��*/
/*
���˹��䡿��btreeҳ���һ��cell index���� (0��ζ��ҳ���ϵĵ�һ����Ԫ��,1��ζ�ŵڶ�����Ԫ��,�ȵ�)����һ��ָ��ָ��Ԫ���ݡ�
�������ֻ�ʺ�ҳ�治���������cells��
*/
#define findCell(P,I) \
  ((P)->aData + ((P)->maskPage & get2byte(&(P)->aCellIdx[2*(I)])))
#define findCellv2(D,M,O,I) (D+(M&get2byte(D+(O+2*(I)))))


/*
** This a more complex version of findCell() that works for
** pages that do contain overflow cells.
** ��԰��������Ԫ�ĸ�Ϊ���ӵ�findCell()�汾
*/
/*
���˹��䡿��������ӵİ汾��findCell()Ϊҳ�����Ĺ������������cells��
*/
static u8 *findOverflowCell(MemPage *pPage, int iCell){
  int i;
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  for(i=pPage->nOverflow-1; i>=0; i--){
    int k;
    k = pPage->aiOvfl[i];
    if( k<=iCell ){
      if( k==iCell ){
        return pPage->apOvfl[i];
      }
      iCell--;
    }
  }
  return findCell(pPage, iCell);
}

/*
** Parse a cell content block and fill in the CellInfo structure.  There
** are two versions of this function.  btreeParseCell() takes a 
** cell index as the second argument and btreeParseCellPtr() 
** takes a pointer to the body of the cell as its second argument.
**
** Within this file, the parseCell() macro can be called instead of
** btreeParseCellPtr(). Using some compilers, this will be faster.
** ������Ԫ���ݿ飬����CellInfo�ṹ�С���������������汾��btreeParseCell()
** ռ��һ����Ԫ���������ǵ�һ����btreeParseCellPtr()ռ��һ��ָ��Ԫ���ָ���ǵڶ����汾��
** �����ļ��ڣ����Ե��ú�parseCell()������btreeParseCellPtr()����һЩ����������졣
*/
/*����cell content block������CellInfo�ṹ�С�*/
/*
���˹��䡿**����cell content block������CellInfo�ṹ�С���������������汾��
��btreeParseCell()�⺯���У���һ����Ԫ����Ϊ�ڶ���������
��btreeParseCellPtr()��������У���һ��ָ��ָ��Ԫ������Ϊ���ڶ���������

**������ļ���,parseCell()�ĺ���Դ���btreeParseCellPtr()��ʹ�ñ�����,����������졣

*/
static void btreeParseCellPtr(           //������Ԫ���ݿ飬����CellInfo�ṹ��
  MemPage *pPage,         /* Page containing the cell ������Ԫ��ҳ*/  /*���˹��䡿������Ԫ���ҳ��*/
  u8 *pCell,              /* Pointer to the cell text. ��Ԫ�ı���ָ��*/ /*���˹��䡿ָ��Ԫ���ݵ�ָ��*/
  CellInfo *pInfo         /* Fill in this structure �������ṹ*/
){
  u16 n;                  /* Number bytes in cell content header ��Ԫ����ͷ�����ֽ���*/  
  u32 nPayload;           /* Number of bytes of cell payload ��Ԫ��Ч�غɣ�B����¼�����ֽ���*/ /*���˹��䡿��Ԫ��Ч���ص��ֽ���*/

  assert( sqlite3_mutex_held(pPage->pBt->mutex) );

  pInfo->pCell = pCell;
  assert( pPage->leaf==0 || pPage->leaf==1 );
  n = pPage->childPtrSize;
  assert( n==4-4*pPage->leaf );
  if( pPage->intKey ){
    if( pPage->hasData ){
      n += getVarint32(&pCell[n], nPayload);
    }else{
      nPayload = 0;
    }
    n += getVarint(&pCell[n], (u64*)&pInfo->nKey);
    pInfo->nData = nPayload;
  }else{
    pInfo->nData = 0;
    n += getVarint32(&pCell[n], nPayload);
    pInfo->nKey = nPayload;
  }
  pInfo->nPayload = nPayload;
  pInfo->nHeader = n;
  testcase( nPayload==pPage->maxLocal );
  testcase( nPayload==pPage->maxLocal+1 );
  if( likely(nPayload<=pPage->maxLocal) ){
    /* This is the (easy) common case where the entire payload fits
    ** on the local page.  No overflow is required.
	** ���Ǹ���������������е���Ч�غɶ��̶��ڱ���ҳ�ϡ�����Ҫ�����
    */
	   /*
	  ���˹��䡿���������׵�����£����������ʺ��ڱ���ҳ���ϡ�����Ҫ���
	  */
    if( (pInfo->nSize = (u16)(n+nPayload))<4 ) pInfo->nSize = 4;
    pInfo->nLocal = (u16)nPayload;
    pInfo->iOverflow = 0;
  }else{
    /* If the payload will not fit completely on the local page, we have
    ** to decide how much to store locally and how much to spill onto
    ** overflow pages.  The strategy is to minimize the amount of unused
    ** space on overflow pages while keeping the amount of local storage
    ** in between minLocal and maxLocal.
    **
    ** Warning:  changing the way overflow payload is distributed in any
    ** way will result in an incompatible file format.
	** ������ز��ʺ���ȫ�ڱ���ҳ��,���Ǳ���������ٴ洢�ڱ��غͶ��ٴ洢�����ҳ��
	** ��������Ǽ������ҳ��δʹ�ÿռ������ ͬʱ���ֱ��ش洢��������minLocal��maxLocal֮�䡣
	** ����:����ı������غɷֲ��ᵼ�²����ݵ��ļ���ʽ��
    */
	  /*
	���˹��䡿������ؽ�����ȫƥ�䱾��ҳ�����Ǳ���������ٴ洢�ڱ��غͶ���й©�����ҳ��
	δʹ�õĲ����Ǽ��ٿռ����ҳͬʱ�����еı��ش洢minLocal��maxLocal֮�䡣
	����:���κη�ʽ�ı������غɷֲ��ķ�ʽ�ᵼ�²����ݵ��ļ���ʽ��
	*/
    /*ʹ���ش洢����С�����ֵ֮�䲢�ҽ����ҳδʹ��������С��*/
    int minLocal;  /* Minimum amount of payload held locally */       //���س��е���С��Ч�غ����� 
    int maxLocal;  /* Maximum amount of payload held locally */       //���س��е������Ч�غ�����
    int surplus;   /* Overflow payload available for local storage */ //�������Ч�غɿ����ڱ��ش洢  /*���˹��䡿�����ڱ��ش洢�������Ч����*/

    minLocal = pPage->minLocal;
    maxLocal = pPage->maxLocal;
    surplus = minLocal + (nPayload - minLocal)%(pPage->pBt->usableSize - 4);
    testcase( surplus==maxLocal );
    testcase( surplus==maxLocal+1 );
    if( surplus <= maxLocal ){
      pInfo->nLocal = (u16)surplus;
    }else{   /*���*/
      pInfo->nLocal = (u16)minLocal;
    }
    pInfo->iOverflow = (u16)(pInfo->nLocal + n);
    pInfo->nSize = pInfo->iOverflow + 4;
  }
}
#define parseCell(pPage, iCell, pInfo) \
  btreeParseCellPtr((pPage), findCell((pPage), (iCell)), (pInfo))
static void btreeParseCell(                                      //������Ԫ���ݿ�
  MemPage *pPage,         /* Page containing the cell */         //������Ԫ��ҳ  /*���˹��䡿������Ԫ���ҳ��*/
  int iCell,              /* The cell index.  First cell is 0 */ //��Ԫ����������һ����ԪiCellΪ0  /*���˹��䡿��Ԫ���������׵�Ԫ����0*/
  CellInfo *pInfo         /* Fill in this structure */           //��д����ṹ
){
  parseCell(pPage, iCell, pInfo);
}

/*
** Compute the total number of bytes that a Cell needs in the cell
** data area of the btree-page.  The return number includes the cell
** data header and the local payload, but not any overflow page or
** the space used by the cell pointer.
*/
/*����һ��Cell��Ҫ���ܵ��ֽ���*/
/*���˹��䡿���㵥Ԫ������������b��ҳ�ĵ�Ԫ����Ҫ�����ֽ�����
���ص����ְ���С������ͷ�ͱ��ظ��أ���û���κ����ҳ��Ԫָ����ʹ�õĿռ䡣

*/
static u16 cellSizePtr(MemPage *pPage, u8 *pCell){  //����һ��Cell��Ҫ���ܵ��ֽ���
  u8 *pIter = &pCell[pPage->childPtrSize];
  u32 nSize;

#ifdef SQLITE_DEBUG
  /* The value returned by this function should always be the same as
  ** the (CellInfo.nSize) value found by doing a full parse of the
  ** cell. If SQLITE_DEBUG is defined, an assert() at the bottom of
  ** this function verifies that this invariant(����ʽ) is not violated(Υ��). 
  ** ��������ķ���ֵӦʼ���Ǻ�ִ��һ�������Ľ����з��ֵĵ�Ԫ��CellInfo.nSize��ֵ��ͬ��
  ** ���SQLITE_DEBUG�����壬һ��assert�������ĵײ����Ĺ�����֤������䲻Υ����*/
  /*
  ���˹��䡿����������ص�ֵӦ������ͬ�ģ�CellInfo.nSize��ֵ��һ��ȫ��Ľ�����Ԫ��ķ��֡�
  ���SQLITE_DEBUG�����壬����������ĵײ�assert()֤�����ֲ��䣨����ʽ����Υ����Υ������
  */
  CellInfo debuginfo;
  btreeParseCellPtr(pPage, pCell, &debuginfo);
#endif

  if( pPage->intKey ){
    u8 *pEnd;
    if( pPage->hasData ){
      pIter += getVarint32(pIter, nSize);
    }else{
      nSize = 0;
    }

    /* pIter now points at the 64-bit integer key value, a variable length 
    ** integer. The following block moves pIter to point at the first byte
    ** past the end of the key value. 
	** pIter����ָ����64λ�����ؼ���ֵ���ɱ䳤������������Ŀ��ƶ�pIterָ���ֵĩβ�ĵ�һ���ֽڡ�*/
	/*���˹��䡿pIterָ��64λ�����Ĺؼ�ֵ��һ���ɱ䳤�ȵ�������
	����Ŀ��ƶ�pIterָ���һ���ֽڵĹؼ�ֵ������ȥ*/
    pEnd = &pIter[9];
    while( (*pIter++)&0x80 && pIter<pEnd );
  }else{
    pIter += getVarint32(pIter, nSize);
  }

  testcase( nSize==pPage->maxLocal );
  testcase( nSize==pPage->maxLocal+1 );
  if( nSize>pPage->maxLocal ){
    int minLocal = pPage->minLocal;
    nSize = minLocal + (nSize - minLocal) % (pPage->pBt->usableSize - 4);
    testcase( nSize==pPage->maxLocal );
    testcase( nSize==pPage->maxLocal+1 );
    if( nSize>pPage->maxLocal ){
      nSize = minLocal;
    }
    nSize += 4;
  }
  nSize += (u32)(pIter - pCell);

  /* The minimum size of any cell is 4 bytes. */ /*���˹��䡿�κε�Ԫ����С�ߴ�Ϊ4���ֽڡ�*/
  if( nSize<4 ){
    nSize = 4;
  }

  assert( nSize==debuginfo.nSize );
  return (u16)nSize;
}

#ifdef SQLITE_DEBUG
/* This variation on cellSizePtr() is used inside of assert() statements
** only. 
** cellSizePtr()�еı�������������assert()����� */
/*���˹��䡿���ֱ仯��cellsizeptr()����assert()���ֻ��ʹ�á�*/
static u16 cellSize(MemPage *pPage, int iCell){
  return cellSizePtr(pPage, findCell(pPage, iCell));
}
#endif

#ifndef SQLITE_OMIT_AUTOVACUUM
/*
** If the cell pCell, part of page pPage contains a pointer
** to an overflow page, insert an entry into the pointer-map
** for the overflow page.
*/
/*���pCell(pPage��һ����)����ָ�����ҳ��ָ�룬��Ϊ������ҳ����һ����Ŀ��pointer-map*/
static void ptrmapPutOvflPtr(MemPage *pPage, u8 *pCell, int *pRC){
  CellInfo info;
  if( *pRC ) return;
  assert( pCell!=0 );
  btreeParseCellPtr(pPage, pCell, &info);
  assert( (info.nData+(pPage->intKey?0:info.nKey))==info.nPayload );
  if( info.iOverflow ){
    Pgno ovfl = get4byte(&pCell[info.iOverflow]);
    ptrmapPut(pPage->pBt, ovfl, PTRMAP_OVERFLOW1, pPage->pgno, pRC);
  }
}
#endif


/*
** Defragment the page given.  All Cells are moved to the
** end of the page and all free space is collected into one
** big FreeBlk that occurs in between the header and cell
** pointer array and the cell content area.
** ����������ҳ�档���еĵ�Ԫ��ת�Ƶ���ҳ�Ľ��������е����ɿռ䱻�ռ���
** ������ͷ���͵�Ԫָ�������С����������֮���һ�����FreeBlk��
*/
/*����ҳ�档*/
/*
���˹��䡿����ҳ�档���еĵ�Ԫ���Ƶ�ҳ��������е����ɿռ䱻�ռ���
һ�����FreeBlk������ͷ�͵�Ԫ��ָ���������������֮�䡣

*/
static int defragmentPage(MemPage *pPage){
  int i;                     /* Loop counter */                      //ѭ���ڵĲ���i  /*���˹��䡿ѭ��������*/
  int pc;                    /* Address of a i-th cell */            //��i����Ԫ�ĵ�ַ /*���˹��䡿һ����Ԫ���ַ*/
  int hdr;                   /* Offset to the page header */         //ҳͷ����ƫ����  /*���˹��䡿ƫ��ҳ��*/
  int size;                  /* Size of a cell */                    //��Ԫ�Ĵ�С     /
  int usableSize;            /* Number of usable bytes on a page */  //ҳ�Ͽ��õ�Ԫ������  /*���˹��䡿ҳ���Ͽ����ֽ���* 
  int cellOffset;            /* Offset to the cell pointer array */  //��Ԫָ�������ƫ����/*���˹��䡿ƫ�Ƶ���Ԫ��ָ������*/
  int cbrk;                  /* Offset to the cell content area */   //��Ԫ���������ƫ����/*���˹��䡿ƫ�Ƶ���Ԫ����������*/
  int nCell;                 /* Number of cells on the page */       //ҳ�ϵ�Ԫ������    /*���˹��䡿ҳ���ϵĵ�Ԫ����*/
  unsigned char *data;       /* The page data */                     //ҳ����
  unsigned char *temp;       /* Temp area for cell content */        //��Ԫ���ݵ���ʱ��   /*���˹��䡿��Ԫ�����ݵ���ʱ����*/
  int iCellFirst;            /* First allowable cell index */        //��һ����ɵĵ�Ԫ����    /*���˹��䡿����Ԫ�������ĵ�һ��*/
  int iCellLast;             /* Last possible cell index */          //���һ�����ܵĵ�Ԫ����   /*���˹��䡿���һ�����ܵĵ�Ԫ������*/


  assert( sqlite3PagerIswriteable(pPage->pDbPage) );
  assert( pPage->pBt!=0 );
  assert( pPage->pBt->usableSize <= SQLITE_MAX_PAGE_SIZE );
  assert( pPage->nOverflow==0 );
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  temp = sqlite3PagerTempSpace(pPage->pBt->pPager);
  data = pPage->aData;
  hdr = pPage->hdrOffset;
  cellOffset = pPage->cellOffset;
  nCell = pPage->nCell;
  assert( nCell==get2byte(&data[hdr+3]) );
  usableSize = pPage->pBt->usableSize;
  cbrk = get2byte(&data[hdr+5]);
  memcpy(&temp[cbrk], &data[cbrk], usableSize - cbrk);
  cbrk = usableSize;
  iCellFirst = cellOffset + 2*nCell;
  iCellLast = usableSize - 4;
  for(i=0; i<nCell; i++){
    u8 *pAddr;     /* The i-th cell pointer */  //��i����Ԫ��ָ��
    pAddr = &data[cellOffset + i*2];
    pc = get2byte(pAddr);
    testcase( pc==iCellFirst );
    testcase( pc==iCellLast );
#if !defined(SQLITE_ENABLE_OVERSIZE_CELL_CHECK)
    /* These conditions have already been verified in btreeInitPage()
    ** if SQLITE_ENABLE_OVERSIZE_CELL_CHECK is defined 
	** ���SQLITE_ENABLE_OVERSIZE_CELL_CHECK�������ˣ�����Щ�����Ѿ���btreeInitPage()�б���֤
    */
	/*
	���˹��䡿���sqlite_enable_oversize_cell_check����,����Щ�����Ѿ���֤��btreeinitpage()
	*/
    if( pc<iCellFirst || pc>iCellLast ){
      return SQLITE_CORRUPT_BKPT;
    }
#endif
    assert( pc>=iCellFirst && pc<=iCellLast );/*��һ������cell���������һ�����ܵ�cell����֮�䡣*/ /*���˹��䡿�ڵ�һ������cell���������һ�����ܵ�cell����֮�䡣*/
    size = cellSizePtr(pPage, &temp[pc]);
    cbrk -= size;
#if defined(SQLITE_ENABLE_OVERSIZE_CELL_CHECK)
    if( cbrk<iCellFirst ){         // ��Ԫ����ƫ�����ȵ�һ����Ԫ��ƫ������С��������
      return SQLITE_CORRUPT_BKPT;
    }
#else
    if( cbrk<iCellFirst || pc+size>usableSize ){
      return SQLITE_CORRUPT_BKPT;
    }
#endif
    assert( cbrk+size<=usableSize && cbrk>=iCellFirst );
    testcase( cbrk+size==usableSize );
    testcase( pc+size==usableSize );
    memcpy(&data[cbrk], &temp[pc], size);/*memcpy(&temp[cbrk], &data[cbrk], usableSize - cbrk);*/
    put2byte(pAddr, cbrk);
  }
  assert( cbrk>=iCellFirst );
  put2byte(&data[hdr+5], cbrk);
  data[hdr+1] = 0;
  data[hdr+2] = 0;
  data[hdr+7] = 0;
  memset(&data[iCellFirst], 0, cbrk-iCellFirst);
  assert( sqlite3PagerIswriteable(pPage->pDbPage) );
  if( cbrk-iCellFirst!=pPage->nFree ){
    return SQLITE_CORRUPT_BKPT;
  }
  return SQLITE_OK;
}

/*
** Allocate nByte bytes of space from within the B-Tree page passed
** as the first argument. Write into *pIdx the index into pPage->aData[]
** of the first byte of allocated space. Return either SQLITE_OK or
** an error code (usually SQLITE_CORRUPT).
** ��ͨ��B��ҳ�з���Ŀռ�nByte�ֽ���Ϊ��һ��������д��*pIdx������Page->aData[]�ķ���
** �ռ�ĵ�һ���ֽڣ�������ҪôSQLITE_OK��һ�������루ͨ��SQLITE_CORRUPT����
**
** The caller guarantees that there is sufficient space to make the
** allocation.  This routine might need to defragment in order to bring
** all the space together, however.  This routine will avoid using
** the first two bytes past the cell pointer area since presumably this
** allocation is being made in order to insert a new cell, so we will
** also end up needing a new cell pointer.
** ���ñ�֤���㹻�Ŀռ�ȥ���䡣���ǣ��ó��������Ҫ������Ƭ����ʹ���еĿռ�ϲ���һ��
** ������򽫱���ʹ��ͨ����Ԫָ��ָ�������ǰ�����ֽڣ��Ʋ�Ϊ�˲���һ���µĵ�Ԫ�����ڷ��䣬
** �������ǽ�Ҳ������Ҫһ���µĵ�Ԫ��ָ�롣
*/
/*��pPage�Ϸ���nByte�ֽڵĿռ䣬������д��pIdx��*/
/*
���˹��䡿**��pPage�Ϸ���nByte�ֽڵĿռ䣬������д��pIdx�з���ռ�ĵ�һ���ֽڡ�
����SQLITE_OK�������루ͨ��SQLITE_CORRUPT����
**���÷���֤���㹻�Ŀռ������з��䡣������������Ҫ������ܴ���
���еĿռ䣬���ǣ�������򽫱���ʹ�õ�һ��2���ֽڹ�ȥ�ĵ�Ԫ��ָ������
��Ϊ���������Ϊ�˲���һ���µĵ�Ԫ�����ǽ�����Ҳ��������Ҫһ���µĵ�Ԫ��ָ�롣
*/
static int allocateSpace(MemPage *pPage, int nByte, int *pIdx){
  const int hdr = pPage->hdrOffset;    /* Local cache of pPage->hdrOffset */       //pPage->hdrOffset�ı��ػ���
  u8 * const data = pPage->aData;      /* Local cache of pPage->aData */           //pPage->aData�ı��ػ���
  int nFrag;                           /* Number of fragmented bytes on pPage */   //ҳ�ϵ���Ƭ�ֽ���
  int top;                             /* First byte of cell content area */       //��Ԫ���ݵĵ�һ���ֽ�
  int gap;        /* First byte of gap between cell pointers and cell content */   //��Ԫָ��͵�Ԫ����֮���϶�ĵ�һ���ֽ�
  int rc;         /* Integer return code */                                        //���ͷ�����
  int usableSize; /* Usable size of the page */    //ҳ�ܹ�ʹ�õĴ�С/*���˹��䡿 ÿҳ���õ��ֽ�����pageSize-ÿҳβ�������ռ�Ĵ�С�����ļ�ͷƫ��Ϊ20���趨�� */
                        
  
  assert( sqlite3PagerIswriteable(pPage->pDbPage) );
  assert( pPage->pBt );
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  assert( nByte>=0 );  /* Minimum cell size is 4 */   //��С��Ԫ��СΪ4�ֽ�
  assert( pPage->nFree>=nByte );
  assert( pPage->nOverflow==0 );
  usableSize = pPage->pBt->usableSize;
  assert( nByte < usableSize-8 );

  nFrag = data[hdr+7];
  assert( pPage->cellOffset == hdr + 12 - 4*pPage->leaf );
  gap = pPage->cellOffset + 2*pPage->nCell;
  top = get2byteNotZero(&data[hdr+5]);
  if( gap>top ) return SQLITE_CORRUPT_BKPT;
  testcase( gap+2==top );
  testcase( gap+1==top );
  testcase( gap==top );

  if( nFrag>=60 ){
    /* Always defragment highly fragmented pages */  //������Ƭ�϶��ҳ
    rc = defragmentPage(pPage);
    if( rc ) return rc;
    top = get2byteNotZero(&data[hdr+5]);
  }else if( gap+2<=top ){
    /* Search the freelist looking for a free slot big enough to satisfy 
    ** the request. The allocation is made from the first free slot in 
    ** the list that is large enough to accomadate it.
	** ���������б�Ѱ������Ҫ����㹻��� free slot��������������б��е�
	** ��һ�� free slot��ɣ������б����㹻װ free slot��
    */
	  /*
	  ���˹��䡿���������б���Ѱ��һ�����ɲ۵��㹻��������Ҫ��
	  ���������б��е��Ǵ������������ĵ�һ������ʱ϶��
	  */
    int pc, addr;
    for(addr=hdr+1; (pc = get2byte(&data[addr]))>0; addr=pc){
      int size;            /* Size of the free slot */   // free slot�Ĵ�С  //���˹��䡿�������ɲ۵Ĵ�С
      if( pc>usableSize-4 || pc<addr+4 ){
        return SQLITE_CORRUPT_BKPT;
      }
      size = get2byte(&data[pc+2]);
      if( size>=nByte ){
        int x = size - nByte;
        testcase( x==4 );
        testcase( x==3 );
        if( x<4 ){
          /* Remove the slot from the free-list. Update the number of
          ** fragmented bytes within the page. */  //�������б����Ƴ�slot����ҳ�ڸ�����Ƭ������
			/*
			���˹��䡿�ӿ����б���ɾ����ۡ�����ҳ���ڵ���Ƭ�ֽ�����
			*/
          memcpy(&data[addr], &data[pc], 2);
          data[hdr+7] = (u8)(nFrag + x);
        }else if( size+pc > usableSize ){
          return SQLITE_CORRUPT_BKPT;
        }else{
          /* The slot remains on the free-list. Reduce its size to account   //slot�����������б��ϣ�
          ** for the portion used by the new allocation. */                 //������ռ��ʹ�õ��·���Ĳ��ֵĴ�С��
			/*���˹��䡿������������б��ϡ��������Ĵ�С��˵���·�����ʹ�õĲ��֡�*/
          put2byte(&data[pc+2], x);
        }
        *pIdx = pc + x;
        return SQLITE_OK; //����SQLITE_OK
      }
    }
  }

  /* Check to make sure there is enough space in the gap to satisfy
  ** the allocation.  If not, defragment.
  ** ���ȷ����gap�����㹻�Ŀռ�������������Ҫ������ռ䲻�㣬��Ƭ����
  */
   /*
 ���˹��䡿 ����϶��ȷ�����㹻�Ŀռ���������䡣���û�У�������
  */
  testcase( gap+2+nByte==top );
  if( gap+2+nByte>top ){
    rc = defragmentPage(pPage);
    if( rc ) return rc;
    top = get2byteNotZero(&data[hdr+5]);
    assert( gap+nByte<=top );
  }

  /* Allocate memory from the gap in between the cell pointer array
  ** and the cell content area.  The btreeInitPage() call has already
  ** validated the freelist.  Given that the freelist is valid, there
  ** is no way that the allocation can extend off the end of the page.
  ** The assert() below verifies the previous sentence.
  ** �ӵ�Ԫָ������͵�Ԫ��������֮��ļ�϶�����ڴ档��btreeInitPage����
  ** �����Ѿ�����Ч�Ŀ����б����ڿ����б�����Ч�ģ����������չ����ҳ
  ** �ǲ��еġ�assert()������֤��ǰ�����䡣
  */
  /*
 ���˹��䡿 ����洢���ӵ�Ԫָ�����к͵�Ԫ��������֮��ļ�϶�н��з��䡣
  btreeInitPage()��������Ч�Ŀ����б�������������Ч�ģ�û�����������ÿ����ӳ�ҳ�Ľ�����
   assert()����֤ǰ�ߵ���䡣
  */
  top -= nByte;
  put2byte(&data[hdr+5], top);
  assert( top+nByte <= (int)pPage->pBt->usableSize );
  *pIdx = top;
  return SQLITE_OK;
}

/*
** Return a section of the pPage->aData to the freelist.
** The first byte of the new free block is pPage->aDisk[start]
** and the size of the block is "size" bytes.
** �¿��п�ĵ�һ���ֽ���pPage->aDisk[start]�ҿ���ֽڴ�С��"size"�ֽڡ�
** Most of the effort here is involved in coalesing adjacent
** free blocks into a single big free block.
** ����pPage-> ADATA�Ĳ��ֵ������б��Ӷ����µĿ��п�ĵ�һ�ֽ���pPage-> aDisk[start]
** �Ϳ�Ĵ�СΪ��size���ֽڡ�����Ĵ���������漰�ϲ����ڿ��п��һ�������Ĵ���п顣
*/
/*�ͷ�pPage->aDisk[start]����СΪsize�ֽڵĿ�*/
/*
���˹��䡿�ͷ�pPage->aDisk[start]����СΪsize�ֽڵĿ�
����󲿷ֵľ�������coalesing���ڵĿ��п��һ����Ŀ��п顣

*/
static int freeSpace(MemPage *pPage, int start, int size){  //�ͷ�pPage->aData�Ĳ��ֲ�д������б�
  int addr, pbegin, hdr;
  int iLast;                        /* Largest possible freeblock offset */   //���Ŀ���freeblockƫ�� 
  unsigned char *data = pPage->aData;

  assert( pPage->pBt!=0 );
  assert( sqlite3PagerIswriteable(pPage->pDbPage) );
  assert( start>=pPage->hdrOffset+6+pPage->childPtrSize );
  assert( (start + size) <= (int)pPage->pBt->usableSize );
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  assert( size>=0 );   /* Minimum cell size is 4 */  //���˹��䡿��С��Ԫ�Ĵ�СΪ4

  if( pPage->pBt->btsFlags & BTS_SECURE_DELETE ){
    /* Overwrite deleted information with zeros when the secure_delete
    ** option is enabled */  //��secure_delete���õ�ʱ�򣬽�ɾ����Ϣ���㡣 /*���˹��䡿����ɾ����Ϣʱ��secure_delete���ѡ������*/
    memset(&data[start], 0, size);
  }

  /* Add the space back into the linked list of freeblocks.  Note that
  ** even though the freeblock list was checked by btreeInitPage(),
  ** btreeInitPage() did not detect overlapping cells or
  ** freeblocks that overlapped cells.   Nor does it detect when the
  ** cell content area exceeds the value in the page header.  If these
  ** situations arise, then subsequent insert operations might corrupt
  ** the freelist.  So we do need to check for corruption while scanning
  ** the freelist.
  ** ��ӿռ䵽���п�������С�ע�⼴ʹbtreeInitPage()�Ѽ������п��б�btreeInitPage()Ҳ���ܼ�⵽�ظ���Ԫ���ظ���Ԫ�Ŀ��п顣
  ** ����Ԫ�������򳬳���ҳͷ��ֵʱҲ�����⡣������������������ô���Ĳ���������ܻ��ƻ������б�
  ** ����������Ҫ����Ƿ����𻵣�ͬʱɨ������б�
  */ 
   /*
  ���˹��䡿��ӿռ�ص�freeblocks����ע�⣬��ʹfreeblock��������btreeinitpage()��btreeinitpage()û�м�⵽cells
  ��freeblocks�ص����ص�cells ������Ԫ���������򳬹���ҳͷ�е�ֵʱ����Ҳ����⡣
  �����Щ������֣���ô�����Ĳ���������ܻ������ݡ�����������Ҫ��ɨ��ʱ��鸯�������б�
  
  */
  hdr = pPage->hdrOffset;
  addr = hdr + 1;
  iLast = pPage->pBt->usableSize - 4;
  assert( start<=iLast );
  while( (pbegin = get2byte(&data[addr]))<start && pbegin>0 ){
    if( pbegin<addr+4 ){
      return SQLITE_CORRUPT_BKPT;
    }
    addr = pbegin;
  }
  if( pbegin>iLast ){
    return SQLITE_CORRUPT_BKPT;
  }
  assert( pbegin>addr || pbegin==0 );
  put2byte(&data[addr], start);
  put2byte(&data[start], pbegin);
  put2byte(&data[start+2], size);
  pPage->nFree = pPage->nFree + (u16)size;

  /* Coalesce adjacent free blocks */ //�ϲ����ڵĿ��п�
  addr = hdr + 1;
  while( (pbegin = get2byte(&data[addr]))>0 ){
    int pnext, psize, x;
    assert( pbegin>addr );
    assert( pbegin <= (int)pPage->pBt->usableSize-4 );
    pnext = get2byte(&data[pbegin]);
    psize = get2byte(&data[pbegin+2]);
    if( pbegin + psize + 3 >= pnext && pnext>0 ){/*�ϲ����п�*/
      int frag = pnext - (pbegin+psize);
      if( (frag<0) || (frag>(int)data[hdr+7]) ){
        return SQLITE_CORRUPT_BKPT;
      }
      data[hdr+7] -= (u8)frag;
      x = get2byte(&data[pnext]);
      put2byte(&data[pbegin], x);
      x = pnext + get2byte(&data[pnext+2]) - pbegin;
      put2byte(&data[pbegin+2], x);
    }else{
      addr = pbegin;
    }
  }

  /* If the cell content area begins with a freeblock, remove it. */  //�����Ԫ������������freeblock��ʼ,ɾ������
  /*���˹��䡿�����Ԫ����������ʼ�ǿ��п飬��ɾ������*/
  if( data[hdr+1]==data[hdr+5] && data[hdr+2]==data[hdr+6] ){
    int top;
    pbegin = get2byte(&data[hdr+1]);
    memcpy(&data[hdr+1], &data[pbegin], 2);
    top = get2byte(&data[hdr+5]) + get2byte(&data[pbegin+2]);
    put2byte(&data[hdr+5], top);
  }
  assert( sqlite3PagerIswriteable(pPage->pDbPage) );
  return SQLITE_OK;
}

/*
** Decode the flags byte (the first byte of the header) for a page
** and initialize fields of the MemPage structure accordingly.
** // Ϊһ��ҳ��MemPage��Ӧ�ṹ�ĳ�ʼ����������־�ֽڡ�
** Only the following combinations are supported.  Anything different
** indicates a corrupt database files:
** //ֻ֧��������ϡ��κβ�ͬ����ָʾһ�������������ļ�
**         PTF_ZERODATA
**         PTF_ZERODATA | PTF_LEAF
**         PTF_LEAFDATA | PTF_INTKEY
**         PTF_LEAFDATA | PTF_INTKEY | PTF_LEAF
*/
/*���˹��䡿����һҳ�ı���ֽڣ�ͷ���ĵ�һ���ֽڣ���ʼ����MemPage��Ӧ�ṹ��
ֻ����������֧�֡��κβ�ͬ��ָʾһ���𻵵����ݿ��ļ���
**         PTF_ZERODATA
**         PTF_ZERODATA | PTF_LEAF
**         PTF_LEAFDATA | PTF_INTKEY
**         PTF_LEAFDATA | PTF_INTKEY | PTF_LEAF
*/
static int decodeFlags(MemPage *pPage, int flagByte){
  BtShared *pBt;     /* A copy of pPage->pBt */   //pPage->pBt��һ������

  assert( pPage->hdrOffset==(pPage->pgno==1 ? 100 : 0) );
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  pPage->leaf = (u8)(flagByte>>3);  assert( PTF_LEAF == 1<<3 );
  flagByte &= ~PTF_LEAF;
  pPage->childPtrSize = 4-4*pPage->leaf;
  pBt = pPage->pBt;
  if( flagByte==(PTF_LEAFDATA | PTF_INTKEY) ){
    pPage->intKey = 1;
    pPage->hasData = pPage->leaf;
    pPage->maxLocal = pBt->maxLeaf;
    pPage->minLocal = pBt->minLeaf;
  }else if( flagByte==PTF_ZERODATA ){
    pPage->intKey = 0;
    pPage->hasData = 0;
    pPage->maxLocal = pBt->maxLocal;
    pPage->minLocal = pBt->minLocal;
  }else{
    return SQLITE_CORRUPT_BKPT;
  }
  pPage->max1bytePayload = pBt->max1bytePayload;
  return SQLITE_OK;
}

/*
** Initialize the auxiliary information for a disk block.
** ��ʼ�����̿�ĸ�����Ϣ��
** Return SQLITE_OK on success.  If we see that the page does
** not contain a well-formed database page, then return 
** SQLITE_CORRUPT.  Note that a return of SQLITE_OK does not
** guarantee that the page is well-formed.  It only shows that
** we failed to detect any corruption.
** �ɹ��򷵻�SQLITE OK��������ǿ���ҳ�治����һ����ʽ���õ����ݿ�ҳ��,Ȼ�󷵻�
** SQLITE_CORRUPT��ע��,SQLITE_OK�Ļع���Բ���֤ҳ��ĸ�ʽ����ȷ�ġ���ֻ��������ʧ����
*/
/*���˹��䡿��ʼ�����̿�ĸ�����Ϣ��
����sqlite_ok�ɹ���
������ǿ�����ҳû�����õ����ݿ�ҳ�棬Ȼ�󷵻�sqlite_corrupt��
ע�⣬����sqlite_ok����֤ҳ���Ǻܺõġ���ֻ˵������û�з����κ��쳣��*/
static int btreeInitPage(MemPage *pPage){     //B����ʼ��ҳ

  assert( pPage->pBt!=0 );
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  assert( pPage->pgno==sqlite3PagerPagenumber(pPage->pDbPage) );
  assert( pPage == sqlite3PagerGetExtra(pPage->pDbPage) );
  assert( pPage->aData == sqlite3PagerGetData(pPage->pDbPage) );

  if( !pPage->isInit ){
    u16 pc;            /* Address of a freeblock within pPage->aData[] */      //pPage->aData[]�ڲ��Ŀ��п�ĵ�ַ
    u8 hdr;            /* Offset to beginning of page header */                //ҳͷ��ʼ��ƫ����/*���˹��䡿 ��page 1Ϊ100��������ҳΪ0 */
    u8 *data;          /* Equal to pPage->aData */                             //����pPage->aData
    BtShared *pBt;     /* The main btree structure */                          //�ɹ����B���ṹ
    int usableSize;    /* Amount of usable space on each page */      //ÿ��ҳ�ϵĿ��ÿռ������ /* ���˹��䡿ÿҳ���õ��ֽ�����pageSize-ÿҳβ�������ռ�Ĵ�С�����ļ�ͷƫ��Ϊ20���趨��*/
																		
    u16 cellOffset;    /* Offset from start of page to first cell pointer */   //��ҳ��Ŀ�ʼ����һ����Ԫָ���ƫ����/* ���˹��䡿��Ԫָ�������ƫ������aData�е�1����Ԫ��ָ�� */
    int nFree;         /* Number of unused bytes on the page */                //ҳ�ϲ���ʹ���ֽڵ�����  /* ���˹��䡿��ʹ�ÿռ���ܺͣ��ֽ����� */
    int top;           /* First byte of the cell content area */               //��Ԫ���ݵĵ�һ���ֽ�
    int iCellFirst;    /* First allowable cell or freeblock offset */          //��һ�����õ�Ԫ����п�ƫ����
    int iCellLast;     /* Last possible cell or freeblock offset */            //���һ�����ܵ�Ԫ����п�ƫ����

    pBt = pPage->pBt;

    hdr = pPage->hdrOffset;
    data = pPage->aData;
    if( decodeFlags(pPage, data[hdr]) ) return SQLITE_CORRUPT_BKPT;
    assert( pBt->pageSize>=512 && pBt->pageSize<=65536 );
    pPage->maskPage = (u16)(pBt->pageSize - 1);
    pPage->nOverflow = 0;
    usableSize = pBt->usableSize;
    pPage->cellOffset = cellOffset = hdr + 12 - 4*pPage->leaf;
    pPage->aDataEnd = &data[usableSize];
    pPage->aCellIdx = &data[cellOffset];
    top = get2byteNotZero(&data[hdr+5]);
    pPage->nCell = get2byte(&data[hdr+3]);
    if( pPage->nCell>MX_CELL(pBt) ){
      /* To many cells for a single page.  The page must be corrupt */  //���ڵ�ҳ������ɵ�ԪҲһ���ǲ�����
      return SQLITE_CORRUPT_BKPT;
    }
    testcase( pPage->nCell==MX_CELL(pBt) );

    /* A malformed(��ȱ�ݵ�) database page might cause us to read past the end
    ** of page when parsing a cell.  
    ** ������Ԫʱ��һ����ȱ�ݵ����ݿ�ҳ���ܻᵼ������ȥ��ҳ��ĩβ�Ĳ��֡�
    ** The following block of code checks early to see if a cell extends
    ** past the end of a page boundary and causes SQLITE_CORRUPT to be 
    ** returned if it does.
	** ����Ĵ���齫��ǰ�˶��Ƿ�һ����Ԫ��չ����ҳ��߽磬�������ȷʵ���SQLITE_CORRUPT�������ء�
    */
	/*
	���˹��䡿 ��ȱ�ݵ����ݿ����ҳ����ܻ�Ϊ��ȥ�Ķ�����ʱ,���з���һ����Ԫ��
	����Ĵ�����飬�����Ƿ���һ����Ԫ���ڹ�ȥ�����һҳ�ı߽硣
	�������ԭ���� SQLITE_CORRUPT�������Ƿ��صġ�
	*/
    iCellFirst = cellOffset + 2*pPage->nCell;
    iCellLast = usableSize - 4;
#if defined(SQLITE_ENABLE_OVERSIZE_CELL_CHECK)
    {
      int i;            /* Index into the cell pointer array */   //����Ԫָ����������� /*���˹��䡿����һ����������Ԫ�������ָ��ı���i*/
      int sz;           /* Size of a cell */      //��Ԫ�Ĵ�С

      if( !pPage->leaf ) iCellLast--;
      for(i=0; i<pPage->nCell; i++){
        pc = get2byte(&data[cellOffset+i*2]);
        testcase( pc==iCellFirst );
        testcase( pc==iCellLast );
        if( pc<iCellFirst || pc>iCellLast ){
          return SQLITE_CORRUPT_BKPT;
        }
        sz = cellSizePtr(pPage, &data[pc]);
        testcase( pc+sz==usableSize );
        if( pc+sz>usableSize ){
          return SQLITE_CORRUPT_BKPT;
        }
      }
      if( !pPage->leaf ) iCellLast++;
    }  
#endif

    /* Compute the total free space on the page */  //����ҳ�������ɿռ������ /*���˹��䡿����ҳ���ϵ��ܿ��пռ�*/
    pc = get2byte(&data[hdr+1]);
    nFree = data[hdr+7] + top;
    while( pc>0 ){
      u16 next, size;
      if( pc<iCellFirst || pc>iCellLast ){
        /* Start of free block is off the page */  //���п�Ŀ�ʼ����ҳ���� /*���˹��䡿�������п��ǹر�ҳ��*/
        return SQLITE_CORRUPT_BKPT; 
      }
      next = get2byte(&data[pc]);
      size = get2byte(&data[pc+2]);
      if( (next>0 && next<=pc+size+3) || pc+size>usableSize ){
        /* Free blocks must be in ascending order. And the last byte of
        ** the free-block must lie on the database page.  
		** ���п������һ�����E��˳�򡣲��ҿ��п������һ���ֽ�һ������һ�����ݿ�ҳ�ϵ�*/
		  /*���˹��䡿���п���밴�������С��Ϳ��п�����һ���ֽڱ���λ�����ݿ�ҳ*/
        return SQLITE_CORRUPT_BKPT; 
      }
      nFree = nFree + size;
      pc = next;
    }

    /* At this point, nFree contains the sum of the offset to the start
    ** of the cell-content area plus the number of free bytes within
    ** the cell-content area. If this is greater than the usable-size
    ** of the page, then the page must be corrupted. This check also
    ** serves to verify that the offset to the start of the cell-content
    ** area, according to the page header, lies within the page.
	** ��ʱ��nFree����ƫ������������ƫ�����ǵ�Ԫ��������ʼ���ּ��ϵ�Ԫ�������ڵĿ����ֽڵ������ĺ͡�
	** ������ҳ��Ŀ��ô�С�������ҳ����뱻�ƻ�������ҳͷ���˼�黹������֤λ�ڸ�ҳ���ڵĵ�Ԫ��������ʼ���ֵ�ƫ������
    */
	/*
	���˹��䡿**����һ���ϣ�nFree����ƫ�Ƶ��ܺ͵���������ʼ�ӵ�Ԫ�����ݷ�Χ�ڿ��õ��ֽ�����
	������Ǵ���ҳ��Ŀ��ô�С����ҳ����뱻�𻵡�
	�˼��Ҳ��������֤�õ�Ԫ�������������ʼƫ���������ݸ�ҳͷ��λ�ڸ�ҳ�С�
	*/
    if( nFree>usableSize ){
      return SQLITE_CORRUPT_BKPT; 
    }
    pPage->nFree = (u16)(nFree - iCellFirst);
    pPage->isInit = 1;
  }
  return SQLITE_OK;
}

/*
** Set up a raw page so that it looks like a database page holding
** no entries.  //����һ��ԭʼҳ��,�Ա�����������һ�����ݿ�û����Ŀ��
*/
/*���˹��䡿����һ��ԭʼҳ�棬��������������һ�����ݿ�ҳ��û���κ���Ŀ��*/
static void zeroPage(MemPage *pPage, int flags){
  unsigned char *data = pPage->aData;
  BtShared *pBt = pPage->pBt;
  u8 hdr = pPage->hdrOffset;
  u16 first;

  assert( sqlite3PagerPagenumber(pPage->pDbPage)==pPage->pgno );
  assert( sqlite3PagerGetExtra(pPage->pDbPage) == (void*)pPage );
  assert( sqlite3PagerGetData(pPage->pDbPage) == data );
  assert( sqlite3PagerIswriteable(pPage->pDbPage) );
  assert( sqlite3_mutex_held(pBt->mutex) );
  if( pBt->btsFlags & BTS_SECURE_DELETE ){
    memset(&data[hdr], 0, pBt->usableSize - hdr);
  }
  data[hdr] = (char)flags;
  first = hdr + 8 + 4*((flags&PTF_LEAF)==0 ?1:0);
  memset(&data[hdr+1], 0, 4);
  data[hdr+7] = 0;
  put2byte(&data[hdr+5], pBt->usableSize);
  pPage->nFree = (u16)(pBt->usableSize - first);
  decodeFlags(pPage, flags);
  pPage->hdrOffset = hdr;
  pPage->cellOffset = first;
  pPage->aDataEnd = &data[pBt->usableSize];
  pPage->aCellIdx = &data[first];
  pPage->nOverflow = 0;
  assert( pBt->pageSize>=512 && pBt->pageSize<=65536 );
  pPage->maskPage = (u16)(pBt->pageSize - 1);
  pPage->nCell = 0;
  pPage->isInit = 1;
}


/*
** Convert a DbPage obtained from the pager into a MemPage used by
** the btree layer.  //ͨ��B���㣬��DbPageת����MemPage
*/
/*convert DbPage into MemPage*//*���˹��䡿����pager�л�õ�DbPageת��Ϊbtree��ʹ�õ�MemPage*/
static MemPage *btreePageFromDbPage(DbPage *pDbPage, Pgno pgno, BtShared *pBt){
  MemPage *pPage = (MemPage*)sqlite3PagerGetExtra(pDbPage);
  pPage->aData = sqlite3PagerGetData(pDbPage);
  pPage->pDbPage = pDbPage;
  pPage->pBt = pBt;
  pPage->pgno = pgno;
  pPage->hdrOffset = pPage->pgno==1 ? 100 : 0;
  return pPage; 
}

/*
** Get a page from the pager.  Initialize the MemPage.pBt and
** MemPage.aData elements if needed.
** ��ҳ����õ�һ��ҳ�������Ҫ�����ʼ��MemPage.pBt��MemPage.aData��Ԫ��
** If the noContent flag is set, it means that we do not care about
** the content of the page at this time.  So do not go to the disk
** to fetch the content.  Just fill in the content with zeros for now.
** If in the future we call sqlite3PagerWrite() on this page, that
** means we have started to be concerned about content and the disk
** read should occur at that point.
** ��������ݱ�ǩ�趨�ˣ�����ζ�����ǽ������Ĵ�ʱ��ҳ�����ݡ����Բ�Ҫȥ���̻�ȡ���ݡ�ֻ������������дʹ���㼴�ɡ�
** ����Ժ����������ҳ���ϵ���sqlite3PagerWrite����������ζ�������Ѿ���ʼ��ע���ݣ���Ӧ�����ڸõ�Ĵ��̶�ȡ��*/
/*
���˹��䡿**�����Ҫ������г�ʼ�� MemPage.pBt��MemPage.aData��Ԫ��
**���noContent��־���ã�����ζ�����ǲ��ں���ʱ��ҳ�����ݡ����Բ�Ҫȥ���̻�ȡ���ݡ�ֻ����д�����������ڡ�
**����������ǵ���sqlite3pagerwrite()���ҳ���ϣ�����ζ�������Ѿ���ʼ��ע���ݺͶ���Ӧ�÷�������һ�㡣

*/
static int btreeGetPage(
  BtShared *pBt,       /* The btree */                          //B��
  Pgno pgno,           /* Number of the page to fetch */        //��ȡ��ҳ����  /*���˹��䡿��ҳ��ҳ��*/
  MemPage **ppPage,    /* Return the page in this parameter */  //�������������ҳ  /*���˹��䡿���ش˲����е�ҳ*/
  int noContent        /* Do not load page content if true */   //���Ϊ�棬�򲻻����ҳ   /*���˹��䡿������,��Ҫ����ҳ������*/
){
  int rc;
  DbPage *pDbPage;

  assert( sqlite3_mutex_held(pBt->mutex) );
  rc = sqlite3PagerAcquire(pBt->pPager, pgno, (DbPage**)&pDbPage, noContent);
  if( rc ) return rc; 
  *ppPage = btreePageFromDbPage(pDbPage, pgno, pBt);/*��pager�л�ȡpage������ppPage��*/
  return SQLITE_OK;
}

/*
** Retrieve a page from the pager cache. If the requested page is not
** already in the pager cache return NULL. Initialize the MemPage.pBt and
** MemPage.aData elements if needed.
** ��ҳ���󻺴����һ��ҳ�档���û�ж�����NULL�����б�Ҫ����ʼ��MemPage.pBt��MemPage.aData��Ԫ��*/
/*���˹��䡿�ӻ���ҳ��������������ҳ���ڻ��淵��null�������Ҫ��ʼ��mempage.pbt��mempage.adataԪ��*/
static MemPage *btreePageLookup(BtShared *pBt, Pgno pgno){
  DbPage *pDbPage;
  assert( sqlite3_mutex_held(pBt->mutex) );
  pDbPage = sqlite3PagerLookup(pBt->pPager, pgno);
  if( pDbPage ){
    return btreePageFromDbPage(pDbPage, pgno, pBt);/*��pager cache��ȡpage*/
  }
  return 0;
}

/*
** Return the size of the database file in pages. If there is any kind of
** error, return ((unsigned int)-1).
** ����ҳ�����ݿ��ļ��Ĵ�С�����д�return ((unsigned int)-1)*/
/*���˹��䡿�������ݿ��ļ��Ĵ�С��������κδ���,�򷵻�((unsigned int)-1)*/
static Pgno btreePagecount(BtShared *pBt){
  return pBt->nPage;
}
u32 sqlite3BtreeLastPage(Btree *p){
  assert( sqlite3BtreeHoldsMutex(p) );
  assert( ((p->pBt->nPage)&0x8000000)==0 );
  return (int)btreePagecount(p->pBt);
}

/*
** Get a page from the pager and initialize it.  This routine is just a
** convenience wrapper around separate calls to btreeGetPage() and 
** btreeInitPage().
** ��ҳ�����л��һ��ҳ�沢��ʼ�����������ֻһ�����ڷֱ����btreeGetPage������btreeInitPage�����ı�ݵİ���
** If an error occurs, then the value *ppPage is set to is undefined. It
** may remain unchanged, or it may be set to an invalid value.
** ��������������ֵ* ppPage������Ϊδ���塣�����Ա��ֲ��䣬���������Ա�����Ϊ��Чֵ��*/
/*
���˹��䡿**��ʼ����������������һ������İ�װ����������btreegetpage()��btreeinitpage()��
**������ִ�����ôֵ* pppage����δ����ġ������Ա��ֲ��䣬����ܱ�����Ϊ��Чֵ��
*/
static int getAndInitPage(
  BtShared *pBt,          /* The database file */         //���ݿ��ļ�
  Pgno pgno,           /* Number of the page to get */    //��õ�ҳ������� /*���˹��䡿��ñ�ҳ��ҳ��*/
  MemPage **ppPage     /* Write the page pointer here */  //�ڸñ�����дָ��
){
  int rc;
  assert( sqlite3_mutex_held(pBt->mutex) );

  if( pgno>btreePagecount(pBt) ){
    rc = SQLITE_CORRUPT_BKPT;
  }else{
    rc = btreeGetPage(pBt, pgno, ppPage, 0); /*Get a page from the pager*/
    if( rc==SQLITE_OK ){
      rc = btreeInitPage(*ppPage);/*��ʼ��page*/
      if( rc!=SQLITE_OK ){/*ppPage��ֵδ�����塣����ֵ����δ�仯����Ϊ��Чֵ��*/
        releasePage(*ppPage);
      }
    }
  }

  testcase( pgno==0 );
  assert( pgno!=0 || rc==SQLITE_CORRUPT );
  return rc;
}

/*
** Release a MemPage.  This should be called once for each prior
** call to btreeGetPage.
** ÿ�ε���֮ǰӦ�ñ�����btreeGetPageһ�Ρ�*/
/*�ͷ��ڴ�ҳ*/
static void releasePage(MemPage *pPage){
  if( pPage ){
    assert( pPage->aData );
    assert( pPage->pBt );
    assert( sqlite3PagerGetExtra(pPage->pDbPage) == (void*)pPage );
    assert( sqlite3PagerGetData(pPage->pDbPage)==pPage->aData );
    assert( sqlite3_mutex_held(pPage->pBt->mutex) );
    sqlite3PagerUnref(pPage->pDbPage);/*�ͷ�ҳ�ϵ�����*/
  }
}

/*
** During a rollback, when the pager reloads(����װ) information into the cache
** so that the cache is restored to its original state at the start of
** the transaction, for each page restored this routine is called.
** �ڻع��ڼ䣬��pager��������װ����Ϣ�������Ա��ڻ���ָ�������ʼ�Ļ���ԭʼ״̬��ʱ�򣬶���ÿ��ҳ��ָ��������������
** This routine needs to reset the extra data section at the end of the
** page to agree with the restored data.
** ���������Ҫ��ҳ�����������趨��������ݲ������ʺϻָ����� */
/*�ع���ҳ����װinformation��cache��*/

/*
���˹��䡿**�ع���ҳ����װinformation��cache����ˣ�������ʼʱ���û��潫�ָ�������ԭʼ״̬��
����ÿһ��ҳ�涼�ָ����������ĵ��á�
**�ó�����Ҫ����ҳ��Ķ������ݶ�����ָ�������һ�¡�
*/
static void pageReinit(DbPage *pData){    //pager��������װ����Ϣ������
  MemPage *pPage;
  pPage = (MemPage *)sqlite3PagerGetExtra(pData);
  assert( sqlite3PagerPageRefcount(pData)>0 );
  if( pPage->isInit ){
    assert( sqlite3_mutex_held(pPage->pBt->mutex) );
    pPage->isInit = 0;
    if( sqlite3PagerPageRefcount(pData)>1 ){
      /* pPage might not be a btree page;  it might be an overflow page
      ** or ptrmap page or a free page.  In those cases, the following  //pPage���ܲ���һ��B��ҳ��;��������һ�����ҳ���ptrmapҳ��հ�ҳ
      ** call to btreeInitPage() will likely return SQLITE_CORRUPT.     //����Щ����£������btreeInitPage()�ĵ��ÿ��ܷ���SQLITE_CORRUPT��
      ** But no harm is done by this.  And it is very important that    //��ÿһ��B��ҳ�ϵ���btreeInitPage()�Ǻ���Ҫ�ģ�
      ** btreeInitPage() be called on every btree page so we make       //�������Ϊÿһ�����³�ʼ����ÿһ��ҳ�淢����������
      ** the call for every page that comes in for re-initing. 
	  */	
	/*���˹��䡿ҳ���ܲ���Btreeҳ����������һ�����ҳ��ptrmapҳ��һ�����е���ҳ��
		����������£�����ĵ��û᷵��sqlite_corrupt btreeinitpage()������û�к����ġ�
		���Ƿǳ���Ҫ�ģ�btreeinitpage()��ÿ��B��ҳ������������������ÿһҳ���������³�ʼ�����á�
		
		*/
      btreeInitPage(pPage);
    }
  }
}

/*
** Invoke the busy handler for a btree.      //����btree��æ�Ĵ������
*/
static int btreeInvokeBusyHandler(void *pArg){
  BtShared *pBt = (BtShared*)pArg;
  assert( pBt->db );
  assert( sqlite3_mutex_held(pBt->db->mutex) );
  return sqlite3InvokeBusyHandler(&pBt->db->busyHandler);/*���˹��䡿����һ��btree��æ�Ĵ������*/
}

/*
** Open a database file.             //�����ݿ��ļ�
** 
** zFilename is the name of the database file.  If zFilename is NULL             
** then an ephemeral(���ݵ�) database is created.  The ephemeral database might 
** be exclusively in memory, or it might use a disk-based memory cache. 
** Either way, the ephemeral database will be automatically deleted              
** when sqlite3BtreeClose() is called. 
** zFilename��������ݿ��ļ������֡����zFilenameΪ�գ��򽫴���һ����ʱ���ݿ⡣
** �����ʱ���ݿ����ڴ���Ψһ�ģ������˻��ڴ��̵��ڴ滺���������ַ�ʽ��sqlite3BtreeClose()�����õ�ʱ��
** �����ʱ���ݿ⽫�Զ�ɾ����
**
** If zFilename is ":memory:" then an in-memory database is created  
** that is automatically destroyed when it is closed.
** ���zFilename��":memory:"��ô�ر�ʱ�Զ����ٵ��ڴ����ݿ⽫�ᴴ����
**
** The "flags" parameter is a bitmask that might contain bits like
** BTREE_OMIT_JOURNAL and/or BTREE_MEMORY.  
** ��flags��������һ�����ܰ�����λ����λBTREE_OMIT_JOURNAL��BTREE_MEMORY��

** If the database is already opened in the same database connection
** and we are in shared cache mode, then the open will fail with an
** SQLITE_CONSTRAINT error.  We cannot allow two or more BtShared
** objects in the same database connection since doing so will lead
** to problems with locking.
** ������ݿ��Ѿ�����ͬ�����ݿ������д��˲����ڹ�����ģʽ��,Ȼ����һ���򿪽���ʧ�ܷ���SQLITE_CONSTRAINT����
** ��ͬһ���ݿ����������ǲ���������������BtShared������Ϊ�������ᵼ�������⡣
*/
/*���˹��䡿�����ݿ��ļ���
**zfilename�����ݿ��ļ������ơ����zFilename��NULL���򴴽�һ�����ݵ����ݿ⡣
���ݵ����ݿ������ר�����ڴ��У�����������ʹ�û��ڴ��̵��ڴ滺�档�������ַ�ʽ��
���ݵ����ݿ⽫�Զ�ɾ��������sqlite3BtreeClose()ʱ��
**���zFilename��":memory:"��ôһ���ڴ����ݿ�Ĵ������ر�ʱ�Զ����١�
**flags������λ������ܰ���λ��BTREE_OMIT_JOURNAL and/or BTREE_MEMORY��
**������ݿ���ͬһ�����ݿ��������Ѵ��������ڹ�����ģʽ��Ȼ��򿪽�ʧ����sqlite_constraint��
���ǲ��������������������ϵ�btshared��ͬһ���ݿ������еĶ�����Ϊ�������������������⡣
*/
int sqlite3BtreeOpen(     //�����ݿ��ļ�������B������
  sqlite3_vfs *pVfs,      /* VFS to use for this b-tree */                      //VFSʹ��B��
  const char *zFilename,  /* Name of the file containing the BTree database */  //����B�����ݿ��ļ�������
  sqlite3 *db,            /* Associated database handle */                      //������ݿ���
  Btree **ppBtree,        /* Pointer to new Btree object written here */        //ָ���ڴ˱�д���µ�B������
  int flags,              /* Options */                                         //ѡ���ǩ
  int vfsFlags            /* Flags passed through to sqlite3_vfs.xOpen() */     //ͨ��sqlite3_vfs.xOpen()���
){
  BtShared *pBt = 0;             /* Shared part of btree structure */           //B���ṹ�Ĺ�����
  Btree *p;                      /* Handle to return */                         //���صľ��
  sqlite3_mutex *mutexOpen = 0;  /* Prevents a race condition. Ticket #3537 */  //���⾺̬��������ǩ#3537/*���˹��䡿��ֹ������������ǩ#3537*/
  int rc = SQLITE_OK;            /* Result code from this function */           //���������״̬��/*���˹��䡿�˺����Ľ������*/
  u8 nReserve;                   /* Byte of unused space on each page */        //ÿ��ҳ�ϵĲ��ÿռ���ֽ���
  unsigned char zDbHeader[100];  /* Database header content */                  //���ݿ��ļ�ͷ����

  /* True if opening an ephemeral, temporary database */
  const int isTempDb = zFilename==0 || zFilename[0]==0;/*zFilenameΪ�ձ�ʾ����ʱ���ݿ�*/

  /* Set the variable isMemdb to true for an in-memory database, or 
  ** false for a file-based database.
  ** �����ڴ����ݿ����ñ���isMemdb�棬���ڻ����ļ������ݿ����isMemdb��Ϊ�١�
  */
   /*
  ���˹��䡿���ñ���ismemdb����Ϊһ���ڴ����ݿ⣬����ٵ�һ�������ļ������ݿ⡣
  */
#ifdef SQLITE_OMIT_MEMORYDB
  const int isMemdb = 0;
#else
  const int isMemdb = (zFilename && strcmp(zFilename, ":memory:")==0)//���˹��䡿zFilenameΪ":memory:"��������Ϣ���ŵ��������У����ᱻд����̡�
                       || (isTempDb && sqlite3TempInMemory(db))
                       || (vfsFlags & SQLITE_OPEN_MEMORY)!=0;
#endif

  assert( db!=0 );
  assert( pVfs!=0 );
  assert( sqlite3_mutex_held(db->mutex) );
  assert( (flags&0xff)==flags );   /* flags fit in 8 bits */    //���ռ8���ֽ�

  /* Only a BTREE_SINGLE database can be BTREE_UNORDERED */  /*���˹��䡿ֻ��һ��btree_single���ݿ������btree_unordered*/
  assert( (flags & BTREE_UNORDERED)==0 || (flags & BTREE_SINGLE)!=0 );

  /* A BTREE_SINGLE database is always a temporary and/or ephemeral */  //BTREE_SINGLE���ݿ�������ʱ�ġ�
  assert( (flags & BTREE_SINGLE)==0 || isTempDb );

  if( isMemdb ){
    flags |= BTREE_MEMORY;
  }
  if( (vfsFlags & SQLITE_OPEN_MAIN_DB)!=0 && (isMemdb || isTempDb) ){
    vfsFlags = (vfsFlags & ~SQLITE_OPEN_MAIN_DB) | SQLITE_OPEN_TEMP_DB;
  }
  p = sqlite3MallocZero(sizeof(Btree));
  if( !p ){
    return SQLITE_NOMEM;
  }
  p->inTrans = TRANS_NONE;
  p->db = db;
#ifndef SQLITE_OMIT_SHARED_CACHE
  p->lock.pBtree = p;
  p->lock.iTable = 1;
#endif

#if !defined(SQLITE_OMIT_SHARED_CACHE) && !defined(SQLITE_OMIT_DISKIO)
  /*
  ** If this Btree is a candidate for shared cache, try to find an
  ** existing BtShared object that we can share with
  ** �����Btree�������Ǻ�ѡ��,�����ҵ�һ���ɹ���Ĵ��ڵ�BtShared����
  */
  /*���˹��䡿�����B����һ��������ĺ�ѡ,����ͼ�ҵ�һ�����е�btshared������������Ƿ���*/
  if( isTempDb==0 && (isMemdb==0 || (vfsFlags&SQLITE_OPEN_URI)!=0) ){
    if( vfsFlags & SQLITE_OPEN_SHAREDCACHE ){
      int nFullPathname = pVfs->mxPathname+1;
      char *zFullPathname = sqlite3Malloc(nFullPathname);
      MUTEX_LOGIC( sqlite3_mutex *mutexShared; )
      p->sharable = 1;
      if( !zFullPathname ){
        sqlite3_free(p);
        return SQLITE_NOMEM;
      }
      if( isMemdb ){
        memcpy(zFullPathname, zFilename, sqlite3Strlen30(zFilename)+1);
      }else{
        rc = sqlite3OsFullPathname(pVfs, zFilename,
                                   nFullPathname, zFullPathname);
        if( rc ){
          sqlite3_free(zFullPathname);
          sqlite3_free(p);
          return rc;
        }
      }
#if SQLITE_THREADSAFE
      mutexOpen = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_OPEN);
      sqlite3_mutex_enter(mutexOpen);
      mutexShared = sqlite3MutexAlloc(
	  	);
      sqlite3_mutex_enter(mutexShared);
#endif
      for(pBt=GLOBAL(BtShared*,sqlite3SharedCacheList); pBt; pBt=pBt->pNext){
        assert( pBt->nRef>0 );
        if( 0==strcmp(zFullPathname, sqlite3PagerFilename(pBt->pPager, 0))
                 && sqlite3PagerVfs(pBt->pPager)==pVfs ){
          int iDb;
          for(iDb=db->nDb-1; iDb>=0; iDb--){
            Btree *pExisting = db->aDb[iDb].pBt;
            if( pExisting && pExisting->pBt==pBt ){/*��ͬһ������cacheģʽ�£�����ͬ���ݿ������У����ݿ��Ǵ򿪵ģ�����SQLITE_CONSTRAINT*/
              sqlite3_mutex_leave(mutexShared);
              sqlite3_mutex_leave(mutexOpen);
              sqlite3_free(zFullPathname);
              sqlite3_free(p);
              return SQLITE_CONSTRAINT;
            }
          }
          p->pBt = pBt;
          pBt->nRef++;
          break;
        }
      }
      sqlite3_mutex_leave(mutexShared);
      sqlite3_free(zFullPathname);
    }
#ifdef SQLITE_DEBUG
    else{
      /* In debug mode, we mark all persistent databases as sharable
      ** even when they are not.  This exercises the locking code and
      ** gives more opportunity for asserts(sqlite3_mutex_held())
      ** statements to find locking problems.
	  ** �ڵ���ģʽ��,���ǽ����г־û����ݿ���Ϊ�ɹ���ģ���ʹ���ǲ��ǳ־û��ġ�����ϰ��������
	  ** ��asserts(sqlite3_mutex_held())��������Ļ����ҵ������⡣
      */

/*���˹��䡿�ڵ���ģʽ�£����Ǳ�����г������ݿ⣬��ʹ���ǲ������ṩ�����asserts(sqlite3_mutex_held())������ҵ��������⡣
*/
      p->sharable = 1;
    }
#endif
  }
#endif
  if( pBt==0 ){
    /*
    ** The following asserts make sure that structures used by the btree are
    ** the right size.  This is to guard against size changes that result
    ** when compiling on a different architecture.
	** ����Ķ�����ȷ��B��ʹ�õĽṹ�Ĵ�С����ȷ�ġ�����Ϊ�˷�ֹ���벻ͬ�ļܹ�ʱ��С�仯�Ľ����
    */
	  /*
	  ���˹��䡿���¶���ȷ��ʹ�õ�B���ṹ��ȷ�Ĵ�С��������һ����ͬ����ϵ�ṹ����ʱ���Խ���Ĵ�С�仯���б�����
	  */
    assert( sizeof(i64)==8 || sizeof(i64)==4 );
    assert( sizeof(u64)==8 || sizeof(u64)==4 );
    assert( sizeof(u32)==4 );
    assert( sizeof(u16)==2 );
    assert( sizeof(Pgno)==4 );
  
    pBt = sqlite3MallocZero( sizeof(*pBt) );
    if( pBt==0 ){
      rc = SQLITE_NOMEM;
      goto btree_open_out;
    }
    rc = sqlite3PagerOpen(pVfs, &pBt->pPager, zFilename,
                          EXTRA_SIZE, flags, vfsFlags, pageReinit);
    if( rc==SQLITE_OK ){
      rc = sqlite3PagerReadFileheader(pBt->pPager,sizeof(zDbHeader),zDbHeader);
    }
    if( rc!=SQLITE_OK ){
      goto btree_open_out;
    }
    pBt->openFlags = (u8)flags;
    pBt->db = db;
    sqlite3PagerSetBusyhandler(pBt->pPager, btreeInvokeBusyHandler, pBt);
    p->pBt = pBt;
  
    pBt->pCursor = 0;
    pBt->pPage1 = 0;
    if( sqlite3PagerIsreadonly(pBt->pPager) ) pBt->btsFlags |= BTS_READ_ONLY;
#ifdef SQLITE_SECURE_DELETE
    pBt->btsFlags |= BTS_SECURE_DELETE;
#endif
    pBt->pageSize = (zDbHeader[16]<<8) | (zDbHeader[17]<<16);
    if( pBt->pageSize<512 || pBt->pageSize>SQLITE_MAX_PAGE_SIZE
         || ((pBt->pageSize-1)&pBt->pageSize)!=0 ){
      pBt->pageSize = 0;
#ifndef SQLITE_OMIT_AUTOVACUUM
      /* If the magic name ":memory:" will create an in-memory database, then
      ** leave the autoVacuum mode at 0 (do not auto-vacuum), even if
      ** SQLITE_DEFAULT_AUTOVACUUM is true. On the other hand, if
      ** SQLITE_OMIT_MEMORYDB has been defined, then ":memory:" is just a
      ** regular file-name. In this case the auto-vacuum applies as per normal.
	  ** ���magic��Ϊ":memory:"��������һ���ڴ��е����ݿ�,Ȼ��ʹautoVacuumģʽΪ0(����Ҫauto-vacuum)
	  ** ��ʹSQLITE_DEFAULT_AUTOVACUUMֵΪ�档��һ���棬���SQLITE_OMIT_MEMORYDB�Ѿ������壬��":memory:"
	  ** ��һ��������ļ�������������£�auto-vacuum����ʹ�á�
      */
	  /*
	  **������magicΪ ":memory:"������һ���ڴ����ݿ⣬Ȼ���autovacuumģʽ0��������auto-vacuum����
        ��ʹsqlite_default_autovacuumΪ�档��һ���棬���sqlite_omit_memorydb�Ѿ������壬
		��ô":memory:"ֻ��һ����ͨ���ļ���������������£�auto-vacuum����������
	  */
      if( zFilename && !isMemdb ){
        pBt->autoVacuum = (SQLITE_DEFAULT_AUTOVACUUM ? 1 : 0);
        pBt->incrVacuum = (SQLITE_DEFAULT_AUTOVACUUM==2 ? 1 : 0);
      }
#endif
      nReserve = 0;
    }else{
      nReserve = zDbHeader[20];
      pBt->btsFlags |= BTS_PAGESIZE_FIXED;
#ifndef SQLITE_OMIT_AUTOVACUUM
      pBt->autoVacuum = (get4byte(&zDbHeader[36 + 4*4])?1:0);
      pBt->incrVacuum = (get4byte(&zDbHeader[36 + 7*4])?1:0);
#endif
    }
    rc = sqlite3PagerSetPagesize(pBt->pPager, &pBt->pageSize, nReserve);
    if( rc ) goto btree_open_out;
    pBt->usableSize = pBt->pageSize - nReserve;
    assert( (pBt->pageSize & 7)==0 );  /* 8-byte alignment of pageSize *//*8�ֽ�ƽ���ҳ��С*/
   
#if !defined(SQLITE_OMIT_SHARED_CACHE) && !defined(SQLITE_OMIT_DISKIO)
    /* Add the new BtShared object to the linked list sharable BtShareds.
    ** ����µ�BtShared���󵽿ɹ����BtShareds������*/
    if( p->sharable ){
      MUTEX_LOGIC( sqlite3_mutex *mutexShared; )
      pBt->nRef = 1;
      MUTEX_LOGIC( mutexShared = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER);)
      if( SQLITE_THREADSAFE && sqlite3GlobalConfig.bCoreMutex ){
        pBt->mutex = sqlite3MutexAlloc(SQLITE_MUTEX_FAST);?        if( pBt->mutex==0 ){
          rc = SQLITE_NOMEM;
          db->mallocFailed = 0;
          goto btree_open_out;
        }
      }
      sqlite3_mutex_enter(mutexShared);
      pBt->pNext = GLOBAL(BtShared*,sqlite3SharedCacheList);
      GLOBAL(BtShared*,sqlite3SharedCacheList) = pBt;
      sqlite3_mutex_leave(mutexShared);
    }
#endif
  }

#if !defined(SQLITE_OMIT_SHARED_CACHE) && !defined(SQLITE_OMIT_DISKIO)
  /* If the new Btree uses a sharable pBtShared, then link the new
  ** Btree into the list of all sharable Btrees for the same connection.
  ** The list is kept in ascending order by pBt address.
  ** ����µ�Btreeʹ�ÿɹ���pBtShared,��ô������ͬ�����ӣ�������B�������пɹ���Btree���б��б�pBt�ĵ�ַ��������
  */
  /*
 ���˹��䡿 ����µ�B��ʹ��һ�������pBtShared��Ȼ�������µ�B�������й���Btrees�б���ͬ�����ӡ�
  ���б�����������PBT��ַ˳��
  */
  if( p->sharable ){
    int i;
    Btree *pSib;
    for(i=0; i<db->nDb; i++){
      if( (pSib = db->aDb[i].pBt)!=0 && pSib->sharable ){
        while( pSib->pPrev ){ pSib = pSib->pPrev; }
        if( p->pBt<pSib->pBt ){ /*�����пɹ���B����������һ��*/
          p->pNext = pSib;
          p->pPrev = 0;
          pSib->pPrev = p;
        }else{
          while( pSib->pNext && pSib->pNext->pBt<p->pBt ){
            pSib = pSib->pNext;
          }
          p->pNext = pSib->pNext;
          p->pPrev = pSib;
          if( p->pNext ){
            p->pNext->pPrev = p;
          }
          pSib->pNext = p;
        }
        break;
      }
    }
  }
#endif
  *ppBtree = p;

btree_open_out:
  if( rc!=SQLITE_OK ){
    if( pBt && pBt->pPager ){
      sqlite3PagerClose(pBt->pPager);
    }
    sqlite3_free(pBt);
    sqlite3_free(p);
    *ppBtree = 0;
  }else{
    /* If the B-Tree was successfully opened, set the pager-cache size to the
    ** default value. Except, when opening on an existing shared pager-cache,
    ** do not change the pager-cache size.
	** ���B���򿪳ɹ���������ҳ�滺��Ĵ�СΪĬ��ֵ������֮�⣬����һ���Ѵ��ڵĿɹ���ҳ�滺���ϴ�ʱ����Ҫ�ı�ҳ�滺��Ĵ�С��
    */
	  /*
	  ���˹��䡿���B�����ɹ��򿪣����û����С��Ĭ��ֵ��ֻ�ǣ�����һ�����еĹ����棬���ı仺���С��
	  */
    if( sqlite3BtreeSchema(p, 0, 0)==0 ){
      sqlite3PagerSetCachesize(p->pBt->pPager, SQLITE_DEFAULT_CACHE_SIZE);
    }
  }
  if( mutexOpen ){
    assert( sqlite3_mutex_held(mutexOpen) );
    sqlite3_mutex_leave(mutexOpen);
  }
  return rc;
}

/*
** Decrement the BtShared.nRef counter.  When it reaches zero,
** remove the BtShared structure from the sharing list.  Return
** true if the BtShared.nRef counter reaches zero and return
** false if it is still positive.
** �ݼ�BtShared.nRef�������������ﵽ��ʱ���ӹ����б���ɾ��BtShared�ṹ��
** ���BtShared.nRef�������ﵽ�㷵��true���������ȻΪ��������false��
*/
/*
���˹��䡿BtShared.nRef�ݼ������������������㣬�ӹ����б���ɾ��BtShared�ṹ��
���BtShared.nRef�������ﵽ�㣬������ȷ�����������Ȼ�������򷵻ش���
*/
static int removeFromSharingList(BtShared *pBt){
#ifndef SQLITE_OMIT_SHARED_CACHE
  MUTEX_LOGIC( sqlite3_mutex *pMaster; )
  BtShared *pList;
  int removed = 0;

  assert( sqlite3_mutex_notheld(pBt->mutex) );
  MUTEX_LOGIC( pMaster = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MASTER); )
  sqlite3_mutex_enter(pMaster);
  pBt->nRef--;
  if( pBt->nRef<=0 ){
    if( GLOBAL(BtShared*,sqlite3SharedCacheList)==pBt ){
      GLOBAL(BtShared*,sqlite3SharedCacheList) = pBt->pNext;/*��BtShared.nRef counter��Ϊ0����pBt�ӷ����б���ɾ����*/
    }else{
      pList = GLOBAL(BtShared*,sqlite3SharedCacheList);
      while( ALWAYS(pList) && pList->pNext!=pBt ){
        pList=pList->pNext;
      }
      if( ALWAYS(pList) ){
        pList->pNext = pBt->pNext;
      }
    }
    if( SQLITE_THREADSAFE ){
      sqlite3_mutex_free(pBt->mutex);
    }
    removed = 1;
  }
  sqlite3_mutex_leave(pMaster);
  return removed;
#else
  return 1;
#endif
}

/*
** Make sure pBt->pTmpSpace points to an allocation of 
** MX_CELL_SIZE(pBt) bytes.
** ȷ��pBt->pTmpSpaceָ��һ��MX_CELL_SIZE(pBt)�ֽڵķ���
*/
static void allocateTempSpace(BtShared *pBt){
  if( !pBt->pTmpSpace ){
    pBt->pTmpSpace = sqlite3PageMalloc( pBt->pageSize );/*ȷ��pBt->pTmpSpaceָ��MX_CELL_SIZE(pBt)�ֽ�*/
  }
}

/*
** Free the pBt->pTmpSpace allocation  //�ͷ�pBt->pTmpSpace����
*/
static void freeTempSpace(BtShared *pBt){
  sqlite3PageFree( pBt->pTmpSpace);
  pBt->pTmpSpace = 0;//���˹��䡿��pBt->pTmpSpace�����ͷ�
}

/*
** Close an open database and invalidate all cursors. //�ر��Ѵ򿪵����ݿⲢ��ʹ�α���Ч
*/
int sqlite3BtreeClose(Btree *p){/*���˹��䡿����һ���ر��Ѵ򿪵����ݿ����Ч�������α�ĺ�����*/
  BtShared *pBt = p->pBt;
  BtCursor *pCur;

  /* Close all cursors opened via this handle.  */  //ͨ�������ر����д򿪵��αꡣ
  assert( sqlite3_mutex_held(p->db->mutex) );
  sqlite3BtreeEnter(p);
  pCur = pBt->pCursor;
  while( pCur ){
    BtCursor *pTmp = pCur;
    pCur = pCur->pNext;
    if( pTmp->pBtree==p ){		
      sqlite3BtreeCloseCursor(pTmp);/* ʹ�����α���Ч *//* ���˹��䡿����ʹ�����α���Ч�ĺ��� */
    }
  }

  /* Rollback any active transaction and free the handle structure.
  ** The call to sqlite3BtreeRollback() drops any table-locks held by
  ** this handle.
  ** �ع��κλ�������ͷž���ṹ������sqlite3BtreeRollback()��ɾ�������������е��κ����ꡣ
  */
  /*���˹��䡿�ع��κλ���񣬲��ͷž���ṹ.����sqlite3BtreeRollback()��ɾ�������������е����б�����*/
  sqlite3BtreeRollback(p, SQLITE_OK);/*ɾ�����������������е����б���*/
  sqlite3BtreeLeave(p);

  /* If there are still other outstanding references to the shared-btree
  ** structure, return now. The remainder of this procedure cleans 
  ** up the shared-btree.
  ** �����Ȼ������δ����Ķ�shared-btree�ṹ�����ã����������ء������ʣ�ಿ������shared-btree��
  */
  assert( p->wantToLock==0 && p->locked==0 );
  if( !p->sharable || removeFromSharingList(pBt) ){
    /* The pBt is no longer on the sharing list, so we can access
    ** it without having to hold the mutex.
    ** pBt���������ڹ����б��ϣ��������ǿ��Է�������������л�������
    ** Clean out and delete the BtShared object.
    ** ����ɾ��BtShared���� */
    assert( !pBt->pCursor );
    sqlite3PagerClose(pBt->pPager);/*�ڹ����б��в����д˶���ɾ��BtShared�������*/
    if( pBt->xFreeSchema && pBt->pSchema ){
      pBt->xFreeSchema(pBt->pSchema);
    }
    sqlite3DbFree(0, pBt->pSchema);
    freeTempSpace(pBt);//���˹��䡿��pBt�ͷ���ʱ�ռ�
    sqlite3_free(pBt);//���˹��䡿ɾ��BtShared�����е�pBt
  }

#ifndef SQLITE_OMIT_SHARED_CACHE
  assert( p->wantToLock==0 );
  assert( p->locked==0 );
  if( p->pPrev ) p->pPrev->pNext = p->pNext;//��ҳָ������ҳ
  if( p->pNext ) p->pNext->pPrev = p->pPrev;//��ҳָ������ҳ
#endif

  sqlite3_free(p); //��p�ͷ�
  return SQLITE_OK;
}

/*
** Change the limit on the number of pages allowed in the cache.
** �ı��ڻ����������ҳ������������
** The maximum number of cache pages is set to the absolute
** value of mxPage.  If mxPage is negative, the pager will
** operate asynchronously(��ͬʱ) - it will not stop to do fsync()s
** to insure data is written to the disk surface before
** continuing.  Transactions still work if synchronous is off,
** and the database cannot be corrupted if this program
** crashes.  But if the operating system crashes or there is
** an abrupt power failure when synchronous is off, the database
** could be left in an inconsistent and unrecoverable state.
** Synchronous is on by default so database corruption is not
** normally a worry.
** ����ҳ��������������ΪmxPage�ļ�ֵ�����mxPageΪ������pager�������첽����
** �ڼ���֮ǰ����������ֹͣ��fsync()��ȷ�����ݱ�д�����̡����ͬ���ǹرյ�������Ȼ�����ã�
** �������������������ݿ���ܲ����𻵡����ǣ��������ϵͳ������ͬ���ر�ʱ����ͻȻ�ϵ磬
** ���ݿ���ܻᴦ�ڲ�һ�µĺͲ��ɻָ���״̬��ͬ����Ĭ�ϵģ�������ݿ���ͨ�������˵��ǵġ�
*/
/*����ҳ�����С�Լ�ͬ��д�루�ڱ���ָʾsynchronous�ж��壩*/
/*���˹��䡿����ҳ�����С�Լ�ͬ��д�루�ڱ���ָʾsynchronous�ж��壩
**����ҳ��������������Ϊmxpage����ֵ�����mxpage�Ǹ��ģ���ҳ�����첽��������ͬʱ��-���᲻ͣ����fsync()ȷ�����ݱ�д�뵽���̱������ǰ��
���ͬ���ǹرյģ���������Ȼ����������ó����޷����𻵱��������ǣ��������ϵͳ��������ͻȻ�ϵ�ʱͬ��ʱ�����ݿ���ܴ��ڲ�һ�µĺͲ��ɻָ���״̬�뿪��
ͬ����Ĭ������£��������ݿ���ͨ���ǲ����ġ�
*/
int sqlite3BtreeSetCacheSize(Btree *p, int mxPage){
  BtShared *pBt = p->pBt;
  assert( sqlite3_mutex_held(p->db->mutex) );
  sqlite3BtreeEnter(p);
  sqlite3PagerSetCachesize(pBt->pPager, mxPage); /*����cache��ҳ������*/
  sqlite3BtreeLeave(p);
  return SQLITE_OK;
}

/*
** Change the way data is synced to disk in order to increase or decrease
** how well the database resists damage due to OS crashes and power
** failures.  Level 1 is the same as asynchronous (no syncs() occur and
** there is a high probability of damage)  Level 2 is the default.  There
** is a very low but non-zero probability of damage.  Level 3 reduces the
** probability of damage to near zero but with a write performance reduction.
*/
/*
sqlite3BtreeSetSafetyLevel ���ı�������ݵ�ͬ����ʽ�������ӻ�������ݿ��������ϵͳ�������Դ���ϵ��𺦵�������
1����ͬ���첽����syncs�������������ڽϸߵ��𺦷��գ�����ͬ�����ñ���ָʾsynchronous=OFF��
2����Ĭ�ϼ��𣬴��ڽϵ͵��𺦷��գ���ͬ�����ñ���ָʾsynchronous=NORMAL��
3���������𺦿����ԣ����սӽ�Ϊ0����������д���ܣ���ͬ�����ñ���ָʾsynchronous=FULL��

*/
#ifndef SQLITE_OMIT_PAGER_PRAGMAS
int sqlite3BtreeSetSafetyLevel(      //�ı�������ݵķ��ʷ�ʽ�������ӻ�������ݿ��������ϵͳ�������Դ���ϵ��𺦵�����
  Btree *p,              /* The btree to set the safety level on */         //btree���ð�ȫ����
  int level,             /* PRAGMA synchronous.  1=OFF, 2=NORMAL, 3=FULL */ //����ָʾͬ����1=OFF, 2=NORMAL, 3=FULL
  int fullSync,          /* PRAGMA fullfsync. */                            //����ָʾfullfsync
  int ckptFullSync       /* PRAGMA checkpoint_fullfync */                   //����ָʾcheckpoint_fullfync
){
  BtShared *pBt = p->pBt;
  assert( sqlite3_mutex_held(p->db->mutex) );
  assert( level>=1 && level<=3 );//���˹��䡿��ȫ����Ϊ1��2��3
  sqlite3BtreeEnter(p);//���˹��䡿���ý���B���ĺ���
  sqlite3PagerSetSafetyLevel(pBt->pPager, level, fullSync, ckptFullSync);//���˹��䡿����Pager���ð�ȫ����ĺ���
  sqlite3BtreeLeave(p);//���˹��䡿�����뿪B���ĺ���
  return SQLITE_OK;
}
#endif

/*
** Return TRUE if the given btree is set to safety level 1.  In other
** words, return TRUE if no sync() occurs on the disk files.
** ������B�����趨�İ�ȫ����Ϊ1����true����������ڴ�����û��sync()���ַ���true��
*/
/*
���˹��䡿���������B���İ�ȫ������1���򷵻�true�����仰˵�����û��sync()�����ڴ����ļ����򷵻�true��
*/
int sqlite3BtreeSyncDisabled(Btree *p){
  BtShared *pBt = p->pBt;
  int rc;
  assert( sqlite3_mutex_held(p->db->mutex) );  
  sqlite3BtreeEnter(p);
  assert( pBt && pBt->pPager );
  rc = sqlite3PagerNosync(pBt->pPager);
  sqlite3BtreeLeave(p);
  return rc;
}

/*
** Change the default pages size and the number of reserved bytes per page.
** Or, if the page size has already been fixed, return SQLITE_READONLY 
** without changing anything.
**
** The page size must be a power of 2 between 512 and 65536.  If the page
** size supplied does not meet this constraint then the page size is not
** changed.
**
** Page sizes are constrained to be a power of two so that the region
** of the database file used for locking (beginning at PENDING_BYTE,
** the first byte past the 1GB boundary, 0x40000000) needs to occur
** at the beginning of a page.
**
** If parameter nReserve is less than zero, then the number of reserved
** bytes per page is left unchanged.
**
** If the iFix!=0 then the BTS_PAGESIZE_FIXED flag is set so that the page size
** and autovacuum mode can no longer be changed.
*/
/*�������ݿ�ҳ��С*/
/*
���˹��䡿**����Ĭ�ϵ�ҳ��С�ͱ������ֽ����������ߣ������ҳ�Ĵ�С�Ѿ��̶���sqlite_readonly�����κθı䡣
**ҳ���С������2������512��65536֮�䡣���ҳ���С�������Լ������ҳ���Сû�иı䡣
**ҳ���С����Ϊ2���ݣ����������������ݿ��ļ�����pending_byte����һ���ֽڹ�ȥ1GB�ı߽磬0x40000000����Ҫ������һҳ�Ŀ�ͷ��
**�������nReserveС���㣬Ȼ��������ÿһҳ���ֽ������䡣
**���iFIX��= 0Ȼ������bts_pagesize_fixed��־��ҳ���С��autovacuumģʽ�����ܱ��ı䡣
*/
int sqlite3BtreeSetPageSize(Btree *p, int pageSize, int nReserve, int iFix){
  int rc = SQLITE_OK;
  BtShared *pBt = p->pBt;
  assert( nReserve>=-1 && nReserve<=255 );
  sqlite3BtreeEnter(p);
  if( pBt->btsFlags & BTS_PAGESIZE_FIXED ){
    sqlite3BtreeLeave(p);
    return SQLITE_READONLY;
  }
  if( nReserve<0 ){
    nReserve = pBt->pageSize - pBt->usableSize; /*����ҳ��С*/
  }
  assert( nReserve>=0 && nReserve<=255 );
  if( pageSize>=512 && pageSize<=SQLITE_MAX_PAGE_SIZE &&
        ((pageSize-1)&pageSize)==0 ){
    assert( (pageSize & 7)==0 );
    assert( !pBt->pPage1 && !pBt->pCursor );
    pBt->pageSize = (u32)pageSize;
    freeTempSpace(pBt);
  }
  rc = sqlite3PagerSetPagesize(pBt->pPager, &pBt->pageSize, nReserve);/*����ҳ�Ĵ�С*/
  pBt->usableSize = pBt->pageSize - (u16)nReserve;
  if( iFix ) pBt->btsFlags |= BTS_PAGESIZE_FIXED;/*iFix!=0������BTS_PAGESIZE_FIXED��־*/
  sqlite3BtreeLeave(p);
  return rc;
}

/*
** Return the currently defined page size
*/
/*
�������ݿ�ҳ�Ĵ�С
*/
int sqlite3BtreeGetPageSize(Btree *p){
  return p->pBt->pageSize;
}

#if !defined(SQLITE_OMIT_PAGER_PRAGMAS) || !defined(SQLITE_OMIT_VACUUM)
/*
** Return the number of bytes of space at the end of every page that
** are intentually left unused.  This is the "reserved" space that is
** sometimes used by extensions.
*/
/*
���˹��䡿���������ÿһҳ��δ��ʹ�õ��ֽ����Ŀռ䡣���ǡ��������Ŀռ䣬��ʱʹ����չ��
*/
int sqlite3BtreeGetReserve(Btree *p){
  int n;
  sqlite3BtreeEnter(p);
  n = p->pBt->pageSize - p->pBt->usableSize;/*ҳ��δ��ʹ�õ��ֽ���*/
  sqlite3BtreeLeave(p);
  return n;
}

/*
** Set the maximum page count for a database if mxPage is positive.
** No changes are made if mxPage is 0 or negative.
** Regardless of the value of mxPage, return the maximum page count.
** ���mxPage�����ģ��������ݿ�����ҳ�������mxPage��0���򲻸ı��С������mxPage��ֵ,�������ҳ����
*/
/*
���˹��䡿���mxpage���������������ҳ�������ݿ⡣���mxpage��0�����ˣ���û�б仯������mxpage��ֵ�����ص����ҳ����

*/
int sqlite3BtreeMaxPageCount(Btree *p, int mxPage){
  int n;
  sqlite3BtreeEnter(p);
  n = sqlite3PagerMaxPageCount(p->pBt->pPager, mxPage);/*mxPageΪ����pPager->mxPgno = mxPage;*/
  sqlite3BtreeLeave(p);
  return n;
}

/*
** Set the BTS_SECURE_DELETE flag if newFlag is 0 or 1.  If newFlag is -1,
** then make no changes.  Always return the value of the BTS_SECURE_DELETE
** setting after the change.
** ���newFlag��0��1������BTS_SECURE_DELETE��־�����newFlag��-1,�����á�һ���趨�����Ƿ���BTS_SECURE_DELETE��ֵ��
*/
/*
���˹��䡿���newflag��0��1��������BTS_SECURE_DELETE��־�����newFlag��-1����ô����Ҫ�ı䡣��֮����BTS_SECURE_DELETE���ø��ĺ��ֵ���С�
*/
int sqlite3BtreeSecureDelete(Btree *p, int newFlag){
  int b;
  if( p==0 ) return 0;
  sqlite3BtreeEnter(p);
  if( newFlag>=0 ){
    p->pBt->btsFlags &= ~BTS_SECURE_DELETE;
    if( newFlag ) p->pBt->btsFlags |= BTS_SECURE_DELETE;
  } 
  b = (p->pBt->btsFlags & BTS_SECURE_DELETE)!=0;
  sqlite3BtreeLeave(p);
  return b;
}
#endif /* !defined(SQLITE_OMIT_PAGER_PRAGMAS) || !defined(SQLITE_OMIT_VACUUM) */

/*
** Change the 'auto-vacuum' property of the database. If the 'autoVacuum'
** parameter is non-zero, then auto-vacuum mode is enabled. If zero, it
** is disabled. The default value for the auto-vacuum property is 
** determined by the SQLITE_DEFAULT_AUTOVACUUM macro.
** �������ݿ��Զ��������ҳ���ԡ������autoVacuum����������,��ôauto-vacuumģʽ���������Ϊ������á�
** auto-vacuum���Ե�Ĭ��ֵ�ɺ�SQLITE_DEFAULT_AUTOVACUUM���塣
*//*�������ݿ��Զ��������ҳ���ԡ�*/
/*
���˹��䡿**�������ݿ��Զ��������ҳ���ԡ�
**�ı����ݿ��'auto-vacuum'���ԡ������autovacuum������Ϊ���㣬��auto-vacuumģʽ�����á�
����㣬�������á�Ϊauto-vacuum���Ե�Ĭ��ֵ����SQLITE_DEFAULT_AUTOVACUUM ��۾�����
*/
int sqlite3BtreeSetAutoVacuum(Btree *p, int autoVacuum){
#ifdef SQLITE_OMIT_AUTOVACUUM
  return SQLITE_READONLY;
#else
  BtShared *pBt = p->pBt;
  int rc = SQLITE_OK;
  u8 av = (u8)autoVacuum;

  sqlite3BtreeEnter(p);
  if( (pBt->btsFlags & BTS_PAGESIZE_FIXED)!=0 && (av ?1:0)!=pBt->autoVacuum ){
    rc = SQLITE_READONLY;
  }else{
    pBt->autoVacuum = av ?1:0; /*av���Ϊ��0��auto-vacuumģʽ����*/
    pBt->incrVacuum = av==2 ?1:0;
  }
  sqlite3BtreeLeave(p);
  return rc;
#endif
}

/*
** Return the value of the 'auto-vacuum' property. If auto-vacuum is 
** enabled 1 is returned. Otherwise 0.
*//*��ȡ���ݿ��Ƿ��Զ�����ҳ��*/
/*
���˹��䡿
**����'auto-vacuum'���Ե�ֵ���������auto-vacuum���򷵻�1������0��
*/
int sqlite3BtreeGetAutoVacuum(Btree *p){//��ȡ���ݿ��Ƿ��Զ�����ҳ��
#ifdef SQLITE_OMIT_AUTOVACUUM
  return BTREE_AUTOVACUUM_NONE;
#else
  int rc;
  sqlite3BtreeEnter(p);
  rc = (
    (!p->pBt->autoVacuum)?BTREE_AUTOVACUUM_NONE:
    (!p->pBt->incrVacuum)?BTREE_AUTOVACUUM_FULL:
    BTREE_AUTOVACUUM_INCR
  );/*��autoVacuumΪ1����ȥ�հ�ҳ���ж�incrVacuum��ֵ����incrVacuum=1�� Incremental vacuum*/
  sqlite3BtreeLeave(p);
  return rc;
#endif
}

/*
** Get a reference to pPage1 of the database file.  This will
** also acquire a readlock on that file.
** �õ�һ���������ݿ��ļ�pPage1�Ĳο�����Ҳ���ڴ��ļ��ϻ�ö���
** SQLITE_OK is returned on success.  If the file is not a
** well-formed database file, then SQLITE_CORRUPT is returned.
** SQLITE_BUSY is returned if the database is locked.  SQLITE_NOMEM
** is returned if we run out of memory. 
** �ɹ��򷵻�SQLITE_OK������ļ�����һ����ʽ���õ����ݿ��ļ�,Ȼ�󷵻�SQLITE_CORRUPT��
** ������ݿⱻ��������SQLITE_BUSY������ڴ�ľ�����SQLITE_NOMEM.
*/
/*
���˹��䡿**�õ�һ���ο������ݿ��ļ�pPage1����Ҳ���Ը��ļ���ö�����
**SQLITE_OK �����سɹ����������ļ�����һ���ܺõ����ݿ��ļ���Ȼ��SQLITE_CORRUPT�����ء�
**������ݿⱻ��������SQLITE_BUSY �����ء��������ʹ���������ڴ棬��SQLITE_NOMEM�����ء�
*/
static int lockBtree(BtShared *pBt){
  int rc;              /* Result code from subfunctions */                     //���Ӻ������ؽ������
  MemPage *pPage1;     /* Page 1 of the database file */                       //���ݿ��ļ���ҳ1 /*���˹��䡿 ���ݿ��page 1 */
  int nPage;           /* Number of pages in the database */                   //���ݿ��е�ҳ����
  int nPageFile = 0;   /* Number of pages in the database file */              //���ݿ��ļ��е�ҳ����
  int nPageHeader;     /* Number of pages in the database according to hdr */  //��hdr�����ݿ��е�ҳ����

  assert( sqlite3_mutex_held(pBt->mutex) );
  assert( pBt->pPage1==0 );
  rc = sqlite3PagerSharedLock(pBt->pPager);
  if( rc!=SQLITE_OK ) return rc;
  rc = btreeGetPage(pBt, 1, &pPage1, 0);
  if( rc!=SQLITE_OK ) return rc;
  /* Do some checking to help insure the file we opened really is
  ** a valid database file. 
  ** ��һЩ���,��������ȷ���򿪵��ļ���һ����Ч�����ݿ��ļ���
  */
  nPage = nPageHeader = get4byte(28+(u8*)pPage1->aData);
  sqlite3PagerPagecount(pBt->pPager, &nPageFile);
  if( nPage==0 || memcmp(24+(u8*)pPage1->aData, 92+(u8*)pPage1->aData,4)!=0 ){
    nPage = nPageFile;
  }
  if( nPage>0 ){
    u32 pageSize;
    u32 usableSize;
    u8 *page1 = pPage1->aData;
    rc = SQLITE_NOTADB;
    if( memcmp(page1, zMagicHeader, 16)!=0 ){
      goto page1_init_failed;
    }

#ifdef SQLITE_OMIT_WAL
    if( page1[18]>1 ){
      pBt->btsFlags |= BTS_READ_ONLY;
    }
    if( page1[19]>1 ){
      goto page1_init_failed;
    }
#else
    if( page1[18]>2 ){
      pBt->btsFlags |= BTS_READ_ONLY;
    }
    if( page1[19]>2 ){
      goto page1_init_failed;
    }
    /* If the write version is set to 2, this database should be accessed
    ** in WAL mode. If the log is not already open, open it now. Then 
    ** return SQLITE_OK and return without populating BtShared.pPage1.
    ** The caller detects this and calls this function again. This is
    ** required as the version of page 1 currently in the page1 buffer
    ** may not be the latest version - there may be a newer one in the log
    ** file.
	** ���д�汾����Ϊ2,Ӧ����WALģʽ�·���������ݿ⡣�����־�����Ѿ���,������Ȼ�󷵻�SQLITE_OK���ҷ���û��ռ��BtShared.pPage1 
	** �����߼�⵽��һ�㲢�ٴε������������������Ҫ��1ҳ�İ汾Ŀǰ��page1�����������ܲ������°汾,���ܻ���һ���µ���־�ļ���
    */
	/*
	���˹��䡿���д�İ汾��2����Ӧ������Ԥд��־ϵͳģʽ�������ݿ⡣�����־��δ�򿪣�������
	Ȼ�󷵻�SQLITE_OK��û�����BtShared.pPage1����⣬�ٵ������������
	������Ҫ1ҳ�İ汾Ŀǰ�ڵ�һҳ���������ܲ������µİ汾���ܻ���һ���µ���־�ļ��С�
	*/
    if( page1[19]==2 && (pBt->btsFlags & BTS_NO_WAL)==0 ){
      int isOpen = 0;
      rc = sqlite3PagerOpenWal(pBt->pPager, &isOpen);/*����־*/
      if( rc!=SQLITE_OK ){
        goto page1_init_failed;
      }else if( isOpen==0 ){
        releasePage(pPage1);
        return SQLITE_OK;
      }
      rc = SQLITE_NOTADB;
    }
#endif
    /* The maximum embedded fraction must be exactly 25%.  And the minimum
    ** embedded fraction must be 12.5% for both leaf-data and non-leaf-data.
    ** The original design allowed these amounts to vary, but as of
    ** version 3.6.0, we require them to be fixed.
	** ����Ƕ�벿�ֱ�����25%������СǶ�벿�ְ���Ҷ�������non-leaf-data������12.5%����������������Щ������ͬ,������3.6.0�汾,����Ҫ�������ǹ̶��ġ�
    */
	/*
	���˹��䡿����Ҷ���ݺͷ�Ҷ���ݣ����Ƕ��ʽ���ֱ�����25%����СǶ�벿�ֱ���Ϊ12.5%��ԭ���������Щ����������ͬ������Ϊ�汾3.6.0������Ҫ�������ǹ̶��ġ�
	*/
    if( memcmp(&page1[21], "\100\040\040",3)!=0 ){
      goto page1_init_failed;
    }
    pageSize = (page1[16]<<8) | (page1[17]<<16);
    if( ((pageSize-1)&pageSize)!=0
     || pageSize>SQLITE_MAX_PAGE_SIZE 
     || pageSize<=256 
    ){
      goto page1_init_failed;
    }
    assert( (pageSize & 7)==0 );
    usableSize = pageSize - page1[20];
    if( (u32)pageSize!=pBt->pageSize ){
      /* After reading the first page of the database assuming a page size
      ** of BtShared.pageSize, we have discovered that the page-size is
      ** actually pageSize. Unlock the database, leave pBt->pPage1 at
      ** zero and return SQLITE_OK. The caller will call this function
      ** again with the correct page-size.
	  ** �����һҳ���ݿ�ļ���һ��BtShared.pageSize��ҳ���С�������Ѿ�����page-size��ʵ�ʵ�ҳ��С�������ݿ�,��pBt->pPage1����0��
	  ** ������SQLITE_OK�������߽�������ȷ��page-size�ٴε������������
      */
		/*
		���˹��䡿����һҳ�����ݿ����һ��BtShared.pageSizeҳ���С�����Ƿ��֣�ʵ������Ϊҳ���С�������ݿ⣬��pBt->pPage1Ϊ��ͷ���SQLITE_OK�����÷����ٴε����������������ȷ��ҳ���С��
		*/
      releasePage(pPage1);
      pBt->usableSize = usableSize;
      pBt->pageSize = pageSize;
      freeTempSpace(pBt);
      rc = sqlite3PagerSetPagesize(pBt->pPager, &pBt->pageSize,
                                   pageSize-usableSize);
      return rc;
    }
    if( (pBt->db->flags & SQLITE_RecoveryMode)==0 && nPage>nPageFile ){
      rc = SQLITE_CORRUPT_BKPT;
      goto page1_init_failed;
    }
    if( usableSize<480 ){
      goto page1_init_failed;
    }
    pBt->pageSize = pageSize;
    pBt->usableSize = usableSize;
#ifndef SQLITE_OMIT_AUTOVACUUM
    pBt->autoVacuum = (get4byte(&page1[36 + 4*4])?1:0);
    pBt->incrVacuum = (get4byte(&page1[36 + 7*4])?1:0);
#endif
  }
  /* maxLocal is the maximum amount of payload to store locally for
  ** a cell.  Make sure it is small enough so that at least minFanout
  ** cells can will fit on one page.  We assume a 10-byte page header.
  ** Besides the payload, the cell must store:
  **     2-byte pointer to the cell
  **     4-byte child pointer
  **     9-byte nKey value
  **     4-byte nData value
  **     4-byte overflow page pointer
  ** So a cell consists of a 2-byte pointer, a header which is as much as
  ** 17 bytes long, 0 to N bytes of payload, and an optional 4 byte overflow
  ** page pointer.
  ** maxLocal�Ǵ洢�ڱ���һ����Ԫ�ĸ��ص����������ȷ�������㹻С,��������minFanout��Ԫ���Թ̶���һ��ҳ���ϡ����Ǽ���һ��10-byteҳͷ��
  ** ���˸���,��Ԫ����洢:
  **     2�ֽڵ�ָ��Ԫ��ָ��
  **     4�ֽڵĺ���ָ��
  **     9���ֽڵ�nKeyֵ
  **     4�ֽڵ�nDataֵ
  **     4�ֽڵ����ҳָ��
  ** ���Ե�Ԫ��һ��2�ֽڵ�ָ��,һ��17�ֽڳ���ͷ��0��N���ֽڵ���Ч����,��һ����ѡ��4�ֽ����ҳ��ָ�롣
  */
   /*
 ���˹��䡿 maxlocal����Ч�غɵ����洢���ֲ���Ԫ��ȷ�������㹻С��
  ��������minfanout��Ԫ����Խ��ʺ���һ��ҳ�档���Ǽ���һ��10�ֽڵ�ҳͷ��
  ������Ч�غɣ������Ԫ�����洢��
  2���ֽ�ָ��ĵ�Ԫ��
  4���ֽڵ�ҳָ��
  9���ֽ�nKey��ֵ
  4���ֽ�nData��ֵ
  4���ֽ����ҳָ��
����һ����Ԫ��һ��2�ֽڵ�ָ�룬ͷ����һ����17�ֽڳ���0��N�ֽڵ���Ч�غɣ���һ����ѡ��4�ֽ����
ҳ��ָ�롣
  */
  pBt->maxLocal = (u16)((pBt->usableSize-12)*64/255 - 23);
  pBt->minLocal = (u16)((pBt->usableSize-12)*32/255 - 23);
  pBt->maxLeaf = (u16)(pBt->usableSize - 35);
  pBt->minLeaf = (u16)((pBt->usableSize-12)*32/255 - 23);
  if( pBt->maxLocal>127 ){
    pBt->max1bytePayload = 127;
  }else{
    pBt->max1bytePayload = (u8)pBt->maxLocal;
  }
  assert( pBt->maxLeaf + 23 <= MX_CELL_SIZE(pBt) );
  pBt->pPage1 = pPage1;
  pBt->nPage = nPage;
  return SQLITE_OK;

page1_init_failed:
  releasePage(pPage1);
  pBt->pPage1 = 0;
  return rc;
}

/*
** If there are no outstanding cursors and we are not in the middle
** of a transaction but there is a read lock on the database, then
** this routine unrefs the first page of the database file which 
** has the effect of releasing the read lock.
** ���û���������α겢�Ҳ����������ж��������ݿ�����һ����������ô������̲��������Ѿ��ͷŶ��������ݿ��ļ��ĵ�һҳ��
** If there is a transaction in progress, this routine is a no-op.
** ����ڽ�������һ������,������̽���һ���ղ�����
*/
/*
���˹��䡿**���û�кܺõ��α�����ǲ���һ�������е��ж��������ݿ⣬Ȼ���������unrefs���ݿ���ļ����ͷŶ���Ӱ��ĵ�һҳ��
**�����һ�������е��������������һ���ղ�����
*/
static void unlockBtreeIfUnused(BtShared *pBt){
  assert( sqlite3_mutex_held(pBt->mutex) );
  assert( pBt->pCursor==0 || pBt->inTransaction>TRANS_NONE );
  if( pBt->inTransaction==TRANS_NONE && pBt->pPage1!=0 ){	/*û������*/
    assert( pBt->pPage1->aData );
    assert( sqlite3PagerRefcount(pBt->pPager)==1 );
    assert( pBt->pPage1->aData );
    releasePage(pBt->pPage1);/*�ͷ��ڴ�*/
    pBt->pPage1 = 0;
  }
}

/*
** If pBt points to an empty file then convert that empty file
** into a new empty database by initializing the first page of
** the database.
** ���pBtָ��һ�����ļ�,��ôͨ����ʼ�����ݿ�ĵ�һҳ���Ϳ��ļ���һ���µĿ����ݿ⡣
*/
/*
���˹��䡿���pBtָ����ļ���Ȼ�󽫿��ļ���һ���µĿ����ݿ��ʼ�����ݿ�ĵ�һҳ��
*/
static int newDatabase(BtShared *pBt){
  MemPage *pP1;
  unsigned char *data;
  int rc;

  assert( sqlite3_mutex_held(pBt->mutex) );
  if( pBt->nPage>0 ){
    return SQLITE_OK;
  }
  pP1 = pBt->pPage1;
  assert( pP1!=0 );
  data = pP1->aData;
  rc = sqlite3PagerWrite(pP1->pDbPage);/*��ʼ�����ݿ��еĵ�һҳ�����Ϳ��ļ����յ����ݿ�*/
  if( rc ) return rc;
  memcpy(data, zMagicHeader, sizeof(zMagicHeader));
  assert( sizeof(zMagicHeader)==16 );
  data[16] = (u8)((pBt->pageSize>>8)&0xff);
  data[17] = (u8)((pBt->pageSize>>16)&0xff);
  data[18] = 1;
  data[19] = 1;
  assert( pBt->usableSize<=pBt->pageSize && pBt->usableSize+255>=pBt->pageSize);
  data[20] = (u8)(pBt->pageSize - pBt->usableSize);
  data[21] = 64;
  data[22] = 32;
  data[23] = 32;
  memset(&data[24], 0, 100-24);
  zeroPage(pP1, PTF_INTKEY|PTF_LEAF|PTF_LEAFDATA );
  pBt->btsFlags |= BTS_PAGESIZE_FIXED;
#ifndef SQLITE_OMIT_AUTOVACUUM
  assert( pBt->autoVacuum==1 || pBt->autoVacuum==0 );
  assert( pBt->incrVacuum==1 || pBt->incrVacuum==0 );
  put4byte(&data[36 + 4*4], pBt->autoVacuum);
  put4byte(&data[36 + 7*4], pBt->incrVacuum);
#endif
  pBt->nPage = 1;
  data[31] = 1;
  return SQLITE_OK;
}

/*
** Attempt to start a new transaction. A write-transaction
** is started if the second argument is nonzero, otherwise a read-
** transaction.  If the second argument is 2 or more and exclusive
** transaction is started, meaning that no other process is allowed
** to access the database.  A preexisting transaction may not be
** upgraded to exclusive by calling this routine a second time - the
** exclusivity flag only works for a new transaction.
** ���Կ�ʼһ������������ڶ��������Ƿ���0����ʼһ��д����,����ʼ����������ڶ���������2���2������ʼһ�����������,
** Ҳ����˵,�����������������������ݿ⡣һ����ǰ���ڵ�������ܲ���ͨ���ڶ��ε���������������������--�����־ֻ������һ��������
** A write-transaction must be started before attempting any 
** changes to the database.  None of the following routines 
** will work unless a transaction is started first:
** д����������޸����ݿ�֮ǰ��ʼ������ĺ���û��һ���������ó����������ȿ�ʼ:
**      sqlite3BtreeCreateTable()
**      sqlite3BtreeCreateIndex()
**      sqlite3BtreeClearTable()
**      sqlite3BtreeDropTable()
**      sqlite3BtreeInsert()
**      sqlite3BtreeDelete()
**      sqlite3BtreeUpdateMeta()
**
** If an initial attempt to acquire the lock fails because of lock contention
** and the database was previously unlocked, then invoke the busy handler
** if there is one.  But if there was previously a read-lock, do not
** invoke the busy handler - just return SQLITE_BUSY.  SQLITE_BUSY is 
** returned when there is already a read-lock in order to avoid a deadlock.
** ����״γ��Ի����ʧ������Ϊ�����������ݿ�֮ǰû����,Ȼ����÷�æ�Ĵ����������еĻ����������֮ǰ�ж���,
** �򲻵��á���ֻ�Ƿ���SQLITE_BUSY���Ѿ���һ�������Ա�������ʱ��SQLITE_BUSY�����ء�
** Suppose there are two processes A and B.  A has a read lock and B has
** a reserved lock.  B tries to promote to exclusive but is blocked because
** of A's read lock.  A tries to promote to reserved but is blocked by B.
** One or the other of the two processes must give way or there can be
** no progress.  By returning SQLITE_BUSY and not invoking the busy callback
** when A already has a read lock, we encourage A to give up and let B
** proceed.
** ��������������A��B��A��һ��������B��reserved����B��ͼ��û��⵫��ΪA�Ķ���������A��ͼ�ٽ���������B����
** һ�����������̱���������ķ�ʽ����û�н��̡���A�Ѿ��й�һ������ʱ����SQLITE_BUSY�����ǵ���æ,������A��������B���С�
*/
/*
���˹��䡿**���������µ����������һ������Ϊ���㣬��Ϊ���㣬��Ϊ������
�������������2�����������Ҷ�ռ��������������ζ��û���������̿��Է������ݿ⡣
һ���Ѿ����ڵ���������޷�ͨ������������̵ڶ��Ρ���������־ֻ������һ���µ���������Ϊ���ҡ�
**�ڳ��Ը������ݿ�֮ǰ�������ȿ�ʼд������û���������̽�����������һ�������ǿ�ʼ�ģ�
**      sqlite3BtreeCreateTable()
**      sqlite3BtreeCreateIndex()
**      sqlite3BtreeClearTable()
**      sqlite3BtreeDropTable()
**      sqlite3BtreeInsert()
**      sqlite3BtreeDelete()
**      sqlite3BtreeUpdateMeta()
**�������ĳ��Ի��������������ʧ�ܺ����ݿ�����ǰ�����ģ������һ�����ݿ⣬����������æ�Ĵ������
�����������ǰ�����������ô������ֻ����SQLITE_BUSY��SQLITE_BUSY����ʱ���Ѿ���һ���������Ա���������
**��������������A��B��A���ж�����B���б���������B��ͼ�ٽ���ռ����������Ϊһ��������A��ͼ�ƹ㵽��������B��ֹ��
һ������һ�����̱�����·����û�н�չ������SQLITE_BUSY�����ûص�ʱæ�Ѿ���һ�����������Ƿ���A��B���С�

*/
int sqlite3BtreeBeginTrans(Btree *p, int wrflag){   //wrflag���㿪ʼд���񣬷���ʼ������
  sqlite3 *pBlock = 0;
  BtShared *pBt = p->pBt;
  int rc = SQLITE_OK;

  sqlite3BtreeEnter(p);
  btreeIntegrity(p);

  /* If the btree is already in a write-transaction, or it
  /* If the btree is already in a write-transaction, or it
  ** is already in a read-transaction and a read-transaction
  ** is requested, this is a no-op.
  ** ���btree�Ѿ���д������,���������ڶ������в��Ҷ���������,��ô����һ���ղ�����
  */
  /*
���B���Ѿ���д���񣬻����Ѿ��ڶ�����������ȡ��������һ���ղ�����
*/
  if( p->inTrans==TRANS_WRITE || (p->inTrans==TRANS_READ && !wrflag) ){
    goto trans_begun;
  }

  /* Write transactions are not possible on a read-only database */ //д���񲻿�����һ��ֻ�������ݿ��� /*���˹��䡿��ֻ�����ݿ���д�������ǲ����ܵ�*/
  if( (pBt->btsFlags & BTS_READ_ONLY)!=0 && wrflag ){
    rc = SQLITE_READONLY;
    goto trans_begun;
  }

#ifndef SQLITE_OMIT_SHARED_CACHE
  /* If another database handle has already opened a write transaction 
  ** on this shared-btree structure and a second write transaction is
  ** requested, return SQLITE_LOCKED.
  ** �����һ�����ݿ⴦������Ѿ�����shared-btree�ṹ������д����������ڶ���д����,�򷵻�SQLITE_LOCKED��
  */
  /*
  ���˹��䡿���һ�����ݿ����ѿ�ͨд�������⹲���B���ṹ������ڶ���д�����򷵻�SQLITE_LOCKED��
  */
  if( (wrflag && pBt->inTransaction==TRANS_WRITE)
   || (pBt->btsFlags & BTS_PENDING)!=0
  ){
    pBlock = pBt->pWriter->db;/*����ͬʱ������д���񣬷���SQLITE_LOCKED*/
  }else if( wrflag>1 ){
    BtLock *pIter;
    for(pIter=pBt->pLock; pIter; pIter=pIter->pNext){
      if( pIter->pBtree!=p ){
        pBlock = pIter->pBtree->db;
        break;
      }
    }
  }
  if( pBlock ){
    sqlite3ConnectionBlocked(p->db, pBlock);
    rc = SQLITE_LOCKED_SHAREDCACHE;
    goto trans_begun;
  }
#endif

  /* Any read-only or read-write transaction implies a read-lock on 
  ** page 1. So if some other shared-cache client already has a write-lock 
  ** on page 1, the transaction cannot be opened. 
  ** �κ�ֻ�����д������ζ����ҳ1���ж������������������ͻ�����ҳ1���Ѿ���һ��д��,��ô�����ܱ�������*/
  /*
 ���˹��䡿 �κ�ֻ�����д������ζ�Ŷ���ҳΪ1����ˣ����һЩ����������ͻ����Ѿ���һ��д��ҳΪ1����������ܱ��򿪡�
  */
  rc = querySharedCacheTableLock(p, MASTER_ROOT, READ_LOCK);
  if( SQLITE_OK!=rc ) goto trans_begun;

  pBt->btsFlags &= ~BTS_INITIALLY_EMPTY;
  if( pBt->nPage==0 ) pBt->btsFlags |= BTS_INITIALLY_EMPTY;
  do {
    /* Call lockBtree() until either pBt->pPage1 is populated or
    ** lockBtree() returns something other than SQLITE_OK. lockBtree()
    ** may return SQLITE_OK but leave pBt->pPage1 set to 0 if after
    ** reading page 1 it discovers that the page-size of the database 
    ** file is not pBt->pageSize. In this case lockBtree() will update
    ** pBt->pageSize to the page-size of the file on disk.
	** ����lockBtree(),ֱ��pBt->pPage1����ֵ����lockBtree()����SQLITE_OK�������Ϣ��
	** lockBtree()���ܷ���SQLITE_OK����pBt->pPage1Ϊ0 ���������1ҳ�������ݿ��ļ�ҳ���С����pBt->pageSize��
	** �����������lockBtree()������pBt->pageSize�Ĵ�СΪ�������ļ���ҳ��С��
    */
	  /*
	 ���˹��䡿 ����lockBtree()����ֱ��pBt->pPage1��������lockBtree()��������SQLITE_OK��������ݡ�
	  lockbtree()�������ܷ���SQLITE_OK����pBt->pPage1����Ϊ0�������ҳΪ1����ҳ���С�� ���ݿ��ļ�����pBt->pageSize��
	  �����������lockbtree()������pBt->pageSize��ҳ���ļ���С�Ĵ����ϡ�
	  */
    while( pBt->pPage1==0 && SQLITE_OK==(rc = lockBtree(pBt)) );

    if( rc==SQLITE_OK && wrflag ){
      if( (pBt->btsFlags & BTS_READ_ONLY)!=0 ){
        rc = SQLITE_READONLY;
      }else{
        rc = sqlite3PagerBegin(pBt->pPager,wrflag>1,sqlite3TempInMemory(p->db));
        if( rc==SQLITE_OK ){
          rc = newDatabase(pBt);
        }
      }
    }
  
    if( rc!=SQLITE_OK ){
      unlockBtreeIfUnused(pBt);
    }
  }while( (rc&0xFF)==SQLITE_BUSY && pBt->inTransaction==TRANS_NONE &&
          btreeInvokeBusyHandler(pBt) );

  if( rc==SQLITE_OK ){
    if( p->inTrans==TRANS_NONE ){
      pBt->nTransaction++;
#ifndef SQLITE_OMIT_SHARED_CACHE
      if( p->sharable ){
        assert( p->lock.pBtree==p && p->lock.iTable==1 );
        p->lock.eLock = READ_LOCK;
        p->lock.pNext = pBt->pLock;
        pBt->pLock = &p->lock;
      }
#endif
    }
    p->inTrans = (wrflag?TRANS_WRITE:TRANS_READ);/*Ϊ1��д�����������*/
    if( p->inTrans>pBt->inTransaction ){
      pBt->inTransaction = p->inTrans;
    }
    if( wrflag ){
      MemPage *pPage1 = pBt->pPage1;
#ifndef SQLITE_OMIT_SHARED_CACHE
      assert( !pBt->pWriter );
      pBt->pWriter = p;
      pBt->btsFlags &= ~BTS_EXCLUSIVE;
      if( wrflag>1 ) pBt->btsFlags |= BTS_EXCLUSIVE;
#endif

      /* If the db-size header field is incorrect (as it may be if an old
      ** client has been writing the database file), update it now. Doing
      ** this sooner rather than later means the database size can safely 
      ** re-read the database size from page 1 if a savepoint or transaction
      ** rollback occurs within the transaction.
	  ** ���db-sizeͷ�ֶβ���ȷ(���һ���ɿͻ���һֱ��д���ݿ��ļ���������������ܷ���),���������¡�
	  ** �������粻�˳٣���Ϊ���һ�������������������з����ع������ݿ��С���Դӵ�1ҳ��ȫ���ض���
      */
	  /*
	 ���˹��䡿 ������ݿ��С��ͷ���Ǵ����(��Ϊ�������һ���ɿͻ�д���ݿ��ļ�),�����ϸ��¡�
	  ������������ζ�ţ����һ�������ع������������������ݿ��С���԰�ȫ�����¶�ȡ���ݿ��СҳΪ1��
	  */
      if( pBt->nPage!=get4byte(&pPage1->aData[28]) ){
        rc = sqlite3PagerWrite(pPage1->pDbPage);/*����db-size��ͷ�ֶ�*/
        if( rc==SQLITE_OK ){
          put4byte(&pPage1->aData[28], pBt->nPage);
        }
      }
    }
  }


trans_begun:
  if( rc==SQLITE_OK && wrflag ){
    /* This call makes sure that the pager has the correct number of
    ** open savepoints. If the second parameter is greater than 0 and
    ** the sub-journal is not already open, then it will be opened here.
	** �������ȷ��pager����ȷ�Ŀ����Ա������Ŀ������ڶ�����������0����sub-journalû�д�,��ô�������򿪡�
    */
	  /*
	  ���˹��䡿������ñ�֤ҳ���������п��ŵı������ȷ�����������������������0��sub-journal�����Ѿ��򿪣���ô����������򿪡�
	  */
    rc = sqlite3PagerOpenSavepoint(pBt->pPager, p->db->nSavepoint);/*wrflag>0,�򿪱����*/
  }

  btreeIntegrity(p);
  sqlite3BtreeLeave(p);
  return rc;
}

#ifndef SQLITE_OMIT_AUTOVACUUM
/*
** Set the pointer-map entries for all children of page pPage. Also, if
** pPage contains cells that point to overflow pages, set the pointer
** map entries for the overflow pages as well.
** ��ҳpPage�����к��ӽڵ�����ָ��ӳ����Ŀ�����pPage����ָ�����ҳ��ָ��ĵ�Ԫ��Ҳ�����ҳ����ָ��ӳ����Ŀ��
*/
/*
���˹��䡿����ָ��λͼΪpPage���к���ҳ��ͬʱ�����pPage����ָ�����ҳ�ĵ�Ԫ���������ҳ��ָ��λͼ��
*/
static int setChildPtrmaps(MemPage *pPage){
  int i;                             /* Counter variable */    //����������
  int nCell;                         /* Number of cells in page pPage */  //��ҳpPage�еĵ�Ԫ������  /* ���˹��䡿��ҳ�ĵ�Ԫ��*/
  int rc;                            /* Return code */    //����ֵ����
  BtShared *pBt = pPage->pBt;
  u8 isInitOrig = pPage->isInit;
  Pgno pgno = pPage->pgno;

  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  rc = btreeInitPage(pPage);
  if( rc!=SQLITE_OK ){
    goto set_child_ptrmaps_out;
  }
  nCell = pPage->nCell;

  for(i=0; i<nCell; i++){
    u8 *pCell = findCell(pPage, i);

    ptrmapPutOvflPtr(pPage, pCell, &rc);

    if( !pPage->leaf ){
      Pgno childPgno = get4byte(pCell);
      ptrmapPut(pBt, childPgno, PTRMAP_BTREE, pgno, &rc);
    }
  }

  if( !pPage->leaf ){
    Pgno childPgno = get4byte(&pPage->aData[pPage->hdrOffset+8]);
    ptrmapPut(pBt, childPgno, PTRMAP_BTREE, pgno, &rc);
  }

set_child_ptrmaps_out:
  pPage->isInit = isInitOrig;
  return rc;
}

/*
** Somewhere on pPage is a pointer to page iFrom.  Modify this pointer so
** that it points to iTo. Parameter eType describes the type of pointer to
** be modified, as  follows:
** ҳiFrom��һ��ָ�룬ָ��ҳ���ϵ�ĳ���ط����޸����ָ��ʹ��ָ��iTo������eType�������޸�ָ�������,������ʾ:
** PTRMAP_BTREE:     pPage is a btree-page. The pointer points at a child 
**                   page of pPage.
**                   ָ��ָ��pPage��һ������ҳ�档
** PTRMAP_OVERFLOW1: pPage is a btree-page. The pointer points at an overflow
**                   page pointed to by one of the cells on pPage.
**                   ָ��ָ��һ�����ҳ�棬��pPage�ϵĵ�Ԫ���е�һ��ָ������ҳ��
** PTRMAP_OVERFLOW2: pPage is an overflow-page. The pointer points at the next
**                   overflow page in the list.
*/                   //ָ��ָ���б��е���һ�����ҳ��
/*
���˹��䡿**��pPageָ��iFromҳ���޸ĸ�ָ��ʹ��ָ��ITO������eType����Ҫ�޸�ָ������ͣ����£�
** PTRMAP_BTREE: pPage��b��ҳ�����ָ��ָ����ҳ��pPage��
** PTRMAP_OVERFLOW1:pPage��b��ҳ��ָ��ָ��һ�����ҳָ����һ����pPage�ĵ�Ԫ��
** PTRMAP_OVERFLOW2:pPage�����ҳ����ָ��ָ���б��е���һ�����ҳ��
*/
static int modifyPagePointer(MemPage *pPage, Pgno iFrom, Pgno iTo, u8 eType){
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  assert( sqlite3PagerIswriteable(pPage->pDbPage) );
  if( eType==PTRMAP_OVERFLOW2 ){
    /* The pointer is always the first 4 bytes of the page in this case.*/  //ָ�����ǵ�һ��ҳ���4���ֽڡ�
    if( get4byte(pPage->aData)!=iFrom ){
      return SQLITE_CORRUPT_BKPT;
    }
    put4byte(pPage->aData, iTo);
  }else{
    u8 isInitOrig = pPage->isInit;
    int i;
    int nCell;

    btreeInitPage(pPage);
    nCell = pPage->nCell;

    for(i=0; i<nCell; i++){
      u8 *pCell = findCell(pPage, i);
      if( eType==PTRMAP_OVERFLOW1 ){
        CellInfo info;
        btreeParseCellPtr(pPage, pCell, &info);
        if( info.iOverflow
         && pCell+info.iOverflow+3<=pPage->aData+pPage->maskPage
         && iFrom==get4byte(&pCell[info.iOverflow])
        ){
          put4byte(&pCell[info.iOverflow], iTo);
          break;
        }
      }else{
        if( get4byte(pCell)==iFrom ){
          put4byte(pCell, iTo);
          break;
        }
      }
    }
  
    if( i==nCell ){
      if( eType!=PTRMAP_BTREE || 
          get4byte(&pPage->aData[pPage->hdrOffset+8])!=iFrom ){
        return SQLITE_CORRUPT_BKPT;
      }
      put4byte(&pPage->aData[pPage->hdrOffset+8], iTo);
    }

    pPage->isInit = isInitOrig;
  }
  return SQLITE_OK;
}
/*
** Move the open database page pDbPage to location iFreePage in the 
** database. The pDbPage reference remains valid.
** �ƶ��������ݿ�ҳpDbPage�����ݿ��е�Ҫ���λ��iFreePage��pDbPage�����Կ��á�
** The isCommit flag indicates that there is no need to remember that
** the journal needs to be sync()ed before database page pDbPage->pgno 
** can be written to. The caller has already promised not to write to that
** page.
** isCommit��־��ʾ�����ݿ�ҳpDbPage->pgno���ܱ�д֮ǰ��־��Ҫsync()ͬ��û�б�Ҫ��¼�������߲�ȥд�Ǹ�ҳ��
*/
/*
���˹��䡿**���򿪵����ݿ�ҳpDbPage���ݿ��е�λ��iFreePage��pDbPage��Ȼ��Ч�Ĳο���
**isCommit��־��ʾ����Ҫ��ס��־����Ҫsync() ED�����ݿ�ҳ��pDbPage->pgno 
����д���������Ѿ���Ӧ��д��ҳ�档
*/
static int relocatePage(
  BtShared *pBt,           /* Btree */               //B��
  MemPage *pDbPage,        /* Open page to move */   //Ҫ�ƶ��Ŀ�����ҳ
  u8 eType,                /* Pointer map 'type' entry for pDbPage */    //pDbPageָ��ӳ��������Ŀ /*���˹��䡿pDbPageָ��λͼ'type'*/
  Pgno iPtrPage,           /* Pointer map 'page-no' entry for pDbPage */ //pDbPageָ��ӳ��'page-no'��Ŀ//���˹��䡿pDbPageָ��λͼ'page-no'
  Pgno iFreePage,          /* The location to move pDbPage to */         //�ƶ�pDbPage����λ��
  int isCommit             /* isCommit flag passed to sqlite3PagerMovepage */  //���ݸ�sqlite3PagerMovepage��isCommit��־
){
  MemPage *pPtrPage;   /* The page that contains a pointer to pDbPage */   //������pDbPage��ҳ
  Pgno iDbPage = pDbPage->pgno;
  Pager *pPager = pBt->pPager;
  int rc;

  assert( eType==PTRMAP_OVERFLOW2 || eType==PTRMAP_OVERFLOW1 || 
      eType==PTRMAP_BTREE || eType==PTRMAP_ROOTPAGE );
  assert( sqlite3_mutex_held(pBt->mutex) );
  assert( pDbPage->pBt==pBt );

  /* Move page iDbPage from its current location to page number iFreePage */ //�ӵ�ǰλ���ƶ�ҳ��iDbPage��ҳ��iFreePage
  TRACE(("AUTOVACUUM: Moving %d to free page %d (ptr page %d type %d)\n", 
      iDbPage, iFreePage, iPtrPage, eType));//���˹��䡿iDbPage�ӵ�ǰλ���ƶ���iFreePageҳ����
  rc = sqlite3PagerMovepage(pPager, pDbPage->pDbPage, iFreePage, isCommit);
  if( rc!=SQLITE_OK ){
    return rc;
  }
  pDbPage->pgno = iFreePage;

  /* If pDbPage was a btree-page, then it may have child pages and/or cells
  ** that point to overflow pages. The pointer map entries for all these
  ** pages need to be changed.
  ** ���pDbPage��btree-page,��ô�������к���ҳ��ָ�����ҳ�ĵ�Ԫ��������Щָ��ӳ����Ŀ����Ҫ���ġ�
  ** If pDbPage is an overflow page, then the first 4 bytes may store a
  ** pointer to a subsequent overflow page. If this is the case, then
  ** the pointer map needs to be updated for the subsequent overflow page.
  ** ���pDbPage��һ�����ҳ,��ô��һ��4�ֽڴ洢һ��ָ��������ҳ��ָ�롣������������,��ôָ��ӳ����Ҫ�������ҳ����¡�
  */
  /*
  ���˹��䡿**���pDbPage��B��ҳ����ô����������ҳ���/��Ԫ��ָ�����ҳ��������Щҳ��ָ��λͼ��Ҫ���ġ�
  **���pDbPage�����ҳ��Ȼ���һ��4�ֽڿ��Դ洢һ��ָ��������ҳ���������������������ָ��λͼ��ҪΪ�������ҳ���и��¡�
  */
  if( eType==PTRMAP_BTREE || eType==PTRMAP_ROOTPAGE ){
    rc = setChildPtrmaps(pDbPage);
    if( rc!=SQLITE_OK ){
      return rc;
    }
  }else{
    Pgno nextOvfl = get4byte(pDbPage->aData);
    if( nextOvfl!=0 ){
      ptrmapPut(pBt, nextOvfl, PTRMAP_OVERFLOW2, iFreePage, &rc);
      if( rc!=SQLITE_OK ){
        return rc;
      }
    }
  }

  /* Fix the database pointer on page iPtrPage that pointed at iDbPage so
  ** that it points at iFreePage. Also fix the pointer map entry for
  ** iPtrPage.
  ** �̶����ݿ�ָ�뵽ҳiPtrPage�ϣ���ҳָ��iDbPage,�Ա���ָ��iFreePage��ͬʱ����iPtrPage���̶�ָ��ӳ����Ŀ��
  */
   /*
 ���˹��䡿 �޸����ݿ�ָ��iPtrPageҳָ��iDbPageʹ��ָ��iFreePage�����һ����޸�iPtrPageָ��λͼ��
  */
  if( eType!=PTRMAP_ROOTPAGE ){
    rc = btreeGetPage(pBt, iPtrPage, &pPtrPage, 0);
    if( rc!=SQLITE_OK ){
      return rc;
    }
    rc = sqlite3PagerWrite(pPtrPage->pDbPage);
    if( rc!=SQLITE_OK ){
      releasePage(pPtrPage);
      return rc;
    }
    rc = modifyPagePointer(pPtrPage, iDbPage, iFreePage, eType);
    releasePage(pPtrPage);
    if( rc==SQLITE_OK ){
      ptrmapPut(pBt, iFreePage, eType, iPtrPage, &rc);
    }
  }
  return rc;
}

/* Forward declaration required by incrVacuumStep(). */   //Ҫ��ͨ��incrVacuumStep()��ǰ����
static int allocateBtreePage(BtShared *, MemPage **, Pgno *, Pgno, u8);

/*
** Perform a single step of an incremental-vacuum. If successful,
** return SQLITE_OK. If there is no work to do (and therefore no
** point in calling this function again), return SQLITE_DONE.
** ִ��һ��������incremental-vacuum���衣����ɹ�,����SQLITE_OK�����û�гɹ�(��û���ٵ����������),����SQLITE_DONE��
** More specificly, this function attempts to re-organize the 
** database so that the last page of the file currently in use
** is no longer in use.
** �������,���������ͼ�������ݿ�,��ʹ��ǰʹ�õ��ļ������һҳ�Ѳ���ʹ�á�
** If the nFin parameter is non-zero, this function assumes
** that the caller will keep calling incrVacuumStep() until
** it returns SQLITE_DONE or an error, and that nFin is the
** number of pages the database file will contain after this 
** process is complete.  If nFin is zero, it is assumed that
** incrVacuumStep() will be called a finite amount of times
** which may or may not empty the freelist.  A full autovacuum
** has nFin>0.  A "PRAGMA incremental_vacuum" has nFin==0.
** ���nFin������Ϊ��,���������������߽����ֵ���incrVacuumStep()ֱ������SQLITE_DONE�����nFin�����ݿ��ļ���ҳ������
** �ڽ������֮�󽫱����������nFin����,���ٶ�incrVacuumStep()�����������޴Σ�freelist���ܻ����ܲ���ա�
** һ��������autovacuum��nFin>0��һ��"PRAGMA incremental_vacuum"��nFin==0��
*/
/*
���˹��䡿**����ִ��һ��������incremental-vacuum������ɹ�������SQLITE_OK�����û�й���Ҫ��������ٴε����������û�е㣩������SQLITE_DONE��
**�������˵�����������ͼ������֯���ݿ⣬���һҳ���ļ�����ʹ�ò���ʹ�á�
**���nFin������Ϊ�㣬��������ٶ������߻�һֱ����incrVacuumStep()ֱ������SQLITE_DONE����󣬲���nFin����ҳ���ݿ��ļ�����������������е������������ġ����nFin���㣬���Ǽٶ�incrVacuumStep()
������Ϊһ�����޵�ʱ�䣬���ܻ����ܲ���������б�
*/
static int incrVacuumStep(BtShared *pBt, Pgno nFin, Pgno iLastPg){        //ִ��һ��������incremental-vacuum���衣
  Pgno nFreeList;           /* Number of pages still on the free-list */  //���ڿ����б��ҳ����
  int rc;

  assert( sqlite3_mutex_held(pBt->mutex) );
  assert( iLastPg>nFin );

  if( !PTRMAP_ISPAGE(pBt, iLastPg) && iLastPg!=PENDING_BYTE_PAGE(pBt) ){
    u8 eType;
    Pgno iPtrPage;

    nFreeList = get4byte(&pBt->pPage1->aData[36]);
    if( nFreeList==0 ){
      return SQLITE_DONE;
    }

    rc = ptrmapGet(pBt, iLastPg, &eType, &iPtrPage);
    if( rc!=SQLITE_OK ){
      return rc;
    }
    if( eType==PTRMAP_ROOTPAGE ){
      return SQLITE_CORRUPT_BKPT;
    }

    if( eType==PTRMAP_FREEPAGE ){
      if( nFin==0 ){
        /* Remove the page from the files free-list. This is not required
        ** if nFin is non-zero. In that case, the free-list will be
        ** truncated to zero after this function returns, so it doesn't 
        ** matter if it still contains some garbage entries.
		** ɾ���ļ������б��ҳ�档���nFin�Ƿ�������ⲻ�Ǳ���ġ������������,�����б�������������غ�ض�Ϊ��,
		** �����������������һЩ������ĿҲû�����⡣
        */
		   /*
		  �ӡ��ļ��������б���ɾ����ҳ�����nFin�Ƿ��㡣����������£��˺������غ󣬿����б����ض�Ϊ�㣬
		  ��ˣ��������Ȼ����һЩû���õ���Ϣ����û�й�ϵ�ġ�
		  */
        Pgno iFreePg;
        MemPage *pFreePg;
        rc = allocateBtreePage(pBt, &pFreePg, &iFreePg, iLastPg, 1);
        if( rc!=SQLITE_OK ){
          return rc;//����allocateBtreePage()����
        }
        assert( iFreePg==iLastPg );
        releasePage(pFreePg);//�����ͷ�ҳ�ĺ���
      }
    } else {
      Pgno iFreePg;             /* Index of free page to move pLastPg to */  //�ƶ�pLastPg��Ҫ���Ŀ���ҳ������
      MemPage *pLastPg;

      rc = btreeGetPage(pBt, iLastPg, &pLastPg, 0);
      if( rc!=SQLITE_OK ){
        return rc;//����btreeGetPage()����
      }
/*�������˹�������*/ 
      /* If nFin is zero, this loop runs exactly once and page pLastPg
      ** is swapped with the first free page pulled off the free list.
      ** ���nFin����,��ѭ����������һ�κ�ҳ��pLastPg�����ڿ����б�ҳ�еĵ�һ������ҳ������
      ** On the other hand, if nFin is greater than zero, then keep
      ** looping until a free-page located within the first nFin pages
      ** of the file is found.
	  ** ��һ����,���nFin������,Ȼ�����ѭ��,ֱ������ҳλ���ļ��ĵ�һ��nFinҳ�汻���֡�
      */
      do {
        MemPage *pFreePg;
        rc = allocateBtreePage(pBt, &pFreePg, &iFreePg, 0, 0);
        if( rc!=SQLITE_OK ){
          releasePage(pLastPg);
          return rc;
        }
        releasePage(pFreePg);
      }while( nFin!=0 && iFreePg>nFin );
      assert( iFreePg<iLastPg );
      
      rc = sqlite3PagerWrite(pLastPg->pDbPage);
      if( rc==SQLITE_OK ){
        rc = relocatePage(pBt, pLastPg, eType, iPtrPage, iFreePg, nFin!=0);
      }
      releasePage(pLastPg);
      if( rc!=SQLITE_OK ){
        return rc;
      }
    }
  }

  if( nFin==0 ){
    iLastPg--;
    while( iLastPg==PENDING_BYTE_PAGE(pBt)||PTRMAP_ISPAGE(pBt, iLastPg) ){
      if( PTRMAP_ISPAGE(pBt, iLastPg) ){
        MemPage *pPg;
        rc = btreeGetPage(pBt, iLastPg, &pPg, 0);
        if( rc!=SQLITE_OK ){
          return rc;
        }
        rc = sqlite3PagerWrite(pPg->pDbPage);
        releasePage(pPg);
        if( rc!=SQLITE_OK ){
          return rc;
        }
      }
      iLastPg--;
    }
    sqlite3PagerTruncateImage(pBt->pPager, iLastPg);
    pBt->nPage = iLastPg;
  }
  return SQLITE_OK;
}

/*
** A write-transaction must be opened before calling this function.
** It performs a single unit of work towards an incremental vacuum.
** �����������֮ǰ��д�������򿪡���ִ�е���������Ԫ��incremental vacuum��
** If the incremental vacuum is finished after this function has run,
** SQLITE_DONE is returned. If it is not finished, but no error occurred,
** SQLITE_OK is returned. Otherwise an SQLite error code. 
** ���incremental vacuum������������н��������,����SQLITE_DONE�����û�����,����û�д�����,����SQLITE_OK��
** ���򷵻�һ��SQLite������롣
*/
/*
�����������֮ǰ��д�������򿪡�
*/
int sqlite3BtreeIncrVacuum(Btree *p){
  int rc;
  BtShared *pBt = p->pBt;

  sqlite3BtreeEnter(p);
  assert( pBt->inTransaction==TRANS_WRITE && p->inTrans==TRANS_WRITE );
  if( !pBt->autoVacuum ){
    rc = SQLITE_DONE;
  }else{
    invalidateAllOverflowCache(pBt);
    rc = incrVacuumStep(pBt, 0, btreePagecount(pBt));
    if( rc==SQLITE_OK ){
      rc = sqlite3PagerWrite(pBt->pPage1->pDbPage);
      put4byte(&pBt->pPage1->aData[28], pBt->nPage);
    }
  }
  sqlite3BtreeLeave(p);
  return rc;
}

/*
** This routine is called prior to sqlite3PagerCommit when a transaction
** is commited for an auto-vacuum database.
** ����һ��auto-vacuum���ݿ⣬��һ�������ύ֮�������������sqlite3PagerCommit֮ǰ�����á�
** If SQLITE_OK is returned, then *pnTrunc is set to the number of pages
** the database file should be truncated to during the commit process. 
** i.e. the database has been reorganized so that only the first *pnTrunc
** pages are in use.
** �������SQLITE_OK,��ô*pnTrunc����ҳ������������ݿ��ļ����ύ������Ӧ�ñ��ضϡ�
** �����ݿ��Ѿ�����,�Ա�ֻ�е�һ��*pnTruncҳ�����á�
*/
static int autoVacuumCommit(BtShared *pBt){
  int rc = SQLITE_OK;
  Pager *pPager = pBt->pPager;
  VVA_ONLY( int nRef = sqlite3PagerRefcount(pPager) );

  assert( sqlite3_mutex_held(pBt->mutex) );
  invalidateAllOverflowCache(pBt);
  assert(pBt->autoVacuum);
  if( !pBt->incrVacuum ){
    Pgno nFin;         /* Number of pages in database after autovacuuming */  //���߶���������ݿ��е�ҳ����
    Pgno nFree;        /* Number of pages on the freelist initially */        //�����б��������ҳ����
    Pgno nPtrmap;      /* Number of PtrMap pages to be freed */               //���ͷŵ�PtrMapҳ������
    Pgno iFree;        /* The next page to be freed */                        //���ͷŵ���һ��ҳ��
    int nEntry;        /* Number of entries on one ptrmap page */             //��һ��ptrmapҳ�ϵ���Ŀ��
    Pgno nOrig;        /* Database size before freeing */                     //�ͷ�ǰ�����ݿ��С

    nOrig = btreePagecount(pBt);
    if( PTRMAP_ISPAGE(pBt, nOrig) || nOrig==PENDING_BYTE_PAGE(pBt) ){
      /* It is not possible to create a database for which the final page
      ** is either a pointer-map page or the pending-byte page. If one
      ** is encountered, this indicates corruption.
	  ** �������һҳ��һ��ָ��ӳ��ҳ������pending-byte���͵�ҳ������һ�����ݿ��ǲ����ܵġ����������,������ǲ����ġ�
      */
      return SQLITE_CORRUPT_BKPT;
    }

    nFree = get4byte(&pBt->pPage1->aData[36]);
    nEntry = pBt->usableSize/5;
    nPtrmap = (nFree-nOrig+PTRMAP_PAGENO(pBt, nOrig)+nEntry)/nEntry;
    nFin = nOrig - nFree - nPtrmap;
    if( nOrig>PENDING_BYTE_PAGE(pBt) && nFin<PENDING_BYTE_PAGE(pBt) ){
      nFin--;
    }
    while( PTRMAP_ISPAGE(pBt, nFin) || nFin==PENDING_BYTE_PAGE(pBt) ){
      nFin--;
    }
    if( nFin>nOrig ) return SQLITE_CORRUPT_BKPT;

    for(iFree=nOrig; iFree>nFin && rc==SQLITE_OK; iFree--){
      rc = incrVacuumStep(pBt, nFin, iFree);
    }
    if( (rc==SQLITE_DONE || rc==SQLITE_OK) && nFree>0 ){
      rc = sqlite3PagerWrite(pBt->pPage1->pDbPage);
      put4byte(&pBt->pPage1->aData[32], 0);
      put4byte(&pBt->pPage1->aData[36], 0);
      put4byte(&pBt->pPage1->aData[28], nFin);
      sqlite3PagerTruncateImage(pBt->pPager, nFin);
      pBt->nPage = nFin;
    }
    if( rc!=SQLITE_OK ){
      sqlite3PagerRollback(pPager);
    }
  }

  assert( nRef==sqlite3PagerRefcount(pPager) );
  return rc;
}
#else /* ifndef SQLITE_OMIT_AUTOVACUUM */
# define setChildPtrmaps(x) SQLITE_OK
#endif

/*
** This routine does the first phase of a two-phase commit.  This routine
** causes a rollback journal to be created (if it does not already exist)
** and populated with enough information so that if a power loss occurs
** the database can be restored to its original state by playing back
** the journal.  Then the contents of the journal are flushed out to
** the disk.  After the journal is safely on oxide, the changes to the
** database are written into the database file and flushed to oxide.
** At the end of this call, the rollback journal still exists on the
** disk and we are still holding all locks, so the transaction has not
** committed.  See sqlite3BtreeCommitPhaseTwo() for the second phase of the
** commit process.
**
** This call is a no-op if no write-transaction is currently active on pBt.
**
** Otherwise, sync the database file for the btree pBt. zMaster points to
** the name of a master journal file that should be written into the
** individual journal file, or is NULL, indicating no master journal file 
** (single database transaction).
**
** When this is called, the master journal should already have been
** created, populated with this journal pointer and synced to disk.
**
** Once this is routine has returned, the only thing required to commit
** the write-transaction for this database file is to delete the journal.
** ������������׶��ύ�ĵ�һ�׶Ρ�������������ع���־(�����������)�������㹻����Ϣ�Ա���������ֹ�����ģ�
** ���ݿ����ͨ����־�ָ���ԭ����״̬����־�����ݾ�д�ص����̡�����־��ȫд�غ�,���ĵ����ݿ�д�����ݿ��ļ���д�����̡�
** ������ý���ʱ,�ع���־�ڴ�������Ȼ���ں��Գ������е���,��������û���ύ.�ύ���̵ĵڶ����׶���sqlite3BtreeCommitPhaseTwo().
** ���û��д����Ŀǰ��Ծ��pBt�ϣ������������һ���ղ�����
** ����,��btree pBt�����ݿ��ļ�ͬ����zMasterָ������־�ļ������֣�������־�ļ�Ӧ��д����������־�ļ��С�
** ����Ϊ��,��ʾû������־�ļ�(�������ݿ�����)����������ʱ,������־Ӧ���ѱ�����������־ָ���ͬ��д�뵽���̡�
** һ���ú������أ�Ψһ���������Ҫ�ύд�������ݿ��ļ�Ӧ��ɾ����־��
/*�ύ�׶η�Ϊ2���֣����ǵ�1���֣��ɹ�����SQLITE_OK���ڶ�������sqlite3BtreeCommitPhaseTwo*/
int sqlite3BtreeCommitPhaseOne(Btree *p, const char *zMaster){
  int rc = SQLITE_OK;
  if( p->inTrans==TRANS_WRITE ){/*��û��д���񣬴˵���Ϊ�ղ���*/
    BtShared *pBt = p->pBt;
    sqlite3BtreeEnter(p);
#ifndef SQLITE_OMIT_AUTOVACUUM
    if( pBt->autoVacuum ){
      rc = autoVacuumCommit(pBt);
      if( rc!=SQLITE_OK ){
        sqlite3BtreeLeave(p);
        return rc;
      }
    }
#endif
    rc = sqlite3PagerCommitPhaseOne(pBt->pPager, zMaster, 0);
    sqlite3BtreeLeave(p);
  }
  return rc;
}

/*
** This function is called from both BtreeCommitPhaseTwo() and BtreeRollback()
** at the conclusion of a transaction.
** ��һ������Ľ�����BtreeCommitPhaseTwo()��BtreeRollback()�������������
*/
static void btreeEndTransaction(Btree *p){
  BtShared *pBt = p->pBt;
  assert( sqlite3BtreeHoldsMutex(p) );

  btreeClearHasContent(pBt);	/*����λͼ���󣬻����ù����ڴ�*/
  if( p->inTrans>TRANS_NONE && p->db->activeVdbeCnt>1 ){
    /* If there are other active statements that belong to this database
    ** handle, downgrade to a read-only transaction. The other statements
    ** may still be reading from the database.  
	** �����������Ծ������������ݿ⴦���������,�µ�һ��ֻ�������������������ڴ����ݿ��ж���*/
    downgradeAllSharedCacheTableLocks(p);
    p->inTrans = TRANS_READ;
  }else{
    /* If the handle had any kind of transaction open, decrement the 
    ** transaction count of the shared btree. If the transaction count 
    ** reaches 0, set the shared state to TRANS_NONE. The unlockBtreeIfUnused()
    ** call below will unlock the pager.  
	** ��������κ����񿪷ţ������ɹ���B����������������������ﵽ0,���ù���״̬ΪTRANS_NONE��
	** unlockBtreeIfUnused()�������潫����pager��*/
    if( p->inTrans!=TRANS_NONE ){ /*������*/
      clearAllSharedCacheTableLocks(p);
      pBt->nTransaction--;
      if( 0==pBt->nTransaction ){
        pBt->inTransaction = TRANS_NONE; /*����������*/
      }
    }

    /* Set the current transaction state to TRANS_NONE and unlock the 
    ** pager if this call closed the only read or write transaction.  
	** ���õ�ǰ����״̬TRANS_NONE�����������ùر�Ψһ����д���������pager��*/
    p->inTrans = TRANS_NONE;
    unlockBtreeIfUnused(pBt);/*���ر������Ķ���д���񣬽���pager*/
  }

  btreeIntegrity(p);
}

/*
** Commit the transaction currently in progress.
** �ύ��ǰ�ڽ����е�����
** This routine implements the second phase of a 2-phase commit.  The
** sqlite3BtreeCommitPhaseOne() routine does the first phase and should
** be invoked prior to calling this routine.  The sqlite3BtreeCommitPhaseOne()
** routine did all the work of writing information out to disk and flushing the
** contents so that they are written onto the disk platter.  All this
** routine has to do is delete or truncate or zero the header in the
** the rollback journal (which causes the transaction to commit) and
** drop locks.
**
** Normally, if an error occurs while the pager layer is attempting to 
** finalize the underlying journal file, this function returns an error and
** the upper layer will attempt a rollback. However, if the second argument
** is non-zero then this b-tree transaction is part of a multi-file 
** transaction. In this case, the transaction has already been committed 
** (by deleting a master journal file) and the caller will ignore this 
** functions return code. So, even if an error occurs in the pager layer,
** reset the b-tree objects internal state to indicate that the write
** transaction has been closed. This is quite safe, as the pager will have
** transitioned to the error state.
**
** This will release the write lock on the database file.  If there
** are no active cursors, it also releases the read lock.
*/

/*�ύ�׶η�Ϊ2���֣����ǵ�2���֡���һ������sqlite3BtreeCommitPhaseOne
���߹�ϵ:
1����һ�׶ε��ú���ܵ��õڶ��׶Ρ�
2����һ�׶����д��Ϣ�����̡��ڶ��׶��ͷ�д�������޻�α꣬�ͷŶ�����
Ϊʲô��2���׶�?
��֤���нڵ��ڽ��������ύʱ����һ���ԡ��ڷֲ�ʽϵͳ�У�ÿ���ڵ���Ȼ����֪���Լ��Ĳ���ʱ�ɹ�
����ʧ�ܣ�ȴ�޷�֪�������ڵ�Ĳ����ĳɹ���ʧ�ܡ���һ�������Խ����ڵ�ʱ��Ϊ�˱��������
ACID���ԣ���Ҫ����һ����ΪЭ���ߵ������ͳһ�ƿ����нڵ�(����������)�Ĳ������������ָʾ
��Щ�ڵ��Ƿ�Ҫ�Ѳ�����������������ύ(���罫���º������д����̵ȵ�)����ˣ����׶��ύ
���㷨˼·���Ը���Ϊ�� �����߽������ɰ�֪ͨЭ���ߣ�����Э���߸������в����ߵķ����鱨����
���������Ƿ�Ҫ�ύ����������ֹ������
*/

int sqlite3BtreeCommitPhaseTwo(Btree *p, int bCleanup){

  if( p->inTrans==TRANS_NONE ) return SQLITE_OK;
  sqlite3BtreeEnter(p);
  btreeIntegrity(p); /*���������һ����״̬*/

  /* If the handle has a write-transaction open, commit the shared-btrees 
  ** transaction and set the shared state to TRANS_READ.
  ** ����þ���п�����д����,�ύshared-btrees��������������״̬ΪTRANS_READ��*/
  if( p->inTrans==TRANS_WRITE ){
    int rc;
    BtShared *pBt = p->pBt;
    assert( pBt->inTransaction==TRANS_WRITE );
    assert( pBt->nTransaction>0 );
    rc = sqlite3PagerCommitPhaseTwo(pBt->pPager); /*�ύ����*/
    if( rc!=SQLITE_OK && bCleanup==0 ){
      sqlite3BtreeLeave(p);
      return rc;
    }
    pBt->inTransaction = TRANS_READ;
  }

  btreeEndTransaction(p);
  sqlite3BtreeLeave(p);
  return SQLITE_OK;
}

/*
** Do both phases of a commit.  ���׶������ύ
*/
int sqlite3BtreeCommit(Btree *p){
  int rc;
  sqlite3BtreeEnter(p);
  rc = sqlite3BtreeCommitPhaseOne(p, 0);
  if( rc==SQLITE_OK ){
    rc = sqlite3BtreeCommitPhaseTwo(p, 0);
  }
  sqlite3BtreeLeave(p);
  return rc;
}

#ifndef NDEBUG
/*
** Return the number of write-cursors open on this handle. This is for use
** in assert() expressions, so it is only compiled if NDEBUG is not
** defined.
** ���������ؿ�����д�α�������assert()�����ʹ�ã�������NDEBUGû�ж�������ֻ���롣
** For the purposes of this routine, a write-cursor is any cursor that
** is capable of writing to the databse.  That means the cursor was
** originally opened for writing and the cursor has not be disabled
** by having its state changed to CURSOR_FAULT.
*/
/*����д�α������*/
static int countWriteCursors(BtShared *pBt){
  BtCursor *pCur;
  int r = 0;
  for(pCur=pBt->pCursor; pCur; pCur=pCur->pNext){
    if( pCur->wrFlag && pCur->eState!=CURSOR_FAULT ) r++; /*pCur->eState!=CURSOR_FAULTʱ���α괦�ڼ���״̬*/
  }
  return r;
}
#endif

/*
** This routine sets the state to CURSOR_FAULT and the error
** code to errCode for every cursor on BtShared that pBtree
** references.
** ����pBtree���õ�BtShared�ϵ��α����������״̬����ΪCURSOR_FAULT�ʹ������ΪerrCode��
** Every cursor is tripped, including cursors that belong
** to other database connections that happen to be sharing
** the cache with pBtree.
** ÿ���α궼�����������������������ݿ����ӵ��α꣬�������ݿ�����������pBtree�����档
** This routine gets called when a rollback occurs.
** All cursors using the same cache must be tripped
** to prevent them from trying to use the btree after
** the rollback.  The rollback may have deleted tables
** or moved root pages, so it is not sufficient to
** save the state of the cursor.  The cursor must be
** invalidated.
** �������ع�ʱ������������á�����ʹ����ͬ�Ļ�����α���뱻����,����ֹ�����ڻع�֮����ͼ����btree��
** �ع�����ɾ������ƶ���ҳ��,���Ա����α��״̬�ǲ����ġ��α����ʧЧ��
*/
/*���α�״̬����Ϊ CURSOR_FAULT ����error code����ΪerrCode*/
void sqlite3BtreeTripAllCursors(Btree *pBtree, int errCode){
  BtCursor *p;
  if( pBtree==0 ) return;
  sqlite3BtreeEnter(pBtree);
  for(p=pBtree->pBt->pCursor; p; p=p->pNext){
    int i;
    sqlite3BtreeClearCursor(p);
    p->eState = CURSOR_FAULT;
    p->skipNext = errCode;
    for(i=0; i<=p->iPage; i++){
      releasePage(p->apPage[i]);
      p->apPage[i] = 0;
    }
  }
  sqlite3BtreeLeave(pBtree);
}

/*
** Rollback the transaction in progress.  All cursors will be
** invalided by this operation.  Any attempt to use a cursor
** that was open at the beginning of this operation will result
** in an error.
** �ع������е�����ͨ���ò����������α�ʧЧ���κ���ͼʹ�������������ʼʱ�򿪵��α궼�����
** This will release the write lock on the database file.  If there
** are no active cursors, it also releases the read lock.
** �⽫�ͷ������ݿ��ļ��е�д�������û�л�Ծ���α�,Ҳ���ͷŶ�����
*/
/*�ع�����ʹ�����α�ʧЧ*/
int sqlite3BtreeRollback(Btree *p, int tripCode){
  int rc;
  BtShared *pBt = p->pBt;
  MemPage *pPage1;

  sqlite3BtreeEnter(p);
  if( tripCode==SQLITE_OK ){
    rc = tripCode = saveAllCursors(pBt, 0, 0);
  }else{
    rc = SQLITE_OK;
  }
  if( tripCode ){
    sqlite3BtreeTripAllCursors(p, tripCode);
  }
  btreeIntegrity(p);

  if( p->inTrans==TRANS_WRITE ){/*�ͷ����ݿ��е�д��*/
    int rc2;

    assert( TRANS_WRITE==pBt->inTransaction );
    rc2 = sqlite3PagerRollback(pBt->pPager);
    if( rc2!=SQLITE_OK ){
      rc = rc2;
    }

    /* The rollback may have destroyed the pPage1->aData value.  So
    ** call btreeGetPage() on page 1 again to make
    ** sure pPage1->aData is set correctly. 
	** �ع������Ѿ��ƻ���pPage1->aData��ֵ��������1ҳ���ڴ˵���btreeGetPage()��ȷ��pPage1->aData������ȷ��*/
    if( btreeGetPage(pBt, 1, &pPage1, 0)==SQLITE_OK ){
      int nPage = get4byte(28+(u8*)pPage1->aData);
      testcase( nPage==0 );
      if( nPage==0 ) sqlite3PagerPagecount(pBt->pPager, &nPage);
      testcase( pBt->nPage!=nPage );
      pBt->nPage = nPage;
      releasePage(pPage1);
    }
    assert( countWriteCursors(pBt)==0 );
    pBt->inTransaction = TRANS_READ;/*��û�л�α꣬�ͷŶ�����*/
  }

  btreeEndTransaction(p);
  sqlite3BtreeLeave(p);
  return rc;
}
/*
** Start a statement subtransaction. The subtransaction can be rolled
** back independently of the main transaction. You must start a transaction 
** before starting a subtransaction. The subtransaction is ended automatically 
** if the main transaction commits or rolls back.
** ��ʼһ�������������������Ա�����������Ļع����ڿ�ʼ������֮ǰ
** Ҫ��һ�������������Ҫ�����ύ��ع����������Զ�������
** Statement subtransactions are used around individual SQL statements
** that are contained within a BEGIN...COMMIT block.  If a constraint
** error occurs within the statement, the effect of that one statement
** can be rolled back without having to rollback the entire transaction.
** ���������ʹ���ڰ�����һ��BEGIN...COMMIT���еĵ���SQL����С�
** ���������ڳ���һ��Լ������,�������Ч�������ǻع�,
** Ȼ��������Ҫ�ع���������
** A statement sub-transaction is implemented as an anonymous savepoint. The
** value passed as the second parameter is the total number of savepoints,
** including the new anonymous savepoint, open on the B-Tree. i.e. if there
** are no active savepoints and no other statement-transactions open,
** iStatement is 1. This anonymous savepoint can be released or rolled back
** using the sqlite3BtreeSavepoint() function.
** ����sub-transaction��ʵ��Ϊһ�������ı���㡣��Ϊ�ڶ�����������
** ��ֵ�Ǳ���������,�����µ����������,��b-ree���ŵġ������û��
** ��Ծ�ı�����û������statement-transactions����,��ôiStatement��1��
** ��������ı�������ʹ��sqlite3BtreeSavepoint()�ͷŻ�ع���
*/
int sqlite3BtreeBeginStmt(Btree *p, int iStatement){  //��ʼһ�����������
  int rc;
  BtShared *pBt = p->pBt;
  sqlite3BtreeEnter(p);
  assert( p->inTrans==TRANS_WRITE );
  assert( (pBt->btsFlags & BTS_READ_ONLY)==0 );
  assert( iStatement>0 );
  assert( iStatement>p->db->nSavepoint );
  assert( pBt->inTransaction==TRANS_WRITE );
  /* At the pager level, a statement transaction is a savepoint with
  ** an index greater than all savepoints created explicitly using
  ** SQL statements. It is illegal to open, release or rollback any
  ** such savepoints while the statement transaction savepoint is active.
  ** ��pager ˮƽ��,���������һ�������,����һ��������ȷʹ��SQL��䴴�������б���
  ** �����������������񱣴���Ծʱ�����ţ��ͷŻ�ع��κ������ı���㶼�ǷǷ��ġ�
  */
  rc = sqlite3PagerOpenSavepoint(pBt->pPager, iStatement);
  sqlite3BtreeLeave(p);
  return rc;
}
/*
** The second argument to this function, op, is always SAVEPOINT_ROLLBACK
** or SAVEPOINT_RELEASE. This function either releases or rolls back the
** savepoint identified by parameter iSavepoint, depending on the value 
** of op.
** �ú����ĵڶ�������,op,����SAVEPOINT_ROLLBACK��SAVEPOINT_RELEASE��
** ��������ͷŻ�ع�������iSavepointʶ��ı����,��������op��ֵ��
** Normally, iSavepoint is greater than or equal to zero. However, if op is
** SAVEPOINT_ROLLBACK, then iSavepoint may also be -1. In this case the 
** contents of the entire transaction are rolled back. This is different
** from a normal transaction rollback, as no locks are released and the
** transaction remains open.
** ͨ��,iSavepoint>=0��Ȼ��,���op�� SAVEPOINT_ROLLBACK,��ôiSavepointҲ������1��������
** �����, ������������ݻع�����������������ع��ǲ�ͬ��,��Ϊû�����ͷ�,������Ȼ���š�
*/
/*opΪSAVEPOINT_ROLLBACK��SAVEPOINT_RELEASE�����ݴ�ֵ�ͷŻ��߻ع������*/
int sqlite3BtreeSavepoint(Btree *p, int op, int iSavepoint){    //���ͷŻ��ǻع������ڲ���op��ֵ
  int rc = SQLITE_OK;
  if( p && p->inTrans==TRANS_WRITE ){
    BtShared *pBt = p->pBt;
    assert( op==SAVEPOINT_RELEASE || op==SAVEPOINT_ROLLBACK );
    assert( iSavepoint>=0 || (iSavepoint==-1 && op==SAVEPOINT_ROLLBACK) );
    sqlite3BtreeEnter(p);
    rc = sqlite3PagerSavepoint(pBt->pPager, op, iSavepoint);
    if( rc==SQLITE_OK ){
      if( iSavepoint<0 && (pBt->btsFlags & BTS_INITIALLY_EMPTY)!=0 ){
        pBt->nPage = 0;
      }
      rc = newDatabase(pBt);
      pBt->nPage = get4byte(28 + pBt->pPage1->aData);
      /* The database size was written into the offset 28 of the header
      ** when the transaction started, so we know that the value at offset
      ** 28 is nonzero. 
	  ** ������ʼʱ�����ݿ�Ĵ�С�Ǳ�д��ͷ����ƫ����28���ģ������ƫ����28��ֵ�Ƿ���ġ�*/
      assert( pBt->nPage>0 );
    }
    sqlite3BtreeLeave(p);
  }
  return rc;
}

/*
** Create a new cursor for the BTree whose root is on the page
** iTable. If a read-only cursor is requested, it is assumed that
** the caller already has at least a read-only transaction open
** on the database already. If a write-cursor is requested, then
** the caller is assumed to have an open write transaction.
**
** If wrFlag==0, then the cursor can only be used for reading.
** If wrFlag==1, then the cursor can be used for reading or for
** writing if other conditions for writing are also met.  These
** are the conditions that must be met in order for writing to
** be allowed:
**
** 1:  The cursor must have been opened with wrFlag==1
**
** 2:  Other database connections that share the same pager cache
**     but which are not in the READ_UNCOMMITTED state may not have
**     cursors open with wrFlag==0 on the same table.  Otherwise
**     the changes made by this write cursor would be visible to
**     the read cursors in the other database connection.
**
** 3:  The database must be writable (not on read-only media)
**
** 4:  There must be an active transaction.
**
** No checking is done to make sure that page iTable really is the
** root page of a b-tree.  If it is not, then the cursor acquired
** will not work correctly.
**
** It is assumed that the sqlite3BtreeCursorZero() has been called
** on pCur to initialize the memory space prior to invoking this routine.
*/
/*
ΪBTree����һ���µ��α꣬B���ĸ���ҳiTable�ϡ�
�������һ��ֻ���α꣬���ݿ���������һ��ֻ������򿪡�
������������д�α꣬������һ�򿪵�д����
��wrFlag== 0�����α�������ڶ�ȡ��
��wrFlag== 1�����α�����ڶ���������д��
1��wrFlag==1�α�����Ѿ��򿪡�
2��������ͬ��ҳ���棬 ����READ_UNCOMMITTED״̬��wrFlag==0 ʱ��
�α���ܲ��Ǵ�״̬��
3�����ݿ�����ǿ�д�ģ�������ֻ�����ʣ�
4��������һ���������
�����ڵ����������֮ǰ��sqlite3BtreeCursorZero���������ã�
��pCur��ʼ���ڴ�ռ䡣
*/
static int btreeCursor(
  Btree *p,                              /* The btree */                                  //pΪB��
  int iTable,                            /* Root page of table to open */      //���ŵı�ĸ�ҳ
  int wrFlag                           /* 1 to write. 0 read-only */              //wrFlagΪ1��ʾд��0��ʾֻ��
  struct KeyInfo *pKeyInfo,              /* First arg to comparison function */     //�ȽϺ����ĵ�һ������
  BtCursor *pCur                         /* Space for new cursor */        //���α�ռ�
){
  BtShared *pBt = p->pBt;                /* Shared b-tree handle */   //�ɹ���B�����

  assert( sqlite3BtreeHoldsMutex(p) );
  assert( wrFlag==0 || wrFlag==1 );

  /* The following assert statements verify that if this is a sharable 
  ** b-tree database, the connection is holding the required table locks, 
  ** and that no other connection has any open cursor that conflicts with 
  ** this lock.  
  ** ����Ķ��Ժ��������֤�ǲ���һ���ɹ����B�����ݿ⣬��֤���ӳ�����Ҫ�ı�������
  ** û�����������������ͻ���κο��ŵ��αꡣ
  */
	/*����������֤���������һ���ɹ���
B�����ݿ⣬���ӳ�������ı�����
��û���������Ӿ����κδ򿪵��α��������ͻ
 */

  assert( hasSharedCacheTableLock(p, iTable, pKeyInfo!=0, wrFlag+1) );
  assert( wrFlag==0 || !hasReadConflicts(p, iTable) );

  /* Assert that the caller has opened the required transaction. */  //���Ե������Կ��������������
  assert( p->inTrans>TRANS_NONE );
  assert( wrFlag==0 || p->inTrans==TRANS_WRITE );
  assert( pBt->pPage1 && pBt->pPage1->aData );

  if( NEVER(wrFlag && (pBt->btsFlags & BTS_READ_ONLY)!=0) ){
    return SQLITE_READONLY;
  }
  if( iTable==1 && btreePagecount(pBt)==0 ){
    assert( wrFlag==0 );
    iTable = 0;
  }

  /* Now that no other errors can occur, finish filling in the BtCursor
  ** variables and link the cursor into the BtShared list.  
  ** ����û������������,��ɸ�BtCursor������ֵ�������α굽BtShared�б�*/
  pCur->pgnoRoot = (Pgno)iTable;
  pCur->iPage = -1;
  pCur->pKeyInfo = pKeyInfo;
  pCur->pBtree = p;
  pCur->pBt = pBt;
  pCur->wrFlag = (u8)wrFlag;
  pCur->pNext = pBt->pCursor;
  if( pCur->pNext ){
    pCur->pNext->pPrev = pCur;
  }
  pBt->pCursor = pCur;
  pCur->eState = CURSOR_INVALID;
  pCur->cachedRowid = 0;
  return SQLITE_OK;
}
/*
����һ��ָ���ض�B-tree���αꡣ�α�����Ƕ��α꣬Ҳ������д�α꣬���Ƕ��α��д�α겻��ͬʱ��
ͬһ��B-tree�д��ڡ�
*/
int sqlite3BtreeCursor(
  Btree *p,                                   /* The btree */                                        //pΪB��
  int iTable,                                 /* Root page of table to open */            //���ŵı�ĸ�ҳ
  int wrFlag,                                 /* 1 to write. 0 read-only */                  //wrFlagΪ1��ʾд��0��ʾֻ��
  struct KeyInfo *pKeyInfo,                   /* First arg to xCompare() */   //�ȽϺ����ĵ�һ������
  BtCursor *pCur                              /* Write new cursor here */            //д�µ��α굽����
){
  int rc;
  sqlite3BtreeEnter(p);
  rc = btreeCursor(p, iTable, wrFlag, pKeyInfo, pCur);
  sqlite3BtreeLeave(p);
  return rc;
}

/*
** Return the size of a BtCursor object in bytes.
** ����BtCursor������ֽڴ�С
** This interfaces is needed so that users of cursors can preallocate
** sufficient storage to hold a cursor.  The BtCursor object is opaque
** to users so they cannot do the sizeof() themselves - they must call
** this routine.
** ����ӿ���Ϊ���α���û�����Ԥ�ȷ����㹻�洢�ռ������һ���αꡣ
** BtCursor������û���͸��,�������ǲ�����sizeof()������������������
*/
	/*
	**����һ��BtCursor����Ĵ�С(���ֽڼ�)����Ҫ�ýӿ�ʹ�α����Ԥ�ȷ���	
	**�㹻�Ĵ洢�ռ䡣��BtCursor������û��ǲ�͸����
	**���û������ǲ�����sizeof����- ���Ǳ�����ô˳���
	*/
int sqlite3BtreeCursorSize(void){  //����BtCursor������ֽڴ�С
  return ROUND8(sizeof(BtCursor));
}

/*
** Initialize memory that will be converted into a BtCursor object.
**
** The simple approach here would be to memset() the entire object
** to zero.  But it turns out that the apPage[] and aiIdx[] arrays
** do not need to be zeroed and they are large, so we can save a lot
** of run-time by skipping the initialization of those elements.
*/
/*
**��ʼ���洢������ת����һ��BtCursor����
**
**����һ���򵥷�������memset()������������Ϊ�㡣����ʵ֤����apPage[]��aiIdx[]���鲻��Ҫ
**���е��㣬���ǱȽϴ���������ͨ��������ЩԪ�صĳ�ʼ�������Խ�ʡ�ܶ������ʱ�䡣
*/
void sqlite3BtreeCursorZero(BtCursor *p){  //��memset()������������Ϊ��
  memset(p, 0, offsetof(BtCursor, iPage));
}

/*
** Set the cached rowid value of every cursor in the same database file
** as pCur and having the same root page number as pCur.  The value is
** set to iRowid.
**
** Only positive rowid values are considered valid for this cache.
** The cache is initialized to zero, indicating an invalid cache.
** A btree will work fine with zero or negative rowids.  We just cannot
** cache zero or negative rowids, which means tables that use zero or
** negative rowids might run a little slower.  But in practice, zero
** or negative rowids are very uncommon so this should not be a problem.
*/
	/*
	**��ͬ�����ݿ��ļ�������ÿ���α��cache�кš�
	**��ֵ����ΪiRowid��ֻ������rowidֵ����Ϊ�������ڸû��档
	**���ٻ��汻��ʼ��Ϊ�㣬��ʾһ����Ч�ĸ��ٻ���洢����
	**һ��B������򸺵�rowid������������
	**���ٻ���Ϊ��򸺵�rowid���У�����ζ�ű�ʹ�����
	**����rowid����������һ�㡣����ʵ���У���
	**�򸺵�rowid�ǳ��ټ������ⲻӦ����һ�����⡣
	*/

void sqlite3BtreeSetCachedRowid(BtCursor *pCur, sqlite3_int64 iRowid){   //������ͬ�����ݿ��ļ�����ÿ���α��cache�к�
  BtCursor *p;
  for(p=pCur->pBt->pCursor; p; p=p->pNext){
    if( p->pgnoRoot==pCur->pgnoRoot ) p->cachedRowid = iRowid;
  }
  assert( pCur->cachedRowid==iRowid );
}

/*
** Return the cached rowid for the given cursor.  A negative or zero
** return value indicates that the rowid cache is invalid and should be
** ignored.  If the rowid cache has never before been set, then a
** zero is returned.
*/
/*
**�����α�Ļ����rowid������Ϊ���
**����ֵ��ʾ��rowid���ٻ�����Ч����Ӧ
**���ԡ����rowid������ǰ��δ�����ã���
**�����㡣
*/
sqlite3_int64 sqlite3BtreeGetCachedRowid(BtCursor *pCur){      //�����α�Ļ����rowid
  return pCur->cachedRowid;
}

/*
** Close a cursor.  The read lock on the database file is released
** when the last cursor is closed.
** �ر�B-tree�αꡣ������α�ر�ʱ�ͷ����ݿ��ϵĶ�����
*/   /*�ر�B-tree�α�*/
int sqlite3BtreeCloseCursor(BtCursor *pCur){  // �ر�B-tree�α�
  Btree *pBtree = pCur->pBtree;
  if( pBtree ){
    int i;
    BtShared *pBt = pCur->pBt;
    sqlite3BtreeEnter(pBtree);
    sqlite3BtreeClearCursor(pCur);
    if( pCur->pPrev ){
      pCur->pPrev->pNext = pCur->pNext;
    }else{
      pBt->pCursor = pCur->pNext;
    }
    if( pCur->pNext ){
      pCur->pNext->pPrev = pCur->pPrev;
    }
    for(i=0; i<=pCur->iPage; i++){
      releasePage(pCur->apPage[i]);
    }
    unlockBtreeIfUnused(pBt);
    invalidateOverflowCache(pCur);
    /* sqlite3_free(pCur); */
    sqlite3BtreeLeave(pBtree);
  }
  return SQLITE_OK;
}

/*
** Make sure the BtCursor* given in the argument has a valid
** BtCursor.info structure.  If it is not already valid, call
** btreeParseCell() to fill it in.
** ȷ����argument�и�����BtCursor ��һ����Ч��BtCursor.info�ṹ�������δ��Ч,����btreeParseCell()ʹ֮��Ч��
** BtCursor.info is a cache of the information in the current cell.
** Using this cache reduces the number of calls to btreeParseCell().
** BtCursor.info��һ���ڵ�ǰ��Ԫ�е���Ϣ���档ʹ�����������ٵ���btreeParseCell()��������
** 2007-06-25:  There is a bug in some versions of MSVC that cause the
** compiler to crash when getCellInfo() is implemented as a macro.
** But there is a measureable speed advantage to using the macro on gcc
** (when less compiler optimizations like -Os or -O0 are used and the
** compiler is not doing agressive inlining.)  So we use a real function
** for MSVC and a macro for everything else.  Ticket #2457.
*/
#ifndef NDEBUG
  static void assertCellInfo(BtCursor *pCur){       /*��֤BtCursor����Ч��BtCursor.info�ṹ������Ч������btreeParseCell()ȥ���*/
    CellInfo info;
    int iPage = pCur->iPage;
    memset(&info, 0, sizeof(info));                        //��info��ǰsizeof(info)���ֽ� ��0�滻������info ��
    btreeParseCell(pCur->apPage[iPage], pCur->aiIdx[iPage], &info);        //������Ԫ���ݿ�
    assert( memcmp(&info, &pCur->info, sizeof(info))==0 );
  }
#else
  #define assertCellInfo(x)
#endif
#ifdef _MSC_VER
  /* Use a real function in MSVC to work around bugs in that compiler. */   //��MSVC��ʹ��һ�������ĺ����������������
  static void getCellInfo(BtCursor *pCur){
    if( pCur->info.nSize==0 ){
      int iPage = pCur->iPage;
      btreeParseCell(pCur->apPage[iPage],pCur->aiIdx[iPage],&pCur->info);
      pCur->validNKey = 1;
    }else{
      assertCellInfo(pCur);
    }
  }
#else /* if not _MSC_VER */
  /* Use a macro in all other compilers so that the function is inlined */  //����������������ʹ�ú�,���������������ġ�
#define getCellInfo(pCur)                                                      \
  if( pCur->info.nSize==0 ){                                                   \
    int iPage = pCur->iPage;                                                   \
    btreeParseCell(pCur->apPage[iPage],pCur->aiIdx[iPage],&pCur->info); \
    pCur->validNKey = 1;                                                       \
  }else{                                                                       \
    assertCellInfo(pCur);                                                      \
  }
#endif /* _MSC_VER */

#ifndef NDEBUG  /* The next routine used only within assert() statements */  //��һ������ֻ��assert()���ʹ�á�
/*
** Return true if the given BtCursor is valid.  A valid cursor is one
** that is currently pointing to a row in a (non-empty) table.
** This is a verification routine is used only within assert() statements.
*/
	/*
	**���������BtCursor����Ч��,����true��һ����Ч���α����ڷǿ�
	**�ı��е�ǰָ����С�����һ����֤����ֻ��assert()�����ʹ�á�
	*/
int sqlite3BtreeCursorIsValid(BtCursor *pCur){          //������BtCursor�Ƿ���Ч
  return pCur && pCur->eState==CURSOR_VALID;
}
#endif /* NDEBUG */

/*
** Set *pSize to the size of the buffer needed to hold the value of
** the key for the current entry.  If the cursor is not pointing
** to a valid entry, *pSize is set to 0. 
** pSizeΪbuffer�Ĵ�С��buffer�������浱ǰ��Ŀ(��pCurָ��)��keyֵ������α�δָ��һ����Ч����Ŀ,* pSize����Ϊ0��
** For a table with the INTKEY flag set, this routine returns the key
** itself, not the number of bytes in the key.
** ��INTKEY��־�ı�����,����������عؼ��ֱ���,�����ǹؼ��ֵ��ֽ�����
** The caller must position the cursor prior to invoking this routine.
** ������
** This routine cannot fail.  It always returns SQLITE_OK.  
*/
/*pSizeΪbuffer�Ĵ�С��buffer�������浱ǰ��Ŀ(��pCurָ��)��keyֵ��*/
int sqlite3BtreeKeySize(BtCursor *pCur, i64 *pSize){
  assert( cursorHoldsMutex(pCur) );
  assert( pCur->eState==CURSOR_INVALID || pCur->eState==CURSOR_VALID );
  if( pCur->eState!=CURSOR_VALID ){          /*�α�ָ����Ч��Ŀ��pSize = 0*/
    *pSize = 0;
  }else{
    getCellInfo(pCur);
    *pSize = pCur->info.nKey;
  }
  return SQLITE_OK;
}

/*
** Set *pSize to the number of bytes of data in the entry the
** cursor currently points to.
** ����*pSize����������ֽ�����*pSize �ڵ�ǰ�α�ָ�����Ŀ�С�
** The caller must guarantee that the cursor is pointing to a non-NULL
** valid entry.  In other words, the calling procedure must guarantee
** that the cursor has Cursor.eState==CURSOR_VALID.
** �����߱��뱣֤�α�ָ��һ���ǿ���Ч��Ŀ�����仰˵,���ó�����뱣֤�α�Cursor.eState = = CURSOR_VALID��
** Failure is not possible.  This function always returns SQLITE_OK.
** It might just as well be a procedure (returning void) but we continue
** to return an integer result code for historical reasons.
** �������ʼ�շ���SQLITE_OK��Ҳ���ܽ�����һ������(����void)�������Ǽ�������һ������������롣
*/

int sqlite3BtreeDataSize(BtCursor *pCur, u32 *pSize){      /*�趨��ǰ�α���ָ��¼�����ݳ��ȣ��ֽڣ�*/
  assert( cursorHoldsMutex(pCur) );
  assert( pCur->eState==CURSOR_VALID );
  getCellInfo(pCur);
  *pSize = pCur->info.nData;/*��pSize�������ݳ���*/
  return SQLITE_OK;
}

/*
** Given the page number of an overflow page in the database (parameter
** ovfl), this function finds the page number of the next page in the 
** linked list of overflow pages. If possible, it uses the auto-vacuum
** pointer-map data instead of reading the content of page ovfl to do so. 
** ����һ�������ݿ��е����ҳҳ��(����Ϊovfl),��������ҵ���һ�����ҳ���������
** ��ҳ���ҳ�롣�������,��ʹ��auto-vacuum  pointer-map���ݶ����Ƕ�ҳ��ovfl�����ݡ�
** If an error occurs an SQLite error code is returned. Otherwise:
** ������ִ���һ��SQLite���ش�����롣����
** The page number of the next overflow page in the linked list is 
** written to *pPgnoNext. If page ovfl is the last page in its linked 
** list, *pPgnoNext is set to zero. 
** �����б��е���һ�����ҳ���ҳ�뱻д��* pPgnoNext�С����ovflҳ�����һҳ,* pPgnoNext����Ϊ�㡣
** If ppPage is not NULL, and a reference to the MemPage object corresponding
** to page number pOvfl was obtained, then *ppPage is set to point to that
** reference. It is the responsibility of the caller to call releasePage()
** on *ppPage to free the reference. In no reference was obtained (because
** the pointer-map was used to obtain the value for *pPgnoNext), then
** *ppPage is set to zero.
** ���ppPage�ǿ�,�� MemPage������Ӧҳ��pOvfl�����ñ����,������* ppPageָ�����á�
** ��������ߵĵ���releasePage()��ppPage���ͷ����� ����û�����û��(��Ϊ pointer-map���������* pPgnoNext��ֵ), 
** ��ô * ppPage����Ϊ�㡣
*/
/*�ҵ���һ�����ҳ��ҳ�š�*/
static int getOverflowPage(
  BtShared *pBt,               /* The database file */                                                       //���ݿ��ļ�
  Pgno ovfl,                   /* Current overflow page number */                                    //��ǰ���ҳ��     
  MemPage **ppPage,            /* OUT: MemPage handle (may be NULL) */         //�ڴ�ҳ���������ΪNULL��
  Pgno *pPgnoNext              /* OUT: Next overflow page number */                       //��һ�����ҳ��ҳ��
){
  Pgno next = 0;
  MemPage *pPage = 0;
  int rc = SQLITE_OK;

  assert( sqlite3_mutex_held(pBt->mutex) );
  assert(pPgnoNext);

#ifndef SQLITE_OMIT_AUTOVACUUM
  /* Try to find the next page in the overflow list using the
  ** autovacuum pointer-map pages. Guess that the next page in 
  ** the overflow list is page number (ovfl+1). If that guess turns 
  ** out to be wrong, fall back to loading the data of page 
  ** number ovfl to determine the next page number.
  ** ��ͼ�ҵ�����б��е���autovacuum pointer-mapҳ����ʹ�õ���һ��ҳ�档�²�����б�
  ** ����һ��ҳ��ҳ��Ϊ(ovfl + 1)������²��Ǵ��,�ص�����ҳ��ovfl��������ȷ����һ��ҳ�롣
  */
  if( pBt->autoVacuum ){
    Pgno pgno;
    Pgno iGuess = ovfl+1;/*�²���һ�����ҳҳ��Ϊovfl+1*/
    u8 eType;

    while( PTRMAP_ISPAGE(pBt, iGuess) || iGuess==PENDING_BYTE_PAGE(pBt) ){
      iGuess++;/*û�¶ԣ�ҳ�������*/
    }

    if( iGuess<=btreePagecount(pBt) ){
      rc = ptrmapGet(pBt, iGuess, &eType, &pgno);
      if( rc==SQLITE_OK && eType==PTRMAP_OVERFLOW2 && pgno==ovfl ){
        next = iGuess;
        rc = SQLITE_DONE;
      }
    }
  }
#endif

  assert( next==0 || rc==SQLITE_DONE );
  if( rc==SQLITE_OK ){
    rc = btreeGetPage(pBt, ovfl, &pPage, 0);
    assert( rc==SQLITE_OK || pPage==0 );
    if( rc==SQLITE_OK ){
      next = get4byte(pPage->aData);
    }
  }

  *pPgnoNext = next;
  if( ppPage ){/*ppPage���գ�ppPageָ��reference*/
    *ppPage = pPage;
  }else{
    releasePage(pPage);
  }
  return (rc==SQLITE_DONE ? SQLITE_OK : rc);
}

/*
** Copy data from a buffer to a page, or from a page to a buffer.
** �����ݴӻ��������Ƶ�һ��ҳ��,���һ��ҳ�浽��������
** pPayload is a pointer to data stored on database page pDbPage.
** If argument eOp is false, then nByte bytes of data are copied
** from pPayload to the buffer pointed at by pBuf. If eOp is true,
** then sqlite3PagerWrite() is called on pDbPage and nByte bytes
** of data are copied from the buffer pBuf to pPayload.
** pPayload��һ��ָ��ָ��洢�����ݿ�ҳpDbPage�ϵ����ݡ��������eOp Ϊ��,��ônByte�ֽڵ����ݴ�pPayload���� ��pBuf
** ָ��Ļ�����ָ�š����eOpΪ��, Ȼ����pDbPage�ϵ���sqlite3PagerWrite()����nByte�ֽڵ����ݴ�pBuf���� ��pPayload��
** SQLITE_OK is returned on success, otherwise an error code.
** �ɹ��򷵻�SQLITE_OK �����򷵻ش�����롣
*/
	/*
	**�ӻ������������ݵ�һ��ҳ�棬���ߴ�һ��ҳ�渴�Ƶ���������
	**
	** pPayload��һ��ָ��洢�����ݿ�ҳ��pDbPage���ݵ�ָ�롣
	**�������EOP�Ǽٵģ���ônByte�ֽ����ݱ�����
	**ͨ��PBUF��pPayload��������ָ�����EOP��true��
	**Ȼ��sqlite3PagerWrite���������ã�nByte�ֽ�
	**���ݴӻ����� pBuf ��pPayload�����ơ�
	** �ɹ�����SQLITE_OK�����򷵻ش�����롣
	*/

static int copyPayload(                      //�����ݴӻ��������Ƶ�һ��ҳ��,���һ��ҳ�浽������
  void *pPayload,           /* Pointer to page data */                             //ҳ�����ݵ�ָ��
  void *pBuf,               /* Pointer to buffer */                                     //������ָ��
  int nByte,                /* Number of bytes to copy */                          //�������ֽ���
  int eOp,                  /* 0 -> copy from page, 1 -> copy to page */   //eOpΪ0��ҳ��������������Ϊ1��ӻ�����������ҳ
  DbPage *pDbPage           /* Page containing pPayload */               //ҳ����pPayload
){
  if( eOp ){
    /* Copy data from buffer to page (a write operation) */  //Ϊ1��ӻ�����������ҳ
    int rc = sqlite3PagerWrite(pDbPage);
    if( rc!=SQLITE_OK ){
      return rc;
    }
    memcpy(pPayload, pBuf, nByte);
  }else{
    /* Copy data from page to buffer (a read operation) */    //eOpΪ0��ҳ������������
    memcpy(pBuf, pPayload, nByte);
  }
  return SQLITE_OK;
}

/*
** This function is used to read or overwrite payload information
** for the entry that the pCur cursor is pointing to. If the eOp
** parameter is 0, this is a read operation (data copied into
** buffer pBuf). If it is non-zero, a write (data copied from
** buffer pBuf).
**
** A total of "amt" bytes are read or written beginning at "offset".
** Data is read to or from the buffer pBuf.
**
** The content being read or written might appear on the main page
** or be scattered out on multiple overflow pages.
**
** If the BtCursor.isIncrblobHandle flag is set, and the current
** cursor entry uses one or more overflow pages, this function
** allocates space for and lazily popluates the overflow page-list 
** cache array (BtCursor.aOverflow). Subsequent calls use this
** cache to make seeking to the supplied offset more efficient.
**
** Once an overflow page-list cache has been allocated, it may be
** invalidated if some other cursor writes to the same table, or if
** the cursor is moved to a different row. Additionally, in auto-vacuum
** mode, the following events may invalidate an overflow page-list cache.
**
**   * An incremental vacuum,
**   * A commit in auto_vacuum="full" mode,
**   * Creating a table (may require moving an overflow page).
*/
	/*
	** �����α�pCur��ָ�����Ŀ���˹������ڶ���д��Ч�غ���Ϣ��
	** �������eOpΪ0������һ�������������ݸ��Ƶ�����pBuf����
	** �������Ϊ�㣬���ݴӻ��� pBuf�и��Ƶ�ҳ��
	** �ܹ��С�amt���ֽڵ����ݱ�����д��"offset"��ʼ.
	** ���ڶ���д�����ݿ��ܳ�������ҳ�ϻ��ɢ�ڶ�����ҳ�� ���������BtCursor.isIncrblobHandle��־,
	** �ҵ�ǰ�α���Ŀʹ��һ���������ҳ,�����������ռ�������������ҳ�б�������(BtCursor.aOverflow)��
	** ��������ʹ���������ʹѰ���ṩ��ƫ�Ƹ���Ч�ʡ�
	** һ�����ҳ�б����Ѿ������䣬��������α�д��
	** ͬһ��������Ч,�����α��ƶ�����ͬ�С�����,��auto-vacuum ģʽ,������¼�����ʹһ���������ҳ�б�����Ч��
	**   * ����ʽ����,
	**   * �� auto_vacuum="full" ģʽ�е�һ���ύ,
	**   * ������ (����Ҫ���ƶ�һ�����ҳ).
	*/

static int accessPayload(          //����д��Ч�غ���Ϣ
  BtCursor *pCur,      /* Cursor pointing to entry to read from */     //�α�ָ��Ҫ��ȡ���ݵ���Ŀ
  u32 offset,          /* Begin reading this far into payload */               //��ʼ��һ��������Ч�غ�
  u32 amt,             /* Read this many bytes */                                       //��ȡ�����ֽ�
  unsigned char *pBuf, /* Write the bytes into this buffer */             //д��д���ݵ�������
  int eOp              /* zero to read. non-zero to write. */                        //���������д
){
  unsigned char *aPayload;
  int rc = SQLITE_OK;
  u32 nKey;
  int iIdx = 0;
  MemPage *pPage = pCur->apPage[pCur->iPage]; /* Btree page of current entry */  //��ǰ��Ŀ��B��ҳ
  BtShared *pBt = pCur->pBt;                  /* Btree this cursor belongs to */                    //���α�������B��

  assert( pPage );
  assert( pCur->eState==CURSOR_VALID );
  assert( pCur->aiIdx[pCur->iPage]<pPage->nCell );
  assert( cursorHoldsMutex(pCur) );

  getCellInfo(pCur);
  aPayload = pCur->info.pCell + pCur->info.nHeader;
  nKey = (pPage->intKey ? 0 : (int)pCur->info.nKey);

  if( NEVER(offset+amt > nKey+pCur->info.nData) 
   || &aPayload[pCur->info.nLocal] > &pPage->aData[pBt->usableSize]
  ){
    /* Trying to read or write past the end of the data is an error */  //����ȥ����д����ĩ���Ժ�����ݽ�����һ������
    return SQLITE_CORRUPT_BKPT;
  }

  /* Check if data must be read/written to/from the btree page itself. */  //����Ƿ��B��ҳ����ȥ��ȡ����д
  if( offset<pCur->info.nLocal ){
    int a = amt;
    if( a+offset>pCur->info.nLocal ){
      a = pCur->info.nLocal - offset;
    }
    rc = copyPayload(&aPayload[offset], pBuf, a, eOp, pPage->pDbPage);
    offset = 0;
    pBuf += a;
    amt -= a;
  }else{
    offset -= pCur->info.nLocal;
  }

  if( rc==SQLITE_OK && amt>0 ){
    const u32 ovflSize = pBt->usableSize - 4;  /* Bytes content per ovfl page */  //ÿ��ovflҳ�������ֽ�����
    Pgno nextPage;

    nextPage = get4byte(&aPayload[pCur->info.nLocal]);

#ifndef SQLITE_OMIT_INCRBLOB
    /* If the isIncrblobHandle flag is set and the BtCursor.aOverflow[]
    ** has not been allocated, allocate it now. The array is sized at
    ** one entry for each overflow page in the overflow chain. The
    ** page number of the first overflow page is stored in aOverflow[0],
    ** etc. A value of 0 in the aOverflow[] array means "not yet known"
    ** (the cache is lazily populated).
    */
    /*���isIncrblobHandle��־�����ú�BtCursor.aOverflow[]��δ���䣬���ڷ�������   
	aOverflow[]�е�0��ʾ�������֡���
	*/
    if( pCur->isIncrblobHandle && !pCur->aOverflow ){
      int nOvfl = (pCur->info.nPayload-pCur->info.nLocal+ovflSize-1)/ovflSize;
      pCur->aOverflow = (Pgno *)sqlite3MallocZero(sizeof(Pgno)*nOvfl);
      /* nOvfl is always positive.  If it were zero, fetchPayload would have
      ** been used instead of this routine. */
      if( ALWAYS(nOvfl) && !pCur->aOverflow ){
        rc = SQLITE_NOMEM;
      }
    }

    /* If the overflow page-list cache has been allocated and the
    ** entry for the first required overflow page is valid, skip
    ** directly to it.
    */
    /*������ҳ�б����ѷ��䣬��һ�����ҳ������Ч�ģ�ֱ����������	*/  
    if( pCur->aOverflow && pCur->aOverflow[offset/ovflSize] ){
      iIdx = (offset/ovflSize);
      nextPage = pCur->aOverflow[iIdx];
      offset = (offset%ovflSize);
    }
#endif

    for( ; rc==SQLITE_OK && amt>0 && nextPage; iIdx++){

#ifndef SQLITE_OMIT_INCRBLOB
      /* If required, populate the overflow page-list cache. */   //�����Ҫ,������ҳ�б��档
      if( pCur->aOverflow ){
        assert(!pCur->aOverflow[iIdx] || pCur->aOverflow[iIdx]==nextPage);
        pCur->aOverflow[iIdx] = nextPage;
      }
#endif

      if( offset>=ovflSize ){
        /* The only reason to read this page is to obtain the page
        ** number for the next page in the overflow chain. The page
        ** data is not required. So first try to lookup the overflow
        ** page-list cache, if any, then fall back to the getOverflowPage()
        ** function.
        */
        /*����ҳ��Ψһԭ����Ϊ�˻�ø�ҳ�������ҳ�����е���һ��ҳ�š�
        ҳ�治��Ҫ���ݡ����ԣ������Ų������ҳ�б��棬����еĻ���
        ���˻ص�getOverflowPage()������
*/
#ifndef SQLITE_OMIT_INCRBLOB
        if( pCur->aOverflow && pCur->aOverflow[iIdx+1] ){
          nextPage = pCur->aOverflow[iIdx+1];
        } else 
#endif
          rc = getOverflowPage(pBt, nextPage, 0, &nextPage);
        offset -= ovflSize;
      }else{
        /* Need to read this page properly. It contains some of the
        ** range of data that is being read (eOp==0) or written (eOp!=0).
		** ��Ҫ��ȷ�ض����ҳ�档��������һЩ���ڱ���ȡ(eOp==0)��д(eOp! = 0)�����ݷ�Χ��
        */
#ifdef SQLITE_DIRECT_OVERFLOW_READ
        sqlite3_file *fd;
#endif
        int a = amt;
        if( a + offset > ovflSize ){
          a = ovflSize - offset;
        }

#ifdef SQLITE_DIRECT_OVERFLOW_READ
        /* If all the following are true:
        **
        **   1) this is a read operation, and 
        **   2) data is required from the start of this overflow page, and
        **   3) the database is file-backed, and
        **   4) there is no open write-transaction, and
        **   5) the database is not a WAL database,
        **
        ** then data can be read directly from the database file into the
        ** output buffer, bypassing the page-cache altogether. This speeds
        ** up loading large records that span many overflow pages.
        */
        /*������е���������Ϊ�棺
		1������һ��������������
		2�����ݱ����ҳ���󣬲���
		3�����ݿ��ļ���֧�֣�����
		4��û�п�����д����
		5�����ݿⲻ��һ��WAL���ݿ⣬
		Ȼ�����ݿ���ֱ�Ӵ����ݿ��ļ����뵽
		������塣��ӿ������ҳ�Ĵ��¼�ļ��ء�
*/
        if( eOp==0                                             /* (1) */
         && offset==0                                          /* (2) */
         && pBt->inTransaction==TRANS_READ                     /* (4) */
         && (fd = sqlite3PagerFile(pBt->pPager))->pMethods     /* (3) */
         && pBt->pPage1->aData[19]==0x01                       /* (5) */
        ){
          u8 aSave[4];
          u8 *aWrite = &pBuf[-4];
          memcpy(aSave, aWrite, 4);
          rc = sqlite3OsRead(fd, aWrite, a+4, (i64)pBt->pageSize*(nextPage-1));
          nextPage = get4byte(aWrite);
          memcpy(aWrite, aSave, 4);
        }else
#endif

        {
          DbPage *pDbPage;
          rc = sqlite3PagerGet(pBt->pPager, nextPage, &pDbPage);
          if( rc==SQLITE_OK ){
            aPayload = sqlite3PagerGetData(pDbPage);
            nextPage = get4byte(aPayload);
            rc = copyPayload(&aPayload[offset+4], pBuf, a, eOp, pDbPage);
            sqlite3PagerUnref(pDbPage);
            offset = 0;
          }
        }
        amt -= a;
        pBuf += a;
      }
    }
  }

  if( rc==SQLITE_OK && amt>0 ){
    return SQLITE_CORRUPT_BKPT;
  }
  return rc;
}

/*
** Read part of the key associated with cursor pCur.  Exactly
** "amt" bytes will be transfered into pBuf[].  The transfer
** begins at "offset".
** �����α�pCur�����Ĺؼ���."amt"�ֽ����������ݵ�����pBuf[]��.���ݴ�"offset"��ʼ.
** The caller must ensure that pCur is pointing to a valid row
** in the table.
** �����߱���ȷ��pCurָ�������Ч���С�
** Return SQLITE_OK on success or an error code if anything goes
** wrong.  An error is returned if "offset+amt" is larger than
** the available payload.
** �ɹ��򷵻�SQLITE_OK������κδ������򷵻ش�����롣��� "offset+amt"�ȿ��õ���Ч�غɻ����򷵻�һ������
*/
/*���α�pCurָ�������Ч��һ�У�����SQLITE_OK����"offset+amt">��Ч���أ�����error code*/
int sqlite3BtreeKey(BtCursor *pCur, u32 offset, u32 amt, void *pBuf){
  assert( cursorHoldsMutex(pCur) );
  assert( pCur->eState==CURSOR_VALID );/*�α�ָ�������Ч��һ��*/
  assert( pCur->iPage>=0 && pCur->apPage[pCur->iPage] );
  assert( pCur->aiIdx[pCur->iPage]<pCur->apPage[pCur->iPage]->nCell );
  return accessPayload(pCur, offset, amt, (unsigned char*)pBuf, 0);/*
  ���ص�ǰ�α���ָ��¼�Ĺؼ��֡�
  */
}

/*
** Read part of the data associated with cursor pCur.  Exactly
** "amt" bytes will be transfered into pBuf[].  The transfer
** begins at "offset".
** �����α�pCur������������."amt"�ֽ����������ݵ�����pBuf[]��.���ݴ�"offset"��ʼ.
** Return SQLITE_OK on success or an error code if anything goes
** wrong.  An error is returned if "offset+amt" is larger than
** the available payload.
** �ɹ��򷵻�SQLITE_OK������κδ������򷵻ش�����롣��� "offset+amt"�ȿ��õ���Ч�غɻ����򷵻�һ������
*/
/*
���ص�ǰ�α���ָ��¼������
*/
int sqlite3BtreeData(BtCursor *pCur, u32 offset, u32 amt, void *pBuf){        
  int rc;

#ifndef SQLITE_OMIT_INCRBLOB
  if ( pCur->eState==CURSOR_INVALID ){
    return SQLITE_ABORT;
  }
#endif

  assert( cursorHoldsMutex(pCur) );
  rc = restoreCursorPosition(pCur);
  if( rc==SQLITE_OK ){
    assert( pCur->eState==CURSOR_VALID );
    assert( pCur->iPage>=0 && pCur->apPage[pCur->iPage] );
    assert( pCur->aiIdx[pCur->iPage]<pCur->apPage[pCur->iPage]->nCell );
    rc = accessPayload(pCur, offset, amt, pBuf, 0);
  }
  return rc;
}

/*
** Return a pointer to payload information from the entry that the 
** pCur cursor is pointing to.  The pointer is to the beginning of
** the key if skipKey==0 and it points to the beginning of data if
** skipKey==1.  The number of bytes of available key/data is written
** into *pAmt.  If *pAmt==0, then the value returned will not be
** a valid pointer.
** ���ش�pCur�α�����ָ�����Ŀ����Ч�غɵ�ָ�롣���skipKey==0 ���ָ��ָ��ؼ��ֵĿ�
** ʼ���������skipKey==1ָ��������Ŀ�ʼ�����ùؼ��ֻ���������ֽ�����д�뵽*pAmt.��
** ��* pAmt ==0,��ô���ص�ֵӦ��һ����Ч��ָ�롣
** This routine is an optimization.  It is common for the entire key
** and data to fit on the local page and for there to be no overflow
** pages.  When that is so, this routine can be used to access the
** key and data without making a copy.  If the key and/or data spills
** onto overflow pages, then accessPayload() must be used to reassemble
** the key/data and copy it into a preallocated buffer.
** �˺�����һ���Ż�������ȫ���ؼ�������������Ӧ����ҳ���ǳ�����,���Ҷ���û�����ҳ���Ҳ�ǳ����ġ�
** �����,�������������������û�и����Ĺؼ��ֺ��������������/�����������ҳ�������,
** ��ôaccessPayload()�������������/���ݲ����临�Ƶ�һ��Ԥ�ȷ���Ļ�������
** The pointer returned by this routine looks directly into the cached
** page of the database.  The data might change or move the next time
** any btree routine is called.
** ָ��ͨ���������ֱ�Ӳ鿴���ݿ��еĻ���ҳ���ء��´��κ�btree����������ʱ���ݿ��ܻ�ı���ƶ���
*/
	/*
	����һ��ָ����Ч�غɵ�ָ�롣���skipKey== 0��ָ��ָ��key�Ŀ�ʼ�����skipKey==1��
	ָ��ָ��data�Ŀ�ʼ�����* PAMT== 0���򷵻ص�ֵ��Ϊ��Чָ�롣�˳�����һ���Ż���
	���κ�B�����򱻵��ã������ݿ��ܻ�ı���ƶ���
	*/
static const unsigned char *fetchPayload(       //���ش�pCur�α�����ָ�����Ŀ����Ч�غɵ�ָ��
  BtCursor *pCur,      /* Cursor pointing to entry to read from */      //ָ��Ҫ��ȡ��Ŀ���α�
  int *pAmt,           /* Write the number of available bytes here */   //д�����ֽ���
  int skipKey          /* read beginning at data if this is true */     //�߼�ֵΪ������ݿ�ʼ��
){
  unsigned char *aPayload;
  MemPage *pPage;
  u32 nKey;
  u32 nLocal;

  assert( pCur!=0 && pCur->iPage>=0 && pCur->apPage[pCur->iPage]);
  assert( pCur->eState==CURSOR_VALID );
  assert( cursorHoldsMutex(pCur) );
  pPage = pCur->apPage[pCur->iPage];
  assert( pCur->aiIdx[pCur->iPage]<pPage->nCell );
  if( NEVER(pCur->info.nSize==0) ){
    btreeParseCell(pCur->apPage[pCur->iPage], pCur->aiIdx[pCur->iPage],
                   &pCur->info);
  }
  aPayload = pCur->info.pCell;
  aPayload += pCur->info.nHeader;
  if( pPage->intKey ){
    nKey = 0;
  }else{
    nKey = (int)pCur->info.nKey;
  }
  if( skipKey ){
    aPayload += nKey;
    nLocal = pCur->info.nLocal - nKey;
  }else{
    nLocal = pCur->info.nLocal;
    assert( nLocal<=nKey );
  }
  *pAmt = nLocal;
  return aPayload;
}

/*
** For the entry that cursor pCur is point to, return as
** many bytes of the key or data as are available on the local
** b-tree page.  Write the number of available bytes into *pAmt.
** 
** The pointer returned is ephemeral.  The key/data may move
** or be destroyed on the next call to any Btree routine,
** including calls from other threads against the same cache.
** Hence, a mutex on the BtShared should be held prior to calling
** this routine.
** 
** These routines is used to get quick access to key and data
** in the common case where no overflow pages are used.
*/
	/*
	�����α�pCurָ����Ŀ������key ���� data�ļ����ֽڡ�д���õ��ֽ�����*pAmt��
	���ص�ָ���Ƕ��ݵġ�����һ�ε����κ�B��������ʱ��key/data�����ƶ������٣�
	���������̶߳���ͬ����ĵ��á����BtShared�ϵ�һ��������Ӧ���ڵ����������
	ǰ���������������û�����ҳʹ�õĳ�������£����ڿ��ٷ���key �� data��
	*/

const void *sqlite3BtreeKeyFetch(BtCursor *pCur, int *pAmt){  //�����α�pCurָ����Ŀ��key �ڱ���B��ҳ�Ͽ��õ��ֽ���
  const void *p = 0;
  assert( sqlite3_mutex_held(pCur->pBtree->db->mutex) );
  assert( cursorHoldsMutex(pCur) );
  if( ALWAYS(pCur->eState==CURSOR_VALID) ){
    p = (const void*)fetchPayload(pCur, pAmt, 0);
  }
  return p;
}

const void *sqlite3BtreeDataFetch(BtCursor *pCur, int *pAmt){  //�����α�pCurָ����Ŀ��data�ڱ���B��ҳ�Ͽ��õ��ֽ���
  const void *p = 0;
  assert( sqlite3_mutex_held(pCur->pBtree->db->mutex) );
  assert( cursorHoldsMutex(pCur) );
  if( ALWAYS(pCur->eState==CURSOR_VALID) ){
    p = (const void*)fetchPayload(pCur, pAmt, 1);
  }
  return p;
}

/*
** Move the cursor down to a new child page.  The newPgno argument is the
** page number of the child page to move to.
** �ƶ��α굽��һ���µĺ���ҳ�档newPgno���������Ե��ĺ���ҳ���ҳ�롣
** This function returns SQLITE_CORRUPT if the page-header flags field of
** the new child page does not match the flags field of the parent (i.e.
** if an intkey page appears to be the parent of a non-intkey page, or
** vice-versa).
** ����µĺ���ҳ���ҳͷ��־��͸��ڵ�ı�־��ƥ�䣬��������SQLITE_CORRUPT.
** (����һ���ڲ��ؼ���ҳ�Ƿ��ڲ��ؼ���ҳ�ĸ��ڵ�ҳ)
*/
	/*
	���ƹ�굽һ���µ���ҳ�档��newPgno��������ҳ���ƶ���ҳ�š�
	���page-header��־���丸�ڵ�ı�־��ƥ�䣬�˺�������SQLITE_CORRUPT��
	*/

static int moveToChild(BtCursor *pCur, u32 newPgno){      //�ƶ��α굽��һ���µĺ���ҳ��
  int rc;
  int i = pCur->iPage;
  MemPage *pNewPage;
  BtShared *pBt = pCur->pBt;

  assert( cursorHoldsMutex(pCur) );
  assert( pCur->eState==CURSOR_VALID );
  assert( pCur->iPage<BTCURSOR_MAX_DEPTH );
  if( pCur->iPage>=(BTCURSOR_MAX_DEPTH-1) ){
    return SQLITE_CORRUPT_BKPT;
  }
  rc = getAndInitPage(pBt, newPgno, &pNewPage);
  if( rc ) return rc;
  pCur->apPage[i+1] = pNewPage;
  pCur->aiIdx[i+1] = 0;
  pCur->iPage++;

  pCur->info.nSize = 0;
  pCur->validNKey = 0;
  if( pNewPage->nCell<1 || pNewPage->intKey!=pCur->apPage[i]->intKey ){
    return SQLITE_CORRUPT_BKPT;
  }
  return SQLITE_OK;
}

#if 0
/*
** Page pParent is an internal (non-leaf) tree page. This function 
** asserts that page number iChild is the left-child if the iIdx'th
** cell in page pParent. Or, if iIdx is equal to the total number of
** cells in pParent, that page number iChild is the right-child of
** the page.
** ҳ��pParent��B���ڲ�(��Ҷ)ҳ������������������iIdx��Ԫ��ҳpParent����ҳ��iChild
** �����ӡ���,���iIdx����pParent�е�Ԫ������,��ôҳ��iChild��ҳ����Һ��ӡ�
*/
static void assertParentIndex(MemPage *pParent, int iIdx, Pgno iChild){  //�ж�ҳpParent�ĺ���ҳ�������ӻ����Һ���
  assert( iIdx<=pParent->nCell );
  if( iIdx==pParent->nCell ){
    assert( get4byte(&pParent->aData[pParent->hdrOffset+8])==iChild );
  }else{
    assert( get4byte(findCell(pParent, iIdx))==iChild );
  }
}
#else
#  define assertParentIndex(x,y,z) 
#endif

/*
** Move the cursor up to the parent page.
** �����ƶ��α굽���ڵ�ҳ�档
** pCur->idx is set to the cell index that contains the pointer
** to the page we are coming from.  If we are coming from the
** right-most child page then pCur->idx is set to one more than
** the largest cell index.
*/
	/*
	���α��ƶ�����ҳ��.pCur-> IDX���趨Ϊ����ָ��ҳ��ָ��ĵ�Ԫ����.���Ϊ���ұߵ���ҳ�棬pCur-> IDX������Ϊ�����ĵ�Ԫ�����������
	*/
static void moveToParent(BtCursor *pCur){     //�����ƶ��α굽���ڵ�ҳ�档
  assert( cursorHoldsMutex(pCur) );
  assert( pCur->eState==CURSOR_VALID );
  assert( pCur->iPage>0 );
  assert( pCur->apPage[pCur->iPage] );

  /* UPDATE: It is actually possible for the condition tested by the assert
  ** below to be untrue if the database file is corrupt. This can occur if
  ** one cursor has modified page pParent while a reference to it is held 
  ** by a second cursor. Which can only happen if a single page is linked
  ** into more than one b-tree structure in a corrupt database.  
  ** ������ݿ��ļ��жϵĻ�����assert���Ե��������п���Ϊ�١������һ������
  ** ����һ���α����ʱһ���α��Ѿ��޸���ҳ��pParent ����ô����ܻᷢ����
  ** ���һ��ҳ�汻���ӵ���ֹһ��b -���ṹ�ڲ��������ݿ���������Ҳ����֡�*/
#if 0
  assertParentIndex(
    pCur->apPage[pCur->iPage-1], 
    pCur->aiIdx[pCur->iPage-1], 
    pCur->apPage[pCur->iPage]->pgno
  );
#endif
  testcase( pCur->aiIdx[pCur->iPage-1] > pCur->apPage[pCur->iPage-1]->nCell );

  releasePage(pCur->apPage[pCur->iPage]);
  pCur->iPage--;
  pCur->info.nSize = 0;
  pCur->validNKey = 0;
}

/*
** Move the cursor to point to the root page of its b-tree structure.
** �ƶ��α�ָ��B���ṹ�ĸ�ҳ��
** If the table has a virtual root page, then the cursor is moved to point
** to the virtual root page instead of the actual root page. A table has a
** virtual root page when the actual root page contains no cells and a 
** single child page. This can only happen with the table rooted at page 1.
** �������һ�������ҳ��,���α��ƶ�ָ�������ҳ�������ʵ�ʵĸ�ҳ����ʵ�ʸ�ҳ
** ��������Ԫ�͵�һ����ҳ��ʱ���ϻ���һ�������ҳ�档��ֻ�ܷ����ڵ�1ҳ�ı��ϡ�
** If the b-tree structure is empty, the cursor state is set to 
** CURSOR_INVALID. Otherwise, the cursor is set to point to the first
** cell located on the root (or virtual root) page and the cursor state
** is set to CURSOR_VALID.
** ���b-���ṹΪ��,���α�״̬��ΪCURSOR_INVALID������,�α꽫ָ��λ�ڸ�(�������)
** ҳ��ĵ�һ����Ԫ,�����α�״̬��ΪCURSOR_VALID��
** If this function returns successfully, it may be assumed that the
** page-header flags indicate that the [virtual] root-page is the expected 
** kind of b-tree page (i.e. if when opening the cursor the caller did not
** specify a KeyInfo structure the flags byte is set to 0x05 or 0x0D,
** indicating a table b-tree, or if the caller did specify a KeyInfo 
** structure the flags byte is set to 0x02 or 0x0A, indicating an index
** b-tree).
** �������������سɹ�,�����ܻ�ٶ�ͷ���ı�־����[����]��ҳ��b-��ҳ��(�����
** �����α������û��ָ��KeyInfo�ṹ������ֽ�����Ϊ0 x05��0 x0d,˵���Ǳ�b-��
** ,�������������ָ��KeyInfo�ṹ����ֽ�����Ϊ0x02��0x0a,˵��������b-tree)��
*/
static int moveToRoot(BtCursor *pCur){      //�ƶ��α�ָ��B���ṹ�ĸ�ҳ
  MemPage *pRoot;
  int rc = SQLITE_OK;
  Btree *p = pCur->pBtree;
  BtShared *pBt = p->pBt;

  assert( cursorHoldsMutex(pCur) );
  assert( CURSOR_INVALID < CURSOR_REQUIRESEEK );
  assert( CURSOR_VALID   < CURSOR_REQUIRESEEK );
  assert( CURSOR_FAULT   > CURSOR_REQUIRESEEK );
  if( pCur->eState>=CURSOR_REQUIRESEEK ){
    if( pCur->eState==CURSOR_FAULT ){
      assert( pCur->skipNext!=SQLITE_OK );
      return pCur->skipNext;
    }
    sqlite3BtreeClearCursor(pCur);
  }

  if( pCur->iPage>=0 ){
    int i;
    for(i=1; i<=pCur->iPage; i++){
      releasePage(pCur->apPage[i]);
    }
    pCur->iPage = 0;
  }else if( pCur->pgnoRoot==0 ){
    pCur->eState = CURSOR_INVALID;
    return SQLITE_OK;
  }else{
    rc = getAndInitPage(pBt, pCur->pgnoRoot, &pCur->apPage[0]);
    if( rc!=SQLITE_OK ){
      pCur->eState = CURSOR_INVALID;
      return rc;
    }
    pCur->iPage = 0;

    /* If pCur->pKeyInfo is not NULL, then the caller that opened this cursor
    ** expected to open it on an index b-tree. Otherwise, if pKeyInfo is
    ** NULL, the caller expects a table b-tree. If this is not the case,
    ** return an SQLITE_CORRUPT error.  
	** ���pCur->pKeyInfo�ǿ�,��ô�����ߴ򿪽�������B���ϴ򿪵��α�.����,���pKeyInfoΪ��,
	** ��������Ҫ��B��������֮�⣬����һ��SQLITE_CORRUPT����*/
    assert( pCur->apPage[0]->intKey==1 || pCur->apPage[0]->intKey==0 );
    if( (pCur->pKeyInfo==0)!=pCur->apPage[0]->intKey ){
      return SQLITE_CORRUPT_BKPT;
    }
  }

  /* Assert that the root page is of the correct type. This must be the
  ** case as the call to this function that loaded the root-page (either
  ** this call or a previous invocation) would have detected corruption 
  ** if the assumption were not true, and it is not possible for the flags 
  ** byte to have been modified while this cursor is holding a reference
  ** to the page.  
  ** �жϸ�ҳ����ȷ�����͡����ô˺������صĸ�ҳʱһ���������ģ�����������û���ǰ���ã���������ǲ���ȷ�ģ�
  ** �ú�������⵽�𻵡����Ҷ��ڱ�־�ֽ���˵�����α����ڳ���ҳ��һ������ʱ���޸��ǲ����ܵġ�*/
  pRoot = pCur->apPage[0];
  assert( pRoot->pgno==pCur->pgnoRoot );
  assert( pRoot->isInit && (pCur->pKeyInfo==0)==pRoot->intKey );

  pCur->aiIdx[0] = 0;
  pCur->info.nSize = 0;
  pCur->atLast = 0;
  pCur->validNKey = 0;

  if( pRoot->nCell==0 && !pRoot->leaf ){
    Pgno subpage;
    if( pRoot->pgno!=1 ) return SQLITE_CORRUPT_BKPT;
    subpage = get4byte(&pRoot->aData[pRoot->hdrOffset+8]);
    pCur->eState = CURSOR_VALID;
    rc = moveToChild(pCur, subpage);
  }else{
    pCur->eState = ((pRoot->nCell>0)?CURSOR_VALID:CURSOR_INVALID);
  }
  return rc;
}

/*
** Move the cursor down to the left-most leaf entry beneath the
** entry to which it is currently pointing.
** �ƶ��α굽����Ҷ����Ŀ���α굱ǰ��ָ�����Ŀ��
** The left-most leaf is the one with the smallest key - the first
** in ascending order.
** �����Ҷ�ӽڵ�ӵ����С�ļ�ֵ������������ĵ�һ���ؼ��֡�
*/
static int moveToLeftmost(BtCursor *pCur){     //�ƶ��α굽����Ҷ��
  Pgno pgno;
  int rc = SQLITE_OK;
  MemPage *pPage;

  assert( cursorHoldsMutex(pCur) );
  assert( pCur->eState==CURSOR_VALID );
  while( rc==SQLITE_OK && !(pPage = pCur->apPage[pCur->iPage])->leaf ){
    assert( pCur->aiIdx[pCur->iPage]<pPage->nCell );
    pgno = get4byte(findCell(pPage, pCur->aiIdx[pCur->iPage]));
    rc = moveToChild(pCur, pgno);
  }
  return rc;
}

/*
** Move the cursor down to the right-most leaf entry beneath the
** page to which it is currently pointing.  Notice the difference
** between moveToLeftmost() and moveToRightmost().  moveToLeftmost()
** finds the left-most entry beneath the *entry* whereas moveToRightmost()
** finds the right-most entry beneath the *page*.
** �ƶ��α굽���ҵ�Ҷ�ӽڵ㡣ע��moveToLeftmost()��moveToRightmost()�Ĳ�ͬ��moveToLeftmost()
**  ���ҵ������*entry*�µ���Ŀ����moveToRightmost()���ҵ����ұ�*page*�µ���Ŀ��
** The right-most entry is the one with the largest key - the last
** key in ascending order.
** ���ұߵ���Ŀ������������е����ļ�ֵ��
*/
static int moveToRightmost(BtCursor *pCur){   //�ƶ��α굽���ҵ�Ҷ�ӽڵ�
  Pgno pgno;
  int rc = SQLITE_OK;
  MemPage *pPage = 0;

  assert( cursorHoldsMutex(pCur) );
  assert( pCur->eState==CURSOR_VALID );
  while( rc==SQLITE_OK && !(pPage = pCur->apPage[pCur->iPage])->leaf ){
    pgno = get4byte(&pPage->aData[pPage->hdrOffset+8]);
    pCur->aiIdx[pCur->iPage] = pPage->nCell;
    rc = moveToChild(pCur, pgno);
  }
  if( rc==SQLITE_OK ){
    pCur->aiIdx[pCur->iPage] = pPage->nCell-1;
    pCur->info.nSize = 0;
    pCur->validNKey = 0;
  }
  return rc;
}

/* Move the cursor to the first entry in the table.  Return SQLITE_OK
** on success.  Set *pRes to 0 if the cursor actually points to something
** or set *pRes to 1 if the table is empty.
** ���α��ƶ������еĵ�һ��Ԫ�ء����ɹ�,����SQLITE_OK��
** �������ָ��һЩ�ط�������*pResΪ0��������ǿյ�����*pResΪ1��
*/
int sqlite3BtreeFirst(BtCursor *pCur, int *pRes){   //���α��ƶ������еĵ�һ����Ŀ
  int rc;

  assert( cursorHoldsMutex(pCur) );
  assert( sqlite3_mutex_held(pCur->pBtree->db->mutex) );
  rc = moveToRoot(pCur);
  if( rc==SQLITE_OK ){
    if( pCur->eState==CURSOR_INVALID ){
      assert( pCur->pgnoRoot==0 || pCur->apPage[pCur->iPage]->nCell==0 );  //��Ϊ��
      *pRes = 1;
    }else{
      assert( pCur->apPage[pCur->iPage]->nCell>0 );    //�α���ָ��ĳ���ط�
      *pRes = 0;
      rc = moveToLeftmost(pCur);
    }
  }
  return rc;
}

/* Move the cursor to the last entry in the table.  Return SQLITE_OK
** on success.  Set *pRes to 0 if the cursor actually points to something
** or set *pRes to 1 if the table is empty.
** ���α��ƶ������е����һ����Ŀ���ɹ��򷵻�SQLITE_OK�����α���ָ��ĳ����*pResΪ0���Ϊ������*pResΪ1��
*/
int sqlite3BtreeLast(BtCursor *pCur, int *pRes){     //���α��ƶ������е����һ����Ŀ
  int rc;
 
  assert( cursorHoldsMutex(pCur) );
  assert( sqlite3_mutex_held(pCur->pBtree->db->mutex) );

  /* If the cursor already points to the last entry, this is a no-op. */   //���α��Ѿ�ָ�����һ����Ŀ�����޲���
  if( CURSOR_VALID==pCur->eState && pCur->atLast ){
#ifdef SQLITE_DEBUG
    /* This block serves to assert() that the cursor really does point 
    ** to the last entry in the b-tree. */    //�ÿ�����������ж��α��Ѿ�ָ����B���е�������Ŀ��
    int ii;
    for(ii=0; ii<pCur->iPage; ii++){
      assert( pCur->aiIdx[ii]==pCur->apPage[ii]->nCell );
    }
    assert( pCur->aiIdx[pCur->iPage]==pCur->apPage[pCur->iPage]->nCell-1 );
    assert( pCur->apPage[pCur->iPage]->leaf );
#endif
    return SQLITE_OK;
  }

  rc = moveToRoot(pCur);   //����B����ҳ
  if( rc==SQLITE_OK ){
    if( CURSOR_INVALID==pCur->eState ){
      assert( pCur->pgnoRoot==0 || pCur->apPage[pCur->iPage]->nCell==0 );
      *pRes = 1;
    }else{
      assert( pCur->eState==CURSOR_VALID );
      *pRes = 0;
      rc = moveToRightmost(pCur);
      pCur->atLast = rc==SQLITE_OK ?1:0;
    }
  }
  return rc;  //rc����һ��״̬�������ص���1��0.
}

/* Move the cursor so that it points to an entry near the key 
** specified by pIdxKey or intKey.   Return a success code.
** �ƶ��α��Ա���ָ��pIdxKey ��intKeyָ�����Ŀ�����Ĺؼ��֡�
** For INTKEY tables, the intKey parameter is used.  pIdxKey 
** must be NULL.  For index tables, pIdxKey is used and intKey
** is ignored.
** ����INTKEY���õ��ǲ���intKey��pIdxKey�����ǿա�����������ʹ��pIdxKey��intKey���Բ��á�
** If an exact match is not found, then the cursor is always
** left pointing at a leaf page which would hold the entry if it
** were present.  The cursor might point to an entry that comes
** before or after the key.
** ���û���ҵ�׼ȷ��ƥ��,���α���������ָ��Ҷ��ҳ�棬���ҳ���Ǹ��ڵ��򽫱�����Ŀ���α����
** ָ��һ���ؼ���֮ǰ��֮�����Ŀ��
** An integer is written into *pRes which is the result of
** comparing the key with the entry to which the cursor is 
** pointing.  The meaning of the integer written into
** *pRes is as follows:
** һ������д��*pRes���������Ƚϴ�����Ŀ�Ĺؼ��ֺ��α�ָ��Ķ�������д�����������:
**     *pRes<0      The cursor is left pointing at an entry tha                   // �α��뿪ָ��һ��С��intKey / pIdxKey����Ŀ
**                  is smaller than intKey/pIdxKey or if the table is empty       //��������ǿյ��α겻ָ���κεط���
**                  and the cursor is therefore left point to nothing.
**    
**     *pRes==0     The cursor is left pointing at an entry that                  //�α�ָ����һ��intKey/pIdxKey���Ӧ����Ŀ��
**                  exactly matches intKey/pIdxKey.
**    
**     *pRes>0      The cursor is left pointing at an entry that                  //�α�ָ���intKey/pIdxKey�������Ŀ
**                  is larger than intKey/pIdxKey.
**
*/
int sqlite3BtreeMovetoUnpacked(                                                   //�α�ָ��һ��intKey/pIdxKey���Ӧ����Ŀ
  BtCursor *pCur,          /* The cursor to be moved */                           //���α꽫�����ƶ�
  UnpackedRecord *pIdxKey, /* Unpacked index key */                               //��ѹ�������ؼ���
  i64 intKey,              /* The table key */                                    //��ؼ���
  int biasRight,           /* If true, bias the search to the high end */         //�ñ���Ϊ�棬ƫ�Ƶ����
  int *pRes                /* Write search results here */                        //�����ҽ��д��ñ���
){
  int rc;

  assert( cursorHoldsMutex(pCur) );
  assert( sqlite3_mutex_held(pCur->pBtree->db->mutex) );
  assert( pRes );
  assert( (pIdxKey==0)==(pCur->pKeyInfo==0) );

  /* If the cursor is already positioned at the point we are trying
  ** to move to, then just return without doing any work 
  ** ����α��Ѿ���Ҫ�Ƶ��ĵ㣬�򷵻ز�������*/  
  if( pCur->eState==CURSOR_VALID && pCur->validNKey 
   && pCur->apPage[0]->intKey 
  ){
    if( pCur->info.nKey==intKey ){
      *pRes = 0;
      return SQLITE_OK;
    }
    if( pCur->atLast && pCur->info.nKey<intKey ){
      *pRes = -1;
      return SQLITE_OK;
    }
  }
  rc = moveToRoot(pCur);   //ָ��B����ҳ
  if( rc ){
    return rc;
  }
  assert( pCur->pgnoRoot==0 || pCur->apPage[pCur->iPage] );
  assert( pCur->pgnoRoot==0 || pCur->apPage[pCur->iPage]->isInit );
  assert( pCur->eState==CURSOR_INVALID || pCur->apPage[pCur->iPage]->nCell>0 );
  if( pCur->eState==CURSOR_INVALID ){
    *pRes = -1;
    assert( pCur->pgnoRoot==0 || pCur->apPage[pCur->iPage]->nCell==0 );
    return SQLITE_OK;
  }
  assert( pCur->apPage[0]->intKey || pIdxKey );
  for(;;){
    int lwr, upr, idx;
    Pgno chldPg;
    MemPage *pPage = pCur->apPage[pCur->iPage];
    int c;
    /* pPage->nCell must be greater than zero. If this is the root-page
    ** the cursor would have been INVALID above and this for(;;) loop
    ** not run. If this is not the root-page, then the moveToChild() routine
    ** would have already detected db corruption. Similarly, pPage must
    ** be the right kind (index or table) of b-tree page. Otherwise
    ** a moveToChild() or moveToRoot() call would have detected corruption.  
	** pPage->nCell�����0��������Ǹ�ҳ��������α���Ч����for(;;) ѭ������ִ�С�
	** ������Ǹ���ҳ����ô moveToChild()���������db������ͬ���� pPage��������ȷ�����B��
	** ����moveToChild() �� moveToRoot() �ĵ��ý����ֱ�����*/
    assert( pPage->nCell>0 );
    assert( pPage->intKey==(pIdxKey==0) );
    lwr = 0;
    upr = pPage->nCell-1;
    if( biasRight ){
      pCur->aiIdx[pCur->iPage] = (u16)(idx = upr);
    }else{
      pCur->aiIdx[pCur->iPage] = (u16)(idx = (upr+lwr)/2);             //���ֲ���
    }
    for(;;){
      u8 *pCell;          /* Pointer to current cell in pPage */       // ָ��pPage�ĵ�ǰ��Ԫ
      assert( idx==pCur->aiIdx[pCur->iPage] );
      pCur->info.nSize = 0;
      pCell = findCell(pPage, idx) + pPage->childPtrSize;
      if( pPage->intKey ){
        i64 nCellKey;
        if( pPage->hasData ){
          u32 dummy;
          pCell += getVarint32(pCell, dummy);
        }
        getVarint(pCell, (u64*)&nCellKey);
        if( nCellKey==intKey ){
          c = 0;
        }else if( nCellKey<intKey ){
          c = -1;
        }else{
          assert( nCellKey>intKey );
          c = +1;
        }
        pCur->validNKey = 1;
        pCur->info.nKey = nCellKey;
      }else{
        /* The maximum supported page-size is 65536 bytes. This means that
        ** the maximum number of record bytes stored on an index B-Tree
        ** page is less than 16384 bytes and may be stored as a 2-byte
        ** varint. This information is used to attempt to avoid parsing 
        ** the entire cell by checking for the cases where the record is 
        ** stored entirely within the b-tree page by inspecting the first 
        ** 2 bytes of the cell.
		** ֧�ֵ����ҳ���СΪ65536�ֽڡ�����ζ�Ŵ�������B��ҳ�еļ�¼��������ֽ�С��16384�ֽ�,���Դ洢Ϊһ��2�ֽڵı�����
		** ����Ϣ������ͼͨ�����������������Ԫ�����ڸ��������¼��ȫ�洢��B��ҳͨ�����õ�Ԫ�Ŀ�ʼ�������ֽڡ�
        */
        int nCell = pCell[0];
        if( nCell<=pPage->max1bytePayload
         /* && (pCell+nCell)<pPage->aDataEnd */
        ){
          /* This branch runs if the record-size field of the cell is a
          ** single byte varint and the record fits entirely on the main
          ** b-tree page.  
		  **�����Ԫ�ļ�¼����һ�����ֽڵı������Ҽ�¼��ȫ�洢����B��ҳ�ϣ�ִ�и÷�֧��*/
          testcase( pCell+nCell+1==pPage->aDataEnd );
          c = sqlite3VdbeRecordCompare(nCell, (void*)&pCell[1], pIdxKey);
        }else if( !(pCell[1] & 0x80) 
          && (nCell = ((nCell&0x7f)<<7) + pCell[1])<=pPage->maxLocal
          /* && (pCell+nCell+2)<=pPage->aDataEnd */
        ){
          /* The record-size field is a 2 byte varint and the record  
          ** fits entirely on the main b-tree page. 
		  ** �����ļ�¼��С����һ�����ֽڵı������Ҽ�¼��ȫ�洢����B��ҳ�ϡ�*/
          testcase( pCell+nCell+2==pPage->aDataEnd );
          c = sqlite3VdbeRecordCompare(nCell, (void*)&pCell[2], pIdxKey);
        }else{
          /* The record flows over onto one or more overflow pages. In
          ** this case the whole cell needs to be parsed, a buffer allocated
          ** and accessPayload() used to retrieve the record into the
          ** buffer before VdbeRecordCompare() can be called. 
		  ** ��¼����Ǵ洢��һ���������ҳ�ϡ������������������Ԫ��Ҫ����,����һ����������accessPayload()
		  ** ���ڼ�����VdbeRecordCompare()���Ա�����֮ǰ���뵽�������ļ�¼��*/
          void *pCellKey;
          u8 * const pCellBody = pCell - pPage->childPtrSize;
          btreeParseCellPtr(pPage, pCellBody, &pCur->info);
          nCell = (int)pCur->info.nKey;
          pCellKey = sqlite3Malloc( nCell );
          if( pCellKey==0 ){
            rc = SQLITE_NOMEM;
            goto moveto_finish;
          }
          rc = accessPayload(pCur, 0, nCell, (unsigned char*)pCellKey, 0);
          if( rc ){
            sqlite3_free(pCellKey);
            goto moveto_finish;
          }
          c = sqlite3VdbeRecordCompare(nCell, pCellKey, pIdxKey);
          sqlite3_free(pCellKey);
        }
      }
      if( c==0 ){
        if( pPage->intKey && !pPage->leaf ){
          lwr = idx;
          break;
        }else{
          *pRes = 0;
          rc = SQLITE_OK;
          goto moveto_finish;
        }
      }
      if( c<0 ){
        lwr = idx+1;
      }else{
        upr = idx-1;
      }
      if( lwr>upr ){
        break;
      }
      pCur->aiIdx[pCur->iPage] = (u16)(idx = (lwr+upr)/2);
    }
    assert( lwr==upr+1 || (pPage->intKey && !pPage->leaf) );
    assert( pPage->isInit );
    if( pPage->leaf ){
      chldPg = 0;
    }else if( lwr>=pPage->nCell ){
      chldPg = get4byte(&pPage->aData[pPage->hdrOffset+8]);
    }else{
      chldPg = get4byte(findCell(pPage, lwr));
    }
    if( chldPg==0 ){
      assert( pCur->aiIdx[pCur->iPage]<pCur->apPage[pCur->iPage]->nCell );
      *pRes = c;
      rc = SQLITE_OK;
      goto moveto_finish;
    }
    pCur->aiIdx[pCur->iPage] = (u16)lwr;
    pCur->info.nSize = 0;
    pCur->validNKey = 0;
    rc = moveToChild(pCur, chldPg);
    if( rc ) goto moveto_finish;
  }
moveto_finish:
  return rc;
}


/*
** Return TRUE if the cursor is not pointing at an entry of the table.
** ����α�û��ָ����һ����Ŀ����true��
** TRUE will be returned after a call to sqlite3BtreeNext() moves
** past the last entry in the table or sqlite3BtreePrev() moves past
** the first entry.  TRUE is also returned if the table is empty.
** ����sqlite3BtreeNext()���ƶ�����Ķ����һ����Ŀ�����sqlite3BtreePrev()�ƶ�����һ����Ŀ�򷵻�True��������ǿյ�Ҳ����true��
*/
int sqlite3BtreeEof(BtCursor *pCur){
  /* TODO: What if the cursor is in CURSOR_REQUIRESEEK but all table entries
  ** have been deleted? This API will need to change to return an error code
  ** as well as the boolean result value.
  ** ����α���CURSOR_REQUIRESEEK�����б���ɾ��ʲô?���API����Ҫ���ķ���һ����������Լ�����ֵ��
  */
  return (CURSOR_VALID!=pCur->eState);
}

/*
** Advance the cursor to the next entry in the database.  If
** successful then set *pRes=0.  If the cursor
** was already pointing to the last entry in the database before
** this routine was called, then set *pRes=1.
** �ƶ��α굽���ݿ��е���һ��Ŀ������ɹ�����*PRes=0������ڵ����������ʱ�α��Ѿ�ָ�������һ��Ŀ���趨*pRes=1��
*/
int sqlite3BtreeNext(BtCursor *pCur, int *pRes){   //�ƶ��α굽���ݿ��е���һ��Ŀ
  int rc;
  int idx;
  MemPage *pPage;

  assert( cursorHoldsMutex(pCur) );
  rc = restoreCursorPosition(pCur);  
  if( rc!=SQLITE_OK ){
    return rc;
  }
  assert( pRes!=0 );
  if( CURSOR_INVALID==pCur->eState ){
    *pRes = 1;
    return SQLITE_OK;
  }
  if( pCur->skipNext>0 ){
    pCur->skipNext = 0;
    *pRes = 0;
    return SQLITE_OK;
  }
  pCur->skipNext = 0;

  pPage = pCur->apPage[pCur->iPage];
  idx = ++pCur->aiIdx[pCur->iPage];
  assert( pPage->isInit );

  /* If the database file is corrupt, it is possible for the value of idx 
  ** to be invalid here. This can only occur if a second cursor modifies
  ** the page while cursor pCur is holding a reference to it. Which can
  ** only happen if the database is corrupt in such a way as to link the
  ** page into more than one b-tree structure. 
  ** ������ݿ��ļ�����,idx�ļ�ֵ������Ч�ġ����α�pCur����һ������ʱ����ڶ����α��޸�ҳ��,
  ** ���ܻ�����ļ��𺦡�������ҳ�����B���ṹʱ������ݿ��������ķ�ʽ������ô��������ᷢ����*/
  testcase( idx>pPage->nCell );

  pCur->info.nSize = 0;
  pCur->validNKey = 0;
  if( idx>=pPage->nCell ){
    if( !pPage->leaf ){
      rc = moveToChild(pCur, get4byte(&pPage->aData[pPage->hdrOffset+8]));
      if( rc ) return rc;
      rc = moveToLeftmost(pCur);
      *pRes = 0;
      return rc;
    }
    do{
      if( pCur->iPage==0 ){
        *pRes = 1;
        pCur->eState = CURSOR_INVALID;
        return SQLITE_OK;
      }
      moveToParent(pCur);
      pPage = pCur->apPage[pCur->iPage];
    }while( pCur->aiIdx[pCur->iPage]>=pPage->nCell );
    *pRes = 0;
    if( pPage->intKey ){
      rc = sqlite3BtreeNext(pCur, pRes);
    }else{
      rc = SQLITE_OK;
    }
    return rc;
  }
  *pRes = 0;
  if( pPage->leaf ){
    return SQLITE_OK;
  }
  rc = moveToLeftmost(pCur);
  return rc;
}


/*
** Step the cursor to the back to the previous entry in the database.  If
** successful then set *pRes=0.  If the cursor
** was already pointing to the first entry in the database before
** this routine was called, then set *pRes=1.
** ��ʹ�α�ص����ݿ�����ǰ����Ŀ���ɹ�������*pRes=0��
** ������������֮ǰ�Ѿ��Ƶ��˵�һ����Ŀ�� *pRes=1
*/
/*Ѱ�����ݿ�����ǰ����Ŀ*/
int sqlite3BtreePrevious(BtCursor *pCur, int *pRes){   //��ʹ�α�ص����ݿ�����ǰ����Ŀ
  int rc;
  MemPage *pPage;

  assert( cursorHoldsMutex(pCur) );
  rc = restoreCursorPosition(pCur);
  if( rc!=SQLITE_OK ){
    return rc;
  }
  pCur->atLast = 0;
  if( CURSOR_INVALID==pCur->eState ){
    *pRes = 1;
    return SQLITE_OK;
  }
  if( pCur->skipNext<0 ){
    pCur->skipNext = 0;
    *pRes = 0;
    return SQLITE_OK;
  }
  pCur->skipNext = 0;

  pPage = pCur->apPage[pCur->iPage];
  assert( pPage->isInit );
  if( !pPage->leaf ){
    int idx = pCur->aiIdx[pCur->iPage];
    rc = moveToChild(pCur, get4byte(findCell(pPage, idx)));
    if( rc ){
      return rc;
    }
    rc = moveToRightmost(pCur);
  }else{
    while( pCur->aiIdx[pCur->iPage]==0 ){
      if( pCur->iPage==0 ){
        pCur->eState = CURSOR_INVALID;
        *pRes = 1;
        return SQLITE_OK;
      }
      moveToParent(pCur);
    }
    pCur->info.nSize = 0;
    pCur->validNKey = 0;

    pCur->aiIdx[pCur->iPage]--;
    pPage = pCur->apPage[pCur->iPage];
    if( pPage->intKey && !pPage->leaf ){
      rc = sqlite3BtreePrevious(pCur, pRes);
    }else{
      rc = SQLITE_OK;
    }
  }
  *pRes = 0;
  return rc;
}

/*
** Allocate a new page from the database file.
** �����ݿ��ļ�����һ����ҳ�档
** The new page is marked as dirty.  (In other words, sqlite3PagerWrite()
** has already been called on the new page.)  The new page has also
** been referenced and the calling routine is responsible for calling
** sqlite3PagerUnref() on the new page when it is done.
** ��ҳ�����ֱ�ǣ�����sqlite3PagerWrite()�Ѿ�����ҳ�ϱ����á���ɷ���ʱ���µ�ҳҲ������
** ���ҵ��ú�����������ҳ�ϵ���sqlite3PagerUnref().
** SQLITE_OK is returned on success.  Any other return value indicates
** an error.  *ppPage and *pPgno are undefined in the event of an error.
** Do not invoke sqlite3PagerUnref() on *ppPage if an error is returned.
** �ɹ��򷵻�SQLITE_OK�������κη���ֵ��ʾһ��������һ�������¼���* ppPage��* pPgno��δ����ġ�
** �������һ����������*ppPage�ϵ���sqlite3PagerUnref()��
** If the "nearby" parameter is not 0, then a (feeble) effort is made to 
** locate a page close to the page number "nearby".  This can be used in an
** attempt to keep related pages close to each other in the database file,
** which in turn can make database access faster.
** ��� "nearby"��������0,��ô(΢����)Ч���Ƕ�λ�ӽ���ҳ���ҳ��"nearby"����������ڳ���ʹ���ҳ�������ݿ��ļ��б��ֽӽ�,
** ����������ʹ���ݿ�����ٶȸ��졣
** If the "exact" parameter is not 0, and the page-number nearby exists 
** anywhere on the free-list, then it is guarenteed to be returned. This
** is only used by auto-vacuum databases when allocating a new table.
** ���"exact"��������0,����ҳ�븽���κεط��������ڿ����б�,��ô����֤�˷��ء��� ֻʹ����auto-vacuum���ݿ����һ���±�ʱ��
*/
static int allocateBtreePage(           //�����ݿ��ļ�����һ����ҳ�棬�ɹ��򷵻�SQLITE_OK
  BtShared *pBt, 
  MemPage **ppPage, 
  Pgno *pPgno, 
  Pgno nearby,
  u8 exact
){
  MemPage *pPage1;
  int rc;
  u32 n;     /* Number of pages on the freelist */                 //�����б��ϵ�ҳ��
  u32 k;     /* Number of leaves on the trunk of the freelist */   //�����б����ɵ�Ҷ����
  MemPage *pTrunk = 0;
  MemPage *pPrevTrunk = 0;
  Pgno mxPage;     /* Total size of the database file */           //���ݿ��ļ��ܵĴ�С

  assert( sqlite3_mutex_held(pBt->mutex) );
  pPage1 = pBt->pPage1;
  mxPage = btreePagecount(pBt);
  n = get4byte(&pPage1->aData[36]);
  testcase( n==mxPage-1 );
  if( n>=mxPage ){
    return SQLITE_CORRUPT_BKPT;
  }
  if( n>0 ){
    /* There are pages on the freelist.  Reuse one of those pages. */        //�����б�����ҳ������ʹ����Щҳ
    Pgno iTrunk;
    u8 searchList = 0; /* If the free-list must be searched for 'nearby' */  //'nearby'�������������б� 
    
    /* If the 'exact' parameter was true and a query of the pointer-map
    ** shows that the page 'nearby' is somewhere on the free-list, then
    ** the entire-list will be searched for that page.
	** �������'exact'��true����һ��ָ��λͼ��ѯ��ʾҳ'nearby'�ڿ����б��ϵ�ĳ������ô���ڸ�ҳ�����б���Ա�������
    */
#ifndef SQLITE_OMIT_AUTOVACUUM
    if( exact && nearby<=mxPage ){
      u8 eType;
      assert( nearby>0 );
      assert( pBt->autoVacuum );
      rc = ptrmapGet(pBt, nearby, &eType, 0);
      if( rc ) return rc;
      if( eType==PTRMAP_FREEPAGE ){
        searchList = 1;
      }
      *pPgno = nearby;
    }
#endif
    /* Decrement the free-list count by 1. Set iTrunk to the index of the
    ** first free-list trunk page. iPrevTrunk is initially 1.
    ** �ݼ������б�������1.�趨iTrunk����һ�������б�ҳ����ҳ������.iPrevTrunk��ʼ��Ϊ1.*/
    rc = sqlite3PagerWrite(pPage1->pDbPage);
    if( rc ) return rc;
    put4byte(&pPage1->aData[36], n-1);

    /* The code within this loop is run only once if the 'searchList' variable
    ** is not true. Otherwise, it runs once for each trunk-page on the
    ** free-list until the page 'nearby' is located.
    ** �������'searchList'Ϊ�٣���ѭ���ڵĴ���ֻ����һ�Ρ���������ڿ����б��ϵ�ÿ����ҳ�涼����һ��ֱ��ֱ��ҳ��nearby*/
    do {
      pPrevTrunk = pTrunk;
      if( pPrevTrunk ){
        iTrunk = get4byte(&pPrevTrunk->aData[0]);
      }else{
        iTrunk = get4byte(&pPage1->aData[32]);
      }
      testcase( iTrunk==mxPage );
      if( iTrunk>mxPage ){
        rc = SQLITE_CORRUPT_BKPT;
      }else{
        rc = btreeGetPage(pBt, iTrunk, &pTrunk, 0);
      }
      if( rc ){
        pTrunk = 0;
        goto end_allocate_page;
      }
      assert( pTrunk!=0 );
      assert( pTrunk->aData!=0 );

      k = get4byte(&pTrunk->aData[4]); /* # of leaves on this trunk page */  //��ҳ���ϵ�Ҷ����
      if( k==0 && !searchList ){
        /* The trunk has no leaves and the list is not being searched. 
        ** So extract the trunk page itself and use it as the newly 
        ** allocated page 
		** ��ҳ������Ҷ�Ӳ����б��ñ�����.������ȡ��ҳ�汾��������Ϊ�·����ҳ��
		*/
        assert( pPrevTrunk==0 );
        rc = sqlite3PagerWrite(pTrunk->pDbPage);
        if( rc ){
          goto end_allocate_page;
        }
        *pPgno = iTrunk;
        memcpy(&pPage1->aData[32], &pTrunk->aData[0], 4);
        *ppPage = pTrunk;
        pTrunk = 0;
        TRACE(("ALLOCATE: %d trunk - %d free pages left\n", *pPgno, n-1));  //���ٷ����˼���ҳ��ʣ�¼�������ҳ��
      }else if( k>(u32)(pBt->usableSize/4 - 2) ){
        /* Value of k is out of range.  Database corruption */     //kֵ������Χ�����ݿ����
        rc = SQLITE_CORRUPT_BKPT;
        goto end_allocate_page;
#ifndef SQLITE_OMIT_AUTOVACUUM
      }else if( searchList && nearby==iTrunk ){
        /* The list is being searched and this trunk page is the page
        ** to allocate, regardless of whether it has leaves.
		** �б������������������ҳ���Ƿ����ҳ����������ʲôҶ��
       */
        assert( *pPgno==iTrunk );
        *ppPage = pTrunk;
        searchList = 0;
        rc = sqlite3PagerWrite(pTrunk->pDbPage);
        if( rc ){
          goto end_allocate_page;
        }
        if( k==0 ){
          if( !pPrevTrunk ){
            memcpy(&pPage1->aData[32], &pTrunk->aData[0], 4);
          }else{
            rc = sqlite3PagerWrite(pPrevTrunk->pDbPage);
            if( rc!=SQLITE_OK ){
              goto end_allocate_page;
            }
            memcpy(&pPrevTrunk->aData[0], &pTrunk->aData[0], 4);
          }
        }else{
          /* The trunk page is required by the caller but it contains 
          ** pointers to free-list leaves. The first leaf becomes a trunk
          ** page in this case.
		  ** ��ҳ�����ڱ����ú�����Ҫ����������ָ������б�ҳ��ָ�롣����������£���һ��Ҷ�ӱ����ҳ�档
          */
          MemPage *pNewTrunk;
          Pgno iNewTrunk = get4byte(&pTrunk->aData[8]);
          if( iNewTrunk>mxPage ){ 
            rc = SQLITE_CORRUPT_BKPT;
            goto end_allocate_page;
          }
          testcase( iNewTrunk==mxPage );
          rc = btreeGetPage(pBt, iNewTrunk, &pNewTrunk, 0);
          if( rc!=SQLITE_OK ){
            goto end_allocate_page;
          }
          rc = sqlite3PagerWrite(pNewTrunk->pDbPage);
          if( rc!=SQLITE_OK ){
            releasePage(pNewTrunk);
            goto end_allocate_page;
          }
          memcpy(&pNewTrunk->aData[0], &pTrunk->aData[0], 4);
          put4byte(&pNewTrunk->aData[4], k-1);
          memcpy(&pNewTrunk->aData[8], &pTrunk->aData[12], (k-1)*4);
          releasePage(pNewTrunk);
          if( !pPrevTrunk ){
            assert( sqlite3PagerIswriteable(pPage1->pDbPage) );
            put4byte(&pPage1->aData[32], iNewTrunk);
          }else{
            rc = sqlite3PagerWrite(pPrevTrunk->pDbPage);
            if( rc ){
              goto end_allocate_page;
            }
            put4byte(&pPrevTrunk->aData[0], iNewTrunk);
          }
        }
        pTrunk = 0;
        TRACE(("ALLOCATE: %d trunk - %d free pages left\n", *pPgno, n-1));
#endif
      }else if( k>0 ){
        /* Extract a leaf from the trunk */  //����ҳ����ȡ��һ��Ҷ��
        u32 closest;
        Pgno iPage;
        unsigned char *aData = pTrunk->aData;
        if( nearby>0 ){
          u32 i;
          int dist;
          closest = 0;
          dist = sqlite3AbsInt32(get4byte(&aData[8]) - nearby);
          for(i=1; i<k; i++){
            int d2 = sqlite3AbsInt32(get4byte(&aData[8+i*4]) - nearby);
            if( d2<dist ){
              closest = i;
              dist = d2;
            }
          }
        }else{
          closest = 0;
        }

        iPage = get4byte(&aData[8+closest*4]);
        testcase( iPage==mxPage );
        if( iPage>mxPage ){
          rc = SQLITE_CORRUPT_BKPT;
          goto end_allocate_page;
        }
        testcase( iPage==mxPage );
        if( !searchList || iPage==nearby ){
          int noContent;
          *pPgno = iPage;
          TRACE(("ALLOCATE: %d was leaf %d of %d on trunk %d"
                 ": %d more free pages\n",
                 *pPgno, closest+1, k, pTrunk->pgno, n-1));
          rc = sqlite3PagerWrite(pTrunk->pDbPage);
          if( rc ) goto end_allocate_page;
          if( closest<k-1 ){
            memcpy(&aData[8+closest*4], &aData[4+k*4], 4);
          }
          put4byte(&aData[4], k-1);
          noContent = !btreeGetHasContent(pBt, *pPgno);
          rc = btreeGetPage(pBt, *pPgno, ppPage, noContent);
          if( rc==SQLITE_OK ){
            rc = sqlite3PagerWrite((*ppPage)->pDbPage);
            if( rc!=SQLITE_OK ){
              releasePage(*ppPage);
            }
          }
          searchList = 0;
        }
      }
      releasePage(pPrevTrunk);
      pPrevTrunk = 0;
    }while( searchList );
  }else{
    /* There are no pages on the freelist, so create a new page at the
    ** end of the file��
	** �ڿ����б���û��ҳ�棬������ļ���ĩβ������ҳ��
	*/
    rc = sqlite3PagerWrite(pBt->pPage1->pDbPage);
    if( rc ) return rc;
    pBt->nPage++;
    if( pBt->nPage==PENDING_BYTE_PAGE(pBt) ) pBt->nPage++;

#ifndef SQLITE_OMIT_AUTOVACUUM
    if( pBt->autoVacuum && PTRMAP_ISPAGE(pBt, pBt->nPage) ){
      /* If *pPgno refers to a pointer-map page, allocate two new pages
      ** at the end of the file instead of one. The first allocated page
      ** becomes a new pointer-map page, the second is used by the caller.
	  ** ���*pPgnoֵ����ָ��λͼҳ�����ļ�ĩβ����������ҳ���滻������һ�����ָ��λͼҳ���ڶ����������á�
      */
      MemPage *pPg = 0;
      TRACE(("ALLOCATE: %d from end of file (pointer-map page)\n", pBt->nPage));
      assert( pBt->nPage!=PENDING_BYTE_PAGE(pBt) );
      rc = btreeGetPage(pBt, pBt->nPage, &pPg, 1);
      if( rc==SQLITE_OK ){
        rc = sqlite3PagerWrite(pPg->pDbPage);
        releasePage(pPg);
      }
      if( rc ) return rc;
      pBt->nPage++;
      if( pBt->nPage==PENDING_BYTE_PAGE(pBt) ){ pBt->nPage++; }
    }
#endif
    put4byte(28 + (u8*)pBt->pPage1->aData, pBt->nPage);
    *pPgno = pBt->nPage;

    assert( *pPgno!=PENDING_BYTE_PAGE(pBt) );
    rc = btreeGetPage(pBt, *pPgno, ppPage, 1);
    if( rc ) return rc;
    rc = sqlite3PagerWrite((*ppPage)->pDbPage);
    if( rc!=SQLITE_OK ){
      releasePage(*ppPage);
    }
    TRACE(("ALLOCATE: %d from end of file\n", *pPgno));
  }

  assert( *pPgno!=PENDING_BYTE_PAGE(pBt) );

end_allocate_page:
  releasePage(pTrunk);
  releasePage(pPrevTrunk);
  if( rc==SQLITE_OK ){
    if( sqlite3PagerPageRefcount((*ppPage)->pDbPage)>1 ){
      releasePage(*ppPage);
      return SQLITE_CORRUPT_BKPT;
    }
    (*ppPage)->isInit = 0;
  }else{
    *ppPage = 0;
  }
  assert( rc!=SQLITE_OK || sqlite3PagerIswriteable((*ppPage)->pDbPage) );
  return rc;
}

/*
** This function is used to add page iPage to the database file free-list. 
** It is assumed that the page is not already a part of the free-list.
** ��������������ҳ��iPage�����ݿ��ļ������б��ٶ�ҳ�治�ǿ����б��һ���֡�
** The value passed as the second argument to this function is optional.
** If the caller happens to have a pointer to the MemPage object 
** corresponding to page iPage handy, it may pass it as the second value. 
** Otherwise, it may pass NULL.
** ��Ϊ�ڶ����������ݸ��ú�����ֵ�ǿ�ѡ��.���������������һ��ָ��ָ��MemPage�����Ӧ iPageҳ��,
** �����ܰ�����Ϊ�ڶ���ֵ.����,������Ϊ�ա�
** If a pointer to a MemPage object is passed as the second argument,
** its reference count is not altered by this function.
** ���һ��ָ��MemPage������Ϊ�ڶ�����������,��ô�����������ᱻ��������ı䡣
*/ 
static int freePage2(BtShared *pBt, MemPage *pMemPage, Pgno iPage){       //���ҳ��iPage�����ݿ��ļ������б�
  MemPage *pTrunk = 0;                /* Free-list trunk page */                 //�����б�ҳ����ҳ��
  Pgno iTrunk = 0;                    /* Page number of free-list trunk page */  //�����б�ҳ����ҳ���ҳ��
  MemPage *pPage1 = pBt->pPage1;      /* Local reference to page 1 */            //�ڴ�����ҳ1
  MemPage *pPage;                     /* Page being freed. May be NULL. */       //ҳ���ͷţ������ǿ�
  int rc;                             /* Return Code */                          //���ش���
  int nFree;                          /* Initial number of pages on free-list */ //�����б�ҳ�������ҳ����

  assert( sqlite3_mutex_held(pBt->mutex) );
  assert( iPage>1 );
  assert( !pMemPage || pMemPage->pgno==iPage );

  if( pMemPage ){
    pPage = pMemPage;
    sqlite3PagerRef(pPage->pDbPage);
  }else{
    pPage = btreePageLookup(pBt, iPage);
  }

  /* Increment the free page count on pPage1 */    //����pPage1�ϵĿ���ҳ������
  rc = sqlite3PagerWrite(pPage1->pDbPage);
  if( rc ) goto freepage_out;                     //���rcֵΪ0ת��freepage_out
  nFree = get4byte(&pPage1->aData[36]);
  put4byte(&pPage1->aData[36], nFree+1);

  if( pBt->btsFlags & BTS_SECURE_DELETE ){
    /* If the secure_delete option is enabled, then
    ** always fully overwrite deleted information with zeros.
	** ���secure_deleteѡ����ã���ô������ȫ��дɾ����ϢΪ0.
    */
    if( (!pPage && ((rc = btreeGetPage(pBt, iPage, &pPage, 0))!=0) )
     ||            ((rc = sqlite3PagerWrite(pPage->pDbPage))!=0)
    ){
      goto freepage_out;
    }
    memset(pPage->aData, 0, pPage->pBt->pageSize);
  }

  /* If the database supports auto-vacuum, write an entry in the pointer-map
  ** to indicate that the page is free.
  ** ������ݿ�֧���Զ�����дһ����Ŀ��ָ��λͼ������Ҳ�ǿ��еġ�
  */
  if( ISAUTOVACUUM ){
    ptrmapPut(pBt, iPage, PTRMAP_FREEPAGE, 0, &rc);
    if( rc ) goto freepage_out;
  }

  /* Now manipulate the actual database free-list structure. There are two
  ** possibilities. If the free-list is currently empty, or if the first
  ** trunk page in the free-list is full, then this page will become a
  ** new free-list trunk page. Otherwise, it will become a leaf of the
  ** first trunk page in the current free-list. This block tests if it
  ** is possible to add the page as a new free-list leaf.
  ** ���ڲ�����ʵ�����ݿ�����б�ṹ���������Ŀ����ԡ���������б�ǰΪ��,����������б�
  ** �ϵĵ�һ��ҳ��������,��ô�⽫��Ϊһ��ҳ���µĿ����б����ҳ�档����,������Ϊ��ǰ������
  ** �еĵ�һ����ҳ���һ��Ҷ�ӡ�������������ҳ����Ϊһ���µĿ����б��Ҷ������������ԡ�
  */
  if( nFree!=0 ){
    u32 nLeaf;                /* Initial number of leaf cells on trunk page */  //��ҳ���������Ҷ�ӵ�Ԫ������

    iTrunk = get4byte(&pPage1->aData[32]);
    rc = btreeGetPage(pBt, iTrunk, &pTrunk, 0);
    if( rc!=SQLITE_OK ){
      goto freepage_out;
    }

    nLeaf = get4byte(&pTrunk->aData[4]);
    assert( pBt->usableSize>32 );
    if( nLeaf > (u32)pBt->usableSize/4 - 2 ){
      rc = SQLITE_CORRUPT_BKPT;
      goto freepage_out;
    }
    if( nLeaf < (u32)pBt->usableSize/4 - 8 ){
      /* In this case there is room on the trunk page to insert the page
      ** being freed as a new leaf.
      ** �����������,����ҳ�����пռ���뱻�ͷŵ�ҳ������Ҷ�ӡ�
      ** Note that the trunk page is not really full until it contains
      ** usableSize/4 - 2 entries, not usableSize/4 - 8 entries as we have
      ** coded.  But due to a coding error in versions of SQLite prior to
      ** 3.6.0, databases with freelist trunk pages holding more than
      ** usableSize/4 - 8 entries will be reported as corrupt.  In order
      ** to maintain backwards compatibility with older versions of SQLite,
      ** we will continue to restrict the number of entries to usableSize/4 - 8
      ** for now.  At some point in the future (once everyone has upgraded
      ** to 3.6.0 or later) we should consider fixing the conditional above
      ** to read "usableSize/4-2" instead of "usableSize/4-8".
	  ** ע��,��ҳ�治����������ֱ��������(usableSize/4-2)����Ŀ,�����ǣ�usableSize/4-8)����Ŀ��
	  ** ������֮ǰ�汾3.6.0��SQLite�ı�������п����б���ҳ������ݿ��ж��ڣ�usableSize/4-8��
	  ** ����Ŀ�����������Ϊ���ϰ汾��SQLite�����������ԣ�������������Ŀ������usableSize/4-8��
	  ** �ڽ�����ĳ��ʱ��(ÿ���˶���һ������3.6.0��֮��)����Ӧ�ÿ��ǽ���������������usableSize/4-2��,
	  ** �����ǡ�usableSize/4-8����
      */
      rc = sqlite3PagerWrite(pTrunk->pDbPage);
      if( rc==SQLITE_OK ){
        put4byte(&pTrunk->aData[4], nLeaf+1);
        put4byte(&pTrunk->aData[8+nLeaf*4], iPage);
        if( pPage && (pBt->btsFlags & BTS_SECURE_DELETE)==0 ){
          sqlite3PagerDontWrite(pPage->pDbPage);
        }
        rc = btreeSetHasContent(pBt, iPage);
      }
      TRACE(("FREE-PAGE: %d leaf on trunk page %d\n",pPage->pgno,pTrunk->pgno));
      goto freepage_out;
    }
  }

  /* If control flows to this point, then it was not possible to add the
  ** the page being freed as a leaf page of the first trunk in the free-list.
  ** Possibly because the free-list is empty, or possibly because the 
  ** first trunk in the free-list is full. Either way, the page being freed
  ** will become the new first trunk page in the free-list.
  ** ����������ﵽ��һ��,��ô���ǲ����ܵ���ӱ��ͷŵ�ҳ���Ϊ�����б��е���ҳ��ĵ�һ��Ҷ��ҳ�档
  ** ��������Ϊ�����б��ǿյ�,���������Ϊ�����б��еĵ�һ����ҳ���Ѿ����ˡ��������ַ�ʽ,ҳ�汻�ͷ�
  ** ����Ϊ�µĿ����б��еĵ�һ����ҳ�档
  */
  if( pPage==0 && SQLITE_OK!=(rc = btreeGetPage(pBt, iPage, &pPage, 0)) ){
    goto freepage_out;
  }
  rc = sqlite3PagerWrite(pPage->pDbPage);
  if( rc!=SQLITE_OK ){
    goto freepage_out;
  }
  put4byte(pPage->aData, iTrunk);
  put4byte(&pPage->aData[4], 0);
  put4byte(&pPage1->aData[32], iPage);
  TRACE(("FREE-PAGE: %d new trunk page replacing %d\n", pPage->pgno, iTrunk));

freepage_out:
  if( pPage ){
    pPage->isInit = 0;
  }
  releasePage(pPage);
  releasePage(pTrunk);
  return rc;
}
static void freePage(MemPage *pPage, int *pRC){
  if( (*pRC)==SQLITE_OK ){
    *pRC = freePage2(pPage->pBt, pPage, pPage->pgno);
  }
}

/*Free any overflow pages associated with the given Cell.*/   
static int clearCell(MemPage *pPage, unsigned char *pCell){     //�ͷ��κ��������Ԫ��ص����ҳ
  BtShared *pBt = pPage->pBt;
  CellInfo info;
  Pgno ovflPgno;
  int rc;
  int nOvfl;
  u32 ovflPageSize;

  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  btreeParseCellPtr(pPage, pCell, &info);
  if( info.iOverflow==0 ){
    return SQLITE_OK;  /* No overflow pages. Return without doing anything */   //û�����ҳ��������������
  }
  if( pCell+info.iOverflow+3 > pPage->aData+pPage->maskPage ){
    return SQLITE_CORRUPT;  /* Cell extends past end of page */  //��Ԫ������ҳ��ķ�Χ
  }
  ovflPgno = get4byte(&pCell[info.iOverflow]);
  assert( pBt->usableSize > 4 );
  ovflPageSize = pBt->usableSize - 4;
  nOvfl = (info.nPayload - info.nLocal + ovflPageSize - 1)/ovflPageSize;
  assert( ovflPgno==0 || nOvfl>0 );
  while( nOvfl-- ){
    Pgno iNext = 0;
    MemPage *pOvfl = 0;
    if( ovflPgno<2 || ovflPgno>btreePagecount(pBt) ){
      /* 0 is not a legal page number and page 1 cannot be an 
      ** overflow page. Therefore if ovflPgno<2 or past the end of the 
      ** file the database must be corrupt.
	  0���ǺϷ���ҳ�벢��1�����������ҳ����ˣ����ovflPgno<2���߳������ݿ��ļ�һ������SQLITE_CORRUPT_BKPT*/
      return SQLITE_CORRUPT_BKPT;
    }
    if( nOvfl ){
      rc = getOverflowPage(pBt, ovflPgno, &pOvfl, &iNext);
      if( rc ) return rc;
    }

    if( ( pOvfl || ((pOvfl = btreePageLookup(pBt, ovflPgno))!=0) )
     && sqlite3PagerPageRefcount(pOvfl->pDbPage)!=1
    ){
      /* There is no reason any cursor should have an outstanding reference 
      ** to an overflow page belonging to a cell that is being deleted/updated.
      ** So if there exists more than one reference to this page, then it 
      ** must not really be an overflow page and the database must be corrupt. 
      ** It is helpful to detect this before calling freePage2(), as 
      ** freePage2() may zero the page contents if secure-delete mode is
      ** enabled. If this 'overflow' page happens to be a page that the
      ** caller is iterating through or using in some other way, this
      ** can be problematic.
	  ** ���������κ��α�Ӧ����һͻ����������������ɾ��/���µĵ�Ԫ�����ҳ��������ڶ����ҳ��������,��ô��
	  ** ��һ����һ��������ҳ������ݿ�һ���������������ڵ���freePage2()֮ǰ�������.�����ȫɾ��ģʽ���ã�
	  ** freePage2()������ҳ�����ݡ�����⡰�����ҳ����һ�����ñ�������������ʽʹ�õ�ҳ��,�������������ġ�
      */
      rc = SQLITE_CORRUPT_BKPT;
    }else{
      rc = freePage2(pBt, pOvfl, ovflPgno);
    }

    if( pOvfl ){
      sqlite3PagerUnref(pOvfl->pDbPage);
    }
    if( rc ) return rc;
    ovflPgno = iNext;
  }
  return SQLITE_OK;
}

/*
** Create the byte sequence used to represent a cell on page pPage
** and write that byte sequence into pCell[].  Overflow pages are
** allocated and filled in as necessary.  The calling procedure
** is responsible for making sure sufficient space has been allocated
** for pCell[].
** �����ֽ�������������һ��pPageҳ�ϵĵ�Ԫ�����ֽ�����д��pCell[]�����ҳ��
** ���������ڱ�Ҫʱ��д�����ó�����ȷ���㹻�Ŀռ����ΪpCell[]��
** Note that pCell does not necessary need to point to the pPage->aData
** area.  pCell might point to some temporary storage.  The cell will
** be constructed in this temporary area then copied into pPage->aData
** later.
** ע��,pCell������Ҫ��Ҫָ��pPage->aData����pCell����ָ��һЩ��ʱ�洢����
** ��Ԫ���������ʱ���򱻴���Ȼ���Ƶ�pPage->aData��
*/
/*�����ֽ�����д��pCell*/
static int fillInCell(     //�����ֽ�������������һ��pPageҳ�ϵĵ�Ԫ�����ֽ�����д��pCell[]
  MemPage *pPage,                /* The page that contains the cell */     //�����õ�Ԫ��ҳ
  unsigned char *pCell,          /* Complete text of the cell */           //��Ԫ�������ı�
  const void *pKey, i64 nKey,    /* The key */                             //�ؼ���
  const void *pData,int nData,   /* The data */                            //������
  int nZero,                     /* Extra zero bytes to append to pData */ //������pData�ϵĶ���0�ֽ�
  int *pnSize                    /* Write cell size here */                //����Ԫ�Ĵ�Сд���ñ���
){
  int nPayload;
  const u8 *pSrc;
  int nSrc, n, rc;
  int spaceLeft;
  MemPage *pOvfl = 0;
  MemPage *pToRelease = 0;
  unsigned char *pPrior;
  unsigned char *pPayload;
  BtShared *pBt = pPage->pBt;
  Pgno pgnoOvfl = 0;
  int nHeader;
  CellInfo info;

  assert( sqlite3_mutex_held(pPage->pBt->mutex) );

  /* pPage is not necessarily writeable since pCell might be auxiliary
  ** buffer space that is separate from the pPage buffer area 
  ** pPage��һ���ǿ�д����ΪpCell�����Ǵ�pPage�������ֳ��ĸ����������ռ�.*/
  assert( pCell<pPage->aData || pCell>=&pPage->aData[pBt->pageSize]
            || sqlite3PagerIswriteable(pPage->pDbPage) );

  /* Fill in the header. */     //���ͷ��Ϣ
  nHeader = 0;
  if( !pPage->leaf ){
    nHeader += 4;
  }
  if( pPage->hasData ){
    nHeader += putVarint(&pCell[nHeader], nData+nZero);
  }else{
    nData = nZero = 0;
  }
  nHeader += putVarint(&pCell[nHeader], *(u64*)&nKey);
  btreeParseCellPtr(pPage, pCell, &info);              //������Ԫ���ݿ飬����CellInfo�ṹ��
  assert( info.nHeader==nHeader );
  assert( info.nKey==nKey );
  assert( info.nData==(u32)(nData+nZero) );
  
  /* Fill in the payload */      //��Ӽ�¼
  nPayload = nData + nZero;
  if( pPage->intKey ){
    pSrc = pData;
    nSrc = nData;
    nData = 0;
  }else{ 
    if( NEVER(nKey>0x7fffffff || pKey==0) ){
      return SQLITE_CORRUPT_BKPT;
    }
    nPayload += (int)nKey;
    pSrc = pKey;
    nSrc = (int)nKey;
  }
  *pnSize = info.nSize;
  spaceLeft = info.nLocal;
  pPayload = &pCell[nHeader];
  pPrior = &pCell[info.iOverflow];

  while( nPayload>0 ){
    if( spaceLeft==0 ){
#ifndef SQLITE_OMIT_AUTOVACUUM
      Pgno pgnoPtrmap = pgnoOvfl; /* Overflow page pointer-map entry page */  //���ҳλͼָ����Ŀҳ
      if( pBt->autoVacuum ){
        do{
          pgnoOvfl++;
        } while( 
          PTRMAP_ISPAGE(pBt, pgnoOvfl) || pgnoOvfl==PENDING_BYTE_PAGE(pBt) 
        );
      }
#endif
      rc = allocateBtreePage(pBt, &pOvfl, &pgnoOvfl, pgnoOvfl, 0);   //�����ݿ��ļ�����һ����ҳ�棬�ɹ��򷵻�SQLITE_OK
#ifndef SQLITE_OMIT_AUTOVACUUM
      /* If the database supports auto-vacuum, and the second or subsequent
      ** overflow page is being allocated, add an entry to the pointer-map
      ** for that page now. 
      ** ������ݿ�֧���Զ������ҵڶ������̵����ҳ�����䣬�Ը�ҳ����Ŀ��ָ��λͼ.
      ** If this is the first overflow page, then write a partial entry 
      ** to the pointer-map. If we write nothing to this pointer-map slot,
      ** then the optimistic overflow chain processing in clearCell()
      ** may misinterpret the uninitialised values and delete the
      ** wrong pages from the database.
	  ** ������ǵ�һ�����ҳ����ôдһ���ֲ�ҳ��Ŀ��ָ��λͼ.�����д��ָ��λͼλ�ã���ô
	  ** �͹���˵��clearCell()�д����������ӽ���Ū��δ��ʼ����ֵ���ҽ������ݿ���ɾ������ҳ.
      */
      if( pBt->autoVacuum && rc==SQLITE_OK ){
        u8 eType = (pgnoPtrmap?PTRMAP_OVERFLOW2:PTRMAP_OVERFLOW1);
        ptrmapPut(pBt, pgnoOvfl, eType, pgnoPtrmap, &rc);
        if( rc ){
          releasePage(pOvfl);
        }
      }
#endif
      if( rc ){
        releasePage(pToRelease);  //�ͷ��ڴ�ҳ
        return rc;
      }

      /* If pToRelease is not zero than pPrior points into the data area
      ** of pToRelease.  Make sure pToRelease is still writeable. 
	  ** ���pToReleaseһ��Ϊ0��pPrior��ָ��pToRelease��������.ȷ��pToRelease�ǿ�д��. */
      assert( pToRelease==0 || sqlite3PagerIswriteable(pToRelease->pDbPage) );

      /* If pPrior is part of the data area of pPage, then make sure pPage
      ** is still writeable 
	  ** ���pPrior��pPage�������һ���֣���ôȷ��pPage��Ȼ��д. */
      assert( pPrior<pPage->aData || pPrior>=&pPage->aData[pBt->pageSize]
            || sqlite3PagerIswriteable(pPage->pDbPage) );

      put4byte(pPrior, pgnoOvfl);
      releasePage(pToRelease);
      pToRelease = pOvfl;
      pPrior = pOvfl->aData;
      put4byte(pPrior, 0);
      pPayload = &pOvfl->aData[4];
      spaceLeft = pBt->usableSize - 4;
    }
    n = nPayload;
    if( n>spaceLeft ) n = spaceLeft;

    /* If pToRelease is not zero than pPayload points into the data area
    ** of pToRelease.  Make sure pToRelease is still writeable.
	** ���pToReleaseһ��Ϊ0��pPrior��ָ��pToRelease��������.ȷ��pToRelease�ǿ�д��.  */
    assert( pToRelease==0 || sqlite3PagerIswriteable(pToRelease->pDbPage) );

    /* If pPayload is part of the data area of pPage, then make sure pPage
    ** is still writeable 
	** ���pPrior��pPage�������һ���֣���ôȷ��pPage��Ȼ��д.*/
    assert( pPayload<pPage->aData || pPayload>=&pPage->aData[pBt->pageSize]
            || sqlite3PagerIswriteable(pPage->pDbPage) );

    if( nSrc>0 ){
      if( n>nSrc ) n = nSrc;
      assert( pSrc );
      memcpy(pPayload, pSrc, n);
    }else{
      memset(pPayload, 0, n);
    }
    nPayload -= n;
    pPayload += n;
    pSrc += n;
    nSrc -= n;
    spaceLeft -= n;
    if( nSrc==0 ){
      nSrc = nData;
      pSrc = pData;
    }
  }
  releasePage(pToRelease);  //�ͷ��ڴ�ҳ
  return SQLITE_OK;
}

/*
** Remove the i-th cell from pPage.  This routine effects pPage only.
** The cell content is not freed or deallocated.  It is assumed that
** the cell content has been copied someplace else.  This routine just
** removes the reference to the cell from pPage.
** ɾ��pPage�ĵ�i����Ԫ.�������������pPage������.��Ԫ�����ݲ����ͷŻ���ʧ.
** �ٶ���Ԫ�������Ѿ������������ط�.���������ֻpPage�е�Ԫ������.
** "sz" must be the number of bytes in the cell. //����sz�ǵ�Ԫ���ֽ���.
*/
static void dropCell(MemPage *pPage, int idx, int sz, int *pRC){      //ɾ��pPage�ĵ�i����Ԫ.
  u32 pc;         /* Offset to cell content of cell being deleted */  //Ҫ��ɾ���ĵ�Ԫ���ݵ�ƫ����
  u8 *data;       /* pPage->aData */                                  //pPage->aData������
  u8 *ptr;        /* Used to move bytes around within data[] */       //��data[]�������ƶ��ֽ�
  u8 *endPtr;     /* End of loop */                                   //ѭ������
  int rc;         /* The return code */                               //���ش���
  int hdr;        /* Beginning of the header.  0 most pages.  100 page 1 */   //ͷ���Ŀ�ʼ��Ϊ0������ҳ��Ϊ1 �ǵ�һҳ

  if( *pRC ) return;

  assert( idx>=0 && idx<pPage->nCell );
  assert( sz==cellSize(pPage, idx) );
  assert( sqlite3PagerIswriteable(pPage->pDbPage) );
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  data = pPage->aData;
  ptr = &pPage->aCellIdx[2*idx];
  pc = get2byte(ptr);
  hdr = pPage->hdrOffset;
  testcase( pc==get2byte(&data[hdr+5]) );
  testcase( pc+sz==pPage->pBt->usableSize );
  if( pc < (u32)get2byte(&data[hdr+5]) || pc+sz > pPage->pBt->usableSize ){
    *pRC = SQLITE_CORRUPT_BKPT;
    return;
  }
  rc = freeSpace(pPage, pc, sz);   //�ͷ�pPage->aData�Ĳ��ֲ�д������б�
  if( rc ){
    *pRC = rc;
    return;
  }
  endPtr = &pPage->aCellIdx[2*pPage->nCell - 2];
  assert( (SQLITE_PTR_TO_INT(ptr)&1)==0 );  /* ptr is always 2-byte aligned */  //ptr�������ֽ�
  while( ptr<endPtr ){
    *(u16*)ptr = *(u16*)&ptr[2];
    ptr += 2;
  }
  pPage->nCell--;
  put2byte(&data[hdr+3], pPage->nCell);
  pPage->nFree += 2;
}

/*
** Insert a new cell on pPage at cell index "i".  pCell points to the
** content of the cell.
** ��pPage�ĵ�Ԫ����i������һ���µ�Ԫ.pCellָ��Ԫ������.
** If the cell content will fit on the page, then put it there.  If it
** will not fit, then make a copy of the cell content into pTemp if
** pTemp is not null.  Regardless of pTemp, allocate a new entry
** in pPage->apOvfl[] and make it point to the cell content (either
** in pTemp or the original pCell) and also record its index. 
** Allocating a new entry in pPage->aCell[] implies that 
** pPage->nOverflow is incremented.
** �����Ԫ������ҳ���ʺϣ��򽫷��ڴ˴�.������ʺϣ���ô���pTemp�ǿտ�����Ԫ�����ݵ�pTemp.
** ����pTemp����pPage->apOvfl[]�з���һ������Ŀ��������ָ��Ԫ����(������pTemp������
** ԭ�е�pCell)Ҳ��¼����������pPage->aCell[]�з���һ���µ���Ŀʵ��pPage->nOverflow������.
** If nSkip is non-zero, then do not copy the first nSkip bytes of the
** cell. The caller will overwrite them after this function returns. If
** nSkip is zero, then pCell may not point to an invalid memory location 
** (but pCell+nSkip is always valid).
** ���nSkip�Ƿ����,��ô��Ҫ���Ƶ�Ԫ�ĵ�һ��nSkip�ֽڡ�����������غ�����߽��������ǡ�
** ���nSkip����,��ôpCell����ָ��һ����Ч���ڴ�λ��(��pCell + nSkip������Ч)��
*/
/*��ҳ�ĵ�i����Ԫ���в���һ����Ԫ��*/
static void insertCell(             //��pPage�ĵ�Ԫ����i������һ���µ�Ԫ
  MemPage *pPage,   /* Page into which we are copying */                      //��ſ������ݵ�ҳ
  int i,            /* New cell becomes the i-th cell of the page */          //�µ�Ԫ����Ϊҳ�ĵ�i����Ԫ
  u8 *pCell,        /* Content of the new cell */                             //�µ�Ԫ������
  int sz,           /* Bytes of content in pCell */                           //pCell�����ݵ��ֽ�
  u8 *pTemp,        /* Temp storage space for pCell, if needed */             //�����Ҫ��������pCell����ʱ�洢�ռ�
  Pgno iChild,      /* If non-zero, replace first 4 bytes with this value */  //�������滻���ֵ�Ŀ�ʼ��4���ֽ�.
  int *pRC          /* Read and write return code from here */                //�������д�����ֽ�
){
  int idx = 0;      /* Where to write new cell content in data[] */           //��data[]��д�µ�Ԫ������
  int j;            /* Loop counter */                                        //ѭ������
  int end;          /* First byte past the last cell pointer in data[] */     //data[]�����һ����Ԫ��ĵ�һ���ֽ�
  int ins;          /* Index in data[] where new cell pointer is inserted */  //data[]�н�Ҫ�����µ�Ԫ�ط�������
  int cellOffset;   /* Address of first cell pointer in data[] */             //data[]�е�һ����Ԫָ��ĵ�ַ
  u8 *data;         /* The content of the whole page */                       //����ҳ������
  u8 *ptr;          /* Used for moving information around in data[] */        //data[]�������ƶ���Ϣ
  u8 *endPtr;       /* End of the loop */                                     //ѭ���Ľ�β

  int nSkip = (iChild ? 4 : 0);

  if( *pRC ) return;

  assert( i>=0 && i<=pPage->nCell+pPage->nOverflow );
  assert( pPage->nCell<=MX_CELL(pPage->pBt) && MX_CELL(pPage->pBt)<=10921 );
  assert( pPage->nOverflow<=ArraySize(pPage->apOvfl) );
  assert( ArraySize(pPage->apOvfl)==ArraySize(pPage->aiOvfl) );
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  /* The cell should normally be sized correctly.  However, when moving a
  ** malformed(���ε�) cell from a leaf page to an interior page, if the cell size
  ** wanted to be less than 4 but got rounded up to(�㵽) 4 on the leaf, then size
  ** might be less than 8 (leaf-size + pointer) on the interior node.  Hence
  ** the term after the || in the following assert(). 
  ** ��Ԫ��Сͨ��Ӧ��ȷ��Ȼ��,���ƶ����ε�Ԫ��һƬҶ��ҳ���ڲ�ҳ,�����Ԫ�Ĵ�С����4����Ҷ���㵽��4,
  ** ��ô�ڲ��ڵ��ϴ�С��������8(leaf-size + pointer)��*/
  assert( sz==cellSizePtr(pPage, pCell) || (sz==8 && iChild>0) );
  if( pPage->nOverflow || sz+2>pPage->nFree ){
    if( pTemp ){
      memcpy(pTemp+nSkip, pCell+nSkip, sz-nSkip);
      pCell = pTemp;
    }
    if( iChild ){
      put4byte(pCell, iChild);
    }
    j = pPage->nOverflow++;
    assert( j<(int)(sizeof(pPage->apOvfl)/sizeof(pPage->apOvfl[0])) );
    pPage->apOvfl[j] = pCell;
    pPage->aiOvfl[j] = (u16)i;
  }else{
    int rc = sqlite3PagerWrite(pPage->pDbPage);
    if( rc!=SQLITE_OK ){
      *pRC = rc;
      return;
    }
    assert( sqlite3PagerIswriteable(pPage->pDbPage) );
    data = pPage->aData;
    cellOffset = pPage->cellOffset;
    end = cellOffset + 2*pPage->nCell;
    ins = cellOffset + 2*i;
    rc = allocateSpace(pPage, sz, &idx);/*��pPage�Ϸ���sz�ֽڵĿռ䣬������д��idx��*/
    if( rc ){ *pRC = rc; return; }
    /* The allocateSpace() routine guarantees the following two properties
    ** if it returns success */
    assert( idx >= end+2 );
    assert( idx+sz <= (int)pPage->pBt->usableSize );
    pPage->nCell++;
    pPage->nFree -= (u16)(2 + sz);
    memcpy(&data[idx+nSkip], pCell+nSkip, sz-nSkip);/*��pCell�����ݿ�����data*/
    if( iChild ){
      put4byte(&data[idx], iChild);
    }
    ptr = &data[end];
    endPtr = &data[ins];
    assert( (SQLITE_PTR_TO_INT(ptr)&1)==0 );  /* ptr is always 2-byte aligned */
    while( ptr>endPtr ){
      *(u16*)ptr = *(u16*)&ptr[-2];
      ptr -= 2;
    }
    put2byte(&data[ins], idx);
    put2byte(&data[pPage->hdrOffset+3], pPage->nCell);
#ifndef SQLITE_OMIT_AUTOVACUUM
    if( pPage->pBt->autoVacuum ){
      /* The cell may contain a pointer to an overflow page. If so, write
      ** the entry for the overflow page into the pointer map.
      ** ��Ԫ���ܰ��������ҳ��ָ��.�����������������ҳд��Ŀ��ָ��λͼ*/
      ptrmapPutOvflPtr(pPage, pCell, pRC);
    }
#endif
  }
}

/*
** Add a list of cells to a page.  The page should be initially empty.
** The cells are guaranteed to fit on the page.
*/
/*
���һ��ҳ�ϵĵ�Ԫ�񡣸�ҳ��Ӧ�������Ϊ�ա�ȷ����Ԫ���ʺ�ҳ��
*/

static void assemblePage(        //��ҳ����ӵ�Ԫ�б�
  MemPage *pPage,   /* The page to be assemblied */                   //װ��ҳ
  int nCell,        /* The number of cells to add to this page */     //��ӵ�ҳ�ϵĵ�Ԫ��
  u8 **apCell,      /* Pointers to cell bodies */                     //��Ԫ���ָ��
  u16 *aSize        /* Sizes of the cells */                          //��Ԫ�ô�С
){
  int i;            /* Loop counter */                                //ѭ����������
  u8 *pCellptr;     /* Address of next cell pointer */                //��һ��Ԫ��ָ���ַ
  int cellbody;     /* Address of next cell body */                   //��һ����Ԫ��ĵ�ַ
  u8 * const data = pPage->aData;             /* Pointer to data for pPage */     //ҳ�����ݵ�ָ��
  const int hdr = pPage->hdrOffset;           /* Offset of header on pPage */     //ҳ��ͷ����ƫ����
  const int nUsable = pPage->pBt->usableSize; /* Usable size of page */           //����ҳ�Ĵ�С

  assert( pPage->nOverflow==0 );
  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  assert( nCell>=0 && nCell<=(int)MX_CELL(pPage->pBt)  //��ӵĵ�Ԫ������0����С����������Ԫ����ҳ�ϵ����Ԫ��<=10921
            && (int)MX_CELL(pPage->pBt)<=10921);
  assert( sqlite3PagerIswriteable(pPage->pDbPage) );

  /* Check that the page has just been zeroed by zeroPage() */  //���ҳ�Ƿ��Ѿ���zeroPage()����.
  assert( pPage->nCell==0 );
  assert( get2byteNotZero(&data[hdr+5])==nUsable );

  pCellptr = &pPage->aCellIdx[nCell*2];
  cellbody = nUsable;
  for(i=nCell-1; i>=0; i--){
    u16 sz = aSize[i];
    pCellptr -= 2;
    cellbody -= sz;
    put2byte(pCellptr, cellbody);
    memcpy(&data[cellbody], apCell[i], sz);     //��apCell[i]����ʼλ�ÿ�ʼ����sz���ֽڵ�&data[cellbody]����ʼλ����
  }
  put2byte(&data[hdr+3], nCell);
  put2byte(&data[hdr+5], cellbody);
  pPage->nFree -= (nCell*2 + nUsable - cellbody);
  pPage->nCell = (u16)nCell;
}

/*
** The following parameters determine how many adjacent pages get involved
** in a balancing operation.  NN is the number of neighbors on either side
** of the page that participate in the balancing operation.  NB is the
** total number of pages that participate, including the target page and
** NN neighbors on either side.
** ���²���ȷ����һ��ƽ��������ж�������ҳ����������NN�ǲ���ƽ�������ҳ������ҳ���������
** NB�����漰��ҳ������,����Ŀ��ҳ���NN������ҳ�档
** The minimum value of NN is 1 (of course).  Increasing NN above 1
** (to 2 or 3) gives a modest improvement in SELECT and DELETE performance
** in exchange for a larger degradation in INSERT and UPDATE performance.
** The value of NN appears to give the best results overall.
** NN����Сֵ��1������NNʹ֮����1(2��3)�ܹ�����SELECT��DELETE����,�Ի�ȡ����Ĳ���͸������ܵ��˻���
** NNֵ�ƺ�������õĽ����
*/
/*����Ĳ���ȷ����ƽ����������漰�������ڵ�ҳ�棬������ΪNN��NB�ǲ����ҳ����������
NN����Сֵ��1������NN��1���ϣ�2��3)�� �ܹ�����SELECT��DELETE���ܡ�
*/


#define NN 1             /* Number of neighbors on either side of pPage */   //pPage�������ڵ�ҳ��
#define NB (NN*2+1)      /* Total pages involved in the balance */           //��ƽ�����漰����ҳ��


#ifndef SQLITE_OMIT_QUICKBALANCE
/*
** This version of balance() handles the common special case where
** a new entry is being inserted on the extreme right-end of the
** tree, in other words, when the new entry will become the largest
** entry in the tree.
** ���balance()�汾�����������������һ������Ŀ�����뵽�������Ҷ�.
** ���仰˵,������Ŀ����Ϊ����������Ŀ��
** Instead of trying to balance the 3 right-most leaf pages, just add
** a new page to the right-hand side and put the one new entry in
** that page.  This leaves the right side of the tree somewhat
** unbalanced.  But odds are that we will be inserting new entries
** at the end soon afterwards so the nearly empty page will quickly
** fill up.  On average.
** ��������ͼƽ�����ұߵ�3��Ҷҳ��,���һ����ҳ����ұ�,��һ������Ŀ�����ҳ���С�
** ��ʹ�������ұ߲�ƽ��ġ�����ֵ���,���ǽ������µ���Ŀ��������Ժܿ콫��ҳ��������
** pPage is the leaf page which is the right-most page in the tree.
** pParent is its parent.  pPage must have a single overflow entry
** which is also the right-most entry on the page.
** pPage��Ҷ��ҳ�棬�����������ұߵ�ҳ�档pParent�����ĸ��ڵ㡣pPage�����е����������ĿҲҳ�������ұߵ���Ŀ��
** The pSpace buffer is used to store a temporary copy of the divider
** cell that will be inserted into pParent. Such a cell consists of a 4
** byte page number followed by a variable length integer. In other
** words, at most 13 bytes. Hence the pSpace buffer must be at
** least 13 bytes in size.
** pSpace���������ڴ洢������pParent����ʱ�����ĵ�Ԫ������һ����Ԫ������һ���ɱ䳤�ȵ��������4�ֽ�ҳ����ɡ�
** ���仰˵,���13�ֽڡ����,pSpace����������Ҫ����13���ֽڴ�С��
*/

/*
�˰汾��balance()���������������������Ŀ�������������Ҷˣ�
���仰˵���µ���Ŀ����Ϊ������Ŀ��pPage��Ҷ��ҳ�������������ұߵ�ҳ�档
pParent���丸�ڵ㡣 pPage��һ�����ҳ����Ŀ��
*/
static int balance_quick(MemPage *pParent, MemPage *pPage, u8 *pSpace){  //�������������һ������Ŀ�����뵽�������Ҷ�.
  BtShared *const pBt = pPage->pBt;    /* B-Tree Database */             //���ݿ���B��
  MemPage *pNew;                       /* Newly allocated page */        //�·����ҳ
  int rc;                              /* Return Code */                 //���ش���
  Pgno pgnoNew;                        /* Page number of pNew */         // pNew��ҳ��

  assert( sqlite3_mutex_held(pPage->pBt->mutex) );
  assert( sqlite3PagerIswriteable(pParent->pDbPage) );
  assert( pPage->nOverflow==1 );

  /* This error condition is now caught prior to reaching this function */ 
  if( pPage->nCell<=0 ) return SQLITE_CORRUPT_BKPT;

  /* Allocate a new page. This page will become the right-sibling of 
  ** pPage. Make the parent page writable, so that the new divider cell
  ** may be inserted. If both these operations are successful, proceed.
  ** ����һ����ҳ����ҳ����ΪpPage�Ҳ�ķ�֧��ȷ����ҳ��ʱ��д���Ա����·ֳ��ĵ�Ԫ�����롣����������������ɹ����򱣻���
  */
  rc = allocateBtreePage(pBt, &pNew, &pgnoNew, 0, 0);  //����һ����ҳ

  if( rc==SQLITE_OK ){

    u8 *pOut = &pSpace[4];
    u8 *pCell = pPage->apOvfl[0];
    u16 szCell = cellSizePtr(pPage, pCell);  //����һ����Ԫ��Ҫ���ܵ��ֽ�������ֵ��szCell
    u8 *pStop;

    assert( sqlite3PagerIswriteable(pNew->pDbPage) );
    assert( pPage->aData[0]==(PTF_INTKEY|PTF_LEAFDATA|PTF_LEAF) );
    zeroPage(pNew, PTF_INTKEY|PTF_LEAFDATA|PTF_LEAF);
    assemblePage(pNew, 1, &pCell, &szCell);

    /* If this is an auto-vacuum database, update the pointer map
    ** with entries for the new page, and any pointer from the 
    ** cell on the page to an overflow page. If either of these
    ** operations fails, the return code is set, but the contents
    ** of the parent page are still manipulated by thh code below.
    ** That is Ok, at this point the parent page is guaranteed to
    ** be marked as dirty. Returning an error code will cause a
    ** rollback, undoing any changes made to the parent page.
    */
	/*�������һ���Զ���յ����ݿ⣬����ָ��һ����ҳ����Ŀ�������Щ
	����ʧ�ܣ����ش��뱻���ã�����ҳ�汻thh������ݡ�����һ���ϱ�֤��ҳ��
	�����Ϊ���֡����ش�����뽫���»ع���������ҳ���������κθ��ġ�
	*/
    if( ISAUTOVACUUM ){
      ptrmapPut(pBt, pgnoNew, PTRMAP_BTREE, pParent->pgno, &rc);
      if( szCell>pNew->minLocal ){
        ptrmapPutOvflPtr(pNew, pCell, &rc);
      }
    }
  
    /* Create a divider cell to insert into pParent. The divider cell
    ** consists of a 4-byte page number (the page number of pPage) and
    ** a variable length key value (which must be the same value as the
    ** largest key on pPage).
    **
    ** To find the largest key value on pPage, first find the right-most 
    ** cell on pPage. The first two fields of this cell are the 
    ** record-length (a variable length integer at most 32-bits in size)
    ** and the key value (a variable length integer, may have any value).
    ** The first of the while(...) loops below skips over the record-length
    ** field. The second while(...) loop copies the key value from the
    ** cell on pPage into the pSpace buffer.
    */

	/*����һ����������Ԫ�����뵽pParent����������һ��4�ֽڵ�ҳ�ţ�PPAGE��ҳ�ţ���
	��һ���ɱ䳤�ȹؼ��ֵ�ֵ������������ͬ��ֵ��ΪPPAGE�����ļ�����ɡ�
	Ҫ����PPAGE���ļ�ֵ�����ҵ����ұߵ�pPage��Ԫ�������Ԫ��ǰ�����ֶ��Ǽ�¼����
	(һ���ɱ䳤�ȵ�������С���32λ)�͹ؼ���ֵ(һ���ɱ䳤�ȵ�����,�����м�ֵ)��
	��һ��whileѭ���������¼�¼�����ֶΡ��ڶ���whileѭ������pPage�ϵĵ�Ԫ�ؼ���ֵ��pSpace�������� 
*/
    pCell = findCell(pPage, pPage->nCell-1);
    pStop = &pCell[9];
    while( (*(pCell++)&0x80) && pCell<pStop );
    pStop = &pCell[9];
    while( ((*(pOut++) = *(pCell++))&0x80) && pCell<pStop );

    /* Insert the new divider cell into pParent. */  //�����µĳ�������Ԫ��pParent
    insertCell(pParent, pParent->nCell, pSpace, (int)(pOut-pSpace),
               0, pPage->pgno, &rc);

    /* Set the right-child pointer of pParent to point to the new page. */  //����pParent�Һ��ӵ�ָ��ָ����ҳ
    put4byte(&pParent->aData[pParent->hdrOffset+8], pgnoNew);
  
    /* Release the reference to the new page. */     //�ͷŶ���ҳ������
    releasePage(pNew);
  }

  return rc;
}
#endif /* SQLITE_OMIT_QUICKBALANCE */

#if 0
/*
** This function does not contribute anything to the operation of SQLite.
** it is sometimes activated temporarily while debugging code responsible 
** for setting pointer-map entries.
** ���������SQLite���������κΰ���.ֻ�ǵ����Դ�������pointer-map��Ŀʱ��ʱ���
*/
static int ptrmapCheckPages(MemPage **apPage, int nPage){
  int i, j;
  for(i=0; i<nPage; i++){
    Pgno n;
    u8 e;
    MemPage *pPage = apPage[i];
    BtShared *pBt = pPage->pBt;
    assert( pPage->isInit );

    for(j=0; j<pPage->nCell; j++){
      CellInfo info;
      u8 *z;
     
      z = findCell(pPage, j);
      btreeParseCellPtr(pPage, z, &info);
      if( info.iOverflow ){
        Pgno ovfl = get4byte(&z[info.iOverflow]);
        ptrmapGet(pBt, ovfl, &e, &n);
        assert( n==pPage->pgno && e==PTRMAP_OVERFLOW1 );
      }
      if( !pPage->leaf ){
        Pgno child = get4byte(z);
        ptrmapGet(pBt, child, &e, &n);
        assert( n==pPage->pgno && e==PTRMAP_BTREE );
      }
    }
    if( !pPage->leaf ){
      Pgno child = get4byte(&pPage->aData[pPage->hdrOffset+8]);
      ptrmapGet(pBt, child, &e, &n);
      assert( n==pPage->pgno && e==PTRMAP_BTREE );
    }
  }
  return 1;
}
#endif

/*
** This function is used to copy the contents of the b-tree node stored 
** on page pFrom to page pTo. If page pFrom was not a leaf page, then
** the pointer-map entries for each child page are updated so that the
** parent page stored in the pointer map is page pTo. If pFrom contained
** any cells with overflow page pointers, then the corresponding pointer
** map entries are also updated so that the parent page is page pTo.
**
** If pFrom is currently carrying any overflow cells (entries in the
** MemPage.apOvfl[] array), they are not copied to pTo. 
**
** Before returning, page pTo is reinitialized using btreeInitPage().
**
** The performance of this function is not critical. It is only used by 
** the balance_shallower() and balance_deeper() procedures, neither of
** which are called often under normal circumstances.
*/
	/*
	**�˺������ڸ���pFromҳ��b���ڵ�Ĵ洢���ݵ�pToҳ�����ҳ��pFrom����Ҷ��ҳ��Ȼ��
	ÿ����ҳָ���ӳ����Ŀ���и��£��Ա��ڴ洢��ָ��λͼ�еĸ��ڵ���pToҳ.���pFrom��
	���κδ������ҳָ��ĵ�Ԫ,��ô��Ӧ��ָ��λͼҲ����ʹ���ڵ��� pToҳ��
	���pFromĿǰ���κ������Ԫ(��MemPage.apOvfl[]�����е���Ŀ),��ô����û�б����Ƶ�pTo��
	����֮ǰ��ҳ��pTo��Ҫ��btreeInitPage()���³�ʼ��.�˹��ܵ����ܲ��ǹؼ���
	 ������balance_shallower������balance_deeper��������ʹ�á�ͨ������£����߶����á�
	*/

static void copyNodeContent(MemPage *pFrom, MemPage *pTo, int *pRC){   //����pFromҳ��b���ڵ�Ĵ洢���ݵ�pToҳ
  if( (*pRC)==SQLITE_OK ){
    BtShared * const pBt = pFrom->pBt;
    u8 * const aFrom = pFrom->aData;
    u8 * const aTo = pTo->aData;
    int const iFromHdr = pFrom->hdrOffset;
    int const iToHdr = ((pTo->pgno==1) ? 100 : 0);
    int rc;
    int iData;
  
  
    assert( pFrom->isInit );
    assert( pFrom->nFree>=iToHdr );
    assert( get2byte(&aFrom[iFromHdr+5]) <= (int)pBt->usableSize );
  
    /* Copy the b-tree node content from page pFrom to page pTo. */  //��pFromҳ����B���ڵ����ݵ�pToҳ
    iData = get2byte(&aFrom[iFromHdr+5]);
    memcpy(&aTo[iData], &aFrom[iData], pBt->usableSize-iData);
    memcpy(&aTo[iToHdr], &aFrom[iFromHdr], pFrom->cellOffset + 2*pFrom->nCell);
  
    /* Reinitialize page pTo so that the contents of the MemPage structure
    ** match the new data. The initialization of pTo can actually fail under
    ** fairly obscure circumstances, even though it is a copy of initialized 
    ** page pFrom.
	** ���³�ʼ��ҳpTo��ʹMemPage�ṹ�����ݺ��µ�����ƥ�䡣�൱ģ��������£�pTo�ĳ�ʼ������ʧ��,
	** ��ʹ����һ���Ѿ���ʼ��pFromҳ�ĸ�����
    */
    pTo->isInit = 0;
    rc = btreeInitPage(pTo);   //��ʼ��ҳpTo
    if( rc!=SQLITE_OK ){
      *pRC = rc;
      return;
    }
    /* If this is an auto-vacuum database, update the pointer-map entries
    ** for any b-tree or overflow pages that pTo now contains the pointers to.
    ** �������һ���Զ���������ݿ⣬��ô����pTo��ָ��ָ����κ�B�������ҳ�����ָ��λͼ��Ŀ.*/
    if( ISAUTOVACUUM ){
      *pRC = setChildPtrmaps(pTo);
    }
  }
}

/*
** This routine redistributes cells on the iParentIdx'th child of pParent
** (hereafter "the page") and up to 2 siblings so that all pages have about the
** same amount of free space. Usually a single sibling on either side of the
** page are used in the balancing, though both siblings might come from one
** side if the page is the first or last child of its parent. If the page 
** has fewer than 2 siblings (something which can only happen if the page
** is a root page or a child of a root page) then all available siblings
** participate in the balancing.
** ��������� pParent�ĵ�iParentIdx���������·��䵥Ԫ(���¼�ơ�ҳ�桱)�ʹﵽ2���ֵܽڵ�,
** ����������ҳ�涼����ͬ���������ɿռ䡣ͨ����ҳ�������һ���ֵܽڵ���ƽ���,
** ���ҳ��ĸ��ڵ��ǵ�һ�������һ���������ֵܽڵ��������һ�ࡣ���ҳ���Ѿ�����2�ֵ�
** (���ҳ����һ�����������ҳ�棬��Щ�ƶ�ϯ����Ψһ����)Ȼ�����п��õ��ֵܽ��ò���ƽ�⡣
** The number of siblings of the page might be increased or decreased by 
** one or two in an effort to keep pages nearly full but not over full. 
** ҳ����ֵܵ��������ܻ����ӻ����һ������������������ҳ�漸������������ȫΪ����
** Note that when this routine is called, some of the cells on the page
** might not actually be stored in MemPage.aData[]. This can happen
** if the page is overfull. This routine ensures that all cells allocated
** to the page and its siblings fit into MemPage.aData[] before returning.
** ע��,�������������,��ҳ���ϵ�һЩ��Ԫ���ܲ���ȫ�Ǵ洢��MemPage.aData[]�����ҳ�����
** ���ܻᷢ�����������������������ҳ������е�Ԫ������֮ǰ���ֵܻ�д��MemPage.aData[]��
** In the course of balancing the page and its siblings, cells may be
** inserted into or removed from the parent page (pParent). Doing so
** may cause the parent page to become overfull or underfull. If this
** happens, it is the responsibility of the caller to invoke the correct
** balancing routine to fix this problem (see the balance() routine). 
** ��ƽ��ҳ��������ֵܹ�����,��Ԫ���ܲ����Ӹ�ҳ��(pParent)ɾ�������������ܵ��¸�ҳ���������
** ����ⷢ����,���ú������������ȷ��ƽ�⺯��������������(��balance()����)��
** If this routine fails for any reason, it might leave the database
** in a corrupted state. So if this routine fails, the database should
** be rolled back.
** ����������ʧ��,������ʹ���ݿ���һ���𻵵�״̬���������������ʧ��,���ݿ�Ӧ�ûع���
** The third argument to this function, aOvflSpace, is a pointer to a
** buffer big enough to hold one page. If while inserting cells into the parent
** page (pParent) the parent page becomes overfull, this buffer is
** used to store the parent's overflow cells. Because this function inserts
** a maximum of four divider cells into the parent page, and the maximum
** size of a cell stored within an internal node is always less than 1/4
** of the page-size, the aOvflSpace[] buffer is guaranteed to be large
** enough for all overflow cells.
** ��������ĵ���������aOvflSpace��һ��ָ�룬ָ��һ���㹻���ҳ�Ļ������������Ԫ��
** ���븸ҳ��(pParent)���ø�ҳ���ù�����,��ô������������ڴ洢��ҳ��������Ԫ��
** ��Ϊ��������������ĸ������ĵ�Ԫ���븸ҳ��,���Ҵ洢��һ���ڲ��ڵ��еĵ�Ԫ�����ֵ
** ����С��1/4��ҳ���С,���aOvflSpace[]�������Ǳ�֤�����Ԫ�㹻��
** If aOvflSpace is set to a null pointer, this function returns SQLITE_NOMEM.
** ���aOvflSpaceû���趨ָ�룬��������SQLITE_NOMEM.
*/
/*
����������·��䵥Ԫ���ֵܽڵ㡣
*/

#if defined(_MSC_VER) && _MSC_VER >= 1700 && defined(_M_ARM)
#pragma optimize("", off)
#endif
static int balance_nonroot(                                //����B���ĸ��ڵ�ʹ֮����ƽ��
  MemPage *pParent,               /* Parent page of siblings being balanced */      //Ҫƽ����ֵܽڵ�ĸ�ҳ��
  int iParentIdx,                 /* Index of "the page" in pParent */              //pParentҶ����ҳ����
  u8 *aOvflSpace,                 /* page-size bytes of space for parent ovfl */    //˫��Ҷ��Ŀռ��С�ֽ�
  int isRoot,                     /* True if pParent is a root-page */              //���pParent�Ǹ�ҳ����Ϊtrue
  int bBulk                       /* True if this call is part of a bulk load */    //��������ǿ鸺�ص�һ������Ϊtrue
){
  BtShared *pBt;               /* The whole database */                             //�������ݿ�
  int nCell = 0;               /* Number of cells in apCell[] */                    //apCell[]�еĵ�Ԫ��
  int nMaxCells = 0;           /* Allocated size of apCell, szCell, aFrom. */       //�����apCell, szCell, aFrom�Ĵ�С
  int nNew = 0;                /* Number of pages in apNew[] */                     //apNew[]��ҳ������
  int nOld;                    /* Number of pages in apOld[] */                     //apOld[]��Ҳ������
  int i, j, k;                 /* Loop counters */                                  //ѭ���еı���
  int nxDiv;                   /* Next divider slot in pParent->aCell[] */          //pParent->aCell[]�е���һ���ָ�λ��
  int rc = SQLITE_OK;          /* The return code */                                //���ش���
  u16 leafCorrection;          /* 4 if pPage is a leaf.  0 if not */                //�����Ҷ�ӽڵ��ֵΪ4������Ϊ0
  int leafData;                /* True if pPage is a leaf of a LEAFDATA tree */     //���pPage��LEAFDATA����Ҷ�ӽڵ���Ϊtrue
  int usableSpace;             /* Bytes in pPage beyond the header */               //pPage��ͷ��������ֽ��������ÿռ�
  int pageFlags;               /* Value of pPage->aData[0] */                       //pPage->aData[0]��ֵ
  int subtotal;                /* Subtotal of bytes in cells on one page */         //һ��ҳ�ϵĵ�Ԫ�е��ֽ���
  int iSpace1 = 0;             /* First unused byte of aSpace1[] */                 // aSpace1[]�е�һ���������ֽ�
  int iOvflSpace = 0;          /* First unused byte of aOvflSpace[] */              //aOvflSpace[]�еĲ������ֽ�
  int szScratch;               /* Size of scratch memory requested */               //�ݴ�����Ҫ�Ĵ�С
  MemPage *apOld[NB];          /* pPage and up to two siblings */                   //pPage���ﵽ�����ֽ�
  MemPage *apCopy[NB];         /* Private copies of apOld[] pages */                //apOld[]��˽�и���
  MemPage *apNew[NB+2];        /* pPage and up to NB siblings after balancing */    //ƽ����pPage��NB���ֵ�
  u8 *pRight;                  /* Location in parent of right-sibling pointer */    //���ֵ�ָ��ĸ��ڵ�λ��
  u8 *apDiv[NB-1];             /* Divider cells in pParent */                       //pParent�еķ���ĵ�Ԫ
  int cntNew[NB+2];            /* Index in aCell[] of cell after i-th page */       //��i��ҳ���Ԫ��aCell[]�е�����
  int szNew[NB+2];             /* Combined size of cells place on i-th page */      //��i��ҳ���ϵĵ�Ԫ���ܴ�С
  u8 **apCell = 0;             /* All cells begin balanced */                       //��ʼʱ����ƽ��ĵ�Ԫ��
  u16 *szCell;                 /* Local size of all cells in apCell[] */            //apCell[]�е����е�Ԫ�ı��ش�С
  u8 *aSpace1;                 /* Space for copies of dividers cells */             //���뵥Ԫ�ĸ����ռ�
  Pgno pgno;                   /* Temp var to store a page number in */             //�����д洢ҳ���

  pBt = pParent->pBt;
  assert( sqlite3_mutex_held(pBt->mutex) );
  assert( sqlite3PagerIswriteable(pParent->pDbPage) );

#if 0
  TRACE(("BALANCE: begin page %d child of %d\n", pPage->pgno, pParent->pgno));
#endif

  /* At this point pParent may have at most one overflow cell. And if
  ** this overflow cell is present, it must be the cell with 
  ** index iParentIdx. This scenario comes about when this function
  ** is called (indirectly) from sqlite3BtreeDelete().
  ** ��ʱpParent�������һ�������Ԫ��������������Ԫ����,��һ���Ǵ���iParentIdx�����ġ�
  ** ������������������sqlite3BtreeDelete()����(���)��
  */
  assert( pParent->nOverflow==0 || pParent->nOverflow==1 );
  assert( pParent->nOverflow==0 || pParent->aiOvfl[0]==iParentIdx );

  if( !aOvflSpace ){
    return SQLITE_NOMEM;
  }

  /* Find the sibling pages to balance. Also locate the cells in pParent 
  ** that divide the siblings. An attempt is made to find NN siblings on 
  ** either side of pPage. More siblings are taken from one side, however, 
  ** if there are fewer than NN siblings on the other side. If pParent
  ** has NB or fewer children then all children of pParent are taken.  
  ** �ҵ�Ҫƽ����ֵ�ҳ��.��ȷ��pParent�зֿ��ֵܵ�Ԫ��λ�á���ͼ
  ** �ҵ�pPage�����NN�ֵܡ�Ȼ��,һ���и�����ֵܽڵ���ô������
  ** NN���ֵ�����һ�ߡ����pParent��NB����ٺ�����ôpParent�ĺ���ռ�ݡ�
  ** This loop also drops the divider cells from the parent page. This
  ** way, the remainder of the function does not have to deal with any
  ** overflow cells in the parent page, since if any existed they will
  ** have already been removed.
  ** ���ѭ��Ҳ�Ӹ�ҳ��ɾ������ĵ�Ԫ���������������ಿ�ֲ���Ҫ�����κ���
  **��ҳ��������ĵ�Ԫ,��Ϊ����κδ��ڵĶ��Ѿ����Ƴ���
  */
  /*�ҵ��ֵ�ҳ�Դﵽƽ�⡣*/
  i = pParent->nOverflow + pParent->nCell;
  if( i<2 ){
    nxDiv = 0;
  }else{
    assert( bBulk==0 || bBulk==1 );
    if( iParentIdx==0 ){                 
      nxDiv = 0;
    }else if( iParentIdx==i ){
      nxDiv = i-2+bBulk;
    }else{
      assert( bBulk==0 );
      nxDiv = iParentIdx-1;
    }
    i = 2-bBulk;
  }
  nOld = i+1;
  if( (i+nxDiv-pParent->nOverflow)==pParent->nCell ){
    pRight = &pParent->aData[pParent->hdrOffset+8];
  }else{
    pRight = findCell(pParent, i+nxDiv-pParent->nOverflow);
  }
  pgno = get4byte(pRight);
  while( 1 ){
    rc = getAndInitPage(pBt, pgno, &apOld[i]);
    if( rc ){
      memset(apOld, 0, (i+1)*sizeof(MemPage*));
      goto balance_cleanup;
    }
    nMaxCells += 1+apOld[i]->nCell+apOld[i]->nOverflow;
    if( (i--)==0 ) break;

    if( i+nxDiv==pParent->aiOvfl[0] && pParent->nOverflow ){
      apDiv[i] = pParent->apOvfl[0];
      pgno = get4byte(apDiv[i]);
      szNew[i] = cellSizePtr(pParent, apDiv[i]);
      pParent->nOverflow = 0;
    }else{
      apDiv[i] = findCell(pParent, i+nxDiv-pParent->nOverflow);
      pgno = get4byte(apDiv[i]);
      szNew[i] = cellSizePtr(pParent, apDiv[i]);

      /* Drop the cell from the parent page. apDiv[i] still points to
      ** the cell within the parent, even though it has been dropped.
      ** This is safe because dropping a cell only overwrites the first
      ** four bytes of it, and this function does not need the first
      ** four bytes of the divider cell. So the pointer is safe to use
      ** later on.  
      ** �Ӹ�ҳ��ɾ����Ԫ��apDiv[i]��Ȼָ�򸸽ڵ��ڵĵ�Ԫ,��ʹ���Ѿ�ɾ�������ǰ�ȫ��,��Ϊ��Ԫ
	  ** ���������Ŀ�ʼ��4���ֽ�,�ú�������Ҫ������Ԫ�Ŀ�ʼ���ĸ��ֽڡ�ָ����԰�ȫ�����ʹ�á�
      ** But not if we are in secure-delete mode. In secure-delete mode,
      ** the dropCell() routine will overwrite the entire cell with zeroes.
      ** In this case, temporarily copy the cell into the aOvflSpace[]
      ** buffer. It will be copied out again as soon as the aSpace[] buffer
      ** is allocated. 
	  ** ������֮����Ҫ��ȫɾ��ģʽ.�ڰ�ȫɾ��ģʽ��,  dropCell()��������0����������Ԫ.����
	  ** �������,��ʱ���ݵ�Ԫ��aOvflSpace[]������.һ��aSpace[]���������������������Ƴ�����
	  */
      if( pBt->btsFlags & BTS_SECURE_DELETE ){
        int iOff;

        iOff = SQLITE_PTR_TO_INT(apDiv[i]) - SQLITE_PTR_TO_INT(pParent->aData);
        if( (iOff+szNew[i])>(int)pBt->usableSize ){
          rc = SQLITE_CORRUPT_BKPT;
          memset(apOld, 0, (i+1)*sizeof(MemPage*));
          goto balance_cleanup;
        }else{
          memcpy(&aOvflSpace[iOff], apDiv[i], szNew[i]);
          apDiv[i] = &aOvflSpace[apDiv[i]-pParent->aData];
        }
      }
      dropCell(pParent, i+nxDiv-pParent->nOverflow, szNew[i], &rc);
    }
  }

  /* Make nMaxCells a multiple of 4 in order to preserve 8-byte alignment */
  //ʹnMaxCellsΪ4�ı���Ϊ�˱���8�ֽڵĶ���.
  nMaxCells = (nMaxCells + 3)&~3;

  /* Allocate space for memory structures */                //Ϊ�ڴ�ṹ����ռ�
  k = pBt->pageSize + ROUND8(sizeof(MemPage));
  szScratch =
       nMaxCells*sizeof(u8*)                       /* apCell */               //��ʼʱ����ƽ��ĵ�Ԫ��
     + nMaxCells*sizeof(u16)                       /* szCell */               //apCell[]�е����е�Ԫ�ı��ش�С
     + pBt->pageSize                               /* aSpace1 */              //���뵥Ԫ�ĸ����ռ�
     + k*nOld;                                     /* Page copies (apCopy) */ //ҳ����
  apCell = sqlite3ScratchMalloc( szScratch ); 
  if( apCell==0 ){
    rc = SQLITE_NOMEM;
    goto balance_cleanup;
  }
  szCell = (u16*)&apCell[nMaxCells];
  aSpace1 = (u8*)&szCell[nMaxCells];
  assert( EIGHT_BYTE_ALIGNMENT(aSpace1) );

  /*
  ** Load pointers to all cells on sibling pages and the divider cells
  ** into the local apCell[] array.  Make copies of the divider cells
  ** into space obtained from aSpace1[] and remove the divider cells
  ** from pParent.
  ** ����ָ���ֵ�ҳ���ϵ����е�Ԫ�ͷ��뵥Ԫ������apCell[]���顣���Ʒַ���ĵ�Ԫ��
  ** ���������aSpace1[]��õĿռ䲢��pParentɾ������ĵ�Ԫ��
  ** If the siblings are on leaf pages, then the child pointers of the
  ** divider cells are stripped from the cells before they are copied
  ** into aSpace1[].  In this way, all cells in apCell[] are without
  ** child pointers.  If siblings are not leaves, then all cell in
  ** apCell[] include child pointers.  Either way, all cells in apCell[]
  ** are alike.
  ** ����ֵ���Ҷҳ��,��ô���뵥Ԫ�ĺ���ָ�������Ǳ���������aSpace1[]֮ǰ�ӵ�Ԫ���Ƴ�.ͨ�����ַ�ʽ,��apCell[]�е����е�
  ** ��Ԫ��û�к���ָ��.����ֵܲ���Ҷ��,��ôapCell[]�е����е�Ԫ���к���ָ��.�������,��apCell[]�ֵ����еĵ�Ԫ����һ����.
  ** leafCorrection:  4 if pPage is a leaf.  0 if pPage is not a leaf.
  ** leafData:  1 if pPage holds key+data and pParent holds only keys.
  ** leafCorrection:���pPage��Ҷ�ӣ�Ϊ4������Ϊ0.    leafData:��pPage��Key��data����pParent����key��ôΪ1.
  */
  leafCorrection = apOld[0]->leaf*4;
  leafData = apOld[0]->hasData;
  for(i=0; i<nOld; i++){
    int limit;
    
    /* Before doing anything else, take a copy of the i'th original sibling
    ** The rest of this function will use data from the copies rather
    ** that the original pages since the original pages will be in the
    ** process of being overwritten. 
	** �����κ���������֮ǰ,����ԭ���ĵ�i���ֵ�.������������ಿ�ֽ�ʹ�����Ը��������ݣ�
	** ������Դʵҵ������,ԭʼҳ�潫�ڱ������ǵĽ����С�
	*/
    MemPage *pOld = apCopy[i] = (MemPage*)&aSpace1[pBt->pageSize + k*i];
    memcpy(pOld, apOld[i], sizeof(MemPage));
    pOld->aData = (void*)&pOld[1];
    memcpy(pOld->aData, apOld[i]->aData, pBt->pageSize);

    limit = pOld->nCell+pOld->nOverflow;
    if( pOld->nOverflow>0 ){
      for(j=0; j<limit; j++){
        assert( nCell<nMaxCells );
        apCell[nCell] = findOverflowCell(pOld, j);
        szCell[nCell] = cellSizePtr(pOld, apCell[nCell]);
        nCell++;
      }
    }else{
      u8 *aData = pOld->aData;
      u16 maskPage = pOld->maskPage;
      u16 cellOffset = pOld->cellOffset;
      for(j=0; j<limit; j++){
        assert( nCell<nMaxCells );
        apCell[nCell] = findCellv2(aData, maskPage, cellOffset, j);
        szCell[nCell] = cellSizePtr(pOld, apCell[nCell]);
        nCell++;
      }
    }       
    if( i<nOld-1 && !leafData){
      u16 sz = (u16)szNew[i];
      u8 *pTemp;
      assert( nCell<nMaxCells );
      szCell[nCell] = sz;
      pTemp = &aSpace1[iSpace1];
      iSpace1 += sz;
      assert( sz<=pBt->maxLocal+23 );
      assert( iSpace1 <= (int)pBt->pageSize );
      memcpy(pTemp, apDiv[i], sz);
      apCell[nCell] = pTemp+leafCorrection;
      assert( leafCorrection==0 || leafCorrection==4 );
      szCell[nCell] = szCell[nCell] - leafCorrection;
      if( !pOld->leaf ){
        assert( leafCorrection==0 );
        assert( pOld->hdrOffset==0 );
        /* The right pointer of the child page pOld becomes the left
        ** pointer of the divider cell 
		** ����ҳ��pOld����ָ���ɷ��뵥Ԫ����ָ��*/
        memcpy(apCell[nCell], &pOld->aData[8], 4);
      }else{
        assert( leafCorrection==4 );
        if( szCell[nCell]<4 ){
          /* Do not allow any cells smaller than 4 bytes. */  //�������κε�ԪС��4���ֽ�
          szCell[nCell] = 4;
        }
      }
      nCell++;
    }
  }

  /*
  ** Figure out the number of pages needed to hold all nCell cells.
  ** Store this number in "k".  Also compute szNew[] which is the total
  ** size of all cells on the i-th page and cntNew[] which is the index
  ** in apCell[] of the cell that divides page i from page i+1.  
  ** cntNew[k] should equal nCell.
  ** �������Ҫ�������� nCell ��Ԫ����������ֵ����k.Ҳ����szNew�����ڵ�i��ҳ�ϵ���Ԫ���ܴ�С��
  ** �Լ�cntNew[]�����ڷֿ���i���͵�i+1��ҳ�ĵ�Ԫ��apCell[]�е�������
  ** Values computed by this block:      //ͨ����������ֵ
  **
  **           k: The total number of sibling pages                                   //k���ֵ�Ҳ������
  **    szNew[i]: Spaced used on the i-th sibling page.                        //szNew[i]����i���ֵ�ҳʹ�õĿռ��С
  **   cntNew[i]: Index in apCell[] and szCell[] for the first cell to     //cntNew[i]����apCell[] ��szCell[]�е�i���ֵ�ҳ���Ҳ��һ����Ԫ������
  **              the right of the i-th sibling page.
  ** usableSpace: Number of bytes of space available on each sibling.   //usableSpace:ÿ���ֵ�ҳ�Ͽ��õĿռ��ֽڴ�С
  */
  usableSpace = pBt->usableSize - 12 + leafCorrection;
  for(subtotal=k=i=0; i<nCell; i++){
    assert( i<nMaxCells );
    subtotal += szCell[i] + 2;
    if( subtotal > usableSpace ){
      szNew[k] = subtotal - szCell[i];
      cntNew[k] = i;
      if( leafData ){ i--; }
      subtotal = 0;
      k++;
      if( k>NB+1 ){ rc = SQLITE_CORRUPT_BKPT; goto balance_cleanup; }
    }
  }
  szNew[k] = subtotal;
  cntNew[k] = nCell;
  k++;

  /*
  ** The packing computed by the previous block is biased toward the siblings
  ** on the left side.  The left siblings are always nearly full, while the
  ** right-most sibling might be nearly empty.  This block of code attempts
  ** to adjust the packing of siblings to get a better balance.
  ** ͨ��ǰһ�������㣬ƫ�����ֵܽڵ㡣���ֵ�����װ�þ�������,�����Ҳ��ֵܽ����ܿա�
  ** ��δ���ĳ��Ե���ʹ���ֵܽڵ���õر���ƽ�⡣
  ** This adjustment is more than an optimization.  The packing above might
  ** be so out of balance as to be illegal.  For example, the right-most
  ** sibling might be completely empty.  This adjustment is not optional.
  ** ���ֵ������Ż�������İ�װ���ܻ���ʧȥƽ��,����ǷǷ��ġ�
  ** ����,���ұ��ֵܿ�����ȫ�ǿյģ���ʱ���ֵ�������ѡ��
  */
  for(i=k-1; i>0; i--){
    int szRight = szNew[i];  /* Size of sibling on the right */                 //���ֵܵĴ�С
    int szLeft = szNew[i-1]; /* Size of sibling on the left */                  //���ֵܵĴ�С
    int r;              /* Index of right-most cell in left sibling */          //���ֵ������ҵ�Ԫ������
    int d;              /* Index of first cell to the left of right sibling */  //���ֵ�������һ����Ԫ������

    r = cntNew[i-1] - 1;
    d = r + 1 - leafData;
    assert( d<nMaxCells );
    assert( r<nMaxCells );
    while( szRight==0 
       || (!bBulk && szRight+szCell[d]+2<=szLeft-(szCell[r]+2)) 
    ){
      szRight += szCell[d] + 2;
      szLeft -= szCell[r] + 2;
      cntNew[i-1]--;
      r = cntNew[i-1] - 1;
      d = r + 1 - leafData;
    }
    szNew[i] = szRight;
    szNew[i-1] = szLeft;
  }

  /* Either we found one or more cells (cntnew[0])>0) or pPage is
  ** a virtual root page.  A virtual root page is when the real root
  ** page is page 1 and we are the only child of that page.
  ** ���Ƿ���һ�������(cntnew[0])> 0)��pPage��һ�������ҳ�档
  ** һ������ĸ�ҳ�ǵ������ĸ�ҳ�ǵ�1ҳ��ʱ����Ǹ�ҳ����Ψһ�ĺ��ӡ�
  ** UPDATE:  The assert() below is not necessarily true if the database
  ** file is corrupt.  The corruption will be detected and reported later
  ** in this procedure so there is no need to act upon it now.
  */
#if 0
  assert( cntNew[0]>0 || (pParent->pgno==1 && pParent->nCell==0) );
#endif

  TRACE(("BALANCE: old: %d %d %d  ",
    apOld[0]->pgno, 
    nOld>=2 ? apOld[1]->pgno : 0,
    nOld>=3 ? apOld[2]->pgno : 0
  ));

  /*Allocate k new pages.  Reuse old pages where possible. */     //����k��ҳ���п�������ʹ����ҳ
  if( apOld[0]->pgno<=1 ){
    rc = SQLITE_CORRUPT_BKPT;
    goto balance_cleanup;
  }
  pageFlags = apOld[0]->aData[0];
  for(i=0; i<k; i++){
    MemPage *pNew;
    if( i<nOld ){
      pNew = apNew[i] = apOld[i];
      apOld[i] = 0;
      rc = sqlite3PagerWrite(pNew->pDbPage);
      nNew++;
      if( rc ) goto balance_cleanup;
    }else{
      assert( i>0 );
      rc = allocateBtreePage(pBt, &pNew, &pgno, (bBulk ? 1 : pgno), 0);
      if( rc ) goto balance_cleanup;
      apNew[i] = pNew;
      nNew++;

      /* Set the pointer-map entry for the new sibling page. */  //�����µ��ֵ�ҳ����ָ��λͼ��Ŀ
      if( ISAUTOVACUUM ){
        ptrmapPut(pBt, pNew->pgno, PTRMAP_BTREE, pParent->pgno, &rc);
        if( rc!=SQLITE_OK ){
          goto balance_cleanup;
        }
      }
    }
  }

  /* Free any old pages that were not reused as new pages.*/    //�ͷ�û������ʹ������ҳ����ҳ
  while( i<nOld ){
    freePage(apOld[i], &rc);
    if( rc ) goto balance_cleanup;
    releasePage(apOld[i]);
    apOld[i] = 0;
    i++;
  }

  /*
  ** Put the new pages in accending order.  This helps to
  ** keep entries in the disk file in order so that a scan
  ** of the table is a linear scan through the file.  That
  ** in turn helps the operating system to deliver pages
  ** from the disk more rapidly.
  ** ʹ��ҳ���������������ڱ��ִ����ļ��е���Ŀ˳���Ա��ڶԱ�Ľ���
  ** һ������ɨ�������ļ�.��������������ϵͳ�Ӵ��̸�����ṩҳ�档
  ** An O(n^2) insertion sort algorithm is used, but since
  ** n is never more than NB (a small constant), that should
  ** not be a problem.
  ** ��һ�����Ӷ�ΪO(n^2)�Ĳ��������㷨,����n���ᳬ��NB��Ӧ�ò�������
  ** When NB==3, this one optimization makes the database
  ** about 25% faster for large insertions and deletions.
  ** ��NB==3,����Ż�ʹ���ݿ����ɾ��������ߴ�Լ25%���ҡ�
  */
  for(i=0; i<k-1; i++){
    int minV = apNew[i]->pgno;
    int minI = i;
    for(j=i+1; j<k; j++){
      if( apNew[j]->pgno<(unsigned)minV ){
        minI = j;
        minV = apNew[j]->pgno;
      }
    }
    if( minI>i ){
      MemPage *pT;
      pT = apNew[i];
      apNew[i] = apNew[minI];
      apNew[minI] = pT;
    }
  }
  TRACE(("new: %d(%d) %d(%d) %d(%d) %d(%d) %d(%d)\n",
    apNew[0]->pgno, szNew[0],
    nNew>=2 ? apNew[1]->pgno : 0, nNew>=2 ? szNew[1] : 0,
    nNew>=3 ? apNew[2]->pgno : 0, nNew>=3 ? szNew[2] : 0,
    nNew>=4 ? apNew[3]->pgno : 0, nNew>=4 ? szNew[3] : 0,
    nNew>=5 ? apNew[4]->pgno : 0, nNew>=5 ? szNew[4] : 0));

  assert( sqlite3PagerIswriteable(pParent->pDbPage) );
  put4byte(pRight, apNew[nNew-1]->pgno);

  /*
  ** Evenly distribute the data in apCell[] across the new pages.
  ** Insert divider cells into pParent as necessary.
  ** ���µ�ҳ���apCell[]�о��ȷֲ����ݡ�����ָ���ԪpParent�Ǳ�Ҫ�ġ�
  */
  j = 0;
  for(i=0; i<nNew; i++){
    /* Assemble the new sibling page. */     //��װ���ֵ�ҳ
    MemPage *pNew = apNew[i];
    assert( j<nMaxCells );
    zeroPage(pNew, pageFlags);
    assemblePage(pNew, cntNew[i]-j, &apCell[j], &szCell[j]);
    assert( pNew->nCell>0 || (nNew==1 && cntNew[0]==0) );
    assert( pNew->nOverflow==0 );

    j = cntNew[i];

    /* If the sibling page assembled above was not the right-most sibling,
    ** insert a divider cell into the parent page. 
	** ���������װ���ֵ�ҳ�沢�������ұߵ��ֵ�,������뵥Ԫ����ҳ�档
    */
    assert( i<nNew-1 || j==nCell );
    if( j<nCell ){
      u8 *pCell;
      u8 *pTemp;
      int sz;

      assert( j<nMaxCells );
      pCell = apCell[j];
      sz = szCell[j] + leafCorrection;
      pTemp = &aOvflSpace[iOvflSpace];
      if( !pNew->leaf ){
        memcpy(&pNew->aData[8], pCell, 4);
      }else if( leafData ){
        /* If the tree is a leaf-data tree, and the siblings are leaves, 
        ** then there is no divider cell in apCell[]. Instead, the divider 
        ** cell consists of the integer key for the right-most cell of 
        ** the sibling-page assembled above only.
		** �����Ҷ���ݵ��������Ҹ��ڵ���Ҷ�ڵ㣬��ô��APCell[]��û�зָԪ��
		** �෴�ָԪ������װ����ֵܽڵ�����ҵĵ�Ԫ�����ιؼ�����ɡ�
        */
        CellInfo info;
        j--;
        btreeParseCellPtr(pNew, apCell[j], &info);
        pCell = pTemp;
        sz = 4 + putVarint(&pCell[4], info.nKey);
        pTemp = 0;
      }else{
        pCell -= 4;
        /* Obscure case for non-leaf-data trees: If the cell at pCell was
        ** previously stored on a leaf node, and its reported size was 4
        ** bytes, then it may actually be smaller than this 
        ** (see btreeParseCellPtr(), 4 bytes is the minimum size of
        ** any cell). But it is important to pass the correct size to 
        ** insertCell(), so reparse the cell now.
        ** ���������non-leaf-data��:�����pCell�ĵ�Ԫ����ǰ�洢��һ��Ҷ�ڵ��ϵ�,������
		** 4�ֽڴ�С,��ʵ�������ܻ�����С(��btreeParseCellPtr(),4���ֽ����κε�Ԫ����Сֵ)��
		** ����Ҫ����ͨ����ȷ�Ĵ�СinsertCell(),�����������½�����Ԫ��
        ** Note that this can never happen in an SQLite data file, as all
        ** cells are at least 4 bytes. It only happens in b-trees used
        ** to evaluate "IN (SELECT ...)" and similar clauses.
		** ע��,����ܲ��ᷢ����һ��SQLite�����ļ���,���е�Ԫ������4���ֽڡ�
		** ��ֻ������b������������"IN (SELECT ...)"������Ӿ䡣
        */
        if( szCell[j]==4 ){
          assert(leafCorrection==4);
          sz = cellSizePtr(pParent, pCell);
        }
      }
      iOvflSpace += sz;
      assert( sz<=pBt->maxLocal+23 );
      assert( iOvflSpace <= (int)pBt->pageSize );
      insertCell(pParent, nxDiv, pCell, sz, pTemp, pNew->pgno, &rc);
      if( rc!=SQLITE_OK ) goto balance_cleanup;
      assert( sqlite3PagerIswriteable(pParent->pDbPage) );

      j++;
      nxDiv++;
    }
  }
  assert( j==nCell );
  assert( nOld>0 );
  assert( nNew>0 );
  if( (pageFlags & PTF_LEAF)==0 ){
    u8 *zChild = &apCopy[nOld-1]->aData[8];
    memcpy(&apNew[nNew-1]->aData[8], zChild, 4);
  }

  if( isRoot && pParent->nCell==0 && pParent->hdrOffset<=apNew[0]->nFree ){
    /* The root page of the b-tree now contains no cells. The only sibling
    ** page is the right-child of the parent. Copy the contents of the
    ** child page into the parent, decreasing the overall height of the
    ** b-tree structure by one. This is described as the "balance-shallower"
    ** sub-algorithm in some documentation.
    ** B���ĸ�ҳ���ڲ�����Ԫ��Ψһ���ֵ�ҳ�����Һ��ӵĸ��ڵ㡣��������ҳ�������
	** ����ҳ��,����B���ṹ������߶�.��һЩ�ĵ��б�����Ϊ��balance-shallower�� ���㷨��
    ** If this is an auto-vacuum database, the call to copyNodeContent() 
    ** sets all pointer-map entries corresponding to database image pages 
    ** for which the pointer is stored within the content being copied.
    ** �����һ���Զ���������ݿ�,����copyNodeContent() �趨����pointer-map��Ŀ������
	** �⾵��ҳ��Ӧ��ָ��洢�ڱ������������С�
    ** The second assert below verifies that the child page is defragmented 
    ** (it must be, as it was just reconstructed using assemblePage()). This
    ** is important if the parent page happens to be page 1 of the database
    ** image.  
	** �ڶ���������֤��ҳ�汻��Ƭ����(���Ǳ����,��Ϊ��ֻ��ʹ��assemblePage()�ؽ�).
	** ���Ǻ���Ҫ��,�����ҳ�������ݿ⾵���page 1��*/
    assert( nNew==1 );
    assert( apNew[0]->nFree == 
        (get2byte(&apNew[0]->aData[5])-apNew[0]->cellOffset-apNew[0]->nCell*2) 
    );
    copyNodeContent(apNew[0], pParent, &rc);
    freePage(apNew[0], &rc);
  }else if( ISAUTOVACUUM ){
    /* Fix the pointer-map entries for all the cells that were shifted around. 
    ** There are several different types of pointer-map entries that need to
    ** be dealt with by this routine. Some of these have been set already, but
    ** many have not. The following is a summary:
    ** �������б�ת�Ƶ���Χ�ĵ�Ԫ�޸�pointer-map��Ŀ.�м��ֲ�ͬ���͵���Ҫpointer-map��Ŀ
	**Ҫͨ����������������е�һЩ�Ѿ�����,�������û������.�������ܽ�:
    **   1) The entries associated with new sibling pages that were not
    **      siblings when this function was called. These have already
    **      been set. We don't need to worry about old siblings that were
    **      moved to the free-list - the freePage() code has taken care
    **      of those.
    **      ��Щ��Ŀ�µ��ֵ�ҳ�����,��Щ�ֵ�ҳ�浱���ú�������ʱ�������ֵܽڵ㡣
	**      ���ص���֮ǰ���ֵܽڵ㱻�Ƶ��˿����б���freePage() �����Ѿ����ǵ���Щ��
    **   2) The pointer-map entries associated with the first overflow
    **      page in any overflow chains used by new divider cells. These 
    **      have also already been taken care of by the insertCell() code.
    **     �����κ���������еĵ�һ�����ҳ��ص�pointer-map��Ŀ�������뵥Ԫ��
	**     ��ЩҲ�Ѿ���insertCell()�����п��ǡ�
    **   3) If the sibling pages are not leaves, then the child pages of
    **      cells stored on the sibling pages may need to be updated.
    **      ����ֵ�ҳ�治��Ҷ��,��ô�洢���ֵ�ҳ���ϵĵ�Ԫ����ҳ����Ҫ����.
    **   4) If the sibling pages are not internal intkey nodes, then any
    **      overflow pages used by these cells may need to be updated
    **      (internal intkey nodes never contain pointers to overflow pages).
    **      ����ֵ�ҳ�����ڲ�intkey�ڵ�,��ô�κα���Щ��Ԫʹ�õ����ҳ������Ҫ����
	**      (�ڲ�intkey�ڵ㲻����ָ�����ҳ��ָ��)��
    **   5) If the sibling pages are not leaves, then the pointer-map
    **      entries for the right-child pages of each sibling may need
    **      to be updated.
    **    ������ֵ�ҳ�治��Ҷ��,��ô��ÿ���ֵܵ��Һ���ҳpointer-map��Ŀ������Ҫ���¡�
    ** Cases 1 and 2 are dealt with above by other code. The next
    ** block deals with cases 3 and 4 and the one after that, case 5. Since
    ** setting a pointer map entry is a relatively expensive operation, this
    ** code only sets pointer map entries for child or overflow pages that have
    ** actually moved between pages.  
	** ǰ����������������봦����һ���鴦�������3��4,֮��5����Ϊ����һ��ָ��ӳ����Ŀ��һ��
	** ����˷ѵĲ���,�����������ֻ����ҳ��֮���ƶ��ĺ��ӻ����ҳ����ָ���ӳ����Ŀ��*/
    MemPage *pNew = apNew[0];
    MemPage *pOld = apCopy[0];
    int nOverflow = pOld->nOverflow;
    int iNextOld = pOld->nCell + nOverflow;
    int iOverflow = (nOverflow ? pOld->aiOvfl[0] : -1);
    j = 0;                             /* Current 'old' sibling page */   //��ǰ'old'�ֵ�ҳ
    k = 0;                             /* Current 'new' sibling page */   //��ǰ'new'�ֵ�ҳ
    for(i=0; i<nCell; i++){
      int isDivider = 0;
      while( i==iNextOld ){
        /* Cell i is the cell immediately following the last cell on old
        ** sibling page j. If the siblings are not leaf pages of an
        ** intkey b-tree, then cell i was a divider cell. 
		** ��Ԫi�ǵ�Ԫ�������ֵ�ҳ���Ԫ��.�������intkeyB����Ҷ�ӽڵ���ô��Ԫi��һ���ָԪ*/
        assert( j+1 < ArraySize(apCopy) );
        assert( j+1 < nOld );
        pOld = apCopy[++j];
        iNextOld = i + !leafData + pOld->nCell + pOld->nOverflow;
        if( pOld->nOverflow ){
          nOverflow = pOld->nOverflow;
          iOverflow = i + !leafData + pOld->aiOvfl[0];
        }
        isDivider = !leafData;  
      }

      assert(nOverflow>0 || iOverflow<i );
      assert(nOverflow<2 || pOld->aiOvfl[0]==pOld->aiOvfl[1]-1);
      assert(nOverflow<3 || pOld->aiOvfl[1]==pOld->aiOvfl[2]-1);
      if( i==iOverflow ){
        isDivider = 1;
        if( (--nOverflow)>0 ){
          iOverflow++;
        }
      }

      if( i==cntNew[k] ){
        /* Cell i is the cell immediately following the last cell on new
        ** sibling page k. If the siblings are not leaf pages of an
        ** intkey b-tree, then cell i is a divider cell.  */
        pNew = apNew[++k];
        if( !leafData ) continue;
      }
      assert( j<nOld );
      assert( k<nNew );

      /* If the cell was originally divider cell (and is not now) or
      ** an overflow cell, or if the cell was located on a different sibling
      ** page before the balancing, then the pointer map entries associated
      ** with any child or overflow pages need to be updated.  */
      if( isDivider || pOld->pgno!=pNew->pgno ){
        if( !leafCorrection ){
          ptrmapPut(pBt, get4byte(apCell[i]), PTRMAP_BTREE, pNew->pgno, &rc);
        }
        if( szCell[i]>pNew->minLocal ){
          ptrmapPutOvflPtr(pNew, apCell[i], &rc);
        }
      }
    }

    if( !leafCorrection ){
      for(i=0; i<nNew; i++){
        u32 key = get4byte(&apNew[i]->aData[8]);
        ptrmapPut(pBt, key, PTRMAP_BTREE, apNew[i]->pgno, &rc);
      }
    }

#if 0
    /* The ptrmapCheckPages() contains assert() statements that verify that
    ** all pointer map pages are set correctly. This is helpful while 
    ** debugging. This is usually disabled because a corrupt database may
    ** cause an assert() statement to fail. 
	** ptrmapCheckPages()������assert()���ʱ��֤���е�ָ��ӳ��ҳ��ȷ�趨.
	** �������ڵ���.�ⳣ�����ã���Ϊһ�����������ݿ�������assert()���ʧ��.*/
    ptrmapCheckPages(apNew, nNew);
    ptrmapCheckPages(&pParent, 1);
#endif
  }

  assert( pParent->isInit );
  TRACE(("BALANCE: finished: old=%d new=%d cells=%d\n",
          nOld, nNew, nCell));

  /*Cleanup before returning.*/    //����֮ǰ����
balance_cleanup:
  sqlite3ScratchFree(apCell);
  for(i=0; i<nOld; i++){
    releasePage(apOld[i]);
  }
  for(i=0; i<nNew; i++){
    releasePage(apNew[i]);
  }

  return rc;
}
#if defined(_MSC_VER) && _MSC_VER >= 1700 && defined(_M_ARM)
#pragma optimize("", on)
#endif


/*
** This function is called when the root page of a b-tree structure is
** overfull (has one or more overflow pages).
** ��B���ṹ�ĸ�ҳ������ʱ���ú����������á�����һ���������ҳ��
** A new child page is allocated and the contents of the current root
** page, including overflow cells, are copied into the child. The root
** page is then overwritten to make it an empty page with the right-child 
** pointer pointing to the new page.
** һ���µĺ���ҳ�������䲢�ҵ�ǰ��ҳ�����ݰ���Ҳ����Ԫ�����������ӽڵ㡣
** ��ҳ����дʹ֮Ϊ��ҳ�����Һ���ָ��ָ����ҳ��
** Before returning, all pointer-map entries corresponding to pages 
** that the new child-page now contains pointers to are updated. The
** entry corresponding to the new right-child pointer of the root
** page is also updated.
** �ڷ���֮ǰ,����pointer-map��Ŀ��Ӧ����ҳ�����ָ���ҳ�汻���¡���Ŀ��Ӧ�ĸ�ҳ�������ӽ��ָ���Ҳ���¡�
** If successful, *ppChild is set to contain a reference to the child 
** page and SQLITE_OK is returned. In this case the caller is required
** to call releasePage() on *ppChild exactly once. If an error occurs,
** an error code is returned and *ppChild is set to 0.
** ����ɹ�,*ppChild������һ���Ժ���ҳ�����ò�����SQLITE_OK�������������,��������Ҫ
** ��*ppChild�϶�releasePage()����ǡ��һ�Ρ�������ִ���,����һ��������벢��ppChild����Ϊ0��
*/
static int balance_deeper(MemPage *pRoot, MemPage **ppChild){            //��һ������B����ҳ
  int rc;                        /* Return value from subprocedures */   //�Ӻ����ķ���ֵ
  MemPage *pChild = 0;           /* Pointer to a new child page */       //�º���ҳ��ָ��
  Pgno pgnoChild = 0;            /* Page number of the new child page */ //�º���ҳ��ҳ��
  BtShared *pBt = pRoot->pBt;    /* The BTree */                         //B��

  assert( pRoot->nOverflow>0 );
  assert( sqlite3_mutex_held(pBt->mutex) );

  /* Make pRoot, the root page of the b-tree, writable. Allocate a new 
  ** page that will become the new right-child of pPage. Copy the contents
  ** of the node stored on pRoot into the new child page.
  ** ʹpRoot��B���ĸ�ҳ����д,����һ����ҳʹ֮��ΪpPage���Һ���.�����洢��pRoot�ϵĽڵ�����ݵ��µĺ���ҳ�档
  */
  rc = sqlite3PagerWrite(pRoot->pDbPage);
  if( rc==SQLITE_OK ){
    rc = allocateBtreePage(pBt,&pChild,&pgnoChild,pRoot->pgno,0);
    copyNodeContent(pRoot, pChild, &rc);
    if( ISAUTOVACUUM ){
      ptrmapPut(pBt, pgnoChild, PTRMAP_BTREE, pRoot->pgno, &rc);
    }
  }
  if( rc ){
    *ppChild = 0;
    releasePage(pChild);
    return rc;
  }
  assert( sqlite3PagerIswriteable(pChild->pDbPage) );
  assert( sqlite3PagerIswriteable(pRoot->pDbPage) );
  assert( pChild->nCell==pRoot->nCell );

  TRACE(("BALANCE: copy root %d into %d\n", pRoot->pgno, pChild->pgno));

  /* Copy the overflow cells from pRoot to pChild */  //���ѳ���Ԫ��pRoot������pChild
  memcpy(pChild->aiOvfl, pRoot->aiOvfl,
         pRoot->nOverflow*sizeof(pRoot->aiOvfl[0]));
  memcpy(pChild->apOvfl, pRoot->apOvfl,
         pRoot->nOverflow*sizeof(pRoot->apOvfl[0]));
  pChild->nOverflow = pRoot->nOverflow;

  /* Zero the contents of pRoot. Then install pChild as the right-child. */
  //����pRoot�е����ݣ���pChild��Ϊ�Һ��ӡ�
  zeroPage(pRoot, pChild->aData[0] & ~PTF_LEAF);
  put4byte(&pRoot->aData[pRoot->hdrOffset+8], pgnoChild);

  *ppChild = pChild;
  return SQLITE_OK;
}

/*
** The page that pCur currently points to has just been modified in
** some way. This function figures out if this modification means the
** tree needs to be balanced, and if so calls the appropriate balancing 
** routine. Balancing routines are:
** �α�pCur��ǰָ���ҳ����ĳ�ַ�ʽ���޸ġ�������Ū�����Ƿ�����޸���Ҫ��ƽ�⣬
** �Ƿ�����ʵ���ƽ�⺯����ƽ�⺯�����£�
**   balance_quick()
**   balance_deeper()
**   balance_nonroot()
*/
static int balance(BtCursor *pCur){
  int rc = SQLITE_OK;
  const int nMin = pCur->pBt->usableSize * 2 / 3;
  u8 aBalanceQuickSpace[13];
  u8 *pFree = 0;

  TESTONLY( int balance_quick_called = 0 );
  TESTONLY( int balance_deeper_called = 0 );

  do {
    int iPage = pCur->iPage;
    MemPage *pPage = pCur->apPage[iPage];

    if( iPage==0 ){
      if( pPage->nOverflow ){
        /* The root page of the b-tree is overfull. In this case call the
        ** balance_deeper() function to create a new child for the root-page
        ** and copy the current contents of the root-page to it. The
        ** next iteration of the do-loop will balance the child page.
		** B���ĸ�ҳ�ǹ����������������,����balance_deeper()����Ϊ��ҳ����һ���µĺ���
		** �����Ƶĵ�ǰ���ݸ�ҳ���ú���ҳ����һ������ѭ������ƽ����ҳ�档
        */ 
        assert( (balance_deeper_called++)==0 );
        rc = balance_deeper(pPage, &pCur->apPage[1]);
        if( rc==SQLITE_OK ){
          pCur->iPage = 1;
          pCur->aiIdx[0] = 0;
          pCur->aiIdx[1] = 0;
          assert( pCur->apPage[1]->nOverflow );
        }
      }else{
        break;
      }
    }else if( pPage->nOverflow==0 && pPage->nFree<=nMin ){
      break;
    }else{
      MemPage * const pParent = pCur->apPage[iPage-1];
      int const iIdx = pCur->aiIdx[iPage-1];

      rc = sqlite3PagerWrite(pParent->pDbPage);
      if( rc==SQLITE_OK ){
#ifndef SQLITE_OMIT_QUICKBALANCE
        if( pPage->hasData
         && pPage->nOverflow==1
         && pPage->aiOvfl[0]==pPage->nCell
         && pParent->pgno!=1
         && pParent->nCell==iIdx
        ){
          /* Call balance_quick() to create a new sibling of pPage on which
          ** to store the overflow cell. balance_quick() inserts a new cell
          ** into pParent, which may cause pParent overflow. If this
          ** happens, the next interation of the do-loop will balance pParent 
          ** use either balance_nonroot() or balance_deeper(). Until this
          ** happens, the overflow cell is stored in the aBalanceQuickSpace[]
          ** buffer. 
          ** ����balance_quick()������һ��pPage�����ֵ�ҳ��ҳ�ϴ洢�������Ԫ��balance_quick()����һ��
		  ** �µ�Ԫ��pParent,����ܻᵼ��pParent���.��������������,��һ������ѭ����佫��balance_nonroot()
		  ** ��balance_deeper()�е���һ��ƽ��pParent.ֱ�������Ԫ�洢��aBalanceQuickSpace[]��������
          ** The purpose of the following assert() is to check that only a
          ** single call to balance_quick() is made for each call to this
          ** function. If this were not verified, a subtle bug involving reuse
          ** of the aBalanceQuickSpace[] might sneak in.
		  ** �����assert()��Ŀ���Ǽ��,����ÿ�����ú���ֻ��һ������balance_quick()��������ǲ���֤,
		  ** aBalanceQuickSpace[]���õ�ʱ�򽫷���һ��΢��Ĵ���
          */
          assert( (balance_quick_called++)==0 );
          rc = balance_quick(pParent, pPage, aBalanceQuickSpace);
        }else
#endif
        {
          /* In this case, call balance_nonroot() to redistribute cells
          ** between pPage and up to 2 of its sibling pages. This involves
          ** modifying the contents of pParent, which may cause pParent to
          ** become overfull or underfull. The next iteration of the do-loop
          ** will balance the parent page to correct this.
          ** ** ����������£�����balance_nonroot()ʹ��Ԫ��pPage�����������ֵܽڵ���طֲ������漰��
		  ** �޸�pParent������,����ܵ���pParent������������һ�ε���ѭ����佫ƽ�⸸ҳ�档
          ** If the parent page becomes overfull, the overflow cell or cells
          ** are stored in the pSpace buffer allocated immediately below. 
          ** A subsequent iteration of the do-loop will deal with this by
          ** calling balance_nonroot() (balance_deeper() may be called first,
          ** but it doesn't deal with overflow cells - just moves them to a
          ** different page). Once this subsequent call to balance_nonroot() 
          ** has completed, it is safe to release the pSpace buffer used by
          ** the previous call, as the overflow cell data will have been 
          ** copied either into the body of a database page or into the new
          ** pSpace buffer passed to the latter call to balance_nonroot().
		  ** �����ҳ���ù���,�����Ԫ��洢��pSpace�������ĵ�Ԫ���������䡣
		  ** ����������ѭ����佫����balance_nonroot()������(balance_deeper()�������ȱ�����,����������
		  ** �����Ԫ����ֻ�������ƶ�����ͬ��ҳ��)��һ�������������balance_nonroot()���,�ͷ�֮ǰ��ʹ��
		  ** ��pSpace���������ǰ�ȫ��,��ʱ�����Ԫ���ݽ������Ƶ����ݿ�ҳ���������Ƶ��µ�pSpace��������
		  ** ��ʵ��ͨ����������balance_nonroot()���ݡ�
          */
          u8 *pSpace = sqlite3PageMalloc(pCur->pBt->pageSize);
          rc = balance_nonroot(pParent, iIdx, pSpace, iPage==1, pCur->hints);
          if( pFree ){
            /* If pFree is not NULL, it points to the pSpace buffer used 
            ** by a previous call to balance_nonroot(). Its contents are
            ** now stored either on real database pages or within the 
            ** new pSpace buffer, so it may be safely freed here. 
			** ** ���pFree����NULL,��ָ��֮ǰ��balance_nonroot()���õ�pSpace������.�����������ڴ洢��ʵ��
			** ���ݿ�ҳ�����pSpace��������,����������԰�ȫ���ͷš�*/
            sqlite3PageFree(pFree);
          }

          /* The pSpace buffer will be freed after the next call to
          ** balance_nonroot(), or just before this function returns, whichever
          ** comes first. 
		  ** pSpace������������һ�ε���ʱ���ͷ�,���������������֮ǰ��*/
          pFree = pSpace;
        }
      }

      pPage->nOverflow = 0;

      /* The next iteration of the do-loop balances the parent page. */  //��һ������ѭ����������ҳ��ƽ��
      releasePage(pPage);
      pCur->iPage--;
    }
  }while( rc==SQLITE_OK );

  if( pFree ){
    sqlite3PageFree(pFree);
  }
  return rc;
}


/*
** Insert a new record into the BTree.  The key is given by (pKey,nKey)
** and the data is given by (pData,nData).  The cursor is used only to
** define what table the record should be inserted into.  The cursor
** is left pointing at a random location.
** ����һ���¼�¼��B��.�ؼ�����(pKey,nKey)��������������(pData,nData)����.
** �α���������������¼Ӧ�ò��뵽ʲô���У������α�ָ������λ��.
** For an INTKEY table, only the nKey value of the key is used.  pKey is
** ignored.  For a ZERODATA table, the pData and nData are both ignored.
** ����һ��INTKEY��ֻ�йؼ��ֵ�nKeyֵ���ã�pKey���ԡ�����ZERODATA��pData��nData�����á�
** If the seekResult parameter is non-zero, then a successful call to
** MovetoUnpacked() to seek cursor pCur to (pKey, nKey) has already
** been performed. seekResult is the search result returned (a negative
** number if pCur points at an entry that is smaller than (pKey, nKey), or
** a positive value if pCur points at an etry that is larger than 
** (pKey, nKey)). 
** ���seekResult��������,��ôһ���ɹ��ĵ���MovetoUnpacked()Ѱ��(pKey nKey)�Ѿ���ִ�е��α�pCur��
** seekResult���������صĽ��(���pCurָ��һ����(pKey, nKey)��С����Ŀ����һ����ֵ,�����pCurָ��
** һ����(pKey nKey)�����ֵ���ֵΪ��ֵ.)��
** If the seekResult parameter is non-zero, then the caller guarantees that
** cursor pCur is pointing at the existing copy of a row that is to be
** overwritten.  If the seekResult parameter is 0, then cursor pCur may
** point to any entry or to no entry at all and so this function has to seek
** the cursor before the new key can be inserted.
** ���seekResult�����Ƿ���,��ô�����߱�֤�α�pCur��һ�б���д�����и��������seekResult������0,��ô
** �α�pCur����ָ���κ���Ŀ��ָ���κ���Ŀ,�����ڲ����ֵ֮ǰ�����������Ѱ���αꡣ
*/
/*
��Btree�в���һ���¼�¼��
�ؼ��ְ���(pKey,nKey)���������ݰ���(pData,nData)������
�ҵ�������λ��
�����ڴ�ռ�
������
*/
int sqlite3BtreeInsert(          //�����¼�¼��B��
  BtCursor *pCur,                /* Insert data into the table of this cursor */  //�������ݵ��α�ָ��ı�
  const void *pKey, i64 nKey,    /* The key of the new record */                  //�¼�¼�ļ�ֵ
  const void *pData, int nData,  /* The data of the new record */                 //�¼�¼������
  int nZero,                     /* Number of extra 0 bytes to append to data */  //���ӵ����ݵĶ����0�ֽ���
  int appendBias,                /* True if this is likely an append */           //����Ǹ��ӵ���Ϊtrue
  int seekResult                 /* Result of prior MovetoUnpacked() call */      //��ǰ����MovetoUnpacked()�Ľ��
){
  int rc;
  int loc = seekResult;          /* -1: before desired location  +1: after */     //-1:ϣ����λ��֮ǰ  +1:֮��
  int szNew = 0;
  int idx;
  MemPage *pPage;
  Btree *p = pCur->pBtree;
  BtShared *pBt = p->pBt;
  unsigned char *oldCell;
  unsigned char *newCell = 0;

  if( pCur->eState==CURSOR_FAULT ){
    assert( pCur->skipNext!=SQLITE_OK );
    return pCur->skipNext;
  }

  assert( cursorHoldsMutex(pCur) );
  assert( pCur->wrFlag && pBt->inTransaction==TRANS_WRITE
              && (pBt->btsFlags & BTS_READ_ONLY)==0 );
  assert( hasSharedCacheTableLock(p, pCur->pgnoRoot, pCur->pKeyInfo!=0, 2) );

  /* Assert that the caller has been consistent. If this cursor was opened
  ** expecting an index b-tree, then the caller should be inserting blob
  ** keys with no associated data. If the cursor was opened expecting an
  ** intkey table, the caller should be inserting integer keys with a
  ** blob of associated data.  
  ** ���Ե�������һ�µġ��������α걻�򿪵�B������,��ô������Ӧ�ò���û���������
  ** ��blob��������α걻���Ŷ���intkey��,������Ӧ�ò������������ݵ�blob����������*/
  assert( (pKey==0)==(pCur->pKeyInfo==0) );

  /* Save the positions of any other cursors open on this table.
  ** �����ڱ��ϴ򿪵��κ������α��λ�á�
  ** In some cases, the call to btreeMoveto() below is a no-op. For
  ** example, when inserting data into a table with auto-generated integer
  ** keys, the VDBE layer invokes sqlite3BtreeLast() to figure out the 
  ** integer key to use. It then calls this function to actually insert the 
  ** data into the intkey B-Tree. In this case btreeMoveto() recognizes
  ** that the cursor is already where it needs to be and returns without
  ** doing any work. To avoid thwarting(����) these optimizations, it is important
  ** not to clear the cursor here.
  ** ��ĳЩ�����,�����btreeMoveto()�ĵ�����������ġ�����,�������ݲ����Զ����ɵ��������ı�ʱ,
  ** VDBE�����sqlite3BtreeLast()���Ҫʹ�õ���������Ȼ���������������������ݵ�intkeyB�������������
  ** ��btreeMoveto()ʶ���α��Ѿ�����Ҫ���ĵط�,�����ء�Ϊ�˱���Ӱ����Щ�Ż�,�����������α��Ǻ���Ҫ�ġ�
  */ 
  rc = saveAllCursors(pBt, pCur->pgnoRoot, pCur);     /*���������α�*/
  if( rc ) return rc;

  /* If this is an insert into a table b-tree, invalidate any incrblob 
  ** cursors open on the row being replaced (assuming this is a replace
  ** operation - if it is not, the following is a no-op).  
  ** ������뵽��B����ʹ�ڱ��滻�����ϵ��κο����Եĵ���blob�α�.(��������һ���滻����,������ǣ����޲���.)*/
  if( pCur->pKeyInfo==0 ){
    invalidateIncrblobCursors(p, nKey, 0);   //ʹ���ŵ��л����е�һ�����޸ĵ�һ��incrblob�α���Ч
  }

  if( !loc ){
    rc = btreeMoveto(pCur, pKey, nKey, appendBias, &loc);
    if( rc ) return rc;
  }
  assert( pCur->eState==CURSOR_VALID || (pCur->eState==CURSOR_INVALID && loc) );

  pPage = pCur->apPage[pCur->iPage];
  assert( pPage->intKey || nKey>=0 );
  assert( pPage->leaf || !pPage->intKey );

  TRACE(("INSERT: table=%d nkey=%lld ndata=%d page=%d %s\n",
          pCur->pgnoRoot, nKey, nData, pPage->pgno,
          loc==0 ? "overwrite" : "new entry"));
  assert( pPage->isInit );
  allocateTempSpace(pBt); /*ȷ��pBtָ��MX_CELL_SIZE(pBt)�ֽ�*/
  newCell = pBt->pTmpSpace;
  if( newCell==0 ) return SQLITE_NOMEM;
  rc = fillInCell(pPage, newCell, pKey, nKey, pData, nData, nZero, &szNew);
  if( rc ) goto end_insert;
  assert( szNew==cellSizePtr(pPage, newCell) );
  assert( szNew <= MX_CELL_SIZE(pBt) );
  idx = pCur->aiIdx[pCur->iPage];
  if( loc==0 ){
    u16 szOld;
    assert( idx<pPage->nCell );
    rc = sqlite3PagerWrite(pPage->pDbPage);
    if( rc ){
      goto end_insert;
    }
    oldCell = findCell(pPage, idx);
    if( !pPage->leaf ){
      memcpy(newCell, oldCell, 4);
    }
    szOld = cellSizePtr(pPage, oldCell);
    rc = clearCell(pPage, oldCell);   //�ͷ��κ��������Ԫ��ص����ҳ
    dropCell(pPage, idx, szOld, &rc); //ɾ��pPage�ĵ�i����Ԫ.
    if( rc ) goto end_insert;
  }else if( loc<0 && pPage->nCell>0 ){
    assert( pPage->leaf );
    idx = ++pCur->aiIdx[pCur->iPage];
  }else{
    assert( pPage->leaf );
  }
  insertCell(pPage, idx, newCell, szNew, 0, 0, &rc); //��pPage�ĵ�Ԫ����i������һ���µ�Ԫ.pCellָ��Ԫ������
  assert( rc!=SQLITE_OK || pPage->nCell>0 || pPage->nOverflow>0 );

  /* If no error has occured and pPage has an overflow cell, call balance() 
  ** to redistribute the cells within the tree. Since balance() may move
  ** the cursor, zero the BtCursor.info.nSize and BtCursor.validNKey
  ** variables.
  ** ���û�д�����,��pPage�������Ԫ,����balance()���·ֲ����ڵĵ�Ԫ����Ϊbalance()
  ** �����ƶ��α�,��������BtCursor.info.nSize��BtCursor.validNKey����������
  ** Previous versions of SQLite called moveToRoot() to move the cursor
  ** back to the root page as balance() used to invalidate the contents
  ** of BtCursor.apPage[] and BtCursor.aiIdx[]. Instead of doing that,
  ** set the cursor state to "invalid". This makes common insert operations
  ** slightly faster.
  ** ��balance()����ʹBtCursor������Чʱ����ǰSQLite�İ汾����moveToRoot()���ƶ��α�ص���ҳ������
  ** ʹBtCursor.apPage[]��BtCursor.aiIdx[]��������Ч���෴,���α�״̬����Ϊ��invalid������ʹ��
  ** �����Ĳ���������졣
  ** There is a subtle but important optimization here too. When inserting
  ** multiple records into an intkey b-tree using a single cursor (as can
  ** happen while processing an "INSERT INTO ... SELECT" statement), it
  ** is advantageous to leave the cursor pointing to the last entry in
  ** the b-tree if possible. If the cursor is left pointing to the last
  ** entry in the table, and the next row inserted has an integer key
  ** larger than the largest existing key, it is possible to insert the
  ** row without seeking the cursor. This can be a big performance boost.
  ** ������һ��΢С����Ҫ���Ż�������һ���α��������¼��һ��intkeyB��ʱʹ(�ڴ���һ��
  ** "INSERT INTO ... SELECT"���),������ܵĻ�,ʹ�α�ָ��������һ����Ŀ���������ġ�
  ** ����α�ָ��������һ����Ŀ,��һ�в�����һ�����Ѵ��ڵ�����ֵҪ���������ֵ,������
  ** ��û���α�ĵ��в��롣����Լ����������ܡ�
  */
  pCur->info.nSize = 0;
  pCur->validNKey = 0;
  if( rc==SQLITE_OK && pPage->nOverflow ){   /*pPage������ĵ�Ԫ�񣬵���balance(pCur)ƽ��B��*/
    rc = balance(pCur);

    /* Must make sure nOverflow is reset to zero even if the balance()
    ** fails. Internal data structure corruption will result otherwise. 
    ** Also, set the cursor state to invalid. This stops saveCursorPosition()
    ** from trying to save the current position of the cursor.  
	** ����ȷ��nOverflow��λΪ��,��ʹbalance()ʧ��.�ڲ����ݽṹ�������������������ҲҪ�����α�״̬Ϊ��Ч��
	** �⽫ʹsaveCursorPosition()����ͼ���浱ǰ����λ��ֹͣ*/
    pCur->apPage[pCur->iPage]->nOverflow = 0;
    pCur->eState = CURSOR_INVALID;
  }
  assert( pCur->apPage[pCur->iPage]->nOverflow==0 );

end_insert:
  return rc;
}

/*
** Delete the entry that the cursor is pointing to.  The cursor
** is left pointing at a arbitrary location. 
** ɾ���α�ָ�����Ŀ��ʹָ֮������λ�á�
*/
/*ɾ���α���ָ��¼*/
int sqlite3BtreeDelete(BtCursor *pCur){    //ɾ���α�ָ�����Ŀ��ʹָ֮������λ��
  Btree *p = pCur->pBtree;
  BtShared *pBt = p->pBt;              
  int rc;                              /* Return code */                     //���ش���
  MemPage *pPage;                      /* Page to delete cell from */        //Ҫɾ����Ԫ���ڵ�ҳ
  unsigned char *pCell;                /* Pointer to cell to delete */       //��Ҫɾ����Ԫ��ָ��
  int iCellIdx;                        /* Index of cell to delete */         //Ҫɾ����Ԫ������
  int iCellDepth;                      /* Depth of node containing pCell */  //����pCell�ĵ�Ԫ���

  assert( cursorHoldsMutex(pCur) );
  assert( pBt->inTransaction==TRANS_WRITE );
  assert( (pBt->btsFlags & BTS_READ_ONLY)==0 );
  assert( pCur->wrFlag );
  assert( hasSharedCacheTableLock(p, pCur->pgnoRoot, pCur->pKeyInfo!=0, 2) );
  assert( !hasReadConflicts(p, pCur->pgnoRoot) );

  if( NEVER(pCur->aiIdx[pCur->iPage]>=pCur->apPage[pCur->iPage]->nCell) 
   || NEVER(pCur->eState!=CURSOR_VALID)
  ){
    return SQLITE_ERROR;  /* Something has gone awry. */   
  }

  iCellDepth = pCur->iPage;
  iCellIdx = pCur->aiIdx[iCellDepth];
  pPage = pCur->apPage[iCellDepth];
  pCell = findCell(pPage, iCellIdx);

  /* If the page containing the entry to delete is not a leaf page, move
  ** the cursor to the largest entry in the tree that is smaller than
  ** the entry being deleted. This cell will replace the cell being deleted
  ** from the internal node. The 'previous' entry is used for this instead
  ** of the 'next' entry, as the previous entry is always a part of the
  ** sub-tree headed by the child page of the cell being deleted. This makes
  ** balancing the tree following the delete operation easier.  
  ** ���ҳ�����Ҫɾ������Ŀ����Ҷ��ҳ��,�ƶ��α굽���б�Ҫɾ����ĿС��������Ŀ��
  ** �����Ԫ��ȡ�����ڲ��ڵ㱻ɾ���ĵ�Ԫ����ǰ����Ŀ���ڴ˴��桰next����Ŀ,��Ϊǰ��
  ** ����Ŀ����Ҫɾ����Ԫ�ĺ���ҳ�����������һ���֡���ʹ��ɾ������������ƽ�����������ס�*/
  if( !pPage->leaf ){
    int notUsed;
    rc = sqlite3BtreePrevious(pCur, &notUsed);  //��ʹ�α�ص����ݿ�����ǰ����Ŀ
    if( rc ) return rc;
  }

  /* Save the positions of any other cursors open on this table before
  ** making any modifications. Make the page containing the entry to be 
  ** deleted writable. Then free any overflow pages associated with the 
  ** entry and finally remove the cell itself from within the page. 
  ** ���κ��޸�֮ǰ���������������ڴ˱��Ͽ��ŵ��α��λ�á�ʹҳ�����Ҫɾ���Ŀ�д����Ŀ��
  ** Ȼ���ͷ��κ�����Ŀ��ص����ҳ,���ɾ��ҳ���ڵĵ�Ԫ����
  */
  rc = saveAllCursors(pBt, pCur->pgnoRoot, pCur);/*�޸�֮ǰ�������д򿪵��α�*/
  if( rc ) return rc;

  /* If this is a delete operation to remove a row from a table b-tree,
  ** invalidate any incrblob cursors open on the row being deleted.  
  ** ����Ǵ�һ��B������ɾ��һ�е�ɾ������,ʹ�����ϱ�ɾ���Ĵ򿪵��κ�incrblob�α���Ч��*/
  if( pCur->pKeyInfo==0 ){
    invalidateIncrblobCursors(p, pCur->info.nKey, 0);/*���Ϊɾ��������ʹ����incrblob�α���Ч*/
  }

  rc = sqlite3PagerWrite(pPage->pDbPage);
  if( rc ) return rc;
  rc = clearCell(pPage, pCell);
  dropCell(pPage, iCellIdx, cellSizePtr(pPage, pCell), &rc);
  if( rc ) return rc;

  /* If the cell deleted was not located on a leaf page, then the cursor
  ** is currently pointing to the largest entry in the sub-tree headed
  ** by the child-page of the cell that was just deleted from an internal
  ** node. The cell from the leaf node needs to be moved to the internal
  ** node to replace the deleted cell. 
  ** ���ɾ����Ԫ����λ��Ҷ��ҳ��,Ȼ���α굱ǰָ�������е�������Ŀ�������Ŀ�������б���
  ** һ���ڲ��ڵ���ɾ���ĵ�Ԫ�ĺ���ҳ����ӡ�Ҷ�ڵ�ĵ�Ԫ��Ҫ�ƶ����ڲ��ڵ��滻ɾ���ĵ�Ԫ��*/
  if( !pPage->leaf ){
    MemPage *pLeaf = pCur->apPage[pCur->iPage];
    int nCell;
    Pgno n = pCur->apPage[iCellDepth+1]->pgno;
    unsigned char *pTmp;

    pCell = findCell(pLeaf, pLeaf->nCell-1);
    nCell = cellSizePtr(pLeaf, pCell);
    assert( MX_CELL_SIZE(pBt) >= nCell );

    allocateTempSpace(pBt);
    pTmp = pBt->pTmpSpace;

    rc = sqlite3PagerWrite(pLeaf->pDbPage);
    insertCell(pPage, iCellIdx, pCell-4, nCell+4, pTmp, n, &rc);/*Ҷ�ӽ���ϵĵ�Ԫ���Ƶ��ڲ�������ɾ���ĵ�Ԫ��*/
    dropCell(pLeaf, pLeaf->nCell-1, nCell, &rc);
    if( rc ) return rc;
  }

  /* Balance the tree. If the entry deleted was located on a leaf page,
  ** then the cursor still points to that page. In this case the first
  ** call to balance() repairs the tree, and the if(...) condition is
  ** never true.
  ** ƽ���������ɾ������Ŀλ��Ҷ��ҳ��,��ô�α���ָ�����ҳ�档�����������,
  ** ��һ�ε���balance()������������if(...)��������Ϊtrue��
  ** Otherwise, if the entry deleted was on an internal node page, then
  ** pCur is pointing to the leaf page from which a cell was removed to
  ** replace the cell deleted from the internal node. This is slightly
  ** tricky as the leaf node may be underfull, and the internal node may
  ** be either under or overfull. In this case run the balancing algorithm
  ** on the leaf node first. If the balance proceeds far enough up the
  ** tree that we can be sure that any problem in the internal node has
  ** been corrected, so be it. Otherwise, after balancing the leaf node,
  ** walk the cursor up the tree to the internal node and balance it as 
  ** well.  
  ** ���Ҫɾ������Ŀ����һ���ڲ��ڵ�ҳ�ϣ���ôpCurָ��Ҷ��ҳ�棬��Ҷ��ҳ���ϵĵ�Ԫ
  ** ���Ƴ��滻�ڲ��ڵ��б�ɾ���ĵ�Ԫ.��������е㼬�֣���ΪҶ�ڵ���ܲ����������ڲ��ڵ�
  ** ������Ҳ���ܲ���.��ʱ������Ҷ�ڵ�������ƽ���㷨.���ƽ������㹻,���ǿ��Կ϶�,���ڲ���
  ** ���κ����ⶼ������,����ִ��.����,Ҷ�ӽڵ�ƽ���, �����α���������ڲ��ڵ㲢ƽ����.*/
  rc = balance(pCur);
  if( rc==SQLITE_OK && pCur->iPage>iCellDepth ){
    while( pCur->iPage>iCellDepth ){
      releasePage(pCur->apPage[pCur->iPage--]);
    }
    rc = balance(pCur);
  }

  if( rc==SQLITE_OK ){
    moveToRoot(pCur);
  }
  return rc;
}

/*
** Create a new BTree table.  Write into *piTable the page
** number for the root page of the new table.
** �����µ�B�������±��ҳ��ҳ��д��*piTable�С�
** The type of type is determined by the flags parameter.  Only the
** following values of flags are currently in use.  Other values for
** flags might not work:
** �����ɱ�־������������־����ֻ�����µĿ��á�������־����û������.
**     BTREE_INTKEY|BTREE_LEAFDATA     Used for SQL tables with rowid keys   //�ñ�ǩ���ڴ�����id��ֵ��SQL��
**     BTREE_ZERODATA                  Used for SQL indices                  //�ñ�ǩ����SQL����
*/
/*
����һ��btree���
�ƶ��������ݿ�Ϊ�±�ĸ�ҳ���ڳ��ռ�
���¸�ҳ����ӳ��Ĵ�����metadata
�±��ҳ��ҳ�ŷ���PgnoRoot��д��piTable
*/
static int btreeCreateTable(Btree *p, int *piTable, int createTabFlags){ //�����µ�B����.�±��ҳҳ��д��*piTable
  BtShared *pBt = p->pBt;
  MemPage *pRoot;
  Pgno pgnoRoot;
  int rc;
  int ptfFlags;          /* Page-type flage for the root page of new table */ //�±��ҳ��ҳ���ͱ�ǩ

  assert( sqlite3BtreeHoldsMutex(p) );
  assert( pBt->inTransaction==TRANS_WRITE );
  assert( (pBt->btsFlags & BTS_READ_ONLY)==0 );

#ifdef SQLITE_OMIT_AUTOVACUUM
  rc = allocateBtreePage(pBt, &pRoot, &pgnoRoot, 1, 0);/*����һ��B��ҳ��*/
  if( rc ){
    return rc;
  }
#else
  if( pBt->autoVacuum ){
    Pgno pgnoMove;      /* Move a page here to make room for the root-page */ //�ƶ�һ��ҳ���˴�Ϊ��ҳ�ڳ��ռ�
    MemPage *pPageMove; /* The page to move to. */    //Ҫ�ƶ���ҳ

    /* Creating a new table may probably require moving an existing database
    ** to make room for the new tables root page. In case this page turns
    ** out to be an overflow page, delete all overflow page-map caches
    ** held by open cursors.
	** ����һ���±����Ҫ�ƶ�һ�����ڵ����ݿ���Ϊ�±�ĸ�ҳ�ڳ��ռ䡣�����ҳ��һ�����ҳ����ôɾ���������α�
	** ӵ�е��������ҳӳ�仺�档
    */
    invalidateAllOverflowCache(pBt); //�ڹ���B���ṹpBt�ϣ������д򿪵��α�ʹ���ҳ�б���Ч

    /* Read the value of meta[3] from the database to determine where the
    ** root page of the new table should go. meta[3] is the largest root-page
    ** created so far, so the new root-page is (meta[3]+1).
	** �����ݿ��ж�ȡmeta[3]��ֵ�������±�ĸ�ҳӦ�ڵ�λ��.meta[3]��Ŀǰ�Դ��������ĸ�ҳ������¸�ҳ��meta[3]+1.
    */
    sqlite3BtreeGetMeta(p, BTREE_LARGEST_ROOT_PAGE, &pgnoRoot);/*��ȡ����ҳmeta*/
    pgnoRoot++; /*��ҳ=��ҳ+1*/

    /* The new root-page may not be allocated on a pointer-map page, or the
    ** PENDING_BYTE page. //�¸�ҳҲ��û����ָ��λͼҳ��PENDING_BYTEҳ�Ϸ��䡣
    */
    while( pgnoRoot==PTRMAP_PAGENO(pBt, pgnoRoot) ||
        pgnoRoot==PENDING_BYTE_PAGE(pBt) ){
      pgnoRoot++;/*�µĸ�ҳ������pointer-map page����PENDING_BYTE page*/
    }
    assert( pgnoRoot>=3 );

    /* Allocate a page. The page that currently resides at pgnoRoot will
    ** be moved to the allocated page (unless the allocated page happens
    ** to reside at pgnoRoot).
	** ����һ��ҳ.��ǰפ���� pgnoRoot��ҳ���ƶ��������ҳ(���Ƿ����ҳ�Ѿ�פ���� pgnoRoot��).
    */
    rc = allocateBtreePage(pBt, &pPageMove, &pgnoMove, pgnoRoot,1);//�����ݿ��ļ�����һ����ҳ��,�ɹ��򷵻�SQLITE_OK
    if( rc!=SQLITE_OK ){
      return rc;
    }

    if( pgnoMove!=pgnoRoot ){
      /* pgnoRoot is the page that will be used for the root-page of
      ** the new table (assuming an error did not occur). But we were
      ** allocated pgnoMove. If required (i.e. if it was not allocated
      ** by extending the file), the current page at position pgnoMove
      ** is already journaled.
	  ** pgnoRoot�ǽ��������±��ҳ��ҳ��(����û�з�������Ļ�)��������pgnoMove��
	  ** �����Ҫ(�����û�б���չ�ļ�����),��ô��ǰҳ��λ��pgnoMove��¼����־��
      */
      u8 eType = 0;
      Pgno iPtrPage = 0;

      releasePage(pPageMove);

      /* Move the page currently at pgnoRoot to pgnoMove. */  //�ƶ���ǰ��pgnoRoot��ҳ�浽pgnoMove.
      rc = btreeGetPage(pBt, pgnoRoot, &pRoot, 0); //��ҳ����õ�һ��ҳ.����Ҫ,���ʼ��MemPage.pBt��MemPage.aData
      if( rc!=SQLITE_OK ){
        return rc;
      }
      rc = ptrmapGet(pBt, pgnoRoot, &eType, &iPtrPage);  //��ָ��λͼ��ȡ
      if( eType==PTRMAP_ROOTPAGE || eType==PTRMAP_FREEPAGE ){
        rc = SQLITE_CORRUPT_BKPT;
      }
      if( rc!=SQLITE_OK ){
        releasePage(pRoot);
        return rc;
      }
      assert( eType!=PTRMAP_ROOTPAGE );
      assert( eType!=PTRMAP_FREEPAGE );
      rc = relocatePage(pBt, pRoot, eType, iPtrPage, pgnoMove, 0); //�ƶ��������ݿ�ҳpRoot��Ҫ���λ��pgnoMove
      releasePage(pRoot);

      /* Obtain the page at pgnoRoot */  //�����pgnoRoot�ϵ�ҳ
      if( rc!=SQLITE_OK ){
        return rc;
      }
      rc = btreeGetPage(pBt, pgnoRoot, &pRoot, 0);
      if( rc!=SQLITE_OK ){
        return rc;
      }
      rc = sqlite3PagerWrite(pRoot->pDbPage);
      if( rc!=SQLITE_OK ){
        releasePage(pRoot);
        return rc;
      }
    }else{
      pRoot = pPageMove;
    } 

    /* Update the pointer-map and meta-data with the new root-page number. */
	//���´����¸�ҳ��ָ��λͼ��Ԫ����
    ptrmapPut(pBt, pgnoRoot, PTRMAP_ROOTPAGE, 0, &rc);
    if( rc ){
      releasePage(pRoot);
      return rc;
    }

    /* When the new root page was allocated, page 1 was made writable in
    ** order either to increase the database filesize, or to decrement the
    ** freelist count.  Hence, the sqlite3BtreeUpdateMeta() call cannot fail.
	** ���¸�ҳ�����ʱ,��һҳ�ǿ�д��Ϊ���������ݿ��ļ���С,������ٿհ��б�ļ�����
	** ���,sqlite3BtreeUpdateMeta()���ò���ʧ�ܡ�
    */
    assert( sqlite3PagerIswriteable(pBt->pPage1->pDbPage) );
    rc = sqlite3BtreeUpdateMeta(p, 4, pgnoRoot);/*�������ݿ��ļ���С�����ٿ����б������*/
    if( NEVER(rc) ){
      releasePage(pRoot);
      return rc;
    }

  }else{
    rc = allocateBtreePage(pBt, &pRoot, &pgnoRoot, 1, 0);  //�����ݿ��ļ�����һ����ҳ
    if( rc ) return rc;
  }
#endif
  assert( sqlite3PagerIswriteable(pRoot->pDbPage) );
  if( createTabFlags & BTREE_INTKEY ){
    ptfFlags = PTF_INTKEY | PTF_LEAFDATA | PTF_LEAF;
  }else{
    ptfFlags = PTF_ZERODATA | PTF_LEAF;
  }
  zeroPage(pRoot, ptfFlags);
  sqlite3PagerUnref(pRoot->pDbPage);
  assert( (pBt->openFlags & BTREE_SINGLE)==0 || pgnoRoot==2 );
  *piTable = (int)pgnoRoot; /*���±��ҳд��piTable*/
  return SQLITE_OK;
}

/*�����ݿ��д���һ����B��������ͼ��ʽ��B+������������ʽ��B����*/
int sqlite3BtreeCreateTable(Btree *p, int *piTable, int flags){ 
  int rc;
  sqlite3BtreeEnter(p);
  rc = btreeCreateTable(p, piTable, flags); //�����µ�B����.�±��ҳҳ��д��*piTable
  sqlite3BtreeLeave(p);
  return rc;
}

/*
** Erase the given database page and all its children.  Return
** the page to the freelist.
** �������������ݿ�ҳ�������к��ӽڵ�.����ҳ���б�ҳ.
*/
static int clearDatabasePage(    //�������������ݿ�ҳ�������к��ӽڵ�.����ҳ���б�ҳ.
  BtShared *pBt,           /* The BTree that contains the table */          //�������B��
  Pgno pgno,               /* Page number to clear */                       //����ҳ��
  int freePageFlag,        /* Deallocate page if true */                    //���Ϊtrue,�ͷ�ҳ
  int *pnChange            /* Add number of Cells freed to this counter */  //��ӵ�Ԫ�ͷŴ˼�����������
){
  MemPage *pPage;
  int rc;
  unsigned char *pCell;
  int i;

  assert( sqlite3_mutex_held(pBt->mutex) );
  if( pgno>btreePagecount(pBt) ){
    return SQLITE_CORRUPT_BKPT;
  }

  rc = getAndInitPage(pBt, pgno, &pPage);  //��ҳ�����л��һ��ҳ�沢��ʼ��
  if( rc ) return rc;
  for(i=0; i<pPage->nCell; i++){
    pCell = findCell(pPage, i);
    if( !pPage->leaf ){
      rc = clearDatabasePage(pBt, get4byte(pCell), 1, pnChange);
      if( rc ) goto cleardatabasepage_out;
    }
    rc = clearCell(pPage, pCell);
    if( rc ) goto cleardatabasepage_out;
  }
  if( !pPage->leaf ){
    rc = clearDatabasePage(pBt, get4byte(&pPage->aData[8]), 1, pnChange);
    if( rc ) goto cleardatabasepage_out;
  }else if( pnChange ){
    assert( pPage->intKey );
    *pnChange += pPage->nCell;
  }
  if( freePageFlag ){
    freePage(pPage, &rc);
  }else if( (rc = sqlite3PagerWrite(pPage->pDbPage))==0 ){
    zeroPage(pPage, pPage->aData[0] | PTF_LEAF);
  }

cleardatabasepage_out:
  releasePage(pPage);
  return rc;
}

/*
** Delete all information from a single table in the database.  iTable is
** the page number of the root of the table.  After this routine returns,
** the root page is empty, but still exists.
** �����ݿ��е�һ������ɾ��������Ϣ��iTable�Ǳ��и���ҳ�š�����������غ�,��ҳ���ǿյ�,����Ȼ���ڡ�
** This routine will fail with SQLITE_LOCKED if there are any open
** read cursors on the table.  Open write cursors are moved to the
** root of the table.
** ����ڱ���������κο������α���ô�������ʧ�ܷ���SQLITE_LOCKED.���ŵ�д�α걻������ĸ�ҳ��
** If pnChange is not NULL, then table iTable must be an intkey table. The
** integer value pointed to by pnChange is incremented by the number of
** entries in the table.
** ���pnChange�ǿ�,��ô��ITableһ����һ��intkey��.pnChangeָ�����������ֵ�����Ǹ��ݱ�����Ŀ���������ӵ�.
*/
/*
ɾ��B-tree�����е����ݣ�������B-tree�ṹ������
*/
int sqlite3BtreeClearTable(Btree *p, int iTable, int *pnChange){  //ɾ��B-tree�����е����ݣ�������B-tree�ṹ����
  int rc;
  BtShared *pBt = p->pBt;
  sqlite3BtreeEnter(p);
  assert( p->inTrans==TRANS_WRITE );

  rc = saveAllCursors(pBt, (Pgno)iTable, 0);

  if( SQLITE_OK==rc ){
    /* Invalidate all incrblob cursors open on table iTable (assuming iTable
    ** is the root of a table b-tree - if it is not, the following call is
    ** a no-op).  
	** �ڱ�ITable��ʹ���ŵ�incrblob�α���Ч.(�ٶ�ITable��B���ĸ�ҳ�������������ĵ����޲���)*/
    invalidateIncrblobCursors(p, 0, 1);
    rc = clearDatabasePage(pBt, (Pgno)iTable, 0, pnChange);
  }
  sqlite3BtreeLeave(p);
  return rc;
}

/*
** Erase all information in a table and add the root of the table to
** the freelist.  Except, the root of the principle table (the one on
** page 1) is never added to the freelist.
** ������ϵ�������Ϣ������ӱ�ĸ��������б�����֮�⣬��Դ��(һ����ҳ1�ϵı�)�ĸ��Ӳ����뵽�����б�.
** This routine will fail with SQLITE_LOCKED if there are any open
** cursors on the table.
** ����ڱ���������κο������α���ô�������ʧ�ܷ���SQLITE_LOCKED.
** If AUTOVACUUM is enabled and the page at iTable is not the last
** root page in the database file, then the last root page 
** in the database file is moved into the slot formerly occupied by
** iTable and that last slot formerly occupied by the last root page
** is added to the freelist instead of iTable.  In this say, all
** root pages are kept at the beginning of the database file, which
** is necessary for AUTOVACUUM to work right.  *piMoved is set to the 
** page number that used to be the last root page in the file before
** the move.  If no page gets moved, *piMoved is set to 0.
** The last root page is recorded in meta[3] and the value of
** meta[3] is updated by this procedure.
** ���AUTOVACUUM���ò���iTable�ϵ�ҳ�������ݿ��ļ�������ҳ����ô���ݿ��ļ������ĸ�ҳ�����ƶ���
** ��iTableռ�õ�λ�ò����ϴα���ҳռ�õļӵ������б������iTbale��Ҳ����˵�����еĸ�ҳ���ݿ��ļ��Ŀ�ʼ��
** �����AUTOVACUUM�����������б�Ҫ�ġ�*piMoved������Ϊ�ƶ�֮ǰ�ļ���������ҳ��ҳ��.���û��ҳҪ�ƶ���
** ��*piMoved��Ϊ0.���ĸ�ҳ�Ǽ�¼��meta[3]�в���meta[3]��ֵ����������б����¡�
*/
static int btreeDropTable(Btree *p, Pgno iTable, int *piMoved){   //������ϵ�������Ϣ������ӱ�ĸ��������б�
  int rc;
  MemPage *pPage = 0;
  BtShared *pBt = p->pBt;

  assert( sqlite3BtreeHoldsMutex(p) );
  assert( p->inTrans==TRANS_WRITE );

  /* It is illegal to drop a table if any cursors are open on the
  ** database. This is because in auto-vacuum mode the backend may
  ** need to move another root-page to fill a gap left by the deleted
  ** root page. If an open cursor was using this page a problem would 
  ** occur.
  ** ������ݿ��ϵ��κ��α꿪���ˣ���ô�����ǷǷ��ġ�������Ϊ��auto-vacumģʽ�º�˿�����Ҫ�ƶ�
  ** ����һ����ҳ�����ͨ��ɾ����ҳ���µķ�϶�����һ���򿪵��α���ҳ������ʹ�ã���ô�����������.
  ** This error is caught long before control reaches this point.
  */
  if( NEVER(pBt->pCursor) ){
    sqlite3ConnectionBlocked(p->db, pBt->pCursor->pBtree->db);
    return SQLITE_LOCKED_SHAREDCACHE;/*�α겻��Ϊ��״̬*/
  }

  rc = btreeGetPage(pBt, (Pgno)iTable, &pPage, 0);
  if( rc ) return rc;
  rc = sqlite3BtreeClearTable(p, iTable, 0);
  if( rc ){
    releasePage(pPage);
    return rc;
  }

  *piMoved = 0;

  if( iTable>1 ){ /*ҳ�������1�����ܱ�ɾ��*/
#ifdef SQLITE_OMIT_AUTOVACUUM
    freePage(pPage, &rc);
    releasePage(pPage);
#else
    if( pBt->autoVacuum ){
      Pgno maxRootPgno;
      sqlite3BtreeGetMeta(p, BTREE_LARGEST_ROOT_PAGE, &maxRootPgno);  //�����ݿ��ļ���Ԫ������Ϣ

      if( iTable==maxRootPgno ){
        /* If the table being dropped is the table with the largest root-page
        ** number in the database, put the root page on the free list. 
		** �����ɾ���ı������ݿ���������ҳ��ı���ô�Ѹ�ҳ�ŵ������б�.
        */
        freePage(pPage, &rc);
        releasePage(pPage);
        if( rc!=SQLITE_OK ){
          return rc;
        }
      }else{
        /* The table being dropped does not have the largest root-page
        ** number in the database. So move the page that does into the 
        ** gap left by the deleted root-page.
		** �����ݿ��б�ɾ���ı�û�����ĸ�ҳ�룬����ƶ�ҳ����ɾ����ҳ�������ķ�϶��.
        */
        MemPage *pMove;
        releasePage(pPage);
        rc = btreeGetPage(pBt, maxRootPgno, &pMove, 0);
        if( rc!=SQLITE_OK ){
          return rc;
        }
        rc = relocatePage(pBt, pMove, PTRMAP_ROOTPAGE, 0, iTable, 0);/*�ƶ�ҳȥ�ɾ���ĸ�ҳ��*/
        releasePage(pMove);
        if( rc!=SQLITE_OK ){
          return rc;
        }
        pMove = 0;
        rc = btreeGetPage(pBt, maxRootPgno, &pMove, 0);
        freePage(pMove, &rc);
        releasePage(pMove);
        if( rc!=SQLITE_OK ){
          return rc;
        }
        *piMoved = maxRootPgno;
      }

      /* Set the new 'max-root-page' value in the database header. This
      ** is the old value less one, less one more if that happens to
      ** be a root-page number, less one again if that is the
      ** PENDING_BYTE_PAGE.
	  ** �����ݿ�ͷ�������µ�max-root-page��ֵ.����ԭ����ֵ��һ��������ڸ�ҳ����Ҫ�ٲ�ֹ1.
	  ** �����PENDING_BYTE_PAGE�ٴμ�1.
      */
      maxRootPgno--;
      while( maxRootPgno==PENDING_BYTE_PAGE(pBt)
             || PTRMAP_ISPAGE(pBt, maxRootPgno) ){
        maxRootPgno--;
      }
      assert( maxRootPgno!=PENDING_BYTE_PAGE(pBt) );

      rc = sqlite3BtreeUpdateMeta(p, 4, maxRootPgno);/*�������ĸ�ҳ*/
    }else{
      freePage(pPage, &rc);
      releasePage(pPage);
    }
#endif
  }else{
    /* If sqlite3BtreeDropTable was called on page 1.
    ** This really never should happen except in a corrupt
    ** database. 
	** ���sqlite3BtreeDropTable��page1�ϱ�����.�����ڱ��������ݿ�������������ᷢ����
    */
    zeroPage(pPage, PTF_INTKEY|PTF_LEAF );
    releasePage(pPage);
  }
  return rc;  
}

int sqlite3BtreeDropTable(Btree *p, int iTable, int *piMoved){  //ɾ�����ݿ��е�һ��B��
  int rc;
  sqlite3BtreeEnter(p);
  rc = btreeDropTable(p, iTable, piMoved);
  sqlite3BtreeLeave(p);
  return rc;
}

/*
** This function may only be called if the b-tree connection already
** has a read or write transaction open on the database.
** ��������ݿ���B���Ѿ���һ������д���񿪷ţ���ô���������Ψһ������.
** Read the meta-information out of a database file.  Meta[0]
** is the number of free pages currently in the database.  Meta[1]
** through meta[15] are available for use by higher layers.  Meta[0]
** is read-only, the others are read/write.
** �����ݿ��ļ���Ԫ������Ϣ.Meta[0]�ǵ�ǰ���ݿ��пհ�ҳ�������.Meta[1]��Meta[15]���ڸ��߲�ʱ���õġ�
** Meta[0]��ֻ���ģ������Ķ��Ƕ�д�ġ�
** The schema layer numbers meta values differently.  At the schema
** layer (and the SetCookie and ReadCookie opcodes) the number of
** free pages is not visible.  So Cookie[0] is the same as Meta[1].
** ģʽ���¼Ԫ���ݵĲ�ֵͬ.��ģʽ��(SetCookie��ReadCookie������)�հ�ҳ�������ǲ��ɼ��ġ�
** ���Cookie[0]�� Meta[1]����ͬ��.
*/ 
/*���b-tree����һ������д��������������ܱ����á������ݿ��ļ��ж���meta-information��
Meta[0]�����ݿ��е�����ҳ��Meta[1]���Ա��û�ͨ��meta[15]���ʡ�Meta[0]Ϊֻ��������Ϊ��д��
*/
void sqlite3BtreeGetMeta(Btree *p, int idx, u32 *pMeta){   //�����ݿ��ļ���Ԫ������Ϣ
  BtShared *pBt = p->pBt;

  sqlite3BtreeEnter(p);
  assert( p->inTrans>TRANS_NONE );
  assert( SQLITE_OK==querySharedCacheTableLock(p, MASTER_ROOT, READ_LOCK) );
  assert( pBt->pPage1 );
  assert( idx>=0 && idx<=15 );

  *pMeta = get4byte(&pBt->pPage1->aData[36 + idx*4]);

  /* If auto-vacuum is disabled in this build and this is an auto-vacuum
  ** database, mark the database as read-only.  */
  /*
  ** ����ڹ�����auto-vacuumΪ������״̬����auto-vacuum���ݿ⣬������ݿ�Ϊֻ����
  */
#ifdef SQLITE_OMIT_AUTOVACUUM
  if( idx==BTREE_LARGEST_ROOT_PAGE && *pMeta>0 ){
    pBt->btsFlags |= BTS_READ_ONLY;
  }
#endif

  sqlite3BtreeLeave(p);  //��B�����˳�������
}

/*
** Write meta-information back into the database.  Meta[0] is
** read-only and may not be written.
*/
/*
** ��meta-informationд�����ݿ⡣Meta[0]Ϊֻ���ҿ��ܲ��ᱻд��
*/

int sqlite3BtreeUpdateMeta(Btree *p, int idx, u32 iMeta){  //��meta-informationд�����ݿ⣬����Ԫ����
  BtShared *pBt = p->pBt;
  unsigned char *pP1;
  int rc;
  assert( idx>=1 && idx<=15 );
  sqlite3BtreeEnter(p);
  assert( p->inTrans==TRANS_WRITE );
  assert( pBt->pPage1!=0 );
  pP1 = pBt->pPage1->aData;
  rc = sqlite3PagerWrite(pBt->pPage1->pDbPage); /*���pBt->pPage1->pDbPage��д*/
  if( rc==SQLITE_OK ){
    put4byte(&pP1[36 + idx*4], iMeta);/*��iMeta�ֳ�4���ֽڷ���pP1*/
#ifndef SQLITE_OMIT_AUTOVACUUM
    if( idx==BTREE_INCR_VACUUM ){
      assert( pBt->autoVacuum || iMeta==0 );
      assert( iMeta==0 || iMeta==1 );
      pBt->incrVacuum = (u8)iMeta;
    }
#endif
  }
  sqlite3BtreeLeave(p);  //��B�����˳�������
  return rc;
}

#ifndef SQLITE_OMIT_BTREECOUNT
/*
** The first argument, pCur, is a cursor opened on some b-tree. Count the
** number of entries in the b-tree and write the result to *pnEntry.
**
** SQLITE_OK is returned if the operation is successfully executed. 
** Otherwise, if an error is encountered (i.e. an IO error or database
** corruption) an SQLite error code is returned.
*/
/*
��һ������pCur����B����һ���򿪵��αꡣ��B���ϵ���Ŀ�������ѽ��д��*pnEntry��
��������ɹ�ִ�У��򷵻� SQLITE_OK����֮����SQLite�������(����I/O��������ݿ����)��
*/
int sqlite3BtreeCount(BtCursor *pCur, i64 *pnEntry){    //��B���ϵ���Ŀ����
  i64 nEntry = 0;                      /* Value to return in *pnEntry */   //Ҫд�뵽*pnEntry��ֵ
  int rc;                              /* Return code */                   //���ش���

  if( pCur->pgnoRoot==0 ){
    *pnEntry = 0;
    return SQLITE_OK;
  }
  rc = moveToRoot(pCur);

  /* Unless an error occurs, the following loop runs one iteration for each
  ** page in the B-Tree structure (not including overflow pages). 
  */
  /*���Ǵ�����������ѭ����ÿһ��B-Tree�ṹ��ִ��һ�ε����������������ҳ*/
  while( rc==SQLITE_OK ){
    int iIdx;              /* Index of child node in parent */  //���ڵ�ĺ��ӽڵ������
    MemPage *pPage;        /* Current page of the b-tree */     //B���ĵ�ǰҳ

    /* If this is a leaf page or the tree is not an int-key tree, then 
    ** this page contains countable entries. Increment the entry counter
    ** accordingly.
    */
    /*
     �������һ��Ҷ��ҳ������B���Ϲؼ��ֲ������͵ģ���ô���ҳ������������Ŀ����Ӧ������
     ��Ŀ��������
	*/
    pPage = pCur->apPage[pCur->iPage];
    if( pPage->leaf || !pPage->intKey ){
      nEntry += pPage->nCell;
    }

    /* pPage is a leaf node. This loop navigates the cursor so that it 
    ** points to the first interior cell that it points to the parent of
    ** the next page in the tree that has not yet been visited. The
    ** pCur->aiIdx[pCur->iPage] value is set to the index of the parent cell
    ** of the page, or to the number of cells in the page if the next page
    ** to visit is the right-child of its parent.
    **
    ** If all pages in the tree have been visited, return SQLITE_OK to the
    ** caller.
    */
    /*pPage��һ��Ҷ��ҳ�����ѭ��ʹ����α�ָ���һ���ڲ��ĵ�Ԫ�������Ԫ��ָ������û�б����ʵ���һҳ��
	���ڵ㡣pCur->aiIdx[pCur->iPage]��ֵ����Ϊҳ�и���Ԫ����������������һҳ��Ҫ����
	���ڵ���Һ��ӣ�pCur->aiIdx[pCur->iPage]��ֵ����Ϊҳ�е�Ԫ���������
	** �����е�����ҳ�������ʣ�����SQLITE_OK.
	*/
    if( pPage->leaf ){
      do {
        if( pCur->iPage==0 ){
          /* All pages of the b-tree have been visited. Return successfully. */  //B��ҳ�������ʳɹ�����
          *pnEntry = nEntry;
          return SQLITE_OK;
        }
        moveToParent(pCur);
      }while ( pCur->aiIdx[pCur->iPage]>=pCur->apPage[pCur->iPage]->nCell );

      pCur->aiIdx[pCur->iPage]++;
      pPage = pCur->apPage[pCur->iPage];
    }

    /* Descend to the child node of the cell that the cursor currently 
    ** points at. This is the right-child if (iIdx==pPage->nCell).
    */
    /*�½�����ǰ�α�ָ����ĺ��ӽ�㡣���iIdx==pPage->nCell��ȡ�Һ��ӽ�㡣*/
    iIdx = pCur->aiIdx[pCur->iPage];
    if( iIdx==pPage->nCell ){
      rc = moveToChild(pCur, get4byte(&pPage->aData[pPage->hdrOffset+8]));
    }else{
      rc = moveToChild(pCur, get4byte(findCell(pPage, iIdx)));
    }
  }

  /* An error has occurred. Return an error code. */
  return rc;
}
#endif

/*
** Return the pager associated with a BTree.  This routine is used for
** testing and debugging only.
** ������B����ص�ҳ���ú������������Ժ͵���.
*/
Pager *sqlite3BtreePager(Btree *p){
  return p->pBt->pPager;
}

#ifndef SQLITE_OMIT_INTEGRITY_CHECK
/*
** Append a message to the error message string.
*/
/*����Ϣ���ӵ�������Ϣ�ַ����ĺ��档*/
static void checkAppendMsg(      //����Ϣ���ӵ�������Ϣ�ַ����ĺ���
  IntegrityCk *pCheck,
  char *zMsg1,
  const char *zFormat,
  ...
){
  va_list ap;
  if( !pCheck->mxErr ) return;
  pCheck->mxErr--;
  pCheck->nErr++;
  va_start(ap, zFormat);
  if( pCheck->errMsg.nChar ){
    sqlite3StrAccumAppend(&pCheck->errMsg, "\n", 1);
  }
  if( zMsg1 ){
    sqlite3StrAccumAppend(&pCheck->errMsg, zMsg1, -1);
  }
  sqlite3VXPrintf(&pCheck->errMsg, 1, zFormat, ap);
  va_end(ap);
  if( pCheck->errMsg.mallocFailed ){
    pCheck->mallocFailed = 1;
  }
}
#endif /* SQLITE_OMIT_INTEGRITY_CHECK */

#ifndef SQLITE_OMIT_INTEGRITY_CHECK

/*
** Return non-zero if the bit in the IntegrityCk.aPgRef[] array that
** corresponds to page iPg is already set.
*/
/*
�����IntegrityCk.aPgRef[]�����У���Ӧ��ҳiPg�Ѿ����ã����ط�0��
*/
static int getPageReferenced(IntegrityCk *pCheck, Pgno iPg){
  assert( iPg<=pCheck->nPage && sizeof(pCheck->aPgRef[0])==1 );
  return (pCheck->aPgRef[iPg/8] & (1 << (iPg & 0x07)));
}

/*
** Set the bit in the IntegrityCk.aPgRef[] array that corresponds to page iPg.
** �趨����ҳiPg��Ӧ��IntegrityCk.aPgRef[]�����е�λ.
*/
static void setPageReferenced(IntegrityCk *pCheck, Pgno iPg){  //�趨����ҳiPg��Ӧ��IntegrityCk.aPgRef[]�����е�λ
  assert( iPg<=pCheck->nPage && sizeof(pCheck->aPgRef[0])==1 );
  pCheck->aPgRef[iPg/8] |= (1 << (iPg & 0x07));
}


/*
** Add 1 to the reference count for page iPage.  If this is the second
** reference to the page, add an error message to pCheck->zErrMsg.
** Return 1 if there are 2 ore more references to the page and 0 if
** if this is the first reference to the page.
**
** Also check that the page number is in bounds.
*/
/*��ҳiPage������������1������Ƕ�ҳ�ĵڶ������ã��Ӵ�����Ϣ��pCheck->zErrMsg��
�����ҳ�����λ��߸���Σ�����1��������ҳ����һ�����ã�����0��
*/
static int checkRef(IntegrityCk *pCheck, Pgno iPage, char *zContext){  //��ҳiPage������������1
  if( iPage==0 ) return 1;
  if( iPage>pCheck->nPage ){
    checkAppendMsg(pCheck, zContext, "invalid page number %d", iPage);
    return 1;
  }
  if( getPageReferenced(pCheck, iPage) ){
    checkAppendMsg(pCheck, zContext, "2nd reference to page %d", iPage);
    return 1;
  }
  setPageReferenced(pCheck, iPage);
  return 0;
}

#ifndef SQLITE_OMIT_AUTOVACUUM
/*
** Check that the entry in the pointer-map for page iChild maps to 
** page iParent, pointer type ptrType. If not, append an error message
** to pCheck.
** �˶Դ�ҳiChildӳ�䵽ҳiParent��ָ��λͼ�е���Ŀ��ָ������ΪptrType.���û�У����Ӵ�����Ϣ��pCheck.
*/

static void checkPtrmap(           //�˶Դ�ҳiChildӳ�䵽ҳiParent��ָ��λͼ�е���Ŀ
  IntegrityCk *pCheck,   /* Integrity check context */                    //�����Լ��������
  Pgno iChild,           /* Child page number */                          //����ҳ��ҳ��
  u8 eType,              /* Expected pointer map type */                  //ָ��ӳ�������
  Pgno iParent,          /* Expected pointer map parent page number */    //ָ��ӳ��ĸ�ҳ��
  char *zContext         /* Context description (used for error msg) */   //����������(������������)
){
  int rc;
  u8 ePtrmapType;
  Pgno iPtrmapParent;

  rc = ptrmapGet(pCheck->pBt, iChild, &ePtrmapType, &iPtrmapParent);      //��ָ��λͼ��ȡ��Ŀ
  if( rc!=SQLITE_OK ){
    if( rc==SQLITE_NOMEM || rc==SQLITE_IOERR_NOMEM ) pCheck->mallocFailed = 1;
    checkAppendMsg(pCheck, zContext, "Failed to read ptrmap key=%d", iChild);
    return;
  }

  if( ePtrmapType!=eType || iPtrmapParent!=iParent ){
    checkAppendMsg(pCheck, zContext, 
      "Bad ptr map entry key=%d expected=(%d,%d) got=(%d,%d)", 
      iChild, eType, iParent, ePtrmapType, iPtrmapParent);
  }
}
#endif

/*
** Check the integrity of the freelist or of an overflow page list.
** Verify that the number of pages on the list is N.
** �������б�����ҳ�б�������ԡ���֤�б��ϵ�ҳ����N.
*/

static void checkList(        //�������б�����ҳ�б��������
  IntegrityCk *pCheck,  /* Integrity checking context */                         //�����������Լ��
  int isFreeList,       /* True for a freelist.  False for overflow page list */ //�����б�Ϊtrue,���ҳ�б�Ϊfalse
  int iPage,            /* Page number for first page in the list */             //�б��е�һ��ҳ��ҳ��
  int N,                /* Expected number of pages in the list */               //���б���Ԥ�ڵ�ҳ������
  char *zContext        /* Context for error messages */                         //�����ĵĴ�����Ϣ
){
  int i;
  int expected = N;
  int iFirst = iPage;
  while( N-- > 0 && pCheck->mxErr ){
    DbPage *pOvflPage;
    unsigned char *pOvflData;
    if( iPage<1 ){
      checkAppendMsg(pCheck, zContext,
         "%d of %d pages missing from overflow list starting at %d",
          N+1, expected, iFirst);
      break;
    }
    if( checkRef(pCheck, iPage, zContext) ) break;
    if( sqlite3PagerGet(pCheck->pPager, (Pgno)iPage, &pOvflPage) ){
      checkAppendMsg(pCheck, zContext, "failed to get page %d", iPage);
      break;
    }
    pOvflData = (unsigned char *)sqlite3PagerGetData(pOvflPage);
    if( isFreeList ){
      int n = get4byte(&pOvflData[4]);
#ifndef SQLITE_OMIT_AUTOVACUUM
      if( pCheck->pBt->autoVacuum ){
        checkPtrmap(pCheck, iPage, PTRMAP_FREEPAGE, 0, zContext);
      }
#endif
      if( n>(int)pCheck->pBt->usableSize/4-2 ){
        checkAppendMsg(pCheck, zContext,
           "freelist leaf count too big on page %d", iPage);
        N--;
      }else{
        for(i=0; i<n; i++){
          Pgno iFreePage = get4byte(&pOvflData[8+i*4]);
#ifndef SQLITE_OMIT_AUTOVACUUM
          if( pCheck->pBt->autoVacuum ){
            checkPtrmap(pCheck, iFreePage, PTRMAP_FREEPAGE, 0, zContext);
          }
#endif
          checkRef(pCheck, iFreePage, zContext);
        }
        N -= n;
      }
    }
#ifndef SQLITE_OMIT_AUTOVACUUM
    else{
      /* If this database supports auto-vacuum and iPage is not the last
      ** page in this overflow list, check that the pointer-map entry for
      ** the following page matches iPage.
      */
      /*
	  ** ������ݿ�֧��auto-vacuum����iPage������������е����һҳ�����ƥ����һҳ��
	  ** iPageƥ���ָ��λͼ��Ŀ��
	  */
      if( pCheck->pBt->autoVacuum && N>0 ){
        i = get4byte(pOvflData);
        checkPtrmap(pCheck, i, PTRMAP_OVERFLOW2, iPage, zContext);
      }
    }
#endif
    iPage = get4byte(pOvflData);
    sqlite3PagerUnref(pOvflPage);
  }
}
#endif /* SQLITE_OMIT_INTEGRITY_CHECK */

#ifndef SQLITE_OMIT_INTEGRITY_CHECK
/*
** Do various sanity checks on a single page of a tree.  Return
** the tree depth.  Root pages return 0.  Parents of root pages
** return 1, and so forth.
** 
** These checks are done:
**
**      1.  Make sure that cells and freeblocks do not overlap
**          but combine to completely cover the page.
**  NO  2.  Make sure cell keys are in order.
**  NO  3.  Make sure no key is less than or equal to zLowerBound.
**  NO  4.  Make sure no key is greater than or equal to zUpperBound.
**      5.  Check the integrity of overflow pages.
**      6.  Recursively call checkTreePage on all children.
**      7.  Verify that the depth of all children is the same.
**      8.  Make sure this page is at least 33% full or else it is
**          the root of the tree.
*/
/*
** ������һ��������ҳ�Ͻ��м�顣����������ȡ���ҳ����0����ҳ�ϵĸ���㷵��1��
** �������ơ����½������:
** 1��ȷ����Ԫ������ɿ鲻���ǣ�������ȫ����ҳ��
** 2��ȷ����Ԫ���ϵĹؼ�������
** 3��ȷ���ؼ��ִ���zLowerBound��
** 4��ȷ���ؼ���С��zUpperBound��
** 5��������ҳ�������ԡ�
** 6�������к��ӽ���ϵݹ����checkTreePage��
** 7��ȷ�����к��ӽ�������ͬ��
** 8�����˸�ҳ��ҳ������33%�Ŀռ䱻ʹ�á�
*/
static int checkTreePage(    //������һ��������ҳ�Ͻ��м��
  IntegrityCk *pCheck,  /* Context for the sanity check */       //�˶�������
  int iPage,            /* Page number of the page to check */   //Ҫ�˶�ҳ��ҳ��
  char *zParentContext, /* Parent context */                     //���ڵ�������
  i64 *pnParentMinKey, 
  i64 *pnParentMaxKey
){
  MemPage *pPage;
  int i, rc, depth, d2, pgno, cnt;
  int hdr, cellStart;
  int nCell;
  u8 *data;
  BtShared *pBt;
  int usableSize;
  char zContext[100];
  char *hit = 0;
  i64 nMinKey = 0;
  i64 nMaxKey = 0;

  sqlite3_snprintf(sizeof(zContext), zContext, "Page %d: ", iPage);

  /* Check that the page exists */
  pBt = pCheck->pBt;
  usableSize = pBt->usableSize;
  if( iPage==0 ) return 0;
  if( checkRef(pCheck, iPage, zParentContext) ) return 0;
  if( (rc = btreeGetPage(pBt, (Pgno)iPage, &pPage, 0))!=0 ){
    checkAppendMsg(pCheck, zContext,
       "unable to get the page. error code=%d", rc);
    return 0;
  }

  /* Clear MemPage.isInit to make sure the corruption detection code in
  ** btreeInitPage() is executed.  
  ** ����MemPage.isInitȷ��btreeInitPage()�еı���������ִ�� */
  pPage->isInit = 0;
  if( (rc = btreeInitPage(pPage))!=0 ){
    assert( rc==SQLITE_CORRUPT );  /* The only possible error from InitPage */
    checkAppendMsg(pCheck, zContext, 
                   "btreeInitPage() returns error code %d", rc);
    releasePage(pPage);
    return 0;
  }

  /* Check out all the cells.*/
  depth = 0;
  for(i=0; i<pPage->nCell && pCheck->mxErr; i++){
    u8 *pCell;
    u32 sz;
    CellInfo info;

    /* Check payload overflow pages */
    sqlite3_snprintf(sizeof(zContext), zContext,
             "On tree page %d cell %d: ", iPage, i);
    pCell = findCell(pPage,i);
    btreeParseCellPtr(pPage, pCell, &info);   //������Ԫ���ݿ飬����CellInfo�ṹ��
    sz = info.nData;
    if( !pPage->intKey ) sz += (int)info.nKey;
    /* For intKey pages, check that the keys are in order. */  //����intKey������˶Թؼ���
    else if( i==0 ) nMinKey = nMaxKey = info.nKey;
    else{
      if( info.nKey <= nMaxKey ){
        checkAppendMsg(pCheck, zContext, 
            "Rowid %lld out of order (previous was %lld)", info.nKey, nMaxKey);
      }
      nMaxKey = info.nKey;
    }
    assert( sz==info.nPayload );
    if( (sz>info.nLocal) 
     && (&pCell[info.iOverflow]<=&pPage->aData[pBt->usableSize])
    ){
      int nPage = (sz - info.nLocal + usableSize - 5)/(usableSize - 4);
      Pgno pgnoOvfl = get4byte(&pCell[info.iOverflow]);
#ifndef SQLITE_OMIT_AUTOVACUUM
      if( pBt->autoVacuum ){
        checkPtrmap(pCheck, pgnoOvfl, PTRMAP_OVERFLOW1, iPage, zContext); //�˶Դ�ҳpgnoOvflӳ�䵽ҳiPage��ָ��λͼ�е���Ŀ
      }
#endif
      checkList(pCheck, 0, pgnoOvfl, nPage, zContext);  //�������б�����ҳ�б��������
    }

    /* Check sanity of left child page. */    //�˶�����
    if( !pPage->leaf ){
      pgno = get4byte(pCell);
#ifndef SQLITE_OMIT_AUTOVACUUM
      if( pBt->autoVacuum ){
        checkPtrmap(pCheck, pgno, PTRMAP_BTREE, iPage, zContext);
      }
#endif
      d2 = checkTreePage(pCheck, pgno, zContext, &nMinKey, i==0 ? NULL : &nMaxKey);
      if( i>0 && d2!=depth ){
        checkAppendMsg(pCheck, zContext, "Child page depth differs");
      }
      depth = d2;
    }
  }

  if( !pPage->leaf ){
    pgno = get4byte(&pPage->aData[pPage->hdrOffset+8]);
    sqlite3_snprintf(sizeof(zContext), zContext, 
                     "On page %d at right child: ", iPage);
#ifndef SQLITE_OMIT_AUTOVACUUM
    if( pBt->autoVacuum ){
      checkPtrmap(pCheck, pgno, PTRMAP_BTREE, iPage, zContext);
    }
#endif
    checkTreePage(pCheck, pgno, zContext, NULL, !pPage->nCell ? NULL : &nMaxKey);
  }
 
  /* For intKey leaf pages, check that the min/max keys are in order
  ** with any left/parent/right pages. 
  ** ����intKeyҶ��ҳ����left/parent/rightҳ����˶�min/max�ؼ���.
  */
  if( pPage->leaf && pPage->intKey ){
    /* if we are a left child page */  //����������ҳ��
    if( pnParentMinKey ){
      /* if we are the left most child page */  //�����������ҳ��
      if( !pnParentMaxKey ){
        if( nMaxKey > *pnParentMinKey ){
          checkAppendMsg(pCheck, zContext, 
              "Rowid %lld out of order (max larger than parent min of %lld)",
              nMaxKey, *pnParentMinKey);
        }
      }else{
        if( nMinKey <= *pnParentMinKey ){
          checkAppendMsg(pCheck, zContext, 
              "Rowid %lld out of order (min less than parent min of %lld)",
              nMinKey, *pnParentMinKey);
        }
        if( nMaxKey > *pnParentMaxKey ){
          checkAppendMsg(pCheck, zContext, 
              "Rowid %lld out of order (max larger than parent max of %lld)",
              nMaxKey, *pnParentMaxKey);
        }
        *pnParentMinKey = nMaxKey;
      }
    /* else if we're a right child page */  //���Һ���ҳ��
    } else if( pnParentMaxKey ){
      if( nMinKey <= *pnParentMaxKey ){
        checkAppendMsg(pCheck, zContext, 
            "Rowid %lld out of order (min less than parent max of %lld)",
            nMinKey, *pnParentMaxKey);
      }
    }
  }

  /* Check for complete coverage of the page .*/ //�˶�ҳ����ȫ����
  data = pPage->aData;
  hdr = pPage->hdrOffset;
  hit = sqlite3PageMalloc( pBt->pageSize );  //�ӻ�������ÿռ�����ҳ
  if( hit==0 ){
    pCheck->mallocFailed = 1;
  }else{
    int contentOffset = get2byteNotZero(&data[hdr+5]);
    assert( contentOffset<=usableSize );  /* Enforced by btreeInitPage() */ //��btreeInitPage()ǿ��ִ��
    memset(hit+contentOffset, 0, usableSize-contentOffset);
    memset(hit, 1, contentOffset);
    nCell = get2byte(&data[hdr+3]);
    cellStart = hdr + 12 - 4*pPage->leaf;
    for(i=0; i<nCell; i++){
      int pc = get2byte(&data[cellStart+i*2]);
      u32 size = 65536;
      int j;
      if( pc<=usableSize-4 ){
        size = cellSizePtr(pPage, &data[pc]); //����һ��Cell��Ҫ���ܵ��ֽ���
      }
      if( (int)(pc+size-1)>=usableSize ){
        checkAppendMsg(pCheck, 0, 
            "Corruption detected in cell %d on page %d",i,iPage);
      }else{
        for(j=pc+size-1; j>=pc; j--) hit[j]++;
      }
    }
    i = get2byte(&data[hdr+1]);
    while( i>0 ){
      int size, j;
      assert( i<=usableSize-4 );     /* Enforced by btreeInitPage() */  //��btreeInitPage()ǿ��ִ��
      size = get2byte(&data[i+2]);
      assert( i+size<=usableSize );  /* Enforced by btreeInitPage() */  //��btreeInitPage()ǿ��ִ��
      for(j=i+size-1; j>=i; j--) hit[j]++;
      j = get2byte(&data[i]);
      assert( j==0 || j>i+size );  /* Enforced by btreeInitPage() */  //��btreeInitPage()ǿ��ִ��
      assert( j<=usableSize-4 );   /* Enforced by btreeInitPage() */  //��btreeInitPage()ǿ��ִ��
      i = j;
    }
    for(i=cnt=0; i<usableSize; i++){
      if( hit[i]==0 ){
        cnt++;
      }else if( hit[i]>1 ){
        checkAppendMsg(pCheck, 0,
          "Multiple uses for byte %d of page %d", i, iPage);
        break;
      }
    }
    if( cnt!=data[hdr+7] ){
      checkAppendMsg(pCheck, 0, 
          "Fragmentation of %d bytes reported as %d on page %d",
          cnt, data[hdr+7], iPage);
    }
  }
  sqlite3PageFree(hit);   //�ͷŴ�sqlite3PageMalloc()��õĻ�����
  releasePage(pPage);     //�ͷ��ڴ�ҳ
  return depth+1;
}
#endif /* SQLITE_OMIT_INTEGRITY_CHECK */

#ifndef SQLITE_OMIT_INTEGRITY_CHECK
/*
** This routine does a complete check of the given BTree file.  aRoot[] is
** an array of pages numbers were each page number is the root page of
** a table.  nRoot is the number of entries in aRoot.
**
** A read-only or read-write transaction must be opened before calling
** this function.
**
** Write the number of error seen in *pnErr.  Except for some memory
** allocation errors,  an error message held in memory obtained from
** malloc is returned if *pnErr is non-zero.  If *pnErr==0 then NULL is
** returned.  If a memory allocation error occurs, NULL is returned.
*/
/* �ú�����BTree�ļ���һ����ȫ�ļ�顣aRoot[]��һ��ҳ�����飬������ÿһ��ҳ�붼�Ǳ�ĸ�ҳ��
** aRoot�е���Ŀ����ΪnRoot���ڵ����������֮ǰ��һ��ֻ�����߶�д�������Ϊ��״̬��
** д����������*pnErr�пɼ�������һЩ�ڴ�������*pnErr���㣬һ����malloc�л�õĴ�����Ϣ�������ڴ档*pnErr==0��
** ����NULL.�ڴ������󷵻�NULL.
*/
char *sqlite3BtreeIntegrityCheck(    //��BTree�ļ���һ�������ļ��
  Btree *p,     /* The btree to be checked */                             //Ҫ������B��
  int *aRoot,   /* An array of root pages numbers for individual trees */ //һ�����ĸ�ҳ������
  int nRoot,    /* Number of entries in aRoot[] */                        //aRoot[]�е���Ŀ��
  int mxErr,    /* Stop reporting errors after this many */               //�ﵽ�����֮��ֹͣ����
  int *pnErr    /* Write number of errors seen to this variable */        //�����������ñ���
){
  Pgno i;
  int nRef;
  IntegrityCk sCheck;
  BtShared *pBt = p->pBt;
  char zErr[100];

  sqlite3BtreeEnter(p);
  assert( p->inTrans>TRANS_NONE && pBt->inTransaction>TRANS_NONE );
  nRef = sqlite3PagerRefcount(pBt->pPager);
  sCheck.pBt = pBt;
  sCheck.pPager = pBt->pPager;
  sCheck.nPage = btreePagecount(sCheck.pBt);  //����ҳ�����ݿ��ļ�(ҳ)�Ĵ�С
  sCheck.mxErr = mxErr;
  sCheck.nErr = 0;
  sCheck.mallocFailed = 0;
  *pnErr = 0;
  if( sCheck.nPage==0 ){
    sqlite3BtreeLeave(p);
    return 0;
  }

  sCheck.aPgRef = sqlite3MallocZero((sCheck.nPage / 8)+ 1);  //���䲢����
  if( !sCheck.aPgRef ){
    *pnErr = 1;
    sqlite3BtreeLeave(p);
    return 0;
  }
  i = PENDING_BYTE_PAGE(pBt);
  if( i<=sCheck.nPage ) setPageReferenced(&sCheck, i); 
  sqlite3StrAccumInit(&sCheck.errMsg, zErr, sizeof(zErr), 20000);  //��ʼ��һ���ַ����ۼ���
  sCheck.errMsg.useMalloc = 2;

  /* Check the integrity of the freelist */   //�˶Կ����б��������
  checkList(&sCheck, 1, get4byte(&pBt->pPage1->aData[32]),
            get4byte(&pBt->pPage1->aData[36]), "Main freelist: ");

  /* Check all the tables.*/                 //�˶����б�
  for(i=0; (int)i<nRoot && sCheck.mxErr; i++){
    if( aRoot[i]==0 ) continue;
#ifndef SQLITE_OMIT_AUTOVACUUM
    if( pBt->autoVacuum && aRoot[i]>1 ){
      checkPtrmap(&sCheck, aRoot[i], PTRMAP_ROOTPAGE, 0, 0);
    }
#endif
    checkTreePage(&sCheck, aRoot[i], "List of tree roots: ", NULL, NULL);  //������һ��������ҳ�Ͻ��м��
  }

  /* Make sure every page in the file is referenced .*/  //ȷ�����ļ��е�ÿ��ҳ�������� 
  for(i=1; i<=sCheck.nPage && sCheck.mxErr; i++){
#ifdef SQLITE_OMIT_AUTOVACUUM
    if( getPageReferenced(&sCheck, i)==0 ){
      checkAppendMsg(&sCheck, 0, "Page %d is never used", i);
    }
#else
    /* If the database supports auto-vacuum, make sure no tables contain
    ** references to pointer-map pages.
	** ������ݿ�֧���Զ�����ȷ��û�б������ָ��λͼҳ�����á�
    */
    if( getPageReferenced(&sCheck, i)==0 && 
       (PTRMAP_PAGENO(pBt, i)!=i || !pBt->autoVacuum) ){
      checkAppendMsg(&sCheck, 0, "Page %d is never used", i);
    }
    if( getPageReferenced(&sCheck, i)!=0 && 
       (PTRMAP_PAGENO(pBt, i)==i && pBt->autoVacuum) ){
      checkAppendMsg(&sCheck, 0, "Pointer map page %d is referenced", i);
    }
#endif
  }

  /* Make sure this analysis did not leave any unref() pages.
  ** This is an internal consistency check; an integrity check
  ** of the integrity check.
  */
  /*ȷ����������©unref()��ҳ�������ڲ�һ���Լ�飬�����Լ�顣*/
  if( NEVER(nRef != sqlite3PagerRefcount(pBt->pPager)) ){
    checkAppendMsg(&sCheck, 0, 
      "Outstanding page count goes from %d to %d during this analysis",
      nRef, sqlite3PagerRefcount(pBt->pPager)
    );
  }

  /* Clean  up and report errors.*/  //��������
  sqlite3BtreeLeave(p);
  sqlite3_free(sCheck.aPgRef);
  if( sCheck.mallocFailed ){
    sqlite3StrAccumReset(&sCheck.errMsg); //����һ��StrAccum���͵��ַ������һ������з�����ڴ档
    *pnErr = sCheck.nErr+1;
    return 0;
  }
  *pnErr = sCheck.nErr;
  if( sCheck.nErr==0 ) sqlite3StrAccumReset(&sCheck.errMsg);
  return sqlite3StrAccumFinish(&sCheck.errMsg);
}
#endif /* SQLITE_OMIT_INTEGRITY_CHECK */

/*
** Return the full pathname of the underlying database file.  Return
** an empty string if the database is in-memory or a TEMP database.
**
** The pager filename is invariant as long as the pager is
** open so it is safe to access without the BtShared mutex.
*/
/* ���صײ����ݿ��ļ���������·��������������ݿ�Ϊ�ڴ����ݿ⣬����Ϊ��ʱ���ݿ⣬
** ���ؿյ��ַ�����pager���ļ����ǲ����ֻҪpager�ǿ��ŵģ����û��BtShared������Ҳ�ܰ�ȫ����.
*/
const char *sqlite3BtreeGetFilename(Btree *p){    //���صײ����ݿ��ļ���������·����
  assert( p->pBt->pPager!=0 );
  return sqlite3PagerFilename(p->pBt->pPager, 1);
}

/*
** Return the pathname of the journal file for this database. The return
** value of this routine is the same regardless of whether the journal file
** has been created or not.
**
** The pager journal filename is invariant as long as the pager is
** open so it is safe to access without the BtShared mutex.
*/
/*
** �������ݿ�����־�ļ���·������������־�ļ��Ƿ񱻴��������򷵻�ֵ��ͬ��
** pager��־�ļ����ǲ����ֻҪpager���ţ����û��BtShared������Ҳ�ܰ�ȫ����.
*/
const char *sqlite3BtreeGetJournalname(Btree *p){  //�������ݿ�����־�ļ���·����
  assert( p->pBt->pPager!=0 );
  return sqlite3PagerJournalname(p->pBt->pPager);
}

/*
** Return non-zero if a transaction is active.  
** ���������У����ط���
*/
int sqlite3BtreeIsInTrans(Btree *p){     //�Ƿ���������
  assert( p==0 || sqlite3_mutex_held(p->db->mutex) );
  return (p && (p->inTrans==TRANS_WRITE));
}

#ifndef SQLITE_OMIT_WAL
/*
** Run a checkpoint on the Btree passed as the first argument.
**
** Return SQLITE_LOCKED if this or any other connection has an open 
** transaction on the shared-cache the argument Btree is connected to.
**
** Parameter eMode is one of SQLITE_CHECKPOINT_PASSIVE, FULL or RESTART.
*/
/*
** ִ��B���ϵļ�����Ϊ��һ���������ݡ����������κ���������
** ��һ�����ŵ����񣬷���SQLITE_LOCKED�������ڱ�B�����ӵĹ����ڲ����ϡ�
** ����eModeΪSQLITE_CHECKPOINT_PASSIVE, FULL or RESTART֮һ��
*/
int sqlite3BtreeCheckpoint(Btree *p, int eMode, int *pnLog, int *pnCkpt){ //ִ��B���ϵļ�����Ϊ��һ����������
  int rc = SQLITE_OK;
  if( p ){
    BtShared *pBt = p->pBt;
    sqlite3BtreeEnter(p);
    if( pBt->inTransaction!=TRANS_NONE ){
      rc = SQLITE_LOCKED;
    }else{
      rc = sqlite3PagerCheckpoint(pBt->pPager, eMode, pnLog, pnCkpt);
    }
    sqlite3BtreeLeave(p);
  }
  return rc;
}
#endif

/*
** Return non-zero if a read (or write) transaction is active.
** �������д�����ڻ�����ط���
*/
int sqlite3BtreeIsInReadTrans(Btree *p){
  assert( p );
  assert( sqlite3_mutex_held(p->db->mutex) );
  return p->inTrans!=TRANS_NONE;
}

int sqlite3BtreeIsInBackup(Btree *p){
  assert( p );
  assert( sqlite3_mutex_held(p->db->mutex) );
  return p->nBackup!=0;
}

/*
** This function returns a pointer to a blob of memory associated with
** a single shared-btree. The memory is used by client code for its own
** purposes (for example, to store a high-level schema associated with 
** the shared-btree). The btree layer manages reference counting issues.
** �����������һ��ָ����һ�������Ĺ���B����ص��ڴ��е�blob���ڴ汻�ͻ��˴���
** ʹ��(����,�洢һ����shared-btree��صĸ߼�ģʽ)��btree��������ü������⡣
** The first time this is called on a shared-btree, nBytes bytes of memory
** are allocated, zeroed, and returned to the caller. For each subsequent 
** call the nBytes parameter is ignored and a pointer to the same blob
** of memory returned. 
** ��������ǵ�һ���ڹ���B���ϵ��á��ڴ��nBytes���ֽڱ����䣬���㣬�����ص�������.
** ����ÿ�����ĵ���nBytes�ֽڶ������Բ���ָ��ָ���ڴ淵�ص���ͬblob����.
** If the nBytes parameter is 0 and the blob of memory has not yet been
** allocated, a null pointer is returned. If the blob has already been
** allocated, it is returned as normal.
** ���nBytes������0�����ڴ�blob��δ����,������һ����ָ�롣�����blob�Ѿ�����,�����������ء�
** Just before the shared-btree is closed, the function passed as the 
** xFree argument when the memory allocation was made is invoked on the 
** blob of allocated memory. The xFree function should not call sqlite3_free()
** on the memory, the btree layer does that.
** ��shared-btree�ر�֮ǰ,�ڴ����ʱ����������ΪxFree�������ڴ�����blob�ϱ����á����ڴ��ϣ�xFree����
** ���ܵ���sqlite3_free()����btree����Ե��á�
*/
/*
��������һ��ָ��blob���ڴ档�ڴ������ͻ��˴��롣��B����������ü��������⡣
��һ������ν�Ĺ���B���ϱ����ã�Ϊnbytes�ֽڵ��ڴ汻���䡣
*/
void *sqlite3BtreeSchema(Btree *p, int nBytes, void(*xFree)(void *)){
  BtShared *pBt = p->pBt;
  sqlite3BtreeEnter(p);
  if( !pBt->pSchema && nBytes ){
    pBt->pSchema = sqlite3DbMallocZero(0, nBytes);
    pBt->xFreeSchema = xFree;
  }
  sqlite3BtreeLeave(p);
  return pBt->pSchema;
}

/*
** Return SQLITE_LOCKED_SHAREDCACHE if another user of the same shared 
** btree as the argument handle holds an exclusive lock on the 
** sqlite_master table. Otherwise SQLITE_OK.
*/
/*�����һ���û��ڹ���B������һ��������������SQLITE_LOCKED_SHAREDCACHE��*/
int sqlite3BtreeSchemaLocked(Btree *p){    //B��ģʽ��
  int rc;
  assert( sqlite3_mutex_held(p->db->mutex) );
  sqlite3BtreeEnter(p);
  rc = querySharedCacheTableLock(p, MASTER_ROOT, READ_LOCK);
  assert( rc==SQLITE_OK || rc==SQLITE_LOCKED_SHAREDCACHE );
  sqlite3BtreeLeave(p);
  return rc;
}


#ifndef SQLITE_OMIT_SHARED_CACHE
/*
** Obtain a lock on the table whose root page is iTab.  The
** lock is a write lock if isWritelock is true or a read lock
** if it is false.
*/
/*��ñ�ĸ�ҳiTab�ϵ�����isWriteLock=1��д��������0Ϊ������*/
int sqlite3BtreeLockTable(Btree *p, int iTab, u8 isWriteLock){   //��ñ�ĸ�ҳiTab�ϵ���
  int rc = SQLITE_OK;
  assert( p->inTrans!=TRANS_NONE );
  if( p->sharable ){
    u8 lockType = READ_LOCK + isWriteLock;
    assert( READ_LOCK+1==WRITE_LOCK );
    assert( isWriteLock==0 || isWriteLock==1 );

    sqlite3BtreeEnter(p);
    rc = querySharedCacheTableLock(p, iTab, lockType);//�鿴Btree���p�Ƿ��ھ��и�ҳiTab�ı��ϻ����lockType����
    if( rc==SQLITE_OK ){
      rc = setSharedCacheTableLock(p, iTab, lockType); //ͨ��B�����p�ڸ�ҳiTable�ı��������������B��
    }
    sqlite3BtreeLeave(p);
  }
  return rc;
}
#endif

#ifndef SQLITE_OMIT_INCRBLOB
/*
** Argument pCsr must be a cursor opened for writing on an 
** INTKEY table currently pointing at a valid table entry. 
** This function modifies the data stored as part of that entry.
** ����pCsr������һ���򿪵��α꣬�ȴ���I��ǰNTKEY����ָ��һ����Ч�ı���Ŀ��
** ��������޸Ĵ洢��������Ϊ��Ŀ��һ���֡�
** Only the data content may only be modified, it is not possible to 
** change the length of the data stored. If this function is called with
** parameters that attempt to write past the end of the existing data,
** no modifications are made and SQLITE_CORRUPT is returned.
** ֻ���������ݿ����޸�,�޸Ĵ洢�����ݵĳ����ǲ����ܵġ�����������������
** ��������д���������ݵ�ĩβ,û���޸Ĳ�����SQLITE_CORRUPT��
*/
/*�������������ܹ����޸ģ������ܸı����ݴ洢�ĳ��ȡ�*/
int sqlite3BtreePutData(BtCursor *pCsr, u32 offset, u32 amt, void *z){  //�޸���������
  int rc;
  assert( cursorHoldsMutex(pCsr) );
  assert( sqlite3_mutex_held(pCsr->pBtree->db->mutex) );
  assert( pCsr->isIncrblobHandle );

  rc = restoreCursorPosition(pCsr);
  if( rc!=SQLITE_OK ){
    return rc;
  }
  assert( pCsr->eState!=CURSOR_REQUIRESEEK );
  if( pCsr->eState!=CURSOR_VALID ){
    return SQLITE_ABORT;
  }

  /* Check some assumptions: 
  **   (a) the cursor is open for writing,        //�ȴ��α꿪��
  **   (b) there is a read/write transaction open, //�ж���д���񿪷�
  **   (c) the connection holds a write-lock on the table (if required),  //�ڱ����Ͼ�����ӳ���д��
  **   (d) there are no conflicting read-locks, and                       //û�г�ͻ����
  **   (e) the cursor points at a valid row of an intKey table.           //�α�ָ��intKey�����Ч��
  */
  if( !pCsr->wrFlag ){
    return SQLITE_READONLY;
  }
  assert( (pCsr->pBt->btsFlags & BTS_READ_ONLY)==0
              && pCsr->pBt->inTransaction==TRANS_WRITE );/*�α�򿪣�д����*/
  assert( hasSharedCacheTableLock(pCsr->pBtree, pCsr->pgnoRoot, 0, 2) );
  assert( !hasReadConflicts(pCsr->pBtree, pCsr->pgnoRoot) );/*����û��ͻ*/
  assert( pCsr->apPage[pCsr->iPage]->intKey );

  return accessPayload(pCsr, offset, amt, (unsigned char *)z, 1);  //����д��Ч�غ���Ϣ
}

/* 
** Set a flag on this cursor to cache the locations of pages from the 
** overflow list for the current row. This is used by cursors opened
** for incremental blob IO only.
** ���ڵ�ǰ�У�������α껺���ҳ���λ������һ����־����־����������blob IO���ŵ��α�ʹ�á�
** This function sets a flag only. The actual page location cache
** (stored in BtCursor.aOverflow[]) is allocated and used by function
** accessPayload() (the worker function for sqlite3BtreeData() and
** sqlite3BtreePutData()).
** �������ֻ����һ����־��ʵ��ҳ��λ�û���(�洢��BtCursor.aOverflow[])����ͱ�accessPayload()ʹ��
** (�Ժ���sqlite3BtreeData()��sqlite3BtreePutData()��Ч)��
*/  /*�˺������α�������һ����־����������б��ϵ�ҳ*/
void sqlite3BtreeCacheOverflow(BtCursor *pCur){  //�˺������α�������һ����־
  assert( cursorHoldsMutex(pCur) );
  assert( sqlite3_mutex_held(pCur->pBtree->db->mutex) );
  invalidateOverflowCache(pCur);
  pCur->isIncrblobHandle = 1;
}
#endif

/*
** Set both the "read version" (single byte at byte offset 18) and 
** "write version" (single byte at byte offset 19) fields in the database
** header to iVersion.
** �����ݿ�ͷ������"read version"(��ƫ����18���ĵ��ֽڴ�)��"write version"(��ƫ����19���ĵ��ֽڴ�)��
*/
int sqlite3BtreeSetVersion(Btree *pBtree, int iVersion){  //�����ݿ�ͷ������"���汾"��"д�汾"��
  BtShared *pBt = pBtree->pBt;
  int rc;                         /* Return code */
 
  assert( iVersion==1 || iVersion==2 );

  /* If setting the version fields to 1, do not automatically open the
  ** WAL connection, even if the version fields are currently set to 2.
  */
  pBt->btsFlags &= ~BTS_NO_WAL;
  if( iVersion==1 ) pBt->btsFlags |= BTS_NO_WAL;/*û�д�Ԥд��־����*/

  rc = sqlite3BtreeBeginTrans(pBtree, 0);
  if( rc==SQLITE_OK ){
    u8 *aData = pBt->pPage1->aData;
    if( aData[18]!=(u8)iVersion || aData[19]!=(u8)iVersion ){
      rc = sqlite3BtreeBeginTrans(pBtree, 2);
      if( rc==SQLITE_OK ){
        rc = sqlite3PagerWrite(pBt->pPage1->pDbPage);
        if( rc==SQLITE_OK ){
          aData[18] = (u8)iVersion;/*18�Ƕ��汾*/
          aData[19] = (u8)iVersion;/*19��д�汾*/
        }
      }
    }
  }

  pBt->btsFlags &= ~BTS_NO_WAL;
  return rc;
}

/*
** set the mask of hint flags for cursor pCsr. Currently the only valid
** values are 0 and BTREE_BULKLOAD.
** �����α�pCsr���롣��ǰΨһ��Чֵ��0,��BTREE_BULKLOAD��
*/
void sqlite3BtreeCursorHints(BtCursor *pCsr, unsigned int mask){
  assert( mask==BTREE_BULKLOAD || mask==0 );/*��������mask=BTREE_BULKLOAD ��0*/
  pCsr->hints = mask;
}
