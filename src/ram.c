#include "ram.h"

#include <stdlib.h>
#include <stdio.h>

#include "cpu.h"
#include "rom.h"
#include "ppu.h"
#include "apu.h"
#include "input.h"

uint8_t cpuRAM[0x800];

uint8_t prgRAM[0x2000];

// janky workaround to not having cycle accurate timings to things
// probably still horribly inaccurate
// could maybe change this to a priority queue system and make it work with nmis and stuff like that
typedef enum {
	EVENT_NONE,

	EVENT_CPU_RAM_WRITE,
	EVENT_PRG_RAM_WRITE, // ideally prg ram should be handled by the mapper implementation, since prg ram doesn't have to be mapped to 0x6000-0x7FFF
	EVENT_ROM_WRITE,

	EVENT_PPU_CTRL_WRITE,
	EVENT_PPU_MASK_WRITE,
	EVENT_OAM_ADDR_WRITE,
	EVENT_OAM_DATA_WRITE,
	EVENT_PPU_SCROLL_WRITE,
	EVENT_PPU_ADDR_WRITE,
	EVENT_PPU_DATA_WRITE,
	EVENT_OAM_DMA_WRITE,

	EVENT_PPU_STATUS_READ,
	EVENT_PPU_DATA_READ,
} ramEventType;

typedef struct {
	ramEventType type;
	uint16_t addr;
	uint8_t byte;
	uint8_t delay; // amount of cycles until it triggers the event
} ramEvent;

//ramEvent currentEvent; // could maybe turn this into a queue if necessary

// arbitrary size
#define EVENT_QUEUE_SIZE 4
ramEvent eventQueue[EVENT_QUEUE_SIZE];
uint8_t eventQueueIndex;
uint8_t eventQueueTop;

void pushRAMEvent(ramEvent e) {
	if(e.type == EVENT_NONE) { return; }
	++e.delay;
	eventQueue[eventQueueTop] = e;
	++eventQueueTop;
	eventQueueTop %= EVENT_QUEUE_SIZE;
}

