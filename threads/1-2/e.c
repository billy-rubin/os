#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

void* mythread(void* arg) {
    pthread_detach(pthread_self());
    printf("mythread [%d %d %d %ld]: Hello from mythread!\n", getpid(), getppid(), gettid(), pthread_self());
    return NULL;

}

int main(void) {
    while (1) {
        pthread_t tid;
        pthread_create(&tid, NULL, mythread, NULL);
        usleep(1000);
    }
    return 0;
}