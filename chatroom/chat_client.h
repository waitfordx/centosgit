/*==============================================
* FileName    : chat_client.h
* Author      : liming
* Create Time : 2019-06-17
* description : 客户端的头文件，包含数据包的声明和函数声明 
==============================================*/

#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H

struct my_protocol{
	int number;
	char send_name[20];
	char recv_name[20];
	char data[1024];
};
typedef struct my_protocol MP;

// 定义连接标志和文件描述符等
int flags = 0;
int ser_fd;
int cli_fd;
char client_name[20];

// 初始化客户端
void init_client();

// 登录服务器
void login_server();

// 处理用户输入的数据
void message_handle(char* mes);

// 向服务器发送信息
void send_to_server(int mesno);

// 向其他用户发送信息
void send_to_client(char* recv_name, char* data);

// 接收消息
void recv_message();

// 退出
void close_client();






#endif
