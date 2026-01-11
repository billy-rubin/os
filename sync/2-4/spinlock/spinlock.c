#include "spinlock.h"
#include <errno.h>
void spin_init(spinlock_t *lock) {
  int value = 0;
  __atomic_store(&lock->flag, &value, __ATOMIC_RELAXED);
}

void spin_lock(spinlock_t *lock) {
  for (;;) {
    int expected = 0;
    int desired = 1;

    if (__atomic_compare_exchange(&lock->flag, &expected, &desired, 0,
                                  __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
      return;
    }

    for (;;) {
      int value;
      __atomic_load(&lock->flag, &value, __ATOMIC_RELAXED);
      if (value == 0) {
        break;
      }
    }
  }
}

void spin_unlock(spinlock_t *lock) {
  int value = 0;
  __atomic_store(&lock->flag, &value, __ATOMIC_RELEASE);
}

int spin_trylock(spinlock_t *lock) {
  int expected = 0;
  int desired = 1;
  if (__atomic_compare_exchange(&lock->flag, &expected, &desired, 0,
    __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
      return 0;
    }
  return EBUSY;
}