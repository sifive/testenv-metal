/* Copyright 2018 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#include <metal/platform.h>

#ifdef METAL_SIFIVE_FE310_G000_PLL

#include <limits.h>
#include <metal/cpu.h>
#include <metal/drivers/sifive_fe310_g000_pll.h>
#include <metal/init.h>
#include <metal/io.h>
#include <metal/private/metal_private_sifive_fe310_g000_pll.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef METAL_SIFIVE_FE310_G000_PRCI
#error No SiFive FE310-G000 PRCI available.
#endif

#define PLL_R 0x00000007UL
#define PLL_F 0x000003F0UL
#define PLL_Q 0x00000C00UL
#define PLL_SEL 0x00010000UL
#define PLL_REFSEL 0x00020000UL
#define PLL_BYPASS 0x00040000UL
#define PLL_LOCK 0x80000000UL

#define DIV_DIV 0x0000003FUL
#define DIV_1 0x00000100UL

#define PLL_R_SHIFT(r) ((r << 0) & PLL_R)
#define PLL_F_SHIFT(f) ((f << 4) & PLL_F)
#define PLL_Q_SHIFT(q) ((q << 10) & PLL_Q)
#define PLL_DIV_SHIFT(d) ((d << 0) & DIV_DIV)

#define PLL_CONFIG_NOT_VALID UINT64_MAX

#define PLL_REGW(offset)                                                       \
    __METAL_ACCESS_ONCE(                                                       \
        (__metal_io_u32 *)(METAL_SIFIVE_FE310_G000_PRCI_0_BASE_ADDRESS +       \
                           (offset)))

struct pll_config_t {
    uint64_t multiplier;
    uint64_t divisor;
    uint64_t min_input_rate;
    uint64_t max_input_rate;
    uint32_t r;
    uint32_t f;
    uint32_t q;
    int32_t d; /* < 0 if disabled */
};

