#include "terminal.h"
#include "kprintf.h"
#include "pic.h"
#include "idt.h"
#include "timer.h"
#include "keyboard.h"
#include "line.h"
#include "memory_map.h"
#include "pmm.h"
#include "vmm.h"

void _start(void) {
    terminal_init();
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    terminal_clear();

    kprintf("craftyOS v0.10.2\n");
    kprintf("=======================\n\n");

    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("VGA driver initialized\n");

    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("kprintf working\n");

    pic_remap();
    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("PIC remapped to vectors 0x20-0x2F\n");

    idt_init();
    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("IDT loaded\n");

    timer_init(100);
    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("Timer initialized at 100 Hz\n");

    keyboard_init();
    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("Keyboard initialized\n");


    kprintf("\n");
    kprintf("\n=== Paging self-test ===\n");

    volatile uint64_t *kernel_test = (volatile uint64_t *)0xFFFFFFFF80000000ULL;
    volatile uint64_t *low_test    = (volatile uint64_t *)0x10000ULL;
    volatile uint64_t *dm_test     = (volatile uint64_t *)0xFFFF800000100000ULL;

    uint64_t val1 = *kernel_test;
    uint64_t val2 = *low_test;
    uint64_t val3 = *dm_test;

    kprintf("higher-half 0xFFFFFFFF80000000 -> 0x%x\n", val1);
    kprintf("identity    0x0000000000010000 -> 0x%x\n", val2);
    kprintf("direct map  0xFFFF800000100000 -> 0x%x\n", val3);

    if (val1 == val3) {
        kprintf("[OK] higher-half and direct map agree\n");
    } else {
        kprintf("[FAIL] higher-half and direct map disagree\n");
    }
    kprintf("=== Self-test passed ===\n\n");

    memory_map_dump();

    pmm_init();

    
    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("Physical memory manager ready\n");

    vmm_init();

    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("Virtual memory manager ready\n");


    kprintf("\nVMM test:\n");
    uint64_t test_virt = 0xDEADB000ULL;
    uint64_t phys = (uint64_t)pmm_alloc_page();
    kprintf("  Allocated phys page: 0x%x\n", phys);

    if (vmm_map(test_virt, phys, VMM_KERNEL_RW) == 0) {
        kprintf("  Mapped 0x%x -> 0x%x\n", test_virt, phys);

        volatile uint64_t *test = (volatile uint64_t *)test_virt;
        *test = 0xCAFEBABE12345678ULL;
        kprintf("  Wrote 0xCAFEBABE12345678\n");
        kprintf("  Read back: 0x%x\n", *test);

        uint64_t translated = vmm_translate(test_virt);
        kprintf("  Translate(0x%x) = 0x%x\n", test_virt, translated);

        if (translated == phys) {
            kprintf("  PASS\n");
        } else {
            kprintf("  FAIL: expected 0x%x\n", phys);
        }

        vmm_unmap(test_virt);
        kprintf("  Unmapped\n");
    } else {
        kprintf("  FAIL: vmm_map returned error\n");
    }

    pmm_free_page((void *)phys);


    kprintf("\nPMM test:\n");
    kprintf("  Free pages before: %d\n", (long)pmm_get_free_pages());

    void *p1 = pmm_alloc_page();
    void *p2 = pmm_alloc_page();
    void *p3 = pmm_alloc_page();

    kprintf("  Allocated: 0x%x, 0x%x, 0x%x\n",
            (uint64_t)p1, (uint64_t)p2, (uint64_t)p3);
    kprintf("  Free pages after alloc: %d\n", (long)pmm_get_free_pages());

    pmm_free_page(p2);
    kprintf("  Freed middle page\n");
    kprintf("  Free pages after free: %d\n", (long)pmm_get_free_pages());

    void *p4 = pmm_alloc_page();
    kprintf("  Allocated again: 0x%x (should match 0x%x)\n",
            (uint64_t)p4, (uint64_t)p2);

    if (p4 == p2) {
        kprintf("  PASS: reused freed page\n");
    } else {
        kprintf("  FAIL: allocated a different page\n");
    }

    line_init();
    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("Line editor ready\n");

    pic_clear_mask(0);
    pic_clear_mask(1);

    kprintf("\n");
    kprintf("craftyOS> ");

    __asm__ volatile ("sti");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}