/*
 * button.c
 *
 *  Created on: 2026. 5. 9.
 *      Author: ADJ
 */

#include "main.h"
#include "global_define.h"

#include "audio_pipeline.h"
#include "adc.h"
#include "bt.h"
#include "visual_renderer.h"

volatile AudioSource_t audioSource = AUDIO_SRC_USB;
volatile uint8_t fftUseEq = 0;
volatile uint8_t i2sUseEq = 0;
volatile uint8_t agcRun = 0;
volatile uint8_t agcOff = 1;

extern TIM_HandleTypeDef htim7;

static uint16_t buttonDebounceTarget;
static uint16_t buttonDebounceCount;

static void ButtonDebounce_TimerStart(void);
static void ButtonDebounce_TimerStop(void);
static void Enable_EXTI(void);
static void Disable_EXTI(void);
static void Confirm_EXTI(void);

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	switch (GPIO_Pin)
	{
		case BTN1_Pin:
			Disable_EXTI();
			buttonDebounceTarget = BTN1_Pin;
			buttonDebounceCount = 0;
			ButtonDebounce_TimerStart();
			break;
		case BTN2_Pin:
			Disable_EXTI();
			buttonDebounceTarget = BTN2_Pin;
			buttonDebounceCount = 0;
			ButtonDebounce_TimerStart();
			break;
		case BTN3_Pin:
			Disable_EXTI();
			buttonDebounceTarget = BTN3_Pin;
			buttonDebounceCount = 0;
			ButtonDebounce_TimerStart();
			break;
		case BTN4_Pin:
			Disable_EXTI();
			buttonDebounceTarget = BTN4_Pin;
			buttonDebounceCount = 0;
			ButtonDebounce_TimerStart();
			break;
		default:
			ButtonDebounce_TimerStop();
			break;
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM7)
	{
		GPIO_PinState raw;

		switch (buttonDebounceTarget)
		{
			case BTN1_Pin:
				raw = HAL_GPIO_ReadPin(BTN1_GPIO_Port, BTN1_Pin);
				break;
			case BTN2_Pin:
				raw = HAL_GPIO_ReadPin(BTN2_GPIO_Port, BTN2_Pin);
				break;
			case BTN3_Pin:
				raw = HAL_GPIO_ReadPin(BTN3_GPIO_Port, BTN3_Pin);
				break;
			case BTN4_Pin:
				raw = HAL_GPIO_ReadPin(BTN4_GPIO_Port, BTN4_Pin);
				break;
			default:
				ButtonDebounce_TimerStop();
				buttonDebounceTarget = 0;
				buttonDebounceCount = 0;
				Enable_EXTI();
				return;
		}

		// BTN3 홀딩, 더블 처리
		if (buttonDebounceTarget == BTN3_Pin)
		{
			// 0~50 첫 버튼 클릭 디바운싱
			if (buttonDebounceCount < 50)
			{
				if (raw == GPIO_PIN_SET)
				{
					buttonDebounceCount++;
				}
				else
				{
					ButtonDebounce_TimerStop();

					agcRun = 0;
					buttonDebounceTarget = 0;
					buttonDebounceCount = 0;

					Enable_EXTI();
				}

				return;
			}

			// 50~119 첫 버튼 클릭 확정 후 홀딩 검사
			// 짧게 누르고 때면 더블클릭 대기 진입
			// 계속 누르고 있으면 120에서 agcRun 1 진입
			if (buttonDebounceCount < 120)
			{
				if (raw == GPIO_PIN_SET)
				{
					buttonDebounceCount++;

					if (buttonDebounceCount >= 120)
					{
						if (!agcOff)
						{
							agcRun = 1;
						}
					}
				}
				else
				{
					agcRun = 0;
					buttonDebounceCount = 121;
				}

				return;
			}

			// 120 홀딩 확정 상태
			// 버튼 누르고 있으면 agcRun 1 유지
			// 버튼 때면 더블클릭 대기 진입
			if (buttonDebounceCount == 120)
			{
				if (raw == GPIO_PIN_SET)
				{
					if (!agcOff)
					{
						agcRun = 1;
					}
					else
					{
						agcRun = 0;
					}
				}
				else
				{
					agcRun = 0;
					buttonDebounceCount = 121;
				}

				return;
			}

			// 121~449 두 번째 클릭 대기
			// 450 클릭 있으면 이동
			if (buttonDebounceCount < 450)
			{
				if (raw == GPIO_PIN_SET)
				{
					// 두 번째 클릭 후보
					buttonDebounceCount = 450;
				}
				else
				{
					buttonDebounceCount++;

					if (buttonDebounceCount >= 450)
					{
						// 더블클릭 없음 = 일반 홀드 종료
						ButtonDebounce_TimerStop();

						agcRun = 0;
						buttonDebounceTarget = 0;
						buttonDebounceCount = 0;

						Enable_EXTI();
					}
				}

				return;
			}

			// 450~499 더블클릭 디바운싱
			if (buttonDebounceCount < 500)
			{
				if (raw == GPIO_PIN_SET)
				{
					buttonDebounceCount++;

					if (buttonDebounceCount >= 500)
					{
						// 더블클릭 확정 AGC ON/OFF 토글
						ButtonDebounce_TimerStop();

						agcOff = !agcOff;
						agcRun = 0;

						buttonDebounceTarget = 0;
						buttonDebounceCount = 0;

						Enable_EXTI();
					}
				}
				else
				{
					// 다시 더블 클릭 검사
					buttonDebounceCount = 121;
				}

				return;
			}

			return;
		}

		// BTN4 단일, 더블 처리
		if (buttonDebounceTarget == BTN4_Pin)
		{
			// 0~50 단일 클릭 디바운싱
			if (buttonDebounceCount < 50)
			{
				if (raw == GPIO_PIN_SET)
				{
					buttonDebounceCount++;

					if (buttonDebounceCount >= 50)
					{
						// Reserved
					}
				}
				else
				{
					ButtonDebounce_TimerStop();
					buttonDebounceTarget = 0;
					buttonDebounceCount = 0;
					Enable_EXTI();
				}

				return;
			}

			// 50~51 단일 클릭 이후 버튼 때기 대기
			if (buttonDebounceCount == 50)
			{
				if (raw == GPIO_PIN_RESET)
				{
					// 버튼을 뗀 뒤부터 더블클릭 대기 시작
					buttonDebounceCount = 51;
				}

				return;
			}

			// ->450 두 번째 클릭 있음
			// 51~449 클릭 없으면 단일 클릭으로 판단
			if (buttonDebounceCount < 450)
			{
				if (raw == GPIO_PIN_SET)
				{
					// 두 번째 클릭 후보 시작
					buttonDebounceCount = 450;
				}
				else
				{
					buttonDebounceCount++;

					if (buttonDebounceCount >= 450)
					{
						// 두 번째 클릭 없음 = 단일 클릭
						ButtonDebounce_TimerStop();

						VisualRenderer_NextMode();

						buttonDebounceTarget = 0;
						buttonDebounceCount = 0;
						Enable_EXTI();
					}
				}

				return;
			}

			// 450~499 두 번째 클릭 디바운스
			if (buttonDebounceCount < 500)
			{
				if (raw == GPIO_PIN_SET)
				{
					buttonDebounceCount++;

					if (buttonDebounceCount >= 500)
					{
						// 두 번째 클릭 확정 = 더블 클릭
						ButtonDebounce_TimerStop();

						VisualRenderer_NextSpectrumTheme();
						VisualRenderer_NextMirrorTheme();

						buttonDebounceTarget = 0;
						buttonDebounceCount = 0;
						Enable_EXTI();
					}
				}
				else
				{
					// 두 번째 클릭인 줄 알았는데 튐이면 다시 대기
					// 두 번째 클릭 검사 다시 시킴
					buttonDebounceCount = 51;
				}

				return;
			}

			return;
		}



		// 단순 글로벌 버튼 처리 로직
		// Confirm_EXTI()로 바로 빠짐
		// 다음 버틈 대기 상태로 복귀
		if (raw == GPIO_PIN_SET)
		{
			buttonDebounceCount++;

			if (buttonDebounceCount >= 50)
			{
				ButtonDebounce_TimerStop();
				Confirm_EXTI();
				buttonDebounceTarget = 0;
				buttonDebounceCount = 0;
				Enable_EXTI();
			}
		}
		else
		{
			ButtonDebounce_TimerStop();
			buttonDebounceTarget = 0;
			buttonDebounceCount = 0;
			Enable_EXTI();
		}
	}
}

