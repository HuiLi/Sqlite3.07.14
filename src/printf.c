/*
** The "printf" code that follows dates from the 1980's.  It is in
** the public domain.  The original comments are included here for
** completeness.  They are very out-of-date but might be useful as
** an historical reference.  Most of the "enhancements" have been backed
** out so that the functionality is now the same as standard printf().
**
**************************************************************************
**
** This file contains code for a set of "printf"-like routines.  These
** routines format strings much like the printf() from the standard C
** library, though the implementation here has enhancements to support
** SQLlite.
*/
/*
** ”printf“代码的起源要追溯到上个20世纪80年代。它不受版权的限制。
** 为了保持完整性，原始的注释都被包含在这里。
** 它们虽然很过时，但是作为一种具有历史意义的参考应该还是有用处的。
** 大多数的增强功能被拿掉，以便现在的功能性和标准的printf()函数一样。
**
**************************************************************************
**
** 这个文件包含一组“printf”式的代码例程。
** 这些例程格式字符串就像标准C库中的printf()函数,尽管这里的实现增强了SQLlite这样的支持。 
*/
#include "sqliteInt.h"/*头文件*/

/*
** Conversion types fall into various categories as defined by the
** following enumeration.
*//*转换类型分为不同的类别，正如以下列举所定义的。*/
#define etRADIX       1 /* Integer types.  %d, %x, %o, and so forth *//*整数类型。%d, %x 和 %o 等等*/
#define etFLOAT       2 /* Floating point.  %f *//*浮点型*/
#define etEXP         3 /* Exponentional notation. %e and %E *//*指数表示。%e 和 %E*/
#define etGENERIC     4 /* Floating or exponential, depending on exponent. %g *//*根据指数，确定是浮点数还是指数表示。%g*/
#define etSIZE        5 /* Return number of characters processed so far. %n *//*到目前为止处理过的字符的返回数。%n*/
#define etSTRING      6 /* Strings. %s *//*字符串。%s*/
#define etDYNSTRING   7 /* Dynamically allocated strings. %z *//*动态分配的字符串。%z*/
#define etPERCENT     8 /* Percent symbol. %% *//*百分比符号。%%*/
#define etCHARX       9 /* Characters. %c *//*字符。%c*/
/* The rest are extensions, not normally found in printf() *//*其余的都是扩展，通常都不会在printf()中发现*/
#define etSQLESCAPE  10 /* Strings with '\'' doubled.  %q *//*带两个'\'的字符串*/
#define etSQLESCAPE2 11 /* Strings with '\'' doubled and enclosed in '',
                          NULL pointers replaced by SQL NULL.  %Q *//*带2个'\'并被放在''里的字符串，空指针被SQL空所取代。%Q*/
#define etTOKEN      12 /* a pointer to a Token structure *//*指向一个令牌结构的指针*/
#define etSRCLIST    13 /* a pointer to a SrcList *//*指向SrcList的指针*/
#define etPOINTER    14 /* The %p conversion *//*%p的转换*/
#define etSQLESCAPE3 15 /* %w -> Strings with '\"' doubled *//*带两个'\"'的字符串。%w*/*/
#define etORDINAL    16 /* %r -> 1st, 2nd, 3rd, 4th, etc.  English only *//*第1个，第2个，第3个，第4个，等等。仅限英语。%r*/

#define etINVALID     0 /* Any unrecognized conversion type *//*任何不能识别的转换类型*/


/*
** An "etByte" is an 8-bit unsigned value.
*//*一个"etByte"表示一字节的无符号数*/
typedef unsigned char etByte;

/*
** Each builtin conversion character (ex: the 'd' in "%d") is described
** by an instance of the following structure
*//*每个装入的转换字符(例:“% d”中的“d”)由以下结构的实例描述*/
typedef struct et_info {   /* Information about each format field *//*关于每一个格式化区域的信息*/
  char fmttype;            /* The format field code letter *//*格式化区域的代码字母*/
  etByte base;             /* The base for radix conversion *//*基数转换的基础*/
  etByte flags;            /* One or more of FLAG_ constants below *//*一个或多个FLAG_下面的常量*/
  etByte type;             /* Conversion paradigm *//*转换模式*/
  etByte charset;          /* Offset into aDigits[] of the digits string *//*aDigits[]中偏移量的数字字符串*/
  etByte prefix;           /* Offset into aPrefix[] of the prefix string *//*aPrefix[]中偏移量的前缀字符串*/
} et_info;

/*
** Allowed values for et_info.flags
*//*et_info.flags允许的值*/
#define FLAG_SIGNED  1     /* True if the value to convert is signed *//*如果要转换的数是有符号数就为真*/
#define FLAG_INTERN  2     /* True if for internal use only *//*如果只作内部使用就为真*/
#define FLAG_STRING  4     /* Allow infinity precision *//*允许无限精度*/


