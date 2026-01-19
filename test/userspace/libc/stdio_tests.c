#include <libs/Unity/src/unity.h>
#include <stdio.h>

/* Basic string formatting */
static void test_sprintf_string(void)
{
    char buf[64];
    int len = sprintf(buf, "hello %s", "world");
    TEST_ASSERT_EQUAL_STRING("hello world", buf);
    TEST_ASSERT_EQUAL(11, len);
}

static void test_sprintf_char(void)
{
    char buf[64];
    int len = sprintf(buf, "char: %c", 'A');
    TEST_ASSERT_EQUAL_STRING("char: A", buf);
    TEST_ASSERT_EQUAL(7, len);
}

/* Integer formatting */
static void test_sprintf_int_positive(void)
{
    char buf[64];
    int len = sprintf(buf, "num: %d", 42);
    TEST_ASSERT_EQUAL_STRING("num: 42", buf);
    TEST_ASSERT_EQUAL(7, len);
}

static void test_sprintf_int_negative(void)
{
    char buf[64];
    int len = sprintf(buf, "num: %d", -123);
    TEST_ASSERT_EQUAL_STRING("num: -123", buf);
    TEST_ASSERT_EQUAL(9, len);
}

static void test_sprintf_int_zero(void)
{
    char buf[64];
    sprintf(buf, "%d", 0);
    TEST_ASSERT_EQUAL_STRING("0", buf);
}

static void test_sprintf_unsigned(void)
{
    char buf[64];
    sprintf(buf, "%u", 4294967295U);
    TEST_ASSERT_EQUAL_STRING("4294967295", buf);
}

/* Hex formatting */
static void test_sprintf_hex_lower(void)
{
    char buf[64];
    sprintf(buf, "%x", 255);
    TEST_ASSERT_EQUAL_STRING("ff", buf);
}

static void test_sprintf_hex_upper(void)
{
    char buf[64];
    sprintf(buf, "%X", 255);
    TEST_ASSERT_EQUAL_STRING("FF", buf);
}

static void test_sprintf_hex_02x_padding(void)
{
    char buf[64];
    sprintf(buf, "%02x", 5);
    TEST_ASSERT_EQUAL_STRING("05", buf);
}

static void test_sprintf_hex_02X_padding(void)
{
    char buf[64];
    sprintf(buf, "%02X", 10);
    TEST_ASSERT_EQUAL_STRING("0A", buf);
}

static void test_sprintf_hex_02x_no_padding_needed(void)
{
    char buf[64];
    sprintf(buf, "%02x", 255);
    TEST_ASSERT_EQUAL_STRING("ff", buf);
}

/* Float formatting */
static void test_sprintf_float_default(void)
{
    char buf[64];
    sprintf(buf, "%f", 3.14159);
    /* Default precision is 6 */
    TEST_ASSERT_EQUAL_STRING("3.141590", buf);
}

static void test_sprintf_float_precision_2(void)
{
    char buf[64];
    sprintf(buf, "%.2f", 3.14159);
    TEST_ASSERT_EQUAL_STRING("3.14", buf);
}

static void test_sprintf_float_precision_0(void)
{
    char buf[64];
    sprintf(buf, "%.0f", 3.7);
    TEST_ASSERT_EQUAL_STRING("4", buf);
}

static void test_sprintf_float_precision_dotf(void)
{
    char buf[64];
    sprintf(buf, "%.f", 3.7);
    TEST_ASSERT_EQUAL_STRING("4", buf);
}

static void test_sprintf_float_negative(void)
{
    char buf[64];
    sprintf(buf, "%.2f", -42.5);
    TEST_ASSERT_EQUAL_STRING("-42.50", buf);
}

static void test_sprintf_float_two_digit_precision(void)
{
    char buf[64];
    sprintf(buf, "%.10f", 1.5);
    /* precision capped at 6 */
    TEST_ASSERT_EQUAL_STRING("1.500000", buf);
}

/* Percent escape */
static void test_sprintf_percent(void)
{
    char buf[64];
    sprintf(buf, "100%%");
    TEST_ASSERT_EQUAL_STRING("100%", buf);
}

/* Unknown specifier */
static void test_sprintf_unknown_specifier(void)
{
    char buf[64];
    sprintf(buf, "%q");
    TEST_ASSERT_EQUAL_STRING("%q", buf);
}

/* Trailing percent */
static void test_sprintf_trailing_percent(void)
{
    char buf[64];
    sprintf(buf, "test%");
    TEST_ASSERT_EQUAL_STRING("test%", buf);
}

/* Combined format */
static void test_sprintf_combined(void)
{
    char buf[128];
    sprintf(buf, "%s: %d, 0x%X, %.2f", "test", 42, 255, 3.14);
    TEST_ASSERT_EQUAL_STRING("test: 42, 0xFF, 3.14", buf);
}

/* Empty string */
static void test_sprintf_empty_format(void)
{
    char buf[64] = "initial";
    sprintf(buf, "");
    TEST_ASSERT_EQUAL_STRING("", buf);
}

/* Plain text */
static void test_sprintf_plain_text(void)
{
    char buf[64];
    sprintf(buf, "hello world");
    TEST_ASSERT_EQUAL_STRING("hello world", buf);
}

void libc_stdio_tests(void)
{
    /* String and char */
    RUN_TEST(test_sprintf_string);
    RUN_TEST(test_sprintf_char);

    /* Integers */
    RUN_TEST(test_sprintf_int_positive);
    RUN_TEST(test_sprintf_int_negative);
    RUN_TEST(test_sprintf_int_zero);
    RUN_TEST(test_sprintf_unsigned);

    /* Hex */
    RUN_TEST(test_sprintf_hex_lower);
    RUN_TEST(test_sprintf_hex_upper);
    RUN_TEST(test_sprintf_hex_02x_padding);
    RUN_TEST(test_sprintf_hex_02X_padding);
    RUN_TEST(test_sprintf_hex_02x_no_padding_needed);

    /* Float */
    RUN_TEST(test_sprintf_float_default);
    RUN_TEST(test_sprintf_float_precision_2);
    RUN_TEST(test_sprintf_float_precision_0);
    RUN_TEST(test_sprintf_float_precision_dotf);
    RUN_TEST(test_sprintf_float_negative);
    RUN_TEST(test_sprintf_float_two_digit_precision);

    /* Special cases */
    RUN_TEST(test_sprintf_percent);
    RUN_TEST(test_sprintf_unknown_specifier);
    RUN_TEST(test_sprintf_trailing_percent);
    RUN_TEST(test_sprintf_combined);
    RUN_TEST(test_sprintf_empty_format);
    RUN_TEST(test_sprintf_plain_text);
}
