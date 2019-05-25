/*==============================================
* FileName    : server.c
* Author      : liming
* Create Time : 2019-05-24
* description : 利用 poll 函数来实现一个并发服务器
==============================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>
#include <ctype.h>

#include "wrap.h"

#define MAXLINE 80
#define SERV_PORT 8000
#define OPEN_MAX 1024

int main(){
	// 定义变量
	struct sockaddr_in serv_addr, clie_addr;
	socklen_t clie_addr_len;
	struct pollfd client[OPEN_MAX];  // poll 函数参数需要的结构体数组 
	char buf[MAXLINE], str[INET_ADDRSTRLEN];

	// 建立一个 socket
	int listen_fd = my_socket(AF_INET, SOCK_STREAM, 0);

	// 初始化和设置端口复用
	int opt = 1;
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERV_PORT);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// band  和 listen
	my_bind(listen_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

	my_listen(listen_fd, 128);

	// 初始化数组，将第一个监听 socket 加入到数组中
	client[0].fd = listen_fd;
	client[0].events = POLLIN;

	for(int i = 1; i < OPEN_MAX; i++)
		client[i].fd = -1;

	// 有效元素中的最大下标值
	int maxi = 0;

	// 循环监听客户端请求并处理
	while(1)
	{
		int nready = poll(client, maxi+1, -1);

		if(client[0].revents & POLLIN)
		{
			clie_addr_len = sizeof(clie_addr);
			int conn_fd = my_accept(listen_fd,(struct sockaddr*)&clie_addr, &clie_addr_len);
			print_port_ip(clie_addr, str);

			int i;
			for(i = 0; i < OPEN_MAX; i++)
				if(client[i].fd < 0)
				{
					client[i].fd = conn_fd;
					break;
				}
		// 设置新的 socket 的读事件，并更新最大下标
			client[i].events = POLLIN;

			if(i > maxi)
				maxi = i;
			if(--nready == 0)
				continue;
		}

		// 开始处理读事件响应了的客户端
		for(int i = 1; i <= maxi; i++)
		{
			int sockfd = client[i].fd;
			if(sockfd < 0)
				continue;

			if(client[i].revents & POLLIN)
			{
				int n = read(sockfd, buf, sizeof(buf));
				if(n < 0)
				{
					if(errno == ECONNRESET)
					{
						printf("client[%d] aborted connection\n", i);
						close(sockfd);
						client[i].fd = -1;
					}
					else
						perr_exit("read error:");
				}
				else if(n == 0)
				{
					printf("client[%d] closed:\n", i);
					close(sockfd);
					client[i].fd = -1;
				}
				else
				{
					for(int j = 0; j < n; j++)
						buf[j] = toupper(buf[j]);
					write(sockfd, buf, n);
				}
				
				if(--nready == 0)
					break;
			}
		}
	}

	close(listen_fd);
	return 0;
}
