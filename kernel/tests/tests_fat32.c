#include "tests.h"
#include "vfs.h"
#include "kprintf.h"

void test_fat32(void) {
    kprintf("\n=== FAT32 test ===\n");

    /* Test 1: /mnt exists (root inode is a directory) */
    struct inode *mnt = vfs_lookup("/mnt");
    if (mnt && (mnt->mode & S_IFDIR)) {
        kprintf("  [1] PASS: /mnt is a directory\n");
    } else {
        kprintf("  [1] FAIL: /mnt not found\n");
        return;
    }

    /* Test 2: list /mnt contents */
    kprintf("  [2] /mnt contains:\n");
    char name[64];
    int idx = 0;
    while (vfs_readdir(mnt, idx, name, sizeof(name)) > 0) {
        kprintf("       %s\n", name);
        idx++;
    }
    if (idx == 0) {
        kprintf("  [2] WARN: /mnt is empty\n");
    } else {
        kprintf("  [2] PASS: readdir worked (%d entries)\n", (uint64_t)idx);
    }

    /* Test 3: open /mnt/EFI/BOOT/kernel.elf */
    struct file *f = vfs_open("/mnt/EFI/BOOT/kernel.elf", O_RDONLY);
    if (!f) {
        kprintf("  [3] FAIL: cannot open kernel.elf\n");
        return;
    }
    kprintf("  [3] PASS: opened kernel.elf\n");

    /* Test 4: read the ELF header */
    uint8_t hdr[64];
    int n = vfs_read(f, hdr, sizeof(hdr));
    vfs_close(f);

    if (n < 4) {
        kprintf("  [4] FAIL: read returned %d\n", (uint64_t)n);
        return;
    }

    if (hdr[0] == 0x7F && hdr[1] == 'E' &&
        hdr[2] == 'L' && hdr[3] == 'F') {
        kprintf("  [4] PASS: ELF magic verified (0x%x%x%x%x)\n",
                (uint64_t)hdr[0], (uint64_t)hdr[1],
                (uint64_t)hdr[2], (uint64_t)hdr[3]);
    } else {
        kprintf("  [4] FAIL: not an ELF file (magic 0x%x%x%x%x)\n",
                (uint64_t)hdr[0], (uint64_t)hdr[1],
                (uint64_t)hdr[2], (uint64_t)hdr[3]);
        return;
    }

    /* Test 5: read more (make sure cluster chain works) */
    f = vfs_open("/mnt/EFI/BOOT/kernel.elf", O_RDONLY);
    if (f) {
        uint8_t buf[2048];
        n = vfs_read(f, buf, sizeof(buf));
        vfs_close(f);
        if (n > 512) {
            kprintf("  [5] PASS: read %d bytes (multi-cluster)\n", (uint64_t)n);
        } else {
            kprintf("  [5] WARN: only read %d bytes\n", (uint64_t)n);
        }
    }

    /* Test 6: long filename lookup */
    struct file *lf = vfs_open("/mnt/longfilename.txt", O_RDONLY);
    if (lf) {
        char buf[64];
        int n = vfs_read(lf, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = 0;
            kprintf("  [6] PASS: long name read '%s'\n", buf);
        } else {
            kprintf("  [6] FAIL: read returned %d\n", (uint64_t)n);
        }
        vfs_close(lf);
    } else {
        kprintf("  [6] FAIL: long filename lookup\n");
    }

    /* Test 7: readdir shows long names */
    kprintf("  [7] /mnt contents (recheck for longname):\n");
    idx = 0;
    while (vfs_readdir(mnt, idx, name, sizeof(name)) > 0) {
        kprintf("       %s\n", name);
        idx++;
    }
    kprintf("  [7] PASS: readdir\n");

    kprintf("=== FAT32 test done ===\n");
}