/*
** The following table is searched linearly, so it is good to put the
** most frequently used conversion types first.
*//*下表是线性搜索,所以最好先使用最常用的转换类型。*/
static const char aDigits[] = "0123456789ABCDEF0123456789abcdef";
static const char aPrefix[] = "-x0\000X0";
static const et_info fmtinfo[] = {
  {  'd', 10, 1, etRADIX,      0,  0 },
  {  's',  0, 4, etSTRING,     0,  0 },
  {  'g',  0, 1, etGENERIC,    30, 0 },
  {  'z',  0, 4, etDYNSTRING,  0,  0 },
  {  'q',  0, 4, etSQLESCAPE,  0,  0 },
  {  'Q',  0, 4, etSQLESCAPE2, 0,  0 },
  {  'w',  0, 4, etSQLESCAPE3, 0,  0 },
  {  'c',  0, 0, etCHARX,      0,  0 },
  {  'o',  8, 0, etRADIX,      0,  2 },
  {  'u', 10, 0, etRADIX,      0,  0 },
  {  'x', 16, 0, etRADIX,      16, 1 },
  {  'X', 16, 0, etRADIX,      0,  4 },
#ifndef SQLITE_OMIT_FLOATING_POINT
  {  'f',  0, 1, etFLOAT,      0,  0 },
  {  'e',  0, 1, etEXP,        30, 0 },
  {  'E',  0, 1, etEXP,        14, 0 },
  {  'G',  0, 1, etGENERIC,    14, 0 },
#endif/*如果这4个元素没有被定义，则重新定义，如果已经定义，那么直接跳过*/
  {  'i', 10, 1, etRADIX,      0,  0 },
  {  'n',  0, 0, etSIZE,       0,  0 },
  {  '%',  0, 0, etPERCENT,    0,  0 },
  {  'p', 16, 0, etPOINTER,    0,  1 },

/* All the rest have the FLAG_INTERN bit set and are thus for internal
** use only *//*其余有FLAG_INTERN位组,因此仅供内部使用*/
  {  'T',  0, 2, etTOKEN,      0,  0 },
  {  'S',  0, 2, etSRCLIST,    0,  0 },
  {  'r', 10, 3, etORDINAL,    0,  0 },
};

/*
** If SQLITE_OMIT_FLOATING_POINT is defined, then none of the floating point
** conversions will work.
*//*如果SQLITE_OMIT_FLOATING_POINT被定义,那么所有的浮点转换将不会有作用*/
#ifndef SQLITE_OMIT_FLOATING_POINT
/*
** "*val" is a double such that 0.1 <= *val < 10.0
** Return the ascii code for the leading digit of *val, then
** multiply "*val" by 10.0 to renormalize.
**
** Example:
**     input:     *val = 3.14159
**     output:    *val = 1.4159    function return = '3'
**
** The counter *cnt is incremented each time.  After counter exceeds
** 16 (the number of significant digits in a 64-bit float) '0' is
** always returned.
*/
/*
** ”*val“是double类型的数，例如0.1<=*val<10.0，返回一个数的最前的数字的ascii码，然后再把*val乘以10之后重新正常化。
** 
** 例如：
**      输入：     *val = 3.14159
** 	    输出：     *val = 1.4159    function return = '3'
** 
** 计数器*cnt每次都会增加，如果超过16（64位浮点数中的有效位数），就返回0
*/
static char et_getdigit(LONGDOUBLE_TYPE *val, int *cnt){
  int digit;
  LONGDOUBLE_TYPE d;
  if( (*cnt)<=0 ) return '0';
  (*cnt)--;
  digit = (int)*val;
  d = digit;
  digit += '0';
  *val = (*val - d)*10.0;
  return (char)digit;
}
#endif /* SQLITE_OMIT_FLOATING_POINT */

/*
** Append N space characters to the given string buffer.
*//*增加N个空格字符到指定字符串缓冲区*/
void sqlite3AppendSpace(StrAccum *pAccum, int N){
  static const char zSpaces[] = "                             ";/*备用的空格字符*/
  while( N>=(int)sizeof(zSpaces)-1 ){
    sqlite3StrAccumAppend(pAccum, zSpaces, sizeof(zSpaces)-1);/*如果缓冲区的空格数少于N，那么添加空格，直到添加N个为止*/
    N -= sizeof(zSpaces)-1;
  }
  if( N>0 ){
    sqlite3StrAccumAppend(pAccum, zSpaces, N);
  }
}

/*
** On machines with a small stack size, you can redefine the
** SQLITE_PRINT_BUF_SIZE to be something smaller, if desired.
*//*在一个小容量栈的机器指令中，如果需要的话，你可以重新定义SQLITE_PRINT_BUF_SIZE使它变得更小。*/
#ifndef SQLITE_PRINT_BUF_SIZE
# define SQLITE_PRINT_BUF_SIZE 70
#endif
#define etBUFSIZE SQLITE_PRINT_BUF_SIZE  /* Size of the output buffer *//*输出缓冲区的大小*/

