/*
 * vim:ts=4:sw=4:expandtab
 *
 * © 2011 Michael Stapelberg
 *
 * xtea_encipher() and xtea_decipher() are
 * public domain by David Wheeler and Roger Needham
 *
 */
#include <stdio.h>
#include <stdint.h>

#include "key.h"

typedef void(*opptr)(uint32_t v[2], uint32_t const k[4]);

/* take 64 bits of data in v[0] and v[1] and 128 bits of key in k[0] - k[3] */
 
static void xtea_encipher(uint32_t v[2], uint32_t const k[4]) {
    unsigned int i;
    uint32_t v0=v[0], v1=v[1], sum=0, delta=0x9E3779B9;
    for (i=0; i < 32; i++) {
        v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + k[sum & 3]);
        sum += delta;
        v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + k[(sum>>11) & 3]);
    }
    v[0]=v0; v[1]=v1;
}
 
static void xtea_decipher(uint32_t v[2], uint32_t const k[4]) {
    unsigned int i;
    uint32_t v0=v[0], v1=v[1], delta=0x9E3779B9, sum=delta*32;
    for (i=0; i < 32; i++) {
        v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + k[(sum>>11) & 3]);
        sum -= delta;
        v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + k[sum & 3]);
    }
    v[0]=v0; v[1]=v1;
}

/*
 * Calls either xtea_encipher or xtea_decipher with the given 'buffer' of size
 * 'chunks'. 'chunks' must be a multiple of 8. The output of 'buffer' will be
 * swapped.
 *
 */
static void operate(opptr op, uint8_t *buffer, const uint8_t chunks) {
    uint8_t c;

    for (c = 0; c < chunks; c++) {
        uint32_t minibuf[2] = {
            (buffer[(c*8)+0] << 24) |
            (buffer[(c*8)+1] << 16) |
            (buffer[(c*8)+2] <<  8) |
            (buffer[(c*8)+3] <<  0),

            (buffer[(c*8)+4] << 24) |
            (buffer[(c*8)+5] << 16) |
            (buffer[(c*8)+6] <<  8) |
            (buffer[(c*8)+7] <<  0)
        };

        op(minibuf, key);

        buffer[(c*8)+0] = (minibuf[0] & 0xFF000000) >> 24;
        buffer[(c*8)+1] = (minibuf[0] & 0x00FF0000) >> 16;
        buffer[(c*8)+2] = (minibuf[0] & 0x0000FF00) >>  8;
        buffer[(c*8)+3] = (minibuf[0] & 0x000000FF) >>  0;

        buffer[(c*8)+4] = (minibuf[1] & 0xFF000000) >> 24;
        buffer[(c*8)+5] = (minibuf[1] & 0x00FF0000) >> 16;
        buffer[(c*8)+6] = (minibuf[1] & 0x0000FF00) >>  8;
        buffer[(c*8)+7] = (minibuf[1] & 0x000000FF) >>  0;
    }
}

void encipher(uint8_t *buffer, uint8_t chunks) {
    operate(xtea_encipher, buffer, chunks);
}

void decipher(uint8_t *buffer, uint8_t chunks) {
    operate(xtea_decipher, buffer, chunks);
}
