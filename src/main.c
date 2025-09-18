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

	while(1) {
		if(handleInput() != 0) { break; }

		ppu.status &= ~PPU_STATUS_SPRITE_0;
		uint32_t lastCycles;
		ppu.currentPixel = 0;
		while(cpu.cycles <= CYCLES_PER_FRAME - CYCLES_PER_VBLANK) {
			lastCycles = cpu.cycles;
			cpuStep();
			for(uint8_t i = 0; ppu.currentPixel < 340*240 && i < cpu.cycles - lastCycles; ++i) {
				for(uint8_t j = 0; j < 3; ++j) {
					// this fixes scrolling in mmc3 stuff but breaks almost everything else lmao
					/*if((ppu.currentPixel % 256) % 8 == 0 && ppu.currentPixel != 0) {
						ppu.w = 0;
					}*/
					if(ppu.currentPixel % 340 < 256) {
						drawPixel(ppu.currentPixel % 340, ppu.currentPixel / 340);
					} else {
						// hblank
						// janky way to get the mmc3 stuff to trigger
						chrReadByte(0);
					}
					++ppu.currentPixel;
				}
			}
			/*if(cpu.cycles >= CYCLES_PER_SCANLINE * ppu.oam[0]) {
				ppu.status |= PPU_STATUS_SPRITE_0;
			}*/
			// still very innacurate and probably will break anything that uses the sprite zero hit besides smb1
			// I don't have any other roms that use the sprite zero hit to test it with
			if(ppu.currentPixel / 340 >= ppu.oam[0] + 8) {
				ppu.status |= PPU_STATUS_SPRITE_0;
			}
		}
		cpu.cycles = 0;
		ppu.status |= PPU_STATUS_VBLANK;
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
