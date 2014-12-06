/*
** 2007 May 7
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
** This file defines various limits of what SQLite can process.  这个文件定义了SQLite能够处理的变量范围(限制)
*/

/*
** The maximum length of a TEXT or BLOB in bytes.  TEXT或者BLOB的最大字节长度   (BLOB (binary large object)，二进制大对象，是一个可以存储二进制文件的容器。在计算机中，BLOB常常是数据库中用来存储二进制文件的字段类型。
                                                                                 BLOB是一个大文件，典型的BLOB是一张图片或一个声音文件，由于它们的尺寸，必须使用特殊的方式来处理（例如：上传、下载或者存放到一个数据库）。)
** This also limits the size of a row in a table or index.   也限制了表或者索引中一行的大小。
**
** The hard limit is the ability of a 32-bit signed integer   硬限制是32位有符号整型的计算能力大小是2^31-1，也就是 2147483647
** to count the size: 2^31-1 or 2147483647.
*/
#ifndef SQLITE_MAX_LENGTH
# define SQLITE_MAX_LENGTH 1000000000
#endif

/*
** This is the maximum number of
**
**    * Columns in a table   视图中的表的最大数
**    * Columns in an index  视图中的索引的最大数
**    * Columns in a view    视图中的列的最大数
**    * Terms in the SET clause of an UPDATE statement   更新语句中SET子句的最大数
**    * Terms in the result set of a SELECT statement    选择语句中结果集的最大数
**    * Terms in the GROUP BY or ORDER BY clauses of a SELECT statement.   选择语句中GROUP BY 或者 ORDER BY子句的最大数
**    * Terms in the VALUES clause of an INSERT statement                  插入语句中VALUES子句的最大数
**
** The hard upper limit here is 32676.  Most database people will       更高级的硬限制是32676.
** tell you that in a well-normalized database, you usually should      大部分数据库人员将会告诉你在一个规范化的数据库中，你通常不应该在任何表中有多于十几列的列数。
** not have more than a dozen or so columns in any table.  And if
** that is the case, there is no point in having more than a few       
** dozen values in any of the other situations described above.         如果是这种情况，在上面所描述的任何其它的情景中(索引、视图等等)中有超过几十的值是毫无意义的
*/
#ifndef SQLITE_MAX_COLUMN
# define SQLITE_MAX_COLUMN 2000
#endif

/*
** The maximum length of a single SQL statement in bytes.   单个SQL语句的最大字节长度
**
** It used to be the case that setting this value to zero would       过去的情况是，如果把值设为0，这个限制就没有了  
** turn the limit off.  That is no longer true.  It is not possible   这个情况不再正确
** to turn this limit off.                                            把这个限制给关了是不可能的。                    
*/
#ifndef SQLITE_MAX_SQL_LENGTH
# define SQLITE_MAX_SQL_LENGTH 1000000000
#endif

/*
** The maximum depth of an expression tree. This is limited to        表达树的最大深度。
** some extent by SQLITE_MAX_SQL_LENGTH. But sometime you might       这是通过在一定程度上限制SQLITE_MAX_SQL_LENGTH。
** want to place more severe limits on the complexity of an           但是，有时候你也许想要对表达树的复杂度施加一个更严格的限制。
** expression.
**
** A value of 0 used to mean that the limit was not enforced.         一个值"0"，用来表达这个限制不是强制性的。
** But that is no longer true.  The limit is now strictly enforced    但这样不再正确了。这个限制现在是严格实施的。
** at all times.
*/
#ifndef SQLITE_MAX_EXPR_DEPTH
# define SQLITE_MAX_EXPR_DEPTH 1000
#endif

/*
** The maximum number of terms in a compound SELECT statement.       复合的选择语句的最大项数。
** The code generator for compound SELECT statements does one        复合的选择语句一级递归的每个项的代码生成器。
** level of recursion for each term.  A stack overflow can result    项数太大会导致堆栈溢出
** if the number of terms is too large.  In practice, most SQL       实际上，大部分的SQL从来没有超过3或者4个项的。
** never has more than 3 or 4 terms.  Use a value of 0 to disable    
** any limit on the number of terms in a compount SELECT.            用"0"来使复合的选择语句的项数限制无效。
*/
#ifndef SQLITE_MAX_COMPOUND_SELECT
# define SQLITE_MAX_COMPOUND_SELECT 500
#endif

/*
** The maximum number of opcodes in a VDBE program.                 虚拟数据库引擎中操作码的最大数。
** Not currently enforced.                                          目前并不是强制的。
*/
#ifndef SQLITE_MAX_VDBE_OP
# define SQLITE_MAX_VDBE_OP 25000
#endif

/*
** The maximum number of arguments to an SQL function.              SQL函数的参数最大数目。
*/
#ifndef SQLITE_MAX_FUNCTION_ARG
# define SQLITE_MAX_FUNCTION_ARG 127
#endif

/*
** The maximum number of in-memory pages to use for the main database     内存页用于主数据表和临时数据表的最大数 
** table and for temporary tables.  The SQLITE_DEFAULT_CACHE_SIZE         
*/
#ifndef SQLITE_DEFAULT_CACHE_SIZE
# define SQLITE_DEFAULT_CACHE_SIZE  2000
#endif
#ifndef SQLITE_DEFAULT_TEMP_CACHE_SIZE
# define SQLITE_DEFAULT_TEMP_CACHE_SIZE  500
#endif

