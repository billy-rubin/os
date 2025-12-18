#include <stdio.h>
#include <stdlib.h>
#include "uthread.h"

typedef struct {
    int id;
    int steps;
} task_t;

void *worker(void *arg) {
    task_t *t = (task_t *)arg;
    for (int i = 0; i < t->steps; i++) {
        printf("Thread %d: step %d\n", t->id, i);
        uthread_yield();
    }
    return NULL;
}

int main(void) {
    uthread_t *t1, *t2;
    task_t task1 = {1, 3};
    task_t task2 = {2, 3};

    if (uthread_create(&t1, worker, &task1) != 0) {
        return 1;
    }
    if (uthread_create(&t2, worker, &task2) != 0) {
        return 1;
    }

    if (uthread_join(t1, NULL) != 0) {
        return 1;
    }
    if (uthread_join(t2, NULL) != 0) {
        return 1;
    }

    return 0;
}