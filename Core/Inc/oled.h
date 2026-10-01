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
void OLED_Draw_Point(uint8_t x, uint8_t y);
void OLED_Draw_Line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

#endif
