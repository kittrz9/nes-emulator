#ifndef PPU_H
#define PPU_H

#include <stdint.h>

#define FB_WIDTH 256
#define FB_HEIGHT 240

#define SCREEN_WIDTH FB_WIDTH*2
#define SCREEN_HEIGHT FB_HEIGHT*2

#define MIRROR_VERTICAL 0
#define MIRROR_HORIZONTAL 1

//https://www.nesdev.org/wiki/PPU_registers
typedef struct {
	uint8_t control;
	uint8_t mask;
	uint8_t status;
	uint16_t oamAddr;
	uint16_t vramAddr;
	uint8_t scrollX;
	uint8_t scrollY;
	uint8_t t;
	uint8_t x;
	uint8_t w;
	uint8_t mirror;
	uint8_t oam[256];
} ppu_t;

extern ppu_t ppu;

extern uint8_t ppuRAM[0x4000];

uint8_t initRenderer(void);
void uninitRenderer(void);

void debugScreenshot(void);

void render(void);

#endif
