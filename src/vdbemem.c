/*   操作”Mem”数据结构的函数 实现对VDBE的存储结构Mem的操作
** VDBE(虚拟数据库引擎)是SQLite的核心：它上面的各模块都是用于创建VDBE程序，它下面的各模块都是用于执行VDBE程序，每次执行一条指令。
** 2004 May 26
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**版权属作者所有，下面的祝福写的真好：
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
**
** This file contains code use to manipulate(操纵) "Mem" structure.  A "Mem"
** stores a single value in the VDBE[虚拟数据库引擎(Virtual DataBase Engine,VDBE)].  Mem is an opaque(不透明的) structure visible
** only within the VDBE.  Interface routines refer to a Mem using the
** name sqlite_value
** 这个文件包含的代码是用于操纵"Mem"这个数据结构.
** 一个"Mem"存储VDBE中的一个单一的数据.
** Mem 是一个不透明的数据结构,它只对于VDBE是可见的.
** 
*/
#include "sqliteInt.h"
#include "vdbeInt.h"

/*
** If pMem is an object with a valid(有效的) string representation(表示), this routine(常规)
** ensures the internal(内部的) encoding for the string representation is
** 'desiredEnc', one of SQLITE_UTF8, SQLITE_UTF16LE or SQLITE_UTF16BE.
** 如果pMem是一个对象,这个对象有着有效的字符串表示,
** 这个routine确保对字符串表示的内部编码是"desiredEnc"
** If pMem is not a string object(字符串对象), or the encoding of the string
** representation is already stored using the requested encoding(被要求的编码方式), then this
** routine is a no-op(空操作).
** 如果pMem不是一个字符串对象,或者字符串表示的编码方式已经用被要求的编码方式存储过了,那么这个routine是一个no-op.
** SQLITE_OK is returned if the conversion(转变,改变) is successful (or not required).
** SQLITE_NOMEM may be returned if a malloc() fails during conversion
** between formats.
** 如果这个转变成功了(或者没有被要求转变)那么SQLITE_OK将被返回.
** 如果一个malloc()在格式化转换过程中失败了,那么SQLITE_NOMEM也许会被返回.
*/
/* sqlite3VdbeChangeEncoding函数用来改变编码方式
** Mem结构会给出同一个值的多种表示法(string、integer等)。因此Mem结构有时需要转化为另一种表示方式
** 如字符串U8转为U16，整数转为实数或二进制数
*/
int sqlite3VdbeChangeEncoding(Mem *pMem, int desiredEnc){
  int rc;
  assert( (pMem->flags&MEM_RowSet)==0 );
  assert( desiredEnc==SQLITE_UTF8 || desiredEnc==SQLITE_UTF16LE
           || desiredEnc==SQLITE_UTF16BE );
  if( !(pMem->flags&MEM_Str) || pMem->enc==desiredEnc ){
    return SQLITE_OK;
  }
  assert( pMem->db==0 || sqlite3_mutex_held(pMem->db->mutex) );
#ifdef SQLITE_OMIT_UTF16
  return SQLITE_ERROR;
#else

  /* MemTranslate() may return SQLITE_OK or SQLITE_NOMEM. If NOMEM is returned,
  ** then the encoding of the value may not have changed.
  ** MemTranslate()函数也许会返回SQLITE_OK 或者 SQLITE_NOMEM
  ** 如果NOMEM返回了,那么值的编码方式不会改变.
  */
  rc = sqlite3VdbeMemTranslate(pMem, (u8)desiredEnc);-- 传递函数值
  assert(rc==SQLITE_OK    || rc==SQLITE_NOMEM);
  assert(rc==SQLITE_OK    || pMem->enc!=desiredEnc);
  assert(rc==SQLITE_NOMEM || pMem->enc==desiredEnc);
  return rc;
#endif
}

/*
** Make sure pMem->z points to a writable allocation(分配) of at least
** n bytes.
** 确保pMem->z指向一个可写的至少有n个字节的分配单元.
** If the third argument(论据,论点) passed to this function is true, then memory
** cell pMem must contain a string or blob. In this case the content is
** preserved(被保护). Otherwise, if the third parameter to this function is false,
** any current string or blob value may be discarded(丢弃).
** 如果被传到这个函数的第三方论点是真的,那么存储单元pMem必须包含一个字符串或者blob.
** 在这种情况下,内容是被保护的.否则,如果这个函数的第三方参数是错误的,任何当前的string或者blob值也许会被丢弃.
** This function sets the MEM_Dyn flag(旗帜,标志) and clears any xDel callback.
** It also clears MEM_Ephem and MEM_Static. If the preserve flag is 
** not set, Mem.n is zeroed.
*/
int sqlite3VdbeMemGrow(Mem *pMem, int n, int preserve){
  assert( 1 >=
    ((pMem->zMalloc && pMem->zMalloc==pMem->z) ? 1 : 0) +
    (((pMem->flags&MEM_Dyn)&&pMem->xDel) ? 1 : 0) + 
    ((pMem->flags&MEM_Ephem) ? 1 : 0) + 
    ((pMem->flags&MEM_Static) ? 1 : 0)
  );
  assert( (pMem->flags&MEM_RowSet)==0 );

  /* If the preserve flag is set to true, then the memory cell must already
  ** contain a valid string or blob value.
  ** 如果被保护的标志设置成真,那么内存单元必须已经包含一个有效的字符串或者blob值.
  */
  assert( preserve==0 || pMem->flags&(MEM_Blob|MEM_Str) );

  if( n<32 ) n = 32;
  if( sqlite3DbMallocSize(pMem->db, pMem->zMalloc)<n ){
    if( preserve && pMem->z==pMem->zMalloc ){
      pMem->z = pMem->zMalloc = sqlite3DbReallocOrFree(pMem->db, pMem->z, n);
      preserve = 0;
    }else{
      sqlite3DbFree(pMem->db, pMem->zMalloc);
      pMem->zMalloc = sqlite3DbMallocRaw(pMem->db, n);
    }
  }

  if( pMem->z && preserve && pMem->zMalloc && pMem->z!=pMem->zMalloc ){
    memcpy(pMem->zMalloc, pMem->z, pMem->n);
  }
  if( pMem->flags&MEM_Dyn && pMem->xDel ){
    assert( pMem->xDel!=SQLITE_DYNAMIC );
    pMem->xDel((void *)(pMem->z));
  }

  pMem->z = pMem->zMalloc;
  if( pMem->z==0 ){
    pMem->flags = MEM_Null;
  }else{
    pMem->flags &= ~(MEM_Ephem|MEM_Static);
  }
  pMem->xDel = 0;
  return (pMem->z ? SQLITE_OK : SQLITE_NOMEM);
}

