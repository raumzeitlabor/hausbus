/*
 * vim:ts=4:sw=4:expandtab
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#define BAUD 9600
#include <util/setbaud.h>
#include <util/delay.h>

#include "bus.h"

#ifdef BUSMASTER
    /* etherrape board */
    #define RS485_DE_DDR    DDRC
    #define RS485_DE_PORT   PORTC
    #define RS485_DE_PIN    PC2
#else
    /* hausbus-644 board */
    #define RS485_DE_DDR    DDRD
    #define RS485_DE_PORT   PORTD
    #define RS485_DE_PIN    PD4
#endif

/* simple ringbuffer */
#define UARTBUF 32

static volatile uint8_t uartbuf[UARTBUF];
static volatile uint8_t uartwrite = 0;
static volatile uint8_t uartread = 0;
static volatile uint8_t errflag = 0;
static volatile uint8_t status = BUS_STATUS_IDLE;

static uint8_t *txwalk;
uint8_t txcnt;

/* contains a non-wrapping copy of the current packet */
static uint8_t packet[UARTBUF];

/*
 * Returns the number of bytes waiting in the ringbuffer
 *
 */
static uint8_t bytes_waiting() {
    if (uartread == uartwrite)
        return 0;

    uint8_t c = 0;
    uint8_t next = uartread;
    while (next != uartwrite) {
        c++;
        next = (next + 1) & (UARTBUF - 1);
    }
    return c;
}

/*
 * Returns the packet length (header and payload) of the packet currently in
 * the ringbuffer (uartbuf).
 *
 */
static uint16_t packet_length() {
    if (bytes_waiting() < sizeof(struct buspkt))
        return 0xfe;

    uint8_t c, next = uartread;
    for (c = 0; c < 4; c++)
        next = (next + 1) & (UARTBUF - 1);
    uint16_t length = (uartbuf[next] << 8);
    next = (next + 1) & (UARTBUF - 1);
    length |= uartbuf[next];

    return sizeof(struct buspkt) + length;
}

/*
 * Returns whether a complete packet is in the ringbuffer.
 *
 */
static void check_complete() {
    uint8_t waiting = bytes_waiting();
    if (waiting < sizeof(struct buspkt))
        return;

    /* check header checksum */
    uint8_t save = 0;
    uint8_t next = uartread;
    uint8_t sum = 0;
    uint8_t c;
    for (c = 0; c < sizeof(struct buspkt); c++) {
        if (c != 2)
            sum += uartbuf[next];
        else save = uartbuf[next];
        next = (next + 1) & (UARTBUF - 1);
    }

    if (sum != save) {
        status = BUS_STATUS_WRONG_CRC;
        return;
    }

    if (waiting == packet_length())
        status = BUS_STATUS_MESSAGE;
}

uint8_t bus_status() {
    return status;
}

ISR(USART0_RX_vect) {
    uint8_t usr;
    uint8_t data;
    uint8_t is_addr;

    is_addr = (UCSR0B & (1 << RXB80));
    usr = UCSR0A;
    data = UDR0;

    if (is_addr) {
        UCSR0A &= ~(1 << MPCM0);
    }

    /* TODO: error handling? */
    if (usr & (1 << DOR0) ||
        usr & (1 << UPE0) ||
        usr & (1 << FE0)) {
        errflag = 'E';
        return;
    }

    /* Calculate next uartwrite position. The bitwise and will make it wrap. */
    uint8_t next = (uartwrite + 1) & (UARTBUF - 1);
    if (next == uartread) {
        errflag = 'f';
        /* ringbuffer overflow, we cannot store that much */
        return;
    }

    uartbuf[uartwrite] = data;
    uartwrite = next;

    check_complete();

    /* After the message was received, we switch back to MPCPU mode */
    if (status == BUS_STATUS_MESSAGE)
        UCSR0A |= (1 << MPCM0);
}

/*
 * Returns a pointer to a static buffer containing a copy of the packet which
 * is currently in the ringbuffer. The copy is necessary to have a non-wrapping
 * linear buffer which you can cast to struct buspkt.
 *
 */
struct buspkt *current_packet() {
    uint8_t next = uartread;
    uint16_t c, length = packet_length();
    for (c = 0; c < length; c++) {
        packet[c] = uartbuf[next];
        next = (next + 1) & (UARTBUF - 1);
    }

    return (struct buspkt*)packet;
}

/*
 * Called when the current packet is handled. Discards the current packet from
 * the ringbuffer by moving the uartread index.
 *
 */
void packet_done() {
    uint16_t length = packet_length();
    while (length > 0) {
        uartread = (uartread + 1) & (UARTBUF-1);
        length--;
    }

    status = BUS_STATUS_IDLE;
    check_complete();
}

void skip_byte() {
    uartread = (uartread + 1) & (UARTBUF - 1);

    check_complete();
}

ISR(USART0_UDRE_vect) {
    if (txcnt == 0) {
        RS485_DE_PORT &= ~(1 << RS485_DE_PIN);
        //UCSR0B &= ~(1 << TXCIE0);
        UCSR0B &= ~(1 << UDRIE0);
    } else {
        UCSR0B &= ~(1 << TXB80);
        UDR0 = *txwalk++;
        --txcnt;
    }
}

void send_packet(struct buspkt *pkt) {
    /* initialize pointer / length counter */
    txwalk = (uint8_t*)pkt;
    txcnt = sizeof(struct buspkt) + pkt->length_lo;

    /* activate driver enable */
    RS485_DE_PORT |= (1 << RS485_DE_PIN);

    /* write first byte */
    UCSR0B |= (1 << TXB80);
    UDR0 = *txwalk++;

    /* activate interrupt */
    //UCSR0B |= (1 << TXCIE0);
    UCSR0B |= (1 << UDRIE0);
}

void net_init() {
    /* Set baudrate (from setbaud.h) */
    UBRR0H = UBRRH_VALUE;
    UBRR0L = UBRRL_VALUE;

    /* Generate an interrupt on incoming data, enable receiver/transmitter */
    UCSR0B = (1 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);

    /* We use 9N1 for multi-cpu-mode */
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    UCSR0B |= (1 << UCSZ02);

    /* enable multi-cpu */
    UCSR0A |= (1 << MPCM0);

    /* set driver enable for RS485 as output */
    RS485_DE_DDR |= (1 << RS485_DE_PIN);
    RS485_DE_PORT &= ~(1 << RS485_DE_PIN);

    /* Enable interrupts */
    /* TODO: move this into the main code for each controller */
    sei();
}

void uart_puts(char *str) {
    char *walk;
    for (walk = str; *walk != '\0'; walk++) {
        while ( !( UCSR0A & (1<<UDRE0)) );
        UDR0 = (unsigned char)*walk;
    }
}
