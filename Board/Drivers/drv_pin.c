//
// Created by An on 2025/12/14.
//

#include "drv_pin.h"
#include "rtthread.h"


static inline void _clock_init(pin_t pin)
{
    if (pin < 0) {
        return; // Invalid pin
    }
    uint32_t gpio_x = (uint32_t)GET_PORT(pin);
    switch (gpio_x) {
        case (uint32_t)GPIOA_BASE:
            crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
            break;
        case (uint32_t)GPIOB_BASE:
            crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
            break;
        case (uint32_t)GPIOC_BASE:
            crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
            break;
        case (uint32_t)GPIOD_BASE:
            crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
            break;
        case (uint32_t)GPIOE_BASE:
            crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);
            break;
        default:
            break;
    }
}

int pin_init(pin_t     pin,
            pin_mode_t mode,
            pin_pull_t pull)
{
    gpio_init_type gpio_init_struct;
    if (pin < 0) {
        return -1;
    }
    _clock_init(pin);
    switch (mode) {
        case PIN_MODE_OPP:
        case PIN_MODE_OOD: {
            gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
        } break;
        case PIN_MODE_INPUT: {
            gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
        } break;
        case PIN_MODE_ANALOG: {
            gpio_init_struct.gpio_mode = GPIO_MODE_ANALOG;
        } break;
        case PIN_MODE_AF:
        case PIN_MODE_AF_OD: {
            gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
        } break;
    }
    gpio_init_struct.gpio_pins           = (1 << GET_PIN_INDEX(pin));
    gpio_init_struct.gpio_out_type       = (gpio_output_type)mode;
    gpio_init_struct.gpio_pull           = (gpio_pull_type)pull;
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(GET_PORT(pin), &gpio_init_struct);
    return 0;
}

int pin_af_init(pin_t     pin,
               pin_pull_t pull,
               pin_mode_t mode,
               int8_t    af_mode)
{
    (void)af_mode;
    gpio_init_type        gpio_init_struct;
    gpio_pins_source_type gpio_pin_source;
    if (pin < 0) {
        return RT_EINVAL; // Invalid pin
    }
    _clock_init(pin);
    gpio_init_struct.gpio_mode           = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins           = (1 << GET_PIN_INDEX(pin));
    gpio_init_struct.gpio_out_type       = mode == PIN_MODE_AF_OD ? GPIO_OUTPUT_OPEN_DRAIN : GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_pull           = (gpio_pull_type)pull;
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;

    gpio_init(GET_PORT(pin), &gpio_init_struct);
    return 0;
}

int pin_write(pin_t pin, uint8_t val)
{
    if (pin < 0) {
        return -1;
    }
    if (val) {
        GET_PORT(pin)->scr = (1 << GET_PIN_INDEX(pin));
    } else {
        GET_PORT(pin)->clr = (1 << GET_PIN_INDEX(pin));
    }
    return 0;
}

int8_t pin_read(pin_t pin)
{
    if (pin < 0) {
        return RT_EINVAL; // Invalid pin
    }
    return (GET_PORT(pin)->idt & (1 << GET_PIN_INDEX(pin))) ? 1 : 0;
}

int8_t pin_read_output(pin_t pin)
{
    if (pin < 0) {
        return RT_EINVAL; // Invalid pin
    }
    return (GET_PORT(pin)->odt & (1 << GET_PIN_INDEX(pin))) ? 1 : 0;
}

int8_t pin_toggle(pin_t pin)
{
    if (pin < 0) {
        return RT_EINVAL; // Invalid pin
    }
    GET_PORT(pin)->odt ^= (1 << GET_PIN_INDEX(pin));
    return pin_read_output(pin);
}

void pin_deinit(pin_t pin)
{
    if (pin < 0) {
        return; // Invalid pin
    }
    gpio_init_type gpio_init_struct;
    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_pins = (1 << GET_PIN_INDEX(pin));
    gpio_init(GET_PORT(pin), &gpio_init_struct);
}