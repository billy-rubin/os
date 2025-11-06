#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>

typedef struct MyStruct {
    int x;
    char *symb;
} MyStruct;

static pid_t gettid_syscall(void) { return (pid_t)syscall(SYS_gettid); }

void* mythread(void* arg) {
    int a = 5;
    MyStruct *s = (MyStruct*)arg;
    printf("mythread [%d %d %d %lu]: Hello from mythread!\n",
           getpid(), getppid(), gettid_syscall(), (unsigned long)pthread_self());
    s->symb = "b";
    printf("MYTHREAD Int: %d \t Char: %s\n", s->x, s->symb);

    int *res = malloc(sizeof(int));
    if (!res) {
        perror("malloc");
        pthread_exit(NULL);
    }
    *res = a;
    pthread_exit(res);
}

int main(void) {
    MyStruct st = { 42, "a" };
    pthread_t tid;

    if (pthread_create(&tid, NULL, mythread, &st) != 0) {
        perror("pthread_create");
        return 1;
    }

    void *retval;
    if (pthread_join(tid, &retval) != 0) {
        perror("pthread_join");
        return 1;
    }

    if (retval != NULL) {
        int got = *(int*)retval;
        printf("main: got value = %d\n", got);
        free(retval);
    } else {
        printf("main: thread returned NULL\n");
    }

    return 0;
}