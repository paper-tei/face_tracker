#define WIDTH 240
#define HEIGHT 240
#include "esp_camera.h"
#include "usb_stream.h" // 假设使用 USB 视频流库
#include "esp_timer.h"
// 创建 RGB565 格式的黑色图像
uint8_t black_image[WIDTH * HEIGHT * 2] = { 0 }; // 每个像素 2 字节

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
