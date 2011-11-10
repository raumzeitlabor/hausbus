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

/* TODO: style */

unsigned long reg32 = 0xffffffff; 		// Schieberegister
 
unsigned long crc32_bytecalc(unsigned char byte)
{
int i;
unsigned long polynom = 0xEDB88320;		// Generatorpolynom

    for (i=0; i<8; ++i)
	{
        if ((reg32&1) != (byte&1))
             reg32 = (reg32>>1)^polynom; 
        else 
             reg32 >>= 1;
		byte >>= 1;
	}
	return reg32 ^ 0xffffffff;	 		// inverses Ergebnis, MSB zuerst
}

unsigned long crc32_messagecalc(unsigned char *data, int len)
{
int i;
reg32 = 0xffffffff;

	for(i=0; i<len; i++) {
		crc32_bytecalc(data[i]);		// Berechne fuer jeweils 8 Bit der Nachricht
	}
	return reg32 ^ 0xffffffff;
}

#if 0
int main()
{
unsigned char data[] = {"123456789"};
unsigned long crc32;

	reg32 = 0xffffffff;					// Initialisiere Shift-Register mit Startwert
	crc32 = crc32_messagecalc(data,9);
	printf("CRC32 = %lx\n",crc32);
	
	return 0;
}
#endif