/*
** Render a string given by "fmt" into the StrAccum object.
*//*渲染一个通过fmt传进StrAccum对象中的字符串*/
void sqlite3VXPrintf(
  StrAccum *pAccum,                  /* Accumulate results here *//*这里表示累计结果*/
  int useExtended,                   /* Allow extended %-conversions *//*允许扩展的%-conversions*/
  const char *fmt,                   /* Format string *//*格式字符串*/
  va_list ap                         /* arguments *//*参数*/
){
  int c;                     /* Next character in the format string *//*下一个格式字符串中的字符*/
  char *bufpt;               /* Pointer to the conversion buffer *//*指向转换缓冲区的指针*/
  int precision;             /* Precision of the current field *//*当前区域的精度*/
  int length;                /* Length of the field *//*区域长度*/
  int idx;                   /* A general purpose loop counter *//*通用循环计数器*/
  int width;                 /* Width of the current field *//*当前区域的宽度*/
  etByte flag_leftjustify;   /* True if "-" flag is present *//*如果“-”标识出现即为真*/
  etByte flag_plussign;      /* True if "+" flag is present *//*如果“+”标识出现即为真*/
  etByte flag_blanksign;     /* True if " " flag is present *//*如果“ ”标识出现即为真*/
  etByte flag_alternateform; /* True if "#" flag is present *//*如果“#”标识出现即为真*/
  etByte flag_altform2;      /* True if "!" flag is present *//*如果“!”标识出现即为真*/
  etByte flag_zeropad;       /* True if field width constant starts with zero *//*如果区域宽度常量都是从0开始即为真*/
  etByte flag_long;          /* True if "l" flag is present *//*如果“1”标识出现即为真*/
  etByte flag_longlong;      /* True if the "ll" flag is present *//*如果“11”标识出现即为真*/
  etByte done;               /* Loop termination flag *//*循环终止标志*/
  etByte xtype = 0;          /* Conversion paradigm *//*转换模式*/
  char prefix;               /* Prefix character.  "+" or "-" or " " or '\0'. *//*前缀字符。"+" 或 "-" 或 " " 或 '\0'*/
  sqlite_uint64 longvalue;   /* Value for integer types *//*值为整数类型*/
  LONGDOUBLE_TYPE realvalue; /* Value for real types *//*值为实数类型*/
  const et_info *infop;      /* Pointer to the appropriate info structure *//*指向适当信息结构的指针*/
  char *zOut;                /* Rendering buffer *//*渲染缓冲区*/
  int nOut;                  /* Size of the rendering buffer *//*渲染缓冲区的大小*/
  char *zExtra;              /* Malloced memory used by some conversion *//*被一些转换使用的内存*/
#ifndef SQLITE_OMIT_FLOATING_POINT
  int  exp, e2;              /* exponent of real numbers *//*实型指数*/
  int nsd;                   /* Number of significant digits returned *//*返回的有效数位*/
  double rounder;            /* Used for rounding floating point values *//*用于四舍五入的浮点值*/
  etByte flag_dp;            /* True if decimal point should be shown *//*如果显示小数即为真*/
  etByte flag_rtz;           /* True if trailing zeros should be removed *//*如果尾随零被删除即为真*/
#endif
  char buf[etBUFSIZE];       /* Conversion buffer *//*转换缓冲区*/

  bufpt = 0;
  for(; (c=(*fmt))!=0; ++fmt){
    if( c!='%' ){
      int amt;
      bufpt = (char *)fmt;
      amt = 1;
      while( (c=(*++fmt))!='%' && c!=0 ) amt++;
      sqlite3StrAccumAppend(pAccum, bufpt, amt);
      if( c==0 ) break;
    }
    if( (c=(*++fmt))==0 ){
      sqlite3StrAccumAppend(pAccum, "%", 1);
      break;
    }
    /* Find out what flags are present *//*找出出现的标识*/
    flag_leftjustify = flag_plussign = flag_blanksign = 
     flag_alternateform = flag_altform2 = flag_zeropad = 0;
    done = 0;
    do{
      switch( c ){
        case '-':   flag_leftjustify = 1;     break;
        case '+':   flag_plussign = 1;        break;
        case ' ':   flag_blanksign = 1;       break;
        case '#':   flag_alternateform = 1;   break;
        case '!':   flag_altform2 = 1;        break;
        case '0':   flag_zeropad = 1;         break;
        default:    done = 1;                 break;
      }
    }while( !done && (c=(*++fmt))!=0 );
    /* Get the field width *//*得到区域宽度*/
    width = 0;
    if( c=='*' ){
      width = va_arg(ap,int);
      if( width<0 ){
        flag_leftjustify = 1;
        width = -width;
      }
      c = *++fmt;
    }else{
      while( c>='0' && c<='9' ){
        width = width*10 + c - '0';
        c = *++fmt;
      }
    }
    /* Get the precision *//*得到区域精度*/
    if( c=='.' ){
      precision = 0;
      c = *++fmt;
      if( c=='*' ){
        precision = va_arg(ap,int);
        if( precision<0 ) precision = -precision;
        c = *++fmt;
      }else{
        while( c>='0' && c<='9' ){
          precision = precision*10 + c - '0';
          c = *++fmt;
        }
      }
    }else{
      precision = -1;
    }
    /* Get the conversion type modifier *//*得到转换类型修饰符*/
    if( c=='l' ){
      flag_long = 1;
      c = *++fmt;
      if( c=='l' ){
        flag_longlong = 1;
        c = *++fmt;
      }else{
        flag_longlong = 0;
      }
    }else{
      flag_long = flag_longlong = 0;
    }
    /* Fetch the info entry for the field *//*得到区域信息入口*/
    infop = &fmtinfo[0];
    xtype = etINVALID;
    for(idx=0; idx<ArraySize(fmtinfo); idx++){
      if( c==fmtinfo[idx].fmttype ){
        infop = &fmtinfo[idx];
        if( useExtended || (infop->flags & FLAG_INTERN)==0 ){
          xtype = infop->type;
        }else{
          return;
        }
        break;
      }
    }
    zExtra = 0;

    /*
    ** At this point, variables are initialized as follows:
    **
    **   flag_alternateform          TRUE if a '#' is present.
    **   flag_altform2               TRUE if a '!' is present.
    **   flag_plussign               TRUE if a '+' is present.
    **   flag_leftjustify            TRUE if a '-' is present or if the
    **                               field width was negative.
    **   flag_zeropad                TRUE if the width began with 0.
    **   flag_long                   TRUE if the letter 'l' (ell) prefixed
    **                               the conversion character.
    **   flag_longlong               TRUE if the letter 'll' (ell ell) prefixed
    **                               the conversion character.
    **   flag_blanksign              TRUE if a ' ' is present.
    **   width                       The specified field width.  This is
    **                               always non-negative.  Zero is the default.
    **   precision                   The specified precision.  The default
    **                               is -1.
    **   xtype                       The class of the conversion.
    **   infop                       Pointer to the appropriate info struct.
    */
	/*
	** 这时候，变量的初始化如下所示
	**
    **   flag_alternateform          如果“#”标识出现即为真。
    **   flag_altform2               如果“!”标识出现即为真。
    **   flag_plussign               如果“+”标识出现即为真。
    **   flag_leftjustify            如果“-”标识出现或区域宽度为负即为真。
    **   flag_zeropad                如果宽度从零开始即为真。
    **   flag_long                   如果字母'l'放在转换字符的前面作为前缀即为真。
    **   flag_longlong               如果字母'll'放在转换字符的前面作为前缀即为真。
    **   flag_blanksign              如果“ ”标识出现即为真。
    **   width                       指定的区域宽度，非负，默认为零。
    **   precision                   制定的精度，默认为-1。
    **   xtype                       类的转换。
    **   infop                       指向适当信息结构的指针。
	*/
    switch( xtype ){
      case etPOINTER:
        flag_longlong = sizeof(char*)==sizeof(i64);
        flag_long = sizeof(char*)==sizeof(long int);
        /* Fall through into the next case *//*进入下一个案例*/
      case etORDINAL:
      case etRADIX:
        if( infop->flags & FLAG_SIGNED ){
          i64 v;
          if( flag_longlong ){
            v = va_arg(ap,i64);
          }else if( flag_long ){
            v = va_arg(ap,long int);
          }else{
            v = va_arg(ap,int);
          }
          if( v<0 ){
            if( v==SMALLEST_INT64 ){
              longvalue = ((u64)1)<<63;
            }else{
              longvalue = -v;
            }
            prefix = '-';
          }else{
            longvalue = v;
            if( flag_plussign )        prefix = '+';
            else if( flag_blanksign )  prefix = ' ';
            else                       prefix = 0;
          }
        }else{
          if( flag_longlong ){
            longvalue = va_arg(ap,u64);
          }else if( flag_long ){
            longvalue = va_arg(ap,unsigned long int);
          }else{
            longvalue = va_arg(ap,unsigned int);
          }
          prefix = 0;
        }
        if( longvalue==0 ) flag_alternateform = 0;
        if( flag_zeropad && precision<width-(prefix!=0) ){
          precision = width-(prefix!=0);
        }
        if( precision<etBUFSIZE-10 ){
          nOut = etBUFSIZE;
          zOut = buf;
        }else{
          nOut = precision + 10;
          zOut = zExtra = sqlite3Malloc( nOut );
          if( zOut==0 ){
            pAccum->mallocFailed = 1;
            return;
          }
        }
        bufpt = &zOut[nOut-1];
        if( xtype==etORDINAL ){
          static const char zOrd[] = "thstndrd";
          int x = (int)(longvalue % 10);
          if( x>=4 || (longvalue/10)%10==1 ){
            x = 0;
          }
          *(--bufpt) = zOrd[x*2+1];
          *(--bufpt) = zOrd[x*2];
        }
        {
          register const char *cset;      /* Use registers for speed *//*使用速度寄存器*/
          register int base;
          cset = &aDigits[infop->charset];
          base = infop->base;
          do{                                           /* Convert to ascii *//*转换为ascii码*/
            *(--bufpt) = cset[longvalue%base];
            longvalue = longvalue/base;
          }while( longvalue>0 );
        }
        length = (int)(&zOut[nOut-1]-bufpt);
        for(idx=precision-length; idx>0; idx--){
          *(--bufpt) = '0';                             /* Zero pad *//*零填充模块*/
        }
        if( prefix ) *(--bufpt) = prefix;               /* Add sign *//*添加过的痕迹*/
        if( flag_alternateform && infop->prefix ){      /* Add "0" or "0x" *//*添加"0" 或 "0x"*/
          const char *pre;
          char x;
          pre = &aPrefix[infop->prefix];
          for(; (x=(*pre))!=0; pre++) *(--bufpt) = x;
        }
        length = (int)(&zOut[nOut-1]-bufpt);
        break;
      case etFLOAT:
      case etEXP:
      case etGENERIC:
        realvalue = va_arg(ap,double);
#ifdef SQLITE_OMIT_FLOATING_POINT
        length = 0;
#else
        if( precision<0 ) precision = 6;         /* Set default precision *//*设置默认的精度*/
        if( realvalue<0.0 ){
          realvalue = -realvalue;
          prefix = '-';
        }else{
          if( flag_plussign )          prefix = '+';
          else if( flag_blanksign )    prefix = ' ';
          else                         prefix = 0;
        }
        if( xtype==etGENERIC && precision>0 ) precision--;
#if 0
        /* Rounding works like BSD when the constant 0.4999 is used.  Wierd! *//*当使用常数0.4999时进行像BSD一样的凑整*/
        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);
#else
        /* It makes more sense to use 0.5 *//*使用0.5更有意义*/
        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1){}
#endif
        if( xtype==etFLOAT ) realvalue += rounder;
        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 *//*使realvalue标准化，即10.0 > realvalue >= 1.0*/
        exp = 0;
        if( sqlite3IsNaN((double)realvalue) ){
          bufpt = "NaN";
          length = 3;
          break;
        }
        if( realvalue>0.0 ){
          LONGDOUBLE_TYPE scale = 1.0;
          while( realvalue>=1e100*scale && exp<=350 ){ scale *= 1e100;exp+=100;}
          while( realvalue>=1e64*scale && exp<=350 ){ scale *= 1e64; exp+=64; }
          while( realvalue>=1e8*scale && exp<=350 ){ scale *= 1e8; exp+=8; }
          while( realvalue>=10.0*scale && exp<=350 ){ scale *= 10.0; exp++; }
          realvalue /= scale;
          while( realvalue<1e-8 ){ realvalue *= 1e8; exp-=8; }
          while( realvalue<1.0 ){ realvalue *= 10.0; exp--; }
          if( exp>350 ){
            if( prefix=='-' ){
              bufpt = "-Inf";
            }else if( prefix=='+' ){
              bufpt = "+Inf";
            }else{
              bufpt = "Inf";
            }
            length = sqlite3Strlen30(bufpt);
            break;
          }
        }
        bufpt = buf;
        /*
        ** If the field type is etGENERIC, then convert to either etEXP
        ** or etFLOAT, as appropriate.
        *//*如果区域类型是etGENERIC，那么合理地转换为etEXP或etFLOAT*/
        if( xtype!=etFLOAT ){
          realvalue += rounder;
          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }
        }
        if( xtype==etGENERIC ){
          flag_rtz = !flag_alternateform;
          if( exp<-4 || exp>precision ){
            xtype = etEXP;
          }else{
            precision = precision - exp;
            xtype = etFLOAT;
          }
        }else{
          flag_rtz = flag_altform2;
        }
        if( xtype==etEXP ){
          e2 = 0;
        }else{
          e2 = exp;
        }
        if( e2+precision+width > etBUFSIZE - 15 ){
          bufpt = zExtra = sqlite3Malloc( e2+precision+width+15 );
          if( bufpt==0 ){
            pAccum->mallocFailed = 1;
            return;
          }
        }
        zOut = bufpt;
        nsd = 16 + flag_altform2*10;
        flag_dp = (precision>0 ?1:0) | flag_alternateform | flag_altform2;
        /* The sign in front of the number *//*数字前面的前缀符号*/
        if( prefix ){
          *(bufpt++) = prefix;
        }
        /* Digits prior to the decimal point *//*小数点前面的数字*/
        if( e2<0 ){
          *(bufpt++) = '0';
        }else{
          for(; e2>=0; e2--){
            *(bufpt++) = et_getdigit(&realvalue,&nsd);
          }
        }
        /* The decimal point *//*小数点*/
        if( flag_dp ){
          *(bufpt++) = '.';
        }
        /* "0" digits after the decimal point but before the first
        ** significant digit of the number *//*在小数点后且在有效位数前面添加”0“*/
        for(e2++; e2<0; precision--, e2++){
          assert( precision>0 );
          *(bufpt++) = '0';
        }
        /* Significant digits after the decimal point *//*小数点后面的有效数字*/
        while( (precision--)>0 ){
          *(bufpt++) = et_getdigit(&realvalue,&nsd);
        }
        /* Remove trailing zeros and the "." if no digits follow the "." *//*如果”.“后面没有数字，那么删除尾随零和”.“*/
        if( flag_rtz && flag_dp ){
          while( bufpt[-1]=='0' ) *(--bufpt) = 0;
          assert( bufpt>zOut );
          if( bufpt[-1]=='.' ){
            if( flag_altform2 ){
              *(bufpt++) = '0';
            }else{
              *(--bufpt) = 0;
            }
          }
        }
        /* Add the "eNNN" suffix *//*添加后缀"eNNN"*/
        if( xtype==etEXP ){
          *(bufpt++) = aDigits[infop->charset];
          if( exp<0 ){
            *(bufpt++) = '-'; exp = -exp;
          }else{
            *(bufpt++) = '+';
          }
          if( exp>=100 ){
            *(bufpt++) = (char)((exp/100)+'0');        /* 100's digit *//*百位数*/
            exp %= 100;
          }
          *(bufpt++) = (char)(exp/10+'0');             /* 10's digit *//*十位数*/
          *(bufpt++) = (char)(exp%10+'0');             /* 1's digit *//*个位数*/
        }
        *bufpt = 0;

        /* The converted number is in buf[] and zero terminated. Output it.
        ** Note that the number is in the usual order, not reversed as with
        ** integer conversions. *//*被转换的数在数组buf[]中，并且以0结尾，然后输出它。注意，这些数按正常顺序排列，没有改变与整数的转换*/
        length = (int)(bufpt-zOut);
        bufpt = zOut;

        /* Special case:  Add leading zeros if the flag_zeropad flag is
        ** set and we are not left justified *//*特例：若设置flag_zeropad标识并且没有向左对齐，则添加前导零*/
        if( flag_zeropad && !flag_leftjustify && length < width){
          int i;
          int nPad = width - length;
          for(i=width; i>=nPad; i--){
            bufpt[i] = bufpt[i-nPad];
          }
          i = prefix!=0;
          while( nPad-- ) bufpt[i++] = '0';
          length = width;
        }
