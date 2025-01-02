

#define WIFI //USB WIFI
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
#include "usb_stream.h"
// 局域网 IP 打印任务
void print_ip_task(void* pvParameters) {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        ESP_LOGE(TAG, "Failed to get netif handle for station mode.");
        vTaskDelete(NULL);
        return;
    }
    while (true) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            ESP_LOGE(TAG, "IP Address: " IPSTR, IP2STR(&ip_info.ip));
        }
        else {
            ESP_LOGE(TAG, "Failed to get IP info. Ensure Wi-Fi is connected.");
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // 延迟 2 秒
    }
}



void app_main(void) {
    //esp_log_level_set("*", ESP_LOG_WARN); // 仅打印警告及以上日志
    esp_log_level_set(TAG, ESP_LOG_INFO); // 打印 info 及以上级别的日志
    // 可选：如果需要摄像头功能，可以在此处初始化摄像头
    // 配置传感器
    setupCameraSensor();
    ESP_LOGI(TAG, "Initializing Camera...");
    if (camera_init() != ESP_OK) {
        ESP_LOGE(TAG, "Camera initialization failed!");
        return;
    }

#ifdef USB
    setup_uart();
    while (1) {
        send_frame();   // 发送图像帧
        //ESP_LOGE(TAG, "Camera send");
        vTaskDelay(pdMS_TO_TICKS(100)); // 延迟 100ms (可根据帧率需求调整)
    }
#endif // USB
#ifdef WIFI
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0) {
        ESP_LOGE(TAG, "PSRAM is available. Free PSRAM: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    }
    else {
        ESP_LOGE(TAG, "PSRAM is not available on this module.\n");
    }

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


    // 启动推流服务器
    ESP_LOGI(TAG, "Starting stream server...");
    start_stream_server();
    //capture_image_and_print();
    // 启动打印 IP 地址的任务
    xTaskCreate(print_ip_task, "Print_IP_Task", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "System initialization complete. Ready for operation.");
#endif // WIFI
}
