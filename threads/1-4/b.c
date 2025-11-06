#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>


void *my_thread(void *arg) {
    pthread_detach(pthread_self());
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    int i = 0;
    while (1) {
        i++;
    }
    printf("my_thread: loop finished, i=%d\n", i);
    return NULL;
}

int main(void) {
    pthread_t t;
    if (pthread_create(&t, NULL, my_thread, NULL) != 0) {
        perror("pthread_create");
        return 1;
    }

    sleep(1);
    printf("Main: sending cancellation request\n");

    if (pthread_cancel(t) != 0) {
        perror("pthread_cancel");
    }

    // void* res;

    // // if (pthread_join(t, &res) != 0) {
    // //     perror("pthread_join");
    // // }

    // if (res == PTHREAD_CANCELED) {
    //     printf("Main: thread was canceled (PTHREAD_CANCELED)\n");
    // }

    // free(res);

    printf("Main done\n");
    pthread_exit(0);
}