static const struct pll_config_t pll_configs[] = {
    /*
     * multiplier
     * ^  divisor
     * |  ^      min_input_rate
     * |  |      ^        max_input_rate
     * |  |      |        ^      r
     * |  |      |        |      ^   f
     * |  |      |        |      |   ^  q
     * |  |      |        |      |   |  ^   d
     * |  |      |        |      |   |  |   ^
     * |  |      |        |      |   |  |   | */
    {1, 32, 12000000, 24000000, 1, 31, 3, 63},
    {1, 32, 24000000, 48000000, 3, 31, 2, 63},
    {1, 16, 6000000, 12000000, 0, 31, 3, 63},
    {1, 16, 12000000, 24000000, 1, 31, 2, 63},
    {1, 16, 24000000, 48000000, 3, 31, 2, 31},
    {1, 8, 6000000, 12000000, 0, 31, 3, 31},
    {1, 8, 12000000, 24000000, 1, 31, 2, 31},
    {1, 8, 24000000, 48000000, 3, 31, 2, 15},
    {1, 4, 6000000, 12000000, 0, 31, 3, 15},
    {1, 4, 12000000, 24000000, 1, 31, 2, 15},
    {1, 4, 24000000, 48000000, 3, 31, 2, 7},
    {1, 2, 6000000, 12000000, 0, 31, 2, 15},
    {1, 2, 12000000, 24000000, 1, 31, 1, 15},
    {1, 2, 24000000, 48000000, 3, 31, 1, 7},
    {2, 1, 6000000, 12000000, 0, 31, 1, 7},
    {2, 1, 12000000, 24000000, 1, 31, 1, 3},
    {2, 1, 24000000, 48000000, 3, 31, 3, -1},
    {4, 1, 6000000, 12000000, 0, 31, 3, 0},
    {4, 1, 12000000, 24000000, 1, 31, 3, -1},
    {4, 1, 24000000, 48000000, 3, 31, 2, -1},
    {6, 1, 6000000, 10666666, 0, 35, 1, 2},
    {6, 1, 10666666, 12000000, 0, 23, 3, -1},
    {6, 1, 12000000, 16000000, 1, 47, 3, -1},
    {6, 1, 16000000, 18000000, 1, 23, 2, -1},
    {6, 1, 18000000, 21333333, 2, 35, 2, -1},
    {8, 1, 6000000, 12000000, 0, 31, 3, -1},
    {8, 1, 12000000, 24000000, 1, 31, 2, -1},
    {8, 1, 24000000, 48000000, 3, 31, 1, -1},
    {10, 1, 6000000, 9600000, 0, 39, 3, -1},
    {10, 1, 9600000, 12000000, 0, 19, 2, -1},
    {10, 1, 12000000, 19200000, 1, 39, 2, -1},
    {10, 1, 19200000, 24000000, 1, 19, 1, -1},
    {10, 1, 24000000, 38400000, 3, 39, 1, -1},
    {12, 1, 6000000, 8000000, 0, 47, 3, -1},
    {12, 1, 8000000, 12000000, 0, 23, 2, -1},
    {12, 1, 12000000, 16000000, 1, 47, 2, -1},
    {12, 1, 16000000, 24000000, 1, 23, 1, -1},
    {12, 1, 24000000, 30000000, 3, 47, 1, -1},
    {12, 1, 30000000, 32000000, 3, 47, 1, -1},
    {14, 1, 6000000, 6857142, 0, 55, 3, -1},
    {14, 1, 6857143, 12000000, 0, 27, 2, -1},
    {14, 1, 12000000, 13714285, 1, 55, 2, -1},
    {14, 1, 13714286, 24000000, 1, 27, 1, -1},
    {14, 1, 24000000, 27428571, 3, 55, 1, -1},
    {16, 1, 6000000, 12000000, 0, 31, 2, -1},
    {16, 1, 12000000, 24000000, 1, 31, 1, -1},
    {18, 1, 6000000, 10666666, 0, 35, 2, -1},
    {18, 1, 10666667, 12000000, 0, 17, 1, -1},
    {18, 1, 12000000, 21333333, 1, 35, 1, -1},
    {20, 1, 6000000, 9600000, 0, 39, 2, -1},
    {20, 1, 9600000, 12000000, 0, 19, 1, -1},
    {20, 1, 12000000, 19200000, 1, 39, 1, -1},
    {22, 1, 6000000, 8727272, 0, 43, 2, -1},
    {22, 1, 8727273, 12000000, 0, 21, 1, -1},
    {22, 1, 12000000, 17454545, 1, 43, 1, -1},
    {24, 1, 6000000, 8000000, 0, 47, 2, -1},
    {24, 1, 8000000, 12000000, 0, 23, 1, -1},
    {24, 1, 12000000, 16000000, 1, 47, 1, -1},
    {26, 1, 6000000, 7384615, 0, 51, 2, -1},
    {26, 1, 7384616, 12000000, 0, 25, 1, -1},
    {26, 1, 12000000, 14768230, 1, 51, 1, -1},
    {28, 1, 6000000, 6857142, 0, 55, 2, -1},
    {28, 1, 6857143, 12000000, 0, 27, 1, -1},
    {28, 1, 12000000, 13714285, 1, 55, 1, -1},
    {30, 1, 6000000, 6400000, 0, 59, 2, -1},
    {30, 1, 6400000, 12000000, 0, 29, 1, -1},
    {30, 1, 12000000, 12800000, 1, 59, 1, -1},
    {32, 1, 6000000, 12000000, 0, 31, 1, -1}};

/* Given the rate of the PLL input frequency and a PLL configuration, what
 * will the resulting PLL output frequency be?
 * Arguments:
 *  - pll_input_rate the PLL input frequency in hertz
 *  - config the PLL configuration
 * Returns:
 *  - PLL_CONFIG_NOT_VALID if the configuration is not valid for the input
 * frequency
 *  - the output frequency, in hertz */
