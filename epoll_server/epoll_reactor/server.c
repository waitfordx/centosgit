/*==============================================
* FileName    : server.c
* Author      : liming
* Create Time : 2019-05-27
* description : 利用 epoll 的反应堆模型来完成并发服务器，添加了回调函数和、非阻塞 I/O 和边沿触发等
* 				知识
==============================================*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "wrap.h"

#define SERV_PORT 8666


// 定义一个全局变量：红黑树的树根

int main(){
	// 定义变量和封装需要的结构体
	

	// 初始化和创建红黑树 
	g_efd = epoll_create(MAX_EVENTS + 1);
	if(g_efd < 0)
		printf("creat efd in %s error %s\n", __func__, strerror(errno));
	
	// 初始化和监听 socket  
	init_listen_socket(g_efd, SERV_PORT);
	
	// 保存满足就绪事件的 epoll_event 数组
	struct epoll_event events[MAX_EVENTS+1];
	printf("server running port : [%d]\n", SERV_PORT);

	// 开始循环监听红黑树和处理事件请求 
	while(1)
	{
		// 开始监听红黑树，将满足的事件 fd 添加到 events 数组中并返回。
		int nfd = epoll_wait(g_efd, events, MAX_EVENTS+1, 1000);
		if(nfd < 0)
			printf("epoll_wait error, exit\n");

		for(int i = 0; i < nfd; i++)
		{
			// 用自定义的结构体来接收返回的 fd 对应的结构体中的 void* 指针
			struct myevent_s *ev = (struct myevent_s *)events[i].data.ptr;
			
			if((events[i].events & EPOLLIN) && (ev->events & EPOLLIN))
				// 读事件就绪，调用它的回调函数
				ev->call_back(ev->fd, events[i].events, ev->arg);
			if((events[i].events & EPOLLOUT) && (ev->events & EPOLLOUT))
				// 写事件就绪，调用它的回调函数
				ev->call_back(ev->fd, events[i].events, ev->arg);
		}
	}

	// 退出前释放所有资源
	return 0;
}
