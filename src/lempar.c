/*
  Lemon的主要功能就是根据上下文无关文法(CFG)，生成支持该文法的分析器。程序的输入文件有两个：
　　(1) 语法规则文件；
　　(2) 分析器模板文件。
  本文件即为分析器模板文件。

  重要概念：
    词法分析器（Tokenizer）
    当执行一个包含SQL语句的字符串时，接口程序要把这个字符串传递给tokenizer。
	Tokenizer的任务是把原有字符串分割成一个个标识符（token），
	并把这些标识符传递给解析器。Tokenizer是用手工编写的，在C文件tokenize.c中。
    在这个设计中需要注意的一点是，tokenizer调用parser。
	熟悉YACC和BISON的人们也许会习惯于用parser调用tokenizer。SQLite的作者已经尝试了这两种方法，并发现用tokenizer调用parser会使程序运行的更好。YACC会使程序更滞后一些。


    公共接口（Interface）
	SQLite库的大部分公共接口由main.c, legacy.c和vdbeapi.c源文件中的函数来实现，
	这些函数依赖于分散在其他文件中的一些程序，因为在这些文件中它们可以访问有文件作用域的数据结构。sqlite3_get_table()例程在table.c中实现，sqlite3_mprintf()可在printf.c中找到，
	sqlite3_complete()则位于tokenize.c中。Tcl接口在tclsqlite.c中实现。SQLite的C接口信息可参考http://sqlite.org/capi3ref.html。

	语法分析器（Parser）
    语法分析器的工作是在指定的上下文中赋予标识符具体的含义。
	SQLite的语法分析器使用Lemon LALR(1)分析程序生成器来产生，Lemon做的工作与YACC/BISON相同，但它使用不同的输入句法，
	这种句法更不易出错。Lemon还产生可重入的并且线程安全的语法分析器。Lemon定义了非终结析构器的概念，当遇到语法错误时它不会泄露内存。驱动Lemon的源文件可在parse.y中找到。
    因为lemon是一个在开发机器上不常见的程序，所以lemon的源代码（只是一个C文件）被放在SQLite的"tool"子目录下。 lemon的文档放在"doc"子目录下。
***/

Driver template for the LEMON parser generator.LEMON语法解析器的驱动模板
** The author disclaims copyright to this source code.
**
** This version of "lempar.c" is modified, slightly, for use by SQLite.
** The only modifications are the addition of a couple of NEVER()
** macros to disable tests that are needed in the case of a general
** LALR(1) grammar but which are always false in the
** specific grammar used by SQLite.
*/
/* First off, code is included that follows the "include" declaration
** in the input grammar file. */
#include <stdio.h>

/* Next is all token values, in a form suitable for use by makeheaders.
** This section will be null unless lemon is run with the -m switch.

   下面给出的是所有终结符的整数值，它们能被makeheaders()程序所运用。
   如果在lemon程序运行时不加“-m”选项，这一段终结符的代码就为空（不会出现）。
*/
/*
** These constants (all generated automatically by the parser generator)
** specify the various kinds of tokens (terminals) that the parser
** understands.
**
** Each symbol here is a terminal symbol in the grammar.

   这些表示终结符的常数，最终生成好的语法分析器能完全理解它们的意思，并且这里的每个符号都是语法文件的终结符。
*/
%%
/* Make sure the INTERFACE macro is defined.确保定义了INTERFACE的宏
*/
#ifndef INTERFACE
# define INTERFACE 1
#endif

/* The next thing included is series of defines which control
** various aspects of the generated parser.

    下面就是一系列的定义，这些定义在多个方面影响和控制着生成器的解析。

	 这里，是把一些常见的整数类型，像unsigned char,int，改成另外一个类型名字。
	 比如YYCODETYPE和YYACTIONTYPE.但对它们的改动并不是无条件的。改之前需要了解语法文件中的终结符的数量。
	 如果少于250个，YYCODETYPE和YYACTIONTYPE就是unsigned char类型，
	 如果多于250个，则YYCODETYPE和YYACTIONTYPE就是unsigned short int类型。
	 一般来说,YYCODETYPE和YYACTIONTYPE总是unsigned char，因为一个语法文件中很少有终结符数量超过250个的。

**    YYCODETYPE         is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 terminals
**                       and nonterminals.  "int" is used otherwise.
**    YYNOCODE           is a number of type YYCODETYPE which corresponds
**                       to no legal terminal or nonterminal number.  This
**                       number is used to fill in empty slots of the hash
**                       table.Parser
      YYNOCODE的值为YYCODETYPE类型（即某一类整型）中的一个常量，其数值为文法符号的所有数量再加上1，表示一个非法的终结符或非法的非终结符。将来，它
	                  会被用于填充在动作数组中的空白位置上。
**    YYFALLBACK         If defined, this indicates that one or more tokens
**                       have fall-back values which should be used if the
**                       original value of the token will not parse.
**    YYACTIONTYPE       is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 rules and
**                       states combined.  "int" is used otherwise.
**    ParseTOKENTYPE     is the data type used for minor tokens given
**                       directly to the parser from the tokenizer.此处是一个‘全局‘变量，它指定了终结符数据类型的名字。词法分析器
**    ParseTOKENTYPE    用于词法分析器分析的较小标识符（token）的数据类型

**                       which is ParseTOKENTYPE.  The entry in the union
**                       for base tokens is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.  If
**                       zero the stack is dynamically sized using realloc()

**    ParseARG_SDECL     A static variable declaration for the %extra_argument
**    ParseARG_PDECL     A parameter declaration for the %extra_argument
**    ParseARG_STORE     Code to store %extra_argument into yypParser
**    ParseARG_FETCH     Code to extract %extra_argument from yypParser
**    YYNSTATE           the combined number of states.//它的值是语法分析器的状态数量
**    YYNRULE            the number of rules in the grammar//它的值是语法文件中产生式的数量
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.  //预先定义的错误符号（即error符号）,这是一个表示它序号的数值。
*/
%%
#define YY_NO_ACTION      (YYNSTATE+YYNRULE+2) //(无动作)定义成一个常数，其大小是状态数量与产生式数量之和再加2
#define YY_ACCEPT_ACTION  (YYNSTATE+YYNRULE+1)//（接受）也定义成一个常数，其大小是状态数量与产生式数量之和再加1
#define YY_ERROR_ACTION   (YYNSTATE+YYNRULE)//（错误动作）定义成一个常数，期大小是状态数量与产生式数量之和