/*
** The default number of frames to accumulate in the log file before       预写式日志模式下，在数据库检查点之前数据库日志文件中积累的默认侦数   
** checkpointing the database in WAL mode.                                 WLA:Write-Ahead Logging预写式日志
*/
#ifndef SQLITE_DEFAULT_WAL_AUTOCHECKPOINT
# define SQLITE_DEFAULT_WAL_AUTOCHECKPOINT  1000
#endif

/*
** The maximum number of attached databases.  This must be between 0      添加数据库的最大数。   必须在0和62之间。
** and 62.  The upper bound on 62 is because a 64-bit integer bitmap      上界是62是因为64位的整型位图利用内存来追踪添加数据库。
** is used internally to track attached databases.  
*/
#ifndef SQLITE_MAX_ATTACHED
# define SQLITE_MAX_ATTACHED 10
#endif


/*
** The maximum value of a ?nnn wildcard that the parser will accept.      解析器能接受的通配符的最大值
*/
#ifndef SQLITE_MAX_VARIABLE_NUMBER
# define SQLITE_MAX_VARIABLE_NUMBER 999
#endif

/* Maximum page size.  The upper bound on this value is 65536.  This a limit    最大页面尺寸。    值得上界是65536。
** imposed by the use of 16-bit offsets within each page.                       通过利用每一页的16位偏移量来强加的限制。
**
** Earlier versions of SQLite allowed the user to change this value at          早期版本的SQLite允许使用者在编译时改变这个值。
** compile time. This is no longer permitted, on the grounds that it creates    现在不在允许了，由于他创建的库与限制上不同的SQLite库的编译技术上是矛盾的
** a library that is technically incompatible with an SQLite library 
** compiled with a different limit. If a process operating on a database        如果一个程序在页面尺寸为65536字节的数据库上的运行失败了，
** with a page-size of 65536 bytes crashes, then an instance of SQLite          然后，SQLite与默认页面大小限制编的一个实例将无法回滚中止交易。
** compiled with the default page-size limit will not be able to rollback 
** the aborted transaction. This could lead to database corruption.             这有可能会导致数据库损坏。   
*/
#ifdef SQLITE_MAX_PAGE_SIZE
# undef SQLITE_MAX_PAGE_SIZE
#endif
#define SQLITE_MAX_PAGE_SIZE 65536


/*
** The default size of a database page.         数据库页的默认大小
*/
#ifndef SQLITE_DEFAULT_PAGE_SIZE
# define SQLITE_DEFAULT_PAGE_SIZE 1024
#endif
#if SQLITE_DEFAULT_PAGE_SIZE>SQLITE_MAX_PAGE_SIZE
# undef SQLITE_DEFAULT_PAGE_SIZE
# define SQLITE_DEFAULT_PAGE_SIZE SQLITE_MAX_PAGE_SIZE
#endif

/*
** Ordinarily, if no value is explicitly provided, SQLite creates databases      通常，如果值没有明文规定，SQLite将设置数据库页的大小为SQLITE_DEFAULT_PAGE_SIZE
** with page size SQLITE_DEFAULT_PAGE_SIZE. However, based on certain            然而，基于一定的设备特性（扇区大小和原子函数write()支持），SQLite可能会选择一个较大的价值。
** device characteristics (sector-size and atomic write() support),
** SQLite may choose a larger value. This constant is the maximum value          此常数的最大值SQLite将让它自行选择。
** SQLite will choose on its own.
*/
#ifndef SQLITE_MAX_DEFAULT_PAGE_SIZE
# define SQLITE_MAX_DEFAULT_PAGE_SIZE 8192
#endif
#if SQLITE_MAX_DEFAULT_PAGE_SIZE>SQLITE_MAX_PAGE_SIZE
# undef SQLITE_MAX_DEFAULT_PAGE_SIZE
# define SQLITE_MAX_DEFAULT_PAGE_SIZE SQLITE_MAX_PAGE_SIZE
#endif


/*
** Maximum number of pages in one database file.       数据库文件的最大页数
**
** This is really just the default value for the max_page_count pragma.    这真的只是编译指示所计算出的最大页数这个默认值。
** This value can be lowered (or raised) at run-time using that the        这个值通过宏max_page_count定义可以在运行时变大或变小。
** max_page_count macro.
*/
#ifndef SQLITE_MAX_PAGE_COUNT
# define SQLITE_MAX_PAGE_COUNT 1073741823
#endif

/*
** Maximum length (in bytes) of the pattern in a LIKE or GLOB     LIKE或者GLOB中pattern的最大字节长度
** operator.
*/
#ifndef SQLITE_MAX_LIKE_PATTERN_LENGTH
# define SQLITE_MAX_LIKE_PATTERN_LENGTH 50000
#endif

/*
** Maximum depth of recursion for triggers.     触发器的最大深度递归
**
** A value of 1 means that a trigger program will not be able to itself    值'1'代表触发器程序不能自己激活/出发其它的触发器。
** fire any triggers. A value of 0 means that no trigger programs at all   值'0'代表根本没有触发器程序已经生效/执行
** may be executed.
*/
#ifndef SQLITE_MAX_TRIGGER_DEPTH
# define SQLITE_MAX_TRIGGER_DEPTH 1000
#endif
