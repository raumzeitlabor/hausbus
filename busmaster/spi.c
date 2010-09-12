/*
 * vim:ts=4:sw=4:expandtab
 *
 * (c) by Alexander Neumann <alexander@bumpern.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License (either version 2 or
 * version 3) as published by the Free Software Foundation.
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

#include <avr/io.h>
#include "spi.h"
#include "compat.h"

void spi_init(void)
{
    /* MOSI, SCK, SS als output */
    DDRB |= (1 << PB4) | (1 << PB5) | (1 << PB7);

    /* MISO als input */
    DDRB &= ~(1 << PB6);

    /* SCK auf high */
    PORTB |= (1 << PB7);

    /* Set the chip-selects as high */
    PIN_SET(SPI_CS_NET);

    /* enable spi, set master and clock modes (f/2) */
    SPCR = _BV(SPE) | _BV(MSTR);
    SPSR = _BV(SPI2X);
}

uint8_t noinline spi_send(uint8_t data)
{
    SPDR = data;
    while (!(SPSR & _BV(SPIF)));

    return SPDR;
}
