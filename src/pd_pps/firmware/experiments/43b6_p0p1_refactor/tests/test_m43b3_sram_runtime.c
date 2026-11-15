#include "m43b3_sram.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
int main(void) {
    assert(sizeof(m43b3_sram_entry_t)==16);
    assert(sizeof(m43b3_sram_t)==1088);
    assert(offsetof(m43b3_sram_t, records)==64);
    m43b3_sram_attach(100u);
    assert(m43b3_sram.session==1 && m43b3_sram.total==0);
    for (unsigned i=0; i<80; i++) m43b3_sram_note(M43B3_EVT_TX_END, 0, i, i+1, 100+i);
    assert(m43b3_sram.count==64 && m43b3_sram.total==80);
    assert(m43b3_sram.head==16);
    assert(m43b3_sram.records[16].arg0==16);
    assert(m43b3_sram.records[15].arg0==79);
    m43b3_sram_freeze(M43B3_FREEZE_TX_FAILED, 180);
    assert(m43b3_sram.freeze_reason==1 && m43b3_sram.active==0);
    m43b3_sram_note(M43B3_EVT_GOODCRC, 1, 1, 1, 181);
    m43b3_sram_detach(200);
    assert(m43b3_sram.total==80 && m43b3_sram.freeze_reason==1);
    m43b3_sram_attach(500);
    assert(m43b3_sram.session==2 && m43b3_sram.total==0);
    m43b3_sram_trace(8,1,0x123,12,501); /* PDO skipped */
    assert(m43b3_sram.total==0);
    m43b3_sram_trace(12,1,0x123,12,502);
    assert(m43b3_sram.total==1 && m43b3_sram.records[0].flags==12);
    m43b3_sram_detach(503);
    assert(m43b3_sram.freeze_reason==M43B3_FREEZE_DETACH);
    puts("M43B3 SRAM HOST PASS");
    return 0;
}
