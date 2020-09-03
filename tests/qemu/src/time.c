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


static inline void
_time_irq_wfi(void)
{
    __asm__ volatile ("wfi");
}

static void
_timer_irq_handler(int id, void * opaque) {
    struct context * ctx = (struct context *)opaque;
    uint64_t tick = metal_cpu_get_mtime(ctx->ct_cpu);
    printf("%lx -> %p\n", tick, ctx->ct_cpu);
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
_time_irq_init(void)
{
    memset(_ctxs, 0, sizeof(_ctxs));

    for (unsigned int hart_id=0; hart_id<ARRAY_SIZE(_ctxs); hart_id++) {
        struct context * ctx = &_ctxs[hart_id];

        ctx->ct_cpu = metal_cpu_get(hart_id);
        TEST_ASSERT_NOT_NULL_MESSAGE(ctx->ct_cpu, "Cannot get CPU");

        printf("HartId %d, CPU %p\n", hart_id, ctx->ct_cpu);

        ctx->ct_cpu_intr = metal_cpu_interrupt_controller(ctx->ct_cpu);
        TEST_ASSERT_NOT_NULL_MESSAGE(ctx->ct_cpu_intr, "Cannot get CPU controller");
        metal_interrupt_init(ctx->ct_cpu_intr);
        metal_interrupt_disable(ctx->ct_cpu_intr, 0);

        ctx->ct_tmr_intr = metal_cpu_timer_interrupt_controller(ctx->ct_cpu);
        TEST_ASSERT_NOT_NULL_MESSAGE(ctx->ct_cpu_intr, "Cannot get CLINT");

        ctx->ct_sw_intr = metal_cpu_software_interrupt_controller(ctx->ct_cpu);
        TEST_ASSERT_NOT_NULL_MESSAGE(ctx->ct_sw_intr, "Cannot get CLINT");
    }

    for (unsigned int hart_id=0; hart_id<ARRAY_SIZE(_ctxs); hart_id++) {
        struct context * ctx = &_ctxs[hart_id];

        metal_interrupt_init(ctx->ct_tmr_intr);
        ctx->ct_tmr_id = metal_cpu_timer_get_interrupt_id(ctx->ct_cpu);

        metal_interrupt_init(ctx->ct_sw_intr);
        ctx->ct_sw_id = metal_cpu_software_get_interrupt_id(ctx->ct_cpu);

        int rc;
        rc = metal_interrupt_register_handler(ctx->ct_tmr_intr, ctx->ct_tmr_id,
                                              _timer_irq_handler, ctx);
        TEST_ASSERT_FALSE_MESSAGE(rc, "Cannot register IRQ handler");

        printf("Registered CTX %p for HartId %u", hart_id);

        metal_interrupt_disable(ctx->ct_tmr_intr, ctx->ct_tmr_id);
        metal_interrupt_disable(ctx->ct_sw_intr, ctx->ct_tmr_id);
        break;
    }

    printf("IRQ initialized\n");
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

static void
_time_irq_signal_hart(unsigned int hart_id, bool enable)
{
    printf("Wake up hartid %u\n", hart_id);

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

    METAL_REG32(msip_base, hart_id<<2u) = enable ? 1u : 0u;
}

static void
_time_irq_main(struct context * ctx)
{
    printf("Enter test Hart %d %p\n",
           metal_cpu_get_current_hartid(), ctx->ct_cpu);

    uint64_t tick;
    tick = metal_cpu_get_mtime(ctx->ct_cpu);
    printf("Tick: %lx\n", tick);

    metal_cpu_set_mtimecmp(ctx->ct_cpu, tick+LF_CLOCK_PERIOD);
    metal_interrupt_enable(ctx->ct_tmr_intr, ctx->ct_tmr_id);
    metal_interrupt_enable(ctx->ct_cpu_intr, 0);

    unsigned int loop;
    uint64_t delay;
    uint64_t period;

    loop = WAIT_LOOP_COUNT;
    printf("Hart %d: Start wait loop\n", metal_cpu_get_current_hartid());
    while ( loop-- ) {
        _time_irq_wfi();
        puts(".");
    }
    tick = metal_cpu_get_mtime(ctx->ct_cpu);
    TEST_ASSERT_MESSAGE(ctx->ct_first_tick, "No tick registered");
    TEST_ASSERT_MESSAGE(ctx->ct_tick_count, "No tick registered");
    delay = tick - ctx->ct_first_tick;
    period = delay/ctx->ct_tick_count;
    printf("Hart %d: End wait loop %lu %lu ms\n",
            metal_cpu_get_current_hartid(),
            period, 1000*period/TIME_BASE);

    _time_irq_fini(ctx);
}

static void
_time_irq_sequence(unsigned int hart_id)
{
    struct context * ctx = &_ctxs[hart_id];

    // run with current hart
    _time_irq_main(ctx);

    _time_irq_fini(ctx);
}

static void
_time_irq_main_hart_1(void)
{
    // be sure the SW interrupt if clear, as we do not have a handler for it
    // and we do not want metal to call a default handler whenever the hart #1
    // is configured to handle exceptions, which is what the timer interrupt
    // is about to do
    _time_irq_signal_hart(1u, false);

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

    _time_irq_init();
}

TEST_TEAR_DOWN(time_irq)
{
    for (unsigned int hid=0; hid<ARRAY_SIZE(_ctxs); hid++) {
        _time_irq_fini(&_ctxs[hid]);
    }
}

TEST(time_irq, lf_clock)
{
    // register the task to call when hart #1 is awaken
    qemu_register_hart_task(1u, &_time_irq_main_hart_1);

    // run sequence from hart #0
    _time_irq_sequence(0u);

    // awake hart #1
    _time_irq_signal_hart(1u, true);

    // take a nap till hart #1 signal hart #0
    _time_irq_wfi();

    _time_irq_signal_hart(0u, false);

    printf("Done\n");
}

TEST_GROUP_RUNNER(time_irq)
{
    RUN_TEST_CASE(time_irq, lf_clock);
}
