#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef enum { IDLE, RECORDING, FAULT } state_t;
typedef enum { START, SAMPLE, RESET } event_t;
typedef bool (*store_fn)(void *ctx, unsigned value);

typedef struct {
    state_t state;
    store_fn store;
    void *ctx;
} recorder_t;

/* The caller owns ctx, and it must outlive every synchronous step call. */
static bool step(recorder_t *r, event_t event, unsigned value)
{
    if (r == NULL || r->store == NULL || r->ctx == NULL) {
        return false;
    }
    switch (r->state) {
    case IDLE:
        if (event == START) {
            r->state = RECORDING;
            return true;
        }
        return false;
    case RECORDING:
        if (event == RESET) {
            r->state = IDLE;
            return true;
        }
        if (event == SAMPLE && value <= 100u) {
            if (!r->store(r->ctx, value)) {
                r->state = FAULT;
                return false;
            }
            return true;
        }
        return false;
    case FAULT:
        if (event == RESET) {
            r->state = IDLE;
            return true;
        }
        return false;
    default:
        return false;
    }
}

typedef struct {
    unsigned data[2];
    size_t used;
} fake_store_t;

static bool save_to_fake(void *ctx, unsigned value)
{
    fake_store_t *s = ctx;
    if (s->used >= sizeof s->data / sizeof s->data[0]) {
        return false;
    }
    s->data[s->used++] = value;
    return true;
}

int main(void)
{
    fake_store_t sink = {{0, 0}, 0};
    recorder_t r = {IDLE, save_to_fake, &sink};
    bool ok = step(&r, SAMPLE, 12u);
    assert(!ok && sink.used == 0);
    ok = step(&r, START, 0u);
    assert(ok && r.state == RECORDING);
    ok = step(&r, SAMPLE, 12u);
    assert(ok);
    ok = step(&r, SAMPLE, 34u);
    assert(ok && sink.used == 2);
    assert(sink.data[0] == 12u && sink.data[1] == 34u);
    ok = step(&r, SAMPLE, 101u);
    assert(!ok && r.state == RECORDING && sink.used == 2);
    ok = step(&r, SAMPLE, 56u);
    assert(!ok && r.state == FAULT && sink.used == 2);
    puts("stored=12,34 state=FAULT");
    ok = step(&r, START, 0u);
    assert(!ok && r.state == FAULT);
    ok = step(&r, RESET, 0u);
    assert(ok && r.state == IDLE && sink.used == 2);
    puts("reset=IDLE used=2");
    puts("pattern checks: OK");
    return 0;
}
