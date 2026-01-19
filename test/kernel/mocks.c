#include <kernel/memory/heap.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

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

void unity_output_char(int c)
{
    (void) putchar(c);
}
