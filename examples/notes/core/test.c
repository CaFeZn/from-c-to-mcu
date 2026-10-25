/* C99 deterministic models: no host threads, no MCU address dereferences. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint16_t bsrr_apply(uint16_t odr, uint32_t command)
{
    uint32_t set = command & UINT32_C(0xffff);
    uint32_t reset = command >> 16;
    return (uint16_t)(((uint32_t)odr & ~reset) | set);
}

static void test_rmw(void)
{
    uint32_t shared = 10u;
    uint32_t local = shared;
    ++shared; /* ISR runs to completion. */
    shared = local + 1u;
    assert(shared == 11u);
    shared = 10u;
    ++shared; /* Critical main update finishes before ISR enters. */
    ++shared;
    assert(shared == 12u);
    assert(bsrr_apply(1u, UINT32_C(0x20)) == 0x21u);
    assert(bsrr_apply(0x21u, UINT32_C(0x00200000)) == 1u);
    assert(bsrr_apply(0u, UINT32_C(0x00200020)) == 0x20u);
    puts("rmw: lost=11, protected=12, bsrr=pass");
}

typedef struct { unsigned enabled, pending, active, priority; } irq_t;

static void test_nvic(void)
{
    irq_t a = {1u, 0u, 0u, 5u};
    irq_t b = {1u, 0u, 0u, 2u};
    a.pending = 1u;
    a.pending = 1u;
    a.pending = 1u;
    unsigned serviced = 0u;
    if (a.enabled && a.pending) {
        a.pending = 0u;
        a.active = 1u;
        ++serviced;
    }
    assert(serviced == 1u);
    b.pending = 1u;
    assert(b.priority < a.priority); /* B can preempt A. */
    b.active = 1u;
    b.pending = 0u;
    a.pending = 1u; /* A is now active AND pending. */
    assert(a.active && a.pending);
    b.active = 0u;
    a.active = 0u;
    if (a.pending) { ++serviced; a.pending = 0u; }
    assert(serviced == 2u);
    assert((5u << 4) == 0x50u);
    puts("nvic: repeated pending coalesces, 2 preempts 5");
}

static void test_vector(void)
{
    const uint32_t irqn = 6u;
    const uint32_t base = UINT32_C(0x08004000);
    const uint32_t slot = base + 4u * (irqn + 16u);
    assert(slot == UINT32_C(0x08004058));
    assert((base & 511u) == 0u);
    assert((16u + 82u) * 4u == 392u);
    assert((UINT32_C(0x08000101) & ~UINT32_C(1)) == UINT32_C(0x08000100));
    printf("vector: EXTI0 slot=0x%08lX, table=392 bytes\n", (unsigned long)slot);
}

static uint32_t w1c(uint32_t state, uint32_t command)
{
    return state & ~command;
}

static void test_storm(void)
{
    uint32_t before = 3u;
    assert(w1c(before, 1u) == 2u);
    assert(w1c(before, before | 1u) == 0u);
    assert(w1c(before, before & ~1u) == 1u);
    uint32_t pr = w1c(1u, 1u); /* HAL clears old event. */
    pr |= 1u;                 /* New event during callback. */
    pr = w1c(pr, 1u);         /* Bad duplicate acknowledge. */
    assert(pr == 0u);
    puts("storm: W1C direct=2, or=0, and=1; duplicate clear loses event");
}

typedef struct { uint32_t value; int tagged; } exclusive_t;

static uint32_t load_exclusive(exclusive_t *m)
{
    m->tagged = 1;
    return m->value;
}

static unsigned store_exclusive(exclusive_t *m, uint32_t next)
{
    unsigned failed = m->tagged ? 0u : 1u;
    m->tagged = 0;
    if (failed == 0u) { m->value = next; }
    return failed;
}

static void test_exclusive(void)
{
    exclusive_t m = {10u, 0};
    uint32_t old = load_exclusive(&m);
    m.tagged = 0; /* Exception clears local tag, regardless of ISR writes. */
    ++m.value;
    assert(store_exclusive(&m, old + 1u) == 1u);
    assert(m.value == 11u);
    old = load_exclusive(&m);
    assert(store_exclusive(&m, old + 1u) == 0u);
    assert(m.value == 12u);
    old = load_exclusive(&m);
    m.tagged = 0; /* Empty ISR or CLREX: still fails. */
    assert(store_exclusive(&m, old + 1u) == 1u);
    m.value = UINT32_MAX;
    old = load_exclusive(&m);
    assert(store_exclusive(&m, old + 1u) == 0u);
    assert(m.value == 0u);
    puts("exclusive: first=fail, retry=success, value=12; wrap=pass");
}

typedef struct { uint32_t word[2], ready; } publication_t;

static int receive(const publication_t *p, uint32_t out[2])
{
    if (!p->ready) { return 0; }
    out[0] = p->word[0];
    out[1] = p->word[1];
    return 1;
}

