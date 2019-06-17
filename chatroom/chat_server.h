/*==============================================
* FileName    : chat.h
* Author      : liming
* Create Time : 2019-06-17
* description : 简易聊天室的头文件，声明协议和相关函数 
==============================================*/

#ifndef CHAT_H
#define CHAT_H


// 对协议的封装，包含有协议号，发送方，接收方，消息
struct my_protocol{
	int number;
	char send_name[20];
	char recv_name[20];
	char data[1024];
};
typedef struct my_protocol MP;

// 封装客户端, 利用客户端名字作为 FIFO。
struct client{
	char client_name[20];
	int fifo_fd;
	int status;
};

// 记录登录客户机的数量.需要改进重复利用下标。
int num_client = 0;
struct client my_client[100];
int max_num = -1;

// 公共管道和服务器启动标志。公共管道用于读取请求
int ser_fd;
int ser_flags = 0;

// 初始化，建立和打开公共管道。
void init_server();

// 接收客户端发送的包
void recv_message();

// 解析客户端发送的包
void parse_packet(MP* ms);

// 处理客户端的登录行为
void client_login(char* login_name);

// 处理发送信息
void send_message(MP* ms);

// 处理客户端退出
void client_quit(char* quit_name);

// 服务器关闭
void close_server();

// 输入数据的处理
void message_handle(char* ms);



#endif
