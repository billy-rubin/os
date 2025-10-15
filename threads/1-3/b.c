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

void* mythread(struct MyStruct* arg) {
    printf("mythread [%d %d %d %ld]: Hello from mythread!\n", getpid(), getppid(), gettid(), pthread_self());
    printf("Int: %d \t Char: %s\n", arg->x, arg->symb);
    return NULL;
}

int main() {
    pthread_t tid;
    MyStruct *st = malloc(sizeof(MyStruct));
    st->x = 10;
    st->symb = "a";

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&tid, &attr, mythread, st);
    pthread_attr_destroy(&attr);

    sleep(1);

    free(st);
    return 0;
}