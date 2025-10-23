#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

void *my_thread(void *arg) {
    int i = 0;
    while(1) {
        printf("my_thread: iteration %d", i);
        i++;
        sleep(1);
    }
    return NULL;
}

int main(void) {
    pthread_t t;
    if (pthread_create(&t, NULL, my_thread, NULL) != 0) {
        perror("pthread_create");
        return 1;
    }

    sleep(3);
    printf("main: cancel");
    if (pthread_cancel(t) != 0) {
        perror("pthread_cancel");
        return 1;
    }

    void *res;
    if (pthread_join(t, &res) != 0) {
        perror("pthread_join");
        return 1;
    }

    if (res == PTHREAD_CANCELED) {
        printf("main: thread was canceled (PTHREAD_CANCELED)");
    } else {
        printf("main: thread returned %p", res);
    }
    return 0;
}