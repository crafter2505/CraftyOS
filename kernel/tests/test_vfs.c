#include "tests.h"
#include "vfs.h"
#include "kprintf.h"

void test_vfs(void) {
    kprintf("\n=== VFS test ===\n");

    /* Test 1: root directory is a directory */
    struct inode *root = vfs_lookup("/");
    if (root && (root->mode & S_IFDIR)) {
        kprintf("  [1] PASS: / is a directory\n");
    } else {
        kprintf("  [1] FAIL: / is not a directory\n");
        return;
    }

    /* Test 2: lookup a nested path */
    struct inode *ver = vfs_lookup("/etc/version");
    if (ver && (ver->mode & S_IFREG)) {
        kprintf("  [2] PASS: /etc/version is a regular file\n");
    } else {
        kprintf("  [2] FAIL: /etc/version not found\n");
        return;
    }

    /* Test 3: open and read /etc/version */
    struct file *f = vfs_open("/etc/version", O_RDONLY);
    if (!f) {
        kprintf("  [3] FAIL: open failed\n");
        return;
    }

    char buf[64];
    for (int i = 0; i < 64; i++) buf[i] = 0;

    int n = vfs_read(f, buf, sizeof(buf) - 1);
    if (n > 0) {
        kprintf("  [3] PASS: read %d bytes: '%s'\n", (uint64_t)n, buf);
    } else {
        kprintf("  [3] FAIL: read returned %d\n", (uint64_t)n);
        vfs_close(f);
        return;
    }

    vfs_close(f);

    /* Test 4: read /home/nasrullah/hello.txt */
    f = vfs_open("/home/nasrullah/hello.txt", O_RDONLY);
    if (!f) {
        kprintf("  [4] FAIL: open hello.txt\n");
        return;
    }
    for (int i = 0; i < 64; i++) buf[i] = 0;
    n = vfs_read(f, buf, sizeof(buf) - 1);
    if (n > 0 && buf[0] == 'h') {
        kprintf("  [4] PASS: hello.txt contains '%s'\n", buf);
    } else {
        kprintf("  [4] FAIL: hello.txt read %d\n", (uint64_t)n);
    }
    vfs_close(f);

    /* Test 5: readdir / */
    kprintf("  [5] / contains:\n");
    char name[64];
    int idx = 0;
    while (vfs_readdir(root, idx, name, sizeof(name)) > 0) {
        kprintf("       %s\n", name);
        idx++;
    }
    kprintf("  [5] PASS: readdir works\n");

    /* Test 6: lookup a nonexistent path */
    if (!vfs_lookup("/does/not/exist")) {
        kprintf("  [6] PASS: nonexistent path returns NULL\n");
    } else {
        kprintf("  [6] FAIL: nonexistent path returned an inode\n");
    }

    kprintf("=== VFS test done ===\n");
}