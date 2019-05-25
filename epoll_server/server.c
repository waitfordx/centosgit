/*==============================================
* FileName    : epoll_server.c
* Author      : liming
* Create Time : 2019-05-25
* description : 利用 epoll API 来实现并发服务器
==============================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>
#include <ctype.h>

#include "wrap.h"

#define MAXLINE 8192
#define SERV_PORT 8000
#define OPEN_MAX 5000

int main(){
	// 定义变量
	struct sockaddr_in serv_addr, clie_addr;
	socklen_t clie_len;
	char buf[MAXLINE], str[INET_ADDRSTRLEN];
	// 定义 event 函数需要使用的结构体 struct epoll_event。一个结构体和一个结构体数组
	struct epoll_event ev, evs[OPEN_MAX];

	// 创建监听 socket
	int listen_fd = my_socket(AF_INET, SOCK_STREAM, 0);

	// 初始化和端口复用
	int opt = 1;
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERV_PORT);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// bind 和 listen
	my_bind(listen_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

	my_listen(listen_fd, 128);

	// 开始创建 epoll 模型， 返回值指向一个红黑树
	int epfd = epoll_create(OPEN_MAX);

	// 将监听 socket 的 fd 加入到红黑树上
	ev.events = EPOLLIN; ev.data.fd = listen_fd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

	// 开始循环处理连接请求和事件处理
	while(1)
	{
		int nready = epoll_wait(epfd, evs, OPEN_MAX, -1);
		
		for(int i = 0; i < nready; i++)
		{
			if(!(evs[i].events & EPOLLIN))
				continue;

			if(evs[i].data.fd == listen_fd)
			{
				clie_len = sizeof(clie_addr);
				int conn_fd = my_accept(listen_fd, (struct sockaddr*)&clie_addr, &clie_len);
				print_port_ip(clie_addr,str);
				// 将新的客户端加入到红黑树上
				ev.events = EPOLLIN; ev.data.fd = conn_fd;
				epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &ev);
			}
			else
			{
				int sockfd = evs[i].data.fd;
				int n = read(sockfd, buf, MAXLINE);
				if(n == 0)
				{
					// 客户端关闭，将该 fd 从红黑树上删除
					epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
					close(sockfd);
					printf("Client[%d] closed\n", sockfd);
				}
				else if(n < 0)
				{
					perror("read error:");
					epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
					close(sockfd);
				}
				else
				{
					for(int j = 0; j < n; j++)
						buf[j] = toupper(buf[j]);
					
					write(sockfd, buf, n);
				}
			}
		}
	}
	close(listen_fd);
	close(epfd);
	return 0;

}
