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

extern RTC_HandleTypeDef hrtc;	// RTC 테스트용 이후에 옮기던지 말던지 할 거임

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
			// RTC 테스트 로직 이후에 지울 예정임
			static uint8_t rtcText[50][4];
			RTC_TimeTypeDef time = {0};
			RTC_DateTypeDef date = {0};

			HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
			HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

			memset(rtcText, 0, sizeof(rtcText));

			rtcText[0][0] = '0' + (time.Hours / 10U);
			rtcText[0][1] = 100U;

			rtcText[1][0] = '0' + (time.Hours % 10U);
			rtcText[1][1] = 100U;

			rtcText[2][0] = '0' + (time.Minutes / 10U);
			rtcText[2][1] = 100U;

			rtcText[3][0] = '0' + (time.Minutes % 10U);
			rtcText[3][1] = 100U;

			WS2812B_Show(TextRenderer_Render(rtcText));
		}
		else if (audioSource == MORE_MOD_TEXT)
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
