#include <assert.h>
#include <stddef.h>
#include <stdio.h>

/* The caller supplies n readable elements; every partial sum must fit in int. */
static int sum(const int *a, size_t n)
{
    int result = 0;
    for (size_t i = 0; i < n; ++i) {
        result += a[i];
    }
    return result;
}

int main(void)
{
    int a[4] = {10, 20, 30, 40};
    const size_t n = sizeof a / sizeof a[0];
    int *p = a;
    int (*whole)[4] = &a;
    ptrdiff_t distance = (p + 3) - p;
    assert(sizeof *whole == sizeof a);
    assert((*whole)[2] == 30 && p[2] == *(p + 2));
    assert(distance == 3 && sum(a, n) == 100);
    assert(sum(NULL, 0) == 0);
    printf("count=%zu sum=%d distance=%td\n", n, sum(a, n), distance);

    size_t visited = 0;
    for (const int *it = a; it != a + n; ++it) {
        assert(*it == a[visited]);
        ++visited;
    }
    assert(visited == n);
    int matrix[2][3] = {{1, 2, 3}, {4, 5, 6}};
    int (*row)[3] = matrix;
    assert(row[1][2] == 6);
    printf("visited=%zu matrix[1][2]=%d\n", visited, row[1][2]);
    printf("array_bytes=%zu pointer_bytes=%zu row_bytes=%zu\n",
           sizeof a, sizeof p, sizeof *row);
    return 0;
}
