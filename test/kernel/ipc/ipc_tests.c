#include <kernel/tasking/task.h>
#include <libs/Unity/src/unity.h>

#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>
#include <kernel/tasking/ipc.h>

#include <stdlib.h>

typedef struct
{
    size_t size;
    uint64_t sender_task_id;
} _ipc_batch_entry_t;

static int _decode_single_message(
    const void *buffer,
    size_t buffer_len,
    const void **payload,
    size_t *payload_size,
    uint64_t *sender_task_id)
{
    if (!buffer || buffer_len < sizeof(_ipc_batch_entry_t))
        return -1;

    const _ipc_batch_entry_t *entry = (const _ipc_batch_entry_t *) buffer;
    size_t total = sizeof(_ipc_batch_entry_t) + entry->size;
    if (buffer_len < total)
        return -1;

    if (payload)
        *payload = entry + 1;
    if (payload_size)
        *payload_size = entry->size;
    if (sender_task_id)
        *sender_task_id = entry->sender_task_id;

    return 0;
}

static task_t _make_task(uint64_t id)
{
    task_t task;
    memset(&task, 0, sizeof(task));
    task.id = id;
    return task;
}

static void _cleanup_channel(task_t *owner, channel_t *channel)
{
    int result = ipc_disconnect(owner, channel);
    if (result != 0) {
        TEST_FAIL_MESSAGE("Failed to cleanup channel");
    }
}

static void test_ipc_new_null_params(void)
{
    task_t owner = _make_task(1);
    TEST_ASSERT_EQUAL(-1, ipc_new_channel(NULL, "ipc_sender"));
    TEST_ASSERT_EQUAL(-1, ipc_new_channel(&owner, NULL));
    TEST_ASSERT_EQUAL(-1, ipc_new_channel(NULL, NULL));
}

static void test_ipc_new_used_channel_name(void)
{
    task_t owner = _make_task(1);
    const char *name = "ipc_sender";
    TEST_ASSERT_EQUAL(0, ipc_new_channel(&owner, name));
    TEST_ASSERT_EQUAL(-1, ipc_new_channel(&owner, name));
    channel_t channel = {0};
    strncpy(channel.name, name, IPC_CHANNEL_NAME_MAX);
    channel.name[IPC_CHANNEL_NAME_MAX - 1] = '\0';
    _cleanup_channel(&owner, &channel);
}

