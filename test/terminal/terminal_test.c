#include <libs/Unity/src/unity.h>

#include <kernel/terminal/terminal.h>
#include <string.h>

terminal_t terminal;
char mock_term_content[10240];
size_t mock_term_index = 0;

static void _flush_callback(struct terminal *term)
{
    memcpy(mock_term_content + mock_term_index, term->buffer, term->index);
    mock_term_index += term->index;
}

void setUp()
{
    term_init(&terminal, _flush_callback, NULL);
    memset(mock_term_content, 0, sizeof(mock_term_content));
    mock_term_index = 0;
}
void tearDown()
{
    mock_term_index = 0;
    terminal.index = 0;
}

/* Test printing a string to the terminal */
void term_puts_test()
{
    const char *str = "Hello world!";
    term_puts(&terminal, str);
    TEST_ASSERT_EQUAL_INT(strlen(str), terminal.index);
    TEST_ASSERT_EQUAL_STRING_LEN(str, terminal.buffer, terminal.index);
}

/* Test flushing the terminal buffer on newline */
void term_puts_flush_on_newline_test()
{
    const char *str = "Hello world!\n";
    term_puts(&terminal, str);
    TEST_ASSERT_EQUAL_INT(strlen(str), mock_term_index);
    TEST_ASSERT_EQUAL_STRING(str, mock_term_content);
}

/* Test flushing the terminal when the buffer is full */
void term_puts_flush_on_buffer_full_test()
{
    /* Fill the buffer one character at a time until it's almost full */
    for (size_t i = 0; i < TERM_BUFFER_SIZE; i++)
        term_puts(&terminal, "a");

    /* At this point the buffer should be full but not yet flushed */
    TEST_ASSERT_EQUAL_INT(TERM_BUFFER_SIZE, terminal.index);

    /* Adding one more character should trigger a flush */
    term_puts(&terminal, "b");

    /* Verify the buffer was flushed */
    TEST_ASSERT_EQUAL_INT(TERM_BUFFER_SIZE, mock_term_index);

    /* Verify the buffer now just contains the most recent character */
    TEST_ASSERT_EQUAL_INT(1, terminal.index);
    TEST_ASSERT_EQUAL_CHAR('b', terminal.buffer[0]);

    /* Verify the flushed content is correct */
    for (size_t i = 0; i < TERM_BUFFER_SIZE; i++) {
        TEST_ASSERT_EQUAL_CHAR('a', mock_term_content[i]);
    }

    /* The last character shouldn't be flushed yet */
    TEST_ASSERT_NOT_EQUAL('b', mock_term_content[TERM_BUFFER_SIZE]);
}

/* Test printing strings in term_printf */
void term_printf_string_test()
{
    const char *expected = "Hello world!";
    size_t expected_len = strlen(expected);
    term_printf(&terminal, "Hello %s!", "world");
    TEST_ASSERT_EQUAL_INT(expected_len, terminal.index);
    TEST_ASSERT_EQUAL_STRING_LEN("Hello world!", terminal.buffer, expected_len);
}

/* Test printing chars in term_printf */
void term_printf_char_test()
{
    const char *expected = "The first character of the english alphabet is: a";
    size_t expected_len = strlen(expected);
    term_printf(&terminal, "The first character of the english alphabet is: %c", 'a');
    TEST_ASSERT_EQUAL_INT(expected_len, terminal.index);
    TEST_ASSERT_EQUAL_STRING_LEN(expected, terminal.buffer, expected_len);
}

/* Test printing positive integer decimals in term_printf */
void term_printf_int_test()
{
    const char *expected = "The number is: 42";
    size_t expected_len = strlen(expected);
    term_printf(&terminal, "The number is: %d", 42);
    TEST_ASSERT_EQUAL_INT(expected_len, terminal.index);
    TEST_ASSERT_EQUAL_STRING_LEN(expected, terminal.buffer, expected_len);
}

/* Test printing negative integer decimals in term_printf */
void term_printf_neg_int_test()
{
    const char *expected = "The number is: -42";
    size_t expected_len = strlen(expected);
    term_printf(&terminal, "The number is: %d", -42);
    TEST_ASSERT_EQUAL_INT(expected_len, terminal.index);
    TEST_ASSERT_EQUAL_STRING_LEN(expected, terminal.buffer, expected_len);
}

/* Test printing hexadecimal integers in term_printf */
void term_printf_hex_test()
{
    const char *expected = "The number is: 0x2a";
    size_t expected_len = strlen(expected);
    term_printf(&terminal, "The number is: 0x%x", 42);
    TEST_ASSERT_EQUAL_INT(expected_len, terminal.index);
    TEST_ASSERT_EQUAL_STRING_LEN(expected, terminal.buffer, expected_len);
}

/* Test printing percentage in term_printf */
void term_printf_percent_test()
{
    const char *expected = "The number is: 42%";
    size_t expected_len = strlen(expected);
    term_printf(&terminal, "The number is: %d%%", 42);
    TEST_ASSERT_EQUAL_INT(expected_len, terminal.index);
    TEST_ASSERT_EQUAL_STRING_LEN(expected, terminal.buffer, expected_len);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(term_puts_test);
    RUN_TEST(term_puts_flush_on_newline_test);
    RUN_TEST(term_puts_flush_on_buffer_full_test);
    RUN_TEST(term_printf_string_test);
    RUN_TEST(term_printf_char_test);
    RUN_TEST(term_printf_int_test);
    RUN_TEST(term_printf_neg_int_test);
    RUN_TEST(term_printf_hex_test);
    RUN_TEST(term_printf_percent_test);
    return UNITY_END();
}
