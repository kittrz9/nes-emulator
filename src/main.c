#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "files.h"
#include "ram.h"
#include "rom.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "input.h"

extern struct {
	struct {
		uint8_t volume;
		uint16_t timer;
		uint8_t counter;
		uint8_t loop;
		uint8_t duty;
	} pulse[2];
	uint8_t frameCounter;
	uint8_t mode;
	uint8_t irqInhibit;
} apu;

int main(int argc, char** argv) {
	if(argc < 2) {
		printf("usage: %s romPath\n", argv[0]);
		return 1;
	}

	if(loadROM(argv[1]) != 0) {
		return 1;
	}

	if(initRenderer() != 0) {
		return 1;
	}
	initInput();

	initAPU();

	cpuInit();

	while(1) {
		if(handleInput() != 0) { break; }

		ppu.status &= ~PPU_STATUS_SPRITE_0;
		uint16_t ppuPixel = 0;
		uint32_t lastCycles;
		while(cpu.cycles <= CYCLES_PER_FRAME - CYCLES_PER_VBLANK) {
			lastCycles = cpu.cycles;
			cpuStep();
			for(uint8_t i = 0; ppuPixel < 256*240 && i < cpu.cycles - lastCycles; ++i) {
				for(uint8_t j = 0; j < 3; ++j) {
					drawPixel(ppuPixel % 256, ppuPixel / 256);
					++ppuPixel;
				}
			}
			if(cpu.cycles >= CYCLES_PER_SCANLINE * ppu.oam[0]) {
				ppu.status |= PPU_STATUS_SPRITE_0;
			}
		}
		cpu.cycles = 0;
		ppu.status |= PPU_STATUS_VBLANK;
		ppu.w = 0;
		if((ppu.control & PPU_CTRL_ENABLE_VBLANK) != 0) {
			push((cpu.pc & 0xFF00) >> 8);
			push(cpu.pc & 0xFF);
			push((cpu.p & ~(B_FLAG)) | 0x20);
			cpu.p |= I_FLAG;
			cpu.pc = ADDR16(NMI_VECTOR);
		}

		while(cpu.cycles <= CYCLES_PER_VBLANK) {
			cpuStep();
		}
		ppu.status &= ~(PPU_STATUS_VBLANK);
		cpu.cycles = 0;

		apuPrintDebug();

		render();
	};

	free(prgROM);
	free(chrROM);

	uninitRenderer();

	return 0;
}
