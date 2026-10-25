#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t sum_to(uint32_t n)
{
    uint32_t sum = 0u;
    uint32_t i;
    /* The caller's small bound keeps the loop counter from wrapping. */
    assert(n <= 1000u);
    for (i = 1u; i <= n; ++i) sum += i;
    return sum;
}

int main(void)
{
    uint32_t result = sum_to(10u);
    assert(result == 55u);
    printf("sum=%lu\n", (unsigned long)result);
    return 0;
}
