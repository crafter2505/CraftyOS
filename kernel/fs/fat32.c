#include "fat32.h"
#include "vfs.h"
#include "kmalloc.h"
#include "kprintf.h"
#include "ahci.h"
#include "buddy.h"
#include "address_space.h"

struct fat32_bpb {
    uint8_t  jmp[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_sig;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
} __attribute__((packed));

struct fat32_dir_entry {
    uint8_t  name[8];
    uint8_t  ext[3];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t access_date;
    uint16_t high_cluster;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t low_cluster;
    uint32_t file_size;
} __attribute__((packed));

struct fat32_lfn_entry {
    uint8_t  seq;
    uint16_t name1[5];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  checksum;
    uint16_t name2[6];
    uint16_t reserved2;
    uint16_t name3[2];
} __attribute__((packed));

#define FAT32_ATTR_LONG_NAME  0x0F
#define FAT32_ATTR_DIRECTORY  0x10
#define FAT32_ATTR_ARCHIVE    0x20
#define FAT32_ATTR_VOLUME_ID  0x08

struct fat32_fs {
    int      port;
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t reserved_sectors;
    uint32_t num_fats;
    uint32_t fat_size_sectors;
    uint32_t root_cluster;
    uint32_t data_start_sector;
    uint32_t cluster_size;

    uint32_t *fat;
    uint32_t fat_entries;
    uint32_t total_clusters;
};

static char lfn_char_to_ascii(uint16_t c) {
    if (c < 0x80) return (char)c;
    return '?';
}

static int lfn_append_chars(struct fat32_lfn_entry *e, char *out,
                             uint32_t out_len, uint32_t base) {
    uint16_t chars[13];
    for (int i = 0; i < 5; i++) chars[i]    = e->name1[i];
    for (int i = 0; i < 6; i++) chars[5+i]  = e->name2[i];
    for (int i = 0; i < 2; i++) chars[11+i] = e->name3[i];

    int wrote = 0;
    for (int i = 0; i < 13; i++) {
        uint32_t pos = base + i;
        if (pos >= out_len - 1) break;
        uint16_t c = chars[i];
        if (c == 0x0000 || c == 0xFFFF) {
            out[pos] = 0;
            return wrote;
        }
        out[pos] = lfn_char_to_ascii(c);
        wrote++;
    }
    return wrote;
}

static struct fat32_fs g_fs;

static int read_sectors(uint64_t lba, uint32_t count, void *buf) {
    if (!buf) return -1;
    for (uint32_t i = 0; i < count; i++) {
        if (ahci_read_sector(g_fs.port, lba + i,
                             (uint8_t *)buf + i * 512) != 0) {
            return -1;
        }
    }
    return 0;
}

static int read_cluster(uint32_t cluster, void *buf) {
    uint64_t lba = g_fs.data_start_sector + (uint64_t)(cluster - 2) * g_fs.sectors_per_cluster;
    return read_sectors(lba, g_fs.sectors_per_cluster, buf);
}

static uint32_t fat_next_cluster(uint32_t cluster) {
    if (cluster < 2) return 0xFFFFFFFF;
    if (cluster >= g_fs.fat_entries) return 0xFFFFFFFF;

    uint32_t val = g_fs.fat[cluster] & 0x0FFFFFFF;
    if (val == 0x0FFFFFF7) return 0xFFFFFFFF;
    if (val >= 0x0FFFFFF8) return 0xFFFFFFFF;
    if (val < 2) return 0xFFFFFFFF;
    if (val >= g_fs.fat_entries) return 0xFFFFFFFF;

    return val;
}

static void *alloc_cluster_buf(void) {
    uint32_t sz = g_fs.cluster_size;
    if (sz <= 2048) return kmalloc(sz);
    int order = 0;
    uint32_t pages = (sz + 4095) / 4096;
    while (((uint32_t)1 << order) < pages) order++;
    void *phys = alloc_pages(order);
    if (!phys) return 0;
    return phys_to_virt((uint64_t)phys);
}

static void free_cluster_buf(void *buf) {
    if (!buf) return;
    uint32_t sz = g_fs.cluster_size;
    if (sz <= 2048) { kfree(buf); }
    else {
        uint64_t phys = virt_to_phys(buf);
        int order = 0;
        uint32_t pages = (sz + 4095) / 4096;
        while (((uint32_t)1 << order) < pages) order++;
        free_pages((void *)phys, order);
    }
}

static void format_short_name(const struct fat32_dir_entry *e,
                              char *out, uint32_t out_len) {
    int j = 0;
    for (int i = 0; i < 8 && j < (int)out_len - 1; i++) {
        if (e->name[i] == ' ') break;
        char c = e->name[i];
        if (c >= 'A' && c <= 'Z') c += 32;
        out[j++] = c;
    }
    if (e->ext[0] != ' ') {
        if (j < (int)out_len - 1) out[j++] = '.';
        for (int i = 0; i < 3 && j < (int)out_len - 1; i++) {
            if (e->ext[i] == ' ') break;
            char c = e->ext[i];
            if (c >= 'A' && c <= 'Z') c += 32;
            out[j++] = c;
        }
    }
    out[j] = 0;
}


struct cached_dir_entry {
    char     name[256];
    uint32_t cluster;
    uint32_t size;
    uint8_t  attr;
};

struct fat32_inode_data {
    uint32_t first_cluster;
    uint32_t size;
    int is_dir;
    int mounted;

