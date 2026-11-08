#include "m3_input.h"
#include <assert.h>
#include <stdio.h>

static m3_levels_t pins(unsigned ab, bool sw, bool back)
{
    m3_levels_t x = {(ab & 1u) != 0, (ab & 2u) != 0, sw, back};
    return x;
}
static int delta(m3_input_t *s, unsigned ab, uint32_t t)
{
    return m3_input_sample(s, pins(ab, true, true), t).rotation;
}

int main(void)
{
    m3_input_t s;
    m3_input_init(&s, pins(3u, true, true), 0u);
    assert(delta(&s, 2u, 2u) == 0);
    assert(delta(&s, 0u, 4u) == 0);
    assert(delta(&s, 1u, 6u) == 0);
    assert(delta(&s, 3u, 8u) == 1);
    assert(s.position == 1 && s.cw_count == 1);
    assert(delta(&s, 1u, 10u) == 0);
    assert(delta(&s, 0u, 12u) == 0);
    assert(delta(&s, 2u, 14u) == 0);
    assert(delta(&s, 3u, 16u) == -1);
    assert(s.position == 0 && s.ccw_count == 1);
    puts("PASS: both quadrature directions");

    /* Contact bounce reverses the previous edge and must not make a click. */
    m3_input_init(&s, pins(3u, true, true), 0u);
    assert(delta(&s, 2u, 1u) == 0);
    assert(delta(&s, 3u, 2u) == 0);
    assert(delta(&s, 2u, 3u) == 0);
    assert(delta(&s, 0u, 4u) == 0);
    assert(delta(&s, 1u, 5u) == 0);
    assert(delta(&s, 3u, 6u) == 1);
    assert(s.position == 1);
    puts("PASS: quadrature contact bounce cancellation");

    m3_input_init(&s, pins(3u, true, true), 0u);
    assert(delta(&s, 0u, 1u) == 0);
    assert(s.glitch_count == 1 && s.position == 0);
    puts("PASS: invalid Gray-code transition resync");

    m3_input_init(&s, pins(3u, true, true), 0u);
    assert(!m3_input_sample(&s, pins(3u, false, true), 1u).encoder_press);
    assert(!m3_input_sample(&s, pins(3u, true, true), 4u).encoder_press);
    assert(!m3_input_sample(&s, pins(3u, false, true), 6u).encoder_press);
    assert(!m3_input_sample(&s, pins(3u, false, true), 29u).encoder_press);
    assert(m3_input_sample(&s, pins(3u, false, true), 31u).encoder_press);
    assert(!m3_input_sample(&s, pins(3u, false, true), 70u).encoder_press);
    assert(!m3_input_sample(&s, pins(3u, true, true), 100u).encoder_press);
    assert(!m3_input_sample(&s, pins(3u, true, true), 125u).encoder_press);
    assert(m3_input_sample(&s, pins(3u, false, true), 160u).encoder_press == false);
    assert(m3_input_sample(&s, pins(3u, false, true), 185u).encoder_press);
    assert(s.press_count == 2);
    puts("PASS: switch debounce, one event per press");

    m3_input_init(&s, pins(3u, true, true), 0u);
    assert(!m3_input_sample(&s, pins(3u, true, false), 0u).back_press);
    assert(m3_input_sample(&s, pins(3u, true, false), 25u).back_press);
    assert(!m3_input_sample(&s, pins(3u, true, false), 26u).back_press);
    assert(s.back_count == 1u);
    puts("PASS: BACK debounce");

    m3_input_init(&s, pins(3u, true, true), UINT32_MAX - 10u);
    assert(!m3_input_sample(&s, pins(3u, true, false), UINT32_MAX - 5u).back_press);
    assert(m3_input_sample(&s, pins(3u, true, false), 20u).back_press);
    puts("PASS: debounce across 32-bit millisecond overflow");

    return 0;
}
