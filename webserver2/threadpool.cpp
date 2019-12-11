#include "threadpool.h"

// 创建线程池
threadpool_t *threadpool_create(int thread_count, int queue_size, int flags)
{
    // 线程池指针
    threadpool_t* pool;
    int i;

    do
    {
        // MAX_THREADS = 1024;  实际传的 thread_count = 4;      MAX_QUEUE = 65535
        if(thread_count <= 0 || thread_count > MAX_THREADS || queue_size <= 0 || queue_size > MAX_QUEUE)
        {
            return NULL;
        }
        // 线程池创建错误
        if((pool = (threadpool_t*) malloc (sizeof(threadpool_t))) == NULL)
        {
            break;
        }
        
        // 初始化线程池相关信息
        pool->thread_count = 0;
        pool->queue_size = queue_size;
        pool->head = pool->tail = pool->count = 0;
        pool->shutdown = pool->started = 0;

        // 线程和任务队列分配空间
        pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * thread_count);
        pool->queue = (threadpool_task_t*)malloc(sizeof(threadpool_task_t) * queue_size);

        // 初始化互斥锁和信号量：可以动态初始化和静态初始化（需要声明为全局静态变量）
        if((pthread_mutex_init(&(pool->lock), NULL) != 0) || (pthread_cond_init(&(pool->notify), NULL)) != 0
            || (pool->threads == NULL) || (pool->queue == NULL))
        {
            break;
        }

        // 启动 thread_count 个线程
        for(i = 0; i < thread_count; i++)
        {
            if(pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void*)pool) != 0)
            {
                threadpool_destroy(pool, 0);
                return NULL;
            }
            pool->thread_count++;
            pool->started++;
        }
        return pool;
    } while (false);

    if(pool != NULL)
    {
        threadpool_free(pool);
    }
    return NULL;
}

// 向线程池中的任务队列中添加任务、并将队列的 tail 指针指向尾部， 任务数加 1
int threadpool_add(threadpool_t *pool, void(*function)(void *), void *argument, int flags)
{
    int err = 0;
    int next;
    if(pool == NULL || function == NULL)
    {
        return THREADPOOL_INVALID;
    }

    // 给线程池加锁
    if(pthread_mutex_lock(&(pool->lock)) != 0)
    {
        return THREADPOOL_LOCK_FAILURE;
    }

    next = (pool->tail + 1) % pool->queue_size;
    do
    {
        if(pool->count == pool->queue_size)
        {
            err = THREADPOOL_QUEUE_FULL;
            break;
        }

        if(pool->shutdown)
        {
            err = THREADPOOL_SHUTDOWN;
            break;
        }

        // 向线程池中添加任务
        pool->queue[pool->tail].function = function;
        pool->queue[pool->tail].argument = argument;
        pool->tail = next;
        pool->count += 1;

        // 唤醒至少一个阻塞在条件变量上的线程
        if(pthread_cond_signal(&(pool->notify)) != 0)
        {
            err = THREADPOOL_LOCK_FAILURE;
            break;
        }
    } while (false);

    if(pthread_mutex_unlock(&pool->lock) != 0)
    {
        err = THREADPOOL_LOCK_FAILURE;
    }
    return err;
}

// 销毁线程池
int threadpool_destroy(threadpool_t *pool, int flags)
{
    printf("Threadpool destroy !\n");
    int i, err = 0;
    if(pool == NULL)
    {
        return THREADPOOL_INVALID;
    }

    if(pthread_mutex_lock(&(pool->lock)) != 0)
    {
        return THREADPOOL_LOCK_FAILURE;
    }
    do
    {
        if(pool->shutdown)
        {
            err = THREADPOOL_SHUTDOWN;
            break;
        }

        // 根据 flag 选择关闭方式
        pool->shutdown = (flags & THREADPOOL_GRACEFUL) ? graceful_shutdown : immediate_shutdown;
        // 唤醒线程
        if((pthread_cond_broadcast(&(pool->notify)) != 0) || (pthread_mutex_unlock(&(pool->lock)) != 0))
        {
            err = THREADPOOL_LOCK_FAILURE;
            break;
        }
        // 回收所有工作进程
        for(i = 0; i < pool->thread_count; ++i)
        {
            if(pthread_join(pool->threads[i], NULL) != 0)
            {
                err = THREADPOOL_THREAD_FAILURE;
            }
        }
    } while (false);
    
    if(!err)
    {
        threadpool_free(pool);
    }
    return err;
}

// 释放线程池资源
int threadpool_free(threadpool_t *pool)
{
    if(pool == NULL || pool->started > 0)
    {
        return -1;
    }
    // 判断是否已回收
    if(pool->threads)
    {
        free(pool->threads);
        free(pool->queue);
        pthread_mutex_lock(&(pool->lock));
        pthread_mutex_destroy(&(pool->lock));
        pthread_cond_destroy(&(pool->notify));
    }
    free(pool);
    return 0;
}

// 线程函数、即线程生成时的回调函数
static void *threadpool_thread(void *threadpool)
{
    threadpool_t* pool = (threadpool_t *) threadpool;
    threadpool_task_t task;

    for(;;)
    {
        pthread_mutex_lock(&(pool->lock));
        while ((pool->count == 0) && (!pool->shutdown))
        {
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }
        if((pool->shutdown == immediate_shutdown) || ((pool->shutdown == graceful_shutdown) && (pool->count == 0)))
            break;
        
        // 开始执行任务
        task.function = pool->queue[pool->head].function;
        task.argument = pool->queue[pool->head].argument;

        pool->head = (pool->head + 1) % pool->queue_size;

        pool->count -= 1;
        pthread_mutex_unlock(&(pool->lock));

        (*(task.function))(task.argument);
    }
    -- pool->started;
    pthread_mutex_unlock(&(pool->lock));
    pthread_exit(NULL);
    return(NULL);
}