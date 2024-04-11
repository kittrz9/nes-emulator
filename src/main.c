#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "rom.h"
#include "ram.h"
#include "cart.h"
#include "cpu.h"
#include "ppu.h"
#include "input.h"

// https://www.nesdev.org/wiki/Cycle_reference_chart
#define CYCLES_PER_FRAME 29780
#define CYCLES_PER_VBLANK 2273

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
		//controllers[0].buttons |= 1;

		while(cpu.cycles <= CYCLES_PER_FRAME - CYCLES_PER_VBLANK) {
			cpuStep();
		}
		printf("funny vblank\n");
		cpu.cycles = 0;
		ppu.status |= 0x80;
		if(ppu.control & 0x80) {
			push(cpu.pc & 0xFF);
			push((cpu.pc & 0xFF00) >> 8);
			push(cpu.p);
			cpu.pc = ADDR16(NMI_VECTOR);
		}

		while(cpu.cycles <= CYCLES_PER_VBLANK) {
			cpuStep();
		}
		//printf("vblank over\n");
		ppu.status &= ~(0x80);
		cpu.cycles = 0;

		render();
	};

	free(prgROM);
	free(chrROM);

	uninitRenderer();

	return 0;
}
