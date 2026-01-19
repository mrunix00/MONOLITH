#include <kernel/tasking/task.h>
#include <libs/Unity/src/unity.h>

#include <kernel/klibc/memory.h>
#include <kernel/memory/heap.h>
#include <kernel/tasking/ipc.h>

#include <stdlib.h>

static task_t _make_task(uint64_t id)
{
    task_t task;
    memset(&task, 0, sizeof(task));
    task.id = id;
    return task;
}

static void _cleanup_channel(task_t *owner, channel_t *channel)
{
    int result = broadcast_disconnect(owner, channel);
    if (result != 0) {
        TEST_FAIL_MESSAGE("Failed to cleanup channel");
    }
}

static void test_broadcast_new_null_params(void)
{
    task_t owner = _make_task(1);
    TEST_ASSERT_EQUAL(-1, broadcast_new(NULL, "ipc_sender"));
    TEST_ASSERT_EQUAL(-1, broadcast_new(&owner, NULL));
    TEST_ASSERT_EQUAL(-1, broadcast_new(NULL, NULL));
}

static void test_broadcast_new_used_channel_name(void)
{
    task_t owner = _make_task(1);
    const char *name = "ipc_sender";
    TEST_ASSERT_EQUAL(0, broadcast_new(&owner, name));
    TEST_ASSERT_EQUAL(-1, broadcast_new(&owner, name));
    channel_t channel = {.name = (char *) name};
    _cleanup_channel(&owner, &channel);
}

