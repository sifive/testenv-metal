#------------------------------------------------------------------------------
# Metal framework
# Beware that cache support is disabled for now (clang syntax issues)
#------------------------------------------------------------------------------

IF ( ENABLE_METAL )
  ADD_LIBRARY (metal
    src/atomic.c
    src/button.c
    # src/cache.c
    src/clock.c
    src/cpu.c
    src/drivers
    src/entry.S
    src/gpio.c
    src/hpm.c
    src/i2c.c
    src/init.c
    src/interrupt.c
    src/led.c
    src/lock.c
    src/memory.c
    src/pmp.c
    src/privilege.c
    src/pwm.c
    src/rtc.c
    src/scrub.S
    src/shutdown.c
    src/spi.c
    src/switch.c
    src/synchronize_harts.c
    src/time.c
    src/timer.c
    src/trap.S
    src/tty.c
    src/uart.c
    src/vector.S
    src/watchdog.c
    src/drivers/fixed-clock.c
    src/drivers/fixed-factor-clock.c
    src/drivers/inline.c
    src/drivers/riscv_clint0.c
    src/drivers/riscv_cpu.c
    src/drivers/riscv_plic0.c
    src/drivers/sifive_buserror0.c
    src/drivers/sifive_ccache0.c
    src/drivers/sifive_clic0.c
    src/drivers/sifive_fe310-g000_hfrosc.c
    src/drivers/sifive_fe310-g000_hfxosc.c
    src/drivers/sifive_fe310-g000_lfrosc.c
    src/drivers/sifive_fe310-g000_pll.c
    src/drivers/sifive_fe310-g000_prci.c
    # src/drivers/ifive_fu540-c000_l2.c
    src/drivers/sifive_global-external-interrupts0.c
    src/drivers/sifive_gpio-buttons.c
    src/drivers/sifive_gpio-leds.c
    src/drivers/sifive_gpio-switches.c
    src/drivers/sifive_gpio0.c
    src/drivers/sifive_i2c0.c
    src/drivers/sifive_local-external-interrupts0.c
    src/drivers/sifive_pwm0.c
    src/drivers/sifive_rtc0.c
    src/drivers/sifive_simuart0.c
    src/drivers/sifive_spi0.c
    src/drivers/sifive_test0.c
    src/drivers/sifive_trace.c
    src/drivers/sifive_uart0.c
    src/drivers/sifive_wdog0.c
    src/drivers/ucb_htif0.c
  )
ENDIF ()
