/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "esp8266.h"
#include "onenet.h"
#include "oled.h"
#include "debug.h"
#include "button.h"
#include "ui.h"
#include "dht11.h"
#include "hcsr04.h"
#include "bh1750.h"
#include "gps.h"
#include "mpu6050.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* ================= 物模型数据 =================
 * 上报属性（每 3 秒）: temperature / humidity / light / waterlevel
 * 接收属性（云端下发）: xxxMax / xxxMin / fanS / lightS / pumpS
 */

/* ---- 传感器数据（上报） ---- */
float temperature = 0.0f;   /* 温度  ℃   */
float humidity    = 0.0f;   /* 湿度  %RH */
float light       = 0.0f;   /* 光照  lux */
float waterlevel  = 0.0f;   /* 水位       */

/* ---- DHT11 实测温湿度（PB1 单总线，单独上报） ---- */
float dhtTemperature = 0.0f;   /* DHT11 温度 ℃  */
float dhtHumidity    = 0.0f;   /* DHT11 湿度 %RH */

/* ---- HC-SR04 超声波测距（TRIG=PB8, ECHO=PB9，单独上报） ---- */
float distance = 0.0f;         /* 距离 cm */

/* ---- BH1750 实测光照（I2C1 与 OLED 共用，单独上报） ---- */
float bhLight = 0.0f;          /* 光照 lx */

/* ---- GY-NEO6MV2 GPS（USART3_RX = PB11，单独上报） ---- */
float gpsLatitude  = 0.0f;     /* 纬度（十进制度，南纬为负） */
float gpsLongitude = 0.0f;     /* 经度（十进制度，西经为负） */
float gpsAltitude  = 0.0f;     /* 海拔 m */
float gpsSats      = 0.0f;     /* 卫星数 */
float gpsSpeed     = 0.0f;     /* 地面速度 km/h */

/* GPS UTC 时间（数值型，方便上报/调试） */
float gpsHour      = 0.0f;     /* 时 0~23 (UTC) */
float gpsMinute    = 0.0f;     /* 分 0~59 (UTC) */
float gpsSecond    = 0.0f;     /* 秒 0~59 (UTC) */

/* ---- MPU6050/6500 姿态角（I2C1 与 OLED/BH1750 共用，单独上报） ---- */
float mpuRoll  = 0.0f;         /* 绕 X 轴 度 */
float mpuPitch = 0.0f;         /* 绕 Y 轴 度 */
float mpuYaw   = 0.0f;         /* 绕 Z 轴 度（会漂移） */

/* ---- 阈值（仅接收，不上报） ---- */
float temperatureMax = 50.0f;
float temperatureMin = 0.0f;
float humidityMax    = 100.0f;
float humidityMin    = 0.0f;
float lightMax       = 1000.0f;
float lightMin       = 0.0f;
float waterlevelMax  = 100.0f;
float waterlevelMin  = 0.0f;

/* ---- 开关状态（仅接收，不上报，0=关 1=开） ---- */
float fanS   = 0.0f;        /* 风扇 */
float lightS = 0.0f;        /* 灯光 */
float pumpS  = 0.0f;        /* 水泵 */

/* debug 控制：1 = 每 3 秒用 ADC 刷新传感器值；0 = 保持 debug 写入的值 */
uint8_t g_adc_enable = 1;

/* USB 设备句柄（用于强制重新枚举） */
extern PCD_HandleTypeDef hpcd_USB_FS;

/**
  * @brief  云端控制量更新后的应用钩子
  * @note   由 OneNet_RevPro() 在解析完下行属性后调用。
  *         在这里把 fanS / lightS / pumpS 或阈值联动的执行器动作落地，
  *         例如驱动继电器、电机等（当前工程未分配输出引脚，先留空）。
  */
