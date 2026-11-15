#include "m5_usb_io.h"
void m5_usb_io_poll(m5_session_t *session,const m5_usb_io_t *io,bool reset_event){
    if(!session)return;
    if(reset_event){m5_session_usb_reset(session);return;}
    if(!io)return;
    const char *bytes=NULL;
    size_t n=m5_session_tx_peek(session,&bytes);
    if(n){
        if(io->tx_submit && io->tx_submit(io->context,(const uint8_t*)bytes,n)==0){
            m5_session_tx_accepted(session,n);
        }
        return;
    }
    if(io->rx_pop){
        uint8_t ch=0;
        if(io->rx_pop(io->context,&ch)) (void)m5_session_rx(session,ch);
    }
}
