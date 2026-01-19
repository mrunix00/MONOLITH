#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <libs/Unity/src/unity.h>
#include <stdlib.h>

static void test_strcmp_different()
{
    const char *str1 = "Hello";
    const char *str2 = "World";
    TEST_ASSERT_EQUAL(-15, strcmp(str1, str2));
}

static void test_strcmp_same()
{
    const char *str1 = "Hello";
    const char *str2 = "Hello";
    TEST_ASSERT_EQUAL(0, strcmp(str1, str2));
}

static void test_atoul()
{
    const char *str = "12345";
    unsigned long result = atoul(str);
    TEST_ASSERT_EQUAL(12345, result);
}

static void test_atoul_whitespace()
{
    const char *str = " 12345 ";
    unsigned long result = atoul(str);
    TEST_ASSERT_EQUAL(12345, result);
}

static void test_strncmp_prefix()
{
    TEST_ASSERT_EQUAL(0, strncmp("hello", "helium", 3));
    TEST_ASSERT_TRUE(strncmp("hello", "helium", 5) > 0);
}

static void test_strcpy_and_strncpy()
{
    char dest[8] = {0};
    strcpy(dest, "abc");
    TEST_ASSERT_EQUAL_STRING("abc", dest);

    char dest2[5] = {0};
    strncpy(dest2, "xyz", 4);
    TEST_ASSERT_EQUAL_STRING("xyz", dest2);
}

static void test_strncat()
{
    char dest[8] = "a";
    strncat(dest, "bcdef", 2);
    TEST_ASSERT_EQUAL_STRING("abc", dest);
}

static void test_strstr()
{
    const char *hay = "find the needle";
    const char *pos = strstr(hay, "needle");
    TEST_ASSERT_NOT_NULL(pos);
    TEST_ASSERT_EQUAL(hay + 9, pos);
}

static void test_atoi_and_atox()
{
    TEST_ASSERT_EQUAL(-42, atoi("  -42"));
    TEST_ASSERT_EQUAL(255, atoi("+255"));
    TEST_ASSERT_EQUAL(31, atox("0x1f"));
    TEST_ASSERT_EQUAL(48879, atox("BEEF"));
}

static void test_itohex()
{
    char buffer[16];
    TEST_ASSERT_EQUAL_STRING("0", itohex(0, buffer));
    TEST_ASSERT_EQUAL_STRING("2f", itohex(0x2f, buffer));
}

static void test_strchr_and_strrchr()
{
    const char *text = "abac";
    TEST_ASSERT_EQUAL(text + 1, strchr(text, 'b'));
    TEST_ASSERT_EQUAL(text + 3, strrchr(text, 'c'));
    TEST_ASSERT_NULL(strchr(text, 'z'));
}

static void test_vstrlen_ignores_ansi_sequences()
{
    const char *text = "Hi \x1b[31mRed\x1b[0m";
    TEST_ASSERT_EQUAL(6, vstrlen(text));
}

void klibc_string_tests()
{
    RUN_TEST(test_strcmp_different);
    RUN_TEST(test_strcmp_same);
    RUN_TEST(test_atoul);
    RUN_TEST(test_atoul_whitespace);
    RUN_TEST(test_strncmp_prefix);
    RUN_TEST(test_strcpy_and_strncpy);
    RUN_TEST(test_strncat);
    RUN_TEST(test_strstr);
    RUN_TEST(test_atoi_and_atox);
    RUN_TEST(test_itohex);
    RUN_TEST(test_strchr_and_strrchr);
    RUN_TEST(test_vstrlen_ignores_ansi_sequences);
}
