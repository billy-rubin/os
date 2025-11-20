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

void* worker_disable_cancel(void *arg) {
    (void)arg;
    printf("[disable] thread started\n");

    int old = 0;
    mythread_setcancelstate(MYTHREAD_CANCEL_DISABLE, &old);
    printf("[disable] cancel disabled (old=%d)\n", old);

    for (int i = 0; i < 1000000; ++i) {
        if (i % 1000 == 0) {
            mythread_testcancel();
        }
    }
    mythread_setcancelstate(MYTHREAD_CANCEL_ENABLE, &old);
    printf("[disable] cancel re-enabled (old=%d)\n", old);

    for (;;) {
        for (int i = 0; i < 100000; ++i) {
        }
        mythread_testcancel();
    }

    printf("[disable] thread finished normally (unexpected)\n");
    return NULL;
}


void* worker_deferred(void *arg) {
    (void)arg;
    printf("[deferred] thread started\n");

    for (;;) {
        sleep(5);
        mythread_testcancel();
    }

    printf("[deferred] thread finished normally (unexpected)\n");
    return NULL;
}

void* worker_async(void *arg) {
    (void)arg;
    printf("[async] thread started\n");

    int oldtype = 0;
    mythread_setcanceltype(MYTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);
    printf("[async] cancel type set to ASYNCHRONOUS (old=%d)\n", oldtype);

    for (;;) {
        sleep(5);
    }
    return NULL;
}


int main(void) {
    int err;
    void *retval;
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
            free(retval);
            return 1;
        }
        free(retval);
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
        printf("[5] first join err=%d (not expected EINVAL=%d)\n", res, EINVAL);

        if (res != 0) {
            printf("[5] first join failed: %d\n", res);
            return 1;
        }

        res = mythread_join(thread, NULL);
        printf("[5] second join err=%d (expect EINVAL=%d)\n", res, EINVAL);
    }

    //Invalid args
    {
        err = mythread_create(NULL, test1, NULL);
        printf("[6] create(NULL,...) err=%d (expect EINVAL=%d)\n", err, EINVAL);

        mythread t = {0};
        mythread_t thread = &t;
        err = mythread_create(thread, NULL, NULL);
        printf("[6] create(..., NULL, ...) err=%d (expect EINVAL=%d)\n", err, EINVAL);

        err = mythread_join(NULL, NULL);
        printf("[6] join(NULL,...) err=%d (expect EINVAL=%d)\n", err, EINVAL);
    }

    //test deffered cancel
    {
        mythread t = {0};
        mythread_t th = &t;

        err = mythread_create(th, worker_deferred, NULL);
        if (err != 0) {
            printf("[7] mythread_create failed: %d\n", err);
            return 1;
        }

        sleep(1);
        err = mythread_cancel(th);
        if (err != 0 && err != ESRCH) {
            printf("[7] mythread_cancel unexpected err=%d\n", err);
            return 1;
        }

        retval = NULL;
        err = mythread_join(th, &retval);
        if (err != 0) {
            printf("[7] mythread_join failed: %d\n", err);
            return 1;
        }

        printf("[7] join retval=%p (expect %p)\n", retval, MYTHREAD_CANCELED);
    }

    //test set disabled
    {
        mythread t = {0};
        mythread_t th = &t;

        err = mythread_create(th, worker_disable_cancel, NULL);
        if (err != 0) {
            printf("[8] mythread_create failed: %d\n", err);
            return 1;
        }

        usleep(100 * 1000); // 100 ms

        err = mythread_cancel(th);
        if (err != 0 && err != ESRCH) {
            printf("[8] mythread_cancel unexpected err=%d\n", err);
            return 1;
        }

        retval = NULL;
        err = mythread_join(th, &retval);
        if (err != 0) {
            printf("[8] mythread_join failed: %d\n", err);
            return 1;
        }

        printf("[8] join retval=%p (expect %p)\n", retval, MYTHREAD_CANCELED);
    }

    //test assync cancel
    {
        mythread t = {0};
        mythread_t th = &t;

        err = mythread_create(th, worker_async, NULL);
        if (err != 0) {
            printf("[9] mythread_create failed: %d\n", err);
            return 1;
        }

        sleep(5);
        err = mythread_cancel(th);
        if (err != 0 && err != ESRCH) {
            printf("[9] mythread_cancel unexpected err=%d\n", err);
            return 1;
        }

        retval = NULL;
        err = mythread_join(th, &retval);
        if (err != 0) {
            printf("[9] mythread_join failed: %d\n", err);
            return 1;
        }

        printf("[9]] join retval=%p (expect %p)\n", retval, MYTHREAD_CANCELED);
    }

    printf("\n Tests have been executed \n");
    return 0;
}