CC = avr-gcc

MCU := atmega128
MHZ := 16000000UL
ADDRESS := 0

# CFLAGS for ATmega
CFLAGS += -Wall
CFLAGS += -std=c99
CFLAGS += -DF_CPU=${MHZ}
CFLAGS += -mmcu=${MCU}
CFLAGS += -Os

CFLAGS += -I../lib
CFLAGS += -DMYADDRESS=${ADDRESS}
CFLAGS += -DBUSMASTER
CFLAGS += -DENC28J60_REV4_WORKAROUND

#.SILENT:

.PHONY: clean program

all: firmware.hex

uart.o: uart.c
	$(CC) $(CFLAGS) -c -o $@ $<

bus.o: ../lib/bus.c
	$(CC) $(CFLAGS) -c -o $@ $<

firmware.hex: main.o spi.o enc28j60.o enc28j60_process.o enc28j60_transmit.o uart.o icmpv6.o bus.o
	$(CC) $(CFLAGS) -o $(shell basename $@ .hex).bin $^
	avr-objcopy -O ihex -R .eeprom $(shell basename $@ .hex).bin $@
	avr-size --mcu=${MCU} -C $(shell basename $@ .hex).bin

clean:
	rm -f *.o

program:
	sudo avrdude -c usbasp -p atmega644 -P usb -U flash:w:firmware.hex:i
