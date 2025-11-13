#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

int isSigintReceived = 0;

void sigint_handler(int sig) {
    printf("Received SIGINT signal\n");
    isSigintReceived = 1;
}

void *BlockingThread(void *arg) {
    sigset_t set;
    sigfillset(&set);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    signal(SIGINT, sigint_handler);

    while (1) {
        sleep(1);
        if (isSigintReceived) {
            isSigintReceived = 0;
            printf("Received SIGINT in BLOCKING THREAD signal\n");
        }
    }
    return NULL;
}

void *SigIntWaiter(void *arg) {
    signal(SIGINT, sigint_handler);
    while (isSigintReceived) {
        if (isSigintReceived) {
            isSigintReceived = 0;
            printf("Received SIGINT signal\n");
        }
    }
    return NULL;
}

void *SigQuitWaiter(void *arg) {
    sigset_t wait_set;
    sigemptyset(&wait_set);
    sigaddset(&wait_set, SIGQUIT);
    pthread_sigmask(SIG_BLOCK, &wait_set, NULL);
    int sig;
    while (1) {
        int result = sigwait(&wait_set, &sig);
        if (result == 0 && sig == SIGQUIT) {
            printf("Received SIGQUIT signal\n");
        }
    }
    return NULL;
}

int main() {
    pthread_t tid1, tid2, tid3;
    int err;

    printf("main [%d %d %d]: Hello from main!\n", getpid(), getppid(), gettid());

    err = pthread_create(&tid1, NULL, BlockingThread, NULL);
    if (err != 0) {
        printf("main: pthread_create(BlockingThread) failed: %s\n", strerror(err));
        return -1;
    }

    err = pthread_create(&tid2, NULL, SigIntWaiter, NULL);
    if (err != 0) {
        printf("main: pthread_create(SigIntWaiter) failed: %s\n", strerror(err));
        return -1;
    }

    err = pthread_create(&tid3, NULL, SigQuitWaiter, NULL);
    if (err != 0) {
        printf("main: pthread_create(SigQuitWaiter) failed: %s\n", strerror(err));
        return -1;
    }

    while (1) {
        printf("Sending SIGINT to SigIntWaiter...\n");
        pthread_kill(tid2, SIGINT);
        
        sleep(2);

        printf("Sending SIGQUIT to SigQuitWaiter...\n");
        pthread_kill(tid3, SIGQUIT);

        printf("Sending SIGINT to Blockingthread...\n");
        pthread_kill(tid1, SIGINT);
        
        sleep(2);
    }

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);

    return 0;
}