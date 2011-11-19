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
#include "crc32.h"
#include "uart2.h"

//#define DEBUG
#ifdef DEBUG
  #define DBG(x) uart2_puts_P(PSTR(x))
#else
  #define DBG(x) (void)0
#endif

enum { LOCKED_PERFECT = 0, LOCKED_OPEN, NOT_LOCKED } lock_state_t;
volatile uint16_t pwmvalue;

/* seite 84 */
static char pin[11];
static uint8_t pincnt = 0;
static volatile char serbuf[64];
static volatile uint8_t sercnt = 0;

static uint8_t packetcnt = 0;
static uint8_t lbuffer[32];

static uint8_t old_pinb[10];
static uint8_t op_current = 0;
static uint16_t check_pinb = 0;

/* Outgoing packet buffer. The message header size is 6 bytes, let's assume a
 * typical message length of 12 bytes. Thereforce, we can fit about 28 messages
 * into this buffer. It will be cleared every second. */
buspkt_full(10);
static struct buspkt_10 rbuffer[32];
static uint8_t rb_current = 0;
static uint8_t rb_next = 0;

static uint8_t get_state() {
    bool pb2 = (PINB & (1 << PB2));
    bool pb3 = (PINB & (1 << PB3));
    bool pb4 = (PINB & (1 << PB4));

    if (!pb2 && pb3 && !pb4)
        return LOCKED_PERFECT;
    else if (pb2 && !pb3 && pb4)
        return NOT_LOCKED;
    else return LOCKED_OPEN;
}

/*
 * Tries to send a message to message group 50 from MYADDRESS
 *
 */
static bool senddata(const char *msg, int len) {
    /* If the packet which is next to be fetched (rb_next) is at the same
     * position where we want to write to, we have to abort */
    if (((rb_current + 1) % 32) == rb_next)
        return false;

    fmt_packet((uint8_t*)&rbuffer[rb_current], 50, MYADDRESS, (void*)msg, len);
    rb_current = (rb_current + 1) % 32;
    packetcnt++;
    return true;
}

static bool sendmsg(const char *msg) {
    return senddata(msg, strlen(msg));
}

static void send_pwm_state(uint8_t new_state) {
    uint8_t sensor1;
    uint8_t sensor2;
    uint8_t state;

    /* Das letzte Bit gibt den Status von Sensor 2 an */
    sensor2 = (new_state & (1 << 0));
    /* Das vorletzte Bit den Status von Sensor 1 */
    sensor1 = (new_state & (1 << 1));
    /* Das dritte und vierte Bit geben den Aktionscode an */
    state = (new_state & ((1 << 2) | (1 << 3))) >> 2;
    char msg[] = "SRAW aabcd";
    msg[5] = (new_state & (1 << 3));
    msg[6] = (new_state & (1 << 2));
    msg[7] = sensor1;
    msg[8] = sensor2;
    msg[9] = (PINB & (1 << PB0));
    senddata(msg, 10);
    if (sensor1 && !sensor2)
        sendmsg("STAT lock");
        /* locked */
    else if (!sensor1 && sensor2)
        sendmsg("STAT open");
    else sendmsg("STAT broke");
}

static void send_state() {
    uint16_t snap = pwmvalue;
    uint8_t c;
    char buf[128];
    for (c = 1; c < 17; c++) {
        if (snap > ((3855 * c) - 500) &&
            snap < ((3855 * c) + 500)) {
            send_pwm_state(c);
            return;
        }
    }
    sendmsg("SRAW bork");
}

void lock_door() {
    /* Ansteuerung des HomeTec++ */
    int freq = 8 * 3855;
    OCR1BH = (freq & 0xFF00) >> 8;
    OCR1BL = (freq & 0x00FF);
    _delay_ms(500);
    freq = 1 * 3855;
    OCR1BH = (freq & 0xFF00) >> 8;
    OCR1BL = (freq & 0x00FF);
    /* laut piepsen */
    //if (get_state() != LOCKED_PERFECT) {
       // uart2_puts("^BEEP 2 $\n");
       // _delay_ms(500);
       // uart2_puts("^BEEP 2 $\n");

        uart2_puts("^LED 2 2$\n");
    //}
}

