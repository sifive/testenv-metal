/* Copyright 2020 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef METAL__DRIVERS__SIFIVE_UART0_H
#define METAL__DRIVERS__SIFIVE_UART0_H

/* GENERATED FILE
 * --------------
 * This file is generated from a template based on the content of the target
 * Devicetree.
 */
#include <metal/interrupt.h>
#include <metal/platform.h>
#include <stdint.h>


#define __METAL_DT_NUM_RISCV_PLIC0S 0

static const uintptr_t plic_context_id[__METAL_DT_NUM_HARTS] = {
    0,
    1,
};
#define PLIC_CONTEXT_ID(hartid) (plic_context_id[hartid])


#include <metal/drivers/riscv_cpu_intc.h>

/* These defines "redirect" the calls to the public Freedom Metal interrupt API
 * to the driver for the controller at compile time. Since they are the same
 * as the actual public API symbols, when they aren't defined (for instance,
 * if the Devicetree doesn't properly describe the interrupt parent for the device)
 * they will link to the stub functions in src/interrupt.c
 */

#define metal_interrupt_init(intc) riscv_cpu_intc_init((intc))
#define metal_interrupt_enable(intc, id) riscv_cpu_intc_enable((intc), (id))
#define metal_interrupt_disable(intc, id) riscv_cpu_intc_disable((intc), (id))

#endif