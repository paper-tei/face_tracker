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

        .xclk_freq_hz = 20000000, // XCLK 频率，20MHz
        .pixel_format = PIXFORMAT_JPEG, // 使用 JPEG 格式
        .frame_size = FRAMESIZE_QVGA,   // 分辨率（QVGA 320x240）
        .jpeg_quality = 12,            // JPEG 压缩质量
        .fb_count = 2                  // 帧缓冲区数
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