#include "oled.h"
#include "i2c.h"
#include "font.h"
#include <string.h>
#include <stdio.h>

/* SSD1306 I2C 8-bit 地址：VCC 侧 0x78 / GND 侧 0x7A */
#define OLED_I2C_ADDR    0x78

extern I2C_HandleTypeDef hi2c1;

/* OLED 是否在线。若不在线，所有写操作直接跳过，
   避免 HAL_I2C 每次 100ms 超时把主循环拖死（看起来像“卡死”）。 */
static uint8_t g_oled_ok = 0;
static uint8_t g_oled_gram[128][8];

/* ------------------------- 底层 I2C 读写 ------------------------- */
static void OLED_WriteCmd(uint8_t cmd)
{
	if (!g_oled_ok) return;
	HAL_I2C_Mem_Write(&hi2c1, OLED_I2C_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, &cmd, 1, 100);
}

static void OLED_WriteData(uint8_t data)
{
	if (!g_oled_ok) return;
	HAL_I2C_Mem_Write(&hi2c1, OLED_I2C_ADDR, 0x40, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
}

static void OLED_SetPos(uint8_t x, uint8_t y)
{
	OLED_WriteCmd(0xB0 + y);
	OLED_WriteCmd(((x & 0xF0) >> 4) | 0x10);
	OLED_WriteCmd(x & 0x0F);
}

/* ------------------------------ 初始化 ------------------------------ */
uint8_t OLED_Init(void)
{
	/* 先探测器件是否存在，不存在就直接放弃，避免初始化阶段长时间阻塞 */
	if (HAL_I2C_IsDeviceReady(&hi2c1, OLED_I2C_ADDR, 2, 100) != HAL_OK)
	{
		g_oled_ok = 0;
		return 0;
	}
	g_oled_ok = 1;
	HAL_Delay(100);

	OLED_WriteCmd(0xAE); /* display off */
	OLED_WriteCmd(0x00);
	OLED_WriteCmd(0x10);
	OLED_WriteCmd(0x40);
	OLED_WriteCmd(0xB0);
	OLED_WriteCmd(0x81); OLED_WriteCmd(0xFF); /* contrast */
	OLED_WriteCmd(0xA1);                      /* segment remap */
	OLED_WriteCmd(0xA6);                      /* normal display */
	OLED_WriteCmd(0xA8); OLED_WriteCmd(0x3F); /* multiplex 1/64 */
	OLED_WriteCmd(0xC8);                      /* COM scan dir */
	OLED_WriteCmd(0xD3); OLED_WriteCmd(0x00); /* display offset */
	OLED_WriteCmd(0xD5); OLED_WriteCmd(0x80); /* clock */
	OLED_WriteCmd(0xD9); OLED_WriteCmd(0xF1); /* pre-charge */
	OLED_WriteCmd(0xDA); OLED_WriteCmd(0x12); /* COM pins */
	OLED_WriteCmd(0xDB); OLED_WriteCmd(0x40); /* VCOMH */
	OLED_WriteCmd(0x8D); OLED_WriteCmd(0x14); /* charge pump */
	OLED_WriteCmd(0xAF);                      /* display on */

	OLED_Clear();
	return 1;
}

void OLED_Clear(void)
{
	memset(g_oled_gram, 0, sizeof(g_oled_gram));
	OLED_Refresh();
}

/* clear the gram only (no I2C traffic); call OLED_Refresh() afterwards */
void OLED_Clear_Gram(void)
{
	memset(g_oled_gram, 0, sizeof(g_oled_gram));
}

void OLED_Clear_some(uint8_t x, uint8_t y, uint8_t x2, uint8_t y2)
{
	int j = 0, i = 0;
	int yy = y / 8;
	int yy2 = y2 / 8 + 1;
	if (!g_oled_ok) return;
	for (j = yy; j < yy2; j++)
	{
		OLED_WriteCmd(0xB0 + j);
		OLED_WriteCmd(0x00);
		OLED_WriteCmd(0x10 + (x >> 4));
		OLED_WriteCmd(x & 0x0F);
		for (i = x; i < x2; i++)
		{
			g_oled_gram[i][j] = 0x00;
			OLED_WriteData(0x00);
		}
	}
}

void OLED_Refresh_some(uint8_t x, uint8_t y, uint8_t x2, uint8_t y2)
{
	int j = 0, i = 0;
	int yy = y / 8;
	int yy2 = y2 / 8 + 1;
	if (!g_oled_ok) return;
	for (j = yy; j < yy2; j++)
	{
		OLED_WriteCmd(0xB0 + j);
		OLED_WriteCmd(0x00);
		OLED_WriteCmd(0x10);
		for (i = x; i < x2; i++)
		{
			OLED_WriteData(g_oled_gram[i][j]);
		}
	}
}

void OLED_Refresh(void)
{
	int j = 0, i = 0;
	if (!g_oled_ok) return;
	for (j = 0; j < 8; j++)
	{
		OLED_WriteCmd(0xB0 + j);
		OLED_WriteCmd(0x00);
		OLED_WriteCmd(0x10);
		for (i = 0; i < 128; i++)
		{
			OLED_WriteData(g_oled_gram[i][j]);
		}
	}
}

/* ------------------------------ 字符显示 ------------------------------ */
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t size)
{
	uint8_t c = 0, i = 0;
	if (!g_oled_ok) return;
	c = chr - ' ';
	OLED_SetPos(x, y);

	if (size == 16)
	{
		for (i = 0; i < 8; i++)
		{
			OLED_WriteData(F8X16[c * 16 + i]);
		}
		OLED_SetPos(x, y + 1);
		for (i = 0; i < 8; i++)
		{
			OLED_WriteData(F8X16[c * 16 + i + 8]);
		}
	}
	else if (size == 8)
	{
		for (i = 0; i < 6; i++)
		{
			OLED_WriteData(F6x8[c][i]);
		}
	}
}

