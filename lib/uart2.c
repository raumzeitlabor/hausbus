/*
 * vim:ts=4:sw=4:expandtab
 */
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#define BAUD 9600
#include <util/setbaud.h>
#include <util/delay.h>

void uart2_init() {
    /* activate second uart */
    UBRR1H = UBRRH_VALUE;
    UBRR1L = UBRRL_VALUE;

    /* Generate an interrupt on incoming data, enable receiver/transmitter */
    UCSR1B = (1 << TXEN1);

    /* frame format: 8N1 */
    UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);
}

void uart2_puts(char *str) {
    char *walk;
    for (walk = str; *walk != '\0'; walk++) {
        while ( !( UCSR1A & (1<<UDRE1)) );
        UDR1 = (unsigned char)*walk;
    }
}

void uart2_puts_P(char *str) {
    char c;
    while (1) {
        c = (char)pgm_read_byte(str);
        if (c == '\0')
            break;

        while ( !( UCSR1A & (1<<UDRE1)) );
        UDR1 = (unsigned char)c;
        str++;
    }
}
