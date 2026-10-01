#ifndef __RC522_H
#define __RC522_H

#include "stm32f1xx_hal.h"

/*
 * RC522 RFID 读卡模块驱动（软件 SPI，GPIO 模拟）
 *
 *  接线：
 *      SDA/CS  -> PA15
 *      SCK     -> PA5
 *      MOSI    -> PA7
 *      MISO    -> PA6
 *      RST     -> 3.3V（或自定义引脚；本驱动默认不控制 RST）
 *      VCC     -> 3.3V
 *      GND     -> GND
 *      IRQ     -> 不接
 *
 *  为什么用软件 SPI：
 *      本工程 Flash 只剩约 7KB，引入 HAL SPI 库（stm32f1xx_hal_spi.c）
 *      会多占 3~5KB。RC522 对 SPI 速度要求很低（<10MHz），
 *      GPIO 模拟完全够用，且省 Flash、时序可控。
 *
 *  说明：
 *      PA15 默认是 JTDI，需关闭 JTAG 保留 SWD 才能当普通 GPIO 用。
 *      本工程 stm32f1xx_hal_msp.c 里已有 __HAL_AFIO_REMAP_SWJ_NOJTAG()，
 *      所以 PA15 可直接使用。
 */

/* ---------- 引脚定义 ---------- */
#define RC522_CS_PORT       GPIOA
#define RC522_CS_PIN        GPIO_PIN_15

#define RC522_SCK_PORT      GPIOA
#define RC522_SCK_PIN       GPIO_PIN_5

#define RC522_MOSI_PORT     GPIOA
#define RC522_MOSI_PIN      GPIO_PIN_7

#define RC522_MISO_PORT     GPIOA
#define RC522_MISO_PIN      GPIO_PIN_6

/* MIFARE Classic 卡片 UID 最大 10 字节（4/7/10） */
#define RC522_UID_MAX       10

typedef struct {
    uint8_t  uid[RC522_UID_MAX];   /* 卡片 UID */
    uint8_t  uid_len;              /* UID 长度（4 / 7 / 10） */
    uint8_t  sak;                  /* SAK：区分卡类型 */
    uint8_t  present;              /* 1 = 当前检测到卡片 */
    uint32_t last_seen_tick;       /* 最后一次读到的时刻 */
} rc522_card_t;

uint8_t RC522_Init(void);

/* 寻找是否有卡（无卡返回 1）。找到时把 UID 填到 card */
uint8_t RC522_RequestCard(rc522_card_t *card);

/* 防冲突：读取卡片 UID（需要在 RequestCard 成功后调用） */
uint8_t RC522_Anticoll(rc522_card_t *card);

/* 一体化：寻卡 + 防冲突，成功返回 0 并填充 card */
uint8_t RC522_ReadCard(rc522_card_t *card);

/* 让卡片进入休眠（读完后调用，避免重复触发） */
void RC522_Halt(void);

#endif /* __RC522_H */
