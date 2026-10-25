#include "ahci.h"
#include "pmm.h"
#include "buddy.h"
#include "vmm.h"
#include "kmalloc.h"
#include "kprintf.h"
#include "address_space.h"

#define AHCI_MMIO_VIRT 0xFFFFC00000000000ULL
#define AHCI_MMIO_SIZE 0x2000ULL

static struct ahci_controller ahci;
static int ahci_initialized;

int ahci_controller_exists(void) {
    struct pci_device dev;
    return pci_find_ahci(&dev) == 0 ? 1 : 0;
}

static inline uint32_t ahci_read(uint32_t reg) {
    return *(volatile uint32_t *)(ahci.abar_virt + reg);
}

static inline void ahci_write(uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(ahci.abar_virt + reg) = val;
}

static uint64_t port_base(int port) {
    return ahci.abar_virt + 0x100 + (uint64_t)port * 0x80;
}

static uint32_t port_read(int port, uint32_t reg) {
    return *(volatile uint32_t *)(port_base(port) + reg);
}

static void port_write(int port, uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(port_base(port) + reg) = val;
}

static int port_stop(int port) {
    uint32_t cmd = port_read(port, AHCI_PXCMD);
    cmd &= ~PXCMD_ST;
    port_write(port, AHCI_PXCMD, cmd);
    uint32_t timeout = 1000000;
    while ((port_read(port, AHCI_PXCMD) & PXCMD_CR) && --timeout);
    if (!timeout) return -1;

    cmd = port_read(port, AHCI_PXCMD);
    cmd &= ~PXCMD_FRE;
    port_write(port, AHCI_PXCMD, cmd);
    timeout = 1000000;
    while ((port_read(port, AHCI_PXCMD) & PXCMD_FR) && --timeout);
    return timeout ? 0 : -1;
}

static void port_start(int port) {
    uint32_t cmd = port_read(port, AHCI_PXCMD);
    cmd |= PXCMD_FRE;
    port_write(port, AHCI_PXCMD, cmd);
    cmd |= PXCMD_ST;
    port_write(port, AHCI_PXCMD, cmd);
}

static void *alloc_dma_page(void) {
    return alloc_pages(0);
}

static void setup_port(int port) {
    struct ahci_port *p = &ahci.ports[port];
    p->base = port_base(port);
    if (port_stop(port) != 0) {
        kprintf("  AHCI: port %d did not stop\n", (uint64_t)port);
        return;
    }

    void *cmd_list_phys = alloc_dma_page();
    void *cmd_tables_phys = alloc_dma_page();
    void *rx_fis_phys = alloc_dma_page();

    if (!cmd_list_phys || !cmd_tables_phys || !rx_fis_phys) {
        kprintf("  AHCI: port %d allocation failed\n", (uint64_t)port);
        if (cmd_list_phys) free_pages(cmd_list_phys, 0);
        if (cmd_tables_phys) free_pages(cmd_tables_phys, 0);
        if (rx_fis_phys) free_pages(rx_fis_phys, 0);
        return;
    }

    p->cmd_list   = (struct ahci_cmd_header *)phys_to_virt((uint64_t)cmd_list_phys);
    p->cmd_tables = (struct ahci_cmd_table *)phys_to_virt((uint64_t)cmd_tables_phys);
    p->rx_fis     = (struct ahci_rx_fis *)phys_to_virt((uint64_t)rx_fis_phys);
    p->cmd_tables_phys = (uint64_t)cmd_tables_phys;

    for (int i = 0; i < 1024; i++) ((uint8_t *)p->cmd_list)[i] = 0;
    for (int i = 0; i < 4096; i++) ((uint8_t *)p->cmd_tables)[i] = 0;
    for (int i = 0; i < 256;  i++) ((uint8_t *)p->rx_fis)[i] = 0;

    port_write(port, AHCI_PXCLB,  (uint32_t)(uint64_t)cmd_list_phys);
    port_write(port, AHCI_PXCLBU, (uint32_t)((uint64_t)cmd_list_phys >> 32));
    port_write(port, AHCI_PXFB,   (uint32_t)(uint64_t)rx_fis_phys);
    port_write(port, AHCI_PXFBU,  (uint32_t)((uint64_t)rx_fis_phys >> 32));

    port_write(port, AHCI_PXIS, 0xFFFFFFFF);

    port_start(port);

    p->has_drive = 1;
    kprintf("  AHCI: port %d initialized\n", (uint64_t)port);
}

