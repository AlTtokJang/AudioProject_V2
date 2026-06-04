/*
 * visual_modes.c
 *
 *  Created on: 2026. 5. 21.
 *      Author: nugur
 */

#include "visual_renderer.h"
#include "visual_modes.h"
#include "visual_theme.h"
#include <string.h>
#include <math.h>

// ======================================================
// FRAME BUFFER
// ======================================================
static uint8_t s_frame[MATRIX_HEIGHT][MATRIX_WIDTH][3];
#define MODE_BRIGHTNESS_GAIN  1.25f
static uint8_t s_gammaTable[256];

static inline uint8_t Gamma(float v)
{
	v *= MODE_BRIGHTNESS_GAIN;

    if (v < 0.0f)
        v = 0.0f;
    else if (v > 255.0f)
        v = 255.0f;

    return s_gammaTable[(uint8_t)v];
}
// ======================================================
// GAMMA
// ======================================================
static void BuildGamma(void)
{
	for (int i = 0; i < 256; i++)
    {
		float x = (float)i / 255.0f;
		s_gammaTable[i] = powf(x, 2.1f) * 240;
    }
}

// ======================================================
// INIT / CLEAR
// ======================================================

void VisualModes_Init(void)
{
    memset(s_frame, 0, sizeof(s_frame));
    BuildGamma();
}

void VisualModes_Clear(void)
{
    memset(s_frame, 0, sizeof(s_frame));
}

// ======================================================
// SPECTRUM CORE
// ======================================================
static void DrawSpectrumTheme(
    uint8_t theme,
    const float *trail,
    const float *peakHold)
{
    memset(s_frame, 0, sizeof(s_frame));

    for (int x = 0; x < MATRIX_WIDTH; x++)
    {
        // Bar height
        int h = (int)(trail[x] + 0.5f);

        if (h < 0) h = 0;
        if (h > MATRIX_HEIGHT) h = MATRIX_HEIGHT;

        // Draw spectrum bar
        for (int y = 0; y < h; y++)
        {
            float t =
                (float)y /
                (float)(MATRIX_HEIGHT - 1);

            float r, g, b;

            VisualTheme_GetColor(
                theme,
                t,
                &r, &g, &b);

            s_frame[y][x][0] = Gamma(r);
            s_frame[y][x][1] = Gamma(g);
            s_frame[y][x][2] = Gamma(b);
        }

        // Peak hold
        int p = (int)(peakHold[x] + 0.5f);

        if (p >= 0 && p < MATRIX_HEIGHT)
        {
            float pr, pg, pb;

            VisualTheme_GetPeakColor(
                theme,
                &pr, &pg, &pb);

            s_frame[p][x][0] = Gamma(pr);
            s_frame[p][x][1] = Gamma(pg);
            s_frame[p][x][2] = Gamma(pb);
        }
    }
}

// ======================================================
// SPECTRUM THEMES
// ======================================================

void VisualModes_DrawSpectrum1(const float *trail, const float *peakHold)
{
    DrawSpectrumTheme(0, trail, peakHold);
}

void VisualModes_DrawSpectrum2(const float *trail, const float *peakHold)
{
    DrawSpectrumTheme(1, trail, peakHold);
}

void VisualModes_DrawSpectrum3(const float *trail, const float *peakHold)
{
    DrawSpectrumTheme(2, trail, peakHold);
}

void VisualModes_DrawSpectrum4(const float *trail, const float *peakHold)
{
    DrawSpectrumTheme(3, trail, peakHold);
}

void VisualModes_DrawSpectrum5(const float *trail, const float *peakHold)
{
    DrawSpectrumTheme(4, trail, peakHold);
}

void VisualModes_DrawSpectrum6(const float *trail, const float *peakHold)
{
    DrawSpectrumTheme(5, trail, peakHold);
}

// ==================================================
// FULL MIRROR
// ==================================================
void VisualModes_DrawMirror_Full(const float *trail)
{
    memset(s_frame, 0, sizeof(s_frame));

    for (int x = 0; x < MATRIX_WIDTH; x++)
    {
        int h = (int)(trail[x] + 0.5f);

        if (h > MATRIX_HEIGHT) h = MATRIX_HEIGHT;
        if (h < 0) h = 0;

        float r, g, b;

        VisualTheme_GetMirrorColor(
            s_mirrorTheme,
            1.0f,
            &r,
            &g,
            &b);

        uint8_t cr = Gamma(r);
        uint8_t cg = Gamma(g);
        uint8_t cb = Gamma(b);

        for (int y = 0; y < h; y++)
        {
            int top    = y;
            int bottom = (MATRIX_HEIGHT - 1) - y;

            s_frame[top][x][0] = cr;
            s_frame[top][x][1] = cg;
            s_frame[top][x][2] = cb;

            s_frame[bottom][x][0] = cr;
            s_frame[bottom][x][1] = cg;
            s_frame[bottom][x][2] = cb;
        }
    }
}

