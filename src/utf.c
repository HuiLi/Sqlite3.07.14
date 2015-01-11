/*
** 2004 April 13
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
* * 2004年4月13日
* *作者放弃这个源代码的版权。
* *不是在发布一个合法的通知,而是在传递一种祝福。
* *
* *希望你能做些好事,不要做让别人讨厌事。
* *希望你可以原谅自己还有他人。
* *希望你能自由地分享这些东西,而不受限制。


*************************************************************************
** This file contains routines used to translate between UTF-8, 
** UTF-16, UTF-16BE, and UTF-16LE.
* *这个文件通过包含的例程进行utf - 8、 utf - 16、UTF-16BE、 UTF-16LE之间的翻译。

**
** Notes on UTF-8:
**
**   Byte-0    Byte-1    Byte-2    Byte-3    Value
**  0xxxxxxx                                 00000000 00000000 0xxxxxxx
**  110yyyyy  10xxxxxx                       00000000 00000yyy yyxxxxxx
**  1110zzzz  10yyyyyy  10xxxxxx             00000000 zzzzyyyy yyxxxxxx
**  11110uuu  10uuzzzz  10yyyyyy  10xxxxxx   000uuuuu zzzzyyyy yyxxxxxx
**
**
** Notes on UTF-16:  (with wwww+1==uuuuu)
**
**      Word-0               Word-1          Value
**  110110ww wwzzzzyy   110111yy yyxxxxxx    000uuuuu zzzzyyyy yyxxxxxx
**  zzzzyyyy yyxxxxxx                        00000000 zzzzyyyy yyxxxxxx
**
**
** BOM or Byte Order Mark:
**     0xff 0xfe   little-endian utf-16 follows
**     0xfe 0xff   big-endian utf-16 follows
**
*/
#include "sqliteInt.h"
#include <assert.h>
#include "vdbeInt.h"

#ifndef SQLITE_AMALGAMATION
/*
** The following constant value is used by the SQLITE_BIGENDIAN and
** SQLITE_LITTLEENDIAN macros.
* *以下常量值被SQLITE_BIGENDIAN和
* * SQLITE_LITTLEENDIAN使用。

*/
const int sqlite3one = 1;
#endif /* SQLITE_AMALGAMATION */

/*
** This lookup table is used to help decode the first byte of
** a multi-byte UTF8 character.
* *此查找表用于帮助解码多字节UTF8 编码值的第一个字节。

*/
static const unsigned char sqlite3Utf8Trans1[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,
};


#define WRITE_UTF8(zOut, c) {                          \
  if( c<0x00080 ){                                     \
    *zOut++ = (u8)(c&0xFF);                            \
  }                                                    \
  else if( c<0x00800 ){                                \
    *zOut++ = 0xC0 + (u8)((c>>6)&0x1F);                \
    *zOut++ = 0x80 + (u8)(c & 0x3F);                   \
  }                                                    \
  else if( c<0x10000 ){                                \
    *zOut++ = 0xE0 + (u8)((c>>12)&0x0F);               \
    *zOut++ = 0x80 + (u8)((c>>6) & 0x3F);              \
    *zOut++ = 0x80 + (u8)(c & 0x3F);                   \
  }else{                                               \
    *zOut++ = 0xF0 + (u8)((c>>18) & 0x07);             \
    *zOut++ = 0x80 + (u8)((c>>12) & 0x3F);             \
    *zOut++ = 0x80 + (u8)((c>>6) & 0x3F);              \
    *zOut++ = 0x80 + (u8)(c & 0x3F);                   \
  }                                                    \
}

#define WRITE_UTF16LE(zOut, c) {                                    \
  if( c<=0xFFFF ){                                                  \
    *zOut++ = (u8)(c&0x00FF);                                       \
    *zOut++ = (u8)((c>>8)&0x00FF);                                  \
  }else{                                                            \
    *zOut++ = (u8)(((c>>10)&0x003F) + (((c-0x10000)>>10)&0x00C0));  \
    *zOut++ = (u8)(0x00D8 + (((c-0x10000)>>18)&0x03));              \
    *zOut++ = (u8)(c&0x00FF);                                       \
    *zOut++ = (u8)(0x00DC + ((c>>8)&0x03));                         \
  }                                                                 \
}

