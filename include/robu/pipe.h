#ifndef ROBU_PIPE_H
#define ROBU_PIPE_H
#include "robu/types.h"
#define PIPE_POOL_SIZE 16
#define PIPE_RING_SIZE 1024
#define PIPE_CHUNK_MAX 32
int64_t pipe_create(tid_t creator, uint64_t *out_handle);
int64_t pipe_read(uint64_t handle, uint8_t *out, uint64_t max, uint64_t *out_len);
int64_t pipe_write(uint64_t handle, const uint8_t *in, uint64_t len, uint64_t *out_len);
int64_t pipe_close(tid_t caller, uint64_t handle, int is_write_end);
int pipe_add_holder(tid_t tid, uint64_t handle, int is_write_end);
void pipe_inherit_fork(tid_t parent, tid_t child);
void pipe_invalidate_tcb_death(tid_t tid);
#endif
