#ifndef __GPS_H
#define __GPS_H

#include "stm32f1xx_hal.h"

/*
 * GY-NEO6MV2 (u-blox NEO-6M) GPS 模块驱动
 *   TX(模块) -> PB11 (USART3_RX)   ★ 只需接收，不用发
 *   RX(模块) -> PB10 (USART3_TX)   （可不接）
 *   VCC -> 3.3V/5V, GND -> GND
 *
 *  默认 9600 波特率，输出 NMEA-0183 语句。
 *  本驱动解析最常用的两条：
 *      $GPRMC / $GNRMC : 时间、定位有效标志、纬度、经度、速度
 *      $GPGGA / $GNGGA : 定位质量、卫星数、海拔
 *
 *  接收走 USART3 中断（和 ESP8266 一样），逐字节喂给 GPS_FeedByte()，
 *  在中断里完成整句解析（NMEA 句子很短，开销很小）。
 *
 *  数据用定点放大保存，避免浮点（省 RAM/CPU）：
 *      latitude  = 纬度 * 1e6 （int32，单位  1e-6 度，南纬为负）
 *      longitude = 经度 * 1e6 （int32，单位  1e-6 度，西经为负）
 *      altitude  = 海拔 * 10  （int16，单位 0.1m）
 *      speed     = 速度 * 100 （uint16，单位 0.01 节）
 *      utc_hhmmss = 时分秒各占字节
 *      valid  : 1 = 定位有效
 *      sats   : 卫星数
 */

/* 经纬度按 1e-6 度放大 */
#define GPS_COORD_SCALE     1000000L
/* 海拔按 0.1m 放大 */
#define GPS_ALT_SCALE       10L

typedef struct {
    int32_t  latitude;      /* 纬度 * 1e6，南纬为负 */
    int32_t  longitude;     /* 经度 * 1e6，西经为负 */
    uint8_t  valid;         /* 1 = 定位有效（RMC 的 A 状态） */
    uint8_t  fix_quality;   /* GGA 定位质量：0=无效 1=GPS 2=差分 */
    uint8_t  sats;          /* 卫星数 */
    int16_t  altitude_dm;   /* 海拔 * 10（0.1m） */
    uint16_t speed_cs;      /* 速度 * 100（0.01 节） */
    /* UTC 时间（NMEA 给的就是 UTC，北京时间 = UTC + 8h） */
    uint8_t  hour, minute, second;  /* 时 分 秒（UTC） */
    uint8_t  year;          /* 年，如 26 表示 2026 */
    uint8_t  month;         /* 月 1~12 */
    uint8_t  day;           /* 日 1~31 */
    uint8_t  updated;       /* 收到过有效定位后置 1（供上报判断） */
} gps_data_t;

void GPS_Init(void);

/* 由 USART3 中断逐字节调用 */
void GPS_FeedByte(uint8_t b);

/* 读取当前定位数据（拷贝一份，避免中断中被打断） */
void GPS_GetData(gps_data_t *out);

/* 经纬度格式化成 度.分 字符串，如 "31.123456" / "121.654321" */
void GPS_FormatCoord(int32_t scaled, char *buf, uint8_t buflen);

#endif /* __GPS_H */
