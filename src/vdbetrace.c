/*
** 2009 November 25
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
** This file contains code used to insert the values of host parameters 此文件包含用于插入主机参数值的代码此文件包含用于插入主机参数值的代码此文件包含用于插入主机参数值的代码.
** (aka "wildcards") into the SQL text output by sqlite3_trace().通过sqlite3_trace()把通配符进入到SQL文本输出中。
**
** The Vdbe parse-tree explainer is also found here. 在这里也发现了Vdbe解释树。
*/
#include "sqliteInt.h"
#include "vdbeInt.h"

#ifndef SQLITE_OMIT_TRACE

/*
** zSql is a zero-terminated string of UTF-8 SQL text.  Return the number of //zSql是一个以零为结束的一串UTF-8 SQL文本
** bytes in this text up to but excluding the first character in 此文本中字节最多，但是排除了在主机参数中的第一个字符。如果文本没有包含主机参数，返回文本中的总字节数。
** a host parameter.  If the text contains no host parameters, return
** the total number of bytes in the text.
这个用于统计SQL文本中的字符数，用字节表示。如果文本中含有通配符，则不统计在内; 如果不含有，则全部统计。其中参数zSql表示以0为结尾的一串uft-8 SQL text。参数pnToken是表示标记参数的长度。
*/
static int findNextHostParameter(const char *zSql, int *pnToken){
  int tokenType;
  int nTotal = 0;
  int n;

  *pnToken = 0;
  while( zSql[0] ){
    n = sqlite3GetToken((u8*)zSql, &tokenType);
    assert( n>0 && tokenType!=TK_ILLEGAL );
    if( tokenType==TK_VARIABLE ){
      *pnToken = n;
      break;
    }
    nTotal += n;
    zSql += n;
  }
  return nTotal;
}

