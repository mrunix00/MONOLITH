#include <libs/Unity/src/unity.h>

#include <kernel/klibc/string.h>

void setUp(void) {}
void tearDown(void) {}

void test_strcmp_different()
{
    const char *str1 = "Hello";
    const char *str2 = "World";
    TEST_ASSERT_EQUAL(-15, strcmp(str1, str2));
}

void test_strcmp_same()
{
    const char *str1 = "Hello";
    const char *str2 = "Hello";
    TEST_ASSERT_EQUAL(0, strcmp(str1, str2));
}

void test_atoul()
{
    const char *str = "12345";
    unsigned long result = atoul(str);
    TEST_ASSERT_EQUAL(12345, result);
}

void test_atoul_whitespace()
{
    const char *str = " 12345 ";
    unsigned long result = atoul(str);
    TEST_ASSERT_EQUAL(12345, result);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_strcmp_different);
    RUN_TEST(test_strcmp_same);
    RUN_TEST(test_atoul);
    RUN_TEST(test_atoul_whitespace);
    return UNITY_END();
}
