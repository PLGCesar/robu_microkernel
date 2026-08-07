#include <stdlib.h>
#include <string.h>
#include "libc_internal.h"
typedef struct block_header {
    size_t size;
    struct block_header *next_free;
} block_header_t;
#define ALIGN_UP(n, a) (((n) + ((a) - 1)) & ~((a) - 1))
#define MIN_ALLOC 16
static uint8_t *g_heap_cursor;
static block_header_t *g_free_list;
void __libc_heap_init(uint64_t heap_base) {
    g_heap_cursor = (uint8_t *)heap_base;
    g_free_list = 0;
}
static block_header_t *find_free_block(size_t size) {
    block_header_t **link = &g_free_list;
    while (*link) {
        if ((*link)->size >= size) {
            block_header_t *found = *link;
            *link = found->next_free;
            return found;
        }
        link = &(*link)->next_free;
    }
    return 0;
}
void *malloc(size_t size) {
    if (size == 0) {
        size = 1;
    }
    size = ALIGN_UP(size, MIN_ALLOC);
    block_header_t *blk = find_free_block(size);
    if (!blk) {
        blk = (block_header_t *)g_heap_cursor;
        g_heap_cursor += sizeof(block_header_t) + size;
        blk->size = size;
    }
    blk->next_free = 0;
    return (void *)(blk + 1);
}
void free(void *ptr) {
    if (!ptr) {
        return;
    }
    block_header_t *blk = (block_header_t *)ptr - 1;
    blk->next_free = g_free_list;
    g_free_list = blk;
}
void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    if (nmemb != 0 && total / nmemb != size) {
        return 0;
    }
    void *p = malloc(total);
    if (p) {
        memset(p, 0, total);
    }
    return p;
}
void *realloc(void *ptr, size_t size) {
    if (!ptr) {
        return malloc(size);
    }
    if (size == 0) {
        free(ptr);
        return 0;
    }
    block_header_t *blk = (block_header_t *)ptr - 1;
    if (blk->size >= size) {
        return ptr;
    }
    void *newptr = malloc(size);
    if (newptr) {
        memcpy(newptr, ptr, blk->size);
        free(ptr);
    }
    return newptr;
}
