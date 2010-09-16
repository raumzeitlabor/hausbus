#ifndef _BUS_H
#define _BUS_H

/* structure of the packet header */
struct buspkt {
    uint8_t destination;
    uint8_t source;
    uint8_t header_chk;
    uint8_t payload_chk;
    uint8_t length_hi;
    uint8_t length_lo;
} __attribute__((packed));

enum {
	BUS_STATUS_IDLE = 0,
	BUS_STATUS_MESSAGE = 1,
	BUS_STATUS_WRONG_CRC = 2,
	BUS_STATUS_BROKEN = 3
};

enum { WAIT_TIMEOUT = 0, WAIT_DATA = 1 };

/* Functions implemented either in socket.c (simulation) or uart.c (microcontroller) */
struct buspkt *current_packet();
void send_packet(struct buspkt *pkt);
void net_init();

uint8_t bus_status();

/* TODO: move */
void uart_puts(char *str);

#endif
