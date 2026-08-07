#ifndef ROBU_SPINLOCK_H
#define ROBU_SPINLOCK_H
#include "robu/types.h"
typedef struct {
    int locked;
} spinlock_t;
#define SPINLOCK_INIT { 0 }
static inline void spin_lock(spinlock_t *l) {
    while (__atomic_exchange_n(&l->locked, 1, __ATOMIC_ACQUIRE)) {
        while (__atomic_load_n(&l->locked, __ATOMIC_RELAXED)) {
            asm volatile("pause");
        }
    }
}
static inline void spin_unlock(spinlock_t *l) {
    __atomic_store_n(&l->locked, 0, __ATOMIC_RELEASE);
}
static inline int spin_trylock(spinlock_t *l) {
    return __atomic_exchange_n(&l->locked, 1, __ATOMIC_ACQUIRE) == 0;
}
#endif
