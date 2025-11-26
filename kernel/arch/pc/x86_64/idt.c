/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/asm.h>
#include <kernel/arch/pc/idt.h>
#include <kernel/arch/pc/morse_debug.h>
#include <kernel/debug.h>
#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/video/panic.h>

/*
 * IDT Gate Descriptor
 * https://wiki.osdev.org/Interrupt_Descriptor_Table#Example_Code_2
 */
typedef struct
{
    uint16_t offset0;
    uint16_t selector;
    uint8_t ist;
    uint8_t flags;
    uint16_t offset1;
    uint32_t offset2;
    uint32_t zero;
} __attribute__((packed)) idt_entry_t;

/*
 * IDTR
 * https://wiki.osdev.org/Interrupt_Descriptor_Table#IDTR
 */
static struct
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) _idtr;

/*
 * Interrupt Descriptor Table
 * https://wiki.osdev.org/Interrupt_Descriptor_Table#Table_2
 */
static idt_entry_t _idt_entries[256];

static void *_irq_routines[16] = {NULL};

extern void _isr0();
extern void _isr1();
extern void _isr2();
extern void _isr3();
extern void _isr4();
extern void _isr5();
extern void _isr6();
extern void _isr7();
extern void _isr8();
extern void _isr9();
extern void _isr10();
extern void _isr11();
extern void _isr12();
extern void _isr13();
extern void _isr14();
extern void _isr15();
extern void _isr16();
extern void _isr17();
extern void _isr18();
extern void _isr19();
extern void _isr20();
extern void _isr21();
extern void _isr22();
extern void _isr23();
extern void _isr24();
extern void _isr25();
extern void _isr26();
extern void _isr27();
extern void _isr28();
extern void _isr29();
extern void _isr30();
extern void _isr31();

extern void _irq0();
extern void _irq1();
extern void _irq2();
extern void _irq3();
extern void _irq4();
extern void _irq5();
extern void _irq6();
extern void _irq7();
extern void _irq8();
extern void _irq9();
extern void _irq10();
extern void _irq11();
extern void _irq12();
extern void _irq13();
extern void _irq14();
extern void _irq15();

void idt_init()
{
    debug_log("[*] Initializing the IDT...\n");

    _idtr.limit = sizeof(_idt_entries) - 1;
    _idtr.base = (uint64_t) &_idt_entries;
    memset(&_idt_entries, 0, sizeof(_idt_entries));

    // Remap the PIC
    asm_outb(0x11, 0x20);
    asm_outb(0x11, 0xA0);
    asm_outb(0x20, 0x21);
    asm_outb(0x28, 0xA1);
    asm_outb(0x04, 0x21);
    asm_outb(0x02, 0xA1);
    asm_outb(0x01, 0x21);
    asm_outb(0x01, 0xA1);
    asm_outb(0x00, 0x21);
    asm_outb(0x00, 0xA1);

    idt_set_gate(0, (void *) _isr0, IDT_TYPE_INTERRUPT);
    idt_set_gate(1, (void *) _isr1, IDT_TYPE_INTERRUPT);
    idt_set_gate(2, (void *) _isr2, IDT_TYPE_INTERRUPT);
    idt_set_gate(3, (void *) _isr3, IDT_TYPE_INTERRUPT);
    idt_set_gate(4, (void *) _isr4, IDT_TYPE_INTERRUPT);
    idt_set_gate(5, (void *) _isr5, IDT_TYPE_INTERRUPT);
    idt_set_gate(6, (void *) _isr6, IDT_TYPE_INTERRUPT);
    idt_set_gate(7, (void *) _isr7, IDT_TYPE_INTERRUPT);
    idt_set_gate(8, (void *) _isr8, IDT_TYPE_INTERRUPT);
    idt_set_gate(9, (void *) _isr9, IDT_TYPE_INTERRUPT);
    idt_set_gate(10, (void *) _isr10, IDT_TYPE_INTERRUPT);
    idt_set_gate(11, (void *) _isr11, IDT_TYPE_INTERRUPT);
    idt_set_gate(12, (void *) _isr12, IDT_TYPE_INTERRUPT);
    idt_set_gate(13, (void *) _isr13, IDT_TYPE_INTERRUPT);
    idt_set_gate(14, (void *) _isr14, IDT_TYPE_INTERRUPT);
    idt_set_gate(15, (void *) _isr15, IDT_TYPE_INTERRUPT);
    idt_set_gate(16, (void *) _isr16, IDT_TYPE_INTERRUPT);
    idt_set_gate(17, (void *) _isr17, IDT_TYPE_INTERRUPT);
    idt_set_gate(18, (void *) _isr18, IDT_TYPE_INTERRUPT);
    idt_set_gate(19, (void *) _isr19, IDT_TYPE_INTERRUPT);
    idt_set_gate(20, (void *) _isr20, IDT_TYPE_INTERRUPT);
    idt_set_gate(21, (void *) _isr21, IDT_TYPE_INTERRUPT);
    idt_set_gate(22, (void *) _isr22, IDT_TYPE_INTERRUPT);
    idt_set_gate(23, (void *) _isr23, IDT_TYPE_INTERRUPT);
    idt_set_gate(24, (void *) _isr24, IDT_TYPE_INTERRUPT);
    idt_set_gate(25, (void *) _isr25, IDT_TYPE_INTERRUPT);
    idt_set_gate(26, (void *) _isr26, IDT_TYPE_INTERRUPT);
    idt_set_gate(27, (void *) _isr27, IDT_TYPE_INTERRUPT);
    idt_set_gate(28, (void *) _isr28, IDT_TYPE_INTERRUPT);
    idt_set_gate(29, (void *) _isr29, IDT_TYPE_INTERRUPT);
    idt_set_gate(30, (void *) _isr30, IDT_TYPE_INTERRUPT);
    idt_set_gate(31, (void *) _isr31, IDT_TYPE_INTERRUPT);

    idt_set_gate(32, (void *) _irq0, IDT_TYPE_INTERRUPT);
    idt_set_gate(33, (void *) _irq1, IDT_TYPE_INTERRUPT);
    idt_set_gate(34, (void *) _irq2, IDT_TYPE_INTERRUPT);
    idt_set_gate(35, (void *) _irq3, IDT_TYPE_INTERRUPT);
    idt_set_gate(36, (void *) _irq4, IDT_TYPE_INTERRUPT);
    idt_set_gate(37, (void *) _irq5, IDT_TYPE_INTERRUPT);
    idt_set_gate(38, (void *) _irq6, IDT_TYPE_INTERRUPT);
    idt_set_gate(39, (void *) _irq7, IDT_TYPE_INTERRUPT);
    idt_set_gate(40, (void *) _irq8, IDT_TYPE_INTERRUPT);
    idt_set_gate(41, (void *) _irq9, IDT_TYPE_INTERRUPT);
    idt_set_gate(42, (void *) _irq10, IDT_TYPE_INTERRUPT);
    idt_set_gate(43, (void *) _irq11, IDT_TYPE_INTERRUPT);
    idt_set_gate(44, (void *) _irq12, IDT_TYPE_INTERRUPT);
    idt_set_gate(45, (void *) _irq13, IDT_TYPE_INTERRUPT);
    idt_set_gate(46, (void *) _irq14, IDT_TYPE_INTERRUPT);
    idt_set_gate(47, (void *) _irq15, IDT_TYPE_INTERRUPT);

    debug_log("[*] Flushing the IDT...\n");
    idt_flush();
    debug_log("[+] IDT initialized\n");
}

