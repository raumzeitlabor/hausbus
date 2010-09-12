/* Compatibility header for Ethersex */
#ifndef _COMPAT_H

#define _PORT_CHAR(character) PORT ## character
#define PORT_CHAR(character) _PORT_CHAR(character)

#define PIN_CLEAR(pin) PORT_CHAR(pin ## _PORT) &= ~_BV(pin ## _PIN)
#define PIN_SET(pin) PORT_CHAR(pin ## _PORT) |= _BV(pin ## _PIN)

#define SPI_CS_HARDWARE_PORT B
#define SPI_CS_HARDWARE_PIN 4
#define HAVE_SPI_CS_HARDWARE 1

#define SPI_CS_NET_PORT SPI_CS_HARDWARE_PORT
#define SPI_CS_NET_PIN SPI_CS_HARDWARE_PIN
#define HAVE_SPI_CS_NET HAVE_SPI_CS_HARDWARE

#define NET_MAX_FRAME_LENGTH 1500

#define LO8(x)  ((uint8_t)(x))
#define HI8(x)  ((uint8_t)((x) >> 8))

#define UIP_BUFSIZE 400

extern uint16_t uip_len;
extern uint8_t uip_buf[UIP_BUFSIZE+2];

#endif