#endif /* !defined(SQLITE_OMIT_FLOATING_POINT) */
        break;
      case etSIZE:
        *(va_arg(ap,int*)) = pAccum->nChar;
        length = width = 0;
        break;
      case etPERCENT:
        buf[0] = '%';
        bufpt = buf;
        length = 1;
        break;
      case etCHARX:
        c = va_arg(ap,int);
        buf[0] = (char)c;
        if( precision>=0 ){
          for(idx=1; idx<precision; idx++) buf[idx] = (char)c;
          length = precision;
        }else{
          length =1;
        }
        bufpt = buf;
        break;
      case etSTRING:
      case etDYNSTRING:
        bufpt = va_arg(ap,char*);
        if( bufpt==0 ){
          bufpt = "";
        }else if( xtype==etDYNSTRING ){
          zExtra = bufpt;
        }
        if( precision>=0 ){
          for(length=0; length<precision && bufpt[length]; length++){}
        }else{
          length = sqlite3Strlen30(bufpt);
        }
        break;
      case etSQLESCAPE:
      case etSQLESCAPE2:
      case etSQLESCAPE3: {
        int i, j, k, n, isnull;
        int needQuote;
        char ch;
        char q = ((xtype==etSQLESCAPE3)?'"':'\'');   /* Quote character *//*转义字符*/
        char *escarg = va_arg(ap,char*);
        isnull = escarg==0;
        if( isnull ) escarg = (xtype==etSQLESCAPE2 ? "NULL" : "(NULL)");
        k = precision;
        for(i=n=0; k!=0 && (ch=escarg[i])!=0; i++, k--){
          if( ch==q )  n++;
        }
        needQuote = !isnull && xtype==etSQLESCAPE2;
        n += i + 1 + needQuote*2;
        if( n>etBUFSIZE ){
          bufpt = zExtra = sqlite3Malloc( n );
          if( bufpt==0 ){
            pAccum->mallocFailed = 1;
            return;
          }
        }else{
          bufpt = buf;
        }
        j = 0;
        if( needQuote ) bufpt[j++] = q;
        k = i;
        for(i=0; i<k; i++){
          bufpt[j++] = ch = escarg[i];
          if( ch==q ) bufpt[j++] = ch;
        }
        if( needQuote ) bufpt[j++] = q;
        bufpt[j] = 0;
        length = j;
        /* The precision in %q and %Q means how many input characters to
        ** consume, not the length of the output...
        ** if( precision>=0 && precision<length ) length = precision; */
		/* %q 和 %Q的精度意味着消耗多少输入字符，而不是输出字符的长度…
		** if( precision>=0 && precision<length ) length = precision;*/
        break;
      }
      case etTOKEN: {
        Token *pToken = va_arg(ap, Token*);
        if( pToken ){
          sqlite3StrAccumAppend(pAccum, (const char*)pToken->z, pToken->n);
        }
        length = width = 0;
        break;
      }
      case etSRCLIST: {
        SrcList *pSrc = va_arg(ap, SrcList*);
        int k = va_arg(ap, int);
        struct SrcList_item *pItem = &pSrc->a[k];
        assert( k>=0 && k<pSrc->nSrc );
        if( pItem->zDatabase ){
          sqlite3StrAccumAppend(pAccum, pItem->zDatabase, -1);
          sqlite3StrAccumAppend(pAccum, ".", 1);
        }
        sqlite3StrAccumAppend(pAccum, pItem->zName, -1);
        length = width = 0;
        break;
      }
      default: {
        assert( xtype==etINVALID );
        return;
      }
    }/* End switch over the format type *//*终端切换格式类型*/
    /*
    ** The text of the conversion is pointed to by "bufpt" and is
    ** "length" characters long.  The field width is "width".  Do
    ** the output.
    *//*转换的文本指向"bufpt"且有length这么长。区域宽度是"width"。然后输出。*/
    if( !flag_leftjustify ){
      register int nspace;
      nspace = width-length;
      if( nspace>0 ){
        sqlite3AppendSpace(pAccum, nspace);
      }
    }
    if( length>0 ){
      sqlite3StrAccumAppend(pAccum, bufpt, length);
    }
    if( flag_leftjustify ){
      register int nspace;
      nspace = width-length;
      if( nspace>0 ){
        sqlite3AppendSpace(pAccum, nspace);
      }
    }
    sqlite3_free(zExtra);
  }/* End for loop over the format string *//*格式字符串的for循环结束*/
} /* End of function *//*功能结束*/

