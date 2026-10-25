/* Declarations match abi_targets.c exactly. C99, no LTO required. */
unsigned abi_sum5(unsigned a, unsigned b, unsigned c, unsigned d, unsigned e);
unsigned abi_twice(unsigned x);
float abi_fadd(float x, float y);

unsigned abi_call5(void)
{
    return abi_sum5(1u, 2u, 3u, 4u, 5u);
}

unsigned abi_keep(unsigned x)
{
    unsigned kept = x + 7u;
    unsigned result = abi_twice(x);
    return result + kept;
}

unsigned abi_tail(unsigned x)
{
    return abi_twice(x);
}

unsigned abi_after(unsigned x)
{
    return abi_twice(x) + 1u;
}

unsigned abi_tail_sum(unsigned n, unsigned acc)
{
    if (n == 0u) {
        return acc;
    }
    return abi_tail_sum(n - 1u, acc + n);
}

#ifndef ABI_NO_MAIN
/* Keep standalone self-checks active even in a Release integration. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <stdio.h>

int main(void)
{
    const unsigned inputs[] = {0u, 1u, 5u, 17u, UINT_MAX};
    unsigned i;
    assert(abi_call5() == 15u);
    assert(abi_sum5(UINT_MAX, 1u, 0u, 0u, 0u) == 0u);
    for (i = 0u; i < sizeof inputs / sizeof inputs[0]; ++i) {
        unsigned x = inputs[i];
        assert(abi_keep(x) == 3u * x + 7u);
        assert(abi_tail(x) == 2u * x);
        assert(abi_after(x) == 2u * x + 1u);
    }
    assert(abi_tail_sum(0u, 0u) == 0u);
    assert(abi_tail_sum(3u, 0u) == 6u);
    assert(abi_tail_sum(100u, 0u) == 5050u);
    assert(abi_tail_sum(1u, UINT_MAX) == 0u);
    assert(abi_fadd(1.5f, 2.25f) == 3.75f);
    printf("sum5=%u keep=%u tail=%u after=%u\n",
           abi_call5(), abi_keep(5u), abi_tail(5u), abi_after(5u));
    printf("tail_sum(3)=%u float=%.2f\n",
           abi_tail_sum(3u, 0u), (double)abi_fadd(1.5f, 2.25f));
    return 0;
}
#endif
