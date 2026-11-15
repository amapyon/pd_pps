#include <stdio.h>
#include <assert.h>
#define M8E_IRQ_HOST_TEST 1
#include "../m8e_irq_trace.h"
#include "../m8d_trace.h"
#include "mock_irq_usb.h"
M8E_TestUsbRegs m8e_test_regs;
M8E_TestUsbContext USBFSCTX;
volatile M8D_UsbTrace m8d_usb_trace;
int main(void)
{
    M8E_IrqTraceInit();
    assert(m8e_irq_trace.magic == 0x4d384952u);
    assert(m8e_irq_trace.struct_size == 2120u);
    assert(m8e_irq_trace.event_size == 32u);
    USBFSCTX.endpoints[3].busy = 1;
    USBFSCTX.endpoints[3].mode = 4;
    m8e_test_regs.ep3_len = 48;
    m8e_test_regs.ep3_ctrl = 0x40;
    m8d_usb_trace.main_loops = 1234;
    M8E_IrqEntry((uint16_t)(2 | (2<<12) | (3<<8) | (1<<14)));
    assert(m8e_irq_trace.ep3_in_total == 1 && m8e_irq_trace.ep3_in_tog_ok == 1);
    M8E_IrqEntry((uint16_t)(2 | (2<<12) | (3<<8)));
    assert(m8e_irq_trace.ep3_in_tog_bad == 1);
    M8E_IrqEntry((uint16_t)(2 | (3<<12)));
    assert(m8e_irq_trace.ep0_setup_total == 1);
    M8E_IrqEntry((uint16_t)(2 | (2<<8)));
    assert(m8e_irq_trace.ep2_out_total == 1);
    M8E_IrqEntry((uint16_t)(1 | 4));
    assert(m8e_irq_trace.bus_reset_total == 1 && m8e_irq_trace.suspend_total == 1);
    for (int i=0;i<100;i++) M8E_IrqEntry(0);
    assert(m8e_irq_trace.irq_total == 105);
    assert(m8e_irq_trace.events[(105-1)&63].sequence == 105);
    assert(m8e_irq_trace.events[0].sequence == 65);
    assert(m8e_irq_trace.events[0].main_loops == 1234);
    puts("PASS: M8E IRQ-entry counters, token decoding, toggles, 64-record wrapping");
    return 0;
}
