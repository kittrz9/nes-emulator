#include "ppu.h"

#include <SDL2/SDL.h>

ppu_t ppu;

uint8_t ppuRAM[0x4000];

SDL_Window* w;
SDL_Surface* windowSurface;
SDL_Surface* tile;
SDL_Surface* nameTable;
SDL_Surface* frameBuffer;

#define NAMETABLE_WIDTH 32*8*2
#define NAMETABLE_HEIGHT 32*8*2

// generated with this: https://github.com/Gumball2415/palgen-persune
// palgen_persune.py -o test -f ".h uint8_t"
uint8_t paletteColors[] = {
	0x62, 0x62, 0x62,
	0x00, 0x1f, 0xb2,
	0x24, 0x04, 0xc8,
	0x52, 0x00, 0xb2,
	0x73, 0x00, 0x76,
	0x80, 0x00, 0x24,
	0x73, 0x0b, 0x00,
	0x52, 0x28, 0x00,
	0x24, 0x44, 0x00,
	0x00, 0x57, 0x00,
	0x00, 0x5c, 0x00,
	0x00, 0x53, 0x24,
	0x00, 0x3c, 0x76,
	0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,

	0xab, 0xab, 0xab,
	0x17, 0x50, 0xff,
	0x55, 0x2a, 0xff,
	0x94, 0x10, 0xff,
	0xc2, 0x08, 0xc5,
	0xd3, 0x16, 0x55,
	0xc2, 0x34, 0x00,
	0x94, 0x5b, 0x00,
	0x55, 0x81, 0x00,
	0x17, 0x9b, 0x00,
	0x00, 0xa2, 0x00,
	0x00, 0x95, 0x55,
	0x00, 0x77, 0xc5,
	0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,

	0xff, 0xff, 0xff,
	0x65, 0xa0, 0xff,
	0xa6, 0x79, 0xff,
	0xe7, 0x5e, 0xff,
	0xff, 0x56, 0xff,
	0xff, 0x64, 0xa6,
	0xff, 0x83, 0x32,
	0xe7, 0xac, 0x00,
	0xa6, 0xd3, 0x00,
	0x65, 0xef, 0x00,
	0x36, 0xf6, 0x32,
	0x24, 0xe9, 0xa6,
	0x36, 0xc9, 0xff,
	0x4e, 0x4e, 0x4e,
	0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,

	0xff, 0xff, 0xff,
	0xc1, 0xd9, 0xff,
	0xdb, 0xc9, 0xff,
	0xf6, 0xbe, 0xff,
	0xff, 0xbb, 0xff,
	0xff, 0xc1, 0xdb,
	0xff, 0xcd, 0xad,
	0xf6, 0xde, 0x8b,
	0xdb, 0xed, 0x7e,
	0xc1, 0xf8, 0x8b,
	0xae, 0xfc, 0xad,
	0xa7, 0xf6, 0xdb,
	0xae, 0xe9, 0xff,
	0xb8, 0xb8, 0xb8,
	0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,
};

uint32_t getPaletteColor(uint8_t index) {
	return \
		/* R */ paletteColors[(index*3)+0] << 24 |
		/* G */ paletteColors[(index*3)+1] << 16 |
		/* B */ paletteColors[(index*3)+2] << 8 |
		/* A */ 0xFF;
}

uint8_t initRenderer(void) {
	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("could not init SDL\n");
		return 1;
	}

	w = SDL_CreateWindow("nesEmu", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
	windowSurface = SDL_GetWindowSurface(w);
	tile = SDL_CreateRGBSurface(0, 8, 16, 32, 0xFF000000, 0xFF0000, 0xFF00, 0xFF);
	frameBuffer = SDL_CreateRGBSurface(0, FB_WIDTH, FB_HEIGHT, 32, 0xFF000000, 0xFF0000, 0xFF00, 0xFF);
	nameTable = SDL_CreateRGBSurface(0, NAMETABLE_WIDTH, NAMETABLE_HEIGHT, 32, 0xFF000000, 0xFF0000, 0xFF00, 0xFF);
	return 0;
}

