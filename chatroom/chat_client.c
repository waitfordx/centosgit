/*==============================================
* FileName    : chat_client.c
* Author      : liming
* Create Time : 2019-06-17
* description : 本地聊天室的客户端，负责登录、发送信心、接收信息。 
==============================================*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>

#include "chat_client.h"

#define SERV_FIFO "/tmp/fifo"
#define BUFSIZE 1068


// 初始化客户端，设置标志位和 STDIN_FILENO 非阻塞
void init_client(){

	login_server();

	flags = 1 - flags;

	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}


// 登录服务器
void login_server(){
	
	printf("please input client name \n");
	scanf("%s", client_name);

	ser_fd = open(SERV_FIFO, O_WRONLY|O_NONBLOCK);
	if(ser_fd < 0)
	{
		perror("open server fifo");
		exit(1);
	}
	
	// 发送 0 号登录包,确定 fifo 名称
	send_to_server(0);
	char path[25] = "/tmp/";
	strcat(path, client_name);

	// 测试管道是否创建成功
	while(access(path, F_OK) != 0);

	cli_fd = open(path, O_RDONLY|O_NONBLOCK);
	if(cli_fd < 0)
	{
		perror("open client fifo");
		exit(1);
	}

	printf(" login succeful!\n");
}


// 向服务器发送信息.转换指针格式
void send_to_server(int mesno){

	MP mes;
	char* buf;

	mes.number = mesno;
	strcpy(mes.send_name, client_name);

	// 将MP结构体转换成泛型。传递给 char* 的 buf
	buf = (void*)&mes;
	write(ser_fd, buf, sizeof(mes));

}


// 处理用户输入的数据，使数据格式化封装成包
void message_handle(char* mes){
	
	if(strcmp(mes, "quit") == 0)
	{
		send_to_server(2);
		close_client();
		return ;
	}

	// 记录发送者姓名和数据 接收者姓名 : 内容
	char recv_name[20];
	char data[1024];

	int i = 0;
	//	截断接收者姓名
	while(mes[i] != '\0' && mes[i] != ':')
	{
		recv_name[i] = mes[i];
		i++;
	}
	recv_name[i] = '\0';

	if(mes[i] == ':')
		i++;

	else 
	{// 数据不规范，没有接收者
		i = 0;
		recv_name[0] = '\0';
	}
	
	int j = 0;
	while(mes[i] != '\0')
		data[j++] = mes[i++];
	
	data[j] = '\0';

	send_to_client(recv_name, data);

}


// 向其他客户端发送消息
void send_to_client(char* recv_name, char* data){

	MP ms;
	char* buf;
	ms.number = 1;
	strcpy(ms.send_name, client_name);
	strcpy(ms.recv_name, recv_name);
	strcpy(ms.data, data);

	// 将结构体转换为 char* 的数据包发送
	buf = (void*)&ms;
	
	write(ser_fd, buf, sizeof(ms));
}


// 从私有 fifo 接收服务器转发的其他客户端的信息
void recv_message(){
	
	char buf[BUFSIZE];
	int len = read(cli_fd, buf, sizeof(MP));

	// 定义一个结构体指针，接收数据包 char* 类型
	MP* mes = NULL;
	mes = (void*) buf;
	
	if(len > 0 && mes->number == 1)
	{
		printf("%s:%s\n", mes->send_name, mes->data);
	}

	else if(len > 0)
		printf("This is server message : %s", buf);
}


// 客户端退出登录
void close_client(){

	flags = 1 - flags;
	close(cli_fd);
	close(ser_fd);
	printf("you are logout\n");
}


int main()
{
	init_client();

	char mes[1024];

	while(flags)
	{
		if(scanf("%s", mes) != EOF)
			message_handle(mes);
		recv_message();
	}

	return 0;
}
