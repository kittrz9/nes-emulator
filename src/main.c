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

		uint64_t lastCycles = cpu.cycles;
		cpuStep();
		for(uint8_t i = 0; i < cpu.cycles - lastCycles; ++i) {
			apuStep();
			for(uint8_t j = 0; j < 3; ++j) {
				// this fixes scrolling in mmc3 stuff but breaks almost everything else lmao
				/*if((ppu.currentPixel % 256) % 8 == 0 && ppu.currentPixel != 0) {
					ppu.w = 0;
				}*/
				if(ppu.currentPixel % 340 < 256 && ppu.currentPixel / 340 < 240) {
					drawPixel(ppu.currentPixel % 340, ppu.currentPixel / 340);
				} else {
					// hblank
					// janky way to get the mmc3 stuff to trigger
					chrReadByte(0);
				}
				// scanline specific stuff should probably be moved to the ppu handling stuff
				if(ppu.currentPixel == 340 * 240) {
					if(handleInput() != 0) { running = 0; }
					ppu.status |= PPU_STATUS_VBLANK;
					if(ppu.control & PPU_CTRL_ENABLE_VBLANK) {
						push((cpu.pc & 0xFF00) >> 8);
						push(cpu.pc & 0xFF);
						push((cpu.p & ~(B_FLAG)) | 0x20);
						cpu.p |= I_FLAG;
						cpu.pc = ADDR16(NMI_VECTOR);
					}
					apuPrintDebug();
					render();
				}
				if(ppu.currentPixel == 340 * 261) {
					ppu.status &= ~PPU_STATUS_SPRITE_0;
					ppu.currentPixel = 0;
					ppu.status &= ~(PPU_STATUS_VBLANK);
					continue;
				}

				// sprite 0 jank since smb1 still breaks
				// it's probably something wrong with how scrolling is handled since it's happening once the x scroll part of the control register gets set
				// I'm not sure what exactly is wrong though, it should be setting it to 0 before rendering starts
				if((ppu.status & PPU_STATUS_SPRITE_0) == 0 && ppu.currentPixel == (ppu.oam[0]+8)*340 + ppu.oam[3]) {
					ppu.status |= PPU_STATUS_SPRITE_0;
				}

				++ppu.currentPixel;
			}
		}
	};

	free(prgROM);
	free(chrROM);

	uninitRenderer();

	return 0;
}
