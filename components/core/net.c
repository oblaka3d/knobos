#include "net.h"
#include "esp_netif_sntp.h"
#include <time.h>

void net_start_sntp(const char *tz) {
    setenv("TZ", tz, 1); tzset();
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&cfg);
}
