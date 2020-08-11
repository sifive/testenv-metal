#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "metal/machine.h"
#include "metal/tty.h"
#include "api/hardware/v0.5/random/hca_trng.h"
#include "api/hardware/v0.5/sifive_hca-0.5.x.h"
#include "api/hardware/hca_utils.h"
#include "api/hardware/hca_macro.h"

#define HCA_TRNG_IRQ_CHANNEL 24

#define DEBUG_HCA

#ifdef DEBUG_HCA
# define LPRINTF(_f_, _l_, _msg_, ...) \
    printf("%s[%d] " _msg_ "\n", _f_, _l_, ##__VA_ARGS__)
# define PRINTF(_msg_, ...) \
    LPRINTF(__func__, __LINE__, _msg_, ##__VA_ARGS__)
#else // DEBUG_HCA
# define LPRINTF(_f_, _l_, _msg_, ...)
# define PRINTF(_msg_, ...)
#endif

static const metal_scl_t scl = {
    .hca_base = METAL_SIFIVE_HCA_0_BASE_ADDRESS,
};

void trng(void) {
    PRINTF("START Poll mode");

    int32_t rc;

    rc = hca_trng_init(&scl);
    if ( rc ) {
        PRINTF("Cannot init TRNG: %d", rc);
        return;
    }

    uint32_t out = 0;

    for(unsigned int ix=0; ix<4; ix++) {
        rc = hca_trng_getdata(&scl, &out);
        if ( rc ) {
            PRINTF("Cannot generate RNG: %d", rc);
            return;
        }
        PRINTF("RNG: 0x%08x", out);
    }

    hca_setfield32(&scl, METAL_SIFIVE_HCA_TRNG_CR, 1,
                   HCA_REGISTER_TRNG_CR_BURSTEN_OFFSET,
                   HCA_REGISTER_TRNG_CR_BURSTEN_MASK);


    for(unsigned int ix=0; ix<4; ix++) {
        rc = hca_trng_getdata(&scl, &out);
        if ( rc ) {
            PRINTF("Cannot generate RNG: %d", rc);
            return;
        }
        PRINTF("RNG: 0x%08x", out);
    }

    hca_setfield32(&scl, METAL_SIFIVE_HCA_TRNG_CR, 0,
                   HCA_REGISTER_TRNG_CR_BURSTEN_OFFSET,
                   HCA_REGISTER_TRNG_CR_BURSTEN_MASK);


    for(unsigned int ix=0; ix<4; ix++) {
        rc = hca_trng_getdata(&scl, &out);
        if ( rc ) {
            PRINTF("Cannot generate RNG: %d", rc);
            return;
        }
        PRINTF("RNG: 0x%08x", out);
    }
}

void hca_irq_handler(int id, void * opaque)
{
    static unsigned int count;
    count += 1;

    if ( count == 4 ) {
        // switch to burst mode
        hca_setfield32(&scl, METAL_SIFIVE_HCA_TRNG_CR, 1,
               HCA_REGISTER_TRNG_CR_BURSTEN_OFFSET,
               HCA_REGISTER_TRNG_CR_BURSTEN_MASK);
    }
    if ( count < 8 ) {
        uint32_t out;
        out = METAL_REG32(scl.hca_base, METAL_SIFIVE_HCA_TRNG_DATA);
        PRINTF("RNG: 0x%08x", out);
    } else  {
        bool * resume = (bool *)opaque;
        *resume = false;
    }
}

void trng_irq(void) {
    PRINTF("START IRQ mode");

    int32_t rc;

    rc = hca_trng_init(&scl);
    if ( rc ) {
        PRINTF("Cannot init TRNG: %d", rc);
        return;
    }

    // Lets get the CPU and and its interrupt
    struct metal_cpu *cpu;
    cpu = metal_cpu_get(metal_cpu_get_current_hartid());
    if (cpu == NULL) {
        PRINTF("Abort. CPU is null.");
        return;
    }

    struct metal_interrupt *cpu_intr;
    cpu_intr = metal_cpu_interrupt_controller(cpu);
    if (cpu_intr == NULL) {
        PRINTF("Abort. CPU interrupt controller is null.");
    }
    metal_interrupt_init(cpu_intr);

    struct metal_interrupt * plic;
    plic = metal_interrupt_get_controller(METAL_PLIC_CONTROLLER, 0);
    if ( ! plic ) {
        PRINTF("No PLIC?");
        return;
    }
    metal_interrupt_init(plic);

    bool resume = true;

    rc = metal_interrupt_register_handler(plic, HCA_TRNG_IRQ_CHANNEL,
                                          &hca_irq_handler, &resume);
    if ( rc ) {
        PRINTF("Cannot register TRNG handler");
        return;
    }

    rc = metal_interrupt_enable(plic, HCA_TRNG_IRQ_CHANNEL);
    if ( rc ) {
        PRINTF("Cannot enable TRNG handler");
        return;
    }

    hca_setfield32(&scl, METAL_SIFIVE_HCA_TRNG_CR, 0,
           HCA_REGISTER_TRNG_CR_BURSTEN_OFFSET,
           HCA_REGISTER_TRNG_CR_BURSTEN_MASK);

    metal_interrupt_set_threshold(plic, 1);
    metal_interrupt_set_priority(plic, HCA_TRNG_IRQ_CHANNEL, 2);
    metal_interrupt_enable(cpu_intr, 0);
    // __metal_interrupt_external_enable();

    hca_setfield32(&scl, METAL_SIFIVE_HCA_TRNG_CR, 1,
                   HCA_REGISTER_TRNG_CR_RNDIRQEN_OFFSET,
                   HCA_REGISTER_TRNG_CR_RNDIRQEN_MASK);

    PRINTF("wait");
    while ( resume ) {
        __asm__ volatile ("wfi");
        // PRINTF("irq");
    }

    rc = metal_interrupt_disable(plic, HCA_TRNG_IRQ_CHANNEL);
    if ( rc ) {
        PRINTF("Cannot disable TRNG handler");
        return;
    }
    // clear interrupt
    METAL_REG32(scl.hca_base, METAL_SIFIVE_HCA_TRNG_DATA);

    PRINTF("IRQ mode STOPPED");

    __asm__ volatile ("wfi");

    PRINTF("IRQ after STOP");
}

int main(void) {

    trng();
    trng_irq();

    return 0;
}