// ==================================================
// CENTER MIRROR
// ==================================================
void VisualModes_DrawMirror_Center(const float *trail)
{
    memset(s_frame, 0, sizeof(s_frame));

    const int halfH = MATRIX_HEIGHT / 2;

    float r, g, b;

    VisualTheme_GetMirrorColor(
        s_mirrorTheme,
        1.0f,
        &r,
        &g,
        &b);

    uint8_t cr = Gamma(r);
    uint8_t cg = Gamma(g);
    uint8_t cb = Gamma(b);

    for (int x = 0; x < MATRIX_WIDTH; x++)
    {
        int h = (int)(trail[x] + 0.5f);

        if (h > halfH) h = halfH;
        if (h < 0) h = 0;

        for (int y = 0; y < h; y++)
        {
            int y0 = halfH - 1 - y;
            int y1 = halfH + y;

            if (y0 >= 0)
            {
                s_frame[y0][x][0] = cr;
                s_frame[y0][x][1] = cg;
                s_frame[y0][x][2] = cb;
            }

            if (y1 < MATRIX_HEIGHT)
            {
                s_frame[y1][x][0] = cr;
                s_frame[y1][x][1] = cg;
                s_frame[y1][x][2] = cb;
            }
        }
    }
}

// ======================================================
// RAINBOW
// ======================================================
void VisualModes_DrawRainbow(const float *trail)
{
    memset(s_frame, 0, sizeof(s_frame));

    static float hue = 0.0f;

    hue += 0.02f;

    for (int x = 0; x < MATRIX_WIDTH; x++)
    {
        int h = (int)(trail[x] + 0.5f);

        if (h < 0)
            h = 0;

        if (h > MATRIX_HEIGHT)
            h = MATRIX_HEIGHT;

        float wave =
            sinf(
                x * 0.25f +
                hue * 3.0f);

        for (int y = 0; y < h; y++)
        {
            float t =
                fmodf(
                    hue +
                    x * 0.06f +
                    wave * 0.15f +
                    y * 0.03f,
                    1.0f);

            float r, g, b;

            VisualTheme_GetColor(
                s_spectrumTheme,
                t,
                &r,
                &g,
                &b);

            s_frame[y][x][0] = Gamma(r);
            s_frame[y][x][1] = Gamma(g);
            s_frame[y][x][2] = Gamma(b);
        }
    }
}

// ======================================================
// PULSE
// ======================================================
void VisualModes_DrawPulse(const float *trail)
{
    memset(s_frame, 0, sizeof(s_frame));

    static float t_global = 0.0f;

    // =========================================
    // AUDIO ENERGY
    // =========================================
    float energy = 0.0f;

    for (int i = 0; i < MATRIX_WIDTH; i++)
        energy += trail[i];

    energy /= MATRIX_WIDTH;
    energy /= (float)MATRIX_HEIGHT;

    // =========================================
    // PULSE SPEED (FASTER)
    // =========================================
    float speed =
        0.035f +
        energy * 0.09f;

    t_global += speed;

    // =========================================
    // GLOBAL PULSE
    // =========================================
    float pulse =
        0.5f +
        0.5f *
        sinf(t_global);

    pulse = powf(pulse, 1.3f);

    // =========================================
    // DRAW
    // =========================================
    for (int x = 0; x < MATRIX_WIDTH; x++)
    {
        int h = (int)(trail[x] + 0.5f);

        if (h < 0)
            h = 0;

        if (h > MATRIX_HEIGHT)
            h = MATRIX_HEIGHT;

        // =====================================
        // WAVE MOTION
        // =====================================
        float wave =
            0.5f +
            0.5f *
            sinf(
                t_global * 0.4f +
                x * 0.15f);

        wave *= wave;

        // =====================================
        // AUDIO REACTIVE BRIGHTNESS
        // =====================================
        float brightness =
            (0.35f + pulse * 0.65f) *
            (0.75f + wave * 0.25f) *
            (0.7f + energy * 1.3f);

        for (int y = 0; y < h; y++)
        {
            float t =
                (float)y /
                (float)(MATRIX_HEIGHT - 1);

            float r, g, b;

            VisualTheme_GetColor(
                s_spectrumTheme,
                t,
                &r,
                &g,
                &b);

            // =================================
            // APPLY
            // =================================
            r *= brightness;
            g *= brightness;
            b *= brightness;

            s_frame[y][x][0] = Gamma(r);
            s_frame[y][x][1] = Gamma(g);
            s_frame[y][x][2] = Gamma(b);
        }
    }
}

