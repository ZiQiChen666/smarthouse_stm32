/**
  ******************************************************************************
  * @file    dht11.c
  * @brief   DHT11 温湿度传感器驱动（单总线，PB1）
  *
  *  时序（主机 -> DHT11）：
  *    1. 主机拉低总线 >= 18ms，然后释放
  *    2. DHT11 响应：拉低 80us，再拉高 80us
  *    3. DHT11 发送 40bit 数据，每位以 50us 低电平开始，
  *       高电平持续时间决定数据：26~28us = 0，70us = 1
  *    4. 数据格式：湿度整数 + 湿度小数 + 温度整数 + 温度小数 + 校验和
  *
  *  PB1 用开漏 + 内部上拉驱动：输出 0 可以拉低总线，输出 1 时靠外部/内部
  *  上拉释放总线，避免推挽输出和 DHT11 抢总线。
  ******************************************************************************
  */

#include "dht11.h"

/* PB1 在 ADC 里没有用到（ADC 用的是 PA0/PA1/PA4/PB0），可以安全复用 */

#define DHT11_PIN           DHT11_GPIO_PIN
#define DHT11_PORT          DHT11_GPIO_PORT

/* 每个 tick 大约 1us（72MHz 下循环开销），仅作超时保护用 */
#define DHT11_TIMEOUT       10000u

static void dht11_delay_us(uint32_t us)
{
    /* 简单忙等；用于 1~80us 级别的短延时 */
    uint32_t n = us * (SystemCoreClock / 1000000u) / 4u;
    while (n--) { __NOP(); }
}

static void dht11_set_output(void)
{
    GPIO_InitTypeDef s = {0};
    s.Pin = DHT11_PIN;
    s.Mode = GPIO_MODE_OUTPUT_OD;      /* 开漏 */
    s.Pull = GPIO_PULLUP;
    s.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_PORT, &s);
}

static void dht11_set_input(void)
{
    GPIO_InitTypeDef s = {0};
    s.Pin = DHT11_PIN;
    s.Mode = GPIO_MODE_INPUT;
    s.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_PORT, &s);
}

static uint8_t dht11_read_bit(void)
{
    uint32_t t = 0;

    /* 等待 50us 低电平结束 */
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET) {
        if (++t > DHT11_TIMEOUT) return 0xFF;
    }
    t = 0;
    /* 高电平持续 <50us -> 0，>=50us -> 1 */
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET) {
        if (++t > DHT11_TIMEOUT) return 0xFF;
    }
    return (t > 50u) ? 1u : 0u;
}

static uint8_t dht11_read_byte(uint8_t *out)
{
    uint8_t i, byte = 0;
    for (i = 0; i < 8; i++) {
        uint8_t b = dht11_read_bit();
        if (b == 0xFF) return 1;       /* timeout */
        byte = (uint8_t)((byte << 1) | b);
    }
    *out = byte;
    return 0;
}

uint8_t DHT11_Init(void)
{
    /* 空闲状态：输出高（开漏，实际由上拉保持高） */
    dht11_set_output();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    HAL_Delay(1000);                   /* DHT11 上电后需 >=1s 稳定 */
    return 0;
}

uint8_t DHT11_Read(float *temperature, float *humidity)
{
    uint8_t data[5] = {0, 0, 0, 0, 0};
    uint8_t i;
    uint32_t t;

    /* ---- 1. 主机起始信号：拉低 >=18ms ---- */
    dht11_set_output();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);

    /* ---- 2. 释放总线，切输入等待 DHT11 响应 ---- */
    dht11_set_input();
    dht11_delay_us(30);

    /* DHT11 拉低 80us */
    t = 0;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET) {
        if (++t > DHT11_TIMEOUT) return 1;   /* 无响应 */
    }
    t = 0;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET) {
        if (++t > DHT11_TIMEOUT) return 2;
    }
    /* DHT11 拉高 80us */
    t = 0;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET) {
        if (++t > DHT11_TIMEOUT) return 3;
    }

    /* ---- 3. 读 40bit ---- */
    for (i = 0; i < 5; i++) {
        if (dht11_read_byte(&data[i]) != 0) return 4;
    }

    /* ---- 4. 校验 ---- */
    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
        return 5;

    if (humidity != NULL)    *humidity    = (float)data[0] + (float)data[1] * 0.1f;
    if (temperature != NULL) *temperature = (float)data[2] + (float)(data[3] & 0x7F) * 0.1f;

    /* 读完后恢复空闲输出高 */
    dht11_set_output();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);

    return 0;
}
