#ifndef _AVR_EEPROM_H
#define _AVR_EEPROM_H

void eeprom_update_block(const uint8_t *src, uint8_t *dest, size_t n);
uint8_t eeprom_read_byte(const uint8_t *src);
void eeprom_read_block(uint8_t *dest, const uint8_t *src, size_t n);

#endif
