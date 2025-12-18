#ifndef LIST_H
#define LIST_H

#define _GNU_SOURCE 
#include <pthread.h>
#include <stdatomic.h>


typedef struct _Node {
    char value[100];
    struct _Node *next;
    pthread_rwlock_t lock;
} Node;

typedef struct _Storage {
    Node *head;
} Storage;

extern Storage g_storage;


extern _Atomic long asc_iterations;
extern _Atomic long desc_iterations;
extern _Atomic long eq_iterations;

extern _Atomic long asc_last_pairs;
extern _Atomic long desc_last_pairs;
extern _Atomic long eq_last_pairs;

extern _Atomic long asc_swaps;
extern _Atomic long desc_swaps;
extern _Atomic long eq_swaps;


void storage_init(Storage *st, int size);
void storage_destroy(Storage *st);
int  storage_length(Storage *st);


void *pairs_counter_thread(void *arg);
void *swapper_thread(void *arg);
void *monitor_thread(void *arg);

#endif 