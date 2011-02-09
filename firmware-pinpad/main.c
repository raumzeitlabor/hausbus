/*
 * vim:ts=4:sw=4:expandtab
 *
 * ATmega644P (4096 Bytes SRAM)
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdint.h>
#define BAUD 9600
#include <util/setbaud.h>


#include "bus.h"
#include "uart2.h"

//#define DEBUG
#ifdef DEBUG
  #define DBG(x) uart2_puts_P(PSTR(x))
#else
  #define DBG(x) (void)0
#endif

/* seite 84 */
static char pin[11];
static uint8_t pincnt = 0;
static volatile char serbuf[64];
static volatile uint8_t sercnt = 0;

static uint8_t packetcnt = 0;
static uint8_t lbuffer[32];

/* Outgoing packet buffer. The message header size is 6 bytes, let's assume a
 * typical message length of 12 bytes. Thereforce, we can fit about 28 messages
 * into this buffer. It will be cleared every second. */
buspkt_full(10);
static struct buspkt_10 rbuffer[32];
static uint8_t rb_current = 0;
static uint8_t rb_next = 0;

/*
 * Tries to send a message to message group 50 from MYADDRESS
 *
 */
bool sendmsg(const char *msg) {
    /* If the packet which is next to be fetched (rb_next) is at the same
     * position where we want to write to, we have to abort */
    if (((rb_current + 1) % 32) == rb_next)
        return false;

    fmt_packet(&rbuffer[rb_current], 50, MYADDRESS, msg, strlen(msg));
    rb_current = (rb_current + 1) % 32;
    packetcnt++;
    return true;
}

ISR(USART1_RX_vect) {
    uint8_t byte;
    uint8_t usr;

    usr = UCSR1A;
    byte = UDR1;

    if (serbuf[8] == '$')
        return;

    if (sercnt == 0 && byte != '^')
        return;

    if (sercnt == 8 && byte != '$')
        return;

    serbuf[sercnt] = byte;
    sercnt++;

    if (sercnt == 9)
        sercnt = 0;
}

static void handle_command(const char *buffer) {
    if (strncmp(buffer, "^PAD ", strlen("^PAD ")) == 0) {
        char c = buffer[5];
        if (pincnt < 10) {
            pin[pincnt] = c;
            pincnt++;

            char msg[] = "KEY x";
            msg[strlen(msg)-1] = c;
            sendmsg(msg);
        }
        uart2_puts("^LED 2 1$\n");
        uart2_puts("^BEEP 1 $\n");
        char *str;
        if (pincnt == 7 && strncmp(pin, "777#", 5) == 0) {
            uart2_puts("^LED 2 2$");
            uart2_puts("^BEEP 2 $\n");
            pincnt = 0;
            memset(pin, '\0', sizeof(pin));
            PORTA &= ~(1 << PA2);
            _delay_ms(500);
            PORTA |= (1 << PA2);
        } else if (pincnt == 4 && strncmp(pin, "666#", 5) == 0) {
            uart2_puts("^LED 2 2$");
            uart2_puts("^BEEP 2 $\n");
            pincnt = 0;
            memset(pin, '\0', sizeof(pin));
            PORTA &= ~(1 << PA3);
            _delay_ms(500);
            PORTA |= (1 << PA3);
        } else if (c == '#') {
            uart2_puts("^LED 1 2$^BEEP 2 $");
            //uart2_puts("^BEEP 2 $\n");
            pincnt = 0;
            memset(pin, '\0', sizeof(pin));
        }

    }
}


/*
 * Sends the packet in the given buffer. Basically a wrapper around
 * send_packet() which adds blinking the LED and waiting for a short time
 * (25ms).
 *
 */
static void send_reply(uint8_t *buffer) {
    struct buspkt *reply = (struct buspkt*)buffer;
    PORTC &= ~(1 << PC7);
    _delay_ms(25);
    PORTC |= (1 << PC7);
    send_packet(reply);
}

int main(int argc, char *argv[]) {
    char bufcopy[10];
    uint8_t status;

    /* enable LED so that the user knows the controller is active */
    DDRC = (1 << PC7);
    PORTC = (1 << PC7);

    /* disable watchdog */
    MCUSR &= ~(1 << WDRF);
    WDTCSR &= ~(1 << WDE);
    wdt_disable();

    /* set pins for the schließer */
    DDRA = (1 << PA2) | (1 << PA3);
    PORTA = (1 << PA2) | (1 << PA3);

    /* init serial line to the pinpad frontend */
    UBRR1H = UBRRH_VALUE;
    UBRR1L = UBRRL_VALUE;
    UCSR1B = (1 << RXCIE1) | (1 << RXEN1) | (1 << TXEN1);

    UCSR1C = (1<<UCSZ10) | (1<<UCSZ11);

    net_init();

    sei();

    _delay_ms(500);
    PORTC = 0;
    _delay_ms(500);
    PORTC = (1 << PC7);

    DBG("Pinpad firmware booted.\r\n\r\n");
    char *str = "^AEEP 1 $\n";
    int c;
    while (1) {

        if (serbuf[8] == '$') {
            strncpy(bufcopy, serbuf, sizeof(bufcopy));
            serbuf[8] = '\0';

            handle_command(bufcopy);
        }

        //_delay_ms(100);
        status = bus_status();

        if (status == BUS_STATUS_IDLE) {
            /* you could use this timeframe to do some stuff with
             * the microcontroller (check sensors etc.) */
            continue;
        }

        if (status == BUS_STATUS_MESSAGE) {
            DBG("got bus message\r\n");

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
            else if (packet->source == 0x00 &&
                memcmp(payload, "send", strlen("send")) == 0) {

                DBG("cached message was sent\r\n");
                send_reply(&rbuffer[rb_next]);
                rb_next = (rb_next + 1) % 32;
                packetcnt--;

                _delay_ms(25);
            }
            else if (memcmp(payload, "open", strlen("open")) == 0) {
                PORTA &= ~(1 << PA2);
                _delay_ms(500);
                PORTA |= (1 << PA2);
            }
            else if (memcmp(payload, "close", strlen("close")) == 0) {
                PORTA &= ~(1 << PA3);
                _delay_ms(500);
                PORTA |= (1 << PA3);
            }
            else if (memcmp(payload, "get_status", strlen("get_status")) == 0) {
                DBG("status was requested\r\n");
                /* increase packet count by one */
                fmt_packet(rbuffer, 50, MYADDRESS, "ready", 5);
                packetcnt++;
            }

            packet_done();
            continue;
        }

        if (status == BUS_STATUS_WRONG_CRC) {
            DBG("wrong crc\r\n");
            /* TODO: think of appropriate action */
            skip_byte();
            continue;
        }

        if (status == BUS_STATUS_BROKEN) {
            /* TODO: enable slow blinking of the LED */
            continue;
        }
    }
}
