#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    const uint32_t pclk = 42000000u;
    const uint32_t prescaler = 6u, bs1 = 11u, bs2 = 2u;
    const uint32_t sjw = 1u;
    const uint32_t n = 1u + bs1 + bs2;
    const uint64_t divisor = (uint64_t)prescaler * n;
    const uint32_t btr = ((sjw - 1u) << 24)
                       | ((bs2 - 1u) << 20)
                       | ((bs1 - 1u) << 16)
                       | (prescaler - 1u);

    assert(prescaler >= 1u && prescaler <= 1024u);
    assert(bs1 >= 1u && bs1 <= 16u);
    assert(bs2 >= 1u && bs2 <= 8u);
    assert(sjw >= 1u && sjw <= 4u && sjw <= bs2);
    assert((uint64_t)pclk == 500000u * divisor);
    assert(btr == UINT32_C(0x001A0005));
    printf("rate=%.0f bit/s, sample=%.3f%%\n",
           (double)pclk / (double)divisor,
           100.0 * (double)(1u + bs1) / (double)n);
    printf("BTR timing fields=0x%08lX\n", (unsigned long)btr);
    return 0;
}
