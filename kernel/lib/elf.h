#ifndef KERNEL_ELF_H
#define KERNEL_ELF_H

#include "types.h"

struct file;

#define ELF_MAGIC   0x464C457F

#define EI_CLASS    4
#define EI_DATA     5
#define ELFCLASS64  2
#define ELFDATA2LSB 1

#define EM_X86_64   62

#define PT_LOAD     1
#define PT_INTERP   3

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

_Static_assert(sizeof(Elf64_Ehdr) == 64, "Elf64_Ehdr size");
_Static_assert(sizeof(Elf64_Phdr) == 56, "Elf64_Phdr size");

#define ELF_MAX_PHDRS  16

/* Maps PT_LOAD segments of `f` into the current address space, copies
 * file bytes, zeroes BSS, and writes e_entry to *entry_out.
 *
 * Validation is done up-front; nothing is mapped if it fails. Load-time
 * failure may leave pages mapped — the caller terminates the process,
 * which frees the whole address space.
 *
 * Returns 0 on success, -1 on failure. */
int elf_load_user(struct file *f, uint64_t *entry_out);

#endif