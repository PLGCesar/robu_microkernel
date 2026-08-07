#ifndef ROBU_LIBC_INTERNAL_H
#define ROBU_LIBC_INTERNAL_H
#include <sys/types.h>
#include "robu/types.h"
void __libc_heap_init(uint64_t heap_base);
extern const char *__libc_progname;
void __libc_stdio_init(void);
int __libc_fd_is_open(int fd);
int __libc_fd_is_ramfs_root(int fd);
int __libc_fd_ramfs_dir_ino(int fd, uint64_t *out_ino);
void __libc_resolve_path(const char *path, char *out, size_t out_size);
int __libc_spawn(const char *name, char *const argv[], char *const envp[]);
int __libc_fd_export(int fd, uint32_t *out_kind, uint64_t *out_handle);
void __libc_fd_inherit(uint64_t spawn_info);
int __libc_fd_alloc_pipe(uint64_t handle, int is_write_end);
#endif
