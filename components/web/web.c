#include "web.h"
#include "wifi_mgr.h"
#include "settings.h"
#include "esp_netif.h" // esp_ip4_addr_t для dns_server.h
#include "dns_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "web";

#define WIFI_TRY_TASK_STACK 4096
#define WIFI_TRY_TASK_PRIO 5
#define POST_BODY_MAX_LEN 512
#define TRY_SSID_MAX_LEN 33
#define TRY_PASS_MAX_LEN 65
#define WIFI_SCAN_JSON_BUF_LEN 2048
#define STATUS_DETAIL_MAX_LEN 64
#define STATUS_RESP_BUF_LEN 128
#define WEB_PASS_MAX_LEN 16
#define AUTH_CRED_MAX_LEN (6 + WEB_PASS_MAX_LEN) // "admin:" + pass + '\0'
#define AUTH_B64_MAX_LEN 32 // base64(AUTH_CRED_MAX_LEN) с запасом + '\0'
#define AUTH_HEADER_MAX_LEN (6 + AUTH_B64_MAX_LEN) // "Basic " + base64 + '\0'

extern const uint8_t portal_html_gz_start[] asm("_binary_portal_html_gz_start");
extern const uint8_t portal_html_gz_end[] asm("_binary_portal_html_gz_end");

static httpd_handle_t s_httpd = NULL;
static dns_server_handle_t s_dns = NULL;

// Ожидаемый заголовок "Basic <base64>" для web_start_normal — считается один раз
// при старте нормального httpd, дальше только strcmp с заголовком запроса.
static char s_expected_auth[AUTH_HEADER_MAX_LEN] = "";

// Последнее состояние wifi_mgr (для /api/wifi/status) — пишется из main-таски колбэком
// wifi_mgr, читается из httpd-таски при GET-запросе.
static wifi_sm_state_t s_last_state = WSM_BOOT;
static char s_last_detail[STATUS_DETAIL_MAX_LEN] = "";

// Гонка из ревью T2: сериализует конкурентные POST /api/wifi — ставится в хендлере
// до старта таски-попытки, снимается в конце этой таски.
static atomic_bool s_try_in_flight = false;
static char s_try_ssid[TRY_SSID_MAX_LEN];
static char s_try_pass[TRY_PASS_MAX_LEN];

void web_notify_wifi_state(wifi_sm_state_t st, const char *detail) {
    s_last_state = st;
    if (detail && detail[0]) {
        strncpy(s_last_detail, detail, sizeof(s_last_detail) - 1);
        s_last_detail[sizeof(s_last_detail) - 1] = '\0';
    } else {
        s_last_detail[0] = '\0';
    }
}

static esp_err_t root_get_handler(httpd_req_t *req) {
    size_t len = (size_t)(portal_html_gz_end - portal_html_gz_start);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_send(req, (const char *)portal_html_gz_start, len);
    return ESP_OK;
}

static esp_err_t api_wifi_scan_get_handler(httpd_req_t *req) {
    char *scan_buf = malloc(WIFI_SCAN_JSON_BUF_LEN);
    if (!scan_buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    wifi_mgr_scan_json(scan_buf, WIFI_SCAN_JSON_BUF_LEN);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, scan_buf);
    free(scan_buf);
    return ESP_OK;
}

static esp_err_t api_wifi_status_get_handler(httpd_req_t *req) {
    const char *state_str;
    switch (s_last_state) {
    case WSM_AP:
        // detail непустой -> уже пробовали кредами из портала (или сохранёнными) и не вышло.
        state_str = s_last_detail[0] ? "fail" : "ap";
        break;
    case WSM_AP_TRYING:
        state_str = "trying";
        break;
    case WSM_STA_OK:
        state_str = "ok";
        break;
    default:
        state_str = "ap";
        break;
    }
    char resp[STATUS_RESP_BUF_LEN];
    snprintf(resp, sizeof(resp), "{\"state\":\"%s\",\"detail\":\"%s\"}", state_str, s_last_detail);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp);
    return ESP_OK;
}

