

#include "config.h"
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
#include "driver/usb_serial_jtag.h"
// 局域网 IP 打印任务

void print_ip_task(void* pvParameters) {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        printf("Failed to get netif handle for station mode.\n");
        vTaskDelete(NULL);
        return;
    }

    bool wifi_connected = false;  // 用于标记 Wi-Fi 是否已连接
    char ip_string[16];           // 存储 IP 地址字符串

    while (true) {
        esp_netif_ip_info_t ip_info;

        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
            // Wi-Fi 已连接且获取到有效 IP
            if (!wifi_connected) {
                printf("Wi-Fi 已连接\n");
                wifi_connected = true;  // 更新标记
            }
            snprintf(ip_string, sizeof(ip_string), IPSTR, IP2STR(&ip_info.ip));
            printf("IP Address: %s\n", ip_string);
        }
        else {
#ifndef STATIC_WIFI_SSID
            // Wi-Fi 未连接或 IP 无效
            if (!wifi_connected) {
                printf("请打开手机连接\"paper_face_tracker\" Wi-Fi进行配网！\n");
                printf("Wi-Fi 密码: 12345678\n");
            }
            wifi_connected = false;  // 更新标记
#endif
#ifdef STATIC_WIFI_SSID
            if (!wifi_connected) {
                printf("固件WIFI配置错误,请重新编译固件或者烧录AP配网版本固件\n");

            }
            wifi_connected = false;  // 更新标记
#endif
        }
        UBaseType_t remaining_stack = uxTaskGetStackHighWaterMark(NULL);
        printf("Remaining stack: %d bytes\n", remaining_stack);

        vTaskDelay(pdMS_TO_TICKS(2000)); // 延迟 2 秒
    }
}



void app_main(void) {
    //esp_log_level_set("*", ESP_LOG_WARN); // 仅打印警告及以上日志
    esp_log_level_set(TAG, ESP_LOG_INFO); // 打印 info 及以上级别的日志
    //vTaskDelay(pdMS_TO_TICKS(1000)); // 延迟 1 秒

    // 可选：如果需要摄像头功能，可以在此处初始化摄像头
    // 配置传感器
    ESP_LOGI(TAG, "Initializing Camera...");
    if (camera_init() != ESP_OK) {
        ESP_LOGE(TAG, "Camera initialization failed!");
        return;
    }
    setupCameraSensor();
    //capture_image_and_print();
#ifdef USB
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
        .rx_buffer_size = 4096, // RX 缓冲区大小
        .tx_buffer_size = 4096 * 4  // TX 缓冲区大小
    };

    // 安装 USB-Serial-JTAG 驱动
    esp_err_t ret = usb_serial_jtag_driver_install(&usb_serial_jtag_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB-Serial-JTAG driver: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "USB-Serial-JTAG driver installed successfully");
    while (1) {

        send_camera_frame();
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

    // 启动打印 IP 地址的任务
    //xTaskCreate(print_ip_task, "Print_IP_Task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "System initialization complete. Ready for operation.");

#endif // WIFI
}
