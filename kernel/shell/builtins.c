#include "builtins.h"
#include "kprintf.h"
#include "terminal.h"
#include "path.h"
#include "vfs.h"
#include "timer.h"
#include "buddy.h"
#include "slab.h"
#include "sched.h"
#include "process.h"
#include "pmm.h"
#include "vmm.h"
#include "io.h"
#include "userspace.h"
#include "user.h"

const struct shell_cmd builtins_table[] = {
    { "help",  cmd_help,  "list available commands" },
    { "echo",  cmd_echo,  "print arguments" },
    { "clear", cmd_clear, "clear the screen" },
    { "pwd",   cmd_pwd,   "print working directory" },
    { "cd",    cmd_cd,    "change directory" },
    { "ls",    cmd_ls,    "list directory contents" },
    { "cat",   cmd_cat,   "print file contents" },
    { "uptime",  cmd_uptime,  "time since boot" },
    { "meminfo", cmd_meminfo, "memory summary" },
    { "ps",      cmd_ps,      "list processes" },
    { "buddy",   cmd_buddy,   "buddy allocator dump" },
    { "slab",    cmd_slab,    "slab allocator dump" },
    { "sched",   cmd_sched,   "scheduler state" },
    { "pgtbl",   cmd_pgtbl,   "walk page tables for a virtual addr" },
    { "dump",    cmd_dump,    "hex dump kernel memory" },
    { "reboot",  cmd_reboot,  "restart the machine" },
    { "forktest", cmd_forktest, "test fork syscall" },
    { "exectest", cmd_exectest, "test fork+exec" },
    { "run", cmd_run, "run an ELF file at an explicit path" },
    { "user_dump", cmd_user_dump, "list users" },
    { "authtest",  cmd_authtest,  "test user authentication" },
    { 0, 0, 0 }
};

int cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;

    kprintf("Commands:\n");
    for (int i = 0; builtins_table[i].name; i++) {
        kprintf("  %s", builtins_table[i].name);
        int n = 0;
        while (builtins_table[i].name[n]) n++;
        for (int j = n; j < 10; j++) kprintf(" ");
        kprintf("%s\n", builtins_table[i].help);
    }
    return 0;
}

int cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) kprintf(" ");
        kprintf("%s", argv[i]);
    }
    kprintf("\n");
    return 0;
}

int cmd_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    terminal_clear();
    return 0;
}

int cmd_pwd(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("%s\n", shell_get_cwd());
    return 0;
};

int cmd_cd(int argc, char **argv) {
    char resolved[512];
    const char *target = (argc >= 2) ? argv[1] : "/";

    if (shell_resolve_path(shell_get_cwd(), target,
                           resolved, (int)sizeof(resolved)) != 0) {
        kprintf("cd: path too long\n");
        return 1;
    }

    struct vfs_stat st;
    if (vfs_stat(resolved, &st) != 0) {
        kprintf("cd: %s: no such directory\n", target);
        return 1;
    }
    if ((st.mode & 0xF000) != 0x4000) {   /* S_IFDIR */
        kprintf("cd: %s: not a directory\n", target);
        return 1;
    }

    shell_set_cwd(resolved);
    return 0;
}

int cmd_ls(int argc, char **argv) {
    char resolved[512];
    const char *target = (argc >= 2) ? argv[1] : shell_get_cwd();

    if (shell_resolve_path(shell_get_cwd(), target, resolved, (int)sizeof(resolved)) != 0) {
        kprintf("ls: path too long\n");
        return 1;
    }

    struct inode *dir = vfs_lookup(resolved);
    if (!dir) {
        kprintf("ls: %s: no such file or directory\n", target);
        return 1;
    }

    struct vfs_stat st;
    if (vfs_stat(resolved, &st) == 0 && (st.mode & 0xF000) != 0x4000) {
        const char *base = resolved;
        for (const char *p = resolved; *p; p++) {
            if (*p == '/') base = p + 1;
        }
        kprintf("%s\n", base);
        return 0;
    }

    char namebuf[256];
    for (uint64_t i = 0; ; i++) {
        int r = vfs_readdir(dir, i, namebuf, sizeof(namebuf));
        if (r < 0) break;
        if (r == 0) break;
        kprintf("%s\n", namebuf);
    }
    return 0;
}

