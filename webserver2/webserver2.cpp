/* 用 c++ 语言完成的静态 web 服务器
** 加入了 线程池、定时器、请求任务队列、 RALL锁、 智能指针、 非拷贝赋值类（移动？）
*/

// 自己封装的类
#include "requestData.h"
#include "epoll.h"
#include "threadpool.h"
#include "util.h"

// 系统库 
#include <sys/epoll.h>
#include <queue>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unistd.h>

using namespace std;

// 常亮和全局变量的定义
const int THREADPOOL_THREAD_NUM = 4;    // 线程池数量
const int QUEUE_SIZE = 65535;           // 任务队列大小

const int PORT = 8888;
const int ASK_STATIC_FILE = 1;
const int ASK_IMAGE_STITCH = 2;

const char* path = "/home/";
const int TIMER_TIME_OUT = 500;

// extern 申明需要在多个文件内使用的变量
extern pthread_mutex_t qlock;           // 线程互斥锁
extern struct epoll_event* events;      // epoll_event 结构体
extern priority_queue<mytimer*, deque<mytimer*>, timerCmp> myTimerQueue;

void acceptConnection(int listen_fd, int epoll_fd, const string& path);

// 创建套接字、端口复用、 bind、 listen  
int socket_bind_listen(int port)
{
    if(port < 1024 || port > 65535) return -1;
    
    // 创建 socket （IPV4 + TCP）， 返回监听套接字描述符
    int listen_fd = 0;
    if((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) return -1;

    // 建立端口复用，避免 time_wait 过多状态
    int optval = 1;
    if(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
        return -1;
    
    // 设置服务器 IP 和 port， bind 和 listen
    struct sockaddr_in server_addr;
    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) return -1;
    if(listen(listen_fd, LISTENQ) == -1) return -1;

    if(listen_fd == -1)
    {
        close(listen_fd);
        return -1;
    }
    return listen_fd;
}

// 事件回调函数, 利用封装的 requestData 类来处理
void myHandler(void *args)
{
    requestData* req_data = (requestData*)args;
    req_data->handleRequset();
}

// 建立连接、并设置边沿触发模式保证一个 socket 在任意时刻只能被一个线程处理
void acceptConnection(int listen_fd, int epoll_fd, const string& path)
{
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    socklen_t client_addr_len = 0;
    int accept_fd = 0;

    while((accept_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_addr_len)) > 0)
    {
        int ret = setSockNonBlocking(accept_fd);
        if(ret < 0)
        {
            perror("Set non block failed");
            return;
        }

        requestData* req_info = new requestData(epoll_fd, accept_fd, path);
		//文件描述符可以读 边缘触发模式 保证一个socket链接在任一时刻只被一个线程处理
		//即使设置EPOLLET模式还是有可能一个套接字上的事件被多次触发
        __uint32_t _epo_event = EPOLLIN | EPOLLET | EPOLLONESHOT;
        epoll_add(epoll_fd, accept_fd, static_cast<void*>(req_info), _epo_event);

        // 新增时间信息
        mytimer* mtimer = new mytimer(req_info, TIMER_TIME_OUT);
        req_info->addTimer(mtimer);

		pthread_mutex_lock(&qlock);
/* 		{
			MutexLockGuard();
		}
        自己封装 mutex 实现 RAII 特性，避免忘记释放锁 
        */
		myTimerQueue.push(mtimer);
		pthread_mutex_unlock(&qlock);

    }
}

// 事件处理、将请求事件放入线程池中
void handle_events(int epoll_fd, int listen_fd, struct epoll_event* events, int events_num, const string &path, threadpool_t* tp)
{
    for(int i = 0; i < events_num; i++)
    {
        requestData* requset = (requestData*)(events[i].data.ptr);
        int fd = requset->getFd();
        // 监听描述符响应返回
        if(fd == listen_fd)
        {
            acceptConnection(listen_fd, epoll_fd, path);
        }
        // 连接的描述符返回
        else
        {
            // 排除错误事件
            if((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN)))
            {
                printf("error event\n");
                delete requset;
                continue;
            }
            //将请求任务加入到线程中
			//加入线程池之前将Timer和request分离
			requset->seperateTimer();
			//int threadpool_add(threadpool_t *pool, void(*function)(void *), void *argument, int flags)
			/*这个回调函数即为处理请求数据的回调函数  myHandler -》 handleRequest*/
			int rc = threadpool_add(tp, myHandler, events[i].data.ptr, 0);
        }
    }
}

/*处理逻辑
因为(1)优先队列不支持随机访问
(2)即使支持 随机删除某节点后破坏了堆的结构 需要重新更新堆结构
所以对于被置为deleted的时间节点 会延迟到它(1)超时或者(2)前面的节点都被删除 它才会删除
一个点被置为deleted 它最迟在TIMER_TIMER_OUT时间后被删除
这样做有两个好处
(1)第一个好处是不需要遍历优先级队列 省时
(2)第二个好处是给超时时间一个容忍时间 就是设定的超时时间是删除的下限（并不是一到超时时间就立即删除）
如果监听的请求在超时后的下一次请求中又出现了一次就不用重新申请requestData节点了，这样可以继续重复利用
前面的requestData 减少了一次delete和一次new的时间
*/

// 处理超时事件
void handle_expired_event()
{
    // MutexLockGuard();
    pthread_mutex_lock(&qlock);
	while (!myTimerQueue.empty())
	{
		mytimer *ptimer_now = myTimerQueue.top();
		if (ptimer_now->isDeleted())
		{
			myTimerQueue.pop();
			delete ptimer_now;
		}
		else if (ptimer_now->isvalid() == false)
		{
			myTimerQueue.pop();
			delete ptimer_now;
		}
		else
		{
			break;
		}
	}
	pthread_mutex_unlock(&qlock);
}

int main()
{
    // 进入工作目录
    int ret = chdir(path);
    if(ret != 0)
    {
        perror("chdir error");
        exit(1);
    }
    
    // 信号处理函数
    handle_for_sigpipe();

    // 创建 epoll 红黑树
    int epoll_fd = epoll_init();
    if(epoll_fd < 0)
    {
        perror("epoll init failed");
        exit(1);
    }

    // 创建线程池
    threadpool_t* threadpool = threadpool_create(THREADPOOL_THREAD_NUM, QUEUE_SIZE, 0);

    // 创建监听套接字
    int listen_fd = socket_bind_listen(PORT);
    if(listen_fd < 0)
    {
        perror("socket bind failed");
        exit(1);
    }

    // 设置非阻塞监听模式
    if(setSockNonBlocking(listen_fd) < 0)
    {
        perror("set socket no block failed");
        exit(1);
    }

    // 设置监听读事件
    __uint32_t event = EPOLLIN | EPOLLET;
    requestData* req = new requestData();
    req->setFd(listen_fd);
    epoll_add(epoll_fd, listen_fd, static_cast<void*>(req), event);

    while(true)
    {
        int events_num = my_epoll_wait(epoll_fd, events, MAXEVENTS, -1);
        if(events_num == 0)
        {
            continue;
        }
        printf("The events_num is %d\n", events_num);

        // 遍历数组， 分发事件
        handle_events(epoll_fd, listen_fd, events, events_num, path, threadpool);

        // 超时事件处理
        handle_expired_event();
    }
    return 0;
}


