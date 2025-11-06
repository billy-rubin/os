#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct MyStruct {
    int x;
    char *symb;
}MyStruct;

void* mythread(void* arg) {
    MyStruct *s = (MyStruct*)arg;
    printf("mythread [%d %d %d %ld]: Hello from mythread!\n", getpid(), getppid(), gettid(), pthread_self());
    printf("Int: %d \t Char: %s\n", s->x, s->symb);
    free(arg);
    pthread_exit(0);
}

int main() {
    pthread_t tid;
    MyStruct *st = malloc(sizeof(MyStruct));
    int err;
    st->x = 10;
    st->symb = "a";

    pthread_attr_t attr;
    err = pthread_attr_init(&attr);

    if (err != 0) {
        printf("err initializing attr");
        return -1;
    }
    
    err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (err != 0) {
        printf("error attr wasn't set to detached");
        return -1;
    }

    err = pthread_create(&tid, &attr, mythread, st);
    if (err != 0) {
        printf("main: pthread_create() failed: %s\n", strerror(err));
        return -1;
    }

    if (pthread_attr_destroy(&attr) != 0) {
        printf("error attr didn't destroy");
        return -1;
    }
    pthread_exit(0);
}