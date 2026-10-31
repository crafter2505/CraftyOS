#include "shell.h"
#include "builtins.h"
#include "line.h"
#include "thread.h"
#include "sched.h"
#include "kprintf.h"
#include "terminal.h"

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
    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("craftyOS# ");
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

    return -2;
}