/* The yyzerominor constant is used to initialize instances of
** YYMINORTYPE objects to zero.
    常量yyzerominor的作用是初始化YYMINORTYPE对象的实例为零。
 */
static const YYMINORTYPE yyzerominor = { 0 };

/* Define the yytestcase() macro to be a no-op if is not already defined
** otherwise.
**
** Applications can choose to define yytestcase() in the %include section
** to a macro that can assist in verifying code coverage.  For production
** code the yytestcase() macro should be turned off.  But it is useful
** for testing.
*/
#ifndef yytestcase
# define yytestcase(X)
#endif


/* Next are the tables used to determine what action to take based on the
** current state and lookahead token.  These tables are used to implement
** functions that take a state number and lookahead value and return an
** action integer.
**
** Suppose the action integer is N.  Then the action is determined as
** follows
**
**   0 <= N < YYNSTATE                  Shift N.  That is, push the lookahead
**                                      token onto the stack and goto state N.
**
**   YYNSTATE <= N < YYNSTATE+YYNRULE   Reduce by rule N-YYNSTATE.
**
**   N == YYNSTATE+YYNRULE              A syntax error has occurred.
**
**   N == YYNSTATE+YYNRULE+1            The parser accepts its input.
**
**   N == YYNSTATE+YYNRULE+2            No such action.  Denotes unused
**                                      slots in the yy_action[] table.
**
** The action table is constructed as a single large table named yy_action[].
** Given state S and lookahead X, the action is computed as
**
**      yy_action[ yy_shift_ofst[S] + X ]
**
** If the index value yy_shift_ofst[S]+X is out of range or if the value
** yy_lookahead[yy_shift_ofst[S]+X] is not equal to X or if yy_shift_ofst[S]
** is equal to YY_SHIFT_USE_DFLT, it means that the action is not in the table
** and that yy_default[S] should be used instead.
**
** The formula above is for computing the action when the lookahead is
** a terminal symbol.  If the lookahead is a non-terminal (as occurs after
** a reduce action) then the yy_reduce_ofst[] array is used in place of
** the yy_shift_ofst[] array and YY_REDUCE_USE_DFLT is used in place of
** YY_SHIFT_USE_DFLT.
**
** The following are the tables generated in this section:
**
**  yy_action[]        A single table containing all actions.
**  yy_lookahead[]     A table containing the lookahead for each entry in
**                     yy_action.  Used to detect hash collisions.
**  yy_shift_ofst[]    For each state, the offset into yy_action for
**                     shifting terminals.
**  yy_reduce_ofst[]   For each state, the offset into yy_action for
**                     shifting non-terminals after a reduce.
**  yy_default[]       Default action for each state.

这里我们要生成一系列的数组，并且这些数组的元素只能是整数，这样大大压缩了语法分析器占用的空间和提高了它的工作效率。

我们的目标是建立一个一维的yy_action[]数组。当我们用表示分析器状态（satate）的整数和表示先行符号（lookahead）的整数，向这个数组检索时，它应该返回对应
那个动作（action）,不过此时的动作也用某个整数N来表示。

 假定，从这个yy_action[]动作数组中检索，获得了代表某个动作的整数数值是N，则我们如何判断它是移进、规约、接受还是错误呢？N数值代表的动作行为可由如下的数值区间决定。

    当N具有下列数值范围时，表示的动作是移进：
       0 <= N < YYNSTATE  （这里的YYNSTATE是状态的总数目）
    并且，这时把先行符压入语法分析栈，然后进入状态N。

    当N具有下列的数值范围时，表示的动作是规约：
       YYNSTATE <= N < YYNSTATE+YYNRULE （这里的YYNSTATE是状态的总数目）

    当N具有下列数值时，表示语法反洗过程中发现一个错误：
       N == YYNSTATE+YYNRULE

    当N具有下列数值时，表示语法分析器到达接受状态。语法分析至此完毕，并且没有错误：
       N == YYNSTATE+YYNRULE+1

    当N具有下列数值时，表示这是不可能有的动作。这意味着yy_aciton[]数组中这些元素上没有动作。
      N == YYNSTATE+YYNRULE+2

    由于我们建立的动作表示一维表（编译原理中常为二维表，一维表示状态，另一维表示先行符合非终结符的产生式），当得到一个状态s和一个先行符x时，可以用如下公式计算，从数组中找到进行的动作N；
      yy_action[ yy_shift_ofst[S] + X ]
         关于上式的解释：由yy_shift_ofst[S]这个公式，通过S在yy-shift_ofst[]数组中进行搜索，可以得到S的状态所有的那一行偏离yys_aciton[]数组的绝对位置（改绝对位置为整数）。
     X先行符（在数组中也是一个整数），是符号的序号，同上述绝对位置相加得到的整数，就是yy_aciton数组某个元素的索引值。

    如果用yy_shift_ofst[S]+X计算得出的索引值大于yy_action[]数组的长度，说明这个状态S中不可能含有这样的X，所以不可能有动作。
    如果我们用yy_lookahead[yy_shift_ofst[S]+X](yy_lookahead是一个数组，可以用来判断某个X的先行符是不是与某个S状态相匹配)
	计算出来的值不等于X本身的值，说明此X先行符被错误使用了，它与这个S状态不匹配，也就不会被接纳进语法分析栈中。

    当yy_shift_ofst[S]的值等于YY_SHIFT_USE_DFLT默认动作时，但是这时的动作都不含在动作表中，此时调用的是yy_default[S]中指定的动作。

    当先行符时非终结符时，则应该运用yy_reduce_ofst[]数组，而不再使用yy_shift_ofst[],同时也应该用YY_REDUCE_USER_DFLT代替YY_SHIFT_USE_DFLT


   下面是对各个数组的简单描述，这些数组在代码运行的过程中自动创建：

    yy_action[]      包含所有动作的一维数组

    yy_lookahead[]   包含了所有合法的克星的先行符的一维数组

    yy_shift_ofst[]  一维数组。对于每一个状态S，当移进一个终结符X时，用于计算该状态在yy_action[]数组中的绝对位置。

    yy_reduce_ofst[] 一位数组。对于每一个状态S，当一个归约动作后并移进该非终结符X时，用于计算该状态在yy-action[]数组中的绝对位置。

    yy_default[]      一维数组。每一个状态S中的默认（Default）动作。
*/