// ======================================================
// SPARK_NOISE
// ======================================================
void VisualModes_DrawSparkNoise(
    const float *trail,
    float time)
{
    memset(s_frame, 0, sizeof(s_frame));

    float sparkPos =
        fmodf(
            time * 10.0f,
            MATRIX_WIDTH + 6.0f) - 3.0f;

    for (int x = 0; x < MATRIX_WIDTH; x++)
    {
        int height =
            (int)(trail[x] + 0.5f);

        if (height < 0)
            height = 0;

        if (height > MATRIX_HEIGHT)
            height = MATRIX_HEIGHT;

        for (int y = 0; y < height; y++)
        {
            float t =
                (float)y /
                (float)(MATRIX_HEIGHT - 1);

            float r,g,b;

            VisualTheme_GetColor(
                s_spectrumTheme,
                t,
                &r,&g,&b);

            // =====================================
            // BASE
            // =====================================
            float intensity = 0.85f;

            // =====================================
            // MOVING SPARK
            // =====================================
            float d =
                fabsf(
                    x - sparkPos);

            if(d < 2.0f)
            {
                intensity +=
                    (2.0f - d) * 1.8f;
            }

            // =====================================
            // RANDOM SPARK
            // =====================================
            float n =
                sinf(
                    x * 19.3f +
                    y * 71.7f +
                    time * 18.0f);

            n -= floorf(n);

            if(n > 0.985f)
            {
                intensity += 2.0f;
            }

            // =====================================
            // APPLY
            // =====================================
            r *= intensity;
            g *= intensity;
            b *= intensity;

            uint8_t rr = Gamma(r);
            uint8_t gg = Gamma(g);
            uint8_t bb = Gamma(b);

            s_frame[y][x][0] = rr;
            s_frame[y][x][1] = gg;
            s_frame[y][x][2] = bb;

            // =====================================
            // BLOOM
            // =====================================
            if(intensity > 2.0f)
            {
                if(y + 1 < MATRIX_HEIGHT)
                {
                    s_frame[y+1][x][0] = rr / 2;
                    s_frame[y+1][x][1] = gg / 2;
                    s_frame[y+1][x][2] = bb / 2;
                }
            }
        }
    }
}

// ======================================================
// GLITCH_GRID
// ======================================================
void VisualModes_DrawGlitchGrid(
    const float *trail,
    float time)
{
    memset(s_frame, 0, sizeof(s_frame));

    // =====================================
    // SCAN BAR
    // =====================================
    int glitchLine =
        (int)fmodf(
            time * 10.0f,
            MATRIX_HEIGHT);

    for (int x = 0; x < MATRIX_WIDTH; x++)
    {
        int h =
            (int)(trail[x] + 0.5f);

        if (h < 0)
            h = 0;

        if (h > MATRIX_HEIGHT)
            h = MATRIX_HEIGHT;

        for (int y = 0; y < h; y++)
        {
            float t =
                (float)y /
                (float)(MATRIX_HEIGHT - 1);

            float r, g, b;

            VisualTheme_GetColor(
                s_spectrumTheme,
                t,
                &r,
                &g,
                &b);

            float intensity = 0.75f;

            // =====================================
            // MOVING GLITCH BAR
            // =====================================
            int d =
                y - glitchLine;

            if (d < 0)
                d = -d;

            if (d <= 2)
            {
                intensity +=
                    (3 - d) * 1.2f;
            }

            // =====================================
            // RANDOM CORRUPTION
            // =====================================
            float n =
                sinf(
                    x * 37.1f +
                    y * 91.7f +
                    time * 25.0f);

            n =
                n -
                floorf(n);

            if (n > 0.992f)
            {
                intensity += 2.5f;
            }

            // =====================================
            // APPLY
            // =====================================
            r *= intensity;
            g *= intensity;
            b *= intensity;

            uint8_t rr = Gamma(r);
            uint8_t gg = Gamma(g);
            uint8_t bb = Gamma(b);

            s_frame[y][x][0] = rr;
            s_frame[y][x][1] = gg;
            s_frame[y][x][2] = bb;

            // =====================================
            // GLITCH SMEAR
            // =====================================
            if (n > 0.992f)
            {
                if (x + 1 < MATRIX_WIDTH)
                {
                    s_frame[y][x + 1][0] = rr;
                    s_frame[y][x + 1][1] = gg;
                    s_frame[y][x + 1][2] = bb;
                }

                if (x + 2 < MATRIX_WIDTH)
                {
                    s_frame[y][x + 2][0] = rr / 2;
                    s_frame[y][x + 2][1] = gg / 2;
                    s_frame[y][x + 2][2] = bb / 2;
                }
            }

            // =====================================
            // SCAN BAR GLOW
            // =====================================
            if (d <= 1)
            {
                if (x + 1 < MATRIX_WIDTH)
                {
                    s_frame[y][x + 1][0] = rr / 3;
                    s_frame[y][x + 1][1] = gg / 3;
                    s_frame[y][x + 1][2] = bb / 3;
                }

                if (x - 1 >= 0)
                {
                    s_frame[y][x - 1][0] = rr / 3;
                    s_frame[y][x - 1][1] = gg / 3;
                    s_frame[y][x - 1][2] = bb / 3;
                }
            }
        }
    }
}

