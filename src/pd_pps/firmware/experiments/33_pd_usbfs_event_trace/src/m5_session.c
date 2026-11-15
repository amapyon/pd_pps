#include "m5_session.h"
#include <string.h>
void m5_session_init(m5_session_t *s) {
    if(!s)return;
    memset(s,0,sizeof(*s));
}
void m5_session_publish(m5_session_t *s,const m5_pd_snapshot_t *snapshot,
                        const m5_usb_health_t *health) {
    if(!s)return;
    if(health)s->health=*health;
    /* Prevent a PD contract/capability transition from leaving an unsent
     * READY response in the transport queue. Already submitted USB packets
     * cannot be revoked; host must still check epoch/connection status. */
    if(m5_session_has_reply(s) && s->snapshot_ready &&
       (!snapshot || s->snapshot.state!=snapshot->state ||
        s->snapshot.caps_epoch!=snapshot->caps_epoch ||
        s->snapshot.caps_valid!=snapshot->caps_valid ||
        s->snapshot.contract_valid!=snapshot->contract_valid ||
        s->snapshot.pdo_index!=snapshot->pdo_index ||
        s->snapshot.requested_mv!=snapshot->requested_mv ||
        s->snapshot.requested_ma!=snapshot->requested_ma ||
        s->snapshot.pps_active!=snapshot->pps_active)) {
        s->reply_len=0;s->reply_sent=0;
    }
    if(snapshot){s->snapshot=*snapshot;s->snapshot_ready=true;}
    else {memset(&s->snapshot,0,sizeof(s->snapshot));s->snapshot_ready=false;}
}
void m5_session_usb_reset(m5_session_t *s) {
    if(!s)return;
    s->command_len=0;s->line_bad=false;
    s->reply_len=0;s->reply_sent=0;
    s->snapshot_ready=false;
    memset(&s->snapshot,0,sizeof(s->snapshot));
}
bool m5_session_has_reply(const m5_session_t *s){
    return s && s->reply_sent<s->reply_len;
}
static void response(m5_session_t *s,const uint8_t *bytes,size_t len){
    if(s->line_bad||len==0){
        s->reply_len=(uint8_t)m5_readonly_reply(NULL,NULL,NULL,0,s->reply);
        s->rejected_lines++;
    } else {
        s->reply_len=(uint8_t)m5_readonly_reply(s->snapshot_ready?&s->snapshot:NULL,
               &s->health,bytes,len,s->reply);
    }
    s->reply_sent=0;
    s->command_len=0;
    s->line_bad=false;
    s->commands_completed++;
}
bool m5_session_rx(m5_session_t *s,uint8_t ch){
    if(!s||m5_session_has_reply(s))return false;
    if(ch=='\n') {
        /* Count LF in the 63-byte request limit, including optional CR. */
        if((size_t)s->command_len+1u > M5_MAX_REQUEST) s->line_bad=true;
        /* A single terminal CR is permitted, but CR inside the body is not. */
        size_t n=s->command_len;
        if(n && s->command[n-1u]=='\r')n--;
        response(s,s->command,n);
        return true;
    }
    if(s->command_len>=M5_MAX_REQUEST){s->line_bad=true;return true;}
    if(ch<' '||ch>'~'){
        if(ch!='\r')s->line_bad=true;
    }
    s->command[s->command_len++]=ch;
    /* At most one CR immediately before the LF. */
    if(ch!='\r'&&s->command_len>=2&&s->command[s->command_len-2u]=='\r')s->line_bad=true;
    return true;
}
size_t m5_session_tx_peek(const m5_session_t *s,const char **data){
    if(data)*data=NULL;
    if(!s||!m5_session_has_reply(s))return 0;
    size_t n=(size_t)(s->reply_len-s->reply_sent);
    if(n>M5_WIRE_LIMIT)n=M5_WIRE_LIMIT;
    if(data)*data=&s->reply[s->reply_sent];
    return n;
}
void m5_session_tx_accepted(m5_session_t *s,size_t size){
    if(!s||!m5_session_has_reply(s))return;
    const size_t avail=(size_t)(s->reply_len-s->reply_sent);
    if(size==0||size>avail||size>M5_WIRE_LIMIT)return;
    s->reply_sent=(uint8_t)(s->reply_sent+size);
    if(s->reply_sent==s->reply_len){s->reply_sent=0;s->reply_len=0;}
}
