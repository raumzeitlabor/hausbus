/*
 * vim:ts=4:sw=4:expandtab
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "enc28j60.h"
#include "bus.h"
#include "compat.h"

/*
 * ----------------------------------------------------------------------
 * The following settings affect how many devices you can put on your bus
 * without the bus getting slow:
 *
 */

/* time to wait for a message from the target in miliseconds */
#define MSG_WAIT_MS 15

/*
 * ----------------------------------------------------------------------
 *
 */

//#define DEBUG
#ifdef DEBUG
  /* TODO: pgmspace benutzen für die strings */
  #define DBG(x) uart_puts(x)
#else
  #define DBG(x) (void)0
#endif

uint16_t uip_len = 0;
uint8_t uip_buf[UIP_BUFSIZE+2];

bool gotmsg = true;

static void build_pkt(struct buspkt *pkt, uint8_t dest, uint8_t *payload, int length) {
    pkt->start_byte = '^';
    /* TODO */
    pkt->checksum = 0;
    pkt->source = MYADDRESS;
    pkt->destination = dest;
    pkt->length = length;
    pkt->payload = payload;
}


/*
 * Generates an IP checksum for the packet
 * TODO: directly store it at the right place in the buffer
 *
 */
static uint16_t ip_checksum(uint8_t *buffer, uint8_t len) {
    uint16_t val;
    uint16_t sum = 0;
    uint8_t c;
    for (c = 0; c < len; c += 2) {
        val = (buffer[c] << 8) | buffer[c+1];
        sum += val;
    }
    sum = (sum > 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return ~sum;
}

static void syslog_send(const char *str, int payload_len) {
    int c;
        uip_buf[17] = 20 /* ip header */ + 8 /* udp header */ + payload_len; /* len = 23 */
        uip_buf[24] = 0x00;
        uip_buf[25] = 0x00;
        uint16_t checksum = ip_checksum(&uip_buf[14], 20);
        uip_buf[24] = (checksum & 0xFF00) >> 8;
        uip_buf[25] = (checksum & 0x00FF);
        uip_buf[39] = 8 /* udp header */ + payload_len; /* data length high */
        for (c = 0; c < payload_len; c++)
            uip_buf[42 + c] = str[c];

        uip_len = 43 + payload_len;
        transmit_packet();
}


int main(int argc, char *argv[]) {
    DDRC |= (1 << PC2);
    PINC &= ~(1 << PC2);
    net_init();
    while (1) {
        /* send data via uart for testing */
        DBG("Initializing SPI...\r\n");

        spi_init();

        DBG("Initializing ENC28J60...\r\n");

        init_enc28j60();

        DBG("Initialized ENC28J60\r\n");

        /* paket aufbereiten und schicken */
        /* mac destination */
        uip_buf[0] = 0x00;
        uip_buf[1] = 0x1f;
        uip_buf[2] = 0x16;
        uip_buf[3] = 0x1a;
        uip_buf[4] = 0xf5;
        uip_buf[5] = 0xb8;
        /* mac source */
        uip_buf[6] = 0xAC;
        uip_buf[7] = 0xDE;
        uip_buf[8] = 0x48;
        uip_buf[9] = 0xFD;
        uip_buf[10] = 0x0F;
        uip_buf[11] = 0xD0;
        /* ethertype (IP) */
        uip_buf[12] = 0x08;
        uip_buf[13] = 0x00;
        /* ethernet payload */
        uip_buf[14] = 0x45; /* minimal header len, ipv4 */
        uip_buf[15] = 0x00; /* DSCP, ECN */
        /* total length: */
        uip_buf[16] = 0x00;
        //uip_buf[17] = 20 /* ip header */ + 8 /* udp header */ + payload_len; /* len = 23 */

        /* identification (16bit) */
        uip_buf[18] = 0x00;
        uip_buf[19] = 0x00;
        /* flags + fragment offset (16bit) */
        uip_buf[20] = 0x00;
        uip_buf[21] = 0x00;

        /* ttl (8bit) */
        uip_buf[22] = 23;
        /* protocol */
        uip_buf[23] = 0x11; /* udp */
        /* checksum */
        uip_buf[24] = 0x00;
        uip_buf[25] = 0x00;
        /* source ip */
        uip_buf[26] = 0xC0;
        uip_buf[27] = 0xa8;
        uip_buf[28] = 0x01;
        uip_buf[29] = 0x2B;
        /* destination ip */
        uip_buf[30] = 0xC0;
        uip_buf[31] = 0xA8;
        uip_buf[32] = 0x01;
        uip_buf[33] = 0x2A;
        //uint16_t checksum = ip_checksum(&uip_buf[14], 20);
        //uip_buf[24] = (checksum & 0xFF00) >> 8;
        //uip_buf[25] = (checksum & 0x00FF);
        /* data */
        /* TODO: UDP */
        uip_buf[34] = 0x02; /* source port low */
        uip_buf[35] = 0x02; /* source port high */
        uip_buf[36] = 0x02; /* dest port low */
        uip_buf[37] = 0x02; /* dest port high */
        uip_buf[38] = 0; /* data length low */
        //uip_buf[39] = 8 /* udp header */ + payload_len; /* data length high */
        /* TODO: udp checksum */
        uip_buf[40] = 0x00;
        uip_buf[41] = 0x00;
        /* TODO? crc32 */


        char buf[16] = "serial:       X\n";
            for (;;) {
                DBG("Reading from UART\r\n");

                uint8_t byte = getbyte();
                buf[14] = byte;

                syslog_send(buf, 16);

                DBG("Done\r\n");

            }
            /* We are the busmaster */
            printf("sending ping\n");
            for (int c = 0; c < 2; c++) {
                struct buspkt pkt;
                build_pkt(&pkt, c, (uint8_t*)"PING\x05", 5);
                send_packet(&pkt);
                if (wait_for_data(MSG_WAIT_MS) == WAIT_TIMEOUT) {
                    printf("error: timeout: ");
                    print_address(pkt.destination);
                    printf(" (%d ms)\n", MSG_WAIT_MS);

                    continue;
                }
                struct buspkt *recv = read_packet();
                if (memcmp(recv->source, c, 16) != 0) {
                    printf("ERROR: wrong source for packet\n");
                    printf("answer from ");
                    print_address(recv->source);
                    continue;
                }
                if (strncmp((char*)recv->payload, "PONG", strlen("PONG")) != 0) {
                    printf("ERROR: not a PONG reply\n");
                    continue;
                }
                printf("pong reply from ");
                print_address(recv->source);

                int messages = recv->payload[4];

                printf(" (XX ms), messages waiting: %d\n", messages);
                if (messages == 0)
                    continue;

                printf("requesting message\n");
                pkt.length = strlen("SENDMSG");
                pkt.payload = (uint8_t*)"SENDMSG";
                send_packet(&pkt);
                if (wait_for_data(MSG_WAIT_MS) == WAIT_TIMEOUT) {
                    printf("no answer to sendmsg in 15ms\n");
                    continue;
                }
                recv = read_packet();
                printf("got a package of len %d\n", recv->length);

            }
            printf("\n");
            _delay_ms(500);
    }
}
