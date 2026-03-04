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
#include "nsf.h"

int nesMain(void) {
	while(1) {
		cpuStep();
		for(uint8_t i = 0; i < cpu.cycles; ++i) {
			cycleCounter();
			apuStep();
			for(uint8_t j = 0; j < 3; ++j) {
				if(ppu.currentPixel == 0) {
					if(handleInput() != 0) { return 1; }
				}
				ppuStep();
			}
		}
		cpu.cycles = 0;
	};
	return 0;
}


int main(int argc, char** argv) {
	if(argc < 2) {
		printf("usage: %s romPath\n", argv[0]);
		return 1;
	}

	if(loadROM(argv[1]) != 0) {
		return 1;
	}

	initInput();

	initAPU();

	if(initRenderer() != 0) {
		return 1;
	}

	if(rom.isNSF) {
		nsfInit(0);
		nsfMain();
	} else {
		cpuInit();
		nesMain();
	}

	free(rom.prgROM);
	free(rom.chrROM);

	uninitRenderer();

	return 0;
}
