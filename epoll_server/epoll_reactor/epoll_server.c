#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* socket 相关处理函数 */

#define BUFLEN 4096
#define MAX_EVENTS 1024
#define SERV_PORT 4096

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


void print_port_ip(struct sockaddr_in client_name){
		printf("Your IP is %s, Port is %d\n", 
		inet_ntoa(client_name.sin_addr), ntohs(client_name.sin_port));
}


/* epoll 反应堆模型相关处理函数*/

void recvdata(int fd, int events, void *arg);
void senddata(int fd, int events, void *arg);

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

int g_efd;
struct myevent_s g_events[MAX_EVENTS+1];


// 初始化自定义的结构体
void eventset(struct myevent_s *ev, int fd, void(*call_back)(int fd, int events, void* arg), void *arg){
	// 读取参数传递的值
	ev->fd = fd;
	ev->call_back = call_back;
	ev->arg = arg;
	
	// 类似于默认初始化	
	ev->status = 0;
	ev->events = 0;
//	memset(ev->buf, 0, sizeof(ev->buf));
//	ev->len = 0;
	ev->last_active = time(NULL);

	return;
}


// 向红黑树上添加一个结点
void eventadd(int efd, int events, struct myevent_s *ev){
	struct epoll_event epv = {0, {0}};
	int op;
	// 该节点的 epoll_event 结构体的指针指向自定义的结构体 myevent_s
	epv.data.ptr = ev;

	epv.events = ev->events = events;
	
	if(ev->status == 1)
		op = EPOLL_CTL_MOD;
	else 
	{
		ev->status = 1;
		op = EPOLL_CTL_ADD;
	}
	
	// 向红黑树上添加或修改事件
	if(epoll_ctl(efd, op, ev->fd, &epv) < 0)
		printf("event add failed [fd=%d], evetns[%d]", ev->fd, events);
	else
		printf("event add ok! [fd=%d], op = %d, events[%d]\n", ev->fd, op, events);

		return ;
}


// 从红黑树上删除一个结点
void eventdel(int efd, struct myevent_s *ev)
{
	struct epoll_event epv = {0, {0}};

	if(ev->status != 1)
		return ;
	
	epv.data.ptr = ev;
	ev->status = 0;
	epoll_ctl(efd, EPOLL_CTL_DEL, ev->fd, &epv);

	return ;
}

// socket：lfd 的回调函数。在这个函数里面执行 accept 和设置新的客户端的回调函数
void acceptconn(int lfd, int events, void* arg)
{
	struct sockaddr_in clie_addr;
	socklen_t clie_len = sizeof(clie_addr);

	int cfd = my_accept(lfd, (struct sockaddr*)&clie_addr, &clie_len);

	do
	{
		int i;
		// 在全局数组中找一个空余的位置，将该事件放入。
		for(i = 0; i < MAX_EVENTS; i++)
			if(g_events[i].status == 0)
				break;

		int flag = 0;
		if((flag = fcntl(cfd, F_SETFL, O_NONBLOCK)) < 0)
		{
			printf("%s fcntl nonblocking failed, %s\n", __func__, strerror(errno));
			break;
		}

		// 将 cfd 设置一个 myevent_s 结构体并加到红黑树上，监听读事件。
		eventset(&g_events[i], cfd, recvdata, &g_events[i]);
		eventadd(g_efd, EPOLLIN, &g_events[i]);

	}while(0);

	print_port_ip(clie_addr);
	return ;
}

// 这个函数完成 socket 的创建和 listen bind
void init_listen_socket(int efd, short port){
	int lfd = my_socket(AF_INET, SOCK_STREAM, 0);
	
	fcntl(lfd, F_SETFL, O_NONBLOCK);		// 设置为非阻塞 I/O

	// 初始化自定义的结构体，并将 lfd 添加到红黑树上
	
	/* void* arg 指针指向了 myevent_s 结构体 */
	eventset(&g_events[MAX_EVENTS], lfd, acceptconn, &g_events[MAX_EVENTS]);	 
	eventadd(efd, EPOLLIN, &g_events[MAX_EVENTS]);

	// 初始化 sockaddr_in 结构体并绑定
	struct sockaddr_in serv_addr;
	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	my_bind(lfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

	my_listen(lfd, 128);

	return;
}




// 客户端接收到数据的回调函数
void recvdata(int fd, int events, void *arg)
{
	// 接收返回的泛型指针
	struct myevent_s *ev = (struct myevent_s *)arg;
	printf("this is ev data %d\n", ev->status);
	int len = recv(fd, ev->buf , sizeof(ev->buf), 0);
	eventdel(g_efd, ev);		// 将该节点从红黑树上摘下	
	if(len > 0)
	{
		
		ev->len = len;
		/*for(int m = 0; m < len; m++)
			(ev->buf)[m] = toupper((ev->buf)[m]);*/
		ev->buf[len] = '\0';
		printf("i have read you %d and that is %s\n", ev->len, ev->buf);
		// 设置该 fd 对应的回调函数为 senddata，并加入到 红黑树上，监听可写事件
		eventset(ev, fd, senddata, ev);
		eventadd(g_efd, EPOLLOUT, ev);
	}
	else if(len == 0)
	{
		close(ev->fd);
		printf("[fd=%d] pos[%ld], closed\n", fd, ev-g_events);
	}
	else
	{
		close(ev->fd);
		printf("recv [fd=%d] error:%s\n", fd, strerror(errno));
	}

	return ;
}


// 服务器回写的回调函数
void senddata(int fd, int events, void* arg)
{
	struct myevent_s *ev = (struct myevent_s*)arg;
	int len = send(fd, ev->buf, ev->len, 0);
	printf("i will write %d  back to you\n", len);
	if(len > 0)
	{
		printf("send [fd=%d], [%d]%s\n", fd, len, ev->buf);

		eventdel(g_efd, ev);
		eventset(ev, fd, recvdata, ev);
		eventadd(g_efd, EPOLLIN, ev);
	}
	else
	{
		// 关闭连接并从红黑树上删除
		close(ev->fd);
		eventdel(g_efd, ev);  
	}

	return ;
}

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