/* The next table maps tokens into fallback tokens.  If a construct
** like the following:
**
**      %fallback ID X Y Z.
**
** appears in the grammar, then ID becomes a fallback token for X, Y,
** and Z.  Whenever one of the tokens X, Y, or Z is input to the parser
** but it does not parse, the type of the token is changed to ID and
** the parse is retried before an error is thrown.

 如果语法文件中出现 %fallback ID X Y Z这样格式的符号，ID就是X、Y、Z的标识符（Identifier）.%fallback是特殊申明符，说明
 它指定的这些符号如X、Y、Z（这里X、Y、Z指的是终结符即token）可以‘回滚’，即可以恢复它的一般标识符身份ID。

 原理解释：在语法文件中，有些特殊终结符，既可以作为终结符（Token）,也可以作为一般的标识符（ID），判断条件如下：
           当它们在语法分析时不能被解释成终结符时，则被作为标识符处理。
		   而一般情况下，不能作为终结符处理的对认为是错误的符号，按错误对待。
*/
#ifdef YYFALLBACK
static const YYCODETYPE yyFallback[] = {
%%
};//数组yyFallback[]存放ID终结符的代表值，可利用其检索对应的终结符。
#endif /* YYFALLBACK */

/* The following structure represents a single element of the
** parser's stack.  Information stored includes:
**
**   +  The state number for the parser at this level of the stack.
**
**   +  The value of the token stored at this level of the stack.
**      (In other words, the "major" token.)
**
**   +  The semantic value stored at this level of the stack.  This is
**      the information used by the action routines in the grammar.
**      It is sometimes called the "minor" token.

        yyStackEntry是进行一次语法分析的基本单位，即‘入栈’符号。由这个基本单位的实例形成了语法分析栈中的一个节点。
*/
struct yyStackEntry {
  YYACTIONTYPE stateno;  /* The state-number */ //状态（int类型），即状态的序号
  YYCODETYPE major;      /* The major token value.  This is the code
                         ** number for the token at this stack level *///符号（int类型），它实际上是语法符号的序号。
  YYMINORTYPE minor;     /* The user-supplied minor token value.  This
                         ** is the value of the token  *///符号的值（YYMINORTYPE类型），它实际上是实际的运用程序中为该文法符号赋予的值，即该符号的属性。
};
typedef struct yyStackEntry yyStackEntry;

/* The state of the parser is completely contained in an instance of
** the following structure
   下面的yyParser结构就构成了语法分析器中的最重要的工作场所，即语法分析栈
*/

struct yyParser {
  int yyidx;                    /* Index of top element in stack 记录栈顶的位置，也就是栈顶元素在yyStack[]数组中的索引*/
#ifdef YYTRACKMAXSTACKDEPTH
  int yyidxMax;                 /* Maximum value of yyidx yyidx的最大值，即yyStack[]数组的总长度-1*/
#endif
  int yyerrcnt;                 /* Shifts left before out of the error 当出现错误时，从栈内抛出一些符号后，才出现在栈顶的符号*/
  ParseARG_SDECL                /* A place to hold %extra_argument 这是一个宏定义， 当语法分析过程中，另有别的参数时，就用它来传递那个参数*/
#if YYSTACKDEPTH<=0
  int yystksz;                  /* Current side of the stack */
  yyStackEntry *yystack;        /* The parser's stack 这里是yyStack[]数组，是将来存放入栈符号的场所，其元素的数据类型为yystackEntry结构 */
#else
  yyStackEntry yystack[YYSTACKDEPTH];  /* The parser's stack */
#endif
};
typedef struct yyParser yyParser;

#ifndef NDEBUG
#include <stdio.h>
static FILE *yyTraceFILE = 0;
static char *yyTracePrompt = 0;
#endif /* NDEBUG */

