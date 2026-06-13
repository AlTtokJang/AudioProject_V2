/*
 * rtc.h
 *
 *  Created on: 2026. 6. 13.
 *      Author: ADJ
 */

#ifndef PERIPH_TIM_RTC_H_
#define PERIPH_TIM_RTC_H_

#include "main.h"
#include <stdint.h>

typedef enum
{
	RTC_NONE = 0,
	RTC_HOUR,
	RTC_MINUTE,
	RTC_YEAR,
	RTC_MONTH,
	RTC_DATE
} RTC_ModifyTarget;

void RTC_LoadFromFlash(void);
void RTC_ModifyIncrease(void);
void RTC_ModifyDecrease(void);
void RTC_MakeModifyTargetNone(void);
void RTC_NextModifyTarget(void);
RTC_ModifyTarget RTC_WhatAreYouModifying(void);
uint8_t RTC_ShouldIBlink(void);
void RTC_GetTimeStruct(RTC_TimeTypeDef *time);
void RTC_GetDateStruct(RTC_DateTypeDef *date);
void RTC_FlashSaveTask(void);
void RTC_NextShowOnLed(void);
void RTC_NextColorOnLed(void);
const uint8_t (*RTC_GetText(void))[4];

#endif /* PERIPH_TIM_RTC_H_ */
