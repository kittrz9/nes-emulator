#include "ram.h"

#include <stdlib.h>
#include <stdio.h>

#include "ppu.h"
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
			*((uint8_t*)&ppu.vramAddr + (1 - ppu.w)) = byte;
			#ifdef DEBUG
				printf("ppu addr set to %04X\n", ppu.vramAddr);
			#endif
			ppu.w = !ppu.w;
			break;
		case 0x2007:
			#ifdef DEBUG
				printf("writing %02X into ppu %04X\n", byte, ppu.vramAddr);
			#endif
			ppuRAM[ppu.vramAddr % 0x4000] = byte;
			if(ppu.vramAddr >= 0x2000 && ppu.vramAddr <= 0x2C00) {
				uint16_t newAddr = ppu.vramAddr;
				if(ppu.mirror == MIRROR_HORIZONTAL) {
					newAddr ^= 0x800;
				} else {
					newAddr ^= 0x400;
				}
				ppuRAM[newAddr % 0x4000] = byte;
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
			//printf("latch set to %02X\n", byte & 0x01);
			break;
		case 0x2005:
			if(!ppu.w) {
				ppu.scrollX = byte;
			} else {
				ppu.scrollY = byte;
			}
			ppu.w = !ppu.w;
			break;
		case 0x2002:
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
		case 0x4015:
		case 0x4017:
			#ifdef DEBUG
				printf("writing ppu/apu register %02X isn't implemented\n", addr);
			#endif
			break;
		default:
			#ifdef DEBUG
				printf("writing byte %02X to %04X\n", byte, addr);
			#endif
			cpuRAM[addr] = byte;
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
				return tmp;
			}
		case 0x2007:
			return ppuRAM[ppu.vramAddr%0x4000];
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
		case 0x4015:
			#ifdef DEBUG
				printf("reading ppu/apu register %02X isn't implemented\n", addr);
			#endif
			break;
		case 0x4016:
			//printf("controller 1: %02X\n", controllers[0].buttons);
			return pollController(0);
		case 0x4017:
			//printf("controller 2: %02X\n", controllers[1].buttons);
			return pollController(1);;
		default:
			#ifdef DEBUG
				printf("read byte %02X from %04X\n", cpuRAM[addr], addr);
			#endif
			return cpuRAM[addr];
	}
	return 0;
}
