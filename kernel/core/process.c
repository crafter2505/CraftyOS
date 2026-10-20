#include "process.h"
#include "thread.h"
#include "sched.h"
#include "kprintf.h"
#include "kmalloc.h"
#include "address_space.h"
#include "buddy.h"
#include "pmm.h"
#include "vmm.h"

static pid_t next_pid = 1;
static struct process *process_list = 0;
static struct process *current_process = 0;
static uint64_t kernel_pml4_phys;

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

    struct thread *t = thread_create(name, entry, arg);
    if (!t) {
        kfree(proc);
        return 0;
    }
    t->process = proc;
    proc->threads = t;

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

    if (p->waiting_parent_thread) {
        sched_wake(p->waiting_parent_thread);
    }
    thread_exit();
    for (;;) __asm__ volatile ("cli; hlt");
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

    if (p->exit_called) {
        if (status_out) *status_out = p->exit_status;
        process_free_address_space(p->pml4_phys);
        return 0;
    }

    struct thread *self = thread_current();
    if (!self) return -1;
    p->waiting_parent_thread = self;
    self->state = THREAD_BLOCKED;
    schedule();

    if (p->exit_called) {
        if (status_out) *status_out = p->exit_status;
        process_free_address_space(p->pml4_phys);
        return 0;
    }
    return -1;
}

void process_free_address_space(uint64_t pml4_phys) {
    if (!pml4_phys || pml4_phys == kernel_pml4_phys) return;

    uint64_t *pml4 = (uint64_t *)(DIRECT_MAP_BASE + pml4_phys);
    uint64_t *kernel_pml4 = (uint64_t *)(DIRECT_MAP_BASE + kernel_pml4_phys);

    for (int i = 0; i < 256; i++) {
        uint64_t pml4_entry = pml4[i];
        if (!(pml4_entry & VMM_PRESENT)) continue;
        if (pml4_entry == kernel_pml4[i]) continue;

        uint64_t *pdpt = (uint64_t *)(DIRECT_MAP_BASE + (pml4_entry & 0x000FFFFFFFFFF000ULL));
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

            uint64_t *pd = (uint64_t *)(DIRECT_MAP_BASE +
                          (pdpt_entry & 0x000FFFFFFFFFF000ULL));

            for (int k = 0; k < 512; k++) {
                uint64_t pd_entry = pd[k];
                if (!(pd_entry & VMM_PRESENT)) continue;

                if (pd_entry & VMM_HUGE) {
                    uint64_t phys = pd_entry & 0x000FFFFFFFE00000ULL;
                    free_pages((void *)phys, 9);
                    continue;
                }

                uint64_t *pt = (uint64_t *)(DIRECT_MAP_BASE + (pd_entry & 0x000FFFFFFFFFF000ULL));
                for (int m = 0; m < 512; m++) {
                    uint64_t pt_entry = pt[m];
                    if (!(pt_entry & VMM_PRESENT)) continue;
                    uint64_t phys = pt_entry & 0x000FFFFFFFFFF000ULL;
                    pmm_free_page((void *)phys);
                }
                pmm_free_page((void *)pt);
            }
            pmm_free_page((void *)pd);
        }
        pmm_free_page((void *)pdpt);
    }
    pmm_free_page((void *)pml4_phys);
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