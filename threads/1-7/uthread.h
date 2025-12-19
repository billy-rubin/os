#ifndef UTHREAD_H
#define UTHREAD_H

#include <stdlib.h>
#include <ucontext.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

typedef struct uthread *uthread_t;

#define UTHREAD_STACK_SIZE (64 * 1024)
#define NUM_WORKERS 4

typedef enum {
    UT_READY,
    UT_RUNNING,
    UT_BLOCKED,
    UT_FINISHED
} uthread_state_t;

int uthread_create(uthread_t *thread, void *(*start_routine)(void *), void *arg);
void uthread_yield(void);
int uthread_join(uthread_t thread, void **retval);

#endif