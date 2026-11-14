#include "process.h"
#include "thread.h"
#include "sched.h"
#include "kprintf.h"
#include "kmalloc.h"
#include "address_space.h"
#include "buddy.h"
#include "pmm.h"
#include "vmm.h"
#include "fd.h"
#include "vfs.h"
#include "userspace.h"
#include "elf.h"
#include "user.h"

#include <stddef.h>

extern void fork_enter_user_asm(struct fork_frame *frame) __attribute__((noreturn));
extern void exec_enter_user_asm(struct exec_frame *frame) __attribute__((noreturn));

_Static_assert(offsetof(struct fork_frame, rip)  == 120, "fork_frame.rip offset changed");
_Static_assert(offsetof(struct fork_frame, rsp)  == 136, "fork_frame.rsp offset changed");
_Static_assert(sizeof(struct fork_frame) == 144, "fork_frame size changed");

_Static_assert(offsetof(struct exec_frame, rip)  == 120, "exec_frame.rip offset");
_Static_assert(offsetof(struct exec_frame, rsp)  == 136, "exec_frame.rsp offset");
_Static_assert(sizeof(struct exec_frame) == 144, "exec_frame size");


static pid_t next_pid = 1;
static struct process *process_list = 0;
static struct process *current_process = 0;
static uint64_t kernel_pml4_phys;

__attribute__((noreturn))
static void fork_enter_user(struct fork_frame *frame, uint64_t pml4_phys) {
    vmm_switch_address_space(pml4_phys);
    fork_enter_user_asm(frame);
    __builtin_unreachable();
}

static void process_unlink_and_free(struct process *p) {
    if (process_list == p) {
        process_list = p->next;
    } else {
        struct process *prev = process_list;
        while (prev && prev->next != p) prev = prev->next;
        if (prev) prev->next = p->next;
    }
    kfree(p);
}

void process_init_subsystem(void) {
    struct process *kproc = (struct process *)kmalloc(sizeof(struct process));
    if (!kproc) {
        kprintf(" PROCESS: failed to allocate kernel process\n");
        return;
    }
    uint8_t *p = (uint8_t *)kproc;
    for (unsigned long i = 0; i < sizeof(struct process); i++) p[i] = 0;
    kproc->pid = next_pid++;
    const char *pn = "kernel";
    int i = 0;
    while (pn[i] && i < PROCESS_NAME_LEN - 1) { kproc->name[i] = pn[i]; i++; }
    kproc->name[i] = 0;
    kproc->state = 0;
    kernel_pml4_phys = vmm_get_current_pml4();
    kproc->pml4_phys = kernel_pml4_phys;
    kproc->threads = 0;
    kproc->parent = 0;
    kproc->next = process_list;
    process_list = kproc;
    kproc->total_ticks = 0;
    kproc->uid = 0;
    kproc->gid = 0;
    fd_table_init(&kproc->fds);
    current_process = kproc;
    struct thread *boot_thread = thread_current();
    if (boot_thread) boot_thread->process = kproc;
    kprintf(" PROCESS: kernel process pid=%d created (pml4=0x%x)\n", (uint64_t)kproc->pid, kproc->pml4_phys);
}

struct process *process_create(const char *name, void (*entry)(void *), void *arg) {
    struct process *proc = (struct process *)kmalloc(sizeof(struct process));
    if (!proc) return 0;

    uint8_t *p = (uint8_t *)proc;
    for (unsigned long i = 0; i < sizeof(struct process); i++) p[i] = 0;

    proc->pid = next_pid++;
    int i = 0;
    while (name[i] && i < PROCESS_NAME_LEN - 1) { proc->name[i] = name[i]; i++; }
    proc->name[i] = 0;

    proc->state = 0;
    proc->pml4_phys = vmm_create_address_space();
    if (!proc->pml4_phys) {
        kfree(proc);
        return 0;
    }
    proc->threads = 0;
    proc->parent = process_current();
    proc->next = process_list;
    process_list = proc;
    proc->total_ticks = 0;

    proc->parent = process_current();
    if (proc->parent) {
        proc->uid = proc->parent->uid;
        proc->gid = proc->parent->gid;
    } else {
        proc->uid = 0;
        proc->gid = 0;
    }
 
    fd_table_init(&proc->fds);

    struct thread *t = thread_create(name, entry, arg);
    if (!t) {
        kfree(proc);
        return 0;
    }
    t->process = proc;
    proc->threads = t;
    sched_add(t);

    kprintf("  PROCESS: created '%s' pid=%d thread_id=%d\n",
            proc->name, (uint64_t)proc->pid, (uint64_t)t->id);

    return proc;
}

