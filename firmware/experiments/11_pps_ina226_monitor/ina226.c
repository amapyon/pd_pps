/* INA226 R010 monitor: software I2C on CH32X035 PA2/PA3.
 * Avoids PC14/15 (USB PD), PC18/19 (WCH-LinkE debug), PA0 (ADC).
 * 3.3V-only I2C. Pull-ups to 3.3V, NEVER to monitored PPS VBUS.
 * IMPORTANT: CH32X035 general-purpose GPIO_CNF_OUT_OD does NOT reliably
 * release the pin. Emulate open-drain with floating input for released HIGH
 * and push-pull OUTPUT LOW for asserted LOW. See CH32X035 community errata.
 */
#include "ch32fun.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "ina226.h"
#include "ina226_units.h"

#define I2C_SCL_BIT           (1u << 2) /* PA2 */
#define I2C_SDA_BIT           (1u << 3) /* PA3 */
#define I2C_HALF_PERIOD_US    5u
#define CLOCK_STRETCH_TRIES   100u
#define INA226_ADDR_FIRST     0x40u
#define INA226_ADDR_LAST      0x4fu
#define INA226_REG_CONFIG     0x00u
#define INA226_REG_SHUNT      0x01u
#define INA226_REG_BUS        0x02u
#define INA226_REG_MANUF      0xfeu
#define INA226_REG_DIE        0xffu
/* 16-sample averaging, 1.1ms VBUS and shunt conversion, continuous. */
#define INA226_CONFIG_16AVG   0x4527u

static bool s_initialized;
static uint8_t s_sensor_address;

/* Preserve the *first* low-level error within a transaction.
 * The ordinary meter path still returns bool; scan can now show why it failed.
 */
typedef enum {
    I2C_ERR_NONE,
    I2C_ERR_NACK,
    I2C_ERR_SCL_TIMEOUT,
    I2C_ERR_SDA_STUCK
} I2CError;
static I2CError s_i2c_error;

static const char *i2c_error_name(I2CError error)
{
    switch (error) {
        case I2C_ERR_NONE: return "none";
        case I2C_ERR_NACK: return "NACK (no slave acknowledgment)";
        case I2C_ERR_SCL_TIMEOUT: return "SCL stayed LOW (timeout)";
        case I2C_ERR_SDA_STUCK: return "SDA stayed LOW (bus stuck)";
        default: return "unknown";
    }
}
static void i2c_set_error(I2CError error)
{
    if (s_i2c_error == I2C_ERR_NONE) s_i2c_error = error;
}


static void i2c_delay(void) { Delay_Us(I2C_HALF_PERIOD_US); }

/* This MCU does not implement a conventional GPIO open-drain output.
 * To release, switch to floating input (external 3.3 V pull-up drives HIGH).
 * To assert LOW, preload output latch LOW before enabling push-pull output.
 * Never drive HIGH as push-pull: a slave must be able to pull SDA LOW for ACK.
 */
static void scl_release(void) { funPinMode(PA2, GPIO_CNF_IN_FLOATING); }
static void sda_release(void) { funPinMode(PA3, GPIO_CNF_IN_FLOATING); }
static void scl_low(void)
{
    GPIOA->BCR = I2C_SCL_BIT; /* preload 0 before enabling output */
    funPinMode(PA2, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP);
}
static void sda_low(void)
{
    GPIOA->BCR = I2C_SDA_BIT; /* preload 0 before enabling output */
    funPinMode(PA3, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP);
}
static bool scl_is_high(void) { return (GPIOA->INDR & I2C_SCL_BIT) != 0u; }
static bool sda_is_high(void) { return (GPIOA->INDR & I2C_SDA_BIT) != 0u; }

static bool scl_high_wait(void)
{
    scl_release();
    for (unsigned i = 0u; i < CLOCK_STRETCH_TRIES; ++i) {
        if (scl_is_high()) return true;
        i2c_delay();
    }
    i2c_set_error(I2C_ERR_SCL_TIMEOUT);
    return false;
}

