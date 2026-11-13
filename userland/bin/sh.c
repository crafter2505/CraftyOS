#include "userlib.h"

#define LINE_MAX 256
#define ARGV_MAX 32

static int is_space(char c) {
    return c == ' ' || c == '\t';
}

static int tokenize(char *line, char **argv, int max) {
    int argc = 0;
    char *p = line;
    while (*p && argc < max) {
        while (*p && is_space(*p)) p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && !is_space(*p)) p++;
        if (*p) { *p = 0; p++; }
    }
    return argc;
}

static void print_prompt(void) {
    write(1, "CraftyOS> ", 10);
}

static void build_path(const char *name, char *out, int out_len) {
    const char *prefix = "/mnt/efi/boot/";
    int p = 0;
    while (prefix[p] && p < out_len - 1) { out[p] = prefix[p]; p++; }
    int n = 0;
    while (name[n] && p < out_len - 1) { out[p++] = name[n++]; }
    out[p] = 0;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    static char line[LINE_MAX];
    static char *tokens[ARGV_MAX];

    for (;;) {
        print_prompt();

        int n = read(0, line, LINE_MAX - 1);
        if (n < 0) {
            const char *e = "read failed\n";
            write(2, e, strlen(e));
            continue;
        }
        if (n == 0) continue;
        line[n] = 0;

        int ntok = tokenize(line, tokens, ARGV_MAX);
        if (ntok == 0) continue;

        if (strcmp(tokens[0], "exit") == 0) {
            _exit(0);
        }
        int pid = fork();
        if (pid == 0) {
            char path[256];
            build_path(tokens[0], path, sizeof(path));
            exec(path, (const char *const *)tokens);
            write(2, "exec failed\n", 12);
            _exit(127);
        } else if (pid > 0) {
            wait(pid);
        } else {
            const char *e = "fork failed\n";
            write(2, e, strlen(e));
        }
    }
}