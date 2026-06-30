/*
 * Copyright (c) 2026, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <kernel/arch/pc/apic.h>
#include <kernel/arch/pc/asm.h>
#include <kernel/arch/pc/gdt.h>
#include <kernel/arch/pc/ia32/multiboot2.h>
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
#include <kernel/rsmgr/rsmgr.h>
#include <kernel/tasking/ipc.h>
#include <kernel/tasking/loader.h>
#include <kernel/tasking/pipe.h>
#include <kernel/tasking/scheduler.h>
#include <kernel/tasking/syscall.h>
#include <kernel/tasking/task_domain.h>
#include <kernel/timer.h>
#include <kernel/types.h>

struct flanterm_context *_fb_ctx;

typedef struct
{
    uint32_t total_size;
    uint32_t reserved;
    struct multiboot_tag tags[0];
} multiboot_header;

static struct multiboot_tag *_next_tag(multiboot_header *header, struct multiboot_tag *tag)
{
    uintptr_t next = ((uintptr_t) tag + tag->size + (MULTIBOOT_TAG_ALIGN - 1))
                     & ~(uintptr_t) (MULTIBOOT_TAG_ALIGN - 1);
    uintptr_t end = (uintptr_t) header + header->total_size;
    if (tag->size == 0 || next + sizeof(struct multiboot_tag) > end)
        return NULL;
    return (struct multiboot_tag *) next;
}

static void _setup_all_framebuffers(multiboot_header *header)
{
    struct multiboot_tag *tag = header->tags;
    while (tag != NULL && tag->type != MULTIBOOT_TAG_TYPE_END) {
        if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
            struct multiboot_tag_framebuffer *fb_tag = (struct multiboot_tag_framebuffer *) tag;
            setup_framebuffer((framebuffer_t){
                .address = (uint32_t *) fb_tag->common.framebuffer_addr,
                .width = fb_tag->common.framebuffer_width,
                .height = fb_tag->common.framebuffer_height,
                .pitch = fb_tag->common.framebuffer_pitch,
                .bpp = fb_tag->common.framebuffer_bpp,
                .memory_model = fb_tag->common.framebuffer_type,
                .red_mask_size = fb_tag->framebuffer_red_mask_size,
                .red_mask_shift = fb_tag->framebuffer_red_field_position,
                .green_mask_size = fb_tag->framebuffer_green_mask_size,
                .green_mask_shift = fb_tag->framebuffer_green_field_position,
                .blue_mask_size = fb_tag->framebuffer_blue_mask_size,
                .blue_mask_shift = fb_tag->framebuffer_blue_field_position,
            });
        }
        tag = _next_tag(header, tag);
    }
}

static mmap_type_t _map_type_from_multiboot2_type(uint64_t type)
{
    switch (type) {
    case MULTIBOOT_MEMORY_AVAILABLE:
        return MMAP_USABLE;
    case MULTIBOOT_MEMORY_RESERVED:
        return MMAP_RESERVED;
    case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
        return MMAP_ACPI_RECLAIMABLE;
    case MULTIBOOT_MEMORY_NVS:
        return MMAP_ACPI_NVS;
    case MULTIBOOT_MEMORY_BADRAM:
        return MMAP_BAD_MEMORY;
    default:
        return MMAP_RESERVED;
    }
}

static void _setup_all_mmaps(multiboot_header *header)
{
    struct multiboot_tag *tag = header->tags;
    while (tag != NULL && tag->type != MULTIBOOT_TAG_TYPE_END) {
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            struct multiboot_tag_mmap *mmap_list = (struct multiboot_tag_mmap *) tag;
            struct multiboot_mmap_entry *mmap_entry = mmap_list->entries;
            uint8_t *mmap_list_end = (uint8_t *) mmap_list + mmap_list->size;
            while ((uint8_t *) mmap_entry < mmap_list_end) {
                mmap_type_t type = _map_type_from_multiboot2_type(mmap_entry->type);
                mmap_add_entry(mmap_entry->addr, mmap_entry->len, type);
                mmap_entry = (struct multiboot_mmap_entry *) ((uint8_t *) mmap_entry
                                                              + mmap_list->entry_size);
            }
            return;
        }
        tag = _next_tag(header, tag);
    }
}

static void _reserve_all_modules(multiboot_header *header)
{
    struct multiboot_tag *tag = header->tags;
    while (tag != NULL && tag->type != MULTIBOOT_TAG_TYPE_END) {
        if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
            struct multiboot_tag_module *module = (struct multiboot_tag_module *) tag;
            mmap_add_entry(
                module->mod_start, module->mod_end - module->mod_start, MMAP_EXECUTABLE_AND_MODULES);
        }
        tag = _next_tag(header, tag);
    }
}

static void _setup_kernel_args(multiboot_header *header)
{
    struct multiboot_tag *tag = header->tags;
    while (tag != NULL && tag->type != MULTIBOOT_TAG_TYPE_END) {
        if (tag->type == MULTIBOOT_TAG_TYPE_CMDLINE) {
            struct multiboot_tag_string *cmdline = (struct multiboot_tag_string *) tag;
            return load_kernel_args(cmdline->string);
        }
        tag = _next_tag(header, tag);
    }
}

static void *_find_rsdp(multiboot_header *header)
{
    struct multiboot_tag *tag = header->tags;
    while (tag != NULL && tag->type != MULTIBOOT_TAG_TYPE_END) {
        if (tag->type == MULTIBOOT_TAG_TYPE_ACPI_OLD || tag->type == MULTIBOOT_TAG_TYPE_ACPI_NEW)
            return ((struct multiboot_tag_new_acpi *) tag)->rsdp;
        tag = _next_tag(header, tag);
    }
    return NULL;
}

static void _setup_initrd(multiboot_header *header, rsrc_node_t *tmpfs_root)
{
    struct multiboot_tag *tag = header->tags;
    while (tag != NULL && tag->type != MULTIBOOT_TAG_TYPE_END) {
        if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
            struct multiboot_tag_module *module = (struct multiboot_tag_module *) tag;
            if (!tmpfs_populate_from_initrd(tmpfs_root, (void *) (uintptr_t) module->mod_start))
                debug_log_fmt("Loaded \"%s\" into tmpfs\n", module->cmdline);
            else
                debug_log_fmt("Failed to load \"%s\" into tmpfs\n", module->cmdline);
        }
        tag = _next_tag(header, tag);
    }
}

void kmain(uint32_t magic, uintptr_t mbi_addr)
{
    multiboot_header *multiboot2_header = (multiboot_header *) mbi_addr;
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC || multiboot2_header == NULL
        || multiboot2_header->total_size == 0)
        return;

    sse_init();

    _setup_all_framebuffers(multiboot2_header);
    _setup_all_mmaps(multiboot2_header);
    _reserve_all_modules(multiboot2_header);
    _setup_kernel_args(multiboot2_header);
    void *_rsdp = _find_rsdp(multiboot2_header);

    start_debug_serial(SERIAL_COM1);
    start_debug_console(get_framebuffer(0));

    pic_init();
    gdt_init();
    idt_init();
    timer_init();
    pmm_init();
    vmm_init();
    if (_rsdp != NULL)
        apic_init(_rsdp);
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
    _setup_initrd(multiboot2_header, tmpfs_root);

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
