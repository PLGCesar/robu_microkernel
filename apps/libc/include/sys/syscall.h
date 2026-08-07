#ifndef ROBU_LIBC_SYS_SYSCALL_H
#define ROBU_LIBC_SYS_SYSCALL_H
#define __NR_copy_file_range 326
#define __NR_memfd_create    319
#define __NR_pivot_root      155
long syscall(long number, ...);
#endif
