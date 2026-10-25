#include <assert.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    int (*set)(void *ctx, unsigned on);
    int (*read)(const void *ctx, unsigned *out);
} LedOps;

typedef struct {
    const LedOps *ops;
    void *ctx;
} Led;

typedef struct { unsigned on; } Cell;
typedef struct { unsigned pin_high; } ActiveLow;

static int cell_set(void *ctx, unsigned on)
{
    Cell *self = ctx;
    self->on = on;
    return 1;
}

static int cell_read(const void *ctx, unsigned *out)
{
    const Cell *self = ctx;
    *out = self->on;
    return 1;
}

static int low_set(void *ctx, unsigned on)
{
    ActiveLow *self = ctx;
    self->pin_high = 1u - on;
    return 1;
}

static int low_read(const void *ctx, unsigned *out)
{
    const ActiveLow *self = ctx;
    *out = 1u - self->pin_high;
    return 1;
}

static const LedOps cell_ops = { .set = cell_set, .read = cell_read };
static const LedOps low_ops = { .set = low_set, .read = low_read };

static Led cell_bind(Cell *state)
{
    Led result = { &cell_ops, state };
    return result;
}

static Led low_bind(ActiveLow *state)
{
    Led result = { &low_ops, state };
    return result;
}

static int led_set(Led *self, unsigned on)
{
    if (self == NULL || self->ctx == NULL || self->ops == NULL ||
        self->ops->set == NULL || on > 1u) {
        return 0;
    }
    return self->ops->set(self->ctx, on);
}

static int led_read(const Led *self, unsigned *out)
{
    if (self == NULL || self->ctx == NULL || self->ops == NULL ||
        self->ops->read == NULL || out == NULL) {
        return 0;
    }
    return self->ops->read(self->ctx, out);
}

int main(void)
{
    Cell a_state = { 0u }, b_state = { 0u };
    ActiveLow c_state = { 1u };
    Led a = cell_bind(&a_state);
    Led b = cell_bind(&b_state);
    Led c = low_bind(&c_state);
    int r1 = led_set(&a, 1u);
    int r2 = led_set(&b, 0u);
    int r3 = led_set(&c, 1u);
    assert(r1 == 1 && r2 == 1 && r3 == 1);
    unsigned av = 99u, bv = 99u, cv = 99u;
    r1 = led_read(&a, &av);
    r2 = led_read(&b, &bv);
    r3 = led_read(&c, &cv);
    assert(r1 == 1 && r2 == 1 && r3 == 1);
    assert(av == 1u && bv == 0u && cv == 1u && c_state.pin_high == 0u);
    assert(a.ops == b.ops && a.ctx != b.ctx);
    printf("same_ops=%d different_ctx=%d\n", a.ops == b.ops, a.ctx != b.ctx);
    printf("logical=%u,%u,%u raw_C=%u\n", av, bv, cv, c_state.pin_high);

    r1 = led_set(&a, 2u);
    assert(r1 == 0 && a_state.on == 1u);
    r2 = led_read(&a, NULL);
    r3 = led_set(NULL, 1u);
    assert(r2 == 0 && r3 == 0);
    Led missing = { NULL, &a_state };
    r1 = led_set(&missing, 1u);
    assert(r1 == 0);
    const LedOps empty_ops = { NULL, NULL };
    missing.ops = &empty_ops;
    r1 = led_set(&missing, 1u);
    unsigned untouched = 99u;
    r2 = led_read(&missing, &untouched);
    assert(r1 == 0 && r2 == 0 && untouched == 99u);
    missing = cell_bind(NULL);
    r1 = led_set(&missing, 1u);
    assert(r1 == 0);
    puts("invalid requests rejected");
    return 0;
}