/*
** Make the given Mem object MEM_Dyn.  In other words, make it so
** that any TEXT or BLOB content is stored in memory obtained from
** malloc().  In this way, we know that the memory is safe to be
** overwritten or altered.

** malloc()执行成功返回SQLITE_OK 执行失败返回SQLITE_NOMEM.
** Return SQLITE_OK on success or SQLITE_NOMEM if malloc fails.
*/
int sqlite3VdbeMemMakeWriteable(Mem *pMem){
  int f;
  assert( pMem->db==0 || sqlite3_mutex_held(pMem->db->mutex) );
  assert( (pMem->flags&MEM_RowSet)==0 );
  ExpandBlob(pMem);
  f = pMem->flags;
  if( (f&(MEM_Str|MEM_Blob)) && pMem->z!=pMem->zMalloc ){
    if( sqlite3VdbeMemGrow(pMem, pMem->n + 2, 1) ){
      return SQLITE_NOMEM;
    }
    pMem->z[pMem->n] = 0;
    pMem->z[pMem->n+1] = 0;
    pMem->flags |= MEM_Term;
#ifdef SQLITE_DEBUG
    pMem->pScopyFrom = 0;
#endif
  }

  return SQLITE_OK;
}

/*
** If the given Mem* has a zero-filled tail, turn it into an ordinary
** blob stored in dynamically allocated space.
** 如果给的Mem*指针是以0填充的尾巴，将它转化成普通的blob类型数据存储在动态分布的空间里
*/
#ifndef SQLITE_OMIT_INCRBLOB
int sqlite3VdbeMemExpandBlob(Mem *pMem){
  if( pMem->flags & MEM_Zero ){
    int nByte;
    assert( pMem->flags&MEM_Blob );
    assert( (pMem->flags&MEM_RowSet)==0 );
    assert( pMem->db==0 || sqlite3_mutex_held(pMem->db->mutex) );

    /* Set nByte to the number of bytes required to store the expanded blob. */
    nByte = pMem->n + pMem->u.nZero;
    if( nByte<=0 ){
      nByte = 1;
    }
    if( sqlite3VdbeMemGrow(pMem, nByte, 1) ){
      return SQLITE_NOMEM;
    }

    memset(&pMem->z[pMem->n], 0, pMem->u.nZero);
    pMem->n += pMem->u.nZero;
    pMem->flags &= ~(MEM_Zero|MEM_Term);
  }
  return SQLITE_OK;
}
#endif


/*
** Make sure the given Mem is \u0000 terminated.
** 确保给出的Mem对象是以\u0000结尾的.
*/
int sqlite3VdbeMemNulTerminate(Mem *pMem){
  assert( pMem->db==0 || sqlite3_mutex_held(pMem->db->mutex) );
  if( (pMem->flags & MEM_Term)!=0 || (pMem->flags & MEM_Str)==0 ){
    return SQLITE_OK;   /* Nothing to do */
  }
  if( sqlite3VdbeMemGrow(pMem, pMem->n+2, 1) ){
    return SQLITE_NOMEM;
  }
  pMem->z[pMem->n] = 0;
  pMem->z[pMem->n+1] = 0;
  pMem->flags |= MEM_Term;
  return SQLITE_OK;
}

/*
** Add MEM_Str to the set of representations for the given Mem.  Numbers
** are converted using sqlite3_snprintf().  Converting a BLOB to a string
** is a no-op.
**
** Existing representations MEM_Int and MEM_Real are *not* invalidated.
** 存在的
** A MEM_Null value will never be passed to this function. This function is
** used for converting values to text for returning to the user (i.e. via
** sqlite3_value_text()), or for ensuring that values to be used as btree
** keys are strings. In the former case a NULL pointer is returned the
** user and the later is an internal programming error.
** 一个MEM_NULL值永远不会被传递到这个函数.这个函数是用来把值转换成文本,然后反馈给用户(比如说sqlite3_value_text()函数).
** 或者确保 这些值将被用作b树的关键字.在前一种情况,一个空指针被返回给用户,后一种情况是一种内部的编码错误.
*/
int sqlite3VdbeMemStringify(Mem *pMem, int enc){
  int rc = SQLITE_OK;
  int fg = pMem->flags;
  const int nByte = 32;

  assert( pMem->db==0 || sqlite3_mutex_held(pMem->db->mutex) );
  assert( !(fg&MEM_Zero) );
  assert( !(fg&(MEM_Str|MEM_Blob)) );
  assert( fg&(MEM_Int|MEM_Real) );
  assert( (pMem->flags&MEM_RowSet)==0 );
  assert( EIGHT_BYTE_ALIGNMENT(pMem) );


  if( sqlite3VdbeMemGrow(pMem, nByte, 0) ){
    return SQLITE_NOMEM;
  }

  /* For a Real or Integer, use sqlite3_mprintf() to produce the UTF-8
  ** string representation of the value. Then, if the required encoding
  ** is UTF-16le or UTF-16be do a translation.
  ** 对于一个整数或者实数,使用sqlite3_mprintf()函数来产生代表这个值的UTF-8编码方式的字符串表示.
  ** 然后,如果这个被要求的编码方式是UTF-16le或者UTF-16be那就做一个转换.
  ** FIX ME: It would be better if sqlite3_snprintf() could do UTF-16.
  */
  if( fg & MEM_Int ){
    sqlite3_snprintf(nByte, pMem->z, "%lld", pMem->u.i);
  }else{
    assert( fg & MEM_Real );
    sqlite3_snprintf(nByte, pMem->z, "%!.15g", pMem->r);
  }
  pMem->n = sqlite3Strlen30(pMem->z);
  pMem->enc = SQLITE_UTF8;
  pMem->flags |= MEM_Str|MEM_Term;
  sqlite3VdbeChangeEncoding(pMem, enc);
  return rc;
}

/*
** Memory cell pMem contains the context of an aggregate function.
** This routine calls the finalize method for that function.  The
** result of the aggregate is stored back into pMem.
** 内存单元pMem包含一个合并的函数内容.这个routine为那个函数调用完成的方法.
** 这个合集的结果被存储到pMem中.
** Return SQLITE_ERROR if the finalizer reports an error.  SQLITE_OK
** otherwise.如果完成函数报告了一个错误那么返回SQLITE_ERROR,否则返回SQLITE_OK.
*/
int sqlite3VdbeMemFinalize(Mem *pMem, FuncDef *pFunc){
  int rc = SQLITE_OK;
  if( ALWAYS(pFunc && pFunc->xFinalize) ){
    sqlite3_context ctx;
    assert( (pMem->flags & MEM_Null)!=0 || pFunc==pMem->u.pDef );
    assert( pMem->db==0 || sqlite3_mutex_held(pMem->db->mutex) );
    memset(&ctx, 0, sizeof(ctx));
    ctx.s.flags = MEM_Null;
    ctx.s.db = pMem->db;
    ctx.pMem = pMem;
    ctx.pFunc = pFunc;
    pFunc->xFinalize(&ctx); /* IMP: R-24505-23230 */
    assert( 0==(pMem->flags&MEM_Dyn) && !pMem->xDel );
    sqlite3DbFree(pMem->db, pMem->zMalloc);
    memcpy(pMem, &ctx.s, sizeof(ctx.s));
    rc = ctx.isError;
  }
  return rc;
}

