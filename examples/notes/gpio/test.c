/* C99 host models for the seven GPIO notes. No MMIO or hardware access. */
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef NDEBUG
#error "Run this example with assertions enabled."
#endif

typedef enum {
    KEY_NONE = 0, KEY_PRESS = 1, KEY_RELEASE = 2
} key_event;

typedef struct {
    uint8_t stable_down;
    uint8_t candidate_down;
    uint32_t since_ms;
} debounce;

static key_event debounce_step(debounce *d, uint8_t down,
                               uint32_t now_ms)
{
    down = down != 0u;
    if (down != d->candidate_down) {
        d->candidate_down = down;
        d->since_ms = now_ms;
    }
    if (d->candidate_down != d->stable_down &&
        (uint32_t)(now_ms - d->since_ms) >= UINT32_C(20)) {
        d->stable_down = d->candidate_down;
        return d->stable_down ? KEY_PRESS : KEY_RELEASE;
    }
    return KEY_NONE;
}

static uint32_t field4(uint32_t original, unsigned pin,
                       unsigned af)
{
    unsigned shift;
    assert(pin < 16u && af < 16u);
    shift = 4u * (pin % 8u);
    return (original & ~(UINT32_C(15) << shift))
         | ((uint32_t)af << shift);
}

static uint16_t bsrr_model(uint16_t odr, uint32_t command)
{
    uint32_t set = command & UINT32_C(0xffff);
    uint32_t reset = command >> 16;
    return (uint16_t)(((uint32_t)odr & ~reset) | set);
}

static void near_value(double actual, double expected, double tolerance)
{
    assert(fabs(actual - expected) <= tolerance);
}

static void test_gpio_and_af(void)
{
    uint16_t odr = UINT16_C(0x0080);
    unsigned pin, af;
    odr = bsrr_model(odr, UINT32_C(1) << 5);
    assert(odr == UINT16_C(0x00a0));
    odr = bsrr_model(odr, UINT32_C(1) << 21);
    assert(odr == UINT16_C(0x0080));
    assert(bsrr_model(odr, 0u) == odr);
    /* RM0090: set wins if both halves request the same bit. */
    assert(bsrr_model(0u, UINT32_C(0x00200020)) == UINT16_C(0x0020));
    assert(field4(UINT32_C(0xaaaaaaaa), 9u, 7u) == UINT32_C(0xaaaaaa7a));
    assert(field4(0u, 6u, 7u) == UINT32_C(0x07000000));
    assert((UINT32_C(2) << 18) == UINT32_C(0x00080000));
    assert((UINT32_C(2) << 12) == UINT32_C(0x00002000));
    for (pin = 0u; pin < 16u; ++pin) {
        uint32_t mask = UINT32_C(15) << (4u * (pin % 8u));
        for (af = 0u; af < 16u; ++af) {
            uint32_t original = UINT32_C(0xa5a5a5a5);
            uint32_t updated = field4(original, pin, af);
            assert((updated & ~mask) == (original & ~mask));
            assert(((updated & mask) >> (4u * (pin % 8u))) == af);
        }
    }
    near_value(3.3 * 1000.0 / 11000.0, 0.3, 1e-12);
    near_value(10000.0 * 100e-12, 1e-6, 1e-15);
    puts("GPIO/AF: BSRR semantics, 256 field cases, divider and RC OK");
}

static uint8_t sample_down(uint32_t t)
{
    if (t < 1u) return 0u;
    if (t < 3u) return 1u;
    if (t < 6u) return 0u;
    if (t < 10u) return 1u;
    if (t < 12u) return 0u;
    if (t < 100u) return 1u;
    if (t < 103u) return 0u;
    if (t < 107u) return 1u;
    if (t < 140u) return 0u;
    return 1u;
}

static void test_debounce(void)
{
    debounce d = {0u, 0u, 0u};
    uint32_t t;
    unsigned presses = 0u, releases = 0u;
    for (t = 0u; t <= 200u; ++t) {
        key_event e = debounce_step(&d, sample_down(t), t);
        if (e == KEY_PRESS) {
            assert(t == 32u || t == 160u);
            ++presses;
        } else if (e == KEY_RELEASE) {
            assert(t == 127u);
            ++releases;
        }
    }
    assert(presses == 2u && releases == 1u);
    {
        debounce a = {0u, 0u, 0u}, b = {0u, 0u, 0u};
        assert(debounce_step(&a, 1u, 0u) == KEY_NONE);
        assert(debounce_step(&b, 0u, 0u) == KEY_NONE);
        assert(debounce_step(&a, 1u, 20u) == KEY_PRESS);
        assert(debounce_step(&b, 0u, 20u) == KEY_NONE);
        /* A release shorter than the window never rearms the key. */
        assert(debounce_step(&a, 0u, 100u) == KEY_NONE);
        assert(debounce_step(&a, 1u, 110u) == KEY_NONE);
        assert(debounce_step(&a, 1u, 150u) == KEY_NONE);
    }
    {
        debounce wrap = {0u, 0u, 0u};
        assert(debounce_step(&wrap, 1u, UINT32_C(0xfffffff0)) == KEY_NONE);
        assert(debounce_step(&wrap, 1u, 3u) == KEY_NONE);
        assert(debounce_step(&wrap, 1u, 4u) == KEY_PRESS);
        assert(debounce_step(&wrap, 1u, 5u) == KEY_NONE);
    }
    {
        debounce boot = {0u, 0u, 0u};
        /* Policy: a held-at-boot key becomes a press after confirmation. */
        assert(debounce_step(&boot, 1u, 4000u) == KEY_NONE);
        assert(debounce_step(&boot, 1u, 4020u) == KEY_PRESS);
        /* A blind interval cannot reveal hidden pulses: documented limit. */
        debounce blind = {0u, 0u, 0u};
        assert(debounce_step(&blind, 0u, 0u) == KEY_NONE);
        assert(debounce_step(&blind, 0u, 100u) == KEY_NONE);
    }
    near_value(-log(1.0 - 0.7), 1.2039728043, 1e-10);
    puts("Debounce: press 32/160, release 127, hold, boot, wrap and isolation OK");
}