/*
** Append N bytes of text from z to the StrAccum object.
*//*从z到对象StrAccum添加N个文本字节*/
void sqlite3StrAccumAppend(StrAccum *p, const char *z, int N){
  assert( z!=0 || N==0 );
  if( p->tooBig | p->mallocFailed ){
    testcase(p->tooBig);
    testcase(p->mallocFailed);
    return;
  }
  assert( p->zText!=0 || p->nChar==0 );
  if( N<0 ){
    N = sqlite3Strlen30(z);
  }
  if( N==0 || NEVER(z==0) ){
    return;
  }
  if( p->nChar+N >= p->nAlloc ){
    char *zNew;
    if( !p->useMalloc ){
      p->tooBig = 1;
      N = p->nAlloc - p->nChar - 1;
      if( N<=0 ){
        return;
      }
    }else{
      char *zOld = (p->zText==p->zBase ? 0 : p->zText);
      i64 szNew = p->nChar;
      szNew += N + 1;
      if( szNew > p->mxAlloc ){
        sqlite3StrAccumReset(p);
        p->tooBig = 1;
        return;
      }else{
        p->nAlloc = (int)szNew;
      }
      if( p->useMalloc==1 ){
        zNew = sqlite3DbRealloc(p->db, zOld, p->nAlloc);
      }else{
        zNew = sqlite3_realloc(zOld, p->nAlloc);
      }
      if( zNew ){
        if( zOld==0 && p->nChar>0 ) memcpy(zNew, p->zText, p->nChar);
        p->zText = zNew;
      }else{
        p->mallocFailed = 1;
        sqlite3StrAccumReset(p);
        return;
      }
    }
  }
  assert( p->zText );
  memcpy(&p->zText[p->nChar], z, N);
  p->nChar += N;
}

