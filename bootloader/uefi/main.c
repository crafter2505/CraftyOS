#include "efi.h"

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
    (void)image;
    if (!st || !st->ConOut || !st->BootServices ||
        !st->BootServices->SetWatchdogTimer ||
        !st->ConOut->ClearScreen || !st->ConOut->OutputString) {
        return EFI_INVALID_PARAMETER;
    }

    EFI_STATUS status = st->BootServices->SetWatchdogTimer(0, 0, 0, NULL);
    if (EFI_ERROR(status)) {
        return status;
    }

    status = st->ConOut->ClearScreen(st->ConOut);
    if (EFI_ERROR(status)) {
        return status;
    }

    status = st->ConOut->OutputString(
        st->ConOut, (CHAR16 *)L"craftyOS UEFI boot\r\n");
    if (EFI_ERROR(status)) {
        return status;
    }

    status = st->ConOut->OutputString(
        st->ConOut, (CHAR16 *)L"toolchain works\r\n");
    if (EFI_ERROR(status)) {
        return status;
    }

    status = st->ConOut->OutputString(
        st->ConOut, (CHAR16 *)L"full EFI headers in place\r\n");
    return status;
}
