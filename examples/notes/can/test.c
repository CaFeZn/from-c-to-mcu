#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Logic and arithmetic checks only; not an electrical or bxCAN simulator. */
typedef struct {
    uint32_t btr;
    double rate;
    double sample_percent;
} timing_t;

static int near(double a, double b)
{
    double delta = a - b;
    return delta > -0.000001 && delta < 0.000001;
}

static int timing(uint32_t clock, unsigned pre, unsigned bs1,
                  unsigned bs2, unsigned sjw, timing_t *out)
{
    uint64_t divisor;
    unsigned n;
    if (out == NULL || clock == 0u || pre < 1u || pre > 1024u ||
        bs1 < 1u || bs1 > 16u || bs2 < 1u || bs2 > 8u ||
        sjw < 1u || sjw > 4u || sjw > bs2) return 0;
    n = 1u + bs1 + bs2;
    divisor = (uint64_t)pre * n;
    out->rate = (double)clock / (double)divisor;
    out->sample_percent = 100.0 * (double)(1u + bs1) / n;
    out->btr = ((uint32_t)(sjw - 1u) << 24)
             | ((uint32_t)(bs2 - 1u) << 20)
             | ((uint32_t)(bs1 - 1u) << 16) | (pre - 1u);
    return 1;
}

static void test_timing(void)
{
    timing_t t;
    assert(timing(42000000u, 6u, 11u, 2u, 1u, &t));
    assert(near(t.rate, 500000.0));
    assert(near(t.sample_percent, 85.7142857142857));
    assert(t.btr == UINT32_C(0x001A0005));
    assert(timing(42000000u, 6u, 10u, 3u, 1u, &t));
    assert(near(t.rate, 500000.0));
    assert(near(t.sample_percent, 78.5714285714286));
    assert(timing(42000000u, 4u, 16u, 4u, 2u, &t));
    assert(t.btr == UINT32_C(0x013F0003));
    assert(near(t.sample_percent, 80.9523809523810));
    assert(!timing(0u, 6u, 11u, 2u, 1u, &t));
    assert(!timing(42000000u, 0u, 11u, 2u, 1u, &t));
    assert(!timing(42000000u, 1025u, 11u, 2u, 1u, &t));
    assert(!timing(42000000u, 6u, 17u, 2u, 1u, &t));
    assert(!timing(42000000u, 6u, 11u, 2u, 3u, &t));
    assert(!timing(42000000u, 6u, 11u, 2u, 1u, NULL));
    puts("PASS timing: rates, sample points, BTR, invalid inputs");
}

/* Ideal dominant/recessive logic, not a transceiver circuit model. */
static unsigned bus_bit(unsigned a, unsigned b)
{
    assert(a <= 1u && b <= 1u);
    return a & b;
}

/* Equal-width comparison keys follow the arbitration bit order.
   Standard frames end before the synthetic trailing zero bits. */
static uint32_t arbitration_key(uint32_t id, int extended, unsigned rtr)
{
    assert(rtr <= 1u);
    if (extended) {
        assert(id <= UINT32_C(0x1FFFFFFF));
        return ((id >> 18) << 21) | (UINT32_C(1) << 20)
             | (UINT32_C(1) << 19)
             | ((id & UINT32_C(0x3FFFF)) << 1) | rtr;
    }
    assert(id <= UINT32_C(0x7FF));
    return (id << 21) | ((uint32_t)rtr << 20);
}

static void test_electrical_arbitration(void)
{
    assert(bus_bit(1u, 1u) == 1u);
    assert(bus_bit(1u, 0u) == 0u);
    assert(bus_bit(0u, 1u) == 0u);
    assert(bus_bit(0u, 0u) == 0u);
    assert(arbitration_key(0x100u, 0, 0u) <
           arbitration_key(0x200u, 0, 0u));
    assert(((0x100u >> 9) & 1u) == 0u);
    assert(((0x200u >> 9) & 1u) == 1u);
    assert(arbitration_key(0x120u, 0, 0u) <
           arbitration_key(0x121u, 0, 0u));
    puts("PASS electrical: four bus logic cases and same-format arbitration");
}

