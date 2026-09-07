#include "wifi_mgr.h"
#include "settings.h"
#include "ids.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "wifi_mgr";

#define STA_CONNECT_TIMEOUT_MS 15000
#define TRY_CREDENTIALS_TIMEOUT_MS 15000
#define RESTART_DELAY_MS 1500
#define AP_PASS_RANDOM_BYTES 8
#define WIFI_SCAN_MAX_APS 20
#define WIFI_EV_GOT_IP BIT0

static const uint32_t RECONNECT_BACKOFF_MS[] = {1000, 5000, 15000};
#define RECONNECT_BACKOFF_COUNT (sizeof(RECONNECT_BACKOFF_MS) / sizeof(RECONNECT_BACKOFF_MS[0]))

static wifi_sm_state_t s_state = WSM_BOOT;
static wifi_mgr_status_cb_t s_status_cb = NULL;
static EventGroupHandle_t s_wifi_ev;
static char s_ip[16] = "";
static char s_ap_ssid[16] = "";
static char s_ap_pass[16] = "";
static bool s_ap_netif_created = false;
static bool s_wifi_started = false;
static bool s_reconnecting = false; // true = в фазе реконнекта с backoff (после потери STA_OK)
static int s_backoff_idx = 0;
static esp_timer_handle_t s_reconnect_timer;
static esp_timer_handle_t s_restart_timer;

// Единственная точка мутации s_state — уведомляет колбэк только при реальной смене состояния.
static void transition(wifi_sm_event_t ev, const char *detail) {
    wifi_sm_state_t next = wifi_sm_next(s_state, ev);
    bool changed = (next != s_state);
    s_state = next;
    if (changed && s_status_cb) s_status_cb(next, detail ? detail : "");
}

static void format_ip(const esp_ip4_addr_t *ip, char *out, size_t out_len) {
    snprintf(out, out_len, IPSTR, IP2STR(ip));
}

static void ensure_ap_creds(const uint8_t mac[6]) {
    knob_ap_ssid(mac, s_ap_ssid);
    if (!settings_get_str("ap_pass", s_ap_pass, sizeof(s_ap_pass))) {
        uint8_t rnd[AP_PASS_RANDOM_BYTES];
        esp_fill_random(rnd, sizeof(rnd));
        knob_gen_pass(rnd, sizeof(rnd), s_ap_pass, sizeof(s_ap_pass));
        settings_set_str("ap_pass", s_ap_pass);
    }
    char web_pass[16];
    if (!settings_get_str("web_pass", web_pass, sizeof(web_pass))) {
        settings_set_str("web_pass", s_ap_pass);
    }
}

static void bring_up_ap(const uint8_t mac[6]) {
    if (!s_ap_netif_created) {
        esp_netif_create_default_wifi_ap();
        s_ap_netif_created = true;
    }
    ensure_ap_creds(mac);

    wifi_config_t ap_cfg = {0};
    strncpy((char *)ap_cfg.ap.ssid, s_ap_ssid, sizeof(ap_cfg.ap.ssid) - 1);
    ap_cfg.ap.ssid_len = strlen(s_ap_ssid);
    strncpy((char *)ap_cfg.ap.password, s_ap_pass, sizeof(ap_cfg.ap.password) - 1);
    ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap_cfg.ap.max_connection = 4;
    ap_cfg.ap.channel = 1;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    if (!s_wifi_started) {
        ESP_ERROR_CHECK(esp_wifi_start());
        s_wifi_started = true;
    }
    ESP_LOGI(TAG, "AP up: ssid=%s", s_ap_ssid);
}

static void schedule_reconnect(void) {
    size_t idx = (s_backoff_idx < (int)RECONNECT_BACKOFF_COUNT) ? (size_t)s_backoff_idx : RECONNECT_BACKOFF_COUNT - 1;
    uint32_t delay_ms = RECONNECT_BACKOFF_MS[idx];
    if (s_backoff_idx < (int)RECONNECT_BACKOFF_COUNT - 1) s_backoff_idx++;
    esp_timer_stop(s_reconnect_timer); // не критично, если уже остановлен
    ESP_ERROR_CHECK(esp_timer_start_once(s_reconnect_timer, (uint64_t)delay_ms * 1000));
}

static void reconnect_timer_cb(void *arg) {
    esp_wifi_connect();
}

static void restart_timer_cb(void *arg) {
    esp_restart();
}

