/*
 * vim:ts=4:sw=4:expandtab
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#define BAUD 9600
#include <util/setbaud.h>
#include <util/delay.h>

/* simple ringbuffer */
#define UARTBUF 32

static volatile uint8_t uartbuf[UARTBUF];
static volatile uint8_t uartwrite = 0;
static volatile uint8_t uartread = 0;
static volatile uint8_t errflag = 0;

uint8_t getbyte() {
    while (uartread == uartwrite && errflag == 0)
        _delay_ms(1);
    if (errflag != 0) {
        uint8_t ret = errflag;
        errflag = 0;
        return ret;
    }
    uint8_t byte = uartbuf[uartread];
    uartbuf[uartread] = 'X';
    uint8_t next = (uartread + 1) & (UARTBUF-1);
    uartread = next;
    return byte;
}

ISR(USART0_RX_vect) {
    uint8_t usr;
    uint8_t data;
    uint8_t is_addr;

    is_addr = (UCSR0B & (1 << RXB80));
    usr = UCSR0A;
    data = UDR0;

    if (is_addr) {
        errflag = 'A';
        UCSR0A &= ~(1 << MPCM0);
        return;
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
}

int wait_for_data(int ms) {
}

struct buspkt *read_packet() {
#if 0
    static struct buspkt pkt;
    pkt.start_byte = getbyte();
    if (pkt.start_byte != '^')
        return NULL;
    pkt.checksum = getbyte();
#endif
}

void send_packet(struct buspkt *pkt) {
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
