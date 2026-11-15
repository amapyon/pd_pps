#include "m5_recovery_nb.h"
#include <string.h>
void m5_usb_recovery_nb_init(m5_usb_recovery_nb_t *s){if(s)memset(s,0,sizeof(*s));}
void m5_usb_recovery_nb_submit_begin(m5_usb_recovery_nb_t *s,uint32_t now_ms){
    if(s&&s->state==M5_USB_ACTIVE){s->submit_ms=now_ms;s->in_flight=true;}
}
void m5_usb_recovery_nb_submit_result(m5_usb_recovery_nb_t *s,bool accepted){
    if(s&&!accepted)s->in_flight=false;
}
void m5_usb_recovery_nb_complete(m5_usb_recovery_nb_t *s){if(s)s->in_flight=false;}
void m5_usb_recovery_nb_bus_reset(m5_usb_recovery_nb_t *s){
    if(s){s->bus_resets++;s->in_flight=false;}
}
m5_usb_recovery_action_t m5_usb_recovery_nb_poll(m5_usb_recovery_nb_t *s,uint32_t now_ms){
    if(!s)return M5_USB_EVENT_NONE;
    /* A bus reset invalidates all pending USB command and reply state. */
    if(s->bus_resets!=s->seen_bus_resets){
        s->seen_bus_resets=s->bus_resets;
        s->in_flight=false;
        return M5_USB_EVENT_DROP_PENDING;
    }
    if(s->state==M5_USB_DISCONNECTED_WAIT){
        if((uint32_t)(now_ms-s->disconnected_at_ms)>=M5_USB_DETACH_HOLD_MS){
            s->state=M5_USB_ACTIVE;
            s->in_flight=false;
            s->reconnects++;
            return M5_USB_EVENT_REINIT;
        }
        return M5_USB_EVENT_NONE;
    }
    if(s->in_flight && (uint32_t)(now_ms-s->submit_ms)>=M5_EP3_TIMEOUT_MS){
        s->tx_timeouts++;
        s->in_flight=false;
        s->state=M5_USB_DISCONNECTED_WAIT;
        s->disconnected_at_ms=now_ms;
        return M5_USB_EVENT_DETACH;
    }
    return M5_USB_EVENT_NONE;
}
