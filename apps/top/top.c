#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/ipc.h"
#include "robu/kinfo.h"
#include "robu/vfs.h"

static tid_t devfs_tid = 0;
static int64_t console_h = -1;

/* Helpers de E/S e Formatação de Texto */
static int str_len(const char *s) {
    int l = 0;
    while (s[l]) l++;
    return l;
}

static void print_str(const char *s) {
    uint64_t len = (uint64_t)str_len(s);
    if (len > 0 && console_h >= 0) {
        vfs_write(devfs_tid, (uint64_t)console_h, s, len);
    }
}

static void print_num(uint64_t n) {
    if (n == 0) { print_str("0"); return; }
    char buf[24], rev[24];
    int r = 0, p = 0;
    while (n > 0) { rev[r++] = '0' + (n % 10); n /= 10; }
    while (r > 0) buf[p++] = rev[--r];
    buf[p] = '\0';
    print_str(buf);
}

static void print_pad_num(uint64_t n, int width) {
    char buf[24], rev[24];
    int r = 0, p = 0;
    if (n == 0) {
        rev[r++] = '0';
    } else {
        while (n > 0) { rev[r++] = '0' + (n % 10); n /= 10; }
    }
    int spaces = width - r;
    while (spaces-- > 0) print_str(" ");
    while (r > 0) buf[p++] = rev[--r];
    buf[p] = '\0';
    print_str(buf);
}

static void print_pad_str(const char *s, int width) {
    int len = str_len(s);
    print_str(s);
    int spaces = width - len;
    while (spaces-- > 0) print_str(" ");
}

static void set_raw_mode(int enable) {
    msg_regs_t m = (msg_regs_t){0};
    m.word[0] = 10; // SYS_INFO_CAT_CONSOLE_MODE
    m.word[1] = enable ? 1 : 0;
    robu_ipc_raw(0, 0, IPC_FLAG_SYS_INFO, &m, NULL);
}

static int read_key(void) {
    uint8_t c;
    int64_t n = vfs_read(devfs_tid, (uint64_t)console_h, &c, 1);
    if (n > 0) return (int)c;
    return -1;
}

static const char *state_to_str(uint64_t st) {
    switch (st) {
    case 0: return "RUNNING";
    case 1: return "READY";
    case 2: return "WAIT_RECV";
    case 3: return "WAIT_SEND";
    case 4: return "WAIT_PF";
    case 5: return "SLEEPING";
    case 6: return "WAIT_NOTIF";
    case 7: return "DEAD";
    case 8: return "ZOMBIE";
    case 9: return "WAIT_CHILD";
    default: return "UNKNOWN";
    }
}

static int get_thread_info(uint32_t tid, uint64_t *state, uint64_t *prio,
                           int64_t *exit_status, uint64_t *parent_tid,
                           char *name_out) {
    msg_regs_t q = (msg_regs_t){0};
    q.word[0] = tid;
    int64_t rc = robu_ipc_raw(0, 0, IPC_FLAG_THREAD_INFO, &q, NULL);
    if (rc != 0) return -1;

    *state = q.word[0];
    *prio = q.word[1];
    *exit_status = (int64_t)q.word[2];
    *parent_tid = q.word[3];

    uint64_t w13 = q.word[4];
    uint64_t w14 = q.word[5];

    int ni = 0;
    for (int i = 0; i < 8; i++) {
        uint8_t c = (uint8_t)(w13 >> (8 * i));
        if (!c) break;
        name_out[ni++] = (char)c;
    }
    if (ni == 8) {
        for (int i = 0; i < 8; i++) {
            uint8_t c = (uint8_t)(w14 >> (8 * i));
            if (!c) break;
            name_out[ni++] = (char)c;
        }
    }
    name_out[ni] = '\0';
    return 0;
}

