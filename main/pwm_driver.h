#ifndef PWM_DRIVER_H
#define PWM_DRIVER_H

// 设置 PWM 输出的 GPIO 引脚和频率等
void pwm_init(int gpio_num, int pwm_freq, int resolution);
void pwm_set_duty(int duty);
void pwm_start();
#define PWM_GPIO 35        // PWM 引脚
#define PWM_FREQ 2000      // PWM 频率
#define PWM_RESOLUTION LEDC_TIMER_13_BIT  // 13 位分辨率
#endif // PWM_DRIVER_H