// ======================================================
// GRID_BREATH
// ======================================================
void VisualModes_DrawGridBreath(const float *trail, float time)
{
    memset(s_frame, 0, sizeof(s_frame));

    float breath =
        0.5f +
        0.5f * sinf(time * 1.5f);

    breath *= breath;

    for (int y = 0; y < MATRIX_HEIGHT; y++)
    {
        for (int x = 0; x < MATRIX_WIDTH; x++)
        {
            float noise =
                sinf(x * 0.8f + time) *
                cosf(y * 0.7f + time);

            float v =
                (noise + 1.0f) *
                0.5f *
                breath;

            float r, g, b;

            VisualTheme_GetColor(
                s_spectrumTheme,
                v,
                &r,
                &g,
                &b);

            float brightness =
                0.75f +
                breath * 0.75f;

            r *= brightness;
            g *= brightness;
            b *= brightness;

            s_frame[y][x][0] = Gamma(r);
            s_frame[y][x][1] = Gamma(g);
            s_frame[y][x][2] = Gamma(b);
        }
    }
}

// ======================================================
// ROTATING_FIELD
// ======================================================
void VisualModes_DrawRotatingField(const float *trail, float time)
{
    memset(s_frame, 0, sizeof(s_frame));

    float cx = MATRIX_WIDTH * 0.5f;
    float cy = MATRIX_HEIGHT * 0.5f;

    float angle = time * 0.8f;

    for (int y = 0; y < MATRIX_HEIGHT; y++)
    {
        for (int x = 0; x < MATRIX_WIDTH; x++)
        {
            float dx = x - cx;
            float dy = y - cy;

            float rx =
                dx * cosf(angle) -
                dy * sinf(angle);

            float ry =
                dx * sinf(angle) +
                dy * cosf(angle);

            float v =
                sinf(rx * 0.3f) +
                cosf(ry * 0.3f);

            v = (v + 2.0f) / 4.0f;

            float r, g, b;

            VisualTheme_GetColor(
                s_spectrumTheme,
                v,
                &r,
                &g,
                &b);

            s_frame[y][x][0] = Gamma(r);
            s_frame[y][x][1] = Gamma(g);
            s_frame[y][x][2] = Gamma(b);
        }
    }
}

// ======================================================
// PLASMA
// ======================================================
void VisualModes_DrawPlasma(
    const float *trail,
    float time)
{
    memset(s_frame, 0, sizeof(s_frame));

    // =========================================
    // AUDIO ENERGY
    // =========================================
    float energy = 0.0f;

    for (int i = 0; i < MATRIX_WIDTH; i++)
    {
        energy += trail[i];
    }

    energy /= MATRIX_WIDTH;
    energy /= MATRIX_HEIGHT;

    // =========================================
    // MOTION
    // =========================================
    float motion =
        0.30f +
        energy * 0.50f;

    // =========================================
    // PLASMA FIELD
    // =========================================
    for (int y = 0; y < MATRIX_HEIGHT; y++)
    {
        for (int x = 0; x < MATRIX_WIDTH; x++)
        {
            float fx = (float)x;
            float fy = (float)y;

            // =====================================
            // AUDIO DISTORTION
            // =====================================
            fx +=
                sinf(
                    fy * 0.35f +
                    time * 0.8f)
                * energy
                * 2.0f;

            fy +=
                cosf(
                    fx * 0.25f +
                    time * 0.6f)
                * energy
                * 1.5f;

            // =====================================
            // WAVE 1
            // =====================================
            float v1 =
                sinf(
                    fx * 0.22f +
                    time * 0.45f * motion);

            // =====================================
            // WAVE 2
            // =====================================
            float v2 =
                sinf(
                    fy * 0.22f +
                    time * 0.40f * motion);

            // =====================================
            // RADIAL
            // =====================================
            float dx =
                fx -
                MATRIX_WIDTH * 0.5f;

            float dy =
                fy -
                MATRIX_HEIGHT * 0.5f;

            float dist =
                sqrtf(dx * dx + dy * dy);

            float v3 =
                sinf(
                    dist * 0.28f -
                    time * 0.65f * motion);

            // =====================================
            // LARGE FLOW
            // =====================================
            float flow =
                sinf(
                    fx * 0.05f +
                    fy * 0.03f +
                    time * 0.18f);

            // =====================================
            // COMBINE
            // =====================================
            float plasma =
                (
                    v1 +
                    v2 +
                    v3 +
                    flow
                ) * 0.25f;

            plasma =
                0.5f +
                plasma * 0.5f;

            // =====================================
            // BRIGHTNESS
            // =====================================
            float brightness =
                0.15f +
                plasma *
                (0.65f + energy * 0.45f);

            // =====================================
            // COLOR FLOW
            // =====================================
            float colorT =
                fmodf(
                    plasma +
                    time * 0.008f,
                    1.0f);

            float r, g, b;

            VisualTheme_GetColor(
                s_spectrumTheme,
                colorT,
                &r, &g, &b);

            // =====================================
            // APPLY BRIGHTNESS
            // =====================================
            r *= brightness;
            g *= brightness;
            b *= brightness;

            // =====================================
            // LIQUID HIGHLIGHT
            // =====================================
            float shine =
                plasma * plasma;

            r *=
                0.90f +
                shine * 0.40f;

            g *=
                0.90f +
                shine * 0.40f;

            b *=
                0.90f +
                shine * 0.40f;

            // =====================================
            // EXTRA GLOW
            // =====================================
            if (plasma > 0.80f)
            {
                r *= 1.15f;
                g *= 1.15f;
                b *= 1.15f;
            }

            // =====================================
            // GAMMA
            // =====================================
            s_frame[y][x][0] = Gamma(r);
            s_frame[y][x][1] = Gamma(g);
            s_frame[y][x][2] = Gamma(b);
        }
    }
}

