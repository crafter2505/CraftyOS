/* switch_test.c - round-trip test for context switching */

#include "tests.h"
#include "switch_test.h"
#include "thread.h"
#include "kprintf.h"

static struct thread_context ctx_a;
static struct thread_context ctx_b;
static struct thread_context *switch_back_to = 0;

static uint8_t stack_b[4096] __attribute__((aligned(16)));

/* Result shared between B and A */
static volatile int b_verified_ok = 0;
static volatile uint64_t b_failed_reg = 0;
static volatile uint64_t b_got_value = 0;
static volatile uint64_t b_expected_value = 0;

/* Sentinels */
#define SENT_R15 0x1111111111111111ULL
#define SENT_R14 0x2222222222222222ULL
#define SENT_R13 0x3333333333333333ULL
#define SENT_R12 0x4444444444444444ULL
#define SENT_R11 0x5555555555555555ULL
#define SENT_R10 0x6666666666666666ULL
#define SENT_R9  0x7777777777777777ULL
#define SENT_R8  0x8888888888888888ULL
#define SENT_RBP 0x9999999999999999ULL

/* Macros to read a specific register */
#define READ_REG(r) ({ uint64_t __v; __asm__ volatile ("mov %%" #r ", %0" : "=r"(__v)); __v; })

static void thread_b_entry(void) {
    /* We're now running on B's stack, with B's registers loaded.
     * Verify each register matches its sentinel. */

    uint64_t r15 = READ_REG(r15);
    uint64_t r14 = READ_REG(r14);
    uint64_t r13 = READ_REG(r13);
    uint64_t r12 = READ_REG(r12);
    uint64_t r11 = READ_REG(r11);
    uint64_t r10 = READ_REG(r10);
    uint64_t r9  = READ_REG(r9);
    uint64_t r8  = READ_REG(r8);
    uint64_t rbp = READ_REG(rbp);

    if (r15 != SENT_R15) { b_failed_reg = 15; b_got_value = r15; b_expected_value = SENT_R15; goto done; }
    if (r14 != SENT_R14) { b_failed_reg = 14; b_got_value = r14; b_expected_value = SENT_R14; goto done; }
    if (r13 != SENT_R13) { b_failed_reg = 13; b_got_value = r13; b_expected_value = SENT_R13; goto done; }
    if (r12 != SENT_R12) { b_failed_reg = 12; b_got_value = r12; b_expected_value = SENT_R12; goto done; }
    if (r11 != SENT_R11) { b_failed_reg = 11; b_got_value = r11; b_expected_value = SENT_R11; goto done; }
    if (r10 != SENT_R10) { b_failed_reg = 10; b_got_value = r10; b_expected_value = SENT_R10; goto done; }
    if (r9  != SENT_R9 ) { b_failed_reg = 9;  b_got_value = r9;  b_expected_value = SENT_R9;  goto done; }
    if (r8  != SENT_R8 ) { b_failed_reg = 8;  b_got_value = r8;  b_expected_value = SENT_R8;  goto done; }

    b_verified_ok = 1;

done:
    /* Switch back to A. Never returns. */
    switch_to(&ctx_b, switch_back_to);

    /* Unreachable */
    for (;;) __asm__ volatile ("cli; hlt");
}

void test_switch(void) {
    kprintf("\n=== switch_to round-trip test ===\n");

    /* Zero both contexts */
    uint8_t *pa = (uint8_t *)&ctx_a;
    uint8_t *pb = (uint8_t *)&ctx_b;
    for (unsigned long i = 0; i < sizeof(ctx_a); i++) pa[i] = 0;
    for (unsigned long i = 0; i < sizeof(ctx_b); i++) pb[i] = 0;

    /* Preload B with sentinels */
    ctx_b.r15 = SENT_R15;
    ctx_b.r14 = SENT_R14;
    ctx_b.r13 = SENT_R13;
    ctx_b.r12 = SENT_R12;
    ctx_b.r11 = SENT_R11;
    ctx_b.r10 = SENT_R10;
    ctx_b.r9  = SENT_R9;
    ctx_b.r8  = SENT_R8;

    ctx_b.rip = (uint64_t)thread_b_entry;

    uint64_t b_stack_top = (uint64_t)stack_b + sizeof(stack_b);
    b_stack_top &= ~0xFULL;
    ctx_b.rsp = b_stack_top;
    ctx_b.rflags = 0x202;

    switch_back_to = &ctx_a;

    b_verified_ok = 0;
    b_failed_reg = 0;
    b_got_value = 0;
    b_expected_value = 0;

    kprintf("  [1] switching to B with sentinels loaded...\n");

    switch_to(&ctx_a, &ctx_b);

    /* Back in A. */
    kprintf("  [2] back in A\n");

    if (b_verified_ok) {
        kprintf("  [3] PASS: all sentinel registers matched in B\n");
        kprintf("=== switch_to test PASSED ===\n\n");
    } else {
        kprintf("  [3] FAIL: register r%d = 0x%x (expected 0x%x)\n",
                b_failed_reg, b_got_value, b_expected_value);
        kprintf("=== switch_to test FAILED ===\n\n");
    }
}

void switch_test(void) {
    test_switch();
}