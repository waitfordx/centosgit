/*==============================================
 * FileName    : server.c
 * Author      : liming
 * Create Time : 2019-05-23
 * description : 利用 I/O 多路复用函数来实现一个并发服务器
 ==============================================*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "wrap.h"

#define SERV_PORT 6666

int main(){
	// 定义需要的相关变量
	struct sockaddr_in serv_addr, clie_addr; 
	socklen_t clie_addr_len;
	char buf[BUFSIZ], str[INET_ADDRSTRLEN];
	int client[FD_SETSIZE];		// 用来记录连接上来的客户端
	fd_set read_set, all_set;	// all_set 用来记录当前所有的fd， read_set 记录每次读事件的 fd

	int listenfd = my_socket(AF_INET, SOCK_STREAM, 0);

	// 初始化、
	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERV_PORT);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// bind 和 listen
	my_bind(listenfd,(struct sockaddr*)&serv_addr, sizeof(serv_addr));
	my_listen(listenfd, 128);

	int maxfd = listenfd;	// 记录最大的文件描述符
	int maxi = -1; 			// 记录当前为止有请求的文件描述符的最大下标

	// 初始化客户端数组
	for(int n = 0; n < FD_SETSIZE; n++ )
		client[n] = -1;

	// 初始化 all_set 集合。开始监听 listenfd 的读事件，以便建立连接
	FD_ZERO(&all_set);
	FD_SET(listenfd, &all_set);

	// while 开始循环处理事件
	while(1)
	{
		int i;
		read_set = all_set;		// 每次循环开始，重新设置可读事件集合
		int nready = select(maxfd+1, &read_set, NULL, NULL, NULL);

		//	当有新的客户端连接请求时
		if(FD_ISSET(listenfd, &read_set))
		{
			clie_addr_len = sizeof(clie_addr);
			int conn_fd = my_accept(listenfd, (struct sockaddr*)&clie_addr, &clie_addr_len);
			print_port_ip(clie_addr, str);

			// 将新连接的客户端加入到 client 数组中
			for( i = 0; i < FD_SETSIZE; i++)
				if(client[i] < 0)
				{
					client[i] = conn_fd;
					break;
				}

			// 将新的客户端加入到 all_Set 集合中进行监听
			FD_SET(conn_fd, &all_set);
			if(conn_fd > maxfd)
				maxfd = conn_fd;		// 更新当前的最大文件描述符，传递给 select 函数

			if(i > maxi)
				maxi = i;			// 更新 client 数组中当前的最大下标

			if(--nready == 0)
				continue;				// 说明当前只有一个连接请求且无其他事件，直接下一次循环
		}

		// 当处理完连接请求，开始处理 client 数组中发生了可读事件的 fd
		for(i = 0; i <= maxi; i++)
		{
			int sockfd = client[i];
			printf("maxi is %d\t, sockfd is %d", maxi, sockfd);
			if(sockfd < 0)
				continue;

			if(FD_ISSET(sockfd, &read_set))
			{
				int n = read(sockfd, buf, sizeof(buf));
				if(n == 0)
				{// 客户端关闭
					close(sockfd);
					client[i] = -1;
					FD_CLR(sockfd, &all_set);
				}
				else if(n > 0)
				{
					for(int k = 0; k < n; k++)
						buf[k] = toupper(buf[k]);
					write(sockfd, buf, n);
				}

			//	发生了可读事件的 fd 都处理完毕。
			if(--nready == 0)
				break;
			}
		}


	}
	close(listenfd);
	return 0;
}