static void ButtonDebounce_TimerStart(void)
{
	HAL_TIM_Base_Stop_IT(&htim7);

	__HAL_TIM_SET_COUNTER(&htim7, 0);
	__HAL_TIM_CLEAR_FLAG(&htim7, TIM_FLAG_UPDATE);

	HAL_TIM_Base_Start_IT(&htim7);
}

static void ButtonDebounce_TimerStop(void)
{
	HAL_TIM_Base_Stop_IT(&htim7);

	__HAL_TIM_SET_COUNTER(&htim7, 0);
	__HAL_TIM_CLEAR_FLAG(&htim7, TIM_FLAG_UPDATE);
}

static void Enable_EXTI(void)
{
	__HAL_GPIO_EXTI_CLEAR_IT(BTN1_Pin);
	__HAL_GPIO_EXTI_CLEAR_IT(BTN2_Pin);
	__HAL_GPIO_EXTI_CLEAR_IT(BTN3_Pin);
	__HAL_GPIO_EXTI_CLEAR_IT(BTN4_Pin);

	HAL_NVIC_EnableIRQ(EXTI2_IRQn);
	HAL_NVIC_EnableIRQ(EXTI4_IRQn);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

static void Disable_EXTI(void)
{
	HAL_NVIC_DisableIRQ(EXTI2_IRQn);
	HAL_NVIC_DisableIRQ(EXTI4_IRQn);
	HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
	HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
}

static void Confirm_EXTI(void)
{
	switch (buttonDebounceTarget)
	{
		case BTN1_Pin:
			switch (audioSource)
			{
				case AUDIO_SRC_USB:
					audioSource = AUDIO_SRC_AUX;
					I2S_Stop_BT();
					AudioPipeline_RingClear();
					ADC_Start_Aux();
					break;
				case AUDIO_SRC_AUX:
					audioSource = AUDIO_SRC_BT;
					ADC_Stop_Aux();
					AudioPipeline_RingClear();
					I2S_Start_BT();
					break;
				case AUDIO_SRC_BT:
					ADC_Stop_Aux();
					I2S_Stop_BT();
					AudioPipeline_RingClear();
					audioSource = AUDIO_SRC_USB;
					break;
				default:
					ADC_Stop_Aux();
					I2S_Stop_BT();
					AudioPipeline_RingClear();
					audioSource = AUDIO_SRC_USB;
					break;
			}
			break;
		case BTN2_Pin:
			i2sUseEq = !i2sUseEq;
			break;
		case BTN3_Pin:
			// Reserved
			break;
		case BTN4_Pin:
			VisualRenderer_NextMode();
			break;
		default:
			break;
	}
}
