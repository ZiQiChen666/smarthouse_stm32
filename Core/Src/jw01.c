/**
  ******************************************************************************
  * @file    jw01.c
  * @brief   空气质量传感器（6 字节 0x2C 帧）驱动，与 GPS 共用 USART3。
  *
  *  帧格式： 0x2C | d0 | d1 | d2 | d3 | checksum
  *  checksum = (0x2C + d0 + d1 + d2 + d3) & 0xFF
  ******************************************************************************
  */

#include "jw01.h"
#include "gps.h"
#include <string.h>

/* 接收缓冲区：稍大于一帧 */
#define JW01_BUF_MAX   12u

/* 路由状态 */
typedef enum {
    ROUTE_IDLE = 0,   /* 空闲：根据帧首决定交给谁 */
    ROUTE_GPS,        /* 正在收 NMEA 整句，直到 '\n' */
    ROUTE_JW01        /* 正在收 JW01 二进制帧 */
} route_state_t;

static uint8_t       s_buf[JW01_BUF_MAX];
static uint8_t       s_len = 0;
static route_state_t s_route = ROUTE_IDLE;

static jw01_data_t   s_jw01;

/* ------------------------------------------------------------------ */
/* 解析一帧完整数据                                                    */
/* ------------------------------------------------------------------ */
static void jw01_parse_frame(const uint8_t *f, uint8_t len)
{
    uint8_t i, sum = 0;

    if (len < JW01_FRAME_LEN)
        return;

    if (f[0] != JW01_HDR)
        return;

    /* 保留原始帧 */
    memcpy(s_jw01.raw, f, JW01_FRAME_LEN);
    for (i = 0; i < JW01_DATA_LEN; i++)
        s_jw01.data[i] = f[1 + i];

    /* 校验 = 前 5 字节之和低 8 位 */
    sum = 0;
    for (i = 0; i < JW01_FRAME_LEN - 1u; i++)
        sum = (uint8_t)(sum + f[i]);
    s_jw01.checksum_rx   = f[JW01_FRAME_LEN - 1u];
    s_jw01.checksum_calc = sum;

#if JW01_STRICT_CHECKSUM
    if (sum != f[JW01_FRAME_LEN - 1u]) {
        s_jw01.frames_err++;
        return;
    }
#endif

    /* 两种 16 位大端拆法 */
    s_jw01.val1 = (uint16_t)(((uint16_t)f[1] << 8) | f[2]);
    s_jw01.val2 = (uint16_t)(((uint16_t)f[3] << 8) | f[4]);

    s_jw01.valid     = 1;
    s_jw01.last_tick = HAL_GetTick();
    s_jw01.frames_ok++;
}

/* ------------------------------------------------------------------ */
void JW01_Init(void)
{
    memset(&s_jw01, 0, sizeof(s_jw01));
    s_len = 0;
    s_route = ROUTE_IDLE;
}

void JW01_FeedByte(uint8_t b)
{
    if (s_len < JW01_BUF_MAX)
        s_buf[s_len++] = b;

    if (s_len >= JW01_FRAME_LEN) {
        jw01_parse_frame(s_buf, s_len);
        s_len = 0;
        s_route = ROUTE_IDLE;
    }
}

/* ------------------------------------------------------------------ */
/* USART3 字节路由                                                     */
/* ------------------------------------------------------------------ */
void JW01_RouteByte(uint8_t b)
{
    switch (s_route)
    {
    case ROUTE_GPS:
        GPS_FeedByte(b);
        if (b == '\n')
            s_route = ROUTE_IDLE;
        return;

    case ROUTE_JW01:
        JW01_FeedByte(b);
        return;

    case ROUTE_IDLE:
    default:
        break;
    }

    if (b == '$') {
        s_route = ROUTE_GPS;
        GPS_FeedByte(b);
    } else if (b == JW01_HDR) {
        s_route = ROUTE_JW01;
        s_len = 0;
        JW01_FeedByte(b);
    }
    /* 其它字节：丢弃（等待帧首） */
}

void JW01_GetData(jw01_data_t *out)
{
    if (out == NULL) return;
    memcpy(out, (const void *)&s_jw01, sizeof(s_jw01));
}
