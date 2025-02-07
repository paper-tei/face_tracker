

#include "config.h"
#define TAG "MAIN"
#include <stdio.h>
#include <inttypes.h>


#include "esp_system.h"
#include "esp_flash.h"
#include "esp_wifi.h"  // 包含 Wi-Fi 相关的头文件
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
#include "esp_sleep.h"
#include <math.h>
#include "mdns.h"

#define MIN_RSSI -100  // 最弱信号（dBm）
#define MAX_RSSI -30      // 最强信号（dBm）
extern long long pow_off;
int rssi_to_percentage(int rssi) {
    if (rssi <= -70) {
        return 0;  // 最弱信号 (-70 dBm)
    }
    else if (rssi >= -30) {
        return 100;  // 最强信号 (-30 dBm)
    }
    else {
        // 使用线性公式进行转换
        return (int)((rssi + 70) * 100 / 40);  // 线性映射
    }
}
void print_ip_task(void* pvParameters) {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        printf("Failed to get netif handle for station mode.\n");
        vTaskDelete(NULL);
        return;
    }

    bool wifi_connected = false;  // 用于标记 Wi-Fi 是否已连接
    char ip_string[16];           // 存储 IP 地址字符串
    static bool mdns_initialized = false;  // 防止重复初始化 mDNS

    while (true) {
        esp_netif_ip_info_t ip_info;
        pow_off++;

        // 关机倒计时
        printf("%llds后关机\n", 600 - pow_off * 3);
        if (pow_off > 200) {
            ESP_LOGI("SHUTDOWN", "Entering deep sleep...");
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_deep_sleep_start();
        }

        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
            // Wi-Fi 已连接且获取到有效 IP
            if (!wifi_connected) {
                printf("Wi-Fi 已连接\n");
                wifi_connected = true;  // 更新标记

                // 初始化 mDNS（仅初始化一次）
                if (!mdns_initialized) {
                    ESP_LOGI(TAG, "Initializing mDNS...");

                    // 初始化 mDNS 服务
                    esp_err_t mdns_ret = mdns_init();
                    if (mdns_ret != ESP_OK) {
                        ESP_LOGE(TAG, "mDNS initialization failed: %s", esp_err_to_name(mdns_ret));
                        vTaskDelete(NULL);
                        return;
                    }
                    esp_err_t err = mdns_hostname_set("paper");
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to set mDNS hostname: %s", esp_err_to_name(err));
                        vTaskDelete(NULL);
                        return;
                    }
                    ESP_LOGI(TAG, "mDNS hostname set to 'paper.local'");

                    // 设置服务（如 HTTP 服务）
                    esp_err_t service_ret = mdns_service_add("ESP32 Stream Server", "_http", "_tcp", 80, NULL, 0);
                    if (service_ret != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to add mDNS service: %s", esp_err_to_name(service_ret));
                        vTaskDelete(NULL);
                        return;
                    }
                    ESP_LOGI(TAG, "mDNS service '_http' on port 80 registered successfully");
                    mdns_initialized = true;  // 标记 mDNS 已初始化
                }
            }

            // 打印 IP 地址
            snprintf(ip_string, sizeof(ip_string), IPSTR, IP2STR(&ip_info.ip));
            printf("IP Address:  http://%s\n", ip_string);
            printf("IP Address:  http://paper.local\n");

            // 获取 Wi-Fi 信号强度
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                int rssi_percentage = rssi_to_percentage(ap_info.rssi);  // 转换为百分比
                printf("Wi-Fi 信号强度: %d dBm (%d%%)\n", ap_info.rssi, rssi_percentage);
            }
            else {
                printf("无法获取信号强度\n");
            }
        }
        else {
#ifndef STATIC_WIFI_SSID
            // Wi-Fi 未连接或 IP 无效
            if (!wifi_connected) {
                ESP_LOGE(TAG, "请打开手机连接\"paper_face_tracker\" Wi-Fi进行配网！\n");
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

        vTaskDelay(pdMS_TO_TICKS(3000));  // 延迟 3 秒
    }
}




void app_main(void) {

    esp_log_level_set("*", ESP_LOG_WARN); // 仅打印警告及以上日志
    //esp_log_level_set(TAG, ESP_LOG_INFO); // 打印 info 及以上级别的日志


    // 可选：如果需要摄像头功能，可以在此处初始化摄像头
    // 配置传感器
    ESP_LOGI(TAG, "Initializing Camera...");
    if (camera_init() != ESP_OK) {
        ESP_LOGE(TAG, "Camera initialization failed!");
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // 延迟 1 秒
    setupCameraSensor();
    //capture_image_and_print();

    usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
        .rx_buffer_size = 4096 * 10, // RX 缓冲区大小
        .tx_buffer_size = 4096 * 10  // TX 缓冲区大小
    };

    // 安装 USB-Serial-JTAG 驱动
    esp_err_t ret = usb_serial_jtag_driver_install(&usb_serial_jtag_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB-Serial-JTAG driver: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "USB-Serial-JTAG driver installed successfully");
    start_usb_read_task();
#ifdef USB
    while (1) {
        send_camera_frame();
    }


#endif // USB
#ifdef WIFI
    // 初始化 NVS（非易失性存储，用于保存 Wi-Fi 配置等）
    esp_err_t ret1 = nvs_flash_init();
    if (ret1 == ESP_ERR_NVS_NO_FREE_PAGES || ret1 == ESP_ERR_NVS_NEW_VERSION_FOUND) {
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
    xTaskCreate(print_ip_task, "Print_IP_Task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "System initialization complete. Ready for operation.");

#endif // WIFI
}
