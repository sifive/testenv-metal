#ifndef __METAL_DT_PMP__H
#define __METAL_DT_PMP__H

/* GENERATED FILE
 * --------------
 * This file is generated from a template based on the content of the target
 * Devicetree.
 */

#include <metal/private/metal_private_cpu.h>

static const uint8_t dt_pmp_regions[__METAL_DT_NUM_HARTS] = {
        0U,
        0U,
};

#define PMP_REGIONS(hartid) (dt_pmp_regions[(hartid)])



#endif /* ! __METAL_DT_PMP__H */
