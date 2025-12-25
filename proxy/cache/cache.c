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

static void lru_remove(cache_table_t *table, cache_entry_t *entry) {
    if (entry->lru_prev) entry->lru_prev->lru_next = entry->lru_next;
    else table->lru_head = entry->lru_next;

    if (entry->lru_next) entry->lru_next->lru_prev = entry->lru_prev;
    else table->lru_tail = entry->lru_prev;

    entry->lru_prev = NULL;
    entry->lru_next = NULL;
}

static void lru_push_front(cache_table_t *table, cache_entry_t *entry) {
    entry->lru_next = table->lru_head;
    entry->lru_prev = NULL;

    if (table->lru_head) table->lru_head->lru_prev = entry;
    else table->lru_tail = entry;

    table->lru_head = entry;
}

static void free_entry_completely(cache_table_t *table, cache_entry_t *entry) {
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

    lru_remove(table, entry);

    if (table->current_size >= entry->size)
        table->current_size -= entry->size;

    log_debug("GC: Evicting key='%s', size=%zu", entry->key, entry->size);
    pthread_mutex_destroy(&entry->lock);
    pthread_cond_destroy(&entry->cond);
    free(entry->data);
    free(entry->key);
    free(entry);
}


void cache_table_init(cache_table_t *table) {
    memset(table->buckets, 0, sizeof(table->buckets));
    table->lru_head = NULL;
    table->lru_tail = NULL;
    table->current_size = 0;
    table->max_size = CACHE_MAX_SIZE;
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
            lru_remove(table, e);
            lru_push_front(table, e);
            log_debug("cache HIT and LRU update key='%s', refcnt=%d", key, e->refcnt);
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
    lru_push_front(table, e);

    *am_writer = 1;
    log_debug("cache MISS, new entry key='%s'", key);

    pthread_mutex_unlock(&table->lock);
    return e;
}

void cache_release(cache_table_t *table, cache_entry_t *entry) {
    pthread_mutex_lock(&table->lock);

    entry->refcnt--;
    log_debug("cache_release key='%s', refcnt=%d", entry->key, entry->refcnt);
    
    if (entry->refcnt == 0 && entry->failed) {
        free_entry_completely(table, entry);
    }

    pthread_mutex_unlock(&table->lock);
}

int cache_add(cache_entry_t *entry, const void *buf, size_t len, cache_table_t *table) {
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

    __sync_fetch_and_add(&table->current_size, len);

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

void cache_evict_if_needed(cache_table_t *table) {
    pthread_mutex_lock(&table->lock);

    while (table->current_size > table->max_size) {
        cache_entry_t *candidate = table->lru_tail;
        
        int freed_something = 0;
        
        while (candidate) {
            cache_entry_t *prev = candidate->lru_prev;
            
            if (candidate->refcnt == 0) {
                log_debug("GC: Removing old entry '%s' to free space", candidate->key);
                free_entry_completely(table, candidate);
                freed_something = 1;
                break;
            }
            
            candidate = prev;
        }

        if (!freed_something) {
            log_debug("GC: Cache full, but all entries are active. Can't evict.");
            break;
        }
    }

    pthread_mutex_unlock(&table->lock);
}