/*
 * vim:ts=4:sw=4:expandtab
 */
/*
Grundlagen zu diesen Funktionen wurden der Webseite:
http://www.cs.waikato.ac.nz/~312/crc.txt
entnommen (A PAINLESS GUIDE TO CRC ERROR DETECTION ALGORITHMS)

Algorithmus entsprechend CRC32 fuer Ethernet

Startwert FFFFFFFF, LSB zuerst
Im Ergebnis kommt MSB zuerst und alle Bits sind invertiert

Das Ergebnis wurde geprueft mit dem CRC-Calculator:
http://www.zorc.breitbandkatze.de/crc.html
(Einstellung Button CRC-32 waehlen, Daten eingeben und calculate druecken)

Autor: K.Moraw, www.helitron.de, Oktober 2009
*/

#include <stdio.h>
#include <stdint.h>

static uint32_t crc32_bytecalc(uint32_t *reg32, uint8_t byte) {
    int i;
    uint32_t polynom = 0xEDB88320;		// Generatorpolynom

    for (i = 0; i < 8; ++i) {
        if (((*reg32)&1) != (byte&1))
            (*reg32) = ((*reg32)>>1)^polynom; 
        else 
            (*reg32) >>= 1;
        byte >>= 1;
    }
    return ((*reg32) ^ 0xffffffff);	 		// inverses Ergebnis, MSB zuerst
}

uint32_t crc32_messagecalc(uint32_t *reg32, const uint8_t *data, int len) {
    int i;

	for (i = 0; i < len; i++) {
        crc32_bytecalc(reg32, data[i]);		// Berechne fuer jeweils 8 Bit der Nachricht
	}

	return (*reg32) ^ 0xffffffff;
}
