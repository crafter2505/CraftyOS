#include "process.h"
#include "thread.h"
#include "kprintf.h"
#include "kmalloc.h"
#include "address_space.h"

static pid_t next_pid = 1;
static struct process *process_list = 0;
static struct process *current_process = 0;
#define KERNEL_PML4_PHYS 0x10000

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
    kproc->pml4_phys = KERNEL_PML4_PHYS;
    kproc->threads = 0;
    kproc->parent = 0;
    kproc->next = process_list;
    process_list = kproc;
    kproc->total_ticks = 0;
    current_process = kproc;
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
    proc->pml4_phys = KERNEL_PML4_PHYS;    /* shared for now */
    proc->threads = 0;
    proc->parent = current_process;
    proc->next = process_list;
    process_list = proc;
    proc->total_ticks = 0;

    /* Create the initial thread for this process */
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
    return current_process;
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