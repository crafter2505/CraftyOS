#include "console.h"
#include "line.h"
#include "terminal.h"
#include "vfs.h"
#include "kmalloc.h"

static int console_read(struct file *f, void *buf, uint64_t count) {
    (void)f;
    if (!buf || count == 0) return -1;
    int n = line_readline((char *)buf, (int)count);
    if (n < 0) return -1;
    return n;
}

static int console_write(struct file *f, const void *buf, uint64_t count) {
    (void)f;
    if (!buf) return -1;
    const char *s = (const char *)buf;
    for (uint64_t i = 0; i < count; i++) {
        terminal_putchar(s[i]);
    }
    return (int)count;
}

static uint64_t console_seek(struct file *f, uint64_t offset, int whence) {
    (void)f;
    (void)offset;
    (void)whence;
    return 0;
}

static int console_close(struct file *f) {
    (void)f;
    return 0;
}

struct file_operations console_file_ops = {
    .read  = console_read,
    .write = console_write,
    .seek  = console_seek,
    .close = console_close,
};

static struct inode console_inode;
static int console_inode_ready = 0;

struct file *console_file_create(void) {
    struct file *f = (struct file *)kmalloc(sizeof(struct file));
    if (!f) return 0;

    if (!console_inode_ready) {
        console_inode.f_op = &console_file_ops;
        console_inode_ready = 1;
    }

    f->inode = &console_inode;
    f->pos = 0;
    f->flags = 0;
    f->refcount = 1;
    return f;
}