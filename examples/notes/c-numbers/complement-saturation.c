/* Portable C99 tests for the two complement/saturation branch articles.
 * Exact-width uint8_t, int16_t and int32_t are required by this example.
 * No signed overflow or out-of-range signed conversion is evaluated.
 * Host-only: no CMSIS dependency, Cortex-M4 instruction or board access.
 */
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#if !defined(UINT8_MAX) || !defined(INT16_MAX) || !defined(INT32_MAX)
#error "This example requires uint8_t, int16_t and int32_t"
#endif

static unsigned add8(unsigned a, unsigned b)
{
    assert(a <= 255u && b <= 255u);
    return (a + b) & 255u;
}

static unsigned neg8(unsigned bits)
{
    assert(bits <= 255u);
    return (~bits + 1u) & 255u;
}

static int decode8(unsigned bits)
{
    assert(bits <= 255u);
    return bits < 128u ? (int)bits : (int)bits - 256;
}

static uint8_t sat_u8_add(uint8_t a, uint8_t b)
{
    unsigned sum = (unsigned)a + (unsigned)b;
    return sum > UINT8_MAX ? UINT8_MAX : (uint8_t)sum;
}

static int16_t sat_i16_add(int16_t a, int16_t b)
{
    int_least32_t sum = (int_least32_t)a + (int_least32_t)b;
    if (sum > INT16_MAX) return INT16_MAX;
    if (sum < INT16_MIN) return INT16_MIN;
    return (int16_t)sum;
}

static int32_t sat_i32_add(int32_t a, int32_t b)
{
    int_least64_t sum = (int_least64_t)a + (int_least64_t)b;
    if (sum > INT32_MAX) return INT32_MAX;
    if (sum < INT32_MIN) return INT32_MIN;
    return (int32_t)sum;
}

int main(void)
{
    unsigned long add_cases = 0;
    unsigned long u8_cases = 0;
    unsigned long i16_cases = 0;
    const int16_t deltas[] = { INT16_MIN, -1, 0, 1, INT16_MAX };
    const unsigned delta_count =
        (unsigned)(sizeof deltas / sizeof deltas[0]);
    unsigned a, b, j;
    long x;

    assert(decode8(0u) == 0);
    assert(decode8(127u) == 127);
    assert(decode8(128u) == -128);
    assert(decode8(255u) == -1);
    assert(neg8(0u) == 0u);
    assert(neg8(5u) == 251u);
    assert(neg8(128u) == 128u);
    assert(add8(5u, neg8(5u)) == 0u);
    assert(add8(255u, 1u) == 0u);
    assert(decode8(add8(127u, 1u)) == -128);

    for (a = 0; a < 256u; ++a) {
        assert(add8(a, neg8(a)) == 0u);
        for (b = 0; b < 256u; ++b) {
            int mathematical = decode8(a) + decode8(b);
            int decoded = decode8(add8(a, b));
            unsigned expected = a + b;
            assert((decoded - mathematical) % 256 == 0);
            assert(add8(a, b) == (a + b) % 256u);
            ++add_cases;
            if (expected > 255u) expected = 255u;
            assert((unsigned)sat_u8_add((uint8_t)a, (uint8_t)b)
                   == expected);
            ++u8_cases;
        }
    }

    /* long is guaranteed to hold [-65536, 65534], even with 16-bit int. */
    for (x = -32768L; x <= 32767L; ++x) {
        for (j = 0; j < delta_count; ++j) {
            long expected = x + (long)deltas[j];
            if (expected > 32767L) expected = 32767L;
            if (expected < -32768L) expected = -32768L;
            assert((long)sat_i16_add((int16_t)x, deltas[j]) == expected);
            ++i16_cases;
        }
    }

    assert(sat_u8_add(250, 20) == 255);
    assert(sat_u8_add(255, 0) == 255);
    assert(sat_i16_add(30000, 10000) == INT16_MAX);
    assert(sat_i16_add(-30000, -10000) == INT16_MIN);
    assert(sat_i16_add(INT16_MIN, INT16_MAX) == -1);
    assert(sat_i16_add(sat_i16_add(30000, 10000), -10000) == 22767);
    assert(sat_i16_add(30000, sat_i16_add(10000, -10000)) == 30000);

    assert(sat_i32_add(INT32_MAX, 1) == INT32_MAX);
    assert(sat_i32_add(INT32_MIN, -1) == INT32_MIN);
    assert(sat_i32_add(INT32_MAX, INT32_MAX) == INT32_MAX);
    assert(sat_i32_add(INT32_MIN, INT32_MIN) == INT32_MIN);
    assert(sat_i32_add(INT32_MIN, INT32_MAX) == -1);
    assert(sat_i32_add(INT32_MIN, 1) == INT32_MIN + 1);
    assert(sat_i32_add(INT32_MAX, -1) == INT32_MAX - 1);
    assert(sat_i32_add(123, -123) == 0);

    printf("8-bit: 5 + (-5) -> bits=%02X value=%d\n",
           add8(5u, neg8(5u)), decode8(add8(5u, neg8(5u))));
    printf("8-bit: 127 + 1 -> bits=%02X value=%d (model)\n",
           add8(127u, 1u), decode8(add8(127u, 1u)));
    printf("u8: 250 + 20 -> wrap=%u sat=%u\n",
           (unsigned)(uint8_t)(250u + 20u),
           (unsigned)sat_u8_add(250, 20));
    printf("i16: 30000 + 10000 -> sat=%ld\n",
           (long)sat_i16_add(30000, 10000));
    printf("i16: -30000 + (-10000) -> sat=%ld\n",
           (long)sat_i16_add(-30000, -10000));
    printf("i32: INT32_MAX + 1 -> sat=%" PRId32 "\n",
           sat_i32_add(INT32_MAX, 1));
    printf("checks: add8=%lu sat_u8=%lu sat_i16=%lu\n",
           add_cases, u8_cases, i16_cases);
    puts("complement/saturation: OK");
    return 0;
}
