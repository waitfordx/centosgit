/*==============================================
* FileName    : thread_pool.c
* Author      : liming
* Create Time : 2019-05-29
* description : 线程池的实现 
==============================================*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>

#include "threadpool.h"

#define DEFAULT_TIME 10			// 管理者线程每10s检测一次
#define MIN_WAIT_TASK_NUM 10	// 如果任务队列中任务数 > MIN_WAIT_TASK_NUM 添加新的线程到线程池中
#define DEFAULT_THREAD_VARY 10	// 每次添加和销毁的数量
#define true  1
#define false 0


// 定义一个子线程的任务结构体
typedef struct {
	void *(*function)(void*);			// 函数指针，回调函数,返回类型为 void*
	void *arg;							// 参数
}threadpool_task_t;


// 定义线程池的结构体，封装线程池的相关信息
struct threadpool_t{
	pthread_mutex_t lock;				// 用于锁住整个结构体
	pthread_mutex_t thread_counter;		// 记录忙线程个数的锁  busy_thread_num
	pthread_cond_t queue_not_full;		// 任务队列未满。队列满时，添加任务的线程阻塞等待
	pthread_cond_t queue_not_empty;		// 任务队列不空。有任务时，唤醒等待任务的线程

	pthread_t *threads;				// 存放没个线程的 tid，数组
	pthread_t adjust_tid;				// 管理者线程的 tid
	threadpool_task_t *task_queue;		// 任务队列

	int min_thr_num;
	int max_thr_num;
	int live_thr_num;
	int busy_thr_num;
	int wait_exit_thr_num;				// 要销毁的线程数

	int queue_front;
	int queue_rear;
	int queue_size;
	int queue_max_size;

	int shutdown;						// 线程池状态，true表示线程池关闭
};


void *threadpool_thread(void *threadpool);

void *adjust_thread(void *threadpool);

int is_thread_alive(pthread_t);

int threadpool_free(threadpool_t *pool);


// 创建一个线程池并初始化
threadpool_t *threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size){
	int i;
	threadpool_t *pool = NULL;
	do{
		if((pool = (threadpool_t*)malloc(sizeof(threadpool_t))) == NULL)
		{// 给线程池分配地址空间
			printf("malloc threadpool failed");
			break;
		}

		pool->min_thr_num = min_thr_num;
		pool->max_thr_num = max_thr_num;
		pool->busy_thr_num = 0;
		pool->live_thr_num = min_thr_num; 				// 初始时，live thread 等于 min thread
		pool->queue_size = 0;							// 刚开始没有产品
		pool->queue_max_size = queue_max_size;
		pool->queue_front = 0;
		pool->queue_rear = 0;
		pool->shutdown = false;

		// 给最大上限数的工作线程分配内存并请零
		pool->threads = (pthread_t *)malloc(sizeof(pthread_t)*max_thr_num);
		if(pool->threads == NULL)
		{
			printf("malloc threas fail");
			break;
		}
		memset(pool->threads, 0, sizeof(pthread_t)*max_thr_num);

		// 队列开辟空间
		pool->task_queue = (threadpool_task_t *)malloc(sizeof(threadpool_task_t)*queue_max_size);
		if(pool->task_queue == NULL)
		{
			printf("queue malloc failed \n");
			break;
		}

		// 初始化互斥锁和条件变量
		if(pthread_mutex_init(&(pool->lock), NULL) != 0
			|| pthread_mutex_init(&(pool->thread_counter), NULL) != 0
			|| pthread_cond_init(&(pool->queue_not_empty), NULL) != 0
			|| pthread_cond_init(&(pool->queue_not_full), NULL)  != 0)
		{
			printf("init the lock or condition fail");
			break;
		}

		// 启动最小数量的工作线程
		for(i = 0; i <min_thr_num; i++)
		{
			/* 传出参数pthread_t，线程属性、 线程函数、 线程函数参数*/ 
			pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void*)pool);
			printf("start thread 0x%x...\n", (unsigned int)pool->threads[i]);
		}

		// 启动管理者线程
		pthread_create(&(pool->adjust_tid), NULL, adjust_thread, (void*) pool);

		return pool;		//返回一个指向线程池结构体的指针
	}while(0);

	// 前面代码失败则释放所有空间
	threadpool_free(pool);

	return NULL;
}


