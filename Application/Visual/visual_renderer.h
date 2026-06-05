/*
 * visual_renderer.h
 *
 *  Created on: 2026. 5. 18.
 *      Author: nugur
 */

#ifndef PERIPH_TIM_VISUAL_RENDERER_H_
#define PERIPH_TIM_VISUAL_RENDERER_H_

#include <stdint.h>
#include "visual_modes.h"

// ======================================================
// VISUAL MODE
// ======================================================

typedef enum
{
    // ==================================================
    // CLASSIC ANALYZER
    // ==================================================
    VISUAL_MODE_SPECTRUM = 0,

    VISUAL_MODE_MIRROR_FULL,
    VISUAL_MODE_MIRROR_CENTER,

    VISUAL_MODE_RAINBOW,
    VISUAL_MODE_PULSE,

    // ==================================================
    // REACTIVE / ENERGY
    // ==================================================
    VISUAL_MODE_SPARK_NOISE,

    VISUAL_MODE_GLITCH_GRID,

    VISUAL_MODE_GRID_BREATH,

    // ==================================================
    // SPACE / MOTION
    // ==================================================
    VISUAL_MODE_ROTATING_FIELD,

    VISUAL_MODE_PLASMA_MODE,

    VISUAL_MODE_MULTI_ORBIT,

    VISUAL_MODE_HEX,

    VISUAL_MODE_LASER,

    // ==================================================
    // FLOW / DEPTH
    // ==================================================
    VISUAL_MODE_WATERUP,

    VISUAL_MODE_SHOCKWAVE,

    VISUAL_MODE_NORTHERN_LIGHTS,

    // ==================================================
    // COUNT
    // ==================================================
    VISUAL_MODE_COUNT

} VisualMode_t;

// ======================================================
// API
// ======================================================
extern uint8_t s_spectrumTheme;
extern uint8_t s_mirrorTheme;

void VisualRenderer_Init(void);

void VisualRenderer_Clear(void);

void VisualRenderer_Draw(const float *trail,const float *peakHold);

void VisualRenderer_NextMode(void);

void VisualRenderer_NextSpectrumTheme(void);
void VisualRenderer_NextMirrorTheme(void);

const uint8_t *VisualRenderer_GetFrame(void);

#endif /* PERIPH_TIM_VISUAL_RENDERER_H_ */
