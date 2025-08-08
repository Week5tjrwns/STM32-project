#include "main.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ===== UART1 레지스터 유틸 ===== */
static inline void uart1_putc(char c){
    volatile uint32_t *SR = (uint32_t *)0x40013800;
    volatile uint32_t *DR = (uint32_t *)0x40013804;
    while(((*SR) & (1<<7)) == 0) {}   // TXE
    *DR = (uint8_t)c;
}
static inline void uart1_puts(const char *s){ while(*s) uart1_putc(*s++); }
static inline int  uart1_getc_nonblock(void){
    volatile uint32_t *SR = (uint32_t *)0x40013800;
    if((*SR) & (1<<5)) {              // RXNE
        volatile uint32_t *DR = (uint32_t *)0x40013804;
        return (int)(uint8_t)(*DR);
    }
    return -1;
}
static inline void uart1_init_9600_on_8MHz(void){
    volatile uint32_t *BRR = (uint32_t *)0x40013808;
    volatile uint32_t *CR1 = (uint32_t *)0x4001380C;
    *BRR = 0x341;                     // 8MHz/9600 ≈ 0x341
    *CR1 = (1<<13)|(1<<3)|(1<<2);     // UE | TE | RE
}

/* ===== GPIOA PA5 (LED) 직접 제어 ===== */
#define GPIOA_CRL  (*(volatile uint32_t*)0x40010800)
#define GPIOA_ODR  (*(volatile uint32_t*)0x4001080C)
#define GPIOA_BSRR (*(volatile uint32_t*)0x40010810)
#define GPIOA_BRR  (*(volatile uint32_t*)0x40010814)

static inline void gpio_pa5_output_init(void){
    uint32_t crl = GPIOA_CRL;
    crl &= ~(0xFu << 20);             // PA5[23:20] 클리어
    crl |=  (0x2u << 20);             // MODE5=10(2MHz), CNF5=00(PP)
    GPIOA_CRL = crl;
}
static inline void led_on(void){  GPIOA_BSRR = (1u<<5); }
static inline void led_off(void){ GPIOA_BRR  = (1u<<5); }
static inline int  led_is_on(void){ return (GPIOA_ODR & (1u<<5)) ? 1 : 0; }

/* ===== 최소 HAL: SysTick만 사용 ===== */
void SystemClock_Config(void){
    RCC_OscInitTypeDef o={0}; RCC_ClkInitTypeDef c={0};
    __HAL_RCC_AFIO_CLK_ENABLE(); __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
    o.OscillatorType=RCC_OSCILLATORTYPE_HSI; o.HSIState=RCC_HSI_ON;
    o.HSICalibrationValue=RCC_HSICALIBRATION_DEFAULT; o.PLL.PLLState=RCC_PLL_OFF;
    HAL_RCC_OscConfig(&o);
    c.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource=RCC_SYSCLKSOURCE_HSI; c.AHBCLKDivider=RCC_SYSCLK_DIV1;
    c.APB1CLKDivider=RCC_HCLK_DIV1; c.APB2CLKDivider=RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&c, FLASH_LATENCY_0);
}
void Error_Handler(void){ __disable_irq(); while(1){} }

/* ===== 간단 명령 파서 ===== */
static char line[32];
static uint8_t idx = 0;

static void print_prompt(void){ uart1_puts("> "); }

static void handle_line(const char* s){
    if(strcmp(s,"help")==0){
        uart1_puts("cmds: led on | led off | status | help\r\n");
    } else if(strcmp(s,"led on")==0){
        led_on();  uart1_puts("[OK] LED ON\r\n");
    } else if(strcmp(s,"led off")==0){
        led_off(); uart1_puts("[OK] LED OFF\r\n");
    } else if(strcmp(s,"status")==0 || strcmp(s,"led status")==0){
        uart1_puts("[STATUS] LED=");
        uart1_puts(led_is_on() ? "ON\r\n" : "OFF\r\n");
    } else if(*s==0){
        // empty: ignore
    } else {
        uart1_puts("[ERR] unknown. type 'help'\r\n");
    }
}

int main(void){
    HAL_Init();
    SystemClock_Config();          // HSI 8MHz
    uart1_init_9600_on_8MHz();     // RCC 없이 UART1 직접 활성화
    gpio_pa5_output_init();        // PA5를 출력으로

    uart1_puts("\r\n=== BOOT F103 (reg-UART1 CLI) ===\r\n");
    uart1_puts("type: help\r\n");
    print_prompt();

    uint32_t last = HAL_GetTick();
    unsigned hb = 0;

    for(;;){
        // 하트비트 (1초)
        if(HAL_GetTick() - last >= 1000){
            uart1_puts("[HB] alive #"); uart1_putc('0'+(hb++%10)); uart1_puts("\r\n");
            last = HAL_GetTick();
        }

        // 한 줄 입력 처리 (엔터 기준)
        int ch = uart1_getc_nonblock();
        if(ch < 0) continue;

        if(ch=='\r' || ch=='\n'){
            uart1_puts("\r\n");
            line[idx] = 0;
            handle_line(line);
            idx = 0;
            print_prompt();
        } else if(ch==8 || ch==127){ // 백스페이스
            if(idx>0){ idx--; uart1_puts("\b \b"); }
        } else if(idx < sizeof(line)-1){
            line[idx++] = (char)ch;
            uart1_putc((char)ch); // 에코
        }
    }
}
