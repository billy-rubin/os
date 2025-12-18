#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

int main(int argc, char **argv) {
    int list_size = 100;

    if (argc > 1) list_size = atoi(argv[1]);
    if (list_size <= 0) list_size = 100;

    srand((unsigned)time(NULL));

    printf("[RWLOCK] init list with %d nodes\n", list_size);
    storage_init(&g_storage, list_size);
    printf("Actual list length: %d\n", storage_length(&g_storage));

    pthread_t th_counter_asc;
    pthread_t th_counter_desc;
    pthread_t th_counter_eq;

    pthread_t th_swapper_asc;
    pthread_t th_swapper_desc;
    pthread_t th_swapper_eq;

    pthread_t th_monitor;

    pthread_create(&th_counter_asc,  NULL, pairs_counter_thread, (void*)(long)0);
    pthread_create(&th_counter_desc, NULL, pairs_counter_thread, (void*)(long)1);
    pthread_create(&th_counter_eq,   NULL, pairs_counter_thread, (void*)(long)2);

    pthread_create(&th_swapper_asc,  NULL, swapper_thread, (void*)(long)0);
    pthread_create(&th_swapper_desc, NULL, swapper_thread, (void*)(long)1);
    pthread_create(&th_swapper_eq,   NULL, swapper_thread, (void*)(long)2);

    pthread_create(&th_monitor, NULL, monitor_thread, (void*)"[RWLOCK]");

    pthread_join(th_monitor, NULL);

    pthread_join(th_counter_asc,  NULL);
    pthread_join(th_counter_desc, NULL);
    pthread_join(th_counter_eq,   NULL);
    pthread_join(th_swapper_asc,  NULL);
    pthread_join(th_swapper_desc, NULL);
    pthread_join(th_swapper_eq,   NULL);

    storage_destroy(&g_storage);
    return 0;
}