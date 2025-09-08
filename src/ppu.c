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
SDL_Surface* tile;
SDL_Surface* nameTable;
SDL_Surface* frameBuffer;

#define NAMETABLE_WIDTH 32*8*2
#define NAMETABLE_HEIGHT 32*8*2

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

	//tile = SDL_CreateRGBSurface(0, 8, 16, 32, 0xFF000000, 0xFF0000, 0xFF00, 0xFF);
	tile = SDL_CreateSurface(8, 16, SDL_PIXELFORMAT_RGBA8888);
	// dealing with any pitch that isn't like uint32_t aligned would be a pain
	// since I can't just add a single byte to a uint32_t* without breaking strict aliasing
	// (or at least I don't think I can with the methods I know in a way that isn't dumb or rewrites all of drawTile)
	// so I'm just not dealing with pitch at all
	if(tile->pitch != sizeof(uint32_t)*8) {
		printf("SDL surface pitch is not just 32 bytes, not gonna deal with that for now\n");
		SDL_DestroySurface(tile);
		SDL_Quit();
		return 1;
	}
	w = SDL_CreateWindow("nesEmu", SCREEN_WIDTH, SCREEN_HEIGHT, 0);
	windowSurface = SDL_GetWindowSurface(w);
	frameBuffer = SDL_CreateSurface(FB_WIDTH, FB_HEIGHT,SDL_PIXELFORMAT_RGBA8888);
	nameTable = SDL_CreateSurface(NAMETABLE_WIDTH, NAMETABLE_HEIGHT, SDL_PIXELFORMAT_RGBA8888);

		initDebugRenderer();

	return 0;
}

void uninitRenderer(void) {
	SDL_DestroySurface(tile);
	SDL_DestroySurface(frameBuffer);
	SDL_DestroySurface(nameTable);
	SDL_DestroyWindowSurface(w);
	SDL_DestroyWindow(w);

	SDL_Quit();
}

void debugScreenshot(void) {
	SDL_SaveBMP(nameTable, "nametable.bmp");
	SDL_SaveBMP(frameBuffer, "framebuffer.bmp");
}

#define FLIP_HORIZONTAL 0x40
#define FLIP_VERTICAL 0x80

void drawTile(SDL_Surface* dst, uint16_t bitplaneStart, uint16_t x, uint16_t y, uint8_t attribs, uint8_t paletteIndex) {
	SDL_Rect targetRect = {
		.x = x,
		.y = y,
		.w = 8,
		.h = 8,
	};
	SDL_Rect srcRect = {
		.x = 0,
		.y = 0,
		.w = 8,
		.h = 8,
	};
	uint32_t* target = (uint32_t*)tile->pixels + (attribs & FLIP_VERTICAL ? 56 : 0);
	for(uint8_t j = 0; j < 8; ++j) {
		uint8_t bitplane1 = chrReadByte(bitplaneStart + j);
		uint8_t bitplane2 = chrReadByte(bitplaneStart + 8 + j);
		uint8_t k = (attribs & FLIP_HORIZONTAL ? 0 : 7);
		while(k < 8) {
			uint8_t combined = ((bitplane1 >> k) & 1) | (((bitplane2 >> k) & 1) << 1);
			uint8_t newIndex = paletteIndex | combined;
			uint8_t* palette = &ppuRAM[0x3F00];
			*target = paletteColors[palette[newIndex]] & (combined == 0 ? 0xFFFFFF00 : 0xFFFFFFFF);
			++target;
			k += (attribs & FLIP_HORIZONTAL ? 1 : -1);
		}
		if(attribs & FLIP_VERTICAL) {
			target -= 16;
		}
	}
	SDL_BlitSurface(tile, &srcRect, dst, &targetRect);
}

/*void drawNametable(uint8_t* chrBank, uint8_t* table, uint16_t x, uint16_t y) {
	uint8_t* attribTable = table + 0x3C0;
	for(uint16_t i = 0; i < 960; ++i) {
		uint8_t tileID = *(table+i);
		uint8_t tileX = i%32;
		uint8_t tileY = i/32;
		uint8_t attribX = tileX/4;
		uint8_t attribY = tileY/4;
		uint8_t attribIndex = (attribY * 8) + attribX;

		uint8_t* bitplaneStart = chrBank + tileID*8*2;
		uint16_t xPos = ((i%32)*8 + x) % 512;
		uint16_t yPos = ((i/32)*8 + y) % 480;
		uint8_t shift = (tileX/2) % 2;
		if((tileY/2)% 2 == 1) {
			shift += 2;
		}
		shift *= 2;
		uint8_t paletteIndex = ((attribTable[attribIndex] >> shift) & 0x3) << 2;
		drawTile(nameTable, bitplaneStart, xPos, yPos, 0, paletteIndex);
	}
}*/

