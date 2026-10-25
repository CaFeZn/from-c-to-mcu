#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef struct { uint32_t value; int tagged; } model_t;

static uint32_t load_exclusive(model_t *m)
{
    m->tagged = 1;
    return m->value;
}

static unsigned store_exclusive(model_t *m, uint32_t next)
{
    unsigned failed = m->tagged ? 0u : 1u;
    m->tagged = 0;
    if (failed == 0u) { m->value = next; }
    return failed;
}

int main(void)
{
    model_t m = {10u, 0};
    uint32_t old = load_exclusive(&m);
    m.tagged = 0;              /* model exception entry */
    ++m.value;                /* model ISR update */
    assert(store_exclusive(&m, old + 1u) == 1u);
    assert(m.value == 11u);
    old = load_exclusive(&m);
    assert(store_exclusive(&m, old + 1u) == 0u);
    assert(m.value == 12u);
    puts("first=fail, retry=success, value=12");
    return 0;
}
