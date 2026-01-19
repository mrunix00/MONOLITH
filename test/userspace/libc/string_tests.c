#include <libs/Unity/src/unity.h>
#include <string.h>

/* strlen tests */
static void test_strlen_empty(void)
{
    TEST_ASSERT_EQUAL(0, strlen(""));
}

static void test_strlen_simple(void)
{
    TEST_ASSERT_EQUAL(5, strlen("hello"));
}

static void test_strlen_with_spaces(void)
{
    TEST_ASSERT_EQUAL(11, strlen("hello world"));
}

/* strnlen tests */
static void test_strnlen_within_limit(void)
{
    TEST_ASSERT_EQUAL(5, strnlen("hello", 10));
}

static void test_strnlen_at_limit(void)
{
    TEST_ASSERT_EQUAL(5, strnlen("hello", 5));
}

static void test_strnlen_exceeds_limit(void)
{
    TEST_ASSERT_EQUAL(3, strnlen("hello", 3));
}

/* strcmp tests */
static void test_strcmp_equal(void)
{
    TEST_ASSERT_EQUAL(0, strcmp("hello", "hello"));
}

static void test_strcmp_less(void)
{
    TEST_ASSERT_TRUE(strcmp("abc", "abd") < 0);
}

static void test_strcmp_greater(void)
{
    TEST_ASSERT_TRUE(strcmp("abd", "abc") > 0);
}

static void test_strcmp_empty(void)
{
    TEST_ASSERT_EQUAL(0, strcmp("", ""));
}

static void test_strcmp_prefix(void)
{
    TEST_ASSERT_TRUE(strcmp("abc", "abcd") < 0);
}

/* strncmp tests */
static void test_strncmp_equal_within_n(void)
{
    TEST_ASSERT_EQUAL(0, strncmp("hello", "hello", 5));
}

static void test_strncmp_equal_partial(void)
{
    TEST_ASSERT_EQUAL(0, strncmp("hello", "hella", 4));
}

static void test_strncmp_diff_within_n(void)
{
    TEST_ASSERT_TRUE(strncmp("hello", "hella", 5) > 0);
}

static void test_strncmp_zero_length(void)
{
    TEST_ASSERT_EQUAL(0, strncmp("abc", "xyz", 0));
}

/* strcpy tests */
static void test_strcpy_simple(void)
{
    char dest[20];
    char *result = strcpy(dest, "hello");
    TEST_ASSERT_EQUAL_STRING("hello", dest);
    TEST_ASSERT_EQUAL_PTR(dest, result);
}

static void test_strcpy_empty(void)
{
    char dest[20] = "initial";
    strcpy(dest, "");
    TEST_ASSERT_EQUAL_STRING("", dest);
}

/* strncpy tests */
static void test_strncpy_full_copy(void)
{
    char dest[20];
    strncpy(dest, "hello", 10);
    TEST_ASSERT_EQUAL_STRING("hello", dest);
}

static void test_strncpy_truncated(void)
{
    char dest[20];
    strncpy(dest, "hello", 3);
    TEST_ASSERT_EQUAL_MEMORY("hel", dest, 3);
}

static void test_strncpy_padded(void)
{
    char dest[10];
    memset(dest, 'X', sizeof(dest));
    strncpy(dest, "hi", 5);
    TEST_ASSERT_EQUAL_STRING("hi", dest);
    TEST_ASSERT_EQUAL('\0', dest[2]);
    TEST_ASSERT_EQUAL('\0', dest[3]);
    TEST_ASSERT_EQUAL('\0', dest[4]);
}

/* strcat tests */
static void test_strcat_simple(void)
{
    char dest[20] = "hello";
    char *result = strcat(dest, " world");
    TEST_ASSERT_EQUAL_STRING("hello world", dest);
    TEST_ASSERT_EQUAL_PTR(dest, result);
}

static void test_strcat_empty_src(void)
{
    char dest[20] = "hello";
    strcat(dest, "");
    TEST_ASSERT_EQUAL_STRING("hello", dest);
}

static void test_strcat_empty_dest(void)
{
    char dest[20] = "";
    strcat(dest, "hello");
    TEST_ASSERT_EQUAL_STRING("hello", dest);
}

/* memset tests */
static void test_memset_fill(void)
{
    char buffer[10];
    void *result = memset(buffer, 'A', 5);
    buffer[5] = '\0';
    TEST_ASSERT_EQUAL_STRING("AAAAA", buffer);
    TEST_ASSERT_EQUAL_PTR(buffer, result);
}

static void test_memset_zero(void)
{
    char buffer[10] = "hello";
    memset(buffer, 0, 5);
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL(0, buffer[i]);
    }
}