static void handle_disconnected(void) {
    if (s_state == WSM_STA_OK) {
        // реальная потеря соединения в устойчивом состоянии -> реконнект с backoff
        transition(WSM_EV_STA_FAIL, "");
        s_ip[0] = '\0';
        s_reconnecting = true;
        s_backoff_idx = 0;
        schedule_reconnect();
        return;
    }
    if (s_reconnecting) {
        // очередная попытка реконнекта провалилась -> следующий шаг backoff
        schedule_reconnect();
        return;
    }
    if (s_state == WSM_STA_CONNECTING || s_state == WSM_AP_TRYING) {
        // первичное подключение при старте или попытка кредов из портала — ретраим
        // немедленно, пока не истёк блокирующий таймаут вызывающей функции
        esp_wifi_connect();
    }
}

static void handle_got_ip(const ip_event_got_ip_t *evt) {
    format_ip(&evt->ip_info.ip, s_ip, sizeof(s_ip));
    if (s_state == WSM_STA_CONNECTING) {
        s_reconnecting = false;
        s_backoff_idx = 0;
        esp_timer_stop(s_reconnect_timer);
        transition(WSM_EV_GOT_IP, s_ip);
        xEventGroupSetBits(s_wifi_ev, WIFI_EV_GOT_IP);
    } else if (s_state == WSM_AP_TRYING) {
        transition(WSM_EV_TRY_OK, s_ip);
        xEventGroupSetBits(s_wifi_ev, WIFI_EV_GOT_IP);
    }
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        handle_disconnected();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        handle_got_ip((const ip_event_got_ip_t *)data);
    }
}

esp_err_t wifi_mgr_start(wifi_mgr_status_cb_t cb) {
    s_status_cb = cb;
    s_wifi_ev = xEventGroupCreate();
    if (!s_wifi_ev) return ESP_ERR_NO_MEM;

    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t loop_err = esp_event_loop_create_default();
    if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(loop_err);
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_event, NULL));

    const esp_timer_create_args_t reconnect_args = { .callback = reconnect_timer_cb, .name = "wifi_reconnect" };
    ESP_ERROR_CHECK(esp_timer_create(&reconnect_args, &s_reconnect_timer));
    const esp_timer_create_args_t restart_args = { .callback = restart_timer_cb, .name = "wifi_restart" };
    ESP_ERROR_CHECK(esp_timer_create(&restart_args, &s_restart_timer));

    char ssid[33] = {0};
    char pass[65] = {0};
    bool has_creds = settings_get_str("wifi_ssid", ssid, sizeof(ssid));
    if (has_creds) settings_get_str("wifi_pass", pass, sizeof(pass));

    if (has_creds) {
        transition(WSM_EV_HAS_CREDS, "");
        wifi_config_t sta_cfg = {0};
        strncpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid) - 1);
        strncpy((char *)sta_cfg.sta.password, pass, sizeof(sta_cfg.sta.password) - 1);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
        ESP_ERROR_CHECK(esp_wifi_start());
        s_wifi_started = true;
        esp_wifi_connect();

        EventBits_t bits = xEventGroupWaitBits(s_wifi_ev, WIFI_EV_GOT_IP, pdFALSE, pdFALSE, pdMS_TO_TICKS(STA_CONNECT_TIMEOUT_MS));
        if (!(bits & WIFI_EV_GOT_IP)) bits = xEventGroupGetBits(s_wifi_ev); // закрыть гонку таймаут/GOT_IP
        if (!(bits & WIFI_EV_GOT_IP)) {
            uint8_t mac[6];
            esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
            ensure_ap_creds(mac); // до transition — колбэк (ui_onboarding_show) читает wifi_mgr_ap_ssid/pass сразу
            transition(WSM_EV_STA_FAIL, "sta timeout");
            bring_up_ap(mac);
        }
    } else {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
        ensure_ap_creds(mac); // до transition — колбэк (ui_onboarding_show) читает wifi_mgr_ap_ssid/pass сразу
        transition(WSM_EV_NO_CREDS, "");
        bring_up_ap(mac);
    }
    return ESP_OK;
}

esp_err_t wifi_mgr_try_credentials(const char *ssid, const char *pass) {
    if (!ssid || !pass) return ESP_ERR_INVALID_ARG;
    if (s_state != WSM_AP) return ESP_ERR_INVALID_STATE;

    transition(WSM_EV_NEW_CREDS, "");

    wifi_config_t sta_cfg = {0};
    strncpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char *)sta_cfg.sta.password, pass, sizeof(sta_cfg.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    xEventGroupClearBits(s_wifi_ev, WIFI_EV_GOT_IP);
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(s_wifi_ev, WIFI_EV_GOT_IP, pdFALSE, pdFALSE, pdMS_TO_TICKS(TRY_CREDENTIALS_TIMEOUT_MS));
    if (!(bits & WIFI_EV_GOT_IP)) bits = xEventGroupGetBits(s_wifi_ev); // закрыть гонку таймаут/GOT_IP
    if (bits & WIFI_EV_GOT_IP) {
        settings_set_str("wifi_ssid", ssid);
        settings_set_str("wifi_pass", pass);
        ESP_ERROR_CHECK(esp_timer_start_once(s_restart_timer, (uint64_t)RESTART_DELAY_MS * 1000));
        return ESP_OK;
    }

    transition(WSM_EV_TRY_FAIL, "wrong password or AP not found");
    return ESP_FAIL;
}

