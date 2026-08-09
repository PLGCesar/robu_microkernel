#ifndef ROBU_VIRTIO_BLK_H
#define ROBU_VIRTIO_BLK_H
#include "robu/types.h"

int virtio_blk_init(void);
int virtio_blk_present(void);
uint64_t virtio_blk_capacity_sectors(void);

int virtio_blk_read(uint64_t sector, uint32_t count, void *buf);
int virtio_blk_write(uint64_t sector, uint32_t count, const void *buf);
#endif