// 向线程池中添加一个任务
int threadpool_add(threadpool_t *pool, void*(*function)(void* arg), void *arg){
	pthread_mutex_lock(&(pool->lock));		// 对线程池操作时先加锁
	// 当队列满时，调用 pthread_cond_Wait 阻塞等待
	while((pool->queue_size == pool->queue_max_size) && (!pool->shutdown))
	{
		// 等待条件变量时，解锁 mutex：pool -> lock ，被唤醒后，竞争到 mutex 加锁
		pthread_cond_wait((&pool->queue_not_full), &(pool->lock));
	}

	if(pool->shutdown)
		pthread_mutex_unlock(&(pool->lock));

	// 清空工作线程回调函数的参数
	if(pool->task_queue[pool->queue_rear].arg != NULL)
	{
		free(pool->task_queue[pool->queue_rear].arg);
		pool->task_queue[pool->queue_rear].arg = NULL;
	}

	// 向任务队列里添加一个任务，添加到队尾
	pool->task_queue[pool->queue_rear].function = function;
	pool->task_queue[pool->queue_rear].arg = arg;
	pool->queue_rear = (pool->queue_rear + 1) % pool->queue_max_size;
	pool->queue_size++;

	// 添加队列后，任务队列不为空，唤醒等待的工作进程
	pthread_cond_signal(&(pool->queue_not_empty));
	pthread_mutex_unlock(&(pool->lock));

	return 0;
}


// 线程池中的工作线程,参数为线程池指针 pool
void *threadpool_thread(void *threadpool)
{
	threadpool_t *pool = (threadpool_t*)threadpool;
	threadpool_task_t task;

	while(true)
	{// 刚创建线程，等待任务队列里有任务，否则阻塞等待，被唤醒后再接收任务
		pthread_mutex_lock(&(pool->lock));		// 操作前先锁住线程池
		
		while((pool->queue_size == 0) && (!pool->shutdown))
		{// 任务队列里没任务，阻塞等待 queue_not_empty 条件变量
			printf("thread 0x%x is waitting\n", (unsigned int)pthread_self());
			pthread_cond_wait(&(pool->queue_not_empty), &(pool->lock));

			// 清除指定数目的空闲进程
			if(pool->wait_exit_thr_num > 0)
			{
				pool->wait_exit_thr_num--;
				
				// 线程池里线程个数大于最小值时，可以结束当前线程
				if(pool->live_thr_num > pool->min_thr_num)
				{
					printf("thread 0x%x is exiting\n", (unsigned int)pthread_self());
					pool->live_thr_num--;
					pthread_mutex_unlock(&(pool->lock));
					pthread_exit(NULL);
				}
			}	
		}

		// shutdown 为真时，退出所有线程
	if(pool->shutdown)
	{
		pthread_mutex_unlock(&pool->lock);
		printf("thread 0x%x is exiting\n", (unsigned int)pthread_self());
		pthread_exit(NULL);
	}

	// 从任务队列取一个任务，出队操作
	task.function = pool->task_queue[pool->queue_front].function;
	task.arg = pool->task_queue[pool->queue_front].arg;
	pool->queue_front = (pool->queue_front + 1 ) % pool->queue_max_size;
	pool->queue_size--;
	
	// 广播：可以添加新的任务到任务队列
	pthread_cond_broadcast(&(pool->queue_not_full));
	
	pthread_mutex_unlock(&(pool->lock));

	// 工作线程开始执行任务
	printf("i am going to do soming and i am thread: 0x%x\n", (unsigned int)pthread_self());
	pthread_mutex_lock(&(pool->thread_counter));		// 忙线程数 变量锁
	pool->busy_thr_num++;
	pthread_mutex_unlock(&(pool->thread_counter));
	// 执行回调函数任务 task.function(task.arg)等价调用
	(*(task.function))(task.arg);

	// 任务结束处理
	printf("i have done the work and i am 0x%x\n", (unsigned int)pthread_self());
	pthread_mutex_lock(&(pool->thread_counter));
	pool->busy_thr_num--;
	pthread_mutex_unlock(&(pool->thread_counter));
	
	}
	pthread_exit(NULL);

}


