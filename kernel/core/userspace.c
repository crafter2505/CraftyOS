#include "userspace.h"
#include "vmm.h"
#include "pmm.h"
#include "sched.h"
#include "thread.h"
#include "kprintf.h"
#include "address_space.h"
#include "percpu.h"
#include "process.h"

extern uint8_t _user_program_start[];
extern uint8_t _user_program_end[];

int enter_userspace(uint64_t entry, uint64_t user_stack_top) {
    struct thread *self = thread_current();
    if (!self || !self->syscall_stack) {
        kprintf("  [user] FAIL: no dedicated syscall stack for current thread\n");
        return -1;
    }
    struct kernel_continuation *cont = &self->user_continuation;

    struct process *proc = process_current();
    if (proc) {
        vmm_switch_address_space(proc->pml4_phys);
    }

    uint64_t continuation_rip;

    __asm__ volatile (
        "mov %%rsp, %0\n"
        "mov %%rbp, %1\n"
        "mov %%rbx, %2\n"
        "mov %%r12, %3\n"
        "mov %%r13, %4\n"
        "mov %%r14, %5\n"
        "mov %%r15, %6\n"
        "lea 1f(%%rip), %7\n"
        : "=m"(cont->rsp),
          "=m"(cont->rbp),
          "=m"(cont->rbx),
          "=m"(cont->r12),
          "=m"(cont->r13),
          "=m"(cont->r14),
          "=m"(cont->r15),
          "=r"(continuation_rip)
        :
        : "memory"
    );
    cont->rip = continuation_rip;

    uint64_t kernel_stack = (uint64_t)self->syscall_stack +
                            self->syscall_stack_size;
    percpu_set_kernel_stack(kernel_stack);

    uint64_t user_cs = 0x28 | 3;
    uint64_t user_ss = 0x20 | 3;
    uint64_t rflags  = 0x202;

    __asm__ volatile (
        "cli\n"
        "push %0\n"
        "push %1\n"
        "push %2\n"
        "push %3\n"
        "push %4\n"
        "swapgs\n"
        "iretq\n"
        "1:\n"
        :
        : "r"(user_ss),
          "r"(user_stack_top),
          "r"(rflags),
          "r"(user_cs),
          "r"(entry)
        : "memory"
    );

    return 0;
}

void userspace_exit_to_kernel(void) {
    struct thread *self = thread_current();
    if (!self) {
        for (;;) __asm__ volatile ("cli; hlt");
    }
    struct kernel_continuation *cont = &self->user_continuation;

    __asm__ volatile (
        "mov %0, %%rsp\n"
        "mov %1, %%rbp\n"
        "mov %2, %%rbx\n"
        "mov %3, %%r12\n"
        "mov %4, %%r13\n"
        "mov %5, %%r14\n"
        "mov %6, %%r15\n"
        "jmp *%7\n"
        :
        : "r"(cont->rsp),
          "r"(cont->rbp),
          "r"(cont->rbx),
          "r"(cont->r12),
          "r"(cont->r13),
          "r"(cont->r14),
          "r"(cont->r15),
          "r"(cont->rip)
        : "memory"
    );
    __builtin_unreachable();
}

int userspace_run_program(const void *prog_start, uint64_t prog_size) {
    struct process *proc = process_current();
    if (!proc) return -1;

    vmm_switch_address_space(proc->pml4_phys);

    const uint8_t *program = prog_start;

    kprintf("  [user] program size: %d bytes\n", prog_size);

    if (!program || prog_size == 0 || prog_size > 4096) {
        kprintf("  [user] FAIL: null program or size out of range\n");
        return -1;
    }

    uint64_t code_phys = (uint64_t)pmm_alloc_page();
    if (!code_phys) {
        kprintf("  [user] FAIL: code page allocation\n");
        return -1;
    }

    int rv = vmm_map_user(USER_CODE_BASE, code_phys, VMM_WRITABLE);
    if (rv != 0) {
        kprintf("  [user] FAIL: vmm_map_user(code)\n");
        pmm_free_page((void *)code_phys);
        return -1;
    }
    proc->last_code_phys = code_phys;

    uint8_t *code_dst = (uint8_t *)(DIRECT_MAP_BASE + code_phys);
    for (uint64_t i = 0; i < prog_size; i++) {
        code_dst[i] = program[i];
    }

    kprintf("  [user] mapped code 0x%x -> phys 0x%x (%d bytes)\n",
            USER_CODE_BASE, code_phys, prog_size);

    uint64_t stack_base = USER_STACK_TOP - USER_STACK_SIZE;
    uint64_t stack_page_count = USER_STACK_SIZE / VMM_PAGE_SIZE;
    uint64_t stack_phys[USER_STACK_SIZE / VMM_PAGE_SIZE];
    for (uint64_t i = 0; i < stack_page_count; i++) {
        stack_phys[i] = (uint64_t)pmm_alloc_page();
        if (!stack_phys[i]) {
            kprintf("  [user] FAIL: stack page allocation\n");
            for (uint64_t j = 0; j < i; j++) {
                pmm_free_page((void *)stack_phys[j]);
            }
            vmm_unmap(USER_CODE_BASE);
            pmm_free_page((void *)code_phys);
            return -1;
        }
    }

    for (uint64_t i = 0; i < stack_page_count; i++) {
        rv = vmm_map_user(stack_base + i * VMM_PAGE_SIZE,
                          stack_phys[i], VMM_WRITABLE);
        if (rv != 0) {
            kprintf("  [user] FAIL: vmm_map_user(stack)\n");
            for (uint64_t j = 0; j < i; j++) {
                vmm_unmap(stack_base + j * VMM_PAGE_SIZE);
            }
            for (uint64_t j = 0; j < stack_page_count; j++) {
                pmm_free_page((void *)stack_phys[j]);
            }
            vmm_unmap(USER_CODE_BASE);
            pmm_free_page((void *)code_phys);
            return -1;
        }
    }

    kprintf("  [user] mapped stack 0x%x-0x%x (top 0x%x)\n",
            stack_base, USER_STACK_TOP, USER_STACK_TOP);

    kprintf("  [user] entering ring 3: RIP=0x%x RSP=0x%x CS=0x%x SS=0x%x\n",
            USER_CODE_BASE, USER_STACK_TOP,
            (uint64_t)(0x28 | 3), (uint64_t)(0x20 | 3));

    rv = enter_userspace(USER_CODE_BASE, USER_STACK_TOP);
    if (rv != 0) {
        return rv;
    }

    kprintf("  [user] returned from ring 3\n");

    return 0;
}

int userspace_run_embedded(void) {
    return userspace_run_program(_user_program_start,
                                 (uint64_t)(_user_program_end - _user_program_start));
}
