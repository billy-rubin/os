#include "my_thread.h"
#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>

static _Atomic int g_counter = 0;

void* test1(void* arg) {
    (void)arg;
    printf("hello from mythread\n");
    return NULL;
}

void* test2(void* arg) {
    (void)arg;
    sleep(3);
    return NULL;
}

void* test3(void* arg) {
    int n = *(int*)arg;
    int s = 0;
    for (int i = 0; i < n; i++) {
        s += (i % 17);
    }
    int *res = malloc(sizeof(int));
    if (res) 
        *res = s;
    return res;
}

void* worker_incr(void* arg) {
    (void)arg;
    for (int i = 0; i < 100000; i++) {
        atomic_fetch_add(&g_counter, 1);
    }
    return NULL;
}


int main(void) {
    //hello world test
    {
        mythread t = {0};
        mythread_t thread = &t;
        void *retval = (void*)0xDEADBEEF;

        int res = mythread_create(thread, test1, NULL);
        if (res != 0) {
            printf("[1] error creating mythread: %d\n", res);
            return 1;
        }

        res = mythread_join(thread, &retval);
        if (res != 0) {
            printf("[1] error joining mythread: %d\n", res);
            return 1;
        }

        printf("[1] join OK, retval=%p\n", retval);
    }

    //sleeping thread + join
    {
        mythread t = {0};
        mythread_t thread = &t;
        void *retval = NULL;

        int res = mythread_create(thread, test2, NULL);
        if (res != 0) {
            printf("[2] error creating mythread: %d\n", res);
            return 1;
        }

        res = mythread_join(thread, &retval);
        if (res != 0) {
            printf("[2] error joining mythread: %d\n", res);
            return 1;
        }
        printf("[2] join OK, retval=%p\n", retval);
    }

    //retval from thread
    {
        mythread t = {0};
        mythread_t thread = &t;
        void *retval = NULL;

        int n = 500000;
        int expect = 0;

        for (int i = 0; i < n; i++) 
            expect += (i % 17);

        int res = mythread_create(thread, test3, &n);
        if (res != 0) {
            printf("[3] error creating mythread: %d\n", res);
            return 1;
        }

        res = mythread_join(thread, &retval);
        if (res != 0) {
            printf("[3] error joining mythread: %d\n", res);
            return 1;
        }

        int got = *(int*)retval;
        printf("[3] sum got=%d expect=%d\n", got, expect);
        if (got != expect) {
            printf("[3] wrong result\n");
            return 1;
        }
    }

    //several threads incrementing counter
    {
        const int N = 4;
        mythread tt[N];
        mythread_t threads[N];
        for (int i = 0; i < N; i++) {
            memset(&tt[i], 0, sizeof(tt[i]));
            threads[i] = &tt[i];
        }
        g_counter = 0;

        for (int i = 0; i < N; i++) {
            int res = mythread_create(threads[i], worker_incr, NULL);
            if (res != 0) {
                printf("[4] error creating thread %d: %d\n", i, res);
                return 1;
            }
        }

        for (int i = 0; i < N; i++) {
            int res = mythread_join(threads[i], NULL);
            if (res != 0) {
                printf("[4] error joining thread %d: %d\n", i, res);
                return 1;
            }
        }

        int expect = N * 100000;
        int got = atomic_load(&g_counter);
        printf("[4] counter got=%d expect=%d\n", got, expect);
        if (got != expect) {
            printf("[4] counter mismatch\n");
            return 1;
        }
    }

    //Double join
    {
        mythread t = {0};
        mythread_t thread = &t;

        int res = mythread_create(thread, test2, NULL);
        if (res != 0) {
            printf("[5] error creating mythread: %d\n", res);
            return 1;
        }


        res = mythread_join(thread, NULL);
        printf("[5] first join rc=%d (not expected EINVAL=%d)\n", res, EINVAL);

        if (res != 0) {
            printf("[5] first join failed: %d\n", res);
            return 1;
        }

        res = mythread_join(thread, NULL);
        printf("[5] second join rc=%d (expect EINVAL=%d)\n", res, EINVAL);
    }

    //Invalid args
    {
        int rc;

        rc = mythread_create(NULL, test1, NULL);
        printf("[6] create(NULL,...) rc=%d (expect EINVAL=%d)\n", rc, EINVAL);

        mythread t = {0};
        mythread_t thread = &t;
        rc = mythread_create(thread, NULL, NULL);
        printf("[6] create(..., NULL, ...) rc=%d (expect EINVAL=%d)\n", rc, EINVAL);

        rc = mythread_join(NULL, NULL);
        printf("[6] join(NULL,...) rc=%d (expect EINVAL=%d)\n", rc, EINVAL);
    }

    printf("\n Tests have been executed \n");
    return 0;
}