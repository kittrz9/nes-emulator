#include "rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ppu.h"

uint8_t* prgROM;
uint8_t* chrROM;

size_t prgSize;
size_t chrSize;

void (*romWriteByte)(uint16_t addr, uint8_t byte);
uint8_t (*romReadByte)(uint16_t addr);

void mapperNoWrite(uint16_t addr, uint8_t byte) {
	// avoid unused variable warning
	(void)addr;
	(void)byte;
	return;
}

uint8_t nromRead(uint16_t addr) {
	addr -= 0x8000;
	if(addr >= 0x4000 && prgSize <= 0x4000) { addr -= 0x4000; }
	return prgROM[addr];
}

// https://www.nesdev.org/wiki/MMC1
struct {
	uint8_t shiftReg;
	uint8_t control;
	uint8_t chrBank0;
	uint8_t chrBank1;
	uint8_t prgBank;
} mmc1;

void mmc1Write(uint16_t addr, uint8_t byte) {
	if(byte & 0x80) {
		mmc1.shiftReg = 0x10;
		return;
	}
	uint8_t tmp = (mmc1.shiftReg >> 1) | ((byte & 1) << 4);
	if(mmc1.shiftReg & 1) {
		switch((addr >> 13) & 0x3) {
			case 0:
				mmc1.control = tmp;
				ppu.mirror = ~tmp & 0x1;
				break;
			case 1:
				mmc1.chrBank0 = tmp;
				// 1 8k bank if 0, 2 4k banks if 1
				if(mmc1.control & 0x10) {
					// dumb workaround
					memcpy(ppuRAM, chrROM + (mmc1.chrBank0 << 12), 0x1000);
				} else {
					memcpy(ppuRAM, chrROM + (mmc1.chrBank0 << 12), 0x2000);
				}
				break;
			case 2:
				mmc1.chrBank1 = tmp;
				if(mmc1.control & 0x10) {
					memcpy(ppuRAM+0x1000, chrROM + (mmc1.chrBank1 << 12), 0x1000);
				}
				break;
			case 3:
				mmc1.prgBank = tmp;
				break;
		}
		tmp = 0x10;
	}
	mmc1.shiftReg = tmp;
}

uint8_t mmc1Read(uint16_t addr) {
	uint32_t newAddr = addr - 0x8000;
	switch((mmc1.control & 0x0C) >> 2) {
		case 0:
		case 1:
			// 32k mode
			newAddr += (mmc1.prgBank & 0x0E) << 16;
			break;
		case 2:
			// first bank locked
			if(newAddr >= 0x4000) {
				newAddr += mmc1.prgBank << 14;
			}
			break;
		case 3:
			// last bank locked
			if(newAddr < 0x4000) {
				newAddr += mmc1.prgBank << 14;
			} else {
				newAddr += prgSize - 0x8000;
			}
			break;
	}
	return prgROM[newAddr];
}

uint8_t unromBank = 0;

void unromWrite(uint16_t addr, uint8_t byte) {
	(void)addr;
	unromBank = byte;
	return;
}

uint8_t unromRead(uint16_t addr) {
	addr -= 0x8000;
	if(addr < 0x4000) {
		return prgROM[addr + 0x4000 * unromBank];
	} else {
		return prgROM[addr + prgSize - 0x8000];
	}
}

void setMapper(uint16_t id) {
	switch(id) {
		case 0x00:
			romReadByte = nromRead;
			romWriteByte = mapperNoWrite;
			break;
		case 0x01:
			romReadByte = mmc1Read;
			romWriteByte = mmc1Write;
			mmc1.shiftReg = 0x10;
			mmc1.control = 0x0C;
			break;
		case 0x02:
			romReadByte = unromRead;
			romWriteByte = unromWrite;
			break;
		default:
			printf("unsupported mapper %02X\n", id);
			exit(1);
	}
}

