#ifndef VFS_H
#define VFS_H

#include "types.h"

/* File types */
#define S_IFMT   0170000
#define S_IFREG  0100000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFBLK  0060000
#define S_IFIFO  0010000
#define S_IFLNK  0120000
#define S_IFSOCK 0140000

/* Permissions */
#define S_IRWXU  00700
#define S_IRUSR  00400
#define S_IWUSR  00200
#define S_IXUSR  00100
#define S_IRWXG  00070
#define S_IRGRP  00040
#define S_IWGRP  00020
#define S_IXGRP  00010
#define S_IRWXO  00007
#define S_IROTH  00004
#define S_IWOTH  00002
#define S_IXOTH  00001

/* Open flags */
#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR   0x0002
#define O_CREAT  0x0040
#define O_TRUNC  0x0200
#define O_APPEND 0x0400
#define O_DIRECTORY 0x10000

/* Lseek */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* -----------------------------------------------------------------
 * Forward declarations — required for circular references
 * ----------------------------------------------------------------- */
struct inode;
struct file;
struct vfs_mount;
struct inode_operations;
struct file_operations;

/* -----------------------------------------------------------------
 * File operations — references struct file (forward-declared)
 * ----------------------------------------------------------------- */
struct file_operations {
    int (*read)(struct file *f, void *buf, uint64_t count);
    int (*write)(struct file *f, const void *buf, uint64_t count);
    uint64_t (*seek)(struct file *f, uint64_t offset, int whence);
    int (*close)(struct file *f);
};

/* -----------------------------------------------------------------
 * Inode operations — references struct inode (forward-declared)
 * ----------------------------------------------------------------- */
struct inode_operations {
    struct inode *(*lookup)(struct inode *dir, const char *name);
    int (*readdir)(struct inode *dir, uint64_t index,
                   char *out_name, uint64_t name_len);
    int (*create)(struct inode *dir, const char *name, uint32_t mode);
    int (*unlink)(struct inode *dir, const char *name);
};

/* -----------------------------------------------------------------
 * struct file — an open file handle
 * ----------------------------------------------------------------- */
struct file {
    struct inode *inode;
    uint64_t pos;
    uint32_t flags;
    uint32_t refcount;
};

/* -----------------------------------------------------------------
 * struct vfs_mount — a mounted filesystem
 * ----------------------------------------------------------------- */
struct vfs_mount {
    struct inode *root;
    char path[256];
    struct vfs_mount *next;
};

/* -----------------------------------------------------------------
 * struct inode — the main VFS object
 * ----------------------------------------------------------------- */
struct inode {
    uint64_t ino;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t _pad;
    uint64_t size;
    uint64_t nlink;
    uint64_t mtime;
    uint64_t atime;
    uint64_t ctime;

    struct inode_operations *i_op;
    struct file_operations  *f_op;

    void *fs_data;
    struct vfs_mount *mount;

    uint32_t refcount;
    uint32_t _pad2;
};

/* -----------------------------------------------------------------
 * VFS API
 * ----------------------------------------------------------------- */
void vfs_init(void);
int vfs_mount(const char *path, struct inode *root);
struct inode *vfs_lookup(const char *path);
struct file *vfs_open(const char *path, uint32_t flags);
int vfs_read(struct file *f, void *buf, uint64_t count);
int vfs_close(struct file *f);
uint64_t vfs_seek(struct file *f, uint64_t offset, int whence);
int vfs_readdir(struct inode *dir, uint64_t index,
                char *out_name, uint64_t name_len);
struct inode *vfs_alloc_inode(void);
void vfs_free_inode(struct inode *i);

#endif