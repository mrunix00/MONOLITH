/* IA-32 two-level paging implementation. */

#include <kernel/arch/pc/asm.h>
#include <kernel/devices/debug.h>
#include <kernel/klibc/memory.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>
#include <kernel/mmap.h>

#define IA32_PTE_PRESENT 0x001
#define IA32_PTE_RW 0x002
#define IA32_PTE_USER 0x004
#define IA32_PTE_FLAGS 0xFFF

static uintptr_t _kernel_cr3;
static uint32_t *_kernel_pd;

void *vmm_get_hhdm_addr(void *phys_addr)
{
    return phys_addr;
}

void *vmm_get_lhdm_addr(void *virt_addr)
{
    return virt_addr;
}

uintptr_t vmm_get_kernel_cr3(void)
{
    return _kernel_cr3;
}

static uint32_t *get_table(uint32_t *pd, uintptr_t virt, bool create)
{
    size_t pd_i = virt >> 22;
    if (!(pd[pd_i] & IA32_PTE_PRESENT)) {
        if (!create)
            return NULL;
        uint32_t *pt_phys = pmm_alloc(1);
        if (pt_phys == NULL)
            return NULL;
        memset(pt_phys, 0, PAGE_SIZE);
        pd[pd_i] = (uintptr_t) pt_phys | IA32_PTE_PRESENT | IA32_PTE_RW | IA32_PTE_USER;
    }
    return (uint32_t *) (uintptr_t) (pd[pd_i] & ~IA32_PTE_FLAGS);
}

static uint32_t *clone_shared_kernel_table(uint32_t *pd, size_t pd_i)
{
    if (pd == _kernel_pd || !(pd[pd_i] & IA32_PTE_PRESENT))
        return (uint32_t *) (uintptr_t) (pd[pd_i] & ~IA32_PTE_FLAGS);

    uint32_t kernel_entry = _kernel_pd[pd_i];
    if (!(kernel_entry & IA32_PTE_PRESENT))
        return (uint32_t *) (uintptr_t) (pd[pd_i] & ~IA32_PTE_FLAGS);

    if ((pd[pd_i] & ~IA32_PTE_FLAGS) != (kernel_entry & ~IA32_PTE_FLAGS))
        return (uint32_t *) (uintptr_t) (pd[pd_i] & ~IA32_PTE_FLAGS);

    uint32_t *old_pt = (uint32_t *) (uintptr_t) (pd[pd_i] & ~IA32_PTE_FLAGS);
    uint32_t *new_pt = pmm_alloc(1);
    if (new_pt == NULL)
        return NULL;

    memcpy(new_pt, old_pt, PAGE_SIZE);
    pd[pd_i] = (uintptr_t) new_pt | (pd[pd_i] & IA32_PTE_FLAGS);
    return new_pt;
}

void vmm_init(void)
{
    debug_log("Initializing IA-32 VMM...\n");
    debug_log("Using IA-32 two-level paging\n");

    _kernel_pd = pmm_alloc(1);
    if (_kernel_pd == NULL) {
        debug_log("Failed to allocate IA-32 page directory\n");
        while (1)
            asm_hlt();
    }
    memset(_kernel_pd, 0, PAGE_SIZE);
    _kernel_cr3 = (uintptr_t) _kernel_pd;

    debug_log("Identity mapping 0x0 - 0x40000000\n");
    for (uintptr_t addr = 0; addr < 0x40000000; addr += PAGE_SIZE)
        vmm_map(_kernel_cr3, addr, addr, IA32_PTE_PRESENT | IA32_PTE_RW, false);

    framebuffer_t *framebuffer = get_framebuffer(0);
    if (framebuffer) {
        uintptr_t fb_addr = (uintptr_t) framebuffer->address;
        size_t fb_size = framebuffer->pitch * framebuffer->height;
        uintptr_t fb_page = fb_addr & ~(PAGE_SIZE - 1);
        debug_log_fmt(
            "Mapping framebuffer 0x%x - 0x%x\n", fb_page, fb_page + fb_size + (fb_addr - fb_page));
        vmm_map_range(
            _kernel_cr3,
            fb_page,
            fb_page,
            fb_size + (fb_addr - fb_page),
            IA32_PTE_PRESENT | IA32_PTE_RW,
            false);
    }

    uint32_t eflags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(eflags) : : "memory");

    asm_write_cr3(_kernel_cr3);
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

    if (eflags & (1u << 9))
        asm_sti();
    debug_log_fmt("The page directory is located at 0x%x\n", (uintptr_t) _kernel_pd);
    debug_log("Initialized IA-32 VMM\n");
}

