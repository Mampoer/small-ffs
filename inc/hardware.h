//-------------------------------------------------------------------------------------
#ifndef __HARDWARE_H__
#define __HARDWARE_H__

//-------------------------------------------------------------------------------------
#include "stm32f10x.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

//-------------------------------------------------------------------------------------
#include "stm32f10x_conf.h"

//-------------------------------------------------------------------------------------
#define RS485_DEBUG

//-------------------------------------------------------------------------------------
#define rs485_putchar(c)        uart_putchar( USART1, c)

//-------------------------------------------------------------------------------------
#define FLASH_CS_HIGH()         GPIOD->BSRR = (uint32_t)GPIO_Pin_2
#define FLASH_CS_LOW()          GPIOD->BSRR = (uint32_t)GPIO_Pin_2 << 16

#define FLASH_WP_HIGH()         GPIOB->BSRR = (uint32_t)GPIO_Pin_6
#define FLASH_WP_LOW()          GPIOB->BSRR = (uint32_t)GPIO_Pin_6 << 16

#define FLASH_HLD_HIGH()        GPIOB->BSRR = (uint32_t)GPIO_Pin_7
#define FLASH_HLD_LOW()         GPIOB->BSRR = (uint32_t)GPIO_Pin_7 << 16

#define RS485_WRITE_DIRECTION() GPIOB->ODR  |=  GPIO_Pin_9
#define RS485_READ_DIRECTION()  GPIOB->ODR  &= ~GPIO_Pin_9

//-------------------------------------------------------------------------------------
void init_hw(void);

//-------------------------------------------------------------------------------------
int uart_putchar( USART_TypeDef *Instance, int c );
int uart_getchar( USART_TypeDef *Instance );

void tx_done_callback(USART_TypeDef *Instance);

//-------------------------------------------------------------------------------------
void Delay(int delay);

//-------------------------------------------------------------------------------------
#endif
//-------------------------------------------------------------------------------------
