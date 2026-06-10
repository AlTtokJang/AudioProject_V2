/*
 * audio_pipeline.c
 *
 *  Created on: 2026. 5. 8.
 *      Author: ADJ
 */

#include "audio_pipeline.h"
#include "main.h"

#include "global_define.h"
#include "audio_ring.h"
#include "eq.h"
#include "agc.h"

#define INPUT_RING_SIZE			(MASTER_BLOCK_SIZE * 11U)
#define OUTPUT_RING_SIZE		(MASTER_BLOCK_SIZE * 11U)
#define FFT_RING_SIZE			(MASTER_BLOCK_SIZE * 32U)

#define AUDIO_PIPELINE_STOP_MS	2000U

#define AUDIO_SYNC_ADJUST_SAMPLES  4U
#define AUDIO_PROCESS_MAX_SAMPLES  (MASTER_BLOCK_SIZE + AUDIO_SYNC_ADJUST_SAMPLES)

extern volatile uint8_t i2sUseEq;
extern volatile uint8_t agcOff;

static uint8_t s_pipelineStopped;
static uint32_t s_inputEmptyStartTick;
static uint32_t s_lastFftZeroPushTick;
static int16_t s_fftZeroBlock[MASTER_BLOCK_SIZE];

static int16_t pcmRaw[AUDIO_PROCESS_MAX_SAMPLES];
static int16_t pcmEQ[AUDIO_PROCESS_MAX_SAMPLES];
static int16_t pcmMono[AUDIO_PROCESS_MAX_SAMPLES / 2];

static AudioRing_t inputRing;
static AudioRing_t outputRing;
static AudioRing_t fftRing;

static int16_t inputRingMemory[INPUT_RING_SIZE];
static int16_t outputRingMemory[OUTPUT_RING_SIZE];
static int16_t fftRingMemory[FFT_RING_SIZE];

static void AudioPipeline_StereoToMono(const int16_t *src, int16_t *dst, uint32_t frameCount);

void AudioPipeline_Init(void)
{
	AudioRing_Init(&inputRing, inputRingMemory, INPUT_RING_SIZE);
	AudioRing_Init(&outputRing, outputRingMemory, OUTPUT_RING_SIZE);
	AudioRing_Init(&fftRing, fftRingMemory, FFT_RING_SIZE);
}

void AudioPipeline_Push(const int16_t *src, uint16_t count)
{
	if (src == NULL || count == 0)
	{
		return;
	}

	AudioRing_Push(&inputRing, src, count);
}

uint16_t AudioPipeline_PopOutput(int16_t *dst, uint16_t count)
{
	if (dst == NULL || count == 0)
	{
		return 0;
	}

	return AudioRing_Pop(&outputRing, dst, count);
}

uint16_t AudioPipeline_PopMono(int16_t *dst, uint16_t count)
{
	if (dst == NULL || count == 0)
	{
		return 0;
	}

	return AudioRing_Pop(&fftRing, dst, count);
}

uint16_t AudioPipeline_Mono_Available(void)
{
	return AudioRing_Available(&fftRing);
}

void AudioPipeline_Process(void)
{
	uint16_t popSize = MASTER_BLOCK_SIZE;

	if (AudioRing_Available(&outputRing) < MASTER_BLOCK_SIZE * 2)
	{
		popSize += AUDIO_SYNC_ADJUST_SAMPLES;
	}

	if (AudioRing_Free(&outputRing) < popSize)
	{
		return;
	}

	// -------------------------------------------------------
	// 오디오 파이프라인 멈춤  해결 로직으로 변경됨
	// 일정 시간 지나면 fft에 0만 공급해서 LED 갱신할 수 있도록 함
	if (AudioRing_Available(&inputRing) < popSize)
	{
		uint32_t now = HAL_GetTick();

		if (s_inputEmptyStartTick == 0)
			s_inputEmptyStartTick = now;

		if (!s_pipelineStopped)
			if ((now - s_inputEmptyStartTick) >= AUDIO_PIPELINE_STOP_MS)
				s_pipelineStopped = 1;

		if (s_pipelineStopped)
		{
			if ((now - s_lastFftZeroPushTick) >= 5)
			{
				s_lastFftZeroPushTick = now;

				if (AudioRing_Free(&fftRing) >= MASTER_BLOCK_SIZE)
					AudioRing_Push(&fftRing, s_fftZeroBlock, MASTER_BLOCK_SIZE);
			}
		}

		return;
	}
	else
	{
		s_inputEmptyStartTick = 0;
		s_pipelineStopped = 0;
	}
	// -------------------------------------------------------

	AudioRing_Pop(&inputRing, pcmRaw, popSize);

	if (i2sUseEq)
	{
		EQ_ProcessStereo(pcmRaw, pcmEQ, popSize / 2);
		AudioRing_Push(&outputRing, pcmEQ, popSize);
	}
	else
	{
		AudioRing_Push(&outputRing, pcmRaw, popSize);
	}

	AudioPipeline_StereoToMono(pcmRaw, pcmMono, popSize / 2);

	if (!agcOff)
	{
		AGC_Run(pcmMono, pcmMono, popSize / 2);
	}

	if (AudioRing_Free(&fftRing) >= popSize / 2)
	{
		AudioRing_Push(&fftRing, pcmMono, popSize / 2);
	}

}

void AudioPipeline_RingClear(void)
{
	AudioRing_Clear(&inputRing);
	AudioRing_Clear(&outputRing);
	AudioRing_Clear(&fftRing);
}

static void AudioPipeline_StereoToMono(const int16_t *src, int16_t *dst, uint32_t frameCount)
{
	for (uint32_t i = 0; i < frameCount; i++)
	{
		int16_t left = src[2U * i];
		int16_t right = src[2U * i + 1U];

		dst [i] = (left + right) / 2;
	}
}
/*
 * 디버거 -------------------------------------------------------------------------------------
 */
static volatile uint16_t inputRing_available;
static volatile uint16_t inputRing_free;

static volatile uint16_t outputRing_available;
static volatile uint16_t outputRing_free;

static volatile uint16_t fftRing_available;
static volatile uint16_t fftRing_free;

void AudioPipeline_Loger(void)
{
	inputRing_available = AudioRing_Available(&inputRing);
	inputRing_free = AudioRing_Free(&inputRing);

	outputRing_available = AudioRing_Available(&outputRing);
	outputRing_free = AudioRing_Free(&outputRing);

	fftRing_available = AudioRing_Available(&fftRing);
	fftRing_free = AudioRing_Free(&fftRing);

	if (AudioRing_HasOverflow(&inputRing))
		Error_Loger(AUDIO_ERR_INPUT_OVERFLOW);
	if (AudioRing_HasUnderflow(&inputRing))
		Error_Loger(AUDIO_ERR_INPUT_UNDERFLOW);
	AudioRing_ClearFlags(&inputRing);

	if (AudioRing_HasOverflow(&outputRing))
		Error_Loger(AUDIO_ERR_OUTPUT_OVERFLOW);
	if (AudioRing_HasUnderflow(&outputRing))
		Error_Loger(AUDIO_ERR_OUTPUT_UNDERFLOW);
	AudioRing_ClearFlags(&outputRing);

	if (AudioRing_HasOverflow(&fftRing))
		Error_Loger(AUDIO_ERR_FFT_OVERFLOW);
	if (AudioRing_HasUnderflow(&fftRing))
		Error_Loger(AUDIO_ERR_FFT_UNDERFLOW);
	AudioRing_ClearFlags(&fftRing);
}