/*
** This function returns a pointer to a nul-terminated string in memory //这个函数返回一个指向内存中从sqlite3DbMalloc()获得的空字符。
** obtained from sqlite3DbMalloc(). If sqlite3.vdbeExecCnt is 1, then the 如果sqlite3.vdbeExecCnt 是1，则该字符串包含一份zRawSql，但是随着主机参数对当前绑定的扩展。
** string contains a copy of zRawSql but with host parameters expanded to  或者，如果sqlite3.vdbeExecCnt 大于1，则返回的字符串包含一份文本每行的前缀为“--”的zRawSql。
** their current bindings. Or, if sqlite3.vdbeExecCnt is greater than 1, 
** then the returned string holds a copy of zRawSql with "-- " prepended
** to each line of text. //调用函数负责确保返回内存最后被释放。
**
** The calling function is responsible for making sure the memory returned
** is eventually freed.
**
** ALGORITHM:  Scan the input string looking for host parameters in any of 算法：扫描输入字符串，寻找用这些形式：?, ?N, $A, @A, :A的主机参数。
** these forms:  ?, ?N, $A, @A, :A.  Take care to avoid text within 注意避免字符串字面的文本，引用标志字符命名，和评论。
** string literals, quoted identifier names, and comments.  For text forms,  对于文本形式，
** the host parameter index is found by scanning the perpared 通过扫描提前准备好的用于对应 OP_Variable 操作码的声明找到主机参数索引。一旦主机参数索引已知，用p->aVar[]定位该值。
** statement for the corresponding OP_Variable opcode.  Once the host
** parameter index is known, locate the value in p->aVar[].  Then render 然后传递该值作为一个文字取代主机参数值。
** the value as a literal in place of the host parameter name.
这个函数的作用是向SQL语句中添加一些通配符。该函数最终返回一个char类型的指针，该指针在内存中指向一个“null”结尾的数组。其中，函数参数为p为一个vdbe指针, zRawSql表示SQL语句。在该部分，
若sqlite3.vdbeExecCnt 的值等于1，则它返回的字符串包含SQL语句内容，并且在语句中添加了一些通配符；若sqlite3.vdbeExecCnt的值大于1，则返回的字符串中每一行都将加上一个前缀”--“。
*/
char *sqlite3VdbeExpandSql(
  Vdbe *p,                 /* The prepared statement being evaluated */ //被估计的准备声明
  const char *zRawSql      /* Raw text of the SQL statement */ //SQL语句的原始文本
){
  sqlite3 *db;             /* The database connection */ //数据库的连接
  int idx = 0;             /* Index of a host parameter */ //主机参数索引
  int nextIndex = 1;       /* Index of next ? host parameter */ //主机参数的下一个索引
  int n;                   /* Length of a token prefix */ //标记前缀的长度
  int nToken;              /* Length of the parameter token */  // 参数标记的长度
  int i;                   /* Loop counter */ //Loop计数
  Mem *pVar;               /* Value of a host parameter */ //一个主机参数值
  StrAccum out;            /* Accumulate the output here */ //对输出的累计
  char zBase[100];         /* Initial working space */ //初始化工作空间


  db = p->db;
  sqlite3StrAccumInit(&out, zBase, sizeof(zBase), 
                      db->aLimit[SQLITE_LIMIT_LENGTH]);
  out.db = db;
  if( db->vdbeExecCnt>1 ){
    while( *zRawSql ){
      const char *zStart = zRawSql;
      while( *(zRawSql++)!='\n' && *zRawSql );
      sqlite3StrAccumAppend(&out, "-- ", 3);
      sqlite3StrAccumAppend(&out, zStart, (int)(zRawSql-zStart));
    }
  }else{
    while( zRawSql[0] ){
      n = findNextHostParameter(zRawSql, &nToken);
      assert( n>0 );
      sqlite3StrAccumAppend(&out, zRawSql, n);
      zRawSql += n;
      assert( zRawSql[0] || nToken==0 );
      if( nToken==0 ) break;
      if( zRawSql[0]=='?' ){
        if( nToken>1 ){
          assert( sqlite3Isdigit(zRawSql[1]) );
          sqlite3GetInt32(&zRawSql[1], &idx);
        }else{
          idx = nextIndex;
        }
      }else{
        assert( zRawSql[0]==':' || zRawSql[0]=='$' || zRawSql[0]=='@' );
        testcase( zRawSql[0]==':' );
        testcase( zRawSql[0]=='$' );
        testcase( zRawSql[0]=='@' );
        idx = sqlite3VdbeParameterIndex(p, zRawSql, nToken);
        assert( idx>0 );
      }
      zRawSql += nToken;
      nextIndex = idx + 1;
      assert( idx>0 && idx<=p->nVar );
      pVar = &p->aVar[idx-1];
      if( pVar->flags & MEM_Null ){
        sqlite3StrAccumAppend(&out, "NULL", 4);
      }else if( pVar->flags & MEM_Int ){
        sqlite3XPrintf(&out, "%lld", pVar->u.i);
      }else if( pVar->flags & MEM_Real ){
        sqlite3XPrintf(&out, "%!.15g", pVar->r);
      }else if( pVar->flags & MEM_Str ){
#ifndef SQLITE_OMIT_UTF16
        u8 enc = ENC(db);
        if( enc!=SQLITE_UTF8 ){
          Mem utf8;
          memset(&utf8, 0, sizeof(utf8));
          utf8.db = db;
          sqlite3VdbeMemSetStr(&utf8, pVar->z, pVar->n, enc, SQLITE_STATIC);
          sqlite3VdbeChangeEncoding(&utf8, SQLITE_UTF8);
          sqlite3XPrintf(&out, "'%.*q'", utf8.n, utf8.z);
          sqlite3VdbeMemRelease(&utf8);
        }else
#endif
        {
          sqlite3XPrintf(&out, "'%.*q'", pVar->n, pVar->z);
        }
      }else if( pVar->flags & MEM_Zero ){
        sqlite3XPrintf(&out, "zeroblob(%d)", pVar->u.nZero);
      }else{
        assert( pVar->flags & MEM_Blob );
        sqlite3StrAccumAppend(&out, "x'", 2);
        for(i=0; i<pVar->n; i++){
          sqlite3XPrintf(&out, "%02x", pVar->z[i]&0xff);
        }
        sqlite3StrAccumAppend(&out, "'", 1);
      }
    }
  }
  return sqlite3StrAccumFinish(&out);
}

#endif /* #ifndef SQLITE_OMIT_TRACE */

/*****************************************************************************
** The following code implements the data-structure explaining logic
** for the Vdbe.
*/

#if defined(SQLITE_ENABLE_TREE_EXPLAIN)

