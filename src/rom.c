#include "rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ppu.h"
#include "cpu.h"

uint8_t* prgROM;
uint8_t* chrROM;

size_t prgSize;
size_t chrSize;

void (*romWriteByte)(uint16_t addr, uint8_t byte);
uint8_t (*romReadByte)(uint16_t addr);

void (*chrWriteByte)(uint16_t addr, uint8_t byte);
uint8_t (*chrReadByte)(uint16_t addr);

uint8_t chrReadNormal(uint16_t addr) {
	return chrROM[addr];
}

void chrWriteNormal(uint16_t addr, uint8_t byte) {
	chrROM[addr] = byte;
}

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
				printf("%02X\n", tmp);
				mmc1.chrBank0 = (tmp & 0x1F);
				/*if(chrSize == 0) { break; }
				// 1 8k bank if 0, 2 4k banks if 1
				if(mmc1.control & 0x10) {
					// dumb workaround
					memcpy(ppuRAM, chrROM + (mmc1.chrBank0 << 12), 0x1000);
				} else {
					memcpy(ppuRAM, chrROM + ((mmc1.chrBank0 & ~1) << 12), 0x2000);
				}*/
				break;
			case 2:
				//if(chrSize == 0) { break; }
				mmc1.chrBank1 = (tmp & 0x1F);
				/*if(mmc1.control & 0x10) {
					memcpy(ppuRAM+0x1000, chrROM + (mmc1.chrBank1 << 12), 0x1000);
				}*/
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