static int ahci_send_cmd(int port, struct ahci_cmd_fis *fis,
                         void *buffer, uint32_t byte_count, int is_write) {
    struct ahci_port *p = &ahci.ports[port];

    int timeout = 100000;
    while ((port_read(port, AHCI_PXCI) & 1) && --timeout);
    if (timeout <= 0) return -1;
    timeout = 1000000;
    while ((port_read(port, AHCI_PXTFD) & 0x88) && --timeout);
    if (timeout <= 0) return -1;

    struct ahci_cmd_header *hdr = &p->cmd_list[0];
    hdr->flags = (sizeof(struct ahci_cmd_fis) / 4) & 0x1F;
    if (is_write) hdr->flags |= (1 << 6);
    hdr->prdtl = byte_count ? 1 : 0;
    hdr->prdbc = 0;
    hdr->ctba  = (uint32_t)p->cmd_tables_phys;
    hdr->ctbau = (uint32_t)(p->cmd_tables_phys >> 32);

    struct ahci_cmd_table *tbl = &p->cmd_tables[0];
    for (int i = 0; i < (int)sizeof(struct ahci_cmd_fis); i++)
        ((uint8_t *)&tbl->cfis)[i] = ((uint8_t *)fis)[i];

    if (byte_count) {
        tbl->prdt[0].dba  = (uint32_t)(uint64_t)buffer;
        tbl->prdt[0].dbau = (uint32_t)((uint64_t)buffer >> 32);
        tbl->prdt[0].dbc  = (byte_count - 1) | (1u << 31);
    }

    port_write(port, AHCI_PXCI, 1);

    timeout = 1000000;
    while ((port_read(port, AHCI_PXCI) & 1) && --timeout);
    if (timeout <= 0) return -1;

    uint32_t tfd = port_read(port, AHCI_PXTFD);
    if (tfd & 0x01) return -1;

    return 0;
}

static void ahci_identify(int port) {
    struct ahci_port *p = &ahci.ports[port];

    void *buf_phys = alloc_dma_page();
    if (!buf_phys) return;
    void *buf = phys_to_virt((uint64_t)buf_phys);
    for (int i = 0; i < 512; i++) ((uint8_t *)buf)[i] = 0;

    struct ahci_cmd_fis fis = {0};
    fis.fis_type = 0x27;
    fis.pmport_c = 1 << 7;
    fis.command  = 0xEC;

    if (ahci_send_cmd(port, &fis, buf_phys, 512, 0) != 0) {
        kprintf("  AHCI: port %d IDENTIFY failed\n", (uint64_t)port);
        free_pages(buf_phys, 0);
        return;
    }

    uint16_t *id = (uint16_t *)buf;

    for (int i = 0; i < 20; i++) {
        p->model[i*2]   = (char)(id[27 + i] >> 8);
        p->model[i*2+1] = (char)(id[27 + i] & 0xFF);
    }
    p->model[40] = 0;

    for (int i = 39; i >= 0 && p->model[i] == ' '; i--) p->model[i] = 0;

    uint32_t lba28 = ((uint32_t)id[61] << 16) | id[60];
    uint32_t lba48_lo = ((uint32_t)id[101] << 16) | id[100];
    uint32_t lba48_hi = ((uint32_t)id[103] << 16) | id[102];
    uint64_t lba48 = ((uint64_t)lba48_hi << 32) | lba48_lo;

    p->sector_count = lba48 ? lba48 : lba28;

    kprintf("  AHCI: port %d model='%s' sectors=%d\n",
            (uint64_t)port, p->model, p->sector_count);
    free_pages(buf_phys, 0);
}

int ahci_read_sector(uint8_t port, uint64_t lba, void *buffer) {
    if (port >= 32 || !buffer) return -1;
    struct ahci_port *p = &ahci.ports[port];
    if (!p->has_drive) return -1;
    uint64_t buffer_phys = vmm_translate((uint64_t)buffer);
    if (!buffer_phys) return -1;

    struct ahci_cmd_fis fis = {0};
    fis.fis_type = 0x27;
    fis.pmport_c = 1 << 7;
    fis.command  = 0x25;
    fis.device   = 1 << 6;
    fis.lba0 = (uint8_t)(lba);
    fis.lba1 = (uint8_t)(lba >> 8);
    fis.lba2 = (uint8_t)(lba >> 16);
    fis.lba3 = (uint8_t)(lba >> 24);
    fis.lba4 = (uint8_t)(lba >> 32);
    fis.lba5 = (uint8_t)(lba >> 40);
    fis.countl = 1;

    return ahci_send_cmd(port, &fis, (void *)buffer_phys, 512, 0);
}