/*
** Allocate a new Explain object //分配一个新的解释工程
*/
void sqlite3ExplainBegin(Vdbe *pVdbe){
  if( pVdbe ){
    Explain *p;
    sqlite3BeginBenignMalloc();
    p = (Explain *)sqlite3MallocZero( sizeof(Explain) );
    if( p ){
      p->pVdbe = pVdbe;
      sqlite3_free(pVdbe->pExplain);
      pVdbe->pExplain = p;
      sqlite3StrAccumInit(&p->str, p->zBase, sizeof(p->zBase),
                          SQLITE_MAX_LENGTH);
      p->str.useMalloc = 2;
    }else{
      sqlite3EndBenignMalloc();
    }
  }
}

/*
** Return true if the Explain ends with a new-line. //如果该解释以一个新行结尾，返回真。
*/
static int endsWithNL(Explain *p){
  return p && p->str.zText && p->str.nChar
           && p->str.zText[p->str.nChar-1]=='\n';
}
    
/*
** Append text to the indentation
使用这个函数的目的主要是针对递归的结构，解释过程中会交错输出数据， 添加文本有力于递归数据的理解
*/
void sqlite3ExplainPrintf(Vdbe *pVdbe, const char *zFormat, ...){
  Explain *p;
  if( pVdbe && (p = pVdbe->pExplain)!=0 ){
    va_list ap;
    if( p->nIndent && endsWithNL(p) ){
      int n = p->nIndent;
      if( n>ArraySize(p->aIndent) ) n = ArraySize(p->aIndent);
      sqlite3AppendSpace(&p->str, p->aIndent[n-1]);
    }   
    va_start(ap, zFormat);
    sqlite3VXPrintf(&p->str, 1, zFormat, ap);
    va_end(ap);
  }
}

/*
** Append a '\n' if there is not already one. //如果还没有一个，附加一个'\n'。
*/
void sqlite3ExplainNL(Vdbe *pVdbe){
  Explain *p;
  if( pVdbe && (p = pVdbe->pExplain)!=0 && !endsWithNL(p) ){
    sqlite3StrAccumAppend(&p->str, "\n", 1);
  }
}

/*
** Push a new indentation level.  Subsequent lines will be indented
** so that they begin at the current cursor position. 推一个新的缩进水平。随后的行将是被缩进以便于他们在光标位置的起点。
 将待解释的结构体入栈
*/
void sqlite3ExplainPush(Vdbe *pVdbe){
  Explain *p;
  if( pVdbe && (p = pVdbe->pExplain)!=0 ){
    if( p->str.zText && p->nIndent<ArraySize(p->aIndent) ){
      const char *z = p->str.zText;
      int i = p->str.nChar-1;
      int x;
      while( i>=0 && z[i]!='\n' ){ i--; }
      x = (p->str.nChar - 1) - i;
      if( p->nIndent && x<p->aIndent[p->nIndent-1] ){
        x = p->aIndent[p->nIndent-1];
      }
      p->aIndent[p->nIndent] = x;
    }
    p->nIndent++;
  }
}

/*
** Pop the indentation stack by one level. 通过一个层次，出缩进栈
用下划线记录当前的索引位置，便于确定开始位置出栈
*/
void sqlite3ExplainPop(Vdbe *p){
  if( p && p->pExplain ) p->pExplain->nIndent--;
}

/*
** Free the indentation structure 释放缩进结构。
 释放结构体
*/
void sqlite3ExplainFinish(Vdbe *pVdbe){
  if( pVdbe && pVdbe->pExplain ){
    sqlite3_free(pVdbe->zExplain);
    sqlite3ExplainNL(pVdbe);
    pVdbe->zExplain = sqlite3StrAccumFinish(&pVdbe->pExplain->str);
    sqlite3_free(pVdbe->pExplain);
    pVdbe->pExplain = 0;
    sqlite3EndBenignMalloc();
  }
}

/*
** Return the explanation of a virtual machine. 返回对虚拟机的解释。
*/
const char *sqlite3VdbeExplanation(Vdbe *pVdbe){
  return (pVdbe && pVdbe->zExplain) ? pVdbe->zExplain : 0;
}
#endif /* defined(SQLITE_DEBUG) */
