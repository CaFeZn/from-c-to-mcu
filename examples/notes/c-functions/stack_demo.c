/* C99. Small bounded inputs only; no escaped automatic-object pointers. */
unsigned stack_leaf(unsigned x)
{
    unsigned local = x + 1u;
    return local;
}

unsigned stack_step(unsigned depth)
{
    unsigned local = 10u + depth;
    if (depth == 0u) {
        return local;
    }
    {
        unsigned child = stack_step(depth - 1u);
        return local + child;
    }
}

#ifndef STACK_NO_MAIN
/* Keep standalone self-checks active even in a Release integration. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <stdio.h>

int main(void)
{
    unsigned depth;
    for (depth = 0u; depth <= 32u; ++depth) {
        unsigned i;
        unsigned expected = 0u;
        for (i = 0u; i <= depth; ++i) {
            expected += 10u + i;
        }
        assert(stack_step(depth) == expected);
    }
    assert(stack_leaf(41u) == 42u);
    assert(stack_leaf(UINT_MAX) == 0u);
    printf("stack_step(0) = %u\n", stack_step(0u));
    printf("stack_step(1) = %u\n", stack_step(1u));
    printf("stack_step(3) = %u\n", stack_step(3u));
    printf("stack_leaf(41) = %u\n", stack_leaf(41u));
    return 0;
}
#endif