// Отдельная таска: блокирующий wifi_mgr_try_credentials не должен держать httpd-хендлер,
// который уже ответил клиенту (см. api_wifi_post_handler).
static void wifi_try_task(void *arg) {
    wifi_mgr_try_credentials(s_try_ssid, s_try_pass);
    atomic_store(&s_try_in_flight, false);
    vTaskDelete(NULL);
}

static esp_err_t api_wifi_post_handler(httpd_req_t *req) {
    bool expected = false;
    if (!atomic_compare_exchange_strong(&s_try_in_flight, &expected, true)) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"busy\"}");
        return ESP_OK;
    }

    if (req->content_len <= 0 || req->content_len > POST_BODY_MAX_LEN) {
        atomic_store(&s_try_in_flight, false);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body size");
        return ESP_FAIL;
    }

    char body[POST_BODY_MAX_LEN + 1];
    int total = req->content_len;
    int cur = 0;
    while (cur < total) {
        int r = httpd_req_recv(req, body + cur, total - cur);
        if (r <= 0) {
            atomic_store(&s_try_in_flight, false);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        cur += r;
    }
    body[cur] = '\0';

    cJSON *json = cJSON_Parse(body);
    const cJSON *ssid_item = json ? cJSON_GetObjectItemCaseSensitive(json, "ssid") : NULL;
    const cJSON *pass_item = json ? cJSON_GetObjectItemCaseSensitive(json, "pass") : NULL;
    if (!cJSON_IsString(ssid_item) || ssid_item->valuestring[0] == '\0') {
        cJSON_Delete(json);
        atomic_store(&s_try_in_flight, false);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"invalid ssid\"}");
        return ESP_OK;
    }

    strncpy(s_try_ssid, ssid_item->valuestring, sizeof(s_try_ssid) - 1);
    s_try_ssid[sizeof(s_try_ssid) - 1] = '\0';
    const char *pass_val = cJSON_IsString(pass_item) ? pass_item->valuestring : "";
    strncpy(s_try_pass, pass_val, sizeof(s_try_pass) - 1);
    s_try_pass[sizeof(s_try_pass) - 1] = '\0';
    cJSON_Delete(json);

    // До ответа сбросить detail от предыдущей попытки — иначе /api/wifi/status,
    // опрошенный сразу после ответа (пока wifi_try_task ещё не стартовала), увидит
    // старый WSM_AP+detail и poll() в portal.html примет его за "fail" этой попытки.
    web_notify_wifi_state(WSM_AP_TRYING, "");

    // Ответ клиенту уходит до старта попытки: во время connect() AP-канал прыгает
    // за STA и телефон кратко теряет сеть — он должен успеть получить ответ и
    // дальше опрашивать /api/wifi/status самостоятельно.
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"trying\"}");

    if (xTaskCreate(wifi_try_task, "wifi_try", WIFI_TRY_TASK_STACK, NULL, WIFI_TRY_TASK_PRIO, NULL) != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate(wifi_try_task) failed");
        atomic_store(&s_try_in_flight, false);
        web_notify_wifi_state(WSM_AP, "failed to start connection attempt");
    }
    return ESP_OK;
}

