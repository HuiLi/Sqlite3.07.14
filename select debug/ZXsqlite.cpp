#include "sqlite3.h"
#include <stdio.h>
int main(int argc, char** argv) {
	char** result;	//查询结果
	int row, colum, ret;	//行，列，打开数据库函数返回值
	char* message;	//错误信息
	sqlite3* pdb;

	ret = sqlite3_open("test.db", &pdb);	//创建数据库连接，返回值SQLITE_OK则表示操作正常
	//if语句设置断点debug
	if (ret == SQLITE_OK) {
		sqlite3_get_table(pdb, "select * from student", &result, &row, &colum, &message);
		for (int i = 0; i <= row; i++) {
			for (int j = 0; j < colum; j++) {
				printf("%s|", *(result + colum * i + j));	//打印查询结果
			}
			printf("\n");
		}
		sqlite3_close(pdb);		//关闭数据库连接
	}
	getchar();
	return 0;
}
