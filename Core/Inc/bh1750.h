#ifndef __BH1750_H
#define __BH1750_H

#include "stm32f1xx_hal.h"

/*
 * BH1750 光照强度传感器驱动（I2C，与 OLED 共用 I2C1：PB6=SCL, PB7=SDA）
 *
 *   - BH1750_Init() : 探测器件、上电、设连续 H-分辨率模式（1 lx）
 *   - BH1750_Read() : 读一次光照
 *        返回 0 成功，非 0 失败
 *        成功时 *lux 为光照强度（单位 lx）
 *
 *  BH1750 的 7bit 地址由 ADDR 引脚决定：
 *      ADDR 接地/悬空 -> 7bit 0x23 -> HAL(8bit) 0x46
 *      ADDR 接 VCC    -> 7bit 0x5C -> HAL(8bit) 0xB8
 *  Init 会自动扫描这两个地址，接哪种都能用。
 *
 *  注意：连续 H 分辨率模式转换时间约 120ms，
 *        两次读取间隔建议 >= 180ms。
 */

/* HAL 的 I2C 接口使用 8bit 地址（7bit << 1） */
#define BH1750_ADDR_LOW     0x46    /* ADDR 接地：7bit 0x23 */
#define BH1750_ADDR_HIGH    0xB8    /* ADDR 接 VCC：7bit 0x5C */

uint8_t BH1750_Init(void);
uint8_t BH1750_Read(float *lux);

/* 器件是否在线；不在线时可用 BH1750_ReInit() 重试探测 */
uint8_t BH1750_IsReady(void);
uint8_t BH1750_ReInit(void);

#endif /* __BH1750_H */
