#ifndef SPINLOCK_H
#define SPINLOCK_H

typedef struct {
    int flag; 
} spinlock_t;

void spin_init(spinlock_t *lock);
void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);
int  spin_trylock(spinlock_t *lock);

#endif 