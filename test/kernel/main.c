#include <libs/Unity/src/unity.h>

#include "ipc/ipc_tests.h"
#include "klibc/memory_tests.h"
#include "klibc/string_tests.h"
#include "vfs/vfs_tests.h"

void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    ipc_tests();
    klibc_string_tests();
    klibc_memory_tests();
    vfs_tests();

    return UNITY_END();
}
