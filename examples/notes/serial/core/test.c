#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Sequential C99 model. Concurrent firmware needs its own synchronization. */
#define CAP 8u
typedef struct { uint8_t data[CAP]; uint32_t head, tail; } ring_t;
static bool ring_put(ring_t *r, uint8_t b)
{
    if ((uint32_t)(r->head - r->tail) == CAP) return false;
    r->data[r->head & (CAP - 1u)] = b;
    ++r->head;
    return true;
}
static bool ring_get(ring_t *r, uint8_t *out)
{
    if (r->head == r->tail) return false;
    *out = r->data[r->tail & (CAP - 1u)];
    ++r->tail;
    return true;
}

/* Exactly mv=<one to five ASCII digits> LF; range 0..65535. */
static bool parse_mv(const uint8_t *p, size_t n, uint16_t *out)
{
    uint32_t v = 0;
    if (n < 5u || n > 9u || p[0] != 'm' || p[1] != 'v' ||
        p[2] != '=' || p[n - 1u] != '\n') return false;
    for (size_t i = 3u; i + 1u < n; ++i) {
        if (p[i] < '0' || p[i] > '9') return false;
        uint32_t d = (uint32_t)(p[i] - '0');
        if (v > (UINT32_C(65535) - d) / 10u) return false;
        v = v * 10u + d;
    }
    *out = (uint16_t)v;
    return true;
}
static void put_u16_le(uint8_t p[2], uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static uint16_t get_u16_le(const uint8_t p[2])
{
    return (uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
}

typedef enum { Q_OPEN, Q_TEXT, Q_ESCAPE, Q_EOL, Q_DROP } qstate_t;
typedef enum { Q_MORE = 0, Q_DONE = 1, Q_ERROR = -1 } qresult_t;
typedef struct { qstate_t state; size_t n; char text[12]; } quoted_t;
static void q_init(quoted_t *q)
{
    q->state = Q_OPEN;
    q->n = 0;
    q->text[0] = '\0';
}
static void q_append(quoted_t *q, uint8_t b)
{
    if (q->n == sizeof q->text - 1u) { q->state = Q_DROP; return; }
    q->text[q->n++] = (char)b;
    q->text[q->n] = '\0';
}
/* One quoted printable-ASCII field per LF-terminated line; escape " and \\. */
static qresult_t q_feed(quoted_t *q, uint8_t b)
{
    if (b == '\n') {
        qresult_t result = q->state == Q_EOL ? Q_DONE : Q_ERROR;
        q->state = Q_OPEN;
        return result;
    }
    switch (q->state) {
    case Q_OPEN:
        q->n = 0; q->text[0] = '\0';
        q->state = b == '"' ? Q_TEXT : Q_DROP;
        break;
    case Q_TEXT:
        if (b == '"') q->state = Q_EOL;
        else if (b == '\\') q->state = Q_ESCAPE;
        else if (b < 0x20u || b > 0x7eu) q->state = Q_DROP;
        else q_append(q, b);
        break;
    case Q_ESCAPE:
        if (b != '"' && b != '\\') q->state = Q_DROP;
        else { q->state = Q_TEXT; q_append(q, b); }
        break;
    case Q_EOL: q->state = Q_DROP; break;
    case Q_DROP: break;
    }
    return Q_MORE;
}
static bool timeout_elapsed(uint32_t now, uint32_t then, uint32_t limit)
{
    return (uint32_t)(now - then) >= limit;
}

static void test_printf_and_uart(void)
{
    char full[16], small[8];
    const char *format = "mv=%u\n";
    int n = snprintf(full, sizeof full, format, 3300u);
    int m = snprintf(small, sizeof small, format, 3300u);
    assert(n == 8 && m == 8);
    assert(strcmp(full, "mv=3300\n") == 0);
    assert(strcmp(small, "mv=3300") == 0);
    assert((size_t)m >= sizeof small);
    /* Full framing model: idle is outside the returned ten bits. */
    const uint8_t expected[10] = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
    uint8_t frame[10] = {0};
    for (unsigned i = 0; i < 8u; ++i) frame[i + 1u] = (uint8_t)((0x55u >> i) & 1u);
    frame[9] = 1;
    assert(memcmp(frame, expected, sizeof frame) == 0);
    uint32_t brr = (UINT32_C(84000000) + 57600u) / 115200u;
    assert(brr == 0x2d9u);
    assert(115200u / 10u == 11520u);
    /* Upper rounded microseconds, exact integer model, not a timing measurement. */
    uint32_t us = (64u * 10u * 1000000u + 115200u - 1u) / 115200u;
    assert(us == 5556u);
    printf("format: need=%d stored=%u; uart: BRR=0x%03X, 64B=%luus-ceil\n",
           m, (unsigned)strlen(small), (unsigned)brr, (unsigned long)us);
}
static void test_ring(void)
{
    ring_t r = {{0}, 0, 0};
    uint8_t b = 0;
    assert(!ring_get(&r, &b));
    for (uint8_t i = 0; i < CAP; ++i) assert(ring_put(&r, i));
    assert(!ring_put(&r, 99));
    for (uint8_t i = 0; i < 3; ++i) { assert(ring_get(&r, &b)); assert(b == i); }
    for (uint8_t i = 8; i < 11; ++i) assert(ring_put(&r, i));
    for (uint8_t i = 3; i < 11; ++i) { assert(ring_get(&r, &b)); assert(b == i); }
    assert(!ring_get(&r, &b));
    r.head = UINT32_MAX - 2u; r.tail = r.head;
    for (uint8_t i = 0; i < CAP; ++i) assert(ring_put(&r, i));
    assert(r.head == 5u && (uint32_t)(r.head - r.tail) == CAP);
    assert(!ring_put(&r, 99));
    for (uint8_t i = 0; i < CAP; ++i) { assert(ring_get(&r, &b)); assert(b == i); }
    assert(r.head == r.tail);
    /* Bounded producer/consumer schedule, repeated array wraps. */
    unsigned sent = 0, received = 0;
    for (unsigned step = 0; received < 10000u; ++step) {
        if (sent < 10000u && step % 3u != 0u && ring_put(&r, (uint8_t)sent)) ++sent;
        if (step % 4u != 0u && ring_get(&r, &b)) { assert(b == (uint8_t)received); ++received; }
        assert((uint32_t)(r.head - r.tail) <= CAP);
    }
    puts("ring: empty/full/order/uint32-wrap/10000-byte schedule OK");
}
static void test_protocol(void)
{
    uint16_t value = 77;
    const uint8_t valid[] = "mv=3300\n";
    assert(parse_mv(valid, sizeof valid - 1u, &value) && value == 3300u);
    const char *bad[] = {"mv=\n", "mv=65536\n", "mv=-1\n", "mv=12x\n", "mv=1\r\n", "mv=1\nextra", "mv=123456\n"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        value = 77;
        assert(!parse_mv((const uint8_t *)bad[i], strlen(bad[i]), &value));
        assert(value == 77);
    }
    const uint8_t embedded_nul[] = {'m', 'v', '=', '1', 0, '\n'};
    assert(!parse_mv(embedded_nul, sizeof embedded_nul, &value));
    uint8_t wire[2];
    for (uint32_t v = 0; v <= UINT32_C(65535); ++v) {
        put_u16_le(wire, (uint16_t)v);
        assert(get_u16_le(wire) == v);
    }
    put_u16_le(wire, 3300u);
    assert(wire[0] == 0xe4u && wire[1] == 0x0cu);
    puts("protocol: text rejection/unchanged output/all 65536 LE values OK");
}
static qresult_t q_line(quoted_t *q, const char *s)
{
    qresult_t r = Q_MORE;
    while (*s != '\0') r = q_feed(q, (uint8_t)*s++);
    return r;
}
static void test_state_machine(void)
{
    quoted_t q, other;
    q_init(&q); q_init(&other);
    assert(q_line(&q, "\"hello world\"\n") == Q_DONE);
    assert(strcmp(q.text, "hello world") == 0);
    assert(q_line(&q, "\"a\\\"b\"\n") == Q_DONE);
    assert(strcmp(q.text, "a\"b") == 0);
    assert(q_line(&q, "\"\"\n") == Q_DONE && q.n == 0u);
    assert(q_line(&q, "\"abcdefghijkl\"\n") == Q_ERROR);
    assert(q_line(&q, "\"bad\\q\"\n") == Q_ERROR);
    assert(q_line(&q, "\"unfinished\n") == Q_ERROR);
    assert(q_line(&q, "\"x\"junk\n") == Q_ERROR);
    assert(q_line(&q, "\"ok\"\n") == Q_DONE && strcmp(q.text, "ok") == 0);
    const char *input = "\"a\\\"b\"\n";
    size_t n = strlen(input);
    for (size_t cut = 0; cut <= n; ++cut) {
        q_init(&q); unsigned done = 0;
        for (size_t i = 0; i < cut; ++i) done += q_feed(&q, (uint8_t)input[i]) == Q_DONE;
        for (size_t i = cut; i < n; ++i) done += q_feed(&q, (uint8_t)input[i]) == Q_DONE;
        assert(done == 1u && strcmp(q.text, "a\"b") == 0);
    }
    q_init(&q); q_init(&other);
    assert(q_line(&q, "\"first") == Q_MORE);
    assert(q_line(&other, "\"second\"\n") == Q_DONE);
    assert(q_line(&q, "\"\n") == Q_DONE && strcmp(q.text, "first") == 0);
    assert(strcmp(other.text, "second") == 0);
    assert(!timeout_elapsed(3u, UINT32_MAX - 5u, 10u));
    assert(timeout_elapsed(4u, UINT32_MAX - 5u, 10u));
    puts("state: quotes/escape/overflow/recovery/all chunk splits/two objects OK");
}
int main(void)
{
    test_printf_and_uart(); test_ring(); test_protocol(); test_state_machine();
    puts("serial core: all tests passed");
    return 0;
}
