#include "ppu.h"

#include "SDL3/SDL.h"

#include <stdio.h>
#include <stdlib.h>

#include "rom.h"
#include "ram.h"
#include "cpu.h"
#include "input.h"
#include "apu.h"

#include "debug.h"

ppu_t ppu;

uint8_t fpsUncap = 0;

SDL_Window* w;
SDL_Surface* windowSurface;
SDL_Surface* frameBuffer;

// generated with this: https://github.com/Gumball2415/palgen-persune
// palgen_persune.py -o test -f ".txt HTML hex"
// modified to be RGBA uint32_t
uint32_t paletteColors[] = {
	0x626262FF,
	0x001FB2FF,
	0x2404C8FF,
	0x5200B2FF,
	0x730076FF,
	0x800024FF,
	0x730B00FF,
	0x522800FF,
	0x244400FF,
	0x005700FF,
	0x005C00FF,
	0x005324FF,
	0x003C76FF,
	0x000000FF,
	0x000000FF,
	0x000000FF,
	0xABABABFF,
	0x1750FFFF,
	0x552AFFFF,
	0x9410FFFF,
	0xC208C5FF,
	0xD31655FF,
	0xC23400FF,
	0x945B00FF,
	0x558100FF,
	0x179B00FF,
	0x00A200FF,
	0x009555FF,
	0x0077C5FF,
	0x000000FF,
	0x000000FF,
	0x000000FF,
	0xFFFFFFFF,
	0x65A0FFFF,
	0xA679FFFF,
	0xE75EFFFF,
	0xFF56FFFF,
	0xFF64A6FF,
	0xFF8332FF,
	0xE7AC00FF,
	0xA6D300FF,
	0x65EF00FF,
	0x36F632FF,
	0x24E9A6FF,
	0x36C9FFFF,
	0x4E4E4EFF,
	0x000000FF,
	0x000000FF,
	0xFFFFFFFF,
	0xC1D9FFFF,
	0xDBC9FFFF,
	0xF6BEFFFF,
	0xFFBBFFFF,
	0xFFC1DBFF,
	0xFFCDADFF,
	0xF6DE8BFF,
	0xDBED7EFF,
	0xC1F88BFF,
	0xAEFCADFF,
	0xA7F6DBFF,
	0xAEE9FFFF,
	0xB8B8B8FF,
	0x000000FF,
	0x000000FF,
};

uint8_t nametables[2][0x400];
uint8_t paletteRAM[0x20];

uint8_t ppuRAMRead(uint16_t addr) {
	if(addr < 0x2000) {
		chrReadByte(ppu.vramAddr);
	} else if(addr >= 0x2000 && addr <= 0x2FFF) {
		uint8_t tableIndex;
		switch(ppu.mirror) {
			case MIRROR_VERTICAL:
				tableIndex = (addr >> 11) & 1;
				break;
			case MIRROR_HORIZONTAL:
				tableIndex = (addr >> 10) & 1;
				break;
			case MIRROR_SINGLE_SCREEN1:
				tableIndex = 0;
				break;
			case MIRROR_SINGLE_SCREEN2:
				tableIndex = 1;
				break;
			default:
				printf("unimplemented mirroring mode %i\n", ppu.mirror);
				exit(1);
				break;
		}
		return nametables[tableIndex][addr & 0x3FF];
	} else if(addr >= 0x3F00) {
		return paletteRAM[addr & 0x1F] & 0x3F;
	}
}

void ppuRAMWrite(uint16_t addr, uint8_t byte) {
	if(addr < 0x2000) {
		chrWriteByte(ppu.vramAddr, byte);
	} else if(addr >= 0x2000 && addr <= 0x2FFF) {
		uint8_t tableIndex;
		switch(ppu.mirror) {
			case MIRROR_VERTICAL:
				tableIndex = (addr >> 11) & 1;
				break;
			case MIRROR_HORIZONTAL:
				tableIndex = (addr >> 10) & 1;
				break;
			case MIRROR_SINGLE_SCREEN1:
				tableIndex = 0;
				break;
			case MIRROR_SINGLE_SCREEN2:
				tableIndex = 1;
				break;
			default:
				printf("unimplemented mirroring mode %i\n", ppu.mirror);
				exit(1);
				break;
		}
		nametables[tableIndex][addr & 0x3FF] = byte;
	} else if(addr >= 0x3F00) {
		byte &= 0x3F;
		if(addr % 4 == 0) { paletteRAM[(addr & 0x1F)^0x10] = byte; }
		paletteRAM[addr & 0x1F] = byte;
	}
}

