#include "userlib.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        const char *u = "usage: cat <path>\n";
        write(1, u, strlen(u));
        return 1;
    }
    int fd = open(argv[1], 0);
    if (fd < 0) {
        const char *e = "cat: open failed\n";
        write(2, e, strlen(e));
        return 1;
    }
    char buf[256];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(1, buf, (size_t)n);
    }
    close(fd);
    return 0;
}