/*
 * ws2812b_text_renderer.c
 *
 *  Created on: 2026. 6. 9.
 *      Author: ADJ
 */

#include "ws2812b_text_renderer.h"

#include "stm32f7xx_hal.h"

#include <string.h>

#define TEXT_MAX_LEN				50
#define TEXT_SCROLL_INTERVAL_MS		200

static uint16_t s_scrollSpeed =  TEXT_SCROLL_INTERVAL_MS;

static uint8_t s_frame[16 * 16 * 3];					// WS2812B_Show() 프레임

static const uint16_t *s_glyphCols[TEXT_MAX_LEN];		// 폰트의 데이터 주소
static int16_t s_glyphX[TEXT_MAX_LEN];					// 글자의 시작 x좌표
static uint8_t s_glyphW[TEXT_MAX_LEN];					// 글자의 폭
static uint8_t s_glyphCount;							// 글자 수

static uint16_t s_textWidth;							// 전체 문자열 폭
static int16_t s_textOffsetX;
static uint32_t s_lastTick;

static uint8_t s_lastText[TEXT_MAX_LEN][4];
static uint8_t s_lastSpacing;

static const uint16_t s_digitFont[10][3] =
{
	{ 0x7FFE, 0x6006, 0x7FFE },	// 0
	{ 0x6018, 0x7FFE, 0x6000 },	// 1
	{ 0x7F86, 0x6186, 0x61FE },	// 2
	{ 0x6186, 0x6186, 0x7FFE },	// 3
	{ 0x01FE, 0x0180, 0x7FFE },	// 4
	{ 0x61FE, 0x6186, 0x7F86 },	// 5
	{ 0x7FFE, 0x6186, 0x7F86 },	// 6
	{ 0x0006, 0x0006, 0x7FFE },	// 7
	{ 0x7FFE, 0x6186, 0x7FFE },	// 8
	{ 0x01FE, 0x0186, 0x7FFE },	// 9
};

static const uint16_t s_alphaFont[26][6] =
{
	{ 4, 0x7FF8, 0x00C6, 0x00C6, 0x7FF8, 0x0000 },	// A
	{ 4, 0x7FFE, 0x60C6, 0x60C6, 0x1F38, 0x0000 },	// B
	{ 4, 0x3FFC, 0x6006, 0x6006, 0x381C, 0x0000 },	// C
	{ 4, 0x7FFE, 0x6006, 0x6006, 0x1FF8, 0x0000 },	// D
	{ 3, 0x7FFE, 0x6186, 0x6186, 0x0000, 0x0000 },	// E
	{ 3, 0x7FFE, 0x0186, 0x0186, 0x0000, 0x0000 },	// F
	{ 4, 0x3FFC, 0x6006, 0x6306, 0x7F1C, 0x0000 },	// G
	{ 4, 0x7FFE, 0x0180, 0x0180, 0x7FFE, 0x0000 },	// H
	{ 3, 0x6006, 0x7FFE, 0x6006, 0x0000, 0x0000 },	// I
	{ 3, 0x3C00, 0x6000, 0x3FFE, 0x0000, 0x0000 },	// J
	{ 4, 0x7FFE, 0x03C0, 0x0E70, 0x781E, 0x0000 },	// K
	{ 3, 0x7FFE, 0x6000, 0x6000, 0x0000, 0x0000 },	// L
	{ 5, 0x7FFE, 0x0038, 0x00E0, 0x0038, 0x7FFE },	// M
	{ 5, 0x7FFE, 0x0078, 0x03C0, 0x1E00, 0x7FFE },	// N
	{ 4, 0x1FF8, 0x6006, 0x6006, 0x1FF8, 0x0000 },	// O
	{ 3, 0x7FFE, 0x0186, 0x00FC, 0x0000, 0x0000 },	// P
	{ 5, 0x1FF8, 0x6006, 0x6606, 0x1FF8, 0x6000 },	// Q
	{ 3, 0x7FFE, 0x0186, 0x7EFC, 0x0000, 0x0000 },	// R
	{ 3, 0x30FC, 0x6186, 0x3F0C, 0x0000, 0x0000 },	// S
	{ 3, 0x0006, 0x7FFE, 0x0006, 0x0000, 0x0000 },	// T
	{ 4, 0x1FFE, 0x6000, 0x6000, 0x1FFE, 0x0000 },	// U
	{ 5, 0x07FE, 0x1800, 0x7000, 0x1800, 0x07FE },	// V
	{ 5, 0x7FFE, 0x1C00, 0x0700, 0x1C00, 0x7FFE },	// W
	{ 5, 0x781E, 0x0F78, 0x03E0, 0x0F78, 0x781E },	// X
	{ 3, 0x007E, 0x7F80, 0x007E, 0x0000, 0x0000 },	// Y
	{ 5, 0x7006, 0x6E06, 0x6186, 0x6076, 0x600E },	// Z
};

// 소문자 대문자로 변환
static char TextRenderer_ToUpper(char ch)
{
	if ((ch >= 'a') && (ch <= 'z'))
	{
		return (char)(ch - ('a' - 'A'));
	}

	return ch;
}


