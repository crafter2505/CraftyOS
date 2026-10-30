#include "terminal.h"
#include "kprintf.h"
#include "pic.h"
#include "idt.h"
#include "timer.h"
#include "keyboard.h"
#include "line.h"
#include "ahci.h"
#include "memory_map.h"
#include "serial.h"

#include "boot_info.h"

#include "vfs.h"
#include "ramfs.h"
#include "fat32.h"

#include "fb.h"

#include "pmm.h"
#include "buddy.h"
#include "kmalloc.h"
#include "vmm.h"

#include "thread.h"
#include "process.h"
#include "sched.h"
#include "syscall.h"
#include "percpu.h"

#include "tests/tests.h"

static void print_ok(const char *msg) {
    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("%s\n", msg);
}

void _start(struct boot_info *boot_info) {
    __asm__ volatile ("cli" : : : "memory");
    percpu_gdt_init();
    serial_init();
    if (boot_info && memory_map_set_boot_info(boot_info) != 0) {
        serial_puts("Invalid UEFI boot information\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }

    int fb_ok = 0;
    if (boot_info) {
        fb_ok = fb_init(boot_info);
        if (fb_ok) {
            serial_puts("FB: initialized\n");
            fb_clear(0x00102030);
            fb_draw_char(100, 100, 'c', 0x00FFFFFF, 0x00102030);
            fb_draw_char(108, 100, 'r', 0x00FFFFFF, 0x00102030);
            fb_draw_char(116, 100, 'a', 0x00FFFFFF, 0x00102030);
            fb_draw_char(124, 100, 'f', 0x00FFFFFF, 0x00102030);
            fb_draw_char(132, 100, 't', 0x00FFFFFF, 0x00102030);
            fb_draw_char(140, 100, 'y', 0x00FFFFFF, 0x00102030);
            fb_draw_char(148, 100, 'O', 0x00FFFFFF, 0x00102030);
            fb_draw_char(156, 100, 'S', 0x00FFFFFF, 0x00102030);
            for (volatile int spin = 0; spin < 50000000; spin++);
        } else {
            serial_puts("FB: unavailable, using VGA/serial\n");
        }
    }

    terminal_init();
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    terminal_clear();

    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("craftyOS v0.17.0");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    if (boot_info) {
        kprintf(" (UEFI boot)\n");
        kprintf("UEFI memmap: %d entries at phys 0x%x\n",
                (uint64_t)boot_info->memmap_count, boot_info->memmap_phys);
    }else {
        kprintf(" (BIOS boot)\n");
    }
    kprintf("=======================\n\n");
    terminal_set_color(VGA_WHITE, VGA_BLACK);

    print_ok("VGA driver initialized");
    print_ok("kprintf working");

    pic_remap();
    print_ok("PIC remapped to vectors 0x20-0x2F");

    idt_init();
    print_ok("IDT loaded");

    timer_init(100);
    print_ok("Timer initialized at 100 Hz");

    keyboard_init();
    print_ok("Keyboard initialized");

    memory_map_dump();

    pmm_init();
    print_ok("Physical memory manager ready");

    buddy_init();
    kmalloc_init();

    vmm_init();
    print_ok("Virtual memory manager ready");

    if (ahci_init() == 0) {
        print_ok("PCI/AHCI storage initialized");
    } else {
        kprintf("PCI/AHCI storage unavailable\n");
    }

    thread_init_subsystem();
    process_init_subsystem();
    sched_init();
    print_ok("Scheduler initialized");

    percpu_init();

    syscall_init();
    print_ok("Syscall mechanism initialized");

    line_init();
    print_ok("Line editor ready");

    extern int ramfs_init(void);
    vfs_init();
    if (ramfs_init() == 0) {
        print_ok("VFS initialized, ramfs mounted at /");
    }

    if (ahci_controller_exists()) {
        if (fat32_mount("/mnt") != 0) {
            kprintf("  [WARN] FAT32 mount failed\n");
        } else {
            print_ok("FAT32 mounted at /mnt");
        }
    }

    pic_clear_mask(0);
    pic_clear_mask(1);

    __asm__ volatile ("sti");

    kprintf("\n");
    terminal_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf("---Running Self-Test---");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    run_all_tests();

    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("\ncraftyOS> ");

    for (;;) {
        (void)line_get();
        __asm__ volatile ("hlt");
    }
}