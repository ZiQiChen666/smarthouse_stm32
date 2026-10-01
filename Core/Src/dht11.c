/**
  ******************************************************************************
  * @file    dht11.c
  * @brief   DHT11 温湿度传感器驱动（单总线，PB1）
  *
  *  接线：
  *    PB1  ---- DHT11 DATA
  *    VCC  ---- 3.3V
  *    GND  ---- GND
  *    DATA 与 VCC 之间需要 4.7k~10k 上拉电阻（模块一般已自带）
  *
  *  时序：
  *    1. 主机拉低总线 >= 18ms，然后释放
  *    2. DHT11 响应：拉低 80us，再拉高 80us
  *    3. DHT11 发送 40bit：每位 50us 低电平起始，
  *       高电平 26~28us = 0，70us = 1
  *    4. 数据：湿度整数 + 湿度小数 + 温度整数 + 温度小数 + 校验和
  *
  *  PB1 用开漏输出 + 上拉：输出 0 拉低总线，输出 1 时释放总线，
  *  避免推挽输出与 DHT11 抢总线。
  *
  *  ★ 位判定改用 DWT CYCCNT 真实计时，不再用循环次数猜时间，
  *    这是之前 “read fail” 的主要原因。
  ******************************************************************************
  */

#include "dht11.h"

#define DHT11_PIN           DHT11_GPIO_PIN
#define DHT11_PORT          DHT11_GPIO_PORT

/* 超时（微秒），远大于正常时序，只用于防止死等 */
#define DHT11_TIMEOUT_US    1000u

/* ------------------------------------------------------------------ */
/* DWT 微秒计时                                                        */
/* ------------------------------------------------------------------ */
static void dht11_dwt_enable(void)
{
    /* 使能 DWT 的 CYCCNT 计数器（Cortex-M3 支持） */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t dht11_cycles(void)
{
    return DWT->CYCCNT;
}

/* 把时钟周期换算成微秒。
 *
 * 注意：不要用 SystemCoreClock！本工程在 SystemClock_Config() 里把主频
 * 配成了 72MHz，但 CMSIS 的 SystemCoreClock 变量停留在初值 8000000
 * （CubeMX 生成的 main.c 没调 SystemCoreClockUpdate()），拿它做换算
 * 会偏差 9 倍。这里直接读 RCC 寄存器，拿到真实的 HCLK。
 */
static uint32_t s_cpu_mhz = 72u;   /* 周期/us，DHT11_Init 里实测更新 */

/* 最近一次读到的原始 5 字节（调试用） */
static uint8_t s_last_raw[5] = {0, 0, 0, 0, 0};

static void dht11_calc_cpu_freq(void)
{
    uint32_t hclk = HAL_RCC_GetHCLKFreq();
    s_cpu_mhz = hclk / 1000000u;
    if (s_cpu_mhz == 0u) s_cpu_mhz = 72u;
}

static inline uint32_t dht11_cycles_to_us(uint32_t cyc)
{
    return cyc / s_cpu_mhz;
}

/* ------------------------------------------------------------------ */
/* GPIO 方向切换                                                       */
/* ------------------------------------------------------------------ */
static void dht11_set_output(void)
{
    GPIO_InitTypeDef s = {0};
    s.Pin   = DHT11_PIN;
    s.Mode  = GPIO_MODE_OUTPUT_OD;   /* 开漏 */
    s.Pull  = GPIO_PULLUP;
    s.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_PORT, &s);
}

static void dht11_set_input(void)
{
    GPIO_InitTypeDef s = {0};
    s.Pin  = DHT11_PIN;
    s.Mode = GPIO_MODE_INPUT;
    s.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_PORT, &s);
}

/* 把引脚写成寄存器直读，避免 HAL 函数调用开销影响时序 */
#define DHT11_READ()   ((DHT11_PORT->IDR & DHT11_PIN) ? 1u : 0u)

