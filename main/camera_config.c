#include "camera_config.h"
#include "esp_camera.h"
#include "esp_log.h"
#include <stdio.h>
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
        .xclk_freq_hz = 24000000,       // 将 XCLK 频率设置为 10MHz

        .fb_location = CAMERA_FB_IN_DRAM,  // 使用 PSRAM 存储帧数据
        .pixel_format = PIXFORMAT_JPEG,// 使用 JPEG 格式
        .frame_size = FRAMESIZE_240X240,// 分辨率 240x240
        .jpeg_quality = 5,            // JPEG 压缩质量
        .fb_count = 5,                  // 帧缓冲区数
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY

    };
    esp_rom_delay_us(100); // 延迟 1ms
    return config;
}

// 初始化摄像头
esp_err_t camera_init() {
    esp_rom_delay_us(100); // 延迟 1ms
    camera_config_t config = get_camera_config();
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
    camera_sensor->set_reg(
        camera_sensor, 0xff, 0x01,
        0x00);  // banksel, here we're directly writing to the registers.
    // 0xFF==0x00 is the first bank, there's also 0xFF==0x01
    camera_sensor->set_reg(camera_sensor, 0xd3, 0xff, 5);  // clock
    // 设置亮度、对比度、饱和度等
    camera_sensor->set_brightness(camera_sensor, 2);  // 亮度：-2 到 2
    camera_sensor->set_contrast(camera_sensor, 2);    // 对比度：-2 到 2
    camera_sensor->set_saturation(camera_sensor, -2); // 饱和度：-2 到 2
    // 设置白平衡
    camera_sensor->set_whitebal(camera_sensor, 1); // 启用自动白平衡
    camera_sensor->set_awb_gain(camera_sensor, 0); // 禁用自动白平衡增益
    camera_sensor->set_wb_mode(camera_sensor, 0);  // 自动白平衡模式

    // 设置曝光控制
    camera_sensor->set_exposure_ctrl(camera_sensor,0); // 禁用自动曝光控制
    camera_sensor->set_aec2(camera_sensor, 0);         // 禁用高级曝光控制
    camera_sensor->set_ae_level(camera_sensor, 0);     // 曝光级别：-2 到 2
    camera_sensor->set_aec_value(camera_sensor, 500);  // 曝光值：0 到 1200

    // 设置增益控制
    camera_sensor->set_gain_ctrl(camera_sensor, 0);        // 禁用自动增益控制
    camera_sensor->set_agc_gain(camera_sensor, 3);         // 设置自动增益值：0 到 30
    camera_sensor->set_gainceiling(camera_sensor, (gainceiling_t)1); // 增益上限：0 到 6

    // 设置像素校正
    camera_sensor->set_bpc(camera_sensor, 1); // 启用黑点校正
    camera_sensor->set_wpc(camera_sensor, 1); // 启用白点校正
    // // digital clamp white balance
    camera_sensor->set_dcw(camera_sensor, 1);  // 0 = disable , 1 = enable
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

