#ifndef PLATFORM_REGISTER_MODEL_H
#define PLATFORM_REGISTER_MODEL_H

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

/* Teaching layout only; firmware should include its vendor device header. */
typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
} gpio_layout_t;

typedef struct {
    volatile uint32_t CR, PLLCFGR, CFGR, CIR;
    volatile uint32_t AHB1RSTR, AHB2RSTR, AHB3RSTR;
    uint32_t RESERVED0;
    volatile uint32_t APB1RSTR, APB2RSTR;
    uint32_t RESERVED1[2];
    volatile uint32_t AHB1ENR;
} rcc_prefix_t;

typedef char check_byte_width[(CHAR_BIT == 8) ? 1 : -1];
typedef char check_bsrr_offset[(offsetof(gpio_layout_t, BSRR) == 0x18) ? 1 : -1];
typedef char check_rcc_offset[(offsetof(rcc_prefix_t, AHB1ENR) == 0x30) ? 1 : -1];

static inline uint32_t model_bsrr(uint32_t odr, uint32_t request)
{
    uint32_t set_bits = request & UINT32_C(0xffff);
    uint32_t reset_bits = (request >> 16) & UINT32_C(0xffff);
    return ((odr & ~reset_bits) | set_bits) & UINT32_C(0xffff);
}

#endif
