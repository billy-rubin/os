#ifndef MUTEX_H
#define MUTEX_H

#define MUTEX_UNLOCKED  0
#define MUTEX_LOCKED    1
#define MUTEX_CONTENDED 2

typedef struct {
    volatile int state;
} futex_mutex_t;

void fmutex_init   (futex_mutex_t *m);
void fmutex_lock   (futex_mutex_t *m);
void fmutex_unlock (futex_mutex_t *m);
int  fmutex_trylock(futex_mutex_t *m);

#endif 