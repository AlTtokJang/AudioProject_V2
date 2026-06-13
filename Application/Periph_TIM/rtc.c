/*
 * rtc.c
 *
 *  Created on: 2026. 6. 13.
 *      Author: ADJ
 */

#include "rtc.h"

typedef enum
{
	LED_CLOCK = 0,
	LED_YEAR,
	LED_MONTH,
	LED_DATE,
	LED_WEEKDAY
} RTC_LedShowTarget;

typedef enum
{
	LED_RED = 0,
	LED_GREEN,
	LED_BLUE
} RTC_LedColor;

static const char *monthText[12] = {
	"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
	"JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};

static const char *weekText[7] = {
	"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"
};

extern RTC_HandleTypeDef hrtc;

static RTC_TimeTypeDef s_time = {0};
static RTC_DateTypeDef s_date = {0};

static RTC_ModifyTarget s_modifyTarget;
static RTC_LedShowTarget s_ledShowTarget;
static RTC_LedColor s_ledColor;

static uint8_t s_ledTextColor[50][4];

static uint32_t s_prevTick;
static uint8_t s_ledFlag;	// 수정 타겟 점멸

static void RTC_UpdateTime(void)
{
	HAL_RTC_GetTime(&hrtc, &s_time, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &s_date, RTC_FORMAT_BIN);
}

static uint8_t RTC_DaysInMonth(uint8_t year, uint8_t month)
{
	static const uint8_t days[12] = {
		31,28,31,30,31,30,31,31,30,31,30,31
	};

	uint16_t fullYear = 2000 + year;

	if (month == 2) {
		if ((fullYear % 4) == 0)
			return 29;
	}

	return days[month - 1];
}

static uint8_t RTC_CalcWeekDay(uint16_t year, uint8_t month, uint8_t day)
{
	if (month < 3) {
		month += 12;
		year--;
	}

	uint16_t k = year % 100;
	uint16_t j = year / 100;

	uint8_t h = (day + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;

	switch (h) {
		case 0: return RTC_WEEKDAY_SATURDAY;
		case 1: return RTC_WEEKDAY_SUNDAY;
		case 2: return RTC_WEEKDAY_MONDAY;
		case 3: return RTC_WEEKDAY_TUESDAY;
		case 4: return RTC_WEEKDAY_WEDNESDAY;
		case 5: return RTC_WEEKDAY_THURSDAY;
		case 6: return RTC_WEEKDAY_FRIDAY;
		default: return RTC_WEEKDAY_MONDAY;
	}
}

static void RTC_NormalizeDate(void)
{
	uint8_t maxDate = RTC_DaysInMonth(s_date.Year, s_date.Month);

	if (s_date.Date > maxDate)
		s_date.Date = maxDate;

	s_date.WeekDay = RTC_CalcWeekDay(2000 + s_date.Year, s_date.Month, s_date.Date);
}

// --------------------------------------------------------------------
// 플래시 저장
#define RTC_FLASH_SECTOR	FLASH_SECTOR_10
#define RTC_FLASH_ADDR		0x08180000U
#define RTC_FLASH_SIZE		(256U * 1024U)
#define RTC_FLASH_MAGIC		0x52544331U

typedef struct
{
	uint32_t magic;
	uint32_t seq;

	uint8_t hours;
	uint8_t minutes;
	uint8_t year;
	uint8_t month;
	uint8_t date;
	uint8_t reserved[3];

	uint32_t sum;
} RTC_FlashRecord_t;

static uint32_t s_rtcSeq = 1U;

static uint32_t RTC_FlashSum(const RTC_FlashRecord_t *r)
{
	return
		r->seq +
		r->hours +
		r->minutes +
		r->year +
		r->month +
		r->date;
}

void RTC_LoadFromFlash(void)
{
	const RTC_FlashRecord_t *latest = 0;
	uint32_t latestSeq = 0U;

	for (uint32_t addr = RTC_FLASH_ADDR;
		 addr + sizeof(RTC_FlashRecord_t) <= RTC_FLASH_ADDR + RTC_FLASH_SIZE;
		 addr += sizeof(RTC_FlashRecord_t))
	{
		const RTC_FlashRecord_t *r = (const RTC_FlashRecord_t *)addr;

		if (r->magic == 0xFFFFFFFFU)
			break;

		if ((r->magic == RTC_FLASH_MAGIC) &&
			(r->sum == RTC_FlashSum(r)) &&
			(r->seq >= latestSeq))
		{
			latest = r;
			latestSeq = r->seq;
		}
	}

	if (latest == 0)
		return;

	s_time.Hours = latest->hours;
	s_time.Minutes = latest->minutes;
	s_time.Seconds = 0;
	s_time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	s_time.StoreOperation = RTC_STOREOPERATION_RESET;

	s_date.Year = latest->year;
	s_date.Month = latest->month;
	s_date.Date = latest->date;
	RTC_NormalizeDate();

	HAL_RTC_SetTime(&hrtc, &s_time, RTC_FORMAT_BIN);
	HAL_RTC_SetDate(&hrtc, &s_date, RTC_FORMAT_BIN);

	s_rtcSeq = latestSeq + 1U;
}

static void RTC_SaveToFlash(void)
{
	RTC_FlashRecord_t r;
	FLASH_EraseInitTypeDef e;
	uint32_t err;
	uint32_t addr = 0U;
	uint8_t ok = 1U;

	for (uint32_t a = RTC_FLASH_ADDR;
		 a + sizeof(RTC_FlashRecord_t) <= RTC_FLASH_ADDR + RTC_FLASH_SIZE;
		 a += sizeof(RTC_FlashRecord_t))
	{
		if (((const RTC_FlashRecord_t *)a)->magic == 0xFFFFFFFFU)
		{
			addr = a;
			break;
		}
	}

	HAL_FLASH_Unlock();

	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP |
						   FLASH_FLAG_OPERR |
						   FLASH_FLAG_WRPERR |
						   FLASH_FLAG_PGAERR |
						   FLASH_FLAG_PGPERR |
						   FLASH_FLAG_ERSERR);

	if (addr == 0U)
	{
		e.TypeErase = FLASH_TYPEERASE_SECTORS;
		e.VoltageRange = FLASH_VOLTAGE_RANGE_3;
		e.Sector = RTC_FLASH_SECTOR;
		e.NbSectors = 1;

		if (HAL_FLASHEx_Erase(&e, &err) != HAL_OK)
		{
			HAL_FLASH_Lock();
			return;
		}

		addr = RTC_FLASH_ADDR;
	}

	memset(&r, 0xFF, sizeof(r));

	r.magic = RTC_FLASH_MAGIC;
	r.seq = s_rtcSeq;
	r.hours = s_time.Hours;
	r.minutes = s_time.Minutes;
	r.year = s_date.Year;
	r.month = s_date.Month;
	r.date = s_date.Date;
	r.sum = RTC_FlashSum(&r);

	for (uint32_t i = 0U; i < sizeof(r); i += 4U)
	{
		uint32_t word;

		memcpy(&word, ((uint8_t *)&r) + i, 4U);

		if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK)
		{
			ok = 0U;
			break;
		}
	}

	if (ok != 0U)
		s_rtcSeq++;

	HAL_FLASH_Lock();
}
// --------------------------------------------------------------------

