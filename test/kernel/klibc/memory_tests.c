#include <libs/Unity/src/unity.h>

#include <kernel/klibc/memory.h>
#include <kernel/memory/heap.h>

static void test_memset(void)
{
    char buffer[21];
    memset(buffer, 'A', 20);
    buffer[20] = '\0';
    TEST_ASSERT_EQUAL_STRING("AAAAAAAAAAAAAAAAAAAA", buffer);
}

static void test_memcmp(void)
{
    const char left[] = "abc";
    const char right[] = "abd";
    TEST_ASSERT_EQUAL(0, memcmp(left, "abc", 3));
    TEST_ASSERT_TRUE(memcmp(left, right, 3) < 0);
    TEST_ASSERT_TRUE(memcmp(right, left, 3) > 0);
}

static void test_memcmp_zero_length(void)
{
    const char left[] = "abc";
    const char right[] = "abd";
    TEST_ASSERT_EQUAL(0, memcmp(left, right, 0));
}

static void test_memcmp_embedded_nulls(void)
{
    const unsigned char left[] = {'a', '\0', 'b', 'c'};
    const unsigned char right[] = {'a', '\0', 'b', 'd'};
    TEST_ASSERT_TRUE(memcmp(left, right, sizeof(left)) < 0);
}

void klibc_memory_tests()
{
    RUN_TEST(test_memset);
    RUN_TEST(test_memcmp);
    RUN_TEST(test_memcmp_zero_length);
    RUN_TEST(test_memcmp_embedded_nulls);
}
