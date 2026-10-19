#include "tests.h"
#include "vmm.h"
#include "pmm.h"
#include "kprintf.h"

void vmm_test(void) {
    kprintf("\nVMM Test:\n");

    uint64_t test_virt = 0xDEADB000ULL;
    uint64_t phys1 = (uint64_t)pmm_alloc_page();
    kprintf(" [1] Allocated phys page: 0x%x\n", phys1);

    if (vmm_map(test_virt, phys1, VMM_KERNEL_RW) != 0) {
        kprintf("  [1] FAIL: vmm_map returned error\n");
        pmm_free_page((void *)phys1);
        return;
    }

    volatile uint64_t *test = (volatile uint64_t *)test_virt;
    *test = 0xCAFEBABE12345678ULL;
    uint64_t read_back = *test;
    uint64_t translated = vmm_translate(test_virt);

    if (read_back == 0xCAFEBABE12345678ULL && translated == phys1) {
        kprintf("  [1] PASS: map/read/translate roundtrip\n");
    } else {
        kprintf("  [1] FAIL: read=0x%x translated=0x%x\n", read_back, translated);
    }

    vmm_unmap(test_virt);
    pmm_free_page((void *)phys1);

     uint64_t base = 0xC0DE0000ULL;
    uint64_t p1 = (uint64_t)pmm_alloc_page();
    uint64_t p2 = (uint64_t)pmm_alloc_page();
    uint64_t p3 = (uint64_t)pmm_alloc_page();

    vmm_map(base,           p1, VMM_KERNEL_RW);
    vmm_map(base + 0x1000,  p2, VMM_KERNEL_RW);
    vmm_map(base + 0x2000,  p3, VMM_KERNEL_RW);

       int ok = 1;
    if (vmm_translate(base)          != p1) ok = 0;
    if (vmm_translate(base + 0x1000) != p2) ok = 0;
    if (vmm_translate(base + 0x2000) != p3) ok = 0;

    vmm_unmap(base + 0x1000);

    if (vmm_translate(base + 0x1000) != 0) ok = 0;
    if (vmm_translate(base)          != p1) ok = 0;
    if (vmm_translate(base + 0x2000) != p3) ok = 0;

    if (ok) {
        kprintf("  [2] PASS: three pages, middle unmapped\n");
    } else {
        kprintf("  [2] FAIL\n");
    }

    vmm_unmap(base);
    vmm_unmap(base + 0x2000);
    pmm_free_page((void *)p1);
    pmm_free_page((void *)p2);
    pmm_free_page((void *)p3);

    uint64_t huge = vmm_translate(0x100000);
    if (huge == 0x100000) {
        kprintf("  [3] PASS: 2 MB huge page translation\n");
    } else {
        kprintf("  [3] FAIL: 0x100000 -> 0x%x\n", huge);
    }

    uint64_t unmapped = vmm_translate(0xBAD000000ULL);
    if (unmapped == 0) {
        kprintf("  [4] PASS: unmapped address returns 0\n");
    } else {
        kprintf("  [4] FAIL: 0xBAD000000 -> 0x%x\n", unmapped);
    }

    kprintf("VMM test complete.\n");
}