// Ловит generate_204 (Android), hotspot-detect.html (iOS/macOS) и т.п. — любой незнакомый
// GET уводит на страницу портала, чтобы ОС предложила открыть капчер-портал.
static esp_err_t http_404_handler(httpd_req_t *req, httpd_err_code_t err) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, "Redirect to captive portal", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t web_start_portal(void) {
    if (s_httpd) return ESP_OK; // идемпотентно — on_wifi_status может звать повторно

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;

    esp_err_t err = httpd_start(&s_httpd, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }

    const httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler };
    const httpd_uri_t scan_uri = { .uri = "/api/wifi/scan", .method = HTTP_GET, .handler = api_wifi_scan_get_handler };
    const httpd_uri_t post_uri = { .uri = "/api/wifi", .method = HTTP_POST, .handler = api_wifi_post_handler };
    const httpd_uri_t status_uri = { .uri = "/api/wifi/status", .method = HTTP_GET, .handler = api_wifi_status_get_handler };
    httpd_register_uri_handler(s_httpd, &root_uri);
    httpd_register_uri_handler(s_httpd, &scan_uri);
    httpd_register_uri_handler(s_httpd, &post_uri);
    httpd_register_uri_handler(s_httpd, &status_uri);
    httpd_register_err_handler(s_httpd, HTTPD_404_NOT_FOUND, http_404_handler);
    ESP_LOGI(TAG, "httpd started on port %d", config.server_port);

    dns_server_config_t dns_cfg = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
    s_dns = start_dns_server(&dns_cfg);
    if (!s_dns) {
        ESP_LOGE(TAG, "start_dns_server failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "dns_server started");

    return ESP_OK;
}

// true, если заголовок Authorization запроса совпадает с s_expected_auth (посчитан
// один раз в web_start_normal). Пустой s_expected_auth (web_start_normal не звался
// или "web_pass" не читается) означает, что ни один заголовок не совпадёт — это ок,
// хендлеры нормального режима регистрируются только внутри web_start_normal.
static bool is_authorized(httpd_req_t *req) {
    char hdr[AUTH_HEADER_MAX_LEN];
    if (httpd_req_get_hdr_value_str(req, "Authorization", hdr, sizeof(hdr)) != ESP_OK) {
        return false;
    }
    return strcmp(hdr, s_expected_auth) == 0;
}

static esp_err_t send_401(httpd_req_t *req) {
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"knobos\"");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"error\":\"auth\"}");
    return ESP_OK;
}

static esp_err_t root_normal_get_handler(httpd_req_t *req) {
    if (!is_authorized(req)) return send_401(req);
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/api/status");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t api_status_get_handler(httpd_req_t *req) {
    if (!is_authorized(req)) return send_401(req);

    const esp_app_desc_t *desc = esp_app_get_description();
    int64_t uptime_s = esp_timer_get_time() / 1000000;

    wifi_ap_record_t ap_info;
    char ssid[33] = "";
    int rssi = 0;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        strncpy(ssid, (const char *)ap_info.ssid, sizeof(ssid) - 1);
        rssi = ap_info.rssi;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", desc->version);
    cJSON_AddNumberToObject(root, "uptime_s", (double)uptime_s);
    cJSON_AddNumberToObject(root, "heap_free", esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "heap_min", esp_get_minimum_free_heap_size());

    cJSON *wifi = cJSON_AddObjectToObject(root, "wifi");
    cJSON_AddStringToObject(wifi, "ssid", ssid);
    cJSON_AddStringToObject(wifi, "ip", wifi_mgr_ip());
    cJSON_AddNumberToObject(wifi, "rssi", rssi);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_str) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);
    return ESP_OK;
}

esp_err_t web_start_normal(void) {
    if (s_httpd) return ESP_OK; // идемпотентно; портал и normal делят один httpd_handle_t

    char web_pass[WEB_PASS_MAX_LEN] = "";
    settings_get_str("web_pass", web_pass, sizeof(web_pass));

    char cred[AUTH_CRED_MAX_LEN];
    snprintf(cred, sizeof(cred), "admin:%s", web_pass);

    unsigned char b64[AUTH_B64_MAX_LEN];
    size_t b64_len = 0;
    if (mbedtls_base64_encode(b64, sizeof(b64), &b64_len, (const unsigned char *)cred, strlen(cred)) != 0) {
        ESP_LOGE(TAG, "mbedtls_base64_encode failed (buffer too small)");
        return ESP_FAIL;
    }
    b64[b64_len] = '\0';
    snprintf(s_expected_auth, sizeof(s_expected_auth), "Basic %s", b64);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;

    esp_err_t err = httpd_start(&s_httpd, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }

    const httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = root_normal_get_handler };
    const httpd_uri_t status_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = api_status_get_handler };
    httpd_register_uri_handler(s_httpd, &root_uri);
    httpd_register_uri_handler(s_httpd, &status_uri);
    ESP_LOGI(TAG, "httpd (normal) started on port %d", config.server_port);

    return ESP_OK;
}