int ahci_init(void) {
    if (ahci_initialized) return 0;

    if (pci_find_ahci(&ahci.pci) != 0) {
        kprintf("  AHCI: no controller found\n");
        return -1;
    }
    if (pci_enable_busmastering(&ahci.pci) != 0) {
        kprintf("  AHCI: unable to enable PCI memory/bus mastering\n");
        return -1;
    }

    kprintf("  AHCI: controller at PCI %d:%d.%d\n",
            (uint64_t)ahci.pci.bus, (uint64_t)ahci.pci.slot, (uint64_t)ahci.pci.func);

    ahci.abar = ahci.pci.bar[5];
    kprintf("  AHCI: ABAR = 0x%x\n", (uint64_t)ahci.abar);

    uint64_t abar_page = ahci.abar & ~(VMM_PAGE_SIZE - 1);
    uint64_t abar_offset = ahci.abar - abar_page;
    uint64_t map_size = (abar_offset + AHCI_MMIO_SIZE + VMM_PAGE_SIZE - 1) &
                        ~(VMM_PAGE_SIZE - 1);
    for (uint64_t offset = 0; offset < map_size; offset += VMM_PAGE_SIZE) {
        if (vmm_map(AHCI_MMIO_VIRT + offset, abar_page + offset,
                    VMM_KERNEL_RW | VMM_WRITE_THROUGH | VMM_CACHE_DISABLE) != 0) {
            while (offset != 0) {
                offset -= VMM_PAGE_SIZE;
                vmm_unmap(AHCI_MMIO_VIRT + offset);
            }
            kprintf("  AHCI: unable to map controller registers\n");
            return -1;
        }
    }
    ahci.abar_virt = AHCI_MMIO_VIRT + abar_offset;

    uint32_t ghc = ahci_read(AHCI_GHC);
    ghc = (ghc | (1u << 31)) & ~(1u << 1);
    ahci_write(AHCI_GHC, ghc);

    uint32_t pi = ahci_read(AHCI_PI);
    ahci.num_ports = 0;
    kprintf("  AHCI: cap=0x%x implemented ports=0x%x\n",
            (uint64_t)ahci_read(AHCI_CAP), (uint64_t)pi);

    for (int port = 0; port < 32; port++) {
        if (!(pi & (1u << port))) continue;

        uint32_t cmd = port_read(port, AHCI_PXCMD);
        port_write(port, AHCI_PXCMD, cmd | PXCMD_SUD | PXCMD_POD);
        uint32_t timeout = 1000000;
        uint32_t ssts;
        do {
            ssts = port_read(port, AHCI_PXSSTS);
        } while ((ssts & 0xF) != 3 && --timeout);
        if ((ssts & 0xF) != 3) {
            kprintf("  AHCI: port %d link status=0x%x signature=0x%x\n",
                    (uint64_t)port, (uint64_t)ssts,
                    (uint64_t)port_read(port, AHCI_PXSIG));
            continue;
        }

        setup_port(port);
        if (!ahci.ports[port].has_drive) continue;
        ahci.num_ports++;
        ahci_identify(port);
    }

    if (ahci.num_ports == 0) {
        kprintf("  AHCI: no drives found\n");
        return -1;
    }

    ahci_initialized = 1;
    kprintf("  AHCI: %d drive(s) ready\n", (uint64_t)ahci.num_ports);
    return 0;
}

void ahci_dump_info(void) {
    kprintf("  AHCI controller: ABAR=0x%x, ports=%d\n",
            ahci.abar, (uint64_t)ahci.num_ports);
    for (int i = 0; i < 32; i++) {
        if (!ahci.ports[i].has_drive) continue;
        kprintf("    port %d: '%s' sectors=%d\n",
                (uint64_t)i, ahci.ports[i].model, ahci.ports[i].sector_count);
    }
}