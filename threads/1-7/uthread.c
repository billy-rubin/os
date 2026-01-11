#define _GNU_SOURCE
#include "uthread.h"

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

static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static uthread_t ready_head = NULL;
static uthread_t ready_tail = NULL;
static pthread_t workers[NUM_WORKERS];
static bool initialized = false;

static __thread uthread_t current_thread = NULL;
static __thread ucontext_t worker_ctx;

static void enqueue_ready(uthread_t t) {
    pthread_mutex_lock(&queue_mutex);
    t->next = NULL;
    t->state = UT_READY;
    if (ready_tail) {
        ready_tail->next = t;
        ready_tail = t;
    } else {
        ready_head = ready_tail = t;
    }
    pthread_mutex_unlock(&queue_mutex);
}

static uthread_t dequeue_ready(void) {
    pthread_mutex_lock(&queue_mutex);
    if (!ready_head) {
        pthread_mutex_unlock(&queue_mutex);
        return NULL;
    }
    uthread_t t = ready_head;
    ready_head = t->next;
    if (!ready_head) 
        ready_tail = NULL;
    t->next = NULL;
    pthread_mutex_unlock(&queue_mutex);
    return t;
}

static void thread_trampoline(void) {
    uthread_t self = current_thread;
    void *ret = self->start_routine(self->arg);
    
    self->retval = ret;
    
    pthread_mutex_lock(&queue_mutex);
    self->state = UT_FINISHED;
    if (self->joiner) {
        self->joiner->state = UT_READY;
        
        self->joiner->next = NULL;
        if (ready_tail) {
            ready_tail->next = self->joiner;
            ready_tail = self->joiner;
        } else {
            ready_head = ready_tail = self->joiner;
        }
        self->joiner = NULL;
    }
    pthread_mutex_unlock(&queue_mutex);

    setcontext(&worker_ctx);
}

static void *worker_routine(void *arg) {
    (void)arg;
    while (true) {
        uthread_t t = dequeue_ready();
        if (t) {
            current_thread = t;
            t->state = UT_RUNNING;
            swapcontext(&worker_ctx, &t->ctx);
            current_thread = NULL;
        } else {
            sched_yield();
        }
    }
    return NULL;
}

static void uthread_init(void) {
    if (initialized) 
        return;
    initialized = true;
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_create(&workers[i], NULL, worker_routine, NULL);
    }
}

int uthread_create(uthread_t *thread, void *(*start_routine)(void *), void *arg) {
    if (!initialized) 
        uthread_init();

    uthread_t t = (uthread_t)calloc(1, sizeof(struct uthread));
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
    if (current_thread) {
        uthread_t self = current_thread;
        enqueue_ready(self);
        swapcontext(&self->ctx, &worker_ctx);
    } else {
        sched_yield();
    }
}

int uthread_join(uthread_t thread, void **retval) {
    if (!thread) 
        return -1;

    if (current_thread) {
        if (thread->state != UT_FINISHED) {
            pthread_mutex_lock(&queue_mutex);
            if (thread->state != UT_FINISHED) {
                current_thread->state = UT_BLOCKED;
                thread->joiner = current_thread;
                pthread_mutex_unlock(&queue_mutex);
                swapcontext(&current_thread->ctx, &worker_ctx);
            } else {
                pthread_mutex_unlock(&queue_mutex);
            }
        }
    } else {
        while (thread->state != UT_FINISHED) {
            usleep(1000);
        }
    }

    if (retval) 
        *retval = thread->retval;

    if (thread->stack) 
        free(thread->stack);
    free(thread);
    
    return 0;
}