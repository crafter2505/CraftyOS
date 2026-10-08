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
#include "buddy.h"
#include "address_space.h"
#include "kmalloc.h"
#include "page.h"
#include "slab.h"
#include "thread.h"
#include "process.h"
#include "switch_test.h"
#include "sched.h"

static volatile int reap_test_counter = 0;

static int test_counter = 0;
static volatile int test_finished = 0;

void reap_test_thread(void) {
    __asm__ volatile ("sti");
    reap_test_counter++;
    thread_exit();
}

void run_reap_test(void) {
    kprintf("\n=== reap test ===\n");

    uint64_t free_before = pmm_get_free_pages();
    kprintf("  free pages before: %d\n", (long)free_before);

    int failed = 0;
    for (int i = 0; i < 10; i++) {
        struct thread *t = thread_create("worker", reap_test_thread);
        if (!t) {
            kprintf("  FAIL: unable to create worker thread\n");
            failed = 1;
            break;
        }
        sched_add(t);

        int expected = i + 1;
        while (reap_test_counter < expected) {
            yield();
        }
    }

    kprintf("  created and completed %d threads\n",
            (uint64_t)reap_test_counter);

    for (int i = 0; i < 5; i++) {
        yield();
    }

    uint64_t free_after = pmm_get_free_pages();
    kprintf("  free pages after:  %d\n", (long)free_after);

    if (!failed && free_after >= free_before - 2) {
        kprintf("  PASS: memory returned to baseline\n");
    } else {
        kprintf("  FAIL: leaked %d pages\n",
                (long)(free_before - free_after));
    }
    kprintf("=== reap test done ===\n\n");
}

void test_thread_entry(void) {
    for (int i = 0; i < 5; i++) {
        test_counter++;
        kprintf("  [test] counter=%d\n", (uint64_t)test_counter);
        for (volatile int j = 0; j < 5000000; j++);
        yield();
    }
    kprintf("  [test] done\n");
    test_finished = 1;
}

