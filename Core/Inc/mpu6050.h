#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32f1xx_hal.h"

/*
 * MPU6050 / MPU6500 六轴姿态传感器驱动
 *   SCL -> PB6（与 OLED / BH1750 共用 I2C1）
 *   SDA -> PB7
 *   VCC -> 3.3V, GND -> GND
 *   AD0 -> GND（地址 0x68）；接 VCC 则 0x69（驱动会自动扫描）
 *
 *  兼容 MPU6050(WHO_AM_I=0x68) 与 MPU6500/9250(WHO_AM_I=0x70/0x71)。
 *
 *  输出三个角度（单位：度）：
 *      roll  = 绕 X 轴（前后翻滚）
 *      pitch = 绕 Y 轴（左右俯仰）
 *      yaw   = 绕 Z 轴（水平旋转，仅靠陀螺积分，会缓慢漂移）
 *
 *  解算方式：加速度计算 roll/pitch（静态准、有噪声，偏航不可用），
 *            陀螺仪积分算角度，再做互补滤波融合 roll/pitch。
 *            yaw 只能靠陀螺积分（无磁力计），会漂移。
 */

#define MPU_ADDR_LOW    0xD0    /* AD0=GND：7bit 0x68 -> 8bit 0xD0 */
#define MPU_ADDR_HIGH   0xD2    /* AD0=VCC：7bit 0x69 -> 8bit 0xD2 */

typedef struct {
    float roll;
    float pitch;
    float yaw;
    float temperature;   /* 芯片内部温度 ℃（粗略） */
    uint8_t online;      /* 1 = 器件在线 */
} mpu_data_t;

uint8_t MPU_Init(void);          /* 0=成功 */
uint8_t MPU_IsReady(void);
uint8_t MPU_ReInit(void);
uint8_t MPU_WhoAmI(void);   /* WHO_AM_I：0x68=MPU6050, 0x70/0x71=MPU6500/9250 */

/* 读一次原始数据 */
uint8_t MPU_ReadRaw(int16_t *ax, int16_t *ay, int16_t *az,
                    int16_t *gx, int16_t *gy, int16_t *gz);

/* 读原始数据并做姿态解算，更新内部角度（dt_ms 为本次与上次的间隔毫秒） */
uint8_t MPU_Update(uint16_t dt_ms);

/* 取最近一次解算出的角度 */
void MPU_GetData(mpu_data_t *out);

/* 零偏校准：静止放置，采样若干次求陀螺零偏（开机调一次） */
void MPU_CalibrateGyro(void);

#endif /* __MPU6050_H */
