/*
 * This file is part of the Black Magic Debug project.
 *
 * Copyright (C) 2017 Uwe Bonnes bon@elektron,ikp,physik.tu-darmstadt.de
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* This file implements the platform specific functions for the STM32F072-IF implementation. */

#include "general.h"
#include "usb.h"
#include "aux_serial.h"
#include "morse.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/syscfg.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/stm32/flash.h>

typedef void (*irq_function_t)(void);

extern uint32_t _ebss; // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)

#define SYSCFG_MEMRM        MMIO32(0x40010000U)
#define SYSMEM_RESET_VECTOR 0x1fffc804U

static const irq_function_t *const volatile reset_vector = (irq_function_t *)(uintptr_t)SYSMEM_RESET_VECTOR;

void platform_init(void)
{
	volatile uint32_t *magic = &_ebss;
	/*
	 * If RCC_CFGR is not at it's reset value, the bootloader
	 * was executed and SET_ADDRESS got us to this place.
	 * On the STM32F3 (???), without any further effort, DFU will not
	 * start in that case - so issue an reset to allow a clean start!
	 */
	if (RCC_CFGR)
		scb_reset_system();
	SYSCFG_MEMRM &= ~3U;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
	/* Button is BOOT0, so button is already evaluated! */
	if (magic[0] == BOOTMAGIC0 && magic[1] == BOOTMAGIC1) {
		magic[0] = 0;
		magic[1] = 0;
		/*
		 * Jump to the built in bootloader by mapping System flash.
		 * As we just come out of reset, no other deinit is needed!
		 */
		SYSCFG_MEMRM |= 1U;
		const irq_function_t bootloader = *reset_vector;
		/* We come out of reset, so MSP is already set */
		bootloader();
		while (true)
			continue;
	}
#pragma GCC diagnostic pop
	rcc_clock_setup_in_hse_8mhz_out_48mhz();

	/* Enable peripherals */
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_CRC);
	rcc_set_usbclk_source(RCC_PLL);

	GPIOA_OSPEEDR &= ~0xf00cU;
	GPIOA_OSPEEDR |= 0x5004U; /* Set medium speed on PA1, PA6,PA7 */
	gpio_mode_setup(JTAG_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, TMS_PIN | TCK_PIN | TDI_PIN);
	gpio_mode_setup(TDO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, TDO_PIN);
	gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_UART | LED_IDLE_RUN | LED_ERROR | LED_BOOTLOADER);
	gpio_mode_setup(NRST_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, NRST_PIN);
	gpio_set(NRST_PORT, NRST_PIN);
	gpio_set_output_options(NRST_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, NRST_PIN);
	platform_timing_init();
	blackmagic_usb_init();
	aux_serial_init();
}

void platform_nrst_set_val(bool assert)
{
	gpio_set_val(NRST_PORT, NRST_PIN, !assert);
}

bool platform_nrst_get_val(void)
{
	return (gpio_get(NRST_PORT, NRST_PIN)) ? false : true;
}

const char *platform_target_voltage(void)
{
	return "ABSENT!";
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"

void platform_request_boot(void)
{
	uint32_t *magic = &_ebss;
	magic[0] = BOOTMAGIC0;
	magic[1] = BOOTMAGIC1;
	scb_reset_system();
}

#pragma GCC diagnostic pop

void platform_target_clk_output_enable(bool enable)
{
	(void)enable;
}