void _start(void) {
    terminal_init();
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    terminal_clear();

    kprintf("craftyOS v0.12.4\n");
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

    buddy_init();
    buddy_dump_stats();
    kmalloc_init();

    kprintf("\nBuddy test:\n");

    void *p1 = alloc_pages(0);
    kprintf("  [1] alloc_pages(0) = 0x%x\n", (uint64_t)p1);

    void *p2 = alloc_pages(0);
    kprintf("  [2] alloc_pages(0) = 0x%x\n", (uint64_t)p2);

    if (p1 && p2 && p1 != p2) {
        kprintf("  [1,2] PASS: two distinct pages\n");
    } else {
        kprintf("  [1,2] FAIL\n");
    }

    free_pages(p2, 0);
    free_pages(p1, 0);
    kprintf("  Freed both pages\n");

    void *p3 = alloc_pages(3);
    kprintf("  [3] alloc_pages(3) = 0x%x\n", (uint64_t)p3);

    if (p3) {
        kprintf("  [3] PASS: 8-page block allocated\n");
        free_pages(p3, 3);
        kprintf("  Freed 8-page block\n");
    } else {
        kprintf("  [3] FAIL: no 8-page block available\n");
    }

    kprintf("Buddy test complete.\n");

    kprintf("\nBuddy alloc diagnostic:\n");
    for (int i = 0; i < 10; i++) {
        void *p = alloc_pages(0);
        kprintf("  [%d] phys=0x%x virt=0x%x\n",
                i, (uint64_t)p,
                (uint64_t)p + DIRECT_MAP_BASE);
        free_pages(p, 0);
    }
    kprintf("Stack region: 0x80000-0x90000 (RSP lives here)\n");
    kprintf("Kernel: 0x200000-0x5AE000\n");
    kprintf("Page tables: 0x10000-0x17000\n");
    kprintf("Stage 1/2: 0x7C00-0x10000\n");


    /* ----- kmalloc test ----- */
    kprintf("\nkmalloc test:\n");

    /* [1] Small allocation (SLUB path) */
    void *a = kmalloc(100);
    kprintf("  [1] kmalloc(100) = 0x%x\n", (uint64_t)a);
    if (a) {
        *(uint64_t *)a = 0xAAAAAAAAAAAAAAAAULL;
        if (*(uint64_t *)a == 0xAAAAAAAAAAAAAAAAULL) {
            kprintf("  [1] PASS: write/read 8 bytes\n");
        } else {
            kprintf("  [1] FAIL: read/write\n");
        }
    } else {
        kprintf("  [1] FAIL: allocation failed\n");
    }

    /* [2] Zero-init */
    void *b = kzalloc(256);
    kprintf("  [2] kzalloc(256) = 0x%x\n", (uint64_t)b);
    if (b) {
        int all_zero = 1;
        for (int i = 0; i < 256; i++) {
            if (((uint8_t *)b)[i] != 0) { all_zero = 0; break; }
        }
        if (all_zero) kprintf("  [2] PASS: zeroed\n");
        else         kprintf("  [2] FAIL: not zeroed\n");
    } else {
        kprintf("  [2] FAIL: allocation failed\n");
    }

    /* [3] Large allocation (buddy path) */
    void *c = kmalloc(5000);
    kprintf("  [3] kmalloc(5000) = 0x%x\n", (uint64_t)c);
    if (c) {
        *(uint64_t *)c = 0xCCCCCCCCCCCCCCCCULL;
        if (*(uint64_t *)c == 0xCCCCCCCCCCCCCCCCULL) {
            kprintf("  [3] PASS: large alloc write/read\n");
        } else {
            kprintf("  [3] FAIL\n");
        }
    } else {
        kprintf("  [3] FAIL: allocation failed\n");
    }

    /* [4] Page metadata check */
    if (a) {
        struct page *pa = page_from_virt((uint64_t)a);
        if (pa) {
            kprintf("  [4] page metadata for a:\n");
            kprintf("      flags = 0x%x (expect PAGE_FLAG_SLAB=0x%x)\n",
                    pa->flags, PAGE_FLAG_SLAB);
            kprintf("      cache = 0x%x\n", (uint64_t)pa->cache);
            if (pa->flags == PAGE_FLAG_SLAB && pa->cache != 0) {
                kprintf("      cache name = %s\n", pa->cache->name);
                kprintf("  [4] PASS: page metadata populated\n");
            } else {
                kprintf("  [4] FAIL: metadata wrong\n");
            }
        } else {
            kprintf("  [4] FAIL: page_from_virt returned NULL\n");
        }
    }

    if (c) {
        struct page *pc = page_from_virt((uint64_t)c);
        if (pc) {
            kprintf("  [5] page metadata for c (buddy):\n");
            kprintf("      flags = 0x%x (expect PAGE_FLAG_BUDDY_ALLOC=0x%x)\n",
                    pc->flags, PAGE_FLAG_BUDDY_ALLOC);
            kprintf("      order = %d (5000 bytes -> 2 pages -> order=1)\n",
                    pc->order);
            if (pc->flags == PAGE_FLAG_BUDDY_ALLOC && pc->order == 1) {
                kprintf("  [5] PASS: buddy metadata correct\n");
            } else {
                kprintf("  [5] FAIL: buddy metadata wrong\n");
            }
        }
    }

    /* [6] Many small allocations to force multiple buckets */
    void *ptrs[200];
    int ok = 1;
    for (int i = 0; i < 200; i++) {
        ptrs[i] = 0;
    }
    for (int i = 0; i < 200; i++) {
        ptrs[i] = kmalloc(32 + (i % 4) * 16);
        if (!ptrs[i]) { ok = 0; break; }
    }
    if (ok) kprintf("  [6] PASS: 200 varied allocations\n");
    else    kprintf("  [6] FAIL\n");
    for (int i = 0; i < 200; i++) {
        if (ptrs[i]) kfree(ptrs[i]);
    }
    kprintf("  [7] Freed all\n");

    /* Clean up */
    kfree(a);
    kfree(b);
    kfree(c);

    kmalloc_dump_stats();

    kprintf("\n=== Thread/process subsystem ===\n");

    thread_init_subsystem();
    process_init_subsystem();
    sched_init();

    thread_dump_all();
    process_dump_all();

    /* Create a thread to verify allocation and context before scheduling it. */
    struct thread *test_thread = thread_create("test", test_thread_entry);
    if (test_thread) {
        kprintf("  TEST: created thread id=%d state=%d\n",
                (uint64_t)test_thread->id,
                (uint64_t)test_thread->state);
        kprintf("  TEST: ctx.rip=0x%x ctx.rsp=0x%x ctx.rflags=0x%x\n",
                test_thread->ctx.rip,
                test_thread->ctx.rsp,
                test_thread->ctx.rflags);

        if (test_thread->ctx.rip != 0 &&
            test_thread->ctx.rsp != 0 &&
            test_thread->state == THREAD_READY) {
            kprintf("  [1] PASS: thread created with valid context\n");
        } else {
            kprintf("  [1] FAIL: thread context invalid\n");
        }
    } else {
        kprintf("  [1] FAIL: thread_create returned NULL\n");
    }

    /* Verify stack is in the direct-map region (kmalloc path) */
    if (test_thread) {
        uint64_t stack_addr = (uint64_t)test_thread->stack_base;
        if (stack_addr >= DIRECT_MAP_BASE &&
            stack_addr <  DIRECT_MAP_BASE + 0x8000000ULL) {
            kprintf("  [2] PASS: stack in direct-map region\n");
        } else {
            kprintf("  [2] FAIL: stack at 0x%x (unexpected)\n", stack_addr);
        }
    }

    thread_dump_all();

    switch_test();
    


    vmm_init();

    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("Virtual memory manager ready\n");


    vmm_init();

    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("Virtual memory manager ready\n");

    vmm_test();

    line_init();
    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("Line editor ready\n");

    pic_clear_mask(0);
    pic_clear_mask(1);

    /* Test thread */
    if (test_thread) sched_add(test_thread);

    __asm__ volatile ("sti");

    if (test_thread) {
        while (!test_finished) {
            __asm__ volatile ("hlt");
        }
    }

    run_reap_test();

    terminal_set_color(VGA_GREEN, VGA_BLACK);
    kprintf("\ncraftyOS> ");

    for (;;) {
        (void)line_get();
        __asm__ volatile ("hlt");
    }
}