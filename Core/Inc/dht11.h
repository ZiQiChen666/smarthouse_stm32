#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f1xx_hal.h"

/*
 * DHT11 温湿度传感器驱动（单总线，接 PB1）
 *
 *   - DHT11_Init()   : 配置 PB1（开漏输出 + 上拉），并做一次上电等待
 *   - DHT11_Read()   : 读一次数据
 *        返回 0 成功，非 0 失败（超时/校验错）
 *        成功时 *temperature / *humidity 为整数部分（DHT11 小数位恒为 0）
 *
 * 注意：DHT11 采样周期 >= 1s（官方建议 2s），不要在主循环里高频调用。
 */

#define DHT11_GPIO_PORT     GPIOB
#define DHT11_GPIO_PIN      GPIO_PIN_1

uint8_t DHT11_Init(void);
uint8_t DHT11_Read(float *temperature, float *humidity);

/* 调试用：取最近一次读到的 5 个原始字节 [H_int, H_dec, T_int, T_dec, Sum] */
void DHT11_GetRaw(uint8_t out[5]);

#endif /* __DHT11_H */
