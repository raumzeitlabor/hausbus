/*
 * vim:ts=4:sw=4:expandtab
 */
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include "bus.h"

void fmt_packet(uint8_t *buffer, uint8_t destination, uint8_t source, void *pnt, uint8_t len) {
    struct buspkt *packet = (struct buspkt*)buffer;
    uint8_t *payload = pnt;
    uint8_t *paybuf = buffer;
    paybuf += sizeof(struct buspkt);

    packet->destination = destination;
    packet->source = source;
    packet->length_hi = 0;
    packet->length_lo = len;
    packet->payload_chk = 0xFF;
    packet->header_chk = packet->destination +
                 packet->source +
                 packet->payload_chk +
                 packet->length_hi +
                 packet->length_lo;
    uint8_t c;
    for (c = 0; c < len; c++)
        paybuf[c] = payload[c];
}
