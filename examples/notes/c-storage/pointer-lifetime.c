#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int *data;
    size_t count;
} Buffer;

/* Call only on an empty, initialized owner. */
static int buffer_create(Buffer *b, size_t n)
{
    if (n == 0) {
        return 1;
    }
    if (n > SIZE_MAX / sizeof *b->data) {
        return 0;
    }
    int *p = malloc(n * sizeof *p);
    if (p == NULL) {
        return 0;
    }
    for (size_t i = 0; i < n; ++i) {
        p[i] = 0;
    }
    b->data = p;
    b->count = n;
    return 1;
}

static void buffer_destroy(Buffer *b)
{
    free(b->data);
    b->data = NULL;
    b->count = 0;
}

int main(void)
{
    Buffer owner = {NULL, 0};
    assert(buffer_create(&owner, 0));
    if (!buffer_create(&owner, 3)) {
        fputs("allocation failed\n", stderr);
        return EXIT_FAILURE;
    }
    int *borrow = owner.data;
    borrow[1] = 42;
    assert(owner.count == 3 && owner.data[0] == 0 && owner.data[1] == 42);
    printf("count=%zu value=%d\n", owner.count, borrow[1]);
    borrow = NULL;
    buffer_destroy(&owner);
    assert(borrow == NULL && owner.data == NULL && owner.count == 0);
    buffer_destroy(&owner);
    puts("owner=empty borrow=null destroy_twice=ok");
    return EXIT_SUCCESS;
}
