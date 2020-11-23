/* Copyright 2018 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#include <metal/platform.h>

#ifdef METAL_SIFIVE_GPIO0

#include <metal/gpio.h>
#include <metal/io.h>
#include <metal/private/metal_private_sifive_gpio0.h>

#define GPIO_REG(offset) ((uintptr_t)((base) + (offset)))
#define GPIO_REGW(offset)                                                      \
    (__METAL_ACCESS_ONCE((__metal_io_u32 *)GPIO_REG(offset)))

int sifive_gpio0_enable_input(struct metal_gpio gpio, int pin) {
    uintptr_t base = BASE_ADDR(gpio);

    GPIO_REGW(METAL_SIFIVE_GPIO0_INPUT_EN) |= (1 << pin);

    return 0;
}

int sifive_gpio0_disable_input(struct metal_gpio gpio, int pin) {
    uintptr_t base = BASE_ADDR(gpio);

    GPIO_REGW(METAL_SIFIVE_GPIO0_INPUT_EN) &= ~(1 << pin);

    return 0;
}

int sifive_gpio0_disable_output(struct metal_gpio gpio, int pin) {
    uintptr_t base = BASE_ADDR(gpio);

    GPIO_REGW(METAL_SIFIVE_GPIO0_OUTPUT_EN) &= ~(1 << pin);

    return 0;
}

int sifive_gpio0_enable_output(struct metal_gpio gpio, int pin) {
    uintptr_t base = BASE_ADDR(gpio);

    GPIO_REGW(METAL_SIFIVE_GPIO0_OUTPUT_EN) |= (1 << pin);

    return 0;
}

int sifive_gpio0_set_pin(struct metal_gpio gpio, int pin, int value) {
    uintptr_t base = BASE_ADDR(gpio);

    if (value == 1) {
        GPIO_REGW(METAL_SIFIVE_GPIO0_PORT) |= (1 << pin);
        return 0;
    }
    return sifive_gpio0_clear_pin(gpio, pin);
}

int sifive_gpio0_get_input_pin(struct metal_gpio gpio, int pin) {
    uintptr_t base = BASE_ADDR(gpio);

    return GPIO_REGW(METAL_SIFIVE_GPIO0_VALUE) & (1 << pin);
}

int sifive_gpio0_get_output_pin(struct metal_gpio gpio, int pin) {
    uintptr_t base = BASE_ADDR(gpio);

    return GPIO_REGW(METAL_SIFIVE_GPIO0_PORT) & (1 << pin);
}

int sifive_gpio0_clear_pin(struct metal_gpio gpio, int pin) {
    uintptr_t base = BASE_ADDR(gpio);

    GPIO_REGW(METAL_SIFIVE_GPIO0_PORT) &= ~(1 << pin);

    return 0;
}

int sifive_gpio0_toggle_pin(struct metal_gpio gpio, int pin) {
    uintptr_t base = BASE_ADDR(gpio);

    GPIO_REGW(METAL_SIFIVE_GPIO0_PORT) ^= (1 << pin);

    return 0;
}

int sifive_gpio0_enable_pinmux(struct metal_gpio gpio, long pin_mask,
                               long io_function_mask) {
    uintptr_t base = BASE_ADDR(gpio);

    GPIO_REGW(METAL_SIFIVE_GPIO0_IOF_SEL) |= pin_mask;

    GPIO_REGW(METAL_SIFIVE_GPIO0_IOF_EN) &= ~pin_mask;
    GPIO_REGW(METAL_SIFIVE_GPIO0_IOF_EN) |= io_function_mask;

    return 0;
}

int sifive_gpio0_disable_pinmux(struct metal_gpio gpio, long pin_mask) {
    uintptr_t base = BASE_ADDR(gpio);

    GPIO_REGW(METAL_SIFIVE_GPIO0_IOF_EN) &= ~pin_mask;

    return 0;
}

int sifive_gpio0_config_interrupt(struct metal_gpio gpio, int pin,
                                  enum metal_gpio_int_type int_type) {
    uintptr_t base = BASE_ADDR(gpio);

    switch (int_type) {
    case METAL_GPIO_INT_DISABLE:
        GPIO_REGW(METAL_SIFIVE_GPIO0_RISE_IE) &= ~(1 << pin);
        GPIO_REGW(METAL_SIFIVE_GPIO0_FALL_IE) &= ~(1 << pin);
        GPIO_REGW(METAL_SIFIVE_GPIO0_HIGH_IE) &= ~(1 << pin);
        GPIO_REGW(METAL_SIFIVE_GPIO0_LOW_IE) &= ~(1 << pin);
        break;
    case METAL_GPIO_INT_RISING:
        GPIO_REGW(METAL_SIFIVE_GPIO0_RISE_IE) |= (1 << pin);
        break;
    case METAL_GPIO_INT_FALLING:
        GPIO_REGW(METAL_SIFIVE_GPIO0_FALL_IE) |= (1 << pin);
        break;
    case METAL_GPIO_INT_BOTH_EDGE:
        GPIO_REGW(METAL_SIFIVE_GPIO0_RISE_IE) |= (1 << pin);
        GPIO_REGW(METAL_SIFIVE_GPIO0_FALL_IE) |= (1 << pin);
        break;
    case METAL_GPIO_INT_HIGH:
        GPIO_REGW(METAL_SIFIVE_GPIO0_HIGH_IE) |= (1 << pin);
        break;
    case METAL_GPIO_INT_LOW:
        GPIO_REGW(METAL_SIFIVE_GPIO0_LOW_IE) |= (1 << pin);
        break;
    case METAL_GPIO_INT_BOTH_LEVEL:
        GPIO_REGW(METAL_SIFIVE_GPIO0_HIGH_IE) |= (1 << pin);
        GPIO_REGW(METAL_SIFIVE_GPIO0_LOW_IE) |= (1 << pin);
        break;
    case METAL_GPIO_INT_MAX:
        GPIO_REGW(METAL_SIFIVE_GPIO0_RISE_IE) |= (1 << pin);
        GPIO_REGW(METAL_SIFIVE_GPIO0_FALL_IE) |= (1 << pin);
        GPIO_REGW(METAL_SIFIVE_GPIO0_HIGH_IE) |= (1 << pin);
        GPIO_REGW(METAL_SIFIVE_GPIO0_LOW_IE) |= (1 << pin);
        break;
    }

    struct metal_interrupt intc = INTERRUPT_PARENT(gpio);
    int id = INTERRUPT_ID_BASE(gpio) + pin;

    return metal_interrupt_enable(intc, id);
}

int sifive_gpio0_clear_interrupt(struct metal_gpio gpio, int pin,
                                 enum metal_gpio_int_type int_type) {
    uintptr_t base = BASE_ADDR(gpio);

    switch (int_type) {
    case METAL_GPIO_INT_RISING:
        GPIO_REGW(METAL_SIFIVE_GPIO0_RISE_IP) |= (1 << pin);
        break;
    case METAL_GPIO_INT_FALLING:
        GPIO_REGW(METAL_SIFIVE_GPIO0_FALL_IP) |= (1 << pin);
        break;
    case METAL_GPIO_INT_BOTH_EDGE:
        GPIO_REGW(METAL_SIFIVE_GPIO0_RISE_IP) |= (1 << pin);
        GPIO_REGW(METAL_SIFIVE_GPIO0_FALL_IP) |= (1 << pin);
        break;
    case METAL_GPIO_INT_HIGH:
        GPIO_REGW(METAL_SIFIVE_GPIO0_HIGH_IP) |= (1 << pin);
        break;
    case METAL_GPIO_INT_LOW:
        GPIO_REGW(METAL_SIFIVE_GPIO0_LOW_IP) |= (1 << pin);
        break;
    case METAL_GPIO_INT_BOTH_LEVEL:
        GPIO_REGW(METAL_SIFIVE_GPIO0_HIGH_IP) |= (1 << pin);
        GPIO_REGW(METAL_SIFIVE_GPIO0_LOW_IP) |= (1 << pin);
        break;
    case METAL_GPIO_INT_MAX:
        GPIO_REGW(METAL_SIFIVE_GPIO0_RISE_IP) |= (1 << pin);
        GPIO_REGW(METAL_SIFIVE_GPIO0_FALL_IP) |= (1 << pin);
        GPIO_REGW(METAL_SIFIVE_GPIO0_HIGH_IP) |= (1 << pin);
        GPIO_REGW(METAL_SIFIVE_GPIO0_LOW_IP) |= (1 << pin);
        break;
    default:
        return -1;
    }
    return 0;
}

#endif /* METAL_SIFIVE_GPIO0 */

typedef int no_empty_translation_units;
