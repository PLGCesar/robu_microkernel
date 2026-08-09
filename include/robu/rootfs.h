#ifndef ROBU_ROOTFS_H
#define ROBU_ROOTFS_H
#include "robu/types.h"
int rootfs_lookup(const char *name, const uint8_t **out_start, const uint8_t **out_end);
// Wraps tar_iterate() over the kernel-resident rootfs_buf -- used by
// IPC_FLAG_BOOTFS's READDIR sub-op (src/core/ipc.c) since bootfs
// (apps/bootfs/bootfs.c) has no direct access to kernel memory.
int rootfs_readdir(uint64_t index, char *name_out, uint64_t name_max, uint64_t *out_size);
#endif
