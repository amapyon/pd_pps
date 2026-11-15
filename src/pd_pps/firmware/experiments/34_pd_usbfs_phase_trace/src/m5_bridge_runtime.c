#include "m5_bridge_runtime.h"
bool m5_bridge_publish_pd17(m5_session_t *session,const pe_t *pe,
                            const pd_sensor_reading_t *reading,
                            bool sensor_fresh,const m5_usb_health_t *health) {
    if(!session)return false;
    m5_pd_snapshot_t snap;
    if(!m5_pd_snapshot_capture(pe,reading,sensor_fresh,&snap)){
        m5_session_publish(session,NULL,health);
        return false;
    }
    m5_session_publish(session,&snap,health);
    return true;
}
