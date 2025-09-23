#include "ppu.h"

#include "SDL3/SDL.h"

#include <stdio.h>

#include "rom.h"

#include "debug.h"

ppu_t ppu;

uint8_t fpsUncap = 0;

uint8_t ppuRAM[0x4000];

uint8_t nametableBank1[0x400];
uint8_t nametableBank2[0x400];

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
	uint8_t* palette = &ppuRAM[0x3F00];
	if(combined == 0) {
		return paletteColors[ppuRAM[0x3F00]];
	} else {
		return paletteColors[palette[newIndex]] & (combined == 0 ? paletteColors[ppuRAM[0x3F00]]: 0xFFFFFFFF);
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
			uint8_t spriteX = ppu.oam[i*4 + 3];
			if(y >= spriteY && y < spriteY + ySize) {
				// not accurately evaluating the sprite overflow stuff
				if(secondaryOAMIndex == 8) {
					ppu.status |= PPU_STATUS_SPRITE_OVERFLOW;
					break;
				} else {
					if(i == 0) { spriteZeroIndex = secondaryOAMIndex; }
					memcpy(&secondaryOAM[secondaryOAMIndex*4], &ppu.oam[i*4], 4);
					++secondaryOAMIndex;
				}
			}
		}
	}
	uint32_t* target = (uint32_t*)(((uint8_t*)frameBuffer->pixels + x*sizeof(uint32_t)) + y*frameBuffer->pitch);
	uint8_t backgroundPixel = 0;
	// incredibly messy code
	if(ppu.mask & PPU_MASK_ENABLE_BACKGROUND && !((ppu.mask & PPU_MASK_LEFT_BACKGROUND) == 0 && x < 8)) {
		uint16_t bank = (ppu.control & PPU_CTRL_BACKGROUND_TABLE ? 0x1000 : 0x0000);
		uint16_t scrollX = (ppu.scrollX + (ppu.control & 0x01 ? 256 : 0));
		uint16_t scrollY = (ppu.scrollY + (ppu.control & 0x02 ? 240 : 0));
		uint16_t nametableX = (x + scrollX) % 512;
		uint16_t nametableY = (y + scrollY) % 480;
		uint8_t* table;
		if(ppu.mirror & MIRROR_HORIZONTAL) {
			if(nametableX >= 256) {
				table = &ppuRAM[0x2400];
			} else {
				table = &ppuRAM[0x2000];
			}
		} else {
			if(nametableY >= 240) {
				table = &ppuRAM[0x2800];
			} else {
				table = &ppuRAM[0x2000];
			}
		}
		uint8_t* attribTable = table + 0x3C0;
		uint8_t tileID = table[((nametableX%256)/8) + ((nametableY%240)/8)*32];
		uint8_t tileX = (nametableX/8)%32;
		uint8_t tileY = (nametableY/8)%30;
		uint8_t shift = (tileX/2) % 2;
		if((tileY/2)% 2 == 1) {
			shift += 2;
		}
		shift *= 2;
		uint8_t attribX = tileX/4;
		uint8_t attribY = tileY/4;
		uint8_t attribIndex = ((attribY * 8) + attribX)%64;
		uint8_t paletteIndex = ((attribTable[attribIndex] >> shift) & 0x3) << 2;
		/*uint8_t* bitplane1 = bank + tileID*8*2 + (nametableY)%8;
		uint8_t* bitplane2 = bitplane1 + 8;*/
		/*uint8_t bitplane1 = chrReadByte(bank + tileID*8*2 + nametableY%8);
		uint8_t bitplane2 = chrReadByte(bank + tileID*8*2 + nametableY%8 + 8);
		uint8_t combined = ((bitplane1 >> (7-(nametableX%8))) & 1) | (((bitplane2 >> (7-(nametableX%8))) & 1) << 1);
		uint8_t newIndex = paletteIndex | combined;
		uint8_t* palette = &ppuRAM[0x3F00];
		if(combined == 0) {
			*target = paletteColors[ppuRAM[0x3F00]];
		} else {
			*target = paletteColors[palette[newIndex]] & (combined == 0 ? paletteColors[ppuRAM[0x3F00]]: 0xFFFFFFFF);
		}*/
		backgroundPixel = bitplaneGetPixel(bank + tileID*8*2, nametableX%8, nametableY%8);
		*target = bitplaneGetColor(backgroundPixel, paletteIndex);
	} else {
		*target = paletteColors[ppuRAM[0x3f00]];
	}
	if(ppu.mask & PPU_MASK_ENABLE_SPRITES && !((ppu.mask & PPU_MASK_LEFT_SPRITES) == 0 && x < 8)) {
		for(uint8_t i = 0; i < 8; ++i) {
			uint8_t spriteX = secondaryOAM[i*4 + 3];
			uint8_t spriteY = secondaryOAM[i*4 + 0] + 1;
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
			if(spriteAttribs & PPU_OAM_PRIORITY && backgroundPixel != 0) {
				continue;
			}
			if(spritePixel != 0) {
				*target = bitplaneGetColor(spritePixel, paletteIndex);
				break;
			}
		}
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