uint8_t initRenderer(void) {
	if(SDL_Init(SDL_INIT_VIDEO) == 0) {
		printf("could not init SDL\n");
		return 1;
	}

	w = SDL_CreateWindow("nesEmu", SCREEN_WIDTH, SCREEN_HEIGHT, 0);
	windowSurface = SDL_GetWindowSurface(w);
	frameBuffer = SDL_CreateSurface(FB_WIDTH, FB_HEIGHT,SDL_PIXELFORMAT_RGBA8888);

	initDebugRenderer();

	return 0;
}

void uninitRenderer(void) {
	SDL_DestroySurface(frameBuffer);
	SDL_DestroyWindowSurface(w);
	SDL_DestroyWindow(w);

	SDL_Quit();
}

void debugScreenshot(void) {
	SDL_SaveBMP(frameBuffer, "framebuffer.bmp");
}

uint8_t bitplaneGetPixel(uint16_t bitplaneStart, uint16_t x, uint16_t y) {
	uint8_t bitplane1 = chrReadByte(bitplaneStart + y);
	uint8_t bitplane2 = chrReadByte(bitplaneStart + 8 + y);
	return ((bitplane1 >> (7-x)) & 1) | (((bitplane2 >> (7-x)) & 1) << 1);
}

uint32_t bitplaneGetColor(uint8_t combined, uint8_t paletteIndex) {
	uint8_t newIndex = paletteIndex | combined;
	if(combined == 0) {
		return paletteColors[ppuRAMRead(0x3F00)];
	} else {
		return paletteColors[ppuRAMRead(0x3F00 + newIndex)] & (combined == 0 ? paletteColors[ppuRAMRead(0x3F00)]: 0xFFFFFFFF);
	}
}

uint8_t secondaryOAM[4*8];
uint8_t secondaryOAMIndex;
uint8_t spriteZeroIndex;