// ======================================================
// MULTI_ORBIT
// ======================================================
void VisualModes_DrawMultiOrbit(
    const float *trail,
    float time)
{
    memset(s_frame, 0, sizeof(s_frame));

    // =========================================
    // AUDIO ENERGY
    // =========================================
    float energy = 0.0f;

    for (int i = 0; i < MATRIX_WIDTH; i++)
        energy += trail[i];

    energy /= MATRIX_WIDTH;
    energy /= MATRIX_HEIGHT;

    float cx =
        MATRIX_WIDTH * 0.5f;

    float cy =
        MATRIX_HEIGHT * 0.5f;

    // =========================================
    // ORBIT LAYERS
    // =========================================
    for (int layer = 0; layer < 3; layer++)
    {
        int particleCount =
            12 + layer * 8;

        float baseRadius =
            2.5f + layer * 2.5f;

        float speed =
            0.8f + layer * 0.4f;

        // 레이어마다 회전 방향 반전
        float dir =
            (layer & 1)
            ? -1.0f
            : 1.0f;

        for (int i = 0; i < particleCount; i++)
        {
            float angle =
                time * speed * dir +
                i *
                (6.28318f / particleCount);

            // =================================
            // ELLIPSE ORBIT
            // =================================
            float rx =
                baseRadius * 1.8f +
                energy * (4.0f + layer);

            float ry =
                baseRadius * 0.8f +
                energy * (1.5f + layer * 0.5f);

            // 미세 진동
            rx +=
                sinf(
                    time * 1.2f + i)
                * 0.5f;

            ry +=
                cosf(
                    time * 1.0f + i)
                * 0.3f;

            float fx =
                cx +
                cosf(angle) * rx;

            float fy =
                cy +
                sinf(angle) * ry;

            int x =
                (int)(fx + 0.5f);

            int y =
                (int)(fy + 0.5f);

            if (x < 0 ||
                x >= MATRIX_WIDTH)
                continue;

            if (y < 0 ||
                y >= MATRIX_HEIGHT)
                continue;

            // =================================
            // COLOR
            // =================================
            float t =
                (float)i /
                particleCount;

            t +=
                layer * 0.18f;

            if (t > 1.0f)
                t -= 1.0f;

            float r, g, b;

            VisualTheme_GetColor(
                s_spectrumTheme,
                t,
                &r,
                &g,
                &b);

            // =================================
            // GLOW
            // =================================
            float glow =
                0.75f +
                0.25f *
                sinf(
                    time * 3.0f +
                    i);

            glow *=
                (0.9f +
                 energy * 1.2f);

            r *= glow;
            g *= glow;
            b *= glow;

            uint8_t rr = Gamma(r);
            uint8_t gg = Gamma(g);
            uint8_t bb = Gamma(b);

            // =================================
            // CORE
            // =================================
            s_frame[y][x][0] = rr;
            s_frame[y][x][1] = gg;
            s_frame[y][x][2] = bb;

            // =================================
            // TRAIL
            // =================================
            float trailAngle =
                angle -
                dir * 0.20f;

            int tx =
                (int)(
                    cx +
                    cosf(trailAngle) *
                    rx +
                    0.5f);

            int ty =
                (int)(
                    cy +
                    sinf(trailAngle) *
                    ry +
                    0.5f);

            if (tx >= 0 &&
                tx < MATRIX_WIDTH &&
                ty >= 0 &&
                ty < MATRIX_HEIGHT)
            {
                s_frame[ty][tx][0] =
                    rr / 2;

                s_frame[ty][tx][1] =
                    gg / 2;

                s_frame[ty][tx][2] =
                    bb / 2;
            }

            // =================================
            // BLOOM
            // =================================
            if (x + 1 < MATRIX_WIDTH)
            {
                s_frame[y][x + 1][0] =
                    rr / 3;

                s_frame[y][x + 1][1] =
                    gg / 3;

                s_frame[y][x + 1][2] =
                    bb / 3;
            }

            if (x - 1 >= 0)
            {
                s_frame[y][x - 1][0] =
                    rr / 3;

                s_frame[y][x - 1][1] =
                    gg / 3;

                s_frame[y][x - 1][2] =
                    bb / 3;
            }

            if (y + 1 < MATRIX_HEIGHT)
            {
                s_frame[y + 1][x][0] =
                    rr / 3;

                s_frame[y + 1][x][1] =
                    gg / 3;

                s_frame[y + 1][x][2] =
                    bb / 3;
            }

            if (y - 1 >= 0)
            {
                s_frame[y - 1][x][0] =
                    rr / 3;

                s_frame[y - 1][x][1] =
                    gg / 3;

                s_frame[y - 1][x][2] =
                    bb / 3;
            }
        }
    }
}

