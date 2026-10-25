#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    unsigned value;
    unsigned limit;
} Counter;

static Counter counter_make(unsigned limit)
{
    Counter result = { 0u, limit };
    return result;
}

/* A failed update leaves the object unchanged. */
static int counter_add(Counter *self, unsigned amount)
{
    if (self == NULL || self->value > self->limit ||
        amount > self->limit - self->value) {
        return 0;
    }
    self->value += amount;
    return 1;
}

/* Precondition: self points to a live, initialized Counter. */
static unsigned counter_value(const Counter *self)
{
    return self->value;
}

int main(void)
{
    Counter a = counter_make(5u);
    Counter b = counter_make(10u);
    int r1 = counter_add(&a, 3u);
    int r2 = counter_add(&b, 8u);
    int r3 = counter_add(&a, 4u);
    assert(r1 == 1 && r2 == 1 && r3 == 0);
    assert(counter_value(&a) == 3u && counter_value(&b) == 8u);
    printf("A=%u B=%u accepted=%d,%d,%d\n",
           counter_value(&a), counter_value(&b), r1, r2, r3);

    Counter snapshot = a;
    Counter *alias = &a;
    r1 = counter_add(&a, 1u);
    r2 = counter_add(alias, 1u);
    assert(r1 == 1 && r2 == 1);
    assert(a.value == 5u && b.value == 8u && snapshot.value == 3u);
    printf("A=%u snapshot=%u alias=%u\n",
           counter_value(&a), counter_value(&snapshot), counter_value(alias));

    Counter edge = counter_make(UINT_MAX);
    r1 = counter_add(&edge, UINT_MAX);
    r2 = counter_add(&edge, 1u);
    r3 = counter_add(&edge, 0u);
    assert(r1 == 1 && r2 == 0 && r3 == 1);
    assert(edge.value == UINT_MAX);
    r1 = counter_add(NULL, 1u);
    assert(r1 == 0);
    Counter zero = counter_make(0u);
    r1 = counter_add(&zero, 0u);
    r2 = counter_add(&zero, 1u);
    assert(r1 == 1 && r2 == 0 && zero.value == 0u);
    puts("boundary checks passed");
    return 0;
}
