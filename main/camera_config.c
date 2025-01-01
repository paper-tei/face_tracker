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
        .xclk_freq_hz = 20000000,       // 将 XCLK 频率设置为 20MHz
        #ifdef ESP32CAM
        .pixel_format = PIXFORMAT_JPEG, // 使用 JPEG 格式
        .fb_location = CAMERA_FB_IN_DRAM,
        .frame_size = FRAMESIZE_240X240,  // 降低分辨率到 240x240
        .jpeg_quality = 9,             // 调高 JPEG 压缩比（默认值是 12，越高占用内存越少）
        .fb_count = 2                  // 减少帧缓冲区数到 2
        #endif 

        #ifdef ESP32S3CAM
        .xclk_freq_hz = 20000000,      // XCLK 频率为 20MHz
        .pixel_format = PIXFORMAT_JPEG,// 使用 JPEG 格式
        .frame_size = FRAMESIZE_240X240,// 分辨率 240x240
        .jpeg_quality = 12,            // JPEG 压缩质量
        .fb_count = 3                  // 帧缓冲区数
        #endif
    };
    return config;
}

// 初始化摄像头
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

// 配置摄像头传感器的设置
void setupCameraSensor() {
    ESP_LOGD(TAG, "Setting up camera sensor...");

    // 获取摄像头传感器对象
    sensor_t* camera_sensor = esp_camera_sensor_get();
    if (!camera_sensor) {
        ESP_LOGE(TAG, "Failed to get camera sensor!");
        return;
    }

    // 设置亮度、对比度、饱和度等
    camera_sensor->set_brightness(camera_sensor, 2);  // 亮度：-2 到 2
    camera_sensor->set_contrast(camera_sensor, 2);    // 对比度：-2 到 2
    camera_sensor->set_saturation(camera_sensor, -2); // 饱和度：-2 到 2

    // 设置图像特效为灰度模式
    camera_sensor->set_special_effect(camera_sensor, 2); // 特效模式：灰度图像

    ESP_LOGD(TAG, "Camera sensor setup complete!");
}

// 捕获图像并打印信息
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
void configure_camera_crop(int x_start, int y_start, int width, int height) {
    sensor_t* s = esp_camera_sensor_get();
    if (s == NULL) {
        ESP_LOGE("CAMERA", "Failed to get sensor");
        return;
    }

    // 设置裁剪起始位置和宽高
    s->set_reg(s, 0xFF, 0x01, 0x00);  // 选择寄存器组1
    s->set_reg(s, 0x11, 0x01, 0x00);  // 设置时钟分频器
    s->set_reg(s, 0x12, 0xFF, 0x40);  // 设置裁剪模式

    // 设置起始坐标
    s->set_reg(s, 0x17, 0xFF, x_start >> 3);  // 起始X坐标高位
    s->set_reg(s, 0x18, 0xFF, (x_start + width) >> 3);  // 结束X坐标高位
    s->set_reg(s, 0x19, 0xFF, y_start >> 2);  // 起始Y坐标高位
    s->set_reg(s, 0x1A, 0xFF, (y_start + height) >> 2);  // 结束Y坐标高位

    ESP_LOGI("CAMERA", "Camera crop configured: x=%d, y=%d, w=%d, h=%d", x_start, y_start, width, height);
}
void check_cropped_image_size() {
    camera_fb_t* fb = esp_camera_fb_get(); // 获取帧缓冲区
    if (!fb) {
        ESP_LOGE("CAMERA", "Failed to capture image!");
        return;
    }

    // 打印图像信息
    ESP_LOGW("CAMERA", "Captured image:");
    ESP_LOGW("CAMERA", "Width: %d", fb->width);
    ESP_LOGW("CAMERA", "Height: %d", fb->height);
    ESP_LOGW("CAMERA", "Size: %d bytes", fb->len);

    // 如果需要打印前100字节的内容作为验证
    ESP_LOGI("CAMERA", "Image data (first 100 bytes):");
    for (int i = 0; i < fb->len && i < 100; i++) {
        printf("%02X ", fb->buf[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");

    // 释放帧缓冲区
    esp_camera_fb_return(fb);
}