#ifndef ROBU_ROOTFS_H
#define ROBU_ROOTFS_H
#include "robu/types.h"
int rootfs_lookup(const char *name, const uint8_t **out_start, const uint8_t **out_end);

int rootfs_readdir(uint64_t index, char *name_out, uint64_t name_max, uint64_t *out_size);
#endif
