/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/apic.h>
#include <kernel/arch/pc/asm.h>
#include <kernel/arch/pc/gdt.h>
#include <kernel/arch/pc/idt.h>
#include <kernel/arch/pc/pic.h>
#include <kernel/arch/pc/sse.h>
#include <kernel/devices/debug.h>
#include <kernel/devices/device_domain.h>
#include <kernel/devices/framebuffer.h>
#include <kernel/devices/input/input_device.h>
#include <kernel/fs/tmpfs.h>
#include <kernel/fs/ustar.h>
#include <kernel/kargs.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/shm.h>
#include <kernel/memory/vmm.h>
#include <kernel/mmap.h>
#include <kernel/tasking/ipc.h>
#include <kernel/tasking/loader.h>
#include <kernel/tasking/pipe.h>
#include <kernel/tasking/scheduler.h>
#include <kernel/tasking/syscall.h>
#include <kernel/tasking/task_domain.h>
#include <kernel/timer.h>
#include <kernel/types.h>
#include <libs/limine-protocol/include/limine.h>

struct flanterm_context *_fb_ctx;

__attribute__((used, section(".limine_requests"))) static volatile uint64_t limine_base_revision[]
    = LIMINE_BASE_REVISION(4);

__attribute__((used, section(".limine_requests_start"))) static volatile uint64_t
    limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests"))) static volatile struct limine_framebuffer_request
    limine_framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 0};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_memmap_request
    limine_mmap_request = {.id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_module_request
    limine_module_request = {.id = LIMINE_MODULE_REQUEST_ID, .revision = 0};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_rsdp_request
    limine_rsdp_request = {.id = LIMINE_RSDP_REQUEST_ID, .revision = 0};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_executable_cmdline_request
    limine_cmdline_request = {.id = LIMINE_EXECUTABLE_CMDLINE_REQUEST_ID, .revision = 0};

__attribute__((used, section(".limine_requests_end"))) static volatile uint64_t
    limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void _setup_all_framebuffers()
{
    for (uint8_t i = 0; i < limine_framebuffer_request.response->framebuffer_count; i++) {
        struct limine_framebuffer *fb = limine_framebuffer_request.response->framebuffers[i];
        setup_framebuffer((framebuffer_t){
            .address = fb->address,
            .width = fb->width,
            .height = fb->height,
            .pitch = fb->pitch,
            .bpp = fb->bpp,
            .memory_model = fb->memory_model,
            .red_mask_size = fb->red_mask_size,
            .red_mask_shift = fb->red_mask_shift,
            .green_mask_size = fb->green_mask_size,
            .green_mask_shift = fb->green_mask_shift,
            .blue_mask_size = fb->blue_mask_size,
            .blue_mask_shift = fb->blue_mask_shift,
        });
    }
}

static mmap_type_t _map_type_from_limine_type(uint64_t type)
{
    switch (type) {
    case LIMINE_MEMMAP_USABLE:
        return MMAP_USABLE;
    case LIMINE_MEMMAP_RESERVED:
        return MMAP_RESERVED;
    case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
        return MMAP_ACPI_RECLAIMABLE;
    case LIMINE_MEMMAP_ACPI_NVS:
        return MMAP_ACPI_NVS;
    case LIMINE_MEMMAP_BAD_MEMORY:
        return MMAP_BAD_MEMORY;
    case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
        return MMAP_BOOTLOADER_RECLAIMABLE;
    case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
        return MMAP_EXECUTABLE_AND_MODULES;
    case LIMINE_MEMMAP_FRAMEBUFFER:
        return MMAP_FRAMEBUFFER;
    default:
        return MMAP_USABLE;
    }
}

static void _setup_all_mmaps()
{
    for (uint64_t i = 0; i < limine_mmap_request.response->entry_count; i++) {
        struct limine_memmap_entry *entry = limine_mmap_request.response->entries[i];
        mmap_add_entry(entry->base, entry->length, _map_type_from_limine_type(entry->type));
    }
}

void kmain()
{
    sse_init();

    _setup_all_framebuffers();
    _setup_all_mmaps();
    load_kernel_args(limine_cmdline_request.response->cmdline);

    start_debug_serial(SERIAL_COM1);
    start_debug_console(get_framebuffer(0));

    pic_init();
    gdt_init();
    idt_init();
    timer_init();
    pmm_init();
    vmm_init();
    apic_init(limine_rsdp_request.response->address);
    heap_init(10);
    syscalls_init();
    device_domain_init();
    debug_device_init();
    serial_devices_init();
    framebuffer_devices_init();
    shm_domain_init();
    ipc_init();
    pipe_domain_init();
    task_switching_init();
    task_domain_init();

    rsrc_node_t *tmpfs_root = tmpfs_mount("system");
    if (limine_module_request.response != NULL && limine_module_request.response->module_count > 0) {
        debug_log("Loading initrd into tmpfs...\n");
        for (size_t i = 0; i < limine_module_request.response->module_count; i++) {
            struct limine_file *module = limine_module_request.response->modules[i];
            if (tmpfs_populate_from_initrd(tmpfs_root, module->address) == 0) {
                debug_log_fmt("Loaded \"%s\" into tmpfs\n", module->path);
            } else {
                debug_log_fmt("Failed to load \"%s\" into tmpfs\n", module->path);
            }
        }
    }

    input_devices_init();

    char init_path[PATH_MAX];
    if (!get_kernel_arg("init", init_path, sizeof(init_path))) {
        debug_log("Missing or invalid init= boot argument\n");
        while (1)
            asm_hlt();
    }

    debug_log_fmt("Launching init process: %s\n", init_path);
    task_t *task = load_exec(init_path, NULL, 1, (const char *[]){init_path});
    if (task == NULL) {
        debug_log_fmt("Failed to load init ELF: %s\n", init_path);
        while (1)
            asm_hlt();
    }
    task_set_state(task, TASK_STATE_RUNNABLE);

    stop_debug_console();
    scheduler_init();

    task_get_current()->quantum = 0; /* De-prioritize the kernel task */
    while (1)
        asm_hlt();
}