// ======================================================
// HEX
// ======================================================
void VisualModes_DrawHexGrid(const float *trail, float time)
{
    memset(s_frame, 0, sizeof(s_frame));

    float energy = 0.0f;

    for (int i = 0; i < MATRIX_WIDTH; i++)
        energy += trail[i];

    energy /= MATRIX_WIDTH;
    energy /= MATRIX_HEIGHT;

    for (int y = 0; y < MATRIX_HEIGHT; y++)
    {
        for (int x = 0; x < MATRIX_WIDTH; x++)
        {
            float xx =
                x + ((y & 1) ? 0.5f : 0.0f);

            float yy = y * 0.85f;

            float d1 =
                sqrtf(
                    (xx - 4.0f) * (xx - 4.0f) +
                    (yy - 5.0f) * (yy - 5.0f));

            float d2 =
                sqrtf(
                    (xx - 12.0f) * (xx - 12.0f) +
                    (yy - 10.0f) * (yy - 10.0f));

            float wave =
                sinf(d1 * 1.4f - time * 3.0f) +
                sinf(d2 * 1.2f - time * 2.2f);

            wave *= 0.5f;

            float t =
                0.5f + 0.5f * wave;

            t *=
                0.45f +
                energy * 1.4f;

            if (t > 1.0f)
                t = 1.0f;

            float r, g, b;

            VisualTheme_GetColor(
                s_spectrumTheme,
                t,
                &r,
                &g,
                &b);

            float pulse =
                0.75f +
                0.25f *
                sinf(time * 4.0f + x + y);

            r *= pulse;
            g *= pulse;
            b *= pulse;

            s_frame[y][x][0] = Gamma(r);
            s_frame[y][x][1] = Gamma(g);
            s_frame[y][x][2] = Gamma(b);
        }
    }
}

// ======================================================
// LASER
// ======================================================
void VisualModes_DrawLaserScan(const float *trail, float time)
{
    static float decay[MATRIX_HEIGHT][MATRIX_WIDTH];

    // =========================================
    // AUDIO ENERGY
    // =========================================
    float energy = 0.0f;

    for (int i = 0; i < MATRIX_WIDTH; i++)
        energy += trail[i];

    energy /= MATRIX_WIDTH;
    energy /= MATRIX_HEIGHT;

    // =========================================
    // FADE
    // =========================================
    for (int y = 0; y < MATRIX_HEIGHT; y++)
    {
        for (int x = 0; x < MATRIX_WIDTH; x++)
        {
            decay[y][x] *= 0.93f;

            if (decay[y][x] < 0.01f)
                decay[y][x] = 0.0f;
        }
    }

    // =========================================
    // LASER BEAMS
    // =========================================
    for (int beam = 0; beam < 6; beam++)
    {
        float speed =
            0.8f +
            beam * 0.25f +
            energy * 1.2f;

        float angle =
            time * speed +
            beam * (6.28318f / 6.0f);

        float sx =
            MATRIX_WIDTH * 0.5f;

        float sy =
            MATRIX_HEIGHT * 0.5f;

        float dx = cosf(angle);
        float dy = sinf(angle);

        for (float t = 0.0f; t < 14.0f; t += 0.20f)
        {
            int x =
                (int)(sx + dx * t);

            int y =
                (int)(sy + dy * t);

            if (x < 0 || x >= MATRIX_WIDTH)
                continue;

            if (y < 0 || y >= MATRIX_HEIGHT)
                continue;

            float power =
                (1.0f - t / 14.0f);

            power *=
                (0.7f + energy * 1.5f);

            if (power > decay[y][x])
                decay[y][x] = power;
        }
    }

    // =========================================
    // DRAW
    // =========================================
    for (int y = 0; y < MATRIX_HEIGHT; y++)
    {
        for (int x = 0; x < MATRIX_WIDTH; x++)
        {
            float v = decay[y][x];

            if (v <= 0.0f)
                continue;

            // ---------------------------------
            // CYBER LASER COLOR
            // ---------------------------------
            float r, g, b;

            VisualTheme_GetMirrorColor(
                s_mirrorTheme,
                1.0f,
                &r,
                &g,
                &b);

            r *= v * 1.4f;
            g *= v * 1.4f;
            b *= v * 1.4f;

            uint8_t rr = Gamma(r);
            uint8_t gg = Gamma(g);
            uint8_t bb = Gamma(b);

            s_frame[y][x][0] = rr;
            s_frame[y][x][1] = gg;
            s_frame[y][x][2] = bb;

            // =================================
            // BLOOM
            // =================================
            if (v > 0.55f)
            {
                if (x + 1 < MATRIX_WIDTH)
                {
                    s_frame[y][x + 1][0] = rr / 3;
                    s_frame[y][x + 1][1] = gg / 3;
                    s_frame[y][x + 1][2] = bb / 3;
                }

                if (y + 1 < MATRIX_HEIGHT)
                {
                    s_frame[y + 1][x][0] = rr / 3;
                    s_frame[y + 1][x][1] = gg / 3;
                    s_frame[y + 1][x][2] = bb / 3;
                }
            }
        }
    }
}

