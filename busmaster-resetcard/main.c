/*
 * vim:ts=4:sw=4:expandtab
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define BAUD 9600
#include <util/setbaud.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "spi.h"
#include "enc28j60.h"
#include "bus.h"
#include "compat.h"
#include "icmpv6.h"

volatile uint8_t eth_to_rs[128];
volatile uint8_t eth_to_rs_cnt = 0;
volatile uint8_t eth_to_rs_rem = 0;
volatile uint8_t sendit = 0;

uint8_t lbuffer[32];
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

#define DEBUG
#ifdef DEBUG
  /* TODO: pgmspace benutzen für die strings */
  #define DBG(x) uart2_puts(x)
#else
  #define DBG(x) (void)0
#endif

uint16_t uip_len = 0;
uint8_t uip_buf[UIP_BUFSIZE+2] =
{
    /* mac destination: IPv6 multicast */
    0x33, 0x33, 0x00, 0xb5, 0x00, 0x01,
    /* mac source: locally administered (02), hausbus (b5) */
    0x02, 0xb5, 0x00, 0x00, 0x00, 0x00,
    /* ethertype (IP) */
    0x86, 0xdd,

    /* ethernet payload */
    0x60,
    /* flowlabel */
    0x00, 0x00, 0x00,
    /* total length (will be filled in later) */
    0x00, 0x00,
    //uip_buf[19] = 8 /* udp header */ + payload_len;

    /* next header: udp */
    0x11,
    /* hop limit */
    0x03,
    /* fd1a:56e6:97e9::/48 ist unsere ULA-range */
    /* source: fe80::b5:ff:fe00:0001 */
    0xfd, 0x1a, 0x56, 0xe6, 0x97, 0xe9, 0x00, 0x00, 0x00, 0xb5, 0x00, 0xff, 0xfe, 0x00, 0x00, 0x01,
    /* destination: ff05::b5:1 */
    0xff, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb5, 0x00, 0x01,

    /* data */
    /* UDP */
    0x02, /* source port low */
    0x02, /* source port high */
    0xA4, /* dest port low */
    0x0F, /* dest port high */
    0 /* data length high */
};

uint8_t uip_recvbuf[UIP_BUFSIZE+2];
uint16_t uip_recvlen = 0;

bool gotmsg = true;

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
    uint16_t c;
    int len = 8 /* udp header */ + payload_len;

    uip_buf[5] = 1;
    uip_buf[53] = 1;

    /* copy packet->source into the MAC and IPv6 address */
    uip_buf[11] = 0;
    uip_buf[37] = 0;


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

