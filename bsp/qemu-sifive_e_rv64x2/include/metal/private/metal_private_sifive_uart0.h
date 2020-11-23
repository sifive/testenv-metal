/* Copyright 2020 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef METAL__DRIVERS__SIFIVE_UART0_H
#define METAL__DRIVERS__SIFIVE_UART0_H

/* GENERATED FILE
 * --------------
 * This file is generated from a template based on the content of the target
 * Devicetree.
 */
#include <metal/clock.h>
#include <metal/private/metal_private_uart.h>
#include <metal/gpio.h>
#include <metal/interrupt.h>
#include <metal/platform.h>
#include <stdbool.h>
#include <stddef.h>


static const struct dt_uart_data {
	uintptr_t base_addr;
	struct metal_clock clock;
	bool has_pinmux;
	struct metal_gpio pinmux;
	uint32_t pinmux_output_selector;
	uint32_t pinmux_source_selector;
	struct metal_interrupt interrupt_parent;
	uint32_t interrupt_id;
} dt_uart_data[__METAL_DT_NUM_UARTS] = {
	{
	    .base_addr = METAL_SIFIVE_UART0_0_BASE_ADDRESS,


		.has_pinmux = 0,

	    /* riscv,plic0 */
		.interrupt_parent = { 0 },
		.interrupt_id = 3,
	},
	{
	    .base_addr = METAL_SIFIVE_UART0_1_BASE_ADDRESS,


		.has_pinmux = 0,

	    /* riscv,plic0 */
		.interrupt_parent = { 0 },
		.interrupt_id = 4,
	},
};



#include <metal/drivers/riscv_plic0.h>

/* These defines "redirect" the calls to the public Freedom Metal interrupt API
 * to the driver for the controller at compile time. Since they are the same
 * as the actual public API symbols, when they aren't defined (for instance,
 * if the Devicetree doesn't properly describe the interrupt parent for the device)
 * they will link to the stub functions in src/interrupt.c
 */

#define metal_interrupt_init(intc) riscv_plic0_init((intc))
#define metal_interrupt_enable(intc, id) riscv_plic0_enable((intc), (id))
#define metal_interrupt_disable(intc, id) riscv_plic0_disable((intc), (id))

/* sifive,uart0 has been selected by the Devicetree to provide
 * standard out for the Freedom Metal program
 */
#define METAL_STDOUT_SIFIVE_UART0
#define __METAL_DT_STDOUT_UART_HANDLE ((struct metal_uart) { 0 })
#define __METAL_DT_STDOUT_UART_BAUD 115200


#endif