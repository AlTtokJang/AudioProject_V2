/*
 * usb_hid_text.h
 *
 *  Created on: 2026. 6. 10.
 *      Author: ADJ
 */

#ifndef USB_HID_TEXT_H_
#define USB_HID_TEXT_H_

#include <stdint.h>

void HIDText_SetTextFromReport(const uint8_t *report, uint16_t len);
const uint8_t (*HIDText_GetText(void))[4];

void HIDText_LoadFromFlash(void);

#endif /* USB_HID_TEXT_H_ */