void ramEventStep(void) {
	if(eventQueueIndex == eventQueueTop) { return; }
	ramEvent* currentEvent = &eventQueue[eventQueueIndex];
	if(currentEvent->type == EVENT_NONE || currentEvent->delay == 0) { ++eventQueueIndex; eventQueueIndex %= EVENT_QUEUE_SIZE; return; }
	--currentEvent->delay;
	if(currentEvent->delay == 0) {
		uint16_t addr = currentEvent->addr;
		uint8_t byte = currentEvent->byte;
		switch(currentEvent->type) {
			case EVENT_CPU_RAM_WRITE:
				cpuRAM[addr] = byte;
				break;
			case EVENT_PRG_RAM_WRITE:
				if(prgRAMEnabled) {
					prgRAM[addr - 0x6000] = byte;
				} else {
					romWriteByte(addr, byte);
				}
				break;
			case EVENT_ROM_WRITE:
				romWriteByte(addr, byte);
				break;
			case EVENT_PPU_CTRL_WRITE:
				if(byte & PPU_CTRL_ENABLE_VBLANK && (ppu.control & PPU_CTRL_ENABLE_VBLANK) == 0 && ppu.status & PPU_STATUS_VBLANK) {
					ppu.nmiHappened = 0;
				}

				ppu.control = byte;
				ppu.t &= ~0xC00;
				ppu.t |= (byte & 3) << 10;
				break;
			case EVENT_PPU_MASK_WRITE:
				ppu.mask = byte;
				break;
			case EVENT_OAM_ADDR_WRITE:
				ppu.oamAddr = byte;
				break;
			case EVENT_OAM_DATA_WRITE:
				ppu.oam[ppu.oamAddr] = byte;
				++ppu.oamAddr;
				break;
			case EVENT_PPU_SCROLL_WRITE:
				if(!ppu.w) {
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
			case EVENT_PPU_ADDR_WRITE:
				if(!ppu.w) {
					ppu.t &= ~0xFF00;
					ppu.t |= (byte & 0x3F) << 8;
				} else {
					ppu.t &= 0xFF00;
					ppu.t |= byte;
					ppu.vramAddr = ppu.t;
				}
				ppu.w = !ppu.w;
				break;
			case EVENT_PPU_DATA_WRITE:
				ppuRAMWrite(ppu.vramAddr, byte);
				ppu.vramAddr += (ppu.control & 0x04 ? 32 : 1);
				break;
			case EVENT_OAM_DMA_WRITE:
				for(uint16_t i = 0; i < 256; ++i) {
					// could potentially do wacky stuff if it gets into the apu/ppu register areas
					ppu.oam[i] = ramReadByte((byte << 8) + i);
				}
				break;
			case EVENT_PPU_STATUS_READ:
				ppu.status &= ~PPU_STATUS_VBLANK;
				ppu.w = 0;
				break;
			case EVENT_PPU_DATA_READ:
				ppu.readBuffer = ppuRAMRead(ppu.vramAddr);
				if(ppu.control & 0x4) {
					ppu.vramAddr += 32;
				} else {
					ppu.vramAddr += 1;
				}
				break;
			default:
				printf("unimplemented ram event %i\n", currentEvent->type);
				exit(1);
		}

		currentEvent->type = EVENT_NONE;

		++eventQueueIndex;
		eventQueueIndex %= EVENT_QUEUE_SIZE;
	}
}

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
	ramEvent e;
	e.delay = cpu.cycles + 1;
	e.addr = addr;
	e.byte = byte;
	e.type = EVENT_NONE;
	if(addr >= 0x8000) {
		e.type = EVENT_ROM_WRITE;
	} else if(addr >= 0x6000) {
		e.type = EVENT_PRG_RAM_WRITE;
	} else if(addr < 0x800) {
		cpuRAM[addr] = byte;
		//e.type = EVENT_CPU_RAM_WRITE;
	}
	//e.delay = 1;
	switch(addr) {
		case 0x2000:
			e.type = EVENT_PPU_CTRL_WRITE;
			e.delay = 1; // janky workaround to the janky workaround to fix the nmi control test
			break;
		case 0x2001:
			e.type = EVENT_PPU_MASK_WRITE;
			break;
		case 0x2003:
			e.type = EVENT_OAM_ADDR_WRITE;
			break;
		case 0x2004:
			e.type = EVENT_OAM_DATA_WRITE;
			e.addr = ppu.oamAddr;
			break;
		case 0x2006:
			e.type = EVENT_PPU_ADDR_WRITE;
			e.delay = 1; // another janky workaround to fix the read buffer
			break;
		case 0x2007:
			e.type = EVENT_PPU_DATA_WRITE;
			e.addr = ppu.vramAddr;
			e.delay = 1;
			break;
		case 0x4014:
			e.type = EVENT_OAM_DMA_WRITE;
			break;
		case 0x4016:
			controllerLatch = byte & 0x01;
			if(controllerLatch) {
				controllers[0].shiftRegister = controllers[0].buttons;
				controllers[1].shiftRegister = controllers[1].buttons;
			}
			break;
		case 0x2005:
			e.type = EVENT_PPU_SCROLL_WRITE;
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
			dmcSetEnableFlag(byte & 0x10);
			break;
		default:
			// open bus
			break;
	}
	pushRAMEvent(e);
}

uint8_t ramReadByte(uint16_t addr) {
	addr = addrMap(addr);
	if(prgRAMEnabled && addr >= 0x6000 && addr < 0x8000) {
		return prgRAM[addr - 0x6000];
	} else if(addr >= 0x6000) {
		return romReadByte(addr);
	} else if(addr < 0x800) {
		return cpuRAM[addr];
	}
	switch(addr) {
		case 0x2002: {
			ramEvent e;
			e.type = EVENT_PPU_STATUS_READ;
			//e.delay = cpu.cycles + 1;
			e.delay = 1;
			e.addr = addr;
			e.byte = 0;
			pushRAMEvent(e);
			return ppu.status;
		}
		case 0x2007: {
			// https://www.nesdev.org/wiki/PPU_registers#The_PPUDATA_read_buffer
			ramEvent e;
			e.type = EVENT_PPU_DATA_READ;
			//e.delay = cpu.cycles + 1;
			e.delay = 1;
			e.addr = addr;
			e.byte = 0;
			pushRAMEvent(e);
			return ppu.readBuffer;
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
			// open bus
			return 0;
	}
	return 0;
}