static uint64_t get_pll_config_freq(uint64_t pll_input_rate,
                                    const struct pll_config_t *config) {
    if (pll_input_rate < config->min_input_rate ||
        pll_input_rate > config->max_input_rate)
        return PLL_CONFIG_NOT_VALID;

    return pll_input_rate * config->multiplier / config->divisor;
}

/* Find a valid configuration for the PLL which is closest to the desired
 * output frequency.
 * Arguments:
 *  - ref_hz PLL input frequency
 *  - rate desired PLL output frequency
 * Returns:
 *  -1 if no valid configuration is available
 *  the index into pll_configs of a valid configuration */
static int find_closest_config(uint64_t ref_hz, uint64_t rate) {
    int closest_index = -1;
    uint64_t closest_diff = UINT64_MAX;

    /* We're probably trying for a fast output frequency, so start from
     * the high end of the configs. */
    for (int i = (sizeof(pll_configs) / sizeof(pll_configs[0])) - 1; i >= 0;
         i--) {
        uint64_t config_freq = get_pll_config_freq(ref_hz, &(pll_configs[i]));
        if (config_freq != PLL_CONFIG_NOT_VALID) {
            uint64_t freq_diff;
            if (config_freq > rate) {
                freq_diff = config_freq - rate;
            } else {
                freq_diff = rate - config_freq;
            }

            if (freq_diff < closest_diff) {
                closest_index = i;
                closest_diff = freq_diff;
            }
        }
    }

    return closest_index;
}

static void sifive_fe310_g000_pll_init(struct metal_clock pll) {
    /* If we're running off of the PLL, switch off before we start configuring
     * it. */
    if ((PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG) & PLL_SEL) != 0) {
        PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG) &= ~PLL_SEL;
    }

    /* Make sure we're running off of the external oscillator for stability */
    if (PLL_HAS_HFXOSC(pll)) {
        PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG) |= PLL_SEL;
    }

    /* Configure the PLL to run at the requested init frequency. */
    sifive_fe310_g000_pll_set_rate_hz(pll, PLL_INIT_RATE(pll));
}

METAL_CONSTRUCTOR(init_sifive_fe310_g000_pll) {
    for (int i = 0; i < __METAL_DT_NUM_SIFIVE_FE310_G000_PLLS; i++) {
        struct metal_clock pll = (struct metal_clock){i};

        /* If the PLL init_rate is zero, don't initialize the PLL */
        if (PLL_INIT_RATE(pll) != 0) {
            sifive_fe310_g000_pll_init(pll);
        }
    }
}

uint64_t sifive_fe310_g000_pll_get_rate_hz(struct metal_clock clock) {
    uint32_t cfg = PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG);
    uint32_t div = PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLOUTDIV);

    /* There's a clock mux before the PLL that selects between the HFROSC and
     * the HFXOSC as the PLL's input clock. */
    uint64_t ref_hz;
    if (__METAL_GET_FIELD(cfg, PLL_SEL) == 0) {
        ref_hz = sifive_fe310_g000_hfrosc_get_rate_hz(PLL_HFROSC(pll));
    } else {
        ref_hz = sifive_fe310_g000_hfxosc_get_rate_hz(PLL_HFXOSC(pll));
    }

    /* It's possible to bypass the PLL, which is an internal bypass. */
    if (__METAL_GET_FIELD(cfg, PLL_BYPASS))
        return ref_hz;

    /* Logically the PLL is a three stage div-mul-div. */
    long div_r = __METAL_GET_FIELD(cfg, PLL_R) + 1;
    long mul_f = 2 * (__METAL_GET_FIELD(cfg, PLL_F) + 1);
    if (__METAL_GET_FIELD(cfg, PLL_Q) == 0)
        return -1;
    long div_q = 1 << __METAL_GET_FIELD(cfg, PLL_Q);

    /* In addition to the dividers inherent in the PLL, there's an additional
     * clock divider that lives after the PLL and lets us pick a more
     * interesting range of frequencies. */
    long pllout = (((ref_hz / div_r) * mul_f) / div_q);
    if (__METAL_GET_FIELD(div, DIV_1))
        return pllout;

    return pllout / (2 * (__METAL_GET_FIELD(div, DIV_DIV) + 1));
}

