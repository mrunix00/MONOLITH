/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <libs/Unity/src/unity.h>
#include <string.h>

/* Mock implementation of alloc_pages for testing malloc/realloc/free */
#define MOCK_HEAP_SIZE (64 * 4096) /* 64 pages */
static char mock_heap[MOCK_HEAP_SIZE];
static size_t mock_heap_offset = 0;

void *mock_alloc_pages(size_t num_pages, unsigned long flags)
{
    (void) flags;
    size_t size = num_pages * 4096;
    if (mock_heap_offset + size > MOCK_HEAP_SIZE) {
        return NULL; /* Out of memory */
    }
    void *ptr = &mock_heap[mock_heap_offset];
    mock_heap_offset += size;
    return ptr;
}

/* Reset heap state (defined in stdlib.c when TEST_ENV is set) */
extern void _heap_reset(void);

/* Declare malloc/realloc/free/qsort from our libc (not system's) */
void *malloc(size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

static void teardown(void)
{
    /* Reset both mock heap and libc heap state between tests */
    mock_heap_offset = 0;
    memset(mock_heap, 0, MOCK_HEAP_SIZE);
    _heap_reset();
}

#define TEST(func) \
    RUN_TEST(func); \
    teardown();

/* Comparator for qsort tests */
static int int_compare(const void *a, const void *b)
{
    return (*(const int *) a) - (*(const int *) b);
}

static int int_compare_desc(const void *a, const void *b)
{
    return (*(const int *) b) - (*(const int *) a);
}

/* ========== malloc tests ========== */

static void test_malloc_returns_non_null(void)
{
    void *ptr = malloc(100);
    TEST_ASSERT_NOT_NULL(ptr);
    free(ptr);
}

static void test_malloc_zero_returns_null(void)
{
    void *ptr = malloc(0);
    TEST_ASSERT_NULL(ptr);
}

static void test_malloc_multiple_allocations(void)
{
    void *ptr1 = malloc(100);
    void *ptr2 = malloc(200);
    void *ptr3 = malloc(50);
    TEST_ASSERT_NOT_NULL(ptr1);
    TEST_ASSERT_NOT_NULL(ptr2);
    TEST_ASSERT_NOT_NULL(ptr3);
    /* Ensure they don't overlap */
    TEST_ASSERT_NOT_EQUAL(ptr1, ptr2);
    TEST_ASSERT_NOT_EQUAL(ptr2, ptr3);
    TEST_ASSERT_NOT_EQUAL(ptr1, ptr3);
    free(ptr1);
    free(ptr2);
    free(ptr3);
}

static void test_malloc_can_write_to_memory(void)
{
    char *ptr = malloc(100);
    TEST_ASSERT_NOT_NULL(ptr);
    memset(ptr, 'A', 100);
    for (int i = 0; i < 100; i++) {
        TEST_ASSERT_EQUAL('A', ptr[i]);
    }
    free(ptr);
}

static void test_malloc_large_allocation(void)
{
    /* Allocate multiple pages */
    void *ptr = malloc(8192);
    TEST_ASSERT_NOT_NULL(ptr);
    free(ptr);
}

static void test_malloc_reuses_freed_memory(void)
{
    void *ptr1 = malloc(100);
    TEST_ASSERT_NOT_NULL(ptr1);
    free(ptr1);
    void *ptr2 = malloc(100);
    TEST_ASSERT_NOT_NULL(ptr2);
    /* After freeing, should reuse the same block */
    TEST_ASSERT_EQUAL_PTR(ptr1, ptr2);
    free(ptr2);
}

/* ========== free tests ========== */

static void test_free_null_is_safe(void)
{
    free(NULL); /* Should not crash */
    TEST_PASS();
}

static void test_free_and_realloc_pattern(void)
{
    void *ptr1 = malloc(50);
    void *ptr2 = malloc(50);
    free(ptr1);
    void *ptr3 = malloc(50);
    /* ptr3 should reuse freed block from ptr1 */
    TEST_ASSERT_EQUAL_PTR(ptr1, ptr3);
    free(ptr2);
    free(ptr3);
}

static void test_free_coalesces_adjacent_blocks(void)
{
    /* Allocate three blocks */
    void *ptr1 = malloc(100);
    void *ptr2 = malloc(100);
    void *ptr3 = malloc(100);
    TEST_ASSERT_NOT_NULL(ptr1);
    TEST_ASSERT_NOT_NULL(ptr2);
    TEST_ASSERT_NOT_NULL(ptr3);

    /* Free middle block first, then first, then last */
    free(ptr2);
    free(ptr1);
    free(ptr3);

    /* Now allocate a larger block - should use coalesced space */
    void *ptr4 = malloc(250);
    TEST_ASSERT_NOT_NULL(ptr4);
    free(ptr4);
}

/* ========== realloc tests ========== */

static void test_realloc_null_acts_as_malloc(void)
{
    void *ptr = realloc(NULL, 100);
    TEST_ASSERT_NOT_NULL(ptr);
    free(ptr);
}

static void test_realloc_zero_frees_memory(void)
{
    void *ptr = malloc(100);
    TEST_ASSERT_NOT_NULL(ptr);
    void *result = realloc(ptr, 0);
    TEST_ASSERT_NULL(result);
}

static void test_realloc_grow(void)
{
    char *ptr = malloc(50);
    TEST_ASSERT_NOT_NULL(ptr);
    memset(ptr, 'X', 50);

    ptr = realloc(ptr, 100);
    TEST_ASSERT_NOT_NULL(ptr);
    /* Original data should be preserved */
    for (int i = 0; i < 50; i++) {
        TEST_ASSERT_EQUAL('X', ptr[i]);
    }
    free(ptr);
}

static void test_realloc_shrink(void)
{
    char *ptr = malloc(100);
    TEST_ASSERT_NOT_NULL(ptr);
    memset(ptr, 'Y', 100);

    /* Shrinking should return same pointer */
    char *new_ptr = realloc(ptr, 50);
    TEST_ASSERT_NOT_NULL(new_ptr);
    TEST_ASSERT_EQUAL_PTR(ptr, new_ptr);
    /* Data should still be there */
    for (int i = 0; i < 50; i++) {
        TEST_ASSERT_EQUAL('Y', new_ptr[i]);
    }
    free(new_ptr);
}

static void test_realloc_preserves_data(void)
{
    int *arr = malloc(5 * sizeof(int));
    TEST_ASSERT_NOT_NULL(arr);
    for (int i = 0; i < 5; i++) {
        arr[i] = i * 10;
    }

    arr = realloc(arr, 10 * sizeof(int));
    TEST_ASSERT_NOT_NULL(arr);
    /* Original data preserved */
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL(i * 10, arr[i]);
    }
    free(arr);
}

