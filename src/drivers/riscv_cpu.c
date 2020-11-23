/* Copyright 2020 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#include <assert.h>
#include <metal/cpu.h>
#include <metal/drivers/riscv_cpu_intc.h>
#include <metal/io.h>
#include <metal/platform.h>
#include <metal/private/metal_private_riscv_cpu.h>
#include <metal/riscv.h>
#include <metal/shutdown.h>
#include <stdint.h>

static inline uint32_t get_hartid(struct metal_cpu cpu) { return cpu.__hartid; }

unsigned long long metal_cpu_get_timer(struct metal_cpu cpu) {
    unsigned long long val = 0;

#if __riscv_xlen == 32
    unsigned long hi, hi1, lo;

    do {
        __asm__ volatile("csrr %0, mcycleh" : "=r"(hi));
        __asm__ volatile("csrr %0, mcycle" : "=r"(lo));
        __asm__ volatile("csrr %0, mcycleh" : "=r"(hi1));
        /* hi != hi1 means mcycle overflow during we get value,
         * so we must retry. */
    } while (hi != hi1);

    val = ((unsigned long long)hi << 32) | lo;
#else
    __asm__ volatile("csrr %0, mcycle" : "=r"(val));
#endif

    return val;
}

unsigned long long metal_cpu_get_timebase(struct metal_cpu cpu) {
    return dt_cpu_data[get_hartid(cpu)].timebase;
}

__attribute__((weak)) uint64_t metal_cpu_get_mtime(struct metal_cpu cpu) {
    return 0;
}

__attribute__((weak)) int metal_cpu_set_mtimecmp(struct metal_cpu cpu,
                                                 uint64_t time) {
    return -1;
}

__attribute__((weak)) void metal_cpu_enable_interrupts(void) {
    __asm__ volatile("csrs mstatus, %0" ::"r"(RISCV_MSTATUS_MIE));
}

__attribute__((weak)) void metal_cpu_disable_interrupts(void) {
    __asm__ volatile("csrc mstatus, %0" ::"r"(RISCV_MSTATUS_MIE));
}

__attribute__((weak)) void metal_cpu_enable_ipi(void) {
    __asm__ volatile("csrs mie, %0" ::"r"(RISCV_MIE_MSIE));
}

__attribute__((weak)) void metal_cpu_disable_ipi(void) {
    __asm__ volatile("csrc mie, %0" ::"r"(RISCV_MIE_MSIE));
}

__attribute__((weak)) void metal_cpu_set_ipi(struct metal_cpu cpu) {}

__attribute__((weak)) void metal_cpu_clear_ipi(struct metal_cpu cpu) {}

__attribute__((weak)) int metal_cpu_get_ipi(struct metal_cpu cpu) { return 0; }

__attribute__((weak)) void metal_cpu_enable_timer_interrupt(void) {
    __asm__ volatile("csrs mie, %0" ::"r"(RISCV_MIE_MTIE));
}

__attribute__((weak)) void metal_cpu_disable_timer_interrupt(void) {
    __asm__ volatile("csrc mie, %0" ::"r"(RISCV_MIE_MTIE));
}

__attribute__((weak)) void metal_cpu_enable_external_interrupt(void) {
    __asm__ volatile("csrs mie, %0" ::"r"(RISCV_MIE_MEIE));
}

__attribute__((weak)) void metal_cpu_disable_external_interrupt(void) {
    __asm__ volatile("csrc mie, %0" ::"r"(RISCV_MIE_MEIE));
}

struct metal_interrupt metal_cpu_interrupt_controller(struct metal_cpu cpu) {
    return (struct metal_interrupt){cpu.__hartid};
}

int metal_cpu_get_instruction_length(struct metal_cpu cpu, uintptr_t epc) {
    /**
     * Per ISA compressed instruction has last two bits of opcode set.
     * The encoding '00' '01' '10' are used for compressed instruction.
     * Only enconding '11' isn't regarded as compressed instruction (>16b).
     */
    return ((*(unsigned short *)epc & RISCV_INSTRUCTION_LENGTH_MASK) ==
            RISCV_INSTRUCTION_NOT_COMPRESSED)
               ? 4
               : 2;
}

uintptr_t metal_cpu_get_exception_pc(struct metal_cpu cpu) {
    uintptr_t mepc;
    __asm__ volatile("csrr %0, mepc" : "=r"(mepc));
    return mepc;
}

int metal_cpu_set_exception_pc(struct metal_cpu cpu, uintptr_t mepc) {
    __asm__ volatile("csrw mepc, %0" ::"r"(mepc));
    return 0;
}