struct process *process_current(void) {
    struct thread *thread = thread_current();
    if (thread && thread->process) return thread->process;
    return current_process;
}

void process_exit(uint32_t status) {
    struct process *p = process_current();
    if (!p) {
        for (;;) __asm__ volatile ("cli; hlt");
    }

    p->exit_status = status;
    p->exit_called = 1;

    kprintf("  PROCESS: '%s' (pid=%d) exiting with status %d\n",
            p->name, (uint64_t)p->pid, (uint64_t)status);

    fd_close_all(&p->fds);

    if (p->waiting_parent_thread) {
        sched_wake(p->waiting_parent_thread);
    }
    thread_exit();
    for (;;) __asm__ volatile ("cli; hlt");
}

void process_free_address_space(uint64_t pml4_phys) {
    if (!pml4_phys || pml4_phys == kernel_pml4_phys) return;

    uint64_t *pml4 = (uint64_t *)(DIRECT_MAP_BASE + pml4_phys);
    uint64_t *kernel_pml4 = (uint64_t *)(DIRECT_MAP_BASE + kernel_pml4_phys);

    for (int i = 0; i < 256; i++) {
        uint64_t pml4_entry = pml4[i];
        if (!(pml4_entry & VMM_PRESENT)) continue;
        if (pml4_entry == kernel_pml4[i]) continue;

        uint64_t pdpt_phys = pml4_entry & 0x000FFFFFFFFFF000ULL;
        uint64_t *pdpt = (uint64_t *)(DIRECT_MAP_BASE + pdpt_phys);

        uint64_t *shared_pdpt = 0;
        if (kernel_pml4[i] & VMM_PRESENT) {
            shared_pdpt = (uint64_t *)(DIRECT_MAP_BASE +
                           (kernel_pml4[i] & 0x000FFFFFFFFFF000ULL));
        }

        for (int j = 0; j < 512; j++) {
            uint64_t pdpt_entry = pdpt[j];
            if (!(pdpt_entry & VMM_PRESENT)) continue;
            if (shared_pdpt && pdpt_entry == shared_pdpt[j]) continue;
            if (pdpt_entry & VMM_HUGE) continue;

            uint64_t pd_phys = pdpt_entry & 0x000FFFFFFFFFF000ULL;
            uint64_t *pd = (uint64_t *)(DIRECT_MAP_BASE + pd_phys);

            for (int k = 0; k < 512; k++) {
                uint64_t pd_entry = pd[k];
                if (!(pd_entry & VMM_PRESENT)) continue;

                if (pd_entry & VMM_HUGE) {
                    uint64_t huge_phys = pd_entry & 0x000FFFFFFFE00000ULL;
                    free_pages((void *)huge_phys, 9);
                    continue;
                }

                uint64_t pt_phys = pd_entry & 0x000FFFFFFFFFF000ULL;
                uint64_t *pt = (uint64_t *)(DIRECT_MAP_BASE + pt_phys);

                for (int m = 0; m < 512; m++) {
                    uint64_t pt_entry = pt[m];
                    if (!(pt_entry & VMM_PRESENT)) continue;
                    uint64_t leaf_phys = pt_entry & 0x000FFFFFFFFFF000ULL;
                    pmm_free_page((void *)leaf_phys);
                }

                pmm_free_page((void *)pt_phys);
            }
            pmm_free_page((void *)pd_phys);
        }
        pmm_free_page((void *)pdpt_phys);
    }
    pmm_free_page((void *)pml4_phys);
}

int process_wait(pid_t pid, uint32_t *status_out) {
    struct process *p = process_list;
    while (p) {
        if (p->pid == pid && p->parent == process_current()) {
            break;
        }
        p = p->next;
    }
    if (!p) return -1;

    struct thread *self = thread_current();
    if (!self) return -1;

    for (;;) {
        if (p->exit_called) {
            if (status_out) *status_out = p->exit_status;
            process_free_address_space(p->pml4_phys);
            process_unlink_and_free(p);
            return 0;
        }

        p->waiting_parent_thread = self;
        self->state = THREAD_BLOCKED;
        schedule();
    }
}

uint64_t process_count(void) {
    uint64_t n = 0;
    for (struct process *p = process_list; p; p = p->next) n++;
    return n;
}

void process_dump_all(void) {
    int count = 0;
    for (struct process *p = process_list; p; p = p->next) count++;
    kprintf("  PROCESS: %d processes\n", (uint64_t)count);
    for (struct process *p = process_list; p; p = p->next) {
        kprintf("    pid=%d name='%s' pml4=0x%x\n",
                (uint64_t)p->pid, p->name, p->pml4_phys);
    }
}

