/* Copyright 2020 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#if __riscv_xlen == 32
.balign 128, 0
#else
.balign 256, 0
#endif
.option norvc
.global __metal_vector_table
__metal_vector_table:
/* IRQ 0 can't be vectored because synchronous exceptions also have to be handled
 * by this first position of the jump table. The user-mode software interrupt
 * handler will be invoked by the exception handler, just as in the unvectored
 * case. */
        j __metal_exception_handler
IRQ_1:
        j metal_riscv_cpu_intc_default_handler
IRQ_2:
        j metal_riscv_cpu_intc_default_handler
IRQ_3:
        j metal_riscv_cpu_intc_msip_handler
IRQ_4:
        j metal_riscv_cpu_intc_default_handler
IRQ_5:
        j metal_riscv_cpu_intc_default_handler
IRQ_6:
        j metal_riscv_cpu_intc_default_handler
IRQ_7:
        j metal_riscv_cpu_intc_mtip_handler
IRQ_8:
        j metal_riscv_cpu_intc_default_handler
IRQ_9:
        j metal_riscv_cpu_intc_default_handler
IRQ_10:
        j metal_riscv_cpu_intc_default_handler
IRQ_11:
        j metal_riscv_plic0_source_0_handler
IRQ_12:
        j metal_riscv_cpu_intc_default_handler
IRQ_13:
        j metal_riscv_cpu_intc_default_handler
IRQ_14:
        j metal_riscv_cpu_intc_default_handler
IRQ_15:
        j metal_riscv_cpu_intc_default_handler
