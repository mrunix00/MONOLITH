#include <libs/Unity/src/unity.h>

#include <kernel/klibc/memory.h>

void setUp(void) {}
void tearDown(void) {}

void test_memset(void)
{
    char buffer[21];
    memset(buffer, 'A', 20);
    buffer[20] = '\0';
    TEST_ASSERT_EQUAL_STRING("AAAAAAAAAAAAAAAAAAAA", buffer);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_memset);
    return UNITY_END();
}
