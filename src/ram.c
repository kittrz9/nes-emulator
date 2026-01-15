#include "ram.h"

#include <stdlib.h>
#include <stdio.h>

#include "cpu.h"
#include "rom.h"
#include "ppu.h"
#include "apu.h"
#include "input.h"

uint8_t cpuRAM[0x10000];

// https://www.nesdev.org/wiki/CPU_memory_map
uint16_t addrMap(uint16_t addr) {
	// weird ram mirroring
	if(addr < 0x2000) {
		addr = addr % 0x800;
	}
	if(addr >= 0x2000 && addr < 0x4000) {
		addr = 0x2000 + ((addr % 0x2000) % 8);
	}

	return addr;
}
void ramWriteByte(uint16_t addr, uint8_t byte) {
	addr = addrMap(addr);
	switch(addr) {
		case 0x2000:
			//printf("%04X %i %02X\n", cpu.pc, ppu.currentPixel / 340, byte);
			ppu.control = byte;
			break;
		case 0x2001:
			ppu.mask = byte;
			break;
		case 0x2003:
			ppu.oamAddr = byte;
			break;
		case 0x2004:
			ppu.oam[ppu.oamAddr] = byte;
			++ppu.oamAddr;
			break;
		case 0x2006:
			//*((uint8_t*)&ppu.vramAddr + (1 - ppu.w)) = byte;
			if(!ppu.w) {
				ppu.vramAddr &= 0xFF;
				ppu.vramAddr |= (byte & 0x3F) << 8;
			} else {
				ppu.vramAddr &= 0xFF00;
				ppu.vramAddr |= byte;
			}
			#ifdef DEBUG
				printf("ppu addr set to %04X\n", ppu.vramAddr);
			#endif
			ppu.w = !ppu.w;
			break;
		case 0x2007:
			#ifdef DEBUG
				printf("writing %02X into ppu %04X\n", byte, ppu.vramAddr);
			#endif
			if(ppu.vramAddr < 0x2000) {
				chrWriteByte(ppu.vramAddr, byte);
			} else {
				ppuRAM[ppu.vramAddr % 0x4000] = byte;
			}
			if(ppu.vramAddr >= 0x2000 && ppu.vramAddr <= 0x3000) {
				if(ppu.mirror & MIRROR_HORIZONTAL) {
					//printf("H%04X %04X\n", ppu.vramAddr | 0x800, ppu.vramAddr & ~(0x800));
					ppuRAM[ppu.vramAddr | 0x800] = byte;
					ppuRAM[ppu.vramAddr & ~(0x800)] = byte;
				} else {
					//printf("V%04X %04X\n", ppu.vramAddr | 0x400, ppu.vramAddr & ~(0x400));
					ppuRAM[ppu.vramAddr | 0x400] = byte;
					ppuRAM[ppu.vramAddr & ~(0x400)] = byte;
				}
			}
			// https://www.nesdev.org/wiki/PPU_palettes#Memory_Map
			if(ppu.vramAddr >= 0x3F00 && ppu.vramAddr < 0x3F20 && ppu.vramAddr % 4 == 0) {
				ppuRAM[ppu.vramAddr ^ 0x10] = byte;
			}
			ppu.vramAddr += (ppu.control & 0x04 ? 32 : 1);
			break;
		case 0x4014:
			{
				uint8_t* oamData = &cpuRAM[byte << 8];
				for(uint16_t i = 0; i < 256; ++i) {
					ppu.oam[i] = oamData[i];
				}
			}
			break;
		case 0x4016:
			controllerLatch = byte & 0x01;
			break;
		case 0x2005:
			if(!ppu.w) {
				//printf("SCROLLX %02X\n", byte);
				ppu.scrollX = byte;
			} else {
				ppu.scrollY = byte;
			}
			ppu.w = !ppu.w;
			break;
		case 0x4000: 
		case 0x4004: {
			uint8_t pulseIndex = (addr - 0x4000) / 4;
			pulseSetVolume(pulseIndex, byte&0xF);
			pulseSetLoop(pulseIndex, byte & 0x20);
			pulseSetDutyCycle(pulseIndex, byte >> 6);
			pulseSetConstVolFlag(pulseIndex, byte & 0x10);
			break;
		}
		case 0x4001:
		case 0x4005: {
			uint8_t pulseIndex = (addr - 0x4001) / 4;
			pulseSetSweepEnable(pulseIndex, byte&0xF);
			pulseSetSweepTimer(pulseIndex, (byte>>4) & 0xF);
			pulseSetSweepNegate(pulseIndex, (byte>>3) & 1);
			pulseSetSweepShift(pulseIndex, byte&0x7);
			break;
		}
		case 0x4002: 
		case 0x4006: {
			uint8_t pulseIndex = (addr - 0x4002) / 4;
			pulseSetTimerLow(pulseIndex, byte);
			break;
		}
		case 0x4003:
		case 0x4007: {
			uint8_t pulseIndex = (addr - 0x4000) / 4;
			pulseSetTimerHigh(pulseIndex, byte&7);
			pulseSetLengthCounter(pulseIndex, byte >> 3);
			break;
		}
		case 0x4008:
			triSetCounterReload(byte&0x7F);
			triSetControlFlag(byte & 0x80);
			break;
		case 0x400A:
			triSetTimerLow(byte);
			break;
		case 0x400B:
			triSetLengthCounter(byte>>3);
			triSetTimerHigh(byte&7);
			triSetReloadFlag(1);
			break;
		case 0x4017:
			apuSetFrameCounterMode(byte);
			break;
		case 0x400C:
			noiseSetVolume(byte & 0xF);
			noiseSetConstVolFlag(byte & 0x10);
			noiseSetLoop(byte & 0x20);
			break;
		case 0x2002:
		case 0x4009:
		case 0x400D:
			#ifdef DEBUG
				printf("writing ppu/apu register %02X isn't implemented\n", addr);
			#endif
			break;
		case 0x4010:
			dmcSetIrqEnable(byte >> 7);
			dmcSetLoop((byte >> 6) & 1);
			dmcSetRate(byte & 0xF);
			break;
		case 0x4011:
			dmcDirectLoad(byte & 0x7F);
			break;
		case 0x4012:
			dmcSetSampleAddress(byte);
			break;
		case 0x4013:
			dmcSetSampleLength(byte);
			break;
		case 0x400E:
			noiseSetTimer(byte&0xF);
			noiseSetMode(byte>>7);
			break;
		case 0x400F:
			noiseSetLengthcounter(byte>>3);
			break;
		case 0x4015:
			// will need to update the other channel's enable flags once those are implemented
			pulseSetEnableFlag(0, byte & 1);
			pulseSetEnableFlag(1, byte & 2);
			triSetEnableFlag(byte & 4);
			noiseSetEnableFlag(byte & 8);
			break;
		default:
			#ifdef DEBUG
				printf("writing byte %02X to %04X\n", byte, addr);
			#endif
			if(addr >= 0x8000) {
				romWriteByte(addr, byte);
			} else {
				cpuRAM[addr] = byte;
			}
	}
}

