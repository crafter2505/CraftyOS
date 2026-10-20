#include "percpu.h"
#include "kprintf.h"
#include "io.h"

static struct percpu boot_cpu;

#define IA32_GS_BASE 0xC0000101
#define IA32_KERNEL_GS_BASE 0xC0000102

struct tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

struct gdtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static uint64_t kernel_gdt[8] __attribute__((aligned(16))) = {
    0,
    0x00CF9A000000FFFFULL,
    0x00AF9A000000FFFFULL,
    0x00CF92000000FFFFULL,
    0x00CFF2000000FFFFULL,
    0x00AFFA000000FFFFULL,
    0,
    0
};
static struct gdtr kernel_gdtr;
static struct tss64 boot_tss;
static uint8_t ring3_kernel_stack[16384] __attribute__((aligned(16)));

void percpu_gdt_init(void) {
    kernel_gdtr.limit = sizeof(kernel_gdt) - 1;
    kernel_gdtr.base = (uint64_t)kernel_gdt;
    __asm__ volatile (
        "lgdt %0\n\t"
        "mov $0x18, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "pushq $0x10\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        :
        : "m"(kernel_gdtr)
        : "rax", "memory");
}

struct percpu *percpu_get(void) {
    return &boot_cpu;
}

void percpu_set_kernel_stack(uint64_t stack) {
    boot_cpu.kernel_stack = stack;
}

void percpu_init(void) {
    uint8_t *p = (uint8_t *)&boot_cpu;
    for (unsigned long i = 0; i < sizeof(boot_cpu); i++) p[i] = 0;
    p = (uint8_t *)&boot_tss;
    for (unsigned long i = 0; i < sizeof(boot_tss); i++) p[i] = 0;

    struct gdtr gdtr;
    __asm__ volatile ("sgdt %0" : "=m"(gdtr));
    if (gdtr.limit < 63) {
        kprintf("  PERCPU: GDT has no TSS descriptor slot\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }

    boot_tss.rsp0 = (uint64_t)ring3_kernel_stack +
                    sizeof(ring3_kernel_stack);
    boot_tss.iomap_base = sizeof(boot_tss);

    uint64_t tss_base = (uint64_t)&boot_tss;
    uint64_t tss_descriptor =
        ((uint64_t)(sizeof(boot_tss) - 1) & 0xFFFF) |
        ((tss_base & 0xFFFFFF) << 16) |
        (0x89ULL << 40) |
        (((tss_base >> 24) & 0xFF) << 56);
    volatile uint64_t *gdt_tss = (volatile uint64_t *)(gdtr.base + 48);
    gdt_tss[0] = tss_descriptor;
    gdt_tss[1] = tss_base >> 32;

    uint16_t tss_selector = 0x30;
    __asm__ volatile ("ltr %0" : : "r"(tss_selector) : "memory");

    boot_cpu.kernel_stack = 0x90000;
    uint64_t gs_base = (uint64_t)&boot_cpu;
    wrmsr(IA32_GS_BASE, gs_base);
    wrmsr(IA32_KERNEL_GS_BASE, 0);

    kprintf("  PERCPU: boot CPU struct at 0x%x\n", gs_base);
}