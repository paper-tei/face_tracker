#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H
#include <stdbool.h>
extern const char* STATIC_WIFI_SSID;
extern const char* STATIC_WIFI_PASS;
// 定义静态 Wi-Fi 配置
// #define STATIC_WIFI_SSID "paper"
// #define STATIC_WIFI_PASS "paperp425"
bool wifi_init();
void save_wifi_config_to_nvs(const char* ssid, const char* password);
#endif // WIFI_CONNECT_H
