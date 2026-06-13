/*
 * lcd.c
 *
 *  Created on: 2026. 5. 7.
 *      Author: ADJ
 */

#include "lcd.h"
#include "main.h"
#include "ssd1306.h"
#include "global_define.h"
#include "adc.h"
#include "agc.h"
#include "ws2812b_text_renderer.h"
#include "rtc.h"
#include <string.h>
#include <stdio.h>

extern volatile AudioSource_t audioSource;
extern volatile uint8_t i2sUseEq;
extern volatile uint8_t agcRun;
extern volatile uint8_t agcOff;

extern I2C_HandleTypeDef hi2c2;
extern DMA_HandleTypeDef hdma_i2c2_tx;

RTC_TimeTypeDef s_time;
RTC_DateTypeDef s_date;

static const char *lcdWeekText[7] = {
	"MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY",
	"FRIDAY", "SATURDAY", "SUNDAY"
};

static uint8_t firstView = 1;	// EQ 초기 실행 표시

#define LCD_BUFFER_SIZE		(SSD1306_WIDTH * SSD1306_HEIGHT / 8)
#define LCD_EQ_HOLD_MS		2000

typedef enum
{
	LCD_SCREEN_MAIN = 0,
	LCD_SCREEN_EQ
} LCD_Screen_t;

static uint8_t lcd_buffer[1 + LCD_BUFFER_SIZE];
static volatile uint8_t lcd_dma_busy = 0;

static LCD_Screen_t lcd_screen = LCD_SCREEN_MAIN;
static uint32_t lcd_eq_hold_tick = 0;


static uint8_t eq_value[3];
static uint8_t eq_value_prev[3];

static HAL_StatusTypeDef WriteCommand(uint8_t command)
{
    return HAL_I2C_Mem_Write(
    		&hi2c2,
    		SSD1306_I2C_ADDR,
			0x00,
			I2C_MEMADD_SIZE_8BIT,
			&command,
			1,
			10);
}

static void LCD_ClearBuffer(void)
{
	lcd_buffer[0] = 0x40;
	memset(&lcd_buffer[1], 0x00, LCD_BUFFER_SIZE);
}

static void LCD_DrawPixel(uint8_t x, uint8_t y, uint8_t on)
{
	if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT)
	{
		return;
	}

	uint16_t index = 1 + x + (y / 8) * SSD1306_WIDTH;
	uint8_t bit = 1 << (y % 8);

	if (on)
	{
		lcd_buffer[index] |= bit;
	}
	else
	{
		lcd_buffer[index] &= ~bit;
	}
}

static void LCD_WriteChar(uint8_t x, uint8_t y, char ch, FontDef font, uint8_t on)
{
	if (ch < 32 || ch > 126)
	{
		ch = '?';
	}

	for (uint8_t row = 0; row < font.FontHeight; row++)
	{
		uint16_t line = font.data[(ch - 32) * font.FontHeight + row];

		for (uint8_t col = 0; col < font.FontWidth; col++)
		{
			if ((line << col) & 0x8000)
			{
				LCD_DrawPixel(x + col, y + row, on);
			}
		}
	}
}

static void LCD_DrawGauge(uint8_t x, uint8_t y, uint8_t w, uint8_t h, int value, int min, int max)
{
	if (value < min)
	{
		value = min;
	}

	if (value > max)
	{
		value = max;
	}

	uint8_t fill = (uint8_t)((value - min) * (w - 4) / (max - min));

	for (uint8_t px = 0; px < w; px++)
	{
		if (px == w / 2)
			continue;

		LCD_DrawPixel(x + px, y, 1);
		LCD_DrawPixel(x + px, y + h - 1, 1);
	}

	for (uint8_t py = 0; py < h; py++)
	{
		LCD_DrawPixel(x, y + py, 1);
		LCD_DrawPixel(x + w - 1, y + py, 1);
	}

	for (uint8_t py = 2; py < h - 2; py++)
	{
		for (uint8_t px = 0; px < fill; px++)
		{
			LCD_DrawPixel(x + 2 + px, y + py, 1);
		}
	}
}

static void LCD_WriteString(uint8_t x, uint8_t y, const char *str, FontDef font, uint8_t on)
{
	while (*str)
	{
		LCD_WriteChar(x, y, *str, font, on);
		x += font.FontWidth;

		if (x + font.FontWidth >= SSD1306_WIDTH)
		{
			break;
		}

		str++;
	}
}