/*
** If the memory cell contains a string value that must be freed by
** invoking an external callback, free it now. Calling this function
** does not free any Mem.zMalloc buffer.
** 如果一个存储单元包含一个字符串值,这个值必须通过条用外部的回调函数才能释放,那就现在释放了.
** 
*/
void sqlite3VdbeMemReleaseExternal(Mem *p){
  assert( p->db==0 || sqlite3_mutex_held(p->db->mutex) );
  if( p->flags&MEM_Agg ){
    sqlite3VdbeMemFinalize(p, p->u.pDef);
    assert( (p->flags & MEM_Agg)==0 );
    sqlite3VdbeMemRelease(p);
  }else if( p->flags&MEM_Dyn && p->xDel ){
    assert( (p->flags&MEM_RowSet)==0 );
    assert( p->xDel!=SQLITE_DYNAMIC );
    p->xDel((void *)p->z);
    p->xDel = 0;
  }else if( p->flags&MEM_RowSet ){
    sqlite3RowSetClear(p->u.pRowSet);
  }else if( p->flags&MEM_Frame ){
    sqlite3VdbeMemSetNull(p);
  }
}

/*
** Release any memory held by the Mem. This may leave the Mem in an
** inconsistent state, for example with (Mem.z==0) and
** (Mem.type==SQLITE_TEXT).
** 释放被Mem存储的任何内存单元.这也许会使得Mem处于一种不一致的状态.
*/
void sqlite3VdbeMemRelease(Mem *p){
  VdbeMemRelease(p);
  sqlite3DbFree(p->db, p->zMalloc);
  p->z = 0;
  p->zMalloc = 0;
  p->xDel = 0;
}

/*
** Convert a 64-bit IEEE double into a 64-bit signed integer.
** If the double is too large, return 0x8000000000000000.
**
** Most systems appear to do this simply by assigning
** variables and without the extra range tests.  But
** there are reports that windows throws an expection
** if the floating point value is out of range. (See ticket #2880.)
** Because we do not completely understand the problem, we will
** take the conservative approach and always do range tests
** before attempting the conversion.
** 大部分的系统似乎仅仅通过分配变量并且不需要额外的范围测试.
** 如果floating指针值超出了范围,....
** 因为我们并没有完全理解这个问题,我们必须take保守的途径并且总是做范围测试在...之前.
*/
static i64 doubleToInt64(double r){
#ifdef SQLITE_OMIT_FLOATING_POINT
  /* When floating-point is omitted, double and int64 are the same thing */
  return r;
#else
  /*
  ** Many compilers we encounter do not define constants for the
  ** minimum and maximum 64-bit integers, or they define them
  ** inconsistently.  And many do not understand the "LL" notation.
  ** So we define our own static constants here using nothing
  ** larger than a 32-bit integer constant.
  ** 我们面对的许多编译器并没有为最小值和最大的64位整数定义常量,或者并没有定义成常量.
  ** 并且许多人并不理解"LL"符号的含义.
  ** 所以我们用不多于32位整数的常量定义我们自己的静态常量
  */
  static const i64 maxInt = LARGEST_INT64;
  static const i64 minInt = SMALLEST_INT64;

  if( r<(double)minInt ){
    return minInt;
  }else if( r>(double)maxInt ){
    /* minInt is correct here - not maxInt.  It turns out that assigning
    ** a very large positive number to an integer results in a very large
    ** negative integer.  This makes no sense, but it is what x86 hardware
    ** does so for compatibility we will do the same in software. */
    return minInt;
  }else{
    return (i64)r;
  }
#endif
}

/*
** Return some kind of integer value which is the best we can do
** at representing the value that *pMem describes as an integer.
** If pMem is an integer, then the value is exact.  If pMem is
** a floating-point then the value returned is the integer part.
** If pMem is a string or blob, then we make an attempt to convert
** it into a integer and return that.  If pMem represents an
** an SQL-NULL value, return 0.
** 返回某些整型值,这些整型值是我们在表示用  把*pMem描述为一个整数时 最好的方法.
** 如果pMem是一个整数,那么这个值是准确的.
** 如果pMem一个浮点型的数那么被返还的值是整数部分.
** 如果pMem是一个string类型或者blob类型,那么我们尽力把它转变成一个interger类型并且返回它.
** 如果pMem代表一个SQL-NULL值,那么返回0.
** If pMem represents a string value, its encoding might be changed.
** 如果pMem代表一个string值,它的编码方式必须被改变.
*/
i64 sqlite3VdbeIntValue(Mem *pMem){
  int flags;
  assert( pMem->db==0 || sqlite3_mutex_held(pMem->db->mutex) );
  assert( EIGHT_BYTE_ALIGNMENT(pMem) );
  flags = pMem->flags;
  if( flags & MEM_Int ){
    return pMem->u.i;
  }else if( flags & MEM_Real ){
    return doubleToInt64(pMem->r);
  }else if( flags & (MEM_Str|MEM_Blob) ){
    i64 value = 0;
    assert( pMem->z || pMem->n==0 );
    testcase( pMem->z==0 );
    sqlite3Atoi64(pMem->z, &value, pMem->n, pMem->enc);
    return value;
  }else{
    return 0;
  }
}

/*
** Return the best representation of pMem that we can get into a
** double.  If pMem is already a double or an integer, return its
** value.  If it is a string or blob, try to convert it to a double.
** If it is a NULL, return 0.0.
** 返回最能代表pMem的表示值,这个pMem是一个double类型的值.
** 如果pMem已经是一个double类型或者整型值,那么就返回它的值.
** 如果它是一个string或者blob类型,那么试着把它转化成一个double类型的值.
** 如果它是一个空值,那么返回0.0
*/
double sqlite3VdbeRealValue(Mem *pMem){
  assert( pMem->db==0 || sqlite3_mutex_held(pMem->db->mutex) );
  assert( EIGHT_BYTE_ALIGNMENT(pMem) );
  if( pMem->flags & MEM_Real ){
    return pMem->r;
  }else if( pMem->flags & MEM_Int ){
    return (double)pMem->u.i;
  }else if( pMem->flags & (MEM_Str|MEM_Blob) ){
    /* (double)0 In case of SQLITE_OMIT_FLOATING_POINT... */
    double val = (double)0;
    sqlite3AtoF(pMem->z, &val, pMem->n, pMem->enc);
    return val;
  }else{
    /* (double)0 In case of SQLITE_OMIT_FLOATING_POINT... */
    return (double)0;
  }
}

