#define TAG "MAIN"
#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "camera_config.h"
#include "wifi_connect.h"
#include "wifi_ap.h"
#include "http_server.h"


void app_main(void) {
    // 初始化 NVS（非易失性存储，用于保存 Wi-Fi 配置等）
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs to be erased.");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // 初始化网络栈和事件循环
    ESP_LOGI(TAG, "Initializing network stack and event loop...");
    ESP_ERROR_CHECK(esp_netif_init()); // 初始化网络接口
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // 创建事件循环

    // 初始化 Wi-Fi 子系统
    ESP_LOGI(TAG, "Initializing Wi-Fi...");
    bool is_connected = wifi_init(); // 调用 Wi-Fi 初始化函数，返回连接状态

    if (!is_connected) {
        // 如果未连接 Wi-Fi，则启动 HTTP 服务器用于配置
        ESP_LOGW(TAG, "Wi-Fi not connected. Starting HTTP server for configuration...");
        start_webserver(); // 启动 HTTP 配置服务器
    }
    else {
        // 如果 Wi-Fi 已连接，跳过 HTTP 配置服务器
        ESP_LOGI(TAG, "Wi-Fi connected successfully. Skipping HTTP server.");
    }

    // 可选：如果需要摄像头功能，可以在此处初始化摄像头
    /*
    ESP_LOGI(TAG, "Initializing Camera...");
    if (camera_init() != ESP_OK) {
        ESP_LOGE(TAG, "Camera initialization failed!");
        return;
    }
    */

    ESP_LOGI(TAG, "System initialization complete. Ready for operation.");
}
