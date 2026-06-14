/*
 * adc.c
 *
 *  Created on: 2026. 5. 7.
 *      Author: ADJ
 */

#include "adc.h"
#include "main.h"

#include "global_define.h"
#include "audio_pipeline.h"
#include "spectral_subtraction.h"

#include <stdio.h>
#include <string.h>

#define ADC_SAMPLE_SIZE MASTER_BLOCK_SIZE
#define ADC_BUFFER_SIZE (ADC_SAMPLE_SIZE * 2U)

#if ADC_SAMPLE_SIZE != SS_HOP_SIZE
#error "ADC_SAMPLE_SIZE must be equal to SS_HOP_SIZE for spectral subtraction"
#endif

#define AUX_HALF_RAW_SIZE      ADC_SAMPLE_SIZE
#define AUX_BLOCK_RAW_SIZE     ADC_BUFFER_SIZE
#define AUX_BLOCK_COUNT        2U

#define AUX_MUTE_ENABLE        1U

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc3;
extern DMA_HandleTypeDef hdma_adc1;
extern DMA_HandleTypeDef hdma_adc3;

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim6;

static volatile uint16_t auxBuffer[ADC_BUFFER_SIZE];

/*
 * ADC DMA callback에서는 FFT를 돌리지 않는다.
 * callback에서는 DMA half/full 원본만 auxRawBlock으로 복사하고 ready flag만 세운다.
 */
static volatile uint16_t auxRawBlock[AUX_BLOCK_COUNT][AUX_BLOCK_RAW_SIZE];
static volatile uint8_t auxWriteBlock;
static volatile uint8_t auxReadyBlock;
static volatile uint8_t auxBlockReady;
static volatile uint32_t auxBlockDropCount;

/* AudioPipeline은 stereo interleaved int16을 기대한다. */
static int16_t auxSample[ADC_BUFFER_SIZE];

static float32_t auxSsSrc[2][SS_HOP_SIZE];
static float32_t auxSsDst[2][SS_HOP_SIZE];

static volatile uint16_t vregBuffer[VREG_BUFFER_SIZE];
static uint8_t  vregValue[VREG_BUFFER_SIZE];
static uint16_t vregLpf[VREG_BUFFER_SIZE];

static void ADC_CaptureAuxBlock(uint8_t target);
static void ADC_ProcessAuxBlock(const volatile uint16_t *raw);
static void DigitalFilter_VReg(void);
static int16_t ADC_CountToPcm16(float32_t x);

static volatile uint8_t auxIsRunning;
static volatile uint8_t vregIsRunning;

#ifdef ADC_DEBUG
static void ADC_DebugCaptureAuxBlock(uint8_t target);
#endif

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
	if (hadc->Instance == ADC1)
	{
		if (auxIsRunning)
		{
			ADC_CaptureAuxBlock(0);

			#ifdef ADC_DEBUG
			ADC_DebugCaptureAuxBlock(0);
			#endif
		}
	}
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	if (hadc->Instance == ADC1)
	{
		if (auxIsRunning)
		{
			ADC_CaptureAuxBlock(1);

		#ifdef ADC_DEBUG
		ADC_DebugCaptureAuxBlock(1);
		#endif
		}
	}
	else if (hadc->Instance == ADC3)
	{
		if (vregIsRunning)
		{
			DigitalFilter_VReg();
		}
	}
}

void ADC_Start_Aux(void)
{
	auxIsRunning = 0;

	HAL_TIM_Base_Stop(&htim2);
	HAL_ADC_Stop_DMA(&hadc1);
	HAL_DMA_Abort(&hdma_adc1);

	__HAL_TIM_SET_COUNTER(&htim2, 0);
	__HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);

	for (uint16_t i = 0; i < ADC_BUFFER_SIZE; i++)
	{
		auxBuffer[i] = 0;
		auxSample[i] = 0;
	}

	memset((void *)auxRawBlock, 0, sizeof(auxRawBlock));
	memset(auxSsSrc, 0, sizeof(auxSsSrc));
	memset(auxSsDst, 0, sizeof(auxSsDst));

	auxWriteBlock = 0;
	auxReadyBlock = 0;
	auxBlockReady = 0;
	auxBlockDropCount = 0;

	SpectralSub_Init();

	if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)auxBuffer, ADC_BUFFER_SIZE) != HAL_OK)
	{
		Error_Loger(AUDIO_ERR_ADC_AUX_START_FAIL);
		auxIsRunning = 0;
		return;
	}

	if (HAL_TIM_Base_Start(&htim2) != HAL_OK)
	{
		Error_Loger(AUDIO_ERR_ADC_AUX_TIM_START_FAIL);

		HAL_ADC_Stop_DMA(&hadc1);
		HAL_DMA_Abort(&hdma_adc1);

		auxIsRunning = 0;
		return;
	}

	auxIsRunning = 1;
}

