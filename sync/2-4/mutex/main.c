#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include "mutex.h"   

#define N_THREADS 4
#define N_ITER    10000

static long long counter_futex = 0;

static futex_mutex_t futex_mtx;

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}


void* worker_futex(void *arg) {
    (void)arg;

    for (int i = 0; i < N_ITER; ++i) {
        fmutex_lock(&futex_mtx);
        counter_futex++;
        fmutex_unlock(&futex_mtx);
    }

    return NULL;
}

int main(void) {
    pthread_t threads[N_THREADS];

    fmutex_init(&futex_mtx);
    printf("=== fmutex_t (my) ===\n");

    double t0 = now_sec();

    for (int i = 0; i < N_THREADS; ++i)
        pthread_create(&threads[i], NULL, worker_futex, NULL);

    for (int i = 0; i < N_THREADS; ++i)
        pthread_join(threads[i], NULL);

    double t1 = now_sec();

    printf("Expected: %lld\n", (long long)N_THREADS * N_ITER);
    printf("Actual:   %lld\n", counter_futex);
    printf("Time:     %.6f s\n\n", t1 - t0);

    return 0;
}