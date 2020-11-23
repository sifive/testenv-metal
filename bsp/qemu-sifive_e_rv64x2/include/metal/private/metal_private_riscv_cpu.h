/* Copyright 2020 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef METAL__GENERATED__RISCV_CPU_H
#define METAL__GENERATED__RISCV_CPU_H

/* GENERATED FILE
 * --------------
 * This file is generated from a template based on the content of the target
 * Devicetree.
 */
#include <metal/cpu.h>
#include <metal/private/metal_private_cpu.h>
#include <metal/interrupt.h>
#include <stdint.h>
#include <stdbool.h>

static const struct dt_cpu_data {
    uint64_t timebase;
} dt_cpu_data[__METAL_DT_NUM_HARTS] = {
    {
        .timebase = 32768,
    },
    {
        .timebase = 32768,
    },
};

#endif