/*==============================================
* FileName    : esay_server.cpp
* Author      : liming
* Create Time : 2019-05-12
* description : a server program about how to use libevent
==============================================*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <event.h>


// declare functions here
void accept_cb(int fd, short events, void* arg);
void socket_read_cb(int fd, short events, void* arg);

int tcp_server_init(int port, int listen_num);

int main(int argc, char** argv){
	int listener = tcp_server_init(9995,10);
	if(listener == -1)
	{
		perror("TCP Init Failed!\n");
		return -1;
	}

	struct event_base *base = event_base_new();

	struct event *ev_listen = event_new(base, listener, EV_READ | EV_PERSIST, accept_cb, base);

	event_add(ev_listen, NULL);

	event_base_dispatch(base);

	return 0;
}


void accept_cb(int fd, short events, void* arg){
	evutil_socket_t sockfd;

	struct sockaddr_in client;
	socklen_t len = sizeof(client);

	sockfd = ::accept(fd, (struct sockaddr*)&client, &len);
	evutil_make_socket_nonblocking(sockfd);

	printf("I accept a client %d\n", sockfd);

	struct event_base *base = (event_base*)arg;

	struct event *ev = event_new(NULL, -1, 0, NULL, NULL);

	event_assign(ev, base, sockfd, EV_READ | EV_PERSIST, socket_read_cb, (void*)ev);

	event_add(ev,NULL);

}

void socket_read_cb(int fd, short events, void* arg){
	char msg[4096];
	struct event *ev = (struct event*)arg;

	int len = read(fd, msg, sizeof(msg)-1);

	if(len <= 0)
	{
		printf("Some error happened when read!\n");
		event_free(ev);
		close(fd);
		return ;
	}
	
	msg[len] = '\0';

	printf("Receive the client msg : %s", msg);

	char reply_msg[2048] = "I have reveive the msg from you~~: ";
	strcat(reply_msg + strlen(reply_msg), msg);

	write(fd, reply_msg, strlen(reply_msg));
}


typedef struct sockaddr SA;
int tcp_server_init(int port, int listen_num){
	int errno_save;
	evutil_socket_t listener;

	listener = ::socket(AF_INET, SOCK_STREAM, 0);
	if(listener == -1)
	{
		return -1;
	}

	evutil_make_listen_socket_reuseable(listener);

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(port);
	

	if(::bind(listener, (SA *)&sin, sizeof(sin) ) < 0)
	{
		errno_save = errno;
		evutil_closesocket(listener);
		errno = errno_save;
		return -1;
	}

	if(::listen(listener, listen_num) < 0)
	{
		errno_save = errno;
		evutil_closesocket(listener);
		errno = errno_save;
		return -1;
	}
	
	evutil_make_socket_nonblocking(listener);

	return listener;
}








