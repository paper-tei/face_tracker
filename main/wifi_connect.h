#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H
#include <stdbool.h>
// 定义静态 Wi-Fi 配置

bool wifi_init();
void save_wifi_config_to_nvs(const char* ssid, const char* password);
#endif // WIFI_CONNECT_H
