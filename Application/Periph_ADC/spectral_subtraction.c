#include "spectral_subtraction.h"
#include "spectral_sub_noise_profile.h"

#include <string.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

#define SS_EPS 1.0e-12f

static arm_rfft_fast_instance_f32 s_rfft;

static float32_t s_win[SS_FFT_SIZE];
static float32_t s_prevIn[2][SS_HOP_SIZE];
static float32_t s_prevOla[2][SS_HOP_SIZE];

static float32_t s_frame[SS_FFT_SIZE];
static float32_t s_fft[SS_FFT_SIZE];
static float32_t s_time[SS_FFT_SIZE];

static inline float32_t SS_CalcGain(float32_t mag, float32_t noise)
{
	float32_t gain;

	if (mag < SS_EPS)
		return SS_FLOOR;

	gain = 1.0f - ((SS_ALPHA * noise) / mag);

	if (gain < SS_FLOOR)
		gain = SS_FLOOR;
	else if (gain > 1.0f)
		gain = 1.0f;

	return gain;
}

void SpectralSub_Init(void)
{
	arm_rfft_fast_init_f32(&s_rfft, SS_FFT_SIZE);

	for (uint32_t i = 0; i < SS_FFT_SIZE; i++)
	{
		float32_t hann;

		hann = 0.5f - 0.5f * arm_cos_f32(2.0f * PI * (float32_t)i / (float32_t)(SS_FFT_SIZE - 1U));
		s_win[i] = sqrtf(hann);
	}

	memset(s_prevIn, 0, sizeof(s_prevIn));
	memset(s_prevOla, 0, sizeof(s_prevOla));
}

static void SpectralSub_ProcessChannel(uint32_t ch,
                                       const float32_t *src,
                                       float32_t *dst)
{
	const float32_t *noise;

	if (ch == 0U)
		noise = g_spectralNoiseL;
	else
		noise = g_spectralNoiseR;

	arm_copy_f32(s_prevIn[ch], &s_frame[0], SS_HOP_SIZE);
	arm_copy_f32(src, &s_frame[SS_HOP_SIZE], SS_HOP_SIZE);

	arm_mult_f32(s_frame, s_win, s_frame, SS_FFT_SIZE);

	arm_rfft_fast_f32(&s_rfft, s_frame, s_fft, 0);

	/* DC bin */
	{
		float32_t re;
		float32_t mag;
		float32_t gain;

		re = s_fft[0];
		mag = fabsf(re);
		gain = SS_CalcGain(mag, noise[0]);
		s_fft[0] = re * gain;
	}

	/* Nyquist bin */
	{
		float32_t re;
		float32_t mag;
		float32_t gain;

		re = s_fft[1];
		mag = fabsf(re);
		gain = SS_CalcGain(mag, noise[SS_BIN_COUNT - 1U]);
		s_fft[1] = re * gain;
	}

	/* Complex bins: 1 .. 255 */
	for (uint32_t k = 1U; k < (SS_FFT_SIZE / 2U); k++)
	{
		uint32_t idx;
		float32_t re;
		float32_t im;
		float32_t mag;
		float32_t gain;

		idx = k * 2U;
		re = s_fft[idx];
		im = s_fft[idx + 1U];

		arm_sqrt_f32((re * re) + (im * im), &mag);
		gain = SS_CalcGain(mag, noise[k]);

		s_fft[idx] = re * gain;
		s_fft[idx + 1U] = im * gain;
	}

	arm_rfft_fast_f32(&s_rfft, s_fft, s_time, 1);

	for (uint32_t i = 0; i < SS_HOP_SIZE; i++)
	{
		float32_t y0;
		float32_t y1;

		y0 = s_time[i] * s_win[i];
		y1 = s_time[i + SS_HOP_SIZE] * s_win[i + SS_HOP_SIZE];

		dst[i] = y0 + s_prevOla[ch][i];
		s_prevOla[ch][i] = y1;
	}

	arm_copy_f32(src, s_prevIn[ch], SS_HOP_SIZE);
}

void SpectralSub_Process(const float32_t src[2][SS_HOP_SIZE],
                         float32_t dst[2][SS_HOP_SIZE])
{
	/* L/R을 각각 독립적으로 FFT -> gain 계산 -> IFFT 처리한다. */
	SpectralSub_ProcessChannel(0U, src[0], dst[0]);
	SpectralSub_ProcessChannel(1U, src[1], dst[1]);
}
