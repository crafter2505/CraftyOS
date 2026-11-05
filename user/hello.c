typedef unsigned long  uint64_t;
typedef unsigned char  uint8_t;

static inline long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile ("syscall"
                      : "=a"(ret)
                      : "a"(n), "D"(a), "S"(b), "d"(c)
                      : "rcx", "r11", "memory");
    return ret;
}

static char zero_check[4096];

static void write_str(const char *s) {
    unsigned long n = 0;
    while (s[n]) n++;
    syscall3(1, 1, (long)s, (long)n);
}

void _start(void) {
    for (int i = 0; i < 4096; i++) {
        if (zero_check[i] != 0) {
            syscall3(0, 99, 0, 0);
            for (;;);
        }
    }

    write_str("hello from exec\n");
    syscall3(0, 0, 0, 0);
    for (;;);
}