/**
  ******************************************************************************
  * @file    debug.c
  * @brief   USB 虚拟串口调试控制台
  *
  *  命令格式（大小写不敏感，行尾 \r 或 \n）：
  *    read  <name>             读变量，例如: read temperature
  *    write <name> <value>     写变量，例如: write temperature 56.5
  *    <PB12|PB13|PB14> Press <ms>   模拟按下并保持 ms 毫秒后自动松开
  *    press   <PBxx> <ms>      同上
  *    release <PBxx>           立即松开
  *    adc on|off               打开/关闭 ADC 实时刷新（off 后 write 的传感器值保持不变）
  *    vars                     打印所有变量
  *    help                     帮助
  *
  *  接收流程：USB 中断 -> CDC_Receive_FS -> Debug_RxByte(逐字节入缓冲)
  *            主循环 -> Debug_Task(解析并执行命令，打印走 UsartPrintf)
  ******************************************************************************
  */

#include "debug.h"
#include "usart.h"
#include "button.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ---- 物模型变量（定义在 main.c） ---- */
extern float temperature;
extern float humidity;
extern float light;
extern float waterlevel;
extern float dhtTemperature, dhtHumidity;
extern float temperatureMax, temperatureMin;
extern float humidityMax, humidityMin;
extern float lightMax, lightMin;
extern float waterlevelMax, waterlevelMin;
extern float fanS, lightS, pumpS;

/* 1 = 每 3 秒用 ADC 刷新传感器值；0 = 保持 debug 写入的值 */
extern uint8_t g_adc_enable;

#define DEBUG_LINE_MAX   128

static volatile char     g_line[DEBUG_LINE_MAX];
static volatile uint16_t g_line_len = 0;
static volatile uint8_t  g_line_ready = 0;

/* ---- 模拟按键 PB12 / PB13 / PB14 / PB15（由 button 模块统一管理） ---- */
static const char *const g_btn_name[BTN_COUNT] = { "PB12", "PB13", "PB14", "PB15" };

/* ---- 可读写变量表 ---- */
typedef struct { const char *name; float *addr; } debug_var_t;

static const debug_var_t g_vars[] = {
	{ "temperature",    &temperature    },
	{ "humidity",       &humidity       },
	{ "light",          &light          },
	{ "waterlevel",     &waterlevel     },
	{ "dhtTemperature", &dhtTemperature },
	{ "dhtHumidity",    &dhtHumidity    },
	{ "temperatureMax", &temperatureMax },
	{ "temperatureMin", &temperatureMin },
	{ "humidityMax",    &humidityMax    },
	{ "humidityMin",    &humidityMin    },
	{ "lightMax",       &lightMax       },
	{ "lightMin",       &lightMin       },
	{ "waterlevelMax",  &waterlevelMax  },
	{ "waterlevelMin",  &waterlevelMin  },
	{ "fanS",           &fanS           },
	{ "lightS",         &lightS         },
	{ "pumpS",          &pumpS          },
};
#define VAR_COUNT   (sizeof(g_vars) / sizeof(g_vars[0]))

