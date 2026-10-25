#include "crc.h"

#include <assert.h>
#include <limits.h>

#if CHAR_BIT != 8
#error "This example requires 8-bit bytes."
#endif

uint16_t crc16_ccitt_false_update(uint16_t state, const uint8_t *data, size_t length)
{
    uint32_t crc = state;
    size_t i;
    assert(data != NULL || length == 0u);
    for (i = 0u; i < length; ++i) {
        unsigned bit;
        crc ^= (uint32_t)data[i] << 8;
        for (bit = 0u; bit < 8u; ++bit) {
            crc = (crc & UINT32_C(0x8000)) != 0u
                ? (crc << 1) ^ UINT32_C(0x1021) : crc << 1;
            crc &= UINT32_C(0xFFFF);
        }
    }
    return (uint16_t)crc;
}

uint16_t crc16_ccitt_false(const uint8_t *data, size_t length)
{
    return crc16_ccitt_false_update(CRC16_CCITT_FALSE_INIT, data, length);
}

uint32_t crc32_iso_update_bitwise(uint32_t state, const uint8_t *data, size_t length)
{
    size_t i;
    assert(data != NULL || length == 0u);
    for (i = 0u; i < length; ++i) {
        unsigned bit;
        state ^= data[i];
        for (bit = 0u; bit < 8u; ++bit) {
            state = (state & UINT32_C(1)) != 0u
                  ? (state >> 1) ^ UINT32_C(0xEDB88320) : state >> 1;
        }
    }
    return state;
}

void crc32_make_table(uint32_t table[256])
{
    unsigned i;
    assert(table != NULL);
    for (i = 0u; i < 256u; ++i) {
        uint32_t value = (uint32_t)i;
        unsigned bit;
        for (bit = 0u; bit < 8u; ++bit) {
            value = (value & UINT32_C(1)) != 0u
                  ? (value >> 1) ^ UINT32_C(0xEDB88320) : value >> 1;
        }
        table[i] = value;
    }
}

uint32_t crc32_iso_update_table(uint32_t state, const uint8_t *data, size_t length,
                               const uint32_t table[256])
{
    size_t i;
    assert(data != NULL || length == 0u);
    assert(table != NULL);
    for (i = 0u; i < length; ++i) {
        unsigned index = (unsigned)((state ^ data[i]) & UINT32_C(0xFF));
        state = (state >> 8) ^ table[index];
    }
    return state;
}

uint32_t crc32_iso_finish(uint32_t state)
{
    return state ^ UINT32_C(0xFFFFFFFF);
}

uint32_t crc32_iso(const uint8_t *data, size_t length)
{
    return crc32_iso_finish(crc32_iso_update_bitwise(CRC32_ISO_INIT, data, length));
}

uint32_t crc32_reverse_bits(uint32_t value)
{
    uint32_t reversed = 0u;
    unsigned bit;
    for (bit = 0u; bit < 32u; ++bit) {
        reversed = (reversed << 1) | (value & UINT32_C(1));
        value >>= 1;
    }
    return reversed;
}

uint32_t crc32_f407_model_word(uint32_t state, uint32_t word)
{
    unsigned bit;
    state ^= word;
    for (bit = 0u; bit < 32u; ++bit) {
        state = (state & UINT32_C(0x80000000)) != 0u
              ? (state << 1) ^ UINT32_C(0x04C11DB7) : state << 1;
    }
    return state;
}

uint32_t crc32_iso_via_f407_model(const uint8_t *data, size_t length)
{
    uint32_t hardware_state = CRC32_ISO_INIT;
    uint32_t reflected_state;
    assert(data != NULL || length == 0u);
    while (length >= 4u) {
        uint32_t little_endian_word = (uint32_t)data[0]
                                   | (uint32_t)data[1] << 8
                                   | (uint32_t)data[2] << 16
                                   | (uint32_t)data[3] << 24;
        hardware_state = crc32_f407_model_word(hardware_state,
                            crc32_reverse_bits(little_endian_word));
        data += 4;
        length -= 4u;
    }
    /* Carry the raw state into a bytewise tail; never pad the message. */
    reflected_state = crc32_reverse_bits(hardware_state);
    reflected_state = crc32_iso_update_bitwise(reflected_state, data, length);
    return crc32_iso_finish(reflected_state);
}
