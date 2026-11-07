#include "userlib.h"

#define TEST_PATH "/mnt/longfilename.txt"

void _start(void) {
    int fd = open(TEST_PATH, 0);
    if (fd < 0) {
        const char *err = "cat: open failed\n";
        write(1, err, strlen(err));
        _exit(1);
    }

    char buf[256];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(1, buf, (size_t)n);
    }
    close(fd);
    _exit(0);
}