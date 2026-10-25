#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

struct toy_rng {
    unsigned state;
};

/* A 4-bit teaching generator, not a production or security PRNG. */
static unsigned toy_next(struct toy_rng *rng)
{
    assert(rng != NULL);
    assert(rng->state < 16u);
    rng->state = (5u * rng->state + 1u) % 16u;
    return rng->state;
}

/* On failure, leave *value unchanged and report consumed draws. */
static bool bounded16(struct toy_rng *rng, unsigned n,
                      unsigned budget, unsigned *value, unsigned *draws)
{
    unsigned limit;

    assert(rng != NULL);
    assert(value != NULL);
    assert(draws != NULL);
    *draws = 0u;
    if (n == 0u || n > 16u) {
        return false;
    }
    limit = 16u - 16u % n;
    while (*draws < budget) {
        unsigned x = toy_next(rng);
        ++*draws;
        if (x < limit) {
            *value = x % n;
            return true;
        }
    }
    return false;
}

static void check_libc_replay(void)
{
    int first[8];
    int implicit[8];
    size_t i;

    /* This function must precede any other rand/srand call in main. */
    for (i = 0u; i < 8u; ++i) {
        implicit[i] = rand();
        assert(implicit[i] >= 0 && implicit[i] <= RAND_MAX);
    }
    srand(1u);
    for (i = 0u; i < 8u; ++i) {
        int actual = rand();
        assert(actual == implicit[i]);
    }
    srand(123u);
    for (i = 0u; i < 8u; ++i) {
        first[i] = rand();
    }
    srand(123u);
    for (i = 0u; i < 8u; ++i) {
        int actual = rand();
        assert(actual == first[i]);
    }
    puts("rand replay: ok");
}

static void check_state(void)
{
    static const unsigned expected[16] = {
        6u, 15u, 12u, 13u, 2u, 11u, 8u, 9u,
        14u, 7u, 4u, 5u, 10u, 3u, 0u, 1u
    };
    struct toy_rng rng = { 1u };
    struct toy_rng copy;
    struct toy_rng replay = { 1u };
    unsigned a;
    unsigned b;
    size_t i;

    printf("seed=1:");
    for (i = 0u; i < 8u; ++i) {
        unsigned x = toy_next(&rng);
        assert(x == expected[i]);
        printf(" %u", x);
    }
    putchar('\n');
    copy = rng;
    a = toy_next(&rng);
    b = toy_next(&copy);
    assert(a == 14u && b == 14u);
    printf("clone next: %u %u\n", a, b);
    for (i = 0u; i < 16u; ++i) {
        unsigned x = toy_next(&replay);
        assert(x == expected[i]);
    }
    assert(replay.state == 1u);
}

static void check_counts(void)
{
    unsigned modulo[6] = { 0u };
    unsigned rejected[6] = { 0u };
    unsigned value;
    unsigned i;

    for (value = 0u; value < 16u; ++value) {
        ++modulo[value % 6u];
        if (value < 12u) {
            ++rejected[value % 6u];
        }
    }
    printf("mod counts:");
    for (i = 0u; i < 6u; ++i) {
        assert(modulo[i] == (i < 4u ? 3u : 2u));
        printf(" %u", modulo[i]);
    }
    printf("\nreject counts:");
    for (i = 0u; i < 6u; ++i) {
        assert(rejected[i] == 2u);
        printf(" %u", rejected[i]);
    }
    putchar('\n');
}

static void check_rejection(void)
{
    struct toy_rng rng = { 1u };
    unsigned first = 99u;
    unsigned second = 99u;
    unsigned d1;
    unsigned d2;
    unsigned counts[6] = { 0u };
    unsigned total_draws = 0u;
    unsigned i;
    bool ok;

    ok = bounded16(&rng, 6u, 16u, &first, &d1);
    assert(ok && first == 0u && d1 == 1u);
    ok = bounded16(&rng, 6u, 16u, &second, &d2);
    assert(ok && second == 2u && d2 == 4u);
    printf("bounded n=6: %u (draws=%u), %u (draws=%u)\n",
           first, d1, second, d2);

    rng.state = 1u;
    for (i = 0u; i < 12u; ++i) {
        unsigned value;
        unsigned draws;
        ok = bounded16(&rng, 6u, 16u, &value, &draws);
        assert(ok && value < 6u);
        ++counts[value];
        total_draws += draws;
    }
    assert(total_draws == 16u && rng.state == 1u);
    for (i = 0u; i < 6u; ++i) {
        assert(counts[i] == 2u);
    }

    /* Exhaustively check accepted preimage counts for every legal bound. */
    for (i = 1u; i <= 16u; ++i) {
        unsigned per_value[16] = { 0u };
        unsigned limit = 16u - 16u % i;
        unsigned x;
        for (x = 0u; x < limit; ++x) {
            ++per_value[x % i];
        }
        for (x = 0u; x < i; ++x) {
            assert(per_value[x] == 16u / i);
        }
    }
}

static void check_boundaries(void)
{
    struct toy_rng rng = { 1u };
    unsigned value = 99u;
    unsigned draws = 99u;
    bool ok;

    ok = bounded16(&rng, 0u, 16u, &value, &draws);
    assert(!ok && value == 99u && draws == 0u && rng.state == 1u);
    ok = bounded16(&rng, 17u, 16u, &value, &draws);
    assert(!ok && value == 99u && draws == 0u && rng.state == 1u);
    ok = bounded16(&rng, 6u, 0u, &value, &draws);
    assert(!ok && value == 99u && draws == 0u && rng.state == 1u);

    rng.state = 6u;
    ok = bounded16(&rng, 6u, 3u, &value, &draws);
    assert(!ok && value == 99u && draws == 3u && rng.state == 13u);
    ok = bounded16(&rng, 6u, 1u, &value, &draws);
    assert(ok && value == 2u && draws == 1u);

    rng.state = 1u;
    ok = bounded16(&rng, 1u, 1u, &value, &draws);
    assert(ok && value == 0u && draws == 1u);
    rng.state = 1u;
    ok = bounded16(&rng, 16u, 1u, &value, &draws);
    assert(ok && value == 6u && draws == 1u);
    rng.state = 6u;
    value = 99u;
    ok = bounded16(&rng, 5u, 1u, &value, &draws);
    assert(!ok && value == 99u && draws == 1u && rng.state == 15u);
    puts("boundaries: ok");
}

int main(void)
{
    check_libc_replay();
    check_state();
    check_counts();
    check_rejection();
    check_boundaries();
    return 0;
}
