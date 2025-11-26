/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <stddef.h>
#include <stdlib.h>

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

void abort() {}