void wifi_mgr_reset_credentials(void) {
    settings_erase_key("wifi_ssid");
    settings_erase_key("wifi_pass");
    esp_restart();
}

bool wifi_mgr_is_provisioned(void) {
    char buf[33];
    return settings_get_str("wifi_ssid", buf, sizeof(buf));
}

const char *wifi_mgr_ip(void) { return s_ip; }
const char *wifi_mgr_ap_ssid(void) { return s_ap_ssid; }
const char *wifi_mgr_ap_pass(void) { return s_ap_pass; }

// SSID соседних сетей — недоверенные данные, попадают в JSON как строковое значение.
// Экранирует " и \ и управляющие символы <0x20 (как \u00XX), иначе битый JSON при кавычке/бэкслеше в SSID.
#define ESCAPED_SSID_MAX 200 // худший случай: 32 байта SSID * \u00XX (6 симв.) + '\0'
static void json_escape_ssid(const char *ssid, char *out, size_t out_len) {
    size_t pos = 0;
    for (size_t i = 0; ssid[i] != '\0' && pos + 1 < out_len; i++) {
        unsigned char c = (unsigned char)ssid[i];
        if (c == '"' || c == '\\') {
            if (pos + 2 >= out_len) break;
            out[pos++] = '\\';
            out[pos++] = (char)c;
        } else if (c < 0x20) {
            if (pos + 6 >= out_len) break;
            pos += (size_t)snprintf(out + pos, out_len - pos, "\\u%04x", c);
        } else {
            out[pos++] = (char)c;
        }
    }
    out[pos] = '\0';
}

esp_err_t wifi_mgr_scan_json(char *out, size_t out_len) {
    if (!out || out_len < 3) return ESP_ERR_INVALID_ARG; // минимум место под "[]" + '\0'

    wifi_scan_config_t scan_cfg = {0};
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
    if (err != ESP_OK) { snprintf(out, out_len, "[]"); return err; }

    uint16_t ap_num = 0;
    esp_wifi_scan_get_ap_num(&ap_num);
    if (ap_num > WIFI_SCAN_MAX_APS) ap_num = WIFI_SCAN_MAX_APS;

    wifi_ap_record_t records[WIFI_SCAN_MAX_APS];
    uint16_t ap_count = ap_num;
    err = esp_wifi_scan_get_ap_records(&ap_count, records);
    if (err != ESP_OK) { snprintf(out, out_len, "[]"); return err; }

    // Схлопнуть дубликаты ssid, оставляя запись с сильнейшим rssi.
    int unique_count = 0;
    for (int i = 0; i < ap_count; i++) {
        int dup = -1;
        for (int j = 0; j < unique_count; j++) {
            if (strcmp((const char *)records[j].ssid, (const char *)records[i].ssid) == 0) { dup = j; break; }
        }
        if (dup >= 0) {
            if (records[i].rssi > records[dup].rssi) records[dup] = records[i];
        } else {
            records[unique_count++] = records[i];
        }
    }

    // Сортировка по убыванию rssi — вставками, n <= 20.
    for (int i = 1; i < unique_count; i++) {
        wifi_ap_record_t key = records[i];
        int j = i - 1;
        while (j >= 0 && records[j].rssi < key.rssi) { records[j + 1] = records[j]; j--; }
        records[j + 1] = key;
    }

    size_t pos = 0;
    out[pos++] = '[';
    for (int i = 0; i < unique_count; i++) {
        if (pos + 2 >= out_len) break; // не осталось места даже под "]" + '\0'
        size_t avail = out_len - pos - 2; // зарезервировать ']' + '\0'
        bool secure = records[i].authmode != WIFI_AUTH_OPEN;
        char escaped_ssid[ESCAPED_SSID_MAX];
        json_escape_ssid((const char *)records[i].ssid, escaped_ssid, sizeof(escaped_ssid));
        int written = snprintf(out + pos, avail + 1, "%s{\"ssid\":\"%s\",\"rssi\":%d,\"secure\":%s}",
                                i == 0 ? "" : ",", escaped_ssid, records[i].rssi, secure ? "true" : "false");
        if (written < 0 || (size_t)written > avail) break; // не влезло — не портим JSON частичной записью
        pos += (size_t)written;
    }
    out[pos++] = ']';
    out[pos] = '\0';
    return ESP_OK;
}