void ADC_Stop_Aux(void)
{
	auxIsRunning = 0;

	if (HAL_TIM_Base_Stop(&htim2) != HAL_OK)
	{
		Error_Loger(AUDIO_ERR_ADC_AUX_TIM_STOP_FAIL);
	}

	if (HAL_ADC_Stop_DMA(&hadc1) != HAL_OK)
	{
		Error_Loger(AUDIO_ERR_ADC_AUX_STOP_FAIL);
	}

	HAL_DMA_Abort(&hdma_adc1);

	__HAL_TIM_SET_COUNTER(&htim2, 0);
	__HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
}

void ADC_Start_VReg(void)
{
	vregIsRunning = 0;

	HAL_TIM_Base_Stop(&htim6);
	HAL_ADC_Stop_DMA(&hadc3);
	HAL_DMA_Abort(&hdma_adc3);

	__HAL_TIM_SET_COUNTER(&htim6, 0);
	__HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);

	for (uint8_t i = 0; i < VREG_BUFFER_SIZE; i++)
	{
		vregBuffer[i] = 0;
		vregValue[i] = 0;
		vregLpf[i] = 0;
	}

	if (HAL_ADC_Start_DMA(&hadc3, (uint32_t *)vregBuffer, VREG_BUFFER_SIZE) != HAL_OK)
	{
		Error_Loger(AUDIO_ERR_ADC_VREG_START_FAIL);
		vregIsRunning = 0;
		return;
	}

	if (HAL_TIM_Base_Start(&htim6) != HAL_OK)
	{
		Error_Loger(AUDIO_ERR_ADC_VREG_TIM_START_FAIL);

		HAL_ADC_Stop_DMA(&hadc3);
		HAL_DMA_Abort(&hdma_adc3);

		vregIsRunning = 0;
		return;
	}

	vregIsRunning = 1;
}

void ADC_Stop_VReg(void)
{
	vregIsRunning = 0;

	if (HAL_TIM_Base_Stop(&htim6) != HAL_OK)
	{
		Error_Loger(AUDIO_ERR_ADC_VREG_TIM_STOP_FAIL);
	}

	if (HAL_ADC_Stop_DMA(&hadc3) != HAL_OK)
	{
		Error_Loger(AUDIO_ERR_ADC_VREG_STOP_FAIL);
	}

	HAL_DMA_Abort(&hdma_adc3);

	__HAL_TIM_SET_COUNTER(&htim6, 0);
	__HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
}

void ADC_GetValue_EQ(uint8_t *out)
{
	if (out == NULL)
	{
		return;
	}

	for (uint8_t i = 0; i < 3; i++)
	{
		out[i] = vregValue[i + 1];
	}
}

void ADC_GetValue_VOL(uint8_t *out)
{
	if (out == NULL)
	{
		return;
	}

	*out = vregValue[0];
}

/*
 * App_Main while(1)에서 최대한 자주 호출한다.
 * ADC DMA callback 안에서는 FFT를 돌리지 않고, 여기서만 SpectralSub_Process를 실행한다.
 */
void ADC_Task_Aux(void)
{
	uint8_t readBlock;

	if (auxBlockReady == 0)
	{
		return;
	}

	readBlock = auxReadyBlock;
	ADC_ProcessAuxBlock(auxRawBlock[readBlock]);

	__disable_irq();
	auxBlockReady = 0;
	__enable_irq();
}

