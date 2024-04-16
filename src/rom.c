#include "rom.h"

#include <stdio.h>
#include <stdlib.h>

uint8_t* prgROM;
uint8_t* chrROM;

size_t prgSize;
size_t chrSize;

void (*romWriteByte)(uint16_t addr, uint8_t byte);
uint8_t (*romReadByte)(uint16_t addr);

void mapperNop() {
	return;
}

uint8_t nromRead(uint16_t addr) {
	addr -= 0x8000;
	if(addr >= 0x4000 && prgSize <= 0x4000) { addr -= 0x4000; }
	return prgROM[addr];
}

void setMapper(uint16_t id) {
	// could probably put all this into a look up table
	switch(id) {
		case 0x00:
			romReadByte = nromRead;
			romWriteByte = mapperNop;
			break;
		default:
			printf("unsupported mapper %02X\n", id);
			exit(1);
	}
}

