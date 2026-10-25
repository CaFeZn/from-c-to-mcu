#ifdef NDEBUG
#undef NDEBUG
#endif
#include "crc.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static const uint8_t check[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};

static void test_crc16(void)
{
    static const uint8_t frame_data[] = {0x01, 0x01, 0x07, 0x01};
    static const uint8_t damaged[] = {0x01, 0x01, 0x07, 0x00};
    static const uint8_t balanced[] = {0x01, 0x02, 0x06, 0x01};
    static const uint8_t zeros[4] = {0};
    uint8_t codeword[6];
    uint16_t crc = crc16_ccitt_false(frame_data, sizeof frame_data);
    size_t i;
    assert(crc16_ccitt_false(check, sizeof check) == UINT16_C(0x29B1));
    assert(crc16_ccitt_false(NULL, 0u) == CRC16_CCITT_FALSE_INIT);
    assert(crc16_ccitt_false(zeros, sizeof zeros) == UINT16_C(0x84C0));
    assert(crc == UINT16_C(0x4CF2));
    assert(crc16_ccitt_false(damaged, sizeof damaged) == UINT16_C(0x5CD3));
    assert(crc16_ccitt_false(balanced, sizeof balanced) != crc);
    assert(frame_data[0] + frame_data[1] + frame_data[2] + frame_data[3]
        == balanced[0] + balanced[1] + balanced[2] + balanced[3]);
    for (i = 0u; i <= sizeof check; ++i) {
        uint16_t state = crc16_ccitt_false_update(CRC16_CCITT_FALSE_INIT, check, i);
        state = crc16_ccitt_false_update(state, check + i, sizeof check - i);
        assert(state == UINT16_C(0x29B1));
    }
    for (i = 0u; i < sizeof frame_data * 8u; ++i) {
        uint8_t changed[sizeof frame_data];
        memcpy(changed, frame_data, sizeof changed);
        changed[i / 8u] ^= (uint8_t)(1u << (i % 8u));
        assert(crc16_ccitt_false(changed, sizeof changed) != crc);
    }
    memcpy(codeword, frame_data, sizeof frame_data);
    codeword[4] = (uint8_t)(crc >> 8);
    codeword[5] = (uint8_t)crc;
    assert(crc16_ccitt_false(codeword, sizeof codeword) == 0u);
    codeword[4] = (uint8_t)crc;
    codeword[5] = (uint8_t)(crc >> 8);
    assert(crc16_ccitt_false(codeword, sizeof codeword) != 0u);
    printf("CRC16 check=29B1 empty=FFFF frame=%04X damaged=5CD3\n", (unsigned)crc);
    printf("frame wire: A5 01 01 07 01 %02X %02X\n", (unsigned)codeword[4], (unsigned)codeword[5]);
    printf("same sum=0A: original=4CF2 balanced=%04X\n",
           (unsigned)crc16_ccitt_false(balanced, sizeof balanced));
}

static void test_crc32(void)
{
    uint32_t table[256];
    uint8_t data[257];
    static const uint8_t zeros[4] = {0};
    static const uint32_t states[] = {0u, 1u, UINT32_C(0x12345678), UINT32_C(0xFFFFFFFF)};
    size_t i;
    unsigned split_cases = 0u;
    crc32_make_table(table);
    assert(table[0] == 0u);
    assert(table[1] == UINT32_C(0x77073096));
    assert(table[255] == UINT32_C(0x2D02EF8D));
    assert(crc32_iso(check, sizeof check) == UINT32_C(0xCBF43926));
    assert(crc32_iso(NULL, 0u) == 0u);
    assert(crc32_iso(zeros, sizeof zeros) == UINT32_C(0x2144DF1C));
    assert(crc32_iso_via_f407_model(NULL, 0u) == 0u);
    assert(crc32_iso_via_f407_model(check, sizeof check) == UINT32_C(0xCBF43926));
    for (i = 0u; i < 256u; ++i) {
        uint8_t byte = (uint8_t)i;
        size_t j;
        for (j = 0u; j < sizeof states / sizeof states[0]; ++j) {
            assert(crc32_iso_update_table(states[j], &byte, 1u, table)
                == crc32_iso_update_bitwise(states[j], &byte, 1u));
        }
    }
    for (i = 0u; i < sizeof data; ++i) {
        data[i] = (uint8_t)((i * 37u + 11u) & 0xFFu);
    }
    for (i = 0u; i <= sizeof data; ++i) {
        size_t cut;
        uint32_t raw = crc32_iso_update_bitwise(CRC32_ISO_INIT, data, i);
        assert(crc32_iso_update_table(CRC32_ISO_INIT, data, i, table) == raw);
        assert(crc32_iso_via_f407_model(data, i) == crc32_iso_finish(raw));
        for (cut = 0u; cut <= i; ++cut) {
            uint32_t split = crc32_iso_update_table(CRC32_ISO_INIT, data, cut, table);
            split = crc32_iso_update_table(split, data + cut, i - cut, table);
            assert(split == raw);
            ++split_cases;
        }
    }
    for (i = 0u; i < sizeof check * 8u; ++i) {
        uint8_t changed[sizeof check];
        memcpy(changed, check, sizeof changed);
        changed[i / 8u] ^= (uint8_t)(1u << (i % 8u));
        assert(crc32_iso(changed, sizeof changed) != UINT32_C(0xCBF43926));
    }
    printf("CRC32 bitwise=%08" PRIX32 " table=%08" PRIX32 " empty=00000000\n",
           crc32_iso(check, sizeof check),
           crc32_iso_finish(crc32_iso_update_table(CRC32_ISO_INIT, check, sizeof check, table)));
    printf("F407 software model word 31323334: raw=%08" PRIX32 "\n",
           crc32_f407_model_word(CRC32_ISO_INIT, UINT32_C(0x31323334)));
    printf("F407 adapted software model: %08" PRIX32 " (9 bytes, no padding)\n",
           crc32_iso_via_f407_model(check, sizeof check));
    printf("PASS: 258 lengths, %u splits, 1024 byte/state pairs, 104 bit flips\n", split_cases);
}

int main(void)
{
    test_crc16();
    test_crc32();
    return 0;
}