    struct cached_dir_entry *cache;
    uint32_t cache_count;
    int      cache_valid;
};

static void fat32_free_inode_data(void *data) {
    struct fat32_inode_data *d = (struct fat32_inode_data *)data;
    if (!d) return;
    if (d->cache) {
        kfree(d->cache);
        d->cache = 0;
    }
    kfree(d);
}

static int fat32_build_dir_cache(struct inode *dir) {
    struct fat32_inode_data *d = (struct fat32_inode_data *)dir->fs_data;
    if (!d || !d->is_dir || d->cache_valid) return 0;

    if (d->cache) {
        kfree(d->cache);
        d->cache = 0;
        d->cache_count = 0;
    }

    void *cluster_buf = alloc_cluster_buf();
    if (!cluster_buf) return -1;

    uint32_t count = 0;
    uint32_t cluster = d->first_cluster;
    int iterations = 0;
    const int MAX_ITER = 65536;

    char     lfn_buf[256];
    uint32_t lfn_len = 0;
    int      lfn_active = 0;

    while (cluster != 0 && cluster != 0xFFFFFFFF) {
        if (++iterations > MAX_ITER) break;
        if (read_cluster(cluster, cluster_buf) != 0) break;
        uint32_t n = g_fs.cluster_size / 32;
        struct fat32_dir_entry *entries = (struct fat32_dir_entry *)cluster_buf;
        for (uint32_t i = 0; i < n; i++) {
            uint8_t first = entries[i].name[0];
            if (first == 0) { cluster = 0xFFFFFFFF; break; }
            if (first == 0xE5) { lfn_active = 0; lfn_len = 0; continue; }
            if (entries[i].attr == FAT32_ATTR_LONG_NAME) {
                struct fat32_lfn_entry *le = (struct fat32_lfn_entry *)&entries[i];
                uint8_t seq = le->seq & 0x3F;
                if (seq == 0 || seq > 20) continue;
                uint32_t base = (uint32_t)(seq - 1) * 13;
                int wrote = lfn_append_chars(le, lfn_buf, sizeof(lfn_buf), base);
                if (wrote > 0) {
                    lfn_active = 1;
                    if (base + wrote > lfn_len) lfn_len = base + wrote;
                }
                continue;
            }
            if (entries[i].attr & FAT32_ATTR_VOLUME_ID) { lfn_active = 0; lfn_len = 0; continue; }
            count++;
            lfn_active = 0; lfn_len = 0;
        }
        if (cluster != 0xFFFFFFFF) cluster = fat_next_cluster(cluster);
    }

    if (count == 0) {
        free_cluster_buf(cluster_buf);
        d->cache_valid = 1;
        return 0;
    }

    d->cache = (struct cached_dir_entry *)kmalloc(
        count * sizeof(struct cached_dir_entry));
    if (!d->cache) { free_cluster_buf(cluster_buf); return -1; }

    cluster = d->first_cluster;
    iterations = 0;
    lfn_active = 0;
    lfn_len = 0;
    lfn_buf[0] = 0;
    uint32_t idx = 0;

    while (cluster != 0 && cluster != 0xFFFFFFFF && idx < count) {
        if (++iterations > MAX_ITER) break;
        if (read_cluster(cluster, cluster_buf) != 0) break;
        uint32_t n = g_fs.cluster_size / 32;
        struct fat32_dir_entry *entries = (struct fat32_dir_entry *)cluster_buf;
        for (uint32_t i = 0; i < n && idx < count; i++) {
            uint8_t first = entries[i].name[0];
            if (first == 0) { cluster = 0xFFFFFFFF; break; }
            if (first == 0xE5) { lfn_active = 0; lfn_len = 0; continue; }

            if (entries[i].attr == FAT32_ATTR_LONG_NAME) {
                struct fat32_lfn_entry *le = (struct fat32_lfn_entry *)&entries[i];
                uint8_t seq = le->seq & 0x3F;
                if (seq == 0 || seq > 20) continue;
                uint32_t base = (uint32_t)(seq - 1) * 13;
                int wrote = lfn_append_chars(le, lfn_buf, sizeof(lfn_buf), base);
                if (wrote > 0) {
                    lfn_active = 1;
                    if (base + wrote > lfn_len) lfn_len = base + wrote;
                }
                continue;
            }

            if (entries[i].attr & FAT32_ATTR_VOLUME_ID) {
                lfn_active = 0; lfn_len = 0; continue;
            }

            char short_name[16];
            format_short_name(&entries[i], short_name, sizeof(short_name));

            struct cached_dir_entry *ce = &d->cache[idx];
            if (lfn_active && lfn_len > 0) {
                lfn_buf[lfn_len] = 0;
                int k = 0;
                while (lfn_buf[k] && k < 255) { ce->name[k] = lfn_buf[k]; k++; }
                ce->name[k] = 0;
            } else {
                int k = 0;
                while (short_name[k] && k < 255) { ce->name[k] = short_name[k]; k++; }
                ce->name[k] = 0;
            }

            ce->cluster = ((uint32_t)entries[i].high_cluster << 16)
                        | entries[i].low_cluster;
            ce->size = entries[i].file_size;
            ce->attr = entries[i].attr;

            idx++;
            lfn_active = 0; lfn_len = 0; lfn_buf[0] = 0;
        }
        if (cluster != 0xFFFFFFFF) cluster = fat_next_cluster(cluster);
    }

    free_cluster_buf(cluster_buf);
    d->cache_count = idx;
    d->cache_valid = 1;
    return 0;
}


static struct inode *fat32_lookup(struct inode *dir, const char *name) {
    struct fat32_inode_data *d = (struct fat32_inode_data *)dir->fs_data;
    if (!d || !d->is_dir) return 0;

