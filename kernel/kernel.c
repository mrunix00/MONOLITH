/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/apic.h>
#include <kernel/arch/pc/asm.h>
#include <kernel/arch/pc/gdt.h>
#include <kernel/arch/pc/idt.h>
#include <kernel/arch/pc/sse.h>
#include <kernel/debug.h>
#include <kernel/fs/tmpfs.h>
#include <kernel/fs/ustar.h>
#include <kernel/fs/vfs.h>
#include <kernel/input/input_events.h>
#include <kernel/input/ps2_keyboard.h>
#include <kernel/input/ps2_mouse.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>
#include <kernel/serial.h>
#include <kernel/tasking/loader.h>
#include <kernel/tasking/scheduler.h>
#include <kernel/tasking/syscall.h>
#include <kernel/timer.h>
#include <libs/flanterm/src/flanterm_backends/fb.h>
#include <libs/limine-protocol/include/limine.h>

__attribute__((used, section(".limine_requests"))) static volatile uint64_t limine_base_revision[]
    = LIMINE_BASE_REVISION(4);

__attribute__((used, section(".limine_requests_start"))) static volatile uint64_t
    limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests"))) volatile struct limine_framebuffer_request
    framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 0};

__attribute__((used, section(".limine_requests"))) volatile struct limine_memmap_request
    limine_mmap_request = {.id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0};

__attribute__((used, section(".limine_requests"))) volatile struct limine_module_request
    limine_module_request = {.id = LIMINE_MODULE_REQUEST_ID, .revision = 0};

__attribute__((used, section(".limine_requests"))) volatile struct limine_rsdp_request
    limine_rsdp_request = {.id = LIMINE_RSDP_REQUEST_ID, .revision = 0};

__attribute__((used, section(".limine_requests_end"))) static volatile uint64_t
    limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

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
    apic_init(limine_rsdp_request.response->address);
    timer_init();
    asm_sti();
    heap_init(10);
    syscalls_init();
    task_switching_init();

    /* Create tmpfs drive */
    vfs_drive_t *tmpfs_drive = tmpfs_new_drive("system");

    /* Optionally populate tmpfs with initrd contents if available */
    if (limine_module_request.response != NULL && limine_module_request.response->module_count > 0) {
        debug_log("Loading initrd into tmpfs...\n");
        for (uint64_t i = 0; i < limine_module_request.response->module_count; i++) {
            struct limine_file *module = limine_module_request.response->modules[i];
            if (tmpfs_populate_from_initrd(tmpfs_drive, (void *) module->address) == 0) {
                debug_log_fmt("Loaded \"%s\" into tmpfs\n", module->path);
            } else {
                debug_log_fmt("Failed to load \"%s\" into tmpfs\n", module->path);
            }
        }
    }

    input_events_init();
    ps2_init_keyboard();
    ps2_mouse_init();
    asm_sti();

    debug_log("Launching desktop...\n");
    task_t *task = load_elf("system:/desktop");
    if (task == NULL) {
        debug_log("Failed to load desktop ELF\n");
        while (1)
            asm_hlt();
    }

    stop_debug_console();
    scheduler_init();

    task_get_current()->quantum = 0; /* De-prioritize the kernel task */
    while (1)
        asm_hlt();
}
