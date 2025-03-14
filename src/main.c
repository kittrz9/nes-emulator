#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "files.h"
#include "ram.h"
#include "rom.h"
#include "cpu.h"
#include "ppu.h"
#include "input.h"

// https://www.nesdev.org/wiki/Cycle_reference_chart
#define CYCLES_PER_FRAME 29780
#define CYCLES_PER_VBLANK 2273
#define CYCLES_PER_SCANLINE 114

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

	cpuInit();

	while(1) {
		if(handleInput() != 0) { break; }

		ppu.status &= ~(0x40);
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
				ppu.status |= 0x40;
			}
		}
		#ifdef DEBUG
			printf("funny vblank\n");
		#endif
		cpu.cycles = 0;
		ppu.status |= 0x80;
		ppu.w = 0;
		if((ppu.control & 0x80) != 0) {
			push((cpu.pc & 0xFF00) >> 8);
			push(cpu.pc & 0xFF);
			push(cpu.p & ~(B_FLAG));
			cpu.p |= I_FLAG;
			cpu.pc = ADDR16(NMI_VECTOR);
		}

		while(cpu.cycles <= CYCLES_PER_VBLANK) {
			cpuStep();
		}
		ppu.status &= ~(0x80);
		cpu.cycles = 0;

		render();
	};

	free(prgROM);
	free(chrROM);

	uninitRenderer();

	return 0;
}
