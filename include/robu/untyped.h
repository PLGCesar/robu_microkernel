#ifndef ROBU_UNTYPED_H
#define ROBU_UNTYPED_H
#include "robu/types.h"
void untyped_init(paddr_t base, uint64_t size);
paddr_t untyped_alloc(uint64_t size);
void untyped_range(paddr_t *out_base, uint64_t *out_size);
#endif
