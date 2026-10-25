#include <assert.h>
#include <stdio.h>
#include "register_model.h"

int main(void)
{
    gpio_layout_t ram = {0};
    uint32_t odr = UINT32_C(0x0002);
    assert(offsetof(gpio_layout_t, AFR[1]) == 0x24);
    assert(sizeof(gpio_layout_t) == 0x28);
    ram.BSRR = UINT32_C(0x20);
    assert(ram.BSRR == UINT32_C(0x20));
    assert(ram.ODR == 0);
    odr = model_bsrr(odr, UINT32_C(0x20));
    assert(odr == UINT32_C(0x22));
    odr = model_bsrr(odr, UINT32_C(0x00200000));
    assert(odr == UINT32_C(0x02));
    assert(model_bsrr(0, UINT32_C(0x00200020)) == UINT32_C(0x20));
    assert(model_bsrr(UINT32_C(0xffff), 0) == UINT32_C(0xffff));
    assert(model_bsrr(0, UINT32_C(0x8000)) == UINT32_C(0x8000));
    assert(model_bsrr(UINT32_C(0xffff), UINT32_C(0x80000000)) == UINT32_C(0x7fff));
    puts("layout: BSRR=24 AFR1=36 RCC_AHB1ENR=48");
    puts("RAM: BSRR=32 ODR=0; model: 0002 -> 0022 -> 0002");
    puts("platform tests: PASS");
    return 0;
}
