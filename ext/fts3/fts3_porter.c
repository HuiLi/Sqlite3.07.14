/*
** 2006 September 30
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** Implementation of the full-text-search tokenizer that implements
** a Porter stemmer.
**实现的是全文检索分词器，运用的是一个porter分词算法
*/

/*
** The code in this file is only compiled if:
**
**     * The FTS3 module is being built as an extension
**       (in which case SQLITE_CORE is not defined), or
**
**     * The FTS3 module is being built into the core of
**       SQLite (in which case SQLITE_ENABLE_FTS3 is defined).
*/
#include "fts3Int.h"
#if !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_FTS3)

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fts3_tokenizer.h"

/*
** Class derived from sqlite3_tokenizer
**类来源于sqlite3_tokenizer
*/
typedef struct porter_tokenizer {
  sqlite3_tokenizer base;      /* Base class */
} porter_tokenizer;

/*
** Class derived from sqlite3_tokenizer_cursor 
*/
typedef struct porter_tokenizer_cursor {
  sqlite3_tokenizer_cursor base;
  const char *zInput;          /* input we are tokenizing */
  int nInput;                  /* size of the input */
  int iOffset;                 /* current position in zInput */
  int iToken;                  /* index of next token to be returned */
  char *zToken;                /* storage for current token */
  int nAllocated;              /* space allocated to zToken buffer */
} porter_tokenizer_cursor;


/*
** Create a new tokenizer instance.
*/
static int porterCreate(
  int argc, const char * const *argv,
  sqlite3_tokenizer **ppTokenizer
){
  porter_tokenizer *t;

  UNUSED_PARAMETER(argc);
  UNUSED_PARAMETER(argv);

  t = (porter_tokenizer *) sqlite3_malloc(sizeof(*t));
  if( t==NULL ) return SQLITE_NOMEM;
  memset(t, 0, sizeof(*t));
  *ppTokenizer = &t->base;
  return SQLITE_OK;
}

/*
** Destroy a tokenizer
**引入头函数fts3_tokenizer.h首先定义两个结构体变量，在变量中定义了
要输入的词，输入词的大小和当前指针zinput的位置，下一个返回值的标记，
定义存储当前编译器然后创建一个新的编译器。
*/
static int porterDestroy(sqlite3_tokenizer *pTokenizer){
  sqlite3_free(pTokenizer);
  return SQLITE_OK;
}

/*
** Prepare to begin tokenizing a particular string.  The input
** string to be tokenized is zInput[0..nInput-1].  A cursor
** used to incrementally tokenize this string is returned in 
** *ppCursor.
**准备初始化一个特定的字符串。输入的字符串被编译为另一个的输入[0..nBytes-1]，
**一个游标被用来递增的编译这个字符串返回到*ppCursor中。

*/
static int porterOpen(
  sqlite3_tokenizer *pTokenizer,         /* The tokenizer */
  const char *zInput, int nInput,        /* String to be tokenized */
  sqlite3_tokenizer_cursor **ppCursor    /* OUT: Tokenization cursor */
){
  porter_tokenizer_cursor *c;

  UNUSED_PARAMETER(pTokenizer);

  c = (porter_tokenizer_cursor *) sqlite3_malloc(sizeof(*c));
  if( c==NULL ) return SQLITE_NOMEM;

  c->zInput = zInput;
  if( zInput==0 ){
    c->nInput = 0;
  }else if( nInput<0 ){
    c->nInput = (int)strlen(zInput);
  }else{
    c->nInput = nInput;
  }
  c->iOffset = 0;                 /* start tokenizing at the beginning */
  c->iToken = 0;
  c->zToken = NULL;               /* no space allocated, yet. */
  c->nAllocated = 0;

  *ppCursor = &c->base;
  return SQLITE_OK;
}

