/*
 * ws2812b_text_renderer.h
 *
 *  Created on: 2026. 6. 9.
 *      Author: ADJ
 */

#ifndef WS2812B_TEXT_RENDERER_H_
#define WS2812B_TEXT_RENDERER_H_

#include <stdint.h>

const uint8_t* TextRenderer_Render(const uint8_t (*text)[4], uint8_t spacing);
uint8_t TextRenderer_TextLen(void);
uint8_t TextRenderer_IsScrolling(void);
void TextRenderer_SpeedUp(void);
void TextRenderer_SpeedDown(void);
uint16_t TextRenderer_WhatIsSpeed(void);

#endif /* WS2812B_TEXT_RENDERER_H_ */
