#include "userlib.h"

int main(int argc, char **argv) {
    const char *path = (argc >= 2) ? argv[1] : "/mnt";
    char name[256];
    for (int i = 0; readdir(path, i, name, sizeof(name)) >= 0; i++) {
        write(1, name, strlen(name));
        write(1, "\n", 1);
    }
    return 0;
}