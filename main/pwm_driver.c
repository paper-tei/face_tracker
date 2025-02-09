#include "driver/ledc.h"
#include "pwm_driver.h"
#include "esp_log.h"

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_CHANNEL LEDC_CHANNEL_0

static int gpio_num;
static int pwm_freq;
static int pwm_resolution;
static int duty_cycle;

void pwm_init(int gpio, int freq, int resolution) {
    gpio_num = gpio;
    pwm_freq = freq;
    pwm_resolution = resolution;

    // 配置定时器
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = pwm_resolution,
        .freq_hz = pwm_freq,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    // 配置通道
    ledc_channel_config_t ledc_channel = {
        .gpio_num = gpio_num,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&ledc_channel);
}

void pwm_set_duty(int duty) {
    duty_cycle = duty;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, duty_cycle);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
}

void pwm_start() {
    ESP_LOGW("PWM", "PWM started on GPIO %d with duty %d", gpio_num, duty_cycle);
}
