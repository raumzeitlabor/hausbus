CC = avr-gcc

MCU := atmega644
MHZ := 12000000UL
ADDRESS := 1

# CFLAGS for ATmega
CFLAGS += -Wall
CFLAGS += -std=c99
CFLAGS += -DF_CPU=${MHZ}
CFLAGS += -mmcu=${MCU}
CFLAGS += -Os

CFLAGS += -I../lib
CFLAGS += -DMYADDRESS=${ADDRESS}
CFLAGS += -DNO_UART2

#.SILENT:

.PHONY: clean

all: firmware.hex

uart.o: ../lib/uart.c
	$(CC) $(CFLAGS) -c -o $@ $<
        
bus.o: ../lib/bus.c
	$(CC) $(CFLAGS) -c -o $@ $<

firmware.hex: main.o bus.o uart.o
	$(CC) -mmcu=atmega644 -o $(shell basename $@ .hex).bin $^
	avr-objcopy -O ihex -R .eeprom $(shell basename $@ .hex).bin $@
	avr-size --mcu=${MCU} -C $(shell basename $@ .hex).bin

clean:
	rm -f *.o
