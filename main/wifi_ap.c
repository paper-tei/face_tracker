#include "wifi_ap.h"
#include "http_server.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include <string.h>

#define TAG "WiFi_AP"

#define WIFI_MAX_CONN 4

void wifi_init_ap_mode(void) {
    esp_netif_create_default_wifi_ap();

    wifi_config_t ap_config = {
    .ap = {
        .ssid = "paper_face_tracker",
        .ssid_len = strlen("paper_face_tracker"),
        .max_connection = 4,
        .password = "",
        .authmode = WIFI_AUTH_OPEN // 显式设置为开放网络
    }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGW(TAG, "Wi-Fi AP started. SSID:paper_face_tracker");

    start_webserver();
}
