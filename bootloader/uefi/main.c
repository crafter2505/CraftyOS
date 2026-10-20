#include "efi.h"
#include "elf.h"
#include "../../kernel/lib/boot_info.h"

#define EFI_ALLOCATE_MAX_ADDRESS 1
#define EFI_ALLOCATE_ADDRESS 2
#define EFI_LOADER_DATA 2
#define EFI_CONVENTIONAL_MEMORY 7
#define EFI_UNUSABLE_MEMORY 8
#define EFI_ACPI_RECLAIM_MEMORY 9
#define EFI_ACPI_MEMORY_NVS 10
#define PAGE_SIZE 4096ULL
#define LARGE_PAGE_SIZE 0x200000ULL
#define IDENTITY_LIMIT 0x40000000ULL
#define DIRECT_MAP_LIMIT 0x400000000ULL
#define DIRECT_MAP_PD_COUNT (DIRECT_MAP_LIMIT / 0x40000000ULL)
#define KERNEL_PHYS_BASE 0x200000ULL
#define KERNEL_VIRT_BASE 0xffffffff80000000ULL
#define KERNEL_PHYS_LIMIT 0x40000000ULL
#define ELF_MAX_PHDRS 32
#define PAGE_TABLE_PAGES (4 + DIRECT_MAP_PD_COUNT + 2)
#define STACK_PAGES 16

static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *console;

static void print(const CHAR16 *text) {
    console->OutputString(console, (CHAR16 *)text);
}

static int add_overflow_u64(uint64_t a, uint64_t b, uint64_t *result) {
    *result = a + b;
    return *result < a;
}

static void zero_memory(void *ptr, UINTN size) {
    uint8_t *bytes = ptr;
    for (UINTN i = 0; i < size; i++) bytes[i] = 0;
}

static EFI_STATUS allocate_low_pages(EFI_BOOT_SERVICES *bs, UINTN pages,
                                     uint64_t *physical) {
    *physical = IDENTITY_LIMIT - 1;
    return bs->AllocatePages(EFI_ALLOCATE_MAX_ADDRESS, EFI_LOADER_DATA,
                             pages, physical);
}

static int read_exact(EFI_FILE_PROTOCOL *file, uint64_t offset, UINTN size,
                      void *buffer) {
    EFI_STATUS status = file->SetPosition(file, offset);
    if (status != EFI_SUCCESS) return 0;
    UINTN actual = size;
    status = file->Read(file, &actual, buffer);
    return status == EFI_SUCCESS && actual == size;
}

