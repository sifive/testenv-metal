/* Copyright 2020 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef METAL__GENERATED__SIFIVE_HCA0_H
#define METAL__GENERATED__SIFIVE_HCA0_H

/* GENERATED FILE
 * --------------
 * This file is generated from a template based on the content of the target
 * Devicetree.
 */
#include <metal/drivers/sifive_hca0_regs.h>
#include <metal/interrupt.h>
#include <metal/platform.h>
#include <stdbool.h>
#include <stddef.h>


static const struct dt_hca_data {
	uintptr_t base_addr;
    HCA_Type *hca_regs;
	struct metal_interrupt interrupt_parent;
	uint32_t interrupt_id;
} dt_hca_data[__METAL_DT_NUM_HCA0S] = {
	{
	    .base_addr = METAL_SIFIVE_HCA0_0_BASE_ADDRESS,
        .hca_regs = (HCA_Type *) METAL_SIFIVE_HCA0_0_BASE_ADDRESS,

	    /* riscv,plic0 */
		.interrupt_parent = { 0 },
		.interrupt_id = 52,
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

#endif /* METAL__GENERATED__SIFIVE_HCA0_H */