void SmartHouse_ApplyControl(void)
{
  /* TODO: 根据 fanS / lightS / pumpS 以及 *_Max / *_Min 控制执行器 */
}
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_USB_DEVICE_Init();
  /* 强制 USB 重新枚举，避免每次下载程序后必须重新插拔 USB 才能识别虚拟串口。
     F103 无软件上下拉控制（USB_DevConnect/Disconnect 为空函数），
     需直接操作 CNTR：
     断开：PDWN 使 USB 收发器掉电、释放内部 D+ 上拉，主机检测到设备拔出；
     重连：__HAL_PCD_ENABLE 重写 CNTR（清 PDWN/FRES），主机重新枚举 */
  hpcd_USB_FS.Instance->CNTR = USB_CNTR_PDWN | USB_CNTR_FRES;
  HAL_Delay(500);
  __HAL_PCD_ENABLE(&hpcd_USB_FS);
  HAL_Delay(100);
  /* USER CODE BEGIN 2 */
  /* 调试控制台（USB CDC） */
  Debug_Init();

  /* DHT11 温湿度传感器（PB1 单总线） */
  DHT11_Init();

  /* HC-SR04 超声波测距（TRIG=PB8, ECHO=PB9） */
  HCSR04_Init();

  /* BH1750 光照传感器（I2C1，与 OLED 共用 PB6/PB7） */
  if(BH1750_Init() == 0)
    UsartPrintf(USART_DEBUG, "[BH1750] init ok\r\n");
  else
    UsartPrintf(USART_DEBUG, "[BH1750] init fail, not found on I2C1 (try 'i2cscan')\r\n");

  /* GY-NEO6MV2 GPS（USART3_RX = PB11，中断接收） */
  GPS_Init();

  /* MPU6050/6500 姿态传感器（I2C1，PB6/PB7）
     MPU_Init() 内部已含上电、配置寄存器、读 WHO_AM_I；
     初始化成功后立即做一次陀螺零偏校准（静置约 0.5 秒） */
  if(MPU_Init() == 0)
  {
    UsartPrintf(USART_DEBUG, "[MPU] init ok, WHO_AM_I=0x%02X (0x68=6050, 0x70/71=6500/9250)\r\n",
                (unsigned)MPU_WhoAmI());
    UsartPrintf(USART_DEBUG, "[MPU] calibrating gyro, keep still...\r\n");
    MPU_CalibrateGyro();
    UsartPrintf(USART_DEBUG, "[MPU] gyro calibrated\r\n");
  }
  else
  {
    UsartPrintf(USART_DEBUG, "[MPU] init fail, not found on I2C1 (will retry in loop)\r\n");
  }

  /* 按键（PB12~PB15）+ TIM2 10ms 扫描中断 */
  Button_Init();

  /* 初始化 OLED（I2C1）。若未接 OLED 会自动跳过，不影响主流程 */
  OLED_Init();

  /* UI 状态机：上电显示主菜单 */
  UI_Init();
  UI_Task();

  /* 初始化 ESP8266（USART2） */
  ESP8266_Init();

  /* 建立与 OneNET MQTT 服务器的 TCP 连接（新版物联网平台） */
  while(ESP8266_SendCmd("AT+CIPSTART=\"TCP\",\"mqtts.heclouds.com\",1883\r\n", "CONNECT"))
    HAL_Delay(500);

  /* 连接 OneNET 平台并订阅属性设置 */
  while(OneNet_DevLink())
    HAL_Delay(500);
  OneNET_Subscribe();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    {
      static uint32_t last_adc = 0;
      static uint32_t last_dht = 0;
      static uint32_t last_ultr = 0;
      static uint32_t last_bh = 0;
      static uint32_t last_gps = 0;
      static uint32_t last_mpu = 0;
      static uint32_t last_report = 0;
      static uint32_t last_hb = 0;
      uint32_t now = HAL_GetTick();

      /* 1. 轮询云端下行命令（属性设置），收到即解析并更新对应变量。
       *    ESP8266_GetIPD 内部超时约 50ms，无数据时不会长时间阻塞。 */
      OneNet_RevPro_Poll();

      /* 1b. USB 调试控制台：解析 read/write/press 等命令，并处理按键自动松开 */
      Debug_Task();

      /* 2. 每 200ms 采集一次 ADC（映射为 0~100），并记录一个曲线点 */
      if((now - last_adc) >= 200u)
      {
        last_adc = now;

        /* 4 路 ADC -> 温度/光照/湿度/水位（0~100）；调试可用 adc off 冻结 */
        if(g_adc_enable)
        {
          ADC_ReadSensors(&temperature, &light, &humidity, &waterlevel);
        }

        /* 记录曲线历史样本（200ms 一个点） */
        UI_PushSample(temperature, humidity, light, waterlevel);
      }

      /* 2b. 每 2 秒读一次 DHT11 温湿度（DHT11 采样周期需 >= 1s） */
      if((now - last_dht) >= 2000u)
      {
        last_dht = now;

        float t = 0.0f, h = 0.0f;
        uint8_t rc = DHT11_Read(&t, &h);
        if(rc == 0)
        {
          dhtTemperature = t;
          dhtHumidity    = h;
          {
            uint8_t raw[5];
            DHT11_GetRaw(raw);
            UsartPrintf(USART_DEBUG, "[DHT11] T=%.1f H=%.1f raw=%02X %02X %02X %02X %02X\r\n",
                        t, h, raw[0], raw[1], raw[2], raw[3], raw[4]);
          }
        }
        else
        {
          /* rc: 1=无响应 2=响应低超时 3=响应高超时 4=读bit超时 5=校验错 */
          uint8_t raw[5];
          DHT11_GetRaw(raw);
          UsartPrintf(USART_DEBUG, "[DHT11] read fail, rc=%u raw=%02X %02X %02X %02X %02X\r\n",
                      rc, raw[0], raw[1], raw[2], raw[3], raw[4]);
        }
      }

      /* 2c. 每 200ms 测一次超声波距离（HC-SR04 间隔需 >= 60ms） */
      if((now - last_ultr) >= 200u)
      {
        last_ultr = now;

        float d = 0.0f;
        uint8_t rc = HCSR04_Read(&d);
        if(rc == 0)
        {
          distance = d;
          UsartPrintf(USART_DEBUG, "[SR04] distance=%.1f cm\r\n", d);
        }
        else
        {
          /* rc: 1=ECHO无响应 2=ECHO超时 */
          UsartPrintf(USART_DEBUG, "[SR04] read fail, rc=%u\r\n", rc);
        }
      }

      /* 2d. 每 500ms 读一次 BH1750 光照（转换约 120ms，间隔需 >= 180ms） */
      if((now - last_bh) >= 500u)
      {
        last_bh = now;

        float lx = 0.0f;

        /* 掉线/首次探测失败时周期性重试，避免永久离线 */
        if(!BH1750_IsReady())
        {
          if(BH1750_ReInit() != 0)
          {
            UsartPrintf(USART_DEBUG, "[BH1750] still not found (use 'i2cscan')\r\n");
            goto bh_done;
          }
          UsartPrintf(USART_DEBUG, "[BH1750] online\r\n");
        }

        if(BH1750_Read(&lx) == 0)
        {
          bhLight = lx;
          UsartPrintf(USART_DEBUG, "[BH1750] light=%.0f lx\r\n", lx);
        }
        else
        {
          UsartPrintf(USART_DEBUG, "[BH1750] read fail (will retry)\r\n");
        }

      bh_done:;
      }

      /* 2e. 每 1 秒读一次 GPS 定位数据（NMEA 在中断里解析） */
      if((now - last_gps) >= 1000u)
      {
        last_gps = now;

        gps_data_t g;
        GPS_GetData(&g);
        if(g.valid)
        {
          gpsLatitude  = (float)g.latitude  / GPS_COORD_SCALE;
          gpsLongitude = (float)g.longitude / GPS_COORD_SCALE;
          gpsAltitude  = (float)g.altitude_dm / GPS_ALT_SCALE;
          gpsSats      = (float)g.sats;
          /* 速度：1 节 = 1.852 km/h；speed_cs 是 0.01 节 */
          gpsSpeed     = (float)g.speed_cs * 0.01f * 1.852f;

          gpsHour      = (float)g.hour;
          gpsMinute    = (float)g.minute;
          gpsSecond    = (float)g.second;

          /* UTC 时间 + 8h = 北京时间（跨日则 +1 天，仅用于显示） */
          {
            unsigned bh = g.hour + 8u;
            unsigned bday = g.day;
            if (bh >= 24u) { bh -= 24u; bday += 1u; }

            UsartPrintf(USART_DEBUG,
                        "[GPS] %.6f, %.6f alt=%.1fm sats=%u spd=%.1fkm/h\r\n",
                        gpsLatitude, gpsLongitude, gpsAltitude,
                        (unsigned)g.sats, gpsSpeed);
            UsartPrintf(USART_DEBUG,
                        "[GPS] 20%02u-%02u-%02u %02u:%02u:%02u UTC\r\n",
                        (unsigned)g.year, (unsigned)g.month, (unsigned)g.day,
                        (unsigned)g.hour, (unsigned)g.minute, (unsigned)g.second);
            UsartPrintf(USART_DEBUG,
                        "[GPS] CST %02u:%02u:%02u (UTC+8, date %02u-%02u)\r\n",
                        bh, (unsigned)g.minute, (unsigned)g.second,
                        (unsigned)g.month, bday);
          }
        }
        else if(g.updated)
        {
          UsartPrintf(USART_DEBUG, "[GPS] no fix (sats=%u)\r\n", (unsigned)g.sats);
        }
      }

      /* 2f. 每 100ms 读一次 MPU6050/6500 并做姿态解算，持续上报 */
      if((now - last_mpu) >= 100u)
      {
        uint16_t dt = (uint16_t)(now - last_mpu);
        last_mpu = now;

        /* 掉线时自动重试：重连成功后重新校准一次 */
        if(!MPU_IsReady())
        {
          if(MPU_ReInit() != 0)
            goto mpu_done;
          UsartPrintf(USART_DEBUG, "[MPU] re-init ok, WHO_AM_I=0x%02X\r\n",
                      (unsigned)MPU_WhoAmI());
          MPU_CalibrateGyro();
        }

        if(MPU_Update(dt) == 0)
        {
          mpu_data_t m;
          MPU_GetData(&m);
          mpuRoll  = m.roll;
          mpuPitch = m.pitch;
          mpuYaw   = m.yaw;

          /* 每 1 秒打印一次姿态角（方便看趋势） */
          {
            static uint32_t last_mpu_log = 0;
            if((now - last_mpu_log) >= 1000u)
            {
              last_mpu_log = now;
              UsartPrintf(USART_DEBUG, "[MPU] roll=%.1f pitch=%.1f yaw=%.1f (T=%.1fC)\r\n",
                          mpuRoll, mpuPitch, mpuYaw, m.temperature);
            }
          }
        }

      mpu_done:;
      }

      /* 2g. 每 3 秒把当前值上报到 OneNET */
      if((now - last_report) >= 3000u)
      {
        last_report = now;

        UsartPrintf(USART_DEBUG, "[ADC] T=%.1f L=%.1f H=%.1f W=%.1f\r\n",
                    temperature, light, humidity, waterlevel);

        OneNet_SendData();
      }

      /* 3. UI：处理按键事件 + 刷新显示 */
      UI_Task();

      /* 4. 每秒一次心跳，确保能看出主循环是否还在跑 */
      if((now - last_hb) >= 1000u)
      {
        last_hb = now;
        UsartPrintf(USART_DEBUG, "[Heartbeat] %lu s\r\n", (unsigned long)(now / 1000u));
      }

      HAL_Delay(100);
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC|RCC_PERIPHCLK_USB;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