/*
** Finish off a string by making sure it is zero-terminated.
** Return a pointer to the resulting string.  Return a NULL
** pointer if any kind of error was encountered.
*//*通过确定以0结尾来结束一个字符串。返回一个指针到生成的字符串。若遇到任何一种错误则返回空指针。*/
char *sqlite3StrAccumFinish(StrAccum *p){
  if( p->zText ){
    p->zText[p->nChar] = 0;
    if( p->useMalloc && p->zText==p->zBase ){
      if( p->useMalloc==1 ){
        p->zText = sqlite3DbMallocRaw(p->db, p->nChar+1 );
      }else{
        p->zText = sqlite3_malloc(p->nChar+1);
      }
      if( p->zText ){
        memcpy(p->zText, p->zBase, p->nChar+1);
      }else{
        p->mallocFailed = 1;
      }
    }
  }
  return p->zText;
}

/*
** Reset an StrAccum string.  Reclaim all malloced memory.
*//*重新设置StrAccum类型的字符串。回收所有被分配的内存。*/
void sqlite3StrAccumReset(StrAccum *p){
  if( p->zText!=p->zBase ){
    if( p->useMalloc==1 ){
      sqlite3DbFree(p->db, p->zText);
    }else{
      sqlite3_free(p->zText);
    }
  }
  p->zText = 0;
}

