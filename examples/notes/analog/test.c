#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>

#define ADC_MAX 4095u
#define DMA_LENGTH 8u
#define EVENT_HT 1u
#define EVENT_TC 2u

typedef struct {
    uint16_t memory[DMA_LENGTH];
    size_t cursor;
    size_t remaining;
    bool increment;
    bool circular;
    bool enabled;
} dma_model_t;

static uint32_t adc_code_to_mv(uint16_t code, uint32_t reference_mv)
{
    assert(code <= ADC_MAX);
    return (uint32_t)(((uint64_t)code * reference_mv + ADC_MAX / 2u)
                      / ADC_MAX);
}

static double settling_error_lsb(unsigned cycles, double adc_hz,
                                 double tau_seconds)
{
    assert(adc_hz > 0.0 && tau_seconds > 0.0);
    return 4096.0 * exp(-((double)cycles / adc_hz) / tau_seconds);
}

static unsigned dma_request(dma_model_t *dma, uint16_t value)
{
    unsigned event = 0u;
    if (!dma->enabled) {
        return event;
    }
    assert(dma->cursor < DMA_LENGTH && dma->remaining > 0u);
    dma->memory[dma->cursor] = value;
    if (dma->increment) {
        ++dma->cursor;
    }
    --dma->remaining;
    if (dma->remaining == DMA_LENGTH / 2u) {
        event |= EVENT_HT;
    }
    if (dma->remaining == 0u) {
        event |= EVENT_TC;
        if (dma->circular) {
            dma->cursor = 0u;
            dma->remaining = DMA_LENGTH;
        } else {
            dma->enabled = false;
        }
    }
    return event;
}

static bool block_fits_deadline(uint32_t latency_us, uint32_t work_us,
                                uint32_t guard_us, uint32_t half_period_us)
{
    return (uint64_t)latency_us + work_us + guard_us < half_period_us;
}

static uint32_t timer_update_hz(uint32_t clock_hz, uint16_t psc,
                                uint16_t arr)
{
    uint64_t divider = ((uint64_t)psc + 1u) * ((uint64_t)arr + 1u);
    return (uint32_t)((uint64_t)clock_hz / divider);
}

static uint64_t sequence_cycles(const uint16_t *sampling, size_t channels)
{
    uint64_t cycles = 0u;
    for (size_t i = 0u; i < channels; ++i) {
        cycles += (uint64_t)sampling[i] + 12u;
    }
    return cycles;
}

static bool vref_from_cal(uint16_t calibration, uint16_t measured,
                          uint32_t calibration_mv, uint32_t *reference_mv)
{
    if (reference_mv == NULL || calibration == 0u || calibration >= ADC_MAX ||
        measured == 0u || measured >= ADC_MAX ||
        calibration_mv < 1800u || calibration_mv > 3600u) {
        return false;
    }
    uint64_t numerator = (uint64_t)calibration * calibration_mv;
    uint32_t result = (uint32_t)((numerator + measured / 2u) / measured);
    if (result < 1800u || result > 3600u) {
        return false;
    }
    *reference_mv = result;
    return true;
}

static void test_sampling(void)
{
    double e3 = settling_error_lsb(3u, 21000000.0, 0.5e-6);
    double e56 = settling_error_lsb(56u, 21000000.0, 0.5e-6);
    double e144 = settling_error_lsb(144u, 21000000.0, 0.5e-6);
    assert(e3 > 3000.0 && e3 < 3100.0);
    assert(e56 > 19.0 && e56 < 20.0);
    assert(e144 < 0.01);
    assert(0.5e-6 * log(8192.0) > 4.5e-6);
    assert(adc_code_to_mv(0u, 3300u) == 0u);
    assert(adc_code_to_mv(2048u, 3300u) == 1650u);
    assert(adc_code_to_mv(4095u, 3300u) == 3300u);
    printf("sampling: error_lsb=%.3f,%.3f,%.6f; midpoint=1650 mV\n",
           e3, e56, e144);
}

