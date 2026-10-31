#ifndef BUILTINS_H
#define BUILTINS_H

#include "shell.h"

extern const struct shell_cmd builtins_table[];

int cmd_help (int argc, char **argv);
int cmd_echo (int argc, char **argv);
int cmd_clear(int argc, char **argv);

#endif