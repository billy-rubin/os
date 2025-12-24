#define _GNU_SOURCE
#include "mutex.h"
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

static int futex_wait(volatile int *addr, int expected) {
  return syscall(SYS_futex, (int *)addr, FUTEX_WAIT, expected, NULL, NULL, 0);
}

static int futex_wake(volatile int *addr, int n) {
  return syscall(SYS_futex, (int *)addr, FUTEX_WAKE, n, NULL, NULL, 0);
}

void fmutex_init(futex_mutex_t *m) {
  int value = MUTEX_UNLOCKED;
  __atomic_store(&m->state, &value, __ATOMIC_RELAXED);
}

void fmutex_lock(futex_mutex_t *m) {
  int expected = MUTEX_UNLOCKED;
  int desired = MUTEX_LOCKED;

  if (__atomic_compare_exchange(&m->state, &expected, &desired, 0,
                                __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
    return;
  }

  for (;;) {
    int state;
    __atomic_load(&m->state, &state, __ATOMIC_RELAXED);

    if (state == MUTEX_UNLOCKED) {
      expected = MUTEX_UNLOCKED;
      desired = MUTEX_LOCKED;
      if (__atomic_compare_exchange(&m->state, &expected, &desired, 0,
                                    __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        return;
      }
      continue;
    }

    if (state == MUTEX_LOCKED) {
      expected = MUTEX_LOCKED;
      desired = MUTEX_CONTENDED;
      if (!__atomic_compare_exchange(&m->state, &expected, &desired, 0,
                                     __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        continue;
      }
      state = MUTEX_CONTENDED;
    }

    futex_wait(&m->state, MUTEX_CONTENDED);
  }
}

void fmutex_unlock(futex_mutex_t *m) {
  int unlocked = MUTEX_UNLOCKED;
  int prev;

  __atomic_exchange(&m->state, &unlocked, &prev, __ATOMIC_RELEASE);

  if (prev == MUTEX_CONTENDED) {
    futex_wake(&m->state, 1);
  }
}

int fmutex_trylock(futex_mutex_t *m) {
  int expected = MUTEX_UNLOCKED;
  int desired = MUTEX_LOCKED;

  return __atomic_compare_exchange(&m->state, &expected, &desired, 0,
                                   __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}