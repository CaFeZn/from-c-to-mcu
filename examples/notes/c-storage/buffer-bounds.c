#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int append_bytes(unsigned char *dst, size_t cap, size_t *used,
                        const unsigned char *src, size_t n)
{
    if (used == NULL || *used > cap || n > cap - *used) {
        return 0;
    }
    if (n == 0) {
        return 1;
    }
    if (dst == NULL || src == NULL) {
        return 0;
    }
    memmove(dst + *used, src, n);
    *used += n;
    return 1;
}

int main(void)
{
    unsigned char storage[6] = {0xCC, 0, 0, 0, 0, 0xCC};
    unsigned char *payload = storage + 1;
    size_t used = 0;
    assert(append_bytes(payload, 4, &used, (const unsigned char *)"ABC", 3));
    assert(!append_bytes(payload, 4, &used, (const unsigned char *)"XY", 2));
    assert(used == 3 && payload[3] == 0);
    assert(append_bytes(payload, 4, &used, (const unsigned char *)"Z", 1));
    assert(!append_bytes(payload, 4, &used, payload, SIZE_MAX));
    assert(append_bytes(payload, 4, &used, NULL, 0));
    assert(used == 4 && memcmp(payload, "ABCZ", 4) == 0);
    assert(storage[0] == 0xCC && storage[5] == 0xCC);
    printf("used=%zu payload=%.*s guards=ok\n", used, 4, (const char *)payload);

    size_t invalid = 5;
    assert(!append_bytes(payload, 4, &invalid, payload, 0));
    size_t empty = 0;
    assert(append_bytes(NULL, 0, &empty, NULL, 0));
    assert(!append_bytes(NULL, 1, &empty, payload, 1));
    unsigned char overlap[6] = {'a', 'b', 'c', 'd', 0, 0};
    size_t overlap_used = 2;
    assert(append_bytes(overlap, sizeof overlap, &overlap_used, overlap, 4));
    assert(overlap_used == 6 && memcmp(overlap, "ababcd", 6) == 0);

    char text[4];
    int needed = snprintf(text, sizeof text, "%s", "ABCDE");
    assert(needed == 5 && strcmp(text, "ABC") == 0);
    assert(needed >= 0 && (size_t)needed >= sizeof text);
    printf("snprintf needed=%d stored=%s\n", needed, text);
    return 0;
}