/*
** The MEM structure is already a MEM_Real.  Try to also make it a
** MEM_Int if we can.
** MEM结构已经是一个实类型的值.如果可以的话那么我们试着把它变成一个int类型的值.
*/
void sqlite3VdbeIntegerAffinity(Mem *pMem){
  assert( pMem->flags & MEM_Real );
  assert( (pMem->flags & MEM_RowSet)==0 );
  assert( pMem->db==0 || sqlite3_mutex_held(pMem->db->mutex) );
  assert( EIGHT_BYTE_ALIGNMENT(pMem) );

  pMem->u.i = doubleToInt64(pMem->r);

  /* Only mark the value as an integer if
  **
  **    (1) the round-trip conversion real->int->real is a no-op, and
  **    (2) The integer is neither the largest nor the smallest
  **        possible integer (ticket #3922)
  **
  ** The second and third terms in the following conditional enforces
  ** the second condition under the assumption that addition overflow causes
  ** values to wrap around.  On x86 hardware, the third term is always
  ** true and could be omitted.  But we leave it in because other
  ** architectures might behave differently.
  ** 第二个或者第三个
  */
  if( pMem->r==(double)pMem->u.i
   && pMem->u.i>SMALLEST_INT64
#if defined(__i486__) || defined(__x86_64__)
   && ALWAYS(pMem->u.i<LARGEST_INT64)
#else
   && pMem->u.i<LARGEST_INT64
#endif
  ){
    pMem->flags |= MEM_Int;
  }
}

/*
** Convert pMem to type integer.  Invalidate any prior representations.
** 把pMem类型转化成integer类型. 使任何先前的表示值都失效.
*/
int sqlite3VdbeMemIntegerify(Mem *pMem){
  assert( pMem->db==0 || sqlite3_mutex_held(pMem->db->mutex) );
  assert( (pMem->flags & MEM_RowSet)==0 );
  assert( EIGHT_BYTE_ALIGNMENT(pMem) );

  pMem->u.i = sqlite3VdbeIntValue(pMem);
  MemSetTypeFlag(pMem, MEM_Int);
  return SQLITE_OK;
}

/*
** Convert pMem so that it is of type MEM_Real.
** Invalidate any prior representations.
** 转化pMem 以便它是MEM_Ral类型的,使任何先前的表示都失效
*/
int sqlite3VdbeMemRealify(Mem *pMem){
  assert( pMem->db==0 || sqlite3_mutex_held(pMem->db->mutex) );
  assert( EIGHT_BYTE_ALIGNMENT(pMem) );

  pMem->r = sqlite3VdbeRealValue(pMem);
  MemSetTypeFlag(pMem, MEM_Real);
  return SQLITE_OK;
}

/*
** Convert pMem so that it has types MEM_Real or MEM_Int or both.
** Invalidate any prior representations.
** 把pMem对象转化为MEM_Real 类型或者 MEM_Int类型,或者两者兼有.使任何先前的类型都失效.
** Every effort is made to force the conversion, even if the input
** is a string that does not look completely like a number.  Convert
** as much of the string as we can and ignore the rest.
** 任何的努力都是为了强制类型转化,即使输入的是一点也不像一个数字的字符串类型,
** 也要尽可能的把字符串类型转换成目标类型并忽视不能转换的字符串.
*/
int sqlite3VdbeMemNumerify(Mem *pMem){
  if( (pMem->flags & (MEM_Int|MEM_Real|MEM_Null))==0 ){
    assert( (pMem->flags & (MEM_Blob|MEM_Str))!=0 );
    assert( pMem->db==0 || sqlite3_mutex_held(pMem->db->mutex) );
    if( 0==sqlite3Atoi64(pMem->z, &pMem->u.i, pMem->n, pMem->enc) ){
      MemSetTypeFlag(pMem, MEM_Int);
    }else{
      pMem->r = sqlite3VdbeRealValue(pMem);
      MemSetTypeFlag(pMem, MEM_Real);
      sqlite3VdbeIntegerAffinity(pMem);
    }
  }
  assert( (pMem->flags & (MEM_Int|MEM_Real|MEM_Null))!=0 );
  pMem->flags &= ~(MEM_Str|MEM_Blob);
  return SQLITE_OK;
}

/*
** Delete any previous value and set the value stored in *pMem to NULL.
** 删除所有先前的值,并且把在*pMem中存储的值置空.
*/
void sqlite3VdbeMemSetNull(Mem *pMem){
  if( pMem->flags & MEM_Frame ){
    VdbeFrame *pFrame = pMem->u.pFrame;
    pFrame->pParent = pFrame->v->pDelFrame;
    pFrame->v->pDelFrame = pFrame;
  }
  if( pMem->flags & MEM_RowSet ){
    sqlite3RowSetClear(pMem->u.pRowSet);
  }
  MemSetTypeFlag(pMem, MEM_Null);
  pMem->type = SQLITE_NULL;
}

/*
** Delete any previous value and set the value to be a BLOB of length
** n containing all zeros.
** 删除任何先前的值,并且把值设置成BLOB的长度n(包含所有的空值???)
*/
void sqlite3VdbeMemSetZeroBlob(Mem *pMem, int n){
  sqlite3VdbeMemRelease(pMem);
  pMem->flags = MEM_Blob|MEM_Zero;
  pMem->type = SQLITE_BLOB;
  pMem->n = 0;
  if( n<0 ) n = 0;
  pMem->u.nZero = n;
  pMem->enc = SQLITE_UTF8;

#ifdef SQLITE_OMIT_INCRBLOB
  sqlite3VdbeMemGrow(pMem, n, 0);
  if( pMem->z ){
    pMem->n = n;
    memset(pMem->z, 0, n);
  }
#endif
}

/*
** Delete any previous value and set the value stored in *pMem to val,
** manifest type INTEGER.
** 删除任何原先存在的值,并且把在*pMem中存储的值设置成val,它表示integer类型.
*/
void sqlite3VdbeMemSetInt64(Mem *pMem, i64 val){
  sqlite3VdbeMemRelease(pMem);
  pMem->u.i = val;
  pMem->flags = MEM_Int;
  pMem->type = SQLITE_INTEGER;
}

#ifndef SQLITE_OMIT_FLOATING_POINT
/*
** Delete any previous value and set the value stored in *pMem to val,
** manifest type REAL.
** 删除所有以前存在的值,并把存储在pMem里面的值给val变量
*/
void sqlite3VdbeMemSetDouble(Mem *pMem, double val){
  if( sqlite3IsNaN(val) ){
    sqlite3VdbeMemSetNull(pMem);//把在pMem中存储的值置空
  }else{
    sqlite3VdbeMemRelease(pMem);//释放被Mem存储的任何内存单元
    pMem->r = val;
    pMem->flags = MEM_Real;
    pMem->type = SQLITE_FLOAT;
  }
}
#endif

/*
** Delete any previous value and set the value of pMem to be an
** empty boolean index.
** 删除任何先前的值并且把pMem的值设置成一个空的boolean类型的索引.
*/
void sqlite3VdbeMemSetRowSet(Mem *pMem){
  sqlite3 *db = pMem->db;
  assert( db!=0 );
  assert( (pMem->flags & MEM_RowSet)==0 );
  sqlite3VdbeMemRelease(pMem);
  pMem->zMalloc = sqlite3DbMallocRaw(db, 64);
  if( db->mallocFailed ){
    pMem->flags = MEM_Null;
  }else{
    assert( pMem->zMalloc );
    pMem->u.pRowSet = sqlite3RowSetInit(db, pMem->zMalloc, 
                                       sqlite3DbMallocSize(db, pMem->zMalloc));
    assert( pMem->u.pRowSet!=0 );
    pMem->flags = MEM_RowSet;
  }
}

