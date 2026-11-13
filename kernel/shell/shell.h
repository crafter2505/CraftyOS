#ifndef SHELL_H
#define SHELL_H

#define ARGV_MAX 32

struct shell_cmd {
    const char *name;
    int (*fn)(int argc, char **argv);
    const char *help;
};

struct exec_args {
    const char *path;
    const char *argv[32];
};

extern struct exec_args g_exec_args;

void shell_exec_entry(void *arg);

int shell_start(void);

const char *shell_get_cwd(void);
void shell_set_cwd(const char *path);

int shell_dispatch(int argc, char **argv);

#endif