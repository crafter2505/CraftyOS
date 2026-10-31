#include "builtins.h"
#include "kprintf.h"
#include "terminal.h"
#include "path.h"
#include "vfs.h"

const struct shell_cmd builtins_table[] = {
    { "help",  cmd_help,  "list available commands" },
    { "echo",  cmd_echo,  "print arguments" },
    { "clear", cmd_clear, "clear the screen" },
    { "pwd",   cmd_pwd,   "print working directory" },
    { "cd",    cmd_cd,    "change directory" },
    { "ls",    cmd_ls,    "list directory contents" },
    { "cat",   cmd_cat,   "print file contents" },
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

    if (shell_resolve_path(shell_get_cwd(), target,
                           resolved, (int)sizeof(resolved)) != 0) {
        kprintf("ls: path too long\n");
        return 1;
    }

    struct inode *dir = vfs_lookup(resolved);
    if (!dir) {
        kprintf("ls: %s: no such file or directory\n", target);
        return 1;
    }

    /* If it's a file, print just the name (like `ls path/to/file`). */
    struct vfs_stat st;
    if (vfs_stat(resolved, &st) == 0 && (st.mode & 0xF000) != 0x4000) {
        /* It's a regular file — print its basename and return. */
        const char *base = resolved;
        for (const char *p = resolved; *p; p++) {
            if (*p == '/') base = p + 1;
        }
        kprintf("%s\n", base);
        return 0;
    }

    /* It's a directory. Iterate. */
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
    if (shell_resolve_path(shell_get_cwd(), argv[1],
                           resolved, (int)sizeof(resolved)) != 0) {
        kprintf("cat: path too long\n");
        return 1;
    }

    /* If it's a directory, refuse — reading directory entries as
     * bytes is never useful and dumps garbage to the terminal. */
    struct vfs_stat st;
    if (vfs_stat(resolved, &st) == 0 && (st.mode & 0xF000) == 0x4000) {
        kprintf("cat: %s: is a directory\n", argv[1]);
        return 1;
    }

    struct file *f = vfs_open(resolved, 0);   /* O_RDONLY = 0 */
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
            /* terminal_putchar handles \n and \b. Everything else
             * prints as-is, including binary. That's the caller's
             * problem — `cat /dev/random` on Linux does the same. */
            terminal_putchar(buf[i]);
        }
    }

    vfs_close(f);
    return 0;
}