#include "camera_config.h"
#include "esp_camera.h"
#include "esp_log.h"

#define TAG "CAMERA_CONFIG"

camera_config_t get_camera_config(void) {
    camera_config_t config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sscb_sda = CAM_PIN_SIOD,
        .pin_sscb_scl = CAM_PIN_SIOC,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,
        .xclk_freq_hz = 20000000,       // 将 XCLK 频率降低到 10MHz
        #ifdef ESP32CAM
        .pixel_format = PIXFORMAT_JPEG, // 使用 JPEG 格式（比 RGB565 更省内存）
        .fb_location = CAMERA_FB_IN_DRAM,
        .frame_size = FRAMESIZE_QQVGA,  // 降低分辨率到 QQVGA (160x120)
        .jpeg_quality = 9,             // 调高 JPEG 压缩比（默认值是 12，越高占用内存越少）
        .fb_count = 2                  // 减少帧缓冲区数到 1
        #endif 

        #ifdef ESP32S3CAM
        .xclk_freq_hz = 20000000, // XCLK 频率，20MHz
        .pixel_format = PIXFORMAT_JPEG, // 使用 JPEG 格式
        .frame_size = FRAMESIZE_QVGA,   // 分辨率（QVGA 320x240）
        .jpeg_quality = 12,            // JPEG 压缩质量
        .fb_count = 2                  // 帧缓冲区数
        #endif
    };
    return config;
}
esp_err_t camera_init() {
    camera_config_t config = get_camera_config();
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
        return err;
    }
    ESP_LOGI(TAG, "Camera initialized successfully!");
    return ESP_OK;
}
void capture_image_and_print() {
    // 获取帧缓冲区（帧数据）
    camera_fb_t* fb = esp_camera_fb_get();

    if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed!");
        return;
    }

    // 打印帧缓冲区的信息
    ESP_LOGI(TAG, "Captured image:");
    ESP_LOGI(TAG, "Width: %d", fb->width);
    ESP_LOGI(TAG, "Height: %d", fb->height);
    ESP_LOGI(TAG, "Format: %s", fb->format == PIXFORMAT_JPEG ? "JPEG" : "OTHER");
    ESP_LOGI(TAG, "Size: %d bytes", fb->len);

    // 如果需要打印帧数据内容（仅打印前 100 字节作为示例）
    ESP_LOGI(TAG, "Image data (first 100 bytes):");
    for (int i = 0; i < fb->len && i < 100; i++) {
        printf("%02X ", fb->buf[i]); // 以十六进制打印数据
        if ((i + 1) % 16 == 0) {
            printf("\n"); // 每 16 字节换行
        }
    }
    printf("\n");

    // 释放帧缓冲区
    esp_camera_fb_return(fb);
}