#include "wifi_connect.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include <string.h>

#define TAG "WiFi_CONNECT"

// Wi-Fi STA 连接超时时间
#define WIFI_STA_CONNECT_TIMEOUT_MS 5000 // 5 秒

static EventGroupHandle_t wifi_event_group;
static const int CONNECTED_BIT = BIT0;

// 清理指定的网络接口
void cleanup_netif(const char* if_key) {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey(if_key);
    if (netif != NULL) {
        ESP_LOGW(TAG, "Destroying existing netif: %s", if_key);
        esp_netif_destroy(netif);
    }
}

// Wi-Fi 事件处理函数
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected from Wi-Fi, retrying...");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Connected to Wi-Fi.");
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    }
}

#ifdef STATIC_WIFI_SSID
// 静态 Wi-Fi 模式初始化
static bool wifi_init_static_mode() {
    wifi_config_t wifi_config = { 0 };

    strncpy((char*)wifi_config.sta.ssid, STATIC_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, STATIC_WIFI_PASS, sizeof(wifi_config.sta.password) - 1);

    ESP_LOGI(TAG, "Connecting using static Wi-Fi configuration: SSID=%s", STATIC_WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 等待连接成功
    if (xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(WIFI_STA_CONNECT_TIMEOUT_MS))) {
        ESP_LOGI(TAG, "Connected using static Wi-Fi configuration.");
        return true;
    }
    else {
        ESP_LOGW(TAG, "Static Wi-Fi connection failed.");
        return false;
    }
}
#endif


// 从 NVS 读取 Wi-Fi 配置的实现
bool read_wifi_config_from_nvs(wifi_config_t* wifi_config) {
    nvs_handle_t nvs;
    size_t len;

    if (nvs_open("wifi_config", NVS_READONLY, &nvs) == ESP_OK) {
        esp_err_t err;

        err = nvs_get_str(nvs, "ssid", NULL, &len);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to get SSID length");
            nvs_close(nvs);
            return false;
        }
        nvs_get_str(nvs, "ssid", (char*)wifi_config->sta.ssid, &len);

        err = nvs_get_str(nvs, "password", NULL, &len);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to get password length");
            nvs_close(nvs);
            return false;
        }
        nvs_get_str(nvs, "password", (char*)wifi_config->sta.password, &len);

        nvs_close(nvs);
        ESP_LOGI(TAG, "Read Wi-Fi config from NVS: SSID=%s", wifi_config->sta.ssid);
        return true;
    }

    ESP_LOGW(TAG, "No Wi-Fi config found in NVS");
    return false;
}

// AP 模式初始化
static void wifi_init_ap_mode() {
    cleanup_netif("WIFI_AP_DEF");

    esp_netif_t* ap_netif = esp_netif_create_default_wifi_ap();
    if (!ap_netif) {
        ESP_LOGE(TAG, "Failed to create default AP netif");
        return;
    }

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "paper_face_tracker",
            .password = "12345678",
            .ssid_len = strlen("paper_face_tracker"),
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };

    if (strlen((char*)ap_config.ap.password) == 0) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGW(TAG, "Wi-Fi AP started. SSID:%s", ap_config.ap.ssid);
}


bool wifi_init_sta_mode() {
    wifi_config_t wifi_config = { 0 };

    if (read_wifi_config_from_nvs(&wifi_config)) {
        ESP_LOGI(TAG, "Connecting using NVS Wi-Fi configuration: SSID=%s", wifi_config.sta.ssid);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        if (xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(WIFI_STA_CONNECT_TIMEOUT_MS))) {
            ESP_LOGI(TAG, "Wi-Fi connected using NVS configuration.");
            return true;
        }
        ESP_LOGW(TAG, "Failed to connect using NVS configuration.");
    }
    else {
        ESP_LOGW(TAG, "No Wi-Fi configuration found in NVS.");
    }

    return false;
}

// Wi-Fi 初始化
bool wifi_init() {
    wifi_event_group = xEventGroupCreate();

    cleanup_netif("WIFI_STA_DEF");

    esp_netif_t* sta_netif = esp_netif_create_default_wifi_sta();
    if (!sta_netif) {
        ESP_LOGE(TAG, "Failed to create default STA netif");
        return false;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_any_id);

#ifdef STATIC_WIFI_SSID
    // 如果定义了静态 Wi-Fi，优先尝试静态配置
    if (wifi_init_static_mode()) {
        ESP_LOGI(TAG, "Static Wi-Fi connection successful.");
        return true;
    }
#else
    // 如果未定义静态 Wi-Fi，先尝试从 NVS 加载配置
    if (wifi_init_sta_mode()) { // 这里假设 wifi_init_sta_mode() 尝试从 NVS 加载配置
        ESP_LOGI(TAG, "Wi-Fi connection using NVS configuration successful.");
        return true;
    }
    ESP_LOGW(TAG, "NVS configuration failed. Switching to AP mode...");
#endif

    // 如果静态配置和 NVS 都失败，进入 AP 模式
    wifi_init_ap_mode();
    return false;
}

void save_wifi_config_to_nvs(const char* ssid, const char* password) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("wifi_config", NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        nvs_set_str(nvs, "ssid", ssid);
        nvs_set_str(nvs, "password", password);
        nvs_commit(nvs);
        nvs_close(nvs);
        ESP_LOGI(TAG, "Wi-Fi configuration saved to NVS.");
    }
    else {
        ESP_LOGE(TAG, "Failed to open NVS for saving Wi-Fi configuration.");
    }
}