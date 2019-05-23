#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

void perr_exit(const char *s){
	// 错误处理函数
	perror(s);
	exit(-1);
}

int my_socket(int domain, int type, int protocol){
	int ret = socket(domain, type, protocol);
	if(ret < 0)
		perr_exit("socket error:");
	return ret;
}

int my_bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen){
	int ret = bind(sockfd, addr, addrlen);
	if(ret < 0)
		perr_exit("Bind error:");
	return ret;
}

int my_listen(int sockfd, int backlog){
	int ret = listen(sockfd, backlog);
	if(ret < 0)
		perr_exit("Listen error:");
	return ret;
}

int my_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen){
	int ret = connect(sockfd, addr, addrlen);
	if(ret < 0)
		perr_exit("Connect error: ");
	return ret;
}

int my_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen){
	// accept 是慢速调用，阻塞等待。循环调用，信号打断后可重启
	int ret;
	while((ret=accept(sockfd, addr, addrlen)) < 0)
	{
		if((errno == ECONNABORTED) ||(errno == EINTR))
			continue;
		else 
		{
			perr_exit("Accept error: ");
			break;
		}			
	}
	return ret;
}

