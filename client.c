/*==============================================
* FileName    : client.c
* Author      : liming
* Create Time : 2019-05-19
* description : 一个简单的1对1 socket 编程的客户端
==============================================*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#define SERV_PORT 8888
#define SERV_IP "127.0.0.1"

int main()
{
	// 定义缓冲区
	char buf [BUFSIZ];

	//创建socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	// 创建结构体 sockaddr_in 保存服务器的 iP 和 port
	struct sockaddr_in server_sock;

	// 清理和初始化结构体,绑定服务器的地址
	bzero(&server_sock, sizeof(server_sock));
	
	server_sock.sin_family = AF_INET;
	server_sock.sin_port = htons(SERV_PORT);
	inet_pton(AF_INET, SERV_IP, &server_sock.sin_addr.s_addr);

	// 和服务器端进行连接通信
	connect(sockfd, (struct sockaddr*)&server_sock, sizeof(server_sock));
	
	while(1)
	{
		// 从终端读取数据并写到 socket 中，传输给服务器
		fgets(buf, BUFSIZ, stdin);
		write(sockfd, buf, strlen(buf));
	
		// 从服务器读取处理后的数据，输出到终端上
		int n = read(sockfd, buf, sizeof(buf));
		write(STDOUT_FILENO, buf, n);
	}	
	// 关闭 socket
	close(sockfd);

	return 0;

}
