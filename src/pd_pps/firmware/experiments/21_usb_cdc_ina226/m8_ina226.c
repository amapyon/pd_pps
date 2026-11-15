/* M8 hardware driver: CJMCU-226 / INA226 R010, following experiment 11.
 * CH32X035: PA2=SCL, PA3=SDA, software I2C, 7-bit addr 0x40.
 * The CH32X035 GPIO does NOT drive a logic HIGH on the I2C wires:
 * HIGH means input-floating; LOW means push-pull output low.
 * External pull-ups must be to 3.3 V. See README before wiring.
 * No calibration register needed: shunt raw × 2.5uV / 10mOhm = raw/4 mA.
 */
#include <stdint.h>
#include <stddef.h>
#include "m8_ina226.h"

#define INA226_ADDR 0x40u
#define INA226_CONFIG_REG 0x00u
#define INA226_SHUNT_REG 0x01u
#define INA226_BUS_REG 0x02u
#define INA226_MFR_ID_REG 0xfeu
#define INA226_DIE_ID_REG 0xffu
#define INA226_MFR_ID 0x5449u
#define INA226_DIE_ID 0x2260u
#define INA226_CONFIG 0x4527u /* verified baseline in experiment 11 */

uint32_t M8_INA226_BusRawToMilliVolts(uint16_t raw)
{
    /* 1.25mV per count; round nearest mV. */
    return ((uint32_t)raw * 5u + 2u) / 4u;
}
int32_t M8_INA226_ShuntRawToMilliAmps(int16_t raw)
{
    /* 2.5uV per count / 0.010ohm = 0.25mA per count. */
    int32_t n = raw;
    return (n >= 0 ? n + 2 : n - 2) / 4;
}

#ifdef M8_INA226_HOST_TEST
/* Host-only mocks: no sensor values are substituted in the target image. */
extern int M8_HostReadRegister(uint8_t reg, uint16_t *value);
extern int M8_HostWriteRegister(uint8_t reg, uint16_t value);
extern void M8_HostConversionWait(void);
#define read_register M8_HostReadRegister
#define write_register M8_HostWriteRegister
#define conversion_wait M8_HostConversionWait
#else
/* Register addresses from CH32X035 Reference Manual, RCC and GPIO chapters. */
#define MMIO32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#define RCC_APB2PCENR MMIO32(0x40021018u)
#define GPIOA_CFGLR   MMIO32(0x40010800u)
#define GPIOA_INDR    MMIO32(0x40010808u)
#define GPIOA_BCR     MMIO32(0x40010814u)
#define SCL_PIN 2u
#define SDA_PIN 3u
#define MODE_IN_FLOATING 0x4u
#define MODE_OUT_2MHZ_PP 0x2u
#include "ch32fun.h"
static int pins_initialized;

