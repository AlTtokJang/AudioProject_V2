#ifndef SPECTRAL_SUBTRACTION_H_
#define SPECTRAL_SUBTRACTION_H_

#include <stdint.h>
#include "arm_math.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SS_SAMPLE_RATE      48000U
#define SS_FFT_SIZE         512U
#define SS_HOP_SIZE         256U
#define SS_BIN_COUNT        (SS_FFT_SIZE / 2U + 1U)

#define SS_ALPHA            1.8f
#define SS_FLOOR            0.0f

/*
 * 입력/출력 단위:
 *   src[ch][i] = ADC raw - 2048.0f  단위의 PCM count
 *   dst[ch][i] = 같은 단위의 필터 후 PCM count
 *
 * ch 0 = Left, ch 1 = Right
 * i = 0..255
 */
void SpectralSub_Init(void);
void SpectralSub_Process(const float32_t src[2][SS_HOP_SIZE],
                         float32_t dst[2][SS_HOP_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* SPECTRAL_SUBTRACTION_H_ */