/* ========== qsort tests ========== */
static void test_qsort_empty(void)
{
    int arr[] = {};
    qsort(arr, 0, sizeof(int), int_compare);
    /* Should not crash */
    TEST_PASS();
}

static void test_qsort_single_element(void)
{
    int arr[] = {42};
    qsort(arr, 1, sizeof(int), int_compare);
    TEST_ASSERT_EQUAL(42, arr[0]);
}

static void test_qsort_two_elements_sorted(void)
{
    int arr[] = {1, 2};
    qsort(arr, 2, sizeof(int), int_compare);
    TEST_ASSERT_EQUAL(1, arr[0]);
    TEST_ASSERT_EQUAL(2, arr[1]);
}

static void test_qsort_two_elements_reversed(void)
{
    int arr[] = {2, 1};
    qsort(arr, 2, sizeof(int), int_compare);
    TEST_ASSERT_EQUAL(1, arr[0]);
    TEST_ASSERT_EQUAL(2, arr[1]);
}

static void test_qsort_small_array(void)
{
    int arr[] = {5, 2, 8, 1, 9};
    qsort(arr, 5, sizeof(int), int_compare);
    TEST_ASSERT_EQUAL(1, arr[0]);
    TEST_ASSERT_EQUAL(2, arr[1]);
    TEST_ASSERT_EQUAL(5, arr[2]);
    TEST_ASSERT_EQUAL(8, arr[3]);
    TEST_ASSERT_EQUAL(9, arr[4]);
}

static void test_qsort_already_sorted(void)
{
    int arr[] = {1, 2, 3, 4, 5};
    qsort(arr, 5, sizeof(int), int_compare);
    TEST_ASSERT_EQUAL(1, arr[0]);
    TEST_ASSERT_EQUAL(2, arr[1]);
    TEST_ASSERT_EQUAL(3, arr[2]);
    TEST_ASSERT_EQUAL(4, arr[3]);
    TEST_ASSERT_EQUAL(5, arr[4]);
}

static void test_qsort_reverse_sorted(void)
{
    int arr[] = {5, 4, 3, 2, 1};
    qsort(arr, 5, sizeof(int), int_compare);
    TEST_ASSERT_EQUAL(1, arr[0]);
    TEST_ASSERT_EQUAL(2, arr[1]);
    TEST_ASSERT_EQUAL(3, arr[2]);
    TEST_ASSERT_EQUAL(4, arr[3]);
    TEST_ASSERT_EQUAL(5, arr[4]);
}