void uninitRenderer(void) {
	SDL_FreeSurface(tile);
	SDL_FreeSurface(frameBuffer);
	SDL_FreeSurface(nameTable);
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

void drawTile(SDL_Surface* dst, uint8_t* bitplaneStart, uint16_t x, uint16_t y, uint8_t attribs, uint8_t paletteIndex) {
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
	uint8_t* bitplane1 = bitplaneStart;
	uint8_t* bitplane2 = bitplane1 + 8;
	for(uint8_t j = 0; j < 8; ++j) {
		uint8_t k = (attribs & FLIP_HORIZONTAL ? 0 : 7);
		while(k < 8) {
			uint8_t combined = ((*bitplane1 >> k) & 1) | (((*bitplane2 >> k) & 1) << 1);
			uint8_t newIndex = paletteIndex | combined;
			uint8_t* palette = &ppuRAM[0x3F00];
			uint8_t* paletteColor = &paletteColors[palette[newIndex]*3];
			*target = getPaletteColor(palette[newIndex]) & (combined == 0 ? 0xFFFFFF00 : 0xFFFFFFFF);
			++target;
			k += (attribs & FLIP_HORIZONTAL ? 1 : -1);
		}
		if(attribs & FLIP_VERTICAL) {
			target -= 16;
		}
		//target += tile->pitch;
		++bitplane1;
		++bitplane2;
	}
	SDL_BlitSurface(tile, &srcRect, dst, &targetRect);
}

void drawNametable(uint8_t* bank, uint16_t tableAddr, uint16_t x, uint16_t y) {
	uint8_t* table = &ppuRAM[tableAddr];
	uint8_t* attribTable = table + 0x3C0;
	for(uint16_t i = 0; i < 960; ++i) {
		uint8_t tileID = *(table+i);
		uint8_t tileX = i%32;
		uint8_t tileY = i/32;
		uint8_t attribX = tileX/4;
		uint8_t attribY = tileY/4;
		uint8_t attribIndex = (attribY * 8) + attribX;

		uint8_t* bitplaneStart = bank + tileID*8*2;
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
}

void render(void) {
	SDL_FillRect(nameTable, &(SDL_Rect){0,0,NAMETABLE_WIDTH,NAMETABLE_HEIGHT}, getPaletteColor(ppuRAM[0x3F00]));
	// draw nametable
	// only dealing with the horizontal mirroring for now
	uint8_t* bank = &ppuRAM[(ppu.control & 0x10 ? 0x1000 : 0x0000)];
	drawNametable(bank, 0x2000, 0, 0);
	drawNametable(bank, 0x2400, 256, 0);
	drawNametable(bank, 0x2800, 0, 240);
	drawNametable(bank, 0x2C00, 256, 240);
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
	if(scrollY + 240 > 480) {
		srcRect.y -= 480;
		SDL_BlitSurface(nameTable, &srcRect, frameBuffer, &dstRect);
	}
	if(scrollX + 256 > 512) {
		srcRect.y -= 512;
		SDL_BlitSurface(nameTable, &srcRect, frameBuffer, &dstRect);
	}
	// draw sprites in OAM
	for(uint8_t i = 0; i < 64; ++i) {
		#ifdef DEBUG
			SDL_Rect asdf = {
				.x = ppu.oam[i*4 + 3],
				.y = ppu.oam[i*4 + 0],
				.w = 8,
				.h = 8,
			};
			SDL_FillRect(frameBuffer, &asdf, 0xFFFFFF55);
		#endif

		uint8_t tileID = ppu.oam[i*4 + 1];
		uint8_t* bank;
		if(ppu.control & 0x20) {
			bank = &ppuRAM[(tileID & 1 ? 0x1000 : 0x0000)];
			tileID &= ~1;
		} else {
			bank = &ppuRAM[(ppu.control & 0x08 ? 0x1000 : 0x0000)];
		}

		uint8_t* bitplaneStart = bank + tileID*8*2;
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
	SDL_BlitScaled(frameBuffer, &(SDL_Rect){0,0,FB_WIDTH,FB_HEIGHT}, windowSurface, &(SDL_Rect){0,0,SCREEN_WIDTH,SCREEN_HEIGHT});
	SDL_UpdateWindowSurface(w);
}
