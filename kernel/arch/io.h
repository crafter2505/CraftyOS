#ifndef IO_H
#define IO_H
static inline void outb(unsigned short port, unsigned char value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char result;
    __asm__ volatile ("inb %1, %0" : "=a"(result): "Nd"(port));
    return result;
}

static inline void wrmsr(unsigned int msr, unsigned long long value) {
    unsigned int lo = (unsigned int)(value & 0xFFFFFFFF);
    unsigned int hi = (unsigned int)(value >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static inline unsigned long long rdmsr(unsigned int msr) {
    unsigned int lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((unsigned long long)hi << 32) | lo;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif