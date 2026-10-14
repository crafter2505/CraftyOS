#include "percpu.h"
#include "kprintf.h"
#include "io.h"

static struct percpu boot_cpu;

#define IA32_GS_BASE 0xC0000101
struct percpu *percpu_get(void) {
    return &boot_cpu;
}

void percpu_init(void) {
    uint8_t *p = (uint8_t *)&boot_cpu;
    for (unsigned long i = 0; i < sizeof(boot_cpu); i++) p[i] = 0;
    uint64_t gs_base = (uint64_t)&boot_cpu;
    uint32_t lo = (uint32_t)(gs_base & 0xFFFFFFFF);
    uint32_t hi = (uint32_t)(gs_base >> 32);
    __asm__ volatile ("wrmsr" : : "c"(IA32_GS_BASE), "a"(lo), "d"(hi));

    kprintf("  PERCPU: boot CPU struct at 0x%x\n", gs_base);
}