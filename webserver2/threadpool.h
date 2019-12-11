// 线程池的实现
#ifndef THREADPOOL
#define THREADPOOL
#include "requestData.h"
#include <pthread.h>


const int THREADPOOL_INVALID = -1;
const int THREADPOOL_LOCK_FAILURE = -2;
const int THREADPOOL_QUEUE_FULL = -3;
const int THREADPOOL_SHUTDOWN = -4;
const int THREADPOOL_THREAD_FAILURE = -5;
const int THREADPOOL_GRACEFUL = -1;

const int MAX_THREADS = 1024;
const int MAX_QUEUE = 65535;

// 关闭方式 ： 直接关闭 和 优雅关闭
typedef enum
{
    immediate_shutdown = 1,
    graceful_shutdown = 2
}threadpool_shutdown_t;

// 任务结构体
typedef struct 
{
    void(*function)(void*);
    void *argument;
}threadpool_task_t;

/**
*  @struct threadpool
*  @brief The threadpool struct
*
*  @var notify       Condition variable to notify worker threads.
*  @var threads      Array containing worker threads ID.
*  @var thread_count Number of threads
*  @var queue        Array containing the task queue.
*  @var queue_size   Size of the task queue.
*  @var head         Index of the first element.
*  @var tail         Index of the next element.
*  @var count        Number of pending tasks
*  @var shutdown     Flag indicating if the pool is shutting down
*  @var started      Number of started threads
*/

// 线程池是一种结构体类型
struct threadpool_t
{
    
    pthread_mutex_t lock;       // 互斥锁
    pthread_cond_t notify;      // 信号量
    
    pthread_t *threads;         // 线程指针 数组

    threadpool_task_t *queue;   // 任务队列
    int thread_count;           // 线程池线程数
    int queue_size;             // 队列大小
    int head;                   // 队列头
    int tail;                   // 队列尾
    int count;                  // 此时队里的容量 线程任务队列的任务数量
    int shutdown;               // 线程池结束状态
    int started;                // 
};

// 线程池操作相关函数
threadpool_t *threadpool_create(int thread_count, int queue_size, int flags);
int threadpool_add(threadpool_t *pool, void(*function)(void *), void *argument, int flags);
int threadpool_destroy(threadpool_t *pool, int flags);
int threadpool_free(threadpool_t *pool);
static void *threadpool_thread(void *threadpool);

#endif