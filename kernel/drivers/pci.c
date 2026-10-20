#include "pci.h"
#include "io.h"
#include "kprintf.h"

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (1u << 31)
                    | ((uint32_t)bus  << 16)
                    | ((uint32_t)slot << 11)
                    | ((uint32_t)func << 8)
                    | (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t address = (1u << 31)
                    | ((uint32_t)bus  << 16)
                    | ((uint32_t)slot << 11)
                    | ((uint32_t)func << 8)
                    | (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, address);
    outl(PCI_CONFIG_DATA, val);
}

static uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (uint16_t)(pci_read32(bus, slot, func, offset) >> ((offset & 2) * 8));
}

static void pci_write16(uint8_t bus, uint8_t slot, uint8_t func,
                        uint8_t offset, uint16_t value) {
    uint32_t address = (1u << 31)
                     | ((uint32_t)bus << 16)
                     | ((uint32_t)slot << 11)
                     | ((uint32_t)func << 8)
                     | (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, address);
    outw(PCI_CONFIG_DATA + (offset & 2), value);
}

int pci_enable_busmastering(const struct pci_device *device) {
    if (!device) return -1;

    uint16_t command = pci_read16(device->bus, device->slot, device->func, 0x04);
    command |= (1u << 1) | (1u << 2);
    pci_write16(device->bus, device->slot, device->func, 0x04, command);
    return 0;
}

int pci_find_ahci(struct pci_device *out) {
    if (!out) return -1;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint16_t vendor = pci_read16(bus, slot, func, 0x00);
                if (vendor == 0xFFFF) continue;

                uint32_t class_reg = pci_read32(bus, slot, func, 0x08);
                uint8_t class_code = (class_reg >> 24) & 0xFF;
                uint8_t subclass   = (class_reg >> 16) & 0xFF;
                uint8_t prog_if    = (class_reg >> 8)  & 0xFF;

                if (class_code == PCI_CLASS_MASS_STORAGE &&
                    subclass == PCI_SUBCLASS_SATA &&
                    prog_if == PCI_PROGIF_AHCI) {
                    out->bus = bus;
                    out->slot = slot;
                    out->func = func;
                    out->vendor_id = vendor;
                    out->device_id = pci_read16(bus, slot, func, 0x02);
                    out->class_code = class_code;
                    out->subclass = subclass;
                    out->prog_if = prog_if;

                    uint32_t bar5 = pci_read32(bus, slot, func, 0x24);
                    if ((bar5 & 1) || (bar5 & 6) == 4) return -1;
                    out->bar[5] = bar5 & ~0xFULL;
                    if (out->bar[5] == 0) return -1;
                    return 0;
                }
            }
        }
    }
    return -1;
}