uint8_t ramReadByte(uint16_t addr) {
	addr = addrMap(addr);
	switch(addr) {
		case 0x2002:
			{
				// clear vblank flag after it's read
				uint8_t tmp = ppu.status;
				ppu.status &= ~(0x80);
				ppu.w = 0;
				return tmp;
			}
		case 0x2007: {
			// https://www.nesdev.org/wiki/PPU_registers#The_PPUDATA_read_buffer
			uint8_t v = ppu.readBuffer;
			ppu.readBuffer = ppuRAM[ppu.vramAddr%0x4000];
			if(ppu.control & 0x4) {
				ppu.vramAddr += 32;
			} else {
				ppu.vramAddr += 1;
			}
			return v;
		}
		case 0x2000:
		case 0x2001:
		case 0x2003:
		case 0x2004:
		case 0x2005:
		case 0x2006:
		case 0x4000:
		case 0x4001:
		case 0x4002:
		case 0x4003:
		case 0x4004:
		case 0x4005:
		case 0x4006:
		case 0x4007:
		case 0x4008:
		case 0x4009:
		case 0x4010:
		case 0x4011:
		case 0x4012:
		case 0x4013:
		case 0x4014:
			#ifdef DEBUG
				printf("reading ppu/apu register %02X isn't implemented\n", addr);
			#endif
			break;
		case 0x4015:
			return apuGetStatus();
			break;
		case 0x4016:
			return pollController(0);
		case 0x4017:
			return pollController(1);;
		default:
			if(addr >= 0x8000) {
				#ifdef DEBUG
					printf("read byte %02X from ROM %04X\n", romReadByte(addr), addr);
				#endif
				return romReadByte(addr);
			} else {
				#ifdef DEBUG
					printf("read byte %02X from %04X\n", cpuRAM[addr], addr);
				#endif
				return cpuRAM[addr];
			}
	}
	return 0;
}
