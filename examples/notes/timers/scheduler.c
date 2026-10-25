#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t take_due(uint32_t now, uint32_t *phase,
                         uint32_t period)
{
    assert(period > 0u && period <= INT32_MAX);
    uint32_t elapsed = (uint32_t)(now - *phase);
    uint32_t n = elapsed / period;
    if (n != 0u) {
        *phase += elapsed - elapsed % period;
    }
    return n;
}

int main(void)
{
    uint32_t phase = 0u;
    assert(take_due(99u, &phase, 100u) == 0u);
    assert(take_due(350u, &phase, 100u) == 3u);
    assert(phase == 300u);
    assert(take_due(399u, &phase, 100u) == 0u);
    assert(take_due(400u, &phase, 100u) == 1u);

    phase = UINT32_MAX - 15u;
    assert(take_due(4u, &phase, 10u) == 2u);
    assert(phase == 4u);
    puts("scheduler: late=3, phase=300; wrap=2");
    return 0;
}
