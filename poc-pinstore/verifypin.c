/*
 * vim:ts=4:sw=4:expandtab
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

unsigned long crc32_messagecalc(unsigned char *data, int len);

/* a CRC32 checksum needs 4 bytes */
#define CRC32_SIZE 4
/* the amount of pins needs 1 byte (0 <= num_pins <= 180) */
#define NUM_SIZE 1
/* a PIN is encoded in 3 bytes */
#define PIN_SIZE 3
/* we have 6 pins (= 6 * 3 = 18 bytes) per block */
#define PINS_PER_BLOCK 6
#define BLOCK_SIZE ((PINS_PER_BLOCK * PIN_SIZE) + CRC32_SIZE)


/*
 * dec | bin
 * 0     0000
 * 1   | 0001
 * 2   | 0010
 * 3   | 0011
 * 4   | 0100
 * 5   | 0101
 * 6   | 0110
 * 7   | 0111
 * 8   | 1000
 * 9   | 1001
 *
 */

/*
 * The lower 24 bits (3 bytes) contain the encoded PIN.
 *
 */
uint32_t encode_pin(const char *pin) {
    return ((pin[0] - '0') << 20) |
           ((pin[1] - '0') << 16) |
           ((pin[2] - '0') << 12) |
           ((pin[3] - '0') <<  8) |
           ((pin[4] - '0') <<  4) |
           ((pin[5] - '0'));
}

void store_pin(uint8_t *dest, uint32_t pin) {
    dest[2] = (uint8_t)(pin & 0xFF);
    pin >>= 8;
    dest[1] = (uint8_t)(pin & 0xFF);
    pin >>= 8;
    dest[0] = (uint8_t)(pin & 0xFF);
}

/*
 * Searches the EEPROM for a specific PIN.
 *
 * NOTE that this function assumes the EEPROM was verified, e.g.
 * verify_eeprom() has been called before!
 *
 * Returns whether the PIN was found.
 *
 */
bool search_pin(uint8_t *eeprom, const char *pin) {
    /* We store the high, mid and low bytes for easy/fast comparison */
    const uint8_t high = ((pin[0] - '0') << 4) | (pin[1] - '0');
    const uint8_t mid = ((pin[2] - '0') << 4) | (pin[3] - '0');
    const uint8_t low = ((pin[4] - '0') << 4) | (pin[5] - '0');

    eeprom += CRC32_SIZE;
    const uint8_t num_pins = *(eeprom++);

    for (uint8_t count = 0; count < num_pins; count++) {
        if (eeprom[0] == high &&
            eeprom[1] == mid &&
            eeprom[2] == low)
            return true;
        eeprom += 3;
    }

    return false;
}

bool verify_checksum(uint8_t *eeprom) {
    const uint8_t num_pins = eeprom[CRC32_SIZE];
    /* In the case of 0 PINs, we don’t consider the EEPROM valid to avoid
     * checking at all. */
    if (num_pins == 0)
        return false;

    /* Calculate the number of blocks, rounding up. That is, for 2 pins, we
     * still need one whole block. */
    const uint8_t num_blocks = (num_pins + PINS_PER_BLOCK - 1) / PINS_PER_BLOCK;
    /* We are not including CRC32_SIZE in bytes_used because the CRC32 will be
     * calculated for the rest only of course. */
    const uint16_t bytes_used = NUM_SIZE + (num_blocks * BLOCK_SIZE);

    /* Check the CRC32 for the whole (used part of the) EEPROM. */
    uint32_t crc = crc32_messagecalc(&eeprom[CRC32_SIZE], bytes_used);
    printf("EEPROM CRC = 0x%08x\n", crc);
    if (((crc >> 24) & 0xFF) != eeprom[0] ||
        ((crc >> 16) & 0xFF) != eeprom[1] ||
        ((crc >>  8) & 0xFF) != eeprom[2] ||
        ( crc        & 0xFF) != eeprom[3]) {
        printf("EEPROM CRC wrong\n");
        return false;
    }

    /* Check the CRC of each block. Better safe than sorry. Also, we need to
     * support CRC == 0x00000000 and CRC == 0xFFFFFFFF but filter out an empty
     * EEPROM. So we check if any of the block CRCs is non-zero and non-FF. */
    bool non_zero_crc = false;

    /* Skip the header, make eeprom point to the blocks. */
    eeprom += CRC32_SIZE;
    eeprom += NUM_SIZE;

    for (uint8_t count = 0; count < num_blocks; count++) {
        crc = crc32_messagecalc(eeprom, BLOCK_SIZE - CRC32_SIZE);
        printf("block CRC = 0x%08x\n", crc);
        if (((crc >> 24) & 0xFF) != eeprom[BLOCK_SIZE - CRC32_SIZE + 0] ||
            ((crc >> 16) & 0xFF) != eeprom[BLOCK_SIZE - CRC32_SIZE + 1] ||
            ((crc >>  8) & 0xFF) != eeprom[BLOCK_SIZE - CRC32_SIZE + 2] ||
            ( crc        & 0xFF) != eeprom[BLOCK_SIZE - CRC32_SIZE + 3]) {
            printf("block CRC wrong\n");
            return false;
        }
        if (crc != 0x00000000 && crc != 0xFFFFFFFF)
            non_zero_crc = true;
        eeprom += BLOCK_SIZE;
    }

    return non_zero_crc;
}

int main() {
    uint8_t eeprom[] = {
        0x22, 0xdd, 0xed, 0x8d,
        0x00, /* number of pins (0 <= num_pins <= 180) */
        0x00, 0x00, 0x00, /* PIN 1 */
        0x00, 0x00, 0x00, /* PIN 2 */
        0x00, 0x00, 0x00, /* PIN 3 */
        0x00, 0x00, 0x00, /* PIN 4 */
        0x00, 0x00, 0x00, /* PIN 5 */
        0x00, 0x00, 0x00, /* PIN 6 */
        0xff, 0x15, 0x3f, 0xaa, /* CRC-32 block 0 */
        0x00, 0x00, 0x00, /* PIN 1 */
        0x00, 0x00, 0x00, /* PIN 2 */
        0x00, 0x00, 0x00, /* PIN 3 */
        0x00, 0x00, 0x00, /* PIN 4 */
        0x00, 0x00, 0x00, /* PIN 5 */
        0x00, 0x00, 0x00, /* PIN 6 */
        0x67, 0x1b, 0xcf, 0x4d, /* CRC-32 block 1 */
    };

    printf("crc32 = %lu\n", crc32_messagecalc("foobar.", strlen("foobar.")));

    uint32_t pin = encode_pin("320192");
    store_pin(&eeprom[4 + 1], pin);
    store_pin(&eeprom[4 + 1 + 3], encode_pin("804233"));
    eeprom[4] = 12;

    if (verify_checksum(eeprom))
        printf("EEPROM verified OK\n");

    if (search_pin(eeprom, "628222"))
        printf("ERROR: pin 628222 was found, but should not\n");
    if (!search_pin(eeprom, "320192"))
        printf("ERROR: pin 628222 was not found, but should\n");
    if (search_pin(eeprom, "804243"))
        printf("ERROR: pin 628222 was found, but should not\n");
    if (!search_pin(eeprom, "804233"))
        printf("ERROR: pin 628222 was not found, but should\n");

    //for (int c = 0; c < sizeof(eeprom); c++) {
    //    printf("%c", eeprom[c]);
    //}
}
