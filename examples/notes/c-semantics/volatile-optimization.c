#include <assert.h>
#include <stdint.h>
#include <stdio.h>

uint32_t sum_plain(const uint32_t *p)
{
    uint32_t total = 0;
    for (unsigned i = 0; i < 4; ++i) {
        total += *p;
    }
    return total;
}

uint32_t sum_volatile(const volatile uint32_t *p)
{
    uint32_t total = 0;
    for (unsigned i = 0; i < 4; ++i) {
        total += *p;
    }
    return total;
}

void write_plain(uint32_t *p)
{
    *p = 1;
    *p = 2;
}

void write_volatile(volatile uint32_t *p)
{
    *p = 1;
    *p = 2;
}

int main(void)
{
    uint32_t ordinary = 7;
    volatile uint32_t observed = 7;
    assert(sum_plain(&ordinary) == 28);
    assert(sum_volatile(&observed) == 28);
    write_plain(&ordinary);
    write_volatile(&observed);
    assert(ordinary == 2 && observed == 2);

    /* A sequential model of an interrupted read-modify-write. */
    volatile uint32_t counter = 0;
    uint32_t saved = counter;
    counter = counter + 1;
    counter = saved + 1;
    assert(counter == 1);
    puts("sums=28,28 writes=2,2 interleaved=1");
    return 0;
}
