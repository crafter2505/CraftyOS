#include "tests.h"
#include "vmm.h"
#include "pmm.h"
#include "kprintf.h"

#define USER_TEST_VIRT  0x0000008000000000ULL

void test_userspace_mem(void) {
    kprintf("\n=== userspace memory test ===\n");

    uint64_t phys = (uint64_t)pmm_alloc_page();
    kprintf("  [1] allocated phys page: 0x%x\n", phys);

    if (!phys) {
        kprintf("  [1] FAIL: allocation\n");
        return;
    }

    int rv = vmm_map_user(USER_TEST_VIRT, phys, VMM_WRITABLE);
    if (rv != 0) {
        kprintf("  [2] FAIL: vmm_map_user returned %d\n", (uint64_t)rv);
        pmm_free_page((void *)phys);
        return;
    }
    kprintf("  [2] mapped 0x%x -> 0x%x as user\n", USER_TEST_VIRT, phys);

    if (vmm_is_user_accessible(USER_TEST_VIRT)) {
        kprintf("  [3] PASS: vmm_is_user_accessible returns 1\n");
    } else {
        kprintf("  [3] FAIL: vmm_is_user_accessible returns 0\n");
    }

    uint64_t translated = vmm_translate(USER_TEST_VIRT);
    if (translated == phys) {
        kprintf("  [4] PASS: vmm_translate returns 0x%x\n", translated);
    } else {
        kprintf("  [4] FAIL: translate=0x%x, expected=0x%x\n",
                translated, phys);
    }

    volatile uint64_t *p = (volatile uint64_t *)USER_TEST_VIRT;
    *p = 0xDEADBEEFCAFEBABEULL;
    if (*p == 0xDEADBEEFCAFEBABEULL) {
        kprintf("  [5] PASS: kernel can write/read mapped page\n");
    } else {
        kprintf("  [5] FAIL: readback mismatch\n");
    }

    vmm_unmap(USER_TEST_VIRT);
    pmm_free_page((void *)phys);
    kprintf("  cleaned up\n");

    kprintf("=== userspace memory test done ===\n");
}