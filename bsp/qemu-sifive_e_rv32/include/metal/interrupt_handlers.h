/* SPDX-License-Identifier: Apache-2.0 */

#ifndef __INTERRUPT_HANDLERS_H
#define __INTERRUPT_HANDLERS_H

#include <metal/cpu.h>
#include <metal/interrupt.h>
#include <metal/riscv.h>

#define __METAL_NUM_EXCEPTIONS 12

#define __METAL_NUM_LOCAL_INTERRUPTS 16

#define __METAL_NUM_GLOBAL_INTERRUPTS 37

/*!
 * @brief Function signature for interrupt callback handlers
 */
typedef void (*metal_interrupt_handler_t)(void);

/*!
 * @brief Function signature for exception handlers
 */
typedef void (*metal_exception_handler_t)(struct metal_cpu cpu, int ecode);


extern const metal_exception_handler_t __metal_exception_table[__METAL_NUM_EXCEPTIONS];
extern const metal_interrupt_handler_t __metal_local_interrupt_table[__METAL_NUM_LOCAL_INTERRUPTS];

void __metal_exception_handler(riscv_xlen_t mcause);

void metal_exception_instruction_address_misaligned_handler(struct metal_cpu cpu, int ecode);
void metal_exception_instruction_address_fault_handler(struct metal_cpu cpu, int ecode);
void metal_exception_illegal_instruction_handler(struct metal_cpu cpu, int ecode);
void metal_exception_breakpoint_handler(struct metal_cpu cpu, int ecode);
void metal_exception_load_address_misaligned_handler(struct metal_cpu cpu, int ecode);
void metal_exception_load_access_fault_handler(struct metal_cpu cpu, int ecode);
void metal_exception_store_amo_address_misaligned_handler(struct metal_cpu cpu, int ecode);
void metal_exception_store_amo_access_fault_handler(struct metal_cpu cpu, int ecode);
void metal_exception_ecall_from_u_mode_handler(struct metal_cpu cpu, int ecode);
void metal_exception_ecall_from_s_mode_handler(struct metal_cpu cpu, int ecode);
void metal_exception_default_handler(struct metal_cpu cpu, int ecode);
void metal_exception_ecall_from_m_mode_handler(struct metal_cpu cpu, int ecode);
void metal_exception_instruction_page_fault_handler(struct metal_cpu cpu, int ecode);
void metal_exception_load_page_fault_handler(struct metal_cpu cpu, int ecode);
void metal_exception_store_amo_page_fault_handler(struct metal_cpu cpu, int ecode);


void metal_riscv_cpu_intc_default_handler(void) __attribute__((interrupt));

void metal_riscv_cpu_intc_usip_handler(void);
void metal_riscv_cpu_intc_ssip_handler(void);
void metal_riscv_cpu_intc_msip_handler(void);

void metal_riscv_cpu_intc_utip_handler(void);
void metal_riscv_cpu_intc_stip_handler(void);
void metal_riscv_cpu_intc_mtip_handler(void);

void metal_riscv_cpu_intc_ueip_handler(void);
void metal_riscv_cpu_intc_seip_handler(void);
void metal_riscv_plic0_source_0_handler(void);




void metal_sifive_uart0_source_0_handler(void);
void metal_sifive_spi0_source_0_handler(void);
void metal_sifive_gpio0_source_0_handler(void);
void metal_sifive_gpio0_source_1_handler(void);
void metal_sifive_gpio0_source_2_handler(void);
void metal_sifive_gpio0_source_3_handler(void);
void metal_sifive_gpio0_source_4_handler(void);
void metal_sifive_gpio0_source_5_handler(void);
void metal_sifive_gpio0_source_6_handler(void);
void metal_sifive_gpio0_source_7_handler(void);
void metal_sifive_gpio0_source_8_handler(void);
void metal_sifive_gpio0_source_9_handler(void);
void metal_sifive_gpio0_source_10_handler(void);
void metal_sifive_gpio0_source_11_handler(void);
void metal_sifive_gpio0_source_12_handler(void);
void metal_sifive_gpio0_source_13_handler(void);
void metal_sifive_gpio0_source_14_handler(void);
void metal_sifive_gpio0_source_15_handler(void);
void metal_sifive_gpio0_source_16_handler(void);
void metal_sifive_gpio0_source_17_handler(void);
void metal_sifive_gpio0_source_18_handler(void);
void metal_sifive_gpio0_source_19_handler(void);
void metal_sifive_gpio0_source_20_handler(void);
void metal_sifive_gpio0_source_21_handler(void);
void metal_sifive_gpio0_source_22_handler(void);
void metal_sifive_gpio0_source_23_handler(void);
void metal_sifive_gpio0_source_24_handler(void);
void metal_sifive_gpio0_source_25_handler(void);
void metal_sifive_gpio0_source_26_handler(void);
void metal_sifive_gpio0_source_27_handler(void);
void metal_sifive_gpio0_source_28_handler(void);
void metal_sifive_gpio0_source_29_handler(void);
void metal_sifive_gpio0_source_30_handler(void);
void metal_sifive_gpio0_source_31_handler(void);
void metal_sifive_hca0_source_0_handler(void);
void metal_sifive_hca0_source_1_handler(void);

#endif 