/*
** Close a tokenization cursor previously opened by a call to
** porterOpen() above.
通过上面porterOpen()函数的调用，关闭刚才打开的标记化游标
*/
static int porterClose(sqlite3_tokenizer_cursor *pCursor){
  porter_tokenizer_cursor *c = (porter_tokenizer_cursor *) pCursor;
  sqlite3_free(c->zToken);
  sqlite3_free(c);
  return SQLITE_OK;
}
/*
** Vowel or consonant
*/
static const char cType[] = {
   0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0,
   1, 1, 1, 2, 1
};

/*
** isConsonant() and isVowel() determine if their first character in
** the string they point to is a consonant or a vowel, according
** to Porter ruls.  
**
** A consonant is any letter other than 'a', 'e', 'i', 'o', or 'u'.
** 'Y' is a consonant unless it follows another consonant,
** in which case it is a vowel.
**
** In these routine, the letters are in reverse order.  So the 'y' rule
** is that 'y' is a consonant unless it is followed by another
** consonent.
isConsonant() and isVowel()用法，根据波特规则，如果字符串中的第一个字母（它们指向的是一个
辅音或者元音）；
除了'a', 'e', 'i', 'o', or 'u'.之外的任何一个辅音都属于字母，‘Y’是一个辅音，除非它在
另一个辅音的后面，否则他就是一个元音。在以上算法中，字母是以倒序排列，因此‘y’的规则就是
‘y’除非它在另一个辅音后面，才是辅音，否则就是一个元音

*/
static int isVowel(const char*);
static int isConsonant(const char *z){
  int j;
  char x = *z;
  if( x==0 ) return 0;
  assert( x>='a' && x<='z' );
  j = cType[x-'a'];
  if( j<2 ) return j;
  return z[1]==0 || isVowel(z + 1);
}
static int isVowel(const char *z){
  int j;
  char x = *z;
  if( x==0 ) return 0;
  assert( x>='a' && x<='z' );
  j = cType[x-'a'];
  if( j<2 ) return 1-j;
  return isConsonant(z + 1);
}

/*
** Let any sequence of one or more vowels be represented by V and let
** C be sequence of one or more consonants.  Then every word can be
** represented as
**
**           [C] (VC){m} [V]
**
** In prose:  A word is an optional consonant followed by zero or
** vowel-consonant pairs followed by an optional vowel.  "m" is the
** number of vowel consonant pairs.  This routine computes the value
** of m for the first i bytes of a word.
**
** Return true if the m-value for z is 1 or more.  In other words,
** return true if z contains at least one vowel that is followed
** by a consonant.
**
** In this routine z[] is in reverse order.  So we are really looking
** for an instance of of a consonant followed by a vowel.

一个或者更多的元音按顺序排好通过V表现，让一个或多个元音按照C的顺序排好。
然后每个词可能代表[C] (VC){m} [V]
在散文中：一个词是一个可选的在0后面的辅音或者是在一个可选元音后面的一堆
元辅音。这个程序就是计算m的值为了第一个i位或者一个字。
如果z的m值是1或者其他值就返回真。换句话说，如z包含至少一个跟在辅音后面的元音
在这个程序中，z[]数组元素师倒序的。因此，我们能找到一个跟在元音后面的辅音。
*/
static int m_gt_0(const char *z){
  while( isVowel(z) ){ z++; }
  if( *z==0 ) return 0;
  while( isConsonant(z) ){ z++; }
  return *z!=0;
}

/* Like mgt0 above except we are looking for a value of m which is
** exactly 1
*/
static int m_eq_1(const char *z){
  while( isVowel(z) ){ z++; }
  if( *z==0 ) return 0;
  while( isConsonant(z) ){ z++; }
  if( *z==0 ) return 0;
  while( isVowel(z) ){ z++; }
  if( *z==0 ) return 1;
  while( isConsonant(z) ){ z++; }
  return *z==0;
}

/* Like mgt0 above except we are looking for a value of m>1 instead
** or m>0
*/
static int m_gt_1(const char *z){
  while( isVowel(z) ){ z++; }
  if( *z==0 ) return 0;
  while( isConsonant(z) ){ z++; }
  if( *z==0 ) return 0;
  while( isVowel(z) ){ z++; }
  if( *z==0 ) return 0;
  while( isConsonant(z) ){ z++; }
  return *z!=0;
}

