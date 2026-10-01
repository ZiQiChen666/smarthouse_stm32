/**
  ******************************************************************************
  * @file    param_dump.c
  * @brief   打印参数区 Flash 原始内容（调试用）
  ******************************************************************************
  */

#include "param.h"
#include "usart.h"

#define PARAM_PAGE_A_ADDR    0x0801F800UL
#define PARAM_PAGE_B_ADDR    0x0801FC00UL

extern float temperatureMax, temperatureMin;
extern float humidityMax, humidityMin;
extern float lightMax, lightMin;
extern float waterlevelMax, waterlevelMin;

/* 打印一页前若干字节的原始内容 + 解析出的记录 */
void Param_DumpPage(uint32_t page_addr, const char *tag)
{
    const uint8_t *p = (const uint8_t *)page_addr;
    uint16_t i;

    UsartPrintf(USART_DEBUG, "[FLASH] %s @0x%08X:\r\n", tag, (unsigned)page_addr);

    /* 打印前 64 字节原始值 */
    for (i = 0; i < 64; i += 16)
    {
        UsartPrintf(USART_DEBUG, "  %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                    p[i+0], p[i+1], p[i+2],  p[i+3],  p[i+4],  p[i+5],  p[i+6],  p[i+7],
                    p[i+8], p[i+9], p[i+10], p[i+11], p[i+12], p[i+13], p[i+14], p[i+15]);
    }

    /* 解析第一条记录（若有效） */
    {
        uint32_t magic;
        uint32_t seq;
        float    vals[8];
        uint8_t  cs;

        for (i = 0; i < 8; i++) { ((uint8_t *)&magic)[i] = 0; }
        for (i = 0; i < 4; i++) ((uint8_t *)&magic)[i] = p[i];
        for (i = 0; i < 4; i++) ((uint8_t *)&seq)[i]   = p[4 + i];
        for (i = 0; i < 32; i++) ((uint8_t *)vals)[i]  = p[8 + i];
        cs = p[40];

        UsartPrintf(USART_DEBUG, "  magic=0x%08X seq=%lu cs=0x%02X\r\n",
                    (unsigned)magic, (unsigned long)seq, cs);
        UsartPrintf(USART_DEBUG, "  T[%.1f,%.1f] H[%.1f,%.1f] L[%.1f,%.1f] W[%.1f,%.1f]\r\n",
                    vals[1], vals[0], vals[3], vals[2], vals[5], vals[4], vals[7], vals[6]);
    }
}

void Param_Dump(void)
{
    UsartPrintf(USART_DEBUG, "[RAM] T[%.1f,%.1f] H[%.1f,%.1f] L[%.1f,%.1f] W[%.1f,%.1f]\r\n",
                temperatureMin, temperatureMax,
                humidityMin, humidityMax,
                lightMin, lightMax,
                waterlevelMin, waterlevelMax);
    Param_DumpPage(PARAM_PAGE_A_ADDR, "PAGE A (62)");
    Param_DumpPage(PARAM_PAGE_B_ADDR, "PAGE B (63)");
}
