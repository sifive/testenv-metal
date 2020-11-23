/* Copyright 2020 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef METAL__DRIVERS__SIFIVE_SPI0_H
#define METAL__DRIVERS__SIFIVE_SPI0_H

/* GENERATED FILE
 * --------------
 * This file is generated from a template based on the content of the target
 * Devicetree.
 */
#include <metal/clock.h>
#include <metal/private/metal_private_spi.h>
#include <metal/gpio.h>
#include <metal/interrupt.h>
#include <metal/spi.h>
#include <metal/platform.h>
#include <stdbool.h>
#include <stddef.h>


static const struct dt_spi_data {
	uintptr_t base_addr;
	struct metal_clock clock;
	bool has_pinmux;
	struct metal_gpio pinmux;
	uint32_t pinmux_output_selector;
	uint32_t pinmux_source_selector;
	struct metal_interrupt interrupt_parent;
	uint32_t interrupt_id;
} dt_spi_data[__METAL_DT_NUM_SPIS] = {
	{
	    .base_addr = METAL_SIFIVE_SPI0_0_BASE_ADDRESS,


		.has_pinmux = 0,

	    /* riscv,plic0 */
		.interrupt_parent = { 0 },
		.interrupt_id = 5,
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

#endif