/*
** Return TRUE if there is a vowel anywhere within z[0..n-1]
*/
static int hasVowel(const char *z){
  while( isConsonant(z) ){ z++; }
  return *z!=0;
}

/*
** Return TRUE if the word ends in a double consonant.
**
** The text is reversed here. So we are really looking at
** the first two characters of z[].
*/
static int doubleConsonant(const char *z){
  return isConsonant(z) && z[0]==z[1];
}

/*
** Return TRUE if the word ends with three letters which
** are consonant-vowel-consonent and where the final consonant
** is not 'w', 'x', or 'y'.
**若这个词末尾以三个（辅音-元音-辅音且最后一个辅音不是‘w’‘x’‘y’）
**的字母结尾就返回为真
** The word is reversed here.  So we are really checking the
** first three letters and the first one cannot be in [wxy].
**这个词被取消了，因此我们真的检测出第一个三字母的但第一个
不是[xyz]的词
*/
static int star_oh(const char *z){
  return
    isConsonant(z) &&
    z[0]!='w' && z[0]!='x' && z[0]!='y' &&
    isVowel(z+1) &&
    isConsonant(z+2);
}

/*
** If the word ends with zFrom and xCond() is true for the stem
** of the word that preceeds the zFrom ending, then change the 
** ending to zTo.
**
** The input word *pz and zFrom are both in reverse order.  zTo
** is in normal order. 
**
** Return TRUE if zFrom matches.  Return FALSE if zFrom does not
** match.  Not that TRUE is returned even if xCond() fails and
** no substitution occurs.

**如果这个词以zFrom（） 和 xCond()函数结尾为真的话进展成为zFrom函数的话，然后
改变zTo的结束。*pz和zFrom函数的输入都是相反顺序。zTo以正常顺序。如果zForm匹配
返回真，不匹配返回假。并不是真就是返回的，即使xCond（）失败，没有替换发生。
*/

static int stem(
  char **pz,             /* The word being stemmed (Reversed) */
  const char *zFrom,     /* If the ending matches this... (Reversed) */
  const char *zTo,       /* ... change the ending to this (not reversed) */
  int (*xCond)(const char*)   /* Condition that must be true */
){
  char *z = *pz;
  while( *zFrom && *zFrom==*z ){ z++; zFrom++; }
  if( *zFrom!=0 ) return 0;
  if( xCond && !xCond(z) ) return 1;
  while( *zTo ){
    *(--z) = *(zTo++);
  }
  *pz = z;
  return 1;
}

/*
** This is the fallback stemmer used when the porter stemmer is
** inappropriate.  The input word is copied into the output with
** US-ASCII case folding.  If the input word is too long (more
** than 20 bytes if it contains no digits or more than 6 bytes if
** it contains digits) then word is truncated to 20 or 6 bytes
** by taking 10 or 3 bytes from the beginning and end.

**当波特抽梗机不适应的时候这是一个回退抽梗机。词的输入被拷入被拷贝到
**US-ASCII的输出进行大小写转换。如果这个输入的词太长（如果它包含的不
仅仅是数字课超过20个词或者若包含的仅仅是数字则可超过6位）然后这个词就
通过取开始到结尾的13或者三个词缩短为20--6个词长度。
**


*/
static void copy_stemmer(const char *zIn, int nIn, char *zOut, int *pnOut){
  int i, mx, j;
  int hasDigit = 0;
  for(i=0; i<nIn; i++){
    char c = zIn[i];
    if( c>='A' && c<='Z' ){
      zOut[i] = c - 'A' + 'a';
    }else{
      if( c>='0' && c<='9' ) hasDigit = 1;
      zOut[i] = c;
    }
  }
  mx = hasDigit ? 3 : 10;
  if( nIn>mx*2 ){
    for(j=mx, i=nIn-mx; i<nIn; i++, j++){
      zOut[j] = zOut[i];
    }
    i = j;
  }
  zOut[i] = 0;
  *pnOut = i;
}


