/*
 * vim:ts=4:sw=4:expandtab
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "compat.h"
#include "enc28j60.h"

/* used to copy the old contents of uip_buf */
static uint8_t saved[59];

static const char mymac[6] PROGMEM = "\x02\xb5\x00\x00\x00\x01";

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

static void start_icmpv6_reply(uint8_t *ip6) {
    memcpy(saved, uip_buf, 59);
    uip_buf[20] = 0x3a; /* next header: icmpv6 */
    uip_buf[21] = 0xFF; /* hop limit must be 255 */
    memcpy(uip_buf + 22 + 16, ip6 + 8, 16); /* destination address */
}

static void finish_icmpv6_reply(uint8_t *ip6) {
    uip_buf[56] = 0x00; /* checksum */
    uip_buf[57] = 0x00; /* checksum */

    /* calculate UDP checksum */
    uint16_t sum = 0;
    sum = ip6[5] + 0x3a;
    sum = chksum(sum, &uip_buf[22], 2 * 16);
    sum = chksum(sum, &uip_buf[54], uip_buf[19]);
    sum = (sum == 0 ? 0xffff : ~sum);
    if (sum == 0)
    sum = 0xffff;

    uip_buf[56] = (sum & 0xFF00) >> 8;
    uip_buf[57] = (sum & 0x00FF);

    transmit_packet();
    memcpy(uip_buf, saved, 59);
}

static bool handle_neighbor_sol() {
    /* check if the packet was adressed to the IPv6mcast MAC */
    //if (memcmp_P(uip_recvbuf, PSTR("\x33\x33\xff\x00\x00\x00"), 6) != 0)
    if (memcmp(uip_recvbuf, "\x33\x33\xff\x00\x00\x00", 6) != 0)
        return false;

    /* skip ethernet header */
    uint8_t *ip6 = uip_recvbuf + 14;

    /* skip the IPv6 header */
    uint8_t *icmp6 = ip6 + 40;

    /* check if the ICMPv6 type is neighbor solicitation */
    if (icmp6[0] != 0x87)
        return false;

    /* generate a neighbor advertisment */
    start_icmpv6_reply(ip6);

    memcpy(uip_buf + 22, icmp6 + 8, 16); /* source address */
    /* ICMPv6 */
    uip_buf[54] = 136; /* neighbor advertisement */
    uip_buf[55] = 0; /* code */
    /* reserved */
    uip_buf[58] = (1 << 6); /* solicited flag */
    uip_buf[59] = 0;
    uip_buf[60] = 0;
    uip_buf[61] = 0;
    /* target */
    memcpy(uip_buf + 62, icmp6 + 8, 16);
    /* ICMPv6 option: target link-layer address */
    uip_buf[78] = 0x02;
    uip_buf[79] = 0x01; /* length: 8 byte */
    /* local address */
    uip_buf[80] = 0x02;
    uip_buf[81] = 0xb5;
    uip_buf[82] = 0x00;
    uip_buf[83] = 0x00;
    uip_buf[84] = 0x00;
    uip_buf[85] = 0x01;
    //memcpy_P(uip_buf + 80, mymac, 6);
    /* length */
    uip_buf[19] = 32;

    uip_len = 86;
    finish_icmpv6_reply(ip6);

    return true;
}

static bool handle_echo_req() {
    /* check if the packet was adressed to my MAC */
    if (memcmp_P(uip_recvbuf, mymac, 6) != 0)
        return false;

    /* skip ethernet header */
    uint8_t *ip6 = uip_recvbuf + 14;

    /* check if the packet was adressed to the IPv6 address of the busmaster */
    if (memcmp_P(ip6 + 24, PSTR("\xFE\x80\x00\x00\x00\x00\x00\x00\x00\xb5\x00\xFF\xFE\x00\x00\x00"), 16) != 0)
        return false;

    /* it’s icmpv6 */
    uint8_t *icmp6 = ip6 + 40;
    if (icmp6[0] != 0x80)
        return false;

    /* generate echo reply */
    start_icmpv6_reply(ip6);
    memcpy(uip_buf + 22 + 16, ip6 + 8, 16); /* destination address */
    /* ICMPv6 */
    uip_buf[54] = 129; /* echo reply */
    uip_buf[55] = 0; /* code */

    /* ID */
    uip_buf[58] = icmp6[4];
    uip_buf[59] = icmp6[5];
    /* sequence */
    uip_buf[60] = icmp6[6];
    uip_buf[61] = icmp6[7];
    /* data */
    /* we only use the lower 8 bit because our UIP_BUFSIZE is < 255 byte */
    uint8_t len = ip6[5];
    memcpy(uip_buf + 62, icmp6 + 8, len - 8);
    /* length */
    uip_buf[19] = len;

    uip_len = 64 + (len-8);
    finish_icmpv6_reply(ip6);

    return true;
}

void handle_icmpv6() {
    /* 0: common checks */

    /* ethernet type must be IPv6 */
    if (uip_recvbuf[12] != 0x86 || uip_recvbuf[13] != 0xdd)
        return;

    /* check if the IPv6 next header type is ICMPv6 */
    if (uip_recvbuf[20] != 0x3a)
        return;

    /* 1: check for neighbor solicitation */
    if (handle_neighbor_sol())
        return;

    /* 2: check for echo requests */
    if (handle_echo_req())
        return;
}
