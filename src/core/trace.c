#include "robu/trace.h"
#if ROBU_TRACE
#include "percpu.h"
#define TRACE_RING_CAPACITY 1024
typedef struct {
    uint64_t timestamp;
    uint64_t event;
    uint64_t arg[6];
} trace_record_t;
typedef struct {
    trace_record_t records[TRACE_RING_CAPACITY];
    uint64_t seq;
} trace_ring_t;
static trace_ring_t trace_rings[MAX_CPUS];
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
void trace_event(trace_event_t event, uint64_t a0, uint64_t a1, uint64_t a2,
                 uint64_t a3, uint64_t a4, uint64_t a5) {
    trace_ring_t *ring = &trace_rings[this_cpu()->cpu_id];
    uint64_t seq = ring->seq;
    trace_record_t *rec = &ring->records[seq & (TRACE_RING_CAPACITY - 1)];
    rec->timestamp = rdtsc();
    rec->event = (uint64_t)event;
    rec->arg[0] = a0;
    rec->arg[1] = a1;
    rec->arg[2] = a2;
    rec->arg[3] = a3;
    rec->arg[4] = a4;
    rec->arg[5] = a5;
    __atomic_store_n(&ring->seq, seq + 1, __ATOMIC_RELEASE);
}
#endif
