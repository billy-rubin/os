#ifndef CACHE_PROXY_CACHE_H
#define CACHE_PROXY_CACHE_H

#include <pthread.h>
#include <stddef.h>

#define CACHE_BUCKETS 1024

typedef struct cache_entry {
    char *key;
    char *data;
    size_t size;
    size_t capacity;
    int complete;
    int failed;
    int refcnt;

    pthread_mutex_t lock;
    pthread_cond_t  cond;

    struct cache_entry *next;
} cache_entry_t;

typedef struct cache_table {
    cache_entry_t *buckets[CACHE_BUCKETS];
    pthread_mutex_t lock;
} cache_table_t;


void cache_table_init(cache_table_t *table);
cache_entry_t *cache_start_or_join(cache_table_t *table, const char *key, int *am_writer);
void cache_release(cache_table_t *table, cache_entry_t *entry);
int cache_add(cache_entry_t *entry, const void *buf, size_t len);
void cache_complete(cache_entry_t *entry);
void cache_failed(cache_entry_t *entry);

#endif