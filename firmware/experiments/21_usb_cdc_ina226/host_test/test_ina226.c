/* Pure conversion and mocked INA226 register access, no target hardware. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define M8_INA226_HOST_TEST
#include "../m8_ina226.c"
static uint16_t regs[256];
static int fail_read, fail_write, waits, writes;
int M8_HostReadRegister(uint8_t reg, uint16_t *value)
{
    if (fail_read) return 0;
    *value = regs[reg];
    return 1;
}
int M8_HostWriteRegister(uint8_t reg, uint16_t value)
{
    if (fail_write) return 0;
    regs[reg] = value;
    writes++;
    return 1;
}
void M8_HostConversionWait(void) { waits++; }
static void need(int pass, const char *why)
{
    if (!pass) { fprintf(stderr, "FAIL: %s\n",why); exit(1); }
}
int main(void)
{
    uint32_t mv = 0;
    int32_t ma = 0;
    need(M8_INA226_BusRawToMilliVolts(5572) == 6965, "bus conversion");
    need(M8_INA226_ShuntRawToMilliAmps(276) == 69, "current conversion");
    need(M8_INA226_ShuntRawToMilliAmps(-276) == -69, "negative current");
    need(M8_INA226_ShuntRawToMilliAmps(4) == 1, "quarter mA to mA");
    need(M8_INA226_ShuntRawToMilliAmps(32767) == 8192, "max positive raw");
    need(M8_INA226_ShuntRawToMilliAmps(-32768) == -8192, "max negative raw");
    need(!M8_INA226_ReadSample(&mv,&ma), "no device never succeeds");
    regs[0xfe] = 0x5449; regs[0xff] = 0x2260;
    regs[0x01] = 276; regs[0x02] = 5572;
    fail_write=1;
    need(!M8_INA226_ReadSample(&mv,&ma), "config write failure");
    fail_write=0;
    need(M8_INA226_ReadSample(&mv,&ma), "real measurement mock");
    need(mv == 6965 && ma == 69, "converted values");
    need(regs[0] == 0x4527 && writes==1 && waits==1, "config once");
    need(M8_INA226_ReadSample(&mv,&ma), "repeat reads");
    need(writes==1, "no redundant config write");
    fail_read=1;
    need(!M8_INA226_ReadSample(&mv,&ma), "failed read returns error");
    fail_read=0;
    need(M8_INA226_ReadSample(&mv,&ma), "recovery from read failure");
    need(writes==2, "reinitializes on failure");
    regs[0xff] = 0;
    need(!M8_INA226_ReadSample(&mv,&ma), "reject wrong device ID");
    need(!M8_INA226_ReadSample(NULL,&ma), "reject NULL voltage pointer");
    puts("PASS: INA226 raw conversion, identity, I2C failure and recovery");
    return 0;
}
