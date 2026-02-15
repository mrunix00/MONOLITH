/*
 * Userspace IPC API tests (batched receive + cache behavior).
 */

#include <libs/Unity/src/unity.h>
#include <ipc.h>
#include <string.h>
#include <stdint.h>
#include <sys/syscall.h>

typedef struct
{
    size_t size;
    uint64_t sender_task_id;
} _ipc_batch_entry_t;

typedef int (*ipc_syscall4_fn)(long num, long arg1, long arg2, long arg3, long arg4);
extern ipc_syscall4_fn ipc_test_syscall4;
extern void ipc_test_reset_cache(void);

static int g_syscall_calls = 0;
static int g_stub_mode = 0;
static int g_last_syscall_num = 0;
static void *g_last_channel_ptr = NULL;

static size_t build_batch(void *buffer,
                          const void *msg1, size_t msg1_len, uint64_t sender1,
                          const void *msg2, size_t msg2_len, uint64_t sender2)
{
    size_t offset = 0;
    if (msg1 && msg1_len > 0) {
        _ipc_batch_entry_t *entry = (_ipc_batch_entry_t *) ((char *) buffer + offset);
        entry->size = msg1_len;
        entry->sender_task_id = sender1;
        memcpy(entry + 1, msg1, msg1_len);
        offset += sizeof(_ipc_batch_entry_t) + msg1_len;
    }
    if (msg2 && msg2_len > 0) {
        _ipc_batch_entry_t *entry = (_ipc_batch_entry_t *) ((char *) buffer + offset);
        entry->size = msg2_len;
        entry->sender_task_id = sender2;
        memcpy(entry + 1, msg2, msg2_len);
        offset += sizeof(_ipc_batch_entry_t) + msg2_len;
    }
    return offset;
}

static int ipc_syscall_stub(long num, long arg1, long arg2, long arg3, long arg4)
{
    g_syscall_calls++;
    g_last_syscall_num = (int) num;
    g_last_channel_ptr = (void *) arg1;

    if (!arg1 || !arg2 || !arg3 || arg4 <= 0)
        return -1;

    if (g_stub_mode == 0) {
        const char *m1 = "hello";
        const char *m2 = "world";
        size_t m1_len = strlen(m1) + 1;
        size_t m2_len = strlen(m2) + 1;
        size_t needed = sizeof(_ipc_batch_entry_t) * 2 + m1_len + m2_len;
        if ((size_t) arg4 < needed)
            return -1;
        size_t len = build_batch((void *) arg3, m1, m1_len, 11, m2, m2_len, 22);
        return (int) len;
    }

    if (g_stub_mode == 1) {
        if (g_syscall_calls == 1) {
            const char *m1 = "one";
            size_t m1_len = strlen(m1) + 1;
            size_t needed = sizeof(_ipc_batch_entry_t) + m1_len;
            if ((size_t) arg4 < needed)
                return -1;
            size_t len = build_batch((void *) arg3, m1, m1_len, 111, NULL, 0, 0);
            return (int) len;
        }
        const char *m2 = "two";
        size_t m2_len = strlen(m2) + 1;
        size_t needed = sizeof(_ipc_batch_entry_t) + m2_len;
        if ((size_t) arg4 < needed)
            return -1;
        size_t len = build_batch((void *) arg3, m2, m2_len, 222, NULL, 0, 0);
        return (int) len;
    }

    if (g_stub_mode == 2) {
        return 1;
    }

    return -1;
}

static void reset_test_state(void)
{
    g_syscall_calls = 0;
    g_stub_mode = 0;
    g_last_syscall_num = 0;
    g_last_channel_ptr = NULL;
    ipc_test_syscall4 = ipc_syscall_stub;
    ipc_test_reset_cache();
}

static channel_t make_channel(const char *name, uint64_t owner_task_id)
{
    channel_t channel;
    memset(&channel, 0, sizeof(channel));
    strncpy(channel.name, name, IPC_CHANNEL_NAME_MAX);
    channel.name[IPC_CHANNEL_NAME_MAX - 1] = '\0';
    channel.owner_task_id = owner_task_id;
    return channel;
}

