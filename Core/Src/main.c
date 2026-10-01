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

      /* 2b. 每 3 秒把当前值上报到 OneNET */
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