static void test_ordering(void)
{
    publication_t p = {{0u, 0u}, 0u};
    uint32_t out[2] = {99u, 99u};
    p.word[0] = 10u;
    assert(receive(&p, out) == 0);
    p.word[1] = 20u;
    p.ready = 1u;
    assert(receive(&p, out) == 1);
    assert(out[0] == 10u && out[1] == 20u);
    publication_t bad = {{0u, 0u}, 1u}; /* Deliberately publish too early. */
    assert(receive(&bad, out) == 1);
    assert(out[0] == 0u && out[1] == 0u);
    puts("ordering: publish-last=(10,20), publish-first=(0,0)");
}

typedef struct { uint32_t sec, ms; } stamp_t;
typedef struct { uint32_t seq; stamp_t value; } sequence_t;

static void publish(sequence_t *s, stamp_t next)
{
    ++s->seq;
    s->value = next;
    ++s->seq;
}

static int try_snapshot(sequence_t *s, stamp_t *out, int inject)
{
    uint32_t first = s->seq;
    if (first & 1u) { return 0; }
    stamp_t copy;
    copy.sec = s->value.sec;
    if (inject) {
        const stamp_t next = {8u, 0u};
        publish(s, next);
    }
    copy.ms = s->value.ms;
    if (first != s->seq) { return 0; }
    *out = copy;
    return 1;
}

typedef enum { FREE_SLOT, READY_SLOT, HELD_SLOT } slot_state_t;
typedef struct { stamp_t value; slot_state_t state; } slot_t;
typedef struct { slot_t slot[2]; unsigned put, get; } pair_t;

static int pair_publish(pair_t *p, stamp_t value)
{
    slot_t *s = &p->slot[p->put];
    if (s->state != FREE_SLOT) { return 0; }
    s->value = value;
    s->state = READY_SLOT;
    p->put ^= 1u;
    return 1;
}

static const stamp_t *pair_acquire(pair_t *p)
{
    slot_t *s = &p->slot[p->get];
    if (s->state != READY_SLOT) { return NULL; }
    s->state = HELD_SLOT;
    return &s->value;
}

static void pair_release(pair_t *p)
{
    assert(p->slot[p->get].state == HELD_SLOT);
    p->slot[p->get].state = FREE_SLOT;
    p->get ^= 1u;
}

static void test_snapshots(void)
{
    sequence_t s = {10u, {7u, 999u}};
    stamp_t out = {99u, 99u};
    assert(try_snapshot(&s, &out, 1) == 0);
    assert(out.sec == 99u && out.ms == 99u);
    assert(try_snapshot(&s, &out, 0) == 1);
    assert(out.sec == 8u && out.ms == 0u);
    s.seq = 13u;
    assert(try_snapshot(&s, &out, 0) == 0);
    s.seq = UINT32_MAX - 1u;
    publish(&s, out);
    assert(s.seq == 0u);
    assert(try_snapshot(&s, &out, 0) == 1);

    pair_t p = {{{{0u, 0u}, FREE_SLOT}, {{0u, 0u}, FREE_SLOT}}, 0u, 0u};
    const stamp_t first = {7u, 999u}, second = {8u, 0u}, third = {8u, 1u};
    assert(pair_publish(&p, first) == 1);
    const stamp_t *held = pair_acquire(&p);
    assert(held != NULL);
    assert(pair_publish(&p, second) == 1);
    assert(pair_publish(&p, third) == 0);
    assert(held->sec == 7u && held->ms == 999u);
    pair_release(&p); /* Do not use held after release. */
    assert(pair_publish(&p, third) == 1);
    held = pair_acquire(&p);
    assert(held != NULL && held->sec == 8u && held->ms == 0u);
    pair_release(&p);
    held = pair_acquire(&p);
    assert(held != NULL && held->ms == 1u);
    pair_release(&p);
    assert(pair_acquire(&p) == NULL);
    puts("snapshot: torn rejected, stable accepted; held buffer protected");
}

typedef struct { unsigned high_start, high_finish, low_finish; } schedule_t;

static schedule_t simulate_schedule(int preempt)
{
    unsigned low = 8u, high = 1u, running_low = 0u;
    schedule_t result = {0u, 0u, 0u};
    for (unsigned now = 0u; low || high; ++now) {
        int high_ready = now >= 2u && high != 0u;
        if (high_ready && (preempt || !running_low)) {
            result.high_start = now;
            --high;
            result.high_finish = now + 1u;
        } else {
            assert(low > 0u);
            running_low = 1u;
            --low;
            if (low == 0u) {
                running_low = 0u;
                result.low_finish = now + 1u;
            }
        }
    }
    return result;
}

static void test_schedule(void)
{
    schedule_t coop = simulate_schedule(0);
    schedule_t pre = simulate_schedule(1);
    assert(coop.high_start == 8u && coop.high_finish == 9u && coop.low_finish == 8u);
    assert(pre.high_start == 2u && pre.high_finish == 3u && pre.low_finish == 9u);
    assert(coop.high_finish > 5u && pre.high_finish <= 5u);
    puts("schedule: cooperative H=8..9; preemptive H=2..3");
}

int main(void)
{
    test_rmw();
    test_nvic();
    test_vector();
    test_storm();
    test_exclusive();
    test_ordering();
    test_snapshots();
    test_schedule();
    puts("core: all deterministic assertions passed");
    return 0;
}
