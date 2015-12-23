/*****************************************************************************
 * 文 件 名  : test_case_select.c
 * 负 责 人  : yankai
 * 创建日期  : 2015年12月21日
 * 功能描述  : 测试用例
 * 其    它  : 
*****************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "sqlite3.h"
#include "test_case_select.h"

char *join(char *a, char *b);
void kHelloSQLite(char *tablename);
void test_API_sqlite3_exec();

int main()
{
    if(RkHelloSQLite){
        kHelloSQLite("info");
    }
    else if(Rtest_minMaxQuery){
        test_minMaxQuery();
    }
    else if(Rtest_API_sqlite3_exec){
        test_API_sqlite3_exec();
    }
    return 0;
}

/*****************************************************************************
 * 数 据 库  : stu.db
 * SQL语 句  : create table info(id integer primary key,name varchar(20));
               insert into info values(1,'yankai');
               insert into info values(2,'Bill');
               insert into info values(3,'Jobs');
               insert into info values(4,'Gates');
 * 创建日期  : 2015年12月21日
 * 执行结果  :  yankai@ubuntu:~/yankai/sqlite3/cases_test$ make
                cc    -c -o test_case_select.o test_case_select.c
                gcc -o main test_case_select.o sqlite3.o -lpthread -ldl
                yankai@ubuntu:~/yankai/sqlite3/cases_test$ ./main
                   1 | yankai    
                   2 | Bill      
                   3 | Jobs      
                   4 | Gates 
*****************************************************************************/
void kHelloSQLite(char tablename[])
{
    int rc,i,ncols;
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char *sql;
    const char *tail;
    
    rc=sqlite3_open("stu.db",&db); //打开数据
    if(rc)
    {
        fprintf(stderr,"Can't open database:%s\n",sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }
    
    sql = join("select * from ",tablename);
    rc=sqlite3_prepare(db,sql,(int)strlen(sql),&stmt,&tail);
    if(rc!=SQLITE_OK)
    {
        fprintf(stderr,"SQL error:%s\n",sqlite3_errmsg(db));
    }
        
    rc=sqlite3_step(stmt);
    ncols=sqlite3_column_count(stmt);
    while(rc==SQLITE_ROW)
    {
        for(i=0;i<ncols;i++)
        {
            if(i==0)
                fprintf(stderr," %3s |",sqlite3_column_text(stmt,i));
            else
                fprintf(stderr," %-10s",sqlite3_column_text(stmt,i));
        }
        fprintf(stderr,"\n");
        rc=sqlite3_step(stmt);
    }

    sqlite3_finalize(stmt);//释放statement
    sqlite3_close(db);//关闭数据库
}

/*****************************************************************************
 *功 能 描 述：利用sqlite3_exec()执行SQL语句
 * 数 据 库  : stu.db
 * SQL语 句  : create table app(id integer primary key,name varchar(20));
               insert app info values(1,'Facebook');
               insert app info values(2,'Twitter');
               insert app info values(3,'YouTube');
               insert app info values(4,'WeChat');
               insert app info values(5,'QQ');
 * 创建日期  : 2015年12月22日
 * 执行结果  :  yankai@ubuntu:~/yankai/sqlite3/cases_test$ make
                cc    -c -o test_case_select.o test_case_select.c
                gcc -o main test_case_select.o sqlite3.o -lpthread -ldl
                yankai@ubuntu:~/yankai/sqlite3/cases_test$ ./main
                   1 | Facebook  
                   2 | Twitter   
                   3 | YouTube   
                   4 | WeChat    
                   5 | QQ
*****************************************************************************/
void test_API_sqlite3_exec()
{
    sqlite3 *db;
    int rc;
    char *kSQL;
    char *errmsg;
    
    kSQL = "create table app(id integer primary key,name varchar(20));insert into app values(1,'Facebook');insert into app values(2,'Twitter');insert into app values(3,'YouTube');insert into app values(4,'WeChat');insert into app values(5,'QQ');";
    rc=sqlite3_open("stu.db",&db); //打开数据
    if(rc)
    {
        fprintf(stderr,"Can't open database:%s\n",sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }
    rc = sqlite3_exec(db,kSQL,0,0,&errmsg);
    if(rc){
        fprintf(stderr,"SQLite3 execute failed:%s\n",sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }
    kHelloSQLite("app");
    sqlite3_close(db);
}


char *join(char *a, char *b) {  
    char *c = (char *) malloc(strlen(a) + strlen(b) + 1); //局部变量，用malloc申请内存  
    if (c == NULL) exit (1);  
    char *tempc = c; //把首地址存下来  
    while (*a != '\0') {  
        *c++ = *a++;  
    }  
    while ((*c++ = *b++) != '\0') {  
        ;  
    }  
    //注意，此时指针c已经指向拼接之后的字符串的结尾'\0' !  
    return tempc;//返回值是局部malloc申请的指针变量，需在函数调用结束后free之
} 