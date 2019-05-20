/*==============================================
* FileName    : server.c
* Author      : liming
* Create Time : 2019-05-19
* description : 实现一个多进程的并发服务器
==============================================*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "wrap.h"
#include <strings.h>
#include <ctype.h>
#include <sys/wait.h>
//#include <signal.h>

#define SERV_PORT 8888

// 封装信号的回收函数,回收子进程

void wait_child(int signo)
{
	while(waitpid(0, NULL, WNOHANG) > 0);
	return ;
}

int main(){

	// 定义变量
	struct sockaddr_in serv_addr, clie_addr;
	socklen_t clie_addr_len;
	char buf[BUFSIZ]; char buf_ip[BUFSIZ];
	int sockfd;
	pid_t pid;
	
	// 创建 socket
	int lfd = my_socket(AF_INET, SOCK_STREAM, 0);

	// 初始化结构体 sockaddr
	bzero(&serv_addr,sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERV_PORT);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// 绑定 IP 和 端口号
	my_bind(lfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	
	// 设置连接数量上限
	my_listen(lfd, 128);

	// 接收请求并建立连接,它有一个传入参数和一个传入传出参数
	while(1)
	{
		clie_addr_len = sizeof(clie_addr);
		sockfd = my_accept(lfd, (struct sockaddr*)&clie_addr, &clie_addr_len);
		printf("Your IP is %s, Port is %d\n", 
		inet_ntop(AF_INET, &clie_addr.sin_addr.s_addr, buf_ip, sizeof(buf_ip)),
		ntohs(clie_addr.sin_port));

		// 创建子进程响应多个客户端请求
		pid = fork();

		if(pid < 0)
		{
			perror("Fork Failed: ");
			exit(1);
		}
		else if(pid == 0)
		{// 进入了子进程的处理过程当中,首先关闭掉从父进程继承的 lfd 这个socket
			close(lfd);
			break;	
			
		}
		else
		{// 进入父进程的处理过程
			close(sockfd);
			
		 //注册一个信号捕捉函数，设置它的回调函数
			signal(SIGCHLD, wait_child);
		}
	}

	if(pid == 0)
	{// 子进程的处理逻辑
			while(1)
			{
				int n =	read(sockfd, buf, sizeof(buf));
				if(n == 0)
				{// 客户端关闭
					close(sockfd);
					return 0;
				}
				else if(n == -1)
				{
					perror("read error: ");
					exit(1);
				}
				else
				{
					for (int i = 0; i < n; i++)
						buf[i] = toupper(buf[i]);
					write(sockfd, buf, n);
				}

			}	
	}


	return 0;
}


