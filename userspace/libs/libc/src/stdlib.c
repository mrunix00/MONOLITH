/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef TEST_ENV
/* Mock alloc_pages for testing - provided by test harness */
#define PAGE_SIZE 4096
#define ALLOC_PAGES_FLAG_RW (1 << 0)
extern void *mock_alloc_pages(size_t num_pages, unsigned long flags);
#define alloc_pages mock_alloc_pages
#else
#include <sys/syscall.h>
#include <unistd.h>
#endif

typedef struct block_header
{
    size_t size;
    struct block_header *next;
} block_header_t;

static struct
{
    int initialized;
    block_header_t *free_list;
} _heap = {0, NULL};

static void _add_free_block(void *memory, size_t size)
{
    block_header_t *new_block = (block_header_t *) memory;
    new_block->size = size - sizeof(block_header_t);
    new_block->next = NULL;

    block_header_t *current = _heap.free_list;
    block_header_t *previous = NULL;
    while (current != NULL && current < new_block) {
        previous = current;
        current = current->next;
    }
    new_block->next = current;
    if (previous == NULL) {
        _heap.free_list = new_block;
    } else {
        previous->next = new_block;
    }
}

static int _heap_grow(size_t min_size)
{
    /* Calculate pages needed (minimum 1 page) */
    size_t pages = (min_size + sizeof(block_header_t) + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages < 1)
        pages = 1;

    void *new_memory = alloc_pages(pages, ALLOC_PAGES_FLAG_RW);
    if (new_memory == NULL)
        return -1;

    _add_free_block(new_memory, pages * PAGE_SIZE);
    return 0;
}

void *malloc(size_t size)
{
    if (size == 0)
        return NULL;

    /* Initialize heap on first use */
    if (!_heap.initialized) {
        if (_heap_grow(size) < 0)
            return NULL;
        _heap.initialized = 1;
    }

    block_header_t *current, *previous;

start:
    current = _heap.free_list;
    previous = NULL;

    /* Search for a suitable free block */
    while (current != NULL) {
        if (current->size >= size + sizeof(block_header_t) + 8) {
            /* Split block */
            block_header_t *new_block = (block_header_t *) ((void *) current
                                                            + sizeof(block_header_t) + size);
            new_block->size = current->size - size - sizeof(block_header_t);
            new_block->next = current->next;
            current->size = size;
            current->next = NULL;
            if (previous != NULL) {
                previous->next = new_block;
            } else {
                _heap.free_list = new_block;
            }
            return (void *) ((void *) current + sizeof(block_header_t));
        } else if (current->size >= size) {
            /* Use entire block */
            if (previous != NULL) {
                previous->next = current->next;
            } else {
                _heap.free_list = current->next;
            }
            return (void *) ((void *) current + sizeof(block_header_t));
        } else {
            previous = current;
            current = current->next;
        }
    }

    /* If no suitable blocks found, grow the heap */
    if (_heap_grow(size) == 0)
        goto start;

    return NULL;
}

void *realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return malloc(size);

    if (size == 0) {
        free(ptr);
        return NULL;
    }

    block_header_t *block = (block_header_t *) ((void *) ptr - sizeof(block_header_t));
    size_t old_size = block->size;

    if (size <= old_size)
        return ptr;

    void *new_ptr = malloc(size);
    if (new_ptr != NULL) {
        memcpy(new_ptr, ptr, old_size);
        free(ptr);
    }
    return new_ptr;
}

void free(void *ptr)
{
    if (ptr == NULL)
        return;

    block_header_t *block = (block_header_t *) ((void *) ptr - sizeof(block_header_t));

    block_header_t *current = _heap.free_list;
    block_header_t *previous = NULL;
    while (current != NULL && current < block) {
        previous = current;
        current = current->next;
    }

    /* Insert the block into the free list */
    block->next = current;
    if (previous == NULL) {
        _heap.free_list = block;
    } else {
        previous->next = block;
    }

    /* Merge with the previous block if adjacent */
    if (previous != NULL
        && (void *) previous + sizeof(block_header_t) + previous->size == (void *) block) {
        previous->size += sizeof(block_header_t) + block->size;
        previous->next = block->next;
        block = previous; /* Update block to point to the merged block */
    }

    /* Merge with the next block if adjacent */
    if (block->next != NULL
        && (void *) block + sizeof(block_header_t) + block->size == (void *) block->next) {
        block->size += sizeof(block_header_t) + block->next->size;
        block->next = block->next->next;
    }
}

