#include "robu/tar.h"
#include "robu/arch.h"
#define USTAR_BLOCK 512
typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} __attribute__((packed)) ustar_header_t;
_Static_assert(sizeof(ustar_header_t) == USTAR_BLOCK,
               "ustar header must be exactly one 512-byte block");
static uint64_t parse_octal(const char *s, int len) {
    int i = 0;
    while (i < len && s[i] == ' ') i++;
    uint64_t v = 0;
    while (i < len && s[i] >= '0' && s[i] <= '7') {
        v = (v << 3) | (uint64_t)(s[i] - '0');
        i++;
    }
    return v;
}
static int is_all_zero(const uint8_t *block) {
    for (int i = 0; i < USTAR_BLOCK; i++) {
        if (block[i] != 0) return 0;
    }
    return 1;
}
int tar_find(const void *archive, uint64_t archive_len, const char *name,
             const uint8_t **out_data, uint64_t *out_size) {
    const uint8_t *base = (const uint8_t *)archive;
    uint64_t off = 0;
    size_t name_len = strlen(name);
    while (off + USTAR_BLOCK <= archive_len) {
        const uint8_t *block = base + off;
        if (is_all_zero(block)) {
            break;
        }
        const ustar_header_t *hdr = (const ustar_header_t *)block;
        if (strncmp(hdr->magic, "ustar", 5) != 0) {
            break;
        }
        uint64_t size = parse_octal(hdr->size, sizeof(hdr->size));
        off += USTAR_BLOCK;
        int is_regular = (hdr->typeflag == '0' || hdr->typeflag == '\0');
        if (is_regular && strncmp(hdr->name, name, name_len + 1) == 0) {
            *out_data = base + off;
            *out_size = size;
            return 0;
        }
        uint64_t data_blocks = (size + USTAR_BLOCK - 1) / USTAR_BLOCK;
        off += data_blocks * USTAR_BLOCK;
    }
    return -1;
}
