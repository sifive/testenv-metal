/* Copyright 2018 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef METAL__DRIVERS__SIFIVE_GPIO0_H
#define METAL__DRIVERS__SIFIVE_GPIO0_H

/* GENERATED FILE
 * --------------
 * This file is generated from a template based on the content of the target
 * Devicetree.
 */
#include <metal/private/metal_private_gpio.h>
#include <metal/interrupt.h>
#include <metal/platform.h>
#include <stddef.h>
#include <stdint.h>



/* Only one sifive,gpio0 exists so all devicetree data is constant */
#define get_index(gpio) 0
#define BASE_ADDR(gpio) METAL_SIFIVE_GPIO0_0_BASE_ADDRESS
#define INTERRUPT_PARENT(gpio) ((struct metal_interrupt) { 0 })
#define INTERRUPT_ID_BASE(gpio) 8



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