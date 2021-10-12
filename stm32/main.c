/* Copyright (C) 2021 Sam Bazley
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "hpgl.h"
#include "uart.h"
#include "usb.h"
#include <usblib.h>
#include <stm32f0xx.h>

void rcc_init()
{
	FLASH->ACR |= FLASH_ACR_LATENCY;

	RCC->CR2 |= RCC_CR2_HSI48ON;

	while (!(RCC->CR2 & RCC_CR2_HSI48RDY)) {
		__NOP();
	}

	RCC->CFGR |= RCC_CFGR_SW_HSI48;

	while (!(RCC->CFGR & RCC_CFGR_SWS_HSI48)) {
		__NOP();
	}
}

void boot()
{
	rcc_init();
	uart_init();

	RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
	GPIOA->MODER &= ~(3 << (2 * 0));
	GPIOA->MODER |= 1 << (2 * 0);
	GPIOA->OTYPER |= 1 << 0;
	GPIOA->ODR |= 1;

	usb_impl_init();

	while (1) {
		if (usb_get_selected_config() != 0) {
			break;
		}
	}

	uart_send_str("booted\n");

	hpgl_loop();
}
