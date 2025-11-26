/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/gdt.h>
#include <kernel/arch/pc/idt.h>
#include <kernel/arch/pc/sse.h>
#include <kernel/debug.h>
#include <kernel/fs/initrdfs.h>
#include <kernel/fs/tmpfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/input/ps2_keyboard.h>
#include <kernel/input/ps2_mouse.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>
#include <kernel/serial.h>
#include <kernel/terminal/terminal.h>
#include <kernel/timer.h>
#include <kernel/usermode/syscall.h>
#include <kernel/usermode/task.h>
#include <libs/flanterm/src/flanterm_backends/fb.h>
#include <libs/limine-protocol/include/limine.h>

__attribute__((used, section(".limine_requests"))) static volatile uint64_t limine_base_revision[]
    = LIMINE_BASE_REVISION(4);

__attribute__((used, section(".limine_requests_start"))) static volatile uint64_t
    limine_requests_start_marker[]
    = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests"))) volatile struct limine_framebuffer_request
    framebuffer_request
    = {.id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 0};

__attribute__((used, section(".limine_requests"))) volatile struct limine_memmap_request
    limine_mmap_request
    = {.id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0};

__attribute__((used, section(".limine_requests"))) volatile struct limine_module_request
    limine_module_request
    = {.id = LIMINE_MODULE_REQUEST_ID, .revision = 0};

__attribute__((used, section(".limine_requests_end"))) static volatile uint64_t
    limine_requests_end_marker[]
    = LIMINE_REQUESTS_END_MARKER;

struct flanterm_context *_fb_ctx;

void kmain()
{
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        while (1)
            __asm__("hlt");
    }
    sse_init();

    start_debug_serial(SERIAL_COM1);
    start_debug_console(framebuffer_request.response);

    gdt_init();
    idt_init();
    pmm_init(limine_mmap_request.response);
    vmm_init(limine_mmap_request.response);
    heap_init(10);
    timer_init();
    syscalls_init();
    task_switching_init();
    initrd_load_modules(limine_module_request.response);
    tmpfs_new_drive("tmpfs");

    ps2_init_keyboard();
    ps2_mouse_init();

    stop_debug_console();
    term_init(framebuffer_request.response);

    while (1)
        __asm__("hlt");
}
