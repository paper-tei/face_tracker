#pragma once

#include "esp_camera.h"

// OV2640 摄像头引脚配置（根据你的硬件修改）
#define CAM_PIN_PWDN    0   // 电源引脚，连接到 CAM_PWDM
#define CAM_PIN_RESET   -1  // 复位引脚（未使用时设置为 -1）
#define CAM_PIN_XCLK    8  // XCLK 引脚，连接到 GPIO15
#define CAM_PIN_SIOD    4   // SCCB 数据引脚，连接到 GPIO4
#define CAM_PIN_SIOC    5   // SCCB 时钟引脚，连接到 GPIO5

#define CAM_PIN_D7      9  // 数据引脚 Y9，连接到 GPIO16
#define CAM_PIN_D6      10  // 数据引脚 Y8，连接到 GPIO17
#define CAM_PIN_D5      11  // 数据引脚 Y7，连接到 GPIO18
#define CAM_PIN_D4      20  // 数据引脚 Y6，连接到 GPIO12
#define CAM_PIN_D3      18  // 数据引脚 Y5，连接到 GPIO10
#define CAM_PIN_D2      19  // 数据引脚 Y2，连接到 GPIO11
#define CAM_PIN_D1      17   // 数据引脚 Y3，连接到 GPIO9
#define CAM_PIN_D0      8   // 数据引脚 Y4，连接到 GPIO8

#define CAM_PIN_VSYNC   6   // 垂直同步信号，连接到 GPIO6
#define CAM_PIN_HREF    7   // 水平参考信号，连接到 GPIO7
#define CAM_PIN_PCLK    15  // 像素时钟，连接到 GPIO13


camera_config_t get_camera_config(void);
esp_err_t camera_init();