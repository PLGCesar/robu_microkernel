#ifndef ROBU_ROOTFS_H
#define ROBU_ROOTFS_H
#include "robu/types.h"
int rootfs_lookup(const char *name, const uint8_t **out_start, const uint8_t **out_end);
#endif
