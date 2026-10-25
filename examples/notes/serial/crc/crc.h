#ifndef SERIAL_CRC_H
#define SERIAL_CRC_H

#include <stddef.h>
#include <stdint.h>

#define CRC16_CCITT_FALSE_INIT UINT16_C(0xFFFF)
#define CRC32_ISO_INIT UINT32_C(0xFFFFFFFF)

/* A null data pointer is permitted only when length is zero. */
uint16_t crc16_ccitt_false_update(uint16_t state, const uint8_t *data, size_t length);
uint16_t crc16_ccitt_false(const uint8_t *data, size_t length);

/* Update functions accept and return raw state, before the final XOR. */
uint32_t crc32_iso_update_bitwise(uint32_t state, const uint8_t *data, size_t length);
void crc32_make_table(uint32_t table[256]);
uint32_t crc32_iso_update_table(uint32_t state, const uint8_t *data, size_t length,
                               const uint32_t table[256]);
uint32_t crc32_iso_finish(uint32_t state);
uint32_t crc32_iso(const uint8_t *data, size_t length);

/* Software models only: these functions never access a peripheral. */
uint32_t crc32_reverse_bits(uint32_t value);
uint32_t crc32_f407_model_word(uint32_t state, uint32_t word);
uint32_t crc32_iso_via_f407_model(const uint8_t *data, size_t length);

#endif
