#define UNITY_OUTPUT_CHAR unity_output_char

#include <libs/Unity/src/unity.h>

#include <kernel/fs/vfs.h>
#include <kernel/klibc/memory.h>
#include <kernel/klibc/string.h>
#include <kernel/memory/heap.h>

#include <stdlib.h>

static vfs_node_t *_alloc_node(const char *name, file_type_t type)
{
    vfs_node_t *node = kmalloc(sizeof(*node));
    TEST_ASSERT_NOT_NULL(node);
    memset(node, 0, sizeof(*node));
    node->name = (char *) name;
    node->type = type;
    return node;
}

static char _last_open_path[PATH_MAX];
static size_t _last_read_size;
static size_t _last_write_size;
static size_t _last_seek_offset;
static seek_mode_t _last_seek_mode;
static size_t _last_getdents_size;
static size_t _last_create_path_len;
static size_t _last_remove_path_len;
static bool _last_getstats_called;
static bool _last_create_called;
static bool _last_remove_called;
static bool _last_tell_called;
static file_t *_last_file_handle;

static const char _fake_content[] = "hello";
static char _last_write_buffer[32];
static char _last_create_path[PATH_MAX];
static char _last_remove_path[PATH_MAX];

static file_t _fake_open(vfs_drive_t *drive, const char *path)
{
    strncpy(_last_open_path, path, sizeof(_last_open_path) - 1);
    _last_open_path[sizeof(_last_open_path) - 1] = '\0';
    return (file_t) {
        .drive = drive,
        .internal = NULL,
        .offset = 0,
    };
}

static int _fake_read(file_t *file, void *buffer, uint32_t size)
{
    _last_file_handle = file;
    _last_read_size = size;
    size_t to_copy = size;
    if (to_copy > sizeof(_fake_content) - 1)
        to_copy = sizeof(_fake_content) - 1;
    memcpy(buffer, _fake_content, to_copy);
    return (int) to_copy;
}

static int _fake_write(file_t *file, const void *buffer, uint32_t size)
{
    _last_file_handle = file;
    _last_write_size = size;
    size_t to_copy = size;
    if (to_copy > sizeof(_last_write_buffer) - 1)
        to_copy = sizeof(_last_write_buffer) - 1;
    memcpy(_last_write_buffer, buffer, to_copy);
    _last_write_buffer[to_copy] = '\0';
    return (int) size;
}

static int _fake_seek(file_t *file, size_t offset, seek_mode_t mode)
{
    _last_file_handle = file;
    _last_seek_offset = offset;
    _last_seek_mode = mode;
    size_t end = sizeof(_fake_content) - 1;
    if (mode == SEEK_SET) {
        file->offset = offset;
    } else if (mode == SEEK_CUR) {
        file->offset += offset;
    } else if (mode == SEEK_END) {
        file->offset = end + offset;
    }
    return 0;
}

static int _fake_getdents(file_t *file, void *buffer, uint32_t size)
{
    _last_file_handle = file;
    _last_getdents_size = size;
    const char *name = "entry";
    size_t name_len = strlen(name);
    uint32_t entry_len = sizeof(dir_entry_t) + name_len + 1;
    if (size < entry_len)
        return -1;
    dir_entry_t *entry = (dir_entry_t *) buffer;
    entry->length = entry_len;
    entry->type = FILE;
    memcpy(entry->name, name, name_len + 1);
    return (int) entry_len;
}

static int _fake_getstats(file_t *file, file_stats_t *stats)
{
    _last_file_handle = file;
    _last_getstats_called = true;
    stats->size = 123;
    stats->type = FILE;
    return 0;
}

static int _fake_create(vfs_drive_t *drive, const char *name, file_type_t type)
{
    (void) drive;
    _last_create_called = true;
    _last_create_path_len = strlen(name);
    strncpy(_last_create_path, name, sizeof(_last_create_path) - 1);
    _last_create_path[sizeof(_last_create_path) - 1] = '\0';
    return type == DIRECTORY ? -1 : 0;
}

