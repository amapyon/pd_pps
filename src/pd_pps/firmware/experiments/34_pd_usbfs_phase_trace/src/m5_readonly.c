#include "m30_diag.h"
#include "m32_diag.h"
#include "m33_diag.h"
#include "m34_diag.h"
#include "m5_readonly.h"
#include <string.h>

/* Small, bounded writer: no printf, heap, or unbounded append. */
typedef struct { char *b; size_t n; bool overflow; } wr_t;
static void putc1(wr_t *w, char c) {
    if (w->n >= M5_WIRE_LIMIT-1) { w->overflow=true; return; }
    w->b[w->n++]=c;
}
static void puts1(wr_t *w, const char *s) { while (*s) putc1(w,*s++); }
static void u32(wr_t *w, uint32_t v) {
    char digits[10]; unsigned n=0;
    do { digits[n++]=(char)('0'+v%10u);v/=10u; } while(v);
    while(n) putc1(w,digits[--n]);
}
static void i32(wr_t *w,int32_t v){
    if(v<0){putc1(w,'-');u32(w,0u-(uint32_t)v);} else u32(w,(uint32_t)v);
}
static void hex32(wr_t *w,uint32_t v){
    const char *hex="0123456789ABCDEF";
    for(int i=7;i>=0;--i)putc1(w,hex[(v>>(i*4))&15u]);
}
static void error(wr_t *w, const char *code) {
    w->n=0; w->overflow=false; puts1(w,"ERR code="); puts1(w,code);
}
static size_t finish(wr_t *w) {
    if(w->overflow){error(w,"reply_too_long");}
    w->b[w->n++]='\n';
    return w->n;
}
static bool eql(const uint8_t *s,size_t n,const char *lit) {
    size_t l=strlen(lit);
    return n==l && memcmp(s,lit,l)==0;
}
static bool prefix(const uint8_t *s,size_t n,const char *lit) {
    size_t l=strlen(lit);
    return n>=l && memcmp(s,lit,l)==0;
}
static const char *state_name(pe_state_t s){
    switch(s){
    case PE_DETACHED:return "detached";
    case PE_WAIT_CAPS:return "wait_caps";
    case PE_WAIT_ACCEPT:return "wait_accept";
    case PE_WAIT_PS_RDY:return "wait_ps_rdy";
    case PE_READY:return "ready";
    case PE_SOFT_RESET_WAIT:return "soft_reset_wait";
    case PE_RECOVERY:return "recovery";
    default:return "detached";
    }
}
static bool valid_caps(const m5_pd_snapshot_t *s){
    return s && s->caps_valid && s->caps_count>0 && s->caps_count<=7 &&
            s->state!=PE_DETACHED && s->state!=PE_RECOVERY;
}
static void print_cap(wr_t *w,unsigned i,uint32_t raw){
    puts1(w,"CAP i=");u32(w,i);
    uint32_t type=raw>>30;
    if(type==0){
        uint32_t mv=((raw>>10)&1023u)*50u, ma=(raw&1023u)*10u;
        if(mv&&ma){
            puts1(w," type=fixed min=");u32(w,mv);
            puts1(w," max=");u32(w,mv);
            puts1(w," ma=");u32(w,ma);
            return;
        }
    } else if(type==3 && ((raw>>28)&3u)==0){
        uint32_t min=((raw>>8)&255u)*100u, max=((raw>>17)&255u)*100u;
        uint32_t ma=(raw&127u)*50u;
        if(min && min<=max && ma){
            puts1(w," type=pps min=");u32(w,min);
            puts1(w," max=");u32(w,max);
            puts1(w," ma=");u32(w,ma);
            return;
        }
    }
    puts1(w," type=unsupported raw=0x");hex32(w,raw);
}
static int32_t whole_ma(int32_t micro_amps) {
    /* Truncation toward zero, matching the existing M5-2 Python reference.
     * Unsigned magnitude avoids undefined negation of INT32_MIN. */
    if(micro_amps<0){
        uint32_t magnitude=0u-(uint32_t)micro_amps;
        return -(int32_t)(magnitude/1000u);
    }
    return micro_amps/1000;
}
size_t m5_readonly_reply(const m5_pd_snapshot_t *snap,
                         const m5_usb_health_t *health,
                         const uint8_t *body,size_t length,char out[64]) {
    if(!out)return 0;
    if(body && length>=5 && memcmp(body,"WDIAG",5)==0)
        return m34_diag_reply(body,length,out);
    if(body && length>=5 && memcmp(body,"TDIAG",5)==0)
        return m33_diag_reply(body,length,out);
    if(body && length>=5 && memcmp(body,"XDIAG",5)==0)
        return m32_diag_reply(body,length,out);
    if(body && length>=4 && memcmp(body,"DIAG",4)==0)
        return m30_diag_reply(body,length,out);
    wr_t w={out,0,false};
    if(!body||length==0||length>M5_MAX_REQUEST){error(&w,"bad_line");return finish(&w);}
    for(size_t i=0;i<length;i++){
        if(body[i]<32u||body[i]>126u){error(&w,"bad_line");return finish(&w);}
    }
    if(eql(body,length,"INFO")) {
        puts1(&w,"INFO board=CH32X035F8U6 firmware=m28_pd_usb_cdc protocol=7");
    } else if(eql(body,length,"HEALTH")) {
        if(!health){error(&w,"snapshot_unavailable");}
        else {
            puts1(&w,"HEALTH bus_resets=");u32(&w,health->bus_resets);
            puts1(&w," tx_timeouts=");u32(&w,health->tx_timeouts);
            puts1(&w," reconnects=");u32(&w,health->reconnects);
        }
    } else if(eql(body,length,"GET_CAPS")) {
        if(!snap){error(&w,"snapshot_unavailable");}
        else if(!valid_caps(snap)){
            puts1(&w,"CAPS status=unavailable count=0 epoch=");u32(&w,snap->caps_epoch);
        }else {
            puts1(&w,"CAPS status=ok count=");u32(&w,snap->caps_count);
            puts1(&w," epoch=");u32(&w,snap->caps_epoch);
        }
    } else if(prefix(body,length,"GET_CAP ")) {
        size_t pos=8;
        if(length==pos||length-pos>2||body[pos]<'1'||body[pos]>'9'){
            error(&w,"bad_arg");
        } else {
            uint32_t index=0;
            for(size_t k=pos;k<length;k++){
                if(body[k]<'0'||body[k]>'9'){error(&w,"bad_arg");return finish(&w);}
                index=index*10u+body[k]-'0';
            }
            if(!snap){error(&w,"snapshot_unavailable");}
            else if(!valid_caps(snap)){error(&w,"caps_unavailable");}
            else if(index>snap->caps_count){error(&w,"out_of_range");}
            else print_cap(&w,index,snap->raw_pdos[index-1u]);
        }
    } else if(eql(body,length,"STATUS")){
        if(!snap){error(&w,"snapshot_unavailable");}
        else {
            puts1(&w,"STATUS state=");puts1(&w,state_name(snap->state));
            bool valid=snap->state==PE_READY && snap->caps_valid && snap->contract_valid &&
                snap->pdo_index>=1 && snap->pdo_index<=snap->caps_count &&
                snap->requested_mv && snap->requested_ma;
            if(!valid){puts1(&w," v=0 pps=0 pos=0 mv=0 ma=0");}
            else {
                puts1(&w," v=1 pps=");u32(&w,snap->pps_active?1u:0u);
                puts1(&w," pos=");u32(&w,snap->pdo_index);
                puts1(&w," mv=");u32(&w,snap->requested_mv);
                puts1(&w," ma=");u32(&w,snap->requested_ma);
            }
        }
    } else if(eql(body,length,"MEASURE")){
        if(!snap){error(&w,"snapshot_unavailable");}
        else if(snap->state!=PE_READY||!snap->ina_valid){
            puts1(&w,"MEASURE status=error reason=sensor_unavailable");
        }else{
            puts1(&w,"MEASURE status=ok voltage_mV=");u32(&w,snap->ina_bus_mv);
            puts1(&w," current_mA=");i32(&w,whole_ma(snap->ina_current_ua));
        }
    } else if(prefix(body,length,"SET_")||prefix(body,length,"OUTPUT_")){
        error(&w,"read_only");
    } else error(&w,"unknown_command");
    return finish(&w);
}
