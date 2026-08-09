#include "robu/virtio_blk.h"
#include "robu/dma.h"
#include "robu/kprintf.h"
#include "robu/arch.h"
#include "pci.h"
#include "portio.h"
// Legacy virtio-pci register offsets from the I/O-space BAR0 base
// (confirmed against the OASIS virtio spec's legacy-interface appendix,
// section 4.1.4.8, and cross-checked against QEMU's own
// include/standard-headers/linux/virtio_pci.h, during planning -- not
// assumed from a secondhand table). Valid when MSI-X is disabled, which
// QEMU's virtio-blk-pci is by default under a plain machine type.
#define VIRTIO_REG_DEVICE_FEATURES 0x00
#define VIRTIO_REG_GUEST_FEATURES  0x04
#define VIRTIO_REG_QUEUE_ADDRESS   0x08
#define VIRTIO_REG_QUEUE_SIZE      0x0C
#define VIRTIO_REG_QUEUE_SELECT    0x0E
#define VIRTIO_REG_QUEUE_NOTIFY    0x10
#define VIRTIO_REG_DEVICE_STATUS   0x12
#define VIRTIO_REG_ISR_STATUS      0x13
#define VIRTIO_REG_CONFIG          0x14
#define VIRTIO_STATUS_ACKNOWLEDGE 0x01
#define VIRTIO_STATUS_DRIVER      0x02
#define VIRTIO_STATUS_DRIVER_OK   0x04
#define VIRTIO_STATUS_FAILED      0x80
#define VIRTQ_DESC_F_NEXT  1
#define VIRTQ_DESC_F_WRITE 2
#define VIRTIO_BLK_T_IN  0
#define VIRTIO_BLK_T_OUT 1
#define VIRTIO_BLK_S_OK 0
#define VIRTIO_BLK_DATA_MAX 4096
#define VIRTIO_BLK_POLL_MAX_ITERS 10000000
// The legacy interface has no per-queue size negotiation: QUEUE_NUM is
// read-only, and the driver MUST lay out its ring at exactly that size
// (confirmed against QEMU's hw/virtio/virtio-pci.c -- the legacy I/O-port
// write path has no case for QUEUE_NUM, only modern's
// VIRTIO_PCI_COMMON_Q_SIZE can shrink virtio_queue_set_num()). A ring
// built for a smaller size silently desyncs from where the device expects
// avail/used to live -- this cap just bounds the DMA allocation; the
// actual size used is always dev_queue_size, read from the device.
#define VIRTIO_BLK_MAX_QUEUE_SIZE 1024
typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed)) virtq_desc_t;
typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed)) virtio_blk_req_hdr_t;
static int present;
static uint16_t io_base;
static uint64_t capacity_sectors;
static uint32_t queue_size;
static volatile virtq_desc_t *desc;
static volatile uint8_t *avail_base;
static volatile uint8_t *used_base;
static uint16_t next_avail_idx;
static uint16_t last_seen_used_idx;
static volatile virtio_blk_req_hdr_t *req_hdr;
static volatile uint8_t *req_status;
static volatile uint8_t *req_data;
// avail: {flags:u16, idx:u16, ring[queue_size]:u16}; used: {flags:u16,
// idx:u16, elem[queue_size]:{id:u32,len:u32}} -- see virtq_avail_t's
// spec-defined layout. Accessed via byte offset from a runtime-sized base
// (not a fixed-N struct) since queue_size is only known at boot, per
// device.
static inline volatile uint16_t *avail_idx_ptr(void) {
    return (volatile uint16_t *)(avail_base + 2);
}
static inline volatile uint16_t *avail_ring_ptr(uint32_t i) {
    return (volatile uint16_t *)(avail_base + 4 + 2 * i);
}
static inline volatile uint16_t *used_idx_ptr(void) {
    return (volatile uint16_t *)(used_base + 2);
}
static int poll_for_completion(void) {
    for (uint64_t i = 0; i < VIRTIO_BLK_POLL_MAX_ITERS; i++) {
        if (*used_idx_ptr() != last_seen_used_idx) {
            last_seen_used_idx = *used_idx_ptr();
            return 0;
        }
        asm volatile("pause");
    }
    return -1;
}
static int do_request(uint32_t type, uint64_t sector, uint32_t count, void *buf, int is_write) {
    if (!present) {
        return -1;
    }
    uint32_t bytes = count * 512;
    if (bytes > VIRTIO_BLK_DATA_MAX) {
        return -1;
    }
    req_hdr->type = type;
    req_hdr->reserved = 0;
    req_hdr->sector = sector;
    if (is_write) {
        memcpy((void *)req_data, buf, bytes);
    }
    *req_status = 0xFF;
    desc[0].addr = (uint64_t)(vaddr_t)req_hdr;
    desc[0].len = sizeof(virtio_blk_req_hdr_t);
    desc[0].flags = VIRTQ_DESC_F_NEXT;
    desc[0].next = 1;
    desc[1].addr = (uint64_t)(vaddr_t)req_data;
    desc[1].len = bytes;
    desc[1].flags = VIRTQ_DESC_F_NEXT | (is_write ? 0 : VIRTQ_DESC_F_WRITE);
    desc[1].next = 2;
    desc[2].addr = (uint64_t)(vaddr_t)req_status;
    desc[2].len = 1;
    desc[2].flags = VIRTQ_DESC_F_WRITE;
    desc[2].next = 0;
    asm volatile("" ::: "memory");
    *avail_ring_ptr(next_avail_idx % queue_size) = 0;
    asm volatile("" ::: "memory");
    next_avail_idx = (uint16_t)(next_avail_idx + 1);
    *avail_idx_ptr() = next_avail_idx;
    asm volatile("" ::: "memory");
    outw(io_base + VIRTIO_REG_QUEUE_NOTIFY, 0);
    if (poll_for_completion() != 0) {
        kprintf("[virtio_blk] request timed out\n");
        return -1;
    }
    if (*req_status != VIRTIO_BLK_S_OK) {
        kprintf("[virtio_blk] device reported error status=%u\n", (unsigned)*req_status);
        return -1;
    }
    if (!is_write) {
        memcpy(buf, (const void *)req_data, bytes);
    }
    return 0;
}
int virtio_blk_present(void) {
    return present;
}
uint64_t virtio_blk_capacity_sectors(void) {
    return capacity_sectors;
}
int virtio_blk_read(uint64_t sector, uint32_t count, void *buf) {
    return do_request(VIRTIO_BLK_T_IN, sector, count, buf, 0);
}
int virtio_blk_write(uint64_t sector, uint32_t count, const void *buf) {
    return do_request(VIRTIO_BLK_T_OUT, sector, count, (void *)buf, 1);
}
static uint64_t align4096(uint64_t x) {
    return (x + 4095) & ~(uint64_t)4095;
}
int virtio_blk_init(void) {
    pci_addr_t addr;
    if (pci_find_device(0x1af4, 0x1001, &addr) != 0) {
        return -1;
    }
    uint16_t cmd = pci_cfg_read16(addr, 0x04);
    pci_cfg_write16(addr, 0x04, cmd | 0x1 /* I/O space */ | 0x4 /* bus master */);
    uint32_t bar0 = pci_cfg_read32(addr, 0x10);
    if (!(bar0 & 0x1)) {
        kprintf("[virtio_blk] BAR0 is not an I/O-space BAR, giving up\n");
        return -1;
    }
    io_base = (uint16_t)(bar0 & ~0x3u);
    outb(io_base + VIRTIO_REG_DEVICE_STATUS, 0);
    outb(io_base + VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    outb(io_base + VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);
    // Negotiate no optional features -- plain sector read/write needs none.
    outl(io_base + VIRTIO_REG_GUEST_FEATURES, 0);
    outw(io_base + VIRTIO_REG_QUEUE_SELECT, 0);
    uint16_t dev_queue_size = inw(io_base + VIRTIO_REG_QUEUE_SIZE);
    if (dev_queue_size == 0 || dev_queue_size > VIRTIO_BLK_MAX_QUEUE_SIZE) {
        kprintf("[virtio_blk] device queue size %u out of the supported [1, %u] range, giving up\n",
                dev_queue_size, (unsigned)VIRTIO_BLK_MAX_QUEUE_SIZE);
        outb(io_base + VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_FAILED);
        return -1;
    }
    queue_size = dev_queue_size;
    uint64_t desc_bytes = 16ull * queue_size;
    uint64_t avail_bytes = 6ull + 2ull * queue_size;
    uint64_t used_off = align4096(desc_bytes + avail_bytes);
    uint64_t used_bytes = 6ull + 8ull * queue_size;
    uint64_t queue_total = used_off + align4096(used_bytes);
    paddr_t queue_paddr = dma_region_alloc(queue_total);
    paddr_t hdr_paddr = dma_region_alloc(sizeof(virtio_blk_req_hdr_t));
    paddr_t status_paddr = dma_region_alloc(1);
    paddr_t data_paddr = dma_region_alloc(VIRTIO_BLK_DATA_MAX);
    if (!queue_paddr || !hdr_paddr || !status_paddr || !data_paddr) {
        kprintf("[virtio_blk] DMA region exhausted, giving up\n");
        outb(io_base + VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_FAILED);
        return -1;
    }
    memset((void *)(vaddr_t)queue_paddr, 0, queue_total);
    memset((void *)(vaddr_t)hdr_paddr, 0, sizeof(virtio_blk_req_hdr_t));
    memset((void *)(vaddr_t)status_paddr, 0, 1);
    desc = (volatile virtq_desc_t *)(vaddr_t)queue_paddr;
    avail_base = (volatile uint8_t *)((vaddr_t)queue_paddr + desc_bytes);
    used_base = (volatile uint8_t *)((vaddr_t)queue_paddr + used_off);
    req_hdr = (volatile virtio_blk_req_hdr_t *)(vaddr_t)hdr_paddr;
    req_status = (volatile uint8_t *)(vaddr_t)status_paddr;
    req_data = (volatile uint8_t *)(vaddr_t)data_paddr;
    next_avail_idx = 0;
    last_seen_used_idx = 0;
    outl(io_base + VIRTIO_REG_QUEUE_ADDRESS, (uint32_t)(queue_paddr >> 12));
    uint32_t cap_lo = inl(io_base + VIRTIO_REG_CONFIG);
    uint32_t cap_hi = inl(io_base + VIRTIO_REG_CONFIG + 4);
    capacity_sectors = (uint64_t)cap_lo | ((uint64_t)cap_hi << 32);
    outb(io_base + VIRTIO_REG_DEVICE_STATUS,
         VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_DRIVER_OK);
    present = 1;
    kprintf("[virtio_blk] ready: io_base=0x%x queue_size=%u capacity=%lu sectors (%lu KiB)\n",
            io_base, queue_size, capacity_sectors, capacity_sectors / 2);
    return 0;
}