static int _fake_remove(vfs_drive_t *drive, const char *name)
{
    (void) drive;
    _last_remove_called = true;
    _last_remove_path_len = strlen(name);
    strncpy(_last_remove_path, name, sizeof(_last_remove_path) - 1);
    _last_remove_path[sizeof(_last_remove_path) - 1] = '\0';
    return 0;
}

static size_t _fake_tell(file_t *file)
{
    _last_file_handle = file;
    _last_tell_called = true;
    return file->offset;
}

static void _reset_fake_io_state(void)
{
    _last_read_size = 0;
    _last_write_size = 0;
    _last_seek_offset = 0;
    _last_seek_mode = SEEK_SET;
    _last_getdents_size = 0;
    _last_create_path_len = 0;
    _last_remove_path_len = 0;
    _last_getstats_called = false;
    _last_create_called = false;
    _last_remove_called = false;
    _last_tell_called = false;
    _last_file_handle = NULL;
    _last_write_buffer[0] = '\0';
    _last_create_path[0] = '\0';
    _last_remove_path[0] = '\0';
}

static void test_vfs_new_drive_unique_names(void)
{
    vfs_drive_t *drive_a = vfs_new_drive("disk");
    vfs_drive_t *drive_b = vfs_new_drive("disk");
    vfs_drive_t *drive_c = vfs_new_drive("disk");

    TEST_ASSERT_NOT_NULL(drive_a);
    TEST_ASSERT_NOT_NULL(drive_b);
    TEST_ASSERT_NOT_NULL(drive_c);
    TEST_ASSERT_EQUAL_STRING("disk", drive_a->name);
    TEST_ASSERT_EQUAL_STRING("disk0", drive_b->name);
    TEST_ASSERT_EQUAL_STRING("disk1", drive_c->name);

    vfs_remove_drive(drive_a);
    vfs_remove_drive(drive_b);
    vfs_remove_drive(drive_c);
}

static void test_vfs_remove_drive_by_name_allows_reuse(void)
{
    vfs_drive_t *drive = vfs_new_drive("data");
    TEST_ASSERT_NOT_NULL(drive);

    vfs_remove_drive_by_name("data");

    vfs_drive_t *drive_again = vfs_new_drive("data");
    TEST_ASSERT_NOT_NULL(drive_again);
    TEST_ASSERT_EQUAL_STRING("data", drive_again->name);

    vfs_remove_drive(drive_again);
}

static void test_vfs_getdrives_lists_entries(void)
{
    vfs_drive_t *drive_a = vfs_new_drive("alpha");
    vfs_drive_t *drive_b = vfs_new_drive("beta");

    TEST_ASSERT_NOT_NULL(drive_a);
    TEST_ASSERT_NOT_NULL(drive_b);

    char buffer[256];
    int bytes = vfs_getdrives(buffer, sizeof(buffer));

    TEST_ASSERT_TRUE(bytes > 0);

    uint32_t offset = 0;
    dir_entry_t *entry_a = (dir_entry_t *) (buffer + offset);
    TEST_ASSERT_EQUAL(DIRECTORY, entry_a->type);
    TEST_ASSERT_EQUAL_STRING("alpha", entry_a->name);
    TEST_ASSERT_EQUAL_UINT32(sizeof(dir_entry_t) + strlen("alpha") + 1, entry_a->length);
    offset += entry_a->length;

    dir_entry_t *entry_b = (dir_entry_t *) (buffer + offset);
    TEST_ASSERT_EQUAL(DIRECTORY, entry_b->type);
    TEST_ASSERT_EQUAL_STRING("beta", entry_b->name);
    TEST_ASSERT_EQUAL_UINT32(sizeof(dir_entry_t) + strlen("beta") + 1, entry_b->length);
    offset += entry_b->length;

    TEST_ASSERT_EQUAL_INT(offset, bytes);

    vfs_remove_drive(drive_a);
    vfs_remove_drive(drive_b);
}

