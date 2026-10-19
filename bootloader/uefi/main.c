#include "efi.h"

static void print_hex(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out, uint64_t v) {
    CHAR16 buf[19];
    buf[0] = L'0'; buf[1] = L'x';
    for (int i = 0; i < 16; i++) {
        int shift = (15 - i) * 4;
        int digit = (v >> shift) & 0xF;
        buf[2 + i] = digit < 10 ? (L'0' + digit) : (L'a' + digit - 10);
    }
    buf[18] = 0;
    out->OutputString(out, buf);
}

static void print_u64(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out, uint64_t v) {
    CHAR16 buf[21];
    int i = 20;
    buf[20] = 0;
    if (v == 0) { buf[--i] = L'0'; }
    while (v > 0) {
        buf[--i] = L'0' + (v % 10);
        v /= 10;
    }
    out->OutputString(out, &buf[i]);
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
    EFI_BOOT_SERVICES *bs = st->BootServices;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out = st->ConOut;

    bs->SetWatchdogTimer(0, 0, 0, NULL);
    out->ClearScreen(out);
    out->OutputString(out, L"craftyOS UEFI boot\r\n\r\n");

    /* ===== Step 1: Memory Map ===== */
    out->OutputString(out, L"=== memory map ===\r\n");

    UINTN map_size = 0, map_key = 0, desc_size = 0;
    uint32_t desc_ver = 0;

    /* First call: get the required size */
    bs->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_ver);
    map_size += 2 * desc_size;   /* slop for the allocation itself */

    void *map = NULL;
    bs->AllocatePool(2 /* EfiLoaderData */, map_size, &map);
    if (!map) {
        out->OutputString(out, L"AllocatePool failed\r\n");
        return 1;
    }

    /* Second call: actually get the map */
    EFI_STATUS status = bs->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_ver);
    if (status != EFI_SUCCESS) {
        out->OutputString(out, L"GetMemoryMap failed\r\n");
        return 1;
    }

    UINTN entries = map_size / desc_size;
    out->OutputString(out, L"entries: ");
    print_u64(out, entries);
    out->OutputString(out, L"\r\n");

    /* Walk the map using desc_size — NOT sizeof(EFI_MEMORY_DESCRIPTOR) */
    uint8_t *p = (uint8_t *)map;
    uint64_t usable_pages = 0;
    for (UINTN i = 0; i < entries; i++) {
        uint32_t type = *(uint32_t *)(p + 0);
        uint64_t phys = *(uint64_t *)(p + 8);
        uint64_t pages = *(uint64_t *)(p + 24);

        if (type == 7 /* EfiConventionalMemory */) {
            usable_pages += pages;
        }

        if (i < 5) {
            out->OutputString(out, L"  [");
            print_u64(out, i);
            out->OutputString(out, L"] type=");
            print_u64(out, type);
            out->OutputString(out, L" addr=");
            print_hex(out, phys);
            out->OutputString(out, L" pages=");
            print_u64(out, pages);
            out->OutputString(out, L"\r\n");
        }

        p += desc_size;
    }

    out->OutputString(out, L"usable: ");
    print_u64(out, usable_pages * 4096 / (1024 * 1024));
    out->OutputString(out, L" MB\r\n\r\n");

    out->OutputString(out, L"=== file system ===\r\n");

    EFI_LOADED_IMAGE_PROTOCOL *li = NULL;
    EFI_GUID li_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    status = bs->HandleProtocol(image, &li_guid, (void **)&li);
    if (status != EFI_SUCCESS || !li) {
        out->OutputString(out, L"HandleProtocol(LoadedImage) failed\r\n");
        return 1;
    }

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    status = bs->HandleProtocol(li->DeviceHandle, &fs_guid, (void **)&fs);
    if (status != EFI_SUCCESS || !fs) {
        out->OutputString(out, L"HandleProtocol(SimpleFileSystem) failed\r\n");
        return 1;
    }

    EFI_FILE_PROTOCOL *root = NULL;
    status = fs->OpenVolume(fs, &root);
    if (status != EFI_SUCCESS || !root) {
        out->OutputString(out, L"OpenVolume failed\r\n");
        return 1;
    }

    EFI_FILE_PROTOCOL *kernel = NULL;
    status = root->Open(root, &kernel, L"\\EFI\\BOOT\\kernel.elf", EFI_FILE_MODE_READ, 0);
    if (status != EFI_SUCCESS || !kernel) {
        out->OutputString(out, L"Open(kernel.elf) failed\r\n");
        return 1;
    }

    EFI_GUID info_guid = EFI_FILE_INFO_GUID;
    uint8_t info_buf[256];
    UINTN info_size = sizeof(info_buf);
    status = kernel->GetInfo(kernel, &info_guid, &info_size, info_buf);
    if (status != EFI_SUCCESS) {
        out->OutputString(out, L"GetInfo failed\r\n");
        return 1;
    }

    EFI_FILE_INFO *info = (EFI_FILE_INFO *)info_buf;
    out->OutputString(out, L"kernel.elf size: ");
    print_u64(out, info->FileSize);
    out->OutputString(out, L" bytes\r\n");

    out->OutputString(out, L"\r\n=== 14.2 complete ===\r\n");
    return EFI_SUCCESS;
}