/*
** Initialize a string accumulator
*//*初始化一个字符串累加器*/
void sqlite3StrAccumInit(StrAccum *p, char *zBase, int n, int mx){
  p->zText = p->zBase = zBase;
  p->db = 0;
  p->nChar = 0;
  p->nAlloc = n;
  p->mxAlloc = mx;
  p->useMalloc = 1;
  p->tooBig = 0;
  p->mallocFailed = 0;
}

/*
** Print into memory obtained from sqliteMalloc().  Use the internal
** %-conversion extensions.
*//*打印到从sqliteMalloc()获得的内存中。使用内部%转换扩展。*/
char *sqlite3VMPrintf(sqlite3 *db, const char *zFormat, va_list ap){
  char *z;
  char zBase[SQLITE_PRINT_BUF_SIZE];
  StrAccum acc;
  assert( db!=0 );
  sqlite3StrAccumInit(&acc, zBase, sizeof(zBase),
                      db->aLimit[SQLITE_LIMIT_LENGTH]);
  acc.db = db;
  sqlite3VXPrintf(&acc, 1, zFormat, ap);
  z = sqlite3StrAccumFinish(&acc);
  if( acc.mallocFailed ){
    db->mallocFailed = 1;
  }
  return z;
}

/*
** Print into memory obtained from sqliteMalloc().  Use the internal
** %-conversion extensions.
*//*打印到从sqliteMalloc()获得的内存中。使用内部%转换扩展。*/
char *sqlite3MPrintf(sqlite3 *db, const char *zFormat, ...){
  va_list ap;
  char *z;
  va_start(ap, zFormat);
  z = sqlite3VMPrintf(db, zFormat, ap);
  va_end(ap);
  return z;
}

/*
** Like sqlite3MPrintf(), but call sqlite3DbFree() on zStr after formatting
** the string and before returnning.  This routine is intended to be used
** to modify an existing string.  For example:
**
**       x = sqlite3MPrintf(db, x, "prefix %s suffix", x);
**
*/
/* 和sqlite3MPrintf()相似，但要在格式化字符串之后且返回之前调用参数类型是zStr的sqlite3DbFree()。
** 这个例程的目的是用于修改现有的字符串。例如：
** 
**       x = sqlite3MPrintf(db, x, "prefix %s suffix", x);
** 
*/
char *sqlite3MAppendf(sqlite3 *db, char *zStr, const char *zFormat, ...){
  va_list ap;
  char *z;
  va_start(ap, zFormat);
  z = sqlite3VMPrintf(db, zFormat, ap);
  va_end(ap);
  sqlite3DbFree(db, zStr);
  return z;
}

/*
** Print into memory obtained from sqlite3_malloc().  Omit the internal
** %-conversion extensions.
*//*打印到从sqlite3_malloc()获得的内存中。忽略内部%转换扩展*/
char *sqlite3_vmprintf(const char *zFormat, va_list ap){
  char *z;
  char zBase[SQLITE_PRINT_BUF_SIZE];
  StrAccum acc;
#ifndef SQLITE_OMIT_AUTOINIT
  if( sqlite3_initialize() ) return 0;
#endif
  sqlite3StrAccumInit(&acc, zBase, sizeof(zBase), SQLITE_MAX_LENGTH);
  acc.useMalloc = 2;
  sqlite3VXPrintf(&acc, 0, zFormat, ap);
  z = sqlite3StrAccumFinish(&acc);
  return z;
}

