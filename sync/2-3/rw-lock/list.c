#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


Storage g_storage;

_Atomic long asc_iterations = 0;
_Atomic long desc_iterations = 0;
_Atomic long eq_iterations = 0;

_Atomic long asc_last_pairs = 0;
_Atomic long desc_last_pairs = 0;
_Atomic long eq_last_pairs = 0;

_Atomic long asc_swaps = 0;
_Atomic long desc_swaps = 0;
_Atomic long eq_swaps = 0;


static Node *node_create(const char *str) {
    Node *n = (Node *)malloc(sizeof(Node));
    if (!n) {
        perror("malloc");
        exit(1);
    }

    strncpy(n->value, str, sizeof(n->value) - 1);
    n->value[sizeof(n->value) - 1] = '\0';
    n->next = NULL;

    if (pthread_rwlock_init(&n->lock, NULL) != 0) {
        perror("pthread_rwlock_init");
        exit(1);
    }

    return n;
}


void storage_init(Storage *st, int size) {
    st->head = node_create("");  

    Node *tail = st->head;

    for (int i = 0; i < size; ++i) {
        int len = rand() % 50 + 1;
        char buf[100];
        for (int j = 0; j < len; ++j) {
            buf[j] = 'a' + (rand() % 26);
        }
        buf[len] = '\0';

        Node *n = node_create(buf);
        tail->next = n;
        tail = n;
    }
}

void storage_destroy(Storage *st) {
    Node *cur = st->head;
    while (cur) {
        Node *next = cur->next;
        pthread_rwlock_destroy(&cur->lock);
        free(cur);
        cur = next;
    }
    st->head = NULL;
}

int storage_length(Storage *st) {
    int len = 0;
    Node *cur = st->head->next;
    while (cur) {
        ++len;
        cur = cur->next;
    }
    return len;
}