static void test_owner_receive_sets_sender(void)
{
    task_t owner = _make_task(1);
    task_t client = _make_task(2);

    const char *name = "ipc_sender";
    TEST_ASSERT_EQUAL(0, broadcast_new(&owner, name));

    channel_t owner_channel = {.name = (char *) name};
    channel_t client_channel = {0};
    TEST_ASSERT_EQUAL(0, broadcast_request_connection(&client, name, &client_channel));

    connection_t pending = {0};
    TEST_ASSERT_EQUAL(0, broadcast_wait_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL_UINT64(client.id, pending.task_id);
    TEST_ASSERT_EQUAL(0, broadcast_accept_connection(&owner, &owner_channel, &pending));

    const char msg[] = "hello";
    TEST_ASSERT_EQUAL(0, broadcast_send(&client, &client_channel, (void *) msg, sizeof(msg)));

    char buf[16] = {0};
    connection_t sender = {0};
    TEST_ASSERT_EQUAL(0, broadcast_receive(&owner, &owner_channel, &sender, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT64(client.id, sender.task_id);
    TEST_ASSERT_EQUAL_STRING(msg, buf);

    broadcast_disconnect(&client, &client_channel);
    _cleanup_channel(&owner, &owner_channel);
}

static void test_owner_broadcast_reaches_all_clients(void)
{
    task_t owner = _make_task(10);
    task_t client1 = _make_task(11);
    task_t client2 = _make_task(12);

    const char *name = "ipc_broadcast";
    TEST_ASSERT_EQUAL(0, broadcast_new(&owner, name));

    channel_t owner_channel = {.name = (char *) name};
    channel_t client1_channel = {0};
    channel_t client2_channel = {0};

    TEST_ASSERT_EQUAL(0, broadcast_request_connection(&client1, name, &client1_channel));
    TEST_ASSERT_EQUAL(0, broadcast_request_connection(&client2, name, &client2_channel));

    connection_t pending = {0};
    TEST_ASSERT_EQUAL(0, broadcast_wait_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL(0, broadcast_accept_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL(0, broadcast_wait_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL(0, broadcast_accept_connection(&owner, &owner_channel, &pending));

    const char msg[] = "broadcast";
    TEST_ASSERT_EQUAL(0, broadcast_send(&owner, &owner_channel, (void *) msg, sizeof(msg)));

    char buf1[32] = {0};
    char buf2[32] = {0};
    connection_t sender1 = {0};
    connection_t sender2 = {0};

    TEST_ASSERT_EQUAL(0, broadcast_receive(&client1, &client1_channel, &sender1, buf1, sizeof(buf1)));
    TEST_ASSERT_EQUAL(0, broadcast_receive(&client2, &client2_channel, &sender2, buf2, sizeof(buf2)));
    TEST_ASSERT_EQUAL_UINT64(owner.id, sender1.task_id);
    TEST_ASSERT_EQUAL_UINT64(owner.id, sender2.task_id);
    TEST_ASSERT_EQUAL_STRING(msg, buf1);
    TEST_ASSERT_EQUAL_STRING(msg, buf2);

    broadcast_disconnect(&client1, &client1_channel);
    broadcast_disconnect(&client2, &client2_channel);
    _cleanup_channel(&owner, &owner_channel);
}

static void test_receive_without_messages_returns_one(void)
{
    task_t owner = _make_task(20);
    task_t client = _make_task(21);

    const char *name = "ipc_empty";
    TEST_ASSERT_EQUAL(0, broadcast_new(&owner, name));

    channel_t owner_channel = {.name = (char *) name};
    channel_t client_channel = {0};

    TEST_ASSERT_EQUAL(0, broadcast_request_connection(&client, name, &client_channel));

    connection_t pending = {0};
    TEST_ASSERT_EQUAL(0, broadcast_wait_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL(0, broadcast_accept_connection(&owner, &owner_channel, &pending));

    char buf[8] = {0};
    connection_t sender = {.task_id = 999};
    TEST_ASSERT_EQUAL(1, broadcast_receive(&client, &client_channel, &sender, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT64(999, sender.task_id);

    broadcast_disconnect(&client, &client_channel);
    _cleanup_channel(&owner, &owner_channel);
}

static void test_reject_removes_connection(void)
{
    task_t owner = _make_task(30);
    task_t client = _make_task(31);

    const char *name = "ipc_reject";
    TEST_ASSERT_EQUAL(0, broadcast_new(&owner, name));

    channel_t owner_channel = {.name = (char *) name};
    channel_t client_channel = {0};
    TEST_ASSERT_EQUAL(0, broadcast_request_connection(&client, name, &client_channel));

    connection_t pending = {0};
    TEST_ASSERT_EQUAL(0, broadcast_wait_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL(0, broadcast_reject_connection(&owner, &owner_channel, &pending));

    const char msg[] = "blocked";
    TEST_ASSERT_EQUAL(-1, broadcast_send(&client, &client_channel, (void *) msg, sizeof(msg)));
    TEST_ASSERT_EQUAL(-1, broadcast_accept_connection(&owner, &owner_channel, &pending));

    _cleanup_channel(&owner, &owner_channel);
}

static void test_wait_connection_returns_one_when_none(void)
{
    task_t owner = _make_task(40);

    const char *name = "ipc_wait_empty";
    TEST_ASSERT_EQUAL(0, broadcast_new(&owner, name));

    channel_t owner_channel = {.name = (char *) name};
    connection_t pending = {0};
    TEST_ASSERT_EQUAL(1, broadcast_wait_connection(&owner, &owner_channel, &pending));

    _cleanup_channel(&owner, &owner_channel);
}

static void test_client_send_before_accept_fails(void)
{
    task_t owner = _make_task(50);
    task_t client = _make_task(51);

    const char *name = "ipc_pending_send";
    TEST_ASSERT_EQUAL(0, broadcast_new(&owner, name));

    channel_t owner_channel = {.name = (char *) name};
    channel_t client_channel = {0};
    TEST_ASSERT_EQUAL(0, broadcast_request_connection(&client, name, &client_channel));

    const char msg[] = "not yet";
    TEST_ASSERT_EQUAL(-1, broadcast_send(&client, &client_channel, (void *) msg, sizeof(msg)));

    _cleanup_channel(&owner, &owner_channel);
}

static void test_owner_disconnect_removes_channel(void)
{
    task_t owner = _make_task(60);
    task_t client = _make_task(61);

    const char *name = "ipc_owner_disconnect";
    TEST_ASSERT_EQUAL(0, broadcast_new(&owner, name));

    channel_t owner_channel = {.name = (char *) name};
    TEST_ASSERT_EQUAL(0, broadcast_disconnect(&owner, &owner_channel));
    channel_t client_channel = {0};
    TEST_ASSERT_EQUAL(-1, broadcast_request_connection(&client, name, &client_channel));
}

static void test_client_disconnect_removes_connection(void)
{
    task_t owner = _make_task(70);
    task_t client = _make_task(71);

    const char *name = "ipc_client_disconnect";
    TEST_ASSERT_EQUAL(0, broadcast_new(&owner, name));

    channel_t owner_channel = {.name = (char *) name};
    channel_t client_channel = {0};

    TEST_ASSERT_EQUAL(0, broadcast_request_connection(&client, name, &client_channel));

    connection_t pending = {0};
    TEST_ASSERT_EQUAL(0, broadcast_wait_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL(0, broadcast_accept_connection(&owner, &owner_channel, &pending));

    TEST_ASSERT_EQUAL(0, broadcast_disconnect(&client, &client_channel));

    char buf[8] = {0};
    connection_t sender = {0};
    TEST_ASSERT_EQUAL(-1, broadcast_receive(&client, &client_channel, &sender, buf, sizeof(buf)));

    _cleanup_channel(&owner, &owner_channel);
}

static void test_ipc_task_cleanup_removes_owner_channel(void)
{
    task_t owner = _make_task(80);
    task_t client = _make_task(81);

    const char *name = "ipc_cleanup_owner";
    TEST_ASSERT_EQUAL(0, broadcast_new(&owner, name));

    channel_t owner_channel = {.name = (char *) name};
    channel_t client_channel = {0};
    TEST_ASSERT_EQUAL(0, broadcast_request_connection(&client, name, &client_channel));

    connection_t pending = {0};
    TEST_ASSERT_EQUAL(0, broadcast_wait_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL(0, broadcast_accept_connection(&owner, &owner_channel, &pending));

    ipc_task_cleanup(&owner);

    channel_t new_channel = {0};
    TEST_ASSERT_EQUAL(-1, broadcast_request_connection(&client, name, &new_channel));
}

static void test_ipc_task_cleanup_notifies_owner_on_client_disconnect(void)
{
    task_t owner = _make_task(90);
    task_t client = _make_task(91);

    const char *name = "ipc_cleanup_client";
    TEST_ASSERT_EQUAL(0, broadcast_new(&owner, name));

    channel_t owner_channel = {.name = (char *) name};
    channel_t client_channel = {0};
    TEST_ASSERT_EQUAL(0, broadcast_request_connection(&client, name, &client_channel));

    connection_t pending = {0};
    TEST_ASSERT_EQUAL(0, broadcast_wait_connection(&owner, &owner_channel, &pending));
    TEST_ASSERT_EQUAL(0, broadcast_accept_connection(&owner, &owner_channel, &pending));

    ipc_task_cleanup(&client);

    uint32_t disconnect_type = 0;
    connection_t sender = {0};
    TEST_ASSERT_EQUAL(0, broadcast_receive(&owner, &owner_channel, &sender, &disconnect_type,
                                          sizeof(disconnect_type)));
    TEST_ASSERT_EQUAL_UINT64(client.id, sender.task_id);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, disconnect_type);
    TEST_ASSERT_EQUAL(-1, broadcast_send(&client, &client_channel, "x", 1));

    _cleanup_channel(&owner, &owner_channel);
}

void ipc_tests(void)
{
    RUN_TEST(test_broadcast_new_null_params);
    RUN_TEST(test_broadcast_new_used_channel_name);
    RUN_TEST(test_owner_receive_sets_sender);
    RUN_TEST(test_owner_broadcast_reaches_all_clients);
    RUN_TEST(test_receive_without_messages_returns_one);
    RUN_TEST(test_reject_removes_connection);
    RUN_TEST(test_wait_connection_returns_one_when_none);
    RUN_TEST(test_client_send_before_accept_fails);
    RUN_TEST(test_owner_disconnect_removes_channel);
    RUN_TEST(test_client_disconnect_removes_connection);
    RUN_TEST(test_ipc_task_cleanup_removes_owner_channel);
    RUN_TEST(test_ipc_task_cleanup_notifies_owner_on_client_disconnect);
}
