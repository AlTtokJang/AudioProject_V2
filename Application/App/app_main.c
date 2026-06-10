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
#include "ws2812b_text_renderer.h"
#include "usb_hid_text.h"

extern volatile AudioSource_t audioSource;

void App_Main(void)
{
	HIDText_LoadFromFlash();

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
		LCD_DrawMainScreen();

		if (audioSource == MORE_MOD_CLOCK)
		{

		}
		else if (audiioSource == MORE_MOD_TEXT)
		{
			WS2812B_Show(TextRenderer_Render(HIDText_GetText()));
		}
		else
		{
			AudioPipeline_Process();
			AudioPipeline_Loger();

			if (FFT_Run())
			{
				const float *trail = Visual_GetTrail();
				const float *peakHold = Visual_GetPeak();

				VisualRenderer_Draw(trail, peakHold);
				WS2812B_Show(VisualRenderer_GetFrame());
			}
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
