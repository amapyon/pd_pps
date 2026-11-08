#include "m2_ssd1306.h"
#include <string.h>

/* Small original ASCII subset for the dashboard (column-major, 5x7). */
typedef struct { char c; uint8_t x[5]; } glyph_t;
static const glyph_t glyphs[] = {
    {' ',{0x00,0x00,0x00,0x00,0x00}},
    {'!',{0x00,0x00,0x5F,0x00,0x00}},
    {'%',{0x63,0x13,0x08,0x64,0x63}},
    {'(',{0x00,0x1C,0x22,0x41,0x00}},
    {')',{0x00,0x41,0x22,0x1C,0x00}},
    {'+',{0x08,0x08,0x3E,0x08,0x08}},
    {',',{0x00,0x50,0x30,0x00,0x00}},
    {'-',{0x08,0x08,0x08,0x08,0x08}},
    {'.',{0x00,0x60,0x60,0x00,0x00}},
    {'/',{0x60,0x10,0x08,0x04,0x03}},
    {'0',{0x3E,0x51,0x49,0x45,0x3E}},
    {'1',{0x00,0x42,0x7F,0x40,0x00}},
    {'2',{0x42,0x61,0x51,0x49,0x46}},
    {'3',{0x21,0x41,0x45,0x4B,0x31}},
    {'4',{0x18,0x14,0x12,0x7F,0x10}},
    {'5',{0x27,0x45,0x45,0x45,0x39}},
    {'6',{0x3C,0x4A,0x49,0x49,0x30}},
    {'7',{0x01,0x71,0x09,0x05,0x03}},
    {'8',{0x36,0x49,0x49,0x49,0x36}},
    {'9',{0x06,0x49,0x49,0x29,0x1E}},
    {':',{0x00,0x36,0x36,0x00,0x00}},
    {'=',{0x14,0x14,0x14,0x14,0x14}},
    {'?',{0x02,0x01,0x51,0x09,0x06}},
    {'A',{0x7E,0x11,0x11,0x11,0x7E}},
    {'B',{0x7F,0x49,0x49,0x49,0x36}},
    {'C',{0x3E,0x41,0x41,0x41,0x22}},
    {'D',{0x7F,0x41,0x41,0x22,0x1C}},
    {'E',{0x7F,0x49,0x49,0x49,0x41}},
    {'F',{0x7F,0x09,0x09,0x09,0x01}},
    {'G',{0x3E,0x41,0x49,0x49,0x7A}},
    {'H',{0x7F,0x08,0x08,0x08,0x7F}},
    {'I',{0x00,0x41,0x7F,0x41,0x00}},
    {'J',{0x20,0x40,0x41,0x3F,0x01}},
    {'K',{0x7F,0x08,0x14,0x22,0x41}},
    {'L',{0x7F,0x40,0x40,0x40,0x40}},
    {'M',{0x7F,0x02,0x0C,0x02,0x7F}},
    {'N',{0x7F,0x04,0x08,0x10,0x7F}},
    {'O',{0x3E,0x41,0x41,0x41,0x3E}},
    {'P',{0x7F,0x09,0x09,0x09,0x06}},
    {'Q',{0x3E,0x41,0x51,0x21,0x5E}},
    {'R',{0x7F,0x09,0x19,0x29,0x46}},
    {'S',{0x46,0x49,0x49,0x49,0x31}},
    {'T',{0x01,0x01,0x7F,0x01,0x01}},
    {'U',{0x3F,0x40,0x40,0x40,0x3F}},
    {'V',{0x1F,0x20,0x40,0x20,0x1F}},
    {'W',{0x7F,0x20,0x18,0x20,0x7F}},
    {'X',{0x63,0x14,0x08,0x14,0x63}},
    {'Y',{0x07,0x08,0x70,0x08,0x07}},
    {'Z',{0x61,0x51,0x49,0x45,0x43}}
};

