#ifndef __DEBUG_H
#define __DEBUG_H

#include "stm32f1xx_hal.h"

/* 调试控制台（走 USB CDC 虚拟串口） */

/* 复位命令行缓冲 */
void Debug_Init(void);

/* USB CDC 接收中断里逐字节调用（只做缓冲，不做打印） */
void Debug_RxByte(uint8_t b);

/* 主循环调用：处理自动松开的按键，并解析/执行整行命令 */
void Debug_Task(void);

#endif /* __DEBUG_H */
