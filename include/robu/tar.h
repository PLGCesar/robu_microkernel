#ifndef ROBU_TAR_H
#define ROBU_TAR_H
#include "robu/types.h"
int tar_find(const void *archive, uint64_t archive_len, const char *name,
             const uint8_t **out_data, uint64_t *out_size);
// Returns the `index`-th regular-file entry's name (NUL-terminated into
// name_out, truncated to name_max) and size, or -1 if index is past the
// last entry. Entries are numbered in on-disk order, matching tar_find()'s
// own walk -- used for READDIR on the /boot mount (src/boot/bootfs.c),
// since tar_find() itself only supports single-name lookup.
int tar_iterate(const void *archive, uint64_t archive_len, uint64_t index,
                 char *name_out, uint64_t name_max, uint64_t *out_size);
#endif
