#include "my_thread.h"

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

    if (t && t->start_routine)
        rv = t->start_routine(t->arg);

    if (t && t->retval)
        *(t->retval) = rv;

    atomic_store((_Atomic int*)&t->isFinished, 1);
    syscall(SYS_exit, 0);
}

int mythread_create(mythread_t thread, void *(*start_routine)(void*), void *arg) {
    if (thread == NULL || start_routine == NULL) {
        return EINVAL;
    }

    thread->start_routine = start_routine;
    thread->arg = arg;
    thread->isFinished = 0;
    thread->isJoined = 0;
    thread->stack_size = STACK_SIZE;

    thread->retval = (void**)malloc(sizeof(void*));
    if (!thread->retval){
        perror("mythread_create: malloc retval");
        return ENOMEM;
    }
    *(thread->retval) = NULL;

    void *stack_base = NULL;
    void *child_sp = map_stack(&stack_base);
    if (!child_sp) {
        perror("mythread_create: mmap/mprotect stack");
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
        free(thread->retval); thread->retval = NULL;
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

int mythread_cancel(mythread_t thread){
    if (!thread)
        return EINVAL;

    if (thread->isFinished)
        return 0;

    if (thread->tid == 0)
        return 0;

    if (kill(thread->tid, SIGTERM) == -1){
        int e = errno;
        perror("mythread_cancel: kill");
        return e;
    }
    return 0;
}