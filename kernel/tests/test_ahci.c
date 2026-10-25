#include "tests.h"
#include "ahci.h"
#include "kmalloc.h"
#include "kprintf.h"
#include "pci.h"

void test_ahci(void) {
    kprintf("\n=== AHCI test ===\n");

    struct pci_device dev;
    if (pci_find_ahci(&dev) != 0) {
        kprintf("  SKIP: no AHCI controller (BIOS boot)\n");
        return;
    }

    kprintf("  AHCI controller at PCI %d:%d.%d\n", (uint64_t)dev.bus, (uint64_t)dev.slot, (uint64_t)dev.func);

    if (ahci_init() != 0) {
        kprintf("  FAIL: AHCI init failed\n");
        return;
    }

    ahci_dump_info();

    uint8_t *sector = kmalloc(512);
    if (!sector) {
        kprintf("  FAIL: kmalloc\n");
        return;
    }

    if (ahci_read_sector(0, 0, sector) != 0) {
        kprintf("  FAIL: read sector 0\n");
        kfree(sector);
        return;
    }

    kprintf("  sector 0: ");
    for (int i = 0; i < 16; i++) kprintf("%x ", (uint64_t)sector[i]);
    kprintf("\n");

    if (sector[510] == 0x55 && sector[511] == 0xAA) {
        kprintf("  PASS: boot signature found (0x55AA)\n");
    } else {
        kprintf("  WARN: no boot signature (sector may be blank)\n");
    }

    kfree(sector);
    kprintf("=== AHCI test done ===\n");
}