static void render_top(void) {
    print_str("\033[0m\033[2J\033[H");
    print_str("\033[44;37;1m --- Robu Microkernel Task Manager (top) --- \033[0m\r\n");

    /* Uptime e Dados de Clock */
    const volatile kinfo_page_t *k = kinfo_user();
    uint64_t ticks = kinfo_read_ticks(k);
    uint32_t hz = k->clock_hz ? k->clock_hz : 100;
    uint64_t uptime_sec = ticks / hz;

    print_str("\033[36mUptime:\033[0m ");
    print_num(uptime_sec);
    print_str("s (");
    print_num(ticks);
    print_str(" ticks) | \033[36mHZ:\033[0m ");
    print_num(hz);
    print_str(" | \033[36mCPUs:\033[0m ");
    print_num(k->cpu_count);
    print_str("\r\n");

    /* Informações de Memória (PMM) */
    msg_regs_t mem_q = (msg_regs_t){0};
    mem_q.word[0] = 0; 
    robu_ipc_raw(0, 0, IPC_FLAG_SYS_INFO, &mem_q, NULL);
    uint64_t total_frames = mem_q.word[0];
    uint64_t free_frames = mem_q.word[1];

    uint64_t total_kib = (total_frames * 4096) / 1024;
    uint64_t free_kib = (free_frames * 4096) / 1024;
    uint64_t used_kib = total_kib > free_kib ? total_kib - free_kib : 0;

    print_str("\033[36mMem:\033[0m ");
    print_num(total_kib);
    print_str(" KiB total | \033[32m");
    print_num(free_kib);
    print_str(" KiB free\033[0m | \033[33m");
    print_num(used_kib);
    print_str(" KiB used\033[0m\r\n");

    /* Estatísticas do Escalonador */
    msg_regs_t sched_q = (msg_regs_t){0};
    sched_q.word[0] = 1; 
    robu_ipc_raw(0, 0, IPC_FLAG_SYS_INFO, &sched_q, NULL);
    print_str("\033[36mSched:\033[0m Switches: ");
    print_num(sched_q.word[0]);
    print_str(" | Preempts: ");
    print_num(sched_q.word[1]);
    print_str(" | IPC Msgs: ");
    print_num(sched_q.word[2]);
    print_str("\r\n");

    print_str("------------------------------------------------------------\r\n");
    print_str("\033[1mTID   NAME            STATE       PRIO  PARENT  EXIT\033[0m\r\n");
    print_str("------------------------------------------------------------\r\n");

    /* Varredura da Tabela de Threads */
    int active_count = 0;
    for (uint32_t tid = 1; tid < 64; tid++) {
        uint64_t state, prio, parent_tid;
        int64_t exit_status;
        char name[17];

        if (get_thread_info(tid, &state, &prio, &exit_status, &parent_tid, name) != 0) {
            continue;
        }

        active_count++;

        print_pad_num(tid, 4);
        print_str("  ");

        print_pad_str(name, 16);

        if (state == 0) print_str("\033[32m");      // RUNNING = Verde
        else if (state == 1) print_str("\033[36m"); // READY = Ciano
        else if (state == 8) print_str("\033[31m"); // ZOMBIE = Vermelho
        else print_str("\033[37m");

        print_pad_str(state_to_str(state), 12);
        print_str("\033[0m");

        print_pad_num(prio, 4);
        print_str("  ");
        print_pad_num(parent_tid, 6);
        print_str("  ");
        print_num((uint64_t)exit_status);
        print_str("\r\n");
    }

    print_str("------------------------------------------------------------\r\n");
    print_str("Active Threads: ");
    print_num(active_count);
    print_str("  |  [q] Quit   [r] Refresh Now\r\n");
}

void _start(void) {
    devfs_tid = (tid_t)kinfo_user()->devfs_tid;
    console_h = vfs_open(devfs_tid, "console", 0);
    if (console_h < 0) ipc_exit(1);

    set_raw_mode(1);

    int timer = 0;
    render_top();

    while (1) {
        int c = read_key();
        if (c == 'q' || c == 'Q') break;
        if (c == 'r' || c == 'R') {
            render_top();
            timer = 0;
        }

        ipc_sleep(10); // Sleep nativo de 100ms
        timer++;
        if (timer >= 10) { // Atualização automática a cada 1 segundo (10x100ms)
            render_top();
            timer = 0;
        }
    }

    print_str("\033[0m\033[2J\033[H");
    set_raw_mode(0);
    vfs_close(devfs_tid, (uint64_t)console_h);
    ipc_exit(0);
}
