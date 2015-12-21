/*****************************************************************************
 * 文 件 名  : Test_case_select.c
 * 负 责 人  : yankai
 * 创建日期  : 2015年12月21日
 * 功能描述  : 测试用例
 * 其    它  : 
*****************************************************************************/
int Test_case_select( test )
{
    #
}


#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "sqlite3.h"

void kHelloSQLite();

int main()
{
    kHelloSQLite();
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
void kHelloSQLite()
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
    
    sql="select * from info";
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