struct process *process_fork(struct syscall_regs *regs) {
    if (!regs) return 0;

    struct process *parent = process_current();
    if (!parent) return 0;

    struct process *child = (struct process *)kmalloc(sizeof(struct process));
    if (!child) return 0;
    uint8_t *zero = (uint8_t *)child;
    for (unsigned long i = 0; i < sizeof(struct process); i++) zero[i] = 0;

    child->pid = next_pid++;
    int i = 0;
    while (parent->name[i] && i < PROCESS_NAME_LEN - 1) {
        child->name[i] = parent->name[i];
        i++;
    }
    child->name[i] = 0;

    child->state = 0;
    child->parent = parent;
    child->next = process_list;
    process_list = child;
    child->total_ticks = 0;
    child->uid = parent->uid;
    child->gid = parent->gid;

    fd_table_init(&child->fds);

    child->pml4_phys = vmm_create_address_space();
    if (!child->pml4_phys) {
        process_unlink_and_free(child);
        return 0;
    }

#define FORK_MAX_USER_PAGES 16
    struct { uint64_t virt; uint64_t parent_phys; uint64_t child_phys; }
        pages[FORK_MAX_USER_PAGES];
    int npages = 0;

    for (uint64_t va = USER_CODE_BASE; va < USER_STACK_TOP; va += VMM_PAGE_SIZE) {
        uint64_t phys = vmm_translate(va);
        if (phys == 0) continue;
        if (npages >= FORK_MAX_USER_PAGES) break;
        pages[npages].virt        = va;
        pages[npages].parent_phys = phys & 0x000FFFFFFFFFF000ULL;
        pages[npages].child_phys  = 0;
        npages++;
    }

    if (npages == 0) {
        kprintf("  FORK: parent has no user pages\n");
        process_free_address_space(child->pml4_phys);
        process_unlink_and_free(child);
        return 0;
    }

    for (int k = 0; k < npages; k++) {
        uint64_t new_phys = (uint64_t)pmm_alloc_page();
        if (!new_phys) {
            kprintf("  FORK: page alloc failed\n");
            for (int j = 0; j < k; j++) pmm_free_page((void *)pages[j].child_phys);
            process_free_address_space(child->pml4_phys);
            process_unlink_and_free(child);
            return 0;
        }
        pages[k].child_phys = new_phys;

        uint8_t *src = (uint8_t *)(DIRECT_MAP_BASE + pages[k].parent_phys);
        uint8_t *dst = (uint8_t *)(DIRECT_MAP_BASE + new_phys);
        for (int b = 0; b < VMM_PAGE_SIZE; b++) dst[b] = src[b];
    }

    uint64_t saved_pml4 = vmm_get_current_pml4();
    vmm_switch_address_space(child->pml4_phys);
    for (int k = 0; k < npages; k++) {
        vmm_map_user(pages[k].virt, pages[k].child_phys, VMM_WRITABLE);
    }
    vmm_switch_address_space(saved_pml4);

    struct thread *t = thread_create(child->name, 0, 0);
    if (!t) {
        process_free_address_space(child->pml4_phys);
        process_unlink_and_free(child);
        return 0;
    }
    t->process  = child;
    child->threads = t;

    uint8_t *stack_top = (uint8_t *)t->stack_base + THREAD_STACK_SIZE;
    struct fork_frame *frame = (struct fork_frame *)
        (stack_top - sizeof(struct fork_frame) - 16);

    frame->rax    = 0;
    frame->rbx    = regs->rbx;
    frame->rcx    = regs->rcx;
    frame->rdx    = regs->rdx;
    frame->rsi    = regs->rsi;
    frame->rdi    = regs->rdi;
    frame->rbp    = regs->rbp;
    frame->r8     = regs->r8;
    frame->r9     = regs->r9;
    frame->r10    = regs->r10;
    frame->r11    = regs->r11;
    frame->r12    = regs->r12;
    frame->r13    = regs->r13;
    frame->r14    = regs->r14;
    frame->r15    = regs->r15;
    frame->rip    = regs->rip;
    frame->rflags = regs->rflags;
    frame->rsp    = regs->rsp;

    t->ctx.rip    = (uint64_t)fork_enter_user;
    t->ctx.rsp    = (uint64_t)frame - 8;
    t->ctx.rdi    = (uint64_t)frame;
    t->ctx.rsi    = child->pml4_phys;
    t->ctx.rflags = 0x202;

    sched_add(t);
    kprintf("  FORK: pid=%d -> child pid=%d (pml4=0x%x, pages=%d) "
            "rip=0x%x rsp=0x%x\n",
            (uint64_t)parent->pid, (uint64_t)child->pid,
            child->pml4_phys, (uint64_t)npages,
            regs->rip, regs->rsp);

