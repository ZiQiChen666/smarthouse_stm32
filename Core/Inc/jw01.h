#ifndef __JW01_H
#define __JW01_H

#include "stm32f1xx_hal.h"

/*
 * JW01 / 空气质量传感器驱动（与 GPS 共用 USART3，PB11 = USART3_RX）
 *
 *   —— 为什么和 GPS 共用一路串口 ——
 *   USART3 收到的字节先交给 JW01_RouteByte()：
 *     - 帧首 '$'  -> NMEA 整句交给 GPS_FeedByte()
 *     - 帧首 0x2C -> JW01 二进制帧
 *   两者波特率一致（9600），帧首不同，不会误判。
 *
 *   —— 实测帧格式（6 字节） ——
 *     byte0 : 0x2C          帧头
 *     byte1 : 数据 1
 *     byte2 : 数据 2
 *     byte3 : 数据 3
 *     byte4 : 数据 4
 *     byte5 : 校验 = (byte0+byte1+byte2+byte3+byte4) & 0xFF
 *
 *   实测样本：
 *     2C 00 00 03 FF 2E     校验 0x2E ✓
 *     2C 01 5E 03 FF 8D     校验 0x8D ✓
 *
 *   由样本可拆出的数值：
 *     帧1: 16位大端(raw1,raw2)=0x0000=0   16位大端(raw3,raw4)=0x03FF=1023
 *     帧2: 16位大端(raw1,raw2)=0x015E=350 16位大端(raw3,raw4)=0x03FF=1023
 *
 *   由于不确定每个字段的物理含义（各固件不同），本驱动把 4 个数据字节
 *   原样保存，并同时给出两种常用拆法：
 *     val1 = (raw[1]<<8) | raw[2]
 *     val2 = (raw[3]<<8) | raw[4]
 *   字段含义请对照你的模块手册，用 JW01_MAP_* 宏指定即可。
 *
 *   —— 校验 ——
 *   实测校验就是「前 5 字节之和取低 8 位」，因此这里强制校验
 *   （JW01_STRICT_CHECKSUM=1），校验不过的帧直接丢弃。
 */

/* ---- 帧参数 ---- */
#define JW01_FRAME_LEN        6u      /* 整帧长度（含帧头与校验） */
#define JW01_DATA_LEN         4u      /* 数据字节个数 */
#define JW01_HDR              0x2Cu   /* 帧头字节 */

/* 1 = 强制校验（默认，已实测通过）；0 = 不强制 */
#define JW01_STRICT_CHECKSUM  1

typedef struct {
    uint8_t  raw[JW01_FRAME_LEN];  /* 完整原始帧 */
    uint8_t  data[JW01_DATA_LEN];  /* 4 个数据字节 raw[1..4] */
    uint16_t val1;                 /* 大端(raw[1],raw[2]) */
    uint16_t val2;                 /* 大端(raw[3],raw[4]) */
    uint8_t  checksum_rx;          /* 收到的校验字节 raw[5] */
    uint8_t  checksum_calc;        /* 计算出的校验 */
    uint8_t  valid;                /* 1 = 收到过至少一帧 */
    uint32_t last_tick;            /* 最近一次成功解析时刻 */
    uint32_t frames_ok;            /* 累计成功帧数 */
    uint32_t frames_err;           /* 累计校验失败帧数 */
} jw01_data_t;

/* 初始化（清空接收状态与数据） */
void JW01_Init(void);

/* 由 USART3 中断逐字节调用（内部使用） */
void JW01_FeedByte(uint8_t b);

/*
 * 串口字节路由：USART3 的每个字节都调用这个函数。
 * 内部根据帧首自动在 GPS($开头) 与 JW01(0x2C 开头) 之间分发。
 */
void JW01_RouteByte(uint8_t b);

/* 读取最新解析结果（拷贝一份，避免被中断打断） */
void JW01_GetData(jw01_data_t *out);

#endif /* __JW01_H */
