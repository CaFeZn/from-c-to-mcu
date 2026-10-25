#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

static int decode_twos8(uint8_t bits) {
    return bits < 128u ? (int)bits : (int)bits - 256;
}

int main(void) {
    uint8_t a = 200u, b = 100u;
    int sum = a + b;
    uint8_t small = (uint8_t)sum;
    uint8_t minus_one = (uint8_t)-1;
    uint16_t width = 60000u, height = 60000u;
    uint32_t area = (uint32_t)width * height;
    uint32_t start = UINT32_MAX - 3u, now = 6u;
    uint32_t elapsed = (uint32_t)(now - start);
    int neg = -1;
    unsigned limit = 1u;
    assert(sum == 300 && small == 44u);
    assert(minus_one == 255u);
    assert(decode_twos8(0xFEu) == -2);
    assert(area == UINT32_C(3600000000));
    assert(elapsed == 10u);
    assert((unsigned)neg == UINT_MAX);
    assert(!((unsigned)neg < limit));
    assert(neg < 0 || (unsigned)neg < limit);
    for (unsigned bits = 0; bits <= 255u; ++bits) {
        int signed_value = decode_twos8((uint8_t)bits);
        assert(signed_value >= -128 && signed_value <= 127);
        assert((uint8_t)signed_value == bits);
    }
    for (int n = -1024; n <= 1024; ++n) {
        int remainder = n % 256;
        if (remainder < 0) remainder += 256;
        assert((uint8_t)n == (unsigned)remainder);
    }
    assert((uint8_t)-129 == 127u);
    printf("sum=%d stored=%u\n", sum, (unsigned)small);
    printf("minus_one=%u decoded=%d\n",
           (unsigned)minus_one, decode_twos8(0xFEu));
    printf("area=%lu elapsed=%lu\n", (unsigned long)area,
           (unsigned long)elapsed);
    puts("conversion boundaries: ok (256 decodes, 2049 reductions)");
    return 0;
}
