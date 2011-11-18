/*
 * vim:ts=4:sw=4:expandtab
 *
 * © 2009 K. Moraw, www.helitron.de
 * © 2011 Michael Stapelberg
 *
 */
#include <stdio.h>
#include <stdint.h>

/* Generatorpolynom */
const uint32_t polynom = 0xEDB88320;

static uint32_t crc32_bytecalc(uint32_t *reg32, uint8_t byte) {
    for (uint8_t i = 0; i < 8; ++i) {
        if (((*reg32)&1) != (byte&1))
            (*reg32) = ((*reg32)>>1)^polynom; 
        else 
            (*reg32) >>= 1;
        byte >>= 1;
    }
    return ((*reg32) ^ 0xffffffff);	 		// inverses Ergebnis, MSB zuerst
}

uint32_t crc32_messagecalc(uint32_t *reg32, const uint8_t *data, uint8_t len) {
	for (uint8_t i = 0; i < len; i++) {
        crc32_bytecalc(reg32, data[i]);		// Berechne fuer jeweils 8 Bit der Nachricht
	}

	return (*reg32) ^ 0xffffffff;
}
