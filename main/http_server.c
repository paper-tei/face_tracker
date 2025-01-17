#include "http_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "wifi_connect.h"
#include "camera_config.h"
#include "esp_camera.h"
#include "esp_timer.h"

#define PART_BOUNDARY "123456789000000000000987654321"
#define STREAM_CONTENT_TYPE "multipart/x-mixed-replace;boundary=" PART_BOUNDARY
#define STREAM_BOUNDARY "\r\n--" PART_BOUNDARY "\r\n"
#define STREAM_PART "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %lld.%06ld\r\n\r\n"



#define TAG "HTTP_SERVER"
// 替代 millis() 宏定义
#define millis() (esp_timer_get_time() / 1000)
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

esp_err_t config_handler(httpd_req_t* req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char ssid[32] = { 0 }, pass[64] = { 0 };
    sscanf(buf, "SSID=%31[^&]&PASS=%63s", ssid, pass);

    ESP_LOGI(TAG, "Received SSID: %s", ssid);
    ESP_LOGI(TAG, "Received Password: %s", pass);

    // 保存Wi-Fi配置
    nvs_handle_t nvs;
    nvs_open("wifi_config", NVS_READWRITE, &nvs);
    nvs_set_str(nvs, "ssid", ssid);
    nvs_set_str(nvs, "password", pass);
    nvs_commit(nvs);
    nvs_close(nvs);

    httpd_resp_send(req, "Wi-Fi Configured. Rebooting...", HTTPD_RESP_USE_STRLEN);
    for (int i = 3; i > 0; i--) {
        ESP_LOGI(TAG, "Rebooting in %d seconds...", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    esp_restart();
    return ESP_OK;
}
long long  pow_off = 0;
esp_err_t stream_handler(httpd_req_t* req) {
    long last_request_time = 0;
    camera_fb_t* fb = NULL;
    struct timeval _timestamp;

    esp_err_t res = ESP_OK;

    size_t _jpg_buf_len = 0;
    uint8_t* _jpg_buf = NULL;

    char part_buf[256];

    static int64_t last_frame = 0;
    if (!last_frame)
        last_frame = esp_timer_get_time();

    // 设置 HTTP 响应类型
    res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK)
        return res;

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "60");

    while (true) {
        pow_off = 0;
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE("STREAM", "Camera capture failed");
            res = ESP_FAIL;
        }
        else {
            // 获取时间戳和图像数据
            _timestamp.tv_sec = fb->timestamp.tv_sec;
            _timestamp.tv_usec = fb->timestamp.tv_usec;
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }

        // 发送流的边界部分
        if (res == ESP_OK)
            res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));

        // 发送 HTTP 头部
        if (res == ESP_OK) {
            size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART,
                _jpg_buf_len,
                (long long)_timestamp.tv_sec,
                (long)_timestamp.tv_usec);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }

        // 发送 JPEG 图像数据
        if (res == ESP_OK)
            res = httpd_resp_send_chunk(req, (const char*)_jpg_buf, _jpg_buf_len);

        // 释放缓冲区
        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        }

        if (res != ESP_OK)
            break;

        // 日志记录帧率和大小
        long request_end = esp_timer_get_time() / 1000;
        long latency = request_end - last_request_time;
        last_request_time = request_end;

        ESP_LOGI("STREAM", "Size: %uKB, Time: %ldms, FPS: %ld",
            _jpg_buf_len / 1024, latency, 1000 / latency);
    }

    last_frame = 0;
    return res;
}
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
    httpd_register_uri_handler(server, &config_uri);

    ESP_LOGI(TAG, "HTTP server started");
}

void start_stream_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t stream_httpd = NULL;

    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_uri_t stream_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = stream_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(stream_httpd, &stream_uri);
        ESP_LOGI(TAG, "Stream server started");
    }
    else {
        ESP_LOGE(TAG, "Failed to start stream server");
    }
}
