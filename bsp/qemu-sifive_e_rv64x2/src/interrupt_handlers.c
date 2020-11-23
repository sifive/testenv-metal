/* Copyright 2020 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#include <metal/interrupt_handlers.h>
#include <metal/shutdown.h>

/* Define all default interrupt and exception handlers to weak no-ops so that if no
 * driver or application overrides them we still have a definition for the interrupt
 * and exception tables
 */

/* No-op functions get defined once so we can alias all undefined handlers to a single
 * copy of the no-op handler
 */
void _metal_exception_handler_nop(struct metal_cpu cpu, int ecode) {
    metal_shutdown(100);
}

void _metal_local_interrupt_handler_nop(void) __attribute((interrupt));
void _metal_local_interrupt_handler_nop(void) {
    metal_shutdown(200);
}

void _metal_global_interrupt_handler_nop(void) {
    metal_shutdown(300);
}

/* Alias default exception handlers to _metal_exception_handler_nop(void) */
void metal_exception_instruction_address_misaligned_handler(struct metal_cpu cpu, int ecode) __attribute__((weak, alias("_metal_exception_handler_nop")));
void metal_exception_instruction_address_fault_handler(struct metal_cpu cpu, int ecode) __attribute__((weak, alias("_metal_exception_handler_nop")));
void metal_exception_illegal_instruction_handler(struct metal_cpu cpu, int ecode) __attribute__((weak, alias("_metal_exception_handler_nop")));
void metal_exception_breakpoint_handler(struct metal_cpu cpu, int ecode) __attribute__((weak, alias("_metal_exception_handler_nop")));
void metal_exception_load_address_misaligned_handler(struct metal_cpu cpu, int ecode) __attribute__((weak, alias("_metal_exception_handler_nop")));
void metal_exception_load_access_fault_handler(struct metal_cpu cpu, int ecode) __attribute__((weak, alias("_metal_exception_handler_nop")));
void metal_exception_store_amo_address_misaligned_handler(struct metal_cpu cpu, int ecode) __attribute__((weak, alias("_metal_exception_handler_nop")));
void metal_exception_store_amo_access_fault_handler(struct metal_cpu cpu, int ecode) __attribute__((weak, alias("_metal_exception_handler_nop")));
void metal_exception_ecall_from_u_mode_handler(struct metal_cpu cpu, int ecode) __attribute__((weak, alias("_metal_exception_handler_nop")));
void metal_exception_ecall_from_s_mode_handler(struct metal_cpu cpu, int ecode) __attribute__((weak, alias("_metal_exception_handler_nop")));
void metal_exception_default_handler(struct metal_cpu cpu, int ecode) __attribute__((weak, alias("_metal_exception_handler_nop")));
void metal_exception_ecall_from_m_mode_handler(struct metal_cpu cpu, int ecode) __attribute__((weak, alias("_metal_exception_handler_nop")));
void metal_exception_instruction_page_fault_handler(struct metal_cpu cpu, int ecode) __attribute__((weak, alias("_metal_exception_handler_nop")));
void metal_exception_load_page_fault_handler(struct metal_cpu cpu, int ecode) __attribute__((weak, alias("_metal_exception_handler_nop")));
void metal_exception_store_amo_page_fault_handler(struct metal_cpu cpu, int ecode) __attribute__((weak, alias("_metal_exception_handler_nop")));

/* Alias default local interrupt handlers to _metal_local_interrupt_handler_nop(void) */
void metal_riscv_cpu_intc_default_handler(void) __attribute__((weak, alias("_metal_local_interrupt_handler_nop"))); 
void metal_riscv_cpu_intc_usip_handler(void) __attribute__((weak, alias("_metal_local_interrupt_handler_nop")));
void metal_riscv_cpu_intc_ssip_handler(void) __attribute__((weak, alias("_metal_local_interrupt_handler_nop")));
void metal_riscv_cpu_intc_msip_handler(void) __attribute__((weak, alias("_metal_local_interrupt_handler_nop")));
void metal_riscv_cpu_intc_utip_handler(void) __attribute__((weak, alias("_metal_local_interrupt_handler_nop")));
void metal_riscv_cpu_intc_stip_handler(void) __attribute__((weak, alias("_metal_local_interrupt_handler_nop")));
void metal_riscv_cpu_intc_mtip_handler(void) __attribute__((weak, alias("_metal_local_interrupt_handler_nop")));
void metal_riscv_cpu_intc_ueip_handler(void) __attribute__((weak, alias("_metal_local_interrupt_handler_nop")));
void metal_riscv_cpu_intc_seip_handler(void) __attribute__((weak, alias("_metal_local_interrupt_handler_nop")));
void metal_riscv_plic0_source_0_handler(void) __attribute__((weak, alias("_metal_local_interrupt_handler_nop")));

/* Alias default global interrupt handlers to _metal_global_interrupt_handler_nop(void) */
void metal_sifive_uart0_source_0_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_spi0_source_0_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_0_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_1_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_2_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_3_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_4_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_5_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_6_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_7_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_8_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_9_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_10_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_11_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_12_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_13_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_14_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_15_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_16_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_17_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_18_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_19_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_20_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_21_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_22_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_23_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_24_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_25_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_26_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_27_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_28_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_29_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_30_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_gpio0_source_31_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_hca_0_5_source_0_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
void metal_sifive_hca_0_5_source_1_handler(void) __attribute__((weak, alias ("_metal_global_interrupt_handler_nop")));
