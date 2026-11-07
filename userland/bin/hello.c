#include "userlib.h"

void _start(void) {
    const char *msg = "hello from userland\n";
    write(1, msg, strlen(msg));
    _exit(0);
}