/*
** Stem the input word zIn[0..nIn-1].  Store the output in zOut.
** zOut is at least big enough to hold nIn bytes.  Write the actual
** size of the output word (exclusive of the '\0' terminator) into *pnOut.
**
** Any upper-case characters in the US-ASCII character set ([A-Z])
** are converted to lower case.  Upper-case UTF characters are
** unchanged.
**
** Words that are longer than about 20 bytes are stemmed by retaining
** a few bytes from the beginning and the end of the word.  If the
** word contains digits, 3 bytes are taken from the beginning and
** 3 bytes from the end.  For long words without digits, 10 bytes
** are taken from each end.  US-ASCII case folding still applies.
** 
** If the input word contains not digits but does characters not 
** in [a-zA-Z] then no stemming is attempted and this routine just 
** copies the input into the input into the output with US-ASCII
** case folding.
**
** Stemming never increases the length of the word.  So there is
** no chance of overflowing the zOut buffer.、
**词干的输入词zIn[0..n[n-1].储存输出给zOut.zOut至少是大到足够去nIn的位。打印出
**输出词的确定尺寸（不包括'\0'终止符）给*pnOut。
**在US-ASCII码字符集中，任何[A-Z]之间的大写字母都被转变为响应的小写字母。UTF格式
**的大写字母不变化。
**那些超过20个字节的词从这个词的开头到结尾被保留几位然后提取出来.如果这个词包含数字，
**那这个词的前三位和后三位提取。对于那些不含有数字的长词，词的后10位被提取。使用US-ASCII
**编码的情况统一适用。
**提取从来都不会增加词的长度，因此就不会发生zOut数组的溢出。
*/
static void porter_stemmer(const char *zIn, int nIn, char *zOut, int *pnOut){
  int i, j;
  char zReverse[28];
  char *z, *z2;
  if( nIn<3 || nIn>=(int)sizeof(zReverse)-7 ){
    /* The word is too big or too small for the porter stemmer.
    ** Fallback to the copy stemmer */
    copy_stemmer(zIn, nIn, zOut, pnOut);
    return;
  }
  for(i=0, j=sizeof(zReverse)-6; i<nIn; i++, j--){
    char c = zIn[i];
    if( c>='A' && c<='Z' ){
      zReverse[j] = c + 'a' - 'A';
    }else if( c>='a' && c<='z' ){
      zReverse[j] = c;
    }else{
      /* The use of a character not in [a-zA-Z] means that we fallback
      ** to the copy stemmer */
      copy_stemmer(zIn, nIn, zOut, pnOut);
      return;
    }
  }
  memset(&zReverse[sizeof(zReverse)-5], 0, 5);
  z = &zReverse[j+1];


  /* Step 1a */
  if( z[0]=='s' ){
    if(
     !stem(&z, "sess", "ss", 0) &&
     !stem(&z, "sei", "i", 0)  &&
     !stem(&z, "ss", "ss", 0)
    ){
      z++;
    }
  }

  /* Step 1b */  
  z2 = z;
  if( stem(&z, "dee", "ee", m_gt_0) ){
    /* Do nothing.  The work was all in the test */
  }else if( 
     (stem(&z, "gni", "", hasVowel) || stem(&z, "de", "", hasVowel))
      && z!=z2
  ){
     if( stem(&z, "ta", "ate", 0) ||
         stem(&z, "lb", "ble", 0) ||
         stem(&z, "zi", "ize", 0) ){
       /* Do nothing.  The work was all in the test */
     }else if( doubleConsonant(z) && (*z!='l' && *z!='s' && *z!='z') ){
       z++;
     }else if( m_eq_1(z) && star_oh(z) ){
       *(--z) = 'e';
     }
  }

  /* Step 1c */
  if( z[0]=='y' && hasVowel(z+1) ){
    z[0] = 'i';
  }

  /* Step 2 */
  switch( z[1] ){
   case 'a':
     stem(&z, "lanoita", "ate", m_gt_0) ||
     stem(&z, "lanoit", "tion", m_gt_0);
     break;
   case 'c':
     stem(&z, "icne", "ence", m_gt_0) ||
     stem(&z, "icna", "ance", m_gt_0);
     break;
   case 'e':
     stem(&z, "rezi", "ize", m_gt_0);
     break;
   case 'g':
     stem(&z, "igol", "log", m_gt_0);
     break;
   case 'l':
     stem(&z, "ilb", "ble", m_gt_0) ||
     stem(&z, "illa", "al", m_gt_0) ||
     stem(&z, "iltne", "ent", m_gt_0) ||
     stem(&z, "ile", "e", m_gt_0) ||
     stem(&z, "ilsuo", "ous", m_gt_0);
     break;
   case 'o':
     stem(&z, "noitazi", "ize", m_gt_0) ||
     stem(&z, "noita", "ate", m_gt_0) ||
     stem(&z, "rota", "ate", m_gt_0);
     break;
   case 's':
     stem(&z, "msila", "al", m_gt_0) ||
     stem(&z, "ssenevi", "ive", m_gt_0) ||
     stem(&z, "ssenluf", "ful", m_gt_0) ||
     stem(&z, "ssensuo", "ous", m_gt_0);
     break;
   case 't':
     stem(&z, "itila", "al", m_gt_0) ||
     stem(&z, "itivi", "ive", m_gt_0) ||
     stem(&z, "itilib", "ble", m_gt_0);
     break;
  }

  /* Step 3 */
  switch( z[0] ){
   case 'e':
     stem(&z, "etaci", "ic", m_gt_0) ||
     stem(&z, "evita", "", m_gt_0)   ||
     stem(&z, "ezila", "al", m_gt_0);
     break;
   case 'i':
     stem(&z, "itici", "ic", m_gt_0);
     break;
   case 'l':
     stem(&z, "laci", "ic", m_gt_0) ||
     stem(&z, "luf", "", m_gt_0);
     break;
   case 's':
     stem(&z, "ssen", "", m_gt_0);
     break;
  }

  /* Step 4 */
  switch( z[1] ){
   case 'a':
     if( z[0]=='l' && m_gt_1(z+2) ){
       z += 2;
     }
     break;
   case 'c':
     if( z[0]=='e' && z[2]=='n' && (z[3]=='a' || z[3]=='e')  && m_gt_1(z+4)  ){
       z += 4;
     }
     break;
   case 'e':
     if( z[0]=='r' && m_gt_1(z+2) ){
       z += 2;
     }
     break;
   case 'i':
     if( z[0]=='c' && m_gt_1(z+2) ){
       z += 2;
     }
     break;
   case 'l':
     if( z[0]=='e' && z[2]=='b' && (z[3]=='a' || z[3]=='i') && m_gt_1(z+4) ){
       z += 4;
     }
     break;
   case 'n':
     if( z[0]=='t' ){
       if( z[2]=='a' ){
         if( m_gt_1(z+3) ){
           z += 3;
         }
       }else if( z[2]=='e' ){
         stem(&z, "tneme", "", m_gt_1) ||
         stem(&z, "tnem", "", m_gt_1) ||
         stem(&z, "tne", "", m_gt_1);
       }
     }
     break;
   case 'o':
     if( z[0]=='u' ){
       if( m_gt_1(z+2) ){
         z += 2;
       }
     }else if( z[3]=='s' || z[3]=='t' ){
       stem(&z, "noi", "", m_gt_1);
     }
     break;
   case 's':
     if( z[0]=='m' && z[2]=='i' && m_gt_1(z+3) ){
       z += 3;
     }
     break;
   case 't':
     stem(&z, "eta", "", m_gt_1) ||
     stem(&z, "iti", "", m_gt_1);
     break;
   case 'u':
     if( z[0]=='s' && z[2]=='o' && m_gt_1(z+3) ){
       z += 3;
     }
     break;
   case 'v':
   case 'z':
     if( z[0]=='e' && z[2]=='i' && m_gt_1(z+3) ){
       z += 3;
     }
     break;
  }

  /* Step 5a */
  if( z[0]=='e' ){
    if( m_gt_1(z+1) ){
      z++;
    }else if( m_eq_1(z+1) && !star_oh(z+1) ){
      z++;
    }
  }

  /* Step 5b */
  if( m_gt_1(z) && z[0]=='l' && z[1]=='l' ){
    z++;
  }

  /* z[] is now the stemmed word in reverse order.  Flip it back
  ** around into forward order and return.
  */
  *pnOut = i = (int)strlen(z);
  zOut[i] = 0;
  while( *z ){
    zOut[--i] = *(z++);
  }
}

