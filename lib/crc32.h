#ifndef _CRC32_H
#define _CRC32_H

/*
 * Calculates the CRC32 checksum for the given 'data' buffer of 'len' bytes.
 *
 * State is kept in 'reg32', pass the address of an uint32_t initialized with
 * 0xffffffff. This makes it possible to run the function on parts of the data.
 *
 */
uint32_t crc32_messagecalc(uint32_t *reg32, const uint8_t *data, uint8_t len);

#endif