/* ------------------------------------------------------------------ */
/* 读 1 bit：低电平 50us 起始，高电平长度决定 0/1                      */
/* ------------------------------------------------------------------ */
static uint8_t dht11_read_bit(uint8_t *bit)
{
    uint32_t t0;

    /* 等 50us 低电平结束 */
    t0 = dht11_cycles();
    while (DHT11_READ() == 0) {
        if (dht11_cycles_to_us(dht11_cycles() - t0) > DHT11_TIMEOUT_US) return 1;
    }

    /* 量高电平持续多久 */
    t0 = dht11_cycles();
    while (DHT11_READ() == 1) {
        if (dht11_cycles_to_us(dht11_cycles() - t0) > DHT11_TIMEOUT_US) return 2;
    }
    *bit = (dht11_cycles_to_us(dht11_cycles() - t0) > 50u) ? 1u : 0u;
    return 0;
}

static uint8_t dht11_read_byte(uint8_t *out)
{
    uint8_t i, byte = 0;
    for (i = 0; i < 8; i++) {
        uint8_t b;
        if (dht11_read_bit(&b) != 0) return 1;
        byte = (uint8_t)((byte << 1) | b);
    }
    *out = byte;
    return 0;
}

/* ------------------------------------------------------------------ */
uint8_t DHT11_Init(void)
{
    dht11_calc_cpu_freq();            /* 实测 HCLK，避免 SystemCoreClock 不准 */
    dht11_dwt_enable();
    dht11_set_output();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    HAL_Delay(1000);                  /* DHT11 上电后需 >= 1s 稳定 */
    return 0;
}

/*
 * 返回值：0 成功
 *         1 无响应（总线没被 DHT11 拉低）
 *         2 DHT11 低电平响应超时
 *         3 DHT11 高电平响应超时
 *         4 读 40bit 过程超时
 *         5 校验和错误
 */
uint8_t DHT11_Read(float *temperature, float *humidity)
{
    uint8_t data[5] = {0, 0, 0, 0, 0};
    uint8_t i;
    uint32_t t0;

    /* ---- 1. 主机起始信号：拉低 >= 18ms ---- */
    dht11_set_output();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);

    /* ---- 2. 释放总线，切输入 ---- */
    dht11_set_input();

    /* DHT11 应在 20~40us 内拉低总线（80us 低电平响应） */
    t0 = dht11_cycles();
    while (DHT11_READ() == 1) {
        if (dht11_cycles_to_us(dht11_cycles() - t0) > DHT11_TIMEOUT_US) return 1;
    }

    /* 等 80us 低电平结束 */
    t0 = dht11_cycles();
    while (DHT11_READ() == 0) {
        if (dht11_cycles_to_us(dht11_cycles() - t0) > DHT11_TIMEOUT_US) return 2;
    }

    /* 等 80us 高电平结束 —— 退出时已进入第 1 个 bit 的 50us 低电平 */
    t0 = dht11_cycles();
    while (DHT11_READ() == 1) {
        if (dht11_cycles_to_us(dht11_cycles() - t0) > DHT11_TIMEOUT_US) return 3;
    }

    /* ---- 3. 读 40bit ---- */
    for (i = 0; i < 5; i++) {
        if (dht11_read_byte(&data[i]) != 0) return 4;
    }

    /* 保存原始字节（调试用） */
    for (i = 0; i < 5; i++) s_last_raw[i] = data[i];

    /* 读完恢复空闲输出高 */
    dht11_set_output();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);

    /* ---- 4. 校验 ---- */
    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
    {
        /* 把原始 5 字节回传出去，方便定位 */
        if (humidity != NULL)    *humidity    = (float)data[0];
        if (temperature != NULL) *temperature = (float)data[2];
        return 5;
    }

    if (humidity != NULL)
        *humidity = (float)data[0] + (float)data[1] * 0.1f;
    if (temperature != NULL)
        *temperature = (float)data[2] + (float)(data[3] & 0x7F) * 0.1f;

    return 0;
}

/* 调试用：把最近一次读到的 5 个原始字节拷出来 */
void DHT11_GetRaw(uint8_t out[5])
{
    uint8_t i;
    if (out == NULL) return;
    for (i = 0; i < 5; i++) out[i] = s_last_raw[i];
}
