#include "sqlite3.h"
#include <stdio.h>
int main(int argc, char** argv) {
	char** result;
	int row, colum, ret;
	char* message;
	sqlite3* pdb;

	ret = sqlite3_open("test.db", &pdb);
	//if设置断点debug
	if (ret == SQLITE_OK) {
		sqlite3_get_table(pdb, "select * from student", &result, &row, &colum, &message);
		for (int i = 0; i <= row; i++) {
			for (int j = 0; j < colum; j++) {
				printf("%s|", *(result + colum * i + j));
			}
			printf("\n");
		}
		sqlite3_close(pdb);
	}
	getchar();
	return 0;
}
