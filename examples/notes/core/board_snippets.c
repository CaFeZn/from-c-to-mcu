/* F407/CMSIS compile probes only: not a complete firmware and not host code.
 * Integrate only selected functions into an initialized, privileged board project.
 * EXTI9_5_IRQHandler must replace, not duplicate, the generated handler.
 * Do not use this file together with HAL EXTI dispatch for the same sources.
 */
#include "stm32f407xx.h"

void count_add(volatile uint32_t *count)
{
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    *count = *count + 1u;
    __set_PRIMASK(saved);
}

void configure_exti_priority(void)
{
    NVIC_SetPriorityGrouping(3u);
    uint32_t p = NVIC_EncodePriority(3u, 5u, 0u);
    NVIC_SetPriority(EXTI0_IRQn, p);
}

void gpio_set_pa5(void)
{
    GPIOA->BSRR = 1u << 5;
}

void gpio_reset_pa5(void)
{
    GPIOA->BSRR = 1u << (5u + 16u);
}

static volatile uint32_t line5_count;
static volatile uint32_t line7_count;

void EXTI9_5_IRQHandler(void)
{
    const uint32_t owned = (1u << 5) | (1u << 7);
    uint32_t seen = EXTI->PR & EXTI->IMR & owned;
    EXTI->PR = seen;
    if ((seen & (1u << 5)) != 0u) { ++line5_count; }
    if ((seen & (1u << 7)) != 0u) { ++line7_count; }
}

uint32_t fetch_inc_m4(volatile uint32_t *p)
{
    uint32_t old;
    do {
        old = __LDREXW(p);
    } while (__STREXW(old + 1u, p) != 0u);
    return old;
}

typedef struct {
    uint32_t word[2];
    volatile uint32_t ready;
} publication_t;

void publish_once(publication_t *p)
{
    p->word[0] = 10u;
    p->word[1] = 20u;
    __DMB();
    p->ready = 1u;
}

int receive_once(const publication_t *p, uint32_t out[2])
{
    if (p->ready == 0u) { return 0; }
    __DMB();
    out[0] = p->word[0];
    out[1] = p->word[1];
    return 1;
}

void compiler_boundary_only(void)
{
    __COMPILER_BARRIER();
}

void complete_accesses(void)
{
    __DSB();
}

void synchronize_instructions(void)
{
    __ISB();
}

typedef struct {
    volatile uint32_t seq;
    volatile uint32_t sec;
    volatile uint32_t ms;
} shared_stamp_t;

typedef struct { uint32_t sec, ms; } stamp_t;

void stamp_publish_isr(shared_stamp_t *s, uint32_t sec, uint32_t ms)
{
    ++s->seq;
    __DMB();
    s->sec = sec;
    s->ms = ms;
    __DMB();
    ++s->seq;
}

int stamp_try_read(const shared_stamp_t *s, stamp_t *out)
{
    uint32_t first = s->seq;
    if ((first & 1u) != 0u) { return 0; }
    __DMB();
    uint32_t sec = s->sec;
    uint32_t ms = s->ms;
    __DMB();
    if (first != s->seq) { return 0; }
    out->sec = sec;
    out->ms = ms;
    return 1;
}

typedef void (*step_fn)(void *context);
typedef struct { step_fn step; void *context; } task_t;

void run_one_round(task_t tasks[], unsigned count)
{
    for (unsigned i = 0; i < count; ++i) {
        tasks[i].step(tasks[i].context);
    }
}
