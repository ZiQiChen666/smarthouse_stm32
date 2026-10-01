#ifndef __HCSR04_H
#define __HCSR04_H

#include "stm32f1xx_hal.h"

/*
 * HC-SR04 超声波测距驱动
 *   TRIG -> PB8  (推挽输出)
 *   ECHO -> PB9  (输入)
 *
 *   - HCSR04_Init() : 配置 PB8 输出 / PB9 输入，并使能微秒计时
 *   - HCSR04_Read() : 触发一次测距
 *        返回 0 成功，非 0 失败
 *        成功时 *distance 为距离，单位 cm
 *
 *  原理：TRIG 给 10us 高电平触发，模块发 8 个 40kHz 脉冲，
 *        ECHO 变高，持续时间 = 声波往返时间。
 *        距离(cm) = 高电平时间(us) / 58
 *
 *  注意：两次测距间隔建议 >= 60ms，避免回波干扰。
 */

#define HCSR04_TRIG_PORT    GPIOB
#define HCSR04_TRIG_PIN     GPIO_PIN_8
#define HCSR04_ECHO_PORT    GPIOB
#define HCSR04_ECHO_PIN     GPIO_PIN_9

uint8_t HCSR04_Init(void);
uint8_t HCSR04_Read(float *distance_cm);

#endif /* __HCSR04_H */