void vmm_map(uintptr_t cr3, uintptr_t virt, uintptr_t phys, size_t flags, bool flush)
{
    uint32_t *pd = (uint32_t *) cr3;
    size_t pd_i = virt >> 22;
    uint32_t *pt = get_table(pd, virt, true);
    if (pt == NULL) {
        debug_log_fmt("Failed to allocate page table for 0x%x\n", virt);
        return;
    }
    pt = clone_shared_kernel_table(pd, pd_i);
    if (pt == NULL) {
        debug_log_fmt("Failed to clone page table for 0x%x\n", virt);
        return;
    }

    pd[pd_i] |= flags & (IA32_PTE_RW | IA32_PTE_USER);

    size_t pt_i = (virt >> 12) & 0x3FF;
    pt[pt_i] = (phys & ~IA32_PTE_FLAGS) | (flags & IA32_PTE_FLAGS) | IA32_PTE_PRESENT;
    if (flush) {
        debug_log_fmt("Mapped phys 0x%x to virt 0x%x\n", phys, virt);
        if (asm_read_cr3() == cr3)
            asm_invlpg((void *) virt);
    }
}

void vmm_map_range(
    uintptr_t cr3, uintptr_t virt_addr, uintptr_t phys_addr, size_t size, size_t flags, bool flush)
{
    debug_log_fmt(
        "Mapping 0x%x - 0x%x to 0x%x - 0x%x in address space 0x%x\n",
        phys_addr,
        phys_addr + size,
        virt_addr,
        virt_addr + size,
        cr3);
    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < num_pages; i++)
        vmm_map(cr3, virt_addr + i * PAGE_SIZE, phys_addr + i * PAGE_SIZE, flags, false);
    if (flush && asm_read_cr3() == cr3)
        asm_write_cr3(cr3);
}

void vmm_unmap(uintptr_t cr3, uintptr_t virt_addr, bool flush)
{
    uint32_t *pd = (uint32_t *) cr3;
    uint32_t *pt = get_table(pd, virt_addr, false);
    if (pt == NULL)
        return;
    pt[(virt_addr >> 12) & 0x3FF] = 0;
    if (flush && asm_read_cr3() == cr3)
        asm_invlpg((void *) virt_addr);
}

void vmm_unmap_range(uintptr_t cr3, uintptr_t virt_addr, size_t size, bool flush)
{
    debug_log_fmt("Unmapping 0x%x - 0x%x from address space 0x%x\n", virt_addr, virt_addr + size, cr3);
    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < num_pages; i++)
        vmm_unmap(cr3, virt_addr + i * PAGE_SIZE, false);
    if (flush && asm_read_cr3() == cr3)
        asm_write_cr3(cr3);
}

uintptr_t vmm_create_address_space(void)
{
    uint32_t *pd = pmm_alloc(1);
    if (pd == NULL) {
        debug_log("Failed to allocate page directory for new address space\n");
        return 0;
    }
    memcpy(pd, _kernel_pd, PAGE_SIZE);
    debug_log_fmt("Created new address space at 0x%x\n", (uintptr_t) pd);
    return (uintptr_t) pd;
}

void vmm_destroy_address_space(uintptr_t cr3)
{
    if (cr3 == 0 || cr3 == _kernel_cr3)
        return;
    pmm_free((void *) cr3, 1);
    debug_log_fmt("Destroyed address space at 0x%x\n", cr3);
}
