/*==============================================
* FileName    : threadpool.h
* Author      : liming
* Create Time : 2019-05-29
* description : 线程池的头文件 
==============================================*/

#ifndef __THREADPOOL_H
#define __THREADPOOL_H

typedef struct threadpool_t;

threadpool_t *threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size);

int threadpool_add(threadpool_t *pool, void*(*function)(void *), void *arg);

int threadpool_destroy(threadpool_t *pool);

int threadpool_all_threadnum(threadpool_t *pool);

int threadpool_busy_threadnum(threadpool_t *pool);





#endif