void RTC_ModifyIncrease(void)
{
	switch (s_modifyTarget) {
		case RTC_HOUR:
			s_time.Hours = (s_time.Hours + 1) % 24;
			break;

		case RTC_MINUTE:
			s_time.Minutes = (s_time.Minutes + 1) % 60;
			break;

		case RTC_YEAR:
			s_date.Year = (s_date.Year + 1) % 100;
			RTC_NormalizeDate();
			break;

		case RTC_MONTH:
			s_date.Month++;
			if (s_date.Month > 12)
				s_date.Month = 1;
			RTC_NormalizeDate();
			break;

		case RTC_DATE:
			s_date.Date++;
			if (s_date.Date > RTC_DaysInMonth(s_date.Year, s_date.Month))
				s_date.Date = 1;
			RTC_NormalizeDate();
			break;

		default:
			return;
	}

	HAL_RTC_SetTime(&hrtc, &s_time, RTC_FORMAT_BIN);
	HAL_RTC_SetDate(&hrtc, &s_date, RTC_FORMAT_BIN);
}

void RTC_ModifyDecrease(void)
{
	switch (s_modifyTarget) {
		case RTC_HOUR:
			s_time.Hours = (s_time.Hours == 0) ? 23 : s_time.Hours - 1;
			break;

		case RTC_MINUTE:
			s_time.Minutes = (s_time.Minutes == 0) ? 59 : s_time.Minutes - 1;
			break;

		case RTC_YEAR:
			s_date.Year = (s_date.Year == 0) ? 99 : s_date.Year - 1;
			RTC_NormalizeDate();
			break;

		case RTC_MONTH:
			s_date.Month = (s_date.Month <= 1) ? 12 : s_date.Month - 1;
			RTC_NormalizeDate();
			break;

		case RTC_DATE:
			if (s_date.Date <= 1)
				s_date.Date = RTC_DaysInMonth(s_date.Year, s_date.Month);
			else
				s_date.Date--;

			RTC_NormalizeDate();
			break;

		default:
			return;
	}

	HAL_RTC_SetTime(&hrtc, &s_time, RTC_FORMAT_BIN);
	HAL_RTC_SetDate(&hrtc, &s_date, RTC_FORMAT_BIN);
}

