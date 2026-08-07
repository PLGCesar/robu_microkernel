#include <signal.h>
#include <errno.h>
void (*signal(int signum, void (*handler)(int)))(int) {
    (void)signum;
    (void)handler;
    return SIG_DFL;
}
int kill(int pid, int sig) {
    (void)pid;
    (void)sig;
    errno = ENOSYS;
    return -1;
}
int raise(int sig) {
    (void)sig;
    errno = ENOSYS;
    return -1;
}
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    (void)signum;
    (void)act;
    if (oldact) {
        *oldact = (struct sigaction){0};
    }
    return 0;
}
int sigemptyset(sigset_t *set) {
    if (set) {
        *set = 0;
    }
    return 0;
}
int sigfillset(sigset_t *set) {
    if (set) {
        *set = ~0UL;
    }
    return 0;
}
int sigaddset(sigset_t *set, int signum) {
    if (set && signum > 0 && signum < NSIG) {
        *set |= (1UL << (signum - 1));
    }
    return 0;
}
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    (void)how;
    (void)set;
    if (oldset) {
        *oldset = 0;
    }
    return 0;
}
