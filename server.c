#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <string.h>

#define SERV_PORT 8888


int main(){
	// 创建一个 sockaddr_in 结构体存储数据
	char buf[BUFSIZ];
	struct sockaddr_in server_sock;
	struct sockaddr_in client_sock;  socklen_t addr_len;
	
	// 创建 socket
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	
	// 初始化记录服务器信息的 sockaddr_in
	
	bzero(&server_sock, sizeof(server_sock));
	
	server_sock.sin_family = AF_INET;
	server_sock.sin_port = htons(SERV_PORT);
	server_sock.sin_addr.s_addr = htonl(INADDR_ANY);
	
	// 绑定 IP 和 port
	bind(lfd, (struct sockaddr*)&server_sock, sizeof(server_sock));
	
	// 设置同时连接上限
	listen(lfd, 10);
	
	// 创建真正用于接收数据的 socket, client_sock 用于返回接收到的客户端的信息
	addr_len = sizeof(client_sock);
	int sockfd = accept(lfd, (struct sockaddr*)&client_sock, &addr_len);
	
	// 处理与客户端的交互
	while(1)
	{
		int n = read(sockfd, buf, sizeof(buf));
		for(int i = 0; i < n; i++)
		{
			buf[i] = toupper(buf[i]);
		}
		write(sockfd, buf, n);
	}
	// 处理完后，关闭文件描述符
	close(lfd);
	close(sockfd);
	
	return 0;
}
