#include "ram.h"

#include <stdlib.h>
#include <stdio.h>

#include "cpu.h"
#include "rom.h"
#include "ppu.h"
#include "apu.h"
#include "input.h"
#include "dma.h"

uint8_t cpuRAM[0x800];

uint8_t prgRAM[0x2000];

uint8_t ramDataBus;
uint8_t ppuDataBus;

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
	ramDataBus = byte;
	addr = addrMap(addr);
	// jank, needs to be changed eventually
	if(rom.isNSF && addr >= 0x5FF8 && addr <= 0x5FFF) {
		romWriteByte(addr, byte);
		return;
	}

	if(rom.prgRAMEnabled && addr >= 0x6000 && addr < 0x8000) {
		prgRAM[addr - 0x6000] = byte;
		return;
	} else if(addr >= 0x6000) {
		romWriteByte(addr, byte);
		return;
	} else if(addr < 0x800) {
		cpuRAM[addr] = byte;
		return;
	}
	switch(addr) {
		case 0x2000:
			ppuDataBus = byte;
			//printf("%04X %i %02X\n", cpu.pc, ppu.currentPixel / 340, byte);
			if(byte & PPU_CTRL_ENABLE_VBLANK && (ppu.control & PPU_CTRL_ENABLE_VBLANK) == 0 && ppu.status & PPU_STATUS_VBLANK) {
				ppu.nmiHappened = 0;
			}

			ppu.control = byte;
			ppu.t &= ~0xC00;
			ppu.t |= (byte & 3) << 10;
			break;
		case 0x2001:
			ppuDataBus = byte;
			ppu.mask = byte;
			break;
		case 0x2002:
			// read only
			ppuDataBus = byte;
			break;
		case 0x2003:
			ppuDataBus = byte;
			ppu.oamAddr = byte;
			break;
		case 0x2004:
			ppuDataBus = byte;
			ppu.oam[ppu.oamAddr] = byte;
			++ppu.oamAddr;
			break;
		case 0x2006:
			ppuDataBus = byte;
			//*((uint8_t*)&ppu.vramAddr + (1 - ppu.w)) = byte;
			if(!ppu.w) {
				ppu.t &= ~0xFF00;
				ppu.t |= (byte & 0x3F) << 8;
			} else {
				ppu.t &= 0xFF00;
				ppu.t |= byte;
				ppu.vramAddr = ppu.t;
			}
			#ifdef DEBUG
				printf("ppu addr set to %04X\n", ppu.vramAddr);
			#endif
			ppu.w = !ppu.w;
			break;
		case 0x2007:
			ppuDataBus = byte;
			#ifdef DEBUG
				printf("writing %02X into ppu %04X\n", byte, ppu.vramAddr);
			#endif
			ppuRAMWrite(ppu.vramAddr % 0x4000, byte);
			ppu.vramAddr += (ppu.control & 0x04 ? 32 : 1);
			break;
		case 0x4014:
			ppuDataBus = byte;
			oamDMAStart(byte);
			break;
			/*{
				for(uint16_t i = 0; i < 256; ++i) {
					// could potentially do wacky stuff if it gets into the apu/ppu register areas
					ppu.oam[i] = ramReadByte((byte << 8) + i);
				}
			}
			break;*/
		case 0x4016:
			controllerLatch = byte & 0x01;
			if(controllerLatch) {
				controllers[0].shiftRegister = controllers[0].buttons;
				controllers[1].shiftRegister = controllers[1].buttons;
			}
			break;
		case 0x2005:
			ppuDataBus = byte;
			if(!ppu.w) {
				//printf("SCROLLX %02X\n", byte);
				ppu.t &= ~0x1F;
				ppu.t |= byte >> 3;
				ppu.x = byte & 0x7;
			} else {
				ppu.t &= ~0x3E0;
				ppu.t |= (byte & 0xF8) << 2;
				ppu.t &= ~0x7000;
				ppu.t |= (byte & 0x7) << 12;
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
			pulseSetSweepEnable(pulseIndex, byte >> 7);
			pulseSetSweepTimer(pulseIndex, (byte>>4) & 0x7);
			pulseSetSweepNegate(pulseIndex, (byte>>3) & 1);
			pulseSetSweepShift(pulseIndex, byte & 0x7);
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
			dmcSetEnableFlag(byte & 0x10);
			break;
		default:
			// open bus
			break;
	}
}

uint8_t ramReadByte(uint16_t addr) {
	addr = addrMap(addr);
	if(rom.prgRAMEnabled && addr >= 0x6000 && addr < 0x8000) {
		ramDataBus = prgRAM[addr - 0x6000];;
	} else if(addr >= 0x6000) {
		ramDataBus = romReadByte(addr);
	} else if(addr < 0x800) {
		ramDataBus = cpuRAM[addr];
	} else {
		switch(addr) {
			case 0x2002:
				{
					// clear vblank flag after it's read
					uint8_t tmp = ppu.status | (ppuDataBus & 0x1F);
					ppuDataBus |= ppu.status & 0xE0;
					ppu.status &= ~PPU_STATUS_VBLANK;
					ppu.w = 0;
					ramDataBus = tmp;
					ppuDataBus &= 0x1F;
					break;
				}
			case 0x2007: {
				// https://www.nesdev.org/wiki/PPU_registers#The_PPUDATA_read_buffer
				uint8_t v = ppu.readBuffer;
				ppuDataBus = ppu.readBuffer;
				// blindly trusting accuracycoin here, I think this is dependant on what revision of the ppu you have
				if(ppu.vramAddr < 0x3f00) {
					ppu.readBuffer = ppuRAMRead(ppu.vramAddr);
				} else {
					v = ppuRAMRead(ppu.vramAddr);
					ppu.readBuffer = ppuRAMRead(ppu.vramAddr - 0x1000);
				}
				if(ppu.control & 0x4) {
					ppu.vramAddr += 32;
				} else {
					ppu.vramAddr += 1;
				}
				ramDataBus = v;
				break;
			}
			case 0x2004:
			case 0x2000:
			case 0x2001:
			case 0x2003:
			case 0x2005:
			case 0x2006:
			case 0x4014:
				ramDataBus = ppuDataBus;
				break;
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
				#ifdef DEBUG
					printf("reading ppu/apu register %02X isn't implemented\n", addr);
				#endif
				break;
			case 0x4015:
				// does not update the data bus
				return apuGetStatus() | (ramDataBus & 0x20);
			case 0x4016:
				ramDataBus &= 0xE0;
				ramDataBus |= pollController(0) & 0x1F;
				break;
			case 0x4017:
				ramDataBus &= 0xE0;
				ramDataBus |= pollController(1) & 0x1F;
				break;
			default:
				// open bus
				//printf("OPEN BUS! %04X %02X\n", addr, ramDataBus);
				break;
		}
	}
	return ramDataBus;
}
