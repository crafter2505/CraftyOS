#ifndef BUILTINS_H
#define BUILTINS_H

#include "shell.h"

extern const struct shell_cmd builtins_table[];

int cmd_help (int argc, char **argv);
int cmd_echo (int argc, char **argv);
int cmd_clear(int argc, char **argv);
int cmd_pwd  (int argc, char **argv);
int cmd_cd   (int argc, char **argv);
int cmd_ls   (int argc, char **argv);
int cmd_cat  (int argc, char **argv);
int cmd_uptime  (int argc, char **argv);
int cmd_meminfo (int argc, char **argv);
int cmd_ps      (int argc, char **argv);
int cmd_buddy   (int argc, char **argv);
int cmd_slab    (int argc, char **argv);
int cmd_sched   (int argc, char **argv);
int cmd_pgtbl   (int argc, char **argv);
int cmd_dump    (int argc, char **argv);
int cmd_reboot  (int argc, char **argv);
int cmd_forktest(int argc, char **argv);
int cmd_exectest(int argc, char **argv);

#endif