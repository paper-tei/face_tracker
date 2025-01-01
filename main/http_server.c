#include "http_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "wifi_connect.h"
#define TAG "HTTP_SERVER"

// HTTP根页面处理函数
esp_err_t root_handler(httpd_req_t* req) {
    const char* html_form =
        "<!DOCTYPE html>"
        "<html>"
        "<body>"
        "<h1>Wi-Fi Configuration</h1>"
        "<form action=\"/config\" method=\"post\">"
        "SSID: <input type=\"text\" name=\"SSID\"><br>"
        "Password: <input type=\"password\" name=\"PASS\"><br>"
        "<input type=\"submit\" value=\"Submit\">"
        "</form>"
        "</body>"
        "</html>";
    httpd_resp_send(req, html_form, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


esp_err_t http_post_handler(httpd_req_t* req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char ssid[32] = { 0 }, password[64] = { 0 };
    if (sscanf(buf, "SSID=%31[^&]&PASS=%63s", ssid, password) == 2) {
        ESP_LOGW(TAG, "Received SSID: %s", ssid);
        ESP_LOGW(TAG, "Received Password: %s", password);

        // 保存到 NVS
        save_wifi_config_to_nvs(ssid, password);

        // 重启以应用新的配置
        ESP_LOGI(TAG, "Rebooting in 3 seconds...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    }
    else {
        ESP_LOGW(TAG, "Invalid data format. Expected format: SSID=<SSID>&PASS=<Password>");
    }

    httpd_resp_send(req, "Wi-Fi Configured. Rebooting...", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// HTTP配置处理函数
esp_err_t config_handler(httpd_req_t* req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // 假设接收的数据格式为：SSID=xxx&PASS=yyy
    char ssid[32] = { 0 }, pass[64] = { 0 };
    sscanf(buf, "SSID=%31[^&]&PASS=%63s", ssid, pass);

    // 打印接收到的SSID和密码
    ESP_LOGW(TAG, "Received SSID: %s", ssid);
    ESP_LOGW(TAG, "Received Password: %s", pass);

    // 保存到NVS并切换到STA模式
    nvs_handle_t nvs;
    nvs_open("wifi_config", NVS_READWRITE, &nvs);
    nvs_set_str(nvs, "ssid", ssid);
    nvs_set_str(nvs, "password", pass);
    nvs_commit(nvs);
    nvs_close(nvs);

    httpd_resp_send(req, "WiFi Configured. Rebooting...", HTTPD_RESP_USE_STRLEN);
    for (int i = 3; i > 0; i--) {
        ESP_LOGI(TAG, "Rebooting in %d seconds...", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    esp_restart();
    return ESP_OK;
}
// favicon.ico 处理函数
esp_err_t favicon_handler(httpd_req_t* req) {
    // 返回一个空的响应
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// 启动HTTP服务器
void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &root_uri);

    httpd_uri_t config_uri = {
        .uri = "/config",
        .method = HTTP_POST,
        .handler = config_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t favicon_uri = {
    .uri = "/favicon.ico",
    .method = HTTP_GET,
    .handler = favicon_handler,
    .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &favicon_uri);
    httpd_register_uri_handler(server, &config_uri);

    ESP_LOGI(TAG, "HTTP server started");
}