void ina226_bus_init(void)
{
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA;
    /* Initialize output latches LOW while pins are still not driven.
     * HIGH is a released input, never a driven output on CH32X035.
     */
    GPIOA->BCR = I2C_SCL_BIT | I2C_SDA_BIT;
    scl_release();
    sda_release();
    s_initialized = false;
    s_sensor_address = 0u;
    s_i2c_error = I2C_ERR_NONE;
    i2c_delay();
}

static bool i2c_start(void)
{
    sda_release();
    if (!scl_high_wait()) return false;
    i2c_delay();
    if (!sda_is_high()) {
        i2c_set_error(I2C_ERR_SDA_STUCK);
        return false;
    } /* SDA stuck low */
    sda_low();
    i2c_delay();
    scl_low();
    return true;
}

static void i2c_stop(void)
{
    sda_low();
    i2c_delay();
    (void)scl_high_wait();
    i2c_delay();
    sda_release();
    i2c_delay();
}

static bool i2c_write_byte(uint8_t data)
{
    for (unsigned b = 0u; b < 8u; ++b) {
        if (data & 0x80u) sda_release();
        else sda_low();
        i2c_delay();
        if (!scl_high_wait()) return false;
        i2c_delay();
        scl_low();
        data <<= 1;
    }
    sda_release(); /* slave ACK */
    i2c_delay();
    if (!scl_high_wait()) return false;
    i2c_delay();
    const bool ack = !sda_is_high();
    scl_low();
    if (!ack) i2c_set_error(I2C_ERR_NACK);
    return ack;
}

static bool i2c_read_byte(uint8_t *data, bool ack)
{
    uint8_t value = 0u;
    sda_release();
    for (unsigned b = 0u; b < 8u; ++b) {
        value <<= 1;
        i2c_delay();
        if (!scl_high_wait()) return false;
        i2c_delay();
        if (sda_is_high()) value |= 1u;
        scl_low();
    }
    if (ack) sda_low(); /* master ACK if another byte follows */
    else sda_release(); /* NACK after last byte */
    i2c_delay();
    if (!scl_high_wait()) return false;
    i2c_delay();
    scl_low();
    sda_release();
    *data = value;
    return true;
}

static bool ina226_register_read(uint8_t address, uint8_t reg, uint16_t *value)
{
    bool ok = false;
    s_i2c_error = I2C_ERR_NONE;
    uint8_t hi = 0u, lo = 0u;
    if (!i2c_start()) goto done;
    if (!i2c_write_byte((uint8_t)(address << 1))) goto done;
    if (!i2c_write_byte(reg)) goto done;
    if (!i2c_start()) goto done; /* repeated START */
    if (!i2c_write_byte((uint8_t)((address << 1) | 1u))) goto done;
    if (!i2c_read_byte(&hi, true)) goto done;
    if (!i2c_read_byte(&lo, false)) goto done;
    *value = (uint16_t)(((uint16_t)hi << 8) | lo);
    ok = true;
done:
    i2c_stop();
    return ok;
}

static bool ina226_register_write(uint8_t address, uint8_t reg, uint16_t value)
{
    bool ok = false;
    s_i2c_error = I2C_ERR_NONE;
    if (!i2c_start()) goto done;
    if (!i2c_write_byte((uint8_t)(address << 1))) goto done;
    if (!i2c_write_byte(reg)) goto done;
    if (!i2c_write_byte((uint8_t)(value >> 8))) goto done;
    if (!i2c_write_byte((uint8_t)value)) goto done;
    ok = true;
done:
    i2c_stop();
    return ok;
}

/* Read without changing the INA226. Diagnose the step that failed.
 * Non-timed GPIO reads must never be used in an IRQ handler.
 */
