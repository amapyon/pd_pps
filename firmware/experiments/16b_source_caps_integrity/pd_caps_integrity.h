/* USB PD SPR Source_Capabilities frame sanity checks.
 * Host-testable. These are defensive filters, not USB PD certification.
 */
#ifndef PD_CAPS_INTEGRITY_H
#define PD_CAPS_INTEGRITY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define PD_CAPS_MAX_PDOS 7u
#define PD_CAPS_MAX_RX_BYTES 34u
static inline bool pd_caps_frame_length_valid(size_t n, size_t rx_bytes)
{
    return n >= 1u && n <= PD_CAPS_MAX_PDOS &&
           rx_bytes >= (2u + 4u * n + 4u) &&
           rx_bytes <= PD_CAPS_MAX_RX_BYTES;
}
static inline bool pd_caps_words_plausible(const uint32_t *words, size_t n)
{
    if (!words || n == 0u || n > PD_CAPS_MAX_PDOS) return false;
    /* The first PDO of an SPR source is a fixed 5 V supply. */
    if ((words[0] >> 30u) != 0u || ((words[0] >> 10u) & 0x3ffu) != 100u ||
        (words[0] & 0x3ffu) == 0u) return false;
    for (size_t i = 0; i < n; ++i) {
        const uint32_t w = words[i];
        const uint32_t kind = w >> 30u;
        if (kind == 0u) { /* Fixed PDO (SPR, 5-20 V). */
            const uint32_t v50 = (w >> 10u) & 0x3ffu;
            const uint32_t i10 = w & 0x3ffu;
            if (v50 < 100u || v50 > 400u || i10 == 0u) return false;
        } else if (kind == 3u && ((w >> 28u) & 3u) == 0u) { /* SPR PPS */
            const uint32_t max_i50 = w & 0x7fu;
            const uint32_t min_v100 = (w >> 8u) & 0xffu;
            const uint32_t max_v100 = (w >> 17u) & 0xffu;
            if (max_i50 == 0u || min_v100 < 33u ||
                max_v100 < min_v100 || max_v100 > 210u) return false;
        }
        /* Other PDO types are left to the PD layer; not interpreted here. */
    }
    return true;
}
#endif