static void pin_mode(unsigned pin, uint32_t mode)
{
    uint32_t shift = pin * 4u;
    uint32_t cfg = GPIOA_CFGLR;
    GPIOA_CFGLR = (cfg & ~(0xfu << shift)) | (mode << shift);
}
static void drive_low(unsigned pin)
{
    GPIOA_BCR = 1u << pin;
    pin_mode(pin, MODE_OUT_2MHZ_PP);
}
static void release_high(unsigned pin)
{
    pin_mode(pin, MODE_IN_FLOATING);
}
static int is_high(unsigned pin) { return (GPIOA_INDR & (1u << pin)) != 0; }
static void tick(void) { Delay_Us(3u); }
static int clock_high(void)
{
    release_high(SCL_PIN);
    for (unsigned i = 0; i < 80u; ++i) {
        if (is_high(SCL_PIN)) { tick(); return 1; }
        Delay_Us(2u);
    }
    return 0; /* bounded clock stretch / disconnected wire */
}
static int bus_init(void)
{
    if (!pins_initialized) {
        RCC_APB2PCENR |= 1u << 2; /* IOPAEN */
        release_high(SDA_PIN);
        release_high(SCL_PIN);
        pins_initialized = 1;
    }
    tick();
    return is_high(SDA_PIN) && is_high(SCL_PIN);
}
static int start(void)
{
    release_high(SDA_PIN);
    if (!clock_high()) return 0;
    if (!is_high(SDA_PIN)) return 0;
    drive_low(SDA_PIN); tick();
    drive_low(SCL_PIN); tick();
    return 1;
}
static void stop(void)
{
    drive_low(SDA_PIN); tick();
    (void)clock_high();
    release_high(SDA_PIN); tick();
}
static int write_byte(uint8_t b)
{
    for (unsigned bit = 0; bit < 8u; ++bit) {
        if (b & 0x80u) release_high(SDA_PIN);
        else drive_low(SDA_PIN);
        tick();
        if (!clock_high()) { drive_low(SCL_PIN); return 0; }
        drive_low(SCL_PIN); tick();
        b <<= 1;
    }
    release_high(SDA_PIN); tick();
    if (!clock_high()) { drive_low(SCL_PIN); return 0; }
    int ack = !is_high(SDA_PIN);
    drive_low(SCL_PIN); tick();
    return ack;
}
static int read_byte(uint8_t *out, int ack)
{
    uint8_t v = 0;
    release_high(SDA_PIN);
    for (unsigned bit = 0; bit < 8u; ++bit) {
        tick();
        if (!clock_high()) { drive_low(SCL_PIN); return 0; }
        v = (uint8_t)((v << 1) | (is_high(SDA_PIN) ? 1 : 0));
        drive_low(SCL_PIN); tick();
    }
    if (ack) drive_low(SDA_PIN);
    else release_high(SDA_PIN);
    tick();
    if (!clock_high()) { drive_low(SCL_PIN); release_high(SDA_PIN); return 0; }
    drive_low(SCL_PIN); release_high(SDA_PIN); tick();
    *out = v;
    return 1;
}
static int read_register(uint8_t reg, uint16_t *value)
{
    if (!bus_init()) return 0;
    int ok = 0;
    uint8_t msb = 0, lsb = 0;
    if (!start()) goto done;
    if (!write_byte(INA226_ADDR << 1)) goto done;
    if (!write_byte(reg)) goto done;
    if (!start()) goto done; /* repeated START */
    if (!write_byte((INA226_ADDR << 1) | 1u)) goto done;
    if (!read_byte(&msb, 1)) goto done;
    if (!read_byte(&lsb, 0)) goto done;
    *value = (uint16_t)(((uint16_t)msb << 8) | lsb);
    ok = 1;
 done:
    stop();
    return ok;
}
static int write_register(uint8_t reg, uint16_t value)
{
    if (!bus_init()) return 0;
    int ok = 0;
    if (!start()) goto done;
    if (!write_byte(INA226_ADDR << 1)) goto done;
    if (!write_byte(reg)) goto done;
    if (!write_byte((uint8_t)(value >> 8))) goto done;
    if (!write_byte((uint8_t)value)) goto done;
    ok = 1;
 done:
    stop();
    return ok;
}
static void conversion_wait(void) { Delay_Us(5000u); }
#endif

int M8_INA226_ReadSample(uint32_t *bus_mV, int32_t *current_mA)
{
    static uint8_t configured;
    uint16_t mfr = 0, die = 0, bus_raw = 0, shunt_raw = 0;
    if (!bus_mV || !current_mA) return 0;
    if (!read_register(INA226_MFR_ID_REG, &mfr) || mfr != INA226_MFR_ID ||
        !read_register(INA226_DIE_ID_REG, &die) || die != INA226_DIE_ID) {
        configured = 0;
        return 0;
    }
    if (!configured) {
        uint16_t readback = 0;
        if (!write_register(INA226_CONFIG_REG, INA226_CONFIG) ||
            !read_register(INA226_CONFIG_REG, &readback) ||
            readback != INA226_CONFIG) return 0;
        conversion_wait();
        configured = 1;
    }
    if (!read_register(INA226_BUS_REG, &bus_raw) ||
        !read_register(INA226_SHUNT_REG, &shunt_raw)) {
        configured = 0;
        return 0;
    }
    *bus_mV = M8_INA226_BusRawToMilliVolts(bus_raw);
    *current_mA = M8_INA226_ShuntRawToMilliAmps((int16_t)shunt_raw);
    return 1;
}
