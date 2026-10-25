#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t magnitude_i32(int32_t value) {
    return value < 0 ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
}

static void print_milli(int32_t value) {
    uint32_t magnitude = magnitude_i32(value);
    printf("%s%lu.%03lu\n", value < 0 ? "-" : "",
           (unsigned long)(magnitude / 1000u),
           (unsigned long)(magnitude % 1000u));
}

static bool scale_milli(int32_t x_milli, int32_t gain_milli, int32_t *out) {
    int64_t product = (int64_t)x_milli * gain_milli;
    int64_t scaled = product / 1000;
    if (scaled < INT32_MIN || scaled > INT32_MAX) return false;
    *out = (int32_t)scaled;
    return true;
}

static bool scale_milli_nearest(int32_t x, int32_t gain, int32_t *out) {
    int64_t product = (int64_t)x * gain;
    product += product < 0 ? -500 : 500;
    int64_t scaled = product / 1000;
    if (scaled < INT32_MIN || scaled > INT32_MAX) return false;
    *out = (int32_t)scaled;
    return true;
}

/* Application range is [-1000,1000] degrees; truncate toward zero. */
static bool temperature_to_milli(float value, int32_t *out) {
    if (!isfinite(value) || value < -1000.0f || value > 1000.0f) return false;
    *out = (int32_t)(value * 1000.0f);
    return true;
}

int main(void) {
    uint16_t raw = 2048u;
    uint32_t ref = 3300u;
    uint32_t mv = (uint32_t)(((uint64_t)raw * ref + 2047u) / 4095u);
    assert(mv == 1650u);
    print_milli(-125);
    printf("adc=%lu mV\n", (unsigned long)mv);
    assert(magnitude_i32(INT32_MIN) == UINT32_C(2147483648));
    assert(magnitude_i32(INT32_MAX) == (uint32_t)INT32_MAX);
    assert(magnitude_i32(0) == 0u);
    char buffer[32];
    uint32_t magnitude = magnitude_i32(INT32_MIN);
    int n = snprintf(buffer, sizeof buffer, "-%lu.%03lu",
                     (unsigned long)(magnitude / 1000u),
                     (unsigned long)(magnitude % 1000u));
    assert(n == 12 && strcmp(buffer, "-2147483.648") == 0);
    int32_t result = 99;
    assert(scale_milli(-12345, 1500, &result) && result == -18517);
    assert(scale_milli_nearest(-12345, 1500, &result) && result == -18518);
    assert(scale_milli(-1, 500, &result) && result == 0);
    assert(scale_milli_nearest(-1, 500, &result) && result == -1);
    assert(scale_milli_nearest(1, 500, &result) && result == 1);
    assert(scale_milli(INT32_MIN, 1000, &result) && result == INT32_MIN);
    assert(scale_milli(INT32_MAX, 1000, &result) && result == INT32_MAX);
    result = 99;
    assert(!scale_milli(INT32_MIN, INT32_MIN, &result) && result == 99);
    assert(!scale_milli_nearest(INT32_MAX, INT32_MAX, &result));
    assert(temperature_to_milli(-0.125f, &result) && result == -125);
    assert(temperature_to_milli(1000.0f, &result) && result == 1000000);
    assert(!temperature_to_milli(1001.0f, &result));
#ifdef NAN
    assert(!temperature_to_milli(NAN, &result));
#endif
#ifdef INFINITY
    assert(!temperature_to_milli(INFINITY, &result));
#endif
    uint32_t previous = 0;
    for (uint32_t sample = 0; sample <= 4095u; ++sample) {
        uint32_t current = (uint32_t)(((uint64_t)sample * ref + 2047u) / 4095u);
        assert(current >= previous && current <= ref);
        previous = current;
    }
    assert(previous == ref);
    puts("fixed-point boundaries: ok (4096 ADC codes, signed endpoints, rounding)");
    return 0;
}
