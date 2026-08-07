#include "robu/pipe.h"
#include "robu/ipc.h"
typedef struct {
    uint32_t generation;
    uint64_t readers_mask;
    uint64_t writers_mask;
    uint32_t head;
    uint32_t count;
    uint8_t ring[PIPE_RING_SIZE];
} pipe_obj_t;
static pipe_obj_t pipe_pool[PIPE_POOL_SIZE];
static int pipe_decode(uint64_t handle, int *out_idx, uint32_t *out_gen) {
    uint64_t idx = handle & 0xFFFFFFFFu;
    if (idx >= PIPE_POOL_SIZE) {
        return 0;
    }
    *out_idx = (int)idx;
    *out_gen = (uint32_t)(handle >> 32);
    return 1;
}
static int pipe_is_free(const pipe_obj_t *p) {
    return p->readers_mask == 0 && p->writers_mask == 0;
}
int64_t pipe_create(tid_t creator, uint64_t *out_handle) {
    for (int i = 0; i < PIPE_POOL_SIZE; i++) {
        pipe_obj_t *p = &pipe_pool[i];
        if (pipe_is_free(p)) {
            p->head = 0;
            p->count = 0;
            uint64_t bit = (1ULL << creator);
            p->readers_mask = bit;
            p->writers_mask = bit;
            *out_handle = ((uint64_t)p->generation << 32) | (uint64_t)i;
            return IPC_ERR_NONE;
        }
    }
    return IPC_ERR_NO_MEM;
}
int64_t pipe_read(uint64_t handle, uint8_t *out, uint64_t max, uint64_t *out_len) {
    int idx;
    uint32_t gen;
    if (!pipe_decode(handle, &idx, &gen)) {
        *out_len = 0;
        return IPC_ERR_NOT_FOUND;
    }
    pipe_obj_t *p = &pipe_pool[idx];
    if (p->generation != gen || pipe_is_free(p)) {
        *out_len = 0;
        return IPC_ERR_NOT_FOUND;
    }
    if (p->count == 0) {
        *out_len = 0;
        if (p->writers_mask == 0) {
            return IPC_ERR_NOT_FOUND;
        }
        return IPC_ERR_NONE;
    }
    uint64_t n = max < p->count ? max : p->count;
    if (n > PIPE_CHUNK_MAX) {
        n = PIPE_CHUNK_MAX;
    }
    for (uint64_t i = 0; i < n; i++) {
        out[i] = p->ring[(p->head + i) % PIPE_RING_SIZE];
    }
    p->head = (p->head + (uint32_t)n) % PIPE_RING_SIZE;
    p->count -= (uint32_t)n;
    *out_len = n;
    return IPC_ERR_NONE;
}
int64_t pipe_write(uint64_t handle, const uint8_t *in, uint64_t len, uint64_t *out_len) {
    int idx;
    uint32_t gen;
    if (!pipe_decode(handle, &idx, &gen)) {
        *out_len = 0;
        return IPC_ERR_NOT_FOUND;
    }
    pipe_obj_t *p = &pipe_pool[idx];
    if (p->generation != gen || pipe_is_free(p)) {
        *out_len = 0;
        return IPC_ERR_NOT_FOUND;
    }
    if (p->readers_mask == 0) {
        *out_len = 0;
        return IPC_ERR_NOT_FOUND;
    }
    uint32_t avail = PIPE_RING_SIZE - p->count;
    if (avail == 0) {
        *out_len = 0;
        return IPC_ERR_NONE;
    }
    uint64_t n = len < avail ? len : avail;
    if (n > PIPE_CHUNK_MAX) {
        n = PIPE_CHUNK_MAX;
    }
    uint32_t tail = (p->head + p->count) % PIPE_RING_SIZE;
    for (uint64_t i = 0; i < n; i++) {
        p->ring[(tail + i) % PIPE_RING_SIZE] = in[i];
    }
    p->count += (uint32_t)n;
    *out_len = n;
    return IPC_ERR_NONE;
}
int64_t pipe_close(tid_t caller, uint64_t handle, int is_write_end) {
    int idx;
    uint32_t gen;
    if (!pipe_decode(handle, &idx, &gen)) {
        return IPC_ERR_NOT_FOUND;
    }
    pipe_obj_t *p = &pipe_pool[idx];
    if (p->generation != gen) {
        return IPC_ERR_NOT_FOUND;
    }
    uint64_t bit = (1ULL << caller);
    if (is_write_end) {
        p->writers_mask &= ~bit;
    } else {
        p->readers_mask &= ~bit;
    }
    if (pipe_is_free(p)) {
        p->generation++;
    }
    return IPC_ERR_NONE;
}
int pipe_add_holder(tid_t tid, uint64_t handle, int is_write_end) {
    int idx;
    uint32_t gen;
    if (!pipe_decode(handle, &idx, &gen)) {
        return -1;
    }
    pipe_obj_t *p = &pipe_pool[idx];
    if (p->generation != gen) {
        return -1;
    }
    uint64_t bit = (1ULL << tid);
    if (is_write_end) {
        p->writers_mask |= bit;
    } else {
        p->readers_mask |= bit;
    }
    return 0;
}
void pipe_inherit_fork(tid_t parent, tid_t child) {
    uint64_t parent_bit = (1ULL << parent);
    uint64_t child_bit = (1ULL << child);
    for (int i = 0; i < PIPE_POOL_SIZE; i++) {
        pipe_obj_t *p = &pipe_pool[i];
        if (p->readers_mask & parent_bit) {
            p->readers_mask |= child_bit;
        }
        if (p->writers_mask & parent_bit) {
            p->writers_mask |= child_bit;
        }
    }
}
void pipe_invalidate_tcb_death(tid_t tid) {
    uint64_t bit = (1ULL << tid);
    for (int i = 0; i < PIPE_POOL_SIZE; i++) {
        pipe_obj_t *p = &pipe_pool[i];
        if ((p->readers_mask | p->writers_mask) & bit) {
            p->readers_mask &= ~bit;
            p->writers_mask &= ~bit;
            if (pipe_is_free(p)) {
                p->generation++;
            }
        }
    }
}
