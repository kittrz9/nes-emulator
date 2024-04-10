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
		case 0x2002:
		case 0x2003:
		case 0x2004:
		case 0x2005:
		case 0x2006:
		case 0x2007:
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
		case 0x4016:
		case 0x4017:
			printf("writing ppu/apu register %02X isn't implemented\n", addr);
			break;
		default:
			//printf("writing byte %02X to %04X\n", byte, addr);
			cpuRAM[addr] = byte;
	}
}
uint8_t ramReadByte(uint16_t addr) {
	addr = addrMap(addr);
	switch(addr) {
		case 0x2002:
			printf("ppuStatus %02X\n", ppu.status);
			return ppu.status;
		case 0x2000:
		case 0x2001:
		case 0x2003:
		case 0x2004:
		case 0x2005:
		case 0x2006:
		case 0x2007:
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
			printf("reading ppu/apu register %02X isn't implemented\n", addr);
			break;
		case 0x4016:
			printf("controller 1: %02X\n", controllers[0]);
			return controllers[0];
		case 0x4017:
			printf("controller 2: %02X\n", controllers[1]);
			return controllers[1];
		default:
			//printf("read byte %02X from %04X\n", cpuRAM[addr], addr);
			return cpuRAM[addr];
	}
	return 0;
}
