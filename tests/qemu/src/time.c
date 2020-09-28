#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
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
// Macros
//-----------------------------------------------------------------------------

#undef PRINTF
#define PRINTF(_msg_, ...) \
    printf("{%d} %s[%d] "_msg_"\n", \
           metal_cpu_get_current_hartid(), __func__, __LINE__, ##__VA_ARGS__)

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


static inline void
_time_irq_wfi(void)
{
    __asm__ volatile ("wfi");
}

static void
_timer_irq_handler(int id, void * opaque) {
    struct context * ctx = (struct context *)opaque;
    uint64_t tick = metal_cpu_get_mtime(ctx->ct_cpu);
    metal_cpu_set_mtimecmp(ctx->ct_cpu, tick+LF_CLOCK_PERIOD);
    if ( ! ctx->ct_first_tick ) {
        ctx->ct_first_tick = tick;
    } else {
        ctx->ct_tick_count += 1u;
    }
    // avoid using printf from IRQ
    char str[] = { '^', 'T', '0' + metal_cpu_get_current_hartid(), '\n', '\0'};
    puts(str);
}

static void
_time_irq_init(unsigned int hart_id)
{
    struct context * ctx = &_ctxs[hart_id];

    ctx->ct_cpu = metal_cpu_get(hart_id);
    TEST_ASSERT_NOT_NULL_MESSAGE(ctx->ct_cpu, "Cannot get CPU");

    PRINTF("HartId %d, CPU %p", hart_id, ctx->ct_cpu);

    ctx->ct_cpu_intr = metal_cpu_interrupt_controller(ctx->ct_cpu);
    TEST_ASSERT_NOT_NULL_MESSAGE(ctx->ct_cpu_intr, "Cannot get CPU controller");
    metal_interrupt_init(ctx->ct_cpu_intr);
    metal_interrupt_disable(ctx->ct_cpu_intr, 0);

    ctx->ct_tmr_intr = metal_cpu_timer_interrupt_controller(ctx->ct_cpu);
    TEST_ASSERT_NOT_NULL_MESSAGE(ctx->ct_cpu_intr, "Cannot get CLINT");

    // there are bugs in Freedom Metal which prevents the initialization
    // of any but the first first core connnected to the same clint
    // so for the purpose of this test, trick Metal to think the Clint
    // initialisation has never been done, so it can actually perform the
    // init of any core.
    ((struct __metal_driver_riscv_clint0 *)(ctx->ct_tmr_intr))->init_done = 0;

    ctx->ct_sw_intr = metal_cpu_software_interrupt_controller(ctx->ct_cpu);
    TEST_ASSERT_NOT_NULL_MESSAGE(ctx->ct_sw_intr, "Cannot get CLINT");
}

static void
_time_irq_enable(unsigned int hart_id)
{
    struct context * ctx = &_ctxs[hart_id];

    metal_interrupt_init(ctx->ct_tmr_intr);
    ctx->ct_tmr_id = metal_cpu_timer_get_interrupt_id(ctx->ct_cpu);

    metal_interrupt_init(ctx->ct_sw_intr);
    ctx->ct_sw_id = metal_cpu_software_get_interrupt_id(ctx->ct_cpu);

    int rc;
    rc = metal_interrupt_register_handler(ctx->ct_tmr_intr, ctx->ct_tmr_id,
                                          _timer_irq_handler, ctx);
    TEST_ASSERT_FALSE_MESSAGE(rc, "Cannot register IRQ handler");

    metal_interrupt_disable(ctx->ct_tmr_intr, ctx->ct_tmr_id);
    metal_interrupt_disable(ctx->ct_sw_intr, ctx->ct_tmr_id);
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
}

static void
_time_irq_signal_hart(unsigned int hart_id, bool enable)
{
    if ( enable ) {
        PRINTF("Wake up hartid %u", hart_id);
    }

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

    // there is no Metal API to set, only to get
    METAL_REG32(msip_base, hart_id<<2u) = enable ? 1u : 0u;
}

static void
_time_irq_sequence(unsigned int hart_id)
{
    struct context * ctx = &_ctxs[hart_id];

    PRINTF("Enter test Hart %p", ctx->ct_cpu);

    uint64_t tick;
    tick = metal_cpu_get_mtime(ctx->ct_cpu);
    PRINTF("Tick: %" PRIx64, tick);

    metal_cpu_set_mtimecmp(ctx->ct_cpu, tick+LF_CLOCK_PERIOD);
    metal_interrupt_enable(ctx->ct_tmr_intr, ctx->ct_tmr_id);
    metal_interrupt_enable(ctx->ct_cpu_intr, 0);

    unsigned int loop;
    uint64_t delay;
    uint64_t period;

    loop = WAIT_LOOP_COUNT;
    PRINTF("Start wait loop");
    while ( loop-- ) {
        _time_irq_wfi();
    }
    if ( ctx->ct_tmr_intr ) {
        metal_interrupt_disable(ctx->ct_tmr_intr, ctx->ct_tmr_id);
    }
    if ( ctx->ct_sw_intr ) {
        metal_interrupt_disable(ctx->ct_sw_intr, ctx->ct_sw_id);
    }

    tick = metal_cpu_get_mtime(ctx->ct_cpu);
    TEST_ASSERT_MESSAGE(ctx->ct_first_tick, "No tick registered");
    TEST_ASSERT_MESSAGE(ctx->ct_tick_count, "No tick registered");
    delay = tick - ctx->ct_first_tick;
    period = delay/ctx->ct_tick_count;
    PRINTF("End wait loop %" PRIu64 " %" PRIu64 " ms",
           period, 1000*period/TIME_BASE);
}

static void
_time_irq_main_hart_1(void)
{
    // be sure the SW interrupt if clear, as we do not have a handler for it
    // and we do not want metal to call a default handler whenever the hart #1
    // is configured to handle exceptions, which is what the timer interrupt
    // is about to do
    _time_irq_signal_hart(1u, false);

    _time_irq_init(1u);
    _time_irq_enable(1u);

    // run sequence from hart #1
    _time_irq_sequence(1u);

    // signal hart #0
    _time_irq_signal_hart(0u, true);
}

//-----------------------------------------------------------------------------
// Unity wrappers
//-----------------------------------------------------------------------------

TEST_GROUP(time_irq);

TEST_SETUP(time_irq)
{
    puts("\n");
}

TEST_TEAR_DOWN(time_irq)
{
}

TEST(time_irq, lf_clock)
{
    _time_irq_signal_hart(0u, false);

    // register the task to call when hart #1 is awaken
    qemu_register_hart_task(1u, &_time_irq_main_hart_1);

    _time_irq_init(0u);
    _time_irq_enable(0u);

    // run sequence from hart #0
    _time_irq_sequence(0u);

    // awake hart #1
    _time_irq_signal_hart(1u, true);

    // take a nap till hart #1 signal hart #0
    _time_irq_wfi();
    _time_irq_signal_hart(0u, false);

    PRINTF("Waken up");
}

TEST_GROUP_RUNNER(time_irq)
{
    RUN_TEST_CASE(time_irq, lf_clock);
}
