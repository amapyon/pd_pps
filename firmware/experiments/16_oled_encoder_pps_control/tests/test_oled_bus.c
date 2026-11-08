#include "m2_bus_adapter.h"
#include <assert.h>
#include <stdio.h>

static unsigned starts;
static unsigned stops;
static unsigned tx_count;
static unsigned nack_on_byte;
static uint8_t transmitted[32];

static bool start(void *context) { (void)context; ++starts; return true; }
static bool tx(uint8_t b, void *context) {
    (void)context;
    transmitted[tx_count] = b;
    ++tx_count;
    return tx_count != nack_on_byte;
}
static void stop(void *context) { (void)context; ++stops; }

int main(void) {
    const uint8_t msg[] = {0x00, 0xAE, 0xA6};
    m2_i2c_ops_t ops = {start,tx,stop,NULL};
    assert(m2_bus_write(0x3C, msg, sizeof(msg), &ops));
    assert(starts == 1 && stops == 1 && tx_count == 4);
    assert(transmitted[0] == 0x78); /* 7-bit 0x3C, write bit=0 */
    assert(transmitted[1] == 0x00 && transmitted[2] == 0xAE);
    nack_on_byte = 2;
    tx_count = 0;
    assert(!m2_bus_write(0x3C, msg, sizeof(msg), &ops));
    assert(starts == 2 && stops == 2 && tx_count == 2);
    puts("PASS: I2C adapter STOP on NACK; uses existing start/write/stop ops");
    return 0;
}
