#ifndef FAT32_H
#define FAT32_H

#include "types.h"
#include "vfs.h"

int fat32_mount(const char *mount_path);

void fat32_info(void);

#endif