#ifndef __I2C_SCAN_H
#define __I2C_SCAN_H

#include "stm32f1xx_hal.h"

/* 扫描 I2C1 总线上所有有响应的 7bit 地址，结果用 UsartPrintf 打印 */
void I2C_ScanBus(void);

#endif /* __I2C_SCAN_H */