/* The PLL needs 100 usec to stabilize before we test PLL_LOCK. Since LFROSC
 * on all targets with the FE310-G000 PLL runs at 32768 Hz, we need to wait
 * at least
 *
 *   ceil(100 usec * 32768 ticks/sec * 1 sec / 1000000 usec) = 4 ticks
 *
 * of mtime before we test PLL_LOCK.
 *
 * TODO: Determine the mtime timebase at compile or runtime and use that
 * here.
 */
#define PLL_LOCK_WAIT_TICKS 4

/* Configure the PLL and wait for it to lock */
static void configure_pll(struct metal_clock clock,
                          const struct pll_config_t *config) {

    PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG) &= ~PLL_R;
    PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG) |= PLL_R_SHIFT(config->r);

    PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG) &= ~PLL_F;
    PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG) |= PLL_F_SHIFT(config->f);

    PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG) &= ~PLL_Q;
    PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG) |= PLL_Q_SHIFT(config->q);

    if (config->d < 0) {
        /* disable final divider */
        PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLOUTDIV) |= DIV_1;

        PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLOUTDIV) &= ~DIV_DIV;
        PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLOUTDIV) |= PLL_DIV_SHIFT(1);
    } else {
        PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLOUTDIV) &= ~DIV_1;

        PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLOUTDIV) &= ~DIV_DIV;
        PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLOUTDIV) |=
            PLL_DIV_SHIFT(config->d);
    }

    PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG) &= ~PLL_BYPASS;

    /* Wait for the PLL to stabilize before testing it for lock */
    struct metal_cpu cpu = metal_cpu_get(metal_cpu_get_current_hartid());
    uint64_t mtime = metal_cpu_get_mtime(cpu);
    uint64_t mtime_end = mtime + PLL_LOCK_WAIT_TICKS;
    while (mtime <= mtime_end) {
        mtime = metal_cpu_get_mtime(cpu);
    }

    /* Wait for PLL to lock */
    while ((PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG) & PLL_LOCK) == 0)
        ;
}

uint64_t sifive_fe310_g000_pll_set_rate_hz(struct metal_clock clock,
                                           uint64_t rate) {
    pre_rate_change_callbacks();

    /* We can't modify the PLL if coreclk is driven by it, so switch it off */
    if ((PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG) & PLL_SEL) != 0) {
        PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG) &= ~PLL_SEL;
    }

    /* There's a clock mux before the PLL that selects between the HFROSC and
     * the HFXOSC as the PLL's input clock. */
    uint64_t ref_hz;
    if ((PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG) & PLL_REFSEL) == 0) {
        ref_hz = sifive_fe310_g000_hfrosc_get_rate_hz(PLL_HFROSC(pll));
    } else {
        ref_hz = sifive_fe310_g000_hfrosc_get_rate_hz(PLL_HFXOSC(pll));
    }

    if ((ref_hz * 3 / 4) <= rate && (ref_hz * 5 / 4) >= rate) {
        /* if the desired rate is within 75%-125% of the input clock, bypass the
         * PLL
         */
        PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG) |= PLL_BYPASS;
    } else {
        int config_index = find_closest_config(ref_hz, rate);
        if (config_index != -1) {
            configure_pll(clock, &(pll_configs[config_index]));
        } else {
            /* unable to find a valid configuration */
            PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG) |= PLL_BYPASS;
        }
    }

    /* Enable the PLL */
    PLL_REGW(METAL_SIFIVE_FE310_G000_PRCI_PLLCFG) |= PLL_SEL;

    post_rate_change_callbacks();

    return sifive_fe310_g000_pll_get_rate_hz(clock);
}

#endif /* METAL_SIFIVE_FE310_G000_PLL */

typedef int no_empty_translation_units;
