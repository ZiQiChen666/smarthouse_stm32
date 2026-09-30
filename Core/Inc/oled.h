#ifndef __OLED_H
#define __OLED_H

#include "stm32f1xx_hal.h"

/* 底层驱动 */
uint8_t OLED_Init(void);
void OLED_Clear(void);
void OLED_Clear_Gram(void);
void OLED_Clear_some(uint8_t x, uint8_t y, uint8_t x2, uint8_t y2);
void OLED_Refresh_some(uint8_t x, uint8_t y, uint8_t x2, uint8_t y2);
void OLED_Refresh(void);
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t size);
void OLED_ShowString(uint8_t x, uint8_t y, const uint8_t *str, uint8_t size);
void OLED_DrawString(uint8_t x, uint8_t y, const uint8_t *str, uint8_t size);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size);
uint32_t OLED_Pow(uint8_t m, uint8_t n);
void OLED_Draw_Point(uint8_t x, uint8_t y);
void OLED_Draw_Line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

/* 应用层：分页显示（0=实时值 1=上限 2=下限 3=开关） */
void OLED_ShowPage(uint8_t page);

#endif
