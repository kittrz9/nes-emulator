#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "files.h"
#include "ram.h"
#include "rom.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "input.h"
#include "nsf.h"
#include "dma.h"

int nesMain(void) {
	while(1) {
		if(!dmaActive) {
			cpuStep();
		} else {
			dmaStep();
		}
		for(uint8_t i = 0; i < cpu.cycles; ++i) {
			dmaCycle = !dmaCycle;
			cycleCounter();
			apuStep();
			for(uint8_t j = 0; j < 3; ++j) {
				if(ppu.currentPixel == 340*241) { // fm2 tasses expect input on vblank instead of frame start I think
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

	char* romPath = NULL;
	char* tasPath = NULL;

	for(uint32_t i = 1; i < argc; ++i) {
		if(strcmp(argv[i], "-t") == 0) {
			++i;
			if(i >= argc) { break; }
			tasPath = argv[i];
		} else {
			romPath = argv[i];
		}
	}

	if(romPath == NULL) {
		return 1;
	}

	if(tasPath != NULL) {
		printf("%s\n", tasPath);
		loadTas(tasPath);
	}

	if(loadROM(romPath) != 0) {
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
