/*
 * usb_hid_text.c
 *
 *  Created on: 2026. 6. 10.
 *      Author: ADJ
 */

#include "usb_hid_text.h"

#include "stm32f7xx_hal.h"

#include <string.h>

#define HID_TEXT_MAX_LEN		50U
#define HID_TEXT_CMD_BLOCK0		0xC1U	// 0~14
#define HID_TEXT_CMD_BLOCK1		0xC2U	// 15~29
#define HID_TEXT_CMD_BLOCK2		0xC3U	// 30~44
#define HID_TEXT_CMD_BLOCK3		0xC4U	// 45~49

static uint8_t s_hidTextColor[HID_TEXT_MAX_LEN][4];

// --------------------------------------------------------------------
// 플래시 저장
#define HID_TEXT_FLASH_SECTOR	FLASH_SECTOR_11
#define HID_TEXT_FLASH_ADDR		0x081C0000U
#define HID_TEXT_FLASH_SIZE		(256U * 1024U)
#define HID_TEXT_FLASH_MAGIC	0x54455831U

typedef struct
{
	uint32_t magic;								// 저장 record 맞는지 확인하는 값
	uint32_t seq;								// 최신 record 확인 값
	uint8_t text[HID_TEXT_MAX_LEN][4];			// 저장 데이터
	uint32_t sum;								// 체크섬
} HIDTextFlashRecord_t;

static uint32_t s_hidTextSeq = 1U;				// 부팅 후 최신 seq로 변경됨

static uint32_t HIDText_Sum(uint32_t seq, const uint8_t text[HID_TEXT_MAX_LEN][4])
{
	uint32_t sum = seq;

	for (uint32_t i = 0U; i < HID_TEXT_MAX_LEN; i++)
	{
		sum += text[i][0] + text[i][1] + text[i][2] + text[i][3];
	}

	return sum;
}

void HIDText_LoadFromFlash(void)
{
	const HIDTextFlashRecord_t *latest = 0;	// 최신 record 포인터
	uint32_t latestSeq = 0U;				// 최신 seq

	for (uint32_t addr = HID_TEXT_FLASH_ADDR; addr + sizeof(HIDTextFlashRecord_t) <= HID_TEXT_FLASH_ADDR + HID_TEXT_FLASH_SIZE; addr += sizeof(HIDTextFlashRecord_t))
	{
		const HIDTextFlashRecord_t *r = (const HIDTextFlashRecord_t *)addr;

		if (r->magic == 0xFFFFFFFFU)
		{
			break;
		}

		if ((r->magic == HID_TEXT_FLASH_MAGIC) &&
			(r->sum == HIDText_Sum(r->seq, r->text)) &&
			(r->seq >= latestSeq))
		{
			latest = r;
			latestSeq = r->seq;
		}
	}

	if (latest == 0)
	{
		return;
	}

	memcpy(s_hidTextColor, latest->text, HID_TEXT_MAX_LEN * 4U);
	s_hidTextSeq = latestSeq + 1U;
}

static void HIDText_SaveToFlash(void)
{
	HIDTextFlashRecord_t r;		// 새로 flash에 쓸 record
	FLASH_EraseInitTypeDef e;	// sector erase 설정 구조체
	uint32_t err;				// erase 실패 시 에러 sector 번호
	uint32_t addr = 0U;			// 이번에 저장할 flash 주소
	uint8_t ok = 1U;			// write 성공 여부

	for (uint32_t a = HID_TEXT_FLASH_ADDR; a + sizeof(HIDTextFlashRecord_t) <= HID_TEXT_FLASH_ADDR + HID_TEXT_FLASH_SIZE; a += sizeof(HIDTextFlashRecord_t))
	{
		if (((const HIDTextFlashRecord_t *)a)->magic == 0xFFFFFFFFU)
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

	if (addr == 0U)	// 빈 공간 없으면 sector erase 수행
	{
		e.TypeErase = FLASH_TYPEERASE_SECTORS;
		e.VoltageRange = FLASH_VOLTAGE_RANGE_3;
		e.Sector = HID_TEXT_FLASH_SECTOR;
		e.NbSectors = 1;

		if (HAL_FLASHEx_Erase(&e, &err) != HAL_OK)	// 지우기 실패하면 flash 잠구고 저장 포기
		{
			HAL_FLASH_Lock();
			return;
		}

		addr = HID_TEXT_FLASH_ADDR;	// erase 후 처음 시작 주소로 저장
	}

	memset(&r, 0xFF, sizeof(r));
	r.magic = HID_TEXT_FLASH_MAGIC;
	r.seq = s_hidTextSeq;
	memcpy(r.text, s_hidTextColor, HID_TEXT_MAX_LEN * 4U);
	r.sum = HIDText_Sum(r.seq, r.text);

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

	if (ok != 0U)		// 쓰기 최종 완료시 seq 증가
	{
		s_hidTextSeq++;
	}

	HAL_FLASH_Lock();
}
// --------------------------------------------------------------------

void HIDText_SetTextFromReport(const uint8_t *report, uint16_t len)
{
	if ((report == 0) || (len == 0U))
	{
		return;
	}

	if (report[0] == HID_TEXT_CMD_BLOCK0)
	{
		if (len >= (1U + (15U * 4U)))
		{
			memcpy(&s_hidTextColor[0][0], &report[1], 15U * 4U);
		}

		return;
	}

	if (report[0] == HID_TEXT_CMD_BLOCK1)
	{
		if (len >= (1U + (15U * 4U)))
		{
			memcpy(&s_hidTextColor[15][0], &report[1], 15U * 4U);
		}

		return;
	}

	if (report[0] == HID_TEXT_CMD_BLOCK2)
	{
		if (len >= (1U + (15U * 4U)))
		{
			memcpy(&s_hidTextColor[30][0], &report[1], 15U * 4U);
		}

		return;
	}

	if (report[0] == HID_TEXT_CMD_BLOCK3)
	{
		if (len >= (1U + (5U * 4U)))
		{
			memcpy(&s_hidTextColor[45][0], &report[1], 5U * 4U);
			HIDText_SaveToFlash();
		}

		return;
	}
}

const uint8_t (*HIDText_GetText(void))[4]
{
	return s_hidTextColor;
}