void OLED_ShowString(uint8_t x, uint8_t y, const uint8_t *str, uint8_t size)
{
	uint8_t i = 0;
	uint8_t char_width = (size == 16) ? 8 : 6;
	uint8_t char_page = (size == 16) ? 2 : 1;

	if (!g_oled_ok) return;

	while (str[i] != '\0')
	{
		if (x + char_width > 127 || (y + char_page - 1) > 7)
		{
			break;
		}
		OLED_ShowChar(x, y, str[i], size);
		x += char_width;
		i++;
	}
}

/* draw a string into the gram (not the panel); call OLED_Refresh() after */
void OLED_DrawString(uint8_t x, uint8_t y, const uint8_t *str, uint8_t size)
{
	uint8_t i = 0;
	uint8_t w = (size == 16) ? 8 : 6;
	uint8_t p = (size == 16) ? 2 : 1;

	while (str[i] != '\0')
	{
		uint8_t c = (uint8_t)(str[i] - ' ');
		uint8_t k;
		if (x + w > 127 || (y + p - 1) > 7) break;
		if (size == 16)
		{
			for (k = 0; k < 8; k++) g_oled_gram[x + k][y]     = F8X16[c * 16 + k];
			for (k = 0; k < 8; k++) g_oled_gram[x + k][y + 1] = F8X16[c * 16 + k + 8];
		}
		else
		{
			for (k = 0; k < 6; k++) g_oled_gram[x + k][y] = F6x8[c][k];
		}
		x += w;
		i++;
	}
}

void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size)
{
	uint8_t i, temp;
	uint8_t enshow = 0;
	for (i = 0; i < len; i++)
	{
		temp = (num / OLED_Pow(10, len - i - 1)) % 10;
		if (enshow == 0 && i < (len - 1))
		{
			if (temp == 0)
			{
				OLED_ShowChar(x + i * ((size == 16) ? 8 : 6), y, ' ', size);
				continue;
			}
			else
			{
				enshow = 1;
			}
		}
		OLED_ShowChar(x + i * ((size == 16) ? 8 : 6), y, temp + '0', size);
	}
}

uint32_t OLED_Pow(uint8_t m, uint8_t n)
{
	uint32_t result = 1;
	while (n--)
	{
		result *= m;
	}
	return result;
}

void OLED_Draw_Point(uint8_t x, uint8_t y)
{
	uint8_t pos, bx, temp = 0;
	if (x > 127 || y > 63) return;
	pos = y / 8;
	bx = y % 8;
	temp = 1 << bx;
	g_oled_gram[x][pos] |= temp;
}

