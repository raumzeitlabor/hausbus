#ifndef _BUS_H
#define _BUS_H

/* structure of the packet header */
struct buspkt {
    uint8_t destination;
    uint8_t source;
    uint8_t checksum;
    uint8_t length_hi;
    uint8_t length_lo;
};

enum {
	BUS_STATUS_IDLE = 0,
	BUS_STATUS_MESSAGE = 1,
	BUS_STATUS_BROKEN = 2
};

enum { WAIT_TIMEOUT = 0, WAIT_DATA = 1 };

/* Functions implemented either in socket.c (simulation) or uart.c (microcontroller) */
int wait_for_data(int ms);
struct buspkt *read_packet();
void send_packet(struct buspkt *pkt);
void net_init();

uint8_t bus_status();

/* TODO: move */
uint8_t getbyte();
void uart_puts(char *str);

#endif