// ======================================================
// WATERUP
// ======================================================
void VisualModes_DrawWaterup(const float *trail)
{
    static uint8_t waterfall[MATRIX_HEIGHT][MATRIX_WIDTH][3];
    static uint8_t blur[MATRIX_HEIGHT][MATRIX_WIDTH][3];

    // =====================================
    // SHIFT + FADE
    // =====================================
    for (int y = MATRIX_HEIGHT - 1; y > 0; y--)
    {
        for (int x = 0; x < MATRIX_WIDTH; x++)
        {
            waterfall[y][x][0] =
                (uint8_t)(waterfall[y - 1][x][0] * 0.94f);

            waterfall[y][x][1] =
                (uint8_t)(waterfall[y - 1][x][1] * 0.94f);

            waterfall[y][x][2] =
                (uint8_t)(waterfall[y - 1][x][2] * 0.94f);
        }
    }

    // =====================================
    // NEW TOP LINE
    // =====================================
    for (int x = 0; x < MATRIX_WIDTH; x++)
    {
        int h = (int)trail[x];

        float t =
            (float)h /
            (float)MATRIX_HEIGHT;

        float r, g, b;

        VisualTheme_GetColor(
            s_spectrumTheme,
            t,
            &r,
            &g,
            &b);

        waterfall[0][x][0] = Gamma(r);
        waterfall[0][x][1] = Gamma(g);
        waterfall[0][x][2] = Gamma(b);
    }

    // =====================================
    // VERTICAL BLUR
    // =====================================
    memcpy(
        blur[0],
        waterfall[0],
        MATRIX_WIDTH * 3);

    memcpy(
        blur[MATRIX_HEIGHT - 1],
        waterfall[MATRIX_HEIGHT - 1],
        MATRIX_WIDTH * 3);

    for (int y = 1; y < MATRIX_HEIGHT - 1; y++)
    {
        for (int x = 0; x < MATRIX_WIDTH; x++)
        {
            blur[y][x][0] =
                (waterfall[y - 1][x][0]
                + waterfall[y][x][0] * 2
                + waterfall[y + 1][x][0]) / 4;

            blur[y][x][1] =
                (waterfall[y - 1][x][1]
                + waterfall[y][x][1] * 2
                + waterfall[y + 1][x][1]) / 4;

            blur[y][x][2] =
                (waterfall[y - 1][x][2]
                + waterfall[y][x][2] * 2
                + waterfall[y + 1][x][2]) / 4;
        }
    }

    // =====================================
    // OUTPUT
    // =====================================
    memcpy(
        s_frame,
        blur,
        sizeof(s_frame));
}

// ======================================================
// SHOCKWAVE
// ======================================================
typedef struct
{
    float radius;
    float power;
    uint8_t active;

} Shockwave_t;

#define MAX_SHOCKWAVES 6