static void test_ipc_receive_batches_and_caches(void)
{
    reset_test_state();

    channel_t channel = make_channel("chanA", 100);
    connection_t sender = {0};
    char buffer[64];

    int result = ipc_receive(&channel, &sender, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_STRING("hello", buffer);
    TEST_ASSERT_EQUAL_UINT64(11, sender.task_id);
    TEST_ASSERT_EQUAL(1, g_syscall_calls);
    TEST_ASSERT_EQUAL(SYSCALL_IPC_RECEIVE_ALL, g_last_syscall_num);
    TEST_ASSERT_EQUAL_PTR(&channel, g_last_channel_ptr);

    memset(buffer, 0, sizeof(buffer));
    result = ipc_receive(&channel, &sender, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_STRING("world", buffer);
    TEST_ASSERT_EQUAL_UINT64(22, sender.task_id);
    TEST_ASSERT_EQUAL(1, g_syscall_calls);
}

static void test_ipc_receive_refills_after_cache_empty(void)
{
    reset_test_state();
    g_stub_mode = 1;

    channel_t channel = make_channel("chanB", 200);
    connection_t sender = {0};
    char buffer[64];

    int result = ipc_receive(&channel, &sender, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_STRING("one", buffer);
    TEST_ASSERT_EQUAL_UINT64(111, sender.task_id);
    TEST_ASSERT_EQUAL(1, g_syscall_calls);

    memset(buffer, 0, sizeof(buffer));
    result = ipc_receive(&channel, &sender, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_STRING("two", buffer);
    TEST_ASSERT_EQUAL_UINT64(222, sender.task_id);
    TEST_ASSERT_EQUAL(2, g_syscall_calls);
}

static void test_ipc_receive_cache_resets_on_channel_change(void)
{
    reset_test_state();

    channel_t channel_a = make_channel("chanA", 300);
    channel_t channel_b = make_channel("chanB", 400);
    connection_t sender = {0};
    char buffer[64];

    int result = ipc_receive(&channel_a, &sender, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(1, g_syscall_calls);
    TEST_ASSERT_EQUAL_PTR(&channel_a, g_last_channel_ptr);

    memset(buffer, 0, sizeof(buffer));
    result = ipc_receive(&channel_b, &sender, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(2, g_syscall_calls);
    TEST_ASSERT_EQUAL_PTR(&channel_b, g_last_channel_ptr);
}

static void test_ipc_receive_no_message_returns_one(void)
{
    reset_test_state();
    g_stub_mode = 2;

    channel_t channel = make_channel("chanC", 500);
    connection_t sender = {0};
    char buffer[64] = {0};

    int result = ipc_receive(&channel, &sender, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL(1, g_syscall_calls);
}

static void test_ipc_receive_invalid_args(void)
{
    reset_test_state();

    channel_t channel = make_channel("chanD", 600);
    connection_t sender = {0};
    char buffer[8];

    int result = ipc_receive(NULL, &sender, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(-1, result);
    TEST_ASSERT_EQUAL(0, g_syscall_calls);

    result = ipc_receive(&channel, NULL, NULL, sizeof(buffer));
    TEST_ASSERT_EQUAL(-1, result);
    TEST_ASSERT_EQUAL(0, g_syscall_calls);

    result = ipc_receive(&channel, &sender, buffer, 0);
    TEST_ASSERT_EQUAL(-1, result);
    TEST_ASSERT_EQUAL(0, g_syscall_calls);
}

void libc_ipc_tests(void)
{
    RUN_TEST(test_ipc_receive_batches_and_caches);
    RUN_TEST(test_ipc_receive_refills_after_cache_empty);
    RUN_TEST(test_ipc_receive_cache_resets_on_channel_change);
    RUN_TEST(test_ipc_receive_no_message_returns_one);
    RUN_TEST(test_ipc_receive_invalid_args);
}
