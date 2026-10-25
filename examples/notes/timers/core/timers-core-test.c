#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

/* Teaching model: synchronized upcounting, one channel, integer clock edges.
 * It is not a peripheral emulator and performs no MMIO. ARR writes in these
 * tests are buffered; bus latency, NVIC, GPIO, and electrical effects are absent. */
typedef struct {
    uint32_t psc, arr, ccr;
    uint32_t psc_pre, arr_pre, ccr_pre;
    uint32_t cnt, prescale_count;
    int arpe, ocpe, udis, urs, uif;
    uint64_t updates;
} timer_model;

static timer_model timer_make(uint32_t psc, uint32_t arr, uint32_t ccr)
{
    timer_model t = {0};
    assert(psc <= UINT16_MAX && arr <= UINT16_MAX && ccr <= UINT16_MAX);
    t.psc = t.psc_pre = psc;
    t.arr = t.arr_pre = arr;
    t.ccr = t.ccr_pre = ccr;
    t.arpe = t.ocpe = 1;
    return t;
}

static void update_event(timer_model *t, int software)
{
    if (t->udis) {
        return;
    }
    t->psc = t->psc_pre;
    if (t->arpe) {
        t->arr = t->arr_pre;
    }
    if (t->ocpe) {
        t->ccr = t->ccr_pre;
    }
    ++t->updates;
    if (!software || !t->urs) {
        t->uif = 1;
    }
}

static void software_ug(timer_model *t)
{
    t->cnt = 0;
    t->prescale_count = 0;
    update_event(t, 1);
}

static void input_tick(timer_model *t)
{
    if (t->arr == 0) {
        return;
    }
    assert(t->cnt <= t->arr);
    if (++t->prescale_count <= t->psc) {
        return;
    }
    t->prescale_count = 0;
    if (t->cnt == t->arr) {
        t->cnt = 0;
        update_event(t, 0);
    } else {
        ++t->cnt;
    }
}

static void advance(timer_model *t, uint32_t input_ticks)
{
    uint32_t i;
    for (i = 0; i < input_ticks; ++i) {
        input_tick(t);
    }
}

static void write_ccr(timer_model *t, uint32_t value)
{
    assert(value <= UINT16_MAX);
    t->ccr_pre = value;
    if (!t->ocpe) {
        t->ccr = value;
    }
}

static uint32_t high_ticks(timer_model *t, uint32_t count)
{
    uint32_t i, high = 0;
    assert(t->psc == 0);
    for (i = 0; i < count; ++i) {
        high += t->cnt < t->ccr ? 1u : 0u;
        input_tick(t);
    }
    return high;
}

static void test_timer(void)
{
    timer_model t = timer_make(0, 4, 0);
    uint32_t i;
    uint64_t full_divisor = (uint64_t)(UINT16_MAX + 1u) * (UINT16_MAX + 1u);
    assert(full_divisor == UINT64_C(4294967296));
    t.psc_pre = 3;
    assert(t.psc == 0);
    software_ug(&t);
    assert(t.psc == 3 && t.cnt == 0 && t.uif == 1);
    t.uif = 0;
    for (i = 1; i <= 4; ++i) {
        advance(&t, 4);
        assert(t.cnt == i && t.uif == 0);
    }
    advance(&t, 4);
    assert(t.cnt == 0 && t.uif == 1 && t.updates == 2);
    advance(&t, 40);
    assert(t.updates == 4 && t.uif == 1);
    t.uif = 0;
    t.urs = 1;
    t.psc_pre = 7;
    software_ug(&t);
    assert(t.psc == 7 && t.uif == 0);
    advance(&t, 40);
    assert(t.uif == 1);
    t = timer_make(0, 0, 0);
    advance(&t, 100);
    assert(t.cnt == 0 && t.updates == 0);
    assert(UINT64_C(84) * 1000 == 84000);
    assert(UINT64_C(85) * 1001 == 85085);
    puts("timer: PSC=3 ARR=4 -> 20 input ticks; three overflows -> UIF=1");
}

static void test_duty_change(int preload)
{
    timer_model t = timer_make(0, 999, 250);
    uint32_t i, high = 0, rises = 0;
    int previous = 0;
    t.ocpe = preload;
    for (i = 0; i < 1000; ++i) {
        int output;
        if (i == 600) {
            write_ccr(&t, 750);
        }
        output = t.cnt < t.ccr;
        high += output ? 1u : 0u;
        if (output && !previous) {
            ++rises;
        }
        previous = output;
        input_tick(&t);
    }
    assert(high == (preload ? 250u : 400u));
    assert(rises == (preload ? 1u : 2u));
    assert(high_ticks(&t, 1000) == 750);
    printf("pwm: preload=%d first_high=%" PRIu32 " first_pulses=%" PRIu32 " next_high=750\n",
           preload, high, rises);
}

