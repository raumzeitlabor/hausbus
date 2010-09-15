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

#if 0
static void build_pkt(struct buspkt *pkt, uint8_t dest, uint8_t *payload, int length) {
    pkt->start_byte = '^';
    /* TODO */
    pkt->checksum = 0;
    pkt->source = MYADDRESS;
    pkt->destination = dest;
    pkt->length = length;
    pkt->payload = payload;
}
#endif

static uint16_t chksum(uint16_t sum, const uint8_t *data, uint16_t len)
{
  uint16_t t;
  const uint8_t *dataptr;
  const uint8_t *last_byte;

  dataptr = data;
  last_byte = data + len - 1;

  while(dataptr < last_byte) {  /* At least two more bytes */
    t = (dataptr[0] << 8) + dataptr[1];
    sum += t;
    if(sum < t) {
      sum++;            /* carry */
    }
    dataptr += 2;
  }

  if(dataptr == last_byte) {
    t = (dataptr[0] << 8) + 0;
    sum += t;
    if(sum < t) {
      sum++;            /* carry */
    }
  }

  /* Return sum in host byte order. */
  return sum;
}

static void syslog_send(const char *str, int payload_len) {
    int c;
    int len = 8 /* udp header */ + payload_len;

    /* IPv6 header → length */
    uip_buf[19] = len;

    /* UDP header → length */
    uip_buf[59] = len;

    /* initialize UDP checksum to zero */
    uip_buf[60] = 0x00;
    uip_buf[61] = 0x00;

    for (c = 0; c < payload_len; c++)
        uip_buf[62 + c] = str[c];

    /* calculate UDP checksum */
    uint16_t sum = 0;
    sum = len + 17;
    sum = chksum(sum, (uint8_t*)&uip_buf[22], 2 * 16);
    sum = chksum(sum, &uip_buf[54], len);
    sum = (sum == 0 ? 0xffff : ~sum);
    if (sum == 0)
        sum = 0xffff;

    uip_buf[60] = (sum & 0xFF00) >> 8;
    uip_buf[61] = (sum & 0x00FF);

    uip_len = 62 + payload_len;
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
        /* mac destination: IPv6 multicast */
        uip_buf[0] = 0x33;
        uip_buf[1] = 0x33;
        uip_buf[2] = 0x00;
        uip_buf[3] = 0xb5;
        uip_buf[4] = 0x00;
        uip_buf[5] = 0x01;
        /* mac source */
        uip_buf[6] = 0xAC;
        uip_buf[7] = 0xDE;
        uip_buf[8] = 0x48;
        uip_buf[9] = 0xFD;
        uip_buf[10] = 0x0F;
        uip_buf[11] = 0xD0;
        /* ethertype (IP) */
        uip_buf[12] = 0x86;
        uip_buf[13] = 0xdd;

        /* ethernet payload */
        uip_buf[14] = 0x60; /* ipv6, traffic class 0 */
        /* flowlabel */
        uip_buf[15] = 0x00;
        uip_buf[16] = 0x00;
        uip_buf[17] = 0x00;
        /* total length: */
        uip_buf[18] = 0x00;
        //uip_buf[19] = 8 /* udp header */ + payload_len;

        /* next header: udp */
        uip_buf[20] = 0x11;
        /* hop limit */
        uip_buf[21] = 0x02;
        /* source: fe80::aede:48ff:fefd:0fd0 */
        uip_buf[22] = 0xfe;
        uip_buf[23] = 0x80;
        uip_buf[24] = 0x00;
        uip_buf[25] = 0x00;
        uip_buf[26] = 0x00;
        uip_buf[27] = 0x00;
        uip_buf[28] = 0x00;
        uip_buf[29] = 0x00;
        uip_buf[30] = 0xae;
        uip_buf[31] = 0xde;
        uip_buf[32] = 0x48;
        uip_buf[33] = 0xff;
        uip_buf[34] = 0xfe;
        uip_buf[35] = 0xfd;
        uip_buf[36] = 0x0f;
        uip_buf[37] = 0xd0;
        /* destination: ff02::b5:1 */
        uip_buf[38] = 0xff;
        uip_buf[39] = 0x02;

        uip_buf[40] = 0x00;
        uip_buf[41] = 0x00;
        uip_buf[42] = 0x00;
        uip_buf[43] = 0x00;

        uip_buf[44] = 0x00;
        uip_buf[45] = 0x00;
        uip_buf[46] = 0x00;
        uip_buf[47] = 0x00;

        uip_buf[48] = 0x00;
        uip_buf[49] = 0x00;
        uip_buf[50] = 0x00;
        uip_buf[51] = 0xb5;
        uip_buf[52] = 0x00;
        uip_buf[53] = 0x01;

        /* data */
        /* UDP */
        uip_buf[54] = 0x02; /* source port low */
        uip_buf[55] = 0x02; /* source port high */
        uip_buf[56] = 0x02; /* dest port low */
        uip_buf[57] = 0x02; /* dest port high */
        uip_buf[58] = 0; /* data length high */

        char buf[16] = "serial:       X\n";
            for (;;) {
                DBG("Reading from UART\r\n");

                uint8_t byte = getbyte();
                buf[14] = byte;

                syslog_send(buf, 16);

                DBG("Done\r\n");

            }
#if 0
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
#endif
    }
}
