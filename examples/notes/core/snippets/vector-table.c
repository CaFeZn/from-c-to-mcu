#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    uint32_t base = UINT32_C(0x08004000);
    uint32_t irqn = 6u;
    uint32_t slot = base + 4u * (irqn + 16u);
    assert(slot == UINT32_C(0x08004058));
    printf("EXTI0 slot=0x%08lX\n", (unsigned long)slot);
    return 0;
}
