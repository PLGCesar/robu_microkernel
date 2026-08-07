#ifndef ROBU_TRACE_H
#define ROBU_TRACE_H
#include "robu/types.h"
typedef enum {
    TRACE_EVT_IPC_SEND = 1,
    TRACE_EVT_IPC_RECV,
    TRACE_EVT_CTX_SWITCH,
    TRACE_EVT_PAGE_FAULT,
} trace_event_t;
#if ROBU_TRACE
void trace_event(trace_event_t event, uint64_t a0, uint64_t a1, uint64_t a2,
                 uint64_t a3, uint64_t a4, uint64_t a5);
#define TRACE(ev, a0, a1, a2, a3, a4, a5) trace_event(ev, a0, a1, a2, a3, a4, a5)
#else
#define TRACE(ev, a0, a1, a2, a3, a4, a5) ((void)0)
#endif
#endif
