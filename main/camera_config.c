#include "camera_config.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "config.h"
#define TAG "CAMERA_CONFIG"
#include "freertos/FreeRTOS.h"
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
        #ifdef WIFI
        .xclk_freq_hz = 20000000,       // 将 XCLK 频率设置为 24MHz
        #endif
        #ifdef USB
        .xclk_freq_hz = 20000000,       // 将 XCLK 频率设置为 10MHz
        #endif
        #ifdef ESP32CAM
        .pixel_format = PIXFORMAT_JPEG, // 使用 JPEG 格式
        .fb_location = CAMERA_FB_IN_DRAM,
        .frame_size = FRAMESIZE_240X240,  // 降低分辨率到 240x240
        .jpeg_quality = 9,             // 调高 JPEG 压缩比（默认值是 12，越高占用内存越少）
        .fb_count = 2                  // 减少帧缓冲区数到 2
        #endif 

        #ifdef ESP32S3CAM
        .fb_location = CAMERA_FB_IN_PSRAM,  // 使用 PSRAM 存储帧数据
        .pixel_format = PIXFORMAT_JPEG,// 使用 JPEG 格式
        .frame_size = FRAMESIZE_240X240,// 分辨率 240x240
        .jpeg_quality = 5,            // JPEG 压缩质量
        .fb_count = 2,                  // 帧缓冲区数
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY // 始终抓取最新帧
        #endif
    };
    esp_rom_delay_us(100); // 延迟 1ms
    return config;
}

// 初始化摄像头
esp_err_t camera_init() {
    esp_rom_delay_us(100); // 延迟 1ms
    camera_config_t config = get_camera_config();
    esp_rom_delay_us(100); // 延迟 1ms
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
        return err;
    }
    ESP_LOGI(TAG, "Camera initialized successfully!");
    esp_rom_delay_us(1000); // 延迟 1ms
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
    // 设置白平衡
    camera_sensor->set_whitebal(camera_sensor, 1); // 启用自动白平衡
    camera_sensor->set_awb_gain(camera_sensor, 0); // 禁用自动白平衡增益
    camera_sensor->set_wb_mode(camera_sensor, 0);  // 自动白平衡模式

    // 设置曝光控制
    camera_sensor->set_exposure_ctrl(camera_sensor, 0); // 禁用自动曝光控制
    camera_sensor->set_aec2(camera_sensor, 0);         // 禁用高级曝光控制
    camera_sensor->set_ae_level(camera_sensor, 0);     // 曝光级别：-2 到 2
    camera_sensor->set_aec_value(camera_sensor, 300);  // 曝光值：0 到 1200

    // 设置增益控制
    camera_sensor->set_gain_ctrl(camera_sensor, 0);        // 禁用自动增益控制
    camera_sensor->set_agc_gain(camera_sensor, 2);         // 设置自动增益值：0 到 30
    camera_sensor->set_gainceiling(camera_sensor, (gainceiling_t)6); // 增益上限：0 到 6

    // 设置像素校正
    camera_sensor->set_bpc(camera_sensor, 1); // 启用黑点校正
    camera_sensor->set_wpc(camera_sensor, 1); // 启用白点校正

    // 设置伽马校正
    camera_sensor->set_raw_gma(camera_sensor, 1); // 启用伽马校正

    // 设置镜头校正
    camera_sensor->set_lenc(camera_sensor, 0); // 禁用镜头校正

    // 设置色彩条用于测试（此处禁用）
    camera_sensor->set_colorbar(camera_sensor, 0); // 禁用色彩条

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