/* memcpy tests */
static void test_memcpy_simple(void)
{
    char src[] = "hello";
    char dest[10];
    void *result = memcpy(dest, src, 6);
    TEST_ASSERT_EQUAL_STRING("hello", dest);
    TEST_ASSERT_EQUAL_PTR(dest, result);
}

static void test_memcpy_partial(void)
{
    char src[] = "hello world";
    char dest[20];
    memcpy(dest, src, 5);
    dest[5] = '\0';
    TEST_ASSERT_EQUAL_STRING("hello", dest);
}

/* memmove tests */
static void test_memmove_nonoverlap(void)
{
    char src[] = "hello";
    char dest[10];
    void *result = memmove(dest, src, 6);
    TEST_ASSERT_EQUAL_STRING("hello", dest);
    TEST_ASSERT_EQUAL_PTR(dest, result);
}

static void test_memmove_overlap_forward(void)
{
    char buffer[] = "hello world";
    memmove(buffer + 2, buffer, 5);
    TEST_ASSERT_EQUAL_MEMORY("hehello", buffer, 7);
}

static void test_memmove_overlap_backward(void)
{
    char buffer[] = "hello world";
    memmove(buffer, buffer + 6, 5);
    TEST_ASSERT_EQUAL_STRING("world world", buffer);
}

/* strtod tests */
static void test_strtod_integer(void)
{
    char *endptr;
    double result = strtod("123", &endptr);
    TEST_ASSERT_EQUAL_DOUBLE(123.0, result);
    TEST_ASSERT_EQUAL('\0', *endptr);
}

static void test_strtod_decimal(void)
{
    char *endptr;
    double result = strtod("123.456", &endptr);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 123.456, result);
}

static void test_strtod_negative(void)
{
    char *endptr;
    double result = strtod("-42.5", &endptr);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, -42.5, result);
}

static void test_strtod_scientific(void)
{
    char *endptr;
    double result = strtod("1.5e2", &endptr);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 150.0, result);
}

static void test_strtod_negative_exponent(void)
{
    char *endptr;
    double result = strtod("1500e-2", &endptr);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 15.0, result);
}

static void test_strtod_leading_whitespace(void)
{
    char *endptr;
    double result = strtod("  \t42", &endptr);
    TEST_ASSERT_EQUAL_DOUBLE(42.0, result);
}

static void test_strtod_positive_sign(void)
{
    char *endptr;
    double result = strtod("+99", &endptr);
    TEST_ASSERT_EQUAL_DOUBLE(99.0, result);
}

void libc_string_tests(void)
{
    /* strlen tests */
    RUN_TEST(test_strlen_empty);
    RUN_TEST(test_strlen_simple);
    RUN_TEST(test_strlen_with_spaces);

    /* strnlen tests */
    RUN_TEST(test_strnlen_within_limit);
    RUN_TEST(test_strnlen_at_limit);
    RUN_TEST(test_strnlen_exceeds_limit);

    /* strcmp tests */
    RUN_TEST(test_strcmp_equal);
    RUN_TEST(test_strcmp_less);
    RUN_TEST(test_strcmp_greater);
    RUN_TEST(test_strcmp_empty);
    RUN_TEST(test_strcmp_prefix);

    /* strncmp tests */
    RUN_TEST(test_strncmp_equal_within_n);
    RUN_TEST(test_strncmp_equal_partial);
    RUN_TEST(test_strncmp_diff_within_n);
    RUN_TEST(test_strncmp_zero_length);

    /* strcpy tests */
    RUN_TEST(test_strcpy_simple);
    RUN_TEST(test_strcpy_empty);

    /* strncpy tests */
    RUN_TEST(test_strncpy_full_copy);
    RUN_TEST(test_strncpy_truncated);
    RUN_TEST(test_strncpy_padded);

    /* strcat tests */
    RUN_TEST(test_strcat_simple);
    RUN_TEST(test_strcat_empty_src);
    RUN_TEST(test_strcat_empty_dest);

    /* memset tests */
    RUN_TEST(test_memset_fill);
    RUN_TEST(test_memset_zero);

    /* memcpy tests */
    RUN_TEST(test_memcpy_simple);
    RUN_TEST(test_memcpy_partial);

    /* memmove tests */
    RUN_TEST(test_memmove_nonoverlap);
    RUN_TEST(test_memmove_overlap_forward);
    RUN_TEST(test_memmove_overlap_backward);

    /* strtod tests */
    RUN_TEST(test_strtod_integer);
    RUN_TEST(test_strtod_decimal);
    RUN_TEST(test_strtod_negative);
    RUN_TEST(test_strtod_scientific);
    RUN_TEST(test_strtod_negative_exponent);
    RUN_TEST(test_strtod_leading_whitespace);
    RUN_TEST(test_strtod_positive_sign);
}
