/*
 * visual_modes.h
 *
 *  Created on: 2026. 5. 21.
 *      Author: nugur
 */

#ifndef PERIPH_TIM_VISUAL_MODES_H_
#define PERIPH_TIM_VISUAL_MODES_H_

#include <stdint.h>
#include "visual_renderer.h"

#define MATRIX_WIDTH 16U
#define MATRIX_HEIGHT 16U

#define LED_ON_R  0x90 // 0x80
#define LED_ON_G  0x70 // 0x68
#define LED_ON_B  0x90 // 0x80
// ======================================================
// INIT / CLEAR
// ======================================================
void VisualModes_Init(void);
void VisualModes_Clear(void);

// ======================================================
// SPECTRUM THEMES
// ======================================================
void VisualModes_DrawSpectrum1(const float *trail, const float *peakHold);
void VisualModes_DrawSpectrum2(const float *trail, const float *peakHold);
void VisualModes_DrawSpectrum3(const float *trail, const float *peakHold);
void VisualModes_DrawSpectrum4(const float *trail, const float *peakHold);
void VisualModes_DrawSpectrum5(const float *trail, const float *peakHold);
void VisualModes_DrawSpectrum6(const float *trail, const float *peakHold);

// ======================================================
// CLASSIC ANALYZER MODES
// ======================================================
void VisualModes_DrawMirror_Full(const float *trail);
void VisualModes_DrawMirror_Center(const float *trail);

void VisualModes_DrawRainbow(const float *trail);
void VisualModes_DrawPulse(const float *trail);

// ======================================================
// REACTIVE / ENERGY MODES
// ======================================================
void VisualModes_DrawSparkNoise(const float *trail, float time);
void VisualModes_DrawGlitchGrid(const float *trail, float time);
void VisualModes_DrawGridBreath(const float *trail, float time);

// ======================================================
// SPACE / MOTION MODES
// ======================================================
void VisualModes_DrawRotatingField(const float *trail, float time);
void VisualModes_DrawPlasma(const float *trail, float time);
void VisualModes_DrawMultiOrbit(const float *trail, float time);
void VisualModes_DrawHexGrid(const float *trail, float time);
void VisualModes_DrawLaserScan(const float *trail, float time);

// ======================================================
// FLOW / DEPTH MODES
// ======================================================
void VisualModes_DrawWaterup(const float *trail);
void VisualModes_DrawShockwave(const float *trail, float time);
void VisualModes_DrawNorthernLights(const float *trail, float time);

// ======================================================
// GET FRAME
// ======================================================
const uint8_t *VisualModes_GetFrame(void);

#endif /* PERIPH_TIM_VISUAL_MODES_H_ */
