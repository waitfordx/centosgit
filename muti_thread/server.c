/*==============================================
 * FileName    : server.c
 * Author      : liming
 * Create Time : 2019-05-20
 * description : 实现一个多线程的并发服务器
 ==============================================*/

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

#include "wrap.h"

#define SERV_PORT 8000
#define MAXLINE 8192

// 定义一个结构体，使得每个子线程处理的客户端和 socket 一致
struct s_info{
	struct sockaddr_in clie_addr;
	int conn_fd;
};


void* do_work(void *arg){
	// 线程的回调函数,void* 类型的指针可以转换成任何类型的指针（使得函数功能更灵活）
	struct  s_info *ts = (struct s_info*)arg;
	char buf[MAXLINE];
	char str[INET_ADDRSTRLEN]; // [ + d 可以查看宏定义

	printf("Receive client IP: %s, Port：%d\n", 
			inet_ntop(AF_INET, &(*ts).clie_addr.sin_addr.s_addr, str, sizeof(str)),
			ntohs((*ts).clie_addr.sin_port)
		  );

	// 处理客户端的请求
	while(1)
	{
		int n = read(ts->conn_fd, buf, MAXLINE );
		if(n == 0)
		{// 读到了文件末尾，代表对端 socket 已经关闭
			printf("The client %d closed!\n", ts->conn_fd);
			break;
		}

		for(int i = 0; i < n; i++)
			buf[i] = toupper(buf[i]);

		write(ts->conn_fd, buf, n);
	}

	close(ts->conn_fd);
	return (void*)0;
}


int main(){
	// 定义相关变量
	struct sockaddr_in serv_addr, clie_addr;
	socklen_t clie_addr_len;
	pthread_t tid;
	int conn_fd;
	struct s_info ts[256];
	int i = 0;

	// 创建 socket 
	int listen_fd = my_socket(AF_INET, SOCK_STREAM, 0);

	// 初始化结构体
	bzero(&serv_addr, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERV_PORT);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// 绑定端口 和 ip
	my_bind(listen_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr) );

	// 设置同时建立连接的上限数
	my_listen(listen_fd, 128);


	// 主线程循环监听客户端连接请求，同时创建子线程处理客户端事物
	while(1)
	{
		clie_addr_len = sizeof(clie_addr);
		conn_fd = my_accept(listen_fd, (struct sockaddr*)&clie_addr, &clie_addr_len);
		ts[i].clie_addr = clie_addr;
		ts[i].conn_fd = conn_fd;

		pthread_create(&tid, NULL, do_work, (void*)&ts[i]);
		// 子线程分离， 避免僵尸进程
		pthread_detach(tid);
		i++;
	}

	return 0;
}

