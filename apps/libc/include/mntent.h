#ifndef ROBU_LIBC_MNTENT_H
#define ROBU_LIBC_MNTENT_H
#include <stdio.h>
struct mntent {
    char *mnt_fsname;
    char *mnt_dir;
    char *mnt_type;
    char *mnt_opts;
    int mnt_freq;
    int mnt_passno;
};
FILE *setmntent(const char *filename, const char *type);
struct mntent *getmntent(FILE *f);
int endmntent(FILE *f);
#endif
