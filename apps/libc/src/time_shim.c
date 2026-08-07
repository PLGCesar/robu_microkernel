#include <time.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "robu/types.h"
#include "robu/kinfo.h"
time_t time(time_t *tloc) {
    const volatile kinfo_page_t *k = kinfo_user();
    uint64_t ticks = kinfo_read_ticks(k);
    uint32_t hz = k->clock_hz ? k->clock_hz : 1;
    time_t now = (time_t)(ticks / hz);
    if (tloc) {
        *tloc = now;
    }
    return now;
}
static long long days_from_civil(long long y, int m, int d) {
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    long long yoe = y - era * 400;
    long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
static void civil_from_days(long long z, int *y, int *m, int *d) {
    z += 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    long long doe = z - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long yy = yoe + era * 400;
    long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long long mp = (5 * doy + 2) / 153;
    long long dd = doy - (153 * mp + 2) / 5 + 1;
    long long mm = mp + (mp < 10 ? 3 : -9);
    *y = (int)(yy + (mm <= 2));
    *m = (int)mm;
    *d = (int)dd;
}
time_t mktime(struct tm *tm) {
    long long days = days_from_civil(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    time_t t = (time_t)(days * 86400LL + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec);
    tm->tm_wday = (int)(((days % 7) + 11) % 7);
    long long yday_jan1 = days_from_civil(tm->tm_year + 1900, 1, 1);
    tm->tm_yday = (int)(days - yday_jan1);
    return t;
}
struct tm *gmtime_r(const time_t *timep, struct tm *result) {
    long long t = *timep;
    long long days = t >= 0 ? t / 86400 : (t - 86399) / 86400;
    long long secs = t - days * 86400;
    int y, mo, d;
    civil_from_days(days, &y, &mo, &d);
    memset(result, 0, sizeof(*result));
    result->tm_year = y - 1900;
    result->tm_mon = mo - 1;
    result->tm_mday = d;
    result->tm_hour = (int)(secs / 3600);
    result->tm_min = (int)((secs / 60) % 60);
    result->tm_sec = (int)(secs % 60);
    result->tm_wday = (int)(((days % 7) + 11) % 7);
    result->tm_yday = (int)(days - days_from_civil(y, 1, 1));
    return result;
}
struct tm *localtime_r(const time_t *timep, struct tm *result) {
    return gmtime_r(timep, result);
}
struct tm *gmtime(const time_t *timep) {
    static struct tm result;
    return gmtime_r(timep, &result);
}
struct tm *localtime(const time_t *timep) {
    return gmtime(timep);
}
__attribute__((target("sse,sse2")))
double difftime(time_t a, time_t b) {
    return (double)(a - b);
}
void tzset(void) {
}
int nanosleep(const struct timespec *req, struct timespec *rem) {
    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    (void)req;
    return 0;
}
int clock_gettime(int clk_id, struct timespec *tp) {
    (void)clk_id;
    const volatile kinfo_page_t *k = kinfo_user();
    uint64_t ticks = kinfo_read_ticks(k);
    uint32_t hz = k->clock_hz ? k->clock_hz : 1;
    tp->tv_sec = (time_t)(ticks / hz);
    tp->tv_nsec = (long)((ticks % hz) * (1000000000ULL / hz));
    return 0;
}
static const char *const month_names[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December",
};
static const char *const day_names[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
};
static void put_num(char **d, char *end, int v, int width) {
    char tmp[16];
    int n = 0;
    int neg = v < 0;
    unsigned uv = neg ? (unsigned)(-v) : (unsigned)v;
    do {
        tmp[n++] = (char)('0' + uv % 10);
        uv /= 10;
    } while (uv);
    while (n < width) {
        tmp[n++] = '0';
    }
    if (neg && *d < end) {
        *(*d)++ = '-';
    }
    while (n > 0 && *d < end) {
        *(*d)++ = tmp[--n];
    }
}
static void put_str(char **d, char *end, const char *s, int n) {
    while (n-- > 0 && *s && *d < end) {
        *(*d)++ = *s++;
    }
}
size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) {
    char *d = s, *end = s + (max ? max - 1 : 0);
    for (; *format; format++) {
        if (*format != '%') {
            if (d < end) {
                *d++ = *format;
            }
            continue;
        }
        format++;
        switch (*format) {
        case 'Y': put_num(&d, end, tm->tm_year + 1900, 1); break;
        case 'y': put_num(&d, end, (tm->tm_year + 1900) % 100, 2); break;
        case 'm': put_num(&d, end, tm->tm_mon + 1, 2); break;
        case 'd': put_num(&d, end, tm->tm_mday, 2); break;
        case 'e': put_num(&d, end, tm->tm_mday, 1); break;
        case 'H': put_num(&d, end, tm->tm_hour, 2); break;
        case 'M': put_num(&d, end, tm->tm_min, 2); break;
        case 'S': put_num(&d, end, tm->tm_sec, 2); break;
        case 'b':
        case 'h':
            put_str(&d, end, month_names[tm->tm_mon % 12], 3);
            break;
        case 'B':
            put_str(&d, end, month_names[tm->tm_mon % 12], 64);
            break;
        case 'a':
            put_str(&d, end, day_names[tm->tm_wday % 7], 3);
            break;
        case 'A':
            put_str(&d, end, day_names[tm->tm_wday % 7], 64);
            break;
        case 'T':
            put_num(&d, end, tm->tm_hour, 2);
            if (d < end) *d++ = ':';
            put_num(&d, end, tm->tm_min, 2);
            if (d < end) *d++ = ':';
            put_num(&d, end, tm->tm_sec, 2);
            break;
        case '%':
            if (d < end) *d++ = '%';
            break;
        case '\0':
            format--;
            break;
        default:
            if (d < end) *d++ = '%';
            if (d < end) *d++ = *format;
            break;
        }
    }
    if (max) {
        *d = '\0';
    }
    return (size_t)(d - s);
}
static int parse_int(const char **s, int maxdigits, int *out) {
    int n = 0, val = 0;
    while (n < maxdigits && isdigit((unsigned char)**s)) {
        val = val * 10 + (**s - '0');
        (*s)++;
        n++;
    }
    if (n == 0) {
        return 0;
    }
    *out = val;
    return 1;
}
char *strptime(const char *s, const char *format, struct tm *tm) {
    for (; *format; format++) {
        if (*format != '%') {
            if (isspace((unsigned char)*format)) {
                while (isspace((unsigned char)*s)) {
                    s++;
                }
            } else if (*s == *format) {
                s++;
            } else {
                return 0;
            }
            continue;
        }
        format++;
        int n, val;
        switch (*format) {
        case 'Y':
            if (!parse_int(&s, 8, &val)) return 0;
            tm->tm_year = val - 1900;
            break;
        case 'y':
            if (!parse_int(&s, 2, &val)) return 0;
            tm->tm_year = (val < 69 ? val + 100 : val);
            break;
        case 'C':
            if (!parse_int(&s, 2, &val)) return 0;
            tm->tm_year = (tm->tm_year % 100) + val * 100 - 1900;
            break;
        case 'm':
            if (!parse_int(&s, 2, &val)) return 0;
            tm->tm_mon = val - 1;
            break;
        case 'd':
        case 'e':
            while (isspace((unsigned char)*s)) s++;
            if (!parse_int(&s, 2, &val)) return 0;
            tm->tm_mday = val;
            break;
        case 'H':
            if (!parse_int(&s, 2, &val)) return 0;
            tm->tm_hour = val;
            break;
        case 'M':
            if (!parse_int(&s, 2, &val)) return 0;
            tm->tm_min = val;
            break;
        case 'S':
            if (!parse_int(&s, 2, &val)) return 0;
            tm->tm_sec = val;
            break;
        case 'T':
            if (!parse_int(&s, 2, &tm->tm_hour) || *s++ != ':'
                || !parse_int(&s, 2, &tm->tm_min) || *s++ != ':'
                || !parse_int(&s, 2, &tm->tm_sec)) {
                return 0;
            }
            break;
        case 'a':
        case 'A':
            n = 0;
            for (int i = 0; i < 7; i++) {
                size_t l = strlen(day_names[i]);
                if (strncasecmp(s, day_names[i], 3) == 0) {
                    tm->tm_wday = i;
                    n = (strncasecmp(s, day_names[i], l) == 0) ? (int)l : 3;
                    break;
                }
            }
            if (!n) return 0;
            s += n;
            break;
        case 'b':
        case 'h':
        case 'B':
            n = 0;
            for (int i = 0; i < 12; i++) {
                size_t l = strlen(month_names[i]);
                if (strncasecmp(s, month_names[i], 3) == 0) {
                    tm->tm_mon = i;
                    n = (strncasecmp(s, month_names[i], l) == 0) ? (int)l : 3;
                    break;
                }
            }
            if (!n) return 0;
            s += n;
            break;
        case 'Z':
            while (*s && !isspace((unsigned char)*s)) {
                s++;
            }
            break;
        case '%':
            if (*s != '%') return 0;
            s++;
            break;
        default:
            return 0;
        }
    }
    return (char *)s;
}
