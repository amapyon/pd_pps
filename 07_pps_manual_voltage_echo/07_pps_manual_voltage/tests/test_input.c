#include "../console_input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void feed(ConsoleInput *in, const char *text)
{
    console_input_feed(in, (const uint8_t *)text, strlen(text));
}
static void echo_equals(ConsoleInput *in, const char *expected)
{
    char out[CONSOLE_INPUT_ECHO_SIZE];
    size_t n = console_input_pop_echo(in, out, sizeof(out));
    assert(n == strlen(expected));
    assert(strcmp(out, expected) == 0);
}
static void line_equals(ConsoleInput *in, const char *expected, bool is_overflow)
{
    char line[CONSOLE_INPUT_LINE_SIZE];
    bool overflow = !is_overflow;
    assert(console_input_take_line(in, line, sizeof(line), &overflow));
    assert(strcmp(line, expected) == 0);
    assert(overflow == is_overflow);
    assert(!console_input_take_line(in, line, sizeof(line), &overflow));
}
int main(void)
{
    ConsoleInput in = {0};
    feed(&in, "7.0");
    echo_equals(&in, "7.0");
    feed(&in, "2\r");
    echo_equals(&in, "2\n");
    line_equals(&in, "7.02", false);
    feed(&in, "\n"); /* second byte of CRLF, should not echo */
    echo_equals(&in, "");

    feed(&in, "7.80\b\b50\r\n");
    echo_equals(&in, "7.80\b \b\b \b50\n");
    line_equals(&in, "7.50", false);

    feed(&in, "status\n");
    echo_equals(&in, "status\n");
    line_equals(&in, "status", false);

    feed(&in, "12345678901234567890123456789012\n");
    echo_equals(&in, "1234567890123456789012345678901\a\n");
    line_equals(&in, "1234567890123456789012345678901", true);

    feed(&in, "11.00\n");
    echo_equals(&in, "11.00\n");
    line_equals(&in, "11.00", false);
    puts("interactive echo test OK");
    return 0;
}
