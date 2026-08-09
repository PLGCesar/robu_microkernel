#ifndef ROBU_VIRTIO_BLK_H
#define ROBU_VIRTIO_BLK_H
#include "robu/types.h"
// Legacy-transport, polled virtio-blk driver (see docs discovered during
// planning: disable-legacy=auto on a plain QEMU machine type means legacy
// transport -- pure port I/O -- is available by default, so this needs no
// MMIO-mapping infrastructure). Kernel-side only: ring-3 has no port I/O
// capability at all in this kernel (no TSS I/O bitmap, IOPL=0), so every
// caller reaches this through IPC_FLAG_BLK_IO (src/core/ipc.c), not
// directly.
//
// Scans PCI for vendor=0x1af4 device=0x1001 (virtio-blk, transitional ID).
// Returns 0 if found and successfully brought up, -1 if no such device is
// present -- not fatal, disk persistence just isn't available this boot.
int virtio_blk_init(void);
int virtio_blk_present(void);
uint64_t virtio_blk_capacity_sectors(void);
// sector is a 512-byte LBA; buf must hold count*512 bytes. Both block
// until the single in-flight request completes or the bounded poll times
// out. Returns 0 on success, -1 on timeout or device-reported error.
int virtio_blk_read(uint64_t sector, uint32_t count, void *buf);
int virtio_blk_write(uint64_t sector, uint32_t count, const void *buf);
#endif
