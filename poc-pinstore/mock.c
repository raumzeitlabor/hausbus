/*
 * vim:ts=4:sw=4:expandtab
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

static uint8_t *eeprom_buffer;

void net_init() {
    eeprom_buffer = malloc(49);
}

void uart_puts(char *str) {
    printf("[UART] %s", str);
}

void eeprom_update_block(const uint8_t *src, uint8_t *dest, size_t n) {
    printf("[EEPROM] update address %p from %p, n=%d\n", dest, src, n);
    memcpy(eeprom_buffer + (int)dest, src, n);
}

uint8_t eeprom_read_byte(const uint8_t *src) {
    printf("[EEPROM] read address %p\n", src);
    return eeprom_buffer[(int)src];
}

void eeprom_read_block(uint8_t *dest, const uint8_t *src, size_t n) {
    printf("[EEPROM] read block src=%p, dest=%p, n=%d\n", src, dest, n);
    memcpy(dest, eeprom_buffer + (int)src, n);
}
