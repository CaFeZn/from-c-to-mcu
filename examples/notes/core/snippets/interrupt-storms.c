#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t write_one_to_clear(uint32_t state, uint32_t command)
{
    return state & ~command;
}

int main(void)
{
    uint32_t before = 3u;
    uint32_t good = write_one_to_clear(before, 1u);
    uint32_t bad_or = write_one_to_clear(before, before | 1u);
    uint32_t bad_and = write_one_to_clear(before, before & ~1u);
    assert(good == 2u);
    assert(bad_or == 0u);
    assert(bad_and == 1u);
    printf("direct=%lu, or=%lu, and=%lu\n",
           (unsigned long)good, (unsigned long)bad_or,
           (unsigned long)bad_and);
    return 0;
}