static void test_vfs_add_and_remove_child_updates_links(void)
{
    vfs_node_t parent = {
        .name = "root",
        .type = DIRECTORY,
        .parent = NULL,
        .child = NULL,
        .sibling = NULL,
        .internal = NULL,
    };

    vfs_node_t *child_a = _alloc_node("a", FILE);
    vfs_node_t *child_b = _alloc_node("b", FILE);

    vfs_add_child(&parent, child_a);
    vfs_add_child(&parent, child_b);

    TEST_ASSERT_EQUAL_PTR(child_a, parent.child);
    TEST_ASSERT_EQUAL_PTR(child_b, child_a->sibling);

    vfs_remove_child(&parent, child_a);
    TEST_ASSERT_EQUAL_PTR(child_b, parent.child);
    TEST_ASSERT_NULL(child_b->sibling);

    vfs_remove_child(&parent, child_b);
    TEST_ASSERT_NULL(parent.child);
}

static void test_vfs_get_relative_path_handles_slashes(void)
{
    vfs_node_t root = {
        .name = "",
        .type = DIRECTORY,
        .parent = NULL,
        .child = NULL,
        .sibling = NULL,
        .internal = NULL,
    };
    vfs_node_t etc = {
        .name = "etc",
        .type = DIRECTORY,
        .parent = NULL,
        .child = NULL,
        .sibling = NULL,
        .internal = NULL,
    };
    vfs_node_t hosts = {
        .name = "hosts",
        .type = FILE,
        .parent = NULL,
        .child = NULL,
        .sibling = NULL,
        .internal = NULL,
    };

    vfs_add_child(&root, &etc);
    vfs_add_child(&etc, &hosts);

    TEST_ASSERT_EQUAL_PTR(&hosts, vfs_get_relative_path(&root, "/etc/hosts"));
    TEST_ASSERT_EQUAL_PTR(&hosts, vfs_get_relative_path(&root, "/etc//hosts/"));
    TEST_ASSERT_NULL(vfs_get_relative_path(&root, "/etc/missing"));

    vfs_node_t file = {
        .name = "file",
        .type = FILE,
        .parent = NULL,
        .child = NULL,
        .sibling = NULL,
        .internal = NULL,
    };
    TEST_ASSERT_NULL(vfs_get_relative_path(&file, "/etc"));
}

static void test_file_open_root_path_uses_drive_callback(void)
{
    vfs_drive_t *drive = vfs_new_drive("dev");
    TEST_ASSERT_NOT_NULL(drive);
    drive->open = _fake_open;
    _last_open_path[0] = '\0';

    file_t file = file_open("dev:/");
    TEST_ASSERT_EQUAL_PTR(drive, file.drive);
    TEST_ASSERT_EQUAL_STRING("/", _last_open_path);

    vfs_remove_drive(drive);
}

static void test_file_open_invalid_path_returns_empty_handle(void)
{
    file_t file = file_open("invalid-path");
    TEST_ASSERT_NULL(file.drive);
    TEST_ASSERT_NULL(file.internal);
    TEST_ASSERT_EQUAL_UINT32(0, file.offset);
}

static void test_file_io_calls_drive_callbacks(void)
{
    _reset_fake_io_state();
    vfs_drive_t *drive = vfs_new_drive("io");
    TEST_ASSERT_NOT_NULL(drive);
    drive->open = _fake_open;
    drive->read = _fake_read;
    drive->write = _fake_write;
    drive->seek = _fake_seek;
    drive->tell = _fake_tell;

    file_t file = file_open("io:/file");
    TEST_ASSERT_EQUAL_PTR(drive, file.drive);

    const char payload[] = "abc";
    TEST_ASSERT_EQUAL_INT(sizeof(payload), file_write(&file, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL_UINT32(sizeof(payload), (uint32_t) _last_write_size);
    TEST_ASSERT_EQUAL_STRING("abc", _last_write_buffer);
    TEST_ASSERT_EQUAL_PTR(&file, _last_file_handle);

    char buffer[8] = {0};
    TEST_ASSERT_EQUAL_INT(5, file_read(&file, buffer, 5));
    TEST_ASSERT_EQUAL_UINT32(5, (uint32_t) _last_read_size);
    TEST_ASSERT_EQUAL_STRING("hello", buffer);

    TEST_ASSERT_EQUAL(0, file_seek(&file, 2, SEEK_SET));
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t) _last_seek_offset);
    TEST_ASSERT_EQUAL(SEEK_SET, _last_seek_mode);
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t) file_tell(&file));
    TEST_ASSERT_TRUE(_last_tell_called);

    TEST_ASSERT_EQUAL(0, file_seek(&file, 1, SEEK_CUR));
    TEST_ASSERT_EQUAL_UINT32(3, (uint32_t) file_tell(&file));

    TEST_ASSERT_EQUAL(0, file_seek(&file, 0, SEEK_END));
    TEST_ASSERT_EQUAL_UINT32(5, (uint32_t) file_tell(&file));

    vfs_remove_drive(drive);
}

