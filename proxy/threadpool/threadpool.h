#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>

typedef void (*threadTaskFn)(void *);

typedef struct threadTask {
    threadTaskFn fn;
    void *arg;
    struct threadTask *next;
} threadTask_t;

typedef struct threadPool {
    int stop;
    int num_threads;
    pthread_t *threads;
    
    threadTask_t *head;
    threadTask_t *tail;

    pthread_mutex_t lock;
    pthread_cond_t  isBusy;
} threadPool_t;

threadPool_t *threadpoll_init(int num_threads);
int           threadpool_submit_task(threadPool_t *pool, threadTaskFn fn, void *arg);
void          threadpool_stop(threadPool_t *pool);

#endif