uint8_t mmc1ChrRead(uint16_t addr) {
	// probably horribly innacurate and will break for most things
	// but this works for now
	// I also haven't encountered an mmc1 rom that doesn't use chr ram
	if(chrSize == 0) {
		// chr ram
		return chrROM[addr];
	} else {
		if(addr < 0x1000) {
			return chrROM[addr + mmc1.chrBank0 * 0x1000];
		} else {
			return chrROM[addr - 0x1000 + mmc1.chrBank1 * 0x1000];
		}
	}
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

struct {
	uint8_t bankSelect;
	uint8_t prgRamWriteProtect;
	uint8_t prgRamEnable;
	uint8_t r[8];
	uint8_t irqEnable;
	uint8_t irqCounter;
	uint8_t irqReload;
	uint8_t irqReloadValue;
	uint8_t ppuA12Prev;
	uint8_t irqSignal;
} mmc3;

void mmc3Write(uint16_t addr, uint8_t byte) {
	//printf("MMC3 WRITE %04X %02X\n", addr, byte);
	switch((addr & 0xF000) >> 12) {
		case 0x8:
		case 0x9:
			if(addr & 1) {
				mmc3.r[mmc3.bankSelect&7] = byte;
			} else {
				mmc3.bankSelect = byte;
			}
			break;
		case 0xA:
		case 0xB:
			if(addr & 1) {
				mmc3.prgRamWriteProtect = (byte & 0x80) >> 7;
				mmc3.prgRamEnable = (byte & 0x40) >> 6;
			} else {
				ppu.mirror = (~byte) & 1;
			}
			break;
		case 0xC:
		case 0xD:
			if(addr & 1) {
				//printf("IRQ LATCH WRITE %02X\n", byte);
				mmc3.irqReloadValue = byte;
			} else {
				//printf("IRQ RESET WRITE\n");
				//mmc3.irqCounter = mmc3.irqReloadValue; // should be reset at the next ppu rising edge
				mmc3.irqCounter = 0;
				mmc3.irqReload = 1;
			}
			break;
		case 0xE:
		case 0xF:
			if(addr & 1) {
				//printf("IRQ DISABLE WRITE\n");
				mmc3.irqEnable = 0;
				mmc3.irqSignal = 1;
			} else {
				//printf("IRQ ENABLE WRITE\n");
				mmc3.irqEnable = 1;
			}
			break;
		default:
			exit(1);
	}
}

uint8_t mmc3Read(uint16_t addr) {
	size_t newAddr = addr - 0x8000;
	//printf("MMC3 READ %04X\n", addr);
	switch((addr & 0xF000) >> 12) {
		case 0x8:
		case 0x9:
			//printf("%02X %02X\n", mmc3.bankSelect, mmc3.r[6]);
			if(mmc3.bankSelect & 0x40) {
				return prgROM[newAddr + prgSize - 0x8000];
			} else {
				return prgROM[newAddr + mmc3.r[6] * 0x2000];
			}
			exit(1);
		case 0xA:
		case 0xB:
			//printf("%02X %02X\n", mmc3.bankSelect, mmc3.r[7]);
			newAddr -= 0x2000;
			return prgROM[newAddr + mmc3.r[7] * 0x2000];
			exit(1);
		case 0xC:
		case 0xD:
			//printf("%02X %02X\n", mmc3.bankSelect, mmc3.r[mmc3.bankSelect&3]);
			if(mmc3.bankSelect & 0x40) {
				return prgROM[newAddr + mmc3.r[6] * 0x2000];
			} else {
				return prgROM[newAddr + prgSize - 0x8000];
			}
			exit(1);
		case 0xE:
		case 0xF:
			//printf("%lx\n", prgSize);
			//printf("%lX\n", newAddr + prgSize - 0x8000);
			return prgROM[newAddr + prgSize - 0x8000];
		default:
			exit(1);
	}
}

uint8_t mmc3ChrRead(uint16_t addr) {
	//printf("MMC3 CHR READ %04X\n", addr);
	uint8_t a12 = (addr>>12) & 1;
	if(mmc3.ppuA12Prev == 0 && a12 == 1) {
		printf("%i %i %i %i %i\n", mmc3.irqCounter, mmc3.irqReload, mmc3.irqReloadValue, mmc3.irqSignal, mmc3.irqEnable);
		if(mmc3.irqCounter == 0 || mmc3.irqReload) {
			mmc3.irqCounter = mmc3.irqReloadValue;
			mmc3.irqReload = 0;
		} else {
			--mmc3.irqCounter;
		}
		if(mmc3.irqCounter == 0) {
			mmc3.irqSignal = 0;
			printf("ASDF\n");
		}
		if(mmc3.irqEnable) {
			cpu.irq = mmc3.irqSignal;
			printf("FDSWA\n");
		}
	}
	mmc3.ppuA12Prev = a12;
	if(mmc3.bankSelect & 0x80) {
		switch((addr >> 8) / 4) {
			case 0: // 0-3
				return chrROM[addr + mmc3.r[2] * 0x400];
			case 1: // 4-7
				return chrROM[addr - 0x0400 + mmc3.r[3] * 0x400];
			case 2: // 8-B
				return chrROM[addr - 0x0800+ mmc3.r[4] * 0x400];
			case 3: // C-F
				return chrROM[addr - 0x0C00 + mmc3.r[5] * 0x400];
			case 4: // 10-13
			case 5: // 14-17
				return chrROM[addr - 0x1000 + (mmc3.r[0]&0xFE) * 0x400];
			case 6: // 18-1B
			case 7: // 1C-1F
				return chrROM[addr - 0x1800 + (mmc3.r[1]&0xFE) * 0x400];
			default:
				printf("ASDFFDSASFD\n");
				exit(1);
		}
	} else {
		switch((addr >> 8) / 4) {
			case 0: // 0-3
			case 1: // 4-7
				return chrROM[addr + (mmc3.r[0]&0xFE) * 0x400];
			case 2: // 8-B
			case 3: // C-F
				return chrROM[addr - 0x0800 + (mmc3.r[1]&0xFE) * 0x400];
			case 4: // 10-13
				return chrROM[addr - 0x1000 + mmc3.r[2] * 0x400];
			case 5: // 14-17
				return chrROM[addr - 0x1400 + mmc3.r[3] * 0x400];
			case 6: // 18-1B
				return chrROM[addr - 0x1800 + mmc3.r[4] * 0x400];
			case 7: // 1C-1F
				return chrROM[addr - 0x1C00 + mmc3.r[5] * 0x400];
			default:
				printf("ASDFFDSASFD\n");
				exit(1);
		}
	}
}

void setMapper(uint16_t id) {
	switch(id) {
		case 0x00:
			romReadByte = nromRead;
			romWriteByte = mapperNoWrite;
			chrReadByte = chrReadNormal;
			chrWriteByte = mapperNoWrite;
			break;
		case 0x01:
			romReadByte = mmc1Read;
			romWriteByte = mmc1Write;
			chrReadByte = mmc1ChrRead;
			chrWriteByte = chrWriteNormal;
			mmc1.shiftReg = 0x10;
			mmc1.control = 0x0C;
			break;
		case 0x02:
			romReadByte = unromRead;
			romWriteByte = unromWrite;
			chrReadByte = chrReadNormal;
			chrWriteByte = chrWriteNormal; 
			break;
		case 0x04:
			romReadByte = mmc3Read;
			romWriteByte = mmc3Write;
			chrReadByte = mmc3ChrRead;
			chrWriteByte = chrWriteNormal;
			break;
		default:
			printf("unsupported mapper %02X\n", id);
			exit(1);
	}
}

