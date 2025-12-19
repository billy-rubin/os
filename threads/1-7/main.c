#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "uthread.h"

typedef struct {
    int id;
    int steps;
} test1_arg_t;

void *test1_worker(void *arg) {
    test1_arg_t *data = (test1_arg_t *)arg;
    for (int i = 0; i < data->steps; i++) {
        printf("  [Test 1] Thread %d: step %d\n", data->id, i + 1);
        uthread_yield();
    }
    return NULL;
}

void test_interleaving(void) {
    printf("=== TEST 1: Argument Passing & Context Switch (Interleaving) ===\n");
    uthread_t t1, t2;
    test1_arg_t arg1 = {1, 3};
    test1_arg_t arg2 = {2, 3};

    if (uthread_create(&t1, test1_worker, &arg1) != 0) {
        fprintf(stderr, "Failed to create thread 1\n");
        exit(1);
    }
    if (uthread_create(&t2, test1_worker, &arg2) != 0) {
        fprintf(stderr, "Failed to create thread 2\n");
        exit(1);
    }

    uthread_join(t1, NULL);
    uthread_join(t2, NULL);
    printf("=== TEST 1 PASSED ===\n\n");
}

void *test2_calculator(void *arg) {
    int input = *(int *)arg;
    printf("  [Test 2] Calculating square of %d\n", input);
    
    uthread_yield(); 
    
    int *result = malloc(sizeof(int));
    *result = input * input;
    
    return result;
}

void test_return_value(void) {
    printf("=== TEST 2: Return Values ===\n");
    uthread_t t;
    int input = 12;
    void *retval;

    if (uthread_create(&t, test2_calculator, &input) != 0) {
        fprintf(stderr, "Failed to create calculator thread\n");
        exit(1);
    }

    if (uthread_join(t, &retval) != 0) {
        fprintf(stderr, "Join failed\n");
        exit(1);
    }

    int result_int = *(int *)retval;
    printf("  [Test 2] Result received: %d\n", result_int);
    
    if (result_int == 144) {
        printf("=== TEST 2 PASSED ===\n\n");
    } else {
        fprintf(stderr, "=== TEST 2 FAILED (Expected 144, got %d) ===\n", result_int);
        exit(1);
    }
    
    free(retval);
}

#define NUM_THREADS 5

void *test3_worker(void *arg) {
    long id = (long)arg;
    printf("  [Test 3] Thread %ld started\n", id);
    uthread_yield();
    printf("  [Test 3] Thread %ld finished\n", id);
    return NULL;
}

void test_many_threads(void) {
    printf("=== TEST 3: Multiple Threads Execution ===\n");
    uthread_t threads[2*NUM_THREADS];

    for (long i = 0; i < 2*NUM_THREADS; i++) {
        if (uthread_create(&threads[i], test3_worker, (void *)(i + 1)) != 0) {
            fprintf(stderr, "Failed to create thread %ld\n", i);
            exit(1);
        }
    }

    for (int i = 0; i <2* NUM_THREADS; i++) {
        uthread_join(threads[i], NULL);
    }
    printf("=== TEST 3 PASSED ===\n\n");
}

int main(void) {
    printf("Starting UThread Library Tests...\n\n");

    test_interleaving();
    test_return_value();
    test_many_threads();

    printf("ALL TESTS PASSED SUCCESSFULLY.\n");
    return 0;
}