typedef enum { ERROR_ACTIVE, ERROR_PASSIVE, BUS_OFF } state_t;

/* Classification only, not all CAN counter update rules. */
static state_t from_counters(unsigned tec, unsigned rec)
{
    if (tec >= 256u) return BUS_OFF;
    if (tec >= 128u || rec >= 128u) return ERROR_PASSIVE;
    return ERROR_ACTIVE;
}

static state_t from_esr(uint32_t esr)
{
    if ((esr & 4u) != 0u) return BUS_OFF;
    if ((esr & 2u) != 0u) return ERROR_PASSIVE;
    return ERROR_ACTIVE;
}

static void test_errors(void)
{
    const uint32_t esr = UINT32_C(0x00800033);
    assert(from_counters(95u, 0u) == ERROR_ACTIVE);
    assert(from_counters(12u * 8u, 0u) == ERROR_ACTIVE);
    assert(from_counters(127u, 127u) == ERROR_ACTIVE);
    assert(from_counters(16u * 8u, 0u) == ERROR_PASSIVE);
    assert(from_counters(31u * 8u, 0u) == ERROR_PASSIVE);
    assert(from_counters(32u * 8u, 0u) == BUS_OFF);
    assert(from_counters(0u, 256u) == ERROR_PASSIVE);
    assert(from_esr(esr) == ERROR_PASSIVE);
    assert(((esr >> 16) & 255u) == 128u);
    assert(((esr >> 24) & 255u) == 0u);
    assert(((esr >> 4) & 7u) == 3u);
    assert(from_esr(4u) == BUS_OFF);
    assert(near(128.0 * 11.0 / 500000.0 * 1000.0, 2.816));
    puts("PASS errors: thresholds, ESR, BOFF precedence, recovery arithmetic");
}

static uint32_t filter_key(uint32_t id, int extended, unsigned rtr)
{
    assert(rtr <= 1u);
    if (extended) {
        assert(id <= UINT32_C(0x1FFFFFFF));
        return (id << 3) | 4u | ((uint32_t)rtr << 1);
    }
    assert(id <= UINT32_C(0x7FF));
    return (id << 21) | ((uint32_t)rtr << 1);
}

static int accepts(uint32_t key, uint32_t filter, uint32_t mask)
{
    return ((key ^ filter) & mask) == 0u;
}

static void test_filters(void)
{
    const uint32_t filter = UINT32_C(0x100) << 21;
    const uint32_t mask = (UINT32_C(0x7F0) << 21) | 6u;
    unsigned count = 0u;
    assert(filter == UINT32_C(0x20000000));
    assert(mask == UINT32_C(0xFE000006));
    for (uint32_t id = 0u; id <= 0x7FFu; ++id) {
        int expected = id >= 0x100u && id <= 0x10Fu;
        int actual = accepts(filter_key(id, 0, 0u), filter, mask);
        assert(actual == expected);
        count += (unsigned)actual;
        assert(!accepts(filter_key(id, 0, 1u), filter, mask));
        assert(!accepts(filter_key(id << 18, 1, 0u), filter, mask));
    }
    assert(count == 16u);
    assert(accepts(filter_key(0x105u, 0, 1u), filter, mask & ~6u));
    assert(accepts(filter_key(0x105u << 18, 1, 0u), filter, mask & ~6u));
    assert((UINT32_C(0x205) << 21) == UINT32_C(0x40A00000));
    assert(((UINT32_C(0x7FF) << 21) | 6u) == UINT32_C(0xFFE00006));
    assert((0x100u << 5) == 0x2000u);
    assert((0x205u << 5) == 0x40A0u);
    puts("PASS filters: exhaustive 11-bit IDs, RTR/IDE rejection, packing");
}

