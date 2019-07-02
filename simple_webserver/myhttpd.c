/*==============================================
* FileName    : myhttpd.c
* Author      : liming
* Create Time : 2019-06-25
* description : 实现一个的 简单的 WEB 服务器， 利用 xinetd 相应浏览器请求
==============================================*/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include "wrap.h"

#define LEN 4096


int main(int argc, char* argv[]){

	char line[LEN];
	char method[LEN], path[LEN], protocol[LEN];
	char* file;
	FILE* fp;
	struct stat sbuf;
	int ich;

// 先判断参数，并读取网页文件根目录
	if(argc != 2)
		send_error("404", "address error", "cant find path!");

// 切换工作目录到网页文件所在的地址
	if(chdir(argv[1]) == -1)
		send_error("404", "address error", "change dir error!");

// 读取浏览器请求并解析 GET /hello.c /http/1.1 
	if(fgets(line, LEN, stdin) == NULL)
		send_error("404", "POST error", "Parsing POST error!");
		
	if(sscanf(line, "%[^ ] %[^ ] %[^ ]", method, path, protocol) != 3)
		send_error("404", "parsing error", "sscanf error!");
	
// 读取协议头剩下部分
	while(fgets(line, LEN, stdin) != NULL)
	{
		if(strcmp(line, "\r\n"))
			break;
	}

	if(strcmp(method, "GET") != 0)
		send_error("404", "address error", "method dir error!");
	if(path[0] != '/')
		send_error("404", "path error", "path dir error!");
// 截取 path 中的文件名， 去掉 /, 打开文件
	file = path + 1;
	if(stat(file, &sbuf) < 0)
		send_error("404", "stat error", "stat dir error!");
	fp = fopen(file, "r");
	if(fp == NULL)
		send_error("404", "open error", "open dir error!");
		
// 发送协议头，并按二进制字节流方式读取文件。按文件属性分类：
	char* dot = strrchr(file, '.');
	char* type;

	if(dot == NULL)
			type = "text/plain";
	else if(strcmp(dot, ".html") == 0)
			type = "text/html";
	else if(strcmp(dot, ".jpg") == 0)
			type = "image/jpeg";
	else if(strcmp(dot, ".mp3") == 0)
			type = "audio/mpeg";
	else 
			type = "text/plain; charset=iso-8859-1";


	send_header(type);

	while((ich=getc(fp)) != EOF)
		putchar(ich);
	fflush(stdout);

	fclose(fp);

	return 0;
}

