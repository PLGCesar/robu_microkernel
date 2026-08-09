#ifndef ROBU_DMA_H
#define ROBU_DMA_H
#include "robu/types.h"
// A second untyped-style carve-out (see kmain()'s existing untyped region,
// src/main.c) for the handful of permanent, physically-contiguous buffers
// virtio_blk_init() needs (the legacy virtqueue, and header/status/data
// scratch buffers) -- pmm_alloc()'s 8 independent per-color free lists
// can't serve a contiguous multi-page request, so this is a separate bump
// allocator, not a pmm_alloc() caller.
void dma_region_init(paddr_t base, uint64_t size);
// Bump-allocates `bytes`, rounded up to a 4096-byte boundary (every caller
// wants page alignment: the virtqueue by spec, everything else for
// simplicity). No free -- matches untyped/captable's own "fixed handful of
// permanent boot-time allocations" convention. Returns 0 (never a valid
// address in this carve-out) if the region is exhausted.
paddr_t dma_region_alloc(uint64_t bytes);
#endif
