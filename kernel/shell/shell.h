/* shell.h - kernel shell (boot shell, debug interface)
 *
 * This shell runs in ring 0 as a scheduler thread. It is the
 * interface for debugging, diagnostics, and system inspection.
 *
 * In Milestone 19, this shell becomes gated behind admin auth (the
 * boot shell / recovery shell). The normal user interface moves to
 * a ring 3 shell (userland/sh), which is a separate program.
 *
 * Prompt: 'craftyOS# ' (root-style, distinguishes from user shell).
 *
 * For Milestone 18, this is the only shell — no gating needed
 * because there's no privilege boundary yet. Milestone 19 adds the
 * login screen and gates this shell behind admin credentials.
 */

#ifndef SHELL_H
#define SHELL_H

#define ARGV_MAX 32

struct shell_cmd {
    const char *name;
    int (*fn)(int argc, char **argv);
    const char *help;
};

int shell_start(void);

const char *shell_get_cwd(void);
void shell_set_cwd(const char *path);

int shell_dispatch(int argc, char **argv);

#endif