int cmd_cat(int argc, char **argv) {
    if (argc < 2) {
        kprintf("cat: missing filename\n");
        return 1;
    }

    char resolved[512];
    if (shell_resolve_path(shell_get_cwd(), argv[1], resolved, (int)sizeof(resolved)) != 0) {
        kprintf("cat: path too long\n");
        return 1;
    }

    struct vfs_stat st;
    if (vfs_stat(resolved, &st) == 0 && (st.mode & 0xF000) == 0x4000) {
        kprintf("cat: %s: is a directory\n", argv[1]);
        return 1;
    }

    struct file *f = vfs_open(resolved, 0);
    if (!f) {
        kprintf("cat: %s: no such file\n", argv[1]);
        return 1;
    }

    char buf[256];
    for (;;) {
        int n = vfs_read(f, buf, sizeof(buf));
        if (n < 0) {
            kprintf("cat: read error\n");
            vfs_close(f);
            return 1;
        }
        if (n == 0) break;
        for (int i = 0; i < n; i++) {
            terminal_putchar(buf[i]);
        }
    }

    vfs_close(f);
    return 0;
}

int cmd_uptime(int argc, char **argv) {
    (void)argc; (void)argv;
    uint64_t ticks = timer_get_ticks();
    uint64_t secs = ticks / 100;
    uint64_t frac = (ticks % 100) * 100 / 100;
    kprintf("up %ds (%d ticks)\n", secs, ticks);
    return 0;
};

int cmd_meminfo(int argc, char **argv) {
    (void)argc; (void)argv;

    uint64_t total = pmm_get_total_pages();
    uint64_t free  = pmm_get_free_pages();
    uint64_t used  = total - free;

    kprintf("=== memory ===\n");
    kprintf("pmm:   %d total pages (%d MB), %d free, %d used\n",
            total, total * 4 / 1024, free, used);

    kprintf("\n-- buddy --\n");
    buddy_dump_stats();

    kprintf("\n-- slab --\n");
    slab_dump_stats();

    return 0;
}

int cmd_ps(int argc, char **argv) {
    (void)argc; (void)argv;

    struct process *cur = process_current();
    if (!cur) {
        kprintf("ps: no current process\n");
        return 1;
    }
    kprintf("  PID  NAME\n");
    kprintf("  %d    %s\n", (uint64_t)cur->pid, cur->name);
    return 0;
}

int cmd_buddy(int argc, char **argv) {
    (void)argc; (void)argv;
    buddy_dump_stats();
    return 0;
}

int cmd_slab(int argc, char **argv) {
    (void)argc; (void)argv;
    slab_dump_stats();
    return 0;
}

int cmd_sched(int argc, char **argv) {
    (void)argc; (void)argv;
    sched_dump();
    return 0;
}

int cmd_pgtbl(int argc, char **argv) {
    if (argc < 2) {
        kprintf("usage: pgtbl <hex-addr>\n");
        return 1;
    }

    uint64_t virt = 0;
    const char *s = argv[1];
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while (*s) {
        uint64_t digit;
        if (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'f') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') digit = *s - 'A' + 10;
        else { kprintf("pgtbl: invalid hex '%s'\n", argv[1]); return 1; }
        virt = (virt << 4) | digit;
        s++;
    }

    uint64_t phys = vmm_translate(virt);
    if (phys == 0) {
        kprintf("pgtbl: 0x%x not mapped\n", virt);
        return 1;
    }
    kprintf("pgtbl: 0x%x -> phys 0x%x\n", virt, phys);
    return 0;
}


static void print_hex_padded(uint64_t value, int digits) {
    const char *hex = "0123456789abcdef";
    for (int i = digits - 1; i >= 0; i--) {
        kprintf("%c", hex[(value >> (i * 4)) & 0xF]);
    }
}