void drawPixel(uint16_t x, uint16_t y) {
	// incredibly messy code
	// should eventually also handle drawing sprites so it can set the sprite zero hit flag accurately
	if(ppu.mask & 0x08) {
		uint16_t bank = (ppu.control & 0x10 ? 0x1000 : 0x0000);
		uint16_t scrollX = (ppu.scrollX + (ppu.control & 0x01 ? 256 : 0));
		uint16_t scrollY = (ppu.scrollY + (ppu.control & 0x02 ? 240 : 0));
		uint16_t nametableX = (x + scrollX) % 512;
		uint16_t nametableY = (y + scrollY) % 480;
		uint8_t* table;
		if(ppu.mirror & MIRROR_HORIZONTAL) {
			if(nametableX >= 256) {
				table = nametableBank2;
			} else {
				table = nametableBank1;
			}
		} else {
			if(nametableY >= 240) {
				table = nametableBank2;
			} else {
				table = nametableBank1;
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
		uint8_t bitplane1 = chrReadByte(bank + tileID*8*2 + nametableY%8);
		uint8_t bitplane2 = chrReadByte(bank + tileID*8*2 + nametableY%8 + 8);
		uint32_t* target = (uint32_t*)(((uint8_t*)frameBuffer->pixels + x*sizeof(uint32_t)) + y*frameBuffer->pitch);
		uint8_t combined = ((bitplane1 >> (7-(nametableX%8))) & 1) | (((bitplane2 >> (7-(nametableX%8))) & 1) << 1);
		uint8_t newIndex = paletteIndex | combined;
		uint8_t* palette = &ppuRAM[0x3F00];
		if(combined == 0) {
			*target = paletteColors[ppuRAM[0x3F00]];
		} else {
			*target = paletteColors[palette[newIndex]] & (combined == 0 ? paletteColors[ppuRAM[0x3F00]]: 0xFFFFFFFF);
		}
	}
}

void render(void) {
	//SDL_FillSurfaceRect(nameTable, &(SDL_Rect){0,0,NAMETABLE_WIDTH,NAMETABLE_HEIGHT}, paletteColors[ppuRAM[0x3F00]]);

	/*uint8_t* bank = &ppuRAM[(ppu.control & 0x10 ? 0x1000 : 0x0000)];
	if(ppu.mirror & MIRROR_HORIZONTAL) {
		drawNametable(bank, nametableBank1, 0, 0);
		drawNametable(bank, nametableBank2, 256, 0);
		drawNametable(bank, nametableBank1, 0, 240);
		drawNametable(bank, nametableBank2, 256, 240);
	} else {
		drawNametable(bank, nametableBank1, 0, 0);
		drawNametable(bank, nametableBank1, 256, 0);
		drawNametable(bank, nametableBank2, 0, 240);
		drawNametable(bank, nametableBank2, 256, 240);
	}

	uint16_t scrollX = (ppu.scrollX + (ppu.control & 0x01 ? 256 : 0));
	uint16_t scrollY = (ppu.scrollY + (ppu.control & 0x02 ? 240 : 0));
	SDL_Rect srcRect = {
		.x = scrollX,
		.y = scrollY,
		.w = 256,
		.h = 240,
	};
	SDL_Rect dstRect = {
		.x = 0,
		.y = 0,
		.w = FB_WIDTH,
		.h = FB_HEIGHT,
	};
	SDL_BlitSurface(nameTable, &srcRect, frameBuffer, &dstRect);

	// this needs to be changed when I inevitably need to deal with more complex scrolling
	if(scrollY + 240 > 480) {
		srcRect.y -= 480;
		SDL_BlitSurface(nameTable, &srcRect, frameBuffer, &dstRect);
	}
	if(scrollX + 256 > 512) {
		srcRect.x -= 512;
		SDL_BlitSurface(nameTable, &srcRect, frameBuffer, &dstRect);
	}*/
	/*for(uint16_t y = 0; y <= 240; ++y) {
		for(uint16_t x = 0; x <= 256; ++x) {
			drawPixel(x,y);
		}
	}*/

	// draw sprites in OAM
	if(ppu.mask & 0x10) {
		for(uint8_t i = 0; i < 64; ++i) {
			/*#ifdef DEBUG
				SDL_Rect asdf = {
					.x = ppu.oam[i*4 + 3],
					.y = ppu.oam[i*4 + 0],
					.w = 8,
					.h = 8,
				};
				SDL_FillRect(frameBuffer, &asdf, 0xFFFFFF55);
			#endif*/

			uint8_t tileID = ppu.oam[i*4 + 1];
			uint16_t bank;
			if(ppu.control & 0x20) {
				bank = (tileID & 1 ? 0x1000 : 0x0000);
				tileID &= ~1;
			} else {
				bank = (ppu.control & 0x08 ? 0x1000 : 0x0000);
			}

			uint16_t bitplaneStart = bank + tileID*8*2;
			uint8_t paletteIndex = 0x10 | ((ppu.oam[i*4 + 2]&0x3) << 2);
			if(ppu.control & 0x20) {
				uint8_t sprite1Off = 0;
				uint8_t sprite2Off = 8;
				if(ppu.oam[i*4 + 2] & FLIP_VERTICAL) {
					sprite1Off = 8;
					sprite2Off = 0;
				}
				drawTile(frameBuffer, bitplaneStart, ppu.oam[i*4 + 3], ppu.oam[i*4 + 0]+sprite1Off, ppu.oam[i*4 + 2], paletteIndex);
				bitplaneStart += 16;
				drawTile(frameBuffer, bitplaneStart, ppu.oam[i*4 + 3], ppu.oam[i*4 + 0]+sprite2Off, ppu.oam[i*4 + 2], paletteIndex);
			} else {
				drawTile(frameBuffer, bitplaneStart, ppu.oam[i*4 + 3], ppu.oam[i*4 + 0], ppu.oam[i*4 + 2], paletteIndex);
			}
		}
	}
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
