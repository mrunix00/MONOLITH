/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/apic.h>
#include <kernel/arch/pc/asm.h>
#include <kernel/debug.h>
#include <kernel/klibc/memory.h>
#include <kernel/memory/vmm.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_ENABLE (1ULL << 11)
#define IA32_APIC_BASE_BSP (1ULL << 8)
#define APIC_BASE_DEFAULT 0xFEE00000UL

#define LAPIC_REG_ID 0x020
#define LAPIC_REG_EOI 0x0B0
#define LAPIC_REG_SPURIOUS 0x0F0
#define LAPIC_SPURIOUS_ENABLE 0x100
#define LAPIC_SPURIOUS_VECTOR 0xFF

#define IOAPIC_REG_ID 0x00
#define IOAPIC_REG_VER 0x01
#define IOAPIC_REG_REDTBL_BASE 0x10
#define IOAPIC_REDTBL_MASK (1ULL << 16)
#define IOAPIC_REDTBL_LEVEL (1ULL << 15)
#define IOAPIC_REDTBL_LOW_ACTIVE (1ULL << 13)

#define PIC1_DATA 0x21
#define PIC2_DATA 0xA1
#define PIC1_COMMAND 0x20
#define PIC2_COMMAND 0xA0
#define PIC_EOI 0x20

#define ACPI_MADT_SIGNATURE 0x43495041
#define APIC_IRQ_BASE 32

typedef struct
{
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) _rsdp_t;

typedef struct
{
    uint32_t signature;
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) _acpi_sdt_header_t;

typedef struct
{
    _acpi_sdt_header_t header;
    uint32_t lapic_addr;
    uint32_t flags;
} __attribute__((packed)) _madt_t;

typedef struct
{
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) _madt_entry_t;

typedef struct
{
    _madt_entry_t header;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_addr;
    uint32_t gsi_base;
} __attribute__((packed)) _madt_ioapic_t;

typedef struct
{
    _madt_entry_t header;
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t gsi;
    uint16_t flags;
} __attribute__((packed)) _madt_iso_t;

typedef struct
{
    volatile uint32_t *base;
    uint32_t gsi_base;
    uint32_t redirection_count;
} _ioapic_t;

typedef struct
{
    uint32_t gsi;
    uint16_t flags;
    bool overridden;
} _irq_override_t;

static volatile uint32_t *_lapic = NULL;
static _ioapic_t _ioapics[8];
static size_t _ioapic_count = 0;
static _irq_override_t _irq_overrides[16];

static uint8_t _checksum(const void *base, size_t length)
{
    const uint8_t *bytes = base;
    uint8_t sum = 0;
    for (size_t i = 0; i < length; i++)
        sum += bytes[i];
    return sum;
}

static void *_phys_to_hhdm(uint64_t phys)
{
    uint64_t phys_page = phys & ~0xFFFULL;
    uintptr_t virt_page = (uintptr_t) vmm_get_hhdm_addr((void *) phys_page);
    vmm_map(
        vmm_get_kernel_cr3(),
        virt_page,
        phys_page,
        PTFLAG_P | PTFLAG_RW | PTFLAG_PWT | PTFLAG_PCD,
        false);
    return (void *) (virt_page + (phys & 0xFFFULL));
}

static uint32_t _lapic_read(uint32_t reg)
{
    return _lapic[reg / 4];
}

static void _lapic_write(uint32_t reg, uint32_t value)
{
    _lapic[reg / 4] = value;
    (void) _lapic_read(LAPIC_REG_ID);
}

static uint32_t _ioapic_read(_ioapic_t *ioapic, uint8_t reg)
{
    ioapic->base[0] = reg;
    return ioapic->base[4];
}

static void _ioapic_write(_ioapic_t *ioapic, uint8_t reg, uint32_t value)
{
    ioapic->base[0] = reg;
    ioapic->base[4] = value;
}

static void _ioapic_write_redirection(_ioapic_t *ioapic, uint32_t index, uint64_t value)
{
    uint8_t reg = IOAPIC_REG_REDTBL_BASE + index * 2;
    _ioapic_write(ioapic, reg, (uint32_t) value);
    _ioapic_write(ioapic, reg + 1, (uint32_t) (value >> 32));
}

static _ioapic_t *_ioapic_for_gsi(uint32_t gsi)
{
    for (size_t i = 0; i < _ioapic_count; i++) {
        uint32_t first = _ioapics[i].gsi_base;
        uint32_t last = first + _ioapics[i].redirection_count;
        if (gsi >= first && gsi <= last)
            return &_ioapics[i];
    }
    return NULL;
}

void apic_set_irq_mask(uint8_t irq, bool masked)
{
    if (irq >= 16)
        return;

    uint32_t gsi = irq;
    uint16_t flags = 0;
    if (_irq_overrides[irq].overridden) {
        gsi = _irq_overrides[irq].gsi;
        flags = _irq_overrides[irq].flags;
    }

    _ioapic_t *ioapic = _ioapic_for_gsi(gsi);
    if (ioapic == NULL)
        return;

    uint64_t entry = APIC_IRQ_BASE + irq;
    if (masked)
        entry |= IOAPIC_REDTBL_MASK;

    /* MADT flags: polarity bits 0-1, trigger mode bits 2-3. */
    if ((flags & 0x3) == 0x3)
        entry |= IOAPIC_REDTBL_LOW_ACTIVE;
    if ((flags & 0xC) == 0xC)
        entry |= IOAPIC_REDTBL_LEVEL;

    _ioapic_write_redirection(ioapic, gsi - ioapic->gsi_base, entry);
}

