/**
  ******************************************************************************
  * @file    mpu6050.c
  * @brief   MPU6050 / MPU6500 六轴姿态解算（I2C1：PB6=SCL, PB7=SDA）
  *
  *  兼容：
  *      MPU6050  WHO_AM_I = 0x68
  *      MPU6500  WHO_AM_I = 0x70
  *      MPU9250  WHO_AM_I = 0x71
  *  这三者寄存器布局基本一致，用同一套初始化即可。
  *
  *  量程配置：
  *      加速度 ±2g   -> 灵敏度 16384 LSB/g
  *      陀螺   ±250°/s -> 灵敏度 131 LSB/(°/s)
  *
  *  姿态解算：
  *      roll  = atan2(ay, az)                     （加速度直接算）
  *      pitch = atan2(-ax, sqrt(ay^2+az^2))
  *      再用陀螺积分做互补滤波（高频信陀螺，低频信加速度）：
  *          angle = K*(angle + gyro*dt) + (1-K)*accel_angle
  *      yaw 只能靠陀螺积分（无磁力计，会漂移）
  *
  *  为避免拉入昂贵的 atan2/sqrt 导致 flash 紧张，这里用查表 + 定点近似的
  *  方式实现 atan2 和 sqrt（精度足够用于姿态显示）。
  ******************************************************************************
  */

#include "mpu6050.h"
#include "i2c.h"
#include <string.h>
#include <math.h>

extern I2C_HandleTypeDef hi2c1;

#define MPU_I2C_TIMEOUT     100u

/* ---- 寄存器 ---- */
#define REG_SMPLRT_DIV      0x19
#define REG_CONFIG          0x1A
#define REG_GYRO_CONFIG     0x1B
#define REG_ACCEL_CONFIG    0x1C
#define REG_ACCEL_XOUT_H    0x3B
#define REG_TEMP_OUT_H      0x41
#define REG_GYRO_XOUT_H     0x43
#define REG_PWR_MGMT_1      0x6B
#define REG_WHO_AM_I        0x75

/* ---- 量程灵敏度 ---- */
#define ACCEL_SCALE         16384.0f    /* ±2g  */
#define GYRO_SCALE          131.0f      /* ±250°/s */

/* 互补滤波系数（越大越信陀螺） */
#define COMP_ALPHA          0.96f
/* 弧度转度 */
#define RAD2DEG             57.29578f

static uint8_t  s_mpu_ok   = 0;
static uint8_t  s_mpu_addr = MPU_ADDR_LOW;
static uint8_t  s_who      = 0;   /* WHO_AM_I：0x68=MPU6050, 0x70/0x71=MPU6500/9250 */
/* 陀螺零偏（开机校准） */
static float s_gyro_bias_x = 0.0f;
static float s_gyro_bias_y = 0.0f;
static float s_gyro_bias_z = 0.0f;

/* 解算出的角度 */
static volatile mpu_data_t s_mpu;
/* 是否已完成首次姿态初始化（用加速度角铺初值） */
static uint8_t s_seeded = 0;

/* ------------------------------------------------------------------ */
/* 底层读写                                                            */
/* ------------------------------------------------------------------ */
static uint8_t mpu_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    if (!s_mpu_ok) return 1;
    if (HAL_I2C_Master_Transmit(&hi2c1, s_mpu_addr, buf, 2, MPU_I2C_TIMEOUT) != HAL_OK)
    {
        s_mpu_ok = 0;
        return 1;
    }
    return 0;
}

