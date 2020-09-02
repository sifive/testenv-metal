#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "metal/machine.h"
#include "metal/io.h"
#include "unity_fixture.h"
#include "qemu.h"


//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define LF_CLOCK_FREQUENCY   4u // Hz
#define LF_CLOCK_PERIOD      ((TIME_BASE)/(LF_CLOCK_FREQUENCY))
#define WAIT_LOOP_COUNT      4u

//-----------------------------------------------------------------------------
// Missing declarations
//-----------------------------------------------------------------------------

extern void __metal_interrupt_software_enable(void);
extern void __metal_interrupt_software_disable(void);
extern void __metal_interrupt_timer_enable(void);
extern void __metal_interrupt_timer_disable(void);
extern void __metal_interrupt_external_enable(void);
extern void __metal_interrupt_external_disable(void);

//-----------------------------------------------------------------------------
// Type definitions
//-----------------------------------------------------------------------------

struct context
{
    struct metal_cpu       * ct_cpu;
    struct metal_interrupt * ct_cpu_intr;
    struct metal_interrupt * ct_tmr_intr;
    struct metal_interrupt * ct_sw_intr;
    int                      ct_tmr_id;
    int                      ct_sw_id;
    volatile uint64_t        ct_first_tick;
    volatile size_t          ct_tick_count;
};

//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------

static struct context _ctxs[2u];

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
    // avoid using printf from IRQ
    char str[] = { '^', 'T', '0' + metal_cpu_get_current_hartid(), '\n', '\0'};
    puts(str);
}

static void
_time_irq_signal_hart(unsigned int hartid)
{
    uintptr_t msip_base = 0;
    /* Get the base address of the MSIP registers */
    #ifdef __METAL_DT_RISCV_CLINT0_HANDLE
    msip_base = __metal_driver_sifive_clint0_control_base(
        __METAL_DT_RISCV_CLINT0_HANDLE);
    msip_base += METAL_RISCV_CLINT0_MSIP_BASE;
    #elif __METAL_DT_RISCV_CLIC0_HANDLE
    msip_base =
        __metal_driver_sifive_clic0_control_base(__METAL_DT_RISCV_CLIC0_HANDLE);
    msip_base += METAL_RISCV_CLIC0_MSIP_BASE;
    #else
    # error "MSIP not available"
    #endif
    METAL_REG32(msip_base, hartid<<2u) = 1u;
}

static inline void
_time_irq_wfi(void)
{
    __asm__ volatile ("wfi");
}

static void
_time_irq_init(struct context * ctx)
{
    memset(ctx, 0, sizeof(*ctx));

    uint32_t hart_id = metal_cpu_get_current_hartid();

    printf("HART id: %u\n", hart_id);

    ctx->ct_cpu = metal_cpu_get(hart_id);
    TEST_ASSERT_NOT_NULL_MESSAGE(ctx->ct_cpu, "Cannot get CPU");

    ctx->ct_cpu_intr = metal_cpu_interrupt_controller(ctx->ct_cpu);
    TEST_ASSERT_NOT_NULL_MESSAGE(ctx->ct_cpu_intr, "Cannot get CPU controller");
    metal_interrupt_init(ctx->ct_cpu_intr);

    ctx->ct_tmr_intr = metal_cpu_timer_interrupt_controller(ctx->ct_cpu);
    TEST_ASSERT_NOT_NULL_MESSAGE(ctx->ct_cpu_intr, "Cannot get CLINT");
    metal_interrupt_init(ctx->ct_tmr_intr);
    ctx->ct_tmr_id = metal_cpu_timer_get_interrupt_id(ctx->ct_cpu);

    ctx->ct_sw_intr = metal_cpu_software_interrupt_controller(ctx->ct_cpu);
    TEST_ASSERT_NOT_NULL_MESSAGE(ctx->ct_sw_intr, "Cannot get CLINT");
    metal_interrupt_init(ctx->ct_sw_intr);

    ctx->ct_sw_id = metal_cpu_software_get_interrupt_id(ctx->ct_cpu);

    int rc;
    rc = metal_interrupt_register_handler(ctx->ct_tmr_intr, ctx->ct_tmr_id,
                                          _timer_irq_handler, ctx);
    TEST_ASSERT_FALSE_MESSAGE(rc, "Cannot register IRQ handler");
}

static void
_time_irq_fini(struct context * ctx)
{
    if ( ctx->ct_tmr_intr ) {
        metal_interrupt_disable(ctx->ct_tmr_intr, ctx->ct_tmr_id);
    }
    if ( ctx->ct_sw_intr ) {
        metal_interrupt_disable(ctx->ct_sw_intr, ctx->ct_sw_id);
    }
    if ( ctx->ct_cpu_intr ) {
        metal_interrupt_disable(ctx->ct_cpu_intr, 0);
    }
    memset(ctx, 0, sizeof(*ctx));
}

//-----------------------------------------------------------------------------
// Unity wrappers
//-----------------------------------------------------------------------------

TEST_GROUP(time_irq);

TEST_SETUP(time_irq)
{
    for (unsigned int cix=0; cix<ARRAY_SIZE(_ctxs); cix++) {
        _time_irq_init(&_ctxs[cix]);
    }
}

TEST_TEAR_DOWN(time_irq)
{
    for (unsigned int cix=0; cix<ARRAY_SIZE(_ctxs); cix++) {
        _time_irq_fini(&_ctxs[cix]);
    }
}

static void
_time_irq_loop(struct context * ctx)
{
    _time_irq_init(ctx);

    metal_cpu_set_mtimecmp(ctx->ct_cpu,
                           metal_cpu_get_mtime(ctx->ct_cpu)+LF_CLOCK_PERIOD);
    metal_interrupt_enable(ctx->ct_tmr_intr, ctx->ct_tmr_id);
    metal_interrupt_enable(ctx->ct_cpu_intr, 0);

    volatile unsigned int x ;
    uint64_t tick;
    uint64_t delay;
    uint64_t period;

    x = WAIT_LOOP_COUNT;
    printf("Hart %d: Start wait loop\n", metal_cpu_get_current_hartid());
    while ( x-- ) {
        _time_irq_wfi();
    }
    tick = metal_cpu_get_mtime(ctx->ct_cpu);
    delay = tick - ctx->ct_first_tick;
    period = delay/ctx->ct_tick_count;
    printf("Hart %d: End wait loop %lu %lu ms\n",
            metal_cpu_get_current_hartid(),
           period, 1000*period/TIME_BASE);

    _time_irq_fini(ctx);
}

static void
_time_irq_loop_hartid_1(void)
{
    _time_irq_loop(&_ctxs[1u]);
    _time_irq_signal_hart(0u);
}

TEST(time_irq, lf_clock)
{
    puts("\n");

    qemu_register_hart_task(1u, &_time_irq_loop_hartid_1);

    // run with current hart
    _time_irq_loop(&_ctxs[0]);

    // start up next hard
    _time_irq_signal_hart(1u);
    // wait for hart task completion
    _time_irq_wfi();
    printf("Done\n");
}

TEST_GROUP_RUNNER(time_irq)
{
    RUN_TEST_CASE(time_irq, lf_clock);
}