static void ADC_CaptureAuxBlock(uint8_t target)
{
	uint16_t dmaOffset;
	uint16_t blockOffset;
	uint8_t writeBlock;

	writeBlock = auxWriteBlock;

	if (target == 0)
	{
		dmaOffset = 0;
		blockOffset = 0;
	}
	else
	{
		dmaOffset = AUX_HALF_RAW_SIZE;
		blockOffset = AUX_HALF_RAW_SIZE;
	}

	for (uint16_t i = 0; i < AUX_HALF_RAW_SIZE; i++)
	{
		auxRawBlock[writeBlock][blockOffset + i] = auxBuffer[dmaOffset + i];
	}

	if (target == 0)
	{
		return;
	}

	if (auxBlockReady == 0)
	{
		auxReadyBlock = writeBlock;
		auxBlockReady = 1;
		auxWriteBlock = writeBlock ^ 1U;
	}
	else
	{
		auxBlockDropCount++;
	}
}

static void ADC_ProcessAuxBlock(const volatile uint16_t *raw)
{
	#define AUX_MUTE_ENTER_LEVEL	3
	#define AUX_MUTE_EXIT_LEVEL		6
	#define AUX_MUTE_HOLD_BLOCKS	30

	#define AUX_GAIN_MAX			32767
	#define AUX_GAIN_STEP			32

	static uint16_t muteHoldCount = 0;
	static uint8_t muteActive = 1;
	static int32_t softGain = AUX_GAIN_MAX;

	uint32_t absSum = 0;
	uint32_t avgAbs;

	/* raw: L0,R0,L1,R1... 총 256 stereo frames */
	for (uint16_t i = 0; i < SS_HOP_SIZE; i++)
	{
		uint16_t rawIndex = i * 2U;

		auxSsSrc[0][i] = (float32_t)raw[rawIndex + 0U] - 2048.0f;
		auxSsSrc[1][i] = (float32_t)raw[rawIndex + 1U] - 2048.0f;
	}

	SpectralSub_Process(auxSsSrc, auxSsDst);

	for (uint16_t i = 0; i < SS_HOP_SIZE; i++)
	{
		float32_t mixed;
		int16_t absSample;

		mixed = (auxSsDst[0][i] + auxSsDst[1][i]) * 0.5f;

		if (mixed < 0.0f)
			absSample = (int16_t)(-mixed);
		else
			absSample = (int16_t)mixed;

		absSum += absSample;
	}

	avgAbs = absSum / SS_HOP_SIZE;

	if (avgAbs < AUX_MUTE_ENTER_LEVEL)
	{
		if (muteHoldCount < AUX_MUTE_HOLD_BLOCKS)
			muteHoldCount++;
	}
	else if (avgAbs > AUX_MUTE_EXIT_LEVEL)
	{
		muteHoldCount = 0;
		muteActive = 0;
	}

	if (muteHoldCount >= AUX_MUTE_HOLD_BLOCKS)
		muteActive = 1;

	#if (AUX_MUTE_ENABLE == 0U)
	muteActive = 0;
	softGain = AUX_GAIN_MAX;
	#endif

	for (uint16_t i = 0; i < SS_HOP_SIZE; i++)
	{
		float32_t l;
		float32_t r;
		int32_t gain;

		if (muteActive)
		{
			if (softGain > AUX_GAIN_STEP)
				softGain -= AUX_GAIN_STEP;
			else
				softGain = 0;
		}
		else
		{
			if (softGain < AUX_GAIN_MAX - AUX_GAIN_STEP)
				softGain += AUX_GAIN_STEP;
			else
				softGain = AUX_GAIN_MAX;
		}

		gain = softGain;

		l = auxSsDst[0][i] * (float32_t)gain / 32767.0f;
		r = auxSsDst[1][i] * (float32_t)gain / 32767.0f;

		auxSample[i * 2U + 0U] = ADC_CountToPcm16(l);
		auxSample[i * 2U + 1U] = ADC_CountToPcm16(r);
	}

	/* 256 stereo frames = 512 int16 samples를 push해야 한다. */
	AudioPipeline_Push(auxSample, ADC_BUFFER_SIZE);

	#undef AUX_MUTE_ENTER_LEVEL
	#undef AUX_MUTE_EXIT_LEVEL
	#undef AUX_MUTE_HOLD_BLOCKS
	#undef AUX_GAIN_MAX
	#undef AUX_GAIN_STEP
}

