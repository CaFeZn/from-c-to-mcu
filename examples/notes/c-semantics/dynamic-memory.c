#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void *(*resize_fn)(void *, size_t);

/* p/n describe one owned allocation; zero means release. */
static int resize_ints(int **p, size_t *n, size_t want, resize_fn resize)
{
    int *next;
    size_t i;

    if (want == 0) {
        free(*p);
        *p = NULL;
        *n = 0;
        return 1;
    }
    if (want > SIZE_MAX / sizeof **p) {
        return 0;
    }
    next = resize(*p, want * sizeof **p);
    if (next == NULL) {
        return 0;
    }
    for (i = *n; i < want; ++i) {
        next[i] = 0;
    }
    *p = next;
    *n = want;
    return 1;
}

static unsigned denied_calls;

/* Test double: nonzero realloc failure leaves the old block alone. */
static void *deny_resize(void *p, size_t bytes)
{
    (void)p;
    assert(bytes != 0);
    (void)bytes;
    ++denied_calls;
    return NULL;
}

struct free_stats {
    size_t total;
    size_t largest;
};

/* Occupancy model only, not an allocator or usable object storage. */
static struct free_stats measure(const unsigned char *used, size_t n)
{
    struct free_stats result = {0, 0};
    size_t run = 0;
    size_t i;

    for (i = 0; i < n; ++i) {
        if (used[i] == 0) {
            ++result.total;
            ++run;
            if (run > result.largest) {
                result.largest = run;
            }
        } else {
            run = 0;
        }
    }
    return result;
}

int main(void)
{
    int *p = NULL;
    int *saved;
    size_t n = 0;
    int ok;
    unsigned char used[32];
    struct free_stats stats;

    if (!resize_ints(&p, &n, 3, realloc)) {
        fputs("initial allocation failed\n", stderr);
        return EXIT_FAILURE;
    }
    assert(n == 3 && p[0] == 0 && p[2] == 0);
    p[0] = 10;
    p[1] = 20;
    p[2] = 30;
    saved = p;
    ok = resize_ints(&p, &n, 6, deny_resize);
    assert(!ok && p == saved && n == 3 && p[2] == 30);
    assert(denied_calls == 1);
    if (sizeof *p > 1) {
        size_t too_many = SIZE_MAX / sizeof *p + 1;
        ok = resize_ints(&p, &n, too_many, deny_resize);
        assert(!ok && p == saved && n == 3 && p[2] == 30);
        assert(denied_calls == 1);
    }
    puts("failure: original allocation preserved");

    if (!resize_ints(&p, &n, 6, realloc)) {
        free(p);
        fputs("growth failed\n", stderr);
        return EXIT_FAILURE;
    }
    assert(n == 6 && p[0] == 10 && p[1] == 20 && p[2] == 30);
    assert(p[3] == 0 && p[4] == 0 && p[5] == 0);
    printf("growth: kept=%d added=%d\n", p[2], p[5]);
    if (!resize_ints(&p, &n, 2, realloc)) {
        free(p);
        fputs("shrink failed\n", stderr);
        return EXIT_FAILURE;
    }
    assert(n == 2 && p[0] == 10 && p[1] == 20);
    ok = resize_ints(&p, &n, 0, realloc);
    assert(ok && p == NULL && n == 0);
    ok = resize_ints(&p, &n, 0, realloc);
    assert(ok && p == NULL && n == 0);
    puts("release: pointer=NULL count=0");

    memset(used, 1, sizeof used);
    stats = measure(used, sizeof used);
    assert(stats.total == 0 && stats.largest == 0);
    memset(used, 0, 8);
    memset(used + 16, 0, 8);
    stats = measure(used, sizeof used);
    assert(stats.total == 16 && stats.largest == 8);
    assert(stats.total >= 12 && stats.largest < 12);
    printf("model: free=%u largest=%u request=12 rejected\n",
           (unsigned)stats.total, (unsigned)stats.largest);
    memset(used + 8, 0, 8);
    stats = measure(used, sizeof used);
    assert(stats.total == 24 && stats.largest == 24);
    printf("model: free=%u largest=%u request=12 fits\n",
           (unsigned)stats.total, (unsigned)stats.largest);
    memset(used, 0, sizeof used);
    stats = measure(used, sizeof used);
    assert(stats.total == 32 && stats.largest == 32);
    stats = measure(used, 0);
    assert(stats.total == 0 && stats.largest == 0);
    puts("dynamic-memory: ok");
    return EXIT_SUCCESS;
}
