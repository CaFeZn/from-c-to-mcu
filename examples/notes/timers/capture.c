#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static int period_hz(uint32_t previous, uint32_t current,
                     uint32_t counter_hz, double *hz)
{
    uint32_t ticks = (uint32_t)(current - previous);
    if (ticks == 0u || counter_hz == 0u || hz == NULL) {
        return 0;
    }
    *hz = (double)counter_hz / (double)ticks;
    return 1;
}

int main(void)
{
    double hz = 0.0;
    assert(period_hz(100u, 1100u, 1000000u, &hz));
    assert(hz == 1000.0);
    assert(period_hz(UINT32_MAX - 499u, 500u,
                     1000000u, &hz));
    assert(hz == 1000.0);
    assert(!period_hz(100u, 100u, 1000000u, &hz));
    puts("capture: normal=1000 Hz, wrap=1000 Hz");
    return 0;
}