#ifndef NDEBUG
/*
** Turn parser tracing on by giving a stream to which to write the trace
** and a prompt to preface each trace message.  Tracing is turned off
** by making either argument NULL
**
** Inputs:
** <ul>
** <li> A FILE* to which trace output should be written.
**      If NULL, then tracing is turned off.该函数第一个参数，即要写入的文件，如果为空，则关闭追踪记录。
** <li> A prefix string written at the beginning of every
**      line of trace output.  If NULL, then tracing is
**      turned off.该函数第二个参数时一个字符串，当在向文件（第一个参数决定）写入时，
  它在每一行内容的前部都加上这个字符串，以作为某次追踪记录的标记。
** </ul>
**
** Outputs:
** None.


 


*/
void ParseTrace(FILE *TraceFILE, char *zTracePrompt){
  yyTraceFILE = TraceFILE;//将送进来的TraceFILE引用到yyTraceFILE指针上
  yyTracePrompt = zTracePrompt;//把zTracePrompt引用到yyTracePrompt字符串指针上
  if( yyTraceFILE==0 ) yyTracePrompt = 0;       /** 条件判断，判断yyTraceFILE和yyTracePrompt是否为空指针，
                                                   两者之间 只要其中之一为空指针，就把另外一个置为空指针**/
  else if( yyTracePrompt==0 ) yyTraceFILE = 0;
}
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing shifts, the names of all terminals and nonterminals
** are required.  The following table supplies these names 为了跟踪移进动作，需要用到所有终结符和非终结符的名称，下面的yyTokenName[]数组提供了这些名称*/
static const char *const yyTokenName[] = {
%%
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
   为了追踪规约动作的记录，需要所有规则的名称
*/
static const char *const yyRuleName[] = {
%%
};
#endif /* NDEBUG */


#if YYSTACKDEPTH<=0
/*
** Try to increase the size of the parser stack.
   增加语法解析栈的容量
*/
static void yyGrowStack(yyParser *p){
  int newSize;
  yyStackEntry *pNew;

  newSize = p->yystksz*2 + 100;
  pNew = realloc(p->yystack, newSize*sizeof(pNew[0]));
  if( pNew ){
    p->yystack = pNew;
    p->yystksz = newSize;
#ifndef NDEBUG
    if( yyTraceFILE ){
      fprintf(yyTraceFILE,"%sStack grows to %d entries!\n",
              yyTracePrompt, p->yystksz);
    }
#endif
  }
}
#endif

/*
** This function allocates a new parser.
** The only argument is a pointer to a function which works like
** malloc.
**
** Inputs:
** A pointer to the function used to allocate memory.
** ParseAlloc函数的参数，也是一个函数，主要用来申请内存空间，而且该函数还具有自身的参数——size_t
** Outputs:
** A pointer to a parser.  This pointer is used in subsequent calls
** to Parse and ParseFree.



	此函数分配一个新的解析器。它的唯一参数是一个指针，指向一个函数，它的工作原理就像 Malloc一样
	输入：
	一个指针，用来分配内存的功能。
	输出：
	一个指针，指向一个解析器。该指示器用于后续调用解析和ParseFree。


	程序在使用Lemon生成的分析器之前，必须创建一个分析器。如下：
	void *pParser = ParseAlloc( malloc );
	ParseAlloc为分析器分配空间，然后初始化它，返回一个指向分析器的指针。SQLite对应的函数为：
	void *sqlite3ParserAlloc(void *(*mallocProc)(size_t))
	函数的参数为一个函数指针，并在函数内调用该指针指向的函数。

	在分析器调用ParseAlloc后，分词器就可以将切分的词传递给Parse(Lemon生成的分析器的核心例程)

    用于建立进行语法分析的语法分析栈。


*/
void *ParseAlloc(void *(*mallocProc)(size_t)){
  yyParser *pParser;
  pParser = (yyParser*)(*mallocProc)( (size_t)sizeof(yyParser) );/**指针变量pParser，类型为yyParser，该类型是语法分析器中的堆栈，
                                                                    当使用mallocPro()函数，向内存申请空间成功时，把还没有任何元素的栈顶的序号写成-1。
																	然后返回这个生成好的语法分析栈pParser的指针。
															        **/
  if( pParser ){
    pParser->yyidx = -1;
#ifdef YYTRACKMAXSTACKDEPTH
    pParser->yyidxMax = 0;
#endif
#if YYSTACKDEPTH<=0
    pParser->yystack = NULL;
    pParser->yystksz = 0;
    yyGrowStack(pParser);
#endif
  }
  return pParser;
}

/* The following function deletes the value associated with a
	** symbol.  The symbol can be either a terminal or nonterminal.
	** "yymajor" is the symbol code, and "yypminor" is a pointer to
	** the value.
	函数废除了一个符号的两个值（废除即收回所占用的内存空间）。废弃的符号符号可以是一个终结符或非终结符号。
	yymajor”是符号的代码，并且“yypminor”是指向的值。


*/
static void yy_destructor(
  yyParser *yypParser,    /* The parser */
  YYCODETYPE yymajor,     /* Type code for object to destroy */
  YYMINORTYPE *yypminor   /* The object to be destroyed */
){
  ParseARG_FETCH;
  switch( yymajor ){
    /* Here is inserted the actions which take place when a
    ** terminal or non-terminal is destroyed.  This can happen
    ** when the symbol is popped from the stack during a
    ** reduce or during error processing or when a parser is
    ** being destroyed before it is finished parsing.
    **
    ** Note: during a reduce, the only symbols destroyed are those
    ** which appear on the RHS of the rule, but which are not used
    ** inside the C code.
    */
%%
    default:  break;   /* If no destructor action specified: do nothing */
  }
}

/*
** Pop the parser's stack once.
**
** If there is a destructor routine associated with the token which
** is popped from the stack, then call it.
**
** Return the major token number for the symbol popped.
  如果一个带有废弃代码的符号从语法分析栈中弹出时，就调用这个yy_pop_parser_stack（）函数。
  该函数参数为yyParser类型的pParser(语法分析栈)，
  函数返回值是这个符号的major(大记号)。
	弹出解析器的堆栈一次。如果没有与程序相关的析构函数，从堆栈中弹出，然后调用它。
	输出：
	返回主程序数量。

*/
static int yy_pop_parser_stack(yyParser *pParser){
  YYCODETYPE yymajor;//局部变量，YYCODETYPE类型的yymajor
  yyStackEntry *yytos = &pParser->yystack[pParser->yyidx];//局部变量，yyStackEntry类型的yytos.yytos从组成堆栈的yystack数组上，取得第yyidx位置上的元素，这个元素也就是栈顶元素（yyidx总是站定元素的索引）

  /* There is no mechanism by which the parser stack can be popped below
  ** empty in SQLite.  */
  if( NEVER(pParser->yyidx<0) ) return 0;//判断语句，若栈内没有任何元素，没有可进行的操作，直接返回0
#ifndef NDEBUG
  if( yyTraceFILE && pParser->yyidx>=0 ){//当记录操作过程的yyTraceFILE文件已经打开，并且判断语句条件成立，则向跟踪文件yyTraceFILE中写入一些有关此事弹出栈顶元素操作的信息
    fprintf(yyTraceFILE,"%sPopping %s\n",//用fprintf（）向跟踪文件yyTraceFILE打印
      yyTracePrompt,//打印的的前一个信息是：用于跟踪所用的yyTracePrompt前缀字符串
      yyTokenName[yytos->major]);//打印的后一个则是弹出的符号，通过yytos->major（符号的代号，即序号），从数组yyTokenName【】中检索得到
  }
#endif
  yymajor = yytos->major;
  yy_destructor(pParser, yymajor, &yytos->minor);//调用yy_destructor（）函数来废弃这个符号的相关操作，参数为这个符号的大记号（yymajor）和小记号（&yytos->minor）
  pParser->yyidx--;//在堆栈中退一步，完成符号退栈操作
  return yymajor;//最后返回应该弹出语法分析栈符号的大记号（符号的序号）。
}

/*
** Deallocate and destroy a parser.  Destructors are all called for
** all stack elements before shutting the parser down.
**
** Inputs:
** <ul>
** <li>  A pointer to the parser.  This should be a pointer
**       obtained from ParseAlloc.
** <li>  A pointer to a function used to reclaim memory obtained
**       from malloc.
** </ul>


   函数作用：当程序不再使用分析器时，应该回收为其分配的内存，废弃整个语法分析栈，在废弃同时，调用yy_destructor()函数，先废除掉堆栈中的元素。
   函数参数：指向被废弃语法分析栈的指针p
            用于释放占用内存的函数指针freeProc()
*/
void ParseFree(
  void *p,                    /* The parser to be deleted 指向被废弃语法分析栈的指针*/
  void (*freeProc)(void*)     /* Function used to reclaim memory  用于释放占用内存的函数指针freeProc()*/
){
  yyParser *pParser = (yyParser*)p;//把void的p指针强制转换为yyParser类型的pParser
  /* In SQLite, we never try to destroy a parser that was not successfully
  ** created in the first place. */
  if( NEVER(pParser==0) ) return;//检测语法分析栈pParser是否为空指针，若为空，直接返回。
  while( pParser->yyidx>=0 ) yy_pop_parser_stack(pParser);//若栈内还有剩余元素，用yy_pop_parser_stack依次弹出，直至为空。
#if YYSTACKDEPTH<=0
  free(pParser->yystack);//释放分析栈占用的内存。
#endif
  (*freeProc)((void*)pParser);
}

/*
** Return the peak depth of the stack for a parser.
   返回分析器堆栈的最大深度。
*/
#ifdef YYTRACKMAXSTACKDEPTH

int ParseStackPeak(void *p){
  yyParser *pParser = (yyParser*)p;
  return pParser->yyidxMax;
}
#endif

/*
	** Find the appropriate action for a parser given the terminal
	** look-ahead token iLookAhead.
	**
	** If the look-ahead token is YYNOCODE, then check to see if the action is
	** independent of the look-ahead.  If it is, return the action, otherwise
	** return YY_NO_ACTION.


	函数作用：根据某个先行符（终结符），判断是否应该是一个移进操作（移进即把当前的这个先行符压入分析栈的顶端）
	函数参数： 分析栈pParser
	           先行符iLookAhead
    返回值：伴随先行符iLookAhead将要移进的状态号stateno


*/
static int yy_find_shift_action(
  yyParser *pParser,        /* The parser */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
  int stateno = pParser->yystack[pParser->yyidx].stateno;//取栈顶单元的状态号stateno,将其值赋给同名的临时变量，并用satateno作为索引，检索数组yystatkc[]中对应的值(也就是i的值)。

  if( stateno>YY_SHIFT_COUNT
   || (i = yy_shift_ofst[stateno])==YY_SHIFT_USE_DFLT ){
    return yy_default[stateno];//i的值与YY_SHIFT_USE_DFLT相等，返回此状态中的默认动作
  }
  assert( iLookAhead!=YYNOCODE );//判断搜索的先行符是否非法
  i += iLookAhead; //把i的值加上先行符的值，得出的和作为索引，可以在数组yystack[]中找出语法分析器对应这个先行符的动作
  if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){//i的值超出了yystack[]数组的左右边界
    if( iLookAhead>0 ){
#ifdef YYFALLBACK
      YYCODETYPE iFallback;            /* Fallback token 回滚*/
      if( iLookAhead<sizeof(yyFallback)/sizeof(yyFallback[0])//判断先行符的大小是否在yyFallback数组的长度内，以及它是否就是一个可以回滚的符号
             && (iFallback = yyFallback[iLookAhead])!=0 ){
#ifndef NDEBUG
        if( yyTraceFILE ){//为跟踪文件设置的判断条件
          fprintf(yyTraceFILE, "%sFALLBACK %s => %s\n",
             yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[iFallback]);
        }
#endif
        return yy_find_shift_action(pParser, iFallback);
      }
#endif
#ifdef YYWILDCARD
      {
        int j = i - iLookAhead + YYWILDCARD;
        if(
#if YY_SHIFT_MIN+YYWILDCARD<0
          j>=0 &&
#endif
#if YY_SHIFT_MAX+YYWILDCARD>=YY_ACTTAB_COUNT
          j<YY_ACTTAB_COUNT &&
#endif
          yy_lookahead[j]==YYWILDCARD
        ){
#ifndef NDEBUG
          if( yyTraceFILE ){
            fprintf(yyTraceFILE, "%sWILDCARD %s => %s\n",
               yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[YYWILDCARD]);
          }
#endif /* NDEBUG */
          return yy_action[j];
        }
      }
#endif /* YYWILDCARD */
    }
    return yy_default[stateno];
  }else{
    return yy_action[i];
  }
}

