#pragma once
#include "esp_camera.h"
#define ESP32CAM
// OV2640 摄像头引脚配置（根据你的硬件修改）

#ifdef ESP32CAM

#define CAM_PIN_PWDN    32   // 电源引脚，连接到 CAM_PWDM
#define CAM_PIN_RESET   -1  // 复位引脚（未使用时设置为 -1）
#define CAM_PIN_XCLK    0  // XCLK 引脚
#define CAM_PIN_SIOD    26   // SCCB 数据引脚
#define CAM_PIN_SIOC    27   // SCCB 时钟引脚

#define CAM_PIN_D7      35  // 数据引脚 Y9
#define CAM_PIN_D6      34  // 数据引脚 Y8
#define CAM_PIN_D5      39  // 数据引脚 Y7
#define CAM_PIN_D4      36  // 数据引脚 Y6
#define CAM_PIN_D3      21  // 数据引脚 Y5
#define CAM_PIN_D2      19   // 数据引脚 Y4
#define CAM_PIN_D1      18   // 数据引脚 Y3
#define CAM_PIN_D0      5   // 数据引脚 Y2

#define CAM_PIN_VSYNC   25   // 垂直同步信号，连接到 GPIO6
#define CAM_PIN_HREF    23   // 水平参考信号，连接到 GPIO7
#define CAM_PIN_PCLK    22  // 像素时钟，连接到 GPIO13

#endif 

#ifdef ESP32S3CAM
#define CAM_PIN_PWDN    -1   // 电源引脚，连接到 CAM_PWDM
#define CAM_PIN_RESET   -1  // 复位引脚（未使用时设置为 -1）
#define CAM_PIN_XCLK    15  // XCLK 引脚，连接到 GPIO15
#define CAM_PIN_SIOD    4   // SCCB 数据引脚，连接到 GPIO4
#define CAM_PIN_SIOC    5   // SCCB 时钟引脚，连接到 GPIO5

#define CAM_PIN_D7      16  // 数据引脚 Y9，连接到 GPIO16
#define CAM_PIN_D6      17  // 数据引脚 Y8，连接到 GPIO17
#define CAM_PIN_D5      18  // 数据引脚 Y7，连接到 GPIO18
#define CAM_PIN_D4      12  // 数据引脚 Y6，连接到 GPIO12
#define CAM_PIN_D3      10  // 数据引脚 Y5，连接到 GPIO10
#define CAM_PIN_D2      11  // 数据引脚 Y2，连接到 GPIO11
#define CAM_PIN_D1      9   // 数据引脚 Y3，连接到 GPIO9
#define CAM_PIN_D0      8   // 数据引脚 Y4，连接到 GPIO8

#define CAM_PIN_VSYNC   6   // 垂直同步信号，连接到 GPIO6
#define CAM_PIN_HREF    7   // 水平参考信号，连接到 GPIO7
#define CAM_PIN_PCLK    13  // 像素时钟，连接到 GPIO13

#endif // DEBUG
camera_config_t get_camera_config(void);
esp_err_t camera_init();
void capture_image_and_print();