#define WRITE_UTF16BE(zOut, c) {                                    \
  if( c<=0xFFFF ){                                                  \
    *zOut++ = (u8)((c>>8)&0x00FF);                                  \
    *zOut++ = (u8)(c&0x00FF);                                       \
  }else{                                                            \
    *zOut++ = (u8)(0x00D8 + (((c-0x10000)>>18)&0x03));              \
    *zOut++ = (u8)(((c>>10)&0x003F) + (((c-0x10000)>>10)&0x00C0));  \
    *zOut++ = (u8)(0x00DC + ((c>>8)&0x03));                         \
    *zOut++ = (u8)(c&0x00FF);                                       \
  }                                                                 \
}

#define READ_UTF16LE(zIn, TERM, c){                                   \
  c = (*zIn++);                                                       \
  c += ((*zIn++)<<8);                                                 \
  if( c>=0xD800 && c<0xE000 && TERM ){                                \
    int c2 = (*zIn++);                                                \
    c2 += ((*zIn++)<<8);                                              \
    c = (c2&0x03FF) + ((c&0x003F)<<10) + (((c&0x03C0)+0x0040)<<10);   \
  }                                                                   \
}

#define READ_UTF16BE(zIn, TERM, c){                                   \
  c = ((*zIn++)<<8);                                                  \
  c += (*zIn++);                                                      \
  if( c>=0xD800 && c<0xE000 && TERM ){                                \
    int c2 = ((*zIn++)<<8);                                           \
    c2 += (*zIn++);                                                   \
    c = (c2&0x03FF) + ((c&0x003F)<<10) + (((c&0x03C0)+0x0040)<<10);   \
  }                                                                   \
}

