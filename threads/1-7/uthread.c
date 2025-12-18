#define _GNU_SOURCE
#include "uthread.h"
#include <stdlib.h>
#include <ucontext.h>
#include <stdbool.h>

struct uthread {
    ucontext_t ctx;
    char *stack;
    void *(*start_routine)(void *);
    void *arg;
    void *retval;
    uthread_state_t state;
    struct uthread *joiner;
    struct uthread *next;
};

static uthread_t *main_thread = NULL;
static uthread_t *current_thread = NULL;
static uthread_t *ready_head = NULL;
static uthread_t *ready_tail = NULL;

static void uthread_cleanup_main(void) {
    if (main_thread) {
        free(main_thread);
        main_thread = NULL;
    }
}

static int uthread_init(void) {
    if (main_thread) return 0;

    main_thread = (uthread_t *)calloc(1, sizeof(struct uthread));
    if (!main_thread) return -1;

    if (getcontext(&main_thread->ctx) == -1) {
        free(main_thread);
        main_thread = NULL;
        return -1;
    }

    main_thread->state = UT_RUNNING;
    current_thread = main_thread;
    
    atexit(uthread_cleanup_main);
    return 0;
}

static void enqueue_ready(uthread_t *t) {
    t->next = NULL;
    if (ready_tail) {
        ready_tail->next = t;
        ready_tail = t;
    } else {
        ready_head = ready_tail = t;
    }
}

static uthread_t *dequeue_ready(void) {
    if (!ready_head) 
        return NULL;
    uthread_t *t = ready_head;
    ready_head = t->next;
    if (!ready_head) 
        ready_tail = NULL;
    t->next = NULL;
    return t;
}

static void schedule(void) {
    uthread_t *prev = current_thread;
    uthread_t *next = dequeue_ready();

    if (!next) {
        if (prev->state == UT_FINISHED && prev == main_thread) {
            exit(0);
        }
        return; 
    }

    current_thread = next;
    current_thread->state = UT_RUNNING;

    if (prev->state == UT_RUNNING) {
        prev->state = UT_READY;
        enqueue_ready(prev);
    }

    swapcontext(&prev->ctx, &next->ctx);
}

static void thread_trampoline(void) {
    uthread_t *self = current_thread;
    void *ret = self->start_routine(self->arg);
    
    self->retval = ret;
    self->state = UT_FINISHED;

    if (self->joiner) {
        if (self->joiner->state == UT_BLOCKED) {
            self->joiner->state = UT_READY;
            enqueue_ready(self->joiner);
        }
        self->joiner = NULL;
    }

    schedule();
}

int uthread_create(uthread_t **thread, void *(*start_routine)(void *), void *arg) {
    if (!main_thread) {
        if (uthread_init() != 0) 
            return -1;
    }

    uthread_t *t = (uthread_t *)calloc(1, sizeof(struct uthread));
    if (!t) 
        return -1;

    t->stack = (char *)malloc(UTHREAD_STACK_SIZE);
    if (!t->stack) {
        free(t);
        return -1;
    }

    if (getcontext(&t->ctx) == -1) {
        free(t->stack);
        free(t);
        return -1;
    }

    t->ctx.uc_stack.ss_sp = t->stack;
    t->ctx.uc_stack.ss_size = UTHREAD_STACK_SIZE;
    t->ctx.uc_link = NULL;

    t->start_routine = start_routine;
    t->arg = arg;
    t->state = UT_READY;

    makecontext(&t->ctx, thread_trampoline, 0);

    enqueue_ready(t);

    if (thread) 
        *thread = t;
    return 0;
}

void uthread_yield(void) {
    if (main_thread) 
        schedule();
}

int uthread_join(uthread_t *thread, void **retval) {
    if (!thread || !main_thread || thread == current_thread) 
        return -1;

    if (thread->joiner) 
        return -1;

    while (thread->state != UT_FINISHED) {
        current_thread->state = UT_BLOCKED;
        thread->joiner = current_thread;
        schedule();
    }

    if (retval) *retval = thread->retval;

    if (thread->stack) 
        free(thread->stack);
    free(thread);

    return 0;
}