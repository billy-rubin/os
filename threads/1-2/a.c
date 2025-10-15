#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

void* mythread(void* arg) {
    
    printf("mythread [%d %d %d %ld]: Hello from mythread!\n", getpid(), getppid(), gettid(), pthread_self());
    return NULL;

}

int main(void) {
    
    pthread_t tid;
    if (pthread_create(&tid, NULL, mythread, NULL) != 0) {
        perror("pthread_create");
        return 1;
    }
    
    pthread_join(tid, NULL);
    printf("main thread waited for thread to finish\n");

    return 0;
}