/* ------------------------------ 小工具 ------------------------------ */
static char to_lower(char c)
{
	return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

/* 大小写不敏感等比较：相等返回 1 */
static int streq_ci(const char *a, const char *b)
{
	if (a == NULL || b == NULL) return 0;
	while (*a != '\0' && *b != '\0')
	{
		if (to_lower(*a) != to_lower(*b)) return 0;
		a++; b++;
	}
	return (*a == '\0' && *b == '\0') ? 1 : 0;
}

static int btn_index(const char *name)
{
	int i;
	for (i = 0; i < BTN_COUNT; i++)
		if (streq_ci(name, g_btn_name[i])) return i;
	return -1;
}

static float *var_find(const char *name)
{
	unsigned int i;
	for (i = 0; i < VAR_COUNT; i++)
		if (streq_ci(name, g_vars[i].name)) return g_vars[i].addr;
	return NULL;
}

static int tokenize(char *s, char *tok[], int max)
{
	int n = 0;
	char *p = s;
	while (*p != '\0' && n < max)
	{
		while (*p == ' ' || *p == '\t') p++;
		if (*p == '\0') break;
		tok[n++] = p;
		while (*p != '\0' && *p != ' ' && *p != '\t') p++;
		if (*p != '\0') { *p = '\0'; p++; }
	}
	return n;
}

/* ------------------------------ 命令实现 ------------------------------ */
static void cmd_help(void)
{
	UsartPrintf(USART_DEBUG, "[DBG] read <name> | write <name> <value> | vars | help\r\n");
	UsartPrintf(USART_DEBUG, "[DBG] <PB12..PB15> Press <ms> | release <PBxx> | adc on|off\r\n");
	UsartPrintf(USART_DEBUG, "[DBG] PB12=+ PB13=- PB14=OK PB15=Back (short<700ms, long>=700ms)\r\n");
}

static void cmd_vars(void)
{
	unsigned int i;
	UsartPrintf(USART_DEBUG, "[DBG] vars:\r\n");
	for (i = 0; i < VAR_COUNT; i++)
		UsartPrintf(USART_DEBUG, "  %-16s = %.2f\r\n", g_vars[i].name, *(g_vars[i].addr));
	UsartPrintf(USART_DEBUG, "  PB12=%u PB13=%u PB14=%u PB15=%u\r\n",
				(unsigned)Button_IsDown(BTN_PB12), (unsigned)Button_IsDown(BTN_PB13),
				(unsigned)Button_IsDown(BTN_PB14), (unsigned)Button_IsDown(BTN_PB15));
}

static void cmd_read(const char *name)
{
	int bi = btn_index(name);
	float *p;

	if (bi >= 0)
	{
		UsartPrintf(USART_DEBUG, "[DBG] %s = %u (%s)\r\n", g_btn_name[bi],
					(unsigned)Button_IsDown((button_id_t)bi),
					Button_IsDown((button_id_t)bi) ? "PRESS" : "RELEASE");
		return;
	}

	p = var_find(name);
	if (p != NULL)
	{
		UsartPrintf(USART_DEBUG, "[DBG] %s = %.2f\r\n", name, *p);
		return;
	}
	UsartPrintf(USART_DEBUG, "[DBG] unknown name: %s\r\n", name);
}

static void cmd_write(const char *name, const char *val)
{
	int bi = btn_index(name);
	float *p;

	if (bi >= 0)
	{
		Button_SimSet((button_id_t)bi, (atoi(val) != 0) ? 1u : 0u);
		UsartPrintf(USART_DEBUG, "[DBG] %s = %u\r\n", g_btn_name[bi],
					(unsigned)Button_IsDown((button_id_t)bi));
		return;
	}

	p = var_find(name);
	if (p != NULL)
	{
		*p = (float)atof(val);
		UsartPrintf(USART_DEBUG, "[DBG] %s = %.2f\r\n", name, *p);
		return;
	}
	UsartPrintf(USART_DEBUG, "[DBG] unknown name: %s\r\n", name);
}

static void cmd_press(const char *name, const char *ms)
{
	int bi = btn_index(name);
	int t;

	if (bi < 0)
	{
		UsartPrintf(USART_DEBUG, "[DBG] unknown button: %s\r\n", name);
		return;
	}

	t = atoi(ms);
	if (t < 0) t = 0;

	Button_SimPress((button_id_t)bi, (uint32_t)t);
	UsartPrintf(USART_DEBUG, "[DBG] %s PRESS, auto release in %d ms\r\n", g_btn_name[bi], t);
}

static void cmd_release(const char *name)
{
	int bi = btn_index(name);
	if (bi < 0)
	{
		UsartPrintf(USART_DEBUG, "[DBG] unknown button: %s\r\n", name);
		return;
	}
	Button_SimSet((button_id_t)bi, 0);
	UsartPrintf(USART_DEBUG, "[DBG] %s RELEASE\r\n", g_btn_name[bi]);
}

/* ------------------------------ 对外接口 ------------------------------ */
void Debug_Init(void)
{
	g_line_len = 0;
	g_line_ready = 0;
}

void Debug_RxByte(uint8_t b)
{
	if (g_line_ready) return;

	if (b == '\r' || b == '\n')
	{
		if (g_line_len > 0)
		{
			g_line[g_line_len] = '\0';
			g_line_ready = 1;
		}
		return;
	}

	if (b == 0x08 || b == 0x7F)   /* 退格 */
	{
		if (g_line_len > 0) g_line_len--;
		return;
	}

	if (g_line_len < DEBUG_LINE_MAX - 1)
	{
		g_line[g_line_len++] = (char)b;
	}
}

void Debug_Task(void)
{
	char line[DEBUG_LINE_MAX];
	char *tok[4];
	int n;

	if (!g_line_ready) return;

	/* 原子地取走一行 */
	__disable_irq();
	n = (int)g_line_len;
	if (n > DEBUG_LINE_MAX - 1) n = DEBUG_LINE_MAX - 1;
	memcpy(line, (const void *)g_line, (size_t)n);
	line[n] = '\0';
	g_line_len = 0;
	g_line_ready = 0;
	__enable_irq();

	n = tokenize(line, tok, 4);
	if (n <= 0) return;

	if (streq_ci(tok[0], "help"))
	{
		cmd_help();
	}
	else if (streq_ci(tok[0], "vars"))
	{
		cmd_vars();
	}
	else if (streq_ci(tok[0], "read") && n >= 2)
	{
		cmd_read(tok[1]);
	}
	else if (streq_ci(tok[0], "write") && n >= 4 && streq_ci(tok[2], "press"))
	{
		cmd_press(tok[1], tok[3]);              /* write PB12 press 2000 */
	}
	else if (streq_ci(tok[0], "write") && n >= 3)
	{
		cmd_write(tok[1], tok[2]);
	}
	else if (streq_ci(tok[0], "press") && n >= 3)
	{
		cmd_press(tok[1], tok[2]);
	}
	else if (streq_ci(tok[0], "release") && n >= 2)
	{
		cmd_release(tok[1]);
	}
	else if (streq_ci(tok[0], "adc") && n >= 2)
	{
		if (streq_ci(tok[1], "on"))
		{
			g_adc_enable = 1;
			UsartPrintf(USART_DEBUG, "[DBG] adc on\r\n");
		}
		else
		{
			g_adc_enable = 0;
			UsartPrintf(USART_DEBUG, "[DBG] adc off\r\n");
		}
	}
	else if (n >= 3 && streq_ci(tok[1], "press"))       /* PB12 Press 2000 */
	{
		cmd_press(tok[0], tok[2]);
	}
	else if (n >= 2 && streq_ci(tok[1], "release"))     /* PB12 release */
	{
		cmd_release(tok[0]);
	}
	else
	{
		UsartPrintf(USART_DEBUG, "[DBG] bad command, type 'help'\r\n");
	}
}
