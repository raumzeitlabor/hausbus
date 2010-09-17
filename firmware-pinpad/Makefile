CC = avr-gcc

MCU := atmega644p
MHZ := 16000000UL
ADDRESS := 1

# CFLAGS for ATmega
CFLAGS += -Wall
CFLAGS += -std=c99
CFLAGS += -DF_CPU=${MHZ}
CFLAGS += -mmcu=${MCU}
CFLAGS += -Os

CFLAGS += -I../lib
CFLAGS += -DMYADDRESS=${ADDRESS}

#.SILENT:

.PHONY: clean

all: firmware.hex

uart.o: ../lib/uart.c
	$(CC) $(CFLAGS) -c -o $@ $<

uart2.o: ../lib/uart2.c
	$(CC) $(CFLAGS) -c -o $@ $<

bus.o: ../lib/bus.c
	$(CC) $(CFLAGS) -c -o $@ $<

firmware.hex: main.o uart.o uart2.o bus.o
	$(CC) -mmcu=atmega644p -o $(shell basename $@ .hex).bin $^
	avr-objcopy -O ihex -R .eeprom $(shell basename $@ .hex).bin $@
	avr-size --mcu=${MCU} -C $(shell basename $@ .hex).bin

clean:
	rm -f *.o

program:
	sudo avrdude -c usbasp -p atmega644p -P usb -U flash:w:firmware.hex:i

fuses:
	sudo avrdude -c usbasp -p atmega644p -P usb -U lfuse:w:0x9f:m -U hfuse:w:0x99:m