// 管理者线程
void *adjust_thread(void *threadpool){
	int i;
	threadpool_t *pool = (threadpool_t *)threadpool;
	while(!pool->shutdown)
	{
		sleep(DEFAULT_TIME);

		pthread_mutex_lock(&(pool->lock));
		int queue_size = pool->queue_size;
		int live_thr_num = pool->live_thr_num;
		pthread_mutex_unlock(&(pool->lock));

		pthread_mutex_lock(&(pool->thread_counter));
		int busy_thr_num =pool->busy_thr_num;
		pthread_mutex_unlock(&(pool->thread_counter));

		// 满足一定条件时，增加线程数
		if(queue_size >= MIN_WAIT_TASK_NUM && live_thr_num < pool->max_thr_num)	
		{
			pthread_mutex_lock(&(pool->lock));
			int add = 0;
			// 一次增加 defult_thread 个线程
			for(i = 0; i < pool->max_thr_num && add < DEFAULT_THREAD_VARY
				&& pool->live_thr_num < pool->max_thr_num; i++ )
			{
				if(pool->threads[i] == 0 || !is_thread_alive(pool->threads[i]))
				{
					pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void*)pool);
					add++;
					pool->live_thr_num++;
				}
			}
			pthread_mutex_unlock(&(pool->lock));
		}

		// 销毁多余的线程数
		if((busy_thr_num *2) <live_thr_num && live_thr_num > pool->min_thr_num)
		{
			pthread_mutex_lock(&(pool->lock));
			pool->wait_exit_thr_num = DEFAULT_THREAD_VARY;
			pthread_mutex_unlock(&(pool->lock));

			for(i = 0; i < DEFAULT_THREAD_VARY; i++)
			{
				// 通知处于空闲状态的线程，它们会自己终止
				pthread_cond_signal(&(pool->queue_not_empty));
			}
		}
		
	}
	return NULL;
}


// 销毁线程池
int threadpool_destroy(threadpool_t *pool){
	int i;
	if(pool == NULL)
		return -1;
	
	pool->shutdown = true;
	// 等待指定的线程结束，回收资源
	pthread_join(pool->adjust_tid, NULL);
	
	for(i = 0; i < pool->live_thr_num; i++)
	{//	通知所有空闲线程
		pthread_cond_broadcast(&(pool->queue_not_empty));
	}

	for(i = 0; i< pool->live_thr_num; i++)
		pthread_join(pool->threads[i],NULL);

	threadpool_free(pool);

	return 0;
}


// 释放线程池的资源
int threadpool_free(threadpool_t *pool){
	if(pool == NULL)
		return -1;
	
	if(pool->task_queue)
		free(pool->task_queue);

	if(pool->threads)
	{
		free(pool->threads);
		pthread_mutex_lock(&(pool->lock));
		pthread_mutex_destroy(&(pool->lock));
		pthread_mutex_lock(&(pool->thread_counter));
		pthread_mutex_destroy(&(pool->thread_counter));
		pthread_cond_destroy(&(pool->queue_not_empty));
		pthread_cond_destroy(&(pool->queue_not_full));
	}
	free(pool);
	pool = NULL;
	return 0;
}


// 统计所有线程数量
int threadpool_all_threadnum(threadpool_t *pool){
	int all_threadnum = -1;
	pthread_mutex_lock(&(pool->lock));
	all_threadnum = pool->live_thr_num;
	pthread_mutex_unlock(&(pool->lock));
	return all_threadnum;
}

// 统计忙线程数量
int threadpool_busy_threadnum(threadpool_t *pool){
	int busy_threadnum = -1;
	pthread_mutex_lock(&(pool->lock));
	busy_threadnum = pool->busy_thr_num;
	pthread_mutex_unlock(&(pool->lock));
	return busy_threadnum;
}

int is_thread_alive(pthread_t tid){
	int kill_rc = pthread_kill(tid, 0); 		// 发送一个 0号信号测试
	if(kill_rc == ESRCH)
		return false;
	return true;
}


// 测试程序
#if 1
// 线程的回调处理函数
void *process(void *arg){
	printf("thread 0x%x working on the task %d\n", (unsigned int)pthread_self(), *(int*)arg);
	sleep(1);
	printf("task %d is end", *(int*)arg);
	
	return NULL;
}



int main(){
	//	创建一个线程池
	threadpool_t *thp = threadpool_create(3,20,20);
	printf("thread init\n");

	int num[20];
	int k;
	for(k = 0; k < 20; k++)
	{// 模拟添加任务
		num[k] = k;
		printf("add a task %d\n", k);
		threadpool_add(thp, process, (void *)&num[k]);
	}

	sleep(10);
	threadpool_destroy(thp);

	return 0;
}

#endif
