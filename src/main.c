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

	uint8_t running = 1;

	while(running) {
		//uint64_t lastCycles = cpu.cycles;
		cpuStep();
		//printf("%i %i\n", cpu.cycles, lastCycles);
		for(uint8_t i = 0; i < cpu.cycles; ++i) {
			apuStep();
			for(uint8_t j = 0; j < 3; ++j) {
				if(ppu.currentPixel == 0) {
					if(handleInput() != 0) { running = 0; }
				}
				ppuStep();
			}
		}
		cpu.cycles = 0;
	};

	free(prgROM);
	free(chrROM);

	uninitRenderer();

	return 0;
}
