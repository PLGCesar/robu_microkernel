#include "robu/untyped.h"
#include "robu/vm.h"
#include "robu/arch.h"
static paddr_t region_base;
static uint64_t region_size;
static uint64_t watermark;
void untyped_init(paddr_t base, uint64_t size) {
    region_base = base;
    region_size = size;
    watermark = 0;
}
paddr_t untyped_alloc(uint64_t size) {
    uint64_t aligned = (size + PAGE_SIZE_4K - 1) & ~(PAGE_SIZE_4K - 1);
    if (aligned == 0) {
        aligned = PAGE_SIZE_4K;
    }
    if (watermark + aligned > region_size) {
        return 0;
    }
    paddr_t frame = region_base + watermark;
    watermark += aligned;
    memset((void *)frame, 0, aligned);
    return frame;
}
void untyped_range(paddr_t *out_base, uint64_t *out_size) {
    *out_base = region_base;
    *out_size = region_size;
}
