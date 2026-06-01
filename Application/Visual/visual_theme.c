/*
 * visual_theme.c (refactored stable version)
 */

#include "visual_theme.h"
#include "visual_renderer.h"
#include <math.h>

// ======================================================
// TYPE
// ======================================================
typedef struct {
    float r, g, b;
} Color;

// ======================================================
// UTIL
// ======================================================
static inline float clamp01(float x)
{
    return (x < 0.0f) ? 0.0f : (x > 1.0f ? 1.0f : x);
}

static inline float clampf(float x, float max)
{
    return (x > max) ? max : x;
}

static inline float smoothstep(float x)
{
    x = clamp01(x);
    return x * x * (3.0f - 2.0f * x);
}

static inline float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

// ======================================================
// LED OUTPUT
// ======================================================
static inline void applyColor(Color c, float *r, float *g, float *b)
{
    *r = LED_ON_R * c.r;
    *g = LED_ON_G * c.g;
    *b = LED_ON_B * c.b;

    *r = clampf(*r, LED_ON_R);
    *g = clampf(*g, LED_ON_G);
    *b = clampf(*b, LED_ON_B);
}

// ======================================================
// SEGMENT
// ======================================================
static inline int segment(
    float t, float t0, float t1,
    Color c0, Color c1,
    float *r, float *g, float *b)
{
    if (t < t0 || t >= t1) return 0;

    float d = t1 - t0;
    if (d <= 1e-6f) return 0;

    float x = clamp01((t - t0) / d);
    float k = smoothstep(x);

    Color out = {
        lerp(c0.r, c1.r, k),
        lerp(c0.g, c1.g, k),
        lerp(c0.b, c1.b, k),
    };

    applyColor(out, r, g, b);
    return 1;
}

// ======================================================
// PALETTE (UNCHANGED)
// ======================================================
static const Color C_RED    = {1.00f, 0.05f, 0.05f};
static const Color C_PINK   = {1.00f, 0.25f, 0.90f};
static const Color C_WHITEP = {1.00f, 0.85f, 1.00f};

static const Color C_PURPLE = {0.60f, 0.10f, 1.00f};
static const Color C_LAV    = {0.85f, 0.65f, 1.00f};

static const Color C_BLUE   = {0.10f, 0.25f, 1.00f};
static const Color C_SKY    = {0.20f, 0.75f, 1.00f};
static const Color C_CYAN   = {0.10f, 1.00f, 1.00f};

static const Color C_GREEN  = {0.10f, 1.00f, 0.20f};
static const Color C_LIME   = {0.60f, 1.00f, 0.10f};

static const Color C_YELLOW = {1.00f, 1.00f, 0.10f};
static const Color C_ORANGE = {1.00f, 0.55f, 0.05f};

static const Color C_WHITEG = {1.00f, 1.00f, 0.75f};
static const Color C_WARMW  = {1.00f, 0.95f, 0.90f};

