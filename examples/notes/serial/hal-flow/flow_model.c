/* C99, serialized ownership/backpressure model; not USB middleware. */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

enum { SLOT_COUNT = 2, BLOCK_SIZE = 4, RX_CAPACITY = 16, HIGH = 12, LOW = 4 };
typedef enum { TX_OK, TX_BUSY } TxResult;
typedef struct {
    bool busy;
    const char *buffer;
    char wire[32];
    size_t used;
} Driver;
typedef struct {
    char slot[SLOT_COUNT][BLOCK_SIZE];
    unsigned head;
    unsigned count;
    bool inflight;
} Queue;

static TxResult submit(Driver *d, const char *buffer)
{
    if (d->busy) return TX_BUSY;
    d->buffer = buffer;
    d->busy = true;
    return TX_OK;
}

static void complete(Driver *d)
{
    assert(d->busy && d->used + BLOCK_SIZE <= sizeof d->wire);
    memcpy(d->wire + d->used, d->buffer, BLOCK_SIZE);
    d->used += BLOCK_SIZE;
    d->busy = false;
    d->buffer = NULL;
}

static bool enqueue(Queue *q, const char *data)
{
    unsigned tail;
    if (q->count == SLOT_COUNT) return false;
    tail = (q->head + q->count) % SLOT_COUNT;
    memcpy(q->slot[tail], data, BLOCK_SIZE);
    ++q->count;
    return true;
}

static void pump(Queue *q, Driver *d)
{
    if (q->count == 0u || q->inflight) return;
    if (submit(d, q->slot[q->head]) == TX_OK) q->inflight = true;
}

static void completion_event(Queue *q, Driver *d)
{
    assert(q->inflight && q->count != 0u);
    complete(d);
    q->head = (q->head + 1u) % SLOT_COUNT;
    --q->count;
    q->inflight = false;
}

static void test_transmit(void)
{
    Driver bad = {0};
    Driver good = {0};
    Queue q = {0};
    char caller[BLOCK_SIZE] = {'A', 'B', 'C', 'D'};
    assert(submit(&bad, caller) == TX_OK);
    caller[0] = 'X';
    complete(&bad);
    assert(memcmp(bad.wire, "XBCD", BLOCK_SIZE) == 0);
    puts("flow: borrowed buffer changed early -> XBCD (model counterexample)");

    caller[0] = 'A';
    assert(enqueue(&q, caller));
    caller[0] = 'X';
    assert(enqueue(&q, "EFGH"));
    assert(!enqueue(&q, "IJKL"));
    pump(&q, &good);
    assert(submit(&good, "IJKL") == TX_BUSY);
    assert(good.buffer == q.slot[q.head]);
    pump(&q, &good);
    assert(q.count == 2u && good.used == 0u);
    completion_event(&q, &good);
    assert(enqueue(&q, "IJKL"));
    pump(&q, &good);
    completion_event(&q, &good);
    pump(&q, &good);
    completion_event(&q, &good);
    assert(q.count == 0u && !good.busy && !q.inflight);
    assert(good.used == 12u && memcmp(good.wire, "ABCDEFGHIJKL", 12u) == 0);
    puts("flow: FULL retains producer data; BUSY retains queue head");
    puts("flow: completed bytes = ABCDEFGHIJKL");
}

static unsigned test_watermarks(bool backpressure)
{
    unsigned ring[RX_CAPACITY] = {0};
    unsigned head = 0u, count = 0u, next = 0u, consumed = 0u;
    unsigned dropped = 0u, maximum = 0u, tail_budget = 0u;
    unsigned pauses = 0u, resumes = 0u, ticks = 0u;
    bool running = true;
    while (next < 40u || count != 0u) {
        assert(++ticks < 1000u);
        if (next < 40u && (running || tail_budget != 0u)) {
            if (!running) --tail_budget;
            if (count == RX_CAPACITY) ++dropped;
            else {
                ring[(head + count) % RX_CAPACITY] = next;
                ++count;
                if (count > maximum) maximum = count;
            }
            ++next;
        }
        if (backpressure && running && count >= HIGH) {
            running = false;
            tail_budget = 3u;
            ++pauses;
        }
        if (ticks % 3u == 0u && count != 0u) {
            if (backpressure) assert(ring[head] == consumed);
            ++consumed;
            head = (head + 1u) % RX_CAPACITY;
            --count;
        }
        if (backpressure && !running && count <= LOW) {
            assert(tail_budget == 0u);
            running = true;
            ++resumes;
        }
    }
    assert(next == consumed + dropped);
    if (backpressure) {
        assert(dropped == 0u && consumed == 40u && maximum <= HIGH + 3u);
        assert(pauses > 0u && resumes > 0u);
    } else assert(dropped > 0u);
    printf("flow: watermarks=%s consumed=%u dropped=%u peak=%u\n",
           backpressure ? "on" : "off", consumed, dropped, maximum);
    return dropped;
}

int main(void)
{
    test_transmit();
    assert(test_watermarks(false) > 0u);
    assert(test_watermarks(true) == 0u);
    puts("flow model: PASS");
    return 0;
}
