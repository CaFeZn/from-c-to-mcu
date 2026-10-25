#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if CHAR_BIT != 8
#error This protocol uses 8-bit bytes.
#endif

static uint32_t read_be32(const unsigned char p[4])
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint32_t read_le32(const unsigned char p[4])
{
    return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[1] << 8) | (uint32_t)p[0];
}

static void write_be32(unsigned char p[4], uint32_t value)
{
    p[0] = (unsigned char)(value >> 24);
    p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8);
    p[3] = (unsigned char)value;
}

static void write_le32(unsigned char p[4], uint32_t value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

static void show(const char *label, const unsigned char p[4])
{
    printf("%s: %02X %02X %02X %02X\n", label,
           (unsigned)p[0], (unsigned)p[1], (unsigned)p[2], (unsigned)p[3]);
}

int main(void)
{
    const unsigned char wire[4] = {0x12, 0x34, 0x56, 0x78};
    uint32_t value = read_be32(wire);
    unsigned char be[4], le[4], host[4];
    assert(value == UINT32_C(0x12345678));
    write_be32(be, value);
    write_le32(le, value);
    memcpy(host, &value, sizeof value);
    assert(memcmp(be, wire, sizeof be) == 0);
    printf("value=%08" PRIX32 "\n", value);
    show("BE", be);
    show("LE", le);
    show("host", host);

    const uint32_t cases[] = {0, 1, UINT32_C(0x12345678),
                             UINT32_C(0x80000000), UINT32_MAX};
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        write_be32(be, cases[i]);
        write_le32(le, cases[i]);
        assert(read_be32(be) == cases[i]);
        assert(read_le32(le) == cases[i]);
    }
    puts("roundtrip=5 cases ok");
    return 0;
}
