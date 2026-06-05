/*
 * app_main.c
 *
 *  Created on: 2026. 5. 7.
 *      Author: ADJ
 */

#include "app_main.h"
#include "main.h"

#include "global_define.h"
#include "audio_pipeline.h"
#include "lcd.h"
#include "adc.h"
#include "dac.h"
#include "eq.h"
#include "fft.h"
#include "visual_renderer.h"
#include "ws2812b.h"

#ifdef WS2812B_DEBUG
static void UART3_SendVisualFrame(void);
#endif

void App_Main(void)
{
	LCD_AppModeInit();

	AudioPipeline_Init();
	EQ_Init();
	FFT_Init();
	VisualRenderer_Init();

	ADC_Start_VReg();
	DAC_OutputStart();
	TIM1_Start();

	#ifdef ADC_DEBUG
	ADC_LogStart();
	#endif

	while (1)
	{
		AudioPipeline_Process();
		//AudioPipeline_Loger();
		LCD_DrawMainScreen();

		if (FFT_Run())
		{
			const float *trail = Visual_GetTrail();
			const float *peakHold = Visual_GetPeak();

			VisualRenderer_Draw(trail, peakHold);
			WS2812B_Show(VisualRenderer_GetFrame());

			#ifdef WS2812B_DEBUG
			UART3_SendVisualFrame();
			#endif
		}
	}
}

/*
 * 디버거 -------------------------------------------------------------------------------------
 */
static volatile uint8_t errorFlags[AUDIO_ERR_COUNT];
static volatile AudioErrorCode_t lastErrorCode;
void Error_Loger(AudioErrorCode_t code)
{
	if (code >= AUDIO_ERR_COUNT)
	{
		return;
	}

	errorFlags[code] = 1;
	lastErrorCode = code;
}

#ifdef WS2812B_DEBUG
extern UART_HandleTypeDef huart3;
static void UART3_SendVisualFrame(void)
{
	const uint8_t *frame = VisualRenderer_GetFrame();

	static const uint8_t header[4] =
	{
		0xAA, 0x55, 0x10, 0x10
	};

	if (frame == NULL)
	{
		return;
	}

	HAL_UART_Transmit(
			&huart3,
			(uint8_t *)header,
			sizeof(header),
			HAL_MAX_DELAY
	);

	HAL_UART_Transmit(
			&huart3,
			(uint8_t *)frame,
			16 * 16 * 3,
			HAL_MAX_DELAY
	);
}
#endif
