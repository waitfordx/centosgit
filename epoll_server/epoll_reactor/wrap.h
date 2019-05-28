#ifndef WRAP_H
#define WARP_H

#define BUFLEN 4096
#define MAX_EVENTS 1024

// 自定义一个结构体，用于实习 epoll 的反应堆模型
struct myevent_s{
	int fd;
	int events;
	void *arg;												// 泛型指针，可以指向自己
	void (*call_back)(int fd, int events, void* arg);		// 回调函数
	int status;												// 记录状态，是否在红黑树上
	char buf[BUFLEN];
	int len;
	long last_active;
};

struct myevent_s g_events[MAX_EVENTS+1];
int g_efd;


// 函数声明
void perr_exit(const char *s);
int my_socket(int domain, int type, int protocol);
int my_bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int my_listen(int sockfd, int backlog);
int my_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int my_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
void print_port_ip(struct sockaddr_in client_name);
void init_listen_socket(int efd, short port);
void eventset(struct myevent_s *ev, int fd, void(*call_back)(int fd, int events, void* arg), void *arg);
void eventadd(int efd, int events, struct myevent_s *ev);
void acceptconn(int lfd, int events, void* arg);
void recvdata(int fd, int events, void *arg);
void senddata(int fd, int events, void *arg);
void eventdel(int efd, struct myevent_s *ev);




#endif

