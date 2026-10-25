#include <stdint.h>
#include "register_model.h"

void pa5_high(void)
{
    gpio_layout_t *gpioa = (gpio_layout_t *)(uintptr_t)UINT32_C(0x40020000);
    gpioa->BSRR = UINT32_C(0x20);
}

uint32_t pa5_input(void)
{
    gpio_layout_t *gpioa = (gpio_layout_t *)(uintptr_t)UINT32_C(0x40020000);
    return (gpioa->IDR >> 5) & UINT32_C(1);
}

void write_twice(uint32_t *p)
{
    *p = UINT32_C(1);
    *p = UINT32_C(2);
}

void write_twice_volatile(volatile uint32_t *p)
{
    *p = UINT32_C(1);
    *p = UINT32_C(2);
}

uint32_t triple_u32(uint32_t x)
{
    return x * UINT32_C(3);
}

extern uint32_t external_helper(uint32_t x);

uint32_t call_helper(uint32_t x)
{
    return external_helper(x) + UINT32_C(1);
}
