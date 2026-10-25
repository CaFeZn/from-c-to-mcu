#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { MAGIC, VERSION, WORDS, BUILD, SEQUENCE, EXC_RETURN, CFSR,
       HFSR, PC, XPSR, FLAGS, CHECKSUM, RECORD_WORDS };
#define RECORD_MAGIC UINT32_C(0x4641554c)
#define FRAME_STACK_ERRORS UINT32_C(0x00003838)

static int basic_frame_range(uint32_t exc, uint32_t cfsr, uint32_t sp,
                             uint32_t low, uint32_t high)
{
    if (exc != UINT32_C(0xfffffff1) && exc != UINT32_C(0xfffffff9) &&
        exc != UINT32_C(0xfffffffd)) return 0;
    if ((cfsr & FRAME_STACK_ERRORS) != 0u || (sp & 3u) != 0u) return 0;
    if (high < low || high - low < 32u) return 0;
    return sp >= low && sp <= high - 32u;
}

static uint32_t record_sum(const uint32_t r[RECORD_WORDS], unsigned end)
{
    uint32_t sum = 0u;
    unsigned i;
    for (i = VERSION; i < end; ++i) sum += r[i];
    return sum;
}

static void record_prepare(uint32_t r[RECORD_WORDS])
{
    memset(r, 0, RECORD_WORDS * sizeof r[0]);
    r[VERSION] = 1u;
    r[WORDS] = RECORD_WORDS;
    r[BUILD] = UINT32_C(0x20261025);
    r[SEQUENCE] = 7u;
    r[EXC_RETURN] = UINT32_C(0xfffffff9);
    r[CFSR] = UINT32_C(0x00010000);
    r[HFSR] = UINT32_C(0x40000000);
    r[PC] = UINT32_C(0x08001234);
    r[XPSR] = UINT32_C(0x01000000);
    r[FLAGS] = 1u;
    r[CHECKSUM] = UINT32_C(0) - record_sum(r, CHECKSUM);
}

static int record_valid(const uint32_t r[RECORD_WORDS])
{
    return r[MAGIC] == RECORD_MAGIC && r[VERSION] == 1u &&
           r[WORDS] == RECORD_WORDS && record_sum(r, RECORD_WORDS) == 0u;
}

typedef struct {
    uint32_t start;
    uint32_t sent;
    uint32_t suppressed;
} log_budget_t;

static int log_admit(log_budget_t *b, uint32_t now)
{
    if ((uint32_t)(now - b->start) >= 1000u) {
        b->start = now;
        b->sent = 0u;
    }
    if (b->sent == 3u) {
        if (b->suppressed != UINT32_MAX) ++b->suppressed;
        return 0;
    }
    ++b->sent;
    return 1;
}

int main(void)
{
    uint32_t good[RECORD_WORDS], torn[RECORD_WORDS], bad[RECORD_WORDS];
    unsigned n, i;
    log_budget_t budget = {0u, 0u, 0u};
    assert(basic_frame_range(UINT32_C(0xfffffff9), 0u,
                            UINT32_C(0x20001000), UINT32_C(0x20000000),
                            UINT32_C(0x20020000)));
    assert(basic_frame_range(UINT32_C(0xfffffffd), 0u,
                            UINT32_C(0x2001ffe0), UINT32_C(0x20000000),
                            UINT32_C(0x20020000)));
    assert(!basic_frame_range(UINT32_C(0xffffffe9), 0u, 0u, 0u, 32u));
    assert(!basic_frame_range(UINT32_C(0xfffffff9), UINT32_C(0x1000), 0u, 0u, 32u));
    assert(!basic_frame_range(UINT32_C(0xfffffff9), 0u, 4u, 0u, 32u));
    assert(!basic_frame_range(UINT32_C(0xfffffff9), 0u, 1u, 0u, 64u));
    assert(!basic_frame_range(UINT32_C(0xfffffff9), 0u, 0u, 64u, 32u));
    {
        const uint32_t faults[] = {8u, 16u, 32u, 0x800u, 0x1000u, 0x2000u};
        for (i = 0u; i < sizeof faults / sizeof faults[0]; ++i)
            assert(!basic_frame_range(UINT32_C(0xfffffff9), faults[i], 0u, 0u, 32u));
    }
    assert(!basic_frame_range(UINT32_C(0xfffffff9), 0u, UINT32_MAX - 15u, 0u, UINT32_MAX));
    puts("frame: basic accepted, extended/bad-stack rejected");

    record_prepare(good);
    assert(!record_valid(good));
    for (n = 0u; n <= RECORD_WORDS; ++n) {
        memset(torn, 0, sizeof torn);
        for (i = 0u; i < n; ++i) torn[i] = good[i];
        assert(!record_valid(torn));
    }
    good[MAGIC] = RECORD_MAGIC;
    assert(record_valid(good));
    for (i = 0u; i < RECORD_WORDS; ++i) {
        memcpy(bad, good, sizeof bad);
        bad[i] ^= 1u;
        assert(!record_valid(bad));
    }
    memcpy(bad, good, sizeof bad);
    bad[VERSION] = 2u;
    bad[CHECKSUM] = 0u - record_sum(bad, CHECKSUM);
    assert(!record_valid(bad));
    record_prepare(bad);
    bad[SEQUENCE] = 8u;
    bad[CHECKSUM] = 0u - record_sum(bad, CHECKSUM);
    for (n = 0u; n < RECORD_WORDS; ++n) {
        memcpy(torn, good, sizeof torn);
        torn[MAGIC] = 0u; /* Invalidate the previous record first. */
        for (i = VERSION; i <= n; ++i) torn[i] = bad[i];
        assert(!record_valid(torn));
    }
    torn[MAGIC] = RECORD_MAGIC;
    assert(record_valid(torn) && torn[SEQUENCE] == 8u);
    memcpy(bad, good, sizeof bad);
    bad[PC] += 1u;
    bad[XPSR] -= 1u;
    assert(record_valid(bad)); /* Deliberately demonstrate additive-checksum collision. */
    printf("record: valid pc=%08" PRIx32 ", torn/corrupt/version rejected\n", good[PC]);

    for (i = 0u; i < 10u; ++i) assert(log_admit(&budget, 0u) == (i < 3u));
    assert(budget.suppressed == 7u);
    assert(!log_admit(&budget, 999u));
    assert(log_admit(&budget, 1000u));
    budget.start = UINT32_MAX - 499u;
    budget.sent = 3u;
    assert(log_admit(&budget, 500u));
    budget.sent = 3u;
    budget.suppressed = UINT32_MAX;
    assert(!log_admit(&budget, 501u));
    assert(budget.suppressed == UINT32_MAX);
    puts("budget: burst=3 suppressed=7, boundary/wrap/saturation passed");
    return 0;
}