/* Reject unrepresentable compare values instead of writing reserved bits. */
static int compare_for(uint32_t arr, uint32_t permille, uint32_t *out)
{
    uint64_t compare;
    if (arr == 0 || arr > UINT16_MAX || permille > 1000) {
        return 0;
    }
    compare = ((uint64_t)arr + 1) * permille / 1000;
    if (compare > UINT16_MAX) {
        return 0;
    }
    *out = (uint32_t)compare;
    return 1;
}

static void test_group_update(void)
{
    timer_model t = timer_make(0, 999, 500);
    uint32_t compare = 123;
    t.arr_pre = 499;
    advance(&t, 1000);
    assert(t.arr == 499 && t.ccr == 500);
    write_ccr(&t, 250);
    assert(high_ticks(&t, 500) == 500);
    assert(high_ticks(&t, 500) == 250);

    t = timer_make(0, 999, 500);
    t.udis = 1;
    t.arr_pre = 499;
    advance(&t, 1000);
    assert(t.cnt == 0 && t.arr == 999 && t.ccr == 500 && t.updates == 0);
    write_ccr(&t, 250);
    t.udis = 0;
    assert(t.arr == 999 && t.ccr == 500);
    advance(&t, 1000);
    assert(t.arr == 499 && t.ccr == 250);
    assert(high_ticks(&t, 500) == 250);

    assert(!compare_for(65535, 1000, &compare) && compare == 123);
    assert(compare_for(65534, 1000, &compare) && compare == 65535);
    assert(compare_for(999, 1000, &compare) && compare == 1000);
    t = timer_make(0, 999, 1000);
    assert(high_ticks(&t, 1000) == 1000);
    t = timer_make(0, 999, 0);
    assert(high_ticks(&t, 1000) == 0);
    t = timer_make(0, 65535, 65535);
    assert(high_ticks(&t, 65536) == 65535);
    puts("pwm: split update has 100% cycle; UDIS groups values; CCR=65536 rejected");
}

static int apb_divisor_valid(uint32_t div)
{
    return div == 1 || div == 2 || div == 4 || div == 8 || div == 16;
}

static uint64_t timer_clock(uint64_t hclk, uint32_t apb_div)
{
    assert(apb_divisor_valid(apb_div));
    assert(hclk % apb_div == 0);
    return hclk / apb_div * (apb_div == 1 ? 1u : 2u);
}

/* Integer-only examples, not a complete clock-plan validator. */
static int pll_clock(uint64_t source, uint32_t m, uint32_t n, uint32_t p,
                     uint32_t q, uint64_t *sysclk, uint64_t *pll48)
{
    uint64_t input, vco;
    if (m < 2 || m > 63 || n < 50 || n > 432 || q < 2 || q > 15 ||
        !(p == 2 || p == 4 || p == 6 || p == 8) || source % m != 0) {
        return 0;
    }
    input = source / m;
    if (input < 1000000 || input > 2000000) {
        return 0;
    }
    vco = input * n;
    if (vco < 100000000 || vco > 432000000 ||
        vco % p != 0 || vco % q != 0 || vco / p > 168000000 ||
        vco / q > 48000000) {
        return 0;
    }
    *sysclk = vco / p;
    *pll48 = vco / q;
    return 1;
}

static void test_clock_tree(void)
{
    uint64_t sysclk = 0, pll48 = 0;
    uint64_t hclk, pclk1, pclk2, tim1, tim2;
    assert(pll_clock(8000000, 4, 168, 2, 7, &sysclk, &pll48));
    hclk = sysclk;
    pclk1 = hclk / 4;
    pclk2 = hclk / 2;
    tim1 = timer_clock(hclk, 4);
    tim2 = timer_clock(hclk, 2);
    assert(hclk == 168000000 && pclk1 == 42000000 && pclk2 == 84000000);
    assert(tim1 == 84000000 && tim2 == 168000000 && pll48 == 48000000);
    assert(tim1 / (84u * 1000u) == 1000);
    assert(tim1 / (42u * 1000u) == 2000);
    assert(timer_clock(hclk, 8) / (84u * 1000u) == 500);
    assert(timer_clock(16000000, 1) == 16000000);
    assert(timer_clock(16000000, 2) == 16000000);
    assert(timer_clock(16000000, 4) == 8000000);
    assert(!pll_clock(12000000, 4, 168, 2, 7, &sysclk, &pll48));
    assert(pll_clock(12000000, 6, 168, 2, 7, &sysclk, &pll48));
    assert(sysclk == 168000000 && pll48 == 48000000);
    printf("clock: HCLK=%" PRIu64 " PCLK1=%" PRIu64 " TIM_APB1=%" PRIu64
           " PLLQ=%" PRIu64 " Hz\n", hclk, pclk1, tim1, pll48);
}

int main(void)
{
    test_timer();
    test_duty_change(0);
    test_duty_change(1);
    test_group_update();
    test_clock_tree();
    puts("timers core: all assertions passed (teaching model, no hardware)");
    return 0;
}
