#define WIDTH 240
#define HEIGHT 240
#include "esp_camera.h"
#include "esp_timer.h"
#include "driver/uart.h"
#define UART_NUM UART_NUM_0  // 使用 UART0
#define TXD_PIN 1            // TX 引脚
#define RXD_PIN 3            // RX 引脚
// 创建 RGB565 格式的黑色图像
uint8_t black_image[WIDTH * HEIGHT * 2] = { 0 }; // 每个像素 2 字节

void setup_uart() {
    const uart_config_t uart_config = {
        .baud_rate = 3000000,                // 波特率 3 Mbps
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    // 配置 UART 参数
    uart_param_config(UART_NUM, &uart_config);

    // 设置 UART 的 TX 和 RX 引脚
    uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // 安装 UART 驱动，设置发送缓冲区大小为 1024 字节
    uart_driver_install(UART_NUM, 1024, 0, 0, NULL, 0);
}
// 模拟黑色帧缓冲区
camera_fb_t simulate_black_frame() {
    static uint8_t black_image[WIDTH * HEIGHT * 2] = { 0 }; // 模拟的黑色图像缓冲区
    camera_fb_t fb;
    fb.buf = black_image;        // 图像数据指针
    fb.len = sizeof(black_image); // 图像数据长度
    fb.width = WIDTH;            // 图像宽度
    fb.height = HEIGHT;          // 图像高度
    fb.format = PIXFORMAT_RGB565; // 使用 RGB565 格式
    return fb;
}

void send_frame() {
    // 获取摄像头帧
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        printf("Camera capture failed\n");
        return;
    }

    // 帧头
    const uint8_t header[] = { 0xFF, 0xA0, 0xFF, 0xA1 }; // ETVR_HEADER + ETVR_HEADER_FRAME
    uart_write_bytes(UART_NUM, (const char*)header, sizeof(header));

    // 数据长度
    uint16_t len = fb->len;  // 图像数据长度
    uint8_t len_bytes[2];
    len_bytes[0] = len & 0xFF;            // 低字节
    len_bytes[1] = (len >> 8) & 0xFF;     // 高字节
    uart_write_bytes(UART_NUM, (const char*)len_bytes, 2);

    // 图像数据
    uart_write_bytes(UART_NUM, (const char*)fb->buf, fb->len);

    // 释放帧缓冲
    esp_camera_fb_return(fb);
}