void OLED_Draw_Line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
	unsigned int t;
	int xerr = 0, yerr = 0, delta_x, delta_y, distance;
	int incx, incy, uRow, uCol;
	delta_x = x2 - x1;
	delta_y = y2 - y1;
	uRow = x1;
	uCol = y1;
	if (delta_x > 0) incx = 1;
	else if (delta_x == 0) incx = 0;
	else { incx = -1; delta_x = -delta_x; }
	if (delta_y > 0) incy = 1;
	else if (delta_y == 0) incy = 0;
	else { incy = -1; delta_y = -delta_y; }
	if (delta_x > delta_y) distance = delta_x;
	else distance = delta_y;
	for (t = 0; t <= distance + 1; t++)
	{
		OLED_Draw_Point(uRow, uCol);
		xerr += delta_x;
		yerr += delta_y;
		if (xerr > distance) { xerr -= distance; uRow += incx; }
		if (yerr > distance) { yerr -= distance; uCol += incy; }
	}
}

/* --------------------------- 应用层：英文显示 --------------------------- */
static void OLED_ShowValue(uint8_t x, uint8_t y, float v)
{
	char buf[12];
	if (v < 0.0f)   v = 0.0f;
	if (v > 999.9f) v = 999.9f;
	snprintf(buf, sizeof(buf), "%.1f", v);
	OLED_ShowString(x, y, (const uint8_t *)buf, 16);
}

static void OLED_ShowOnOff(uint8_t x, uint8_t y, float s)
{
	OLED_ShowString(x, y, (const uint8_t *)((s >= 0.5f) ? "ON " : "OFF"), 16);
}

/**
  * @brief  分页显示物模型数据（英文）
  * @param  page: 0=传感器  1=上限  2=下限  3=开关
  */
void OLED_ShowPage(uint8_t page)
{
	extern float temperature, humidity, light, waterlevel;
	extern float temperatureMax, temperatureMin;
	extern float humidityMax, humidityMin;
	extern float lightMax, lightMin;
	extern float waterlevelMax, waterlevelMin;
	extern float fanS, lightS, pumpS;

	if (!g_oled_ok) return;

	OLED_Clear();

	switch (page % 4u)
	{
	case 0: /* 实时值 */
		OLED_ShowString(0, 0, (const uint8_t *)"TEMP :", 16); OLED_ShowValue(56, 0, temperature);
		OLED_ShowString(0, 2, (const uint8_t *)"HUMI :", 16); OLED_ShowValue(56, 2, humidity);
		OLED_ShowString(0, 4, (const uint8_t *)"LIGHT:", 16); OLED_ShowValue(56, 4, light);
		OLED_ShowString(0, 6, (const uint8_t *)"WATER:", 16); OLED_ShowValue(56, 6, waterlevel);
		break;

	case 1: /* 上限阈值 */
		OLED_ShowString(0, 0, (const uint8_t *)"T-MAX:", 16); OLED_ShowValue(56, 0, temperatureMax);
		OLED_ShowString(0, 2, (const uint8_t *)"H-MAX:", 16); OLED_ShowValue(56, 2, humidityMax);
		OLED_ShowString(0, 4, (const uint8_t *)"L-MAX:", 16); OLED_ShowValue(56, 4, lightMax);
		OLED_ShowString(0, 6, (const uint8_t *)"W-MAX:", 16); OLED_ShowValue(56, 6, waterlevelMax);
		break;

	case 2: /* 下限阈值 */
		OLED_ShowString(0, 0, (const uint8_t *)"T-MIN:", 16); OLED_ShowValue(56, 0, temperatureMin);
		OLED_ShowString(0, 2, (const uint8_t *)"H-MIN:", 16); OLED_ShowValue(56, 2, humidityMin);
		OLED_ShowString(0, 4, (const uint8_t *)"L-MIN:", 16); OLED_ShowValue(56, 4, lightMin);
		OLED_ShowString(0, 6, (const uint8_t *)"W-MIN:", 16); OLED_ShowValue(56, 6, waterlevelMin);
		break;

	default: /* 开关状态 */
		OLED_ShowString(0, 0, (const uint8_t *)"FAN  :", 16); OLED_ShowOnOff(56, 0, fanS);
		OLED_ShowString(0, 2, (const uint8_t *)"LIGHT:", 16); OLED_ShowOnOff(56, 2, lightS);
		OLED_ShowString(0, 4, (const uint8_t *)"PUMP :", 16); OLED_ShowOnOff(56, 4, pumpS);
		break;
	}
}