static int validate_elf(EFI_FILE_PROTOCOL *file, uint64_t file_size,
                        Elf64_Ehdr *header, Elf64_Phdr *program_headers,
                        uint64_t *kernel_end) {
    if (!read_exact(file, 0, sizeof(*header), header)) return 0;
    if (header->e_ident[0] != 0x7f || header->e_ident[1] != 'E' ||
        header->e_ident[2] != 'L' || header->e_ident[3] != 'F' ||
        header->e_ident[4] != 2 || header->e_ident[5] != 1 ||
        header->e_ident[6] != EV_CURRENT ||
        header->e_type != ET_EXEC || header->e_machine != EM_X86_64 ||
        header->e_version != EV_CURRENT ||
        header->e_ehsize != sizeof(*header) ||
        header->e_phentsize != sizeof(Elf64_Phdr) ||
        header->e_phnum == 0 || header->e_phnum > ELF_MAX_PHDRS) {
        return 0;
    }

    uint64_t phdr_bytes = (uint64_t)header->e_phnum * header->e_phentsize;
    uint64_t phdr_end;
    if (add_overflow_u64(header->e_phoff, phdr_bytes, &phdr_end) ||
        phdr_end > file_size ||
        !read_exact(file, header->e_phoff, (UINTN)phdr_bytes, program_headers)) {
        return 0;
    }

    uint64_t first_phys = UINT64_MAX;
    uint64_t last_phys = 0;
    int entry_is_executable = 0;
    for (uint16_t i = 0; i < header->e_phnum; i++) {
        Elf64_Phdr *phdr = &program_headers[i];
        if (phdr->p_type != PT_LOAD) continue;
        uint64_t file_end;
        uint64_t segment_end;
        if (phdr->p_memsz < phdr->p_filesz ||
            add_overflow_u64(phdr->p_offset, phdr->p_filesz, &file_end) ||
            file_end > file_size ||
            add_overflow_u64(phdr->p_paddr, phdr->p_memsz, &segment_end) ||
            phdr->p_paddr < KERNEL_PHYS_BASE ||
            segment_end > KERNEL_PHYS_LIMIT ||
            phdr->p_vaddr != KERNEL_VIRT_BASE +
                               (phdr->p_paddr - KERNEL_PHYS_BASE)) {
            return 0;
        }
        if (phdr->p_memsz == 0) continue;
        if (phdr->p_paddr < first_phys) first_phys = phdr->p_paddr;
        if (segment_end > last_phys) last_phys = segment_end;
        if ((phdr->p_flags & PF_X) &&
            header->e_entry >= phdr->p_vaddr &&
            header->e_entry < phdr->p_vaddr + phdr->p_memsz) {
            entry_is_executable = 1;
        }
    }
    if (!entry_is_executable || first_phys == UINT64_MAX ||
        (first_phys & (PAGE_SIZE - 1)) != 0) {
        return 0;
    }
    *kernel_end = (last_phys + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    return *kernel_end > first_phys;
}

static int load_segments(EFI_FILE_PROTOCOL *file, const Elf64_Ehdr *header,
                         const Elf64_Phdr *program_headers) {
    for (uint16_t i = 0; i < header->e_phnum; i++) {
        const Elf64_Phdr *phdr = &program_headers[i];
        if (phdr->p_type != PT_LOAD || phdr->p_memsz == 0) continue;
        if (phdr->p_filesz != 0) {
            if (!read_exact(file, phdr->p_offset, (UINTN)phdr->p_filesz,
                            (void *)(uintptr_t)phdr->p_paddr)) {
                return 0;
            }
        }
        zero_memory((void *)(uintptr_t)(phdr->p_paddr + phdr->p_filesz),
                    (UINTN)(phdr->p_memsz - phdr->p_filesz));
    }
    return 1;
}

static int build_page_tables(uint64_t pml4_phys) {
    uint64_t *pml4 = (uint64_t *)(uintptr_t)pml4_phys;
    uint64_t *identity_pdpt = pml4 + 512;
    uint64_t *direct_map_pdpt = identity_pdpt + 512;
    uint64_t *kernel_pdpt = direct_map_pdpt + 512;
    uint64_t *identity_pd = kernel_pdpt + 512;
    uint64_t *direct_map_pds = identity_pd + 512;
    uint64_t *kernel_pd = direct_map_pds + DIRECT_MAP_PD_COUNT * 512;

    zero_memory((void *)(uintptr_t)pml4_phys, PAGE_TABLE_PAGES * PAGE_SIZE);

    pml4[0] = (uint64_t)(uintptr_t)identity_pdpt | 0x3;
    pml4[256] = (uint64_t)(uintptr_t)direct_map_pdpt | 0x3;
    pml4[511] = (uint64_t)(uintptr_t)kernel_pdpt | 0x3;

    identity_pdpt[0] = (uint64_t)(uintptr_t)identity_pd | 0x3;
    for (uint64_t j = 0; j < 512; j++) {
        identity_pd[j] = (j * LARGE_PAGE_SIZE) | 0x83;
    }

    for (uint64_t i = 0; i < DIRECT_MAP_PD_COUNT; i++) {
        direct_map_pdpt[i] =
            (uint64_t)(uintptr_t)(direct_map_pds + i * 512) | 0x3;
        for (uint64_t j = 0; j < 512; j++) {
            uint64_t physical = (i * 512 + j) * LARGE_PAGE_SIZE;
            direct_map_pds[i * 512 + j] = physical | 0x83;
        }
    }

    kernel_pdpt[510] = (uint64_t)(uintptr_t)kernel_pd | 0x3;
    for (uint64_t i = 0; i < 512; i++) {
        kernel_pd[i] = (KERNEL_PHYS_BASE + i * LARGE_PAGE_SIZE) | 0x83;
    }
    return 1;
}

static uint32_t e820_type(uint32_t efi_type) {
    if (efi_type == EFI_CONVENTIONAL_MEMORY) return 1;
    if (efi_type == EFI_ACPI_RECLAIM_MEMORY) return 3;
    if (efi_type == EFI_ACPI_MEMORY_NVS) return 4;
    if (efi_type == EFI_UNUSABLE_MEMORY) return 5;
    return 2;
}

static int translate_memory_map(const void *map, UINTN map_size,
                               UINTN descriptor_size,
                               struct boot_info *boot_info,
                               struct e820_entry *entries) {
    if (descriptor_size < 40 || map_size % descriptor_size != 0) return 0;
    UINTN descriptor_count = map_size / descriptor_size;
    boot_info->memmap_count = 0;
    for (UINTN i = 0; i < descriptor_count; i++) {
        const uint8_t *descriptor = (const uint8_t *)map + i * descriptor_size;
        uint32_t type = *(const uint32_t *)(const void *)(descriptor + 0);
        uint64_t base = *(const uint64_t *)(const void *)(descriptor + 8);
        uint64_t pages = *(const uint64_t *)(const void *)(descriptor + 24);
        if (pages == 0 || pages > UINT64_MAX / PAGE_SIZE) continue;
        uint64_t length = pages * PAGE_SIZE;
        uint64_t end;
        if (add_overflow_u64(base, length, &end)) return 0;
        uint32_t converted_type = e820_type(type);

        if (boot_info->memmap_count != 0) {
            struct e820_entry *last = &entries[boot_info->memmap_count - 1];
            if (last->type == converted_type &&
                last->base + last->length == base) {
                last->length += length;
                continue;
            }
        }
        if (boot_info->memmap_count == BOOT_INFO_MEMMAP_MAX) return 0;
        struct e820_entry *entry = &entries[boot_info->memmap_count++];
        entry->base = base;
        entry->length = length;
        entry->type = converted_type;
        entry->_pad = 0;
    }
    return boot_info->memmap_count != 0;
}

static EFI_STATUS get_file_info(EFI_BOOT_SERVICES *bs,
                                EFI_FILE_PROTOCOL *file,
                                EFI_FILE_INFO **info) {
    EFI_GUID info_guid = EFI_FILE_INFO_GUID;
    UINTN size = 0;
    EFI_STATUS status = file->GetInfo(file, &info_guid, &size, NULL);
    if (status != EFI_BUFFER_TOO_SMALL || size < sizeof(EFI_FILE_INFO)) {
        return status == EFI_SUCCESS ? EFI_LOAD_ERROR : status;
    }
    status = bs->AllocatePool(EFI_LOADER_DATA, size, (void **)info);
    if (status != EFI_SUCCESS) return status;
    status = file->GetInfo(file, &info_guid, &size, *info);
    if (status != EFI_SUCCESS) {
        bs->FreePool(*info);
        *info = NULL;
    }
    return status;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st);

__attribute__((noreturn))
static void enter_kernel(uint64_t stack_top, struct boot_info *boot_info,
                         uint64_t entry, uint64_t pml4_phys) {
    __asm__ volatile (
        "cli\n\t"
        "mov %0, %%r12\n\t"
        "mov %1, %%r13\n\t"
        "mov %2, %%r14\n\t"
        "mov %3, %%r15\n\t"
        "mov %%r12, %%rsp\n\t"
        "and $-16, %%rsp\n\t"
        "sub $8, %%rsp\n\t"
        "mov %%r15, %%cr3\n\t"
        "mov %%r13, %%rdi\n\t"
        "jmp *%%r14\n\t"
        :
        : "r"(stack_top), "r"(boot_info), "r"(entry), "r"(pml4_phys)
        : "memory", "cc", "r12", "r13", "r14", "r15", "rdi");
    __builtin_unreachable();
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
    if (!st || !st->BootServices || !st->ConOut) return EFI_INVALID_PARAMETER;
    EFI_BOOT_SERVICES *bs = st->BootServices;
    console = st->ConOut;
    (void)bs->SetWatchdogTimer(0, 0, 0, NULL);
    console->ClearScreen(console);
    print(L"CraftyOS UEFI loader\r\n");

    EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;
    EFI_GUID loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_STATUS status = bs->HandleProtocol(image, &loaded_image_guid,
                                           (void **)&loaded_image);
    if (status != EFI_SUCCESS || !loaded_image) {
        print(L"Unable to locate Loaded Image protocol\r\n");
        return status;
    }
    uint64_t image_end;
    uint64_t image_base = (uint64_t)(uintptr_t)loaded_image->ImageBase;
    if (add_overflow_u64(image_base, loaded_image->ImageSize, &image_end) ||
        image_end > IDENTITY_LIMIT) {
        print(L"UEFI loader image is outside the identity-mapped region\r\n");
        return EFI_LOAD_ERROR;
    }

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *filesystem = NULL;
    EFI_GUID filesystem_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    status = bs->HandleProtocol(loaded_image->DeviceHandle, &filesystem_guid,
                                (void **)&filesystem);
    if (status != EFI_SUCCESS || !filesystem) {
        print(L"Unable to locate Simple File System protocol\r\n");
        return status;
    }

    EFI_FILE_PROTOCOL *root = NULL;
    status = filesystem->OpenVolume(filesystem, &root);
    if (status != EFI_SUCCESS || !root) {
        print(L"Unable to open EFI volume\r\n");
        return status;
    }
    EFI_FILE_PROTOCOL *kernel_file = NULL;
    status = root->Open(root, &kernel_file, L"\\EFI\\BOOT\\kernel.elf",
                        EFI_FILE_MODE_READ, 0);
    if (status != EFI_SUCCESS || !kernel_file) {
        print(L"Unable to open kernel.elf\r\n");
        root->Close(root);
        return status;
    }

    EFI_FILE_INFO *file_info = NULL;
    status = get_file_info(bs, kernel_file, &file_info);
    if (status != EFI_SUCCESS || !file_info) {
        print(L"Unable to get kernel file information\r\n");
        kernel_file->Close(kernel_file);
        root->Close(root);
        return status;
    }
    uint64_t file_size = file_info->FileSize;
    (void)bs->FreePool(file_info);

    Elf64_Ehdr elf_header;
    Elf64_Phdr program_headers[ELF_MAX_PHDRS];
    uint64_t kernel_end;
    if (!validate_elf(kernel_file, file_size, &elf_header, program_headers,
                      &kernel_end)) {
        print(L"Invalid or unsupported kernel ELF\r\n");
        kernel_file->Close(kernel_file);
        root->Close(root);
        return EFI_LOAD_ERROR;
    }

    UINTN kernel_pages = (UINTN)((kernel_end - KERNEL_PHYS_BASE) / PAGE_SIZE);
    uint64_t allocated_kernel = KERNEL_PHYS_BASE;
    status = bs->AllocatePages(EFI_ALLOCATE_ADDRESS, EFI_LOADER_DATA,
                               kernel_pages, &allocated_kernel);
    if (status != EFI_SUCCESS || allocated_kernel != KERNEL_PHYS_BASE) {
        print(L"Unable to reserve kernel physical range\r\n");
        kernel_file->Close(kernel_file);
        root->Close(root);
        return status;
    }
    if (!load_segments(kernel_file, &elf_header, program_headers)) {
        print(L"Unable to read kernel load segments\r\n");
        kernel_file->Close(kernel_file);
        root->Close(root);
        return EFI_LOAD_ERROR;
    }
    status = kernel_file->Close(kernel_file);
    if (status != EFI_SUCCESS) {
        root->Close(root);
        return status;
    }
    status = root->Close(root);
    if (status != EFI_SUCCESS) return status;

    uint64_t page_tables_phys;
    status = allocate_low_pages(bs, PAGE_TABLE_PAGES, &page_tables_phys);
    if (status != EFI_SUCCESS || !build_page_tables(page_tables_phys)) {
        print(L"Unable to allocate kernel page tables\r\n");
        return status;
    }
    uint64_t boot_info_phys;
    status = allocate_low_pages(bs, 1, &boot_info_phys);
    if (status != EFI_SUCCESS) {
        print(L"Unable to allocate boot information\r\n");
        return status;
    }
    uint64_t memory_map_phys;
    status = allocate_low_pages(bs, 1, &memory_map_phys);
    if (status != EFI_SUCCESS) {
        print(L"Unable to allocate translated memory map\r\n");
        return status;
    }
    uint64_t stack_phys;
    status = allocate_low_pages(bs, STACK_PAGES, &stack_phys);
    if (status != EFI_SUCCESS) {
        print(L"Unable to allocate kernel stack\r\n");
        return status;
    }

    struct boot_info *boot_info = (struct boot_info *)(uintptr_t)boot_info_phys;
    zero_memory(boot_info, PAGE_SIZE);
    boot_info->magic = BOOT_INFO_MAGIC;
    boot_info->memmap_phys = memory_map_phys;
    boot_info->kernel_phys_base = KERNEL_PHYS_BASE;
    boot_info->kernel_phys_end = kernel_end;
    boot_info->pml4_phys = page_tables_phys;

    print(L"Exiting UEFI boot services and starting kernel...\r\n");
    UINTN map_capacity = 0;
    void *memory_map = NULL;
    for (;;) {
        UINTN map_size = map_capacity;
        UINTN map_key = 0;
        UINTN descriptor_size = 0;
        uint32_t descriptor_version = 0;
        status = bs->GetMemoryMap(&map_size, memory_map, &map_key,
                                  &descriptor_size, &descriptor_version);
        if (status == EFI_BUFFER_TOO_SMALL) {
            if (memory_map) (void)bs->FreePool(memory_map);
            UINTN extra = descriptor_size ? descriptor_size * 16 : 4096;
            if (map_size > UINT64_MAX - extra) return EFI_LOAD_ERROR;
            map_capacity = map_size + extra;
            status = bs->AllocatePool(EFI_LOADER_DATA, map_capacity,
                                      &memory_map);
            if (status != EFI_SUCCESS) return status;
            continue;
        }
        if (status != EFI_SUCCESS || descriptor_size == 0) {
            print(L"Unable to retrieve final UEFI memory map\r\n");
            return status == EFI_SUCCESS ? EFI_LOAD_ERROR : status;
        }
        if (!translate_memory_map(memory_map, map_size, descriptor_size,
                                  boot_info,
                                  (struct e820_entry *)(uintptr_t)memory_map_phys)) {
            print(L"UEFI memory map exceeds supported capacity\r\n");
            return EFI_LOAD_ERROR;
        }
        status = bs->ExitBootServices(image, map_key);
        if (status == EFI_SUCCESS) {
            enter_kernel(stack_phys + STACK_PAGES * PAGE_SIZE, boot_info,
                         elf_header.e_entry, page_tables_phys);
        }
        if (status != EFI_INVALID_PARAMETER) {
            print(L"ExitBootServices failed\r\n");
            return status;
        }
    }
}
