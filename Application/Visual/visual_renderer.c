/*
 * visual_renderer.c
 *
 *  Created on: 2026. 5. 18.
 *      Author: nugur
 */

#include "visual_theme.h"
#include "visual_renderer.h"

#include <string.h>
#include <math.h>

// ======================================================
// CURRENT STATE
// ======================================================
static VisualMode_t s_visualMode = VISUAL_MODE_SPECTRUM;

// global time
static float s_time = 0.0f;

// themes
uint8_t s_spectrumTheme = 0;
uint8_t s_mirrorTheme   = 0;

// ======================================================
// INTERNAL
// ======================================================
static inline void UpdateTime(float speed)
{
    s_time += speed;

    if (s_time > 1000.0f)
    {
        s_time = 0.0f;
    }
}

// ======================================================
// INIT / CLEAR
// ======================================================
void VisualRenderer_Init(void)
{
    VisualModes_Init();
}

void VisualRenderer_Clear(void)
{
    VisualModes_Clear();
}

// ======================================================
// MODE CONTROL
// ======================================================
void VisualRenderer_NextMode(void)
{
    s_visualMode++;

    if (s_visualMode >= VISUAL_MODE_COUNT)
        s_visualMode = 0;
}

void VisualRenderer_NextSpectrumTheme(void)
{
    s_spectrumTheme++;

    if (s_spectrumTheme >= SPECTRUM_THEME_COUNT)
        s_spectrumTheme = 0;
}

void VisualRenderer_NextMirrorTheme(void)
{
    s_mirrorTheme++;

    if (s_mirrorTheme >= MIRROR_THEME_COUNT)
        s_mirrorTheme = 0;
}

// ======================================================
// DRAW DISPATCH
// ======================================================
void VisualRenderer_Draw(const float *trail, const float *peakHold)
{
    switch (s_visualMode)
    {
        // ==================================================
        // SPECTRUM
        // ==================================================
        case VISUAL_MODE_SPECTRUM:
            switch (s_spectrumTheme)
            {
                case 0: VisualModes_DrawSpectrum1(trail, peakHold); break;
                case 1: VisualModes_DrawSpectrum2(trail, peakHold); break;
                case 2: VisualModes_DrawSpectrum3(trail, peakHold); break;
                case 3: VisualModes_DrawSpectrum4(trail, peakHold); break;
                case 4: VisualModes_DrawSpectrum5(trail, peakHold); break;
                case 5: VisualModes_DrawSpectrum6(trail, peakHold); break;
                default: VisualModes_DrawSpectrum1(trail, peakHold); break;
            }
            break;

        // ==================================================
        // MIRROR
        // ==================================================
        case VISUAL_MODE_MIRROR_FULL:
            VisualModes_DrawMirror_Full(trail);
            break;

        case VISUAL_MODE_MIRROR_CENTER:
            VisualModes_DrawMirror_Center(trail);
            break;

        // ==================================================
        // SIMPLE MODES
        // ==================================================
        case VISUAL_MODE_RAINBOW:
            VisualModes_DrawRainbow(trail);
            break;

        case VISUAL_MODE_PULSE:
            VisualModes_DrawPulse(trail);
            break;

        // ==================================================
        // ENERGY MODES
        // ==================================================
        case VISUAL_MODE_SPARK_NOISE:
            UpdateTime(0.016f);
            VisualModes_DrawSparkNoise(trail, s_time);
            break;

        case VISUAL_MODE_GLITCH_GRID:
            UpdateTime(0.016f);
            VisualModes_DrawGlitchGrid(trail, s_time);
            break;

        case VISUAL_MODE_GRID_BREATH:
            UpdateTime(0.016f);
            VisualModes_DrawGridBreath(trail, s_time);
            break;

        // ==================================================
        // MOTION MODES
        // ==================================================
        case VISUAL_MODE_ROTATING_FIELD:
            UpdateTime(0.016f);
            VisualModes_DrawRotatingField(trail, s_time);
            break;

        case VISUAL_MODE_PLASMA_MODE:
            UpdateTime(0.016f);
            VisualModes_DrawPlasma(trail, s_time);
            break;

        case VISUAL_MODE_MULTI_ORBIT:
            UpdateTime(0.020f);
            VisualModes_DrawMultiOrbit(trail, s_time);
            break;

        case VISUAL_MODE_HEX:
            UpdateTime(0.018f);
            VisualModes_DrawHexGrid(trail, s_time);
            break;

        case VISUAL_MODE_LASER:
            UpdateTime(0.030f);
            VisualModes_DrawLaserScan(trail, s_time);
            break;

        // ==================================================
        // FLOW MODES
        // ==================================================
        case VISUAL_MODE_WATERUP:
            VisualModes_DrawWaterup(trail);
            break;

        case VISUAL_MODE_SHOCKWAVE:
            UpdateTime(0.020f);
            VisualModes_DrawShockwave(trail, s_time);
            break;

        case VISUAL_MODE_NORTHERN_LIGHTS:
            UpdateTime(0.010f);
            VisualModes_DrawNorthernLights(trail, s_time);
            break;

        // ==================================================
        // DEFAULT
        // ==================================================
        default:

            VisualModes_DrawSpectrum1(trail, peakHold);

            break;
    }
}

// ======================================================
// GET FRAME BUFFER
// ======================================================
const uint8_t *VisualRenderer_GetFrame(void)
{
    return VisualModes_GetFrame();
}