/*
** Find the appropriate action for a parser given the non-terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.

   查找给定非终结先行符的解析器动作。
   如果先行符是YYNOCODE，然后检查，看该动作是否独立于先行符，如果是，则返回该动作，否则回报YY_NO_ACTION

  函数作用：用于寻找表示归约动作的代号
  输入参数：当前状态序号
            非终结符的先行符
  返回值：

*/
static int yy_find_reduce_action(
  int stateno,              /* Current state number 当前状态序号*/
  YYCODETYPE iLookAhead     /* The look-ahead token 非终结符的先行符*/
){
  int i;
#ifdef YYERRORSYMBOL
  if( stateno>YY_REDUCE_COUNT ){
    return yy_default[stateno];
  }
#else
  assert( stateno<=YY_REDUCE_COUNT );
#endif
  i = yy_reduce_ofst[stateno];
  assert( i!=YY_REDUCE_USE_DFLT );
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
#ifdef YYERRORSYMBOL
  if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
    return yy_default[stateno];
  }
#else
  assert( i>=0 && i<YY_ACTTAB_COUNT );
  assert( yy_lookahead[i]==iLookAhead );
#endif
  return yy_action[i];
}

/*
** The following routine is called if the stack overflows.

  如果堆栈溢出函数将被调用。
*/
static void yyStackOverflow(yyParser *yypParser, YYMINORTYPE *yypMinor){
   ParseARG_FETCH;
   yypParser->yyidx--;
#ifndef NDEBUG
   if( yyTraceFILE ){
     fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
   }
#endif
   while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
   /* Here code is inserted which will execute if the parser
   ** stack every overflows */
%%
   ParseARG_STORE; /* Suppress warning about unused %extra_argument var */
}

