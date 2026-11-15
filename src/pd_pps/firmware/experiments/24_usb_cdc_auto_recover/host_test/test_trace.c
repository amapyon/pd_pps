/* Host-only tests of the SRAM trace ABI, event lifetime, and circular rings. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define M8D_TRACE_HOST_TEST
#include "../m8d_trace.c"
MockUSBRegs mock_regs;
MockUSBState USBFSCTX;
static void require(int condition, const char *message)
{
    if (!condition) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}
int main(void)
{
    M8D_TraceInit();
    require(sizeof(M8D_TraceEvent) == 36u, "ABI event exactly 36 bytes");
    require(sizeof(M8D_UsbTrace) == 60u + 2u * 32u * 36u, "ABI struct size");
    require(m8d_usb_trace.magic == M8D_TRACE_MAGIC, "magic initialized");
    require(m8d_usb_trace.version == 1u, "version initialized");
    mock_regs.EP3_TX_LEN=48;
    mock_regs.EP3_TX_CTRL=0x82;
    mock_regs.INT_FG=2;
    mock_regs.INT_ST=0x23;
    USBFSCTX.endpoints[3].busy=1;
    USBFSCTX.endpoints[3].mode=4;
    M8D_TraceFG(M8D_EV_TX_ACCEPTED,48,11,10);
    require(m8d_usb_trace.fg_events[0].event==M8D_EV_TX_ACCEPTED,"fg accepted recorded");
    require(m8d_usb_trace.fg_events[0].ctx_flags==0x401u,"busy=1 mode=4");
    M8D_TracePoll(11,10);
    require(m8d_usb_trace.outstanding_iterations==1u,"first outstanding iteration");
    require(m8d_usb_trace.fg_count==2u,"first wait snapshot");
    require((m8d_usb_trace.last_ep3_regs&0xffffu)==(0x8200u+48u),"regs snapshot");
    for (unsigned n=1; n<256u; n++) M8D_TracePoll(11,10);
    require(m8d_usb_trace.fg_count==3u,"256th iteration snapshot");
    require(m8d_usb_trace.fg_events[2].arg==256u,"iteration count recorded");
    M8D_TraceIRQ(M8D_EV_EP3_IRQ_IN,0,11,11);
    require(m8d_usb_trace.irq_count==1u,"irq recorded");
    M8D_TracePoll(11,11);
    require(m8d_usb_trace.outstanding_iterations==0u,"end clears outstanding");
    require(m8d_usb_trace.fg_count==4u,"wait end recorded");
    for (uint32_t i=0;i<40u;i++) M8D_TraceFG(M8D_EV_TX_BEGIN,i,11,11);
    require(m8d_usb_trace.fg_count==44u,"fg ring wrapping");
    require(m8d_usb_trace.fg_events[(44u-1u)%32u].arg==39u,"last wrapped event");
    require(m8d_usb_trace.irq_count==1u,"fg events do not overwrite irq ring");
    printf("PASS: M8D SRAM trace ABI, wait sampling, IRQ/FG isolation, wrap\n");
    return 0;
}
