#ifndef FD_H
#define FD_H

#include "types.h"

#define MAX_FDS 16
#define FD_STDIN  0
#define FD_STDOUT 1
#define FD_STDERR 2

struct file;

struct fd_table {
    struct file *fds[MAX_FDS];
};


void fd_table_init(struct fd_table *t);

int fd_alloc(struct fd_table *t, struct file *f);

struct file *fd_get(struct fd_table *t, int fd);

int fd_close(struct fd_table *t, int fd);

void fd_close_all(struct fd_table *t);

#endif