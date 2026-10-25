#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static void scale_copy(size_t n, unsigned *restrict dst,
                       const unsigned *restrict src)
{
    for (size_t i = 0; i < n; ++i) {
        dst[i] = 2u * src[i];
    }
}

static void add_vectors(size_t n, unsigned *restrict dst,
                        const unsigned *restrict left,
                        const unsigned *restrict right)
{
    for (size_t i = 0; i < n; ++i) {
        dst[i] = left[i] + right[i];
    }
}

int main(void)
{
    int x = 3;
    int y = 4;
    const int *view = &x;
    int *const fixed = &x;
    *fixed = 9;
    assert(*view == 9);
    view = &y;
    assert(*view == 4);

    unsigned data[6] = {1, 2, 3, 0, 0, 0};
    scale_copy(3, data + 3, data);
    assert(data[3] == 2 && data[4] == 4 && data[5] == 6);

    unsigned result[3] = {0};
    add_vectors(3, result, data, data);
    assert(result[0] == 2 && result[1] == 4 && result[2] == 6);

    unsigned moved[4] = {1, 2, 3, 4};
    memmove(moved + 1, moved, 3 * sizeof moved[0]);
    assert(moved[0] == 1 && moved[1] == 1);
    assert(moved[2] == 2 && moved[3] == 3);
    puts("view=4 scaled=2,4,6 shared-input=2,4,6 moved=1,1,2,3");
    return 0;
}
