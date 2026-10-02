#ifndef __OLED_H
#define __OLED_H

#include "stm32f1xx_hal.h"

/* 底层驱动 */
uint8_t OLED_Init(void);
void OLED_Clear(void);
void OLED_Clear_Gram(void);
void OLED_Clear_some(uint8_t x, uint8_t y, uint8_t x2, uint8_t y2);
void OLED_Refresh(void);
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t size);
void OLED_ShowString(uint8_t x, uint8_t y, const uint8_t *str, uint8_t size);
void OLED_DrawString(uint8_t x, uint8_t y, const uint8_t *str, uint8_t size);

/* 中文 16x16 (依赖 Core/Src/hz16.c 字库, 需把该文件加入工程) */
void OLED_ShowCN(uint8_t x, uint8_t y, const char *utf8);      /* 直接写屏, 单个汉字 */
void OLED_ShowCNString(uint8_t x, uint8_t y, const char *str); /* 直接写屏, 中英混排  */
void OLED_DrawCN(uint8_t x, uint8_t y, const char *utf8);      /* 写显存, 单个汉字     */
void OLED_DrawCNString(uint8_t x, uint8_t y, const char *str); /* 写显存, 中英混排     */
void OLED_Draw_Point(uint8_t x, uint8_t y);
void OLED_Draw_Line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

#endif
