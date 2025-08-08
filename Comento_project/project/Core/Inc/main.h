#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* ===== 상태 타입 & 전역 ===== */
typedef enum { IDLE = 0, RUN, ERR } State_t;
extern volatile State_t g_state;

/* ===== LED 핀 (PA5) ===== */
#define LED_GPIO_Port  GPIOA
#define LED_Pin        GPIO_PIN_5

/* ===== 프로토타입 ===== */
void SystemClock_Config(void);
void Error_Handler(void);
void MX_GPIO_Init(void);
void MX_USART2_UART_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
