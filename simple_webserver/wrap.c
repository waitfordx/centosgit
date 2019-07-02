/*==============================================
* FileName    : wrap.c
* Author      : liming
* Create Time : 2019-06-25
* description : 函数的封装和出错处理 
==============================================*/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>


void send_header(char* type){
	printf("HTTP/1.1 200 ok\n");
	printf("Content-Type: %s; charset=iso-8859-1\n", type);
	printf("Connection: close\n");
	printf("\r\n");
}


void send_error(char* err_num, char* err_discrible, char* err_content){
	send_header("text/html");
	printf("<html>\n");
	printf("<head><title>%s %s</title></head>\n", err_num, err_discrible);
	printf("<body>%s</body>\n", err_content);
	printf("</html>");

	exit(1);
}