int cmd_dump(int argc, char **argv) {
    if (argc < 2) {
        kprintf("usage: dump <hex-addr> [bytes]\n");
        return 1;
    }

    uint64_t addr = 0;
    const char *s = argv[1];
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    if (!*s) {
        kprintf("dump: empty address\n");
        return 1;
    }
    while (*s) {
        uint64_t digit;
        if (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'f') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') digit = *s - 'A' + 10;
        else { kprintf("dump: invalid hex '%s'\n", argv[1]); return 1; }
        addr = (addr << 4) | digit;
        s++;
    }

    uint64_t count = 64;
    if (argc >= 3) {
        count = 0;
        const char *c = argv[2];
        while (*c >= '0' && *c <= '9') { count = count * 10 + (*c - '0'); c++; }
        if (count == 0) count = 64;
        if (count > 1024) count = 1024;
    }

    const uint8_t *p = (const uint8_t *)addr;
    for (uint64_t off = 0; off < count; off += 16) {
        print_hex_padded(addr + off, 8);
        kprintf("  ");
        for (uint64_t i = 0; i < 16; i++) {
            if (off + i < count) {
                print_hex_padded(p[off + i], 2);
                kprintf(" ");
            } else {
                kprintf("   ");
            }
            if (i == 7) kprintf(" ");
        }
        kprintf(" |");
        for (uint64_t i = 0; i < 16 && off + i < count; i++) {
            uint8_t ch = p[off + i];
            kprintf("%c", (ch >= 32 && ch < 127) ? (char)ch : '.');
        }
        kprintf("|\n");
    }
    return 0;
}


int cmd_reboot(int argc, char **argv) {
    (void)argc; (void)argv;

    kprintf("rebooting...\n");

    for (int i = 0; i < 100000 && (inb(0x64) & 0x01); i++) {
        (void)inb(0x60);
    }

    __asm__ volatile ("cli");

    outb(0x64, 0xFE);

    for (volatile int i = 0; i < 1000000; i++);


    struct __attribute__((packed)) {
        uint16_t limit;
        uint64_t base;
    } null_idt = { 0, 0 };
    __asm__ volatile ("lidt %0" : : "m"(null_idt));
    __asm__ volatile ("int $0x03");

    for (;;) __asm__ volatile ("hlt");
    return 0;
}

extern uint8_t _user_fork_program_start[];
extern uint8_t _user_fork_program_end[];

int cmd_forktest(int argc, char **argv) {
    (void)argc; (void)argv;
    int rc = userspace_run_program(_user_fork_program_start, _user_fork_program_end - _user_fork_program_start);
    kprintf("forktest: program exited %d\n", (uint64_t)rc);
    return (rc == 0) ? 0 : 1;
}

extern uint8_t _user_fork_exec_start[];
extern uint8_t _user_fork_exec_end[];

int cmd_exectest(int argc, char **argv) {
    (void)argc; (void)argv;
    int rc = userspace_run_program(_user_fork_exec_start, _user_fork_exec_end - _user_fork_exec_start);
    kprintf("exectest: program exited %d\n", (uint64_t)rc);
    return (rc == 0) ? 0 : 1;
}

int cmd_run(int argc, char **argv) {
    if (argc < 2) {
        kprintf("usage: run <path> [args...]\n");
        return 1;
    }

    static char path_copy[256];
    int i = 0;
    while (argv[1][i] && i < (int)sizeof(path_copy) - 1) {
        path_copy[i] = argv[1][i];
        i++;
    }
    path_copy[i] = 0;

    g_exec_args.path = path_copy;
    g_exec_args.argv[0] = path_copy;
    for (int k = 2; k < argc && k - 1 < 31; k++) {
        g_exec_args.argv[k - 1] = argv[k];
    }
    g_exec_args.argv[(argc - 1 < 31) ? argc - 1 : 31] = 0;

    struct process *child = process_create("run", shell_exec_entry, &g_exec_args);
    if (!child) {
        kprintf("run: process_create failed\n");
        return 1;
    }

    uint32_t status = 0;
    if (process_wait(child->pid, &status) != 0) {
        kprintf("run: wait failed\n");
        return 1;
    }

    if (status != 0) {
        kprintf("run: exited with status %d\n", (uint64_t)status);
        return 1;
    }
    return 0;
}

int cmd_user_dump(int argc, char **argv) {
    (void)argc; (void)argv;
    user_dump();
    return 0;
}

int cmd_authtest(int argc, char **argv) {
    if (argc < 3) {
        kprintf("usage: authtest <name> <password>\n");
        return 1;
    }
    int uid = user_authenticate(argv[1], argv[2]);
    if (uid < 0) {
        kprintf("authtest: authentication failed\n");
        return 1;
    }
    kprintf("authtest: %s authenticated (uid=%d)\n",
            argv[1], (uint64_t)uid);
    return 0;
}