#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static int accepts(uint32_t key, uint32_t id, uint32_t mask)
{
    return ((key ^ id) & mask) == 0u;
}

int main(void)
{
    const uint32_t id = UINT32_C(0x100) << 21;
    const uint32_t mask = (UINT32_C(0x7F0) << 21) | 6u;
    const uint32_t standard_data = UINT32_C(0x105) << 21;
    const uint32_t standard_remote = standard_data | 2u;
    const uint32_t extended_data =
        ((UINT32_C(0x105) << 18) << 3) | 4u;

    assert(accepts(standard_data, id, mask));
    assert(!accepts(standard_remote, id, mask));
    assert(!accepts(extended_data, id, mask));
    assert(!accepts(UINT32_C(0x110) << 21, id, mask));
    puts("standard data: pass; remote/extended/outside: drop");
    return 0;
}