/*
** Translate a single UTF-8 character.  Return the unicode value.
**
** During translation, assume that the byte that zTerm points
** is a 0x00.
**
** Write a pointer to the next unread byte back into *pzNext.
**
** Notes On Invalid UTF-8:
**
**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to
**     be encoded as a multi-byte character.  Any multi-byte character that
**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.
**
**  *  This routine never allows a UTF16 surrogate value to be encoded.
**     If a multi-byte character attempts to encode a value between
**     0xd800 and 0xe000 then it is rendered as 0xfffd.
**
**  *  Bytes in the range of 0x80 through 0xbf which occur as the first
**     byte of a character are interpreted as single-byte characters
**     and rendered as themselves even though they are technically
**     invalid characters.
**
**  *  This routine accepts an infinite number of different UTF8 encodings
**     for unicode values 0x80 and greater.  It do not change over-length
**     encodings to 0xfffd as some systems recommend.
* *翻译一个utf - 8字符。返回的unicode值。
* *
* *在翻译过程中,假设zTerm点的字节
* *是一个0 x00。
* *
* *指针指到下一个未读的字节，并返回* pzNext。

* *
* *记录无效的utf - 8:
* *
* * *这个例程从不允许7位字符(0 x00到0 x7f)
* *作为多字节字符编码。用任何的多字节字符
* *编码0 x00和0 x7f之间的值，其值都为0 xfffd。
* *
* * *这个例程不能用UTF16编码。
* *如果用一个多字节字符编码
* * 0 xd800和0 xe000之间的一个值， xfffd都为0。
* *
* * *第一个字符中的字节，且在00x8到 0xbf的范围内，都被编码为单字节字符，
* *,即使他们在技术上出现无效字符。
* *
* * 对于用unicode编码的0x80，这个例程也可使用UTF8编码。当系统提示时，它不能使后边的字符编码为0 xfffd。


*/
#define READ_UTF8(zIn, zTerm, c)                           \
  c = *(zIn++);                                            \
  if( c>=0xc0 ){                                           \
    c = sqlite3Utf8Trans1[c-0xc0];                         \
    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \
      c = (c<<6) + (0x3f & *(zIn++));                      \
    }                                                      \
    if( c<0x80                                             \
        || (c&0xFFFFF800)==0xD800                          \
        || (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \
  }
u32 sqlite3Utf8Read(
  const unsigned char *zIn,       /*UTF-8特征值的第一个字节 */
  const unsigned char **pzNext    /*记录utf - 8字符的第一个字节 */
){
  unsigned int c;

  /* Same as READ_UTF8() above but without the zTerm parameter.
  ** For this routine, we assume the UTF8 string is always zero-terminated.
  ** 以上和READ_UTF8（）一样但是没有zTerm参数。
 **对这个例程,我们假设UTF8字符串的值均为zero-terminated。
  */
  c = *(zIn++);
  if( c>=0xc0 ){
    c = sqlite3Utf8Trans1[c-0xc0];
    while( (*zIn & 0xc0)==0x80 ){
      c = (c<<6) + (0x3f & *(zIn++));
    }
    if( c<0x80
        || (c&0xFFFFF800)==0xD800
        || (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }
  }
  *pzNext = zIn;
  return c;
}




/*
** If the TRANSLATE_TRACE macro is defined, the value of each Mem is
** printed on stderr on the way into and out of sqlite3VdbeMemTranslate().
* *如果TRANSLATE_TRACE被定义,那么每个Mem的值
* *都被输出到stderr sqlite3VdbeMemTranslate()函数上面。

*/ 
/* #define TRANSLATE_TRACE 1 */

#ifndef SQLITE_OMIT_UTF16
/*
** This routine transforms the internal text encoding used by pMem to
** desiredEnc. It is an error if the string is already of the desired
** encoding, or if *pMem does not contain a string value.
* *这个例程可以通过pMem转换内部文本编码到desiredEnc上。如果已经得到编码,
* * 或者如果* pMem不包含一个字符串值，那就说明这是一个错误的字符串。


*/
int sqlite3VdbeMemTranslate(Mem *pMem, u8 desiredEnc){
  int len;                    /* 输出字符串的最大长度字节 */
  unsigned char *zOut;                  /* 输出缓冲区 */
  unsigned char *zIn;                   /* 输入迭代器 */
  unsigned char *zTerm;                 /* 输入结束 */
  unsigned char *z;                     /* 输出迭代器 */
  unsigned int c;

  assert( pMem->db==0 || sqlite3_mutex_held(pMem->db->mutex) );
  assert( pMem->flags&MEM_Str );
  assert( pMem->enc!=desiredEnc );
  assert( pMem->enc!=0 );
  assert( pMem->n>=0 );

#if defined(TRANSLATE_TRACE) && defined(SQLITE_DEBUG)
  {
    char zBuf[100];
    sqlite3VdbeMemPrettyPrint(pMem, zBuf);
    fprintf(stderr, "INPUT:  %s\n", zBuf);
  }
#endif

  /* If the translation is between UTF-16 little and big endian, then 
  ** all that is required is to swap the byte order. This case is handled
  ** differently from the others.
  **如果翻译的结果是用utf - 16编码的最小和最大尾数之间的数,,
  * *那么就需要交换字节顺序。这种情况下处理不同于其它的情况。
  */

  if( pMem->enc!=SQLITE_UTF8 && desiredEnc!=SQLITE_UTF8 ){
    u8 temp;
    int rc;
    rc = sqlite3VdbeMemMakeWriteable(pMem);
    if( rc!=SQLITE_OK ){
      assert( rc==SQLITE_NOMEM );
      return SQLITE_NOMEM;
    }
    zIn = (u8*)pMem->z;
    zTerm = &zIn[pMem->n&~1];
    while( zIn<zTerm ){
      temp = *zIn;
      *zIn = *(zIn+1);
      zIn++;
      *zIn++ = temp;
    }
    pMem->enc = desiredEnc;
    goto translate_out;
  }

  /*  在输出缓冲区len设置为所需的最大字节数。*/
 
 
  if( desiredEnc==SQLITE_UTF8 ){
    /* When converting from UTF-16, the maximum growth results from
    ** translating a 2-byte character to a 4-byte UTF-8 character.
    ** A single byte is required for the output string
    ** nul-terminator.
    * *用utf - 16编码时转化的时候,最大的增长的结果
    * *从2字节字符到4字节字符中产生。
    * *输出的字符串nul-terminator需要单个字节。
    */
    pMem->n &= ~1;
    len = pMem->n * 2 + 1;
  }else{
    /* When converting from UTF-8 to UTF-16 the maximum growth is caused
    ** when a 1-byte UTF-8 character is translated into a 2-byte UTF-16
    ** character. Two bytes are required in the output buffer for the
    ** nul-terminator.
    **当转换utf - 8编码到utf - 16编码时，会引起最大的长度变化。
    * *当一个utf - 8字符转换为2字节utf - 16
    * *字符时， nul-terminator需要两个字节的输出缓冲区。
    */
    len = pMem->n * 2 + 2;
  }

  /* Set zIn to point at the start of the input buffer and zTerm to point 1
  ** byte past the end.
  **
  ** Variable zOut is set to point at the output buffer, space obtained
  ** from sqlite3_malloc().
  **在输入缓冲区的开始设置zIn为一点和在结束时，zTerm为 1个
  * *字节的字符。
  * *
  * *在输出缓冲区变量zOut设置为一点,而且从sqlite3_malloc()中可得到内存空间。
	*/

  zIn = (u8*)pMem->z;
  zTerm = &zIn[pMem->n];
  zOut = sqlite3DbMallocRaw(pMem->db, len);
  if( !zOut ){
    return SQLITE_NOMEM;
  }
  z = zOut;

  if( pMem->enc==SQLITE_UTF8 ){
    if( desiredEnc==SQLITE_UTF16LE ){
      /* UTF-8 -> UTF-16 Little-endian */
      while( zIn<zTerm ){
        /* c = sqlite3Utf8Read(zIn, zTerm, (const u8**)&zIn); */
        READ_UTF8(zIn, zTerm, c);
        WRITE_UTF16LE(z, c);
      }
    }else{
      assert( desiredEnc==SQLITE_UTF16BE );
      /* UTF-8 -> UTF-16 Big-endian */
      while( zIn<zTerm ){
        /* c = sqlite3Utf8Read(zIn, zTerm, (const u8**)&zIn); */
        READ_UTF8(zIn, zTerm, c);
        WRITE_UTF16BE(z, c);
      }
    }
    pMem->n = (int)(z - zOut);
    *z++ = 0;
  }else{
    assert( desiredEnc==SQLITE_UTF8 );
    if( pMem->enc==SQLITE_UTF16LE ){
      /* UTF-16 Little-endian -> UTF-8 */
      while( zIn<zTerm ){
        READ_UTF16LE(zIn, zIn<zTerm, c); 
        WRITE_UTF8(z, c);
      }
    }else{
      /* UTF-16 Big-endian -> UTF-8 */
      while( zIn<zTerm ){
        READ_UTF16BE(zIn, zIn<zTerm, c); 
        WRITE_UTF8(z, c);
      }
    }
    pMem->n = (int)(z - zOut);
  }
  *z = 0;
  assert( (pMem->n+(desiredEnc==SQLITE_UTF8?1:2))<=len );

  sqlite3VdbeMemRelease(pMem);
  pMem->flags &= ~(MEM_Static|MEM_Dyn|MEM_Ephem);
  pMem->enc = desiredEnc;
  pMem->flags |= (MEM_Term|MEM_Dyn);
  pMem->z = (char*)zOut;
  pMem->zMalloc = pMem->z;

translate_out:
#if defined(TRANSLATE_TRACE) && defined(SQLITE_DEBUG)
  {
    char zBuf[100];
    sqlite3VdbeMemPrettyPrint(pMem, zBuf);
    fprintf(stderr, "OUTPUT: %s\n", zBuf);
  }
#endif
  return SQLITE_OK;
}

/*
** This routine checks for a byte-order mark at the beginning of the 
** UTF-16 string stored in *pMem. If one is present, it is removed and
** the encoding of the Mem adjusted. This routine does not do any
** byte-swapping, it just sets Mem.enc appropriately.
**
** The allocation (static, dynamic etc.) and encoding of the Mem may be
** changed by this function.
* *当字符存储在* pMem 中且在utf - 16字符串的开始时，这个例程允许检查字节顺序。
* *如果有一个已经存在,那么另外的可以被删除，并调整Mem的编码。这个例程不做    
* *任何字节交换,它只是被设置Mem.enc。
* *分配(静态、动态空间)的Mem的编码可能会被这个函数改变。
*/

int sqlite3VdbeMemHandleBom(Mem *pMem){
  int rc = SQLITE_OK;
  u8 bom = 0;

  assert( pMem->n>=0 );
  if( pMem->n>1 ){
    u8 b1 = *(u8 *)pMem->z;
    u8 b2 = *(((u8 *)pMem->z) + 1);
    if( b1==0xFE && b2==0xFF ){
      bom = SQLITE_UTF16BE;
    }
    if( b1==0xFF && b2==0xFE ){
      bom = SQLITE_UTF16LE;
    }
  }
  
  if( bom ){
    rc = sqlite3VdbeMemMakeWriteable(pMem);
    if( rc==SQLITE_OK ){
      pMem->n -= 2;
      memmove(pMem->z, &pMem->z[2], pMem->n);
      pMem->z[pMem->n] = '\0';
      pMem->z[pMem->n+1] = '\0';
      pMem->flags |= MEM_Term;
      pMem->enc = bom;
    }
  }
  return rc;
}
#endif /* SQLITE_OMIT_UTF16 */

/*
** pZ is a UTF-8 encoded unicode string. If nByte is less than zero,
** return the number of unicode characters in pZ up to (but not including)
** the first 0x00 byte. If nByte is not less than zero, return the
** number of unicode characters in the first nByte of pZ (or up to 
** the first 0x00, whichever comes first).
 * pZ是unicode utf - 8编码的字符串。如果nByte小于零,
* *那么在pZ上，返回unicode字符(但不包括)
* *为第一个0x00字节。如果nByte不小于零，那么返回的
* *编码字符在 pZ的第一个nByte(上或第一个0 x00,无论哪一个先开始)。


*/
int sqlite3Utf8CharLen(const char *zIn, int nByte){
  int r = 0;
  const u8 *z = (const u8*)zIn;
  const u8 *zTerm;
  if( nByte>=0 ){
    zTerm = &z[nByte];
  }else{
    zTerm = (const u8*)(-1);
  }
  assert( z<=zTerm );
  while( *z!=0 && z<zTerm ){
    SQLITE_SKIP_UTF8(z);
    r++;
  }
  return r;
}

/* This test function is not currently used by the automated test-suite. 
** Hence it is only available in debug builds.
**这个测试函数并不是目前使用的自动化测试套件。
* *因此只有在调试版可用。

*/
#if defined(SQLITE_TEST) && defined(SQLITE_DEBUG)
/*
** Translate UTF-8 to UTF-8.
**
** This has the effect of making sure that the string is well-formed
** UTF-8.  Miscoded characters are removed.
**
** The translation is done in-place and aborted if the output
** overruns the input.
* * utf - 8转换为utf - 8。
* *这样做的效果是确保字符串的格式是正确的
* * utf - 8。错编字符被删除。
* *如果输出超过输入，那么翻译就地完成和抛弃。


*/
int sqlite3Utf8To8(unsigned char *zIn){
  unsigned char *zOut = zIn;
  unsigned char *zStart = zIn;
  u32 c;

  while( zIn[0] && zOut<=zIn ){
    c = sqlite3Utf8Read(zIn, (const u8**)&zIn);
    if( c!=0xfffd ){
      WRITE_UTF8(zOut, c);
    }
  }
  *zOut = 0;
  return (int)(zOut - zStart);
}
#endif

#ifndef SQLITE_OMIT_UTF16
/*
** Convert a UTF-16 string in the native encoding into a UTF-8 string.
** Memory to hold the UTF-8 string is obtained from sqlite3_malloc and must
** be freed by the calling function.
**
** NULL is returned if there is an allocation error.
 *把原始编码utf - 16字符串转换成utf - 8编码的字符串。
* *utf - 8字符串的内存从sqlite3_malloc被得到 和必须由调用函数释放。
* *
* *如果有配置错误则返回NULL。


*/
char *sqlite3Utf16to8(sqlite3 *db, const void *z, int nByte, u8 enc){
  Mem m;
  memset(&m, 0, sizeof(m));
  m.db = db;
  sqlite3VdbeMemSetStr(&m, z, nByte, enc, SQLITE_STATIC);
  sqlite3VdbeChangeEncoding(&m, SQLITE_UTF8);
  if( db->mallocFailed ){
    sqlite3VdbeMemRelease(&m);
    m.z = 0;
  }
  assert( (m.flags & MEM_Term)!=0 || db->mallocFailed );
  assert( (m.flags & MEM_Str)!=0 || db->mallocFailed );
  assert( (m.flags & MEM_Dyn)!=0 || db->mallocFailed );
  assert( m.z || db->mallocFailed );
  return m.z;
}

/*
** Convert a UTF-8 string to the UTF-16 encoding specified by parameter
** enc. A pointer to the new string is returned, and the value of *pnOut
** is set to the length of the returned string in bytes. The call should
** arrange to call sqlite3DbFree() on the returned pointer when it is
** no longer required.
** 
** If a malloc failure occurs, NULL is returned and the db.mallocFailed
** flag set.

**通过参数 enc，一个utf - 8编码的字符串可转换为utf -16编码。新的字符串返回一个指针,并* pnOut的值通过字节设定为返回的字符串的长度。
**不再需要时，则调用sqlite3DbFree()返回的指针。
**如果malloc出现错误,则返回NULL或db.mallocFailed设标记集。



*/
#ifdef SQLITE_ENABLE_STAT3
char *sqlite3Utf8to16(sqlite3 *db, u8 enc, char *z, int n, int *pnOut){
  Mem m;
  memset(&m, 0, sizeof(m));
  m.db = db;
  sqlite3VdbeMemSetStr(&m, z, n, SQLITE_UTF8, SQLITE_STATIC);
  if( sqlite3VdbeMemTranslate(&m, enc) ){
    assert( db->mallocFailed );
    return 0;
  }
  assert( m.z==m.zMalloc );
  *pnOut = m.n;
  return m.z;
}
#endif

/*
** zIn is a UTF-16 encoded unicode string at least nChar characters long.
** Return the number of bytes in the first nChar unicode characters
** in pZ.  nChar must be non-negative.
**zIn是一个utf -16编码的unicode字符串，长度至少nChar。
**pZ返回nChar unicode字符的第一个字节。nChar必须是非负数。


*/
int sqlite3Utf16ByteLen(const void *zIn, int nChar){
  int c;
  unsigned char const *z = zIn;
  int n = 0;
  
  if( SQLITE_UTF16NATIVE==SQLITE_UTF16BE ){
    while( n<nChar ){
      READ_UTF16BE(z, 1, c);
      n++;
    }
  }else{
    while( n<nChar ){
      READ_UTF16LE(z, 1, c);
      n++;
    }
  }
  return (int)(z-(unsigned char const *)zIn);
}

#if defined(SQLITE_TEST)
/*
** This routine is called from the TCL test function "translate_selftest".
** It checks that the primitives for serializing and deserializing
** characters in each encoding are inverses of each other.
**TCL测试函数“translate_selftest”可调用这个例程。每个
**编码是对方的逆反编码，原语用在序列化和反序列化编码中。

*/
void sqlite3UtfSelfTest(void){
  unsigned int i, t;
  unsigned char zBuf[20];
  unsigned char *z;
  int n;
  unsigned int c;

  for(i=0; i<0x00110000; i++){
    z = zBuf;
    WRITE_UTF8(z, i);
    n = (int)(z-zBuf);
    assert( n>0 && n<=4 );
    z[0] = 0;
    z = zBuf;
    c = sqlite3Utf8Read(z, (const u8**)&z);
    t = i;
    if( i>=0xD800 && i<=0xDFFF ) t = 0xFFFD;
    if( (i&0xFFFFFFFE)==0xFFFE ) t = 0xFFFD;
    assert( c==t );
    assert( (z-zBuf)==n );
  }
  for(i=0; i<0x00110000; i++){
    if( i>=0xD800 && i<0xE000 ) continue;
    z = zBuf;
    WRITE_UTF16LE(z, i);
    n = (int)(z-zBuf);
    assert( n>0 && n<=4 );
    z[0] = 0;
    z = zBuf;
    READ_UTF16LE(z, 1, c);
    assert( c==i );
    assert( (z-zBuf)==n );
  }
  for(i=0; i<0x00110000; i++){
    if( i>=0xD800 && i<0xE000 ) continue;
    z = zBuf;
    WRITE_UTF16BE(z, i);
    n = (int)(z-zBuf);
    assert( n>0 && n<=4 );
    z[0] = 0;
    z = zBuf;
    READ_UTF16BE(z, 1, c);
    assert( c==i );
    assert( (z-zBuf)==n );
  }
}
#endif /* SQLITE_TEST */
#endif /* SQLITE_OMIT_UTF16 */
