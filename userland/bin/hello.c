#include "userlib.h"

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *msg = "hello from userland\n";
    write(1, msg, strlen(msg));
    return 0;
}