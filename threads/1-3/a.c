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
    int digit = 10;
    char a = 'a';
    char* c = &a;
    struct MyStruct st = {digit, c};
    pthread_t tid;

    int err = pthread_create(&tid, NULL, mythread, &st);
    if (err) {
        printf("main: pthread_create() failed: %s\n", strerror(err));
        return -1;
    }
    pthread_join(tid, NULL);
    
    return 0;
}