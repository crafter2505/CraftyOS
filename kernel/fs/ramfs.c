#include "vfs.h"
#include "kmalloc.h"
#include "kprintf.h"


struct ramfs_node {
    const char *name;
    const char *content;
    uint64_t content_len;
    int is_dir;
    int parent_index;
};

static const char etc_version[] = "craftyOS 0.16.0\n";
static const char hello_txt[]   = "hello from craftyOS\n";

static struct ramfs_node ramfs_nodes[] = {
    /*  name          content        len                          is_dir  parent */
    {   "",           0,             0,                           1,     -1 },   /* 0: / */
    {   "etc",        0,             0,                           1,      0 },   /* 1: /etc */
    {   "version",    etc_version,   sizeof(etc_version) - 1,     0,      1 },   /* 2: /etc/version */
    {   "home",       0,             0,                           1,      0 },   /* 3: /home */
    {   "nasrullah",  0,             0,                           1,      3 },   /* 4: /home/nasrullah */
    {   "hello.txt",  hello_txt,     sizeof(hello_txt) - 1,       0,      4 },   /* 5: /home/nasrullah/hello.txt */
    {   "dev",        0,             0,                           1,      0 },   /* 6: /dev */
};

#define N_NODES (sizeof(ramfs_nodes) / sizeof(ramfs_nodes[0]))

/* Per-inode ramfs data */
struct ramfs_inode_data {
    int node_index;
};

/* Forward declarations */
static int ramfs_readdir(struct inode *dir, uint64_t index,
                          char *out_name, uint64_t name_len);
static struct inode *ramfs_lookup(struct inode *dir, const char *name);
static int ramfs_read(struct file *f, void *buf, uint64_t count);
static uint64_t ramfs_seek(struct file *f, uint64_t offset, int whence);

static struct inode_operations ramfs_i_op = {
    .lookup  = ramfs_lookup,
    .readdir = ramfs_readdir,
    .create  = 0,
    .unlink  = 0,
};

static struct file_operations ramfs_f_op = {
    .read  = ramfs_read,
    .write = 0,
    .seek  = ramfs_seek,
    .close = 0,
};

/* Create an inode for a node index */
static struct inode *ramfs_make_inode(int node_idx) {
    struct inode *i = vfs_alloc_inode();
    if (!i) return 0;

    i->ino = (uint64_t)node_idx + 1;
    i->mode = ramfs_nodes[node_idx].is_dir ? (S_IFDIR | 0755) : (S_IFREG | 0644);
    i->uid = 0;
    i->gid = 0;
    i->size = ramfs_nodes[node_idx].content_len;
    i->i_op = &ramfs_i_op;
    i->f_op = &ramfs_f_op;

    struct ramfs_inode_data *d = (struct ramfs_inode_data *)kmalloc(sizeof(*d));
    d->node_index = node_idx;
    i->fs_data = d;

    return i;
}

static int ramfs_readdir(struct inode *dir, uint64_t index,
                          char *out_name, uint64_t name_len) {
    struct ramfs_inode_data *d = (struct ramfs_inode_data *)dir->fs_data;
    if (!d) return -1;

    int node_idx = d->node_index;

    /* Count children of this node (sorted by array order) */
    uint64_t found = 0;
    for (unsigned long i = 0; i < N_NODES; i++) {
        if (ramfs_nodes[i].parent_index != node_idx) continue;
        if (found == index) {
            /* Copy the name */
            int j = 0;
            while (ramfs_nodes[i].name[j] && (uint64_t)j < name_len - 1) {
                out_name[j] = ramfs_nodes[i].name[j];
                j++;
            }
            out_name[j] = 0;
            return j;
        }
        found++;
    }
    return -1;
}

static struct inode *ramfs_lookup(struct inode *dir, const char *name) {
    struct ramfs_inode_data *d = (struct ramfs_inode_data *)dir->fs_data;
    if (!d) return 0;

    int parent_idx = d->node_index;

    for (unsigned long i = 0; i < N_NODES; i++) {
        if (ramfs_nodes[i].parent_index != parent_idx) continue;

        /* Compare names */
        int j = 0;
        while (ramfs_nodes[i].name[j] && name[j] && ramfs_nodes[i].name[j] == name[j]) j++;
        if (ramfs_nodes[i].name[j] == 0 && name[j] == 0) {
            return ramfs_make_inode((int)i);
        }
    }
    return 0;
}

static int ramfs_read(struct file *f, void *buf, uint64_t count) {
    struct ramfs_inode_data *d = (struct ramfs_inode_data *)f->inode->fs_data;
    if (!d) return -1;

    const struct ramfs_node *node = &ramfs_nodes[d->node_index];
    if (node->is_dir) return -1;

    uint64_t remaining = node->content_len - f->pos;
    uint64_t to_read = count < remaining ? count : remaining;

    for (uint64_t i = 0; i < to_read; i++) {
        ((uint8_t *)buf)[i] = (uint8_t)node->content[f->pos + i];
    }

    f->pos += to_read;
    return (int)to_read;
}

static uint64_t ramfs_seek(struct file *f, uint64_t offset, int whence) {
    struct ramfs_inode_data *d = (struct ramfs_inode_data *)f->inode->fs_data;
    if (!d) return (uint64_t)-1;

    const struct ramfs_node *node = &ramfs_nodes[d->node_index];
    uint64_t new_pos;

    if (whence == SEEK_SET) new_pos = offset;
    else if (whence == SEEK_CUR) new_pos = f->pos + offset;
    else if (whence == SEEK_END) new_pos = node->content_len + offset;
    else return (uint64_t)-1;

    if (new_pos > node->content_len) new_pos = node->content_len;
    f->pos = new_pos;
    return new_pos;
}

/* Initialize ramfs and mount it at "/" */
int ramfs_init(void) {
    struct inode *root = ramfs_make_inode(0);
    if (!root) return -1;

    if (vfs_mount("/", root) != 0) return -1;

    kprintf("  RAMFS: mounted at /\n");
    return 0;
}