static void test_frames(void)
{
    const uint32_t ext = (UINT32_C(0x123) << 18) | 5u;
    assert(ext == UINT32_C(0x048C0005));
    assert((ext >> 18) == 0x123u);
    assert((ext & UINT32_C(0x3FFFF)) == 5u);
    assert(arbitration_key(0x123u, 0, 0u) < arbitration_key(ext, 1, 0u));
    assert(arbitration_key(0x123u, 0, 1u) < arbitration_key(ext, 1, 0u));
    assert(arbitration_key(ext, 1, 0u) < arbitration_key(ext, 1, 1u));
    assert(arbitration_key(0x3FFFFu, 1, 0u) <
           arbitration_key(0x100u, 0, 0u));
    assert(arbitration_key(0x1FFFFFFFu, 1, 1u) == UINT32_MAX);
    puts("PASS frames: 29-bit packing, standard/extended and RTR arbitration");
}

static int fd_length(unsigned dlc, unsigned *bytes)
{
    static const unsigned table[16] = {
        0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u,
        12u, 16u, 20u, 24u, 32u, 48u, 64u
    };
    if (dlc >= 16u || bytes == NULL) return 0;
    *bytes = table[dlc];
    return 1;
}

static void test_fd_canopen(void)
{
    unsigned length;
    for (unsigned dlc = 0u; dlc <= 8u; ++dlc) {
        assert(fd_length(dlc, &length) && length == dlc);
    }
    assert(fd_length(9u, &length) && length == 12u);
    assert(fd_length(10u, &length) && length == 16u);
    assert(fd_length(11u, &length) && length == 20u);
    assert(fd_length(12u, &length) && length == 24u);
    assert(fd_length(13u, &length) && length == 32u);
    assert(fd_length(14u, &length) && length == 48u);
    assert(fd_length(15u, &length) && length == 64u);
    assert(!fd_length(16u, &length));
    assert(!fd_length(0u, NULL));
    assert(0x700u + 5u == 0x705u);
    assert(0x70Cu - 0x700u == 12u);
    puts("PASS FD/CANopen: all DLC values, invalid inputs, boot-up ID arithmetic");
}

static void test_topology(void)
{
    assert(near(1.0 / (1.0 / 120.0 + 1.0 / 120.0), 60.0));
    assert(near(120.0 / 3.0, 40.0));
    assert(400u + 2u * 20u * 5u == 600u);
    assert(400u + 2u * 100u * 5u == 1400u);
    assert((700u - 400u) / (2u * 5u) == 30u);
    assert((700u - 520u) / (2u * 5u) == 18u);
    assert(520u + 2u * 20u * 5u == 720u);
    puts("PASS topology: termination and explicitly assumed delay budgets");
}

static unsigned frame_bound(unsigned d, int extended)
{
    unsigned extra = extended ? 20u : 0u;
    assert(d <= 8u);
    return 47u + extra + 8u * d
         + (34u + extra + 8u * d - 1u) / 4u;
}

static void test_load(void)
{
    double load;
    assert(frame_bound(0u, 0) == 55u);
    assert(frame_bound(4u, 0) == 95u);
    assert(frame_bound(8u, 0) == 135u);
    assert(frame_bound(0u, 1) == 80u);
    assert(frame_bound(4u, 1) == 120u);
    assert(frame_bound(8u, 1) == 160u);
    assert(frame_bound(3u, 0) == 85u);
    load = 170.0 / 10000.0 + 270.0 / 20000.0 + 270.0 / 100000.0;
    assert(near(load * 100.0, 3.32));
    load = 170.0 / 5000.0 + 270.0 / 20000.0 + 270.0 / 100000.0;
    assert(near(load * 100.0, 5.02));
    assert(270u + 170u == 440u);
    assert(270u + 170u + 270u == 710u);
    assert(55u * 2u + 170u == 280u);
    puts("PASS load: frame bounds, utilization and one-shot blocking timeline");
}

int main(void)
{
    test_timing();
    test_electrical_arbitration();
    test_errors();
    test_filters();
    test_frames();
    test_fd_canopen();
    test_topology();
    test_load();
    puts("All eight CAN lesson model groups passed.");
    return 0;
}