/*
** Return true if the Mem object contains a TEXT or BLOB that is
** too large - whose size exceeds SQLITE_MAX_LENGTH.
** 如果Mem对象包含的TEXT或者BLOB太大--他们所占的存储空间超过了SQLITE_MAX_LENGTH.那么就返回真.
*/
int sqlite3VdbeMemTooBig(Mem *p){
  assert( p->db!=0 );
  if( p->flags & (MEM_Str|MEM_Blob) ){
    int n = p->n;
    if( p->flags & MEM_Zero ){
      n += p->u.nZero;
    }
    return n>p->db->aLimit[SQLITE_LIMIT_LENGTH];
  }
  return 0; 
}

#ifdef SQLITE_DEBUG
/*
** This routine prepares a memory cell for modication by breaking
** its link to a shallow copy and by marking any current shallow
** copies of this cell as invalid.
** 
** This is used for testing and debugging only - to make sure shallow
** copies are not misused.
** 这个函数仅仅是用来测试和调试的--来确保这个浅拷贝不会被误用.
*/
void sqlite3VdbeMemAboutToChange(Vdbe *pVdbe, Mem *pMem){
  int i;
  Mem *pX;
  for(i=1, pX=&pVdbe->aMem[1]; i<=pVdbe->nMem; i++, pX++){
    if( pX->pScopyFrom==pMem ){
      pX->flags |= MEM_Invalid;
      pX->pScopyFrom = 0;
    }
  }
  pMem->pScopyFrom = 0;
}
#endif /* SQLITE_DEBUG */

/*
** Size of struct Mem not including the Mem.zMalloc member.
*/
#define MEMCELLSIZE (size_t)(&(((Mem *)0)->zMalloc))

/*
** Make an shallow copy of pFrom into pTo.  Prior contents of
** pTo are freed.  The pFrom->z field is not duplicated.  If
** pFrom->z is used, then pTo->z points to the same thing as pFrom->z
** and flags gets srcType (either MEM_Ephem or MEM_Static).
** 把pFrom浅拷贝到pTo里面.pTo中原先存储的内容被释放.
** pFrom->z区域没有被复制.
** 如果pFrom->z被使用了,那么pTo->z指向和pFrom->z一样的相同的东西并且....??
*/
void sqlite3VdbeMemShallowCopy(Mem *pTo, const Mem *pFrom, int srcType){
  assert( (pFrom->flags & MEM_RowSet)==0 );
  VdbeMemRelease(pTo);
  memcpy(pTo, pFrom, MEMCELLSIZE);
  pTo->xDel = 0;
  if( (pFrom->flags&MEM_Static)==0 ){
    pTo->flags &= ~(MEM_Dyn|MEM_Static|MEM_Ephem);
    assert( srcType==MEM_Ephem || srcType==MEM_Static );
    pTo->flags |= srcType;
  }
}

/*
** Make a full copy of pFrom into pTo.  Prior contents of pTo are
** freed before the copy is made.
** 把pFrom完全复制到pTo里面.pTo里以前的内容在复制完成之前被释放.
*/
int sqlite3VdbeMemCopy(Mem *pTo, const Mem *pFrom){
  int rc = SQLITE_OK;

  assert( (pFrom->flags & MEM_RowSet)==0 );
  VdbeMemRelease(pTo);
  memcpy(pTo, pFrom, MEMCELLSIZE);
  pTo->flags &= ~MEM_Dyn;

  if( pTo->flags&(MEM_Str|MEM_Blob) ){
    if( 0==(pFrom->flags&MEM_Static) ){
      pTo->flags |= MEM_Ephem;
      rc = sqlite3VdbeMemMakeWriteable(pTo);
    }
  }

  return rc;
}

/*
** Transfer the contents of pFrom to pTo. Any existing value in pTo is
** freed. If pFrom contains ephemeral data, a copy is made.
** 移动pFrom内存单元中的数据到pTo.在pTo数据单元中的任何存在的数据都会被释放.
** 如果pFrom包含短暂的数据,一份复制就生成了.
** pFrom contains an SQL NULL when this routine returns.
** 当这个操作返回时,pFrom单元中的就只包含一个空的SQL语句.
*/
void sqlite3VdbeMemMove(Mem *pTo, Mem *pFrom){
  assert( pFrom->db==0 || sqlite3_mutex_held(pFrom->db->mutex) );
  assert( pTo->db==0 || sqlite3_mutex_held(pTo->db->mutex) );
  assert( pFrom->db==0 || pTo->db==0 || pFrom->db==pTo->db );

  sqlite3VdbeMemRelease(pTo);
  memcpy(pTo, pFrom, sizeof(Mem));
  pFrom->flags = MEM_Null;
  pFrom->xDel = 0;
  pFrom->zMalloc = 0;
}

