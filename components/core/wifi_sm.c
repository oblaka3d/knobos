#include "wifi_sm.h"

wifi_sm_state_t wifi_sm_next(wifi_sm_state_t s, wifi_sm_event_t ev) {
    switch (s) {
        case WSM_BOOT:
            if (ev == WSM_EV_HAS_CREDS) return WSM_STA_CONNECTING;
            if (ev == WSM_EV_NO_CREDS) return WSM_AP;
            break;
        case WSM_STA_CONNECTING:
            if (ev == WSM_EV_GOT_IP) return WSM_STA_OK;
            if (ev == WSM_EV_STA_FAIL) return WSM_AP;
            break;
        case WSM_STA_OK:
            if (ev == WSM_EV_STA_FAIL) return WSM_STA_CONNECTING;
            break;
        case WSM_AP:
            if (ev == WSM_EV_NEW_CREDS) return WSM_AP_TRYING;
            break;
        case WSM_AP_TRYING:
            if (ev == WSM_EV_TRY_OK) return WSM_STA_OK;
            if (ev == WSM_EV_TRY_FAIL) return WSM_AP;
            break;
    }
    return s;
}
