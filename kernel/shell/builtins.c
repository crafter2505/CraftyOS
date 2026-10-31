#include "builtins.h"
#include "kprintf.h"
#include "terminal.h"

const struct shell_cmd builtins_table[] = {
    { "help",  cmd_help,  "list available commands" },
    { "echo",  cmd_echo,  "print arguments" },
    { "clear", cmd_clear, "clear the screen" },
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