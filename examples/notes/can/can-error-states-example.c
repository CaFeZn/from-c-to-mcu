#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static const char *can_state(uint32_t esr)
{
    if ((esr & 4u) != 0u) return "bus-off";
    if ((esr & 2u) != 0u) return "error-passive";
    return "error-active";
}

int main(void)
{
    const uint32_t esr = UINT32_C(0x00800033);
    const unsigned tec_low = (unsigned)((esr >> 16) & 255u);
    const unsigned rec = (unsigned)((esr >> 24) & 255u);
    const unsigned lec = (unsigned)((esr >> 4) & 7u);
    assert(tec_low == 128u && rec == 0u && lec == 3u);
    printf("TEC-low=%u REC=%u LEC=%u %s\n",
           tec_low, rec, lec, can_state(esr));
    printf("BOFF snapshot: %s\n", can_state(4u));
    return 0;
}
