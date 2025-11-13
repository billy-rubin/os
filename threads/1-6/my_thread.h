#ifndef MYTHREAD_H
#define MYTHREAD_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <signal.h>
#include <errno.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define GUARD_SIZE 4096
#define STACK_SIZE 8388608

#define MYTHREAD_CANCELED ((void*)-1)

typedef enum mythread_cancel_state{
    MYTHREAD_CANCEL_ENABLE,
    MYTHREAD_CANCEL_DISABLE
} mythread_cancel_state_t;

typedef enum mythread_cancel_type{
    MYTHREAD_CANCEL_DEFERRED,
    MYTHREAD_CANCEL_ASYNCHRONOUS
} mythread_cancel_type_t;

typedef struct mythread_cleanup {
    void (*routine)(void *);
    void *arg;
    struct mythread_cleanup *next;
} mythread_cleanup_t;

typedef struct mythread {
    int tid;
    void* arg;
    void** retval;
    void* stack;
    void* (*start_routine)(void*);
    size_t stack_size;
    int isFinished;
    int isJoined;
    int isCanceled;
    mythread_cancel_state_t cancelationState; //enable, disable
    mythread_cancel_type_t cancelationType; //deffered, assync
    mythread_cleanup_t *cleanupHandlers;
} mythread;

typedef mythread* mythread_t;

int mythread_create(mythread_t thread, void *(*start_routine)(void*), void *arg);
int mythread_join  (mythread_t thread, void** retval);
int mythread_cancel(mythread_t thread);
void mythread_testcancel(void);
int mythread_setcancelstate(int state, int *oldstate);
int mythread_setcanceltype (int type,  int *oldtype);
#endif