#ifndef CONSOLE_INPUT_H
#define CONSOLE_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CONSOLE_INPUT_LINE_SIZE 32u
#define CONSOLE_INPUT_ECHO_SIZE 256u

/* Single-threaded input editor. Feed it only from the debug-input callback;
 * drain the echo queue and run commands from main(), NEVER from the callback.
 * printf() can itself invoke ch32fun's input handler, so immediate echoing
 * within handle_debug_input() risks re-entrancy.
 */
typedef struct {
    char line[CONSOLE_INPUT_LINE_SIZE];
    unsigned line_length;
    bool line_ready;
    bool line_overflow;
    bool last_was_newline;
    uint8_t echo[CONSOLE_INPUT_ECHO_SIZE];
    unsigned echo_head;
    unsigned echo_tail;
} ConsoleInput;

static inline void console_input_queue_echo(ConsoleInput *in, uint8_t c)
{
    unsigned next = (in->echo_head + 1u) % CONSOLE_INPUT_ECHO_SIZE;
    if (next != in->echo_tail) {
        in->echo[in->echo_head] = c;
        in->echo_head = next;
    }
}

static inline void console_input_feed(ConsoleInput *in, const uint8_t *bytes,
                                       size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        const uint8_t c = bytes[i];
        if (in->line_ready) continue;
        if (c == '\r' || c == '\n') {
            if (in->last_was_newline) continue; /* CRLF = one ENTER. */
            in->last_was_newline = true;
            in->line[in->line_length] = '\0';
            in->line_ready = true;
            console_input_queue_echo(in, '\n');
            continue;
        }
        in->last_was_newline = false;
        if (c == 8u || c == 127u) { /* Backspace, DEL */
            if (in->line_length != 0u) {
                --in->line_length;
                console_input_queue_echo(in, '\b');
                console_input_queue_echo(in, ' ');
                console_input_queue_echo(in, '\b');
            }
            continue;
        }
        /* The command language is ASCII; don't echo terminal controls. */
        if (c < 32u || c > 126u) continue;
        if (in->line_length + 1u < CONSOLE_INPUT_LINE_SIZE) {
            in->line[in->line_length++] = (char)c;
            console_input_queue_echo(in, c);
        } else {
            in->line_overflow = true;
            console_input_queue_echo(in, '\a'); /* bell: line too long */
        }
    }
}

/* Called from main() after poll_input(); returns a zero-terminated fragment. */
static inline size_t console_input_pop_echo(ConsoleInput *in,
                                            char *out, size_t out_size)
{
    if (out_size == 0u) return 0u;
    size_t n = 0u;
    while (n + 1u < out_size && in->echo_tail != in->echo_head) {
        out[n++] = (char)in->echo[in->echo_tail];
        in->echo_tail = (in->echo_tail + 1u) % CONSOLE_INPUT_ECHO_SIZE;
    }
    out[n] = '\0';
    return n;
}

static inline bool console_input_take_line(ConsoleInput *in,
                                           char *out, size_t out_size,
                                           bool *overflow)
{
    if (!in->line_ready || out_size < CONSOLE_INPUT_LINE_SIZE || !overflow)
        return false;
    memcpy(out, in->line, in->line_length + 1u);
    *overflow = in->line_overflow;
    in->line_length = 0u;
    in->line_overflow = false;
    in->line_ready = false;
    return true;
}

#endif