// ======================================================
// THEME ENGINE (UNCHANGED LOGIC)
// ======================================================
void VisualTheme_GetColor(uint8_t theme, float t, float *r, float *g, float *b)
{
    t = clamp01(t);

    switch (theme)
    {
        case 0:
            if (segment(t,0.00f,0.14f,C_SKY,C_WHITEG,r,g,b)) break;
            if (segment(t,0.14f,0.28f,C_WHITEG,C_YELLOW,r,g,b)) break;
            if (segment(t,0.28f,0.46f,C_YELLOW,C_ORANGE,r,g,b)) break;
            if (segment(t,0.46f,0.64f,C_ORANGE,C_WHITEP,r,g,b)) break;
            if (segment(t,0.64f,0.82f,C_WHITEP,C_PINK,r,g,b)) break;
            segment(t,0.82f,1.0f,C_PINK,C_PURPLE,r,g,b);
            break;

        case 1:
            if (segment(t,0.00f,0.25f,C_LIME,C_CYAN,r,g,b)) break;
            if (segment(t,0.25f,0.50f,C_CYAN,C_BLUE,r,g,b)) break;
            if (segment(t,0.50f,0.75f,C_BLUE,C_PURPLE,r,g,b)) break;
            segment(t,0.75f,1.0f,C_PURPLE,C_LAV,r,g,b);
            break;

        case 2:
            if (segment(t,0.00f,0.12f,C_RED,C_PINK,r,g,b)) break;
            if (segment(t,0.12f,0.26f,C_PINK,C_WHITEP,r,g,b)) break;
            if (segment(t,0.26f,0.40f,C_WHITEP,C_LAV,r,g,b)) break;
            if (segment(t,0.40f,0.54f,C_LAV,C_PURPLE,r,g,b)) break;
            if (segment(t,0.54f,0.68f,C_PURPLE,C_BLUE,r,g,b)) break;
            if (segment(t,0.68f,0.84f,C_BLUE,C_SKY,r,g,b)) break;
            segment(t,0.84f,1.0f,C_SKY,C_CYAN,r,g,b);
            break;

        case 3:
            if (segment(t,0.00f,0.22f,C_LIME,C_YELLOW,r,g,b)) break;
            if (segment(t,0.22f,0.44f,C_YELLOW,C_ORANGE,r,g,b)) break;
            if (segment(t,0.44f,0.66f,C_ORANGE,C_RED,r,g,b)) break;
            segment(t,0.66f,1.0f,C_RED,C_BLUE,r,g,b);
            break;

        case 4:
            if (segment(t,0.00f,0.16f,C_RED,C_PINK,r,g,b)) break;
            if (segment(t,0.16f,0.34f,C_PINK,C_WHITEP,r,g,b)) break;
            if (segment(t,0.34f,0.54f,C_WHITEP,C_LIME,r,g,b)) break;
            if (segment(t,0.54f,0.76f,C_LIME,C_GREEN,r,g,b)) break;
            segment(t,0.76f,1.0f,C_GREEN,C_CYAN,r,g,b);
            break;

        default:
            if (segment(t,0.00f,0.14f,C_YELLOW,C_WARMW,r,g,b)) break;
            if (segment(t,0.14f,0.28f,C_WARMW,C_PINK,r,g,b)) break;
            if (segment(t,0.28f,0.44f,C_PINK,C_PURPLE,r,g,b)) break;
            if (segment(t,0.44f,0.62f,C_PURPLE,C_BLUE,r,g,b)) break;
            if (segment(t,0.62f,0.82f,C_BLUE,C_CYAN,r,g,b)) break;
            segment(t,0.82f,1.0f,C_CYAN,C_GREEN,r,g,b);
            break;
    }
}

// ======================================================
// PEAK COLOR
// ======================================================

void VisualTheme_GetPeakColor(
    uint8_t theme,
    float *r,
    float *g,
    float *b)
{
    Color c;

    switch (theme)
    {
    case 0: c = C_GREEN;   break;
    case 1: c = C_PINK;    break;
    case 2: c = C_YELLOW;  break;
    case 3: c = C_WARMW;   break;
    case 4: c = C_BLUE;    break;
    default: c = C_ORANGE; break;
    }

    applyColor(c, r, g, b);
}

// ======================================================
// MIRROR COLOR (STABLE NONLINEAR)
// ======================================================

void VisualTheme_GetMirrorColor(
    uint8_t theme,
    float t,
    float *r,
    float *g,
    float *b)
{
    t = clamp01(t);

    float k = clamp01(powf(t, 0.72f));
    Color c;

    switch (theme)
    {
    case 0: // BLUE (강하게, 딥블루)
        c = (Color){
            0.05f + 0.20f*k,
            0.10f + 0.25f*k,
            1.00f
        };
        break;

    case 1: // CYAN-GREEN (구분용)
        c = (Color){
            0.05f + 0.20f*k,
            1.00f,
            0.20f + 0.30f*k
        };
        break;

    case 2: // RED (강하게, 순수 레드)
        c = (Color){
            1.00f,
            0.05f + 0.20f*k,
            0.05f + 0.20f*k
        };
        break;

    case 3: // PURPLE (딥 퍼플)
        c = (Color){
            0.60f + 0.30f*k,
            0.05f + 0.15f*k,
            1.00f
        };
        break;

    case 4: // ORANGE (강한 따뜻한색)
        c = (Color){
            1.00f,
            0.35f + 0.35f*k,
            0.05f
        };
        break;

    default: // WHITE
        c = (Color){
            0.85f + 0.15f*k,
            0.85f + 0.15f*k,
            0.85f + 0.15f*k
        };
        break;
    }

    applyColor(c, r, g, b);
}
