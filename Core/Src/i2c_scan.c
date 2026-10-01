/**
  ******************************************************************************
  * @file    i2c_scan.c
  * @brief   I2C1 总线扫描（调试用）
  *
  *  逐个 7bit 地址(0x08~0x77)发探测，把有 ACK 的地址打印出来。
  *  用法：USB 调试台输入  i2cscan
  *
  *  常见器件地址（8bit，HAL 用）：
  *      SSD1306 OLED : 0x78
  *      BH1750 (ADDR=GND) : 7bit 0x23 -> 8bit 0x46
  *      BH1750 (ADDR=VCC) : 7bit 0x5C -> 8bit 0xB8
  ******************************************************************************
  */

#include "i2c_scan.h"
#include "i2c.h"
#include "usart.h"

extern I2C_HandleTypeDef hi2c1;

void I2C_ScanBus(void)
{
    uint8_t addr;
    uint8_t found = 0;

    UsartPrintf(USART_DEBUG, "[I2C] scanning bus 1 (PB6/PB7)...\r\n");

    for (addr = 0x08; addr < 0x78; addr++)
    {
        /* HAL 需要 8bit 地址 */
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 2, 20) == HAL_OK)
        {
            UsartPrintf(USART_DEBUG, "[I2C] found: 7bit 0x%02X  (8bit 0x%02X)\r\n",
                        addr, (unsigned)(addr << 1));
            found++;
        }
    }

    if (found == 0)
        UsartPrintf(USART_DEBUG, "[I2C] no device found (check wiring / pull-ups)\r\n");
    else
        UsartPrintf(USART_DEBUG, "[I2C] scan done, %u device(s)\r\n", found);
}