/*
** Change the value of a Mem to be a string or a BLOB.
** 把Mem结构中的数据转换成string或者BLOB类型.
** The memory management strategy depends on the value of the xDel
** parameter. If the value passed is SQLITE_TRANSIENT, then the 
** string is copied into a (possibly existing) buffer managed by the 
** Mem structure. Otherwise, any existing buffer is freed and the
** pointer copied.
** 内存管理策略依赖于xDel参数的值.
** 如果通过的数据是SQLITE_TRANSIENT,那么string被拷贝到一个被Mem结构所管理的缓冲区(很可能存在).
** 否则,任何存在的缓冲区被释放并且被拷贝.
** If the string is too large (if it exceeds the SQLITE_LIMIT_LENGTH
** size limit) then no memory allocation occurs.  If the string can be
** stored without allocating memory, then it is.  If a memory allocation
** is required to store the string, then value of pMem is unchanged.  In
** either case, SQLITE_TOOBIG is returned.
** 如果那个string类型的数据太大(如果它超过SQLITE_LIMIT_LENGTH所限定的长度)那么任何内存分配发生.
** 如果那个string类型的数据不用被分配的内存单元就能被存储,then it is.--!
** 如果一个内存战友单元被要求存储string类型的数据,那么pMem的值不会被改变.否则返回SQLITE_TOOBIG
*/
int sqlite3VdbeMemSetStr(
  Mem *pMem,          /* Memory cell to set to string value 内存单元被设置成string数据类型 */
  const char *z,      /* String pointer string类型的指针 */
  int n,              /* Bytes in string, or negative */
  u8 enc,             /* Encoding of z.  0 for BLOBs */
  void (*xDel)(void*) /* Destructor function 析构函数*/
){
  int nByte = n;      /* New value for pMem->n */
  int iLimit;         /* Maximum allowed string or blob size */
  u16 flags = 0;      /* New value for pMem->flags */

  assert( pMem->db==0 || sqlite3_mutex_held(pMem->db->mutex) );
  assert( (pMem->flags & MEM_RowSet)==0 );

  /* If z is a NULL pointer, set pMem to contain an SQL NULL.
   * 如果z是空指针，设置pMem包含一个SQL空语句*/
  if( !z ){
    sqlite3VdbeMemSetNull(pMem);
    return SQLITE_OK;
  }

  if( pMem->db ){
    iLimit = pMem->db->aLimit[SQLITE_LIMIT_LENGTH];
  }else{
    iLimit = SQLITE_MAX_LENGTH;
  }
  flags = (enc==0?MEM_Blob:MEM_Str);
  if( nByte<0 ){
    assert( enc!=0 );
    if( enc==SQLITE_UTF8 ){
      for(nByte=0; nByte<=iLimit && z[nByte]; nByte++){}
    }else{
      for(nByte=0; nByte<=iLimit && (z[nByte] | z[nByte+1]); nByte+=2){}
    }
    flags |= MEM_Term;
  }

  /* The following block sets the new values of Mem.z and Mem.xDel. It
  ** also sets a flag in local variable "flags" to indicate the memory
  ** management (one of MEM_Dyn or MEM_Static).
  ** 接下来的块是为Mem.z和Mem.xDel设置新值.它也用来在当前的局部变量"flags"设置一个标志以表明为存储管理
  */
  if( xDel==SQLITE_TRANSIENT ){
    int nAlloc = nByte;
    if( flags&MEM_Term ){
      nAlloc += (enc==SQLITE_UTF8?1:2);
    }
    if( nByte>iLimit ){
      return SQLITE_TOOBIG;
    }
    if( sqlite3VdbeMemGrow(pMem, nAlloc, 0) ){
      return SQLITE_NOMEM;
    }
    memcpy(pMem->z, z, nAlloc);
  }else if( xDel==SQLITE_DYNAMIC ){
    sqlite3VdbeMemRelease(pMem);
    pMem->zMalloc = pMem->z = (char *)z;
    pMem->xDel = 0;
  }else{
    sqlite3VdbeMemRelease(pMem);
    pMem->z = (char *)z;
    pMem->xDel = xDel;
    flags |= ((xDel==SQLITE_STATIC)?MEM_Static:MEM_Dyn);
  }

  pMem->n = nByte;
  pMem->flags = flags;
  pMem->enc = (enc==0 ? SQLITE_UTF8 : enc);
  pMem->type = (enc==0 ? SQLITE_BLOB : SQLITE_TEXT);

#ifndef SQLITE_OMIT_UTF16
  if( pMem->enc!=SQLITE_UTF8 && sqlite3VdbeMemHandleBom(pMem) ){
    return SQLITE_NOMEM;
  }
#endif

  if( nByte>iLimit ){
    return SQLITE_TOOBIG;
  }

  return SQLITE_OK;
}

/*
** Compare the values contained by the two memory cells, returning
** negative, zero or positive if pMem1 is less than, equal to, or greater
** than pMem2. Sorting order is NULL's first, followed by numbers (integers
** and reals) sorted numerically, followed by text ordered by the collating
** sequence pColl and finally blob's ordered by memcmp().
** 比较两个内存单元里的数据,如果pMem1小于,等于,或者大于pMem2,返回否定.
** 排序的顺序首先是空的,然后数据(整型或者实型)按照大小排序,
** Two NULL values are considered equal by this function.
** 两个空数据在这个函数里面被认为是相等的.
*/
int sqlite3MemCompare(const Mem *pMem1, const Mem *pMem2, const CollSeq *pColl){
  int rc;
  int f1, f2;
  int combined_flags;

  f1 = pMem1->flags;
  f2 = pMem2->flags;
  combined_flags = f1|f2;
  assert( (combined_flags & MEM_RowSet)==0 );
 
  /* If one value is NULL, it is less than the other. If both values
  ** are NULL, return 0.
  ** 如果一个数据是空的,那么认为它比任何一个数据都要小.
  ** 如果两个数据都是空的,返回0.
  */
  if( combined_flags&MEM_Null ){
    return (f2&MEM_Null) - (f1&MEM_Null);
  }

  /* If one value is a number and the other is not, the number is less.
  ** If both are numbers, compare as reals if one is a real, or as integers
  ** if both values are integers.
  ** 如果其中一个数据是数字并且另一个不是,那么是数字的那个数据更小.
  ** 如果两个数据都是数字,如果其中一个是实型,那么按照实型来比较,
  ** 如果两个都是整型,那么按照整型来比较.
  */
  if( combined_flags&(MEM_Int|MEM_Real) ){
    if( !(f1&(MEM_Int|MEM_Real)) ){
      return 1;
    }
    if( !(f2&(MEM_Int|MEM_Real)) ){
      return -1;
    }
    if( (f1 & f2 & MEM_Int)==0 ){
      double r1, r2;
      if( (f1&MEM_Real)==0 ){
        r1 = (double)pMem1->u.i;
      }else{
        r1 = pMem1->r;
      }
      if( (f2&MEM_Real)==0 ){
        r2 = (double)pMem2->u.i;
      }else{
        r2 = pMem2->r;
      }
      if( r1<r2 ) return -1;
      if( r1>r2 ) return 1;
      return 0;
    }else{
      assert( f1&MEM_Int );
      assert( f2&MEM_Int );
      if( pMem1->u.i < pMem2->u.i ) return -1;
      if( pMem1->u.i > pMem2->u.i ) return 1;
      return 0;
    }
  }

  /* If one value is a string and the other is a blob, the string is less.
  ** If both are strings, compare using the collating functions.
  ** 如果其中一个数据是string类型,另一个是blob类型,string类型的更小.
  ** 如果两个都是string类型,用collatin函数来比较.
  */
  if( combined_flags&MEM_Str ){
    if( (f1 & MEM_Str)==0 ){
      return 1;
    }
    if( (f2 & MEM_Str)==0 ){
      return -1;
    }

    assert( pMem1->enc==pMem2->enc );
    assert( pMem1->enc==SQLITE_UTF8 || 
            pMem1->enc==SQLITE_UTF16LE || pMem1->enc==SQLITE_UTF16BE );

    /* The collation sequence must be defined at this point, even if
    ** the user deletes the collation sequence after the vdbe program is
    ** compiled (this was not always the case).
    ** the collation sequence在这点上必须被定义,即使在vdbe程序被编译之后用户删除了the collation sequence
    ** (这种情况并不常见)
    */
    assert( !pColl || pColl->xCmp );

    if( pColl ){
      if( pMem1->enc==pColl->enc ){
        /* The strings are already in the correct encoding.  Call the
        ** comparison function directly 
        ** 字符串类型已经被正确的编码了,就直接调用比较函数.
        */
        return pColl->xCmp(pColl->pUser,pMem1->n,pMem1->z,pMem2->n,pMem2->z);
      }else{
        const void *v1, *v2;
        int n1, n2;
        Mem c1;
        Mem c2;
        memset(&c1, 0, sizeof(c1));
        memset(&c2, 0, sizeof(c2));
        sqlite3VdbeMemShallowCopy(&c1, pMem1, MEM_Ephem);
        sqlite3VdbeMemShallowCopy(&c2, pMem2, MEM_Ephem);
        v1 = sqlite3ValueText((sqlite3_value*)&c1, pColl->enc);
        n1 = v1==0 ? 0 : c1.n;
        v2 = sqlite3ValueText((sqlite3_value*)&c2, pColl->enc);
        n2 = v2==0 ? 0 : c2.n;
        rc = pColl->xCmp(pColl->pUser, n1, v1, n2, v2);
        sqlite3VdbeMemRelease(&c1);
        sqlite3VdbeMemRelease(&c2);
        return rc;
      }
    }
    /* If a NULL pointer was passed as the collate function, fall through
    ** to the blob case and use memcmp().  */
  }
 
  /* Both values must be blobs.  Compare using memcmp().  
  ** 如果两个数据都是blob类型,那么调用memcmp()函数来比较大小.
  */
  rc = memcmp(pMem1->z, pMem2->z, (pMem1->n>pMem2->n)?pMem2->n:pMem1->n);
  if( rc==0 ){
    rc = pMem1->n - pMem2->n;
  }
  return rc;
}

