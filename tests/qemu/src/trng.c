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
#include "unity_fixture.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define HCA_TRNG_IRQ_CHANNEL 24
#define TRNG_MAX_RESULTS     8

static const metal_scl_t scl = {
    .hca_base = METAL_SIFIVE_HCA_0_BASE_ADDRESS,
};

//-----------------------------------------------------------------------------
// Type definitions
//-----------------------------------------------------------------------------

struct trng_results
{
    size_t   tr_count;
    bool     tr_resume;
    uint32_t tr_values[TRNG_MAX_RESULTS];
 };

//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_a_) (sizeof((_a_))/sizeof((_a_)[0]))
#endif // ARRAY_SIZE

#define ALIGN(_a_) __attribute__((aligned((_a_))))

//-----------------------------------------------------------------------------
// Debug
//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------

static struct trng_results _trng_results;

//-----------------------------------------------------------------------------
// TRNG tests
//-----------------------------------------------------------------------------

TEST_GROUP(trng);

TEST_SETUP(trng) {}

TEST_TEAR_DOWN(trng) {}

TEST(trng, poll)
{
    PRINTF("START Poll mode");

    int32_t rc;

    rc = hca_trng_init(&scl);
    TEST_ASSERT_FALSE_MESSAGE(rc, "Cannot init TRNG");

    uint32_t out = 0;

    for(unsigned int ix=0; ix<4; ix++) {
        rc = hca_trng_getdata(&scl, &out);
        TEST_ASSERT_FALSE_MESSAGE(rc, "Cannot generate TRNG");
        // there is a 1/1^32 chance to get a zeroed valid value...
        TEST_ASSERT_TRUE_MESSAGE(out != 0, "Zero value found");
        //PRINTF("RNG: 0x%08x", out);
    }

    hca_setfield32(&scl, METAL_SIFIVE_HCA_TRNG_CR, 1,
                   HCA_REGISTER_TRNG_CR_BURSTEN_OFFSET,
                   HCA_REGISTER_TRNG_CR_BURSTEN_MASK);


    for(unsigned int ix=0; ix<4; ix++) {
        rc = hca_trng_getdata(&scl, &out);
        TEST_ASSERT_FALSE_MESSAGE(rc, "Cannot generate PRNG");
        // there is a 1/1^32 chance to get a zeroed valid value...
        TEST_ASSERT_TRUE_MESSAGE(out != 0, "Zero value found");
        //PRINTF("RNG: 0x%08x", out);
    }

    hca_setfield32(&scl, METAL_SIFIVE_HCA_TRNG_CR, 0,
                   HCA_REGISTER_TRNG_CR_BURSTEN_OFFSET,
                   HCA_REGISTER_TRNG_CR_BURSTEN_MASK);


    for(unsigned int ix=0; ix<4; ix++) {
        rc = hca_trng_getdata(&scl, &out);
        TEST_ASSERT_FALSE_MESSAGE(rc, "Cannot generate TRNG");
        // there is a 1/1^32 chance to get a zeroed valid value...
        TEST_ASSERT_TRUE_MESSAGE(out != 0, "Zero value found");
        //PRINTF("RNG: 0x%08x", out);
    }
}

void hca_irq_handler(int id, void * opaque)
{
    struct trng_results * results = (struct trng_results *)opaque;

    if ( results->tr_count == (ARRAY_SIZE(results->tr_values)/2u - 1u) ) {
        // switch to burst mode
        hca_setfield32(&scl, METAL_SIFIVE_HCA_TRNG_CR, 1,
               HCA_REGISTER_TRNG_CR_BURSTEN_OFFSET,
               HCA_REGISTER_TRNG_CR_BURSTEN_MASK);
    }
    if ( results->tr_count < ARRAY_SIZE(results->tr_values) ) {
        uint32_t out;
        out = METAL_REG32(scl.hca_base, METAL_SIFIVE_HCA_TRNG_DATA);
        //PRINTF("RNG: 0x%08x", out);
        results->tr_values[results->tr_count++] = out;
    } else  {
        results->tr_resume = false;
    }
}

TEST(trng, irq)
{
    PRINTF("START IRQ mode");

    int32_t rc;

    rc = hca_trng_init(&scl);
    TEST_ASSERT_FALSE_MESSAGE(rc, "Cannot init TRNG");

    // Lets get the CPU and and its interrupt
    struct metal_cpu *cpu;
    cpu = metal_cpu_get(metal_cpu_get_current_hartid());
    TEST_ASSERT_NOT_NULL_MESSAGE(cpu, "Cannot get CPU");

    struct metal_interrupt *cpu_intr;
    cpu_intr = metal_cpu_interrupt_controller(cpu);
    TEST_ASSERT_NOT_NULL_MESSAGE(cpu_intr, "Cannot get CPU controller");
    metal_interrupt_init(cpu_intr);

    struct metal_interrupt * plic;
    plic = metal_interrupt_get_controller(METAL_PLIC_CONTROLLER, 0);
    TEST_ASSERT_NOT_NULL_MESSAGE(plic, "Cannot get PLIC");
    metal_interrupt_init(plic);

    memset(&_trng_results, 0, sizeof(_trng_results));
    _trng_results.tr_resume = true;

    rc = metal_interrupt_register_handler(plic, HCA_TRNG_IRQ_CHANNEL,
                                          &hca_irq_handler, &_trng_results);
    TEST_ASSERT_FALSE_MESSAGE(rc, "Cannot register IRQ handler");

    rc = metal_interrupt_enable(plic, HCA_TRNG_IRQ_CHANNEL);
    TEST_ASSERT_FALSE_MESSAGE(rc, "Cannot enable IRQ");

    hca_setfield32(&scl, METAL_SIFIVE_HCA_TRNG_CR, 0,
           HCA_REGISTER_TRNG_CR_BURSTEN_OFFSET,
           HCA_REGISTER_TRNG_CR_BURSTEN_MASK);

    metal_interrupt_set_threshold(plic, 1);
    metal_interrupt_set_priority(plic, HCA_TRNG_IRQ_CHANNEL, 2);
    metal_interrupt_enable(cpu_intr, 0);

    hca_setfield32(&scl, METAL_SIFIVE_HCA_TRNG_CR, 1,
                   HCA_REGISTER_TRNG_CR_RNDIRQEN_OFFSET,
                   HCA_REGISTER_TRNG_CR_RNDIRQEN_MASK);

    while ( _trng_results.tr_resume ) {
        // do not use WFI as there is a small time vulnerability window
        // see DMA test for a workaround with the CLINT timer.
    }

    rc = metal_interrupt_disable(plic, HCA_TRNG_IRQ_CHANNEL);
    TEST_ASSERT_FALSE_MESSAGE(rc, "Cannot diable IRQ");

    // clear interrupt
    METAL_REG32(scl.hca_base, METAL_SIFIVE_HCA_TRNG_DATA);

    TEST_ASSERT_EQUAL_MESSAGE(_trng_results.tr_count, TRNG_MAX_RESULTS,
                              "Missing RNG values");
    for(unsigned int ix=0; ix<_trng_results.tr_count; ix++) {
        // there is a slight chance up to (TRNG_MAX_RESULTS/1^32) to get
        // zeroes... anyway, for now, consider is it never zeroed.
        TEST_ASSERT_TRUE_MESSAGE(_trng_results.tr_values[ix] != 0,
                                 "Zero value found");
    }
}

TEST_GROUP_RUNNER(trng)
{
    RUN_TEST_CASE(trng, poll);
    RUN_TEST_CASE(trng, irq);
}

#if 0
int main(void) {

    trng();
    trng_irq();

    return 0;
}
#endif