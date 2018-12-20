#include "thread_pool.h"
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

static tpool_t *pool = NULL;

//工作线程，static防止其他文件调用
static void* thread_routine(void *arg){
    tpool_work *work;

    while(1){
        //自旋锁，用于抢夺工作队列
        pthread_mutex_lock(&pool->quene_lock);
        //队列为空且线程池未关闭
        while(!pool->quene_head && !pool->shutdown){
            pthread_cond_wait(&pool->quene_ready, &pool->quene_lock);
        }

        if(pool->shutdown){
            pthread_mutex_unlock(&pool->quene_lock);
            pthread_exit(NULL);
        }

        work = pool->quene_head;
        pool->quene_head = pool->quene_head->next;
        pthread_mutex_unlock(&pool->quene_lock);
        //执行工作函数
        work->routine(work->arg);
        //释放任务
        free(work->arg);
        free(work->routine);
    }
    return NULL;
}

//创建线程池
int tpool_creat(int num){
    pool = calloc(1, sizeof(tpool_t));
    if(!pool){
        printf("calloc pool failed!\n");
        exit(1);
    }

    //初始化
    pool->shutdown = 0;
    pool->max_num = num;
    pool->quene_head = NULL;
    pool->quene_tail = NULL;
    if(pthread_mutex_init(&pool->quene_lock, NULL) !=0){
        printf("pthread_mutex_init failed!\n");
        exit(-1);
    }
    if(pthread_cond_init(&pool->quene_ready, NULL) !=0){
        printf("pthread_cond_init failed!\n");
        exit(-1);
    }

    pool->thread_id = calloc(num, sizeof(pthread_t));
    if(!pool->thread_id){
        printf("calloc thread_id failed!\n");
        exit(1);
    }
    for(int i=0;i<num;i++){
        if(pthread_create(&pool->thread_id[i], NULL, thread_routine, NULL) != 0){
            printf("pool->thread_id[%d] pthread_cread failed!\n");
            exit(-1);
        }
    }
    return 0;
}

void tpool_destroy(){
    tpool_work *temp;
    if(pool->shutdown){
        return;
    }
    pool->shutdown = 1;

    pthread_mutex_lock(&pool->quene_lock);
    pthread_cond_broadcast(&pool->quene_ready);
    pthread_mutex_unlock(&pool->quene_lock);

    for(int i = 0;i < pool->max_num; ++i){
        pthread_join(pool->thread_id[i], NULL);
    }

    free(pool->thread_id);

    while(pool->quene_head){
        temp = pool->quene_head;
        pool->quene_head = pool->quene_head->next;
        free(temp->arg);
        free(temp);
    }

    pthread_mutex_destroy(&pool->quene_lock);
    pthread_cond_destroy(&pool->quene_ready);

    free(pool);
}

int tpool_add_work(void*(*routine)(void *), void *arg){
    tpool_work *work;

    if(!routine){
        printf("Invalid argument!\n");
        return -1;
    }

    work = malloc(sizeof(tpool_work));
    if(!work){
        printf("malloc work failed!\n");
        return -1;
    }
    work->routine = routine;
    work->arg = arg;
    work->next = NULL;

    pthread_mutex_lock(&pool->quene_lock);
    if(!pool->quene_head){
        pool->quene_head = work;
        pool->quene_tail = work;
    }else{
        pool->quene_tail->next = work;
        pool->quene_tail = work;
    }
    
    pthread_cond_signal(&pool->quene_ready);
    pthread_mutex_unlock(&pool->quene_lock);
    return 0;
}

