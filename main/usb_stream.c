#include "driver/usb_serial_jtag.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "esp_vfs_dev.h"
#include <fcntl.h>
#include "esp_camera.h"
#include "esp_log.h"
#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#define ETVR_HEADER       "\xFF\xA0"
#define ETVR_HEADER_FRAME "\xFF\xA1"

#define USB_SEND_CHUNK_SIZE   64         // USB 单次发送的最大字节数

static const char* TAG = "USB_STREAM";

void send_frame_via_usb(const uint8_t* buf, size_t len) {
    uint8_t len_bytes[2];

    // 发送帧头
    usb_serial_jtag_write_bytes(ETVR_HEADER, 2, portMAX_DELAY);
    usb_serial_jtag_write_bytes(ETVR_HEADER_FRAME, 2, portMAX_DELAY);

    // 计算并发送帧长度（低位在前，高位在后）
    len_bytes[0] = len & 0xFF;           // 长度低 8 位
    len_bytes[1] = (len >> 8) & 0xFF;    // 长度高 8 位
    usb_serial_jtag_write_bytes(len_bytes, 2, portMAX_DELAY);

    // 分块发送帧数据
    size_t sent = 0;
    while (sent < len) {
        size_t to_send = (len - sent > USB_SEND_CHUNK_SIZE) ? USB_SEND_CHUNK_SIZE : (len - sent);
        usb_serial_jtag_write_bytes(buf + sent, to_send, portMAX_DELAY);
        sent += to_send;
    }
}
extern long long pow_off;
void send_camera_frame() {
    pow_off = 0;
    // 从摄像头获取帧数据
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Failed to get camera frame");
        return;
    }

    // 打印帧信息
    ESP_LOGI(TAG, "Sending frame of size %d bytes", fb->len);

    // 通过 USB 发送帧数据
    send_frame_via_usb(fb->buf, fb->len);

    // 释放帧缓冲区
    esp_camera_fb_return(fb);
}
// 提取 SSID 和密码的函数
void extract_wifi_data(const char* receivedData, char* ssid, char* password) {
    // 提取 SSID
    char* ssid_pos = strstr(receivedData, "SSID:");
    if (ssid_pos != NULL) {
        sscanf(ssid_pos, "SSID:%s", ssid);  // 提取 SSID
    }

    // 提取密码
    char* password_pos = strstr(receivedData, "PASSWORD:");
    if (password_pos != NULL) {
        sscanf(password_pos, "PASSWORD:%s", password);  // 提取密码
    }

    ESP_LOGI(TAG, "Extracted SSID: %s", ssid);
    ESP_LOGI(TAG, "Extracted PASSWORD: %s", password);
}
// 保存 SSID 和密码到 NVS
esp_err_t save_wifi_to_nvs(const char* ssid, const char* password) {
    esp_err_t err;
    nvs_handle_t my_handle;

    // 打开 NVS 存储（如果不存在会创建）
    err = nvs_open("wifi_config", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle! Error: %s", esp_err_to_name(err));
        return err;
    }

    // 保存 SSID
    err = nvs_set_str(my_handle, "ssid", ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write SSID to NVS! Error: %s", esp_err_to_name(err));
        nvs_close(my_handle);
        return err;
    }

    // 保存密码
    err = nvs_set_str(my_handle, "password", password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write PASSWORD to NVS! Error: %s", esp_err_to_name(err));
        nvs_close(my_handle);
        return err;
    }

    // 提交保存的数据
    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit data to NVS! Error: %s", esp_err_to_name(err));
    }

    // 关闭 NVS 存储
    nvs_close(my_handle);

    return err;
}
void handle_received_data(uint8_t* data, size_t len) {
    // 将接收到的数据转换为 C 风格字符串
    data[len] = '\0';  // 确保字符串以 null 结尾
    char receivedData[len + 1];
    memcpy(receivedData, data, len);

    // 提取 SSID 和密码
    char ssid[64] = { 0 };
    char password[64] = { 0 };
    extract_wifi_data(receivedData, ssid, password);

    // 保存 SSID 和密码到 NVS
    esp_err_t err = save_wifi_to_nvs(ssid, password);
    if (err == ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi credentials saved to NVS.");
    }
    else {
        ESP_LOGW(TAG, "Failed to save Wi-Fi credentials to NVS.");
    }
}
void read_data_from_usb() {
    uint8_t buf[2000];
    int bytesRead;

    while (1) {
        // 这里使用非阻塞方式
        bytesRead = usb_serial_jtag_read_bytes(buf, sizeof(buf), 0);  // 0 表示非阻塞

        if (bytesRead < 0) {
            ESP_LOGE(TAG, "USB read failed, error code: %d", bytesRead);
            vTaskDelay(pdMS_TO_TICKS(100));  // 如果读取失败，稍作等待
            continue;  // 跳过当前循环
        }
        else if (bytesRead > 0) {
            ESP_LOGW(TAG, "Received %d bytes from USB", bytesRead);
            // 确保接收到的字节数据以 null 结尾
            buf[bytesRead] = '\0';  // 让数据成为一个 C 风格字符串
            // 将接收到的字节数据打印为字符串
            // 打印接收到的字符串

            ESP_LOGW(TAG, "Received data: %s", buf);

            buf[bytesRead] = '\0';  // 确保字符串以 null 结尾
            char receivedData[bytesRead + 1];
            memcpy(receivedData, buf, bytesRead);

            // 提取 SSID 和密码
            char ssid[64] = { 0 };
            char password[64] = { 0 };
            extract_wifi_data(receivedData, ssid, password);
            ESP_LOGW(TAG, "ssid: %s", ssid);
            ESP_LOGW(TAG, "password: %s", password);
            // 保存 SSID 和密码到 NVS
            esp_err_t err = save_wifi_to_nvs(ssid, password);
            // if (err == ESP_OK) {
            //     ESP_LOGI(TAG, "Wi-Fi credentials saved to NVS.");
            // }
            // else {
            //     ESP_LOGE(TAG, "Failed to save Wi-Fi credentials to NVS.");
            // }
        }


        // 延迟 100ms，防止 CPU 占用过高
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void usb_read_task(void* arg) {
    read_data_from_usb();  // 持续读取数据
}

void start_usb_read_task() {
    xTaskCreate(usb_read_task, "usb_read_task", 4096 * 2, NULL, 5, NULL);
}