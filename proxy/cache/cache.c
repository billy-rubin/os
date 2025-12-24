#include "cache.h"
#include "logger/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long hash_key(const char *s) {
    unsigned long h = 1469598103934665603ull;
    while (*s) {
        h ^= (unsigned char)(*s++);
        h *= 1099511628211ull;
    }
    return h;
}

void cache_table_init(cache_table_t *table) {
    memset(table->buckets, 0, sizeof(table->buckets));
    pthread_mutex_init(&table->lock, NULL);
    log_debug("cache table initialized");
}

static int ensure_capacity(cache_entry_t *entry, size_t need) {
    if (need <= entry->capacity) 
        return 0;

    size_t newcap = entry->capacity ? entry->capacity * 2 : 8192;
    while (newcap < need) 
        newcap *= 2;
    
    char *p = realloc(entry->data, newcap);
    if (!p) 
        return -1;

    entry->data = p;
    entry->capacity = newcap;
    return 0;
}

cache_entry_t *cache_start_or_join(cache_table_t *table, const char *key, int *am_writer) {
    *am_writer = 0;
    pthread_mutex_lock(&table->lock);

    unsigned long h = hash_key(key);
    size_t idx = h % CACHE_BUCKETS;

    cache_entry_t *e = table->buckets[idx];
    while (e) {
        if (strcmp(e->key, key) == 0) {
            e->refcnt++;
            log_debug("cache HIT key='%s', refcnt=%d", key, e->refcnt);
            pthread_mutex_unlock(&table->lock);
            return e;
        }
        e = e->next;
    }

    e = calloc(1, sizeof(*e));
    if (!e) {
        log_error("cache_start_or_join: malloc failed for key '%s'", key);
        pthread_mutex_unlock(&table->lock);
        return NULL;
    }

    e->key = strdup(key);
    e->data = NULL;
    e->size = 0;
    e->capacity = 0;
    e->complete = 0;
    e->failed = 0;
    e->refcnt = 1;

    pthread_mutex_init(&e->lock, NULL);
    pthread_cond_init(&e->cond, NULL);

    e->next = table->buckets[idx];
    table->buckets[idx] = e;

    *am_writer = 1;
    log_debug("cache MISS, new entry key='%s'", key);

    pthread_mutex_unlock(&table->lock);
    return e;
}

void cache_release(cache_table_t *table, cache_entry_t *entry) {
    pthread_mutex_lock(&table->lock);

    entry->refcnt--;
    log_debug("cache_release key='%s', new refcnt=%d", entry->key, entry->refcnt);

    if (entry->refcnt > 0) {
        pthread_mutex_unlock(&table->lock);
        return;
    }

    unsigned long h = hash_key(entry->key);
    size_t idx = h % CACHE_BUCKETS;

    cache_entry_t **pp = &table->buckets[idx];
    while (*pp) {
        if (*pp == entry) {
            *pp = entry->next;
            break;
        }
        pp = &(*pp)->next;
    }

    pthread_mutex_unlock(&table->lock);
    pthread_mutex_destroy(&entry->lock);
    pthread_cond_destroy(&entry->cond);
    free(entry->data);
    free(entry->key);
    free(entry);

    log_debug("cache entry fully freed");
}

int cache_add(cache_entry_t *entry, const void *buf, size_t len) {
    pthread_mutex_lock(&entry->lock);

    if (entry->failed) {
        pthread_mutex_unlock(&entry->lock);
        return -1;
    }

    size_t need = entry->size + len;
    if (ensure_capacity(entry, need) != 0) {
        entry->failed = 1;
        pthread_cond_broadcast(&entry->cond);
        pthread_mutex_unlock(&entry->lock);
        log_error("cache_add: realloc failed");
        return -1;
    }

    memcpy(entry->data + entry->size, buf, len);
    entry->size += len;

    pthread_cond_broadcast(&entry->cond);
    pthread_mutex_unlock(&entry->lock);
    return 0;
}

void cache_complete(cache_entry_t *entry) {
    pthread_mutex_lock(&entry->lock);
    entry->complete = 1;
    pthread_cond_broadcast(&entry->cond);
    pthread_mutex_unlock(&entry->lock);
    log_debug("cache_complete for key='%s'", entry->key);
}

void cache_failed(cache_entry_t *entry) {
    pthread_mutex_lock(&entry->lock);
    entry->failed = 1;
    pthread_cond_broadcast(&entry->cond);
    pthread_mutex_unlock(&entry->lock);
    log_error("cache_failed for key='%s'", entry->key);
}