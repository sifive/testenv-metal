/* Copyright 2020 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef METAL__DRIVERS__SIFIVE_FE310_G000_PLL_H
#define METAL__DRIVERS__SIFIVE_FE310_G000_PLL_H

#include <metal/clock.h>

uint64_t sifive_fe310_g000_pll_get_rate_hz(struct metal_clock clock);
uint64_t sifive_fe310_g000_pll_set_rate_hz(struct metal_clock clock,
                                           uint64_t rate);

#endif
