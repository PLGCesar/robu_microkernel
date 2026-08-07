#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/kinfo.h"
#include "robu/testreport.h"
#define BENCH_ITERATIONS 100000
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
void _start(void) {
    const volatile kinfo_page_t *kinfo = (const volatile kinfo_page_t *)KINFO_VA;
    tid_t server = (tid_t)kinfo->benchserver_tid;
    tid_t report = (tid_t)kinfo->test_report_tid;
    msg_regs_t m = (msg_regs_t){0};
    ipc_call(server, &m, NULL);
    uint64_t t0 = rdtsc();
    for (uint64_t i = 0; i < BENCH_ITERATIONS; i++) {
        ipc_call(server, &m, NULL);
    }
    uint64_t t1 = rdtsc();
    uint64_t avg_cycles = (t1 - t0) / BENCH_ITERATIONS;
    m = (msg_regs_t){0};
    m.word[0] = TEST_REPORT_KIND_BENCH;
    m.word[1] = avg_cycles;
    m.word[2] = BENCH_ITERATIONS;
    ipc_send(report, &m);
    for (;;) {
        ipc_sleep(1000000);
    }
}