static void test_dma(void)
{
    dma_model_t dma = {{0}, 0u, DMA_LENGTH, true, false, true};
    for (size_t i = 0u; i < DMA_LENGTH; ++i) {
        unsigned event = dma_request(&dma, (uint16_t)((i + 1u) * 100u));
        assert(event == (i == 3u ? EVENT_HT : i == 7u ? EVENT_TC : 0u));
    }
    assert(dma.remaining == 0u && !dma.enabled);
    assert(dma_request(&dma, 999u) == 0u && dma.memory[7] == 800u);
    for (size_t i = 0u; i < DMA_LENGTH; ++i) {
        assert(dma.memory[i] == (uint16_t)((i + 1u) * 100u));
    }
    dma_model_t no_increment = {{0}, 0u, DMA_LENGTH, false, false, true};
    for (size_t i = 0u; i < DMA_LENGTH; ++i) {
        (void)dma_request(&no_increment, (uint16_t)((i + 1u) * 100u));
    }
    assert(no_increment.memory[0] == 800u && no_increment.memory[1] == 0u);
    printf("dma: HT after 4, TC after 8, no-MINC keeps only last value\n");
}

static void test_streaming(void)
{
    dma_model_t dma = {{0}, 0u, DMA_LENGTH, true, true, true};
    uint16_t snapshot[DMA_LENGTH / 2u];
    unsigned ht = 0u;
    unsigned tc = 0u;
    unsigned callbacks = 0u;
    bool ready = false;
    for (size_t i = 0u; i < 2u * DMA_LENGTH; ++i) {
        unsigned event = dma_request(&dma, (uint16_t)(i + 1u));
        if ((event & EVENT_HT) != 0u) {
            ++ht;
            ++callbacks;
            ready = true;
        }
        if ((event & EVENT_TC) != 0u) {
            ++tc;
        }
        if (i == 3u) {
            for (size_t j = 0u; j < DMA_LENGTH / 2u; ++j) {
                snapshot[j] = dma.memory[j];
            }
        }
    }
    assert(ht == 2u && tc == 2u && callbacks == 2u && ready);
    assert(dma.remaining == DMA_LENGTH);
    for (size_t j = 0u; j < DMA_LENGTH / 2u; ++j) {
        assert(snapshot[j] == (uint16_t)(j + 1u));
        assert(dma.memory[j] == (uint16_t)(j + 9u));
    }
    assert(block_fits_deadline(1000u, 4000u, 500u, 16000u));
    assert(!block_fits_deadline(1000u, 16000u, 500u, 16000u));
    assert(!block_fits_deadline(UINT32_MAX, 1u, 1u, 16000u));
    printf("stream: saved=1..4, live=9..12; 5.5 ms PASS, 17.5 ms REJECT\n");
}

static void test_timer(void)
{
    const uint16_t one_channel[] = {56u};
    const uint16_t three_channels[] = {56u, 56u, 480u};
    uint32_t fs = timer_update_hz(84000000u, 83u, 499u);
    assert(fs == 2000u);
    assert(timer_update_hz(84000000u, 83u, 999u) == 1000u);
    assert(timer_update_hz(UINT32_MAX, UINT16_MAX, UINT16_MAX) == 0u);
    assert(sequence_cycles(one_channel, 1u) == 68u);
    assert(sequence_cycles(three_channels, 3u) == 628u);
    assert(sequence_cycles(three_channels, 3u) * fs < 21000000u);
    assert(!(sequence_cycles(three_channels, 3u) * 50000u < 21000000u));
    printf("timer: 2000 Hz, one=68 cycles, three=628 cycles; 50 kHz REJECT\n");
}

static void test_reference(void)
{
    uint32_t reference_mv = 777u;
    assert(vref_from_cal(1500u, 1650u, 3300u, &reference_mv));
    assert(reference_mv == 3000u);
    assert(adc_code_to_mv(2048u, reference_mv) == 1500u);
    assert(adc_code_to_mv(2048u, 3300u) == 1650u);
    assert(!vref_from_cal(1500u, 0u, 3300u, &reference_mv));
    assert(!vref_from_cal(UINT16_MAX, 1650u, 3300u, &reference_mv));
    assert(!vref_from_cal(1500u, 4095u, 3300u, &reference_mv));
    assert(!vref_from_cal(1500u, 10u, 3300u, &reference_mv));
    assert(!vref_from_cal(1500u, 1650u, 3300u, NULL));
    assert(reference_mv == 3000u);
    assert(vref_from_cal(1547u, 1547u, 3210u, &reference_mv));
    assert(reference_mv == 3210u);
    printf("reference: VREF+=3000 mV, input=1500 mV, invalid readings rejected\n");
}

int main(void)
{
    test_sampling();
    test_dma();
    test_streaming();
    test_timer();
    test_reference();
    puts("analog: all five topic models passed");
    return 0;
}
