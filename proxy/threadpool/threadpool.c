#include "threadpool.h"

#include <stdlib.h>
#include <stdio.h>

static void *workerLoop(void *arg);

threadPool_t *CreateThreadPool(int num_threads) {
    if (num_threads <= 0) 
        return NULL;
    
    threadPool_t *pool = calloc(1, sizeof(*pool));
    if (!pool) 
        return NULL;
    
    pool->num_threads = num_threads;
    pool->threads = calloc(num_threads, sizeof(pthread_t));
    if (!pool->threads) {
        free(pool);
        return NULL;
    }

    pool->head = NULL;
    pool->tail = NULL;
    pool->stop = 0;

    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        free(pool->threads);
        free(pool);
        return NULL;
    }

    if (pthread_cond_init(&pool->isBusy, NULL) != 0) {
        pthread_mutex_destroy(&pool->lock);
        free(pool->threads);
        free(pool);
        return NULL;
    }

    for (int i = 0; i < num_threads; ++i) {
        if (pthread_create(&pool->threads[i], NULL, workerLoop, pool) != 0) {
            pool->stop = 1;
            pthread_cond_broadcast(&pool->isBusy);

            for (int j = 0; j < i; ++j) 
                pthread_join(pool->threads[j], NULL);
            
            pthread_cond_destroy(&pool->isBusy);
            pthread_mutex_destroy(&pool->lock);
            free(pool->threads);
            free(pool);
            return NULL;
        }
    }

    return pool;
}

int SubmitTask(threadPool_t *pool, threadTaskFn fn, void *arg) {
    if (!pool || !fn) 
        return -1;

    threadTask_t *task = malloc(sizeof(*task));
    if (!task) 
        return -1;
    
    task->fn = fn;
    task->arg = arg;
    task->next = NULL;

    pthread_mutex_lock(&pool->lock);

    if (pool->stop) {
        pthread_mutex_unlock(&pool->lock);
        free(task);
        return -1;
    }

    if (pool->tail) {
        pool->tail->next = task;
        pool->tail = task;
    } 
    else 
        pool->head = pool->tail = task;
    
    pthread_cond_signal(&pool->isBusy);
    pthread_mutex_unlock(&pool->lock);

    return 0;
}

void StopThreadPool(threadPool_t *pool) {
    if (!pool) 
        return;

    pthread_mutex_lock(&pool->lock);
    pool->stop = 1;
    pthread_cond_broadcast(&pool->isBusy);
    pthread_mutex_unlock(&pool->lock);

    for (int i = 0; i < pool->num_threads; ++i) 
        pthread_join(pool->threads[i], NULL);
    
    threadTask_t *cur = pool->head;
    while (cur) 
    {
        threadTask_t *next = cur->next;
        free(cur);
        cur = next;
    }

    pthread_cond_destroy(&pool->isBusy);
    pthread_mutex_destroy(&pool->lock);
    free(pool->threads);
    free(pool);
}

static void *workerLoop(void *arg) {
    threadPool_t *pool = (threadPool_t *)arg;

    while (1) {
        pthread_mutex_lock(&pool->lock);
        while (!pool->stop && pool->head == NULL) 
            pthread_cond_wait(&pool->isBusy, &pool->lock);
        
        if (pool->stop && pool->head == NULL) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }

        threadTask_t *task = pool->head;
        pool->head = task->next;
        if (pool->head == NULL) 
            pool->tail = NULL;
        
        pthread_mutex_unlock(&pool->lock);

        task->fn(task->arg);
        free(task);
    }

    return NULL;
}