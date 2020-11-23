#ifndef __METAL_DT_CACHE__H
#define __METAL_DT_CACHE__H

/* GENERATED FILE
 * --------------
 * This file is generated from a template based on the content of the target
 * Devicetree.
 */
#include <stdbool.h>


#include <metal/private/metal_private_cpu.h>

static const bool dt_l1_icache[__METAL_DT_NUM_HARTS] = {
    false,
    false,
};
static const bool dt_l1_dcache[__METAL_DT_NUM_HARTS] = {
    false,
    false,
};

#define HART_HAS_L1_ICACHE(hartid) (dt_l1_icache[(hartid)])
#define HART_HAS_L1_DCACHE(hartid) (dt_l1_dcache[(hartid)])


#endif /* ! __METAL_DT_CACHE__H */