    return child;
}

static void free_user_pages(struct process *p) {
    uint64_t saved = vmm_get_current_pml4();
    vmm_switch_address_space(p->pml4_phys);
    for (uint64_t va = USER_CODE_BASE; va < USER_STACK_TOP; va += VMM_PAGE_SIZE) {
        uint64_t phys = vmm_translate(va);
        if (phys == 0) continue;
        uint64_t page = phys & 0x000FFFFFFFFFF000ULL;
        vmm_unmap(va);
        pmm_free_page((void *)page);
    }
    vmm_switch_address_space(saved);
}

static int copy_to_user_stack(uint64_t user_vaddr, const void *src, uint64_t len) {
    const uint8_t *s = (const uint8_t *)src;
    for (uint64_t i = 0; i < len; i++) {
        uint64_t va = user_vaddr + i;
        uint64_t phys = vmm_translate(va);
        if (phys == 0) return -1;
        uint8_t *kva = (uint8_t *)(DIRECT_MAP_BASE +
                                    (phys & 0x000FFFFFFFFFF000ULL)) +
                       (va & 0xFFF);
        *kva = s[i];
    }
    return 0;
}

void process_exec(const char *path, const char *const argv[]) {
    if (!path) process_exit(127);

    struct file *f = vfs_open(path, 0);
    if (!f) {
        kprintf("  EXEC: '%s' not found\n", path);
        process_exit(127);
    }

    struct process *p = process_current();
    if (!p) {
        vfs_close(f);
        process_exit(127);
    }

    free_user_pages(p);

    uint64_t entry = 0;
    int rc = elf_load_user(f, &entry);
    vfs_close(f);
    if (rc != 0) {
        kprintf("  EXEC: load failed\n");
        process_exit(127);
    }

    uint64_t stack_base  = USER_STACK_TOP - USER_STACK_SIZE;
    uint64_t stack_pages = USER_STACK_SIZE / VMM_PAGE_SIZE;
    for (uint64_t i = 0; i < stack_pages; i++) {
        uint64_t phys = (uint64_t)pmm_alloc_page();
        if (!phys) process_exit(127);
        uint8_t *kva = (uint8_t *)(DIRECT_MAP_BASE + phys);
        for (int b = 0; b < (int)VMM_PAGE_SIZE; b++) kva[b] = 0;
        if (vmm_map_user(stack_base + i * VMM_PAGE_SIZE, phys,
                          VMM_WRITABLE) != 0) {
            pmm_free_page((void *)phys);
            process_exit(127);
        }
    }

    int argc = 0;
    if (argv) {
        while (argc < 32 && argv[argc]) argc++;
    }

    uint64_t strings_bytes = 0;
    int arg_lens[32];
    for (int i = 0; i < argc; i++) {
        int len = 0;
        while (argv[i][len]) len++;
        len++;
        arg_lens[i] = len;
        strings_bytes += (uint64_t)len;
    }

    uint64_t block_bytes = strings_bytes + 8 + (uint64_t)argc * 8 + 8;

    block_bytes = (block_bytes + 15) & ~0xFULL;

    if (block_bytes > USER_STACK_SIZE) process_exit(127);

    uint64_t sp_at_entry = USER_STACK_TOP - block_bytes;

    uint64_t write_pos = USER_STACK_TOP;
    uint64_t arg_ptrs[32];
    for (int i = argc - 1; i >= 0; i--) {
        write_pos -= (uint64_t)arg_lens[i];
        if (copy_to_user_stack(write_pos, argv[i], (uint64_t)arg_lens[i]) != 0) {
            process_exit(127);
        }
        arg_ptrs[i] = write_pos;
    }

    write_pos = sp_at_entry;

    uint64_t argc_u64 = (uint64_t)argc;
    if (copy_to_user_stack(write_pos, &argc_u64, 8) != 0) process_exit(127);
    write_pos += 8;

    for (int i = 0; i < argc; i++) {
        if (copy_to_user_stack(write_pos, &arg_ptrs[i], 8) != 0) process_exit(127);
        write_pos += 8;
    }

    uint64_t zero = 0;
    if (copy_to_user_stack(write_pos, &zero, 8) != 0) process_exit(127);

    struct exec_frame frame;
    for (size_t i = 0; i < sizeof(frame); i++) ((uint8_t *)&frame)[i] = 0;
    frame.rip    = entry;
    frame.rsp    = sp_at_entry;
    frame.rflags = 0x202;

    exec_enter_user_asm(&frame);
    __builtin_unreachable();
}