void VisualModes_DrawShockwave(
    const float *trail,
    float time)
{
    static Shockwave_t waves[MAX_SHOCKWAVES];

    memset(s_frame, 0, sizeof(s_frame));

    // ==================================================
    // AUDIO ENERGY
    // ==================================================
    float energy = 0.0f;

    for(int i=0;i<MATRIX_WIDTH;i++)
    {
        energy += trail[i];
    }

    energy /= MATRIX_WIDTH;
    energy /= MATRIX_HEIGHT;

    // ==================================================
    // BEAT DETECT
    // ==================================================
    static float beatCooldown = 0.0f;

    beatCooldown -= 0.016f;

    if(beatCooldown < 0.0f)
        beatCooldown = 0.0f;

    if(energy > 0.18f &&
       beatCooldown <= 0.0f)
    {
        for(int i=0;i<MAX_SHOCKWAVES;i++)
        {
            if(!waves[i].active)
            {
                waves[i].active = 1;

                waves[i].radius = 0.0f;

                // ==========================
                // POWER 강화
                // ==========================
                waves[i].power =
                    1.5f +
                    energy * 4.0f;

                beatCooldown = 0.18f;

                break;
            }
        }
    }

    // ==================================================
    // CENTER
    // ==================================================
    float cx =
        MATRIX_WIDTH * 0.5f;

    float cy =
        MATRIX_HEIGHT * 0.5f;

    // ==================================================
    // UPDATE WAVES
    // ==================================================
    for(int i=0;i<MAX_SHOCKWAVES;i++)
    {
        if(!waves[i].active)
            continue;

        waves[i].radius +=
            0.30f +
            energy * 0.70f;

        waves[i].power *= 0.985f;

        if(waves[i].radius > 20.0f ||
           waves[i].power  < 0.05f)
        {
            waves[i].active = 0;
        }
    }

    // ==================================================
    // DRAW FIELD
    // ==================================================
    for(int y=0;y<MATRIX_HEIGHT;y++)
    {
        for(int x=0;x<MATRIX_WIDTH;x++)
        {
            float dx =
                x - cx;

            float dy =
                y - cy;

            float dist =
                sqrtf(dx*dx + dy*dy);

            float brightness = 0.0f;

            // ==========================================
            // SHOCKWAVE RINGS
            // ==========================================
            for(int i=0;i<MAX_SHOCKWAVES;i++)
            {
                if(!waves[i].active)
                    continue;

                float d =
                    fabsf(
                        dist -
                        waves[i].radius);

                // ==============================
                // TAIL RING
                // ==============================
                float ring =
                    expf(
                        -d * 0.55f);

                brightness +=
                    ring *
                    waves[i].power;
            }

            // ==========================================
            // CENTER CORE
            // ==========================================
            float centerGlow =
                1.0f -
                (dist / 5.0f);

            if(centerGlow < 0.0f)
                centerGlow = 0.0f;

            centerGlow *= centerGlow;
            centerGlow *= 0.35f;

            brightness += centerGlow;

            // ==========================================
            // CLAMP
            // ==========================================
            if(brightness > 2.0f)
                brightness = 2.0f;

            // ==========================================
            // COLOR FLOW
            // ==========================================
            float colorT =
                fmodf(
                    dist * 0.06f +
                    time * 0.01f,
                    1.0f);

            float r,g,b;

            VisualTheme_GetColor(
                s_spectrumTheme,
                colorT,
                &r,&g,&b);

            // ==========================================
            // APPLY
            // ==========================================
            r *= brightness;
            g *= brightness;
            b *= brightness;

            if(brightness > 1.0f)
            {
                r *= 1.15f;
                g *= 1.15f;
                b *= 1.15f;
            }

            uint8_t rr = Gamma(r);
            uint8_t gg = Gamma(g);
            uint8_t bb = Gamma(b);

            s_frame[y][x][0] = rr;
            s_frame[y][x][1] = gg;
            s_frame[y][x][2] = bb;

            // ==========================================
            // BLOOM
            // ==========================================
            if(brightness > 0.35f)
            {
                if(x + 1 < MATRIX_WIDTH)
                {
                    s_frame[y][x+1][0] = rr / 3;
                    s_frame[y][x+1][1] = gg / 3;
                    s_frame[y][x+1][2] = bb / 3;
                }

                if(x - 1 >= 0)
                {
                    s_frame[y][x-1][0] = rr / 3;
                    s_frame[y][x-1][1] = gg / 3;
                    s_frame[y][x-1][2] = bb / 3;
                }

                if(y + 1 < MATRIX_HEIGHT)
                {
                    s_frame[y+1][x][0] = rr / 3;
                    s_frame[y+1][x][1] = gg / 3;
                    s_frame[y+1][x][2] = bb / 3;
                }

                if(y - 1 >= 0)
                {
                    s_frame[y-1][x][0] = rr / 3;
                    s_frame[y-1][x][1] = gg / 3;
                    s_frame[y-1][x][2] = bb / 3;
                }
            }
        }
    }
}

// ======================================================
// NORTHERN_LIGHTS
// ======================================================
void VisualModes_DrawNorthernLights(
    const float *trail,
    float time)
{
    memset(s_frame, 0, sizeof(s_frame));

    // =========================================
    // ENERGY
    // =========================================
    float energy = 0.0f;

    for (int i = 0; i < MATRIX_WIDTH; i++)
        energy += trail[i];

    energy /= MATRIX_WIDTH;
    energy /= MATRIX_HEIGHT;

    // =========================================
    // AURORA
    // =========================================
    for (int x = 0; x < MATRIX_WIDTH; x++)
    {
        float layer1 =
            sinf(
                x * 0.12f +
                time * 0.25f);

        float layer2 =
            sinf(
                x * 0.05f -
                time * 0.18f);

        float layer3 =
            sinf(
                x * 0.02f +
                time * 0.10f);

        float aurora =
            layer1 * 0.55f +
            layer2 * 0.30f +
            layer3 * 0.15f;

        float centerY =
            MATRIX_HEIGHT * 0.5f +
            aurora *
            (4.0f + energy * 6.0f);

        float curtainWidth =
            3.5f +
            energy * 3.0f;

        for (int y = 0; y < MATRIX_HEIGHT; y++)
        {
            float dist =
                fabsf(y - centerY);

            float intensity =
                1.0f -
                dist / curtainWidth;

            if (intensity <= 0.0f)
                continue;

            intensity *= intensity;

            float t =
                fmodf(
                    x * 0.04f +
                    time * 0.02f,
                    1.0f);

            float r, g, b;

            VisualTheme_GetColor(
                s_spectrumTheme,
                t,
                &r,
                &g,
                &b);

            float brightness =
                0.55f +
                energy * 0.55f;

            r *= intensity * brightness;
            g *= intensity * brightness;
            b *= intensity * brightness;

            s_frame[y][x][0] = Gamma(r);
            s_frame[y][x][1] = Gamma(g);
            s_frame[y][x][2] = Gamma(b);
        }
    }
}


// ======================================================
// GET FRAME
// ======================================================
const uint8_t *VisualModes_GetFrame(void)
{
	return &s_frame[0][0][0];
}