int abs(int value)
{
    return (value < 0) ? -value : value;
}

#ifdef TEST_ENV
/* Reset heap state for testing */
void _heap_reset(void)
{
    _heap.initialized = 0;
    _heap.free_list = NULL;
}
#endif

static void swap(void *a, void *b, size_t size)
{
    char *ca = a;
    char *cb = b;
    for (size_t i = 0; i < size; i++) {
        char tmp = ca[i];
        ca[i] = cb[i];
        cb[i] = tmp;
    }
}

static void insertion_sort(
    void *base, size_t low, size_t high, size_t size, int (*compar)(const void *, const void *))
{
    char *base_ptr = (char *) base;
    for (size_t i = low + 1; i <= high; i++) {
        size_t j = i;
        while (j > low) {
            char *current = base_ptr + j * size;
            char *prev = base_ptr + (j - 1) * size;
            if (compar(prev, current) > 0) {
                swap(prev, current, size);
                j--;
            } else {
                break;
            }
        }
    }
}

static size_t partition(
    void *base, size_t low, size_t high, size_t size, int (*compar)(const void *, const void *))
{
    char *base_ptr = (char *) base;
    size_t mid = low + (high - low) / 2;
    char *a = base_ptr + low * size;
    char *b = base_ptr + mid * size;
    char *c = base_ptr + high * size;

    // Median-of-three: sort a, b, c
    if (compar(a, b) > 0)
        swap(a, b, size);
    if (compar(a, c) > 0)
        swap(a, c, size);
    if (compar(b, c) > 0)
        swap(b, c, size);

    // Move median (b) to high
    swap(b, c, size);

    // Partition using last element (median) as pivot
    void *pivot = c;
    size_t i = low;
    for (size_t j = low; j < high; j++) {
        if (compar(base_ptr + j * size, pivot) < 0) {
            swap(base_ptr + i * size, base_ptr + j * size, size);
            i++;
        }
    }
    swap(base_ptr + i * size, pivot, size);
    return i;
}

void qsort(void *base, size_t n, size_t size, int (*compar)(const void *, const void *))
{
    if (n <= 1)
        return;

    // Define stack structure and size
    typedef struct
    {
        size_t low;
        size_t high;
    } Pair;
#define MAX_STACK_DEPTH 64
    Pair stack[MAX_STACK_DEPTH];
    int top = -1;

    stack[++top] = (Pair) {0, n - 1};

    while (top >= 0) {
        Pair current = stack[top--];
        size_t low = current.low;
        size_t high = current.high;
        size_t len = high - low + 1;

        // Use insertion sort for small segments
        if (len <= 7) {
            insertion_sort(base, low, high, size, compar);
            continue;
        }

        // Partition the segment
        size_t p = partition(base, low, high, size, compar);

        // Push sub-segments with >=2 elements onto stack
        if (p > low + 1 && top + 1 < MAX_STACK_DEPTH) {
            stack[++top] = (Pair) {low, p - 1};
        }
        if (high > p + 1 && top + 1 < MAX_STACK_DEPTH) {
            stack[++top] = (Pair) {p + 1, high};
        }
    }
}

#ifndef TEST_ENV
[[noreturn]] void exit(int status)
{
    (void) status;
    syscall0(SYSCALL_EXIT);
    for (;;) {}
}

[[noreturn]] void abort(void)
{
    syscall0(SYSCALL_EXIT);
    for (;;) {}
}
#endif /* TEST_ENV */