    if (fat32_build_dir_cache(dir) != 0) return 0;

    char needle[256];
    int n = 0;
    while (name[n] && n < 255) {
        char c = name[n];
        if (c >= 'A' && c <= 'Z') c += 32;
        needle[n] = c;
        n++;
    }
    needle[n] = 0;

    for (uint32_t i = 0; i < d->cache_count; i++) {
        struct cached_dir_entry *ce = &d->cache[i];

        char lower[256];
        int k = 0;
        while (ce->name[k] && k < 255) {
            char c = ce->name[k];
            if (c >= 'A' && c <= 'Z') c += 32;
            lower[k] = c;
            k++;
        }
        lower[k] = 0;

        int j = 0;
        while (lower[j] && needle[j] && lower[j] == needle[j]) j++;
        if (lower[j] == 0 && needle[j] == 0) {
            struct inode *ino = vfs_alloc_inode();
            if (!ino) return 0;
            struct fat32_inode_data *nd =
                (struct fat32_inode_data *)kmalloc(sizeof(*nd));
            if (!nd) { vfs_free_inode(ino); return 0; }

            nd->first_cluster = ce->cluster;
            nd->size = ce->size;
            nd->is_dir = (ce->attr & FAT32_ATTR_DIRECTORY) ? 1 : 0;
            nd->mounted = 0;
            nd->cache = 0;
            nd->cache_count = 0;
            nd->cache_valid = 0;

            ino->ino = ce->cluster;
            ino->mode = nd->is_dir ? (S_IFDIR | 0755) : (S_IFREG | 0644);
            ino->uid = 0;
            ino->gid = 0;
            ino->size = ce->size;
            ino->fs_data = nd;
            ino->i_op = dir->i_op;
            ino->f_op = dir->f_op;
            ino->mount = dir->mount;

            return ino;
        }
    }
    return 0;
}

static int fat32_readdir(struct inode *dir, uint64_t index,
                          char *out_name, uint64_t name_len) {
    struct fat32_inode_data *d = (struct fat32_inode_data *)dir->fs_data;
    if (!d || !d->is_dir) return -1;

    if (fat32_build_dir_cache(dir) != 0) return -1;
    if (index >= d->cache_count) return -1;

    struct cached_dir_entry *ce = &d->cache[index];
    int k = 0;
    while (ce->name[k] && (uint64_t)k < name_len - 1) {
        out_name[k] = ce->name[k];
        k++;
    }
    out_name[k] = 0;
    return k;
}


static int fat32_read(struct file *f, void *buf, uint64_t count) {
    struct fat32_inode_data *d = (struct fat32_inode_data *)f->inode->fs_data;
    if (!d || d->is_dir) return -1;

    if (f->pos >= d->size) return 0;

    uint64_t remaining = d->size - f->pos;
    uint64_t to_read = count < remaining ? count : remaining;

    void *cluster_buf = alloc_cluster_buf();
    if (!cluster_buf) return -1;

    uint32_t cluster = d->first_cluster;
    uint64_t skip = f->pos / g_fs.cluster_size;
    for (uint64_t i = 0; i < skip; i++) {
        cluster = fat_next_cluster(cluster);
        if (cluster == 0xFFFFFFFF) { free_cluster_buf(cluster_buf); return -1; }
    }

    uint64_t pos_in_cluster = f->pos % g_fs.cluster_size;
    uint64_t bytes_read = 0;

    while (bytes_read < to_read) {
        if (read_cluster(cluster, cluster_buf) != 0) break;

        uint64_t avail = g_fs.cluster_size - pos_in_cluster;
        uint64_t chunk = to_read - bytes_read;
        if (chunk > avail) chunk = avail;

        for (uint64_t i = 0; i < chunk; i++) {
            ((uint8_t *)buf)[bytes_read + i] = ((uint8_t *)cluster_buf)[pos_in_cluster + i];
        }

        bytes_read += chunk;
        f->pos += chunk;

        if (bytes_read < to_read) {
            cluster = fat_next_cluster(cluster);
            if (cluster == 0xFFFFFFFF) break;
            pos_in_cluster = 0;
        }
    }

    free_cluster_buf(cluster_buf);
    return (int)bytes_read;
}

static uint64_t fat32_seek(struct file *f, uint64_t offset, int whence) {
    if (!f || !f->inode) return (uint64_t)-1;
    uint64_t new_pos;
    if (whence == SEEK_SET)      new_pos = offset;
    else if (whence == SEEK_CUR) new_pos = f->pos + offset;
    else if (whence == SEEK_END) new_pos = f->inode->size + offset;
    else return (uint64_t)-1;

    if (new_pos > f->inode->size) new_pos = f->inode->size;
    f->pos = new_pos;
    return new_pos;
}

static struct inode_operations fat32_i_op = {
    .lookup  = fat32_lookup,
    .readdir = fat32_readdir,
    .create  = 0,
    .unlink  = 0,
};

static struct file_operations fat32_f_op = {
    .read  = fat32_read,
    .write = 0,
    .seek  = fat32_seek,
    .close = 0,
};

int fat32_mount(const char *mount_path) {
    kprintf("  FAT32: mounting at '%s'\n", mount_path);

    g_fs.port = 0;

    uint8_t boot[512];
    if (ahci_read_sector(g_fs.port, 0, boot) != 0) {
        kprintf("  FAT32: failed to read boot sector\n");
        return -1;
    }

    struct fat32_bpb *bpb = (struct fat32_bpb *)boot;

    if (bpb->bytes_per_sector != 512) {
        kprintf("  FAT32: unsupported sector size %d\n", (uint64_t)bpb->bytes_per_sector);
        return -1;
    }

    if (bpb->sectors_per_cluster == 0 || bpb->sectors_per_cluster > 128) {
        kprintf("  FAT32: invalid sectors_per_cluster %d\n",
                (uint64_t)bpb->sectors_per_cluster);
        return -1;
    }

    if (bpb->num_fats == 0 || bpb->num_fats > 2) {
        kprintf("  FAT32: invalid num_fats %d\n", (uint64_t)bpb->num_fats);
        return -1;
    }

    if (bpb->fat_size_32 == 0) {
        kprintf("  FAT32: invalid fat_size_32 0\n");
        return -1;
    }

    if (bpb->root_cluster < 2) {
        kprintf("  FAT32: invalid root_cluster %d\n",
                (uint64_t)bpb->root_cluster);
        return -1;
    }

    g_fs.bytes_per_sector = bpb->bytes_per_sector;
    g_fs.sectors_per_cluster = bpb->sectors_per_cluster;
    g_fs.reserved_sectors = bpb->reserved_sectors;
    g_fs.num_fats = bpb->num_fats;
    g_fs.fat_size_sectors = bpb->fat_size_32;
    g_fs.root_cluster = bpb->root_cluster;
    g_fs.cluster_size = g_fs.bytes_per_sector * g_fs.sectors_per_cluster;

    kprintf("  FAT32: bps=%d spc=%d reserved=%d fats=%d fat_size=%d root_cluster=%d\n",
            (uint64_t)g_fs.bytes_per_sector,
            (uint64_t)g_fs.sectors_per_cluster,
            (uint64_t)g_fs.reserved_sectors,
            (uint64_t)g_fs.num_fats,
            (uint64_t)g_fs.fat_size_sectors,
            (uint64_t)g_fs.root_cluster);

    g_fs.data_start_sector = g_fs.reserved_sectors + g_fs.num_fats * g_fs.fat_size_sectors;

    uint64_t fat_bytes = (uint64_t)g_fs.fat_size_sectors * g_fs.bytes_per_sector;
    g_fs.fat = (uint32_t *)kmalloc(fat_bytes);
    if (!g_fs.fat) {
        kprintf("  FAT32: failed to allocate FAT table\n");
        return -1;
    }

    uint64_t fat_lba = g_fs.reserved_sectors;
    if (read_sectors(fat_lba, g_fs.fat_size_sectors, g_fs.fat) != 0) {
        kprintf("  FAT32: failed to read FAT\n");
        kfree(g_fs.fat);
        g_fs.fat = 0;
        return -1;
    }

    g_fs.fat_entries = fat_bytes / 4;
    g_fs.total_clusters = g_fs.fat_entries;

    kprintf("  FAT32: FAT loaded, %d entries\n", (uint64_t)g_fs.fat_entries);

    struct inode *root = vfs_alloc_inode();
    if (!root) { kfree(g_fs.fat); return -1; }

    struct fat32_inode_data *root_d =
        (struct fat32_inode_data *)kmalloc(sizeof(*root_d));
    if (!root_d) { vfs_free_inode(root); kfree(g_fs.fat); return -1; }

    root_d->first_cluster = g_fs.root_cluster;
    root_d->size = 0;
    root_d->is_dir = 1;
    root_d->mounted = 1;
    root_d->cache = 0;
    root_d->cache_count = 0;
    root_d->cache_valid = 0;

    root->ino = g_fs.root_cluster;
    root->mode = S_IFDIR | 0755;
    root->uid = 0;
    root->gid = 0;
    root->size = 0;
    root->fs_data = root_d;
    root->i_op = &fat32_i_op;
    root->f_op = &fat32_f_op;

    if (vfs_mount(mount_path, root) != 0) {
        kprintf("  FAT32: vfs_mount failed\n");
        return -1;
    }

    vfs_mount_set_free_hook(mount_path, fat32_free_inode_data);

    kprintf("  FAT32: mounted successfully\n");
    return 0;
}

void fat32_dump_info(void) {
    kprintf("  FAT32: bps=%d spc=%d cluster_size=%d\n",
            (uint64_t)g_fs.bytes_per_sector,
            (uint64_t)g_fs.sectors_per_cluster,
            (uint64_t)g_fs.cluster_size);
    kprintf("  FAT32: root_cluster=%d data_start=%d\n",
            (uint64_t)g_fs.root_cluster,
            (uint64_t)g_fs.data_start_sector);
}