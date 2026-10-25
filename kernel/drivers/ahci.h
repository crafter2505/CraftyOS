#ifndef AHCI_H
#define AHCI_H

#include "types.h"
#include "pci.h"

#define AHCI_CAP      0x00
#define AHCI_GHC      0x04
#define AHCI_IS       0x08
#define AHCI_PI       0x0C
#define AHCI_VS       0x10

#define AHCI_PXCLB    0x00
#define AHCI_PXCLBU   0x04
#define AHCI_PXFB     0x08
#define AHCI_PXFBU    0x0C
#define AHCI_PXIS     0x10
#define AHCI_PXIE     0x14
#define AHCI_PXCMD    0x18
#define AHCI_PXSIG    0x24
#define AHCI_PXTFD    0x20
#define AHCI_PXSSTS   0x28
#define AHCI_PXCI     0x38

#define PXCMD_ST      (1 << 0)
#define PXCMD_SUD     (1 << 1)
#define PXCMD_POD     (1 << 2)
#define PXCMD_FRE     (1 << 4)
#define PXCMD_FR      (1 << 14)
#define PXCMD_CR      (1 << 15)

struct ahci_cmd_header {
    uint16_t flags;
    uint16_t prdtl;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved[4];
} __attribute__((packed));

struct ahci_cmd_fis {
    uint8_t  fis_type;
    uint8_t  pmport_c;
    uint8_t  command;
    uint8_t  featurel;
    uint8_t  lba0;
    uint8_t  lba1;
    uint8_t  lba2;
    uint8_t  device;
    uint8_t  lba3;
    uint8_t  lba4;
    uint8_t  lba5;
    uint8_t  featureh;
    uint8_t  countl;
    uint8_t  counth;
    uint8_t  icc;
    uint8_t  control;
    uint32_t reserved;
} __attribute__((packed));

struct ahci_prdt_entry {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved;
    uint32_t dbc;
} __attribute__((packed));

struct ahci_cmd_table {
    struct ahci_cmd_fis cfis;
    uint8_t  cfis_reserved[44];
    uint8_t  acmd[16];
    uint8_t  reserved[48];
    struct ahci_prdt_entry prdt[1];
} __attribute__((packed));


struct ahci_rx_fis {
    uint8_t dsfis[0x20];
    uint8_t psfis[0x20];
    uint8_t rfis[0x18];
    uint8_t sdbfis[0x08];
    uint8_t ufis[0x40];
    uint8_t reserved[0x60];
} __attribute__((packed));

struct ahci_port {
    uint64_t base;
    uint64_t cmd_tables_phys;
    struct ahci_cmd_header *cmd_list;
    struct ahci_cmd_table *cmd_tables;
    struct ahci_rx_fis *rx_fis;
    int has_drive;
    uint64_t sector_count;
    char model[41];
};

struct ahci_controller {
    struct pci_device pci;
    uint64_t abar;
    uint64_t abar_virt;
    struct ahci_port ports[32];
    int num_ports;
};

int  ahci_init(void);
int  ahci_read_sector(uint8_t port, uint64_t lba, void *buffer);
int ahci_controller_exists(void);
void ahci_dump_info(void);

#endif