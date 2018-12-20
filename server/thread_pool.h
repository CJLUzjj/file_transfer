#ifndef THREAD_POOL_H__
#define THREAD_POOL_H__

#include <pthread.h>

//任务节点
typedef struct tpool_work {
    void*               (*routine)(void*);      //任务函数
    void                *arg;                   //任务函数的参数
    struct tpool_work   *next; 
}tpool_work;

//thread pool
typedef struct tpool {
    int             shutdown;                   //判断线程是否销毁
    int             max_num;                    //最大线程数
    pthread_t       *thread_id;                 //thread id adress
    tpool_work      *quene_head;                //work list head
    tpool_work      *quene_tail;                //work list tail
    pthread_mutex_t quene_lock;                 //用于保护队列
    pthread_cond_t  quene_ready;                //用于队列为空时等待
}tpool_t;

//creat tpool
int tpool_creat(int num);

//destroy tpool
void tpool_destroy();

//add work in tpool
int tpool_add_work(void*(*routine)(void *), void *arg);

#endif
