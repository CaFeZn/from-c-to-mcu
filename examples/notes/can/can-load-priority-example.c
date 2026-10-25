#include <assert.h>
#include <stdio.h>

static unsigned frame_bound(unsigned d, int extended)
{
    const unsigned extra = extended ? 20u : 0u;
    assert(d <= 8u);
    return 47u + extra + 8u * d
         + (34u + extra + 8u * d - 1u) / 4u;
}

int main(void)
{
    for (unsigned d = 0u; d <= 8u; d += 4u) {
        printf("d=%u std=%u ext=%u\n", d,
               frame_bound(d, 0), frame_bound(d, 1));
    }
    assert(frame_bound(8u, 0) == 135u);
    assert(frame_bound(8u, 1) == 160u);
    return 0;
}
