/*
 * vim:ts=4:sw=4:expandtab
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <util/delay.h>
#include <avr/eeprom.h>

#include "bus.h"

uint32_t crc32_messagecalc(uint32_t *reg32, const uint8_t *data, int len);

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
    return (((uint32_t)(pin[0]) - '0') << 20) |
           (((uint32_t)(pin[1]) - '0') << 16) |
           (((uint32_t)(pin[2]) - '0') << 12) |
           (((uint32_t)(pin[3]) - '0') <<  8) |
           (((uint32_t)(pin[4]) - '0') <<  4) |
           (((uint32_t)(pin[5]) - '0'));
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
bool search_pin(const char *pin) {
    /* We store the high, mid and low bytes for easy/fast comparison */
    const uint8_t high = ((pin[0] - '0') << 4) | (pin[1] - '0');
    const uint8_t mid = ((pin[2] - '0') << 4) | (pin[3] - '0');
    const uint8_t low = ((pin[4] - '0') << 4) | (pin[5] - '0');

    const uint8_t num_pins = eeprom_read_byte((uint8_t*)CRC32_SIZE);
    uint8_t buffer[3];

    for (uint8_t count = 0; count < num_pins; count++) {
        const uint8_t *offset =
                       /* skip header */
            (uint8_t*)(CRC32_SIZE + NUM_SIZE +
                       /* every pin is 3 bytes long */
                       (count * 3) +
                       /* every 6 pins, there are 4 bytes CRC */
                       ((count / 6) * 4));

        eeprom_read_block(buffer, offset, 3);

        if (buffer[0] == high &&
            buffer[1] == mid &&
            buffer[2] == low)
            return true;
    }

    return false;
}

bool verify_checksum() {
    const uint8_t num_pins = eeprom_read_byte((uint8_t*)CRC32_SIZE);
    /* In the case of 0 PINs, we don’t consider the EEPROM valid to avoid
     * checking at all. */
    if (num_pins == 0)
        return false;

    /* Calculate the number of blocks, rounding up. That is, for 2 pins, we
     * still need one whole block. */
    const uint8_t num_blocks = (num_pins + PINS_PER_BLOCK - 1) / PINS_PER_BLOCK;

    /* Check the CRC32 for the whole (used part of the) EEPROM. */
    /* Register for checking the whole EEPROM (in chunks) */
    uint32_t whole_reg32 = 0xffffffff;
    uint32_t whole_crc = 0;
    crc32_messagecalc(&whole_reg32, &num_pins, 1);

    /* Check the CRC of each block. Better safe than sorry. Also, we need to
     * support CRC == 0x00000000 and CRC == 0xFFFFFFFF but filter out an empty
     * EEPROM. So we check if any of the block CRCs is non-zero and non-FF. */
    bool non_zero_crc = false;

    uint8_t block[BLOCK_SIZE];
    uint32_t block_reg32;
    uint32_t block_crc;
    for (uint8_t count = 0; count < num_blocks; count++) {
        const uint8_t *offset =
                       /* skip header */
            (uint8_t*)(CRC32_SIZE + NUM_SIZE +
                       /* every block is BLOCK_SIZE bytes */
                       (count * BLOCK_SIZE));

        eeprom_read_block(block, offset, BLOCK_SIZE);

        /* Calculate the CRC for this block */
        block_reg32 = 0xffffffff;
        block_crc = crc32_messagecalc(&block_reg32, block, BLOCK_SIZE - CRC32_SIZE);

        /* Continuously update the whole_crc */
        whole_crc = crc32_messagecalc(&whole_reg32, block, BLOCK_SIZE);

        /* Error out if this block has a wrong CRC */
        if (((block_crc >> 24) & 0xFF) != block[BLOCK_SIZE - CRC32_SIZE + 0] ||
            ((block_crc >> 16) & 0xFF) != block[BLOCK_SIZE - CRC32_SIZE + 1] ||
            ((block_crc >>  8) & 0xFF) != block[BLOCK_SIZE - CRC32_SIZE + 2] ||
            ( block_crc        & 0xFF) != block[BLOCK_SIZE - CRC32_SIZE + 3]) {
            return false;
        }
        if (block_crc != 0x00000000 && block_crc != 0xFFFFFFFF)
            non_zero_crc = true;
    }

    /* Error out if the CRC32 for the whole EEPROM is wrong */
    eeprom_read_block(block, (uint8_t*)0, CRC32_SIZE);
    if (((whole_crc >> 24) & 0xFF) != block[0] ||
        ((whole_crc >> 16) & 0xFF) != block[1] ||
        ((whole_crc >>  8) & 0xFF) != block[2] ||
        ( whole_crc        & 0xFF) != block[3]) {
        return false;
    }

    return non_zero_crc;
}

int main() {
    /* Initialize UART */
    net_init();

    /* Sleep 0.25 seconds to give the UART some time to come up */
    _delay_ms(250);

    uart_puts("Booting..\r\n");

    _delay_ms(250);

    uart_puts("Writing EEPROM..\r\n");

    uint8_t eeprom[] = {
        0x22, 0xdd, 0xed, 0x8d,
        0x0C, /* number of pins (0 <= num_pins <= 180) */
        0x32, 0x01, 0x92, /* PIN 1 */
        0x80, 0x42, 0x33, /* PIN 2 */
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

    /* Write the above block of bytes to the EEPROM at address 0 */
    eeprom_update_block(eeprom, (uint8_t*)0x0, sizeof(eeprom));
    uart_puts("Done. Now testing\r\n");

    if (verify_checksum(eeprom))
        uart_puts("EEPROM verified OK\r\n");

    if (search_pin("628222"))
        uart_puts("ERROR: pin 628222 was found, but should not\r\n");
    if (!search_pin("320192"))
        uart_puts("ERROR: pin 628222 was not found, but should\r\n");
    if (search_pin("804243"))
        uart_puts("ERROR: pin 628222 was found, but should not\r\n");
    if (!search_pin("804233"))
        uart_puts("ERROR: pin 628222 was not found, but should\r\n");

    uart_puts("Tests completed.\r\n");
    /* Sleep. */
    for (;;) {
    }
}
