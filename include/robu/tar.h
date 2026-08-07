#ifndef ROBU_TAR_H
#define ROBU_TAR_H
#include "robu/types.h"
int tar_find(const void *archive, uint64_t archive_len, const char *name,
             const uint8_t **out_data, uint64_t *out_size);
#endif
