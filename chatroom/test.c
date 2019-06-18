/*==============================================
* FileName    : test.c
* Author      : liming
* Create Time : 2019-06-18
* description : 
==============================================*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int main(){

	char* data = "this is a test data";
	char buf[15];
	int n =	read(STDIN_FILENO, buf, sizeof(buf));
	write(STDOUT_FILENO, buf, n);
	buf[n-1] = '\0';
	printf("your input is %s and has %d byte", buf, n);
	printf("this is the second line %s", data);
	return 0;

}