static HAL_StatusTypeDef LCD_RefreshDMA(void)
{
	HAL_StatusTypeDef ret;

	if (lcd_dma_busy)
	{
		return HAL_BUSY;
	}

	lcd_dma_busy = 1;

	ret = HAL_I2C_Master_Transmit_DMA(
		&hi2c2,
		SSD1306_I2C_ADDR,
		lcd_buffer,
		sizeof(lcd_buffer));

	if (ret != HAL_OK)
	{
		lcd_dma_busy = 0;
		return ret;
	}

	return HAL_OK;
}

void LCD_AppModeInit(void)
{
	WriteCommand(0x20);	// 메모리 어드레싱 설정 진입
	WriteCommand(0x00);	// 수평 어드레싱 모드

	LCD_ClearBuffer();

	WriteCommand(0x21);	// 열 주소 설정 진입
	WriteCommand(0x00);	// 열 시작 0
	WriteCommand(0x7F);	// 열 끝  127

	WriteCommand(0x22);	// 페이지 주소 설정 진입
	WriteCommand(0x00);	// 페이지 시작 0
	WriteCommand(0x07);	// 페이지 끝  7

	LCD_RefreshDMA();
}

static uint8_t EqValueChanged(const uint8_t *now, const uint8_t *prev, uint8_t size)
{
	for (uint8_t i = 0; i < size; i++)
	{
		int diff = (int)now[i] - (int)prev[i];

		if (diff < 0)
		{
			diff = -diff;
		}

		if (diff >= 3)
		{
			return 1;
		}
	}

	return 0;
}

static void LCD_DrawEQScreen(void)
{
	LCD_ClearBuffer();

	// 노랑/파랑 영역 경계선
	for (uint8_t x = 0; x < SSD1306_WIDTH; x++)
	{
		LCD_DrawPixel(x, 15, 1);
	}

	LCD_WriteString(29, 3, "EQ SETTING", Font_7x10, 1);

	LCD_DrawGauge(28, 28, 100, 10, (int)eq_value[0], 0, 100);	LCD_WriteString(0, 30, "LOW", Font_7x10, 1);
	LCD_DrawGauge(28, 40, 100, 10, (int)eq_value[1], 0, 100);	LCD_WriteString(0, 42, "MID", Font_7x10, 1);
	LCD_DrawGauge(28, 52, 100, 10, (int)eq_value[2], 0, 100);	LCD_WriteString(0, 54, "HIG", Font_7x10, 1);

	LCD_RefreshDMA();
}

static void LCD_WriteStringCenter(uint8_t y, const char *str, FontDef font, uint8_t on)
{
	uint8_t len = strlen(str);
	uint8_t w = len * font.FontWidth;
	uint8_t x = 0;

	if (w < SSD1306_WIDTH)
		x = (SSD1306_WIDTH - w) / 2;

	LCD_WriteString(x, y, str, font, on);
}

