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

/* macro to create a data type with the right payload_size */
#define buspkt_full(payload_size) \
	struct buspkt_ ## payload_size { \
	    uint8_t destination; \
	    uint8_t source; \
	    uint8_t header_chk; \
	    uint8_t payload_chk; \
	    uint8_t length_hi; \
	    uint8_t length_lo; \
		uint8_t payload[payload_size]; \
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
void skip_byte();
void packet_done();

/* TODO: move */
void uart_puts(char *str);

void fmt_packet(uint8_t *buffer, uint8_t destination, uint8_t source, void *payload, uint8_t len);

#endif