static void test_file_getdents_and_getstats(void)
{
    _reset_fake_io_state();
    vfs_drive_t *drive = vfs_new_drive("dir");
    TEST_ASSERT_NOT_NULL(drive);
    drive->open = _fake_open;
    drive->getdents = _fake_getdents;
    drive->getstats = _fake_getstats;

    file_t file = file_open("dir:/");
    char buffer[64];
    int bytes = file_getdents(&file, buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(bytes > 0);
    TEST_ASSERT_EQUAL_UINT32(sizeof(buffer), (uint32_t) _last_getdents_size);
    dir_entry_t *entry = (dir_entry_t *) buffer;
    TEST_ASSERT_EQUAL(FILE, entry->type);
    TEST_ASSERT_EQUAL_STRING("entry", entry->name);

    file_stats_t stats = {0};
    TEST_ASSERT_EQUAL(0, file_getstats(&file, &stats));
    TEST_ASSERT_TRUE(_last_getstats_called);
    TEST_ASSERT_EQUAL_UINT64(123, stats.size);
    TEST_ASSERT_EQUAL(FILE, stats.type);

    vfs_remove_drive(drive);
}

static void test_file_create_and_remove(void)
{
    _reset_fake_io_state();
    vfs_drive_t *drive = vfs_new_drive("ops");
    TEST_ASSERT_NOT_NULL(drive);
    drive->create = _fake_create;
    drive->remove = _fake_remove;

    TEST_ASSERT_EQUAL(0, file_create("ops:/file.txt", FILE));
    TEST_ASSERT_TRUE(_last_create_called);
    TEST_ASSERT_EQUAL_STRING("/file.txt", _last_create_path);
    TEST_ASSERT_EQUAL_UINT32(9, (uint32_t) _last_create_path_len);

    TEST_ASSERT_EQUAL(-1, file_create("ops:/dir", DIRECTORY));
    TEST_ASSERT_TRUE(_last_create_called);

    TEST_ASSERT_EQUAL(0, file_remove("ops:/file.txt"));
    TEST_ASSERT_TRUE(_last_remove_called);
    TEST_ASSERT_EQUAL_STRING("/file.txt", _last_remove_path);
    TEST_ASSERT_EQUAL_UINT32(9, (uint32_t) _last_remove_path_len);

    vfs_remove_drive(drive);
}

void vfs_tests(void)
{
    RUN_TEST(test_vfs_new_drive_unique_names);
    RUN_TEST(test_vfs_remove_drive_by_name_allows_reuse);
    RUN_TEST(test_vfs_getdrives_lists_entries);
    RUN_TEST(test_vfs_add_and_remove_child_updates_links);
    RUN_TEST(test_vfs_get_relative_path_handles_slashes);
    RUN_TEST(test_file_open_root_path_uses_drive_callback);
    RUN_TEST(test_file_open_invalid_path_returns_empty_handle);
    RUN_TEST(test_file_io_calls_drive_callbacks);
    RUN_TEST(test_file_getdents_and_getstats);
    RUN_TEST(test_file_create_and_remove);
}