/*
** Perform a shift action.
   函数作用：执行一个移进操作
   函数参数：语法分析栈
             将被移进堆栈的新状态（状态号）
			 将被移进堆栈符号的大记号yyMajor
			 将被移进堆栈符号的小记号yypMinor

*/
static void yy_shift(
  yyParser *yypParser,          /* The parser to be shifted 将要执行移进操作的语法解析器*/
  int yyNewState,               /* The new state to shift in  将被移进堆栈的新状态（状态号）*/
  int yyMajor,                  /* The major token to shift in 将被移进堆栈符号的大记号yyMajor*/
  YYMINORTYPE *yypMinor         /* Pointer to the minor token to shift in  将被移进堆栈符号的小记号yypMinor*/
){
  /**
    *移进一个符号，实质是移进一个语法分析的基本单元（即具有yyStackEntry类型的变量）
  **/
  yyStackEntry *yytos;//申明语法分析的基本单元变量
  yypParser->yyidx++;//让语法分析栈的yyidx加1，为新进栈的新元素转备好空位
#ifdef YYTRACKMAXSTACKDEPTH
  if( yypParser->yyidx>yypParser->yyidxMax ){//对堆栈是否具备足够空间进行检测判断
    yypParser->yyidxMax = yypParser->yyidx;
  }
#endif
#if YYSTACKDEPTH>0
  if( yypParser->yyidx>=YYSTACKDEPTH ){
    yyStackOverflow(yypParser, yypMinor);//超过堆栈最大深度，堆栈溢出
    return;
  }
#else
  if( yypParser->yyidx>=yypParser->yystksz ){
    yyGrowStack(yypParser);
    if( yypParser->yyidx>=yypParser->yystksz ){
      yyStackOverflow(yypParser, yypMinor);
      return;
    }
  }
#endif
  yytos = &yypParser->yystack[yypParser->yyidx];
  yytos->stateno = (YYACTIONTYPE)yyNewState;
  yytos->major = (YYCODETYPE)yyMajor;
  yytos->minor = *yypMinor;
#ifndef NDEBUG
  if( yyTraceFILE && yypParser->yyidx>0 ){
    int i;
    fprintf(yyTraceFILE,"%sShift %d\n",yyTracePrompt,yyNewState);
    fprintf(yyTraceFILE,"%sStack:",yyTracePrompt);
    for(i=1; i<=yypParser->yyidx; i++)
      fprintf(yyTraceFILE," %s",yyTokenName[yypParser->yystack[i].major]);
    fprintf(yyTraceFILE,"\n");
  }
#endif
}

/* The following table contains information about every rule that
** is used during the reduce.
   下面的结构体包含了在规约过程中每个规则的信息。
*/
static const struct {
  YYCODETYPE lhs;         /* Symbol on the left-hand side of the rule */
  unsigned char nrhs;     /* Number of right-hand side symbols in the rule */
} yyRuleInfo[] = {
%%
};

static void yy_accept(yyParser*);  /* Forward Declaration */

/*
** Perform a reduce action and the shift that must immediately
** follow the reduce.
  函数作用：运行过程中执行两个动作：一个是归约动作，另一个是归约结束后立即进行的移进动作
  函数参数：语法分析栈
            进行归约的产生式序号
*/
static void yy_reduce(
  yyParser *yypParser,         /* The parser 语法分析栈*/
  int yyruleno                 /* Number of the rule by which to reduce 进行归约的产生式序号 */
){
  int yygoto;                     /* The next state 用于存放下一个新的状态号*/
  int yyact;                      /* The next action 用于存放下一个将要执行的动作*/
  YYMINORTYPE yygotominor;        /* The LHS of the rule reduced  归约产生式左边符号的小记号*/
  yyStackEntry *yymsp;            /* The top of the parser's stack 指向语法分析栈的栈顶元素*/
  int yysize;                     /* Amount to pop the stack 用于记录从堆栈中弹出的元素数量*/
  ParseARG_FETCH;      //宏定义语句，同Parse * pParse = yyParser->pParse;
  yymsp = &yypParser->yystack[yypParser->yyidx];//在语法分析栈取得栈顶元素，引用到局部变量yymsp上
#ifndef NDEBUG
  if( yyTraceFILE && yyruleno>=0
        && yyruleno<(int)(sizeof(yyRuleName)/sizeof(yyRuleName[0])) ){
    fprintf(yyTraceFILE, "%sReduce [%s].\n", yyTracePrompt,
      yyRuleName[yyruleno]);
  }
#endif /* NDEBUG */

  /* Silence complaints from purify about yygotominor being uninitialized
  ** in some cases when it is copied into the stack after the following
  ** switch.  yygotominor is uninitialized when a rule reduces that does
  ** not set the value of its left-hand side nonterminal.  Leaving the
  ** value of the nonterminal uninitialized is utterly harmless as long
  ** as the value is never used.  So really the only thing this code
  ** accomplishes is to quieten purify.
  **
  ** 2007-01-16:  The wireshark project (www.wireshark.org) reports that
  ** without this code, their parser segfaults.  I'm not sure what there
  ** parser is doing to make this happen.  This is the second bug report
  ** from wireshark this week.  Clearly they are stressing Lemon in ways
  ** that it has not been previously stressed...  (SQLite ticket #2172)
  */
  /*memset(&yygotominor, 0, sizeof(yygotominor));*/
  yygotominor = yyzerominor;


  switch( yyruleno ){//switch语句，分别处理各个产生式在归约情况下应该执行的动作
  /* Beginning here are the reduction cases.  A typical example
  ** follows:
  **   case 0:
  **  #line <lineno> <grammarfile>
  **     { ... }           // User supplied code
  **  #line <lineno> <thisfile>
  **     break;
  */
%%
  };
  yygoto = yyRuleInfo[yyruleno].lhs;//yygoto该产生式左边的文法符号（它的序数）
  yysize = yyRuleInfo[yyruleno].nrhs;//yysize该产生式右边文法符号的数量
  yypParser->yyidx -= yysize;//语法分析栈弹出了yysize个元素，即产生式右边的所有符号都退栈了
  yyact = yy_find_reduce_action(yymsp[-yysize].stateno,(YYCODETYPE)yygoto);
  if( yyact < YYNSTATE ){
#ifdef NDEBUG
    /* If we are not debugging and the reduce action popped at least
    ** one element off the stack, then we can push the new element back
    ** onto the stack here, and skip the stack overflow test in yy_shift().
    ** That gives a significant speed improvement. */
    if( yysize ){
      yypParser->yyidx++;
      yymsp -= yysize-1;
      yymsp->stateno = (YYACTIONTYPE)yyact;
      yymsp->major = (YYCODETYPE)yygoto;
      yymsp->minor = yygotominor;
    }else
#endif
    {
      yy_shift(yypParser,yyact,yygoto,&yygotominor);
    }
  }else{//此处进行的接受动作
    assert( yyact == YYNSTATE + YYNRULE + 1 );
    yy_accept(yypParser);//yy_accept函数是把语法分析栈所有剩余的符号进行退栈操作
  }
}

