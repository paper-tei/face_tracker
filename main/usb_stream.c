#include "driver/usb_serial_jtag.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "esp_vfs_dev.h"
#include <fcntl.h>
#include "esp_camera.h"
#include "esp_log.h"
#include <string.h>
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