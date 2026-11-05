#include "elf.h"
#include "vfs.h"
#include "vmm.h"
#include "pmm.h"
#include "address_space.h"
#include "userspace.h"
#include "kprintf.h"

#define PAGE_SIZE 4096ULL

static uint64_t round_up_page(uint64_t n) {
    return (n + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static int validate_ehdr(const Elf64_Ehdr *h) {
    if (*(const uint32_t *)h->e_ident != ELF_MAGIC)     return -1;
    if (h->e_ident[EI_CLASS] != ELFCLASS64)             return -1;
    if (h->e_ident[EI_DATA]  != ELFDATA2LSB)            return -1;
    if (h->e_machine != EM_X86_64)                       return -1;
    if (h->e_phnum == 0 || h->e_phnum > ELF_MAX_PHDRS)   return -1;
    if (h->e_phentsize != sizeof(Elf64_Phdr))            return -1;
    return 0;
}

static int validate_phdr(const Elf64_Phdr *ph, uint64_t file_size) {
    if (ph->p_memsz < ph->p_filesz)                     return -1;
    if (ph->p_offset + ph->p_filesz < ph->p_offset)     return -1;
    if (ph->p_offset + ph->p_filesz > file_size)        return -1;
    if (ph->p_vaddr < USER_CODE_BASE)                   return -1;
    if (ph->p_vaddr >= USER_SPACE_LIMIT)                return -1;
    if (ph->p_vaddr + ph->p_memsz < ph->p_vaddr)        return -1;
    if (ph->p_vaddr + ph->p_memsz > USER_SPACE_LIMIT)   return -1;
    if (ph->p_align > PAGE_SIZE)                        return -1;
    if (ph->p_align != 0 && ph->p_align != 1 &&
        (ph->p_align & (ph->p_align - 1)) != 0)         return -1;
    return 0;
}

int elf_load_user(struct file *f, uint64_t *entry_out) {
    if (!f || !entry_out) {
        kprintf("  ELF: null args\n");
        return -1;
    }

    struct vfs_stat st;
    if (vfs_fstat(f, &st) != 0) {
        kprintf("  ELF: fstat failed\n");
        return -1;
    }
    uint64_t file_size = st.size;
    kprintf("  ELF: file_size=%d\n", file_size);

    if (vfs_seek(f, 0, SEEK_SET) != 0) {
        kprintf("  ELF: rewind failed\n");
        return -1;
    }

    Elf64_Ehdr ehdr;
    if (vfs_read(f, &ehdr, sizeof(ehdr)) != (int)sizeof(ehdr)) {
        kprintf("  ELF: ehdr read failed\n");
        return -1;
    }
    kprintf("  ELF: magic=0x%x class=%d data=%d machine=%d phnum=%d phentsz=%d phoff=0x%x entry=0x%x\n",
            *(uint32_t *)ehdr.e_ident, ehdr.e_ident[4], ehdr.e_ident[5],
            ehdr.e_machine, ehdr.e_phnum, ehdr.e_phentsize,
            ehdr.e_phoff, ehdr.e_entry);

    if (validate_ehdr(&ehdr) != 0) {
        kprintf("  ELF: validate_ehdr failed\n");
        return -1;
    }

    if (vfs_seek(f, ehdr.e_phoff, SEEK_SET) != ehdr.e_phoff) {
        kprintf("  ELF: seek phoff failed (wanted 0x%x)\n", ehdr.e_phoff);
        return -1;
    }

    Elf64_Phdr phdrs[ELF_MAX_PHDRS];
    uint64_t ph_bytes = (uint64_t)ehdr.e_phnum * sizeof(Elf64_Phdr);
    if (vfs_read(f, phdrs, ph_bytes) != (int)ph_bytes) {
        kprintf("  ELF: phdr read failed\n");
        return -1;
    }

    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
        kprintf("  ELF: phdr[%d] type=%d off=0x%x vaddr=0x%x filesz=%d memsz=%d align=0x%x\n",
                i, phdrs[i].p_type, phdrs[i].p_offset, phdrs[i].p_vaddr,
                phdrs[i].p_filesz, phdrs[i].p_memsz, phdrs[i].p_align);
    }

    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_INTERP) {
            kprintf("  ELF: PT_INTERP rejected\n");
            return -1;
        }
    }

    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
        const Elf64_Phdr *ph = &phdrs[i];
        if (ph->p_type != PT_LOAD)   continue;
        if (ph->p_memsz == 0)        continue;
        if (validate_phdr(ph, file_size) != 0) {
            kprintf("  ELF: validate_phdr[%d] failed\n", i);
            return -1;
        }
    }

    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
        const Elf64_Phdr *ph = &phdrs[i];
        if (ph->p_type != PT_LOAD)   continue;
        if (ph->p_memsz == 0)        continue;

        uint64_t va_start = ph->p_vaddr & ~(PAGE_SIZE - 1);
        uint64_t va_end   = round_up_page(ph->p_vaddr + ph->p_memsz);
        uint64_t nr_pages = (va_end - va_start) / PAGE_SIZE;

        kprintf("  ELF: loading segment %d, %d pages\n", i, nr_pages);

        if (nr_pages > 64) {
            kprintf("  ELF: too many pages\n");
            return -1;
        }
        uint64_t phys_addrs[64];

        for (uint64_t p = 0; p < nr_pages; p++) {
            uint64_t va   = va_start + p * PAGE_SIZE;
            uint64_t phys = (uint64_t)pmm_alloc_page();
            if (!phys) {
                kprintf("  ELF: page alloc failed\n");
                return -1;
            }

            uint8_t *kva = (uint8_t *)(DIRECT_MAP_BASE + phys);
            for (int b = 0; b < (int)PAGE_SIZE; b++) kva[b] = 0;

            if (vmm_map_user(va, phys, VMM_WRITABLE) != 0) {
                kprintf("  ELF: vmm_map_user failed at 0x%x\n", va);
                pmm_free_page((void *)phys);
                return -1;
            }
            phys_addrs[p] = phys;
        }

        if (ph->p_filesz == 0) continue;

        if (vfs_seek(f, ph->p_offset, SEEK_SET) != ph->p_offset) {
            kprintf("  ELF: seek p_offset failed (wanted 0x%x)\n", ph->p_offset);
            return -1;
        }

        uint64_t written   = 0;
        uint64_t remaining = ph->p_filesz;
        while (remaining > 0) {
            uint64_t cur_va    = ph->p_vaddr + written;
            uint64_t page_idx  = (cur_va - va_start) / PAGE_SIZE;
            uint64_t off_in_pg = cur_va & (PAGE_SIZE - 1);
            uint64_t space     = PAGE_SIZE - off_in_pg;
            uint64_t chunk     = remaining < space ? remaining : space;

            uint8_t *dst = (uint8_t *)(DIRECT_MAP_BASE
                                       + phys_addrs[page_idx] + off_in_pg);
            int got = vfs_read(f, dst, chunk);
            if (got != (int)chunk) {
                kprintf("  ELF: read chunk failed (got %d, want %d)\n", got, (int)chunk);
                return -1;
            }

            written   += chunk;
            remaining -= chunk;
        }
    }

    kprintf("  ELF: success entry=0x%x\n", ehdr.e_entry);
    *entry_out = ehdr.e_entry;
    return 0;
}