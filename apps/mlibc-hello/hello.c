#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

int main(int argc, char **argv) {
    printf("hello from mlibc on robu\n");
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }

    const char *path = "/var/tmp/mlibc-hello.txt";
    FILE *f = fopen(path, "w");
    if (!f) {
        printf("fopen(w) failed: %s\n", strerror(errno));
        return 1;
    }
    fputs("hello from mlibc file io\n", f);
    fclose(f);

    struct stat st;
    if (stat(path, &st) != 0) {
        printf("stat failed: %s\n", strerror(errno));
        return 1;
    }
    printf("stat: size=%ld mode=0%o\n", (long)st.st_size, (unsigned)st.st_mode & 0777);

    f = fopen(path, "r");
    if (!f) {
        printf("fopen(r) failed: %s\n", strerror(errno));
        return 1;
    }
    char line[128];
    if (fgets(line, sizeof(line), f)) {
        printf("read back: %s", line);
    }
    fclose(f);

    DIR *d = opendir("/var/tmp");
    if (!d) {
        printf("opendir failed: %s\n", strerror(errno));
        return 1;
    }
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        printf("entry: %s\n", de->d_name);
    }
    closedir(d);

    char cwd[64];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("cwd: %s\n", cwd);
    }

    printf("mlibc-hello: all checks done\n");
    return 0;
}
