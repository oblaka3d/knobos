#pragma once

// Чистая стейт-машина WiFi-менеджера — без зависимостей от IDF, хост-тестируемая.

typedef enum { WSM_BOOT, WSM_STA_CONNECTING, WSM_STA_OK, WSM_AP, WSM_AP_TRYING } wifi_sm_state_t;
typedef enum { WSM_EV_HAS_CREDS, WSM_EV_NO_CREDS, WSM_EV_GOT_IP, WSM_EV_STA_FAIL, WSM_EV_NEW_CREDS, WSM_EV_TRY_OK, WSM_EV_TRY_FAIL } wifi_sm_event_t;

// Переходы: BOOT+HAS_CREDS->STA_CONNECTING; BOOT+NO_CREDS->AP;
// STA_CONNECTING+GOT_IP->STA_OK; STA_CONNECTING+STA_FAIL->AP;
// AP+NEW_CREDS->AP_TRYING; AP_TRYING+TRY_OK->STA_OK; AP_TRYING+TRY_FAIL->AP;
// STA_OK+STA_FAIL->STA_CONNECTING (реконнект);
// любое другое сочетание -> текущее состояние без изменений.
wifi_sm_state_t wifi_sm_next(wifi_sm_state_t s, wifi_sm_event_t ev);
