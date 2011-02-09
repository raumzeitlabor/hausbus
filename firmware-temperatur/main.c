/*
 * vim:ts=4:sw=4:expandtab
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdint.h>

#if 0
#include "bus.h"

static uint8_t packetcnt = 0;
static uint8_t lbuffer[32];
static uint8_t rbuffer[32];

static void send_reply(uint8_t *buffer) {
    struct buspkt *reply = (struct buspkt*)buffer;
    PORTB &= ~(1 << PB0);
    _delay_ms(25);
    PORTB |= (1 << PB0);
    send_packet(reply);
}
#endif

//static void build_pkt(struct buspkt *pkt, uint8_t dest, uint8_t *payload, int length) {
//    pkt->start_byte = '^';
//    /* TODO */
//    pkt->checksum = 0;
//    pkt->source = MYADDRESS;
//    pkt->destination = dest;
//    pkt->length = length;
//    pkt->payload = payload;
//}

int main(int argc, char *argv[]) {
    //uint8_t status;

    /* enable LED so that the user knows the controller is active */
    DDRC = (1 << PC7);
    PORTC = (1 << PC7);

    /* disable watchdog */
    MCUSR &= ~(1 << WDRF);
    WDTCSR &= ~(1 << WDE);
    wdt_disable();

    //net_init();

    sei();

    _delay_ms(500);
    PORTC = 0;
    _delay_ms(500);
    PORTC = (1 << PC7);

    for (;;) {
    }

#if 0
    while (1) {
        status = bus_status();
        if (status == BUS_STATUS_IDLE) {
            /* you could use this timeframe to do some stuff with
             * the microcontroller (check sensors etc.) */
            continue;
        }

        if (status == BUS_STATUS_MESSAGE) {
            /* we received a message other than a ping message */
            /* TODO: check message */
            /* we received a message */
            struct buspkt *packet = current_packet();
            uint8_t *payload = (uint8_t*)packet;
            payload += sizeof(struct buspkt);
            if (packet->source == 0x00 &&
                memcmp(payload, "ping", strlen("ping")) == 0) {
                
                uint8_t reply[5] = {'p', 'o', 'n', 'g', packetcnt};
                fmt_packet(lbuffer, packet->source, MYADDRESS, reply, 5);
                send_reply(lbuffer);
            }

            packet_done();
            continue;
        }

        if (status == BUS_STATUS_BROKEN) {
            /* TODO: enable slow blinking of the LED */
            continue;
        }
        
#if 0
        struct buspkt *recv = read_packet();
        if (recv == NULL)
            continue;
        printf("client received packet with payload: %.*s\n", recv->length, recv->payload);
        if (strncmp("PING", (char*)recv->payload, strlen("PING")) == 0) {
            /* i need to respond to ping */
            printf("PING PONG\n");

            struct buspkt pkt;
            build_pkt(&pkt, recv->source, (uint8_t*)(gotmsg ? "PONG\x01" : "PONG\x00"), 6);
            send_packet(&pkt);
            continue;
        }

        if (strncmp("SENDMSG", (char*)recv->payload, strlen("SENDMSG")) == 0) {
            printf("should send message\n");
            struct buspkt pkt;
            build_pkt(&pkt, recv->source, (uint8_t*)"FOO", 3);
            send_packet(&pkt);
            gotmsg = false;
            continue;
        }
#endif
    }
#endif
}
