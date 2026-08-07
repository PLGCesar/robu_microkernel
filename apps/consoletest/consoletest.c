#include <unistd.h>
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    char buf[64];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n > 0) {
        write(STDOUT_FILENO, buf, (size_t)n);
    }
    return 0;
}