static void draw_char(m2_ssd1306_t *oled, uint8_t page, uint8_t col, char c)
{
    static const uint8_t unknown[5] = {0x02,0x01,0x51,0x09,0x06};
    const uint8_t *pattern = unknown;
    size_t k;
    for (k = 0; k < sizeof(glyphs) / sizeof(glyphs[0]); ++k) {
        if (glyphs[k].c == c) { pattern = glyphs[k].x; break; }
    }
    for (k = 0; k < 5u; ++k) oled->fb[(size_t)page * 128u + col + k] = pattern[k];
    oled->fb[(size_t)page * 128u + col + 5u] = 0u;
}

bool m2_ssd1306_init(m2_ssd1306_t *oled, m2_i2c_write_fn write,
                     void *context, uint8_t address7)
{
    static const uint8_t cmds[] = {
        0x00, /* control: Co=0, D/C#=0 => commands */
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x02, /* page addressing */
        0xA1, 0xC8, 0xDA, 0x12, 0x81, 0x7F,
        0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0x2E, 0xAF
    };
    if (!oled || !write || (address7 != 0x3Cu && address7 != 0x3Du)) return false;
    memset(oled, 0, sizeof(*oled));
    oled->write = write;
    oled->context = context;
    oled->address7 = address7;
    if (!write(address7, cmds, sizeof(cmds), context)) return false;
    oled->initialized = true;
    return true;
}

bool m2_ssd1306_init_auto(m2_ssd1306_t *oled, m2_i2c_write_fn write,
                          void *context)
{
    return m2_ssd1306_init(oled, write, context, 0x3Cu) ||
           m2_ssd1306_init(oled, write, context, 0x3Du);
}

void m2_ssd1306_clear(m2_ssd1306_t *oled)
{
    if (!oled || oled->busy) return;
    memset(oled->fb, 0, sizeof(oled->fb));
}

void m2_ssd1306_text(m2_ssd1306_t *oled, uint8_t row,
                     uint8_t column_chars, const char *text)
{
    uint8_t px;
    if (!oled || !text || oled->busy || row >= 8u || column_chars >= 21u) return;
    px = (uint8_t)(column_chars * 6u);
    while (*text && px <= 122u) {
        char c = *text++;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        draw_char(oled, row, px, c);
        px = (uint8_t)(px + 6u);
    }
}

bool m2_ssd1306_begin_flush(m2_ssd1306_t *oled)
{
    if (!oled || !oled->initialized || oled->busy) return false;
    oled->page = 0u;
    oled->column = 0u;
    oled->page_needs_command = true;
    oled->busy = true;
    return true;
}

m2_flush_result_t m2_ssd1306_flush_step(m2_ssd1306_t *oled)
{
    uint8_t buffer[M2_OLED_CHUNK_BYTES + 1u];
    uint8_t addr_cmd[4];
    uint8_t remaining;
    uint8_t n;
    if (!oled || !oled->initialized) return M2_FLUSH_ERROR;
    if (!oled->busy) return M2_FLUSH_IDLE;
    if (oled->page >= 8u) { oled->busy = false; return M2_FLUSH_DONE; }
    if (oled->page_needs_command) {
        addr_cmd[0] = 0x00; /* command control */
        addr_cmd[1] = (uint8_t)(0xB0u | oled->page);
        addr_cmd[2] = 0x00;
        addr_cmd[3] = 0x10;
        if (!oled->write(oled->address7, addr_cmd, sizeof(addr_cmd), oled->context)) {
            oled->busy = false;
            return M2_FLUSH_ERROR;
        }
        oled->page_needs_command = false;
        return M2_FLUSH_BUSY;
    }
    remaining = (uint8_t)(128u - oled->column);
    n = (remaining > M2_OLED_CHUNK_BYTES) ? M2_OLED_CHUNK_BYTES : remaining;
    buffer[0] = 0x40; /* data control */
    memcpy(buffer + 1, oled->fb + (size_t)oled->page * 128u + oled->column, n);
    if (!oled->write(oled->address7, buffer, (size_t)n + 1u, oled->context)) {
        oled->busy = false;
        return M2_FLUSH_ERROR;
    }
    oled->column = (uint8_t)(oled->column + n);
    if (oled->column >= 128u) {
        oled->column = 0u;
        ++oled->page;
        oled->page_needs_command = true;
        if (oled->page == 8u) { oled->busy = false; return M2_FLUSH_DONE; }
    }
    return M2_FLUSH_BUSY;
}
