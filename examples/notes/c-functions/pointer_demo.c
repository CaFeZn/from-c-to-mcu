#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef int (*relation_fn)(int, int);

static int order(int a, int b)
{
    return (a > b) - (a < b);
}

static int call_order(relation_fn fn, int a, int b, int *out)
{
    if (fn == NULL || out == NULL) {
        return -1;
    }
    *out = fn(a, b);
    return 0;
}

static int order_elements(const void *pa, const void *pb)
{
    const int *a = pa;
    const int *b = pb;
    return order(*a, *b);
}

int main(void)
{
    relation_fn fn = order;
    int result = 99;
    int rc = call_order(fn, 7, 9, &result);
    int data[] = {9, -7, 0, 9, -2};
    const int expected[] = {-7, -2, 0, 9, 9};
    const size_t n = sizeof data / sizeof data[0];

    assert(rc == 0 && result == -1);
    assert(fn == &order);
    assert((*fn)(9, 9) == 0);
    printf("direct=%d indirect=%d\n", order(7, 9), result);

    qsort(data, n, sizeof data[0], order_elements);
    for (size_t i = 0; i < n; ++i) {
        assert(data[i] == expected[i]);
    }
    printf("sorted=%d,%d,%d,%d,%d\n",
           data[0], data[1], data[2], data[3], data[4]);

    assert(order(INT_MIN, INT_MAX) == -1);
    assert(order(INT_MAX, INT_MIN) == 1);
    rc = call_order(NULL, 1, 2, &result);
    assert(rc == -1 && result == -1);
    rc = call_order(fn, 1, 2, NULL);
    assert(rc == -1);
    puts("edge checks: OK");
    return 0;
}
