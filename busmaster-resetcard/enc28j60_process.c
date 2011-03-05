/*
 * Copyright (c) by Alexander Neumann <alexander@bumpern.de>
 * Copyright (c) 2007,2008,2009 by Stefan Siegl <stesie@brokenpipe.de>
 * Copyright (c) 2008 by Christian Dietrich <stettberger@dokucode.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License (version 3)
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * For more information on the GPL, please go to:
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <avr/pgmspace.h>

#include "enc28j60.h"
#include "compat.h"

/*FIXME: interrupts not supported */
#define ENC28J60_POLL
#ifndef ENC28J60_POLL
    #define interrupt_occured() (! PIN_HIGH(INT_PIN))
    #define wol_interrupt_occured() (! PIN_HIGH(WOL_PIN))
#else
    #define interrupt_occured() 1
    #define wol_interrupt_occured() 0
#endif

/* prototypes */
void process_packet(void);

//#define DEBUG_INTERRUPT
//#define DEBUG

#ifdef DEBUG
static void debug_printf(const char *fmt, ...) {
	char buf[64];
	va_list ap;

	va_start(ap, fmt);

	vsnprintf_P(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	char *walk;
	for (walk = buf; *walk != '\0'; walk++) {
		while ( !( UCSR0A & (1<<UDRE0)) );
		UDR0 = (unsigned char)*walk;
	}
        while ( !( UCSR0A & (1<<UDRE0)) );
	UDR0 = '\r';
        while ( !( UCSR0A & (1<<UDRE0)) );
	UDR0 = '\n';
}
#endif


void network_process(void)
{
    /* also check packet counter, see errata #6 */
#   ifdef ENC28J60_REV4_WORKAROUND
    uint8_t pktcnt = read_control_register(REG_EPKTCNT);
#   endif

    /* if no interrupt occured and no packets are in the receive
     * buffer, return */
    if ( !interrupt_occured()
#   ifdef ENC28J60_REV4_WORKAROUND
                || pktcnt == 0
#   endif
           )
        return;

#   if defined(ENC28J60_REV4_WORKAROUND) && defined(DEBUG_REV4_WORKAROUND)
    if (pktcnt > 5)
        debug_printf("net: BUG: pktcnt > 5\n");
#   endif

    /* read interrupt register */
    uint8_t EIR = read_control_register(REG_EIR);

    /* clear global interrupt flag */
    bit_field_clear(REG_EIE, _BV(INTIE));

#ifdef DEBUG_INTERRUPT
    /* check if some interrupts occured */
    if (EIR != 0) {

        debug_printf(PSTR("net: controller interrupt, EIR = 0x%02x"), EIR);
        if (EIR & _BV(LINKIF))
            debug_printf(PSTR("\t* Link\n"));
        if (EIR & _BV(TXIF))
            debug_printf(PSTR("\t* Tx\n"));
        if (EIR & _BV(PKTIF))
            debug_printf(PSTR("\t* Pkt\n"));
        if (EIR & _BV(RXERIF))
            debug_printf(PSTR("\t* rx error\n"));
        if (EIR & _BV(TXERIF))
            debug_printf(PSTR("\t* tx error\n"));
    }
#endif

    /* check each interrupt flag the interrupt is activated for, and clear it
     * if neccessary */

    /* link change flag */
    if (EIR & _BV(LINKIF)) {

        /* clear interrupt flag */
        read_phy(PHY_PHIR);

        /* read new link state */
        uint8_t link_state = (read_phy(PHY_PHSTAT2) & _BV(LSTAT)) > 0;

			if (link_state) {
				//debug_printf("net: got link!\n");
				#ifdef STATUSLED_NETLINK_SUPPORT
				PIN_SET(STATUSLED_NETLINK);
				#endif
			} else {
				//debug_printf("net: no link!\n");
				#ifdef STATUSLED_NETLINK_SUPPORT
				PIN_CLEAR(STATUSLED_NETLINK);
				#endif
			}
    }

    /* packet transmit flag */
    if (EIR & _BV(TXIF)) {

#ifdef DEBUG
        uint8_t ESTAT = read_control_register(REG_ESTAT);

        if (ESTAT & _BV(TXABRT))
            debug_printf(PSTR("net: packet transmit failed\n"));
#endif
        /* clear flags */
        bit_field_clear(REG_EIR, _BV(TXIF));
        bit_field_clear(REG_ESTAT, _BV(TXABRT) | _BV(LATECOL) );
    }

    /* packet receive flag */
    if (EIR & _BV(PKTIF)) {
#if 0
      if (uip_buf_lock ())
	return;			/* already locked */
#endif

      process_packet();
#if 0
      uip_buf_unlock ();
#endif
    }

    /* receive error */
    if (EIR & _BV(RXERIF)) {
#ifdef DEBUG
        debug_printf(PSTR("net: receive error!\n"));
#endif

        bit_field_clear(REG_EIR, _BV(RXERIF));

#ifdef ENC28J60_REV4_WORKAROUND
        init_enc28j60();
#endif

    }

    /* transmit error */
    if (EIR & _BV(TXERIF)) {
#ifdef DEBUG
        debug_printf(PSTR("net: transmit error!\n"));
#endif

        bit_field_clear(REG_EIR, _BV(TXERIF));
    }

    /* set global interrupt flag */
    bit_field_set(REG_EIE, _BV(INTIE));
}


void process_packet(void)
{
    /* if there is a packet to process */
    if (read_control_register(REG_EPKTCNT) == 0)
        return;

    /* read next packet pointer */
    set_read_buffer_pointer(enc28j60_next_packet_pointer);
    enc28j60_next_packet_pointer = read_buffer_memory() | (read_buffer_memory() << 8);

    /* read receive status vector */
    struct receive_packet_vector_t rpv;
    uint8_t *p = (uint8_t *)&rpv;

    for (uint8_t i = 0; i < sizeof(struct receive_packet_vector_t); i++)
        *p++ = read_buffer_memory();

    /* decrement rpv received_packet_size by 4, because the 4 byte CRC checksum is counted */
    rpv.received_packet_size -= 4;

    /* check size */
    if (rpv.received_packet_size > NET_MAX_FRAME_LENGTH
            || rpv.received_packet_size < 14
            || rpv.received_packet_size > UIP_BUFSIZE) {
#       ifdef DEBUG
        debug_printf(PSTR("net: packet too large or too small for an "
		     "ethernet header: %d\n"), rpv.received_packet_size);
#       endif
        init_enc28j60();
	//goto skip;
        return;
    }

    /* read packet */
    p = uip_recvbuf;
    for (uint16_t i = 0; i < rpv.received_packet_size; i++)
        *p++ = read_buffer_memory();

    uip_recvlen = rpv.received_packet_size;

skip:
    /* advance receive read pointer, ensuring that an odd value is programmed
     * (next_receive_packet_pointer is always even), see errata #13 */
    if ( (enc28j60_next_packet_pointer - 1) < RXBUFFER_START
            || (enc28j60_next_packet_pointer - 1) > RXBUFFER_END) {

        write_control_register(REG_ERXRDPTL, LO8(RXBUFFER_END));
        write_control_register(REG_ERXRDPTH, HI8(RXBUFFER_END));

    } else {

        write_control_register(REG_ERXRDPTL, LO8(enc28j60_next_packet_pointer - 1));
        write_control_register(REG_ERXRDPTH, HI8(enc28j60_next_packet_pointer - 1));

    }

    /* decrement packet counter */
    bit_field_set(REG_ECON2, _BV(PKTDEC));

}