/*
** The following code executes when the parse fails

当解析代码失败时，执行这个函数
函数作用：为处理出错而生成处理代码
*/
#ifndef YYNOERRORRECOVERY
static void yy_parse_failed(
  yyParser *yypParser           /* The parser */
){
  ParseARG_FETCH;//宏定义语句
#ifndef NDEBUG
  if( yyTraceFILE ){//打开跟踪文件yyTraceFILE
    fprintf(yyTraceFILE,"%sFail!\n",yyTracePrompt);//打印“Fail”作为提示语
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);//反复调用yy_pop_parser_stack（）函数，把分析栈中的所有元素都予以退栈处理
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
%%
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}
#endif /* YYNOERRORRECOVERY */

/*
** The following code executes when a syntax error first occurs.
   函数作用：当一个代码发生语法错误时，执行这个函数。
   函数参数：语法分析栈（yypParser）
             出错文法符号的大记号（yymajor）
			 出错文法符号的小记号（yyminor）
*/
static void yy_syntax_error(
  yyParser *yypParser,           /* The parser 语法分析栈*/
  int yymajor,                   /* The major type of the error token  出错文法符号的大记号（yymajor）*/
  YYMINORTYPE yyminor            /* The minor type of the error token 错文法符号的小记号（yyminor） */
){
  ParseARG_FETCH;
#define TOKEN (yyminor.yy0)//宏定义，指定终结符（Token）的数据类型必定是yyminor.yy0
%%
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  yyParser *yypParser           /* The parser */
){
  ParseARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sAccept!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
%%
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "ParseAlloc" which describes the current state of the parser.
** The second argument is the major token number.  The third is
** the minor token.  The fourth optional argument is whatever the
** user wants (and specified in the grammar) and is available for
** use by the action routines.
**
** Inputs:
** <ul>
** <li> A pointer to the parser (an opaque structure.)
** <li> The major token number.
** <li> The minor token number.
** <li> An option argument of a grammar-specified type.
** </ul>
**
** Outputs:
** None.
   语法分析器最重要的函数，作用是从标记流中，每次接收一个符号，然后在函数内自动地对之进行语法分析
   函数参数：1.第一个参数指向了语法分析栈。
             2.第二个参数是符号的大记号yymajor，即此符号的序号
			 3.第三个参数是此符号的小记号yyminor。即此符号的值
			 4.第四个参数是一个可选的参数，由进行语法分析的代码编写者在语法文件中专门指定的。
*/
void Parse(
  void *yyp,                   /* The parser  语法分析栈*/
  int yymajor,                 /* The major token code number符号的大记号，即此符号的序号 */
  ParseTOKENTYPE yyminor       /* The value for the token 此符号的小记号。即此符号的值*/
  ParseARG_PDECL               /* Optional %extra_argument parameter 一个可选的参数，由进行语法分析的代码编写者在语法文件中专门指定的（在对parse.c文件中得知：该宏定义申明了一个具有Parse数据类型的pParse的指针变量）*/
){
  YYMINORTYPE yyminorunion;//YYMINORTYPE类型的局部变量，是为语法分析器所会碰到所有符号的数据类型的一个联合
  int yyact;            /* The parser action. ing类型的局部变量，用于存储语法分析堆栈中可能发生的动作 */
#if !defined(YYERRORSYMBOL) && !defined(YYNOERRORRECOVERY)
  int yyendofinput;     /* True if we are at the end of input. int类型的局部变量，是一个标志量，用来标记是否到达尾部，如果已达到输入符号流的末尾，则它的值为‘真’，其余情况为假(值为0)*/
#endif
#ifdef YYERRORSYMBOL
  int yyerrorhit = 0;   /* True if yymajor has invoked an error. int类型的局部变量，如果碰到错误的yymajor，其值为真，初始值为假（即值为0）*/
#endif
  yyParser *yypParser;  /* The parser 进行语法分析的堆栈*/

  /* (re)initialize the parser, if necessary  函数主体一：对语法分析栈进行初始化*/
  yypParser = (yyParser*)yyp;  //这里yyp（语法分析栈）本是void类型的，强转为yypParse类型，并引用到yypParse变量上面
  if( yypParser->yyidx<0 ){ //检测yypParser语法分析栈是不是刚刚生成的，如果刚生成，yyidx值应为-1，所以此处判断是否小于0
#if YYSTACKDEPTH<=0
    if( yypParser->yystksz <=0 ){
      /*memset(&yyminorunion, 0, sizeof(yyminorunion));*/
      yyminorunion = yyzerominor;
      yyStackOverflow(yypParser, &yyminorunion);
      return;
    }
#endif
    yypParser->yyidx = 0;//由于堆栈刚刚生成创建，还没有语法分析单元，yyidx设定为0
    yypParser->yyerrcnt = -1;//此处的yyerrcnt是记录允许错误的数量的
    yypParser->yystack[0].stateno = 0;//将堆栈中第0个元素的状态（stateno）为第0状态，即语法分析时的初始状态
    yypParser->yystack[0].major = 0;//将第0个堆栈元素的major为0，此处的0即为标志接受的符号“$”
  }
  //经过以上处理，语法分析栈就是启动后的首个状态
  yyminorunion.yy0 = yyminor;//接受当前松紧语法分析栈的yyminor，也就是记号（终结符）的值
#if !defined(YYERRORSYMBOL) && !defined(YYNOERRORRECOVERY)
  yyendofinput = (yymajor==0);//此处意味着语法分析栈还没有达到结束的接受状态
#endif
  ParseARG_STORE;//宏定义语句

#ifndef NDEBUG
  if( yyTraceFILE ){//开启跟踪文件
    fprintf(yyTraceFILE,"%sInput %s\n",yyTracePrompt,yyTokenName[yymajor]);//写进当前送进的yymajor的名字（可以从yyTokenName[]数组中提取此文法符号本身的名字）
  }
#endif
//下面是do-while循环，主要进行与此时送进的终结符相关的一系列语法分析操作
  do{
    yyact = yy_find_shift_action(yypParser,(YYCODETYPE)yymajor);//把语法分析栈yypParser和当前先行符yymajor两者作为参数，调用yy_find_shift_action函数，以便得到对应的动作代码yyact
    if( yyact<YYNSTATE ){ //yyact动作小于YYNSTATE（状态的总数量），此时的操作动作应是一个文法符号的进栈移进操作
      yy_shift(yypParser,yyact,yymajor,&yyminorunion);//调用yy_shift函数，把文法法号移进分析栈
      yypParser->yyerrcnt--;//允许错误数量的计数器，数量减1
      yymajor = YYNOCODE;
    }else if( yyact < YYNSTATE + YYNRULE ){//判断条件成立，则是关于归约动作的操作
      yy_reduce(yypParser,yyact-YYNSTATE);//把yypParser(语法分析栈),yyact-YYNSTATE（相减得到结果，代表此时归约操作的产生式）两个参数传入yy_reduce（）函数，让该函数进行一次归约操作
    }else{//关于出现错误时的动作操作过程
      assert( yyact == YY_ERROR_ACTION );
#ifdef YYERRORSYMBOL
      int yymx;
#endif
#ifndef NDEBUG
      if( yyTraceFILE ){
        fprintf(yyTraceFILE,"%sSyntax Error!\n",yyTracePrompt);
      }
#endif
#ifdef YYERRORSYMBOL
      /* A syntax error has occurred.
      ** The response to an error depends upon whether or not the
      ** grammar defines an error token "ERROR".
      **
      ** This is what we do if the grammar does define ERROR:
      **
      **  * Call the %syntax_error function.
      **
      **  * Begin popping the stack until we enter a state where
      **    it is legal to shift the error symbol, then shift
      **    the error symbol.
      **
      **  * Set the error count to three.
      **
      **  * Begin accepting and shifting new tokens.  No new error
      **    processing will occur until three tokens have been
      **    shifted successfully.
      **  首先，如果语法文件中的产生式中曾经定义过终结符ERROR的话，可以调用%syntax_error特殊声明符定义过的那个语法出错处理程序。
	      其次，从分析栈中不断第弹出语法分析单元，直到我们碰到某一个状态，这个状态能够让表示错误的error文件符号移进栈内。
		  然后，把累计出错计数器置为3
		  最后，开始新符号的移进。代码在连续三个新符号移进语法栈的过程中，即使扔有错误符号，程序也不会调用%syntax_error下面的出错处理程序。
      */
      if( yypParser->yyerrcnt<0 ){//小于0，说明没有发生过输入符号出错，或者，已经出现过输入符号的出错，但已经连续书案例地输入3个以上符号
        yy_syntax_error(yypParser,yymajor,yyminorunion);//调用yy_syntax_error符号，来处理输入错误语法符号的情形。
      }
      yymx = yypParser->yystack[yypParser->yyidx].major;//取栈顶元素的major放置在yymx中
      if( yymx==YYERRORSYMBOL || yyerrorhit ){//将上句中得到的yymx与YYERRORSYMBOL比较，相同，则是表示错误的文法符号
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE,"%sDiscard input token %s\n",
             yyTracePrompt,yyTokenName[yymajor]);
        }
#endif
        yy_destructor(yypParser, (YYCODETYPE)yymajor,&yyminorunion);//执行将符号废弃的操作
        yymajor = YYNOCODE;//将yymajor的值改为YYNOCODE，以便顺利执行do-while循环
      }else{//下面是碰到的符号不是error的情形
         while(
          yypParser->yyidx >= 0 &&//循环判断条件一：分析栈中还有元素
          yymx != YYERRORSYMBOL &&//循环判断条件二：栈顶元素不是错误符号
          (yyact = yy_find_reduce_action(//循环判断条件三：把错误符号YYERRORSYMBOL作为先行符，放到yy_find_reduce_action（）函数中去判断
                        yypParser->yystack[yypParser->yyidx].stateno,
                        YYERRORSYMBOL)) >= YYNSTATE
        ){
          yy_pop_parser_stack(yypParser);
        }
        if( yypParser->yyidx < 0 || yymajor==0 ){//判断条件，表示已到达输入末尾
          yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);//废弃此时输入的大记号yymajor和小记号yyminorunion
          yy_parse_failed(yypParser);//用yy_parse_failed（）处理语法分析栈，表示整个语法分析都失败了
          yymajor = YYNOCODE;
        }else if( yymx!=YYERRORSYMBOL ){
          YYMINORTYPE u2;
          u2.YYERRSYMDT = 0; //YYERRSYMDT(预定义的错误符号类型代号，为一个数值，大小为所有文法符号数量的两倍加1)值设为0。
          yy_shift(yypParser,yyact,YYERRORSYMBOL,&u2);
        }
      }
      yypParser->yyerrcnt = 3;//设置允许出错计数器yyerrcnt的值为3的操作
      yyerrorhit = 1;
#elif defined(YYNOERRORRECOVERY)
      /* If the YYNOERRORRECOVERY macro is defined, then do not attempt to
      ** do any kind of error recovery.  Instead, simply invoke the syntax
      ** error routine and continue going as if nothing had happened.
      **
      ** Applications can set this macro (for example inside %include) if
      ** they intend to abandon the parse upon the first syntax error seen.
      */
      yy_syntax_error(yypParser,yymajor,yyminorunion);
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      yymajor = YYNOCODE;

#else  /* YYERRORSYMBOL is not defined */
      /* This is what we do if the grammar does not define ERROR:
      **
      **  * Report an error message, and throw away the input token.
      **
      **  * If the input token is $, then fail the parse.
      **
      ** As before, subsequent error messages are suppressed until
      ** three input tokens have been successfully shifted.
	     下面的程序是针对没有定义ERROR的情况下而言的。
		 先是报告一个错误，然后夫妻输入的记号。当记号是输入的末端记号“$”时。则这次语法分析是失败的
      */
      if( yypParser->yyerrcnt<=0 ){//yyerrcnt(允许出错的计数器)的值小于等于0,
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yypParser->yyerrcnt = 3;//此处之所以为3，是为了连续三个符号的移进过程汇总，不再响应yy_syntax_error()函数
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);//对当前输入的符号，用yy_destructor（）函数进行废弃处理
      if( yyendofinput ){
        yy_parse_failed(yypParser);
      }
      yymajor = YYNOCODE;
#endif
    }
  }while( yymajor!=YYNOCODE && yypParser->yyidx>=0 );
  return;
}