/* 其他
** Move data out of a btree key or data field and into a Mem structure.
** The data or key is taken from the entry that pCur is currently pointing
** to.  offset and amt determine what portion of the data or key to retrieve.
** key is true to get the key or false to get data.  The result is written
** into the pMem element.
** 将数据从一个b树的键或者值空间中移入一个Mem结构体当中。这个值或者键是从pCur通常指向的入口获取的。
** The pMem structure is assumed to be uninitialized.  Any prior content
** is overwritten without being freed.
** pMem结构体被假定没有被初始化，任何之前的内容都将被重写而不是被释放。
** If this routine fails for any reason (malloc returns NULL or unable
** to read from the disk) then the pMem is left in an inconsistent state.
*/
int sqlite3VdbeMemFromBtree(
  BtCursor *pCur,   /* Cursor pointing at record to retrieve. 只想恢复记录的游标*/
  int offset,       /* Offset from the start of data to return bytes from.从开始数据到返回字节数的相位差 */
  int amt,          /* Number of bytes to return. 返回字节数的数目*/
  int key,          /* If true, retrieve from the btree key, not data. 如果为真，不是从数值而是从b树的键检索，*/
  Mem *pMem         /* OUT: Return data in this Mem structure. 这个mem结构的返回数据*/
){
  char *zData;        /* Data from the btree layer 来自b树层的数据*/
  int available = 0;  /* Number of bytes available on the local btree page 本地b树页面的可用字节数*/
  int rc = SQLITE_OK; /* Return code 返回标示符*/

  assert( sqlite3BtreeCursorIsValid(pCur) );

  /* Note: the calls to BtreeKeyFetch() and DataFetch() below assert() 
  ** that both the BtShared and database handle mutexes are held. 
  ** 在assert()方法下面调用b树取键和取数据的方法。共享b树和数据库控制互斥*/
  assert( (pMem->flags & MEM_RowSet)==0 );
  if( key ){
    zData = (char *)sqlite3BtreeKeyFetch(pCur, &available);
  }else{
    zData = (char *)sqlite3BtreeDataFetch(pCur, &available);
  }
  assert( zData!=0 );

  if( offset+amt<=available && (pMem->flags&MEM_Dyn)==0 ){
    sqlite3VdbeMemRelease(pMem);
    pMem->z = &zData[offset];
    pMem->flags = MEM_Blob|MEM_Ephem;
  }else if( SQLITE_OK==(rc = sqlite3VdbeMemGrow(pMem, amt+2, 0)) ){
    pMem->flags = MEM_Blob|MEM_Dyn|MEM_Term;
    pMem->enc = 0;
    pMem->type = SQLITE_BLOB;
    if( key ){
      rc = sqlite3BtreeKey(pCur, offset, amt, pMem->z);
    }else{
      rc = sqlite3BtreeData(pCur, offset, amt, pMem->z);
    }
    pMem->z[amt] = 0;
    pMem->z[amt+1] = 0;
    if( rc!=SQLITE_OK ){
      sqlite3VdbeMemRelease(pMem);
    }
  }
  pMem->n = amt;

  return rc;
}

/* This function is only available internally, it is not part of the
** external API. It works in a similar way to sqlite3_value_text(),
** except the data returned is in the encoding specified by the second
** parameter, which must be one of SQLITE_UTF16BE, SQLITE_UTF16LE or
** SQLITE_UTF8.
** 这个方法仅在内部可用，它不是外部API的部分。它的作用类似sqlite3_value_text（）方法，
** 除了返回数据通过第二个参数以特殊的方式编码，其他的必须是以下三种编码方式之一。
** (2006-02-16:)  The enc value can be or-ed with SQLITE_UTF16_ALIGNED.
** If that is the case, then the result must be aligned on an even byte
** boundary.编码值以此方式那么结果必须在字符边界对齐。
*/
const void *sqlite3ValueText(sqlite3_value* pVal, u8 enc){
  if( !pVal ) return 0;

  assert( pVal->db==0 || sqlite3_mutex_held(pVal->db->mutex) );
  assert( (enc&3)==(enc&~SQLITE_UTF16_ALIGNED) );
  assert( (pVal->flags & MEM_RowSet)==0 );

  if( pVal->flags&MEM_Null ){
    return 0;
  }
  assert( (MEM_Blob>>3) == MEM_Str );
  pVal->flags |= (pVal->flags & MEM_Blob)>>3;
  ExpandBlob(pVal);
  if( pVal->flags&MEM_Str ){
    sqlite3VdbeChangeEncoding(pVal, enc & ~SQLITE_UTF16_ALIGNED);
    if( (enc & SQLITE_UTF16_ALIGNED)!=0 && 1==(1&SQLITE_PTR_TO_INT(pVal->z)) ){
      assert( (pVal->flags & (MEM_Ephem|MEM_Static))!=0 );
      if( sqlite3VdbeMemMakeWriteable(pVal)!=SQLITE_OK ){
        return 0;
      }
    }
    sqlite3VdbeMemNulTerminate(pVal); /* IMP: R-31275-44060 */
  }else{
    assert( (pVal->flags&MEM_Blob)==0 );
    sqlite3VdbeMemStringify(pVal, enc);
    assert( 0==(1&SQLITE_PTR_TO_INT(pVal->z)) );
  }
  assert(pVal->enc==(enc & ~SQLITE_UTF16_ALIGNED) || pVal->db==0
              || pVal->db->mallocFailed );
  if( pVal->enc==(enc & ~SQLITE_UTF16_ALIGNED) ){
    return pVal->z;
  }else{
    return 0;
  }
}

/*
** Create a new sqlite3_value object.创建这个对象
*/
sqlite3_value *sqlite3ValueNew(sqlite3 *db){
  Mem *p = sqlite3DbMallocZero(db, sizeof(*p));
  if( p ){
    p->flags = MEM_Null;
    p->type = SQLITE_NULL;
    p->db = db;
  }
  return p;
}

