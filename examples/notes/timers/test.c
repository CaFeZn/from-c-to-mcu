#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

/* Arithmetic/state models, not emulation of STM32 registers. */
static int encoder_delta(uint16_t previous, uint16_t current,
                         int32_t *delta)
{
    uint32_t u = (uint16_t)(current - previous);
    if (u == 32768u) {
        return 0;
    }
    *delta = u < 32768u ? (int32_t)u : (int32_t)u - 65536;
    return 1;
}

static uint32_t deadtime_cycles(uint8_t dtg)
{
    if (dtg < 0x80u) return dtg;
    if (dtg < 0xc0u) return (64u + (dtg & 0x3fu)) * 2u;
    if (dtg < 0xe0u) return (32u + (dtg & 0x1fu)) * 8u;
    return (32u + (dtg & 0x1fu)) * 16u;
}

static int systick_reload(uint32_t clock_hz, uint32_t tick_hz,
                          uint32_t *reload)
{
    if (tick_hz == 0u || clock_hz % tick_hz != 0u) return 0;
    uint32_t cycles = clock_hz / tick_hz;
    /* Exclude LOAD=0: not a useful repeating exception source. */
    if (cycles < 2u || cycles > 16777216u) return 0;
    *reload = cycles - 1u;
    return 1;
}

static void test_encoder(void)
{
    int32_t delta = 0;
    assert(encoder_delta(65530u, 4u, &delta) && delta == 10);
    assert(encoder_delta(4u, 65530u, &delta) && delta == -10);
    assert(encoder_delta(65535u, 0u, &delta) && delta == 1);
    assert(encoder_delta(0u, 65535u, &delta) && delta == -1);
    assert(!encoder_delta(0u, 32768u, &delta));
    /* Abstract A/B ordering convention from the lesson. */
    const int8_t steps[4][4] = {
        {0, -1, 1, 0}, {1, 0, 0, -1},
        {-1, 0, 0, 1}, {0, 1, -1, 0}
    };
    const uint8_t forward[] = {0u, 2u, 3u, 1u, 0u};
    int total = 0;
    for (unsigned i = 1u; i < 5u; ++i) {
        assert((forward[i] ^ forward[i - 1u]) != 3u);
        total += steps[forward[i - 1u]][forward[i]];
    }
    assert(total == 4);
    assert(fabs(100.0 / 4000.0 / 0.01 * 60.0 - 150.0) < 1e-9);
    puts("encoder: +10, -10, half-turn rejected; quadrature=+4");
}

static void test_deadtime(void)
{
    const uint8_t codes[] = {0u, 0x54u, 0x7fu, 0x80u, 0x94u,
                              0xbfu, 0xc0u, 0xdfu, 0xe0u, 0xffu};
    const uint32_t cycles[] = {0u, 84u, 127u, 128u, 168u,
                                254u, 256u, 504u, 512u, 1008u};
    for (unsigned i = 0u; i < sizeof codes / sizeof codes[0]; ++i) {
        assert(deadtime_cycles(codes[i]) == cycles[i]);
    }
    for (unsigned i = 1u; i <= 255u; ++i) {
        assert(deadtime_cycles((uint8_t)i) > deadtime_cycles((uint8_t)(i - 1u)));
    }
    assert(deadtime_cycles(0xa8u) == 208u);
    assert(fabs(deadtime_cycles(0x54u) * 1e9 / 168e6 - 500.0) < 1e-9);
    assert(fabs(deadtime_cycles(0x94u) * 1e9 / 168e6 - 1000.0) < 1e-9);
    puts("deadtime: 0x54=500 ns, 0x94=1000 ns; all segments checked");
}

static void test_systick(void)
{
    uint32_t reload = 0u;
    assert(systick_reload(16000000u, 1000u, &reload) && reload == 15999u);
    assert(systick_reload(168000000u, 1000u, &reload) && reload == 167999u);
    assert(systick_reload(84000000u, 1000u, &reload) && reload == 83999u);
    assert(systick_reload(168000000u, 100u, &reload) && reload == 1679999u);
    assert(systick_reload(16777216u, 1u, &reload) && reload == 0xffffffu);
    assert(!systick_reload(168000000u, 1u, &reload));
    assert(!systick_reload(1000u, 1000u, &reload));
    assert(!systick_reload(1000u, 0u, &reload));
    const uint32_t start = 100u, wait = 3u;
    for (uint32_t now = 100u; now < 103u; ++now) {
        assert((uint32_t)(now - start) < wait);
    }
    assert((uint32_t)(103u - start) == wait);
    assert((uint32_t)(1u - (UINT32_MAX - 1u)) == 3u);
    puts("systick: LOAD=167999; Delay(2) model waits 3 default ticks");
}

static void test_rtc_accuracy(void)
{
    assert(32768u / ((127u + 1u) * (255u + 1u)) == 1u);
    assert(fabs(32768.0 / (129.0 * 257.0) - 0.9883871746146653) < 1e-12);
    assert((255.0 - 127.0) / 256.0 == 0.5);
    assert((255.0 - 191.0) / 256.0 == 0.25);
    assert((255.0 - 0.0) / 256.0 * 1000.0 == 996.09375);
    assert(fabs(86400.0 * 20.0 / 1e6 - 1.728) < 1e-12);
    assert(fabs(30.0 * 86400.0 * 20.0 / 1e6 - 51.84) < 1e-10);
    assert(fabs(1e6 / 1048576.0 - 0.95367431640625) < 1e-12);
    /* RM0090 exact smooth-calibration ratio for CALP=0, CALM=21. */
    double calibrated = 32768.0 * 1.000020 *
        (1.0 - 21.0 / (1048576.0 + 21.0));
    assert(fabs((calibrated / 32768.0 - 1.0) * 1e6) < 0.03);
    puts("rtc: 1 Hz, SSR=127 -> 500 ms; 20 ppm -> 1.728 s/day");
}

int main(void)
{
    test_encoder();
    test_deadtime();
    test_systick();
    test_rtc_accuracy();
    return 0;
}
