/**
  ******************************************************************************
  * @file    hcsr04.c
  * @brief   HC-SR04 超声波测距驱动（TRIG=PB8, ECHO=PB9）
  *
  *  接线：
  *    VCC  -> 5V（HC-SR04 一般 5V 供电）
  *    TRIG -> PB8
  *    ECHO -> PB9   （★ ECHO 输出 5V，PB9 是 5V 容忍的 FT 引脚，可直连）
  *    GND  -> GND
  *
  *  时序：
  *    1. TRIG 拉高 >= 10us，触发一次测量
  *    2. 模块发 8 个 40kHz 脉冲，然后把 ECHO 拉高
  *    3. ECHO 高电平持续时间 = 声波往返时间
  *    4. 距离(cm) = 时间(us) / 58
  *
  *  计时用 DWT CYCCNT（和 DHT11 一样），微秒换算基于实测 HCLK，
  *  不用 SystemCoreClock（本工程该变量停在 8MHz，不准）。
  ******************************************************************************
  */

#include "hcsr04.h"

/* ECHO 高电平超时（微秒）。HC-SR04 量程约 4m，往返约 23ms，取 30ms 上限 */
#define HCSR04_TIMEOUT_US   30000u

/* ------------------------------------------------------------------ */
/* DWT 微秒计时（与 dht11.c 中相同的做法）                             */
/* ------------------------------------------------------------------ */
static uint32_t s_cpu_mhz = 72u;

static void hcsr04_calc_cpu_freq(void)
{
    uint32_t hclk = HAL_RCC_GetHCLKFreq();
    s_cpu_mhz = hclk / 1000000u;
    if (s_cpu_mhz == 0u) s_cpu_mhz = 72u;
}

static void hcsr04_dwt_enable(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t hcsr04_cycles(void)
{
    return DWT->CYCCNT;
}

static inline uint32_t hcsr04_cycles_to_us(uint32_t cyc)
{
    return cyc / s_cpu_mhz;
}

/* 微秒 -> 周期数。全程用周期比较：无整数除法误差，
 * 且无符号差值天然支持 CYCCNT 回绕。 */
static inline uint32_t hcsr04_us_to_cycles(uint32_t us)
{
    return us * s_cpu_mhz;
}

/* 阻塞延时（周期域比较） */
static void hcsr04_delay_us(uint32_t us)
{
    uint32_t t0 = hcsr04_cycles();
    uint32_t n  = hcsr04_us_to_cycles(us);
    while ((uint32_t)(hcsr04_cycles() - t0) < n) { __NOP(); }
}

/* 直读寄存器，避免 HAL 函数调用开销影响计时 */
#define HCSR04_ECHO_READ() \
    ((HCSR04_ECHO_PORT->IDR & HCSR04_ECHO_PIN) ? 1u : 0u)

/* ------------------------------------------------------------------ */
uint8_t HCSR04_Init(void)
{
    GPIO_InitTypeDef s = {0};

    hcsr04_calc_cpu_freq();
    hcsr04_dwt_enable();

    /* TRIG: PB8 推挽输出，默认低 */
    s.Pin   = HCSR04_TRIG_PIN;
    s.Mode  = GPIO_MODE_OUTPUT_PP;
    s.Pull  = GPIO_NOPULL;
    s.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(HCSR04_TRIG_PORT, &s);
    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);

    /* ECHO: PB9 输入 */
    s.Pin  = HCSR04_ECHO_PIN;
    s.Mode = GPIO_MODE_INPUT;
    s.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(HCSR04_ECHO_PORT, &s);

    return 0;
}

/*
 * 返回值：0 成功
 *         1 ECHO 一直没有变高（模块无响应 / 未接）
 *         2 ECHO 高电平超时（超出量程 / 回波丢失）
 */
uint8_t HCSR04_Read(float *distance_cm)
{
    uint32_t t0;
    uint32_t echo_us;

    /* ---- 1. TRIG 给 >= 10us 高电平 ---- */
    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
    hcsr04_delay_us(2);                 /* 先拉低 2us，保证下降沿彻底 */
    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_SET);
    hcsr04_delay_us(12);                /* 高电平保持 12us（规格要求 >= 10us） */
    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);

    /* ---- 2. 等 ECHO 变高 ---- */
    t0 = hcsr04_cycles();
    while (HCSR04_ECHO_READ() == 0) {
        if ((uint32_t)(hcsr04_cycles() - t0) > hcsr04_us_to_cycles(HCSR04_TIMEOUT_US)) return 1;
    }

    /* ---- 3. 量 ECHO 高电平宽度 ---- */
    t0 = hcsr04_cycles();
    while (HCSR04_ECHO_READ() == 1) {
        if ((uint32_t)(hcsr04_cycles() - t0) > hcsr04_us_to_cycles(HCSR04_TIMEOUT_US)) return 2;
    }
    echo_us = hcsr04_cycles_to_us((uint32_t)(hcsr04_cycles() - t0));

    /* ---- 4. 换算距离：cm = us / 58 ---- */
    if (distance_cm != NULL) {
        *distance_cm = (float)echo_us / 58.0f;
    }
    return 0;
}
