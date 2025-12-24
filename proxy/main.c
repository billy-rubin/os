#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "threadpool/threadpool.h"

#define N_THREADS 4

static void my_function(void *arg) {
    long id = *(long *)arg;
    free(arg);

    printf("[task %ld] started\n", id);
    usleep(100 * 1000);
    printf("[task %ld] finished\n", id);
}

int main(void) {
    threadPool_t *pool = Create(3);
    if (!pool) {
        fprintf(stderr, "failed to create thread pool\n");
        return 1;
    }

    for (long i = 0; i < N_THREADS; ++i) {
        long *id = malloc(sizeof(long));
        if (!id) {
            fprintf(stderr, "malloc failed\n");
            break;
        }
        *id = i;
        if (Submit(pool, my_function, id) != 0) {
            fprintf(stderr, "submit failed for task %ld\n", i);
            free(id);
        }
    }

    sleep(1);

    Stop(pool);
    return 0;
}