#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if CHAR_BIT != 8 || !defined(__GNUC__)
#error This example requires 8-bit bytes and GCC-compatible packed attributes.
#endif

struct __attribute__((packed)) Packet {
    uint8_t tag;
    uint32_t value;
};

struct Flags {
    unsigned int ready : 1;
    unsigned int mode : 3;
};

enum Kind { COUNT, RATIO };
struct Value {
    enum Kind kind;
    union {
        uint32_t count;
        float ratio;
    } data;
};

int main(void)
{
    struct Packet p = {3, UINT32_C(0x12345678)};
    uint32_t copy;
    memcpy(&copy, (const unsigned char *)&p + offsetof(struct Packet, value),
           sizeof copy);
    assert(offsetof(struct Packet, tag) == 0);
    assert(sizeof p >= offsetof(struct Packet, value) + sizeof p.value);
    assert(copy == UINT32_C(0x12345678));
    printf("packed size=%zu value_offset=%zu\n", sizeof p,
           offsetof(struct Packet, value));

    struct Flags flags = {1, 5};
    unsigned char wire = (unsigned char)((flags.mode << 1) | flags.ready);
    assert(wire == 0x0B && ((wire >> 1) & 7u) == 5);
    printf("ready=%u mode=%u wire=%02X\n",
           (unsigned)flags.ready, (unsigned)flags.mode, (unsigned)wire);

    struct Value v = {.kind = COUNT, .data = {.count = 27}};
    assert(v.kind == COUNT && v.data.count == 27);
    printf("tagged count=%lu\n", (unsigned long)v.data.count);
    return 0;
}
