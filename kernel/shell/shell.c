#include "shell.h"
#include "builtins.h"
#include "line.h"
#include "thread.h"
#include "sched.h"
#include "kprintf.h"
#include "terminal.h"
#include "vfs.h"
#include "process.h"

struct exec_args g_exec_args;


static char cwd[256] = "/";

static void shell_thread(void *arg);
static int  tokenize(char *line, char **argv, int max_args);
static void print_prompt(void);

int shell_start(void) {
    struct thread *t = thread_create("shell", shell_thread, 0);
    if (!t) {
        kprintf("shell: failed to create shell thread\n");
        return -1;
    }
    sched_add(t);
    return 0;
}

const char *shell_get_cwd(void) {
    return cwd;
}

void shell_set_cwd(const char *path) {
    if (!path) return;
    int i = 0;
    while (path[i] && i < (int)sizeof(cwd) - 1) {
        cwd[i] = path[i];
        i++;
    }
    cwd[i] = 0;
}

static void print_prompt(void) {
    const char *cwd = shell_get_cwd();
    const char *shown = cwd;
    int len = 0;
    while (cwd[len]) len++;
    if (len > 24) {
        int last_slash = 0;
        for (int i = 0; i < len; i++) {
            if(cwd[i] == '/') last_slash = i;
        }
        if (last_slash > 0 && last_slash < len -1) {
            shown = cwd + last_slash + 1;
        }
    }

    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("CraftyOS:%s# ", shown);
    terminal_set_color(VGA_WHITE, VGA_BLACK);
}

static void shell_thread(void *arg) {
    (void)arg;

    static char linebuf[LINE_MAX];
    static char *argv[ARGV_MAX];

    for (;;) {
        print_prompt();

        int n = line_readline(linebuf, LINE_MAX);
        if (n < 0) {
            thread_exit();
        }

        int argc = tokenize(linebuf, argv, ARGV_MAX);
        if (argc == 0) continue;

        int rc = shell_dispatch(argc, argv);
        if (rc == -2) {
            kprintf("%s: command not found\n", argv[0]);
        }
    }
}

static int is_space(char c) {
    return c == ' ' || c == '\t';
}

static int tokenize(char *line, char **argv, int max_args) {
    if (!line || !argv || max_args <= 0) return -1;

    int argc = 0;
    char *p = line;

    while (*p && argc < max_args) {
        while (*p && is_space(*p)) p++;
        if (!*p) break;

        argv[argc++] = p;

        while (*p && !is_space(*p)) p++;
        if (*p) {
            *p = 0;
            p++;
        }
    }

    return argc;
}

void shell_exec_entry(void *arg) {
    struct exec_args *ea = (struct exec_args *)arg;
    process_exec(ea->path, ea->argv);
}

static int shell_run_external(int argc, char **argv) {
    char path[256];
    const char *prefix = "/mnt/efi/boot/";
    const char *name = argv[0];
    int p = 0;
    while (prefix[p] && p < 255) { path[p] = prefix[p]; p++; }
    int n = 0;
    while (name[n] && p < 255) { path[p++] = name[n++]; }
    path[p] = 0;

    struct vfs_stat st;
    if (vfs_stat(path, &st) != 0) return -2;

    static char path_copy[256];
    for (int i = 0; i <= p; i++) path_copy[i] = path[i];
    g_exec_args.path = path_copy;

    for (int i = 0; i < argc && i < 31; i++) g_exec_args.argv[i] = argv[i];
    g_exec_args.argv[argc < 31 ? argc : 31] = 0;

    struct process *child = process_create("ext", shell_exec_entry, &g_exec_args);
    if (!child) return -1;

    uint32_t status = 0;
    if (process_wait(child->pid, &status) != 0) return -1;
    return (status == 0) ? 0 : -1;
}

int shell_dispatch(int argc, char **argv) {
    if (argc == 0 || !argv || !argv[0]) return -1;
    for (int i = 0; builtins_table[i].name; i++) {
        const char *a = argv[0];
        const char *b = builtins_table[i].name;
        int j = 0;
        while (a[j] && b[j] && a[j] == b[j]) j++;
        if (a[j] == 0 && b[j] == 0) {
            int rc = builtins_table[i].fn(argc, argv);
            return (rc == 0) ? 0 : -1;
        }
    }

    return shell_run_external(argc, argv);
}