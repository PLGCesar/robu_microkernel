#include "robu/cmdline.h"
#include "robu/arch.h"
#include "robu/types.h"
#define CMDLINE_BUF_SIZE   256
#define CMDLINE_MAX_TOKENS 16
static char cmdline_buf[CMDLINE_BUF_SIZE];
static const char *keys[CMDLINE_MAX_TOKENS];
static const char *values[CMDLINE_MAX_TOKENS];
static int ntokens;
void cmdline_parse(const char *raw) {
    ntokens = 0;
    if (!raw) {
        cmdline_buf[0] = '\0';
        return;
    }
    size_t len = strlen(raw);
    if (len >= sizeof(cmdline_buf)) {
        len = sizeof(cmdline_buf) - 1;
    }
    memcpy(cmdline_buf, raw, len);
    cmdline_buf[len] = '\0';
    char *p = cmdline_buf;
    while (*p && ntokens < CMDLINE_MAX_TOKENS) {
        while (*p == ' ') p++;
        if (!*p) break;
        char *key = p;
        while (*p && *p != ' ' && *p != '=') p++;
        if (*p == '=') {
            *p++ = '\0';
            char *value = p;
            while (*p && *p != ' ') p++;
            if (*p) {
                *p++ = '\0';
            }
            keys[ntokens] = key;
            values[ntokens] = value;
            ntokens++;
        } else {
            if (*p) p++;
        }
    }
}
const char *cmdline_get(const char *key) {
    size_t klen = strlen(key);
    for (int i = 0; i < ntokens; i++) {
        if (strncmp(keys[i], key, klen + 1) == 0) {
            return values[i];
        }
    }
    return NULL;
}
