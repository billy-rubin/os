#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void cleanup(void *arg) {
    char *s = (char*)arg;
    printf("Cleanup: freeing memory\n");
    free(s);
}

void* my_thread(void* arg) {
    char *s = malloc(12);
    strcpy(s, "hello world");
    pthread_cleanup_push(cleanup, s); 

    while (1) {
        printf("Thread: %s\n", s);
        usleep(100000);
    }
    
    pthread_cleanup_pop(0);
    return NULL;
}

int main() {
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

    void* res;

    if (pthread_join(t, &res) != 0) {
        perror("pthread_join");
    }

    if (res == PTHREAD_CANCELED) {
        printf("Main: thread was canceled (PTHREAD_CANCELED)\n");
    }

    printf("Main done\n");
    return 0;
}