static void test_electrical(void)
{
    double high_min = 4.75 * 17820.0 / (12120.0 + 17820.0);
    double high_max = 5.25 * 18180.0 / (11880.0 + 18180.0);
    double led_nominal = (3.3 - 2.0) / 330.0;
    double led_max = (3.6 - 1.8) / (330.0 * 0.95);
    double resistor_power = (3.6 - 1.8) * (3.6 - 1.8) / (330.0 * 0.95);
    near_value(5.0 * 18000.0 / 30000.0, 3.0, 1e-12);
    assert(high_min > 2.826 && high_min < 2.828);
    assert(high_max > 3.174 && high_max < 3.176);
    assert(high_min > 0.7 * 3.3 && high_max < 3.3);
    near_value((12000.0 * 18000.0 / 30000.0) * 20e-12, 144e-9, 1e-15);
    assert(led_nominal > 0.00393 && led_nominal < 0.00395);
    assert(led_max > 0.00573 && led_max < 0.00575);
    assert(resistor_power > 0.01033 && resistor_power < 0.01035);
    near_value(3.3 - 0.4, 2.9, 1e-12);
    assert(3.3 - 1.3 < 0.7 * 3.3);
    printf("Electrical: divider %.3f..%.3f V; LED %.2f mA max, resistor %.2f mW\n",
           high_min, high_max, 1000.0 * led_max, 1000.0 * resistor_power);
}

static uint32_t pwm_high_counts(uint16_t arr, uint16_t ccr)
{
    uint32_t count = 0u, cnt;
    for (cnt = 0u; cnt <= (uint32_t)arr; ++cnt) {
        if (cnt < (uint32_t)ccr) ++count;
    }
    return count;
}

static int gate_on(uint32_t start, uint32_t duration, uint32_t now)
{
    return (uint32_t)(now - start) < duration;
}

static void test_buzzer_pwm(void)
{
    assert(pwm_high_counts(499u, 250u) == 250u);
    assert(pwm_high_counts(499u, 0u) == 0u);
    assert(pwm_high_counts(999u, 500u) == 500u);
    assert(pwm_high_counts(249u, 500u) == 250u);
    assert(pwm_high_counts(249u, 125u) == 125u);
    assert(pwm_high_counts(999u, 250u) == 250u);
    near_value(1000000.0 / 500.0, 2000.0, 1e-12);
    near_value(1000000.0 / 2272.0, 440.1408450704, 1e-9);
    assert(gate_on(UINT32_C(0xfffffff0), 80u, 63u));
    assert(!gate_on(UINT32_C(0xfffffff0), 80u, 64u));
    assert(!gate_on(12u, 0u, 12u));
    assert(gate_on(60u, 80u, 80u)); /* Retrigger at 60 restarts the window. */
    assert(!gate_on(60u, 80u, 140u));
    puts("Buzzer/PWM: 2k/1k/440 Hz, silence, constant-high fault, timeout wrap OK");
}

static void uart_frame(uint8_t value, uint8_t frame[10])
{
    unsigned bit;
    frame[0] = 0u;
    for (bit = 0u; bit < 8u; ++bit) {
        frame[bit + 1u] = (uint8_t)((value >> bit) & 1u);
    }
    frame[9] = 1u;
}

static int uart_decode_samples(const uint8_t frame[10], uint8_t *value)
{
    unsigned bit;
    uint8_t v = 0u;
    if (frame[0] != 0u || frame[9] != 1u) return 0;
    for (bit = 0u; bit < 8u; ++bit) {
        if (frame[bit + 1u] > 1u) return 0;
        v = (uint8_t)(v | (uint8_t)(frame[bit + 1u] << bit));
    }
    *value = v;
    return 1;
}

static void test_measurements(void)
{
    const uint8_t expected[10] = {0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u};
    uint8_t frame[10], value = 0u;
    unsigned i;
    uart_frame(0x55u, frame);
    for (i = 0u; i < 10u; ++i) assert(frame[i] == expected[i]);
    assert(uart_decode_samples(frame, &value) && value == 0x55u);
    frame[9] = 0u;
    assert(!uart_decode_samples(frame, &value));
    for (i = 0u; i <= 255u; ++i) {
        uart_frame((uint8_t)i, frame);
        assert(uart_decode_samples(frame, &value) && value == (uint8_t)i);
    }
    near_value(1000000.0 / 115200.0, 8.6805555556, 1e-9);
    near_value(10.0 * 1000000.0 / 115200.0, 86.8055555556, 1e-9);
    near_value(2000000.0 / 115200.0, 17.3611111111, 1e-9);
    near_value(1000000.0 / 10000000.0, 0.1, 1e-12);
    puts("Measurement math: PWM 25%, UART 8N1 0x55, 256 frames and timing OK");
}

int main(void)
{
    test_gpio_and_af();
    test_debounce();
    test_electrical();
    test_buzzer_pwm();
    test_measurements();
    puts("All GPIO note host assertions passed. No hardware was exercised.");
    return 0;
}