static int16_t ADC_CountToPcm16(float32_t x)
{
	int32_t y;

	/* raw - 2048 기준 12bit count를 16bit PCM 스케일로 확장 */
	x *= 16.0f;

	if (x > 32767.0f)
		x = 32767.0f;
	else if (x < -32768.0f)
		x = -32768.0f;

	if (x >= 0.0f)
		y = (int32_t)(x + 0.5f);
	else
		y = (int32_t)(x - 0.5f);

	return (int16_t)y;
}

static void DigitalFilter_VReg(void)
{
	for (uint8_t i = 0; i < VREG_BUFFER_SIZE; i++)
	{
		uint16_t raw = vregBuffer[i];
		uint16_t targetLpf;

		if (raw <= 50)
		{
			// 0~50 -> 0~6700
			targetLpf = (raw * 6700U + 25U) / 50U;
		}
		else
		{
			// 50~255 -> 6700~10000
			targetLpf = 6700U + (((raw - 50U) * 3300U + 102U) / 205U);
		}

		if (targetLpf > vregLpf[i])
		{
			uint16_t diff = targetLpf - vregLpf[i];
			vregLpf[i] += (diff + 3U) >> 2;
		}
		else
		{
			uint16_t diff = vregLpf[i] - targetLpf;
			vregLpf[i] -= (diff + 3U) >> 2;
		}

		vregValue[i] = (vregLpf[i] + 50U) / 100U;

		if (vregValue[i] > 100U)
		{
			vregValue[i] = 100U;
		}
	}
}

/*
 * 디버거 -------------------------------------------------------------------------------------
 */

#ifdef ADC_DEBUG

#define AUX_CAPTURE_PAIRS		81920U
#define AUX_CAPTURE_VALUES		(AUX_CAPTURE_PAIRS * 2U)

static uint16_t auxCapture[AUX_CAPTURE_VALUES];
static volatile uint32_t auxCaptureIndex = 0;
static volatile uint8_t auxCaptureRunning = 0;
static volatile uint8_t auxCaptureDone = 0;

static void ADC_DebugStartAuxCapture(void)
{
	auxCaptureIndex = 0;
	auxCaptureDone = 0;
	auxCaptureRunning = 1;
}

static uint8_t ADC_DebugIsAuxCaptureDone(void)
{
	return auxCaptureDone;
}

static void ADC_DebugCaptureAuxBlock(uint8_t target)
{
	uint16_t offset;

	if (!auxCaptureRunning)
	{
		return;
	}

	if (auxCaptureDone)
	{
		return;
	}

	if (target == 0)
	{
		offset = 0;
	}
	else
	{
		offset = ADC_SAMPLE_SIZE;
	}

	for (uint16_t i = 0; i < ADC_SAMPLE_SIZE; i++)
	{
		if (auxCaptureIndex >= AUX_CAPTURE_VALUES)
		{
			auxCaptureRunning = 0;
			auxCaptureDone = 1;
			return;
		}

		auxCapture[auxCaptureIndex] = auxBuffer[offset + i];
		auxCaptureIndex++;
	}

	if (auxCaptureIndex >= AUX_CAPTURE_VALUES)
	{
		auxCaptureRunning = 0;
		auxCaptureDone = 1;
	}
}

static void ADC_DebugDumpAuxCaptureCsv(void)
{
	char line[48];

	UART3_Print("n,left,right\r\n");

	for (uint32_t n = 0; n < AUX_CAPTURE_PAIRS; n++)
	{
		uint16_t left = auxCapture[n * 2U];
		uint16_t right = auxCapture[n * 2U + 1U];

		snprintf(line, sizeof(line), "%lu,%u,%u\r\n", n, left, right);
		UART3_Print(line);
	}
}

void ADC_LogStart(void)
{
	ADC_Start_Aux();
	ADC_DebugStartAuxCapture();

	while (!ADC_DebugIsAuxCaptureDone())
	{
	}

	ADC_Stop_Aux();
	ADC_DebugDumpAuxCaptureCsv();

	while (1)
	{
	}
}

#endif
