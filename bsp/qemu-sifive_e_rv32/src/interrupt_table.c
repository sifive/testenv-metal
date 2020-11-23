/* Copyright 2020 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#include <metal/interrupt_handlers.h>

const metal_exception_handler_t __metal_exception_table[__METAL_NUM_EXCEPTIONS] = {
    metal_exception_instruction_address_misaligned_handler,
    metal_exception_instruction_address_fault_handler,
    metal_exception_illegal_instruction_handler,
    metal_exception_breakpoint_handler,
    metal_exception_load_address_misaligned_handler,
    metal_exception_load_access_fault_handler,
    metal_exception_store_amo_address_misaligned_handler,
    metal_exception_store_amo_access_fault_handler,
    metal_exception_ecall_from_u_mode_handler,
    metal_exception_ecall_from_s_mode_handler,
    metal_exception_default_handler,
    metal_exception_ecall_from_m_mode_handler,
#if __METAL_NUM_EXCEPTIONS > 12
    metal_exception_instruction_page_fault_handler,
    metal_exception_load_page_fault_handler,
    metal_exception_default_handler,
    metal_exception_store_amo_page_fault_handler,
#endif
};

__attribute__((aligned(64))) const metal_interrupt_handler_t __metal_local_interrupt_table[__METAL_NUM_LOCAL_INTERRUPTS] = {
    metal_riscv_cpu_intc_usip_handler,
    metal_riscv_cpu_intc_ssip_handler,
    metal_riscv_cpu_intc_default_handler, /* reserved */
    metal_riscv_cpu_intc_msip_handler,
    metal_riscv_cpu_intc_utip_handler,
    metal_riscv_cpu_intc_stip_handler,
    metal_riscv_cpu_intc_default_handler, /* reserved */
    metal_riscv_cpu_intc_mtip_handler,
    metal_riscv_cpu_intc_ueip_handler,
    metal_riscv_cpu_intc_seip_handler,
    metal_riscv_cpu_intc_default_handler, /* reserved */
    metal_riscv_plic0_source_0_handler,
    metal_riscv_cpu_intc_default_handler, /* reserved */
    metal_riscv_cpu_intc_default_handler, /* reserved */
    metal_riscv_cpu_intc_default_handler, /* reserved */
    metal_riscv_cpu_intc_default_handler, /* reserved */
};

/* The global interrupt table holds function pointers for interrupt handlers called by the PLIC.
 * It has one more entry than __METAL_NUM_GLOBAL_INTERRUPTS because the PLIC interrupt line 0
 * is always unused.
 */
const metal_interrupt_handler_t __metal_global_interrupt_table[__METAL_NUM_GLOBAL_INTERRUPTS + 1] = {
    metal_riscv_cpu_intc_default_handler, /* The PLIC has no source at id 0 */
    metal_sifive_uart0_source_0_handler,
    metal_sifive_uart0_source_0_handler,
    metal_sifive_spi0_source_0_handler,
    metal_sifive_gpio0_source_0_handler,
    metal_sifive_gpio0_source_1_handler,
    metal_sifive_gpio0_source_2_handler,
    metal_sifive_gpio0_source_3_handler,
    metal_sifive_gpio0_source_4_handler,
    metal_sifive_gpio0_source_5_handler,
    metal_sifive_gpio0_source_6_handler,
    metal_sifive_gpio0_source_7_handler,
    metal_sifive_gpio0_source_8_handler,
    metal_sifive_gpio0_source_9_handler,
    metal_sifive_gpio0_source_10_handler,
    metal_sifive_gpio0_source_11_handler,
    metal_sifive_gpio0_source_12_handler,
    metal_sifive_gpio0_source_13_handler,
    metal_sifive_gpio0_source_14_handler,
    metal_sifive_gpio0_source_15_handler,
    metal_sifive_gpio0_source_16_handler,
    metal_sifive_gpio0_source_17_handler,
    metal_sifive_gpio0_source_18_handler,
    metal_sifive_gpio0_source_19_handler,
    metal_sifive_gpio0_source_20_handler,
    metal_sifive_gpio0_source_21_handler,
    metal_sifive_gpio0_source_22_handler,
    metal_sifive_gpio0_source_23_handler,
    metal_sifive_gpio0_source_24_handler,
    metal_sifive_gpio0_source_25_handler,
    metal_sifive_gpio0_source_26_handler,
    metal_sifive_gpio0_source_27_handler,
    metal_sifive_gpio0_source_28_handler,
    metal_sifive_gpio0_source_29_handler,
    metal_sifive_gpio0_source_30_handler,
    metal_sifive_gpio0_source_31_handler,
    metal_sifive_hca0_source_0_handler,
    metal_sifive_hca0_source_1_handler,
};
