#include "debug.h"

#include <stdio.h>
#include <stdarg.h>

#include "ppu.h"

SDL_Surface* debugFont;
SDL_Surface* debugSurface;

uint8_t debugEnabled;

void initDebugRenderer(void) {
	debugFont = SDL_LoadBMP("font.bmp");
	if(debugFont == NULL) {
		printf("could not load font, \"%s\"\n", SDL_GetError());
		return;
	}

	debugSurface = SDL_CreateSurface(SCREEN_WIDTH, SCREEN_HEIGHT, SDL_PIXELFORMAT_RGBA8888);

	return;
}

void drawDebugText(uint16_t x, uint16_t y, char* fmt, ...) {
	if(!debugEnabled) { return; } 
	va_list args;
	va_start(args, fmt);
	char tempStr[256];
	vsnprintf(tempStr, 256, fmt, args);
	//printf("%s\n", tempStr);
	uint16_t startX = x;
	SDL_Rect charRect = {
		.x = 0,
		.y = 0,
		.w = 8,
		.h = 16,
	};
	SDL_Rect destRect = {
		.x = x,
		.y = y,
		.w = 8,
		.h = 16,
	};
	for(uint16_t i = 0; i < 256; ++i) {
		char c = tempStr[i];
		if(c == '\0') {
			break;
		}
		charRect.x = (c - '!') * 8;

		SDL_BlitSurface(debugFont, &charRect, debugSurface, &destRect);

		destRect.x += 8;
	}
}

void renderDebugInfo(SDL_Surface* windowSurface) {
	if(!debugEnabled) { return; } 
	SDL_BlitSurface(debugSurface, NULL, windowSurface, NULL);
	SDL_ClearSurface(debugSurface, 0, 0, 0, 0);
}

void uninitDebugRenderer(void) {
	SDL_DestroySurface(debugFont);
	return;
}

void toggleDebugInfo(void) {
	debugEnabled = !debugEnabled;
}