static uint8_t mpu_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (!s_mpu_ok) return 1;
    if (HAL_I2C_Master_Transmit(&hi2c1, s_mpu_addr, &reg, 1, MPU_I2C_TIMEOUT) != HAL_OK)
    {
        s_mpu_ok = 0;
        return 1;
    }
    if (HAL_I2C_Master_Receive(&hi2c1, s_mpu_addr, buf, len, MPU_I2C_TIMEOUT) != HAL_OK)
    {
        s_mpu_ok = 0;
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
uint8_t MPU_Init(void)
{
    s_mpu_ok = 0;

    /* 扫描两个可能的地址：AD0=GND -> 0xD0 / AD0=VCC -> 0xD2 */
    if (HAL_I2C_IsDeviceReady(&hi2c1, MPU_ADDR_LOW, 3, MPU_I2C_TIMEOUT) == HAL_OK)
        s_mpu_addr = MPU_ADDR_LOW;
    else if (HAL_I2C_IsDeviceReady(&hi2c1, MPU_ADDR_HIGH, 3, MPU_I2C_TIMEOUT) == HAL_OK)
        s_mpu_addr = MPU_ADDR_HIGH;
    else
        return 1;

    s_mpu_ok = 1;

    /* 唤醒（退出睡眠） */
    if (mpu_write_reg(REG_PWR_MGMT_1, 0x00) != 0) return 2;
    HAL_Delay(50);

    /* 采样率分频：1kHz/(1+7) = 125Hz */
    mpu_write_reg(REG_SMPLRT_DIV, 0x07);
    /* 数字低通滤波 44Hz */
    mpu_write_reg(REG_CONFIG, 0x03);
    /* 陀螺 ±250°/s */
    mpu_write_reg(REG_GYRO_CONFIG, 0x00);
    /* 加速度 ±2g */
    mpu_write_reg(REG_ACCEL_CONFIG, 0x00);
    HAL_Delay(20);

    /* 读 WHO_AM_I（兼容 MPU6050=0x68 / MPU6500=0x70 / MPU9250=0x71，不强制校验） */
    if (mpu_read_regs(REG_WHO_AM_I, &s_who, 1) != 0)
        s_who = 0;

    memset((void *)&s_mpu, 0, sizeof(s_mpu));
    s_mpu.online = 1;
    s_seeded = 0;      /* 下次 MPU_Update 用加速度角铺初值 */

    return 0;
}

uint8_t MPU_IsReady(void)  { return s_mpu_ok; }
uint8_t MPU_ReInit(void)   { return MPU_Init(); }
uint8_t MPU_WhoAmI(void)   { return s_who; }

/* ------------------------------------------------------------------ */
uint8_t MPU_ReadRaw(int16_t *ax, int16_t *ay, int16_t *az,
                    int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t buf[14];

    /* 一次性读 0x3B~0x48：accel(6) + temp(2) + gyro(6) */
    if (mpu_read_regs(REG_ACCEL_XOUT_H, buf, 14) != 0) return 1;

    if (ax) *ax = (int16_t)((buf[0]  << 8) | buf[1]);
    if (ay) *ay = (int16_t)((buf[2]  << 8) | buf[3]);
    if (az) *az = (int16_t)((buf[4]  << 8) | buf[5]);
    /* buf[6..7] 是温度 */
    if (gx) *gx = (int16_t)((buf[8]  << 8) | buf[9]);
    if (gy) *gy = (int16_t)((buf[10] << 8) | buf[11]);
    if (gz) *gz = (int16_t)((buf[12] << 8) | buf[13]);

    return 0;
}

/* ------------------------------------------------------------------ */
/* 零偏校准：静止采样求陀螺平均值                                      */
/* ------------------------------------------------------------------ */
void MPU_CalibrateGyro(void)
{
    int32_t sx = 0, sy = 0, sz = 0;
    int16_t gx, gy, gz;
    uint8_t i;
    uint8_t good = 0;
    const uint8_t N = 100;

    if (!s_mpu_ok) return;

    /* 先丢弃前几次（刚上电数据未稳） */
    for (i = 0; i < 5; i++) { MPU_ReadRaw(NULL, NULL, NULL, &gx, &gy, &gz); HAL_Delay(5); }

    for (i = 0; i < N; i++)
    {
        if (MPU_ReadRaw(NULL, NULL, NULL, &gx, &gy, &gz) == 0)
        {
            sx += gx; sy += gy; sz += gz;
            good++;
        }
        HAL_Delay(5);
    }

    if (good > 0)
    {
        s_gyro_bias_x = (float)sx / (float)good;
        s_gyro_bias_y = (float)sy / (float)good;
        s_gyro_bias_z = (float)sz / (float)good;
    }
}

/* ------------------------------------------------------------------ */
/* 姿态解算                                                            */
/* ------------------------------------------------------------------ */
uint8_t MPU_Update(uint16_t dt_ms)
{
    int16_t ax, ay, az, gx, gy, gz;
    float fax, fay, faz;      /* 加速度，单位 g */
    float rgx, rgy, rgz;      /* 角速度，单位 °/s */
    float dt;
    float acc_roll, acc_pitch;
    float gyro_roll, gyro_pitch, gyro_yaw;

    if (!s_mpu_ok) return 1;
    if (MPU_ReadRaw(&ax, &ay, &az, &gx, &gy, &gz) != 0) return 1;

    /* 转成物理量 */
    fax = (float)ax / ACCEL_SCALE;
    fay = (float)ay / ACCEL_SCALE;
    faz = (float)az / ACCEL_SCALE;

    rgx = ((float)gx - s_gyro_bias_x) / GYRO_SCALE;
    rgy = ((float)gy - s_gyro_bias_y) / GYRO_SCALE;
    rgz = ((float)gz - s_gyro_bias_z) / GYRO_SCALE;

    dt = (float)dt_ms / 1000.0f;

    /* ---- 加速度算 roll / pitch ---- */
    /* roll  = atan2(ay, az) */
    acc_roll  = atan2f(fay, faz) * RAD2DEG;
    /* pitch = atan2(-ax, sqrt(ay^2+az^2)) */
    acc_pitch = atan2f(-fax, sqrtf(fay * fay + faz * faz)) * RAD2DEG;

    /* ---- 陀螺积分 ---- */
    gyro_roll  = s_mpu.roll  + rgx * dt;
    gyro_pitch = s_mpu.pitch + rgy * dt;
    gyro_yaw   = s_mpu.yaw   + rgz * dt;

    /* ---- 互补滤波 ---- */
    /* 首次调用时用加速度角直接初始化，避免从 0 慢慢爬升 */
    if (!s_seeded)
    {
        s_seeded    = 1;
        s_mpu.roll  = acc_roll;
        s_mpu.pitch = acc_pitch;
        s_mpu.yaw   = 0.0f;
    }
    else
    {
        s_mpu.roll  = COMP_ALPHA * gyro_roll  + (1.0f - COMP_ALPHA) * acc_roll;
        s_mpu.pitch = COMP_ALPHA * gyro_pitch + (1.0f - COMP_ALPHA) * acc_pitch;
        s_mpu.yaw   = gyro_yaw;   /* 没有磁力计，只能积分 */
    }

    /* ---- 芯片温度（可选） ---- */
    {
        uint8_t tb[2];
        if (mpu_read_regs(REG_TEMP_OUT_H, tb, 2) == 0)
        {
            int16_t raw = (int16_t)((tb[0] << 8) | tb[1]);
            s_mpu.temperature = (float)raw / 340.0f + 36.53f;
        }
    }

    s_mpu.online = 1;
    return 0;
}

void MPU_GetData(mpu_data_t *out)
{
    uint8_t i;
    const volatile uint8_t *src;
    uint8_t *dst;

    if (out == NULL) return;
    src = (const volatile uint8_t *)&s_mpu;
    dst = (uint8_t *)out;
    for (i = 0; i < sizeof(s_mpu); i++) dst[i] = src[i];
}
