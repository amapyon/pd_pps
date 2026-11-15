/* Host-only sanity check for the PPS RDO packing defined in usbpd.h.
 * This checks bit layout/expected wire word, not MCU compilation.
 */
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t OperatingCurrentIn50mA : 7u;
    uint32_t Reserved_7_8bit : 2u;
    uint32_t OutputVoltageIn20mV : 12u;
    uint32_t Reserved_21bit : 1u;
    uint32_t ERPCapable : 1u;
    uint32_t UnchunkedExtendedMessage : 1u;
    uint32_t NoUSBSuspended : 1u;
    uint32_t USBComsCapable : 1u;
    uint32_t CapabilityMissmatch : 1u;
    uint32_t Reserved_27bit : 1u;
    uint32_t ObjectPosition : 4u;
} TestPPSRDO;

static uint32_t pack(uint32_t mv, uint32_t ma)
{
    TestPPSRDO r = {
        .OperatingCurrentIn50mA = ma / 50u,
        .OutputVoltageIn20mV = mv / 20u,
        .NoUSBSuspended = 1u,
        .USBComsCapable = 1u,
        .ObjectPosition = 4u, /* APDO index 3 + 1 */
    };
    uint32_t word = 0;
    _Static_assert(sizeof(r) == sizeof(word), "PPS RDO must be 4 bytes");
    memcpy(&word, &r, sizeof(word));
    return word;
}

int main(void)
{
    assert(pack(7000u, 1800u) == UINT32_C(0x4302bc24));
    assert(pack(7000u, 1000u) == UINT32_C(0x4302bc14));
    assert(pack(3300u, 50u) == UINT32_C(0x43014a01));
    assert(pack(21000u, 3000u) == UINT32_C(0x4308343c));
    printf("PPS RDO test OK: 7V/1.8A=0x%08lx 7V/1.0A=0x%08lx\n",
        (unsigned long)pack(7000, 1800), (unsigned long)pack(7000, 1000));
    return 0;
}
