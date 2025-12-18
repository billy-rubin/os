#ifndef UTHREAD_H
#define UTHREAD_H

#include <ucontext.h>

#define UTHREAD_STACK_SIZE (64 * 1024)

typedef struct uthread uthread_t;

typedef enum {
    UT_READY,
    UT_RUNNING,
    UT_BLOCKED,
    UT_FINISHED,
} uthread_state_t;


int uthread_create(uthread_t **thread, void *(*start_routine)(void *), void *arg);


int uthread_join(uthread_t *thread, void **retval);

void uthread_init(void);

void uthread_yield(void);

#endif