static bool diagnose_register_read(uint8_t addr, uint8_t reg,
                                   uint16_t *value, const char **failed_step)
{
    uint8_t hi = 0u, lo = 0u;
    bool ok = false;
    s_i2c_error = I2C_ERR_NONE;
    *failed_step = "START";
    if (!i2c_start()) goto finish;
    *failed_step = "write address (SLA+W)";
    if (!i2c_write_byte((uint8_t)(addr << 1))) goto finish;
    *failed_step = "register pointer";
    if (!i2c_write_byte(reg)) goto finish;
    *failed_step = "repeated START";
    if (!i2c_start()) goto finish;
    *failed_step = "read address (SLA+R)";
    if (!i2c_write_byte((uint8_t)((addr << 1) | 1u))) goto finish;
    *failed_step = "read first byte";
    if (!i2c_read_byte(&hi, true)) goto finish;
    *failed_step = "read second byte";
    if (!i2c_read_byte(&lo, false)) goto finish;
    *value = (uint16_t)(((uint16_t)hi << 8) | lo);
    *failed_step = "none";
    ok = true;
finish:
    i2c_stop();
    return ok;
}

static bool diagnose_register_write(uint8_t addr, uint8_t reg,
                                    uint16_t value, const char **failed_step)
{
    bool ok = false;
    s_i2c_error = I2C_ERR_NONE;
    *failed_step = "START";
    if (!i2c_start()) goto finish;
    *failed_step = "write address (SLA+W)";
    if (!i2c_write_byte((uint8_t)(addr << 1))) goto finish;
    *failed_step = "register pointer";
    if (!i2c_write_byte(reg)) goto finish;
    *failed_step = "CONFIG high byte";
    if (!i2c_write_byte((uint8_t)(value >> 8))) goto finish;
    *failed_step = "CONFIG low byte";
    if (!i2c_write_byte((uint8_t)value)) goto finish;
    *failed_step = "none";
    ok = true;
finish:
    i2c_stop();
    return ok;
}

/* Scan all legal INA226 addresses (A1/A0 select 0x40..0x4f),
 * including ACK-vs-NACK, ID-read stages and CONFIG write verification.
 * Successful configuration matches the regular meter initialization.
 * Existing PPS/ADC control is intentionally untouched.
 */
