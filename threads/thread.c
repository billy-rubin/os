#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

int global = 1;

void *mythread(void *arg) {
	int local = 5;
    static int localStatic = 10;
    const int localConst = 7;

	printf("mythread [%d %d %d %ld]: Hello from mythread!\n", getpid(), getppid(), gettid(), pthread_self());
	printf("addresses - local: %p static: %p const: %p global: %p\n", &local, &localStatic, &localConst, &global);
	printf("values - local: %d static: %d const: %d global: %d\n", local, localStatic, localConst, global);

	local += 1;
    global += 1;
	printf("local value - %d, global value - %d", local, global);
	sleep(5);
	return NULL;
}

int main() {
    pthread_t threads[5];	
	int err;

	printf("main [%d %d %d]: Hello from main!\n", getpid(), getppid(), gettid());

	for (int i = 0; i < 5; i++) {

		err = pthread_create(&threads[i], NULL, mythread, NULL);
		printf("main [%ld]\n", threads[i]);
		if (err) {
		    printf("main: pthread_create() failed: %s\n", strerror(err));
			for (int j = 0; j < i; j++) {
				pthread_join(threads[j], NULL);
			}
			return -1;
		}
		sleep(1000);
    }

	for (int i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }

	return 0;
}

