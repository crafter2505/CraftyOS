#ifndef CONSOLE_H
#define CONSOLE_H

#include "vfs.h"

extern struct file_operations console_file_ops;

struct file *console_file_create(void);

#endif