void ina226_scan(void)
{
    unsigned ack_count = 0u;
    unsigned valid_count = 0u;
    scl_release();
    sda_release();
    i2c_delay();
    printf("I2C SCAN: PA2/SCL=%s PA3/SDA=%s (idle; expected HIGH/HIGH)\n",
           scl_is_high() ? "HIGH" : "LOW",
           sda_is_high() ? "HIGH" : "LOW");
    printf("I2C SCAN: 7-bit 0x40..0x4F; 0x40 sends 0x80(write)/0x81(read)\n");

    if (!scl_is_high() || !sda_is_high()) {
        printf("I2C SCAN: bus not idle; inspect wiring and pull-ups before scan\n");
        return;
    }

    for (uint8_t addr = INA226_ADDR_FIRST; addr <= INA226_ADDR_LAST; ++addr) {
        s_i2c_error = I2C_ERR_NONE;
        const bool started = i2c_start();
        const bool ack = started && i2c_write_byte((uint8_t)(addr << 1));
        const I2CError probe_error = s_i2c_error;
        i2c_stop();
        if (!ack) {
            printf("I2C SCAN: 0x%02X %s\n", (unsigned)addr,
                   probe_error == I2C_ERR_NACK ? "NACK" : i2c_error_name(probe_error));
            if (probe_error != I2C_ERR_NACK) {
                printf("I2C SCAN: STOP: electrical/bus error, not an address mismatch\n");
                return;
            }
            continue;
        }
        ++ack_count;
        printf("I2C SCAN: 0x%02X ACK\n", (unsigned)addr);
        const char *stage = NULL;
        uint16_t manuf = 0u;
        uint16_t die = 0u;
        if (!diagnose_register_read(addr, INA226_REG_MANUF, &manuf, &stage)) {
            printf("I2C SCAN: 0x%02X MANUF read failed at %s: %s\n",
                   (unsigned)addr, stage, i2c_error_name(s_i2c_error));
            continue;
        }
        printf("I2C SCAN: 0x%02X Manufacturer ID = 0x%04X (expect 0x5449)\n",
               (unsigned)addr, (unsigned)manuf);
        if (!diagnose_register_read(addr, INA226_REG_DIE, &die, &stage)) {
            printf("I2C SCAN: 0x%02X DIE read failed at %s: %s\n",
                   (unsigned)addr, stage, i2c_error_name(s_i2c_error));
            continue;
        }
        printf("I2C SCAN: 0x%02X Die ID = 0x%04X (expect 0x226x)\n",
               (unsigned)addr, (unsigned)die);
        if (manuf != 0x5449u || (die & 0xfff0u) != 0x2260u) {
            printf("I2C SCAN: 0x%02X ID MISMATCH (ACK is real, but no INA226 ID)\n",
                   (unsigned)addr);
            continue;
        }
        ++valid_count;
        /* Same configuration as ina226_read(): measurement continues normally. */
        if (!diagnose_register_write(addr, INA226_REG_CONFIG,
                                     INA226_CONFIG_16AVG, &stage)) {
            printf("I2C SCAN: 0x%02X CONFIG write failed at %s: %s\n",
                   (unsigned)addr, stage, i2c_error_name(s_i2c_error));
            continue;
        }
        uint16_t readback = 0u;
        if (!diagnose_register_read(addr, INA226_REG_CONFIG, &readback, &stage)) {
            printf("I2C SCAN: 0x%02X CONFIG readback failed at %s: %s\n",
                   (unsigned)addr, stage, i2c_error_name(s_i2c_error));
            continue;
        }
        if (readback != INA226_CONFIG_16AVG) {
            printf("I2C SCAN: 0x%02X CONFIG mismatch: wrote 0x%04X read 0x%04X\n",
                   (unsigned)addr, (unsigned)INA226_CONFIG_16AVG,
                   (unsigned)readback);
            continue;
        }
        s_sensor_address = addr;
        s_initialized = true;
        printf("I2C SCAN: 0x%02X CONFIG=0x%04X OK; INA226 READY\n",
               (unsigned)addr, (unsigned)readback);
    }
    printf("I2C SCAN: complete: %u ACK address(es), %u INA226 ID match(es)\n",
           ack_count, valid_count);
    if (!ack_count) {
        printf("I2C SCAN: no ACK; check PA2/PA3, pin order, pull-ups and waveform\n");
    } else if (!valid_count) {
        printf("I2C SCAN: ACK but no INA226 ID; check read transaction and device type\n");
    }
}

static bool find_and_configure(void)
{
    uint16_t id = 0u;
    uint16_t die = 0u;
    for (uint8_t addr = INA226_ADDR_FIRST; addr <= INA226_ADDR_LAST; ++addr) {
        if (!ina226_register_read(addr, INA226_REG_MANUF, &id) || id != 0x5449u)
            continue;
        if (!ina226_register_read(addr, INA226_REG_DIE, &die) ||
            (die & 0xfff0u) != 0x2260u)
            continue;
        if (!ina226_register_write(addr, INA226_REG_CONFIG, INA226_CONFIG_16AVG))
            continue;
        s_sensor_address = addr;
        s_initialized = true;
        return true;
    }
    return false;
}

bool ina226_read(INA226Reading *reading)
{
    if (!reading) return false;
    if (!s_initialized && !find_and_configure()) return false;
    uint16_t bus_raw = 0u;
    uint16_t shunt_raw = 0u;
    if (!ina226_register_read(s_sensor_address, INA226_REG_BUS, &bus_raw) ||
        !ina226_register_read(s_sensor_address, INA226_REG_SHUNT, &shunt_raw)) {
        s_initialized = false; /* allow recovery after disconnect */
        return false;
    }
    reading->address = s_sensor_address;
    reading->bus_raw = bus_raw;
    reading->shunt_raw = (int16_t)shunt_raw;
    reading->bus_mv = ina226_bus_mv(bus_raw);
    reading->current_ua = ina226_r010_current_ua((int16_t)shunt_raw);
    reading->power_mw = ina226_power_mw(reading->bus_mv, reading->current_ua);
    return true;
}
