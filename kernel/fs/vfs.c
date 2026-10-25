#include "vfs.h"
#include "kmalloc.h"
#include "kprintf.h"

static struct vfs_mount *root_mount = 0;

static struct vfs_mount *mounts = 0;

void vfs_init(void) {
    root_mount = 0;
    mounts = 0;
    kprintf("  VFS: initialized\n");
}

int vfs_mount(const char *path, struct inode *root) {
    if (!path || !root) return -1;
    if (!(root->mode & S_IFDIR)) return -1;

    struct vfs_mount *m = (struct vfs_mount *)kmalloc(sizeof(struct vfs_mount));
    if (!m) return -1;

    int i = 0;
    while (path[i] && i < 255) {
        m->path[i] = path[i];
        i++;
    }
    m->path[i] = 0;

    m->root = root;
    m->next = 0;

    if (path[0] == '/' && path[1] == 0) {
        root_mount = m;
    }

    if (!mounts) {
        mounts = m;
    } else {
        struct vfs_mount *last = mounts;
        while (last->next) last = last->next;
        last->next = m;
    }

    kprintf("  VFS: mounted '%s'\n", path);
    return 0;
}

struct inode *vfs_lookup(const char *path) {
    if (!path || !root_mount) return 0;

    struct inode *cur = root_mount->root;
    const char *rest = path;
    struct vfs_mount *m;
    struct vfs_mount *best = 0;
    uint64_t best_len = 0;

    for (m = mounts; m; m = m->next) {
        uint64_t len = 0;
        while (m->path[len] && path[len] && m->path[len] == path[len]) len++;
        if (m->path[len] == 0 && len > best_len &&
            (path[len] == 0 || path[len] == '/' ||
             (len > 0 && m->path[len - 1] == '/'))) {
            best = m;
            best_len = len;
        }
    }

    if (best && best_len > 0) {
        cur = best->root;
        rest = path + best_len;
    }

    /* Root path */
    if (rest[0] == 0) return cur;
    if (rest[0] == '/' && rest[1] == 0) return cur;

    /* Skip leading slash */
    if (*rest == '/') rest++;

    char name[256];
    while (*rest) {
        int i = 0;
        while (*rest && *rest != '/' && i < 255) {
            name[i++] = *rest++;
        }
        name[i] = 0;
        while (*rest == '/') rest++;

        if (name[0] == 0) break;

        if (!cur->i_op || !cur->i_op->lookup) return 0;
        cur = cur->i_op->lookup(cur, name);
        if (!cur) return 0;
    }

    return cur;
}

struct file *vfs_open(const char *path, uint32_t flags) {
    struct inode *i = vfs_lookup(path);
    if (!i) return 0;

    if ((flags & O_DIRECTORY) && !(i->mode & S_IFDIR)) return 0;
    if (!(flags & O_DIRECTORY) && (i->mode & S_IFDIR)) return 0;

    struct file *f = (struct file *)kmalloc(sizeof(struct file));
    if (!f) return 0;

    f->inode = i;
    f->pos = 0;
    f->flags = flags;
    f->refcount = 1;

    i->refcount++;

    return f;
}

int vfs_read(struct file *f, void *buf, uint64_t count) {
    if (!f || !f->inode || !buf) return -1;
    if ((f->flags & 3) == O_WRONLY) return -1;
    if (!f->inode->f_op || !f->inode->f_op->read) return -1;
    return f->inode->f_op->read(f, buf, count);
}

int vfs_close(struct file *f) {
    if (!f) return -1;
    if (f->inode) {
        if (f->inode->f_op && f->inode->f_op->close) {
            f->inode->f_op->close(f);
        }
        if (f->inode->refcount > 0) f->inode->refcount--;
    }
    kfree(f);
    return 0;
}

uint64_t vfs_seek(struct file *f, uint64_t offset, int whence) {
    if (!f || !f->inode) return (uint64_t)-1;

    if (f->inode->f_op && f->inode->f_op->seek) {
        return f->inode->f_op->seek(f, offset, whence);
    }

    uint64_t new_pos;
    if (whence == SEEK_SET)      new_pos = offset;
    else if (whence == SEEK_CUR) new_pos = f->pos + offset;
    else if (whence == SEEK_END) new_pos = f->inode->size + offset;
    else return (uint64_t)-1;

    if (new_pos > f->inode->size) new_pos = f->inode->size;
    f->pos = new_pos;
    return new_pos;
}

int vfs_readdir(struct inode *dir, uint64_t index,
                char *out_name, uint64_t name_len) {
    if (!dir || !out_name || name_len == 0) return -1;
    if (!(dir->mode & S_IFDIR)) return -1;
    if (!dir->i_op || !dir->i_op->readdir) return -1;
    return dir->i_op->readdir(dir, index, out_name, name_len);
}

struct inode *vfs_alloc_inode(void) {
    struct inode *i = (struct inode *)kmalloc(sizeof(struct inode));
    if (!i) return 0;
    uint8_t *p = (uint8_t *)i;
    for (unsigned long j = 0; j < sizeof(struct inode); j++) p[j] = 0;
    return i;
}

void vfs_free_inode(struct inode *i) {
    if (i) kfree(i);
}