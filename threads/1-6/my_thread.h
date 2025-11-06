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

typedef struct mythread {
    int tid;
    void* arg;
    void** retval;
    void* stack;
    void* (*start_routine)(void*);
    size_t stack_size;
    int isFinished;
    int isJoined;
} mythread;

typedef mythread* mythread_t;

int mythread_create(mythread_t thread, void *(*start_routine)(void*), void *arg);
int mythread_join  (mythread_t thread, void** retval);
int mythread_cancel(mythread_t thread);

#endif