/*
 * vim:ts=4:sw=4:expandtab
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdint.h>

#include "bus.h"
#include "uart2.h"

#define DEBUG
#ifdef DEBUG
  #define DBG(x) uart2_puts_P(PSTR(x))
#else
  #define DBG(x) (void)0
#endif

static uint8_t packetcnt = 0;
static uint8_t lbuffer[32];
static uint8_t rbuffer[32];

/*
 * Formats a reply packet in the given buffer
 *
 */
static void fmt_reply(uint8_t *buffer, uint8_t destination, void *payload, uint8_t len) {
    struct buspkt *reply = (struct buspkt*)buffer;
    uint8_t *payback = buffer;
    payback += sizeof(struct buspkt);
    reply->destination = destination;
    reply->source = MYADDRESS;
    reply->length_hi = 0;
    reply->length_lo = len;
    reply->payload_chk = 0xFF;
    reply->header_chk = reply->destination +
                         reply->source +
                         reply->payload_chk +
                         reply->length_hi +
                         reply->length_lo;
    memcpy(payback, payload, len);
}

/*
 * Sends the packet in the given buffer. Basically a wrapper around
 * send_packet() which adds blinking the LED and waiting for a short time
 * (25ms).
 *
 */
static void send_reply(uint8_t *buffer) {
    struct buspkt *reply = (struct buspkt*)buffer;
    PORTC &= ~(1 << PC7);
    _delay_ms(25);
    PORTC |= (1 << PC7);
    send_packet(reply);
}

int main(int argc, char *argv[]) {
    uint8_t status;

    /* enable LED so that the user knows the controller is active */
    DDRC = (1 << PC7);
    PORTC = (1 << PC7);

    /* disable watchdog */
    MCUSR &= ~(1 << WDRF);
    WDTCSR &= ~(1 << WDE);
    wdt_disable();

    net_init();
    uart2_init();

    sei();

    DBG("Pinpad firmware booted.\r\n\r\n");
    while (1) {
        _delay_ms(100);
        status = bus_status();

        if (status == BUS_STATUS_IDLE) {
            /* you could use this timeframe to do some stuff with
             * the microcontroller (check sensors etc.) */
            continue;
        }

        if (status == BUS_STATUS_MESSAGE) {
            DBG("got bus message\r\n");

            /* we received a message */
            struct buspkt *packet = current_packet();
            uint8_t *payload = (uint8_t*)packet;
            payload += sizeof(struct buspkt);
            if (packet->source == 0x00 &&
                memcmp(payload, "ping", strlen("ping")) == 0) {

                uint8_t reply[5] = {'p', 'o', 'n', 'g', packetcnt};
                fmt_reply(lbuffer, packet->source, reply, 5);
                send_reply(lbuffer);
            }
            else if (packet->source == 0x00 &&
                memcmp(payload, "send", strlen("send")) == 0) {

                DBG("cached message was sent\r\n");
                send_reply(rbuffer);

                _delay_ms(25);
                packetcnt--;
            }
            else if (memcmp(payload, "get_status", strlen("get_status")) == 0) {
                DBG("status was requested\r\n");
                /* increase packet count by one */
                fmt_reply(rbuffer, 50, "ready", 5);
                packetcnt++;
            }

            packet_done();
            continue;
        }

        if (status == BUS_STATUS_WRONG_CRC) {
            DBG("wrong crc\r\n");
            /* TODO: think of appropriate action */
            skip_byte();
            continue;
        }

        if (status == BUS_STATUS_BROKEN) {
            /* TODO: enable slow blinking of the LED */
            continue;
        }
    }
}