static void test_qsort_duplicates(void)
{
    int arr[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3};
    qsort(arr, 10, sizeof(int), int_compare);
    TEST_ASSERT_EQUAL(1, arr[0]);
    TEST_ASSERT_EQUAL(1, arr[1]);
    TEST_ASSERT_EQUAL(2, arr[2]);
    TEST_ASSERT_EQUAL(3, arr[3]);
    TEST_ASSERT_EQUAL(3, arr[4]);
    TEST_ASSERT_EQUAL(4, arr[5]);
    TEST_ASSERT_EQUAL(5, arr[6]);
    TEST_ASSERT_EQUAL(5, arr[7]);
    TEST_ASSERT_EQUAL(6, arr[8]);
    TEST_ASSERT_EQUAL(9, arr[9]);
}

static void test_qsort_all_same(void)
{
    int arr[] = {7, 7, 7, 7, 7};
    qsort(arr, 5, sizeof(int), int_compare);
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL(7, arr[i]);
    }
}

static void test_qsort_descending(void)
{
    int arr[] = {1, 5, 3, 2, 4};
    qsort(arr, 5, sizeof(int), int_compare_desc);
    TEST_ASSERT_EQUAL(5, arr[0]);
    TEST_ASSERT_EQUAL(4, arr[1]);
    TEST_ASSERT_EQUAL(3, arr[2]);
    TEST_ASSERT_EQUAL(2, arr[3]);
    TEST_ASSERT_EQUAL(1, arr[4]);
}

static void test_qsort_large_array(void)
{
    /* Test with array larger than insertion sort threshold (7) */
    int arr[] = {15, 3, 9, 8, 5, 2, 7, 1, 6, 13, 11, 12, 10, 4, 14};
    int n = sizeof(arr) / sizeof(arr[0]);
    qsort(arr, n, sizeof(int), int_compare);

    for (int i = 0; i < n; i++) {
        TEST_ASSERT_EQUAL(i + 1, arr[i]);
    }
}

static void test_qsort_negative_numbers(void)
{
    int arr[] = {-5, 3, -1, 0, 2, -3};
    qsort(arr, 6, sizeof(int), int_compare);
    TEST_ASSERT_EQUAL(-5, arr[0]);
    TEST_ASSERT_EQUAL(-3, arr[1]);
    TEST_ASSERT_EQUAL(-1, arr[2]);
    TEST_ASSERT_EQUAL(0, arr[3]);
    TEST_ASSERT_EQUAL(2, arr[4]);
    TEST_ASSERT_EQUAL(3, arr[5]);
}

/* Test qsort with structs */
typedef struct
{
    int key;
    char value;
} pair_t;

static int pair_compare(const void *a, const void *b)
{
    return ((const pair_t *) a)->key - ((const pair_t *) b)->key;
}

static void test_qsort_structs(void)
{
    pair_t arr[] = {{3, 'c'}, {1, 'a'}, {2, 'b'}};
    qsort(arr, 3, sizeof(pair_t), pair_compare);
    TEST_ASSERT_EQUAL(1, arr[0].key);
    TEST_ASSERT_EQUAL('a', arr[0].value);
    TEST_ASSERT_EQUAL(2, arr[1].key);
    TEST_ASSERT_EQUAL('b', arr[1].value);
    TEST_ASSERT_EQUAL(3, arr[2].key);
    TEST_ASSERT_EQUAL('c', arr[2].value);
}

void libc_stdlib_tests(void)
{
    /* malloc tests */
    TEST(test_malloc_returns_non_null);
    TEST(test_malloc_zero_returns_null);
    TEST(test_malloc_multiple_allocations);
    TEST(test_malloc_can_write_to_memory);
    TEST(test_malloc_large_allocation);
    TEST(test_malloc_reuses_freed_memory);

    /* free tests */
    TEST(test_free_null_is_safe);
    TEST(test_free_and_realloc_pattern);
    TEST(test_free_coalesces_adjacent_blocks);

    /* realloc tests */
    TEST(test_realloc_null_acts_as_malloc);
    TEST(test_realloc_zero_frees_memory);
    TEST(test_realloc_grow);
    TEST(test_realloc_shrink);
    TEST(test_realloc_preserves_data);

    /* qsort tests */
    TEST(test_qsort_empty);
    TEST(test_qsort_single_element);
    TEST(test_qsort_two_elements_sorted);
    TEST(test_qsort_two_elements_reversed);
    TEST(test_qsort_small_array);
    TEST(test_qsort_already_sorted);
    TEST(test_qsort_reverse_sorted);
    TEST(test_qsort_duplicates);
    TEST(test_qsort_all_same);
    TEST(test_qsort_descending);
    TEST(test_qsort_large_array);
    TEST(test_qsort_negative_numbers);
    TEST(test_qsort_structs);
}
