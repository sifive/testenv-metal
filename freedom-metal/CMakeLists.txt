#------------------------------------------------------------------------------
# Metal framework
# Beware that cache support is disabled for now (clang syntax issues)
#------------------------------------------------------------------------------

IF ( ENABLE_METAL )
  ADD_LIBRARY (metal
    sifive-blocks/src/drivers/sifive_aon0.c
    sifive-blocks/src/drivers/sifive_buserror0.c
    sifive-blocks/src/drivers/sifive_ccache0.c
    sifive-blocks/src/drivers/sifive_clic0.c
    sifive-blocks/src/drivers/sifive_fe310_g000_hfrosc.c
    sifive-blocks/src/drivers/sifive_fe310_g000_hfxosc.c
    sifive-blocks/src/drivers/sifive_fe310_g000_lfrosc.c
    sifive-blocks/src/drivers/sifive_fe310_g000_pll.c
    sifive-blocks/src/drivers/sifive_gpio0.c
    sifive-blocks/src/drivers/sifive_gpio_buttons.c
    sifive-blocks/src/drivers/sifive_gpio_leds.c
    sifive-blocks/src/drivers/sifive_gpio_switches.c
    sifive-blocks/src/drivers/sifive_hca0.c
    sifive-blocks/src/drivers/sifive_hca0_aes.c
    sifive-blocks/src/drivers/sifive_hca0_sha.c
    sifive-blocks/src/drivers/sifive_hca0_trng.c
    sifive-blocks/src/drivers/sifive_i2c0.c
    sifive-blocks/src/drivers/sifive_l2pf0.c
    sifive-blocks/src/drivers/sifive_pwm0.c
    sifive-blocks/src/drivers/sifive_spi0.c
    sifive-blocks/src/drivers/sifive_test0.c
    sifive-blocks/src/drivers/sifive_uart0.c
    src/button.c
    # src/cache.c
    src/clock.c
    src/drivers/fixed_clock.c
    src/drivers/fixed_factor_clock.c
    src/drivers/riscv_clint0.c
    src/drivers/riscv_cpu.c
    src/drivers/riscv_cpu_intc.c
    src/drivers/riscv_plic0.c
    src/drivers/ucb_htif0.c
    src/exception.c
    src/gpio.c
    src/hpm.c
    src/init.c
    src/interrupt.c
    src/led.c
    src/lock.c
    src/pmp.c
    src/privilege.c
    src/rtc.c
    src/shutdown.c
    src/switch.c
    src/synchronize_harts.c
    src/tty.c
    src/uart.c
    src/watchdog.c
    src/entry.S
    src/trap.S
    ${XBSPDIR}/src/interrupt_handlers.c
    ${XBSPDIR}/src/interrupt_table.c
    ${XBSPDIR}/src/jump_table.S
  )
ENDIF ()

# Search directories with CMakeFiles.txt files, and load them
include_subprojects ()