static _acpi_sdt_header_t *_find_madt(_rsdp_t *rsdp)
{
    _acpi_sdt_header_t *root;
    bool xsdt = rsdp->revision >= 2 && rsdp->xsdt_address != 0;

    if (xsdt) {
        root = _phys_to_hhdm(rsdp->xsdt_address);
        if (_checksum(root, root->length) != 0)
            return NULL;

        uint64_t *entries = (uint64_t *) (root + 1);
        size_t count = (root->length - sizeof(_acpi_sdt_header_t)) / sizeof(uint64_t);
        for (size_t i = 0; i < count; i++) {
            _acpi_sdt_header_t *table = _phys_to_hhdm(entries[i]);
            if (_checksum(table, table->length) == 0 && table->signature == ACPI_MADT_SIGNATURE)
                return table;
        }
    } else {
        root = _phys_to_hhdm(rsdp->rsdt_address);
        if (_checksum(root, root->length) != 0)
            return NULL;

        uint32_t *entries = (uint32_t *) (root + 1);
        size_t count = (root->length - sizeof(_acpi_sdt_header_t)) / sizeof(uint32_t);
        for (size_t i = 0; i < count; i++) {
            _acpi_sdt_header_t *table = _phys_to_hhdm(entries[i]);
            if (_checksum(table, table->length) == 0 && table->signature == ACPI_MADT_SIGNATURE)
                return table;
        }
    }

    return NULL;
}

static void _parse_madt(_madt_t *madt, uint64_t *lapic_phys)
{
    *lapic_phys = madt->lapic_addr;

    uint8_t *entry = (uint8_t *) (madt + 1);
    uint8_t *end = (uint8_t *) madt + madt->header.length;
    while (entry < end) {
        _madt_entry_t *header = (_madt_entry_t *) entry;
        if (header->length == 0)
            break;

        switch (header->type) {
        case 1: { /* I/O APIC */
            if (_ioapic_count >= sizeof(_ioapics) / sizeof(_ioapics[0]))
                break;
            _madt_ioapic_t *madt_ioapic = (_madt_ioapic_t *) entry;
            _ioapic_t *ioapic = &_ioapics[_ioapic_count++];
            ioapic->base = _phys_to_hhdm(madt_ioapic->ioapic_addr);
            ioapic->gsi_base = madt_ioapic->gsi_base;
            ioapic->redirection_count = ((_ioapic_read(ioapic, IOAPIC_REG_VER) >> 16) & 0xFF);
        } break;
        case 2: { /* Interrupt Source Override */
            _madt_iso_t *iso = (_madt_iso_t *) entry;
            if (iso->bus_source == 0 && iso->irq_source < 16) {
                _irq_overrides[iso->irq_source].gsi = iso->gsi;
                _irq_overrides[iso->irq_source].flags = iso->flags;
                _irq_overrides[iso->irq_source].overridden = true;
            }
        } break;
        case 5: { /* Local APIC Address Override */
            uint64_t *addr = (uint64_t *) (entry + sizeof(_madt_entry_t) + sizeof(uint16_t));
            *lapic_phys = *addr;
        } break;
        default:
            break;
        }

        entry += header->length;
    }
}

bool _is_apic_supported()
{
    uint32_t eax, ebx, ecx, edx;
    asm_cpuid(&eax, &ebx, &ecx, &edx);
    return (edx & (1 << 9)) != 0;
}

void apic_init(void *rsdp_addr)
{
    debug_log("Initializing the APIC...\n");

    if (rsdp_addr == NULL)
        return debug_log("No ACPI RSDP provided; APIC unavailable\n");

    if (!_is_apic_supported())
        return debug_log("APIC is not supported on this CPU\n");

    _rsdp_t *rsdp = rsdp_addr;
    if (_checksum(rsdp, 20) != 0)
        return debug_log("Invalid RSDP checksum; APIC unavailable\n");

    if (rsdp->revision >= 2 && _checksum(rsdp, rsdp->length) != 0)
        return debug_log("Invalid extended RSDP checksum; APIC unavailable\n");

    _acpi_sdt_header_t *madt_header = _find_madt(rsdp);
    if (madt_header == NULL)
        return debug_log("MADT not found; APIC unavailable\n");

    uint64_t lapic_phys = APIC_BASE_DEFAULT;
    _parse_madt((_madt_t *) madt_header, &lapic_phys);

    uint64_t apic_base_msr = asm_read_msr(IA32_APIC_BASE_MSR);
    uint64_t apic_base = apic_base_msr & 0xFFFFF000;
    if (apic_base == 0)
        apic_base = lapic_phys;
    _lapic = _phys_to_hhdm(apic_base);

    asm_write_msr(
        IA32_APIC_BASE_MSR,
        apic_base | IA32_APIC_BASE_ENABLE | (apic_base_msr & IA32_APIC_BASE_BSP));
    _lapic_write(
        LAPIC_REG_SPURIOUS,
        _lapic_read(LAPIC_REG_SPURIOUS) | LAPIC_SPURIOUS_ENABLE | LAPIC_SPURIOUS_VECTOR);

    for (size_t i = 0; i < _ioapic_count; i++) {
        for (uint32_t redir = 0; redir <= _ioapics[i].redirection_count; redir++)
            _ioapic_write_redirection(&_ioapics[i], redir, IOAPIC_REDTBL_MASK);
    }

    debug_log("APIC initialized\n");
}

void apic_eoi()
{
    if (_lapic != NULL)
        _lapic_write(LAPIC_REG_EOI, 0);
}

bool apic_is_initialized()
{
    return _lapic != NULL;
}
