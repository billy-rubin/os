#include "list.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <stdio.h>


typedef enum {
    MODE_ASC  = 0,
    MODE_DESC = 1,
    MODE_EQ   = 2  
} mode_t;


void *pairs_counter_thread(void *arg) {
    mode_t mode = (mode_t)(long)arg;

    for (;;) {

        long local_pairs = 0;   

        Node *prev_locked = NULL;
        Node *cur = g_storage.head->next; 

        if (!cur) {
            if (mode == MODE_ASC) {
                atomic_fetch_add(&asc_iterations, 1);
                atomic_store(&asc_last_pairs, 0);
            } else if (mode == MODE_DESC) {
                atomic_fetch_add(&desc_iterations, 1);
                atomic_store(&desc_last_pairs, 0);
            } else {
                atomic_fetch_add(&eq_iterations, 1);
                atomic_store(&eq_last_pairs, 0);
            }
            continue;
        }

        pthread_mutex_lock(&cur->lock);

        while (cur) {
            Node *next = cur->next;
            if (!next)
                break;

            pthread_mutex_lock(&next->lock);

            size_t len1 = strlen(cur->value);
            size_t len2 = strlen(next->value);

            if (mode == MODE_ASC  && len1 < len2)  local_pairs++;
            if (mode == MODE_DESC && len1 > len2)  local_pairs++;
            if (mode == MODE_EQ   && len1 == len2) local_pairs++;

            if (prev_locked)
                pthread_mutex_unlock(&prev_locked->lock);

            prev_locked = cur;
            cur = next;
        }

        if (prev_locked)
            pthread_mutex_unlock(&prev_locked->lock);
        if (cur)
            pthread_mutex_unlock(&cur->lock);

        if (mode == MODE_ASC) {
            atomic_fetch_add(&asc_iterations, 1);
            atomic_store(&asc_last_pairs, local_pairs);
        } else if (mode == MODE_DESC) {
            atomic_fetch_add(&desc_iterations, 1);
            atomic_store(&desc_last_pairs, local_pairs);
        } else {
            atomic_fetch_add(&eq_iterations, 1);
            atomic_store(&eq_last_pairs, local_pairs);
        }

        sched_yield();
    }

    return NULL;
}


static int should_swap(Node *cur, Node *next, mode_t mode) {
    int len1 = (int)strlen(cur->value);
    int len2 = (int)strlen(next->value);

    if (mode == MODE_ASC)  
        return len1 > len2;
    if (mode == MODE_DESC) 
        return len1 < len2;  
    if (mode == MODE_EQ)   
        return len1 != len2; 
    return 0;
}

void *swapper_thread(void *arg) {
    mode_t mode = (mode_t)(long)arg;

    for (;;) {

        Node *prev = g_storage.head;
        pthread_mutex_lock(&prev->lock);

        Node *cur = prev->next;
        if (!cur) {
            pthread_mutex_unlock(&prev->lock);
            sched_yield();
            continue;
        }

        pthread_mutex_lock(&cur->lock);

        int did_swap_in_pass = 0;

        while (cur && cur->next) {
            Node *next = cur->next;
            pthread_mutex_lock(&next->lock);

            int swapped = 0;

            if ((rand() & 0xF) == 0) {
                if (should_swap(cur, next, mode)) {
                    Node *tail = next->next;

                    prev->next = next;
                    next->next = cur;
                    cur->next  = tail;

                    if (mode == MODE_ASC)      
                        atomic_fetch_add(&asc_swaps, 1);
                    else if (mode == MODE_DESC) 
                        atomic_fetch_add(&desc_swaps, 1);
                    else if (mode == MODE_EQ)   
                        atomic_fetch_add(&eq_swaps, 1);

                    swapped = 1;
                    did_swap_in_pass = 1;
                }
            }

            pthread_mutex_unlock(&next->lock);

            if (swapped) {
                pthread_mutex_unlock(&cur->lock);
                pthread_mutex_unlock(&prev->lock);
                break; 
            } else {
                pthread_mutex_unlock(&prev->lock);
                prev = cur;
                cur = cur->next;
                if (cur)
                    pthread_mutex_lock(&cur->lock);
            }
        }

        if (!did_swap_in_pass) {
            if (cur)
                pthread_mutex_unlock(&cur->lock);
            pthread_mutex_unlock(&prev->lock);
        }

        sched_yield();
    }

    return NULL;
}


void *monitor_thread(void *arg) {
    const char *tag = (const char *)arg;
    if (!tag) tag = "[MONITOR]";

    for (;;) {
        printf(
            "%s stats: iters: asc=%ld desc=%ld eq=%ld  "
            "last_pairs: asc=%ld desc=%ld eq=%ld  "
            "swaps: asc=%ld desc=%ld eq=%ld\n",
            tag,
            (long)atomic_load(&asc_iterations),
            (long)atomic_load(&desc_iterations),
            (long)atomic_load(&eq_iterations),
            (long)atomic_load(&asc_last_pairs),
            (long)atomic_load(&desc_last_pairs),
            (long)atomic_load(&eq_last_pairs),
            (long)atomic_load(&asc_swaps),
            (long)atomic_load(&desc_swaps),
            (long)atomic_load(&eq_swaps)
        );
        sleep(1);
    }

    return NULL;
}