/*
** Print into memory obtained from sqlite3_malloc()().  Omit the internal
** %-conversion extensions.
*//*打印到从sqlite3_malloc()获得的内存中。省略内部%转换扩展*/
char *sqlite3_mprintf(const char *zFormat, ...){
  va_list ap;
  char *z;
#ifndef SQLITE_OMIT_AUTOINIT
  if( sqlite3_initialize() ) return 0;
#endif
  va_start(ap, zFormat);
  z = sqlite3_vmprintf(zFormat, ap);
  va_end(ap);
  return z;
}

/*
** sqlite3_snprintf() works like snprintf() except that it ignores the
** current locale settings.  This is important for SQLite because we
** are not able to use a "," as the decimal point in place of "." as
** specified by some locales.
**
** Oops:  The first two arguments of sqlite3_snprintf() are backwards
** from the snprintf() standard.  Unfortunately, it is too late to change
** this without breaking compatibility, so we just have to live with the
** mistake.
**
** sqlite3_vsnprintf() is the varargs version.
*/
/*
** sqlite3_snprintf() 和 snprintf()工作方式很像。
** 区别是：前者省略了当前环境的设置。
** 这对于SQLite来说很重要，
** 因为我们不能使用一个”,“作为小数点来取代在某些环境中已经说明了的小数点”.“。
** 可惜的是:  sqlite3_snprintf()前两个参数执行snprintf()的标准。
** 不幸的是，在不破坏兼容性的情况下改变它已经太迟了。
** 所以我们不得不容忍了这种错误。
** 
** sqlite3_vsnprintf()是变量参数的版本
*/
char *sqlite3_vsnprintf(int n, char *zBuf, const char *zFormat, va_list ap){
  StrAccum acc;
  if( n<=0 ) return zBuf;
  sqlite3StrAccumInit(&acc, zBuf, n, 0);
  acc.useMalloc = 0;
  sqlite3VXPrintf(&acc, 0, zFormat, ap);
  return sqlite3StrAccumFinish(&acc);
}
char *sqlite3_snprintf(int n, char *zBuf, const char *zFormat, ...){
  char *z;
  va_list ap;
  va_start(ap,zFormat);
  z = sqlite3_vsnprintf(n, zBuf, zFormat, ap);
  va_end(ap);
  return z;
}

/*
** This is the routine that actually formats the sqlite3_log() message.
** We house it in a separate routine from sqlite3_log() to avoid using
** stack space on small-stack systems when logging is disabled.
**
** sqlite3_log() must render into a static buffer.  It cannot dynamically
** allocate memory because it might be called while the memory allocator
** mutex is held.
*/
/*
** 这个例程实际上是用来格式化sqlite3_log()消息的。
** 我们将它包含在一个来自sqlite3_log()的独立例程中，
** 以避免在禁用日志记录时使用堆栈空间。
** 
** sqlite3_log()必须转化为一个静态缓冲区。
** 它不能动态地分配内存，因为当内存分配器互斥时它会被调用。
*/
static void renderLogMsg(int iErrCode, const char *zFormat, va_list ap){
  StrAccum acc;                          /* String accumulator *//*字符串累加器*/
  char zMsg[SQLITE_PRINT_BUF_SIZE*3];    /* Complete log message *//*完整的日志消息*/

  sqlite3StrAccumInit(&acc, zMsg, sizeof(zMsg), 0);
  acc.useMalloc = 0;
  sqlite3VXPrintf(&acc, 0, zFormat, ap);
  sqlite3GlobalConfig.xLog(sqlite3GlobalConfig.pLogArg, iErrCode,
                           sqlite3StrAccumFinish(&acc));
}

/*
** Format and write a message to the log if logging is enabled.
*//*若启用日志记录，则将格式和消息写入日志*/
void sqlite3_log(int iErrCode, const char *zFormat, ...){
  va_list ap;                             /* Vararg list *//*变量参数列表*/
  if( sqlite3GlobalConfig.xLog ){
    va_start(ap, zFormat);
    renderLogMsg(iErrCode, zFormat, ap);
    va_end(ap);
  }
}

#if defined(SQLITE_DEBUG)
/*
** A version of printf() that understands %lld.  Used for debugging.
** The printf() built into some versions of windows does not understand %lld
** and segfaults if you give it a long long int.
*/
/*
** 一个能够读懂%lld的printf()版本。用于排除故障。
** 内置于某些windows版本中的printf()无法读懂%lld，
** 并且若给它一个很长的长整型整数则会出现段错误。
*/
void sqlite3DebugPrintf(const char *zFormat, ...){
  va_list ap;
  StrAccum acc;
  char zBuf[500];
  sqlite3StrAccumInit(&acc, zBuf, sizeof(zBuf), 0);
  acc.useMalloc = 0;
  va_start(ap,zFormat);
  sqlite3VXPrintf(&acc, 0, zFormat, ap);
  va_end(ap);
  sqlite3StrAccumFinish(&acc);
  fprintf(stdout,"%s", zBuf);
  fflush(stdout);
}
#endif

#ifndef SQLITE_OMIT_TRACE
/*
** variable-arguments wrapper around sqlite3VXPrintf().
*//*装入sqlite3VXPrintf()的可变参数*/
void sqlite3XPrintf(StrAccum *p, const char *zFormat, ...){
  va_list ap;
  va_start(ap,zFormat);
  sqlite3VXPrintf(p, 1, zFormat, ap);
  va_end(ap);
}
#endif