void drawPixel(uint16_t x, uint16_t y) {
	uint8_t ySize = 8;
	if(ppu.control & PPU_CTRL_SPRITE_SIZE) {
		ySize = 16;
	}
	if(x == 0) {
		spriteZeroIndex = 9;
		// https://www.nesdev.org/wiki/PPU_sprite_evaluation
		memset(secondaryOAM, 0xFF, sizeof(secondaryOAM));
		secondaryOAMIndex = 0;
		for(uint8_t i = 0; i < 64; ++i) {
			uint8_t spriteY = ppu.oam[i*4 + 0] + 1;
			if(y >= spriteY && y < spriteY + ySize) {
				// not accurately evaluating the sprite overflow stuff
				if(secondaryOAMIndex == 8) {
					ppu.status |= PPU_STATUS_SPRITE_OVERFLOW;
					break;
				} else {
					if(i == 0) { spriteZeroIndex = secondaryOAMIndex; }
					memcpy(&secondaryOAM[secondaryOAMIndex*4], &ppu.oam[i*4], 4);
					++secondaryOAMIndex;
					drawDebugText(ppu.oam[i*4 + 3] * 2, spriteY * 2, "%i", i);
				}
			}
		}
	}
	uint32_t* target = (uint32_t*)(((uint8_t*)frameBuffer->pixels + x*sizeof(uint32_t)) + y*frameBuffer->pitch);
	// mmc2 requires an extra tile to be read at the end of the scanline
	// this is to avoid needing to change the rest of the code to have ifs in them
	// should probably move chr rom reading stuff into their own functions and call them in ppuStep instead
	uint32_t asdf;
	if(x > 256 || y > 240) {
		target = &asdf;
	}

	uint8_t backgroundPixel = 0;
	// incredibly messy code
	if(ppu.mask & PPU_MASK_ENABLE_BACKGROUND && !((ppu.mask & PPU_MASK_LEFT_BACKGROUND) == 0 && x < 8)) {
		// https://www.nesdev.org/wiki/PPU_scrolling#Tile_and_attribute_fetching
		uint16_t tileAddr = 0x2000 | (ppu.vramAddr & 0xFFF);
		uint16_t attribAddr = 0x23C0 | (ppu.vramAddr & 0xC00) | ((ppu.vramAddr >> 4) & 0x38) | ((ppu.vramAddr >> 2) & 0x7);
		uint8_t coarseX = ppu.vramAddr & 0x1F;
		uint8_t coarseY = (ppu.vramAddr >> 5) & 0x1F;
		uint8_t fineX = ppu.x + (x % 8);
		uint8_t fineY = (ppu.vramAddr >> 12);
		if(fineX > 7) {
			if(coarseX % 4 == 3) {
				attribAddr += 1;
			}
			if(coarseX < 0x1F) {
				++coarseX;
				++tileAddr;
			} else {
				coarseX = 0;
				attribAddr += 0x400 - 0x1F/4 - 1;
				tileAddr += 0x400 - 0x1F; // move into the next nametable
				if(tileAddr >= 0x3000) {
					tileAddr -= 0x1000;
					attribAddr -= 0x1000;
				}
			}
		}
		fineX %= 8;
		if(fineY > 7) {
			tileAddr += 256/8;
			++coarseY;
		}
		fineY %= 8;

		uint8_t shift = (coarseX/2) % 2;
		if((coarseY/2)% 2 == 1) {
			shift += 2;
		}
		shift *= 2;

		uint16_t bank = (ppu.control & PPU_CTRL_BACKGROUND_TABLE ? 0x1000 : 0x0000);
		uint8_t tileID = ppuRAMRead(tileAddr);
		uint8_t attrib = ppuRAMRead(attribAddr);
		uint8_t paletteIndex = ((attrib >> shift) & 0x3) << 2;

		backgroundPixel = bitplaneGetPixel(bank + tileID*8*2, fineX % 8, fineY % 8);
		*target = bitplaneGetColor(backgroundPixel, paletteIndex);
	} else {
		*target = paletteColors[ppuRAMRead(0x3f00)];
	}
	if(ppu.mask & PPU_MASK_ENABLE_SPRITES && !((ppu.mask & PPU_MASK_LEFT_SPRITES) == 0 && x < 8)) {
		for(uint8_t i = 0; i < 8; ++i) {
			uint8_t spriteX = secondaryOAM[i*4 + 3];
			uint16_t spriteY = secondaryOAM[i*4 + 0] + 1;
			uint8_t spriteAttribs = secondaryOAM[i*4 + 2];
			if(x < spriteX || x > spriteX + 7 || y < spriteY || y > spriteY + ySize - 1) {
				   continue;
			}
			uint16_t bank;
			uint8_t tileID = secondaryOAM[i*4 + 1];
			uint8_t paletteIndex = 0x10 | ((secondaryOAM[i*4 + 2]&0x3) << 2);
			if(ySize > 8) {
				bank = (tileID & 1 ? 0x1000 : 0x0000);
				tileID &= ~1;
			} else {
				bank = (ppu.control & PPU_CTRL_SPRITE_TABLE ? 0x1000 : 0x0000);
			}
			uint16_t bitplane = bank + tileID*8*2;
			uint8_t xOffset = x - spriteX;
			uint8_t yOffset = y - spriteY;
			if(spriteAttribs & PPU_OAM_FLIP_HORIZONTAL) {
				xOffset = 7-xOffset;
			}
			if(spriteAttribs & PPU_OAM_FLIP_VERTICAL) {
				yOffset = ySize-1-yOffset;
			}
			if(yOffset > 7) {
				bitplane += 16;
			}
			uint8_t spritePixel = bitplaneGetPixel(bitplane, xOffset%8, yOffset%8);
			if(x < 255 && (ppu.status & PPU_STATUS_SPRITE_0) == 0 && i == spriteZeroIndex && spritePixel != 0 && backgroundPixel != 0) {
				//printf("sprite 0\n");
				ppu.status |= PPU_STATUS_SPRITE_0;
			}
			if(spritePixel != 0) {
				if((spriteAttribs & PPU_OAM_PRIORITY) == 0 || backgroundPixel == 0) {
					*target = bitplaneGetColor(spritePixel, paletteIndex);
				}
				break;
			}
		}
	}
}