void RTC_MakeModifyTargetNone(void)
{
	RTC_SaveToFlash();
	s_modifyTarget = RTC_NONE;
}

void RTC_NextModifyTarget(void)
{
	switch (s_modifyTarget)
	{
		case RTC_NONE:
			s_modifyTarget = RTC_HOUR;
			s_ledShowTarget = LED_CLOCK;
			break;
		case RTC_HOUR:
			s_modifyTarget = RTC_MINUTE;
			s_ledShowTarget = LED_CLOCK;
			break;
		case RTC_MINUTE:
			s_modifyTarget = RTC_YEAR;
			s_ledShowTarget = LED_YEAR;
			break;
		case RTC_YEAR:
			s_modifyTarget = RTC_MONTH;
			s_ledShowTarget = LED_MONTH;
			break;
		case RTC_MONTH:
			s_modifyTarget = RTC_DATE;
			s_ledShowTarget = LED_DATE;
			break;
		case RTC_DATE:
			RTC_MakeModifyTargetNone();
			s_ledShowTarget = LED_CLOCK;
			break;
		default:
			break;
	}
}

RTC_ModifyTarget RTC_WhatAreYouModifying(void)
{
	return s_modifyTarget;
}

uint8_t RTC_ShouldIBlink(void)
{
	return s_ledFlag;
}

void RTC_GetTimeStruct(RTC_TimeTypeDef *time)
{
	*time = s_time;
}

void RTC_GetDateStruct(RTC_DateTypeDef *date)
{
	*date = s_date;
}

// 반복 실행
void RTC_FlashSaveTask(void)
{
	static uint8_t prevMinute = 0xFF;

	if (s_modifyTarget != RTC_NONE)
		return;

	RTC_UpdateTime();

	if (s_time.Minutes != prevMinute) {
		prevMinute = s_time.Minutes;
		RTC_SaveToFlash();
	}
}

void RTC_NextShowOnLed(void)
{
	if (s_ledShowTarget < LED_WEEKDAY)
		s_ledShowTarget++;
	else
		s_ledShowTarget = LED_CLOCK;
}

void RTC_NextColorOnLed(void)
{
	if (s_ledColor < LED_BLUE)
		s_ledColor++;
	else
		s_ledColor = LED_RED;
}

