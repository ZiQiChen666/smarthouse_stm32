/**
  ******************************************************************************
  * @file    bh1750.c
  * @brief   BH1750 光照强度传感器驱动（I2C1，与 OLED 共用 PB6/PB7）
  *
  *  接线：
  *    VCC  -> 3.3V
  *    GND  -> GND
  *    SCL  -> PB6（和 OLED 共用）
  *    SDA  -> PB7（和 OLED 共用）
  *    ADDR -> 接地（地址 7bit 0x23 / HAL 0x46）或 VCC（0x5C / 0xB8）
 *            （驱动会自动扫描这两个地址）
  *
  *  指令：
  *    0x01 上电
  *    0x00 断电
  *    0x10 连续 H 分辨率模式（1 lx，约 120ms 转换）
  *    0x11 连续 H 分辨率模式2（0.5 lx，约 120ms）
  *    0x13 连续 L 分辨率模式（4 lx，约 16ms）
  *    0x20 单次 H 分辨率模式（读完后自动断电）
  *
  *  读取：连续 H 模式下直接读 2 字节（大端），
  *        光照(lx) = raw / 1.2
  ******************************************************************************
  */

#include "bh1750.h"
#include "i2c.h"

extern I2C_HandleTypeDef hi2c1;

/* 本模块是否探测到器件；不在线时所有操作直接跳过，
   避免 HAL_I2C 每次超时把主循环拖慢 */
static uint8_t s_bh_ok = 0;
/* 实际使用的 8bit 地址（Init 时探测确定） */
static uint8_t s_bh_addr = BH1750_ADDR_LOW;

/* 单次 I2C 超时（ms） */
#define BH1750_I2C_TIMEOUT   100u

/* ------------------------------------------------------------------ */
static uint8_t bh1750_write_cmd(uint8_t cmd)
{
    if (!s_bh_ok) return 1;
    if (HAL_I2C_Master_Transmit(&hi2c1, s_bh_addr, &cmd, 1,
                                BH1750_I2C_TIMEOUT) != HAL_OK)
    {
        s_bh_ok = 0;      /* 通信失败，标记离线，后续跳过 */
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
uint8_t BH1750_Init(void)
{
    /* 扫描两个可能的地址：ADDR 悬空/接地 0x46 / ADDR 接 VCC 0xB8 */
    s_bh_ok = 0;
    if (HAL_I2C_IsDeviceReady(&hi2c1, BH1750_ADDR_LOW, 3, BH1750_I2C_TIMEOUT) == HAL_OK)
    {
        s_bh_addr = BH1750_ADDR_LOW;
        s_bh_ok = 1;
    }
    else if (HAL_I2C_IsDeviceReady(&hi2c1, BH1750_ADDR_HIGH, 3, BH1750_I2C_TIMEOUT) == HAL_OK)
    {
        s_bh_addr = BH1750_ADDR_HIGH;
        s_bh_ok = 1;
    }
    else
    {
        s_bh_ok = 0;
        return 1;
    }

    bh1750_write_cmd(0x01);      /* 上电 */
    HAL_Delay(10);
    bh1750_write_cmd(0x10);      /* 连续 H 分辨率模式（1 lx） */
    HAL_Delay(180);              /* 等第一次转换完成 */

    return 0;
}

/* 器件是否已在线 */
uint8_t BH1750_IsReady(void)
{
    return s_bh_ok;
}

/* 供主循环周期性重试（掉线后自动恢复） */
uint8_t BH1750_ReInit(void)
{
    return BH1750_Init();
}

/*
 * 返回值：0 成功
 *         1 I2C 通信失败 / 器件不在线
 */
uint8_t BH1750_Read(float *lux)
{
    uint8_t buf[2] = {0, 0};
    uint16_t raw;

    if (!s_bh_ok) return 1;

    /* 连续 H 分辨率模式：直接读 2 字节，大端 */
    if (HAL_I2C_Master_Receive(&hi2c1, s_bh_addr, buf, 2,
                               BH1750_I2C_TIMEOUT) != HAL_OK)
    {
        s_bh_ok = 0;
        return 1;
    }

    raw = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);

    /* 光照(lx) = raw / 1.2 */
    if (lux != NULL)
        *lux = (float)raw / 1.2f;

    return 0;
}