void ppuStep(void) {
	uint16_t x = ppu.currentPixel % 341;
	uint16_t y = ppu.currentPixel / 341;
	if(y == 261 && x == 0) {
		ppu.status &= ~PPU_STATUS_SPRITE_0;
		ppu.status &= ~PPU_STATUS_VBLANK;
	}
	if(y == 241 && x == 1) {
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
	// https://www.nesdev.org/wiki/PPU_scrolling#Wrapping_around
	if(ppu.mask & (PPU_MASK_ENABLE_BACKGROUND | PPU_MASK_ENABLE_SPRITES)) {
		if(y < 240 || y == 261) {
			if(x == 256) {
				// increment ppu.vramAddr vertically
				if((ppu.vramAddr & 0x7000) != 0x7000) {
					ppu.vramAddr += 0x1000;
				} else {
					ppu.vramAddr &= ~0x7000;
					uint8_t coarseY = (ppu.vramAddr & 0x3E0) >> 5;
					if(coarseY == 29) {
						coarseY = 0;
						ppu.vramAddr ^= 0x800;
					} else if(coarseY == 31) {
						coarseY = 0;
					} else {
						++coarseY;
					}
					ppu.vramAddr = (ppu.vramAddr & ~0x3E0) | (coarseY << 5);
				}
			}
			if(x == 257) {
				// copy horizontal bits from ppu.t to ppu.vramAddr
				ppu.vramAddr &= ~0x41F;
				ppu.vramAddr |= ppu.t & 0x41F;
			}
			if(x != 0 && x % 8 == 0 && x <= 256) {
				// increment ppu.vramAddr horizontally
				if((ppu.vramAddr & 0x1F) == 31) {
					ppu.vramAddr &= ~0x1F;
					ppu.vramAddr ^= 0x400;
				} else {
					++ppu.vramAddr;
				}
			}
		}
		if(y == 261 && x >= 280 && x <= 304) {
			// copy vertical bits from ppu.t to ppu.vramAddr
			ppu.vramAddr &= ~0x7BE0;
			ppu.vramAddr |= ppu.t & 0x7BE0;
		}
	}
	if(x == 260 && (y < 240 || y == 261)) {
		scanlineCounter();
	}
	if(x < 321 && y < 240) {
		drawPixel(x, y);
	}

	if(ppu.currentPixel < 341*261 + 340) {
		++ppu.currentPixel;
	} else {
		ppu.currentPixel = 0;
	}
}

void render(void) {
	SDL_BlitSurfaceScaled(frameBuffer, &(SDL_Rect){0,0,FB_WIDTH,FB_HEIGHT}, windowSurface, &(SDL_Rect){0,0,SCREEN_WIDTH,SCREEN_HEIGHT}, SDL_SCALEMODE_NEAREST);

	renderDebugInfo(windowSurface);
	SDL_UpdateWindowSurface(w);

	if(!fpsUncap) {
		static uint64_t lastTicks = 0;
		if(lastTicks == 0) {
			lastTicks = SDL_GetTicksNS();
		}
		uint64_t currentTicks = SDL_GetTicksNS();
		if(currentTicks - lastTicks < 1000000000/60) {
			SDL_DelayNS(1000000000/60 - (currentTicks - lastTicks));
		}
		lastTicks = SDL_GetTicksNS();
	}
}

void toggleFPSCap(void) {
	fpsUncap = !fpsUncap;
}
