#ifndef ROBU_LIBC_TERMIOS_H
#define ROBU_LIBC_TERMIOS_H
typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;
#define NCCS 32
struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_line;
    cc_t c_cc[NCCS];
    speed_t c_ispeed;
    speed_t c_ospeed;
};
struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};
#define ICANON  0x0002
#define ECHO    0x0008
#define ISIG    0x0001
#define IUTF8   0x4000
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define TIOCNOTTY  0x5422
#define ICRNL   0x0100
#define IXANY   0x0800
#define OPOST   0x0001
#define ONLCR   0x0004
#define CS8     0x0030
#define CREAD   0x0800
#define ECHOE   0x0010
#define ECHOK   0x0020
#define ECHOCTL 0x0200
#define ECHOKE  0x0800
#define IEXTEN  0x8000
int cfsetspeed(struct termios *t, speed_t speed);
#define TCIFLUSH  0
#define TCOFLUSH  1
#define TCIOFLUSH 2
int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);
int tcflush(int fd, int queue_selector);
int tcflow(int fd, int action);
int tcdrain(int fd);
int tcgetpgrp(int fd);
int tcsetpgrp(int fd, int pgrp);
speed_t cfgetispeed(const struct termios *t);
speed_t cfgetospeed(const struct termios *t);
int cfsetispeed(struct termios *t, speed_t speed);
int cfsetospeed(struct termios *t, speed_t speed);
void cfmakeraw(struct termios *t);
#endif
