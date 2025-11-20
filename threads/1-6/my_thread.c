#include "my_thread.h"
static __thread mythread *mythread_self = NULL;

//sigusr1 for assync threads
static int cancel_handler_installed = 0;

static void mythread_do_cancel(mythread_t thread);


static void cancel_signal_handler(int signo) {
    (void)signo;
    if (mythread_self) {
        mythread_do_cancel(mythread_self);
    } else {
        syscall(SYS_exit, 0);
    }
}

static void mythread_install_cancel_handler(void) {
    if (cancel_handler_installed) {
        return;
    }
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = cancel_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction(SIGUSR1)");
    } else {
        cancel_handler_installed = 1;
    }
}

static void* map_stack(void **out_base){
    size_t total = STACK_SIZE + GUARD_SIZE;
    void *stack = mmap(NULL, total, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED) {
        return NULL;
    }
    if (mprotect((char*)stack + GUARD_SIZE, STACK_SIZE, PROT_READ | PROT_WRITE) != 0){
        int e = errno;
        munmap(stack, total);
        errno = e;
        return NULL;
    }
    if (out_base)
        *out_base = stack;
    return (char*)stack + total;
}

static int start_routine_wrapper(void *arg){
    mythread *t = (mythread*)arg;
    void *rv = NULL;

    mythread_self = t;


    t->cancelationState = MYTHREAD_CANCEL_ENABLE;
    t->cancelationType  = MYTHREAD_CANCEL_DEFERRED;
    t->isCanceled       = 0;
    t->cleanupHandlers  = NULL;

    if (t && t->start_routine)
        rv = t->start_routine(t->arg);

    if (t && t->retval)
        *(t->retval) = rv;

    atomic_store((_Atomic int*)&t->isFinished, 1);
    syscall(SYS_exit, 0);
    return 0;
}

int mythread_create(mythread_t thread, void *(*start_routine)(void*), void *arg) {
    if (thread == NULL || start_routine == NULL) {
        return EINVAL;
    }

    mythread_install_cancel_handler();
    thread->start_routine = start_routine;
    thread->arg = arg;
    thread->isFinished = 0;
    thread->isJoined = 0;
    thread->stack_size = STACK_SIZE;

    thread->isCanceled      = 0;
    thread->cancelationState = MYTHREAD_CANCEL_ENABLE;
    thread->cancelationType  = MYTHREAD_CANCEL_DEFERRED;
    thread->cleanupHandlers  = NULL;

    thread->retval = (void**)malloc(sizeof(void*));
    if (!thread->retval){
        perror("mythread_create: malloc retval");
        return ENOMEM;
    }
    *(thread->retval) = NULL;

    void *stack_base = NULL;
    void *child_sp = map_stack(&stack_base);
    if (!child_sp) {
        perror("mythread_create: mmap stack");
        free(thread->retval);
        thread->retval = NULL;
        return errno ? errno : ENOMEM;
    }
    thread->stack = stack_base;

    int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND
              | CLONE_THREAD | CLONE_SYSVSEM
              | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;

    int tid = clone(start_routine_wrapper, child_sp, flags, thread, &thread->tid, NULL, &thread->tid);
    if (tid == -1) {
        fprintf(stderr, "mythread_create: clone() failed: %s\n", strerror(errno));
        munmap(thread->stack, STACK_SIZE + GUARD_SIZE);
        free(thread->retval); 
        thread->retval = NULL;
        return errno ? errno : -1;
    }
    thread->tid = tid;
    return 0;
}


int mythread_join(mythread_t thread, void** retval) {
    if (!thread || thread->isJoined)
        return EINVAL;

    while (atomic_load((_Atomic int*)&thread->isFinished) != 1) {
        usleep(10);
    }

    if (retval) {
        *retval = thread->retval ? *(thread->retval) : NULL;
    }

    if (thread->stack) {
        munmap(thread->stack, STACK_SIZE + GUARD_SIZE);
        thread->stack = NULL;
    }
    if (thread->retval) {
        free(thread->retval);
        thread->retval = NULL;
    }

    thread->isJoined = 1;
    thread->tid = 0;
    return 0;
}

static void mythread_do_cancel(mythread_t thread) {
    if (thread == NULL) {
        return;
    }
    if (!thread->isCanceled) {
        return;
    }
    if (thread->cancelationState == MYTHREAD_CANCEL_DISABLE) {
        return;
    }
    mythread_cleanup_t *handler = thread->cleanupHandlers;
    while (handler) {
        mythread_cleanup_t *next = handler->next;
        if (handler->routine) {
            handler->routine(handler->arg);
        }
        free(handler);
        handler = next;
    }

    thread->cleanupHandlers = NULL;
    if (thread->retval) {
        *(thread->retval) = MYTHREAD_CANCELED;
    }
    atomic_store((_Atomic int*)&thread->isFinished, 1);
    syscall(SYS_exit, 0);
}

int mythread_cancel(mythread_t thread) {
    if (thread == NULL) {
        return EINVAL;
    }
    if (thread->isFinished) {
        return ESRCH;
    }
    thread->isCanceled = 1;
    if (thread->cancelationState == MYTHREAD_CANCEL_DISABLE) {
        return 0;
    }
    if (thread->cancelationType == MYTHREAD_CANCEL_DEFERRED)
        return 0;

    if (thread->cancelationType == MYTHREAD_CANCEL_ASYNCHRONOUS) {
        pid_t pid = getpid();
        int rc = syscall(SYS_tgkill, pid, thread->tid, SIGUSR1);
        if (rc != 0) {
            return errno;
        }
    }
    return 0;
}


void mythread_testcancel(void) {
    mythread *t = mythread_self;
    if (!t)
        return;

    if (!t->isCanceled)
        return;

    if (t->cancelationState == MYTHREAD_CANCEL_DISABLE)
        return;

    if (t->cancelationType != MYTHREAD_CANCEL_DEFERRED)
        return;

    mythread_do_cancel(t);
}

int mythread_setcancelstate(int state, int *oldstate) {
    mythread *t = mythread_self;
    if (!t)
        return EINVAL;

    if (state != MYTHREAD_CANCEL_ENABLE &&
        state != MYTHREAD_CANCEL_DISABLE) {
        return EINVAL;
    }
    if (oldstate)
        *oldstate = t->cancelationState;

    t->cancelationState = (mythread_cancel_state_t)state;
    return 0;
}

int mythread_setcanceltype(int type, int *oldtype) {
    mythread *t = mythread_self;
    if (!t)
        return EINVAL;

    if (type != MYTHREAD_CANCEL_DEFERRED &&
        type != MYTHREAD_CANCEL_ASYNCHRONOUS) {
        return EINVAL;
    }

    if (oldtype)
        *oldtype = t->cancelationType;

    t->cancelationType = (mythread_cancel_type_t)type;
    return 0;
}

void mythread_cleanup_push(void (*routine)(void *), void *arg) {
    mythread *t = mythread_self;
    if (!t) 
        return;

    mythread_cleanup_t *c = malloc(sizeof(*c));
    if (!c) {
        return;
    }

    c->routine = routine;
    c->arg     = arg;
    c->next    = t->cleanupHandlers;

    t->cleanupHandlers = c;
}

void mythread_cleanup_pop(int execute) {
    mythread *t = mythread_self;
    if (!t) return;

    mythread_cleanup_t *c = t->cleanupHandlers;
    if (!c) 
        return;

    t->cleanupHandlers = c->next;

    if (execute && c->routine) {
        c->routine(c->arg);
    }

    free(c);
}