static void test_owner_receive_sets_sender(void)
{
    task_t owner = _make_task(1);
    task_t client = _make_task(2);

    const char *name = "ipc_sender";
    TEST_ASSERT_EQUAL(0, ipc_new_channel(&owner, name));

    channel_t owner_channel = {0};
    strncpy(owner_channel.name, name, IPC_CHANNEL_NAME_MAX);
    owner_channel.name[IPC_CHANNEL_NAME_MAX - 1] = '\0';
    channel_t client_channel = {0};
    TEST_ASSERT_EQUAL(0, ipc_connect(&client, name, &client_channel));

    connection_t pending = {0};
    TEST_ASSERT_EQUAL(0, ipc_await_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL_UINT64(client.id, pending.task_id);
    TEST_ASSERT_EQUAL(0, ipc_accept_connection(&owner, &owner_channel, &pending));

    const char msg[] = "hello";
    TEST_ASSERT_EQUAL(0, ipc_send(&client, &client_channel, (void *) msg, sizeof(msg)));

    char buf[64] = {0};
    connection_t sender = {0};
    size_t bytes_received = ipc_receive(&owner, &owner_channel, &sender, buf, sizeof(buf));
    size_t expected_size = sizeof(_ipc_batch_entry_t) + sizeof(msg);
    TEST_ASSERT_EQUAL(expected_size, bytes_received);

    const void *payload = NULL;
    size_t payload_size = 0;
    uint64_t sender_task_id = 0;
    TEST_ASSERT_EQUAL(
        0, _decode_single_message(buf, bytes_received, &payload, &payload_size, &sender_task_id));
    TEST_ASSERT_EQUAL_UINT64(client.id, sender.task_id);
    TEST_ASSERT_EQUAL_UINT64(client.id, sender_task_id);
    TEST_ASSERT_EQUAL(sizeof(msg), payload_size);
    TEST_ASSERT_EQUAL_STRING(msg, (const char *) payload);

    ipc_disconnect(&client, &client_channel);
    _cleanup_channel(&owner, &owner_channel);
}

static void test_owner_ipc_reaches_all_clients(void)
{
    task_t owner = _make_task(10);
    task_t client1 = _make_task(11);
    task_t client2 = _make_task(12);

    const char *name = "ipc_broadcast";
    TEST_ASSERT_EQUAL(0, ipc_new_channel(&owner, name));

    channel_t owner_channel = {0};
    strncpy(owner_channel.name, name, IPC_CHANNEL_NAME_MAX);
    owner_channel.name[IPC_CHANNEL_NAME_MAX - 1] = '\0';
    channel_t client1_channel = {0};
    channel_t client2_channel = {0};

    TEST_ASSERT_EQUAL(0, ipc_connect(&client1, name, &client1_channel));
    TEST_ASSERT_EQUAL(0, ipc_connect(&client2, name, &client2_channel));

    connection_t pending = {0};
    TEST_ASSERT_EQUAL(0, ipc_await_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL(0, ipc_accept_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL(0, ipc_await_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL(0, ipc_accept_connection(&owner, &owner_channel, &pending));

    const char msg[] = "broadcast";
    TEST_ASSERT_EQUAL(0, ipc_send(&owner, &owner_channel, (void *) msg, sizeof(msg)));

    char buf1[64] = {0};
    char buf2[64] = {0};
    connection_t sender1 = {0};
    connection_t sender2 = {0};
    size_t bytes_received1 = ipc_receive(&client1, &client1_channel, &sender1, buf1, sizeof(buf1));
    size_t bytes_received2 = ipc_receive(&client2, &client2_channel, &sender2, buf2, sizeof(buf2));

    size_t expected_size = sizeof(_ipc_batch_entry_t) + sizeof(msg);
    TEST_ASSERT_EQUAL(expected_size, bytes_received1);
    TEST_ASSERT_EQUAL(expected_size, bytes_received2);

    const void *payload1 = NULL;
    const void *payload2 = NULL;
    size_t payload1_size = 0;
    size_t payload2_size = 0;
    uint64_t sender_task_id1 = 0;
    uint64_t sender_task_id2 = 0;

    TEST_ASSERT_EQUAL(
        0,
        _decode_single_message(buf1, bytes_received1, &payload1, &payload1_size, &sender_task_id1));
    TEST_ASSERT_EQUAL(
        0,
        _decode_single_message(buf2, bytes_received2, &payload2, &payload2_size, &sender_task_id2));

    TEST_ASSERT_EQUAL_UINT64(owner.id, sender1.task_id);
    TEST_ASSERT_EQUAL_UINT64(owner.id, sender2.task_id);
    TEST_ASSERT_EQUAL_UINT64(owner.id, sender_task_id1);
    TEST_ASSERT_EQUAL_UINT64(owner.id, sender_task_id2);
    TEST_ASSERT_EQUAL(sizeof(msg), payload1_size);
    TEST_ASSERT_EQUAL(sizeof(msg), payload2_size);
    TEST_ASSERT_EQUAL_STRING(msg, (const char *) payload1);
    TEST_ASSERT_EQUAL_STRING(msg, (const char *) payload2);

    ipc_disconnect(&client1, &client1_channel);
    ipc_disconnect(&client2, &client2_channel);
    _cleanup_channel(&owner, &owner_channel);
}

static void test_receive_without_messages_returns_one(void)
{
    task_t owner = _make_task(20);
    task_t client = _make_task(21);

    const char *name = "ipc_empty";
    TEST_ASSERT_EQUAL(0, ipc_new_channel(&owner, name));

    channel_t owner_channel = {0};
    strncpy(owner_channel.name, name, IPC_CHANNEL_NAME_MAX);
    owner_channel.name[IPC_CHANNEL_NAME_MAX - 1] = '\0';
    channel_t client_channel = {0};

    TEST_ASSERT_EQUAL(0, ipc_connect(&client, name, &client_channel));

    connection_t pending = {0};
    TEST_ASSERT_EQUAL(0, ipc_await_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL(0, ipc_accept_connection(&owner, &owner_channel, &pending));

    char buf[32] = {0};
    connection_t sender = {.task_id = 999};
    size_t bytes_received = ipc_receive(&client, &client_channel, &sender, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(1, bytes_received);
    TEST_ASSERT_EQUAL_UINT64(999, sender.task_id);

    ipc_disconnect(&client, &client_channel);
    _cleanup_channel(&owner, &owner_channel);
}

static void test_reject_removes_connection(void)
{
    task_t owner = _make_task(30);
    task_t client = _make_task(31);

    const char *name = "ipc_reject";
    TEST_ASSERT_EQUAL(0, ipc_new_channel(&owner, name));

    channel_t owner_channel = {0};
    strncpy(owner_channel.name, name, IPC_CHANNEL_NAME_MAX);
    owner_channel.name[IPC_CHANNEL_NAME_MAX - 1] = '\0';
    channel_t client_channel = {0};
    TEST_ASSERT_EQUAL(0, ipc_connect(&client, name, &client_channel));

    connection_t pending = {0};
    TEST_ASSERT_EQUAL(0, ipc_await_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL(0, ipc_reject_connection(&owner, &owner_channel, &pending));

    const char msg[] = "blocked";
    TEST_ASSERT_EQUAL(-1, ipc_send(&client, &client_channel, (void *) msg, sizeof(msg)));
    TEST_ASSERT_EQUAL(-1, ipc_accept_connection(&owner, &owner_channel, &pending));

    _cleanup_channel(&owner, &owner_channel);
}

static void test_wait_connection_returns_one_when_none(void)
{
    task_t owner = _make_task(40);

    const char *name = "ipc_wait_empty";
    TEST_ASSERT_EQUAL(0, ipc_new_channel(&owner, name));

    channel_t owner_channel = {0};
    strncpy(owner_channel.name, name, IPC_CHANNEL_NAME_MAX);
    owner_channel.name[IPC_CHANNEL_NAME_MAX - 1] = '\0';
    connection_t pending = {0};
    TEST_ASSERT_EQUAL(1, ipc_await_connection(&owner, &owner_channel, &pending));

    _cleanup_channel(&owner, &owner_channel);
}

static void test_client_send_before_accept_fails(void)
{
    task_t owner = _make_task(50);
    task_t client = _make_task(51);

    const char *name = "ipc_pending_send";
    TEST_ASSERT_EQUAL(0, ipc_new_channel(&owner, name));

    channel_t owner_channel = {0};
    strncpy(owner_channel.name, name, IPC_CHANNEL_NAME_MAX);
    owner_channel.name[IPC_CHANNEL_NAME_MAX - 1] = '\0';
    channel_t client_channel = {0};
    TEST_ASSERT_EQUAL(0, ipc_connect(&client, name, &client_channel));

    const char msg[] = "not yet";
    TEST_ASSERT_EQUAL(-1, ipc_send(&client, &client_channel, (void *) msg, sizeof(msg)));

    _cleanup_channel(&owner, &owner_channel);
}

static void test_owner_disconnect_removes_channel(void)
{
    task_t owner = _make_task(60);
    task_t client = _make_task(61);

    const char *name = "ipc_owner_disconnect";
    TEST_ASSERT_EQUAL(0, ipc_new_channel(&owner, name));

    channel_t owner_channel = {0};
    strncpy(owner_channel.name, name, IPC_CHANNEL_NAME_MAX);
    owner_channel.name[IPC_CHANNEL_NAME_MAX - 1] = '\0';
    TEST_ASSERT_EQUAL(0, ipc_disconnect(&owner, &owner_channel));
    channel_t client_channel = {0};
    TEST_ASSERT_EQUAL(-1, ipc_connect(&client, name, &client_channel));
}

static void test_client_disconnect_removes_connection(void)
{
    task_t owner = _make_task(70);
    task_t client = _make_task(71);

    const char *name = "ipc_client_disconnect";
    TEST_ASSERT_EQUAL(0, ipc_new_channel(&owner, name));

    channel_t owner_channel = {0};
    strncpy(owner_channel.name, name, IPC_CHANNEL_NAME_MAX);
    owner_channel.name[IPC_CHANNEL_NAME_MAX - 1] = '\0';
    channel_t client_channel = {0};

    TEST_ASSERT_EQUAL(0, ipc_connect(&client, name, &client_channel));

    connection_t pending = {0};
    TEST_ASSERT_EQUAL(0, ipc_await_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL(0, ipc_accept_connection(&owner, &owner_channel, &pending));

    TEST_ASSERT_EQUAL(0, ipc_disconnect(&client, &client_channel));

    char buf[32] = {0};
    connection_t sender = {0};
    int bytes_written = ipc_receive(&client, &client_channel, &sender, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(-1, bytes_written);

    _cleanup_channel(&owner, &owner_channel);
}

static void test_ipc_task_cleanup_removes_owner_channel(void)
{
    task_t owner = _make_task(80);
    task_t client = _make_task(81);

    const char *name = "ipc_cleanup_owner";
    TEST_ASSERT_EQUAL(0, ipc_new_channel(&owner, name));

    channel_t owner_channel = {0};
    strncpy(owner_channel.name, name, IPC_CHANNEL_NAME_MAX);
    owner_channel.name[IPC_CHANNEL_NAME_MAX - 1] = '\0';
    channel_t client_channel = {0};
    TEST_ASSERT_EQUAL(0, ipc_connect(&client, name, &client_channel));

    connection_t pending = {0};
    TEST_ASSERT_EQUAL(0, ipc_await_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL(0, ipc_accept_connection(&owner, &owner_channel, &pending));

    ipc_task_cleanup(&owner);

    channel_t new_channel = {0};
    TEST_ASSERT_EQUAL(-1, ipc_connect(&client, name, &new_channel));
}

static void test_ipc_task_cleanup_notifies_owner_on_client_disconnect(void)
{
    task_t owner = _make_task(90);
    task_t client = _make_task(91);

    const char *name = "ipc_cleanup_client";
    TEST_ASSERT_EQUAL(0, ipc_new_channel(&owner, name));

    channel_t owner_channel = {0};
    strncpy(owner_channel.name, name, IPC_CHANNEL_NAME_MAX);
    owner_channel.name[IPC_CHANNEL_NAME_MAX - 1] = '\0';
    channel_t client_channel = {0};
    TEST_ASSERT_EQUAL(0, ipc_connect(&client, name, &client_channel));

    connection_t pending = {0};
    TEST_ASSERT_EQUAL(0, ipc_await_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL(0, ipc_accept_connection(&owner, &owner_channel, &pending));

    ipc_task_cleanup(&client);

    char buf[64] = {0};
    connection_t sender = {0};
    int bytes_written = ipc_receive(&owner, &owner_channel, &sender, buf, sizeof(buf));
    TEST_ASSERT_EQUAL((int) (sizeof(_ipc_batch_entry_t) + sizeof(uint32_t)), bytes_written);

    const void *payload = NULL;
    size_t payload_size = 0;
    uint64_t sender_task_id = 0;
    TEST_ASSERT_EQUAL(
        0, _decode_single_message(buf, bytes_written, &payload, &payload_size, &sender_task_id));

    uint32_t disconnect_type = 0;
    memcpy(&disconnect_type, payload, sizeof(disconnect_type));
    TEST_ASSERT_EQUAL_UINT64(client.id, sender.task_id);
    TEST_ASSERT_EQUAL_UINT64(client.id, sender_task_id);
    TEST_ASSERT_EQUAL(sizeof(disconnect_type), payload_size);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, disconnect_type);
    TEST_ASSERT_EQUAL(-1, ipc_send(&client, &client_channel, "x", 1));

    _cleanup_channel(&owner, &owner_channel);
}

void ipc_tests(void)
{
    RUN_TEST(test_ipc_new_null_params);
    RUN_TEST(test_ipc_new_used_channel_name);
    RUN_TEST(test_owner_receive_sets_sender);
    RUN_TEST(test_owner_ipc_reaches_all_clients);
    RUN_TEST(test_receive_without_messages_returns_one);
    RUN_TEST(test_reject_removes_connection);
    RUN_TEST(test_wait_connection_returns_one_when_none);
    RUN_TEST(test_client_send_before_accept_fails);
    RUN_TEST(test_owner_disconnect_removes_channel);
    RUN_TEST(test_client_disconnect_removes_connection);
    RUN_TEST(test_ipc_task_cleanup_removes_owner_channel);
    RUN_TEST(test_ipc_task_cleanup_notifies_owner_on_client_disconnect);
}