static void raw_send(const char *str, int payload_len) {
    uint16_t c;
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


ISR(USART0_RX_vect) {
    uint8_t usr;
    uint8_t data;
    uint8_t is_addr;

    usr = UCSR0A;
    data = UDR0;

    if (eth_to_rs_cnt == 127) {
        eth_to_rs_cnt = 0;
    }

    if ((eth_to_rs_cnt == 0 && data == '^') ||
        (eth_to_rs_cnt > 0)) {
        eth_to_rs[eth_to_rs_cnt] = data;
        eth_to_rs_cnt++;
    }
#if 0

    if (eth_to_rs_rem > 0) {
        eth_to_rs[eth_to_rs_cnt] = data;
        eth_to_rs_cnt++;
        eth_to_rs_rem--;
        //while ( !( UCSR0A & (1<<UDRE0)) );
        //UDR0 = data;
    } else {
        if (data < 100) {
        //        while ( !( UCSR0A & (1<<UDRE0)) );
        //    UDR0 = 'R';
        //        while ( !( UCSR0A & (1<<UDRE0)) );
        //    UDR0 = data;
        //        while ( !( UCSR0A & (1<<UDRE0)) );
        //    UDR0 = 'R';
        eth_to_rs_rem = data;
        eth_to_rs_cnt = 0;
        } else {
            eth_to_rs_rem = 0;
            eth_to_rs_cnt = 0;
        }
    }

    if (eth_to_rs_rem == 0 && eth_to_rs_cnt > 0) {
        //while ( !( UCSR0A & (1<<UDRE0)) );
        //UDR0 = 'S';
        //DBG("Packet done!\r\n");
        eth_to_rs[eth_to_rs_cnt] = '\0';
        sendit = 1;
        //DBG(eth_to_rs);
    } else {
        //while ( !( UCSR0A & (1<<UDRE0)) );
        //UDR0 = 'W';
    }

    //DBG("got something!\r\n");
    //snprintf(buf, sizeof(buf), "received: *%02x*\r\n", data);
    //DBG(buf);

    //buf[0] = data;
    //buf[1] = '\r';
    //buf[2] = '\n';
    //buf[3] = '\0';
    //DBG(buf);
#endif
}

int main(int argc, char *argv[]) {
    /* Disable driver enable for RS485 ASAP */
    //DDRC |= (1 << PC2);
    //PORTC &= ~(1 << PC2);


    /* init serial line debugging */
    UBRR0H = UBRRH_VALUE;
    UBRR0L = UBRRL_VALUE;
    UCSR0B = (1 << RXCIE1) | (1 << RXEN1) | (1 << TXEN0);

    UCSR0C = (1<<UCSZ00) | (1<<UCSZ01);

    /* Initialize UART */
    net_init();
    DBG("READY!\r\n");

    DBG("Initializing SPI...\r\n");

    spi_init();

    DBG("Initializing ENC28J60...\r\n");

    init_enc28j60();

    DBG("Initialized ENC28J60\r\n");
    char obuf[64];
    snprintf(obuf, sizeof(obuf), "enc28j60 rev 0x%x\n", read_control_register(REG_EREVID));
    DBG(obuf);

    char buf[16] = "serial:       X\n";
    int cnt = 0;
    while (1) {
        if (eth_to_rs_cnt > 0 &&
            eth_to_rs[eth_to_rs_cnt-1] == '$') {
            eth_to_rs[eth_to_rs_cnt-1] = '\0';

            int dest = 0;
            int pktlen = 0;
            char minibuf[16];
            minibuf[0] = eth_to_rs[1];
            minibuf[1] = eth_to_rs[2];
            minibuf[2] = '\0';
            if (sscanf(minibuf, "%d", &dest) != 1) {
                DBG("Could not parse dest\r\n");
                eth_to_rs_cnt = 0;
                continue;
            }
            minibuf[0] = eth_to_rs[3];
            minibuf[1] = eth_to_rs[4];
            if (sscanf(minibuf, "%d", &pktlen) != 1) {
                DBG("Could not parse len\r\n");
                eth_to_rs_cnt = 0;
                continue;
            }

            if (pktlen != (eth_to_rs_cnt - 6)) {
                DBG("lens are not the same\r\n");
                minibuf[2] = '\r';
                minibuf[3] = '\n';
                minibuf[4] = '\0';
                DBG(minibuf);
                snprintf(minibuf, sizeof(minibuf), "e: %d\r\n", eth_to_rs_cnt-6);
                DBG(minibuf);
                snprintf(minibuf, sizeof(minibuf), "p: %d\r\n", pktlen);
                DBG(minibuf);
                eth_to_rs_cnt = 0;
                continue;
            }

                fmt_packet(lbuffer, dest, 0xFF, eth_to_rs + 5, pktlen);
                struct buspkt *packet = (struct buspkt*)lbuffer;

                send_packet(packet);
                syslog_send("sent packet", strlen("sent packet"));
                _delay_ms(25);
                sendit = 0;
                eth_to_rs_cnt = 0;
        }
#if 0
        network_process();
        if (uip_recvlen > 0) {
            syslog_send("handling ethernet packet", strlen("handling ethernet packet"));
            DBG("Handling packet\r\n");
            handle_icmpv6();

            if (uip_recvbuf[20] == 0x11) {
                syslog_send("handling udp packet", strlen("handling udp packet"));
                /* UDP */
                uint8_t *udp = uip_recvbuf + 14 + 40;
                uint8_t len = udp[5] - 8;
                /* TODO: sanity check */
                uint8_t *recvpayload = udp + 8 /* udp */;

                fmt_packet(lbuffer, uip_recvbuf[53], 0xFF, recvpayload, len);
                struct buspkt *packet = (struct buspkt*)lbuffer;

                //syslog_send("sending packet", strlen("sending packet"));
                send_packet(packet);
                _delay_ms(25);
                cnt = 85;
                syslog_send("ethernet to rs485 done", strlen("ethernet to rs485 done"));
            }

            //syslog_send("received a packet", strlen("received a packet"));
            buf[14] = uip_recvlen;

            //syslog_send(uip_recvbuf, uip_recvlen);
            uip_recvlen = 0;
        }
#endif
        _delay_ms(10);
        if (cnt++ == 100) {
            fmt_packet(lbuffer, 1, 0, "ping", 4);
            struct buspkt *packet = (struct buspkt*)lbuffer;
            syslog_send("ping sent", strlen("ping sent"));
            send_packet(packet);
            cnt = 0;
        snprintf(obuf, sizeof(obuf), "cnt = %d, rem = %d\r\n",
                eth_to_rs_cnt, eth_to_rs_rem);
        syslog_send(obuf, strlen(obuf));
        }

        uint8_t status = bus_status();
        if (status == BUS_STATUS_IDLE)
            continue;

        if (status == BUS_STATUS_MESSAGE) {
            /* get a copy of the current packet */
            struct buspkt *packet = current_packet();
            uint8_t *walk = packet;
            uint8_t *payload = (uint8_t*)packet;
            payload += sizeof(struct buspkt);

            /* check for ping replies */
            if (packet->destination == 0x00 &&
                memcmp(payload, "pong", strlen("pong")) == 0) {
                syslog_send("pong received", strlen("pong received"));
                /* TODO: store that this controller is reachable */
                /* check if the controller has any waiting messages */
                if (payload[4] > 0) {
                    /* request the message */
                    fmt_packet(lbuffer, packet->source, 0, "send", 4);
                    struct buspkt *reply = (struct buspkt*)lbuffer;
                    //syslog_send("sending packet", strlen("sending packet"));
                    _delay_ms(25);
                    send_packet(reply);
                    syslog_send("sendreq sent", strlen("sendreq sent"));
                    //syslog_send(reply, reply->length_lo + sizeof(struct buspkt));

                    _delay_ms(25);
                    cnt = 0;
                }
            }

#if 0
            /* copy packet->destination into the MAC and IPv6 address */
            uip_buf[5] = packet->destination; /* MAC */
            uip_buf[53] = packet->destination; /* IPv6 */

            /* copy packet->source into the MAC and IPv6 address */
            uip_buf[11] = packet->source; /* MAC */
            uip_buf[37] = packet->source; /* IPv6 */

            raw_send(payload, packet->length_lo);
#endif

            char minibuf[16];
            snprintf(minibuf, sizeof(minibuf), "^%02d%02d", 0, packet->length_lo);
            uart_puts(minibuf);


            uint8_t c;
            for (c = 0; c < sizeof(struct buspkt); c++) {
                while ( !( UCSR0A & (1<<UDRE0)) );
                UDR0 = walk[c];
            }
            for (c = 0; c < packet->length_lo; c++) {
                while ( !( UCSR0A & (1<<UDRE0)) );
                UDR0 = payload[c];
            }
                while ( !( UCSR0A & (1<<UDRE0)) );
                UDR0 = '$';



            /* discard the packet from serial buffer */
            packet_done();
            continue;
        }

        if (status == BUS_STATUS_WRONG_CRC) {
            syslog_send("broken", strlen("broken"));
            struct buspkt *packet = current_packet();
            raw_send(packet, 16);
            skip_byte();
            continue;
        }
    }
}