/*
** Characters that can be part of a token.  We assume any character
** whose value is greater than 0x80 (any UTF character) can be
** part of a token.  In other words, delimiters all must have
** values of 0x7f or lower.
那些可能成为标记部分的字符，我们假设任何词的值都要比0X80（任何UTF字符）大的
都可能成为标记的部分。换句话说，分隔符必须有0X7fDE或者更小的值。
*/
static const char porterIdChar[] = {
/* x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,  /* 3x */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 4x */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,  /* 5x */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 6x */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,  /* 7x */
};
#define isDelim(C) (((ch=C)&0x80)==0 && (ch<0x30 || !porterIdChar[ch-0x30]))

/*
** Extract the next token from a tokenization cursor.  The cursor must
** have been opened by a prior call to porterOpen().
**从分词器游标中提取下一个被标记的词。这个游标一定会被先前函数porterOpen()的调用打开
*/
static int porterNext(
  sqlite3_tokenizer_cursor *pCursor,  /* Cursor returned by porterOpen */
  const char **pzToken,               /* OUT: *pzToken is the token text */
  int *pnBytes,                       /* OUT: Number of bytes in token */
  int *piStartOffset,                 /* OUT: Starting offset of token */
  int *piEndOffset,                   /* OUT: Ending offset of token */
  int *piPosition                     /* OUT: Position integer of token */
){
  porter_tokenizer_cursor *c = (porter_tokenizer_cursor *) pCursor;
  const char *z = c->zInput;

  while( c->iOffset<c->nInput ){
    int iStartOffset, ch;

    /* Scan past delimiter characters */
    while( c->iOffset<c->nInput && isDelim(z[c->iOffset]) ){
      c->iOffset++;
    }

    /* Count non-delimiter characters. */
    iStartOffset = c->iOffset;
    while( c->iOffset<c->nInput && !isDelim(z[c->iOffset]) ){
      c->iOffset++;
    }

    if( c->iOffset>iStartOffset ){
      int n = c->iOffset-iStartOffset;
      if( n>c->nAllocated ){
        char *pNew;
        c->nAllocated = n+20;
        pNew = sqlite3_realloc(c->zToken, c->nAllocated);
        if( !pNew ) return SQLITE_NOMEM;
        c->zToken = pNew;
      }
      porter_stemmer(&z[iStartOffset], n, c->zToken, pnBytes);
      *pzToken = c->zToken;
      *piStartOffset = iStartOffset;
      *piEndOffset = c->iOffset;
      *piPosition = c->iToken++;
      return SQLITE_OK;
    }
  }
  return SQLITE_DONE;
}

/*
** The set of routines that implement the porter-stemmer tokenizer
*/
static const sqlite3_tokenizer_module porterTokenizerModule = {
  0,
  porterCreate,
  porterDestroy,
  porterOpen,
  porterClose,
  porterNext,
  0
};

/*
** Allocate a new porter tokenizer.  Return a pointer to the new
** tokenizer in *ppModule
*/
void sqlite3Fts3PorterTokenizerModule(
  sqlite3_tokenizer_module const**ppModule
){
  *ppModule = &porterTokenizerModule;
}

#endif /* !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_FTS3) */
