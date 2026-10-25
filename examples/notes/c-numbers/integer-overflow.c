#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Non-null, writable out; a failed check leaves it unchanged. */
static bool add_i32(int32_t a, int32_t b, int32_t *out) {
    if ((b > 0 && a > INT32_MAX - b) ||
        (b < 0 && a < INT32_MIN - b)) return false;
    *out = a + b;
    return true;
}

static bool sub_i32(int32_t a, int32_t b, int32_t *out) {
    if ((b > 0 && a < INT32_MIN + b) ||
        (b < 0 && a > INT32_MAX + b)) return false;
    *out = a - b;
    return true;
}

static bool size_product(size_t count, size_t each, size_t *out) {
    if (each != 0 && count > SIZE_MAX / each) return false;
    *out = count * each;
    return true;
}

int main(void) {
    const int32_t values[] = {INT32_MIN, INT32_MIN + 1, -1000, -1,
                             0, 1, 1000, INT32_MAX - 1, INT32_MAX};
    int32_t result = 99;
    bool ok = add_i32(INT32_MAX, 1, &result);
    assert(!ok && result == 99);
    printf("ok=%d result=%ld\n", (int)ok, (long)result);
    ok = add_i32(1200, -700, &result);
    assert(ok && result == 500);
    printf("ok=%d result=%ld\n", (int)ok, (long)result);
    for (size_t i = 0; i < sizeof values / sizeof values[0]; ++i) {
        for (size_t j = 0; j < sizeof values / sizeof values[0]; ++j) {
            int64_t sum = (int64_t)values[i] + values[j];
            int64_t difference = (int64_t)values[i] - values[j];
            bool expected = sum >= INT32_MIN && sum <= INT32_MAX;
            result = 99;
            assert(add_i32(values[i], values[j], &result) == expected);
            assert(expected ? (int64_t)result == sum : result == 99);
            expected = difference >= INT32_MIN && difference <= INT32_MAX;
            result = 99;
            assert(sub_i32(values[i], values[j], &result) == expected);
            assert(expected ? (int64_t)result == difference : result == 99);
#ifdef __GNUC__
            int32_t builtin_result;
            bool overflow = __builtin_add_overflow(values[i], values[j],
                                                   &builtin_result);
            assert(overflow == !(sum >= INT32_MIN && sum <= INT32_MAX));
            if (!overflow) assert((int64_t)builtin_result == sum);
#endif
        }
    }
    size_t bytes = 99;
    assert(size_product(10, 24, &bytes) && bytes == 240);
    bytes = 99;
    assert(!size_product(SIZE_MAX, 2, &bytes) && bytes == 99);
    assert(size_product(SIZE_MAX, 0, &bytes) && bytes == 0);
    assert(size_product(SIZE_MAX, 1, &bytes) && bytes == SIZE_MAX);
    assert(size_product(0, SIZE_MAX, &bytes) && bytes == 0);
    assert(size_product(SIZE_MAX / 2, 2, &bytes));
    assert(!size_product(SIZE_MAX / 2 + 1, 2, &bytes));
    puts("overflow boundaries: ok (81 sums, 81 differences, size products)");
    return 0;
}