void idt_set_gate(uint8_t num, void *handler, uint8_t flags)
{
    uint64_t address = (uint64_t) handler;
    _idt_entries[num].offset0 = (uint16_t) address;
    _idt_entries[num].selector = 0x08;
    _idt_entries[num].ist = 0;
    _idt_entries[num].flags = flags;
    _idt_entries[num].offset1 = (uint16_t) (address >> 16);
    _idt_entries[num].offset2 = (uint32_t) (address >> 32);
    _idt_entries[num].zero = 0;
}

void idt_flush()
{
    __asm__ volatile("lidtq %0" : : "m"(_idtr));
    __asm__ volatile("sti");
}

static const char *error_messages[] = {
    "Divide by zero",
    "Debug",
    "NMI",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double fault",
    "Co-processor Segment Overrun",
    "Invalid TSS",
    "Segment not present",
    "Stack-Segment Fault",
    "GPF",
    "Page Fault",
    "Reserved",
    "x87 Floating Point Exception",
    "alignment check",
    "Machine check",
    "SIMD floating-point exception",
    "Virtualization Exception",
    "Deadlock",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Security Exception",
    "Reserved",
    "Triple Fault",
    "FPU error",
};

void isr_handler(struct interrupt_registers *regs)
{
    if (regs->isr_number < 32) {
        uint8_t tmp = asm_inb(0x61);
        if (tmp != (tmp | 3)) {
            asm_outb(tmp | 3, 0x61);
        }
        debug_log_fmt("[-] System panic!\n");
        switch (regs->isr_number) {
        case 14: { /* Page Fault */
            uintptr_t address = asm_read_cr2();
            char buffer[128];
            strcpy(buffer, "Page fault at 0x");
            size_t index = strlen("Page fault at 0x");
            itohex(address, buffer + index);
            debug_log_fmt("[-] %s\n", buffer);
            panic(buffer, regs);
            morse_log(buffer);
        } break;
        case 13: { /* General Protection Fault */
            uintptr_t address = regs->rip;
            uintptr_t rax = regs->rax;
            uintptr_t rdx = regs->rdx;
            uintptr_t rcx = regs->rcx;
            char buffer[128];
            strcpy(buffer, "General Protection Fault at 0x");
            size_t index = strlen("General Protection Fault at 0x");
            itohex(address, buffer + index);
            debug_log_fmt(
                "[*] %s\n[*] rax: 0x%x\n[*] rdx: 0x%x\n[*] rcx: 0x%x\n", buffer, rax, rdx, rcx);
            panic(buffer, regs);
            morse_log(buffer);
        } break;
        default:
            panic(error_messages[regs->isr_number], regs);
            morse_log(error_messages[regs->isr_number]);
            debug_log_fmt("[-] Error: %s\n", error_messages[regs->isr_number]);
        }
        while (1)
            __asm__("hlt");
    }
}

void irq_register_handler(const uint8_t irq, void *handler)
{
    _irq_routines[irq] = handler;
}

void irq_unregister_handler(const uint8_t irq)
{
    _irq_routines[irq] = NULL;
}

void irq_handler(struct interrupt_registers *reg)
{
    void (*handler)(struct interrupt_registers *) = _irq_routines[reg->isr_number - 32];
    if (handler)
        handler(reg);
    if (reg->isr_number >= 40)
        asm_outb(0x20, 0xA0);
    asm_outb(0x20, 0x20);
}
