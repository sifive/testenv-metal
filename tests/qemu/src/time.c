#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "metal/machine.h"
#include "unity_fixture.h"


//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define TIME_BASE            32768u // cannot rely on buggy metal API
#define LF_CLOCK_FREQUENCY   4u // Hz
#define LF_CLOCK_PERIOD      ((TIME_BASE)/(LF_CLOCK_FREQUENCY))

//-----------------------------------------------------------------------------
// Type definitions
//-----------------------------------------------------------------------------

struct context
{
    struct metal_cpu       * ct_cpu;
    struct metal_interrupt * ct_cpu_intr;
    struct metal_interrupt * ct_tmr_intr;
    int                      ct_tmr_id;
    volatile uint64_t        ct_first_tick;
    volatile size_t          ct_tick_count;
};

//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------

struct context _ctx;

//-----------------------------------------------------------------------------
// Test implementation
//-----------------------------------------------------------------------------

static void
_timer_irq_handler(int id, void * opaque) {
    struct context * ct = (struct context *)opaque;
    uint64_t tick = metal_cpu_get_mtime(ct->ct_cpu);
    if ( ! ct->ct_first_tick ) {
        ct->ct_first_tick = tick;
    } else {
        ct->ct_tick_count += 1u;
    }
    metal_cpu_set_mtimecmp(ct->ct_cpu, tick+LF_CLOCK_PERIOD);
    puts("^T\n");
}

static void
_time_irq_init(struct context * ctx)
{
    memset(ctx, 0, sizeof(*ctx));

    // Lets get the CPU and and its interrupt
    ctx->ct_cpu = metal_cpu_get(metal_cpu_get_current_hartid());
    TEST_ASSERT_NOT_NULL_MESSAGE(ctx->ct_cpu, "Cannot get CPU");

    ctx->ct_cpu_intr = metal_cpu_interrupt_controller(ctx->ct_cpu);
    TEST_ASSERT_NOT_NULL_MESSAGE(ctx->ct_cpu_intr, "Cannot get CPU controller");
    metal_interrupt_init(ctx->ct_cpu_intr);

    // use a timer IRQ as an easier workaround for a time vulnarability
    // issue between WFI instruction and ISR. To avoid being stuck in WFI,
    // add an hearbeat.
    ctx->ct_tmr_intr = metal_cpu_timer_interrupt_controller(ctx->ct_cpu);
    TEST_ASSERT_NOT_NULL_MESSAGE(ctx->ct_cpu_intr, "Cannot get CLINT");
    metal_interrupt_init(ctx->ct_tmr_intr);

    ctx->ct_tmr_id = metal_cpu_timer_get_interrupt_id(ctx->ct_cpu);

    int rc;
    rc = metal_interrupt_register_handler(ctx->ct_tmr_intr, ctx->ct_tmr_id,
                                          _timer_irq_handler, ctx);
    TEST_ASSERT_FALSE_MESSAGE(rc, "Cannot register IRQ handler");

    metal_cpu_set_mtimecmp(ctx->ct_cpu,
                           metal_cpu_get_mtime(ctx->ct_cpu)+LF_CLOCK_PERIOD);
    metal_interrupt_enable(ctx->ct_tmr_intr, ctx->ct_tmr_id);
    metal_interrupt_enable(ctx->ct_cpu_intr, 0);
    puts("\n");
}

static void
_time_irq_fini(struct context * ctx)
{
    metal_interrupt_disable(ctx->ct_tmr_intr, ctx->ct_tmr_id);
    metal_interrupt_disable(ctx->ct_cpu_intr, 0);
}

//-----------------------------------------------------------------------------
// Unity wrappers
//-----------------------------------------------------------------------------

TEST_GROUP(time_irq);

TEST_SETUP(time_irq)
{
    _time_irq_init(&_ctx);
}

TEST_TEAR_DOWN(time_irq)
{
    _time_irq_fini(&_ctx);
}

TEST(time_irq, lf_clock)
{
    volatile unsigned int x = 20u;
    printf("Start wait loop\n");
    while ( x-- ) {
        __asm__ volatile ("wfi;");
    }
    const struct context * ct = &_ctx;
    uint64_t tick = metal_cpu_get_mtime(ct->ct_cpu);
    uint64_t delay = tick - ct->ct_first_tick;
    uint64_t period = delay/ct->ct_tick_count;
    printf("End wait loop %lu %lu ms\n", period, 1000*period/TIME_BASE);
}

TEST_GROUP_RUNNER(time_irq)
{
    RUN_TEST_CASE(time_irq, lf_clock);
}
