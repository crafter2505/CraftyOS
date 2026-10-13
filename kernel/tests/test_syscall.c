#include "tests.h"
#include "syscall.h"
#include "io.h"
#include "kprintf.h"

#define IA32_EFER  0xC0000080
#define IA32_STAR  0xC0000081
#define IA32_LSTAR 0xC0000082
#define IA32_FMASK 0xC0000084

extern void syscall_entry(void);

void test_syscall(void) {
    const uint64_t expected_star = (0x18ULL << 48) | (0x10ULL << 32);
    const uint64_t expected_fmask = 0x40700;
    const uint64_t efer = rdmsr(IA32_EFER);

    kprintf("\n=== syscall MSR test ===\n");
    syscall_dump_msrs();

    if (rdmsr(IA32_STAR) != expected_star
            || rdmsr(IA32_LSTAR) != (uint64_t)syscall_entry
            || rdmsr(IA32_FMASK) != expected_fmask
            || (efer & 1ULL) == 0) {
        kprintf("  FAIL: syscall MSR configuration mismatch\n");
        return;
    }

    kprintf("  PASS: syscall MSRs configured correctly\n");
}
