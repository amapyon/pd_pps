#ifndef M2_SSD1306_H
#define M2_SSD1306_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define M2_OLED_WIDTH 128u
#define M2_OLED_HEIGHT 64u
#define M2_OLED_PAGES 8u
#define M2_OLED_BUFFER_BYTES 1024u
#define M2_OLED_CHUNK_BYTES 16u

/*
 * The function must transmit all bytes in ONE I2C transaction:
 * START, (address7 << 1) | 0, bytes[0..length-1], STOP.
 * Return false on any address/data NACK or bus error, always issue STOP.
 * The function does not drive SDA/SCL itself: use the proven M1 bus.
 */
typedef bool (*m2_i2c_write_fn)(uint8_t address7,
                                const uint8_t *bytes,
                                size_t length,
                                void *context);

typedef struct {
    m2_i2c_write_fn write;
    void *context;
    uint8_t address7;
    uint8_t fb[M2_OLED_BUFFER_BYTES];
    uint8_t page;
    uint8_t column;
    bool busy;
    bool initialized;
    bool page_needs_command;
} m2_ssd1306_t;

typedef enum {
    M2_FLUSH_ERROR = -1,
    M2_FLUSH_IDLE = 0,
    M2_FLUSH_BUSY = 1,
    M2_FLUSH_DONE = 2
} m2_flush_result_t;

/* Calls one init transaction; OLED power must be stable first (>=100ms recommended). */
bool m2_ssd1306_init(m2_ssd1306_t *oled, m2_i2c_write_fn write,
                     void *context, uint8_t address7);
/* Tries 0x3C then 0x3D, only if both addresses have been cleared for use. */
bool m2_ssd1306_init_auto(m2_ssd1306_t *oled, m2_i2c_write_fn write,
                          void *context);
void m2_ssd1306_clear(m2_ssd1306_t *oled);
/* 8 rows of 6x8 ASCII glyphs; use capitals for guaranteed support. */
void m2_ssd1306_text(m2_ssd1306_t *oled, uint8_t row,
                     uint8_t column_chars, const char *text);
/* Begin flush only when idle; framebuffer must not change while busy. */
bool m2_ssd1306_begin_flush(m2_ssd1306_t *oled);
/* At most ONE I2C transaction per call; call from normal application loop. */
m2_flush_result_t m2_ssd1306_flush_step(m2_ssd1306_t *oled);

#endif