void unlock_door() {
    int freq = 4 * 3855;
    OCR1BH = (freq & 0xFF00) >> 8;
    OCR1BL = (freq & 0x00FF);
    _delay_ms(500);
    freq = 1 * 3855;
    OCR1BH = (freq & 0xFF00) >> 8;
    OCR1BL = (freq & 0x00FF);
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

volatile uint16_t lowvalue;
volatile uint16_t highvalue;

ISR(TIMER1_CAPT_vect) {
    /* Schauen, bei welcher Flanke wir gerade sind */
    TIMSK1 &= ~(1 << ICIE1);

    /* Steigende Flanke */
    if (TCCR1B & (1 << ICES1)) {
        /* Wert wegspeichern */
        lowvalue = ICR1L;
        lowvalue |= (ICR1H << 8);

        /* flanke ändern */
        TCCR1B &= ~(1 << ICES1);
    } else {
        /* Fallende Flanke */
        highvalue = ICR1L;
        highvalue |= (ICR1H << 8);

        /* Zählerüberlauf */
        if (highvalue < lowvalue) {
            pwmvalue = 0xFFFF - lowvalue + highvalue;
        } else {
            pwmvalue = highvalue - lowvalue;
        }

        /* flanke ändern */
        TCCR1B |= (1 << ICES1);
    }

    TIMSK1 |= (1 << ICIE1);
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
        if (c != '#') {
            uart2_puts("^LED 2 1$\n");
            uart2_puts("^BEEP 1 $\n");
        }
        if (pincnt == 7 && strncmp(pin, "777#", 5) == 0) {
            uart2_puts("^LED 2 2$");
            uart2_puts("^BEEP 2 $\n");
            pincnt = 0;
            memset(pin, '\0', sizeof(pin));
            unlock_door();
            sendmsg("OPEN pin");
        } else if (pincnt == 4 && strncmp(pin, "666#", 5) == 0) {
            uart2_puts("^LED 2 2$");
            uart2_puts("^BEEP 2 $\n");
            pincnt = 0;
            memset(pin, '\0', sizeof(pin));
            lock_door();
            sendmsg("LOCK pin");
        } else if (pincnt == 5 && strncmp(pin, "0000#", 5) == 0) {

            uart2_puts("^LED 2 2$");
            uart2_puts("^BEEP 2 $\n");
            pincnt = 0;
            memset(pin, '\0', sizeof(pin));
    int freq = 16 * 3855;
    OCR1BH = (freq & 0xFF00) >> 8;
    OCR1BL = (freq & 0x00FF);
                sendmsg("reset PIN");
                _delay_ms(250);
    freq = 1 * 3855;
    OCR1BH = (freq & 0xFF00) >> 8;
    OCR1BL = (freq & 0x00FF);
        } else if (c == '#') {
            uart2_puts("^LED 1 2$^BEEP 2 $");
            //uart2_puts("^BEEP 2 $\n");
            pincnt = 0;
            memset(pin, '\0', sizeof(pin));
            sendmsg("DISCARD");
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

    /* set pins for status */
    DDRB = 0;
    PORTB = (1 << PB0) | (1 << PB2) | (1 << PB3) | (1 << PB4);

    /* init serial line to the pinpad frontend */
    UBRR1H = UBRRH_VALUE;
    UBRR1L = UBRRL_VALUE;
    UCSR1B = (1 << RXCIE1) | (1 << RXEN1) | (1 << TXEN1);

    UCSR1C = (1<<UCSZ10) | (1<<UCSZ11);

    /* PWM zur Kommunikation mit dem HomeTec++ initialisieren */
    DDRD |= (1 << PD4);
    TCCR1A = (1 << COM1B1) | (1 << WGM11) | (1 << WGM10);
    TCCR1B = (1 << ICNC1) | (1 << ICES1) | (1 << WGM13) | (1 << WGM12) | (1 << CS11) | (1 << CS10);
    OCR1AH = 0xFF;
    OCR1AL = 0xFF;
    TCNT1H = 0;
    TCNT1L = 0;

    TIMSK1 = (1 << ICIE1);

    int freq = 1 * 3855;
    OCR1BH = (freq & 0xFF00) >> 8;
    OCR1BL = (freq & 0x00FF);

    net_init();

    sei();

    _delay_ms(500);
    PORTC = 0;
    _delay_ms(500);
    PORTC = (1 << PC7);

    DBG("Pinpad firmware booted.\r\n\r\n");
    uint8_t c;
    bool pinb_changed;
    while (1) {
        /* Handle serial input */
        if (serbuf[8] == '$') {
            strncpy(bufcopy, (const char*)serbuf, sizeof(bufcopy));
            serbuf[8] = '\0';

            handle_command(bufcopy);
        }

        /* Check if the sensors have changed */
        if (check_pinb++ == 0) {
            pinb_changed = true;
            for (c = 0; c < 10; c++) {
                if (c != op_current && old_pinb[c] != PINB) {
                    pinb_changed = false;
                    break;
                }
            }
            if (old_pinb[op_current] == PINB)
                pinb_changed = false;

            old_pinb[op_current] = PINB;
            op_current = (op_current + 1) % 10;

            if (pinb_changed)
                send_state();
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
                send_reply((uint8_t*)&rbuffer[rb_next]);
                rb_next = (rb_next + 1) % 32;
                packetcnt--;

                _delay_ms(25);
            }
            else if (payload[0] == 'E') {
                /* EEPROM write command:
                 * 'E' <uint16_t dest><uint8_t len><bytes><uint32_t crc32> */

                /* Skip the 'E' */
                payload++;

                /* Store the EEPROM destination address. */
                uint16_t dest = (payload[0] << 8) | payload[1];
                payload += 2;

                /* Store the number of bytes in this packet. */
                uint8_t len = payload[0];
                payload++;

                /* Calculate the checksum over the whole packet and see if it
                 * matches the stored checksum. */
                uint32_t reg32 = 0xffffffff;
                uint32_t crc32 = crc32_messagecalc(&reg32,
                        (uint8_t*)packet + sizeof(struct buspkt),
                        len + 1 + 2 + 1);

                uint8_t *stored_crc = payload + len;

                if (((crc32 >> 24) & 0xFF) != stored_crc[0] ||
                    ((crc32 >> 16) & 0xFF) != stored_crc[1] ||
                    ((crc32 >>  8) & 0xFF) != stored_crc[2] ||
                    ( crc32        & 0xFF) != stored_crc[3]) {
                    /* The CRC32 did not match, send an error and discard this
                     * packet. */
                    sendmsg("EEP CRCERR");
                } else {
                    /* The CRC32 did match. Write to the EEPROM and acknowledge
                     * the write. */
                    eeprom_update_block(payload, (uint8_t*)(dest), len);
                    sendmsg("EEP ACK");
                }
            }
            else if (memcmp(payload, "open", strlen("open")) == 0) {
                unlock_door();
                sendmsg("OPEN bus");
            }
            else if (memcmp(payload, "close", strlen("close")) == 0) {
                lock_door();
                sendmsg("LOCK bus");

            } else if (memcmp(payload, "nop", strlen("nop")) == 0) {
    int freq = 1 * 3855;
    OCR1BH = (freq & 0xFF00) >> 8;
    OCR1BL = (freq & 0x00FF);
                sendmsg("NOP bus");
            } else if (memcmp(payload, "reset", strlen("reset")) == 0) {

    int freq = 16 * 3855;
    OCR1BH = (freq & 0xFF00) >> 8;
    OCR1BL = (freq & 0x00FF);
                sendmsg("reset BUS");
                _delay_ms(250);
    freq = 1 * 3855;
    OCR1BH = (freq & 0xFF00) >> 8;
    OCR1BL = (freq & 0x00FF);
            } else if (memcmp(payload, "status", strlen("status")) == 0) {
                send_state();
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
