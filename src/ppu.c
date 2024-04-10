#include "ppu.h"

ppu_t ppu;

uint8_t ppuRAM[0x4000];

SDL_Window* w;
SDL_Renderer* r;
SDL_Surface* windowSurface;
SDL_Surface* tile;

void initRenderer(void) {
	w = SDL_CreateWindow("nesEmu", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
	r = SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED);
	windowSurface = SDL_GetWindowSurface(w);
	tile = SDL_CreateRGBSurface(0, 8, 8, 32, 0xFF000000, 0xFF0000, 0xFF00, 0xFF);
}

void render(void) {
	SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
	SDL_RenderClear(r);
	SDL_FillRect(windowSurface, &(SDL_Rect){0,0,SCREEN_WIDTH,SCREEN_HEIGHT}, 0xFF000000);
	SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
	static SDL_Rect asdf = {
		.x = 0,
		.y = 0,
		.w = 8,
		.h = 8,
	};
	for(uint8_t i = 0; i < 64; ++i) {
		asdf.x = ppu.oam[i*4 + 3];
		asdf.y = ppu.oam[i*4 + 0];
		//SDL_RenderFillRect(r, &asdf);

		uint8_t* bank = &ppuRAM[(ppu.control & 0x08 ? 0x1000 : 0x0000)];
		uint8_t tileID = ppu.oam[i*4 + 1];

		uint8_t* bitplane1 = bank + tileID;
		uint8_t* bitplane2 = bank + tileID + 8;
		uint32_t* target = tile->pixels;
		for(uint8_t j = 0; j < 8; ++j) {
			for(uint8_t k = 7; k < 8; --k) {
				uint8_t combined = ((*bitplane1 >> k) & 1) | (((*bitplane2 >> k) & 1) << 1);
				*target = 0xFFFFFF00 | (0xFF * (combined != 0)); // just doing this until I set up pallete stuff
				++target;
			}
			//target += tile->pitch;
			++bitplane1;
			++bitplane2;
		}
		SDL_BlitSurface(tile, &(SDL_Rect){0,0,8,8}, windowSurface, &asdf);
	}
	SDL_UpdateWindowSurface(w);

	
	SDL_RenderPresent(r);
}
