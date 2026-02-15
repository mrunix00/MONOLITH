#include <libs/Unity/src/unity.h>

#include "libc/math_tests.h"
#include "libc/stdio_tests.h"
#include "libc/stdlib_tests.h"
#include "libc/string_tests.h"
#include "libc/ipc_tests.h"

void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    libc_math_tests();
    libc_stdio_tests();
    libc_stdlib_tests();
    libc_string_tests();
    libc_ipc_tests();

    return UNITY_END();
}