// 글자 비트맵 그리기
static void TextRenderer_BuildGlyphs(const uint8_t (*text)[4], uint8_t spacing)
{
	uint16_t cursorX = 0;

	s_glyphCount = 0;
	s_textWidth = 0;

	if (text == 0)
	{
		return;
	}

	// 최대 TEXT_MAX_LEN 글자까지만 처리
	// 문자열 끝 '\0'을 만나면 종료
	for (uint8_t i = 0; (i < TEXT_MAX_LEN) && (text[i][0] != '\0'); i++)
	{
		char ch = TextRenderer_ToUpper((char)text[i][0]);	// 대문자 변환

		s_glyphCols[s_glyphCount] = 0;
		s_glyphX[s_glyphCount] = cursorX;
		s_glyphW[s_glyphCount] = 0;

		if ((ch >= '0') && (ch <= '9'))			// 숫자
		{
			s_glyphCols[s_glyphCount] = s_digitFont[ch - '0'];
			s_glyphW[s_glyphCount] = 3;
		}
		else if ((ch >= 'A') && (ch <= 'Z'))	// 대문자
		{
			s_glyphCols[s_glyphCount] = &s_alphaFont[ch - 'A'][1];
			s_glyphW[s_glyphCount] = (uint8_t)s_alphaFont[ch - 'A'][0];
		}
		else if (ch == ' ')
		{
			s_glyphCols[s_glyphCount] = 0;
			s_glyphW[s_glyphCount] = spacing ? 2 : 1;
		}
		else
		{
			continue;
		}

		if (spacing && (text[i + 1][0] != '\0'))
			cursorX += s_glyphW[s_glyphCount] + 1;
		else
			cursorX += s_glyphW[s_glyphCount];

		s_glyphCount++;
	}

	s_textWidth = cursorX;

	if (s_textWidth > 16u)
	{
		s_textOffsetX = 16;
	}
	else
	{
		s_textOffsetX = (16 - s_textWidth) / 2;
	}

	s_lastTick = HAL_GetTick();
}

// 불 켜기
static void TextRenderer_SetPixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b)
{
	uint32_t base;

	if ((x >= 16) || (y >= 16))
	{
		return;
	}

	y = 15 - y;

	base = (((uint32_t)y * 16u) + (uint32_t)x) * 3u;

	s_frame[base + 0u] = r;
	s_frame[base + 1u] = g;
	s_frame[base + 2u] = b;
}

// index: 글자 번호
// screenX: 화면 좌표
static void TextRenderer_DrawGlyph(uint8_t index, int16_t screenX, uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t x;
	uint8_t y;
	uint16_t col;

	// 스페이스 문자 종료
	if (s_glyphCols[index] == 0)
	{
		return;
	}

	// 글자의 세로줄을 왼쪽부터 읽기
	for (x = 0; x < s_glyphW[index]; x++)
	{
		// 화면 왼쪽 넘었는지 검사
		if ((screenX + (int16_t)x) < 0)
		{
			continue;
		}

		// 화면 오른쪽 넘었는지 검사
		if ((screenX + (int16_t)x) >= 16)
		{
			break;
		}

		col = s_glyphCols[index][x];

		for (y = 0; y < 16; y++)
		{
			if (((col >> y) & 0x0001u) != 0u)
			{
				TextRenderer_SetPixel((uint8_t)(screenX + (int16_t)x), y, r, g, b);
			}
		}
	}
}

// 윈도우 스크롤 로직
static void TextRenderer_UpdateScroll(void)
{
	uint32_t now;

	if (s_textWidth <= 16u)
	{
		return;
	}

	now = HAL_GetTick();

	if ((now - s_lastTick) < s_scrollSpeed)
	{
		return;
	}

	s_lastTick = now;

	s_textOffsetX--;

	if (s_textOffsetX <= -(int16_t)s_textWidth)
	{
		s_textOffsetX = 16;
	}
}

const uint8_t* TextRenderer_Render(const uint8_t (*text)[4], uint8_t spacing)
{
	int16_t offsetX;
	int16_t screenX;

	if (text == 0)
	{
		memset(s_frame, 0, sizeof(s_frame));
		return s_frame;
	}

	if (memcmp(s_lastText, text, sizeof(s_lastText)) != 0 || s_lastSpacing != spacing)
	{
		memcpy(s_lastText, text, sizeof(s_lastText));
		s_lastSpacing = spacing;
		TextRenderer_BuildGlyphs(text, spacing);
	}

	TextRenderer_UpdateScroll();

	memset(s_frame, 0, sizeof(s_frame));

	offsetX = s_textOffsetX;

	for (uint8_t i = 0; i < s_glyphCount; i++)
	{
		screenX = s_glyphX[i] + offsetX;

		if ((screenX + (int16_t)s_glyphW[i]) <= 0)
		{
			continue;
		}

		if (screenX >= 16)
		{
			break;
		}

		TextRenderer_DrawGlyph(i, screenX, text[i][1], text[i][2], text[i][3]);
	}

	return s_frame;
}

void TextRenderer_SpeedUp(void)
{
	if (s_scrollSpeed > 100)
		s_scrollSpeed -= 20;
}

void TextRenderer_SpeedDown(void)
{
	if (s_scrollSpeed < 300)
		s_scrollSpeed += 20;
}

uint8_t TextRenderer_TextLen(void)
{
	return s_glyphCount;
}

uint8_t TextRenderer_IsScrolling(void)
{
	return s_textWidth > 16;
}

uint16_t TextRenderer_WhatIsSpeed(void)
{
	return 10 - ((s_scrollSpeed - 100) / 20);
}