/*
** Create a new sqlite3_value object, containing the value of pExpr.
**创建一个新的对象，这个对象包含pExpr的值
** This only works for very simple expressions that consist of one constant
** token (i.e. "5", "5.1", "'a string'"). If the expression can
** be converted directly into a value, then the value is allocated and
** a pointer written to *ppVal. 
** 这仅对几个简单的包含以下常量的表达式起作用，如果表示被直接改变为一个值，
** 这个只被分配内存，并写入指针ppval。
**The caller is responsible for deallocating
** the value by passing it to sqlite3ValueFree() later on. If the expression
** cannot be converted to a value, then *ppVal is set to NULL.
** 调用者负责为传递给sqlite3ValueFree()方法的值重新分配内存。如果这个表达式不能被修改，则将指针值为空。
*/
int sqlite3ValueFromExpr(
  sqlite3 *db,              /* The database connection 数据库连接*/
  Expr *pExpr,              /* The expression to evaluate求值表达式 */
  u8 enc,                   /* Encoding to use 用于编码*/
  u8 affinity,              /* Affinity to use 想死的使用*/
  sqlite3_value **ppVal     /* Write the new value here 写入新的值*/
){
  int op;
  char *zVal = 0;
  sqlite3_value *pVal = 0;
  int negInt = 1;
  const char *zNeg = "";

  if( !pExpr ){
    *ppVal = 0;
    return SQLITE_OK;
  }
  op = pExpr->op;

  /* op can only be TK_REGISTER if we have compiled with SQLITE_ENABLE_STAT3.
  ** The ifdef here is to enable us to achieve 100% branch test coverage even
  ** when SQLITE_ENABLE_STAT3 is omitted.
  ** 如果我们编译了SQLITE_ENABLE_STAT3，op只能被放入寄存器。
  ** 这个定义使我们完成所有的测试分值的覆盖即便当SQLITE_ENABLE_STAT3被省去。
  */
#ifdef SQLITE_ENABLE_STAT3
  if( op==TK_REGISTER ) op = pExpr->op2;
#else
  if( NEVER(op==TK_REGISTER) ) op = pExpr->op2;
#endif

  /* Handle negative integers in a single step.  This is needed in the
  ** case when the value is -9223372036854775808.
  ** 单步处理非负整数集。当值为-9223372036854775808时需要这样做。
  */
  if( op==TK_UMINUS
   && (pExpr->pLeft->op==TK_INTEGER || pExpr->pLeft->op==TK_FLOAT) ){
    pExpr = pExpr->pLeft;
    op = pExpr->op;
    negInt = -1;
    zNeg = "-";
  }

  if( op==TK_STRING || op==TK_FLOAT || op==TK_INTEGER ){
    pVal = sqlite3ValueNew(db);
    if( pVal==0 ) goto no_mem;
    if( ExprHasProperty(pExpr, EP_IntValue) ){
      sqlite3VdbeMemSetInt64(pVal, (i64)pExpr->u.iValue*negInt);
    }else{
      zVal = sqlite3MPrintf(db, "%s%s", zNeg, pExpr->u.zToken);
      if( zVal==0 ) goto no_mem;
      sqlite3ValueSetStr(pVal, -1, zVal, SQLITE_UTF8, SQLITE_DYNAMIC);
      if( op==TK_FLOAT ) pVal->type = SQLITE_FLOAT;
    }
    if( (op==TK_INTEGER || op==TK_FLOAT ) && affinity==SQLITE_AFF_NONE ){
      sqlite3ValueApplyAffinity(pVal, SQLITE_AFF_NUMERIC, SQLITE_UTF8);
    }else{
      sqlite3ValueApplyAffinity(pVal, affinity, SQLITE_UTF8);
    }
    if( pVal->flags & (MEM_Int|MEM_Real) ) pVal->flags &= ~MEM_Str;
    if( enc!=SQLITE_UTF8 ){
      sqlite3VdbeChangeEncoding(pVal, enc);
    }
  }else if( op==TK_UMINUS ) {
    /* This branch happens for multiple negative signs. 这个分支用于处理多重负号。 Ex: -(-5) */
    if( SQLITE_OK==sqlite3ValueFromExpr(db,pExpr->pLeft,enc,affinity,&pVal) ){
      sqlite3VdbeMemNumerify(pVal);
      if( pVal->u.i==SMALLEST_INT64 ){
        pVal->flags &= MEM_Int;
        pVal->flags |= MEM_Real;
        pVal->r = (double)LARGEST_INT64;
      }else{
        pVal->u.i = -pVal->u.i;
      }
      pVal->r = -pVal->r;
      sqlite3ValueApplyAffinity(pVal, affinity, enc);
    }
  }else if( op==TK_NULL ){
    pVal = sqlite3ValueNew(db);
    if( pVal==0 ) goto no_mem;
  }
#ifndef SQLITE_OMIT_BLOB_LITERAL
  else if( op==TK_BLOB ){
    int nVal;
    assert( pExpr->u.zToken[0]=='x' || pExpr->u.zToken[0]=='X' );
    assert( pExpr->u.zToken[1]=='\'' );
    pVal = sqlite3ValueNew(db);
    if( !pVal ) goto no_mem;
    zVal = &pExpr->u.zToken[2];
    nVal = sqlite3Strlen30(zVal)-1;
    assert( zVal[nVal]=='\'' );
    sqlite3VdbeMemSetStr(pVal, sqlite3HexToBlob(db, zVal, nVal), nVal/2,
                         0, SQLITE_DYNAMIC);
  }
#endif

  if( pVal ){
    sqlite3VdbeMemStoreType(pVal);
  }
  *ppVal = pVal;
  return SQLITE_OK;

no_mem:
  db->mallocFailed = 1;
  sqlite3DbFree(db, zVal);
  sqlite3ValueFree(pVal);
  *ppVal = 0;
  return SQLITE_NOMEM;
}

/*
** Change the string value of an sqlite3_value object改变这个对象的字符串值
*/
void sqlite3ValueSetStr(
  sqlite3_value *v,     /* Value to be set 值被设置*/
  int n,                /* Length of string z 字符串z的长度*/
  const void *z,        /* Text of the new string 新的字符串文本*/
  u8 enc,               /* Encoding to use 用于编码*/
  void (*xDel)(void*)   /* Destructor for the string 解除字符串*/
){
  if( v ) sqlite3VdbeMemSetStr((Mem *)v, z, n, enc, xDel);
}

/*
** Free an sqlite3_value object释放对象
*/
void sqlite3ValueFree(sqlite3_value *v){
  if( !v ) return;
  sqlite3VdbeMemRelease((Mem *)v);
  sqlite3DbFree(((Mem*)v)->db, v);
}

/*
** Return the number of bytes in the sqlite3_value object assuming
** that it uses the encoding "enc"返回对象假设的字节数，它用于编码。
*/
int sqlite3ValueBytes(sqlite3_value *pVal, u8 enc){
  Mem *p = (Mem*)pVal;
  if( (p->flags & MEM_Blob)!=0 || sqlite3ValueText(pVal, enc) ){
    if( p->flags & MEM_Zero ){
      return p->n + p->u.nZero;
    }else{
      return p->n;
    }
  }
  return 0;
}