void LCD_DrawMainScreen(void)
{
	if (lcd_dma_busy)
	{
		return;
	}

	if (audioSource == MORE_MOD_CLOCK)
	{
		RTC_GetTimeStruct(&s_time);
		RTC_GetDateStruct(&s_date);

		LCD_ClearBuffer();

		LCD_WriteString(29, 3, "CLOCK MODE", Font_7x10, 1);

		// 노랑/파랑 영역 경계선
		for (uint8_t x = 0; x < SSD1306_WIDTH; x++)
		{
			LCD_DrawPixel(x, 15, 1);
		}

		char timeText[8];
		char dateText[14];
		const char *week = "UNKNOWN";

		snprintf(timeText, sizeof(timeText),
				"%02d:%02d",
				s_time.Hours,
				s_time.Minutes);

		snprintf(dateText, sizeof(dateText),
				"20%02d-%02d-%02d",
				s_date.Year,
				s_date.Month,
				s_date.Date);

		week = lcdWeekText[s_date.WeekDay - 1];

		if (RTC_ShouldIBlink())
		{
			switch (RTC_WhatAreYouModifying())
			{
				case RTC_HOUR:
					timeText[0] = ' ';
					timeText[1] = ' ';
					break;
				case RTC_MINUTE:
					timeText[3] = ' ';
					timeText[4] = ' ';
					break;
				case RTC_YEAR:
					dateText[0] = ' ';
					dateText[1] = ' ';
					dateText[2] = ' ';
					dateText[3] = ' ';
					break;
				case RTC_MONTH:
					dateText[5] = ' ';
					dateText[6] = ' ';
					break;
				case RTC_DATE:
					dateText[8] = ' ';
					dateText[9] = ' ';
					break;
				default:
					break;
			}
		}

		LCD_WriteString(46, 19, timeText, Font_7x10, 1);
		LCD_WriteString(29, 34, dateText, Font_7x10, 1);
		LCD_WriteStringCenter(49, week, Font_7x10, 1);
	}
	else if (audioSource == MORE_MOD_TEXT)
	{
		LCD_ClearBuffer();

		LCD_WriteString(32, 3, "TEXT MODE", Font_7x10, 1);

		// 노랑/파랑 영역 경계선
		for (uint8_t x = 0; x < SSD1306_WIDTH; x++)
		{
			LCD_DrawPixel(x, 15, 1);
		}

		char lenText[8];
		snprintf(lenText, sizeof(lenText), "%d/50", TextRenderer_TextLen());
		LCD_WriteString(2, 19, "CHARS:", Font_7x10, 1);
		LCD_WriteString(44, 19, lenText, Font_7x10, 1);

		LCD_WriteString(2, 34, "SHIFT:", Font_7x10, 1);
		LCD_WriteString(44, 34, TextRenderer_IsScrolling() ? "ON" : "OFF", Font_7x10, 1);

		if (TextRenderer_IsScrolling())
		{
			char speedText[9];
			snprintf(speedText, sizeof(speedText), "%d/10", TextRenderer_WhatIsSpeed());
			LCD_WriteString(2, 49, "SPEED:", Font_7x10, 1);
			LCD_WriteString(44, 49, speedText, Font_7x10, 1);
		}
	}
	else
	{
		ADC_GetValue_EQ(eq_value);

		if (firstView || EqValueChanged(eq_value, eq_value_prev, sizeof(eq_value)))
		{
			firstView = 0;

			memcpy(eq_value_prev, eq_value, sizeof(eq_value));

			lcd_screen = LCD_SCREEN_EQ;
			lcd_eq_hold_tick = HAL_GetTick();

			LCD_DrawEQScreen();
			return;
		}

		if (lcd_screen == LCD_SCREEN_EQ)
		{
			if ((HAL_GetTick() - lcd_eq_hold_tick) < LCD_EQ_HOLD_MS)
			{
				LCD_DrawEQScreen();
				return;
			}

			lcd_screen = LCD_SCREEN_MAIN;
		}

		char *input_mode;
		switch (audioSource)
		{
			case AUDIO_SRC_USB:
				input_mode = "USB";
				break;
			case AUDIO_SRC_AUX:
				input_mode = "AUX";
				break;
			case AUDIO_SRC_BT:
				input_mode = " BT";
				break;
			default:
				input_mode = " ";
				break;
		}

		uint8_t db_min;
		ADC_GetValue_VOL(&db_min);

		LCD_ClearBuffer();

		// y = 0~15 노랑 영역
		LCD_WriteString(36, 3, input_mode, Font_7x10, 1);
		LCD_WriteString(64, 3, "MODE", Font_7x10, 1);

		// 노랑/파랑 영역 경계선
		for (uint8_t x = 0; x < SSD1306_WIDTH; x++)
		{
			LCD_DrawPixel(x, 15, 1);
		}

		// 상태 표시
		LCD_WriteString(2, 18, "EQ:", Font_7x10, 1);
		LCD_WriteString(23, 18, i2sUseEq ? "ON" : "OFF", Font_7x10, 1);

		if (!agcOff)
		{
			char agcText[8];
			snprintf(agcText, sizeof(agcText), "%.2f", AGC_GetGain());
			LCD_WriteString(62, 18, "AGC:", Font_7x10, 1);
			LCD_WriteString(90, 18, agcText, Font_7x10, 1);
		}

		// dB_min 게이지
		LCD_WriteString(19, 38, "LED RANGE(dB)", Font_7x10, 1);
		LCD_WriteString(0, 54, "-60", Font_7x10, 1);
		LCD_DrawGauge(28, 52, 72, 10, (int)db_min, 0, 100);
		LCD_WriteString(106, 54, "-20", Font_7x10, 1);
	}

	LCD_RefreshDMA();
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	if (hi2c->Instance == I2C2)
	{
		lcd_dma_busy = 0;
	}
}
