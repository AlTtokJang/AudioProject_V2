#ifndef INC_WS2812B_H_
#define INC_WS2812B_H_

#include <stdint.h>

// UART3 출력 디버깅 활성화
// UART 전송량 때문에 MCU 리소스 누수 있음
// 이를 반드시 참고하고 디버깅할 것
// 평소엔 반드시 비활성화 할 것 아니면 소리 깨짐
#define WS2812B_DEBUG

// ================================================================
// MATRIX CONFIG
// ================================================================
#define LED_WIDTH           16U
#define LED_HEIGHT          16U
#define LED_COUNT           (LED_WIDTH * LED_HEIGHT)

// ================================================================
// PWM / TIM CONFIG
// ================================================================
#define PWM_BUF_SIZE        6208U

#define WS2812_BITS_PER_LED 24U

#define WS2812_T0H          90U
#define WS2812_T1H          180U

// ================================================================
// API
// ================================================================
void WS2812B_Show(const uint8_t *frame);

#endif /* INC_WS2812B_H_ */