const uint8_t (*RTC_GetText(void))[4]
{
	if (HAL_GetTick() - s_prevTick >= 500)
	{
		s_prevTick = HAL_GetTick();
		s_ledFlag = !s_ledFlag;
	}

	memset(s_ledTextColor, 0, sizeof(s_ledTextColor));

	for (uint8_t i = 0; i < 50; i++)
	{
		switch (s_ledColor)
		{
			case LED_RED:
				s_ledTextColor[i][1] = 100;
				break;
			case LED_GREEN:
				s_ledTextColor[i][2] = 100;
				break;
			case LED_BLUE:
				s_ledTextColor[i][3] = 100;
				break;
			default:
				break;
		}
	}

	if (s_ledFlag && s_modifyTarget != RTC_NONE)
	{
		switch (s_modifyTarget)
		{
			case RTC_HOUR:
				for (uint8_t i = 0; i < 9; i++)
					s_ledTextColor[i][0] = ' ';
				s_ledTextColor[9][0] = '0' + (s_time.Minutes / 10);
				s_ledTextColor[10][0] = ' ';
				s_ledTextColor[11][0] = '0' + (s_time.Minutes % 10);
				break;
			case RTC_MINUTE:
				s_ledTextColor[0][0] = '0' + (s_time.Hours / 10);
				s_ledTextColor[1][0] = ' ';
				s_ledTextColor[2][0] = '0' + (s_time.Hours % 10);
				for (uint8_t i = 3; i < 12; i++)
					s_ledTextColor[i][0] = ' ';
				break;
			default:
				break;
		}

		return s_ledTextColor;
	}
	else
	{
		switch (s_ledShowTarget)
		{
			case LED_CLOCK:
				s_ledTextColor[0][0] = '0' + (s_time.Hours / 10);
				s_ledTextColor[1][0] = ' ';
				s_ledTextColor[2][0] = '0' + (s_time.Hours % 10);
				s_ledTextColor[3][0] = ' ';
				s_ledTextColor[4][0] = ' ';
				s_ledTextColor[5][0] = '0' + (s_time.Minutes / 10);
				s_ledTextColor[6][0] = ' ';
				s_ledTextColor[7][0] = '0' + (s_time.Minutes % 10);
				break;
			case LED_YEAR:
				s_ledTextColor[0][0] = '2';
				s_ledTextColor[1][0] = ' ';
				s_ledTextColor[2][0] = '0';
				s_ledTextColor[3][0] = ' ';
				s_ledTextColor[4][0] = ' ';
				s_ledTextColor[5][0] = '0' + (s_date.Year / 10);
				s_ledTextColor[6][0] = ' ';
				s_ledTextColor[7][0] = '0' + (s_date.Year % 10);
				break;
			case LED_MONTH:
				const char *m = monthText[s_date.Month - 1];

				s_ledTextColor[0][0] = m[0];
				s_ledTextColor[1][0] = ' ';
				s_ledTextColor[2][0] = m[1];
				s_ledTextColor[3][0] = ' ';
				s_ledTextColor[4][0] = m[2];
				break;
			case LED_DATE:
				if (s_date.Date > 9)
				{
					s_ledTextColor[0][0] = '0' + (s_date.Date / 10);
					s_ledTextColor[1][0] = ' ';
					s_ledTextColor[2][0] = '0' + (s_date.Date % 10);
				}
				else
				{
					s_ledTextColor[0][0] = '0' + (s_date.Date);
				}
				break;
			case LED_WEEKDAY:
				const char *w = weekText[s_date.WeekDay - 1];

				s_ledTextColor[0][0] = w[0];
				s_ledTextColor[1][0] = ' ';
				s_ledTextColor[2][0] = w[1];
				s_ledTextColor[3][0] = ' ';
				s_ledTextColor[4][0] = w[2];
				break;
			default:
				break;
		}
	}

	return s_ledTextColor;
}
