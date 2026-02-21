#include <kernel/memory/heap.h>
#include <kernel/memory/pmm.h>
#include <kernel/tasking/task.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#define TASK_REGISTRY_MAX 64

static task_t *g_task_registry[TASK_REGISTRY_MAX];
static size_t g_task_registry_count;

void task_registry_reset(void)
{
    g_task_registry_count = 0;
}

int task_registry_add(task_t *task)
{
    if (!task || g_task_registry_count >= TASK_REGISTRY_MAX)
        return -1;

    g_task_registry[g_task_registry_count++] = task;
    return 0;
}

int task_registry_remove(task_t *task)
{
    if (!task)
        return -1;

    for (size_t i = 0; i < g_task_registry_count; i++) {
        if (g_task_registry[i] == task) {
            for (size_t j = i + 1; j < g_task_registry_count; j++)
                g_task_registry[j - 1] = g_task_registry[j];
            g_task_registry_count--;
            return 0;
        }
    }

    return -1;
}

void *kmalloc(size_t size)
{
    return malloc(size);
}

void *krealloc(void *pointer, size_t size)
{
    return realloc(pointer, size);
}

void kfree(void *pointer)
{
    free(pointer);
}

bool heap_init(size_t pages)
{
    (void) pages;
    return true;
}

heap_stats_t heap_get_stats(void)
{
    return (heap_stats_t) {0};
}

void *pmm_alloc(size_t pages)
{
    if (pages == 0)
        return NULL;
    return malloc(pages * PAGE_SIZE);
}

void pmm_free(void *ptr, size_t pages)
{
    (void) pages;
    free(ptr);
}

task_t *task_find_by_id(uint64_t id)
{
    if (id == 0)
        return NULL;

    for (size_t i = 0; i < g_task_registry_count; i++) {
        task_t *task = g_task_registry[i];
        if (task && task->id == id)
            return task;
    }

    return NULL;
}

uintptr_t task_find_free_vaddr(task_t *task, size_t num_pages)
{
    (void) task;
    (void) num_pages;
    return 0x1000;
}

int task_map(
    task_t *task,
    uintptr_t virt_addr,
    uintptr_t phys_addr,
    size_t page_count,
    uintptr_t flags,
    bool release_on_exit)
{
    (void) task;
    (void) virt_addr;
    (void) phys_addr;
    (void) page_count;
    (void) flags;
    (void) release_on_exit;
    return 0;
}

int task_unmap(task_t *task, uintptr_t virt_addr, size_t page_count, bool release_on_exit)
{
    (void) task;
    (void) virt_addr;
    (void) page_count;
    (void) release_on_exit;
    return 0;
}
