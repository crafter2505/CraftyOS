#include "fd.h"
#include "vfs.h"
#include "kprintf.h"

void fd_table_init(struct fd_table *t) {
    if (!t) return;
    for (int i = 0; i < MAX_FDS; i++) {
        t->fds[i] = 0;
    }
}

int fd_alloc(struct fd_table *t, struct file *f) {
    if (!t || !f) return -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (t->fds[i] == 0) {
            t->fds[i] = f;
            return i;
        }
    }
    return -1;
}

struct file *fd_get(struct fd_table *t, int fd) {
    if (!t || fd < 0 || fd >= MAX_FDS) return 0;
    return t->fds[fd];
}

int fd_close(struct fd_table *t, int fd) {
    if (!t || fd < 0 || fd >= MAX_FDS) return -1;
    if (t->fds[fd] == 0) return -1;
    vfs_close(t->fds[fd]);
    t->fds[fd] = 0;
    return 0;
}

void fd_close_all(struct fd_table *t) {
    if (!t) return;
    for (int i = 0; i < MAX_FDS; i++) {
        if (t->fds[i]) {
            vfs_close(t->fds[i]);
            t->fds[i] = 0;
        }
    }
}