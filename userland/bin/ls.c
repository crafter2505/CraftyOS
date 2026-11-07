#include "userlib.h"

#define TEST_PATH "/mnt"

void _start(void) {
    char name[256];
    for (int i = 0; readdir(TEST_PATH, i, name, sizeof(name)) >= 0; i++) {
        write(1, name, strlen(name));
        write(1, "\n", 1);
    }
    _exit(0);
}